/**
 * Shorthands for various lengthy V8 syntaxt constructs.
 */

#ifndef _JS_MACROS_H
#define _JS_MACROS_H

#define _STRING(x) #x
#define STRING(x) _STRING(x)

#define SAVE_PTR(index, ptr) args.This()->SetInternalField(index, v8::External::New((void *)ptr)); 
#define LOAD_PTR(index, type) reinterpret_cast<type>(v8::Handle<v8::External>::Cast(args.This()->GetInternalField(index))->Value());
#define SAVE_VALUE(index, val) args.This()->SetInternalField(index, val)
#define LOAD_VALUE(index) args.This()->GetInternalField(index)
#define JS_STR(...) v8::String::New(__VA_ARGS__)
#define JS_INT(val) v8::Integer::New(val)
#define JS_FLOAT(val) v8::Number::New(val)
#define JS_BOOL(val) v8::Boolean::New(val)
#define JS_METHOD(name) v8::Handle<v8::Value> name(const v8::Arguments& args)
#define JS_EXCEPTION(reason) v8::ThrowException(JS_STR(reason))
#define JS_RETHROW(tc) v8::Local<v8::Value>::New(tc.Exception());

#define JS_GLOBAL v8::Context::GetCurrent()->Global()
#define GLOBAL_PROTO v8::Handle<v8::Object>::Cast(JS_GLOBAL->GetPrototype())
#define APP_PTR reinterpret_cast<v8cgi_App *>(v8::Handle<v8::External>::Cast(GLOBAL_PROTO->GetInternalField(0))->Value());
#define GC_PTR reinterpret_cast<GC *>(v8::Handle<v8::External>::Cast(GLOBAL_PROTO->GetInternalField(1))->Value());

#define ASSERT_CONSTRUCTOR if (!args.IsConstructCall()) { return JS_EXCEPTION("Invalid call format. Please use the 'new' operator."); }

#ifdef _WIN32
#   define SHARED_INIT() extern "C" __declspec(dllexport) void init(v8::Handle<v8::Object> exports)
#else
#   define SHARED_INIT() extern "C" void init(v8::Handle<v8::Object> exports)
#endif

#endif
