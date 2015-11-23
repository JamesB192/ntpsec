/*
 * ntp_timer.c - event timer support routines
 */
#include <config.h>

#include "ntp_machine.h"
#include "ntpd.h"
#include "ntp_stdlib.h"
#include "ntp_calendar.h"
#include "ntp_leapsec.h"
#include "ntp_intercept.h"

#if defined(HAVE_IO_COMPLETION_PORT)
# include "ntp_iocompletionport.h"
# include "ntp_timer.h"
#endif

#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#ifdef HAVE_KERNEL_PLL
#include "ntp_syscall.h"
#endif /* HAVE_KERNEL_PLL */

#ifdef ENABLE_AUTOKEY
#include <openssl/rand.h>
#endif	/* ENABLE_AUTOKEY */


/* TC_ERR represents the timer_create() error return value. */
#define	TC_ERR	(-1)

static void check_leapsec(uint32_t, const time_t*, bool);

/*
 * These routines provide support for the event timer.  The timer is
 * implemented by an interrupt routine which sets a flag once every
 * second, and a timer routine which is called when the mainline code
 * gets around to seeing the flag.  The timer routine dispatches the
 * clock adjustment code if its time has come, then searches the timer
 * queue for expiries which are dispatched to the transmit procedure.
 * Finally, we call the hourly procedure to do cleanup and print a
 * message.
 */
volatile int interface_interval;     /* init_io() sets def. 300s */

/*
 * Alarm flag. The mainline code imports this.
 */
volatile bool alarm_flag;

/*
 * The counters and timeouts
 */
static  u_long interface_timer;	/* interface update timer */
static	u_long adjust_timer;	/* second timer */
static	u_long stats_timer;	/* stats timer */
static	u_long leapf_timer;	/* Report leapfile problems once/day */
static	u_long huffpuff_timer;	/* huff-n'-puff timer */
static	u_long worker_idle_timer;/* next check for idle intres */
u_long	leapsec;	        /* seconds to next leap (proximity class) */
int     leapdif;                /* TAI difference step at next leap second*/
u_long	orphwait; 		/* orphan wait time */
#ifdef ENABLE_AUTOKEY
static	u_long revoke_timer;	/* keys revoke timer */
static	u_long keys_timer;	/* session key timer */
u_long	sys_revoke = KEY_REVOKE; /* keys revoke timeout (log2 s) */
u_long	sys_automax = NTP_AUTOMAX; /* key list timeout (log2 s) */
#endif	/* ENABLE_AUTOKEY */

/*
 * Statistics counter for the interested.
 */
volatile u_long alarm_overflow;

u_long current_time;		/* seconds since startup */

/*
 * Stats.  Number of overflows and number of calls to transmit().
 */
u_long timer_timereset;
u_long timer_overflows;
u_long timer_xmtcalls;

#ifdef SYS_WINNT
HANDLE WaitableTimerHandle;
#else
static	void alarming (int);
#endif /* SYS_WINNT */

#if !defined SYS_WINNT || defined(SYS_CYGWIN32)
#  ifdef HAVE_TIMER_CREATE
static timer_t timer_id;
typedef struct itimerspec intervaltimer;
#  define	itv_frac	tv_nsec
#  else
typedef struct itimerval intervaltimer;
#   define	itv_frac	tv_usec
#  endif
intervaltimer itimer;
#endif

#if !defined(SYS_WINNT)
void	set_timer_or_die(const intervaltimer *);
#endif


#if !defined(SYS_WINNT)
void
set_timer_or_die(
	const intervaltimer *	ptimer
	)
{
	const char *	setfunc;
	int		rc;

	UNUSED_ARG(ptimer);

#ifdef HAVE_TIMER_CREATE
	setfunc = "timer_settime";
	rc = timer_settime(timer_id, 0, &itimer, NULL);
#else
	setfunc = "setitimer";
	rc = setitimer(ITIMER_REAL, &itimer, NULL);
#endif
	if (-1 == rc) {
		msyslog(LOG_ERR, "interval timer %s failed, %m",
			setfunc);
		exit(1);
	}
}
#endif	/* !SYS_WINNT */


/*
 * reinit_timer - reinitialize interval timer after a clock step.
 */
