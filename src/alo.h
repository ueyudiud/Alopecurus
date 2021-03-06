/*
 * alo.h
 *
 * basic API.
 *
 *  Created on: 2019年7月20日
 *      Author: ueyudiud
 */

#ifndef ALO_H_
#define ALO_H_

#define ALO aloE_vernum

#define ALO_VERSION_MAJOR	"0"
#define ALO_VERSION_MINOR	"2"
#define ALO_VERSION_NUM		0, 2
#define ALO_VERSION_RELEASE	"1"

#define ALO_NAME "Alopecurus"
#define ALO_VERSION ALO_NAME" "ALO_VERSION_MAJOR"."ALO_VERSION_MINOR
#define ALO_VERSION_FULL ALO_VERSION"."ALO_VERSION_RELEASE
#define ALO_COPYRIGHT ALO_VERSION_FULL" Copyright (C) 2018-2020 ueyudiud"
#define ALO_AUTHOR "ueyudiud"

#include "adef.h"
#include "acfg.h"

/**
 ** version number declaration
 */

#define aloE_vermajor aloE_apply(aloE_1, ALO_VERSION_NUM)
#define aloE_verminor aloE_apply(aloE_2, ALO_VERSION_NUM)
#define aloE_vernum (aloE_vermajor * 100 + aloE_verminor)

/**
 ** the ALO_API marked for API functions.
 */
#if defined(ALO_BUILD_TO_DLL)

#if defined(ALO_CORE) || defined(ALO_LIB)
#define ALO_API __declspec(dllexport)
#else
#define ALO_API __declspec(dllimport)
#endif

#else

#define ALO_API extern

#endif

#define ALO_GLOBAL_IDNEX (-ALO_MAXSTACKSIZE - 50000)
#define ALO_REGISTRY_INDEX (-ALO_MAXSTACKSIZE - 100000)
#define ALO_CAPTURE_INDEX(x) (-ALO_MAXSTACKSIZE - 150000 + (x))

#define ALO_NOERRFUN (-ALO_MAXSTACKSIZE - 50000)
#define ALO_LASTERRFUN (-ALO_MAXSTACKSIZE - 100000)

#if defined(ALO_USER_H_)
#include ALO_USER_H_
#endif

/**
 * state manipulation
 */

ALO_API astate alo_newstate(aalloc, void*);
ALO_API void alo_deletestate(astate);

ALO_API void alo_setpanic(astate, acfun);
ALO_API aalloc alo_getalloc(astate, void**);
ALO_API void alo_setalloc(astate, aalloc, void*);

ALO_API const aver_t* alo_version(astate);

/**
 ** basic stack manipulation
 */

ALO_API ssize_t alo_absindex(astate, ssize_t);
ALO_API int alo_ensure(astate, size_t);
ALO_API ssize_t alo_gettop(astate);
ALO_API void alo_settop(astate, ssize_t);
ALO_API void alo_copy(astate, ssize_t, ssize_t);
ALO_API void alo_push(astate, ssize_t);
ALO_API void alo_pop(astate, ssize_t);
ALO_API void alo_erase(astate, ssize_t);
ALO_API void alo_insert(astate, ssize_t);
ALO_API void alo_xmove(astate, astate, size_t);

#define alo_drop(T) alo_settop(T, -1)

/**
 ** access functions (stack -> C)
 */

ALO_API int alo_isinteger(astate, ssize_t);
ALO_API int alo_isnumber(astate, ssize_t);
ALO_API int alo_iscfunction(astate, ssize_t);
ALO_API int alo_israwdata(astate, ssize_t);

#define alo_isboolean(T,index) (alo_typeid(T, index) == ALO_TBOOL)
#define alo_isstring(T,index) (alo_typeid(T, index) == ALO_TSTRING)
#define alo_isreference(T,index) (alo_typeid(T, index) >= ALO_TTUPLE)
#define alo_isnothing(T,index) (alo_typeid(T, index) <= ALO_TNIL)
#define alo_isnone(T,index) (alo_typeid(T, index) == ALO_TUNDEF)
#define alo_isnonnil(T,index) (alo_typeid(T, index) > ALO_TNIL)

