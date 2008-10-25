/*
 * v8cgi main file. Loosely based on V8's "shell" sample app.
 */

#include <v8.h>
#include "system.h"
#include "io.h"
#include "jsmysql.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#define _STRING(x) #x
#define STRING(x) _STRING(x)

v8::Handle<v8::Array> onexit;

void die(int code) {
    int max = onexit->Length();
    v8::Handle<v8::Function> fun;
    for (int i=0;i<max;i++) {
	fun = v8::Handle<v8::Function>::Cast(onexit->Get(v8::Integer::New(i)));
	fun->Call(v8::Context::GetCurrent()->Global(), 0, NULL);
    }
    exit(code);
}

v8::Handle<v8::String> read_file(const char* name) {
  FILE* file = fopen(name, "rb");
  if (file == NULL) return v8::Handle<v8::String>();

  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = fread(&chars[i], 1, size - i, file);
    i += read;
  }
  fclose(file);

  /* remove shebang line */  
  std::string str = chars;
  if ( str.find('#',0) == 0 && str.find('!',1) == 1 ) {
    int pfix = str.find('\n',0);
    str.erase(0,pfix);
  };
  
  v8::Handle<v8::String> result = v8::String::New(str.c_str(), str.size());
  delete[] chars;
  return result;
}

void report_exception(v8::TryCatch* try_catch) {
  v8::HandleScope handle_scope;
  v8::String::Utf8Value exception(try_catch->Exception());
  v8::Handle<v8::Message> message = try_catch->Message();

  char * msg;
  std::string msgstring = "";

  if (message.IsEmpty()) {
    asprintf(&msg, "%s\n", *exception);
    msgstring += msg;
  } else {
    // Print (filename):(line number): (message).
    v8::String::Utf8Value filename(message->GetScriptResourceName());
    int linenum = message->GetLineNumber();
    asprintf(&msg, "%s:%i: %s\n", *filename, linenum, *exception);
    msgstring += msg;
    // Print line of source code.
    v8::String::Utf8Value sourceline(message->GetSourceLine());
    asprintf(&msg, "%s\n", *sourceline);
    msgstring += msg;

    // Print wavy underline (GetUnderline is deprecated).
    int start = message->GetStartColumn();
    for (int i = 0; i < start; i++) {
      msgstring += " ";
    }
    int end = message->GetEndColumn();
    for (int i = start; i < end; i++) {
      msgstring += "^";
    }
    msgstring += "\n";
  }
  
  int cgi = 0;
  v8::Local<v8::Function> fun;
  v8::Local<v8::Value> context = v8::Context::GetCurrent()->Global()->Get(v8::String::New("response"));
  if (context->IsObject()) {
    v8::Local<v8::Value> print = context->ToObject()->Get(v8::String::New("error"));
    if (print->IsObject()) {
	fun = v8::Local<v8::Function>::Cast(print);
	cgi = 1;
    }
  }
  if (!cgi) {
    context = v8::Context::GetCurrent()->Global()->Get(v8::String::New("System"));
    fun = v8::Local<v8::Function>::Cast(context->ToObject()->Get(v8::String::New("stdout")));
  }
  
  v8::Handle<v8::Value> data[1];
  data[0] = v8::String::New(msgstring.c_str());
  fun->Call(context->ToObject(), 1, data);
}

int execute_file(char * str) {
    v8::HandleScope handle_scope;
    v8::TryCatch try_catch;
    v8::Handle<v8::String> name = v8::String::New(str);
    v8::Handle<v8::String> source = read_file(str);

    if (source.IsEmpty()) {
	printf("Error reading '%s'\n", str);
        return 1;
    }
    
    v8::Handle<v8::Script> script = v8::Script::Compile(source, name);
    if (script.IsEmpty()) {
	report_exception(&try_catch);
	return 1;
    } else {
	v8::Handle<v8::Value> result = script->Run();
	if (result.IsEmpty()) {
	    report_exception(&try_catch);
	    return 1;
	}
    }
    return 0;
}

void init() {
    int result = execute_file(STRING(CONFIG_PATH));
    if (result) { 
	printf("Cannot load configuration, quitting...\n");
	die(1);
    }
}

v8::Handle<v8::Value> _include(const v8::Arguments& args) {
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope;
    v8::String::Utf8Value file(args[i]);
    execute_file(*file); /* in include, we don't care about returned values */
  }
  return v8::Undefined();
}


v8::Handle<v8::Value> _exit(const v8::Arguments& args) {
  die(args[0]->Int32Value());
  return v8::Undefined();
}

v8::Handle<v8::Value> _onexit(const v8::Arguments& args) {
  onexit->Set(v8::Integer::New(onexit->Length()), args[0]);
  return v8::Undefined();
}

int main(int argc, char ** argv, char ** envp) {
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  v8::HandleScope handle_scope;

  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
  v8::Handle<v8::Context> context = v8::Context::New(NULL, global);
  v8::Context::Scope context_scope(context);

  onexit = v8::Array::New();
  context->Global()->Set(v8::String::New("include"), v8::FunctionTemplate::New(_include)->GetFunction());
  context->Global()->Set(v8::String::New("exit"), v8::FunctionTemplate::New(_exit)->GetFunction());
  context->Global()->Set(v8::String::New("onexit"), v8::FunctionTemplate::New(_onexit)->GetFunction());
  context->Global()->Set(v8::String::New("global"), context->Global());

  setup_system(envp, context->Global());
  setup_io(context->Global());  
  
  #ifdef HAVE_MYSQL
    setup_mysql(context->Global());  
  #endif
  
  init();
  
  if (argc == 1) {
    printf("Nothing to do.\n");
  } else {
    int result = execute_file(argv[1]);
    if (result) { die(result); }
  }
  die(0);
}
