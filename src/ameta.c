/*
 * ameta.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#include "astr.h"
#include "alis.h"
#include "atab.h"
#include "ameta.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"
#include "ado.h"

#define TM(id,name) "__"#name

const astr aloT_typenames[ALO_NUMTYPE] = {
	"nil", "bool", "int", "float", "pointer", "string", "tuple",
	"list", "table", "function", "thread", "rawdata"
};

const astr aloT_tmnames_[TM_N] = {
	TM_LIST
};

#undef TM

void aloT_init(astate T) {
	Gd(T);
	for (size_t i = 0; i < TM_N; ++i) {
		astring_t* name = aloS_of(T, aloT_tmnames_[i]);
		aloG_fix(T, name); /* fix TM name */
		/* settle extra data */
		name->ftagname = true;
		name->extra = i;
		G->stagnames[i] = name; /* set tagged name to global */
	}
}

const atval_t* aloT_fastgetaux(atable_t* table, astring_t* name, atmi id) {
	const atval_t* out = aloH_getis(table, name);
	if (ttisnil(out)) {
		table->reserved |= 1 << id; /* marked TM is not presents in table. */
		return NULL;
	}
	return out;
}

static void callbin(astate T, const atval_t* f, const atval_t* t1, const atval_t* t2) {
	ptrdiff_t oldtop = getstkoff(T, T->top);
	/* assume extra stack to place values */
	aloT_vmput3(T, f, t1, t2);
	aloD_call(T, T->top - 3, 1); /* call function */
	T->top = putstkoff(T, oldtop); /* restore to old top of stack, the result will be here */
}

static void callunr(astate T, const atval_t* f, const atval_t* t1) {
	ptrdiff_t oldtop = getstkoff(T, T->top);
	/* assume extra stack to place values */
	aloT_vmput2(T, f, t1);
	aloD_call(T, T->top - 2, 1); /* call function */
	T->top = putstkoff(T, oldtop); /* restore to old top of stack, the result will be here */
}

atable_t* aloT_getmt(const atval_t* o) {
	switch (ttpnv(o)) {
	case ALO_TLIST   : return tgetlis(o)->metatable;
	case ALO_TTABLE  : return tgettab(o)->metatable;
	case ALO_TRAWDATA: return tgetrdt(o)->metatable;
	default          : return NULL;
	}
}

static const atval_t* aloT_fullget(astate T, const atval_t* self, atable_t* table, atmi id) {
	aloE_assert(table, "meta table should not be NULL.");
	const atval_t* result;
	Gd(T);
	astring_t* const name = G->stagnames[id];
	find:  /* find tagged method directly */
	if (!ttisnil(result = aloH_getis(table, name))) {
		return result;
	}
	/* failed get tagged method directly, call 'lookup' method */
	const atval_t* lookup = aloT_gfastget(G, table, TM_LKUP);
	if (lookup) {
		switch (ttpnv(lookup)) {
		case ALO_TTABLE: /* find method in parent meta table */
			table = tgettab(lookup);
			goto find;
		case ALO_TLIST: { /* find method in MRO */
			alist_t* mro = tgetlis(lookup);
			if (mro->length > 0) {
				atval_t* i = mro->array + mro->length;
				while (--i >= mro->array) {
					switch (ttpnv(i)) {
					case ALO_TTABLE: /* static target */
						if (!ttisnil(result = aloH_getis(tgettab(i), name))) {
							return result;
						}
						break; /* no tagged method found */
					case ALO_TFUNCTION: { /* dynamic target */
						atval_t t = tnewstr(name);
						callbin(T, i, self, &t);
						if (!ttisnil(T->top)) { /* find target? */
							return T->top;
						}
						break; /* no tagged method found */
					}
					default:
						break; /* illegal MRO value */
					}
				}
			}
			break;
		}
		case ALO_TFUNCTION: {
			atval_t t = tnewstr(G->stagnames[id]);
			callbin(T, lookup, self, &t);
			if (!ttisnil(T->top)) { /* find target? */
				return T->top;
			}
			break;
		}
		default: /* no available lookup value */
			break;
		}
	}
	return NULL;
}

const atval_t* aloT_gettm(astate T, const atval_t* self, atmi id, int dolookup) {
	atable_t* table = aloT_getmt(self);
	if (table == NULL)
		return NULL;
	if (dolookup)
		return aloT_fullget(T, self, table, id);
	Gd(T);
	const atval_t* result = aloH_getis(table, G->stagnames[id]);
	return ttisnil(result) ? NULL : result;
}