void 
reinit_timer(void)
{
#if !defined(SYS_WINNT)
	ZERO(itimer);
#ifdef HAVE_TIMER_CREATE
	timer_gettime(timer_id, &itimer);
#else
	getitimer(ITIMER_REAL, &itimer);
#endif
	if (itimer.it_value.tv_sec < 0 ||
	    itimer.it_value.tv_sec > (1 << EVENT_TIMEOUT))
		itimer.it_value.tv_sec = (1 << EVENT_TIMEOUT);
	if (itimer.it_value.itv_frac < 0)
		itimer.it_value.itv_frac = 0;
	if (0 == itimer.it_value.tv_sec &&
	    0 == itimer.it_value.itv_frac)
		itimer.it_value.tv_sec = (1 << EVENT_TIMEOUT);
	itimer.it_interval.tv_sec = (1 << EVENT_TIMEOUT);
	itimer.it_interval.itv_frac = 0;
	set_timer_or_die(&itimer);
# endif
}


/*
 * init_timer - initialize the timer data structures
 */
void
init_timer(void)
{
	/*
	 * Initialize...
	 */
	alarm_flag = false;
	alarm_overflow = 0;
	adjust_timer = 1;
	stats_timer = SECSPERHR;
	leapf_timer = SECSPERDAY;
	huffpuff_timer = 0;
	interface_timer = 0;
	current_time = 0;
	timer_overflows = 0;
	timer_xmtcalls = 0;
	timer_timereset = 0;

#ifndef SYS_WINNT
	/*
	 * Set up the alarm interrupt.	The first comes 2**EVENT_TIMEOUT
	 * seconds from now and they continue on every 2**EVENT_TIMEOUT
	 * seconds.
	 */
#ifdef HAVE_TIMER_CREATE
	if (TC_ERR == timer_create(CLOCK_REALTIME, NULL, &timer_id)) {
		msyslog(LOG_ERR, "timer_create failed, %m");
		exit(1);
	}
#endif
	signal_no_reset(SIGALRM, alarming);
	itimer.it_interval.tv_sec = 
		itimer.it_value.tv_sec = (1 << EVENT_TIMEOUT);
	itimer.it_interval.itv_frac = itimer.it_value.itv_frac = 0;
	set_timer_or_die(&itimer);
#else	/* SYS_WINNT follows */
	/*
	 * Set up timer interrupts for every 2**EVENT_TIMEOUT seconds
	 * Under Windows/NT, 
	 */

	WaitableTimerHandle = CreateWaitableTimer(NULL, false, NULL);
	if (WaitableTimerHandle == NULL) {
		msyslog(LOG_ERR, "CreateWaitableTimer failed: %m");
		exit(1);
	}
	else {
		DWORD		Period;
		LARGE_INTEGER	DueTime;
		bool		rc;

		Period = (1 << EVENT_TIMEOUT) * 1000;
		DueTime.QuadPart = Period * 10000i64;
		rc = SetWaitableTimer(WaitableTimerHandle, &DueTime,
				      Period, NULL, NULL, false);
		if (!rc) {
			msyslog(LOG_ERR, "SetWaitableTimer failed: %m");
			exit(1);
		}
	}

#endif	/* SYS_WINNT */
}


/*
 * intres_timeout_req(s) is invoked in the parent to schedule an idle
 * timeout to fire in s seconds, if not reset earlier by a call to
 * intres_timeout_req(0), which clears any pending timeout.  When the
 * timeout expires, worker_idle_timer_fired() is invoked (again, in the
 * parent).
 *
 * ntpdig and ntpd each provide implementations adapted to their timers.
 */
void
intres_timeout_req(
	u_int	seconds		/* 0 cancels */
	)
{
	if (0 == seconds) {
		worker_idle_timer = 0;
		return;
	}
	worker_idle_timer = current_time + seconds;
}


/*
 * timer - event timer
 */
