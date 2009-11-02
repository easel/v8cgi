/**
 * v8cgi app file. This class represents (f)cgi binary; apache module inherits from it.
 */

#include <sstream>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <v8.h>
#include <v8-debug.h>

#ifdef FASTCGI
#  include <fcgi_stdio.h>
#endif

#include "js_app.h"
#include "js_system.h"
#include "js_io.h"
#include "js_macros.h"
#include "js_cache.h"
#include "js_path.h"

#ifndef windows
#   include <dlfcn.h>
#else
#   include <windows.h>
#	define dlsym(x, y) GetProcAddress((HMODULE)x, y)
#endif

/**
 * global.include = global.require + populate global object
 */
JS_METHOD(_include) {
	v8cgi_App * app = APP_PTR;
	v8::String::Utf8Value file(args[0]);
	v8::Handle<v8::Object> exports;
	try {
		exports = app->include(*file);
	} catch (std::string e) {
		return JS_EXCEPTION(e.c_str());
	}
	return exports;
}

/**
 * global.require = load module and return its (cached) exports
 */
JS_METHOD(_require) {
	v8cgi_App * app = APP_PTR;
	v8::String::Utf8Value file(args[0]);
	v8::Handle<v8::Object> exports;
	try {
		exports = app->require(*file);
	} catch (std::string e) {
		return JS_EXCEPTION(e.c_str());
	}
	return exports;
}

/**
 * global.onexit = add a function to be executed when context ends
 */
JS_METHOD(_onexit) {
	v8cgi_App * app = APP_PTR;
	if (!args[0]->IsFunction()) { return JS_EXCEPTION("Non-function passed to onexit()"); }
	v8::Persistent<v8::Function> fun = v8::Persistent<v8::Function>::New(v8::Handle<v8::Function>::Cast(args[0]));
	app->onexit.push_back(fun);
	return v8::Undefined();
}

/**
 * global.exit - terminate execution
 */
JS_METHOD(_exit) {
	v8cgi_App * app = APP_PTR;
	app->terminated = true;
	v8::V8::TerminateExecution();
	
	/* do something at least a bit complex so the stack guard can throw the termination exception */
	v8::Handle<v8::String> source = JS_STR("(function(){})()");
	v8::Script::Compile(source)->Run();

	return v8::Undefined();
}

/**
 * Format for command line arguments
 *
 * as you can see if you wish to pass any arguments to v8, you MUST
 * put a -- surrounded by whitespace after all the v8 arguments
 *
 * any arguments after the v8_args but before the program_file are
 * used by v8cgi.
 */
static const char * v8cgi_usage = "v8cgi [v8_args --] [-c path] [-d port] program_file [argument ...]";

/**
 * To be executed only once - process command line arguments, set config file name
 */
int v8cgi_App::init(int argc, char ** argv) {
	this->cfgfile = STRING(CONFIG_PATH);
	try {
		this->process_args(argc, argv);
	} catch (std::string e) {
		/* initialization error -> goes to stderr */
		this->error(e.c_str(), __FILE__, __LINE__); 
		return 1;
	}

	/**
	 * Reusable context is created only once, here.
 	 */
#ifdef REUSE_CONTEXT
	this->create_context();
#endif

	return 0;
}

/**
 * Initialize and setup the context. Executed during every request, prior to executing main request file.
 */
void v8cgi_App::prepare(char ** envp) {
	v8::HandleScope handle_scope;
	v8::Handle<v8::Object> g = JS_GLOBAL;

	g->Set(JS_STR("include"), v8::FunctionTemplate::New(_include)->GetFunction());
	g->Set(JS_STR("require"), v8::FunctionTemplate::New(_require)->GetFunction());
	g->Set(JS_STR("onexit"), v8::FunctionTemplate::New(_onexit)->GetFunction());
	g->Set(JS_STR("exit"), v8::FunctionTemplate::New(_exit)->GetFunction());
	g->Set(JS_STR("global"), g);
	g->Set(JS_STR("Config"), v8::Object::New());

	setup_system(g, envp);
	setup_io(g);
	
	/* config file */
	this->include(this->cfgfile); 
	
	this->autoload();
}

