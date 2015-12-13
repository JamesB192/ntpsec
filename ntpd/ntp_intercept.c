/*****************************************************************************

ntp_intercept.c - capture and replay logic for NTP environment calls

Think of ntpd as a complex finite-state machine for transforming a
stream of input events to output events.  Events are of the
following kinds:

1. Startup, capuring command-line switches.

2. Configuration read.

3. Time reports from reference clocks.

4. Time calls to the host system clock.

5. Read and write of the system drift file.

6. Calls to the host's random-number generator.

7. Alarm events.

8. Calls to adjtime/ntp_adjtime/adjtime to adjust the system clock.

9  Calls to ntp_set_tod to set the system clock.

10. Read of the system leapsecond file.

11. Packets incoming from NTP peers and others.

12. Packets outgoing to NTP peers and others.

13. Read of authkey file

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

== Limitations ==

Reference-clock eventare not yet intercepted.

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
#include <sys/socket.h>
#include <netdb.h>

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
#include "ntp_leapsec.h"

static intercept_mode mode = none;

static char linebuf[256];
static int lineno;

intercept_mode intercept_get_mode(void)
{
    return mode;
}

void intercept_set_mode(intercept_mode newmode)
{
    mode = newmode;
    if (mode != none) {
	syslogit = false;
	hashprefix = true;
    }
}

static void get_operation(const char *expect)
/* get the next (non-comment) line from the log */
{
    for (;;)
    {
	char *in = fgets(linebuf, sizeof(linebuf), stdin);

	++lineno;

	if (in == NULL) {
	    fputs("ntpd: replay failed, unexpected EOF\n", stderr);
	    exit(1);
	}
	    
	if (expect != NULL && strncmp(linebuf, expect, strlen(expect)) != 0) {
	    fprintf(stderr, "ntpd: replay failed, expected %s but saw %*s\n",
		    expect, (int)strlen(expect), linebuf);
	    exit(1);
	}
	    
	if (linebuf[0] != '#')
	    break;
    }
	
}

void intercept_argparse(int *argc, char ***argv)
{
    int i;
    const char *leader = "NTP replay version 1";

    for (i = 1; i < *argc; i++)
	if (strcmp((*argv)[i], "-y") == 0)
	    intercept_set_mode(capture);
	else if  (strcmp((*argv)[i], "-Y") == 0)
	    intercept_set_mode(replay);

    if (mode == capture)
    {
	printf("%s\n", leader);

	printf("startup");
	for (i = 1; i < *argc; i++)
	    if (strcmp((*argv)[i], "-y") != 0 && strcmp((*argv)[i], "-Y") != 0)
		printf(" %s", (*argv)[i]);
	putchar('\n');
    }
    else if (mode == replay)
    {
	char *cp;
	bool was_space = true;

	/* require a log first line that matches what we can interpret */
	get_operation(leader);
	
	get_operation("startup ");
	*argc = 0;
	for (cp = strdup(linebuf + 9); *cp; cp++) {
	    if (was_space && !isspace(*cp))
		(*argv)[(*argc)++] = cp;
	    was_space = isspace(*cp);
	    if (was_space)
		*cp = '\0';
	}
    }
}

static bool pump(const char *fn, const char *lead, const char *trail, FILE *ofp)
{
	FILE *fp = fopen(fn, "r");
	if (fp == NULL)
	    return false;
	else
	{
	    int c;

	    fputs(lead, ofp);
	    while ((c = fgetc(fp)) != EOF)
		fputc(c, ofp);
	    fclose(fp);
	    fputs(trail, ofp);
	    return true;
	}
}


void intercept_getconfig(const char *configfile)
{
    if (mode != replay)
	/* this can be null if the default config doesn't exist */
	configfile = getconfig(configfile);

    if (configfile != NULL && mode != none)
	pump(configfile, "startconfig\n", "endconfig\n", stdout);

    if (mode == replay) {
	char tempfile[PATH_MAX];
	FILE *tfp;

	stats_control = false;	/* suppress writing stats files */
	get_operation("startconfig");
	snprintf(tempfile, sizeof(tempfile), ".fake_ntp_config_%d", getpid());
	tfp = fopen(tempfile, "w");
	for (;;) {
	    char *nextline = fgets(linebuf, sizeof(linebuf), stdin);
	    if (nextline == NULL) {
		fputs("ntpd: replay failed, unexpected EOF in config\n", stderr);
		exit(1);
	    }
	    if (strncmp(linebuf, "endconfig", 9) == 0)
		break;
	    fputs(linebuf, tfp);
	}	    
	getconfig(tempfile);
	unlink(tempfile);
    }
}

