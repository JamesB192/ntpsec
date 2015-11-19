from waflib import Utils

out="build"

from pylib.configure import cmd_configure

OPT_STORE = {} # Storage for options to pass into configure

def parse_version():
        with open("VERSION", "r") as f:
                version_string = f.read().strip()
        [major,minor,rev] = version_string.split(".")
        return {
                # "NTPS" for NTPSec -- this avoids any naming collisions
                "NTPS_VERSION_MAJOR" : int(major),
                "NTPS_VERSION_MINOR" : int(minor),
                "NTPS_VERSION_REV" : int(rev)
        }

config = parse_version()

def dist(ctx):
        ctx.base_name = "ntpsec-%d.%d.%d" % \
                        (config["NTPS_VERSION_MAJOR"], \
                         config["NTPS_VERSION_MINOR"], \
                         config["NTPS_VERSION_REV"])
        if ctx.options.build_version_tag:
                ctx.base_name = ctx.base_name + "-" + \
                                ctx.options.build_version_tag

def options(ctx):
	ctx.load("compiler_c")
	ctx.load("msvc")

	def callback_flags(option, opt, value, parser):
		OPT_STORE.setdefault(opt, []).append(value)

	grp = ctx.add_option_group("NTP build options")
	grp.add_option('--enable-debug', action='store_true', default=False, help="Enable debugging code")
	grp.add_option('--enable-debug-gdb', action='store_true', default=False, help="Enable GDB debugging symbols")
	grp.add_option('--enable-doc', action='store_true', default=False, help="Build NTP documentation")
	grp.add_option('--enable-a2x-xmllint', action='store_true', default=False, help="Build NTP documentation with a2x XML lint")
	grp.add_option('--enable-crypto', action='store_true', default=False, help="Enable OpenSSL.")
	grp.add_option('--disable-droproot', action='store_true', default=False, help="Disable dropping root.")
	grp.add_option('--disable-dns-lookup', action='store_true', default=False, help="Disable DNS lookups.")
	grp.add_option('--disable-dns-retry', action='store_true', default=False, help="Disable DNS lookups.")
	grp.add_option('--disable-mdns-registration', action='store_true', default=False, help="Disable DNS lookups.")


	grp = ctx.add_option_group("NTP features")
	grp.add_option('--enable-leap-smear', action='store_true', default=False, help="Enable Leap Smearing.")
	grp.add_option('--enable-mssntp', action='store_true', default=False, help="Enable Samba MSS NTP support.")
	grp.add_option('--enable-lockclock', action='store_true', default=False, help="Enable NIST lockclock scheme.")

	grp = ctx.add_option_group("Refclock options")
	grp.add_option('--refclock', dest='refclocks', help="Comma-separated list of Refclock IDs to build (or \"all\")", type='string')
	grp.add_option('--list', action='store_true', default=False, help="List available Refclocks")

	grp = ctx.add_option_group("NTP developer options")
	grp.add_option('--enable-saveconfig', action='store_true', help="Enable configuration saving on exit.")
	grp.add_option('--build-version-tag', type='string', help="Append a tag to the version string.")
	grp.add_option('--cflags', type='string', action="callback", callback=callback_flags, help="Users should use CFLAGS in their environment.")
	grp.add_option('--ldflags', type='string', action="callback", callback=callback_flags, help="Users should use LDFLAGS in their environment.")
	grp.add_option('--enable-fortify', action='store_true', help="Enable HP Fortify.")
	grp.add_option('--fortify-flags', type='string', action='store', help="Fortify flags.")
	grp.add_option('--check', action='store_true', default=False, help="Run tests")



def configure(ctx):
	ctx.env.NTPS_VERSION_MAJOR = config["NTPS_VERSION_MAJOR"]
	ctx.env.NTPS_VERSION_MINOR = config["NTPS_VERSION_MINOR"]
	ctx.env.NTPS_VERSION_REV = config["NTPS_VERSION_REV"]

	ctx.env.NTPS_VERSION = "%s.%s.%s" % (ctx.env.NTPS_VERSION_MAJOR, ctx.env.NTPS_VERSION_MINOR, ctx.env.NTPS_VERSION_REV)
	ctx.define("NTPS_VERSION_MAJOR", ctx.env.NTPS_VERSION_MAJOR)
	ctx.define("NTPS_VERSION_MINOR", ctx.env.NTPS_VERSION_REV)
	ctx.define("NTPS_VERSION_REV", ctx.env.NTPS_VERSION_REV)


	ctx.load('waf', tooldir='pylib/')

	ctx.env.OPT_STORE = OPT_STORE

	cmd_configure(ctx)

from waflib.Build import BuildContext
class check(BuildContext):
	cmd = 'check'


def build(ctx):
	ctx.load('waf', tooldir='pylib/')
	ctx.load('bison')
	ctx.load('asciidoc', tooldir='pylib/')

	if ctx.env.ENABLE_DOC:
		ctx.recurse("docs")

	ctx.recurse("libisc")
	if ctx.env.REFCLOCK_PARSE: # Only required by the parse refclock
		ctx.recurse("libparse")
	ctx.recurse("libntp")
	ctx.recurse("ntpdig")
	ctx.recurse("libsodium")
	ctx.recurse("ntpd")
	ctx.recurse("ntpfrob")
	ctx.recurse("ntpq")
	ctx.recurse("ntpkeygen")
	ctx.recurse("ntptime")
	ctx.recurse("util")
	ctx.recurse("tests")


	subst_scripts = [
		"ntpwait/ntpwait",
		"ntptrace/ntptrace",
		"util/ntpsweep/ntpsweep",
	]

	ctx(
		features    = "subst",
		source      = subst_scripts,
		target	    = [x.replace(".in", "") for x in subst_scripts],
		chmod	    = Utils.O755
	)

	ctx.manpage(8, "ntpleapfetch/ntpleapfetch-man.txt")
	ctx.manpage(8, "ntpwait/ntpwait-man.txt")
	ctx.manpage(1, "ntptrace/ntptrace-man.txt")


	if ctx.cmd == "check":
		for bin, args in ctx.env.TEST_BIN:

			ctx(
				rule	= "%s -v %s" % (bin, args or " "),
				shell	= True
			)

# end
