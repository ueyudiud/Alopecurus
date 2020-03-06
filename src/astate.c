/*
 * astate.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#include "astr.h"
#include "atab.h"
#include "afun.h"
#include "astate.h"
#include "ameta.h"
#include "adebug.h"
#include "agc.h"
#include "ado.h"
#include "alex.h"
#include "alo.h"

#include <time.h>
#include <inttypes.h>

const aver_t aloR_version = { ALO_VERSION_NUM };

typedef struct {
	char extra[ALO_THREAD_EXTRASPACE];
	athread_t state;
} TX;

#define getraw(T) aloE_cast(TX*, aloE_cast(void*, T) - ALO_THREAD_EXTRASPACE)

typedef struct {
	TX t;
	aglobal_t g;
} TG;

static void init_stack(astate T, athread_t* thread) {
	aloM_clsb(thread->stack, thread->stacksize);
	aloM_newb(T, thread->stack, thread->stacksize, ALO_BASESTACKSIZE);
	thread->top = thread->stack + 1;
	/* initialize base frame stack */
	thread->base_frame.fun = thread->stack;
	thread->base_frame.top = thread->top + ALO_MINSTACKSIZE;
	tsetnil(thread->base_frame.fun); /* no function for base stack */
}

static void init_registry(astate T) {
	Gd(T);
	atable_t* registry = aloH_new(T);
	G->registry = tnewref(registry, ALO_TTABLE);
	aloH_ensure(T, registry, ALO_GLOBAL_INITIALIZESIZE);
}

static void initialize(astate T, __attribute__((unused)) void* context) {
	init_stack(T, T);
	init_registry(T);
	aloS_init(T);
	aloT_init(T);
	aloX_init(T);
	aloU_init(T);
	aloE_openthread(T, NULL);
}

static ahash_t newseed(astate T) {
	size_t pool[4];
	pool[0] = aloE_addr(T);
	pool[1] = aloE_addr(pool);
	pool[2] = aloE_addr(alo_newstate);
	pool[3] = aloE_addr(aloO_tnil);
	return aloS_rhash(aloE_cast(const char*, pool), sizeof(pool), time(NULL));
}

static void preinit(astate T, aglobal_t* G) {
	T->status = ThreadStateYield;
	T->caller = NULL;
	T->g = G;
	T->frame = &T->base_frame;
	T->berrno = 0;
	T->nccall = 0;
	T->nframe = 1;
	T->nxyield = 0;
	T->label = NULL;
	T->flags = 0;
	T->base_frame.name = "<start>";
	T->base_frame.next = T->base_frame.prev = NULL;
	T->base_frame.fun = T->base_frame.top = NULL;
	T->base_frame.flags = 0;
	T->base_frame.c.kfun = NULL;
	T->base_frame.c.ctx = NULL;
	T->captures = NULL;
	T->hook = NULL;
	T->hookmask = 0;
	T->fallowhook = true;
}

astate alo_newstate(aalloc alloc, void* ctx) {
	TG* tg = aloE_cast(TG*, alloc(ctx, NULL, 0, sizeof(TG)));
	if (tg == NULL)
		return NULL;
	athread_t* T = &tg->t.state;
	aglobal_t* G = &tg->g;
	G->alloc = alloc;
	G->context = ctx;
	G->seed = newseed(T);
	G->mbase = sizeof(TG);
	G->mdebt = 0;
	G->mestimate = sizeof(TG);
	G->mtraced = 0;
	G->gnormal = G->gfixed = G->gfnzable = G->gtobefnz = NULL;
	G->gcstepmul = ALO_GCSTEPMUL;
	G->gcpausemul = ALO_GCPAUSEMUL;
	G->panic = NULL;
	G->tmain = T;
	G->trun = T;
	G->smerrmsg = NULL;
	G->sempty = NULL;
	G->version = aloR_version;
	G->whitebit = aloG_white1;
	G->gcstep = GCStepPause;
	G->gckind = GCKindNormal;
	G->gcrunning = false;
	tsetnil(&G->registry); /* registry = nil */
	G->flags = 0;
	G->itable.length = 0;
	G->itable.capacity = 0;
	G->itable.array = NULL;
	G->ctable.length = 0;
	G->ctable.capacity = 0;
	G->ctable.array = NULL;
	T->gcprev = NULL;
	T->tt = ALO_TTHREAD;
	T->mark = aloG_white1;
	preinit(T, G);

	/* run initializer with memory allocation, if failed, delete thread directly */
	if (aloD_prun(T, initialize, NULL) != ThreadStateRun) {
		alo_deletestate(T);
		return NULL;
	}

	aloE_log("alo in debug mode");
	aloE_log("new alo state initialized, seed: %"PRId64, G->seed);

	G->fgc = true; /* enable GC */
	T->status = ThreadStateRun;
	return T;
}

athread_t* aloR_newthread(astate T) {
	Gd(T);
	athread_t* thread = &aloM_newo(T, TX)->state;
	aloG_register(T, thread, ALO_TTHREAD);
	preinit(thread, G);
	init_stack(T, thread);
	aloE_openthread(thread, T);
	return thread;
}

/**
 ** close frame and free all references in the frame.
 */
void aloR_closeframe(astate T, aframe_t* frame) {
	frame->flags = 0; /* clear frame flags, the frame will regard as a C functions calling frame now. */
	/* make old context unreachable */
	frame->c.kfun = NULL;
}

static void destory_thread(astate T, athread_t* v, int close) {
	aframe_t* frame = aloR_topframe(v);
	while (frame->prev) { /* true if frame is not base frame */
		aframe_t* current = frame;
		frame = frame->prev; /* move to previous frame */
		aloR_closeframe(T, current);
		aloM_delo(T, current); /* delete current frame */
	}
	if (close) {
		aloF_close(T, T->stack);
	}
	else {
		acap *c, *n = T->captures;
		while ((c = n)) {
			n = c->prev;
			aloM_delo(T, c); /* delete capture */
		}
	}
	aloM_delb(T, v->stack, v->stacksize); /* clear stack */
}

void alo_deletestate(astate rawT) {
	Gd(rawT);
	astate T = G->tmain;
	aloE_closethread(T);
	aloG_clear(T); /* clean all objects */
	destory_thread(T, T, false);
	aloE_assert(totalmem(G) == sizeof(TG), "memory leaked");
	G->alloc(G->context, getraw(T), sizeof(TG), 0);
}

/**
 ** delete thread.
 */
void aloR_deletethread(astate T, athread_t* v) {
	aloE_closethread(T);
	destory_thread(T, v, true);
	aloM_delo(T, getraw(T));
}

/**
 ** get top of frames.
 */
aframe_t* aloR_topframe(astate T) {
	aframe_t* frame = T->frame;
	while (frame->next) {
		frame = frame->next;
	}
	return frame;
}

aframe_t* aloR_pushframe(astate T) {
	aframe_t* oldframe = T->frame;
	aloE_assert(oldframe->next == NULL, "not reach top of frame yet.");
	aframe_t* newframe = aloM_newo(T, aframe_t);
	newframe->prev = oldframe;
	oldframe->next = newframe;
	newframe->next = NULL;
	T->nframe++;
	return newframe;
}