void
timer(void)
{
	struct peer *	p;
	struct peer *	next_peer;
	l_fp		now;
	time_t          tnow;

	/*
	 * The basic timerevent is one second.  This is used to adjust the
	 * system clock in time and frequency, implement the kiss-o'-death
	 * function and the association polling function.
	 */
	current_time++;
	if (adjust_timer <= current_time) {
		adjust_timer += 1;
		adj_host_clock();
#ifdef REFCLOCK
		for (p = peer_list; p != NULL; p = next_peer) {
			next_peer = p->p_link;
			if (FLAG_REFCLOCK & p->flags)
				refclock_timer(p);
		}
#endif /* REFCLOCK */
	}

	/*
	 * Now dispatch any peers whose event timer has expired. Be
	 * careful here, since the peer structure might go away as the
	 * result of the call.
	 */
	for (p = peer_list; p != NULL; p = next_peer) {
		next_peer = p->p_link;

		/*
		 * Restrain the non-burst packet rate not more
		 * than one packet every 16 seconds. This is
		 * usually tripped using iburst and minpoll of
		 * 128 s or less.
		 */
		if (p->throttle > 0)
			p->throttle--;
		if (p->nextdate <= current_time) {
#ifdef REFCLOCK
			if (FLAG_REFCLOCK & p->flags)
				refclock_transmit(p);
			else
#endif	/* REFCLOCK */
				transmit(p);
		}
	}

	/*
	 * Orphan mode is active when enabled and when no servers less
	 * than the orphan stratum are available. A server with no other
	 * synchronization source is an orphan. It shows offset zero and
	 * reference ID the loopback address.
	 */
	if (sys_orphan < STRATUM_UNSPEC && sys_peer == NULL &&
	    current_time > orphwait) {
		if (sys_leap == LEAP_NOTINSYNC) {
			sys_leap = LEAP_NOWARNING;
#ifdef ENABLE_AUTOKEY
			if (crypto_flags)	
				crypto_update();
#endif	/* ENABLE_AUTOKEY */
		}
		sys_stratum = (uint8_t)sys_orphan;
		if (sys_stratum > 1)
			sys_refid = htonl(LOOPBACKADR);
		else
			memcpy(&sys_refid, "LOOP", REFIDLEN);
		sys_offset = 0;
		sys_rootdelay = 0;
		sys_rootdisp = 0;
	}

	intercept_get_systime(__func__, &now);
	time(&tnow);

	/*
	 * Leapseconds. Get time and defer to worker if either something
	 * is imminent or every 8th second.
	 */
	if (leapsec > LSPROX_NOWARN || 0 == (current_time & 7))
		check_leapsec(now.l_ui, &tnow,
                                (sys_leap == LEAP_NOTINSYNC));
        if (sys_leap != LEAP_NOTINSYNC) {
                if (leapsec >= LSPROX_ANNOUNCE && leapdif) {
		        if (leapdif > 0)
			        sys_leap = LEAP_ADDSECOND;
		        else
			        sys_leap = LEAP_DELSECOND;
                } else {
                        sys_leap = LEAP_NOWARNING;
                }
	}

	/*
	 * Update huff-n'-puff filter.
	 */
	if (huffpuff_timer <= current_time) {
		huffpuff_timer += HUFFPUFF;
		huffpuff();
	}

#ifdef ENABLE_AUTOKEY
	/*
	 * Garbage collect expired keys.
	 */
	if (keys_timer <= current_time) {
		keys_timer += 1 << sys_automax;
		auth_agekeys();
	}

	/*
	 * Generate new private value. This causes all associations
	 * to regenerate cookies.
	 */
	if (revoke_timer && revoke_timer <= current_time) {
		revoke_timer += 1 << sys_revoke;
		RAND_bytes((uint8_t *)&sys_private, 4);
	}
#endif	/* ENABLE_AUTOKEY */

	/*
	 * Interface update timer
	 */
	if (interface_interval && interface_timer <= current_time) {
		timer_interfacetimeout(current_time +
		    interface_interval);
		DPRINTF(2, ("timer: interface update\n"));
		interface_update(NULL, NULL);
	}

	if (worker_idle_timer && worker_idle_timer <= current_time)
		worker_idle_timer_fired();

	/*
	 * Finally, write hourly stats and do the hourly
	 * and daily leapfile checks.
	 */
	if (stats_timer <= current_time) {
		stats_timer += SECSPERHR;
		write_stats();
		if (leapf_timer <= current_time) {
			leapf_timer += SECSPERDAY;
			check_leap_file(true, now.l_ui, &tnow);
		} else {
			check_leap_file(false, now.l_ui, &tnow);
		}
	}
}


