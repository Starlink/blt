
/*
 * bltDBuffer.h --
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

#ifndef _BLT_DBUFFER_H
#define _BLT_DBUFFER_H

typedef struct _Blt_DBuffer {
    unsigned char *bytes;	/* Stores output (malloc-ed).*/
    size_t nBytes;		/* Size of dynamically allocated buffer. */

    size_t count;		/* # of bytes read into the buffer. Marks the
				 * # current fill point of the buffer. */
    size_t cursor;		/* Current position in buffer. */
    size_t chunk;		/* Buffer growth size. */

} *Blt_DBuffer;


BLT_EXTERN void Blt_DBuffer_VarAppend TCL_VARARGS(Blt_DBuffer, buffer);

BLT_EXTERN void Blt_DBuffer_Print TCL_VARARGS(Blt_DBuffer, buffer);

BLT_EXTERN void Blt_DBuffer_Init(Blt_DBuffer buffer);
BLT_EXTERN void Blt_DBuffer_Free(Blt_DBuffer buffer);
BLT_EXTERN unsigned char *Blt_DBuffer_Extend(Blt_DBuffer buffer, size_t extra);
BLT_EXTERN int Blt_DBuffer_AppendData(Blt_DBuffer buffer, 
	const unsigned char *bytes, size_t extra);
BLT_EXTERN int Blt_DBuffer_Resize(Blt_DBuffer buffer, size_t length);
BLT_EXTERN Blt_DBuffer Blt_DBuffer_Create(void);
BLT_EXTERN void Blt_DBuffer_Destroy(Blt_DBuffer buffer);

BLT_EXTERN int Blt_DBuffer_LoadFile(Tcl_Interp *interp, const char *fileName, 
	Blt_DBuffer buffer); 
BLT_EXTERN int Blt_DBuffer_SaveFile(Tcl_Interp *interp, const char *fileName, 
	Blt_DBuffer buffer); 

BLT_EXTERN void Blt_DBuffer_AppendByte(Blt_DBuffer buffer, unsigned char byte);
BLT_EXTERN void Blt_DBuffer_AppendShort(Blt_DBuffer buffer, 
	unsigned short value);
BLT_EXTERN void Blt_DBuffer_AppendLong(Blt_DBuffer buffer, unsigned int value);
BLT_EXTERN Tcl_Obj *Blt_DBuffer_ByteArrayObj(Blt_DBuffer buffer);
BLT_EXTERN Tcl_Obj *Blt_DBuffer_StringObj(Blt_DBuffer buffer);

#define Blt_DBuffer_Bytes(s)		((s)->bytes)
#define Blt_DBuffer_Size(s)		((s)->nBytes)

#define Blt_DBuffer_BytesLeft(s)	((s)->count - (s)->cursor)
#define Blt_DBuffer_NextByte(s)		((s)->bytes[(s)->cursor++])
#define Blt_DBuffer_Pointer(s)		((s)->bytes + (s)->cursor)
#define Blt_DBuffer_SetPointer(s,p)	((s)->cursor = (p) - (s)->bytes)

#define Blt_DBuffer_ResetCursor(s)	((s)->cursor = 0)
#define Blt_DBuffer_Cursor(s)		((s)->cursor)
#define Blt_DBuffer_SetCursor(s,n)	((s)->cursor = (n))
#define Blt_DBuffer_IncrCursor(s,i)	((s)->cursor += (i))

#define Blt_DBuffer_End(s)		((s)->bytes + (s)->count)
#define Blt_DBuffer_Length(s)		((s)->count)
#define Blt_DBuffer_SetLengthFromPointer(s,p) \
	((s)->count = ((p) - (s)->bytes))
#define Blt_DBuffer_SetLength(s,i)    \
	((s)->count = (i), (s)->bytes[(s)->count] = '\0')
#define Blt_DBuffer_IncrLength(s,i)	((s)->count += (i))

BLT_EXTERN int Blt_DBuffer_DecodeBase64(Tcl_Interp *interp, 
	const char *string, size_t length, Blt_DBuffer buffer);
BLT_EXTERN char *Blt_DBuffer_EncodeBase64(Tcl_Interp *interp, 
	Blt_DBuffer buffer);

BLT_EXTERN int Blt_IsBase64(const unsigned char *bytes, size_t length);

#endif /*_BLT_DBUFFER_H*/
