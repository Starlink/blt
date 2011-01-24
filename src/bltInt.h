
/*
 * bltInt.h --
 *
 *	Copyright 1993-2004 George A Howlett.
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

#ifndef _BLT_INT_H
#define _BLT_INT_H

#undef SIZEOF_LONG
#include "config.h"
#include "blt.h"

#undef  BLT_STORAGE_CLASS
#define BLT_STORAGE_CLASS	DLLEXPORT

#define _VERSION(a,b,c)	    (((a) << 16) + ((b) << 8) + (c))

#define _TCL_VERSION _VERSION(TCL_MAJOR_VERSION, TCL_MINOR_VERSION, TCL_RELEASE_SERIAL)
#define _TK_VERSION _VERSION(TK_MAJOR_VERSION, TK_MINOR_VERSION, TK_RELEASE_SERIAL)

#ifndef __WIN32__
#   if defined(_WIN32) || defined(WIN32) || defined(__MINGW32__) || defined(__BORLANDC__)
#	define __WIN32__
#	ifndef WIN32
#	    define WIN32
#	endif
#   endif
#endif

#ifdef _MSC_VER
#  define _CRT_SECURE_NO_DEPRECATE
#  define _CRT_NONSTDC_NO_DEPRECATE
#endif
#ifdef WIN32
#  define STRICT
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  undef STRICT
#  undef WIN32_LEAN_AND_MEAN
#  include <windowsx.h>
#endif /* WIN32 */

#include <tcl.h>

#ifdef USE_TCL_STUBS
#  include "tclIntDecls.h"
#  ifdef WIN32
#    include "tclIntPlatDecls.h"
#  endif
#endif	/* USE_TCL_STUBS */

#define USE_COMPOSITELESS_PHOTO_PUT_BLOCK 
#include <tk.h>

#ifdef USE_TK_STUBS
#  include "tkIntDecls.h"
#  ifdef WIN32
#    include "tkPlatDecls.h"
#    include "tkIntXlibDecls.h"
#  endif
#endif	/* USE_TK_STUBS */

#include <stdio.h>

#ifdef WIN32 
#  ifndef __GNUC__
#    ifdef O_NONBLOCK
#      define O_NONBLOCK	1
#    endif
#  endif /* __GNUC__ */

#endif /* WIN32 */

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_STDDEF_H
#  include <stddef.h>
#endif /* HAVE_STDDEF_H */

#ifdef HAVE_ERRNO_H
#  include <errno.h>
#endif /* HAVE_ERRNO_H */

#ifdef HAVE_CTYPE_H
#  include <ctype.h>
#endif /* HAVE_CTYPE_H */

#ifdef HAVE_MEMORY_H
#  include <memory.h>
#endif /* HAVE_MEMORY_H */

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif

#include "bltMath.h"
#include "bltString.h"

#undef INLINE

#ifdef __GNUC__
#  define INLINE __inline__
#else
#  define INLINE
#endif

#if defined(__GNUC__) && defined(HAVE_X86) && defined(__OPTIMIZE__)
#  define HAVE_X86_ASM
#endif

#undef VARARGS
#ifdef __cplusplus
#  define ANYARGS (...)
#  define VARARGS(first)  (first, ...)
#  define VARARGS2(first, second)  (first, second, ...)
#else
#  define ANYARGS ()
#  define VARARGS(first) ()
#  define VARARGS2(first, second) ()
#endif /* __cplusplus */

#undef MIN
#define MIN(a,b)	(((a)<(b))?(a):(b))

#undef MAX
#define MAX(a,b)	(((a)>(b))?(a):(b))

#undef MIN3
#define MIN3(a,b,c)	(((a)<(b))?(((a)<(c))?(a):(c)):(((b)<(c))?(b):(c)))

#undef MAX3
#define MAX3(a,b,c)	(((a)>(b))?(((a)>(c))?(a):(c)):(((b)>(c))?(b):(c)))

#define TRUE 	1
#define FALSE 	0

/*
 * The macro below is used to modify a "char" value (e.g. by casting
 * it to an unsigned character) so that it can be used safely with
 * macros such as isspace.
 */
#define UCHAR(c) ((unsigned char) (c))

#define BLT_ANCHOR_TOP		(TK_ANCHOR_CENTER+1)
#define BLT_ANCHOR_BOTTOM	(TK_ANCHOR_CENTER+2)
#define BLT_ANCHOR_LEFT		(TK_ANCHOR_CENTER+3)
#define BLT_ANCHOR_RIGHT	(TK_ANCHOR_CENTER+4)

/*
 *---------------------------------------------------------------------------
 *
 * Blt_InitCmdSpec --
 *
 *---------------------------------------------------------------------------
 */
