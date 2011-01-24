
/*
 * bltWinPainter.c --
 *
 * This module implements Win32-specific image processing procedures for the
 * BLT toolkit.
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
#include "bltBitmap.h"
#include "bltPicture.h"
#include "bltPictInt.h"
#include "bltPainter.h"
#include "bltWinPainter.h"
#include <X11/Xutil.h>
#include "tkDisplay.h"

#define GetBit(x, y) \
   srcBits[(srcBytesPerRow * (srcHeight - y - 1)) + (x>>3)] & (0x80 >> (x&7))
#define SetBit(x, y) \
   destBits[(destBytesPerRow * (destHeight - y - 1)) + (x>>3)] |= (0x80 >>(x&7))

Pixmap
Blt_PhotoImageMask(
    Tk_Window tkwin,
    Tk_PhotoImageBlock src)
{
    TkWinBitmap *twdPtr;
    int offset, count;
    int x, y;
    unsigned char *srcPtr;
    int destBytesPerRow;
    int destHeight;
    unsigned char *destBits;

    destBytesPerRow = ((src.width + 31) & ~31) / 8;
    destBits = Blt_AssertCalloc(src.height, destBytesPerRow);
    destHeight = src.height;

    offset = count = 0;

    /* FIXME: figure out why this is so! */
    for (y = src.height - 1; y >= 0; y--) {
	srcPtr = src.pixelPtr + offset;
	for (x = 0; x < src.width; x++) {
	    if (srcPtr[src.offset[3]] == 0x00) {
		SetBit(x, y);
		count++;
	    }
	    srcPtr += src.pixelSize;
	}
	offset += src.pitch;
    }
    if (count > 0) {
	HBITMAP hBitmap;
	BITMAP bm;

	bm.bmType = 0;
	bm.bmWidth = src.width;
	bm.bmHeight = src.height;
	bm.bmWidthBytes = destBytesPerRow;
	bm.bmPlanes = 1;
	bm.bmBitsPixel = 1;
	bm.bmBits = destBits;
	hBitmap = CreateBitmapIndirect(&bm);

	twdPtr = Blt_AssertMalloc(sizeof(TkWinBitmap));
	twdPtr->type = TWD_BITMAP;
	twdPtr->handle = hBitmap;
	twdPtr->depth = 1;
	if (Tk_WindowId(tkwin) == None) {
	    twdPtr->colormap = DefaultColormap(Tk_Display(tkwin), 
			 DefaultScreen(Tk_Display(tkwin)));
	} else {
	    twdPtr->colormap = Tk_Colormap(tkwin);
	}
    } else {
	twdPtr = NULL;
    }
    if (destBits != NULL) {
	Blt_Free(destBits);
    }
    return (Pixmap)twdPtr;
}

