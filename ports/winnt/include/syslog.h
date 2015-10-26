/*
 * Copyright (C) 2001  Internet Software Consortium.
 * Copyright 2015 by the NTPsec project contributors
 * SPDX-License-Identifier: ISC
 */

/* From BIND 9 lib/isc/include/isc/: syslog.h,v 1.4 2002/08/01 03:43:31 mayer */

#ifndef GUARD_SYSLOG_H
#define GUARD_SYSLOG_H

#include <stdio.h>

/* Constant definitions for openlog() */
#define LOG_PID		1
#define LOG_CONS	2
/* NT event log does not support facility level */
#define LOG_KERN	0
#define LOG_USER	0
#define LOG_MAIL	0
#define LOG_DAEMON	0
#define LOG_AUTH	0
#define LOG_SYSLOG	0
#define LOG_LPR		0
#define LOG_LOCAL0	0
#define LOG_LOCAL1	0
#define LOG_LOCAL2	0
#define LOG_LOCAL3	0
#define LOG_LOCAL4	0
#define LOG_LOCAL5	0
#define LOG_LOCAL6	0
#define LOG_LOCAL7	0

#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but signification condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */

/*
 * These are ignored on NT
 */
#define LOG_NDELAY	0	/* Open the connection to syslogd immediately */

#define LOG_UPTO(pri)   ((1 << ((pri)+1)) - 1)  /* all priorities through pri */

void
syslog(int level, const char *fmt, ...);

void
openlog(const char *, int, ...);

void
closelog(void);

void
ModifyLogLevel(int level);

int
setlogmask(int maskpri);

void
InitNTLogging(FILE *, int);

void
NTReportError(const char *, const char *);

#endif
