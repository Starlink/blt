/*
 * tkIntDecls.h --
 *
 * This file contains the declarations for all unsupported functions
 * that are exported by the Tk library.  These interfaces are not
 * guaranteed to remain the same between versions.  Use at your own
 * risk.
 *
 *	Copyright 2003-2004 George A Howlett.
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
 *
 * This file was adapted from tkIntDecls.h of the Tk library distribution.
 *
 *	Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 *	See the file "license.terms" for information on usage and
 *	redistribution of this file, and for a DISCLAIMER OF ALL
 *	WARRANTIES.
 */

#ifndef _TKINTDECLS
#define _TKINTDECLS

#ifdef BUILD_tk
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT
#endif

/*
 * WARNING: This file is automatically generated by the tools/genStubs.tcl
 * script.  Any modifications to the function declarations below should be made
 * in the generic/tkInt.decls script.
 */

/* !BEGIN!: Do not edit below this line. */

/*
 * Exported function declarations:
 */

/* 9 */
extern void TkComputeAnchor(Tk_Anchor anchor, Tk_Window tkwin, int xPad, 
	int yPad, int innerWidth, int innerHeight, int *xPtr, int *yPtr);
/* 10 */
extern int TkCopyAndGlobalEval(Tcl_Interp *interp, char * script);
/* 14 */
extern Tk_Window TkCreateMainWindow(Tcl_Interp *interp, char * screenName, 
	char *baseName);
/* 64 */
extern void TkpMakeContainer(Tk_Window tkwin);
/* 74 */
extern void TkpSetMainMenubar(Tcl_Interp *interp, Tk_Window tkwin, 
	char *menuName);
/* 75 */
extern int TkpUseWindow(Tcl_Interp *interp, Tk_Window tkwin, char *string);
/* 84 */
extern void TkSetClassProcs(Tk_Window tkwin, void *procs, 
	ClientData instanceData);
/* 85 */
extern void TkSetWindowMenuBar(Tcl_Interp *interp, Tk_Window tkwin, 
	char *oldMenuName, char *menuName);
/* 95 */
extern void TkWmRestackToplevel(Tk_Window tkwin, int aboveBelow, 
	Tk_Window otherPtr);

