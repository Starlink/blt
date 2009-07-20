
/*
 * bltPictGif.c --
 *
 * This module implements GIF file format conversion routines for the picture
 * image type in the BLT toolkit.
 *
 *	Copyright 2003-2005 George A Howlett.
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
 *
 * The GIF reader is from converters/other/giftopnm.c in the netpbm
 * (version 10.19) distribution.
 *
 *	Copyright 1990, 1991, 1993, David Koblas.  (koblas@netcom.com)
 *	Permission to use, copy, modify, and distribute this software and its
 *	documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appear in all copies and that
 *	both that copyright notice and this permission notice appear in
 *	supporting documentation.  This software is provided "as is" without
 *	express or implied warranty.
 *
 * The GIF writer is in part derived from converters/other/pamtogif.c in 
 * the netpbm (version 10.19) distribution.  
 *
 *	Original version, named 'ppmgif' was by Jef Poskanzer in 1989, based
 *	on GIFENCOD by David Rowley <mgardi@watdscu.waterloo.edu>.A Lempel-Zim
 *	compression based on "compress".
 *
 *	Switched to use libnetpbm PAM facilities (ergo process PAM images) and
 *	renamed 'pamtogif' by Bryan Henderson November 2006.
 *
 *
 *	Copyright (C) 1989 by Jef Poskanzer.
 *
 *	Permission to use, copy, modify, and distribute this software and its
 *	documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appear in all copies and that
 *	both that copyright notice and this permission notice appear in
 *	supporting documentation.  This software is provided "as is" without
 *	express or implied warranty.
 *
 *	The Graphics Interchange Format(c) is the Copyright property of
 *	CompuServe Incorporated.  GIF(sm) is a Service Mark property of
 *	CompuServe Incorporated.
 *
 * The compression routines also use the following
 *
 *	GIFCOMPR.C       - GIF Image compression routines
 *
 *	Lempel-Ziv compression based on 'compress'.  GIF modifications by *
 *	David Rowley (mgardi@watdcsu.waterloo.edu)
 *
 *	GIF Image compression - modified 'compress'
 *
 *	Based on: compress.c - File compression ala IEEE Computer, June 1984.
 *
 *	By Authors:  
 *		Spencer W. Thomas       (decvax!harpo!utah-cs!utah-gr!thomas)
 *              Jim McKie               (decvax!mcvax!jim)
 *              Steve Davies            (decvax!vax135!petsd!peora!srd)
 *              Ken Turkowski           (decvax!decwrl!turtlevax!ken)
 *              James A. Woods          (decvax!ihnp4!ames!jaw)
 *              Joe Orost               (decvax!vax135!petsd!joe)
 *
 */

#include "blt.h"
#include "config.h"

#include <ctype.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#endif /* HAVE_STRING_H */

#include <tcl.h>
#include <bltAlloc.h>
#include <bltSwitch.h>
#include <bltDBuffer.h>
#include <bltHash.h>
#include "bltPicture.h"
#include "bltPictFmts.h"

#ifdef _MSC_VER
#define vsnprintf		_vsnprintf
#endif

#include <setjmp.h>

typedef struct _Blt_Picture Pict;

typedef struct {
    Blt_Picture original;
    Pict *srcPtr;
    int delay;
    int nColors;
} Frame;

#define TRUE		1
#define FALSE		0

#define LZW_MAX_BITS	12
#define LZW_MAX_CODE	(1 << LZW_MAX_BITS)

#define GIF87A		1
#define GIF89A		2

typedef struct {
    Tcl_Obj *dataObjPtr;
    Tcl_Obj *fileObjPtr;
} GifImportSwitches;

static Blt_SwitchSpec importSwitches[] = 
{
    {BLT_SWITCH_OBJ, "-data",  "data",
	Blt_Offset(GifImportSwitches, dataObjPtr), 0},
    {BLT_SWITCH_OBJ, "-file",  "fileName",
	Blt_Offset(GifImportSwitches, fileObjPtr), 0},
    {BLT_SWITCH_END}
};

typedef struct {
    Tcl_Obj *dataObjPtr;
    Tcl_Obj *fileObjPtr;
    Blt_Pixel bg;
    const char **comments;		/* Comments */
    unsigned int flags;
    int index;
} GifExportSwitches;

BLT_EXTERN Blt_SwitchParseProc Blt_ColorSwitchProc;
static Blt_SwitchCustom colorSwitch = {
    Blt_ColorSwitchProc, NULL, (ClientData)0,
};

#define EXPORT_BLEND	(1<<0)

static Blt_SwitchSpec exportSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-bg", "color",
	Blt_Offset(GifExportSwitches, bg),         0, 0, &colorSwitch},
    {BLT_SWITCH_BITMASK, "-blend", "",
	Blt_Offset(GifExportSwitches, flags),	   0, EXPORT_BLEND},
    {BLT_SWITCH_OBJ, "-data",  "data",
	Blt_Offset(GifExportSwitches, dataObjPtr), 0},
    {BLT_SWITCH_OBJ, "-file",  "fileName",
	Blt_Offset(GifExportSwitches, fileObjPtr), 0},
    {BLT_SWITCH_LIST, "-comments", "{string...}",
	Blt_Offset(GifExportSwitches, comments),   0},
    {BLT_SWITCH_INT_NNEG, "-index", "int",
	Blt_Offset(GifExportSwitches, index), 0},
    {BLT_SWITCH_END}
};

typedef struct {
    jmp_buf jmpbuf;
    Tcl_DString errors;
    Tcl_DString warnings;
    int nWarnings, nErrors;
} GifMessage;

typedef struct {
    int version;		/* Eithe GIF87A or GIF89A */
    Blt_Pixel globalColorTable[256];
    Blt_Pixel localColorTable[256];

    /* Data to be parsed */
    Blt_DBuffer dbuffer;

    /* Global parameters. */
    int logicalScreenWidth, logicalScreenHeight;
    int globalColorTableFlag;
    int colorResolution;
    int sortFlag;
    unsigned int globalColorTableSize;
    int bgColorIndex;
    int pixelAspectRatio;

    /* Local parameters. */
    int imageLeftPosition, imageTopPosition;
    int imageWidth, imageHeight;
    int localColorTableFlag;
    int interlaceFlag;
    int imageSortFlag;
    unsigned int localColorTableSize;

    /* Graphic control extension. */

    int disposalMethod;
    int userInputFlag;
    int transparentColorFlag;
    int delayTime;
    int transparentColorIndex;

    int lzwMinimumCodeSize;

} Gif;

static GifMessage *gifMessagePtr;

DLLEXPORT extern Tcl_AppInitProc Blt_PictureGifInit;

static unsigned short
GifGetShort(unsigned char *buf)
{
#ifdef WORDS_BIGENDIAN
    return (buf[0] << 8) | buf[1];
#else
    return buf[0] | (buf[1] << 8);
#endif
}

static unsigned char *
GifSetShort(unsigned char *buf, unsigned int value)
{
#ifdef WORDS_BIGENDIAN
    buf[0] = (value >> 8)  & 0xFF;
    buf[1] = (value)       & 0xFF;
#else
    buf[0] = (value)       & 0xFF;
    buf[1] = (value >> 8)  & 0xFF;
#endif
    return buf + 2;
}

/*ARGSUSED*/
static void
GifError TCL_VARARGS_DEF(const char *, arg1)
{
    const char *fmt;
    char string[BUFSIZ+4];
    int length;
    va_list args;

    fmt = TCL_VARARGS_START(const char *, arg1, args);
    length = vsnprintf(string, BUFSIZ, fmt, args);
    if (length > BUFSIZ) {
	strcat(string, "...");
    }
    Tcl_DStringAppend(&gifMessagePtr->errors, string, -1);
    va_end(args);
    longjmp(gifMessagePtr->jmpbuf, 0);
}

/*ARGSUSED*/
static void
GifWarning TCL_VARARGS_DEF(const char *, arg1)
{
    const char *fmt;
    char string[BUFSIZ+4];
    int length;
    va_list args;

    fmt = TCL_VARARGS_START(const char *, arg1, args);
    length = vsnprintf(string, BUFSIZ, fmt, args);
    if (length > BUFSIZ) {
	strcat(string, "...");
    }
    Tcl_DStringAppend(&gifMessagePtr->warnings, string, -1);
    va_end(args);
    gifMessagePtr->nWarnings++;
}

/*
 *     0  3	Signature	"GIF" 
 *     3  3     Version		"89a"
 */
static int
GifHeader(Blt_DBuffer dbuffer, Gif *gifPtr)
{
    unsigned char *bp;

    Blt_DBuffer_ResetCursor(dbuffer);
    if (Blt_DBuffer_BytesLeft(dbuffer) < 6) {
	return FALSE;
    }
    bp = Blt_DBuffer_Pointer(dbuffer);
    if ((bp[0] != 'G') || (bp[1] != 'I') || (bp[2] != 'F') || (bp[3] != '8') ||
	(bp[5] != 'a')) {
	return FALSE;
    }
    if (bp[4] == '7') {
	gifPtr->version = GIF87A;
    } else if (bp[4] == '9') {
	gifPtr->version = GIF89A;
    }
    Blt_DBuffer_IncrCursor(dbuffer, 6);
    return TRUE;
}

/*
 *     0  2	Logical Screen Width
 *     2  2     Logical Screen Height 
 *     4  1     Flags 
 *		bit 7    = Global color table flag.
 *		bits 4-6 = Color resolution.
 *		bit 3    = Sort flag.
 *		bits 0-2 = Size of global color table.
 *     5  1     Background Color Index
 *     6  1     Pixel Aspect Ratio
 */
