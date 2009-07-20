
/*
 * bltVar.h --
 *
 *	Copyright 1993-2004 George A Howlett.
 *
 *	Permission is hereby granted, free of charge, to any person
 *	obtaining a copy of this software and associated documentation
 *	files (the "Software"), to deal in the Software without
 *	restriction, including without limitation the rights to use,
 *	copy, modify, merge, publish, distribute, sublicense, and/or
 *	sell copies of the Software, and to permit persons to whom the
 *	Software is furnished to do so, subject to the following
 *	conditions:
 *
 *	The above copyright notice and this permission notice shall be
 *	included in all copies or substantial portions of the
 *	Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 *	KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 *	WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 *	PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 *	OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 *	OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 *	OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _BLT_VAR_H
#define _BLT_VAR_H

/*
 * The following procedures are helper routines to build TCL variables
 * for special variable resolvers.  
 */
BLT_EXTERN Tcl_Var Blt_GetCachedVar(Blt_HashTable *tablePtr, const char *label,
	Tcl_Obj *objPtr);

BLT_EXTERN void Blt_FreeCachedVars(Blt_HashTable *tablePtr);

/*
 * The following declarations are missing from the TCL public API.
 */
#ifndef USE_TCL_STUBS
typedef struct _Tcl_ResolvedVarInfo Tcl_ResolvedVarInfo;

typedef int (Tcl_ResolveCmdProc)(Tcl_Interp* interp, const char *name, 
	Tcl_Namespace *context, int flags, Tcl_Command *rPtr);

typedef int (Tcl_ResolveVarProc)(Tcl_Interp *interp, const char *name, 
	Tcl_Namespace *context, int flags, Tcl_Var *rPtr);

typedef int (Tcl_ResolveCompiledVarProc)(Tcl_Interp* interp, const char *name,
	int length, Tcl_Namespace *context, Tcl_ResolvedVarInfo **rPtr);

BLT_EXTERN void Tcl_SetNamespaceResolvers(Tcl_Namespace *nsPtr, 
	Tcl_ResolveCmdProc *cmdProc, Tcl_ResolveVarProc *varProc, 
	Tcl_ResolveCompiledVarProc *compiledVarProc);
#endif

#endif /* _BLT_VAR_H */
