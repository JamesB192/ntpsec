/* Copyright 2015 by the NTPsec project contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>

#include "config.h"
#include "ntpfrob.h"

int
main(int argc, char **argv)
{
	int ch;
	iomode mode = plain_text;
	while ((ch = getopt(argc, argv, "a:Ab:cejp:r")) != EOF) {
		switch (ch) {
		case 'A':
		    tickadj(mode==json, 0);
		    break;
		case 'a':
		    tickadj(mode, atoi(optarg));
		    break;
		case 'b':
		    bumpclock(atoi(optarg));
		    break;
		case 'c':
		    jitter(mode);
		    exit(0);
		    break;
		case 'e':
		    precision(mode);
		    exit(0);
		    break;
		case 'j':
		    mode = json;
		    break;
		case 'p':
#ifdef HAVE_SYS_TIMEPPS_H
		    ppscheck(optarg);
#else
		    fputs("ntpfrob: no PPS kernel interface.\n", stderr);
		    exit(0);
#endif
		    break;
		case 'r':
		    mode = raw;
		    break;
		default:
		    fputs("ntpfrob: no mode option specified.\n", stderr);
		    exit(1);
		    break;
		}
	}

    exit(0);
}
