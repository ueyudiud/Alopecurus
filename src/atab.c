/*
 * atab.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#include "astr.h"
#include "atab.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"
#include "ado.h"

#include <math.h>

#define NO_NODE 0

/* empty entry */
#define aloH_empty (aentry_t) { .value = aloO_nil, { .key = aloO_nil }, .prev = NO_NODE, .next = NO_NODE, .hash = 0 }

#define delety(e) (*(e) = aloH_empty)

#define hasnext(e) ((e)->next != NO_NODE)
#define hasprev(e) ((e)->prev != NO_NODE)

#define getnext(e) aloE_check(hasnext(e), "reach to end of nodes", (e) + (e)->next)
#define getprev(e) aloE_check(hasprev(e), "reach to begin of nodes", (e) - (e)->prev)

static void movety(aentry_t* dest, aentry_t* src) {
	aloE_assert(!ttisnil(src), "entry should not be empty.");
	int diff = src - dest;
	dest->key = src->key;
	dest->value = src->value;
	dest->prev = hasprev(src) ? src->prev - diff : NO_NODE;
	dest->next = hasnext(src) ? src->next + diff : NO_NODE;
	dest->hash = src->hash;
}

#define isinvalidfltkey(v) (isnan(v))

#define isinvalidkey(o) (ttisnil(o) || (ttisflt(o) && isinvalidfltkey(tgetflt(o))))

/**
 ** create new table value.
 */
atable_t* aloH_new(astate T) {
	atable_t* value = aloM_newo(T, atable_t);
	aloG_register(T, value, ALO_TTABLE);
	value->reserved = aloE_byte(~0);
	value->array = NULL;
	value->lastfree = NULL;
	value->capacity = 0;
	value->length = 0;
	value->metatable = NULL;
	return value;
}

static void resizetable(astate, atable_t*, size_t);

/**
 ** aloH_ensure table can store extra size of entries.
 */
int aloH_ensure(astate T, atable_t* self, size_t size) {
	size_t require = self->length + size;
	if (require > self->capacity) {
		resizetable(T, self, aloM_adjsize(T, self->capacity, require, ALO_MAX_BUFSIZE));
		return true;
	}
	return false;
}

/**
 ** get head entry by hash.
 */
#define headety(table,hash) ((table)->array + (hash) % (table)->capacity)

/**
 ** get entry that no key occupy it.
 */
static aentry_t* newety(astate T, atable_t* self) {
	while (self->lastfree >= self->array) {
		if (ttisnil(self->lastfree)) {
			return self->lastfree--;
		}
		self->lastfree--;
	}
	return NULL;
}

/**
 ** add entry to table and link to previous node.
 */
static aentry_t* addety(astate T, atable_t* self, aentry_t* prev) {
	aentry_t* entry = newety(T, self);
	int diff = entry - prev;
	if (hasnext(prev)) {
		entry->next = prev->next - diff;
	}
	entry->prev = prev->next = diff;
	return entry;
}

static atval_t* putety(astate T, atable_t* self, const atval_t* key, ahash_t hash) {
	aentry_t* entry = headety(self, hash); /* first hashing */
	if (!ttisnil(entry)) { /* blocked by other element */
		if (!hasprev(entry)) { /* check element is head of hash list */
			entry = addety(T, self, entry);
		}
		else {
			movety(newety(T, self), entry);
			entry->prev = entry->next = NO_NODE;
			delety(entry);
		}
	}
	entry->hash = hash;
	tsetobj(T, amkey(entry), key);
	aloE_assert(ttisnil(amval(entry)), "uninitialized entry value should be nil.");
	return amval(entry);
}

static void resizetable(astate T, atable_t* self, size_t newsize) {
	size_t oldsize = self->capacity;
	aentry_t* oldarray = self->array;
	aentry_t* newarray = aloM_newa(T, aentry_t, newsize);
	for (size_t i = 0; i < newsize; ++i) {
		newarray[i] = aloH_empty;
	}
	self->array = newarray;
	self->capacity = newsize;
	self->lastfree = newarray + newsize - 1;
	for (size_t i = 0; i < oldsize; ++i) {
		aentry_t* entry = &oldarray[i];
		tsetobj(T, putety(T, self, amkey(entry), entry->hash), amval(entry));
	}
	aloM_dela(T, oldarray, oldsize);
}

/**
 ** trim table capacity to length
 */