ALO_API int alo_typeid(astate, ssize_t);
ALO_API astr alo_typename(astate, ssize_t);
ALO_API astr alo_tpidname(astate, int);
ALO_API int alo_toboolean(astate, ssize_t);
ALO_API aint alo_tointegerx(astate, ssize_t, int*);
ALO_API afloat alo_tonumberx(astate, ssize_t, int*);
ALO_API astr alo_tolstring(astate, ssize_t, size_t*);
ALO_API acfun alo_tocfunction(astate, ssize_t);
ALO_API void* alo_torawdata(astate, ssize_t);
ALO_API astate alo_tothread(astate, ssize_t);

#define alo_tointeger(T,index) alo_tointegerx(T, index, NULL)
#define alo_tonumber(T,index) alo_tonumberx(T, index, NULL)
#define alo_tostring(T,index) alo_tolstring(T, index, NULL)
#define alo_toobject(T,index,type) aloE_cast(type, alo_torawdata(T, index))
#define alo_toiterator(T,index) ((aitr) { alo_tointeger(T, index) })

ALO_API void* alo_rawptr(astate, ssize_t);
ALO_API size_t alo_rawlen(astate, ssize_t);
ALO_API size_t alo_len(astate, ssize_t);
ALO_API int alo_equal(astate, ssize_t, ssize_t);
ALO_API void alo_arith(astate, int);
ALO_API int alo_compare(astate, ssize_t, ssize_t, int);

/**
 ** push functions (C -> stack)
 */

ALO_API void alo_pushnil(astate);
ALO_API void alo_pushboolean(astate, int);
ALO_API void alo_pushinteger(astate, aint);
ALO_API void alo_pushnumber(astate, afloat);
ALO_API astr alo_pushlstring(astate, const char*, size_t);
ALO_API astr alo_pushstring(astate, astr);
ALO_API astr alo_pushfstring(astate, astr, ...);
ALO_API astr alo_pushvfstring(astate, astr, va_list);
ALO_API void alo_pushcfunction(astate, acfun, size_t, int);
ALO_API void alo_pushpointer(astate, void*);
ALO_API int alo_pushthread(astate);

#define alo_pushunit(T) alo_newtuple(T, 0)
#define alo_pushiterator(T,i) alo_pushinteger(T, (i).offset)
#define alo_pushcstring(T,s) alo_pushlstring(T, ""s, sizeof(s) / sizeof(char) - 1)
#define alo_pushlightcfunction(T,f) alo_pushcfunction(T, f, 0, false)
#define alo_pushcclosure(T,f,n) alo_pushcfunction(T, f, n, false)

/**
 ** arithmetic operation, comparison and other functions
 */

ALO_API void alo_rawcat(astate, size_t);

/**
 ** get functions (Alopecurus -> stack)
 */

ALO_API int alo_inext(astate, ssize_t, aitr*);
ALO_API void alo_iremove(astate, ssize_t, aitr*);
ALO_API int alo_rawget(astate, ssize_t);
ALO_API int alo_rawgeti(astate, ssize_t, aint);
ALO_API int alo_rawgets(astate, ssize_t, astr);
ALO_API int alo_get(astate, ssize_t);
ALO_API int alo_gets(astate, ssize_t, astr);
ALO_API int alo_put(astate, ssize_t);
ALO_API int alo_getmetatable(astate, ssize_t);
ALO_API int alo_getmeta(astate, ssize_t, astr, int);
ALO_API int alo_getdelegate(astate, ssize_t);

#define alo_getreg(T,key) alo_gets(T, ALO_REGISTRY_INDEX, key)

ALO_API amem alo_newdata(astate, size_t);
ALO_API void alo_newtuple(astate, size_t);
ALO_API void alo_newlist(astate, size_t);
ALO_API void alo_newtable(astate, size_t);
ALO_API astate alo_newthread(astate);

#define alo_newobject(T,type) aloE_cast(typeof(type)*, alo_newdata(T, sizeof(type)))
#define alo_ibegin(T,index) (aloE_void(T), aloE_void(index), (aitr) { ALO_ITERATE_BEGIN })

/**
 ** set functions (stack -> Alopecurus)
 */

