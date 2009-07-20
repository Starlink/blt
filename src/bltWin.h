
/*
 * bltWin.h --
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

#ifndef _BLT_WIN_H
#define _BLT_WIN_H

#define _CRT_SECURE_NO_DEPRECATE

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef STRICT
#undef WIN32_LEAN_AND_MEAN
#include <windowsx.h>

#undef STD_NORMAL_BACKGROUND
#undef STD_NORMAL_FOREGROUND
#undef STD_SELECT_BACKGROUND
#undef STD_SELECT_FOREGROUND
#undef STD_TEXT_FOREGROUND
#undef STD_FONT
#undef STD_FONT_LARGE
#undef STD_FONT_SMALL

#define STD_NORMAL_BACKGROUND	"SystemButtonFace"
#define STD_NORMAL_FOREGROUND	"SystemButtonText"
#define STD_SELECT_BACKGROUND	"SystemHighlight"
#define STD_SELECT_FOREGROUND	"SystemHighlightText"
#define STD_TEXT_FOREGROUND	"SystemWindowText"
#define STD_FONT		"Arial 8"
#define STD_FONT_LARGE		"Arial 12"
#define STD_FONT_SMALL		"Arial 6"

#ifdef CHECK_UNICODE_CALLS
#define _UNICODE
#define UNICODE
#define __TCHAR_DEFINED
typedef float *_TCHAR;
#define _TCHAR_DEFINED
typedef float *TCHAR;
#endif /* CHECK_UNICODE_CALLS */

/* DOS Encapsulated PostScript File Header */
#pragma pack(2)
typedef struct {
    BYTE magic[4];		/* Magic number for a DOS EPS file
				 * C5,D0,D3,C6 */
    DWORD psStart;		/* Offset of PostScript section. */
    DWORD psLength;		/* Length of the PostScript section. */
    DWORD wmfStart;		/* Offset of Windows Meta File section. */
    DWORD wmfLength;		/* Length of Meta file section. */
    DWORD tiffStart;		/* Offset of TIFF section. */
    DWORD tiffLength;		/* Length of TIFF section. */
    WORD checksum;		/* Checksum of header. If FFFF, ignore. */
} DOSEPSHEADER;

#pragma pack()

/* Aldus Portable Metafile Header */
#pragma pack(2)
typedef struct {
    DWORD key;			/* Type of metafile */
    WORD hmf;			/* Unused. Must be NULL. */
    SMALL_RECT bbox;		/* Bounding rectangle */
    WORD inch;			/* Units per inch. */
    DWORD reserved;		/* Unused. */
    WORD checksum;		/* XOR of previous fields (10 32-bit words). */
} APMHEADER;
#pragma pack()

#undef Blt_Export
#define Blt_Export __declspec(dllexport)

#if defined(_MSC_VER) || defined(__BORLANDC__)
#define fstat	 _fstat
#define stat	 _stat
#ifdef _MSC_VER
#define fileno	 _fileno
#endif
#define isnan(x)		_isnan(x)
#define strcasecmp(s1,s2)	_stricmp(s1,s2)
#define strncasecmp(s1,s2,n)	_strnicmp(s1,s2,n)
#define vsnprintf		_vsnprintf
#define isascii(c)		__isascii(c)
#endif /* _MSC_VER || __BORLANDC__ */

#ifdef __BORLANDC__
#define isnan(x)		_isnan(x)
#endif

#if defined(__BORLANDC__) || defined(_MSC_VER)
#ifdef FINITE
#undef FINITE
#define FINITE(x)		_finite(x)
#endif
#endif /* __BORLANDC__ || _MSC_VER */

#ifdef __GNUC__ 
#include <wingdi.h>
#include <windowsx.h>
#undef Status
#include <winspool.h>
#define Status int
/*
 * Add definitions missing from windgi.h, windowsx.h, and winspool.h
 */
#include <missing.h> 
#endif /* __GNUC__ */

