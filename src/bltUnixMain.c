
/*
 * bltUnixMain.c --
 *
 * Provides a default version of the Tcl_AppInit procedure for
 * use in wish and similar Tk-based applications.
 *
 *	Copyright 1998-2004 George A Howlett.
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
 *
 * This file was adapted from the Tk distribution.
 *
 *	Copyright (c) 1993 The Regents of the University of
 *	California. All rights reserved.
 *
 *	Permission is hereby granted, without written agreement and
 *	without license or royalty fees, to use, copy, modify, and
 *	distribute this software and its documentation for any
 *	purpose, provided that the above copyright notice and the
 *	following two paragraphs appear in all copies of this
 *	software.
 *
 *	IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
 *	ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
 *	CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS
 *	SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 *	CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
 *	WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *	PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
 *	BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO *
 *	PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 *	MODIFICATIONS.
 *
 */

#include <blt.h>
#include <tcl.h>
#ifndef TCL_ONLY
#include <tk.h>
#endif
#include "config.h"
/*
 * The following variable is a special hack that is needed in order for
 * Sun shared libraries to be used for Tcl.
 */

#ifdef NEED_MATHERR
BLT_EXTERN int matherr();
int *tclDummyMathPtr = (int *)matherr;
#endif


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
BLT_EXTERN Tcl_AppInitProc Blt_PicturePhotoInit;
BLT_EXTERN Tcl_AppInitProc Blt_PicturePngInit;
BLT_EXTERN Tcl_AppInitProc Blt_PicturePsInit;
BLT_EXTERN Tcl_AppInitProc Blt_PictureTifInit;
BLT_EXTERN Tcl_AppInitProc Blt_PictureXbmInit;
BLT_EXTERN Tcl_AppInitProc Blt_PictureXpmInit;
#endif /* TCL_ONLY */

/* Data table format packages. */
BLT_EXTERN Tcl_AppInitProc Blt_DataTable_CsvInit;
#ifdef HAVE_LIBMYSQL
BLT_EXTERN Tcl_AppInitProc Blt_DataTable_MysqlInit;
#endif	/* HAVE_LIBMYSQL */
BLT_EXTERN Tcl_AppInitProc Blt_DataTable_TreeInit;
BLT_EXTERN Tcl_AppInitProc Blt_DataTable_VectorInit;
#ifdef HAVE_LIBEXPAT
BLT_EXTERN Tcl_AppInitProc Blt_DataTable_XmlInit;
#endif

/* Tree format packages. */
#ifdef HAVE_LIBEXPAT
BLT_EXTERN Tcl_AppInitProc Blt_TreeXmlInit;
#endif

#endif /* STATIC_PKGS */

static int
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
    if (Blt_DataTable_CsvInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }

    /* Data table packages. */
    Tcl_StaticPackage(interp, "blt_datatable_csv", Blt_DataTable_CsvInit, 
	Blt_DataTable_CsvInit);

#ifdef HAVE_LIBMYSQL
    if (Blt_DataTable_MysqlInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_datatable_mysql", Blt_DataTable_MysqlInit, 
	Blt_DataTable_MysqlInit);
#endif	/* HAVE_LIBMYSQL */

    if (Blt_DataTable_TreeInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_datatable_tree", Blt_DataTable_TreeInit, 
	Blt_DataTable_TreeInit);

    if (Blt_DataTable_VectorInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_datatable_vector", Blt_DataTable_VectorInit,
	Blt_DataTable_VectorInit);

#ifdef HAVE_LIBEXPAT
    if (Blt_DataTable_XmlInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "blt_datatable_xml", Blt_DataTable_XmlInit, 
	Blt_DataTable_XmlInit);
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
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * main --
 *
 *	This is the main program for the application.
 *
 * Results:
 *	None: Tk_Main never returns here, so this procedure never
 *	returns either.
 *
 * Side effects:
 *	Whatever the application does.
 *
 *---------------------------------------------------------------------------
 */
int
main(int argc, char **argv)
{
#ifdef TCL_ONLY
    Tcl_Main(argc, argv, Initialize);
#else 
    Tk_Main(argc, argv, Initialize);
#endif
    return 0;			/* Needed only to prevent compiler warning. */
}

