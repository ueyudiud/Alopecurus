/*
 * aaux.h
 *
 * auxiliary API.
 *
 *  Created on: 2019年7月26日
 *      Author: ueyudiud
 */

#ifndef AAUX_H_
#define AAUX_H_

#include "alo.h"

ALO_API astate aloL_newstate(void);

ALO_API void aloL_pushscopedcfunction(astate, acfun);
ALO_API void aloL_newstringlist_(astate, size_t, ...);

#define aloL_newstringlist(T,args...) aloL_newstringlist_(T, sizeof((astr[]) { args }) / sizeof(astr), ##args)

ALO_API void aloL_ensure(astate, size_t);
ALO_API void aloL_checkany(astate, ssize_t);
ALO_API void aloL_checktype(astate, ssize_t, int);
ALO_API int aloL_checkbool(astate, ssize_t);
ALO_API int aloL_getoptbool(astate, ssize_t, int);
ALO_API aint aloL_checkinteger(astate, ssize_t);
ALO_API aint aloL_getoptinteger(astate, ssize_t, aint);
ALO_API afloat aloL_checknumber(astate, ssize_t);
ALO_API afloat aloL_getoptnumber(astate, ssize_t, afloat);
ALO_API const char* aloL_checklstring(astate, ssize_t, size_t*);
ALO_API const char* aloL_getoptlstring(astate, ssize_t, const char*, size_t, size_t*);
ALO_API int aloL_checkenum(astate, ssize_t, astr, const astr[]);

#define aloL_checkstring(T,index) aloL_checklstring(T, index, NULL)
#define aloL_getoptstring(T,index,def) aloL_getoptlstring(T, index, def, 0, NULL)

ALO_API void aloL_checkcall(astate, ssize_t);
ALO_API const char* aloL_tostring(astate, ssize_t, size_t*);
ALO_API astr aloL_sreplace(astate, astr, astr, astr);

ALO_API void aloL_setfuns(astate, ssize_t, const acreg_t*);

ALO_API int aloL_getmetatable(astate, astr);
ALO_API int aloL_getsubtable(astate, ssize_t, astr);
ALO_API int aloL_getsubfield(astate, ssize_t, astr, astr);
ALO_API int aloL_callselfmeta(astate, ssize_t, astr);

#define aloL_getmodfield(T,mod,key) aloL_getsubfield(T, ALO_REGISTRY_INDEX, mod, key)

ALO_API void aloL_require(astate, astr, acfun, int);
ALO_API int aloL_compiles(astate, ssize_t, astr, astr);
ALO_API int aloL_compilef(astate, astr, astr);
ALO_API int aloL_loadf(astate, astr);
ALO_API int aloL_savef(astate, astr, int);

/**
 ** error handling
 */

ALO_API int aloL_getframe(astate, int, astr, aframeinfo_t*);
ALO_API void aloL_where(astate, int);
ALO_API int aloL_errresult_(astate, astr);
ALO_API anoret aloL_error(astate, astr, ...);

ALO_API anoret aloL_argerror(astate, ssize_t, astr, ...);
ALO_API anoret aloL_typeerror(astate, ssize_t, astr);
ALO_API anoret aloL_tagerror(astate, ssize_t, int);

#define aloL_errresult(T,c,m) ((c) ? (alo_pushboolean(T, true), 1) : aloL_errresult_(T, m))


/**
 ** base layer of messaging and logging.
 */

#define aloL_fflush(file) fflush(file)
#define aloL_fputs(file,src,len) fwrite(src, sizeof(char), len, file)
#define aloL_fputc(file,ch) fputc(ch, file)
#define aloL_fputln(file) (aloL_fputs(file, "\n", 1), aloL_fflush(file))

/**============================================================================
 ** the contents below this line only takes effect with standard class library
 **============================================================================*/

ALO_API void aloL_checkclassname(astate, ssize_t);
ALO_API int aloL_getsimpleclass(astate, astr);
ALO_API void aloL_newclass_(astate, astr, ...);

#define aloL_newclass(T,name,parents...) aloL_newclass_(T, name, ##parents, NULL)

/**
 ** functions for memory buffer.
 */

/** the minimum heaped memory buffer capacity */
#define ALOL_MBUFSIZE (256 * __SIZEOF_POINTER__)

#define aloL_usebuf(T,n) for (alo_newbuf(T, n); n->buf; alo_popbuf(T, n))

/* buffer field getter */
#define aloL_blen(b) ((b)->len) /* readable and writable */
#define aloL_braw(b) aloE_cast(abyte*, (b)->buf) /* read-only */

/* basic buffer operation */
#define aloL_bclean(b) aloE_void(aloL_blen(b) = 0)
#define aloL_bempty(b) (aloL_blen(b) == 0)
#define aloL_bcheck(T,b,l) ({ size_t $req = (b)->len + (l); if ($req > (b)->cap) alo_growbuf(T, b, $req); })

/* buffer as string builder */
#define aloL_bputc(T,b,ch) (aloE_cast(void, aloL_blen(b) < (b)->cap || (aloL_bcheck(T, b, 1), true)), aloL_bputcx(b, ch))
#define aloL_bputls(T,b,s,l) aloL_bputm(T, b, s, (l) * sizeof(char))
#define aloL_bputxs(T,b,s) aloL_bputls(T, b, ""s, sizeof(s) / sizeof(char) - 1)
#define aloL_bsetc(b,i,ch) (aloL_braw(b)[i] = aloE_byte(ch))
#define aloL_bputcx(b,ch) aloL_bsetc(b, aloL_blen(b)++, ch)
#define aloL_b2str(T,b) aloE_cast(astr, (aloL_bputc(T, b, '\0'), aloL_braw(b))) /* read-only, not alive till memory buffer pop */

/* buffer as object stack */
#define aloL_bpush(T,b,o) aloL_bputm(T, b, &(o), sizeof(o))
#define aloL_bpop(b,t) \
	aloE_check(aloL_blen(b) >= sizeof(t), "no "#t" in buffer.", aloE_cast(typeof(t)*, aloL_braw(b) + (aloL_blen(b) -= sizeof(t))))

ALO_API void aloL_bputm(astate, ambuf_t*, const void*, size_t);
ALO_API void aloL_bputs(astate, ambuf_t*, astr);
ALO_API void aloL_breptc(astate, ambuf_t*, astr, int, int);
ALO_API void aloL_brepts(astate, ambuf_t*, astr, astr, astr);
ALO_API void aloL_bputf(astate, ambuf_t*, astr, ...);
ALO_API void aloL_bputvf(astate, ambuf_t*, astr, va_list);
ALO_API void aloL_bwrite(astate, ambuf_t*, ssize_t);
ALO_API astr aloL_bpushstring(astate, ambuf_t*);

ALO_API int aloL_bwriter(astate, void*, const void*, size_t);

#endif /* AAUX_H_ */
