
/*
 * bltWinop.c --
 *
 * This module implements simple window commands for the BLT toolkit.
 *
 *	Copyright 1991-2004 George A Howlett.
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

#include "bltInt.h"

#ifndef NO_WINOP
#include "bltOp.h"
#include "bltPicture.h"
#include "bltImage.h"
#include <X11/Xutil.h>
#include "tkDisplay.h"

#define CLAMP(c)	((((c) < 0.0) ? 0.0 : ((c) > 255.0) ? 255.0 : (c)))
static Tcl_ObjCmdProc WinopCmd;

static int
GetRealizedWindowFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, 
			 Tk_Window *tkwinPtr)
{
    const char *string;
    Tk_Window tkwin;

    string = Tcl_GetString(objPtr);
    assert(interp != NULL);
    tkwin = Tk_NameToWindow(interp, string, Tk_MainWindow(interp));
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    if (Tk_WindowId(tkwin) == None) {
	Tk_MakeWindowExist(tkwin);
    }
    *tkwinPtr = tkwin;
    return TCL_OK;
}

static int
GetWindowFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, Window *windowPtr)
{
    const char *string;

    string = Tcl_GetString(objPtr);
    if (string[0] == '.') {
	Tk_Window tkwin;

	if (GetRealizedWindowFromObj(interp, objPtr, &tkwin) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tk_IsTopLevel(tkwin)) {
	    *windowPtr = Blt_GetWindowId(tkwin);
	} else {
	    *windowPtr = Tk_WindowId(tkwin);
	}
    } else if (strcmp(string, "root") == 0) {
	*windowPtr = Tk_RootWindow(Tk_MainWindow(interp));
    } else {
	int xid;

	if (Tcl_GetIntFromObj(interp, objPtr, &xid) != TCL_OK) {
	    return TCL_ERROR;
	}
#ifdef WIN32
	{ 
	    static TkWinWindow tkWinWindow;
	    
	    tkWinWindow.handle = (HWND)xid;
	    tkWinWindow.winPtr = NULL;
	    tkWinWindow.type = TWD_WINDOW;
	    *windowPtr = (Window)&tkWinWindow;
	}
#else
	*windowPtr = (Window)xid;
#endif
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
LowerOp(Tk_Window tkMain, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int i;

    for (i = 2; i < objc; i++) {
	Window window;

	if (GetWindowFromObj(interp, objv[i], &window) != TCL_OK) {
	    return TCL_ERROR;
	}
	XLowerWindow(Tk_Display(tkMain), window);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
RaiseOp(Tk_Window tkMain, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int i;

    for (i = 2; i < objc; i++) {
	Window window;

	if (GetWindowFromObj(interp, objv[i], &window) != TCL_OK) {
	    return TCL_ERROR;
	}
	XRaiseWindow(Tk_Display(tkMain), window);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
MapOp(Tk_Window tkMain, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int i;

    for (i = 2; i < objc; i++) {
	const char *string;
	Window window;

	string = Tcl_GetString(objv[i]);
	if (string[0] == '.') {
	    Tk_Window tkwin;
	    Tk_FakeWin *fakePtr;

	    if (GetRealizedWindowFromObj(interp, objv[i], &tkwin) != TCL_OK) {
		return TCL_ERROR;
	    }
#ifdef  WIN32
	Tk_MapWindow(tkwin);		
#endif
	    fakePtr = (Tk_FakeWin *) tkwin;
	    fakePtr->flags |= TK_MAPPED;
	    window = Tk_WindowId(tkwin);
	} else {
	    int xid;

	    if (Tcl_GetIntFromObj(interp, objv[i], &xid) != TCL_OK) {
		return TCL_ERROR;
	    }
	    window = (Window)xid;
	}
	XMapWindow(Tk_Display(tkMain), window);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
MoveOp(Tk_Window tkMain, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int x, y;
    Window window;

    if (GetWindowFromObj(interp, objv[2], &window) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tk_GetPixelsFromObj(interp, tkMain, objv[3], &x) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tk_GetPixelsFromObj(interp, tkMain, objv[4], &y) != TCL_OK) {
	return TCL_ERROR;
    }
    XMoveWindow(Tk_Display(tkMain), window, x, y);
    return TCL_OK;
}

/*ARGSUSED*/
static int
UnmapOp(Tk_Window tkMain, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int i;

    for (i = 2; i < objc; i++) {
	const char *string;
	Window window;

	string = Tcl_GetString(objv[i]);
	if (string[0] == '.') {
	    Tk_Window tkwin;
	    Tk_FakeWin *fakePtr;

	    if (GetRealizedWindowFromObj(interp, objv[i], &tkwin) != TCL_OK) {
		return TCL_ERROR;
	    }
	    fakePtr = (Tk_FakeWin *) tkwin;
	    fakePtr->flags &= ~TK_MAPPED;
	    window = Tk_WindowId(tkwin);
	} else {
	    int xid;

	    if (Tcl_GetIntFromObj(interp, objv[i], &xid) != TCL_OK) {
		return TCL_ERROR;
	    }
	    window = (Window)xid;
	}
	XMapWindow(Tk_Display(tkMain), window);
    }
    return TCL_OK;
}

