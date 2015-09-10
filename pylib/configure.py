from waflib.Configure import conf
from util import msg, msg_setting

from posix_thread import posix_thread_version

TYPE_FRAG = """
#include <stdint.h>
#include <sys/types.h>
int main () {
	if (sizeof (%s))
		return 0;
	return 0;
}
"""

@conf
def check_type(ctx, type, mandatory=False):
	name = "HAVE_%s" % type.upper()

	ctx.check_cc(
		fragment	= TYPE_FRAG % (type),
		define_name = name,
		execute     = False,
		msg         = "Checking for type %s" % (type),
		mandatory	= mandatory
	)
#        conf.check(features='c', fragment='int main(){return 0;}') 7


SIZE_FRAG = """
%s
#include <stdio.h>
int main () {
	printf("%%lu", sizeof(%s));
	return 0;
}
"""

@conf
def check_sizeof(ctx, header, sizeof, mandatory=True):
	sizeof_ns = sizeof.replace(" ", "_")
	name = "SIZEOF_%s" % sizeof_ns.upper()

	header_snippet = ""
	if header:
		ctx.start_msg("Checking sizeof %s (%s)" % (sizeof, header))
		header_snippet = "#include <%s>" % header
	else:
		ctx.start_msg("Checking sizeof %s" % (sizeof))

	ctx.check_cc(
		fragment	= SIZE_FRAG % (header_snippet, sizeof),
		define_name = name,
		execute     = True,
		define_ret  = True,
		quote		= False,
		mandatory	= mandatory,
	)
	ctx.end_msg(ctx.get_define(name))



def refclock_config(ctx):
	from refclock import refclock_map

	if ctx.options.refclocks == "all":
		ids = refclock_map.keys()
	else:
		# XXX: better error checking
		ids = ctx.options.refclocks.split(",")

	ctx.env.REFCLOCK_DEFINES = []
	ctx.env.REFCLOCK_SOURCE = []

	for id in ids:
		try:
			id = int(id)
		except ValueError:
			ctx.fatal("'%s' is not an integer." % id)

		if id not in refclock_map:
			ctx.fatal("'%s' is not a valid Refclock ID" % id)

		rc = refclock_map[id]

		ctx.start_msg("Enabling Refclock %s:" % id)
		ctx.env.REFCLOCK_SOURCE.append((rc["file"], rc["define"]))
		ctx.end_msg(rc["descr"])
		ctx.env["REFCLOCK_%s" % rc["file"].upper()] = True

	ctx.env.REFCLOCK_ENABLE = True

#ctx.env.REFCLOCK_DEFINES
#ctx.env.REFCLOCK_SOURCE






