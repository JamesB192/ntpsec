/*
 * lib_strbuf - library string storage
 */
#include <config.h>

#include <isc/net.h>
#include <isc/result.h>

#include "ntp_fp.h"
#include "ntp_stdlib.h"
#include "lib_strbuf.h"


/*
 * Storage declarations
 */
int		debug;
libbufstr	lib_stringbuf[LIB_NUMBUF];
int		lib_nextbuf;
bool		ipv4_works;
bool		ipv6_works;


/*
 * initialization routine.  Might be needed if the code is ROMized.
 */
void
init_lib(void)
{
	static bool		lib_inited;

	if (lib_inited)
		return;
	ipv4_works = (ISC_R_SUCCESS == isc_net_probeipv4());
	ipv6_works = (ISC_R_SUCCESS == isc_net_probeipv6());
	init_systime();
	lib_inited = true;
}