typedef struct {
    const char *name;			/* Name of command */
    Tcl_ObjCmdProc *cmdProc;
    Tcl_CmdDeleteProc *cmdDeleteProc;
    ClientData clientData;
} Blt_InitCmdSpec;

BLT_EXTERN int Blt_DictionaryCompare (const char *s1, const char *s2);

BLT_EXTERN void Blt_Draw3DRectangle(Tk_Window tkwin, Drawable drawable,
	Tk_3DBorder border, int x, int y, int width, int height, 
	int borderWidth, int relief);
BLT_EXTERN void Blt_Fill3DRectangle(Tk_Window tkwin, Drawable drawable,
	Tk_3DBorder border, int x, int y, int width, int height, 
	int borderWidth, int relief);

/* ---------------------------------------------------------------- */

#define PIXELS_NNEG		0
#define PIXELS_POS		1
#define PIXELS_ANY		2

#define COUNT_NNEG		0
#define COUNT_POS		1
#define COUNT_ANY		2

#define BLT_SCROLL_MODE_CANVAS	(1<<0)
#define BLT_SCROLL_MODE_LISTBOX	(1<<1)
#define BLT_SCROLL_MODE_HIERBOX	(1<<2)

#define RGB_ANTIQUEWHITE1	"#ffefdb"
#define RGB_BISQUE1		"#ffe4c4"
#define RGB_BISQUE2		"#eed5b7"
#define RGB_BISQUE3		"#cdb79e"
#define RGB_BLACK		"#000000"
#define RGB_BLUE		"#0000ff"
#define RGB_GREEN		"#00ff00"
#define RGB_GREY		"#b0b0b0"
#define RGB_GREY15		"#262626"
#define RGB_GREY20		"#333333"
#define RGB_GREY25		"#404040"
#define RGB_GREY30		"#4d4d4d"
#define RGB_GREY35		"#595959"
#define RGB_GREY40		"#666666"
#define RGB_GREY50		"#7f7f7f"
#define RGB_GREY64		"#a3a3a3"
#define RGB_GREY70		"#b3b3b3"
#define RGB_GREY75		"#bfbfbf"
#define RGB_GREY77		"#c3c3c3"
#define RGB_GREY82		"#d1d1d1"
#define RGB_GREY85		"#d9d9d9"
#define RGB_GREY90		"#e5e5e5"
#define RGB_GREY93		"#ececec"
#define RGB_GREY95		"#f2f2f2"
#define RGB_GREY97		"#f7f7f7"
#define RGB_LIGHTBLUE0		"#e4f7ff"
#define RGB_LIGHTBLUE00		"#D9F5FF"
#define RGB_LIGHTBLUE1		"#bfefff"
#define RGB_LIGHTBLUE2		"#b2dfee"
#define RGB_LIGHTSKYBLUE1	"#b0e2ff"
#define RGB_MAROON		"#b03060"
#define RGB_NAVYBLUE		"#000080"
#define RGB_PINK		"#ffc0cb"
#define RGB_BISQUE1		"#ffe4c4"
#define RGB_RED			"#ff0000"
#define RGB_RED3		"#cd0000"
#define RGB_WHITE		"#ffffff"
#define RGB_YELLOW		"#ffff00"
#define RGB_SKYBLUE4		"#4a708b"

#ifdef OLD_TK_COLORS
#define STD_NORMAL_BACKGROUND	RGB_BISQUE1
#define STD_ACTIVE_BACKGROUND	RGB_BISQUE2
#define STD_SELECT_BACKGROUND	RGB_LIGHTBLUE1
#define STD_SELECT_FOREGROUND	RGB_BLACK
#else
#define STD_NORMAL_BACKGROUND	RGB_GREY85
#define STD_ACTIVE_BACKGROUND	RGB_GREY90
#define STD_SELECT_BACKGROUND	RGB_SKYBLUE4
#define STD_SELECT_FOREGROUND	RGB_WHITE
#endif /* OLD_TK_COLORS */

