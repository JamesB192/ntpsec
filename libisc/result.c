/*
 * Copyright (C) 2004, 2005, 2007, 2008, 2012  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2001, 2003  Internet Software Consortium.
 * Copyright 2015 by the NTPsec project contributors
 * SPDX-License-Identifier: ISC
 */

/*! \file */

#include <config.h>

#include <stddef.h>
#include <stdlib.h>

#include <isc/once.h>
#include <isc/resultclass.h>
#include <isc/util.h>

typedef struct resulttable {
	unsigned int				base;
	unsigned int				last;
	const char **				text;
	int					set;
	ISC_LINK(struct resulttable)		link;
} resulttable;

static const char *text[ISC_R_NRESULTS] = {
	"success",				/*%< 0 */
	"out of memory",			/*%< 1 */
	"timed out",				/*%< 2 */
	"no available threads",			/*%< 3 */
	"address not available",		/*%< 4 */
	"address in use",			/*%< 5 */
	"permission denied",			/*%< 6 */
	"no pending connections",		/*%< 7 */
	"network unreachable",			/*%< 8 */
	"host unreachable",			/*%< 9 */
	"network down",				/*%< 10 */
	"host down",				/*%< 11 */
	"connection refused",			/*%< 12 */
	"not enough free resources",		/*%< 13 */
	"end of file",				/*%< 14 */
	"socket already bound",			/*%< 15 */
	"reload",				/*%< 16 */
	"lock busy",				/*%< 17 */
	"already exists",			/*%< 18 */
	"ran out of space",			/*%< 19 */
	"operation canceled",			/*%< 20 */
	"socket is not bound",			/*%< 21 */
	"shutting down",			/*%< 22 */
	"not found",				/*%< 23 */
	"unexpected end of input",		/*%< 24 */
	"failure",				/*%< 25 */
	"I/O error",				/*%< 26 */
	"not implemented",			/*%< 27 */
	"unbalanced parentheses",		/*%< 28 */
	"no more",				/*%< 29 */
	"invalid file",				/*%< 30 */
	"bad base64 encoding",			/*%< 31 */
	"unexpected token",			/*%< 32 */
	"quota reached",			/*%< 33 */
	"unexpected error",			/*%< 34 */
	"already running",			/*%< 35 */
	"ignore",				/*%< 36 */
	"address mask not contiguous",		/*%< 37 */
	"file not found",			/*%< 38 */
	"file already exists",			/*%< 39 */
	"socket is not connected",		/*%< 40 */
	"out of range",				/*%< 41 */
	"out of entropy",			/*%< 42 */
	"invalid use of multicast address",	/*%< 43 */
	"not a file",				/*%< 44 */
	"not a directory",			/*%< 45 */
	"queue is full",			/*%< 46 */
	"address family mismatch",		/*%< 47 */
	"address family not supported",		/*%< 48 */
	"bad hex encoding",			/*%< 49 */
	"too many open files",			/*%< 50 */
	"not blocking",				/*%< 51 */
	"unbalanced quotes",			/*%< 52 */
	"operation in progress",		/*%< 53 */
	"connection reset",			/*%< 54 */
	"soft quota reached",			/*%< 55 */
	"not a valid number",			/*%< 56 */
	"disabled",				/*%< 57 */
	"max size",				/*%< 58 */
	"invalid address format",		/*%< 59 */
	"bad base32 encoding",			/*%< 60 */
	"unset",				/*%< 61 */
};

#define ISC_RESULT_RESULTSET			2
#define ISC_RESULT_UNAVAILABLESET		3

static isc_once_t 				once = ISC_ONCE_INIT;
static ISC_LIST(resulttable)			tables;
static pthread_mutex_t				lock;

static isc_result_t
register_table(unsigned int base, unsigned int nresults, const char **txt,
	       int set)
{
	resulttable *table;

	REQUIRE(base % ISC_RESULTCLASS_SIZE == 0);
	REQUIRE(nresults <= ISC_RESULTCLASS_SIZE);
	REQUIRE(txt != NULL);

	/*
	 * We use malloc() here because we we want to be able to use
	 * isc_result_totext() even if there is no memory context.
	 */
	table = malloc(sizeof(*table));
	if (table == NULL)
		return (ISC_R_NOMEMORY);
	table->base = base;
	table->last = base + nresults - 1;
	table->text = txt;
	table->set = set;
	ISC_LINK_INIT(table, link);

	LOCK(&lock);

	ISC_LIST_APPEND(tables, table, link);

	UNLOCK(&lock);

	return (ISC_R_SUCCESS);
}

static void
initialize_action(void) {
	isc_result_t result;

	RUNTIME_CHECK(pthread_mutex_init(&lock, NULL) == 0);
	ISC_LIST_INIT(tables);

	result = register_table(ISC_RESULTCLASS_ISC, ISC_R_NRESULTS, text,
				ISC_RESULT_RESULTSET);
	if (result != ISC_R_SUCCESS)
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "register_table() failed: %u", result);
}

static void
initialize(void) {
	RUNTIME_CHECK(isc_once_do(&once, initialize_action) == ISC_R_SUCCESS);
}

const char *
isc_result_totext(isc_result_t result) {
	resulttable *table;
	const char *txt;
	int idx;

	initialize();

	LOCK(&lock);

	txt = NULL;
	for (table = ISC_LIST_HEAD(tables);
	     table != NULL;
	     table = ISC_LIST_NEXT(table, link)) {
		if (result >= table->base && result <= table->last) {
			idx = (int)(result - table->base);
			txt = table->text[idx];
			break;
		}
	}
	if (txt == NULL)
		txt = "(result code text not available)";

	UNLOCK(&lock);

	return (txt);
}

isc_result_t
isc_result_register(unsigned int base, unsigned int nresults,
		    const char **txt, int set)
{
	initialize();

	return (register_table(base, nresults, txt, set));
}
