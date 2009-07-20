
/*
 * bltPictCmd.c --
 *
 * This module implements the Tk image interface for picture images.
 *
 *	Copyright 2003-2004 George A Howlett.
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
#include <bltHash.h>
#include <bltImage.h>
#include <bltDBuffer.h>
#include "bltSwitch.h"
#include "bltPicture.h"
#include "bltPictInt.h"
#include "bltPictFmts.h"
#include "bltPainter.h"

#include "bltPs.h"
#include <X11/Xutil.h>

#include <sys/types.h>
#include <sys/stat.h>

typedef struct _Blt_DBuffer DBuffer;

typedef int (PictCmdProc)(Blt_Picture picture, Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv);

#if (_TCL_VERSION < _VERSION(8,2,0)) 
typedef struct _Tk_PostscriptInfo *Tk_PostscriptInfo;
#endif

/*
 * PictureCmdInterpData --
 *
 *	Structure containing global data, used on a interpreter by interpreter
 *	basis.
 *
 *	This structure holds the hash table of instances of datatable commands
 *	associated with a particular interpreter.
 */
typedef struct {
    Tcl_Interp *interp;
} PictureCmdInterpData;

/*
 * Various external file/string formats handled by the picture image.
 */

enum PictureFormats {
    FMT_JPG, /* Joint Photographic Experts Group r/w */
    FMT_PNG, /* Portable Network Graphics r/w */
    FMT_TIF, /* Tagged Image File Format r/w */
    FMT_XPM, /* X Pixmap r/w */
    FMT_XBM, /* X Bitmap r/w */
    FMT_GIF, /* Graphics Interchange Format r/w */
    FMT_PS,  /* PostScript r/w */
    FMT_PDF, /* Portable Document Format w TBA */
    FMT_PHO, /* Tk Photo Image r/w */
    FMT_BMP, /* Device-independent bitmap r/w */
    FMT_PBM, /* Portable Bitmap Format r/w */
    FMT_EMF, /* Enhanced Metafile Format r/w (Windows only) TBA */
    FMT_WMF, /* Windows Metafile Format r/w (Windows only) TBA */
    NUMFMTS
};

typedef struct {
    const char *name;			/* Name of format. */
    unsigned int flags;
    Blt_PictureIsFmtProc *isFmtProc;
    Blt_PictureReadDataProc *readProc;  /* Used for -file and -data
					 * configuration options. */
    Blt_PictureWriteDataProc *writeProc;/* Used for cget -data. */
    Blt_PictureImportProc *importProc;
    Blt_PictureExportProc *exportProc;
} PictFormat;

static PictFormat pictFormats[] = {
    { "jpg" },
    { "png" },
    { "tif" },			/* Multi-page */
    { "xpm" },
    { "xbm" },
    { "gif" },			/* Multi-page */
    { "ps"  },			/* Multi-page */
    { "pdf" },			/* Not implemented yet. */
    { "photo" },
    { "bmp" },
    { "pbm" },			/* Multi-page */
    { "emf" },
    { "wmf" },
};

static Blt_HashTable fmtTable;


/*
 * Default configuration options for picture images. 
 */
#define DEF_PIC_ANGLE		"0.0"
#define DEF_PIC_ASPECT		"yes"
#define DEF_PIC_CACHE		"no"
#define DEF_PIC_DITHER		"no"
#define DEF_PIC_DATA		(char *)NULL
#define DEF_PIC_FILE		(char *)NULL
#define DEF_PIC_FILTER		(char *)NULL
#define DEF_PIC_GAMMA		"1.0"
#define DEF_PIC_HEIGHT		"0"
#define DEF_PIC_WIDTH		"0"
#define DEF_PIC_WINDOW		(char *)NULL
#define DEF_PIC_IMAGE		(char *)NULL
#define DEF_PIC_SHARPEN		"no"

#define DEF_PIC_OPAQUE		"no"
#define DEF_PIC_OPAQUE_BACKGROUND	"white"

#define IMPORTED_NONE		0
#define IMPORTED_FILE		(1<<0)
#define IMPORTED_IMAGE		(1<<1)
#define IMPORTED_WINDOW		(1<<2)
#define IMPORTED_DATA		(1<<3)
#define IMPORTED_MASK	\
	(IMPORTED_FILE|IMPORTED_IMAGE|IMPORTED_WINDOW|IMPORTED_DATA)

/*
 * A PictImage implements a Tk_ImageMaster for the "picture" image type.  It
 * represents a set of bits (i.e. the picture), some options, and operations
 * (sub-commands) to manipulate the picture.
 *
 * The PictImage manages the TCL interface to a picture (using Tk's "image"
 * command).  Pictures and the mechanics of drawing the picture to the display
 * (painters) are orthogonal.  The PictImage knows nothing about the display
 * type (the display is stored only to free options when it's destroyed).
 * Information specific to the visual context (combination of display, visual,
 * depth, colormap, and gamma) is stored in each cache entry.  The picture
 * image manages the various picture transformations: reading, writing,
 * scaling, rotation, etc.
 */
typedef struct _Blt_PictureImage {
    Tk_ImageMaster imgToken;		/* Tk's token for image master.  If
					 * NULL, the image has been deleted. */

    Tcl_Interp *interp;			/* Interpreter associated with the
					 * application using this image. */

    Display *display;			/* Display associated with this picture
					 * image.  This is used to free the 
					 * configuration options. */
    Colormap colormap;

    Tcl_Command cmdToken;		/* Token for image command (used to
					 * delete the command when the image
					 * goes away).  NULL means the image
					 * command has already been deleted. */

    unsigned int flags;			/* Various bit-field flags defined
					 * below. */

    Blt_Chain chain;			/* List of pictures. (multi-page
					 * formats)  */

    Blt_Picture picture;		/* Current picture displayed. */
    
    /* User-requested options. */
    float angle;			/* Angle in degrees to rotate the
					 * image. */

    int reqWidth, reqHeight;		/* User-requested size of picture. The
					 * picture is scaled accordingly.  These
					 * dimensions may or may not be used,
					 * depending * upon the -aspect
					 * option. */

    int aspect;				/* If non-zero, indicates to maintain
					 * the minimum aspect ratio while
					 * scaling. The larger dimension is
					 * discarded. */

    int dither;				/* If non-zero, dither the picture
					 * before drawing. */
    int sharpen;			/* If non-zero, indicates to sharpen the
					 * image. */

    Blt_ResampleFilter filter;		/* 1D Filter to use when the picture is
					 * resampled (resized). The same filter
					 * is applied both horizontally and
					 * vertically. */

    float gamma;			/* Gamma correction value of the
					 * monitor. In theory, the same picture
					 * image may be displayed on two
					 * monitors simultaneously (using
					 * xinerama).  Here we're assuming
					 * (almost certainly wrong) that both
					 * monitors will have the same gamma
					 * value. */

    const char *name;			/* Name of the image, file, or window
					 * read into the picture image. */

    unsigned int index;			/* Index of the picture in the above
					 * list. */

    Tcl_TimerToken timerToken;		/* Token for timer handler which polls
					 * for the exit status of each
					 * sub-process. If zero, there's no
					 * timer handler queued. */
    int interval;

    PictFormat *fmtPtr;			/* External format of last image read
					 * into the picture image. We use this
					 * to write back the same format if the
					 * user doesn't specify the format. */

    int doCache;			/* If non-zero, indicates to generate a
					 * pixmap of the picture. The pixmap is
					 * cached * in the table below. */

    Blt_HashTable cacheTable;		/* Table of cache entries specific to
					 * each visual context where this
					 * picture is displayed. */

} PictImage;


/*
 * A PictCacheKey represents the visual context of a cache entry. type.  It
 * contains information specific to the visual context (combination of display,
 * visual, depth, colormap, and gamma).  It is used as a hash table key for
 * cache entries of picture images.  The same picture may be displayed in more
 * than one visual context.
 */
typedef struct {
    Display *display;			/* Display where the picture will be
					 * drawn. Used to free colors allocated
					 * by the painter. */

    Visual *visualPtr;			/* Visual information of window
					 * displaying the image. */

    Colormap colormap;			/* Colormap used.  This may be the
					 * default colormap, or an allocated
					 * private map. */

    int depth;				/* Depth of the display. */

    unsigned int index;			/* Index of the picture in the list. */

    float gamma;			/* Gamma correction value */
} PictCacheKey;


/*
 * PictInstances (image instances in the Tk parlance) represent a picture image
 * in some specific combination of visual, display, colormap, depth, and output
 * gamma.  Cache entries are stored by each picture image.
 *
 * The purpose is to 1) allocate and hold the painter-specific to the visual and
 * 2) provide caching of XImage's (drawn pictures) into pixmaps.  This feature
 * is enabled only for 100% opaque pictures.  If the picture must be blended
 * with the current background, there is no guarantee (between redraws) that the
 * background will not have changed.  This feature is widget specific. There's
 * no simple way to detect when the pixmap must be redrawn.  In general, we
 * should rely on the widget itself to perform its own caching of complex
 * scenes.
 */
typedef struct {
    Blt_PictureImage image;		/* The picture image represented by this
					 * entry. */

    Blt_Painter painter;		/* The painter allocated for this
					 * particular combination of visual,
					 * display, colormap, depth, and
					 * gamma. */

    Display *display;			/* Used to free the pixmap below when
					 * the entry is destroyed. */

    Blt_HashEntry *hashPtr;		/* These two fields allow the cache */
    Blt_HashTable *tablePtr;		/* entry to be deleted from the picture
					 * image's table of entries. */

    Pixmap pixmap;			/* If non-NULL, is a cached pixmap of
					 * the picture. It's recreated each time
					 * the * picture changes. */

    int refCount;			/* This entry may be shared by all
					 * clients displaying this picture image
					 * with the same painter. */
} PictInstance;


static Blt_OptionParseProc ObjToFile;
static Blt_OptionPrintProc FileToObj;
static Blt_CustomOption fileOption =
{
    ObjToFile, FileToObj, NULL, (ClientData)0
};

static Blt_OptionParseProc ObjToData;
static Blt_OptionPrintProc DataToObj;
static Blt_CustomOption dataOption =
{
    ObjToData, DataToObj, NULL, (ClientData)0
};

static Blt_OptionParseProc ObjToFilter;
static Blt_OptionPrintProc FilterToObj;
Blt_CustomOption bltFilterOption =
{
    ObjToFilter, FilterToObj, NULL, (ClientData)0
};

static Blt_OptionParseProc ObjToImage;
static Blt_OptionPrintProc ImageToObj;
static Blt_CustomOption imageOption =
{
    ObjToImage, ImageToObj, NULL, (ClientData)0
};

static Blt_OptionParseProc ObjToWindow;
static Blt_OptionPrintProc WindowToObj;
static Blt_CustomOption windowOption =
{
    ObjToWindow, WindowToObj, NULL, (ClientData)0
};

static Blt_ConfigSpec configSpecs[] =
{
    {BLT_CONFIG_BOOLEAN, "-aspect", (char *)NULL, (char *)NULL, 
	DEF_PIC_ASPECT, Blt_Offset(PictImage, aspect), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-data", (char *)NULL, (char *)NULL, DEF_PIC_DATA, 
	Blt_Offset(PictImage, picture), 0, &dataOption},
    {BLT_CONFIG_BOOLEAN, "-dither", (char *)NULL, (char *)NULL, 
	DEF_PIC_DITHER, Blt_Offset(PictImage, dither), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-file", (char *)NULL, (char *)NULL, DEF_PIC_DATA, 
	Blt_Offset(PictImage, picture), 0, &fileOption},
    {BLT_CONFIG_CUSTOM, "-filter", (char *)NULL, (char *)NULL, 
	DEF_PIC_FILTER, Blt_Offset(PictImage, filter), 0, 
        &bltFilterOption},
    {BLT_CONFIG_FLOAT, "-gamma", (char *)NULL, (char *)NULL, DEF_PIC_GAMMA,
	Blt_Offset(PictImage, gamma), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-height", (char *)NULL, (char *)NULL,
	DEF_PIC_HEIGHT, Blt_Offset(PictImage, reqHeight), 0},
    {BLT_CONFIG_CUSTOM, "-image", (char *)NULL, (char *)NULL, DEF_PIC_IMAGE,
	Blt_Offset(PictImage, picture), 0, &imageOption},
    {BLT_CONFIG_FLOAT, "-rotate", (char *)NULL, (char *)NULL, 
	DEF_PIC_ANGLE, Blt_Offset(PictImage, angle), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BOOLEAN, "-sharpen", (char *)NULL, (char *)NULL, 
	DEF_PIC_SHARPEN, Blt_Offset(PictImage, sharpen), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-width", (char *)NULL, (char *)NULL,
	DEF_PIC_WIDTH, Blt_Offset(PictImage, reqWidth), 0},
    {BLT_CONFIG_CUSTOM, "-window", (char *)NULL, (char *)NULL, 
	DEF_PIC_WINDOW, Blt_Offset(PictImage, picture), 0, 
	&windowOption},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

BLT_EXTERN Blt_SwitchParseProc Blt_ColorSwitchProc;
static Blt_SwitchCustom colorSwitch = {
    Blt_ColorSwitchProc, NULL, (ClientData)0,
};

static Blt_SwitchParseProc BBoxSwitch;
static Blt_SwitchCustom bboxSwitch = {
    BBoxSwitch, NULL, (ClientData)0,
};

static Blt_SwitchParseProc FilterSwitch;
static Blt_SwitchCustom filterSwitch = {
    FilterSwitch, NULL, (ClientData)0,
};

static Blt_SwitchParseProc BlendingModeSwitch;
static Blt_SwitchCustom blendModeSwitch = {
    BlendingModeSwitch, NULL, (ClientData)0,
};

static Blt_SwitchParseProc ShapeSwitch;
static Blt_SwitchCustom shapeSwitch = {
    ShapeSwitch, NULL, (ClientData)0,
};

static Blt_SwitchParseProc PathSwitch;
static Blt_SwitchCustom pathSwitch = {
    PathSwitch, NULL, (ClientData)0,
};

typedef struct {
    Blt_Pixel bg, fg;			/* Fg and bg colors. */
    Blt_Gradient gradient;
} GradientSwitches;

static Blt_SwitchSpec gradientSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-direction", "direction",
	Blt_Offset(GradientSwitches, gradient.path),  0, 0, &pathSwitch},
    {BLT_SWITCH_CUSTOM, "-shape",     "shape",
	Blt_Offset(GradientSwitches, gradient.shape), 0, 0, &shapeSwitch},
    {BLT_SWITCH_CUSTOM, "-high",      "color",
	Blt_Offset(GradientSwitches, fg),             0, 0, &colorSwitch},
    {BLT_SWITCH_CUSTOM, "-low",       "color",
	Blt_Offset(GradientSwitches, bg),             0, 0, &colorSwitch},
    {BLT_SWITCH_BOOLEAN,"-logscale",  "bool",
	Blt_Offset(GradientSwitches, gradient.logScale), 0},
    {BLT_SWITCH_BOOLEAN, "-jitter",   "bool",
	Blt_Offset(GradientSwitches, gradient.jitter),   0},
    {BLT_SWITCH_END}
};

