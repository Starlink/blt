
/*
 * bltPictDraw.c --
 *
 * This module implements image drawing primitives (line, circle, rectangle,
 * text, etc.) for picture images in the BLT toolkit.
 *
 *	Copyright 1997-2004 George A Howlett.
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
#include "bltOp.h"
#include "bltHash.h"
#include "bltSwitch.h"
#include "bltPicture.h"
#include "bltPictInt.h"
#include <X11/Xutil.h>
#include "bltFont.h"
#include "bltText.h"
#include <string.h>

#define imul8x8(a,b,t)	((t) = (a)*(b)+128,(((t)+((t)>>8))>>8))
#define CLAMP(c)	((((c) < 0.0) ? 0.0 : ((c) > 255.0) ? 255.0 : (c)))

typedef struct {
    unsigned int alpha;
    unsigned int offset;
} Shadow;

#ifdef HAVE_FT2BUILD_H
#include <ft2build.h>
#include FT_FREETYPE_H
#ifdef HAVE_LIBXFT
#include <X11/Xft/Xft.h>
#endif /* HAVE_LIBXFT */


typedef struct {
    FT_Face face;
    FT_Matrix matrix;
    FT_Library lib;
    XftFont *xftFont;
    int fontSize;
    float angle;
    int height;
    int ascent, descent;
} FtFont;    
#endif /*HAVE_FT2BUILD_H*/

static Blt_SwitchParseProc ArraySwitchProc;
static Blt_SwitchFreeProc ArrayFreeProc;
static Blt_SwitchCustom arraySwitch = {
    ArraySwitchProc, ArrayFreeProc, (ClientData)0,
};

static Blt_SwitchParseProc AnchorSwitch;
static Blt_SwitchCustom anchorSwitch = {
    AnchorSwitch, NULL, (ClientData)0,
};

static Blt_SwitchParseProc AlphaSwitch;
static Blt_SwitchCustom alphaSwitch = {
    AlphaSwitch, NULL, (ClientData)0,
};

static Blt_SwitchParseProc ShadowSwitch;
static Blt_SwitchCustom shadowSwitch = {
    ShadowSwitch, NULL, (ClientData)0,
};

BLT_EXTERN Blt_SwitchParseProc Blt_ColorSwitchProc;
static Blt_SwitchCustom colorSwitch = {
    Blt_ColorSwitchProc, NULL, (ClientData)0,
};

typedef struct {
    int alpha;			/* Overrides alpha value of color. */
    Blt_Pixel outline, fill;	/* Outline and fill colors for the circle. */
    Shadow shadow;
    int antialiased;
    int lineWidth;		/* Width of outline.  If zero, indicates to
				 * draw a solid circle. */
} CircleSwitches;

static Blt_SwitchSpec circleSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-alpha", "value",
	Blt_Offset(CircleSwitches, alpha), 0, 0, &alphaSwitch},
    {BLT_SWITCH_CUSTOM, "-fill", "fill",
	Blt_Offset(CircleSwitches, fill),    0, 0, &colorSwitch},
    {BLT_SWITCH_BOOLEAN, "-antialiased", "bool",
	Blt_Offset(CircleSwitches, antialiased), 0},
    {BLT_SWITCH_INT_NNEG, "-linewidth", "value",
	Blt_Offset(CircleSwitches, lineWidth), 0},
    {BLT_SWITCH_CUSTOM, "-outline", "outline",
	Blt_Offset(CircleSwitches, outline),    0, 0, &colorSwitch},
    {BLT_SWITCH_CUSTOM, "-shadow", "offset",
        Blt_Offset(CircleSwitches, shadow), 0, 0, &shadowSwitch},
    {BLT_SWITCH_END}
};

typedef struct {
    int alpha;			/* Overrides alpha value of color. */
    Blt_Pixel bg;		/* Color of line. */
    int lineWidth;		/* Width of outline. */
    Array x, y;
    Array coords;
} LineSwitches;

typedef struct {
    int alpha;			/* Overrides alpha value of color. */
    Blt_Pixel bg;		/* Fill color of polygon. */
    int antialiased;
    Shadow shadow;
    int lineWidth;		/* Width of outline. Default is 1, If zero,
				 * indicates to draw a solid polygon. */
    Array coords;
    Array x, y;

} PolygonSwitches;

static Blt_SwitchSpec polygonSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-alpha", "int",
	Blt_Offset(PolygonSwitches, alpha), 0, 0, &alphaSwitch},
    {BLT_SWITCH_BOOLEAN, "-antialiased", "bool",
	Blt_Offset(PolygonSwitches, antialiased), 0},
    {BLT_SWITCH_CUSTOM, "-color", "color",
	Blt_Offset(PolygonSwitches, bg),    0, 0, &colorSwitch},
    {BLT_SWITCH_CUSTOM, "-coords", "{x0 y0 x1 y1 ... xn yn}",
	Blt_Offset(PolygonSwitches, coords), 0, 0, &arraySwitch},
    {BLT_SWITCH_CUSTOM, "-x", "{x0 x1 ... xn}",
	Blt_Offset(PolygonSwitches, x), 0, 0, &arraySwitch},
    {BLT_SWITCH_CUSTOM, "-y", "{x0 x1 ... xn}",
	Blt_Offset(PolygonSwitches, y), 0, 0, &arraySwitch},
    {BLT_SWITCH_CUSTOM, "-shadow", "offset",
        Blt_Offset(PolygonSwitches, shadow), 0, 0, &shadowSwitch},
    {BLT_SWITCH_INT_POS, "-linewidth", "int",
	Blt_Offset(PolygonSwitches, lineWidth), 0},
    {BLT_SWITCH_END}
};

static Blt_SwitchSpec lineSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-alpha", "int",
	Blt_Offset(LineSwitches, alpha), 0, 0, &alphaSwitch},
    {BLT_SWITCH_CUSTOM, "-color", "color",
	Blt_Offset(LineSwitches, bg),    0, 0, &colorSwitch},
    {BLT_SWITCH_CUSTOM, "-coords", "{x0 y0 x1 y1 ... xn yn}",
	Blt_Offset(LineSwitches, coords), 0, 0, &arraySwitch},
    {BLT_SWITCH_INT_POS, "-linewidth", "int",
	Blt_Offset(LineSwitches, lineWidth), 0},
    {BLT_SWITCH_CUSTOM, "-x", "{x0 x1 ... xn}",
	Blt_Offset(LineSwitches, x), 0, 0, &arraySwitch},
    {BLT_SWITCH_CUSTOM, "-y", "{x0 x1 ... xn}",
	Blt_Offset(LineSwitches, y), 0, 0, &arraySwitch},
    {BLT_SWITCH_END}
};

typedef struct {
    int alpha;			/* Overrides alpha value of color. */
    Blt_Pixel bg;		/* Color of rectangle. */
    Shadow shadow;
    int lineWidth;		/* Width of outline. If zero, indicates to
				 * draw a solid rectangle. */
    int radius;			/* Radius of rounded corner. */
    int antialiased;
} RectangleSwitches;

static Blt_SwitchSpec rectangleSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-alpha", "int", 
	Blt_Offset(RectangleSwitches, alpha), 0, 0, &alphaSwitch},
    {BLT_SWITCH_CUSTOM, "-color", "color",
	Blt_Offset(RectangleSwitches, bg),    0, 0, &colorSwitch},
    {BLT_SWITCH_BOOLEAN, "-antialiased", "bool",
	Blt_Offset(RectangleSwitches, antialiased), 0},
    {BLT_SWITCH_INT_NNEG, "-radius", "number",
	Blt_Offset(RectangleSwitches, radius), 0},
    {BLT_SWITCH_CUSTOM, "-shadow", "offset",
        Blt_Offset(RectangleSwitches, shadow), 0, 0, &shadowSwitch},
    {BLT_SWITCH_INT_NNEG, "-linewidth", "number",
	Blt_Offset(RectangleSwitches, lineWidth), 0},
    {BLT_SWITCH_END}
};

typedef struct {
    int alpha;			/* Overrides alpha value of color. */
    int kerning;
    Blt_Pixel color;		/* Color of text. */
    Shadow shadow;
    int fontSize;
    Tcl_Obj *fontObjPtr;
    int justify;
    Tk_Anchor anchor;
    float angle;
} TextSwitches;

static Blt_SwitchSpec textSwitches[] = 
{
    {BLT_SWITCH_CUSTOM,  "-alpha",    "int",
	Blt_Offset(TextSwitches, alpha),  0, 0, &alphaSwitch},
    {BLT_SWITCH_CUSTOM,  "-anchor",   "anchor",
	Blt_Offset(TextSwitches, anchor), 0, 0, &anchorSwitch},
    {BLT_SWITCH_CUSTOM,  "-color",    "colorName",
	Blt_Offset(TextSwitches, color),  0, 0, &colorSwitch},
    {BLT_SWITCH_OBJ,     "-font",     "fontName",
	Blt_Offset(TextSwitches, fontObjPtr), 0},
    {BLT_SWITCH_BOOLEAN, "-kerning",  "bool",
	Blt_Offset(TextSwitches, kerning),  0},
    {BLT_SWITCH_FLOAT,  "-rotate",   "angle",
	Blt_Offset(TextSwitches, angle), 0},
    {BLT_SWITCH_INT,     "-size",     "number",
	Blt_Offset(TextSwitches, fontSize),  0}, 
    {BLT_SWITCH_CUSTOM, "-shadow", "offset",
        Blt_Offset(TextSwitches, shadow), 0, 0, &shadowSwitch},
    {BLT_SWITCH_END}
};

#ifdef HAVE_FT2BUILD_H
static void
MeasureText(FtFont *fontPtr, const char *string, size_t length,
	    size_t *widthPtr, size_t *heightPtr);

static size_t GetTextWidth(FtFont *fontPtr, const char *string, size_t length, 
	int kerning);
#endif /*HAVE_FT2BUILD_H*/

