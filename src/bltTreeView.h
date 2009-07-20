
/*
 * bltTreeView.h --
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

#ifndef BLT_TREEVIEW_H
#define BLT_TREEVIEW_H

#include "bltImage.h"
#include "bltHash.h"
#include "bltFont.h"
#include "bltText.h"
#include "bltChain.h"
#include "bltTree.h"
#include "bltTile.h"
#include "bltBind.h"
#include "bltBgStyle.h"

#define ITEM_ENTRY		(ClientData)0
#define ITEM_ENTRY_BUTTON	(ClientData)1
#define ITEM_COLUMN_TITLE	(ClientData)2
#define ITEM_COLUMN_RULE	(ClientData)3
#define ITEM_STYLE		(ClientData)0x10004

#if HAVE_UTF
#else
#define Tcl_NumUtfChars(s,n)	(((n) == -1) ? strlen((s)) : (n))
#define Tcl_UtfAtIndex(s,i)	((s) + (i))
#endif

#define ODD(x)			((x) | 0x01)

#define END			(-1)
#define SEPARATOR_LIST		((char *)NULL)
#define SEPARATOR_NONE		((char *)-1)

#define SEARCH_Y		1

#define TV_ARROW_WIDTH 17
#define TV_ARROW_HEIGHT 17

typedef const char *UID;

/*
 * The macro below is used to modify a "char" value (e.g. by casting it to an
 * unsigned character) so that it can be used safely with macros such as
 * isspace.
 */
#define UCHAR(c)	((unsigned char) (c))

#define TOGGLE(x, mask) (((x) & (mask)) ? ((x) & ~(mask)) : ((x) | (mask)))


#define SCREENX(h, wx)	((wx) - (h)->xOffset + (h)->inset)
#define SCREENY(h, wy)	((wy) - (h)->yOffset + (h)->inset + (h)->titleHeight)

#define WORLDX(h, sx)	((sx) - (h)->inset + (h)->xOffset)
#define WORLDY(h, sy)	((sy) - ((h)->inset + (h)->titleHeight) + (h)->yOffset)

#define VPORTWIDTH(h)	(Tk_Width((h)->tkwin) - 2 * (h)->inset)
#define VPORTHEIGHT(h) \
	(Tk_Height((h)->tkwin) - (h)->titleHeight - 2 * (h)->inset)

#define ICONWIDTH(d)	(tvPtr->levelInfo[(d)].iconWidth)
#define LEVELX(d)	(tvPtr->levelInfo[(d)].x)

#define DEPTH(h, n)	(((h)->flatView) ? 0 : Blt_Tree_NodeDepth(n))

#define SELECT_FG(t)	\
   (((((t)->flags & TV_FOCUS)) || ((t)->selOutFocusFgColor == NULL)) \
	? (t)->selInFocusFgColor : (t)->selOutFocusFgColor)
#define SELECT_FOCUS_BG(t)	\
   (((((t)->flags & TV_FOCUS)) || ((t)->selOutFocusBg == NULL)) \
	? (t)->selInFocusBg : (t)->selOutFocusBg)

#define SELECT_MODE_SINGLE	(1<<0)
#define SELECT_MODE_MULTIPLE	(1<<1)

/*
 *---------------------------------------------------------------------------
 *
 *  Internal treeview widget flags:
 *
 *	TV_LAYOUT	The layout of the hierarchy needs to be recomputed.
 *
 *	TV_REDRAW	A redraw request is pending for the widget.
 *
 *	TV_XSCROLL	X-scroll request is pending.
 *
 *	TV_YSCROLL	Y-scroll request is pending.
 *
 *	TV_SCROLL	Both X-scroll and  Y-scroll requests are pending.
 *
 *	TV_FOCUS	The widget is receiving keyboard events.
 *			Draw the focus highlight border around the widget.
 *
 *	TV_DIRTY	The hierarchy has changed. It may invalidate
 *			the locations and pointers to entries.  The widget 
 *			will need to recompute its layout.
 *
 *	TV_RESORT	The tree has changed such that the view needs to
 *			be resorted.  This can happen when an entry is
 *			open or closed, it's label changes, a column value
 *			changes, etc.
 *
 *	TV_BORDERS      The borders of the widget (highlight ring and
 *			3-D border) need to be redrawn.
 *
 *	TV_VIEWPORT     Indicates that the viewport has changed in some
 *			way: the size of the viewport, the location of 
 *			the viewport, or the contents of the viewport.
 *
 */

#define TV_LAYOUT	(1<<0)
#define TV_REDRAW	(1<<1)
#define TV_XSCROLL	(1<<2)
#define TV_YSCROLL	(1<<3)
#define TV_SCROLL	(TV_XSCROLL | TV_YSCROLL)
#define TV_FOCUS	(1<<4)
#define TV_DIRTY	(1<<5)
#define TV_UPDATE	(1<<6)
#define TV_RESORT	(1<<7)
#define TV_SORTED	(1<<8)
#define TV_SORT_PENDING (1<<9)
#define TV_BORDERS	(1<<10)
#define TV_VIEWPORT	(1<<11)
#define TV_SETUP	(1<<12)

/*
 *  Rule related flags: Rules are XOR-ed lines. We need to track whether
 *			they have been drawn or not. 
 *
 *	TV_RULE_ACTIVE  Indicates that a rule is currently being drawn
 *			for a column.
 *			
 *
 *	TV_RULE_NEEDED  Indicates that a rule is needed (but not yet
 *			drawn) for a column.
 */

#define TV_RULE_ACTIVE	(1<<15)
#define TV_RULE_NEEDED	(1<<16)