typedef struct {
    PictRegion region;			/* Area to tile. */
    int xOrigin;
    int yOrigin;
} TileSwitches;

static Blt_SwitchSpec tileSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-region",  "bbox",
	Blt_Offset(TileSwitches, region), 0, 0, &bboxSwitch},
    {BLT_SWITCH_INT,    "-xorigin", "x",
	Blt_Offset(TileSwitches, xOrigin),  0},
    {BLT_SWITCH_INT,    "-yorigin", "y",
	Blt_Offset(TileSwitches, yOrigin),  0},
    {BLT_SWITCH_END}
};

typedef struct {
    int invert;				/* Flag. */
    Tcl_Obj *maskObjPtr;
} ArithSwitches;

static Blt_SwitchSpec arithSwitches[] = 
{
    {BLT_SWITCH_OBJ,     "-mask",   "mask",
	Blt_Offset(ArithSwitches, maskObjPtr), 0},
    {BLT_SWITCH_BOOLEAN, "-invert", "bool",
	Blt_Offset(ArithSwitches, invert), 0},
    {BLT_SWITCH_END}
};

typedef struct {
    PictRegion region;			/* Area to crop. */
    int nocopy;				/* If non-zero, don't copy the source
					 * image. */
} DupSwitches;

#define DUP_NOCOPY	1

static Blt_SwitchSpec dupSwitches[] = 
{
    {BLT_SWITCH_BITMASK,"-nocopy", "",
	Blt_Offset(DupSwitches, nocopy), 0, DUP_NOCOPY},
    {BLT_SWITCH_CUSTOM, "-region", "bbox",
	Blt_Offset(DupSwitches, region), 0, 0, &bboxSwitch},
    {BLT_SWITCH_END}
};

typedef struct {
    Blt_BlendingMode mode;		/* Blending mode. */
} BlendSwitches;

static Blt_SwitchSpec blendSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-mode", "blendingmode",
	Blt_Offset(BlendSwitches, mode), 0, 0, &blendModeSwitch},
    {BLT_SWITCH_END}
};

typedef struct {
    Blt_ResampleFilter vFilter;		/* Color of rectangle. */
    Blt_ResampleFilter hFilter;		/* Width of outline. */
    Blt_ResampleFilter filter;
} ConvolveSwitches;

static Blt_SwitchSpec convolveSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-filter",  "filter",
	Blt_Offset(ConvolveSwitches, filter),  0, 0, &filterSwitch},
    {BLT_SWITCH_CUSTOM, "-hfilter", "filter",
	Blt_Offset(ConvolveSwitches, hFilter), 0, 0, &filterSwitch},
    {BLT_SWITCH_CUSTOM, "-vfilter", "filter",
	Blt_Offset(ConvolveSwitches, vFilter), 0, 0, &filterSwitch},
    {BLT_SWITCH_END}
};

typedef struct {
    PictRegion from, to;
    int blend;
} CopySwitches;

static Blt_SwitchSpec copySwitches[] = 
{
    {BLT_SWITCH_BOOLEAN,"-blend", "", 
	Blt_Offset(CopySwitches, blend), 0, 0},
    {BLT_SWITCH_CUSTOM, "-from", "bbox", 
	Blt_Offset(CopySwitches,from), 0, 0, &bboxSwitch},
    {BLT_SWITCH_CUSTOM, "-to",   "bbox", 
	Blt_Offset(CopySwitches, to),  0, 0, &bboxSwitch},
    {BLT_SWITCH_END}
};

typedef struct {
    Blt_ResampleFilter filter;
    PictRegion region;
} ResampleSwitches;

static Blt_SwitchSpec resampleSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-filter", "filter",
	Blt_Offset(ResampleSwitches, filter), 0, 0, &filterSwitch},
    {BLT_SWITCH_CUSTOM, "-from",   "bbox",
	Blt_Offset(ResampleSwitches, region), 0, 0, &bboxSwitch},
    {BLT_SWITCH_END}
};

typedef struct {
    PictRegion region;
    int raise;
} SnapSwitches;

static Blt_SwitchSpec snapSwitches[] = 
{
    {BLT_SWITCH_CUSTOM,  "-region", "bbox",
	Blt_Offset(SnapSwitches, region), 0, 0, &bboxSwitch},
    {BLT_SWITCH_BITMASK, "-raise",  "",
	Blt_Offset(SnapSwitches, raise),  0, TRUE},
    {BLT_SWITCH_END}
};

/* 
 * Forward references for TCL command callbacks used below. 
 */
static Tcl_ObjCmdProc PictureInstCmdProc;
static Tcl_CmdDeleteProc PictureInstCmdDeletedProc;

Blt_Picture
Blt_GetNthPicture(Blt_Chain chain, size_t index)
{
    Blt_ChainLink link;

    link = Blt_Chain_GetNthLink(chain, index);
    if (link == NULL) {
	return NULL;
    }
    return Blt_Chain_GetValue(link);
}

static Blt_Picture
PictureFromPictImage(PictImage *imgPtr)
{
    imgPtr->picture = Blt_GetNthPicture(imgPtr->chain, imgPtr->index);
    return imgPtr->picture;
}

static void
ReplacePicture(PictImage *imgPtr, Blt_Picture picture)
{
    Blt_ChainLink link;

    if (imgPtr->chain == NULL) {
	imgPtr->chain = Blt_Chain_Create();
    }
    link = Blt_Chain_GetNthLink(imgPtr->chain, imgPtr->index);
    if (link == NULL) {
	int n;

	n = Blt_Chain_GetLength(imgPtr->chain);
	link = Blt_Chain_Append(imgPtr->chain, picture);
	imgPtr->index = n;
    } else {
	Blt_Picture old;

	old = Blt_Chain_GetValue(link);
	Blt_FreePicture(old);
    }
    Blt_Chain_SetValue(link, picture);
    imgPtr->picture = picture;
}

static void
FreePictures(Blt_Chain chain)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_FirstLink(chain); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	Blt_Picture picture;
	
	picture = Blt_Chain_GetValue(link);
	Blt_FreePicture(picture);
    }
    Blt_Chain_Destroy(chain);
}

Blt_Picture
Blt_GetPictureFromImage(Tk_Image tkImage)
{
    PictInstance *instancePtr;

    if (!Blt_IsPicture(tkImage)) {
	return NULL;
    }
    instancePtr = Blt_ImageGetInstanceData(tkImage);
    return PictureFromPictImage(instancePtr->image);
}

void
Blt_NotifyImageChanged(PictImage *imgPtr)
{
    int w, h;

    if (imgPtr->picture != NULL) {
	w = Blt_PictureWidth(imgPtr->picture);
	h = Blt_PictureHeight(imgPtr->picture);
	Tk_ImageChanged(imgPtr->imgToken, 0, 0, w, h, w, h);
    }
}

Blt_Pixel 
Blt_XColorToPixel(XColor *colorPtr)
{
    Blt_Pixel new;

    /* Convert X Color with 3 channel, 16-bit components to Blt_Pixel
     * (8-bit, with alpha component) */
    new.Red = colorPtr->red / 257;
    new.Green = colorPtr->green / 257;
    new.Blue = colorPtr->blue / 257;
    new.Alpha = ALPHA_OPAQUE;
    return new;
}

int
Blt_GetPixel(Tcl_Interp *interp, const char *string, Blt_Pixel *pixelPtr)
{
    int length;

    length = strlen(string);
    if ((string[0] == '0') && (string[1] == 'x')) {
	unsigned int value;
	char *term;

	value = strtoul(string + 2, &term, 16); 
	if ((term == (string + 1)) || (*term != '\0')) {
	    Tcl_AppendResult(interp, "expected color value but got \"", string,
		"\"", (char *) NULL);
	    return TCL_ERROR;
	}
	pixelPtr->u32 = value;
    } else {
    	XColor color;
	Tk_Window tkwin;

	tkwin = Tk_MainWindow(interp);
	if (!XParseColor(Tk_Display(tkwin), Tk_Colormap(tkwin), string, 
		&color)) {
	    Tcl_AppendResult(interp, "unknown color name \"", string, "\"",
			     (char *) NULL);
	    return TCL_ERROR;
	}
	*pixelPtr = Blt_XColorToPixel(&color);
    }  
    return TCL_OK;
}

int
Blt_GetPixelFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, Blt_Pixel *pixelPtr)
{
    return Blt_GetPixel(interp, Tcl_GetString(objPtr), pixelPtr);
}

const char *
Blt_NameOfPixel(Blt_Pixel *pixelPtr)
{
    static char string[10];

    sprintf_s(string, 10, "%02x%02x%02x%02x", pixelPtr->Alpha,
	    pixelPtr->Red, pixelPtr->Green, pixelPtr->Blue);
    return string;
}

int
Blt_GetBBoxFromObjv(
    Tcl_Interp *interp, 
    int objc,
    Tcl_Obj *const *objv, 
    PictRegion *regionPtr)
{
    double left, top, right, bottom;

    if ((objc != 2) && (objc != 4)) {
	Tcl_AppendResult(interp, "wrong # elements in bounding box ", 
		(char *)NULL);
	return TCL_ERROR;
    }
    regionPtr->x = regionPtr->y = regionPtr->w = regionPtr->h = 0;
    if ((Tcl_GetDoubleFromObj(interp, objv[0], &left) != TCL_OK) ||
	(Tcl_GetDoubleFromObj(interp, objv[1], &top) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (objc == 2) {
	regionPtr->x = ROUND(left), regionPtr->y = ROUND(top);
	return TCL_OK;
    }
    if ((Tcl_GetDoubleFromObj(interp, objv[2], &right) != TCL_OK) ||
	(Tcl_GetDoubleFromObj(interp, objv[3], &bottom) != TCL_OK)) {
	return TCL_ERROR;
    }

    /* Flip the coordinates of the bounding box if necessary so that its the
     * upper-left and lower-right corners */
    if (left > right) {
	double tmp;

	tmp = left, left = right, right = tmp;
    }
    if (top > bottom) {
	double tmp;

	tmp = top, top = bottom, bottom = tmp;
    }
    top = floor(top), left = floor(left);
    bottom = ceil(bottom), right = ceil(right);
    regionPtr->x = (int)left, regionPtr->y = (int)top;
    regionPtr->w = (int)right - regionPtr->x + 1;
    regionPtr->h = (int)bottom - regionPtr->y + 1;
    return TCL_OK;
}

static int
GetBBoxFromObj(
    Tcl_Interp *interp, 
    Tcl_Obj *objPtr, 
    PictRegion *regionPtr)
{
    int objc;
    Tcl_Obj **objv;

    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    return Blt_GetBBoxFromObjv(interp, objc, objv, regionPtr);
}

int
Blt_AdjustRegionToPicture(Blt_Picture picture, PictRegion *regionPtr)
{
    int w, h;

    w = Blt_PictureWidth(picture);
    h = Blt_PictureHeight(picture);

    if ((regionPtr->w == 0) || (regionPtr->w > w)) {
	regionPtr->w = w;
    }
    if ((regionPtr->h == 0) || (regionPtr->h > h)) {
	regionPtr->h = h;
    }

    /* Verify that some part of the bounding box is actually inside the
     * picture. */
    if ((regionPtr->x >= w) || ((regionPtr->x + regionPtr->w) <= 0) ||
	(regionPtr->y >= h) || ((regionPtr->y + regionPtr->h) <= 0)) {
	return FALSE;
    }
    /* If needed, adjust the bounding box so that it resides totally inside the
     * picture */
    if (regionPtr->x < 0) {
	regionPtr->w += regionPtr->x;
	regionPtr->x = 0;
    } 
    if (regionPtr->y < 0) {
	regionPtr->h += regionPtr->y;
	regionPtr->y = 0;
    }
    if ((regionPtr->x + regionPtr->w) > w) {
	regionPtr->w = w - regionPtr->x;
    }
    if ((regionPtr->y + regionPtr->h) > h) {
	regionPtr->h = h - regionPtr->y;
    }
    return TRUE;
}

int
Blt_ResetPicture(Tcl_Interp *interp, const char *imageName, Blt_Picture picture)
{
    Tcl_CmdInfo cmdInfo;

    if (Tcl_GetCommandInfo(interp, imageName, &cmdInfo)) {
	if (cmdInfo.objProc == PictureInstCmdProc) {
	    PictImage *imgPtr;

	    imgPtr = cmdInfo.objClientData;
	    if (imgPtr->picture != picture) {
		ReplacePicture(imgPtr, picture);
	    }
	    Blt_NotifyImageChanged(imgPtr);
	    return TCL_OK;
	}
    }
    Tcl_AppendResult(interp, "can't find picture \"", imageName, "\"", 
	(char *)NULL);
    return TCL_ERROR;
}
    
#ifdef notdef
int
Blt_ResetPictureFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, 
			Blt_Picture picture)
{
    return Blt_ResetPicture(interp, Tcl_GetString(objPtr), picture);
}
#endif

int
Blt_GetPicture(Tcl_Interp *interp, const char *string, Blt_Picture *picturePtr)
{
    Tcl_CmdInfo cmdInfo;

    if (Tcl_GetCommandInfo(interp, string, &cmdInfo)) {
	if (cmdInfo.objProc == PictureInstCmdProc) {
	    PictImage *imgPtr;

	    imgPtr = cmdInfo.objClientData;
	    *picturePtr = imgPtr->picture;
	    return TCL_OK;
	}
    }
    Tcl_AppendResult(interp, "can't find picture \"", string, "\"",
		     (char *)NULL);
    return TCL_ERROR;
}

int
Blt_GetPictureFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, Blt_Picture *pictPtr)
{
    return Blt_GetPicture(interp, Tcl_GetString(objPtr), pictPtr);
}