/*
 *---------------------------------------------------------------------------
 *
 * ArrayFreeProc --
 *
 *	Free the storage associated with the -table switch.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
ArrayFreeProc(char *record, int offset, int flags)
{
    Array *arrayPtr = (Array *)(record + offset);

    if (arrayPtr->values != NULL) {
	Blt_Free(arrayPtr->values);
    }
    arrayPtr->values = NULL;
    arrayPtr->nValues = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * ArraySwitchProc --
 *
 *	Convert a Tcl_Obj list of numbers into an array of floats.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ArraySwitchProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    const char *switchName,	/* Not used. */
    Tcl_Obj *objPtr,		/* String representation */
    char *record,		/* Structure record */
    int offset,			/* Offset to field in structure */
    int flags)			/* Not used. */
{
    Tcl_Obj **objv;
    Array *arrayPtr = (Array *)(record + offset);
    float *values;
    int i;
    int objc;

    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    values = Blt_Malloc(sizeof(float) * objc);
    if (values == NULL) {
	Tcl_AppendResult(interp, "can't allocated coordinate array of ",
		Blt_Itoa(objc), " elements", (char *)NULL);
	return TCL_ERROR;
    }
    for (i = 0; i < objc; i++) {
	double x;

	if (Tcl_GetDoubleFromObj(interp, objv[i], &x) != TCL_OK) {
	    Blt_Free(values);
	    return TCL_ERROR;
	}
	values[i] = (float)x;
    }
    arrayPtr->values = values;
    arrayPtr->nValues = objc;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * AlphaSwitch --
 *
 *	Convert a Tcl_Obj representing a number for the alpha value.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
AlphaSwitch(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    const char *switchName,	/* Not used. */
    Tcl_Obj *objPtr,		/* String representation */
    char *record,		/* Structure record */
    int offset,			/* Offset to field in structure */
    int flags)			/* Not used. */
{
    int *alphaPtr = (int *)(record + offset);
    int value;

    if (Tcl_GetIntFromObj(interp, objPtr, &value) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((value < 0) || (value > 255)) {
	Tcl_AppendResult(interp, "bad value \"", Tcl_GetString(objPtr), 
		"\" for alpha: must be 0..255", (char *)NULL);
	return TCL_ERROR;
    }
    *alphaPtr = value;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * AnchorSwitch --
 *
 *	Convert a Tcl_Obj representing an anchor.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
AnchorSwitch(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    const char *switchName,	/* Not used. */
    Tcl_Obj *objPtr,		/* String representation */
    char *record,		/* Structure record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Tk_Anchor *anchorPtr = (Tk_Anchor *)(record + offset);

    if (Tk_GetAnchorFromObj(interp, objPtr, anchorPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ShadowSwitch --
 *
 *	Convert a Tcl_Obj representing a number for the alpha value.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ShadowSwitch(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    const char *switchName,	/* Not used. */
    Tcl_Obj *objPtr,		/* String representation */
    char *record,		/* Structure record */
    int offset,			/* Offset to field in structure */
    int flags)			/* Not used. */
{
    Shadow *shadowPtr = (Shadow *)(record + offset);
    int value, alpha;
    int objc;
    Tcl_Obj **objv;

    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((objc != 1) && (objc != 2)) {
	Tcl_AppendResult(interp, "bad shadow list \"", Tcl_GetString(objPtr), 
		"\": should be \"offset ?alpha?\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (Tcl_GetIntFromObj(interp, objv[0], &value) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((value < 0) || (value > 20)) {
	Tcl_AppendResult(interp, "bad offset value \"", 
		Tcl_GetString(objv[0]), "\": must be 0..20", (char *)NULL);
	return TCL_ERROR;
    }
    alpha = 0xA0;
    if (objc == 2) {
	if (Tcl_GetIntFromObj(interp, objv[1], &alpha) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((alpha < 0) || (alpha > 255)) {
	    Tcl_AppendResult(interp, "bad value \"", Tcl_GetString(objv[1]), 
		"\" for alpha: must be 0..255", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    shadowPtr->offset = (unsigned int)value;
    shadowPtr->alpha = (unsigned int)alpha;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_ColorSwitchProc --
 *
 *	Convert a Tcl_Obj representing a Blt_Pixel color.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_ColorSwitchProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    const char *switchName,	/* Not used. */
    Tcl_Obj *objPtr,		/* String representation */
    char *record,		/* Structure record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Blt_Pixel *pixelPtr = (Blt_Pixel *)(record + offset);
    const char *string;

    string = Tcl_GetString(objPtr);
    if (string[0] == '\0') {
	pixelPtr->u32 = 0x00;
	return TCL_OK;
    }
    if (Blt_GetPixelFromObj(interp, objPtr, pixelPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#ifdef HAVE_FT2BUILD_H
/*
 *---------------------------------------------------------------------------
 *
 * CreateSimpleTextLayout --
 *
 *	Get the extents of a possibly multiple-lined text string.
 *
 * Results:
 *	Returns via *widthPtr* and *heightPtr* the dimensions of the text
 *	string.
 *
 *---------------------------------------------------------------------------
 */
static TextLayout *
CreateSimpleTextLayout(FtFont *fontPtr, const char *text, size_t textLen, 
		       TextStyle *tsPtr)
{
    TextFragment *fp;
    TextLayout *layoutPtr;
    size_t count;		/* Count # of characters on each line */
    int lineHeight;
    int maxHeight, maxWidth;
    int nFrags;
    int width;			/* Running dimensions of the text */
    const char *p, *endp, *start;
    int i;
    size_t size;

    nFrags = 0;
    endp = text + ((textLen < 0) ? strlen(text) : textLen);
    for (p = text; p < endp; p++) {
	if (*p == '\n') {
	    nFrags++;
	}
    }
    if ((p != text) && (*(p - 1) != '\n')) {
	nFrags++;
    }
    size = sizeof(TextLayout) + (sizeof(TextFragment) * (nFrags - 1));

    layoutPtr = Blt_AssertCalloc(1, size);
    layoutPtr->nFrags = nFrags;

    nFrags = count = 0;
    width = maxWidth = 0;
    maxHeight = tsPtr->padTop;
    lineHeight = fontPtr->height;

    fp = layoutPtr->fragments;
    for (p = start = text; p < endp; p++) {
	if (*p == '\n') {
	    size_t w;

	    if (count > 0) {
		w = GetTextWidth(fontPtr, start, count, 1);
		if (w > maxWidth) {
		    maxWidth = w;
		}
	    } else {
		w = 0;
	    }
	    fp->width = w;
	    fp->count = count;
	    fp->sy = fp->y = maxHeight + fontPtr->ascent;
	    fp->text = start;
	    maxHeight += lineHeight;
	    fp++;
	    nFrags++;
	    start = p + 1;	/* Start the text on the next line */
	    count = 0;		/* Reset to indicate the start of a new
				 * line */
	    continue;
	}
	count++;
    }
    if (nFrags < layoutPtr->nFrags) {
	size_t w;

	w = GetTextWidth(fontPtr, start, count, 1);
	if (w > maxWidth) {
	    maxWidth = w;
	}
	fp->width = w;
	fp->count = count;
	fp->sy = fp->y = maxHeight + fontPtr->ascent;
	fp->text = start;
	maxHeight += lineHeight;
	nFrags++;
    }
    maxHeight += tsPtr->padBottom;
    maxWidth += PADDING(tsPtr->xPad);
    fp = layoutPtr->fragments;
    for (i = 0; i < nFrags; i++, fp++) {
	switch (tsPtr->justify) {
	default:
	case TK_JUSTIFY_LEFT:
	    /* No offset for left justified text strings */
	    fp->x = fp->sx = tsPtr->padLeft;
	    break;
	case TK_JUSTIFY_RIGHT:
	    fp->x = fp->sx = (maxWidth - fp->width) - tsPtr->padRight;
	    break;
	case TK_JUSTIFY_CENTER:
	    fp->x = fp->sx = (maxWidth - fp->width) / 2;
	    break;
	}
    }
    if (tsPtr->underline >= 0) {
	fp = layoutPtr->fragments;
	for (i = 0; i < nFrags; i++, fp++) {
	    int first, last;

	    first = fp->text - text;
	    last = first + fp->count;
	    if ((tsPtr->underline >= first) && (tsPtr->underline < last)) {
		layoutPtr->underlinePtr = fp;
		layoutPtr->underline = tsPtr->underline - first;
		break;
	    }
	}
    }
    layoutPtr->width = maxWidth;
    layoutPtr->height = maxHeight - tsPtr->leader;
    return layoutPtr;
}
#endif

static void INLINE 
PutPixel(Pict *destPtr, int x, int y, Blt_Pixel *colorPtr)  
{
    if ((x >= 0) && (x < destPtr->width) && (y >= 0) && (y < destPtr->height)) {
	Blt_Pixel *dp;

	dp = Blt_PicturePixel(destPtr, x, y);
	dp->u32 = colorPtr->u32; 
    }
}

static INLINE Blt_Pixel
PremultiplyAlpha(Blt_Pixel *colorPtr, unsigned int alpha)
{
    Blt_Pixel new;
    int t;

    new.u32 = colorPtr->u32;
    alpha = imul8x8(alpha, colorPtr->Alpha, t);
    if ((alpha != 0xFF) && (alpha != 0x00)) {
	new.Red = imul8x8(alpha, colorPtr->Red, t);
	new.Green = imul8x8(alpha, colorPtr->Green, t);
	new.Blue = imul8x8(alpha, colorPtr->Blue, t);
    }
    new.Alpha = alpha;
    return new;
}

static void INLINE
HorizLine(Pict *destPtr, int x1, int x2, int y, Blt_Pixel *colorPtr)  
{
    Blt_Pixel *destRowPtr;
    Blt_Pixel *dp, *dend;
    size_t length;

    if (x1 > x2) {
	int tmp;

	tmp = x1, x1 = x2, x2 = tmp;
    }
    destRowPtr = destPtr->bits + (destPtr->pixelsPerRow * y) + x1;
    length = x2 - x1 + 1;
    for (dp = destRowPtr, dend = dp + length; dp < dend; dp++) {
	dp->u32 = colorPtr->u32;
    }
}

static void INLINE 
VertLine(Pict *destPtr, int x, int y1, int y2, Blt_Pixel *colorPtr)  
{
    Blt_Pixel *dp;
    int y;

    if (y1 > y2) {
	int tmp;

	tmp = y1, y1 = y2, y2 = tmp;
    }
    dp = destPtr->bits + (destPtr->pixelsPerRow * y1) + x;
    for (y = y1; y <= y2; y++) {
	dp->u32 = colorPtr->u32;
	dp += destPtr->pixelsPerRow;
    }
}

static INLINE void
BlendPixel(Blt_Pixel *bgPtr, Blt_Pixel *colorPtr, unsigned char weight)
{
    unsigned char alpha;
    int t1;

    alpha = imul8x8(colorPtr->Alpha, weight, t1);
    if (alpha == 0xFF) {
	bgPtr->u32 = colorPtr->u32;
    } else if (alpha != 0x00) {
	unsigned char beta;
	int t2;

	beta = alpha ^ 0xFF;
	bgPtr->Red   = imul8x8(alpha, colorPtr->Red, t1) + 
	    imul8x8(beta, bgPtr->Red, t2);
	bgPtr->Green = imul8x8(alpha, colorPtr->Green, t1) + 
	    imul8x8(beta, bgPtr->Green, t2);
	bgPtr->Blue  = imul8x8(alpha, colorPtr->Blue, t1)  + 
	    imul8x8(beta, bgPtr->Blue, t2);
	bgPtr->Alpha = alpha + imul8x8(beta, bgPtr->Alpha, t2);
    }
}
    
static void 
PaintLineSegment(
    Pict *destPtr,
    int x1, int y1, 
    int x2, int y2, 
    int lineWidth,
    Blt_Pixel *colorPtr)
{
    int dx, dy, xDir;
    unsigned long error;
    Blt_Pixel edge;

    if (y1 > y2) {
	int tmp;

	tmp = y1, y1 = y2, y2 = tmp;
	tmp = x1, x1 = x2, x2 = tmp;
    }
    edge = PremultiplyAlpha(colorPtr, 255);
    /* First and last Pixels always get Set: */
    PutPixel(destPtr, x1, y1, &edge);
    PutPixel(destPtr, x2, y2, &edge);

    dx = x2 - x1;
    dy = y2 - y1;

    if (dx >= 0) {
	xDir = 1;
    } else {
	xDir = -1;
	dx = -dx;
    }
    if (dy == 0) {		/* Horizontal line */
	HorizLine(destPtr, x1, x2, y1, &edge);
	return;
    }
    if (dx == 0) {		/*  Vertical line */
	VertLine(destPtr, x1, y1, y2, &edge);
	return;
    }
    if (dx == dy) {		/* Diagonal line. */
	Blt_Pixel *dp;

	dp = Blt_PicturePixel(destPtr, x1, y1);
	while(dy-- > 0) {
	    dp += destPtr->pixelsPerRow + xDir;
	    dp->u32 = colorPtr->u32;
	}
	return;
    }

    /* use Wu Antialiasing: */

    error = 0;
    if (dy > dx) {		/* y-major line */
	unsigned long adjust;

	/* x1 -= lineWidth / 2; */
	adjust = (dx << 16) / dy;
	while(--dy) {
	    unsigned int weight;
	    
	    error += adjust;
	    ++y1;
	    if (error & ~0xFFFF) {
		x1 += xDir;
		error &= 0xFFFF;
	    }
	    weight = (unsigned char)(error >> 8);
	    edge = PremultiplyAlpha(colorPtr, ~weight);
	    PutPixel(destPtr, x1, y1, &edge);
	    edge = PremultiplyAlpha(colorPtr, weight);
	    PutPixel(destPtr, x1 + xDir, y1, &edge);
	}
    } else {			/* x-major line */
	unsigned long adjust;

	/* y1 -= lineWidth / 2; */
	adjust = (dy << 16) / dx;
	while (--dx) {
	    unsigned int weight;

	    error += adjust;
	    x1 += xDir;
	    if (error & ~0xFFFF) {
		y1++;
		error &= 0xFFFF;
	    }
	    weight = (error >> 8) & 0xFF;
	    edge = PremultiplyAlpha(colorPtr, ~weight);
	    PutPixel(destPtr, x1, y1, &edge);
	    edge = PremultiplyAlpha(colorPtr, weight);
	    PutPixel(destPtr, x1, y1 + 1, &edge);
	}
    }
}

#include "bltPaintDraw.c"

static void 
PaintLineSegment2(
    Pict *destPtr,
    int x1, int y1, 
    int x2, int y2, 
    int cw,
    Blt_Pixel *colorPtr)
{
    Blt_Pixel interior;
    int dx, dy, xDir;
    unsigned long error;

    if (y1 > y2) {
	int tmp;

	tmp = y1, y1 = y2, y2 = tmp;
	tmp = x1, x1 = x2, x2 = tmp;
	cw = !cw;
    } 
    if (x1 > x2) {
	cw = !cw;
    }
    interior = PremultiplyAlpha(colorPtr, 255);
    /* First and last Pixels always get Set: */
    PutPixel(destPtr, x1, y1, &interior);
    PutPixel(destPtr, x2, y2, &interior);

    dx = x2 - x1;
    dy = y2 - y1;

    if (dx >= 0) {
	xDir = 1;
    } else {
	xDir = -1;
	dx = -dx;
    }
    if (dx == 0) {		/*  Vertical line */
	VertLine(destPtr, x1, y1, y2, &interior);
	return;
    }
    if (dy == 0) {		/* Horizontal line */
	HorizLine(destPtr, x1, x2, y1, &interior);
	return;
    }
    if (dx == dy) {		/* Diagonal line. */
	Blt_Pixel *dp;

	dp = Blt_PicturePixel(destPtr, x1, y1);
	while(dy-- > 0) {
	    dp += destPtr->pixelsPerRow + xDir;
	    dp->u32 = interior.u32;
	}
	return;
    }

    /* use Wu Antialiasing: */

    error = 0;
    if (dy > dx) {		/* y-major line */
	unsigned long adjust;

	/* x1 -= lineWidth / 2; */
	adjust = (dx << 16) / dy;
	while(--dy) {
	    Blt_Pixel *dp;
	    int x;
	    unsigned char weight;
	    
	    error += adjust;
	    ++y1;
	    if (error & ~0xFFFF) {
		x1 += xDir;
		error &= 0xFFFF;
	    }
	    dp = Blt_PicturePixel(destPtr, x1, y1);
	    weight = (unsigned char)(error >> 8);
	    x = x1;
	    if (x >= 0) {
		if (cw) {
		    *dp = PremultiplyAlpha(colorPtr, weight ^ 0xFF);
		} else {
		    *dp = interior;
		}
	    }
	    x += xDir;
	    dp += xDir;
	    if (x >= 0) {
		if (!cw) {
		    *dp = PremultiplyAlpha(colorPtr, weight);
		} else {
		    *dp = interior;
		}
	    }
	}
    } else {			/* x-major line */
	unsigned long adjust;

	/* y1 -= lineWidth / 2; */
	adjust = (dy << 16) / dx;
	while (--dx) {
	    Blt_Pixel *dp;
	    int y;
	    unsigned char weight;

	    error += adjust;
	    x1 += xDir;
	    if (error & ~0xFFFF) {
		y1++;
		error &= 0xFFFF;
	    }
	    dp = Blt_PicturePixel(destPtr, x1, y1);
	    weight = (unsigned char)(error >> 8);
	    y = y1;
	    if (y >= 0) {
		if (!cw) {
		    *dp = PremultiplyAlpha(colorPtr, weight ^ 0xFF);
		} else {
		    *dp = interior;
		}
	    }
	    dp += destPtr->pixelsPerRow;
	    y++;
	    if (y >= 0) {
		if (cw) {
		    *dp = PremultiplyAlpha(colorPtr, weight);
		} else {
		    *dp = interior;
		}
	    } 
	}
    }
    destPtr->flags |= (BLT_PIC_BLEND | BLT_PIC_ASSOCIATED_COLORS);
}


static void 
PaintPolyline(
    Pict *destPtr,
    int nPoints, 
    Point2f *points, 
    int lineWidth,
    Blt_Pixel *colorPtr)
{
    int i;
    Region2d r;
    Point2f p;

    r.left = r.top = 0;
    r.right = destPtr->width - 1;
    r.bottom = destPtr->height - 1;
    p.x = points[0].x, p.y = points[0].y;
    for (i = 1; i < nPoints; i++) {
	Point2f q, next;

	q.x = points[i].x, q.y = points[i].y;
	next = q;
	PaintLineSegment(destPtr, ROUND(p.x), ROUND(p.y), ROUND(q.x), 
			  ROUND(q.y), 0, colorPtr);
#ifdef notdef
	if (Blt_LineRectClip(&r, &p, &q)) {
	    PaintLineSegment(destPtr, ROUND(p.x), ROUND(p.y), ROUND(q.x), 
		ROUND(q.y), 1, colorPtr);
	}
#endif
	p = next;
    }
}

#ifdef HAVE_FT2BUILD_H

#undef __FTERRORS_H__
#define FT_ERRORDEF( e, v, s )  { e, s },
#define FT_ERROR_START_LIST     {
#define FT_ERROR_END_LIST       { -1, 0 } };

static const char *
FtError(FT_Error fterr)
{
    struct ft_errors {                                          
	int          code;             
	const char*  msg;
    };
    static struct ft_errors ft_err_mesgs[] = 
#include FT_ERRORS_H            

    struct ft_errors *fp;
    for (fp = ft_err_mesgs; fp->msg != NULL; fp++) {
	if (fp->code == fterr) {
	    return fp->msg;
	}
    }
    return "unknown Freetype error";
}

static void
MeasureText(FtFont *fontPtr, const char *string, size_t length,
	    size_t *widthPtr, size_t *heightPtr)
{
    FT_Vector pen;		/* Untransformed origin  */
    FT_GlyphSlot  slot;
    FT_Matrix matrix;		/* Transformation matrix. */
    int maxX, maxY;
    const char *p, *pend;
    double radians;
    int x;
    FT_Face face;

    radians = 0.0;
    matrix.yy = matrix.xx = (FT_Fixed)(cos(radians) * 65536.0);
    matrix.yx = (FT_Fixed)(sin(radians) * 65536.0);
    matrix.xy = -matrix.yx;

    face = fontPtr->face;
    slot = face->glyph;
    
    maxY = maxX = 0;
    pen.y = 0;
    x = 0;
    fprintf(stderr, "face->height=%d, face->size->height=%d\n",
	    face->height, (int)face->size->metrics.height);
    fprintf(stderr, "face->ascender=%d, face->descender=%d\n",
	    face->ascender, face->descender);
    for (p = string, pend = p + length; p < pend; p++) {
	maxY += face->size->metrics.height;
	pen.x = x << 6;
	while ((*p != '\n') && (p < pend)) {
	    FT_Set_Transform(face, &matrix, &pen);
	    /* Load glyph image into the slot (erase previous) */
	    if (FT_Load_Char(face, *p, FT_LOAD_RENDER)) {
		fprintf(stderr, "can't load character \"%c\"\n", *p);
		continue;                 /* ignore errors */
	    }
	    pen.x += slot->advance.x;
	    pen.y += slot->advance.y;
	    p++;
	}
	if (pen.x > maxX) {
	    maxX = pen.x;
	}
    }	
    fprintf(stderr, "w=%d,h=%d\n", maxX >> 6, maxY >> 6);
    *widthPtr = (size_t)(maxX >> 6);
    *heightPtr = (size_t)(maxY >> 6);
    fprintf(stderr, "w=%d,h=%d\n", *widthPtr, *heightPtr);
}

static size_t
GetTextWidth(FtFont *fontPtr, const char *string, size_t length, int kerning)
{
    FT_Vector pen;		/* Untransformed origin  */
    FT_GlyphSlot  slot;
    FT_Matrix matrix;		/* Transformation matrix. */
    int maxX, maxY;
    const char *p, *pend;
    double radians;
    FT_Face face;
    int previous;
    int fterr;

    radians = 0.0;
    matrix.yy = matrix.xx = (FT_Fixed)(cos(radians) * 65536.0);
    matrix.yx = (FT_Fixed)(sin(radians) * 65536.0);
    matrix.xy = -matrix.yx;

    face = fontPtr->face;
    slot = face->glyph;
    
    maxY = maxX = 0;
    pen.y = pen.x = 0;
    fprintf(stderr, "face->height=%d, face->size->height=%d\n",
	    face->height, (int)face->size->metrics.height);
    for (p = string, pend = p + length; p < pend; p++) {
	int current;

	current = FT_Get_Char_Index(face, *p);
	if ((kerning) && (previous >= 0)) { 
	    FT_Vector delta; 
		
	    FT_Get_Kerning(face, previous, current, FT_KERNING_DEFAULT, &delta);
	    pen.x += delta.x; 
	} 
	FT_Set_Transform(face, &matrix, &pen);
	previous = current;
	/* load glyph image into the slot (erase previous one) */  
	fterr = FT_Load_Glyph(face, current, FT_LOAD_DEFAULT); 
	if (fterr) {
	    fprintf(stderr, "can't load character \"%c\": %s\n", *p,
		    FtError(fterr));
	    continue;                 /* ignore errors */
	}
	pen.x += slot->advance.x;
	pen.y += slot->advance.y;
	if (pen.x > maxX) {
	    maxX = pen.x;
	}
    }	
    return maxX >> 6;
}


static void
BlitGlyph(Pict *destPtr, 
	  FT_GlyphSlot slot, 
	  int dx, int dy,
	  int xx, int yy,
	  Blt_Pixel *colorPtr)
{
    int gx, gy, gw, gh; 

    fprintf(stderr, "dx=%d, dy=%d\n", dx, dy);
    fprintf(stderr, "  slot.bitmap.width=%d\n", (int)slot->bitmap.width);
    fprintf(stderr, "  slot.bitmap.rows=%d\n", slot->bitmap.rows);
    fprintf(stderr, "  slot.bitmap_left=%d\n", (int)slot->bitmap_left);
    fprintf(stderr, "  slot.bitmap_top=%d\n", (int)slot->bitmap_top);
    fprintf(stderr, "  slot.bitmap.pixel_mode=%x\n", slot->bitmap.pixel_mode);
    fprintf(stderr, "  slot.advance.x=%d\n", (int)slot->advance.x);
    fprintf(stderr, "  slot.advance.y=%d\n", (int)slot->advance.y);
    
    fprintf(stderr, "  slot.format=%c%c%c%c\n", 
	    (slot->format >> 24) & 0xFF, 
	    (slot->format >> 16) & 0xFF, 
	    (slot->format >> 8) & 0xFF, 
	    (slot->format & 0xFF));
    if ((dx >= destPtr->width) || ((dx + slot->bitmap.width) <= 0) ||
	(dy >= destPtr->height) || ((dy + slot->bitmap.rows) <= 0)) {
	return;			/* No portion of the glyph is visible in the
				 * picture. */
    }
    /* By default, set the region to cover the entire glyph */
    gx = 0;
    gy = 0;
    gw = slot->bitmap.width;
    gh = slot->bitmap.rows;

    /* Determine the portion of the glyph inside the picture. */

    if (dx < 0) {		/* Left side of glyph overhangs. */
	gx -= dx;		
	gw += dx;
	dx = 0;		
    }
    if (dy < 0) {		/* Top of glyph overhangs. */
	gy -= dy;
	gh += dy;
	dy = 0;
    }
    if ((dx + gw) > destPtr->width) { /* Right side of glyph overhangs. */
	gw = destPtr->width - dx;
    }
    if ((dy + gh) > destPtr->height) { /* Bottom of glyph overhangs. */
	gh = destPtr->height - dy;
    }
    if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
	Blt_Pixel *destRowPtr;
	unsigned char *srcRowPtr;
	int y;

	srcRowPtr = slot->bitmap.buffer + (gy * slot->bitmap.pitch);
	destRowPtr = Blt_PicturePixel(destPtr, xx, yy);
	for (y = gy; y < gh; y++) {
	    Blt_Pixel *dp, *dend;
	    int x;
	    
	    for (dp = destRowPtr, dend = dp+gw, x = gx; dp < dend; x++, dp++) {
		int pixel;

		pixel = srcRowPtr[x >> 3] & (1 << (7 - (x & 0x7)));
		if (pixel != 0x0) {
		    BlendPixel(dp, colorPtr, 0xFF);
		}
	    }
	    srcRowPtr += slot->bitmap.pitch;
	    destRowPtr += destPtr->pixelsPerRow;
	}
    } else {
	Blt_Pixel *destRowPtr;
	unsigned char *srcRowPtr;
	int y;

	srcRowPtr = slot->bitmap.buffer + ((gy * slot->bitmap.pitch) + gx);
	destRowPtr = Blt_PicturePixel(destPtr, dx, dy);
	for (y = gy; y < gh; y++) {
	    Blt_Pixel *dp;
	    unsigned char *sp, *send;
	    
	    dp = destRowPtr;
	    for (sp = srcRowPtr, send = sp + gw; sp < send; sp++, dp++) {
		if (*sp != 0x0) {
		    BlendPixel(dp, colorPtr, *sp);
		}
	    }
	    srcRowPtr += slot->bitmap.pitch;
	    destRowPtr += destPtr->pixelsPerRow;
	}
    }
}

static void
CopyGrayGlyph(Pict *destPtr, FT_GlyphSlot slot, int x, int y, 
	      Blt_Pixel *colorPtr)
{
    int gx, gy, gw, gh; 

#ifdef notdef
    fprintf(stderr, "x=%d, y=%d\n", x, y);
    fprintf(stderr, "  slot.bitmap.width=%d\n", slot->bitmap.width);
    fprintf(stderr, "  slot.bitmap.rows=%d\n", slot->bitmap.rows);
    fprintf(stderr, "  slot.bitmap_left=%d\n", slot->bitmap_left);
    fprintf(stderr, "  slot.bitmap_top=%d\n", slot->bitmap_top);
    fprintf(stderr, "  slot.bitmap.pixel_mode=%x\n", slot->bitmap.pixel_mode);
    fprintf(stderr, "  slot.advance.x=%d\n", slot->advance.x);
    fprintf(stderr, "  slot.advance.y=%d\n", slot->advance.y);
    
    fprintf(stderr, "  slot.format=%c%c%c%c\n", 
	    (slot->format >> 24) & 0xFF, 
	    (slot->format >> 16) & 0xFF, 
	    (slot->format >> 8) & 0xFF, 
	    (slot->format & 0xFF));
#endif
    if ((x >= destPtr->width) || ((x + slot->bitmap.width) <= 0) ||
	(y >= destPtr->height) || ((y + slot->bitmap.rows) <= 0)) {
	return;			/* No portion of the glyph is visible in the
				 * picture. */
    }

    /* By default, set the region to cover the entire glyph */
    gx = 0;
    gy = 0;
    gw = slot->bitmap.width;
    gh = slot->bitmap.rows;
    fprintf(stderr, "before: x=%d,y=%d, gx=%d, gy=%d gw=%d gh=%d\n",
	    x, y, gx, gy, gw, gh);

    /* Determine the portion of the glyph inside the picture. */

    if (x < 0) {		/* Left side of glyph overhangs. */
	gx -= x;		
	gw += x;
	x = 0;		
    }
    if (y < 0) {		/* Top of glyph overhangs. */
	gy -= y;
	gh += y;
	y = 0;
    }
    if ((x + gw) > destPtr->width) { /* Right side of glyph overhangs. */
	gw = destPtr->width - x;
    }
    if ((y + gh) > destPtr->height) { /* Bottom of glyph overhangs. */
	gh = destPtr->height - y;
    }
    fprintf(stderr, "after: x=%d,y=%d, gx=%d, gy=%d gw=%d gh=%d\n",
	    x, y, gx, gy, gw, gh);
    {
	Blt_Pixel *destRowPtr;
	unsigned char *srcRowPtr;

	srcRowPtr = slot->bitmap.buffer + ((gy * slot->bitmap.pitch) + gx);
	destRowPtr = Blt_PicturePixel(destPtr, x, y);
	for (y = gy; y < gh; y++) {
	    Blt_Pixel *dp;
	    unsigned char *sp, *send;
	    
	    dp = destRowPtr;
	    for (sp = srcRowPtr, send = sp + gw; sp < send; sp++, dp++) {
		if (*sp != 0x0) {
		    int t, a;
		    a = imul8x8(*sp, colorPtr->Alpha, t);
		    dp->u32 = colorPtr->u32;
		    dp->Alpha = a;
		}
	    }
	    srcRowPtr += slot->bitmap.pitch;
	    destRowPtr += destPtr->pixelsPerRow;
	}
    }
}

static void
PaintGrayGlyph(Pict *destPtr, FT_GlyphSlot slot, int x, int y, 
	      Blt_Pixel *colorPtr)
{
    int gx, gy, gw, gh; 

#ifndef notdef
    fprintf(stderr, "x=%d, y=%d\n", x, y);
    fprintf(stderr, "  slot.bitmap.width=%d\n", slot->bitmap.width);
    fprintf(stderr, "  slot.bitmap.rows=%d\n", slot->bitmap.rows);
    fprintf(stderr, "  slot.bitmap_left=%d\n", slot->bitmap_left);
    fprintf(stderr, "  slot.bitmap_top=%d\n", slot->bitmap_top);
    fprintf(stderr, "  slot.bitmap.pixel_mode=%x\n", slot->bitmap.pixel_mode);
    fprintf(stderr, "  slot.advance.x=%d\n", slot->advance.x);
    fprintf(stderr, "  slot.advance.y=%d\n", slot->advance.y);
    
    fprintf(stderr, "  slot.format=%c%c%c%c\n", 
	    (slot->format >> 24) & 0xFF, 
	    (slot->format >> 16) & 0xFF, 
	    (slot->format >> 8) & 0xFF, 
	    (slot->format & 0xFF));
#endif
    if ((x >= destPtr->width) || ((x + slot->bitmap.width) <= 0) ||
	(y >= destPtr->height) || ((y + slot->bitmap.rows) <= 0)) {
	return;			/* No portion of the glyph is visible in the
				 * picture. */
    }

    /* By default, set the region to cover the entire glyph */
    gx = 0;
    gy = 0;
    gw = slot->bitmap.width;
    gh = slot->bitmap.rows;
    fprintf(stderr, "before: x=%d,y=%d, gx=%d, gy=%d gw=%d gh=%d\n",
	    x, y, gx, gy, gw, gh);

    /* Determine the portion of the glyph inside the picture. */

    if (x < 0) {		/* Left side of glyph overhangs. */
	gx -= x;		
	gw += x;
	x = 0;		
    }
    if (y < 0) {		/* Top of glyph overhangs. */
	gy -= y;
	gh += y;
	y = 0;
    }
    if ((x + gw) > destPtr->width) { /* Right side of glyph overhangs. */
	gw = destPtr->width - x;
    }
    if ((y + gh) > destPtr->height) { /* Bottom of glyph overhangs. */
	gh = destPtr->height - y;
    }
    fprintf(stderr, "after: x=%d,y=%d, gx=%d, gy=%d gw=%d gh=%d\n",
	    x, y, gx, gy, gw, gh);
    {
	Blt_Pixel *destRowPtr;
	unsigned char *srcRowPtr;

	srcRowPtr = slot->bitmap.buffer + ((gy * slot->bitmap.pitch) + gx);
	destRowPtr = Blt_PicturePixel(destPtr, x, y);
	for (y = gy; y < gh; y++) {
	    Blt_Pixel *dp;
	    unsigned char *sp, *send;
	    
	    dp = destRowPtr;
	    for (sp = srcRowPtr, send = sp + gw; sp < send; sp++, dp++) {
		if (*sp != 0x0) {
		    BlendPixel(dp, colorPtr, *sp);
		}
	    }
	    srcRowPtr += slot->bitmap.pitch;
	    destRowPtr += destPtr->pixelsPerRow;
	}
    }
}

static void
CopyMonoGlyph(Pict *destPtr, FT_GlyphSlot slot, int dx, int dy,
	  Blt_Pixel *colorPtr)
{
    int gx, gy, gw, gh; 

#ifdef notdef
    fprintf(stderr, "dx=%d, dy=%d\n", dx, dy);
    fprintf(stderr, "  slot.bitmap.width=%d\n", slot->bitmap.width);
    fprintf(stderr, "  slot.bitmap.rows=%d\n", slot->bitmap.rows);
    fprintf(stderr, "  slot.bitmap_left=%d\n", slot->bitmap_left);
    fprintf(stderr, "  slot.bitmap_top=%d\n", slot->bitmap_top);
    fprintf(stderr, "  slot.bitmap.pixel_mode=%x\n", slot->bitmap.pixel_mode);
    fprintf(stderr, "  slot.advance.x=%d\n", slot->advance.x);
    fprintf(stderr, "  slot.advance.y=%d\n", slot->advance.y);
    fprintf(stderr, "  slot.format=%c%c%c%c\n", 
	    (slot->format >> 24) & 0xFF, 
	    (slot->format >> 16) & 0xFF, 
	    (slot->format >> 8) & 0xFF, 
	    (slot->format & 0xFF));
#endif
    
    if ((dx >= destPtr->width) || ((dx + slot->bitmap.width) <= 0) ||
	(dy >= destPtr->height) || ((dy + slot->bitmap.rows) <= 0)) {
	return;			/* No portion of the glyph is visible in the
				 * picture. */
    }
    /* By default, set the region to cover the entire glyph */
    gx = 0;
    gy = 0;
    gw = slot->bitmap.width;
    gh = slot->bitmap.rows;

    /* Determine the portion of the glyph inside the picture. */

    if (dx < 0) {		/* Left side of glyph overhangs. */
	gx -= dx;		
	gw += dx;
	dx = 0;		
    }
    if (dy < 0) {		/* Top of glyph overhangs. */
	gy -= dy;
	gh += dy;
	dy = 0;
    }
    if ((dx + gw) > destPtr->width) { /* Right side of glyph overhangs. */
	gw = destPtr->width - dx;
    }
    if ((dy + gh) > destPtr->height) { /* Bottom of glyph overhangs. */
	gh = destPtr->height - dy;
    }
    {
	Blt_Pixel *destRowPtr;
	unsigned char *srcRowPtr;
	int y;

	srcRowPtr = slot->bitmap.buffer + (gy * slot->bitmap.pitch);
	destRowPtr = Blt_PicturePixel(destPtr, dx, dy);
	for (y = gy; y < gh; y++) {
	    Blt_Pixel *dp, *dend;
	    int x;
	    
	    for (dp = destRowPtr, dend = dp+gw, x = gx; dp < dend; x++, dp++) {
		int pixel;

		pixel = srcRowPtr[x >> 3] & (1 << (7 - (x & 0x7)));
		if (pixel != 0x0) {
		    dp->u32 = colorPtr->u32;
		    /* BlendPixel(dp, colorPtr, 0xFF); */
		}
	    }
	    srcRowPtr += slot->bitmap.pitch;
	    destRowPtr += destPtr->pixelsPerRow;
	}
    } 
}

static int 
ScaleFont(Tcl_Interp *interp, FtFont *fontPtr, size_t fontSize)
{
    if (fontSize > 0) {
	int fterr;
	unsigned int xdpi, ydpi;

	Blt_ScreenDPI(Tk_MainWindow(interp), &xdpi, &ydpi);
	fterr = FT_Set_Char_Size(fontPtr->face, 0, fontSize << 6, xdpi, ydpi);
	if (fterr) {
	    Tcl_AppendResult(interp, "can't set font size to \"", 
		Blt_Itoa(fontSize), "\": ", FtError(fterr), (char *)NULL);
	    return TCL_ERROR;
	}
    }
    fontPtr->height = fontPtr->face->size->metrics.height >> 6;
    fontPtr->ascent = fontPtr->face->size->metrics.ascender >> 6;
    fontPtr->descent = fontPtr->face->size->metrics.descender >> 6;
    return TCL_OK;
}

static void
RotateFont(FtFont *fontPtr, float angle)
{
    /* Set up the transformation matrix. */
    double radians;

    radians = (angle / 180.0) * M_PI;
    fontPtr->matrix.yy = fontPtr->matrix.xx = 
	(FT_Fixed)(cos(radians) * 65536.0);
    fontPtr->matrix.yx = (FT_Fixed)(sin(radians) * 65536.0);
    fontPtr->matrix.xy = -fontPtr->matrix.yx;
}

static FtFont *
OpenFont(Tcl_Interp *interp, const char *fontName, size_t fontSize) 
{
#ifdef HAVE_FT2BUILD_H
    FtFont *fontPtr;
    int fterr;

    fontPtr = Blt_AssertMalloc(sizeof(FtFont));

    fterr = FT_Init_FreeType(&fontPtr->lib);
    if (fterr) {
	Tcl_AppendResult(interp, "can't initialize freetype library: ", 
		FtError(fterr), (char *)NULL);
	return NULL;
    }
    fontPtr->face = NULL;
    if (fontName[0] == '@') {
	fterr = FT_New_Face(fontPtr->lib, fontName+1, 0, &fontPtr->face);
	if (fterr) {
	    Tcl_AppendResult(interp, "can't create face from font file \"", 
		fontName+1, "\": ", FtError(fterr), (char *)NULL);
	    return NULL;
	}
    } else {
#ifdef HAVE_LIBXFT
	fontPtr->xftFont = Blt_OpenXftFont(interp, fontName);
	if (fontPtr->xftFont == NULL) {
	    return NULL;
	}
	fontPtr->face = XftLockFace(fontPtr->xftFont);
	if (fontPtr->face == NULL) {
	    Tcl_AppendResult(interp, "no font specified for drawing operation", 
		(char *)NULL);
	    return NULL;
	}
#else
	Tcl_AppendResult(interp, "freetype library not available", 
		(char *)NULL);
	return NULL;
#endif
    }
    if (ScaleFont(interp, fontPtr, fontSize) != TCL_OK) {
	return NULL;
    }
    RotateFont(fontPtr, 0.0f);	/* Initializes the rotation matrix. */
    return fontPtr;
#else 
    Tcl_AppendResult(interp, "freetype library not available", 
		(char *)NULL);
    return NULL;
#endif /* HAVE_FT2BUILD_H */
}

static void
CloseFont(FtFont *fontPtr) 
{
#ifdef HAVE_LIBXFT
    if (fontPtr->xftFont != NULL) {
	XftUnlockFace(fontPtr->xftFont);
    } else {
	FT_Done_Face(fontPtr->face);
    }
#else
    FT_Done_Face(fontPtr->face);
#endif /* HAVE_LIBXFT */ 
    FT_Done_FreeType(fontPtr->lib);
}


static int
PaintText(
    Pict *destPtr,
    FtFont *fontPtr, 
    const char *string,
    size_t length,
    int x, int y,		/* Anchor coordinates of text. */
    int kerning,
    Blt_Pixel *colorPtr)
{
#ifdef HAVE_FT2BUILD_H
    int fterr;
    int h;

    FT_Vector pen;	/* Untransformed origin  */
    const char *p, *pend;
    FT_GlyphSlot  slot;
    FT_Face face;		/* Face object. */  

    h = destPtr->height;
    face = fontPtr->face;
    slot = face->glyph;
    int xx, yy;
    int previous;

    if (destPtr->flags & BLT_PIC_ASSOCIATED_COLORS) {
	Blt_UnassociateColors(destPtr);
    }
#ifdef notdef
    fprintf(stderr, 
	    "num_faces=%d\n"
	    "face_flags=%x\n"
	    "style_flags=%x\n"
	    "num_glyphs=%d\n"
	    "family_name=%s\n"
	    "style_name=%s\n"
	    "num_fixed_sizes=%d\n"
	    "num_charmaps=%d\n"
	    "units_per_EM=%d\n"
	    "face->size->metrics.height=%d\n"
	    "face->size->metrics.ascender=%d\n"
	    "face->size->metrics.descender=%d\n"
	    "ascender=%d\n"
	    "descender=%d\n"
	    "height=%d\n"
	    "max_advance_width=%d\n"
	    "max_advance_height=%d\n"
	    "underline_position=%d\n"
	    "underline_thickness=%d\n",
	    face->num_faces,
	    face->face_flags,
	    face->style_flags,
	    face->num_glyphs,
	    face->family_name,
	    face->style_name,
	    face->num_fixed_sizes,
	    face->num_charmaps,
	    face->units_per_EM,
	    face->size->metrics.height,
	    face->size->metrics.ascender,
	    face->size->metrics.descender,
	    face->ascender,
	    face->descender,
	    face->height,
	    face->max_advance_width,
	    face->max_advance_height,
	    face->underline_position,
	    (int)face->underline_thickness);
#endif
    xx = x, yy = y;
    previous = -1;
    FT_Set_Transform(face, &fontPtr->matrix, NULL);
    pen.y = (h - y) << 6;
    xx = x;
    pen.x = x << 6;
    for (p = string, pend = p + length; p < pend; p++) {
	int current;

	current = FT_Get_Char_Index(face, *p);
	if ((kerning) && (previous >= 0)) { 
	    FT_Vector delta; 
		
	    FT_Get_Kerning(face, previous, current, FT_KERNING_DEFAULT, &delta);
	    pen.x += delta.x; 
	} 
	FT_Set_Transform(face, &fontPtr->matrix, &pen);
	previous = current;
	/* load glyph image into the slot (erase previous one) */  
	fterr = FT_Load_Glyph(face, current, FT_LOAD_DEFAULT); 
	if (fterr) {
	    fprintf(stderr, "can't load character \"%c\": %s\n", *p,
		    FtError(fterr));
	    continue;                 /* ignore errors */
	}
	fterr = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
	if (fterr) {
	    fprintf(stderr, "can't render glyph \"%c\": %s\n", *p,
		    FtError(fterr));
	    continue;                 /* ignore errors */
	}
	switch(slot->bitmap.pixel_mode) {
	case FT_PIXEL_MODE_MONO:
	    CopyMonoGlyph(destPtr, slot, pen.x >> 6, yy - slot->bitmap_top, 
		colorPtr);
	    break;
	case FT_PIXEL_MODE_LCD:
	case FT_PIXEL_MODE_LCD_V:
	case FT_PIXEL_MODE_GRAY:
	    fprintf(stderr, "h=%d, slot->bitmap_top=%d\n", h, slot->bitmap_top);
	    PaintGrayGlyph(destPtr, slot, slot->bitmap_left, 
			   h - slot->bitmap_top, colorPtr);
	case FT_PIXEL_MODE_GRAY2:
	case FT_PIXEL_MODE_GRAY4:
	    break;
	}
	pen.x += slot->advance.x; 
	pen.y += slot->advance.y;
	previous = -1;
    }	
#endif /* HAVE_FT2BUILD_H */
    return TCL_OK;
}

#ifdef notdef
static void
PaintTextShadow(
    Blt_Picture picture,
    Tcl_Interp *interp,
    const char *string,
    int x, int y,		/* Anchor coordinates of text. */

    TextSwitches *switchesPtr,
    int offset)
{
    int w, h;
    Blt_Picture blur;
    Blt_Pixel color;

    MeasureText(FT_Face face, char *string, float angle, int *widthPtr, 
	    int *heightPtr)
    w = (width + offset*2);
    h = (height + offset*2);
    blur = Blt_CreatePicture(w, h);
    color.u32 = 0x00;
    Blt_BlankPicture(blur, &color);
    color.u32 = 0xA0000000;

    PaintText(blur, fontPtr, string, x+offset/2, y+offset/2, 0, colorPtr);

    PaintText(blur, interp, string, x+offset/2, y+offset/2, switchesPtr);
    Blt_BlurPicture(blur, blur, offset);
    Blt_BlendPictures(picture, blur, 0, 0, w, h, x+offset/2, y+offset/2);
    Blt_FreePicture(blur);
}
#endif
#endif /*HAVE_FT2BUILD_H*/

static void 
xPaintArc(Pict *destPtr, int x1, int y1, int x2, int y2, int lineWidth, 
	  Blt_Pixel *colorPtr)
{
    Blt_Pixel *dp;
    double t;
    int r2;
    int radius;
    int dx, dy;
    int x, y;
    int xoff, yoff;
    int fill = 1;

    t = 0.0;
    dx = x2 - x1;
    dy = y2 - y1;
    radius = MIN(dx, dy) / 2;
    xoff = x1;
    yoff = y1;
    x = radius;
    y = 0;
    dp = Blt_PicturePixel(destPtr, x + xoff - 1, y + yoff);
    dp->u32 = colorPtr->u32;
    r2 = radius * radius;
    if (fill) {
	PaintLineSegment(destPtr, x1, y + yoff, x + xoff - 2, y + yoff, 1, 
			 colorPtr);
    }
    while (x > y) {
	double z;
	double d, q;
	unsigned char a;

	y++;
	z = sqrt(r2 - (y * y));
	d = floor(z) - z;
	if (d < t) {
	    x--;
	}
	dp = Blt_PicturePixel(destPtr, x + xoff, y + yoff);
	q = FABS(d * 255.0);
	a = (unsigned int)CLAMP(q);
	BlendPixel(dp, colorPtr, a);
	dp--;			/* x - 1 */
	a = (unsigned int)CLAMP(255.0 - q);
	BlendPixel(dp, colorPtr, a);
	t = d;
	x1++;
	if (fill) {
	    PaintLineSegment(destPtr, x1, y + yoff, x + xoff - 1, y + yoff, 1, colorPtr);
	}
    }
}

static Point2d
PolygonArea(int nPoints, Point2d *points, double *areaPtr)
{
    Point2d *p, *pend;
    Point2d c;
    double area;
    int i;
    
    area = c.x = c.y = 0.0;
    for (p = points, pend = p + nPoints, i = 0; p < pend; p++, i++) {
	Point2d *q;
	double factor;
	int j;
	
	j = (i + 1) % nPoints;
	q = points + j;
	factor = (p->x * q->y) - (p->y * q->x);
	area += factor;
	c.x += (p->x + q->x) * factor;
	c.y += (p->y + q->y) * factor;
    }
    area *= 0.5;
    c.x /= 6.0 * area;
    c.y /= 6.0 * area;
    *areaPtr = area;
    return c;
}

static void
BlendLine(Pict *destPtr, int x1, int x2, int y, Blt_Pixel *colorPtr)  
{
    Blt_Pixel *destRowPtr;
    Blt_Pixel *dp, *dend;
    size_t length;

    if (x1 > x2) {
	int tmp;

	tmp = x1, x1 = x2, x2 = tmp;
    }
    destRowPtr = destPtr->bits + (destPtr->pixelsPerRow * y) + x1;
    length = x2 - x1 + 1;
    for (dp = destRowPtr, dend = dp + length; dp < dend; dp++) {
	BlendPixel(dp, colorPtr, 0xFF);
    }
}

/*
 * Concave Polygon Scan Conversion
 * by Paul Heckbert
 * from "Graphics Gems", Academic Press, 1990
 */

/*
 * concave: scan convert nvert-sided concave non-simple polygon with vertices at
 * (point[i].x, point[i].y) for i in [0..nvert-1] within the window win by
 * calling spanproc for each visible span of pixels.
 * Polygon can be clockwise or counterclockwise.
 * Algorithm does uniform point sampling at pixel centers.
 * Inside-outside test done by Jordan's rule: a point is considered inside if
 * an emanating ray intersects the polygon an odd number of times.
 * drawproc should fill in pixels from xl to xr inclusive on scanline y,
 * e.g:
 *	drawproc(y, xl, xr)
 *	int y, xl, xr;
 *	{
 *	    int x;
 *	    for (x=xl; x<=xr; x++)
 *		pixel_write(x, y, pixelvalue);
 *	}
 *
 *  Paul Heckbert	30 June 81, 18 Dec 89
 */

#include <stdio.h>
#include <math.h>

typedef struct {		/* a polygon edge */
    double x;	/* x coordinate of edge's intersection with current scanline */
    double dx;	/* change in x with respect to y */
    int i;	/* edge number: edge i goes from pt[i] to pt[i+1] */
} ActiveEdge;

typedef struct {
    int nActive;
    ActiveEdge *active;
} AET;

/* comparison routines for qsort */
static int n;			/* number of vertices */
static Point2f *pt;		/* vertices */

static int 
CompareIndices(const void *a, const void *b)
{
    return (pt[*(int *)a].y <= pt[*(int *)b].y) ? -1 : 1;
}

static int 
CompareActive(const void *a, const void *b)
{
    const ActiveEdge *u, *v;

    u = a, v = b;
    return (u->x <= v->x) ? -1 : 1;
}

static void
cdelete(AET *tablePtr, int i)	/* remove edge i from active list */
{
    int j;

    for (j=0; (j < tablePtr->nActive) && (tablePtr->active[j].i != i); j++) {
	/*empty*/
    }
    if (j >= tablePtr->nActive) {
	return;		      /* edge not in active list; happens at win->y0*/
    }
    tablePtr->nActive--;

    bcopy(&tablePtr->active[j+1], &tablePtr->active[j], 
	  (tablePtr->nActive-j) *sizeof tablePtr->active[0]);
}

/* append edge i to end of active list */
static void
cinsert(AET *tablePtr, size_t n, Point2f *points, int i, int y)
{
    int j;
    Point2f *p, *q;
    ActiveEdge *edgePtr;

    j = (i < (n - 1)) ? i + 1 : 0;
    if (points[i].y < points[j].y) {
	p = points + i, q = points + j;
    } else {
	p = points + j, q = points + i;
    }
    edgePtr = tablePtr->active + tablePtr->nActive;
    assert(tablePtr->nActive < n);
    /* initialize x position at intersection of edge with scanline y */
    edgePtr->dx = (q->x - p->x) / (q->y - p->y);
    edgePtr->x  = edgePtr->dx * (y + 0.5 - p->y) + p->x;
    edgePtr->i  = i;
    tablePtr->nActive++;
}

void
Blt_PaintPolygon(Pict *destPtr, int nVertices, Point2f *vertices, 
		 Blt_Pixel *colorPtr)
{
    int y, k;
    int top, bot;
    int *map;	  /* list of vertex indices, sorted by pt[map[j]].y */
    AET aet;

    n = nVertices;
    pt = vertices;
    if (n <= 0) {
	return;
    }
    if (destPtr->height == 0) {
	return;
    }
    map = Blt_AssertMalloc(nVertices * sizeof(unsigned int));
    aet.active = Blt_AssertCalloc(nVertices, sizeof(ActiveEdge));
    /* create y-sorted array of indices map[k] into vertex list */
    for (k = 0; k < n; k++) {
	map[k] = k;
    }
    /* sort map by pt[map[k]].y */
    qsort(map, n, sizeof(unsigned int), CompareIndices); 
    aet.nActive = 0;		/* start with empty active list */
    k = 0;			/* map[k] is next vertex to process */
    top = MAX(0, ceil(vertices[map[0]].y-.5)); /* ymin of polygon */
    bot = MIN(destPtr->height-1, floor(vertices[map[n-1]].y-.5)); /* ymax */
    for (y = top; y <= bot; y++) { /* step through scanlines */
	unsigned int i, j;

	/* Scanline y is at y+.5 in continuous coordinates */

	/* Check vertices between previous scanline and current one, if any */
	for (/*empty*/; (k < n) && (vertices[map[k]].y <= (y +.5)); k++) {
	    /* to simplify, if pt.y=y+.5, pretend it's above */
	    /* invariant: y-.5 < pt[i].y <= y+.5 */
	    i = map[k];	
	    /*
	     * Insert or delete edges before and after vertex i (i-1 to i, and
	     * i to i+1) from active list if they cross scanline y
	     */
	    /* vertex previous to i */
	    j = (i > 0) ? (i - 1) : (n - 1);
	    if (vertices[j].y <= (y - 0.5)) {	
		/* old edge, remove from active list */
		cdelete(&aet, j);
	    } else if (vertices[j].y > (y + 0.5)) { 
		/* new edge, add to active list */
		cinsert(&aet, nVertices, vertices, j, y);
	    }
	    j = (i < (n - 1)) ? (i + 1) : 0; /* vertex next after i */
	    if (vertices[j].y <= (y-.5)) { 
		/* old edge, remove from active list */
		cdelete(&aet, i);
	    } else if (vertices[j].y > (y+.5)){ 
		/* new edge, add to active list */
		cinsert(&aet, nVertices, vertices, i, y);
	    }
	}
	/* Sort active edge list by active[j].x */
	qsort(aet.active, aet.nActive, sizeof(ActiveEdge), CompareActive);

	/* Draw horizontal segments for scanline y */
	for (j = 0; j < aet.nActive; j += 2) {	/* draw horizontal segments */
	    int left, right;
	    ActiveEdge *p, *q;

	    p = aet.active + j;
	    q = p+1;
	    /* span 'tween j & j+1 is inside, span tween j+1 & j+2 is outside */
	    left  = (int)ceil (p->x - 0.5); /* left end of span */
	    right = (int)floor(q->x - 0.5); /* right end of span */
	    if (left < 0) {
		left = 0;
	    }
	    if (right >= (int)destPtr->width) {
		right = destPtr->width - 1;
	    }
	    if (left <= right) {
		BlendLine(destPtr, left, right, y, colorPtr);
	    }
	    p->x += p->dx;	/* increment edge coords */
	    q->x += q->dx;
	}
    }
    Blt_Free(aet.active);
    Blt_Free(map);
}

static void
GetPolygonBoundingBox(size_t nVertices, Point2f *vertices, Region2f *regionPtr)
{
    Point2f *pp, *pend;

    regionPtr->left = regionPtr->top = FLT_MAX;
    regionPtr->right = regionPtr->bottom = -FLT_MAX;
    for (pp = vertices, pend = pp + nVertices; pp < pend; pp++) {
	if (pp->x < regionPtr->left) {
	    regionPtr->left = pp->x;
	} else if (pp->x > regionPtr->right) {
	    regionPtr->right = pp->x;
	}
	if (pp->y < regionPtr->top) {
	    regionPtr->top = pp->y;
	} else if (pp->y > regionPtr->bottom) {
	    regionPtr->bottom = pp->y;
	}
    }
}

static void
TranslatePolygon(size_t nVertices, Point2f *vertices, float x, float y, 
		 float scale)
{
    Point2f *pp, *pend;

    for (pp = vertices, pend = pp + nVertices; pp < pend; pp++) {
	pp->x = (pp->x + x) * scale;
	pp->y = (pp->y + y) * scale;
    }
}

static void
PaintPolygonAA(Pict *destPtr, size_t nVertices, Point2f *vertices, 
	       Region2f *regionPtr, Blt_Pixel *colorPtr)
{
    Region2f r2;
    int w, h;
    Blt_Picture big, tmp;
    Blt_Pixel color;
    Point2f *v;

    int x1, x2, y1, y2;

    x1 = y1 = 0;
    x2 = destPtr->width, y2 = destPtr->height;
    if (regionPtr->left > 0) {
	x1 = (int)floor(regionPtr->left);
    }
    if (regionPtr->top > 0) {
	y1 = (int)floor(regionPtr->top);
    }
    if (regionPtr->right < x2) {
	double d;
	d = ceil(regionPtr->right);
	x2 = (int)ceil(regionPtr->right);
    }
    if (regionPtr->bottom < y2) {
	y2 = (int)ceil(regionPtr->bottom);
    }
    v = Blt_AssertMalloc(nVertices * sizeof(Point2f));
    memcpy(v, vertices, sizeof(Point2f) * nVertices);
    TranslatePolygon(nVertices, v, -x1+1, -y1+1, 4.0f);
    GetPolygonBoundingBox(nVertices, v, &r2);
    
    w = (x2 - x1 + 2) * 4;
    h = (y2 - y1 + 2) * 4;
    big = Blt_CreatePicture(w, h);
    color.u32 = 0x00;
    Blt_BlankPicture(big, &color);
    color.Alpha = colorPtr->Alpha;
    Blt_PaintPolygon(big, nVertices, v, &color);
    Blt_Free(v);
    w = (x2 - x1 + 2);
    h = (y2 - y1 + 2);
    tmp = Blt_CreatePicture(w, h);
    Blt_ResamplePicture(tmp, big, bltBoxFilter, bltBoxFilter);
    Blt_FreePicture(big);
    Blt_ApplyColorToPicture(tmp, colorPtr);
    /* Replace the bounding box in the original with the new. */
    Blt_BlendPictures(destPtr, tmp, 0, 0, w, h, (int)floor(regionPtr->left)-1, 
		      (int)floor(regionPtr->top)-1);
    Blt_FreePicture(tmp);
}

static void
PaintPolygonShadow(Pict *destPtr, size_t nVertices, Point2f *vertices, 
		   Region2f *regionPtr, Shadow *shadowPtr)
{
    int w, h;
    Blt_Picture blur, tmp;
    Point2f *v;
    Blt_Pixel color;
    int x1, x2, y1, y2;
    Region2f r2;

    x1 = y1 = 0;
    x2 = destPtr->width, y2 = destPtr->height;
    if (regionPtr->left > 0) {
	x1 = (int)regionPtr->left;
    }
    if (regionPtr->top > 0) {
	y1 = (int)regionPtr->top;
    }
    if (regionPtr->right < x2) {
	x2 = (int)ceil(regionPtr->right);
    }
    if (regionPtr->bottom < y2) {
	y2 = (int)ceil(regionPtr->bottom);
    }
    if ((x1 > 0) || (y1 > 0)) {
	v = Blt_AssertMalloc(nVertices * sizeof(Point2f));
	memcpy(v, vertices, sizeof(Point2f) * nVertices);
	TranslatePolygon(nVertices, v, -x1, -y1, 1.0f);
    } else {
	v = vertices;
    }
    w = (x2 - x1 + shadowPtr->offset*8);
    h = (y2 - y1 + shadowPtr->offset*8);
    tmp = Blt_CreatePicture(w, h);
    color.u32 = 0x00;
    Blt_BlankPicture(tmp, &color);
    color.Alpha = shadowPtr->alpha;
    GetPolygonBoundingBox(nVertices, v, &r2);
    Blt_PaintPolygon(tmp, nVertices, v, &color);
    if (v != vertices) {
	Blt_Free(v);
    }
    blur = Blt_CreatePicture(w, h);
    color.u32 = 0x00;
    Blt_BlankPicture(blur, &color);
    Blt_CopyPictureBits(blur, tmp, 0, 0, w, h, shadowPtr->offset*2, 
			shadowPtr->offset*2); 
    Blt_BlurPicture(blur, blur, shadowPtr->offset);
    Blt_MaskPicture(blur, tmp, 0, 0, w, h, 0, 0, &color);
    Blt_FreePicture(tmp);
    Blt_BlendPictures(destPtr, blur, 0, 0, w, h, x1, y1);
    Blt_FreePicture(blur);
}

static void
PaintPolygonAA2(Pict *destPtr, size_t nVertices, Point2f *vertices, 
		Region2f *regionPtr, Blt_Pixel *colorPtr, Shadow *shadowPtr)
{
    Region2f r2;
    Blt_Picture big, tmp;
    Blt_Pixel color;
    /* 
     * Get the minimum size region to draw both a supersized polygon and
     * shadow.
     *
     * Draw the shadow and then the polygon. Everything is 4x bigger including
     * the shadow offset.  This is a much bigger blur.
     * 
     * Resample the image back down to 1/4 the size and blend it into 
     * the destination picture.
     */
    big = Blt_CreatePicture(destPtr->width * 4, destPtr->height * 4);
    TranslatePolygon(nVertices, vertices, 0.0f, 0.0f, 4.0f);
    color.u32 = 0x00;
    Blt_BlankPicture(big, &color);
    GetPolygonBoundingBox(nVertices, vertices, &r2);
    PaintPolygonShadow(big, nVertices, vertices, &r2, shadowPtr);
    Blt_PaintPolygon(big, nVertices, vertices, colorPtr);
    tmp = Blt_CreatePicture(destPtr->width, destPtr->height);
    Blt_ResamplePicture(tmp, big, bltBoxFilter, bltBoxFilter);
    Blt_FreePicture(big);
    Blt_BlendPictures(destPtr, tmp, 0, 0, destPtr->width, destPtr->height, 0,0);
    Blt_FreePicture(tmp);
}

static void
DrawCircle(Blt_Picture picture, int x, int y, int radius, 
	    CircleSwitches *switchesPtr)
{
    int filled;

    filled = (switchesPtr->fill.u32 != 0x00);
    if (switchesPtr->antialiased) {
	int nSamples = 4; 
	Pict *bigPtr, *tmpPtr;
 	int w, h;
	Blt_Pixel color;
	int offset, r, lw;
	int cx, cy;

	r = radius * nSamples;
	w = h = r + r;
	offset = switchesPtr->shadow.offset * nSamples;

	/* Scale the region forming the bounding box of the ellipse into a new
	 * picture. The bounding box is scaled by *nSamples* times. */
	bigPtr = Blt_CreatePicture(w+(1+offset)*4, h+(1+offset)*4);
	cx = bigPtr->width / 2;
	cy = bigPtr->height / 2;
	
	color.u32 = 0x00;
	color.Alpha = 0x00;
	Blt_BlankPicture(bigPtr, &color);
	if (switchesPtr->shadow.offset > 0) {
	    color.Alpha = switchesPtr->shadow.alpha;
	    /* Either ring or full circle for blur stencil. */
	    lw = switchesPtr->lineWidth * nSamples;
	    if (filled) {
		lw = 0;
	    }
	    PaintEllipse(bigPtr, cx, cy, r, r, lw, &color, 0);
	    Blt_BlurPicture(bigPtr, bigPtr, offset/2);
	    /* Offset the circle from the shadow. */
	    cx -= offset;
	    cy -= offset;
	}
	lw = switchesPtr->lineWidth * nSamples;
	if ((lw > 0) && (r > lw) && (switchesPtr->outline.u32 != 0x00)) {
	    /* Paint ring outline. */
	    PaintEllipse(bigPtr, cx, cy, r, r, lw, &switchesPtr->outline, 0);
	    r -= lw;
	}
	if (filled) {
	    /* Paint filled interior */
	    PaintEllipse(bigPtr, cx, cy, r, r, 0, &switchesPtr->fill, 0);
	}
	offset = switchesPtr->shadow.offset;
	w = h = radius + radius + (1 + offset) * 2;
	tmpPtr = Blt_CreatePicture(w, h);
	Blt_ResamplePicture(tmpPtr, bigPtr, bltBoxFilter, bltBoxFilter);
#ifdef notdef
	fprintf(stderr, "big=%dx%d, blur=%d\n", bigPtr->width,bigPtr->height,
		(switchesPtr->shadow.offset * nSamples)/2);
#endif
	Blt_FreePicture(bigPtr);
	/*Blt_ApplyColorToPicture(tmpPtr, &switchesPtr->fill); */
	Blt_BlendPictures(picture, tmpPtr, 0, 0, w, h, x-w/2+offset, 
			  y-h/2+offset);
	Blt_FreePicture(tmpPtr);
    } else if (switchesPtr->shadow.offset > 0) {
	Pict *blurPtr;
	int w, h;
	Blt_Pixel color;
	int offset, r, lw;
	int cx, cy;

	w = h = (radius + radius);
	r = radius;
	offset = switchesPtr->shadow.offset;

	/* Scale the region forming the bounding box of the ellipse into a new
	 * picture. The bounding box is scaled by *nSamples* times. */
	blurPtr = Blt_CreatePicture(w+(offset*4), h+(offset*4));
	cx = blurPtr->width / 2;
	cy = blurPtr->height / 2;
	color.u32 = 0x00;
	Blt_BlankPicture(blurPtr, &color);

	color.Alpha = switchesPtr->shadow.alpha;
	/* Either ring or full circle for blur stencil. */
	lw = switchesPtr->lineWidth;
	if (filled) {
	    lw = 0;
	}
	PaintEllipse(blurPtr, cx, cy, r, r, lw, &color, 0);
	Blt_BlurPicture(blurPtr, blurPtr, offset);
	/* Offset the circle from the shadow. */
	cx -= offset;
	cy -= offset;
	lw = switchesPtr->lineWidth;
	if ((lw > 0) && (r > lw) && (switchesPtr->outline.u32 != 0x00)) {
	    /* Paint ring outline. */
	    PaintEllipse(blurPtr, cx, cy, r, r, lw, &switchesPtr->outline, 0); 
	    r -= lw;
	}
	if (filled) {
	    /* Paint filled interior */
	    PaintEllipse(blurPtr, cx, cy, r, r, 0, &switchesPtr->fill, 0);
	}
	x -= blurPtr->width/2 + offset;
	if (x < 0) {
	    x = 0;
	}
	y -= blurPtr->height/2 + offset;
	if (y < 0) {
	    y = 0;
	}
	Blt_BlendPictures(picture, blurPtr, 0, 0, blurPtr->width, 
			  blurPtr->height, x, y);
	Blt_FreePicture(blurPtr);
    } else {
	int r;

	r = radius;
	if ((switchesPtr->lineWidth > 0) && (r > switchesPtr->lineWidth)) {
	    /* Paint ring outline. */
	    PaintEllipse(picture, x, y, r, r, switchesPtr->lineWidth, 
		&switchesPtr->outline, 1); 
	    r -= switchesPtr->lineWidth;
	}
	if (filled) {
	    /* Paint filled interior */
	    PaintEllipse(picture, x, y, r, r, 0, &switchesPtr->fill, 1);
	}
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * CircleOp --
 *
 * Results:
 *	Returns a standard TCL return value.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static int
CircleOp(
    Blt_Picture picture,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument objects. */
{
    CircleSwitches switches;
    int x, y, radius;

    if (objc < 5) {
	Tcl_AppendResult(interp, "wrong # of coordinates for circle",
			 (char *)NULL);
	return TCL_ERROR;
    }
    if ((Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK) ||
	(Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK) ||
	(Tcl_GetIntFromObj(interp, objv[5], &radius) != TCL_OK)) {
	return TCL_ERROR;
    }
    /* Process switches  */
    switches.lineWidth = 1;
    switches.fill.u32    = 0xffffffff;
    switches.outline.u32 = 0xff000000;
    switches.alpha = -1;
    switches.antialiased = 0;
    switches.shadow.offset = 0;
    switches.shadow.alpha = 0xa0;
    if (Blt_ParseSwitches(interp, circleSwitches, objc - 6, objv + 6, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
#ifdef notdef
    if (switches.alpha != -1) {
	switches.fill.Alpha = switches.alpha;
	switches.outline.Alpha = switches.alpha;
    }
#endif
    DrawCircle(picture, x, y, radius, &switches);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * EllipseOp --
 *
 * Results:
 *	Returns a standard TCL return value.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static int
EllipseOp(
    Blt_Picture picture,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument objects. */
{
    CircleSwitches switches;
    int x, y, a, b;

    if (objc < 7) {
	Tcl_AppendResult(interp, "wrong # of coordinates for circle",
			 (char *)NULL);
	return TCL_ERROR;
    }
    if ((Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK) ||
	(Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK) ||
	(Tcl_GetIntFromObj(interp, objv[5], &a) != TCL_OK) ||
	(Tcl_GetIntFromObj(interp, objv[6], &b) != TCL_OK)) {
	return TCL_ERROR;
    }
    /* Process switches  */
    switches.lineWidth = 0;
    switches.fill.u32 = 0xFFFFFFFF;
    switches.outline.u32 = 0xFF000000;
    switches.alpha = -1;
    switches.antialiased = FALSE;
    if (Blt_ParseSwitches(interp, circleSwitches, objc - 7, objv + 7, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if (switches.alpha != -1) {
	switches.fill.Alpha = switches.alpha;
	switches.outline.Alpha = switches.alpha;
    }
    if ((switches.lineWidth >= a) || (switches.lineWidth >= b)) {
	/* If the requested line width is greater than the radius then draw a
	 * solid ellipse instead. */
	switches.lineWidth = 0;
    }
    if (switches.antialiased) {
	PaintEllipseAA(picture, x, y, a, b, switches.lineWidth, &switches.fill);
    } else {
	PaintEllipse(picture, x, y, a, b, switches.lineWidth, &switches.fill, 1);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * LineOp --
 *
 * Results:
 *	Returns a standard TCL return value.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static int
LineOp(
    Blt_Picture picture,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument objects. */
{
    LineSwitches switches;
    size_t nPoints;
    Point2f *points;
    
    memset(&switches, 0, sizeof(switches));
    switches.bg.u32 = 0xFFFFFFFF;
    switches.alpha = -1;
    if (Blt_ParseSwitches(interp, lineSwitches, objc - 3, objv + 3, 
		&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if (switches.alpha != -1) {
	switches.bg.Alpha = switches.alpha;
    }
    if (switches.x.nValues != switches.y.nValues) {
	Tcl_AppendResult(interp, "-x and -y coordinate lists must have the ",
		" same number of coordinates.",(char *)NULL);
	return TCL_ERROR;
    }
    points = NULL;
    if (switches.x.nValues > 0) {
	size_t i;
	float *x, *y;

	nPoints = switches.x.nValues;
	points = Blt_Malloc(sizeof(Point2f) * nPoints);
	if (points == NULL) {
	    Tcl_AppendResult(interp, "can't allocate memory for ", 
		Blt_Itoa(nPoints + 1), " points", (char *)NULL);
	    return TCL_ERROR;
	}
	x = (float *)switches.x.values;
	y = (float *)switches.y.values;
	for (i = 0; i < nPoints; i++) {
	    points[i].x = x[i];
	    points[i].y = y[i];
	}
	Blt_Free(switches.x.values);
	Blt_Free(switches.y.values);
	switches.x.values = switches.y.values = NULL;
    } else if (switches.coords.nValues > 0) {
	size_t i, j;
	float *coords;

	if (switches.coords.nValues & 0x1) {
	    Tcl_AppendResult(interp, "bad -coords list: ",
		"must have an even number of values", (char *)NULL);
	    return TCL_ERROR;
	}
	nPoints = (switches.coords.nValues / 2);
	points = Blt_Malloc(sizeof(Point2f)* nPoints);
	if (points == NULL) {
	    Tcl_AppendResult(interp, "can't allocate memory for ", 
		Blt_Itoa(nPoints + 1), " points", (char *)NULL);
	    return TCL_ERROR;
	}
	coords = (float *)switches.coords.values;
	for (i = 0, j = 0; i < switches.coords.nValues; i += 2, j++) {
	    points[j].x = coords[i];
	    points[j].y = coords[i+1];
	}
	Blt_Free(switches.coords.values);
	switches.coords.values = NULL;
    }
    if (points != NULL) {
	PaintPolyline(picture, nPoints, points, switches.lineWidth, 
		   &switches.bg);
	Blt_Free(points);
    }
    Blt_FreeSwitches(lineSwitches, (char *)&switches, 0);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * PolygonOp --
 *
 * Results:
 *	Returns a standard TCL return value.
 *
 * Side effects:
 *	None.
 *
 *	$pict draw polygon -coords $coords -color $color 
 *---------------------------------------------------------------------------
 */
static int
PolygonOp(
    Pict *destPtr,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument objects. */
{
    PolygonSwitches switches;
    size_t nVertices;
    Point2f *vertices;
    Region2f r;

    memset(&switches, 0, sizeof(switches));
    switches.bg.u32 = 0xFFFFFFFF;
    switches.alpha = -1;
    if (Blt_ParseSwitches(interp, polygonSwitches, objc - 3, objv + 3, 
		&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if (switches.alpha != -1) {
	switches.bg.Alpha = switches.alpha;
    }
    if (switches.x.nValues != switches.y.nValues) {
	Tcl_AppendResult(interp, "-x and -y coordinate lists must have the ",
		" same number of coordinates.",(char *)NULL);
	return TCL_ERROR;
    }
    vertices = NULL;
    r.top = r.left = FLT_MAX, r.bottom = r.right = -FLT_MAX;
    if (switches.x.nValues > 0) {
	size_t i;
	float *x, *y;

	nVertices = switches.x.nValues;
	vertices = Blt_Malloc(sizeof(Point2f) * (switches.x.nValues + 1));
	if (vertices == NULL) {
	    Tcl_AppendResult(interp, "can't allocate memory for ", 
		Blt_Itoa(nVertices + 1), " vertices", (char *)NULL);
	    return TCL_ERROR;
	}
	x = (float *)switches.x.values;
	y = (float *)switches.y.values;
	for (i = 0; i < switches.x.nValues; i++) {
	    vertices[i].x = x[i];
	    vertices[i].y = y[i];
	    if (r.left > x[i]) {
		r.left = x[i];
	    } else if (r.right < x[i]) {
		r.right = x[i];
	    }
	    if (r.top > y[i]) {
		r.top = y[i];
	    } else if (r.bottom < y[i]) {
		r.bottom = y[i];
	    }
	}
	if ((x[0] != x[i-1]) || (y[0] != y[i-1])) {
	    vertices[i].x = x[0];
	    vertices[i].y = y[0];
	    nVertices++;
	}
	Blt_Free(switches.x.values);
	Blt_Free(switches.y.values);
	switches.x.values = switches.y.values = NULL;
    } else if (switches.coords.nValues > 0) {
	size_t i, j;
	float *coords;

	if (switches.coords.nValues & 0x1) {
	    Tcl_AppendResult(interp, "bad -coords list: ",
		"must have an even number of values", (char *)NULL);
	    return TCL_ERROR;
	}
	nVertices = (switches.coords.nValues / 2);
	vertices = Blt_Malloc(sizeof(Point2f)* (nVertices + 1));
	if (vertices == NULL) {
	    Tcl_AppendResult(interp, "can't allocate memory for ", 
		Blt_Itoa(nVertices + 1), " vertices", (char *)NULL);
	    return TCL_ERROR;
	}
	coords = (float *)switches.coords.values;
	for (i = 0, j = 0; i < switches.coords.nValues; i += 2, j++) {
	    vertices[j].x = coords[i];
	    vertices[j].y = coords[i+1];
	    if (r.left > coords[i]) {
		r.left = coords[i];
	    } else if (r.right < coords[i]) {
		r.right = coords[i];
	    }
	    if (r.top > coords[i+1]) {
		r.top = coords[i+1];
	    } else if (r.bottom < coords[i+1]) {
		r.bottom = coords[i+1];
	    }
	}
	if ((coords[0] != coords[i-2]) || (coords[1] != coords[i-1])) {
	    vertices[j].x = coords[0];
	    vertices[j].y = coords[1];
	    nVertices++;
	}
	Blt_Free(switches.coords.values);
	switches.coords.values = NULL;
    }
    if (vertices != NULL) {
	if ((r.left < destPtr->width) && (r.right >= 0) &&
	    (r.top < destPtr->height) && (r.bottom >= 0)) {
	    if (switches.antialiased) {
		PaintPolygonAA2(destPtr, nVertices, vertices, &r, 
			&switches.bg, &switches.shadow);
	    } else {
		if (switches.shadow.offset > 0) {
		    PaintPolygonShadow(destPtr, nVertices, vertices, &r, 
				       &switches.shadow);
		}
		Blt_PaintPolygon(destPtr, nVertices, vertices, &switches.bg);
	    }
	}
	Blt_Free(vertices);
    }
    Blt_FreeSwitches(polygonSwitches, (char *)&switches, 0);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RectangleOp --
 *
 * Results:
 *	Returns a standard TCL return value.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static int
RectangleOp(
    Blt_Picture picture,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument objects. */
{
    RectangleSwitches switches;
    PictRegion r;
    
    if (Blt_GetBBoxFromObjv(interp, 4, objv + 3, &r) != TCL_OK) {
	return TCL_ERROR;
    }
    memset(&switches, 0, sizeof(switches));
    /* Process switches  */
    switches.lineWidth = 0;
    switches.bg.u32 = 0xFFFFFFFF;
    switches.alpha = -1;
    if (Blt_ParseSwitches(interp, rectangleSwitches, objc - 7, objv + 7, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if (switches.alpha != -1) {
	switches.bg.Alpha = switches.alpha;
    }
    if (switches.shadow.offset > 0) {
	PaintRectangleShadow(picture, r.x, r.y, r.w, r.h, switches.radius, 
		switches.lineWidth, &switches.shadow);
    }
    if ((switches.antialiased) && (switches.radius > 0)) {
	PaintRectangleAA(picture, r.x, r.y, r.w, r.h, switches.radius, 
		switches.lineWidth, &switches.bg);
    } else {
	PaintRectangle(picture, r.x, r.y, r.w, r.h, switches.radius, 
		switches.lineWidth, &switches.bg);
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * TextOp --
 *
 * Results:
 *	Returns a standard TCL return value.
 *
 * Side effects:
 *	None.
 *
 *	image draw text string x y switches 
 *---------------------------------------------------------------------------
 */
static int
TextOp(
    Pict *destPtr,		/* Picture. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument objects. */
{ 
#ifdef HAVE_FT2BUILD_H
    FtFont *fontPtr;
    TextSwitches switches;
    const char *string;
    const char *fontName;
    int length;
    int result;
    int x, y;

    string = Tcl_GetStringFromObj(objv[3], &length);
    if ((Tcl_GetIntFromObj(interp, objv[4], &x) != TCL_OK) ||
	(Tcl_GetIntFromObj(interp, objv[5], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    /* Process switches  */
    switches.alpha = -1;
    switches.anchor = TK_ANCHOR_NW;
    switches.angle = 0.0;
    switches.shadow.offset = 0;
    switches.color.u32 = 0xFF000000;  /* black. */
    switches.shadow.alpha = 0xA0; /* black. */
    switches.fontObjPtr = Tcl_NewStringObj("Arial 12", -1);
    switches.fontSize = 0;
    switches.kerning = FALSE;
    switches.justify = TK_JUSTIFY_LEFT;
    if (Blt_ParseSwitches(interp, textSwitches, objc - 6, objv + 6, &switches, 
	BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if (switches.alpha != -1) {
	switches.color.Alpha = switches.alpha;
    }
    fontName = Tcl_GetString(switches.fontObjPtr);
    fontPtr = OpenFont(interp, fontName, switches.fontSize);
    if (fontPtr == NULL) {
	fprintf(stderr, "can't open font %s\n", fontName);
	return TCL_ERROR;
    }
    if (destPtr->flags & BLT_PIC_ASSOCIATED_COLORS) {
	Blt_UnassociateColors(destPtr);
    }
    if (switches.angle != 0.0) {
	TextStyle ts;
	TextLayout *layoutPtr;

	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetJustify(ts, switches.justify);
	layoutPtr = CreateSimpleTextLayout(fontPtr, string, length, &ts);
	if (fontPtr->face->face_flags & FT_FACE_FLAG_SCALABLE) {
	    TextFragment *fp, *fend;
	    double rw, rh;

	    Blt_RotateStartingTextPositions(layoutPtr, switches.angle);
	    Blt_GetBoundingBox(layoutPtr->width, layoutPtr->height, 
		switches.angle, &rw, &rh, (Point2d *)NULL);
	    Blt_TranslateAnchor(x, y, (int)(rw), (int)(rh), switches.anchor, 
		&x, &y);
	    RotateFont(fontPtr, switches.angle);
	    for (fp = layoutPtr->fragments, fend = fp + layoutPtr->nFrags; 
		fp < fend; fp++) {
		PaintText(destPtr, fontPtr, fp->text, fp->count, x + fp->sx, 
			y + fp->sy, switches.kerning, &switches.color);
	    }
	} else {
	    Blt_Picture tmp;
	    Blt_Pixel color;
	    Pict *rotPtr;
	    TextFragment *fp, *fend;
	    
	    tmp = Blt_CreatePicture(layoutPtr->width, layoutPtr->height);
	    color.u32 = 0x00FF0000;
	    Blt_BlankPicture(tmp, &color);
	    color.Alpha = 0xFF;
	    for (fp = layoutPtr->fragments, fend = fp + layoutPtr->nFrags; 
		fp < fend; fp++) {
		PaintText(tmp, fontPtr, fp->text, fp->count, fp->sx, 
			fp->sy, switches.kerning, &switches.color);
	    }
	    rotPtr = Blt_RotatePicture(tmp, switches.angle);
	    Blt_FreePicture(tmp);
	    Blt_TranslateAnchor(x, y, rotPtr->width, rotPtr->height, 
				switches.anchor, &x, &y);
	    Blt_BlendPictures(destPtr, rotPtr, 0, 0, 
		rotPtr->width, rotPtr->height, x, y);
	    Blt_FreePicture(rotPtr);
	}
	Blt_Free(layoutPtr);
    } else {
	size_t w, h;

	MeasureText(fontPtr, string, length, &w, &h);
	Blt_TranslateAnchor(x, y, w, h, switches.anchor, &x, &y);
	fprintf(stderr, "string=%s w=%d h=%d\n", string, w, h);
	if (switches.shadow.offset > 0) {
	    Blt_Pixel color;
	    Pict *blurPtr, *tmpPtr;
	    int offset = switches.shadow.offset;

	    MeasureText(fontPtr, string, length, &w, &h);
	    tmpPtr = Blt_CreatePicture(w, h);
	    color.u32 = 0x00000000;
	    Blt_BlankPicture(tmpPtr, &color);
	    color.Alpha = switches.shadow.alpha;
	    PaintText(tmpPtr, fontPtr, string, length, 0, fontPtr->ascent, 
		      switches.kerning, &color);
	    blurPtr = Blt_CreatePicture(w+offset*4, h+offset*4);
	    color.Alpha = 0;
	    Blt_BlankPicture(blurPtr, &color);
	    Blt_CopyPictureBits(blurPtr, tmpPtr, 0, 0, w, h, 
				offset, offset); 
	    Blt_FreePicture(tmpPtr);
	    Blt_BlurPicture(blurPtr, blurPtr, offset);
	    Blt_BlendPictures(destPtr, blurPtr, 0, 0, blurPtr->width, 
			      blurPtr->height, x, y);
	    Blt_FreePicture(blurPtr);
	}
	y += fontPtr->ascent;
	result = PaintText(destPtr, fontPtr, string, length, x, y, 
			   switches.kerning, &switches.color);
    }
    Blt_FreeSwitches(textSwitches, (char *)&switches, 0);
    CloseFont(fontPtr);
#endif
    return TCL_OK;
}

Blt_OpSpec bltPictDrawOps[] =
{
    {"circle",    1, CircleOp,    4, 0, "x y r ?switches?",},
    {"ellipse",   1, EllipseOp,   5, 0, "x y a b ?switches?",},
    {"line",      1, LineOp,      3, 0, "?switches?",},
    {"polygon",   1, PolygonOp,   3, 0, "?switches?",},
    {"rectangle", 1, RectangleOp, 7, 0, "x1 y1 x2 y2 ?switches?",},
    {"text",      1, TextOp,      6, 0, "string x y ?switches?",},
};

int bltPictDrawNOps = sizeof(bltPictDrawOps) / sizeof(Blt_OpSpec);

static void 
Polyline2(Pict *destPtr, int x1, int y1, int x2, int y2, Blt_Pixel *colorPtr)
{
    Blt_Pixel *dp;
    int dx, dy, xDir;
    unsigned long error;

    if (y1 > y2) {
	int tmp;

	tmp = y1, y1 = y2, y2 = tmp;
	tmp = x1, x1 = x2, x2 = tmp;
    }

    /* First and last Pixels always get Set: */
    dp = Blt_PicturePixel(destPtr, x1, y1);
    dp->u32 = colorPtr->u32;
    dp = Blt_PicturePixel(destPtr, x2, y2);
    dp->u32 = colorPtr->u32;

    dx = x2 - x1;
    dy = y2 - y1;

    if (dx >= 0) {
	xDir = 1;
    } else {
	xDir = -1;
	dx = -dx;
    }
    if (dx == 0) {		/*  Vertical line */
	Blt_Pixel *dp;

	dp = Blt_PicturePixel(destPtr, x1, y1);
	while (dy-- > 0) {
	    dp += destPtr->pixelsPerRow;
	    dp->u32 = colorPtr->u32;
	}
	return;
    }
    if (dy == 0) {		/* Horizontal line */
	Blt_Pixel *dp;

	dp = Blt_PicturePixel(destPtr, x1, y1);
	while(dx-- > 0) {
	    dp += xDir;
	    dp->u32 = colorPtr->u32;
	}
	return;
    }
    if (dx == dy) {		/* Diagonal line. */
	Blt_Pixel *dp;

	dp = Blt_PicturePixel(destPtr, x1, y1);
	while(dy-- > 0) {
	    dp += destPtr->pixelsPerRow + xDir;
	    dp->u32 = colorPtr->u32;
	}
	return;
    }

    /* use Wu Antialiasing: */

    error = 0;
    if (dy > dx) {		/* y-major line */
	unsigned long adjust;

	/* x1 -= lineWidth / 2; */
	adjust = (dx << 16) / dy;
	while(--dy) {
	    Blt_Pixel *dp;
	    int x;
	    unsigned char weight;
	    
	    error += adjust;
	    ++y1;
	    if (error & ~0xFFFF) {
		x1 += xDir;
		error &= 0xFFFF;
	    }
	    dp = Blt_PicturePixel(destPtr, x1, y1);
	    weight = (unsigned char)(error >> 8);
	    x = x1;
	    if (x >= 0) {
		dp->u32 = colorPtr->u32;
		dp->Alpha = ~weight;
	    }
	    x += xDir;
	    dp += xDir;
	    if (x >= 0) {
		dp->u32 = colorPtr->u32;
		dp->Alpha = weight;
	    }
	}
    } else {			/* x-major line */
	unsigned long adjust;

	/* y1 -= lineWidth / 2; */
	adjust = (dy << 16) / dx;
	while (--dx) {
	    Blt_Pixel *dp;
	    int y;
	    unsigned char weight;

	    error += adjust;
	    x1 += xDir;
	    if (error & ~0xFFFF) {
		y1++;
		error &= 0xFFFF;
	    }
	    dp = Blt_PicturePixel(destPtr, x1, y1);
	    weight = (unsigned char)(error >> 8);
	    y = y1;
	    if (y >= 0) {
		dp->u32 = colorPtr->u32;
		dp->Alpha = ~weight;
	    }
	    dp += destPtr->pixelsPerRow;
	    y++;
	    if (y >= 0) {
		dp->u32 = colorPtr->u32;
		dp->Alpha = weight;
	    } 
	}
    }
}


Blt_Picture
Blt_PaintCheckbox(int w, int h, unsigned int fill, unsigned int outline, 
		  unsigned int check, int on)
{
    Shadow shadow;
    Blt_Pixel color;
    int x, y;
    Pict *destPtr;

    destPtr = Blt_CreatePicture(w, h);
    w -= 5, h -= 5;
    color.u32 = 0x00;
    Blt_BlankPicture(destPtr, &color);

    shadow.offset = 1;
    shadow.alpha = 0xA0;
    x = y = 2;
#ifdef nodef
    PaintRectangleShadow(destPtr, x+1, y+1, w-2, h-2, 0, 0, &shadow);
    color.u32 = fill;
    PaintRectangle(destPtr, x+1, y+1, w-2, h-2, 0, 0, &color);
    color.u32 = outline;
    PaintRectangle(destPtr, x, y, w, h, 0, 1, &color);
#endif
    x += 2, y += 2;
    w -= 5, h -= 5;
    if (on) {
	Point2f points[7];
	Region2f r;

	points[0].x = points[1].x = points[6].x = x;
	points[0].y = points[6].y = y + (0.4 * h);
	points[1].y = y + (0.6 * h);
	points[2].x = points[5].x = x + (0.4 * w);
	points[2].y = y + h;
	points[3].x = points[4].x = x + w;
	points[3].y = y + (0.2 * h);
	points[4].y = y;
	points[5].y = y + (0.7 * h);
	points[6].x = points[0].x;
	points[6].y = points[0].y;
	shadow.offset = 2;
	r.left = x, r.right = x + w;
	r.top = y, r.bottom = y + h;
	color.u32 = check;
	PaintPolygonAA2(destPtr, 7, points, &r, &color, &shadow);
    }
    return destPtr;
}

Blt_Picture
Blt_PaintRadioButton(int w, int h, unsigned int fill, unsigned int outline, 
		     unsigned int indicator, int on)
{
    Blt_Picture picture;
    CircleSwitches switches;
    int x, y, r;
    Blt_Pixel color;

    /* Process switches  */
    switches.lineWidth = 1;
    switches.fill.u32    = fill;
    switches.outline.u32 = outline;
    switches.alpha = -1;
    switches.antialiased = 0;
    switches.shadow.offset = 1;
    switches.shadow.alpha = 0xa0;
    switches.antialiased = 1;

    picture = Blt_CreatePicture(w, h);
    color.u32 = 0x00;
    Blt_BlankPicture(picture, &color);
    w -= 6, h -= 6;
    x = w / 2 + 2;
    y = h / 2 + 2;
    r = (w+1) / 2;
    switches.fill.u32 = fill;
    switches.outline.u32 = outline;
    DrawCircle(picture, x, y, r, &switches);
    if (on) {
	r -= 6;
	if (r < 1) {
	    r = 2;
	}
	switches.fill.u32 = indicator;
	switches.outline.u32 = indicator;
	switches.lineWidth = 0;
	switches.shadow.offset = 0;
	DrawCircle(picture, x, y, r, &switches);
    }
    return picture;
}