/*
 *  Selection related flags:
 *
 *	TV_SELECT_Blt_Export	Export the selection to X11.
 *
 *	TV_SELECT_PENDING	A "selection" command idle task is pending.
 *
 *	TV_SELECT_CLEAR		Clear selection flag of entry.
 *
 *	TV_SELECT_SET		Set selection flag of entry.
 *
 *	TV_SELECT_TOGGLE	Toggle selection flag of entry.
 *
 *	TV_SELECT_MASK		Mask of selection set/clear/toggle flags.
 *
 *	TV_SELECT_SORTED	Indicates if the entries in the selection 
 *				should be sorted or displayed in the order 
 *				they were selected.
 *
 */
#define TV_SELECT_CLEAR		(1<<16)
#define TV_SELECT_EXPORT	(1<<17) 
#define TV_SELECT_PENDING	(1<<18)
#define TV_SELECT_SET		(1<<19)
#define TV_SELECT_TOGGLE	(TV_SELECT_SET | TV_SELECT_CLEAR)
#define TV_SELECT_MASK		(TV_SELECT_SET | TV_SELECT_CLEAR)
#define TV_SELECT_SORTED	(1<<20)

/*
 *  Miscellaneous flags:
 *
 *	TV_ALLOW_DUPLICATES	When inserting new entries, create 
 *			        duplicate entries.
 *
 *	TV_FILL_ANCESTORS	Automatically create ancestor entries 
 *				as needed when inserting a new entry.
 *
 *	TV_HIDE_ROOT		Don't display the root entry.
 *
 *	TV_HIDE_LEAVES		Don't display entries that are leaves.
 *
 *	TV_SHOW_COLUMN_TITLES	Indicates whether to draw titles over each
 *				column.
 *
 */
#define TV_ALLOW_DUPLICATES	(1<<21)
#define TV_FILL_ANCESTORS	(1<<22)
#define TV_HIDE_ROOT		(1<<23) 
#define TV_HIDE_LEAVES		(1<<24) 
#define TV_SHOW_COLUMN_TITLES	(1<<25)
#define TV_SORT_AUTO		(1<<26)
#define TV_NEW_TAGS		(1<<27)
#define TV_HIGHLIGHT_CELLS	(1<<28)

#define TV_ITEM_COLUMN	1
#define TV_ITEM_RULE	2

/*
 *---------------------------------------------------------------------------
 *
 *  Internal entry flags:
 *
 *	ENTRY_HAS_BUTTON	Indicates that a button needs to be
 *				drawn for this entry.
 *
 *	ENTRY_CLOSED		Indicates that the entry is closed and
 *				its subentries are not displayed.
 *
 *	ENTRY_HIDE		Indicates that the entry is hidden (i.e.
 *				can not be viewed by opening or scrolling).
 *
 *	BUTTON_AUTO
 *	BUTTON_SHOW
 *	BUTTON_MASK
 *
 *---------------------------------------------------------------------------
 */
#define ENTRY_CLOSED		(1<<0)
#define ENTRY_HIDE		(1<<1)
#define ENTRY_NOT_LEAF		(1<<2)
#define ENTRY_MASK		(ENTRY_CLOSED | ENTRY_HIDE)

#define ENTRY_HAS_BUTTON	(1<<3)
#define ENTRY_ICON		(1<<4)
#define ENTRY_REDRAW		(1<<5)
#define ENTRY_LAYOUT_PENDING	(1<<6)
#define ENTRY_DATA_CHANGED	(1<<7)
#define ENTRY_DIRTY		(ENTRY_DATA_CHANGED | ENTRY_LAYOUT_PENDING)

#define BUTTON_AUTO		(1<<8)
#define BUTTON_SHOW		(1<<9)
#define BUTTON_MASK		(BUTTON_AUTO | BUTTON_SHOW)

#define ENTRY_EDITABLE		(1<<10)

#define COLUMN_RULE_PICKED	(1<<1)
#define COLUMN_DIRTY		(1<<2)

#define STYLE_TEXTBOX		(0)
#define STYLE_COMBOBOX		(1)
#define STYLE_CHECKBOX		(2)
#define STYLE_TYPE		0x3

#define STYLE_LAYOUT		(1<<3)
#define STYLE_DIRTY		(1<<4)
#define STYLE_HIGHLIGHT		(1<<5)
#define STYLE_USER		(1<<6)

#define STYLE_EDITABLE		(1<<10)

typedef struct _Blt_TreeViewColumn Blt_TreeViewColumn;
typedef struct _Blt_TreeViewCombobox Blt_TreeView_Combobox;
typedef struct _Blt_TreeViewEntry Blt_TreeViewEntry;
typedef struct _Blt_TreeView Blt_TreeView;
typedef struct _Blt_TreeViewStyleClass Blt_TreeViewStyleClass;
typedef struct _Blt_TreeViewStyle Blt_TreeViewStyle;

typedef int (Blt_TreeViewCompareProc)(Tcl_Interp *interp, const char *name, 
	const char *pattern);

typedef Blt_TreeViewEntry *(Blt_TreeViewIterProc)(Blt_TreeViewEntry *entryPtr, 
	unsigned int mask);

typedef struct {
    int tagType;
    Blt_TreeView *tvPtr;
    Blt_HashSearch cursor;
    Blt_TreeViewEntry *entryPtr;
} Blt_TreeViewTagIter;