static void
GifReadLogicalScreenDescriptor(Blt_DBuffer dbuffer, Gif *gifPtr)
{
    unsigned char *bp;

    if (Blt_DBuffer_BytesLeft(dbuffer) <  7) {
	GifError("short file: expected 7 bytes for GIF logical screen descriptor");
    }
    bp = Blt_DBuffer_Pointer(dbuffer);
    /* Logical Screen Descriptor. */
    gifPtr->logicalScreenWidth =  GifGetShort(bp + 0);
    gifPtr->logicalScreenHeight = GifGetShort(bp + 2);
    gifPtr->globalColorTableFlag = (bp[4] & 0x80);
    gifPtr->colorResolution =	((bp[4] & 0x70) >> 4);
    gifPtr->sortFlag = (bp[4] & 0x08) >> 3;
    gifPtr->globalColorTableSize = 1 << ((bp[4] & 0x07) + 1);
    gifPtr->bgColorIndex = bp[5];	/* Index of bgcolor color */
    gifPtr->pixelAspectRatio = bp[6];
    Blt_DBuffer_IncrCursor(dbuffer, 7);
#ifdef notdef
    fprintf(stderr, "screen width = %d\n", gifPtr->logicalScreenWidth);
    fprintf(stderr, "screen height = %d\n", gifPtr->logicalScreenHeight);
    fprintf(stderr, "global color table flag = %d\n", gifPtr->globalColorTableFlag);
    fprintf(stderr, "color resolution = %d\n", gifPtr->colorResolution);
    fprintf(stderr, "sort flag = %d\n", gifPtr->sortFlag);
    fprintf(stderr, "global color table size = %d\n", gifPtr->globalColorTableSize);
    fprintf(stderr, "bg color index = %d\n", gifPtr->bgColorIndex);
    fprintf(stderr, "pixel aspect ratio = %d\n", gifPtr->pixelAspectRatio);
#endif
}

/*
 *        7 6 5 4 3 2 1 0        Field Name                    Type 
 *      +================+ 
 *   0  |                |       Red 0                         Byte 
 *   1  |                |       Green 0                       Byte 
 *   2  |                |       Blue 0                        Byte 
 *   3  |                |       Red 1                         Byte 
 *      |                |       Green 1                       Byte 
 *  up  |                | 
 *  to  |                | 
 *      |                |       Green 255                     Byte 
 * 767  |                |       Blue 255                      Byte 
 *      +================+ 
 */
static void
GifReadGlobalColorTable(Blt_DBuffer dbuffer, Gif *gifPtr)
{
    Blt_Pixel *dp;
    unsigned int i;
    unsigned char *bp;

    if (Blt_DBuffer_BytesLeft(dbuffer) < (gifPtr->globalColorTableSize*3)) {
	GifError("short file: expected %d bytes for GIF global color table",
		 gifPtr->globalColorTableSize * 3);
    }
    bp = Blt_DBuffer_Pointer(dbuffer);
    dp = gifPtr->globalColorTable;
    for (i = 0; i < gifPtr->globalColorTableSize; i++) {
	dp->Red = bp[0];
	dp->Green = bp[1];
	dp->Blue = bp[2];
	dp->Alpha = ALPHA_OPAQUE;
	dp++;
	bp += 3;
    }	    
    Blt_DBuffer_SetPointer(dbuffer, bp);
}

/*
 *       7 6 5 4 3 2 1 0        Field Name                    Type
 *     +----------------+
 *  0  |                |       Image Separator               Byte
 *     +----------------+
 *  1  |                |       Image Left Position           Unsigned
 *  2  |                |
 *     +----------------+
 *  3  |                |       Image Top Position            Unsigned
 *  4  |                |
 *     +----------------+
 *  5  |                |       Image Width                   Unsigned
 *  6  |                |
 *     +----------------+
 *  7  |                |       Image Height                  Unsigned
 *  8  |                |
 *     +----------------+
 *  9  | | | |   |      |	See below
 *     +----------------+
 *
 *			=	Local Color Table Flag        1 Bit
 *				Interlace Flag                1 Bit
 *				Sort Flag                     1 Bit
 *				Reserved                      2 Bits
 *				Size of Local Color Table     3 Bits
 */
static void
GifReadImageDescriptor(Blt_DBuffer dbuffer, Gif *gifPtr)
{
    unsigned char *bp;
    int size;

    if (Blt_DBuffer_BytesLeft(dbuffer) < 9) {
	GifError("short file: expected 9 bytes for GIF image descriptor");
    }
    bp = Blt_DBuffer_Pointer(dbuffer);
    gifPtr->imageLeftPosition =   GifGetShort(bp + 0);
    gifPtr->imageTopPosition =    GifGetShort(bp + 2);
    gifPtr->imageWidth =          GifGetShort(bp + 4);
    gifPtr->imageHeight =         GifGetShort(bp + 6);
    gifPtr->localColorTableFlag = (bp[8] & 0x80) >> 7;
    gifPtr->interlaceFlag =       (bp[8] & 0x40) >> 6;
    gifPtr->imageSortFlag =       (bp[8] & 0x20) >> 5;
    size =                        (bp[8] & 0x07);
    if (size > 0) {
        size = 1 << (size + 1);
    }
    gifPtr->localColorTableSize = size;
    Blt_DBuffer_IncrCursor(dbuffer, 9);
}

/*
 *        7 6 5 4 3 2 1 0        Field Name                    Type 
 *      +================+ 
 *   0  |                |       Red 0                         Byte 
 *   1  |                |       Green 0                       Byte 
 *   2  |                |       Blue 0                        Byte 
 *   3  |                |       Red 1                         Byte 
 *      |                |       Green 1                       Byte 
 *  up  |                | 
 *  to  |                | 
 *      |                |       Green 255                     Byte 
 * 767  |                |       Blue 255                      Byte 
 *      +================+ 
 */
static void
GifReadLocalColorTable(Blt_DBuffer dbuffer, Gif *gifPtr)
{
    unsigned char *bp;
    Blt_Pixel *dp;
    unsigned int i;

    if (Blt_DBuffer_BytesLeft(dbuffer) < (gifPtr->localColorTableSize*3)) {
	GifError("short file: expected %d bytes for GIF local color table",
		 gifPtr->localColorTableSize * 3);
    }
    bp = Blt_DBuffer_Pointer(dbuffer);
    dp = gifPtr->localColorTable;
    for (i = 0; i < gifPtr->localColorTableSize; i++) {
	dp->Red = bp[0];
	dp->Green = bp[1];
	dp->Blue = bp[2];
	dp->Alpha = ALPHA_OPAQUE;
	dp++;
	bp += 3;
    }	    
    Blt_DBuffer_SetPointer(dbuffer, bp);
}


/*
 *        7 6 5 4 3 2 1 0        Field Name                    Type 
 *      +----------------+ 
 *   0  |                |       Extension Introducer          Byte 
 *      +----------------+ 
 *   1  |                |       Graphic Control Label         Byte 
 *      +----------------+ 
 *
 *      +----------------+ 
 *   0  |                |       Block Size                    Byte 
 *      +----------------+ 
 *   1  |      |     | | |                      See below 
 *      +----------------+ 
 *   2  |                |       Delay Time                    Unsigned 
 *   3  |                | 
 *      +----------------+ 
 *   4  |                |       Transparent Color Index       Byte 
 *      +----------------+ 
 *
 *      +----------------+ 
 *   0  |                |       Block Terminator              Byte 
 *      +----------------+ 
 *
 *
 *      		  =     Reserved                      3 Bits 
 *                              Disposal Method               3 Bits 
 *                              User Input Flag               1 Bit 
 *                              Transparent Color Flag        1 Bit 
 */
static void
GifReadGraphicControlExtension(Blt_DBuffer dbuffer, Gif *gifPtr)
{
    unsigned int length;

    
    while ((length = Blt_DBuffer_NextByte(dbuffer)) > 0) {
	unsigned char *bp;

	bp = Blt_DBuffer_Pointer(dbuffer);
	gifPtr->disposalMethod = (bp[0] & 0x1C) >> 2;
	gifPtr->userInputFlag = bp[0] & 0x02;
	gifPtr->transparentColorFlag = bp[0] & 0x01;
	gifPtr->delayTime = GifGetShort(bp + 1);
	gifPtr->transparentColorIndex = bp[3];
	Blt_DBuffer_IncrCursor(dbuffer, length);
    }
}

/*
 * Optional comment block
 */
static void 
GifReadCommentExtension(Blt_DBuffer dbuffer, Gif *gifPtr)
{
    unsigned char length;

    while ((length = Blt_DBuffer_NextByte(dbuffer)) > 0) {
	unsigned char *bp;

	if (Blt_DBuffer_BytesLeft(dbuffer) < length) {
	    GifError("short file: expected %d bytes for GIF comment block", 
		     length);
	}
	bp = Blt_DBuffer_Pointer(dbuffer);
#ifdef notdef
	{
	int i;
	fprintf(stderr, "comment %d bytes follows\n", length);
	for (i = 0; i < length; i++) {
	    if (isprint(bp[i]) || isspace(bp[i])) {
		fprintf(stderr, "%c", bp[i]);
	    } else {
		fprintf(stderr, "{%.2x}", bp[i]);
	    }
	}
	fprintf(stderr, "\n");
	}
#endif
	Blt_DBuffer_IncrCursor(dbuffer, length);
    }
    assert(length == 0);
}

static void
GifReadUnknownExtension(Blt_DBuffer dbuffer, int ident, Gif *gifPtr)
{
    unsigned char length;
    const char *ext;
    
    if (ident == 0x01) {
	ext = "plain text";
    } else if (ident == 0xFF) {
	ext = "application";
    } else {
	ext = "???";
    }
#ifdef notdef
    fprintf(stderr, "read %s extension\n", ext);
#endif
    /* Skip the data sub-blocks */
    while ((length = Blt_DBuffer_NextByte(dbuffer)) > 0) {
	if (Blt_DBuffer_BytesLeft(dbuffer) < length) {
	    GifError("short file: expected %d bytes for %s extension block",
		     length, ext);
	}
	Blt_DBuffer_IncrCursor(dbuffer, length);
    }
#ifdef notdef
    fprintf(stderr, "down reading %s extension\n", ext);
#endif
}

