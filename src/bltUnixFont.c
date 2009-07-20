
/*
 * bltUnixFont.c --
 *
 * This module implements freetype (Xft) and Tk fonts for the BLT toolkit.
 * 
 * The Blt_Font is a wrapper around the existing Tk font structure, adding
 * Freetype fonts (via the XRender extension).  The original Tk font
 * procedures act as a fallback if a suitable Xft enabled server can't be
 * found.
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

#ifdef HAVE_LIBXFT
#include <ft2build.h>
#include FT_FREETYPE_H
#include <X11/Xft/Xft.h>
#endif

#include "bltFont.h"

/*
 * This module provides antialiased fonts via Freetype as now does Tk 8.5.
 * This version also includes rotated fonts. No subfont matching is done to
 * avoid rotating dozens of subfonts for every rotated font.  It's possible
 * that glyphs may be missing that exist in the Tk version.  The trade-off
 * seems fair when weighing the benefit of high-quality antialiased rotated
 * fonts.
 *
 * Font rotation is done via the freetype font matrix for outline fonts.  For
 * bitmap fonts we fall back on drawing the text into a bitmap and rotate the
 * bitmap.  This requires depth-aware versions of Tk_DrawChars, since Xft is
 * drawing into a drawable of a different depth (depth is 1).
 * 
 * The best tactic is to 1) not use bitmapped fonts if better outline fonts
 * are available and 2) provide our own font handling routines that allow font
 * rotation and font aliasing.  The font aliases allow us to use a font name
 * like "Sans Serif" that translates into a good font for that platform and
 * set of fonts available (Xft or Xlfd font).
 */
 
#define DEBUG_FONT_SELECTION	0
#define DEBUG_FONT_SELECTION2	0

typedef struct _Blt_Font _Blt_Font;

enum FontTypes { 
    FONT_UNKNOWN, 		/* Unknown font type. */
    FONT_TK, 			/* Normal Tk font. */
    FONT_FT 			/* Freetype font. */
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
} TkFontPattern;

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

static FontSpec spacingSpecs[] = {
    { "charcell",     2, FC_SPACING, FC_CHARCELL,	  "c"},
    { "dual",         2, FC_SPACING, FC_DUAL,		  "*"},
    { "mono",         2, FC_SPACING, FC_MONO,		  "m"},
    { "proportional", 1, FC_SPACING, FC_PROPORTIONAL,	  "p"},
};
static int nSpacingSpecs = sizeof(spacingSpecs) / sizeof(FontSpec);

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

static Blt_HashTable fontTable;
static void TkGetFontFamilies(Tk_Window tkwin, Blt_HashTable *tablePtr);

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

#ifdef HAVE_LIBXFT
static void FtGetFontFamilies(Tk_Window tkwin, Blt_HashTable *tablePtr);
static int initialized = FALSE;

static int
IsXRenderAvailable(Tk_Window tkwin)
{
    static int isXRenderAvail = -1;

    if (isXRenderAvail < 0) {
	int eventBase, errorBase;

	isXRenderAvail = FALSE;
	Blt_InitHashTable(&fontTable, BLT_STRING_KEYS);
	initialized = TRUE;
	if (!XRenderQueryExtension(Tk_Display(tkwin), &eventBase, &errorBase)) {
	    return FALSE;
	}
	if (XRenderFindVisualFormat(Tk_Display(tkwin), Tk_Visual(tkwin)) == 0) {
	    return FALSE;
	}
	isXRenderAvail = TRUE;
    }
    return isXRenderAvail;
}
#endif

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
SearchForFontSpec(
    FontSpec *table,		/* Table of font options.  */
    int nSpecs,			/* # specifications in font spec table. */
    const char *string)		/* Name of font option to search for. */
{
    char c;
    int high, low;
    size_t length;

    low = 0;
    high = nSpecs - 1;
    c = tolower((unsigned char)string[0]);
    length = strlen(string);
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
FindSpec(Tcl_Interp *interp, FontSpec *tablePtr, int nSpecs, const char *string)
{
    int n;
    
    n = SearchForFontSpec(tablePtr, nSpecs, string);
    if (n < 0) {
	if (n == -1) {
	    Tcl_AppendResult(interp, "unknown ", tablePtr[0].key,
			     " specification \"", string, "\"", (char *)NULL); 
	}
	if (n == -2) {
	    Tcl_AppendResult(interp, "ambiguous ", tablePtr[0].key,
			     " specification \"", string, "\"", (char *)NULL); 
	}
	return NULL;
    }
    return tablePtr + n;
}

static Blt_HashTable aliasTable;
static int alias_initialized = 0;

typedef struct {
    const char *name, *aliases[10];
} FontAlias;

#ifdef HAVE_LIBXFT
static FontAlias xftFontAliases[] = {
    { "math",	{ "mathematica1",  "nimbus sans l condensed", "courier"}},
    { "serif",  { "times new roman", "nimbus roman no9 l" "times" }},
    { "sans serif", { "arial", "nimbus sans l", "helvetica" }},
    { "monospace", { "courier new", "nimbus mono l", "courier" }},
    { "symbol", { "standard symbols l", "symbol" }},
    { NULL }
};
#endif

static FontAlias xlfdFontAliases[] = {
    { "math",		{"courier"}},
    { "serif",		{"times"}},
    { "sans serif",	{ "helvetica" }},
    { "monospace",	{ "courier" }},
    { NULL }
};

static void
MakeAliasTable(Tk_Window tkwin)
{
    Blt_HashTable familyTable;
    FontAlias *fp;
    FontAlias *table;

    Blt_InitHashTable(&familyTable, TCL_STRING_KEYS);
#ifdef HAVE_LIBXFT
    if (IsXRenderAvailable(tkwin)) {
	FtGetFontFamilies(tkwin, &familyTable);
    } else {
	TkGetFontFamilies(tkwin, &familyTable);
    }
#else 
    TkGetFontFamilies(tkwin, &familyTable);
#endif
    Blt_InitHashTable(&aliasTable, TCL_STRING_KEYS);
#ifdef HAVE_LIBXFT
    table = (IsXRenderAvailable(tkwin)) ? xftFontAliases : xlfdFontAliases;
#else 
    table = xlfdFontAliases;
#endif
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
static Blt_TextWidthProc		TkTextWidthProc;
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
    TkTextWidthProc,		/* Blt_TextWidthProc */
    TkCanRotateFontProc,	/* Blt_CanRotateFontProc */
    TkDrawCharsProc,		/* Blt_DrawCharsProc */
    TkPostscriptFontNameProc,	/* Blt_PostscriptFontNameProc */
    TkFreeFontProc,		/* Blt_FreeFontProc */
    TkUnderlineCharsProc,	/* Blt_UnderlineCharsProc */
};

static TkFontPattern *
TkNewFontPattern(void)
{
    TkFontPattern *patternPtr;

    patternPtr = Blt_Calloc(1, sizeof(TkFontPattern));
    return patternPtr;
}

static void
TkFreeFontPattern(TkFontPattern *patternPtr)
{
    if (patternPtr->family != NULL) {
	Blt_Free((char *)patternPtr->family);
    }
    Blt_Free(patternPtr);
}


static void
TkGetFontFamilies(Tk_Window tkwin, Blt_HashTable *tablePtr)
{
    char **list, **np, **nend;
    const char *pat;
    int n;
    
    pat = "-*-*-*-*-*-*-*-*-*-*-*-*-*-*";
    list = XListFonts(Tk_Display(tkwin), pat, 10000, &n);
    for (np = list, nend = np + n; np < nend; np++) {
	Blt_HashEntry *hPtr;
	int isNew;
	char *family, *dash;
	
	/* Parse out the family name. Assume the names are all lower case. */
	dash = strchr(*np+1, '-');
	if (dash == NULL) {
	    continue;
	}
	family = dash+1;
	dash = strchr(family, '-');
	if (dash != NULL) {
	    *dash = '\0';
	}
	hPtr = Blt_CreateHashEntry(tablePtr, family, &isNew);
	Blt_SetHashValue(hPtr, NULL);
    }
    XFreeFontNames(list);
}


/*
 *---------------------------------------------------------------------------
 *
 * TkParseTkDesc --
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
static TkFontPattern *
TkParseTkDesc(Tcl_Interp *interp, int objc, Tcl_Obj **objv)
{
    TkFontPattern *patternPtr;
    Tcl_Obj **aobjv;
    int aobjc;
    int i;

    patternPtr = TkNewFontPattern();

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

	key = Tcl_GetString(aobjv[i]);
	specPtr = FindSpec(interp, fontSpecs, nFontSpecs, key);
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
#if DEBUG_FONT_SELECTION
    fprintf(stderr, "found TkDesc => Tk font \"%s\"\n", patternPtr->family);
#endif
    return patternPtr;
 error:
    TkFreeFontPattern(patternPtr);
    return NULL;
}	

/*
 *---------------------------------------------------------------------------
 *
 * TkParseNameValuePairs --
 *
 *	Given Tcl_Obj list of name value pairs, parse the list saving
 *	in the values in a font pattern structure.
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
static TkFontPattern *
TkParseNameValuePairs(Tcl_Interp *interp, Tcl_Obj *objPtr) 
{
    TkFontPattern *patternPtr;
    Tcl_Obj **objv;
    int objc;
    int i;

    if ((Tcl_ListObjGetElements(NULL, objPtr, &objc, &objv) != TCL_OK) ||
	(objc < 1)) {
	return NULL;		/* Can't split list or list is empty. */
    }
    if (objc & 0x1) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "odd number of elements, missing value",
			 (char *)NULL);
	}
	return NULL;		/* Odd number of elements in list. */
    }
    patternPtr = TkNewFontPattern();
    for (i = 0; i < objc; i += 2) {
	const char *key, *value;

	key = Tcl_GetString(objv[i]);
	value = Tcl_GetString(objv[i+1]);
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

	    specPtr = FindSpec(interp, weightSpecs, nWeightSpecs, value);
	    if (specPtr == NULL) {
		goto error;
	    }
	    patternPtr->weight = specPtr->oldvalue;
	} else if (strcmp(key, "-slant") == 0) {
	    FontSpec *specPtr;

	    specPtr = FindSpec(interp, slantSpecs, nSlantSpecs, value);
	    if (specPtr == NULL) {
		goto error;
	    }
	    patternPtr->slant = specPtr->oldvalue;
	} else if (strcmp(key, "-spacing") == 0) {
	    FontSpec *specPtr;

	    specPtr = FindSpec(interp, spacingSpecs, nSpacingSpecs, value);
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
    TkFreeFontPattern(patternPtr);
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * TkParseNameValuePairs --
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
static TkFontPattern *
TkParseFontObj(Tcl_Interp *interp, Tcl_Obj *objPtr) 
{
    TkFontPattern *patternPtr;
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
	patternPtr = TkParseNameValuePairs(interp, Tcl_GetObjResult(interp));
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
 * TkGetPattern --
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
static TkFontPattern *
TkGetPattern(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    TkFontPattern *patternPtr;
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
	patternPtr = TkParseNameValuePairs(interp, objPtr);
	if (patternPtr == NULL) {
	    return NULL;		/* XLFD font description */
	}
    } else if (*desc == '*') {
	return NULL;		/* XLFD font description */
    } else if (strpbrk(desc, "::") != NULL) {
	patternPtr = TkParseFontObj(interp, objPtr);
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
	    patternPtr = TkParseFontObj(interp, objv[0]);
	} 
	if (patternPtr == NULL) {
	    /* 
	     * Case 3b: List of font attributes in the form "family size
	     *		?attrs?"
	     */
	    patternPtr = TkParseTkDesc(interp, objc, objv);
	}
    }	
    return patternPtr;
}

