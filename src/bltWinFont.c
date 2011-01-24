
/*
 * bltWinFont.c --
 *
 * This module implements rotated fonts for the BLT toolkit.
 *
 *	Copyright 2005 George A Howlett.
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
#include <bltHash.h>
#include "tkDisplay.h"
#include "tkFont.h"

#include "bltFont.h"

#define WINDEBUG		0
#define DEBUG_FONT_SELECTION	1
#define DEBUG_FONT_SELECTION2	0

typedef struct _Blt_Font _Blt_Font;

enum FontTypes { 
    FONT_UNKNOWN, 		/* Unknown font type. */
    FONT_TK, 			/* Normal Tk font. */
    FONT_WIN 			/* Windows font. */
};

#ifndef HAVE_LIBXFT
#define FC_WEIGHT_THIN		    0
#define FC_WEIGHT_EXTRALIGHT	    40
#define FC_WEIGHT_ULTRALIGHT	    FC_WEIGHT_EXTRALIGHT
#define FC_WEIGHT_LIGHT		    50
#define FC_WEIGHT_BOOK		    75
#define FC_WEIGHT_REGULAR	    80
#define FC_WEIGHT_NORMAL	    FC_WEIGHT_REGULAR
#define FC_WEIGHT_MEDIUM	    100
#define FC_WEIGHT_DEMIBOLD	    180
#define FC_WEIGHT_SEMIBOLD	    FC_WEIGHT_DEMIBOLD
#define FC_WEIGHT_BOLD		    200
#define FC_WEIGHT_EXTRABOLD	    205
#define FC_WEIGHT_ULTRABOLD	    FC_WEIGHT_EXTRABOLD
#define FC_WEIGHT_BLACK		    210
#define FC_WEIGHT_HEAVY		    FC_WEIGHT_BLACK
#define FC_WEIGHT_EXTRABLACK	    215
#define FC_WEIGHT_ULTRABLACK	    FC_WEIGHT_EXTRABLACK

#define FC_SLANT_ROMAN		    0
#define FC_SLANT_ITALIC		    100
#define FC_SLANT_OBLIQUE	    110

#define FC_WIDTH_ULTRACONDENSED	    50
#define FC_WIDTH_EXTRACONDENSED	    63
#define FC_WIDTH_CONDENSED	    75
#define FC_WIDTH_SEMICONDENSED	    87
#define FC_WIDTH_NORMAL		    100
#define FC_WIDTH_SEMIEXPANDED	    113
#define FC_WIDTH_EXPANDED	    125
#define FC_WIDTH_EXTRAEXPANDED	    150
#define FC_WIDTH_ULTRAEXPANDED	    200

#define FC_PROPORTIONAL		    0
#define FC_DUAL			    90
#define FC_MONO			    100
#define FC_CHARCELL		    110

#define FC_ANTIALIAS	    "antialias"		/* Bool (depends) */
#define FC_AUTOHINT	    "autohint"		/* Bool (false) */
#define FC_DECORATIVE	    "decorative"	/* Bool  */
#define FC_EMBEDDED_BITMAP  "embeddedbitmap"	/* Bool  */
#define FC_EMBOLDEN	    "embolden"		/* Bool */
#define FC_FAMILY	    "family"		/* String */
#define FC_GLOBAL_ADVANCE   "globaladvance"	/* Bool (true) */
#define FC_HINTING	    "hinting"		/* Bool (true) */
#define FC_MINSPACE	    "minspace"		/* Bool */
#define FC_OUTLINE	    "outline"		/* Bool */
#define FC_SCALABLE	    "scalable"		/* Bool */
#define FC_SIZE		    "size"		/* Double */
#define FC_SLANT	    "slant"		/* Int */
#define FC_SPACING	    "spacing"		/* Int */
#define FC_STYLE	    "style"		/* String */
#define FC_VERTICAL_LAYOUT  "verticallayout"	/* Bool (false) */
#define FC_WEIGHT	    "weight"		/* Int */
#define FC_WIDTH	    "width"		/* Int */

#endif

#ifndef FC_WEIGHT_EXTRABLACK
#define FC_WEIGHT_EXTRABLACK	    215
#define FC_WEIGHT_ULTRABLACK	    FC_WEIGHT_EXTRABLACK
#endif

typedef struct {
    char *family;
    const char *weight;
    const char *slant;
    const char *width;
    const char *spacing;
    int size;			/* If negative, pixels, else points */
} FontPattern;

typedef struct {
    const char *name;
    int minChars;
    const char *key;
    int value;
    const char *oldvalue;
} FontSpec;
    
static FontSpec fontSpecs[] = {
    { "black",        2, FC_WEIGHT,  FC_WEIGHT_BLACK,     "*"},
    { "bold",         3, FC_WEIGHT,  FC_WEIGHT_BOLD,      "bold"},
    { "book",         3, FC_WEIGHT,  FC_WEIGHT_MEDIUM,	  "medium"},
    { "charcell",     2, FC_SPACING, FC_CHARCELL,	  "c"},
    { "condensed",    2, FC_WIDTH,   FC_WIDTH_CONDENSED,  "condensed"},
    { "demi",         4, FC_WEIGHT,  FC_WEIGHT_BOLD,      "semi"},
    { "demibold",     5, FC_WEIGHT,  FC_WEIGHT_DEMIBOLD,  "semibold"},
    { "dual",         2, FC_SPACING, FC_DUAL,		  "*"},
    { "i",            1, FC_SLANT,   FC_SLANT_ITALIC,	  "i"},
    { "italic",       2, FC_SLANT,   FC_SLANT_ITALIC,	  "i"},
    { "light",        1, FC_WEIGHT,  FC_WEIGHT_LIGHT,	  "light"},
    { "medium",       2, FC_WEIGHT,  FC_WEIGHT_MEDIUM,	  "medium"},
    { "mono",         2, FC_SPACING, FC_MONO,		  "m"},
    { "normal",       1, FC_WIDTH,   FC_WIDTH_NORMAL,	  "normal"},
    { "o",            1, FC_SLANT,   FC_SLANT_OBLIQUE,	  "o"},
    { "obilque",      2, FC_SLANT,   FC_SLANT_OBLIQUE,	  "o"},
    { "overstrike",   2, NULL,       0,			  "*"},
    { "proportional", 1, FC_SPACING, FC_PROPORTIONAL,	  "p"},
    { "r",            1, FC_SLANT,   FC_SLANT_ROMAN,      "r"},
    { "roman",        2, FC_SLANT,   FC_SLANT_ROMAN,      "r"},
    { "semibold",     5, FC_WEIGHT,  FC_WEIGHT_DEMIBOLD,  "semibold"},
    { "semicondensed",5, FC_WIDTH,   FC_WIDTH_SEMICONDENSED,  "semicondensed"},
    { "underline",    1, NULL,       0,		          "*"},
};
static int nFontSpecs = sizeof(fontSpecs) / sizeof(FontSpec);