void aloH_trim(astate T, atable_t* self) {
	if (self->length == 0) {
		if (self->capacity > 0) { /* delete array if it is non-null */
			aloM_delb(T, self->array, self->capacity);
			self->lastfree = NULL;
		}
	}
	else if (self->length < aloE_cast(size_t, self->capacity / 4)) {
		resizetable(T, self, self->capacity / 2); /* shrink table to half capacity */
	}
}

#define anyof(_self,_hash,_cond) \
	if ((_self)->length > 0) { /* element is present */ \
		aentry_t* _entry = headety(_self, _hash); \
		if (!hasprev(_entry)) \
		label: { if (_cond) return amval(_entry); else if (hasnext(_entry)) { _entry = getnext(_entry); goto label; } } \
	}

/**
 ** get value by integer key.
 */
const atval_t* aloH_geti(atable_t* self, aint key) {
	anyof(self, aloO_inthash(key), ttisint(_entry) && tgetint(_entry) == key);
	return aloO_tnil;
}

/**
 ** get value by raw string key.
 */
const atval_t* aloH_gets(astate T, atable_t* self, astr src, size_t len) {
	anyof(self, aloS_rhash(src, len, T->g->seed), ttisstr(_entry) && aloS_requal(tgetstr(_entry), src, len));
	return aloO_tnil;
}

/**
 ** get value by interned string key.
 */
const atval_t* aloH_getis(atable_t* self, astring_t* key) {
	aloE_assert(ttisistr(key), "key should be interned string.");
	anyof(self, key->hash, ttisstr(_entry) && tgetstr(_entry) == key);
	return aloO_tnil;
}

static const atval_t* aloH_geths(astate T, atable_t* self, astring_t* key) {
	aloE_assert(ttishstr(key), "key should be heaped string.");
	anyof(self, key->hash, ttisstr(_entry) && aloS_hequal(tgetstr(_entry), key));
	return aloO_tnil;
}

/**
 ** get value by string object.
 */
const atval_t* aloH_getxs(astate T, atable_t* self, astring_t* key) {
	return ttisistr(key) ? aloH_getis(self, key) : aloH_geths(T, self, key);
}

static const atval_t* getgeneric(astate T, atable_t* self, const atval_t* key) {
	anyof(self, aloV_hashof(T, key), aloV_equal(T, amkey(_entry), key));
	return aloO_tnil;
}

/**
 ** get value by tagged value.
 */
const atval_t* aloH_get(astate T, atable_t* self, const atval_t* key) {
	switch (ttype(key)) {
	case ALO_TNIL:
		return aloO_tnil;
	case ALO_TINT:
		return aloH_geti(self, tgetint(key));
	case ALO_TFLOAT: {
		if (isinvalidfltkey(tgetflt(key)))
			return aloO_tnil;
		aint i;
		if (aloV_tointx(key, &i, 0)) { /* try cast float to integer value. */
			return aloH_geti(self, i);
		}
		break;
	}
	case ALO_THSTRING:
		return aloH_geths(T, self, tgetstr(key));
	case ALO_TISTRING:
		return aloH_getis(self, tgetstr(key));
	}
	return getgeneric(T, self, key);
}

/**
 ** find a value slot by key, or create a new slot if not exist.
 */
atval_t* aloH_find(astate T, atable_t* self, const atval_t* key) {
	atval_t aux;
	switch (ttpnv(key)) {
	case ALO_TNIL: {
		aloU_invalidkey(T);
		break;
	}
	case ALO_TFLOAT: {
		afloat v = tgetflt(key);
		if (aloO_flt2int(v, &aux.v.i, 0)) {
			aux.tt = ALO_TINT;
			key = &aux;
		}
		else if (isnan(v)) {
			aloU_invalidkey(T);
		}
		break;
	}
	}
	const atval_t* value = aloH_get(T, self, key);
	if (value != aloO_tnil) {
		return aloE_cast(atval_t*, value);
	}
	aloH_ensure(T, self, 1);
	self->length ++;
	return putety(T, self, key, aloV_hashof(T, key));
}

/**
 ** find string from string set.
 */
atval_t* aloH_findxset(astate T, atable_t* self, const char* ksrc, size_t klen) {
	ahash_t hash = aloS_rhash(ksrc, klen, T->g->seed);
	anyof(self, hash, ttisstr(_entry) && aloS_requal(tgetstr(_entry), ksrc, klen));

	aloH_ensure(T, self, 1);
	atval_t key = tnewstr(aloS_new(T, ksrc, klen));
	atval_t* slot = putety(T, self, &key, hash);
	tsetstr(T, slot, tgetstr(&key));
	self->length ++;
	return slot;
}