/**
 * Setup system.args array of arguments.
 * First item (system.args[0]) is the main file name
 */
void v8cgi_App::setup_args() {
	v8::Handle<v8::Object> args = v8::Array::New();
	args->Set(JS_INT(0), JS_STR(this->mainfile.c_str()));
	
	for (size_t i = 0; i < this->mainfile_args.size(); ++i) {
		args->Set(JS_INT(i+1), JS_STR(this->mainfile_args.at(i).c_str()));
	}
	v8::Handle<v8::Object> sys = JS_GLOBAL->Get(JS_STR("system"))->ToObject();
	sys->Set(JS_STR("args"), args);
}

/**
 * Load default libraries
 */
void v8cgi_App::autoload() {
	v8::HandleScope handle_scope;
	v8::Handle<v8::Value> config = JS_GLOBAL->Get(JS_STR("Config"));
	v8::Handle<v8::Array> list = v8::Handle<v8::Array>::Cast(this->get_config("libraryAutoload"));
	int cnt = list->Length();
	v8::Handle<v8::Value> dummy;
	
	for (int i=0;i<cnt;i++) {
		v8::Handle<v8::Value> item = list->Get(JS_INT(i));
		v8::String::Utf8Value name(item);
		std::string filename = *name;
		this->include(filename);
	}
}

/**
 * Process a request.
 * @param {bool} change Perform a chdir?
 * @param {char**} envp Environment
 */
int v8cgi_App::execute(bool change, char ** envp) {
	v8::HandleScope handle_scope;
	
	/**
	 * Context must be clened before reusing
	 */
#ifdef REUSE_CONTEXT
	this->clear_global();
#else
	/**
	 * No reusing -> new context 
	 */
	this->create_context();
#endif

	this->terminated = false;

	try {
		/* prepare JS environment: config, default libraries */
		this->prepare(envp); 
		/* try to locate main file */
		this->findmain();
	} catch (std::string e) {
		/* error with config file or default libs -> goes to stderr */
		this->error(e.c_str(), __FILE__, __LINE__); 
		this->finish();
		return 1;
	}
	
	/* if requested, chdir */
	if (change) { path_chdir(path_dirname(this->mainfile)); } 
	
	/* pass command line arguments to system.args */
	this->setup_args();
	
	/* setup builtin request and response, if running as CGI */
	if (this->http()) {
		/* do we have a forced http handler? */
		v8::Handle<v8::Value> handler = this->get_config("httpHandler");
		if (handler->ToBoolean()->IsTrue()) {
			v8::String::Utf8Value name(handler);
			this->mainfile = *name;
		}
	}

	try {
		/* require the mainfile */
		this->require(this->mainfile); 
	} catch (std::string e) {
		/* error when executing main file */
		v8::Handle<v8::Value> show = this->get_config("showErrors");
		if (show->ToBoolean()->IsTrue()) {
			this->js_error(e.c_str()); 
		} else {
			this->error(e.c_str(), __FILE__, __LINE__);
		}
		this->finish();
		return 1;
	}
	
	this->finish();
	return 0;
}

/**
 * End request
 */
void v8cgi_App::finish() {
	/* user callbacks */
	for (unsigned int i=0; i<this->onexit.size(); i++) {
		this->onexit[i]->Call(JS_GLOBAL, 0, NULL);
		this->onexit[i].Dispose();
		this->onexit[i].Clear();
	}
	this->onexit.clear();

	/* garbage collection */
	this->gc.finish();
	
	/* export cache */
	this->cache.clearExports();
	
	/* paths */
	while (!this->paths.empty()) { this->paths.pop(); }
	
#ifndef REUSE_CONTEXT
	/**
	 * Delete current context
	 */
	this->delete_context();
#endif
}

