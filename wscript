VERSION_MAJOR = "1"
VERSION_MINOR = "0"
VERSION_REV   = "0"

out="build"

from pylib.configure import cmd_configure


def options(ctx):
	ctx.load("compiler_c")


def configure(ctx):
	ctx.env.VERSION_MAJOR = VERSION_MAJOR
	ctx.env.VERSION_MINOR = VERSION_MINOR
	ctx.env.VERSION_REV = VERSION_REV
	ctx.env.VERSION_FULL = "%s.%s.%s" % (VERSION_MAJOR, VERSION_MINOR, VERSION_REV)


	ctx.load('waf', tooldir='pylib/')

	cmd_configure(ctx)


def build(ctx):
	ctx.load('waf', tooldir='pylib/')
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

