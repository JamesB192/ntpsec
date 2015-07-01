#ifndef NTP_IO_H
#define NTP_IO_H

#include "ntp_workimpl.h"

/*
 * POSIX says use <fnctl.h> to get O_* symbols and
 * SEEK_SET symbol form <unistd.h>.
 */
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#if !defined(SEEK_SET) && defined(L_SET)
# define SEEK_SET L_SET
#endif

#ifdef SYS_WINNT
# include <io.h>
# include "win32_io.h"
#endif

#include <isc/boolean.h>
#include <isc/netaddr.h>

#include <netinet/in.h>

#if defined(HAVE_NETINET_IP_H)
# ifdef HAVE_NETINET_IN_SYSTM_H
#  include <netinet/in_systm.h>
# endif
# include <netinet/ip.h>
#endif

#include "libntp.h"	/* This needs Something above for GETDTABLESIZE */

/*
 * Define FNDELAY and FASYNC using O_NONBLOCK and O_ASYNC if we need
 * to (and can).  This is here initially for QNX, but may help for
 * others as well...
 */
#ifndef FNDELAY
# ifdef O_NONBLOCK
#  define FNDELAY O_NONBLOCK
# endif
#endif

#ifndef FASYNC
# ifdef O_ASYNC
#  define FASYNC O_ASYNC
# endif
#endif


/*
 * NIC rule match types
 */
typedef enum {
	MATCH_ALL,
	MATCH_IPV4,
	MATCH_IPV6,
	MATCH_WILDCARD,
	MATCH_IFNAME,
	MATCH_IFADDR
} nic_rule_match;

/*
 * NIC rule actions
 */
typedef enum {
	ACTION_LISTEN,
	ACTION_IGNORE,
	ACTION_DROP
} nic_rule_action;


extern int	qos;
SOCKET		move_fd(SOCKET fd);
isc_boolean_t	get_broadcastclient_flag(void);
extern int	is_ip_address(const char *, u_short, sockaddr_u *);
extern void	sau_from_netaddr(sockaddr_u *, const isc_netaddr_t *);
extern void	add_nic_rule(nic_rule_match match_type,
			     const char *if_name, int prefixlen,
			     nic_rule_action action);
#ifndef HAVE_IO_COMPLETION_PORT
extern	void	maintain_activefds(int fd, int closing);
#else
#define		maintain_activefds(f, c)	do {} while (0)
#endif

/* hack to ignore GCC Unused Result */
#define IGNORE(r) do{if(r);}while(0) 

#endif	/* NTP_IO_H */