#define STD_ACTIVE_BG_MONO	RGB_BLACK
#define STD_ACTIVE_FOREGROUND	RGB_BLACK
#define STD_ACTIVE_FG_MONO	RGB_WHITE
#define STD_BORDERWIDTH 	"2"
#define STD_FONT_HUGE		"{Sans Serif} 18"
#define STD_FONT_LARGE		"{Sans Serif} 14"
#define STD_FONT_MEDIUM		"{Sans Serif} 11"
#define STD_FONT_NORMAL		"{Sans Serif} 9"
#define STD_FONT_SMALL		"{Sans Serif} 8"
#define	STD_FONT_NUMBERS	"Math 8"
#define STD_FONT		STD_FONT_NORMAL
#define STD_INDICATOR_COLOR	RGB_RED3
#define STD_NORMAL_BG_MONO	RGB_WHITE
#define STD_NORMAL_FOREGROUND	RGB_BLACK
#define STD_NORMAL_FG_MONO	RGB_BLACK
#define STD_SELECT_BG_MONO	RGB_BLACK
#define STD_SELECT_BORDERWIDTH	"2"
#define STD_SELECT_FG_MONO	RGB_WHITE
#define STD_SHADOW_MONO		RGB_BLACK
#define STD_SELECT_FONT_HUGE	"{Sans Serif} 18 Bold"
#define STD_SELECT_FONT_LARGE	"{Sans Serif} 14 Bold"
#define STD_SELECT_FONT_MEDIUM	"{Sans Serif} 11 Bold"
#define STD_SELECT_FONT_NORMAL	"{Sans Serif} 9 Bold"
#define STD_SELECT_FONT_SMALL	"{Sans Serif} 8 Bold"
#define STD_SELECT_FONT		STD_SELECT_FONT_NORMAL
#define STD_DISABLED_FOREGROUND	RGB_GREY70
#define STD_DISABLED_BACKGROUND	RGB_GREY90

#define LineWidth(w)	(((w) > 1) ? (w) : 0)

typedef char *Blt_Uid;

BLT_EXTERN Blt_Uid Blt_GetUid(const char *string);
BLT_EXTERN void Blt_FreeUid(Blt_Uid uid);
BLT_EXTERN Blt_Uid Blt_FindUid(const char *string);

#ifdef TCL_UTF_MAX
#  define HAVE_UTF	1
#else
#  define HAVE_UTF	0
#endif /* TCL_UTF_MAX */

#include <bltAlloc.h>

typedef char *DestroyData;

#ifndef TK_RELIEF_SOLID
#  define TK_RELIEF_SOLID		TK_RELIEF_FLAT
#endif


typedef int (QSortCompareProc) (const void *, const void *);

/*
 *---------------------------------------------------------------------------
 *
 * Point2 --
 *
 *	2-D coordinate.
 *
 *---------------------------------------------------------------------------
 */
typedef struct {
    double x, y;
} Point2d;

typedef struct {
    float x, y;
} Point2f;

typedef struct {
    int x, y;
} Point2i;

typedef struct {
    size_t nValues; 
    void *values;
} Array;

/*
 *---------------------------------------------------------------------------
 *
 * Point3D --
 *
 *	3-D coordinate.
 *
 *---------------------------------------------------------------------------
 */
typedef struct {
    double x, y, z;
} Point3D;

/*
 *---------------------------------------------------------------------------
 *
 * Segment2 --
 *
 *	2-D line segment.
 *
 *---------------------------------------------------------------------------
 */
typedef struct {
    Point2f p, q;		/* The two end points of the segment. */
} Segment2f;

typedef struct {
    Point2d p, q;		/* The two end points of the segment. */
} Segment2d;

/*
 *---------------------------------------------------------------------------
 *
 * Dim2D --
 *
 *	2-D dimension.
 *
 *---------------------------------------------------------------------------
 */
typedef struct {
    short int width, height;
} Dim2D;

/*
 *---------------------------------------------------------------------------
 *
 * Region2d --
 *
 *      2-D region.  Used to copy parts of images.
 *
 *---------------------------------------------------------------------------
 */
typedef struct {
    float left, right, top, bottom;
} Region2f;

typedef struct {
    double left, right, top, bottom;
} Region2d;

typedef struct {
    double left, right, top, bottom, front, back;
} Region3D;

#define RegionWidth(r)		((r)->right - (r)->left + 1)
#define RegionHeight(r)		((r)->bottom - (r)->top + 1)

#define PointInRegion(e,x,y) \
	(((x) <= (e)->right) && ((x) >= (e)->left) && \
	 ((y) <= (e)->bottom) && ((y) >= (e)->top))

#define PointInRectangle(r,x0,y0) \
	(((x0) <= (int)((r)->x + (r)->width - 1)) && ((x0) >= (int)(r)->x) && \
	 ((y0) <= (int)((r)->y + (r)->height - 1)) && ((y0) >= (int)(r)->y))

/*-------------------------------------------------------------------------------
 *
 * ColorPair --
 *
 *	Holds a pair of foreground, background colors.
 *
 *---------------------------------------------------------------------------
 */
typedef struct {
    XColor *fgColor, *bgColor;
} ColorPair;

#define COLOR_NONE		(XColor *)0
#define COLOR_DEFAULT		(XColor *)1
#define COLOR_ALLOW_DEFAULTS	1

