/*****************************************************************************

intercept.c - capture and replay logic for NTP environment calls

Think of ntpd as a complex finite-state machine for transforming a
stream of input events to output events.  Events are of the
following kinds:

1. Startup or termination.

2. Configuration read (including option state).

3. Time reports from reference clocks.

4. Time calls to the host system clock.

5. Read and write of the system drift file.

6. Calls to the host's random-number generator.

7. Alarm events.

8. Calls to adjtime to set the system clock.

9. Read of the system leapsecond file.  (TODO)

10. Packets incoming from NTP daemons.  (TODO)

11. Packets outgoing to NTP daemons.  (TODO)

12. Read of authkey file  (TODO)

13. getaddrinfo() calls (TODO)

14. Termination.

We must support two modes of operation.  In "capture" mode, ntpd
operates normally, logging all events.  In "replay" mode, ntpd accepts
an event-capture log and replays it, processing all input events in a
previous capture.

We say that test performance is *stable* when replay mode is
idempotent - that is, replaying an event-capture log produces an exact
copy of itself by duplicating the same output events.

When test performance is stable, we know two things: (1) we have
successfully captured all inputs of the system, and (2) the code
has experienced no functional regressions since the event capture.

We can regression-test the code by capturing logs of production
behavior and replaying them.  We can also hand-craft tests to probe
edge cases.  To support the latter case, it is highly desirable that the
event-capture format be a text stream in an eyeball-friendly,
readily-editable format.

== Implementation ==

ntpd needs two new switches: capture and replay.  The capture switch
says: to log all event calls to an event capture file in addition to
their normal behaviors.  This includes both read events (such as
refclock inputs) and write events (such as adjtimex calls).

The replay switch has more complex behavior. Interpret a capture
file. Mock all event calls with code that looks at each event
sequentially from the capture.  If a read call is performed, and the
next data in the log file is for that read call, the logged data is
returned for the call.  If a write call is performed, the call type and
call data is compared to the next log data.  If the next event doesn't
match the expected type or has different data, show the difference and
terminate - the replay failed.  Otherwise continue.

Replay succeeds if the event stream reaches the shutdown event with
no mismatches.

*****************************************************************************/

#include <time.h>
#include <sys/time.h>

#include <config.h>

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libgen.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_control.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"
#include "ntp_config.h"
#include "ntp_crypto.h"
#include "ntp_assert.h"
#include "ntp_intercept.h"
#include "ntp_fp.h"
#include "ntp_syscall.h"

static intercept_mode mode = none;

intercept_mode intercept_get_mode(void)
{
    return mode;
}

void intercept_set_mode(intercept_mode newmode)
{
    mode = newmode;
    if (newmode == replay)
	syslogit = false;
}

void intercept_argparse(int *argc, char ***argv)
{
    int i;
    for (i = 1; i < *argc; i++)
	if (strcmp((*argv)[i], "-y") == 0)
	    mode = capture;
	else if  (strcmp((*argv)[i], "-Y") == 0)
	    mode = replay;

    if (mode == capture)
    {
	printf("event startup");
	for (i = 1; i < *argc; i++)
	    if (strcmp((*argv)[i], "-y") != 0 && strcmp((*argv)[i], "-Y") != 0)
		printf(" %s", (*argv)[i]);
	putchar('\n');
    }
}

void intercept_getconfig(const char *configfile)
{
    if (mode == none)
	getconfig(configfile);
    else {
	fputs("startconfig\n", stdout);
#ifdef SAVECONFIG
	dump_all_config_trees(stdout, false);
#endif
	fputs("endconfig\n", stdout);

    }

    if (mode == replay) {
	stats_control = false;	/* suppress writing stats files */
    }
}

void intercept_log(const char *fmt, ...)
{
    if (mode != none) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
    }
}