ALO_API void alo_sizehint(astate, ssize_t, size_t);
ALO_API void alo_trim(astate, ssize_t);
ALO_API void alo_triml(astate, ssize_t, size_t);
ALO_API void alo_rawsetx(astate, ssize_t, int);
ALO_API void alo_rawseti(astate, ssize_t, aint);
ALO_API void alo_rawsets(astate, ssize_t, astr);
ALO_API int alo_rawrem(astate, ssize_t);
ALO_API void alo_rawclr(astate, ssize_t);
ALO_API void alo_add(astate, ssize_t);
ALO_API void alo_setx(astate, ssize_t, int);
ALO_API int alo_remove(astate, ssize_t);
ALO_API int alo_setmetatable(astate, ssize_t);
ALO_API int alo_setdelegate(astate, ssize_t);

#define alo_rawset(T,index) alo_rawsetx(T, index, false)
#define alo_set(T,index) alo_setx(T, index, false)

/**
 ** call and IO functions
 */

ALO_API void alo_callk(astate, int, int, akfun, akctx);
ALO_API int alo_pcallk(astate, int, int, ssize_t, akfun, akctx);
ALO_API int alo_compile(astate, astr, astr, areader, void*);
ALO_API int alo_load(astate, astr, areader, void*);
ALO_API int alo_save(astate, awriter, void*, int);

#define alo_call(T,a,r) alo_callk(T, a, r, NULL, 0)
#define alo_pcall(T,a,r,e) alo_pcallk(T, a, r, e, NULL, 0)

/**
 ** coroutine functions.
 */

ALO_API int alo_resume(astate, astate, int);
ALO_API void alo_yieldk(astate, int, akfun, akctx);
ALO_API int alo_status(astate);
ALO_API int alo_isyieldable(astate);

#define alo_yield(T,nres) alo_yieldk(T, nres, NULL, 0)

/**
 ** error handling
 */

ALO_API anoret alo_error(astate);
ALO_API anoret alo_throw(astate);

/**
 ** miscellaneous functions.
 */

enum {
	ALO_GCRUNNING,
	ALO_GCSTOP,
	ALO_GCRERUN,
	ALO_GCUSED,
	ALO_GCSTEPMUL,
	ALO_GCPAUSEMUL,
};

ALO_API size_t alo_gcconf(astate, int, size_t);
ALO_API void alo_fullgc(astate);
ALO_API void alo_checkgc(astate);
ALO_API int alo_format(astate, awriter, void*, astr, ...);
ALO_API int alo_vformat(astate, awriter, void*, astr, va_list);

ALO_API void alo_bufpush(astate, ambuf_t*);
ALO_API void alo_bufgrow(astate, ambuf_t*, size_t);
ALO_API void alo_bufpop(astate, ambuf_t*);

#define alo_isgcrunning(T) aloE_cast(int, alo_gcconf(T, ALO_GCRUNNING, 0))
#define alo_memused(T) alo_gcconf(T, ALO_GCUSED, 0)

/**
 ** debugger
 */

typedef struct alo_DebugInformation adbinfo_t;

/* hook function type, the function will be called in specific events */
typedef void (*ahfun)(astate, adbinfo_t*);

enum {
	ALO_INFCURR,
	ALO_INFPREV,
	ALO_INFSTACK
};

ALO_API int alo_getinfo(astate, int, astr, adbinfo_t*);

#define ALO_HMASKCALL  (1 << 0)
#define ALO_HMASKRET   (1 << 1)
#define ALO_HMASKLINE  (1 << 2)

ALO_API void alo_sethook(astate, ahfun, int);
ALO_API ahfun alo_gethook(astate);
ALO_API int alo_gethookmask(astate);

struct alo_DebugInformation {
	int event;
	astr name; /* apply by 'n' */
	astr kind; /* kind of frame, apply by 'n' */
	astr src; /* apply by 's' */
	int linefdef, lineldef; /* apply by 's' */
	int line; /* apply by 'l' */
	unsigned nargument; /* apply by 'a' */
	unsigned ncapture; /* apply by 'a' */
	abyte vararg : 1; /* apply by 'a' */
	abyte istailc : 1; /* apply by 'c' */
	abyte isfinc : 1; /* apply by 'c' */
	/* for private use */
	void* _frame;
};

#endif /* ALO_H_ */