#undef XCopyArea		
#define XCopyArea		Blt_EmulateXCopyArea
#undef XCopyPlane		
#define XCopyPlane		Blt_EmulateXCopyPlane
#undef XDrawArcs		
#define XDrawArcs		Blt_EmulateXDrawArcs
#undef XDrawLine		
#define XDrawLine		Blt_EmulateXDrawLine
#undef XDrawLines		
#define XDrawLines		Blt_EmulateXDrawLines
#undef XDrawPoints		
#define XDrawPoints		Blt_EmulateXDrawPoints
#undef XDrawRectangle		
#define XDrawRectangle		Blt_EmulateXDrawRectangle
#undef XDrawRectangles		
#define XDrawRectangles		Blt_EmulateXDrawRectangles
#undef XDrawSegments		
#define XDrawSegments		Blt_EmulateXDrawSegments
#undef XDrawString		
#define XDrawString		Blt_EmulateXDrawString
#undef XFillArcs		
#define XFillArcs		Blt_EmulateXFillArcs
#undef XFillPolygon		
#define XFillPolygon		Blt_EmulateXFillPolygon
#undef XFillRectangle		
#define XFillRectangle		Blt_EmulateXFillRectangle
#undef XFillRectangles		
#define XFillRectangles		Blt_EmulateXFillRectangles
#undef XFree			
#define XFree			Blt_EmulateXFree
#undef XGetWindowAttributes	
#define XGetWindowAttributes	Blt_EmulateXGetWindowAttributes
#undef XLowerWindow		
#define XLowerWindow		Blt_EmulateXLowerWindow
#undef XMaxRequestSize		
#define XMaxRequestSize		Blt_EmulateXMaxRequestSize
#undef XRaiseWindow		
#define XRaiseWindow		Blt_EmulateXRaiseWindow
#undef XReparentWindow		
#define XReparentWindow		Blt_EmulateXReparentWindow
#undef XSetDashes		
#define XSetDashes		Blt_EmulateXSetDashes
#undef XUnmapWindow		
#define XUnmapWindow		Blt_EmulateXUnmapWindow
#undef XWarpPointer		
#define XWarpPointer		Blt_EmulateXWarpPointer

BLT_EXTERN GC Blt_EmulateXCreateGC(Display *display, Drawable drawable,
    unsigned long mask, XGCValues *valuesPtr);
BLT_EXTERN void Blt_EmulateXCopyArea(Display *display, Drawable src, Drawable dest,
    GC gc, int src_x, int src_y, unsigned int width, unsigned int height,
    int dest_x, int dest_y);
BLT_EXTERN void Blt_EmulateXCopyPlane(Display *display, Drawable src,
    Drawable dest, GC gc, int src_x, int src_y, unsigned int width,
    unsigned int height, int dest_x, int dest_y, unsigned long plane);
BLT_EXTERN void Blt_EmulateXDrawArcs(Display *display, Drawable drawable, GC gc,
    XArc *arcArr, int nArcs);
BLT_EXTERN void Blt_EmulateXDrawLine(Display *display, Drawable drawable, GC gc,
    int x1, int y1, int x2, int y2);
BLT_EXTERN void Blt_EmulateXDrawLines(Display *display, Drawable drawable, GC gc,
    XPoint *pointArr, int nPoints, int mode);
BLT_EXTERN void Blt_EmulateXDrawPoints(Display *display, Drawable drawable, GC gc,
    XPoint *pointArr, int nPoints, int mode);
BLT_EXTERN void Blt_EmulateXDrawRectangle(Display *display, Drawable drawable,
    GC gc, int x, int y, unsigned int width, unsigned int height);
BLT_EXTERN void Blt_EmulateXDrawRectangles(Display *display, Drawable drawable,
    GC gc, XRectangle *rectArr, int nRects);
BLT_EXTERN void Blt_EmulateXDrawSegments(Display *display, Drawable drawable,
    GC gc, XSegment *segArr, int nSegments);
BLT_EXTERN void Blt_EmulateXDrawSegments(Display *display, Drawable drawable,
    GC gc, XSegment *segArr, int nSegments);