static void
GifReadExtensions(Blt_DBuffer dbuffer, Gif *gifPtr)
{
    unsigned char byte;
    /* 
     * Handle only the "Graphic control extension" block, ignoring all others
     * (text, comment, and application).
     */
    byte = Blt_DBuffer_NextByte(dbuffer);
    switch (byte) {
    case 0xF9:			/* Graphic control extension. */
	GifReadGraphicControlExtension(dbuffer, gifPtr);
	break;

    case 0xFE:			/* Comment extension. */
	GifReadCommentExtension(dbuffer, gifPtr);
	break;

    default:
    case 0x01:			/* Plain text extension. */
    case 0xFF:			/* Application extension */
	GifReadUnknownExtension(dbuffer, byte, gifPtr);
	break;
    }
#ifdef notdef
    assert(byte == 0);
    byte = Blt_DBuffer_NextByte(dbuffer);
#endif
}

static int
GifReadDataBlock(Blt_DBuffer dbuffer, unsigned char *out) 
{
    unsigned int length;

    length = Blt_DBuffer_NextByte(dbuffer);
    if (Blt_DBuffer_BytesLeft(dbuffer) < length) {
	return -1;
    } 
    if (length > 0) {
	memcpy(out, Blt_DBuffer_Pointer(dbuffer), length);
	Blt_DBuffer_IncrCursor(dbuffer, length);
    }
    return (int)length;
}


/* Stack grows from low addresses to high addresses */
typedef struct {
    int *stack;			/* Malloc'ed array */
    int *sp;			/* Stack pointer */
    int *top;			/* Next word above top of stack */
} LzwStack;

typedef struct {
    unsigned char buf[280];	/* This is the buffer through which we read
				 * the data from the stream.  We must buffer
				 * it because we have to read whole data
				 * blocks at a time, but our client wants one
				 * code at a time.  The buffer typically
				 * contains the contents of one data block
				 * plus two bytes from the previous data
				 * block. */
    int bufCount;		/* # of bytes of contents in buf[]. */
    int curbit;			/* The bit number (starting at 0) within buf[]
				 * of the next bit to be returned.  If the
				 * next bit to be returned is not yet in buf[]
				 * (we've already returned everything in
				 * there), this points one beyond the end of
				 * the buffer contents.
				 */
    int streamExhausted;	/* The last time we read from the input
				 * stream, we got an EOD marker or EOF */
} LzwCodeState;


/*-------------------------------------------------------------------------------
   Some notes on LZW.

   LZW is an extension of Limpel-Ziv.  The two extensions are:

     1) in Limpel-Ziv, codes are all the same number of bits.  In
        LZW, they start out small and increase as the stream progresses.

     2) LZW has a clear code that resets the string table and code
        size.

   The LZW code space is allocated as follows:

   The true data elements are dataWidth bits wide, so the maximum value of a
   true data element is 2**dataWidth-1.  We call that max_dataVal.  The first
   byte in the stream tells you what dataWidth is.

   LZW codes 0 - max_dataVal are direct codes.  Each on represents the true
   data element whose value is that of the LZW code itself.  No decompression
   is required.

   max_dataVal + 1 and up are compression codes.  They encode true data
   elements:

   max_dataVal + 1 is the clear code.
         
   max_dataVal + 2 is the end code.

   max_dataVal + 3 and up are string codes.  Each string code represents a
   string of true data elements.  The translation from a string code to the
   string of true data elements varies as the stream progresses.  In the
   beginning and after every clear code, the translation table is empty, so no
   string codes are valid.  As the stream progresses, the table gets filled
   and more string codes become valid.

--------------------------------------------------------------------------*/

typedef struct {
    LzwStack stack;
    int fresh;			/* The stream is right after a clear code or
				   at the very beginning */
    int codeSize;		/* Current code size -- each LZW code in this
				 * part of the image is this many bits.  Ergo,
				 * we read this many bits at a time from the
				 * stream. */

    int maxnum_code;		/* Maximum number of LZW codes that can be
				 * represented with the current code size. (1
				 * << codeSize) */

    int next_tableSlot;		/* Index in the code translation table of the
				 * next free entry */
    int firstcode;		/* This is always a true data element code */
    int prevcode;		/* The code just before, in the image, the one
				 * we're processing now */

    int table[2][(1 << LZW_MAX_BITS)];

    /* The following are constant for the life of the decompressor */
    int init_codeSize;
    int max_dataVal;
    int clear_code;
    int end_code; 

    Blt_DBuffer dbuffer;
} LzwDecompressor;

static int zeroDataBlock = 0;

static void
LzwInitCodeState(LzwCodeState *statePtr) 
{
    /* Fake a previous data block */
    statePtr->buf[0] = statePtr->buf[1] = 0;
    statePtr->bufCount = 2;
    statePtr->curbit = statePtr->bufCount * 8;
    statePtr->streamExhausted = 0;
}

static void
LzwGetAnotherBlock(Blt_DBuffer dbuffer, LzwCodeState *statePtr) 
{
    int count;
    unsigned int assumed_count;

    /* 
     * Shift buffer down so last two bytes are now the first two bytes.  Shift
     * 'curbit' cursor, which must be somewhere in or immediately after those
     * two bytes, accordingly.
     */
    statePtr->buf[0] = statePtr->buf[statePtr->bufCount-2];
    statePtr->buf[1] = statePtr->buf[statePtr->bufCount-1];

    statePtr->curbit -= (statePtr->bufCount-2)*8;
    statePtr->bufCount = 2;
        
    /* Add the next block to the buffer */
    count = GifReadDataBlock(dbuffer, statePtr->buf  + statePtr->bufCount);
    if (count == -1) {
        GifWarning("EOF encountered in image before EOD marker.  The GIF file is malformed, but we are proceeding anyway as if an EOD marker were at the end of the file.");
        assumed_count = 0;
    } else {
        assumed_count = count;
    }
    statePtr->streamExhausted = (assumed_count == 0);
    statePtr->bufCount += assumed_count;
}

static int
LzwNextCode(Blt_DBuffer dbuffer, int codeSize, LzwCodeState *statePtr) 
{
    int result;

    if (((statePtr->curbit + codeSize) > (statePtr->bufCount * 8)) && 
	(!statePtr->streamExhausted)) {
        /* 
	 * Not enough left in buffer to satisfy request.  Get the next
	 * data block into the buffer.
	 */
        LzwGetAnotherBlock(dbuffer, statePtr);
    }
    if ((statePtr->curbit + codeSize) > (statePtr->bufCount * 8)) {
        /* 
	 * If the buffer still doesn't have enough bits in it, that means
	 * there were no data blocks left to read.
	 */
	result = -1;  /* EOF */

        {
            int const bitsUnused = (statePtr->bufCount * 8) - statePtr->curbit;
            if (bitsUnused > 0) {
                GifWarning("Stream ends with a partial code (%d bits left in file; expected a %d bit code). Ignoring.", bitsUnused, codeSize);
	    }
        }
    } else {
        int i, j;
        int code;
        unsigned char *bp;

        bp = statePtr->buf;
        code = 0;		/* Initial value */
        for (i = statePtr->curbit, j = 0; j < codeSize; ++i, ++j) {
            code |= ((bp[ i / 8 ] & (1 << (i % 8))) != 0) << j;
	}
        statePtr->curbit += codeSize;
        result = code;
    }
    return result;
}

/*---------------------------------------------------------------------------
 * 
 * LzwGetCode --
 *
 *	If 'init', initialize the code getter.  Otherwise, read and return the
 *	next LZW code from the buffer.
 *
 * Results:
 *	Retunes the retrieved code.  Otherwise -1 if we encounter the
 *	end of the file.
 *
 *---------------------------------------------------------------------------
 */
static int
LzwGetCode(
    Blt_DBuffer dbuffer,	/* Buffer to read from. */
    int codeSize,		/* # of bits in the code we are to get. */
    int init)			/* Indicates to initial the code reader. */
{
    static LzwCodeState state;

    if (init) {
        LzwInitCodeState(&state);
        return 0;
    } 
    return LzwNextCode(dbuffer, codeSize, &state);
}

static void 
LzwInitStack(
    LzwDecompressor *lzwPtr, 
    size_t size) 
{
    lzwPtr->stack.stack = Blt_Malloc(size * sizeof(int));
    if (lzwPtr->stack.stack == NULL) {
        GifError("Unable to allocate %d -word stack.", size);
    }
    lzwPtr->stack.sp = lzwPtr->stack.stack;
    lzwPtr->stack.top = lzwPtr->stack.stack + size;
}

static void
LzwPushStack(LzwDecompressor *lzwPtr, int const value) 
{
    if (lzwPtr->stack.sp >= lzwPtr->stack.top) {
        GifError("stack overflow");
    }
    *(lzwPtr->stack.sp++) = value;
}

static int
LzwStackIsEmpty(LzwDecompressor *lzwPtr) 
{
    return (lzwPtr->stack.sp == lzwPtr->stack.stack);
}

static int
LzwPopStack(LzwDecompressor *lzwPtr) 
{
    if (lzwPtr->stack.sp <= lzwPtr->stack.stack) {
        GifError("stack underflow");
    }
    return *(--lzwPtr->stack.sp);
}

static void
LzwTermStack(LzwDecompressor *lzwPtr) 
{
    Blt_Free(lzwPtr->stack.stack);
    lzwPtr->stack.stack = NULL;
}

static void
LzwResetDecompressor(LzwDecompressor *lzwPtr) 
{
    lzwPtr->codeSize = lzwPtr->init_codeSize + 1;
    lzwPtr->maxnum_code = 1 << lzwPtr->codeSize;
    lzwPtr->next_tableSlot = lzwPtr->max_dataVal + 3;
    lzwPtr->fresh = 1;
}

