
/*
 * bltDBuffer.c --
 *
 * This module implements a dynamic buffer for the BLT toolkit.
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

#include "bltInt.h"
#include <bltDBuffer.h>

typedef struct _Blt_DBuffer DBuffer;

void
Blt_DBuffer_Init(DBuffer *srcPtr)
{
    srcPtr->bytes = NULL;
    srcPtr->cursor = srcPtr->count = srcPtr->nBytes = 0;
    srcPtr->chunk = 64;
}

void
Blt_DBuffer_Free(DBuffer *srcPtr)
{
    if ((srcPtr->bytes != NULL) && (srcPtr->nBytes > 0)) {
	Blt_Free(srcPtr->bytes);
    }
    Blt_DBuffer_Init(srcPtr);
}

Blt_DBuffer
Blt_DBuffer_Create(void)
{
    DBuffer *srcPtr;

    srcPtr = Blt_AssertMalloc(sizeof(DBuffer));
    Blt_DBuffer_Init(srcPtr);
    return srcPtr;
}

void 
Blt_DBuffer_Destroy(DBuffer *srcPtr) 
{
    Blt_DBuffer_Free(srcPtr);
    Blt_Free(srcPtr);
}

int
Blt_DBuffer_Resize(DBuffer *srcPtr, size_t nBytes)
{
    if (srcPtr->nBytes <= nBytes) {
	size_t size, wanted;
	unsigned char *bytes;

	wanted = nBytes + 1;
	size = srcPtr->chunk; 

	/* 
	 * Double the buffer size until we have enough room or hit 64K.  After
	 * 64K, increase by multiples of 64K.
	 */
	while ((size <= wanted) && (size < (1<<16))) {
	    size += size;
	}    
	srcPtr->chunk = size;
	while (size <= wanted) {
	    size += srcPtr->chunk;
	}
	if (srcPtr->bytes == NULL) {
 	    bytes = Blt_Malloc(size);
 	} else {
	    bytes = Blt_Realloc(srcPtr->bytes, size);
	}
	if (bytes == NULL) {
	    return FALSE;
 	}
	srcPtr->bytes = bytes;
	srcPtr->nBytes = size;
    }
    return TRUE;
}

unsigned char *
Blt_DBuffer_Extend(DBuffer *srcPtr, size_t nBytes) 
{
    unsigned char *bp;

    if (!Blt_DBuffer_Resize(srcPtr, srcPtr->count + nBytes)) {
	return NULL;
    }
    bp = srcPtr->bytes + srcPtr->count;
    srcPtr->count += nBytes;
    return bp;
}

void
Blt_DBuffer_AppendByte(DBuffer *destPtr, unsigned char value)
{
    if (Blt_DBuffer_Resize(destPtr, destPtr->count + sizeof(value))) {
	destPtr->bytes[destPtr->count] = value;
	destPtr->count++;
    }
}

void
Blt_DBuffer_AppendShort(DBuffer *destPtr, unsigned short value)
{
    if (Blt_DBuffer_Resize(destPtr, destPtr->count + sizeof(value))) {
	unsigned char *bp;

	bp = destPtr->bytes + destPtr->count;
#ifdef WORDS_BIGENDIAN
	bp[0] = (value >> 8)  & 0xFF;
	bp[1] = (value)       & 0xFF;
#else
	bp[0] = (value)       & 0xFF;
	bp[1] = (value >> 8)  & 0xFF;
#endif
	destPtr->count += 2;
    }
}

void
Blt_DBuffer_AppendLong(DBuffer *destPtr, unsigned int value)
{
    if (Blt_DBuffer_Resize(destPtr, destPtr->count + sizeof(value))) {
	unsigned char *bp;

	bp = destPtr->bytes + destPtr->count;
#ifdef WORDS_BIGENDIAN
	bp[0] = (value >> 24) & 0xFF;
	bp[1] = (value >> 16) & 0xFF;
	bp[2] = (value >> 8)  & 0xFF;
	bp[3] = (value)       & 0xFF;
#else
	bp[0] = (value)       & 0xFF;
	bp[1] = (value >> 8)  & 0xFF;
	bp[2] = (value >> 16) & 0xFF;
	bp[3] = (value >> 24) & 0xFF;
#endif
	destPtr->count += 4;
    }
}