static void
TkWriteXLFDDescription(Tk_Window tkwin, TkFontPattern *patternPtr, 
		       Tcl_DString *resultPtr)
{
    const char *string;
    int size;
    
    /* Rewrite the font description using the aliased family. */
    Tcl_DStringInit(resultPtr);

    /* Foundry */
    Tcl_DStringAppend(resultPtr, "-*-", 3);
    /* Family */
    string = (patternPtr->family != NULL) ? patternPtr->family : "*";
    Tcl_DStringAppend(resultPtr, string, -1);
    Tcl_DStringAppend(resultPtr, "-", 1);
    /* Weight */
    string = (patternPtr->weight == NULL) ? "*" : patternPtr->weight;
    Tcl_DStringAppend(resultPtr, string, -1);
    Tcl_DStringAppend(resultPtr, "-", 1);
    /* Slant */
    string = (patternPtr->slant == NULL) ? "*" : patternPtr->slant;
    Tcl_DStringAppend(resultPtr, string, -1);
    Tcl_DStringAppend(resultPtr, "-", 1);
    /* Width */
    string = (patternPtr->width == NULL) ? "*" : patternPtr->width;
    Tcl_DStringAppend(resultPtr, string, -1);
    /* Style */
    Tcl_DStringAppend(resultPtr, "-*-", 3);
    /* Pixel size */
    size = (int)(PointsToPixels(tkwin, patternPtr->size) + 0.5);
    string = (size == 0) ? "*" : Blt_Itoa(size);
    Tcl_DStringAppend(resultPtr, string, -1);
    /* Point size */
    Tcl_DStringAppend(resultPtr, "-", 1);
    size = (int)(PixelsToPoints(tkwin, patternPtr->size) + 0.5);
    string = (size == 0) ? "*" : Blt_Itoa(size);
    Tcl_DStringAppend(resultPtr, string, -1);
    
    /* resx, resy */
    Tcl_DStringAppend(resultPtr, "-*-*-", 5);
    /* Spacing */
    string = (patternPtr->spacing == NULL) ? "*" : patternPtr->spacing;
    Tcl_DStringAppend(resultPtr, string, -1);
    /* Average Width, Registry, Encoding */
    Tcl_DStringAppend(resultPtr, "-*-*-*-", 7);
}
    
/* 
 *---------------------------------------------------------------------------
 *
 * TkGetFontFromObj --
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
static Tk_Font
TkGetFontFromObj(Tcl_Interp *interp, Tk_Window tkwin, Tcl_Obj *objPtr)
{
    Tk_Font tkFont;
    TkFontPattern *patternPtr;

    if (!alias_initialized) {
	MakeAliasTable(tkwin);
	alias_initialized++;
    }
    patternPtr = TkGetPattern(interp, objPtr);
    if (patternPtr == NULL) {
	tkFont = Tk_GetFont(interp, tkwin, Tcl_GetString(objPtr));
    } else {
	Tcl_DString ds;

	/* Rewrite the font description using the aliased family. */
	TkWriteXLFDDescription(tkwin, patternPtr, &ds);
	tkFont = Tk_GetFont(interp, tkwin, Tcl_DStringValue(&ds));
	fprintf(stderr, "Tkfont: %s => %s\n", Tcl_GetString(objPtr),
		Tcl_DStringValue(&ds));
	Tcl_DStringFree(&ds);
	TkFreeFontPattern(patternPtr);
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
TkTextWidthProc(_Blt_Font *fontPtr, const char *string, int nBytes)
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
    Tk_DrawChars(display, drawable, gc, fontPtr->clientData,text, nBytes, x, y);
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
    int x, int y,		/* Coordinates at which first character of
				 * string is drawn. */
    int first,			/* Byte offset of the first character. */
    int last)			/* Byte offset after the last character. */
{
    Tk_UnderlineChars(display, drawable, gc, fontPtr->clientData, string, x, y, 
	first, last);
}


#ifdef HAVE_LIBXFT
#include <ft2build.h>
#include FT_FREETYPE_H
#include <X11/Xft/Xft.h>

static Blt_NameOfFontProc		FtNameOfFontProc;
static Blt_FamilyOfFontProc		FtFamilyOfFontProc;
static Blt_FontIdProc			FtFontIdProc;
static Blt_GetFontMetricsProc		FtGetFontMetricsProc;
static Blt_MeasureCharsProc		FtMeasureCharsProc;
static Blt_TextWidthProc		FtTextWidthProc;
static Blt_FreeFontProc			FtFreeFontProc;
static Blt_DrawCharsProc		FtDrawCharsProc;
static Blt_PostscriptFontNameProc	FtPostscriptFontNameProc;
static Blt_CanRotateFontProc		FtCanRotateFontProc;
static Blt_UnderlineCharsProc		FtUnderlineCharsProc;

