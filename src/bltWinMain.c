
/*
 * bltWinMain.c --
 *
 * Main entry point for wish and other Tk-based applications.
 *
 *	Copyright 1998-2004 George A Howlett.
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
 * This file was adapted from the Tk library distribution.
 *
 *	Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 *	See the file "license.terms" for information on usage and
 *	redistribution * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "config.h"
#undef USE_TK_STUBS
#undef USE_TCL_STUBS
#include "blt.h"
#include <tcl.h>
#include <tk.h>
#include <locale.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#define _VERSION(a,b,c)	    (((a) << 16) + ((b) << 8) + (c))

#define _TCL_VERSION _VERSION(TCL_MAJOR_VERSION, TCL_MINOR_VERSION, TCL_RELEASE_SERIAL)
#define _TK_VERSION _VERSION(TK_MAJOR_VERSION, TK_MINOR_VERSION, TK_RELEASE_SERIAL)

#ifdef WIN32
#  define STRICT
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  undef STRICT
#  undef WIN32_LEAN_AND_MEAN
#  include <windowsx.h>
#endif /* WIN32 */

#define vsnprintf		_vsnprintf

/*
 * Forward declarations for procedures defined later in this file:
 */

static void setargv(int *argcPtr, char ***argvPtr);

#ifndef TCL_ONLY
#if (_TCL_VERSION >= _VERSION(8,2,0)) 
static BOOL consoleRequired = TRUE;
#endif
#endif

static Tcl_AppInitProc Initialize;
#ifndef TCL_ONLY
static Tcl_PanicProc WishPanic;
#endif

#if (_TK_VERSION < _VERSION(8,2,0)) 
/*
 * The following declarations refer to internal Tk routines.  These interfaces
 * are available for use, but are not supported.
 */
extern void Blt_ConsoleCreate(void);
extern int Blt_ConsoleInit(Tcl_Interp *interp);

#endif /* _TK_VERSION < 8.2.0 */


/*
 *---------------------------------------------------------------------------
 *
 * setargv --
 *
 *	Parse the Windows command line string into argc/argv.  Done here
 *	because we don't trust the builtin argument parser in crt0.  Windows
 *	applications are responsible for breaking their command line into
 *	arguments.
 *
 *	2N backslashes + quote -> N backslashes + begin quoted string
 *	2N + 1 backslashes + quote -> literal
 *	N backslashes + non-quote -> literal
 *	quote + quote in a quoted string -> single quote
 *	quote + quote not in quoted string -> empty string
 *	quote -> begin quoted string
 *
 * Results:
 *	Fills argcPtr with the number of arguments and argvPtr with the
 *	array of arguments.
 *
 * Side effects:
 *	Memory allocated.
 *
 *---------------------------------------------------------------------------
 */

static void
setargv(
    int *argcPtr,		/* Filled with number of argument strings. */
    char ***argvPtr)
{				/* Filled with argument strings (malloc'd). */
    char *cmdLine, *p, *arg, *argSpace;
    char **argv;
    int argc, size, inquote, copy, slashes;

    cmdLine = GetCommandLine();	/* INTL: BUG */

    /*
     * Precompute an overly pessimistic guess at the number of arguments in
     * the command line by counting non-space spans.
     */

    size = 2;
    for (p = cmdLine; *p != '\0'; p++) {
	if ((*p == ' ') || (*p == '\t')) {	/* INTL: ISO space. */
	    size++;
	    while ((*p == ' ') || (*p == '\t')) { /* INTL: ISO space. */
		p++;
	    }
	    if (*p == '\0') {
		break;
	    }
	}
    }
    argSpace = (char *)Tcl_Alloc(
	(unsigned)(size * sizeof(char *) + strlen(cmdLine) + 1));
    argv = (char **)argSpace;
    argSpace += size * sizeof(char *);
    size--;

    p = cmdLine;
    for (argc = 0; argc < size; argc++) {
	argv[argc] = arg = argSpace;
	while ((*p == ' ') || (*p == '\t')) {	/* INTL: ISO space. */
	    p++;
	}
	if (*p == '\0') {
	    break;
	}
	inquote = 0;
	slashes = 0;
	while (1) {
	    copy = 1;
	    while (*p == '\\') {
		slashes++;
		p++;
	    }
	    if (*p == '"') {
		if ((slashes & 1) == 0) {
		    copy = 0;
		    if ((inquote) && (p[1] == '"')) {
			p++;
			copy = 1;
		    } else {
			inquote = !inquote;
		    }
		}
		slashes >>= 1;
	    }
	    while (slashes) {
		*arg = '\\';
		arg++;
		slashes--;
	    }

	    if ((*p == '\0') || (!inquote && ((*p == ' ') || 
	      (*p == '\t')))) {	/* INTL: ISO space. */
		break;
	    }
	    if (copy != 0) {
		*arg = *p;
		arg++;
	    }
	    p++;
	}
	*arg = '\0';
	argSpace = arg + 1;
    }
    argv[argc] = NULL;

    *argcPtr = argc;
    *argvPtr = argv;
}