static void
LzwInitDecompressor(Blt_DBuffer dbuffer, int codeSize, LzwDecompressor *lzwPtr) 
{
    lzwPtr->init_codeSize = codeSize;

    lzwPtr->max_dataVal = (1 << codeSize) - 1;
    lzwPtr->clear_code = lzwPtr->max_dataVal + 1;
    lzwPtr->end_code = lzwPtr->max_dataVal + 2;
    
    /* 
     * The entries in the translation table for true data codes are constant
     * throughout the stream.  We set them now and they never change.
     */
    {
        int i;

        for (i = 0; i <= lzwPtr->max_dataVal; ++i) {
            lzwPtr->table[0][i] = 0;
            lzwPtr->table[1][i] = i;
        }
    }
    LzwResetDecompressor(lzwPtr);
    LzwGetCode(dbuffer, 0, TRUE);
    lzwPtr->dbuffer = dbuffer;
    lzwPtr->fresh = TRUE;
    LzwInitStack(lzwPtr, LZW_MAX_CODE * 2);
}

static void
LzwTermDecompressor(LzwDecompressor *lzwPtr) 
{
    LzwTermStack(lzwPtr);
}

/*
 *---------------------------------------------------------------------------
 * 
 * LzwExpandCodeOntoStack --
 *
 *	'incode' is an LZW string code.  It represents a string of true data
 *	elements, as defined by the string translation table in *decompP.
 *
 *	Expand the code to a string of LZW direct codes and push them onto the
 *	stack such that the leftmost code is on top.
 *
 *	Also add to the translation table where appropriate.
 *
 * Results:
 *	If successful, return TRUE. Iff the translation table contains a cycle
 *	(which means the LZW stream from which it was built is invalid),
 *	return FALSE.
 *	
 *---------------------------------------------------------------------------
 */