static Blt_FontClass ftFontClass = {
    FONT_FT,
    FtNameOfFontProc,		/* Blt_NameOfFontProc */
    FtFamilyOfFontProc,		/* Blt_FamilyOfFontProc */
    FtFontIdProc,		/* Blt_FontIdProc */
    FtGetFontMetricsProc,	/* Blt_GetFontMetricsProc */
    FtMeasureCharsProc,		/* Blt_MeasureCharsProc */
    FtTextWidthProc,		/* Blt_TextWidthProc */
    FtCanRotateFontProc,	/* Blt_CanRotateFontProc */
    FtDrawCharsProc,		/* Blt_DrawCharsProc */
    FtPostscriptFontNameProc,	/* Blt_PostscriptFontNameProc */
    FtFreeFontProc,		/* Blt_FreeFontProc */
    FtUnderlineCharsProc,	/* Blt_UnderlineCharsProc */
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

    Font fid;			/* Font id used to fake out Tk_FontId. */

    FcPattern *pattern;		/* Pattern matching the current non-rotated
				 * font. Used to create rotated fonts by
				 * duplicating the pattern and adding a
				 * rotation matrix. */

    Blt_HashTable fontTable;	/* Hash table containing an Xft font for each
				 * angle it's used at. Will always contain a 0
				 * degree entry. */

    /* Information specific to the display/drawable being used. The drawables
     * are changed as the drawable changes for each drawing request.
     * Typically this will change for each pixmap. */

    Drawable drawable;		/* Drawable associated with draw. */
    XftDraw *draw;		/* Current Xft drawable. */
    int drawDepth;		/* Depth of current drawable. */

    XftColor color;		/* Color to be displayed.  We don't actually
				 * allocate this color, since we assume it's
				 * been already allocated by the standard Tk
				 * procedures. */

    /* Saved Information from the Tk_Window used to created the initial
     * font. */
    Display *display;		
    Visual *visual;
    int screenNum;
    Colormap colormap;

    int underlineHeight;	/* Thickness of underline rectangle. */
    int underlinePos;		/* Offset of underline. */
    int tabWidth;
} FtFont;

static FontSpec rgbaSpecs[] = {
    { "bgr",	 1, FC_RGBA, FC_RGBA_BGR,     },
    { "none",	 1, FC_RGBA, FC_RGBA_NONE,    },
    { "rgb",	 1, FC_RGBA, FC_RGBA_RGB,     },
    { "unknown", 1, FC_RGBA, FC_RGBA_UNKNOWN, },
    { "vbgr",	 2, FC_RGBA, FC_RGBA_VBGR,    },
    { "vrgb",	 2, FC_RGBA, FC_RGBA_VRGB,    },
};
static int nRgbaSpecs = sizeof(rgbaSpecs) / sizeof(FontSpec);

static FontSpec hintSpecs[] = {
    { "full",	 1, FC_HINT_STYLE, FC_HINT_FULL,    },
    { "medium",	 1, FC_HINT_STYLE, FC_HINT_MEDIUM,  },
    { "none",    1, FC_HINT_STYLE, FC_HINT_NONE,    },
    { "slight",	 1, FC_HINT_STYLE, FC_HINT_SLIGHT,  },
};
static int nHintSpecs = sizeof(hintSpecs) / sizeof(FontSpec);

static void
FtGetFontFamilies(Tk_Window tkwin, Blt_HashTable *tablePtr)
{
    XftFontSet *fsPtr;
    int i;
    
    fsPtr = XftListFonts(Tk_Display(tkwin), 
			 Tk_ScreenNumber(tkwin), 
			 (char*)NULL, /* pattern elements */
			 XFT_FAMILY, (char*)NULL); /* fields */
    for (i = 0; i < fsPtr->nfont; i++) {
	FcResult result;
	FcChar8 *family;
	
	result = FcPatternGetString(fsPtr->fonts[i], FC_FAMILY, 0, &family);
	if (result == FcResultMatch) {
	    int isNew;
	    char *name;

	    /* Family names must be all lower case in the hash table. */
	    name = Blt_AssertStrdup((const char *)family);
	    strtolower(name);
	    Blt_CreateHashEntry(tablePtr, name, &isNew);
	    Blt_Free(name);
	}
    }
    XftFontSetDestroy(fsPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * FtParseTkDesc --
 *
 *	Try to open a Xft font from an Tk style font description.
 *
 * Results:
 *	Return value is TCL_ERROR if string was not a fully specified XLFD.
 *	Otherwise, fills font attribute buffer with the values parsed from the
 *	XLFD and returns TCL_OK.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static FcPattern *
FtParseTkDesc(Tcl_Interp *interp, Tk_Window tkwin, int objc, Tcl_Obj **objv)
{
    FcPattern *pattern;
    int i;
    const char *family;

    pattern = FcPatternCreate();
    FcPatternAddBool(pattern, FC_ANTIALIAS, FcTrue);

    /* Font family. */
    family = GetAlias(Tcl_GetString(objv[0]));
    FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *)family);

    /* Size */
    if (objc > 1) {
	int size;

	if (Tcl_GetIntFromObj(NULL, objv[1], &size) != TCL_OK) {
	    goto error;
	}
	FcPatternAddDouble(pattern, FC_SIZE, PixelsToPoints(tkwin, size));
    }
    i = 2;
    if (objc == 3) {
	if (Tcl_ListObjGetElements(interp, objv[2], &objc, &objv) != TCL_OK) {
	    goto error;
	}
	i = 0;
    }
    for (/*empty*/; i < objc; i++) {
	FontSpec *specPtr;
	
	specPtr = FindSpec(interp, fontSpecs, nFontSpecs, 
			   Tcl_GetString(objv[i]));
	if (specPtr == NULL) {
	    goto error;
	}
	if (specPtr->key != NULL) {
	    FcPatternAddInteger(pattern, specPtr->key, specPtr->value);
	}
    }
#if DEBUG_FONT_SELECTION
    fprintf(stderr, "parsed TkDesc-XFT font \"%s\"\n", Tcl_GetString(objv[0]));
#endif
    return pattern;
 error:
    if (pattern != NULL) {
	FcPatternDestroy(pattern);
    }
    return NULL;
}	

static FcPattern *
FtParseTkFontAttributeList(Tcl_Interp *interp, Tk_Window tkwin, 
			   Tcl_Obj *objPtr) 
{
    FcPattern *pattern;
    Tcl_Obj **objv;
    int objc;
    int i;

    if ((Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) ||
	(objc < 1)) {
	return NULL;		/* Can't split list or list is empty. */
    }
    if (objc & 0x1) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "odd number of elements, missing value",
			 (char *)NULL);
	}
	return NULL;		/* Odd number of elements in list. */
    }
    pattern = FcPatternCreate();
    FcPatternAddBool(pattern, FC_ANTIALIAS, FcTrue);
    for (i = 0; i < objc; i += 2) {
	char *key, *value;

	key = Tcl_GetString(objv[i]);
	value = Tcl_GetString(objv[i+1]);
	if (strcmp(key, "-family") == 0) {
	    const char *family;

	    family = GetAlias(value);
	    FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *)family);
	} else if (strcmp(key, "-size") == 0) {
	    int size;

	    if (Tcl_GetIntFromObj(interp, objv[i+1], &size) != TCL_OK) {
		goto error;
	    }
	    FcPatternAddDouble(pattern, FC_SIZE, PixelsToPoints(tkwin, size));
	} else if (strcmp(key, "-weight") == 0) {
	    FontSpec *specPtr;

	    specPtr = FindSpec(interp, weightSpecs, nWeightSpecs, value);
	    if (specPtr == NULL) {
		goto error;
	    }
	    FcPatternAddInteger(pattern, FC_WEIGHT, specPtr->value);
	} else if (strcmp(key, "-slant") == 0) {
	    FontSpec *specPtr;

	    specPtr = FindSpec(interp, slantSpecs, nSlantSpecs, value);
	    if (specPtr == NULL) {
		goto error;
	    }
	    FcPatternAddInteger(pattern, FC_SLANT, specPtr->value);
	} else if (strcmp(key, "-hint") == 0) {
	    FontSpec *specPtr;

	    specPtr = FindSpec(interp, hintSpecs, nHintSpecs, value);
	    if (specPtr == NULL) {
		goto error;
	    }
	    FcPatternAddInteger(pattern, FC_HINT_STYLE, specPtr->value);
	} else if (strcmp(key, "-rgba") == 0) {
	    FontSpec *specPtr;

	    specPtr = FindSpec(interp, rgbaSpecs, nRgbaSpecs, value);
	    if (specPtr == NULL) {
		goto error;
	    }
	    FcPatternAddInteger(pattern, FC_RGBA, specPtr->value);
	} else if (strcmp(key, "-underline") == 0) {
	    /* Ignore */
	} else if (strcmp(key, "-overstrike") == 0) {
	    /* Ignore */
	} else {
	    FontSpec *specPtr;
	    int bool;

	    specPtr = FindSpec(interp, boolSpecs, nBoolSpecs, key+1);
	    if (specPtr == NULL) {
		goto error;
	    }
	    if (Tcl_GetBooleanFromObj(interp, objv[i+1], &bool) != TCL_OK) {
		goto error;
	    }
	    FcPatternAddBool(pattern, specPtr->key, bool);
	}
    }