/*
 *---------------------------------------------------------------------------
 *
 * Initialize --
 *
 *	This procedure performs application-specific initialization.  Most
 *	applications, especially those that incorporate additional packages,
 *	will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard TCL completion code, and leaves an error message in
 *	the interp's result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *---------------------------------------------------------------------------
 */

BLT_EXTERN Tcl_AppInitProc Blt_core_Init;
BLT_EXTERN Tcl_AppInitProc Blt_core_SafeInit;

#ifndef TCL_ONLY
BLT_EXTERN Tcl_AppInitProc Blt_x_Init;
BLT_EXTERN Tcl_AppInitProc Blt_x_SafeInit;
#endif

#ifdef STATIC_PKGS

/* Picture format packages. */
#ifndef TCL_ONLY
BLT_EXTERN Tcl_AppInitProc Blt_PictureBmpInit;
BLT_EXTERN Tcl_AppInitProc Blt_PictureGifInit;
BLT_EXTERN Tcl_AppInitProc Blt_PictureJpgInit;
BLT_EXTERN Tcl_AppInitProc Blt_PicturePbmInit;
BLT_EXTERN Tcl_AppInitProc Blt_PicturePdfInit;
BLT_EXTERN Tcl_AppInitProc Blt_PicturePhotoInit;
BLT_EXTERN Tcl_AppInitProc Blt_PicturePngInit;
BLT_EXTERN Tcl_AppInitProc Blt_PicturePsInit;
BLT_EXTERN Tcl_AppInitProc Blt_PictureTifInit;
BLT_EXTERN Tcl_AppInitProc Blt_PictureXbmInit;
BLT_EXTERN Tcl_AppInitProc Blt_PictureXpmInit;
#endif /* TCL_ONLY */

/* Data table format packages. */
BLT_EXTERN Tcl_AppInitProc Blt_Table_CsvInit;
#ifdef HAVE_LIBMYSQL
BLT_EXTERN Tcl_AppInitProc Blt_Table_MysqlInit;
#endif	/* HAVE_LIBMYSQL */
BLT_EXTERN Tcl_AppInitProc Blt_Table_TreeInit;
BLT_EXTERN Tcl_AppInitProc Blt_Table_VectorInit;
#ifdef HAVE_LIBEXPAT
BLT_EXTERN Tcl_AppInitProc Blt_Table_XmlInit;
#endif

/* Tree format packages. */
#ifdef HAVE_LIBEXPAT
BLT_EXTERN Tcl_AppInitProc Blt_TreeXmlInit;
#endif

#endif /* STATIC_PKGS */

