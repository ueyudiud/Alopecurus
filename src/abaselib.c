/*
 * abaselib.c
 *
 *  Created on: 2019年7月29日
 *      Author: ueyudiud
 */

#define ABASELIB_C_
#define ALO_LIB

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#include <stdio.h>

static void base_print_impl(astate T) {
	ssize_t n = alo_gettop(T);
	const char* src;
	size_t len;
	alo_getreg(T, "tostring");
	for (ssize_t i = 0; i < n; ++i) {
		alo_push(T, -1);
		alo_push(T, i);
		alo_call(T, 1, 1);
		src = alo_tolstring(T, -1, &len);
		if (i > 0) {
			aloL_fputc(stdout, '\t');
		}
		aloL_fputs(stdout, src, len);
		alo_drop(T);
	}
}

/**
 ** print string into the screen.
 ** prototype: print({args})
 */
static int base_print(astate T) {
	base_print_impl(T);
	aloL_fflush(stdout);
	return 0;
}

/**
 ** print string into the screen, and move to next line.
 ** prototype: println({args})
 */
static int base_println(astate T) {
	base_print_impl(T);
	aloL_fputln(stdout);
	return 0;
}

/**
 ** transform object to string.
 ** prototype: tostring(arg)
 */
static int base_tostring(astate T) {
	aloL_checkany(T, 0);
	aloL_tostring(T, 0, NULL);
	return 1;
}

/**
 ** transform object to integer.
 ** if the object cannot cast to integer, an error will be thrown.
 ** prototype: toint(arg)
 */
static int base_toint(astate T) {
	aloL_checkany(T, 0);
	int flag;
	aint value = alo_tointegerx(T, 0, &flag);
	if (!flag) {
		aloL_error(T, "can not cast object to integer.");
	}
	alo_pushinteger(T, value);
	return 1;
}

/**
 ** transform object to number.
 ** if the object cannot cast to number, an error will be thrown.
 ** prototype: tonum(arg)
 */
static int base_tonum(astate T) {
	aloL_checkany(T, 0);
	int flag;
	afloat value = alo_tonumberx(T, 0, &flag);
	if (!flag) {
		aloL_error(T, "can not cast object to number.");
	}
	alo_pushnumber(T, value);
	return 1;
}

/**
 ** get element from structure.
 ** prototype: rawget(arg, idx)
 */
static int base_rawget(astate T) {
	aloL_checkany(T, 1);
	int succ = alo_rawget(T, 0) != ALO_TUNDEF;
	return succ ? 1 : 0;
}

/**
 ** set element to structure.
 ** prototype: rawset(arg, idx, key)
 */
static int base_rawset(astate T) {
	aloL_checkany(T, 2);
	alo_rawsetx(T, 0, true);
	return 1;
}

/**
 ** remove element from structure.
 ** if nothing is removed, return no elements.
 ** prototype: rawrem(arg, idx)
 */
static int base_rawrem(astate T) {
	aloL_checkany(T, 1);
	int succ = alo_rawrem(T, 0) != ALO_TUNDEF;
	return succ ? 1 : 0;
}

/**
 ** get length of structure.
 ** prototype: rawlen(arg)
 */
static int base_rawlen(astate T) {
	aloL_checkany(T, 1);
	alo_pushinteger(T, alo_rawlen(T, 0));
	return 1;
}

/**
 ** test is parameter can cast to true.
 ** throw an error if there is no parameter or parameter can cast to false.
 ** prototype: assert([arg])
 */
static int base_assert(astate T) {
	if (alo_gettop(T) == 0 || !alo_toboolean(T, 0)) { /* when assertion failed */
		aloL_error(T, "assertion failed.");
	}
	return 0;
}

/**
 ** throw an error, the message can be nil, error code (an integer value) or
 ** error message (a string value).
 ** prototype: throw([msg])
 */
static int base_throw(astate T) {
	switch (alo_typeid(T, 0)) {
	case ALO_TNIL:
		alo_pushstring(T, "no error message.");
		break;
	case ALO_TINT:
		alo_pushfstring(T, "error code: %i", alo_tointeger(T, 0));
		break;
	case ALO_TSTRING:
		alo_settop(T, 1);
		break;
	default:
		alo_pushstring(T, "illegal error message.");
		break;
	}
	alo_error(T);
	/* unreachable code */
	return 0;
}

static int finish_try(astate T, int status, akctx context) {
	if (status == ThreadStateRun || status == ThreadStateYield) {
		return alo_gettop(T) - context;
	}
	else { /*catch an error */
		alo_pushboolean(T, false);
		alo_push(T, -2);
		return 2;
	}
}

static int base_try(astate T) {
	aloL_checkcall(T, 0);
	alo_pushboolean(T, true); /* true is first result if no error */
	alo_insert(T, 0);
	int status = alo_pcallk(T, alo_gettop(T) - 2, ALO_MULTIRET, ALO_NOERRFUN, finish_try, 0);
	return finish_try(T, status, 0);
}