#if DEBUG_FONT_SELECTION
    fprintf(stderr, "parsed TkAttrList-XFT font \"%s\"\n", 
	    Tcl_GetString(objPtr));
#endif
    return pattern;
 error:
    FcPatternDestroy(pattern);
    return NULL;
}

static FcPattern *
FtGetAttributesFromFontObj(Tk_Window tkwin, Tcl_Interp *interp, 
			   Tcl_Obj *objPtr) 
{
    FcPattern *pattern;
    Tcl_Obj *cmd[3];
    int result;

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
	pattern = FtParseTkFontAttributeList(interp, tkwin, 
		Tcl_GetObjResult(interp));
    } else {
	pattern = NULL;
    }
    Tcl_ResetResult(interp);
#if DEBUG_FONT_SELECTION
    if (pattern != NULL) {
	fprintf(stderr, "found FontObject => XFT font \"%s\"\n", 
	    Tcl_GetString(objPtr));
    }
#endif
    return pattern;
}

/*
 *---------------------------------------------------------------------------
 *
 * FtParseXLFD --
 *
 *	Try to open a Xft font from an XLFD description.
 *
 * Results:
 *	Return value is TCL_ERROR if string was not a fully specified XLFD.
 *	Otherwise, fills font attribute buffer with the values parsed from the
 *	XLFD and returns TCL_OK.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static FcPattern *
FtParseXLFD(Tcl_Interp *interp, Tk_Window tkwin, char *fontName)
{
    FcPattern *pattern;
    FontSpec *specPtr;
    int argc;
    char **argv;
    double size;

    if (fontName[0] == '-') {
	fontName++;
    }
    ParseXLFD(fontName, &argc, &argv);

    pattern = FcPatternCreate();
    FcPatternAddBool(pattern, FC_ANTIALIAS, FcTrue);

    if (argv[XLFD_FOUNDRY] != NULL) {
	FcPatternAddString(pattern, FC_FOUNDRY, 
		(const FcChar8 *)argv[XLFD_FOUNDRY]);
    }
    if (argv[XLFD_FAMILY] != NULL) {
	FcPatternAddString(pattern, FC_FAMILY, 
		(const FcChar8 *)argv[XLFD_FAMILY]);
    }
    if (argv[XLFD_WEIGHT] != NULL) {
	specPtr = FindSpec(interp, weightSpecs, nWeightSpecs, 
			   argv[XLFD_WEIGHT]);
	if (specPtr == NULL) {
	    goto error;
	}
	FcPatternAddInteger(pattern, FC_WEIGHT, specPtr->value);
    }
    if (argv[XLFD_SLANT] != NULL) {
	specPtr = FindSpec(interp, slantSpecs, nSlantSpecs, argv[XLFD_SLANT]);
	if (specPtr == NULL) {
	    goto error;
	}
	FcPatternAddInteger(pattern, FC_SLANT, specPtr->value);
    }
    if (argv[XLFD_SETWIDTH] != NULL) {
	specPtr = FindSpec(interp, widthSpecs, nWidthSpecs, 
			   argv[XLFD_SETWIDTH]);
	if (specPtr == NULL) {
	    goto error;
	}
	FcPatternAddInteger(pattern, FC_WIDTH, specPtr->value);
    }
    if (argv[XLFD_ADD_STYLE] != NULL) {
	FcPatternAddString(pattern, FC_STYLE, 
		(const FcChar8 *)argv[XLFD_ADD_STYLE]);
    }
    size = 12.0;
    if (argv[XLFD_PIXEL_SIZE] != NULL) {
	int value;
	if (argv[XLFD_PIXEL_SIZE][0] == '[') {
	    /*
	     * Some X fonts have the point size specified as follows:
	     *
	     *	    [ N1 N2 N3 N4 ]
	     *
	     * where N1 is the point size (in points, not decipoints!), and
	     * N2, N3, and N4 are some additional numbers that I don't know
	     * the purpose of, so I ignore them.
	     */
	    value = atoi(argv[XLFD_PIXEL_SIZE]+1);
	} else if (Tcl_GetInt(NULL, argv[XLFD_PIXEL_SIZE], &value) == TCL_OK) {
	    /* empty */
	} else {
	    goto error;
	}
	size = PixelsToPoints(tkwin, -value);
	fprintf(stderr, "pixel size = %s,%g\n",  argv[XLFD_PIXEL_SIZE], 
		size);
    }
#ifndef notdef
    if (argv[XLFD_POINT_SIZE] != NULL) {
	int value;
	if (argv[XLFD_POINT_SIZE][0] == '[') {
	    /*
	     * Some X fonts have the point size specified as follows:
	     *
	     *	    [ N1 N2 N3 N4 ]
	     *
	     * where N1 is the point size (in points, not decipoints!), and
	     * N2, N3, and N4 are some additional numbers that I don't know
	     * the purpose of, so I ignore them.
	     */
	    value = atoi(argv[XLFD_POINT_SIZE]+1);
	} else if (Tcl_GetInt(NULL, argv[XLFD_POINT_SIZE], &value) == TCL_OK) {
	    /* empty */
	} else {
	    goto error;
	}
	size = PixelsToPoints(tkwin, -value) * 0.1;
	fprintf(stderr, "point size = %s,%g\n",  argv[XLFD_POINT_SIZE],size);
    }
#endif
    FcPatternAddDouble(pattern, FC_SIZE, (double)size);

    if (argv[XLFD_SPACING] != NULL) {
	specPtr = FindSpec(interp, spacingSpecs, nSpacingSpecs, 
			   argv[XLFD_SPACING]);
	if (specPtr == NULL) {
	    goto error;
	}
	FcPatternAddInteger(pattern, FC_SPACING, specPtr->value);
    }
#ifdef notdef
    if (argv[XLFD_CHARSET] != NULL) {
	FcPatternAddString(pattern, FC_CHARSET, 
		(const FcChar8 *)argv[XLFD_CHARSET]);
    } else {
	FcPatternAddString(pattern, FC_CHARSET, (const FcChar8 *)"iso8859-1");
    }
#endif
    Blt_Free((char *)argv);
#if DEBUG_FONT_SELECTION
    fprintf(stderr, "parsed Xlfd-XFT font \"%s\"\n", fontName);
#endif
    return pattern;
 error:
#if DEBUG_FONT_SELECTION
    fprintf(stderr, "can't open font \"%s\" as XLFD\n", fontName);
#endif
    Blt_Free((char *)argv);
    FcPatternDestroy(pattern);
    return NULL;
}


static void
FtDeleteFont(FtFont *ftPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    
    for (hPtr = Blt_FirstHashEntry(&ftPtr->fontTable, &cursor); 
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	XftFont *xftPtr;
	
	xftPtr = Blt_GetHashValue(hPtr);
	XftFontClose(ftPtr->display, xftPtr);
    }
    Blt_DeleteHashTable(&ftPtr->fontTable);
    
    if (ftPtr->name != NULL) {
	Blt_Free(ftPtr->name);
    }
    if (ftPtr->draw != 0) {
	XftDrawDestroy(ftPtr->draw);
    }
    if (ftPtr->fid) {
	XUnloadFont(ftPtr->display, ftPtr->fid);
    }
    Blt_DeleteHashEntry(&fontTable, ftPtr->hashPtr);
    Blt_Free(ftPtr);
}

