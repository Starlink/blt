
/*
 * bltTreeView.c --
 *
 * This module implements an hierarchy widget for the BLT toolkit.
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

#include "bltTreeView.h"

#define BUTTON_PAD		2
#define BUTTON_IPAD		1
#define BUTTON_SIZE		7
#define COLUMN_PAD		2
#define FOCUS_WIDTH		1
#define ICON_PADX		2
#define ICON_PADY		1
#define INSET_PAD		0
#define LABEL_PADX		0
#define LABEL_PADY		0

#include <X11/Xutil.h>
#include <X11/Xatom.h>

#define DEF_ICON_WIDTH		1
#define DEF_ICON_HEIGHT		16

typedef TreeViewColumn Column;

typedef ClientData (TagProc)(TreeView *viewPtr, const char *string);

static Blt_TreeApplyProc DeleteApplyProc;
static Blt_TreeApplyProc CreateApplyProc;

BLT_EXTERN Blt_CustomOption bltTreeViewDataOption;

static Blt_OptionParseProc ObjToTree;
static Blt_OptionPrintProc TreeToObj;
static Blt_OptionFreeProc FreeTree;
Blt_CustomOption bltTreeViewTreeOption =
{
    ObjToTree, TreeToObj, FreeTree, NULL,
};

static Blt_OptionParseProc ObjToIcons;
static Blt_OptionPrintProc IconsToObj;
static Blt_OptionFreeProc FreeIcons;

Blt_CustomOption bltTreeViewIconsOption =
{
    ObjToIcons, IconsToObj, FreeIcons, NULL,
};

static Blt_OptionParseProc ObjToButton;
static Blt_OptionPrintProc ButtonToObj;
static Blt_CustomOption buttonOption = {
    ObjToButton, ButtonToObj, NULL, NULL,
};

static Blt_OptionParseProc ObjToUid;
static Blt_OptionPrintProc UidToObj;
static Blt_OptionFreeProc FreeUid;
Blt_CustomOption bltTreeViewUidOption = {
    ObjToUid, UidToObj, FreeUid, NULL,
};

static Blt_OptionParseProc ObjToScrollmode;
static Blt_OptionPrintProc ScrollmodeToObj;
static Blt_CustomOption scrollmodeOption = {
    ObjToScrollmode, ScrollmodeToObj, NULL, NULL,
};

static Blt_OptionParseProc ObjToSelectmode;
static Blt_OptionPrintProc SelectmodeToObj;
static Blt_CustomOption selectmodeOption = {
    ObjToSelectmode, SelectmodeToObj, NULL, NULL,
};

static Blt_OptionParseProc ObjToSeparator;
static Blt_OptionPrintProc SeparatorToObj;
static Blt_OptionFreeProc FreeSeparator;
static Blt_CustomOption separatorOption = {
    ObjToSeparator, SeparatorToObj, FreeSeparator, NULL,
};

static Blt_OptionParseProc ObjToLabel;
static Blt_OptionPrintProc LabelToObj;
static Blt_OptionFreeProc FreeLabel;
static Blt_CustomOption labelOption = {
    ObjToLabel, LabelToObj, FreeLabel, NULL,
};

static Blt_OptionParseProc ObjToStyles;
static Blt_OptionPrintProc StylesToObj;
static Blt_CustomOption stylesOption = {
    ObjToStyles, StylesToObj, NULL, NULL,
};

#define DEF_BUTTON_ACTIVE_BACKGROUND	RGB_WHITE
#define DEF_BUTTON_ACTIVE_BG_MONO	STD_ACTIVE_BG_MONO
#define DEF_BUTTON_ACTIVE_FOREGROUND	STD_ACTIVE_FOREGROUND
#define DEF_BUTTON_ACTIVE_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_BUTTON_BORDERWIDTH		"1"
#define DEF_BUTTON_CLOSE_RELIEF		"solid"
#define DEF_BUTTON_OPEN_RELIEF		"solid"
#define DEF_BUTTON_NORMAL_BACKGROUND	RGB_WHITE
#define DEF_BUTTON_NORMAL_BG_MONO	STD_NORMAL_BG_MONO
#define DEF_BUTTON_NORMAL_FOREGROUND	STD_NORMAL_FOREGROUND
#define DEF_BUTTON_NORMAL_FG_MONO	STD_NORMAL_FG_MONO
#define DEF_BUTTON_SIZE			"7"

/* RGB_LIGHTBLUE1 */

#define DEF_ACTIVE_FOREGROUND	RBG_BLACK
#define DEF_ACTIVE_RELIEF	"flat"
#define DEF_ALLOW_DUPLICATES	"yes"
#define DEF_BACKGROUND		RGB_WHITE
#define DEF_ALT_BACKGROUND	RGB_GREY97
#define DEF_BORDERWIDTH		STD_BORDERWIDTH
#define DEF_BUTTON		"auto"
#define DEF_DASHES		"dot"
#define DEF_EXPORT_SELECTION	"no"
#define DEF_FOREGROUND		STD_NORMAL_FOREGROUND
#define DEF_FG_MONO		STD_NORMAL_FG_MONO
#define DEF_FLAT		"no"
#define DEF_FOCUS_DASHES	"dot"
#define DEF_FOCUS_EDIT		"no"
#define DEF_FOCUS_FOREGROUND	STD_ACTIVE_FOREGROUND
#define DEF_FOCUS_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_FONT		STD_FONT_SMALL
#define DEF_HEIGHT		"400"
#define DEF_HIDE_LEAVES		"no"
#define DEF_HIDE_ROOT		"yes"
#define DEF_FOCUS_HIGHLIGHT_BACKGROUND	STD_NORMAL_BACKGROUND
#define DEF_FOCUS_HIGHLIGHT_COLOR	RGB_BLACK
#define DEF_FOCUS_HIGHLIGHT_WIDTH	"2"
#define DEF_ICONVARIABLE	((char *)NULL)
#define DEF_ICONS "::blt::TreeView::closeIcon ::blt::TreeView::openIcon"
#define DEF_LINECOLOR		RGB_GREY30
#define DEF_LINECOLOR_MONO	STD_NORMAL_FG_MONO
#define DEF_LINESPACING		"0"
#define DEF_LINEWIDTH		"1"
#define DEF_MAKE_PATH		"no"
#define DEF_NEW_TAGS		"no"
#define DEF_NORMAL_BACKGROUND 	STD_NORMAL_BACKGROUND
#define DEF_NORMAL_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_RELIEF		"sunken"
#define DEF_RESIZE_CURSOR	"arrow"
#define DEF_SCROLL_INCREMENT	"20"
#define DEF_SCROLL_MODE		"hierbox"
#define DEF_SELECT_BACKGROUND 	STD_SELECT_BACKGROUND 
#define DEF_SELECT_BG_MONO  	STD_SELECT_BG_MONO
#define DEF_SELECT_BORDERWIDTH	"1"
#define DEF_SELECT_FOREGROUND 	STD_SELECT_FOREGROUND
#define DEF_SELECT_FG_MONO  	STD_SELECT_FG_MONO
#define DEF_SELECT_MODE		"single"
#define DEF_SELECT_RELIEF	"flat"
#define DEF_SHOW_ROOT		"yes"
#define DEF_SHOW_TITLES		"yes"
#define DEF_SORT_SELECTION	"no"
#define DEF_TAKE_FOCUS		"1"
#define DEF_TEXT_COLOR		STD_NORMAL_FOREGROUND
#define DEF_TEXT_MONO		STD_NORMAL_FG_MONO
#define DEF_TEXTVARIABLE	((char *)NULL)
#define DEF_TRIMLEFT		""
#define DEF_WIDTH		"200"

#define TvOffset(x)	Blt_Offset(TreeView, x)

Blt_ConfigSpec bltTreeViewButtonSpecs[] =
{
    {BLT_CONFIG_BACKGROUND, "-activebackground", "activeBackground", 
	"Background", DEF_BUTTON_ACTIVE_BACKGROUND, TvOffset(button.activeBg), 0},
    {BLT_CONFIG_SYNONYM, "-activebg", "activeBackground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-activefg", "activeForeground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-activeforeground", "activeForeground", "Foreground",
	DEF_BUTTON_ACTIVE_FOREGROUND, TvOffset(button.activeFgColor), 0},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	DEF_BUTTON_NORMAL_BACKGROUND, TvOffset(button.bg), 0},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_BUTTON_BORDERWIDTH, TvOffset(button.borderWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_RELIEF, "-closerelief", "closeRelief", "Relief",
	DEF_BUTTON_CLOSE_RELIEF, TvOffset(button.closeRelief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_BUTTON_NORMAL_FOREGROUND, TvOffset(button.fgColor), 0},
    {BLT_CONFIG_CUSTOM, "-images", "images", "Icons", (char *)NULL, 
	TvOffset(button.icons), BLT_CONFIG_NULL_OK, &bltTreeViewIconsOption},
    {BLT_CONFIG_RELIEF, "-openrelief", "openRelief", "Relief",
	DEF_BUTTON_OPEN_RELIEF, TvOffset(button.openRelief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-size", "size", "Size", DEF_BUTTON_SIZE, 
	TvOffset(button.reqSize), 0},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 0, 0}
};

#define EntryOffset(x)	Blt_Offset(TreeViewEntry, x)

Blt_ConfigSpec bltTreeViewEntrySpecs[] =
{
    {BLT_CONFIG_CUSTOM, "-bindtags", (char *)NULL, (char *)NULL, (char *)NULL, 
	EntryOffset(tagsUid), BLT_CONFIG_NULL_OK, &bltTreeViewUidOption},
    {BLT_CONFIG_CUSTOM, "-button", (char *)NULL, (char *)NULL, DEF_BUTTON, 
	EntryOffset(flags), BLT_CONFIG_DONT_SET_DEFAULT, &buttonOption},
    {BLT_CONFIG_CUSTOM, "-closecommand", (char *)NULL, (char *)NULL,
	(char *)NULL, EntryOffset(closeCmd), BLT_CONFIG_NULL_OK, 
	&bltTreeViewUidOption},
    {BLT_CONFIG_CUSTOM, "-data", (char *)NULL, (char *)NULL, (char *)NULL, 0, 
	BLT_CONFIG_NULL_OK, &bltTreeViewDataOption},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_FONT, "-font", (char *)NULL, (char *)NULL, (char *)NULL, 
	EntryOffset(font), 0},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", (char *)NULL, (char *)NULL,
	 EntryOffset(color), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_NNEG, "-height", (char *)NULL, (char *)NULL, 
	(char *)NULL, EntryOffset(reqHeight), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-icons", (char *)NULL, (char *)NULL, (char *)NULL, 
	EntryOffset(icons), BLT_CONFIG_NULL_OK, &bltTreeViewIconsOption},
    {BLT_CONFIG_CUSTOM, "-label", (char *)NULL, (char *)NULL, (char *)NULL, 
	EntryOffset(labelUid), 0, &labelOption},
    {BLT_CONFIG_CUSTOM, "-opencommand", (char *)NULL, (char *)NULL, 
	(char *)NULL, EntryOffset(openCmd), BLT_CONFIG_NULL_OK, 
	&bltTreeViewUidOption},
    {BLT_CONFIG_CUSTOM, "-styles", (char *)NULL, (char *)NULL, 
	(char *)NULL, EntryOffset(values), BLT_CONFIG_NULL_OK, &stylesOption},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 
	0, 0}
};

