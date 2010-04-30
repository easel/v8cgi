#include <v8.h>
#include <string>

#ifdef FASTCGI
#  include <fcgi_stdio.h>
#endif

#include "macros.h"
#include "common.h"
#include "app.h"
#include "system.h"
#include "path.h"
#include <sys/time.h>

#ifndef HAVE_SLEEP
#	include <windows.h>
#	define sleep(num) { Sleep(num * 1000); }
#	define usleep(num) { Sleep(num / 1000); }
#endif

namespace {

/**
 * Read characters from stdin
 * @param {int} count How many; 0 == all
 * @param {bool} [arr=false] Return as array of bytes?
 */
JS_METHOD(_stdin) {
	v8cgi_App * app = APP_PTR;

	size_t count = 0;
	if (args.Length() && args[0]->IsNumber()) {
		count = args[0]->IntegerValue();
	}
	
	std::string data;
	size_t size = 0;

   if (count == 0) { /* all */
		size_t tmp;
		char * buf = new char[1024];
		do {
			tmp = app->reader(buf, sizeof(buf));
			size += tmp;
			data.insert(data.length(), buf, tmp);
		} while (tmp == sizeof(buf));
		delete[] buf;
	} else {
		char * tmp = new char[count];
		size = app->reader(tmp, count);
		data.insert(0, tmp, size);
		delete[] tmp;
	}
	
	if (args.Length() > 1 && args[1]->IsTrue()) {
		return JS_CHARARRAY((char *) data.data(), size);
	} else {
		return JS_STR(data.data(), size);
	}
}

/**
 * Dump data to stdout
 * @param {string|int[]} String or array of bytes
 */
JS_METHOD(_stdout) {
	v8cgi_App * app = APP_PTR;
	if (args[0]->IsArray()) {
		v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(args[0]);
		uint32_t len = arr->Length();
		std::string data;
		for (unsigned int i=0;i<len;i++) {
			data += (char) arr->Get(JS_INT(i))->Int32Value();
		}
		app->writer((char *) data.data(), len);
	} else {
		v8::String::Utf8Value str(args[0]);
		app->writer(*str, str.length());
	}
	return v8::Undefined();
}

JS_METHOD(_stderr) {
	v8cgi_App * app = APP_PTR;
	v8::String::Utf8Value str(args[0]);
	v8::String::Utf8Value f(args[1]);
	int line = args[2]->Int32Value();
	app->error(*str, *f, line);
	return v8::Undefined();
}

JS_METHOD(_getcwd) {
	return JS_STR(path_getcwd().c_str());
}

/**
 * Sleep for a given number of seconds
 */
JS_METHOD(_sleep) {
	int num = args[0]->Int32Value();
	sleep(num);
	return v8::Undefined();
}

/**
 * Sleep for a given number of microseconds
 */
JS_METHOD(_usleep) {
	v8::HandleScope handle_scope;
	int num = args[0]->Int32Value();
	usleep(num);
	return v8::Undefined();
}

/**
 * Return the number of microseconds that have elapsed since the epoch.
 */
JS_METHOD(_getTimeInMicroseconds) {
	struct timeval tv;
	gettimeofday(&tv, 0);
	char buffer[24];
	sprintf(buffer, "%lu%06lu", tv.tv_sec, tv.tv_usec);
	return JS_STR(buffer)->ToNumber();
}

JS_METHOD(_flush) {
	v8cgi_App * app = APP_PTR;
	if (!app->flush()) { return JS_ERROR("Can not flush stdout"); }
	return v8::Undefined();
}

}

void setup_system(v8::Handle<v8::Object> global, char ** envp, std::string mainfile, std::vector<std::string> args) {
	v8::HandleScope handle_scope;
	v8::Handle<v8::Object> system = v8::Object::New();
	v8::Handle<v8::Object> env = v8::Object::New();
	global->Set(JS_STR("system"), system);
	
	/**
	 * Create system.args 
	 */
	v8::Handle<v8::Array> arr = v8::Array::New();
	arr->Set(JS_INT(0), JS_STR(mainfile.c_str()));
	for (size_t i = 0; i < args.size(); ++i) {
		arr->Set(JS_INT(i+1), JS_STR(args.at(i).c_str()));
	}
	system->Set(JS_STR("args"), arr);
	

	v8::Handle<v8::Function> stdout_function = v8::FunctionTemplate::New(_stdout)->GetFunction();
	stdout_function->Set(JS_STR("flush"), v8::FunctionTemplate::New(_flush)->GetFunction());
	system->Set(JS_STR("stdout"), stdout_function);
	system->Set(JS_STR("stdin"), v8::FunctionTemplate::New(_stdin)->GetFunction());
	system->Set(JS_STR("stderr"), v8::FunctionTemplate::New(_stderr)->GetFunction());
	system->Set(JS_STR("getcwd"), v8::FunctionTemplate::New(_getcwd)->GetFunction());
	system->Set(JS_STR("sleep"), v8::FunctionTemplate::New(_sleep)->GetFunction());
	system->Set(JS_STR("usleep"), v8::FunctionTemplate::New(_usleep)->GetFunction());
	system->Set(JS_STR("getTimeInMicroseconds"), v8::FunctionTemplate::New(_getTimeInMicroseconds)->GetFunction());
	system->Set(JS_STR("env"), env);
	
	std::string name, value;
	bool done;
	int i,j;
	char ch;
	
	/* extract environment variables and create JS object */
	for (i = 0; envp[i] != NULL; i++) {
		done = false;
		name = "";
		value = "";
		for (j = 0; envp[i][j] != '\0'; j++) {
			ch = envp[i][j];
			if (!done) {
				if (ch == '=') {
					done = true;
				} else {
					name += ch;
				}
			} else {
				value += ch;
			}
		}
		env->Set(JS_STR(name.c_str()), JS_STR(value.c_str()));
	}
}