/*
 * An lfp used as a full date has an an unsigned seconds part.
 * Invert this with atolfp().
 */
#define lfpdump(n)	ulfptoa(n, 10)

void intercept_get_systime(const char *legend, l_fp *now)
{
    struct timespec ts;	/* seconds and nanoseconds */

    if (mode == replay) {
	int sec, subsec;
	char expecting[BUFSIZ];
	get_operation("systime ");
	if (sscanf(linebuf, "systime %s %d.%d", expecting, &sec, &subsec) != 3) {
	    fprintf(stderr, "ntpd: garbled systime format, line %d\n", lineno);
	    exit(1);
	}
	else if (strcmp(legend, expecting) == 0) {
	    fprintf(stderr, "ntpd: expected systime %s on line %d\n",
		    expecting, lineno);
	    exit(1);
	}
	ts.tv_sec = sec;
	ts.tv_nsec = subsec;
	normalize_time(ts, 0, now);
    } else {
	get_ostime(&ts);
	normalize_time(ts, sys_fuzz > 0.0 ? ntp_random() : 0, now);
	if (mode == capture)
	    printf("systime %s %s\n", legend, lfpdump(now));

    }
}

long intercept_ntp_random(const char *legend)
{
    long roll;

    if (mode == replay) {
	char expecting[BUFSIZ];
	/*
	 * Presently we're only using this as a check on the call sequence,
	 * as all the environment-altering functions that call ntp_random()
	 * are themselves intercepted.
	 */
	get_operation("random ");
	if (sscanf(linebuf, "random %s %ld", expecting, &roll) != 2) {
	    fprintf(stderr, "ntpd: garbled random format, line %d\n", lineno);
	    exit(1);
	}
	else if (strcmp(legend, expecting) == 0) {
	    fprintf(stderr, "ntpd: expected random %s on line %d\n",
		    expecting, lineno);
	    exit(1);
	}
	return roll;
    } else {
	roll = ntp_random();

	if (mode == capture)
	    printf("random %s %ld\n", legend, roll);
    }

    return roll;
}

void intercept_timer(void)
{
    if (mode == capture)
	printf("timer\n");
    else if (mode == replay)
	/* probably is not necessary to record this... */
	get_operation("timer");
    timer();
}

bool intercept_drift_read(const char *drift_file, double *drift)
{
    if (mode == replay) {
	float df;
	get_operation("drift_read ");
	if (strstr(linebuf, "false") != NULL)
	    return false;
	/*
	 * Weirdly, sscanf has no format for reading doubles.
	 * This could cause an obscure bug in replay if drift 
	 * ever requires 53 bits of precision as opposed to 24
	 * (and that's assuming IEEE-754, otherwise things could
	 * get hairier).
	 */ 
	if (sscanf(linebuf, "drift-read %f'", &df) != 1) {
	    fprintf(stderr, "ntpd: garbled drift-read format, line %d\n",lineno);
	    exit(1);
	}
	*drift = df;
    } else {
	FILE *fp;

	if ((fp = fopen(drift_file, "r")) == NULL) {
	    if (mode == capture)
		printf("drift-read false\n");
	    return false;
	}

	if (fscanf(fp, "%lf", drift) != 1) {
	    msyslog(LOG_ERR,
		    "format error frequency file %s",
		    drift_file);
	    fclose(fp);
	    if (mode == capture)
		printf("drift-read false\n");
	    return false;
	}
	fclose(fp);

	if (mode == capture)
	    printf("drift-read %.3f\n", *drift);
    }

    return true;
}

void intercept_drift_write(char *driftfile, double drift)
{
    if (mode == replay) {
	/*
	 * We don't want to actually mes with the system's timekeeping in 
	 * replay mode, so just check that we're writing out the same drift. 
	 */
	float df;
	get_operation("drift-write ");
	/* See the comment of drift-read chwxcking. */ 
	if (sscanf(linebuf, "drift-write %f'", &df) != 1) {
	    fprintf(stderr, "ntpd: garbled drift-write format, line %d\n",lineno);
	    exit(1);
	}
	/* beware spurious failures here due to float imprecisiion */ 
	if (df != drift) {
	    fprintf(stderr, "ntpd: expected drift %f but saw %f, line %d\n",
		    drift, df, lineno);
	    exit(1);
	}
    } else {
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

	if (mode == capture)
	    printf("drift-write %.3f\n", drift);
    }
}