const atval_t* aloT_fastgetx(astate T, const atval_t* self, atmi id) {
	aloE_assert(id < ALO_NUMTM && id != TM_LKUP, "invalid fast call tagged method id.");
	atable_t* table = aloT_getmt(self);
	if (table == NULL) {
		return NULL;
	}
	const atval_t* result;
	Gd(T);
	find: /* find tagged method directly */
	if ((result = aloT_gfastget(G, table, id))) {
		return result;
	}
	/* failed get tagged method directly, call 'lookup' method */
	const atval_t* lookup = aloT_gfastget(G, table, TM_LKUP);
	if (lookup) {
		switch (ttpnv(lookup)) {
		case ALO_TTABLE: /* find method in parent meta table */
			table = tgettab(lookup);
			goto find;
		case ALO_TLIST: { /* find method in MRO */
			alist_t* mro = tgetlis(lookup);
			{
				atval_t* i = mro->array + mro->length;
				while (--i >= mro->array) {
					switch (ttpnv(i)) {
					case ALO_TTABLE: /* static target */
						if ((result = aloT_gfastget(G, tgettab(i), id))) {
							return result;
						}
						break; /* no tagged method found */
					case ALO_TFUNCTION: { /* dynamic target */
						atval_t t = tnewstr(G->stagnames[id]);
						callbin(T, i, self, &t);
						if (!ttisnil(T->top)) { /* find target? */
							return T->top;
						}
						break; /* no tagged method found */
					}
					default:
						break; /* illegal MRO value */
					}
				}
			}
			break;
		}
		case ALO_TFUNCTION: {
			atval_t t = tnewstr(G->stagnames[id]);
			callbin(T, lookup, self, &t);
			if (!ttisnil(T->top)) { /* find target? */
				return T->top;
			}
			break;
		}
		default: /* no available lookup value */
			break;
		}
	}
	return NULL;
}

static const atval_t* lookup_mro(astate T, const atval_t* self, const atval_t* key, alist_t* mro) {
	if (mro->length == 0)
		return NULL;
	const atval_t* result;
	atval_t* i = mro->array + mro->length;
	while (--i >= mro->array) {
		switch (ttpnv(i)) {
		case ALO_TTABLE: /* static target */
			if (!ttisnil(result = aloH_get(T, tgettab(i), key))) {
				return result;
			}
			break; /* no tagged method found */
		case ALO_TFUNCTION: { /* dynamic target */
			callbin(T, i, self, key);
			if (!ttisnil(T->top)) { /* find target? */
				return T->top;
			}
			break; /* no tagged method found */
		}
		default:
			break; /* illegal MRO value */
		}
	}
	return NULL;
}

/**
 ** look up method in meta table.
 */
const atval_t* aloT_lookup(astate T, const atval_t* self, const atval_t* key) {
	const atval_t* result;
	atable_t* table = aloT_getmt(self);
	if (table) { /* check in meta table */
		find: /* find tagged method directly */
		if (!ttisnil(result = aloH_get(T, table, key))) {
			return result;
		}
		/* failed get tagged method directly, call 'lookup' method */
		const atval_t* lookup = aloT_gfastget(T->g, table, TM_LKUP);
		if (lookup) {
			switch (ttpnv(lookup)) {
			case ALO_TTABLE: /* find method in parent meta table */
				table = tgettab(lookup);
				goto find;
			case ALO_TLIST: { /* find method in MRO */
				if ((result = lookup_mro(T, self, key, tgetlis(lookup)))) {
					return result;
				}
				break;
			}
			case ALO_TFUNCTION: {
				callbin(T, lookup, self, key);
				if (!ttisnil(T->top)) { /* find target? */
					return T->top;
				}
				break;
			}
			default: /* no available lookup value */
				break;
			}
		}
	}
	const atval_t* delegate = aloT_getreg(T);
	if (ttistab(delegate) && (result = aloH_getis(tgettab(delegate), T->g->stagnames[TM_DLGT]))) { /* check by delegate function */
		aloT_vmput3(T, result, self, key);
		aloD_callnoyield(T, T->top - 3, 1);
		if (!ttisnil(--T->top)) { /* found object? */
			return T->top;
		}
	}
	return NULL;
}

/**
 ** call unary method.
 */
void aloT_callunr(astate T, const atval_t* f, const atval_t* t1, atval_t* o) {
	callunr(T, f, t1);
	tsetobj(T, o, T->top); /* move object to output slot */
}

/**
 ** call binary method between two values.
 */
void aloT_callbin(astate T, const atval_t* f, const atval_t* t1, const atval_t* t2, atval_t* o) {
	callbin(T, f, t1, t2);
	tsetobj(T, T->top, o); /* move object to output slot */
}

/**
 ** call compare method between two values.
 */
int aloT_callcmp(astate T, const atval_t* f, const atval_t* t1, const atval_t* t2) {
	callbin(T, f, t1, t2);
	return aloV_getbool(T->top); /* cast object to boolean value */
}

/**
 ** get registry from current environment.
 */
const atval_t* aloT_getreg(astate T) {
	aframe_t* frame = T->frame;
	while (frame->prev) {
		switch (ttype(frame->fun)) {
		case ALO_TACL:
		case ALO_TCCL:
			return &tgetclo(frame->fun)->delegate; /* return delegate registry */
		}
		frame = frame->prev;
	}
	return &T->g->registry; /* no local registry found, return global registry. */
}


/**
 ** try take meta unary operation, return true if success and false for otherwise.
 */
int aloT_tryunr(astate T, const atval_t* t1, atmi id) {
	const atval_t* tm = aloT_gettm(T, t1, id, true);
	if (tm) {
		callunr(T, tm, t1);
		return true;
	}
	else {
		return false;
	}
}

/**
 ** try take meta binary operation, return true if success and false for otherwise.
 */
int aloT_trybin(astate T, const atval_t* t1, const atval_t* t2, atmi id) {
	const atval_t* tm = aloT_gettm(T, t1, id, true);
	if (tm) {
		callbin(T, tm, t1, t2);
		return true;
	}
	else {
		return false;
	}
}