/**
 * To include a module, we first require it and than populate global object with retrieved data
 * @param {std::string} name
 */
v8::Handle<v8::Object> v8cgi_App::include(std::string name) {
	v8::HandleScope handle_scope;

	v8::Handle<v8::Object> exports = this->require(name);
	v8::Handle<v8::Array> names = exports->GetPropertyNames();
	for (unsigned i=0;i<names->Length();i++) {
		v8::Handle<v8::Value> name = names->Get(JS_INT(i));
		JS_GLOBAL->Set(name, exports->Get(name));		
	}

	return handle_scope.Close(exports);
}

/**
 * Require a module
 * @param {std::string} name
 */
v8::Handle<v8::Object> v8cgi_App::require(std::string name) {
	v8::HandleScope handle_scope;
#ifdef VERBOSE
	printf("[require] looking for '%s'\n", name.c_str()); 
#endif	
	std::string filename = this->resolve_module(name);
#ifdef VERBOSE
	printf("[require] resolved as '%s'\n", filename.c_str()); 
#endif	
	
	if (filename == "") {
		std::string s = "Cannot find '";
		s += name;
		s += "'";
		throw s;
	}
	
	v8::Handle<v8::Object> exports = this->cache.getExports(filename);
	/* check if exports are cached */
	if (!exports.IsEmpty()) { return exports; }

	/* add new blank exports to cache */
	exports = v8::Object::New();
	this->cache.addExports(filename, exports);

	/* create the "module" variable" */
	v8::Handle<v8::Object> module = v8::Object::New();
	module->Set(JS_STR("id"), JS_STR(filename.c_str()));

	/* prepare path to stack */
	this->paths.push(path_dirname(filename)); 
	
	/* result */
	v8::Handle<v8::Value> data; 

	try {
		size_t index = filename.find_last_of(".");
		std::string ext = filename.substr(index+1);
		if (ext == STRING(DSO_EXT)) {
			data = this->load_dso(filename, exports, module);
		} else {
			data = this->load_js(filename, exports, module);
		}
	} catch (std::string e) {
		this->paths.pop(); 
		/* remove from export cache */
		this->cache.removeExports(filename);
		/* rethrow */
		throw e; 
	}

	/* execution ended, remove top path */
	this->paths.pop(); 
	return handle_scope.Close(exports);
}

/**
 * Include a js module
 */
v8::Handle<v8::Value> v8cgi_App::load_js(std::string filename, v8::Handle<v8::Object> exports, v8::Handle<v8::Object> module) {
	v8::HandleScope handle_scope;
	v8::TryCatch tc;

	/* compiled script wrapped in anonymous function */
	v8::Handle<v8::Script> script = this->cache.getScript(filename);
	
	/* compilation error? */
	if (tc.HasCaught()) {
		throw this->format_exception(&tc);
	} else {
		/* run the script, no error should happen here */
		v8::Handle<v8::Value> result = script->Run();

		v8::Handle<v8::Function> fun = v8::Handle<v8::Function>::Cast(result);
		v8::Handle<v8::Value> params[2] = {exports, module}; 
		result = fun->Call(exports, 2, params);

		/* runtime error in inner code */
		if (tc.HasCaught() && !this->terminated) { throw this->format_exception(&tc); }
		return exports;
	}
}

/**
 * Include a dso module
 */
v8::Handle<v8::Value> v8cgi_App::load_dso(std::string filename, v8::Handle<v8::Object> exports, v8::Handle<v8::Object> module) {
	v8::HandleScope handle_scope;
	void * handle = this->cache.getHandle(filename);

	typedef void (*init_t)(v8::Handle<v8::Object>, v8::Handle<v8::Object>);

	init_t func = (init_t) dlsym(handle, "init");	
	
	if (!func) {
		std::string error = "Cannot initialize shared library '";
		error += filename;
		error += "'";
		throw error;
	}
	
	func(exports, module);
	return handle_scope.Close(exports);	
}