static FontSpec weightSpecs[] ={
    { "black",		2, FC_WEIGHT, FC_WEIGHT_BLACK,	    "bold"},
    { "bold",		3, FC_WEIGHT, FC_WEIGHT_BOLD,	    "bold"},
    { "book",		3, FC_WEIGHT, FC_WEIGHT_MEDIUM,	    "*"},
    { "demi",		4, FC_WEIGHT, FC_WEIGHT_BOLD,	    "*"},
    { "demibold",	5, FC_WEIGHT, FC_WEIGHT_DEMIBOLD,   "*"},
    { "extrablack",	6, FC_WEIGHT, FC_WEIGHT_EXTRABLACK, "*"},
    { "extralight",	6, FC_WEIGHT, FC_WEIGHT_EXTRALIGHT, "*"},
    { "heavy",		1, FC_WEIGHT, FC_WEIGHT_HEAVY,      "*"},
    { "light",		1, FC_WEIGHT, FC_WEIGHT_LIGHT,	    "light"},
    { "medium",		1, FC_WEIGHT, FC_WEIGHT_MEDIUM,	    "medium"},
    { "regular",	1, FC_WEIGHT, FC_WEIGHT_REGULAR,    "medium"},
    { "semibold",	1, FC_WEIGHT, FC_WEIGHT_SEMIBOLD,   "semibold"},
    { "thin",		1, FC_WEIGHT, FC_WEIGHT_THIN,       "thin"},
    { "ultrablack",	7, FC_WEIGHT, FC_WEIGHT_ULTRABLACK, "*"},
    { "ultrabold",	7, FC_WEIGHT, FC_WEIGHT_ULTRABOLD,  "*"},
    { "ultralight",	6, FC_WEIGHT, FC_WEIGHT_ULTRALIGHT, "*"},
};
static int nWeightSpecs = sizeof(weightSpecs) / sizeof(FontSpec);

static FontSpec slantSpecs[] ={
    { "i",		1, FC_SLANT, FC_SLANT_ITALIC,	"i"},
    { "italic",		2, FC_SLANT, FC_SLANT_ITALIC,	"i"},
    { "o",		1, FC_SLANT, FC_SLANT_OBLIQUE,	"o"},
    { "obilque",	3, FC_SLANT, FC_SLANT_OBLIQUE,	"o"},
    { "r",		1, FC_SLANT, FC_SLANT_ROMAN,	"r"},
    { "roman",		2, FC_SLANT, FC_SLANT_ROMAN,	"r"},
};
static int nSlantSpecs = sizeof(slantSpecs) / sizeof(FontSpec);

static FontSpec spacingSpecs[] = {
    { "charcell",     2, FC_SPACING, FC_CHARCELL,	  "c"},
    { "dual",         2, FC_SPACING, FC_DUAL,		  "*"},
    { "mono",         2, FC_SPACING, FC_MONO,		  "m"},
    { "proportional", 1, FC_SPACING, FC_PROPORTIONAL,	  "p"},
};
static int nSpacingSpecs = sizeof(spacingSpecs) / sizeof(FontSpec);

#ifdef notdef
static FontSpec widthSpecs[] ={
    { "condensed",	1, FC_WIDTH, FC_WIDTH_CONDENSED,      "condensed"},
    { "expanded",	3, FC_WIDTH, FC_WIDTH_EXPANDED,	      "*"},
    { "extracondensed", 6, FC_WIDTH, FC_WIDTH_EXTRACONDENSED, "*"},
    { "extraexpanded",	6, FC_WIDTH, FC_WIDTH_EXTRAEXPANDED,  "*"},
    { "normal",		1, FC_WIDTH, FC_WIDTH_NORMAL,	      "normal"},
    { "semicondensed",	5, FC_WIDTH, FC_WIDTH_SEMICONDENSED,  "semicondensed"},
    { "semiexpanded",	5, FC_WIDTH, FC_WIDTH_SEMIEXPANDED,   "*"},
    { "ultracondensed",	6, FC_WIDTH, FC_WIDTH_ULTRACONDENSED, "*"},
    { "ultraexpanded",	6, FC_WIDTH, FC_WIDTH_ULTRAEXPANDED,  "*"},
};
static int nWidthSpecs = sizeof(widthSpecs) / sizeof(FontSpec);

static FontSpec boolSpecs[] ={
    { "antialias",	1, FC_ANTIALIAS,	},
    { "decorative",	1, FC_DECORATIVE,	},
    { "embeddedbitmap", 4, FC_EMBEDDED_BITMAP,	},
    { "embolden",	4, FC_EMBOLDEN,		},
    { "globaladvance",	1, FC_GLOBAL_ADVANCE,	},
    { "hinting",	1, FC_HINTING,		},
    { "minspace",	1, FC_MINSPACE,		},
    { "outline",	1, FC_OUTLINE,		},
    { "scalable",	1, FC_SCALABLE,		},
    { "verticallayout", 1, FC_VERTICAL_LAYOUT,	},
};
static int nBoolSpecs = sizeof(boolSpecs) / sizeof(FontSpec);
#endif

static Blt_HashTable fontTable;
static Blt_HashTable aliasTable;
static int initialized = 0;
static void GetFontFamilies(Tk_Window tkwin, Blt_HashTable *tablePtr);

enum XLFDFields { 
    XLFD_FOUNDRY, 
    XLFD_FAMILY, 
    XLFD_WEIGHT, 
    XLFD_SLANT,	
    XLFD_SETWIDTH, 
    XLFD_ADD_STYLE, 
    XLFD_PIXEL_SIZE,
    XLFD_POINT_SIZE, 
    XLFD_RESOLUTION_X, 
    XLFD_RESOLUTION_Y,
    XLFD_SPACING, 
    XLFD_AVERAGE_WIDTH, 
    XLFD_CHARSET,
    XLFD_NUMFIELDS
};

/* 
 * Freetype font container.
 */
typedef struct {
    char *name;			/* Name of the font (malloc-ed). */
    int refCount;		/* Reference count for this structure.  When
				 * refCount reaches zero, it means to free the
				 * resources associated with this
				 * structure. */

    Blt_HashEntry *hashPtr;	/* Pointer to this entry in global font hash
				 * table. Used to remove the entry from the
				 * table. */

    Blt_HashTable fontTable;	/* Hash table containing an Xft font for each
				 * angle it's used at. Will always contain a 0
				 * degree entry. */
    Tk_Font tkfont;		

} RotatedFont;

static double
PointsToPixels(Tk_Window tkwin, int size)
{
    double d;

    if (size < 0) {
        return -size;
    }
    d = size * 25.4 / 72.0;
    d *= WidthOfScreen(Tk_Screen(tkwin));
    d /= WidthMMOfScreen(Tk_Screen(tkwin));
    return d;
}

static double
PixelsToPoints(Tk_Window tkwin, int size)		
{
    double d;

    if (size >= 0) {
	return size;
    }
    d = -size * 72.0 / 25.4;
    d *= WidthMMOfScreen(Tk_Screen(tkwin));
    d /= WidthOfScreen(Tk_Screen(tkwin));
    return d;
}