/*
 * Blt_TreeViewIcon --
 *
 *	Since instances of the same Tk image can be displayed in
 *	different windows with possibly different color palettes, Tk
 *	internally stores each instance in a linked list.  But if
 *	the instances are used in the same widget and therefore use
 *	the same color palette, this adds a lot of overhead,
 *	especially when deleting instances from the linked list.
 *
 *	For the treeview widget, we never need more than a single
 *	instance of an image, regardless of how many times it's used.
 *	Cache the image, maintaining a reference count for each
 *	image used in the widget.  It's likely that the treeview
 *	widget will use many instances of the same image (for example
 *	the open/close icons).
 */

typedef struct _Blt_TreeViewIcon {
    Tk_Image tkImage;		/* The Tk image being cached. */

    int refCount;		/* Reference count for this image. */

    short int width, height;	/* Dimensions of the cached image. */

    Blt_HashEntry *hashPtr;	/* Hash table pointer to the image. */

} *Blt_TreeViewIcon;

#define Blt_TreeView_IconHeight(icon)	((icon)->height)
#define Blt_TreeView_IconWidth(icon)	((icon)->width)
#define Blt_TreeView_IconBits(icon)	((icon)->tkImage)

/*
 * Blt_TreeViewColumn --
 *
 *	A column describes how to display a field of data in the tree.
 *	It may display a title that you can bind to. It may display a
 *	rule for resizing the column.  Columns may be hidden, and have
 *	attributes (foreground color, background color, font, etc)
 *	that override those designated globally for the treeview
 *	widget.
 */
struct _Blt_TreeViewColumn {
    int type;			/* Always TV_COLUMN */
    Blt_TreeKey key;		/* Data cell identifier for current tree. */
    int position;		/* Position of column in list.  Used
				 * to indicate the first and last
				 * columns. */
    UID tagsUid;		/* List of binding tags for this
				 * entry.  UID, not a string, because
				 * in the typical case most columns
				 * will have the same bindtags. */

    Blt_TreeView *tvPtr;
    unsigned int flags;

    /* Title-related information */
    const char *title;		/* Text displayed in column heading as its
				 * title. By default, this is the same as 
				 * the data cell name. */
    Blt_Font titleFont;		/* Font to draw title in. */

    XColor *titleFgColor;	/* Foreground color of text displayed in 
				 * the heading */

    Blt_Background titleBg;	/* Background color of the heading. */

    GC titleGC;

    XColor *activeTitleFgColor;	/* Foreground color of the heading when the
				 * column is activated.*/
    Blt_Background activeTitleBg;	

    int titleBorderWidth;
    int titleRelief;

    GC activeTitleGC;

    TextLayout *titleTextPtr;
    short int titleWidth, titleHeight;

    Blt_TreeViewIcon titleIcon;	/* Icon displayed in column heading */
    const char *titleCmd;	/* TCL script to be executed by the column's
				 * "invoke" operation. */

    const char *sortCmd;	/* TCL script used to compare two columns. */

    /* General information. */
    int hidden;			/* Indicates if the column is displayed */
    int state;			/* Indicates if column title can invoked. */
    int editable;		/* Indicates if column can be edited. */

    int max;			/* Maximum space allowed for column. */
    int reqMin, reqMax;		/* Requested bounds on the width of column.
				 * Does not include any padding or the
				 * borderwidth of column.  If non-zero,
				 * overrides the computed width of the
				 * column. */

    int reqWidth;		/* User-requested width of column. Does not
				 * include any padding or the borderwidth of
				 * column.  If non-zero, overrides the
				 * computed column width. */

    int maxWidth;		/* Width of the widest entry in the column. */

    int worldX;			/* Starting world x-coordinate of the
				 * column. */

    double weight;		/* Growth factor for column.  Zero indicates
				 * that the column can not be resized. */

    int width;			/* Computed width of column. */

    Blt_TreeViewStyle *stylePtr;	/* Default style for column. */

    Blt_Background bg;		/* Background color of column. */
    int borderWidth;		/* Border width of the column. */
    int relief;			/* Relief of the column. */
    Blt_Pad pad;		/* Horizontal padding on either side of the
				 * column. */

    Tk_Justify justify;		/* Indicates how the text or icon is
				 * justified within the column. */

    Blt_ChainLink link;
    
    int ruleLineWidth;
    Blt_Dashes ruleDashes;
    GC ruleGC;
};


struct _Blt_TreeViewStyle {
    int refCount;		/* Usage reference count.  A reference count
				 * of zero indicates that the style may be
				 * freed. */
    unsigned int flags;		/* Bit field containing both the style type
				 * and various flags. */
    const char *name;		/* Instance name. */
    Blt_TreeViewStyleClass *classPtr; 
				/* Contains class-specific information such as
				 * configuration specifications and configure,
				 * draw, etc. routines. */
    Blt_HashEntry *hashPtr;	/* If non-NULL, points to the hash table entry
				 * for the style.  A style that's been
				 * deleted, but still in use (non-zero
				 * reference count) will have no hash table
				 * entry.
				 */
    Blt_ChainLink link;		/* If non-NULL, container for the style in a
				 * list of all newly created styles. */
    /* General style fields. */
    Tk_Cursor cursor;		/* X Cursor */

    Blt_TreeViewIcon icon;	/* If non-NULL, is a Tk_Image to be drawn in
				 * the cell. */
    int gap;			/* # pixels gap between icon and text. */
    Blt_Font font;
    XColor *fgColor;		/* Normal foreground color of cell. */
    XColor *highlightFgColor;	/* Foreground color of cell when
				 * highlighted. */
    XColor *activeFgColor;	/* Foreground color of cell when active. */
    XColor *selInFocusFgColor;	/* Foreground color of a selected cell. If
				 * non-NULL, overrides default foreground
				 * color specification. */

