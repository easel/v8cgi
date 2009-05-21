#include <v8.h>
#include <string>
#include <stdlib.h>

#ifdef FASTCGI
#  include <fcgi_stdio.h>
#endif

#include "js_macros.h"
#include "js_common.h"
#include "js_app.h"
#include "js_system.h"
#include "js_path.h"

#ifndef HAVE_SLEEP
#	include <windows.h>
#	define sleep(num) { Sleep(num * 1000); }
#endif

namespace {

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

JS_METHOD(_system) {
	if (args.Length() != 1) {
		return JS_EXCEPTION("Wrong argument count. Use System.system(\"command\")");
	}
	
	v8::String::Utf8Value cmd(args[0]);
	int result = system(*cmd);
	return JS_INT(result);
}

JS_METHOD(_getcwd) {
	return JS_STR(path_getcwd().c_str());
}

JS_METHOD(_sleep) {
	int num = args[0]->Int32Value();
	sleep(num);
	return v8::Undefined();
}

/* 
JS_METHOD(_usleep) {
	v8::HandleScope handle_scope;
	int num = args[0]->Int32Value();
	usleep(num);
	return v8::Undefined();
}
*/

}

extern char ** environ;

void setup_system(v8::Handle<v8::Object> global, char ** envp) {
	v8::HandleScope handle_scope;
	v8::Handle<v8::ObjectTemplate> systemt = v8::ObjectTemplate::New();
	v8::Handle<v8::Object> system = systemt->NewInstance();

	v8::Handle<v8::ObjectTemplate> envt = v8::ObjectTemplate::New();
	v8::Handle<v8::Object> env = envt->NewInstance();

	global->Set(JS_STR("System"), system);
	system->Set(JS_STR("stdin"), v8::FunctionTemplate::New(_stdin)->GetFunction());
	system->Set(JS_STR("stdout"), v8::FunctionTemplate::New(_stdout)->GetFunction());
	system->Set(JS_STR("stderr"), v8::FunctionTemplate::New(_stderr)->GetFunction());
	system->Set(JS_STR("system"), v8::FunctionTemplate::New(_system)->GetFunction());
	system->Set(JS_STR("getcwd"), v8::FunctionTemplate::New(_getcwd)->GetFunction());
	system->Set(JS_STR("sleep"), v8::FunctionTemplate::New(_sleep)->GetFunction());
/*	system->Set(JS_STR("usleep"), v8::FunctionTemplate::New(_usleep)->GetFunction()); */
	system->Set(JS_STR("env"), env);
	
	std::string name, value;
	bool done;
	int i,j;
	char ch;
	
	char ** e = envp;
	if (e == NULL) { e = environ; }
	if (e == NULL) { return; }
	
	for (i = 0; e[i] != NULL; i++) {
		done = false;
		name = "";
		value = "";
		for (j = 0; e[i][j] != '\0'; j++) {
			ch = e[i][j];
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