static int
LoadFormat(Tcl_Interp *interp, const char *name)
{
    Tcl_DString ds;
    const char *result;

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, "blt_picture_", 12);
    Tcl_DStringAppend(&ds, name, -1);
    strtolower(Tcl_DStringValue(&ds));
#define EXACT 1
    result = Tcl_PkgRequire(interp, Tcl_DStringValue(&ds), BLT_VERSION, EXACT);
    Tcl_DStringFree(&ds);
    return (result != NULL);
}

static PictFormat *
QueryExternalFormat(
    Tcl_Interp *interp,			/* Interpreter to load new format
					 * into. */
    Blt_DBuffer dbuffer,		/* Data to be tested. */
    const char *ext)			/* Extension of file name read in.  Will
					 * be NULL if read from data/base64
					 * string. */
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;

    /* 
     * Step 1. Try to match the format to the extension, if there is
     *	       one.  We're trying to minimize the number of formats 
     *	       loaded using a blind query (.i.e. -file fileName).
     */
    if (ext != NULL) {
	hPtr = Blt_FindHashEntry(&fmtTable, ext);
	if (hPtr != NULL) {
	    PictFormat *fmtPtr;

	    fmtPtr = Blt_GetHashValue(hPtr);
#ifdef notdef
	    fprintf(stderr, "trying %s\n", fmtPtr->name);
#endif
	    if ((fmtPtr->flags & BLT_PIC_FMT_LOADED) == 0) {
		LoadFormat(interp, ext);
	    }
	    if (((fmtPtr->flags & BLT_PIC_FMT_LOADED) == 0) || 
		(fmtPtr->isFmtProc == NULL)) {
		fprintf(stderr, "can't load format %s\n", fmtPtr->name);
		return NULL;		/* Could not load the format or the
					 * format doesn't have a discovery
					 * procedure. */
	    }
	    if ((*fmtPtr->isFmtProc)(dbuffer)) {
		return fmtPtr;
	    }
#ifdef notdef
	    fprintf(stderr, "failed to match %s\n", fmtPtr->name);
#endif
	    /* If the image doesn't match, even though the extension matches,
	     * fall through and try all the other formats available. */
	}
    }
    /* 
     * Step 2. Try to match the image against all the previously 
     *	       loaded formats.
     */
    for (hPtr = Blt_FirstHashEntry(&fmtTable, &iter); hPtr != NULL; 
	 hPtr = Blt_NextHashEntry(&iter)) {
	PictFormat *fmtPtr;

	fmtPtr = Blt_GetHashValue(hPtr);
#ifdef notdef
	    fprintf(stderr, "pass2 trying %s\n", fmtPtr->name);
#endif
	if ((fmtPtr->flags & BLT_PIC_FMT_LOADED) == 0) {
	    continue;			/* Format isn't already loaded. */
	}
	if (fmtPtr->isFmtProc == NULL) {
	    continue;			/* No discover procedure.  */
	}
	if ((*fmtPtr->isFmtProc)(dbuffer)) {
	    return fmtPtr;
	}
    }
    /* 
     * Step 3. Try to match the image against any format not previously loaded.
     */
    for (hPtr = Blt_FirstHashEntry(&fmtTable, &iter); hPtr != NULL; 
	 hPtr = Blt_NextHashEntry(&iter)) {
	PictFormat *fmtPtr;

	fmtPtr = Blt_GetHashValue(hPtr);
#ifdef notdef
	    fprintf(stderr, "pass3 trying %s\n", fmtPtr->name);
#endif
	if (fmtPtr->flags & BLT_PIC_FMT_LOADED) {
	    continue;			/* Format is already loaded.  */
	}
	if (!LoadFormat(interp, fmtPtr->name)) {
	    continue;			/* Can't load format. */
	}
	if (((fmtPtr->flags & BLT_PIC_FMT_LOADED) == 0) || 
	    (fmtPtr->isFmtProc == NULL)) {
	    fprintf(stderr, "can't load format %s\n", fmtPtr->name);
	    return NULL;		/* Could not load the format or the
					 * format doesn't have a discovery
					 * procedure. */
	}
	if ((*fmtPtr->isFmtProc)(dbuffer)) {
	    return fmtPtr;
	}
    }
    return NULL;
}

static int
ImageToPicture(Tcl_Interp *interp, PictImage *imgPtr, const char *imageName)
{
    Blt_Picture picture;
    Tk_PhotoHandle photo;

    photo = Tk_FindPhoto(interp, imageName);
    if (photo != NULL) {
	picture = Blt_PhotoToPicture(photo);
    } else if (Blt_GetPicture(interp, imageName, &picture) == TCL_OK) {
	picture = Blt_ClonePicture(picture);
    } else {
	return TCL_ERROR;
    }
    ReplacePicture(imgPtr, picture);
    if (imgPtr->name != NULL) {
	Blt_Free(imgPtr->name);
    }
    imgPtr->name = Blt_AssertStrdup(imageName);
    imgPtr->flags &= ~IMPORTED_MASK;
    imgPtr->flags |= IMPORTED_IMAGE;
    return TCL_OK;
}

