
/*
 * bltBgPattern.c --
 *
 * This module creates background patterns for the BLT toolkit.
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

#include "bltInt.h"
#include "bltOp.h"
#include "bltChain.h"
#include "bltHash.h"
#include "bltImage.h"
#include "bltPicture.h"
#include "bltPainter.h"
#include <X11/Xutil.h>
#include "bltBgStyle.h"

#define BG_PATTERN_THREAD_KEY	"BLT Background Pattern Data"

/* 
   bgpattern create pattern 
	-image $image
	-color $color
	-darkcolor $color
	-lightcolor $color
	-resamplefilter $filter
	-opacity $alpha
	-xorigin $x
	-yorigin $y
	-tile yes
	-center yes
	-scale no
	-relativeto self|toplevel|window
	-mask image|bitmap

  bgpattern create tile 
	-relativeto self|toplevel|window
	-image $image
	-bg $color
  bgpattern create picture 
        -image $image
	-filter $filterName
	-bg $color
  bgpattern create gradient 
	-type radial|xlinear|ylinear|diagonal
	-low $color
	-high $color
	-bg $color
  bgpattern create border 
	-bg $color
	-alpha $color 

  bgpattern create texture -type metal|wind|??? 
	-bg $color

  bgpattern names
  bgpattern configure $tile
  bgpattern delete $tile
*/


enum PatternTypes {
    PATTERN_GRADIENT,		/* Color gradient. */
    PATTERN_TILE,		/* Tiled or resizable color picture. */
    PATTERN_SOLID,		/* General pattern. */
    PATTERN_TEXTURE,		/* Procedural texture. */
};


static const char *patternTypes[] = {
    "gradient",
    "tile",
    "solid",
    "texture"
};

enum ReferenceTypes {
    REFERENCE_SELF,		/* Current window. */
    REFERENCE_TOPLEVEL,		/* Toplevel of current window. */
    REFERENCE_WINDOW,		/* Specifically named window. */
    REFERENCE_NONE,		/* Don't use reference window. Background
				 * region will be defined by user. */
};

typedef struct {
    int x, y;
    unsigned int w, h;
    Tk_Window tkwin;
} Reference;

typedef struct {
    int x, y, width, height;
} BgRegion;

typedef struct {
    Blt_HashTable patternTable;	/* Hash table of pattern structures keyed by
				 * the name of the image. */

    Tcl_Interp *interp;		/* Interpreter associated with this set of
				 * background patterns. */

    int nextId;			/* Serial number of the identifier to be used
				 * for next background pattern created.  */
} BackgroundInterpData;

typedef struct _Pattern Pattern;

typedef void (DestroyPatternProc)(Pattern *patternPtr);
typedef int (ConfigurePatternProc)(Tcl_Interp *interp, Pattern *patternPtr,
	int objc, Tcl_Obj *const *objv, unsigned int flags);
typedef void (DrawRectangleProc)(Tk_Window tkwin, Drawable drawable, 
	Pattern *patternPtr, int x, int y, int w, int h);
typedef void (DrawPolygonProc)(Tk_Window tkwin, Drawable drawable, 
	Pattern *patternPtr, int nPoints, XPoint *points);

typedef struct {
    enum PatternTypes type;	/* Type of pattern style: solid, tile,
				 * texture, or gradient. */

    Blt_ConfigSpec *configSpecs;
    DestroyPatternProc *destroyProc;
    ConfigurePatternProc *configProc;
    DrawRectangleProc *drawRectangleProc;
    DrawPolygonProc *drawPolygonProc;
} PatternClass;


struct _Pattern {
    const char *name;		/* Generated name of background pattern. */
    PatternClass *classPtr;
    BackgroundInterpData *dataPtr;
    Tk_Window tkwin;		/* Main window. Used to query background pattern
				 * options. */
    Display *display;		/* Display of this background pattern. */
    unsigned int flags;		/* See definitions below. */
    Blt_HashEntry *hashPtr;	/* Hash entry in pattern table. */
    Blt_Chain chain;		/* List of pattern tokens.  Used to register
				 * callbacks for each client of the background
				 * pattern. */
    Blt_ChainLink link;		/* Background token that is associated with
				 * the pattern creation "bgpattern create...".
				 */
    Tk_3DBorder border;		/* 3D Border.  May be used for all background
				 * types. */
    Tk_Window refWindow;	/* Refer to coordinates in this window when
				 * determining the tile/gradient origin. */
    BgRegion refRegion;
    Blt_HashTable pictTable;	/* Table of pictures cached for each
				 * pattern reference. */
    int reference;		/* "self", "toplevel", or "window". */
    int xOrigin, yOrigin;
};

typedef struct {
    const char *name;		/* Generated name of background pattern. */

    PatternClass *classPtr;

    BackgroundInterpData *dataPtr;

    Tk_Window tkwin;		/* Main window. Used to query background pattern
				 * options. */

    Display *display;		/* Display of this background pattern. */

    unsigned int flags;		/* See definitions below. */

    Blt_HashEntry *hashPtr;	/* Link to original client. */

    Blt_Chain chain;		/* List of pattern tokens.  Used to register
				 * callbacks for each client of the background
				 * pattern. */

    Blt_ChainLink link;		/* Background token that is associated with
				 * the pattern creation "bgpattern create...".
				 */
    Tk_3DBorder border;		/* 3D Border.  May be used for all pattern
				 * types. */
    
    Tk_Window refWindow;	/* Refer to coordinates in this window when
				 * determining the tile/gradient origin. */

    BgRegion refRegion;

    Blt_HashTable pictTable;	/* Table of pictures cached for each pattern
				 * reference. */

    int reference;		/* "self", "toplevel", or "window". */

    int xOrigin, yOrigin;

    /* Solid pattern specific fields. */
    int alpha;			/* Transparency value. */

} SolidPattern;

typedef struct {
    const char *name;		/* Generated name of background pattern. */

    PatternClass *classPtr;

    BackgroundInterpData *dataPtr;

    Tk_Window tkwin;		/* Main window. Used to query background
				 * pattern options. */

    Display *display;		/* Display of this background pattern. */

    unsigned int flags;		/* See definitions below. */

    Blt_HashEntry *hashPtr;	/* Link to original client. */

    Blt_Chain chain;		/* List of pattern tokens.  Used to register
				 * callbacks for each client of the background
				 * pattern. */

    Blt_ChainLink link;		/* Background token that is associated with
				 * the pattern creation "bgpattern create...".
				 */
    Tk_3DBorder border;		/* 3D Border.  May be used for all pattern
				 * types. */
    
    Tk_Window refWindow;	/* Refer to coordinates in this window when
				 * determining the tile/gradient origin. */

    BgRegion refRegion;

    Blt_HashTable pictTable;	/* Table of pictures cached for each pattern
				 * reference. */

    int reference;		/* "self", "toplevel", or "window". */

    int xOrigin, yOrigin;

    /* Image pattern specific fields. */

    Tk_Image tkImage;		/* Original image (before resampling). */

    Blt_ResampleFilter filter;	/* 1-D image filter to use to when resizing
				 * the original picture. */

} TilePattern;

typedef struct {
    const char *name;		/* Generated name of background pattern. */

    PatternClass *classPtr;

    BackgroundInterpData *dataPtr;

    Tk_Window tkwin;		/* Main window. Used to query background
				 * pattern options. */

    Display *display;		/* Display of this background pattern. */

    unsigned int flags;		/* See definitions below. */

    Blt_HashEntry *hashPtr;	/* Link to original client. */

    Blt_Chain chain;		/* List of pattern tokens.  Used to register
				 * callbacks for each client of the background
				 * pattern. */

    Blt_ChainLink link;		/* Background token that is associated with
				 * the pattern creation "bgpattern create...".
				 */
    Tk_3DBorder border;		/* 3D Border.  May be used for all pattern
				 * types. */
    
    Tk_Window refWindow;	/* Refer to coordinates in this window when
				 * determining the tile/gradient origin. */

    BgRegion refRegion;

    Blt_HashTable pictTable;	/* Table of pictures cached for each
				 * pattern reference. */

    int reference;		/* "self", "toplevel", or "window". */

    int xOrigin, yOrigin;

    /* Gradient pattern specific fields. */

    Blt_Gradient gradient;

    Blt_Pixel low, high;	/* Texture or gradient colors. */

    int alpha;			/* Transparency value. */

} GradientPattern;

typedef struct {
    const char *name;		/* Generated name of background pattern. */

    PatternClass *classPtr;

    BackgroundInterpData *dataPtr;

    Tk_Window tkwin;		/* Main window. Used to query background
				 * pattern options. */

    Display *display;		/* Display of this background pattern. */

    unsigned int flags;		/* See definitions below. */

    Blt_HashEntry *hashPtr;	/* Link to original client. */

    Blt_Chain chain;		/* List of pattern tokens.  Used to register
				 * callbacks for each client of the background
				 * pattern. */

    Blt_ChainLink link;		/* Background token that is associated with
				 * the pattern creation "bgpattern create...".
				 */
    Tk_3DBorder border;		/* 3D Border.  May be used for all pattern
				 * types. */
    
    Tk_Window refWindow;	/* Refer to coordinates in this window when
				 * determining the tile/gradient origin. */

    BgRegion refRegion;

    Blt_HashTable pictTable;	/* Table of pictures cached for each
				 * pattern reference. */

    int reference;		/* "self", "toplevel", or "window". */

    int xOrigin, yOrigin;

    /* Texture pattern specific fields. */

    Blt_Pixel low, high;	/* Texture colors. */

    int alpha;			/* Transparency value. */

} TexturePattern;

struct _Blt_Background {
    Pattern *corePtr;		/* Pointer to master background pattern
				 * object. */

