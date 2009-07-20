
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

#define DEF_ICON_WIDTH		16
#define DEF_ICON_HEIGHT		16

typedef ClientData (TagProc)(Blt_TreeView *tvPtr, const char *string);

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

#define DEF_ACTIVE_FOREGROUND	"black"
#define DEF_ACTIVE_ICONS \
	"::blt::tv::activeOpenFolder ::blt::tv::activeCloseFolder"
#define DEF_ACTIVE_RELIEF	"flat"
#define DEF_ACTIVE_STIPPLE	"gray25"
#define DEF_ALLOW_DUPLICATES	"yes"
#define DEF_BACKGROUND		"white"
#define DEF_ALT_BACKGROUND	"grey95"
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
#define DEF_FOCUS_HIGHLIGHT_COLOR		"black"
#define DEF_FOCUS_HIGHLIGHT_WIDTH		"2"
#define DEF_ICONS "::blt::tv::normalOpenFolder ::blt::tv::normalCloseFolder"
#define DEF_VLINE_COLOR		RGB_GREY50
#define DEF_VLINE_MONO		STD_NORMAL_FG_MONO
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
#define DEF_SELECT_BACKGROUND 	STD_SELECT_BACKGROUND /* RGB_LIGHTBLUE1 */
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
#define DEF_TRIMLEFT		""
#define DEF_WIDTH		"200"

#define TreeViewOffset(x)	Blt_Offset(Blt_TreeView, x)