BLT_EXTERN int Blt_GetColorPair (Tcl_Interp *interp, Tk_Window tkwin, 
	char *fgColor, char *bgColor, ColorPair *pairPtr, int colorFlag);

BLT_EXTERN void Blt_FreeColorPair (ColorPair *pairPtr);


#define ARROW_LEFT		(0)
#define ARROW_UP		(1)
#define ARROW_RIGHT		(2)
#define ARROW_DOWN		(3)
#define ARROW_OFFSET		4
#define STD_ARROW_HEIGHT	3
#define STD_ARROW_WIDTH		((2 * (ARROW_OFFSET - 1)) + 1)

/*
 *---------------------------------------------------------------------------
 *
 * 	X11/Xosdefs.h requires XNOSTDHDRS be set for some systems.  This is a
 * 	guess.  If I can't find STDC headers or unistd.h, assume that this is
 * 	non-POSIX and non-STDC environment.  (needed for Encore Umax 3.4 ?)
 *
 *---------------------------------------------------------------------------
 */
#if !defined(STDC_HEADERS) && !defined(HAVE_UNISTD_H)
#  define XNOSTDHDRS 	1
#endif

BLT_EXTERN FILE *Blt_OpenFile(Tcl_Interp *interp, const char *fileName, 
	const char *mode);

BLT_EXTERN int Blt_ExprDoubleFromObj (Tcl_Interp *interp, Tcl_Obj *objPtr, 
	double *valuePtr);

BLT_EXTERN int Blt_ExprIntFromObj (Tcl_Interp *interp, Tcl_Obj *objPtr, 
	int *valuePtr);

BLT_EXTERN const char *Blt_Itoa(int value);

BLT_EXTERN const char *Blt_Ltoa(long value);

BLT_EXTERN const char *Blt_Utoa(unsigned int value);

BLT_EXTERN const char *Blt_Dtoa(Tcl_Interp *interp, double value);

BLT_EXTERN unsigned char *Blt_Base64_Decode(Tcl_Interp *interp, 
	const char *string, size_t *lengthPtr);

BLT_EXTERN char *Blt_Base64_Encode(Tcl_Interp *interp, 
	const unsigned char *buffer, size_t bufsize);

BLT_EXTERN int Blt_IsBase64(const unsigned char *buf, size_t length);

BLT_EXTERN int Blt_InitCmd (Tcl_Interp *interp, const char *namespace, 
	Blt_InitCmdSpec *specPtr);

BLT_EXTERN int Blt_InitCmds (Tcl_Interp *interp, const char *namespace, 
	Blt_InitCmdSpec *specPtr, int nCmds);

BLT_EXTERN int Blt_GlobalEvalObjv(Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv);
BLT_EXTERN int Blt_GlobalEvalListObj(Tcl_Interp *interp, Tcl_Obj *cmdObjPtr);

BLT_EXTERN int Blt_GetDoubleFromString(Tcl_Interp *interp, const char *s, 
	double *valuePtr);
BLT_EXTERN int Blt_GetDoubleFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, 
	double *valuePtr);

#ifdef WIN32

typedef struct {
    DWORD pid;
    HANDLE hProcess;
} ProcessId;

#else
typedef pid_t ProcessId;
#endif

BLT_EXTERN int Blt_CreatePipeline(Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv, ProcessId **pidArrayPtr, int *stdinPipePtr,
	int *stdoutPipePtr, int *stderrPipePtr);

BLT_EXTERN void Blt_GetLineExtents(size_t nPoints, Point2d *points, 
	Region2d *r);

BLT_EXTERN int Blt_LineRectClip(Region2d *regionPtr, Point2d *p, Point2d *q);

#if (_TCL_VERSION < _VERSION(8,1,0))
BLT_EXTERN const char *Tcl_GetString (Tcl_Obj *objPtr);

BLT_EXTERN int Tcl_EvalObjv (Tcl_Interp *interp, int objc, Tcl_Obj **objv, 
	int flags);

BLT_EXTERN int Tcl_WriteObj (Tcl_Channel channel, Tcl_Obj *objPtr);

BLT_EXTERN char *Tcl_SetVar2Ex (Tcl_Interp *interp, const char *part1, 
	const char *part2, Tcl_Obj *objPtr, int flags);

BLT_EXTERN Tcl_Obj *Tcl_GetVar2Ex (Tcl_Interp *interp, const char *part1, 
	const char *part2, int flags);
#endif /* _TCL_VERSION < 8.1.0 */

BLT_EXTERN int Blt_NaturalSpline (Point2d *origPts, int nOrigPts, Point2d *intpPts,
	int nIntpPts);

