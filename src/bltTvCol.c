
/*
 * bltTvCol.c --
 *
 * This module implements an hierarchy widget for the BLT toolkit.
 *
 *	Copyright 1998-2004 George A Howlett.
 *
 *	Permission is hereby granted, free of charge, to any person obtaining a
 *	copy of this software and associated documentation files (the
 *	"Software"), to deal in the Software without restriction, including
 *	without limitation the rights to use, copy, modify, merge, publish,
 *	distribute, sublicense, and/or sell copies of the Software, and to
 *	permit persons to whom the Software is furnished to do so, subject to
 *	the following conditions:
 *
 *	The above copyright notice and this permission notice shall be included
 *	in all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 *	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *	LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *	OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * TODO:
 *
 * BUGS:
 *   1.  "open" operation should change scroll offset so that as many
 *	 new entries (up to half a screen) can be seen.
 *   2.  "open" needs to adjust the scrolloffset so that the same entry
 *	 is seen at the same place.
 */
#include "bltInt.h"

#ifndef NO_TREEVIEW
#include "bltOp.h"
#include "bltTreeView.h"
#include <X11/Xutil.h>

#define RULE_AREA		(8)

typedef int (TreeViewCmdProc)(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv);

static const char *sortTypeStrings[] = {
    "dictionary", "ascii", "integer", "real", "command", "none", NULL
};

enum SortTypeValues { 
    SORT_DICTIONARY, SORT_ASCII, SORT_INTEGER, 
    SORT_REAL, SORT_COMMAND, SORT_NONE
};

#define DEF_SORT_COLUMN		(char *)NULL
#define DEF_SORT_COMMAND	(char *)NULL
#define DEF_SORT_DECREASING	"no"
#define DEF_SORT_TYPE		"dictionary"

#ifdef WIN32
#define DEF_COLUMN_ACTIVE_TITLE_BG	RGB_GREY85
#else
#define DEF_COLUMN_ACTIVE_TITLE_BG	RGB_GREY90
#endif
#define DEF_COLUMN_ACTIVE_TITLE_FG	STD_ACTIVE_FOREGROUND
#define DEF_COLUMN_ARROWWIDTH		"0"
#define DEF_COLUMN_BACKGROUND		(char *)NULL
#define DEF_COLUMN_BIND_TAGS		"all"
#define DEF_COLUMN_BORDERWIDTH		STD_BORDERWIDTH
#define DEF_COLUMN_COLOR		RGB_BLACK
#define DEF_COLUMN_EDIT			"yes"
#define DEF_COLUMN_FONT			STD_FONT
#define DEF_COLUMN_COMMAND		(char *)NULL
#define DEF_COLUMN_FORMATCOMMAND	(char *)NULL
#define DEF_COLUMN_HIDE			"no"
#define DEF_COLUMN_SHOW			"yes"
#define DEF_COLUMN_JUSTIFY		"center"
#define DEF_COLUMN_MAX			"0"
#define DEF_COLUMN_MIN			"0"
#define DEF_COLUMN_PAD			"2"
#define DEF_COLUMN_RELIEF		"flat"
#define DEF_COLUMN_STATE		"normal"
#define DEF_COLUMN_STYLE		"text"
#define DEF_COLUMN_TITLE_BACKGROUND	STD_NORMAL_BACKGROUND
#define DEF_COLUMN_TITLE_BORDERWIDTH	STD_BORDERWIDTH
#define DEF_COLUMN_TITLE_FONT		STD_FONT_SMALL
#define DEF_COLUMN_TITLE_FOREGROUND	STD_NORMAL_FOREGROUND
#define DEF_COLUMN_TITLE_RELIEF		"raised"
#define DEF_COLUMN_WEIGHT		"1.0"
#define DEF_COLUMN_WIDTH		"0"
#define DEF_COLUMN_RULE_DASHES		"dot"

static Blt_OptionParseProc ObjToEnum;
static Blt_OptionPrintProc EnumToObj;
static Blt_CustomOption typeOption =
{
    ObjToEnum, EnumToObj, NULL, (ClientData)sortTypeStrings
};

static Blt_OptionParseProc ObjToColumn;
static Blt_OptionPrintProc ColumnToObj;
static Blt_CustomOption columnOption =
{
    ObjToColumn, ColumnToObj, NULL, (ClientData)0
};

static Blt_OptionParseProc ObjToData;
static Blt_OptionPrintProc DataToObj;
Blt_CustomOption bltTreeViewDataOption =
{
    ObjToData, DataToObj, NULL, (ClientData)0,
};

static Blt_OptionParseProc ObjToStyle;
static Blt_OptionPrintProc StyleToObj;
static Blt_OptionFreeProc FreeStyle;
static Blt_CustomOption styleOption =
{
    /* Contains a pointer to the widget that's currently being
     * configured.  This is used in the custom configuration parse
     * routine for icons.  */
    ObjToStyle, StyleToObj, FreeStyle, NULL,
};

BLT_EXTERN Blt_CustomOption bltTreeViewUidOption;
BLT_EXTERN Blt_CustomOption bltTreeViewIconOption;

