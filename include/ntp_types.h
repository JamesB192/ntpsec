/*
 *  ntp_types.h - NTP basic types and macros.
 *
 * Assume C99 fixed width integer types are available: int32_t and uint32_t
 */
#ifndef GUARD_NTP_TYPES_H
#define GUARD_NTP_TYPES_H

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

#include <limits.h>
#include "ntp_machine.h"


/*
 * This is another naming conflict.
 * On NetBSD for MAC the macro "mac" is defined as 1
 * this is fun for us as a packet structure contains an
 * optional "mac" member - severe confusion results 8-)
 * As we hopefully do not have to rely on that macro we
 * just undefine that.
 */
#ifdef mac
#undef mac
#endif

/*
 * used to quiet compiler warnings
 */
#ifndef UNUSED_ARG
#define UNUSED_ARG(arg)		((void)(arg))
#endif
#ifndef UNUSED_LOCAL
#define UNUSED_LOCAL(arg)	((void)(arg))
#endif

/*
 * COUNTOF(array) - size of array in elements
 */
#define COUNTOF(arr)	(sizeof(arr) / sizeof((arr)[0]))

/*
 * We now assume the platform supports a 64-bit scalar type (the ISC
 * library wouldn't compile otherwise). Sadly, getting rid of vint64
 * is not as simple as turning it into a scalar due to same strange
 * code in the calendar calculations.
 */

typedef union {
#   ifdef WORDS_BIGENDIAN
	struct {
	        int32_t hi; uint32_t lo;
	} d_s;
	struct {
		uint32_t hi; uint32_t lo;
	} D_s;
#   else
	struct {
		uint32_t lo;   int32_t hi;
	} d_s;
	struct {
		uint32_t lo; uint32_t hi;
	} D_s;
#   endif

	int64_t	q_s;	/*   signed quad scalar */
	uint64_t Q_s;	/* unsigned quad scalar */
} vint64; /* variant int 64 */


typedef uint16_t	associd_t; /* association ID */
#define ASSOCID_MAX	USHRT_MAX
typedef uint32_t keyid_t;	/* cryptographic key ID */
#define KEYID_T_MAX	(0xffffffff)
typedef uint32_t tstamp_t;	/* NTP seconds timestamp */

/*
 * Cloning malloc()'s behavior of always returning pointers suitably
 * aligned for the strictest alignment requirement of any type is not
 * easy to do portably, as the maximum alignment required is not
 * exposed.  Use the size of a union of the types known to represent the
 * strictest alignment on some platform.
 */
typedef union max_alignment_tag {
	double		d;
} max_alignment;

#define MAXALIGN		sizeof(max_alignment)
#define ALIGN_UNITS(sz)		(((sz) + MAXALIGN - 1) / MAXALIGN)
#define ALIGNED_SIZE(sz)	(MAXALIGN * ALIGN_UNITS(sz))
#define INC_ALIGNED_PTR(b, m)	((void *)aligned_ptr((void *)(b), m))

static inline
max_alignment *
aligned_ptr(
	max_alignment *	base,
	size_t		minsize
	)
{
	return base + ALIGN_UNITS((minsize < 1) ? 1 : minsize);
}

/*
 * Macro to use in otherwise-empty source files to comply with ANSI C
 * requirement that each translation unit (source file) contain some
 * declaration.  This has commonly been done by declaring an unused
 * global variable of type int or char.  An extern reference to exit()
 * serves the same purpose without bloat.
 */
#define	NONEMPTY_TRANSLATION_UNIT	extern void exit(int);

/*
 * On Unix struct sock_timeval is equivalent to struct timeval.
 * On Windows built with 64-bit time_t, sock_timeval.tv_sec is a long
 * as required by Windows' socket() interface timeout argument, while
 * timeval.tv_sec is time_t for the more common use as a UTC time 
 * within NTP.
 */
#ifndef SYS_WINNT
#define	sock_timeval	timeval
#endif

/*
 * On Unix open() works for tty (serial) devices just fine, while on
 * Windows refclock serial devices are opened using CreateFile, a lower
 * level than the CRT-provided descriptors, because the C runtime lacks
 * tty APIs.  For refclocks which wish to use open() as well as or 
 * instead of refclock_open(), tty_open() is equivalent to open() on
 * Unix and  implemented in the Windows port similarly to
 * refclock_open().
 * Similarly, the termios emulation in the Windows code needs to know
 * about serial ports being closed, while the Posix systems do not.
 */
#ifndef SYS_WINNT
# define tty_open(f, a, m)	open(f, a, m)
# define closeserial(fd)	close(fd)
# define closesocket(fd)	close(fd)
typedef int SOCKET;
# define INVALID_SOCKET		(-1)
# define SOCKET_ERROR		(-1)
# define socket_errno()		(errno)
#else	/* SYS_WINNT follows */
# define socket_errno()		(errno = WSAGetLastError())
#endif



#endif	/* GUARD_NTP_TYPES_H */
