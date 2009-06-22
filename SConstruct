import sys
import os

# base source files
sources = [
	"js_common.cc",
	"js_system.cc",
	"js_io.cc",
	"js_cache.cc",
	"js_gc.cc",
	"js_app.cc",
	"js_path.cc"
]
sources = [ "src/%s" % s for s in sources ]

config_path = ""
mysql_include = ""
os_string = ""
apache_include = ""
apr_include = ""

# platform-based default values
if sys.platform.find("win") != -1 and sys.platform.find("darwin") == -1:
	mysql_include = "c:/"
	apache_include = "c:/"
	apr_include = "c:/"
	config_path = "c:/v8cgi.conf"
	os_string = "windows"
else:
	mysql_include = "/usr/include/mysql"
	apache_include = "/usr/include/apache2"
	apr_include = "/usr/include/apr-1.0"
	config_path = "/etc/v8cgi.conf"
	os_string = "posix"
# endif 

# command line options
opts = Options()
opts.Add(BoolOption("mysql", "MySQL library", 1))
opts.Add(BoolOption("gd", "GD library", 1))
opts.Add(BoolOption("sqlite", "SQLite library", 1))
opts.Add(BoolOption("socket", "Socket library", 1))
opts.Add(BoolOption("module", "Build Apache module", 1))
opts.Add(BoolOption("cgi", "Build CGI binray", 1))
opts.Add(BoolOption("fcgi", "FastCGI support (for CGI binary)", 0))
opts.Add(BoolOption("debug", "Debugging support", 0))
opts.Add(BoolOption("verbose", "Verbose debugging messages", 0))
opts.Add(BoolOption("reuse_context", "Reuse context for multiple requests", 0))

opts.Add(("mysql_path", "MySQL header path", mysql_include))
opts.Add(("apache_path", "Apache header path", apache_include))
opts.Add(("apr_path", "APR header path", apr_include))

opts.Add(PathOption("v8_path", "Directory with V8", "../v8"))
opts.Add(EnumOption("os", "Operating system", os_string, allowed_values = ["windows", "posix"]))
opts.Add(("config_file", "Config file", config_path))

env = Environment(options=opts)

Help(opts.GenerateHelpText(env))
conf = Configure(env)

# adjust variables based on user selection
if conf.CheckCHeader("unistd.h", include_quotes = "<>"):
	env.Append(CPPDEFINES = ["HAVE_UNISTD_H"])

if conf.CheckCHeader("dirent.h", include_quotes = "<>"):
	env.Append(CPPDEFINES = ["HAVE_DIRENT_H"])

if conf.CheckCHeader("sys/mman.h", include_quotes = "<>"):
	env.Append(CPPDEFINES = ["HAVE_MMAN_H"])

if not conf.CheckFunc("close"):
	env.Append(CPPDEFINES = ["HAVE_WINSOCK"])

if conf.CheckFunc("mkdir"):
	env.Append(CPPDEFINES = ["HAVE_MKDIR"])

if conf.CheckFunc("rmdir"):
	env.Append(CPPDEFINES = ["HAVE_RMDIR"])

if conf.CheckFunc("chdir"):
	env.Append(CPPDEFINES = ["HAVE_CHDIR"])

if conf.CheckFunc("getcwd"):
	env.Append(CPPDEFINES = ["HAVE_GETCWD"])

if conf.CheckFunc("sleep"):
    env.Append(CPPDEFINES = ["HAVE_SLEEP"])
	
env = conf.Finish()

# default values
env.Append(
	LIBS = ["v8"], 
	CPPPATH = ["src"], 
	CCFLAGS = ["-Wall", "-O3", "-m32"], 
	CPPDEFINES = [],
	LIBPATH = "",
	LINKFLAGS = ["-m32"]
)

if env["os"] == "posix":
	env.Append(
		LIBS = ["pthread", "rt"]
	)
# if

if ((env["os"] != "windows") and not (conf.CheckLib("v8"))):
	print "Cannot find V8 library!"
	sys.exit(1)
# if

env.Append(
	CPPDEFINES = [
		"CONFIG_PATH=" + env["config_file"], 
		env["os"] 
	],
	CPPPATH = env["v8_path"] + "/include",
	LIBPATH = env["v8_path"]
)

if env["fcgi"] == 1:
	env.Append(
		LIBS = ["fcgi"],
		CPPPATH = ["src/fcgi/include"],
		CPPDEFINES = ["FASTCGI"]
	)
# if

if env["os"] == "posix":
	env.Append(LIBS = ["dl"])
# if

if env["os"] == "windows":
	env.Append(
		LIBS = ["ws2_32"],
		CPPDEFINES = ["USING_V8_SHARED", "WIN32"],
		LIBPATH = os.environ["LIBPATH"].split(";"),
		CPPPATH = os.environ["INCLUDE"].split(";")
	)
# if

if env["debug"] == 1:
	env.Append(
		CCFLAGS = ["-O0", "-g", "-g3", "-gdwarf-2", "-pg"],
		LINKFLAGS = ["-pg"]
	)
# if

if env["verbose"] == 1:
	env.Append(
		CPPDEFINES = ["VERBOSE"]
	)
# if

if env["reuse_context"] == 1:
	env.Append(
		CPPDEFINES = ["REUSE_CONTEXT"]
	)
# if

if env["mysql"] == 1:
	e = env.Clone()
	if env["os"] == "windows":
		e.Append(
			LIBS = ["wsock32", "user32", "advapi32"],
			LINKFLAGS = ["/nodefaultlib:\"libcmtd\""]
		)
	# if
	e.Append(
		CPPPATH = env["mysql_path"],
		LIBS = "mysqlclient"
	)
	e.SharedLibrary(
		target = "lib/mysql", 
		source = ["src/js_gc.cc", "src/lib/mysql/js_mysql.cc"],
		SHLIBPREFIX=""
	)
# if

if env["sqlite"] == 1:
	e = env.Clone()
	e.Append(
		LIBS = "sqlite3"
	)
	e.SharedLibrary(
		target = "lib/sqlite", 
		source = ["src/js_gc.cc", "src/lib/sqlite/js_sqlite.cc"],
		SHLIBPREFIX=""
	)
# if

if env["gd"] == 1:
	e = env.Clone()
	libname = ("gd", "bgd")[env["os"] == "windows"]
	e.Append(
		LIBS = [libname]
	)
	e.SharedLibrary(
		target = "lib/gd", 
		source = ["src/js_common.cc", "src/lib/gd/js_gd.cc"],
		SHLIBPREFIX=""
	)
# if

if env["socket"] == 1:
	e = env.Clone()
	e.SharedLibrary(
		target = "lib/socket", 
		source = ["src/lib/socket/js_socket.cc"],
		SHLIBPREFIX=""
	)
# if

if env["module"] == 1:
	e = env.Clone()
	e.Append(
		CPPPATH = [env["apache_path"], env["apr_path"]]
	)
	if env["os"] == "windows":
		e.Append(
			LIBS = ["libapr-1", "libhttpd"]
		)
	# if
	
	s = []
	s[:] = sources[:]
	s.append("src/mod_v8cgi.cc")
	e.SharedLibrary(
		target = "mod_v8cgi", 
		source = s,
		SHLIBPREFIX=""
	)
# if

if env["cgi"] == 1:
	sources.append("src/v8cgi.cc")
	env.Program(
		source = sources, 
		target = "v8cgi"
	)
# if