BLT_EXTERN int Blt_QuadraticSpline (Point2d *origPts, int nOrigPts, 
	Point2d *intpPts, int nIntpPts);

BLT_EXTERN int Blt_SimplifyLine (Point2d *origPts, int low, int high, 
	double tolerance, int *indices);

BLT_EXTERN int Blt_NaturalParametricSpline (Point2d *origPts, int nOrigPts, 
	Region2d *extsPtr, int isClosed, Point2d *intpPts, int nIntpPts);

BLT_EXTERN int Blt_CatromParametricSpline (Point2d *origPts, int nOrigPts, 
	Point2d *intpPts, int nIntpPts);

BLT_EXTERN int Blt_StringToFlag (ClientData clientData, Tcl_Interp *interp, 
	Tk_Window tkwin, char *string, char *widgRec, int flags);

BLT_EXTERN char *Blt_FlagToString (ClientData clientData, Tk_Window tkwin, 
	char *string, int offset, Tcl_FreeProc **freeProc);

BLT_EXTERN void Blt_InitHexTable (unsigned char *table);

BLT_EXTERN GC Blt_GetPrivateGC(Tk_Window tkwin, unsigned long gcMask,
	XGCValues *valuePtr);

BLT_EXTERN GC Blt_GetPrivateGCFromDrawable(Display *display, Drawable drawable, 
	unsigned long gcMask, XGCValues *valuePtr);

BLT_EXTERN void Blt_FreePrivateGC(Display *display, GC gc);

BLT_EXTERN int Blt_GetWindowFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, 
	Window *windowPtr);

BLT_EXTERN Window Blt_GetParentWindow(Display *display, Window window);

BLT_EXTERN Tk_Window Blt_FindChild(Tk_Window parent, char *name);

BLT_EXTERN Tk_Window Blt_FirstChild(Tk_Window parent);

BLT_EXTERN Tk_Window Blt_NextChild(Tk_Window tkwin);

BLT_EXTERN void Blt_RelinkWindow (Tk_Window tkwin, Tk_Window newParent, int x, 
	int y);

BLT_EXTERN Tk_Window Blt_Toplevel(Tk_Window tkwin);

BLT_EXTERN Tk_Window Blt_GetToplevelWindow(Tk_Window tkwin);

BLT_EXTERN int Blt_GetPixels(Tcl_Interp *interp, Tk_Window tkwin, 
	const char *string, int check, int *valuePtr);

BLT_EXTERN int Blt_GetPosition(Tcl_Interp *interp, const char *string, 
	long *valuePtr);

BLT_EXTERN int Blt_GetLong(Tcl_Interp *interp, const char *string, 
	long *valuePtr);

BLT_EXTERN int Blt_GetCount(Tcl_Interp *interp, const char *string, int check, 
	long *valuePtr);

BLT_EXTERN int Blt_GetCountFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, 
	int check, long *valuePtr);

BLT_EXTERN const char *Blt_NameOfFill(int fill);

BLT_EXTERN const char *Blt_NameOfResize(int resize);

BLT_EXTERN int Blt_GetXY (Tcl_Interp *interp, Tk_Window tkwin, 
	const char *string, int *xPtr, int *yPtr);

BLT_EXTERN Point2d Blt_GetProjection (int x, int y, Point2d *p, Point2d *q);

BLT_EXTERN void Blt_DrawArrowOld(Display *display, Drawable drawable, GC gc, 
	int x, int y, int w, int h, int borderWidth, int orientation);

BLT_EXTERN void Blt_DrawArrow (Display *display, Drawable drawable, XColor *color, 
	int x, int y, int w, int h, int borderWidth, int orientation);

BLT_EXTERN int Blt_OldConfigModified TCL_VARARGS(Tk_ConfigSpec *, specs);

BLT_EXTERN void Blt_DStringAppendElements TCL_VARARGS(Tcl_DString *, args);

BLT_EXTERN void Blt_MakeTransparentWindowExist (Tk_Window tkwin, Window parent, 
	int isBusy);

BLT_EXTERN Window Blt_GetParent (Display *display, Window tkwin);

BLT_EXTERN void Blt_GetBoundingBox (int width, int height, float angle, 
	double *widthPtr, double *heightPtr, Point2d *points);

BLT_EXTERN void Blt_InitEpsCanvasItem (Tcl_Interp *interp);

BLT_EXTERN void Blt_TranslateAnchor (int x, int y, int width, int height, 
	Tk_Anchor anchor, int *transXPtr, int *transYPtr);

BLT_EXTERN Point2d Blt_AnchorPoint (double x, double y, double width, 
	double height, Tk_Anchor anchor);