typedef struct TkIntStubs {
    int magic;
    struct TkIntStubHooks *hooks;

    void *tkAllocWindow;	/* 0 */
    void *tkBezierPoints;	/* 1 */
    void *tkBezierScreenPoints; /* 2 */
    void *tkBindDeadWindow;	/* 3 */
    void *tkBindEventProc;	/* 4 */
    void *tkBindFree;		/* 5 */
    void *tkBindInit;		/* 6 */
    void *tkChangeEventWindow; /* 7 */
    void *tkClipInit;		/* 8 */

    void (*tkComputeAnchor)(Tk_Anchor anchor, Tk_Window tkwin, 
	int xPad, int yPad, int innerWidth, int innerHeight, 
	int *xPtr, int *yPtr);	/* 9 */

    int (*tkCopyAndGlobalEval)(Tcl_Interp *interp, char *script); /* 10 */

    void *tkCreateBindingProcedure; /* 11 */
    void *tkCreateCursorFromData; /* 12 */
    void *tkCreateFrame;	/* 13 */

    Tk_Window (*tkCreateMainWindow)(Tcl_Interp *interp, char *screenName, 
	char * baseName);	/* 14 */

    void *tkCurrentTime;	/* 15 */
    void *tkDeleteAllImages;	/* 16 */
    void *tkDoConfigureNotify;	/* 17 */
    void *tkDrawInsetFocusHighlight; /* 18 */
    void *tkEventDeadWindow;	/* 19 */
    void *tkFillPolygon;	/* 20 */
    void *tkFindStateNum;	/* 21 */
    void *tkFindStateString;	/* 22 */
    void *tkFocusDeadWindow;	/* 23 */
    void *tkFocusFilterEvent;	/* 24 */
    void *tkFocusKeyEvent;	/* 25 */
    void *tkFontPkgInit;	/* 26 */
    void *tkFontPkgFree;	/* 27 */
    void *tkFreeBindingTags;	/* 28 */
    void *tkpFreeCursor;	/* 29 */
    void *tkGetBitmapData;	/* 30 */
    void *tkGetButtPoints;	/* 31 */
    void *tkGetCursorByName;	/* 32 */
    void *tkGetDefaultScreenName; /* 33 */
    void *tkGetDisplay;		/* 34 */
    void *tkGetDisplayOf;	/* 35 */
    void *tkGetFocusWin;	/* 36 */
    void *tkGetInterpNames;	/* 37 */
    void *tkGetMiterPoints;	/* 38 */
    void *tkGetPointerCoords;	/* 39 */
    void *tkGetServerInfo;	/* 40 */
    void *tkGrabDeadWindow;	/* 41 */
    void *tkGrabState;		/* 42 */
    void *tkIncludePoint;	/* 43 */
    void *tkInOutEvents;	/* 44 */
    void *tkInstallFrameMenu;	/* 45 */
    void *tkKeysymToString;	/* 46 */
    void *tkLineToArea;		/* 47 */
    void *tkLineToPoint;	/* 48 */
    void *tkMakeBezierCurve;	/* 49 */
    void *tkMakeBezierPostscript; /* 50 */
    void *tkOptionClassChanged; /* 51 */
    void *tkOptionDeadWindow;	/* 52 */
    void *tkOvalToArea;		/* 53 */
    void *tkOvalToPoint;	/* 54 */
    void *tkpChangeFocus;	/* 55 */
    void *tkpCloseDisplay;	/* 56 */
    void *tkpClaimFocus;	/* 57 */
    void *tkpDisplayWarning;	/* 58 */
    void *tkpGetAppName;	/* 59 */
    void *tkpGetOtherWindow;	/* 60 */
    void *tkpGetWrapperWindow;	/* 61 */
    void *tkpInit;		/* 62 */
    void *tkpInitializeMenuBindings; /* 63 */

    void (*tkpMakeContainer)(Tk_Window tkwin); /* 64 */

    void *tkpMakeMenuWindow;	/* 65 */
    void *tkpMakeWindow;	/* 66 */
    void *tkpMenuNotifyToplevelCreate; /* 67 */
    void *tkpOpenDisplay;	/* 68 */
    void *tkPointerEvent;	/* 69 */
    void *tkPolygonToArea;	/* 70 */
    void *tkPolygonToPoint;	/* 71 */
    void *tkPositionInTree;	/* 72 */
    void *tkpRedirectKeyEvent;	/* 73 */

    void (*tkpSetMainMenubar)(Tcl_Interp *interp, Tk_Window tkwin, 
	char *menuName);	/* 74 */

    int (*tkpUseWindow)(Tcl_Interp *interp, Tk_Window tkwin, 
	char *string);		/* 75 */

    void *tkpWindowWasRecentlyDeleted; /* 76 */
    void *tkQueueEventForAllChildren; /* 77 */
    void *tkReadBitmapFile;	/* 78 */
    void *tkScrollWindow;	/* 79 */
    void *tkSelDeadWindow;	/* 80 */
    void *tkSelEventProc;	/* 81 */
    void *tkSelInit;		/* 82 */
    void *tkSelPropProc;	/* 83 */

    void (*tkSetClassProcs)(Tk_Window tkwin, void *procs, 
	ClientData instanceData); /* 84 */

    void (*tkSetWindowMenuBar)(Tcl_Interp *interp, Tk_Window tkwin, 
	char *oldMenuName, char *menuName); /* 85 */

    void *tkStringToKeysym;	/* 86 */
    void *tkThickPolyLineToArea; /* 87 */
    void *tkWmAddToColormapWindows; /* 88 */
    void *tkWmDeadWindow;	/* 89 */
    void *tkWmFocusToplevel;	/* 90 */
    void *tkWmMapWindow;	/* 91 */
    void *tkWmNewWindow;	/* 92 */
    void *tkWmProtocolEventProc; /* 93 */
    void *tkWmRemoveFromColormapWindows; /* 94 */

    void (*tkWmRestackToplevel)(Tk_Window tkwin, int aboveBelow, 
	Tk_Window other); /* 95 */

    void *tkWmSetClass;		/* 96 */
    void *tkWmUnmapWindow;	/* 97 */
    void *tkDebugBitmap;	/* 98 */
    void *tkDebugBorder;	/* 99 */
    void *tkDebugCursor;	/* 100 */
    void *tkDebugColor;		/* 101 */
    void *tkDebugConfig;	/* 102 */
    void *tkDebugFont;		/* 103 */
    void *tkFindStateNumObj;	/* 104 */
    void *tkGetBitmapPredefTable; /* 105 */
    void *tkGetDisplayList;	/* 106 */
    void *tkGetMainInfoList;	/* 107 */
    void *tkGetWindowFromObj;	/* 108 */
    void *tkpGetString;		/* 109 */
    void *tkpGetSubFonts;	/* 110 */
    void *tkpGetSystemDefault;	/* 111 */
    void *tkpMenuThreadInit;	/* 112 */
    void *tkClipBox;		/* 113 */
    void *tkCreateRegion;	/* 114 */
    void *tkDestroyRegion;	/* 115 */
    void *tkIntersectRegion;	/* 116 */
    void *tkRectInRegion;	/* 117 */
    void *tkSetRegion;		/* 118 */
    void *tkUnionRectWithRegion; /* 119 */
    void *reserved120;
    void *tkpCreateNativeBitmap; /* 121 */
    void *tkpDefineNativeBitmaps; /* 122 */
    void *reserved123;
    void *tkpGetNativeAppBitmap; /* 124 */
    void *reserved125;
    void *reserved126;
    void *reserved127;
    void *reserved128;
    void *reserved129;
    void *reserved130;
    void *reserved131;
    void *reserved132;
    void *reserved133;
    void *reserved134;
    void *tkpDrawHighlightBorder; /* 135 */
    void *tkSetFocusWin;	/* 136 */
    void *tkpSetKeycodeAndState; /* 137 */
    void *tkpGetKeySym;		/* 138 */
    void *tkpInitKeymapInfo;	/* 139 */
} TkIntStubs;