    Blt_Background bg;		/* Normal background color of cell. */
    Blt_Background highlightBg;	/* Background color of cell when
				 * highlighted. */
    Blt_Background activeBg;	/* Background color of cell when active. */

    Blt_Background selInFocusBg;/* Background color of a selected cell.  If
				 * non-NULL, overrides the default background
				 * color specification. */
    const char *validateCmd;

};

typedef struct _Blt_TreeViewValue {
    Blt_TreeViewColumn *columnPtr;	/* Column in which the value is located. */
    size_t width, height;	/* Dimensions of value. */
    Blt_TreeViewStyle *stylePtr;	/* Style information for cell
				 * displaying value. */
    const char *string;		/* Raw text string. */
    TextLayout *textPtr;	/* Processes string to be displayed .*/
    struct _Blt_TreeViewValue *nextPtr;
} Blt_TreeViewValue;
    
typedef void (StyleConfigProc)(Blt_TreeView *tvPtr, 
	Blt_TreeViewStyle *stylePtr);
typedef void (StyleDrawProc)(Blt_TreeView *tvPtr, Drawable drawable, 
	Blt_TreeViewEntry *entryPtr, Blt_TreeViewValue *valuePtr, 
	Blt_TreeViewStyle *stylePtr, int x, int y);
typedef int (StyleEditProc)(Blt_TreeView *tvPtr, Blt_TreeViewEntry *entryPtr, 
	Blt_TreeViewColumn *colPtr, Blt_TreeViewStyle *stylePtr);
typedef void (StyleFreeProc)(Blt_TreeView *tvPtr, Blt_TreeViewStyle *stylePtr);
typedef void (StyleMeasureProc)(Blt_TreeView *tvPtr, 
	Blt_TreeViewStyle *stylePtr, Blt_TreeViewValue *valuePtr);
typedef int (StylePickProc)(Blt_TreeViewEntry *entryPtr, 
	Blt_TreeViewValue *valuePtr, Blt_TreeViewStyle *stylePtr, int x, int y);

struct _Blt_TreeViewStyleClass {
    const char *className;	/* Class name of the style */
    Blt_ConfigSpec *specsPtr;	/* Style configuration specifications */
    StyleConfigProc *configProc;/* Sets the GCs for style. */
    StyleMeasureProc *measProc;	/* Measures the area needed for the value with
				 * this style. */
    StyleDrawProc *drawProc;	/* Draw the value in it's style. */
    StylePickProc *pickProc;	/* Routine to pick the style's button.
				 * Indicates if the mouse pointer is over the
				 * style's button (if it has one). */
    StyleEditProc *editProc;	/* Routine to edit the style's value. */
    StyleFreeProc *freeProc;	/* Routine to free the style's resources. */
};

/*
 * Blt_TreeViewEntry --
 *
 *	Contains data-specific information how to represent the data
 *	of a node of the hierarchy.
 *
 */
struct _Blt_TreeViewEntry {
    Blt_TreeNode node;		/* Node containing entry */
    int worldX, worldY;		/* X-Y position in world coordinates where the
				 * entry is positioned. */

    size_t width, height;	/* Dimensions of the entry. This includes the
				 * size of its columns. */

    int reqHeight;		/* Requested height of the entry.  Overrides
				 * computed height. */

    int vertLineLength;		/* Length of the vertical line segment. */

    short int lineHeight;	/* Height of first line of text. */

    unsigned short int flags;	/* Flags for this entry. For the definitions
				 * of the various bit fields see below. */

    UID tagsUid;		/* List of binding tags for this entry.  UID,
				 * not a string, because in the typical case
				 * most entries will have the same
				 * bindtags. */
    Blt_TreeView *tvPtr;

    UID openCmd, closeCmd;	/* TCL commands to invoke when entries are
				 * opened or closed. They override those
				 * specified globally. */
    /*
     * Button information:
     */
    short int buttonX, buttonY; /* X-Y coordinate offsets from to upper left
				 * corner of the entry to the upper-left
				 * corner of the button.  Used to pick the
				 * button quickly */

    Blt_TreeViewIcon *icons;	/* Tk images displayed for the entry.  The
				 * first image is the icon displayed to the
				 * left of the entry's label. The second is
				 * icon displayed when entry is "open". */

    Blt_TreeViewIcon *activeIcons; /* Tk images displayed for the entry.  The
				 * first image is the icon displayed to the
				 * left of the entry's label. The second is
				 * icon displayed when entry is "open". */

    short int iconWidth;
    short int iconHeight;	/* Maximum dimensions for icons and buttons
				 * for this entry. This is used to align the
				 * button, icon, and text. */
    /*
     * Label information:
     */
    TextLayout *textPtr;

    short int labelWidth;
    short int labelHeight;

    UID labelUid;		/* Text displayed right of the icon. */

    Blt_Font font;		/* Font of label. Overrides global font
				 * specification. */
    const char *fullName;

    int flatIndex;		/* Used to navigate to next/last entry when
				 * the view is flat. */

    Tcl_Obj *dataObjPtr;	/* pre-fetched data for sorting */

    XColor *color;		/* Color of label. If non-NULL, overrides
				 * default text color specification. */
    GC gc;

    Blt_TreeViewValue *values;	/* List of column-related information for each
				 * data value in the node.  Non-NULL only if
				 * there are value entries. */
};

/*
 * Blt_TreeViewButton --
 *
 *	A button is the open/close indicator at the far left of the entry.  It
 *	is displayed as a plus or minus in a solid colored box with optionally
 *	an border. It has both "active" and "normal" colors.
 */
