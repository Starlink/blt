
/*
 * bltPaneset.c --
 *
 *	Copyright 2009 George A Howlett.
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
 * Modes 
 *
 *  1.  enlarge/reduce:  change only the panes touching the anchor.
 *  2.  slinky:		 change all panes on both sides of the anchor.
 *  3.  hybrid:		 one side slinky, the other enlarge/reduce
 *  4.  locked minus 1   changed only the pane to the left of the anchor.
 *  5.  filmstrip        move the panes left or right.
 */
#include "bltInt.h"
#include "bltChain.h"
#include "bltList.h"
#include "bltHash.h"
#include "bltOp.h"
#include "bltSwitch.h"
#include "bltBgStyle.h"
#include "bltBind.h"

#define GETATTR(t,attr)		\
   (((t)->attr != NULL) ? (t)->attr : (t)->setPtr->attr)
#define VPORTWIDTH(s) \
    ((s)->flags & VERTICAL) ? Tk_Height((s)->tkwin) : Tk_Width((s)->tkwin);

#define TRACE	0
#define TRACE1	0
/*
 * Limits --
 *
 * 	Defines the bounding of a size (width or height) in the paneset.  It may
 * 	be related to the widget, pane or paneset size.
 */

typedef struct {
    int flags;				/* Flags indicate whether using default
					 * values for limits or not. See flags
					 * below. */
    int max, min;			/* Values for respective limits. */
    int nom;				/* Nominal starting value. */
} Limits;

#define MIN_LIMIT_SET	(1<<0)
#define MAX_LIMIT_SET	(1<<1)
#define NOM_LIMIT_SET	(1<<2)

#define MIN_LIMIT	0		/* Default minimum limit  */
#define MAX_LIMIT	SHRT_MAX	/* Default maximum limit */
#define NOM_LIMIT	-1000		/* Default nomimal value.  Indicates if
					 * a pane has received any space yet */

/* 
 * The following are the adjustment modes for the paneset widget.
 */
typedef enum AdjustModes {
    ADJUST_SLINKY,			/* Adjust all panes when resizing */
    ADJUST_GIVETAKE,			/* Adjust panes to immediate left/right
					 * or top/bottom of active sash. */
    ADJUST_SPREADSHEET,			/* Adjust only the left pane and the
					 * last pane. */
    ADJUST_FILMSTRIP			/* Move panes.  */
} AdjustMode;

typedef struct _Paneset Paneset;
typedef struct _Pane Pane;
typedef int (LimitsProc)(int value, Limits *limitsPtr);
typedef int (SizeProc)(Pane *panePtr);
typedef int (PanesetCmdProc)(Paneset *setPtr, Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv);


/*
 * These following flags indicate in what ways each pane in the widget can be
 * resized from its default dimensions.  The normal size of a pane is the
 * minimum amount of space needed to hold the widget embedded in it.  The
 * paneset may then be stretched or shrunk depending if the widget is larger or
 * smaller than the paneset.
 *
 * 	  RESIZE_NONE 	  - No resizing from normal size.
 *	  RESIZE_EXPAND   - Do not allow the size to decrease.
 *			    The size may increase however.
 *        RESIZE_SHRINK   - Do not allow the size to increase.
 *			    The size may decrease however.
 *	  RESIZE_BOTH     - Allow the size to increase or
 *			    decrease from the normal size.
 */
#define RESIZE_NONE			0
#define RESIZE_EXPAND			(1<<0)
#define RESIZE_SHRINK			(1<<1)
#define RESIZE_BOTH			(RESIZE_EXPAND | RESIZE_SHRINK)

/*
 * Default values for widget attributes.
 */
#define DEF_ACTIVESASHCOLOR	RGB_GREY50
#define DEF_ACTIVESASHRELIEF    "raised"
#define DEF_BACKGROUND		STD_NORMAL_BACKGROUND
#define DEF_BORDERWIDTH		"0"
#define DEF_FOREGROUND		STD_NORMAL_FOREGROUND
#define DEF_HEIGHT		"0"
#define DEF_HIGHLIGHTCOLOR	RGB_BLACK
#define DEF_HIGHLIGHTTHICKNESS	"0"
#define DEF_LAYOUT		"spreadsheet"
#define DEF_ORIENT		"horizontal"
#define DEF_PAD			"0"
#define DEF_PROPAGATE		"0"
#define DEF_SASHBORDERWIDTH	"1"
#define DEF_SASHCOLOR		STD_ACTIVE_BACKGROUND
#define DEF_SASHCURSOR		"sb_h_double_arrow"
#define DEF_SASHPAD		"0"
#define DEF_SASHRELIEF		"sunken"
#define DEF_SASHTHICKNESS	"2"
#define DEF_WEIGHT		"0"
#define DEF_WIDTH		"0"
#define DEF_SCROLLINCREMENT	"10"
#define DEF_SCROLLCOMMAND	"0"
#define DEF_SCROLLDELAY		"30"

#define DEF_PANE_ANCHOR		"center"
#define DEF_PANE_BORDERWIDTH	"0"
#define DEF_PANE_CONTROL	"normal"
#define DEF_PANE_FILL		"none"
#define DEF_PANE_HIDE		"0"
#define DEF_PANE_MAXSPAN	"0"
#define DEF_PANE_PAD		"0"
#define DEF_PANE_PADX		"0"
#define DEF_PANE_PADY		"0"
#define DEF_PANE_PROPAGATE 	"1"
#define DEF_PANE_RESIZE		"both"
#define DEF_PANE_WEIGHT		"1.0"
#define DEF_PANE_WINDOW		(char *)NULL

/*
 * Control --
 */
#define CONTROL_NORMAL	1.0		/* Consider the widget when calculating
					 * the row heights and column
					 * widths.  */
#define CONTROL_NONE	0.0		/* Ignore the widget.  The height and
					 * width of the rows/columns spanned by
					 * this widget will not affected by the
					 * size of the widget. */
#define CONTROL_FULL	-1.0		/* Consider only this widget when
					 * determining the column widths and row
					 * heights of the pane it spans. */
#define EXCL_PAD 	0
#define INCL_PAD	1
#define FCLAMP(x)	((((x) < 0.0) ? 0.0 : ((x) > 1.0) ? 1.0 : (x)))


/*
 * Pane --
 *
 *	An pane holds a widget and a possibly a sash.  It describes how 
 *	the widget should appear in the pane. 
 *	 1. padding.
 *	 2. size bounds for the widget.
 *
 */

struct _Pane  {
    Tk_Window tkwin;			/* Widget to be managed. */

    const char *name;			/* Name of pane */
    unsigned int flags;

    Paneset *setPtr;			/* Paneset widget managing this pane. */

    int borderWidth;			/* The external border width of the
					 * widget. This is needed to check if
					 * Tk_Changes(tkwin)->border_width 
					 * changes. */

    int manageWhenNeeded;		/* If non-zero, allow joint custody of
					 * the widget.  This is for cases where
					 * the same widget may be shared between
					 * two different panesets (e.g. same
					 * graph on two different panes).  Claim
					 * the widget only when the paneset is
					 * mapped. Don't destroy the pane if the
					 * pane loses custody of the widget. */

    Limits reqWidth, reqHeight;		/* Bounds for width and height requests
					 * made by the widget. */
    Tk_Anchor anchor;			/* Anchor type: indicates how the widget
					 * is positioned if extra space is
					 * available in the pane. */

    Blt_Pad xPad;			/* Extra padding placed left and right
					 * of the widget. */
    Blt_Pad yPad;			/* Extra padding placed above and below
					 * the widget */

    int iPadX, iPadY;			/* Extra padding added to the interior
					 * of the widget (i.e. adds to the
					 * requested * size of the widget) */

    int fill;				/* Indicates how the widget should fill
					 * the span of cells it occupies. */

    int worldX, worldY;			/* Origin of pane wrt container. */

    short int width, height;		/* Size of pane, including sash. */

    short int slaveX, slaveY;
    short int slaveWidth, slaveHeight;

    Blt_ChainLink link;			/* Pointer of this pane into the list of
					 * panes. */

    Blt_HashEntry *hashPtr;	        /* Pointer of this pane into hashtable
					 * of panes. */

    int index;				/* Index of the pane. */

    int size;				/* Current size of the pane. This size
					 * is bounded by min and max. */

    /*
     * nom and size perform similar duties.  I need to keep track of the
     * amount of space allocated to the pane (using size).  But at the same
     * time, I need to indicate that space can be parcelled out to this pane.
     * If a nominal size was set for this pane, I don't want to add space.
     */

    int nom;				/* The nominal size (neither expanded
					 * nor shrunk) of the pane based upon
					 * the requested size of the widget
					 * embedded in this pane. */

    int min, max;			/* Size constraints on the pane */

    float weight;			/* Weight of pane. */

    int resize;				/* Indicates if the pane should shrink
					 * or expand from its nominal size. */

    Limits reqSize;			/* Requested bounds for the size of the
					 * pane. The pane will not expand or
					 * shrink beyond these limits,
					 * regardless of how it was specified
					 * (max widget size).  This includes any
					 * extra padding which may be
					 * specified. */

    int relief;
    Blt_Background sashBg;
    Blt_Background activeSashBg;
    Blt_Background bg;			/* 3D background border surrounding the
					 * widget */
    Tcl_Obj *cmdObjPtr;

};

#define CONTEXT_SASH	(ClientData)0
#define CONTEXT_PANE	(ClientData)1

#define HIDE		(1<<0)
#define DISABLED	(1<<1)
#define ONSCREEN	(1<<4)
#define SASH_ACTIVE	(1<<5)
#define SASH		(1<<2)
/*
 * Paneset structure
 */
struct _Paneset {
    int flags;				/* See the flags definitions below. */

    Display *display;

    Tk_Window tkwin;			/* The container widget into which other
					 * widgets are arranged. */
    Tcl_Interp *interp;			/* Interpreter associated with all
					 * widgets */

    Tcl_Command cmdToken;

    int propagate;			/* If non-zero, the paneset will make a
					 * geometry request on behalf of the
					 * container widget. */

    AdjustMode mode;			/* Mode to use to resize panes when
					   the user adjusts a sash. */

    short int containerWidth;		/* Last known dimension of the
					 * container. */
    short int containerHeight;
    int normalWidth;			/* Normal dimensions of the paneset */
    int normalHeight;

    int reqWidth, reqHeight;		/* Constraints on the paneset's normal
					 * width and height */

    Tk_Cursor cursor;			/* X Cursor */

    Tk_Cursor sashCursor;		/* Sash cursor. */

    int borderWidth;
    int relief;

    short int width, height;		/* Requested size of the widget. */

    Blt_Background bg;			/* 3D border surrounding the window
					 * (viewport). */

    /*
     * Scrolling information:
     */
    int worldWidth;
    int scrollOffset;			/* Offset of viewport in world
					 * coordinates. */
    Tcl_Obj *scrollCmdObjPtr;		/* Command strings to control
					 * scrollbar.*/
    int scrollUnits;			/* Smallest unit of scrolling for
					 * tabs. */
    int scrollMark;			/* Target offset to scroll to. */
    int scrollIncr;			/* Current increment. */
    int interval;			/* Current increment. */
    Tcl_TimerToken timerToken;

    /*
     * Focus highlight ring
     */
    XColor *highlightColor;		/* Color for drawing traversal
					 * highlight. */
    int highlightThickness;
    GC highlightGC;			/* GC for focus highlight. */

    const char *takeFocus;		/* Says whether to select this widget
					 * during tab traveral operations.  This
					 * value isn't used in C code, but for
					 * the widget's TCL bindings. */

    int sashBorderWidth;
    int sashRelief;
    int activeSashRelief;
    int sashThickness;
    Blt_Background sashBg;
    Blt_Background activeSashBg;
    int sashAnchor, sashMark;
    int sashCurrent;

    Blt_Chain chain;			/* List of panes. Describes the order of
					 * the panes in the widget. */
    Blt_HashTable paneTable;		/* Table of panes.  Serves as a
					 * directory to look up panes from
					 * windows. */
    Blt_BindTable bindTable;		/* Tab binding information */
    Blt_HashTable tagTable;		/* Table of tags. */
    Pane *activePtr;			/* Indicates the pane with the active
					 * sash. */

    Pane *focusPtr;			/* Pane currently with focus. Draw with
					 * highlight ring around the sash. */
    Pane *anchorPtr;			/* Pane that is currently anchored */
    Pane *sinkPtr;
    int anchorX, anchorY;		/* Location of the anchor pane. */
    Tcl_Obj *cmdObjPtr;
    size_t nVisible;			/* # of visible panes. */
    GC gc;
    size_t nextId;
};

/*
 * Paneset flags definitions
 */
#define REDRAW_PENDING  (1<<0)		/* A redraw request is pending. */
#define LAYOUT_PENDING 	(1<<1)		/* Get the requested sizes of the
					 * widgets before expanding/shrinking
					 * the size of the container.  It's
					 * necessary to recompute the layout
					 * every time a pane is added,
					 * reconfigured, or deleted, but not
					 * when the container is resized. */
#define SCROLL_PENDING 	(1<<2)		/* Get the requested sizes of the
					 * widgets before expanding/shrinking
					 * the size of the container.  It's
					 * necessary to recompute the layout
					 * every time a pane is added,
					 * reconfigured, or deleted, but not
					 * when the container is resized. */
#define NON_PARENT	(1<<3)		/* The container is managing widgets
					 * that arern't children of the
					 * container.  This requires that they
					 * are manually moved when the container
					 * is moved (a definite performance
					 * hit). */

#define VERTICAL	(1<<4)
#define HORIZONTAL	(1<<5)
#define FOCUS		(1<<6)

#define PANE_DEF_PAD		0

/*
 * Default values for widget attributes.
 */
#define DEF_PANE_ANCHOR		"center"
#define DEF_PANE_FILL		"none"
#define DEF_PANE_PADX		"0"
#define DEF_PANE_PADY		"0"
#define DEF_PANE_PROPAGATE 	"1"
#define DEF_PANE_RESIZE		"both"
#define DEF_PANE_CONTROL	"normal"
#define DEF_PANE_WEIGHT		"1.0"

#define PANE_DEF_PAD		0
#define PANE_DEF_ANCHOR		TK_ANCHOR_CENTER
#define PANE_DEF_FILL		FILL_NONE
#define PANE_DEF_SPAN		1
#define PANE_DEF_CONTROL	CONTROL_NORMAL
#define PANE_DEF_IPAD		0

#define PANE_DEF_RESIZE		RESIZE_BOTH
#define PANE_DEF_PAD		0
#define PANE_DEF_WEIGHT		1.0

static Tk_GeomRequestProc PaneGeometryProc;
static Tk_GeomLostSlaveProc PaneCustodyProc;
static Tk_GeomMgr panesetMgrInfo =
{
    (char *)"paneset",			/* Name of geometry manager used by
					 * winfo */
    PaneGeometryProc,			/* Procedure to for new geometry
					 * requests */
    PaneCustodyProc,			/* Procedure when widget is taken
					 * away */
};

static Blt_OptionParseProc ObjToChild;
static Blt_OptionPrintProc ChildToObj;
static Blt_CustomOption childOption = {
    ObjToChild, ChildToObj, NULL, (ClientData)0,
};

static Blt_OptionParseProc ObjToLimits;
static Blt_OptionPrintProc LimitsToObj;
static Blt_CustomOption limitsOption =
{
    ObjToLimits, LimitsToObj, NULL, (ClientData)0
};

static Blt_OptionParseProc ObjToResize;
static Blt_OptionPrintProc ResizeToObj;
static Blt_CustomOption resizeOption =
{
    ObjToResize, ResizeToObj, NULL, (ClientData)0
};

static Blt_OptionParseProc ObjToOrientProc;
static Blt_OptionPrintProc OrientToObjProc;
static Blt_CustomOption orientOption = {
    ObjToOrientProc, OrientToObjProc, NULL, (ClientData)0
};

static Blt_OptionParseProc ObjToMode;
static Blt_OptionPrintProc ModeToObj;
static Blt_CustomOption adjustOption = {
    ObjToMode, ModeToObj, NULL, (ClientData)0,
};

static Blt_ConfigSpec paneSpecs[] =
{
    {BLT_CONFIG_BACKGROUND, "-activesashcolor", "activeSashColor", "SashColor",
	DEF_ACTIVESASHCOLOR, Blt_Offset(Pane, activeSashBg), 
        BLT_CONFIG_DONT_SET_DEFAULT | BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_ANCHOR, "-anchor", (char *)NULL, (char *)NULL, DEF_PANE_ANCHOR,
	Blt_Offset(Pane, anchor), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	(char *)NULL, Blt_Offset(Pane, bg), 
	BLT_CONFIG_NULL_OK | BLT_CONFIG_DONT_SET_DEFAULT },
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_PANE_BORDERWIDTH, Blt_Offset(Pane, borderWidth), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_FILL, "-fill", (char *)NULL, (char *)NULL, DEF_PANE_FILL, 
	Blt_Offset(Pane, fill), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_SYNONYM, "-height", "reqHeight", (char *)NULL, (char *)NULL, 
	Blt_Offset(Pane, reqHeight), 0},
    {BLT_CONFIG_BITMASK, "-hide", "hide", "Hide", DEF_PANE_HIDE, 
        Blt_Offset(Pane, flags), BLT_CONFIG_DONT_SET_DEFAULT, 
	(Blt_CustomOption *)HIDE},
    {BLT_CONFIG_PIXELS_NNEG, "-ipadx", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(Pane, iPadX), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-ipady", (char *)NULL, (char *)NULL, 
	(char *)NULL, Blt_Offset(Pane, iPadY), 0},
    {BLT_CONFIG_PAD, "-padx", (char *)NULL, (char *)NULL, (char *)NULL, 
	Blt_Offset(Pane, xPad), 0},
    {BLT_CONFIG_PAD, "-pady", (char *)NULL, (char *)NULL, (char *)NULL, 
	Blt_Offset(Pane, yPad), 0},
    {BLT_CONFIG_CUSTOM, "-reqheight", "reqHeight", (char *)NULL, (char *)NULL, 
	Blt_Offset(Pane, reqHeight), 0, &limitsOption},
    {BLT_CONFIG_CUSTOM, "-reqwidth", "reqWidth", (char *)NULL, (char *)NULL, 
	Blt_Offset(Pane, reqWidth), 0, &limitsOption},
    {BLT_CONFIG_CUSTOM, "-resize", (char *)NULL, (char *)NULL, DEF_PANE_RESIZE,
	Blt_Offset(Pane, resize), BLT_CONFIG_DONT_SET_DEFAULT, &resizeOption},
    {BLT_CONFIG_BACKGROUND, "-sashcolor", "sashColor", "SashColor",
	DEF_SASHCOLOR, Blt_Offset(Pane, sashBg), 
        BLT_CONFIG_DONT_SET_DEFAULT | BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-size", (char *)NULL, (char *)NULL, (char *)NULL, 
	Blt_Offset(Pane, reqSize), 0, &limitsOption},
    {BLT_CONFIG_FLOAT, "-weight", "weight", "Weight", DEF_PANE_WEIGHT,
	Blt_Offset(Pane, weight), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_SYNONYM, "-width", "reqWidth", (char *)NULL, (char *)NULL, 
	Blt_Offset(Pane, reqWidth), 0},
    {BLT_CONFIG_CUSTOM, "-window", "window", "Window", DEF_PANE_WINDOW, 
	Blt_Offset(Pane, tkwin), BLT_CONFIG_NULL_OK, &childOption},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/* 
 * Hide the sash. 
 *
 *	.p configure -sashthickness 0
 *	.p pane configure -hide yes 
 *	Put all the drawers in the paneset widget, hidden by default.
 *	Reveal/hide drawers to pop them out.
 *	plotarea | sidebar | scroller
 */

