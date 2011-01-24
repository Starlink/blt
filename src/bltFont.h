
/*
 * bltFont.h --
 *
 *	Copyright 1993-2004 George A Howlett.v
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

#ifndef _BLT_FONT_H
#define _BLT_FONT_H

#define FONT_ITALIC	(1<<0)
#define FONT_BOLD	(1<<1)

typedef struct {
    int ascent;			/* The amount in pixels that the tallest
				 * letter sticks up above the baseline, plus
				 * any extra blank space added by the designer
				 * of the font. */
    int descent;		/* The largest amount in pixels that any
				 * letter sticks below the baseline, plus any
				 * extra blank space added by the designer of
				 * the font. */
    int linespace;		/* The sum of the ascent and descent.  How
				 * far apart two lines of text in the same
				 * font should be placed so that none of the
				 * characters in one line overlap any of the
				 * characters in the other line. */
    int tabWidth;		/* Width of tabs in this font (pixels). */
    int	underlinePos;		/* Offset from baseline to origin of
				 * underline bar (used for drawing underlines
				 * on a non-underlined font). */
    int underlineHeight;	/* Height of underline bar (used for drawing
				 * underlines on a non-underlined font). */
} Blt_FontMetrics;


typedef struct _Blt_Font *Blt_Font;
typedef struct Blt_FontClass Blt_FontClass;

typedef const char *(Blt_NameOfFontProc)(Blt_Font font);
typedef void (Blt_GetFontMetricsProc)(Blt_Font font, 
	Blt_FontMetrics *metricsPtr);
typedef Font (Blt_FontIdProc)(Blt_Font font);
typedef int (Blt_TextStringWidthProc)(Blt_Font font, const char *string, 
	int nBytes);
typedef void (Blt_FreeFontProc)(Blt_Font font);
typedef int (Blt_MeasureCharsProc)(Blt_Font font, const char *text, int nBytes, 
	int maxLength, int flags, int *lengthPtr);
typedef void (Blt_DrawCharsProc)(Display *display, Drawable drawable, GC gc, 
	Blt_Font font, int depth, float angle, const char *text, int length, 
	int x, int y);
typedef int (Blt_PostscriptFontNameProc)(Blt_Font font, Tcl_DString *resultPtr);
typedef const char *(Blt_FamilyOfFontProc)(Blt_Font font);
typedef int (Blt_CanRotateFontProc)(Blt_Font font, float angle);
typedef void (Blt_UnderlineCharsProc)(Display *display, Drawable drawable, 
	GC gc, Blt_Font font, const char *text, int textLen, int x, int y, 
	int first, int last, int xMax);

struct Blt_FontClass {
    int type;			/* Indicates the type of font used. */
    Blt_NameOfFontProc *nameOfFont;
    Blt_FamilyOfFontProc *familyOfFont;
    Blt_FontIdProc *fontId;
    Blt_GetFontMetricsProc *getFontMetrics;
    Blt_MeasureCharsProc *measureChars;
    Blt_TextStringWidthProc *textWidth;
    Blt_CanRotateFontProc *canRotateFont;
    Blt_DrawCharsProc *drawChars;
    Blt_PostscriptFontNameProc *postscriptFontName;
    Blt_FreeFontProc  *freeFont;
    Blt_UnderlineCharsProc  *underlineChars;
};

struct _Blt_Font {
    void *clientData;
    Tcl_Interp *interp;
    Display *display;
    Blt_FontClass *classPtr;
};

#define Blt_NameOfFont(f) (*(f)->classPtr->nameOfFont)(f)
#define Blt_FontId(f) (*(f)->classPtr->fontId)(f)
#define Blt_MeasureChars(f,s,l,ml,fl,lp) \
	(*(f)->classPtr->measureChars)(f,s,l,ml,fl,lp)
#define Blt_DrawChars(d,w,gc,f,dp,a,t,l,x,y)		\
	(*(f)->classPtr->drawChars)(d,w,gc,f,dp,a,t,l,x,y)
#define Blt_PostscriptFontName(f,rp) (*(f)->classPtr->postscriptFontName)(f,rp)
#define Blt_FamilyOfFont(f) (*(f)->classPtr->familyOfFont)(f)
#define Blt_CanRotateFont(f,a) (*(f)->classPtr->canRotateFont)(f,a)
#define Blt_FreeFont(f) (*(f)->classPtr->freeFont)(f)
#define Blt_UnderlineChars(d,w,g,f,s,l,x,y,a,b,m)		\
	(*(f)->classPtr->underlineChars)(d,w,g,f,s,l,x,y,a,b,m)

BLT_EXTERN Blt_Font Blt_GetFont(Tcl_Interp *interp, Tk_Window tkwin, 
	const char *string);
BLT_EXTERN Blt_Font Blt_AllocFontFromObj(Tcl_Interp *interp, Tk_Window tkwin, 
	Tcl_Obj *objPtr);

BLT_EXTERN void Blt_DrawCharsWithEllipsis(Tk_Window tkwin, Drawable drawable,
	GC gc, Blt_Font font, int depth, float angle, const char *string, 
	int nBytes, int x, int y, int maxLength);

BLT_EXTERN Blt_Font Blt_GetFontFromObj(Tcl_Interp *interp, Tk_Window tkwin,
	Tcl_Obj *objPtr);

BLT_EXTERN void Blt_GetFontMetrics(Blt_Font font, Blt_FontMetrics *fmPtr);
BLT_EXTERN int Blt_TextWidth(Blt_Font font, const char *string, int length);
BLT_EXTERN Tcl_Interp *Blt_GetFontInterp(Blt_Font font);

#ifdef _XFT_H_
BLT_EXTERN const char *Blt_GetFontFileFromObj(Tcl_Interp *interp, 
	Tcl_Obj *objPtr, double *sizePtr);
BLT_EXTERN const char *Blt_GetFontFile(Tcl_Interp *interp, const char *spec,
	double *sizePtr);
#endif /*_XFT_H_*/

#endif /* _BLT_FONT_H */