Pixmap
Blt_PictureMask(
    Tk_Window tkwin,
    Blt_Picture pict)
{
    TkWinBitmap *twdPtr;
    int count;
    int x, y;
    Blt_Pixel *sp;
    int destBytesPerRow;
    int destWidth, destHeight;
    unsigned char *destBits;

    destWidth = Blt_PictureWidth(pict);
    destHeight = Blt_PictureHeight(pict);
    destBytesPerRow = ((destWidth + 31) & ~31) / 8;
    destBits = Blt_AssertCalloc(destHeight, destBytesPerRow);
    count = 0;
    sp = Blt_PictureBits(pict);
    for (y = 0; y < destHeight; y++) {
	for (x = 0; x < destWidth; x++) {
	    if (sp->Alpha == 0x00) {
		SetBit(x, y);
		count++;
	    }
	    sp++;
	}
    }
    if (count > 0) {
	HBITMAP hBitmap;
	BITMAP bm;

	bm.bmType = 0;
	bm.bmWidth = Blt_PictureWidth(pict);
	bm.bmHeight = Blt_PictureHeight(pict);
	bm.bmWidthBytes = destBytesPerRow;
	bm.bmPlanes = 1;
	bm.bmBitsPixel = 1;
	bm.bmBits = destBits;
	hBitmap = CreateBitmapIndirect(&bm);

	twdPtr = Blt_AssertMalloc(sizeof(TkWinBitmap));
	twdPtr->type = TWD_BITMAP;
	twdPtr->handle = hBitmap;
	twdPtr->depth = 1;
	if (Tk_WindowId(tkwin) == None) {
	    twdPtr->colormap = DefaultColormap(Tk_Display(tkwin), 
			 DefaultScreen(Tk_Display(tkwin)));
	} else {
	    twdPtr->colormap = Tk_Colormap(tkwin);
	}
    } else {
	twdPtr = NULL;
    }
    if (destBits != NULL) {
	Blt_Free(destBits);
    }
    return (Pixmap)twdPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_RotateBitmap --
 *
 *	Creates a new bitmap containing the rotated image of the given
 *	bitmap.  We also need a special GC of depth 1, so that we do
 *	not need to rotate more than one plane of the bitmap.
 *
 *	Note that under Windows, monochrome bitmaps are stored
 *	bottom-to-top.  This is why the right angle rotations 0/180
 *	and 90/270 look reversed.
 *
 * Results:
 *	Returns a new bitmap containing the rotated image.
 *
 *---------------------------------------------------------------------------
 */
Pixmap
Blt_RotateBitmap(
    Tk_Window tkwin,
    Pixmap srcBitmap,		/* Source bitmap to be rotated */
    int srcWidth, 
    int srcHeight,		/* Width and height of the source bitmap */
    float angle,		/* Right angle rotation to perform */
    int *destWidthPtr, 
    int *destHeightPtr)
{
    Display *display;		/* X display */
    Window root;		/* Root window drawable */
    Pixmap destBitmap;
    double rotWidth, rotHeight;
    HDC hDC;
    TkWinDCState state;
    int x, y;			/* Destination bitmap coordinates */
    int sx, sy;			/* Source bitmap coordinates */
    unsigned long pixel;
    HBITMAP hBitmap;
    int result;
    struct MonoBitmap {
	BITMAPINFOHEADER bi;
	RGBQUAD colors[2];
    } mb;
    int srcBytesPerRow, destBytesPerRow;
    int destWidth, destHeight;
    unsigned char *srcBits, *destBits;

    display = Tk_Display(tkwin);
    root = Tk_RootWindow(tkwin);
    Blt_GetBoundingBox(srcWidth, srcHeight, angle, &rotWidth, &rotHeight,
	(Point2d *)NULL);

    destWidth = (int)ceil(rotWidth);
    destHeight = (int)ceil(rotHeight);
    destBitmap = Tk_GetPixmap(display, root, destWidth, destHeight, 1);
    if (destBitmap == None) {
	return None;		/* Can't allocate pixmap. */
    }
    srcBits = Blt_GetBitmapData(display, srcBitmap, srcWidth, srcHeight,
	&srcBytesPerRow);
    if (srcBits == NULL) {
	OutputDebugString("Blt_GetBitmapData failed");
	return None;
    }
    destBytesPerRow = ((destWidth + 31) & ~31) / 8;
    destBits = Blt_AssertCalloc(destHeight, destBytesPerRow);

    angle = FMOD(angle, 360.0);
    if (FMOD(angle, (double)90.0) == 0.0) {
	int quadrant;

	/* Handle right-angle rotations specially. */

	quadrant = (int)(angle / 90.0);
	switch (quadrant) {
	case ROTATE_270:	/* 270 degrees */
	    for (y = 0; y < destHeight; y++) {
		sx = y;
		for (x = 0; x < destWidth; x++) {
		    sy = destWidth - x - 1;
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	case ROTATE_180:		/* 180 degrees */
	    for (y = 0; y < destHeight; y++) {
		sy = destHeight - y - 1;
		for (x = 0; x < destWidth; x++) {
		    sx = destWidth - x - 1;
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	case ROTATE_90:		/* 90 degrees */
	    for (y = 0; y < destHeight; y++) {
		sx = destHeight - y - 1;
		for (x = 0; x < destWidth; x++) {
		    sy = x;
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	case ROTATE_0:		/* 0 degrees */
	    for (y = 0; y < destHeight; y++) {
		for (x = 0; x < destWidth; x++) {
		    pixel = GetBit(x, y);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	default:
	    /* The calling routine should never let this happen. */
	    break;
	}
    } else {
	double radians, sinTheta, cosTheta;
	double srcCX, srcCY;	/* Center of source rectangle */
	double destCX, destCY;	/* Center of destination rectangle */
	double tx, ty;
	double rx, ry;		/* Angle of rotation for x and y coordinates */

	radians = (angle / 180.0) * M_PI;
	sinTheta = sin(radians), cosTheta = cos(radians);

	/*
	 * Coordinates of the centers of the source and destination rectangles
	 */
	srcCX = srcWidth * 0.5;
	srcCY = srcHeight * 0.5;
	destCX = destWidth * 0.5;
	destCY = destHeight * 0.5;

	/* Rotate each pixel of dest image, placing results in source image */

	for (y = 0; y < destHeight; y++) {
	    ty = y - destCY;
	    for (x = 0; x < destWidth; x++) {

		/* Translate origin to center of destination image */
		tx = x - destCX;

		/* Rotate the coordinates about the origin */
		rx = (tx * cosTheta) - (ty * sinTheta);
		ry = (tx * sinTheta) + (ty * cosTheta);

		/* Translate back to the center of the source image */
		rx += srcCX;
		ry += srcCY;

		sx = ROUND(rx);
		sy = ROUND(ry);

		/*
		 * Verify the coordinates, since the destination image can be
		 * bigger than the source
		 */

		if ((sx >= srcWidth) || (sx < 0) || (sy >= srcHeight) ||
		    (sy < 0)) {
		    continue;
		}
		pixel = GetBit(sx, sy);
		if (pixel) {
		    SetBit(x, y);
		}
	    }
	}
    }
    hBitmap = ((TkWinDrawable *)destBitmap)->bitmap.handle;
    ZeroMemory(&mb, sizeof(mb));
    mb.bi.biSize = sizeof(BITMAPINFOHEADER);
    mb.bi.biPlanes = 1;
    mb.bi.biBitCount = 1;
    mb.bi.biCompression = BI_RGB;
    mb.bi.biWidth = destWidth;
    mb.bi.biHeight = destHeight;
    mb.bi.biSizeImage = destBytesPerRow * destHeight;
    mb.colors[0].rgbBlue = mb.colors[0].rgbRed = mb.colors[0].rgbGreen = 0x0;
    mb.colors[1].rgbBlue = mb.colors[1].rgbRed = mb.colors[1].rgbGreen = 0xFF;
    hDC = TkWinGetDrawableDC(display, destBitmap, &state);
    result = SetDIBits(hDC, hBitmap, 0, destHeight, (LPVOID)destBits, 
	(BITMAPINFO *)&mb, DIB_RGB_COLORS);
    TkWinReleaseDrawableDC(destBitmap, hDC, &state);
    if (!result) {
#if WINDEBUG
	PurifyPrintf("can't setDIBits: %s\n", Blt_LastError());
#endif
	destBitmap = None;
    }
    if (destBits != NULL) {
         Blt_Free(destBits);
    }
    if (srcBits != NULL) {
         Blt_Free(srcBits);
    }

    *destWidthPtr = destWidth;
    *destHeightPtr = destHeight;
    return destBitmap;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_ScaleBitmap --
 *
 *	Creates a new scaled bitmap from another bitmap. 
 *
 * Results:
 *	The new scaled bitmap is returned.
 *
 * Side Effects:
 *	A new pixmap is allocated. The caller must release this.
 *
 *---------------------------------------------------------------------------
 */
Pixmap
Blt_ScaleBitmap(
    Tk_Window tkwin,
    Pixmap srcBitmap,
    int srcWidth, 
    int srcHeight, 
    int destWidth, 
    int destHeight)
{
    TkWinDCState srcState, destState;
    HDC src, dest;
    Pixmap destBitmap;
    Window root;
    Display *display;

    /* Create a new bitmap the size of the region and clear it */

    display = Tk_Display(tkwin);
    root = Tk_RootWindow(tkwin);
    destBitmap = Tk_GetPixmap(display, root, destWidth, destHeight, 1);
    if (destBitmap == None) {
	return None;
    }
    src = TkWinGetDrawableDC(display, srcBitmap, &srcState);
    dest = TkWinGetDrawableDC(display, destBitmap, &destState);

    StretchBlt(dest, 0, 0, destWidth, destHeight, src, 0, 0,
	srcWidth, srcHeight, SRCCOPY);

    TkWinReleaseDrawableDC(srcBitmap, src, &srcState);
    TkWinReleaseDrawableDC(destBitmap, dest, &destState);
    return destBitmap;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_ScaleRotateBitmapArea --
 *
 *	Creates a scaled and rotated bitmap from a given bitmap.  The
 *	caller also provides (offsets and dimensions) the region of
 *	interest in the destination bitmap.  This saves having to
 *	process the entire destination bitmap is only part of it is
 *	showing in the viewport.
 *
 *	This uses a simple rotation/scaling of each pixel in the 
 *	destination image.  For each pixel, the corresponding 
 *	pixel in the source bitmap is used.  This means that 
 *	destination coordinates are first scaled to the size of 
 *	the rotated source bitmap.  These coordinates are then
 *	rotated back to their original orientation in the source.
 *
 * Results:
 *	The new rotated and scaled bitmap is returned.
 *
 * Side Effects:
 *	A new pixmap is allocated. The caller must release this.
 *
 *---------------------------------------------------------------------------
 */
Pixmap
Blt_ScaleRotateBitmapArea(
    Tk_Window tkwin,
    Pixmap srcBitmap,		/* Source bitmap. */
    unsigned int srcWidth, 
    unsigned int srcHeight,	/* Size of source bitmap */
    int regionX, 
    int regionY,		/* Offset of region in virtual
				 * destination bitmap. */
    unsigned int regionWidth, 
    unsigned int regionHeight,	/* Desire size of bitmap region. */
    unsigned int virtWidth,		
    unsigned int virtHeight,	/* Virtual size of destination bitmap. */
    float angle)		/* Angle to rotate bitmap.  */
{
    Display *display;		/* X display */
    Pixmap destBitmap;
    Window root;		/* Root window drawable */
    double rWidth, rHeight;
    double xScale, yScale;
    int srcBytesPerRow, destBytesPerRow;
    int destHeight;
    int result;
    unsigned char *srcBits, *destBits;

    display = Tk_Display(tkwin);
    root = Tk_RootWindow(tkwin);

    /* Create a bitmap and image big enough to contain the rotated text */
    destBitmap = Tk_GetPixmap(display, root, regionWidth, regionHeight, 1);
    if (destBitmap == None) {
	return None;		/* Can't allocate pixmap. */
    }
    srcBits = Blt_GetBitmapData(display, srcBitmap, srcWidth, srcHeight,
	&srcBytesPerRow);
    if (srcBits == NULL) {
	OutputDebugString("Blt_GetBitmapData failed");
	return None;
    }
    destBytesPerRow = ((regionWidth + 31) & ~31) / 8;
    destBits = Blt_AssertCalloc(regionHeight, destBytesPerRow);
    destHeight = regionHeight;

    angle = FMOD(angle, 360.0);
    Blt_GetBoundingBox(srcWidth, srcHeight, angle, &rWidth, &rHeight,
	       (Point2d *)NULL);
    xScale = rWidth / (double)virtWidth;
    yScale = rHeight / (double)virtHeight;

    if (FMOD(angle, (double)90.0) == 0.0) {
	int quadrant;
	int y;

	/* Handle right-angle rotations specifically */

	quadrant = (int)(angle / 90.0);
	switch (quadrant) {
	case ROTATE_270:	/* 270 degrees */
	    for (y = 0; y < (int)regionHeight; y++) {
		int sx, x;

		sx = (int)(yScale * (double)(y+regionY));
		for (x = 0; x < (int)regionWidth; x++) {
		    unsigned long pixel;
		    int sy;

		    sy = (int)(xScale *(double)(virtWidth - (x+regionX) - 1));
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	case ROTATE_180:	/* 180 degrees */
	    for (y = 0; y < (int)regionHeight; y++) {
		int sy, x;

		sy = (int)(yScale * (double)(virtHeight - (y + regionY) - 1));
		for (x = 0; x < (int)regionWidth; x++) {
		    unsigned long pixel;
		    int sx;

		    sx = (int)(xScale *(double)(virtWidth - (x+regionX) - 1));
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	case ROTATE_90:		/* 90 degrees */
	    for (y = 0; y < (int)regionHeight; y++) {
		int sx, x;

		sx = (int)(yScale * (double)(virtHeight - (y + regionY) - 1));
		for (x = 0; x < (int)regionWidth; x++) {
		    int sy;
		    unsigned long pixel;

		    sy = (int)(xScale * (double)(x + regionX));
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	case ROTATE_0:		/* 0 degrees */
	    for (y = 0; y < (int)regionHeight; y++) {
		int sy, x;

		sy = (int)(yScale * (double)(y + regionY));
		for (x = 0; x < (int)regionWidth; x++) {
		    int sx;
		    unsigned long pixel;

		    sx = (int)(xScale * (double)(x + regionX));
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	default:
	    /* The calling routine should never let this happen. */
	    break;
	}
    } else {
	double radians, sinTheta, cosTheta;
	double scx, scy; 	/* Offset from the center of the
				 * source rectangle. */
	double rcx, rcy; 	/* Offset to the center of the
				 * rotated rectangle. */
	int y;

	radians = (angle / 180.0) * M_PI;
	sinTheta = sin(radians), cosTheta = cos(radians);

	/*
	 * Coordinates of the centers of the source and destination rectangles
	 */
	scx = srcWidth * 0.5;
	scy = srcHeight * 0.5;
	rcx = rWidth * 0.5;
	rcy = rHeight * 0.5;

	/* For each pixel of the destination image, transform back to the
	 * associated pixel in the source image. */

	for (y = 0; y < (int)regionHeight; y++) {
	    int x;
	    double ty;		/* Translated coordinates from center */

	    ty = (yScale * (double)(y + regionY)) - rcy;
	    for (x = 0; x < (int)regionWidth; x++) {
		double rx, ry;	/* Angle of rotation for x and y coordinates */
		double tx;	/* Translated coordinates from center */
		int sx, sy;
		unsigned long pixel;

		/* Translate origin to center of destination image. */
		tx = (xScale * (double)(x + regionX)) - rcx;

		/* Rotate the coordinates about the origin. */
		rx = (tx * cosTheta) - (ty * sinTheta);
		ry = (tx * sinTheta) + (ty * cosTheta);

		/* Translate back to the center of the source image. */
		rx += scx;
		ry += scy;

		sx = ROUND(rx);
		sy = ROUND(ry);

		/*
		 * Verify the coordinates, since the destination image can be
		 * bigger than the source.
		 */

		if ((sx >= (int)srcWidth) || (sx < 0) || 
		    (sy >= (int)srcHeight) || (sy < 0)) {
		    continue;
		}
		pixel = GetBit(sx, sy);
		if (pixel) {
		    SetBit(x, y);
		}
	    }
	}
    }
    {
	HBITMAP hBitmap;
	HDC hDC;
	TkWinDCState state;
	struct MonoBitmap {
	    BITMAPINFOHEADER bmiHeader;
	    RGBQUAD colors[2];
	} mb;
	
	/* Write the rotated image into the destination bitmap. */
	hBitmap = ((TkWinDrawable *)destBitmap)->bitmap.handle;
	ZeroMemory(&mb, sizeof(mb));
	mb.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	mb.bmiHeader.biPlanes = 1;
	mb.bmiHeader.biBitCount = 1;
	mb.bmiHeader.biCompression = BI_RGB;
	mb.bmiHeader.biWidth = regionWidth;
	mb.bmiHeader.biHeight = regionHeight;
	mb.bmiHeader.biSizeImage = destBytesPerRow * regionHeight;
	mb.colors[0].rgbBlue = mb.colors[0].rgbRed = mb.colors[0].rgbGreen = 
	    0x0;
	mb.colors[1].rgbBlue = mb.colors[1].rgbRed = mb.colors[1].rgbGreen = 
	    0xFF;
	hDC = TkWinGetDrawableDC(display, destBitmap, &state);
	result = SetDIBits(hDC, hBitmap, 0, regionHeight, (LPVOID)destBits, 
		(BITMAPINFO *)&mb, DIB_RGB_COLORS);
	TkWinReleaseDrawableDC(destBitmap, hDC, &state);
    }
    if (!result) {
#if WINDEBUG
	PurifyPrintf("can't setDIBits: %s\n", Blt_LastError());
#endif
	destBitmap = None;
    }
    if (destBits != NULL) {
         Blt_Free(destBits);
    }
    if (srcBits != NULL) {
         Blt_Free(srcBits);
    }
    return destBitmap;
}

#ifdef notdef
/*
 *---------------------------------------------------------------------------
 *
 * Blt_PaintPictureWithBlend --
 *
 *      Takes a snapshot of an X drawable (pixmap or window) and
 *	converts it to a picture.
 *
 * Results:
 *      Returns a picture of the drawable.  If an error occurred,
 *	NULL is returned.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_PaintPictureWithBlend(
    Blt_Painter painter,
    Drawable drawable,
    int width, int height,	/* Dimension of the drawable. */
    Region2d *regionPtr)	/* Area to be snapped. */
{
    void *data;
    BITMAPINFO bmi;
    DIBSECTION ds;
    HBITMAP hBitmap, oldBitmap;
    HDC memDC;
    unsigned char *bits;
    unsigned char *srcPtr;
    HDC hDC;
    TkWinDCState state;
    Blt_Pixel *destRowPtr;
    Pict *destPtr;
    int x, y;

    if (regionPtr == NULL) {
	regionPtr = Blt_SetRegion(0, 0, PictureWidth(pict), 
		PictureHeight(pict), &region);
    }
    if (regionPtr->left < 0) {
	regionPtr->left = 0;
    }
    if (regionPtr->right >= destWidth) {
	regionPtr->right = destWidth - 1;
    }
    if (regionPtr->top < 0) {
	regionPtr->top = 0;
    }
    if (regionPtr->bottom >= destHeight) {
	regionPtr->bottom = destHeight - 1;
    }
    width = RegionWidth(regionPtr);
    height = RegionHeight(regionPtr);

    hDC = TkWinGetDrawableDC(display, drawable, &state);

    /* Create the intermediate drawing surface at window resolution. */
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    hBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, &data, NULL, 0);
    memDC = CreateCompatibleDC(hDC);
    oldBitmap = SelectBitmap(memDC, hBitmap);

#ifdef notdef
    if (GetDeviceCaps(hDC, RASTERCAPS) & RC_PALETTE) {
	TkWinColormap *cmap;

	cmap = (TkWinColormap *)painterPtr->colormap;
	SelectPalette(hDC, cmap->palette, FALSE);
	RealizePalette(hDC);
	SelectPalette(memDC, cmap->palette, FALSE);
	RealizePalette(memDC);
    }
#endif
    pict = NULL;
    /* Copy the window contents to the memory surface. */
    if (!BitBlt(memDC, 0, 0, width, height, hDC, regionPtr->left, 
	regionPtr->top, SRCCOPY)) {
#ifdef notdef
	PurifyPrintf("can't blit: %s\n", Blt_LastError());
#endif
	goto done;
    }
    if (GetObject(hBitmap, sizeof(DIBSECTION), &ds) == 0) {
#ifdef notdef
	PurifyPrintf("can't get object: %s\n", Blt_LastError());
#endif
	goto done;
    }
    bits = (unsigned char *)ds.dsBm.bmBits;
    destPtr = Blt_CreatePicture(width, height);
    destRowPtr = destPtr->bits;

    /* 
     * Copy the DIB RGB data into the picture. The DIB scanlines
     * are stored bottom-to-top and the order of the RGBA color
     * components is BGRA. Who says Win32 GDI programming isn't
     * backwards?  
     */
    for (y = height - 1; y >= 0; y--) {
	unsigned char *sp;
	Blt_Pixel *dp;

	sp = bits + (y * ds.dsBm.bmWidthBytes);
	dp = destRowPtr;
	for (dp = destRowPtr, dend = dp + destPtr->width; dp < dend, dp++) {
	    if (dp->Alpha > 0) {
		/* Blend picture with background. */
		dp->Blue = *sp++;
		dp->Green = *sp++;
		dp->Red = *sp++;
		dp->Alpha = ALPHA_OPAQUE;
		sp++;
	    }
	}
	destRowPtr += destPtr->pixelsPerRow;
    }
  done:
    DeleteBitmap(SelectBitmap(memDC, oldBitmap));
    DeleteDC(memDC);
    TkWinReleaseDrawableDC(drawable, hDC, &state);
    return pict;
}
#endif

#ifdef HAVE_IJL_H

#include <ijl.h>

Blt_Picture
Blt_JPEGToPicture(interp, fileName)
    Tcl_Interp *interp;
    char *fileName;
{
    JPEG_CORE_PROPERTIES jpgProps;
    Blt_Picture pict;

    ZeroMemory(&jpgProps, sizeof(JPEG_CORE_PROPERTIES));
    if(ijlInit(&jpgProps) != IJL_OK) {
	Tcl_AppendResult(interp, "can't initialize Intel JPEG library",
			 (char *)NULL);
	return NULL;
    }
    jpgProps.JPGFile = fileName;
    if (ijlRead(&jpgProps, IJL_JFILE_READPARAMS) != IJL_OK) {
	Tcl_AppendResult(interp, "can't read JPEG file header from \"",
			 fileName, "\" file.", (char *)NULL);
	goto error;
    }

    // !dudnik: to fix bug case 584680, [OT:287A305B]
    // Set the JPG color space ... this will always be
    // somewhat of an educated guess at best because JPEG
    // is "color blind" (i.e., nothing in the bit stream
    // tells you what color space the data was encoded from).
    // However, in this example we assume that we are
    // reading JFIF files which means that 3 channel images
    // are in the YCbCr color space and 1 channel images are
    // in the Y color space.
    switch(jpgProps.JPGChannels) {
    case 1:
	jpgProps.JPGColor = IJL_G;
	jpgProps.DIBChannels = 4;
	jpgProps.DIBColor = IJL_RGBA_FPX;
	break;
	
    case 3:
	jpgProps.JPGColor = IJL_YCBCR;
	jpgProps.DIBChannels = 4;
	jpgProps.DIBColor = IJL_RGBA_FPX;
	break;

    case 4:
	jpgProps.JPGColor = IJL_YCBCRA_FPX;
	jpgProps.DIBChannels = 4;
	jpgProps.DIBColor = IJL_RGBA_FPX;
	break;

    default:
	/* This catches everything else, but no color twist will be
           performed by the IJL. */
	jpgProps.DIBColor = (IJL_COLOR)IJL_OTHER;
 	jpgProps.JPGColor = (IJL_COLOR)IJL_OTHER;
	jpgProps.DIBChannels = jpgProps.JPGChannels;
	break;
    }

    jpgProps.DIBWidth    = jpgProps.JPGWidth;
    jpgProps.DIBHeight   = jpgProps.JPGHeight;
    jpgProps.DIBPadBytes = IJL_DIB_PAD_BYTES(jpgProps.DIBWidth, 
					     jpgProps.DIBChannels);

    pict = Blt_CreatePicture(jpgProps.JPGWidth, jpgProps.JPGHeight);

    jpgProps.DIBBytes = (BYTE *)Blt_PictureBits(pict);
    if (ijlRead(&jpgProps, IJL_JFILE_READWHOLEIMAGE) != IJL_OK) {
	Tcl_AppendResult(interp, "can't read image data from \"", fileName,
		 "\"", (char *)NULL);
	goto error;
    }
    if (ijlFree(&jpgProps) != IJL_OK) {
	Tcl_AppendResult(interp, "can't free Intel(R) JPEG library.", 
			 (char *)NULL);
    }
    return pict;

 error:
    ijlFree(&jpgProps);
    if (pict != NULL) {
	Blt_FreePicture(pict);
    }
    ijlFree(&jpgProps);
    return NULL;
} 

#endif /* HAVE_IJL_H */
