/*
 * Created: Sat Aug 20 14:23:01 2005
 *
 * Copyright (C) 2005 by Frank Kardel
 * Copyright 2015 by the NTPsec project contributors
 * SPDX-License-Identifier: BSD-2-clause
 */
#ifndef GUARD_NTP_DEBUG_H
#define GUARD_NTP_DEBUG_H

/*
 * macro for debugging output - cut down on #ifdef pollution.
 *
 * TRACE() is similar to ntpd's DPRINTF() for utilities and libntp.
 * Uses mprintf() and so supports %m, replaced by strerror(errno).
 *
 * The calling convention is not attractive:
 *     TRACE(debuglevel, (fmt, ...));
 *     TRACE(2, ("this will appear on stdout if debug >= %d\n", 2));
 */
#define TRACE(lvl, arg)					\
	do { 						\
		if (debug >= (lvl))			\
			mprintf arg;			\
	} while (0)

#endif	/* GUARD_NTP_DEBUG_H */