static int
FtMeasureChars(FtFont *ftPtr, const char *source, int nBytes, int maxLength,
	       int flags, int *lengthPtr)
{
    FcChar32 c;
    XGlyphInfo extents;
    int clen;
    int curX, newX;
    int termByte = 0, termX = 0;
    int curByte, newByte, sawNonSpace;
#if 0
    char string[256];
    int len = 0;
#endif
    XftFont *xftPtr;
    Blt_HashEntry *hPtr;
    
    hPtr = Blt_FindHashEntry(&ftPtr->fontTable, (char *)0L);
    if (hPtr == NULL) {
	return 0;
    }
    xftPtr = Blt_GetHashValue(hPtr);
    curX = 0;
    curByte = 0;
    sawNonSpace = 0;
    while (nBytes > 0) {
	Tcl_UniChar unichar;

	clen = Tcl_UtfToUniChar(source, &unichar);
	c = (FcChar32)unichar;

	if (clen <= 0) {
	    /* This can't happen (but see #1185640) */
	    *lengthPtr = curX;
	    return curByte;
	}

	source += clen;
	nBytes -= clen;
	if (c < 256 && isspace(c)) {		/* I18N: ??? */
	    if (sawNonSpace) {
		termByte = curByte;
		termX = curX;
		sawNonSpace = 0;
	    }
	} else {
	    sawNonSpace = 1;
	}

#if 0
	string[len++] = (char) c;
#endif
	XftTextExtents32(ftPtr->display, xftPtr, &c, 1, &extents);

	newX = curX + extents.xOff;
	newByte = curByte + clen;
	if (maxLength >= 0 && newX > maxLength) {
	    if ((flags & TK_PARTIAL_OK) ||
		((flags & TK_AT_LEAST_ONE && curByte == 0))) {
		curX = newX;
		curByte = newByte;
	    } else if ((flags & TK_WHOLE_WORDS) && (termX != 0)) {
		curX = termX;
		curByte = termByte;
	    }
	    break;
	}

	curX = newX;
	curByte = newByte;
    }
#if 0
    string[len] = '\0';
    printf("MeasureChars %s length %d bytes %d\n", string, curX, curByte);
#endif
    *lengthPtr = curX;
    return curByte;
}

static void
FtSetFontParams(Tk_Window tkwin, FtFont *ftPtr, XftFont *xftPtr)
{
    FT_UInt glyph;
    XGlyphInfo metrics;
    double size;
    FcResult result;

    /*
     * Get information used for drawing underlines from the 0 angle font.
     */
    glyph = XftCharIndex(ftPtr->display, xftPtr, '0');
    XftGlyphExtents(ftPtr->display, xftPtr, &glyph, 1, &metrics);
    
    ftPtr->underlinePos = xftPtr->descent / 2;
    result = FcPatternGetDouble(xftPtr->pattern, FC_SIZE, 0, &size);
    if (result != FcResultMatch) {
	size = 12.0;
    }
    ftPtr->underlineHeight = (int)(PointsToPixels(tkwin,(int)size)/10.0 + 0.5);
    if (ftPtr->underlineHeight == 0) {
	ftPtr->underlineHeight = 1;
    }
    if ((ftPtr->underlinePos + ftPtr->underlineHeight) > xftPtr->descent) {
	/*
	 * If this set of values would cause the bottom of the underline bar
	 * to stick below the descent of the font, jack the underline up a bit
	 * higher.
	 */
	ftPtr->underlineHeight = xftPtr->descent - ftPtr->underlinePos;
	if (ftPtr->underlineHeight == 0) {
	    ftPtr->underlinePos--;
	    ftPtr->underlineHeight = 1;
	}
    }
    FtMeasureChars(ftPtr, "0", 1, -1, 0, &ftPtr->tabWidth);
    if (ftPtr->tabWidth == 0) {
	ftPtr->tabWidth = xftPtr->max_advance_width;
    }
    ftPtr->tabWidth *= 8;
    /*
     * Make sure the tab width isn't zero (some fonts may not have enough
     * information to set a reasonable tab width).
     */
    if (ftPtr->tabWidth == 0) {
	ftPtr->tabWidth = 1;
    }
}

static FtFont *
FtNewFont(
    Tk_Window tkwin, 
    const char *fontName, 
    XftFont *xftPtr)
{
    Blt_HashEntry *hPtr;
    FtFont *ftPtr;
    int isNew;

    ftPtr = Blt_AssertCalloc(1, sizeof(FtFont));
    ftPtr->name = Blt_AssertStrdup(fontName);
    ftPtr->visual = Tk_Visual(tkwin);
    ftPtr->colormap = Tk_Colormap(tkwin);
    ftPtr->display = Tk_Display(tkwin);
    ftPtr->fid = XLoadFont(Tk_Display(tkwin), "fixed");
    ftPtr->color.pixel = 0xFFFFFFFF;
    ftPtr->pattern = xftPtr->pattern;
    ftPtr->refCount = 1;
	    
    /* 
     * Initialize the Xft font table for this font.  Add the initial Xft font
     * for the case of 0 degrees rotation.
     */
    Blt_InitHashTable(&ftPtr->fontTable, BLT_ONE_WORD_KEYS);
    hPtr = Blt_CreateHashEntry(&ftPtr->fontTable, (char *)0L, &isNew);
    assert(isNew);
    Blt_SetHashValue(hPtr, xftPtr);

#ifdef notdef
    /* Try to use the Freetype routine FT_Get_Postscript_Name.  If this fails,
     * build a string from the various fields of the font. */
    {
	FT_Face face;
	const char *string;
	
	face = XftLockFace(xftPtr);
	string = FT_Get_Postscript_Name(face);
	XftUnlockFace(xftPtr);
	if (string != NULL) {
	    fprintf(stderr, "%s: psfont=(%s)\n", fontName, string);
	}
    }
#endif
    /* Add the font information to the font table. */
    hPtr = Blt_CreateHashEntry(&fontTable, fontName, &isNew);
    assert(isNew);
    Blt_SetHashValue(hPtr, ftPtr);
    ftPtr->hashPtr = hPtr;
    FtSetFontParams(tkwin, ftPtr, xftPtr);
    return ftPtr;
}

/*
 * FtGetPattern --
 *
 *	Generates an pattern based upon the font description provided.  The
 *	description is parsed base upon Tk's font selection rules (listed
 *	below).
 *
 *      Tk's Font Selection Rules:
 *
 *	When font description font is used, the system attempts to parse the
 *	description according to each of the above five rules, in the order
 *	specified.  Cases [1] and [2] must match the name of an existing named
 *	font or of a system font.  Cases [3], [4], and [5] are accepted on all
 *	platforms and the closest available font will be used.  In some
 *	situations it may not be possible to find any close font (e.g., the
 *	font family was a garbage value); in that case, some system-dependant
 *	default font is chosen.  If the font description does not match any of
 *	the above patterns, an error is generated.
 *
 * [1] fontname
 *	The name of a named font, created using the font create command.  When
 *	a widget uses a named font, it is guaranteed that this will never
 *	cause an error, as long as the named font exists, no mat- ter what
 *	potentially invalid or meaningless set of attributes the named font
 *	has.  If the named font cannot be displayed with exactly the specified
 *	attributes, some other close font will be substituted automatically.
 *	
 *	[Query the named font (using "font configure") and generate an Xft
 *	font with the same attributes.  It's assumed that these names don't
 *	start with a '*' or '-'.]
 *
 * [2] systemfont
 *	The platform-specific name of a font, interpreted by the graphics
 *	server.  This also includes, under X, an XLFD (see [4]) for which a
 *	single ``*'' character was used to elide more than one field in the
 *	middle of the name.  See PLATFORM-SPECIFIC issues for a list of the
 *	system fonts.
 *
 *	[Same as above. Query the named font (using "font configure") and
 *	generate an Xft font with the same attributes.]
 *
 * [3] family ?size? ?style? ?style ...? 
 *	A properly formed list whose first element is the desired font family
 *	and whose optional second element is the desired size.  The
 *	interpretation of the size attribute follows the same rules described
 *	for -size in FONT OPTIONS below.  Any additional optional arguments
 *	following the size are font styles.  Possible values for the style
 *	arguments are as follows:
 *
 *	   normal, bold, roman, italic, underline, overstrike 
 *
 *	[Parse the list of attributes and generate a corresponding Xft font.]
 *
 * [4] X-font names (XLFD)
 *	A Unix-centric font name of the form -foundry-family-weight
 *	slant-setwidth-addstyle-pixel-point-resx-resy-spacing-width
 *	charset-encoding.  The ``*'' character may be used to skip indi vidual
 *	fields that the user does not care about.  There must be exactly one
 *	``*'' for each field skipped, except that a ``*'' at the end of the
 *	XLFD skips any remaining fields; the shortest valid XLFD is simply
 *	``*'', signifying all fields as defaults.  Any fields that were
 *	skipped are given default values.  For compatibility, an XLFD always
 *	chooses a font of the specified pixel size (not point size); although
 *	this interpretation is not strictly correct, all existing applications
 *	using XLFDs assumed that one ``point'' was in fact one pixel and would
 *	display incorrectly (generally larger) if the correct size font were
 *	actually used.
 *
 *	[Parse the font description and generate a corresponding Xft font.]
 *
 * [5] option value ?option value ...?
 *	A properly formed list of option-value pairs that specify the desired
 *	attributes of the font, in the same format used when defining a named
 *	font.
 *
 *	[Parse the option-value list and generate a corresponding Xft font.]
 *
 *  Extra:
 * [6] Xft font description.
 *
 *	[Handle the newer Xft font descriptions.]
 */

