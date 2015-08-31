VERSION_MAJOR = "1"
VERSION_MINOR = "0"
VERSION_REV   = "0"


out="b"
from waflib.Configure import conf
from waflib.TaskGen import feature, before, after, extension, after_method, before_method
from waflib.Task import Task

@after_method('apply_incpaths')
@feature('bld_include')
def insert_blddir(self):
	srcnode = self.bld.bldnode.abspath()
	self.env.prepend_value('INCPATHS', srcnode)

@after_method('apply_incpaths')
@feature('src_include')
def insert_srcdir(self):
	srcnode = self.bld.srcnode.abspath()
	self.env.prepend_value('INCPATHS', ["%s/include" % srcnode])


@after_method('apply_incpaths')
@feature('libisc_include')
def insert_libiscdir(self):
	srcnode = self.bld.srcnode.abspath()
	self.env.prepend_value('INCPATHS', ["%s/lib/isc/include/" % srcnode, "%s/lib/isc/unix/include/" % srcnode])


@after_method('apply_incpaths')
@feature('libisc_pthread_include')
def insert_libiscpthreaddir(self):
	srcnode = self.bld.srcnode.abspath()
	self.env.prepend_value('INCPATHS', ["%s/lib/isc/pthreads/include/" % srcnode])


@after_method('apply_incpaths')
@feature('libopts_include')
def insert_libopts(self):
	srcnode = self.bld.srcnode.abspath()
	self.env.prepend_value('INCPATHS', ["%s/sntp/libopts/" % srcnode])


class version(Task):
	vars = ['VERSION_FULL', 'TARGET']
	def run(self):
		self.outputs[0].write("const char *Version = \"%s %s\";" % (self.env.TARGET, self.env.VERSION_FULL))

@before_method('process_source')
@feature('ntp_version')
def ntp_version(self):
	n = self.path.find_or_declare('version.c')
	tsk = self.create_task('version', [], [n])
	tsk.env.TARGET = self.target
	self.source.extend([n])




TYPE_FRAG = """
#include <sys/types.h>
int main () {
	if (sizeof (%s))
		return 0;
	return 0;
}
"""

def options(ctx):
	ctx.load("compiler_c")

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

def configure(ctx):
	ctx.env.VERSION_FULL = "%s.%s.%s" % (VERSION_MAJOR, VERSION_MINOR, VERSION_REV)


	ctx.load('compiler_c')
	ctx.load('bison')
	ctx.find_program("yacc", var="BIN_YACC")
	ctx.find_program("awk", var="BIN_AWK")
	ctx.find_program("perl", var="BIN_PERL")

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

	types = ["int32", "int32_t", "uint32_t", "uint_t", "size_t", "wint_t", "pid_t"]

	for type in sorted(types):
		ctx.check_type(type)


	ctx.define("SIZEOF_SHORT", 2)
#	ctx.define("SIZEOF_LONG", 8)
#	ctx.define("SIZEOF_LONG_LONG", 8)

#	ctx.define("SIZEOF_CHAR_P", 8)
	ctx.define("SIZEOF_INT", 8)
#	ctx.define("SIZEOF_PTHREAD_T", 8)
	ctx.define("NEED_S_CHAR_TYPEDEF", 1)
	ctx.define("SIZEOF_SIGNED_CHAR", 1)
	ctx.define("SIZEOF_TIME_T", 8)
	ctx.define("HAVE_SYSCONF", 1)
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

	ctx.check_cc(lib="m")
	ctx.check_cc(lib="pthread")
	ctx.check_cc(lib="rt")
	ctx.check_cc(lib="thr")


	ctx.define("HAVE_STRUCT_SOCKADDR_STORAGE", 1)

	ctx.start_msg("Writing configuration header:")
	ctx.write_config_header("config.h")
	ctx.end_msg("config.h", "PINK")









def build(ctx):
	ctx.load('bison')

	ctx.recurse("lib/isc")
	ctx.recurse("libparse")
	ctx.recurse("libntp")
	ctx.recurse("sntp")
	ctx.recurse("ntpd")
	ctx.recurse("ntpdate")
	ctx.recurse("ntpdc")
	ctx.recurse("ntpq")
#	ctx.recurse("ntpsnmpd")
	ctx.recurse("parseutil")
#	ctx.recurse("adjtimed")
#	ctx.recurse("clockstuff") - 
#	ctx.recurse("kernel")
#	ctx.recurse("util") - 
#	ctx.recurse("unity")
#	ctx.recurse("tests")