/**
 * Try to report error via JS means, instead of stderr 
 */
void v8cgi_App::js_error(std::string message) {
	int cgi = 0;
	v8::Local<v8::Function> fun;
	v8::Local<v8::Value> context = JS_GLOBAL->Get(JS_STR("response"));
	if (context->IsObject()) {
		v8::Local<v8::Value> print = context->ToObject()->Get(JS_STR("write"));
		if (print->IsFunction()) {
			fun = v8::Local<v8::Function>::Cast(print);
			cgi = 1;
		}
	}
	if (!cgi) {
		context = JS_GLOBAL->Get(JS_STR("system"));
		if (context->IsUndefined()) {
			this->error(message.c_str(), __FILE__, __LINE__);
			return;
		} else {
			fun = v8::Local<v8::Function>::Cast(context->ToObject()->Get(JS_STR("stdout")));
		}
	}
	
	v8::Handle<v8::Value> data[1] = { JS_STR(message.c_str()) };
	fun->Call(context->ToObject(), 1, data);
}

/**
 * Fully expand/resolve module name
 */
std::string v8cgi_App::resolve_module(std::string name) {
	if (!name.length()) { return std::string(""); }

	if (path_isabsolute(name)) {
		/* v8cgi non-standard extension - absolute path */
#ifdef VERBOSE
		printf("[resolve_module] expanded to '%s'\n", name.c_str()); 
#endif	
		return this->find_extension(name);
	} else if (name.at(0) == '.') {
		/* local module, relative to current path */
		std::string path = this->paths.empty() ? path_getcwd() : this->paths.top();
		path += "/";
		path += name;
#ifdef VERBOSE
		printf("[resolve_module] expanded to '%s'\n", path.c_str()); 
#endif	
		return this->find_extension(path);
	} else {
		/* global module, relative to some of the given searchpaths */
		v8::Handle<v8::Object> req = JS_GLOBAL->Get(JS_STR("require"))->ToObject();
		v8::Handle<v8::Value> paths = req->Get(JS_STR("paths"));
		if (paths->IsUndefined()) { paths = this->get_config("libraryPath"); } /* backwards compatibility */
		
		/* convert to array of search paths */
		v8::Handle<v8::Array> arr;
		if (paths->IsArray()) {
			arr = v8::Handle<v8::Array>::Cast(paths);
		} else {
			arr = v8::Array::New(1);
			arr->Set(JS_INT(0), paths);
		}
		
		int length = arr->Length();
		v8::Handle<v8::Value> prefix;
		
		for (int i=0;i<length;i++) {
			prefix = arr->Get(JS_INT(i));
			v8::String::Utf8Value pfx(prefix);
			std::string path(*pfx);
			path += "/";
			path += name;
#ifdef VERBOSE
		printf("[resolve_module] expanded to '%s'\n", path.c_str()); 
#endif	
			path = this->find_extension(path);
			if (path != "") { return path; }
		}
		
		return std::string("");
	}
}

/**
 * Try to adjust file's extension in order to locate an existing file
 */
std::string v8cgi_App::find_extension(std::string path) {
	/* remove /./, /../ etc */
	std::string fullPath = path_normalize(path); 
	if (path_file_exists(fullPath)) { return fullPath; }
	
	/* suffixes to be tried if file does not exist */
	const char * suffixes[] = {"js", STRING(DSO_EXT)};

	/* try all suffixes */
	std::string path2; 
	for (int j=0;j<2;j++) {
		path2 = fullPath;
		path2 += ".";
		path2 += suffixes[j];
		if (path_file_exists(path2)) { return path2; }
	}
	return std::string("");
}

/**
 * Create global.response and global.request
 */