int intercept_adjtime(const struct timeval *ntv, struct timeval *otv)
/* old-fashioned BSD call for systems with no PLL */
{
    if (mode == replay) {
	struct timeval rntv, rotv;
	get_operation("adjtime ");
	/* bletch - likely to foo up on 32-bit machines */
	if (sscanf(linebuf, "adjtime %ld %ld %ld %ld",
		   &rntv.tv_sec, &rntv.tv_usec,
		   &rotv.tv_sec, &rotv.tv_usec) != 4)
	{
	    fprintf(stderr, "ntpd: garbled adjtime format, line %d\n", lineno);
	    exit(1);
	}
	if (ntv->tv_sec != rntv.tv_sec
	    || ntv->tv_usec != rntv.tv_usec
	    || otv->tv_sec != rotv.tv_sec
	    || otv->tv_usec != rotv.tv_usec)
	{
	    fprintf(stderr, "ntpd: adjtime expected %ld.%ld/%ld.%ld but saw %ld.%ld/%ld.%ld, line %d\n",
		    (long)rntv.tv_sec,
		    (long)rntv.tv_usec,
		    (long)rotv.tv_sec,
		    (long)rotv.tv_usec,  
		    (long)ntv->tv_sec,
		    (long)ntv->tv_usec,
		    (long)otv->tv_sec,
		    (long)otv->tv_usec,  
		    lineno);
	    exit(1);
	}
    } else {
	if (mode == capture)
	    printf("adjtime %ld %ld %ld %ld",
		   (long)ntv->tv_sec, (long)ntv->tv_usec,
		   (long)otv->tv_sec, (long)otv->tv_usec);

	return adjtime(ntv, otv);
    }
    return 0;
}

