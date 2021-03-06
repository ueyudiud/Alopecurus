/*
 * astate.h
 *
 * thread and global state.
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#ifndef ASTATE_H_
#define ASTATE_H_

#include "aobj.h"
#include "ameta.h"
#include "alo.h"

#include <signal.h>

/**
 ** interned string table.
 */
typedef struct {
	size_t capacity;
	size_t length;
	astring_t** array;
} aitable_t;

/**
 ** memory stack to allocate and free memory buffer.
 */
typedef struct {
	ambuf_t* top; /* top of buffer stack */
	abyte* ptr; /* the memory pointer of buffer */
	size_t cap; /* the capacity of buffer */
	size_t len; /* always be zero */
} amstack_t;

/* get base memory buffer */
#define basembuf(T) aloE_cast(ambuf_t*, &(T)->memstk.ptr)

/**
 ** global state, shared by each thread.
 */
typedef struct alo_Global {
	aalloc alloc;
	void* context;
	ssize_t mbase; /* base memory */
	ssize_t mdebt; /* memory debt */
	size_t mestimate; /* estimated memory for live objects */
	size_t mtraced; /* traced memory (only used in GC) */
	aitable_t itable; /* intern string table */
	atval_t registry; /* global environment */
	ahash_t seed; /* randomized seed for hash */
	agct gnormal; /* list for normal objects */
	agct gfixed; /* list for fixed objects */
	agct gfnzable; /* list for finalizable objects */
	agct gtobefnz; /* list for objects to be finalize */
	agct gray; /* list for gray objects (only used in GC) */
	agct grayagain; /* list for gray again objects (only used in GC) */
	agct weakk; /* list for weak key linked collection */
	agct weakv; /* list for weak value linked collection */
	agct weaka; /* list for all weak linked collection */
	agct* sweeping; /* sweeping object list */
	size_t nfnzobj; /* count of calling finalizer each step */
	unsigned gcpausemul; /* the multiplier for GC pause */
	unsigned gcstepmul; /* the multiplier for GC step */
	acfun panic; /* the handler for uncaught errors */
	athread_t* tmain; /* main thread */
	athread_t* trun; /* the current running thread */
	astring_t* smerrmsg; /* memory error message */
	astring_t* sempty; /* empty string */
	astring_t* stagnames[TM_N]; /* special tag method names */
	astring_t* scache[ALO_STRCACHE_N][ALO_STRCACHE_M]; /* string cache */
	atval_t stack[1]; /* reserved stack, used for provided a fake stack */
	const aver_t* version; /* version of VM */
	abyte whitebit; /* current white bit */
	abyte gcstep; /* current GC step */
	abyte gckind; /* kind of GC running */
	abyte gcrunning; /* true if GC running */
	union {
		struct {
			abyte fgc : 1;
		};
		abyte flags;
	};
} aglobal_t;

/*  get total memory used by VM. */
#define totalmem(G) aloE_cast(size_t, (G)->mbase + (G)->mdebt)

/**
 ** function calling frame.
 */
typedef struct alo_Frame aframe_t;

struct alo_Frame {
	askid_t fun; /* function index */
	askid_t top; /* top index */
	aframe_t *prev, *next; /* linked list for frames */
	astr name; /* calling function name (might be null if it is unknown sources) */
	atval_t* env; /* the environment of current frame */
	union { /* specific data for different functions */
		struct { /* for Alopecurus functions */
			askid_t base;
			const ainsn_t* pc;
		} a;
		struct { /* for C functions */
			akfun kfun;
			akctx kctx;
			ptrdiff_t oef; /* the old error function for previous frame */
		} c;
	};
	int nresult; /* expected results from this frame */
	union { /* frame flags */
		struct {
			abyte mode : 3; /* 3 bits for calling mode */
			abyte falo : 1; /* is frame called by Alopecurus function */
			abyte ffnz : 1; /* is calling finalizer */
			abyte fypc : 1; /* is function calling in yieldable protection */
			abyte fact : 1; /* is active calling by VM */
			abyte fitr : 1; /* is iterator 'hasNext' checking */
			abyte fhook: 1; /* is frame calling a hook */
		};
		abyte flags;
	};
};

enum {
	FrameModeNormal,	/* a normal call */
	FrameModeTail,		/* a tail call */
	FrameModeFianlize,	/* a finalizing call by GC */
	FrameModeCompare	/* a call with '__lt' or '__le' */
};

struct alo_Thread {
	RefHeader(
		abyte status;
		union {
			struct {
				abyte fallowhook : 1;
			};
			abyte flags;
		};
		abyte hookmask; /* hook mask */
	);
	aglobal_t* g; /* global state */
	athread_t* caller;
	aframe_t* frame;
	acap_t* captures; /* captures */
	askid_t stack; /* stack base */
	askid_t top; /* top of stack */
	size_t stacksize; /* current stack size */
	ajmp_t* label;
	ptrdiff_t errfun; /* index of error handling function */
	amstack_t memstk; /* memory stack */
	ahfun hook;
	aframe_t base_frame;
	uint16_t nframe; /* frame depth */
	uint16_t nxyield; /* nested non-yieldable depth */
	uint16_t nccall; /* C caller depth */
};

/**
 ** the extra stack for tagged methods call and other utility methods.
 */
#define EXTRA_STACK 8

#define Gd(T) aglobal_t* G = (T)->g

ALO_IFUN void aloR_closeframe(astate, aframe_t*);
ALO_IFUN void aloR_deletethread(astate, athread_t*);
ALO_IFUN aframe_t* aloR_topframe(astate);
ALO_IFUN aframe_t* aloR_pushframe(astate);

#endif /* ASTATE_H_ */
