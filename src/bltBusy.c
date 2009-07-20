
/*
 * bltBusy.c --
 *
 * This module implements busy windows for the BLT toolkit.
 *
 *	Copyright 1993-2009 George A Howlett.
 *
 *	Permission is hereby granted, free of charge, to any person obtaining
 *	a copy of this software and associated documentation files (the
 *	"Software"), to deal in the Software without restriction, including
 *	without limitation the rights to use, copy, modify, merge, publish,
 *	distribute, sublicense, and/or sell copies of the Software, and to
 *	permit persons to whom the Software is furnished to do so, subject to
 *	the following conditions:
 *
 *	The above copyright notice and this permission notice shall be
 *	included in all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *	LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *	OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "bltInt.h"

#ifndef NO_BUSY
#include "bltOp.h"
#include "bltHash.h"
#include "tkDisplay.h"
#define BUSYDEBUG 0

#ifndef TK_REPARENTED
#define TK_REPARENTED 0
#endif

#define BUSY_THREAD_KEY	"BLT Busy Data"

typedef struct {
    Display *display;		/* Display of busy window */
    Tcl_Interp *interp;		/* Interpreter where "busy" command was
				 * created. It's used to key the searches in the
				 * window hierarchy. See the "windows"
				 * command. */

    Tk_Window tkBusy;		/* Busy window: Transparent window used to block
				 * delivery of events to windows underneath
				 * it. */

    Tk_Window tkParent;		/* Parent window of the busy window. It may be
				 * the reference window (if the reference is a
				 * toplevel) or a mutual ancestor of the
				 * reference window */

    Tk_Window tkRef;		/* Reference window of the busy window.  It's is
				 * used to manage the size and position of the
				 * busy window. */


    int x, y;			/* Position of the reference window */

    int width, height;		/* Size of the reference window. Retained to
				 * know if the reference window has been
				 * reconfigured to a new size. */

    int isBusy;			/* Indicates whether the transparent window
				 * should be displayed. This can be different
				 * from what Tk_IsMapped says because the a
				 * sibling reference window may be unmapped,
				 * forcing the busy window to be also hidden. */

    int menuBar;		/* Menu bar flag. */
    Tk_Cursor cursor;		/* Cursor for the busy window. */

    Blt_HashEntry *hashPtr;	/* Used the delete the busy window entry out of
				 * the global hash table. */
    Blt_HashTable *tablePtr;
} Busy;

#ifdef WIN32
#define DEF_BUSY_CURSOR "wait"
#else
#define DEF_BUSY_CURSOR "watch"
#endif

