
/*
 * bltCanvEps.c --
 *
 * This file implements an Encapsulated PostScript item for canvas
 * widgets.
 *
 *	Copyright 1991-2004 George A Howlett.
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

/*
 * To do:
 *
 *	1. Add -rotate option.  Allow arbitrary rotation of image and EPS.
 *	2. Draw pictures instead of photos. This will eliminate the need
 *	   to create hidden photo images.
 *	3. Create a spiffy demo that lets you edit your page description.
 */
#define USE_OLD_CANVAS	1

#include "bltInt.h"
#include "bltPs.h"
#include "bltImage.h"
#include "bltPicture.h"
#include "bltPainter.h"

#ifdef HAVE_TIFF_H
#include "tiff.h"
#endif
#include <fcntl.h>

#if defined(_MSC_VER) || defined(__BORLANDC__) 
#include <io.h>
#define open _open
#define close _close
#define write _write
#define unlink _unlink
#define lseek _lseek
#define fdopen _fdopen
#define fcntl _fcntl
#ifdef _MSC_VER
#define O_RDWR	_O_RDWR 
#define O_CREAT	_O_CREAT
#define O_TRUNC	_O_TRUNC
#define O_EXCL	_O_EXCL
#endif /* _MSC_VER */
#endif /* _MSC_VER || __BORLANDC__ */

#define DEBUG_READER 0
#ifndef WIN32
#define PurifyPrintf printf
#endif
#define PS_PREVIEW_EPSI	0
#define PS_PREVIEW_WMF	1
#define PS_PREVIEW_TIFF	2

#define MAX_EPS_LINE_LENGTH 255		/* Maximum line length for a EPS
					 * file */
/*
 * ParseInfo --
 *
 *	This structure is used to pass PostScript file information around to
 *	various routines while parsing the EPS file.
 */
typedef struct {
    int maxBytes;			/* Maximum length of PostScript
					 * code.  */
    int lineNumber;			/* Current line number of EPS file */
    char line[MAX_EPS_LINE_LENGTH + 1];	/* Buffer to contain a single line
					 * from the PostScript file. */
    unsigned char hexTable[256];	/* Table for converting ASCII hex
					 * digits to values */

    char *nextPtr;			/* Pointer to the next character to
					 * process on the current line.  If
					 * NULL (or if nextPtr points a NULL
					 * byte), this indicates the the next
					 * line needs to be read. */
    FILE *f;				/*  */
} ParseInfo;

#define DEF_EPS_ANCHOR		"nw"
#define DEF_EPS_OUTLINE_COLOR	RGB_BLACK
#define DEF_EPS_BORDERWIDTH	STD_BORDERWIDTH
#define DEF_EPS_FILE_NAME	(char *)NULL
#define DEF_EPS_FONT		STD_FONT
#define DEF_EPS_FILL_COLOR     	STD_NORMAL_FOREGROUND
#define DEF_EPS_HEIGHT		"0"
#define DEF_EPS_IMAGE_NAME	(char *)NULL
#define DEF_EPS_JUSTIFY		"center"
#define DEF_EPS_QUICK_RESIZE	"no"
#define DEF_EPS_RELIEF		"sunken"
#define DEF_EPS_SHOW_IMAGE	"yes"
#define DEF_EPS_STIPPLE		(char *)NULL
#define DEF_EPS_TAGS		(char *)NULL
#define DEF_EPS_TITLE		(char *)NULL
#define DEF_EPS_TITLE_ANCHOR	"center"
#define DEF_EPS_TITLE_COLOR	RGB_BLACK
#define DEF_EPS_WIDTH		"0"

/*
 * Information used for parsing configuration specs:
 */

static Tk_CustomOption tagsOption;

BLT_EXTERN Tk_CustomOption bltDistanceOption;

/*
 * The structure below defines the record for each EPS item.
 */
typedef struct {
    Tk_Item item;			/* Generic stuff that's the same for
					 * all types.  MUST BE FIRST IN
					 * STRUCTURE. */
    Tk_Canvas canvas;			/* Canvas containing the EPS item. */
    int lastWidth, lastHeight;		/* Last known dimensions of the EPS
					 * item.  This is used to know if the
					 * picture preview needs to be
					 * resized. */
    Tcl_Interp *interp;
    FILE *psFile;			/* File pointer to Encapsulated
					 * PostScript file. We'll hold this as
					 * long as the EPS item is using this
					 * file. */
    unsigned int psStart;		/* File offset of PostScript code. */
    unsigned int psLength;		/* Length of PostScript code. If zero,
					 * indicates to read to EOF. */
    unsigned int wmfStart;		/* File offset of Windows Metafile
					 * preview.  */
    unsigned int wmfLength;		/* Length of WMF portion in bytes. If
					 * zero, indicates there is no WMF
					 * preview. */
    unsigned int tiffStart;		/* File offset of TIFF preview. */
    unsigned int tiffLength;		/* Length of TIFF portion in bytes. If
					 * zero, indicates there is no TIFF *
					 * preview. */
    const char *previewImageName;
    int previewFormat;

    Tk_Image preview;			/* A Tk photo image provided as a
					 * preview of the EPS contents. This
					 * image supersedes any EPS preview
					 * embedded PostScript preview
					 * (EPSI). */
    Blt_Painter painter;
    Blt_Picture original;		/* The original photo or PostScript
					 * preview image converted to a
					 * picture. */
    int origFromPicture;
    Blt_Picture picture;		/* Holds resized preview image.
					 * Created and deleted internally. */
    int firstLine, lastLine;		/* First and last line numbers of the
					 * PostScript preview.  They are used
					 * to skip over the preview when
					 * encapsulating PostScript for the
					 * canvas item. */
    GC fillGC;				/* Graphics context to fill background
					 * of image outline if no preview
					 * image was present. */
    int llx, lly, urx, ury;		/* Lower left and upper right
					 * coordinates of PostScript bounding
					 * box, retrieved from file's
					 * "BoundingBox:" field. */
    const char *title;			/* Title, retrieved from the file's
					 * "Title:" field, to be displayed
					 * over the top of the EPS preview
					 * (malloc-ed).  */
    Tcl_DString dString;		/* Contains the encapsulated
					 * PostScript. */

    /* User configurable fields */

    double x, y;			/* Requested anchor in canvas
					 * coordinates of the item */
    Tk_Anchor anchor;

    Region2d bb;

    const char *fileName;		/* Name of the encapsulated PostScript
					 * file.  If NULL, indicates that no
					 * EPS file has be successfully
					 * loaded yet. */
    const char *reqTitle;		/* Title to be displayed in the EPS
					 * item.  Supersedes the title found
					 * in the EPS file. If NULL, indicates
					 * that the title found in the EPS
					 * file should be used. */
    int width, height;			/* Requested dimension of EPS item in
					 * canvas coordinates.  If non-zero,
					 * this overrides the dimension
					 * computed from the "%%BoundingBox:"
					 * specification in the EPS file
					 * used. */
    int showImage;			/* Indicates if the image or the
					 * outline rectangle should be
					 * displayed */

    int quick;
    unsigned int flags;

    XColor *fillColor;			/* Fill color of the image outline. */

    Tk_3DBorder border;			/* Outline color */

    int borderWidth;
    int relief;

    TextStyle titleStyle;		/* Font, color, etc. for title */
    Blt_Font font;		
    Pixmap stipple;			/* Stipple for image fill */

    ClientData tiffPtr;
#ifdef WIN32
    HENHMETAFILE *hMetaFile;		/* Windows metafile. */
#endif
} Eps;

static int StringToFont(ClientData clientData, Tcl_Interp *interp,
	Tk_Window tkwin, const char *string, char *widgRec, int offset);
static char *FontToString (ClientData clientData, Tk_Window tkwin, 
	char *widgRec, int offset, Tcl_FreeProc **proc);

static Tk_CustomOption bltFontOption =
{
    StringToFont, FontToString, (ClientData)0
};