static Blt_ConfigSpec panesetSpecs[] =
{
    {BLT_CONFIG_BACKGROUND, "-activesashcolor", "activeSashColor", "SashColor",
	DEF_ACTIVESASHCOLOR, Blt_Offset(Paneset, activeSashBg), 0},
    {BLT_CONFIG_RELIEF, "-activesashrelief", "activeSashRelief", "SashRelief", 
	DEF_ACTIVESASHRELIEF, Blt_Offset(Paneset, activeSashRelief), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	DEF_BACKGROUND, Blt_Offset(Paneset, bg), 0},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_BORDERWIDTH, Blt_Offset(Paneset, borderWidth), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_PIXELS_NNEG, "-height", "height", "Height", DEF_HEIGHT,
	Blt_Offset(Paneset, reqHeight), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_HIGHLIGHTCOLOR, Blt_Offset(Paneset, highlightColor), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-highlightthickness", "highlightThickness",
	"HighlightThickness", DEF_HIGHLIGHTTHICKNESS, 
	Blt_Offset(Paneset, highlightThickness), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-adjust", "adjust", "Adjust", DEF_LAYOUT,
	Blt_Offset(Paneset, mode), BLT_CONFIG_DONT_SET_DEFAULT, &adjustOption},
    {BLT_CONFIG_CUSTOM, "-orient", "orient", "Orient", DEF_ORIENT, 
	Blt_Offset(Paneset, flags), BLT_CONFIG_DONT_SET_DEFAULT, &orientOption},
    {BLT_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	(char *)NULL, Blt_Offset(Paneset, cursor), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-reqheight", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(Paneset, reqHeight), 0, &limitsOption},
    {BLT_CONFIG_CUSTOM, "-reqwidth", (char *)NULL, (char *)NULL,
	(char *)NULL, Blt_Offset(Paneset, reqWidth), 0, &limitsOption},
    {BLT_CONFIG_PIXELS_NNEG, "-sashborderwidth", "sashBorderWidth", 
        "SashBorderWidth", DEF_SASHBORDERWIDTH, 
	Blt_Offset(Paneset, sashBorderWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BACKGROUND, "-sashcolor", "sashColor", "SashColor",
	DEF_SASHCOLOR, Blt_Offset(Paneset, sashBg), 0},
    {BLT_CONFIG_CURSOR, "-sashcursor", "sashCursor", "SashCursor",
        DEF_SASHCURSOR, Blt_Offset(Paneset, sashCursor), 0},
    {BLT_CONFIG_RELIEF, "-sashrelief", "sashRelief", "SashRelief", 
	DEF_SASHRELIEF, Blt_Offset(Paneset, sashRelief), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-sashthickness", "sashThickness", "SashThickness",
	DEF_SASHTHICKNESS, Blt_Offset(Paneset, sashThickness),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-scrollcommand", "scrollCommand", "ScrollCommand",
	(char *)NULL, Blt_Offset(Paneset, scrollCmdObjPtr),BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_POS, "-scrollincrement", "scrollIncrement",
	"ScrollIncrement", DEF_SCROLLINCREMENT, 
	Blt_Offset(Paneset, scrollUnits), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_INT_NNEG, "-scrolldelay", "scrollDelay", "ScrollDelay",
	DEF_SCROLLDELAY, Blt_Offset(Paneset, interval),
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-width", "width", "Width", DEF_WIDTH,
	Blt_Offset(Paneset, reqWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};


/*
 * PaneIterator --
 *
v *	Panes may be tagged with strings.  A pane may have many tags.  The same
 *	tag may be used for many panes.
 *	
 */
typedef enum { 
    ITER_SINGLE, ITER_ALL, ITER_TAG, ITER_PATTERN, 
} IteratorType;

typedef struct _Iterator {
    Paneset *setPtr;		       /* Paneset that we're iterating over. */

    IteratorType type;			/* Type of iteration:
					 * ITER_TAG	 By item tag.
					 * ITER_ALL      By every item.
					 * ITER_SINGLE   Single item: either 
					 *               tag or index.
					 * ITER_PATTERN  Over a consecutive 
					 *               range of indices.
					 */

    Pane *startPtr;			/* Starting pane.  Starting point of
					 * search, saved if iterator is reused.
					 * Used for ITER_ALL and ITER_SINGLE
					 * searches. */
    Pane *endPtr;			/* Ending pend (inclusive). */

    Pane *nextPtr;			/* Next pane. */

    /* For tag-based searches. */
    char *tagName;			/* If non-NULL, is the tag that we are
					 * currently iterating over. */

    Blt_HashTable *tablePtr;		/* Pointer to tag hash table. */

    Blt_HashSearch cursor;		/* Search iterator for tag hash
					 * table. */
    Blt_ChainLink link;
} PaneIterator;

/*
 * Forward declarations
 */
static Tcl_FreeProc PanesetFreeProc;
static Tcl_IdleProc DisplayPaneset;
static Tcl_ObjCmdProc PanesetCmd;
static Tk_EventProc PanesetEventProc;
static Tk_EventProc PaneEventProc;
static Tcl_FreeProc PaneFreeProc;
static Blt_BindPickProc PanePickProc;
static Tcl_ObjCmdProc PanesetInstCmdProc;
static Tcl_CmdDeleteProc PanesetInstCmdDeleteProc;


static int GetPaneIterator(Tcl_Interp *interp, Paneset *setPtr, Tcl_Obj *objPtr,
	PaneIterator *iterPtr);
static int GetPaneFromObj(Tcl_Interp *interp, Paneset *setPtr, Tcl_Obj *objPtr, 
	Pane **panePtrPtr);

/*
 *---------------------------------------------------------------------------
 *
 * ObjToChild --
 *
 *	Converts a window name into Tk window.
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
ObjToChild(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window parent,		/* Parent window */
    Tcl_Obj *objPtr,		/* String representation. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Pane *panePtr = (Pane *)widgRec;
    Paneset *setPtr;
    Tk_Window *tkwinPtr = (Tk_Window *)(widgRec + offset);
    Tk_Window old, tkwin;
    const char *string;

    old = *tkwinPtr;
    tkwin = NULL;
    setPtr = panePtr->setPtr;
    string = Tcl_GetString(objPtr);
    if (string[0] != '\0') {
	tkwin = Tk_NameToWindow(interp, string, parent);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	if (tkwin == old) {
	    return TCL_OK;
	}
	/*
	 * Allow only widgets that are children of the tabset to be embedded
	 * into the page.  This way we can make assumptions about the window
	 * based upon its parent; either it's the tabset window or it has been
	 * torn off.
	 */
	parent = Tk_Parent(tkwin);
	if (parent != setPtr->tkwin) {
	    Tcl_AppendResult(interp, "can't manage \"", Tk_PathName(tkwin),
		"\" in paneset \"", Tk_PathName(setPtr->tkwin), "\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
	Tk_ManageGeometry(tkwin, &panesetMgrInfo, panePtr);
	Tk_CreateEventHandler(tkwin, StructureNotifyMask, PaneEventProc, 
		panePtr);
	/*
	 * We need to make the window to exist immediately.  If the window is
	 * torn off (placed into another container window), the timing between
	 * the container and the its new child (this window) gets tricky.
	 * This should work for Tk 4.2.
	 */
	Tk_MakeWindowExist(tkwin);
    }
    if (old != NULL) {
	Tk_DeleteEventHandler(old, StructureNotifyMask, PaneEventProc, panePtr);
	Tk_ManageGeometry(old, (Tk_GeomMgr *)NULL, panePtr);
	Tk_UnmapWindow(old);
    }
    *tkwinPtr = tkwin;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ChildToObj --
 *
 *	Converts the Tk window back to a Tcl_Obj (i.e. its name).
 *
 * Results:
 *	The name of the window is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ChildToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window parent,		/* Not used. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Tk_Window tkwin = *(Tk_Window *)(widgRec + offset);
    Tcl_Obj *objPtr;

    if (tkwin == NULL) {
	objPtr = Tcl_NewStringObj("", -1);
    } else {
	objPtr = Tcl_NewStringObj(Tk_PathName(tkwin), -1);
    }
    return objPtr;
}


/*
 *---------------------------------------------------------------------------
 *
 * ObjToMode --
 *
 *	Converts an adjust mode name into a enum.
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
ObjToMode(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window parent,		/* Parent window */
    Tcl_Obj *objPtr,		/* String representation. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    AdjustMode *modePtr = (AdjustMode *)(widgRec + offset);
    const char *string;

    string = Tcl_GetString(objPtr);
    if (strcmp(string, "slinky") == 0) {
	*modePtr = ADJUST_SLINKY;
    } else if (strcmp(string, "givetake") == 0) {
	*modePtr = ADJUST_GIVETAKE;
    } else if (strcmp(string, "spreadsheet") == 0) {
	*modePtr = ADJUST_SPREADSHEET;
    } else if (strcmp(string, "filmstrip") == 0) {
	*modePtr = ADJUST_FILMSTRIP;
    } else {
	Tcl_AppendResult(interp, "unknown mode \"", string, "\": should be "
		"givetake, slinky, spreadsheet, or filmstrip\"", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ChildToObj --
 *
 *	Converts the enum back to a mode string (i.e. its name).
 *
 * Results:
 *	The name of the mode is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ModeToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window parent,		/* Not used. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    AdjustMode mode = *(AdjustMode *)(widgRec + offset);
    const char *string;

    switch (mode) {
    case ADJUST_SLINKY: 
	string = "slinky"; 
	break;
    case ADJUST_GIVETAKE: 
	string = "givetake"; 
	break;
    case ADJUST_SPREADSHEET: 
	string = "spreadsheet"; 
	break;
    case ADJUST_FILMSTRIP: 
	string = "filmstrip"; 
	break;
    default:
	string = "???"; 
	break;
    }
    return Tcl_NewStringObj(string, -1);
}



/*
 *---------------------------------------------------------------------------
 *
 * ResetLimits --
 *
 *	Resets the limits to their default values.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
INLINE static void
ResetLimits(Limits *limitsPtr)		/* Limits to be imposed on the value */
{
    limitsPtr->flags = 0;
    limitsPtr->min = MIN_LIMIT;
    limitsPtr->max = MAX_LIMIT;
    limitsPtr->nom = NOM_LIMIT;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetBoundedWidth --
 *
 *	Bounds a given width value to the limits described in the limit
 *	structure.  The initial starting value may be overridden by the nominal
 *	value in the limits.
 *
 * Results:
 *	Returns the constrained value.
 *
 *---------------------------------------------------------------------------
 */
static int
GetBoundedWidth(
    int width,				/* Initial value to be constrained */
    Limits *limitsPtr)			/* Limits to be imposed on the value */
{
    /*
     * Check widgets for requested width values;
     */
    if (limitsPtr->flags & NOM_LIMIT_SET) {
	width = limitsPtr->nom;		/* Override initial value */
    }
    if (width < limitsPtr->min) {
	width = limitsPtr->min;		/* Bounded by minimum value */
    }
    if (width > limitsPtr->max) {
	width = limitsPtr->max;		/* Bounded by maximum value */
    }
    return width;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetBoundedHeight --
 *
 *	Bounds a given value to the limits described in the limit structure.
 *	The initial starting value may be overridden by the nominal value in
 *	the limits.
 *
 * Results:
 *	Returns the constrained value.
 *
 *---------------------------------------------------------------------------
 */
static int
GetBoundedHeight(
    int height,			/* Initial value to be constrained */
    Limits *limitsPtr)		/* Limits to be imposed on the value */
{
    /*
     * Check widgets for requested height values;
     */
    if (limitsPtr->flags & NOM_LIMIT_SET) {
	height = limitsPtr->nom;	/* Override initial value */
    }
    if (height < limitsPtr->min) {
	height = limitsPtr->min;	/* Bounded by minimum value */
    } 
    if (height > limitsPtr->max) {
	height = limitsPtr->max;	/* Bounded by maximum value */
    }
    return height;
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToLimits --
 *
 *	Converts the list of elements into zero or more pixel values which
 *	determine the range of pixel values possible.  An element can be in any
 *	form accepted by Tk_GetPixels. The list has a different meaning based
 *	upon the number of elements.
 *
 *	    # of elements:
 *
 *	    0 - the limits are reset to the defaults.
 *	    1 - the minimum and maximum values are set to this
 *		value, freezing the range at a single value.
 *	    2 - first element is the minimum, the second is the
 *		maximum.
 *	    3 - first element is the minimum, the second is the
 *		maximum, and the third is the nominal value.
 *
 *	Any element may be the empty string which indicates the default.
 *
 * Results:
 *	The return value is a standard TCL result.  The min and max fields
 *	of the range are set.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToLimits(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,		        /* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Widget of paneset */
    Tcl_Obj *objPtr,			/* New width list */
    char *widgRec,			/* Widget record */
    int offset,				/* Offset to field in structure */
    int flags)	
{
    Limits *limitsPtr = (Limits *)(widgRec + offset);
    int values[3];
    int nValues;
    int limitsFlags;

    /* Initialize limits to default values */
    values[2] = NOM_LIMIT;
    values[1] = MAX_LIMIT;
    values[0] = MIN_LIMIT;
    limitsFlags = 0;
    nValues = 0;
    if (objPtr != NULL) {
	Tcl_Obj **objv;
	int objc;
	int i;

	if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (objc > 3) {
	    Tcl_AppendResult(interp, "wrong # limits \"", Tcl_GetString(objPtr),
			     "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	for (i = 0; i < objc; i++) {
	    const char *string;
	    int size;

	    string = Tcl_GetString(objv[i]);
	    if (string[0] == '\0') {
		continue;		/* Empty string: use default value */
	    }
	    limitsFlags |= (1 << i);
	    if (Tk_GetPixelsFromObj(interp, tkwin, objv[i], &size) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if ((size < MIN_LIMIT) || (size > MAX_LIMIT)) {
		Tcl_AppendResult(interp, "bad limit \"", string, "\"",
				 (char *)NULL);
		return TCL_ERROR;
	    }
	    values[i] = size;
	}
	nValues = objc;
    }
    /*
     * Check the limits specified.  We can't check the requested size of
     * widgets.
     */
    switch (nValues) {
    case 1:
	limitsFlags |= (MIN_LIMIT_SET | MAX_LIMIT_SET);
	values[1] = values[0];		/* Set minimum and maximum to value */
	break;

    case 2:
	if (values[1] < values[0]) {
	    Tcl_AppendResult(interp, "bad range \"", Tcl_GetString(objPtr),
		"\": min > max", (char *)NULL);
	    return TCL_ERROR;		/* Minimum is greater than maximum */
	}
	break;

    case 3:
	if (values[1] < values[0]) {
	    Tcl_AppendResult(interp, "bad range \"", Tcl_GetString(objPtr),
			     "\": min > max", (char *)NULL);
	    return TCL_ERROR;		/* Minimum is greater than maximum */
	}
	if ((values[2] < values[0]) || (values[2] > values[1])) {
	    Tcl_AppendResult(interp, "nominal value \"", Tcl_GetString(objPtr),
		"\" out of range", (char *)NULL);
	    return TCL_ERROR;		/* Nominal is outside of range defined
					 * by minimum and maximum */
	}
	break;
    }
    limitsPtr->min = values[0];
    limitsPtr->max = values[1];
    limitsPtr->nom = values[2];
    limitsPtr->flags = limitsFlags;
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * LimitsToObj --
 *
 *	Convert the limits of the pixel values allowed into a list.
 *
 * Results:
 *	The string representation of the limits is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
LimitsToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Not used. */
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,			/* Row/column structure record */
    int offset,				/* Offset to field in structure */
    int flags)	
{
    Limits *limitsPtr = (Limits *)(widgRec + offset);
    Tcl_Obj *listObjPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (limitsPtr->flags & MIN_LIMIT_SET) {
	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewIntObj(limitsPtr->min));
    } else {
	Tcl_ListObjAppendElement(interp, listObjPtr, Blt_EmptyStringObj());
    }
    if (limitsPtr->flags & MAX_LIMIT_SET) {
	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewIntObj(limitsPtr->max));
    } else {
	Tcl_ListObjAppendElement(interp, listObjPtr, Blt_EmptyStringObj());
    }
    if (limitsPtr->flags & NOM_LIMIT_SET) {
	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewIntObj(limitsPtr->nom));
    } else {
	Tcl_ListObjAppendElement(interp, listObjPtr, Blt_EmptyStringObj());
    }
    return listObjPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToResize --
 *
 *	Converts the resize mode into its numeric representation.  Valid mode
 *	strings are "none", "expand", "shrink", or "both".
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToResize(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,		        /* Interpreter to send results back 
					 * to */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* Resize style string */
    char *widgRec,			/* Pane structure record */
    int offset,				/* Offset to field in structure */
    int flags)	
{
    const char *string;
    char c;
    int *resizePtr = (int *)(widgRec + offset);
    int length;

    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[0];
    if ((c == 'n') && (strncmp(string, "none", length) == 0)) {
	*resizePtr = RESIZE_NONE;
    } else if ((c == 'b') && (strncmp(string, "both", length) == 0)) {
	*resizePtr = RESIZE_BOTH;
    } else if ((c == 'e') && (strncmp(string, "expand", length) == 0)) {
	*resizePtr = RESIZE_EXPAND;
    } else if ((c == 's') && (strncmp(string, "shrink", length) == 0)) {
	*resizePtr = RESIZE_SHRINK;
    } else {
	Tcl_AppendResult(interp, "bad resize argument \"", string,
	    "\": should be \"none\", \"expand\", \"shrink\", or \"both\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ObjToOrientProc --
 *
 *	Converts the string representing a state into a bitflag.
 *
 * Results:
 *	The return value is a standard TCL result.  The state flags are
 *	updated.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToOrientProc(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Interpreter to send results back
					 * to */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* String representing state. */
    char *widgRec,			/* Widget record */
    int offset,				/* Offset to field in structure */
    int flags)	
{
    Paneset *setPtr = (Paneset *)(widgRec);
    unsigned int *flagsPtr = (unsigned int *)(widgRec + offset);
    const char *string;
    int length;

    string = Tcl_GetString(objPtr);
    length = strlen(string);
    if (strncmp(string, "vertical", length) == 0) {
	*flagsPtr |= VERTICAL;
    } else if (strncmp(string, "horizontal", length) == 0) {
	*flagsPtr &= ~VERTICAL;
    } else {
	Tcl_AppendResult(interp, "bad orientation \"", string,
	    "\": must be vertical or horizontal", (char *)NULL);
	return TCL_ERROR;
    }
    setPtr->flags |= LAYOUT_PENDING;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * OrientToObjProc --
 *
 *	Return the name of the style.
 *
 * Results:
 *	The name representing the style is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
OrientToObjProc(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,			/* Widget information record */
    int offset,				/* Offset to field in structure */
    int flags)	
{
    unsigned int mask = *(unsigned int *)(widgRec + offset);
    const char *string;

    string = (mask & VERTICAL) ? "vertical" : "horizontal";
    return Tcl_NewStringObj(string, -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * NameOfResize --
 *
 *	Converts the resize value into its string representation.
 *
 * Results:
 *	Returns a pointer to the static name string.
 *
 *---------------------------------------------------------------------------
 */
static const char *
NameOfResize(int resize)
{
    switch (resize & RESIZE_BOTH) {
    case RESIZE_NONE:
	return "none";
    case RESIZE_EXPAND:
	return "expand";
    case RESIZE_SHRINK:
	return "shrink";
    case RESIZE_BOTH:
	return "both";
    default:
	return "unknown resize value";
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ResizeToObj --
 *
 *	Returns resize mode string based upon the resize flags.
 *
 * Results:
 *	The resize mode string is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ResizeToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Not used. */
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,			/* Row/column structure record */
    int offset,				/* Offset to field in structure */
    int flags)	
{
    int resize = *(int *)(widgRec + offset);

    return Tcl_NewStringObj(NameOfResize(resize), -1);
}


static void
EventuallyRedraw(Paneset *setPtr)
{
    if ((setPtr->flags & REDRAW_PENDING) == 0) {
	setPtr->flags |= REDRAW_PENDING;
	Tcl_DoWhenIdle(DisplayPaneset, setPtr);
    }
}


static Pane *
FirstPane(Paneset *setPtr)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	Pane *panePtr;

	panePtr = Blt_Chain_GetValue(link);
	if ((panePtr->flags & (HIDE|DISABLED)) == 0) {
	    return panePtr;
	}
    }
    return NULL;
}

static Pane *
LastPane(Paneset *setPtr)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_LastLink(setPtr->chain); link != NULL;
	 link = Blt_Chain_PrevLink(link)) {
	Pane *panePtr;

	panePtr = Blt_Chain_GetValue(link);
	if ((panePtr->flags & HIDE) == 0) {
	    return panePtr;
	}
    }
    return NULL;
}


static Pane *
NextPane(Pane *panePtr)
{
    if (panePtr != NULL) {
	Blt_ChainLink link;

	for (link = Blt_Chain_NextLink(panePtr->link); link != NULL; 
	     link = Blt_Chain_NextLink(link)) {
	    panePtr = Blt_Chain_GetValue(link);
	    if ((panePtr->flags & HIDE) == 0) {
		return panePtr;
	    }
	}
    }
    return NULL;
}

static Pane *
PrevPane(Pane *panePtr)
{
    if (panePtr != NULL) {
	Blt_ChainLink link;
	
	for (link = Blt_Chain_PrevLink(panePtr->link); link != NULL; 
	     link = Blt_Chain_PrevLink(link)) {
	    panePtr = Blt_Chain_GetValue(link);
	    if ((panePtr->flags & HIDE) == 0) {
		return panePtr;
	    }
	}
    }
    return NULL;
}


/*
 *---------------------------------------------------------------------------
 *
 * DestroyPane --
 *
 *	Removes the Pane structure from the hash table and frees the memory
 *	allocated by it.  
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the pane is freed up.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroyPane(Pane *panePtr)
{
    PaneFreeProc((DestroyData)panePtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * PanesetEventProc --
 *
 *	This procedure is invoked by the Tk event handler when the container
 *	widget is reconfigured or destroyed.
 *
 *	The paneset will be rearranged at the next idle point if the container
 *	widget has been resized or moved. There's a distinction made between
 *	parent and non-parent container arrangements.  When the container is
 *	the parent of the embedded widgets, the widgets will automatically
 *	keep their positions relative to the container, even when the
 *	container is moved.  But if the container is not the parent, those
 *	widgets have to be moved manually.  This can be a performance hit in
 *	rare cases where we're scrolling the container (by moving the window)
 *	and there are lots of non-child widgets arranged inside.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the paneset associated with tkwin to have its layout
 *	re-computed and drawn at the next idle point.
 *
 *---------------------------------------------------------------------------
 */
static void
PanesetEventProc(ClientData clientData, XEvent *eventPtr)
{
    Paneset *setPtr = clientData;

    if (eventPtr->type == Expose) {
	if (eventPtr->xexpose.count == 0) {
	    EventuallyRedraw(setPtr);
	}
    } else if ((eventPtr->type == FocusIn) || (eventPtr->type == FocusOut)) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    if (eventPtr->type == FocusIn) {
		setPtr->flags |= FOCUS;
	    } else {
		setPtr->flags &= ~FOCUS;
	    }
	    EventuallyRedraw(setPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (setPtr->tkwin != NULL) {
	    Blt_DeleteWindowInstanceData(setPtr->tkwin);
	    setPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(setPtr->interp, setPtr->cmdToken);
	}
	if (setPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayPaneset, setPtr);
	}
	Tcl_EventuallyFree(setPtr, PanesetFreeProc);
    } else if (eventPtr->type == ConfigureNotify) {
	setPtr->flags |= LAYOUT_PENDING;
	EventuallyRedraw(setPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * PaneEventProc --
 *
 *	This procedure is invoked by the Tk event handler when StructureNotify
 *	events occur in a widget managed by the paneset.
 *
 *	For example, when a managed widget is destroyed, it frees the
 *	corresponding pane structure and arranges for the paneset layout to be
 *	re-computed at the next idle point.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the managed widget was deleted, the Pane structure gets cleaned up
 *	and the paneset is rearranged.
 *
 *---------------------------------------------------------------------------
 */
static void
PaneEventProc(
    ClientData clientData,	/* Pointer to Pane structure for widget
				 * referred to by eventPtr. */
    XEvent *eventPtr)		/* Describes what just happened. */
{
    Pane *panePtr = (Pane *)clientData;
    Paneset *setPtr = panePtr->setPtr;

    if (eventPtr->type == ConfigureNotify) {
	int borderWidth;

	if (panePtr->tkwin == NULL) {
	    return;
	}
	/* setPtr->flags |= LAYOUT_PENDING; */
	borderWidth = Tk_Changes(panePtr->tkwin)->border_width;
	if (panePtr->borderWidth != borderWidth) {
	    panePtr->borderWidth = borderWidth;
	    EventuallyRedraw(setPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	panePtr->tkwin = NULL;
	DestroyPane(panePtr);
	setPtr->flags |= LAYOUT_PENDING;
	EventuallyRedraw(setPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * PaneCustodyProc --
 *
 * 	This procedure is invoked when a widget has been stolen by another
 * 	geometry manager.  The information and memory associated with the widget
 * 	is released.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the paneset to have its layout recomputed at the next idle
 *	point.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
PaneCustodyProc(ClientData clientData, Tk_Window tkwin)
{
    Pane *panePtr = (Pane *)clientData;
    Paneset *setPtr = panePtr->setPtr;

    if (Tk_IsMapped(panePtr->tkwin)) {
	Tk_UnmapWindow(panePtr->tkwin);
    }
    Tk_UnmaintainGeometry(panePtr->tkwin, setPtr->tkwin);
    panePtr->tkwin = NULL;
    DestroyPane(panePtr);
    setPtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(setPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * PaneGeometryProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for widgets managed by
 *	the paneset geometry manager.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the paneset to have its layout re-computed and re-arranged
 *	at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
PaneGeometryProc(ClientData clientData, Tk_Window tkwin)
{
    Pane *panePtr = (Pane *)clientData;

    panePtr->setPtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(panePtr->setPtr);
}


static Blt_HashTable *
GetTagTable(Paneset *setPtr, const char *tagName)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&setPtr->tagTable, tagName);
    if (hPtr == NULL) {
	return NULL;			/* No tag by name. */
    }
    return Blt_GetHashValue(hPtr);
}

static int
HasTag(Pane *panePtr, const char *tagName)
{
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;

    if (strcmp(tagName, "all") == 0) {
	return TRUE;
    }
    tablePtr = GetTagTable(panePtr->setPtr, tagName);
    if (tablePtr == NULL) {
	return FALSE;
    }
    hPtr = Blt_FindHashEntry(tablePtr, (char *)panePtr);
    if (hPtr == NULL) {
	return FALSE;
    }
    return TRUE;
}

static Blt_HashTable *
AddTagTable(Paneset *setPtr, const char *tagName)
{
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    int isNew;

    hPtr = Blt_CreateHashEntry(&setPtr->tagTable, tagName, &isNew);
    if (isNew) {
	tablePtr = Blt_AssertMalloc(sizeof(Blt_HashTable));
	Blt_InitHashTable(tablePtr, BLT_ONE_WORD_KEYS);
	Blt_SetHashValue(hPtr, tablePtr);
    } else {
	tablePtr = Blt_GetHashValue(hPtr);
    }
    return tablePtr;
}

static void
AddTag(Paneset *setPtr, Pane *panePtr, const char *tagName)
{
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    int isNew;

    tablePtr = AddTagTable(setPtr, tagName);
    hPtr = Blt_CreateHashEntry(tablePtr, (char *)panePtr, &isNew);
    if (isNew) {
	Blt_SetHashValue(hPtr, panePtr);
    }
}


static void
ForgetTag(Paneset *setPtr, const char *tagName)
{
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;

    if (strcmp(tagName, "all") == 0) {
	return;				/* Can't remove tag "all". */
    }
    hPtr = Blt_FindHashEntry(&setPtr->tagTable, tagName);
    if (hPtr == NULL) {
	return;				/* No tag by name. */
    }
    tablePtr = Blt_GetHashValue(hPtr);
    Blt_DeleteHashTable(tablePtr);
    Blt_Free(tablePtr);
    Blt_DeleteHashEntry(&setPtr->tagTable, hPtr);
}

static void
DestroyTags(Paneset *setPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;

    for (hPtr = Blt_FirstHashEntry(&setPtr->tagTable, &iter); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&iter)) {
	Blt_HashTable *tablePtr;
	
	tablePtr = Blt_GetHashValue(hPtr);
	Blt_DeleteHashTable(tablePtr);
    }
}

static void
RemoveTag(Pane *panePtr, const char *tagName)
{
    Blt_HashTable *tablePtr;
    Paneset *setPtr;

    setPtr = panePtr->setPtr;
    tablePtr = GetTagTable(setPtr, tagName);
    if (tablePtr != NULL) {
	Blt_HashEntry *hPtr;

	hPtr = Blt_FindHashEntry(tablePtr, (char *)panePtr);
	if (hPtr != NULL) {
	    Blt_DeleteHashEntry(tablePtr, hPtr);
	}
    }
}

static void
ClearTags(Pane *panePtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    Paneset *setPtr;

    setPtr = panePtr->setPtr;
    for (hPtr = Blt_FirstHashEntry(&setPtr->tagTable, &iter); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&iter)) {
	Blt_HashTable *tablePtr;
	Blt_HashEntry *h2Ptr;
	
	tablePtr = Blt_GetHashValue(hPtr);
	h2Ptr = Blt_FindHashEntry(tablePtr, (char *)panePtr);
	if (h2Ptr != NULL) {
	    Blt_DeleteHashEntry(tablePtr, h2Ptr);
	}
    }
}

static void
AppendTags(Pane *panePtr, Blt_List list)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    Paneset *setPtr;

    setPtr = panePtr->setPtr;
    for (hPtr = Blt_FirstHashEntry(&setPtr->tagTable, &iter); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&iter)) {
	Blt_HashTable *tablePtr;
	
	tablePtr = Blt_GetHashValue(hPtr);
	if (Blt_FindHashEntry(tablePtr, (char *)panePtr) != NULL) {
	    Blt_List_Append(list, Blt_GetHashKey(&setPtr->tagTable, hPtr), 0);
	}
    }
}

static ClientData
MakeTag(Paneset *setPtr, const char *tagName)
{
    Blt_HashTable *tablePtr;
    Blt_HashEntry *hPtr;

    tablePtr = AddTagTable(setPtr, tagName);
    hPtr = Blt_FindHashEntry(&setPtr->tagTable, tagName);
    return Blt_GetHashKey(&setPtr->tagTable, hPtr);
}

/*ARGSUSED*/
static void
PaneTagsProc(Blt_BindTable table, ClientData object, ClientData context, 
	    Blt_List list)
{
    Pane *panePtr = (Pane *)object;
    Paneset *setPtr;

    setPtr = table->clientData;
    if (context == CONTEXT_SASH) {
	Blt_List_Append(list, MakeTag(setPtr, "Sash"), 0);
    } else if (context == CONTEXT_PANE) {
	ClientData tag;

	tag = MakeTag(setPtr, panePtr->name);
	Blt_List_Append(list, tag, 0);
	AppendTags(panePtr, list);
	Blt_List_Append(list, MakeTag(setPtr, "all"), 0);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * PanePickProc --
 *
 *	Searches the pane located within the given screen X-Y coordinates in
 *	the viewport.  
 *
 * Results:
 *	Returns the pointer to the pane.  If the pointer isn't contained by any
 *	pane, NULL is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static ClientData
PanePickProc(ClientData clientData, int x, int y, ClientData *contextPtr)
{
    Paneset *setPtr = clientData;
    int sashSize;
    Pane *panePtr;

    /* Convert to world coordinates. */
    if (setPtr->flags & VERTICAL) {
	y += setPtr->scrollOffset;
    } else {
	x += setPtr->scrollOffset;
    }    
    sashSize = setPtr->sashThickness + 2 * setPtr->highlightThickness;
    for (panePtr = FirstPane(setPtr); panePtr != NULL; 
	 panePtr = NextPane(panePtr)) {
	ClientData context;

	if ((x < panePtr->worldX) || (x >= (panePtr->worldX+panePtr->width)) ||
	    (y < panePtr->worldY) || (y >= (panePtr->worldY+panePtr->height))) {
	    continue;
	}
	context = CONTEXT_PANE;
	if (setPtr->flags & VERTICAL) {
	    if (y >= (panePtr->worldY + panePtr->height - sashSize)) {
		context = CONTEXT_SASH;
	    } 
	} else {
#if TRACE1
    fprintf(stderr, "PickPane(%s %d, x=%d, y=%d) x=%d, y=%d w=%d, h=%d\n", 
	    panePtr->name, panePtr->index, x, y, panePtr->worldX, panePtr->worldY, panePtr->width - sashSize, panePtr->height);
#endif
 	    if (x >= (panePtr->worldX + panePtr->width - sashSize)) {
		context = CONTEXT_SASH;
	    }
	}
	if (contextPtr != NULL) {
	    *contextPtr = context;
	}
	return panePtr;
    }
    return NULL;
}


/*
 *---------------------------------------------------------------------------
 *
 * PaneSearch --
 *
 * 	Searches for the pane designated by a screen coordinate.
 *
 * Results:
 *	Returns a pointer to the pane the given point.  If no pane contains the
 *	coordinate, NULL is returned.
 *
 *---------------------------------------------------------------------------
 */
static Pane *
PaneSearch(Paneset *setPtr, int x, int y)
{
    Blt_ChainLink link;

    /* 
     * This search assumes that rows/columns are organized in increasing order.
     */
    for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL; 
	 link = Blt_Chain_NextLink(link)) {
	Pane *panePtr;

	panePtr = Blt_Chain_GetValue(link);
	/*
	 *|         |x    |x+size  |        |
	 *            ^
	 *            x
	 */
	if (x < panePtr->worldX) { 
	    return NULL;		/* Too far, can't find row/column. */
	}
	if (x < (panePtr->worldX + panePtr->size)) {
	    return panePtr;
	}
    }
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * SashSearch --
 *
 * 	Searches for the pane designated by a screen coordinate.
 *
 * Results:
 *	Returns a pointer to the pane the given point.  If no pane contains the
 *	coordinate, NULL is returned.
 *
 *---------------------------------------------------------------------------
 */
static Pane *
SashSearch(Paneset *setPtr, int x, int y)
{
    Blt_ChainLink link;

    /* 
     * This search assumes that rows/columns are organized in increasing order.
     */
    for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL; 
	 link = Blt_Chain_NextLink(link)) {
	Pane *panePtr;

	panePtr = Blt_Chain_GetValue(link);
	/*
	 *|         |x    |x+size  |        |
	 *            ^
	 *            x
	 */
	if (x < panePtr->worldX) { 
	    return NULL;		/* Too far, can't find row/column. */
	}
	if (x < (panePtr->worldX + panePtr->size)) {
	    return panePtr;
	}
    }
    return NULL;
}

static INLINE Pane *
BeginPane(Paneset *setPtr)
{
    Blt_ChainLink link;

    link = Blt_Chain_FirstLink(setPtr->chain); 
    if (link != NULL) {
	return Blt_Chain_GetValue(link);
    }
    return NULL;
}

static INLINE Pane *
EndPane(Paneset *setPtr)
{
    Blt_ChainLink link;

    link = Blt_Chain_LastLink(setPtr->chain); 
    if (link != NULL) {
	return Blt_Chain_GetValue(link);
    }
    return NULL;
}

static Pane *
StepPane(Pane *panePtr)
{
    if (panePtr != NULL) {
	Blt_ChainLink link;

	link = Blt_Chain_NextLink(panePtr->link); 
	if (link != NULL) {
	    return Blt_Chain_GetValue(link);
	}
    }
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * NextTaggedPane --
 *
 *	Returns the next pane derived from the given tag.
 *
 * Results:
 *	Returns the pointer to the next pane in the iterator.  If no more panes
 *	are available, then NULL is returned.
 *
 *---------------------------------------------------------------------------
 */
static Pane *
NextTaggedPane(PaneIterator *iterPtr)
{
    switch (iterPtr->type) {
    case ITER_TAG:
	{
	    Blt_HashEntry *hPtr;
	    
	    hPtr = Blt_NextHashEntry(&iterPtr->cursor); 
	    if (hPtr != NULL) {
		return Blt_GetHashValue(hPtr);
	    }
	    break;
	}
    case ITER_ALL:
	if (iterPtr->link != NULL) {
	    Pane *panePtr;
	    
	    panePtr = Blt_Chain_GetValue(iterPtr->link);
	    iterPtr->link = Blt_Chain_NextLink(iterPtr->link);
	    return panePtr;
	}
	break;
    case ITER_PATTERN:
	{
	    Blt_ChainLink link;
	    
	    for (link = iterPtr->link; link != NULL; 
		 link = Blt_Chain_NextLink(link)) {
		Pane *panePtr;
		
		panePtr = Blt_Chain_GetValue(iterPtr->link);
		if (Tcl_StringMatch(panePtr->name, iterPtr->tagName)) {
		    iterPtr->link = Blt_Chain_NextLink(link);
		    return panePtr;
		}
	    }
	    break;
	}
    default:
	break;
    }	
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * FirstTaggedPane --
 *
 *	Returns the first pane derived from the given tag.
 *
 * Results:
 *	Returns the first pane in the sequence.  If no more panes are in
 *	the list, then NULL is returned.
 *
 *---------------------------------------------------------------------------
 */
static Pane *
FirstTaggedPane(PaneIterator *iterPtr)
{
    switch (iterPtr->type) {
    case ITER_TAG:
	{
	    Blt_HashEntry *hPtr;
	    
	    hPtr = Blt_FirstHashEntry(iterPtr->tablePtr, &iterPtr->cursor);
	    if (hPtr != NULL) {
		return Blt_GetHashValue(hPtr);
	    }
	}

    case ITER_ALL:
	if (iterPtr->link != NULL) {
	    Pane *panePtr;
	    
	    panePtr = Blt_Chain_GetValue(iterPtr->link);
	    iterPtr->link = Blt_Chain_NextLink(iterPtr->link);
	    return panePtr;
	}
	break;

    case ITER_PATTERN:
	{
	    Blt_ChainLink link;
	    
	    for (link = iterPtr->link; link != NULL; 
		 link = Blt_Chain_NextLink(link)) {
		Pane *panePtr;
		
		panePtr = Blt_Chain_GetValue(iterPtr->link);
		if (Tcl_StringMatch(panePtr->name, iterPtr->tagName)) {
		    iterPtr->link = Blt_Chain_NextLink(link);
		    return panePtr;
		}
	    }
	    break;
	}
    case ITER_SINGLE:
	return iterPtr->startPtr;
    } 
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetPaneFromObj --
 *
 *	Gets the pane associated the given index, tag, or label.  This routine
 *	is used when you want only one pane.  It's an error if more than one
 *	pane is specified (e.g. "all" tag or range "1:4").  It's also an error
 *	if the tag is empty (no panes are currently tagged).
 *
 *---------------------------------------------------------------------------
 */
static int 
GetPaneFromObj(Tcl_Interp *interp, Paneset *setPtr, Tcl_Obj *objPtr,
	      Pane **panePtrPtr)
{
    PaneIterator iter;
    Pane *firstPtr;

    if (GetPaneIterator(interp, setPtr, objPtr, &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    firstPtr = FirstTaggedPane(&iter);
    if (firstPtr != NULL) {
	Pane *nextPtr;

	nextPtr = NextTaggedPane(&iter);
	if (nextPtr != NULL) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "multiple panes specified by \"", 
			Tcl_GetString(objPtr), "\"", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
    }
    *panePtrPtr = firstPtr;
    return TCL_OK;
}

static int
GetPaneByIndex(Tcl_Interp *interp, Paneset *setPtr, const char *string, 
	      int length, Pane **panePtrPtr)
{
    Pane *panePtr;
    char c;
    long pos;

    panePtr = NULL;
    c = string[0];
    if (Blt_GetLong(NULL, string, &pos) == TCL_OK) {
	Blt_ChainLink link;

	link = Blt_Chain_GetNthLink(setPtr->chain, pos);
	if (link != NULL) {
	    panePtr = Blt_Chain_GetValue(link);
	} 
	if (panePtr == NULL) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "can't find pane: bad index \"", 
			string, "\"", (char *)NULL);
	    }
	    return TCL_ERROR;
	}		
    } else if ((c == 'a') && (strcmp(string, "active") == 0)) {
	panePtr = setPtr->activePtr;
    } else if ((c == 'c') && (strcmp(string, "current") == 0)) {
	panePtr = (Pane *)Blt_GetCurrentItem(setPtr->bindTable);
    } else if ((c == 'f') && (strcmp(string, "focus") == 0)) {
	panePtr = setPtr->focusPtr;
    } else if ((c == 'f') && (strcmp(string, "first") == 0)) {
	panePtr = FirstPane(setPtr);
    } else if ((c == 'l') && (strcmp(string, "last") == 0)) {
	panePtr = LastPane(setPtr);
    } else if ((c == 'n') && (strcmp(string, "none") == 0)) {
	panePtr = NULL;
    } else if (c == '@') {
	int x, y;

	if (Blt_GetXY(interp, setPtr->tkwin, string, &x, &y) != TCL_OK) {
	    return TCL_ERROR;
	}
	panePtr = (Pane *)PanePickProc(setPtr, x, y, NULL);
    } else {
	return TCL_CONTINUE;
    }
    *panePtrPtr = panePtr;
    return TCL_OK;
}

static Pane *
GetPaneByName(Paneset *setPtr, const char *string)
{
    Blt_HashEntry *hPtr;
    
    hPtr = Blt_FindHashEntry(&setPtr->paneTable, string);
    if (hPtr == NULL) {
	return NULL;
    }
    return Blt_GetHashValue(hPtr);
}


/*
 *---------------------------------------------------------------------------
 *
 * GetPaneIterator --
 *
 *	Converts a string representing a pane index into an pane pointer.  The
 *	index may be in one of the following forms:
 *
 *	 number		Pane at index in the list of panes.
 *	 @x,y		Pane closest to the specified X-Y screen coordinates.
 *	 "active"	Pane where mouse pointer is located.
 *	 "posted"       Pane is the currently posted cascade pane.
 *	 "next"		Next pane from the focus pane.
 *	 "previous"	Previous pane from the focus pane.
 *	 "end"		Last pane.
 *	 "none"		No pane.
 *
 *	 number		Pane at position in the list of panes.
 *	 @x,y		Pane closest to the specified X-Y screen coordinates.
 *	 "active"	Pane mouse is located over.
 *	 "focus"	Pane is the widget's focus.
 *	 "select"	Currently selected pane.
 *	 "right"	Next pane from the focus pane.
 *	 "left"		Previous pane from the focus pane.
 *	 "up"		Next pane from the focus pane.
 *	 "down"		Previous pane from the focus pane.
 *	 "end"		Last pane in list.
 *	"name:string"   Pane named "string".
 *	"index:number"  Pane at index number in list of panes.
 *	"tag:string"	Pane(s) tagged by "string".
 *	"label:pattern"	Pane(s) with label matching "pattern".
 *	
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.  The
 *	pointer to the node is returned via panePtrPtr.  Otherwise, TCL_ERROR
 *	is returned and an error message is left in interpreter's result
 *	field.
 *
 *---------------------------------------------------------------------------
 */
static int
GetPaneIterator(Tcl_Interp *interp, Paneset *setPtr, Tcl_Obj *objPtr,
	       PaneIterator *iterPtr)
{
    Pane *panePtr, *startPtr, *endPtr;
    Blt_HashTable *tablePtr;
    char *string;
    char c;
    int nBytes;
    int length;
    int result;

    iterPtr->setPtr = setPtr;
    iterPtr->type = ITER_SINGLE;
    iterPtr->tagName = Tcl_GetStringFromObj(objPtr, &nBytes);
    iterPtr->nextPtr = NULL;
    iterPtr->startPtr = iterPtr->endPtr = NULL;

#ifdef notdef
    if (setPtr->flags & (LAYOUT_PENDING | SCROLL_PENDING)) {
	ComputeMenuGeometry(setPtr);
    }
    if (setPtr->flags & CM_SCROLL_PENDING) {
	ComputeVisiblePanes(setPtr);
    } 
#endif
    if (setPtr->focusPtr == NULL) {
	setPtr->focusPtr = setPtr->activePtr;
	Blt_SetFocusItem(setPtr->bindTable, setPtr->focusPtr, NULL);
    }
    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[0];
    iterPtr->startPtr = iterPtr->endPtr = setPtr->activePtr;
    startPtr = endPtr = panePtr = NULL;
    if (c == '\0') {
	startPtr = endPtr = NULL;
    } 
    iterPtr->type = ITER_SINGLE;
    result = GetPaneByIndex(interp, setPtr, string, length, &panePtr);
    if (result == TCL_ERROR) {
	return TCL_ERROR;
    }
    if (result == TCL_OK) {
	iterPtr->startPtr = iterPtr->endPtr = panePtr;
	return TCL_OK;
    }
    if ((c == 'a') && (strcmp(iterPtr->tagName, "all") == 0)) {
	iterPtr->type  = ITER_ALL;
	iterPtr->link = Blt_Chain_FirstLink(setPtr->chain);
    } else if ((c == 'i') && (length > 6) && 
	       (strncmp(string, "index:", 6) == 0)) {
	if (GetPaneByIndex(interp, setPtr, string + 6, length - 6, &panePtr) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	iterPtr->startPtr = iterPtr->endPtr = panePtr;
    } else if ((c == 'n') && (length > 5) && 
	       (strncmp(string, "name:", 5) == 0)) {
	panePtr = GetPaneByName(setPtr, string + 5);
	if (panePtr == NULL) {
	    Tcl_AppendResult(interp, "can't find a pane name \"", string + 5,
			"\" in \"", Tk_PathName(setPtr->tkwin), "\"",
			(char *)NULL);
	    return TCL_ERROR;
	}
	iterPtr->startPtr = iterPtr->endPtr = panePtr;
    } else if ((c == 't') && (length > 4) && 
	       (strncmp(string, "tag:", 4) == 0)) {
	Blt_HashTable *tablePtr;

	tablePtr = GetTagTable(setPtr, string + 4);
	if (tablePtr == NULL) {
	    Tcl_AppendResult(interp, "can't find a tag \"", string + 5,
			"\" in \"", Tk_PathName(setPtr->tkwin), "\"",
			(char *)NULL);
	    return TCL_ERROR;
	}
	iterPtr->tagName = string + 4;
	iterPtr->tablePtr = tablePtr;
	iterPtr->type = ITER_TAG;
    } else if ((c == 'l') && (length > 6) && 
	       (strncmp(string, "label:", 6) == 0)) {
	iterPtr->link = Blt_Chain_FirstLink(setPtr->chain);
	iterPtr->tagName = string + 6;
	iterPtr->type = ITER_PATTERN;
    } else if ((panePtr = GetPaneByName(setPtr, string)) != NULL) {
	iterPtr->startPtr = iterPtr->endPtr = panePtr;
    } else if ((tablePtr = GetTagTable(setPtr, string)) != NULL) {
	iterPtr->tagName = string;
	iterPtr->tablePtr = tablePtr;
	iterPtr->type = ITER_TAG;
    } else {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find pane index, name, or tag \"", 
		string, "\" in \"", Tk_PathName(setPtr->tkwin), "\"", 
			     (char *)NULL);
	}
	return TCL_ERROR;
    }	
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * NewPane --
 *
 *	This procedure creates and initializes a new Pane structure to hold a
 *	widget.  A valid widget has a parent widget that is either a) the
 *	container widget itself or b) a mutual ancestor of the container widget.
 *
 * Results:
 *	Returns a pointer to the new structure describing the new widget pane.
 *	If an error occurred, then the return value is NULL and an error message
 *	is left in interp->result.
 *
 * Side effects:
 *	Memory is allocated and initialized for the Pane structure.
 *
 * ---------------------------------------------------------------------------- 
 */
static Pane *
NewPane(Tcl_Interp *interp, Paneset *setPtr, Tcl_Obj *objPtr)
{
    Blt_HashEntry *hPtr;
    Pane *panePtr;
    const char *name;
    int isNew;
    char string[200];

    if (objPtr == NULL) {
	sprintf(string, "pane%lu", (unsigned long)setPtr->nextId++);
	name = string;
    } else {
	name = Tcl_GetString(objPtr);
    }
    hPtr = Blt_CreateHashEntry(&setPtr->paneTable, name, &isNew);
    if (!isNew) {
	Tcl_AppendResult(interp, "pane \"", name, "\" already exists.", 
		(char *)NULL);
	return NULL;
    }
    panePtr = Blt_AssertCalloc(1, sizeof(Pane));
    ResetLimits(&panePtr->reqWidth);
    ResetLimits(&panePtr->reqHeight);
    ResetLimits(&panePtr->reqSize);
    panePtr->setPtr = setPtr;
    panePtr->name = Blt_GetHashKey(&setPtr->paneTable, hPtr);
    panePtr->anchor = TK_ANCHOR_CENTER;
    panePtr->fill = FILL_NONE;
    panePtr->nom  = NOM_LIMIT;
    panePtr->resize = RESIZE_BOTH;
    panePtr->size = panePtr->index = 0;
    panePtr->tkwin = NULL;
    panePtr->weight = 1.0f;
    Blt_SetHashValue(hPtr, panePtr);
    return panePtr;
}


/*
 *---------------------------------------------------------------------------
 *
 * PaneFreeProc --
 *
 *	Removes the Pane structure from the hash table and frees the memory
 *	allocated by it.  
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the pane is freed up.
 *
 *---------------------------------------------------------------------------
 */
static void
PaneFreeProc(DestroyData dataPtr)
{
    Pane *panePtr = (Pane *)dataPtr;
    Paneset *setPtr = panePtr->setPtr;

    ClearTags(panePtr);
    if (panePtr->link != NULL) {
	Blt_Chain_DeleteLink(setPtr->chain, panePtr->link);
    }
    if (panePtr->tkwin != NULL) {
	Tk_DeleteEventHandler(panePtr->tkwin, StructureNotifyMask,
	      PaneEventProc, (ClientData)panePtr);
	Tk_ManageGeometry(panePtr->tkwin, (Tk_GeomMgr *)NULL, 
			  (ClientData)panePtr);
	if ((setPtr->tkwin != NULL) && 
	    (Tk_Parent(setPtr->tkwin) != setPtr->tkwin)) {
	    Tk_UnmaintainGeometry(panePtr->tkwin, setPtr->tkwin);
	}
	if (Tk_IsMapped(panePtr->tkwin)) {
	    Tk_UnmapWindow(panePtr->tkwin);
	}
    }
    if (panePtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(&setPtr->paneTable, panePtr->hashPtr);
    }
    Blt_DeleteBindings(setPtr->bindTable, panePtr);
    Blt_Free(panePtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * NewPaneset --
 *
 *	This procedure creates and initializes a new Paneset structure with
 *	tkwin as its container widget. The internal structures associated with
 *	the paneset are initialized.
 *
 * Results:
 *	Returns the pointer to the new Paneset structure describing the new
 *	paneset geometry manager.  If an error occurred, the return value will
 *	be NULL and an error message is left in interp->result.
 *
 * Side effects:
 *	Memory is allocated and initialized, an event handler is set up to
 *	watch tkwin, etc.
 *
 *---------------------------------------------------------------------------
 */
static Paneset *
NewPaneset(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    Paneset *setPtr;
    Tk_Window tkwin;

    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp), 
	Tcl_GetString(objPtr), (char *)NULL);
    if (tkwin == NULL) {
	return NULL;
    }
    Tk_SetClass(tkwin, (char *)"Paneset");
    setPtr = Blt_AssertCalloc(1, sizeof(Paneset));
    setPtr->tkwin = tkwin;
    setPtr->interp = interp;
    setPtr->display = Tk_Display(tkwin);
    setPtr->propagate = TRUE;
    setPtr->chain = Blt_Chain_Create();
    setPtr->sashThickness = 2;
    setPtr->sashRelief = TK_RELIEF_SUNKEN;
    setPtr->activeSashRelief = TK_RELIEF_RAISED;
    setPtr->sashBorderWidth = 1;
    setPtr->highlightThickness = 1;
    setPtr->flags = LAYOUT_PENDING;
    setPtr->mode = ADJUST_SPREADSHEET;
    setPtr->interval = 30;
    setPtr->scrollMark = 0;
    setPtr->scrollUnits = 10;
    Blt_SetWindowInstanceData(tkwin, setPtr);
    Blt_InitHashTable(&setPtr->paneTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&setPtr->tagTable, BLT_STRING_KEYS);
    setPtr->bindTable = Blt_CreateBindingTable(interp, tkwin, setPtr, 
	PanePickProc, PaneTagsProc);
    Tk_CreateEventHandler(tkwin, 
	ExposureMask|StructureNotifyMask|FocusChangeMask,
	PanesetEventProc, setPtr);
    setPtr->chain = Blt_Chain_Create();
    setPtr->cmdToken = Tcl_CreateObjCommand(interp, Tk_PathName(tkwin), 
	PanesetInstCmdProc, setPtr, PanesetInstCmdDeleteProc);
    return setPtr;
}


static void
RenumberPanes(Paneset *setPtr)
{
    int count;
    Blt_ChainLink link;

    count = 0;
    for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	Pane *panePtr;
	
	panePtr = Blt_Chain_GetValue(link);
	panePtr->index = count;
	count++;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * PanesetFreeProc --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release to
 *	clean up the Paneset structure at a safe time (when no-one is using it
 *	anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the paneset geometry manager is freed up.
 *
 *---------------------------------------------------------------------------
 */
static void
PanesetFreeProc(DestroyData dataPtr) /* Paneset structure */
{
    Blt_ChainLink link;
    Paneset *setPtr = (Paneset *)dataPtr;

    Blt_FreeOptions(panesetSpecs, (char *)setPtr, setPtr->display, 0);
    /* Release the chain of entries. */
    for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL;
	link = Blt_Chain_NextLink(link)) {
	Pane *panePtr;

	panePtr = Blt_Chain_GetValue(link);
	panePtr->link = NULL; /* Don't disrupt this chain of entries */
	DestroyPane(panePtr);
    }
    DestroyTags(setPtr);
    Blt_Chain_Destroy(setPtr->chain);
    Blt_DeleteHashTable(&setPtr->paneTable);
    Blt_DestroyBindingTable(setPtr->bindTable);
    Blt_Free(setPtr);
}


/*
 *---------------------------------------------------------------------------
 *
 * TranslateAnchor --
 *
 * 	Translate the coordinates of a given bounding box based upon the
 * 	anchor specified.  The anchor indicates where the given xy position is
 * 	in relation to the bounding box.
 *
 *  		nw --- n --- ne
 *  		|            |     x,y ---+
 *  		w   center   e      |     |
 *  		|            |      +-----+
 *  		sw --- s --- se
 *
 * Results:
 *	The translated coordinates of the bounding box are returned.
 *
 *---------------------------------------------------------------------------
 */
static void
TranslateAnchor(
    int dx, int dy,		/* Difference between outer and inner
				 * regions */
    Tk_Anchor anchor,		/* Direction of the anchor */
    int *xPtr, int *yPtr)
{
    int x, y;

    x = y = 0;
    switch (anchor) {
    case TK_ANCHOR_NW:		/* Upper left corner */
	break;
    case TK_ANCHOR_W:		/* Left center */
	y = (dy / 2);
	break;
    case TK_ANCHOR_SW:		/* Lower left corner */
	y = dy;
	break;
    case TK_ANCHOR_N:		/* Top center */
	x = (dx / 2);
	break;
    case TK_ANCHOR_CENTER:	/* Centered */
	x = (dx / 2);
	y = (dy / 2);
	break;
    case TK_ANCHOR_S:		/* Bottom center */
	x = (dx / 2);
	y = dy;
	break;
    case TK_ANCHOR_NE:		/* Upper right corner */
	x = dx;
	break;
    case TK_ANCHOR_E:		/* Right center */
	x = dx;
	y = (dy / 2);
	break;
    case TK_ANCHOR_SE:		/* Lower right corner */
	x = dx;
	y = dy;
	break;
    }
    *xPtr = (*xPtr) + x;
    *yPtr = (*yPtr) + y;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetReqWidth --
 *
 *	Returns the width requested by the widget starting in the given pane.
 *	The requested space also includes any internal padding which has been
 *	designated for this widget.
 *
 *	The requested width of the widget is always bounded by the limits set
 *	in panePtr->reqWidth.
 *
 * Results:
 *	Returns the requested width of the widget.
 *
 *---------------------------------------------------------------------------
 */
static int
GetReqWidth(Pane *panePtr)
{
    int width;

    width = (2 * panePtr->iPadX);
    if (panePtr->tkwin != NULL) {
	width += Tk_ReqWidth(panePtr->tkwin);
    }
    width = GetBoundedWidth(width, &panePtr->reqWidth);
    return width;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetReqHeight --
 *
 *	Returns the height requested by the widget starting in the given
 *	pane.  The requested space also includes any internal padding which
 *	has been designated for this widget.
 *
 *	The requested height of the widget is always bounded by the limits set
 *	in panePtr->reqHeight.
 *
 * Results:
 *	Returns the requested height of the widget.
 *
 *---------------------------------------------------------------------------
 */
static int
GetReqHeight(Pane *panePtr)
{
    int height;

    height = 2 * panePtr->iPadY;
    if (panePtr->tkwin != NULL) {
	height += Tk_ReqHeight(panePtr->tkwin);
    }
    height = GetBoundedHeight(height, &panePtr->reqHeight);
    return height;
}

/*
 *---------------------------------------------------------------------------
 *
 * LeftSpan --
 *
 *	Sums the space requirements of all the panes.
 *
 * Results:
 *	Returns the space currently used by the paneset widget.
 *
 *---------------------------------------------------------------------------
 */
static int
LeftSpan(Paneset *setPtr)
{
    int total;
    Pane *panePtr;

    total = 0;
    /* The left span is every pane before and including) the anchor pane. */
    for (panePtr = setPtr->anchorPtr; panePtr != NULL; 
	 panePtr = PrevPane(panePtr)) {
	total += panePtr->size;
    }
    return total;
}

/*
 *---------------------------------------------------------------------------
 *
 * RightSpan --
 *
 *	Sums the space requirements of all the panes.
 *
 * Results:
 *	Returns the space currently used by the paneset widget.
 *
 *---------------------------------------------------------------------------
 */
static int
RightSpan(Paneset *setPtr)
{
    int total;
    Pane *panePtr;

    total = 0;
    for (panePtr = NextPane(setPtr->anchorPtr); panePtr != NULL; 
	 panePtr = NextPane(panePtr)) {
	total += panePtr->size;
    }
    return total;
}

static int
GetReqPaneWidth(Pane *panePtr)
{
    return GetReqWidth(panePtr) + PADDING(panePtr->xPad) + 
	2 * panePtr->borderWidth;
}

static int
GetReqPaneHeight(Pane *panePtr)
{
    return GetReqHeight(panePtr) + PADDING(panePtr->yPad) +
	2 * panePtr->borderWidth;
}


/*
 *---------------------------------------------------------------------------
 *
 * GrowPane --
 *
 *	Expand the span by the amount of the extra space needed.  This
 *	procedure is used in Layout*Panes to grow the panes to their
 *	minimum nominal size, starting from a zero width and height space.
 *
 *	On the first pass we try to add space to panes which have not
 *	been touched yet (i.e. have no nominal size).  
 *
 *	If there is still extra space after the first pass, this means
 *	that there were no panes could be expanded. This pass will try 
 *	to remedy this by parcelling out the left over space evenly among 
 *	the rest of the panes.
 *
 *	On each pass, we have to keep iterating over the list, evenly doling
 *	out slices of extra space, because we may hit pane limits as space is
 *	donated.  In addition, if there are left over pixels because of
 *	round-off, this will distribute them as evenly as possible.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The panes in the span may be expanded.
 *
 *---------------------------------------------------------------------------
 */
static void
GrowPane(Pane *panePtr, int extra)
{
    if ((panePtr->nom == NOM_LIMIT) && (panePtr->max > panePtr->size)) {
	int avail;

	avail = panePtr->max - panePtr->size;
	if (avail > extra) {
	    panePtr->size += extra;
	    return;
	} else {
	    extra -= avail;
	    panePtr->size += avail;
	}
    }
    /* Find out how many panes still have space available */
    if ((panePtr->resize & RESIZE_EXPAND) && (panePtr->max > panePtr->size)) {
	int avail;

	avail = panePtr->max - panePtr->size;
	if (avail > extra) {
	    panePtr->size += extra;
	    return;
	} else {
	    extra -= avail;
	    panePtr->size += avail;
	}
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * GrowSpan --
 *
 *	Grow the span by the designated amount.  Size constraints on the panes
 *	may prevent any or all of the spacing adjustments.
 *
 *	This is very much like the GrowPane procedure, but in this case we are
 *	expanding all the panes. It uses a two pass approach, first giving
 *	space to panes which are smaller than their nominal sizes. This is
 *	because constraints on the panes may cause resizing to be non-linear.
 *
 *	If there is still extra space, this means that all panes are at least
 *	to their nominal sizes.  The second pass will try to add the left over
 *	space evenly among all the panes which still have space available
 *	(i.e. haven't reached their specified max sizes).
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The size of the pane may be increased.
 *
 *---------------------------------------------------------------------------
 */
static void
GrowSpan(Blt_Chain chain, int adjustment)	
{
    int delta;				/* Amount of space needed */
    int nAdjust;			/* Number of rows/columns that still can
					 * be adjusted. */
    Blt_ChainLink link;
    float totalWeight;

    /*
     * Pass 1:  First adjust the size of panes that still haven't reached their
     *		nominal size.
     */
    delta = adjustment;

    nAdjust = 0;
    totalWeight = 0.0f;
    for (link = Blt_Chain_LastLink(chain); link != NULL; 
	 link = Blt_Chain_PrevLink(link)) {
	Pane *panePtr;

	panePtr = Blt_Chain_GetValue(link);
	if ((panePtr->weight > 0.0f) && (panePtr->nom > panePtr->size)) {
	    nAdjust++;
	    totalWeight += panePtr->weight;
	}
    }

    while ((nAdjust > 0) && (totalWeight > 0.0f) && (delta > 0)) {
	Blt_ChainLink link;
	int ration;			/* Amount of space to add to each
					 * row/column. */
	ration = (int)(delta / totalWeight);
	if (ration == 0) {
	    ration = 1;
	}
	for (link = Blt_Chain_LastLink(chain); (link != NULL) && (delta > 0);
	    link = Blt_Chain_PrevLink(link)) {
	    Pane *panePtr;

	    panePtr = Blt_Chain_GetValue(link);
	    if (panePtr->weight > 0.0f) {
		int avail;		/* Amount of space still available. */

		avail = panePtr->nom - panePtr->size;
		if (avail > 0) {
		    int size;		/* Amount of space requested for a
					 * particular row/column. */
		    size = (int)(ration * panePtr->weight);
		    if (size > delta) {
			size = delta;
		    }
		    if (size < avail) {
			delta -= size;
			panePtr->size += size;
		    } else {
			delta -= avail;
			panePtr->size += avail;
			nAdjust--;
			totalWeight -= panePtr->weight;
		    }
		}
	    }
	}
    }

    /*
     * Pass 2: Adjust the panes with space still available
     */
    nAdjust = 0;
    totalWeight = 0.0f;
    for (link = Blt_Chain_LastLink(chain); link != NULL; 
	 link = Blt_Chain_PrevLink(link)) {
	Pane *panePtr;

	panePtr = Blt_Chain_GetValue(link);
	if ((panePtr->weight > 0.0f) && (panePtr->max > panePtr->size)) {
	    nAdjust++;
	    totalWeight += panePtr->weight;
	}
    }
    while ((nAdjust > 0) && (totalWeight > 0.0f) && (delta > 0)) {
	Blt_ChainLink link;
	int ration;			/* Amount of space to add to each
					 * row/column. */

	ration = (int)(delta / totalWeight);
	if (ration == 0) {
	    ration = 1;
	}
	for (link = Blt_Chain_LastLink(chain); (link != NULL) && (delta > 0);
	    link = Blt_Chain_PrevLink(link)) {
	    Pane *panePtr;

	    panePtr = Blt_Chain_GetValue(link);
	    if (panePtr->weight > 0.0f) {
		int avail;		/* Amount of space still available */

		avail = (panePtr->max - panePtr->size);
		if (avail > 0) {
		    int size;		/* Amount of space requested for a
					 * particular row/column. */
		    size = (int)(ration * panePtr->weight);
		    if (size > delta) {
			size = delta;
		    }
		    if (size < avail) {
			delta -= size;
			panePtr->size += size;
		    } else {
			delta -= avail;
			panePtr->size += avail;
			nAdjust--;
			totalWeight -= panePtr->weight;
		    }
		}
	    }
	}
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * ShrinkSpan --
 *
 *	Shrink the span by the amount specified.  Size constraints on the panes
 *	may prevent any or all of the spacing adjustments.
 *
 *	This is very much like the GrowPane procedure, but in this case we are
 *	shrinking the panes. It uses a two pass approach, first subtracting
 *	space to panes which are larger than their nominal sizes. This is
 *	because constraints on the panes may cause resizing to be non-linear.
 *
 *	After pass 1, if there is still extra to be removed, this means that all
 *	panes are at least to their nominal sizes.  The second pass will try to
 *	remove the extra space evenly among all the panes which still have space
 *	available (i.e haven't reached their respective min sizes).
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The size of the panes may be decreased.
 *
 *---------------------------------------------------------------------------
 */
static void
ShrinkSpan(Blt_Chain chain, int adjustment)
{
    Blt_ChainLink link;
    int extra;				/* Amount of space needed */
    int nAdjust;			/* Number of panes that still can be
					 * adjusted. */
    float totalWeight;

    extra = -adjustment;
    /*
     * Pass 1: First adjust the size of panes that still aren't at their
     *         nominal size.
     */

    nAdjust = 0;
    totalWeight = 0.0f;
    for (link = Blt_Chain_FirstLink(chain); link != NULL; 
	 link = Blt_Chain_NextLink(link)) {
	Pane *panePtr;

	panePtr = Blt_Chain_GetValue(link);
	if ((panePtr->weight > 0.0f) && (panePtr->nom < panePtr->size)) {
	    nAdjust++;
	    totalWeight += panePtr->weight;
	}
    }

    while ((nAdjust > 0) && (totalWeight > 0.0f) && (extra > 0)) {
	Blt_ChainLink link;
	int ration;			/* Amount of space to subtract from each
					 * row/column. */
	ration = (int)(extra / totalWeight);
	if (ration == 0) {
	    ration = 1;
	}
#if TRACE
	fprintf(stderr,"nAdjust=%d totalWeight=%g extra=%d ration=%d\n", 
		nAdjust, totalWeight, extra, ration);
#endif
	for (link = Blt_Chain_FirstLink(chain); (link != NULL) && (extra > 0);
	    link = Blt_Chain_NextLink(link)) {
	    Pane *panePtr;

	    panePtr = Blt_Chain_GetValue(link);
	    if (panePtr->weight > 0.0f) {
		int avail;		/* Amount of space still available */

		avail = panePtr->size - panePtr->nom;
		if (avail > 0) {
		    int slice;		/* Amount of space requested for a
					 * particular row/column. */
		    slice = (int)(ration * panePtr->weight);
		    if (slice > extra) {
			slice = extra;
		    }
		    if (avail > slice) {
			extra -= slice;
			panePtr->size -= slice;  
		    } else {
			extra -= avail;
			panePtr->size -= avail;
			nAdjust--;	/* Goes to zero (nominal). */
			totalWeight -= panePtr->weight;
		    }
		}
	    }
	}
    }
    /*
     * Pass 2: Now adjust the panes with space still available (i.e.
     *	       are bigger than their minimum size).
     */
    nAdjust = 0;
    totalWeight = 0.0f;
    for (link = Blt_Chain_FirstLink(chain); link != NULL; 
	 link = Blt_Chain_NextLink(link)) {
	Pane *panePtr;

	panePtr = Blt_Chain_GetValue(link);
	if ((panePtr->weight > 0.0f) && (panePtr->size > panePtr->min)) {
	    nAdjust++;
	    totalWeight += panePtr->weight;
	}
    }
    while ((nAdjust > 0) && (totalWeight > 0.0f) && (extra > 0)) {
	Blt_ChainLink link;
	int ration;			/* Amount of space to subtract from each
					 * pane. */
	ration = (int)(extra / totalWeight);
	if (ration == 0) {
	    ration = 1;
	}
#if TRACE
	fprintf(stderr,"shrink nAdjust=%d totalWeight=%g extra=%d ration=%d\n", 
		nAdjust, totalWeight, extra, ration);
#endif
	for (link = Blt_Chain_FirstLink(chain); (link != NULL) && (extra > 0);
	    link = Blt_Chain_NextLink(link)) {
	    Pane *panePtr;

	    panePtr = Blt_Chain_GetValue(link);
#if TRACE
	    fprintf(stderr,"shrink pane %s weight=%g\n", panePtr->name, 
		    panePtr->weight);
#endif
	    if (panePtr->weight > 0.0f) {
		int avail;		/* Amount of space still available */

#if TRACE
		fprintf(stderr,"shrink pane %s size=%d min=%d, extra=%d\n",
			panePtr->name, panePtr->size, panePtr->min, extra);
#endif
		avail = panePtr->size - panePtr->min;
		if (avail > 0) {
		    int slice;		/* Amount of space requested for a
					 * particular pane. */
		    slice = (int)(ration * panePtr->weight);
#if TRACE
		    fprintf(stderr,"shrink pane %s slice=%d avail=%d size=%d\n",
			    panePtr->name, slice, avail, panePtr->size);
#endif
		    if (slice > extra) {
			slice = extra;
		    }
		    if (avail > slice) {
			extra -= slice;
			panePtr->size -= slice;
		    } else {
			extra -= avail;
			panePtr->size -= avail;
			nAdjust--;
			totalWeight -= panePtr->weight;
		    }
		}
	    }
	}
    }
}


/* |pos     anchor| */
static void 
ShrinkLeftGrowRight(Paneset *setPtr, Pane *leftPtr, Pane *rightPtr, int delta)
{
    int extra;
    Pane *panePtr;
    int sashSize;
    sashSize = setPtr->sashThickness + 2 * setPtr->highlightThickness;

#if TRACE
    fprintf(stderr, "left=%s %d right=%s %d delta=%d\n",
	    leftPtr->name, leftPtr->size, rightPtr->name, 
	    rightPtr->size, delta);
#endif
    extra = delta;
    for (panePtr = leftPtr; (panePtr != NULL) && (extra > 0); 
	 panePtr = PrevPane(panePtr)) {
	int avail;			/* Space available to shrink */
	
	avail = panePtr->size - panePtr->min;
#if TRACE
	fprintf(stderr, "shrink pane %s, avail=%d extra=%d\n", 
		panePtr->name, avail, extra);
#endif
	if (avail > sashSize) {
	    if (avail > extra) {
		panePtr->size -= extra;
		extra = 0;
	    } else {
		panePtr->size -= avail;
		extra -= avail;
	    }
	}
    }
    extra = delta - extra;
    for (panePtr = rightPtr; (panePtr != NULL) && (extra > 0); 
	 panePtr = NextPane(panePtr)) {
	int avail;			/* Space available to grow. */
	
	avail = panePtr->max - panePtr->size;
#if TRACE
	fprintf(stderr, "grow pane %s, avail=%d extra=%d\n", 
		panePtr->name, avail, extra);
#endif
	if (avail > sashSize) {
	    if (avail > extra) {
		panePtr->size += extra;
		extra = 0;
	    } else {
		panePtr->size += avail;
		extra -= avail;
	    }
	}
    }
}

/* |anchor     pos| */
static void 
GrowLeftShrinkRight(Paneset *setPtr, Pane *leftPtr, Pane *rightPtr, int delta)
{
    int extra;
    Pane *panePtr;
    int sashSize;
    sashSize = setPtr->sashThickness + 2 * setPtr->highlightThickness;

#if TRACE
    fprintf(stderr, "left=%s %d right=%s %d dx=%d\n",
	    leftPtr->name, leftPtr->size, rightPtr->name, 
	    rightPtr->size, delta);
#endif
    extra = delta;
    for (panePtr = rightPtr; (panePtr != NULL) && (extra > 0); 
	 panePtr = NextPane(panePtr)) {
	int avail;			/* Space available to shrink */
	
	avail = panePtr->size - panePtr->min;
#if TRACE
	fprintf(stderr, "shrink pane %s, avail=%d extra=%d\n", 
		panePtr->name, avail, extra);
#endif
	if (avail > sashSize) {
	    if (avail > extra) {
		panePtr->size -= extra;
		extra = 0;
	    } else {
		panePtr->size -= avail;
		extra -= avail;
	    }
	}
    }
    extra = delta - extra;
    for (panePtr = leftPtr; (panePtr != NULL) && (extra > 0); 
	 panePtr = PrevPane(panePtr)) {
	int avail;			/* Space available to grow. */
	
	avail = panePtr->max - panePtr->size;
#if TRACE
	fprintf(stderr, "grow pane %s, avail=%d extra=%d\n", 
		panePtr->name, avail, extra);
#endif
	if (avail > sashSize) {
	    if (avail > extra) {
		panePtr->size += extra;
		extra = 0;
	    } else {
		panePtr->size += avail;
		extra -= avail;
	    }
	}
    }
}


/* |pos     anchor| */
static void 
ShrinkLeftGrowLast(Paneset *setPtr, Pane *leftPtr, Pane *rightPtr, int delta)
{
    int extra;
    Pane *panePtr;
    int sashSize;
    sashSize = setPtr->sashThickness + 2 * setPtr->highlightThickness;

#if TRACE
    fprintf(stderr, "left=%s %d right=%s %d delta=%d\n",
	    leftPtr->name, leftPtr->size, rightPtr->name, 
	    rightPtr->size, delta);
#endif
    extra = delta;
    for (panePtr = leftPtr; (panePtr != NULL) && (extra > 0); 
	 panePtr = PrevPane(panePtr)) {
	int avail;			/* Space available to shrink */
	
	avail = panePtr->size - panePtr->min;
#if TRACE
	fprintf(stderr, "shrink pane %s, avail=%d extra=%d\n", 
		panePtr->name, avail, extra);
#endif
	if (avail > sashSize) {
	    if (avail > extra) {
		panePtr->size -= extra;
		extra = 0;
	    } else {
		panePtr->size -= avail;
		extra -= avail;
	    }
	}
    }
#ifdef notdef
    extra = delta - extra;
    for (panePtr = LastPane(setPtr); (panePtr != leftPtr) && (extra > 0); 
	 panePtr = PrevPane(panePtr)) {
	int avail;			/* Space available to grow. */
	
	avail = panePtr->max - panePtr->size;
#if TRACE
	fprintf(stderr, "grow pane %s, avail=%d extra=%d\n", 
		panePtr->name, avail, extra);
#endif
	if (avail > sashSize) {
	    if (avail > extra) {
		panePtr->size += extra;
		extra = 0;
	    } else {
		panePtr->size += avail;
		extra -= avail;
	    }
	}
    }
#endif
}

/* |anchor     pos| */
static void 
GrowLeftShrinkLast(Paneset *setPtr, Pane *leftPtr, Pane *rightPtr, int delta)
{
    int extra;
    Pane *panePtr;
    int sashSize;
    sashSize = setPtr->sashThickness + 2 * setPtr->highlightThickness;

#if TRACE
    fprintf(stderr, "left=%s %d right=%s %d dx=%d\n",
	    leftPtr->name, leftPtr->size, rightPtr->name, 
	    rightPtr->size, delta);
#endif
    extra = delta;
#ifdef notdef
    for (panePtr = LastPane(setPtr); (panePtr != leftPtr) && (extra > 0); 
	 panePtr = PrevPane(panePtr)) {
	int avail;			/* Space available to shrink */
	
	avail = panePtr->size - panePtr->min;
#if TRACE
	fprintf(stderr, "shrink pane %s, avail=%d extra=%d\n", 
		panePtr->name, avail, extra);
#endif
	if (avail > sashSize) {
	    if (avail > extra) {
		panePtr->size -= extra;
		extra = 0;
	    } else {
		panePtr->size -= avail;
		extra -= avail;
	    }
	}
    }
    extra = delta - extra;
#endif
    for (panePtr = leftPtr; (panePtr != NULL) && (extra > 0); 
	 panePtr = PrevPane(panePtr)) {
	int avail;			/* Space available to grow. */
	
	avail = panePtr->max - panePtr->size;
#if TRACE
	fprintf(stderr, "grow pane %s, avail=%d extra=%d\n", 
		panePtr->name, avail, extra);
#endif
	if (avail > sashSize) {
	    if (avail > extra) {
		panePtr->size += extra;
		extra = 0;
	    } else {
		panePtr->size += avail;
		extra -= avail;
	    }
	}
    }
}

static Blt_Chain 
SortedSpan(Paneset *setPtr, Pane *firstPtr, Pane *lastPtr)
{
    Blt_Chain chain;
    SizeProc *proc;
    Pane *panePtr;

    proc = (setPtr->flags & VERTICAL) ? GetReqPaneHeight : GetReqPaneWidth;
    chain = Blt_Chain_Create();
    for (panePtr = firstPtr; panePtr != lastPtr; panePtr = NextPane(panePtr)) {
	int d1;
	Blt_ChainLink link, before, newLink;

	d1 = (*proc)(panePtr) - panePtr->size;
	before = NULL;
	for (link = Blt_Chain_FirstLink(chain); link != NULL; 
	     link = Blt_Chain_NextLink(link)) {
	    Pane *pane2Ptr;
	    int d2;

	    pane2Ptr = Blt_Chain_GetValue(link);
	    d2 = (*proc)(pane2Ptr) - pane2Ptr->size;
	    if (d2 >= d1) {
		before = link;
		break;
	    }
	}
	newLink = Blt_Chain_NewLink();
	Blt_Chain_SetValue(newLink, panePtr);
	if (before != NULL) {
	    Blt_Chain_LinkBefore(chain, newLink, before);
	} else {
	    Blt_Chain_LinkAfter(chain, newLink, NULL);
	}
    }
    return chain;
}

/*
 *---------------------------------------------------------------------------
 *
 * ResetPanes --
 *
 *	Sets/resets the size of each pane to the minimum limit of the pane
 *	(this is usually zero). This routine gets called when new widgets are
 *	added, deleted, or resized.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The size of each pane is re-initialized to its minimum size.
 *
 *---------------------------------------------------------------------------
 */
static void
ResetPanes(Paneset *setPtr, LimitsProc *limitsProc)
{
    Blt_ChainLink link;

#if TRACE
    fprintf(stderr, "ResetPanes\n");
#endif
    for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL;
	link = Blt_Chain_NextLink(link)) {
	Pane *panePtr;
	int pad, size;

	panePtr = Blt_Chain_GetValue(link);
	/*
	 * The constraint procedure below also has the desired side-effect of
	 * setting the minimum, maximum, and nominal values to the requested
	 * size of its associated widget (if one exists).
	 */
	size = (*limitsProc)(0, &panePtr->reqSize);
	if (setPtr->flags & VERTICAL) {
	    pad = PADDING(panePtr->yPad);
	} else {
	    pad = PADDING(panePtr->xPad);
	}
	if (panePtr->reqSize.flags & NOM_LIMIT_SET) {
	    /*
	     * This could be done more cleanly.  We want to ensure that the
	     * requested nominal size is not overridden when determining the
	     * normal sizes.  So temporarily fix min and max to the nominal size
	     * and reset them back later.
	     */
	    panePtr->min = panePtr->max = panePtr->size = panePtr->nom = 
		size + pad;
	} else {
	    /* The range defaults to 0..MAXINT */
	    panePtr->min = panePtr->reqSize.min + pad;
	    panePtr->max = panePtr->reqSize.max - pad;
	    panePtr->nom = NOM_LIMIT;
	    panePtr->size = pad;
	}
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * SetNominalSizes
 *
 *	Sets the normal sizes for each pane.  The pane size is the requested
 *	widget size plus an amount of padding.  In addition, adjust the min/max
 *	bounds of the pane depending upon the resize flags (whether the pane can
 *	be expanded or shrunk from its normal size).
 *
 * Results:
 *	Returns the total space needed for the all the panes.
 *
 * Side Effects:
 *	The nominal size of each pane is set.  This is later used to determine
 *	how to shrink or grow the table if the container can't be resized to
 *	accommodate the exact size requirements of all the panes.
 *
 *---------------------------------------------------------------------------
 */
static int
SetNominalSizes(Paneset *setPtr)
{
    Blt_ChainLink link;
    int total;

#if TRACE
    fprintf(stderr, "SetNominalSizes\n");
#endif
    total = 0;
    for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL;
	link = Blt_Chain_NextLink(link)) {
	Pane *panePtr;
	int pad, size;

	panePtr = Blt_Chain_GetValue(link);
	if (setPtr->flags & VERTICAL) {
	    pad = PADDING(panePtr->yPad);
	} else {
	    pad = PADDING(panePtr->xPad);
	}

	/*
	 * Restore the real bounds after temporarily setting nominal size.
	 * These values may have been set in ResetPanes to restrict the size of
	 * the pane to the requested range.
	 */
	panePtr->min = panePtr->reqSize.min + pad;
	panePtr->max = panePtr->reqSize.max + pad;
	size = panePtr->size;
	if (size > panePtr->max) {
	    size = panePtr->max;
	} 
	if (size < panePtr->min) {
	    size = panePtr->min;
	}
	panePtr->nom = panePtr->size = size;

	/*
	 * If a pane can't be resized (to either expand or shrink), hold its
	 * respective limit at its normal size.
	 */
	if ((panePtr->resize & RESIZE_EXPAND) == 0) {
	    panePtr->max = panePtr->nom;
	}
	if ((panePtr->resize & RESIZE_SHRINK) == 0) {
	    panePtr->min = panePtr->nom;
	}
	panePtr->nom = size;
	total += panePtr->nom;
    }
    return total;
}

/*
 *---------------------------------------------------------------------------
 *
 * LayoutHorizontalPanes --
 *
 *	Calculates the normal space requirements for panes.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The sum of normal sizes set here will be used as the normal size for
 * 	the container widget.
 *
 *---------------------------------------------------------------------------
 */
static void
LayoutHorizontalPanes(Paneset *setPtr)
{
    Blt_ChainLink link, next;
    int total;
    int maxHeight;
    int sashSize;
    int x, y;

#if TRACE
    fprintf(stderr, "LayoutHorizontalPanes(%s)\n", Tk_PathName(setPtr->tkwin));
#endif
    maxHeight = 0;
    ResetPanes(setPtr, GetBoundedWidth);
    sashSize = (setPtr->sashThickness + 2 * setPtr->highlightThickness);
    for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL; link = next) {
	Pane *panePtr;
	int width, height;

	next = Blt_Chain_NextLink(link);
	panePtr = Blt_Chain_GetValue(link);
#if TRACE
	fprintf(stderr, "pane %s: weight=%g\n", panePtr->name, panePtr->weight);
#endif
	if (panePtr->flags & HIDE) {
	    continue;
	}
	width = GetReqPaneWidth(panePtr);
	if (width <= 0) {
	    /* continue; */
	}
	panePtr->flags &= ~SASH;
	if ((next != NULL) || (setPtr->mode == ADJUST_SPREADSHEET)) {
	    /* Add the size of the sash to the pane. */
	    width += sashSize;
	    panePtr->flags |= SASH;
	}
	height = GetReqPaneHeight(panePtr);
	if (maxHeight < height) {
	    maxHeight = height;
	}
	if (width > panePtr->size) {
	    GrowPane(panePtr, width - panePtr->size);
	}
    }
    x = y = 0;
    for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL;
	link = Blt_Chain_NextLink(link)) {
	Pane *panePtr;

	panePtr = Blt_Chain_GetValue(link);
	panePtr->height = maxHeight;
	panePtr->width = panePtr->size;
	panePtr->worldX = x;
	panePtr->worldY = y;
	x += panePtr->size;
    }
    total = SetNominalSizes(setPtr);
    setPtr->worldWidth = total;
    setPtr->normalWidth = total + 2 * Tk_InternalBorderWidth(setPtr->tkwin);
    setPtr->normalHeight = maxHeight + 2*Tk_InternalBorderWidth(setPtr->tkwin);
    if (setPtr->normalWidth < 1) {
	setPtr->normalWidth = 1;
    }
    if (setPtr->normalHeight < 1) {
	setPtr->normalHeight = 1;
    }
    setPtr->flags &= ~LAYOUT_PENDING;
    setPtr->flags |= SCROLL_PENDING;
}

/*
 *---------------------------------------------------------------------------
 *
 * LayoutVerticalPanes --
 *
 *	Calculates the normal space requirements for panes.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The sum of normal sizes set here will be used as the normal size for
 * 	the container widget.
 *
 *---------------------------------------------------------------------------
 */
static void
LayoutVerticalPanes(Paneset *setPtr)
{
    Blt_ChainLink link, next;
    int total;
    int maxWidth;
    int x, y;
    int sashSize;

#if TRACE
    fprintf(stderr, "LayoutVerticallPanes(%s)\n", Tk_PathName(setPtr->tkwin));
#endif
    maxWidth = 0;
    ResetPanes(setPtr, GetBoundedHeight);
    sashSize = (setPtr->sashThickness + 2 * setPtr->highlightThickness);
    for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL; link = next) {
	Pane *panePtr;
	int width, height;
	
	next = Blt_Chain_NextLink(link);
	panePtr = Blt_Chain_GetValue(link);
	if (panePtr->flags & HIDE) {
	    continue;
	}
	height = GetReqPaneHeight(panePtr);
	if (height <= 0) {
	    continue;
	}
	if ((next != NULL) || (setPtr->mode == ADJUST_SPREADSHEET)) {
	    height += sashSize;
	    panePtr->flags |= SASH;
	}
	width = GetReqPaneWidth(panePtr);
	if (maxWidth < width) {
	    maxWidth = width;
	}
	if (height > panePtr->size) {
	    GrowPane(panePtr, height - panePtr->size);
	}
    }
    x = y = 0;
    for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL;
	link = Blt_Chain_NextLink(link)) {
	Pane *panePtr;

	panePtr = Blt_Chain_GetValue(link);
	panePtr->width = maxWidth;
	panePtr->height = panePtr->size;
	panePtr->worldX = x;
	panePtr->worldY = y;
	y += panePtr->size;
    }
    total = SetNominalSizes(setPtr);
    setPtr->worldWidth = total;
    setPtr->normalHeight = total + 2*Tk_InternalBorderWidth(setPtr->tkwin);
    setPtr->normalWidth = maxWidth + 2*Tk_InternalBorderWidth(setPtr->tkwin);
    if (setPtr->normalWidth < 1) {
	setPtr->normalWidth = 1;
    }
    if (setPtr->normalHeight < 1) {
	setPtr->normalHeight = 1;
    }
    setPtr->flags &= ~LAYOUT_PENDING;
    setPtr->flags |= SCROLL_PENDING;
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawSash
 *
 *	Draws the pane's sash at its proper location.  First determines the size
 *	and position of the each window.  It then considers the following:
 *
 *	  1. translation of widget position its parent widget.
 *	  2. fill style
 *	  3. anchor
 *	  4. external and internal padding
 *	  5. widget size must be greater than zero
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The size of each pane is re-initialized its minimum size.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawSash(Pane *panePtr, int x, int y, Drawable drawable) 
{
    int w, h;
    Blt_Background bg;
    int relief;
    Paneset *setPtr;
    int sashSize;

    setPtr = panePtr->setPtr;
    sashSize = setPtr->sashThickness + 2 * setPtr->highlightThickness;
#if TRACE 
    fprintf(stderr, "sash %s %d x=%d y=%d size=%d\n", panePtr->name, 
	    panePtr->index, x, y, panePtr->size);
#endif
    if (setPtr->flags & VERTICAL) {
	x = 0;
	y += panePtr->height - sashSize;
	w = Tk_Width(setPtr->tkwin);
	h = sashSize; 
    } else {
	y = 0;
	x += panePtr->width - sashSize;
	h = Tk_Height(setPtr->tkwin);
	w = sashSize; 
    }
    if (setPtr->activePtr == panePtr) {
	bg = GETATTR(panePtr, activeSashBg);
	relief = setPtr->activeSashRelief;
    } else {
	bg = GETATTR(panePtr, sashBg);
	relief = setPtr->sashRelief;
    }
    Blt_FillBackgroundRectangle(setPtr->tkwin, drawable, bg, x, y, w, h, 0,
	TK_RELIEF_FLAT);
    /* Draw focus highlight ring. */
    if ((setPtr->flags & FOCUS) && (setPtr->focusPtr == panePtr) &&
	(setPtr->highlightThickness > 0)) {
	XDrawRectangle(setPtr->display, drawable, setPtr->highlightGC, 
		       x + setPtr->highlightThickness/2, 
		       y + setPtr->highlightThickness/2,
		       w - setPtr->highlightThickness, 
		       h - setPtr->highlightThickness);
    }
    x += setPtr->highlightThickness;
    y += setPtr->highlightThickness;
    w  -= 2 * setPtr->highlightThickness;
    h -= 2 * setPtr->highlightThickness;
    Blt_DrawBackgroundRectangle(setPtr->tkwin, drawable, bg, x, y, w, h,  
	setPtr->sashBorderWidth, relief);
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawPane
 *
 *	Places each window at its proper location.  First determines the size
 *	and position of the each window.  It then considers the following:
 *
 *	  1. translation of widget position its parent widget.
 *	  2. fill style
 *	  3. anchor
 *	  4. external and internal padding
 *	  5. widget size must be greater than zero
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The size of each pane is re-initialized its minimum size.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawPane(Pane *panePtr, int xOffset, int yOffset, Drawable drawable) 
{
    Paneset *setPtr;
    int cavityWidth, cavityHeight;
    int dx, dy;
    int sashSize;
    int winWidth, winHeight;
    int xMax, yMax;
    int x, y;
#if TRACE1
    fprintf(stderr, "DrawPane(%s %d, x=%d, y=%d) size=%d\n", panePtr->name, 
	    panePtr->index, x, y, panePtr->size);
#endif
    setPtr = panePtr->setPtr;
    x = xOffset;
    y = yOffset;
    if (setPtr->flags & VERTICAL) {
	panePtr->height = panePtr->size;
	panePtr->width = Tk_Width(setPtr->tkwin);
    } else {
	panePtr->width = panePtr->size;
	panePtr->height = Tk_Height(setPtr->tkwin);
    }
    cavityWidth  = panePtr->width - PADDING(panePtr->xPad);
    cavityHeight = panePtr->height - PADDING(panePtr->yPad);
    sashSize = setPtr->sashThickness + 2 * setPtr->highlightThickness;
    if (panePtr->tkwin != NULL) {
	xMax = x + panePtr->width;
	yMax = y + panePtr->height;
	x += panePtr->xPad.side1 + Tk_Changes(panePtr->tkwin)->border_width;
	y += panePtr->yPad.side1 + Tk_Changes(panePtr->tkwin)->border_width;
	if (panePtr->flags & SASH) {
	    if (setPtr->flags & VERTICAL) {
		cavityHeight -= sashSize;
	    } else {
		cavityWidth -= sashSize;
	    }
	}
	
	/*
	 * Unmap any widgets that start beyond of the right edge of the
	 * container.
	 */
	if ((x >= xMax) || (y >= yMax)) {
	    if (Tk_IsMapped(panePtr->tkwin)) {
		if (Tk_Parent(panePtr->tkwin) != setPtr->tkwin) {
		    Tk_UnmaintainGeometry(panePtr->tkwin, setPtr->tkwin);
		}
		Tk_UnmapWindow(panePtr->tkwin);
	    }
	    return;
	}
	winWidth = GetReqWidth(panePtr);
	winHeight = GetReqHeight(panePtr);
	
	/*
	 *
	 * Compare the widget's requested size to the size of the cavity.
	 *
	 * 1) If the widget is larger than the cavity or if the fill flag is
	 * set, make the widget the size of the cavity. Check that the new size
	 * is within the bounds set for the widget.
	 *
	 * 2) Otherwise, position the widget in the space according to its
	 *    anchor.
	 *
	 */
	if ((cavityWidth <= winWidth) || (panePtr->fill & FILL_X)) {
	    winWidth = cavityWidth;
	    if (winWidth > panePtr->reqWidth.max) {
		winWidth = panePtr->reqWidth.max;
	    }
	}
	if ((cavityHeight <= winHeight) || (panePtr->fill & FILL_Y)) {
	    winHeight = cavityHeight;
	    if (winHeight > panePtr->reqHeight.max) {
		winHeight = panePtr->reqHeight.max;
	    }
	}
	dx = dy = 0;
	if (cavityWidth > winWidth) {
	    dx = (cavityWidth - winWidth);
	}
	if (cavityHeight > winHeight) {
	    dy = (cavityHeight - winHeight);
	}
	if ((dx > 0) || (dy > 0)) {
	    TranslateAnchor(dx, dy, panePtr->anchor, &x, &y);
	}
	/*
	 * Clip the widget at the bottom and/or right edge of the container.
	 */
	if (winWidth > (xMax - x)) {
	    winWidth = (xMax - x);
	}
	if (winHeight > (yMax - y)) {
	    winHeight = (yMax - y);
	}
	
	/*
	 * If the widget is too small (i.e. it has only an external border)
	 * then unmap it.
	 */
	if ((winWidth < 1) || (winHeight < 1)) {
	    if (Tk_IsMapped(panePtr->tkwin)) {
		if (setPtr->tkwin != Tk_Parent(panePtr->tkwin)) {
		    Tk_UnmaintainGeometry(panePtr->tkwin, setPtr->tkwin);
		}
		Tk_UnmapWindow(panePtr->tkwin);
	    }
	    goto sash;
	}
	
	/*
	 * Resize and/or move the widget as necessary.
	 */
	if (setPtr->tkwin != Tk_Parent(panePtr->tkwin)) {
	    Tk_MaintainGeometry(panePtr->tkwin, setPtr->tkwin, x, y,
				winWidth, winHeight);
	} else {
	    if ((x != Tk_X(panePtr->tkwin)) || (y != Tk_Y(panePtr->tkwin)) ||
		(winWidth != Tk_Width(panePtr->tkwin)) ||
		(winHeight != Tk_Height(panePtr->tkwin))) {
		Tk_MoveResizeWindow(panePtr->tkwin, x, y, winWidth, winHeight);
	    }
	    if (!Tk_IsMapped(panePtr->tkwin)) {
		Tk_MapWindow(panePtr->tkwin);
	    }
	}
    }
 sash:
    if (panePtr->flags & SASH) {
	DrawSash(panePtr, xOffset, yOffset, drawable);
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * DrawVerticalPanes --
 *
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The widgets in the paneset are possibly resized and redrawn.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawVerticalPanes(Paneset *setPtr, Drawable drawable)
{
    int height;
    int top, bottom;
    int y;
    int xPad, yPad;
    Pane *panePtr;

#if TRACE
    fprintf(stderr, "DrawVerticalPanes(%s)\n", Tk_PathName(setPtr->tkwin));
#endif
    /*
     * If the paneset has no children anymore, then don't do anything at all:
     * just leave the container widget's size as-is.
     */
    panePtr = FirstPane(setPtr);
    if (panePtr == NULL) {
	return;
    }
    if (setPtr->anchorPtr == NULL) {
	setPtr->anchorPtr = panePtr;
    }
    if (setPtr->flags & LAYOUT_PENDING) {
	fprintf(stderr, "layout v panes\n");
	LayoutVerticalPanes(setPtr);
    }
    /*
     * Save the width and height of the container so we know when its size has
     * changed during ConfigureNotify events.
     */
    setPtr->containerWidth  = Tk_Width(setPtr->tkwin);
    setPtr->containerHeight = Tk_Height(setPtr->tkwin);
    xPad = yPad = 2 * Tk_InternalBorderWidth(setPtr->tkwin);

    top = LeftSpan(setPtr);
    bottom = RightSpan(setPtr);
    height = top + bottom;

    /*
     * If the previous geometry request was not fulfilled (i.e. the size of the
     * container is different from paneset space requirements), try to adjust
     * size of the panes to fit the widget.
     */
    {
	Pane *firstPtr, *lastPtr;
	Blt_Chain span;
	int dy;

	dy = setPtr->anchorY - top;
	firstPtr = FirstPane(setPtr);
	lastPtr = NextPane(setPtr->anchorPtr);
	if (firstPtr != lastPtr) {
	    span = SortedSpan(setPtr, firstPtr, lastPtr);
	    if (dy > 0) {
		GrowSpan(span, dy);
	    } else {
		ShrinkSpan(span, dy);
	    }
	    top = LeftSpan(setPtr) + yPad;
	    Blt_Chain_Destroy(span);
	}
	dy = (Tk_Height(setPtr->tkwin) - setPtr->anchorY) - bottom;
	span = SortedSpan(setPtr, lastPtr, NULL);
	if (dy > 0) {
	    GrowSpan(span, dy);
	} else {
	    ShrinkSpan(span, dy);
	}
	Blt_Chain_Destroy(span);
	bottom = RightSpan(setPtr) + yPad;
	height = top + bottom;
    }

    /*
     * If after adjusting the size of the panes the space required does not
     * equal the size of the widget, do one of the following:
     *
     * 1) If it's smaller, center the paneset in the widget.
     * 2) If it's bigger, clip the panes that extend beyond the edge of the
     *    container.
     *
     * Set the row and column offsets (including the container's internal border
     * width). To be used later when positioning the widgets.
     */

#ifdef notdef
    if (height < setPtr->containerHeight) {
	y += (setPtr->containerHeight - height) / 2;
    }
#endif
    y = 0;
    for (panePtr = FirstPane(setPtr); panePtr != NULL; 
	 panePtr = NextPane(panePtr)) {
	panePtr->worldY = y;
	DrawPane(panePtr, 0, y - setPtr->scrollOffset, drawable);
	y += panePtr->size;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawHorizontalPanes --
 *
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The widgets in the paneset are possibly resized and redrawn.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawHorizontalPanes(Paneset *setPtr, Drawable drawable)
{
    int width;
    int left, right;
    int x, worldX;
    int xPad, yPad;
    Pane *panePtr;

#if TRACE
    fprintf(stderr, "DrawHorizontalPanes(%s)\n", Tk_PathName(setPtr->tkwin));
#endif
    /*
     * If the paneset has no children anymore, then don't do anything at all:
     * just leave the container widget's size as-is.
     */
    panePtr = FirstPane(setPtr);
    if (panePtr == NULL) {
	fprintf(stderr, "DrawHPanes: first pane is null\n");
	return;
    }
    if (setPtr->anchorPtr == NULL) {
	setPtr->anchorPtr = panePtr;
    }
    if (setPtr->flags & LAYOUT_PENDING) {
	fprintf(stderr, "layout h panes\n");
	LayoutHorizontalPanes(setPtr);
    }

    /*
     * Save the width and height of the container so we know when its size has
     * changed during ConfigureNotify events.
     */
    setPtr->containerWidth  = Tk_Width(setPtr->tkwin);
    setPtr->containerHeight = Tk_Height(setPtr->tkwin);
    xPad = yPad = 2 * Tk_InternalBorderWidth(setPtr->tkwin);

    left = LeftSpan(setPtr);
    right = RightSpan(setPtr);
    setPtr->worldWidth = width = left + right;
    
    /*
     * If the previous geometry request was not fulfilled (i.e. the size of the
     * paneset is different from the total panes space requirements), try to
     * adjust size of the panes to fit the widget.
     */
    if (setPtr->mode < ADJUST_SPREADSHEET) {
	Pane *firstPtr, *lastPtr;
	Blt_Chain span;
	int dx;

	dx = setPtr->anchorX - left;
	firstPtr = FirstPane(setPtr);
	lastPtr = NextPane(setPtr->anchorPtr);
	if (firstPtr != lastPtr) {
	    span = SortedSpan(setPtr, firstPtr, lastPtr);
	    if (dx > 0) {
		GrowSpan(span, dx);
	    } else {
		ShrinkSpan(span, dx);
	    }
	    left = LeftSpan(setPtr) + xPad;
	    Blt_Chain_Destroy(span);
	}
	dx = (Tk_Width(setPtr->tkwin) - setPtr->anchorX) - right;
	span = SortedSpan(setPtr, lastPtr, NULL);
	if (dx > 0) {
	    GrowSpan(span, dx);
	} else {
	    ShrinkSpan(span, dx);
	}
	Blt_Chain_Destroy(span);
	right = RightSpan(setPtr) + xPad;
	setPtr->worldWidth = width = left + right;
    }

    /*
     * If after adjusting the size of the panes the space required does
     * not equal the size of the widget, do one of the following:
     *
     * 1) If it's smaller, center the paneset in the widget.
     * 2) If it's bigger, clip the panes that extend beyond
     *    the edge of the container.
     *
     * Set the row and column offsets (including the container's internal
     * border width). To be used later when positioning the widgets.
     */

#ifdef notdef
    if (width < setPtr->containerWidth) {
	x += (setPtr->containerWidth - width) / 2;
    }
#endif
    x = 0;
    for (panePtr = FirstPane(setPtr); panePtr != NULL; 
	 panePtr = NextPane(panePtr)) {
	panePtr->worldX = x;
	DrawPane(panePtr, x - setPtr->scrollOffset, 0, drawable);
	x += panePtr->size;
    }
}

static void
ComputeGeometry(Paneset *setPtr) 
{
#if TRACE
    fprintf(stderr, "ComputeGeometry(%s)\n", Tk_PathName(setPtr->tkwin));
#endif
    if (setPtr->flags & LAYOUT_PENDING) {
	if (setPtr->flags & VERTICAL) {
	    LayoutVerticalPanes(setPtr);
	} else {
	    LayoutHorizontalPanes(setPtr);
	}
	setPtr->flags &= ~LAYOUT_PENDING;
    }
}

static void
ConfigurePaneset(Paneset *setPtr) 
{
    GC newGC;
    XGCValues gcValues;
    unsigned int gcMask;

    /*
     * GC for focus highlight.
     */
    gcMask = GCForeground | GCLineWidth | GCDashList | GCLineStyle;
    gcValues.line_style = LineOnOffDash;
    gcValues.dashes = 1;
    gcValues.foreground = setPtr->highlightColor->pixel;
    gcValues.line_width = setPtr->highlightThickness;
    newGC = Tk_GetGC(setPtr->tkwin, gcMask, &gcValues);
    if (setPtr->highlightGC != NULL) {
	Tk_FreeGC(setPtr->display, setPtr->highlightGC);
    }
    setPtr->highlightGC = newGC;
}

/*
 *---------------------------------------------------------------------------
 *
 * AddOp --
 *
 *	Appends a pane into the widget.
 *
 * Results:
 *	Returns a standard TCL result.  The index of the pane is left
 *	in interp->result.
 *
 *	.p add ?name? ?option value...?
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
AddOp(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{ 
    Paneset *setPtr = clientData;
    Pane *panePtr;
    Tcl_Obj *nameObjPtr;

    nameObjPtr = NULL;
    if (objc > 2) {
	const char *string;

	string = Tcl_GetString(objv[2]);
	if (string[0] != '-') {
	    if (GetPaneFromObj(NULL, setPtr, objv[2], &panePtr) == TCL_OK) {
		Tcl_AppendResult(interp, "pane \"", Tcl_GetString(objv[2]), 
				 "\" already exists", (char *)NULL);
		return TCL_ERROR;
	    }
	    nameObjPtr = objv[3];
	    objc--, objv++;
	}
    } 
    panePtr = NewPane(interp, setPtr, nameObjPtr);
    if (panePtr == NULL) {
	return TCL_ERROR;
    }
    if (Blt_ConfigureWidgetFromObj(interp, setPtr->tkwin, paneSpecs, objc - 2, 
	objv + 2, (char *)panePtr, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    panePtr->link = Blt_Chain_Append(setPtr->chain, panePtr);
    RenumberPanes(setPtr);
    EventuallyRedraw(setPtr);
    Tcl_SetIntObj(Tcl_GetObjResult(interp), panePtr->index);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * BindOp --
 *
 *	  .t bind index sequence command
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BindOp(
    Paneset *setPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *const *objv)
{
    ClientData tag;

    if (objc == 2) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;
	Tcl_Obj *listObjPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	for (hPtr = Blt_FirstHashEntry(&setPtr->tagTable, &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    Tcl_Obj *objPtr;
	    objPtr = Blt_GetHashValue(hPtr);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	Tcl_SetObjResult(interp, listObjPtr);
	return TCL_OK;
    }
    tag = MakeTag(setPtr, Tcl_GetString(objv[2]));
    return Blt_ConfigureBindingsFromObj(interp, setPtr->bindTable, tag, 
	objc - 3, objv + 3);
}

/*
 *---------------------------------------------------------------------------
 *
 * CgetOp --
 *
 *	Returns the name, position and options of a widget in the paneset.
 *
 * Results:
 *	Returns a standard TCL result.  A list of the widget attributes is
 *	left in interp->result.
 *
 *	.p cget option
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOp(ClientData clientData, Tcl_Interp *interp, int objc, 
       Tcl_Obj *const *objv)
{
    Paneset *setPtr = clientData;

    return Blt_ConfigureValueFromObj(interp, setPtr->tkwin, panesetSpecs, 
	(char *)setPtr, objv[2], 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 *	Returns the name, position and options of a widget in the paneset.
 *
 * Results:
 *	Returns a standard TCL result.  A list of the paneset configuration
 *	option information is left in interp->result.
 *
 *	.p configure option value
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ConfigureOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	    Tcl_Obj *const *objv)
{
    Paneset *setPtr = clientData;

    if (objc == 2) {
	return Blt_ConfigureInfoFromObj(interp, setPtr->tkwin, panesetSpecs, 
		(char *)setPtr, (Tcl_Obj *)NULL, 0);
    } else if (objc == 3) {
	return Blt_ConfigureInfoFromObj(interp, setPtr->tkwin, panesetSpecs, 
		(char *)setPtr, objv[2], 0);
    }
    if (Blt_ConfigureWidgetFromObj(interp, setPtr->tkwin, panesetSpecs,
	    objc - 2, objv + 2, (char *)setPtr, BLT_CONFIG_OBJV_ONLY) 
	!= TCL_OK) {
	return TCL_ERROR;
    }
    ConfigurePaneset(setPtr);
    /* Arrange for the paneset layout to be computed at the next idle point. */
    setPtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(setPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * DeleteOp --
 *
 *	Deletes the specified panes from the widget.  Note that the pane
 *	indices can be fixed only after all the deletions have occurred.
 *
 *	.p delete widget widget widget
 *
 * Results:
 *	Returns a standard TCL result.
 *
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeleteOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	 Tcl_Obj *const *objv)
{
    Paneset *setPtr = clientData;
    int i;

    for (i = 2; i < objc; i++) {
	Pane *panePtr;

	if (GetPaneFromObj(interp, setPtr, objv[i], &panePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	DestroyPane(panePtr);
    }
    EventuallyRedraw(setPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * InsertOp --
 *
 *	Add new entries into a pane set.
 *
 *	.t insert position ?label? option-value label option-value...
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InsertOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Pane *panePtr;
    Blt_ChainLink link, before;
    char c;
    const char *string;

    string = Tcl_GetString(objv[2]);
    c = string[0];
    if ((c == 'e') && (strcmp(string, "end") == 0)) {
	before = NULL;
    } else if (isdigit(UCHAR(c))) {
	int pos;

	if (Tcl_GetIntFromObj(interp, objv[2], &pos) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (pos < 0) {
	    before = Blt_Chain_FirstLink(setPtr->chain);
	} else if (pos > Blt_Chain_GetLength(setPtr->chain)) {
	    before = NULL;
	} else {
	    before = Blt_Chain_GetNthLink(setPtr->chain, pos);
	}
    } else {
	Pane *beforePtr;

	if (GetPaneFromObj(interp, setPtr, objv[2], &beforePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (beforePtr == NULL) {
	    Tcl_AppendResult(interp, "can't find a pane \"", 
		Tcl_GetString(objv[2]), "\" in \"", Tk_PathName(setPtr->tkwin), 
		"\"", (char *)NULL);
	    return TCL_ERROR;
	}
	before = beforePtr->link;
    }
    string = NULL;
    if (objc > 3) {
	const char *name;

	name = Tcl_GetString(objv[3]);
	if (name[0] != '-') {
	    string = name;
	    objc--, objv++;
	}
    } 
    panePtr = NewPane(interp, setPtr, objv[3]);
    if (panePtr == NULL) {
	return TCL_ERROR;
    }
    setPtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(setPtr);
    if (Blt_ConfigureComponentFromObj(interp, setPtr->tkwin, panePtr->name, 
	"Pane", paneSpecs, objc - 3, objv + 3, (char *)panePtr, 0) != TCL_OK) {
	DestroyPane(panePtr);
	return TCL_ERROR;
    }
    link = Blt_Chain_NewLink();
    if (before != NULL) {
	Blt_Chain_LinkBefore(setPtr->chain, link, before);
    } else {
	Blt_Chain_AppendLink(setPtr->chain, link);
    }
    panePtr->link = link;
    Blt_Chain_SetValue(link, panePtr);
    RenumberPanes(setPtr);
    Tcl_SetStringObj(Tcl_GetObjResult(interp), panePtr->name, -1);
    return TCL_OK;

}

/*
 *---------------------------------------------------------------------------
 *
 * InvokeOp --
 *
 * 	This procedure is called to invoke a selection command.
 *
 *	  .ps invoke index
 *
 * Results:
 *	A standard TCL result.  If TCL_ERROR is returned, then interp->result
 *	contains an error message.
 *
 * Side Effects:
 *	Configuration information, such as text string, colors, font, etc. get
 *	set; old resources get freed, if there were any.  The widget is
 *	redisplayed if needed.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InvokeOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Pane *panePtr;
    Tcl_Obj *cmdObjPtr;

    if (GetPaneFromObj(interp, setPtr, objv[2], &panePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((panePtr == NULL) || (panePtr->flags & (DISABLED|HIDE))) {
	return TCL_OK;
    }
    cmdObjPtr = GETATTR(panePtr, cmdObjPtr);
    if (cmdObjPtr != NULL) {
	Tcl_Obj **args;
	Tcl_Obj **cmdv;
	int cmdc;
	int i;
	int result;

	if (Tcl_ListObjGetElements(interp, cmdObjPtr, &cmdc, &cmdv) != TCL_OK) {
	    return TCL_ERROR;
	}
	args = Blt_AssertMalloc(sizeof(Tcl_Obj *) * (cmdc + 1));
	for (i = 0; i < cmdc; i++) {
	    args[i] = cmdv[i];
	}
	args[i] = Tcl_NewIntObj(panePtr->index);
	result = Blt_GlobalEvalObjv(interp, cmdc + 1, args);
	Blt_Free(args);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * MoveOp --
 *
 *	Moves a pane to a new location.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
MoveOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Pane *panePtr, *fromPtr;
    char c;
    const char *string;
    int isBefore;
    int length;

    if (GetPaneFromObj(interp, setPtr, objv[2], &panePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((panePtr == NULL) || (panePtr->flags & DISABLED)) {
	return TCL_OK;
    }
    string = Tcl_GetStringFromObj(objv[3], &length);
    c = string[0];
    if ((c == 'b') && (strncmp(string, "before", length) == 0)) {
	isBefore = TRUE;
    } else if ((c == 'a') && (strncmp(string, "after", length) == 0)) {
	isBefore = FALSE;
    } else {
	Tcl_AppendResult(interp, "bad key word \"", string,
	    "\": should be \"after\" or \"before\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (GetPaneFromObj(interp, setPtr, objv[4], &fromPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (fromPtr == NULL) {
	Tcl_AppendResult(interp, "can't find a pane \"", 
		Tcl_GetString(objv[4]), "\" in \"", Tk_PathName(setPtr->tkwin), 
		"\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (panePtr == fromPtr) {
	return TCL_OK;
    }
    Blt_Chain_UnlinkLink(setPtr->chain, panePtr->link);
    if (isBefore) {
	Blt_Chain_LinkBefore(setPtr->chain, panePtr->link, fromPtr->link);
    } else {
	Blt_Chain_LinkAfter(setPtr->chain, panePtr->link, fromPtr->link);
    }
    setPtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(setPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * NamesOp --
 *
 *	  .ps names pattern
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
NamesOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)	
{
    Tcl_Obj *listObjPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (objc == 2) {
	Blt_ChainLink link;

	for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL;
	    link = Blt_Chain_NextLink(link)) {
	    Pane *panePtr;

	    panePtr = Blt_Chain_GetValue(link);
	    Tcl_ListObjAppendElement(interp, listObjPtr, 
			Tcl_NewStringObj(panePtr->name, -1));
	}
    } else {
	Blt_ChainLink link;

	for (link = Blt_Chain_FirstLink(setPtr->chain); link != NULL;
	     link = Blt_Chain_NextLink(link)) {
	    Pane *panePtr;
	    int i;

	    panePtr = Blt_Chain_GetValue(link);
	    for (i = 2; i < objc; i++) {
		if (Tcl_StringMatch(panePtr->name, Tcl_GetString(objv[i]))) {
		    Tcl_ListObjAppendElement(interp, listObjPtr, 
				Tcl_NewStringObj(panePtr->name, -1));
		    break;
		}
	    }
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
NearestOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int x, y;			/* Screen coordinates of the test point. */
    Pane *panePtr;

    if ((Tk_GetPixelsFromObj(interp, setPtr->tkwin, objv[2], &x) != TCL_OK) ||
	(Tk_GetPixelsFromObj(interp, setPtr->tkwin, objv[3], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (setPtr->nVisible > 0) {
	panePtr = (Pane *)PanePickProc(setPtr, x, y, NULL);
	if ((panePtr != NULL) && ((panePtr->flags & DISABLED) == 0)) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), panePtr->name, -1);
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * PaneCgetOp --
 *
 *	Returns the name, position and options of a widget in the paneset.
 *
 * Results:
 *	Returns a standard TCL result.  A list of the widget attributes is
 *	left in interp->result.
 *
 *	.p pane cget pane option
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PaneCgetOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	   Tcl_Obj *const *objv)
{
    Paneset *setPtr = clientData;
    Pane *panePtr;

    if (GetPaneFromObj(interp, setPtr, objv[3], &panePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return Blt_ConfigureValueFromObj(interp, setPtr->tkwin, paneSpecs, 
	(char *)panePtr, objv[4], 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * PaneConfigureOp --
 *
 *	Returns the name, position and options of a widget in the paneset.
 *
 * Results:
 *	Returns a standard TCL result.  A list of the paneset configuration
 *	option information is left in interp->result.
 *
 *	.p pane configure pane option value
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PaneConfigureOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	    Tcl_Obj *const *objv)
{
    Paneset *setPtr = clientData;
    Pane *panePtr;

    if (GetPaneFromObj(interp, setPtr, objv[3], &panePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 4) {
	return Blt_ConfigureInfoFromObj(interp, panePtr->tkwin, paneSpecs, 
		(char *)panePtr, (Tcl_Obj *)NULL, 0);
    } else if (objc == 5) {
	return Blt_ConfigureInfoFromObj(interp, panePtr->tkwin, paneSpecs, 
		(char *)panePtr, objv[4], 0);
    }
    if (Blt_ConfigureWidgetFromObj(interp, panePtr->tkwin, paneSpecs, objc-4, 
	objv+4, (char *)panePtr, BLT_CONFIG_OBJV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    setPtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(setPtr);
    return TCL_OK;
}

static Blt_OpSpec paneOps[] =
{
    {"cget",       2, PaneCgetOp,      5, 5, "pane option",},
    {"configure",  2, PaneConfigureOp, 4, 0, "pane ?option value?",},
};

static int nPaneOps = sizeof(paneOps) / sizeof(Blt_OpSpec);

/*
 *---------------------------------------------------------------------------
 *
 * PaneOp --
 *
 *	This procedure is invoked to process the TCL command that corresponds
 *	to the paneset geometry manager.  See the user documentation for details
 *	on what it does.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
static int
PaneOp(ClientData clientData, Tcl_Interp *interp, int objc, 
       Tcl_Obj *const *objv)
{
    Tcl_ObjCmdProc *proc;

    proc = Blt_GetOpFromObj(interp, nPaneOps, paneOps, BLT_OP_ARG2, 
			    objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc)(clientData, interp, objc, objv);
}


static void
UpdateMark(Paneset *setPtr, int x, int y)
{
    Pane *panePtr;

    setPtr->anchorX = x;
    setPtr->anchorY = y;
    switch (setPtr->mode) {
    case ADJUST_GIVETAKE:
	{
	    Pane *leftPtr, *rightPtr;
	    int delta;
	    
	    leftPtr = setPtr->anchorPtr;
	    rightPtr = NextPane(leftPtr);
	    delta = setPtr->sashMark - setPtr->sashAnchor;
	    if (delta > 0) {
		GrowLeftShrinkRight(setPtr, leftPtr, rightPtr, delta);
	    } else if (delta < 0) {
		ShrinkLeftGrowRight(setPtr, leftPtr, rightPtr, -delta);
	    }
	} 
	break;
    case ADJUST_SPREADSHEET:
	{
	    Pane *leftPtr, *rightPtr;
	    int delta;
	    
	    leftPtr = setPtr->anchorPtr;
	    rightPtr = NextPane(leftPtr);
	    delta = setPtr->sashMark - setPtr->sashAnchor;
	    if (delta > 0) {
		GrowLeftShrinkLast(setPtr, leftPtr, rightPtr, delta);
	    } else if (delta < 0) {
		ShrinkLeftGrowLast(setPtr, leftPtr, rightPtr, -delta);
	    }
	} 
	break;
    case ADJUST_FILMSTRIP:
	{
	    int delta;
	    
	    delta = setPtr->sashMark - setPtr->sashAnchor;
	    setPtr->scrollOffset -= delta;
	}	
	break;
    case ADJUST_SLINKY:
	break;
    }
    for (panePtr = FirstPane(setPtr); panePtr != NULL; 
	 panePtr = NextPane(panePtr)) {
	panePtr->nom = panePtr->size;
    }
    setPtr->sashAnchor = setPtr->sashMark;
    setPtr->flags |= SCROLL_PENDING;
    EventuallyRedraw(setPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * SashActivateOp --
 *
 *	Checks if the pointer is over a sash and changes the cursor
 *	accordingly;
 *
 *	.p sash activate pane
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SashActivateOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	       Tcl_Obj *const *objv)
{
    Paneset *setPtr = clientData;
    Pane *panePtr;

    if (GetPaneFromObj(interp, setPtr, objv[3], &panePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (panePtr != setPtr->activePtr) {
	if (panePtr == NULL) {
	    if (setPtr->cursor != None) {
		Tk_DefineCursor(setPtr->tkwin, setPtr->cursor);
	    } else {
		Tk_UndefineCursor(setPtr->tkwin);
	    }
	} else {
	    if (setPtr->sashCursor != None) {
		Tk_DefineCursor(setPtr->tkwin, setPtr->sashCursor);
	    } 
	}
	EventuallyRedraw(setPtr);
	setPtr->activePtr = panePtr;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SashAnchorOp --
 *
 *	Set the anchor for the resize.
 *
 *	.p sash anchor x y
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SashAnchorOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	     Tcl_Obj *const *objv)
{
    Paneset *setPtr = clientData;
    Pane *panePtr;
    int x, y;

    if ((Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK) ||
	(Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)) {
	return TCL_ERROR;
    } 
    if (setPtr->activePtr == NULL) {
	return TCL_OK;		/* No sash is active. */
    }
    setPtr->sashAnchor = (setPtr->flags & VERTICAL) ? y : x;
    panePtr = setPtr->activePtr;
    setPtr->anchorPtr = panePtr;
    setPtr->anchorX = panePtr->worldX;
    setPtr->anchorY = panePtr->worldY;
    setPtr->sashCurrent = panePtr->size;
    setPtr->flags |= SASH_ACTIVE;
    /*UpdateMark(setPtr, x, y);*/
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * SashIdentifyOp --
 *
 *
 *	Returns the sash index given a root screen coordinates.
 *
 * Results:
 *	Returns a standard TCL result.
 *
 *	.p sash identify %X %Y
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
SashIdentifyOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	   Tcl_Obj *const *objv)
{
    Paneset *setPtr = clientData;
    int x, y;
    long index;

    if (Blt_GetPixelsFromObj(interp, setPtr->tkwin, objv[2], PIXELS_ANY, &x)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_GetPixelsFromObj(interp, setPtr->tkwin, objv[3], PIXELS_ANY, &y)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    index = -1;
    setPtr->activePtr = PaneSearch(setPtr, x, y);
    if (setPtr->activePtr != NULL) {
	index = setPtr->activePtr->index;
    }
    Tcl_SetLongObj(Tcl_GetObjResult(interp), index);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SashMarkOp --
 *
 *	Sets the sash mark.  The distance between the mark and the anchor
 *	is the delta to change the location of the sash from its previous
 *	location.  The sash is move to its new location and the panes are 
 *	moved and/or rearranged according to the mode.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SashMarkOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	   Tcl_Obj *const *objv)
{
    Paneset *setPtr = clientData;
    int x, y;

    if ((Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK) ||
	(Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)) {
	return TCL_ERROR;
    } 
    if (setPtr->activePtr == NULL) {
	return TCL_OK;		/* No sash is active. */
    }
    setPtr->sashMark = (setPtr->flags & VERTICAL) ? y : x;
    UpdateMark(setPtr, x, y);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SashSetOp --
 *
 *	Sets the location of the sash to coordinate (x or y) specified.
 *	The windows are move and/or arranged according to the mode.
 *
 *	.p sash set $x $y
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SashSetOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv)
{
    Paneset *setPtr = clientData;
    int x, y;

    if ((Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK) ||
	(Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)) {
	return TCL_ERROR;
    } 
    if (setPtr->activePtr == NULL) {
	return TCL_OK;		/* No sash is active. */
    }
    setPtr->flags &= ~SASH_ACTIVE;
    setPtr->sashMark = (setPtr->flags & VERTICAL) ? y : x;
    UpdateMark(setPtr, x, y);
    return TCL_OK;
}

static Blt_OpSpec sashOps[] =
{
    {"activate", 2, SashActivateOp, 4, 4, "pane"},
    {"anchor",   2, SashAnchorOp,   5, 5, "x y"},
    {"identify", 1, SashIdentifyOp, 5, 5, "x y"},
    {"mark",     1, SashMarkOp,     5, 5, "x y"},
    {"set",      1, SashSetOp,      5, 5, "x y"},
};

static int nSashOps = sizeof(sashOps) / sizeof(Blt_OpSpec);

/*
 *---------------------------------------------------------------------------
 *
 * SashOp --
 *
 *	This procedure is invoked to process the TCL command that corresponds
 *	to the paneset geometry manager.  See the user documentation for details
 *	on what it does.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *	.p sash activate pane
 *	.p sash anchor index x 
 *	.p sash mark index x
 *	.p sash set index 
 *	.p sash identify x y 
 *---------------------------------------------------------------------------
 */
static int
SashOp(ClientData clientData, Tcl_Interp *interp, int objc, 
       Tcl_Obj *const *objv)
{
    Tcl_ObjCmdProc *proc;

    proc = Blt_GetOpFromObj(interp, nSashOps, sashOps, BLT_OP_ARG2, 
	objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc)(clientData, interp, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * MotionTimerProc --
 *
 *---------------------------------------------------------------------------
 */
static void
MotionTimerProc(ClientData clientData)
{
    Paneset *setPtr = clientData;

    if (setPtr->scrollMark > setPtr->scrollOffset) {
	setPtr->scrollOffset += setPtr->scrollIncr;
	if (setPtr->scrollOffset > setPtr->scrollMark) {
	    setPtr->scrollOffset = setPtr->scrollMark;
	} 
    } else if (setPtr->scrollMark < setPtr->scrollOffset) {
	setPtr->scrollOffset -= setPtr->scrollIncr;
	if (setPtr->scrollOffset < setPtr->scrollMark) {
	    setPtr->scrollOffset = setPtr->scrollMark;
	} 
    }
    setPtr->scrollIncr += setPtr->scrollIncr;
    if (setPtr->scrollMark == setPtr->scrollOffset) {
	if (setPtr->timerToken != (Tcl_TimerToken)0) {
	    Tcl_DeleteTimerHandler(setPtr->timerToken);
	    setPtr->timerToken = 0;
	    setPtr->scrollIncr = setPtr->scrollUnits;
	}
    } else {
	setPtr->timerToken = Tcl_CreateTimerHandler(setPtr->interval, 
		MotionTimerProc, setPtr);
    }
    setPtr->flags |= SCROLL_PENDING;
    EventuallyRedraw(setPtr);
}

/*ARGSUSED*/
static int
SeeOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Pane *panePtr;

    if (GetPaneFromObj(interp, setPtr, objv[2], &panePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (panePtr != NULL) {
	int left, right, width, margin;

	width = VPORTWIDTH(setPtr);
	left = setPtr->scrollOffset;
	right = setPtr->scrollOffset + width;
	margin = 20;

#if TRACE
	fprintf(stderr, "pane %s x=%d size=%d left=%d right=%d\n", 
		panePtr->name, panePtr->worldX, panePtr->size, left, right);
#endif
	/* If the pane is partially obscured, scroll so that it's entirely in
	 * view. */
	if (panePtr->worldX < left) {
	    setPtr->scrollMark = panePtr->worldX - (width - panePtr->size)/2;
	    if ((panePtr->size + margin) < width) {
		setPtr->scrollMark -= margin;
	    }
	} else if ((panePtr->worldX + panePtr->size) >= right) {
	    setPtr->scrollMark = panePtr->worldX - (width - panePtr->size)/2;
	    if ((panePtr->size + margin) < width) {
		setPtr->scrollMark += margin;
	    }
	}
	setPtr->scrollIncr = setPtr->scrollUnits;
	setPtr->timerToken = Tcl_CreateTimerHandler(setPtr->interval, 
		MotionTimerProc, setPtr);
	setPtr->flags |= SCROLL_PENDING;
	EventuallyRedraw(setPtr);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
SizeOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int size;

    if (objc == 2) {
	size = Blt_Chain_GetLength(setPtr->chain);
    } else {
	Pane *panePtr;

	if (GetPaneFromObj(interp, setPtr, objv[2], &panePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (objc == 4) {
	    int newSize;
	    
	    if (Blt_GetPixelsFromObj(interp, setPtr->tkwin, objv[3], 
				     PIXELS_NNEG, &newSize) != TCL_OK) {
		return TCL_ERROR;
	    }
	    panePtr->size = panePtr->nom = newSize;
	    EventuallyRedraw(setPtr);
	}
	size = panePtr->size;
    } 
    Tcl_SetIntObj(Tcl_GetObjResult(interp), size);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagAddOp --
 *
 *	.t tag add tagName pane1 pane2 pane2 pane4
 *
 *---------------------------------------------------------------------------
 */
static int
TagAddOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    const char *tag;

    tag = Tcl_GetString(objv[3]);
    if (isdigit(UCHAR(tag[0]))) {
	Tcl_AppendResult(interp, "bad tag \"", tag, 
		 "\": can't start with a digit", (char *)NULL);
	return TCL_ERROR;
    }
    if (strcmp(tag, "all") == 0) {
	Tcl_AppendResult(interp, "can't add reserved tag \"", tag, "\"", 
			 (char *)NULL);
	return TCL_ERROR;
    }
    if (objc == 4) {
	/* No nodes specified.  Just add the tag. */
	AddTag(setPtr, NULL, tag);
    } else {
	int i;

	for (i = 4; i < objc; i++) {
	    Pane *panePtr;
	    PaneIterator iter;
	    
	    if (GetPaneIterator(interp, setPtr, objv[i], &iter) != TCL_OK) {
		return TCL_ERROR;
	    }
	    for (panePtr = FirstTaggedPane(&iter); panePtr != NULL; 
		 panePtr = NextTaggedPane(&iter)) {
		AddTag(setPtr, panePtr, tag);
	    }
	}
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * TagDeleteOp --
 *
 *	.t delete tagName pane1 pane2 pane3
 *
 *---------------------------------------------------------------------------
 */
static int
TagDeleteOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    const char *tag;
    Blt_HashTable *tablePtr;

    tag = Tcl_GetString(objv[3]);
    if (isdigit(UCHAR(tag[0]))) {
	Tcl_AppendResult(interp, "bad tag \"", tag, 
		 "\": can't start with a digit", (char *)NULL);
	return TCL_ERROR;
    }
    if (strcmp(tag, "all") == 0) {
	Tcl_AppendResult(interp, "can't delete reserved tag \"", tag, "\"", 
			 (char *)NULL);
        return TCL_ERROR;
    }
    tablePtr = GetTagTable(setPtr, tag);
    if (tablePtr != NULL) {
        int i;
      
        for (i = 4; i < objc; i++) {
	    Pane *panePtr;
	    PaneIterator iter;

	    if (GetPaneIterator(interp, setPtr, objv[i], &iter) != TCL_OK) {
	        return TCL_ERROR;
	    }
	    for (panePtr = FirstTaggedPane(&iter); panePtr != NULL; 
		 panePtr = NextTaggedPane(&iter)) {
		Blt_HashEntry *hPtr;

	        hPtr = Blt_FindHashEntry(tablePtr, (char *)panePtr);
	        if (hPtr != NULL) {
		    Blt_DeleteHashEntry(tablePtr, hPtr);
	        }
	   }
       }
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * TagExistsOp --
 *
 *	Returns the existence of the one or more tags in the given node.  If
 *	the node has any the tags, true is return in the interpreter.
 *
 *	.t tag exists pane tag1 tag2 tag3...
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TagExistsOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int i;
    PaneIterator iter;

    if (GetPaneIterator(interp, setPtr, objv[3], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 4; i < objc; i++) {
	const char *tag;
	Pane *panePtr;

	tag = Tcl_GetString(objv[i]);
	for (panePtr = FirstTaggedPane(&iter); panePtr != NULL; 
	     panePtr = NextTaggedPane(&iter)) {
	    if (HasTag(panePtr, tag)) {
		Tcl_SetBooleanObj(Tcl_GetObjResult(interp), TRUE);
		return TCL_OK;
	    }
	}
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), FALSE);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagForgetOp --
 *
 *	Removes the given tags from all panes.
 *
 *	.ts tag forget tag1 tag2 tag3...
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TagForgetOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int i;

    for (i = 3; i < objc; i++) {
	const char *tag;

	tag = Tcl_GetString(objv[i]);
	if (isdigit(UCHAR(tag[0]))) {
	    Tcl_AppendResult(interp, "bad tag \"", tag, 
			     "\": can't start with a digit", (char *)NULL);
	    return TCL_ERROR;
	}
	ForgetTag(setPtr, tag);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagGetOp --
 *
 *	Returns tag names for a given node.  If one of more pattern arguments
 *	are provided, then only those matching tags are returned.
 *
 *	.t tag get pane pat1 pat2...
 *
 *---------------------------------------------------------------------------
 */
static int
TagGetOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Pane *panePtr; 
    PaneIterator iter;
    Tcl_Obj *listObjPtr;

    if (GetPaneIterator(interp, setPtr, objv[3], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (panePtr = FirstTaggedPane(&iter); panePtr != NULL; 
	 panePtr = NextTaggedPane(&iter)) {
	if (objc == 4) {
	    Blt_HashEntry *hPtr;
	    Blt_HashSearch hiter;

	    for (hPtr = Blt_FirstHashEntry(&setPtr->tagTable, &hiter); 
		 hPtr != NULL; hPtr = Blt_NextHashEntry(&hiter)) {
		Blt_HashTable *tablePtr;

		tablePtr = Blt_GetHashValue(hPtr);
		if (Blt_FindHashEntry(tablePtr, (char *)panePtr) != NULL) {
		    const char *tag;
		    Tcl_Obj *objPtr;

		    tag = Blt_GetHashKey(&setPtr->tagTable, hPtr);
		    objPtr = Tcl_NewStringObj(tag, -1);
		    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
		}
	    }
	    Tcl_ListObjAppendElement(interp, listObjPtr, 
				     Tcl_NewStringObj("all", 3));
	} else {
	    int i;
	    
	    /* Check if we need to add the special tags "all" */
	    for (i = 4; i < objc; i++) {
		const char *pattern;

		pattern = Tcl_GetString(objv[i]);
		if (Tcl_StringMatch("all", pattern)) {
		    Tcl_Obj *objPtr;

		    objPtr = Tcl_NewStringObj("all", 3);
		    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
		    break;
		}
	    }
	    /* Now process any standard tags. */
	    for (i = 4; i < objc; i++) {
		Blt_HashEntry *hPtr;
		Blt_HashSearch hiter;
		const char *pattern;
		
		pattern = Tcl_GetString(objv[i]);
		for (hPtr = Blt_FirstHashEntry(&setPtr->tagTable, &hiter); 
		     hPtr != NULL; hPtr = Blt_NextHashEntry(&hiter)) {
		    const char *tag;
		    Blt_HashTable *tablePtr;

		    tablePtr = Blt_GetHashValue(hPtr);
		    tag = Blt_GetHashKey(&setPtr->tagTable, hPtr);
		    if (!Tcl_StringMatch(tag, pattern)) {
			continue;
		    }
		    if (Blt_FindHashEntry(tablePtr, (char *)panePtr) != NULL) {
			Tcl_Obj *objPtr;

			objPtr = Tcl_NewStringObj(tag, -1);
			Tcl_ListObjAppendElement(interp, listObjPtr,objPtr);
		    }
		}
	    }
	}    
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagNamesOp --
 *
 *	Returns the names of all the tags in the paneset.  If one of more node
 *	arguments are provided, then only the tags found in those nodes are
 *	returned.
 *
 *	.t tag names pane pane pane...
 *
 *---------------------------------------------------------------------------
 */
static int
TagNamesOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_Obj *listObjPtr, *objPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    objPtr = Tcl_NewStringObj("all", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    if (objc == 3) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch iter;

	for (hPtr = Blt_FirstHashEntry(&setPtr->tagTable, &iter); hPtr != NULL; 
	     hPtr = Blt_NextHashEntry(&iter)) {
	    const char *tag;
	    tag = Blt_GetHashKey(&setPtr->tagTable, hPtr);
	    objPtr = Tcl_NewStringObj(tag, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    } else {
	Blt_HashTable uniqTable;
	int i;

	Blt_InitHashTable(&uniqTable, BLT_STRING_KEYS);
	for (i = 3; i < objc; i++) {
	    PaneIterator iter;
	    Pane *panePtr;

	    if (GetPaneIterator(interp, setPtr, objPtr, &iter) != TCL_OK) {
		goto error;
	    }
	    for (panePtr = FirstTaggedPane(&iter); panePtr != NULL; 
		 panePtr = NextTaggedPane(&iter)) {
		Blt_HashEntry *hPtr;
		Blt_HashSearch hiter;
		for (hPtr = Blt_FirstHashEntry(&setPtr->tagTable, &hiter); 
		     hPtr != NULL; hPtr = Blt_NextHashEntry(&hiter)) {
		    const char *tag;
		    Blt_HashTable *tablePtr;

		    tag = Blt_GetHashKey(&setPtr->tagTable, hPtr);
		    tablePtr = Blt_GetHashValue(hPtr);
		    if (Blt_FindHashEntry(tablePtr, panePtr) != NULL) {
			int isNew;

			Blt_CreateHashEntry(&uniqTable, tag, &isNew);
		    }
		}
	    }
	}
	{
	    Blt_HashEntry *hPtr;
	    Blt_HashSearch hiter;

	    for (hPtr = Blt_FirstHashEntry(&uniqTable, &hiter); hPtr != NULL;
		 hPtr = Blt_NextHashEntry(&hiter)) {
		objPtr = Tcl_NewStringObj(Blt_GetHashKey(&uniqTable, hPtr), -1);
		Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    }
	}
	Blt_DeleteHashTable(&uniqTable);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
 error:
    Tcl_DecrRefCount(listObjPtr);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagIndicesOp --
 *
 *	Returns the indices associated with the given tags.  The indices
 *	returned will represent the union of panes for all the given tags.
 *
 *	.t tag indices tag1 tag2 tag3...
 *
 *---------------------------------------------------------------------------
 */
static int
TagIndicesOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_HashTable paneTable;
    int i;
	
    Blt_InitHashTable(&paneTable, BLT_ONE_WORD_KEYS);
    for (i = 3; i < objc; i++) {
	const char *tag;

	tag = Tcl_GetString(objv[i]);
	if (isdigit(UCHAR(tag[0]))) {
	    Tcl_AppendResult(interp, "bad tag \"", tag, 
			     "\": can't start with a digit", (char *)NULL);
	    goto error;
	}
	if (strcmp(tag, "all") == 0) {
	    break;
	} else {
	    Blt_HashTable *tablePtr;
	    
	    tablePtr = GetTagTable(setPtr, tag);
	    if (tablePtr != NULL) {
		Blt_HashEntry *hPtr;
		Blt_HashSearch iter;

		for (hPtr = Blt_FirstHashEntry(tablePtr, &iter); 
		     hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
		    Pane *panePtr;
		    int isNew;

		    panePtr = Blt_GetHashValue(hPtr);
		    if (panePtr != NULL) {
			Blt_CreateHashEntry(&paneTable, (char *)panePtr, &isNew);
		    }
		}
		continue;
	    }
	}
	Tcl_AppendResult(interp, "can't find a tag \"", tag, "\"",
			 (char *)NULL);
	goto error;
    }
    {
	Blt_HashEntry *hPtr;
	Blt_HashSearch iter;
	Tcl_Obj *listObjPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	for (hPtr = Blt_FirstHashEntry(&paneTable, &iter); hPtr != NULL; 
	     hPtr = Blt_NextHashEntry(&iter)) {
	    Pane *panePtr;
	    Tcl_Obj *objPtr;

	    panePtr = (Pane *)Blt_GetHashKey(&paneTable, hPtr);
	    objPtr = Tcl_NewLongObj(panePtr->index);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	Tcl_SetObjResult(interp, listObjPtr);
    }
    Blt_DeleteHashTable(&paneTable);
    return TCL_OK;

 error:
    Blt_DeleteHashTable(&paneTable);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagSetOp --
 *
 *	Sets one or more tags for a given pane.  Tag names can't start with a
 *	digit (to distinquish them from node ids) and can't be a reserved tag
 *	("all").
 *
 *	.t tag set pane tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
TagSetOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int i;
    PaneIterator iter;

    if (GetPaneIterator(interp, setPtr, objv[3], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 4; i < objc; i++) {
	const char *tag;
	Pane *panePtr;
	
	tag = Tcl_GetString(objv[i]);
	if (isdigit(UCHAR(tag[0]))) {
	    Tcl_AppendResult(interp, "bad tag \"", tag, 
			     "\": can't start with a digit", (char *)NULL);
	    return TCL_ERROR;
	}
	if (strcmp(tag, "all") == 0) {
	    Tcl_AppendResult(interp, "can't add reserved tag \"", tag, "\"",
			     (char *)NULL);	
	    return TCL_ERROR;
	}
	for (panePtr = FirstTaggedPane(&iter); panePtr != NULL; 
	     panePtr = NextTaggedPane(&iter)) {
	    AddTag(setPtr, panePtr, tag);
	}    
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagUnsetOp --
 *
 *	Removes one or more tags from a given pane. If a tag doesn't exist or
 *	is a reserved tag ("all"), nothing will be done and no error
 *	message will be returned.
 *
 *	.t tag unset pane tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
TagUnsetOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Pane *panePtr;
    PaneIterator iter;

    if (GetPaneIterator(interp, setPtr, objv[3], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    for (panePtr = FirstTaggedPane(&iter); panePtr != NULL; 
	 panePtr = NextTaggedPane(&iter)) {
	int i;
	for (i = 4; i < objc; i++) {
	    RemoveTag(panePtr, Tcl_GetString(objv[i]));
	}    
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagOp --
 *
 * 	This procedure is invoked to process tag operations.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side Effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec tagOps[] =
{
    {"add",     1, TagAddOp,      2, 0, "?name? ?option value...?",},
    {"delete",  1, TagDeleteOp,   2, 0, "?pane...?",},
    {"exists",  1, TagExistsOp,   4, 0, "pane ?tag...?",},
    {"forget",  1, TagForgetOp,   3, 0, "?tag...?",},
    {"get",     1, TagGetOp,      4, 0, "pane ?pattern...?",},
    {"indices", 1, TagIndicesOp,  3, 0, "?tag...?",},
    {"names",   2, TagNamesOp,    3, 0, "?pane...?",},
    {"set",     1, TagSetOp,      4, 0, "pane ?tag...",},
    {"unset",   1, TagUnsetOp,    4, 0, "pane ?tag...",},
};

static int nTagOps = sizeof(tagOps) / sizeof(Blt_OpSpec);

static int
TagOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    PanesetCmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nTagOps, tagOps, BLT_OP_ARG2,
	objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc)(setPtr, interp, objc, objv);
    return result;
}

static int
ViewOp(Paneset *setPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int width;

    width = VPORTWIDTH(setPtr);
    if (objc == 2) {
	double fract;
	Tcl_Obj *listObjPtr, *objPtr;

	/* Report first and last fractions */
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	/*
	 * Note: we are bounding the fractions between 0.0 and 1.0 to support
	 * the "canvas"-style of scrolling.
	 */
	fract = (double)setPtr->scrollOffset / setPtr->worldWidth;
	objPtr = Tcl_NewDoubleObj(FCLAMP(fract));
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	fract = (double)(setPtr->scrollOffset + width) / setPtr->worldWidth;
	objPtr = Tcl_NewDoubleObj(FCLAMP(fract));
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	Tcl_SetObjResult(interp, listObjPtr);
	return TCL_OK;
    }
    if (Blt_GetScrollInfoFromObj(interp, objc - 2, objv + 2, 
		&setPtr->scrollOffset, setPtr->worldWidth, width, 
		setPtr->scrollUnits, BLT_SCROLL_MODE_HIERBOX) != TCL_OK) {
	return TCL_ERROR;
    }
    setPtr->flags |= SCROLL_PENDING;
    EventuallyRedraw(setPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Paneset operations.
 *
 * The fields for Blt_OpSpec are as follows:
 *
 *   - operation name
 *   - minimum number of characters required to disambiguate the operation name.
 *   - function associated with operation.
 *   - minimum number of arguments required.
 *   - maximum number of arguments allowed (0 indicates no limit).
 *   - usage string
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec panesetOps[] =
{
    {"add",        1, AddOp,       3, 0, "widget ?option value?",},
    {"bind",       1, BindOp,	   2, 5, "pane ?sequence command?",},
    {"cget",       2, CgetOp,      3, 3, "option",},
    {"configure",  2, ConfigureOp, 2, 0, "?option value?",},
    {"delete",     1, DeleteOp,    3, 0, "widget ?widget?...",},
    {"insert",     3, InsertOp,    4, 0, "widget position ?option value?",},
    {"invoke",     3, InvokeOp,    3, 3, "pane",},
    {"move",       1, MoveOp,      3, 0, "pane ?option value?",},
    {"names",      1, NamesOp,     2, 0, "?pattern...?",},
    {"pane",       1, PaneOp,      2, 0, "oper ?args?",},
    {"sash",       2, SashOp,      2, 0, "oper ?args?",},
    {"see",        2, SeeOp,       3, 3, "pane",},
    {"size",       2, SizeOp,	   2, 4, "?pane? ?size?",},
    {"tag",        1, TagOp,	   2, 0, "oper args",},
    {"view",       1, ViewOp,	   2, 5, 
	"?moveto fract? ?scroll number what?",},
};

static int nPanesetOps = sizeof(panesetOps) / sizeof(Blt_OpSpec);

/*
 *---------------------------------------------------------------------------
 *
 * PanesetInstCmdDeleteProc --
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
PanesetInstCmdDeleteProc(ClientData clientData)
{
    Paneset *setPtr = clientData;

    /*
     * This procedure could be invoked either because the window was destroyed
     * and the command was then deleted (in which case tkwin is NULL) or
     * because the command was deleted, and then this procedure destroys the
     * widget.
     */
    if (setPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = setPtr->tkwin;
	setPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * PanesetInstCmdProc --
 *
 *	This procedure is invoked to process the TCL command that corresponds
 *	to the paneset geometry manager.  See the user documentation for details
 *	on what it does.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
static int
PanesetInstCmdProc(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    PanesetCmdProc *proc;

    proc = Blt_GetOpFromObj(interp, nPanesetOps, panesetOps, BLT_OP_ARG1, 
		objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc)(clientData, interp, objc, objv);
}


/*
 *---------------------------------------------------------------------------
 *
 * PanesetCmd --
 *
 * 	This procedure is invoked to process the TCL command that corresponds
 * 	to a widget managed by this module. See the user documentation for
 * 	details on what it does.
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
PanesetCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument strings. */
{
    Paneset *setPtr;
    Tcl_CmdInfo cmdInfo;

    if (objc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", 
		Tcl_GetString(objv[0]), " pathName ?option value?...\"", 
		(char *)NULL);
	return TCL_ERROR;
    }
    setPtr = NewPaneset(interp, objv[1]);
    if (setPtr == NULL) {
	goto error;
    }


#ifdef notdef
    /*
     * Invoke a procedure to initialize various bindings on treeview entries.
     * If the procedure doesn't already exist, source it from
     * "$blt_library/paneset.tcl".  We deferred sourcing the file until now so
     * that the variable $blt_library could be set within a script.
     */
    if (!Tcl_GetCommandInfo(interp, "::blt::paneset::Initialize", &cmdInfo)) {
	char cmd[200];
	sprintf_s(cmd, 200, "source [file join $blt_library paneset.tcl]\n");
	if (Tcl_GlobalEval(interp, cmd) != TCL_OK) {
	    char info[200];

	    sprintf_s(info, 200, "\n    (while loading bindings for %.50s)", 
		    Tcl_GetString(objv[0]));
	    Tcl_AddErrorInfo(interp, info);
	    goto error;
	}
    }
#endif
    if (Blt_ConfigureWidgetFromObj(interp, setPtr->tkwin, panesetSpecs, 
	objc - 2, objv + 2, (char *)setPtr, 0) != TCL_OK) {
	goto error;
    }
    ConfigurePaneset(setPtr);
    Tcl_SetStringObj(Tcl_GetObjResult(interp), Tk_PathName(setPtr->tkwin),-1);
    return TCL_OK;
  error:
    if (setPtr != NULL) {
	Tk_DestroyWindow(setPtr->tkwin);
    }
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_PanesetCmdInitProc --
 *
 *	This procedure is invoked to initialize the TCL command that
 *	corresponds to the paneset geometry manager.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates the new TCL command.
 *
 *---------------------------------------------------------------------------
 */
int
Blt_PanesetCmdInitProc(Tcl_Interp *interp)
{
    static Blt_InitCmdSpec cmdSpec = {"paneset", PanesetCmd, };

    return Blt_InitCmd(interp, "::blt", &cmdSpec);
}


/*
 *---------------------------------------------------------------------------
 *
 * DisplayPaneset --
 *
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The widgets in the paneset are possibly resized and redrawn.
 *
 *---------------------------------------------------------------------------
 */
static void
DisplayPaneset(ClientData clientData)
{
    Paneset *setPtr = clientData;
    Pixmap pixmap;

    setPtr->flags &= ~REDRAW_PENDING;
#if TRACE
    fprintf(stderr, "DisplayPaneset(%s)\n", Tk_PathName(setPtr->tkwin));
#endif
    if (setPtr->flags & LAYOUT_PENDING) {
	ComputeGeometry(setPtr);
    }
    if (setPtr->reqWidth > 0) {
	setPtr->normalWidth = setPtr->reqWidth;
    }
    if (setPtr->reqHeight > 0) {
	setPtr->normalHeight = setPtr->reqHeight;
    }
    if ((setPtr->normalWidth != Tk_ReqWidth(setPtr->tkwin)) ||
	(setPtr->normalHeight != Tk_ReqHeight(setPtr->tkwin))) {
	Tk_GeometryRequest(setPtr->tkwin, setPtr->normalWidth,
	    setPtr->normalHeight);
	EventuallyRedraw(setPtr);
	return;
    }
    if ((Tk_Width(setPtr->tkwin) <= 1) || (Tk_Height(setPtr->tkwin) <=1)) {
	/* Don't bother computing the layout until the size of the window is
	 * something reasonable. */
	return;
    }
    if (!Tk_IsMapped(setPtr->tkwin)) {
	/* The paneset's window isn't displayed, so don't bother drawing
	 * anything.  By getting this far, we've at least computed the
	 * coordinates of the new layout.  */
	return;
    }
    if (setPtr->flags & SCROLL_PENDING) {
	int width;
	width = VPORTWIDTH(setPtr);
	setPtr->scrollOffset = Blt_AdjustViewport(setPtr->scrollOffset,
	    setPtr->worldWidth, width, setPtr->scrollUnits, 
	    BLT_SCROLL_MODE_CANVAS);
	if (setPtr->scrollCmdObjPtr != NULL) {
	    Blt_UpdateScrollbar(setPtr->interp, setPtr->scrollCmdObjPtr,
		(double)setPtr->scrollOffset / setPtr->worldWidth,
		(double)(setPtr->scrollOffset + width) / setPtr->worldWidth);
	}
	setPtr->flags &= ~SCROLL_PENDING;
    }
    pixmap = Tk_GetPixmap(setPtr->display, Tk_WindowId(setPtr->tkwin),
	Tk_Width(setPtr->tkwin), Tk_Height(setPtr->tkwin), 
	Tk_Depth(setPtr->tkwin));
    /*
     * Clear the background either by tiling a pixmap or filling with
     * a solid color. Tiling takes precedence.
     */
#ifndef notdef
    Blt_FillBackgroundRectangle(setPtr->tkwin, pixmap, setPtr->bg, 
	0, 0, Tk_Width(setPtr->tkwin), Tk_Height(setPtr->tkwin), 
	setPtr->borderWidth, setPtr->relief);
#endif

    setPtr->nVisible = Blt_Chain_GetLength(setPtr->chain);
    if (setPtr->nVisible > 0) {
	if (setPtr->flags & VERTICAL) {
	    DrawVerticalPanes(setPtr, pixmap);
	} else {
	    DrawHorizontalPanes(setPtr, pixmap);
	}
    }
    XCopyArea(setPtr->display, pixmap, Tk_WindowId(setPtr->tkwin), 
	DefaultGC(Tk_Display(setPtr->tkwin), Tk_ScreenNumber(setPtr->tkwin)),
	0, 0, Tk_Width(setPtr->tkwin), Tk_Height(setPtr->tkwin), 0, 0);
    Tk_FreePixmap(setPtr->display, pixmap);
}
