#include <string>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef windows
#   include <dlfcn.h>
#else
#   include <windows.h>
#   define dlopen(x,y) (void*)LoadLibrary(x)
#   define dlsym(x,y) (void*)GetProcAddress((HMODULE)x,y)
#   define dlclose(x) FreeLibrary((HMODULE)x)
#endif

#include "js_macros.h"
#include "js_cache.h"
#include "js_common.h"

/**
 * Is this file already cached?
 */
bool Cache::isCached(std::string filename) {
	struct stat st;
	int result = stat(filename.c_str(), &st);
	if (result != 0) { return false; 

	TimeValue::iterator it = modified.find(filename);
	if (it == modified.end()) { return false; } /* not seen yet */
	
	if (it->second != st.st_mtime) { /* was modified */
		erase(filename);
		return false;
	}
	return true;
}

/**
 * Mark filename as "cached"
 * */
void Cache::mark(std::string filename) {
	struct stat st;
	stat(filename.c_str(), &st);
	modified[filename] = st.st_mtime;
}

/**
 * Remove file from all available caches
 */
void Cache::erase(std::string filename) {
	SourceValue::iterator it1 = sources.find(filename);
	if (it1 != sources.end()) { sources.erase(it1); }

	HandleValue::iterator it2 = handles.find(filename);
	if (it2 != handles.end()) { 
		dlclose(it2->second);
		handles.erase(it2); 
	}
	
	ScriptValue::iterator it3 = scripts.find(filename);
	if (it3 != scripts.end()) { 
		it3->second.Dispose();
		scripts.erase(it3); 
	}
	
}

/**
 * Return source code for a given file
 */
std::string Cache::getSource(std::string filename, bool wrap) {
#ifdef VERBOSE
	printf("[getSource] cache try for '%s' .. ", filename.c_str()); 
#endif	
	if (isCached(filename)) {
#ifdef VERBOSE
		printf("cache hit\n"); 
#endif	
		SourceValue::iterator it = sources.find(filename);
		return it->second;
	} else {
#ifdef VERBOSE
		printf("cache miss\n"); 
#endif	
		FILE * file = fopen(filename.c_str(), "rb");
		if (file == NULL) { 
			std::string s = "Error reading '";
			s += filename;
			s += "'";
			throw s; 
		}
		
		mark(filename); /* mark as cached */
		fseek(file, 0, SEEK_END);
		size_t size = ftell(file);
		rewind(file);
		char* chars = new char[size + 1];
		chars[size] = '\0';
		for (unsigned int i = 0; i < size;) {
			size_t read = fread(&chars[i], 1, size - i, file);
			i += read;
		}
		fclose(file);
		std::string source = chars;
		delete[] chars;

		/* remove shebang line */
		if (source.find('#',0) == 0 && source.find('!',1) == 1 ) {
			unsigned int pfix = source.find('\n',0);
			source.erase(0,pfix);
		};
		
		if (wrap) { source = wrapExports(source); }
		sources[filename] = source;
		return source;
	}
}

/**
 * Return dlopen handle for a given file
 */
void * Cache::getHandle(std::string filename) {
#ifdef VERBOSE
	printf("[getHandle] cache try for '%s' .. ", filename.c_str()); 
#endif	
	if (isCached(filename)) {
#ifdef VERBOSE
		printf("cache hit\n"); 
#endif	
		HandleValue::iterator it = handles.find(filename);
		return it->second;
	} else {
#ifdef VERBOSE
		printf("cache miss\n"); 
#endif	
		void * handle = dlopen(filename.c_str(), RTLD_LAZY);
		if (!handle) { 
			std::string error = "Error opening shared library '";
			error += filename;
			error += "'";
			throw error;
		}
		mark(filename); /* mark as cached */
		handles[filename] = handle;
		return handle;
	}
}

/**
 * Return compiled script from a given file
 */
v8::Handle<v8::Script> Cache::getScript(std::string filename, bool wrap) {
#ifdef VERBOSE
	printf("[getScript] cache try for '%s' .. ", filename.c_str()); 
#endif	
	if (isCached(filename)) {
#ifdef VERBOSE
		printf("cache hit\n"); 
#endif	
		ScriptValue::iterator it = scripts.find(filename);
		return it->second;
	} else {
#ifdef VERBOSE
		printf("cache miss\n"); 
#endif	
		std::string source = getSource(filename, wrap);
		v8::Handle<v8::Script> script = v8::Script::Compile(JS_STR(source.c_str()), JS_STR(filename.c_str()));		
		v8::Persistent<v8::Script> result = v8::Persistent<v8::Script>::New(script);
		scripts[filename] = result;
		return result;
	}
}

/**
 * Return exports object for a given file
 */
v8::Handle<v8::Object> Cache::getExports(std::string filename) {
	ExportsValue::iterator it = exports.find(filename);
	if (it != exports.end()) { 
#ifdef VERBOSE
		printf("[getExports] using cached exports for '%s'\n", filename.c_str()); 
#endif	
		return it->second;
	} else {
#ifdef VERBOSE
		printf("[getExports] '%s' has no cached exports\n", filename.c_str()); 
#endif	
		return v8::Handle<v8::Object>::Handle();
	}
}

/**
 * Add a single item to exports cache
 */
void Cache::addExports(std::string filename, v8::Handle<v8::Object> obj) {
#ifdef VERBOSE
		printf("[addExports] caching exports for '%s'\n", filename.c_str()); 
#endif	
	exports[filename] = v8::Persistent<v8::Object>::New(obj);
}

/**
 * Remove a cached exports object
 */
void Cache::removeExports(std::string filename) {
#ifdef VERBOSE
		printf("[removeExports] removing exports for '%s'\n", filename.c_str()); 
#endif	
	ExportsValue::iterator it = exports.find(filename);
	if (it != exports.end()) { 
		it->second.Dispose();
		it->second.Clear();
		exports.erase(it);
	}
}

/**
 * Remove all cached exports
 */
void Cache::clearExports() {
	ExportsValue::iterator it;
	for (it=exports.begin(); it != exports.end(); it++) {
		it->second.Dispose();
		it->second.Clear();
	}
	exports.clear();
}
