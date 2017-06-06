/*
 * Copyright (C) 2004-2007, 2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2001  Internet Software Consortium.
 * Copyright 2015 by the NTPsec project contributors
 * SPDX-License-Identifier: ISC
 */

#ifndef GUARD_ISC_ERROR_H
#define GUARD_ISC_ERROR_H 1

/*! \file isc/error.h */

#include <stdarg.h>

/*
 * ISC_FORMAT_PRINTF().
 *
 * fmt is the location of the format string parameter.
 * args is the location of the first argument (or 0 for no argument checking).
 * 
 * Note:
 * The first parameter is 1, not 0.
 */
#ifdef __GNUC__
#define ISC_FORMAT_PRINTF(fmt, args) __attribute__((__format__(__printf__, fmt, args)))
#else
#define ISC_FORMAT_PRINTF(fmt, args)
#endif


typedef void (*isc_errorcallback_t)(const char *, int, const char *, va_list);

/* set unexpected error */
void
isc_error_setunexpected(isc_errorcallback_t);

/* unexpected error */
void
isc_error_unexpected(const char *, int, const char *, ...)
     ISC_FORMAT_PRINTF(3, 4);

/* Unexpected Error */
#define UNEXPECTED_ERROR		isc_error_unexpected

/* hack to ignore GCC Unused Result */
#define ISC_IGNORE(r) do{if(r){}}while(0)

/* fatal error */
void
isc_error_fatal(const char *, int, const char *, ...)
ISC_FORMAT_PRINTF(3, 4) __attribute__	((__noreturn__));

#endif /* GUARD_ISC_ERROR_H */