static int
LzwExpandCodeOntoStack(LzwDecompressor *lzwPtr, int incode) 
{
    int code;
    int error;

    error = FALSE;
    if (incode < lzwPtr->next_tableSlot) {
        code = incode;
    } else {
        /* It's a code that isn't in our translation table yet */
        LzwPushStack(lzwPtr, lzwPtr->firstcode);
        code = lzwPtr->prevcode;
    }

    {
        /* 
	 * Get the whole string that this compression code represents and push
	 * it onto the code stack so the leftmost code is on top.  Set
	 * lzwPtr->firstcode to the first (leftmost) code in that string.
	 */
        unsigned int stringCount;
        stringCount = 0;

        while ((code > lzwPtr->max_dataVal) && (!error)) {
            if (stringCount > LZW_MAX_CODE) {
                GifError("contains LZW string loop");
            } 
	    ++stringCount;
	    LzwPushStack(lzwPtr, lzwPtr->table[1][code]);
	    code = lzwPtr->table[0][code];
        }
        lzwPtr->firstcode = lzwPtr->table[1][code];
        LzwPushStack(lzwPtr, lzwPtr->firstcode);
    }

    if (lzwPtr->next_tableSlot < LZW_MAX_CODE) {

        lzwPtr->table[0][lzwPtr->next_tableSlot] = lzwPtr->prevcode;
        lzwPtr->table[1][lzwPtr->next_tableSlot] = lzwPtr->firstcode;
        ++lzwPtr->next_tableSlot;

        if (lzwPtr->next_tableSlot >= lzwPtr->maxnum_code) {
            /* 
	     * We've used up all the codes of the current code size.  Future
	     * codes in the stream will have codes one bit longer.  But
	     * there's an exception if we're already at the LZW maximum, in
	     * which case the codes will simply continue the same size.
	     */
            if (lzwPtr->codeSize < LZW_MAX_BITS) {
                ++lzwPtr->codeSize;
                lzwPtr->maxnum_code = 1 << lzwPtr->codeSize;
            }
        }
    }
    lzwPtr->prevcode = incode;
    return error == 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * LzwReadByte --
 *
 *	Returns the next data element of the decompressed image.  In the
 *	context of a GIF, a data element is the color table index of one
 *	pixel.
 *
 *	We read and return the next byte of the decompressed image:
 *
 * Results:
 *	Returns -1, if we hit EOF prematurely (i.e. before an "end" code.  We
 *	forgive the case that the "end" code is followed by EOF instead of an
 *	EOD marker (zero length DataBlock)).
 *
 *	Returns -2 if there are no more bytes in the image.
 *
 *	Returns -3 if we encounter errors in the LZW stream. 
 *
 *---------------------------------------------------------------------------
 */
static int
LzwReadByte(LzwDecompressor *lzwPtr) 
{
    int code;

    if (!LzwStackIsEmpty(lzwPtr)) {
        return LzwPopStack(lzwPtr);
    }
    if (lzwPtr->fresh) {
        lzwPtr->fresh = FALSE;
        /* 
	 * Read off all initial clear codes, read the first non-clear code,
	 * and return it.  There are no strings in the table yet, so the next
	 * code must be a direct true data code.
	 */
        do {
            lzwPtr->firstcode = LzwGetCode(lzwPtr->dbuffer, lzwPtr->codeSize, 
		FALSE);
            lzwPtr->prevcode = lzwPtr->firstcode; 
	} while (lzwPtr->firstcode == lzwPtr->clear_code);

        return lzwPtr->firstcode;
    } 
    code = LzwGetCode(lzwPtr->dbuffer, lzwPtr->codeSize, FALSE);
    if (code < 0) {
	return code;
    } 
    if (code == lzwPtr->clear_code) {
	LzwResetDecompressor(lzwPtr);
	return LzwReadByte(lzwPtr);
    } 
    if (code == lzwPtr->end_code) {
	if (!zeroDataBlock) {
	    /* readThroughEod(lzwPtr->ifP); */
	}
	return -2;
    } 
    if (!LzwExpandCodeOntoStack(lzwPtr, code)) {
	return -3;
    }
    return LzwPopStack(lzwPtr);
}

static Blt_Picture
GifCreatePictureFromData(Blt_DBuffer dbuffer, Gif *gifPtr) 
{
    Pict *destPtr;
    Blt_Pixel *colormap;
    LzwDecompressor lzw;
    int v;
    int xOffset, yOffset;

    colormap = (gifPtr->localColorTableFlag) ? gifPtr->localColorTable :
	gifPtr->globalColorTable;
    LzwInitDecompressor(dbuffer, gifPtr->lzwMinimumCodeSize, &lzw);

    destPtr = Blt_CreatePicture(gifPtr->logicalScreenWidth, 
				gifPtr->logicalScreenHeight);
    if (!gifPtr->transparentColorFlag) {
	gifPtr->transparentColorIndex = -1;
    } 
    xOffset = gifPtr->imageLeftPosition;
    yOffset = gifPtr->imageTopPosition;
    if ((xOffset > 0) || (yOffset > 0) || (gifPtr->transparentColorFlag)) {
	Blt_Pixel color;

	color.u32 = 0x00000000;
	Blt_BlankPicture(destPtr, &color);
    }
    destPtr->delay = gifPtr->delayTime * 10;  /* Convert to milliseconds. */
    if (destPtr->delay == 0) {
	destPtr->delay = 100;
    }
    if (gifPtr->interlaceFlag) {
	static int istart[] = { 0, 4, 2, 1 };
	static int istep[] = { 8, 8, 4, 2 };
	int x, y, pass;
	
	pass = 0;
	x = y = 0;
	while ((v = LzwReadByte(&lzw)) >= 0) {
	    Blt_Pixel *dp;
	    
	    dp = Blt_PicturePixel(destPtr, x + xOffset, y + yOffset);
	    dp->u32 = colormap[v].u32;
	    if (v == gifPtr->transparentColorIndex) {
		dp->Alpha = 0x0;
		destPtr->flags |= BLT_PIC_MASK;
	    }
	    ++x;
	    if (x == gifPtr->imageWidth) {
		x = 0;
		y += istep[pass]; /* 8, 8, 4, 2 */
		if (y >= gifPtr->imageHeight) {
		    ++pass;
		    y = istart[pass]; /* 0, 4, 2, 1 */
		    if (y == 0) {
			goto done;
		    }
		}
	    }
	    if (y >= gifPtr->imageHeight) {
		break;
	    }
	}
    } else {
	Blt_Pixel *destRowPtr;
	int y;

	v = 0;			/* Suppress compiler warning. */
	destRowPtr = destPtr->bits + (yOffset*destPtr->pixelsPerRow) + xOffset;
	for (y = 0; y < gifPtr->imageHeight; y++) {
	    Blt_Pixel *dp, *dend;

	    for (dp = destRowPtr, dend = dp + gifPtr->imageWidth; dp < dend; 
		 dp++) {
		v = LzwReadByte(&lzw);
		if (v < 0) {
		    goto done;
		}
		dp->u32 = colormap[v].u32;
		if (v == gifPtr->transparentColorIndex) { 
		    dp->Alpha = 0x0;
		    destPtr->flags |= BLT_PIC_MASK;
		}
	    }
	    destRowPtr += destPtr->pixelsPerRow;
	}
    }
 done:
    if (v == -3) {
        GifError("Error in GIF input stream");
    } 
    if (LzwReadByte(&lzw) >= 0) {
	GifWarning("too much input data, ignoring extra...");
    }
    LzwTermDecompressor(&lzw);
    return destPtr;
}

static Blt_Picture 
GifImageDescriptor(Blt_DBuffer dbuffer, Gif *gifPtr)
{
    Blt_Picture picture;

    GifReadImageDescriptor(dbuffer, gifPtr);
    if (gifPtr->localColorTableFlag) {
	GifReadLocalColorTable(dbuffer, gifPtr);
    }
    /* Get LZW minimum code size. */
    gifPtr->lzwMinimumCodeSize = Blt_DBuffer_NextByte(dbuffer);
    /* Get Image data */
    picture = GifCreatePictureFromData(dbuffer, gifPtr);

    /* The image block may or may not end in a 0 byte. Skip it if it
    * exists. */
    if (*Blt_DBuffer_Pointer(dbuffer) == 0) {
	Blt_DBuffer_IncrCursor(dbuffer, 1);
    }
    return picture;
}

/*
 *---------------------------------------------------------------------------
 *
 * GifToPictures --
 *
 *      Reads a GIF file and converts it into a one or more pictures
 *	returned in a list.
 *
 * Results:
 *      The picture is returned.  If an error occured, such as the
 *      designated file could not be opened, NULL is returned.
 *
 *---------------------------------------------------------------------------
 */
static Blt_Chain 
GifToPictures(Tcl_Interp *interp, const char *fileName, Blt_DBuffer dbuffer,
	      GifImportSwitches *switchesPtr)
{
    Blt_Chain chain;
    Blt_Picture composite, next;
    Gif gif;
    GifMessage message;
    
    gifMessagePtr = &message;
    memset(&gif, 0, sizeof(gif)); /* Clear the structure. */
    message.nWarnings = 0;
    Tcl_DStringInit(&message.errors);
    Tcl_DStringInit(&message.warnings);
    Tcl_DStringAppend(&message.errors, "error reading \"", -1);
    Tcl_DStringAppend(&message.errors, fileName, -1);
    Tcl_DStringAppend(&message.errors, "\": ", -1);

    if (setjmp(message.jmpbuf)) {
	Tcl_DStringResult(interp, &message.errors);
	Tcl_DStringFree(&message.warnings);
	return NULL;
    }
    if (!GifHeader(dbuffer, &gif)) {
	GifError("bad GIF header");
    }
    GifReadLogicalScreenDescriptor(dbuffer, &gif);
    if (gif.globalColorTableFlag) {
	GifReadGlobalColorTable(dbuffer, &gif);
    }
    chain = Blt_Chain_Create();
    next = composite = NULL;
    while (Blt_DBuffer_Cursor(dbuffer) < Blt_DBuffer_Length(dbuffer)) {
	int byte;
	Pict *srcPtr;

	byte = Blt_DBuffer_NextByte(dbuffer);
	switch (byte) {
	case '!':		/* GIF extension */
	    GifReadExtensions(dbuffer, &gif);
	    break;
	case ',':		/* Image descriptor. */
	    srcPtr = GifImageDescriptor(dbuffer, &gif);
	    if (composite == NULL) {
		next = Blt_ClonePicture(srcPtr);
	    } else {
		switch (gif.disposalMethod) {
		case 0:		/* Do nothing, blend with background */
		case 1:	{	/* Blend with background */
		    int delay;
		    
		    delay = srcPtr->delay;
		    /* Blend in the next frame into the current composite. */
		    Blt_BlendPictures(composite, srcPtr, 0, 0, 
				      srcPtr->width, srcPtr->height, 0, 0);
		    Blt_FreePicture(srcPtr);
		    srcPtr = composite;
		    /* Save a copy of the new composite for the next frame. */
		    next = Blt_ClonePicture(composite);
		    srcPtr->delay = delay;
		    break;
		} 
		case 2: {	/* Restore background. Don't blend. */
		    Blt_Pixel pixel;
		    int delay;
		    
		    delay = srcPtr->delay;
		    Blt_BlendPictures(composite, srcPtr, 0, 0, 
				      srcPtr->width, srcPtr->height, 0, 0);
		    Blt_FreePicture(srcPtr);
		    srcPtr = composite;
		    /* Make a copy of the new composite. */
		    next = Blt_ClonePicture(composite);
		    srcPtr->delay = delay;
		    /* Clear the current region for the next frame. */
		    pixel.u32 = 0x0;
		    Blt_BlankRegion(next, gif.imageLeftPosition, 
				    gif.imageTopPosition, 
				    gif.imageWidth, gif.imageHeight, 
				    &pixel);
		    break;
		}
		case 3: {	/* Restore to previous. */
		    int delay;
		    
		    delay = srcPtr->delay;
		    /* Save a copy of the current composite for the next
		     * frame. */
		    next = Blt_ClonePicture(composite);
		    /* Blend in the next frame. */
		    Blt_BlendPictures(composite, srcPtr, 0, 0, 
				      srcPtr->width, srcPtr->height, 0, 0);
		    Blt_FreePicture(srcPtr);
		    srcPtr = composite;
		    srcPtr->delay = delay;
		    break;
		}
		default:
		    fprintf(stderr, "unknown disposal method %d\n", 
			    gif.disposalMethod);
		    break;
		}
	    }
	    Blt_Chain_Append(chain, srcPtr);
	    composite = next;
	    break;
	case ';':		/* File terminator */
	    goto done;
	default:
	    fprintf(stderr, "ignoring %x at %d of %d\n", byte, 
		    (int)Blt_DBuffer_Cursor(dbuffer), 
		    (int)Blt_DBuffer_Length(dbuffer));
	}
    }
 done:
    if (composite != NULL) {
	Blt_FreePicture(composite);
    }
    if (message.nWarnings > 0) {
	Tcl_SetErrorCode(interp, "PICTURE", "GIF_READ_WARNINGS", 
		Tcl_DStringValue(&message.warnings), (char *)NULL);
    } else {
	Tcl_SetErrorCode(interp, "NONE", (char *)NULL);
    }
    Tcl_DStringFree(&message.warnings);
    Tcl_DStringFree(&message.errors);
    return chain;
}

static int 
GetLog2(unsigned int n) 
{
    int i;

    for (i = 0; (n >> i) != 0; i++) {
	/*empty*/
    }
    return i;
}

/*
 * Logical Screen Descriptor
 *
 *     6  0  2    Logical Screen Width
 *     8  2  2    Logical Screen Height 
 *     10 4  1    Flags 
 *			bit 7    = Global color table flag.
 *			bits 4-6 = Color resolution.
 *			bit 3    = Sort flag.
 *			bits 0-2 = Size of global color table.
 *     11 5  1    Background Color Index
 *     12 6  1    Pixel Aspect Ratio
 */
static void 
GifWriteLogicalScreenDescriptor(
    int width, 
    int height, 
    int bitsPerPixel, 
    unsigned char *bp)
{
    unsigned char flags;

#define MAXSIZE (1<<16)
    if ((width >= MAXSIZE) || (height >= MAXSIZE)) {
	GifError("picture is too big for GIF format");
    }	     
    if ((bitsPerPixel > 8) || (bitsPerPixel < 1)) {
	GifError("#bits per pixel is %d for GIF format", bitsPerPixel);
    }	     
    GifSetShort(bp + 0, width);
    GifSetShort(bp + 2, height);
    flags = 0;
    flags |= (1<<7);		/* Global color map flag.*/
    flags |= (bitsPerPixel - 1) << 4;  /* Color resolution. */
    flags |= (bitsPerPixel - 1); /* Size of global color table. */

    bp[4] = flags;
    bp[5] = 0;			/* Background color index. */
    bp[6] = 0;			/* Pixel aspect ratio. */
}


/*
 * Image Descriptor 
 *
 *   0 1 Image Separator
 *   1 2 Image Left Position
 *   3 2 Image Top Position      
 *   5 2 Image Width
 *   7 2 Image Height
 *   9 1 Image flags:
 *			=	Local Color Table Flag        1 Bit
 *				Interlace Flag                1 Bit
 *				Sort Flag                     1 Bit
 *				Reserved                      2 Bits
 *				Size of Local Color Table     3 Bits
 */
static void 
GifWriteImageDescriptor(int w, int h, unsigned char *bp)
{
    bp[0] = ',';		/* Image separator */
    bp[1] = bp[2] = 0;		/* Left offset of image. */
    bp[3] = bp[4] = 0;		/* Top offset of image. */
    GifSetShort(bp + 5, w);	/* Width of image. */
    GifSetShort(bp + 7, h);	/* Height of image. */
    bp[9] = 0;			/* Flags: no local table, not interlaced,
				 * and unsorted. */
}

/*
 *  Graphic Control Extension 
 *
 *   0 1  Extension Introducer
 *   1 2  Graphic Control Label
 *   2 1  Block size
 *   3 1  Flags.
 *		bits 5-7 Reserved                      3 Bits 
 *		bits 2-4 Disposal Method               3 Bits 
 *		bit 1    User Input Flag               1 Bit 
 *		bit 0    Transparent Color Flag        1 Bit 
 *   4 2  Delay time.
 *   6 1  Transparent Color Index
 *   7 1  Block terminator
 */
static void 
GifWriteGraphicControlExtension(int colorIndex, unsigned char *bp)
{
    bp[0] = '!';		/* Extension introducer */
    bp[1] = 0xF9; 		/* Graphic control label */
    bp[2] = 4;			/* Block size. */
    bp[3] = (colorIndex != -1);	/* Transparent flag. */
    bp[4] = bp[5] = 0;		/* Delay time. */
    bp[6] = colorIndex;		/* Transparent color index */
    bp[7] = 0;			/* Block terminator */
}

/*
 * Write optional comment block
 */
static void 
GifWriteCommentExtension(Blt_DBuffer dbuffer, const char *comment)
{
    int n, length;
    unsigned char *bp;

    /* Comment extension and label */
    length = strlen(comment);
    if (length > 255) {
	length = 255;
    }
    n = 3 + length + 1;
    bp = Blt_DBuffer_Extend(dbuffer, n);
    bp[0] = '!';
    bp[1] = 0xFE;
    bp[2] = length;
    memcpy(bp + 3, comment, length);
    bp[n - 1] = '\0';
}

#define DICTSIZE  5003            /* 80% occupancy */
#define MAXCODE(n)        ((1 << (n)) - 1)

typedef struct {
    int fcode;
    unsigned int ent;
} LzwDictEntry;

typedef struct {
    int nBits;
    int maxCode;
    int codeLimit;
    LzwDictEntry dict[DICTSIZE];
    int ent;
    int hshift;
    int nextUnusedCode;
    int codeCount;
    int initBits;
    int clearCode;
    int eofCode;
    unsigned int curAccum;
    int curBits;
    unsigned char accum[256];
    int a_count;
    Blt_DBuffer dbuffer;
} LzwCompressor;

static void
LzwDictShift(LzwCompressor *lzwPtr)
{
    int hshift;
    int fcode;
    
    hshift = 0;
    for (fcode = DICTSIZE; fcode < 65536L; fcode += fcode) {
	hshift++;
    }
    lzwPtr->hshift = 8 - hshift;	/* set hash code range bound */
}

static void
LzwDictInit(LzwCompressor *lzwPtr)
{
    LzwDictEntry *hp, *hend;
    
    for (hp = lzwPtr->dict, hend = hp + DICTSIZE; hp < hend; hp++) {
	hp->fcode = -1;
	hp->ent = 0;
    }
    lzwPtr->nextUnusedCode = lzwPtr->clearCode + 2;
}
 
/*
 * Flush the packet to disk, and reset the accumulator
 */
INLINE static void
LzwFlush(LzwCompressor *lzwPtr)
{
    if(lzwPtr->a_count > 0) {
	unsigned char *bp;

	bp = Blt_DBuffer_Extend(lzwPtr->dbuffer, lzwPtr->a_count + 1);
	bp[0] = lzwPtr->a_count;
	memcpy(bp + 1, lzwPtr->accum, lzwPtr->a_count);
	lzwPtr->a_count = 0;
    }
}

/*
 * Add a character to the end of the current packet, and if it is 254
 * characters, flush the packet to disk.
 */
INLINE static void
LzwAccum(LzwCompressor *lzwPtr, unsigned char byte)
{
    lzwPtr->accum[lzwPtr->a_count] = byte;
    lzwPtr->a_count++;
    if(lzwPtr->a_count >= 254) {
	LzwFlush(lzwPtr);
    }
}

INLINE static void
LzwSetCodeSize(LzwCompressor *lzwPtr, int nBits)
{
    lzwPtr->nBits = nBits;
    assert(nBits <= LZW_MAX_BITS);
    lzwPtr->maxCode = MAXCODE(nBits);
}

INLINE static void
LzwIncrCodeSize(LzwCompressor *lzwPtr) 
{
    assert((lzwPtr->nBits + 1) <= LZW_MAX_BITS);
    LzwSetCodeSize(lzwPtr, lzwPtr->nBits + 1);
}

INLINE static void
LzwAdjustCodeSize(LzwCompressor *lzwPtr) 
{
    assert(lzwPtr->nextUnusedCode <= lzwPtr->codeLimit);
    if (lzwPtr->nextUnusedCode == lzwPtr->codeLimit) {
	lzwPtr->codeLimit += lzwPtr->codeLimit;
	LzwIncrCodeSize(lzwPtr);
    }
    lzwPtr->nextUnusedCode++;
    assert(lzwPtr->codeLimit <= LZW_MAX_CODE);
}

static int
LzwDictSearch(LzwCompressor *lzwPtr, int pixel)
{
    int hval;
    int fcode;
    int disp;
    LzwDictEntry *hPtr;

    fcode = (int) ((pixel << LZW_MAX_BITS) + lzwPtr->ent);
    hval = ((pixel << lzwPtr->hshift) ^ lzwPtr->ent); /* XOR hashing */

    /* Secondary hash (after G. Knott) */
    disp = (hval == 0) ? 1 : DICTSIZE - hval; 
    while ((lzwPtr->dict[hval].fcode != fcode) && 
	   (lzwPtr->dict[hval].fcode >= 0)) {
	hval -= disp;
	if (hval < 0) {
	    hval += DICTSIZE;
	}
    }
    hPtr = lzwPtr->dict + hval;
    if (hPtr->fcode == fcode ) {
	lzwPtr->ent = hPtr->ent;
	return TRUE;
    } else {
	hPtr->ent = lzwPtr->nextUnusedCode;
	hPtr->fcode = fcode;
	return FALSE;
    }
}

static void
LzwPutCode(LzwCompressor *lzwPtr, int code)
{
    assert(code <= lzwPtr->maxCode);
    lzwPtr->curAccum &= (1 << lzwPtr->curBits) - 1;
    if (lzwPtr->curBits > 0) {
        lzwPtr->curAccum |= ((unsigned long)code << lzwPtr->curBits);
    } else {
        lzwPtr->curAccum = code;
    }
    lzwPtr->curBits += lzwPtr->nBits;

    while (lzwPtr->curBits >= 8) {
        LzwAccum(lzwPtr, (unsigned int)lzwPtr->curAccum & 0xff);
        lzwPtr->curAccum >>= 8;
        lzwPtr->curBits -= 8;
    }
    lzwPtr->codeCount++;
}


static void
LzwResetCompressor(LzwCompressor *lzwPtr)
{
    LzwDictInit(lzwPtr);	/* Clear out the hash table */
    LzwPutCode(lzwPtr, lzwPtr->clearCode);
    LzwSetCodeSize(lzwPtr, lzwPtr->initBits);
    lzwPtr->codeLimit = (1 << lzwPtr->initBits);
}

static void
LzwOutputCurrent(LzwCompressor *lzwPtr)
{
    LzwPutCode(lzwPtr, lzwPtr->ent);
    /*
     * If the next entry is going to be too big for the code size, then
     * increase it, if possible.
     */
    if (lzwPtr->nextUnusedCode < LZW_MAX_CODE) {
	LzwAdjustCodeSize(lzwPtr);
    } else {
	LzwResetCompressor(lzwPtr);
    }
    if (lzwPtr->ent == lzwPtr->eofCode) {
	/* At EOF, write the rest of the buffer. */
	while (lzwPtr->curBits > 0) {
	    LzwAccum(lzwPtr, (unsigned int)lzwPtr->curAccum & 0xff);
	    lzwPtr->curAccum >>= 8;
	    lzwPtr->curBits -= 8;
	}
	LzwFlush(lzwPtr);
    }
}

static void
GifWriteGlobalColorTable(Blt_HashTable *colorTablePtr, unsigned char *bp)
{
    unsigned long index;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    
    index = 0;
    for (hPtr = Blt_FirstHashEntry(colorTablePtr, &cursor); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&cursor)) {
	union {
	    unsigned long index;
	    char *key;
	} value;
	Blt_Pixel pixel;
	
	Blt_SetHashValue(hPtr, index);
	value.key = Blt_GetHashKey(colorTablePtr, hPtr);
	pixel.u32 = (unsigned int)value.index;
	bp[0] = pixel.Red;
	bp[1] = pixel.Green;
	bp[2] = pixel.Blue;
	bp += 3;
	index++;
    }
}

