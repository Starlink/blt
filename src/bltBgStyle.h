
/*
 * bltBgPattern.h --
 *
 *	Copyright 1995-2004 George A Howlett.
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

#ifndef _BLT_BGPATTERN_H
#define _BLT_BGPATTERN_H

typedef struct _Blt_Background *Blt_Background;

typedef void Blt_BackgroundChangedProc(ClientData clientData);

BLT_EXTERN Blt_Background Blt_GetBackground(Tcl_Interp *interp, Tk_Window tkwin,
	const char *styleName);

BLT_EXTERN Blt_Background Blt_GetBackgroundFromObj(Tcl_Interp *interp, 
	Tk_Window tkwin, Tcl_Obj *objPtr);

BLT_EXTERN XColor *Blt_BackgroundBorderColor(Blt_Background bg);

BLT_EXTERN Tk_3DBorder Blt_BackgroundBorder(Blt_Background bg);

BLT_EXTERN const char *Blt_NameOfBackground(Blt_Background bg);

BLT_EXTERN void Blt_FreeBackground(Blt_Background bg);

BLT_EXTERN void Blt_DrawBackgroundRectangle(Tk_Window tkwin, Drawable drawable,
	Blt_Background bg, int x, int y, int width, int height, int borderWidth,
	int relief);

BLT_EXTERN void Blt_FillBackgroundRectangle(Tk_Window tkwin, Drawable drawable,
	Blt_Background bg, int x, int y, int width, int height, 
	int borderWidth, int relief);

BLT_EXTERN void Blt_DrawBackgroundPolygon(Tk_Window tkwin, Drawable drawable, 
	Blt_Background bg, XPoint *points, int nPoints, int borderWidth, 
	int leftRelief);

BLT_EXTERN void Blt_FillBackgroundPolygon(Tk_Window tkwin, Drawable drawable, 
	Blt_Background bg, XPoint *points, int nPoints, int borderWidth, 
	int leftRelief);

BLT_EXTERN void Blt_GetBackgroundOrigin(Blt_Background bg, int *xPtr,int *yPtr);

BLT_EXTERN void Blt_SetBackgroundChangedProc(Blt_Background bg, 
	Blt_BackgroundChangedProc *notifyProc, ClientData clientData);

BLT_EXTERN void Blt_SetBackgroundOrigin(Tk_Window tkwin, Blt_Background bg, 
	int x, int y);

BLT_EXTERN void Blt_DrawFocusBackground(Tk_Window tkwin, Blt_Background bg, 
	int highlightWidth, Drawable drawable);

BLT_EXTERN GC Blt_BackgroundBorderGC(Tk_Window tkwin, Blt_Background bg, 
	int which);

BLT_EXTERN void Blt_SetBackgroundFromBackground(Tk_Window tkwin, 
	Blt_Background bg);

BLT_EXTERN void Blt_UnsetBackgroundClipRegion(Tk_Window tkwin, 
	Blt_Background bg);

BLT_EXTERN void Blt_SetBackgroundClipRegion(Tk_Window tkwin, Blt_Background bg,
	TkRegion rgn);

#endif /* BLT_BGPATTERN_H */