typedef struct {
    XColor *fgColor;		/* Foreground color. */

    Blt_Background bg;		/* Background color. */

    XColor *activeFgColor;	/* Active foreground color. */

    Blt_Background activeBg;	/* Active background color. */

    GC normalGC;
    GC activeGC;

    int reqSize;

    int borderWidth;

    int openRelief, closeRelief;

    int width, height;

    Blt_TreeViewIcon *icons;

} Blt_TreeViewButton;

/*
 * LevelInfo --
 *
 */
typedef struct {
    int x;
    int iconWidth;
    int labelWidth;
} LevelInfo;

/*
 * Blt_TreeView --
 *
 *	A Blt_TreeView is a widget that displays an hierarchical table of one
 *	or more entries.
 *
 *	Entries are positioned in "world" coordinates, referring to the
 *	virtual treeview.  Coordinate 0,0 is the upper-left corner of the root
 *	entry and the bottom is the end of the last entry.  The widget's Tk
 *	window acts as view port into this virtual space. The treeview's
 *	xOffset and yOffset fields specify the location of the view port in
 *	the virtual world.  Scrolling the viewport is therefore simply
 *	changing the xOffset and/or yOffset fields and redrawing.
 *
 *	Note that world coordinates are integers, not signed short integers
 *	like X11 screen coordinates.  It's very easy to create a hierarchy
 *	taller than 0x7FFF pixels.
 */
struct _Blt_TreeView {
    Tcl_Interp *interp;

    Tcl_Command cmdToken;	/* Token for widget's TCL command. */

    Blt_Tree tree;		/* Token holding internal tree. */
    const char *treeName;	/* In non-NULL, is the name of the tree we are
				 * attached to */
    Blt_HashEntry *hashPtr;

    /* Blt_TreeView_ specific fields. */ 

    Tk_Window tkwin;		/* Window that embodies the widget.  NULL
                                 * means that the window has been destroyed
                                 * but the data structures haven't yet been
                                 * cleaned up.*/

    Display *display;		/* Display containing widget; needed, among
                                 * other things, to release resources after
                                 * tkwin has already gone away. */

    Blt_HashTable entryTable;	/* Table of entry information, keyed by the
				 * node pointer. */

    Blt_HashTable columnTable;	/* Table of column information. */
    Blt_Chain columns;		/* Chain of columns. Same as the hash table
				 * above but maintains the order in which
				 * columns are displayed. */

    unsigned int flags;		/* For bitfield definitions, see below */

    int inset;			/* Total width of all borders, including
				 * traversal highlight and 3-D border.
				 * Indicates how much interior stuff must be
				 * offset from outside edges to leave room for
				 * borders. */

    Blt_Font font;
    XColor *fgColor;

    Blt_Background bg;		/* 3D border surrounding the window
				 * (viewport). */
    Blt_Background altBg;

    int borderWidth;		/* Width of 3D border. */

    int relief;			/* 3D border relief. */

    int highlightWidth;		/* Width in pixels of highlight to draw around
				 * widget when it has the focus.  <= 0 means
				 * don't draw a highlight. */

    XColor *highlightBgColor;	/* Color for drawing traversal highlight area
				 * when highlight is off. */

    XColor *highlightColor;	/* Color for drawing traversal highlight. */

    const char *pathSep;	/* Pathname separators */

    const char *trimLeft;	/* Leading characters to trim from
				 * pathnames */

    /*
     * Entries are connected by horizontal and vertical lines. They may be
     * drawn dashed or solid.
     */
    int lineWidth;		/* Width of lines connecting entries */

    int dashes;			/* Dash on-off value. */

    XColor *lineColor;		/* Color of connecting lines. */

    /*
     * Button Information:
     *
     * The button is the open/close indicator at the far left of the entry.
     * It is usually displayed as a plus or minus in a solid colored box with
     * optionally an border. It has both "active" and "normal" colors.
     */
    Blt_TreeViewButton button;

    /*
     * Selection Information:
     *
     * The selection is the rectangle that contains a selected entry.  There
     * may be many selected entries.  It is displayed as a solid colored box
     * with optionally a 3D border.
     */
    int selRelief;		/* Relief of selected items. Currently is
				 * always raised. */

    int selBorderWidth;		/* Border width of a selected entry.*/

    XColor *selInFocusFgColor;	/* Text color of a selected entry. */
    XColor *selOutFocusFgColor;

    Blt_Background selInFocusBg;
    Blt_Background selOutFocusBg;

    Blt_TreeViewEntry *selAnchorPtr; /* Fixed end of selection (i.e. entry at
				      * which selection was started.) */
    Blt_TreeViewEntry *selMarkPtr;
    
    int	selectMode;		/* Selection style: "single" or
				 * "multiple".  */

    const char *selectCmd;	/* TCL script that's invoked whenever the
				 * selection changes. */

    Blt_HashTable selectTable;	/* Hash table of currently selected
				 * entries. */

    Blt_Chain selected;		/* Chain of currently selected entries.
				 * Contains the same information as the above
				 * hash table, but maintains the order in
				 * which entries are selected.
				 */

    int leader;			/* Number of pixels padding between
				 * entries. */

    Tk_Cursor cursor;		/* X Cursor */

    Tk_Cursor resizeCursor;	/* Resize Cursor */

    int reqWidth, reqHeight;	/* Requested dimensions of the treeview
				 * widget's window. */

    GC lineGC;			/* GC for drawing dotted line between
				 * entries. */