    Blt_BackgroundChangedProc *notifyProc;

    ClientData clientData;	/* Data to be passed on notifier
				 * callbacks.  */

    Blt_ChainLink link;		/* Link of this entry in notifier list. */

};

#define DELETE_PENDING		(1<<0)
#define BG_CENTER		(1<<2)
#define BG_SCALE		(1<<3)

typedef struct _Blt_Background Background;

#define DEF_OPACITY		"100.0"
#define DEF_ORIGIN_X		"0"
#define DEF_ORIGIN_Y		"0"
#define DEF_BORDER		STD_NORMAL_BACKGROUND
#define DEF_GRADIENT_PATH	"y"
#define DEF_GRADIENT_HIGH	"grey90"
#define DEF_GRADIENT_JITTER	"no"
#define DEF_GRADIENT_LOGSCALE	"yes"
#define DEF_GRADIENT_LOW	"grey50"
#define DEF_GRADIENT_MODE	"xlinear"
#define DEF_GRADIENT_SHAPE	"linear"
#define DEF_REFERENCE		"toplevel"
#define DEF_RESAMPLE_FILTER	"box"
#define DEF_SCALE		"no"
#define DEF_CENTER		"no"
#define DEF_TILE		"no"

static Blt_OptionParseProc ObjToImageProc;
static Blt_OptionPrintProc ImageToObjProc;
static Blt_OptionFreeProc FreeImageProc;
static Blt_CustomOption imageOption =
{
    ObjToImageProc, ImageToObjProc, FreeImageProc, (ClientData)0
};

extern Blt_CustomOption bltFilterOption;

static Blt_OptionParseProc ObjToReferenceProc;
static Blt_OptionPrintProc ReferenceToObjProc;
static Blt_CustomOption referenceToOption =
{
    ObjToReferenceProc, ReferenceToObjProc, NULL, (ClientData)0
};

static Blt_OptionParseProc ObjToShapeProc;
static Blt_OptionPrintProc ShapeToObjProc;
static Blt_CustomOption shapeOption =
{
    ObjToShapeProc, ShapeToObjProc, NULL, (ClientData)0
};

static Blt_OptionParseProc ObjToPathProc;
static Blt_OptionPrintProc PathToObjProc;
static Blt_CustomOption pathOption =
{
    ObjToPathProc, PathToObjProc, NULL, (ClientData)0
};

static Blt_OptionParseProc ObjToOpacityProc;
static Blt_OptionPrintProc OpacityToObjProc;
static Blt_CustomOption opacityOption =
{
    ObjToOpacityProc, OpacityToObjProc, NULL, (ClientData)0
};