bool v8cgi_App::http() {
	v8::Handle<v8::Object> sys = JS_GLOBAL->Get(JS_STR("system"))->ToObject();
	v8::Handle<v8::Value> env = sys->ToObject()->Get(JS_STR("env"));
	v8::Handle<v8::Value> ss = env->ToObject()->Get(JS_STR("SERVER_SOFTWARE"));
	if (!ss->IsString()) { return false; }
	v8::Handle<v8::Object> http = JS_GLOBAL->Get(JS_STR("HTTP"))->ToObject();
	v8::Handle<v8::Value> req = http->Get(JS_STR("ServerRequest"));
	v8::Handle<v8::Value> res = http->Get(JS_STR("ServerResponse"));
	v8::Handle<v8::Function> reqf = v8::Handle<v8::Function>::Cast(req);
	v8::Handle<v8::Function> resf = v8::Handle<v8::Function>::Cast(res);

	v8::Handle<v8::Value> reqargs[] = { 
		sys->Get(JS_STR("stdin")),
		sys->Get(JS_STR("env"))
	};
	v8::Handle<v8::Value> resargs[] = { 
		sys->Get(JS_STR("stdout"))
	};

	JS_GLOBAL->Set(JS_STR("response"), resf->NewInstance(1, resargs));
	JS_GLOBAL->Set(JS_STR("request"), reqf->NewInstance(2, reqargs));
	return true;
}

/**
 * Try to locate main file
 */
void v8cgi_App::findmain() {
	v8::HandleScope handle_scope;
	v8::Handle<v8::Value> sys = JS_GLOBAL->Get(JS_STR("system"));
	v8::Handle<v8::Value> env = sys->ToObject()->Get(JS_STR("env"));
	v8::Handle<v8::Value> pt = env->ToObject()->Get(JS_STR("PATH_TRANSLATED"));
	v8::Handle<v8::Value> sf = env->ToObject()->Get(JS_STR("SCRIPT_FILENAME"));
	if (pt->IsString()) {
		v8::String::Utf8Value jsname(pt);
		this->mainfile = *jsname;
	} else if (sf->IsString()) {
		v8::String::Utf8Value jsname(sf);
		this->mainfile = *jsname;
	}
	
	if (!this->mainfile.length()) { throw std::string("Cannot locate main file."); }
}

/**
 * Process command line arguments.
 * @returns {bool} True if we were able to understand arguments and 
 * set a mainfile. False if usage was invalid.
 */
void v8cgi_App::process_args(int argc, char ** argv) {
	std::string err = "Invalid command line usage.\n";
	err += "Correct usage: ";
	err += v8cgi_usage;

	/* see the v8cgi_usage definition for the format */
	
	/* we must have at least one arg */
	if (argc == 1) { throw err; }
	
	int index = 0;
	
	/* see if we have v8 options */
	bool have_v8args = false;
	for (; index < argc; ++index) {
		/* FIXME: if there is a standalone "--" after the name of the script
		 then this breaks.  I can't figure out a solution to this, so
		 for now we don't support any standalone "--" after the script name.
		 One solution (and the way it currently works) is to require "--"
		 before all other args even if no v8 args are used, but that seems
		 I don't like this, but it is where we are right now. */
		if (std::string(argv[index]) == "--") {
			/* treat all previous arguments as v8 arguments */
			int v8argc = index;
			v8::V8::SetFlagsFromCommandLine(&v8argc, argv, true);
			index++; /* skip over the "--" */
			have_v8args = true;
			break;
		}
	}
	
	/* if there were no v8 args, then reset index to the first argument */
	if (!have_v8args) index = 1;
	
	
	/* scan for v8cgi-specific arguments now */
	while (1) {
		/* we haven't found a mainfile yet, so there MUST be more arguments */
		if (index >= argc) { throw err; }
		std::string optname(argv[index]);
		if (optname[0] != '-') { break; } /* not starting with "-" => mainfile */
		if (optname.length() != 2) { throw err; } /* one-character options only */
		index++; /* skip the option name */
		
		if (index >= argc) { throw err; } /* missing option value */
		
		switch (optname[1]) {
			case 'c':
				this->cfgfile = argv[index];		
#ifdef VERBOSE
				printf("cfgfile: %s\n", argv[index]);
#endif
			break;
			
			case 'd':
				v8::Debug::EnableAgent("v8cgi", atoi(argv[index]));
#ifdef VERBOSE
				printf("port: %s\n", argv[index]);
#endif
			break;
			
			default:
				throw err;
			break;
		}
		
		index++; /* skip the option value */
	} 
	
	/* argv[index] MUST be the program_file.  If it doesn't
	 exist then we have an error. */
	if (index >= argc) {
		throw err;
	} else {
		this->mainfile = argv[index];
		/* expand mainfile to absolute path */
		if (!path_isabsolute(this->mainfile)) {
			std::string tmp = path_getcwd();
			tmp += "/";
			tmp += this->mainfile;
			this->mainfile = path_normalize(this->mainfile);
		}
		index++; /* skip over the program_file */
	}
	
	/* all the rest of the arguments are arguments to the program_file */
	for (; index < argc; ++index) {
#ifdef VERBOSE
		printf("program_arg: %s\n", argv[index]);
#endif
		this->mainfile_args.push_back(std::string(argv[index]));
	}
}