#ifndef SYS_WINNT
/*
 * alarming - tell the world we've been alarmed
 */
static void
alarming(
	int sig
	)
{
# ifdef DEBUG
	const char *msg = "alarming: initializing TRUE\n";
# endif

	UNUSED_ARG(sig);

	if (!initializing) {
		if (alarm_flag) {
			alarm_overflow++;
# ifdef DEBUG
			msg = "alarming: overflow\n";
# endif
		} else {
			alarm_flag++;
# ifdef DEBUG
			msg = "alarming: normal\n";
# endif
		}
	}
# ifdef DEBUG
	if (debug >= 4)
		(void)(-1 == write(1, msg, strlen(msg)));
# endif
}
#endif /* !SYS_WINNT */


void
timer_interfacetimeout(u_long timeout)
{
	interface_timer = timeout;
}


/*
 * timer_clr_stats - clear timer module stat counters
 */
void
timer_clr_stats(void)
{
	timer_overflows = 0;
	timer_xmtcalls = 0;
	timer_timereset = current_time;
}

static void
check_leap_sec_in_progress( const leap_result_t *lsdata ) {
	bool prv_leap_sec_in_progress = leap_sec_in_progress;
	leap_sec_in_progress = lsdata->tai_diff && (lsdata->ddist < 3);

	/* if changed we may have to update the leap status sent to clients */
	if (leap_sec_in_progress != prv_leap_sec_in_progress)
		set_sys_leap(sys_leap);
}