static FcPattern *
FtGetPattern(Tcl_Interp *interp, Tk_Window tkwin, Tcl_Obj *objPtr)
{
    FcPattern *pattern;
    char *desc;

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
	pattern = FtParseTkFontAttributeList(NULL, tkwin, objPtr);
	if (pattern == NULL) {
	    /* Try parsing it as an XLFD font description. */
	    pattern = FtParseXLFD(interp, tkwin, desc);
	}
    } else if (*desc == '*') {
	pattern = FtParseXLFD(interp, tkwin, desc);
    } else if (strpbrk(desc, ":,=") != NULL) {
	/* 
	 * Case 2: XFT font description.   
	 *
	 *   If the font description contains a ':', '=', or ',' in it, we
	 *   assume it's a new XFT font description. We want to allow these
	 *   font descriptions too.
	 */
	pattern = NULL;
	if (strstr(desc, "::") != NULL) {
	    pattern = FtGetAttributesFromFontObj(tkwin, interp, objPtr);
	} 
	if (pattern == NULL) {
	    pattern = FcNameParse((const FcChar8 *)desc);
	}
#if DEBUG_FONT_SELECTION
	if (pattern != NULL) {
	    fprintf(stderr, "found XFT => XFT font \"%s\"\n", desc);
	}
#endif
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
	if (objc == 1) {
	    /* 
	     * Case 3a: Tk font object name.
	     *
	     *   Assuming that Tk font object names won't contain whitespace,
	     *   see if its a font object.
	     */
	    pattern = FtGetAttributesFromFontObj(tkwin, interp, objv[0]);
	    if (pattern == NULL) {
		pattern = FcNameParse((const FcChar8 *)desc);
#if DEBUG_FONT_SELECTION
		if (pattern != NULL) {
		    fprintf(stderr, "found XFT => XFT font \"%s\"\n", desc);
		}
#endif
	    }
	} else {
	    /* 
	     * Case 3b: List of font attributes in the form "family size
	     *		?attrs?"
	     */
	    pattern = FtParseTkDesc(interp, tkwin, objc, objv);
	}
    }	
    return pattern;
}

static XftFont *
FtOpenFont(Tcl_Interp *interp, Tk_Window tkwin, Tcl_Obj *objPtr)
{
    FcPattern *pattern;

    pattern = FtGetPattern(interp, tkwin, objPtr);
    if (pattern != NULL) {
	FcPattern *match;
	FcResult result;

	/* 
	 * XftFontMatch only sets *result* on complete match failures.  So
	 * initialize it here for a successful match. We'll accept partial
	 * matches.
	 */
	result = FcResultMatch; 
	match = XftFontMatch(Tk_Display(tkwin), Tk_ScreenNumber(tkwin), 
		pattern, &result);
#if DEBUG_FONT_SELECTION
	if (match != NULL) {
	    FcChar8 *name;

	    name = FcNameUnparse(pattern);
	    fprintf(stderr, "\nfont=%s\n", Tcl_GetString(objPtr));
	    fprintf(stderr, "orignal spec was %s\n", name);
	    free(name);
	    name = FcNameUnparse(match);
	    fprintf(stderr, "matching spec is %s\n", name);
	    free(name);
	}
#endif
	FcPatternDestroy(pattern);
	if ((match != NULL) && (result == FcResultMatch)) {
	    return XftFontOpenPattern(Tk_Display(tkwin), match);
	}
    }
#if DEBUG_FONT_SELECTION
    fprintf(stderr, "can't open font pattern \"%s\"\n", Tcl_GetString(objPtr));
#endif
    return NULL;
}


XftFont *
Blt_OpenXftFontFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    Tk_Window tkwin;
    XftFont *xftFont;

    tkwin = Tk_MainWindow(interp);
    if (!alias_initialized) {
	MakeAliasTable(tkwin);
	alias_initialized++;
    }
    if (!IsXRenderAvailable(tkwin)) {
	Tcl_AppendResult(interp, "can't open Xft font: ",
		"X server doesn't support XRENDER extension",
		(char *)NULL);
	return NULL;
    }
    xftFont = FtOpenFont(interp, tkwin, objPtr);
    if (xftFont != NULL) {
	xftFont = XftFontCopy(Tk_Display(tkwin), xftFont);
    }
    return xftFont;
}

XftFont *
Blt_OpenXftFont(Tcl_Interp *interp, const char *fontName)
{
    XftFont *xftPtr;
    Tcl_Obj *objPtr;

    objPtr = Tcl_NewStringObj(fontName, strlen(fontName));
    Tcl_IncrRefCount(objPtr);
    xftPtr = Blt_OpenXftFontFromObj(interp, objPtr);
    Tcl_DecrRefCount(objPtr);
    return xftPtr;
}

static FtFont *
FtGetFontFromObj(Tcl_Interp *interp, Tk_Window tkwin, Tcl_Obj *objPtr)
{
    Blt_HashEntry *hPtr;
    char *desc;

    desc = Tcl_GetString(objPtr);
    while (isspace(UCHAR(*desc))) {
	desc++;			/* Skip leading blanks. */
    }
    /* Is the font already in the cache? */
    hPtr = Blt_FindHashEntry(&fontTable, desc);
    if (hPtr != NULL) {
	FtFont *ftPtr;

	ftPtr = Tcl_GetHashValue(hPtr);
	ftPtr->refCount++;
	return ftPtr;
    } else {
	XftFont *xftPtr;

	xftPtr = FtOpenFont(interp, tkwin, objPtr);
	if (xftPtr != NULL) {
	    return FtNewFont(tkwin, desc, xftPtr);
	}
    }
    return NULL;
}

static const char *
FtNameOfFontProc(_Blt_Font *fontPtr) 
{
    FtFont *ftPtr = fontPtr->clientData;
    return ftPtr->name;
}

static const char *
FtFamilyOfFontProc(_Blt_Font *fontPtr) 
{
    FtFont *ftPtr = fontPtr->clientData;
    FcChar8 *string; 
    FcResult result;

    result = FcPatternGetString(ftPtr->pattern, FC_FAMILY, 0, &string);
    if (result == FcResultMatch) {
	return (const char *)string;
    }
    return NULL;
}


static Font
FtFontIdProc(_Blt_Font *fontPtr)
{
    FtFont *ftPtr = fontPtr->clientData;
    return ftPtr->fid;
}

static void
FtGetFontMetricsProc(_Blt_Font *fontPtr, Blt_FontMetrics *mPtr)
{
    FtFont *ftPtr = fontPtr->clientData;
    Blt_HashEntry *hPtr;

    /* Always take font metrics from the non-rotated font. */
    hPtr = Blt_FindHashEntry(&ftPtr->fontTable, (char *)0L);
    if (hPtr != NULL) {
	FT_UInt glyph;
	XGlyphInfo metrics;
	XftFont *xftPtr;

	xftPtr = Blt_GetHashValue(hPtr);
	glyph = XftCharIndex(ftPtr->display, xftPtr, '0');
	XftGlyphExtents(ftPtr->display, xftPtr, &glyph, 1, &metrics);
	mPtr->ascent = xftPtr->ascent;
	mPtr->descent = xftPtr->descent;
	mPtr->linespace = mPtr->ascent + mPtr->descent;
	mPtr->tabWidth = ftPtr->tabWidth;
	mPtr->underlinePos = ftPtr->underlinePos;
	mPtr->underlineHeight = ftPtr->underlineHeight;
    }
}

static int
FtMeasureCharsProc(
    _Blt_Font *fontPtr,
    const char *source,	
    int nBytes,
    int maxLength,
    int flags,
    int *lengthPtr)
{
    FtFont *ftPtr = fontPtr->clientData;

    return FtMeasureChars(ftPtr, source, nBytes, maxLength, flags, lengthPtr);
}

static int
FtTextWidthProc(Blt_Font font, const char *string, int nBytes)
{
    int width;

    FtMeasureCharsProc(font, string, nBytes, -1, 0, &width);
    return width;
}    