static int
WindowToPicture(Tcl_Interp *interp, PictImage *imgPtr, Tcl_Obj *objPtr)
{
    Blt_Picture picture;
    Window window;
    int w, h;

    if (Blt_GetWindowFromObj(interp, objPtr, &window) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_GetWindowRegion(imgPtr->display, window, (int *)NULL, (int *)NULL,
		&w, &h) != TCL_OK) {
	Tcl_AppendResult(interp, "can't get dimensions of window \"", 
		Tcl_GetString(objPtr), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    /* Depth, visual, colormap */
    picture = Blt_WindowToPicture(imgPtr->display, window, 0, 0, w, h, 
	imgPtr->gamma);
    if (picture == NULL) {
	Tcl_AppendResult(interp, "can't obtain snapshot of window \"", 
		Tcl_GetString(objPtr), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    ReplacePicture(imgPtr, picture);
    if (imgPtr->name != NULL) {
	Blt_Free(imgPtr->name);
    }
    imgPtr->name = Blt_AssertStrdup(Tcl_GetString(objPtr));
    imgPtr->flags &= ~IMPORTED_MASK;
    imgPtr->flags |= IMPORTED_WINDOW;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToFile --
 *
 *	Given a file name, determine the image type and convert into a
 *	picture.
 *
 * Results:
 *	The return value is a standard TCL result.  
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToFile(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,		        /* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* String representation of value. */
    char *widgRec,			/* Widget record. */
    int offset,				/* Offset to field in structure */
    int flags)				/* Not used. */
{
    Blt_Chain chain;
    Blt_DBuffer dbuffer;
    Blt_Picture *picturePtr = (Blt_Picture *)(widgRec + offset);
    Blt_Picture picture;
    PictImage *imgPtr = (PictImage *)widgRec;
    const char *fileName;
    PictFormat *fmtPtr;
    char *ext;

    dbuffer = Blt_DBuffer_Create();
    fileName = Tcl_GetString(objPtr);
    if (Blt_DBuffer_LoadFile(interp, fileName, dbuffer) != TCL_OK) {
	goto error;
    }
    ext = NULL;
    if (fileName[0] != '@') {
	ext = strrchr(fileName, '.');
	if (ext != NULL) {
	    ext++;
	    if (*ext == '\0') {
		ext = NULL;
	    } 
	    strtolower(ext);
	}
    }
    fmtPtr = QueryExternalFormat(interp, dbuffer, ext);
    if (fmtPtr == NULL) {
	Tcl_AppendResult(interp, "unknown image file format in \"", fileName, 
		"\"", (char *)NULL);
	goto error;
    }
    if (fmtPtr->readProc == NULL) {
	Tcl_AppendResult(interp, "no reader for format \"", fmtPtr->name, "\".",
		(char *)NULL);
	goto error;
    }
    chain = (*fmtPtr->readProc)(interp, fileName, dbuffer);
    if (chain == NULL) {
	goto error;
    }
    FreePictures(imgPtr->chain);
    imgPtr->chain = chain;
    imgPtr->index = 0;
    imgPtr->picture = Blt_Chain_FirstValue(chain);
    if (imgPtr->name != NULL) {
	Blt_Free(imgPtr->name);
    }
    imgPtr->fmtPtr = fmtPtr;
    imgPtr->name = Blt_AssertStrdup(fileName);
    imgPtr->flags &= ~IMPORTED_MASK;
    imgPtr->flags |= IMPORTED_FILE;
    imgPtr->interval = 0;		/* 100 microseconds */
    *picturePtr = picture;
    Blt_DBuffer_Destroy(dbuffer);
    return TCL_OK;
 error:
    Blt_DBuffer_Destroy(dbuffer);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * FileToObj --
 *
 *	Convert the picture into a TCL list of pixels.
 *
 * Results:
 *	The string representation of the picture is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
FileToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Not used. */
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,			/* Widget record */
    int offset,				/* Not used. */
    int flags)				/* Not used. */
{
    PictImage *imgPtr = (PictImage *)widgRec;

    if (((imgPtr->flags & IMPORTED_FILE) == 0) || (imgPtr->name == NULL)) {
	return Tcl_NewStringObj("", -1);
    }
    return Tcl_NewStringObj(imgPtr->name, -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToFilter --
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
ObjToFilter(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,		        /* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* String representation of value. */
    char *widgRec,			/* Widget record. */
    int offset,				/* Offset to field in structure */
    int flags)				/* Not used. */
{
    Blt_ResampleFilter *filterPtr = (Blt_ResampleFilter *)(widgRec + offset);
    const char *string;

    string = Tcl_GetString(objPtr);
    if (string[0] == '\0') {
	*filterPtr = NULL;
	return TCL_OK;
    }
    return Blt_GetResampleFilterFromObj(interp, objPtr, filterPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * FilterToObj --
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
FilterToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Not used. */
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,			/* Widget record */
    int offset,				/* Offset to field in structure */
    int flags)				/* Not used. */
{
    Blt_ResampleFilter filter = *(Blt_ResampleFilter *)(widgRec + offset);

    if (filter == NULL) {
	return Tcl_NewStringObj("", -1);
    }
    return Tcl_NewStringObj(Blt_NameOfResampleFilter(filter), -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToData --
 *
 *	Given a string of data or binary Tcl_Obj, determine the image
 *	type and convert into a picture.
 *
 * Results:
 *	The return value is a standard TCL result.  
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToData(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representation of value. */
    char *widgRec,		/* Widget record. */
    int offset,			/* Offset to field in structure */
    int flags)			/* Not used. */
{
    Blt_Chain chain;
    Blt_DBuffer dbuffer;
    Blt_Picture *picturePtr = (Blt_Picture *)(widgRec + offset);
    PictFormat *fmtPtr;
    PictImage *imgPtr = (PictImage *)widgRec;
    const unsigned char *bytes;
    int length;
    size_t nBytes;

    dbuffer = Blt_DBuffer_Create();
    bytes = Tcl_GetByteArrayFromObj(objPtr, &length);
    nBytes = (size_t)length;
    if (Blt_IsBase64((const char *)bytes, nBytes)) {
	if (Blt_DBuffer_DecodeBase64(interp, (const char *)bytes, 
		nBytes, dbuffer) != TCL_OK) {
	    goto error;
	}
    } else {
	Blt_DBuffer_AppendData(dbuffer, bytes, nBytes);
    }
#ifdef notdef
    {
	FILE *f;

	f = fopen("junk.unk", "w");
	fwrite(Blt_DBuffer_Bytes(dbuffer), 1, Blt_DBuffer_Length(dbuffer), f);
	fclose(f);
    }
#endif
    fmtPtr = QueryExternalFormat(interp, dbuffer, NULL);
    if (fmtPtr == NULL) {
	Tcl_AppendResult(interp, "unknown image file format in \"",
		Tcl_GetString(objPtr), "\"", (char *)NULL);
	goto error;
    }
    if (fmtPtr->readProc == NULL) {
	Tcl_AppendResult(interp, "no reader for format \"", fmtPtr->name, "\".",
		(char *)NULL);
	goto error;
    }
    chain = (*fmtPtr->readProc)(interp, "-data", dbuffer);
    if (chain == NULL) {
	goto error;
    }
    FreePictures(imgPtr->chain);
    imgPtr->chain = chain;
    imgPtr->index = 0;
    *picturePtr = Blt_Chain_FirstValue(chain);
    imgPtr->flags &= ~IMPORTED_MASK;
    imgPtr->flags |= IMPORTED_DATA;
    Blt_DBuffer_Destroy(dbuffer);
    return TCL_OK;
 error:
    Blt_DBuffer_Destroy(dbuffer);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * DataToObj --
 *
 *	Convert the picture into a TCL list of pixels.
 *
 * Results:
 *	The string representation of the picture is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
DataToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,			/* Widget record */
    int offset,				/* Not used. */
    int flags)				/* Not used. */
{
    PictImage *imgPtr = (PictImage *)(widgRec);
    PictFormat *fmtPtr;

    if ((imgPtr->flags & IMPORTED_DATA) == 0) {
	return Tcl_NewStringObj("", -1);
    }
    fmtPtr = imgPtr->fmtPtr;
    if ((fmtPtr == NULL) || (fmtPtr->writeProc == NULL)) {
	Tcl_AppendResult(interp, "no writer for format \"", fmtPtr->name, 
		"\".", (char *)NULL);
	return NULL;
    }
    return (*fmtPtr->writeProc)(interp, imgPtr->chain);
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToImage --
 *
 *	Convert a named image into a picture.
 *
 * Results:
 *	The return value is a standard TCL result.  
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToImage(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representation of value. */
    char *widgRec,		/* Widget record. */
    int offset,			/* Not used. */
    int flags)			/* Not used. */
{
    PictImage *imgPtr = (PictImage *)(widgRec);

    return ImageToPicture(interp, imgPtr, Tcl_GetString(objPtr));
}

/*
 *---------------------------------------------------------------------------
 *
 * ImageToObj --
 *
 *	Convert the named image into a picture.
 *
 * Results:
 *	The string representation of the picture is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ImageToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Not used. */
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,			/* Widget record */
    int offset,				/* Not used. */
    int flags)				/* Not used. */
{
    PictImage *imgPtr = (PictImage *)widgRec;

    if (((imgPtr->flags & IMPORTED_IMAGE) == 0) || (imgPtr->name == NULL)) {
	return Tcl_NewStringObj("", -1);
    }
    return Tcl_NewStringObj(imgPtr->name, -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToWindow --
 *
 *	Given a file name, determine the image type and convert 
 *	into a picture.
 *
 * Results:
 *	The return value is a standard TCL result.  
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToWindow(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,		        /* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* String representation of value. */
    char *widgRec,			/* Widget record. */
    int offset,				/* Not used. */
    int flags)				/* Not used. */
{
    PictImage *imgPtr = (PictImage *)(widgRec);

    return WindowToPicture(interp, imgPtr, objPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * WindowToObj --
 *
 *	Convert the picture into a TCL list of pixels.
 *
 * Results:
 *	The string representation of the picture is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
WindowToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Not used. */
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,			/* Widget record */
    int offset,				/* Not used. */
    int flags)				/* Not used. */
{
    PictImage *imgPtr = (PictImage *)widgRec;

    if (((imgPtr->flags & IMPORTED_WINDOW) == 0) || (imgPtr->name == NULL)) {
	return Tcl_NewStringObj("", -1);
    }
    return Tcl_NewStringObj(imgPtr->name, -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * CacheKey --
 *
 *	Returns a key representing a specific visual context for a PictImage.
 *	Keys are used to create/find cache entries stored in the hash table of
 *	each PictImage.
 *
 * Results:
 *	A pointer to a static cache key.  
 *
 * Side effects:
 *	The key is overwritten on each call.  Care must be taken by the caller
 *	to save the key before making additional calls.
 *
 *---------------------------------------------------------------------------
 */
static PictCacheKey *
CacheKey(Tk_Window tkwin, unsigned int index)
{
    static PictCacheKey key;

    key.display = Tk_Display(tkwin);
    key.visualPtr = Tk_Visual(tkwin);
    key.colormap = Tk_Colormap(tkwin);
    key.depth = Tk_Depth(tkwin);
    key.index = index;
    return &key;
}

/*
 *---------------------------------------------------------------------------
 *
 * DestroyCache --
 *
 *	This procedure is a callback for Tcl_EventuallyFree to release the
 *	resources and memory used by a PictInstance entry. The entry is
 *	destroyed only if noone else is currently using the entry (using
 *	reference counting).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cache entry is possibly deallocated.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroyCache(DestroyData data)
{
    PictInstance *cachePtr = (PictInstance *)data;
    
    if (cachePtr->refCount <= 0) {
	if (cachePtr->pixmap != None) {
	    Tk_FreePixmap(cachePtr->display, cachePtr->pixmap);
	}
	if (cachePtr->painter != NULL) {
	    Blt_FreePainter(cachePtr->painter);
	}
	if (cachePtr->hashPtr != NULL) {
	    Blt_DeleteHashEntry(cachePtr->tablePtr, cachePtr->hashPtr);
	}
	Blt_Free(cachePtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * GetPictInstance --
 *
 *	This procedure is called for each use of a picture image in a widget.
 *
 * Results:
 *	The return value is an entry for the visual context, which will be
 *	passed back to us on calls to DisplayPictureImage and
 *	FreePictInstance.
 *
 * Side effects:
 *	A new entry is possibly allocated (or shared if one already exists).
 *
 *---------------------------------------------------------------------------
 */
static ClientData
GetPictInstance(
    Tk_Window tkwin,			/* Window in which the picture will be
					 * displayed. */
    ClientData clientData)		/* Pointer to picture image for the
					 * image. */
{
    Blt_HashEntry *hPtr;
    PictCacheKey *keyPtr;
    PictImage *imgPtr = clientData;
    PictInstance *cachePtr;
    int isNew;

    keyPtr = CacheKey(tkwin, imgPtr->index);
    hPtr = Blt_CreateHashEntry(&imgPtr->cacheTable, (char *)keyPtr, &isNew);
    if (isNew) {
	cachePtr = Blt_Malloc(sizeof(PictInstance));	
	if (cachePtr == NULL) {
	    return NULL;		/* Can't allocate memory. */
	}
	cachePtr->painter = Blt_GetPainter(tkwin, imgPtr->gamma);
	cachePtr->image = imgPtr;
	cachePtr->display = Tk_Display(tkwin);
	cachePtr->pixmap = None;
	cachePtr->hashPtr = hPtr;
	cachePtr->tablePtr = &imgPtr->cacheTable;
	cachePtr->refCount = 0;
	Blt_SetHashValue(hPtr, cachePtr);

	if (imgPtr->picture != NULL) {
	    Blt_NotifyImageChanged(imgPtr);
	}
    } 
    cachePtr = Blt_GetHashValue(hPtr);
    cachePtr->refCount++;
    return cachePtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * FreePictInstance --
 *
 *	This procedure is called when a widget ceases to use a particular
 *	instance of a picture image.  We don't actually get rid of the entry
 *	until later because we may be about to re-get this instance again.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Internal data structures get freed.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
FreePictInstance(
    ClientData clientData,		/* Pointer to cache structure */
    Display *display)			/* Not used. */
{
    PictInstance *cachePtr = clientData;

    cachePtr->refCount--;
    if (cachePtr->refCount <= 0) {
	/* 
	 * Right now no one is using the entry. But delay the removal of the
	 * cache entry in case it's reused shortly afterwards.
	 */
 	Tcl_EventuallyFree(cachePtr, DestroyCache);
    }    
}

/*
 *---------------------------------------------------------------------------
 *
 * DeletePictureImage --
 *
 *	This procedure is called by the Tk image code to delete the master
 *	structure (PictureImage) for a picture image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources associated with the picture image are freed.
 *
 *---------------------------------------------------------------------------
 */
static void
DeletePictureImage(
    ClientData clientData)		/* Pointer to PhotoMaster structure for
					 * image.  Must not have any more
					 * instances. */
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    PictImage *imgPtr = clientData;
 
    for (hPtr = Blt_FirstHashEntry(&imgPtr->cacheTable, &iter); 
	hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	PictInstance *cachePtr;

	cachePtr = Blt_GetHashValue(hPtr);
	cachePtr->hashPtr = NULL; /* Flag for FreePictInstance. */
	DestroyCache((DestroyData)cachePtr);
    }
    imgPtr->imgToken = NULL;
    if (imgPtr->cmdToken != NULL) {
	Tcl_DeleteCommandFromToken(imgPtr->interp, imgPtr->cmdToken);
    }
    Blt_DeleteHashTable(&imgPtr->cacheTable);
    Blt_FreeOptions(configSpecs, (char *)imgPtr, imgPtr->display, 0);
    if (imgPtr->chain != NULL) {
	FreePictures(imgPtr->chain);
    }
    if (imgPtr->timerToken != (Tcl_TimerToken)0) {
	Tcl_DeleteTimerHandler(imgPtr->timerToken);
	imgPtr->timerToken = 0;
    }
    Blt_Free(imgPtr);
}

static int 
ConfigurePictureImage(
    Tcl_Interp *interp, 
    PictImage *imgPtr, 
    int objc, 
    Tcl_Obj *const *objv, 
    int flags) 
{
    int w, h;

    if (Blt_ConfigureWidgetFromObj(interp, Tk_MainWindow(interp), configSpecs, 
	objc, objv, (char *)imgPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    imgPtr->picture = PictureFromPictImage(imgPtr);
    if (imgPtr->picture == NULL) {
	w = (imgPtr->reqWidth == 0) ? 16 : imgPtr->reqWidth;
	h = (imgPtr->reqHeight == 0) ? 16 : imgPtr->reqHeight;
	ReplacePicture(imgPtr, Blt_CreatePicture(w, h));
    }

    if (imgPtr->angle != 0.0) {
	Blt_Picture rotate;

	/* Rotate the picture */
	rotate = Blt_RotatePicture(imgPtr->picture, imgPtr->angle);
	ReplacePicture(imgPtr, rotate);
    }

    w = (imgPtr->reqWidth == 0) ? 
	Blt_PictureWidth(imgPtr->picture) : imgPtr->reqWidth;
    h = (imgPtr->reqHeight == 0) ? 
	Blt_PictureHeight(imgPtr->picture) : imgPtr->reqHeight;

    if (imgPtr->aspect) {
	double sx, sy, scale;

	sx = (double)w / (double)Blt_PictureWidth(imgPtr->picture);
	sy = (double)h / (double)Blt_PictureHeight(imgPtr->picture);
	scale = MIN(sx, sy);
	w = (int)(Blt_PictureWidth(imgPtr->picture) * scale + 0.5);
	h = (int)(Blt_PictureHeight(imgPtr->picture) * scale + 0.5);
    }	    
    if ((Blt_PictureWidth(imgPtr->picture) != w) || 
	(Blt_PictureHeight(imgPtr->picture) != h)) {
	Blt_Picture resize;

	/* Scale the picture */
	if (imgPtr->filter == NULL) {
	    resize = Blt_ScalePicture(imgPtr->picture, 0, 0,
		Blt_PictureWidth(imgPtr->picture), 
		Blt_PictureHeight(imgPtr->picture), w, h);
	} else {
	    resize = Blt_CreatePicture(w, h);
	    Blt_ResamplePicture(resize, imgPtr->picture, imgPtr->filter, 
		imgPtr->filter);
	}	
	ReplacePicture(imgPtr, resize);
    }
    if (imgPtr->sharpen > 0) {
	Blt_SharpenPicture(imgPtr->picture, imgPtr->picture);
    }
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}    

/*
 *---------------------------------------------------------------------------
 *
 * CreatePictureImage --
 *
 *	This procedure is called by the Tk image code to create a new photo
 *	image.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	The data structure for a new photo image is allocated and initialized.
 *
 *---------------------------------------------------------------------------
 */
static int
CreatePictureImage(
    Tcl_Interp *interp,		        /* Interpreter to report errors back
					 * to. */
    char *name,				/* Name to use for image command. */
    int objc,				/* # of option arguments. */
    Tcl_Obj *const *objv,		/* Option arguments (doesn't include
					 * image name or type). */
    Tk_ImageType *typePtr,		/* Not used. */
    Tk_ImageMaster imgToken,		/* Token for image, to be used by us in
					 * later callbacks. */
    ClientData *clientDataPtr)		/* Store manager's token for image here;
					 * it will be returned in later
					 * callbacks. */
{
    PictImage *imgPtr;
    Tk_Window tkwin;

    /*
     * Allocate and initialize the photo image master record.
     */

    imgPtr = Blt_AssertCalloc(1, sizeof(PictImage));
    imgPtr->imgToken = imgToken;
    imgPtr->interp = interp;
    imgPtr->gamma = 1.0f;
    imgPtr->cmdToken = Tcl_CreateObjCommand(interp, name, PictureInstCmdProc,
	    imgPtr, PictureInstCmdDeletedProc);
    imgPtr->aspect = TRUE;
    imgPtr->dither = 2;
    tkwin = Tk_MainWindow(interp);
    imgPtr->display = Tk_Display(tkwin);
    imgPtr->colormap = Tk_Colormap(tkwin);
#ifdef notdef
    imgPtr->typePtr = typePtr;
#endif
    Blt_InitHashTable(&imgPtr->cacheTable, sizeof(PictCacheKey));

    /*
     * Process configuration options given in the image create command.
     */
    if (ConfigurePictureImage(interp, imgPtr, objc, objv, 0) != TCL_OK) {
	DeletePictureImage(imgPtr);
	return TCL_ERROR;
    }
    *clientDataPtr = imgPtr;
    Tcl_SetStringObj(Tcl_GetObjResult(interp), name, -1);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * DisplayPictureImage --
 *
 *	This procedure is invoked to draw a photo image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A portion of the image gets rendered in a pixmap or window.
 *
 *---------------------------------------------------------------------------
 */
static void
DisplayPictureImage(
    ClientData clientData,	/* Pointer to token structure for the picture
				 * to be displayed. */
    Display *display,		/* Display on which to draw picture. */
    Drawable drawable,		/* Pixmap or window in which to draw image. */
    int x, int y,		/* Upper-left corner of region within image to
				 * draw. */
    int w, int h,		/* Dimension of region within image to
				 * draw. */
    int dx, int dy)		/* Coordinates within destination drawable
				 * that correspond to imageX and imageY. */
{
    PictInstance *instPtr = clientData;
    PictImage *imgPtr;
    Blt_Picture picture;

    imgPtr = instPtr->image;
    
    picture = PictureFromPictImage(imgPtr);
    if (picture == NULL) {
	return;
    }
    if ((instPtr->pixmap != None) && (Blt_PictureIsDirty(picture))) {
	Tk_FreePixmap(display, instPtr->pixmap);
	instPtr->pixmap = None;
    }
    if ((imgPtr->doCache) && (Blt_PictureIsOpaque(picture))) {
	if (instPtr->pixmap == None) {
	    Pixmap pixmap;

	    /* Save the entire picture in the pixmap. */
	    pixmap = Tk_GetPixmap(display, drawable, 
		Blt_PictureWidth(picture), 
		Blt_PictureHeight(picture),
		Blt_PainterDepth(instPtr->painter));
	    Blt_PaintPicture(instPtr->painter, drawable, picture,
		0, 0, Blt_PictureWidth(picture), 
		Blt_PictureHeight(picture), 
		0, 0, imgPtr->dither);
	    instPtr->pixmap = pixmap;
	}
	/* Draw only the area that need to be repaired. */
	XCopyArea(display, instPtr->pixmap, drawable, 
		Blt_PainterGC(instPtr->painter), x, y, (unsigned int)w, 
		(unsigned int)h, dx, dy);
    } else {
	unsigned int flags;

	if (instPtr->pixmap != None) {
	    /* Kill the cached pixmap. */
	    Tk_FreePixmap(display, instPtr->pixmap);
	    instPtr->pixmap = None;
	}
	flags = 0;
	if ((imgPtr->dither == 1) || ((imgPtr->dither == 2) && 
		(Blt_PainterDepth(instPtr->painter) < 15))) {
	    flags |= BLT_PAINTER_DITHER;
	}
	Blt_PaintPicture(instPtr->painter, drawable, picture, 
		x, y, w, h, dx, dy, flags);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * PictureImageToPostScript --
 *
 *	This procedure is called to output the contents of a photo
 *	image in PostScript by calling the Tk_PostscriptPhoto
 *	function.
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
PictureImageToPostScript(
    ClientData clientData,		/* Master picture image. */
    Tcl_Interp *interp,			/* Interpreter to return generated
					 * PostScript. */
    Tk_Window tkwin,
    Tk_PostscriptInfo psInfo,		/* Not used.  Only useful for Tk
					 * internal Photo and Bitmap image
					 * types.  */
    int x, int y,			/* First pixel to output */
    int w, int h,			/* Width and height of picture area */
    int prepass)
{
    PictImage *imgPtr = clientData;

    if (prepass) {
	return TCL_OK;
    }
    if (Blt_PictureIsOpaque(imgPtr->picture)) {
	Blt_Ps ps;
	PageSetup setup;

	memset(&setup, 0, sizeof(setup));
	ps = Blt_Ps_Create(interp, &setup);
	Blt_Ps_DrawPicture(ps, imgPtr->picture, (double)x, (double)y);
	Blt_Ps_SetInterp(ps, interp);
	Blt_Ps_Free(ps);
    } else {
	Blt_HashEntry *hPtr;
	Blt_Picture bg;
	Blt_Ps ps;
	Drawable drawable;
	PageSetup setup;
	PictInstance *instPtr;
	PictCacheKey *keyPtr;

	instPtr = NULL;
	keyPtr = CacheKey(tkwin, imgPtr->index);
	hPtr = Blt_FindHashEntry(&imgPtr->cacheTable,(char *)keyPtr);
	if (hPtr != NULL) {
	    instPtr = Blt_GetHashValue(hPtr);
	}
	if ((instPtr != NULL) && (instPtr->pixmap != None)) {
	    drawable = instPtr->pixmap;
	} else {
	    drawable = Tk_WindowId(tkwin);
	}
	bg = Blt_DrawableToPicture(tkwin, drawable, x, y, w, h, imgPtr->gamma); 
	if (bg == NULL) {
	    return TCL_ERROR;
	}
	Blt_BlendPictures(bg, imgPtr->picture, 0, 0, w, h, 0, 0);

	memset(&setup, 0, sizeof(setup));
	ps = Blt_Ps_Create(interp, &setup);
	Blt_Ps_DrawPicture(ps, bg, (double)x, (double)y);
	Blt_Ps_SetInterp(ps, interp);
	Blt_Ps_Free(ps);
	Blt_FreePicture(bg);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * BBoxSwitch --
 *
 *	Convert a Tcl_Obj list of 2 or 4 numbers into representing a bounding
 *	box structure.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BBoxSwitch(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Interpreter to send results back
					 * to */
    const char *switchName,		/* Not used. */
    Tcl_Obj *objPtr,			/* String representation */
    char *record,			/* Structure record */
    int offset,				/* Offset to field in structure */
    int flags)				/* Not used. */
{
    PictRegion *regionPtr = (PictRegion *)(record + offset);

    if (GetBBoxFromObj(interp, objPtr, regionPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * FilterSwitch --
 *
 *	Convert a Tcl_Obj representing a 1D image filter.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
FilterSwitch(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    const char *switchName,	/* Not used. */
    Tcl_Obj *objPtr,		/* String representation */
    char *record,		/* Structure record */
    int offset,			/* Offset to field in structure */
    int flags)			/* Not used. */
{
    Blt_ResampleFilter *filterPtr = (Blt_ResampleFilter *)(record + offset);

    return Blt_GetResampleFilterFromObj(interp, objPtr, filterPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * BlendingModeSwitch --
 *
 *	Convert a Tcl_Obj representing a blending mode.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BlendingModeSwitch(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    const char *switchName,	/* Not used. */
    Tcl_Obj *objPtr,		/* String representation */
    char *record,		/* Structure record */
    int offset,			/* Offset to field in structure */
    int flags)			/* Not used. */
{
    Blt_BlendingMode *modePtr = (Blt_BlendingMode *)(record + offset);
    const char *string;
    int length;
    char c;

    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[0];
    if ((c == 'n') && (strncmp(string, "normal", length) == 0)) {
	*modePtr = BLT_BLEND_NORMAL;
    } else if ((c == 'm') && (strncmp(string, "multiply", length) == 0)) {
	*modePtr = BLT_BLEND_MULTIPLY;
    } else if ((c == 's') && (strncmp(string, "screen", length) == 0)) {
	*modePtr = BLT_BLEND_SCREEN;
    } else if ((c == 'd') && (length > 1) && 
	       (strncmp(string, "darken", length) == 0)) {
	*modePtr = BLT_BLEND_DARKEN;
    } else if ((c == 'l') && (strncmp(string, "lighten", length) == 0)) {
	*modePtr = BLT_BLEND_LIGHTEN;
    } else if ((c == 'd') && (length > 1) && 
	       (strncmp(string, "difference", length) == 0)) {
	*modePtr = BLT_BLEND_DIFFERENCE;
    } else if ((c == 'h') && (strncmp(string, "hardlight", length) == 0)) {
	*modePtr = BLT_BLEND_HARDLIGHT;
    } else if ((c == 's') && (strncmp(string, "softlight", length) == 0)) {
	*modePtr = BLT_BLEND_SOFTLIGHT;
    } else if ((c == 'c') && (length > 5) && 
	       (strncmp(string, "colordodge", length) == 0)) {
	*modePtr = BLT_BLEND_COLORDODGE;
    } else if ((c == 'c') && (length > 5) && 
	       (strncmp(string, "colorburn", length) == 0)) {
	*modePtr = BLT_BLEND_COLORBURN;
    } else if ((c == 'o') && (strncmp(string, "overlay", length) == 0)) {
	*modePtr = BLT_BLEND_OVERLAY;
    } else {
	Tcl_AppendResult(interp, "unknown blending mode \"", string, "\": ",
		"should be normal, mulitply, screen, darken, lighten, ",
		"or difference", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ShapeSwitch --
 *
 *	Convert a Tcl_Obj representing a gradient shape.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ShapeSwitch(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    const char *switchName,	/* Not used. */
    Tcl_Obj *objPtr,		/* String representation */
    char *record,		/* Structure record */
    int offset,			/* Offset to field in structure */
    int flags)			/* Not used. */
{
    Blt_GradientShape *shapePtr = (Blt_GradientShape *)(record + offset);
    const char *string, **p;
    int shape;
    static const char *gradientShapes[] = {
	"linear",
	"bilinear",
	"radial",
	"rectangular",
	NULL
    };
    string = Tcl_GetString(objPtr);
    for (shape = 0, p = gradientShapes; *p != NULL; p++, shape++) {
	if (strcmp(string, *p) == 0) {
	    *shapePtr = (Blt_GradientShape)shape;
	    return TCL_OK;
	}
    }
    Tcl_AppendResult(interp, "unknown gradient type \"", string, "\"",
		     (char *) NULL);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * PathSwitch --
 *
 *	Convert a Tcl_Obj representing a gradient path.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PathSwitch(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    const char *switchName,	/* Not used. */
    Tcl_Obj *objPtr,		/* String representation */
    char *record,		/* Structure record */
    int offset,			/* Offset to field in structure */
    int flags)			/* Not used. */
{
    Blt_GradientPath *pathPtr = (Blt_GradientPath *)(record + offset);
    const char *string, **p;
    int path;
    static const char *gradientPaths[] = {
	"x", "y", "xy", "yx", NULL
    };

    string = Tcl_GetString(objPtr);
    for (path = 0, p = gradientPaths; *p != NULL; p++, path++) {
	if (strcmp(string, *p) == 0) {
	    *pathPtr = (Blt_GradientPath)path;
	    return TCL_OK;
	}
    }
    Tcl_AppendResult(interp, "unknown gradient type \"", string, "\"",
		     (char *) NULL);
    return TCL_ERROR;
}

static Blt_Picture
NextImage(PictImage *imgPtr)
{
    Blt_Picture picture;

    imgPtr->index++;
    if (imgPtr->index >= Blt_Chain_GetLength(imgPtr->chain)) {
	imgPtr->index = 0;
    }
    picture = PictureFromPictImage(imgPtr);
    Blt_NotifyImageChanged(imgPtr);
    return picture;
}

static void
TimerProc(ClientData clientData)
{
    PictImage *imgPtr = clientData;
    Blt_Picture picture;
    int delay;

    picture = NextImage(imgPtr);
    delay = Blt_PictureDelay(imgPtr->picture);
    if (imgPtr->interval > 0) {
	delay = imgPtr->interval;
    }
    imgPtr->timerToken = Tcl_CreateTimerHandler(delay, TimerProc, imgPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * AnimateOp --
 *
 *	$im animate prepare
 *	$im animate delay $time
 *
 *	$im animate stop
 *	$im animate start
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
AnimateOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Not used. */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)		/* Not used. */
{
    PictImage *imgPtr = clientData;
    const char *string;
    int interval;
    
    string = Tcl_GetString(objv[2]);
    if (strcmp(string, "stop") == 0) {
	if (imgPtr->timerToken != 0) {
	    Tcl_DeleteTimerHandler(imgPtr->timerToken);
	    imgPtr->timerToken = 0;
	}
    } else if (strcmp(string, "start") == 0) {
	if (imgPtr->timerToken == 0) {
	    int delay;

	    delay = Blt_PictureDelay(imgPtr->picture);
	    imgPtr->timerToken = 
		Tcl_CreateTimerHandler(delay, TimerProc, imgPtr);
	}	
    } else if (Tcl_GetIntFromObj(interp, objv[2], &interval) == TCL_OK) {
	imgPtr->interval = interval;
	if (imgPtr->timerToken != 0) {
	    Tcl_DeleteTimerHandler(imgPtr->timerToken);
	}
	imgPtr->timerToken = Tcl_CreateTimerHandler(imgPtr->interval, 
		TimerProc, imgPtr);
    } else {
	return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ArithOp --
 *
 *	$image arith op $src -from { 0 0 100 100 } -to { 0 0 }
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
ArithOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Interpreter to report errors back
					 * to. */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)
{
    Blt_Picture src, mask;
    Blt_PictureArithOps op;
    PictImage *imgPtr = clientData;
    Blt_Pixel scalar;
    const char *string;
    char c;
    int len;
    ArithSwitches switches;

    src = NULL;
    string = Tcl_GetString(objv[2]);
    if ((string[0] == '0') && (string[1] == 'x')) {
	if (Blt_GetPixel(interp, string, &scalar) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else if (Blt_GetPicture(interp, string, &src) != TCL_OK) {
	return TCL_ERROR;
    }
    string = Tcl_GetStringFromObj(objv[1], &len);
    op = 0;
    c = string[0];
    if ((c == 'a') && (len > 1) && (strncmp(string, "add", len) == 0)) {
	op = PIC_ARITH_ADD;
    } else if ((c == 's') && (strncmp(string, "subtract", len) == 0)) {
	op = PIC_ARITH_SUB;
    } else if ((c == 'a') && (len > 1) && (strncmp(string, "and", len) == 0)) {
	op = PIC_ARITH_AND;
    } else if ((c == 'o') && (strncmp(string, "or", len) == 0)) {
	op = PIC_ARITH_OR;
    } else if ((c == 'n') && (len > 1) && (strncmp(string, "nand", len) == 0)) {
	op = PIC_ARITH_NAND;
    } else if ((c == 'n') && (len > 1) && (strncmp(string, "nor", len) == 0)) {
	op = PIC_ARITH_NOR;
    } else if ((c == 'x') && (strncmp(string, "xor", len) == 0)) {
	op = PIC_ARITH_XOR;
    } else if ((c == 'm') && (len > 1) && (strncmp(string, "max", len) == 0)) {
	op = PIC_ARITH_MAX;
    } else if ((c == 'm') && (len > 1) && (strncmp(string, "min", len) == 0)) {
	op = PIC_ARITH_MIN;
    }	

    memset(&switches, 0, sizeof(switches));
    if (Blt_ParseSwitches(interp, arithSwitches, objc - 3, objv + 3, &switches, 
	BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    mask = NULL;
    if (switches.maskObjPtr != NULL) {
	if (Blt_GetPictureFromObj(interp, switches.maskObjPtr, &mask)!=TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (mask == NULL) {
	if (src == NULL) {
	    Blt_ApplyScalarToPicture(imgPtr->picture, &scalar, op);
	} else {
	    Blt_ApplyPictureToPicture(imgPtr->picture, src, 
				      0, 0, 
				      Blt_PictureWidth(src),
				      Blt_PictureHeight(src),
				      0, 0, 
				      op);
	}	
    } else {
	if (src == NULL) {
	    Blt_ApplyScalarToPictureWithMask(imgPtr->picture, &scalar, mask, 
		switches.invert, op);
	} else {
	    Blt_ApplyPictureToPictureWithMask(imgPtr->picture, src, mask, 
					      0, 0, 
					      Blt_PictureWidth(src),
					      Blt_PictureHeight(src),
					      0, 0, 
					      switches.invert, op);
	}	
    }
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * BlankOp --
 *	
 *	Resets the picture at its current size to blank (by default 
 *	white, fully opaque) pixels.  
 *
 *		$image blank -color #000000 -region { 0 0 20 20 }
 * Results:
 *	Returns a standard TCL return value. If an error occured parsing
 *	the pixel.
 *
 *
 * Side effects:
 *	A Tk_ImageChanged notification is triggered.
 *
 *---------------------------------------------------------------------------
 */
static int
BlankOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    PictImage *imgPtr = clientData;
    Blt_Pixel bg;

    bg.u32 = 0x00000000;
    if ((objc == 3) && (Blt_GetPixelFromObj(interp, objv[2], &bg)!= TCL_OK)) {
	return TCL_ERROR;
    }
    Blt_BlankPicture(imgPtr->picture, &bg);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * BlendOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
BlendOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Interpreter to report errors back
					 * to. */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)
{
    BlendSwitches switches;
    Blt_Picture fg, bg;
    PictImage *imgPtr = clientData;
    Blt_Picture dest;

    if ((Blt_GetPictureFromObj(interp, objv[2], &bg) != TCL_OK) ||
	(Blt_GetPictureFromObj(interp, objv[3], &fg) != TCL_OK)) {
	return TCL_ERROR;
    }
    switches.mode = BLT_BLEND_NORMAL;
    if (Blt_ParseSwitches(interp, blendSwitches, objc - 4, objv + 4, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    dest = PictureFromPictImage(imgPtr);
    Blt_ResizePicture(dest, Blt_PictureWidth(bg), Blt_PictureHeight(bg));
    Blt_CopyPictureBits(dest, bg, 0, 0, 
	Blt_PictureWidth(bg), Blt_PictureHeight(bg), 0, 0);
    Blt_BlendPictures(dest, fg, 0, 0, Blt_PictureWidth(fg), 
	Blt_PictureHeight(fg), 0, 0); 
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blend2Op --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
Blend2Op(
    ClientData clientData,	/* Information about picture cmd. */
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    int objc,			/* Not used. */
    Tcl_Obj *const *objv)
{
    Blt_Picture fg, bg, dest;
    BlendSwitches switches;
    PictImage *imgPtr = clientData;

    if ((Blt_GetPictureFromObj(interp, objv[2], &bg) != TCL_OK) ||
	(Blt_GetPictureFromObj(interp, objv[3], &fg) != TCL_OK)) {
	return TCL_ERROR;
    }
    switches.mode = BLT_BLEND_NORMAL;
    if (Blt_ParseSwitches(interp, blendSwitches, objc - 4, objv + 4, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    dest = PictureFromPictImage(imgPtr);
    Blt_ResizePicture(dest, Blt_PictureWidth(bg), Blt_PictureHeight(bg));
    Blt_CopyPictureBits(dest, bg, 0, 0, Blt_PictureWidth(bg), 
	Blt_PictureHeight(bg), 0, 0);
    Blt_BlendPicturesByMode(dest, fg, switches.mode); 
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * BlurOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
BlurOp(
    ClientData clientData,	/* Information about picture cmd. */
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    int objc,			/* Not used. */
    Tcl_Obj *const *objv)
{
    Blt_Picture src, dest;
    PictImage *imgPtr;
    int r;			/* Radius of the blur. */

    if (Blt_GetPictureFromObj(interp, objv[2], &src) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_GetIntFromObj(interp, objv[3], &r) != TCL_OK) {
	return TCL_ERROR;
    }
    if (r < 1) {
	Tcl_AppendResult(interp, "radius of blur must be > 1 pixel wide",
			 (char *)NULL);
	return TCL_ERROR;
    }
    imgPtr = clientData;
    dest = PictureFromPictImage(imgPtr);
    Blt_BlurPicture(dest, src, r);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * CgetOp --
 *
 *	Returns the value of the configuration option specified.
 *
 * Results:
 *	Returns a standard TCL return value.  If TCL_OK, the value of the
 *	picture configuration option specified is returned in the interpreter
 *	result.  Otherwise an error message is returned.
 * 
 *---------------------------------------------------------------------------
 */
static int
CgetOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    PictImage *imgPtr = clientData;
    
    return Blt_ConfigureValueFromObj(interp, Tk_MainWindow(interp), configSpecs,
	(char *)imgPtr, objv[2], 0);
}


/*
 *---------------------------------------------------------------------------
 *
 * ConfigureOp --
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
ConfigureOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    PictImage *imgPtr;
    Tk_Window tkwin;
    unsigned int flags;
    
    flags = BLT_CONFIG_OBJV_ONLY;
    tkwin = Tk_MainWindow(interp);
    imgPtr = clientData;
    if (objc == 2) {
	return Blt_ConfigureInfoFromObj(interp, tkwin, configSpecs,
	    (char *)imgPtr, (Tcl_Obj *)NULL, flags);
    } else if (objc == 3) {
	return Blt_ConfigureInfoFromObj(interp, tkwin, configSpecs,
	    (char *)imgPtr, objv[2], flags);
    } else {
	if (ConfigurePictureImage(interp, imgPtr, objc - 2, objv + 2, flags) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ConvolveOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
ConvolveOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Interpreter to report errors back
					 * to. */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)
{
    Blt_Picture src;
    ConvolveSwitches switches;
    PictImage *imgPtr = clientData;

    if (Blt_GetPictureFromObj(interp, objv[2], &src) != TCL_OK) {
	return TCL_ERROR;
    }
    switches.vFilter = bltBoxFilter;
    switches.hFilter = bltBoxFilter;
    switches.filter = NULL;
    if (Blt_ParseSwitches(interp, convolveSwitches, objc - 3, objv + 3, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if (switches.filter != NULL) {
	switches.hFilter = switches.vFilter = switches.filter;
    }
#ifdef notdef
    dest = PictureFromPictImage(imgPtr);
    Blt_ConvolvePicture(dest, src, switches.vFilter, switches.hFilter);
#endif
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * CopyOp --
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
CopyOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    Blt_Picture src, dest;
    CopySwitches switches;
    PictImage *imgPtr;

    if (Blt_GetPictureFromObj(interp, objv[2], &src) != TCL_OK) {
	return TCL_ERROR;
    }
    switches.from.x = switches.from.y = 0;
    switches.from.w = Blt_PictureWidth(src);
    switches.from.h = Blt_PictureHeight(src);
    switches.to.x = switches.to.y = 0;
    switches.to.w = Blt_PictureWidth(src);
    switches.to.h = Blt_PictureHeight(src);
    switches.blend = FALSE;

    if (Blt_ParseSwitches(interp, copySwitches, objc - 3, objv + 3, &switches, 
	BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if (!Blt_AdjustRegionToPicture(src, &switches.from)) {
	return TCL_OK;			/* Region is not inside of source. */
    }
    imgPtr = clientData;
    dest = PictureFromPictImage(imgPtr);
    if (!Blt_AdjustRegionToPicture(dest, &switches.to)) {
	return TCL_OK;			/* Region is not inside of
					 * destination. */
    }
    if (switches.blend) {
	Blt_BlendPictures(dest, src, switches.from.x, 
		switches.from.y, switches.from.w, switches.from.h,
		switches.to.x, switches.to.y);
    } else {
	Blt_CopyPictureBits(dest, src, switches.from.x, 
		switches.from.y, switches.from.w, switches.from.h,
		switches.to.x, switches.to.y);
    }
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * CropOp --
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
CropOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    PictRegion region;
    Blt_Picture src, dest;
    PictImage *imgPtr;

    if (Blt_GetBBoxFromObjv(interp, objc - 2, objv + 2, &region) != TCL_OK) {
	return TCL_ERROR;
    }
    imgPtr = clientData;
    src = PictureFromPictImage(imgPtr);
    if (!Blt_AdjustRegionToPicture(src, &region)) {
	Tcl_AppendResult(interp, "impossible coordinates for region", 
			 (char *)NULL);
	return TCL_ERROR;
    }
    dest = Blt_CreatePicture(region.w, region.h);
    Blt_CopyPictureBits(dest, src, region.x, region.y, region.w, region.h, 0,0);
    ReplacePicture(imgPtr, dest);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

BLT_EXTERN Blt_OpSpec bltPictDrawOps[];
BLT_EXTERN int bltPictDrawNOps;

/*
 *---------------------------------------------------------------------------
 *
 * DrawOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
DrawOp(
    ClientData clientData,		/* Contains reference to picture
					 * object. */
    Tcl_Interp *interp,
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)
{
    PictImage *imgPtr = clientData;
    Blt_Picture picture;
    PictCmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, bltPictDrawNOps, bltPictDrawOps, 
	BLT_OP_ARG2, objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    picture = PictureFromPictImage(imgPtr);
    result = (*proc) (picture, interp, objc, objv);
    if (result == TCL_OK) {
	Blt_NotifyImageChanged(imgPtr);
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * DupOp --
 *
 *	Creates a duplicate of the current picture in a new picture image.  The
 *	name of the new image is returned.
 *
 * Results:
 *	Returns a standard TCL return value.  If TCL_OK, The name of
 *	the new image command is returned via the interpreter result.
 *	Otherwise an error message is returned.
 *
 * Side effects:
 *	A new image command is created.
 *
 *---------------------------------------------------------------------------
 */
static int
DupOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    Blt_Picture picture;
    DupSwitches switches;
    PictImage *imgPtr = clientData;
    Tcl_Obj *objPtr;

    memset(&switches, 0, sizeof(switches));
    if (Blt_ParseSwitches(interp, dupSwitches, objc - 2, objv + 2, &switches, 
	BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if (!Blt_AdjustRegionToPicture(imgPtr->picture, &switches.region)) {
	Tcl_AppendResult(interp, "impossible coordinates for region", 
			 (char *)NULL);
	return TCL_ERROR;
    }
    /* 
     * Create a new picture image. Let Tk automatically generate the command
     * name.
     */
    if (Tcl_Eval(interp, "image create picture") != TCL_OK) {
	return TCL_ERROR;
    }
    /* The interpreter result now contains the name of the image. */
    objPtr = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(objPtr);

    /* 
     * Reset the result just in case Blt_ResetPicture returns an error. 
     */
    Tcl_ResetResult(interp);	
    
    picture = Blt_CreatePicture(switches.region.w, switches.region.h);
    if (switches.nocopy) {		/* Set the picture to a blank image. */
	Blt_BlankPicture(picture, 0x00000000);
    } else {				/* Copy region into new picture. */
	Blt_CopyPictureBits(picture, imgPtr->picture, switches.region.x,
	    switches.region.y, switches.region.w, switches.region.h, 0, 0);
    }
    if (Blt_ResetPicture(interp, Tcl_GetString(objPtr), picture) != TCL_OK) {
	Tcl_DecrRefCount(objPtr);
	Blt_FreePicture(picture);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, objPtr);
    Tcl_DecrRefCount(objPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ExportOp --
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
ExportOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    Blt_HashEntry *hPtr;
    PictFormat *fmtPtr;
    PictImage *imgPtr = clientData;
    int result;

    if (objc == 2) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch iter;

	for (hPtr = Blt_FirstHashEntry(&fmtTable, &iter); hPtr != NULL; 
	     hPtr = Blt_NextHashEntry(&iter)) {
	    PictFormat *fmtPtr;

	    fmtPtr = Blt_GetHashValue(hPtr);
	    if ((fmtPtr->flags & BLT_PIC_FMT_LOADED) == 0) {
		continue;
	    }
	    if (fmtPtr->exportProc == NULL) {
		continue;
	    }
	    Tcl_AppendElement(interp, fmtPtr->name);
	}
	return TCL_OK;
    }
    hPtr = Blt_FindHashEntry(&fmtTable, Tcl_GetString(objv[2]));
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "can't export \"", Tcl_GetString(objv[2]),
			 "\": format not registered", (char *)NULL);
	return TCL_ERROR;
    }
    fmtPtr = Blt_GetHashValue(hPtr);
    if (fmtPtr->exportProc == NULL) {
	Tcl_AppendResult(interp, "no export procedure registered for \"", 
			 fmtPtr->name, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    result = (*fmtPtr->exportProc)(interp, imgPtr->chain, objc, objv);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * FadeOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
FadeOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Interpreter to report errors back
					 * to. */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)
{
    Blt_Picture src;
    PictImage *imgPtr = clientData;
    int alpha;

    if (Blt_GetPictureFromObj(interp, objv[2], &src) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_GetIntFromObj(interp, objv[3], &alpha) != TCL_OK) {
	return TCL_ERROR;
    }
    Blt_FadePicture(imgPtr->picture, src, 0, 0, Blt_PictureWidth(src), 
	Blt_PictureHeight(src), 0, 0, alpha);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * FlipOp --
 *
 *	Flips the picture either horizontally or vertically.
 *
 * Results:
 *	Returns a standard TCL return value.  If TCL_OK, the components of the
 *	pixel are returned as a list in the interpreter result.  Otherwise an
 *	error message is returned.
 *
 *---------------------------------------------------------------------------
 */
static int
FlipOp(
    ClientData clientData,	/* Information about picture cmd. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Not used. */
    Tcl_Obj *const *objv)	/* Argument objects. */
{
    PictImage *imgPtr = clientData;
    const char *string;
    int isVertical;

    string = Tcl_GetString(objv[2]);
    if ((string[0] ==  'x') && (string[1] == '\0')) {
	isVertical = FALSE;
    } else if ((string[0] ==  'y') && (string[1] == '\0')) {
	isVertical = TRUE;
    } else {
	Tcl_AppendResult(interp, "bad flip argument \"", string, 
		"\": should be x or y", (char *)NULL);
	return TCL_ERROR;
    }
    Blt_FlipPicture(imgPtr->picture, isVertical);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetOp --
 *
 *	Returns the RGBA components of the pixel at the specified coordinate.
 *
 * Results:
 *	Returns a standard TCL return value.  If TCL_OK, the components of the
 *	pixel are returned as a list in the interpreter result.  Otherwise an
 *	error message is returned.
 *
 *---------------------------------------------------------------------------
 */
static int
GetOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    PictImage *imgPtr = clientData;
    Blt_Pixel *sp, pixel;
    int x, y;

    if ((Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK) ||
	(Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if ((x < 0) || (x >= Blt_PictureWidth(imgPtr->picture))) {
	Tcl_AppendResult(interp, "x-coordinate \"", Tcl_GetString(objv[2]),
		"\" is out of range", (char *)NULL);
	return TCL_ERROR;
    }
    if ((y < 0) || (y >= Blt_PictureHeight(imgPtr->picture))) {
	Tcl_AppendResult(interp, "y-coordinate \"", Tcl_GetString(objv[3]),
		"\" is out of range", (char *)NULL);
	return TCL_ERROR;
    }
    sp = Blt_PicturePixel(imgPtr->picture, x, y);
    pixel = *sp;
    if ((Blt_PictureFlags(imgPtr->picture) & BLT_PIC_ASSOCIATED_COLORS) &&
	((sp->Alpha != 0xFF) && (sp->Alpha != 0x00))) {
	int bias = sp->Alpha >> 1;

	pixel.Red   = (mul255(sp->Red)   + bias) / sp->Alpha;
	pixel.Green = (mul255(sp->Green) + bias) / sp->Alpha;
	pixel.Blue  = (mul255(sp->Blue)  + bias) / sp->Alpha;
    }
    {
	char string[11];

	sprintf_s(string, 11, "0x%02x%02x%02x%02x", pixel.Alpha, pixel.Red, 
		 pixel.Green, pixel.Blue);
	Tcl_SetObjResult(interp, Tcl_NewStringObj(string, 10));
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * GradientOp --
 *
 *	Flips the picture either horizontally or vertically.
 *
 * Results:
 *	Returns a standard TCL return value.  If TCL_OK, the components of the
 *	pixel are returned as a list in the interpreter result.  Otherwise an
 *	error message is returned.
 *
 *---------------------------------------------------------------------------
 */
static int
GradientOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    PictImage *imgPtr = clientData;
    GradientSwitches switches;

    memset(&switches, 0, sizeof(switches));
    colorSwitch.clientData = imgPtr;
    switches.fg.u32 = 0xFFFFFFFF;
    switches.bg.u32 = 0xFF000000;
    switches.gradient.shape = BLT_GRADIENT_SHAPE_LINEAR;
    switches.gradient.path = BLT_GRADIENT_PATH_X; 
    switches.gradient.logScale = TRUE;
    switches.gradient.jitter = FALSE;
   if (Blt_ParseSwitches(interp, gradientSwitches, objc - 2, objv + 2, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    Blt_GradientPicture(imgPtr->picture, &switches.fg, &switches.bg, 
	&switches.gradient);
    if ((switches.bg.Alpha != 0xFF) || (switches.fg.Alpha != 0xFF)) {
	imgPtr->picture->flags |= BLT_PIC_BLEND;
    }
    Blt_AssociateColors(imgPtr->picture);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * GreyscaleOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
GreyscaleOp(
    ClientData clientData,	/* Information about picture cmd. */
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    int objc,			/* Not used. */
    Tcl_Obj *const *objv)
{
    Blt_Picture src, dest;
    PictImage *imgPtr = clientData;

    if (Blt_GetPictureFromObj(interp, objv[2], &src) != TCL_OK) {
	return TCL_ERROR;
    }
    dest = Blt_GreyscalePicture(src);
    ReplacePicture(imgPtr, dest);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HeightOp --
 *	Returns the current height of the picture.
 *
 *---------------------------------------------------------------------------
 */
static int
HeightOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* # of arguments. */
    Tcl_Obj *const *objv)		/* Argument vector. */
{
    PictImage *imgPtr = clientData;
    int h;

    h = 0;
    if (objc == 3) {
	int w;

	if (Tcl_GetIntFromObj(interp, objv[2], &h) != TCL_OK) {
	    return TCL_ERROR;
	}
	w = Blt_PictureWidth(imgPtr->picture);
	Blt_AdjustPicture(imgPtr->picture, w, h);
	Blt_NotifyImageChanged(imgPtr);
    } 
    if (imgPtr->picture != NULL) {
	h = Blt_PictureHeight(imgPtr->picture);
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), h);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ImportOp --
 *
 *	Imports an image source into a picture.  The image source can be a file,
 *	base64 string, or binary Tcl_Obj.  This performs basically the same
 *	function as "configure".  Any extra functionality this command has is
 *	based upon the ability to pass extra flags to the various converters,
 *	something that can't be done really be done with
 *
 *		$image configure -file file.jpg
 *
 * Results:
 *	Returns a standard TCL return value.  If no error, the interpreter
 *	result will contain the number of images successfully read.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static int
ImportOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    Blt_Chain chain;
    Blt_HashEntry *hPtr;
    PictFormat *fmtPtr;
    PictImage *imgPtr = clientData;
    const char *fileName;

    if (objc == 2) {
	PictFormat *fp, *fend;
	
	for (fp = pictFormats, fend = fp + NUMFMTS; fp < fend; fp++) {
	    if ((fp->flags & BLT_PIC_FMT_LOADED) == 0) {
		continue;
	    }
	    if (fp->importProc == NULL) {
		continue;
	    }
	    Tcl_AppendElement(interp, fp->name);
	}
	return TCL_OK;
    }
    hPtr = Blt_FindHashEntry(&fmtTable, Tcl_GetString(objv[2]));
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown picture format \"", 
		Tcl_GetString(objv[2]), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    fmtPtr = Blt_GetHashValue(hPtr);
    if (fmtPtr->importProc == NULL) {
	Tcl_AppendResult(interp, "no import procedure registered for \"", 
		fmtPtr->name, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    chain = (*fmtPtr->importProc)(interp, objc, objv, &fileName);
    if (chain == NULL) {
	return TCL_ERROR;
    }
    FreePictures(imgPtr->chain);
    imgPtr->chain = chain;
    imgPtr->index = 0;
    imgPtr->picture = Blt_Chain_FirstValue(chain);

    /* 
     * Save the format type and file name in the image record.  The file name
     * is used when querying image options -file or -data via configure.  The
     * type is used by the "-data" operation to establish a default format to
     * output.
     */
    imgPtr->fmtPtr = fmtPtr;
    imgPtr->flags &= ~IMPORTED_MASK;
    if (imgPtr->name != NULL) {
	Blt_Free(imgPtr->name);
    }
    if (fileName == NULL) {
	imgPtr->name = NULL;
	imgPtr->flags |= IMPORTED_DATA;
    } else {
	imgPtr->name = Blt_AssertStrdup(fileName);
	imgPtr->flags |= IMPORTED_FILE;
    }
    Blt_NotifyImageChanged(imgPtr);

    /* Return the number of separates images read. */
    Tcl_SetIntObj(Tcl_GetObjResult(interp), Blt_Chain_GetLength(imgPtr->chain));
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * InfoOp --
 *
 *	Reports the basic information about a picture.  
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
InfoOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)		/* Not used. */
{
    PictImage *imgPtr = clientData;
    int nColors;
    const char *string;
    Tcl_Obj *listObjPtr, *objPtr;

    nColors = Blt_QueryColors(imgPtr->picture, (Blt_HashTable *)NULL);
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);

    objPtr = Tcl_NewStringObj("colors", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    objPtr = Tcl_NewIntObj(nColors);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);

    objPtr = Tcl_NewStringObj("type", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    string = Blt_PictureIsColor(imgPtr->picture) ? "color" : "greyscale";
    objPtr = Tcl_NewStringObj(string, -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);

    objPtr = Tcl_NewStringObj("opacity", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    if (Blt_PictureIsBlended(imgPtr->picture)) {
	string = "blended";
    } else if (Blt_PictureIsMasked(imgPtr->picture)) {
	string = "masked";
    } else {
	string = "full";
    }
    objPtr = Tcl_NewStringObj(string, -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);

    objPtr = Tcl_NewStringObj("width", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    objPtr = Tcl_NewIntObj(Blt_PictureWidth(imgPtr->picture));
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    
    objPtr = Tcl_NewStringObj("height", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    objPtr = Tcl_NewIntObj(Blt_PictureHeight(imgPtr->picture));
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);

    objPtr = Tcl_NewStringObj("count", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    objPtr = Tcl_NewIntObj(Blt_Chain_GetLength(imgPtr->chain));
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);

    objPtr = Tcl_NewStringObj("index", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    objPtr = Tcl_NewIntObj(imgPtr->index);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);

    objPtr = Tcl_NewStringObj("read-format", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    objPtr = Tcl_NewStringObj(imgPtr->fmtPtr->name, -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);

    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * MultiplyOp --
 *
 *	$image multiply scalar
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
MultiplyOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Interpreter to report errors back
					 * to. */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)
{
    PictImage *imgPtr = clientData;
    double scalar;

    if (Tcl_GetDoubleFromObj(interp, objv[2], &scalar) != TCL_OK) {
	return TCL_ERROR;
    }
    Blt_MultiplyPixels(imgPtr->picture, (float)scalar);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * NextOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
NextOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Not used. */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)		/* Not used. */
{
    PictImage *imgPtr = clientData;
    
    NextImage(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * PutOp --
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
PutOp(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * QuantizeOp --
 *
 *	$dest quantize $src 256
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
QuantizeOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Interpreter to report errors back
					 * to. */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)
{
    Blt_Picture src, dest;
    PictImage *imgPtr = clientData;
    int nColors;

    if (Blt_GetPictureFromObj(interp, objv[2], &src) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_GetIntFromObj(interp, objv[3], &nColors) != TCL_OK) {
	return TCL_ERROR;
    }
    dest = Blt_QuantizePicture(src, nColors);
    if (dest == NULL) {
	return TCL_ERROR;
    }
    ReplacePicture(imgPtr, dest);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ResampleOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
ResampleOp(
    ClientData clientData,	/* Information about picture cmd. */
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    int objc,			/* Not used. */
    Tcl_Obj *const *objv)
{
    Blt_Picture src, tmp;
    Blt_ResampleFilter hFilter, vFilter;
    PictImage *imgPtr = clientData;
    ResampleSwitches switches;
    int destWidth, destHeight;

    if (Blt_GetPictureFromObj(interp, objv[2], &src) != TCL_OK) {
	return TCL_ERROR;
    }
    switches.region.x = switches.region.y = 0;
    switches.region.w = Blt_PictureWidth(src);
    switches.region.h = Blt_PictureHeight(src);

    switches.filter = NULL;
    if (Blt_ParseSwitches(interp, resampleSwitches, objc - 3, objv + 3, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if (!Blt_AdjustRegionToPicture(src, &switches.region)) {
	Tcl_AppendResult(interp, "impossible coordinates for region", 
			 (char *)NULL);
	return TCL_ERROR;
    }
    destWidth = Blt_PictureWidth(imgPtr->picture);
    destHeight = Blt_PictureHeight(imgPtr->picture);
    if (switches.filter == NULL) {
	if (switches.region.w < destWidth) {
	    hFilter = bltMitchellFilter;
	} else {
	    hFilter = bltBoxFilter;
	}
	if (switches.region.h < destHeight) {
	    vFilter = bltMitchellFilter;
	} else {
	    vFilter = bltBoxFilter;
	}
    } else {
	hFilter = vFilter = switches.filter;
    }
    tmp = Blt_CreatePicture(switches.region.w, switches.region.h);
    Blt_CopyPictureBits(tmp, src, switches.region.x, switches.region.y, 
	switches.region.w, switches.region.h, 0, 0);
    Blt_ResamplePicture(imgPtr->picture, tmp, vFilter, hFilter);
    Blt_FreePicture(tmp);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * RotateOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
RotateOp(
    ClientData clientData,	/* Information about picture cmd. */
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    int objc,			/* Not used. */
    Tcl_Obj *const *objv)
{
    Blt_Picture src, dest;
    PictImage *imgPtr = clientData;
    double angle;

    if (Blt_GetPictureFromObj(interp, objv[2], &src) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_GetDoubleFromObj(interp, objv[3], &angle) != TCL_OK) {
	const char *string;

	string = Tcl_GetString(objv[3]);
	if (Tcl_ExprDouble(interp, string, &angle) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    dest = Blt_RotatePicture(src, angle);
    ReplacePicture(imgPtr, dest);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SelectOp --
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
SelectOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    Blt_Picture src;
    PictImage *imgPtr = clientData;
    Blt_Pixel lower, upper;
    unsigned char tmp;

    if (Blt_GetPictureFromObj(interp, objv[2], &src) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_GetPixelFromObj(interp, objv[3], &lower) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 5) {
	if (Blt_GetPixelFromObj(interp, objv[4], &upper) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	upper.u32 = lower.u32;
    }
    if (lower.Red > upper.Red) {
	tmp = lower.Red, lower.Red = upper.Red, upper.Red = tmp;
    }
    if (lower.Green > upper.Green) {
	tmp = lower.Green, lower.Green = upper.Green, upper.Green = tmp;
    }
    if (lower.Blue > upper.Blue) {
	tmp = lower.Blue, lower.Blue = upper.Blue, upper.Blue = tmp;
    }
    if (lower.Alpha > upper.Alpha) {
	tmp = lower.Alpha, lower.Alpha = upper.Alpha, upper.Alpha = tmp;
    }
    Blt_SelectPixels(imgPtr->picture, src, &lower, &upper);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SharpenOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
SharpenOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Not used. */
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)		/* Not used. */
{
    PictImage *imgPtr = clientData;
    
    Blt_SharpenPicture(imgPtr->picture, imgPtr->picture);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SnapOp --
 *
 * Results:
 *	Returns a standard TCL return value.
 *
 * Side effects:
 *	None.
 *
 *   $pict snap window -region {x y w h} -raise 
 *
 *---------------------------------------------------------------------------
 */
static int
SnapOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    PictImage *imgPtr = clientData;
    Blt_Picture picture;
    Window window;
    int w, h;
    SnapSwitches switches;

    if (Blt_GetWindowFromObj(interp, objv[2], &window) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_GetWindowRegion(imgPtr->display, window, (int *)NULL, (int *)NULL,
		&w, &h) != TCL_OK) {
	Tcl_AppendResult(interp, "can't get dimensions of window \"", 
		Tcl_GetString(objv[2]), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    memset(&switches, 0, sizeof(switches));
    switches.region.x = switches.region.y = 0;
    switches.region.w = w;
    switches.region.h = h;
    if (Blt_ParseSwitches(interp, snapSwitches, objc - 3, objv + 3, &switches, 
	BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if ((switches.region.w + switches.region.x) > w) {
	switches.region.w = (w - switches.region.x);
    }
    if ((switches.region.h + switches.region.y) > h) {
	switches.region.h = (h - switches.region.y);
    }
    if (switches.raise) {
	XRaiseWindow(imgPtr->display, window);
    }
    /* Depth, visual, colormap */
    picture = Blt_WindowToPicture(imgPtr->display, window, switches.region.x, 
	switches.region.y, switches.region.w, switches.region.h, imgPtr->gamma);
    if (picture == NULL) {
	Tcl_AppendResult(interp, "can't obtain snapshot of window \"", 
		Tcl_GetString(objv[2]), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    ReplacePicture(imgPtr, picture);
    if (imgPtr->name != NULL) {
	Blt_Free(imgPtr->name);
    }
    Blt_NotifyImageChanged(imgPtr);
    imgPtr->flags &= ~IMPORTED_MASK;
    Blt_FreeSwitches(snapSwitches, &switches, 0);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * TileOp --
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
TileOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    PictImage *imgPtr = clientData;
    Blt_Picture src;
    TileSwitches switches;

    if (Blt_GetPictureFromObj(interp, objv[2], &src) != TCL_OK) {
	return TCL_ERROR;
    }
    switches.xOrigin = switches.yOrigin = 0;
    switches.region.x = switches.region.y = 0;
    switches.region.w = Blt_PictureWidth(imgPtr->picture);
    switches.region.h = Blt_PictureHeight(imgPtr->picture);

    if (Blt_ParseSwitches(interp, tileSwitches, objc - 3, objv + 3, &switches, 
	BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if (!Blt_AdjustRegionToPicture(imgPtr->picture, &switches.region)) {
	Tcl_AppendResult(interp, "impossible coordinates for region", 
			 (char *)NULL);
	return TCL_ERROR;
    }
    Blt_TilePicture(imgPtr->picture, src, switches.xOrigin, switches.yOrigin, 
            switches.region.x, switches.region.y, switches.region.w,
	    switches.region.h);
    Blt_NotifyImageChanged(imgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * WidthOp --
 *	Returns the current width of the picture.
 *
 *---------------------------------------------------------------------------
 */
static int
WidthOp(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* # of arguments. */
    Tcl_Obj *const *objv)		/* Argument vector. */
{
    PictImage *imgPtr = clientData;
    int w;

    if (objc == 3) {
	int h;

	if (Tcl_GetIntFromObj(interp, objv[2], &w) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (w < 0) {
	    Tcl_AppendResult(interp, "bad width \"", Tcl_GetString(objv[2]), 
			     "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	h = Blt_PictureHeight(imgPtr->picture);
	Blt_AdjustPicture(imgPtr->picture, w, h);
	Blt_NotifyImageChanged(imgPtr);
    } 
    w = Blt_PictureWidth(imgPtr->picture);
    Tcl_SetIntObj(Tcl_GetObjResult(interp), w);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Picture instance sub-command specification:
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
static Blt_OpSpec pictInstOps[] =
{
    {"add",       2, ArithOp,     3, 0, "image|color",},
    {"and",       3, ArithOp,     3, 0, "image|color",},
    {"animate",   3, AnimateOp,   3, 3, "cmd",},
    {"blank",     3, BlankOp,     2, 3, "?color?",},
    {"blend",     3, BlendOp,     4, 0, "bg fg ?switches?",},
    {"blend2",    3, Blend2Op,    4, 0, "bg fg ?switches?",},
    {"blur",      3, BlurOp,      4, 4, "src width",},
    {"bold",      2, Blend2Op,    4, 0, "bg fg ?switches?",},
    {"cget",      2, CgetOp,      3, 3, "option",},
    {"configure", 4, ConfigureOp, 2, 0, "?option value?...",},
    {"convolve",  4, ConvolveOp,  3, 0, "src ?switches?",},
    {"copy",      3, CopyOp,      3, 0, "srcPict ?switches?",},
    {"crop",      3, CropOp,      2, 0, "?switches?",},
    {"draw",      2, DrawOp,	  2, 0, "?args?",},
    {"dup",       2, DupOp,       2, 0, "?switches?",},
    {"export",    1, ExportOp,    2, 0, "format ?switches?...",},
    {"fade",      2, FadeOp,      4, 4, "src factor",},
    {"flip",      2, FlipOp,      3, 0, "x|y",},
    {"get",       2, GetOp,       4, 4, "x y",},
    {"gradient",  3, GradientOp,  2, 0, "?switches?",},
    {"greyscale", 3, GreyscaleOp, 3, 3, "src",},
    {"height",    1, HeightOp,    2, 3, "?newHeight?",},
    {"import",    2, ImportOp,    2, 0, "format ?switches?...",},
    {"info",      2, InfoOp,      2, 2, "info",},
    {"max",	  2, ArithOp,     3, 0, "image|color",},
    {"min",	  2, ArithOp,     3, 0, "image|color",},
    {"multiply",  2, MultiplyOp,  3, 3, "float",},
    {"nand",      2, ArithOp,     3, 0, "image|color",},
    {"next",      2, NextOp,      2, 2, "",},
    {"nor",       2, ArithOp,     3, 0, "image|color",},
    {"or",        1, ArithOp,     3, 0, "image|color ?switches?",},
    {"put",       1, PutOp,       2, 0, "color ?window?...",},
    {"quantize",  1, QuantizeOp,  4, 4, "src numColors",},
    {"resample",  2, ResampleOp,  3, 0, "src ?switches?",},
    {"resize",    2, ResampleOp,  3, 0, "src ?switches?",},
    {"rotate",    2, RotateOp,    4, 4, "src angle",},
    {"select",    2, SelectOp,    4, 5, "src color ?color?",},
    {"sharpen",   2, SharpenOp,   2, 0, "",},
    {"snap",      2, SnapOp,      3, 0, "window ?switches?",},
    {"subtract",  2, ArithOp,     3, 0, "image|color",},
    {"tile",      2, TileOp,      3, 0, "image ?switches?",},
    {"width",     1, WidthOp,     2, 3, "?newWidth?",},
    {"xor",       1, ArithOp,     3, 0, "image|color ?switches?",},
};

static int nPictInstOps = sizeof(pictInstOps) / sizeof(Blt_OpSpec);

static int
PictureInstCmdProc(
    ClientData clientData,		/* Information about picture cmd. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument objects. */
{
    Tcl_ObjCmdProc *proc;

    proc = Blt_GetOpFromObj(interp, nPictInstOps, pictInstOps, BLT_OP_ARG1, 
	objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc) (clientData, interp, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * PictureInstCmdDeleteProc --
 *
 *	This procedure is invoked when a picture command is deleted.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
PictureInstCmdDeletedProc(ClientData clientData) /* Pointer to record. */
{
    PictImage *imgPtr = clientData;

    imgPtr->cmdToken = NULL;
    if (imgPtr->imgToken != NULL) {
	Tk_DeleteImage(imgPtr->interp, Tk_NameOfImage(imgPtr->imgToken));
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * PictureLoadOp --
 *
 *	Loads the dynamic library representing the converters for the named
 *	format.  Designed to be called by "package require", not directly by the
 *	user.
 *	
 * Results:
 *	A standard TCL result.  Return TCL_OK is the converter was successfully
 *	loaded, TCL_ERROR otherwise.
 *
 * blt::datatable load fmt libpath lib
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PictureLoadOp(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    Tcl_DString lib;
    char *fmt;
    char *safeProcName, *initProcName;
    int result;
    int length;

    fmt = Tcl_GetStringFromObj(objv[2], &length);
    hPtr = Blt_FindHashEntry(&fmtTable, fmt);
    if (hPtr != NULL) {
	PictFormat *fmtPtr;

	fmtPtr = Blt_GetHashValue(hPtr);
	if (fmtPtr->flags & BLT_PIC_FMT_LOADED) {
	    return TCL_OK;		/* Converter is already loaded. */
	}
    }
    Tcl_DStringInit(&lib);
    {
	Tcl_DString ds;
	const char *pathName;

	Tcl_DStringInit(&ds);
	pathName = Tcl_TranslateFileName(interp, Tcl_GetString(objv[3]), &ds);
	if (pathName == NULL) {
	    Tcl_DStringFree(&ds);
	    return TCL_ERROR;
	}
	Tcl_DStringAppend(&lib, pathName, -1);
	Tcl_DStringFree(&ds);
    }
    Tcl_DStringAppend(&lib, "/", -1);
    Tcl_UtfToTitle(fmt);
    Tcl_DStringAppend(&lib, "Picture", 7);
    Tcl_DStringAppend(&lib, fmt, -1);
    Tcl_DStringAppend(&lib, Blt_Itoa(BLT_MAJOR_VERSION), 1);
    Tcl_DStringAppend(&lib, Blt_Itoa(BLT_MINOR_VERSION), 1);
    Tcl_DStringAppend(&lib, BLT_LIB_SUFFIX, -1);
    Tcl_DStringAppend(&lib, BLT_SO_EXT, -1);

    initProcName = Blt_AssertMalloc(11 + length + 4 + 1);
    sprintf_s(initProcName, 11 + length + 4 + 1, "Blt_Picture%sInit", fmt);
    safeProcName = Blt_AssertMalloc(11 + length + 8 + 1);
    sprintf_s(safeProcName, 11 + length + 8 + 1, "Blt_Picture%sSafeInit", fmt);

    result = Blt_LoadLibrary(interp, Tcl_DStringValue(&lib), initProcName, 
	safeProcName); 
    Tcl_DStringFree(&lib);
    if (safeProcName != NULL) {
	Blt_Free(safeProcName);
    }
    if (initProcName != NULL) {
	Blt_Free(initProcName);
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * Picture instance sub-command specification:
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
static Blt_OpSpec pictureOps[] =
{
    {"load",      1, PictureLoadOp, 4, 0, "fmt lib",},
#ifdef notdef
    {"blur",      1, BlurOp,        4, 0, "src dest ?switches?",},
    {"brighten"   1, BrightenOp,    4, 0, "src dest ?switches?",},
    {"darken"     1, BrightenOp,    4, 0, "src dest ?switches?",},
    {"medianf"    1, MedianOp,      4, 0, "src dest ?switches?",},
    {"translate", 1, TranslateOp,   4, 0, "src dest ?switches?",},
#endif
};
static int nPictureOps = sizeof(pictureOps) / sizeof(Blt_OpSpec);

/*
 *---------------------------------------------------------------------------
 *
 * PictureImageCmdProc --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int 
PictureImageCmdProc(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,
    int objc,				/* Not used. */
    Tcl_Obj *const *objv)
{
    Tcl_ObjCmdProc *proc;

    proc = Blt_GetOpFromObj(interp, nPictureOps, pictureOps, BLT_OP_ARG1, 
	objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc)(clientData, interp, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_PictureCmdInitProc --
 *
 *	This procedure is invoked to initialize the "tree" command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates the new command and adds a new entry into a global Tcl
 *	associative array.
 *
 *---------------------------------------------------------------------------
 */
int
Blt_PictureCmdInitProc(Tcl_Interp *interp)
{
    static Blt_InitCmdSpec cmdSpec = { 
	"picture", PictureImageCmdProc, 
    };
    /* cmdSpec.clientData = GetPictureCmdInterpData(interp); */
    if (Blt_InitCmd(interp, "::blt", &cmdSpec) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

static Tk_ImageType pictureImageType = {
    (char *)"picture",		
    CreatePictureImage,
    GetPictInstance,		
    DisplayPictureImage,	
    FreePictInstance,	
    DeletePictureImage,	
    PictureImageToPostScript,	
    (Tk_ImageType *)NULL		/* nextPtr */
};

int
Blt_IsPicture(Tk_Image tkImage)
{
    Tk_ImageType *typePtr;

    typePtr = Blt_ImageGetType(tkImage);
    return (typePtr == &pictureImageType);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_RegisterPictureImageType --
 *
 *	Registers the "picture" image type with Tk.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_RegisterPictureImageType(Tcl_Interp *interp) 
{
    PictFormat *fp, *fend;

    Tk_CreateImageType(&pictureImageType);

    Blt_CpuFeatures(interp, NULL);

    Blt_InitHashTable(&fmtTable, BLT_STRING_KEYS);
    for (fp = pictFormats, fend = fp + NUMFMTS; fp < fend; fp++) {
	Blt_HashEntry *hPtr;
	int isNew;

	hPtr = Blt_CreateHashEntry(&fmtTable, fp->name, &isNew);
	Blt_SetHashValue(hPtr, fp);
    }
}

int
Blt_PictureRegisterFormat(
    Tcl_Interp *interp, 			  
    const char *fmt,
    Blt_PictureIsFmtProc  *isProc,
    Blt_PictureReadDataProc *readProc,
    Blt_PictureWriteDataProc *writeProc,
    Blt_PictureImportProc *importProc,
    Blt_PictureExportProc *exportProc)
{
    PictFormat *fmtPtr;
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&fmtTable, fmt);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown format \"", fmt, "\"", 
			 (char *)NULL);
	return TCL_ERROR;
    }
    fmtPtr = Blt_GetHashValue(hPtr);
    fmtPtr->flags = BLT_PIC_FMT_LOADED;
    fmtPtr->isFmtProc = isProc;
    fmtPtr->readProc = readProc;
    fmtPtr->writeProc = writeProc;
    fmtPtr->importProc = importProc;
    fmtPtr->exportProc = exportProc;
    return TCL_OK;
}