    XColor *focusColor;

    Blt_Dashes focusDashes;	/* Dash on-off value. */

    GC focusGC;			/* Graphics context for the active label. */

    Tk_Window comboWin;		

    Blt_TreeViewEntry *activePtr; /* Last active entry. */ 

    Blt_TreeViewEntry *focusPtr; /* Entry that currently has focus. */

    Blt_TreeViewEntry *activeBtnPtr; /* Pointer to last active button */

    Blt_TreeViewEntry *fromPtr;

    Blt_TreeViewValue *activeValuePtr; /* Last active value. */ 

    int xScrollUnits, yScrollUnits; /* # of pixels per scroll unit. */

    /* Command strings to control horizontal and vertical scrollbars. */
    Tcl_Obj *xScrollCmdObjPtr, *yScrollCmdObjPtr;

    int scrollMode;		/* Selects mode of scrolling: either
				 * BLT_SCROLL_MODE_HIERBOX, 
				 * BLT_SCROLL_MODE_LISTBOX, 
				 * or BLT_SCROLL_MODE_CANVAS. */
    /*
     * Total size of all "open" entries. This represents the range of world
     * coordinates.
     */
    int worldWidth, worldHeight;

    int xOffset, yOffset;	/* Translation between view port and world
				 * origin. */

    short int minHeight;	/* Minimum entry height. Used to to compute
				 * what the y-scroll unit should be. */
    short int titleHeight;	/* Height of column titles. */

    LevelInfo *levelInfo;

    /* Scanning information: */
    int scanAnchorX, scanAnchorY; /* Scan anchor in screen coordinates. */

    int scanX, scanY;		/* X-Y world coordinate where the scan
				 * started. */


    Blt_HashTable iconTable;	/* Table of Tk images */

    Blt_HashTable uidTable;	/* Table of strings. */

    Blt_HashTable styleTable;	/* Table of cell styles. */

    Blt_Chain userStyles;	/* List of user-created styles. */

    Blt_TreeViewEntry *rootPtr;	/* Root entry of tree. */

    Blt_TreeViewEntry **visibleArr;	/* Array of visible entries */

    int nVisible;		/* Number of entries in the above array */

    int nEntries;		/* Number of entries in tree. */
    int treeWidth;		/* Computed width of the tree. */

    int buttonFlags;		/* Global button indicator for all entries.
				 * This may be overridden by the entry's
				 * -button option. */

    const char *openCmd, *closeCmd; /* TCL commands to invoke when entries are
				 * opened or closed. */

    Blt_TreeViewIcon *icons;	/* Tk images displayed for the entry.  The
				 * first image is the icon displayed to the
				 * left of the entry's label. The second is
				 * icon displayed when entry is "open". */
    Blt_TreeViewIcon *activeIcons; /* Tk images displayed for the entry.  The
				 * first image is the icon displayed to the
				 * left of the entry's label. The second is
				 * icon displayed when entry is "open". */
    const char *takeFocus;

    ClientData clientData;

    Blt_BindTable bindTable;	/* Binding information for entries. */

    Blt_HashTable entryTagTable;
    Blt_HashTable buttonTagTable;
    Blt_HashTable columnTagTable;
    Blt_HashTable styleTagTable;

    Blt_TreeViewStyle *stylePtr;	/* Default style for text cells */

    Blt_TreeViewColumn treeColumn;
    
    Blt_TreeViewColumn *activeColumnPtr; 
    Blt_TreeViewColumn *activeTitleColumnPtr; 
				/* Column title currently active. */

    Blt_TreeViewColumn *resizeColumnPtr; 
				/* Column that is being resized. */

    size_t depth;

    int flatView;		/* Indicates if the view of the tree has been
				 * flattened. */

    Blt_TreeViewEntry **flatArr;	/* Flattened array of entries. */

    const char *sortField;	/* Field to be sorted. */

    int sortType;		/* Type of sorting to be performed. See below
				 * for valid values. */

    const char *sortCmd;	/* Sort command. */

    int sortDecreasing;		/* Indicates entries should be sorted in
				 * decreasing order. */

    int viewIsDecreasing;	/* Current sorting direction */

    Blt_TreeViewColumn *sortColumnPtr;/* Column to use for sorting criteria. */

#ifdef notdef
    Pixmap drawable;		/* Pixmap used to cache the entries displayed.
				 * The pixmap is saved so that only selected
				 * elements can be drawn quicky. */

    short int drawWidth, drawHeight;
#endif
    short int ruleAnchor, ruleMark;

    Blt_Pool entryPool;
    Blt_Pool valuePool;
};


BLT_EXTERN UID Blt_TreeView_GetUid(Blt_TreeView *tvPtr, const char *string);
BLT_EXTERN void Blt_TreeView_FreeUid(Blt_TreeView *tvPtr, UID uid);

BLT_EXTERN void Blt_TreeView_EventuallyRedraw(Blt_TreeView *tvPtr);
BLT_EXTERN Tcl_ObjCmdProc Blt_TreeView_WidgetInstCmd;
BLT_EXTERN Blt_TreeViewEntry *Blt_TreeView_NearestEntry(Blt_TreeView *tvPtr, 
	int x, int y, int flags);
BLT_EXTERN const char *Blt_TreeView_GetFullName(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr, int checkEntryLabel, Tcl_DString *dsPtr);
BLT_EXTERN void Blt_TreeView_SelectCmdProc(ClientData clientData);
BLT_EXTERN void Blt_TreeView_InsertText(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr, const char *string, int extra, 
	int insertPos);