int
Initialize(Tcl_Interp *interp)	/* Interpreter for application. */
{
#ifdef TCLLIBPATH
    /* 
     * It seems that some distributions of TCL don't compile-in a
     * default location of the library.  This causes Tcl_Init to fail
     * if bltwish and bltsh are moved to another directory. The
     * workaround is to set the magic variable "tclDefaultLibrary".
     */
    Tcl_SetVar(interp, "tclDefaultLibrary", TCLLIBPATH, TCL_GLOBAL_ONLY);
#endif /* TCLLIBPATH */
    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    /*
     * Call the init procedures for included packages.  Each call should
     * look like this:
     *
     * if (Mod_Init(interp) == TCL_ERROR) {
     *     return TCL_ERROR;
     * }
     *
     * where "Mod" is the name of the module.
     */
    if (Blt_core_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_core", Blt_core_Init, Blt_core_SafeInit);

#ifdef STATIC_PKGS
    /* Tcl-only static packages */
    if (Blt_Table_CsvInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }

    /* Data table packages. */
    Tcl_StaticPackage(interp, "blt_datatable_csv", Blt_Table_CsvInit, 
	Blt_Table_CsvInit);

#ifdef HAVE_LIBMYSQL
    if (Blt_Table_MysqlInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_datatable_mysql", Blt_Table_MysqlInit, 
	Blt_Table_MysqlInit);
#endif	/* HAVE_LIBMYSQL */

    if (Blt_Table_TreeInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_datatable_tree", Blt_Table_TreeInit, 
	Blt_Table_TreeInit);

    if (Blt_Table_VectorInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_datatable_vector", Blt_Table_VectorInit,
	Blt_Table_VectorInit);

#ifdef HAVE_LIBEXPAT
    if (Blt_Table_XmlInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_datatable_xml", Blt_Table_XmlInit, 
	Blt_Table_XmlInit);
#endif	/* HAVE_LIBEXPAT */

    /* Tree packages. */
#ifdef HAVE_LIBEXPAT
    if (Blt_TreeXmlInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_tree_xml", Blt_TreeXmlInit, Blt_TreeXmlInit);
#endif	/* HAVE_LIBEXPAT */

#endif /* STATIC_PKGS */

#ifndef TCL_ONLY
    if (Tk_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    if (Blt_x_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_extra", Blt_x_Init, Blt_x_SafeInit);

#ifdef STATIC_PKGS

    /* Picture packages. */

    if (Blt_PictureBmpInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_picture_bmp", Blt_PictureBmpInit, 
	Blt_PictureBmpInit);

    if (Blt_PictureGifInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_picture_gif", Blt_PictureGifInit, 
	Blt_PictureGifInit);

#ifdef HAVE_LIBJPG
    if (Blt_PictureJpgInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_picture_jpg", Blt_PictureJpgInit, 
		      Blt_PictureJpgInit);
#endif /*HAVE_LIBJPG*/

    if (Blt_PicturePbmInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_picture_pbm", Blt_PicturePbmInit, 
	Blt_PicturePbmInit);

    if (Blt_PicturePhotoInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_picture_photo", Blt_PicturePhotoInit, 
	Blt_PicturePhotoInit);

#ifdef HAVE_LIBPNG
    if (Blt_PicturePngInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_picture_png", Blt_PicturePngInit, 
	Blt_PicturePngInit);
#endif /*HAVE_LIBPNG*/

    if (Blt_PicturePsInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_picture_ps", Blt_PicturePsInit, 
	Blt_PicturePsInit);

    if (Blt_PicturePdfInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_picture_pdf", Blt_PicturePdfInit, 
	Blt_PicturePdfInit);

#ifdef HAVE_LIBTIF
    if (Blt_PictureTifInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_picture_tif", Blt_PictureTifInit, 
		      Blt_PictureTifInit);
#endif /*HAVE_LIBTIF*/

    if (Blt_PictureXbmInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_picture_xbm", Blt_PictureXbmInit, 
	Blt_PictureXbmInit);

#ifdef HAVE_LIBXPM
    if (Blt_PictureXpmInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_picture_xpm", Blt_PictureXpmInit, 
	Blt_PictureXpmInit);
#endif /*HAVE_LIBXPM*/

#endif /* STATIC_PKGS */
#endif /*TCL_ONLY*/
    /*
     * Specify a user-specific startup file to invoke if the application
     * is run interactively.  Typically the startup file is "~/.apprc"
     * where "app" is the name of the application.  If this line is deleted
     * then no user-specific startup file will be run under any conditions.
     */
#ifdef TCL_ONLY
    Tcl_SetVar(interp, "tcl_rcFileName", "~/tclshrc.tcl", TCL_GLOBAL_ONLY);
#else 
    Tcl_SetVar(interp, "tcl_rcFileName", "~/wishrc.tcl", TCL_GLOBAL_ONLY);
#endif

#ifndef TCL_ONLY
#if (_TCL_VERSION >= _VERSION(8,2,0)) 
    if (consoleRequired) {
	if (Tk_CreateConsoleWindow(interp) == TCL_ERROR) {
	    goto error;
	}
    }
#else
    /*
     * Initialize the console only if we are running as an interactive
     * application.
     */
    if (Blt_ConsoleInit(interp) == TCL_ERROR) {
	goto error;
    }
#endif	/* _TCL_VERSION >= 8.2.0 */
#endif	/* TCL_ONLY */
    return TCL_OK;
#ifndef TCL_ONLY
  error:
    WishPanic(Tcl_GetStringResult(interp));
#endif
    return TCL_ERROR;
}

#ifdef TCL_ONLY
/*
 *---------------------------------------------------------------------------
 *
 * main --
 *
 *	This is the main program for the application.
 *
 * Results:
 *	None: Tcl_Main never returns here, so this procedure never returns
 *	either.
 *
 * Side effects:
 *	Whatever the application does.
 *
 *---------------------------------------------------------------------------
 */
int
main(argc, argv)
    int argc;			/* Number of command-line arguments. */
    char **argv;		/* Values of command-line arguments. */
{
    char buffer[MAX_PATH +1];
    char *p;

    /*
     * Set up the default locale to be standard "C" locale so parsing is
     * performed correctly.
     */

    setlocale(LC_ALL, "C");
    setargv(&argc, &argv);

    /*
     * Replace argv[0] with full pathname of executable, and forward slashes
     * substituted for backslashes.
     */

    GetModuleFileName(NULL, buffer, sizeof(buffer));
    argv[0] = buffer;
    for (p = buffer; *p != '\0'; p++) {
	if (*p == '\\') {
	    *p = '/';
	}
    }
    Tcl_Main(argc, argv, Initialize);
    return 0;			/* Needed only to prevent compiler warning. */
}

#else /* TCL_ONLY */


/*
 *---------------------------------------------------------------------------
 *
 * WishPanic --
 *
 *	Display a message and exit.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Exits the program.
 *
 *---------------------------------------------------------------------------
 */

static void
WishPanic 
TCL_VARARGS_DEF(const char *, arg1)
{
    va_list argList;
    char buf[1024];
    const char *format;

    format = TCL_VARARGS_START(char *, arg1, argList);
    vsnprintf(buf, 1024, format, argList);
    buf[1023] = '\0';
    MessageBeep(MB_ICONEXCLAMATION);
    MessageBox(NULL, buf, "Fatal Error in Wish",
	MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
#if defined(_MSC_VER) || defined(__BORLANDC__)
    DebugBreak();
#ifdef notdef			/* Panics shouldn't cause exceptions.  Simply
				 * let the program exit. */
    _asm {
	int 3
    }
#endif
#endif /* _MSC_VER || __BORLANDC__ */
    ExitProcess(1);
}

/*
 *---------------------------------------------------------------------------
 *
 * WinMain --
 *
 *	Main entry point from Windows.
 *
 * Results:
 *	Returns false if initialization fails, otherwise it never returns.
 *
 * Side effects:
 *	Just about anything, since from here we call arbitrary TCL code.
 *
 *---------------------------------------------------------------------------
 */

int APIENTRY
WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine,
    int nCmdShow)
{
    char **argv;
    int argc;

    Tcl_SetPanicProc(WishPanic);

    /*
     * Set up the default locale to be standard "C" locale so parsing is
     * performed correctly.
     */

    setlocale(LC_ALL, "C");
    setargv(&argc, &argv);

    /*
     * Increase the application queue size from default value of 8.  At the
     * default value, cross application SendMessage of WM_KILLFOCUS will fail
     * because the handler will not be able to do a PostMessage!  This is only
     * needed for Windows 3.x, since NT dynamically expands the queue.
     */

    SetMessageQueue(64);

    /*
     * Create the console channels and install them as the standard channels.
     * All I/O will be discarded until Blt_ConsoleInit is called to attach the
     * console to a text widget.
     */
#if (_TCL_VERSION >= _VERSION(8,2,0)) 
    consoleRequired = TRUE;
#else
    Blt_ConsoleCreate();
#endif
    Tk_Main(argc, argv, Initialize);
    return 1;
}

#endif /* TCL_ONLY */