/*
 *---------------------------------------------------------------------------
 *
 * FtPostscriptFontNameProc --
 *
 *	Given a Xft font, return the name of the corresponding Postscript
 *	font.
 *
 * Results:
 *	The return value is the pointsize of the given Xft font.  The name of
 *	the Postscript font is appended to dsPtr.
 *
 * Side effects:
 *	If the font does not exist on the printer, the print job will fail at
 *	print time.  Given a "reasonable" Postscript printer, the following
 *	Tk_Font font families should print correctly:
 *
 *	    Avant Garde, Arial, Bookman, Courier, Courier New, Geneva,
 *	    Helvetica, Monaco, New Century Schoolbook, New York, Palatino,
 *	    Symbol, Times, Times New Roman, Zapf Chancery, and Zapf Dingbats.
 *
 *	Any other Xft font families may not print correctly because the
 *	computed Postscript font name may be incorrect.
 *
 *---------------------------------------------------------------------------
 */

static int
FtPostscriptFontNameProc(_Blt_Font *fontPtr, Tcl_DString *dsPtr)	
{
    FtFont *ftPtr = fontPtr->clientData;
    FcChar8 *string;
    const char *familyName, *weightName, *slantName;
    FcResult result;
    int weight, slant;
    double size;
    int len;

    result = FcPatternGetDouble(ftPtr->pattern, FC_SIZE, 0, &size);
    if (result != FcResultMatch) {
	size = 12.0;
    }
#ifdef notdef
    /* Try to use the Freetype routine FT_Get_Postscript_Name.  If this fails,
     * build a string from the various fields of the font. */
    {
	Blt_HashEntry *hPtr;

	/* Always take font metrics from the non-rotated font. */
	hPtr = Blt_FindHashEntry(&ftPtr->fontTable, (char *)0L);
	if (hPtr != NULL) {
	    XftFont *xftPtr;
	    FT_Face face;
	    char *string;
	    
	    xftPtr = Blt_GetHashValue(hPtr);
	    face = XftLockFace(xftPtr);
	    string = FT_Get_Postscript_Name(face);
	    XftUnlockFace(xftPtr);
	    
	    if (string != NULL) {
		Tcl_DStringAppend(dsPtr, string, -1);
		return (int)size;
	    }
	}
    }
#endif

    len = Tcl_DStringLength(dsPtr);
    /*
     * Convert the case-insensitive Tk_Font family name to the case-sensitive
     * Postscript family name.  Take out any spaces and capitalize the first
     * letter of each word.
     */
    familyName = "Unknown";
    result = FcPatternGetString(ftPtr->pattern, FC_FAMILY, 0, &string);
    if (result == FcResultMatch) {
	familyName = (const char *)string;
    }
    if (strncasecmp(familyName, "itc ", 4) == 0) {
	familyName += 4;
    }
    if ((strcasecmp(familyName, "Arial") == 0) || 
	(strcasecmp(familyName, "Geneva") == 0)) {
	familyName = "Helvetica";
    } else if ((strcasecmp(familyName, "Times New Roman") == 0) || 
	       (strcasecmp(familyName, "New York") == 0)) {
	familyName = "Times";
    } else if ((strcasecmp(familyName, "Courier New") == 0) || 
	       (strcasecmp(familyName, "Monaco") == 0)) {
	familyName = "Courier";
    } else if (strcasecmp(familyName, "AvantGarde") == 0) {
	familyName = "AvantGarde";
    } else if (strcasecmp(familyName, "ZapfChancery") == 0) {
	familyName = "ZapfChancery";
    } else if (strcasecmp(familyName, "ZapfDingbats") == 0) {
	familyName = "ZapfDingbats";
    } else {
	Tcl_UniChar ch;
	char *src, *dest;
	int upper;

	/*
	 * Inline, capitalize the first letter of each word, lowercase the
	 * rest of the letters in each word, and then take out the spaces
	 * between the words.  This may make the DString shorter, which is
	 * safe to do.
	 */

	Tcl_DStringAppend(dsPtr, familyName, -1);

	src = dest = Tcl_DStringValue(dsPtr) + len;
	upper = 1;
	for (; *src != '\0'; ) {
	    while (isspace(UCHAR(*src))) { /* INTL: ISO space */
		src++;
		upper = 1;
	    }
	    src += Tcl_UtfToUniChar(src, &ch);
	    if (upper) {
		ch = Tcl_UniCharToUpper(ch);
		upper = 0;
	    } else {
	        ch = Tcl_UniCharToLower(ch);
	    }
	    dest += Tcl_UniCharToUtf(ch, dest);
	}
	*dest = '\0';
	Tcl_DStringSetLength(dsPtr, dest - Tcl_DStringValue(dsPtr));
	familyName = Tcl_DStringValue(dsPtr) + len;
    }
    if (familyName != Tcl_DStringValue(dsPtr) + len) {
	Tcl_DStringAppend(dsPtr, familyName, -1);
	familyName = Tcl_DStringValue(dsPtr) + len;
    }

    if (strcasecmp(familyName, "NewCenturySchoolbook") == 0) {
	Tcl_DStringSetLength(dsPtr, len);
	Tcl_DStringAppend(dsPtr, "NewCenturySchlbk", -1);
	familyName = Tcl_DStringValue(dsPtr) + len;
    }

    /*
     * Get the string to use for the weight.
     */
    result = FcPatternGetInteger(ftPtr->pattern, FC_WEIGHT, 0, &weight);
    if (result != FcResultMatch) {
	weight = FC_WEIGHT_MEDIUM;
    }
    weightName = NULL;
    if (weight <= FC_WEIGHT_MEDIUM) {
	if (strcmp(familyName, "Bookman") == 0) {
	    weightName = "Light";
	} else if (strcmp(familyName, "AvantGarde") == 0) {
	    weightName = "Book";
	} else if (strcmp(familyName, "ZapfChancery") == 0) {
	    weightName = "Medium";
	}
    } else {
	if ((strcmp(familyName, "Bookman") == 0) || 
	    (strcmp(familyName, "AvantGarde") == 0)) {
	    weightName = "Demi";
	} else {
	    weightName = "Bold";
	}
    }

    /*
     * Get the string to use for the slant.
     */

    result = FcPatternGetInteger(ftPtr->pattern, FC_SLANT, 0, &slant);
    if (result != FcResultMatch) {
	slant = FC_SLANT_ROMAN;
    }
    slantName = NULL;
    if (slant > FC_SLANT_ROMAN) {
	if ((strcmp(familyName, "Helvetica") == 0) || 
	    (strcmp(familyName, "Courier") == 0) || 
	    (strcmp(familyName, "AvantGarde") == 0)) {
	    slantName = "Oblique";
	} else {
	    slantName = "Italic";
	}
    }

    /*
     * The string "Roman" needs to be added to some fonts that are not bold
     * and not italic.
     */

    if ((slantName == NULL) && (weightName == NULL)) {
	if ((strcmp(familyName, "Times") == 0) || 
	    (strcmp(familyName, "NewCenturySchlbk") == 0) || 
	    (strcmp(familyName, "Palatino") == 0)) {
	    Tcl_DStringAppend(dsPtr, "-Roman", -1);
	}
    } else {
	Tcl_DStringAppend(dsPtr, "-", -1);
	if (weightName != NULL) {
	    Tcl_DStringAppend(dsPtr, weightName, -1);
	}
	if (slantName != NULL) {
	    Tcl_DStringAppend(dsPtr, slantName, -1);
	}
    }
    return (int)size;
}

/*
 *---------------------------------------------------------------------------
 *
 * FtUnderlineCharsProc --
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
FtUnderlineCharsProc(
    Display *display,		/* Display on which to draw. */
    Drawable drawable,		/* Window or pixmap in which to draw. */
    GC gc,			/* Graphics context for actually drawing
				 * line. */
    _Blt_Font *fontPtr,
    const char *string,		/* String containing characters to be
				 * underlined or overstruck. */
    int x, int y,		/* Coordinates at which first character of
				 * string is drawn. */
    int first,			/* Index of first byte of first character. */
    int last)			/* Index of first byte after the last
				 * character. */
{
    FtFont *ftPtr = fontPtr->clientData;
    int firstX, lastX;

    Blt_MeasureChars(fontPtr, string, first, -1, 0, &firstX);
    Blt_MeasureChars(fontPtr, string, last, -1, 0, &lastX);

    x += firstX;
    y += ftPtr->underlinePos;
    XFillRectangle(display, drawable, gc, x, y, (unsigned int)(lastX - firstX),
	    (unsigned int)ftPtr->underlineHeight);
}