BLT_EXTERN void Blt_HSV (XColor *colorPtr, double *huePtr, double *valPtr, 
	double *satPtr);

BLT_EXTERN void Blt_RGB (double hue, double sat, double val, XColor *colorPtr);

BLT_EXTERN int Blt_ParseFlag (ClientData, Tcl_Interp *, Tk_Window, char *, 
	char *, int);
BLT_EXTERN char *Blt_FlagPrint (ClientData, Tk_Window, char *, int, 
	Tcl_FreeProc **);

BLT_EXTERN long Blt_MaxRequestSize (Display *display, size_t elemSize);

BLT_EXTERN Window Blt_GetWindowId (Tk_Window tkwin);

BLT_EXTERN void Blt_InitXRandrConfig(Tcl_Interp *interp);
BLT_EXTERN void Blt_SizeOfScreen(Tk_Window tkwin, int *widthPtr,int *heightPtr);

BLT_EXTERN int Blt_RootX (Tk_Window tkwin);

BLT_EXTERN int Blt_RootY (Tk_Window tkwin);

BLT_EXTERN void Blt_RootCoordinates (Tk_Window tkwin, int x, int y, 
	int *rootXPtr, int *rootYPtr);

BLT_EXTERN void Blt_MapToplevelWindow(Tk_Window tkwin);

BLT_EXTERN void Blt_UnmapToplevelWindow(Tk_Window tkwin);

BLT_EXTERN void Blt_RaiseToplevelWindow(Tk_Window tkwin);

BLT_EXTERN void Blt_LowerToplevelWindow(Tk_Window tkwin);

BLT_EXTERN void Blt_ResizeToplevelWindow(Tk_Window tkwin, int w, int h);

BLT_EXTERN void Blt_MoveToplevelWindow(Tk_Window tkwin, int x, int y);

BLT_EXTERN void Blt_MoveResizeToplevelWindow(Tk_Window tkwin, int x, int y, 
	int w, int h);

BLT_EXTERN int Blt_GetWindowRegion(Display *display, Window window, int *xPtr,
	int *yPtr, int *widthPtr, int *heightPtr);

BLT_EXTERN ClientData Blt_GetWindowInstanceData (Tk_Window tkwin);

BLT_EXTERN void Blt_SetWindowInstanceData (Tk_Window tkwin, 
	ClientData instanceData);

BLT_EXTERN void Blt_DeleteWindowInstanceData (Tk_Window tkwin);

BLT_EXTERN int Blt_AdjustViewport (int offset, int worldSize, int windowSize, 
	int scrollUnits, int scrollMode);

BLT_EXTERN int Blt_GetScrollInfoFromObj (Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv, int *offsetPtr, int worldSize, int windowSize,
	int scrollUnits, int scrollMode);

BLT_EXTERN void Blt_UpdateScrollbar(Tcl_Interp *interp, 
	Tcl_Obj *scrollCmdObjPtr, int first, int last, int width);

BLT_EXTERN int Blt_ReparentWindow (Display *display, Window window, 
	Window newParent, int x, int y);

BLT_EXTERN int Blt_LoadLibrary(Tcl_Interp *interp, const char *libPath, 
	const char *initProcName, const char *safeProcName);

extern void Blt_RegisterPictureImageType(Tcl_Interp *interp);
extern int  Blt_CpuFeatures(Tcl_Interp *interp, int *featuresPtr);
extern void Blt_RegisterEpsCanvasItem(void);

typedef struct {
    Drawable id;
    unsigned short int width, height;
    int depth;
    Colormap colormap;
    Visual *visual;
} Blt_DrawableAttributes;

BLT_EXTERN Blt_DrawableAttributes *Blt_GetDrawableAttribs(Display *display,
	Drawable drawable);

BLT_EXTERN void Blt_SetDrawableAttribs(Display *display, Drawable drawable,
	int width, int height, int depth, Colormap colormap, Visual *visual);

BLT_EXTERN void Blt_SetDrawableAttribsFromWindow(Tk_Window tkwin, 
	Drawable drawable);

BLT_EXTERN void Blt_FreeDrawableAttribs(Display *display, Drawable drawable);

BLT_EXTERN GC Blt_GetBitmapGC(Tk_Window tkwin);

#define Tk_RootWindow(tkwin)	\
	RootWindow(Tk_Display(tkwin),Tk_ScreenNumber(tkwin))

#ifndef WIN32
#  define PurifyPrintf  printf
#endif /* WIN32 */

#include "bltConfig.h"

/*
 * Define this if you want to be able to tile to the main window "."  This
 * will cause a conflict with Tk if you try to compile and link statically.
 */
#undef TK_MAINWINDOW