Tcl_Obj *
Blt_DBuffer_ByteArrayObj(DBuffer *srcPtr)
{
    return Tcl_NewByteArrayObj(srcPtr->bytes, srcPtr->count);
}

Tcl_Obj *
Blt_DBuffer_StringObj(DBuffer *srcPtr)
{
    return Tcl_NewStringObj((char *)srcPtr->bytes, srcPtr->count);
}

int
Blt_DBuffer_AppendData(DBuffer *srcPtr, const unsigned char *data, 
		       size_t nBytes)
{
    unsigned char *bp;

    bp = Blt_DBuffer_Extend(srcPtr, nBytes);
    if (bp == NULL) {
	return FALSE;
    }
    memcpy(bp, data, nBytes);
    return TRUE;
}

void
Blt_DBuffer_VarAppend
TCL_VARARGS_DEF(DBuffer *, arg1)
{
    DBuffer *srcPtr;
    va_list args;

    srcPtr = TCL_VARARGS_START(DBuffer, arg1, args);
    for (;;) {
	const unsigned char *string;

	string = va_arg(args, const unsigned char *);
	if (string == NULL) {
	    break;
	}
	Blt_DBuffer_AppendData(srcPtr, string, strlen((const char *)string));
    }
}

void
Blt_DBuffer_Print
TCL_VARARGS_DEF(DBuffer *, arg1)
{
    DBuffer *srcPtr;
    char *fmt;
    char string[BUFSIZ+4];
    int length;
    va_list args;

    srcPtr = TCL_VARARGS_START(DBuffer, arg1, args);
    fmt = va_arg(args, char *);
    length = vsnprintf(string, BUFSIZ, fmt, args);
    if (length > BUFSIZ) {
	strcat(string, "...");
    }
    va_end(args);
    Blt_DBuffer_AppendData(srcPtr, (unsigned char *)string, strlen(string));
}

#include <sys/types.h>
#include <sys/stat.h>

#ifdef notdef
int
Blt_DBuffer_LoadFile(Tcl_Interp *interp, const char *fileName, 
		     Blt_DBuffer dBuffer)
{
    FILE *f;
    size_t nBytes, nRead;
    struct stat sb;
    unsigned char *bytes;

#ifdef WIN32
#define READ_MODE "rb"
#else 
#define READ_MODE "r"
#endif
    f = Blt_OpenFile(interp, fileName, READ_MODE);
    if (f == NULL) {
	return TCL_ERROR;
    }
    if (fstat(fileno(f), &sb)) {
	Tcl_AppendResult(interp, "can't stat \"", fileName, "\": ",
		Tcl_PosixError(interp), (char *)NULL);
	return TCL_ERROR;
    }	
    Blt_DBuffer_Init(dBuffer);
    nBytes = sb.st_size;	/* Size of buffer */
    if (!Blt_DBuffer_Resize(dBuffer, nBytes)) {
	fclose(f);
	return TCL_ERROR;
    }	
    bytes = Blt_DBuffer_Bytes(dBuffer);
    nRead = fread(bytes, sizeof(unsigned char), nBytes, f);
    Blt_DBuffer_SetLength(dBuffer, nRead);
    fclose(f);
    if (nRead != nBytes) {
	Tcl_AppendResult(interp, "short file \"", fileName, "\" : read ", 
		Blt_Itoa(nBytes), " bytes.", (char *)NULL); 
	Blt_DBuffer_Free(dBuffer);
	return TCL_ERROR;
    }	
    return TCL_OK;
}

#else