BLT_EXTERN void Blt_TreeView_ComputeLayout(Blt_TreeView *tvPtr);
BLT_EXTERN void Blt_TreeView_PercentSubst(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr, const char *command, 
	Tcl_DString *resultPtr);
BLT_EXTERN void Blt_TreeView_DrawButton(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr, Drawable drawable, int x, int y);
BLT_EXTERN void Blt_TreeView_DrawValue(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr, Blt_TreeViewValue *valuePtr, 
	Drawable drawable, int x, int y);
BLT_EXTERN void Blt_TreeView_DrawOuterBorders(Blt_TreeView *tvPtr, 
	Drawable drawable);
BLT_EXTERN int Blt_TreeView_DrawIcon(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr, Drawable drawable, int x, int y);
BLT_EXTERN void Blt_TreeView_DrawHeadings(Blt_TreeView *tvPtr, 
	Drawable drawable);

BLT_EXTERN void Blt_TreeView_DrawRule(Blt_TreeView *tvPtr, 
	Blt_TreeViewColumn *colPtr, Drawable drawable);

BLT_EXTERN void Blt_TreeView_ConfigureButtons(Blt_TreeView *tvPtr);
BLT_EXTERN int Blt_TreeView_UpdateWidget(Tcl_Interp *interp, 
	Blt_TreeView *tvPtr);
BLT_EXTERN int Blt_TreeView_ScreenToIndex(Blt_TreeView *tvPtr, int x, int y);

BLT_EXTERN void Blt_TreeView_FreeIcon(Blt_TreeView *tvPtr, 
	Blt_TreeViewIcon icon);
BLT_EXTERN Blt_TreeViewIcon Blt_TreeView_GetIcon(Blt_TreeView *tvPtr, 
	const char *iconName);
BLT_EXTERN void Blt_TreeView_AddValue(Blt_TreeViewEntry *entryPtr, 
	Blt_TreeViewColumn *colPtr);
BLT_EXTERN int Blt_TreeView_CreateColumn(Blt_TreeView *tvPtr, 
	Blt_TreeViewColumn *colPtr, const char *name, const char *defLabel);
BLT_EXTERN void Blt_TreeView_DestroyValue(Blt_TreeView *tvPtr, 
	Blt_TreeViewValue *valuePtr);
BLT_EXTERN Blt_TreeViewValue *Blt_TreeView_FindValue(
	Blt_TreeViewEntry *entryPtr, Blt_TreeViewColumn *colPtr);
BLT_EXTERN void Blt_TreeView_DestroyColumns(Blt_TreeView *tvPtr);
BLT_EXTERN void Blt_TreeView_AllocateColumnUids(Blt_TreeView *tvPtr);
BLT_EXTERN void Blt_TreeView_FreeColumnUids(Blt_TreeView *tvPtr);
BLT_EXTERN void Blt_TreeView_UpdateColumnGCs(Blt_TreeView *tvPtr, 
	Blt_TreeViewColumn *colPtr);
BLT_EXTERN Blt_TreeViewColumn *Blt_TreeView_NearestColumn(Blt_TreeView *tvPtr, 
	int x, int y, ClientData *contextPtr);

BLT_EXTERN int Blt_TreeView_TextOp(Blt_TreeView *tvPtr, Tcl_Interp *interp, 
	int objc, Tcl_Obj *const *objv);

BLT_EXTERN int Blt_TreeView_CreateCombobox(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr, Blt_TreeViewColumn *colPtr);
BLT_EXTERN int Blt_TreeView_CreateEntry(Blt_TreeView *tvPtr, Blt_TreeNode node,
	int objc, Tcl_Obj *const *objv, int flags);
BLT_EXTERN int Blt_TreeView_ConfigureEntry(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr, int objc, Tcl_Obj *const *objv, int flags);
BLT_EXTERN int Blt_TreeView_OpenEntry(Blt_TreeView *tvPtr,
	Blt_TreeViewEntry *entryPtr);
BLT_EXTERN int Blt_TreeView_CloseEntry(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr);
BLT_EXTERN Blt_TreeViewEntry *Blt_TreeView_NextEntry(
	Blt_TreeViewEntry *entryPtr, unsigned int mask);
BLT_EXTERN Blt_TreeViewEntry *Blt_TreeView_PrevEntry(
	Blt_TreeViewEntry *entryPtr, unsigned int mask);
BLT_EXTERN int Blt_TreeView_GetEntry(Blt_TreeView *tvPtr, Tcl_Obj *objPtr, 
	Blt_TreeViewEntry **entryPtrPtr);
BLT_EXTERN int Blt_TreeView_EntryIsHidden(Blt_TreeViewEntry *entryPtr);
BLT_EXTERN Blt_TreeViewEntry *Blt_TreeView_NextSibling(
	Blt_TreeViewEntry *entryPtr, unsigned int mask);
BLT_EXTERN Blt_TreeViewEntry *Blt_TreeView_PrevSibling(
	Blt_TreeViewEntry *entryPtr, unsigned int mask);
BLT_EXTERN Blt_TreeViewEntry *Blt_TreeView_FirstChild(
	Blt_TreeViewEntry *parentPtr, unsigned int mask);
BLT_EXTERN Blt_TreeViewEntry *Blt_TreeView_LastChild(
	Blt_TreeViewEntry *entryPtr, unsigned int mask);
BLT_EXTERN Blt_TreeViewEntry *Blt_TreeView_ParentEntry(
	Blt_TreeViewEntry *entryPtr);

typedef int (Blt_TreeViewApplyProc)(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr);