#ifdef WIN32
#  define	NO_PTYEXEC	1
#  define	NO_CUTBUFFER	1
#  define	NO_DND		1  
#else
#  define	NO_DDE		1
#  define	NO_PRINTER	1
#  ifdef MACOSX
#    define	NO_BUSY		1
#    define	NO_CONTAINER	1
#  endif  
#endif /* WIN32 */

#ifndef NO_BASE64
BLT_EXTERN Tcl_AppInitProc Blt_Base64CmdInitProc;
#endif
#ifndef NO_BEEP
BLT_EXTERN Tcl_AppInitProc Blt_BeepCmdInitProc;
#endif
#ifndef NO_BGEXEC
BLT_EXTERN Tcl_AppInitProc Blt_BgexecCmdInitProc;
#endif
#ifndef NO_PTYEXEC
BLT_EXTERN Tcl_AppInitProc Blt_PtyExecCmdInitProc;
#endif
#ifndef NO_BITMAP
BLT_EXTERN Tcl_AppInitProc Blt_BitmapCmdInitProc;
#endif
#ifndef NO_BUSY
BLT_EXTERN Tcl_AppInitProc Blt_BusyCmdInitProc;
#endif
#ifndef NO_CONTAINER
BLT_EXTERN Tcl_AppInitProc Blt_ContainerCmdInitProc;
#endif
#ifndef NO_CRC32
BLT_EXTERN Tcl_AppInitProc Blt_Crc32CmdInitProc;
#endif
#ifndef NO_CVS
BLT_EXTERN Tcl_AppInitProc Blt_CsvCmdInitProc;
#endif
#ifndef NO_CUTBUFFER
BLT_EXTERN Tcl_AppInitProc Blt_CutbufferCmdInitProc;
#endif
#ifndef NO_DEBUG
BLT_EXTERN Tcl_AppInitProc Blt_DebugCmdInitProc;
#endif
#ifndef NO_DRAGDROP
BLT_EXTERN Tcl_AppInitProc Blt_DragDropCmdInitProc;
#endif
#ifndef NO_DND
BLT_EXTERN Tcl_AppInitProc Blt_DndCmdInitProc;
#endif
#ifndef NO_GRAPH
BLT_EXTERN Tcl_AppInitProc Blt_GraphCmdInitProc;
#endif
#ifndef NO_HIERBOX
BLT_EXTERN Tcl_AppInitProc Blt_HierboxCmdInitProc;
#endif
#ifndef NO_HIERTABLE
BLT_EXTERN Tcl_AppInitProc Blt_HiertableCmdInitProc;
#endif
#ifndef NO_HTEXT
BLT_EXTERN Tcl_AppInitProc Blt_HtextCmdInitProc;
#endif
#ifdef WIN32
#  ifndef NO_PRINTER
BLT_EXTERN Tcl_AppInitProc Blt_PrinterCmdInitProc;
#  endif
#endif
BLT_EXTERN Tcl_AppInitProc Blt_AfmCmdInitProc;
#ifndef NO_PICTURE
BLT_EXTERN Tcl_AppInitProc Blt_PictureCmdInitProc;
#endif
#ifndef NO_TABLEMGR
BLT_EXTERN Tcl_AppInitProc Blt_TableMgrCmdInitProc;
#endif
#ifndef NO_VECTOR
BLT_EXTERN Tcl_AppInitProc Blt_VectorCmdInitProc;
#endif
#ifndef NO_WINOP
BLT_EXTERN Tcl_AppInitProc Blt_WinopCmdInitProc;
#endif
#ifndef NO_WATCH
BLT_EXTERN Tcl_AppInitProc Blt_WatchCmdInitProc;
#endif
#ifndef NO_SPLINE
BLT_EXTERN Tcl_AppInitProc Blt_SplineCmdInitProc;
#endif
#ifndef NO_TABSET
BLT_EXTERN Tcl_AppInitProc Blt_TabsetCmdInitProc;
#endif
#ifndef NO_TABLE
BLT_EXTERN Tcl_AppInitProc Blt_TableCmdInitProc;
#endif
#ifndef NO_TREE
BLT_EXTERN Tcl_AppInitProc Blt_TreeCmdInitProc;
#endif
#ifndef NO_TREEVIEW
BLT_EXTERN Tcl_AppInitProc Blt_TreeViewCmdInitProc;
#endif
#ifndef NO_TKFRAME
BLT_EXTERN Tcl_AppInitProc Blt_FrameCmdInitProc;
#endif
#ifndef NO_TKBUTTON
BLT_EXTERN Tcl_AppInitProc Blt_ButtonCmdInitProc;
#endif
#ifndef NO_SCROLLSET
BLT_EXTERN Tcl_AppInitProc Blt_ScrollsetCmdInitProc;
#endif
#ifndef NO_PANESET
BLT_EXTERN Tcl_AppInitProc Blt_PanesetCmdInitProc;
#endif
#ifndef NO_TKSCROLLBAR
BLT_EXTERN Tcl_AppInitProc Blt_ScrollbarCmdInitProc;
#endif
BLT_EXTERN Tcl_AppInitProc Blt_BgPatternCmdInitProc;