/* ARGSUSED */
static int
ChangesOp(Tk_Window tkMain, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tk_Window tkwin;

    if (GetRealizedWindowFromObj(interp, objv[2], &tkwin) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tk_IsTopLevel(tkwin)) {
	XSetWindowAttributes attrs;
	Window window;
	unsigned int mask;

	window = Blt_GetWindowId(tkwin);
	attrs.backing_store = WhenMapped;
	attrs.save_under = True;
	mask = CWBackingStore | CWSaveUnder;
	XChangeWindowAttributes(Tk_Display(tkwin), window, mask, &attrs);
    }
    return TCL_OK;
}

/* ARGSUSED */
static int
QueryOp(Tk_Window tkMain, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int rootX, rootY, childX, childY;
    Window root, child;
    unsigned int mask;

    /* GetCursorPos */
    if (XQueryPointer(Tk_Display(tkMain), Tk_WindowId(tkMain), &root,
	    &child, &rootX, &rootY, &childX, &childY, &mask)) {
	Tcl_Obj *listObjPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewIntObj(rootX));
	Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewIntObj(rootY));
	Tcl_SetObjResult(interp, listObjPtr);
    }
    return TCL_OK;
}

/* ARGSUSED */
static int
TreeOp(Tk_Window tkMain, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Window *ancestors, window, root, parent;
    size_t nAncestors;

    if (GetWindowFromObj(interp, objv[2], &window) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((XQueryTree(Tk_Display(tkMain), window, &root, &parent, &ancestors, 
		&nAncestors)) && (nAncestors > 0)) {
	unsigned int i;
	Tcl_Obj *listObjPtr;
	char string[200];
	Tcl_Obj *objPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	sprintf(string, "0x%x", (unsigned int)root);
	objPtr = Tcl_NewStringObj(string , -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	sprintf(string, "0x%x", (unsigned int)parent);
	objPtr = Tcl_NewStringObj(string , -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	sprintf(string, "0x%x", (unsigned int)window);
	objPtr = Tcl_NewStringObj(string , -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	for (i = 0; i < nAncestors; i++) {
	    sprintf(string, "0x%x", (unsigned int)ancestors[i]);
	    objPtr = Tcl_NewStringObj(string , -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	Tcl_SetObjResult(interp, listObjPtr);
	if (ancestors != NULL) {
	    XFree((char *)ancestors); 
	}
    }
    return TCL_OK;
}


/*ARGSUSED*/
static int
WarpToOp(Tk_Window tkMain, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    if (objc == 3) {
	Tk_Window tkwin;

	if (GetRealizedWindowFromObj(interp, objv[2], &tkwin) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Tk_IsMapped(tkwin)) {
	    Tcl_AppendResult(interp, "can't warp to unmapped window \"",
		     Tk_PathName(tkwin), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	XWarpPointer(Tk_Display(tkwin), None, Tk_WindowId(tkwin),
	     0, 0, 0, 0, Tk_Width(tkwin) / 2, Tk_Height(tkwin) / 2);
    } else if (objc == 4) {
	int x, y;
	Window root;
	
	if ((Tk_GetPixelsFromObj(interp, tkMain, objv[2], &x) != TCL_OK) ||
	    (Tk_GetPixelsFromObj(interp, tkMain, objv[3], &y) != TCL_OK)) {
	    return TCL_ERROR;
	}
	root = Tk_RootWindow(tkMain);
	XWarpPointer(Tk_Display(tkMain), None, root, 0, 0, 0, 0, x, y);
    }
    return QueryOp(tkMain, interp, 0, (Tcl_Obj **)NULL);
}

static Blt_OpSpec winOps[] =
{
    {"changes", 1, ChangesOp, 3, 3, "window",},
    {"lower",   1, LowerOp,   2, 0, "window ?window?...",},
    {"map",     2, MapOp,     2, 0, "window ?window?...",},
    {"move",    2, MoveOp,    5, 5, "window x y",},
    {"query",   1, QueryOp,   2, 2, "",},
    {"raise",   1, RaiseOp,   2, 0, "window ?window?...",},
    {"tree",    1, TreeOp,    3, 3, "window",},
    {"unmap",   1, UnmapOp,   2, 0, "window ?window?...",},
    {"warpto",  1, WarpToOp,  2, 5, "?window?",},
};

static int nWinOps = sizeof(winOps) / sizeof(Blt_OpSpec);

/* ARGSUSED */
static int
WinopCmd(ClientData clientData, Tcl_Interp *interp, int objc, 
	 Tcl_Obj *const *objv)
{
    Tcl_ObjCmdProc *proc;
    int result;
    Tk_Window tkwin;

    proc = Blt_GetOpFromObj(interp, nWinOps, winOps, BLT_OP_ARG1,  objc, objv, 
	0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    tkwin = Tk_MainWindow(interp);
    result = (*proc) (tkwin, interp, objc, objv);
    return result;
}

int
Blt_WinopCmdInitProc(Tcl_Interp *interp)
{
    static Blt_InitCmdSpec cmdSpec = {"winop", WinopCmd,};

    return Blt_InitCmd(interp, "::blt", &cmdSpec);
}

#endif /* NO_WINOP */