static int
GetColorIndex(Blt_HashTable *colorTablePtr, Blt_Pixel *colorPtr)
{
    Blt_HashEntry *hPtr;
    Blt_Pixel pixel;
    int index;
    int nColors;
    union {
	unsigned long pixel;
	char *key;
    } value;

    nColors = colorTablePtr->numEntries;
    pixel.u32 = colorPtr->u32;
    pixel.Alpha = 0xFF;
    value.pixel = pixel.u32;
    hPtr = Blt_FindHashEntry(colorTablePtr, value.key);
    if (hPtr == NULL) {
	GifError("can't find color %x,%x,%x in color table\n", 
		 colorPtr->Red, colorPtr->Blue, colorPtr->Green);
    }
    if (colorPtr->Alpha == 0x00) {
	index = (int)nColors;
    } else {
	index = (unsigned long)Blt_GetHashValue(hPtr);
    }
    return index;
}

static void
GifAddText(Blt_DBuffer dbuffer, const char **comments)
{
    const char **p;

    for (p = comments; *p != NULL; p++) {
	GifWriteCommentExtension(dbuffer, *p);
    }
}

static void
GifWriteImageData(Blt_DBuffer dbuffer, Pict *srcPtr, 
		  Blt_HashTable *colorTablePtr)
{
    LzwCompressor lzw;
    int initBits;
    int initCodeSize;
    int bitsPerPixel;
    
    bitsPerPixel = GetLog2(colorTablePtr->numEntries - 1);
    assert((bitsPerPixel > 0) && (bitsPerPixel <= 8));
    /* 
       bitsPerPixel = (colorMapSize == 1) ? 1 : ns(colorMapSize - 1);
       initCodeSize = bitsPerPixel <= 1 ? 2 : bitsPerPixel;
       initBits = initCodeSize + 1;
       clearCode = 1 << (initBits - 1);
     */
    initCodeSize = (bitsPerPixel <= 1) ? 2 : bitsPerPixel;
    initBits = bitsPerPixel + 1;
    memset(&lzw, 0, sizeof(lzw));
    /*
     * Set up the globals:  initBits - initial number of bits
     */
    lzw.initBits = initBits;
    lzw.curAccum = 0;
    lzw.curBits = 0;
    lzw.dbuffer = dbuffer;

    /* Set up the necessary values */
    lzw.codeCount      = 0;
    lzw.nBits          = lzw.initBits;
    lzw.maxCode        = MAXCODE(lzw.nBits);
    lzw.clearCode      = (1 << (initBits - 1));
    lzw.eofCode        = lzw.clearCode + 1;
    lzw.nextUnusedCode = lzw.clearCode + 2;
    lzw.codeLimit      = (1 << initBits);
    lzw.a_count        = 0;
    LzwDictShift(&lzw);

    Blt_DBuffer_AppendByte(dbuffer, initCodeSize);
    LzwResetCompressor(&lzw);
    /* Add compressed image data. */
    {
	Blt_Pixel *srcRowPtr, *sp;
	int y;

	sp = srcRowPtr = srcPtr->bits;
	lzw.ent = GetColorIndex(colorTablePtr, sp);
	sp++;
	for(y = 0; y < srcPtr->height; y++) {
	    Blt_Pixel *send;
	    
	    for (send = srcRowPtr + srcPtr->width; sp < send; sp++) {
		unsigned long index;
		
		index = GetColorIndex(colorTablePtr, sp);
		if (!LzwDictSearch(&lzw, index)) {
		    LzwOutputCurrent(&lzw);
		    lzw.ent = index;
		}
	    }
	    srcRowPtr += srcPtr->pixelsPerRow;
	    sp = srcRowPtr;
	}
    }
    /* Put out the final code and sub-block terminator. */
    LzwOutputCurrent(&lzw);
    lzw.ent = lzw.eofCode;
    LzwOutputCurrent(&lzw);
    Blt_DBuffer_AppendByte(dbuffer, '\0');  /* Mark end of data sub-blocks. */
}
 