int
Blt_DBuffer_LoadFile(
    Tcl_Interp *interp, 
    const char *fileName, 
    Blt_DBuffer dBuffer)
{
    int nBytes;
    Tcl_Channel channel;

    if (fileName[0] == '@') { 
	int mode;

	/* If the file name starts with a '@', then it represents the name of
	 * a previously opened channel.  Verify that the channel was opened
	 * for reading. */
	fileName++;
	channel = Tcl_GetChannel(interp, fileName, &mode);
	if ((mode & TCL_READABLE) == 0) {
	    Tcl_AppendResult(interp, "can't read from \"", fileName, "\"",
			     (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	channel = Tcl_OpenFileChannel(interp, fileName, "r", 0);
    }
    if (channel == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_SetChannelOption(interp, channel, "-encoding", "binary")
	!= TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_SetChannelOption(interp, channel, "-translation", "binary") 
	!= TCL_OK) {
	return TCL_ERROR;
    }
    Blt_DBuffer_Init(dBuffer);
    nBytes = 0;
    while (!Tcl_Eof(channel)) {
	int nRead;
#define BUFFER_SIZE (1<<16)
	char *bp;

	bp = (char *)Blt_DBuffer_Extend(dBuffer, BUFFER_SIZE);
	nRead = Tcl_ReadRaw(channel, bp, BUFFER_SIZE);
	if (nRead == -1) {
	    Tcl_AppendResult(interp, "error reading ", fileName, ": ",
			Tcl_PosixError(interp), (char *)NULL);
	    Blt_DBuffer_Free(dBuffer);
	    return TCL_ERROR;
	}
	nBytes += nRead;
	Blt_DBuffer_SetLength(dBuffer, nBytes);
    }
    Tcl_Close(interp, channel);
    return TCL_OK;
}

#endif

int 
Blt_DBuffer_SaveFile(Tcl_Interp *interp, const char *fileName, 
		     Blt_DBuffer dBuffer)
{
    Tcl_Channel channel;
    size_t nWritten, nBytes;
    unsigned char *bytes;

    channel = Tcl_OpenFileChannel(interp, fileName, "w", 0660);
    if (channel == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetChannelOption(interp, channel, "-translation", "binary");
    Tcl_SetChannelOption(interp, channel, "-encoding", "binary");

    nBytes = Blt_DBuffer_Length(dBuffer);
    bytes = Blt_DBuffer_Bytes(dBuffer);
    nWritten = Tcl_Write(channel, (char *)bytes, nBytes);
    Tcl_Close(interp, channel);
    if (nWritten != nBytes) {
	Tcl_AppendResult(interp, "short file \"", fileName, (char *)NULL);
	Tcl_AppendResult(interp, "\" : wrote ", Blt_Itoa(nWritten), " of ", 
			 (char *)NULL);
	Tcl_AppendResult(interp, Blt_Itoa(nBytes), " bytes.", (char *)NULL); 
	return TCL_ERROR;
    }	
    return TCL_OK;
}

#ifdef notdef
static int 
ReadNextBlock(DBuffer *srcPtr)
{
    if (srcPtr->channel == NULL) {
	return -1;
    }
    if (Tcl_Eof(srcPtr->channel)) {
	return 0;
    }
    nRead = Tcl_ReadRaw(srcPtr->channel, srcPtr->bytes, BUFFER_SIZE);
    if (nRead == -1) {
	Tcl_AppendResult(interp, "error reading channel: ",
			 Tcl_PosixError(interp), (char *)NULL);
	return -1;
    }
    srcPtr->cursor = srcPtr->bytes;
    srcPtr->count = nRead;
    return 1;
}

int
Blt_DBuffer_GetNext(DBuffer *srcPtr)
{
    int byte;

    if ((srcPtr->cursor - srcPtr->bytes) >= srcPtr->count) {
	int result;

	result = 0;
	if (srcPtr->channel != NULL) {
	    result = ReadNextBlock(srcPtr);
	}
	if (result <= 0) {
	    return result;
	}
    }
    byte = *srcPtr->cursor;
    srcPtr->cursor++;
    return byte;
}
#endif

int
Blt_DBuffer_DecodeBase64(Tcl_Interp *interp, const char *string, size_t length,
			 DBuffer *destPtr)
{
    unsigned char *bp;

    bp = Blt_Base64_Decode(interp, string, &length);	
    if (bp == NULL) {
	return TCL_ERROR;
    }
    if (destPtr->bytes != NULL) {
	Blt_Free(destPtr->bytes);
    }
    destPtr->bytes = bp;
    destPtr->nBytes = destPtr->count = length;
    destPtr->cursor = 0;
    destPtr->chunk = 64;
    return TCL_OK;
}


char *
Blt_DBuffer_EncodeBase64(
    Tcl_Interp *interp,		/* Interpreter to report errors to. */
    DBuffer *srcPtr)		/* Input binary buffer. */
{
    return Blt_Base64_Encode(interp, srcPtr->bytes, srcPtr->count);
}