BLT_EXTERN void Blt_EmulateXDrawString(Display *display, Drawable drawable, GC gc,
    int x, int y, _Xconst char *string, int length);
BLT_EXTERN void Blt_EmulateXFillArcs(Display *display, Drawable drawable, GC gc,
    XArc *arcArr, int nArcs);
BLT_EXTERN void Blt_EmulateXFillPolygon(Display *display, Drawable drawable,
    GC gc, XPoint *points, int nPoints,  int shape, int mode);
BLT_EXTERN void Blt_EmulateXFillRectangle(Display *display, Drawable drawable,
    GC gc, int x, int y, unsigned int width, unsigned int height);
BLT_EXTERN void Blt_EmulateXFillRectangles(Display *display, Drawable drawable,
    GC gc, XRectangle *rectArr, int nRects);
BLT_EXTERN void Blt_EmulateXFree(void *ptr);
BLT_EXTERN int Blt_EmulateXGetWindowAttributes(Display *display, Window window,
    XWindowAttributes * attrsPtr);
BLT_EXTERN void Blt_EmulateXLowerWindow(Display *display, Window window);
BLT_EXTERN void Blt_EmulateXMapWindow(Display *display, Window window);
BLT_EXTERN long Blt_EmulateXMaxRequestSize(Display *display);
BLT_EXTERN void Blt_EmulateXRaiseWindow(Display *display, Window window);
BLT_EXTERN void Blt_EmulateXReparentWindow(Display *display, Window window,
    Window parent, int x, int y);
BLT_EXTERN void Blt_EmulateXSetDashes(Display *display, GC gc, int dashOffset,
    _Xconst char *dashList, int n);
BLT_EXTERN void Blt_EmulateXUnmapWindow(Display *display, Window window);
BLT_EXTERN void Blt_EmulateXWarpPointer(Display *display, Window srcWindow,
    Window destWindow, int srcX, int srcY, unsigned int srcWidth,
    unsigned int srcHeight, int destX, int destY);

BLT_EXTERN void Blt_DrawLine2D(Display *display, Drawable drawable, GC gc,
    POINT *screenPts, int nScreenPts);

BLT_EXTERN unsigned char *Blt_GetBitmapData(Display *display, Pixmap bitmap, 
	int width, int height, int *pitchPtr);

BLT_EXTERN HFONT Blt_CreateRotatedFont(Tk_Window tkwin, unsigned long font, 
	float angle);

BLT_EXTERN HPALETTE Blt_GetSystemPalette(void);

BLT_EXTERN HPEN Blt_GCToPen(HDC dc, GC gc);

BLT_EXTERN double hypot(double x, double y);
BLT_EXTERN int Blt_AsyncRead(int fd, char *buffer, unsigned int size);
BLT_EXTERN int Blt_AsyncWrite(int fd, const char *buffer, unsigned int size);
BLT_EXTERN void Blt_CreateFileHandler(int fd, int flags, 
	Tcl_FileProc * proc, ClientData clientData);
BLT_EXTERN void Blt_DeleteFileHandler(int fd);
BLT_EXTERN int Blt_GetPlatformId(void);
BLT_EXTERN const char *Blt_LastError(void);
BLT_EXTERN const char *Blt_PrintError(int error);

BLT_EXTERN int Blt_GetOpenPrinter(Tcl_Interp *interp, const char *id,
    Drawable *drawablePtr);
BLT_EXTERN int Blt_PrintDialog(Tcl_Interp *interp, Drawable *drawablePtr);
BLT_EXTERN int Blt_OpenPrinterDoc(Tcl_Interp *interp, const char *id);
BLT_EXTERN int Blt_ClosePrinterDoc(Tcl_Interp *interp, const char *id);
BLT_EXTERN void Blt_GetPrinterScale(HDC dc, double *xRatio, double *yRatio);
BLT_EXTERN int Blt_StartPrintJob(Tcl_Interp *interp, Drawable drawable);
BLT_EXTERN int Blt_EndPrintJob(Tcl_Interp *interp, Drawable drawable);

#endif /*_BLT_WIN_H*/