#ifdef __cplusplus
extern "C" {
#endif
extern TkIntStubs *tkIntStubsPtr;
#ifdef __cplusplus
}
#endif

#if defined(USE_TK_STUBS) && !defined(USE_TK_STUB_PROCS)

/*
 * Inline function declarations:
 */

#ifndef TkComputeAnchor
#define TkComputeAnchor \
	(tkIntStubsPtr->tkComputeAnchor) /* 9 */
#endif

#ifndef TkCopyAndGlobalEval
#define TkCopyAndGlobalEval \
	(tkIntStubsPtr->tkCopyAndGlobalEval) /* 10 */
#endif

#ifndef TkCreateMainWindow
#define TkCreateMainWindow \
	(tkIntStubsPtr->tkCreateMainWindow) /* 14 */
#endif

#ifndef TkpMakeContainer
#define TkpMakeContainer \
	(tkIntStubsPtr->tkpMakeContainer) /* 64 */
#endif

#ifndef TkpSetMainMenubar
#define TkpSetMainMenubar \
	(tkIntStubsPtr->tkpSetMainMenubar) /* 74 */
#endif

#ifndef TkpUseWindow
#define TkpUseWindow \
	(tkIntStubsPtr->tkpUseWindow) /* 75 */
#endif

#ifndef TkSetClassProcs
#define TkSetClassProcs \
	(tkIntStubsPtr->tkSetClassProcs) /* 84 */
#endif

#ifndef TkSetWindowMenuBar
#define TkSetWindowMenuBar \
	(tkIntStubsPtr->tkSetWindowMenuBar) /* 85 */
#endif

#ifndef TkWmRestackToplevel
#define TkWmRestackToplevel \
        (tkIntStubsPtr->tkWmRestackToplevel) /* 95 */
#endif

#endif

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _TKINTDECLS */
