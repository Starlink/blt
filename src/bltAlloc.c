#include "bltInt.h"

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif /* HAVE_MALLOC_H */

/*
 *	Memory allocation/deallocation in BLT is performed via the global
 *	variables bltMallocPtr, bltFreePtr, and bltReallocPtr.  By default,
 *	they point to the same routines that TCL uses.  The routine
 *	Blt_AllocInit allows you to specify your own memory allocation and
 *	deallocation routines for BLT on a library-wide basis.
 */
#ifdef WIN32

#ifdef __GNUC__
#ifndef USE_TCL_STUBS
EXTERN char *Tcl_Alloc(unsigned int size);
EXTERN void Tcl_Free(char * ptr);
EXTERN char *Tcl_Realloc(char *ptr, unsigned int size);
#endif
#endif /*__GNUC__*/

#else

#if (_TCL_VERSION >= _VERSION(8,1,0)) 
#ifndef USE_TCL_STUBS
BLT_EXTERN Blt_MallocProc TclpAlloc;
BLT_EXTERN Blt_FreeProc TclpFree;
BLT_EXTERN Blt_ReallocProc TclpRealloc;
#endif
#else
#if !HAVE_DECL_FREE
BLT_EXTERN void free (void *);
#endif
#endif /* >= 8.1.0 */
#endif /* WIN32 */

static Blt_MallocProc *bltMallocPtr;
static Blt_ReallocProc *bltReallocPtr;
static Blt_FreeProc *bltFreePtr;

void *
Blt_Malloc(size_t size) 
{
    return (*bltMallocPtr)(size);
}


void
Blt_Free(const void *mem)
{
    (*bltFreePtr)((void *)mem);
}

void *
Blt_Realloc(void *ptr, size_t size)
{
    return (*bltReallocPtr)(ptr, size);
}

void *
Blt_Calloc(size_t nElem, size_t elemSize)
{
    void *ptr;
    size_t size;

    size = nElem * elemSize;
    ptr = (*bltMallocPtr)(size);
    if (ptr != NULL) {
	memset(ptr, 0, size);
    }
    return ptr;
}

void *
Blt_MallocAbortOnError(size_t size, const char *fileName, int lineNum) 
{
    void *ptr;

    ptr = (*bltMallocPtr)(size);
    if (ptr == NULL) {
#ifdef WINDEBUG
	PurifyPrintf("line %d of %s: can't allocate %lu bytes of memory\n", 
		lineNum, fileName, (unsigned long)size);
#endif
	fprintf(stderr, "line %d of %s: can't allocate %lu bytes of memory\n", 
		lineNum, fileName, (unsigned long)size);

	fflush(stderr);
	abort();
    }
    return ptr;
}

void *
Blt_CallocAbortOnError(size_t nElem, size_t elemSize, const char *fileName, 
		       int lineNum)
{
    void *ptr;
    size_t size;

    size = nElem * elemSize;
    ptr = (*bltMallocPtr)(size);
    if (ptr == NULL) {
#ifdef WINDEBUG
	PurifyPrintf(
		"line %d of %s: can't allocate %lu item(s) of size %lu each\n",
		lineNum, fileName, (unsigned long)nElem, 
		(unsigned long)elemSize);
#endif
	fprintf(stderr, 
		"line %d of %s: can't allocate %lu item(s) of size %lu each\n", 
		lineNum, fileName, (unsigned long)nElem, 
		(unsigned long)elemSize);
	fflush(stderr);
	abort();
    }
    memset(ptr, 0, size);
    return ptr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Strdup --
 *
 *      Create a copy of the string from heap storage.
 *
 * Results:
 *      Returns a pointer to the need string copy.
 *
 *---------------------------------------------------------------------------
 */
char *
Blt_Strdup(const char *string)
{
    size_t size;
    char *ptr;

    size = strlen(string) + 1;
    ptr = (*bltMallocPtr)(size * sizeof(char));
    if (ptr != NULL) {
	strcpy(ptr, string);
    }
    return ptr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_StrdupAbortOnError --
 *
 *      Create a copy of the string from heap storage.
 *
 * Results:
 *      Returns a pointer to the need string copy.
 *
 *---------------------------------------------------------------------------
 */
char *
Blt_StrdupAbortOnError(const char *string, const char *fileName, int lineNum)
{
    size_t size;
    char *ptr;

    size = strlen(string) + 1;
    ptr = (*bltMallocPtr)(size * sizeof(char));
    if (ptr == NULL) {
#ifdef WINDEBUG
	PurifyPrintf("line %d of %s: can't allocate string of %lu bytes\n",
		     lineNum, fileName, (unsigned long)size);
#endif
	fprintf(stderr, "line %d of %s: can't allocate string of %lu bytes\n",
		lineNum, fileName, (unsigned long)size);
	fflush(stderr);
	abort();
    }
    strcpy(ptr, string);
    return ptr;
}

void
Blt_AllocInit(
    Blt_MallocProc *mallocProc,
    Blt_ReallocProc *reallocProc,
    Blt_FreeProc *freeProc)
{
    Blt_MallocProc *defMallocProc;
    Blt_FreeProc *defFreeProc;
    Blt_ReallocProc *defReallocProc;
    
    /* 
     * Try to use the same memory allocator/deallocator that TCL is
     * using. Before 8.1 it used malloc/free.
     */
#ifdef WIN32
    defMallocProc = (Blt_MallocProc *)Tcl_Alloc;
    defFreeProc = (Blt_FreeProc *)Tcl_Free; 
    defReallocProc = (Blt_ReallocProc *)Tcl_Realloc; 
#else
#if (_TCL_VERSION >= _VERSION(8,1,0)) 
    /* 
     * We're pointing to the private TclpAlloc/TclpFree instead of public
     * Tcl_Alloc/Tcl_Free routines because they don't automatically trigger a
     * panic when not enough memory is available. There are cases (such as
     * allocating a very large vector) where an out-of-memory error is
     * recoverable.
     */
    defMallocProc = (Blt_MallocProc *)TclpAlloc;
    defFreeProc = (Blt_FreeProc *)TclpFree; 
    defReallocProc = (Blt_ReallocProc *)TclpRealloc; 
#else 
    defMallocProc = malloc;
    defFreeProc = free; 
    defReallocProc = realloc;
#endif /* >= 8.1.0 */
#endif /* WIN32 */
    if (bltMallocPtr == NULL) {
	bltMallocPtr = (mallocProc != NULL) ? mallocProc : defMallocProc;
    }
    if (bltFreePtr == NULL) {
	bltFreePtr = (freeProc != NULL) ? freeProc : defFreeProc;
    }
    if (bltReallocPtr == NULL) {
	bltReallocPtr = (reallocProc != NULL) ? reallocProc : defReallocProc;
    }
}