static Blt_ConfigSpec configSpecs[] =
{
    {BLT_CONFIG_CURSOR, "-cursor", "busyCursor", "BusyCursor",
	DEF_BUSY_CURSOR, Blt_Offset(Busy, cursor), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

typedef struct {
    Blt_HashTable busyTable;	/* Hash table of busy window structures keyed
				 * by the address of the reference Tk
				 * window */
    Tk_Window tkMain;
    Tcl_Interp *interp;
} BusyInterpData;

static Tk_GeomRequestProc BusyGeometryProc;
static Tk_GeomLostSlaveProc BusyCustodyProc;
static Tk_GeomMgr busyMgrInfo =
{
    (char *)"busy",		/* Name of geometry manager used by winfo */
    BusyGeometryProc,		/* Procedure to for new geometry requests */
    BusyCustodyProc,		/* Procedure when window is taken away */
};

/* Forward declarations */
static Tcl_FreeProc DestroyBusy;
static Tk_EventProc BusyEventProc;
static Tk_EventProc RefWinEventProc;
static Tcl_ObjCmdProc BusyCmd;
static Tcl_InterpDeleteProc BusyInterpDeleteProc;

static void
ShowBusyWindow(Busy *busyPtr)
{
    if (busyPtr->tkBusy != NULL) {
	Tk_MapWindow(busyPtr->tkBusy);
	/* 
	 * Always raise the busy window just in case new sibling windows have
	 * been created in the meantime. Can't use Tk_RestackWindow because it
	 * doesn't work under Win32.
	 */
	XRaiseWindow(busyPtr->display, Tk_WindowId(busyPtr->tkBusy));
    }
#ifdef WIN32
    {
	POINT point;
	/*
	 * In Win32 cursors aren't associated with windows.  Tk fakes this by
	 * watching <Motion> events on its windows.  Tk will automatically
	 * change the cursor when the pointer enters the Busy window.  But
	 * Windows doesn't immediately change the cursor; it waits for the
	 * cursor position to change or a system call.  We need to change the
	 * cursor before the application starts processing, so set the cursor
	 * position redundantly back to the current position.
	 */
	GetCursorPos(&point);
	SetCursorPos(point.x, point.y);
    }
#endif /* WIN32 */
    XFlush(busyPtr->display);
}

static void
HideBusyWindow(Busy *busyPtr)
{
    if (busyPtr->tkBusy != NULL) {
	Tk_UnmapWindow(busyPtr->tkBusy); 
    }
#ifdef WIN32
    {
	POINT point;
	/*
	 * Under Win32, cursors aren't associated with windows.  Tk fakes this
	 * by watching Motion events on its windows.  So Tk will automatically
	 * change the cursor when the pointer enters the Busy window.  But
	 * Windows doesn't immediately change the cursor: it waits for the
	 * cursor position to change or a system call.  We need to change the
	 * cursor before the application starts processing, so set the cursor
	 * position redundantly back to the current position.
	 */
	GetCursorPos(&point);
	SetCursorPos(point.x, point.y);
    }
#endif /* WIN32 */
    XFlush(busyPtr->display);
}

/*
 *---------------------------------------------------------------------------
 *
 * BusyEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for events on the busy
 *	window itself.  We're only concerned with destroy events.
 *
 *	It might be necessary (someday) to watch resize events.  Right now, I
 *	don't think there's any point in it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When a busy window is destroyed, all internal structures associated
 *	with it released at the next idle point.
 *
 *---------------------------------------------------------------------------
 */
static void
BusyEventProc(
    ClientData clientData,		/* Busy window record */
    XEvent *eventPtr)			/* Event which triggered call to
					 * routine */
{
    Busy *busyPtr = clientData;

    if (eventPtr->type == DestroyNotify) {
	busyPtr->tkBusy = NULL;
	Tcl_EventuallyFree(busyPtr, DestroyBusy);
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * BusyCustodyProc --
 *
 *	This procedure is invoked when the busy window has been stolen by
 *	another geometry manager.  The information and memory associated with
 *	the busy window is released. I don't know why anyone would try to pack
 *	a busy window, but this should keep everything sane, if it is.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Busy structure is freed at the next idle point.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
BusyCustodyProc(
    ClientData clientData,		/* Information about the busy window. */
    Tk_Window tkwin)			/* Not used. */
{
    Busy *busyPtr = clientData;

    Tk_DeleteEventHandler(busyPtr->tkBusy, StructureNotifyMask, BusyEventProc, 
	busyPtr);
    HideBusyWindow(busyPtr);
    busyPtr->tkBusy = NULL;
    Tcl_EventuallyFree(busyPtr, DestroyBusy);
}

/*
 *---------------------------------------------------------------------------
 *
 * BusyGeometryProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for busy windows.
 *	Busy windows never request geometry, so it's unlikely that this
 *	routine will ever be called.  The routine exists simply as a place
 *	holder for the GeomProc in the Geometry Manager structure.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
BusyGeometryProc(
    ClientData clientData,		/* Information about window that got new
					 * preferred geometry.  */
    Tk_Window tkwin)			/* Other Tk-related information about
					 * the window. */
{
    /* Should never get here */
}

/*
 *---------------------------------------------------------------------------
 *
 * RefWinEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for the following events
 *	on the reference window.  If the reference and parent windows are the
 *	same, only the first event is important.
 *
 *	   1) ConfigureNotify  - The reference window has been resized or
 *				 moved.  Move and resize the busy window
 *				 to be the same size and position of the
 *				 reference window.
 *
 *	   2) DestroyNotify    - The reference window was destroyed. Destroy
 *				 the busy window and the free resources
 *				 used.
 *
 *	   3) MapNotify	       - The reference window was (re)shown. Map the
 *				 busy window again.
 *
 *	   4) UnmapNotify      - The reference window was hidden. Unmap the
 *				 busy window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the reference window gets deleted, internal structures get cleaned
 *	up.  When it gets resized, the busy window is resized accordingly. If
 *	it's displayed, the busy window is displayed. And when it's hidden, the
 *	busy window is unmapped.
 *
 *---------------------------------------------------------------------------
 */
static void
RefWinEventProc(
    ClientData clientData,		/* Busy window record */
    XEvent *eventPtr)			/* Event that triggered the call to this
					 * routine */
{
    Busy *busyPtr = clientData;

    switch (eventPtr->type) {
    case ReparentNotify:
    case DestroyNotify:

	/*
	 * Arrange for the busy structure to be removed at a proper time.
	 */
	Tcl_EventuallyFree(busyPtr, DestroyBusy);
	break;

    case ConfigureNotify:
	if ((busyPtr->width != Tk_Width(busyPtr->tkRef)) ||
	    (busyPtr->height != Tk_Height(busyPtr->tkRef)) ||
	    (busyPtr->x != Tk_X(busyPtr->tkRef)) ||
	    (busyPtr->y != Tk_Y(busyPtr->tkRef))) {
	    int x, y;

	    busyPtr->width = Tk_Width(busyPtr->tkRef);
	    busyPtr->height = Tk_Height(busyPtr->tkRef);
	    busyPtr->x = Tk_X(busyPtr->tkRef);
	    busyPtr->y = Tk_Y(busyPtr->tkRef);

	    x = y = 0;

	    if (busyPtr->tkParent != busyPtr->tkRef) {
		Tk_Window tkwin;

		for (tkwin = busyPtr->tkRef; (tkwin != NULL) &&
			 (!Tk_IsTopLevel(tkwin)); tkwin = Tk_Parent(tkwin)) {
		    if (tkwin == busyPtr->tkParent) {
			break;
		    }
		    x += Tk_X(tkwin) + Tk_Changes(tkwin)->border_width;
		    y += Tk_Y(tkwin) + Tk_Changes(tkwin)->border_width;
		}
	    }
#if BUSYDEBUG
	    PurifyPrintf("menubar2: width=%d, height=%d\n", 
		busyPtr->width, busyPtr->height);
#endif
	    if (busyPtr->tkBusy != NULL) {
#if BUSYDEBUG
		fprintf(stderr, "busy window %s is at %d,%d %dx%d\n", 
			Tk_PathName(busyPtr->tkBusy),
			x, y, busyPtr->width, busyPtr->height);
#endif
		Tk_MoveResizeWindow(busyPtr->tkBusy, x, y, busyPtr->width,
		    busyPtr->height);
		if (busyPtr->isBusy) {
		    ShowBusyWindow(busyPtr);
		}
	    }
	}
	break;

    case MapNotify:
	if ((busyPtr->tkParent != busyPtr->tkRef) && (busyPtr->isBusy)) {
	    ShowBusyWindow(busyPtr);
	}
	break;

    case UnmapNotify:
	if (busyPtr->tkParent != busyPtr->tkRef) {
	    HideBusyWindow(busyPtr);
	}
	break;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ConfigureBusy --
 *
 *	This procedure is called from the Tk event dispatcher. It releases X
 *	resources and memory used by the busy window and updates the internal
 *	hash table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory and resources are released and the Tk event handler is removed.
 *
 *---------------------------------------------------------------------------
 */
static int
ConfigureBusy(
    Tcl_Interp *interp,
    Busy *busyPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Tk_Cursor oldCursor;

    oldCursor = busyPtr->cursor;
    if (Blt_ConfigureWidgetFromObj(interp, busyPtr->tkRef, configSpecs, 
	objc, objv, (char *)busyPtr, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    if (busyPtr->cursor != oldCursor) {
	if (busyPtr->cursor == None) {
	    Tk_UndefineCursor(busyPtr->tkBusy);
	} else {
	    Tk_DefineCursor(busyPtr->tkBusy, busyPtr->cursor);
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * CreateBusy --
 *
 *	Creates a child transparent window that obscures its parent window
 *	thereby effectively blocking device events.  The size and position of
 *	the busy window is exactly that of the reference window.
 *
 *	We want to create sibling to the window to be blocked.  If the busy
 *	window is a child of the window to be blocked, Enter/Leave events can
 *	sneak through.  Futhermore under WIN32, messages of transparent
 *	windows are sent directly to the parent.  The only exception to this
 *	are toplevels, since we can't make a sibling.  Fortunately, toplevel
 *	windows rarely receive events that need blocking.
 *
 * Results:
 *	Returns a pointer to the new busy window structure.
 *
 * Side effects:
 *	When the busy window is eventually displayed, it will screen device
 *	events (in the area of the reference window) from reaching its parent
 *	window and its children.  User feed back can be achieved by changing
 *	the cursor.
 *
 *---------------------------------------------------------------------------
 */
static Busy *
CreateBusy(
    Tcl_Interp *interp,			/* Interpreter to report error to */
    Tk_Window tkRef)			/* Window hosting the busy window */
{
    Busy *busyPtr;
    int length;
    const char *fmt;
    char *name;
    Tk_Window tkBusy;
    Window parent;
    Tk_Window tkChild, tkParent;
    Tk_FakeWin *winPtr;
    int x, y;

    busyPtr = Blt_AssertCalloc(1, sizeof(Busy));
    x = y = 0;
    length = strlen(Tk_Name(tkRef));
    name = Blt_AssertMalloc(length + 6);
    if (Tk_IsTopLevel(tkRef)) {
	fmt = "_Busy";		/* Child */
	tkParent = tkRef;
    } else {
	Tk_Window tkwin;

	fmt = "%s_Busy";	/* Sibling */
	tkParent = Tk_Parent(tkRef);
	for (tkwin = tkRef; (tkwin != NULL) && (!Tk_IsTopLevel(tkwin)); 
	     tkwin = Tk_Parent(tkwin)) {
	    if (tkwin == tkParent) {
		break;
	    }
	    x += Tk_X(tkwin) + Tk_Changes(tkwin)->border_width;
	    y += Tk_Y(tkwin) + Tk_Changes(tkwin)->border_width;
	}
    }
    for (tkChild = Blt_FirstChild(tkParent); tkChild != NULL; 
	 tkChild = Blt_NextChild(tkChild)) {
	Tk_MakeWindowExist(tkChild);
    }
    sprintf_s(name, length + 6, fmt, Tk_Name(tkRef));
    tkBusy = Tk_CreateWindow(interp, tkParent, name, (char *)NULL);
    Blt_Free(name);

    if (tkBusy == NULL) {
	return NULL;
    }
    Tk_MakeWindowExist(tkRef);
    busyPtr->display = Tk_Display(tkRef);
    busyPtr->interp = interp;
    busyPtr->tkRef = tkRef;
    busyPtr->tkParent = tkParent;
    busyPtr->tkBusy = tkBusy;
    busyPtr->width = Tk_Width(tkRef);
    busyPtr->height = Tk_Height(tkRef);
    busyPtr->x = Tk_X(tkRef);
    busyPtr->y = Tk_Y(tkRef);
    busyPtr->cursor = None;
    busyPtr->isBusy = FALSE;
    Tk_SetClass(tkBusy, "Busy");
    Blt_SetWindowInstanceData(tkBusy, busyPtr);

    winPtr = (Tk_FakeWin *) tkRef;
    if (winPtr->flags & TK_REPARENTED) {
	/*
	 * This works around a bug in the implementation of menubars for
	 * non-MacIntosh window systems (Win32 and X11).  Tk doesn't reset the
	 * pointers to the parent window when the menu is reparented
	 * (winPtr->parentPtr points to the wrong window). We get around this
	 * by determining the parent via the native API calls.
	 */
#ifdef WIN32
	{
	    HWND hWnd;
	    RECT rect;

	    hWnd = Tk_GetHWND(Tk_WindowId(tkRef));
	    hWnd = GetParent(hWnd);
	    parent = (Window) hWnd;
	    if (GetWindowRect(hWnd, &rect)) {
		busyPtr->width = rect.right - rect.left;
		busyPtr->height = rect.bottom - rect.top;
#if BUSYDEBUG
		PurifyPrintf("menubar: width=%d, height=%d\n",
		    busyPtr->width, busyPtr->height);
#endif
	    }
	}
#else
	parent = Blt_GetParentWindow(busyPtr->display, Tk_WindowId(tkRef));
#endif
    } else {
	parent = Tk_WindowId(tkParent);
#ifdef WIN32
	parent = (Window) Tk_GetHWND(parent);
#endif
    }
    Blt_MakeTransparentWindowExist(tkBusy, parent, TRUE);

#if BUSYDEBUG
    PurifyPrintf("menubar1: width=%d, height=%d\n", busyPtr->width, 
	busyPtr->height);
    fprintf(stderr, "busy window %s is at %d,%d %dx%d\n", Tk_PathName(tkBusy),
	    x, y, busyPtr->width, busyPtr->height);
#endif
    Tk_MoveResizeWindow(tkBusy, x, y, busyPtr->width, busyPtr->height);

    /* Only worry if the busy window is destroyed.  */
    Tk_CreateEventHandler(tkBusy, StructureNotifyMask, BusyEventProc, busyPtr);
    /*
     * Indicate that the busy window's geometry is being managed.  This will
     * also notify us if the busy window is ever packed.
     */
    Tk_ManageGeometry(tkBusy, &busyMgrInfo, busyPtr);
    if (busyPtr->cursor != None) {
	Tk_DefineCursor(tkBusy, busyPtr->cursor);
    }
    /* Track the reference window to see if it is resized or destroyed.  */
    Tk_CreateEventHandler(tkRef, StructureNotifyMask, RefWinEventProc, busyPtr);
    return busyPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * DestroyBusy --
 *
 *	This procedure is called from the Tk event dispatcher. It releases X
 *	resources and memory used by the busy window and updates the internal
 *	hash table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory and resources are released and the Tk event handler is removed.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroyBusy(DestroyData data)		/* Busy window structure record */
{
    Busy *busyPtr = (Busy *)data;

    Blt_FreeOptions(configSpecs, (char *)busyPtr, busyPtr->display, 0);
    if (busyPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(busyPtr->tablePtr, busyPtr->hashPtr);
    }
    Tk_DeleteEventHandler(busyPtr->tkRef, StructureNotifyMask, 
	RefWinEventProc, busyPtr);
    if (busyPtr->tkBusy != NULL) {
	Tk_DeleteEventHandler(busyPtr->tkBusy, StructureNotifyMask,
	    BusyEventProc, busyPtr);
	Tk_ManageGeometry(busyPtr->tkBusy, NULL, busyPtr);
	Tk_DestroyWindow(busyPtr->tkBusy);
    }
    Blt_Free(busyPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * GetBusy --
 *
 *	Returns the busy window structure associated with the reference
 *	window, keyed by its path name.  The clientData argument is the main
 *	window of the interpreter, used to search for the reference window in
 *	its own window hierarchy.
 *
 * Results:
 *	If path name represents a reference window with a busy window, a
 *	pointer to the busy window structure is returned. Otherwise, NULL is
 *	returned and an error message is left in interp->result.
 *
 *---------------------------------------------------------------------------
 */
static int
GetBusy(
    BusyInterpData *dataPtr,		/* Interpreter-specific data. */
    Tcl_Interp *interp,			/* Interpreter to report errors to. If
					 * NULL, indicates not to generate error
					 * message. */
    Tcl_Obj *objPtr,
    Busy **busyPtrPtr)		        /* Will contain address of busy window
					 * if found. */
{
    Blt_HashEntry *hPtr;
    Tk_Window tkwin;
    const char *pathName;		/* Path name of parent window */

    pathName = Tcl_GetString(objPtr);
    tkwin = Tk_NameToWindow(dataPtr->interp, pathName, dataPtr->tkMain);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    hPtr = Blt_FindHashEntry(&dataPtr->busyTable, (char *)tkwin);
    if (hPtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find busy window \"", pathName, 
		"\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    *busyPtrPtr = Blt_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HoldBusy --
 *
 *	Creates (if necessary) and maps a busy window, thereby preventing
 *	device events from being be received by the parent window and its
 *	children.
 *
 * Results:
 *	Returns a standard TCL result. If path name represents a busy window,
 *	it is unmapped and TCL_OK is returned. Otherwise, TCL_ERROR is
 *	returned and an error message is left in interp->result.
 *
 * Side effects:
 *	The busy window is created and displayed, blocking events from the
 *	parent window and its children.
 *
 *---------------------------------------------------------------------------
 */
static int
HoldBusy(
    BusyInterpData *dataPtr,		/* Interpreter-specific data. */
    Tcl_Interp *interp,			/* Interpreter to report errors to */
    int objc,
    Tcl_Obj *const *objv)		/* Window name and option pairs */
{
    Tk_Window tkwin;
    Blt_HashEntry *hPtr;
    Busy *busyPtr;
    int isNew;
    int result;

    tkwin = Tk_NameToWindow(interp, Tcl_GetString(objv[0]), dataPtr->tkMain);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    hPtr = Blt_CreateHashEntry(&dataPtr->busyTable, (char *)tkwin, &isNew);
    if (isNew) {
	busyPtr = (Busy *)CreateBusy(interp, tkwin);
	if (busyPtr == NULL) {
	    return TCL_ERROR;
	}
	Blt_SetHashValue(hPtr, (char *)busyPtr);
	busyPtr->hashPtr = hPtr;
    } else {
	busyPtr = Blt_GetHashValue(hPtr);
    }
    busyPtr->tablePtr = &dataPtr->busyTable;
    result = ConfigureBusy(interp, busyPtr, objc - 1, objv + 1);

    /* 
     * Don't map the busy window unless the reference window is also currently
     * displayed.
     */
    if (Tk_IsMapped(busyPtr->tkRef)) {
	ShowBusyWindow(busyPtr);
    } else {
	HideBusyWindow(busyPtr);
    }
    busyPtr->isBusy = TRUE;
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * StatusOp --
 *
 *	Returns the status of the busy window; whether it's blocking events or
 *	not.
 *
 * Results:
 *	Returns a standard TCL result. If path name represents a busy window,
 *	the status is returned via interp->result and TCL_OK is
 *	returned. Otherwise, TCL_ERROR is returned and an error message is
 *	left in interp->result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StatusOp(
    ClientData clientData,		/* Interpreter-specific data. */
    Tcl_Interp *interp,			/* Interpreter to report error to */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)
{
    BusyInterpData *dataPtr = clientData;
    Busy *busyPtr;

    if (GetBusy(dataPtr, interp, objv[2], &busyPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), busyPtr->isBusy);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ForgetOp --
 *
 *	Destroys the busy window associated with the reference window and
 *	arranges for internal resources to the released when they're not being
 *	used anymore.
 *
 * Results:
 *	Returns a standard TCL result. If path name represents a busy window,
 *	it is destroyed and TCL_OK is returned. Otherwise, TCL_ERROR is
 *	returned and an error message is left in interp->result.
 *
 * Side effects:
 *	The busy window is removed.  Other related memory and resources are
 *	eventually released by the Tk dispatcher.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ForgetOp(
    ClientData clientData,		/* Interpreter-specific data. */
    Tcl_Interp *interp,			/* Not used. */
    int objc,
    Tcl_Obj *const *objv)
{
    BusyInterpData *dataPtr = clientData;
    int i;

    for (i = 2; i < objc; i++) {
	Busy *busyPtr;

	if (GetBusy(dataPtr, (Tcl_Interp *)NULL, objv[i], &busyPtr) == TCL_OK) {
	    /* Unmap the window even though it will be soon destroyed */
	    HideBusyWindow(busyPtr);
	    Tcl_EventuallyFree(busyPtr, DestroyBusy);
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ReleaseOp --
 *
 *	Unmaps the busy window, thereby permitting device events to be
 *	received by the parent window and its children.
 *
 * Results:
 *	Returns a standard TCL result. If path name represents a busy window,
 *	it is unmapped and TCL_OK is returned. Otherwise, TCL_ERROR is
 *	returned and an error message is left in interp->result.
 *
 * Side effects:
 *	The busy window is hidden, allowing the parent window and its children
 *	to receive events again.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ReleaseOp(
    ClientData clientData,		/* Interpreter-specific data. */
    Tcl_Interp *interp,			/* Not used. */
    int objc,
    Tcl_Obj *const *objv)
{
    BusyInterpData *dataPtr = clientData;
    Busy *busyPtr;
    int i;

    for (i = 2; i < objc; i++) {
	if (GetBusy(dataPtr, (Tcl_Interp *)NULL, objv[i], &busyPtr) == TCL_OK) {
	    HideBusyWindow(busyPtr);
	    busyPtr->isBusy = FALSE;
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * NamesOp --
 *
 *	Reports the names of all widgets with busy windows attached to them,
 *	matching a given pattern.  If no pattern is given, all busy widgets
 *	are listed.
 *
 * Results:
 *	Returns a TCL list of the names of the widget with busy windows
 *	attached to them, regardless if the widget is currently busy or not.
 *
 *---------------------------------------------------------------------------
 */
static int
NamesOp(
    ClientData clientData,		/* Interpreter-specific data. */
    Tcl_Interp *interp,			/* Interpreter to report errors to */
    int objc,
    Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    BusyInterpData *dataPtr = clientData;
    Tcl_Obj *listObjPtr;
    const char *pattern;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    pattern = (objc > 2) ? Tcl_GetString(objv[2]) : NULL;
    for (hPtr = Blt_FirstHashEntry(&dataPtr->busyTable, &iter);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	Busy *busyPtr;

	busyPtr = Blt_GetHashValue(hPtr);
	if ((pattern == NULL) ||
	    (Tcl_StringMatch(Tk_PathName(busyPtr->tkRef), pattern))) {
	    Tcl_Obj *objPtr;
	    objPtr = Tcl_NewStringObj(Tk_PathName(busyPtr->tkRef), -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * BusyOp --
 *
 *	Reports the names of all widgets with busy windows attached to them,
 *	matching a given pattern.  If no pattern is given, all busy widgets
 *	are listed.
 *
 * Results:
 *	Returns a TCL list of the names of the widget with busy windows
 *	attached to them, regardless if the widget is currently busy or not.
 *
 *---------------------------------------------------------------------------
 */
static int
BusyOp(
    ClientData clientData,		/* Interpreter-specific data. */
    Tcl_Interp *interp,			/* Interpreter to report errors to */
    int objc,
    Tcl_Obj *const *objv)
{
    BusyInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    Tcl_Obj *listObjPtr;
    const char *pattern;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    pattern = (objc > 2) ? Tcl_GetString(objv[2]) : NULL;
    for (hPtr = Blt_FirstHashEntry(&dataPtr->busyTable, &iter);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	Busy *busyPtr;

	busyPtr = Blt_GetHashValue(hPtr);
	if (!busyPtr->isBusy) {
	    continue;
	}
	if ((pattern == NULL) || 
	    (Tcl_StringMatch(Tk_PathName(busyPtr->tkRef), pattern))) {
	    Tcl_Obj *objPtr;
	    objPtr = Tcl_NewStringObj(Tk_PathName(busyPtr->tkRef), -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * CheckOp --
 *
 *	Checks if the named window is currently busy.  This also includes
 *	windows whose ancestors are currently busy.
 *
 * Results:
 *	Returns 1 a TCL list of the names of the widget with busy windows
 *	attached to them, regardless if the widget is currently busy or not.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CheckOp(
    ClientData clientData,		/* Interpreter-specific data. */
    Tcl_Interp *interp,			/* Interpreter to report errors to */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    BusyInterpData *dataPtr = clientData;
    Tk_Window tkwin;
    const char *pathName;

    pathName = Tcl_GetString(objv[2]);
    tkwin = Tk_NameToWindow(interp, pathName, dataPtr->tkMain);
    do {
	hPtr = Blt_FindHashEntry(&dataPtr->busyTable, (char *)tkwin);
	if (hPtr != NULL) {
	    Busy *busyPtr;

	    /* Found a busy window, is it on? */
	    busyPtr = Blt_GetHashValue(hPtr);
	    if (busyPtr->isBusy) {
		Tcl_SetBooleanObj(Tcl_GetObjResult(interp), 1);
		return TCL_OK;
	    }
	}
	tkwin = Tk_Parent(tkwin);
    } while(tkwin != NULL);
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), 0);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HoldOp --
 *
 *	Creates (if necessary) and maps a busy window, thereby preventing
 *	device events from being be received by the parent window and its
 *	children. The argument vector may contain option-value pairs of
 *	configuration options to be set.
 *
 * Results:
 *	Returns a standard TCL result.
 *
 * Side effects:
 *	The busy window is created and displayed, blocking events from the
 *	parent window and its children.
 *
 *---------------------------------------------------------------------------
 */
static int
HoldOp(
    ClientData clientData,		/* Interpreter-specific data. */
    Tcl_Interp *interp,			/* Interpreter to report errors to */
    int objc,
    Tcl_Obj *const *objv)		/* Window name and option pairs */
{
    BusyInterpData *dataPtr = clientData;
    int i;
    const char *string;

    string = Tcl_GetString(objv[1]);
    if ((string[0] == 'h') && (strcmp(string, "hold") == 0)) {
	objc--, objv++;			/* Command used "hold" keyword */
    }
    for (i = 1; i < objc; i++) {
	int count;
	/*
	 * Find the end of the option-value pairs for this window.
	 */
	for (count = i + 1; count < objc; count += 2) {
	    string = Tcl_GetString(objv[count]);
	    if (string[0] != '-') {
		break;
	    }
	}
	if (count > objc) {
	    count = objc;
	}
	if (HoldBusy(dataPtr, interp, count - i, objv + i) != TCL_OK) {
	    return TCL_ERROR;
	}
	i = count;
    }
    return TCL_OK;
}

/* ARGSUSED*/
static int
CgetOp(
    ClientData clientData,		/* Interpreter-specific data. */
    Tcl_Interp *interp,			/* Interpreter to report errors to */
    int objc,
    Tcl_Obj *const *objv)		/* Widget pathname and option switch */
{
    BusyInterpData *dataPtr = clientData;
    Busy *busyPtr;
    int result;

    if (GetBusy(dataPtr, interp, objv[2], &busyPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_Preserve(busyPtr);
    result = Blt_ConfigureValueFromObj(interp, busyPtr->tkRef, configSpecs,
	(char *)busyPtr, objv[3], 0);
    Tcl_Release(busyPtr);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 *	This procedure is called to process an objv/objc list in order to
 *	configure (or reconfigure) a busy window.
 *
 * Results:
 *	The return value is a standard TCL result.  If TCL_ERROR is returned,
 *	then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information get set for busyPtr; old resources get
 *	freed, if there were any.  The busy window destroyed and recreated in
 *	a new parent window.
 *
 *---------------------------------------------------------------------------
 */
static int
ConfigureOp(
    ClientData clientData,		/* Interpreter-specific data. */
    Tcl_Interp *interp,			/* Interpreter to report errors to */
    int objc,
    Tcl_Obj *const *objv)		/* Reference window path name and
					 * options */
{
    BusyInterpData *dataPtr = clientData;
    Busy *busyPtr;
    int result;

    if (GetBusy(dataPtr, interp, objv[2], &busyPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 3) {
	result = Blt_ConfigureInfoFromObj(interp, busyPtr->tkRef, configSpecs,
	    (char *)busyPtr, (Tcl_Obj *)NULL, 0);
    } else if (objc == 4) {
	result = Blt_ConfigureInfoFromObj(interp, busyPtr->tkRef, configSpecs,
	    (char *)busyPtr, objv[3], 0);
    } else {
	Tcl_Preserve(busyPtr);
	result = ConfigureBusy(interp, busyPtr, objc - 3, objv + 3);
	Tcl_Release(busyPtr);
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * BusyInterpDeleteProc --
 *
 *	This is called when the interpreter hosting the "busy" command is
 *	destroyed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys all the hash table managing the busy windows.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
BusyInterpDeleteProc(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp)
{
    BusyInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;

    for (hPtr = Blt_FirstHashEntry(&dataPtr->busyTable, &iter);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	Busy *busyPtr;

	busyPtr = Blt_GetHashValue(hPtr);
	busyPtr->hashPtr = NULL;
	DestroyBusy((DestroyData)busyPtr);
    }
    Blt_DeleteHashTable(&dataPtr->busyTable);
    Tcl_DeleteAssocData(interp, BUSY_THREAD_KEY);
    Blt_Free(dataPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Busy Sub-command specification:
 *
 *	- Name of the sub-command.
 *	- Minimum number of characters needed to unambiguously
 *        recognize the sub-command.
 *	- Pointer to the function to be called for the sub-command.
 *	- Minimum number of arguments accepted.
 *	- Maximum number of arguments accepted.
 *	- String to be displayed for usage (arguments only).
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec busyOps[] =
{
    {"cget", 2, CgetOp, 4, 4, "window option",},
    {"check", 1, CheckOp, 3, 3, "window",},
    {"configure", 2, ConfigureOp, 3, 0, "window ?options?...",},
    {"forget", 1, ForgetOp, 2, 0, "?window?...",},
    {"hold", 3, HoldOp, 3, 0, 
	"window ?options?... ?window options?...",},
    {"isbusy", 1, BusyOp, 2, 3, "?pattern?",},
    {"names", 1, NamesOp, 2, 3, "?pattern?",},
    {"release", 1, ReleaseOp, 2, 0, "?window?...",},
    {"status", 1, StatusOp, 3, 3, "window",},
    {"windows", 1, NamesOp, 2, 3, "?pattern?",},
};
static int nBusyOps = sizeof(busyOps) / sizeof(Blt_OpSpec);

/*
 *---------------------------------------------------------------------------
 *
 * BusyCmd --
 *
 *	This procedure is invoked to process the "busy" TCL command.  See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
static int
BusyCmd(
    ClientData clientData,		/* Interpreter-specific data. */
    Tcl_Interp *interp,			/* Interpreter associated with
					 * command */
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_ObjCmdProc *proc;
    int result;
    
    if (objc > 1) {
	const char *string;

	string = Tcl_GetString(objv[1]);
	if (string[0] == '.') {
	    return HoldOp(clientData, interp, objc, objv);
	}
    }
    proc = Blt_GetOpFromObj(interp, nBusyOps, busyOps, BLT_OP_ARG1, objc, objv,
	    0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (clientData, interp, objc, objv);
    return result;
}

static BusyInterpData *
GetBusyInterpData(Tcl_Interp *interp)
{
    BusyInterpData *dataPtr;
    Tcl_InterpDeleteProc *proc;

    dataPtr = (BusyInterpData *)
	Tcl_GetAssocData(interp, BUSY_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_AssertMalloc(sizeof(BusyInterpData));
	Tcl_SetAssocData(interp, BUSY_THREAD_KEY, BusyInterpDeleteProc, 
		dataPtr);
	Blt_InitHashTable(&dataPtr->busyTable, BLT_ONE_WORD_KEYS);
	dataPtr->interp = interp;
	dataPtr->tkMain = Tk_MainWindow(interp);
    }
    return dataPtr;
}

int
Blt_BusyCmdInitProc(Tcl_Interp *interp)
{
    static Blt_InitCmdSpec cmdSpec = {"busy", BusyCmd, };

    cmdSpec.clientData = GetBusyInterpData(interp);
    return Blt_InitCmd(interp, "::blt", &cmdSpec);
}
#endif /* NO_BUSY */