/** 
 * Convert JS exception to c string 
 */
std::string v8cgi_App::format_exception(v8::TryCatch* try_catch) {
	v8::HandleScope handle_scope;
	v8::String::Utf8Value exception(try_catch->Exception());
	v8::Handle<v8::Message> message = try_catch->Message();
	std::string msgstring = "";
	std::stringstream ss;

	if (message.IsEmpty()) {
		msgstring += *exception;
	} else {
		v8::String::Utf8Value filename(message->GetScriptResourceName());
		int linenum = message->GetLineNumber();
		msgstring += *filename;
		msgstring += ":";
		ss << linenum;
		msgstring += ss.str();
		msgstring += ": ";
		msgstring += *exception;
	}
	return msgstring;
}

/**
 * Creates a new context
 */
void v8cgi_App::create_context() {
	v8::HandleScope handle_scope;
	v8::Handle<v8::ObjectTemplate> globaltemplate = v8::ObjectTemplate::New();
	globaltemplate->SetInternalFieldCount(2);
	this->context = v8::Context::New(NULL, globaltemplate);
	this->context->Enter();

	GLOBAL_PROTO->SetInternalField(0, v8::External::New((void *) this)); 
	GLOBAL_PROTO->SetInternalField(1, v8::External::New((void *) &(this->gc))); 
}

/**
 * Deletes the existing context
 */
void v8cgi_App::delete_context() {
	this->context->Exit();
	this->context.Dispose();
	this->context.Clear();
}

/**
 * Removes all "garbage" from the global object
 */
void v8cgi_App::clear_global() {
	v8::Handle<v8::Array> keys = JS_GLOBAL->GetPropertyNames();
	int length = keys->Length();
	for (int i=0;i<length;i++) {
		v8::Handle<v8::String> key = keys->Get(JS_INT(i))->ToString();
		JS_GLOBAL->ForceDelete(key);
	}
}

/**
 * Retrieve a configuration value
 */
v8::Handle<v8::Value> v8cgi_App::get_config(std::string name) {
	v8::Handle<v8::Value> config = JS_GLOBAL->Get(JS_STR("Config"));
	return config->ToObject()->Get(JS_STR(name.c_str()));
}

/**
 * Default stdin routine
 */
size_t v8cgi_App::reader(char * destination, size_t amount) {
	return fread((void *) destination, sizeof(char), amount, stdin);
}

/**
 * Default stdout routine
 */
size_t v8cgi_App::writer(const char * data, size_t amount) {
	return fwrite((void *) data, sizeof(char), amount, stdout);
}

/**
 * Default stderr routine
 */
void v8cgi_App::error(const char * data, const char * file, int line) {
	fwrite((void *) data, sizeof(char), strlen(data), stderr);
	fwrite((void *) "\n", sizeof(char), 1, stderr);
}