/*
 *---------------------------------------------------------------------------
 *
 * PictureToGif --
 *
 *      Converts a picture to the GIF format.  Since the GIF format doesn't
 *      handle semi-transparent pixels we have to blend the image with a given
 *      background color, while still retaining any 100% transparent pixels.
 *
 * Results:
 *      A standard TCL result is returned.  If an error occured, 
 *	such as the designated file could not be opened, TCL_ERROR is returned.
 *
 * Side Effects:
 *	The dynamic buffer is filled with the GIF image.
 *
 *---------------------------------------------------------------------------
 */
static int
PictureToGif(Tcl_Interp *interp, Blt_Picture original, Blt_DBuffer dbuffer,
	     GifExportSwitches *switchesPtr)
{
    Blt_HashTable colorTable;
    Pict *srcPtr;
    int bitsPerPixel;
    int isMasked;
    int n;
    int nColors, maxColors;
    unsigned char *bp;

    srcPtr = original;
    nColors = Blt_QueryColors(srcPtr, (Blt_HashTable *)NULL);
    maxColors = 256;
    if (Blt_PictureIsMasked(srcPtr)) {
	if (switchesPtr->flags & EXPORT_BLEND) {	
	    Blt_Picture background;
	    
	    /* Blend picture with solid color background. */
	    background = Blt_CreatePicture(srcPtr->width, srcPtr->height);
	    Blt_BlankPicture(background, &switchesPtr->bg); 
	    Blt_BlendPictures(background, srcPtr, 0, 0, srcPtr->width, 
			      srcPtr->height, 0, 0);
	    if (srcPtr != original) {
		Blt_FreePicture(srcPtr);
	    }
	    srcPtr = background;
	    nColors = Blt_QueryColors(srcPtr, (Blt_HashTable *)NULL);
	} else if (nColors >= 256) {
	    maxColors--;
	}
    }
    if (nColors > maxColors) {
	Blt_Picture quant;

	quant = Blt_QuantizePicture(srcPtr, maxColors);
	if (srcPtr != original) {
	    Blt_FreePicture(srcPtr);
	}
	srcPtr = quant;
    }
    Blt_InitHashTable(&colorTable, BLT_ONE_WORD_KEYS);
    nColors = Blt_QueryColors(srcPtr, &colorTable);
    isMasked = Blt_PictureIsMasked(srcPtr);
    bitsPerPixel = GetLog2(nColors - 1);
    /* 
     * 6			Header
     * 7			Logical Screen Descriptor
     * 3 * (1<<bitPerPixel)	Global Color Table
     * 8			Graphic Control Extension
     * 10			Image Descriptor
     */
    n = 6 + 7 + (3 * (1<<bitsPerPixel)) + 10;
    if (isMasked) {
	n += 8;
    }
    bp = Blt_DBuffer_Extend(dbuffer, n);

    /* Header */
    if (isMasked) {
	memcpy(bp, "GIF89a", 6);
    } else {
	memcpy(bp, "GIF87a", 6);
    }
    bp += 6;			/* Size of header */
    GifWriteLogicalScreenDescriptor(srcPtr->width, srcPtr->height, 
				    bitsPerPixel, bp);
    bp += 7;			/* Size of logical screen descriptor. */
    GifWriteGlobalColorTable(&colorTable, bp);
    bp += 3 * (1<<bitsPerPixel);  /* Size of global color table. */
    if (isMasked) {
	GifWriteGraphicControlExtension(nColors, bp);
	bp += 8;		/* Size of graphic control extension. */
    }
    GifWriteImageDescriptor(srcPtr->width, srcPtr->height, bp);
    bp += 10;			/* Size of image descriptor. */

    GifWriteImageData(dbuffer, srcPtr, &colorTable);
    if (switchesPtr->comments != NULL) {
	GifAddText(dbuffer, switchesPtr->comments);
    }
    Blt_DBuffer_AppendByte(dbuffer, ';');  /* File terminator */
    if (srcPtr != original) {
	Blt_FreePicture(srcPtr);
    }
    return TCL_OK;
}

/* { 7,  7,  5  },		 8 bits, 198 colors */

static void
FillPalette(int nRed, int nGreen, int nBlue, Blt_Pixel *palette,
	    unsigned int *rBits, unsigned int *gBits, unsigned int *bBits)
{
    int i, rMult;
    double rScale, gScale, bScale;
    
    rMult = nGreen * nBlue;
    
    rScale = 255.0 / (nRed - 1);
    gScale = 255.0 / (nGreen - 1);
    bScale = 255.0 / (nBlue - 1);
    
    for (i = 0; i < 256; i++) {
	int r, g, b;
	
	r = (i * (nRed   - 1) + 127) / 255;
	g = (i * (nGreen - 1) + 127) / 255;
	b = (i * (nBlue  - 1) + 127) / 255;
	palette[i].Red = (unsigned char)(r * rScale + 0.5);
	palette[i].Green = (unsigned char)(g * gScale + 0.5);
	palette[i].Blue = (unsigned char)(b * bScale + 0.5);
	rBits[i] = r * rMult;
	gBits[i] = g * nBlue;
	bBits[i] = b;
    }
}    


/*
 *---------------------------------------------------------------------------
 *
 * IsGif --
 *
 *      Attempts to parse a GIF file header.
 *
 * Results:
 *      Returns 1 is the header is GIF and 0 otherwise.  Note that the
 *      validity of the header contents is not checked here.  That's done in
 *      GifToPictures.
 *
 *---------------------------------------------------------------------------
 */
static int
IsGif(Blt_DBuffer dbuffer)
{
    Gif gif;
    int bool;

    bool = GifHeader(dbuffer, &gif);
    return bool;
}

static Blt_Chain
ReadGif(Tcl_Interp *interp, const char *fileName, Blt_DBuffer dbuffer)
{
    GifImportSwitches switches;

    memset(&switches, 0, sizeof(switches));
    return GifToPictures(interp, fileName, dbuffer, &switches);
}

static Tcl_Obj *
WriteGif(Tcl_Interp *interp, Blt_Chain chain)
{
    Tcl_Obj *objPtr;
    Blt_DBuffer dbuffer;
    GifExportSwitches switches;
    Blt_ChainLink link;

    /* Default export switch settings. */
    memset(&switches, 0, sizeof(switches));

    dbuffer = Blt_DBuffer_Create();
    objPtr = NULL;
    for (link = Blt_Chain_FirstLink(chain); link != NULL; 
	 link = Blt_Chain_NextLink(link)) {
	Blt_Picture picture;

	picture = Blt_Chain_GetValue(link);
	if (PictureToGif(interp, picture, dbuffer, &switches) != TCL_OK) {
	    Blt_DBuffer_Destroy(dbuffer);
	    return NULL;
	}
    }
    char *bytes;

    bytes = Blt_DBuffer_EncodeBase64(interp, dbuffer);
    if (bytes != NULL) {
	objPtr = Tcl_NewStringObj(bytes, -1);
	Blt_Free(bytes);
    }
    Blt_DBuffer_Destroy(dbuffer);
    return objPtr;
}

static Blt_Chain
ImportGif(Tcl_Interp *interp, int objc, Tcl_Obj *const *objv, 
	  const char **fileNamePtr)
{
    Blt_DBuffer dbuffer;
    Blt_Chain chain;
    const char *string;
    GifImportSwitches switches;

    memset(&switches, 0, sizeof(switches));
    if (Blt_ParseSwitches(interp, importSwitches, objc - 3, objv + 3, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	Blt_FreeSwitches(importSwitches, (char *)&switches, 0);
	return NULL;
    }
    if ((switches.dataObjPtr != NULL) && (switches.fileObjPtr != NULL)) {
	Tcl_AppendResult(interp, "more than one import source: ",
		"use only one -file or -data flag.", (char *)NULL);
	Blt_FreeSwitches(importSwitches, (char *)&switches, 0);
	return NULL;
    }
    dbuffer = Blt_DBuffer_Create();
    chain = NULL;
    if (switches.dataObjPtr != NULL) {
	unsigned char *bytes;
	int nBytes;

	bytes = Tcl_GetByteArrayFromObj(switches.dataObjPtr, &nBytes);
	if (Blt_IsBase64((const char *)bytes, nBytes)) {
	    if (Blt_DBuffer_DecodeBase64(interp, string, nBytes, dbuffer) 
		!= TCL_OK) {
		goto error;
	    }
	} else {
	    Blt_DBuffer_AppendData(dbuffer, bytes, nBytes);
	} 
	string = "data buffer";
	*fileNamePtr = NULL;
    } else {
	string = Tcl_GetString(switches.fileObjPtr);
	*fileNamePtr = string;
	if (Blt_DBuffer_SaveFile(interp, string, dbuffer) != TCL_OK) {
	    goto error;
	}
    }
    chain = GifToPictures(interp, string, dbuffer, &switches);
    if (chain == NULL) {
	fprintf(stderr, "import gif: can't read buffer\n");
    }
 error:
    Blt_FreeSwitches(importSwitches, (char *)&switches, 0);
    Blt_DBuffer_Destroy(dbuffer);
    return chain;
}

static int
ExportGif(Tcl_Interp *interp, Blt_Chain chain, int objc, Tcl_Obj *const *objv)
{
    Blt_ChainLink link;
    GifExportSwitches switches;
    Blt_DBuffer dbuffer;
    int result;

    /* Default export switch settings. */
    memset(&switches, 0, sizeof(switches));

    if (Blt_ParseSwitches(interp, exportSwitches, objc - 3, objv + 3, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	Blt_FreeSwitches(exportSwitches, (char *)&switches, 0);
	return TCL_ERROR;
    }
    if ((switches.dataObjPtr != NULL) && (switches.fileObjPtr != NULL)) {
	Tcl_AppendResult(interp, "more than one export destination: ",
		"use only one -file or -data flag.", (char *)NULL);
	Blt_FreeSwitches(exportSwitches, (char *)&switches, 0);
	return TCL_ERROR;
    }

    dbuffer = Blt_DBuffer_Create();
    for (link = Blt_Chain_FirstLink(chain); link != NULL; 
	 link = Blt_Chain_NextLink(link)) {
	Blt_Picture picture;

	picture = Blt_Chain_GetValue(link);
	if (PictureToGif(interp, picture, dbuffer, &switches) != TCL_OK) {
	    Tcl_AppendResult(interp, "can't convert \"", 
			     Tcl_GetString(objv[2]), "\"", (char *)NULL);
	    goto error;
	}
    }
    if (result != TCL_OK) {
	Tcl_AppendResult(interp, "can't export \"", 
		Tcl_GetString(objv[2]), "\" to GIF format", (char *)NULL);
	goto error;
    }

    if (switches.fileObjPtr != NULL) {
	const char *fileName;

	/* Write the image into the designated file. */
	fileName = Tcl_GetString(switches.fileObjPtr);
	result = Blt_DBuffer_SaveFile(interp, fileName, dbuffer);
    } else if (switches.dataObjPtr != NULL) {
	Tcl_Obj *objPtr;

	/* Write the image into the designated TCL variable. */
	objPtr = Tcl_ObjSetVar2(interp, switches.dataObjPtr, NULL, 
		Blt_DBuffer_ByteArrayObj(dbuffer), 0);
	result = (objPtr == NULL) ? TCL_ERROR : TCL_OK;
    } else {
	char *string;

	/* Return the image as a base64 string in the interpreter result. */
	result = TCL_ERROR;
	string = Blt_DBuffer_EncodeBase64(interp, dbuffer);
	if (string != NULL) {
	    Tcl_Obj *objPtr;

	    objPtr = Tcl_NewStringObj(string, -1);
	    Blt_Free(string);
	    Tcl_SetObjResult(interp, objPtr);
	    result = TCL_OK;
	}
    }
 error:
    Blt_FreeSwitches(exportSwitches, (char *)&switches, 0);
    Blt_DBuffer_Destroy(dbuffer);
    return result;
}

int 
Blt_PictureGifInit(Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 1) == NULL) {
	return TCL_ERROR;
    };
#endif
    if (Tcl_PkgRequire(interp, "blt_core", BLT_VERSION, /*Exact*/1) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgRequire(interp, "blt_extra", BLT_VERSION, /*Exact*/1) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgProvide(interp, "blt_picture_gif", BLT_VERSION) != TCL_OK) {
	return TCL_ERROR;
    }
    return Blt_PictureRegisterFormat(interp, 
	"gif",			/* Name of format. */
	IsGif,			/* Discovery routine. */
	ReadGif,		/* Read format procedure. */
	WriteGif,		/* Write format procedure. */
	ImportGif,		/* Import format procedure. */
	ExportGif);		/* Export format switches. */
}