static int base_xtry(astate T) {
	aloL_checkcall(T, 1);
	int lastfun = !alo_isnonnil(T, 0);
	if (!lastfun) {
		aloL_checkcall(T, 0);
	}
	alo_pushboolean(T, true); /* true is first result if no error */
	alo_insert(T, 1);
	int status = alo_pcallk(T, alo_gettop(T) - 3, ALO_MULTIRET, lastfun ? ALO_LASTERRFUN : 0, finish_try, 1);
	return finish_try(T, status, 1);
}

/**
 ** iterate to next element for tuple or list.
 ** prototype: [list, idx] inext()
 */
static int base_inext(astate T) {
	aitr itr = alo_toiterator(T, ALO_CAPTURE_INDEX(1)); /* get offset from capture */
	if (alo_inext(T, ALO_CAPTURE_INDEX(0), &itr) != ALO_TUNDEF) {
		/* erase unused value */
		alo_erase(T, -2);
		/* update offset to capture */
		alo_pushiterator(T, itr);
		alo_pop(T, ALO_CAPTURE_INDEX(1));
		return 1;
	}
	return 0;
}

/**
 ** iterate to next element for table.
 ** prototype: [list, idx] mnext()
 */
static int base_mnext(astate T) {
	aitr itr = alo_toiterator(T, ALO_CAPTURE_INDEX(1)); /* get offset from capture */
	if (alo_inext(T, ALO_CAPTURE_INDEX(0), &itr) != ALO_TUNDEF) {
		/* update offset to capture */
		alo_pushiterator(T, itr);
		alo_pop(T, ALO_CAPTURE_INDEX(1));
		return 2;
	}
	return 0;
}

/**
 ** create new iterator for object.
 ** prototype: newiterator(iterable)
 */
static int base_newiterator(astate T) {
	if (alo_getmeta(T, 1, "__itr", true) != ALO_TUNDEF) { /* call meta method */
		alo_push(T, 0); /* push self */
		alo_call(T, 1, 1);
	}
	else {
		alo_pushiterator(T, alo_ibegin(T, 0)); /* beginning of offset */
		switch (alo_typeid(T, 0)) {
		case ALO_TTUPLE:
		case ALO_TLIST:
			alo_pushcclosure(T, base_inext, 2);
			break;
		case ALO_TTABLE:
			alo_pushcclosure(T, base_mnext, 2);
			break;
		default:
			aloL_argerror(T, 0, "argument should be iterable object.");
			break;
		}
	}
	return 1;
}

/**
 ** return the type name of argument
 ** prototype: typeof(arg)
 */
static int base_typeof(astate T) {
	aloL_checkany(T, 0);
	alo_pushstring(T, alo_typename(T, 0));
	return 1;
}

/**
 ** get default functions collection (used look up table for primitive values).
 ** prototype: __dlgt(arg, key)
 */
static int base_delegate(astate T) {
	aloL_checkany(T, 0);
	if (alo_getreg(T, "__basic_delegates") != ALO_TLIST) {
		aloL_tagerror(T, -1, ALO_TLIST);
	}
	alo_rawgeti(T, -1, alo_typeid(T, 0));
	alo_push(T, 1);
	alo_get(T, -2);
	return 1;
}

/**
 ** set meta table for object.
 ** prototype: setmeta(obj, metatable)
 */
static int base_setmeta(astate T) {
	aloL_checkany(T, 0);
	if (alo_isnonnil(T, 1)) {
		aloL_checktype(T, 1, ALO_TTABLE);
		alo_setmetatable(T, 0);
	}
	else {
		alo_pushnil(T);
		alo_setmetatable(T, 0);
	}
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "assert", base_assert },
	{ "print", base_print },
	{ "println", base_println },
	{ "newiterator", base_newiterator },
	{ "rawget", base_rawget },
	{ "rawlen", base_rawlen },
	{ "rawrem", base_rawrem },
	{ "rawset", base_rawset },
	{ "setmeta", base_setmeta },
	{ "toint", base_toint },
	{ "tonum", base_tonum },
	{ "tostring", base_tostring },
	{ "throw", base_throw },
	{ "try", base_try },
	{ "typeof", base_typeof },
	{ "xtry", base_xtry },
	{ "__basic_delegates", NULL },
	{ "__dlgt", base_delegate },
	{ "_G", NULL },
	{ "_VERSION", NULL },
	{ NULL, NULL }
};

#define NUM_DELEGATES (ALO_TRAWDATA + 1)

int aloopen_base(astate T) {
	alo_push(T, ALO_GLOBAL_IDNEX);
	aloL_setfuns(T, -1, mod_funcs);
	alo_pushcstring(T, ALO_VERSION);
	alo_rawsets(T, -2, "_VERSION");
	alo_newlist(T, NUM_DELEGATES);
	for (int i = 0; i < NUM_DELEGATES; ++i) {
		alo_newtable(T, 0);
		alo_rawseti(T, -2, i);
	}
	alo_rawsets(T, 1, "__basic_delegates");
	return 1;
}