Blt_ConfigSpec bltTreeViewButtonSpecs[] =
{
    {BLT_CONFIG_BACKGROUND, "-activebackground", "activeBackground", 
	"Background", DEF_BUTTON_ACTIVE_BACKGROUND, 
	TreeViewOffset(button.activeBg), 0},
    {BLT_CONFIG_SYNONYM, "-activebg", "activeBackground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-activefg", "activeForeground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-activeforeground", "activeForeground", "Foreground",
	DEF_BUTTON_ACTIVE_FOREGROUND, 
	TreeViewOffset(button.activeFgColor), 0},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	DEF_BUTTON_NORMAL_BACKGROUND, TreeViewOffset(button.bg), 0},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 
	0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_BUTTON_BORDERWIDTH, TreeViewOffset(button.borderWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_RELIEF, "-closerelief", "closeRelief", "Relief",
	DEF_BUTTON_CLOSE_RELIEF, TreeViewOffset(button.closeRelief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_BUTTON_NORMAL_FOREGROUND, TreeViewOffset(button.fgColor), 0},
    {BLT_CONFIG_CUSTOM, "-images", "images", "Icons", (char *)NULL, 
	TreeViewOffset(button.icons), BLT_CONFIG_NULL_OK, 
	&bltTreeViewIconsOption},
    {BLT_CONFIG_RELIEF, "-openrelief", "openRelief", "Relief",
	DEF_BUTTON_OPEN_RELIEF, TreeViewOffset(button.openRelief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-size", "size", "Size", 
	DEF_BUTTON_SIZE, TreeViewOffset(button.reqSize), 0},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

#define EntryOffset(x)	Blt_Offset(Blt_TreeViewEntry, x)

Blt_ConfigSpec bltTreeViewEntrySpecs[] =
{
    {BLT_CONFIG_CUSTOM, "-activeicons", (char *)NULL, (char *)NULL, 
	(char *)NULL, EntryOffset(activeIcons), BLT_CONFIG_NULL_OK, 
	&bltTreeViewIconsOption},
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
    {BLT_CONFIG_CUSTOM, "-activeicons", "activeIcons", "Icons",
	DEF_ACTIVE_ICONS, TreeViewOffset(activeIcons),
	BLT_CONFIG_NULL_OK, &bltTreeViewIconsOption},
    {BLT_CONFIG_BITMASK, 
	"-allowduplicates", "allowDuplicates", "AllowDuplicates",
	DEF_ALLOW_DUPLICATES, TreeViewOffset(flags),
	BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_ALLOW_DUPLICATES},
    {BLT_CONFIG_BACKGROUND, "-alternatebackground", "alternateBackground", 
	"Background", DEF_ALT_BACKGROUND, TreeViewOffset(altBg), 0},
    {BLT_CONFIG_BITMASK, "-autocreate", "autoCreate", "AutoCreate",
	DEF_MAKE_PATH, TreeViewOffset(flags),
	BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_FILL_ANCESTORS},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	DEF_BACKGROUND, TreeViewOffset(bg), 0},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_BORDERWIDTH, TreeViewOffset(borderWidth), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-button", "button", "Button", DEF_BUTTON, 
	TreeViewOffset(buttonFlags), BLT_CONFIG_DONT_SET_DEFAULT, 
	&buttonOption},
    {BLT_CONFIG_STRING, "-closecommand", "closeCommand", "CloseCommand",
	(char *)NULL, TreeViewOffset(closeCmd), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	(char *)NULL, TreeViewOffset(cursor), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_DASHES, "-dashes", "dashes", "Dashes", 	DEF_DASHES, 
	TreeViewOffset(dashes), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BITMASK, "-exportselection", "exportSelection",
	"ExportSelection", DEF_EXPORT_SELECTION, TreeViewOffset(flags),
	BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_SELECT_EXPORT},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_BOOLEAN, "-flat", "flat", "Flat", DEF_FLAT, 
	TreeViewOffset(flatView), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_DASHES, "-focusdashes", "focusDashes", "FocusDashes",
	DEF_FOCUS_DASHES, TreeViewOffset(focusDashes), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-focusforeground", "focusForeground", "FocusForeground",
	DEF_FOCUS_FOREGROUND, TreeViewOffset(focusColor),
	BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-focusforeground", "focusForeground", "FocusForeground",
	DEF_FOCUS_FG_MONO, TreeViewOffset(focusColor),
	BLT_CONFIG_MONO_ONLY},
    {BLT_CONFIG_FONT, "-font", "font", "Font", DEF_FONT, 
	TreeViewOffset(font), 0},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_TEXT_COLOR, TreeViewOffset(fgColor), BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_TEXT_MONO, TreeViewOffset(fgColor), BLT_CONFIG_MONO_ONLY},
    {BLT_CONFIG_PIXELS_NNEG, "-height", "height", "Height", DEF_HEIGHT, 
	TreeViewOffset(reqHeight), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BITMASK, "-hideleaves", "hideLeaves", "HideLeaves",
	DEF_HIDE_LEAVES, TreeViewOffset(flags),
	BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_HIDE_LEAVES},
    {BLT_CONFIG_BITMASK, "-hideroot", "hideRoot", "HideRoot", DEF_HIDE_ROOT,
	TreeViewOffset(flags), BLT_CONFIG_DONT_SET_DEFAULT, 
	(Blt_CustomOption *)TV_HIDE_ROOT},
    {BLT_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_FOCUS_HIGHLIGHT_BACKGROUND, 
        TreeViewOffset(highlightBgColor), 0},
    {BLT_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_FOCUS_HIGHLIGHT_COLOR, TreeViewOffset(highlightColor), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-highlightthickness", "highlightThickness",
	"HighlightThickness", DEF_FOCUS_HIGHLIGHT_WIDTH, 
	TreeViewOffset(highlightWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-icons", "icons", "Icons", DEF_ICONS, 
	TreeViewOffset(icons), BLT_CONFIG_NULL_OK, &bltTreeViewIconsOption},
    {BLT_CONFIG_BACKGROUND, "-nofocusselectbackground", 
	"noFocusSelectBackground", "NoFocusSelectBackground", 
	DEF_SELECT_BACKGROUND, TreeViewOffset(selOutFocusBg), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-nofocusselectforeground", "noFocusSelectForeground", 
	"NoFocusSelectForeground", DEF_SELECT_FOREGROUND, 
	TreeViewOffset(selOutFocusFgColor), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-linecolor", "lineColor", "LineColor",
	DEF_VLINE_COLOR, TreeViewOffset(lineColor), BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-linecolor", "lineColor", "LineColor", 
	DEF_VLINE_MONO, TreeViewOffset(lineColor), BLT_CONFIG_MONO_ONLY},
    {BLT_CONFIG_PIXELS_NNEG, "-linespacing", "lineSpacing", "LineSpacing",
	DEF_LINESPACING, TreeViewOffset(leader),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-linewidth", "lineWidth", "LineWidth", 
	DEF_LINEWIDTH, TreeViewOffset(lineWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_STRING, "-opencommand", "openCommand", "OpenCommand",
	(char *)NULL, TreeViewOffset(openCmd), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_RELIEF, "-relief", "relief", "Relief", DEF_RELIEF, 
	TreeViewOffset(relief), 0},
    {BLT_CONFIG_CURSOR, "-resizecursor", "resizeCursor", "ResizeCursor",
	DEF_RESIZE_CURSOR, TreeViewOffset(resizeCursor), 0},
    {BLT_CONFIG_CUSTOM, "-scrollmode", "scrollMode", "ScrollMode",
	DEF_SCROLL_MODE, TreeViewOffset(scrollMode),
	BLT_CONFIG_DONT_SET_DEFAULT, &scrollmodeOption},
    {BLT_CONFIG_BACKGROUND, "-selectbackground", "selectBackground", 
	"Foreground", DEF_SELECT_BACKGROUND, TreeViewOffset(selInFocusBg), 
	 0},
    {BLT_CONFIG_PIXELS_NNEG, "-selectborderwidth", "selectBorderWidth", 
	"BorderWidth", DEF_SELECT_BORDERWIDTH, 
	TreeViewOffset(selBorderWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_STRING, "-selectcommand", "selectCommand", "SelectCommand",
	(char *)NULL, TreeViewOffset(selectCmd), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_SELECT_FOREGROUND, TreeViewOffset(selInFocusFgColor), 0},
    {BLT_CONFIG_CUSTOM, "-selectmode", "selectMode", "SelectMode",
	DEF_SELECT_MODE, TreeViewOffset(selectMode),
	BLT_CONFIG_DONT_SET_DEFAULT, &selectmodeOption},
    {BLT_CONFIG_RELIEF, "-selectrelief", "selectRelief", "Relief",
	DEF_SELECT_RELIEF, TreeViewOffset(selRelief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-separator", "separator", "Separator", (char *)NULL, 
	TreeViewOffset(pathSep), BLT_CONFIG_NULL_OK, &separatorOption},
    {BLT_CONFIG_BITMASK, "-newtags", "newTags", "newTags", 
	DEF_NEW_TAGS, TreeViewOffset(flags), 
        BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_NEW_TAGS},
    {BLT_CONFIG_BITMASK, "-showtitles", "showTitles", "ShowTitles",
	DEF_SHOW_TITLES, TreeViewOffset(flags), 0,
        (Blt_CustomOption *)TV_SHOW_COLUMN_TITLES},
    {BLT_CONFIG_BITMASK, "-sortselection", "sortSelection", "SortSelection",
	DEF_SORT_SELECTION, TreeViewOffset(flags), 
        BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TV_SELECT_SORTED},
    {BLT_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_TAKE_FOCUS, TreeViewOffset(takeFocus), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_STRING, "-tree", "tree", "Tree", (char *)NULL, 
	TreeViewOffset(treeName), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_STRING, "-trim", "trim", "Trim", DEF_TRIMLEFT, 
	TreeViewOffset(trimLeft), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_NNEG, "-width", "width", "Width", DEF_WIDTH, 
	TreeViewOffset(reqWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
	(char *)NULL, TreeViewOffset(xScrollCmdObjPtr), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_NNEG, "-xscrollincrement", "xScrollIncrement", 
	"ScrollIncrement", DEF_SCROLL_INCREMENT, 
	TreeViewOffset(xScrollUnits), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
	(char *)NULL, TreeViewOffset(yScrollCmdObjPtr), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_NNEG, "-yscrollincrement", "yScrollIncrement", 
	"ScrollIncrement", DEF_SCROLL_INCREMENT, 
	TreeViewOffset(yScrollUnits), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

/* Forward Declarations */
BLT_EXTERN Blt_TreeApplyProc Blt_TreeViewSortApplyProc;
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

static int ComputeVisibleEntries(Blt_TreeView *tvPtr);

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
Blt_TreeView_EventuallyRedraw(Blt_TreeView *tvPtr)
{
    if ((tvPtr->tkwin != NULL) && ((tvPtr->flags & TV_REDRAW) == 0)) {
	tvPtr->flags |= TV_REDRAW;
	Tcl_DoWhenIdle(DisplayTreeView, tvPtr);
    }
}

void
Blt_TreeView_TraceColumn(Blt_TreeView *tvPtr, Blt_TreeViewColumn *colPtr)
{
    Blt_Tree_CreateTrace(tvPtr->tree, NULL /* Node */, colPtr->key, NULL,
	TREE_TRACE_FOREIGN_ONLY | TREE_TRACE_WRITE | TREE_TRACE_UNSET, 
	TreeTraceProc, tvPtr);
}

static void
TraceColumns(Blt_TreeView *tvPtr)
{
    Blt_ChainLink link;

    for(link = Blt_Chain_FirstLink(tvPtr->columns); link != NULL;
	link = Blt_Chain_NextLink(link)) {
	Blt_TreeViewColumn *colPtr;

	colPtr = Blt_Chain_GetValue(link);
	Blt_Tree_CreateTrace(
		tvPtr->tree, 
		NULL /* Node */, 
		colPtr->key /* Key pattern */, 
		NULL /* Tag */,
	        TREE_TRACE_FOREIGN_ONLY | TREE_TRACE_WRITE | TREE_TRACE_UNSET, 
	        TreeTraceProc /* Callback routine */, 
		tvPtr /* Client data */);
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
	Blt_TreeView *tvPtr = clientData;

	/* 
	 * Release the current tree, removing any entry fields. 
	 */
	root = Blt_Tree_RootNode(*treePtr);
	Blt_Tree_Apply(root, DeleteApplyProc, tvPtr);
	Blt_TreeView_ClearSelection(tvPtr);
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
    } else if ((c == 'h') && (strcmp(string, "hiertable") == 0)) {
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    UID *labelPtr = (UID *)(widgRec + offset);
    char *string;

    string = Tcl_GetString(objPtr);
    if (string[0] != '\0') {
	Blt_TreeView *tvPtr = clientData;

	*labelPtr = Blt_TreeView_GetUid(tvPtr, string);
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    UID labelUid = *(UID *)(widgRec + offset);
    const char *string;

    if (labelUid == NULL) {
	Blt_TreeViewEntry *entryPtr  = (Blt_TreeViewEntry *)widgRec;

	string = Blt_Tree_NodeLabel(entryPtr->node);
    } else {
	string = labelUid;
    }
    return Tcl_NewStringObj(string, -1);
}

/*ARGSUSED*/
static void
FreeLabel(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    UID *labelPtr = (UID *)(widgRec + offset);

    if (*labelPtr != NULL) {
	Blt_TreeView *tvPtr = clientData;

	Blt_TreeView_FreeUid(tvPtr, *labelPtr);
	*labelPtr = NULL;
    }
}
/*

 *---------------------------------------------------------------------------
 *
 * ObjToStyles --
 *
 *	Convert the list representing the field-name style-name pairs 
 *	into stylePtr's.
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
ObjToStyles(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Blt_TreeViewEntry *entryPtr = (Blt_TreeViewEntry *)widgRec;
    Blt_TreeView *tvPtr;
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
    tvPtr = entryPtr->tvPtr;
    for (i = 0; i < objc; i += 2) {
	Blt_TreeViewValue *valuePtr;
	Blt_TreeViewStyle *stylePtr;
	Blt_TreeViewColumn *colPtr;
	char *string;

	if (Blt_TreeView_GetColumn(interp, tvPtr, objv[i], &colPtr)!=TCL_OK) {
	    return TCL_ERROR;
	}
	valuePtr = Blt_TreeView_FindValue(entryPtr, colPtr);
	if (valuePtr == NULL) {
	    return TCL_ERROR;
	}
	string = Tcl_GetString(objv[i+1]);
	stylePtr = NULL;
	if ((*string != '\0') && (Blt_TreeView_GetStyle(interp, tvPtr, string, 
		&stylePtr) != TCL_OK)) {
	    return TCL_ERROR;			/* No data ??? */
	}
	if (valuePtr->stylePtr != NULL) {
	    Blt_TreeView_FreeStyle(tvPtr, valuePtr->stylePtr);
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Blt_TreeViewEntry *entryPtr = (Blt_TreeViewEntry *)widgRec;
    Blt_TreeViewValue *vp;
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
 *	Gets or creates a unique string identifier.  Strings are
 *	reference counted.  The string is placed into a hashed table
 *	local to the treeview.
 *
 * Results:
 *	Returns the pointer to the hashed string.
 *
 *---------------------------------------------------------------------------
 */
UID
Blt_TreeView_GetUid(Blt_TreeView *tvPtr, const char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;
    size_t refCount;

    hPtr = Blt_CreateHashEntry(&tvPtr->uidTable, string, &isNew);
    if (isNew) {
	refCount = 1;
    } else {
	refCount = (size_t)Blt_GetHashValue(hPtr);
	refCount++;
    }
    Blt_SetHashValue(hPtr, refCount);
    return Blt_GetHashKey(&tvPtr->uidTable, hPtr);
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
Blt_TreeView_FreeUid(Blt_TreeView *tvPtr, UID uid)
{
    Blt_HashEntry *hPtr;
    size_t refCount;

    hPtr = Blt_FindHashEntry(&tvPtr->uidTable, uid);
    assert(hPtr != NULL);
    refCount = (size_t)Blt_GetHashValue(hPtr);
    refCount--;
    if (refCount > 0) {
	Blt_SetHashValue(hPtr, refCount);
    } else {
	Blt_DeleteHashEntry(&tvPtr->uidTable, hPtr);
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Blt_TreeView *tvPtr = clientData;
    UID *uidPtr = (UID *)(widgRec + offset);

    *uidPtr = Blt_TreeView_GetUid(tvPtr, Tcl_GetString(objPtr));
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
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
FreeUid(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    UID *uidPtr = (UID *)(widgRec + offset);

    if (*uidPtr != NULL) {
	Blt_TreeView *tvPtr = clientData;

	Blt_TreeView_FreeUid(tvPtr, *uidPtr);
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
    int x,			/* Not used. */
    int y,			/* Not used. */
    int width,			/* Not used. */
    int height,			/* Not used. */
    int imageWidth, 		/* Not used. */
    int imageHeight)		/* Not used. */
{
    Blt_TreeView *tvPtr = clientData;

    tvPtr->flags |= (TV_DIRTY | TV_LAYOUT | TV_SCROLL);
    Blt_TreeView_EventuallyRedraw(tvPtr);
}

Blt_TreeViewIcon
Blt_TreeView_GetIcon(Blt_TreeView *tvPtr, const char *iconName)
{
    Blt_HashEntry *hPtr;
    int isNew;
    struct _Blt_TreeViewIcon *iconPtr;

    hPtr = Blt_CreateHashEntry(&tvPtr->iconTable, iconName, &isNew);
    if (isNew) {
	Tk_Image tkImage;
	int width, height;

	tkImage = Tk_GetImage(tvPtr->interp, tvPtr->tkwin, (char *)iconName, 
		IconChangedProc, tvPtr);
	if (tkImage == NULL) {
	    Blt_DeleteHashEntry(&tvPtr->iconTable, hPtr);
	    return NULL;
	}
	Tk_SizeOfImage(tkImage, &width, &height);
	iconPtr = Blt_AssertMalloc(sizeof(struct _Blt_TreeViewIcon));
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
Blt_TreeView_FreeIcon(Blt_TreeView *tvPtr, Blt_TreeViewIcon icon)
{
    struct _Blt_TreeViewIcon *iconPtr = icon;

    iconPtr->refCount--;
    if (iconPtr->refCount == 0) {
	Blt_DeleteHashEntry(&tvPtr->iconTable, iconPtr->hashPtr);
	Tk_FreeImage(iconPtr->tkImage);
	Blt_Free(iconPtr);
    }
}

static void
DumpIconTable(Blt_TreeView *tvPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    struct _Blt_TreeViewIcon *iconPtr;

    for (hPtr = Blt_FirstHashEntry(&tvPtr->iconTable, &iter);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	iconPtr = Blt_GetHashValue(hPtr);
	Tk_FreeImage(iconPtr->tkImage);
	Blt_Free(iconPtr);
    }
    Blt_DeleteHashTable(&tvPtr->iconTable);
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Tcl_Obj **objv;
    Blt_TreeView *tvPtr = clientData;
    Blt_TreeViewIcon **iconPtrPtr = (Blt_TreeViewIcon **)(widgRec + offset);
    Blt_TreeViewIcon *icons;
    int objc;
    int result;

    result = TCL_OK;
    icons = NULL;
    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc > 0) {
	int i;
	
	icons = Blt_AssertMalloc(sizeof(Blt_TreeViewIcon *) * (objc + 1));
	for (i = 0; i < objc; i++) {
	    icons[i] = Blt_TreeView_GetIcon(tvPtr, Tcl_GetString(objv[i]));
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Blt_TreeViewIcon *icons = *(Blt_TreeViewIcon **)(widgRec + offset);
    Tcl_Obj *listObjPtr;
    
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (icons != NULL) {
	Blt_TreeViewIcon *iconPtr;

	for (iconPtr = icons; *iconPtr != NULL; iconPtr++) {
	    Tcl_Obj *objPtr;

	    objPtr = Tcl_NewStringObj(Blt_NameOfImage((*iconPtr)->tkImage), -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);

	}
    }
    return listObjPtr;
}

/*ARGSUSED*/
static void
FreeIcons(ClientData clientData, Display *display, char *widgRec, int offset)
{
    Blt_TreeViewIcon **iconsPtr = (Blt_TreeViewIcon **)(widgRec + offset);

    if (*iconsPtr != NULL) {
	Blt_TreeViewIcon *ip;
	Blt_TreeView *tvPtr = clientData;

	for (ip = *iconsPtr; *ip != NULL; ip++) {
	    Blt_TreeView_FreeIcon(tvPtr, *ip);
	}
	Blt_Free(*iconsPtr);
	*iconsPtr = NULL;
    }
}

Blt_TreeViewEntry *
Blt_TreeView_NodeToEntry(Blt_TreeView *tvPtr, Blt_TreeNode node)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&tvPtr->entryTable, (char *)node);
    if (hPtr == NULL) {
	fprintf(stderr, "Blt_TreeView_NodeToEntry: can't find node %s\n", 
		Blt_Tree_NodeLabel(node));
	abort();
	return NULL;
    }
    return Blt_GetHashValue(hPtr);
}

Blt_TreeViewEntry *
Blt_TreeView_FindEntry(Blt_TreeView *tvPtr, Blt_TreeNode node)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&tvPtr->entryTable, (char *)node);
    if (hPtr == NULL) {
	return NULL;
    }
    return Blt_GetHashValue(hPtr);
}

int
Blt_TreeView_Apply(
    Blt_TreeView *tvPtr,
    Blt_TreeViewEntry *entryPtr,	/* Root entry of subtree. */
    Blt_TreeViewApplyProc *proc,	/* Procedure to call for each entry. */
    unsigned int flags)
{
    if ((flags & ENTRY_HIDE) && 
	(Blt_TreeView_EntryIsHidden(entryPtr))) {
	return TCL_OK;		/* Hidden node. */
    }
    if ((flags & ENTRY_HIDE) && (entryPtr->flags & ENTRY_HIDE)) {
	return TCL_OK;		/* Hidden node. */
    }
    if (((flags & ENTRY_CLOSED) == 0) || 
	((entryPtr->flags & ENTRY_CLOSED) == 0)) {
	Blt_TreeViewEntry *childPtr;
	Blt_TreeNode node, next;

	for (node = Blt_Tree_FirstChild(entryPtr->node); node != NULL; 
	     node = next) {
	    next = Blt_Tree_NextSibling(node);
	    /* 
	     * Get the next child before calling Blt_TreeView_Apply
	     * recursively.  This is because the apply callback may delete the
	     * node and its link.
	     */
	    childPtr = Blt_TreeView_NodeToEntry(tvPtr, node);
	    if (Blt_TreeView_Apply(tvPtr, childPtr, proc, flags) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    if ((*proc) (tvPtr, entryPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

int
Blt_TreeView_EntryIsHidden(Blt_TreeViewEntry *entryPtr)
{
    Blt_TreeView *tvPtr = entryPtr->tvPtr; 

    if ((tvPtr->flags & TV_HIDE_LEAVES) && (Blt_Tree_IsLeaf(entryPtr->node))) {
	return TRUE;
    }
    return (entryPtr->flags & ENTRY_HIDE) ? TRUE : FALSE;
}

#ifdef notdef
int
Blt_TreeViewEntryIsMapped(Blt_TreeViewEntry *entryPtr)
{
    Blt_TreeView *tvPtr = entryPtr->tvPtr; 

    /* Don't check if the entry itself is open, only that its
     * ancestors are. */
    if (Blt_TreeViewEntryIsHidden(entryPtr)) {
	return FALSE;
    }
    if (entryPtr == tvPtr->rootPtr) {
	return TRUE;
    }
    entryPtr = Blt_TreeView_ParentEntry(entryPtr);
    while (entryPtr != tvPtr->rootPtr) {
	if (entryPtr->flags & (ENTRY_CLOSED | ENTRY_HIDE)) {
	    return FALSE;
	}
	entryPtr = Blt_TreeView_ParentEntry(entryPtr);
    }
    return TRUE;
}
#endif

Blt_TreeViewEntry *
Blt_TreeView_FirstChild(Blt_TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeNode node;
    Blt_TreeView *tvPtr = entryPtr->tvPtr; 

    for (node = Blt_Tree_FirstChild(entryPtr->node); node != NULL; 
	 node = Blt_Tree_NextSibling(node)) {
	entryPtr = Blt_TreeView_NodeToEntry(tvPtr, node);
	if (((mask & ENTRY_HIDE) == 0) || 
	    (!Blt_TreeView_EntryIsHidden(entryPtr))) {
	    return entryPtr;
	}
    }
    return NULL;
}

Blt_TreeViewEntry *
Blt_TreeView_LastChild(Blt_TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeNode node;
    Blt_TreeView *tvPtr = entryPtr->tvPtr; 

    for (node = Blt_Tree_LastChild(entryPtr->node); node != NULL; 
	 node = Blt_Tree_PrevSibling(node)) {
	entryPtr = Blt_TreeView_NodeToEntry(tvPtr, node);
	if (((mask & ENTRY_HIDE) == 0) ||
	    (!Blt_TreeView_EntryIsHidden(entryPtr))) {
	    return entryPtr;
	}
    }
    return NULL;
}

Blt_TreeViewEntry *
Blt_TreeView_NextSibling(Blt_TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeNode node;
    Blt_TreeView *tvPtr = entryPtr->tvPtr; 

    for (node = Blt_Tree_NextSibling(entryPtr->node); node != NULL; 
	 node = Blt_Tree_NextSibling(node)) {
	entryPtr = Blt_TreeView_NodeToEntry(tvPtr, node);
	if (((mask & ENTRY_HIDE) == 0) ||
	    (!Blt_TreeView_EntryIsHidden(entryPtr))) {
	    return entryPtr;
	}
    }
    return NULL;
}

Blt_TreeViewEntry *
Blt_TreeView_PrevSibling(Blt_TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeNode node;
    Blt_TreeView *tvPtr = entryPtr->tvPtr; 

    for (node = Blt_Tree_PrevSibling(entryPtr->node); node != NULL; 
	 node = Blt_Tree_PrevSibling(node)) {
	entryPtr = Blt_TreeView_NodeToEntry(tvPtr, node);
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
 *	Returns the "previous" node in the tree.  This node (in 
 *	depth-first order) is its parent if the node has no siblings
 *	that are previous to it.  Otherwise it is the last descendant 
 *	of the last sibling.  In this case, descend the sibling's
 *	hierarchy, using the last child at any ancestor, until we
 *	we find a leaf.
 *
 *---------------------------------------------------------------------------
 */
Blt_TreeViewEntry *
Blt_TreeView_PrevEntry(Blt_TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeView *tvPtr = entryPtr->tvPtr; 
    Blt_TreeViewEntry *prevPtr;

    if (entryPtr->node == Blt_Tree_RootNode(tvPtr->tree)) {
	return NULL;		/* The root is the first node. */
    }
    prevPtr = Blt_TreeView_PrevSibling(entryPtr, mask);
    if (prevPtr == NULL) {
	/* There are no siblings previous to this one, so pick the parent. */
	prevPtr = Blt_TreeView_ParentEntry(entryPtr);
    } else {
	/*
	 * Traverse down the right-most thread in order to select the
	 * last entry.  Stop if we find a "closed" entry or reach a leaf.
	 */
	entryPtr = prevPtr;
	while ((entryPtr->flags & mask) == 0) {
	    entryPtr = Blt_TreeView_LastChild(entryPtr, mask);
	    if (entryPtr == NULL) {
		break;		/* Found a leaf. */
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
 * Blt_TreeView_NextNode --
 *
 *	Returns the "next" node in relation to the given node.  
 *	The next node (in depth-first order) is either the first 
 *	child of the given node the next sibling if the node has
 *	no children (the node is a leaf).  If the given node is the 
 *	last sibling, then try it's parent next sibling.  Continue
 *	until we either find a next sibling for some ancestor or 
 *	we reach the root node.  In this case the current node is 
 *	the last node in the tree.
 *
 *---------------------------------------------------------------------------
 */
Blt_TreeViewEntry *
Blt_TreeView_NextEntry(Blt_TreeViewEntry *entryPtr, unsigned int mask)
{
    Blt_TreeView *tvPtr = entryPtr->tvPtr; 
    Blt_TreeViewEntry *nextPtr;
    int ignoreLeaf;

    ignoreLeaf = ((tvPtr->flags & TV_HIDE_LEAVES) && 
		  (Blt_Tree_IsLeaf(entryPtr->node)));

    if ((!ignoreLeaf) && ((entryPtr->flags & mask) == 0)) {
	nextPtr = Blt_TreeView_FirstChild(entryPtr, mask); 
	if (nextPtr != NULL) {
	    return nextPtr;	/* Pick the first sub-node. */
	}
    }


    /* 
     * Back up until to a level where we can pick a "next sibling".  
     * For the last entry we'll thread our way back to the root.
     */

    while (entryPtr != tvPtr->rootPtr) {
	nextPtr = Blt_TreeView_NextSibling(entryPtr, mask);
	if (nextPtr != NULL) {
	    return nextPtr;
	}
	entryPtr = Blt_TreeView_ParentEntry(entryPtr);
    }
    return NULL;			/* At root, no next node. */
}

void
Blt_TreeView_ConfigureButtons(Blt_TreeView *tvPtr)
{
    GC newGC;
    Blt_TreeViewButton *buttonPtr = &tvPtr->button;
    XGCValues gcValues;
    unsigned long gcMask;

    gcMask = GCForeground;
    gcValues.foreground = buttonPtr->fgColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (buttonPtr->normalGC != NULL) {
	Tk_FreeGC(tvPtr->display, buttonPtr->normalGC);
    }
    buttonPtr->normalGC = newGC;

    gcMask = GCForeground;
    gcValues.foreground = buttonPtr->activeFgColor->pixel;
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (buttonPtr->activeGC != NULL) {
	Tk_FreeGC(tvPtr->display, buttonPtr->activeGC);
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
	    width = Blt_TreeView_IconWidth(buttonPtr->icons[i]);
	    height = Blt_TreeView_IconWidth(buttonPtr->icons[i]);
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
Blt_TreeView_ConfigureEntry(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr,
			    int objc, Tcl_Obj *const *objv, int flags)
{
    GC newGC;
    Blt_ChainLink link;
    Blt_TreeViewColumn *colPtr;

    bltTreeViewIconsOption.clientData = tvPtr;
    bltTreeViewUidOption.clientData = tvPtr;
    labelOption.clientData = tvPtr;
    if (Blt_ConfigureWidgetFromObj(tvPtr->interp, tvPtr->tkwin, 
	bltTreeViewEntrySpecs, objc, objv, (char *)entryPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    /* 
     * Check if there are values that need to be added 
     */
    for(link = Blt_Chain_FirstLink(tvPtr->columns); link != NULL;
	link = Blt_Chain_NextLink(link)) {
	colPtr = Blt_Chain_GetValue(link);
	Blt_TreeView_AddValue(entryPtr, colPtr);
    }

    newGC = NULL;
    if ((entryPtr->font != NULL) || (entryPtr->color != NULL)) {
	Blt_Font font;
	XColor *colorPtr;
	XGCValues gcValues;
	unsigned long gcMask;

	font = entryPtr->font;
	if (font == NULL) {
	    font = Blt_TreeView_GetStyleFont(tvPtr, tvPtr->treeColumn.stylePtr);
	}
	colorPtr = CHOOSE(tvPtr->fgColor, entryPtr->color);
	gcMask = GCForeground | GCFont;
	gcValues.foreground = colorPtr->pixel;
	gcValues.font = Blt_FontId(font);
	newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    }
    if (entryPtr->gc != NULL) {
	Tk_FreeGC(tvPtr->display, entryPtr->gc);
    }
    /* Assume all changes require a new layout. */
    entryPtr->gc = newGC;
    entryPtr->flags |= ENTRY_LAYOUT_PENDING;
    if (Blt_ConfigModified(bltTreeViewEntrySpecs, "-font", (char *)NULL)) {
	tvPtr->flags |= TV_UPDATE;
    }
    tvPtr->flags |= (TV_LAYOUT | TV_DIRTY);
    return TCL_OK;
}

void
Blt_TreeView_DestroyValue(Blt_TreeView *tvPtr, Blt_TreeViewValue *valuePtr)
{
    if (valuePtr->stylePtr != NULL) {
	Blt_TreeView_FreeStyle(tvPtr, valuePtr->stylePtr);
    }
    if (valuePtr->textPtr != NULL) {
	Blt_Free(valuePtr->textPtr);
    }
}


static void
DestroyEntry(DestroyData data)
{
    Blt_TreeViewEntry *entryPtr = (Blt_TreeViewEntry *)data;
    Blt_TreeView *tvPtr;
    
    tvPtr = entryPtr->tvPtr;
    bltTreeViewIconsOption.clientData = tvPtr;
    bltTreeViewUidOption.clientData = tvPtr;
    labelOption.clientData = tvPtr;
    Blt_FreeOptions(bltTreeViewEntrySpecs, (char *)entryPtr, tvPtr->display,
	0);
    if (!Blt_Tree_TagTableIsShared(tvPtr->tree)) {
	/* Don't clear tags unless this client is the only one using
	 * the tag table.*/
	Blt_Tree_ClearTags(tvPtr->tree, entryPtr->node);
    }
    if (entryPtr->gc != NULL) {
	Tk_FreeGC(tvPtr->display, entryPtr->gc);
    }
    /* Delete the chain of data values from the entry. */
    if (entryPtr->values != NULL) {
	Blt_TreeViewValue *valuePtr, *nextPtr;
	
	for (valuePtr = entryPtr->values; valuePtr != NULL; 
	     valuePtr = nextPtr) {
	    nextPtr = valuePtr->nextPtr;
	    Blt_TreeView_DestroyValue(tvPtr, valuePtr);
	}
	entryPtr->values = NULL;
    }
    if (entryPtr->fullName != NULL) {
	Blt_Free(entryPtr->fullName);
    }
    if (entryPtr->textPtr != NULL) {
	Blt_Free(entryPtr->textPtr);
    }
    Blt_PoolFreeItem(tvPtr->entryPool, entryPtr);
}

Blt_TreeViewEntry *
Blt_TreeView_ParentEntry(Blt_TreeViewEntry *entryPtr)
{
    Blt_TreeView *tvPtr = entryPtr->tvPtr; 
    Blt_TreeNode node;

    if (entryPtr->node == Blt_Tree_RootNode(tvPtr->tree)) {
	return NULL;
    }
    node = Blt_Tree_ParentNode(entryPtr->node);
    if (node == NULL) {
	return NULL;
    }
    return Blt_TreeView_NodeToEntry(tvPtr, node);
}

static void
FreeEntry(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr)
{
    Blt_HashEntry *hPtr;

    if (entryPtr == tvPtr->activePtr) {
	tvPtr->activePtr = Blt_TreeView_ParentEntry(entryPtr);
    }
    if (entryPtr == tvPtr->activeBtnPtr) {
	tvPtr->activeBtnPtr = NULL;
    }
    if (entryPtr == tvPtr->focusPtr) {
	tvPtr->focusPtr = Blt_TreeView_ParentEntry(entryPtr);
	Blt_SetFocusItem(tvPtr->bindTable, tvPtr->focusPtr, ITEM_ENTRY);
    }
    if (entryPtr == tvPtr->selAnchorPtr) {
	tvPtr->selMarkPtr = tvPtr->selAnchorPtr = NULL;
    }
    Blt_TreeView_DeselectEntry(tvPtr, entryPtr);
    Blt_TreeView_PruneSelection(tvPtr, entryPtr);
    Blt_DeleteBindings(tvPtr->bindTable, entryPtr);
    hPtr = Blt_FindHashEntry(&tvPtr->entryTable, entryPtr->node);
    if (hPtr != NULL) {
	Blt_DeleteHashEntry(&tvPtr->entryTable, hPtr);
    }
    entryPtr->node = NULL;

    Tcl_EventuallyFree(entryPtr, DestroyEntry);
    /*
     * Indicate that the screen layout of the hierarchy may have changed because
     * this node was deleted.  The screen positions of the nodes in
     * tvPtr->visibleArr are invalidated.
     */
    tvPtr->flags |= (TV_LAYOUT | TV_DIRTY);
    Blt_TreeView_EventuallyRedraw(tvPtr);
}

int
Blt_TreeView_EntryIsSelected(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&tvPtr->selectTable, (char *)entryPtr);
    return (hPtr != NULL);
}

void
Blt_TreeView_SelectEntry(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr)
{
    int isNew;
    Blt_HashEntry *hPtr;

    hPtr = Blt_CreateHashEntry(&tvPtr->selectTable, (char *)entryPtr, &isNew);
    if (isNew) {
	Blt_ChainLink link;

	link = Blt_Chain_Append(tvPtr->selected, entryPtr);
	Blt_SetHashValue(hPtr, link);
    }
}

void
Blt_TreeView_DeselectEntry(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&tvPtr->selectTable, (char *)entryPtr);
    if (hPtr != NULL) {
	Blt_ChainLink link;

	link = Blt_GetHashValue(hPtr);
	Blt_Chain_DeleteLink(tvPtr->selected, link);
	Blt_DeleteHashEntry(&tvPtr->selectTable, hPtr);
    }
}

const char *
Blt_TreeView_GetFullName(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr,
			 int checkEntryLabel, Tcl_DString *resultPtr)
{
    const char **names;		       /* Used the stack the component names. */
    const char *staticSpace[64+2];
    int level;
    int i;

    level = Blt_Tree_NodeDepth(entryPtr->node);
    if (tvPtr->rootPtr->labelUid == NULL) {
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
	    entryPtr = Blt_TreeView_NodeToEntry(tvPtr, node);
	}
    }
    Tcl_DStringInit(resultPtr);
    if (level >= 0) {
	if ((tvPtr->pathSep == SEPARATOR_LIST) || 
	    (tvPtr->pathSep == SEPARATOR_NONE)) {
	    for (i = 0; i <= level; i++) {
		Tcl_DStringAppendElement(resultPtr, names[i]);
	    }
	} else {
	    Tcl_DStringAppend(resultPtr, names[0], -1);
	    for (i = 1; i <= level; i++) {
		Tcl_DStringAppend(resultPtr, tvPtr->pathSep, -1);
		Tcl_DStringAppend(resultPtr, names[i], -1);
	    }
	}
    } else {
	if ((tvPtr->pathSep != SEPARATOR_LIST) &&
	    (tvPtr->pathSep != SEPARATOR_NONE)) {
	    Tcl_DStringAppend(resultPtr, tvPtr->pathSep, -1);
	}
    }
    if (names != staticSpace) {
	Blt_Free(names);
    }
    return Tcl_DStringValue(resultPtr);
}


int
Blt_TreeView_CloseEntry(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr)
{
    const char *cmd;

    if (entryPtr->flags & ENTRY_CLOSED) {
	return TCL_OK;			/* Entry is already closed. */
    }
    entryPtr->flags |= ENTRY_CLOSED;

    /*
     * Invoke the entry's "close" command, if there is one. Otherwise try the
     * treeview's global "close" command.
     */
    cmd = CHOOSE(tvPtr->closeCmd, entryPtr->closeCmd);
    if (cmd != NULL) {
	Tcl_DString dString;
	int result;

	Blt_TreeView_PercentSubst(tvPtr, entryPtr, cmd, &dString);
	Tcl_Preserve(entryPtr);
	result = Tcl_GlobalEval(tvPtr->interp, Tcl_DStringValue(&dString));
	Tcl_Release(entryPtr);
	Tcl_DStringFree(&dString);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    tvPtr->flags |= TV_LAYOUT;
    return TCL_OK;
}


int
Blt_TreeView_OpenEntry(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr)
{
    const char *cmd;

    if ((entryPtr->flags & ENTRY_CLOSED) == 0) {
	return TCL_OK;			/* Entry is already open. */
    }
    entryPtr->flags &= ~ENTRY_CLOSED;
    /*
     * If there's a "open" command proc specified for the entry, use that
     * instead of the more general "open" proc for the entire treeview.
     */
    cmd = CHOOSE(tvPtr->openCmd, entryPtr->openCmd);
    if (cmd != NULL) {
	Tcl_DString dString;
	int result;

	Blt_TreeView_PercentSubst(tvPtr, entryPtr, cmd, &dString);
	Tcl_Preserve(entryPtr);
	result = Tcl_GlobalEval(tvPtr->interp, Tcl_DStringValue(&dString));
	Tcl_Release(entryPtr);
	Tcl_DStringFree(&dString);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    tvPtr->flags |= TV_LAYOUT;
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
    Blt_TreeView *tvPtr,
    Blt_TreeNode node,			/* Node that has just been created. */
    int objc,
    Tcl_Obj *const *objv,
    int flags)
{
    Blt_TreeViewEntry *entryPtr;
    int isNew;
    Blt_HashEntry *hPtr;

    hPtr = Blt_CreateHashEntry(&tvPtr->entryTable, (char *)node, &isNew);
    if (isNew) {
	/* Create the entry structure */
	entryPtr = Blt_PoolAllocItem(tvPtr->entryPool, sizeof(Blt_TreeViewEntry));
	memset(entryPtr, 0, sizeof(Blt_TreeViewEntry));
	entryPtr->flags = (unsigned short)(tvPtr->buttonFlags | ENTRY_CLOSED);
	entryPtr->tvPtr = tvPtr;
	entryPtr->labelUid = NULL;
	entryPtr->node = node;
	Blt_SetHashValue(hPtr, entryPtr);

    } else {
	entryPtr = Blt_GetHashValue(hPtr);
    }
    if (Blt_TreeView_ConfigureEntry(tvPtr, entryPtr, objc, objv, flags) 
	!= TCL_OK) {
	FreeEntry(tvPtr, entryPtr);
	return TCL_ERROR;		/* Error configuring the entry. */
    }
    tvPtr->flags |= (TV_LAYOUT | TV_DIRTY);
    Blt_TreeView_EventuallyRedraw(tvPtr);
    return TCL_OK;
}


/*ARGSUSED*/
static int
CreateApplyProc(Blt_TreeNode node, ClientData clientData, int order)
{
    Blt_TreeView *tvPtr = clientData; 
    return Blt_TreeView_CreateEntry(tvPtr, node, 0, NULL, 0);
}

/*ARGSUSED*/
static int
DeleteApplyProc(Blt_TreeNode node, ClientData clientData, int order)
{
    Blt_TreeView *tvPtr = clientData;
    /* 
     * Unsetting the tree value triggers a call back to destroy the entry and
     * also releases the Tcl_Obj that contains it.
     */
    return Blt_Tree_UnsetValueByKey(tvPtr->interp, tvPtr->tree, node, 
	tvPtr->treeColumn.key);
}

static int
TreeEventProc(ClientData clientData, Blt_TreeNotifyEvent *eventPtr)
{
    Blt_TreeNode node;
    Blt_TreeView *tvPtr = clientData; 

    node = Blt_Tree_GetNode(eventPtr->tree, eventPtr->inode);
    switch (eventPtr->type) {
    case TREE_NOTIFY_CREATE:
	return Blt_TreeView_CreateEntry(tvPtr, node, 0, NULL, 0);
    case TREE_NOTIFY_DELETE:
	/*  
	 * Deleting the tree node triggers a call back to free the treeview
	 * entry that is associated with it.
	 */
	if (node != NULL) {
	    Blt_TreeViewEntry *entryPtr;

	    entryPtr = Blt_TreeView_FindEntry(tvPtr, node);
	    if (entryPtr != NULL) {
		FreeEntry(tvPtr, entryPtr);
	    }
	}
	break;
    case TREE_NOTIFY_RELABEL:
	if (node != NULL) {
	    Blt_TreeViewEntry *entryPtr;

	    entryPtr = Blt_TreeView_NodeToEntry(tvPtr, node);
	    entryPtr->flags |= ENTRY_DIRTY;
	}
	/*FALLTHRU*/
    case TREE_NOTIFY_MOVE:
    case TREE_NOTIFY_SORT:
	tvPtr->flags |= (TV_LAYOUT | TV_RESORT | TV_DIRTY);
	Blt_TreeView_EventuallyRedraw(tvPtr);
	break;
    default:
	/* empty */
	break;
    }	
    return TCL_OK;
}

Blt_TreeViewValue *
Blt_TreeView_FindValue(Blt_TreeViewEntry *entryPtr, Blt_TreeViewColumn *colPtr)
{
    Blt_TreeViewValue *vp;

    for (vp = entryPtr->values; vp != NULL; vp = vp->nextPtr) {
	if (vp->columnPtr == colPtr) {
	    return vp;
	}
    }
    return NULL;
}

void
Blt_TreeView_AddValue(Blt_TreeViewEntry *entryPtr, Blt_TreeViewColumn *colPtr)
{
    if (Blt_TreeView_FindValue(entryPtr, colPtr) == NULL) {
	Tcl_Obj *objPtr;

	if (Blt_TreeView_GetData(entryPtr, colPtr->key, &objPtr) == TCL_OK) {
	    Blt_TreeViewValue *valuePtr;

	    /* Add a new value only if a data entry exists. */
	    valuePtr = Blt_PoolAllocItem(entryPtr->tvPtr->valuePool, 
			 sizeof(Blt_TreeViewValue));
	    valuePtr->columnPtr = colPtr;
	    valuePtr->nextPtr = entryPtr->values;
	    valuePtr->textPtr = NULL;
	    valuePtr->width = valuePtr->height = 0;
	    valuePtr->stylePtr = NULL;
	    valuePtr->string = NULL;
	    entryPtr->values = valuePtr;
	}
    }
    entryPtr->tvPtr->flags |= (TV_LAYOUT | TV_DIRTY);
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
    Blt_TreeView *tvPtr = clientData; 
    Blt_TreeViewColumn *colPtr;
    Blt_TreeViewEntry *entryPtr;
    Blt_TreeViewValue *valuePtr, *nextPtr, *lastPtr;
    
    hPtr = Blt_FindHashEntry(&tvPtr->entryTable, (char *)node);
    if (hPtr == NULL) {
	return TCL_OK;			/* Not a node that we're interested
					 * in. */
    }
    entryPtr = Blt_GetHashValue(hPtr);
    flags &= TREE_TRACE_WRITE | TREE_TRACE_READ | TREE_TRACE_UNSET;
    switch (flags) {
    case TREE_TRACE_WRITE:
	hPtr = Blt_FindHashEntry(&tvPtr->columnTable, key);
	if (hPtr == NULL) {
	    return TCL_OK;		/* Data value isn't used by widget. */
	}
	colPtr = Blt_GetHashValue(hPtr);
	if (colPtr != &tvPtr->treeColumn) {
	    Blt_TreeView_AddValue(entryPtr, colPtr);
	}
	entryPtr->flags |= ENTRY_DIRTY;
	Blt_TreeView_EventuallyRedraw(tvPtr);
	tvPtr->flags |= (TV_LAYOUT | TV_DIRTY);
	break;

    case TREE_TRACE_UNSET:
	lastPtr = NULL;
	for(valuePtr = entryPtr->values; valuePtr != NULL; 
	    valuePtr = nextPtr) {
	    nextPtr = valuePtr->nextPtr;
	    if (valuePtr->columnPtr->key == key) { 
		Blt_TreeView_DestroyValue(tvPtr, valuePtr);
		if (lastPtr == NULL) {
		    entryPtr->values = nextPtr;
		} else {
		    lastPtr->nextPtr = nextPtr;
		}
		entryPtr->flags |= ENTRY_DIRTY;
		Blt_TreeView_EventuallyRedraw(tvPtr);
		tvPtr->flags |= (TV_LAYOUT | TV_DIRTY);
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
GetValueSize(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr, 
	     Blt_TreeViewValue *valuePtr, Blt_TreeViewStyle *stylePtr)
{
    Blt_TreeViewColumn *colPtr;

    colPtr = valuePtr->columnPtr;
    valuePtr->width = valuePtr->height = 0;
    if (entryPtr->flags & ENTRY_DIRTY) { /* Reparse the data. */
	char *string;
	Blt_TreeViewIcon icon;

	Tcl_Obj *objPtr;
	Blt_TreeViewStyle *newStylePtr;
    
	icon = NULL;
	newStylePtr = NULL;
	if (Blt_TreeView_GetData(entryPtr, valuePtr->columnPtr->key, &objPtr) 
	    != TCL_OK) {
	    return;			/* No data ??? */
	}
	string = Tcl_GetString(objPtr);
	valuePtr->string = string;
	if (string[0] == '@') {		/* Name of style or Tk image. */
	    int objc;
	    Tcl_Obj **objv;
	    
	    if ((Tcl_ListObjGetElements(tvPtr->interp, objPtr, &objc, 
		&objv) != TCL_OK) || (objc < 1) || (objc > 2)) {
		goto handleString;
	    }
	    if (objc > 0) {
		const char *name;
		
		name = Tcl_GetString(objv[0]) + 1;
		if (Blt_TreeView_GetStyle((Tcl_Interp *)NULL, tvPtr, name, 
					 &newStylePtr) != TCL_OK) {
		    icon = Blt_TreeView_GetIcon(tvPtr, name);
		    if (icon == NULL) {
			goto handleString;
		    }
		    /* Create a new style by the name of the image. */
		    newStylePtr = Blt_TreeView_CreateStyle((Tcl_Interp *)NULL, 
			tvPtr, STYLE_TEXTBOX, name);
		    assert(newStylePtr);
		    Blt_TreeView_UpdateStyleGCs(tvPtr, newStylePtr);
		}
		
	    }
	    if (valuePtr->stylePtr != NULL) {
		Blt_TreeView_FreeStyle(tvPtr, valuePtr->stylePtr);
	    }
	    if (icon != NULL) {
		Blt_TreeView_SetStyleIcon(tvPtr, newStylePtr, icon);
	    }
	    valuePtr->stylePtr = newStylePtr;
	    valuePtr->string = (objc > 1) ? Tcl_GetString(objv[1]) : NULL;
	}
    }
 handleString:
    stylePtr = CHOOSE(colPtr->stylePtr, valuePtr->stylePtr);
    /* Measure the text string. */
    (*stylePtr->classPtr->measProc)(tvPtr, stylePtr, valuePtr);
    if (Blt_Tree_NodeId(entryPtr->node) == 6169) {
	fprintf(stderr, "sizeof node is %d %d\n", valuePtr->width, 
		valuePtr->height);
    }
}

static void
GetRowExtents(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr, int *widthPtr, 
	      int *heightPtr)
{
    Blt_TreeViewValue *valuePtr;
    int valueWidth;			/* Width of individual value.  */
    int width, height;			/* Compute dimensions of row. */
    Blt_TreeViewStyle *stylePtr;

    width = height = 0;
    for (valuePtr = entryPtr->values; valuePtr != NULL; 
	valuePtr = valuePtr->nextPtr) {
	stylePtr = valuePtr->stylePtr;
	if (stylePtr == NULL) {
	    stylePtr = valuePtr->columnPtr->stylePtr;
	}
	if ((entryPtr->flags & ENTRY_DIRTY) || 
	    (stylePtr->flags & STYLE_DIRTY)) {
	    GetValueSize(tvPtr, entryPtr, valuePtr, stylePtr);
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
Blt_TreeViewEntry *
Blt_TreeView_NearestEntry(Blt_TreeView *tvPtr, int x, int y, int selectOne)
{
    Blt_TreeViewEntry *lastPtr;
    Blt_TreeViewEntry **p;

    /*
     * We implicitly can pick only visible entries.  So make sure that the
     * tree exists.
     */
    if (tvPtr->nVisible == 0) {
	return NULL;
    }
    if (y < tvPtr->titleHeight) {
	return (selectOne) ? tvPtr->visibleArr[0] : NULL;
    }
    /*
     * Since the entry positions were previously computed in world
     * coordinates, convert Y-coordinate from screen to world coordinates too.
     */
    y = WORLDY(tvPtr, y);
    lastPtr = tvPtr->visibleArr[0];
    for (p = tvPtr->visibleArr; *p != NULL; p++) {
	Blt_TreeViewEntry *entryPtr;

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
Blt_TreeView_EntryTag(Blt_TreeView *tvPtr, const char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;				/* Not used. */

    hPtr = Blt_CreateHashEntry(&tvPtr->entryTagTable, string, &isNew);
    return Blt_GetHashKey(&tvPtr->entryTagTable, hPtr);
}

ClientData
Blt_TreeView_ButtonTag(Blt_TreeView *tvPtr, const char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;				/* Not used. */

    hPtr = Blt_CreateHashEntry(&tvPtr->buttonTagTable, string, &isNew);
    return Blt_GetHashKey(&tvPtr->buttonTagTable, hPtr);
}

ClientData
Blt_TreeView_ColumnTag(Blt_TreeView *tvPtr, const char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;				/* Not used. */

    hPtr = Blt_CreateHashEntry(&tvPtr->columnTagTable, string, &isNew);
    return Blt_GetHashKey(&tvPtr->columnTagTable, hPtr);
}

#ifdef notdef
ClientData
Blt_TreeViewStyleTag(Blt_TreeView *tvPtr, const char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;				/* Not used. */

    hPtr = Blt_CreateHashEntry(&tvPtr->styleTagTable, string, &isNew);
    return Blt_GetHashKey(&tvPtr->styleTagTable, hPtr);
}
#endif

static void
AddIdsToList(Blt_TreeView *tvPtr, Blt_List ids, const char *string, 
	     TagProc *tagProc)
{
    int argc;
    const char **argv;
    
    if (Tcl_SplitList((Tcl_Interp *)NULL, string, &argc, &argv) == TCL_OK) {
	const char **p;

	for (p = argv; *p != NULL; p++) {
	    Blt_List_Append(ids, (*tagProc)(tvPtr, *p), 0);
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
    Blt_TreeView *tvPtr;

    tvPtr = Blt_GetBindingData(table);
    if (context == (ClientData)ITEM_ENTRY_BUTTON) {
	Blt_TreeViewEntry *entryPtr = object;

	Blt_List_Append(ids, Blt_TreeView_ButtonTag(tvPtr, "Button"), 0);
	if (entryPtr->tagsUid != NULL) {
	    AddIdsToList(tvPtr, ids, entryPtr->tagsUid, Blt_TreeView_ButtonTag);
	} else {
	    Blt_List_Append(ids, Blt_TreeView_ButtonTag(tvPtr, "Entry"), 0);
	    Blt_List_Append(ids, Blt_TreeView_ButtonTag(tvPtr, "all"), 0);
	}
    } else if (context == (ClientData)ITEM_COLUMN_TITLE) {
	Blt_TreeViewColumn *colPtr = object;

	Blt_List_Append(ids, (char *)colPtr, 0);
	if (colPtr->tagsUid != NULL) {
	    AddIdsToList(tvPtr, ids, colPtr->tagsUid, 
			 Blt_TreeView_ColumnTag);
	}
    } else if (context == ITEM_COLUMN_RULE) {
	Blt_List_Append(ids, Blt_TreeView_ColumnTag(tvPtr, "Rule"), 0);
    } else {
	Blt_TreeViewEntry *entryPtr = object;

	Blt_List_Append(ids, (char *)entryPtr, 0);
	if (entryPtr->tagsUid != NULL) {
	    AddIdsToList(tvPtr, ids, entryPtr->tagsUid, Blt_TreeView_EntryTag);
	} else if (context == ITEM_ENTRY){
	    Blt_List_Append(ids, Blt_TreeView_EntryTag(tvPtr, "Entry"), 0);
	    Blt_List_Append(ids, Blt_TreeView_EntryTag(tvPtr, "all"), 0);
	} else {
	    Blt_TreeViewValue *valuePtr = context;

	    if (valuePtr != NULL) {
		Blt_TreeViewStyle *stylePtr = valuePtr->stylePtr;

		if (stylePtr == NULL) {
		    stylePtr = valuePtr->columnPtr->stylePtr;
		}
		Blt_List_Append(ids, 
	            Blt_TreeView_EntryTag(tvPtr, stylePtr->name), 0);
		Blt_List_Append(ids, 
		    Blt_TreeView_EntryTag(tvPtr, valuePtr->columnPtr->key), 0);
		Blt_List_Append(ids, 
		    Blt_TreeView_EntryTag(tvPtr, stylePtr->classPtr->className),
		    0);
#ifndef notdef
		Blt_List_Append(ids, Blt_TreeView_EntryTag(tvPtr, "Entry"), 0);
		Blt_List_Append(ids, Blt_TreeView_EntryTag(tvPtr, "all"), 0);
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
    Blt_TreeView *tvPtr = clientData;
    Blt_TreeViewEntry *entryPtr;
    Blt_TreeViewColumn *colPtr;

    if (contextPtr != NULL) {
	*contextPtr = NULL;
    }
    if (tvPtr->flags & TV_DIRTY) {
	/* Can't trust the selected entry if nodes have been added or
	 * deleted. So recompute the layout. */
	if (tvPtr->flags & TV_LAYOUT) {
	    Blt_TreeView_ComputeLayout(tvPtr);
	} 
	ComputeVisibleEntries(tvPtr);
    }
    colPtr = Blt_TreeView_NearestColumn(tvPtr, x, y, contextPtr);
    if ((*contextPtr != NULL) && (tvPtr->flags & TV_SHOW_COLUMN_TITLES)) {
	return colPtr;
    }
    if (tvPtr->nVisible == 0) {
	return NULL;
    }
    entryPtr = Blt_TreeView_NearestEntry(tvPtr, x, y, FALSE);
    if (entryPtr == NULL) {
	return NULL;
    }
    x = WORLDX(tvPtr, x);
    y = WORLDY(tvPtr, y);
    if (contextPtr != NULL) {
	*contextPtr = ITEM_ENTRY;
	if (colPtr != NULL) {
	    Blt_TreeViewValue *valuePtr;

	    valuePtr = Blt_TreeView_FindValue(entryPtr, colPtr);
	    if (valuePtr != NULL) {
		Blt_TreeViewStyle *stylePtr;
		
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
	    Blt_TreeViewButton *buttonPtr = &tvPtr->button;
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
GetEntryExtents(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr)
{
    int entryWidth, entryHeight;
    int width, height;

    /*
     * FIXME: Use of DIRTY flag inconsistent.  When does it
     *	      mean "dirty entry"? When does it mean "dirty column"?
     *	      Does it matter? probably
     */
    if ((entryPtr->flags & ENTRY_DIRTY) || (tvPtr->flags & TV_UPDATE)) {
	Blt_Font font;
	Blt_FontMetrics fontMetrics;
	Blt_TreeViewIcon *icons;
	const char *label;

	entryPtr->iconWidth = entryPtr->iconHeight = 0;
	icons = CHOOSE(tvPtr->icons, entryPtr->icons);
	if (icons != NULL) {
	    int i;
	    
	    for (i = 0; i < 2; i++) {
		if (icons[i] == NULL) {
		    break;
		}
		if (entryPtr->iconWidth < Blt_TreeView_IconWidth(icons[i])) {
		    entryPtr->iconWidth = Blt_TreeView_IconWidth(icons[i]);
		}
		if (entryPtr->iconHeight < Blt_TreeView_IconHeight(icons[i])) {
		    entryPtr->iconHeight = Blt_TreeView_IconHeight(icons[i]);
		}
	    }
	}
	if ((icons == NULL) || (icons[0] == NULL)) {
	    entryPtr->iconWidth = DEF_ICON_WIDTH;
	    entryPtr->iconHeight = DEF_ICON_HEIGHT;
	}
	entryPtr->iconWidth += 2 * ICON_PADX;
	entryPtr->iconHeight += 2 * ICON_PADY;
	entryHeight = MAX(entryPtr->iconHeight, tvPtr->button.height);
	font = entryPtr->font;
	if (font == NULL) {
	    font = Blt_TreeView_GetStyleFont(tvPtr, tvPtr->treeColumn.stylePtr);
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
				    tvPtr->selBorderWidth) + tvPtr->leader;
	label = GETLABEL(entryPtr);
	if (label[0] == '\0') {
	    width = height = entryPtr->lineHeight;
	} else {
	    TextStyle ts;

	    Blt_Ts_InitStyle(ts);
	    Blt_Ts_SetFont(ts, font);
	    
	    if (tvPtr->flatView) {
		Tcl_DString dString;

		Blt_TreeView_GetFullName(tvPtr, entryPtr, TRUE, &dString);
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
	width += 2 * (FOCUS_WIDTH + LABEL_PADX + tvPtr->selBorderWidth);
	height += 2 * (FOCUS_WIDTH + LABEL_PADY + tvPtr->selBorderWidth);
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
    /*  
     * Find the maximum height of the data value entries. This also has the
     * side effect of contributing the maximum width of the column.
     */
    GetRowExtents(tvPtr, entryPtr, &width, &height);
    if (entryHeight < height) {
	entryHeight = height;
    }
    entryPtr->width = entryWidth + COLUMN_PAD;
    entryPtr->height = entryHeight + tvPtr->leader;
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
static Blt_TreeView *
CreateTreeView(Tcl_Interp *interp, Tcl_Obj *objPtr, const char *className)
{
    Tk_Window tkwin;
    Blt_TreeView *tvPtr;
    char *name;
    Tcl_DString dString;
    int result;

    name = Tcl_GetString(objPtr);
    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp), name,
	(char *)NULL);
    if (tkwin == NULL) {
	return NULL;

    }
    Tk_SetClass(tkwin, (char *)className);

    tvPtr = Blt_AssertCalloc(1, sizeof(Blt_TreeView));
    tvPtr->tkwin = tkwin;
    tvPtr->display = Tk_Display(tkwin);
    tvPtr->interp = interp;
    tvPtr->flags = (TV_HIDE_ROOT | TV_SHOW_COLUMN_TITLES | 
		    TV_DIRTY | TV_LAYOUT | TV_SETUP);
    tvPtr->leader = 0;
    tvPtr->dashes = 1;
    tvPtr->highlightWidth = 2;
    tvPtr->selBorderWidth = 1;
    tvPtr->borderWidth = 2;
    tvPtr->relief = TK_RELIEF_SUNKEN;
    tvPtr->selRelief = TK_RELIEF_FLAT;
    tvPtr->scrollMode = BLT_SCROLL_MODE_HIERBOX;
    tvPtr->selectMode = SELECT_MODE_SINGLE;
    tvPtr->button.closeRelief = tvPtr->button.openRelief = TK_RELIEF_SOLID;
    tvPtr->reqWidth = 200;
    tvPtr->reqHeight = 400;
    tvPtr->xScrollUnits = tvPtr->yScrollUnits = 20;
    tvPtr->lineWidth = 1;
    tvPtr->button.borderWidth = 1;
    tvPtr->columns = Blt_Chain_Create();
    tvPtr->buttonFlags = BUTTON_AUTO;
    tvPtr->selected = Blt_Chain_Create();
    tvPtr->userStyles = Blt_Chain_Create();
    tvPtr->sortColumnPtr = &tvPtr->treeColumn;
    Blt_InitHashTableWithPool(&tvPtr->entryTable, BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&tvPtr->columnTable, BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&tvPtr->iconTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&tvPtr->selectTable, BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&tvPtr->uidTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&tvPtr->styleTable, BLT_STRING_KEYS);
    tvPtr->bindTable = Blt_CreateBindingTable(interp, tkwin, tvPtr, PickItem, 
	GetTags);
    Blt_InitHashTable(&tvPtr->entryTagTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&tvPtr->columnTagTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&tvPtr->buttonTagTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&tvPtr->styleTagTable, BLT_STRING_KEYS);

    tvPtr->entryPool = Blt_PoolCreate(BLT_FIXED_SIZE_ITEMS);
    tvPtr->valuePool = Blt_PoolCreate(BLT_FIXED_SIZE_ITEMS);
    Blt_SetWindowInstanceData(tkwin, tvPtr);
    tvPtr->cmdToken = Tcl_CreateObjCommand(interp, Tk_PathName(tvPtr->tkwin), 
	Blt_TreeView_WidgetInstCmd, tvPtr, WidgetInstCmdDeleteProc);

    Tk_CreateSelHandler(tvPtr->tkwin, XA_PRIMARY, XA_STRING, SelectionProc,
	tvPtr, XA_STRING);
    Tk_CreateEventHandler(tvPtr->tkwin, ExposureMask | StructureNotifyMask |
	FocusChangeMask, TreeViewEventProc, tvPtr);
    /* 
     * Create a default style. This must exist before we can create
     * the treeview column. 
     */  
    tvPtr->stylePtr = Blt_TreeView_CreateStyle(interp, tvPtr, STYLE_TEXTBOX,
	"text");
    if (tvPtr->stylePtr == NULL) {
	return NULL;
    }
    /*
     * By default create a tree. The name will be the same as the widget
     * pathname.
     */
    tvPtr->tree = Blt_Tree_Open(interp, Tk_PathName(tvPtr->tkwin), TREE_CREATE);
    if (tvPtr->tree == NULL) {
	return NULL;
    }
    /* Create a default column to display the view of the tree. */
    Tcl_DStringInit(&dString);
    Tcl_DStringAppend(&dString, "BLT TreeView ", -1);
    Tcl_DStringAppend(&dString, Tk_PathName(tvPtr->tkwin), -1);
    result = Blt_TreeView_CreateColumn(tvPtr, &tvPtr->treeColumn, 
				      Tcl_DStringValue(&dString), "");
    Tcl_DStringFree(&dString);
    if (result != TCL_OK) {
	return NULL;
    }
    Blt_Chain_Append(tvPtr->columns, &tvPtr->treeColumn);
    return tvPtr;
}

static void
TeardownEntries(Blt_TreeView *tvPtr)
{
    Blt_HashSearch iter;
    Blt_HashEntry *hPtr;

    /* Release the current tree, removing any entry fields. */
    for (hPtr = Blt_FirstHashEntry(&tvPtr->entryTable, &iter); hPtr != NULL; 
	 hPtr = Blt_NextHashEntry(&iter)) {
	Blt_TreeViewEntry *entryPtr;

	entryPtr = Blt_GetHashValue(hPtr);
	DestroyEntry((ClientData)entryPtr);
    }
    Blt_DeleteHashTable(&tvPtr->entryTable);
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
    Blt_TreeView *tvPtr = (Blt_TreeView *)dataPtr;
    Blt_TreeViewButton *buttonPtr;
    Blt_TreeViewStyle *stylePtr;

    TeardownEntries(tvPtr);
    if (tvPtr->tree != NULL) {
	Blt_Tree_Close(tvPtr->tree);
    }
    bltTreeViewTreeOption.clientData = tvPtr;
    bltTreeViewIconsOption.clientData = tvPtr;
    Blt_FreeOptions(bltTreeViewSpecs, (char *)tvPtr, tvPtr->display, 0);
    if (tvPtr->tkwin != NULL) {
	Tk_DeleteSelHandler(tvPtr->tkwin, XA_PRIMARY, XA_STRING);
    }
    if (tvPtr->lineGC != NULL) {
	Tk_FreeGC(tvPtr->display, tvPtr->lineGC);
    }
    if (tvPtr->focusGC != NULL) {
	Blt_FreePrivateGC(tvPtr->display, tvPtr->focusGC);
    }
    if (tvPtr->visibleArr != NULL) {
	Blt_Free(tvPtr->visibleArr);
    }
    if (tvPtr->flatArr != NULL) {
	Blt_Free(tvPtr->flatArr);
    }
    if (tvPtr->levelInfo != NULL) {
	Blt_Free(tvPtr->levelInfo);
    }
    buttonPtr = &tvPtr->button;
    if (buttonPtr->activeGC != NULL) {
	Tk_FreeGC(tvPtr->display, buttonPtr->activeGC);
    }
    if (buttonPtr->normalGC != NULL) {
	Tk_FreeGC(tvPtr->display, buttonPtr->normalGC);
    }
    if (tvPtr->stylePtr != NULL) {
	Blt_TreeView_FreeStyle(tvPtr, tvPtr->stylePtr);
    }
    Blt_TreeView_DestroyColumns(tvPtr);
    Blt_DestroyBindingTable(tvPtr->bindTable);
    Blt_Chain_Destroy(tvPtr->selected);
    Blt_DeleteHashTable(&tvPtr->entryTagTable);
    Blt_DeleteHashTable(&tvPtr->columnTagTable);
    Blt_DeleteHashTable(&tvPtr->buttonTagTable);
    Blt_DeleteHashTable(&tvPtr->styleTagTable);

    /* Remove any user-specified style that might remain. */
    for (link = Blt_Chain_FirstLink(tvPtr->userStyles); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	stylePtr = Blt_Chain_GetValue(link);
	stylePtr->link = NULL;
	Blt_TreeView_FreeStyle(tvPtr, stylePtr);
    }
    Blt_Chain_Destroy(tvPtr->userStyles);
    if (tvPtr->comboWin != NULL) {
	Tk_DestroyWindow(tvPtr->comboWin);
    }
    Blt_DeleteHashTable(&tvPtr->styleTable);
    Blt_DeleteHashTable(&tvPtr->selectTable);
    Blt_DeleteHashTable(&tvPtr->uidTable);
    Blt_PoolDestroy(tvPtr->entryPool);
    Blt_PoolDestroy(tvPtr->valuePool);
    DumpIconTable(tvPtr);
    Blt_Free(tvPtr);
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
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 *---------------------------------------------------------------------------
 */
static void
TreeViewEventProc(ClientData clientData, XEvent *eventPtr)
{
    Blt_TreeView *tvPtr = clientData;

    if (eventPtr->type == Expose) {
	if (eventPtr->xexpose.count == 0) {
	    Blt_TreeView_EventuallyRedraw(tvPtr);
	    Blt_PickCurrentItem(tvPtr->bindTable);
	}
    } else if (eventPtr->type == ConfigureNotify) {
	tvPtr->flags |= (TV_LAYOUT | TV_SCROLL);
	Blt_TreeView_EventuallyRedraw(tvPtr);
    } else if ((eventPtr->type == FocusIn) || (eventPtr->type == FocusOut)) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    if (eventPtr->type == FocusIn) {
		tvPtr->flags |= TV_FOCUS;
	    } else {
		tvPtr->flags &= ~TV_FOCUS;
	    }
	    Blt_TreeView_EventuallyRedraw(tvPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (tvPtr->tkwin != NULL) {
	    tvPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(tvPtr->interp, tvPtr->cmdToken);
	}
	if (tvPtr->flags & TV_REDRAW) {
	    Tcl_CancelIdleCall(DisplayTreeView, tvPtr);
	}
	if (tvPtr->flags & TV_SELECT_PENDING) {
	    Tcl_CancelIdleCall(Blt_TreeView_SelectCmdProc, tvPtr);
	}
	Tcl_EventuallyFree(tvPtr, DestroyTreeView);
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
    Blt_TreeView *tvPtr = clientData;
    Blt_TreeViewEntry *entryPtr;
    int size;

    if ((tvPtr->flags & TV_SELECT_EXPORT) == 0) {
	return -1;
    }
    /*
     * Retrieve the names of the selected entries.
     */
    Tcl_DStringInit(&dString);
    if (tvPtr->flags & TV_SELECT_SORTED) {
	Blt_ChainLink link;

	for (link = Blt_Chain_FirstLink(tvPtr->selected); 
	     link != NULL; link = Blt_Chain_NextLink(link)) {
	    entryPtr = Blt_Chain_GetValue(link);
	    Tcl_DStringAppend(&dString, GETLABEL(entryPtr), -1);
	    Tcl_DStringAppend(&dString, "\n", -1);
	}
    } else {
	for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
	     entryPtr = Blt_TreeView_NextEntry(entryPtr, ENTRY_MASK)) {
	    if (Blt_TreeView_EntryIsSelected(tvPtr, entryPtr)) {
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
    Blt_TreeView *tvPtr = clientData;

    /*
     * This procedure could be invoked either because the window was destroyed
     * and the command was then deleted (in which case tkwin is NULL) or
     * because the command was deleted, and then this procedure destroys the
     * widget.
     */
    if (tvPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = tvPtr->tkwin;
	tvPtr->tkwin = NULL;
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
 *	set for tvPtr; old resources get freed, if there were any.  The widget
 *	is redisplayed.
 *
 *---------------------------------------------------------------------------
 */
int
Blt_TreeView_UpdateWidget(Tcl_Interp *interp, Blt_TreeView *tvPtr)	
{
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;

    /*
     * GC for dotted vertical line.
     */
    gcMask = (GCForeground | GCLineWidth);
    gcValues.foreground = tvPtr->lineColor->pixel;
    gcValues.line_width = tvPtr->lineWidth;
    if (tvPtr->dashes > 0) {
	gcMask |= (GCLineStyle | GCDashList);
	gcValues.line_style = LineOnOffDash;
	gcValues.dashes = tvPtr->dashes;
    }
    newGC = Tk_GetGC(tvPtr->tkwin, gcMask, &gcValues);
    if (tvPtr->lineGC != NULL) {
	Tk_FreeGC(tvPtr->display, tvPtr->lineGC);
    }
    tvPtr->lineGC = newGC;

    /*
     * GC for active label. Dashed outline.
     */
    gcMask = GCForeground | GCLineStyle;
    gcValues.foreground = tvPtr->focusColor->pixel;
    gcValues.line_style = (LineIsDashed(tvPtr->focusDashes))
	? LineOnOffDash : LineSolid;
    newGC = Blt_GetPrivateGC(tvPtr->tkwin, gcMask, &gcValues);
    if (LineIsDashed(tvPtr->focusDashes)) {
	tvPtr->focusDashes.offset = 2;
	Blt_SetDashes(tvPtr->display, newGC, &tvPtr->focusDashes);
    }
    if (tvPtr->focusGC != NULL) {
	Blt_FreePrivateGC(tvPtr->display, tvPtr->focusGC);
    }
    tvPtr->focusGC = newGC;

    Blt_TreeView_ConfigureButtons(tvPtr);
    tvPtr->inset = tvPtr->highlightWidth + tvPtr->borderWidth + INSET_PAD;

    /*
     * If the tree object was changed, we need to setup the new one.
     */
    if (Blt_ConfigModified(bltTreeViewSpecs, "-tree", (char *)NULL)) {
	TeardownEntries(tvPtr);
	Blt_InitHashTableWithPool(&tvPtr->entryTable, BLT_ONE_WORD_KEYS);
	Blt_TreeView_ClearSelection(tvPtr);
	if (Blt_Tree_Attach(interp, tvPtr->tree, tvPtr->treeName) != TCL_OK) {
	    return TCL_ERROR;
	}
	tvPtr->flags |= TV_SETUP;
    }

    /*
     * These options change the layout of the box.  Mark the widget for update.
     */
    if (Blt_ConfigModified(bltTreeViewSpecs, "-font", 
	"-linespacing", "-*width", "-height", "-hide*", "-tree", "-flat", 
	(char *)NULL)) {
	tvPtr->flags |= (TV_LAYOUT | TV_SCROLL);
    }
    /*
     * If the tree view was changed, mark all the nodes dirty (we'll be
     * switching back to either the full path name or the label) and free the
     * array representing the flattened view of the tree.
     */
    if (Blt_ConfigModified(bltTreeViewSpecs, "-hideleaves", "-flat",
			      (char *)NULL)) {
	Blt_TreeViewEntry *entryPtr;
	
	tvPtr->flags |= TV_DIRTY;
	/* Mark all entries dirty. */
	for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
	     entryPtr = Blt_TreeView_NextEntry(entryPtr, 0)) {
	    entryPtr->flags |= ENTRY_DIRTY;
	}
	if ((!tvPtr->flatView) && (tvPtr->flatArr != NULL)) {
	    Blt_Free(tvPtr->flatArr);
	    tvPtr->flatArr = NULL;
	}
    }

    if (tvPtr->flags & TV_SETUP) {
	Blt_TreeNode root;

	Blt_Tree_CreateEventHandler(tvPtr->tree, TREE_NOTIFY_ALL, TreeEventProc, 
		   tvPtr);
	TraceColumns(tvPtr);
	root = Blt_Tree_RootNode(tvPtr->tree);

	/* Automatically add view-entry values to the new tree. */
	Blt_Tree_Apply(root, CreateApplyProc, tvPtr);
	tvPtr->focusPtr = tvPtr->rootPtr = Blt_TreeView_NodeToEntry(tvPtr,root);
	tvPtr->selMarkPtr = tvPtr->selAnchorPtr = NULL;
	Blt_SetFocusItem(tvPtr->bindTable, tvPtr->rootPtr, ITEM_ENTRY);

	/* Automatically open the root node. */
	if (Blt_TreeView_OpenEntry(tvPtr, tvPtr->rootPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (tvPtr->flags & TV_NEW_TAGS) {
	    Blt_Tree_NewTagTable(tvPtr->tree);
	}
	tvPtr->flags &= ~TV_SETUP;
    }

    if (Blt_ConfigModified(bltTreeViewSpecs, "-font", "-color", 
	(char *)NULL)) {
	Blt_TreeView_UpdateColumnGCs(tvPtr, &tvPtr->treeColumn);
    }
    Blt_TreeView_EventuallyRedraw(tvPtr);
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
ResetCoordinates(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr, int *yPtr, 
		 int *indexPtr)
{
    int depth;

    entryPtr->worldY = -1;
    entryPtr->vertLineLength = -1;
    if ((entryPtr != tvPtr->rootPtr) && 
	(Blt_TreeView_EntryIsHidden(entryPtr))) {
	return;				/* If the entry is hidden, then do
					 * nothing. */
    }
    entryPtr->worldY = *yPtr;
    entryPtr->vertLineLength = -(*yPtr);
    *yPtr += entryPtr->height;
    entryPtr->flatIndex = *indexPtr;
    (*indexPtr)++;
    depth = DEPTH(tvPtr, entryPtr->node) + 1;
    if (tvPtr->levelInfo[depth].labelWidth < entryPtr->labelWidth) {
	tvPtr->levelInfo[depth].labelWidth = entryPtr->labelWidth;
    }
    if (tvPtr->levelInfo[depth].iconWidth < entryPtr->iconWidth) {
	tvPtr->levelInfo[depth].iconWidth = entryPtr->iconWidth;
    }
    tvPtr->levelInfo[depth].iconWidth |= 0x01;
    if ((entryPtr->flags & ENTRY_CLOSED) == 0) {
	Blt_TreeViewEntry *bottomPtr, *childPtr;

	bottomPtr = entryPtr;
	for (childPtr = Blt_TreeView_FirstChild(entryPtr, ENTRY_HIDE); 
	     childPtr != NULL; 
	     childPtr = Blt_TreeView_NextSibling(childPtr, ENTRY_HIDE)){
	    ResetCoordinates(tvPtr, childPtr, yPtr, indexPtr);
	    bottomPtr = childPtr;
	}
	entryPtr->vertLineLength += bottomPtr->worldY;
    }
}

static void
AdjustColumns(Blt_TreeView *tvPtr)
{
    Blt_ChainLink link;
    Blt_TreeViewColumn *colPtr;
    double weight;
    int nOpen;
    int size, avail, ration, growth;

    growth = VPORTWIDTH(tvPtr) - tvPtr->worldWidth;
    nOpen = 0;
    weight = 0.0;
    /* Find out how many columns still have space available */
    for (link = Blt_Chain_FirstLink(tvPtr->columns); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	colPtr = Blt_Chain_GetValue(link);
	if ((colPtr->hidden) || 
	    (colPtr->weight == 0.0) || 
	    (colPtr->width >= colPtr->max) || 
	    (colPtr->reqWidth > 0)) {
	    continue;
	}
	nOpen++;
	weight += colPtr->weight;
    }

    while ((nOpen > 0) && (weight > 0.0) && (growth > 0)) {
	ration = (int)(growth / weight);
	if (ration == 0) {
	    ration = 1;
	}
	for (link = Blt_Chain_FirstLink(tvPtr->columns); 
	     link != NULL; link = Blt_Chain_NextLink(link)) {
	    colPtr = Blt_Chain_GetValue(link);
	    if ((colPtr->hidden) || 
		(colPtr->weight == 0.0) ||
		(colPtr->width >= colPtr->max) || 
		(colPtr->reqWidth > 0)) {
		continue;
	    }
	    size = (int)(ration * colPtr->weight);
	    if (size > growth) {
		size = growth; 
	    }
	    avail = colPtr->max - colPtr->width;
	    if (size > avail) {
		size = avail;
		nOpen--;
		weight -= colPtr->weight;
	    }
	    growth -= size;
	    colPtr->width += size;
	}
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
ComputeFlatLayout(Blt_TreeView *tvPtr)
{
    Blt_ChainLink link;
    Blt_TreeViewColumn *colPtr;
    Blt_TreeViewEntry **p;
    Blt_TreeViewEntry *entryPtr;
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
    if (tvPtr->flags & (TV_DIRTY | TV_UPDATE)) {
	int position;

	/* Reset the positions of all the columns and initialize the column used
	 * to track the widest value. */
	position = 1;
	for (link = Blt_Chain_FirstLink(tvPtr->columns); 
	     link != NULL; link = Blt_Chain_NextLink(link)) {
	    colPtr = Blt_Chain_GetValue(link);
	    colPtr->maxWidth = 0;
	    colPtr->max = SHRT_MAX;
	    if (colPtr->reqMax > 0) {
		colPtr->max = colPtr->reqMax;
	    }
	    colPtr->position = position;
	    position++;
	}


	/* If the view needs to be resorted, free the old view. */
	if ((tvPtr->flags & (TV_RESORT | TV_SORT_PENDING | TV_SORT_AUTO)) && 
	     (tvPtr->flatArr != NULL)) {
	    Blt_Free(tvPtr->flatArr);
	    tvPtr->flatArr = NULL;
	}

	/* Recreate the flat view of all the open and not-hidden entries. */
	if (tvPtr->flatArr == NULL) {
	    count = 0;
	    /* Count the number of open entries to allocate for the array. */
	    for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
		entryPtr = Blt_TreeView_NextEntry(entryPtr, ENTRY_MASK)) {
		if ((tvPtr->flags & TV_HIDE_ROOT) && 
		    (entryPtr == tvPtr->rootPtr)) {
		    continue;
		}
		count++;
	    }
	    tvPtr->nEntries = count;

	    /* Allocate an array for the flat view. */
	    tvPtr->flatArr = Blt_AssertCalloc((count + 1), 
					      sizeof(Blt_TreeViewEntry *));
	    /* Fill the array with open and not-hidden entries */
	    p = tvPtr->flatArr;
	    for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
		entryPtr = Blt_TreeView_NextEntry(entryPtr, ENTRY_MASK)) {
		if ((tvPtr->flags & TV_HIDE_ROOT) && 
		    (entryPtr == tvPtr->rootPtr)) {
		    continue;
		}
		*p++ = entryPtr;
	    }
	    *p = NULL;
	    tvPtr->flags &= ~TV_SORTED;		/* Indicate the view isn't
						 * sorted. */
	}

	/* Collect the extents of the entries in the flat view. */
	tvPtr->depth = 0;
	tvPtr->minHeight = SHRT_MAX;
	for (p = tvPtr->flatArr; *p != NULL; p++) {
	    entryPtr = *p;
	    GetEntryExtents(tvPtr, entryPtr);
	    if (tvPtr->minHeight > entryPtr->height) {
		tvPtr->minHeight = entryPtr->height;
	    }
	    entryPtr->flags &= ~ENTRY_HAS_BUTTON;
	}
	if (tvPtr->levelInfo != NULL) {
	    Blt_Free(tvPtr->levelInfo);
	}
	tvPtr->levelInfo = Blt_AssertCalloc(tvPtr->depth+2, sizeof(LevelInfo));
	tvPtr->flags &= ~(TV_DIRTY | TV_UPDATE | TV_RESORT);
	if (tvPtr->flags & TV_SORT_AUTO) {
	    /* If we're auto-sorting, schedule the view to be resorted. */
	    tvPtr->flags |= TV_SORT_PENDING;
	}
    } 

    if (tvPtr->flags & TV_SORT_PENDING) {
	Blt_TreeView_SortFlatView(tvPtr);
    }

    tvPtr->levelInfo[0].labelWidth = tvPtr->levelInfo[0].x = 
	    tvPtr->levelInfo[0].iconWidth = 0;
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
    for(p = tvPtr->flatArr; *p != NULL; p++) {
	entryPtr = *p;
	entryPtr->flatIndex = count++;
	entryPtr->worldY = y;
	entryPtr->vertLineLength = 0;
	y += entryPtr->height;
	if (tvPtr->levelInfo[0].labelWidth < entryPtr->labelWidth) {
	    tvPtr->levelInfo[0].labelWidth = entryPtr->labelWidth;
	}
	if (tvPtr->levelInfo[0].iconWidth < entryPtr->iconWidth) {
	    tvPtr->levelInfo[0].iconWidth = entryPtr->iconWidth;
	}
    }
    tvPtr->levelInfo[0].iconWidth |= 0x01;
    tvPtr->worldHeight = y;		/* Set the scroll height of the
					 * hierarchy. */
    if (tvPtr->worldHeight < 1) {
	tvPtr->worldHeight = 1;
    }
    maxX = tvPtr->levelInfo[0].iconWidth + tvPtr->levelInfo[0].labelWidth;
    tvPtr->treeColumn.maxWidth = maxX;
    tvPtr->treeWidth = maxX;
    tvPtr->flags |= TV_VIEWPORT;
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
ComputeTreeLayout(Blt_TreeView *tvPtr)
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
    if (tvPtr->flags & TV_DIRTY) {
	Blt_ChainLink link;
	Blt_TreeViewEntry *ep;
	int position;

	position = 1;
	for (link = Blt_Chain_FirstLink(tvPtr->columns); 
	     link != NULL; link = Blt_Chain_NextLink(link)) {
	    Blt_TreeViewColumn *colPtr;

	    colPtr = Blt_Chain_GetValue(link);
	    colPtr->maxWidth = 0;
	    colPtr->max = SHRT_MAX;
	    if (colPtr->reqMax > 0) {
		colPtr->max = colPtr->reqMax;
	    }
	    colPtr->position = position;
	    position++;
	}
	tvPtr->minHeight = SHRT_MAX;
	tvPtr->depth = 0;
	for (ep = tvPtr->rootPtr; ep != NULL; 
	     ep = Blt_TreeView_NextEntry(ep, 0)){
	    GetEntryExtents(tvPtr, ep);
	    if (tvPtr->minHeight > ep->height) {
		tvPtr->minHeight = ep->height;
	    }
	    /* 
	     * Determine if the entry should display a button (indicating that
	     * it has children) and mark the entry accordingly.
	     */
	    ep->flags &= ~ENTRY_HAS_BUTTON;
	    if (ep->flags & BUTTON_SHOW) {
		ep->flags |= ENTRY_HAS_BUTTON;
	    } else if (ep->flags & BUTTON_AUTO) {
		if (tvPtr->flags & TV_HIDE_LEAVES) {
		    /* Check that a non-leaf child exists */
		    if (Blt_TreeView_FirstChild(ep, ENTRY_HIDE) != NULL) {
			ep->flags |= ENTRY_HAS_BUTTON;
		    }
		} else if (!Blt_Tree_IsLeaf(ep->node)) {
		    ep->flags |= ENTRY_HAS_BUTTON;
		}
	    }
	    /* Determine the depth of the tree. */
	    if (tvPtr->depth < DEPTH(tvPtr, ep->node)) {
		tvPtr->depth = DEPTH(tvPtr, ep->node);
	    }
	}
	if (tvPtr->flags & TV_SORT_PENDING) {
	    Blt_TreeView_SortView(tvPtr);
	}
	if (tvPtr->levelInfo != NULL) {
	    Blt_Free(tvPtr->levelInfo);
	}
	tvPtr->levelInfo = Blt_AssertCalloc(tvPtr->depth+2, sizeof(LevelInfo));
	tvPtr->flags &= ~(TV_DIRTY | TV_RESORT);
    }
    { 
	size_t i;

	for (i = 0; i <= (tvPtr->depth + 1); i++) {
	    tvPtr->levelInfo[i].labelWidth = tvPtr->levelInfo[i].x = 
		tvPtr->levelInfo[i].iconWidth = 0;
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
    if (tvPtr->flags & TV_HIDE_ROOT) {
	/* If the root entry is to be hidden, cheat by offsetting the
	 * y-coordinates by the height of the entry. */
	y = -(tvPtr->rootPtr->height);
    } 
    index = 0;
    ResetCoordinates(tvPtr, tvPtr->rootPtr, &y, &index);
    tvPtr->worldHeight = y;		/* Set the scroll height of the
					 * hierarchy. */
    if (tvPtr->worldHeight < 1) {
	tvPtr->worldHeight = 1;
    }
    {
	int maxX;
	int sum;
	size_t i;

	sum = maxX = 0;
	i = 0;
	for (/*empty*/; i <= (tvPtr->depth + 1); i++) {
	    int x;

	    sum += tvPtr->levelInfo[i].iconWidth;
	    if (i <= tvPtr->depth) {
		tvPtr->levelInfo[i + 1].x = sum;
	    }
	    x = sum;
	    if (((tvPtr->flags & TV_HIDE_ROOT) == 0) || (i > 1)) {
		x += tvPtr->levelInfo[i].labelWidth;
	    }
	    if (x > maxX) {
		maxX = x;
	    }
	}
	tvPtr->treeColumn.maxWidth = maxX;
	tvPtr->treeWidth = maxX;
    }
}

static void
LayoutColumns(Blt_TreeView *tvPtr)
{
    Blt_ChainLink link;
    int sum;

    /* The width of the widget (in world coordinates) is the sum of the column
     * widths. */

    tvPtr->worldWidth = tvPtr->titleHeight = 0;
    sum = 0;
    for (link = Blt_Chain_FirstLink(tvPtr->columns); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	Blt_TreeViewColumn *cp;

	cp = Blt_Chain_GetValue(link);
	cp->width = 0;
	if (!cp->hidden) {
	    if ((tvPtr->flags & TV_SHOW_COLUMN_TITLES) &&
		(tvPtr->titleHeight < cp->titleHeight)) {
		tvPtr->titleHeight = cp->titleHeight;
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
	}
	cp->worldX = sum;
	sum += cp->width;
    }
    tvPtr->worldWidth = sum;
    if (VPORTWIDTH(tvPtr) > sum) {
	AdjustColumns(tvPtr);
    }
    sum = 0;
    for (link = Blt_Chain_FirstLink(tvPtr->columns); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	Blt_TreeViewColumn *cp;

	cp = Blt_Chain_GetValue(link);
	cp->worldX = sum;
	sum += cp->width;
    }
    if (tvPtr->titleHeight > 0) {
	/* If any headings are displayed, add some extra padding to the
	 * height. */
	tvPtr->titleHeight += 4;
    }
    /* tvPtr->worldWidth += 10; */
    if (tvPtr->yScrollUnits < 1) {
	tvPtr->yScrollUnits = 1;
    }
    if (tvPtr->xScrollUnits < 1) {
	tvPtr->xScrollUnits = 1;
    }
    if (tvPtr->worldWidth < 1) {
	tvPtr->worldWidth = 1;
    }
    tvPtr->flags &= ~TV_LAYOUT;
    tvPtr->flags |= TV_SCROLL;
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
Blt_TreeView_ComputeLayout(Blt_TreeView *tvPtr)
{
    Blt_ChainLink link;
    Blt_TreeViewColumn *colPtr;
    Blt_TreeViewEntry *entryPtr;
    Blt_TreeViewValue *valuePtr;

    if (tvPtr->flatView) {
	ComputeFlatLayout(tvPtr);
    } else {
	ComputeTreeLayout(tvPtr);
    }

    /*
     * Determine the width of each column based upon the entries that as open
     * (not hidden).  The widest entry in a column determines the width of that
     * column.
     */

    /* Initialize the columns. */
    for (link = Blt_Chain_FirstLink(tvPtr->columns); 
	 link != NULL; link = Blt_Chain_NextLink(link)) {
	colPtr = Blt_Chain_GetValue(link);
	colPtr->maxWidth = 0;
	colPtr->max = SHRT_MAX;
	if (colPtr->reqMax > 0) {
	    colPtr->max = colPtr->reqMax;
	}
    }
    /* The treeview column width was computed earlier. */
    tvPtr->treeColumn.maxWidth = tvPtr->treeWidth;

    /* 
     * Look at all open entries and their values.  Determine the column widths
     * by tracking the maximum width value in each column.
     */
    for (entryPtr = tvPtr->rootPtr; entryPtr != NULL; 
	 entryPtr = Blt_TreeView_NextEntry(entryPtr, ENTRY_MASK)) {
	for (valuePtr = entryPtr->values; valuePtr != NULL; 
	     valuePtr = valuePtr->nextPtr) {
	    if (valuePtr->columnPtr->maxWidth < valuePtr->width) {
		valuePtr->columnPtr->maxWidth = valuePtr->width;
	    }
	}	    
    }
    /* Now layout the columns with the proper sizes. */
    LayoutColumns(tvPtr);
}

#ifdef notdef
static void
PrintFlags(Blt_TreeView *tvPtr, char *string)
{    
    fprintf(stderr, "%s: flags=", string);
    if (tvPtr->flags & TV_LAYOUT) {
	fprintf(stderr, "layout ");
    }
    if (tvPtr->flags & TV_REDRAW) {
	fprintf(stderr, "redraw ");
    }
    if (tvPtr->flags & TV_XSCROLL) {
	fprintf(stderr, "xscroll ");
    }
    if (tvPtr->flags & TV_YSCROLL) {
	fprintf(stderr, "yscroll ");
    }
    if (tvPtr->flags & TV_FOCUS) {
	fprintf(stderr, "focus ");
    }
    if (tvPtr->flags & TV_DIRTY) {
	fprintf(stderr, "dirty ");
    }
    if (tvPtr->flags & TV_UPDATE) {
	fprintf(stderr, "update ");
    }
    if (tvPtr->flags & TV_RESORT) {
	fprintf(stderr, "resort ");
    }
    if (tvPtr->flags & TV_SORTED) {
	fprintf(stderr, "sorted ");
    }
    if (tvPtr->flags & TV_SORT_PENDING) {
	fprintf(stderr, "sort_pending ");
    }
    if (tvPtr->flags & TV_BORDERS) {
	fprintf(stderr, "borders ");
    }
    if (tvPtr->flags & TV_VIEWPORT) {
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
ComputeVisibleEntries(Blt_TreeView *tvPtr)
{
    int height;
    int nSlots;
    int maxX;
    int xOffset, yOffset;

    xOffset = Blt_AdjustViewport(tvPtr->xOffset, tvPtr->worldWidth,
	VPORTWIDTH(tvPtr), tvPtr->xScrollUnits, tvPtr->scrollMode);
    yOffset = Blt_AdjustViewport(tvPtr->yOffset, 
	tvPtr->worldHeight, VPORTHEIGHT(tvPtr), tvPtr->yScrollUnits, 
	tvPtr->scrollMode);

    if ((xOffset != tvPtr->xOffset) || (yOffset != tvPtr->yOffset)) {
	tvPtr->yOffset = yOffset;
	tvPtr->xOffset = xOffset;
	tvPtr->flags |= TV_VIEWPORT;
    }
    height = VPORTHEIGHT(tvPtr);

    /* Allocate worst case number of slots for entry array. */
    nSlots = (height / tvPtr->minHeight) + 3;
    if (nSlots != tvPtr->nVisible) {
	if (tvPtr->visibleArr != NULL) {
	    Blt_Free(tvPtr->visibleArr);
	}
	tvPtr->visibleArr = Blt_AssertCalloc(nSlots, sizeof(Blt_TreeViewEntry *));
    }
    tvPtr->nVisible = 0;
    tvPtr->visibleArr[0] = NULL;

    if (tvPtr->rootPtr->flags & ENTRY_HIDE) {
	return TCL_OK;			/* Root node is hidden. */
    }
    /* Find the node where the view port starts. */
    if (tvPtr->flatView) {
	Blt_TreeViewEntry **epp;

	/* Find the starting entry visible in the viewport. It can't be hidden
	 * or any of it's ancestors closed. */
    again:
	for (epp = tvPtr->flatArr; *epp != NULL; epp++) {
	    if (((*epp)->worldY + (*epp)->height) > tvPtr->yOffset) {
		break;
	    }
	}	    
	/*
	 * If we can't find the starting node, then the view must be scrolled
	 * down, but some nodes were deleted.  Reset the view back to the top
	 * and try again.
	 */
	if (*epp == NULL) {
	    if (tvPtr->yOffset == 0) {
		return TCL_OK;		/* All entries are hidden. */
	    }
	    tvPtr->yOffset = 0;
	    goto again;
	}

	maxX = 0;
	height += tvPtr->yOffset;
	for (/*empty*/; *epp != NULL; epp++) {
	    int x;

	    (*epp)->worldX = LEVELX(0) + tvPtr->treeColumn.worldX;
	    x = (*epp)->worldX + ICONWIDTH(0) + (*epp)->width;
	    if (x > maxX) {
		maxX = x;
	    }
	    if ((*epp)->worldY >= height) {
		break;
	    }
	    tvPtr->visibleArr[tvPtr->nVisible] = *epp;
	    tvPtr->nVisible++;
	}
	tvPtr->visibleArr[tvPtr->nVisible] = NULL;
    } else {
	Blt_TreeViewEntry *ep;

	ep = tvPtr->rootPtr;
	while ((ep->worldY + ep->height) <= tvPtr->yOffset) {
	    for (ep = Blt_TreeView_LastChild(ep, ENTRY_HIDE); ep != NULL; 
		 ep = Blt_TreeView_PrevSibling(ep, ENTRY_HIDE)) {
		if (ep->worldY <= tvPtr->yOffset) {
		    break;
		}
	    }
	    /*
	     * If we can't find the starting node, then the view must be
	     * scrolled down, but some nodes were deleted.  Reset the view
	     * back to the top and try again.
	     */
	    if (ep == NULL) {
		if (tvPtr->yOffset == 0) {
		    return TCL_OK;	/* All entries are hidden. */
		}
		tvPtr->yOffset = 0;
		continue;
	    }
	}
	

	height += tvPtr->yOffset;
	maxX = 0;
	tvPtr->treeColumn.maxWidth = tvPtr->treeWidth;

	for (/*empty*/; ep != NULL; ep = Blt_TreeView_NextEntry(ep, ENTRY_MASK)){
	    int x;
	    int level;

	    /*
	     * Compute and save the entry's X-coordinate now that we know the
	     * maximum level offset for the entire widget.
	     */
	    level = DEPTH(tvPtr, ep->node);
	    ep->worldX = LEVELX(level) + tvPtr->treeColumn.worldX;
	    x = ep->worldX + ICONWIDTH(level) + ICONWIDTH(level + 1) + ep->width;
	    if (x > maxX) {
		maxX = x;
	    }
	    if (ep->worldY >= height) {
		break;
	    }
	    tvPtr->visibleArr[tvPtr->nVisible] = ep;
	    tvPtr->nVisible++;
	}
	tvPtr->visibleArr[tvPtr->nVisible] = NULL;
    }
    /*
     * Note:	It's assumed that the view port always starts at or
     *		over an entry.  Check that a change in the hierarchy
     *		(e.g. closing a node) hasn't left the viewport beyond
     *		the last entry.  If so, adjust the viewport to start
     *		on the last entry.
     */
    if (tvPtr->xOffset > (tvPtr->worldWidth - tvPtr->xScrollUnits)) {
	tvPtr->xOffset = tvPtr->worldWidth - tvPtr->xScrollUnits;
    }
    if (tvPtr->yOffset > (tvPtr->worldHeight - tvPtr->yScrollUnits)) {
	tvPtr->yOffset = tvPtr->worldHeight - tvPtr->yScrollUnits;
    }
    tvPtr->xOffset = Blt_AdjustViewport(tvPtr->xOffset, 
	tvPtr->worldWidth, VPORTWIDTH(tvPtr), tvPtr->xScrollUnits, 
	tvPtr->scrollMode);
    tvPtr->yOffset = Blt_AdjustViewport(tvPtr->yOffset,
	tvPtr->worldHeight, VPORTHEIGHT(tvPtr), tvPtr->yScrollUnits,
	tvPtr->scrollMode);

    tvPtr->flags &= ~TV_DIRTY;
    Blt_PickCurrentItem(tvPtr->bindTable);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * DrawVerticals --
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
DrawVerticals(
    Blt_TreeView *tvPtr,		/* Widget record containing the
					 * attribute information for buttons. */
    Blt_TreeViewEntry *entryPtr,	/* Entry to be drawn. */
    Drawable drawable)			/* Pixmap or window to draw into. */
{
    while (entryPtr != tvPtr->rootPtr) {
	int level;
	
	entryPtr = Blt_TreeView_ParentEntry(entryPtr);
	if (entryPtr == NULL) {
	    break;
	}
	level = DEPTH(tvPtr, entryPtr->node);
	if (entryPtr->vertLineLength > 0) {
	    int ax, ay, bx, by;
	    int x, y;
	    int height;

	    /*
	     * World X-coordinates aren't computed only for entries that are
	     * outside the view port.  So for each off-screen ancestor node
	     * compute it here too.
	     */
	    entryPtr->worldX = LEVELX(level) + tvPtr->treeColumn.worldX;
	    x = SCREENX(tvPtr, entryPtr->worldX);
	    y = SCREENY(tvPtr, entryPtr->worldY);
	    height = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
			  tvPtr->button.height);
	    y += (height - tvPtr->button.height) / 2;
	    ax = bx = x + ICONWIDTH(level) + ICONWIDTH(level + 1) / 2;
	    ay = y + tvPtr->button.height / 2;
	    by = ay + entryPtr->vertLineLength;
	    if ((entryPtr == tvPtr->rootPtr) && (tvPtr->flags & TV_HIDE_ROOT)) {
		ay += entryPtr->height;
	    }
	    /*
	     * Clip the line's Y-coordinates at the viewport's borders.
	     */
	    if (ay < 0) {
		ay = (ay & 0x1);	/* Make sure the dotted line starts on
					 * the same even/odd pixel. */
	    }
	    if (by > Tk_Height(tvPtr->tkwin)) {
		by = Tk_Height(tvPtr->tkwin);
	    }
	    if ((ay < Tk_Height(tvPtr->tkwin)) && (by > 0)) {
		XDrawLine(tvPtr->display, drawable, tvPtr->lineGC, 
			  ax, ay, bx, by);
	    }
	}
    }
}

void
Blt_TreeView_DrawRule(
    Blt_TreeView *tvPtr,		/* Widget record containing the
					 * attribute information for rules. */
    Blt_TreeViewColumn *colPtr,
    Drawable drawable)			/* Pixmap or window to draw into. */
{
    int x, y1, y2;

    x = SCREENX(tvPtr, colPtr->worldX) + 
	colPtr->width + tvPtr->ruleMark - tvPtr->ruleAnchor - 1;

    y1 = tvPtr->titleHeight + tvPtr->inset;
    y2 = Tk_Height(tvPtr->tkwin) - tvPtr->inset;
    XDrawLine(tvPtr->display, drawable, colPtr->ruleGC, x, y1, x, y2);
    tvPtr->flags = TOGGLE(tvPtr->flags, TV_RULE_ACTIVE);
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
    Blt_TreeView *tvPtr,		/* Widget record containing the
					 * attribute information for buttons. */
    Blt_TreeViewEntry *entryPtr,	/* Entry. */
    Drawable drawable,			/* Pixmap or window to draw into. */
    int x, int y)
{
    Blt_Background bg;
    Blt_TreeViewButton *buttonPtr = &tvPtr->button;
    Blt_TreeViewIcon icon;
    int relief;
    int width, height;

    bg = (entryPtr == tvPtr->activeBtnPtr) 
	? buttonPtr->activeBg : buttonPtr->bg;
    relief = (entryPtr->flags & ENTRY_CLOSED) 
	? buttonPtr->closeRelief : buttonPtr->openRelief;
    if (relief == TK_RELIEF_SOLID) {
	relief = TK_RELIEF_FLAT;
    }
    Blt_FillBackgroundRectangle(tvPtr->tkwin, drawable, bg, x, y,
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
	Tk_RedrawImage(Blt_TreeView_IconBits(icon), 0, 0, width, height, 
		drawable, x, y);
    } else {
	int top, bottom, left, right;
	XSegment segments[6];
	int count;
	GC gc;

	gc = (entryPtr == tvPtr->activeBtnPtr)
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
	XDrawSegments(tvPtr->display, drawable, gc, segments, count);
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
Blt_TreeViewIcon
Blt_TreeView_GetEntryIcon(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr)
{
    Blt_TreeViewIcon *icons;
    Blt_TreeViewIcon icon;

    int isActive, hasFocus;

    isActive = (entryPtr == tvPtr->activePtr);
    hasFocus = (entryPtr == tvPtr->focusPtr);
    icons = NULL;
    if (isActive) {
	icons = CHOOSE(tvPtr->activeIcons, entryPtr->activeIcons);
    }
    if (icons == NULL) {
	icons = CHOOSE(tvPtr->icons, entryPtr->icons);

    }
    icon = NULL;
    if (icons != NULL) {	/* Selected or normal icon? */
	icon = icons[0];
	if ((hasFocus) && (icons[1] != NULL)) {
	    icon = icons[1];
	}
    }
    return icon;
}


int
Blt_TreeView_DrawIcon(
    Blt_TreeView *tvPtr,		/* Widget record containing the
					 * attribute information for buttons. */
    Blt_TreeViewEntry *entryPtr,	/* Entry to display. */
    Drawable drawable,			/* Pixmap or window to draw into. */
    int x, int y)
{
    Blt_TreeViewIcon icon;

    icon = Blt_TreeView_GetEntryIcon(tvPtr, entryPtr);

    if (icon != NULL) {			/* Icon or default icon bitmap? */
	int entryHeight;
	int level;
	int maxY;
	int top, bottom;
	int topInset, botInset;
	int width, height;

	level = DEPTH(tvPtr, entryPtr->node);
	entryHeight = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
		tvPtr->button.height);
	height = Blt_TreeView_IconHeight(icon);
	width = Blt_TreeView_IconWidth(icon);
	if (tvPtr->flatView) {
	    x += (ICONWIDTH(0) - width) / 2;
	} else {
	    x += (ICONWIDTH(level + 1) - width) / 2;
	}	    
	y += (entryHeight - height) / 2;
	botInset = tvPtr->inset - INSET_PAD;
	topInset = tvPtr->titleHeight + tvPtr->inset;
	maxY = Tk_Height(tvPtr->tkwin) - botInset;
	top = 0;
	bottom = y + height;
	if (y < topInset) {
	    height += y - topInset;
	    top = -y + topInset;
	    y = topInset;
	} else if (bottom >= maxY) {
	    height = maxY - y;
	}
	Tk_RedrawImage(Blt_TreeView_IconBits(icon), 0, top, width, height, 
		drawable, x, y);
    } 
    return (icon != NULL);
}

static int
DrawLabel(
    Blt_TreeView *tvPtr,		/* Widget record. */
    Blt_TreeViewEntry *entryPtr,	/* Entry attribute information. */
    Drawable drawable,			/* Pixmap or window to draw into. */
    int x, int y,
    int maxLength)			
{
    const char *label;
    int entryHeight;
    int width, height;			/* Width and height of label. */
    int isFocused, isSelected, isActive;

    entryHeight = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
       tvPtr->button.height);
    isFocused = ((entryPtr == tvPtr->focusPtr) && 
		 (tvPtr->flags & TV_FOCUS));
    isSelected = Blt_TreeView_EntryIsSelected(tvPtr, entryPtr);
    isActive = (entryPtr == tvPtr->activePtr);

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

	    color = SELECT_FG(tvPtr);
	    XSetForeground(tvPtr->display, tvPtr->focusGC, color->pixel);
	}
	if (width > maxLength) {
	    width = maxLength;
	}
	XDrawRectangle(tvPtr->display, drawable, tvPtr->focusGC, x, y, 
		       width - 1, height - 1);
	if (isSelected) {
	    XSetForeground(tvPtr->display, tvPtr->focusGC, 
		tvPtr->focusColor->pixel);
	}
    }
    x += FOCUS_WIDTH + LABEL_PADX + tvPtr->selBorderWidth;
    y += FOCUS_WIDTH + LABEL_PADY + tvPtr->selBorderWidth;

    label = GETLABEL(entryPtr);
    if (label[0] != '\0') {
	Blt_TreeViewStyle *stylePtr;
	TextStyle ts;
	Blt_Font font;
	XColor *color;

	stylePtr = tvPtr->treeColumn.stylePtr;
	font = entryPtr->font;
	if (font == NULL) {
	    font = Blt_TreeView_GetStyleFont(tvPtr, stylePtr);
	}
	if (isSelected) {
	    color = SELECT_FG(tvPtr);
	} else if (entryPtr->color != NULL) {
	    color = entryPtr->color;
	} else {
	    color = Blt_TreeView_GetStyleFg(tvPtr, stylePtr);
	}
	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, font);
	Blt_Ts_SetForeground(ts, color);
	Blt_Ts_DrawLayout(tvPtr->tkwin, drawable, entryPtr->textPtr, &ts, 
		x, y, maxLength);
	if (isActive) {
	    Blt_Ts_UnderlineLayout(tvPtr->tkwin, drawable, entryPtr->textPtr, 
		&ts, x, y, maxLength);
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
    Blt_TreeView *tvPtr,		/* Widget record containing the
					 * attribute information for buttons. */
    Blt_TreeViewEntry *entryPtr,	/* Entry to be drawn. */
    Drawable drawable)			/* Pixmap or window to draw into. */
{
    int level;
    int x, y, xMax;

    entryPtr->flags &= ~ENTRY_REDRAW;

    x = SCREENX(tvPtr, entryPtr->worldX);
    y = SCREENY(tvPtr, entryPtr->worldY);
    if (!Blt_TreeView_DrawIcon(tvPtr, entryPtr, drawable, x, y)) {
	x -= (DEF_ICON_WIDTH * 2) / 3;
    }
    level = 0;
    x += ICONWIDTH(level);
    /* Entry label. */
    xMax = SCREENX(tvPtr, tvPtr->treeColumn.worldX) + tvPtr->treeColumn.width - 
	tvPtr->treeColumn.titleBorderWidth - tvPtr->treeColumn.pad.side2;
    DrawLabel(tvPtr, entryPtr, drawable, x, y, xMax - x);
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
DrawTreeEntry(
    Blt_TreeView *tvPtr,		/* Widget record. */
    Blt_TreeViewEntry *entryPtr,	/* Entry to be drawn. */
    Drawable drawable)			/* Pixmap or window to draw into. */
{
    Blt_TreeViewButton *buttonPtr = &tvPtr->button;
    int buttonY;
    int level;
    int width, height;
    int x, y, xMax;
    int x1, y1, x2, y2;

    entryPtr->flags &= ~ENTRY_REDRAW;

    x = SCREENX(tvPtr, entryPtr->worldX);
    y = SCREENY(tvPtr, entryPtr->worldY);

    level = DEPTH(tvPtr, entryPtr->node);
    width = ICONWIDTH(level);
    height = MAX3(entryPtr->lineHeight, entryPtr->iconHeight, 
	buttonPtr->height);

    entryPtr->buttonX = (width - buttonPtr->width) / 2;
    entryPtr->buttonY = (height - buttonPtr->height) / 2;

    buttonY = y + entryPtr->buttonY;

    x1 = x + (width / 2);
    y1 = y2 = buttonY + (buttonPtr->height / 2);
    x2 = x1 + (ICONWIDTH(level) + ICONWIDTH(level + 1)) / 2;

    if ((Blt_Tree_ParentNode(entryPtr->node) != NULL) && 
	(tvPtr->lineWidth > 0)) {
	/*
	 * For every node except root, draw a horizontal line from the
	 * vertical bar to the middle of the icon.
	 */
	XDrawLine(tvPtr->display, drawable, tvPtr->lineGC, x1, y1, x2, y2);
    }
    if (((entryPtr->flags & ENTRY_CLOSED) == 0) && (tvPtr->lineWidth > 0) &&
	(entryPtr->vertLineLength > 0)) {
	/* Entry is open, draw vertical line. */
	y2 = y1 + entryPtr->vertLineLength;
	if (y2 > Tk_Height(tvPtr->tkwin)) {
	    y2 = Tk_Height(tvPtr->tkwin); /* Clip line at window border. */
	}
	XDrawLine(tvPtr->display, drawable, tvPtr->lineGC, x2, y1, x2, y2);
    }
    if ((entryPtr->flags & ENTRY_HAS_BUTTON) && (entryPtr != tvPtr->rootPtr)) {
	/*
	 * Except for the root, draw a button for every entry that needs one.
	 * The displayed button can be either an icon (Tk image) or a line
	 * drawing (rectangle with plus or minus sign).
	 */
	Blt_TreeView_DrawButton(tvPtr, entryPtr, drawable, x + entryPtr->buttonX,
		y + entryPtr->buttonY);
    }
    x += ICONWIDTH(level);

    if (!Blt_TreeView_DrawIcon(tvPtr, entryPtr, drawable, x, y)) {
	x -= (DEF_ICON_WIDTH * 2) / 3;
    }
    x += ICONWIDTH(level + 1);

    /* Entry label. */
    xMax = SCREENX(tvPtr, tvPtr->treeColumn.worldX) + tvPtr->treeColumn.width - 
	tvPtr->treeColumn.titleBorderWidth - tvPtr->treeColumn.pad.side2;
    DrawLabel(tvPtr, entryPtr, drawable, x, y, xMax - x);
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
    Blt_TreeView *tvPtr,		/* Widget record. */
    Blt_TreeViewEntry *entryPtr,	/* Node of entry to be drawn. */
    Blt_TreeViewValue *valuePtr,
    Drawable drawable,			/* Pixmap or window to draw into. */
    int x, int y)
{
    Blt_TreeViewStyle *stylePtr;

    stylePtr = CHOOSE(valuePtr->columnPtr->stylePtr, valuePtr->stylePtr);
    (*stylePtr->classPtr->drawProc)(tvPtr, drawable, entryPtr, valuePtr, 
		stylePtr, x, y);
}

static void
DrawTitle(
    Blt_TreeView *tvPtr,
    Blt_TreeViewColumn *cp,
    Drawable drawable,							
    int x)				/* Starting x-coordinate of the column.
					 * May be off-screen because of
					 * scrolling. */
{
    GC gc;
    Blt_Background bg;
    XColor *fgColor;
    int drawWidth, drawX;
    int avail, need;
    int startX, arrowX;
    int needArrow;

    if (tvPtr->titleHeight < 1) {
	return;
    }
    startX = drawX = x;
    drawWidth = cp->width;
    if (cp->position == Blt_Chain_GetLength(tvPtr->columns)) {
	/* If there's any room left over, let the last column take it. */
	drawWidth = Tk_Width(tvPtr->tkwin) - x;
    } else if (cp->position == 1) {
	drawWidth += x;
	drawX = 0;
    }
    if (cp == tvPtr->activeTitleColumnPtr) {
	bg = cp->activeTitleBg;
	gc = cp->activeTitleGC;
	fgColor = cp->activeTitleFgColor;
    } else {
	bg = cp->titleBg;
	gc = cp->titleGC;
	fgColor = cp->titleFgColor;
    }
    /* Clear the title area by drawing the background. */
    Blt_FillBackgroundRectangle(tvPtr->tkwin, drawable, bg, drawX, 
	tvPtr->inset, drawWidth, tvPtr->titleHeight, 0, TK_RELIEF_FLAT);

    x += cp->pad.side1 + cp->titleBorderWidth;
    startX = x;
    needArrow = ((cp == tvPtr->sortColumnPtr) && (tvPtr->flatView));
    avail = cp->width - (2 * cp->titleBorderWidth) - PADDING(cp->pad);
    need = cp->titleWidth - (2 * cp->titleBorderWidth) - TV_ARROW_WIDTH;
    if (avail > need) {
	x += (avail - need) / 2;
    }
    arrowX = 0;			/* Suppress compiler warning. */
    if (needArrow) {
	arrowX = drawX + cp->width - cp->pad.side2 - TV_ARROW_WIDTH;
	if ((x + need) > arrowX) {
	    x -= (x + need) - arrowX;
	}
	if (x < startX) {
	    avail -= (startX - x);
	    x = startX;
	}
    }
    if (cp->titleIcon != NULL) {
	int iconX, iconY, iconWidth, iconHeight;

	iconHeight = Blt_TreeView_IconHeight(cp->titleIcon);
	iconWidth = Blt_TreeView_IconWidth(cp->titleIcon);
	iconX = x;

	/* Center the icon vertically.  We already know the column title is at
	 * least as tall as the icon. */
	iconY = tvPtr->inset + (tvPtr->titleHeight - iconHeight) / 2;

	Tk_RedrawImage(Blt_TreeView_IconBits(cp->titleIcon), 0, 0, iconWidth, 
		iconHeight, drawable, iconX, iconY);
	x += iconWidth + 6;
	avail -= iconWidth + 6;
    }
    if (cp->titleTextPtr != NULL) {
	TextStyle ts;

	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, cp->titleFont);
	Blt_Ts_SetForeground(ts, fgColor);
	Blt_Ts_DrawLayout(tvPtr->tkwin, drawable, cp->titleTextPtr, &ts, 
		x, tvPtr->inset + 1, avail);
    }
    if (needArrow) {
	Blt_DrawArrow(tvPtr->display, drawable, gc, arrowX, tvPtr->inset,  
		TV_ARROW_WIDTH, tvPtr->titleHeight, cp->titleBorderWidth,
		(tvPtr->sortDecreasing) ? ARROW_UP : ARROW_DOWN);
    }
    Blt_DrawBackgroundRectangle(tvPtr->tkwin, drawable, bg, drawX, tvPtr->inset,
	drawWidth, tvPtr->titleHeight, cp->titleBorderWidth, cp->titleRelief);
}

void
Blt_TreeView_DrawHeadings(Blt_TreeView *tvPtr, Drawable drawable)
{
    Blt_ChainLink link;
    Blt_TreeViewColumn *cp;
    int x;

    for (link = Blt_Chain_FirstLink(tvPtr->columns); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	cp = Blt_Chain_GetValue(link);
	if (cp->hidden) {
	    continue;
	}
	x = SCREENX(tvPtr, cp->worldX);
	if ((x + cp->width) < 0) {
	    continue;			/* Don't draw columns before the left
					 * edge. */
	}
	if (x > Tk_Width(tvPtr->tkwin)) {
	    break;			/* Discontinue when a column starts
					 * beyond the right edge. */
	}
	DrawTitle(tvPtr, cp, drawable, x);
    }
}

static void
DrawNormalBackground(Blt_TreeView *tvPtr, Drawable drawable, int x, int w)
{
    Blt_Background bg;

    bg = Blt_TreeView_GetStyleBackground(tvPtr, tvPtr->stylePtr);
    /* This also fills the background where there are no entries. */
    Blt_FillBackgroundRectangle(tvPtr->tkwin, drawable, bg, x, 0, w, 
	Tk_Height(tvPtr->tkwin), 0, TK_RELIEF_FLAT);
    if (tvPtr->altBg != NULL) {
	Blt_TreeViewEntry **epp;
	int count;

 	for (count = 0, epp = tvPtr->visibleArr; *epp != NULL; epp++, count++) {
	    if ((*epp)->flatIndex & 0x1) {
		int y;

		y = SCREENY(tvPtr, (*epp)->worldY) - 1;
		Blt_FillBackgroundRectangle(tvPtr->tkwin, drawable, 
			tvPtr->altBg, x, y, w, (*epp)->height + 1, 
			tvPtr->selBorderWidth, tvPtr->selRelief);
	    }
	}
    }
}

static void
DrawSelectionBackground(Blt_TreeView *tvPtr, Drawable drawable, int x, int w)
{
    Blt_TreeViewEntry **epp;

    /* 
     * Draw the backgrounds of selected entries first.  The vertical lines
     * connecting child entries will be draw on top.
     */
    for (epp = tvPtr->visibleArr; *epp != NULL; epp++) {
	if (Blt_TreeView_EntryIsSelected(tvPtr, *epp)) {
	    Blt_Background bg;
	    int y;

	    y = SCREENY(tvPtr, (*epp)->worldY) - 1;
	    bg = SELECT_FOCUS_BG(tvPtr);
	    Blt_FillBackgroundRectangle(tvPtr->tkwin, drawable, bg, x, y, w,
		(*epp)->height + 1, tvPtr->selBorderWidth, tvPtr->selRelief);
	}
    }
}

static void
DrawTreeView(Blt_TreeView *tvPtr, Drawable drawable, int x)
{
    Blt_TreeViewEntry **epp;

    if ((tvPtr->lineWidth > 0) && (tvPtr->nVisible > 0)) { 
	/* Draw all the vertical lines from topmost node. */
	DrawVerticals(tvPtr, tvPtr->visibleArr[0], drawable);
    }

    for (epp = tvPtr->visibleArr; *epp != NULL; epp++) {
	DrawTreeEntry(tvPtr, *epp, drawable);
    }
}


static void
DrawFlatView(
    Blt_TreeView *tvPtr,
    Drawable drawable,
    int x)
{
    Blt_TreeViewEntry **epp;

    for (epp = tvPtr->visibleArr; *epp != NULL; epp++) {
	DrawFlatEntry(tvPtr, *epp, drawable);
    }
}

void
Blt_TreeView_DrawOuterBorders(Blt_TreeView *tvPtr, Drawable drawable)
{
    /* Draw 3D border just inside of the focus highlight ring. */
    if (tvPtr->borderWidth > 0) {
	Blt_DrawBackgroundRectangle(tvPtr->tkwin, drawable, tvPtr->bg,
	    tvPtr->highlightWidth, tvPtr->highlightWidth,
	    Tk_Width(tvPtr->tkwin) - 2 * tvPtr->highlightWidth,
	    Tk_Height(tvPtr->tkwin) - 2 * tvPtr->highlightWidth,
	    tvPtr->borderWidth, tvPtr->relief);
    }
    /* Draw focus highlight ring. */
    if (tvPtr->highlightWidth > 0) {
	XColor *color;
	GC gc;

	color = (tvPtr->flags & TV_FOCUS)
	    ? tvPtr->highlightColor : tvPtr->highlightBgColor;
	gc = Tk_GCForColor(color, drawable);
	Tk_DrawFocusHighlight(tvPtr->tkwin, gc, tvPtr->highlightWidth,
	    drawable);
    }
    tvPtr->flags &= ~TV_BORDERS;
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
    Blt_TreeView *tvPtr = clientData;
    Pixmap drawable; 
    int reqWidth, reqHeight;

    tvPtr->flags &= ~TV_REDRAW;
    if (tvPtr->tkwin == NULL) {
	return;				/* Window has been destroyed. */
    }
    if (tvPtr->flags & TV_LAYOUT) {
	/*
	 * Recompute the layout when entries are opened/closed,
	 * inserted/deleted, or when text attributes change (such as font,
	 * linespacing).
	 */
	Blt_TreeView_ComputeLayout(tvPtr);
    }
    if (tvPtr->flags & (TV_SCROLL | TV_DIRTY)) {
	int width, height;
	/* 
	 * Scrolling means that the view port has changed and that the visible
	 * entries need to be recomputed.
	 */
	ComputeVisibleEntries(tvPtr);

	width = VPORTWIDTH(tvPtr);
	height = VPORTHEIGHT(tvPtr);
	if (tvPtr->flags & TV_XSCROLL) {
	    if (tvPtr->xScrollCmdObjPtr != NULL) {
		Blt_UpdateScrollbar(tvPtr->interp, tvPtr->xScrollCmdObjPtr,
		    (double)tvPtr->xOffset / tvPtr->worldWidth,
		    (double)(tvPtr->xOffset + width) / tvPtr->worldWidth);
	    }
	}
	if (tvPtr->flags & TV_YSCROLL) {
	    if (tvPtr->yScrollCmdObjPtr != NULL) {
		Blt_UpdateScrollbar(tvPtr->interp, tvPtr->yScrollCmdObjPtr,
		    (double)tvPtr->yOffset / tvPtr->worldHeight,
		    (double)(tvPtr->yOffset + height) / tvPtr->worldHeight);
	    }
	}
	tvPtr->flags &= ~TV_SCROLL;
    }

    reqHeight = (tvPtr->reqHeight > 0) ? tvPtr->reqHeight : 
	tvPtr->worldHeight + tvPtr->titleHeight + 2 * tvPtr->inset + 1;
    reqWidth = (tvPtr->reqWidth > 0) ? tvPtr->reqWidth : 
	tvPtr->worldWidth + 2 * tvPtr->inset;
    if ((reqWidth != Tk_ReqWidth(tvPtr->tkwin)) || 
	(reqHeight != Tk_ReqHeight(tvPtr->tkwin))) {
	Tk_GeometryRequest(tvPtr->tkwin, reqWidth, reqHeight);
    }
#ifdef notdef
    if (tvPtr->reqWidth == 0) {
	int w;
	/* 
	 * The first time through this routine, set the requested width to the
	 * computed width.  All we want is to automatically set the width of
	 * the widget, not dynamically grow/shrink it as attributes change.
	 */
	w = tvPtr->worldWidth + 2 * tvPtr->inset;
	Tk_GeometryRequest(tvPtr->tkwin, w, tvPtr->reqHeight);
    }
#endif
    if (!Tk_IsMapped(tvPtr->tkwin)) {
	return;
    }
    drawable = Tk_GetPixmap(tvPtr->display, Tk_WindowId(tvPtr->tkwin), 
	Tk_Width(tvPtr->tkwin), Tk_Height(tvPtr->tkwin), 
	Tk_Depth(tvPtr->tkwin));

    if ((tvPtr->focusPtr == NULL) && (tvPtr->nVisible > 0)) {
	/* Re-establish the focus entry at the top entry. */
	tvPtr->focusPtr = tvPtr->visibleArr[0];
    }
    tvPtr->flags |= TV_VIEWPORT;
    if ((tvPtr->flags & TV_RULE_ACTIVE) && (tvPtr->resizeColumnPtr != NULL)) {
	Blt_TreeView_DrawRule(tvPtr, tvPtr->resizeColumnPtr, drawable);
    }
    for (link = Blt_Chain_FirstLink(tvPtr->columns); link != NULL; 
	 link = Blt_Chain_NextLink(link)) {
	Blt_TreeViewColumn *colPtr;
	int x;

	colPtr = Blt_Chain_GetValue(link);
	colPtr->flags &= ~COLUMN_DIRTY;
	if (colPtr->hidden) {
	    continue;
	}
	x = SCREENX(tvPtr, colPtr->worldX);
	if ((x + colPtr->width) < 0) {
	    continue;			/* Don't draw columns before the left
					 * edge. */
	}
	if (x > Tk_Width(tvPtr->tkwin)) {
	    break;			/* Discontinue when a column starts
					 * beyond the right edge. */
	}
	/* Clear the column background. */
	DrawNormalBackground(tvPtr, drawable, x, colPtr->width);
	if (colPtr != &tvPtr->treeColumn) {
	    Blt_TreeViewEntry **epp;
	    
	    DrawSelectionBackground(tvPtr, drawable, x, colPtr->width);
	    for (epp = tvPtr->visibleArr; *epp != NULL; epp++) {
		Blt_TreeViewValue *vp;
		
		/* Check if there's a corresponding value in the entry. */
		vp = Blt_TreeView_FindValue(*epp, colPtr);
		if (vp != NULL) {
		    int y;

		    y = SCREENY(tvPtr, (*epp)->worldY) - 1;
		    Blt_TreeView_DrawValue(tvPtr, *epp, vp, drawable, 
			x + colPtr->pad.side1, y);
		}
	    }
	} else {
	    DrawSelectionBackground(tvPtr, drawable, x, 
				    tvPtr->treeColumn.width);
	    if (tvPtr->flatView) {
		DrawFlatView(tvPtr, drawable, x);
	    } else {
		DrawTreeView(tvPtr, drawable, x);
	    }
	}
 	if (colPtr->relief != TK_RELIEF_FLAT) {
	    Blt_Background bg;

	    /* Draw a 3D border around the column. */
	    bg = Blt_TreeView_GetStyleBackground(tvPtr, tvPtr->stylePtr);
	    Blt_DrawBackgroundRectangle(tvPtr->tkwin, drawable, bg, x, 0, 
		colPtr->width, Tk_Height(tvPtr->tkwin), colPtr->borderWidth, 
		colPtr->relief);
	}
    }
    if (tvPtr->flags & TV_SHOW_COLUMN_TITLES) {
	Blt_TreeView_DrawHeadings(tvPtr, drawable);
    }
    Blt_TreeView_DrawOuterBorders(tvPtr, drawable);
    if ((tvPtr->flags & TV_RULE_NEEDED) &&
	(tvPtr->resizeColumnPtr != NULL)) {
	Blt_TreeView_DrawRule(tvPtr, tvPtr->resizeColumnPtr, drawable);
    }
    /* Now copy the new view to the window. */
    XCopyArea(tvPtr->display, drawable, Tk_WindowId(tvPtr->tkwin), 
	tvPtr->lineGC, 0, 0, Tk_Width(tvPtr->tkwin), Tk_Height(tvPtr->tkwin), 
	0, 0);
    Tk_FreePixmap(tvPtr->display, drawable);
    tvPtr->flags &= ~TV_VIEWPORT;
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
    Blt_TreeView *tvPtr = clientData;

    Tcl_Preserve(tvPtr);
    if (tvPtr->selectCmd != NULL) {
	tvPtr->flags &= ~TV_SELECT_PENDING;
	if (Tcl_GlobalEval(tvPtr->interp, tvPtr->selectCmd) != TCL_OK) {
	    Tcl_BackgroundError(tvPtr->interp);
	}
    }
    Tcl_Release(tvPtr);
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
    Blt_TreeView *tvPtr;
    Tcl_CmdInfo cmdInfo;
    Tcl_Obj *initObjv[2];
    const char *className;
    char *string;
    int result;

    string = Tcl_GetString(objv[0]);
    if (objc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", string, 
		" pathName ?option value?...\"", (char *)NULL);
	return TCL_ERROR;
    }
    className = (string[0] == 'h') ? "Hiertable" : "TreeView";
    tvPtr = CreateTreeView(interp, objv[1], className);
    if (tvPtr == NULL) {
	goto error;
    }

    /*
     * Invoke a procedure to initialize various bindings on treeview entries.
     * If the procedure doesn't already exist, source it from
     * "$blt_library/treeview.tcl".  We deferred sourcing the file until now so
     * that the variable $blt_library could be set within a script.
     */
    if (!Tcl_GetCommandInfo(interp, "::blt::tv::Initialize", &cmdInfo)) {
	char cmd[200];
	sprintf_s(cmd, 200, "set className %s\n\
source [file join $blt_library treeview.tcl]\n\
unset className\n", className);
	if (Tcl_GlobalEval(interp, cmd) != TCL_OK) {
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
    bltTreeViewIconsOption.clientData = tvPtr;
    bltTreeViewTreeOption.clientData = tvPtr;
    if (Blt_ConfigureWidgetFromObj(interp, tvPtr->tkwin, bltTreeViewSpecs, 
	objc - 2, objv + 2, (char *)tvPtr, 0) != TCL_OK) {
	goto error;
    }
    if (Blt_ConfigureComponentFromObj(interp, tvPtr->tkwin, "button", "Button",
	 bltTreeViewButtonSpecs, 0, (Tcl_Obj **)NULL, (char *)tvPtr, 0) != TCL_OK) {
	goto error;
    }

    /* 
     * Rebuild the widget's GC and other resources that are predicated by the
     * widget's configuration options.  Do the same for the default column.
     */
    if (Blt_TreeView_UpdateWidget(interp, tvPtr) != TCL_OK) {
	goto error;
    }
    Blt_TreeView_UpdateColumnGCs(tvPtr, &tvPtr->treeColumn);
    Blt_TreeView_UpdateStyleGCs(tvPtr, tvPtr->stylePtr);

    /*
     * Invoke a procedure to initialize various bindings on treeview entries.
     * If the procedure doesn't already exist, source it from
     * "$blt_library/treeview.tcl".  We deferred sourcing the file until now
     * so that the variable $blt_library could be set within a script.
     */
    initObjv[0] = Tcl_NewStringObj("::blt::tv::Initialize", -1);
    initObjv[1] = objv[1];
    Tcl_IncrRefCount(initObjv[0]);
    Tcl_IncrRefCount(initObjv[1]);
    result = Tcl_EvalObjv(interp, 2, initObjv, TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(initObjv[1]);
    Tcl_DecrRefCount(initObjv[0]);
    if (result != TCL_OK) {
	goto error;
    }
    Tcl_SetStringObj(Tcl_GetObjResult(interp), Tk_PathName(tvPtr->tkwin), -1);
    return TCL_OK;
  error:
    if (tvPtr != NULL) {
	Tk_DestroyWindow(tvPtr->tkwin);
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