static Blt_ConfigSpec solidConfigSpecs[] =
{
    {BLT_CONFIG_SYNONYM, "-background", "color", (char *)NULL, (char *)NULL, 
        0, 0},
    {BLT_CONFIG_SYNONYM, "-bg", "color", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_BORDER, "-color", "color", "Color", DEF_BORDER, 
	Blt_Offset(SolidPattern, border), 0},
    {BLT_CONFIG_CUSTOM, "-opacity", "opacity", "Opacity", "100.0", 
	Blt_Offset(SolidPattern, alpha), BLT_CONFIG_DONT_SET_DEFAULT, 
	&opacityOption},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static Blt_ConfigSpec tileConfigSpecs[] =
{
    {BLT_CONFIG_BITMASK, "-center", "center", "Center", DEF_CENTER,
        Blt_Offset(TilePattern, flags), BLT_CONFIG_DONT_SET_DEFAULT, 
	(Blt_CustomOption *)BG_CENTER},
    {BLT_CONFIG_BORDER, "-color", "color", "Color", DEF_BORDER, 
	Blt_Offset(TilePattern, border), 0},
    {BLT_CONFIG_BORDER, "-darkcolor", "darkColor", "DarkColor", DEF_BORDER, 
	Blt_Offset(TilePattern, border), 0},
    {BLT_CONFIG_CUSTOM, "-filter", "filter", "Filter", DEF_RESAMPLE_FILTER, 
	Blt_Offset(TilePattern, filter), 0, &bltFilterOption},
    {BLT_CONFIG_CUSTOM, "-image", "image", "Image", (char *)NULL,
        Blt_Offset(TilePattern, tkImage), BLT_CONFIG_DONT_SET_DEFAULT, 
	&imageOption},
    {BLT_CONFIG_BORDER, "-lightcolor", "lightColor", "LightColor", DEF_BORDER, 
	Blt_Offset(TilePattern, border), 0},
    {BLT_CONFIG_CUSTOM, "-relativeto", "relativeTo", "RelativeTo", 
	DEF_REFERENCE, Blt_Offset(TilePattern, reference), 
	BLT_CONFIG_DONT_SET_DEFAULT, &referenceToOption},
    {BLT_CONFIG_BITMASK, "-scale", "scale", "scale", DEF_SCALE,
        Blt_Offset(TilePattern, flags), BLT_CONFIG_DONT_SET_DEFAULT, 
	(Blt_CustomOption *)BG_SCALE},
    {BLT_CONFIG_PIXELS, "-xorigin", "xOrigin", "XOrigin", DEF_ORIGIN_X,
        Blt_Offset(TilePattern, xOrigin), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS, "-yorigin", "yOrigin", "YOrigin", DEF_ORIGIN_Y,
        Blt_Offset(TilePattern, yOrigin), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static Blt_ConfigSpec gradientConfigSpecs[] =
{
    {BLT_CONFIG_BORDER, "-background", "background", "Background", DEF_BORDER,
	Blt_Offset(GradientPattern, border), 0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_CUSTOM, "-direction", "direction", "Direction", 
	DEF_GRADIENT_PATH, Blt_Offset(GradientPattern, gradient.path), 
	BLT_CONFIG_DONT_SET_DEFAULT, &pathOption},
    {BLT_CONFIG_PIX32, "-high", "high", "High", DEF_GRADIENT_HIGH,
        Blt_Offset(GradientPattern, high), 0},
    {BLT_CONFIG_BOOLEAN, "-jitter", "jitter", "Jitter", 
	DEF_GRADIENT_JITTER, Blt_Offset(GradientPattern, gradient.jitter), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BOOLEAN, "-logscale", "logscale", "Logscale", 
	DEF_GRADIENT_LOGSCALE, Blt_Offset(GradientPattern,gradient.logScale),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIX32, "-low", "low", "Low", DEF_GRADIENT_LOW,
        Blt_Offset(GradientPattern, low), 0},
    {BLT_CONFIG_CUSTOM, "-opacity", "opacity", "Opacity", "100.0", 
	Blt_Offset(GradientPattern, alpha), BLT_CONFIG_DONT_SET_DEFAULT, 
	&opacityOption},
    {BLT_CONFIG_CUSTOM, "-relativeto", "relativeTo", "RelativeTo", 
	DEF_REFERENCE, Blt_Offset(GradientPattern, reference), 
	BLT_CONFIG_DONT_SET_DEFAULT, &referenceToOption},
    {BLT_CONFIG_CUSTOM, "-shape", "shape", "Shape", DEF_GRADIENT_SHAPE, 
	Blt_Offset(GradientPattern, gradient.shape), 
	BLT_CONFIG_DONT_SET_DEFAULT, &shapeOption},
    {BLT_CONFIG_PIXELS, "-xorigin", "xOrigin", "XOrigin", DEF_ORIGIN_X,
        Blt_Offset(TilePattern, xOrigin), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS, "-yorigin", "yOrigin", "YOrigin", DEF_ORIGIN_Y,
        Blt_Offset(TilePattern, yOrigin), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static Blt_ConfigSpec textureConfigSpecs[] =
{
    {BLT_CONFIG_BORDER, "-background", "background", "Background", DEF_BORDER,
	Blt_Offset(TexturePattern, border), 0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_PIX32, "-high", "high", "High", DEF_GRADIENT_HIGH,
        Blt_Offset(TexturePattern, high), 0},
    {BLT_CONFIG_PIX32, "-low", "low", "Low", DEF_GRADIENT_LOW,
        Blt_Offset(TexturePattern, low), 0},
    {BLT_CONFIG_CUSTOM, "-relativeto", "relativeTo", "RelativeTo", 
	DEF_REFERENCE, Blt_Offset(TexturePattern, reference), 
	BLT_CONFIG_DONT_SET_DEFAULT, &referenceToOption},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static void NotifyClients(Pattern *corePtr);

static Blt_Picture
ImageToPicture(TilePattern *patternPtr, int *isFreePtr)
{
    Blt_Picture picture;

    picture = NULL;
    *isFreePtr = FALSE;
    if (Blt_IsPicture(patternPtr->tkImage)) {
	picture = Blt_GetPictureFromImage(patternPtr->tkImage);
    } else {
	Tk_PhotoHandle photo;

	photo = Tk_FindPhoto(patternPtr->dataPtr->interp, 
			     Blt_NameOfImage(patternPtr->tkImage));
	if (photo != NULL) {
	    picture = Blt_PhotoToPicture(photo);
	    *isFreePtr = TRUE;
	}
    }
    return picture;
}

/*
 *---------------------------------------------------------------------------
 *
 * ImageChangedProc
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
ImageChangedProc(
    ClientData clientData,
    int x, int y, int width, int height, /* Not used. */
    int imageWidth, int imageHeight)	 /* Not used. */
{
    Pattern *corePtr = clientData;

    /* Propagate the change in the image to all the clients. */
    NotifyClients(corePtr);
}

/*ARGSUSED*/
static void
FreeImageProc(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    TilePattern *patternPtr = (TilePattern *)(widgRec);

    if (patternPtr->tkImage != NULL) {
	Tk_FreeImage(patternPtr->tkImage);
	patternPtr->tkImage = NULL;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToImageProc --
 *
 *	Given an image name, get the Tk image associated with it.
 *
 * Results:
 *	The return value is a standard TCL result.  
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToImageProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representation of value. */
    char *widgRec,		/* Widget record. */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    TilePattern *patternPtr = (TilePattern *)(widgRec);
    Tk_Image tkImage;

    tkImage = Tk_GetImage(interp, patternPtr->tkwin, Tcl_GetString(objPtr), 
	ImageChangedProc, patternPtr);
    if (tkImage == NULL) {
	return TCL_ERROR;
    }
    patternPtr->tkImage = tkImage;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ImageToObjProc --
 *
 *	Convert the image name into a string Tcl_Obj.
 *
 * Results:
 *	The string representation of the image is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ImageToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    TilePattern *patternPtr = (TilePattern *)(widgRec);

    if (patternPtr->tkImage == NULL) {
	return Tcl_NewStringObj("", -1);
    }
    return Tcl_NewStringObj(Blt_NameOfImage(patternPtr->tkImage), -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToReference --
 *
 *	Given a string name, get the resample filter associated with it.
 *
 * Results:
 *	The return value is a standard TCL result.  
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToReferenceProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representation of value. */
    char *widgRec,		/* Widget record. */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Pattern *patternPtr = (Pattern *)(widgRec);
    int *referencePtr = (int *)(widgRec + offset);
    const char *string;
    char c;
    int refType;
    int length;

    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[0];
    if ((c == 's') && (strncmp(string, "self", length) == 0)) {
	refType = REFERENCE_SELF;
    } else if ((c == 't') && (strncmp(string, "toplevel", length) == 0)) {
	refType = REFERENCE_TOPLEVEL;
    } else if ((c == 'n') && (strncmp(string, "none", length) == 0)) {
	refType = REFERENCE_NONE;
    } else if (c == '.') {
	Tk_Window tkwin, tkMain;

	tkMain = Tk_MainWindow(interp);
	tkwin = Tk_NameToWindow(interp, string, tkMain);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	refType = REFERENCE_WINDOW;
	patternPtr->refWindow = tkwin;
    } else {
	Tcl_AppendResult(interp, "unknown reference type \"", string, "\"",
			 (char *)NULL);
	return TCL_ERROR;
    }
    *referencePtr = refType;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ReferenceToObj --
 *
 *	Convert the picture filter into a string Tcl_Obj.
 *
 * Results:
 *	The string representation of the filter is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ReferenceToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    int reference = *(int *)(widgRec + offset);
    const char *string;

    switch (reference) {
    case REFERENCE_SELF:
	string = "self";
	break;

    case REFERENCE_TOPLEVEL:
	string = "toplevel";
	break;

    case REFERENCE_NONE:
	string = "none";
	break;

    case REFERENCE_WINDOW:
	{
	    Pattern *patternPtr = (Pattern *)(widgRec);

	    string = Tk_PathName(patternPtr->refWindow);
	}
	break;

    default:
	string = "???";
	break;
    }
    return Tcl_NewStringObj(string, -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToShapeProc --
 *
 *	Translate the given string to the gradient shape is represents.  Value
 *	shapes are "linear", "bilinear", "radial", and "rectangular".
 *
 * Results:
 *	The return value is a standard TCL result.  
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToShapeProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representation of value. */
    char *widgRec,		/* Widget record. */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Blt_GradientShape *shapePtr = (Blt_GradientShape *)(widgRec + offset);
    char *string;

    string = Tcl_GetString(objPtr);
    if (strcmp(string, "linear") == 0) {
	*shapePtr = BLT_GRADIENT_SHAPE_LINEAR;
    } else if (strcmp(string, "bilinear") == 0) {
	*shapePtr = BLT_GRADIENT_SHAPE_BILINEAR;
    } else if (strcmp(string, "radial") == 0) {
	*shapePtr = BLT_GRADIENT_SHAPE_RADIAL;
    } else if (strcmp(string, "rectangular") == 0) {
	*shapePtr = BLT_GRADIENT_SHAPE_RECTANGULAR;
    } else {
	Tcl_AppendResult(interp, "unknown gradient type \"", string, "\"",
			 (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ShapeToObjProc --
 *
 *	Returns the string representing the current gradiant shape.
 *
 * Results:
 *	The string representation of the shape is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ShapeToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Blt_GradientShape shape = *(Blt_GradientShape *)(widgRec + offset);
    const char *string;
    
    switch (shape) {
    case BLT_GRADIENT_SHAPE_LINEAR:
	string = "linear";
	break;

    case BLT_GRADIENT_SHAPE_BILINEAR:
	string = "bilinear";
	break;

    case BLT_GRADIENT_SHAPE_RADIAL:
	string = "radial";
	break;

    case BLT_GRADIENT_SHAPE_RECTANGULAR:
	string = "rectangular";
	break;

    default:
	string = "???";
    }
    return Tcl_NewStringObj(string, -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToPathProc --
 *
 *	Translates the given string to the gradient path it represents.  Valid
 *	paths are "x", "y", "xy", and "yx".
 *
 * Results:
 *	The return value is a standard TCL result.  
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToPathProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representation of value. */
    char *widgRec,		/* Widget record. */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Blt_GradientPath *pathPtr = (Blt_GradientPath *)(widgRec + offset);
    char *string;

    string = Tcl_GetString(objPtr);
    if (strcmp(string, "x") == 0) {
	*pathPtr = BLT_GRADIENT_PATH_X;
    } else if (strcmp(string, "y") == 0) {
	*pathPtr = BLT_GRADIENT_PATH_Y;
    } else if (strcmp(string, "xy") == 0) {
	*pathPtr = BLT_GRADIENT_PATH_XY;
    } else if (strcmp(string, "yx") == 0) {
	*pathPtr = BLT_GRADIENT_PATH_YX;
    } else {
	Tcl_AppendResult(interp, "unknown gradient path \"", string, "\"",
			 (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * PathToObjProc --
 *
 *	Convert the picture filter into a string Tcl_Obj.
 *
 * Results:
 *	The string representation of the filter is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
PathToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Blt_GradientPath path = *(Blt_GradientPath *)(widgRec + offset);
    const char *string;

    switch (path) {
    case BLT_GRADIENT_PATH_X:
	string = "x";
	break;

    case BLT_GRADIENT_PATH_Y:
	string = "y";
	break;

    case BLT_GRADIENT_PATH_XY:
	string = "xy";
	break;

    case BLT_GRADIENT_PATH_YX:
	string = "yx";
	break;

    default:
	string = "?? unknown path ??";
	break;
    }
    return Tcl_NewStringObj(string, -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToOpacity --
 *
 *	Given a string name, get the resample filter associated with it.
 *
 * Results:
 *	The return value is a standard TCL result.  
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToOpacityProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representation of value. */
    char *widgRec,		/* Widget record. */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    int *alphaPtr = (int *)(widgRec + offset);
    double opacity;

    if (Tcl_GetDoubleFromObj(interp, objPtr, &opacity) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((opacity < 0.0) || (opacity > 100.0)) {
	Tcl_AppendResult(interp, "invalid percent opacity \"", 
		Tcl_GetString(objPtr), "\" should be 0 to 100", (char *)NULL);
	return TCL_ERROR;
    }
    opacity = (opacity / 100.0) * 255.0;
    *alphaPtr = ROUND(opacity);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * OpacityToObj --
 *
 *	Convert the picture filter into a string Tcl_Obj.
 *
 * Results:
 *	The string representation of the filter is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
OpacityToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    int *alphaPtr = (int *)(widgRec + offset);
    double opacity;

    opacity = (*alphaPtr / 255.0) * 100.0;
    return Tcl_NewDoubleObj(opacity);
}

/*
 *---------------------------------------------------------------------------
 *
 * NotifyClients --
 *
 *	Notify each client that the background pattern has changed.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
NotifyClients(Pattern *patternPtr)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_FirstLink(patternPtr->chain); link != NULL;
	link = Blt_Chain_NextLink(link)) {
	Background *bgPtr;

	/* Notify each client that the background pattern has changed. The
	 * client should schedule itself for redrawing.  */
	bgPtr = Blt_Chain_GetValue(link);
	if (bgPtr->notifyProc != NULL) {
	    (*bgPtr->notifyProc)(bgPtr->clientData);
	}
    }
}

static const char *
NameOfPattern(Pattern *patternPtr) 
{
    return patternTypes[patternPtr->classPtr->type];
}

static int 
GetPatternTypeFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int *typePtr)
{
    const char *string;
    char c;
    int length;

    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[0];
    if ((c == 't') && (length > 1) && (strncmp(string, "tile", length) == 0)) {
	*typePtr = PATTERN_TILE;
    } else if ((c == 'g') && (strncmp(string, "gradient", length) == 0)) {
	*typePtr = PATTERN_GRADIENT;
    } else if ((c == 's') && (strncmp(string, "solid", length) == 0)) {
	*typePtr = PATTERN_SOLID;
    } else if ((c == 't') && (length > 1)  &&
	       (strncmp(string, "texture", length) == 0)) {
	*typePtr = PATTERN_TEXTURE;
    } else {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "unknown background pattern \"", string, 
		"\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    return TCL_OK;
}

static void
ClearCache(Pattern *corePtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;

    for (hPtr = Blt_FirstHashEntry(&corePtr->pictTable, &iter); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&iter)) {
	Blt_Picture picture;

	picture = Blt_GetHashValue(hPtr);
	Blt_FreePicture(picture);
    }
}

static void 
GetTileOffsets(Tk_Window tkwin, Pattern *patternPtr, Blt_Picture picture, 
	       int x, int y, int *xOffsetPtr, int *yOffsetPtr)
{
    int dx, dy;
    int x0, y0;
    int tw, th;
    Tk_Window refWindow;

    if (patternPtr->reference == REFERENCE_SELF) {
	refWindow = tkwin;
    } else if (patternPtr->reference == REFERENCE_TOPLEVEL) {
	refWindow = Blt_Toplevel(tkwin);
    } else if (patternPtr->reference == REFERENCE_WINDOW) {
	refWindow = patternPtr->refWindow;
    } else if (patternPtr->reference == REFERENCE_NONE) {
	refWindow = NULL;
    } else {
	return;		/* Unknown reference window. */
    }
    if ((patternPtr->reference == REFERENCE_WINDOW) ||
	(patternPtr->reference == REFERENCE_TOPLEVEL)) {
	Tk_Window tkwin2;
	
	tkwin2 = tkwin;
	while ((tkwin2 != refWindow) && (tkwin2 != NULL)) {
	    x += Tk_X(tkwin2) + Tk_Changes(tkwin2)->border_width;
	    y += Tk_Y(tkwin2) + Tk_Changes(tkwin2)->border_width;
	    tkwin2 = Tk_Parent(tkwin2);
	}
	if (tkwin2 == NULL) {
	    /* 
	     * The window associated with the background pattern isn't an
	     * ancestor of the current window. That means we can't use the
	     * reference window as a guide to the size of the picture.  Simply
	     * convert to a self reference.
	     */
	    patternPtr->reference = REFERENCE_SELF;
	    refWindow = tkwin;
	    abort();
	}
    }

    x0 = patternPtr->xOrigin;
    y0 = patternPtr->yOrigin;
    tw = Blt_PictureWidth(picture);
    th = Blt_PictureHeight(picture);

    /* Compute the starting x and y offsets of the tile/gradient from the
     * coordinates of the origin. */
    dx = (x0 - x) % tw;
    if (dx > 0) {
	dx = (tw - dx);
    } else if (dx < 0) {
	dx = x - x0;
    } 
    dy = (y0 - y) % th;
    if (dy > 0) {
	dy = (th - dy);
    } else if (dy < 0) {
	dy = y - y0;
    }
    *xOffsetPtr = dx % tw;
    *yOffsetPtr = dy % th;
#ifdef notdef
    fprintf(stderr, "Tile offsets x0=%d y0=%d x=%d,y=%d sx=%d,sy=%d\n",
	    x0, y0, x, y, *xOffsetPtr, *yOffsetPtr);
#endif
}


static void
Tile(
    Tk_Window tkwin,
    Drawable drawable,
    Pattern *patternPtr,
    Blt_Picture picture,	/* Picture used as the tile. */
    int x, int y, int w, int h)	/* Region of destination picture to be
				 * tiled. */
{
    Blt_Painter painter;
    int xOffset, yOffset;	/* Starting upper left corner of region. */
    int tileWidth, tileHeight;	/* Tile dimensions. */
    int right, bottom, left, top;

    tileWidth = Blt_PictureWidth(picture);
    tileHeight = Blt_PictureHeight(picture);
    GetTileOffsets(tkwin, patternPtr, picture, x, y, &xOffset, &yOffset);

#ifdef notdef
    fprintf(stderr, "tile is (xo=%d,yo=%d,tw=%d,th=%d)\n", 
	patternPtr->xOrigin, patternPtr->yOrigin, tileWidth, tileHeight);
    fprintf(stderr, "region is (x=%d,y=%d,w=%d,h=%d)\n", x, y, w, h);
    fprintf(stderr, "starting offsets at sx=%d,sy=%d\n", xOffset, yOffset);
#endif

    left = x;
    top = y;
    right = x + w;
    bottom = y + h;
    
    painter = Blt_GetPainter(tkwin, 1.0);
    for (y = (top - yOffset); y < bottom; y += tileHeight) {
	int sy, dy, ih;

	if (y < top) {
	    dy = top;
	    ih = MIN(tileHeight - yOffset, bottom - top);
	    sy = yOffset;
	} else {
	    dy = y;
	    ih = MIN(tileHeight, bottom - y);
	    sy = 0;
	}

	for (x = (left - xOffset); x < right; x += tileWidth) {
	    int sx, dx, iw;	

	    if (x < left) {
		dx = left;
		iw = MIN(tileWidth - xOffset, right - left);
		sx = xOffset;
	    } else {
		dx = x;
		iw = MIN(tileWidth, right - x);
		sx = 0;
	    }

	    Blt_PaintPicture(painter, drawable, picture, sx, sy, iw, ih, 
			     dx, dy, /*flags*/0);
#ifdef notdef
	    fprintf(stderr, "drawing pattern (sx=%d,sy=%d,iw=%d,ih=%d) at dx=%d,dy=%d\n",
		    sx, sy, iw, ih, dx, dy);
#endif
	}
    }
}

static void
GetPolygonBBox(XPoint *points, int n, int *leftPtr, int *rightPtr, 
	       int *topPtr, int *bottomPtr)
{
    XPoint *p, *pend;
    int left, right, bottom, top;

    /* Determine the bounding box of the polygon. */
    left = right = points[0].x;
    top = bottom = points[0].y;
    for (p = points, pend = p + n; p < pend; p++) {
	if (p->x < left) {
	    left = p->x;
	} 
	if (p->x > right) {
	    right = p->x;
	}
	if (p->y < top) {
	    top = p->y;
	} 
	if (p->y > bottom) {
	    bottom = p->y;
	}
    }
}

#ifdef notdef
/*
 *---------------------------------------------------------------------------
 *
 * DestroySolidPattern --
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroySolidPattern(Pattern *patternPtr)
{
}
#endif

static int
ConfigureSolidPattern(Tcl_Interp *interp, Pattern *corePtr, int objc, 
	Tcl_Obj *const *objv, unsigned int flags)
{
    SolidPattern *patternPtr = (SolidPattern *)corePtr;

    if (Blt_ConfigureWidgetFromObj(interp, patternPtr->tkwin, 
	patternPtr->classPtr->configSpecs, objc, objv, (char *)patternPtr, 
	flags) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawSolidRectangle --
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawSolidRectangle(Tk_Window tkwin, Drawable drawable, Pattern *corePtr,
		 int x, int y, int w, int h)
{
    SolidPattern *patternPtr = (SolidPattern *)corePtr;

    if ((h <= 0) || (w <= 0)) {
	return;
    }
    if (patternPtr->alpha == 0xFF) {
	Tk_Fill3DRectangle(tkwin, drawable, patternPtr->border, x, y, w, h,
		0, TK_RELIEF_FLAT);
    } else if (patternPtr->alpha != 0x00) {
	Blt_Picture picture;
	Blt_Painter painter;
	Blt_Pixel color;
	
	picture = Blt_DrawableToPicture(tkwin, drawable, x, y, w, h, 1.0);
	if (picture == NULL) {
	    return;		/* Background is obscured. */
	}
	color = Blt_XColorToPixel(Tk_3DBorderColor(patternPtr->border));
	color.Alpha = patternPtr->alpha;
	Blt_PaintRectangle(picture, 0, 0, w, h, 0, 0, &color);
	painter = Blt_GetPainter(tkwin, 1.0);
	Blt_PaintPicture(painter, drawable, picture, 0, 0, w, h, x, y, 0);
	Blt_FreePicture(picture);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawSolidRectangle --
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawSolidPolygon(Tk_Window tkwin, Drawable drawable, Pattern *corePtr, int n, 
		 XPoint *points)
{
    SolidPattern *patternPtr = (SolidPattern *)corePtr;

    if (n <= 2) {
	return;			/* Not enough points for polygon */
    }
    if (patternPtr->alpha == 0xFF) {
	Tk_Fill3DPolygon(tkwin, drawable, patternPtr->border, points, n,
		0, TK_RELIEF_FLAT);
    } else if (patternPtr->alpha != 0x00) {
	Blt_Picture picture;
	Blt_Painter painter;
	Blt_Pixel color;
	int x1, x2, y1, y2;
	int i;
	Point2f *vertices;
	int w, h;

	/* Get polygon bounding box. */
	GetPolygonBBox(points, n, &x1, &x2, &y1, &y2);
	vertices = Blt_AssertMalloc(n * sizeof(Point2f));
	/* Translate the polygon */
	for (i = 0; i < n; i++) {
	    vertices[i].x = (float)(points[i].x - x1);
	    vertices[i].y = (float)(points[i].y - y1);
	}
	w = x2 - x1 + 1;
	h = y2 - y1 + 1;
	picture = Blt_DrawableToPicture(tkwin, drawable, x1, y1, w, h, 1.0);
	if (picture == NULL) {
	    return;		/* Background is obscured. */
	}
	color = Blt_XColorToPixel(Tk_3DBorderColor(patternPtr->border));
	color.Alpha = patternPtr->alpha;
	Blt_PaintPolygon(picture, n, vertices, &color);
	Blt_Free(vertices);
	painter = Blt_GetPainter(tkwin, 1.0);
	Blt_PaintPicture(painter, drawable, picture, 0, 0, w, h, x1, y1, 0);
	Blt_FreePicture(picture);
    }
}

static PatternClass solidPatternClass = {
    PATTERN_SOLID,
    solidConfigSpecs,
    NULL, /* DestroySolidPattern */
    ConfigureSolidPattern,
    DrawSolidRectangle,
    DrawSolidPolygon
};

/*
 *---------------------------------------------------------------------------
 *
 * CreateSolidPattern --
 *
 *	Creates a new solid background pattern.
 *
 * Results:
 *	Returns pointer to the new background pattern.
 *
 *---------------------------------------------------------------------------
 */
static Pattern *
CreateSolidPattern(void)
{
    SolidPattern *patternPtr;

    patternPtr = Blt_Calloc(1, sizeof(SolidPattern));
    if (patternPtr == NULL) {
	return NULL;
    }
    patternPtr->classPtr = &solidPatternClass;
    patternPtr->alpha = 0xFF;
    return (Pattern *)patternPtr;
}

#ifdef notdef
/*
 *---------------------------------------------------------------------------
 *
 * DestroyTilePattern --
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroyTilePattern(Pattern *corePtr)
{
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * DrawTileRectangle --
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawTileRectangle(Tk_Window tkwin, Drawable drawable, Pattern *corePtr,
	int x, int y, int w, int h)
{
    TilePattern *patternPtr = (TilePattern *)corePtr;
    Blt_Painter painter;
    Tk_Window refWindow;

    if ((h <= 0) || (w <= 0)) {
	return;
    }
    if (patternPtr->tkImage == NULL) {
	/* No image so draw solid color background using border. */
	Tk_Fill3DRectangle(tkwin, drawable, patternPtr->border, x, y, w, h,
		0, TK_RELIEF_FLAT);
	return;
    }
    if (patternPtr->flags & BG_SCALE) {
	Blt_Picture picture;
	int refWidth, refHeight;
	Blt_HashEntry *hPtr;

	hPtr = NULL;
	picture = NULL;
	refWidth = w, refHeight = h;
	if (patternPtr->reference != REFERENCE_NONE) {
	    int isNew;

	    /* See if a picture has previously been generated. There will be a
	     * picture for each reference window. */
	    hPtr = Blt_CreateHashEntry(&patternPtr->pictTable, 
		(char *)refWindow, &isNew);
	    if (!isNew) {
		picture = Blt_GetHashValue(hPtr);
	    } 
	    refWidth = Tk_Width(refWindow);
	    refHeight = Tk_Height(refWindow);
	}
	if ((picture == NULL) || 
	    (Blt_PictureWidth(picture) != refWidth) ||
	    (Blt_PictureHeight(picture) != refHeight)) {
	    Blt_Picture original;
	    int isNew;
	    
	    /* 
	     * Either the size of the reference window has changed or one of
	     * the background pattern options has been reset. Resize the
	     * picture if necessary and regenerate the background.
	     */
	    if (picture == NULL) {
		picture = Blt_CreatePicture(refWidth, refHeight);
		if (hPtr != NULL) {
		    Blt_SetHashValue(hPtr, picture);
		}
	    } else {
		Blt_ResizePicture(picture, refWidth, refHeight);
	    }
	    original = ImageToPicture(patternPtr, &isNew);
	    if (original != NULL) {
		Blt_ResamplePicture(picture, original, patternPtr->filter, 
				    patternPtr->filter);
		if (isNew) {
		    Blt_FreePicture(original);
		}
	    }
	}
	Blt_PaintPicture(painter, drawable, picture, 0, 0, w, h, x, y, 0);
    } else {
	int isNew;
	Blt_Picture picture;

	picture = ImageToPicture(patternPtr, &isNew);
	Tile(tkwin, drawable, corePtr, picture, x, y, w, h);
	if (isNew) {
	    Blt_FreePicture(picture);
	}
    }
}

static int
ConfigureTilePattern(Tcl_Interp *interp, Pattern *corePtr, int objc, 
	Tcl_Obj *const *objv, unsigned int flags)
{
    TilePattern *patternPtr = (TilePattern *)corePtr;

    if (Blt_ConfigureWidgetFromObj(interp, patternPtr->tkwin, 
	patternPtr->classPtr->configSpecs, objc, objv, (char *)patternPtr, 
	flags) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

static PatternClass tilePatternClass = {
    PATTERN_TILE,
    tileConfigSpecs,
    NULL, /* DestroyTilePattern, */
    ConfigureTilePattern,
    DrawTileRectangle
};

/*
 *---------------------------------------------------------------------------
 *
 * CreateTilePattern --
 *
 *	Creates a new image background pattern.
 *
 * Results:
 *	Returns pointer to the new background pattern.
 *
 *---------------------------------------------------------------------------
 */
static Pattern *
CreateTilePattern(void)
{
    TilePattern *patternPtr;

    patternPtr = Blt_Calloc(1, sizeof(TilePattern));
    if (patternPtr == NULL) {
	return NULL;
    }
    patternPtr->classPtr = &tilePatternClass;
    patternPtr->reference = REFERENCE_TOPLEVEL;
    return (Pattern *)patternPtr;
}

#ifdef notdef
/*
 *---------------------------------------------------------------------------
 *
 * DestroyGradientPattern --
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroyGradientPattern(Pattern *patternPtr)
{
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * DrawGradientRectangle --
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawGradientRectangle(Tk_Window tkwin, Drawable drawable, Pattern *corePtr, 
	int x, int y, int w, int h)
{
    GradientPattern *patternPtr = (GradientPattern *)corePtr;
    int sx, sy;
    Tk_Window refWindow;
    Blt_Picture picture;
    int refWidth, refHeight;
    Blt_HashEntry *hPtr;

    if ((h <= 0) || (w <= 0)) {
	return;
    }
    if (patternPtr->reference == REFERENCE_SELF) {
	refWindow = tkwin;
    } else if (patternPtr->reference == REFERENCE_TOPLEVEL) {
	refWindow = Blt_Toplevel(tkwin);
    } else if (patternPtr->reference == REFERENCE_WINDOW) {
	refWindow = patternPtr->refWindow;
    } else if (patternPtr->reference == REFERENCE_NONE) {
	refWindow = NULL;
    } else {
	return;		/* Unknown reference window. */
    }

    hPtr = NULL;
    picture = NULL;
    refWidth = w, refHeight = h;
    sx = x, sy = y;
    if (patternPtr->reference != REFERENCE_NONE) {
	int isNew;
	    
	if ((patternPtr->reference == REFERENCE_WINDOW) ||
	    (patternPtr->reference == REFERENCE_TOPLEVEL)) {
	    Tk_Window tkwin2;
	    
	    tkwin2 = tkwin;
	    while ((tkwin2 != refWindow) && (tkwin2 != NULL)) {
		sx += Tk_X(tkwin2) + Tk_Changes(tkwin2)->border_width;
		sy += Tk_Y(tkwin2) + Tk_Changes(tkwin2)->border_width;
		tkwin2 = Tk_Parent(tkwin2);
	    }
	    if (tkwin2 == NULL) {
		/* 
		 * The window associated with the background pattern isn't an
		 * ancestor of the current window. That means we can't use the
		 * reference window as a guide to the size of the picture.
		 * Simply convert to a self reference.
		 */
		patternPtr->reference = REFERENCE_SELF;
		refWindow = tkwin;
		sx = x, sy = y;	
	    }
	}
	/* See if a picture has previously been generated. There will be a
	 * picture for each reference window. */
	hPtr = Blt_CreateHashEntry(&patternPtr->pictTable, (char *)refWindow, 
		&isNew);
	if (!isNew) {
	    picture = Blt_GetHashValue(hPtr);
	} 
	refWidth = Tk_Width(refWindow);
	refHeight = Tk_Height(refWindow);
    }
    if (patternPtr->reference == REFERENCE_SELF) {
	refWidth = Tk_Width(refWindow);
	refHeight = Tk_Height(refWindow);
	sx = x, sy = y;
    }
    if ((picture == NULL) || 
	(Blt_PictureWidth(picture) != refWidth) ||
	(Blt_PictureHeight(picture) != refHeight)) {
	/* 
	 * Either the size of the reference window has changed or one of the
	 * background pattern options has been reset. Resize the picture if
	 * necessary and regenerate the background.
	 */
	if (picture == NULL) {
	    picture = Blt_CreatePicture(refWidth, refHeight);
	    if (hPtr != NULL) {
		Blt_SetHashValue(hPtr, picture);
	    }
	} else {
	    Blt_ResizePicture(picture, refWidth, refHeight);
	}
	Blt_GradientPicture(picture, &patternPtr->high, &patternPtr->low, 
		&patternPtr->gradient);
    }
    Tile(tkwin, drawable, corePtr, picture, x, y, w, h);
#ifdef notdef
    painter = Blt_GetPainter(tkwin, 1.0);
    Blt_PaintPicture(painter, drawable, picture, sx, sy, w, h, x, y, 0);
#endif
}

static int
ConfigureGradientPattern(Tcl_Interp *interp, Pattern *corePtr, int objc, 
	Tcl_Obj *const *objv, unsigned int flags)
{
    GradientPattern *patternPtr = (GradientPattern *)corePtr;

    if (Blt_ConfigureWidgetFromObj(interp, patternPtr->tkwin, 
	patternPtr->classPtr->configSpecs, objc, objv, (char *)patternPtr, 
	flags) != TCL_OK) {
	return TCL_ERROR;
    }
    if (patternPtr->alpha != 0xFF) {
	patternPtr->low.Alpha = patternPtr->alpha;
	patternPtr->high.Alpha = patternPtr->alpha;
    }
    return TCL_OK;
}

static PatternClass gradientPatternClass = {
    PATTERN_GRADIENT,
    gradientConfigSpecs,
    NULL, /* DestroyGradientPattern, */
    ConfigureGradientPattern, 
    DrawGradientRectangle
};

/*
 *---------------------------------------------------------------------------
 *
 * CreateGradientPattern --
 *
 *	Creates a new solid background pattern.
 *
 * Results:
 *	Returns pointer to the new background pattern.
 *
 *---------------------------------------------------------------------------
 */
static Pattern *
CreateGradientPattern(void)
{
    GradientPattern *patternPtr;

    patternPtr = Blt_Calloc(1, sizeof(GradientPattern));
    if (patternPtr == NULL) {
	return NULL;
    }
    patternPtr->classPtr = &gradientPatternClass;
    patternPtr->reference = REFERENCE_TOPLEVEL;
    patternPtr->gradient.shape = BLT_GRADIENT_SHAPE_LINEAR;
    patternPtr->gradient.path = BLT_GRADIENT_PATH_Y;
    patternPtr->gradient.jitter = FALSE;
    patternPtr->gradient.logScale = TRUE;
    return (Pattern *)patternPtr;
}

#ifdef notdef
/*
 *---------------------------------------------------------------------------
 *
 * DestroyTexturePattern --
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroyTexturePattern(Pattern *patternPtr)
{
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * DrawTextureRectangle --
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawTextureRectangle(Tk_Window tkwin, Drawable drawable, Pattern *corePtr, 
	int x, int y, int w, int h)
{
    Blt_Painter painter;
    Blt_Picture picture;
    TexturePattern *patternPtr = (TexturePattern *)corePtr;
    Tk_Window refWindow;
    int refWidth, refHeight;
    Blt_HashEntry *hPtr;

    if ((h <= 0) || (w <= 0)) {
	return;
    }
    if (patternPtr->reference == REFERENCE_SELF) {
	refWindow = tkwin;
    } else if (patternPtr->reference == REFERENCE_TOPLEVEL) {
	refWindow = Blt_Toplevel(tkwin);
    } else if (patternPtr->reference == REFERENCE_WINDOW) {
	refWindow = patternPtr->refWindow;
    } else if (patternPtr->reference == REFERENCE_NONE) {
	refWindow = NULL;
    } else {
	return;		/* Unknown reference window. */
    }
    painter = Blt_GetPainter(tkwin, 1.0);

    picture = NULL;
    refWidth = w, refHeight = h;
    if (patternPtr->reference != REFERENCE_NONE) {
	int isNew;
	    
	/* See if a picture has previously been generated. There will be a
	 * picture for each reference window. */
	hPtr = Blt_CreateHashEntry(&patternPtr->pictTable, (char *)refWindow, 
				   &isNew);
	if (!isNew) {
	    picture = Blt_GetHashValue(hPtr);
	} 
	refWidth = Tk_Width(refWindow);
	refHeight = Tk_Height(refWindow);
    }
    if ((picture == NULL) || 
	(Blt_PictureWidth(picture) != refWidth) ||
	(Blt_PictureHeight(picture) != refHeight)) {
	
	/* 
	 * Either the size of the reference window has changed or one of the
	 * background pattern options has been reset. Resize the picture if
	 * necessary and regenerate the background.
	 */
	if (picture == NULL) {
	    picture = Blt_CreatePicture(refWidth, refHeight);
	    if (hPtr != NULL) {
		Blt_SetHashValue(hPtr, picture);
	    }
	} else {
	    Blt_ResizePicture(picture, refWidth, refHeight);
	}
	Blt_TexturePicture(picture, &patternPtr->high, &patternPtr->low, 0);
    }
    Blt_PaintPicture(painter, drawable, picture, 0, 0, w, h, x, y, 0);
}

static int
ConfigureTexturePattern(Tcl_Interp *interp, Pattern *corePtr, int objc, 
	Tcl_Obj *const *objv, unsigned int flags)
{
    TexturePattern *patternPtr = (TexturePattern *)corePtr;

    if (Blt_ConfigureWidgetFromObj(interp, patternPtr->tkwin, 
	patternPtr->classPtr->configSpecs, objc, objv, (char *)patternPtr, 
	flags) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

static PatternClass texturePatternClass = {
    PATTERN_TEXTURE,
    textureConfigSpecs,
    NULL, /* DestroyTexturePattern, */
    ConfigureTexturePattern,
    DrawTextureRectangle
};

/*
 *---------------------------------------------------------------------------
 *
 * CreateTexturePattern --
 *
 *	Creates a new texture background pattern.
 *
 * Results:
 *	Returns pointer to the new background pattern.
 *
 *---------------------------------------------------------------------------
 */
static Pattern *
CreateTexturePattern()
{
    TexturePattern *patternPtr;

    patternPtr = Blt_Calloc(1, sizeof(TexturePattern));
    if (patternPtr == NULL) {
	return NULL;
    }
    patternPtr->classPtr = &texturePatternClass;
    patternPtr->reference = REFERENCE_TOPLEVEL;
    return (Pattern *)patternPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * CreatePattern --
 *
 *	Creates a new background pattern.
 *
 * Results:
 *	Returns pointer to the new background pattern.
 *
 *---------------------------------------------------------------------------
 */
static Pattern *
CreatePattern(BackgroundInterpData *dataPtr, Tcl_Interp *interp, int type)
{
    Pattern *patternPtr;

    switch (type) {
    case PATTERN_SOLID:
	patternPtr = CreateSolidPattern();
	break;
    case PATTERN_TILE:
	patternPtr = CreateTilePattern();
	break;
    case PATTERN_GRADIENT:
	patternPtr = CreateGradientPattern();
	break;
    case PATTERN_TEXTURE:
	patternPtr = CreateTexturePattern();
	break;
    default:
	abort();
	break;
    }
    if (patternPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate background pattern", 
		(char *)NULL);
	return NULL;
    }
    patternPtr->dataPtr = dataPtr;
    Blt_InitHashTable(&patternPtr->pictTable, BLT_ONE_WORD_KEYS);
    patternPtr->chain = Blt_Chain_Create();
    patternPtr->tkwin = Tk_MainWindow(interp);
    patternPtr->display = Tk_Display(patternPtr->tkwin);
    return patternPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * DestroyPattern --
 *
 *	Removes the client from the servers's list of clients and memory used
 *	by the client token is released.  When the last client is deleted, the
 *	server is also removed.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroyPattern(Pattern *patternPtr)
{
    Blt_FreeOptions(patternPtr->classPtr->configSpecs, (char *)patternPtr, 
	patternPtr->display, 0);
    if (patternPtr->classPtr->destroyProc != NULL) {
	(*patternPtr->classPtr->destroyProc)(patternPtr);
    }
    if (patternPtr->border != NULL) {
	Tk_Free3DBorder(patternPtr->border);
    }
    if (patternPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(&patternPtr->dataPtr->patternTable, 
		patternPtr->hashPtr);
    }
    ClearCache(patternPtr);
    Blt_Chain_Destroy(patternPtr->chain);
    Blt_DeleteHashTable(&patternPtr->pictTable);
    Blt_Free(patternPtr);
}


/*
 *---------------------------------------------------------------------------
 *
 * DestroyBackground --
 *
 *	Removes the client from the servers's list of clients and memory used
 *	by the client token is released.  When the last client is deleted, the
 *	server is also removed.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroyBackground(Background *bgPtr)
{
    Pattern *patternPtr = bgPtr->corePtr;

    Blt_Chain_DeleteLink(patternPtr->chain, bgPtr->link);
    if (Blt_Chain_GetLength(patternPtr->chain) <= 0) {
	DestroyPattern(patternPtr);
    }
    Blt_Free(bgPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * GetPatternFromObj --
 *
 *	Retrieves the background pattern named by the given the Tcl_Obj.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static int
GetPatternFromObj(Tcl_Interp *interp, BackgroundInterpData *dataPtr,
		  Tcl_Obj *objPtr, Pattern **patternPtrPtr)
{
    Blt_HashEntry *hPtr;
    const char *string;

    string = Tcl_GetString(objPtr);
    hPtr = Blt_FindHashEntry(&dataPtr->patternTable, string);
    if (hPtr == NULL) {
	Tcl_AppendResult(dataPtr->interp, "can't find background pattern \"", 
		string, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    *patternPtrPtr = Blt_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 * bgpattern create type ?option values?...
 */
static int
CreateOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	 Tcl_Obj *const *objv)
{
    Background *bgPtr;
    BackgroundInterpData *dataPtr = clientData;
    Pattern *patternPtr;
    int type;

    if (GetPatternTypeFromObj(interp, objv[2], &type) != TCL_OK) {
	return TCL_ERROR;
    }
    patternPtr = CreatePattern(dataPtr, interp, type);
    if (patternPtr == NULL) {
	return TCL_ERROR;
    }
    if ((*patternPtr->classPtr->configProc)(interp, patternPtr, 
		objc - 3, objv + 3, 0) != TCL_OK) {
	DestroyPattern(patternPtr);
	return TCL_ERROR;
    }
    /* Create the container for the pattern. */
    bgPtr = Blt_Calloc(1, sizeof(Background));
    if (bgPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate background.", (char *)NULL);
	DestroyPattern(patternPtr);
	return TCL_ERROR;
    }
    /* Generate a unique name for the pattern.  */
    {
	int isNew;
	char name[200];
	Blt_HashEntry *hPtr;

	do {
	    sprintf_s(name, 200, "bgpattern%d", dataPtr->nextId++);
	    hPtr = Blt_CreateHashEntry(&dataPtr->patternTable, name, &isNew);
	} while (!isNew);
	assert(hPtr != NULL);
	assert(patternPtr != NULL);
	Blt_SetHashValue(hPtr, patternPtr);
	patternPtr->hashPtr = hPtr;
	patternPtr->name = Blt_GetHashKey(&dataPtr->patternTable, hPtr);
    }

    /* Add the container to the pattern's list. */
    bgPtr->link = Blt_Chain_Append(patternPtr->chain, bgPtr);
    patternPtr->link = bgPtr->link;
    bgPtr->corePtr = patternPtr;
    Tcl_SetStringObj(Tcl_GetObjResult(interp), patternPtr->name, -1);
    return TCL_OK;
}    

/*
 * bgpattern cget $pattern ?option?...
 */
static int
CgetOp(ClientData clientData, Tcl_Interp *interp, int objc, 
       Tcl_Obj *const *objv)
{
    BackgroundInterpData *dataPtr = clientData;
    Pattern *patternPtr;

    if (GetPatternFromObj(interp, dataPtr, objv[2], &patternPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return Blt_ConfigureValueFromObj(interp, patternPtr->tkwin, 
	patternPtr->classPtr->configSpecs, (char *)patternPtr, objv[3], 0);
}

/*
 * bgpattern configure $pattern ?option?...
 */
static int
ConfigureOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	    Tcl_Obj *const *objv)
{
    BackgroundInterpData *dataPtr = clientData;
    Pattern *patternPtr;
    int flags;

    if (GetPatternFromObj(interp, dataPtr, objv[2], &patternPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    flags = BLT_CONFIG_OBJV_ONLY;
    if (objc == 3) {
	return Blt_ConfigureInfoFromObj(interp, patternPtr->tkwin, 
		patternPtr->classPtr->configSpecs, (char *)patternPtr, 
		(Tcl_Obj *)NULL, flags);
    } else if (objc == 4) {
	return Blt_ConfigureInfoFromObj(interp, patternPtr->tkwin, 
		patternPtr->classPtr->configSpecs, (char *)patternPtr, objv[3], 
		flags);
    } else {
	if ((*patternPtr->classPtr->configProc)(interp, patternPtr, 
		objc-3, objv+3, flags) != TCL_OK) {
	    return TCL_ERROR;
	}
	ClearCache(patternPtr);
	NotifyClients(patternPtr);
	return TCL_OK;
    }
}

/*
 * bgpattern delete $pattern... 
 */
static int
DeleteOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	 Tcl_Obj *const *objv)
{
    BackgroundInterpData *dataPtr = clientData;
    int i;

    for (i = 2; i < objc; i++) {
	Blt_HashEntry *hPtr;
	Pattern *patternPtr;
	const char *name;

	name = Tcl_GetString(objv[i]);
	hPtr = Blt_FindHashEntry(&dataPtr->patternTable, name);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "can't find background pattern \"",
			     name, "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	patternPtr = Blt_GetHashValue(hPtr);
	assert(patternPtr->hashPtr == hPtr);

	/* FIXME: Assuming that the first background token is always
	 * associated with the command. Need to known when pattern was created
	 * by bgpattern command.  Does bgpattern delete #ffffff make sense? */
	/* 
	 * Look up clientData from command hash table. If it's found it 
	 * represents a command?
	 */
	if (patternPtr->link != NULL) {
	    Background *bgPtr;

	    bgPtr = Blt_Chain_GetValue(patternPtr->link);
	    assert(patternPtr->link == bgPtr->link);
	    /* Take the pattern entry out of the hash table.  */
	    Blt_DeleteHashEntry(&patternPtr->dataPtr->patternTable, 
				patternPtr->hashPtr);
	    patternPtr->name = NULL;
	    patternPtr->hashPtr = NULL;
	    patternPtr->link = NULL;  /* Disconnect pattern. */
	    DestroyBackground(bgPtr);
	}
    }
    return TCL_OK;
}

/*
 * bgpattern type $pattern
 */
static int
TypeOp(ClientData clientData, Tcl_Interp *interp, int objc, 
       Tcl_Obj *const *objv)
{
    BackgroundInterpData *dataPtr = clientData;
    Pattern *patternPtr;

    if (GetPatternFromObj(interp, dataPtr, objv[2], &patternPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetStringObj(Tcl_GetObjResult(interp), NameOfPattern(patternPtr), -1);
    return TCL_OK;
}

static Blt_OpSpec patternOps[] =
{
    {"cget",      2, CgetOp,      4, 4, "pattern option",},
    {"configure", 2, ConfigureOp, 3, 0, "pattern ?option value?...",},
    {"create",    2, CreateOp,    3, 0, "type ?args?",},
    {"delete",    1, DeleteOp,    2, 0, "pattern...",},
    {"type",      1, TypeOp,      3, 3, "pattern",},
};
static int nPatternOps = sizeof(patternOps) / sizeof(Blt_OpSpec);

static int
BgPatternCmdProc(ClientData clientData, Tcl_Interp *interp, int objc,
		 Tcl_Obj *const *objv)
{
    Tcl_ObjCmdProc *proc;

    proc = Blt_GetOpFromObj(interp, nPatternOps, patternOps, BLT_OP_ARG1, 
	objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc) (clientData, interp, objc, objv);
}

static void
BgPatternDeleteCmdProc(ClientData clientData) 
{
    BackgroundInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;

    for (hPtr = Blt_FirstHashEntry(&dataPtr->patternTable, &iter); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&iter)) {
	Pattern *patternPtr;
	Blt_ChainLink link, next;

	patternPtr = Blt_GetHashValue(hPtr);
	patternPtr->hashPtr = NULL;
	for (link = Blt_Chain_FirstLink(patternPtr->chain); link != NULL; 
	     link = next) {
	    Background *bgPtr;

	    next = Blt_Chain_NextLink(link);
	    bgPtr = Blt_Chain_GetValue(link);
	    DestroyBackground(bgPtr);
	}
    }
    Blt_DeleteHashTable(&dataPtr->patternTable);
    Tcl_DeleteAssocData(dataPtr->interp, BG_PATTERN_THREAD_KEY);
}

/*
 *---------------------------------------------------------------------------
 *
 * GetBackgroundInterpData --
 *
 *---------------------------------------------------------------------------
 */
static BackgroundInterpData *
GetBackgroundInterpData(Tcl_Interp *interp)
{
    BackgroundInterpData *dataPtr;
    Tcl_InterpDeleteProc *proc;

    dataPtr = (BackgroundInterpData *)
	Tcl_GetAssocData(interp, BG_PATTERN_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_AssertMalloc(sizeof(BackgroundInterpData));
	dataPtr->interp = interp;
	dataPtr->nextId = 1;


	/* FIXME: Create interp delete proc to teardown the hash table and
	 * data entry.  Must occur after all the widgets have been destroyed
	 * (clients of the background pattern). */

	Tcl_SetAssocData(interp, BG_PATTERN_THREAD_KEY, 
		(Tcl_InterpDeleteProc *)NULL, dataPtr);
	Blt_InitHashTable(&dataPtr->patternTable, BLT_STRING_KEYS);
    }
    return dataPtr;
}


/*LINTLIBRARY*/
int
Blt_BgPatternCmdInitProc(Tcl_Interp *interp)
{
    static Blt_InitCmdSpec cmdSpec = {
	"bgpattern", BgPatternCmdProc, BgPatternDeleteCmdProc,
    };
    cmdSpec.clientData = GetBackgroundInterpData(interp);
    return Blt_InitCmd(interp, "::blt", &cmdSpec);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_GetBackground
 *
 *	Creates a new background from the given pattern description.  The
 *	background structure returned is a token for the client to use the
 *	background.  If the pattern isn't a solid pattern (i.e. a solid color
 *	that Tk_Get3DBorder will accept) then the pattern must already exist.
 *	Solid patterns are the exception to this rule.  This lets "-background
 *	#ffffff" work without already having allocated a background pattern
 *	"#ffffff".
 *
 * Results:
 *	Returns a background pattern token.
 *
 * Side Effects:
 *	Memory is allocated for the new token.
 *
 *---------------------------------------------------------------------------
 */
Blt_Background
Blt_GetBackground(Tcl_Interp *interp, Tk_Window tkwin, const char *name)
{
    Pattern *patternPtr;
    BackgroundInterpData *dataPtr;
    Background *bgPtr;		/* Pattern container. */
    Blt_HashEntry *hPtr;
    int isNew;
    
    /* Create new token for the background. */
    bgPtr = Blt_Calloc(1, sizeof(Background));
    if (bgPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate background \"", name, "\".", 
		(char *)NULL);
	return NULL;
    }
    dataPtr = GetBackgroundInterpData(interp);
    hPtr = Blt_CreateHashEntry(&dataPtr->patternTable, name, &isNew);
    if (isNew) {
	Tk_3DBorder border;

	/* Pattern doesn't already exist, see if it's a color name
	 * (i.e. something that Tk_Get3DBorder will accept). */
	border = Tk_Get3DBorder(interp, tkwin, name);
	if (border == NULL) {
	    goto error;		/* Nope. It's an error. */
	} 
	patternPtr = CreatePattern(dataPtr, interp, PATTERN_SOLID);
	if (patternPtr == NULL) {
	    Tk_Free3DBorder(border);
	    goto error;		/* Can't allocate new pattern. */
	}
	patternPtr->border = border;
	patternPtr->hashPtr = hPtr;
	patternPtr->name = Blt_GetHashKey(&dataPtr->patternTable, hPtr);
	patternPtr->link = NULL;
	Blt_SetHashValue(hPtr, patternPtr);
    } else {
	patternPtr = Blt_GetHashValue(hPtr);
	assert(patternPtr != NULL);
    }
    /* Add the new background to the pattern's list of clients. */
    bgPtr->link = Blt_Chain_Append(patternPtr->chain, bgPtr);
    bgPtr->corePtr = patternPtr;
    return bgPtr;
 error:
    Blt_Free(bgPtr);
    Blt_DeleteHashEntry(&dataPtr->patternTable, hPtr);
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_GetBackgroundFromObj
 *
 *	Retrieves a new token of a background pattern from the named background
 *	pattern.
 *
 * Results:
 *	Returns a background pattern token.
 *
 * Side Effects:
 *	Memory is allocated for the new token.
 *
 *---------------------------------------------------------------------------
 */
Blt_Background
Blt_GetBackgroundFromObj(Tcl_Interp *interp, Tk_Window tkwin, Tcl_Obj *objPtr)
{
    return Blt_GetBackground(interp, tkwin, Tcl_GetString(objPtr));
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_SetBackgroundChangedProc
 *
 *	Sets the routine to called when an image changes.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The designated routine will be called the next time the image
 *	associated with the tile changes.
 *
 *---------------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_SetBackgroundChangedProc(
    Background *bgPtr,		/* Background to register callback with. */
    Blt_BackgroundChangedProc *notifyProc, /* Function to call when pattern
					    * has changed. NULL indicates to
					    * unset the callback.*/
    ClientData clientData)
{
    bgPtr->notifyProc = notifyProc;
    bgPtr->clientData = clientData;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_FreeBackground
 *
 *	Removes the background pattern token.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Memory is freed.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_FreeBackground(Background *bgPtr)
{
    Pattern *patternPtr = bgPtr->corePtr;

    assert(patternPtr != NULL);
    DestroyBackground(bgPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_GetBackgroundOrigin
 *
 *	Returns the coordinates of the origin of the background pattern
 *	referenced by the token.
 *
 * Results:
 *	Returns the coordinates of the origin.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_GetBackgroundOrigin(Background *bgPtr, int *xPtr, int *yPtr)
{
    *xPtr = bgPtr->corePtr->xOrigin;
    *yPtr = bgPtr->corePtr->yOrigin;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_SetBackgroundOrigin
 *
 *	Sets the origin of the background pattern referenced by the token.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_SetBackgroundOrigin(Tk_Window tkwin, Background *bgPtr, int x, int y)
{
    Pattern *patternPtr = bgPtr->corePtr;
    patternPtr->xOrigin = x;
    patternPtr->yOrigin = y;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_NameOfBackground
 *
 *	Returns the name of the core background pattern referenced by the
 *	token.
 *
 * Results:
 *	Return the name of the background pattern.
 *
 *---------------------------------------------------------------------------
 */
const char *
Blt_NameOfBackground(Background *bgPtr)
{
    if (bgPtr->corePtr->name == NULL) {
	return "";
    }
    return bgPtr->corePtr->name;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_BackgroundBorderColor
 *
 *	Returns the border color of the background pattern referenced by the
 *	token.
 *
 * Results:
 *	Returns the XColor representing the border color of the pattern.
 *
 *---------------------------------------------------------------------------
 */
XColor *
Blt_BackgroundBorderColor(Background *bgPtr)
{
    return Tk_3DBorderColor(bgPtr->corePtr->border);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_BackgroundBorder
 *
 *	Returns the border of the background pattern referenced by the token.
 *
 * Results:
 *	Return the border of the background pattern.
 *
 *---------------------------------------------------------------------------
 */
Tk_3DBorder
Blt_BackgroundBorder(Background *bgPtr)
{
    return bgPtr->corePtr->border;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_DrawBackgroundRectangle
 *
 *	Draws the background pattern in the designated window.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_DrawBackgroundRectangle(
    Tk_Window tkwin, 
    Drawable drawable, 
    Background *bgPtr, 
    int x, int y, int w, int h, 
    int borderWidth, int relief)
{
    Tk_Draw3DRectangle(tkwin, drawable, bgPtr->corePtr->border, x, y, w, h, 
	borderWidth, relief);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_FillBackgroundRectangle
 *
 *	Draws the background pattern in the designated window.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_FillBackgroundRectangle(
    Tk_Window tkwin, 
    Drawable drawable, 
    Background *bgPtr, 
    int x, int y, int w, int h, 
    int borderWidth, int relief)
{
    Pattern *patternPtr;

    if ((h <= 0) || (w <= 0)) {
	return;
    }
    patternPtr = bgPtr->corePtr;
    (*patternPtr->classPtr->drawRectangleProc)(tkwin, drawable, patternPtr, 
	x, y, w, h);
    if ((relief != TK_RELIEF_FLAT) && (borderWidth > 0)) {
	Tk_Draw3DRectangle(tkwin, drawable, patternPtr->border, x, y, w, h,
		borderWidth, relief);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_DrawBackgroundPolygon
 *
 *	Draws the background pattern in the designated window.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_DrawBackgroundPolygon(
    Tk_Window tkwin, 
    Drawable drawable, 
    Background *bgPtr, 
    XPoint *points, int n,
    int borderWidth, int relief)
{
    Pattern *patternPtr;

#ifdef notdef
    if (n < 3) {
	return;
    }
#endif
    patternPtr = bgPtr->corePtr;
    Tk_Draw3DPolygon(tkwin, drawable, patternPtr->border, points, n,
	borderWidth, relief);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_FillBackgroundPolygon
 *
 *	Draws the background pattern in the designated window.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_FillBackgroundPolygon(
    Tk_Window tkwin, 
    Drawable drawable, 
    Background *bgPtr, 
    XPoint *points, int n,
    int borderWidth, int relief)
{
    Pattern *patternPtr;

    if (n < 3) {
	return;
    }
    patternPtr = bgPtr->corePtr;
    (*patternPtr->classPtr->drawPolygonProc)(tkwin, drawable, patternPtr, 
	n, points);
    if ((relief != TK_RELIEF_FLAT) && (borderWidth != 0)) {
	Tk_Draw3DPolygon(tkwin, drawable, patternPtr->border, points, n,
		borderWidth, relief);
    }
}

#ifdef notdef
/*
 *---------------------------------------------------------------------------
 *
 * Blt_FillPictureBackground
 *
 *	Draws the background pattern in the designated picture.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_FillPictureBackground(
    Tk_Window tkwin, 
    Blt_Picture dest,
    Background *bgPtr, 
    int x, int y, int w, int h, 
    int borderWidth, int relief)
{
    Pattern *patternPtr;
    Blt_Picture picture;
    int sx, sy;

    patternPtr = bgPtr->corePtr;
    if (patternPtr->classPtr->pattern == PATTERN_BORDER) {
	return;
    } 
    picture = GetBackgroundPicture(patternPtr, tkwin, x, y, &sx, &sy);
    if (picture == NULL) {
	return;
    }
    Blt_CopyPictureBits(dest, picture, sx, sy, w, h, x, y);
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * Blt_DrawFocusBackground
 *
 *	Draws the background pattern in the designated picture.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_DrawFocusBackground(Tk_Window tkwin, Background *bgPtr, 
			int highlightThickness, Drawable drawable)
{
    Pattern *patternPtr = (Pattern *)bgPtr->corePtr;
    int w, h;

    w = Tk_Width(tkwin);
    h = Tk_Height(tkwin);
    /* Top */
    (*patternPtr->classPtr->drawRectangleProc)(tkwin, drawable, patternPtr, 
	0, 0, w, highlightThickness);
    /* Bottom */
    (*patternPtr->classPtr->drawRectangleProc)(tkwin, drawable, patternPtr, 
	0, h - highlightThickness, w, highlightThickness);
    /* Left */
    (*patternPtr->classPtr->drawRectangleProc)(tkwin, drawable, patternPtr,
	0, highlightThickness, highlightThickness, h - 2 * highlightThickness);
    /* Right */
    (*patternPtr->classPtr->drawRectangleProc)(tkwin, drawable, patternPtr,
	w - highlightThickness, highlightThickness, highlightThickness, 
	h - 2 * highlightThickness);
}


#ifdef notdef
static void 
Draw3DRectangle(
    Tk_Window tkwin, 
    Drawable drawable, 
    Background *bgPtr, 
    int x, int y, int w, int h,
    int borderWidth, int relief)
{
    int nSegments;
    XSegment *segments, *sp;
    int i;

    nSegments = borderWidth + borderWidth;
    segments = Blt_AssertMalloc(sizeof(XSegment) * nSegments);
    sp = segments;
    for (i = 0; i < borderWidth; i++) {
	sp->x1 = x + i;
	sp->y1 = y + i;
	sp->x2 = x + (w - 1) - i;
	sp->y2 = y + i;
	sp++;
	sp->x1 = x + i;
	sp->y1 = y + i;
	sp->x2 = x + i;
	sp->y2 = y + (h - 1) - i;
	sp++;
    }
    gc = Tk_3DBorderGC(tkwin, bgPtr->corePtr->border, TK_3D_LIGHT_GC);
    XDrawSegments(Tk_Display(tkwin), drawable, gc, segments, nSegments);

    sp = segments;
    for (i = 0; i < borderWidth; i++) {
	sp->x1 = x + i;
	sp->y1 = y + (h - 1) - i;
	sp->x2 = x + (w - 1) - i;
	sp->y2 = y + (h - 1) - i;
	sp++;
	sp->x1 = x + (w - 1 ) - i;
	sp->y1 = y + i;
	sp->x2 = x + (w - 1) - i;
	sp->y2 = y + (h - 1) - i;
	sp++;
    }
    gc = Tk_3DBorderGC(tkwin, bgPtr->corePtr->border, TK_3D_DARK_GC);
    XDrawSegments(Tk_Display(tkwin), drawable, gc, segments, nSegments);
}
#endif

#ifdef notdef
void
Blt_SetBackgroundRegion(Background *bgPtr, int x, int y, int w, int h)
{
    if (bgPtr->corePtr->reference == REFERENCE_NONE) {
	bgPtr->corePtr->refRegion.x = x;
	bgPtr->corePtr->refRegion.y = y;
	bgPtr->corePtr->refRegion.width = w;
	bgPtr->corePtr->refRegion.height = h;
    }
}
#endif

void
Blt_SetBackgroundFromBackground(Tk_Window tkwin, Background *bgPtr)
{
    Tk_SetBackgroundFromBorder(tkwin, bgPtr->corePtr->border);
}

GC
Blt_BackgroundBorderGC(Tk_Window tkwin, Background *bgPtr, int which)
{
    return Tk_3DBorderGC(tkwin, bgPtr->corePtr->border, which);
}