static Blt_ConfigSpec columnSpecs[] =
{
    {BLT_CONFIG_BACKGROUND, "-activetitlebackground", "activeTitleBackground", 
	"Background", DEF_COLUMN_ACTIVE_TITLE_BG, 
	Blt_Offset(TreeViewColumn, activeTitleBg), 0},
    {BLT_CONFIG_COLOR, "-activetitleforeground", "activeTitleForeground", 
	"Foreground", DEF_COLUMN_ACTIVE_TITLE_FG, 
	Blt_Offset(TreeViewColumn, activeTitleFgColor), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-arrowwidth", "arrowWidth","ArrowWidth",
	DEF_COLUMN_ARROWWIDTH, Blt_Offset(TreeViewColumn, reqArrowWidth), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	DEF_COLUMN_BACKGROUND, Blt_Offset(TreeViewColumn, bg), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_CUSTOM, "-bindtags", "bindTags", "BindTags",
	DEF_COLUMN_BIND_TAGS, Blt_Offset(TreeViewColumn, tagsUid),
	BLT_CONFIG_NULL_OK, &bltTreeViewUidOption},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_COLUMN_BORDERWIDTH, Blt_Offset(TreeViewColumn, borderWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_STRING, "-command", "command", "Command",
	DEF_COLUMN_COMMAND, Blt_Offset(TreeViewColumn, titleCmd),
	BLT_CONFIG_DONT_SET_DEFAULT | BLT_CONFIG_NULL_OK}, 
    {BLT_CONFIG_BITMASK_INVERT, "-edit", "edit", "Edit", DEF_COLUMN_STATE, 
	Blt_Offset(TreeViewColumn, flags), BLT_CONFIG_DONT_SET_DEFAULT, 
	(Blt_CustomOption *)COLUMN_READONLY},
    {BLT_CONFIG_OBJ, "-formatcommand", "formatCommand", "FormatCommand",
	(char *)NULL, Blt_Offset(TreeViewColumn, fmtCmdPtr), 
	BLT_CONFIG_NULL_OK}, 
    {BLT_CONFIG_BITMASK, "-hide", "hide", "Hide", DEF_COLUMN_HIDE, 
	Blt_Offset(TreeViewColumn, flags), BLT_CONFIG_DONT_SET_DEFAULT, 
	(Blt_CustomOption *)COLUMN_HIDDEN},
    {BLT_CONFIG_CUSTOM, "-icon", "icon", "icon", (char *)NULL, 
	Blt_Offset(TreeViewColumn, titleIcon),
        BLT_CONFIG_NULL_OK | BLT_CONFIG_DONT_SET_DEFAULT, 
	&bltTreeViewIconOption},
    {BLT_CONFIG_JUSTIFY, "-justify", "justify", "Justify", DEF_COLUMN_JUSTIFY, 
	Blt_Offset(TreeViewColumn, justify), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-max", "max", "Max", DEF_COLUMN_MAX, 
	Blt_Offset(TreeViewColumn, reqMax), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-min", "min", "Min", DEF_COLUMN_MIN, 
	Blt_Offset(TreeViewColumn, reqMin), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PAD, "-pad", "pad", "Pad", DEF_COLUMN_PAD, 
	Blt_Offset(TreeViewColumn, pad), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_RELIEF, "-relief", "relief", "Relief", DEF_COLUMN_RELIEF, 
	Blt_Offset(TreeViewColumn, relief), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_DASHES, "-ruledashes", "ruleDashes", "RuleDashes",
	DEF_COLUMN_RULE_DASHES, Blt_Offset(TreeViewColumn, ruleDashes),
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_BITMASK_INVERT, "-show", "show", "Show", DEF_COLUMN_SHOW, 
	Blt_Offset(TreeViewColumn, flags), BLT_CONFIG_DONT_SET_DEFAULT,
	(Blt_CustomOption *)COLUMN_HIDDEN},
    {BLT_CONFIG_OBJ, "-sortcommand", "sortCommand", "SortCommand",
	DEF_SORT_COMMAND, Blt_Offset(TreeViewColumn, sortCmdPtr), 
	BLT_CONFIG_NULL_OK}, 
    {BLT_CONFIG_STATE, "-state", "state", "State", DEF_COLUMN_STATE, 
	Blt_Offset(TreeViewColumn, state), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-style", "style", "Style", DEF_COLUMN_STYLE, 
	Blt_Offset(TreeViewColumn, stylePtr), BLT_CONFIG_NULL_OK, &styleOption},
    {BLT_CONFIG_STRING, "-text", "text", "Text",
	(char *)NULL, Blt_Offset(TreeViewColumn, text), 0},
    {BLT_CONFIG_STRING, "-title", "title", "Title", (char *)NULL, 
	Blt_Offset(TreeViewColumn, text), 0},
    {BLT_CONFIG_BACKGROUND, "-titlebackground", "titleBackground", 
	"TitleBackground", DEF_COLUMN_TITLE_BACKGROUND, 
	Blt_Offset(TreeViewColumn, titleBg), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-titleborderwidth", "titleBorderWidth", 
	"TitleBorderWidth", DEF_COLUMN_TITLE_BORDERWIDTH, 
	Blt_Offset(TreeViewColumn, titleBW), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_FONT, "-titlefont", "titleFont", "Font",
	DEF_COLUMN_TITLE_FONT, Blt_Offset(TreeViewColumn, titleFont), 0},
    {BLT_CONFIG_COLOR, "-titleforeground", "titleForeground", "TitleForeground",
	DEF_COLUMN_TITLE_FOREGROUND, 
	Blt_Offset(TreeViewColumn, titleFgColor), 0},
    {BLT_CONFIG_JUSTIFY, "-titlejustify", "titleJustify", "TitleJustify", 
        DEF_COLUMN_JUSTIFY, Blt_Offset(TreeViewColumn, titleJustify), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_RELIEF, "-titlerelief", "titleRelief", "TitleRelief",
	DEF_COLUMN_TITLE_RELIEF, Blt_Offset(TreeViewColumn, titleRelief), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_DOUBLE, "-weight", (char *)NULL, (char *)NULL,
	DEF_COLUMN_WEIGHT, Blt_Offset(TreeViewColumn, weight), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-width", "width", "Width",
	DEF_COLUMN_WIDTH, Blt_Offset(TreeViewColumn, reqWidth), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

static Blt_ConfigSpec sortSpecs[] =
{
    {BLT_CONFIG_OBJ, "-command", "command", "Command",
	DEF_SORT_COMMAND, Blt_Offset(TreeView, sortCmdPtr),
	BLT_CONFIG_DONT_SET_DEFAULT | BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-column", "column", "Column",
	DEF_SORT_COLUMN, Blt_Offset(TreeView, sortColumnPtr),
        BLT_CONFIG_NULL_OK | BLT_CONFIG_DONT_SET_DEFAULT, &columnOption},
    {BLT_CONFIG_BOOLEAN, "-decreasing", "decreasing", "Decreasing",
	DEF_SORT_DECREASING, Blt_Offset(TreeView, sortDecreasing),
        BLT_CONFIG_DONT_SET_DEFAULT}, 
    {BLT_CONFIG_CUSTOM, "-mode", "mode", "Mode", DEF_SORT_TYPE, 
	Blt_Offset(TreeView, sortType), 0, &typeOption},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

static Blt_TreeApplyProc SortApplyProc;
static Blt_TreeCompareNodesProc CompareNodes;

/*
 *---------------------------------------------------------------------------
 *
 * ObjToEnum --
 *
 *	Converts the string into its enumerated type.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToEnum(
    ClientData clientData,	/* Vectors of valid strings. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,
    char *widgRec,		/* Widget record. */
    int offset,			/* Offset of field in record */
    int flags)			
{
    int *enumPtr = (int *)(widgRec + offset);
    char c;
    char **p;
    int i;
    int count;
    char *string;

    string = Tcl_GetString(objPtr);
    c = string[0];
    count = 0;
    for (p = (char **)clientData; *p != NULL; p++) {
	if ((c == p[0][0]) && (strcmp(string, *p) == 0)) {
	    *enumPtr = count;
	    return TCL_OK;
	}
	count++;
    }
    *enumPtr = -1;

    Tcl_AppendResult(interp, "bad value \"", string, "\": should be ", 
	(char *)NULL);
    p = (char **)clientData; 
    if (count > 0) {
	Tcl_AppendResult(interp, p[0], (char *)NULL);
    }
    for (i = 1; i < (count - 1); i++) {
	Tcl_AppendResult(interp, " ", p[i], ", ", (char *)NULL);
    }
    if (count > 1) {
	Tcl_AppendResult(interp, " or ", p[count - 1], ".", (char *)NULL);
    }
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * EnumToObj --
 *
 *	Returns the string associated with the enumerated type.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
EnumToObj(
    ClientData clientData,	/* List of strings. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    int value = *(int *)(widgRec + offset);
    char **strings = (char **)clientData;
    char **p;
    int count;

    count = 0;
    for (p = strings; *p != NULL; p++) {
	if (value == count) {
	    return Tcl_NewStringObj(*p, -1);
	}
	count++;
    }
    return Tcl_NewStringObj("unknown value", -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToColumn --
 *
 *	Convert the string reprsenting a column, to its numeric
 *	form.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToColumn(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* New legend position string */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    TreeView *viewPtr = (TreeView *)widgRec;
    TreeViewColumn **columnPtrPtr = (TreeViewColumn **)(widgRec + offset);
    char *string;

    string = Tcl_GetString(objPtr);
    if (string[0] == '\0') {
	*columnPtrPtr = &viewPtr->treeColumn;
    } else {
	if (Blt_TreeView_GetColumn(interp, viewPtr, objPtr, columnPtrPtr) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnToString --
 *
 * Results:
 *	The string representation of the column is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ColumnToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    TreeViewColumn *columnPtr = *(TreeViewColumn **)(widgRec + offset);

    if (columnPtr == NULL) {
	return Tcl_NewStringObj("", -1);
    } else {
	return Tcl_NewStringObj(columnPtr->key, -1);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToData --
 *
 *	Convert the string reprsenting a scroll mode, to its numeric
 *	form.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToData(
    ClientData clientData,	/* Node of entry. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing new data. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Tcl_Obj **objv;
    TreeViewEntry *entryPtr = (TreeViewEntry *)widgRec;
    char *string;
    int objc;
    int i;

    string = Tcl_GetString(objPtr);
    if (*string == '\0') {
	return TCL_OK;
    } 
    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 0) {
	return TCL_OK;
    }
    if (objc & 0x1) {
	Tcl_AppendResult(interp, "data \"", string, 
		 "\" must be in even name-value pairs", (char *)NULL);
	return TCL_ERROR;
    }
    for (i = 0; i < objc; i += 2) {
	TreeViewColumn *columnPtr;
	TreeView *viewPtr = entryPtr->viewPtr;

	if (Blt_TreeView_GetColumn(interp, viewPtr, objv[i], &columnPtr) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Blt_Tree_SetValueByKey(viewPtr->interp, viewPtr->tree, entryPtr->node, 
		columnPtr->key, objv[i + 1]) != TCL_OK) {
	    return TCL_ERROR;
	}
	Blt_TreeView_AddValue(entryPtr, columnPtr);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * DataToObj --
 *
 * Results:
 *	The string representation of the data is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
DataToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Tcl_Obj *listObjPtr, *objPtr;
    TreeViewEntry *entryPtr = (TreeViewEntry *)widgRec;
    TreeViewValue *valuePtr;

    /* Add the key-value pairs to a new Tcl_Obj */
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (valuePtr = entryPtr->values; valuePtr != NULL; 
	valuePtr = valuePtr->nextPtr) {
	objPtr = Tcl_NewStringObj(valuePtr->columnPtr->key, -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	if (Blt_TreeView_GetData(entryPtr, valuePtr->columnPtr->key, &objPtr)
	    != TCL_OK) {
	    objPtr = Tcl_NewStringObj("", -1);
	    Tcl_IncrRefCount(objPtr);
	} 
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    return listObjPtr;
}

int
Blt_TreeView_GetColumn(Tcl_Interp *interp, TreeView *viewPtr, Tcl_Obj *objPtr, 
		 TreeViewColumn **columnPtrPtr)
{
    char *string;

    string = Tcl_GetString(objPtr);
    if (strcmp(string, "treeView") == 0) {
	*columnPtrPtr = &viewPtr->treeColumn;
    } else {
	Blt_HashEntry *hPtr;
    
	hPtr = Blt_FindHashEntry(&viewPtr->columnTable, 
		Blt_Tree_GetKey(viewPtr->tree, string));
	if (hPtr == NULL) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "can't find column \"", string, 
			"\" in \"", Tk_PathName(viewPtr->tkwin), "\"", 
			(char *)NULL);
	    }
	    return TCL_ERROR;
	} 
	*columnPtrPtr = Blt_GetHashValue(hPtr);
    }
    return TCL_OK;
}


/*ARGSUSED*/
static void
FreeStyle(ClientData clientData, Display *display, char *widgRec, int offset)
{
    TreeView *viewPtr = clientData;
    TreeViewStyle **stylePtrPtr = (TreeViewStyle **)(widgRec + offset);

    if (*stylePtrPtr != NULL) {
	Blt_TreeView_FreeStyle(viewPtr, *stylePtrPtr);
	*stylePtrPtr = NULL;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToStyle --
 *
 *	Convert the name of an icon into a treeview style.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left in
 *	interpreter's result field.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToStyle(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    TreeView *viewPtr = clientData;
    TreeViewStyle **stylePtrPtr = (TreeViewStyle **)(widgRec + offset);
    TreeViewStyle *stylePtr;

    if (Blt_TreeView_GetStyle(interp, viewPtr, Tcl_GetString(objPtr), 
	     &stylePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    stylePtr->flags |= STYLE_DIRTY;
    viewPtr->flags |= (LAYOUT_PENDING | DIRTY);
    if (*stylePtrPtr != NULL) {
	Blt_TreeView_FreeStyle(viewPtr, *stylePtrPtr);
    }
    *stylePtrPtr = stylePtr;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * IconToObj --
 *
 *	Converts the icon into its string representation (its name).
 *
 * Results:
 *	The name of the icon is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
StyleToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    TreeViewStyle *stylePtr = *(TreeViewStyle **)(widgRec + offset);

    if (stylePtr == NULL) {
	return Tcl_NewStringObj("", -1);
    } else {
	return Tcl_NewStringObj(stylePtr->name, -1);
    }
}


void
Blt_TreeView_ConfigureColumn(TreeView *viewPtr, TreeViewColumn *columnPtr)
{
    Drawable drawable;
    GC newGC;
    Blt_Background bg;
    XGCValues gcValues;
    int ruleDrawn;
    unsigned long gcMask;
    int iconWidth, iconHeight;
    unsigned int textWidth, textHeight;

    gcMask = GCForeground | GCFont;
    gcValues.font = Blt_FontId(columnPtr->titleFont);

    /* Normal title text */
    gcValues.foreground = columnPtr->titleFgColor->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (columnPtr->titleGC != NULL) {
	Tk_FreeGC(viewPtr->display, columnPtr->titleGC);
    }
    columnPtr->titleGC = newGC;

    /* Active title text */
    gcValues.foreground = columnPtr->activeTitleFgColor->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (columnPtr->activeTitleGC != NULL) {
	Tk_FreeGC(viewPtr->display, columnPtr->activeTitleGC);
    }
    columnPtr->activeTitleGC = newGC;

    columnPtr->titleWidth = 2 * columnPtr->titleBW;
    iconWidth = iconHeight = 0;
    if (columnPtr->titleIcon != NULL) {
	iconWidth = TreeView_IconWidth(columnPtr->titleIcon);
	iconHeight = TreeView_IconHeight(columnPtr->titleIcon);
	columnPtr->titleWidth += iconWidth;
    }
    textWidth = textHeight = 0;
    if (columnPtr->text != NULL) {
	TextStyle ts;

	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, columnPtr->titleFont);
	Blt_Ts_GetExtents(&ts, columnPtr->text,  &textWidth, &textHeight);
	columnPtr->textWidth = textWidth;
	columnPtr->textHeight = textHeight;
	columnPtr->titleWidth += textWidth;
    }
    if (columnPtr->reqArrowWidth > 0) {
	columnPtr->arrowWidth = columnPtr->reqArrowWidth;
    } else {
	columnPtr->arrowWidth = Blt_TextWidth(columnPtr->titleFont, "0", 1) + 4;
    }
    if ((iconWidth > 0) && (textWidth > 0)) {
	columnPtr->titleWidth += 8;
    }
    columnPtr->titleWidth += columnPtr->arrowWidth + 2;
    columnPtr->titleHeight = MAX(iconHeight, textHeight);
    gcMask = (GCFunction | GCLineWidth | GCLineStyle | GCForeground);

    /* 
     * If the rule is active, turn it off (i.e. draw again to erase
     * it) before changing the GC.  If the color changes, we won't be
     * able to erase the old line, since it will no longer be
     * correctly XOR-ed with the background.
     */
    drawable = Tk_WindowId(viewPtr->tkwin);
    ruleDrawn = ((viewPtr->flags & TV_RULE_ACTIVE) &&
		 (viewPtr->activeTitleColumnPtr == columnPtr) && 
		 (drawable != None));
    if (ruleDrawn) {
	Blt_TreeView_DrawRule(viewPtr, columnPtr, drawable);
    }
    /* XOR-ed rule column divider */ 
    gcValues.line_width = LineWidth(columnPtr->ruleLineWidth);
    gcValues.foreground = 
	Blt_TreeView_GetStyleFg(viewPtr, columnPtr->stylePtr)->pixel;
    if (LineIsDashed(columnPtr->ruleDashes)) {
	gcValues.line_style = LineOnOffDash;
    } else {
	gcValues.line_style = LineSolid;
    }
    gcValues.function = GXxor;

    bg = CHOOSE(viewPtr->bg, columnPtr->bg);
    gcValues.foreground ^= Blt_BackgroundBorderColor(bg)->pixel; 
    newGC = Blt_GetPrivateGC(viewPtr->tkwin, gcMask, &gcValues);
    if (columnPtr->ruleGC != NULL) {
	Blt_FreePrivateGC(viewPtr->display, columnPtr->ruleGC);
    }
    if (LineIsDashed(columnPtr->ruleDashes)) {
	Blt_SetDashes(viewPtr->display, newGC, &columnPtr->ruleDashes);
    }
    columnPtr->ruleGC = newGC;
    if (ruleDrawn) {
	Blt_TreeView_DrawRule(viewPtr, columnPtr, drawable);
    }
    columnPtr->flags |= COLUMN_DIRTY;
    viewPtr->flags |= UPDATE;
}

static void
DestroyColumn(TreeView *viewPtr, TreeViewColumn *columnPtr)
{
    Blt_HashEntry *hPtr;

    bltTreeViewUidOption.clientData = viewPtr;
    bltTreeViewIconOption.clientData = viewPtr;
    styleOption.clientData = viewPtr;
    Blt_FreeOptions(columnSpecs, (char *)columnPtr, viewPtr->display, 0);
    if (columnPtr->titleGC != NULL) {
	Tk_FreeGC(viewPtr->display, columnPtr->titleGC);
    }
    if (columnPtr->ruleGC != NULL) {
	Blt_FreePrivateGC(viewPtr->display, columnPtr->ruleGC);
    }
    hPtr = Blt_FindHashEntry(&viewPtr->columnTable, columnPtr->key);
    if (hPtr != NULL) {
	Blt_DeleteHashEntry(&viewPtr->columnTable, hPtr);
    }
    if (columnPtr->link != NULL) {
	Blt_Chain_DeleteLink(viewPtr->columns, columnPtr->link);
    }
    if (columnPtr == &viewPtr->treeColumn) {
	columnPtr->link = NULL;
    } else {
	Blt_Free(columnPtr);
    }
}

void
Blt_TreeView_DestroyColumns(TreeView *viewPtr)
{
    if (viewPtr->columns != NULL) {
	Blt_ChainLink link;
	TreeViewColumn *columnPtr;
	
	for (link = Blt_Chain_FirstLink(viewPtr->columns); link != NULL;
	     link = Blt_Chain_NextLink(link)) {
	    columnPtr = Blt_Chain_GetValue(link);
	    columnPtr->link = NULL;
	    DestroyColumn(viewPtr, columnPtr);
	}
	Blt_Chain_Destroy(viewPtr->columns);
	viewPtr->columns = NULL;
    }
    Blt_DeleteHashTable(&viewPtr->columnTable);
}

int
Blt_TreeView_CreateColumn(TreeView *viewPtr, TreeViewColumn *columnPtr, 
			  const char *name, const char *defTitle)
{
    Blt_HashEntry *hPtr;
    int isNew;

    columnPtr->key = Blt_Tree_GetKeyFromInterp(viewPtr->interp, name);
    columnPtr->text = Blt_AssertStrdup(defTitle);
    columnPtr->justify = TK_JUSTIFY_CENTER;
    columnPtr->relief = TK_RELIEF_FLAT;
    columnPtr->borderWidth = 1;
    columnPtr->pad.side1 = columnPtr->pad.side2 = 2;
    columnPtr->state = STATE_NORMAL;
    columnPtr->weight = 1.0;
    columnPtr->ruleLineWidth = 1;
    columnPtr->titleBW = 2;
    columnPtr->titleRelief = TK_RELIEF_RAISED;
    columnPtr->titleIcon = NULL;
    hPtr = Blt_CreateHashEntry(&viewPtr->columnTable, columnPtr->key, &isNew);
    Blt_SetHashValue(hPtr, columnPtr);

    bltTreeViewUidOption.clientData = viewPtr;
    bltTreeViewIconOption.clientData = viewPtr;
    styleOption.clientData = viewPtr;
    if (Blt_ConfigureComponentFromObj(viewPtr->interp, viewPtr->tkwin, name, 
	"Column", columnSpecs, 0, (Tcl_Obj **)NULL, (char *)columnPtr, 0) 
	!= TCL_OK) {
	DestroyColumn(viewPtr, columnPtr);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static TreeViewColumn *
CreateColumn(TreeView *viewPtr, Tcl_Obj *nameObjPtr, int objc, 
	     Tcl_Obj *const *objv)
{
    TreeViewColumn *columnPtr;

    columnPtr = Blt_AssertCalloc(1, sizeof(TreeViewColumn));
    if (Blt_TreeView_CreateColumn(viewPtr, columnPtr, Tcl_GetString(nameObjPtr),
	Tcl_GetString(nameObjPtr)) != TCL_OK) {
	return NULL;
    }
    bltTreeViewUidOption.clientData = viewPtr;
    bltTreeViewIconOption.clientData = viewPtr;
    styleOption.clientData = viewPtr;
    if (Blt_ConfigureComponentFromObj(viewPtr->interp, viewPtr->tkwin, 
	columnPtr->key, "Column", columnSpecs, objc, objv, (char *)columnPtr, 
	BLT_CONFIG_OBJV_ONLY) != TCL_OK) {
	DestroyColumn(viewPtr, columnPtr);
	return NULL;
    }
    Blt_TreeView_ConfigureColumn(viewPtr, columnPtr);
    return columnPtr;
}

TreeViewColumn *
Blt_TreeView_NearestColumn(TreeView *viewPtr, int x, int y, 
			   ClientData *contextPtr)
{
    if (viewPtr->nVisible > 0) {
	Blt_ChainLink link;
	TreeViewColumn *columnPtr;
	int right;

	/*
	 * Determine if the pointer is over the rightmost portion of the
	 * column.  This activates the rule.
	 */
	x = WORLDX(viewPtr, x);
	for(link = Blt_Chain_FirstLink(viewPtr->columns); link != NULL;
	    link = Blt_Chain_NextLink(link)) {
	    columnPtr = Blt_Chain_GetValue(link);
	    right = columnPtr->worldX + columnPtr->width;
	    if ((x >= columnPtr->worldX) && (x <= right)) {
		if (contextPtr != NULL) {
		    *contextPtr = NULL;
		    if ((viewPtr->flags & TV_SHOW_COLUMN_TITLES) && 
			(y >= viewPtr->inset) &&
			(y < (viewPtr->titleHeight + viewPtr->inset))) {
			*contextPtr = (x >= (right - RULE_AREA)) 
			    ? ITEM_COLUMN_RULE : ITEM_COLUMN_TITLE;
		    } 
		}
		return columnPtr;
	    }
	}
    }
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnActivateOp --
 *
 *	Selects the button to appear active.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColumnActivateOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
		 Tcl_Obj *const *objv)
{
    if (objc == 4) {
	Drawable drawable;
	TreeViewColumn *columnPtr;
	char *string;

	string = Tcl_GetString(objv[3]);
	if (string[0] == '\0') {
	    columnPtr = NULL;
	} else {
	    if (Blt_TreeView_GetColumn(interp, viewPtr, objv[3], &columnPtr) 
		!= TCL_OK) {
		return TCL_ERROR;
	    }
	    if (((viewPtr->flags & TV_SHOW_COLUMN_TITLES) == 0) || 
		(columnPtr->flags & COLUMN_HIDDEN) || 
		(columnPtr->state == STATE_DISABLED)) {
		columnPtr = NULL;
	    }
	}
	viewPtr->activeTitleColumnPtr = viewPtr->activeColumnPtr = columnPtr;
	drawable = Tk_WindowId(viewPtr->tkwin);
	if (drawable != None) {
	    Blt_TreeView_DrawHeadings(viewPtr, drawable);
	    Blt_TreeView_DrawOuterBorders(viewPtr, drawable);
	}
    }
    if (viewPtr->activeTitleColumnPtr != NULL) {
	Tcl_SetStringObj(Tcl_GetObjResult(interp), 
			 viewPtr->activeTitleColumnPtr->key, -1);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnBindOp --
 *
 *	  .t bind tag sequence command
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColumnBindOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	     Tcl_Obj *const *objv)
{
    ClientData object;
    TreeViewColumn *columnPtr;

    if (Blt_TreeView_GetColumn(NULL, viewPtr, objv[3], &columnPtr) == TCL_OK) {
	object = Blt_TreeView_ColumnTag(viewPtr, columnPtr->key);
    } else {
	object = Blt_TreeView_ColumnTag(viewPtr, Tcl_GetString(objv[3]));
    }
    return Blt_ConfigureBindingsFromObj(interp, viewPtr->bindTable, object,
	objc - 4, objv + 4);
}


/*
 *---------------------------------------------------------------------------
 *
 * ColumnCgetOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColumnCgetOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	     Tcl_Obj *const *objv)
{
    TreeViewColumn *columnPtr;

    if (Blt_TreeView_GetColumn(interp, viewPtr, objv[3], &columnPtr) != TCL_OK){
	return TCL_ERROR;
    }
    return Blt_ConfigureValueFromObj(interp, viewPtr->tkwin, columnSpecs, 
	(char *)columnPtr, objv[4], 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnConfigureOp --
 *
 * 	This procedure is called to process a list of configuration
 *	options database, in order to reconfigure the one of more
 *	entries in the widget.
 *
 *	  .h entryconfigure node node node node option value
 *
 * Results:
 *	A standard TCL result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for viewPtr; old resources get freed, if there
 *	were any.  The hypertext is redisplayed.
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnConfigureOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
		  Tcl_Obj *const *objv)
{
    int nOptions, start;
    int i;

    /* Figure out where the option value pairs begin */
    for(i = 3; i < objc; i++) {
	TreeViewColumn *columnPtr;

	if (Blt_ObjIsOption(columnSpecs, objv[i], 0)) {
	    break;
	}
	if (Blt_TreeView_GetColumn(interp, viewPtr, objv[i], &columnPtr) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    start = i;
    nOptions = objc - start;
    
    bltTreeViewUidOption.clientData = viewPtr;
    bltTreeViewIconOption.clientData = viewPtr;
    styleOption.clientData = viewPtr;
    for (i = 3; i < start; i++) {
	TreeViewColumn *columnPtr;

	if (Blt_TreeView_GetColumn(interp, viewPtr, objv[i], &columnPtr) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	if (nOptions == 0) {
	    return Blt_ConfigureInfoFromObj(interp, viewPtr->tkwin, columnSpecs, 
		(char *)columnPtr, (Tcl_Obj *)NULL, 0);
	} else if (nOptions == 1) {
	    return Blt_ConfigureInfoFromObj(interp, viewPtr->tkwin, columnSpecs, 
		(char *)columnPtr, objv[start], 0);
	}
	if (Blt_ConfigureWidgetFromObj(viewPtr->interp, viewPtr->tkwin, 
	       columnSpecs, nOptions, objv + start, (char *)columnPtr, 
		BLT_CONFIG_OBJV_ONLY) != TCL_OK) {
	    return TCL_ERROR;
	}
	Blt_TreeView_ConfigureColumn(viewPtr, columnPtr);
    }
    /*FIXME: Makes every change redo everything. */
    viewPtr->flags |= (LAYOUT_PENDING | DIRTY);
    Blt_TreeView_EventuallyRedraw(viewPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnDeleteOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColumnDeleteOp(TreeView *viewPtr, Tcl_Interp *interp, int objc,
	       Tcl_Obj *const *objv)
{
    int i;

    for(i = 3; i < objc; i++) {
	TreeViewColumn *columnPtr;
	TreeViewEntry *entryPtr;

	if (Blt_TreeView_GetColumn(interp, viewPtr, objv[i], &columnPtr) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	/* Traverse the tree deleting values associated with the column.  */
	for(entryPtr = viewPtr->rootPtr; entryPtr != NULL;
	    entryPtr = Blt_TreeView_NextEntry(entryPtr, 0)) {
	    if (entryPtr != NULL) {
		TreeViewValue *valuePtr, *lastPtr, *nextPtr;
		
		lastPtr = NULL;
		for (valuePtr = entryPtr->values; valuePtr != NULL; 
		     valuePtr = nextPtr) {
		    nextPtr = valuePtr->nextPtr;
		    if (valuePtr->columnPtr == columnPtr) {
			Blt_TreeView_DestroyValue(viewPtr, valuePtr);
			if (lastPtr == NULL) {
			    entryPtr->values = nextPtr;
			} else {
			    lastPtr->nextPtr = nextPtr;
			}
			break;
		    }
		    lastPtr = valuePtr;
		}
	    }
	}
	DestroyColumn(viewPtr, columnPtr);
    }
    /* Deleting a column may affect the height of an entry. */
    viewPtr->flags |= (LAYOUT_PENDING | DIRTY /*| RESORT */);
    Blt_TreeView_EventuallyRedraw(viewPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnInsertOp --
 *
 *	Add new columns to the tree.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColumnInsertOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	       Tcl_Obj *const *objv)
{
    Blt_ChainLink before;
    Tcl_Obj *const *options;
    int i;
    int nOptions;
    int start;
    long insertPos;

    if (Blt_GetPositionFromObj(viewPtr->interp, objv[3], &insertPos) != TCL_OK){
	return TCL_ERROR;
    }
    if ((insertPos == -1) || 
	(insertPos >= Blt_Chain_GetLength(viewPtr->columns))) {
	before = NULL;		/* Insert at end of list. */
    } else {
	before =  Blt_Chain_GetNthLink(viewPtr->columns, insertPos);
    }
    /*
     * Count the column names that follow.  Count the arguments until we spot
     * one that looks like a configuration option (i.e. starts with a minus
     * ("-")).
     */
    for (i = 4; i < objc; i++) {
	if (Blt_ObjIsOption(columnSpecs, objv[i], 0)) {
	    break;
	}
    }
    start = i;
    nOptions = objc - i;
    options = objv + start;

    for (i = 4; i < start; i++) {
	TreeViewColumn *columnPtr;
	TreeViewEntry *entryPtr;

	if (Blt_TreeView_GetColumn(NULL, viewPtr, objv[i], &columnPtr) == 
	    TCL_OK) {
	    Tcl_AppendResult(interp, "column \"", Tcl_GetString(objv[i]), 
		"\" already exists", (char *)NULL);
	    return TCL_ERROR;
	}
	columnPtr = CreateColumn(viewPtr, objv[i], nOptions, options);
	if (columnPtr == NULL) {
	    return TCL_ERROR;
	}
	if (before == NULL) {
	    columnPtr->link = Blt_Chain_Append(viewPtr->columns, columnPtr);
	} else {
	    columnPtr->link = Blt_Chain_NewLink();
	    Blt_Chain_SetValue(columnPtr->link, columnPtr);
	    Blt_Chain_LinkBefore(viewPtr->columns, columnPtr->link, before);
	}
	/* 
	 * Traverse the tree adding column entries where needed.
	 */
	for(entryPtr = viewPtr->rootPtr; entryPtr != NULL;
	    entryPtr = Blt_TreeView_NextEntry(entryPtr, 0)) {
	    Blt_TreeView_AddValue(entryPtr, columnPtr);
	}
	Blt_TreeView_TraceColumn(viewPtr, columnPtr);
    }
    Blt_TreeView_EventuallyRedraw(viewPtr);
    return TCL_OK;
}



/*
 *---------------------------------------------------------------------------
 *
 * ColumnCurrentOp --
 *
 *	Make the rule to appear active.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColumnCurrentOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
		Tcl_Obj *const *objv)
{
    ClientData context;
    TreeViewColumn *columnPtr;

    columnPtr = NULL;
    context = Blt_GetCurrentContext(viewPtr->bindTable);
    if ((context == ITEM_COLUMN_TITLE) || (context == ITEM_COLUMN_RULE)) {
	columnPtr = Blt_GetCurrentItem(viewPtr->bindTable);
    }
    if (context >= ITEM_STYLE) {
	TreeViewValue *valuePtr = context;
	
	columnPtr = valuePtr->columnPtr;
    }
    if (columnPtr != NULL) {
	Tcl_SetStringObj(Tcl_GetObjResult(interp), columnPtr->key, -1);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnInvokeOp --
 *
 * 	This procedure is called to invoke a column command.
 *
 *	  .h column invoke columnName
 *
 * Results:
 *	A standard TCL result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColumnInvokeOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	       Tcl_Obj *const *objv)
{
    TreeViewColumn *columnPtr;
    char *string;

    string = Tcl_GetString(objv[3]);
    if (string[0] == '\0') {
	return TCL_OK;
    }
    if (Blt_TreeView_GetColumn(interp, viewPtr, objv[3], &columnPtr) != TCL_OK){
	return TCL_ERROR;
    }
    if ((columnPtr->state == STATE_NORMAL) && (columnPtr->titleCmd != NULL)) {
	int result;

	Tcl_Preserve(viewPtr);
	Tcl_Preserve(columnPtr);
	result = Tcl_GlobalEval(interp, columnPtr->titleCmd);
	Tcl_Release(columnPtr);
	Tcl_Release(viewPtr);
	return result;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnMoveOp --
 *
 *	Move a column.
 *
 * .h column move field1 position
 *---------------------------------------------------------------------------
 */

/*
 *---------------------------------------------------------------------------
 *
 * ColumnNamesOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColumnNamesOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	      Tcl_Obj *const *objv)
{
    Blt_ChainLink link;
    Tcl_Obj *listObjPtr, *objPtr;
    TreeViewColumn *columnPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for(link = Blt_Chain_FirstLink(viewPtr->columns); link != NULL;
	link = Blt_Chain_NextLink(link)) {
	columnPtr = Blt_Chain_GetValue(link);
	objPtr = Tcl_NewStringObj(columnPtr->key, -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
ColumnNearestOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
		Tcl_Obj *const *objv)
{
    int x, y;			/* Screen coordinates of the test point. */
    TreeViewColumn *columnPtr;
    ClientData context;
    int checkTitle;
#ifdef notdef
    int isRoot;

    isRoot = FALSE;
    string = Tcl_GetString(objv[3]);

    if (strcmp("-root", string) == 0) {
	isRoot = TRUE;
	objv++, objc--;
    }
    if (objc != 5) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", 
		Tcl_GetString(objv[0]), " ", Tcl_GetString(objv[1]), 
		Tcl_GetString(objv[2]), " ?-root? x y\"", (char *)NULL);
	return TCL_ERROR;
			 
    }
#endif
    if (Tk_GetPixelsFromObj(interp, viewPtr->tkwin, objv[3], &x) != TCL_OK) {
	return TCL_ERROR;
    } 
    y = 0;
    checkTitle = FALSE;
    if (objc == 5) {
	if (Tk_GetPixelsFromObj(interp, viewPtr->tkwin, objv[4], &y) != TCL_OK) {
	    return TCL_ERROR;
	}
	checkTitle = TRUE;
    }
    columnPtr = Blt_TreeView_NearestColumn(viewPtr, x, y, &context);
    if ((checkTitle) && (context == NULL)) {
	columnPtr = NULL;
    }
    if (columnPtr != NULL) {
	Tcl_SetStringObj(Tcl_GetObjResult(interp), columnPtr->key, -1);
    }
    return TCL_OK;
}

static void
UpdateMark(TreeView *viewPtr, int newMark)
{
    Drawable drawable;
    TreeViewColumn *cp;
    int dx;
    int width;

    cp = viewPtr->resizeColumnPtr;
    if (cp == NULL) {
	return;
    }
    drawable = Tk_WindowId(viewPtr->tkwin);
    if (drawable == None) {
	return;
    }

    /* Erase any existing rule. */
    if (viewPtr->flags & TV_RULE_ACTIVE) { 
	Blt_TreeView_DrawRule(viewPtr, cp, drawable);
    }
    
    dx = newMark - viewPtr->ruleAnchor; 
    width = cp->width - (PADDING(cp->pad) + 2 * cp->borderWidth);
    if ((cp->reqMin > 0) && ((width + dx) < cp->reqMin)) {
	dx = cp->reqMin - width;
    }
    if ((cp->reqMax > 0) && ((width + dx) > cp->reqMax)) {
	dx = cp->reqMax - width;
    }
    if ((width + dx) < 4) {
	dx = 4 - width;
    }
    viewPtr->ruleMark = viewPtr->ruleAnchor + dx;

    /* Redraw the rule if required. */
    if (viewPtr->flags & TV_RULE_NEEDED) {
	Blt_TreeView_DrawRule(viewPtr, cp, drawable);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ResizeActivateOp --
 *
 *	Turns on/off the resize cursor.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ResizeActivateOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
		 Tcl_Obj *const *objv)
{
    TreeViewColumn *columnPtr;
    char *string;

    string = Tcl_GetString(objv[4]);
    if (string[0] == '\0') {
	if (viewPtr->cursor != None) {
	    Tk_DefineCursor(viewPtr->tkwin, viewPtr->cursor);
	} else {
	    Tk_UndefineCursor(viewPtr->tkwin);
	}
	viewPtr->resizeColumnPtr = NULL;
    } else if (Blt_TreeView_GetColumn(interp, viewPtr, objv[4], &columnPtr) 
	       == TCL_OK) {
	if (viewPtr->resizeCursor != None) {
	    Tk_DefineCursor(viewPtr->tkwin, viewPtr->resizeCursor);
	} 
	viewPtr->resizeColumnPtr = columnPtr;
    } else {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ResizeAnchorOp --
 *
 *	Set the anchor for the resize.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ResizeAnchorOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	       Tcl_Obj *const *objv)
{
    int x;

    if (Tcl_GetIntFromObj(NULL, objv[4], &x) != TCL_OK) {
	return TCL_ERROR;
    } 
    viewPtr->ruleAnchor = x;
    viewPtr->flags |= TV_RULE_NEEDED;
    UpdateMark(viewPtr, x);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ResizeMarkOp --
 *
 *	Sets the resize mark.  The distance between the mark and the anchor
 *	is the delta to change the width of the active column.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ResizeMarkOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	     Tcl_Obj *const *objv)
{
    int x;

    if (Tcl_GetIntFromObj(NULL, objv[4], &x) != TCL_OK) {
	return TCL_ERROR;
    } 
    viewPtr->flags |= TV_RULE_NEEDED;
    UpdateMark(viewPtr, x);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ResizeSetOp --
 *
 *	Returns the new width of the column including the resize delta.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ResizeSetOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	    Tcl_Obj *const *objv)
{
    viewPtr->flags &= ~TV_RULE_NEEDED;
    UpdateMark(viewPtr, viewPtr->ruleMark);
    if (viewPtr->resizeColumnPtr != NULL) {
	int width, delta;
	TreeViewColumn *columnPtr;

	columnPtr = viewPtr->resizeColumnPtr;
	delta = (viewPtr->ruleMark - viewPtr->ruleAnchor);
	width = viewPtr->resizeColumnPtr->width + delta - 
	    (PADDING(columnPtr->pad) + 2 * columnPtr->borderWidth) - 1;
	Tcl_SetIntObj(Tcl_GetObjResult(interp), width);
    }
    return TCL_OK;
}

static Blt_OpSpec resizeOps[] =
{ 
    {"activate", 2, ResizeActivateOp, 5, 5, "column"},
    {"anchor",   2, ResizeAnchorOp,   5, 5, "x"},
    {"mark",     1, ResizeMarkOp,     5, 5, "x"},
    {"set",      1, ResizeSetOp,      4, 4, "",},
};

static int nResizeOps = sizeof(resizeOps) / sizeof(Blt_OpSpec);

/*
 *---------------------------------------------------------------------------
 *
 * ColumnResizeOp --
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnResizeOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	       Tcl_Obj *const *objv)
{
    TreeViewCmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nResizeOps, resizeOps, BLT_OP_ARG3, 
	objc, objv,0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (viewPtr, interp, objc, objv);
    return result;
}


static Blt_OpSpec columnOps[] =
{
    {"activate",  1, ColumnActivateOp,  3, 4, "?field?",},
    {"bind",      1, ColumnBindOp,      4, 6, "tagName ?sequence command?",},
    {"cget",      2, ColumnCgetOp,      5, 5, "field option",},
    {"configure", 2, ColumnConfigureOp, 4, 0, "field ?option value?...",},
    {"current",   2, ColumnCurrentOp,   3, 3, "",},
    {"delete",    1, ColumnDeleteOp,    3, 0, "?field...?",},
    {"highlight", 1, ColumnActivateOp,  3, 4, "?field?",},
    {"insert",    3, ColumnInsertOp,    5, 0, 
	"position field ?field...? ?option value?...",},
    {"invoke",    3, ColumnInvokeOp,    4, 4, "field",},
    {"names",     2, ColumnNamesOp,     3, 3, "",},
    {"nearest",   2, ColumnNearestOp,   4, 5, "x ?y?",},
    {"resize",    1, ColumnResizeOp,    3, 0, "arg",},
};
static int nColumnOps = sizeof(columnOps) / sizeof(Blt_OpSpec);

/*
 *---------------------------------------------------------------------------
 *
 * TreeViewColumnOp --
 *
 *---------------------------------------------------------------------------
 */
int
Blt_TreeView_ColumnOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
		Tcl_Obj *const *objv)
{
    TreeViewCmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nColumnOps, columnOps, BLT_OP_ARG2, 
	objc, objv,0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (viewPtr, interp, objc, objv);
    return result;
}


static int
InvokeCompare(TreeView *viewPtr, TreeViewEntry *e1Ptr, TreeViewEntry *e2Ptr, 
	      Tcl_Obj *cmdPtr)
{
    int result;
    Tcl_Obj *objv[8];
    int i;

    objv[0] = cmdPtr;
    objv[1] = Tcl_NewStringObj(Tk_PathName(viewPtr->tkwin), -1);
    objv[2] = Tcl_NewLongObj(Blt_Tree_NodeId(e1Ptr->node));
    objv[3] = Tcl_NewLongObj(Blt_Tree_NodeId(e2Ptr->node));
    objv[4] = Tcl_NewStringObj(viewPtr->sortColumnPtr->key, -1);
	     
    if (viewPtr->flatView) {
	objv[5] = Tcl_NewStringObj(e1Ptr->fullName, -1);
	objv[6] = Tcl_NewStringObj(e2Ptr->fullName, -1);
    } else {
	objv[5] = Tcl_NewStringObj(GETLABEL(e1Ptr), -1);
	objv[6] = Tcl_NewStringObj(GETLABEL(e2Ptr), -1);
    }
    for(i = 0; i < 7; i++) {
	Tcl_IncrRefCount(objv[i]);
    }
    objv[7] = NULL;
    result = Tcl_EvalObjv(viewPtr->interp, 7, objv, TCL_EVAL_GLOBAL);
    if ((result != TCL_OK) ||
	(Tcl_GetIntFromObj(viewPtr->interp, Tcl_GetObjResult(viewPtr->interp), 
			   &result) != TCL_OK)) {
	Tcl_BackgroundError(viewPtr->interp);
    }
    for(i = 0; i < 7; i++) {
	Tcl_DecrRefCount(objv[i]);
    }
    Tcl_ResetResult(viewPtr->interp);
    return result;
}

static TreeView *treeViewInstance;

static int
CompareEntries(const void *a, const void *b)
{
    TreeView *viewPtr;
    TreeViewEntry **e1PtrPtr = (TreeViewEntry **)a;
    TreeViewEntry **e2PtrPtr = (TreeViewEntry **)b;
    Tcl_Obj *obj1, *obj2;
    const char *s1, *s2;
    int result;

    viewPtr = (*e1PtrPtr)->viewPtr;
    obj1 = (*e1PtrPtr)->dataObjPtr;
    obj2 = (*e2PtrPtr)->dataObjPtr;
    s1 = Tcl_GetString(obj1);
    s2 = Tcl_GetString(obj2);
    result = 0;
    switch (viewPtr->sortType) {
    case SORT_ASCII:
	result = strcmp(s1, s2);
	break;

    case SORT_COMMAND:
	{
	    Tcl_Obj *cmdPtr;

	    cmdPtr = viewPtr->sortColumnPtr->sortCmdPtr;
	    if (cmdPtr == NULL) {
		cmdPtr = viewPtr->sortCmdPtr;
	    }
	    if (cmdPtr == NULL) {
		result = Blt_DictionaryCompare(s1, s2);
	    } else {
		result = InvokeCompare(viewPtr, *e1PtrPtr, *e2PtrPtr, cmdPtr);
	    }
	}
	break;

    case SORT_DICTIONARY:
	result = Blt_DictionaryCompare(s1, s2);
	break;

    case SORT_INTEGER:
	{
	    int i1, i2;

	    if (Tcl_GetIntFromObj(NULL, obj1, &i1)==TCL_OK) {
		if (Tcl_GetIntFromObj(NULL, obj2, &i2) == TCL_OK) {
		    result = i1 - i2;
		} else {
		    result = -1;
		} 
	    } else if (Tcl_GetIntFromObj(NULL, obj2, &i2) == TCL_OK) {
		result = 1;
	    } else {
		result = Blt_DictionaryCompare(s1, s2);
	    }
	}
	break;

    case SORT_REAL:
	{
	    double r1, r2;

	    if (Tcl_GetDoubleFromObj(NULL, obj1, &r1) == TCL_OK) {
		if (Tcl_GetDoubleFromObj(NULL, obj2, &r2) == TCL_OK) {
		    result = (r1 < r2) ? -1 : (r1 > r2) ? 1 : 0;
		} else {
		    result = -1;
		} 
	    } else if (Tcl_GetDoubleFromObj(NULL, obj2, &r2) == TCL_OK) {
		result = 1;
	    } else {
		result = Blt_DictionaryCompare(s1, s2);
	    }
	}
	break;
    }
    if (result == 0) {
	result = strcmp((*e1PtrPtr)->fullName, (*e2PtrPtr)->fullName);
    }
    if (viewPtr->sortDecreasing) {
	return -result;
    } 
    return result;
}


/*
 *---------------------------------------------------------------------------
 *
 * CompareNodes --
 *
 *	Comparison routine (used by qsort) to sort a chain of subnodes.
 *
 * Results:
 *	1 is the first is greater, -1 is the second is greater, 0
 *	if equal.
 *
 *---------------------------------------------------------------------------
 */
static int
CompareNodes(Blt_TreeNode *n1Ptr, Blt_TreeNode *n2Ptr)
{
    TreeView *viewPtr = treeViewInstance;
    TreeViewEntry *e1Ptr, *e2Ptr;

    e1Ptr = Blt_TreeView_NodeToEntry(viewPtr, *n1Ptr);
    e2Ptr = Blt_TreeView_NodeToEntry(viewPtr, *n2Ptr);

    /* Fetch the data for sorting. */
    if (viewPtr->sortType == SORT_COMMAND) {
	e1Ptr->dataObjPtr = Tcl_NewLongObj(Blt_Tree_NodeId(*n1Ptr));
	e2Ptr->dataObjPtr = Tcl_NewLongObj(Blt_Tree_NodeId(*n2Ptr));
    } else {
	Blt_TreeKey key;
	Tcl_Obj *objPtr;

	key = viewPtr->sortColumnPtr->key;
	if (Blt_TreeView_GetData(e1Ptr, key, &objPtr) != TCL_OK) {
	    e1Ptr->dataObjPtr = Tcl_NewStringObj("", -1);
	} else {
	    e1Ptr->dataObjPtr = objPtr;
	}
	if (Blt_TreeView_GetData(e2Ptr, key, &objPtr) != TCL_OK) {
	    e2Ptr->dataObjPtr = Tcl_NewStringObj("", -1);
	} else {
	    e2Ptr->dataObjPtr = objPtr;
	}
    }
    {
	Tcl_DString dString;

	Tcl_DStringInit(&dString);
	if (e1Ptr->fullName == NULL) {
	    Blt_TreeView_GetFullName(viewPtr, e1Ptr, TRUE, &dString);
	    e1Ptr->fullName = Blt_AssertStrdup(Tcl_DStringValue(&dString));
	}
	e1Ptr->dataObjPtr = Tcl_NewStringObj(e1Ptr->fullName, -1);
	if (e2Ptr->fullName == NULL) {
	    Blt_TreeView_GetFullName(viewPtr, e2Ptr, TRUE, &dString);
	    e2Ptr->fullName = Blt_AssertStrdup(Tcl_DStringValue(&dString));
	}
	e2Ptr->dataObjPtr = Tcl_NewStringObj(e2Ptr->fullName, -1);
	Tcl_DStringFree(&dString);
    }
    return CompareEntries(&e1Ptr, &e2Ptr);
}

static int
SortAutoOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	   Tcl_Obj *const *objv)
{

    if (objc == 4) {
	int bool;
	int isAuto;

	isAuto = ((viewPtr->flags & TV_SORT_AUTO) != 0);
	if (Tcl_GetBooleanFromObj(interp, objv[3], &bool) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (isAuto != bool) {
	    viewPtr->flags |= (LAYOUT_PENDING | DIRTY | RESORT);
	    Blt_TreeView_EventuallyRedraw(viewPtr);
	}
	if (bool) {
	    viewPtr->flags |= TV_SORT_AUTO;
	} else {
	    viewPtr->flags &= ~TV_SORT_AUTO;
	}
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), (viewPtr->flags & TV_SORT_AUTO));
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SortCgetOp --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SortCgetOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	   Tcl_Obj *const *objv)
{
    return Blt_ConfigureValueFromObj(interp, viewPtr->tkwin, sortSpecs, 
	(char *)viewPtr, objv[3], 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * SortConfigureOp --
 *
 * 	This procedure is called to process a list of configuration
 *	options database, in order to reconfigure the one of more
 *	entries in the widget.
 *
 *	  .h sort configure option value
 *
 * Results:
 *	A standard TCL result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for viewPtr; old resources get freed, if there
 *	were any.  The hypertext is redisplayed.
 *
 *---------------------------------------------------------------------------
 */
static int
SortConfigureOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
		Tcl_Obj *const *objv)
{
    int oldType;
    Tcl_Obj *oldCmdPtr;
    TreeViewColumn *oldColumn;

    if (objc == 3) {
	return Blt_ConfigureInfoFromObj(interp, viewPtr->tkwin, sortSpecs, 
		(char *)viewPtr, (Tcl_Obj *)NULL, 0);
    } else if (objc == 4) {
	return Blt_ConfigureInfoFromObj(interp, viewPtr->tkwin, sortSpecs, 
		(char *)viewPtr, objv[3], 0);
    }
    oldColumn = viewPtr->sortColumnPtr;
    oldType = viewPtr->sortType;
    oldCmdPtr = viewPtr->sortCmdPtr;
    if (Blt_ConfigureWidgetFromObj(interp, viewPtr->tkwin, sortSpecs, 
	objc - 3, objv + 3, (char *)viewPtr, BLT_CONFIG_OBJV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((oldColumn != viewPtr->sortColumnPtr)||(oldType != viewPtr->sortType) ||
	(oldCmdPtr != viewPtr->sortCmdPtr)) {
	viewPtr->flags &= ~SORTED;
	viewPtr->flags |= (DIRTY | RESORT);
    } 
    if (viewPtr->flags & TV_SORT_AUTO) {
	viewPtr->flags |= SORT_PENDING;
    }
    Blt_TreeView_EventuallyRedraw(viewPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
SortOnceOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	   Tcl_Obj *const *objv)
{
    int recurse;

    recurse = FALSE;
    if (objc > 3) {
	char *string;
	int length;

	string = Tcl_GetStringFromObj(objv[3], &length);
	if ((string[0] == '-') && (length > 1) &&
	    (strncmp(string, "-recurse", length) == 0)) {
	    objv++, objc--;
	    recurse = TRUE;
	}
    }
#ifdef notdef
    { 
	int i;

	treeViewInstance = viewPtr;
	for (i = 3; i < objc; i++) {
	    TreeViewEntry *entryPtr;
	    int result;
	    
	    if (Blt_TreeView_GetEntry(viewPtr, objv[i], &entryPtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (recurse) {
		result = Blt_Tree_Apply(entryPtr->node, SortApplyProc, viewPtr);
	    } else {
		result = SortApplyProc(entryPtr->node, viewPtr, TREE_PREORDER);
	    }
	    if (result != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
#endif
    viewPtr->flags |= (LAYOUT_PENDING | DIRTY | UPDATE | SORT_PENDING | 
		     RESORT);
    Blt_TreeView_EventuallyRedraw(viewPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_SortOp --
 *
 *	Comparison routine (used by qsort) to sort a chain of subnodes.
 *	A simple string comparison is performed on each node name.
 *
 *	.h sort auto
 *	.h sort once -recurse root
 *
 * Results:
 *	1 is the first is greater, -1 is the second is greater, 0
 *	if equal.
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec sortOps[] =
{
    {"auto",      1, SortAutoOp,      3, 4, "?boolean?",},
    {"cget",      2, SortCgetOp,      4, 4, "option",},
    {"configure", 2, SortConfigureOp, 3, 0, "?option value?...",},
    {"once",      1, SortOnceOp,      3, 0, "?-recurse? node...",},
};
static int nSortOps = sizeof(sortOps) / sizeof(Blt_OpSpec);

/*ARGSUSED*/
int
Blt_TreeView_SortOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
		    Tcl_Obj *const *objv)
{
    TreeViewCmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nSortOps, sortOps, BLT_OP_ARG2, objc, 
	    objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (viewPtr, interp, objc, objv);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * SortApplyProc --
 *
 *	Sorts the subnodes at a given node.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SortApplyProc(Blt_TreeNode node, ClientData clientData, int order)
{
    TreeView *viewPtr = clientData;

    if (!Blt_Tree_IsLeaf(node)) {
	Blt_Tree_SortNode(viewPtr->tree, node, CompareNodes);
    }
    return TCL_OK;
}
 
/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_SortFlatView --
 *
 *	Sorts the flatten array of entries.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_TreeView_SortFlatView(TreeView *viewPtr)
{
    TreeViewEntry *entryPtr, **p;

    viewPtr->flags &= ~SORT_PENDING;
    if ((viewPtr->sortType == SORT_NONE) || (viewPtr->nEntries < 2)) {
	return;
    }
    if (viewPtr->flags & SORTED) {
	int first, last;
	TreeViewEntry *hold;

	if (viewPtr->sortDecreasing == viewPtr->viewIsDecreasing) {
	    return;
	}

	/* 
	 * The view is already sorted but in the wrong direction. 
	 * Reverse the entries in the array.
	 */
 	for (first = 0, last = viewPtr->nEntries - 1; last > first; 
	     first++, last--) {
	    hold = viewPtr->flatArr[first];
	    viewPtr->flatArr[first] = viewPtr->flatArr[last];
	    viewPtr->flatArr[last] = hold;
	}
	viewPtr->viewIsDecreasing = viewPtr->sortDecreasing;
	viewPtr->flags |= SORTED | LAYOUT_PENDING;
	return;
    }
    /* Fetch each entry's data as Tcl_Objs for sorting. */
    if (viewPtr->sortColumnPtr == &viewPtr->treeColumn) {
	for(p = viewPtr->flatArr; *p != NULL; p++) {
	    entryPtr = *p;
	    if (entryPtr->fullName == NULL) {
		Tcl_DString dString;

		Blt_TreeView_GetFullName(viewPtr, entryPtr, TRUE, &dString);
		entryPtr->fullName = 
		    Blt_AssertStrdup(Tcl_DStringValue(&dString));
		Tcl_DStringFree(&dString);
	    }
	    entryPtr->dataObjPtr = Tcl_NewStringObj(entryPtr->fullName, -1);
	    Tcl_IncrRefCount(entryPtr->dataObjPtr);
	}
    } else {
	Blt_TreeKey key;
	Tcl_Obj *objPtr;

	key = viewPtr->sortColumnPtr->key;
	for(p = viewPtr->flatArr; *p != NULL; p++) {
	    entryPtr = *p;
	    if (Blt_TreeView_GetData(entryPtr, key, &objPtr) != TCL_OK) {
		objPtr = Tcl_NewStringObj("", -1);
	    }
	    entryPtr->dataObjPtr = objPtr;
	    Tcl_IncrRefCount(entryPtr->dataObjPtr);
	}
    }
    qsort((char *)viewPtr->flatArr, viewPtr->nEntries, sizeof(TreeViewEntry *),
	  (QSortCompareProc *)CompareEntries);

    /* Free all the Tcl_Objs used for comparison data. */
    for(p = viewPtr->flatArr; *p != NULL; p++) {
	Tcl_DecrRefCount((*p)->dataObjPtr);
    }
    viewPtr->viewIsDecreasing = viewPtr->sortDecreasing;
    viewPtr->flags |= SORTED;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_SortView --
 *
 *	Sorts the tree array of entries.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_TreeView_SortView(TreeView *viewPtr)
{
    viewPtr->flags &= ~SORT_PENDING;
    if (viewPtr->sortType != SORT_NONE) {
	treeViewInstance = viewPtr;
	Blt_Tree_Apply(viewPtr->rootPtr->node, SortApplyProc, viewPtr);
    }
    viewPtr->viewIsDecreasing = viewPtr->sortDecreasing;
}


#endif /* NO_TREEVIEW */