#if (BLT_MAJOR_VERSION == 3)
#  ifndef NO_MOUNTAIN
BLT_EXTERN Tcl_AppInitProc Blt_MountainCmdInitProc;
#  endif
#endif
#ifndef NO_TED
BLT_EXTERN Tcl_AppInitProc Blt_TedCmdInitProc;
#endif

BLT_EXTERN Tcl_AppInitProc Blt_ComboButtonInitProc;
BLT_EXTERN Tcl_AppInitProc Blt_ComboEntryInitProc;
BLT_EXTERN Tcl_AppInitProc Blt_ComboMenuInitProc;
BLT_EXTERN Tcl_AppInitProc Blt_ComboTreeInitProc;
BLT_EXTERN Tcl_AppInitProc Blt_ListViewInitProc;

BLT_EXTERN Tcl_AppInitProc Blt_SendEventCmdInitProc;

#ifndef NO_DDE
BLT_EXTERN Tcl_AppInitProc Blt_DdeCmdInitProc;
#endif


#ifndef HAVE_SPRINTF_S
BLT_EXTERN int sprintf_s(char *s, size_t size, const char *fmt, /*args*/ ...);
#endif	/* HAVE_SPRINTF_S */


BLT_EXTERN Pixmap Blt_GetPixmap(Display *dpy, Drawable draw, int w, int h, 
	int depth, int lineNum, const char *fileName);

#define Tk_GetPixmap(dpy, draw, w, h, depth) \
    Blt_GetPixmap(dpy, draw, w, h, depth, __LINE__, __FILE__)

#if defined(HAVE_LIBXRANDR) && defined(HAVE_X11_EXTENSIONS_RANDR_H)
#define HAVE_RANDR 1
#else 
#define HAVE_RANDR 0 
#endif

#undef panic
#define panic(mesg)	Blt_Panic("%s:%d %s", __FILE__, __LINE__, (mesg))
BLT_EXTERN void Blt_Panic TCL_VARARGS(const char *, args);

#ifdef WIN32
#  include "bltWin.h"
#else 

#ifdef MACOSX
#  include "bltMacOSX.h"
#endif /* MACOSX */

#endif /* WIN32 */

BLT_EXTERN double Blt_NaN(void);

BLT_EXTERN void Blt_ScreenDPI(Tk_Window tkwin, unsigned int *xPtr, 
	unsigned int *yPtr);

#include <bltAlloc.h>
#include <bltAssert.h>

#if defined (WIN32) || defined(MAC_TCL) || defined(MAC_OSX_TCL)
typedef struct _TkRegion *TkRegion;	/* Opaque type */

/* 114 */
extern TkRegion TkCreateRegion(void);
/* 115 */
extern void TkDestroyRegion (TkRegion rgn);
/* 116 */
extern void TkIntersectRegion (TkRegion sra, TkRegion srcb, TkRegion dr_return);
/* 117 */
extern int TkRectInRegion(TkRegion rgn, int x, int y, unsigned int width, 
			  unsigned int height);
/* 118 */
extern void TkSetRegion(Display* display, GC gc, TkRegion rgn);
/* 119 */
extern void TkUnionRectWithRegion(XRectangle* rect, TkRegion src, 
	TkRegion dr_return);
#else
typedef struct _TkRegion *TkRegion;	/* Opaque type */
#define TkClipBox(rgn, rect) XClipBox((Region) rgn, rect)
#define TkCreateRegion() (TkRegion) XCreateRegion()
#define TkDestroyRegion(rgn) XDestroyRegion((Region) rgn)
#define TkIntersectRegion(a, b, r) XIntersectRegion((Region) a, \
	(Region) b, (Region) r)
#define TkRectInRegion(r, x, y, w, h) XRectInRegion((Region) r, x, y, w, h)
#define TkSetRegion(d, gc, rgn) XSetRegion(d, gc, (Region) rgn)
#define TkSubtractRegion(a, b, r) XSubtractRegion((Region) a, \
	(Region) b, (Region) r)
#define TkUnionRectWithRegion(rect, src, ret) XUnionRectWithRegion(rect, \
	(Region) src, (Region) ret)
#endif

#endif /*_BLT_INT_H*/