def cmd_configure(ctx):

	if ctx.options.list:
		from refclock import refclock_map
		print "ID    Description"
		print "~~    ~~~~~~~~~~~"
		for id in refclock_map:
			print "%-5s %s" % (id, refclock_map[id]["descr"])

		return


	ctx.load('compiler_c')
	ctx.load('bison')

	if ctx.options.enable_debug:
		ctx.env.BISONFLAGS += ["--debug"]

	ctx.find_program("yacc", var="BIN_YACC")
	ctx.find_program("awk", var="BIN_AWK")
	ctx.find_program("perl", var="BIN_PERL")


	ctx.start_msg("Checking build target")
	from sys import platform
	if platform == "win32":
		ctx.env.PLATFORM_TARGET = "win"
	else:
		ctx.env.PLATFORM_TARGET = "unix"
	ctx.end_msg(ctx.env.PLATFORM_TARGET	)


	from os.path import exists
	from waflib.Utils import subprocess
	if exists(".git"):
		ctx.find_program("git", var="BIN_GIT")
		ctx.start_msg("DEVEL: Getting revision")
		cmd = ["git", "log", "-1", "--format=%H"]
		p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=None)
		ctx.env.REVISION, stderr = p.communicate()
		ctx.env.REVISION = ctx.env.REVISION.replace("\n", "")
		ctx.end_msg(ctx.env.REVISION)

	ctx.start_msg("Building version")
	if ctx.env.REVISION:
		ctx.env.VERSION_FULL = "%s-%s" % (ctx.env.VERSION_FULL, ctx.env.REVISION[:7])
	ctx.end_msg(ctx.env.VERSION_FULL)


	ctx.recurse("lib/isc")
	ctx.recurse("libntp")
	ctx.recurse("sntp")

	types = ["int32", "int32_t", "uint32_t", "uint_t", "size_t", "wint_t", "pid_t", "intptr_t", "uintptr_t"]

	for type in sorted(types):
		ctx.check_type(type)

	# XXX: hack
	ctx.env.PLATFORM_INCLUDES = ["/usr/local/include"]
	ctx.env.PLATFORM_LIBPATH = ["/usr/local/lib"]

	sizeofs = [
		("time.h",		"time_t"),
		(None,			"int"),
		(None,			"short"),
		(None,			"long"),
		(None,			"long long"),
		("pthread.h",	"pthread_t"),
		(None,			"signed char"),
	]

	for header, sizeof in sorted(sizeofs):
		ctx.check_sizeof(header, sizeof)


	ctx.define("NEED_S_CHAR_TYPEDEF", 1)

	ctx.define("NTP_KEYSDIR", "%s/etc" % ctx.env.PREFIX)
	ctx.define("GETSOCKNAME_SOCKLEN_TYPE", "socklen_t", quote=False)
	ctx.define("DFLT_RLIMIT_STACK", 50)
	ctx.define("DFLT_RLIMIT_MEMLOCK", 32)
	ctx.define("POSIX_SHELL", "/bin/sh")

	ctx.define("MCAST", 1) # XXX: check for mcast support
	ctx.define("TYPEOF_IP_MULTICAST_LOOP", "u_char", quote=False) #XXX: check for mcast type
	ctx.define("OPEN_BCAST_SOCKET", 1)

	ctx.check_cc(function_name='arc4random_buf', header_name="stdlib.h", mandatory=False)
	ctx.check_cc(function_name='sysconf', header_name="unistd.h", mandatory=False)
	ctx.check_cc(header_name="stdbool.h", mandatory=False)
	ctx.check_cc(header_name="sys/soundcard.h", mandatory=False) #XXX: check for others in libntp/audio.c

	# XXX: This needs fixing.
	for header in ["timepps.h", "sys/timepps.h"]:
		ctx.check_cc(
			fragment="""
#include <inttypes.h>
#include <%s>
int main() { return 0; }
""" % header,
					define_name="HAVE_%s" % header.replace(".","_").replace("/","_").upper(),
					msg = "Checking for %s" % header,
					mandatory = False
		)

	if ctx.get_define("HAVE_TIMEPPS_H") or ctx.get_define("HAVE_SYS_TIMEPPS_H"):
		ctx.define("HAVE_PPSAPI", 1)



	ctx.check_cc(lib="m")
	ctx.check_cc(lib="pthread")
	ctx.check_cc(lib="rt")
	ctx.check_cc(lib="thr", mandatory=False)

	ctx.check_cc(header_name="event2/event.h", includes=ctx.env.PLATFORM_INCLUDES)
	ctx.check_cc(feature="c cshlib", lib="event", libpath=ctx.env.PLATFORM_LIBPATH, uselib_store="LIBEVENT")
	ctx.check_cc(feature="c cshlib", lib="event_core", libpath=ctx.env.PLATFORM_LIBPATH, uselib_store="LIBEVENT_CORE")
	ctx.check_cc(feature="c cshlib", lib="event_pthreads", libpath=ctx.env.PLATFORM_LIBPATH, uselib_store="LIBEVENT_PTHREADS", use="LIBEVENT")


	ctx.check_cc(
		fragment	= """
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct sockaddr_storage n;
""",
		define_name = "HAVE_STRUCT_SOCKADDR_STORAGE",
		features	= "c",
		msg         = "Checking for type sockaddr_storage",
		mandatory	= False
	)

	ctx.check_cc(
		fragment	= """
#include <sys/types.h>
#include <sys/socket.h>
int main () {
  extern struct sockaddr *ps;
  return ps->sa_len;
}
""",
		define_name = "ISC_PLATFORM_HAVESALEN",
		features	= "c",
		msg         = "Checking for sockaddr->sa_len",
		mandatory	= False
	)

	posix_thread_version(ctx)
	ctx.define('HAVE_PTHREADS', ctx.env.POSIX_THREAD_VERISON)


	if ctx.options.refclocks:
		refclock_config(ctx)

	if ctx.options.enable_debug:
		ctx.env.CFLAGS += ["-g"]
		ctx.define("DEBUG", 1)

	if ctx.options.enable_ipv6:
		ctx.define("INCLUDE_IPV6_SUPPORT", 1)


	ctx.start_msg("Writing configuration header:")
	ctx.write_config_header("config.h")
	ctx.end_msg("config.h", "PINK")


	msg("")
	msg("Build Options")
	msg_setting("CC", " ".join(ctx.env.CC))
	msg_setting("CFLAGS", " ".join(ctx.env.CFLAGS))
	msg_setting("LDFLAGS", " ".join(ctx.env.LDFLAGS))
	msg_setting("PREFIX", ctx.env.PREFIX)
	msg_setting("Debug Support", ctx.options.enable_debug)
	msg_setting("IPv6 Support", ctx.options.enable_ipv6)
	msg_setting("Refclocks", ctx.options.refclocks)