static int
FtCanRotateFontProc(_Blt_Font *fontPtr, float angle) 
{
    FcPattern *pattern;
    FcResult result;
    FtFont *ftPtr = fontPtr->clientData;
    int boolean;
    long angle10;

    angle10 = (long)((double)angle * 10.0);
    if (Blt_FindHashEntry(&ftPtr->fontTable, (char *)angle10) != NULL) {
	return TRUE;		/* Rotated font already exists. */
    }

    /* 
     * I don't know if this is correct.  Some PCF fonts don't rotate properly.
     * The chararcter positions are rotated but the glyph itself is drawn with
     * no rotation.  The standard Adobe Helvetica font is a good example of
     * this.  So I need to bail on those fonts.  I check if antialias=True in
     * the Xft font pattern to determine if the font will rotate properly.
     */
    result = FcPatternGetBool(ftPtr->pattern, FC_ANTIALIAS, 0, &boolean);
    if ((result == FcResultMatch) && (!boolean)) {
	return FALSE;
    }

    {
	XftMatrix matrix;
	double cosTheta, sinTheta, theta;

	theta = ((double)angle / 180.0) * M_PI;
	sinTheta = sin(theta);
	cosTheta = cos(theta);
	
	XftMatrixInit(&matrix);
	XftMatrixRotate(&matrix, cosTheta, sinTheta);
	pattern = FcPatternDuplicate(ftPtr->pattern);
	FcPatternAddMatrix(pattern, FC_MATRIX, &matrix);
    }

    {
	FcResult result;
	FcPattern *match;

	/* 
	 * XftFontMatch only sets *result* on complete match failures.  So
	 * initialize it here for a successful match. We'll accept partial
	 * matches. 
	 */
	result = FcResultMatch; 
	match = XftFontMatch(ftPtr->display, ftPtr->screenNum, pattern,&result);
	if ((match != NULL) && (result == FcResultMatch)) {
	    XftFont *xftPtr;
	
	    xftPtr = XftFontOpenPattern(ftPtr->display, match);
	    /* Add the new rotated font to the hash table. */
	    if (xftPtr != NULL) {
		Blt_HashEntry *hPtr;
		int isNew;
		
		hPtr = Blt_CreateHashEntry(&ftPtr->fontTable, (char *)angle10, 
			&isNew);
		assert(isNew);
		Blt_SetHashValue(hPtr, xftPtr);
		FcPatternDestroy(pattern);
		return TRUE;
	    }
	}
	FcPatternDestroy(pattern);
    }
    return FALSE;
}

static void
FtDrawCharsProc(
    Display *display,		/* Display on which to draw. */
    Drawable drawable,		/* Window or pixmap in which to draw. */
    GC gc,			/* Graphics context for drawing characters. */
    _Blt_Font *fontPtr,		/* Font in which characters will be drawn;
				 * must be the same as font used in GC. */
    int depth,
    float angle,
    const char *source,		/* UTF-8 string to be displayed.  Need not be
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
    XftFont *xftPtr;
    FtFont *ftPtr = fontPtr->clientData;
    Blt_HashEntry *hPtr;
    long angle10;

    angle10 = (long)(angle * 10.0);
    hPtr = Blt_FindHashEntry(&ftPtr->fontTable, (char *)angle10);
    if (hPtr == NULL) {
	fprintf(stderr, "can't find font %s rotated at %g degrees\n", 
		ftPtr->name, angle);
	return;			/* Can't find instance at requested angle. */
    }
    xftPtr = Blt_GetHashValue(hPtr);
    if ((ftPtr->draw == 0) || (ftPtr->drawDepth != depth)) {
	XftDraw *draw;

	if (depth == 1) {
	    draw = XftDrawCreateBitmap(display, drawable);
	} else {
	    draw = XftDrawCreate(display, drawable, ftPtr->visual, 
				 ftPtr->colormap);
	}
	if (ftPtr->draw != 0) {
	    XftDrawDestroy(ftPtr->draw);
	}
	ftPtr->drawDepth = depth;
	ftPtr->draw = draw;
	ftPtr->drawable = drawable;
    } else {
	Tk_ErrorHandler handler;
#if 0
	printf("Switch to drawable 0x%x\n", drawable);
#endif
        handler = Tk_CreateErrorHandler(display, -1, -1, -1, 
		(Tk_ErrorProc *)NULL, (ClientData) NULL);
	XftDrawChange(ftPtr->draw, drawable);
	ftPtr->drawable = drawable;
	Tk_DeleteErrorHandler(handler);
    }
    {
	XGCValues values;

	XGetGCValues(display, gc, GCForeground, &values);
	if (values.foreground != ftPtr->color.pixel) {
	    XColor xc;
	    
	    xc.pixel = values.foreground;
	    XQueryColor(display, ftPtr->colormap, &xc);
	    ftPtr->color.color.red = xc.red;
	    ftPtr->color.color.green = xc.green;
	    ftPtr->color.color.blue = xc.blue;
	    ftPtr->color.color.alpha = 0xffff; /* Assume opaque. */
	    ftPtr->color.pixel = values.foreground;
	}
    }

    {
#define NUM_SPEC    1024
	XftGlyphFontSpec *specPtr;
	XftGlyphFontSpec specs[NUM_SPEC];
	int nSpecs;
	const int maxCoord = 0x7FFF; /* Xft coordinates are 16 bit values */

	nSpecs = 0;
	specPtr = specs;
	while ((nBytes > 0) && (x <= maxCoord) && (y <= maxCoord)) {
	    XftChar32 c;
	    int charLen;
	    XGlyphInfo metrics;
	    
	    charLen = XftUtf8ToUcs4((XftChar8 *)source, &c, nBytes);
	    if (charLen <= 0) {
		/* This should not happen, but it can. */
		return;
	    }
	    source += charLen;
	    nBytes -= charLen;
	    
	    specPtr = specs + nSpecs;
	    specPtr->font = xftPtr;
	    specPtr->glyph = XftCharIndex(display, xftPtr, c);
	    specPtr->x = x;
	    specPtr->y = y;
	    XftGlyphExtents(display, xftPtr, &specPtr->glyph, 1, &metrics);
	    x += metrics.xOff;
	    y += metrics.yOff;
	    nSpecs++, specPtr++;
	    if (nSpecs == NUM_SPEC) {
		XftDrawGlyphFontSpec(ftPtr->draw, &ftPtr->color, specs, 
			nSpecs);
		nSpecs = 0;
		specPtr = specs;
	    }
	}
	if (nSpecs > 0) {
	    XftDrawGlyphFontSpec(ftPtr->draw, &ftPtr->color, specs, nSpecs);
	}
    }
}

static void
FtFreeFontProc(_Blt_Font *fontPtr)
{
    FtFont *ftPtr = fontPtr->clientData;

    ftPtr->refCount--;
    if (ftPtr->refCount <= 0) {
	FtDeleteFont(ftPtr);
    }
    Blt_Free(fontPtr);
}

#endif	/* HAVE_LIBXFT */

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
    if (!alias_initialized) {
	MakeAliasTable(tkwin);
	alias_initialized++;
    }
#ifdef HAVE_LIBXFT
    if (IsXRenderAvailable(tkwin)) {
	FtFont *ftPtr;

	/* Check first if we open the specified font as an XFT font. */
	ftPtr = FtGetFontFromObj(interp, tkwin, objPtr);
	if (ftPtr != NULL) {
	    fontPtr->classPtr = &ftFontClass;
	    fontPtr->clientData = ftPtr;
#if DEBUG_FONT_SELECTION2
	    fprintf(stderr, "SUCCESS: Found XFT font \"%s\"\n", 
		    Tcl_GetString(objPtr));
#endif
	    return fontPtr;	/* Found Xft font.  */
	}
	/* Otherwise fall thru and try to open font as a normal Tk font. */
    }
#endif	/* HAVE_LIBXFT */
    fontPtr->clientData = TkGetFontFromObj(interp, tkwin, objPtr);
    if (fontPtr->clientData == NULL) {
	Blt_Free(fontPtr);
#if DEBUG_FONT_SELECTION
    fprintf(stderr, "FAILED to find either Xft or Tk font \"%s\"\n", Tcl_GetString(objPtr));
#endif
	return NULL;		/* Failed to find either Xft or Tk fonts. */
    }
#if DEBUG_FONT_SELECTION
    fprintf(stderr, "SUCCESS: Found Tk font \"%s\"\n", Tcl_GetString(objPtr));
#endif
    fontPtr->classPtr = &tkFontClass;
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