static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_ANCHOR, (char *)"-anchor", (char *)NULL, (char *)NULL,
	DEF_EPS_ANCHOR, Blt_Offset(Eps, anchor),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_SYNONYM, (char *)"-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, (char *)"-borderwidth", "borderWidth", (char *)NULL,
	DEF_EPS_BORDERWIDTH, Blt_Offset(Eps, borderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_STRING, (char *)"-file", (char *)NULL, (char *)NULL,
	DEF_EPS_FILE_NAME, Blt_Offset(Eps, fileName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, (char *)"-font", "font", "Font",
        DEF_EPS_FONT, Blt_Offset(Eps, font), 0, &bltFontOption},
    {TK_CONFIG_COLOR, (char *)"-fill", "fill", (char *)NULL,
	DEF_EPS_FILL_COLOR, Blt_Offset(Eps, fillColor), 0},
    {TK_CONFIG_CUSTOM, (char *)"-height", (char *)NULL, (char *)NULL,
	DEF_EPS_HEIGHT, Blt_Offset(Eps, height),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_STRING, (char *)"-image", (char *)NULL, (char *)NULL,
	DEF_EPS_IMAGE_NAME, Blt_Offset(Eps, previewImageName),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_JUSTIFY, (char *)"-justify", "justify", "Justify",
	DEF_EPS_JUSTIFY, Blt_Offset(Eps, titleStyle.justify),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BORDER, (char *)"-outline", "outline", (char *)NULL,
	DEF_EPS_OUTLINE_COLOR, Blt_Offset(Eps, border),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, (char *)"-quick", "quick", "Quick",
	DEF_EPS_QUICK_RESIZE, Blt_Offset(Eps, quick),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_RELIEF, (char *)"-relief", (char *)NULL, (char *)NULL,
	DEF_EPS_RELIEF, Blt_Offset(Eps, relief),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, (char *)"-showimage", "showImage", "ShowImage",
	DEF_EPS_SHOW_IMAGE, Blt_Offset(Eps, showImage),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BITMAP, (char *)"-stipple", (char *)NULL, (char *)NULL,
	DEF_EPS_STIPPLE, Blt_Offset(Eps, stipple), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, (char *)"-tags", (char *)NULL, (char *)NULL,
	DEF_EPS_TAGS, 0, TK_CONFIG_NULL_OK, &tagsOption},
    {TK_CONFIG_STRING, (char *)"-title", (char *)NULL, (char *)NULL,
	DEF_EPS_TITLE, Blt_Offset(Eps, reqTitle), TK_CONFIG_NULL_OK},
    {TK_CONFIG_ANCHOR, (char *)"-titleanchor", (char *)NULL, (char *)NULL,
	DEF_EPS_TITLE_ANCHOR, Blt_Offset(Eps, titleStyle.anchor), 0},
    {TK_CONFIG_COLOR, (char *)"-titlecolor", (char *)NULL, (char *)NULL,
	DEF_EPS_TITLE_COLOR, Blt_Offset(Eps, titleStyle.color), 0},
    {TK_CONFIG_CUSTOM, (char *)"-width", (char *)NULL, (char *)NULL,
	DEF_EPS_WIDTH, Blt_Offset(Eps, width),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

/* Prototypes for procedures defined in this file: */
static Tk_ImageChangedProc ImageChangedProc;
static Tk_ItemCoordProc EpsCoords;
static Tk_ItemAreaProc EpsToArea;
static Tk_ItemPointProc EpsToPoint;
static Tk_ItemConfigureProc ConfigureEps;
static Tk_ItemCreateProc CreateEps;
static Tk_ItemDeleteProc DeleteEps;
static Tk_ItemDisplayProc DisplayEps;
static Tk_ItemScaleProc ScaleEps;
static Tk_ItemTranslateProc TranslateEps;
static Tk_ItemPostscriptProc EpsToPostScript;

static void ComputeEpsBbox(Tk_Canvas canvas, Eps *imgPtr);
static int ReadPostScript(Tcl_Interp *interp, Eps *epsPtr);


/*
 *---------------------------------------------------------------------------
 *
 * StringToFont --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToFont(
    ClientData clientData,		/* Indicated how to check distance */
    Tcl_Interp *interp,			/* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Window */
    const char *string,			/* Pixel value string */
    char *widgRec,			/* Widget record */
    int offset)				/* Offset of pixel size in record */
{
    Blt_Font *fontPtr = (Blt_Font *)(widgRec + offset);
    Blt_Font font;

    font = Blt_GetFont(interp, tkwin, string);
    if (font == NULL) {
	return TCL_ERROR;
    }
    *fontPtr = font;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * FontToString --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
FontToString(
    ClientData clientData,		/* Not used. */
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,			/* Widget structure record */
    int offset,				/* Offset in widget record */
    Tcl_FreeProc **freeProcPtr)		/* Not used. */
{
    Blt_Font font = *(Blt_Font *)(widgRec + offset);
    const char *string;

    string = Blt_NameOfFont(font);
    *freeProcPtr = (Tcl_FreeProc *)TCL_STATIC;
    return (char *)string;
}

static char *
SkipBlanks(ParseInfo *piPtr)
{
    char *s;

    for (s = piPtr->line; isspace(UCHAR(*s)); s++) {
	/*empty*/
    }
    return s;
}

static int
ReadPsLine(ParseInfo *piPtr)
{
    int nBytes;

    nBytes = 0;
    if (ftell(piPtr->f) < piPtr->maxBytes) {
	char *cp;

	cp = piPtr->line;
	while ((*cp = fgetc(piPtr->f)) != EOF) {
	    if (*cp == '\r') {
		continue;
	    }
	    nBytes++;
	    if ((*cp == '\n') || (nBytes >= MAX_EPS_LINE_LENGTH)) {
		break;
	    }
	    cp++;
	}
	if (*cp == '\n') {
	    piPtr->lineNumber++;
	}
	*cp = '\0';
    }
    return nBytes;
}

/*
 *---------------------------------------------------------------------------
 *
 * ReverseBits --
 *
 *	Convert a byte from a X image into PostScript image order.  This
 *	requires not only the nybbles to be reversed but also their bit
 *	values.
 *
 * Results:
 *	The converted byte is returned.
 *
 *---------------------------------------------------------------------------
 */
INLINE static unsigned char
ReverseBits(unsigned char byte)
{
    byte = ((byte >> 1) & 0x55) | ((byte << 1) & 0xaa);
    byte = ((byte >> 2) & 0x33) | ((byte << 2) & 0xcc);
    byte = ((byte >> 4) & 0x0f) | ((byte << 4) & 0xf0);
    return byte;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetHexValue --
 *
 *	Reads the next ASCII hex value from EPS preview image and converts it.
 *
 * Results:
 *	One of three TCL return values is possible.
 *
 *	TCL_OK		the next byte was successfully parsed.
 *	TCL_ERROR	an error occurred processing the next hex value.
 *	TCL_RETURN	"%%EndPreview" line was detected.
 *
 *	The converted hex value is returned via "bytePtr".
 *
 *---------------------------------------------------------------------------
 */
static int
GetHexValue(ParseInfo *piPtr, unsigned char *bytePtr)
{
    char *p;
    unsigned int byte;
    unsigned char a, b;

    p = piPtr->nextPtr;
    if (p == NULL) {

      nextLine:
	if (!ReadPsLine(piPtr)) {
#if DEBUG_READER
	    PurifyPrintf("short file\n");
#endif
	    return TCL_ERROR;		/* Short file */
	}
	if (piPtr->line[0] != '%') {
#if DEBUG_READER
	    PurifyPrintf("line doesn't start with %% (%s)\n", piPtr->line);
#endif
	    return TCL_ERROR;
	}
	if ((piPtr->line[1] == '%') &&
	    (strncmp(piPtr->line + 2, "EndPreview", 10) == 0)) {
#if DEBUG_READER
	    PurifyPrintf("end of preview (%s)\n", piPtr->line);
#endif
	    return TCL_RETURN;
	}
	p = piPtr->line + 1;
    }
    while (isspace((int)*p)) {
	p++;				/* Skip spaces */
    }
    if (*p == '\0') {
	goto nextLine;
    }

    a = piPtr->hexTable[(int)p[0]];
    b = piPtr->hexTable[(int)p[1]];

    if ((a == 0xFF) || (b == 0xFF)) {
#if DEBUG_READER
	PurifyPrintf("not a hex digit (%s)\n", piPtr->line);
#endif
	return TCL_ERROR;
    }
    byte = (a << 4) | b;
    p += 2;
    piPtr->nextPtr = p;
    *bytePtr = byte;
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ReadEPSI --
 *
 *	Reads the EPS preview image from the PostScript file, converting it
 *	into a picture.  If an error occurs when parsing the preview, the
 *	preview is silently ignored.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
ReadEPSI(Eps *epsPtr, ParseInfo *piPtr)
{
    Blt_Picture picture;
    int width, height, bitsPerPixel, nLines;
    char *dscBeginPreview;

    dscBeginPreview = piPtr->line + 16;
    if (sscanf(dscBeginPreview, "%d %d %d %d", &width, &height, &bitsPerPixel, 
	&nLines) != 4) {
#if DEBUG_READER
	PurifyPrintf("bad %%BeginPreview (%s) format\n", dscBeginPreview);
#endif
	return;
    }
    if (((bitsPerPixel != 1) && (bitsPerPixel != 8)) || (width < 1) ||
	(width > SHRT_MAX) || (height < 1) || (height > SHRT_MAX)) {
#if DEBUG_READER
	PurifyPrintf("Bad %%BeginPreview (%s) values\n", dscBeginPreview);
#endif
	return;			/* Bad "%%BeginPreview:" information */
    }
    epsPtr->firstLine = piPtr->lineNumber;
    Blt_InitHexTable(piPtr->hexTable);
    piPtr->nextPtr = NULL;
    picture = Blt_CreatePicture(width, height);

    if (bitsPerPixel == 8) {
	Blt_Pixel *destRowPtr;
	int y;

	destRowPtr = Blt_PictureBits(picture) + 
	    (height - 1) * Blt_PictureStride(picture);
	for (y = height - 1; y >= 0; y--) {
	    Blt_Pixel *dp;
	    int x;

	    dp = destRowPtr;
	    for (x = 0; x < width; x++, dp++) {
		int result;
		unsigned char byte;

		result = GetHexValue(piPtr, &byte);
		if (result == TCL_ERROR) {
		    goto error;
		}
		if (result == TCL_RETURN) {
		    goto done;
		}
		dp->Red = dp->Green = dp->Blue = ~byte;
		dp->Alpha = ALPHA_OPAQUE;
	    }
	    destRowPtr -= Blt_PictureStride(picture);
	}
    } else if (bitsPerPixel == 1) {
	Blt_Pixel *destRowPtr;
	int y;

	destRowPtr = Blt_PictureBits(picture);
	for (y = 0; y < height; y++) {
	    Blt_Pixel *dp, *dend;
	    int bit;
	    unsigned char byte;

	    bit = 8;
	    byte = 0;			/* Suppress compiler warning. */
	    for (dp = destRowPtr, dend = dp + width; dp < dend; dp++) {
		if (bit == 8) {
		    int result;

		    result = GetHexValue(piPtr, &byte);
		    if (result == TCL_ERROR) {
			goto error;
		    }
		    if (result == TCL_RETURN) {
			goto done;
		    }
		    byte = ReverseBits(byte);
		    bit = 0;
		}
		if (((byte >> bit) & 0x01) == 0) {
		    dp->u32 = 0xFFFFFFFF;
		}
		bit++;
	    }
	    destRowPtr += Blt_PictureStride(picture);
	}
    } else {
	fprintf(stderr, "unknown EPSI bitsPerPixel (%d)\n", bitsPerPixel);
    }
  done:
    epsPtr->original = picture;
    epsPtr->origFromPicture = FALSE;
    epsPtr->lastWidth = Blt_PictureWidth(picture);
    epsPtr->lastHeight = Blt_PictureHeight(picture);
    epsPtr->lastLine = piPtr->lineNumber + 1;
    return;

  error:
    epsPtr->firstLine = epsPtr->lastLine = -1;
    if (picture != NULL) {
	Blt_FreePicture(picture);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ReadPostScript --
 *
 *	This routine reads and parses the few fields we need out of an EPS
 *	file.
 *
 *	The EPS standards are outlined from Appendix H of the "PostScript
 *	Language Reference Manual" pp. 709-736.
 *
 *	Mandatory fields:
 *
 *	- Starts with "%!PS*"
 *	- Contains "%%BoundingBox: llx lly urx ury"
 *
 *	Optional fields for EPS item:
 *	- "%%BeginPreview: w h bpp #lines"
 *		Preview is in hexadecimal. Each line must start with "%"
 *      - "%%EndPreview"
 *	- "%%Title: (string)"
 *
 *---------------------------------------------------------------------------
 */
static int
ReadPostScript(Tcl_Interp *interp, Eps *epsPtr)
{
    char *field;
    char *dscTitle, *dscBoundingBox;
    char *dscEndComments;
    ParseInfo pi;

    pi.line[0] = '\0';
    pi.maxBytes = epsPtr->psLength;
    pi.lineNumber = 0;
    pi.f = epsPtr->psFile;

    Tcl_DStringInit(&epsPtr->dString);
    if (pi.maxBytes == 0) {
	pi.maxBytes = INT_MAX;
    }
    if (epsPtr->psStart > 0) {
	if (fseek(epsPtr->psFile, epsPtr->psStart, 0) != 0) {
	    Tcl_AppendResult(interp, 
			     "can't seek to start of PostScript code in \"", 
			     epsPtr->fileName, "\"", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    if (!ReadPsLine(&pi)) {
	Tcl_AppendResult(interp, "file \"", epsPtr->fileName, "\" is empty?",
	    (char *)NULL);
	return TCL_ERROR;
    }
    if (strncmp(pi.line, "%!PS", 4) != 0) {
	Tcl_AppendResult(interp, "file \"", epsPtr->fileName,
	    "\" doesn't start with \"%!PS\"", (char *)NULL);
	return TCL_ERROR;
    }

    /*
     * Initialize field flags to NULL. We want to look only at the first
     * appearance of these comment fields.  The file itself may have another
     * EPS file embedded into it.
     */
    dscBoundingBox = dscTitle = dscEndComments = NULL;
    pi.lineNumber = 1;
    while (ReadPsLine(&pi)) {
	pi.lineNumber++;
	if ((pi.line[0] == '%') && (pi.line[1] == '%')) { /* Header comment */
	    field = pi.line + 2;
	    if (field[0] == 'B') {
		if (strncmp(field, "BeginSetup", 8) == 0) {
		    break;		/* Done */
		}
		if (strncmp(field, "BeginProlog", 8) == 0) {
		    break;		/* Done */
		}
		if ((strncmp(field, "BoundingBox:", 12) == 0) &&
		    (dscBoundingBox == NULL)) {
		    int nFields;
		    
		    dscBoundingBox = field + 12;
		    nFields = sscanf(dscBoundingBox, "%d %d %d %d",
				     &epsPtr->llx, &epsPtr->lly,
				     &epsPtr->urx, &epsPtr->ury);
		    if (nFields != 4) {
			Tcl_AppendResult(interp,
					 "bad \"%%BoundingBox\" values: \"",
					 dscBoundingBox, "\"", (char *)NULL);
			goto error;
		    }
		}
	    } else if ((field[0] == 'T') &&
		(strncmp(field, "Title:", 6) == 0)) {
		if (dscTitle == NULL) {
		    char *lp, *rp;

		    lp = strchr(field + 6, '(');
		    if (lp != NULL) {
			rp = strrchr(field + 6, ')');
			if (rp != NULL) {
			    *rp = '\0';
			}
			dscTitle = Blt_AssertStrdup(lp + 1);
		    } else {
			dscTitle = Blt_AssertStrdup(field + 6);
		    }
		}
	    } else if (field[0] == 'E') {
		if (strncmp(field, "EndComments", 11) == 0) {
		    dscEndComments = field;
		    break;		/* Done */
		}
	    }
	} /* %% */
    }
    if (dscBoundingBox == NULL) {
	Tcl_AppendResult(interp, "no \"%%BoundingBox:\" found in \"",
			 epsPtr->fileName, "\"", (char *)NULL);
	goto error;
    }
    if (dscEndComments != NULL) {
	/* Check if a "%%BeginPreview" immediately follows */
	while (ReadPsLine(&pi)) {
	    field = SkipBlanks(&pi);
	    if (field[0] != '\0') {
		break;
	    }
	}
	if (strncmp(pi.line, "%%BeginPreview:", 15) == 0) {
	    ReadEPSI(epsPtr, &pi);
	}
    }
    if (dscTitle != NULL) {
	epsPtr->title = dscTitle;
    }
    /* Finally save the PostScript into a dynamic string. */
    while (ReadPsLine(&pi)) {
	Tcl_DStringAppend(&epsPtr->dString, pi.line, -1);
	Tcl_DStringAppend(&epsPtr->dString, "\n", 1);
    }
    return TCL_OK;
 error:
    if (dscTitle != NULL) {
	Blt_Free(dscTitle);
    }
    return TCL_ERROR;	/* BoundingBox: is required. */
}

static int
OpenEpsFile(Tcl_Interp *interp, Eps *epsPtr)
{
    FILE *f;
#ifdef WIN32
    DOSEPSHEADER dosHeader;
    int nBytes;
#endif

    f = Blt_OpenFile(interp, epsPtr->fileName, "rb");
    if (f == NULL) {
	Tcl_AppendResult(epsPtr->interp, "can't open \"", epsPtr->fileName,
	    "\": ", Tcl_PosixError(epsPtr->interp), (char *)NULL);
	return TCL_ERROR;
    }
    epsPtr->psFile = f;
    epsPtr->psStart = epsPtr->psLength = 0L;
    epsPtr->wmfStart = epsPtr->wmfLength = 0L;
    epsPtr->tiffStart = epsPtr->tiffLength = 0L;
    
#ifdef WIN32
    nBytes = fread(&dosHeader, sizeof(DOSEPSHEADER), 1, f);
    if ((nBytes == sizeof(DOSEPSHEADER)) &&
	(dosHeader.magic[0] == 0xC5) && (dosHeader.magic[1] == 0xD0) &&
	(dosHeader.magic[2] == 0xD3) && (dosHeader.magic[3] == 0xC6)) {

	/* DOS EPS file */
	epsPtr->psStart = dosHeader.psStart;
	epsPtr->wmfStart = dosHeader.wmfStart;
	epsPtr->wmfLength = dosHeader.wmfLength;
	epsPtr->tiffStart = dosHeader.tiffStart;
	epsPtr->tiffLength = dosHeader.tiffLength;
	epsPtr->previewFormat = PS_PREVIEW_EPSI;
#ifdef HAVE_TIFF_H
	if (epsPtr->tiffLength > 0) {
	    epsPtr->previewFormat = PS_PREVIEW_TIFF;
	}	    
#endif /* HAVE_TIFF_H */
	if (epsPtr->wmfLength > 0) {
	    epsPtr->previewFormat = PS_PREVIEW_WMF;
	}
    }
    fseek(f, 0, 0);
#endif /* WIN32 */
    return ReadPostScript(interp, epsPtr);
}

static void
CloseEpsFile(Eps *epsPtr)
{
    if (epsPtr->psFile != NULL) {
	fclose(epsPtr->psFile);
	epsPtr->psFile = NULL;
    }
}

#ifdef WIN32
#ifdef HAVE_TIFF_H
static void
ReadTiffPreview(Eps *epsPtr)
{
    unsigned int width, height;
    Blt_Picture picture;
    Blt_Pixel *dataPtr;
    FILE *f;
    int n;

    TIFFGetField(epsPtr->tiffPtr, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(epsPtr->tiffPtr, TIFFTAG_IMAGELENGTH, &height);
    picture = Blt_CreatePicture(width, height);
    dataPtr = Blt_PictureBits(picture);
    if (!TIFFReadRGBAImage(epsPtr->tiffPtr, width, height, dataPtr, 0)) {
	Blt_FreePicture(picture);
	return;
    }
    /* Reverse the order of the components for each pixel. */
    /* ... */
    epsPtr->origFromPicture = FALSE;
    epsPtr->picture = picture;
}
#endif

#ifdef notdef
ReadWMF(f, epsPtr, headerPtr)
    FILE *f;
{
    HANDLE hMem;
    Tk_Window tkwin;

    if (fseek(f, headerPtr->wmfStart, 0) != 0) {
	Tcl_AppendResult(interp, "can't seek in \"", epsPtr->fileName, 
			 "\"", (char *)NULL);
	return TCL_ERROR;
    }
    hMem = GlobalAlloc(GHND, size);
    if (hMem == NULL) {
	Tcl_AppendResult(graphPtr->interp, "can't allocate global memory:", 
			 Blt_LastError(), (char *)NULL);
	return TCL_ERROR;
    }
    buffer = (LPVOID)GlobalLock(hMem);
    /* Read the header and see what kind of meta file it is. */
    fread(buffer, sizeof(unsigned char), headerPtr->wmfLength, f);
    mfp.mm = 0;
    mfp.xExt = epsPtr->width;
    mfp.yExt = epsPtr->height;
    mfp.hMF = hMetaFile;
    tkwin = Tk_CanvasTkwin(epsPtr->canvas);
    hRefDC = TkWinGetDrawableDC(Tk_Display(tkwin), Tk_WindowId(tkwin), &state);
    hDC = CreateEnhMetaFile(hRefDC, NULL, NULL, NULL);
    mfp.hMF = CloseEnhMetaFile(hDC);
    hMetaFile = SetWinMetaFileBits(size, buffer, MM_ANISOTROPIC, &picture);
	Tcl_AppendResult(graphPtr->interp, "can't get metafile data:", 
		Blt_LastError(), (char *)NULL);
	goto error;
}
#endif
#endif /* WIN32 */
/*
 *---------------------------------------------------------------------------
 *
 * DeleteEps --
 *
 *	This procedure is called to clean up the data structure associated
 *	with a EPS item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources associated with itemPtr are released.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
DeleteEps( 
    Tk_Canvas canvas,			/* Info about overall canvas
					 * widget. */
    Tk_Item *itemPtr,			/* Item that is being deleted. */
    Display *display)			/* Display containing window for
					 * canvas. */
{
    Eps *epsPtr = (Eps *)itemPtr;

    Tk_FreeOptions(configSpecs, (char *)epsPtr, display, 0);
    CloseEpsFile(epsPtr);
    if ((!epsPtr->origFromPicture) && (epsPtr->original != NULL)) {
	Blt_FreePicture(epsPtr->original);
    }
    if (epsPtr->picture != NULL) {
	Blt_FreePicture(epsPtr->picture);
    }
    if (epsPtr->painter != NULL) {
	Blt_FreePainter(epsPtr->painter);
    }
    if (epsPtr->preview != NULL) {
	Tk_FreeImage(epsPtr->preview);
    }
    if (epsPtr->previewImageName != NULL) {
	Blt_Free(epsPtr->previewImageName);
    }
    if (epsPtr->stipple != None) {
	Tk_FreePixmap(display, epsPtr->stipple);
    }
    if (epsPtr->fillGC != NULL) {
	Tk_FreeGC(display, epsPtr->fillGC);
    }
    Blt_Ts_FreeStyle(display, &epsPtr->titleStyle);

    if (epsPtr->title != NULL) {
	Blt_Free(epsPtr->title);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * CreateEps --
 *
 *	This procedure is invoked to create a new EPS item in a canvas.
 *
 * Results:
 *	A standard TCL return value.  If an error occurred in creating the
 *	item, then an error message is left in interp->result; in this case
 *	itemPtr is left uninitialized, so it can be safely freed by the
 *	caller.
 *
 * Side effects:
 *	A new EPS item is created.
 *
 *---------------------------------------------------------------------------
 */
static int
CreateEps(
    Tcl_Interp *interp,			/* Interpreter for error reporting. */
    Tk_Canvas canvas,			/* Canvas to hold new item. */
    Tk_Item *itemPtr,			/* Record to hold new item; header has
					 * been initialized by caller. */
    int argc,				/* Number of arguments in argv. */
    char **argv)			/* Arguments describing rectangle. */
{
    Eps *epsPtr = (Eps *)itemPtr;
    Tk_Window tkwin;
    double x, y;

    tkwin = Tk_CanvasTkwin(canvas);
    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
	    Tk_PathName(tkwin), " create ", itemPtr->typePtr->name,
	    " x1 y1 ?options?\"", (char *)NULL);
	return TCL_ERROR;
    }

    /* Initialize the item's record by hand (bleah). */
    epsPtr->anchor = TK_ANCHOR_NW;
    epsPtr->border = NULL;
    epsPtr->borderWidth = 0;
    epsPtr->canvas = canvas;
    epsPtr->fileName = NULL;
    epsPtr->psFile = NULL;
    epsPtr->fillGC = NULL;
    epsPtr->fillColor = NULL;
    epsPtr->painter = NULL;
    epsPtr->original = NULL;
    epsPtr->origFromPicture = FALSE;
    epsPtr->previewImageName = NULL;
    epsPtr->preview = NULL;
    epsPtr->interp = interp;
    epsPtr->picture = NULL;
    epsPtr->firstLine = epsPtr->lastLine = -1;
    epsPtr->relief = TK_RELIEF_SUNKEN;
    epsPtr->reqTitle = NULL;
    epsPtr->stipple = None;
    epsPtr->showImage = TRUE;
    epsPtr->quick = FALSE;
    epsPtr->title = NULL;
    epsPtr->lastWidth = epsPtr->lastHeight = 0;
    epsPtr->width = epsPtr->height = 0;
    epsPtr->x = epsPtr->y = 0.0; 
    epsPtr->llx = epsPtr->lly = epsPtr->urx = epsPtr->ury = 0;
    epsPtr->bb.left = epsPtr->bb.right = epsPtr->bb.top = epsPtr->bb.bottom = 0;
    Tcl_DStringInit(&epsPtr->dString);
    Blt_Ts_InitStyle(epsPtr->titleStyle);
#define PAD	8
    Blt_Ts_SetPadding(epsPtr->titleStyle, PAD, PAD, PAD, PAD);

    /* Process the arguments to fill in the item record. */
    if ((Tk_CanvasGetCoord(interp, canvas, argv[0], &x) != TCL_OK) ||
	(Tk_CanvasGetCoord(interp, canvas, argv[1], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    epsPtr->x = x;
    epsPtr->y = y;
    if (ConfigureEps(interp, canvas, itemPtr, argc - 2, argv + 2, 0) 
	!= TCL_OK) {
	DeleteEps(canvas, itemPtr, Tk_Display(tkwin));
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ImageChangedProc
 *
 *	The image is over-written each time the EPS item is resized.  So we
 *	only worry if the image is deleted.
 *
 *	We always resample from the picture we saved when the photo image was
 *	specified (-image option).
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
    Eps *epsPtr = clientData;

    if ((epsPtr->preview == NULL) || (Blt_Image_IsDeleted(epsPtr->preview))) {
	epsPtr->preview = NULL;
	if (epsPtr->previewImageName != NULL) {
	    Blt_Free(epsPtr->previewImageName);
	    epsPtr->previewImageName = NULL;
	}
	Tk_CanvasEventuallyRedraw(epsPtr->canvas, epsPtr->item.x1, 
		epsPtr->item.y1, epsPtr->item.x2, epsPtr->item.y2);
    }
    if (epsPtr->preview != NULL) {
	int result;

	if ((!epsPtr->origFromPicture) && (epsPtr->original != NULL)) {
	    Blt_FreePicture(epsPtr->original);
	}
	result = Blt_GetPicture(epsPtr->interp, epsPtr->previewImageName, 
				&epsPtr->original);
	if (result == TCL_OK) {
	    epsPtr->origFromPicture = TRUE;
	} else {
	    Tk_PhotoHandle photo;	/* Photo handle to Tk image. */
	    
	    photo = Tk_FindPhoto(epsPtr->interp, epsPtr->previewImageName);
	    if (photo == NULL) {
		fprintf(stderr, "image \"%s\" isn't a picture or photo image\n",
			epsPtr->previewImageName);
		return;
	    }
	    epsPtr->original = Blt_PhotoToPicture(photo);
	    epsPtr->origFromPicture = FALSE;
	}
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ConfigureEps --
 *
 *	This procedure is invoked to configure various aspects of an EPS item,
 *	such as its background color.
 *
 * Results:
 *	A standard TCL result code.  If an error occurs, then an error message
 *	is left in interp->result.
 *
 * Side effects:
 *	Configuration information may be set for itemPtr.
 *
 *---------------------------------------------------------------------------
 */
static int
ConfigureEps(
    Tcl_Interp *interp,			/* Used for error reporting. */
    Tk_Canvas canvas,			/* Canvas containing itemPtr. */
    Tk_Item *itemPtr,			/* EPS item to reconfigure. */
    int argc,				/* Number of elements in argv.  */
    char **argv,			/* Arguments describing things to
					 * configure. */
    int flags)				/* Flags to pass to
					 * Tk_ConfigureWidget. */
{
    Eps *epsPtr = (Eps *)itemPtr;
    Tk_Window tkwin;
    XGCValues gcValues;
    unsigned long gcMask;
    GC newGC;
    int width, height;
    Blt_Painter painter;

    tkwin = Tk_CanvasTkwin(canvas);
    if (Tk_ConfigureWidget(interp, tkwin, configSpecs, argc, (const char**)argv,
		(char *)epsPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    painter = Blt_GetPainter(tkwin, 1.0);
    if (epsPtr->painter != NULL) {
	Blt_FreePainter(epsPtr->painter);
    }
    epsPtr->painter = painter;
    /* Determine the size of the EPS item */
    /*
     * Check for a "-image" option specifying an image to be displayed
     * representing the EPS canvas item.
     */
    if (Blt_OldConfigModified(configSpecs, "-image", (char *)NULL)) {
	if (epsPtr->preview != NULL) {
	    Tk_FreeImage(epsPtr->preview);	/* Release old Tk image */
	    if ((!epsPtr->origFromPicture) && (epsPtr->original != NULL)) {
		Blt_FreePicture(epsPtr->original);
	    }
	    epsPtr->original = NULL;
	    if (epsPtr->picture != NULL) {
		Blt_FreePicture(epsPtr->picture);
	    }
	    epsPtr->picture = NULL;
	    epsPtr->preview = NULL;
	    epsPtr->origFromPicture = FALSE;
	}
	if (epsPtr->previewImageName != NULL) {
	    int result;

	    /* Allocate a new image, if one was named. */
	    epsPtr->preview = Tk_GetImage(interp, tkwin, 
			epsPtr->previewImageName, ImageChangedProc, epsPtr);
	    if (epsPtr->preview == NULL) {
		Tcl_AppendResult(interp, "can't find an image \"",
		    epsPtr->previewImageName, "\"", (char *)NULL);
		Blt_Free(epsPtr->previewImageName);
		epsPtr->previewImageName = NULL;
		return TCL_ERROR;
	    }
	    result = Blt_GetPicture(interp, epsPtr->previewImageName, 
				    &epsPtr->original);
	    if (result == TCL_OK) {
		epsPtr->origFromPicture = TRUE;
	    } else {
		Tk_PhotoHandle photo;	/* Photo handle to Tk image. */

		photo = Tk_FindPhoto(interp, epsPtr->previewImageName);
		if (photo == NULL) {
		    Tcl_AppendResult(interp, "image \"", 
			epsPtr->previewImageName,
			"\" is not a picture or photo image", (char *)NULL);
		    return TCL_ERROR;
		}
		epsPtr->original = Blt_PhotoToPicture(photo);
		epsPtr->origFromPicture = FALSE;
	    }
	}
    }
    if (Blt_OldConfigModified(configSpecs, "-file", (char *)NULL)) {
	CloseEpsFile(epsPtr);
	if ((!epsPtr->origFromPicture) && (epsPtr->original != NULL)) {
	    Blt_FreePicture(epsPtr->original);
	    epsPtr->original = NULL;
	}
	if (epsPtr->picture != NULL) {
	    Blt_FreePicture(epsPtr->picture);
	    epsPtr->picture = NULL;
	}
	epsPtr->firstLine = epsPtr->lastLine = -1;
	if (epsPtr->fileName != NULL) {
	    if (OpenEpsFile(interp, epsPtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    /* Compute the normal width and height of the item, but let the
     * user-requested dimensions override them. */
    width = height = 0;
    if (epsPtr->preview != NULL) {
	/* Default dimension is the size of the image. */
	Tk_SizeOfImage(epsPtr->preview, &width, &height);
    }
    if (epsPtr->fileName != NULL) {
	/* Use dimensions provided by the BoundingBox. */
	width = (epsPtr->urx - epsPtr->llx); 
	height = (epsPtr->ury - epsPtr->lly); 
    }
    if (epsPtr->width == 0) {
	epsPtr->width = width;
    }
    if (epsPtr->height == 0) {
	epsPtr->height = height;
    }

    if (Blt_OldConfigModified(configSpecs, "-quick", (char *)NULL)) {
	epsPtr->lastWidth = epsPtr->lastHeight = 0;
    }
    /* Fill color GC */

    newGC = NULL;
    if (epsPtr->fillColor != NULL) {
	gcMask = GCForeground;
	gcValues.foreground = epsPtr->fillColor->pixel;
	if (epsPtr->stipple != None) {
	    gcMask |= (GCStipple | GCFillStyle);
	    gcValues.stipple = epsPtr->stipple;
	    if (epsPtr->border != NULL) {
		gcValues.foreground = Tk_3DBorderColor(epsPtr->border)->pixel;
		gcValues.background = epsPtr->fillColor->pixel;
		gcMask |= GCBackground;
		gcValues.fill_style = FillOpaqueStippled;
	    } else {
		gcValues.fill_style = FillStippled;
	    }
	}
	newGC = Tk_GetGC(tkwin, gcMask, &gcValues);
    }
    if (epsPtr->fillGC != NULL) {
	Tk_FreeGC(Tk_Display(tkwin), epsPtr->fillGC);
    }
    epsPtr->fillGC = newGC;
    CloseEpsFile(epsPtr);
    ComputeEpsBbox(canvas, epsPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * EpsCoords --
 *
 *	This procedure is invoked to process the "coords" widget command on
 *	EPS items.  See the user documentation for details on what it does.
 *
 * Results:
 *	Returns TCL_OK or TCL_ERROR, and sets interp->result.
 *
 * Side effects:
 *	The coordinates for the given item may be changed.
 *
 *---------------------------------------------------------------------------
 */
static int
EpsCoords(
    Tcl_Interp *interp,			/* Used for error reporting. */
    Tk_Canvas canvas,			/* Canvas containing item. */
    Tk_Item *itemPtr,			/* Item whose coordinates are to be
					 * read or modified. */
    int argc,				/* Number of coordinates supplied in
					 * argv. */
    char **argv)			/* Array of coordinates: x1, y1, x2,
					 * y2, ... */
{
    Eps *epsPtr = (Eps *)itemPtr;

    if ((argc != 0) && (argc != 2)) {
	Tcl_AppendResult(interp, "wrong # coordinates: expected 0 or 2, got ",
	    Blt_Itoa(argc), (char *)NULL);
	return TCL_ERROR;
    }
    if (argc == 2) {
	double x, y;			/* Don't overwrite old coordinates on
					 * errors */

	if ((Tk_CanvasGetCoord(interp, canvas, argv[0], &x) != TCL_OK) ||
	    (Tk_CanvasGetCoord(interp, canvas, argv[1], &y) != TCL_OK)) {
	    return TCL_ERROR;
	}
	epsPtr->x = x;
	epsPtr->y = y;
	ComputeEpsBbox(canvas, epsPtr);
	return TCL_OK;
    }
    Tcl_AppendElement(interp, Blt_Dtoa(interp, epsPtr->x));
    Tcl_AppendElement(interp, Blt_Dtoa(interp, epsPtr->y));
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ComputeEpsBbox --
 *
 *	This procedure is invoked to compute the bounding box of all the
 *	pixels that may be drawn as part of a EPS item.  This procedure is
 *	where the preview image's placement is computed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The fields x1, y1, x2, and y2 are updated in the item for itemPtr.
 *
 *---------------------------------------------------------------------------
 */
 /* ARGSUSED */
static void
ComputeEpsBbox(
    Tk_Canvas canvas,			/* Canvas that contains item. */
    Eps *epsPtr)			/* Item whose bbox is to be
					 * recomputed. */
{
    Point2d anchorPos;

    /* Translate the coordinates wrt the anchor. */
    anchorPos = Blt_AnchorPoint(epsPtr->x, epsPtr->y, (double)epsPtr->width, 
	(double)epsPtr->height, epsPtr->anchor);
    /*
     * Note: The right and bottom are exterior to the item.  
     */
    epsPtr->bb.left = anchorPos.x;
    epsPtr->bb.top = anchorPos.y;
    epsPtr->bb.right = epsPtr->bb.left + epsPtr->width;
    epsPtr->bb.bottom = epsPtr->bb.top + epsPtr->height;

    epsPtr->item.x1 = ROUND(epsPtr->bb.left);
    epsPtr->item.y1 = ROUND(epsPtr->bb.top);
    epsPtr->item.x2 = ROUND(epsPtr->bb.right);
    epsPtr->item.y2 = ROUND(epsPtr->bb.bottom);
}

/*
 *---------------------------------------------------------------------------
 *
 * DisplayEps --
 *
 *	This procedure is invoked to draw the EPS item in a given drawable.
 *	The EPS item may be drawn as either a solid rectangle or a pixmap of
 *	the preview image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	ItemPtr is drawn in drawable using the transformation information in
 *	canvas.
 *
 *---------------------------------------------------------------------------
 */
static void
DisplayEps(
    Tk_Canvas canvas,			/* Canvas that contains item. */
    Tk_Item *itemPtr,			/* Item to be displayed. */
    Display *display,			/* Display on which to draw item. */
    Drawable drawable,			/* Pixmap or window in which to draw
					 * item. */
    int rx, int ry, 
    int rw, int rh)			/* Describes region of canvas that
					 * must be redisplayed (not used). */
{
    Blt_Picture picture;
    Eps *epsPtr = (Eps *)itemPtr;
    Tk_Window tkwin;
    const char *title;
    int w, h;
    short int dx, dy;

    w = (int)(epsPtr->bb.right - epsPtr->bb.left);
    h = (int)(epsPtr->bb.bottom - epsPtr->bb.top);
    if ((w < 1) || (h < 1)) {
	return;
    }
    tkwin = Tk_CanvasTkwin(canvas);
    if (epsPtr->original != NULL) {
	if ((epsPtr->lastWidth != w) || (epsPtr->lastHeight != h)) {
	    if (epsPtr->quick) {
		picture = Blt_ScalePicture(epsPtr->original, 0, 0,
			Blt_PictureWidth(epsPtr->original),
			Blt_PictureHeight(epsPtr->original), w, h);
	    } else {
		fprintf(stderr, "orig=%dx%d new=width=%dx%d last=%dx%d\n", 
			Blt_PictureWidth(epsPtr->original),
			Blt_PictureHeight(epsPtr->original),
			w, h,
			epsPtr->lastWidth, epsPtr->lastHeight);
		picture = Blt_CreatePicture(w, h);
		Blt_ResamplePicture(picture, epsPtr->original, bltBoxFilter, 
			bltBoxFilter);
	    }
	    if (epsPtr->picture != NULL) {
		Blt_FreePicture(epsPtr->picture);
	    }
	    epsPtr->picture = picture;
	    epsPtr->lastHeight = h;
	    epsPtr->lastWidth = w;
	} 
    }
    picture = epsPtr->picture;
    if (picture == NULL) {
	picture = epsPtr->original;
    }

    /*
     * Translate the coordinates to those of the EPS item, then redisplay it.
     */
    Tk_CanvasDrawableCoords(canvas, epsPtr->bb.left, epsPtr->bb.top, 
			    &dx, &dy);

    title = epsPtr->title;

    if (epsPtr->reqTitle != NULL) {
	title = epsPtr->reqTitle;
    }
    if ((epsPtr->showImage) && (picture != NULL)) {
	struct region {
	    short int left, right, top, bottom;
	} p, r;
	short int destX, destY;

	/* The eps item may only partially exposed. Be careful to clip the
	 * unexposed portions. */

	/* Convert everything to screen coordinates since the origin of the
	 * item is only available in */

	p.left = dx, p.top = dy;
	Tk_CanvasDrawableCoords(canvas, epsPtr->bb.right, epsPtr->bb.bottom,
		&p.right, &p.bottom);
	Tk_CanvasDrawableCoords(canvas, (double)rx, (double)ry, 
		&r.left, &r.top);
	Tk_CanvasDrawableCoords(canvas,(double)(rx+rw), (double)(ry+rh), 
		&r.right, &r.bottom);
	destX = (int)dx, destY = (int)dy;
	if (p.left < r.left) {
	    p.left = r.left;
	}
	if (p.top < r.top) {
	    p.top = r.top;
	}
	if (p.right > r.right) {
	    p.right = r.right;
	}
	if (p.bottom > r.bottom) {
	    p.bottom = r.bottom;
	}
	if (destX < r.left) {
	    destX = r.left;
	}
	if (destY < r.top) {
	    destY = r.top;
	}
	p.left -= dx, p.right -= dx;
	p.top -= dy, p.bottom -= dy;;
	if (0 /* epsPtr->quick */) {
	    Blt_Picture fade;

	    fade = Blt_CreatePicture(Blt_PictureWidth(picture), 
				     Blt_PictureHeight(picture));
	    Blt_FadePicture(fade, picture, 0, 0, Blt_PictureWidth(picture), 
		Blt_PictureHeight(picture), 0, 0, 150);
	    Blt_PaintPicture(epsPtr->painter, drawable, fade, 
		(int)p.left, (int)p.top, (int)(p.right - p.left), 
		(int)(p.bottom - p.top), destX, destY, FALSE);
	    Blt_FreePicture(fade);
	} else {
	    Blt_PaintPicture(epsPtr->painter, drawable, picture, (int)p.left, 
		(int)p.top, (int)(p.right - p.left), (int)(p.bottom - p.top), 
		destX, destY, FALSE);
	}
    } else {
	if (epsPtr->fillGC != NULL) {
	    XSetTSOrigin(display, epsPtr->fillGC, dx, dy);
	    XFillRectangle(display, drawable, epsPtr->fillGC, dx, dy,
		epsPtr->width, epsPtr->height);
	    XSetTSOrigin(display, epsPtr->fillGC, 0, 0);
	}
    }

    if (title != NULL) {
	TextLayout *textPtr;
	double rw, rh;
	int dw, dh;

	/* Translate the title to an anchor position within the EPS item */
	epsPtr->titleStyle.font = epsPtr->font;
	textPtr = Blt_Ts_CreateLayout(title, -1, &epsPtr->titleStyle);
	Blt_GetBoundingBox(textPtr->width, textPtr->height, 
	     epsPtr->titleStyle.angle, &rw, &rh, (Point2d *)NULL);
	dw = (int)ceil(rw);
	dh = (int)ceil(rh);
	if ((dw <= w) && (dh <= h)) {
	    int tx, ty;

	    Blt_TranslateAnchor(dx, dy, w, h, epsPtr->titleStyle.anchor, 
		&tx, &ty);
	    if (picture == NULL) {
		tx += epsPtr->borderWidth;
		ty += epsPtr->borderWidth;
	    }
	    Blt_Ts_DrawLayout(tkwin, drawable, textPtr, &epsPtr->titleStyle, 
		tx, ty);
	}
	Blt_Free(textPtr);
    }
    if ((picture == NULL) && (epsPtr->border != NULL) && 
	(epsPtr->borderWidth > 0)) {
	Blt_Draw3DRectangle(tkwin, drawable, epsPtr->border, dx, dy,
	    epsPtr->width, epsPtr->height, epsPtr->borderWidth, epsPtr->relief);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * EpsToPoint --
 *
 *	Computes the distance from a given point to a given rectangle, in
 *	canvas units.
 *
 * Results:
 *	The return value is 0 if the point whose x and y coordinates are
 *	coordPtr[0] and coordPtr[1] is inside the EPS item.  If the point
 *	isn't inside the item then the return value is the distance from the
 *	point to the EPS item.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static double
EpsToPoint(
    Tk_Canvas canvas,			/* Canvas containing item. */
    Tk_Item *itemPtr,			/* Item to check against point. */
    double *pts)			/* Array of x and y coordinates. */
{
    Eps *epsPtr = (Eps *)itemPtr;
    double x, y, dx, dy;

    x = pts[0], y = pts[1];

    /*
     * Check if point is outside the bounding rectangle and compute the
     * distance to the closest side.
     */
    dx = dy = 0;
    if (x < epsPtr->item.x1) {
	dx = epsPtr->item.x1 - x;
    } else if (x > epsPtr->item.x2) {
	dx = x - epsPtr->item.x2;
    }
    if (y < epsPtr->item.y1) {
	dy = epsPtr->item.y1 - y;
    } else if (y > epsPtr->item.y2) {
	dy = y - epsPtr->item.y2;
    }
    return hypot(dx, dy);
}

/*
 *---------------------------------------------------------------------------
 *
 * EpsToArea --
 *
 *	This procedure is called to determine whether an item lies entirely
 *	inside, entirely outside, or overlapping a given rectangle.
 *
 * Results:
 *	-1 is returned if the item is entirely outside the area given by
 *	rectPtr, 0 if it overlaps, and 1 if it is entirely inside the given
 *	area.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
EpsToArea(
    Tk_Canvas canvas,			/* Canvas containing the item. */
    Tk_Item *itemPtr,			/* Item to check against bounding
					 * rectangle. */
    double pts[])			/* Array of four coordinates (x1, y1,
					 * x2, y2) describing area.  */
{
    Eps *epsPtr = (Eps *)itemPtr;

    if ((pts[2] <= epsPtr->bb.left) || (pts[0] >= epsPtr->bb.right) ||
	(pts[3] <= epsPtr->bb.top) || (pts[1] >= epsPtr->bb.bottom)) {
	return -1;			/* Outside. */
    }
    if ((pts[0] <= epsPtr->bb.left) && (pts[1] <= epsPtr->bb.top) &&
	(pts[2] >= epsPtr->bb.right) && (pts[3] >= epsPtr->bb.bottom)) {
	return 1;			/* Inside. */
    }
    return 0;				/* Overlap. */
}

/*
 *---------------------------------------------------------------------------
 *
 * ScaleEps --
 *
 *	This procedure is invoked to rescale an item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The item referred to by itemPtr is rescaled so that the
 *	following transformation is applied to all point coordinates:
 *		x' = xOrigin + xScale*(x-xOrigin)
 *		y' = yOrigin + yScale*(y-yOrigin)
 *
 *---------------------------------------------------------------------------
 */
static void
ScaleEps(
    Tk_Canvas canvas,			/* Canvas containing rectangle. */
    Tk_Item *itemPtr,			/* Rectangle to be scaled. */
    double xOrigin, double yOrigin,	/* Origin wrt scale rect. */
    double xScale, double yScale)
{
    Eps *epsPtr = (Eps *)itemPtr;

    epsPtr->bb.left = xOrigin + xScale * (epsPtr->bb.left - xOrigin);
    epsPtr->bb.right = xOrigin + xScale * (epsPtr->bb.right - xOrigin);
    epsPtr->bb.top = yOrigin + yScale * (epsPtr->bb.top - yOrigin);
    epsPtr->bb.bottom = yOrigin + yScale *(epsPtr->bb.bottom - yOrigin);

    /* Reset the user-requested values to the newly scaled values. */
    epsPtr->width = ROUND(epsPtr->bb.right - epsPtr->bb.left);
    epsPtr->height = ROUND(epsPtr->bb.bottom - epsPtr->bb.top);
    epsPtr->x = ROUND(epsPtr->bb.left);
    epsPtr->y = ROUND(epsPtr->bb.top);

    epsPtr->item.x1 = ROUND(epsPtr->bb.left);
    epsPtr->item.y1 = ROUND(epsPtr->bb.top);
    epsPtr->item.x2 = ROUND(epsPtr->bb.right);
    epsPtr->item.y2 = ROUND(epsPtr->bb.bottom);
}

/*
 *---------------------------------------------------------------------------
 *
 * TranslateEps --
 *
 *	This procedure is called to move an item by a given amount.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The position of the item is offset by (dx, dy), and the bounding box
 *	is updated in the generic part of the item structure.
 *
 *---------------------------------------------------------------------------
 */
static void
TranslateEps(
    Tk_Canvas canvas,			/* Canvas containing item. */
    Tk_Item *itemPtr,			/* Item that is being moved. */
    double dx, double dy)		/* Amount by which item is to be
					 * moved. */
{
    Eps *epsPtr = (Eps *)itemPtr;

    epsPtr->bb.left += dx;
    epsPtr->bb.right += dx;
    epsPtr->bb.top += dy;
    epsPtr->bb.bottom += dy;

    epsPtr->x = epsPtr->bb.left;
    epsPtr->y = epsPtr->bb.top;

    epsPtr->item.x1 = ROUND(epsPtr->bb.left);
    epsPtr->item.x2 = ROUND(epsPtr->bb.right);
    epsPtr->item.y1 = ROUND(epsPtr->bb.top);
    epsPtr->item.y2 = ROUND(epsPtr->bb.bottom);
}

/*
 *---------------------------------------------------------------------------
 *
 * EpsToPostScript --
 *
 *	This procedure is called to generate PostScript for EPS canvas items.
 *
 * Results:
 *	The return value is a standard TCL result.  If an error occurs in
 *	generating PostScript then an error message is left in interp->result,
 *	replacing whatever used to be there.  If no errors occur, then
 *	PostScript output for the item is appended to the interpreter result.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static int
EpsToPostScript(
    Tcl_Interp *interp,			/* Interpreter to hold generated
					 * PostScript or reports errors back
					 * to. */
    Tk_Canvas canvas,			/* Information about overall
					 * canvas. */
    Tk_Item *itemPtr,			/* eps item. */
    int prepass)			/* If 1, this is a prepass to collect
					 * font information; 0 means final *
					 * PostScript is being created. */
{
    Eps *epsPtr = (Eps *)itemPtr;
    Blt_Ps ps;
    double xScale, yScale;
    double x, y, w, h;
    PageSetup setup;

    if (prepass) {
	return TCL_OK;			/* Don't worry about fonts. */
    }
    memset(&setup, 0, sizeof(setup));
    ps = Blt_Ps_Create(interp, &setup);

    /* Lower left corner of item on page. */
    x = epsPtr->bb.left;
    y = Tk_CanvasPsY(canvas, epsPtr->bb.bottom);
    w = epsPtr->bb.right - epsPtr->bb.left;
    h = epsPtr->bb.bottom - epsPtr->bb.top;

    if (epsPtr->fileName == NULL) {
	/* No PostScript file, generate PostScript of resized image instead. */
	if (epsPtr->picture != NULL) {
	    Blt_Ps_Format(ps, "gsave\n");
	    /*
	     * First flip the PostScript y-coordinate axis so that the origin
	     * is the upper-left corner like our picture.
	     */
	    Blt_Ps_Format(ps, "  %g %g translate\n", x, y + h);
	    Blt_Ps_Format(ps, "  1 -1 scale\n");

	    Blt_Ps_DrawPicture(ps, epsPtr->picture, 0.0, 0.0);
	    Blt_Ps_Format(ps, "grestore\n");

	    Blt_Ps_SetInterp(ps, interp);
	    Blt_Ps_Free(ps);
	}
	return TCL_OK;
    }

    /* Copy in the PostScript prolog for EPS encapsulation. */
    if (Blt_Ps_IncludeFile(interp, ps, "bltCanvEps.pro") != TCL_OK) {
	goto error;
    }
    Blt_Ps_Append(ps, "BeginEPSF\n");

    xScale = w / (double)(epsPtr->urx - epsPtr->llx);
    yScale = h / (double)(epsPtr->ury - epsPtr->lly);

    /* Set up scaling and translation transformations for the EPS item */

    Blt_Ps_Format(ps, "%g %g translate\n", x, y);
    Blt_Ps_Format(ps, "%g %g scale\n", xScale, yScale);
    Blt_Ps_Format(ps, "%d %d translate\n", -(epsPtr->llx), -(epsPtr->lly));

    /* FIXME: Why clip against the old bounding box? */
    Blt_Ps_Format(ps, "%d %d %d %d SetClipRegion\n", epsPtr->llx, 
	epsPtr->lly, epsPtr->urx, epsPtr->ury);

    Blt_Ps_VarAppend(ps, "%% including \"", epsPtr->fileName, "\"\n\n",
	 (char *)NULL);

    Blt_Ps_AppendBytes(ps, Tcl_DStringValue(&epsPtr->dString), 
	Tcl_DStringLength(&epsPtr->dString));
    Blt_Ps_Append(ps, "EndEPSF\n");
    Blt_Ps_SetInterp(ps, interp);
    Blt_Ps_Free(ps);
    return TCL_OK;

  error:
    Blt_Ps_Free(ps);
    return TCL_ERROR;
}


/*
 * The structures below defines the EPS item type in terms of procedures that
 * can be invoked by generic item code.
 */
static Tk_ItemType itemType = {
    (char *)"eps",			/* name */
    sizeof(Eps),			/* itemSize */
    CreateEps,				/* createProc */
    configSpecs,			/* configSpecs */
    ConfigureEps,			/* configureProc */
    EpsCoords,				/* coordProc */
    DeleteEps,				/* deleteProc */
    DisplayEps,				/* displayProc */
    0,					/* alwaysRedraw */
    EpsToPoint,				/* pointProc */
    EpsToArea,				/* areaProc */
    EpsToPostScript,			/* postscriptProc */
    ScaleEps,				/* scaleProc */
    TranslateEps,			/* translateProc */
    (Tk_ItemIndexProc *)NULL,		/* indexProc */
    (Tk_ItemCursorProc *)NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *)NULL,	/* selectionProc */
    (Tk_ItemInsertProc *)NULL,		/* insertProc */
    (Tk_ItemDCharsProc *)NULL,		/* dTextProc */
    (Tk_ItemType *)NULL			/* nextPtr */
};

/*ARGSUSED*/
void
Blt_RegisterEpsCanvasItem(void)
{
    Tk_CreateItemType(&itemType);
    /* Initialize custom canvas option routines. */
    tagsOption.parseProc = Tk_CanvasTagsParseProc;
    tagsOption.printProc = Tk_CanvasTagsPrintProc;
}