/**
 ** put key-value pair into table and set old value to 'out' if it is not NULL.
 */
void aloH_set(astate T, atable_t* self, const atval_t* key, const atval_t* value, atval_t* out) {
	atval_t* t = aloH_find(T, self, key);
	if (out) {
		tsetobj(T, out, t);
	}
	tsetobj(T, t, value);
	aloG_barrierbackt(T, self, t);
}

static atval_t* finds(astate T, atable_t* self, astr ksrc, size_t klen, ahash_t hash) {
	anyof(self, hash, ttisstr(_entry) && aloS_requal(tgetstr(_entry), ksrc, klen));
	return NULL;
}

/**
 ** put key-value pair into table, string should be '\0'-terminated string.
 */
void aloH_sets(astate T, atable_t* self, astr ksrc, size_t klen, const atval_t* value, atval_t* out) {
	ahash_t hash = aloS_rhash(ksrc, klen, T->g->seed);
	atval_t* slot = finds(T, self, ksrc, klen, hash);
	if (slot == NULL) {
		aloH_ensure(T, self, 1);
		astring_t* str = aloS_new(T, ksrc, klen);
		atval_t key = tnewstr(str);
		slot = putety(T, self, &key, hash);
		self->length ++;
		if (str->ftagname && str->extra < ALO_NUMTM) { /* check key is TM name that able to fast get value */
			/* correct TM data */
			self->reserved &= ~(1 << str->extra);
		}
	}

	if (out) {
		tsetobj(T, out, value);
	}
	tsetobj(T, slot, value);
	aloG_barrierbackt(T, self, slot);
}

static int remety(aentry_t* entry) {
	aloE_assert(!ttisnil(entry), "no entry exist.");
	if (hasprev(entry)) {
		aentry_t* prev = getprev(entry);
		if (hasnext(entry)) {
			aentry_t* next = getnext(entry);
			prev->next = next->prev = prev - next;
		}
		else {
			prev->next = NO_NODE;
		}
	}
	else if (hasnext(entry)) {
		movety(entry, getnext(entry));
		return true;
	}
	delety(entry);
	return false;
}

/**
 ** remove entry by entry index.
 */
void aloH_rawrem(astate T, atable_t* self, ptrdiff_t* pindex, atval_t* out) {
	size_t index = *pindex;
	aloE_assert(0 <= index && index < self->capacity, "table index out of bound.");
	aentry_t* entry = self->array + index;
	aloE_assert(!ttisnil(entry), "no entry exist.");
	if (out) {
		tsetobj(T, out, amval(entry));
	}
	if (remety(entry)) {
		(*pindex)--;
	}
	self->length--;
}

/**
 ** remove entry by key, return 'true' if key founded, or 'false' in the otherwise.
 */
int aloH_remove(astate T, atable_t* self, const atval_t* key, atval_t* out) {
	if (isinvalidkey(key)) {
		return false; /* a invalid key should not exist in table. */
	}
	ahash_t hash = aloV_hashof(T, key);
	aentry_t* entry = headety(self, hash);
	if (ttisnil(entry) || hasprev(entry)) {
		return false;
	}
	else if (entry->hash == hash && aloV_equal(T, amkey(entry), key)) { /* found entry in right place */
		goto find;
	}
	else while (hasnext(entry)) {
		entry = getnext(entry);
		if (entry->hash == hash && aloV_equal(T, amkey(entry), key)) {
			goto find;
		}
	}
	return false;

	find:
	if (out) {
		tsetobj(T, out, amval(entry));
	}
	remety(entry);
	self->length--;
	return true;
}

/**
 ** iterate to next element in table.
 */
const aentry_t* aloH_next(atable_t* self, ptrdiff_t* poff) {
	aloE_assert(*poff >= -1, "illegal offset.");
	ptrdiff_t off = *poff + 1;
	if (off >= self->capacity) { /* iterating already ended */
		 return NULL;
	}
	aentry_t* const end = self->array + self->capacity;
	aentry_t* e = self->array + off;
	while (e < end) { /* find next non-nil key */
		if (!ttisnil(e)) {
			*poff = e - self->array; /* adjust offset */
			return e;
		}
		e += 1;
	}
	/* reach to end? */
	*poff = self->capacity;
	return NULL;
}