#ifdef notdef
/*
 *---------------------------------------------------------------------------
 *
 * PicturesToGif --
 *
 *      Converts a picture to the GIF format.  Since the GIF format doesn't
 *      handle semi-transparent pixels we have to blend the image with a given
 *      background color, while still retaining any 100% transparent pixels.
 *
 * Results:
 *      A standard TCL result is returned.  If an error occured, 
 *	such as the designated file could not be opened, TCL_ERROR is returned.
 *
 * Side Effects:
 *	The dynamic buffer is filled with the GIF image.
 *
 *---------------------------------------------------------------------------
 */
static int
PicturesToGif(Tcl_Interp *interp, Blt_Chain chain, Blt_DBuffer dbuffer,
	      GifExportSwitches *switchesPtr, int noHeader)
{
    Blt_HashTable colorTable;
    Pict *srcPtr;
    int bitsPerPixel;
    int isMasked;
    int n;
    int nColors, maxColors;
    unsigned char *bp;
    Blt_Chain converted;
    
    /* For each frame 
     * quantize if needed, accumulate the colors used.
     * try to assemble  */

    /* Step 1. Count the Convert the pictures as necessary adding into another list. */
    Blt_InitHashTable(&colorTable, BLT_ONE_WORD_KEYS);
    nFrames = Blt_Chain_GetLength(chain);
    if (nFrames == 0) {
	return TCL_ERROR;
    }
    frames = Blt_Malloc(sizeof(Frame) * nFrames);
    if (frames == NULL) {
	Tcl_AppendResult(interp, "can't allocate ", Blt_Itoa(nFrames), 
			 " frames",  (char *)NULL);
	return TCL_ERROR;
    }
    count = 0;
    link = Blt_Chain_FirstLink(chain);
    framePtr = frames;
    framePtr->picture = Blt_Chain_GetValue(link);

    screenWidth = Blt_PictureWidth(frame[0].picture);
    screenHeight = Blt_PictureHeight(frame[0].picture);
    for (fp = frames = Blt_Chain_FirstLink(chain); link != NULL; 
	 link = Blt_Chain_NextLink(link), fp++) {
	Pict *srcPtr;

	srcPtr = fp->original = Blt_Chain_GetValue(link);
	fp->delay = srcPtr->delay;
	if (srcPtr->width > screenWidth) {
	    screenWidth = srcPtr->width;
	}
	if (srcPtr->height > screenHeight) {
	    screenHeight = srcPtr->height;
	}
    }
    for (fp = frames; fendPtr = frames + nFrames; fp < fendPtr; fp++) {
	Pict *srcPtr;

	srcPtr = fp->current = fp->original;
	if ((srcPtr->width != screenWidth) ||
	    (srcPtr->height != screenHeight) ||
	    (srcPtr->flags | (BLT_PIC_MASK|BLT_PIC_BLEND)) {
		if (switchesPtr->flags & EXPORT_BLEND) {	
		    Blt_Picture background;
		    
		    /* Blend picture with solid color background. */
		    background = Blt_CreatePicture(screenWidth, screenHeight);
		    Blt_BlankPicture(background, &switchesPtr->bg); 
		    Blt_BlendPictures(background, srcPtr, 0, 0, srcPtr->width, 
				      srcPtr->height, 0, 0);
		    srcPtr = fp->current = background;
		}
		nColors = Blt_QueryColors(srcPtr, (Blt_HashTable *)NULL);
	    } else if (nColors >= 256) {
		maxColors--;
	    }
	}
	if (nColors > maxColors) {
	    Blt_Picture quant;
	    
	    quant = Blt_QuantizePicture(srcPtr, maxColors);
	    if (srcPtr != original) {
		Blt_FreePicture(srcPtr);
	    }
	    srcPtr = quant;
	}
	/* FIXME: Save the quantized or standard image. Save delay. */
	Blt_QueryColors(srcPtr, &colorTable);
    }
    if (colorTable.numEntries > maxEntries) {
	Blt_Pixel palette[256];
	unsigned int rBits[256], gBits[256], bBits[256];

	FillPalette(7,7,5, palette, rBits, gBits, bBits);
	/* Pass 2. Create a color ramp and match the frames to the ramp. */
	for (link = Blt_Chain_FirstLink(newChain); link != NULL; 
	     link = Blt_Chain_NextLink(link)) {
	    Pict *srcPtr;
	    srcPtr = Blt_Chain_GetValue(link);
	    index = (rBits[sp->Red] | gBits[sp->Green] | bBits[sp->Blue]);
	    
	}
    }
    srcPtr = original;
    nColors = Blt_QueryColors(srcPtr, (Blt_HashTable *)NULL);
    maxColors = 256;
    if (Blt_PictureIsMasked(srcPtr)) {
	if (switchesPtr->flags & EXPORT_BLEND) {	
	    Blt_Picture background;
	    
	    /* Blend picture with solid color background. */
	    background = Blt_CreatePicture(srcPtr->width, srcPtr->height);
	    Blt_BlankPicture(background, &switchesPtr->bg); 
	    Blt_BlendPictures(background, srcPtr, 0, 0, srcPtr->width, 
			      srcPtr->height, 0, 0);
	    if (srcPtr != original) {
		Blt_FreePicture(srcPtr);
	    }
	    srcPtr = background;
	    nColors = Blt_QueryColors(srcPtr, (Blt_HashTable *)NULL);
	} else if (nColors >= 256) {
	    maxColors--;
	}
    }
    if (nColors > maxColors) {
	Blt_Picture quant;

	quant = Blt_QuantizePicture(srcPtr, maxColors);
	if (srcPtr != original) {
	    Blt_FreePicture(srcPtr);
	}
	srcPtr = quant;
    }
    Blt_InitHashTable(&colorTable, BLT_ONE_WORD_KEYS);
    nColors = Blt_QueryColors(srcPtr, &colorTable);
    isMasked = Blt_PictureIsMasked(srcPtr);
    bitsPerPixel = GetLog2(nColors - 1);
    /* 
     * 6			Header
     * 7			Logical Screen Descriptor
     * 3 * (1<<bitPerPixel)	Global Color Table
     * 8			Graphic Control Extension
     * 10			Image Descriptor
     */
    n = 6 + 7 + (3 * (1<<bitsPerPixel)) + 10;
    if (isMasked) {
	n += 8;
    }
    bp = Blt_DBuffer_Extend(dbuffer, n);

    /* Header */
    if (isMasked) {
	memcpy(bp, "GIF89a", 6);
    } else {
	memcpy(bp, "GIF87a", 6);
    }
    bp += 6;			/* Size of header */
    GifWriteLogicalScreenDescriptor(srcPtr->width, srcPtr->height, 
				    bitsPerPixel, bp);
    bp += 7;			/* Size of logical screen descriptor. */
    GifWriteGlobalColorTable(&colorTable, bp);
    bp += 3 * (1<<bitsPerPixel);  /* Size of global color table. */
    if (isMasked) {
	GifWriteGraphicControlExtension(nColors, bp);
	bp += 8;		/* Size of graphic control extension. */
    }
    GifWriteImageDescriptor(srcPtr->width, srcPtr->height, bp);
    bp += 10;			/* Size of image descriptor. */

    GifWriteImageData(dbuffer, srcPtr, &colorTable);
    if (switchesPtr->comments != NULL) {
	GifAddText(dbuffer, switchesPtr->comments);
    }
    Blt_DBuffer_AppendByte(dbuffer, ';');  /* File terminator */
    if (srcPtr != original) {
	Blt_FreePicture(srcPtr);
    }
    return TCL_OK;
}
#endif