Blt_ConfigSpec bltTreeViewSpecs[] =
{
    {BLT_CONFIG_BITMASK, "-allowduplicates", "allowDuplicates", 
	"AllowDuplicates", DEF_ALLOW_DUPLICATES, TvOffset(flags),
	BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_ALLOW_DUPLICATES},
    {BLT_CONFIG_SYNONYM, "-altbg", "alternateBackground", (char *)NULL,
	(char *)NULL, 0, 0},
    {BLT_CONFIG_BACKGROUND, "-alternatebackground", "alternateBackground", 
	"Background", DEF_ALT_BACKGROUND, TvOffset(altBg), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_BITMASK, "-autocreate", "autoCreate", "AutoCreate", 
	DEF_MAKE_PATH, TvOffset(flags), BLT_CONFIG_DONT_SET_DEFAULT, 
	(Blt_CustomOption *)TV_FILL_ANCESTORS},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	DEF_BACKGROUND, TvOffset(bg), 0},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_BORDERWIDTH, TvOffset(borderWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-button", "button", "Button", DEF_BUTTON, 
	TvOffset(buttonFlags), BLT_CONFIG_DONT_SET_DEFAULT, &buttonOption},
    {BLT_CONFIG_STRING, "-closecommand", "closeCommand", "CloseCommand",
	(char *)NULL, TvOffset(closeCmd), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor", (char *)NULL, 
	TvOffset(cursor), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_DASHES, "-dashes", "dashes", "Dashes", 	DEF_DASHES, 
	TvOffset(dashes), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BITMASK, "-exportselection", "exportSelection",
	"ExportSelection", DEF_EXPORT_SELECTION, TvOffset(flags),
	BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_SELECT_EXPORT},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_BOOLEAN, "-flat", "flat", "Flat", DEF_FLAT, TvOffset(flatView), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_DASHES, "-focusdashes", "focusDashes", "FocusDashes",
	DEF_FOCUS_DASHES, TvOffset(focusDashes), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-focusforeground", "focusForeground", "FocusForeground",
	DEF_FOCUS_FOREGROUND, TvOffset(focusColor),BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-focusforeground", "focusForeground", "FocusForeground",
	DEF_FOCUS_FG_MONO, TvOffset(focusColor), BLT_CONFIG_MONO_ONLY},
    {BLT_CONFIG_FONT, "-font", "font", "Font", DEF_FONT, TvOffset(font), 0},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground", DEF_TEXT_COLOR,
	TvOffset(fgColor), BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground", DEF_TEXT_MONO, 
	TvOffset(fgColor), BLT_CONFIG_MONO_ONLY},
    {BLT_CONFIG_PIXELS_NNEG, "-height", "height", "Height", DEF_HEIGHT, 
	TvOffset(reqHeight), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BITMASK, "-hideleaves", "hideLeaves", "HideLeaves",
	DEF_HIDE_LEAVES, TvOffset(flags), BLT_CONFIG_DONT_SET_DEFAULT, 
	(Blt_CustomOption *)TV_HIDE_LEAVES},
    {BLT_CONFIG_BITMASK, "-hideroot", "hideRoot", "HideRoot", DEF_HIDE_ROOT,
	TvOffset(flags), BLT_CONFIG_DONT_SET_DEFAULT, 
	(Blt_CustomOption *)HIDE_ROOT},
    {BLT_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_FOCUS_HIGHLIGHT_BACKGROUND, 
        TvOffset(highlightBgColor), 0},
    {BLT_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_FOCUS_HIGHLIGHT_COLOR, TvOffset(highlightColor), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-highlightthickness", "highlightThickness",
	"HighlightThickness", DEF_FOCUS_HIGHLIGHT_WIDTH, TvOffset(highlightWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-iconvariable", "iconVariable", "IconVariable", 
	DEF_TEXTVARIABLE, Blt_Offset(TreeView, iconVarObjPtr), 
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-icons", "icons", "Icons", DEF_ICONS, 
	TvOffset(icons), BLT_CONFIG_NULL_OK, &bltTreeViewIconsOption},
    {BLT_CONFIG_COLOR, "-linecolor", "lineColor", "LineColor",
	DEF_LINECOLOR, TvOffset(lineColor), BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-linecolor", "lineColor", "LineColor", 
	DEF_LINECOLOR_MONO, TvOffset(lineColor), BLT_CONFIG_MONO_ONLY},
    {BLT_CONFIG_PIXELS_NNEG, "-linespacing", "lineSpacing", "LineSpacing",
	DEF_LINESPACING, TvOffset(leader), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-linewidth", "lineWidth", "LineWidth", 
	DEF_LINEWIDTH, TvOffset(lineWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_STRING, "-opencommand", "openCommand", "OpenCommand",
	(char *)NULL, TvOffset(openCmd), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_RELIEF, "-relief", "relief", "Relief", DEF_RELIEF, 
	TvOffset(relief), 0},
    {BLT_CONFIG_CURSOR, "-resizecursor", "resizeCursor", "ResizeCursor",
	DEF_RESIZE_CURSOR, TvOffset(resizeCursor), 0},
    {BLT_CONFIG_CUSTOM, "-scrollmode", "scrollMode", "ScrollMode",
	DEF_SCROLL_MODE, TvOffset(scrollMode),
	BLT_CONFIG_DONT_SET_DEFAULT, &scrollmodeOption},
    {BLT_CONFIG_BACKGROUND, "-selectbackground", "selectBackground", 
	"Foreground", DEF_SELECT_BACKGROUND, TvOffset(selBg), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-selectborderwidth", "selectBorderWidth", 
	"BorderWidth", DEF_SELECT_BORDERWIDTH, TvOffset(selBW), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_STRING, "-selectcommand", "selectCommand", "SelectCommand",
	(char *)NULL, TvOffset(selectCmd), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_SELECT_FOREGROUND, TvOffset(selFgColor), 0},
    {BLT_CONFIG_CUSTOM, "-selectmode", "selectMode", "SelectMode",
	DEF_SELECT_MODE, TvOffset(selectMode), 
	BLT_CONFIG_DONT_SET_DEFAULT, &selectmodeOption},
    {BLT_CONFIG_RELIEF, "-selectrelief", "selectRelief", "Relief",
	DEF_SELECT_RELIEF, TvOffset(selRelief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-separator", "separator", "Separator", (char *)NULL, 
	TvOffset(pathSep), BLT_CONFIG_NULL_OK, &separatorOption},
    {BLT_CONFIG_BITMASK, "-newtags", "newTags", "newTags", 
	DEF_NEW_TAGS, TvOffset(flags), 
        BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_NEW_TAGS},
    {BLT_CONFIG_BITMASK, "-showtitles", "showTitles", "ShowTitles",
	DEF_SHOW_TITLES, TvOffset(flags), 0,
        (Blt_CustomOption *)TV_SHOW_COLUMN_TITLES},
    {BLT_CONFIG_BITMASK, "-sortselection", "sortSelection", "SortSelection",
	DEF_SORT_SELECTION, TvOffset(flags), 
        BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_SELECT_SORTED},
    {BLT_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_TAKE_FOCUS, TvOffset(takeFocus), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_OBJ, "-textvariable", "textVariable", "TextVariable", 
	DEF_TEXTVARIABLE, Blt_Offset(TreeView, textVarObjPtr), 
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_STRING, "-tree", "tree", "Tree", (char *)NULL, 
	TvOffset(treeName), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_STRING, "-trim", "trim", "Trim", DEF_TRIMLEFT, 
	TvOffset(trimLeft), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_NNEG, "-width", "width", "Width", DEF_WIDTH, 
	TvOffset(reqWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
	(char *)NULL, TvOffset(xScrollCmdObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_NNEG, "-xscrollincrement", "xScrollIncrement", 
	"ScrollIncrement", DEF_SCROLL_INCREMENT, TvOffset(xScrollUnits), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
	(char *)NULL, TvOffset(yScrollCmdObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_NNEG, "-yscrollincrement", "yScrollIncrement", 
	"ScrollIncrement", DEF_SCROLL_INCREMENT, 
	TvOffset(yScrollUnits), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 
	0, 0}
};

/* Forward Declarations */
BLT_EXTERN Blt_TreeApplyProc TreeViewSortApplyProc;
static Blt_BindPickProc PickItem;
static Blt_BindTagProc GetTags;
static Blt_TreeNotifyEventProc TreeEventProc;
static Blt_TreeTraceProc TreeTraceProc;
static Tcl_CmdDeleteProc WidgetInstCmdDeleteProc;
static Tcl_FreeProc DestroyEntry;
static Tcl_FreeProc DestroyTreeView;
static Tcl_IdleProc DisplayTreeView;
static Tcl_ObjCmdProc TreeViewObjCmd;
static Tk_EventProc TreeViewEventProc;
static Tk_ImageChangedProc IconChangedProc;
static Tk_SelectionProc SelectionProc;

static int ComputeVisibleEntries(TreeView *viewPtr);

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_EventuallyRedraw --
 *
 *	Queues a request to redraw the widget at the next idle point.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets redisplayed.  Right now we don't do selective
 *	redisplays:  the whole window will be redrawn.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_TreeView_EventuallyRedraw(TreeView *viewPtr)
{
    if ((viewPtr->tkwin != NULL) && ((viewPtr->flags & REDRAW_PENDING) == 0) &&
	((viewPtr->flags & DONT_UPDATE) == 0)) {
	viewPtr->flags |= REDRAW_PENDING;
	Tcl_DoWhenIdle(DisplayTreeView, viewPtr);
    }
}

void
Blt_TreeView_TraceColumn(TreeView *viewPtr, TreeViewColumn *cp)
{
    Blt_Tree_CreateTrace(viewPtr->tree, NULL /* Node */, cp->key, NULL,
	TREE_TRACE_FOREIGN_ONLY | TREE_TRACE_WRITE | TREE_TRACE_UNSET, 
	TreeTraceProc, viewPtr);
}

static void
TraceColumns(TreeView *viewPtr)
{
    Blt_ChainLink link;

    for(link = Blt_Chain_FirstLink(viewPtr->columns); link != NULL;
	link = Blt_Chain_NextLink(link)) {
	TreeViewColumn *cp;

	cp = Blt_Chain_GetValue(link);
	Blt_Tree_CreateTrace(
		viewPtr->tree, 
		NULL		/* Node */, 
		cp->key		/* Key pattern */, 
		NULL		/* Tag */,
	        TREE_TRACE_FOREIGN_ONLY | TREE_TRACE_WRITE | TREE_TRACE_UNSET, 
	        TreeTraceProc	/* Callback routine */, 
		viewPtr		/* Client data */);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToTree --
 *
 *	Convert the string representing the name of a tree object 
 *	into a tree token.
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
ObjToTree(
    ClientData clientData,		  /* Not used. */
    Tcl_Interp *interp,		          /* Interpreter to send results back
					   * to */
    Tk_Window tkwin,			  /* Not used. */
    Tcl_Obj *objPtr,		          /* Tcl_Obj representing the new
					   * value. */  
    char *widgRec,
    int offset,				  /* Offset to field in structure */
    int flags)	
{
    Blt_Tree tree = *(Blt_Tree *)(widgRec + offset);

    if (Blt_Tree_Attach(interp, tree, Tcl_GetString(objPtr)) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeToObj --
 *
 * Results:
 *	The string representation of the button boolean is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
TreeToObj(
    ClientData clientData,		  /* Not used. */
    Tcl_Interp *interp,		
    Tk_Window tkwin,			  /* Not used. */
    char *widgRec,
    int offset,				  /* Offset to field in structure */
    int flags)	
{
    Blt_Tree tree = *(Blt_Tree *)(widgRec + offset);

    if (tree == NULL) {
	return Tcl_NewStringObj("", -1);
    } else {
	return Tcl_NewStringObj(Blt_Tree_Name(tree), -1);
    }
}

/*ARGSUSED*/
static void
FreeTree(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    Blt_Tree *treePtr = (Blt_Tree *)(widgRec + offset);

    if (*treePtr != NULL) {
	Blt_TreeNode root;
	TreeView *viewPtr = clientData;

	/* 
	 * Release the current tree, removing any entry fields. 
	 */
	root = Blt_Tree_RootNode(*treePtr);
	Blt_Tree_Apply(root, DeleteApplyProc, viewPtr);
	Blt_TreeView_ClearSelection(viewPtr);
	Blt_Tree_Close(*treePtr);
	*treePtr = NULL;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToScrollmode --
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
ObjToScrollmode(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* New legend position string */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    char *string;
    char c;
    int *modePtr = (int *)(widgRec + offset);

    string = Tcl_GetString(objPtr);
    c = string[0];
    if ((c == 'l') && (strcmp(string, "listbox") == 0)) {
	*modePtr = BLT_SCROLL_MODE_LISTBOX;
    } else if ((c == 't') && (strcmp(string, "treeview") == 0)) {
	*modePtr = BLT_SCROLL_MODE_HIERBOX;
    } else if ((c == 'c') && (strcmp(string, "canvas") == 0)) {
	*modePtr = BLT_SCROLL_MODE_CANVAS;
    } else {
	Tcl_AppendResult(interp, "bad scroll mode \"", string,
	    "\": should be \"treeview\", \"listbox\", or \"canvas\"", 
		(char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ScrollmodeToObj --
 *
 * Results:
 *	The string representation of the button boolean is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ScrollmodeToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    int mode = *(int *)(widgRec + offset);

    switch (mode) {
    case BLT_SCROLL_MODE_LISTBOX:
	return Tcl_NewStringObj("listbox", -1);
    case BLT_SCROLL_MODE_HIERBOX:
	return Tcl_NewStringObj("hierbox", -1);
    case BLT_SCROLL_MODE_CANVAS:
	return Tcl_NewStringObj("canvas", -1);
    default:
	return Tcl_NewStringObj("unknown scroll mode", -1);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToSelectmode --
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
ObjToSelectmode(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* Tcl_Obj representing the new
					 * value. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    char *string;
    char c;
    int *modePtr = (int *)(widgRec + offset);

    string = Tcl_GetString(objPtr);
    c = string[0];
    if ((c == 's') && (strcmp(string, "single") == 0)) {
	*modePtr = SELECT_MODE_SINGLE;
    } else if ((c == 'm') && (strcmp(string, "multiple") == 0)) {
	*modePtr = SELECT_MODE_MULTIPLE;
    } else if ((c == 'a') && (strcmp(string, "active") == 0)) {
	*modePtr = SELECT_MODE_SINGLE;
    } else {
	Tcl_AppendResult(interp, "bad select mode \"", string,
	    "\": should be \"single\" or \"multiple\"", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SelectmodeToObj --
 *
 * Results:
 *	The string representation of the button boolean is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
SelectmodeToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    int mode = *(int *)(widgRec + offset);

    switch (mode) {
    case SELECT_MODE_SINGLE:
	return Tcl_NewStringObj("single", -1);
    case SELECT_MODE_MULTIPLE:
	return Tcl_NewStringObj("multiple", -1);
    default:
	return Tcl_NewStringObj("unknown scroll mode", -1);
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * ObjToButton --
 *
 *	Convert a string to one of three values.
 *		0 - false, no, off
 *		1 - true, yes, on
 *		2 - auto
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left in
 *	interpreter's result field.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToButton(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* Tcl_Obj representing the new
					 * value. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    char *string;
    int *flagsPtr = (int *)(widgRec + offset);

    string = Tcl_GetString(objPtr);
    if ((string[0] == 'a') && (strcmp(string, "auto") == 0)) {
	*flagsPtr &= ~BUTTON_MASK;
	*flagsPtr |= BUTTON_AUTO;
    } else {
	int bool;

	if (Tcl_GetBooleanFromObj(interp, objPtr, &bool) != TCL_OK) {
	    return TCL_ERROR;
	}
	*flagsPtr &= ~BUTTON_MASK;
	if (bool) {
	    *flagsPtr |= BUTTON_SHOW;
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ButtonToObj --
 *
 * Results:
 *	The string representation of the button boolean is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ButtonToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    int bool;
    unsigned int buttonFlags = *(int *)(widgRec + offset);

    bool = (buttonFlags & BUTTON_MASK);
    if (bool == BUTTON_AUTO) {
	return Tcl_NewStringObj("auto", 4);
    } else {
	return Tcl_NewBooleanObj(bool);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToScrollmode --
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
ObjToSeparator(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* Tcl_Obj representing the new
					 * value. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    char **sepPtr = (char **)(widgRec + offset);
    char *string;

    string = Tcl_GetString(objPtr);
    if (string[0] == '\0') {
	*sepPtr = SEPARATOR_LIST;
    } else if (strcmp(string, "none") == 0) {
	*sepPtr = SEPARATOR_NONE;
    } else {
	*sepPtr = Blt_AssertStrdup(string);
    } 
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SeparatorToObj --
 *
 * Results:
 *	The string representation of the separator is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
SeparatorToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    char *separator = *(char **)(widgRec + offset);

    if (separator == SEPARATOR_NONE) {
	return Tcl_NewStringObj("", -1);
    } else if (separator == SEPARATOR_LIST) {
	return Tcl_NewStringObj("list", -1);
    }  else {
	return Tcl_NewStringObj(separator, -1);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * FreeSeparator --
 *
 *	Free the UID from the widget record, setting it to NULL.
 *
 * Results:
 *	The UID in the widget record is set to NULL.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
FreeSeparator(ClientData clientData, Display *display, char *widgRec, 
	      int offset)
{
    char **sepPtr = (char **)(widgRec + offset);

    if ((*sepPtr != SEPARATOR_LIST) && (*sepPtr != SEPARATOR_NONE)) {
	Blt_Free(*sepPtr);
	*sepPtr = SEPARATOR_NONE;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToLabel --
 *
 *	Convert the string representing the label. 
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
ObjToLabel(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* Tcl_Obj representing the new
					 * value. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    UID *labelPtr = (UID *)(widgRec + offset);
    char *string;

    string = Tcl_GetString(objPtr);
    if (string[0] != '\0') {
	TreeView *viewPtr = clientData;

	*labelPtr = Blt_TreeView_GetUid(viewPtr, string);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeToObj --
 *
 * Results:
 *	The string of the entry's label is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
LabelToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,		
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    UID labelUid = *(UID *)(widgRec + offset);
    const char *string;

    if (labelUid == NULL) {
	TreeViewEntry *entryPtr  = (TreeViewEntry *)widgRec;

	string = Blt_Tree_NodeLabel(entryPtr->node);
    } else {
	string = labelUid;
    }
    return Tcl_NewStringObj(string, -1);
}

/*ARGSUSED*/
static void
FreeLabel(ClientData clientData, Display *display, char *widgRec, int offset)
{
    UID *labelPtr = (UID *)(widgRec + offset);

    if (*labelPtr != NULL) {
	TreeView *viewPtr = clientData;

	Blt_TreeView_FreeUid(viewPtr, *labelPtr);
	*labelPtr = NULL;
    }
}
/*

 *---------------------------------------------------------------------------
 *
 * ObjToStyles --
 *
 *	Convert the list representing the field-name style-name pairs into
 *	stylePtr's.
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
ObjToStyles(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* Tcl_Obj representing the new
					 * value. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    TreeViewEntry *entryPtr = (TreeViewEntry *)widgRec;
    TreeView *viewPtr;
    Tcl_Obj **objv;
    int objc;
    int i;

    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc & 1) {
	Tcl_AppendResult(interp, "odd number of field/style pairs in \"",
			 Tcl_GetString(objPtr), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    viewPtr = entryPtr->viewPtr;
    for (i = 0; i < objc; i += 2) {
	TreeViewValue *valuePtr;
	TreeViewStyle *stylePtr;
	TreeViewColumn *cp;
	char *string;

	if (Blt_TreeView_GetColumn(interp, viewPtr, objv[i], &cp)!=TCL_OK) {
	    return TCL_ERROR;
	}
	valuePtr = Blt_TreeView_FindValue(entryPtr, cp);
	if (valuePtr == NULL) {
	    return TCL_ERROR;
	}
	string = Tcl_GetString(objv[i+1]);
	stylePtr = NULL;
	if ((*string != '\0') && (Blt_TreeView_GetStyle(interp, viewPtr, string, 
		&stylePtr) != TCL_OK)) {
	    return TCL_ERROR;			/* No data ??? */
	}
	if (valuePtr->stylePtr != NULL) {
	    Blt_TreeView_FreeStyle(viewPtr, valuePtr->stylePtr);
	}
	valuePtr->stylePtr = stylePtr;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * StylesToObj --
 *
 * Results:
 *	The string representation of the button boolean is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
StylesToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,		
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    TreeViewEntry *entryPtr = (TreeViewEntry *)widgRec;
    TreeViewValue *vp;
    Tcl_Obj *listObjPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (vp = entryPtr->values; vp != NULL; vp = vp->nextPtr) {
	const char *styleName;

	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewStringObj(vp->columnPtr->key, -1));
	styleName = (vp->stylePtr != NULL) ? vp->stylePtr->name : "";
	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewStringObj(styleName, -1));
    }
    return listObjPtr;
}


/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_GetUid --
 *
 *	Gets or creates a unique string identifier.  Strings are reference
 *	counted.  The string is placed into a hashed table local to the
 *	treeview.
 *
 * Results:
 *	Returns the pointer to the hashed string.
 *
 *---------------------------------------------------------------------------
 */
UID
Blt_TreeView_GetUid(TreeView *viewPtr, const char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;
    size_t refCount;

    hPtr = Blt_CreateHashEntry(&viewPtr->uidTable, string, &isNew);
    if (isNew) {
	refCount = 1;
    } else {
	refCount = (size_t)Blt_GetHashValue(hPtr);
	refCount++;
    }
    Blt_SetHashValue(hPtr, refCount);
    return Blt_GetHashKey(&viewPtr->uidTable, hPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_FreeUid --
 *
 *	Releases the uid.  Uids are reference counted, so only when the
 *	reference count is zero (i.e. no one else is using the string) is the
 *	entry removed from the hash table.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_TreeView_FreeUid(TreeView *viewPtr, UID uid)
{
    Blt_HashEntry *hPtr;
    size_t refCount;

    hPtr = Blt_FindHashEntry(&viewPtr->uidTable, uid);
    assert(hPtr != NULL);
    refCount = (size_t)Blt_GetHashValue(hPtr);
    refCount--;
    if (refCount > 0) {
	Blt_SetHashValue(hPtr, refCount);
    } else {
	Blt_DeleteHashEntry(&viewPtr->uidTable, hPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToUid --
 *
 *	Converts the string to a Uid. Uid's are hashed, reference counted
 *	strings.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToUid(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* Tcl_Obj representing the new
					 * value. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    TreeView *viewPtr = clientData;
    UID *uidPtr = (UID *)(widgRec + offset);

    *uidPtr = Blt_TreeView_GetUid(viewPtr, Tcl_GetString(objPtr));
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * UidToObj --
 *
 *	Returns the uid as a string.
 *
 * Results:
 *	The fill style string is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
UidToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    UID uid = *(UID *)(widgRec + offset);

    if (uid == NULL) {
	return Tcl_NewStringObj("", -1);
    } else {
	return Tcl_NewStringObj(uid, -1);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * FreeUid --
 *
 *	Free the UID from the widget record, setting it to NULL.
 *
 * Results:
 *	The UID in the widget record is set to NULL.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
FreeUid(ClientData clientData, Display *display, char *widgRec, int offset)
{
    UID *uidPtr = (UID *)(widgRec + offset);

    if (*uidPtr != NULL) {
	TreeView *viewPtr = clientData;

	Blt_TreeView_FreeUid(viewPtr, *uidPtr);
	*uidPtr = NULL;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * IconChangedProc
 *
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
IconChangedProc(
    ClientData clientData,
    int x,				/* Not used. */
    int y,				/* Not used. */
    int width,				/* Not used. */
    int height,				/* Not used. */
    int imageWidth,			/* Not used. */
    int imageHeight)			/* Not used. */
{
    TreeView *viewPtr = clientData;

    viewPtr->flags |= (DIRTY | LAYOUT_PENDING | SCROLL_PENDING);
    Blt_TreeView_EventuallyRedraw(viewPtr);
}

TreeViewIcon
Blt_TreeView_GetIcon(TreeView *viewPtr, const char *iconName)
{
    Blt_HashEntry *hPtr;
    int isNew;
    struct _TreeViewIcon *iconPtr;

    hPtr = Blt_CreateHashEntry(&viewPtr->iconTable, iconName, &isNew);
    if (isNew) {
	Tk_Image tkImage;
	int width, height;

	tkImage = Tk_GetImage(viewPtr->interp, viewPtr->tkwin, (char *)iconName, 
		IconChangedProc, viewPtr);
	if (tkImage == NULL) {
	    Blt_DeleteHashEntry(&viewPtr->iconTable, hPtr);
	    return NULL;
	}
	Tk_SizeOfImage(tkImage, &width, &height);
	iconPtr = Blt_AssertMalloc(sizeof(struct _TreeViewIcon));
	iconPtr->tkImage = tkImage;
	iconPtr->hashPtr = hPtr;
	iconPtr->refCount = 1;
	iconPtr->width = width;
	iconPtr->height = height;
	Blt_SetHashValue(hPtr, iconPtr);
    } else {
	iconPtr = Blt_GetHashValue(hPtr);
	iconPtr->refCount++;
    }
    return iconPtr;
}

void
Blt_TreeView_FreeIcon(TreeView *viewPtr, TreeViewIcon icon)
{
    struct _TreeViewIcon *iconPtr = icon;

    iconPtr->refCount--;
    if (iconPtr->refCount == 0) {
	Blt_DeleteHashEntry(&viewPtr->iconTable, iconPtr->hashPtr);
	Tk_FreeImage(iconPtr->tkImage);
	Blt_Free(iconPtr);
    }
}

static void
DumpIconTable(TreeView *viewPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    struct _TreeViewIcon *iconPtr;

    for (hPtr = Blt_FirstHashEntry(&viewPtr->iconTable, &iter);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	iconPtr = Blt_GetHashValue(hPtr);
	Tk_FreeImage(iconPtr->tkImage);
	Blt_Free(iconPtr);
    }
    Blt_DeleteHashTable(&viewPtr->iconTable);
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToIcons --
 *
 *	Convert a list of image names into Tk images.
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
ObjToIcons(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* Tcl_Obj representing the new
					 * value. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    Tcl_Obj **objv;
    TreeView *viewPtr = clientData;
    TreeViewIcon **iconPtrPtr = (TreeViewIcon **)(widgRec + offset);
    TreeViewIcon *icons;
    int objc;
    int result;

    result = TCL_OK;
    icons = NULL;
    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc > 0) {
	int i;
	
	icons = Blt_AssertMalloc(sizeof(TreeViewIcon *) * (objc + 1));
	for (i = 0; i < objc; i++) {
	    icons[i] = Blt_TreeView_GetIcon(viewPtr, Tcl_GetString(objv[i]));
	    if (icons[i] == NULL) {
		result = TCL_ERROR;
		break;
	    }
	}
	icons[i] = NULL;
    }
    *iconPtrPtr = icons;
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * IconsToObj --
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
IconsToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    TreeViewIcon *icons = *(TreeViewIcon **)(widgRec + offset);
    Tcl_Obj *listObjPtr;
    
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (icons != NULL) {
	TreeViewIcon *iconPtr;

	for (iconPtr = icons; *iconPtr != NULL; iconPtr++) {
	    Tcl_Obj *objPtr;

	    objPtr = Tcl_NewStringObj(Blt_Image_Name((*iconPtr)->tkImage), -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);

	}
    }
    return listObjPtr;
}

/*ARGSUSED*/
static void
FreeIcons(ClientData clientData, Display *display, char *widgRec, int offset)
{
    TreeViewIcon **iconsPtr = (TreeViewIcon **)(widgRec + offset);

    if (*iconsPtr != NULL) {
	TreeViewIcon *ip;
	TreeView *viewPtr = clientData;

	for (ip = *iconsPtr; *ip != NULL; ip++) {
	    Blt_TreeView_FreeIcon(viewPtr, *ip);
	}
	Blt_Free(*iconsPtr);
	*iconsPtr = NULL;
    }
}

TreeViewEntry *
Blt_TreeView_NodeToEntry(TreeView *viewPtr, Blt_TreeNode node)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&viewPtr->entryTable, (char *)node);
    if (hPtr == NULL) {
	fprintf(stderr, "Blt_TreeView_NodeToEntry: can't find node %s\n", 
		Blt_Tree_NodeLabel(node));
	abort();
	return NULL;
    }
    return Blt_GetHashValue(hPtr);
}

TreeViewEntry *
Blt_TreeView_FindEntry(TreeView *viewPtr, Blt_TreeNode node)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&viewPtr->entryTable, (char *)node);
    if (hPtr == NULL) {
	return NULL;
    }
    return Blt_GetHashValue(hPtr);
}

int
Blt_TreeView_Apply(
    TreeView *viewPtr,
    TreeViewEntry *entryPtr,		/* Root entry of subtree. */
    TreeViewApplyProc *proc,		/* Procedure called for each entry. */
    unsigned int flags)
{
    if ((flags & ENTRY_HIDE) && (Blt_TreeView_EntryIsHidden(entryPtr))) {
	return TCL_OK;			/* Hidden node. */
    }
    if ((flags & entryPtr->flags) & ENTRY_HIDE) {
	return TCL_OK;			/* Hidden node. */
    }
    if ((flags | entryPtr->flags) & ENTRY_CLOSED) {
	TreeViewEntry *childPtr;
	Blt_TreeNode node, next;

	for (node = Blt_Tree_FirstChild(entryPtr->node); node != NULL; 
	     node = next) {
	    next = Blt_Tree_NextSibling(node);
	    /* 
	     * Get the next child before calling Blt_TreeView_Apply
	     * recursively.  This is because the apply callback may delete the
	     * node and its link.
	     */
	    childPtr = Blt_TreeView_NodeToEntry(viewPtr, node);
	    if (Blt_TreeView_Apply(viewPtr, childPtr, proc, flags) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    if ((*proc) (viewPtr, entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

int
Blt_TreeView_EntryIsHidden(TreeViewEntry *entryPtr)
{
    TreeView *viewPtr = entryPtr->viewPtr; 

    if ((viewPtr->flags & TV_HIDE_LEAVES) && (Blt_Tree_IsLeaf(entryPtr->node))) {
	return TRUE;
    }
    return (entryPtr->flags & ENTRY_HIDE) ? TRUE : FALSE;
}

#ifdef notdef
int
TreeViewEntryIsMapped(TreeViewEntry *entryPtr)
{
    TreeView *viewPtr = entryPtr->viewPtr; 

    /* Don't check if the entry itself is open, only that its ancestors
     * are. */
    if (TreeViewEntryIsHidden(entryPtr)) {
	return FALSE;
    }
    if (entryPtr == viewPtr->rootPtr) {
	return TRUE;
    }
    entryPtr = Blt_TreeView_ParentEntry(entryPtr);
    while (entryPtr != viewPtr->rootPtr) {
	if (entryPtr->flags & (ENTRY_CLOSED | ENTRY_HIDE)) {
	    return FALSE;
	}
	entryPtr = Blt_TreeView_ParentEntry(entryPtr);
    }
    return TRUE;
}
#endif

TreeViewEntry *
Blt_TreeView_FirstChild(TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeNode node;
    TreeView *viewPtr = entryPtr->viewPtr; 

    for (node = Blt_Tree_FirstChild(entryPtr->node); node != NULL; 
	 node = Blt_Tree_NextSibling(node)) {
	entryPtr = Blt_TreeView_NodeToEntry(viewPtr, node);
	if (((mask & ENTRY_HIDE) == 0) || 
	    (!Blt_TreeView_EntryIsHidden(entryPtr))) {
	    return entryPtr;
	}
    }
    return NULL;
}

TreeViewEntry *
Blt_TreeView_LastChild(TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeNode node;
    TreeView *viewPtr = entryPtr->viewPtr; 

    for (node = Blt_Tree_LastChild(entryPtr->node); node != NULL; 
	 node = Blt_Tree_PrevSibling(node)) {
	entryPtr = Blt_TreeView_NodeToEntry(viewPtr, node);
	if (((mask & ENTRY_HIDE) == 0) ||
	    (!Blt_TreeView_EntryIsHidden(entryPtr))) {
	    return entryPtr;
	}
    }
    return NULL;
}

TreeViewEntry *
Blt_TreeView_NextSibling(TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeNode node;
    TreeView *viewPtr = entryPtr->viewPtr; 

    for (node = Blt_Tree_NextSibling(entryPtr->node); node != NULL; 
	 node = Blt_Tree_NextSibling(node)) {
	entryPtr = Blt_TreeView_NodeToEntry(viewPtr, node);
	if (((mask & ENTRY_HIDE) == 0) ||
	    (!Blt_TreeView_EntryIsHidden(entryPtr))) {
	    return entryPtr;
	}
    }
    return NULL;
}

TreeViewEntry *
Blt_TreeView_PrevSibling(TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeNode node;
    TreeView *viewPtr = entryPtr->viewPtr; 

    for (node = Blt_Tree_PrevSibling(entryPtr->node); node != NULL; 
	 node = Blt_Tree_PrevSibling(node)) {
	entryPtr = Blt_TreeView_NodeToEntry(viewPtr, node);
	if (((mask & ENTRY_HIDE) == 0) ||
	    (!Blt_TreeView_EntryIsHidden(entryPtr))) {
	    return entryPtr;
	}
    }
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_PrevEntry --
 *
 *	Returns the "previous" node in the tree.  This node (in depth-first
 *	order) is its parent if the node has no siblings that are previous to
 *	it.  Otherwise it is the last descendant of the last sibling.  In this
 *	case, descend the sibling's hierarchy, using the last child at any
 *	ancestor, until we we find a leaf.
 *
 *---------------------------------------------------------------------------
 */
TreeViewEntry *
Blt_TreeView_PrevEntry(TreeViewEntry *entryPtr, unsigned int mask)
{
    TreeView *viewPtr = entryPtr->viewPtr; 
    TreeViewEntry *prevPtr;

    if (entryPtr->node == Blt_Tree_RootNode(viewPtr->tree)) {
	return NULL;			/* The root is the first node. */
    }
    prevPtr = Blt_TreeView_PrevSibling(entryPtr, mask);
    if (prevPtr == NULL) {
	/* There are no siblings previous to this one, so pick the parent. */
	prevPtr = Blt_TreeView_ParentEntry(entryPtr);
    } else {
	/*
	 * Traverse down the right-most thread in order to select the last
	 * entry.  Stop if we find a "closed" entry or reach a leaf.
	 */
	entryPtr = prevPtr;
	while ((entryPtr->flags & mask) == 0) {
	    entryPtr = Blt_TreeView_LastChild(entryPtr, mask);
	    if (entryPtr == NULL) {
		break;			/* Found a leaf. */
	    }
	    prevPtr = entryPtr;
	}
    }
    if (prevPtr == NULL) {
	return NULL;
    }
    return prevPtr;
}


/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_NextEntry --
 *
 *	Returns the "next" node in relation to the given node.  The next node
 *	(in depth-first order) is either the first child of the given node the
 *	next sibling if the node has no children (the node is a leaf).  If the
 *	given node is the last sibling, then try it's parent next sibling.
 *	Continue until we either find a next sibling for some ancestor or we
 *	reach the root node.  In this case the current node is the last node
 *	in the tree.
 *
 *---------------------------------------------------------------------------
 */
TreeViewEntry *
Blt_TreeView_NextEntry(TreeViewEntry *entryPtr, unsigned int mask)
{
    TreeView *viewPtr = entryPtr->viewPtr; 
    TreeViewEntry *nextPtr;
    int ignoreLeaf;

    ignoreLeaf = ((viewPtr->flags & TV_HIDE_LEAVES) && 
		  (Blt_Tree_IsLeaf(entryPtr->node)));

    if ((!ignoreLeaf) && ((entryPtr->flags & mask) == 0)) {
	nextPtr = Blt_TreeView_FirstChild(entryPtr, mask); 
	if (nextPtr != NULL) {
	    return nextPtr;		/* Pick the first sub-node. */
	}
    }
    /* 
     * Back up until to a level where we can pick a "next sibling".  For the
     * last entry we'll thread our way back to the root.
     */
    while (entryPtr != viewPtr->rootPtr) {
	nextPtr = Blt_TreeView_NextSibling(entryPtr, mask);
	if (nextPtr != NULL) {
	    return nextPtr;
	}
	entryPtr = Blt_TreeView_ParentEntry(entryPtr);
    }
    return NULL;			/* At root, no next node. */
}

void
Blt_TreeView_ConfigureButtons(TreeView *viewPtr)
{
    GC newGC;
    TreeViewButton *buttonPtr = &viewPtr->button;
    XGCValues gcValues;
    unsigned long gcMask;

    gcMask = GCForeground;
    gcValues.foreground = buttonPtr->fgColor->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (buttonPtr->normalGC != NULL) {
	Tk_FreeGC(viewPtr->display, buttonPtr->normalGC);
    }
    buttonPtr->normalGC = newGC;

    gcMask = GCForeground;
    gcValues.foreground = buttonPtr->activeFgColor->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (buttonPtr->activeGC != NULL) {
	Tk_FreeGC(viewPtr->display, buttonPtr->activeGC);
    }
    buttonPtr->activeGC = newGC;

    buttonPtr->width = buttonPtr->height = ODD(buttonPtr->reqSize);
    if (buttonPtr->icons != NULL) {
	int i;

	for (i = 0; i < 2; i++) {
	    int width, height;

	    if (buttonPtr->icons[i] == NULL) {
		break;
	    }
	    width = TreeView_IconWidth(buttonPtr->icons[i]);
	    height = TreeView_IconWidth(buttonPtr->icons[i]);
	    if (buttonPtr->width < width) {
		buttonPtr->width = width;
	    }
	    if (buttonPtr->height < height) {
		buttonPtr->height = height;
	    }
	}
    }
    buttonPtr->width += 2 * buttonPtr->borderWidth;
    buttonPtr->height += 2 * buttonPtr->borderWidth;
}

int
Blt_TreeView_ConfigureEntry(TreeView *viewPtr, TreeViewEntry *entryPtr,
			    int objc, Tcl_Obj *const *objv, int flags)
{
    GC newGC;
    Blt_ChainLink link;
    TreeViewColumn *cp;

    bltTreeViewIconsOption.clientData = viewPtr;
    bltTreeViewUidOption.clientData = viewPtr;
    labelOption.clientData = viewPtr;
    if (Blt_ConfigureWidgetFromObj(viewPtr->interp, viewPtr->tkwin, 
	bltTreeViewEntrySpecs, objc, objv, (char *)entryPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    /* 
     * Check if there are values that need to be added 
     */
    for(link = Blt_Chain_FirstLink(viewPtr->columns); link != NULL;
	link = Blt_Chain_NextLink(link)) {
	cp = Blt_Chain_GetValue(link);
	Blt_TreeView_AddValue(entryPtr, cp);
    }

    newGC = NULL;
    if ((entryPtr->font != NULL) || (entryPtr->color != NULL)) {
	Blt_Font font;
	XColor *colorPtr;
	XGCValues gcValues;
	unsigned long gcMask;

	font = entryPtr->font;
	if (font == NULL) {
	    font = Blt_TreeView_GetStyleFont(viewPtr, 
		viewPtr->treeColumn.stylePtr);
	}
	colorPtr = CHOOSE(viewPtr->fgColor, entryPtr->color);
	gcMask = GCForeground | GCFont;
	gcValues.foreground = colorPtr->pixel;
	gcValues.font = Blt_FontId(font);
	newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    }
    if (entryPtr->gc != NULL) {
	Tk_FreeGC(viewPtr->display, entryPtr->gc);
    }
    /* Assume all changes require a new layout. */
    entryPtr->gc = newGC;
    entryPtr->flags |= ENTRY_LAYOUT_PENDING;
    if (Blt_ConfigModified(bltTreeViewEntrySpecs, "-font", (char *)NULL)) {
	viewPtr->flags |= UPDATE;
    }
    viewPtr->flags |= (LAYOUT_PENDING | DIRTY);
    return TCL_OK;
}

void
Blt_TreeView_DestroyValue(TreeView *viewPtr, TreeViewValue *valuePtr)
{
    if (valuePtr->stylePtr != NULL) {
	Blt_TreeView_FreeStyle(viewPtr, valuePtr->stylePtr);
    }
    if (valuePtr->textPtr != NULL) {
	Blt_Free(valuePtr->textPtr);
    }
}


static void
DestroyEntry(DestroyData data)
{
    TreeViewEntry *entryPtr = (TreeViewEntry *)data;
    TreeView *viewPtr;
    
    viewPtr = entryPtr->viewPtr;
    bltTreeViewIconsOption.clientData = viewPtr;
    bltTreeViewUidOption.clientData = viewPtr;
    labelOption.clientData = viewPtr;
    Blt_FreeOptions(bltTreeViewEntrySpecs, (char *)entryPtr, viewPtr->display,
	0);
    if (!Blt_Tree_TagTableIsShared(viewPtr->tree)) {
	/* Don't clear tags unless this client is the only one using
	 * the tag table.*/
	Blt_Tree_ClearTags(viewPtr->tree, entryPtr->node);
    }
    if (entryPtr->gc != NULL) {
	Tk_FreeGC(viewPtr->display, entryPtr->gc);
    }
    /* Delete the chain of data values from the entry. */
    if (entryPtr->values != NULL) {
	TreeViewValue *valuePtr, *nextPtr;
	
	for (valuePtr = entryPtr->values; valuePtr != NULL; 
	     valuePtr = nextPtr) {
	    nextPtr = valuePtr->nextPtr;
	    Blt_TreeView_DestroyValue(viewPtr, valuePtr);
	}
	entryPtr->values = NULL;
    }
    if (entryPtr->fullName != NULL) {
	Blt_Free(entryPtr->fullName);
    }
    if (entryPtr->textPtr != NULL) {
	Blt_Free(entryPtr->textPtr);
    }
    Blt_PoolFreeItem(viewPtr->entryPool, entryPtr);
}

TreeViewEntry *
Blt_TreeView_ParentEntry(TreeViewEntry *entryPtr)
{
    TreeView *viewPtr = entryPtr->viewPtr; 
    Blt_TreeNode node;

    if (entryPtr->node == Blt_Tree_RootNode(viewPtr->tree)) {
	return NULL;
    }
    node = Blt_Tree_ParentNode(entryPtr->node);
    if (node == NULL) {
	return NULL;
    }
    return Blt_TreeView_NodeToEntry(viewPtr, node);
}

static void
FreeEntry(TreeView *viewPtr, TreeViewEntry *entryPtr)
{
    Blt_HashEntry *hPtr;

    if (entryPtr == viewPtr->activePtr) {
	viewPtr->activePtr = Blt_TreeView_ParentEntry(entryPtr);
    }
    if (entryPtr == viewPtr->activeBtnPtr) {
	viewPtr->activeBtnPtr = NULL;
    }
    if (entryPtr == viewPtr->focusPtr) {
	viewPtr->focusPtr = Blt_TreeView_ParentEntry(entryPtr);
	Blt_SetFocusItem(viewPtr->bindTable, viewPtr->focusPtr, ITEM_ENTRY);
    }
    if (entryPtr == viewPtr->selAnchorPtr) {
	viewPtr->selMarkPtr = viewPtr->selAnchorPtr = NULL;
    }
    Blt_TreeView_DeselectEntry(viewPtr, entryPtr);
    Blt_TreeView_PruneSelection(viewPtr, entryPtr);
    Blt_DeleteBindings(viewPtr->bindTable, entryPtr);
    hPtr = Blt_FindHashEntry(&viewPtr->entryTable, entryPtr->node);
    if (hPtr != NULL) {
	Blt_DeleteHashEntry(&viewPtr->entryTable, hPtr);
    }
    entryPtr->node = NULL;

    Tcl_EventuallyFree(entryPtr, DestroyEntry);
    /*
     * Indicate that the screen layout of the hierarchy may have changed
     * because this node was deleted.  The screen positions of the nodes in
     * viewPtr->visibleArr are invalidated.
     */
    viewPtr->flags |= (LAYOUT_PENDING | DIRTY);
    Blt_TreeView_EventuallyRedraw(viewPtr);
}

int
Blt_TreeView_EntryIsSelected(TreeView *viewPtr, TreeViewEntry *entryPtr)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&viewPtr->selectTable, (char *)entryPtr);
    return (hPtr != NULL);
}

void
Blt_TreeView_SelectEntry(TreeView *viewPtr, TreeViewEntry *entryPtr)
{
    int isNew;
    Blt_HashEntry *hPtr;
    TreeViewIcon icon;
    const char *label;

    hPtr = Blt_CreateHashEntry(&viewPtr->selectTable, (char *)entryPtr, &isNew);
    if (isNew) {
	Blt_ChainLink link;

	link = Blt_Chain_Append(viewPtr->selected, entryPtr);
	Blt_SetHashValue(hPtr, link);
    }
    label = GETLABEL(entryPtr);
    if ((viewPtr->textVarObjPtr != NULL) && (label != NULL)) {
	Tcl_Obj *objPtr;
	
	objPtr = Tcl_NewStringObj(label, -1);
	if (Tcl_ObjSetVar2(viewPtr->interp, viewPtr->textVarObjPtr, NULL, 
		objPtr, TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
	    return;
	}
    }
    icon = Blt_TreeView_GetEntryIcon(viewPtr, entryPtr);
    if ((viewPtr->iconVarObjPtr != NULL) && (icon != NULL)) {
	Tcl_Obj *objPtr;
	
	objPtr = Tcl_NewStringObj(TreeView_IconName(icon), -1);
	if (Tcl_ObjSetVar2(viewPtr->interp, viewPtr->iconVarObjPtr, NULL, 
		objPtr, TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
	    return;
	}
    }
}

void
Blt_TreeView_DeselectEntry(TreeView *viewPtr, TreeViewEntry *entryPtr)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&viewPtr->selectTable, (char *)entryPtr);
    if (hPtr != NULL) {
	Blt_ChainLink link;

	link = Blt_GetHashValue(hPtr);
	Blt_Chain_DeleteLink(viewPtr->selected, link);
	Blt_DeleteHashEntry(&viewPtr->selectTable, hPtr);
    }
}

const char *
Blt_TreeView_GetFullName(TreeView *viewPtr, TreeViewEntry *entryPtr,
			 int checkEntryLabel, Tcl_DString *resultPtr)
{
    const char **names;		       /* Used the stack the component names. */
    const char *staticSpace[64+2];
    int level;
    int i;

    level = Blt_Tree_NodeDepth(entryPtr->node);
    if (viewPtr->rootPtr->labelUid == NULL) {
	level--;
    }
    if (level > 64) {
	names = Blt_AssertMalloc((level + 2) * sizeof(char *));
    } else {
	names = staticSpace;
    }
    for (i = level; i >= 0; i--) {
	Blt_TreeNode node;

	/* Save the name of each ancestor in the name array. */
	if (checkEntryLabel) {
	    names[i] = GETLABEL(entryPtr);
	} else {
	    names[i] = Blt_Tree_NodeLabel(entryPtr->node);
	}
	node = Blt_Tree_ParentNode(entryPtr->node);
	if (node != NULL) {
	    entryPtr = Blt_TreeView_NodeToEntry(viewPtr, node);
	}
    }
    Tcl_DStringInit(resultPtr);
    if (level >= 0) {
	if ((viewPtr->pathSep == SEPARATOR_LIST) || 
	    (viewPtr->pathSep == SEPARATOR_NONE)) {
	    for (i = 0; i <= level; i++) {
		Tcl_DStringAppendElement(resultPtr, names[i]);
	    }
	} else {
	    Tcl_DStringAppend(resultPtr, names[0], -1);
	    for (i = 1; i <= level; i++) {
		Tcl_DStringAppend(resultPtr, viewPtr->pathSep, -1);
		Tcl_DStringAppend(resultPtr, names[i], -1);
	    }
	}
    } else {
	if ((viewPtr->pathSep != SEPARATOR_LIST) &&
	    (viewPtr->pathSep != SEPARATOR_NONE)) {
	    Tcl_DStringAppend(resultPtr, viewPtr->pathSep, -1);
	}
    }
    if (names != staticSpace) {
	Blt_Free(names);
    }
    return Tcl_DStringValue(resultPtr);
}


int
Blt_TreeView_CloseEntry(TreeView *viewPtr, TreeViewEntry *entryPtr)
{
    const char *cmd;

    if (entryPtr->flags & ENTRY_CLOSED) {
	return TCL_OK;			/* Entry is already closed. */
    }
    entryPtr->flags |= ENTRY_CLOSED;
    viewPtr->flags |= LAYOUT_PENDING;

    /*
     * Invoke the entry's "close" command, if there is one. Otherwise try the
     * treeview's global "close" command.
     */
    cmd = CHOOSE(viewPtr->closeCmd, entryPtr->closeCmd);
    if (cmd != NULL) {
	Tcl_DString dString;
	int result;

	Blt_TreeView_PercentSubst(viewPtr, entryPtr, cmd, &dString);
	Tcl_Preserve(entryPtr);
	result = Tcl_GlobalEval(viewPtr->interp, Tcl_DStringValue(&dString));
	Tcl_Release(entryPtr);
	Tcl_DStringFree(&dString);
	if (result != TCL_OK) {
	    viewPtr->flags |= DIRTY;
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}


int
Blt_TreeView_OpenEntry(TreeView *viewPtr, TreeViewEntry *entryPtr)
{
    const char *cmd;

    if ((entryPtr->flags & ENTRY_CLOSED) == 0) {
	return TCL_OK;			/* Entry is already open. */
    }
    entryPtr->flags &= ~ENTRY_CLOSED;
    viewPtr->flags |= LAYOUT_PENDING;

    /*
     * If there's a "open" command proc specified for the entry, use that
     * instead of the more general "open" proc for the entire treeview.
     * Be careful because the "open" command may perform an update.
     */
    cmd = CHOOSE(viewPtr->openCmd, entryPtr->openCmd);
    if (cmd != NULL) {
	Tcl_DString dString;
	int result;

	Blt_TreeView_PercentSubst(viewPtr, entryPtr, cmd, &dString);
	Tcl_Preserve(entryPtr);
	result = Tcl_GlobalEval(viewPtr->interp, Tcl_DStringValue(&dString));
	Tcl_Release(entryPtr);
	Tcl_DStringFree(&dString);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_CreateEntry --
 *
 *	This procedure is called by the Tree object when a node is created and
 *	inserted into the tree.  It adds a new treeview entry field to the node.
 *
 * Results:
 *	Returns the entry.
 *
 *---------------------------------------------------------------------------
 */
int
Blt_TreeView_CreateEntry(
    TreeView *viewPtr,
    Blt_TreeNode node,			/* Node that has just been created. */
    int objc,
    Tcl_Obj *const *objv,
    int flags)
{
    TreeViewEntry *entryPtr;
    int isNew;
    Blt_HashEntry *hPtr;

    hPtr = Blt_CreateHashEntry(&viewPtr->entryTable, (char *)node, &isNew);
    if (isNew) {
	/* Create the entry structure */
	entryPtr = Blt_PoolAllocItem(viewPtr->entryPool, sizeof(TreeViewEntry));
	memset(entryPtr, 0, sizeof(TreeViewEntry));
	entryPtr->flags = (unsigned short)(viewPtr->buttonFlags | ENTRY_CLOSED);
	entryPtr->viewPtr = viewPtr;
	entryPtr->labelUid = NULL;
	entryPtr->node = node;
	Blt_SetHashValue(hPtr, entryPtr);

    } else {
	entryPtr = Blt_GetHashValue(hPtr);
    }
    if (Blt_TreeView_ConfigureEntry(viewPtr, entryPtr, objc, objv, flags) 
	!= TCL_OK) {
	FreeEntry(viewPtr, entryPtr);
	return TCL_ERROR;		/* Error configuring the entry. */
    }
    viewPtr->flags |= (LAYOUT_PENDING | DIRTY);
    Blt_TreeView_EventuallyRedraw(viewPtr);
    return TCL_OK;
}


/*ARGSUSED*/
static int
CreateApplyProc(Blt_TreeNode node, ClientData clientData, int order)
{
    TreeView *viewPtr = clientData; 
    return Blt_TreeView_CreateEntry(viewPtr, node, 0, NULL, 0);
}

/*ARGSUSED*/
static int
DeleteApplyProc(Blt_TreeNode node, ClientData clientData, int order)
{
    TreeView *viewPtr = clientData;
    /* 
     * Unsetting the tree value triggers a call back to destroy the entry and
     * also releases the Tcl_Obj that contains it.
     */
    return Blt_Tree_UnsetValueByKey(viewPtr->interp, viewPtr->tree, node, 
	viewPtr->treeColumn.key);
}

static int
TreeEventProc(ClientData clientData, Blt_TreeNotifyEvent *eventPtr)
{
    Blt_TreeNode node;
    TreeView *viewPtr = clientData; 

    node = Blt_Tree_GetNode(eventPtr->tree, eventPtr->inode);
    switch (eventPtr->type) {
    case TREE_NOTIFY_CREATE:
	return Blt_TreeView_CreateEntry(viewPtr, node, 0, NULL, 0);
    case TREE_NOTIFY_DELETE:
	/*  
	 * Deleting the tree node triggers a call back to free the treeview
	 * entry that is associated with it.
	 */
	if (node != NULL) {
	    TreeViewEntry *entryPtr;

	    entryPtr = Blt_TreeView_FindEntry(viewPtr, node);
	    if (entryPtr != NULL) {
		FreeEntry(viewPtr, entryPtr);
	    }
	}
	break;
    case TREE_NOTIFY_RELABEL:
	if (node != NULL) {
	    TreeViewEntry *entryPtr;

	    entryPtr = Blt_TreeView_NodeToEntry(viewPtr, node);
	    entryPtr->flags |= ENTRY_DIRTY;
	}
	/*FALLTHRU*/
    case TREE_NOTIFY_MOVE:
    case TREE_NOTIFY_SORT:
	viewPtr->flags |= (LAYOUT_PENDING | RESORT | DIRTY);
	Blt_TreeView_EventuallyRedraw(viewPtr);
	break;
    default:
	/* empty */
	break;
    }	
    return TCL_OK;
}

TreeViewValue *
Blt_TreeView_FindValue(TreeViewEntry *entryPtr, TreeViewColumn *cp)
{
    TreeViewValue *vp;

    for (vp = entryPtr->values; vp != NULL; vp = vp->nextPtr) {
	if (vp->columnPtr == cp) {
	    return vp;
	}
    }
    return NULL;
}

void
Blt_TreeView_AddValue(TreeViewEntry *entryPtr, TreeViewColumn *cp)
{
    if (Blt_TreeView_FindValue(entryPtr, cp) == NULL) {
	Tcl_Obj *objPtr;

	if (Blt_TreeView_GetData(entryPtr, cp->key, &objPtr) == TCL_OK) {
	    TreeViewValue *valuePtr;

	    /* Add a new value only if a data entry exists. */
	    valuePtr = Blt_PoolAllocItem(entryPtr->viewPtr->valuePool, 
			 sizeof(TreeViewValue));
	    valuePtr->columnPtr = cp;
	    valuePtr->nextPtr = entryPtr->values;
	    valuePtr->textPtr = NULL;
	    valuePtr->width = valuePtr->height = 0;
	    valuePtr->stylePtr = NULL;
	    valuePtr->fmtString = NULL;
	    entryPtr->values = valuePtr;
	}
    }
    entryPtr->viewPtr->flags |= (LAYOUT_PENDING | DIRTY);
    entryPtr->flags |= ENTRY_DIRTY;
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeTraceProc --
 *
 *	Mirrors the individual values of the tree object (they must also be
 *	listed in the widget's columns chain). This is because it must track and
 *	save the sizes of each individual data entry, rather than re-computing
 *	all the sizes each time the widget is redrawn.
 *
 *	This procedure is called by the Tree object when a node data value is
 *	set unset.
 *
 * Results:
 *	Returns TCL_OK.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TreeTraceProc(
    ClientData clientData,
    Tcl_Interp *interp,
    Blt_TreeNode node,			/* Node that has just been updated. */
    Blt_TreeKey key,			/* Key of value that's been updated. */
    unsigned int flags)
{
    Blt_HashEntry *hPtr;
    TreeView *viewPtr = clientData; 
    TreeViewColumn *cp;
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr, *nextPtr, *lastPtr;
    
    hPtr = Blt_FindHashEntry(&viewPtr->entryTable, (char *)node);
    if (hPtr == NULL) {
	return TCL_OK;			/* Not a node that we're interested
					 * in. */
    }
    entryPtr = Blt_GetHashValue(hPtr);
    flags &= TREE_TRACE_WRITE | TREE_TRACE_READ | TREE_TRACE_UNSET;
    switch (flags) {
    case TREE_TRACE_WRITE:
	hPtr = Blt_FindHashEntry(&viewPtr->columnTable, key);
	if (hPtr == NULL) {
	    return TCL_OK;		/* Data value isn't used by widget. */
	}
	cp = Blt_GetHashValue(hPtr);
	if (cp != &viewPtr->treeColumn) {
	    Blt_TreeView_AddValue(entryPtr, cp);
	}
	entryPtr->flags |= ENTRY_DIRTY;
	Blt_TreeView_EventuallyRedraw(viewPtr);
	viewPtr->flags |= (LAYOUT_PENDING | DIRTY);
	break;

    case TREE_TRACE_UNSET:
	lastPtr = NULL;
	for(valuePtr = entryPtr->values; valuePtr != NULL; valuePtr = nextPtr) {
	    nextPtr = valuePtr->nextPtr;
	    if (valuePtr->columnPtr->key == key) { 
		Blt_TreeView_DestroyValue(viewPtr, valuePtr);
		if (lastPtr == NULL) {
		    entryPtr->values = nextPtr;
		} else {
		    lastPtr->nextPtr = nextPtr;
		}
		entryPtr->flags |= ENTRY_DIRTY;
		Blt_TreeView_EventuallyRedraw(viewPtr);
		viewPtr->flags |= (LAYOUT_PENDING | DIRTY);
		break;
	    }
	    lastPtr = valuePtr;
	}		
	break;

    default:
	break;
    }
    return TCL_OK;
}

static void
FormatValue(TreeViewEntry *entryPtr, TreeViewValue *valuePtr)
{
    TreeViewColumn *colPtr;
    Tcl_Obj *resultObjPtr;
    Tcl_Obj *valueObjPtr;

    colPtr = valuePtr->columnPtr;
    if (Blt_TreeView_GetData(entryPtr, colPtr->key, &valueObjPtr) != TCL_OK) {
	return;				/* No data ??? */
    }
    if (valuePtr->fmtString != NULL) {
	Blt_Free(valuePtr->fmtString);
    }
    valuePtr->fmtString = NULL;
    if (valueObjPtr == NULL) {
	return;
    }
    if (colPtr->fmtCmdPtr  != NULL) {
	Tcl_Interp *interp = entryPtr->viewPtr->interp;
	Tcl_Obj *cmdObjPtr, *objPtr;
	int result;

	cmdObjPtr = Tcl_DuplicateObj(colPtr->fmtCmdPtr);
	objPtr = Tcl_NewLongObj(Blt_Tree_NodeId(entryPtr->node));
	Tcl_ListObjAppendElement(interp, cmdObjPtr, objPtr);
	Tcl_ListObjAppendElement(interp, cmdObjPtr, valueObjPtr);
	Tcl_IncrRefCount(cmdObjPtr);
	result = Tcl_EvalObjEx(interp, cmdObjPtr, TCL_EVAL_GLOBAL);
	Tcl_DecrRefCount(cmdObjPtr);
	if (result != TCL_OK) {
	    Tcl_BackgroundError(interp);
	    return;
	}
	resultObjPtr = Tcl_GetObjResult(interp);
    } else {
	resultObjPtr = valueObjPtr;
    }
    valuePtr->fmtString = Blt_Strdup(Tcl_GetString(resultObjPtr));
}


static void
GetValueSize(TreeViewEntry *entryPtr, TreeViewValue *valuePtr, 
	     TreeViewStyle *stylePtr)
{
    valuePtr->width = valuePtr->height = 0;
    FormatValue(entryPtr, valuePtr);
    stylePtr = CHOOSE(valuePtr->columnPtr->stylePtr, valuePtr->stylePtr);
    /* Measure the text string. */
    (*stylePtr->classPtr->measProc)(entryPtr->viewPtr, stylePtr, valuePtr);
}

static void
GetRowExtents(TreeViewEntry *entryPtr, int *widthPtr, int *heightPtr)
{
    TreeViewValue *valuePtr;
    int width, height;			/* Compute dimensions of row. */

    width = height = 0;
    for (valuePtr = entryPtr->values; valuePtr != NULL; 
	 valuePtr = valuePtr->nextPtr) {
	TreeViewStyle *stylePtr;
	int valueWidth;			/* Width of individual value.  */

	stylePtr = valuePtr->stylePtr;
	if (stylePtr == NULL) {
	    stylePtr = valuePtr->columnPtr->stylePtr;
	}
	if ((entryPtr->flags & ENTRY_DIRTY) || (stylePtr->flags & STYLE_DIRTY)){
	    GetValueSize(entryPtr, valuePtr, stylePtr);
	}
	if (valuePtr->height > height) {
	    height = valuePtr->height;
	}
	valueWidth = valuePtr->width;
	width += valueWidth;
    }	    
    *widthPtr = width;
    *heightPtr = height;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_NearestEntry --
 *
 *	Finds the entry closest to the given screen X-Y coordinates in the
 *	viewport.
 *
 * Results:
 *	Returns the pointer to the closest node.  If no node is visible (nodes
 *	may be hidden), NULL is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
TreeViewEntry *
Blt_TreeView_NearestEntry(TreeView *viewPtr, int x, int y, int selectOne)
{
    TreeViewEntry *lastPtr;
    TreeViewEntry **p;

    /*
     * We implicitly can pick only visible entries.  So make sure that the
     * tree exists.
     */
    if (viewPtr->nVisible == 0) {
	return NULL;
    }
    if (y < viewPtr->titleHeight) {
	return (selectOne) ? viewPtr->visibleArr[0] : NULL;
    }
    /*
     * Since the entry positions were previously computed in world
     * coordinates, convert Y-coordinate from screen to world coordinates too.
     */
    y = WORLDY(viewPtr, y);
    lastPtr = viewPtr->visibleArr[0];
    for (p = viewPtr->visibleArr; *p != NULL; p++) {
	TreeViewEntry *entryPtr;

	entryPtr = *p;
	/*
	 * If the start of the next entry starts beyond the point, use the last
	 * entry.
	 */
	if (entryPtr->worldY > y) {
	    return (selectOne) ? entryPtr : NULL;
	}
	if (y < (entryPtr->worldY + entryPtr->height)) {
	    return entryPtr;		/* Found it. */
	}
	lastPtr = entryPtr;
    }
    return (selectOne) ? lastPtr : NULL;
}


ClientData
Blt_TreeView_EntryTag(TreeView *viewPtr, const char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;				/* Not used. */

    hPtr = Blt_CreateHashEntry(&viewPtr->entryTagTable, string, &isNew);
    return Blt_GetHashKey(&viewPtr->entryTagTable, hPtr);
}

ClientData
Blt_TreeView_ButtonTag(TreeView *viewPtr, const char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;				/* Not used. */

    hPtr = Blt_CreateHashEntry(&viewPtr->buttonTagTable, string, &isNew);
    return Blt_GetHashKey(&viewPtr->buttonTagTable, hPtr);
}

ClientData
Blt_TreeView_ColumnTag(TreeView *viewPtr, const char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;				/* Not used. */

    hPtr = Blt_CreateHashEntry(&viewPtr->columnTagTable, string, &isNew);
    return Blt_GetHashKey(&viewPtr->columnTagTable, hPtr);
}

#ifdef notdef
ClientData
TreeViewStyleTag(TreeView *viewPtr, const char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;				/* Not used. */

    hPtr = Blt_CreateHashEntry(&viewPtr->styleTagTable, string, &isNew);
    return Blt_GetHashKey(&viewPtr->styleTagTable, hPtr);
}
#endif

static void
AddIdsToList(TreeView *viewPtr, Blt_List ids, const char *string, 
	     TagProc *tagProc)
{
    int argc;
    const char **argv;
    
    if (Tcl_SplitList((Tcl_Interp *)NULL, string, &argc, &argv) == TCL_OK) {
	const char **p;

	for (p = argv; *p != NULL; p++) {
	    Blt_List_Append(ids, (*tagProc)(viewPtr, *p), 0);
	}
	Blt_Free(argv);
    }
}

static void
GetTags(
    Blt_BindTable table,
    ClientData object,			/* Object picked. */
    ClientData context,			/* Context of object. */
    Blt_List ids)			/* (out) List of binding ids to be
					 * applied for this object. */
{
    TreeView *viewPtr;

    viewPtr = Blt_GetBindingData(table);
    if (context == (ClientData)ITEM_ENTRY_BUTTON) {
	TreeViewEntry *entryPtr = object;

	Blt_List_Append(ids, Blt_TreeView_ButtonTag(viewPtr, "Button"), 0);
	if (entryPtr->tagsUid != NULL) {
	    AddIdsToList(viewPtr, ids, entryPtr->tagsUid, Blt_TreeView_ButtonTag);
	} else {
	    Blt_List_Append(ids, Blt_TreeView_ButtonTag(viewPtr, "Entry"), 0);
	    Blt_List_Append(ids, Blt_TreeView_ButtonTag(viewPtr, "all"), 0);
	}
    } else if (context == (ClientData)ITEM_COLUMN_TITLE) {
	TreeViewColumn *cp = object;

	Blt_List_Append(ids, (char *)cp, 0);
	if (cp->tagsUid != NULL) {
	    AddIdsToList(viewPtr, ids, cp->tagsUid, 
			 Blt_TreeView_ColumnTag);
	}
    } else if (context == ITEM_COLUMN_RULE) {
	Blt_List_Append(ids, Blt_TreeView_ColumnTag(viewPtr, "Rule"), 0);
    } else {
	TreeViewEntry *entryPtr = object;

	Blt_List_Append(ids, (char *)entryPtr, 0);
	if (entryPtr->tagsUid != NULL) {
	    AddIdsToList(viewPtr, ids, entryPtr->tagsUid, Blt_TreeView_EntryTag);
	} else if (context == ITEM_ENTRY){
	    Blt_List_Append(ids, Blt_TreeView_EntryTag(viewPtr, "Entry"), 0);
	    Blt_List_Append(ids, Blt_TreeView_EntryTag(viewPtr, "all"), 0);
	} else {
	    TreeViewValue *valuePtr = context;

	    if (valuePtr != NULL) {
		TreeViewStyle *stylePtr = valuePtr->stylePtr;

		if (stylePtr == NULL) {
		    stylePtr = valuePtr->columnPtr->stylePtr;
		}
		Blt_List_Append(ids, 
	            Blt_TreeView_EntryTag(viewPtr, stylePtr->name), 0);
		Blt_List_Append(ids, 
		    Blt_TreeView_EntryTag(viewPtr, valuePtr->columnPtr->key), 0);
		Blt_List_Append(ids, 
		    Blt_TreeView_EntryTag(viewPtr, stylePtr->classPtr->className),
		    0);
#ifndef notdef
		Blt_List_Append(ids, Blt_TreeView_EntryTag(viewPtr, "Entry"), 0);
		Blt_List_Append(ids, Blt_TreeView_EntryTag(viewPtr, "all"), 0);
#endif
	    }
	}
    }
}

/*ARGSUSED*/
static ClientData
PickItem(
    ClientData clientData,
    int x, int y,			/* Screen coordinates of the test
					 * point. */
    ClientData *contextPtr)		/* (out) Context of item selected:
					 * should be ITEM_ENTRY,
					 * ITEM_ENTRY_BUTTON, ITEM_COLUMN_TITLE,
					 * ITEM_COLUMN_RULE, or ITEM_STYLE. */
{
    TreeView *viewPtr = clientData;
    TreeViewEntry *entryPtr;
    TreeViewColumn *cp;

    if (contextPtr != NULL) {
	*contextPtr = NULL;
    }
    if (viewPtr->flags & DIRTY) {
	/* Can't trust the selected entry if nodes have been added or
	 * deleted. So recompute the layout. */
	if (viewPtr->flags & LAYOUT_PENDING) {
	    Blt_TreeView_ComputeLayout(viewPtr);
	} 
	ComputeVisibleEntries(viewPtr);
    }
    cp = Blt_TreeView_NearestColumn(viewPtr, x, y, contextPtr);
    if ((*contextPtr != NULL) && (viewPtr->flags & TV_SHOW_COLUMN_TITLES)) {
	return cp;
    }
    if (viewPtr->nVisible == 0) {
	return NULL;
    }
    entryPtr = Blt_TreeView_NearestEntry(viewPtr, x, y, FALSE);
    if (entryPtr == NULL) {
	return NULL;
    }
    x = WORLDX(viewPtr, x);
    y = WORLDY(viewPtr, y);
    if (contextPtr != NULL) {
	*contextPtr = ITEM_ENTRY;
	if (cp != NULL) {
	    TreeViewValue *valuePtr;

	    valuePtr = Blt_TreeView_FindValue(entryPtr, cp);
	    if (valuePtr != NULL) {
		TreeViewStyle *stylePtr;
		
		stylePtr = valuePtr->stylePtr;
		if (stylePtr == NULL) {
		    stylePtr = valuePtr->columnPtr->stylePtr;
		}
		if ((stylePtr->classPtr->pickProc == NULL) ||
		    ((*stylePtr->classPtr->pickProc)(entryPtr, valuePtr, 
			stylePtr, x, y))) {
		    *contextPtr = valuePtr;
		} 
	    }
	}
	if (entryPtr->flags & ENTRY_HAS_BUTTON) {
	    TreeViewButton *buttonPtr = &viewPtr->button;
	    int left, right, top, bottom;
	    
	    left = entryPtr->worldX + entryPtr->buttonX - BUTTON_PAD;
	    right = left + buttonPtr->width + 2 * BUTTON_PAD;
	    top = entryPtr->worldY + entryPtr->buttonY - BUTTON_PAD;
	    bottom = top + buttonPtr->height + 2 * BUTTON_PAD;
	    if ((x >= left) && (x < right) && (y >= top) && (y < bottom)) {
		*contextPtr = (ClientData)ITEM_ENTRY_BUTTON;
	    }
	}
    }
    return entryPtr;
}

static void
GetEntryExtents(TreeView *viewPtr, TreeViewEntry *entryPtr)
{
    int entryWidth, entryHeight;
    int width, height;

    /*
     * FIXME: Use of DIRTY flag inconsistent.  When does it
     *	      mean "dirty entry"? When does it mean "dirty column"?
     *	      Does it matter? probably
     */
    if ((entryPtr->flags & ENTRY_DIRTY) || (viewPtr->flags & UPDATE)) {
	Blt_Font font;
	Blt_FontMetrics fontMetrics;
	TreeViewIcon *icons;
	const char *label;

	entryPtr->iconWidth = entryPtr->iconHeight = 0;
	icons = CHOOSE(viewPtr->icons, entryPtr->icons);
	if (icons != NULL) {
	    int i;
	    
	    for (i = 0; i < 2; i++) {
		if (icons[i] == NULL) {
		    break;
		}
		if (entryPtr->iconWidth < TreeView_IconWidth(icons[i])) {
		    entryPtr->iconWidth = TreeView_IconWidth(icons[i]);
		}
		if (entryPtr->iconHeight < TreeView_IconHeight(icons[i])) {
		    entryPtr->iconHeight = TreeView_IconHeight(icons[i]);
		}
	    }
	    entryPtr->iconWidth  += 2 * ICON_PADX;
	    entryPtr->iconHeight += 2 * ICON_PADY;
	} else if ((icons == NULL) || (icons[0] == NULL)) {
	    entryPtr->iconWidth = DEF_ICON_WIDTH;
	    entryPtr->iconHeight = DEF_ICON_HEIGHT;
	}
	entryHeight = MAX(entryPtr->iconHeight, viewPtr->button.height);
	font = entryPtr->font;
	if (font == NULL) {
	    font = Blt_TreeView_GetStyleFont(viewPtr, 
		viewPtr->treeColumn.stylePtr);
	}
	if (entryPtr->fullName != NULL) {
	    Blt_Free(entryPtr->fullName);
	    entryPtr->fullName = NULL;
	}
	if (entryPtr->textPtr != NULL) {
	    Blt_Free(entryPtr->textPtr);
	    entryPtr->textPtr = NULL;
	}
	
	Blt_GetFontMetrics(font, &fontMetrics);
	entryPtr->lineHeight = fontMetrics.linespace;
	entryPtr->lineHeight += 2 * (FOCUS_WIDTH + LABEL_PADY + 
				    viewPtr->selBW) + viewPtr->leader;

	label = GETLABEL(entryPtr);
	if (label[0] == '\0') {
	    width = height = entryPtr->lineHeight;
	} else {
	    TextStyle ts;

	    Blt_Ts_InitStyle(ts);
	    Blt_Ts_SetFont(ts, font);
	    if (viewPtr->flatView) {
		Tcl_DString dString;

		Blt_TreeView_GetFullName(viewPtr, entryPtr, TRUE, &dString);
		entryPtr->fullName = 
		    Blt_AssertStrdup(Tcl_DStringValue(&dString));
		Tcl_DStringFree(&dString);
		entryPtr->textPtr = Blt_Ts_CreateLayout(entryPtr->fullName, -1,
			&ts);
	    } else {
		entryPtr->textPtr = Blt_Ts_CreateLayout(label, -1, &ts);
	    }
	    width = entryPtr->textPtr->width;
	    height = entryPtr->textPtr->height;
	}
	width  += 2 * (FOCUS_WIDTH + LABEL_PADX + viewPtr->selBW);
	height += 2 * (FOCUS_WIDTH + LABEL_PADY + viewPtr->selBW);
	width = ODD(width);
	if (entryPtr->reqHeight > height) {
	    height = entryPtr->reqHeight;
	} 
	height = ODD(height);
	entryWidth = width;
	if (entryHeight < height) {
	    entryHeight = height;
	}
	entryPtr->labelWidth = width;
	entryPtr->labelHeight = height;
    } else {
	entryHeight = entryPtr->labelHeight;
	entryWidth = entryPtr->labelWidth;
    }
    entryHeight = MAX3(entryPtr->iconHeight, entryPtr->lineHeight, 
		       entryPtr->labelHeight);

    /*  
     * Find the maximum height of the data value entries. This also has the
     * side effect of contributing the maximum width of the column.
     */
    GetRowExtents(entryPtr, &width, &height);
    if (entryHeight < height) {
	entryHeight = height;
    }
    entryPtr->width = entryWidth + COLUMN_PAD;
    entryPtr->height = entryHeight + viewPtr->leader;

    /*
     * Force the height of the entry to an even number. This is to make the
     * dots or the vertical line segments coincide with the start of the
     * horizontal lines.
     */
    if (entryPtr->height & 0x01) {
	entryPtr->height++;
    }
    entryPtr->flags &= ~ENTRY_DIRTY;
}

/*
 * TreeView Procedures
 */

/*
 *---------------------------------------------------------------------------
 *
 * CreateTreeView --
 *
 *---------------------------------------------------------------------------
 */
static TreeView *
CreateTreeView(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    Tk_Window tkwin;
    TreeView *viewPtr;
    char *name;
    Tcl_DString dString;
    int result;

    name = Tcl_GetString(objPtr);
    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp), name,
	(char *)NULL);
    if (tkwin == NULL) {
	return NULL;

    }
    Tk_SetClass(tkwin, "TreeView");

    viewPtr = Blt_AssertCalloc(1, sizeof(TreeView));
    viewPtr->tkwin = tkwin;
    viewPtr->display = Tk_Display(tkwin);
    viewPtr->interp = interp;
    viewPtr->flags = (HIDE_ROOT | TV_SHOW_COLUMN_TITLES | DIRTY | 
		      LAYOUT_PENDING | REPOPULATE);
    viewPtr->leader = 0;
    viewPtr->dashes = 1;
    viewPtr->highlightWidth = 2;
    viewPtr->selBW = 1;
    viewPtr->borderWidth = 2;
    viewPtr->relief = TK_RELIEF_SUNKEN;
    viewPtr->selRelief = TK_RELIEF_FLAT;
    viewPtr->scrollMode = BLT_SCROLL_MODE_HIERBOX;
    viewPtr->selectMode = SELECT_MODE_SINGLE;
    viewPtr->button.closeRelief = viewPtr->button.openRelief = TK_RELIEF_SOLID;
    viewPtr->reqWidth = 0;
    viewPtr->reqHeight = 0;
    viewPtr->xScrollUnits = viewPtr->yScrollUnits = 20;
    viewPtr->lineWidth = 1;
    viewPtr->button.borderWidth = 1;
    viewPtr->columns = Blt_Chain_Create();
    viewPtr->buttonFlags = BUTTON_AUTO;
    viewPtr->selected = Blt_Chain_Create();
    viewPtr->userStyles = Blt_Chain_Create();
    viewPtr->sortColumnPtr = &viewPtr->treeColumn;
    Blt_InitHashTableWithPool(&viewPtr->entryTable, BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&viewPtr->columnTable, BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&viewPtr->iconTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&viewPtr->selectTable, BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&viewPtr->uidTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&viewPtr->styleTable, BLT_STRING_KEYS);
    viewPtr->bindTable = Blt_CreateBindingTable(interp, tkwin, viewPtr, PickItem, 
	GetTags);
    Blt_InitHashTable(&viewPtr->entryTagTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&viewPtr->columnTagTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&viewPtr->buttonTagTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&viewPtr->styleTagTable, BLT_STRING_KEYS);

    viewPtr->entryPool = Blt_PoolCreate(BLT_FIXED_SIZE_ITEMS);
    viewPtr->valuePool = Blt_PoolCreate(BLT_FIXED_SIZE_ITEMS);
    Blt_SetWindowInstanceData(tkwin, viewPtr);
    viewPtr->cmdToken = Tcl_CreateObjCommand(interp, Tk_PathName(viewPtr->tkwin), 
	Blt_TreeView_WidgetInstCmd, viewPtr, WidgetInstCmdDeleteProc);

    Tk_CreateSelHandler(viewPtr->tkwin, XA_PRIMARY, XA_STRING, SelectionProc,
	viewPtr, XA_STRING);
    Tk_CreateEventHandler(viewPtr->tkwin, ExposureMask | StructureNotifyMask |
	FocusChangeMask, TreeViewEventProc, viewPtr);
    /* 
     * Create a default style. This must exist before we can create the
     * treeview column.
     */  
    viewPtr->stylePtr = Blt_TreeView_CreateStyle(interp, viewPtr, STYLE_TEXTBOX,
	"text");
    if (viewPtr->stylePtr == NULL) {
	return NULL;
    }
    /*
     * By default create a tree. The name will be the same as the widget
     * pathname.
     */
    viewPtr->tree = Blt_Tree_Open(interp, Tk_PathName(viewPtr->tkwin), TREE_CREATE);
    if (viewPtr->tree == NULL) {
	return NULL;
    }
    /* Create a default column to display the view of the tree. */
    Tcl_DStringInit(&dString);
    Tcl_DStringAppend(&dString, "BLT TreeView ", -1);
    Tcl_DStringAppend(&dString, Tk_PathName(viewPtr->tkwin), -1);
    result = Blt_TreeView_CreateColumn(viewPtr, &viewPtr->treeColumn, 
				      Tcl_DStringValue(&dString), "");
    Tcl_DStringFree(&dString);
    if (result != TCL_OK) {
	return NULL;
    }
    Blt_Chain_Append(viewPtr->columns, &viewPtr->treeColumn);
    return viewPtr;
}

static void
TeardownEntries(TreeView *viewPtr)
{
    Blt_HashSearch iter;
    Blt_HashEntry *hPtr;

    /* Release the current tree, removing any entry fields. */
    for (hPtr = Blt_FirstHashEntry(&viewPtr->entryTable, &iter); hPtr != NULL; 
	 hPtr = Blt_NextHashEntry(&iter)) {
	TreeViewEntry *entryPtr;

	entryPtr = Blt_GetHashValue(hPtr);
	DestroyEntry((ClientData)entryPtr);
    }
    Blt_DeleteHashTable(&viewPtr->entryTable);
}


/*
 *---------------------------------------------------------------------------
 *
 * DestroyTreeView --
 *
 * 	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release to
 * 	clean up the internal structure of a TreeView at a safe time (when
 * 	no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the widget is freed up.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroyTreeView(DestroyData dataPtr)	/* Pointer to the widget record. */
{
    Blt_ChainLink link;
    TreeView *viewPtr = (TreeView *)dataPtr;
    TreeViewButton *buttonPtr;
    TreeViewStyle *stylePtr;

    TeardownEntries(viewPtr);
    if (viewPtr->tree != NULL) {
	Blt_Tree_Close(viewPtr->tree);
	viewPtr->tree = NULL;
    }
    bltTreeViewTreeOption.clientData = viewPtr;
    bltTreeViewIconsOption.clientData = viewPtr;
    Blt_FreeOptions(bltTreeViewSpecs, (char *)viewPtr, viewPtr->display, 0);
    if (viewPtr->tkwin != NULL) {
	Tk_DeleteSelHandler(viewPtr->tkwin, XA_PRIMARY, XA_STRING);
    }
    if (viewPtr->lineGC != NULL) {
	Tk_FreeGC(viewPtr->display, viewPtr->lineGC);
    }
    if (viewPtr->focusGC != NULL) {
	Blt_FreePrivateGC(viewPtr->display, viewPtr->focusGC);
    }
    if (viewPtr->selGC != NULL) {
	Blt_FreePrivateGC(viewPtr->display, viewPtr->selGC);
    }
    if (viewPtr->visibleArr != NULL) {
	Blt_Free(viewPtr->visibleArr);
    }
    if (viewPtr->flatArr != NULL) {
	Blt_Free(viewPtr->flatArr);
    }
    if (viewPtr->levelInfo != NULL) {
	Blt_Free(viewPtr->levelInfo);
    }
    buttonPtr = &viewPtr->button;
    if (buttonPtr->activeGC != NULL) {
	Tk_FreeGC(viewPtr->display, buttonPtr->activeGC);
    }
    if (buttonPtr->normalGC != NULL) {
	Tk_FreeGC(viewPtr->display, buttonPtr->normalGC);
    }
    if (viewPtr->stylePtr != NULL) {
	Blt_TreeView_FreeStyle(viewPtr, viewPtr->stylePtr);
    }
    Blt_TreeView_DestroyColumns(viewPtr);
    Blt_DestroyBindingTable(viewPtr->bindTable);
    Blt_Chain_Destroy(viewPtr->selected);
    Blt_DeleteHashTable(&viewPtr->entryTagTable);
    Blt_DeleteHashTable(&viewPtr->columnTagTable);
    Blt_DeleteHashTable(&viewPtr->buttonTagTable);
    Blt_DeleteHashTable(&viewPtr->styleTagTable);

    /* Remove any user-specified style that might remain. */
    for (link = Blt_Chain_FirstLink(viewPtr->userStyles); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	stylePtr = Blt_Chain_GetValue(link);
	stylePtr->link = NULL;
	Blt_TreeView_FreeStyle(viewPtr, stylePtr);
    }
    Blt_Chain_Destroy(viewPtr->userStyles);
    if (viewPtr->comboWin != NULL) {
	Tk_DestroyWindow(viewPtr->comboWin);
    }
    Blt_DeleteHashTable(&viewPtr->styleTable);
    Blt_DeleteHashTable(&viewPtr->selectTable);
    Blt_DeleteHashTable(&viewPtr->uidTable);
    Blt_PoolDestroy(viewPtr->entryPool);
    Blt_PoolDestroy(viewPtr->valuePool);
    DumpIconTable(viewPtr);
    Blt_Free(viewPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeViewEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various events on
 * 	treeview widgets.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get cleaned up.
 *	When it gets exposed, it is redisplayed.
 *
 *---------------------------------------------------------------------------
 */
static void
TreeViewEventProc(ClientData clientData, XEvent *eventPtr)
{
    TreeView *viewPtr = clientData;

    if (eventPtr->type == Expose) {
	if (eventPtr->xexpose.count == 0) {
	    Blt_TreeView_EventuallyRedraw(viewPtr);
	    Blt_PickCurrentItem(viewPtr->bindTable);
	}
    } else if (eventPtr->type == ConfigureNotify) {
	viewPtr->flags |= (LAYOUT_PENDING | SCROLL_PENDING);
	Blt_TreeView_EventuallyRedraw(viewPtr);
    } else if ((eventPtr->type == FocusIn) || (eventPtr->type == FocusOut)) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    if (eventPtr->type == FocusIn) {
		viewPtr->flags |= FOCUS;
	    } else {
		viewPtr->flags &= ~FOCUS;
	    }
	    Blt_TreeView_EventuallyRedraw(viewPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (viewPtr->tkwin != NULL) {
	    viewPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(viewPtr->interp, viewPtr->cmdToken);
	}
	if (viewPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayTreeView, viewPtr);
	}
	if (viewPtr->flags & TV_SELECT_PENDING) {
	    Tcl_CancelIdleCall(Blt_TreeView_SelectCmdProc, viewPtr);
	}
	Tcl_EventuallyFree(viewPtr, DestroyTreeView);
    }
}

/* Selection Procedures */
/*
 *---------------------------------------------------------------------------
 *
 * SelectionProc --
 *
 *	This procedure is called back by Tk when the selection is requested by
 *	someone.  It returns part or all of the selection in a buffer provided
 *	by the caller.
 *
 * Results:
 *	The return value is the number of non-NULL bytes stored at buffer.
 *	Buffer is filled (or partially filled) with a NUL-terminated string
 *	containing part or all of the selection, as given by offset and
 *	maxBytes.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static int
SelectionProc(
    ClientData clientData,		/* Information about the widget. */
    int offset,				/* Offset within selection of first
					 * character to be returned. */
    char *buffer,			/* Location in which to place
					 * selection. */
    int maxBytes)			/* Maximum number of bytes to place at
					 * buffer, not including terminating
					 * NULL character. */
{
    Tcl_DString dString;
    TreeView *viewPtr = clientData;
    TreeViewEntry *entryPtr;
    int size;

    if ((viewPtr->flags & TV_SELECT_EXPORT) == 0) {
	return -1;
    }
    /*
     * Retrieve the names of the selected entries.
     */
    Tcl_DStringInit(&dString);
    if (viewPtr->flags & TV_SELECT_SORTED) {
	Blt_ChainLink link;

	for (link = Blt_Chain_FirstLink(viewPtr->selected); 
	     link != NULL; link = Blt_Chain_NextLink(link)) {
	    entryPtr = Blt_Chain_GetValue(link);
	    Tcl_DStringAppend(&dString, GETLABEL(entryPtr), -1);
	    Tcl_DStringAppend(&dString, "\n", -1);
	}
    } else {
	for (entryPtr = viewPtr->rootPtr; entryPtr != NULL; 
	     entryPtr = Blt_TreeView_NextEntry(entryPtr, ENTRY_MASK)) {
	    if (Blt_TreeView_EntryIsSelected(viewPtr, entryPtr)) {
		Tcl_DStringAppend(&dString, GETLABEL(entryPtr), -1);
		Tcl_DStringAppend(&dString, "\n", -1);
	    }
	}
    }
    size = Tcl_DStringLength(&dString) - offset;
    strncpy(buffer, Tcl_DStringValue(&dString) + offset, maxBytes);
    Tcl_DStringFree(&dString);
    buffer[maxBytes] = '\0';
    return (size > maxBytes) ? maxBytes : size;
}

/*
 *---------------------------------------------------------------------------
 *
 * WidgetInstCmdDeleteProc --
 *
 *	This procedure is invoked when a widget command is deleted.  If the
 *	widget isn't already in the process of being destroyed, this command
 *	destroys it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget is destroyed.
 *
 *---------------------------------------------------------------------------
 */
static void
WidgetInstCmdDeleteProc(ClientData clientData)
{
    TreeView *viewPtr = clientData;

    /*
     * This procedure could be invoked either because the window was destroyed
     * and the command was then deleted (in which case tkwin is NULL) or
     * because the command was deleted, and then this procedure destroys the
     * widget.
     */
    if (viewPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = viewPtr->tkwin;
	viewPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_UpdateWidget --
 *
 *	Updates the GCs and other information associated with the treeview
 *	widget.
 *
 * Results:
 *	The return value is a standard TCL result.  If TCL_ERROR is returned,
 *	then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font, etc. get
 *	set for viewPtr; old resources get freed, if there were any.  The widget
 *	is redisplayed.
 *
 *---------------------------------------------------------------------------
 */
int
Blt_TreeView_UpdateWidget(Tcl_Interp *interp, TreeView *viewPtr)	
{
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;

    /*
     * GC for dotted vertical line.
     */
    gcMask = (GCForeground | GCLineWidth);
    gcValues.foreground = viewPtr->lineColor->pixel;
    gcValues.line_width = viewPtr->lineWidth;
    if (viewPtr->dashes > 0) {
	gcMask |= (GCLineStyle | GCDashList);
	gcValues.line_style = LineOnOffDash;
	gcValues.dashes = viewPtr->dashes;
    }
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (viewPtr->lineGC != NULL) {
	Tk_FreeGC(viewPtr->display, viewPtr->lineGC);
    }
    viewPtr->lineGC = newGC;

    /*
     * GC for active label. Dashed outline.
     */
    gcMask = GCForeground | GCLineStyle;
    gcValues.foreground = viewPtr->focusColor->pixel;
    gcValues.line_style = (LineIsDashed(viewPtr->focusDashes))
	? LineOnOffDash : LineSolid;
    newGC = Blt_GetPrivateGC(viewPtr->tkwin, gcMask, &gcValues);
    if (LineIsDashed(viewPtr->focusDashes)) {
	viewPtr->focusDashes.offset = 2;
	Blt_SetDashes(viewPtr->display, newGC, &viewPtr->focusDashes);
    }
    if (viewPtr->focusGC != NULL) {
	Blt_FreePrivateGC(viewPtr->display, viewPtr->focusGC);
    }
    viewPtr->focusGC = newGC;

    /*
     * GC for selection. Dashed outline.
     */
    gcMask = GCForeground | GCLineStyle;
    gcValues.foreground = viewPtr->selFgColor->pixel;
    gcValues.line_style = (LineIsDashed(viewPtr->focusDashes))
	? LineOnOffDash : LineSolid;
    newGC = Blt_GetPrivateGC(viewPtr->tkwin, gcMask, &gcValues);
    if (LineIsDashed(viewPtr->focusDashes)) {
	viewPtr->focusDashes.offset = 2;
	Blt_SetDashes(viewPtr->display, newGC, &viewPtr->focusDashes);
    }
    if (viewPtr->selGC != NULL) {
	Blt_FreePrivateGC(viewPtr->display, viewPtr->selGC);
    }
    viewPtr->selGC = newGC;

    Blt_TreeView_ConfigureButtons(viewPtr);
    viewPtr->inset = viewPtr->highlightWidth + viewPtr->borderWidth + INSET_PAD;

    /*
     * If the tree object was changed, we need to setup the new one.
     */
    if (Blt_ConfigModified(bltTreeViewSpecs, "-tree", (char *)NULL)) {
	TeardownEntries(viewPtr);
	Blt_InitHashTableWithPool(&viewPtr->entryTable, BLT_ONE_WORD_KEYS);
	Blt_TreeView_ClearSelection(viewPtr);
	if (Blt_Tree_Attach(interp, viewPtr->tree, viewPtr->treeName) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	viewPtr->flags |= REPOPULATE;
    }

    /*
     * These options change the layout of the box.  Mark the widget for update.
     */
    if (Blt_ConfigModified(bltTreeViewSpecs, "-font", 
	"-linespacing", "-*width", "-height", "-hide*", "-tree", "-flat", 
	(char *)NULL)) {
	viewPtr->flags |= (LAYOUT_PENDING | SCROLL_PENDING);
    }
    /*
     * If the tree view was changed, mark all the nodes dirty (we'll be
     * switching back to either the full path name or the label) and free the
     * array representing the flattened view of the tree.
     */
    if (Blt_ConfigModified(bltTreeViewSpecs, "-hideleaves", "-flat",
			      (char *)NULL)) {
	TreeViewEntry *entryPtr;
	
	viewPtr->flags |= DIRTY;
	/* Mark all entries dirty. */
	for (entryPtr = viewPtr->rootPtr; entryPtr != NULL; 
	     entryPtr = Blt_TreeView_NextEntry(entryPtr, 0)) {
	    entryPtr->flags |= ENTRY_DIRTY;
	}
	if ((!viewPtr->flatView) && (viewPtr->flatArr != NULL)) {
	    Blt_Free(viewPtr->flatArr);
	    viewPtr->flatArr = NULL;
	}
    }

    if (viewPtr->flags & REPOPULATE) {
	Blt_TreeNode root;

	Blt_Tree_CreateEventHandler(viewPtr->tree, TREE_NOTIFY_ALL, 
		TreeEventProc, viewPtr);
	TraceColumns(viewPtr);
	root = Blt_Tree_RootNode(viewPtr->tree);

	/* Automatically add view-entry values to the new tree. */
	Blt_Tree_Apply(root, CreateApplyProc, viewPtr);
	viewPtr->focusPtr = viewPtr->rootPtr = 
	    Blt_TreeView_NodeToEntry(viewPtr,root);
	viewPtr->selMarkPtr = viewPtr->selAnchorPtr = NULL;
	Blt_SetFocusItem(viewPtr->bindTable, viewPtr->rootPtr, ITEM_ENTRY);

	/* Automatically open the root node. */
	if (Blt_TreeView_OpenEntry(viewPtr, viewPtr->rootPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (viewPtr->flags & TV_NEW_TAGS) {
	    Blt_Tree_NewTagTable(viewPtr->tree);
	}
	viewPtr->flags &= ~REPOPULATE;
    }

    if (Blt_ConfigModified(bltTreeViewSpecs, "-font", "-color", 
	(char *)NULL)) {
	Blt_TreeView_ConfigureColumn(viewPtr, &viewPtr->treeColumn);
    }
    Blt_TreeView_EventuallyRedraw(viewPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ResetCoordinates --
 *
 *	Determines the maximum height of all visible entries.
 *
 *	1. Sets the worldY coordinate for all mapped/open entries.
 *	2. Determines if entry needs a button.
 *	3. Collects the minimum height of open/mapped entries. (Do for all
 *	   entries upon insert).
 *	4. Figures out horizontal extent of each entry (will be width of 
 *	   tree view column).
 *	5. Collects maximum icon size for each level.
 *	6. The height of its vertical line
 *
 * Results:
 *	Returns 1 if beyond the last visible entry, 0 otherwise.
 *
 * Side effects:
 *	The array of visible nodes is filled.
 *
 *---------------------------------------------------------------------------
 */
static void
ResetCoordinates(TreeView *viewPtr, TreeViewEntry *entryPtr, int *yPtr, 
		 int *indexPtr)
{
    int depth, height;

    entryPtr->worldY = -1;
    entryPtr->vertLineLength = -1;
    if ((entryPtr != viewPtr->rootPtr) && 
	(Blt_TreeView_EntryIsHidden(entryPtr))) {
	return;				/* If the entry is hidden, then do
					 * nothing. */
    }
    entryPtr->worldY = *yPtr;
    height = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
		  viewPtr->button.height);
    entryPtr->vertLineLength = -(*yPtr + height / 2);
    *yPtr += entryPtr->height;
    entryPtr->flatIndex = *indexPtr;
    (*indexPtr)++;
    depth = DEPTH(viewPtr, entryPtr->node) + 1;
    if (viewPtr->levelInfo[depth].labelWidth < entryPtr->labelWidth) {
	viewPtr->levelInfo[depth].labelWidth = entryPtr->labelWidth;
    }
    if (viewPtr->levelInfo[depth].iconWidth < entryPtr->iconWidth) {
	viewPtr->levelInfo[depth].iconWidth = entryPtr->iconWidth;
    }
    viewPtr->levelInfo[depth].iconWidth |= 0x01;
    if ((entryPtr->flags & ENTRY_CLOSED) == 0) {
	TreeViewEntry *bottomPtr, *childPtr;

	bottomPtr = entryPtr;
	for (childPtr = Blt_TreeView_FirstChild(entryPtr, ENTRY_HIDE); 
	     childPtr != NULL; 
	     childPtr = Blt_TreeView_NextSibling(childPtr, ENTRY_HIDE)){
	    ResetCoordinates(viewPtr, childPtr, yPtr, indexPtr);
	    bottomPtr = childPtr;
	}
	height = MAX3(bottomPtr->lineHeight, bottomPtr->iconHeight, 
		      viewPtr->button.height);
	entryPtr->vertLineLength += bottomPtr->worldY + height / 2;
    }
}

static void
AdjustColumns(TreeView *viewPtr)
{
    Blt_ChainLink link;
    TreeViewColumn *lastPtr;
    double weight;
    int growth;
    int nOpen;

    growth = VPORTWIDTH(viewPtr) - viewPtr->worldWidth;
    lastPtr = NULL;
    nOpen = 0;
    weight = 0.0;
    /* Find out how many columns still have space available */
    for (link = Blt_Chain_FirstLink(viewPtr->columns); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	TreeViewColumn *cp;

	cp = Blt_Chain_GetValue(link);
	if (cp->flags & COLUMN_HIDDEN) {
	    continue;
	}
	lastPtr = cp;
	if ((cp->weight == 0.0) || (cp->width >= cp->max) || 
	    (cp->reqWidth > 0)) {
	    continue;
	}
	nOpen++;
	weight += cp->weight;
    }

    while ((nOpen > 0) && (weight > 0.0) && (growth > 0)) {
	int ration;

	ration = (int)(growth / weight);
	if (ration == 0) {
	    ration = 1;
	}
	for (link = Blt_Chain_FirstLink(viewPtr->columns); 
	     link != NULL; link = Blt_Chain_NextLink(link)) {
	    TreeViewColumn *cp;
	    int size, avail;

	    cp = Blt_Chain_GetValue(link);
	    if (cp->flags & COLUMN_HIDDEN) {
		continue;
	    }
	    lastPtr = cp;
	    if ((cp->weight == 0.0) || (cp->width >= cp->max) || 
		(cp->reqWidth > 0)) {
		continue;
	    }
	    size = (int)(ration * cp->weight);
	    if (size > growth) {
		size = growth; 
	    }
	    avail = cp->max - cp->width;
	    if (size > avail) {
		size = avail;
		nOpen--;
		weight -= cp->weight;
	    }
	    cp->width += size;
	    growth -= size;
	    ration -= size;
	}
    }
    if ((growth > 0) && (lastPtr != NULL)) {
	lastPtr->width += growth;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ComputeFlatLayout --
 *
 *	Recompute the layout when entries are opened/closed, inserted/deleted,
 *	or when text attributes change (such as font, linespacing).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The world coordinates are set for all the opened entries.
 *
 *---------------------------------------------------------------------------
 */
static void
ComputeFlatLayout(TreeView *viewPtr)
{
    Blt_ChainLink link;
    TreeViewColumn *cp;
    TreeViewEntry **p;
    TreeViewEntry *entryPtr;
    int count;
    int maxX;
    int y;

    /* 
     * Pass 1:	Reinitialize column sizes and loop through all nodes. 
     *
     *		1. Recalculate the size of each entry as needed. 
     *		2. The maximum depth of the tree. 
     *		3. Minimum height of an entry.  Dividing this by the
     *		   height of the widget gives a rough estimate of the 
     *		   maximum number of visible entries.
     *		4. Build an array to hold level information to be filled
     *		   in on pass 2.
     */
    if (viewPtr->flags & (DIRTY | UPDATE)) {
	int position;

	/* Reset the positions of all the columns and initialize the column used
	 * to track the widest value. */
	position = 1;
	for (link = Blt_Chain_FirstLink(viewPtr->columns); 
	     link != NULL; link = Blt_Chain_NextLink(link)) {
	    cp = Blt_Chain_GetValue(link);
	    cp->maxWidth = 0;
	    cp->max = SHRT_MAX;
	    if (cp->reqMax > 0) {
		cp->max = cp->reqMax;
	    }
	    cp->position = position;
	    position++;
	}

	/* If the view needs to be resorted, free the old view. */
	if ((viewPtr->flags & (DIRTY|RESORT|SORT_PENDING|TV_SORT_AUTO)) && 
	     (viewPtr->flatArr != NULL)) {
	    Blt_Free(viewPtr->flatArr);
	    viewPtr->flatArr = NULL;
	}

	/* Recreate the flat view of all the open and not-hidden entries. */
	if (viewPtr->flatArr == NULL) {
	    count = 0;
	    /* Count the number of open entries to allocate for the array. */
	    for (entryPtr = viewPtr->rootPtr; entryPtr != NULL; 
		entryPtr = Blt_TreeView_NextEntry(entryPtr, ENTRY_MASK)) {
		if ((viewPtr->flags & HIDE_ROOT) && 
		    (entryPtr == viewPtr->rootPtr)) {
		    continue;
		}
		count++;
	    }
	    viewPtr->nEntries = count;

	    /* Allocate an array for the flat view. */
	    viewPtr->flatArr = Blt_AssertCalloc((count + 1), 
		sizeof(TreeViewEntry *));
	    /* Fill the array with open and not-hidden entries */
	    p = viewPtr->flatArr;
	    for (entryPtr = viewPtr->rootPtr; entryPtr != NULL; 
		entryPtr = Blt_TreeView_NextEntry(entryPtr, ENTRY_MASK)) {
		if ((viewPtr->flags & HIDE_ROOT) && 
		    (entryPtr == viewPtr->rootPtr)) {
		    continue;
		}
		*p++ = entryPtr;
	    }
	    *p = NULL;
	    viewPtr->flags &= ~SORTED;		/* Indicate the view isn't
						 * sorted. */
	}

	/* Collect the extents of the entries in the flat view. */
	viewPtr->depth = 0;
	viewPtr->minHeight = SHRT_MAX;
	for (p = viewPtr->flatArr; *p != NULL; p++) {
	    entryPtr = *p;
	    GetEntryExtents(viewPtr, entryPtr);
	    if (viewPtr->minHeight > entryPtr->height) {
		viewPtr->minHeight = entryPtr->height;
	    }
	    entryPtr->flags &= ~ENTRY_HAS_BUTTON;
	}
	if (viewPtr->levelInfo != NULL) {
	    Blt_Free(viewPtr->levelInfo);
	}
	viewPtr->levelInfo = 
	    Blt_AssertCalloc(viewPtr->depth+2, sizeof(LevelInfo));
	viewPtr->flags &= ~(DIRTY | UPDATE | RESORT);
	if (viewPtr->flags & TV_SORT_AUTO) {
	    /* If we're auto-sorting, schedule the view to be resorted. */
	    viewPtr->flags |= SORT_PENDING;
	}
    } 

    if (viewPtr->flags & SORT_PENDING) {
	Blt_TreeView_SortFlatView(viewPtr);
    }

    viewPtr->levelInfo[0].labelWidth = viewPtr->levelInfo[0].x = 
	    viewPtr->levelInfo[0].iconWidth = 0;
    /* 
     * Pass 2:	Loop through all open/mapped nodes. 
     *
     *		1. Set world y-coordinates for entries. We must defer
     *		   setting the x-coordinates until we know the maximum 
     *		   icon sizes at each level.
     *		2. Compute the maximum depth of the tree. 
     *		3. Build an array to hold level information.
     */
    y = 0;			
    count = 0;
    for(p = viewPtr->flatArr; *p != NULL; p++) {
	entryPtr = *p;
	entryPtr->flatIndex = count++;
	entryPtr->worldY = y;
	entryPtr->vertLineLength = 0;
	y += entryPtr->height;
	if (viewPtr->levelInfo[0].labelWidth < entryPtr->labelWidth) {
	    viewPtr->levelInfo[0].labelWidth = entryPtr->labelWidth;
	}
	if (viewPtr->levelInfo[0].iconWidth < entryPtr->iconWidth) {
	    viewPtr->levelInfo[0].iconWidth = entryPtr->iconWidth;
	}
    }
    viewPtr->levelInfo[0].iconWidth |= 0x01;
    viewPtr->worldHeight = y;		/* Set the scroll height of the
					 * hierarchy. */
    if (viewPtr->worldHeight < 1) {
	viewPtr->worldHeight = 1;
    }
    maxX = viewPtr->levelInfo[0].iconWidth + viewPtr->levelInfo[0].labelWidth;
    viewPtr->treeColumn.maxWidth = maxX;
    viewPtr->treeWidth = maxX;
    viewPtr->flags |= VIEWPORT;
}

/*
 *---------------------------------------------------------------------------
 *
 * ComputeTreeLayout --
 *
 *	Recompute the layout when entries are opened/closed, inserted/deleted,
 *	or when text attributes change (such as font, linespacing).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The world coordinates are set for all the opened entries.
 *
 *---------------------------------------------------------------------------
 */
static void
ComputeTreeLayout(TreeView *viewPtr)
{
    int y;
    int index;
    /* 
     * Pass 1:	Reinitialize column sizes and loop through all nodes. 
     *
     *		1. Recalculate the size of each entry as needed. 
     *		2. The maximum depth of the tree. 
     *		3. Minimum height of an entry.  Dividing this by the
     *		   height of the widget gives a rough estimate of the 
     *		   maximum number of visible entries.
     *		4. Build an array to hold level information to be filled
     *		   in on pass 2.
     */
    if (viewPtr->flags & DIRTY) {
	Blt_ChainLink link;
	TreeViewEntry *ep;
	int position;

	position = 1;
	for (link = Blt_Chain_FirstLink(viewPtr->columns); 
	     link != NULL; link = Blt_Chain_NextLink(link)) {
	    TreeViewColumn *cp;

	    cp = Blt_Chain_GetValue(link);
	    cp->maxWidth = 0;
	    cp->max = SHRT_MAX;
	    if (cp->reqMax > 0) {
		cp->max = cp->reqMax;
	    }
	    cp->position = position;
	    position++;
	}
	viewPtr->minHeight = SHRT_MAX;
	viewPtr->depth = 0;
	for (ep = viewPtr->rootPtr; ep != NULL; 
	     ep = Blt_TreeView_NextEntry(ep, 0)){
	    GetEntryExtents(viewPtr, ep);
	    if (viewPtr->minHeight > ep->height) {
		viewPtr->minHeight = ep->height;
	    }
	    /* 
	     * Determine if the entry should display a button (indicating that
	     * it has children) and mark the entry accordingly.
	     */
	    ep->flags &= ~ENTRY_HAS_BUTTON;
	    if (ep->flags & BUTTON_SHOW) {
		ep->flags |= ENTRY_HAS_BUTTON;
	    } else if (ep->flags & BUTTON_AUTO) {
		if (Blt_TreeView_FirstChild(ep, ENTRY_HIDE) != NULL) {
		    ep->flags |= ENTRY_HAS_BUTTON;
		}
	    }
	    /* Determine the depth of the tree. */
	    if (viewPtr->depth < DEPTH(viewPtr, ep->node)) {
		viewPtr->depth = DEPTH(viewPtr, ep->node);
	    }
	}
	if (viewPtr->flags & SORT_PENDING) {
	    Blt_TreeView_SortView(viewPtr);
	}
	if (viewPtr->levelInfo != NULL) {
	    Blt_Free(viewPtr->levelInfo);
	}
	viewPtr->levelInfo = Blt_AssertCalloc(viewPtr->depth+2, 
					      sizeof(LevelInfo));
	viewPtr->flags &= ~(DIRTY | RESORT);
    }
    { 
	size_t i;

	for (i = 0; i <= (viewPtr->depth + 1); i++) {
	    viewPtr->levelInfo[i].labelWidth = viewPtr->levelInfo[i].x = 
		viewPtr->levelInfo[i].iconWidth = 0;
	}
    }
    /* 
     * Pass 2:	Loop through all open/mapped nodes. 
     *
     *		1. Set world y-coordinates for entries. We must defer
     *		   setting the x-coordinates until we know the maximum 
     *		   icon sizes at each level.
     *		2. Compute the maximum depth of the tree. 
     *		3. Build an array to hold level information.
     */
    y = 0;
    if (viewPtr->flags & HIDE_ROOT) {
	/* If the root entry is to be hidden, cheat by offsetting the
	 * y-coordinates by the height of the entry. */
	y = -(viewPtr->rootPtr->height);
    } 
    index = 0;
    ResetCoordinates(viewPtr, viewPtr->rootPtr, &y, &index);
    viewPtr->worldHeight = y;		/* Set the scroll height of the
					 * hierarchy. */
    if (viewPtr->worldHeight < 1) {
	viewPtr->worldHeight = 1;
    }
    {
	int maxX;
	int sum;
	size_t i;

	sum = maxX = 0;
	i = 0;
	for (/*empty*/; i <= (viewPtr->depth + 1); i++) {
	    int x;

	    sum += viewPtr->levelInfo[i].iconWidth;
	    if (i <= viewPtr->depth) {
		viewPtr->levelInfo[i + 1].x = sum;
	    }
	    x = sum;
	    if (((viewPtr->flags & HIDE_ROOT) == 0) || (i > 1)) {
		x += viewPtr->levelInfo[i].labelWidth;
	    }
	    if (x > maxX) {
		maxX = x;
	    }
	}
	viewPtr->treeColumn.maxWidth = maxX;
	viewPtr->treeWidth = maxX;
    }
}

static void
LayoutColumns(TreeView *viewPtr)
{
    Blt_ChainLink link;
    int sum;

    /* The width of the widget (in world coordinates) is the sum of the column
     * widths. */

    viewPtr->worldWidth = viewPtr->titleHeight = 0;
    sum = 0;
    for (link = Blt_Chain_FirstLink(viewPtr->columns); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	TreeViewColumn *cp;

	cp = Blt_Chain_GetValue(link);
	cp->width = 0;
	if (cp->flags & COLUMN_HIDDEN) {
	    continue;
	}
	if ((viewPtr->flags & TV_SHOW_COLUMN_TITLES) &&
	    (viewPtr->titleHeight < cp->titleHeight)) {
	    viewPtr->titleHeight = cp->titleHeight;
	}
	if (cp->reqWidth > 0) {
	    cp->width = cp->reqWidth;
	} else {
	    /* The computed width of a column is the maximum of the title
	     * width and the widest entry. */
	    cp->width = MAX(cp->titleWidth, cp->maxWidth);
	    
	    /* Check that the width stays within any constraints that have
	     * been set. */
	    if ((cp->reqMin > 0) && (cp->reqMin > cp->width)) {
		cp->width = cp->reqMin;
	    }
	    if ((cp->reqMax > 0) && (cp->reqMax < cp->width)) {
		cp->width = cp->reqMax;
	    }
	}
	cp->width += PADDING(cp->pad) + 2 * cp->borderWidth;
	cp->worldX = sum;
	sum += cp->width;
    }
    viewPtr->worldWidth = sum;
    if (VPORTWIDTH(viewPtr) > sum) {
	AdjustColumns(viewPtr);
    }
    sum = 0;
    for (link = Blt_Chain_FirstLink(viewPtr->columns); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	TreeViewColumn *cp;

	cp = Blt_Chain_GetValue(link);
	cp->worldX = sum;
	sum += cp->width;
    }
    if (viewPtr->titleHeight > 0) {
	/* If any headings are displayed, add some extra padding to the
	 * height. */
	viewPtr->titleHeight += 4;
    }
    /* viewPtr->worldWidth += 10; */
    if (viewPtr->yScrollUnits < 1) {
	viewPtr->yScrollUnits = 1;
    }
    if (viewPtr->xScrollUnits < 1) {
	viewPtr->xScrollUnits = 1;
    }
    if (viewPtr->worldWidth < 1) {
	viewPtr->worldWidth = 1;
    }
    viewPtr->flags &= ~LAYOUT_PENDING;
    viewPtr->flags |= SCROLL_PENDING;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_ComputeLayout --
 *
 *	Recompute the layout when entries are opened/closed, inserted/deleted,
 *	or when text attributes change (such as font, linespacing).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The world coordinates are set for all the opened entries.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_TreeView_ComputeLayout(TreeView *viewPtr)
{
    Blt_ChainLink link;
    TreeViewColumn *cp;
    TreeViewEntry *entryPtr;
    TreeViewValue *valuePtr;

    if (viewPtr->flatView) {
	ComputeFlatLayout(viewPtr);
    } else {
	ComputeTreeLayout(viewPtr);
    }

    /*
     * Determine the width of each column based upon the entries that as open
     * (not hidden).  The widest entry in a column determines the width of that
     * column.
     */
    /* Initialize the columns. */
    for (link = Blt_Chain_FirstLink(viewPtr->columns); 
	 link != NULL; link = Blt_Chain_NextLink(link)) {
	cp = Blt_Chain_GetValue(link);
	cp->maxWidth = 0;
	cp->max = SHRT_MAX;
	if (cp->reqMax > 0) {
	    cp->max = cp->reqMax;
	}
    }
    /* The treeview column width was computed earlier. */
    viewPtr->treeColumn.maxWidth = viewPtr->treeWidth;

    /* 
     * Look at all open entries and their values.  Determine the column widths
     * by tracking the maximum width value in each column.
     */
    for (entryPtr = viewPtr->rootPtr; entryPtr != NULL; 
	 entryPtr = Blt_TreeView_NextEntry(entryPtr, ENTRY_MASK)) {
	for (valuePtr = entryPtr->values; valuePtr != NULL; 
	     valuePtr = valuePtr->nextPtr) {
	    if (valuePtr->columnPtr->maxWidth < valuePtr->width) {
		valuePtr->columnPtr->maxWidth = valuePtr->width;
	    }
	}	    
    }
    /* Now layout the columns with the proper sizes. */
    LayoutColumns(viewPtr);
}

#ifdef notdef
static void
PrintFlags(TreeView *viewPtr, char *string)
{    
    fprintf(stderr, "%s: flags=", string);
    if (viewPtr->flags & LAYOUT_PENDING) {
	fprintf(stderr, "layout ");
    }
    if (viewPtr->flags & REDRAW_PENDING) {
	fprintf(stderr, "redraw ");
    }
    if (viewPtr->flags & SCROLLX) {
	fprintf(stderr, "xscroll ");
    }
    if (viewPtr->flags & SCROLLY) {
	fprintf(stderr, "yscroll ");
    }
    if (viewPtr->flags & FOCUS) {
	fprintf(stderr, "focus ");
    }
    if (viewPtr->flags & DIRTY) {
	fprintf(stderr, "dirty ");
    }
    if (viewPtr->flags & UPDATE) {
	fprintf(stderr, "update ");
    }
    if (viewPtr->flags & RESORT) {
	fprintf(stderr, "resort ");
    }
    if (viewPtr->flags & SORTED) {
	fprintf(stderr, "sorted ");
    }
    if (viewPtr->flags & SORT_PENDING) {
	fprintf(stderr, "sort_pending ");
    }
    if (viewPtr->flags & REDRAW_BORDERS) {
	fprintf(stderr, "borders ");
    }
    if (viewPtr->flags & VIEWPORT) {
	fprintf(stderr, "viewport ");
    }
    fprintf(stderr, "\n");
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * ComputeVisibleEntries --
 *
 *	The entries visible in the viewport (the widget's window) are inserted
 *	into the array of visible nodes.
 *
 * Results:
 *	Returns 1 if beyond the last visible entry, 0 otherwise.
 *
 * Side effects:
 *	The array of visible nodes is filled.
 *
 *---------------------------------------------------------------------------
 */
static int
ComputeVisibleEntries(TreeView *viewPtr)
{
    int height;
    int nSlots;
    int maxX;
    int xOffset, yOffset;

    xOffset = Blt_AdjustViewport(viewPtr->xOffset, viewPtr->worldWidth,
	VPORTWIDTH(viewPtr), viewPtr->xScrollUnits, viewPtr->scrollMode);
    yOffset = Blt_AdjustViewport(viewPtr->yOffset, 
	viewPtr->worldHeight, VPORTHEIGHT(viewPtr), viewPtr->yScrollUnits, 
	viewPtr->scrollMode);

    if ((xOffset != viewPtr->xOffset) || (yOffset != viewPtr->yOffset)) {
	viewPtr->yOffset = yOffset;
	viewPtr->xOffset = xOffset;
	viewPtr->flags |= VIEWPORT;
    }
    height = VPORTHEIGHT(viewPtr);

    /* Allocate worst case number of slots for entry array. */
    nSlots = (height / viewPtr->minHeight) + 3;
    if (nSlots != viewPtr->nVisible) {
	if (viewPtr->visibleArr != NULL) {
	    Blt_Free(viewPtr->visibleArr);
	}
	viewPtr->visibleArr = Blt_AssertCalloc(nSlots + 1, 
					       sizeof(TreeViewEntry *));
    }
    viewPtr->nVisible = 0;
    viewPtr->visibleArr[nSlots] = viewPtr->visibleArr[0] = NULL;

    if (viewPtr->rootPtr->flags & ENTRY_HIDE) {
	return TCL_OK;			/* Root node is hidden. */
    }
    /* Find the node where the view port starts. */
    if (viewPtr->flatView) {
	TreeViewEntry **epp;

	/* Find the starting entry visible in the viewport. It can't be hidden
	 * or any of it's ancestors closed. */
    again:
	for (epp = viewPtr->flatArr; *epp != NULL; epp++) {
	    if (((*epp)->worldY + (*epp)->height) > viewPtr->yOffset) {
		break;
	    }
	}	    
	/*
	 * If we can't find the starting node, then the view must be scrolled
	 * down, but some nodes were deleted.  Reset the view back to the top
	 * and try again.
	 */
	if (*epp == NULL) {
	    if (viewPtr->yOffset == 0) {
		return TCL_OK;		/* All entries are hidden. */
	    }
	    viewPtr->yOffset = 0;
	    goto again;
	}

	maxX = 0;
	height += viewPtr->yOffset;
	for (/*empty*/; *epp != NULL; epp++) {
	    int x;

	    (*epp)->worldX = LEVELX(0) + viewPtr->treeColumn.worldX;
	    x = (*epp)->worldX + ICONWIDTH(0) + (*epp)->width;
	    if (x > maxX) {
		maxX = x;
	    }
	    if ((*epp)->worldY >= height) {
		break;
	    }
	    viewPtr->visibleArr[viewPtr->nVisible] = *epp;
	    viewPtr->nVisible++;
	}
	viewPtr->visibleArr[viewPtr->nVisible] = NULL;
    } else {
	TreeViewEntry *ep;

	ep = viewPtr->rootPtr;
	while ((ep->worldY + ep->height) <= viewPtr->yOffset) {
	    for (ep = Blt_TreeView_LastChild(ep, ENTRY_HIDE); ep != NULL; 
		 ep = Blt_TreeView_PrevSibling(ep, ENTRY_HIDE)) {
		if (ep->worldY <= viewPtr->yOffset) {
		    break;
		}
	    }
	    /*
	     * If we can't find the starting node, then the view must be
	     * scrolled down, but some nodes were deleted.  Reset the view
	     * back to the top and try again.
	     */
	    if (ep == NULL) {
		if (viewPtr->yOffset == 0) {
		    return TCL_OK;	/* All entries are hidden. */
		}
		viewPtr->yOffset = 0;
		continue;
	    }
	}
	
	height += viewPtr->yOffset;
	maxX = 0;
	viewPtr->treeColumn.maxWidth = viewPtr->treeWidth;

	for (; ep != NULL; ep = Blt_TreeView_NextEntry(ep, ENTRY_MASK)){
	    int x;
	    int level;

	    /*
	     * Compute and save the entry's X-coordinate now that we know the
	     * maximum level offset for the entire widget.
	     */
	    level = DEPTH(viewPtr, ep->node);
	    ep->worldX = LEVELX(level) + viewPtr->treeColumn.worldX;
	    x = ep->worldX + ICONWIDTH(level) + ICONWIDTH(level+1) + ep->width;
	    if (x > maxX) {
		maxX = x;
	    }
	    if (ep->worldY >= height) {
		break;
	    }
	    viewPtr->visibleArr[viewPtr->nVisible] = ep;
	    viewPtr->nVisible++;
	}
	viewPtr->visibleArr[viewPtr->nVisible] = NULL;
    }
    /*
     * Note:	It's assumed that the view port always starts at or
     *		over an entry.  Check that a change in the hierarchy
     *		(e.g. closing a node) hasn't left the viewport beyond
     *		the last entry.  If so, adjust the viewport to start
     *		on the last entry.
     */
    if (viewPtr->xOffset > (viewPtr->worldWidth - viewPtr->xScrollUnits)) {
	viewPtr->xOffset = viewPtr->worldWidth - viewPtr->xScrollUnits;
    }
    if (viewPtr->yOffset > (viewPtr->worldHeight - viewPtr->yScrollUnits)) {
	viewPtr->yOffset = viewPtr->worldHeight - viewPtr->yScrollUnits;
    }
    viewPtr->xOffset = Blt_AdjustViewport(viewPtr->xOffset, 
	viewPtr->worldWidth, VPORTWIDTH(viewPtr), viewPtr->xScrollUnits, 
	viewPtr->scrollMode);
    viewPtr->yOffset = Blt_AdjustViewport(viewPtr->yOffset,
	viewPtr->worldHeight, VPORTHEIGHT(viewPtr), viewPtr->yScrollUnits,
	viewPtr->scrollMode);

    viewPtr->flags &= ~DIRTY;
    Blt_PickCurrentItem(viewPtr->bindTable);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * DrawLines --
 *
 * 	Draws vertical lines for the ancestor nodes.  While the entry of the
 * 	ancestor may not be visible, its vertical line segment does extent
 * 	into the viewport.  So walk back up the hierarchy drawing lines
 * 	until we get to the root.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Vertical lines are drawn for the ancestor nodes.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawLines(
    TreeView *viewPtr,			/* Widget record containing the
					 * attribute information for buttons. */
    GC gc,
    Drawable drawable)			/* Pixmap or window to draw into. */
{
    TreeViewEntry **epp;
    TreeViewButton *buttonPtr;
    TreeViewEntry *entryPtr;		/* Entry to be drawn. */

    entryPtr = viewPtr->visibleArr[0];
    while (entryPtr != viewPtr->rootPtr) {
	int level;
	
	entryPtr = Blt_TreeView_ParentEntry(entryPtr);
	if (entryPtr == NULL) {
	    break;
	}
	level = DEPTH(viewPtr, entryPtr->node);
	if (entryPtr->vertLineLength > 0) {
	    int ax, ay, by;
	    int x, y;
	    int height;

	    /*
	     * World X-coordinates aren't computed for entries that are
	     * outside the viewport.  So for each off-screen ancestor node
	     * compute it here too.
	     */
	    entryPtr->worldX = LEVELX(level) + viewPtr->treeColumn.worldX;
	    x = SCREENX(viewPtr, entryPtr->worldX);
	    y = SCREENY(viewPtr, entryPtr->worldY);
	    height = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
			  viewPtr->button.height);
	    ax = x + ICONWIDTH(level) + ICONWIDTH(level + 1) / 2;
	    ay = y + height / 2;
	    by = ay + entryPtr->vertLineLength;
	    if ((entryPtr == viewPtr->rootPtr) && (viewPtr->flags & HIDE_ROOT)){
		TreeViewEntry *nextPtr;
		int h;

		/* If the root node is hidden, go to the next entry to start
		 * the vertical line. */
		nextPtr = Blt_TreeView_NextEntry(viewPtr->rootPtr, ENTRY_MASK);
		h = MAX3(nextPtr->lineHeight, nextPtr->iconHeight, 
			 viewPtr->button.height);
		ay = SCREENY(viewPtr, nextPtr->worldY) + h / 2;
	    }
	    /*
	     * Clip the line's Y-coordinates at the viewport's borders.
	     */
	    if (ay < 0) {
		ay &= 0x1;		/* Make sure the dotted line starts on
					 * the same even/odd pixel. */
	    }
	    if (by > Tk_Height(viewPtr->tkwin)) {
		by = Tk_Height(viewPtr->tkwin);
	    }
	    if ((ay < Tk_Height(viewPtr->tkwin)) && (by > 0)) {
		ay |= 0x1;
		XDrawLine(viewPtr->display, drawable, gc, ax, ay, ax, by);
	    }
	}
    }
    buttonPtr = &viewPtr->button;
    for (epp = viewPtr->visibleArr; *epp != NULL; epp++) {
	int x, y, w, h;
	int buttonY, level;
	int x1, x2, y1, y2;

	entryPtr = *epp;
	/* Entry is open, draw vertical line. */
	x = SCREENX(viewPtr, entryPtr->worldX);
	y = SCREENY(viewPtr, entryPtr->worldY);
	level = DEPTH(viewPtr, entryPtr->node);
	w = ICONWIDTH(level);
	h = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, buttonPtr->height);
	entryPtr->buttonX = (w - buttonPtr->width) / 2;
	entryPtr->buttonY = (h - buttonPtr->height) / 2;
	buttonY = y + entryPtr->buttonY;
	x1 = x + (w / 2);
	y1 = buttonY + (buttonPtr->height / 2);
	x2 = x1 + (ICONWIDTH(level) + ICONWIDTH(level + 1)) / 2;
	if (Blt_Tree_ParentNode(entryPtr->node) != NULL) {
	    /*
	     * For every node except root, draw a horizontal line from the
	     * vertical bar to the middle of the icon.
	     */
	    y1 |= 0x1;
	    XDrawLine(viewPtr->display, drawable, gc, x1, y1, x2, y1);
	}
	if (((entryPtr->flags & ENTRY_CLOSED) == 0) && 
	    (entryPtr->vertLineLength > 0)) {
	    y2 = y1 + entryPtr->vertLineLength;
	    if (y2 > Tk_Height(viewPtr->tkwin)) {
		y2 = Tk_Height(viewPtr->tkwin); /* Clip line at window border.*/
	    }
	    XDrawLine(viewPtr->display, drawable, gc, x2, y1, x2, y2);
	}
    }	
}

void
Blt_TreeView_DrawRule(
    TreeView *viewPtr,			/* Widget record containing the
					 * attribute information for rules. */
    TreeViewColumn *cp,
    Drawable drawable)			/* Pixmap or window to draw into. */
{
    int x, y1, y2;

    x = SCREENX(viewPtr, cp->worldX) + 
	cp->width + viewPtr->ruleMark - viewPtr->ruleAnchor - 1;

    y1 = viewPtr->titleHeight + viewPtr->inset;
    y2 = Tk_Height(viewPtr->tkwin) - viewPtr->inset;
    XDrawLine(viewPtr->display, drawable, cp->ruleGC, x, y1, x, y2);
    viewPtr->flags = TOGGLE(viewPtr->flags, TV_RULE_ACTIVE);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_DrawButton --
 *
 * 	Draws a button for the given entry. The button is drawn centered in the
 * 	region immediately to the left of the origin of the entry (computed in
 * 	the layout routines). The height and width of the button were previously
 * 	calculated from the average row height.
 *
 *		button height = entry height - (2 * some arbitrary padding).
 *		button width = button height.
 *
 *	The button may have a border.  The symbol (either a plus or minus) is
 *	slight smaller than the width or height minus the border.
 *
 *	    x,y origin of entry
 *
 *              +---+
 *              | + | icon label
 *              +---+
 *             closed
 *
 *           |----|----| horizontal offset
 *
 *              +---+
 *              | - | icon label
 *              +---+
 *              open
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A button is drawn for the entry.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_TreeView_DrawButton(
    TreeView *viewPtr,			/* Widget record containing the
					 * attribute information for buttons. */
    TreeViewEntry *entryPtr,		/* Entry. */
    Drawable drawable,			/* Pixmap or window to draw into. */
    int x, int y)
{
    Blt_Background bg;
    TreeViewButton *buttonPtr = &viewPtr->button;
    TreeViewIcon icon;
    int relief;
    int width, height;

    bg = (entryPtr == viewPtr->activeBtnPtr) 
	? buttonPtr->activeBg : buttonPtr->bg;
    relief = (entryPtr->flags & ENTRY_CLOSED) 
	? buttonPtr->closeRelief : buttonPtr->openRelief;
    if (relief == TK_RELIEF_SOLID) {
	relief = TK_RELIEF_FLAT;
    }
    Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, bg, x, y,
	buttonPtr->width, buttonPtr->height, buttonPtr->borderWidth, relief);

    x += buttonPtr->borderWidth;
    y += buttonPtr->borderWidth;
    width  = buttonPtr->width  - (2 * buttonPtr->borderWidth);
    height = buttonPtr->height - (2 * buttonPtr->borderWidth);

    icon = NULL;
    if (buttonPtr->icons != NULL) {	/* Open or close button icon? */
	icon = buttonPtr->icons[0];
	if (((entryPtr->flags & ENTRY_CLOSED) == 0) && 
	    (buttonPtr->icons[1] != NULL)) {
	    icon = buttonPtr->icons[1];
	}
    }
    if (icon != NULL) {			/* Icon or rectangle? */
	Tk_RedrawImage(TreeView_IconBits(icon), 0, 0, width, height, 
		drawable, x, y);
    } else {
	int top, bottom, left, right;
	XSegment segments[6];
	int count;
	GC gc;

	gc = (entryPtr == viewPtr->activeBtnPtr)
	    ? buttonPtr->activeGC : buttonPtr->normalGC;
	if (relief == TK_RELIEF_FLAT) {
	    /* Draw the box outline */

	    left = x - buttonPtr->borderWidth;
	    top = y - buttonPtr->borderWidth;
	    right = left + buttonPtr->width - 1;
	    bottom = top + buttonPtr->height - 1;

	    segments[0].x1 = left;
	    segments[0].x2 = right;
	    segments[0].y2 = segments[0].y1 = top;
	    segments[1].x2 = segments[1].x1 = right;
	    segments[1].y1 = top;
	    segments[1].y2 = bottom;
	    segments[2].x2 = segments[2].x1 = left;
	    segments[2].y1 = top;
	    segments[2].y2 = bottom;
#ifdef WIN32
	    segments[2].y2++;
#endif
	    segments[3].x1 = left;
	    segments[3].x2 = right;
	    segments[3].y2 = segments[3].y1 = bottom;
#ifdef WIN32
	    segments[3].x2++;
#endif
	}
	top = y + height / 2;
	left = x + BUTTON_IPAD;
	right = x + width - BUTTON_IPAD;

	segments[4].y1 = segments[4].y2 = top;
	segments[4].x1 = left;
	segments[4].x2 = right - 1;
#ifdef WIN32
	segments[4].x2++;
#endif

	count = 5;
	if (entryPtr->flags & ENTRY_CLOSED) { /* Draw the vertical line for the
					       * plus. */
	    top = y + BUTTON_IPAD;
	    bottom = y + height - BUTTON_IPAD;
	    segments[5].y1 = top;
	    segments[5].y2 = bottom - 1;
	    segments[5].x1 = segments[5].x2 = x + width / 2;
#ifdef WIN32
	    segments[5].y2++;
#endif
	    count = 6;
	}
	XDrawSegments(viewPtr->display, drawable, gc, segments, count);
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_GetEntryIcon --
 *
 * 	Selects the correct image for the entry's icon depending upon the
 * 	current state of the entry: active/inactive normal/selected.
 *
 *		active - normal
 *		active - selected
 *		inactive - normal
 *		inactive - selected
 *
 * Results:
 *	Returns the image for the icon.
 *
 *---------------------------------------------------------------------------
 */
TreeViewIcon
Blt_TreeView_GetEntryIcon(TreeView *viewPtr, TreeViewEntry *entryPtr)
{
    TreeViewIcon *icons;
    TreeViewIcon icon;
    int hasFocus;

    hasFocus = (entryPtr == viewPtr->focusPtr);
    icons = CHOOSE(viewPtr->icons, entryPtr->icons);
    icon = NULL;
    if (icons != NULL) {	/* Selected or normal icon? */
	icon = icons[0];
	if ((hasFocus) && (icons[1] != NULL)) {
	    icon = icons[1];
	}
    }
    return icon;
}


static int
DrawImage(
    TreeView *viewPtr,			/* Widget record containing the
					 * attribute information for buttons. */
    TreeViewEntry *entryPtr,		/* Entry to display. */
    Drawable drawable,			/* Pixmap or window to draw into. */
    int x, int y)
{
    TreeViewIcon icon;

    icon = Blt_TreeView_GetEntryIcon(viewPtr, entryPtr);

    if (icon != NULL) {			/* Icon or default icon bitmap? */
	int entryHeight;
	int level;
	int maxY;
	int top, bottom;
	int topInset, botInset;
	int width, height;

	level = DEPTH(viewPtr, entryPtr->node);
	entryHeight = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
		viewPtr->button.height);
	height = TreeView_IconHeight(icon);
	width = TreeView_IconWidth(icon);
	if (viewPtr->flatView) {
	    x += (ICONWIDTH(0) - width) / 2;
	} else {
	    x += (ICONWIDTH(level + 1) - width) / 2;
	}	    
	y += (entryHeight - height) / 2;
	botInset = viewPtr->inset - INSET_PAD;
	topInset = viewPtr->titleHeight + viewPtr->inset;
	maxY = Tk_Height(viewPtr->tkwin) - botInset;
	top = 0;
	bottom = y + height;
	if (y < topInset) {
	    height += y - topInset;
	    top = -y + topInset;
	    y = topInset;
	} else if (bottom >= maxY) {
	    height = maxY - y;
	}
	Tk_RedrawImage(TreeView_IconBits(icon), 0, top, width, height, 
		drawable, x, y);
    } 
    return (icon != NULL);
}

static int
DrawLabel(
    TreeView *viewPtr,			/* Widget record. */
    TreeViewEntry *entryPtr,		/* Entry attribute information. */
    Drawable drawable,			/* Pixmap or window to draw into. */
    int x, int y,
    int maxLength)			
{
    const char *label;
    int entryHeight;
    int width, height;			/* Width and height of label. */
    int isFocused, isSelected, isActive;

    entryHeight = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
       viewPtr->button.height);
    isFocused = ((entryPtr == viewPtr->focusPtr) && (viewPtr->flags & FOCUS));
    isSelected = Blt_TreeView_EntryIsSelected(viewPtr, entryPtr);
    isActive = (entryPtr == viewPtr->activePtr);

    /* Includes padding, selection 3-D border, and focus outline. */
    width = entryPtr->labelWidth;
    height = entryPtr->labelHeight;

    /* Center the label, if necessary, vertically along the entry row. */
    if (height < entryHeight) {
	y += (entryHeight - height) / 2;
    }
    if (isFocused) {			/* Focus outline */
	if (isSelected) {
	    XColor *color;

	    color = viewPtr->selFgColor;
	    XSetForeground(viewPtr->display, viewPtr->focusGC, color->pixel);
	}
	if (width > maxLength) {
	    width = maxLength | 0x1;	/* Width has to be odd for the dots in
					 * the focus rectangle to align. */
	}
	XDrawRectangle(viewPtr->display, drawable, viewPtr->focusGC, x, y+1, 
		       width - 1, height - 3);
	if (isSelected) {
	    XSetForeground(viewPtr->display, viewPtr->focusGC, 
		viewPtr->focusColor->pixel);
	}
    }
    x += FOCUS_WIDTH + LABEL_PADX + viewPtr->selBW;
    y += FOCUS_WIDTH + LABEL_PADY + viewPtr->selBW;

    label = GETLABEL(entryPtr);
    if (label[0] != '\0') {
	TreeViewStyle *stylePtr;
	TextStyle ts;
	Blt_Font font;
	XColor *color;

	stylePtr = viewPtr->treeColumn.stylePtr;
	font = entryPtr->font;
	if (font == NULL) {
	    font = Blt_TreeView_GetStyleFont(viewPtr, stylePtr);
	}
	if (isSelected) {
	    color = viewPtr->selFgColor;
	} else if (entryPtr->color != NULL) {
	    color = entryPtr->color;
	} else {
	    color = Blt_TreeView_GetStyleFg(viewPtr, stylePtr);
	}
	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, font);
	Blt_Ts_SetForeground(ts, color);
	Blt_Ts_SetMaxLength(ts, maxLength);
	Blt_Ts_DrawLayout(viewPtr->tkwin, drawable, entryPtr->textPtr, &ts, 
		x, y);
	if (isActive) {
	    Blt_Ts_UnderlineLayout(viewPtr->tkwin, drawable, entryPtr->textPtr, 
		&ts, x, y);
	}
    }
    return entryHeight;
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawFlatEntry --
 *
 * 	Draws a button for the given entry.  Note that buttons should only be
 * 	drawn if the entry has sub-entries to be opened or closed.  It's the
 * 	responsibility of the calling routine to ensure this.
 *
 *	The button is drawn centered in the region immediately to the left of
 *	the origin of the entry (computed in the layout routines). The height
 *	and width of the button were previously calculated from the average row
 *	height.
 *
 *		button height = entry height - (2 * some arbitrary padding).
 *		button width = button height.
 *
 *	The button has a border.  The symbol (either a plus or minus) is slight
 *	smaller than the width or height minus the border.
 *
 *	    x,y origin of entry
 *
 *              +---+
 *              | + | icon label
 *              +---+
 *             closed
 *
 *           |----|----| horizontal offset
 *
 *              +---+
 *              | - | icon label
 *              +---+
 *              open
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A button is drawn for the entry.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawFlatEntry(
    TreeView *viewPtr,			/* Widget record containing the
					 * attribute information for
					 * buttons. */
    TreeViewEntry *entryPtr,		/* Entry to be drawn. */
    Drawable drawable)			/* Pixmap or window to draw into. */
{
    int level;
    int x, y, xMax;

    entryPtr->flags &= ~ENTRY_REDRAW;

    x = SCREENX(viewPtr, entryPtr->worldX);
    y = SCREENY(viewPtr, entryPtr->worldY);
    if (!DrawImage(viewPtr, entryPtr, drawable, x, y)) {
	x -= (DEF_ICON_WIDTH * 2) / 3;
    }
    level = 0;
    x += ICONWIDTH(level);
    /* Entry label. */
    xMax = SCREENX(viewPtr, viewPtr->treeColumn.worldX) + 
	viewPtr->treeColumn.width - viewPtr->treeColumn.titleBW - 
	viewPtr->treeColumn.pad.side2;
    DrawLabel(viewPtr, entryPtr, drawable, x, y, xMax - x);
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawTreeEntry --
 *
 * 	Draws a button for the given entry.  Note that buttons should only be
 * 	drawn if the entry has sub-entries to be opened or closed.  It's the
 * 	responsibility of the calling routine to ensure this.
 *
 *	The button is drawn centered in the region immediately to the left of
 *	the origin of the entry (computed in the layout routines). The height
 *	and width of the button were previously calculated from the average
 *	row height.
 *
 *		button height = entry height - (2 * some arbitrary padding).
 *		button width = button height.
 *
 *	The button has a border.  The symbol (either a plus or minus) is
 *	slight smaller than the width or height minus the border.
 *
 *	    x,y origin of entry
 *
 *              +---+
 *              | + | icon label
 *              +---+
 *             closed
 *
 *           |----|----| horizontal offset
 *
 *              +---+
 *              | - | icon label
 *              +---+
 *              open
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A button is drawn for the entry.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawTreeEntry(
    TreeView *viewPtr,			/* Widget record. */
    TreeViewEntry *entryPtr,		/* Entry to be drawn. */
    Drawable drawable)			/* Pixmap or window to draw into. */
{
    TreeViewButton *buttonPtr = &viewPtr->button;
    int buttonY;
    int level;
    int width, height;
    int x, y, xMax;
    int x1, y1, x2, y2;

    entryPtr->flags &= ~ENTRY_REDRAW;
    x = SCREENX(viewPtr, entryPtr->worldX);
    y = SCREENY(viewPtr, entryPtr->worldY);

    level = DEPTH(viewPtr, entryPtr->node);
    width = ICONWIDTH(level);
    height = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
	buttonPtr->height);

    entryPtr->buttonX = (width - buttonPtr->width) / 2;
    entryPtr->buttonY = (height - buttonPtr->height) / 2;

    buttonY = y + entryPtr->buttonY;

    x1 = x + (width / 2);
    y1 = y2 = buttonY + (buttonPtr->height / 2);
    x2 = x1 + (ICONWIDTH(level) + ICONWIDTH(level + 1)) / 2;

    if ((entryPtr->flags & ENTRY_HAS_BUTTON) && (entryPtr != viewPtr->rootPtr)){
	/*
	 * Except for the root, draw a button for every entry that needs one.
	 * The displayed button can be either an icon (Tk image) or a line
	 * drawing (rectangle with plus or minus sign).
	 */
	Blt_TreeView_DrawButton(viewPtr, entryPtr, drawable, 
		x + entryPtr->buttonX, y + entryPtr->buttonY);
    }
    x += ICONWIDTH(level);

    if (!DrawImage(viewPtr, entryPtr, drawable, x, y)) {
	x -= (DEF_ICON_WIDTH * 2) / 3;
    }
    x += ICONWIDTH(level + 1);

    /* Entry label. */
    xMax = SCREENX(viewPtr, viewPtr->treeColumn.worldX) + 
	viewPtr->treeColumn.width - viewPtr->treeColumn.titleBW - 
	viewPtr->treeColumn.pad.side2;
    DrawLabel(viewPtr, entryPtr, drawable, x, y, xMax - x);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_DrawValue --
 *
 * 	Draws a column value for the given entry.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A button is drawn for the entry.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_TreeView_DrawValue(
    TreeView *viewPtr,			/* Widget record. */
    TreeViewEntry *entryPtr,		/* Node of entry to be drawn. */
    TreeViewValue *valuePtr,
    Drawable drawable,			/* Pixmap or window to draw into. */
    int x, int y)
{
    TreeViewStyle *stylePtr;

    stylePtr = CHOOSE(valuePtr->columnPtr->stylePtr, valuePtr->stylePtr);
    (*stylePtr->classPtr->drawProc)(viewPtr, drawable, entryPtr, valuePtr, 
		stylePtr, x, y);
}

static void
DrawTitle(TreeView *viewPtr, Column *columnPtr, Drawable drawable, int x)
{
    Blt_Background bg;
    XColor *fgColor;
    int dw, dx;
    int avail, need;
    int startX, arrowX;
    int needArrow;

    if (viewPtr->titleHeight < 1) {
	return;
    }
    startX = dx = arrowX = x;
    dw = columnPtr->width;
    if (columnPtr->position == Blt_Chain_GetLength(viewPtr->columns)) {
	/* If there's any room left over, let the last column take it. */
	dw = Tk_Width(viewPtr->tkwin) - x;
    } else if (columnPtr->position == 1) {
	dw += x;
	dx = 0;
    }

    if (columnPtr == viewPtr->activeColumnPtr) {
	bg = columnPtr->activeTitleBg;
	fgColor = columnPtr->activeTitleFgColor;
    } else {
	bg = columnPtr->titleBg;
	fgColor = columnPtr->titleFgColor;
    }

    /* Clear the title area by drawing the background. */
    Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, bg, dx, 
	viewPtr->inset, dw, viewPtr->titleHeight, 0, TK_RELIEF_FLAT);

    x += columnPtr->pad.side1 + columnPtr->titleBW;
    startX = x;
    needArrow = ((columnPtr == viewPtr->sortColumnPtr) && (viewPtr->flatView));
    needArrow = (columnPtr == viewPtr->sortColumnPtr);

    avail = columnPtr->width - (2*columnPtr->titleBW) - PADDING(columnPtr->pad);
    need = columnPtr->titleWidth - (2 * columnPtr->titleBW) -
	columnPtr->arrowWidth;

    if (avail > need) {
	switch (columnPtr->titleJustify) {
	case TK_JUSTIFY_RIGHT:
	    x += avail - need;
	    break;
	case TK_JUSTIFY_CENTER:
	    x += (avail - need) / 2;
	    break;
	case TK_JUSTIFY_LEFT:
	    break;
	}
    }
    if (needArrow) {
	arrowX = x + need + columnPtr->pad.side2;
	if (arrowX > (startX + avail - columnPtr->arrowWidth)) {
	    arrowX = startX + avail + columnPtr->pad.side1 + columnPtr->titleBW 
		- columnPtr->arrowWidth - 1;
	    avail -= columnPtr->arrowWidth + 1;
	    x -= columnPtr->arrowWidth + 1;
	    if (x < startX) {
		avail -= (startX - x);
		x = startX;
	    }
	}
    }
    if (columnPtr->titleIcon != NULL) {
	int ix, iy, iw, ih;

	ih = TreeView_IconHeight(columnPtr->titleIcon);
	iw = TreeView_IconWidth(columnPtr->titleIcon);
	ix = x;
	/* Center the icon vertically.  We already know the column title is at
	 * least as tall as the icon. */
	iy = viewPtr->inset + (viewPtr->titleHeight - ih) / 2;
	Tk_RedrawImage(TreeView_IconBits(columnPtr->titleIcon), 0, 0, iw, ih, 
		drawable, ix, iy);
	x += iw + 6;
	avail -= iw + 6;
    }
    if (columnPtr->text != NULL) {
	TextStyle ts;
	int ty;

	ty = viewPtr->inset + 1;
	if (viewPtr->titleHeight > columnPtr->textHeight) {
	    ty += (viewPtr->titleHeight - columnPtr->textHeight) / 2;
	}
	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, columnPtr->titleFont);
	Blt_Ts_SetForeground(ts, fgColor);
	Blt_Ts_SetMaxLength(ts, avail);
	Blt_Ts_DrawText(viewPtr->tkwin, drawable, columnPtr->text, -1, &ts, x, 
		ty);
    }
    if (needArrow) {
	Blt_DrawArrow(viewPtr->display, drawable, fgColor, arrowX, 
		viewPtr->inset, columnPtr->arrowWidth, viewPtr->titleHeight, 
		columnPtr->titleBW, 
		(viewPtr->sortDecreasing) ? ARROW_UP : ARROW_DOWN);
    }
    Blt_DrawBackgroundRectangle(viewPtr->tkwin, drawable, bg, dx, 
	viewPtr->inset, dw, viewPtr->titleHeight, columnPtr->titleBW, 
	columnPtr->titleRelief);
}

void
Blt_TreeView_DrawHeadings(TreeView *viewPtr, Drawable drawable)
{
    Blt_ChainLink link;
    TreeViewColumn *columnPtr;
    int x;

    for (link = Blt_Chain_FirstLink(viewPtr->columns); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	columnPtr = Blt_Chain_GetValue(link);
	if (columnPtr->flags & COLUMN_HIDDEN) {
	    continue;
	}
	x = SCREENX(viewPtr, columnPtr->worldX);
	if ((x + columnPtr->width) < 0) {
	    continue;			/* Don't draw columns before the left
					 * edge. */
	}
	if (x > Tk_Width(viewPtr->tkwin)) {
	    break;			/* Discontinue when a column starts
					 * beyond the right edge. */
	}
	DrawTitle(viewPtr, columnPtr, drawable, x);
    }
}

static void
DrawNormalBackground(TreeView *viewPtr, Drawable drawable, int x, int w)
{
    Blt_Background bg;

    bg = Blt_TreeView_GetStyleBackground(viewPtr, viewPtr->stylePtr);
    /* This also fills the background where there are no entries. */
    Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, bg, x, 0, w, 
	Tk_Height(viewPtr->tkwin), 0, TK_RELIEF_FLAT);
    if (viewPtr->altBg != NULL) {
	TreeViewEntry **epp;
	int count;

 	for (count = 0, epp = viewPtr->visibleArr; *epp != NULL; epp++, 
		 count++) {
	    if ((*epp)->flatIndex & 0x1) {
		int y;

		y = SCREENY(viewPtr, (*epp)->worldY);
		Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, 
			viewPtr->altBg, x, y, w, (*epp)->height, 
			viewPtr->selBW, viewPtr->selRelief);
	    }
	}
    }
}

static void
DrawSelectionBackground(TreeView *viewPtr, Drawable drawable, int x, int w)
{
    TreeViewEntry **epp;

    /* 
     * Draw the backgrounds of selected entries first.  The vertical lines
     * connecting child entries will be draw on top.
     */
    for (epp = viewPtr->visibleArr; *epp != NULL; epp++) {
	if (Blt_TreeView_EntryIsSelected(viewPtr, *epp)) {
	    Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, 
		viewPtr->selBg, x, SCREENY(viewPtr, (*epp)->worldY), 
		w, (*epp)->height, viewPtr->selBW, viewPtr->selRelief);
	}
    }
}

static void
DrawTreeView(TreeView *viewPtr, Drawable drawable, int x)
{
    TreeViewEntry **epp;
    int count;

    count = 0;
    for (epp = viewPtr->visibleArr; *epp != NULL; epp++) {
	(*epp)->flags &= ~ENTRY_SELECTED;
	if (Blt_TreeView_EntryIsSelected(viewPtr, *epp)) {
	    (*epp)->flags |= ENTRY_SELECTED;
	    count++;
	}
    }
    if ((viewPtr->lineWidth > 0) && (viewPtr->nVisible > 0)) { 
	/* Draw all the vertical lines from topmost node. */
	DrawLines(viewPtr, viewPtr->lineGC, drawable);
	if (count > 0) {
	    TkRegion rgn;

	    rgn = TkCreateRegion();
	    for (epp = viewPtr->visibleArr; *epp != NULL; epp++) {
		if ((*epp)->flags & ENTRY_SELECTED) {
		    XRectangle r;

		    r.x = 0;
		    r.y = SCREENY(viewPtr, (*epp)->worldY);
		    r.width = Tk_Width(viewPtr->tkwin);
		    r.height = (*epp)->height;
		    TkUnionRectWithRegion(&r, rgn, rgn);
		}
	    }
	    TkSetRegion(viewPtr->display, viewPtr->selGC, rgn);
	    DrawLines(viewPtr, viewPtr->selGC, drawable);
	    XSetClipMask(viewPtr->display, viewPtr->selGC, None);
	    TkDestroyRegion(rgn);
	}
    }
    for (epp = viewPtr->visibleArr; *epp != NULL; epp++) {
	DrawTreeEntry(viewPtr, *epp, drawable);
    }
}


static void
DrawFlatView(TreeView *viewPtr, Drawable drawable, int x)
{
    TreeViewEntry **epp;

    for (epp = viewPtr->visibleArr; *epp != NULL; epp++) {
	DrawFlatEntry(viewPtr, *epp, drawable);
    }
}

void
Blt_TreeView_DrawOuterBorders(TreeView *viewPtr, Drawable drawable)
{
    /* Draw 3D border just inside of the focus highlight ring. */
    if (viewPtr->borderWidth > 0) {
	Blt_DrawBackgroundRectangle(viewPtr->tkwin, drawable, viewPtr->bg,
	    viewPtr->highlightWidth, viewPtr->highlightWidth,
	    Tk_Width(viewPtr->tkwin) - 2 * viewPtr->highlightWidth,
	    Tk_Height(viewPtr->tkwin) - 2 * viewPtr->highlightWidth,
	    viewPtr->borderWidth, viewPtr->relief);
    }
    /* Draw focus highlight ring. */
    if (viewPtr->highlightWidth > 0) {
	XColor *color;
	GC gc;

	color = (viewPtr->flags & FOCUS)
	    ? viewPtr->highlightColor : viewPtr->highlightBgColor;
	gc = Tk_GCForColor(color, drawable);
	Tk_DrawFocusHighlight(viewPtr->tkwin, gc, viewPtr->highlightWidth,
	    drawable);
    }
    viewPtr->flags &= ~REDRAW_BORDERS;
}

/*
 *---------------------------------------------------------------------------
 *
 * DisplayTreeView --
 *
 * 	This procedure is invoked to display the widget.
 *
 *      Recompute the layout of the text if necessary. This is necessary if the
 *      world coordinate system has changed.  Specifically, the following may
 *      have occurred:
 *
 *	  1.  a text attribute has changed (font, linespacing, etc.).
 *	  2.  an entry's option changed, possibly resizing the entry.
 *
 *      This is deferred to the display routine since potentially many of
 *      these may occur.
 *
 *	Set the vertical and horizontal scrollbars.  This is done here since the
 *	window width and height are needed for the scrollbar calculations.
 *
 * Results:
 *	None.
 *
 * Side effects:
 * 	The widget is redisplayed.
 *
 *---------------------------------------------------------------------------
 */
static void
DisplayTreeView(ClientData clientData)	/* Information about widget. */
{
    Blt_ChainLink link;
    Pixmap drawable; 
    TreeView *viewPtr = clientData;
    int reqWidth, reqHeight;

    viewPtr->flags &= ~REDRAW_PENDING;
    if (viewPtr->tkwin == NULL) {
	return;				/* Window has been destroyed. */
    }
    fprintf(stderr, "DisplayTreeView %s\n", Tk_PathName(viewPtr->tkwin));
    if (viewPtr->flags & LAYOUT_PENDING) {
	/*
	 * Recompute the layout when entries are opened/closed,
	 * inserted/deleted, or when text attributes change (such as font,
	 * linespacing).
	 */
	Blt_TreeView_ComputeLayout(viewPtr);
    }
    if (viewPtr->flags & (SCROLL_PENDING | DIRTY)) {
	int width, height;
	/* 
	 * Scrolling means that the view port has changed and that the visible
	 * entries need to be recomputed.
	 */
	ComputeVisibleEntries(viewPtr);
	width = VPORTWIDTH(viewPtr);
	height = VPORTHEIGHT(viewPtr);
	if ((viewPtr->flags & SCROLLX) && (viewPtr->xScrollCmdObjPtr != NULL)) {
	    Blt_UpdateScrollbar(viewPtr->interp, viewPtr->xScrollCmdObjPtr, 
		viewPtr->xOffset, viewPtr->xOffset + width, viewPtr->worldWidth);
	}
	if ((viewPtr->flags & SCROLLY) && (viewPtr->yScrollCmdObjPtr != NULL)) {
	    Blt_UpdateScrollbar(viewPtr->interp, viewPtr->yScrollCmdObjPtr,
		viewPtr->yOffset, viewPtr->yOffset+height, viewPtr->worldHeight);
	}
	viewPtr->flags &= ~SCROLL_PENDING;
    }

    reqHeight = (viewPtr->reqHeight > 0) ? viewPtr->reqHeight : 
	viewPtr->worldHeight + viewPtr->titleHeight + 2 * viewPtr->inset + 1;
    reqWidth = (viewPtr->reqWidth > 0) ? viewPtr->reqWidth : 
	viewPtr->worldWidth + 2 * viewPtr->inset;
    if ((reqWidth != Tk_ReqWidth(viewPtr->tkwin)) || 
	(reqHeight != Tk_ReqHeight(viewPtr->tkwin))) {
	Tk_GeometryRequest(viewPtr->tkwin, reqWidth, reqHeight);
    }
#ifdef notdef
    if (viewPtr->reqWidth == 0) {
	int w;
	/* 
	 * The first time through this routine, set the requested width to the
	 * computed width.  All we want is to automatically set the width of
	 * the widget, not dynamically grow/shrink it as attributes change.
	 */
	w = viewPtr->worldWidth + 2 * viewPtr->inset;
	Tk_GeometryRequest(viewPtr->tkwin, w, viewPtr->reqHeight);
    }
#endif
    if (!Tk_IsMapped(viewPtr->tkwin)) {
	return;
    }
    drawable = Tk_GetPixmap(viewPtr->display, Tk_WindowId(viewPtr->tkwin), 
	Tk_Width(viewPtr->tkwin), Tk_Height(viewPtr->tkwin), 
	Tk_Depth(viewPtr->tkwin));

    if ((viewPtr->focusPtr == NULL) && (viewPtr->nVisible > 0)) {
	/* Re-establish the focus entry at the top entry. */
	viewPtr->focusPtr = viewPtr->visibleArr[0];
    }
    viewPtr->flags |= VIEWPORT;
    if ((viewPtr->flags & TV_RULE_ACTIVE) && (viewPtr->resizeColumnPtr!=NULL)) {
	Blt_TreeView_DrawRule(viewPtr, viewPtr->resizeColumnPtr, drawable);
    }
    for (link = Blt_Chain_FirstLink(viewPtr->columns); link != NULL; 
	 link = Blt_Chain_NextLink(link)) {
	TreeViewColumn *columnPtr;
	int x;

	columnPtr = Blt_Chain_GetValue(link);
	columnPtr->flags &= ~COLUMN_DIRTY;
	if (columnPtr->flags & COLUMN_HIDDEN) {
	    continue;
	}
	x = SCREENX(viewPtr, columnPtr->worldX);
	if ((x + columnPtr->width) < 0) {
	    continue;			/* Don't draw columns before the left
					 * edge. */
	}
	if (x > Tk_Width(viewPtr->tkwin)) {
	    break;			/* Discontinue when a column starts
					 * beyond the right edge. */
	}
	/* Clear the column background. */
	DrawNormalBackground(viewPtr, drawable, x, columnPtr->width);
	DrawSelectionBackground(viewPtr, drawable, x, columnPtr->width);
	if (columnPtr != &viewPtr->treeColumn) {
	    TreeViewEntry **epp;
	    
	    for (epp = viewPtr->visibleArr; *epp != NULL; epp++) {
		TreeViewValue *vp;
		
		/* Check if there's a corresponding value in the entry. */
		vp = Blt_TreeView_FindValue(*epp, columnPtr);
		if (vp != NULL) {
		    Blt_TreeView_DrawValue(viewPtr, *epp, vp, drawable, 
			x + columnPtr->pad.side1, SCREENY(viewPtr, (*epp)->worldY));
		}
	    }
	} else {
	    if (viewPtr->flatView) {
		DrawFlatView(viewPtr, drawable, x);
	    } else {
		DrawTreeView(viewPtr, drawable, x);
	    }
	}
 	if (columnPtr->relief != TK_RELIEF_FLAT) {
	    Blt_Background bg;

	    /* Draw a 3D border around the column. */
	    bg = Blt_TreeView_GetStyleBackground(viewPtr, viewPtr->stylePtr);
	    Blt_DrawBackgroundRectangle(viewPtr->tkwin, drawable, bg, x, 0, 
		columnPtr->width, Tk_Height(viewPtr->tkwin), 
		columnPtr->borderWidth, columnPtr->relief);
	}
    }
    if (viewPtr->flags & TV_SHOW_COLUMN_TITLES) {
	Blt_TreeView_DrawHeadings(viewPtr, drawable);
    }
    Blt_TreeView_DrawOuterBorders(viewPtr, drawable);
    if ((viewPtr->flags & TV_RULE_NEEDED) &&
	(viewPtr->resizeColumnPtr != NULL)) {
	Blt_TreeView_DrawRule(viewPtr, viewPtr->resizeColumnPtr, drawable);
    }
    /* Now copy the new view to the window. */
    XCopyArea(viewPtr->display, drawable, Tk_WindowId(viewPtr->tkwin), 
	viewPtr->lineGC, 0, 0, Tk_Width(viewPtr->tkwin), 
	Tk_Height(viewPtr->tkwin), 0, 0);
    Tk_FreePixmap(viewPtr->display, drawable);
    viewPtr->flags &= ~VIEWPORT;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TreeView_SelectCmdProc --
 *
 *      Invoked at the next idle point whenever the current selection changes.
 *      Executes some application-specific code in the -selectcommand option.
 *      This provides a way for applications to handle selection changes.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      TCL code gets executed for some application-specific task.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_TreeView_SelectCmdProc(ClientData clientData) 
{
    TreeView *viewPtr = clientData;

    viewPtr->flags &= ~TV_SELECT_PENDING;
    Tcl_Preserve(viewPtr);
    if (viewPtr->selectCmd != NULL) {
	if (Tcl_GlobalEval(viewPtr->interp, viewPtr->selectCmd) != TCL_OK) {
	    Tcl_BackgroundError(viewPtr->interp);
	}
    }
    Tcl_Release(viewPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeViewObjCmd --
 *
 * 	This procedure is invoked to process the TCL command that corresponds to
 * 	a widget managed by this module. See the user documentation for details
 * 	on what it does.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
TreeViewObjCmd(
    ClientData clientData,		/* Main window associated with
					 * interpreter. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument strings. */
{
    TreeView *viewPtr;
    Tcl_CmdInfo cmdInfo;
    Tcl_Obj *initObjv[2];
    char *string;
    int result;

    string = Tcl_GetString(objv[0]);
    if (objc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", string, 
		" pathName ?option value?...\"", (char *)NULL);
	return TCL_ERROR;
    }
    viewPtr = CreateTreeView(interp, objv[1]);
    if (viewPtr == NULL) {
	goto error;
    }

    /*
     * Invoke a procedure to initialize various bindings on treeview entries.
     * If the procedure doesn't already exist, source it from
     * "$blt_library/treeview.tcl".  We deferred sourcing the file until now so
     * that the variable $blt_library could be set within a script.
     */
    if (!Tcl_GetCommandInfo(interp, "::blt::TreeView::Initialize", &cmdInfo)) {
	if (Tcl_GlobalEval(interp, 
		"source [file join $blt_library treeview.tcl]") != TCL_OK) {
	    char info[200];

	    sprintf_s(info, 200, "\n    (while loading bindings for %.50s)", 
		    Tcl_GetString(objv[0]));
	    Tcl_AddErrorInfo(interp, info);
	    goto error;
	}
    }
    /* 
     * Initialize the widget's configuration options here. The options need to
     * be set first, so that entry, column, and style components can use them
     * for their own GCs.
     */
    bltTreeViewIconsOption.clientData = viewPtr;
    bltTreeViewTreeOption.clientData = viewPtr;
    if (Blt_ConfigureWidgetFromObj(interp, viewPtr->tkwin, bltTreeViewSpecs, 
	objc - 2, objv + 2, (char *)viewPtr, 0) != TCL_OK) {
	goto error;
    }
    if (Blt_ConfigureComponentFromObj(interp, viewPtr->tkwin, "button", 
	"Button", bltTreeViewButtonSpecs, 0, (Tcl_Obj **)NULL, (char *)viewPtr,
	0) != TCL_OK) {
	goto error;
    }

    /* 
     * Rebuild the widget's GC and other resources that are predicated by the
     * widget's configuration options.  Do the same for the default column.
     */
    if (Blt_TreeView_UpdateWidget(interp, viewPtr) != TCL_OK) {
	goto error;
    }
    Blt_TreeView_ConfigureColumn(viewPtr, &viewPtr->treeColumn);
    Blt_TreeView_UpdateStyleGCs(viewPtr, viewPtr->stylePtr);

    /*
     * Invoke a procedure to initialize various bindings on treeview entries.
     * If the procedure doesn't already exist, source it from
     * "$blt_library/treeview.tcl".  We deferred sourcing the file until now
     * so that the variable $blt_library could be set within a script.
     */
    initObjv[0] = Tcl_NewStringObj("::blt::TreeView::Initialize", -1);
    initObjv[1] = objv[1];
    Tcl_IncrRefCount(initObjv[0]);
    Tcl_IncrRefCount(initObjv[1]);
    result = Tcl_EvalObjv(interp, 2, initObjv, TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(initObjv[1]);
    Tcl_DecrRefCount(initObjv[0]);
    if (result != TCL_OK) {
	goto error;
    }
    Tcl_SetStringObj(Tcl_GetObjResult(interp), Tk_PathName(viewPtr->tkwin), -1);
    return TCL_OK;
  error:
    if (viewPtr != NULL) {
	Tk_DestroyWindow(viewPtr->tkwin);
    }
    return TCL_ERROR;
}

int
Blt_TreeViewCmdInitProc(Tcl_Interp *interp)
{
    static Blt_InitCmdSpec cmdSpecs[] = { 
	{ "treeview", TreeViewObjCmd, },
	{ "hiertable", TreeViewObjCmd, }
    };

    return Blt_InitCmds(interp, "::blt", cmdSpecs, 2);
}

#endif /* NO_TREEVIEW */


int
Blt_TreeView_DrawLabel(TreeView *viewPtr, TreeViewEntry *entryPtr, 
		       Drawable drawable)
{
    Blt_Background bg;
    TreeViewIcon icon;
    int level;
    int overlap;
    int srcX, srcY, destX, destY, pmWidth, pmHeight;
    int y2, y1, x1, x2;
    int dx, dy;
    int x, y, xMax, w, h;

    x = SCREENX(viewPtr, entryPtr->worldX);
    y = SCREENY(viewPtr, entryPtr->worldY);
    h = entryPtr->height - 1;
    w = viewPtr->treeColumn.width - 
	(entryPtr->worldX - viewPtr->treeColumn.worldX);
    xMax = SCREENX(viewPtr, viewPtr->treeColumn.worldX) + 
	viewPtr->treeColumn.width - viewPtr->treeColumn.titleBW - 
	viewPtr->treeColumn.pad.side2;

    icon = Blt_TreeView_GetEntryIcon(viewPtr, entryPtr);
    entryPtr->flags |= ENTRY_ICON;
    if (viewPtr->flatView) {
	x += ICONWIDTH(0);
	w -= ICONWIDTH(0);
	if (icon == NULL) {
	    x -= (DEF_ICON_WIDTH * 2) / 3;
	}
    } else {
	level = DEPTH(viewPtr, entryPtr->node);
	if (!viewPtr->flatView) {
	    x += ICONWIDTH(level);
	    w -= ICONWIDTH(level);
	}
	if (icon != NULL) {
	    x += ICONWIDTH(level + 1);
	    w -= ICONWIDTH(level + 1);
	}
    }
    if (Blt_TreeView_EntryIsSelected(viewPtr, entryPtr)) {
	bg = viewPtr->selBg;
    } else {
	bg = Blt_TreeView_GetStyleBackground(viewPtr, viewPtr->stylePtr);
	if ((viewPtr->altBg != NULL) && (entryPtr->flatIndex & 0x1))  {
	    bg = viewPtr->altBg;
	}
    }
    x1 = viewPtr->inset;
    x2 = Tk_Width(viewPtr->tkwin) - viewPtr->inset;
    y1 = viewPtr->titleHeight + viewPtr->inset;
    y2 = Tk_Height(viewPtr->tkwin) - viewPtr->inset - INSET_PAD;

    /* Verify that the label is currently visible on screen. */
    if (((x + w) <  x1) || (x > x2) || ((y + h) < y1) || (y > y2)) {
	return 0;
    }
    /* 
     * sx, sy
     *  +================+
     *  |  +-------------|--------------+
     *  |  | dx, dy      | h            |
     *  +================+              |
     *     |<---- w ---->               |
     *     |                            |
     *     +----------------------------+
     */
    overlap = FALSE;
    destX = x;
    destY = y;
    srcX = srcY = 0;
    pmWidth = w, pmHeight = h;
    dy = y1 - y;
    if (dy > 0) {
	overlap = TRUE;
	pmHeight -= dy;		/* Reduce the height of the pixmap. */
	srcY = -dy;			/* Offset from the origin of the
					 * pixmap. */
	destY = y1;
    } 
    dy = (y + h) - y2;
    if (dy > 0) {
	overlap = TRUE;
	pmHeight -= dy;
    }
    dx = x1 - x;
    if (dx > 0) {
	overlap = TRUE;
	pmWidth -= dx;
	srcX = -dx;
	destX = x1;
    } 
    dx = (x + w) - x2;
    if (dx > 0) {
	overlap = TRUE;
	pmWidth -= dx;
    }
    if ((overlap) && (pmWidth > 0) && (pmHeight)) {
	Pixmap pm;

	pm = Tk_GetPixmap(viewPtr->display, Tk_WindowId(viewPtr->tkwin), 
		pmWidth, pmHeight, Tk_Depth(viewPtr->tkwin));
	/* Clear the entry label background. */
	Blt_FillBackgroundRectangle(viewPtr->tkwin, pm, bg, 0, 0, pmWidth, 
		pmHeight, 0, TK_RELIEF_FLAT);
	DrawLabel(viewPtr, entryPtr, pm, srcX, srcY, xMax - x);
	XCopyArea(viewPtr->display, pm, drawable, viewPtr->lineGC, 0, 0, 
		pmWidth, pmHeight, destX, destY);
	Tk_FreePixmap(viewPtr->display, pm);
    } else {
	/* Clear the entry label background. */
	Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, bg, x, y, w, h,
		0, TK_RELIEF_FLAT);
	DrawLabel(viewPtr, entryPtr, drawable, x, y, xMax - x);
    }
    return 1;
}