BLT_EXTERN int Blt_TreeView_Apply(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr, Blt_TreeViewApplyProc *proc, 
	unsigned int mask);

BLT_EXTERN int Blt_TreeView_ColumnOp(Blt_TreeView *tvPtr, Tcl_Interp *interp, 
	int objc, Tcl_Obj *const *objv);
BLT_EXTERN int Blt_TreeView_SortOp(Blt_TreeView *tvPtr, Tcl_Interp *interp, 
	int objc, Tcl_Obj *const *objv);
BLT_EXTERN int Blt_TreeView_GetColumn(Tcl_Interp *interp, Blt_TreeView *tvPtr, 
	Tcl_Obj *objPtr, Blt_TreeViewColumn **colPtrPtr);

BLT_EXTERN void Blt_TreeView_SortFlatView(Blt_TreeView *tvPtr);
BLT_EXTERN void Blt_TreeView_SortView(Blt_TreeView *tvPtr);

BLT_EXTERN int Blt_TreeView_EntryIsSelected(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr);
BLT_EXTERN void Blt_TreeView_SelectEntry(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr);
BLT_EXTERN void Blt_TreeView_DeselectEntry(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr);
BLT_EXTERN void Blt_TreeView_PruneSelection(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr);
BLT_EXTERN void Blt_TreeView_ClearSelection(Blt_TreeView *tvPtr);
BLT_EXTERN void Blt_TreeView_ClearTags(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr);
BLT_EXTERN int Blt_TreeView_FindTaggedEntries(Blt_TreeView *tvPtr, 
	Tcl_Obj *objPtr, Blt_TreeViewTagIter *iterPtr);
BLT_EXTERN Blt_TreeViewEntry *Blt_TreeView_FirstTaggedEntry(
	Blt_TreeViewTagIter *iterPtr); 
BLT_EXTERN Blt_TreeViewEntry *Blt_TreeView_NextTaggedEntry(
	Blt_TreeViewTagIter *iterPtr); 
BLT_EXTERN ClientData Blt_TreeView_ButtonTag(Blt_TreeView *tvPtr, 
	const char *string);
BLT_EXTERN ClientData Blt_TreeView_EntryTag(Blt_TreeView *tvPtr, 
	const char *string);
BLT_EXTERN ClientData Blt_TreeView_ColumnTag(Blt_TreeView *tvPtr, 
	const char *string);
BLT_EXTERN void Blt_TreeView_GetTags(Tcl_Interp *interp, Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr, Blt_List list);
BLT_EXTERN void Blt_TreeView_TraceColumn(Blt_TreeView *tvPtr, 
	Blt_TreeViewColumn *colPtr);
BLT_EXTERN Blt_TreeViewIcon Blt_TreeView_GetEntryIcon(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr);
BLT_EXTERN void Blt_TreeView_SetStyleIcon(Blt_TreeView *tvPtr, 
	Blt_TreeViewStyle *stylePtr, Blt_TreeViewIcon icon);
BLT_EXTERN int Blt_TreeView_GetStyle(Tcl_Interp *interp, Blt_TreeView *tvPtr, 
	const char *styleName, Blt_TreeViewStyle **stylePtrPtr);
BLT_EXTERN void Blt_TreeView_FreeStyle(Blt_TreeView *tvPtr, 
	Blt_TreeViewStyle *stylePtr);
BLT_EXTERN Blt_TreeViewStyle *Blt_TreeView_CreateStyle(Tcl_Interp *interp, 
	Blt_TreeView *tvPtr, int type, const char *styleName);
BLT_EXTERN void Blt_TreeView_UpdateStyleGCs(Blt_TreeView *tvPtr, 
	Blt_TreeViewStyle *stylePtr);
BLT_EXTERN Blt_Background Blt_TreeView_GetStyleBackground(Blt_TreeView *tvPtr, 
	Blt_TreeViewStyle *stylePtr);
BLT_EXTERN GC Blt_TreeView_GetStyleGC(Blt_TreeViewStyle *stylePtr);
BLT_EXTERN Blt_Font Blt_TreeView_GetStyleFont(Blt_TreeView *tvPtr, 
	Blt_TreeViewStyle *stylePtr);
BLT_EXTERN XColor *Blt_TreeView_GetStyleFg(Blt_TreeView *tvPtr, 
	Blt_TreeViewStyle *stylePtr);
BLT_EXTERN Blt_TreeViewEntry *Blt_TreeView_NodeToEntry(Blt_TreeView *tvPtr, 
	Blt_TreeNode node);
BLT_EXTERN int Blt_TreeView_StyleOp(Blt_TreeView *tvPtr, Tcl_Interp *interp, 
	int objc, Tcl_Obj *const *objv);
BLT_EXTERN Blt_TreeViewEntry *Blt_TreeView_FindEntry(Blt_TreeView *tvPtr, 
	Blt_TreeNode node);

BLT_EXTERN int Blt_TreeView_CreateTextbox(Blt_TreeView *tvPtr, 
	Blt_TreeViewEntry *entryPtr, Blt_TreeViewColumn *colPtr);

#define CHOOSE(default, override)	\
	(((override) == NULL) ? (default) : (override))

#define GETLABEL(e)		\
	(((e)->labelUid != NULL)?(e)->labelUid:Blt_Tree_NodeLabel((e)->node))

#define Blt_TreeView_GetData(entryPtr, key, objPtrPtr) \
	Blt_Tree_GetValueByKey((Tcl_Interp *)NULL, (entryPtr)->tvPtr->tree, \
	      (entryPtr)->node, key, objPtrPtr)

#endif /* BLT_TREEVIEW_H */