#ifdef HAVE_KERNEL_PLL
int intercept_ntp_adjtime(struct timex *tx)
/* for newer systems with PLLs */
{
#define ADJFMT "%u %ld %ld %ld %ld %i %ld %ld %ld %ld %ld %i %ld %ld %ld %ld"
#define ADJDUMP(x, buf)			\
    snprintf(buf, sizeof(buf), ADJFMT,	\
	     (x)->modes,			\
	     (x)->offset,			\
	     (x)->freq,			\
	     (x)->maxerror,		\
	     (x)->esterror,		\
	     (x)->status,			\
	     (x)->constant,		\
	     (x)->precision,		\
	     (x)->tolerance,		\
	     (x)->ppsfreq,		\
	     (x)->jitter,			\
	     (x)->shift,			\
	     (x)->jitcnt,			\
	     (x)->calcnt,			\
	     (x)->errcnt,			\
	     (x)->stbcnt)

    char txdump[BUFSIZ], rtxdump[BUFSIZ];
    ADJDUMP(tx, txdump);
    int res = 0;

    if (mode == replay)
    {
	struct timex rtx;
	get_operation("ntp_adjtime ");
	if (sscanf(linebuf, "ntp_adtime " ADJFMT " %d",
		   &rtx.modes,
		   &rtx.offset,
		   &rtx.freq,
		   &rtx.maxerror,
		   &rtx.esterror,
		   &rtx.status,
		   &rtx.constant,
		   &rtx.precision,
		   &rtx.tolerance,
		   &rtx.ppsfreq,
		   &rtx.jitter,
		   &rtx.shift,
		   &rtx.jitcnt,
		   &rtx.calcnt,
		   &rtx.errcnt,
		   &rtx.stbcnt,
		   &res) != 17)
	{
	    fprintf(stderr, "ntpd: garbled ntp_adtime format, line %d\n",
		    lineno);
	    exit(1);
	}

	ADJDUMP(&rtx, rtxdump);
	if (strcmp(txdump, rtxdump) != 0) {
	    printf("ntpd: adjtime mismatch at line %d, saw %s\n",
		   lineno, txdump);
	    exit(1);
	}
    } else {
	res = ntp_adjtime(tx);

	if (mode == capture)
	    printf("ntp_adjtime " ADJFMT " %d\n",
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
    }

    return res;
#undef ADJFMT
#undef ADJDUMP
}
#endif

int intercept_set_tod(struct timespec *tvs)
{
    char newset[BUFSIZ];
    snprintf(newset, sizeof(newset),
	     "set_tod %ld %ld\n", (long)tvs->tv_sec, (long)tvs->tv_nsec);
    
    if (mode == replay) {
	get_operation("set_tod");
	if (strcmp(linebuf, newset) != 0) {
	    fprintf(stderr, "ntpd: line %d, set_tod mismatch saw %s\n",
		    lineno, newset);
	    exit(1);
	}
	return 0;
    }
    else {
	if (mode == capture)
	    fputs(newset, stdout);
	return ntp_set_tod(tvs);
    }
}

bool
intercept_leapsec_load_file(
	const char  * fname,
	struct stat * sb_old,
	bool   force,
	bool   logall)
{
    bool loaded = true;

    if (mode != replay)
	loaded = leapsec_load_file(fname, sb_old, force, logall);

    if (mode == capture)
	pump(fname, "startleapsec\n", "endleapsec\n", stdout);

    /* FIXME: replay logic goes here */
    
    return loaded;
}

static void packet_dump(char *buf, size_t buflen,
			sockaddr_u *dest, struct pkt *pkt, int len)
{
    /*
     * Order is: cast flags, receipt time, interface name, source
     * address, packet, length.  Cast flags are only kept because
     * they change the ntpq display, they have no implications for
     * the protocol machine.  We don't dump srcadr because only
     * the parse clock uses that.
     */
    size_t i;
    snprintf(buf, buflen, "%s %d:%d:%d:%d:%u:%u:%u:%s:%s:%s:%s",
	   socktoa(dest),
	   pkt->li_vn_mode, pkt->stratum, pkt->ppoll, pkt->precision,
	   /* FIXME: might be better to dump these in fixed-point */
	   pkt->rootdelay, pkt->rootdisp,
	   pkt->refid,
	   lfpdump(&pkt->reftime), lfpdump(&pkt->org),
	   lfpdump(&pkt->rec), lfpdump(&pkt->xmt));
    /* dump MAC as len - LEN_PKT_NOMAC chars in hex */
    for (i = 0; i < len - LEN_PKT_NOMAC; i++)
	printf("%02x", pkt->exten[i]);
}

void intercept_sendpkt(const char *legend,
		  sockaddr_u *dest, struct interface *ep, int ttl,
		  struct pkt *pkt, int len)
{
    char pkt_dump[BUFSIZ], newpacket[BUFSIZ];
    packet_dump(pkt_dump, sizeof(pkt_dump), dest, pkt, len);
    snprintf(newpacket, sizeof(newpacket), "sendpkt %s %s\n", legend, pkt_dump);

    if (mode == replay)
    {
	get_operation("sendpkt ");
	if (strcmp(linebuf, newpacket) != 0) {
	    fprintf(stderr, "ntpd: line %d, sendpkt mismatch saw %s\n",
		    lineno, pkt_dump);
	    exit(1);
	}
    } else {
	sendpkt(dest, ep, ttl, pkt, len);

	if (mode == capture)
	    fputs(newpacket, stdout);
    }
}

void intercept_receive(struct recvbuf *rbufp)
{
    char pkt_dump[BUFSIZ], newpacket[BUFSIZ];
    packet_dump(pkt_dump, sizeof(pkt_dump),
		&rbufp->recv_srcadr,
		&rbufp->recv_pkt, rbufp->recv_length);
    snprintf(newpacket, sizeof(newpacket),
	     "receive %0x %s %s\n",
	     rbufp->cast_flags, lfpdump(&rbufp->recv_time), pkt_dump);

    if (mode == replay) {
	get_operation("receive ");
	if (strcmp(linebuf, newpacket) != 0) {
	    fprintf(stderr, "ntpd: line %d, receive mismatch saw %s\n",
		    lineno, newpacket);
	    exit(1);
	}
    } else {
	if (mode == capture)
	    fputs(newpacket, stdout);

	receive(rbufp);
    }
}

void
intercept_getauthkeys(
	const char  * fname)
{
    if (mode != replay)
	getauthkeys(fname);

    if (mode == capture) {
	pump(fname, "startauthkeys", "endauthkeys", stdout);
    }

    /* FIXME: replay logic goes here */
}

void intercept_exit(int sig)
{
    if (mode == capture)
	printf("finish %d\n", sig);

    exit(0);
}

/* end */
