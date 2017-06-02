/*
 * ntp_assert.h - design by contract stuff
 *
 * Copyright (C) 2004-2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1997-2001  Internet Software Consortium.
 * Copyright 2015 by the NTPsec project contributors
 * SPDX-License-Identifier: ISC
 *
 * example:
 *
 * int foo(char *a) {
 *	int result;
 *	int value;
 *
 *	REQUIRE(a != NULL);
 *	...
 *	bar(&value);
 *	INSIST(value > 2);
 *	...
 *
 *	ENSURE(result != 12);
 *	return result;
 * }
 *
 * open question: when would we use INVARIANT()?
 *
 * For cases where the overhead for non-debug builds is deemed too high,
 * use DEBUG_REQUIRE(), DEBUG_INSIST(), DEBUG_ENSURE(), and/or
 * DEBUG_INVARIANT().
 */

#ifndef GUARD_NTP_ASSERT_H
#define GUARD_NTP_ASSERT_H

#include <stdbool.h>

#if defined(__FLEXELINT__)

#include <assert.h>

#define REQUIRE(x)      assert(x)
#define INSIST(x)       assert(x)
#define INVARIANT(x)    assert(x)
#define ENSURE(x)       assert(x)

# else	/* not FlexeLint */

/*% isc assertion type */
typedef enum {
	isc_assertiontype_require,
	isc_assertiontype_ensure,
	isc_assertiontype_insist,
	isc_assertiontype_invariant
} isc_assertiontype_t;


/* our assertion catcher */
extern void	assertion_failed(const char *, int,
				 isc_assertiontype_t,
				 const char *)
			__attribute__	((__noreturn__));

#define REQUIRE(cond) \
	((void) ((cond) || (assertion_failed(__FILE__, __LINE__, \
					 isc_assertiontype_require, \
					 #cond), 0)))

#define ENSURE(cond) \
	((void) ((cond) || (assertion_failed(__FILE__, __LINE__, \
					 isc_assertiontype_ensure, \
					 #cond), 0)))

#define INSIST(cond) \
	((void) ((cond) || (assertion_failed(__FILE__, __LINE__, \
					 isc_assertiontype_insist, \
					 #cond), 0)))
#define INVARIANT(cond) \
	((void) ((cond) || (assertion_failed(__FILE__, __LINE__, \
					 isc_assertiontype_invariant, \
					 #cond), 0)))
# endif /* not FlexeLint */

# ifdef DEBUG
#define	DEBUG_REQUIRE(x)	REQUIRE(x)
#define	DEBUG_INSIST(x)		INSIST(x)
#define	DEBUG_ENSURE(x)		ENSURE(x)
# else
#define	DEBUG_REQUIRE(x)	do {} while (false)
#define	DEBUG_INSIST(x)		do {} while (false)
#define	DEBUG_ENSURE(x)		do {} while (false)
# endif

#endif	/* GUARD_NTP_ASSERT_H */