static void
check_leapsec(
	uint32_t        now  ,
	const time_t * tpiv ,
        bool           reset)
{
	static const char leapmsg_p_step[] =
	    "Positive leap second, stepped backward.";
	static const char leapmsg_p_slew[] =
	    "Positive leap second, no step correction. "
	    "System clock will be inaccurate for a long time.";

	static const char leapmsg_n_step[] =
	    "Negative leap second, stepped forward.";
	static const char leapmsg_n_slew[] =
	    "Negative leap second, no step correction. "
	    "System clock will be inaccurate for a long time.";

	leap_result_t lsdata;
	uint32_t       lsprox;
	
#ifndef SYS_WINNT  /* WinNT port has its own leap second handling */
# ifdef HAVE_KERNEL_PLL
	leapsec_electric((pll_control && kern_enable) ? electric_on : electric_off);
# else
	leapsec_electric(electric_off);
# endif
#endif	
#ifdef ENABLE_LEAP_SMEAR
	leap_smear.enabled = (leap_smear_intv != 0);
#endif
	if (reset)	{
		lsprox = LSPROX_NOWARN;
		leapsec_reset_frame();
		memset(&lsdata, 0, sizeof(lsdata));
	} else {
		int fired = leapsec_query(&lsdata, now, tpiv);

		DPRINTF(1, ("*** leapsec_query: fired %i, now %u (0x%08X), tai_diff %i, ddist %u\n",
		      fired, now, now, lsdata.tai_diff, lsdata.ddist));
#ifdef ENABLE_LEAP_SMEAR
		leap_smear.in_progress = false;
		leap_smear.doffset = 0.0;

		if (leap_smear.enabled) {
		      if (lsdata.tai_diff) {
			      if (leap_smear.interval == 0) {
				      leap_smear.interval = leap_smear_intv;
				      leap_smear.intv_end = lsdata.ttime.Q_s;
				      leap_smear.intv_start = leap_smear.intv_end - leap_smear.interval;
				      DPRINTF(1, ("*** leapsec_query: setting leap_smear interval %li, begin %.0f, end %.0f\n",
					      leap_smear.interval, leap_smear.intv_start, leap_smear.intv_end));
			      }
		      }
		      else {
			      if (leap_smear.interval)
				      DPRINTF(1, ("*** leapsec_query: clearing leap_smear interval\n"));
			      leap_smear.interval = 0;
		      }

		      if (leap_smear.interval) {
			      double dtemp = now;
			      if (dtemp >= leap_smear.intv_start && dtemp <= leap_smear.intv_end) {
				      double leap_smear_time = dtemp - leap_smear.intv_start;
				      /*
				       * For now we just do a linear interpolation over the smear interval
				       */
#if 0
				      // linear interpolation
				      leap_smear.doffset = -(leap_smear_time * lsdata.tai_diff / leap_smear.interval);
#else
				      // Google approach: lie(t) = (1.0 - cos(pi * t / w)) / 2.0
				      leap_smear.doffset = -((double) lsdata.tai_diff - cos( M_PI * leap_smear_time / leap_smear.interval)) / 2.0;
#endif
				      /*
				       * TODO see if we're inside an
				       * inserted leap second, so we
				       * need to compute
				       * leap_smear.doffset = 1.0 -
				       * leap_smear.doffset
				       */
				      leap_smear.in_progress = true;
#if 0 && defined( DEBUG )
				      msyslog(LOG_NOTICE, "*** leapsec_query: [%.0f:%.0f] (%li), now %u (%.0f), smear offset %.6f ms\n",
					      leap_smear.intv_start, leap_smear.intv_end, leap_smear.interval,
					      now, leap_smear_time, leap_smear.doffset);
#else
				      DPRINTF(1, ("*** leapsec_query: [%.0f:%.0f] (%li), now %u (%.0f), smear offset %.6f ms\n",
					      leap_smear.intv_start, leap_smear.intv_end, leap_smear.interval,
					      now, leap_smear_time, leap_smear.doffset));
#endif

			      }
		      }
		}
		else
		      leap_smear.interval = 0;

		/*
		 * Update the current leap smear offset, eventually 0.0 if outside smear interval.
		 */
		DTOLFP(leap_smear.doffset, &leap_smear.offset);
#endif	/* ENABLE_LEAP_SMEAR */

		/* Full hit. Eventually step the clock, but always
		 * announce the leap event has happened.
		 */
	  	if (fired) {
			const char *leapmsg = NULL;
			if (lsdata.warped < 0) {
				if (clock_max_back > 0.0 &&
				    clock_max_back < abs(lsdata.warped)) {
					step_systime(lsdata.warped);
					leapmsg = leapmsg_p_step;
				} else {
					leapmsg = leapmsg_p_slew;
				}
			} else 	if (lsdata.warped > 0) {
				if (clock_max_fwd > 0.0 &&
				    clock_max_fwd < abs(lsdata.warped)) {
					step_systime(lsdata.warped);
					leapmsg = leapmsg_n_step;
				} else {
					leapmsg = leapmsg_n_slew;
				}
			}
			if (leapmsg)
				msyslog(LOG_NOTICE, "%s", leapmsg);
			report_event(EVNT_LEAP, NULL, NULL);
			lsprox  = LSPROX_NOWARN;
			leapsec = LSPROX_NOWARN;
			sys_tai = lsdata.tai_offs;
		} else {
			lsprox  = lsdata.proximity;
			sys_tai = lsdata.tai_offs;
		}
	}

	/* We guard against panic alarming during the red alert phase.
	 * Strange and evil things might happen if we go from stone cold
	 * to piping hot in one step. If things are already that wobbly,
	 * we let the normal clock correction take over, even if a jump
	 * is involved.
         * Also make sure the alarming events are edge-triggered, that is,
         * ceated only when the threshold is crossed.
         */
	if (  (leapsec > 0 || lsprox < LSPROX_ALERT)
	    && leapsec < lsprox                     ) {
		if (  leapsec < LSPROX_SCHEDULE
                   && lsprox >= LSPROX_SCHEDULE) {
			if (lsdata.dynamic)
				report_event(PEVNT_ARMED, sys_peer, NULL);
			else
				report_event(EVNT_ARMED, NULL, NULL);
		}
		leapsec = lsprox;
	}
	if (leapsec > lsprox) {
		if (  leapsec >= LSPROX_SCHEDULE
                   && lsprox   < LSPROX_SCHEDULE) {
			report_event(EVNT_DISARMED, NULL, NULL);
		}
		leapsec = lsprox;
	}

        if (leapsec >= LSPROX_SCHEDULE)
                leapdif = lsdata.tai_diff;
        else
                leapdif = 0;

	check_leap_sec_in_progress(&lsdata);
}
