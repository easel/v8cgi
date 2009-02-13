/*
 * v8cgi app file. Loosely based on V8's "shell" sample app.
 */

#ifndef _JS_APP_H
#define _JS_APP_H

#include <v8.h>
#include "js_cache.h"

class v8cgi_App {
public:
	virtual ~v8cgi_App() {};
	int init(int argc, char ** argv);
	int execute(char ** envp);
	void addOnExit(v8::Handle<v8::Value>);
	int include(std::string str, bool populate);
	v8::Handle<v8::Value> require(std::string str, bool wrap);
	
	virtual size_t reader (char * destination, size_t size);
	virtual size_t writer (const char * source, size_t size);
	virtual void error(const char * data, const char * file, int line);
	virtual void header(const char * name, const char * value);

private:
	v8::Handle<v8::Array> onexit; /* what to do on exit */
	std::string cfgfile; /* config file */
	std::string mainfile; /* command-line specified file */
	std::string cwd;
	Cache cache;
	
	int prepare(char ** envp);
	int go(char ** envp);
	void finish();
	void http();
	void report_error(const char * message);
	void exception(v8::TryCatch* try_catch);
	int autoload();
	
	std::string findname(std::string name);
	void populate_global(v8::Handle<v8::Object> exports);
	void save_cwd();
	void restore_cwd();
	std::string wrap(std::string original);
	std::string dirname(std::string filename);
	v8::Handle<v8::Value> include_js(std::string filename, bool wrap);
	v8::Handle<v8::Value> include_dso(std::string filename);
	bool exists(std::string filename);
};

#endif
