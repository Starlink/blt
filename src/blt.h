
/*
 * blt.h --
 *
 *	Copyright 1991-2004 George A Howlett.
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

#ifndef _BLT_H
#define _BLT_H

#define BLT_MAJOR_VERSION 	3
#define BLT_MINOR_VERSION 	0
#define BLT_VERSION		"3.0"
#define BLT_PATCH_LEVEL		"3.0a"
#define BLT_RELEASE_SERIAL	0

#define BLT_STORAGE_CLASS	

#ifdef __cplusplus
#  define BLT_EXTERN BLT_STORAGE_CLASS extern "C" 
#else
#  define BLT_EXTERN BLT_STORAGE_CLASS extern 
#endif	/* __cplusplus */

#define _VERSION(a,b,c)	    (((a) << 16) + ((b) << 8) + (c))

#ifndef _ANSI_ARGS_
#   define _ANSI_ARGS_(x)       ()
#endif

#endif /*_BLT_H*/
