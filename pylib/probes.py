"""
This module exists to contin custom probe functions so they don't clutter
up the logic in the main configure.py.
"""

def probe_header_with_prerequisites(ctx, header, prerequisites):
	"Check that a header (with its prerequisites) compiles."
	src = ""
        for hdr in prerequisites + [header]:
        	src += "#include <%s>\n" % hdr
        src += "int main() { return 0; }\n"
	ctx.check_cc(
		fragment=src,
		define_name="HAVE_%s" % header.replace(".","_").replace("/","_").upper(),
		msg = "Checking for %s" % header,
		mandatory = False)

def probe_multicast(ctx, symbol, legend):
	"Probe for IP multicast capability."
	ctx.check_cc(
		fragment="""
#include <netinet/in.h>
int main() {
	struct ip_mreq ipmr;
	ipmr.imr_interface.s_addr = 0;
	return 0;
}
""",
		define_name=symbol,
		msg = legend,
		mandatory = False)

# What we really want to do here is test to see if the following program
# compiles *and exits with 9 status*.  Because we don;t know how to check
# the return status we must seettle for a simpler tesl
'''
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int call_vsnprintf(char *dst, size_t sz, const char *fmt, ...);

int call_vsnprintf(char *dst, size_t sz, const char *fmt,...)
{
	va_list	ap;
	int		rc;

	va_start(ap, fmt);
	rc = vsnprintf(dst, sz, fmt, ap);
	va_end(ap);

	return rc;
}

int main()
{
	char	sbuf[512];
	char	pbuf[512];
	int		slen;

	strcpy(sbuf, strerror(ENOENT));
	errno = ENOENT;
	slen = call_vsnprintf(pbuf, sizeof(pbuf), "%m",
			  "wrong");
	return strcmp(sbuf, pbuf);
}
'''


def probe_vsprintfm(ctx, symbol, legend):
	"Probe for %m expanding to strerror(error) in glibc style."
	ctx.check_cc(
		fragment='''
#include <features.h>
int main()
{
#ifndef __GLIBC__
# error __GLIBC__ is not defined
#endif
}
''',
		define_name=symbol,
		msg = legend,
		mandatory = False)
