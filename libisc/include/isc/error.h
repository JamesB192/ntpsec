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

#include "isc/formatcheck.h"

typedef void (*isc_errorcallback_t)(const char *, int, const char *, va_list);

/*% set unexpected error */
void
isc_error_setunexpected(isc_errorcallback_t);

/*% set fatal error */
void
isc_error_setfatal(isc_errorcallback_t);

/*% unexpected error */
void
isc_error_unexpected(const char *, int, const char *, ...)
     ISC_FORMAT_PRINTF(3, 4);

/*% fatal error */
void
isc_error_fatal(const char *, int, const char *, ...)
ISC_FORMAT_PRINTF(3, 4) __attribute__	((__noreturn__));

/*% runtimecheck error */
void
isc_error_runtimecheck(const char *, int, const char *)
			__attribute__	((__noreturn__));

#define RUNTIME_CHECK(cond) \
	((void) ((cond) || \
		 (isc_error_runtimecheck(__FILE__, __LINE__, #cond), 0)))

#endif /* GUARD_ISC_ERROR_H */