static void
ParseXLFD(const char *fontName, int *argcPtr, char ***argvPtr)
{
    char *p, *pend, *desc, *buf;
    size_t arrayLen, stringLen;
    int count;
    char **field;

    arrayLen = (sizeof(char *) * (XLFD_NUMFIELDS + 1));
    stringLen = strlen(fontName);
    buf = Blt_AssertCalloc(1, arrayLen + stringLen + 1);
    desc = buf + arrayLen;
    strcpy(desc, fontName);
    field = (char **)buf;

    count = 0;
    for (p = desc, pend = p + stringLen; p < pend; p++, count++) {
	char *word;

	field[count] = NULL;
	/* Get the next word, separated by dashes (-). */
	word = p;
	while ((*p != '\0') && (*p != '-')) {
	    if (((*p & 0x80) == 0) && Tcl_UniCharIsUpper(UCHAR(*p))) {
		*p = (char)Tcl_UniCharToLower(UCHAR(*p));
	    }
	    p++;
	}
	if (*p != '\0') {
	    *p = '\0';
	}
	if ((word[0] == '\0') || 
	    (((word[0] == '*') || (word[0] == '?')) && (word[1] == '\0'))) {
	    continue;		/* Field not specified. -- -*- -?- */
	}
	field[count] = word;
    }

    /*
     * An XLFD of the form -adobe-times-medium-r-*-12-*-* is pretty common,
     * but it is (strictly) malformed, because the first * is eliding both the
     * Setwidth and the Addstyle fields. If the Addstyle field is a number,
     * then assume the above incorrect form was used and shift all the rest of
     * the fields right by one, so the number gets interpreted as a pixelsize.
     * This fix is so that we don't get a million reports that "it works under
     * X (as a native font name), but gives a syntax error under Windows (as a
     * parsed set of attributes)".
     */

    if ((count > XLFD_ADD_STYLE) && (field[XLFD_ADD_STYLE] != NULL)) {
	int dummy;

	if (Tcl_GetInt(NULL, field[XLFD_ADD_STYLE], &dummy) == TCL_OK) {
	    int j;
	    
	    for (j = XLFD_NUMFIELDS - 1; j >= XLFD_ADD_STYLE; j--) {
		field[j + 1] = field[j];
	    }
	    field[XLFD_ADD_STYLE] = NULL;
	    count++;
	}
    }
    *argcPtr = count;
    *argvPtr = field;

    field[XLFD_NUMFIELDS] = NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * SearchForFontSpec --
 *
 *      Performs a binary search on the array of font specification to find a
 *      partial, anchored match for the given option string.
 *
 * Results:
 *	If the string matches unambiguously the index of the specification in
 *	the array is returned.  If the string does not match, even as an
 *	abbreviation, any operation, -1 is returned.  If the string matches,
 *	but ambiguously -2 is returned.
 *
 *---------------------------------------------------------------------------
 */
static int
SearchForFontSpec(FontSpec *table, int nSpecs, const char *string, int length)
{
    char c;
    int high, low;

    low = 0;
    high = nSpecs - 1;
    c = tolower((unsigned char)string[0]);
    while (low <= high) {
	FontSpec *sp;
	int compare;
	int median;
	
	median = (low + high) >> 1;
	sp = table + median;

	/* Test the first character */
	compare = c - sp->name[0];
	if (compare == 0) {
	    /* Now test the entire string */
	    compare = strncasecmp(string, sp->name, length);
	    if (compare == 0) {
		if ((int)length < sp->minChars) {
		    return -2;	/* Ambiguous spec name */
		}
	    }
	}
	if (compare < 0) {
	    high = median - 1;
	} else if (compare > 0) {
	    low = median + 1;
	} else {
	    return median;	/* Spec found. */
	}
    }
    return -1;			/* Can't find spec */
}

static FontSpec *
FindSpec(FontSpec *tablePtr, int nSpecs, const char *string, int length)
{
    int n;
    
    n = SearchForFontSpec(tablePtr, nSpecs, string, length);
    if (n < 0) {
	if (n == -1) {
	    fprintf(stderr, "unknown %s specification \"%s\"\n", 
		    tablePtr[n].key, string);
	}
	if (n == -2) {
	    fprintf(stderr, "ambiguous %s specification \"%s\"\n", 
		    tablePtr[n].key, string);
	}
	return NULL;
    }
    return tablePtr + n;
}


typedef struct {
    const char *name, *aliases[10];
} FontAlias;

static FontAlias xlfdFontAliases[] = {
    { "math",		{"mathematica1", "courier"}},
    { "serif",		{"times"}},
    { "sans serif",	{ "arial" }},
    { "monospace",	{ "courier new" }},
    { NULL }
};

static void
MakeAliasTable(Tk_Window tkwin)
{
    Blt_HashTable familyTable;
    FontAlias *fp;
    FontAlias *table;

    Blt_InitHashTable(&familyTable, TCL_STRING_KEYS);
    GetFontFamilies(tkwin, &familyTable);
    Blt_InitHashTable(&aliasTable, TCL_STRING_KEYS);
    table = xlfdFontAliases;
    for(fp = table; fp->name != NULL; fp++) {
	Blt_HashEntry *hPtr;
	const char **alias;
	   
	for (alias = fp->aliases; *alias != NULL; alias++) {
	    hPtr = Blt_FindHashEntry(&familyTable, *alias);
	    if (hPtr != NULL) {
		int isNew;
		
		hPtr = Blt_CreateHashEntry(&aliasTable, fp->name, &isNew);
		Blt_SetHashValue(hPtr, *alias);
		break;
	    }
	}
    }
    Blt_DeleteHashTable(&familyTable);
}

static const char *
GetAlias(const char *family)
{
    Blt_HashEntry *hPtr;

    strtolower((char *)family);
    hPtr = Blt_FindHashEntry(&aliasTable, family);
    if (hPtr != NULL) {
	return Blt_GetHashValue(hPtr);
    }
    return family;
}

static Blt_NameOfFontProc		TkNameOfFontProc;
static Blt_GetFontMetricsProc		TkGetFontMetricsProc;
static Blt_FontIdProc			TkFontIdProc;
static Blt_MeasureCharsProc		TkMeasureCharsProc;
static Blt_TextStringWidthProc		TkTextStringWidthProc;
static Blt_FreeFontProc			TkFreeFontProc;
static Blt_DrawCharsProc		TkDrawCharsProc;
static Blt_PostscriptFontNameProc	TkPostscriptFontNameProc;
static Blt_FamilyOfFontProc		TkFamilyOfFontProc;
static Blt_CanRotateFontProc		TkCanRotateFontProc;
static Blt_UnderlineCharsProc		TkUnderlineCharsProc;

static Blt_FontClass tkFontClass = {
    FONT_TK,
    TkNameOfFontProc,		/* Blt_NameOfFontProc */
    TkFamilyOfFontProc,		/* Blt_FamilyOfFontProc */
    TkFontIdProc,		/* Blt_FontIdProc */
    TkGetFontMetricsProc,	/* Blt_GetFontMetricsProc */
    TkMeasureCharsProc,		/* Blt_MeasureCharsProc */
    TkTextStringWidthProc,	/* Blt_TexStringtWidthProc */
    TkCanRotateFontProc,	/* Blt_CanRotateFontProc */
    TkDrawCharsProc,		/* Blt_DrawCharsProc */
    TkPostscriptFontNameProc,	/* Blt_PostscriptFontNameProc */
    TkFreeFontProc,		/* Blt_FreeFontProc */
    TkUnderlineCharsProc,	/* Blt_UnderlineCharsProc */
};

static FontPattern *
NewFontPattern(void)
{
    FontPattern *patternPtr;

    patternPtr = Blt_Calloc(1, sizeof(FontPattern));
    return patternPtr;
}

static void
FreeFontPattern(FontPattern *patternPtr)
{
    if (patternPtr->family != NULL) {
	Blt_Free((char *)patternPtr->family);
    }
    Blt_Free(patternPtr);
}

static int CALLBACK
FontFamilyEnumProc(
    ENUMLOGFONT *lfPtr,		/* Logical-font data. */
    NEWTEXTMETRIC *tmPtr,	/* Physical-font data (not used). */
    int fontType,		/* Type of font (not used). */
    LPARAM lParam)		/* Result object to hold result. */
{
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    Tcl_DString ds;
    const char *faceName;
    Tcl_Encoding encoding;
    int isNew;

    tablePtr = (Blt_HashTable *)lParam;
    faceName = lfPtr->elfLogFont.lfFaceName;
    encoding = Tcl_GetEncoding(NULL, "Unicode");
    Tcl_ExternalToUtfDString(encoding, faceName, -1, &ds);
    faceName = Tcl_DStringValue(&ds);
    strtolower((char *)faceName);
    hPtr = Blt_CreateHashEntry(tablePtr, faceName, &isNew);
    Blt_SetHashValue(hPtr, NULL);
    Tcl_DStringFree(&ds);
    return 1;
}

static void
GetFontFamilies(Tk_Window tkwin, Blt_HashTable *tablePtr)
{    
    HDC hDC;
    HWND hWnd;
    Window window;

    window = Tk_WindowId(tkwin);
    hWnd = (window == None) ? (HWND)NULL : Tk_GetHWND(window);
    hDC = GetDC(hWnd);

    /*
     * On any version NT, there may fonts with international names.  
     * Use the NT-only Unicode version of EnumFontFamilies to get the 
     * font names.  If we used the ANSI version on a non-internationalized 
     * version of NT, we would get font names with '?' replacing all 
     * the international characters.
     *
     * On a non-internationalized verson of 95, fonts with international
     * names are not allowed, so the ANSI version of EnumFontFamilies will 
     * work.  On an internationalized version of 95, there may be fonts with 
     * international names; the ANSI version will work, fetching the 
     * name in the system code page.  Can't use the Unicode version of 
     * EnumFontFamilies because it only exists under NT.
     */

    if (Blt_GetPlatformId() == VER_PLATFORM_WIN32_NT) {
	EnumFontFamiliesW(hDC, NULL, (FONTENUMPROCW)FontFamilyEnumProc,
		(LPARAM)tablePtr);
    } else {
	EnumFontFamiliesA(hDC, NULL, (FONTENUMPROCA)FontFamilyEnumProc,
		(LPARAM)tablePtr);
    }	    
    ReleaseDC(hWnd, hDC);
}

/*
 *---------------------------------------------------------------------------
 *
 * ParseTkDesc --
 *
 *	Parses an array of Tcl_Objs as a Tk style font description .  
 *	
 *	      "family [size] [optionList]"
 *
 * Results:
 *	Returns a pattern structure, filling in with the necessary fields.
 *	Returns NULL if objv doesn't contain a  Tk font description.
 *
 * Side effects:
 *	Memory is allocated for the font pattern and the its strings.
 *
 *---------------------------------------------------------------------------
 */
static FontPattern *
ParseTkDesc(int objc, Tcl_Obj **objv)
{
    FontPattern *patternPtr;
    Tcl_Obj **aobjv;
    int aobjc;
    int i;

    patternPtr = NewFontPattern();

    /* Font family. */
    {
	char *family, *dash;
	family = Tcl_GetString(objv[0]);
	dash = strchr(family, '-');
	if (dash != NULL) {
	    int size;
	    
	    if (Tcl_GetInt(NULL, dash + 1, &size) != TCL_OK) {
		goto error;
	    }
	    patternPtr->size = size;
	}
	if (dash != NULL) {
	    *dash = '\0';
	}
	patternPtr->family = Blt_AssertStrdup(GetAlias(family));
	if (dash != NULL) {
	    *dash = '-';
	    i = 1;
	}
	objv++, objc--;
    }
    if (objc > 0) {
	int size;

	if (Tcl_GetIntFromObj(NULL, objv[0], &size) == TCL_OK) {
	    patternPtr->size = size;
	    objv++, objc--;
	}
    }
    aobjc = objc, aobjv = objv;
    if (objc > 0) {
	if (Tcl_ListObjGetElements(NULL, objv[0], &aobjc, &aobjv) != TCL_OK) {
	    goto error;
	}
    }
    for (i = 0; i < aobjc; i++) {
	FontSpec *specPtr;
	const char *key;
	int length;

	key = Tcl_GetStringFromObj(aobjv[i], &length);
	specPtr = FindSpec(fontSpecs, nFontSpecs, key, length);
	if (specPtr == NULL) {
	    goto error;
	}
	if (specPtr->key == NULL) {
	    continue;
	}
	if (strcmp(specPtr->key, FC_WEIGHT) == 0) {
	    patternPtr->weight = specPtr->oldvalue;
	} else if (strcmp(specPtr->key, FC_SLANT) == 0) {
	    patternPtr->slant = specPtr->oldvalue;
	} else if (strcmp(specPtr->key, FC_SPACING) == 0) {
	    patternPtr->spacing = specPtr->oldvalue;
	} else if (strcmp(specPtr->key, FC_WIDTH) == 0) {
	    patternPtr->width = specPtr->oldvalue;
	}
    }
    return patternPtr;
 error:
    FreeFontPattern(patternPtr);
    return NULL;
}	

/*
 *---------------------------------------------------------------------------
 *
 * ParseNameValuePairs --
 *
 *	Given Tcl_Obj list of name value pairs, parse the list saving in the
 *	values in a font pattern structure.
 *	
 *	      "-family family -size size -weight weight"
 *
 * Results:
 *	Returns a pattern structure, filling in with the necessary fields.
 *	Returns NULL if objv doesn't contain a valid name-value list 
 *	describing a font.
 *
 * Side effects:
 *	Memory is allocated for the font pattern and the its strings.
 *
 *---------------------------------------------------------------------------
 */
static FontPattern *
ParseNameValuePairs(Tcl_Interp *interp, Tcl_Obj *objPtr) 
{
    FontPattern *patternPtr;
    Tcl_Obj **objv;
    int objc;
    int i;

    if ((Tcl_ListObjGetElements(NULL, objPtr, &objc, &objv) != TCL_OK) ||
	(objc < 1)) {
	return NULL;		/* Can't split list or list is empty. */
    }
    if (objc & 0x1) {
	return NULL;		/* Odd number of elements in list. */
    }
    patternPtr = NewFontPattern();
    for (i = 0; i < objc; i += 2) {
	const char *key, *value;
	int length;

	key = Tcl_GetString(objv[i]);
	value = Tcl_GetStringFromObj(objv[i+1], &length);
	if (strcmp(key, "-family") == 0) {
	    if (patternPtr->family != NULL) {
		Blt_Free(patternPtr->family);
	    }
	    patternPtr->family = Blt_AssertStrdup(GetAlias(value));
	} else if (strcmp(key, "-size") == 0) {
	    int size;

	    if (Tcl_GetIntFromObj(interp, objv[i+1], &size) != TCL_OK) {
		goto error;
	    }
	    patternPtr->size = size;
	} else if (strcmp(key, "-weight") == 0) {
	    FontSpec *specPtr;

	    specPtr = FindSpec(weightSpecs, nWeightSpecs, value, length);
	    if (specPtr == NULL) {
		goto error;
	    }
	    patternPtr->weight = specPtr->oldvalue;
	} else if (strcmp(key, "-slant") == 0) {
	    FontSpec *specPtr;

	    specPtr = FindSpec(slantSpecs, nSlantSpecs, value, length);
	    if (specPtr == NULL) {
		goto error;
	    }
	    patternPtr->slant = specPtr->oldvalue;
	} else if (strcmp(key, "-spacing") == 0) {
	    FontSpec *specPtr;

	    specPtr = FindSpec(spacingSpecs, nSpacingSpecs, value, length);
	    if (specPtr == NULL) {
		goto error;
	    }
	    patternPtr->spacing = specPtr->oldvalue;
	} else if (strcmp(key, "-hint") == 0) {
	    /* Ignore */
	} else if (strcmp(key, "-rgba") == 0) {
	    /* Ignore */
	} else if (strcmp(key, "-underline") == 0) {
	    /* Ignore */
	} else if (strcmp(key, "-overstrike") == 0) {
	    /* Ignore */
	} else {
	    /* Ignore */
	}
    }
#if DEBUG_FONT_SELECTION
    fprintf(stderr, "found TkAttrList => Tk font \"%s\"\n", 
	    Tcl_GetString(objPtr));
#endif
    return patternPtr;
 error:
    FreeFontPattern(patternPtr);
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * ParseFontObj --
 *
 *	Given the name of a Tk font object, get its configuration values 
 *	save the data in a font pattern structure.
 *	
 *	      "-family family -size size -weight weight"
 *
 * Results:
 *	Returns a pattern structure, filling in with the necessary fields.
 *	Returns NULL if objv doesn't contain a valid name-value list 
 *	describing a font.
 *
 * Side effects:
 *	Memory is allocated for the font pattern and the its strings.
 *
 *---------------------------------------------------------------------------
 */
static FontPattern *
ParseFontObj(Tcl_Interp *interp, Tcl_Obj *objPtr) 
{
    FontPattern *patternPtr;
    Tcl_Obj *cmd[3];
    int result;

    patternPtr = NULL;
    cmd[0] = Tcl_NewStringObj("font", -1);
    cmd[1] = Tcl_NewStringObj("configure", -1);
    cmd[2] = objPtr;
    Tcl_IncrRefCount(cmd[0]);
    Tcl_IncrRefCount(cmd[1]);
    Tcl_IncrRefCount(cmd[2]);
    result = Tcl_EvalObjv(interp, 3, cmd, 0);
    Tcl_DecrRefCount(cmd[2]);
    Tcl_DecrRefCount(cmd[1]);
    Tcl_DecrRefCount(cmd[0]);
    if (result == TCL_OK) {
	patternPtr = ParseNameValuePairs(interp, Tcl_GetObjResult(interp));
    }
    Tcl_ResetResult(interp);
#if DEBUG_FONT_SELECTION
    if (patternPtr != NULL) {
	fprintf(stderr, "found FontObject => Tk font \"%s\"\n", 
	    Tcl_GetString(objPtr));
    }
#endif
    return patternPtr;
}

/* 
 *---------------------------------------------------------------------------
 *
 * GetPattern --
 * 
 *	Parses the font description so that the font can rewritten with an
 *	aliased font name.  This allows us to use
 *
 *	  "Sans Serif", "Serif", "Math", "Monospace"
 *
 *	font names that correspond to the proper font regardless if the
 *	standard X fonts or XFT fonts are being used.
 *
 *	Leave XLFD font descriptions alone.  Let users describe exactly the
 *	font they wish.
 *
 *---------------------------------------------------------------------------
 */
static FontPattern *
GetPattern(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    FontPattern *patternPtr;
    const char *desc;

    desc = Tcl_GetString(objPtr);
    while (isspace(UCHAR(*desc))) {
	desc++;			/* Skip leading blanks. */
    }
    if (*desc == '-') {
	/* 
	 * Case 1: XLFD font description or Tk attribute list.   
	 *
	 *   If the font description starts with a '-', it could be either an
	 *   old fashion XLFD font description or a Tk font attribute
	 *   option-value list.
	 */
	patternPtr = ParseNameValuePairs(interp, objPtr);
	if (patternPtr == NULL) {
	    return NULL;		/* XLFD font description */
	}
    } else if (*desc == '*') {
	return NULL;		/* XLFD font description */
    } else if (strpbrk(desc, "::") != NULL) {
	patternPtr = ParseFontObj(interp, objPtr);
    } else {
	int objc;
	Tcl_Obj **objv;
	/* 
	 * Case 3: Tk-style description.   
	 */
	if ((Tcl_ListObjGetElements(NULL, objPtr, &objc, &objv) != TCL_OK) || 
	    (objc < 1)) {
	    return NULL;		/* Can't split into a list or
					 * list is empty. */
	}
	patternPtr = NULL;
	if (objc == 1) {
	    /* 
	     * Case 3a: Tk font object name.
	     *
	     *   Assuming that Tk font object names won't contain whitespace,
	     *   see if its a font object.
	     */
	    patternPtr = ParseFontObj(interp, objv[0]);
	} 
	if (patternPtr == NULL) {
	    /* 
	     * Case 3b: List of font attributes in the form "family size
	     *		?attrs?"
	     */
	    patternPtr = ParseTkDesc(objc, objv);
	}
    }	
    return patternPtr;
}

static void
WriteXLFDDescription(Tk_Window tkwin, FontPattern *patternPtr, 
		       Tcl_DString *resultPtr)
{
    int size;
    
    /* Rewrite the font description using the aliased family. */
    Tcl_DStringInit(resultPtr);

    /* Family */
    if (patternPtr->family != NULL) {
	Tcl_DStringAppendElement(resultPtr, "-family");
	Tcl_DStringAppendElement(resultPtr, patternPtr->family);
    }
    /* Weight */
    if (patternPtr->weight != NULL) {
	Tcl_DStringAppendElement(resultPtr, "-weight");
	Tcl_DStringAppendElement(resultPtr, patternPtr->weight);
    }
    /* Slant */
    if (patternPtr->slant != NULL) {
	Tcl_DStringAppendElement(resultPtr, "-slant");
	Tcl_DStringAppendElement(resultPtr, patternPtr->slant);
    }
    /* Width */
    if (patternPtr->width != NULL) {
	Tcl_DStringAppendElement(resultPtr, "-width");
	Tcl_DStringAppendElement(resultPtr, patternPtr->width);
    }
    /* Size */
    Tcl_DStringAppendElement(resultPtr, "-size");
    size = (int)(PointsToPixels(tkwin, patternPtr->size) + 0.5);
    size = patternPtr->size;
    Tcl_DStringAppendElement(resultPtr, Blt_Itoa(size));
}
    

static Tk_Font 
OpenTkFont(Tcl_Interp *interp, Tk_Window tkwin, Tcl_Obj *objPtr)
{
    Tk_Font tkFont;
    FontPattern *patternPtr;
    Blt_HashEntry *hPtr;
    const char *desc;

    desc = Tcl_GetString(objPtr);
    hPtr = Blt_FindHashEntry(&fontTable, desc);
    patternPtr = GetPattern(interp, objPtr);
    if (patternPtr == NULL) {
	tkFont = Tk_GetFont(interp, tkwin, Tcl_GetString(objPtr));
    } else {
	Tcl_DString ds;

	/* Rewrite the font description using the aliased family. */
	WriteXLFDDescription(tkwin, patternPtr, &ds);
	tkFont = Tk_GetFont(interp, tkwin, Tcl_DStringValue(&ds));
	fprintf(stderr, "Tkfont: %s => %s\n", Tcl_GetString(objPtr),
		Tcl_DStringValue(&ds));
	Tcl_DStringFree(&ds);
	FreeFontPattern(patternPtr);
    }
    return tkFont;
}


static const char *
TkNameOfFontProc(_Blt_Font *fontPtr) 
{
    return Tk_NameOfFont(fontPtr->clientData);
}

static const char *
TkFamilyOfFontProc(_Blt_Font *fontPtr) 
{
    return ((TkFont *)fontPtr->clientData)->fa.family;
}

static Font
TkFontIdProc(_Blt_Font *fontPtr) 
{
    return Tk_FontId(fontPtr->clientData);
}

static void
TkGetFontMetricsProc(_Blt_Font *fontPtr, Blt_FontMetrics *fmPtr)
{
    TkFont *tkFontPtr = fontPtr->clientData;
    Tk_FontMetrics fm;

    Tk_GetFontMetrics(fontPtr->clientData, &fm);
    fmPtr->ascent = fm.ascent;
    fmPtr->descent = fm.descent;
    fmPtr->linespace = fm.linespace;
    fmPtr->tabWidth = tkFontPtr->tabWidth;
    fmPtr->underlinePos = tkFontPtr->underlinePos;
    fmPtr->underlineHeight = tkFontPtr->underlineHeight;
}

static int
TkMeasureCharsProc(_Blt_Font *fontPtr, const char *text, int nBytes, int max, 
		   int flags, int *lengthPtr)
{
    return Tk_MeasureChars(fontPtr->clientData, text, nBytes, max, flags, 
	lengthPtr);
}

static int
TkTextStringWidthProc(_Blt_Font *fontPtr, const char *string, int nBytes)
{
    return Tk_TextWidth(fontPtr->clientData, string, nBytes);
}    

static void
TkDrawCharsProc(
    Display *display,		/* Display on which to draw. */
    Drawable drawable,		/* Window or pixmap in which to draw. */
    GC gc,			/* Graphics context for drawing characters. */
    _Blt_Font *fontPtr,		/* Font in which characters will be drawn;
				 * must be the same as font used in GC. */
    int depth,			/* Not used. */
    float angle,		/* Not used. */
    const char *text,		/* UTF-8 string to be displayed.  Need not be
				 * '\0' terminated.  All Tk meta-characters
				 * (tabs, control characters, and newlines)
				 * should be stripped out of the string that
				 * is passed to this function.  If they are
				 * not stripped out, they will be displayed as
				 * regular printing characters. */
    int nBytes,			/* Number of bytes in string. */
    int x, int y)		/* Coordinates at which to place origin of
				 * string when drawing. */
{
    Tk_DrawChars(display, drawable, gc, fontPtr->clientData, text, nBytes, 
		 x, y);
}

static int
TkPostscriptFontNameProc(_Blt_Font *fontPtr, Tcl_DString *resultPtr) 
{
    return Tk_PostscriptFontName(fontPtr->clientData, resultPtr);
}

static int
TkCanRotateFontProc(_Blt_Font *fontPtr, float angle) 
{
    return FALSE;
}

static void
TkFreeFontProc(_Blt_Font *fontPtr) 
{
    Tk_FreeFont(fontPtr->clientData);
    Blt_Free(fontPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TkUnderlineCharsProc --
 *
 *	This procedure draws an underline for a given range of characters in a
 *	given string.  It doesn't draw the characters (which are assumed to
 *	have been displayed previously); it just draws the underline.  This
 *	procedure would mainly be used to quickly underline a few characters
 *	without having to construct an underlined font.  To produce properly
 *	underlined text, the appropriate underlined font should be constructed
 *	and used.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets displayed in "drawable".
 *
 *---------------------------------------------------------------------------
 */
static void
TkUnderlineCharsProc(
    Display *display,		/* Display on which to draw. */
    Drawable drawable,		/* Window or pixmap in which to draw. */
    GC gc,			/* Graphics context for actually drawing
				 * line. */
    _Blt_Font *fontPtr,		/* Font used in GC; must have been
				 * allocated by Tk_GetFont().  Used for
				 * character dimensions, etc. */
    const char *string,		/* String containing characters to be
				 * underlined or overstruck. */
    int textLen,		/* Unused. */
    int x, int y,		/* Coordinates at which first character of
				 * string is drawn. */
    int first,			/* Byte offset of the first character. */
    int last,			/* Byte offset after the last character. */
    int xMax)
{
    Tk_UnderlineChars(display, drawable, gc, fontPtr->clientData, string, x, y, 
	first, last);
}

static Blt_NameOfFontProc		WinNameOfFontProc;
static Blt_GetFontMetricsProc		WinGetFontMetricsProc;
static Blt_FontIdProc			WinFontIdProc;
static Blt_MeasureCharsProc		WinMeasureCharsProc;
static Blt_TextStringWidthProc		WinTextStringWidthProc;
static Blt_FreeFontProc			WinFreeFontProc;
static Blt_DrawCharsProc		WinDrawCharsProc;
static Blt_PostscriptFontNameProc	WinPostscriptFontNameProc;
static Blt_FamilyOfFontProc		WinFamilyOfFontProc;
static Blt_CanRotateFontProc		WinCanRotateFontProc;
static Blt_UnderlineCharsProc		WinUnderlineCharsProc;

static Blt_FontClass winFontClass = {
    FONT_WIN,
    WinNameOfFontProc,		/* Blt_NameOfFontProc */
    WinFamilyOfFontProc,	/* Blt_FamilyOfFontProc */
    WinFontIdProc,		/* Blt_FontIdProc */
    WinGetFontMetricsProc,	/* Blt_GetFontMetricsProc */
    WinMeasureCharsProc,	/* Blt_MeasureCharsProc */
    WinTextStringWidthProc,	/* Blt_TextStringWidthProc */
    WinCanRotateFontProc,	/* Blt_CanRotateFontProc */
    WinDrawCharsProc,		/* Blt_DrawCharsProc */
    WinPostscriptFontNameProc,	/* Blt_PostscriptFontNameProc */
    WinFreeFontProc,		/* Blt_FreeFontProc */
    WinUnderlineCharsProc,	/* Blt_UnderlineCharsProc */
};


/*
 *---------------------------------------------------------------------------
 *
 * CreateRotatedFont --
 *
 *	Creates a rotated copy of the given font.  This only works for
 *	TrueType fonts.
 *
 * Results:
 *	Returns the newly create font or NULL if the font could not be
 *	created.
 *
 *---------------------------------------------------------------------------
 */
static HFONT
CreateRotatedFont(
    TkFont *fontPtr,		/* Font identifier (actually a Tk_Font) */
    long angle10)
{				/* Number of degrees to rotate font */
    TkFontAttributes *faPtr;	/* Set of attributes to match. */
    HFONT hfont;
    LOGFONTW lf;

    faPtr = &fontPtr->fa;
    ZeroMemory(&lf, sizeof(LOGFONT));
    lf.lfHeight = -faPtr->size;
    if (lf.lfHeight < 0) {
	HDC dc;

	dc = GetDC(NULL);
	lf.lfHeight = -MulDiv(faPtr->size, GetDeviceCaps(dc, LOGPIXELSY), 72);
	ReleaseDC(NULL, dc);
    }
    lf.lfWidth = 0;
    lf.lfEscapement = lf.lfOrientation = angle10;
#define TK_FW_NORMAL	0
    lf.lfWeight = (faPtr->weight == TK_FW_NORMAL) ? FW_NORMAL : FW_BOLD;
    lf.lfItalic = faPtr->slant;
    lf.lfUnderline = faPtr->underline;
    lf.lfStrikeOut = faPtr->overstrike;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

    hfont = NULL;
    if (faPtr->family == NULL) {
	lf.lfFaceName[0] = '\0';
    } else {
#if (_TCL_VERSION >= _VERSION(8,1,0)) 
	Tcl_DString dString;
	Tcl_Encoding encoding;

	encoding = Tcl_GetEncoding(NULL, "unicode");
	Tcl_UtfToExternalDString(encoding, faPtr->family, -1, &dString);
	if (Blt_GetPlatformId() == VER_PLATFORM_WIN32_NT) {
	    Tcl_UniChar *src, *dst;
	    
	    /*
	     * We can only store up to LF_FACESIZE wide characters
	     */
	    if (Tcl_DStringLength(&dString) >= (LF_FACESIZE * sizeof(WCHAR))) {
		Tcl_DStringSetLength(&dString, LF_FACESIZE);
	    }
	    src = (Tcl_UniChar *)Tcl_DStringValue(&dString);
	    dst = (Tcl_UniChar *)lf.lfFaceName;
	    while (*src != '\0') {
		*dst++ = *src++;
	    }
	    *dst = '\0';
	    hfont = CreateFontIndirectW((LOGFONTW *)&lf);
	} else {
	    /*
	     * We can only store up to LF_FACESIZE characters
	     */
	    if (Tcl_DStringLength(&dString) >= LF_FACESIZE) {
		Tcl_DStringSetLength(&dString, LF_FACESIZE);
	    }
	    strcpy((char *)lf.lfFaceName, Tcl_DStringValue(&dString));
	    hfont = CreateFontIndirectA((LOGFONTA *)&lf);
	}
	Tcl_DStringFree(&dString);
#else
	strncpy((char *)lf.lfFaceName, faPtr->family, LF_FACESIZE - 1);
	lf.lfFaceName[LF_FACESIZE] = '\0';
#endif /* _TCL_VERSION >= 8.1.0 */
    }

    if (hfont == NULL) {
#if WINDEBUG
	PurifyPrintf("can't create font: %s\n", Blt_LastError());
#endif
    } else { 
	HFONT oldFont;
	TEXTMETRIC tm;
	HDC hdc;
	int result;

	/* Check if the rotated font is really a TrueType font. */

	hdc = GetDC(NULL);		/* Get the desktop device context */
	oldFont = SelectFont(hdc, hfont);
	result = ((GetTextMetrics(hdc, &tm)) && 
		  (tm.tmPitchAndFamily & TMPF_TRUETYPE));
	SelectFont(hdc, oldFont);
	ReleaseDC(NULL, hdc);
	if (!result) {
#if WINDEBUG
	    PurifyPrintf("not a true type font\n");
#endif
	    DeleteFont(hfont);
	    return NULL;
	}
    }
    return hfont;
}

static void
DestroyFont(RotatedFont *rotFontPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    
    for (hPtr = Blt_FirstHashEntry(&rotFontPtr->fontTable, &cursor); 
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	HFONT hfont;
	
	hfont = Blt_GetHashValue(hPtr);
	DeleteFont(hfont);
    }
    Tk_FreeFont(rotFontPtr->tkfont);
    Blt_DeleteHashTable(&rotFontPtr->fontTable);
    Blt_DeleteHashEntry(&fontTable, rotFontPtr->hashPtr);
    Blt_Free(rotFontPtr);
}

/* 
 *---------------------------------------------------------------------------
 *
 * GetRotatedFontFromObj --
 * 
 *	Opens a Tk font based on the description in the Tcl_Obj.  We first
 *	parse the description and if necessary rewrite it using the proper
 *	font aliases.  The font names
 *
 *	  "Sans Serif", "Serif", "Math", "Monospace"
 *
 *	correspond to the proper font regardless if the standard X fonts or
 *	XFT fonts are being used.
 *
 *	Leave XLFD font descriptions alone.  Let users describe exactly the
 *	font they wish.
 *
 *	Outside of reimplementing the Tk font mechanism, rewriting the
 *	font allows use to better handle programs that must work with
 *	X servers with and without the XRender extension.  It means 
 *	that the widget's default font settings do not have to use
 *	XLFD fonts even if XRender is available.
 *	
 *---------------------------------------------------------------------------
 */
static RotatedFont *
GetRotatedFontFromObj(Tcl_Interp *interp, Tk_Window tkwin, Tcl_Obj *objPtr)
{
    Blt_HashEntry *hPtr;
    RotatedFont *rotFontPtr;
    const char *desc;
    int isNew;

    desc = Tcl_GetString(objPtr);
    while (isspace(UCHAR(*desc))) {
	desc++;			/* Skip leading blanks. */
    }
    /* Is the font already in the cache? */
    hPtr = Blt_CreateHashEntry(&fontTable, desc, &isNew);
    if (isNew) {
	Tk_Font tkFont;

	tkFont = OpenTkFont(interp, tkwin, objPtr);
	if (tkFont == NULL) {
	    Blt_DeleteHashEntry(&fontTable, hPtr);
	    return NULL;
	}
	rotFontPtr = Blt_AssertMalloc(sizeof(RotatedFont));
	rotFontPtr->refCount = 1;
	rotFontPtr->tkfont = tkFont;
	rotFontPtr->name = Blt_AssertStrdup(desc);
	Blt_SetHashValue(hPtr, rotFontPtr);
	Blt_InitHashTable(&rotFontPtr->fontTable, BLT_ONE_WORD_KEYS);
    } else {
	rotFontPtr = Tcl_GetHashValue(hPtr);
	rotFontPtr->refCount++;
    }
    return rotFontPtr;
}

static const char *
WinNameOfFontProc(_Blt_Font *fontPtr) 
{
    RotatedFont *rotFontPtr = fontPtr->clientData;

    return Tk_NameOfFont(rotFontPtr->tkfont);
}

static const char *
WinFamilyOfFontProc(_Blt_Font *fontPtr) 
{
    RotatedFont *rotFontPtr = fontPtr->clientData;

    return ((TkFont *)rotFontPtr->tkfont)->fa.family;
}

static Font
WinFontIdProc(_Blt_Font *fontPtr) 
{
    RotatedFont *rotFontPtr = fontPtr->clientData;

    return Tk_FontId(rotFontPtr->tkfont);
}

static void
WinGetFontMetricsProc(_Blt_Font *fontPtr, Blt_FontMetrics *fmPtr)
{
    RotatedFont *rotFontPtr = fontPtr->clientData;
    TkFont *tkFontPtr = (TkFont *)rotFontPtr->tkfont;
    Tk_FontMetrics fm;

    Tk_GetFontMetrics(rotFontPtr->tkfont, &fm);
    fmPtr->ascent = fm.ascent;
    fmPtr->descent = fm.descent;
    fmPtr->linespace = fm.linespace;
    fmPtr->tabWidth = tkFontPtr->tabWidth;
    fmPtr->underlinePos = tkFontPtr->underlinePos;
    fmPtr->underlineHeight = tkFontPtr->underlineHeight;
}

static int
WinMeasureCharsProc(_Blt_Font *fontPtr, const char *text, int nBytes,
		   int max, int flags, int *lengthPtr)
{
    RotatedFont *rotFontPtr = fontPtr->clientData;

    return Tk_MeasureChars(rotFontPtr->tkfont, text, nBytes, max, flags, 
		lengthPtr);
}

static int
WinTextStringWidthProc(_Blt_Font *fontPtr, const char *text, int nBytes)
{
    RotatedFont *rotFontPtr = fontPtr->clientData;

    return Tk_TextWidth(rotFontPtr->tkfont, text, nBytes);
}    


static void
WinDrawCharsProc(
    Display *display,		/* Display on which to draw. */
    Drawable drawable,		/* Window or pixmap in which to draw. */
    GC gc,			/* Graphics context for drawing characters. */
    _Blt_Font *fontPtr,		/* Font in which characters will be drawn;
				 * must be the same as font used in GC. */
    int depth,			/* Not used. */
    float angle,		/* Not used. */
    const char *text,		/* UTF-8 string to be displayed.  Need not be
				 * '\0' terminated.  All Tk meta-characters
				 * (tabs, control characters, and newlines)
				 * should be stripped out of the string that
				 * is passed to this function.  If they are
				 * not stripped out, they will be displayed as
				 * regular printing characters. */
    int nBytes,			/* Number of bytes in string. */
    int x, int y)		/* Coordinates at which to place origin of
				 * string when drawing. */
{
    RotatedFont *rotFontPtr = fontPtr->clientData;

    if (angle != 0.0) {
	long angle10;
	Blt_HashEntry *hPtr;
    
	angle *= 10.0f;
	angle10 = ROUND(angle);
	hPtr = Blt_FindHashEntry(&rotFontPtr->fontTable, (char *)angle10);
	if (hPtr == NULL) {
	    fprintf(stderr, "can't find font %s at %g rotated\n", 
		    rotFontPtr->name, angle);
           return;		/* Can't find instance at requested angle. */
	}
	display->request++;
	if (drawable != None) {
	    HDC hdc;
	    HFONT hfont;
	    TkWinDCState state;
	    
	    hfont = Blt_GetHashValue(hPtr);
	    hdc = TkWinGetDrawableDC(display, drawable, &state);
	    Blt_TextOut(hdc, gc, hfont, text, nBytes, x, y);
	    TkWinReleaseDrawableDC(drawable, hdc, &state);
	}
    } else {
	Tk_DrawChars(display, drawable, gc, rotFontPtr->tkfont, text, nBytes, 
		     x, y);
    }
}


static int
WinPostscriptFontNameProc(_Blt_Font *fontPtr, Tcl_DString *resultPtr) 
{
    RotatedFont *rotFontPtr = fontPtr->clientData;

    return Tk_PostscriptFontName(rotFontPtr->tkfont, resultPtr);
}

static int
WinCanRotateFontProc(_Blt_Font *fontPtr, float angle) 
{
    Blt_HashEntry *hPtr;
    HFONT hfont;
    RotatedFont *rotFontPtr = fontPtr->clientData;
    int isNew;
    long angle10;

    angle *= 10.0f;
    angle10 = ROUND(angle);
    if (angle == 0L) {
	return TRUE;
    }
    hPtr = Blt_CreateHashEntry(&rotFontPtr->fontTable, (char *)angle10, &isNew);
    if (!isNew) {
	return TRUE;		/* Rotated font already exists. */
    }
    hfont = CreateRotatedFont((TkFont *)Blt_FontId(fontPtr), angle10);
    if (hfont == NULL) {
	Blt_DeleteHashEntry(&rotFontPtr->fontTable, hPtr);
	return FALSE;
    }
    Blt_SetHashValue(hPtr, hfont);
    return TRUE;
}

static void
WinFreeFontProc(_Blt_Font *fontPtr) 
{
    RotatedFont *rotFontPtr = fontPtr->clientData;

    rotFontPtr->refCount--;
    if (rotFontPtr->refCount <= 0) {
	DestroyFont(rotFontPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * WinUnderlineCharsProc --
 *
 *	This procedure draws an underline for a given range of characters in a
 *	given string.  It doesn't draw the characters (which are assumed to
 *	have been displayed previously); it just draws the underline.  This
 *	procedure would mainly be used to quickly underline a few characters
 *	without having to construct an underlined font.  To produce properly
 *	underlined text, the appropriate underlined font should be constructed
 *	and used.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets displayed in "drawable".
 *
 *---------------------------------------------------------------------------
 */
static void
WinUnderlineCharsProc(
    Display *display,		/* Display on which to draw. */
    Drawable drawable,		/* Window or pixmap in which to draw. */
    GC gc,			/* Graphics context for actually drawing
				 * line. */
    _Blt_Font *fontPtr,		/* Font used in GC; must have been
				 * allocated by Tk_GetFont().  Used for
				 * character dimensions, etc. */
    const char *string,		/* String containing characters to be
				 * underlined or overstruck. */
    int textLen,		/* Unused. */
    int x, int y,		/* Coordinates at which first character of
				 * string is drawn. */
    int first,			/* Byte offset of the first character. */
    int last,			/* Byte offset after the last character. */
    int xMax)
{
    RotatedFont *rotFontPtr = fontPtr->clientData;

    Tk_UnderlineChars(display, drawable, gc, rotFontPtr->tkfont, string, x, y, 
	first, last);
}


/*
 *---------------------------------------------------------------------------
 *
 * Blt_GetFontFromObj -- 
 *
 *	Given a string description of a font, map the description to a
 *	corresponding Tk_Font that represents the font.
 *
 * Results:
 *	The return value is token for the font, or NULL if an error prevented
 *	the font from being created.  If NULL is returned, an error message
 *	will be left in the interp's result.
 *
 * Side effects:
 *	The font is added to an internal database with a reference count.  For
 *	each call to this procedure, there should eventually be a call to
 *	Tk_FreeFont() or Tk_FreeFontFromObj() so that the database is cleaned
 *	up when fonts aren't in use anymore.
 *
 *---------------------------------------------------------------------------
 */
Blt_Font
Blt_GetFontFromObj(
    Tcl_Interp *interp,		/* Interp for database and error return. */
    Tk_Window tkwin,		/* For display on which font will be used. */
    Tcl_Obj *objPtr)		/* String describing font, as: named font,
				 * native format, or parseable string. */
{
    _Blt_Font *fontPtr; 
   
    fontPtr = Blt_Calloc(1, sizeof(_Blt_Font));
    if (fontPtr == NULL) {
	return NULL;		/* Out of memory. */
    }
    if (!initialized) {
	Blt_InitHashTable(&fontTable, BLT_STRING_KEYS);
	MakeAliasTable(tkwin);
	initialized++;
    }
    fontPtr->clientData = OpenTkFont(interp, tkwin, objPtr);

    if (fontPtr->clientData == NULL) {
	Blt_Free(fontPtr);
#if DEBUG_FONT_SELECTION
	fprintf(stderr, "FAILED to find either Xft or Tk font \"%s\"\n", 
	    Tcl_GetString(objPtr));
#endif
	return NULL;		/* Failed to find either Xft or Tk fonts. */
    }
    fontPtr->classPtr = &tkFontClass;
#ifdef notdef
    fontPtr->clientData = GetRotatedFontFromObj(interp, tkwin, objPtr);
#if DEBUG_FONT_SELECTION
    fprintf(stderr, "SUCCESS: Found Tk font \"%s\"\n", Tcl_GetString(objPtr));
#endif
    fontPtr->classPtr = &winFontClass;
#endif
    fontPtr->interp = interp;
    fontPtr->display = Tk_Display(tkwin);
    return fontPtr;		/* Found Tk font. */
}


Blt_Font
Blt_AllocFontFromObj(
    Tcl_Interp *interp,		/* Interp for database and error return. */
    Tk_Window tkwin,		/* For screen on which font will be used. */
    Tcl_Obj *objPtr)		/* Object describing font, as: named font,
				 * native format, or parseable string. */
{
    return Blt_GetFontFromObj(interp, tkwin, objPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_GetFont -- 
 *
 *	Given a string description of a font, map the description to a
 *	corresponding Tk_Font that represents the font.
 *
 * Results:
 *	The return value is token for the font, or NULL if an error prevented
 *	the font from being created.  If NULL is returned, an error message
 *	will be left in interp's result object.
 *
 * Side effects:
 * 	The font is added to an internal database with a reference count.  For
 * 	each call to this procedure, there should eventually be a call to
 * 	Blt_FreeFont so that the database is cleaned up when fonts aren't in
 * 	use anymore.
 *
 *---------------------------------------------------------------------------
 */

Blt_Font
Blt_GetFont(
    Tcl_Interp *interp,		/* Interp for database and error return. */
    Tk_Window tkwin,		/* For screen on which font will be used. */
    const char *string)		/* Object describing font, as: named font,
				 * native format, or parseable string. */
{
    Blt_Font font;
    Tcl_Obj *objPtr;

    objPtr = Tcl_NewStringObj(string, strlen(string));
    Tcl_IncrRefCount(objPtr);
    font = Blt_GetFontFromObj(interp, tkwin, objPtr);
    Tcl_DecrRefCount(objPtr);
    return font;
}

Tcl_Interp *
Blt_GetFontInterp(_Blt_Font *fontPtr) 
{
    return fontPtr->interp;
}

int
Blt_TextWidth(_Blt_Font *fontPtr, const char *string, int length)
{
    if (Blt_Ps_IsPrinting()) {
	int width;

	width = Blt_Ps_TextWidth(fontPtr, string, length);
	if (width >= 0) {
	    return width;
	}
    }
    return (*fontPtr->classPtr->textWidth)(fontPtr, string, length);
}

void
Blt_GetFontMetrics(_Blt_Font *fontPtr, Blt_FontMetrics *fmPtr)
{
    if (Blt_Ps_IsPrinting()) {
	if (Blt_Ps_GetFontMetrics(fontPtr, fmPtr) == TCL_OK) {
	    return;
	}
    }
    return (*fontPtr->classPtr->getFontMetrics)(fontPtr, fmPtr);
}
