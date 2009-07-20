
/*
 * bltMacOSX.h --
 *
 *	Copyright 2004 George A Howlett.
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

#ifndef _BLT_MACOSX_H
#define _BLT_MACOSX_H

#ifdef MACOSX
#define NO_CUTBUFFER	1
#define NO_DND		1
#define NO_BUSY		1
#define NO_CONTAINER	1
#endif
/* #define NO_TED */

#define XFlush(display)
#define XFree(data)     Blt_Free(data)


#include <sys/cdefs.h>

#ifdef __BIG_ENDIAN__
#define WORDS_BIGENDIAN 1
#else /* !__BIG_ENDIAN__ */
/* #undef WORDS_BIGENDIAN */
#endif /* __BIG_ENDIAN__ */

#ifdef __LP64__
#define SIZEOF_FLOAT 	8
#define SIZEOF_LONG 	8
#define SIZEOF_INT 	4
#define SIZEOF_LONG_LONG 8
#define SIZEOF_VOID_P 	8
#else /* !__LP64__ */
#define SIZEOF_FLOAT 	4
#define SIZEOF_LONG 	4
#define SIZEOF_INT 	4
#define SIZEOF_LONG_LONG 8
#define SIZEOF_VOID_P 	4
#endif /* __LP64__ */

#if defined (__i386__) || defined(__x86_64__)
#define HAVE_X86 1
#endif

#endif /*_BLT_MACOSX_H*/
