/*
 * humandate.c - convert an NTP (or the current) time to something readable
 */
#include <config.h>
#include <stdio.h>

#include "ntp_fp.h"
#include "ntp_unixtime.h"	/* includes <sys/time.h> and <time.h> */
#include "lib_strbuf.h"
#include "ntp_stdlib.h"


/* This is used in msyslog.c; we don't want to clutter up the log with
   the year and day of the week, etc.; just the minimal date and time.  */

const char *
humanlogtime(void)
{
	char *		bp;
	time_t		cursec;
	struct tm	tmbuf, *tm;
	
	cursec = time(NULL);
	tm = localtime_r(&cursec, &tmbuf);
	if (!tm)
		return "-- --- --:--:--";

	LIB_GETBUF(bp);
	
	snprintf(bp, LIB_BUFLENGTH, "%02d-%02d%02d:%02d:%02d",
		 tm->tm_mon+1, tm->tm_mday,
		 tm->tm_hour, tm->tm_min, tm->tm_sec);
		
	return bp;
}


/*
 * humantime() -- like humanlogtime() but without date, and with the
 *		  time to display given as an argument.
 */
const char *
humantime(
	time_t cursec
	)
{
	char *		bp;
	struct tm	tmbuf, *tm;
	
	tm = localtime_r(&cursec, &tmbuf);
	if (!tm)
		return "--:--:--";

	LIB_GETBUF(bp);
	
	snprintf(bp, LIB_BUFLENGTH, "%02d:%02d:%02d",
		 tm->tm_hour, tm->tm_min, tm->tm_sec);
		
	return bp;
}