void intercept_get_systime(const char *legend, l_fp *now)
{
    struct timespec ts;	/* seconds and nanoseconds */
    get_ostime(&ts);

    if (mode == capture)
	printf("event systime %s %ld %ld\n",
		legend, (long)ts.tv_sec, ts.tv_nsec);

    normalize_time(ts, ntp_random(), now);
}

long intercept_ntp_random(const char *legend)
{
    long rand = ntp_random();

    /* FIXME: replay logic goes here */

    if (mode != none)
	printf("event random %s %ld\n", legend, rand);

    return rand;
}

void intercept_alarm(void)
{
    /* FIXME: replay mode, calling timer(), goes here */

    if (mode != none)
	printf("event tick\n");
}

bool intercept_drift_read(const char *drift_file, double *drift)
{
    FILE *fp;

    if (mode != replay) {
	if ((fp = fopen(drift_file, "r")) == NULL)
	    return false;

	if (fscanf(fp, "%lf", drift) != 1) {
	    msyslog(LOG_ERR,
		    "format error frequency file %s",
		    drift_file);
	    fclose(fp);
	    return false;
	}
	fclose(fp);
    }

    if (mode != none)
	printf("event drift %.3f\n", *drift);

    return true;
}

void intercept_drift_write(char *driftfile, double drift)
{
    if (mode == capture || mode == replay)
	printf("event drift %.3f\n", drift);
    else
    {
	int fd;
	char tmpfile[PATH_MAX], driftcopy[PATH_MAX];
	char driftval[32];

	strlcpy(driftcopy, driftfile, PATH_MAX);
	strlcpy(tmpfile, dirname(driftcopy), sizeof(tmpfile));
	strlcat(tmpfile, "/driftXXXXXX", sizeof(tmpfile));
	/* coverity[secure_temp] Warning is bogus on POSIX-compliant systems*/
	if ((fd = mkstemp(tmpfile)) < 0) {
	    msyslog(LOG_ERR, "frequency file %s: %m", tmpfile);
	    return;
	}
	snprintf(driftval, sizeof(driftval), "%.3f\n", drift);
	IGNORE(write(fd, driftval, strlen(driftval)));
	(void)close(fd);
	/* atomic */
#ifdef SYS_WINNT
	if (_unlink(driftfile)) /* rename semantics differ under NT */
	    msyslog(LOG_WARNING,
		    "Unable to remove prior drift file %s, %m",
		    driftfile);
#endif /* SYS_WINNT */
	if (rename(tmpfile, driftfile))
	    msyslog(LOG_WARNING,
		    "Unable to rename temp drift file %s to %s, %m",
		    tmpfile, driftfile);
    }
}

#ifdef HAVE_KERNEL_PLL
int intercept_kernel_pll_adjtime(struct timex *tx)
{
    int res = 0;

    /* FIXME: replay logic goes here */

    if (mode != replay)
	res = ntp_adjtime(tx);

    if (mode != none)
	printf("event adjtime %u %ld %ld %ld %ld %i %ld %ld %ld %ld %ld %i %ld %ld %ld %ld %d\n",
	       tx->modes,
	       tx->offset,
	       tx->freq,
	       tx->maxerror,
	       tx->esterror,
	       tx->status,
	       tx->constant,
	       tx->precision,
	       tx->tolerance,
	       tx->ppsfreq,
	       tx->jitter,
	       tx->shift,
	       tx->jitcnt,
	       tx->calcnt,
	       tx->errcnt,
	       tx->stbcnt,
	       res
	    );

    return res;
}
#endif

void intercept_sendpkt(const char *legend,
		  sockaddr_u *dest, struct interface *ep, int ttl,
		  struct pkt *pkt, int len)
{
    if (mode != replay)
	sendpkt(dest, ep, ttl, pkt, len);

    if (mode != none) {
	printf("event sendpkt \"%s\" ...", legend);
	/* FIXME: dump the destination and the guts of the packet */
	fputs("\n", stdout);
    }
}
