/**
 * Garbage collection support. Every C++ class can subscribe to be notified 
 * when its JS representation gets GC'ed.
 */

#include "gc.h"

/**
 * GC handler: executed when given object dies
 * @param {v8::Value} object
 * @param {void *} ptr Pointer to GC instance
 */
void GC::handler(v8::Persistent<v8::Value> object, void * ptr) {
	GC * gc = (GC *) ptr;
	GC::objlist::iterator it = gc->data.begin();
	GC::objlist::iterator end = gc->data.end();
	while (it != end && it->first != object) { it++; }
	
	/* only if we have this one */
	if (it != end) { 
		gc->go(it);
	}
}

/**
 * Add a method to be executed when object dies
 * @param {v8::Value} object Object to monitor
 * @param {char *} method Method name
 */
void GC::add(v8::Handle<v8::Value> object, GC::dtor_t dtor) {
	v8::Persistent<v8::Value> p = v8::Persistent<v8::Value>::New(object);
	p.MakeWeak((void *) this, &handler);
	this->data.push_back(std::pair<v8::Persistent<v8::Value>, GC::dtor_t>(p, dtor));
}

/**
 * Execute ongarbagecollection callback
 */
void GC::go(objlist::iterator it) {
	v8::HandleScope handle_scope;
	v8::Handle<v8::Object> obj = it->first->ToObject();
	dtor_t dtor = it->second;
	dtor(obj);
	this->data.erase(it);
}

/**
 * Finish = execute all callbacks
 */
void GC::finish() {
	while (!this->data.empty()) {
		this->go(this->data.begin());
	}
	this->data.clear();
}
