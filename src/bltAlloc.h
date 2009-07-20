
/*
 *	Memory allocation/deallocation in BLT is performed via the
 *	global variables bltMallocPtr, bltFreePtr, and bltReallocPtr.
 *	By default, they point to the same routines that TCL uses.
 *	The routine Blt_AllocInit allows you to specify your own
 *	memory allocation/deallocation routines for BLT on a
 *	library-wide basis.
 */

#ifndef _BLT_ALLOC_H
#define _BLT_ALLOC_H

#include <assert.h>

typedef void *(Blt_MallocProc) (size_t size);
typedef void *(Blt_ReallocProc) (void *ptr, size_t size);
typedef void (Blt_FreeProc) (const void *ptr);

BLT_EXTERN void Blt_AllocInit(Blt_MallocProc *mallocProc, 
	Blt_ReallocProc *reallocProc, Blt_FreeProc *freeProc);

BLT_EXTERN void *Blt_Malloc(size_t size);
BLT_EXTERN void *Blt_Realloc(void *ptr, size_t size);
BLT_EXTERN void Blt_Free(const void *ptr);
BLT_EXTERN void *Blt_Calloc(size_t nElem, size_t size);
BLT_EXTERN char *Blt_Strdup(const char *string);

BLT_EXTERN void *Blt_MallocAbortOnError(size_t size, const char *file,int line);

BLT_EXTERN void *Blt_CallocAbortOnError(size_t nElem, size_t size, 
	const char *file, int line);
BLT_EXTERN char *Blt_StrdupAbortOnError(const char *ptr, const char *file, 
	int line);

#define Blt_AssertCalloc(n,s) (Blt_CallocAbortOnError(n,s,__FILE__, __LINE__))
#define Blt_AssertMalloc(s) (Blt_MallocAbortOnError(s,__FILE__, __LINE__))
#define Blt_AssertStrdup(s) (Blt_StrdupAbortOnError(s,__FILE__, __LINE__))

#endif /* _BLT_ALLOC_H */
