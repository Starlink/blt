
/*
 * bltMenubar.c --
 *
 * This module implements a menubar widget for the BLT toolkit.
 *
 *	Copyright 2006 George A Howlett.
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
#include "bltOp.h"
#include "bltFont.h"
#include "bltText.h"
#include "bltChain.h"
#include "bltImage.h"
#include "bltHash.h"
#include "bltBgStyle.h"

#define IPAD		2	/* Internal pad between components. */
#define YPAD		2	/* External pad between components. */
#define XPAD		2	/* External pad between border and item. */

#define STATE_NORMAL    (0)	/* Draw widget normally. */
#define STATE_ACTIVE    (1<<0)	/* Widget is currently active. */
#define STATE_DISABLED  (1<<1)	/* Widget is disabled. */
#define STATE_POSTED    (1<<2)	/* Widget is currently posting its menu. */
#define STATE_MASK	(STATE_ACTIVE|STATE_DISABLED|STATE_POSTED)

#define REDRAW_PENDING  (1<<3)	/* Widget is scheduled to be redrawn. */
#define LAYOUT_PENDING  (1<<4)	/* Widget layout needs to be recomputed. */
#define FOCUS           (1<<5)	/* Widget has focus. */
#define VERTICAL	(1<<6)

#define ITEM_BUTTON	(1<<7)
#define ITEM_SEPARATOR  (1<<8)
#define ITEM_FILLER	(1<<9)
#define ITEM_MASK	(ITEM_BUTTON|ITEM_SEPARATOR|ITEM_FILLER)

#define DEF_ACTIVE_BG		STD_ACTIVE_BACKGROUND
#define DEF_ACTIVE_FG		STD_ACTIVE_FOREGROUND
#define DEF_BORDERWIDTH		"2"
#define DEF_CMD			((char *)NULL)
#define DEF_CURSOR		((char *)NULL)
#define DEF_DIRECTION		((char *)NULL)
#define DEF_DISABLED_BG		STD_DISABLED_BACKGROUND
#define DEF_DISABLED_FG		STD_DISABLED_FOREGROUND
#define DEF_ENTRY_BG		RGB_GREY90
#define DEF_FONT		STD_FONT
#define DEF_HEIGHT		"0"
#define DEF_HIGHLIGHT_BG_COLOR  ""
#define DEF_HIGHLIGHT_COLOR     "black"
#define DEF_HIGHLIGHT_WIDTH	"2"
#define DEF_ICON		((char *)NULL)
#define DEF_IMAGE		((char *)NULL)
#define DEF_JUSTIFY		"left"
#define DEF_MENU		((char *)NULL)
#define DEF_MENU_ANCHOR		"sw"
#define DEF_NORMAL_BG		STD_NORMAL_BACKGROUND
#define DEF_NORMAL_FG		STD_NORMAL_FOREGROUND
#define DEF_ORIENT		"horizonatal"
#define DEF_POSTED_BG		RGB_SKYBLUE4
#define DEF_POSTED_FG		RGB_WHITE
#define DEF_NORMAL_RELIEF	"raised"
#define DEF_POSTED_RELIEF	"flat"
#define DEF_ACTIVE_RELIEF	"raised"
#define DEF_STATE		"normal"
#define DEF_TAKE_FOCUS		"1"
#define DEF_TEXT		((char *)NULL)
#define DEF_TYPE		"button"
#define DEF_UNDERLINE		"-1"
#define DEF_WIDTH		"0"

#define SCREEN_HEIGHT(w)	HeightOfScreen(Tk_Screen(w))
#define SCREEN_WIDTH(w)		WidthOfScreen(Tk_Screen(w))

static Blt_OptionFreeProc FreeTextProc;
static Blt_OptionParseProc ObjToTextProc;
static Blt_OptionPrintProc TextToObjProc;
static Blt_CustomOption textOption = {
    ObjToTextProc, TextToObjProc, FreeTextProc, (ClientData)0
};

static Blt_OptionFreeProc FreeIconProc;
static Blt_OptionParseProc ObjToIconProc;
static Blt_OptionPrintProc IconToObjProc;
static Blt_CustomOption iconOption = {
    ObjToIconProc, IconToObjProc, FreeIconProc, (ClientData)0
};

static Blt_OptionParseProc ObjToStateProc;
static Blt_OptionPrintProc StateToObjProc;
static Blt_CustomOption stateOption = {
    ObjToStateProc, StateToObjProc, NULL, (ClientData)0
};

static Blt_OptionParseProc ObjToOrientProc;
static Blt_OptionPrintProc OrientToObjProc;
static Blt_CustomOption orientOption = {
    ObjToOrientProc, OrientToObjProc, NULL, (ClientData)0
};

static const char *emptyString = "";

/*
 * Icon --
 *
 *	Since instances of the same Tk image can be displayed in different
 *	windows with possibly different color palettes, Tk internally stores
 *	each instance in a linked list.  But if the instances are used in the
 *	same widget and therefore use the same color palette, this adds a lot
 *	of overhead, especially when deleting instances from the linked list.
 *
 *	For the menubar widget, we never need more than a single instance
 *	of an image, regardless of how many times it's used.  Cache the image,
 *	maintaining a reference count for each image used in the widget.  It's
 *	likely that the menubar widget will use many instances of the same
 *	image.
 */

typedef struct Icon {
    Tk_Image tkImage;		/* The Tk image being cached. */
    short int width, height;	/* Dimensions of the cached image. */
} *Icon;

#define IconHeight(i)	((i)->height)
#define IconWidth(i)	((i)->width)
#define IconImage(i)	((i)->tkImage)
#define IconName(i)	(Blt_NameOfImage((i)->tkImage))

typedef struct _Menubar Menubar;

typedef struct  {
    Menubar *mbarPtr;

    Blt_ChainLink link;

    Blt_HashEntry *hashPtr;

    unsigned int flags;
    /* 
     * The item contains optionally an icon and a text string. 
     */
    Icon icon;			/* If non-NULL, image to be displayed in
				 * item. */

    Icon image;			/* If non-NULL, image to be displayed instead
				 * of text in the item. */

    const char *text;		/* Text string to be displayed in the item
				 * if an image has no been designated. */

    Tk_Justify justify;		/* Justification to use for text within the
				 * item. */

    short int textLen;		/* # bytes of text. */

    short int underline;	/* Character index of character to be
				 * underlined. If -1, no character is
				 * underlined. */

    Tcl_Obj *takeFocusObjPtr;	/* Value of -takefocus option; not used in the
				 * C code, but used by keyboard traversal
				 * scripts. */
    short int iconWidth, iconHeight;
    short int entryWidth, entryHeight;
    short int textWidth, textHeight;
    
    int reqWidth, reqHeight;

    Tcl_Obj *cmdObjPtr;		/* If non-NULL, command to be executed when
				 * this menu is posted. */
    Tcl_Obj *menuObjPtr;	

    Tcl_Obj *postCmdObjPtr;	/* If non-NULL, command to be executed when
				 * this menu is posted. */
    int menuAnchor;
    short int inset;
    short int index;
    short int x, y, width, height;
    Blt_Pad xPad, yPad;
} Item;


struct _Menubar  {
    Tcl_Interp *interp;		/* Interpreter associated with menubar. */
    Tk_Window tkwin;		/* Window that embodies the menubar. If
				 * NULL, indicates the window has been
				 * destroyed but the data structures haven't
				 * yet been cleaned up.*/
    Tk_Window parent;		/* Original parent of the menubar. */
    XRectangle region;
    Display *display;		/* Display containing widget.  Used, among
				 * other things, so that resources can be
				 * freed even after tkwin has gone away. */

    Tcl_Command cmdToken;	/* Token for widget command. */
    Tk_Window floatWin;		/* Toplevel window used to hold the menubar
				 * when it's floating. */
    Item *activePtr;	/* Currently active item. */
    Item *postedPtr;	/* Currently posted item. */
    Item *focusPtr;	/* Item with focus. */

    int reqWidth, reqHeight;
     
    int relief, postedRelief, activeRelief;
    int borderWidth;
    Blt_Background normalBg;
    Blt_Background activeBg;
    Blt_Background postedBg;
    Blt_Background disabledBg;

    Tcl_Obj *takeFocusObjPtr;	/* Value of -takefocus option; not used in the
				 * C code, but used by keyboard traversal
				 * scripts. */

    /*
     * In/Out Focus Highlight Ring:
     */
    XColor *highlightColor;
    GC highlightGC;
    XColor *highlightBgColor;
    GC highlightBgGC;
    int highlightWidth;

    /* 
     * The item contains optionally an icon and a text string. 
     */
    Icon icon;			/* If non-NULL, image to be displayed in
				 * item. */

    Icon image;			/* If non-NULL, image to be displayed instead
				 * of text in the item. */

    const char *text;		/* Text string to be displayed in the item
				 * if an image has no been designated. */

    Blt_Font font;		/* Font of text to be display in item. */

    Tk_Justify justify;		/* Justification to use for text within the
				 * item. */

    int textLen;		/* # bytes of text. */

    int underline;		/* Character index of character to be
				 * underlined. If -1, no character is
				 * underlined. */

    XColor *textNormalColor;
    XColor *textActiveColor;
    XColor *textPostedColor;
    XColor *textDisabledColor;

    GC textActiveGC;
    GC textNormalGC;
    GC textPostedGC;
    GC textDisabledGC;

    Tk_Cursor cursor;		/* Current cursor for window or None. */

    int inset;
    short int width, height;

    unsigned int flags;

    Blt_Chain chain;		/* List of menu items. */
    unsigned int nextId;
    Blt_HashTable itemTable;

};


static Blt_ConfigSpec itemSpecs[] =
{
    {BLT_CONFIG_OBJ, "-command", "command", "Command", 
	DEF_CMD, Blt_Offset(Item, cmdObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_NNEG, "-height", "height", "Height", DEF_HEIGHT, 
	Blt_Offset(Item, reqHeight), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-icon", "icon", "Icon", DEF_ICON, 
	Blt_Offset(Item, icon), BLT_CONFIG_NULL_OK, &iconOption},
    {BLT_CONFIG_CUSTOM, "-image", "image", "Image", DEF_IMAGE, 
	Blt_Offset(Item, image), BLT_CONFIG_NULL_OK, &iconOption},
    {BLT_CONFIG_JUSTIFY, "-justify", "justify", "Justify", DEF_JUSTIFY, 
	Blt_Offset(Item, justify), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-menu", "menu", "Menu", DEF_MENU, 
	Blt_Offset(Item, menuObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_ANCHOR, "-menuanchor", "menuAnchor", "MenuAnchor", 
	DEF_MENU_ANCHOR, Blt_Offset(Item, menuAnchor), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_OBJ, "-postcommand", "postCommand", "PostCommand", 
	DEF_CMD, Blt_Offset(Item, postCmdObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-state", "state", "State", DEF_STATE, 
	Blt_Offset(Item, flags), BLT_CONFIG_DONT_SET_DEFAULT, &stateOption},
    {BLT_CONFIG_OBJ, "-takefocus", "takeFocus", "TakeFocus", DEF_TAKE_FOCUS, 
	Blt_Offset(Item, takeFocusObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-text", "text", "Text", DEF_TEXT, 
	Blt_Offset(Item, text), 0, &textOption},
    {BLT_CONFIG_INT, "-underline", "underline", "Underline", DEF_UNDERLINE, 
	Blt_Offset(Item, underline), BLT_CONFIG_DONT_SET_DEFAULT },
    {BLT_CONFIG_PIXELS_NNEG, "-width", "width", "Width", DEF_WIDTH, 
	Blt_Offset(Item, reqWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 
	0, 0}
};

static Blt_ConfigSpec menubarSpecs[] =
{
    {BLT_CONFIG_BACKGROUND, "-activebackground", "activeBackground",
	"ActiveBackground", DEF_ACTIVE_BG, Blt_Offset(Menubar, activeBg),0},
    {BLT_CONFIG_COLOR, "-activeforeground", "activeForeground",
	"ActiveForeground", DEF_ACTIVE_FG, 
	Blt_Offset(Menubar, textActiveColor), 0},
    {BLT_CONFIG_RELIEF, "-activerelief", "activeRelief", "Relief", 
	DEF_POSTED_RELIEF, Blt_Offset(Menubar, activeRelief), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	DEF_NORMAL_BG, Blt_Offset(Menubar, normalBg), 0},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0,0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_BORDERWIDTH, Blt_Offset(Menubar, borderWidth), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_CURSOR, Blt_Offset(Menubar, cursor), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_BACKGROUND, "-disabledbackground", "disabledBackground",
	"DisabledBackground", DEF_DISABLED_BG, 
	Blt_Offset(Menubar, disabledBg), 0},
    {BLT_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_DISABLED_FG, 
	Blt_Offset(Menubar, textDisabledColor), 0},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_FONT, "-font", "font", "Font", DEF_FONT, 
	Blt_Offset(Menubar, font), 0},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_NORMAL_FG, Blt_Offset(Menubar, textNormalColor), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-height", "height", "Height", DEF_HEIGHT, 
	Blt_Offset(Menubar, reqHeight), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_COLOR, "-highlightbackground", "highlightBackground", 
	"HighlightBackground", DEF_HIGHLIGHT_BG_COLOR, 
	Blt_Offset(Menubar, highlightBgColor), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_HIGHLIGHT_COLOR, Blt_Offset(Menubar, highlightColor), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-highlightthickness", "highlightThickness",
	"HighlightThickness", DEF_HIGHLIGHT_WIDTH, 
	Blt_Offset(Menubar, highlightWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-orient", "orient", "Orient", DEF_ORIENT, 
	Blt_Offset(Menubar, flags), BLT_CONFIG_DONT_SET_DEFAULT, &orientOption},
    {BLT_CONFIG_BACKGROUND, "-postedbackground", "postedBackground",
        "PostedBackground", DEF_POSTED_BG, Blt_Offset(Menubar, postedBg),0},
    {BLT_CONFIG_COLOR, "-postedforeground", "postedForeground",
	"PostedForeground", DEF_POSTED_FG, 
	Blt_Offset(Menubar, textPostedColor), 0},
    {BLT_CONFIG_RELIEF, "-postedrelief", "postedRelief", "PostedRelief", 
	DEF_POSTED_RELIEF, Blt_Offset(Menubar, postedRelief), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_RELIEF, "-relief", "relief", "Relief", DEF_NORMAL_RELIEF, 
	Blt_Offset(Menubar, relief), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-takefocus", "takeFocus", "TakeFocus", DEF_TAKE_FOCUS, 
	Blt_Offset(Menubar, takeFocusObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_NNEG, "-width", "width", "Width", DEF_WIDTH, 
	Blt_Offset(Menubar, reqWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 
	0, 0}
};

static Tk_EventProc FloatEventProc;
static Tk_GeomRequestProc FloatGeometryProc;
static Tk_GeomLostSlaveProc FloatCustodyProc;
static Tk_GeomMgr menubarMgrInfo =
{
    (char *)"menubar",		/* Name of geometry manager used by winfo */
    FloatGeometryProc,		/* Procedure to for new geometry requests */
    FloatCustodyProc,		/* Procedure when window is taken away */
};

static Tcl_IdleProc DisplayMenubar;
static Tcl_FreeProc DestroyMenubar;
static Tcl_FreeProc FreeFloatProc;
static Tk_EventProc MenubarEventProc;
static Tcl_ObjCmdProc MenubarInstCmdProc;
static Tcl_CmdDeleteProc MenubarInstCmdDeletedProc;

/*
 *---------------------------------------------------------------------------
 *
 * EventuallyRedraw --
 *
 *	Tells the Tk dispatcher to call the menubar display routine at the
 *	next idle point.  This request is made only if the window is displayed
 *	and no other redraw request is pending.
 *
 * Results: None.
 *
 * Side effects:
 *	The window is eventually redisplayed.
 *
 *---------------------------------------------------------------------------
 */
static void
EventuallyRedraw(Menubar *mbarPtr) 
{
    if ((mbarPtr->tkwin != NULL) && ((mbarPtr->flags & REDRAW_PENDING) == 0)) {
	mbarPtr->flags |= REDRAW_PENDING;
	Tcl_DoWhenIdle(DisplayMenubar, mbarPtr);
    }
}


static void
FreeIcon(Menubar *mbarPtr, Icon icon)
{
    Tk_FreeImage(IconImage(icon));
    Blt_Free(icon);
}

#ifdef notdef
static char *
GetInterpResult(Tcl_Interp *interp)
{
#define MAX_ERR_MSG	1023
    static char mesg[MAX_ERR_MSG+1];

    strncpy(mesg, Tcl_GetStringResult(interp), MAX_ERR_MSG);
    mesg[MAX_ERR_MSG] = '\0';
    return mesg;
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * IconChangedProc
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
    int x, int y, int w, int h,      /* Not used. */
    int imageWidth, int imageHeight) /* Not used. */
{
    Menubar *mbarPtr = clientData;

    mbarPtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(mbarPtr);
}

static int
GetIconFromObj(Tcl_Interp *interp, Menubar *mbarPtr, Tcl_Obj *objPtr, 
	       Icon *iconPtr)
{
    Tk_Image tkImage;
    const char *iconName;

    iconName = Tcl_GetString(objPtr);
    if (iconName[0] == '\0') {
	*iconPtr = NULL;
	return TCL_OK;
    }
    tkImage = Tk_GetImage(interp, mbarPtr->tkwin, iconName, IconChangedProc, 
	mbarPtr);
    if (tkImage != NULL) {
	struct Icon *ip;
	int width, height;

	ip = Blt_AssertMalloc(sizeof(struct Icon));
	Tk_SizeOfImage(tkImage, &width, &height);
	ip->tkImage = tkImage;
	ip->width = width;
	ip->height = height;
	*iconPtr = ip;
	return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * MenubarEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various events on
 * 	menubar widgets.
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
MenubarEventProc(ClientData clientData, XEvent *eventPtr)
{
    Menubar *mbarPtr = clientData;

    if (eventPtr->type == Expose) {
	if (eventPtr->xexpose.count == 0) {
	    EventuallyRedraw(mbarPtr);
	}
    } else if (eventPtr->type == ConfigureNotify) {
	EventuallyRedraw(mbarPtr);
    } else if ((eventPtr->type == FocusIn) || (eventPtr->type == FocusOut)) {
	if (eventPtr->xfocus.detail == NotifyInferior) {
	    return;
	}
	if (eventPtr->type == FocusIn) {
	    mbarPtr->flags |= FOCUS;
	} else {
	    mbarPtr->flags &= ~FOCUS;
	}
	EventuallyRedraw(mbarPtr);
    } else if (eventPtr->type == DestroyNotify) {
	if (mbarPtr->tkwin != NULL) {
	    mbarPtr->tkwin = NULL; 
	}
	if (mbarPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayMenubar, mbarPtr);
	}
	Tcl_EventuallyFree(mbarPtr, DestroyMenubar);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToStateProc --
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
ObjToStateProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representing state. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Item *itemPtr = (Item *)(widgRec);
    unsigned int *flagsPtr = (unsigned int *)(widgRec + offset);
    const char *string;
    int flag;

    string = Tcl_GetString(objPtr);
    if (strcmp(string, "disabled") == 0) {
	flag = STATE_DISABLED;
    } else if (strcmp(string, "normal") == 0) {
	flag = STATE_NORMAL;
    } else if (strcmp(string, "active") == 0) {
	Menubar *mbarPtr;

	mbarPtr = itemPtr->mbarPtr;
	if (mbarPtr->activePtr != NULL) {
	    mbarPtr->activePtr->flags &= ~STATE_ACTIVE;
	}
	flag = STATE_ACTIVE;
	mbarPtr->activePtr = itemPtr;
    } else {
	Tcl_AppendResult(interp, "unknown state \"", string, 
	    "\": should be active, disabled, or normal.", (char *)NULL);
	return TCL_ERROR;
    }
    if (itemPtr->flags & flag) {
	return TCL_OK;		/* State is already set to value. */
    }
    *flagsPtr &= ~STATE_MASK;
    *flagsPtr |= flag;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * StateToObjProc --
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
StateToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget information record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    unsigned int state = *(unsigned int *)(widgRec + offset);
    const char *string;

    switch (state & STATE_MASK) {
    case STATE_NORMAL:		string = "normal";	break;
    case STATE_ACTIVE:		string = "active";	break;
    case STATE_POSTED:		string = "posted";	break;
    case STATE_DISABLED:	string = "disabled";	break;
    default:			string = Blt_Itoa(state & STATE_MASK);
		break;
    }
    return Tcl_NewStringObj(string, -1);
}

/*ARGSUSED*/
static void
FreeIconProc(ClientData clientData, Display *display, char *widgRec, int offset)
{
    Icon icon = *(Icon *)(widgRec + offset);

    if (icon != NULL) {
	Menubar *mbarPtr = (Menubar *)widgRec;

	FreeIcon(mbarPtr, icon);
    }
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representing state. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Menubar *mbarPtr = (Menubar *)(widgRec);
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
    mbarPtr->flags |= LAYOUT_PENDING;
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
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget information record */
    int offset,			/* Offset to field in structure */
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
 * ObjToIconProc --
 *
 *	Convert a image into a hashed icon.
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
ObjToIconProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Tcl_Obj representing the new value. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Menubar *mbarPtr = (Menubar *)widgRec;
    Icon *iconPtr = (Icon *)(widgRec + offset);
    Icon icon;

    if (GetIconFromObj(interp, mbarPtr, objPtr, &icon) != TCL_OK) {
	return TCL_ERROR;
    }
    if (*iconPtr != NULL) {
	FreeIcon(mbarPtr, *iconPtr);
    }
    *iconPtr = icon;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * IconToObjProc --
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
IconToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Icon icon = *(Icon *)(widgRec + offset);
    Tcl_Obj *objPtr;

    if (icon == NULL) {
	objPtr = Tcl_NewStringObj("", 0);
    } else {
	objPtr = Tcl_NewStringObj(Blt_NameOfImage(IconImage(icon)), -1);
    }
    return objPtr;
}


/*ARGSUSED*/
static void
FreeTextProc(ClientData clientData, Display *display, char *widgRec, int offset)
{
    Item *itemPtr = (Item *)(widgRec);

    if (itemPtr->hashPtr != NULL) {
	Menubar *mbarPtr = itemPtr->mbarPtr;

	Blt_DeleteHashEntry(&mbarPtr->itemTable, itemPtr->hashPtr);
	itemPtr->hashPtr = NULL;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToTextProc --
 *
 *	Save the text and add the item to the text hashtable.
 *
 * Results:
 *	A standard TCL result. 
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToTextProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representing style. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Item *itemPtr = (Item *)(widgRec);
    Menubar *mbarPtr;
    int isNew;
    Blt_HashEntry *hPtr;
    const char *string;

    mbarPtr = itemPtr->mbarPtr;
    string = Tcl_GetString(objPtr);
    hPtr = Blt_CreateHashEntry(&mbarPtr->itemTable, string, &isNew);
    if (!isNew) {
	Tcl_AppendResult(interp, "item \"", string, "\" already exists in ",
			 Tk_PathName(mbarPtr->tkwin), (char *)NULL);
	return TCL_ERROR;
    }
    if (itemPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(&mbarPtr->itemTable, itemPtr->hashPtr);
    }
    itemPtr->hashPtr = hPtr;
    itemPtr->text = Blt_GetHashKey(&mbarPtr->itemTable, hPtr);
    Blt_SetHashValue(hPtr, itemPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TextToObjProc --
 *
 *	Return the text of the item.
 *
 * Results:
 *	The text is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
TextToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget information record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Item *itemPtr = (Item *)(widgRec);

    return Tcl_NewStringObj(itemPtr->text, -1);
}

static INLINE Item *
FirstItem(Menubar *mbarPtr)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_FirstLink(mbarPtr->chain); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	Item *itemPtr;

	itemPtr = Blt_Chain_GetValue(link);
	if ((itemPtr->flags & (ITEM_BUTTON|STATE_DISABLED)) == ITEM_BUTTON) {
	    return itemPtr;
	}
    }
    return NULL;
}

static INLINE Item *
LastItem(Menubar *mbarPtr)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_LastLink(mbarPtr->chain); link != NULL;
	 link = Blt_Chain_PrevLink(link)) {
	Item *itemPtr;

	itemPtr = Blt_Chain_GetValue(link);
	if ((itemPtr->flags & (ITEM_BUTTON|STATE_DISABLED)) == ITEM_BUTTON) {
	    return itemPtr;
	}
    }
    return NULL;
}

static Item *
NextItem(Item *itemPtr)
{
    Blt_ChainLink link;

    if (itemPtr == NULL) {
	return NULL;
    }
    for (link = Blt_Chain_NextLink(itemPtr->link); link != NULL; 
	 link = Blt_Chain_NextLink(link)) {
	itemPtr = Blt_Chain_GetValue(link);
	if ((itemPtr->flags & (ITEM_BUTTON|STATE_DISABLED)) == ITEM_BUTTON) {
	    return itemPtr;
	}
    }
    return NULL;
}

static INLINE Item *
PrevItem(Item *itemPtr)
{
    Blt_ChainLink link;

    if (itemPtr == NULL) {
	return NULL;
    }
    for (link = Blt_Chain_PrevLink(itemPtr->link); link != NULL; 
	 link = Blt_Chain_PrevLink(link)) {
	itemPtr = Blt_Chain_GetValue(link);
	if ((itemPtr->flags & (ITEM_BUTTON|STATE_DISABLED)) == ITEM_BUTTON) {
	    return itemPtr;
	}
    }
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * SearchForItem --
 *
 *	Performs a binary search for the item at the given y-offset in world
 *	coordinates.  The range of items is specified by menu indices (high
 *	and low).  The item must be (visible) in the viewport.
 *
 * Results:
 *	Returns 0 if no item is found, other the index of the item (menu
 *	indices start from 1).
 *
 *---------------------------------------------------------------------------
 */
static Item *
SearchForItem(Menubar *mbarPtr, int x, int y)
{
    Item *itemPtr;

    for (itemPtr = FirstItem(mbarPtr); itemPtr != NULL;
	 itemPtr = NextItem(itemPtr)) {

	if ((x >= itemPtr->x) && (x < (itemPtr->x + itemPtr->width))) {
	    return itemPtr;
	}
    }
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * NearestItem --
 *
 *	Find the item closest to the x-y screen coordinate given.  The
 *	item must be (visible) in the viewport.
 *
 * Results:
 *	Returns the closest item.  If selectOne is set, then always returns
 *	an item (unless the menu is empty).  Otherwise, NULL is returned is
 *	the pointer is not over an item.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Item *
NearestItem(Menubar *mbarPtr, int x, int y, int selectOne)
{
    Item *itemPtr;

    if ((x < 0) || (x >= Tk_Width(mbarPtr->tkwin)) || 
	(y < 0) || (y >= Tk_Height(mbarPtr->tkwin))) {
	return NULL;		/* Screen coordinates are outside of menu. */
    }
    itemPtr = SearchForItem(mbarPtr, x, y);
    if (itemPtr == NULL) {
	if (!selectOne) {
	    return NULL;
	}
	if (x < mbarPtr->borderWidth) {
	    return FirstItem(mbarPtr);
	}
	return LastItem(mbarPtr);
    }
    return itemPtr;
}

/* 
 *---------------------------------------------------------------------------
 *  H
 *  C
 *  L
 *  P
 *  max of icon/text/image/item
 *  P
 *  L
 *  C
 *  H
 *
 * |H|C|L|P| icon |P| text/image |P|L|B| item |B|C|H|
 * 
 * H = highlight thickness
 * C = menubar borderwidth
 * L = label borderwidth
 * P = pad
 * I = icon
 * T = text or image
 *---------------------------------------------------------------------------
 */
static void
ComputeItemGeometry(Item *itemPtr)
{
    Menubar *mbarPtr = itemPtr->mbarPtr;

    /* Determine the height of the item.  It's the maximum height of all
     * it's components: icon, label, and item. */
    itemPtr->iconWidth = itemPtr->iconHeight = 0;
    itemPtr->entryWidth = itemPtr->entryHeight = 0;
    itemPtr->textWidth = itemPtr->textHeight = 0;
    itemPtr->inset = mbarPtr->highlightWidth;

    if (itemPtr->icon != NULL) {
	itemPtr->iconWidth  = IconWidth(itemPtr->icon);
	itemPtr->iconHeight = IconHeight(itemPtr->icon);
    }
    itemPtr->entryWidth += itemPtr->iconWidth;
    if (itemPtr->entryHeight < itemPtr->iconHeight) {
	itemPtr->entryHeight = itemPtr->iconHeight;
    }
    if (itemPtr->image != NULL) {
	itemPtr->textWidth  = IconWidth(itemPtr->image);
	itemPtr->textHeight = IconHeight(itemPtr->image);
    } else if (itemPtr->text != NULL) {
	unsigned int w, h;

	Blt_GetTextExtents(mbarPtr->font, 0, itemPtr->text, itemPtr->textLen, 
		&w, &h);
	itemPtr->textWidth  = w + 2 * IPAD;
	itemPtr->textHeight = h;
    }
    itemPtr->entryWidth += itemPtr->textWidth + IPAD;
    if (itemPtr->iconWidth == 0) {
	itemPtr->entryWidth += IPAD;
    }
    if (itemPtr->entryHeight < itemPtr->textHeight) {
	itemPtr->entryHeight = itemPtr->textHeight;
    }
    itemPtr->entryHeight += 2 * YPAD;
    itemPtr->entryWidth += 2 * XPAD;
    itemPtr->width = itemPtr->entryWidth + 2 * itemPtr->inset;
    itemPtr->height = itemPtr->entryHeight + 2 * itemPtr->inset;
    itemPtr->flags &= ~LAYOUT_PENDING;
}

static void
ComputeMenubarGeometry(Menubar *mbarPtr)
{
    Blt_ChainLink link;
    int w, h;
    int maxWidth, maxHeight;
    int totalWidth, totalHeight;

    mbarPtr->width = mbarPtr->height = 0;
    mbarPtr->inset = mbarPtr->borderWidth + mbarPtr->highlightWidth;
    if (mbarPtr->flags & LAYOUT_PENDING) {
	for (link = Blt_Chain_FirstLink(mbarPtr->chain); link != NULL;
	     link = Blt_Chain_NextLink(link)) {
	    Item *itemPtr;
	    
	    itemPtr = Blt_Chain_GetValue(link);
	    ComputeItemGeometry(itemPtr);
	}
    }
    totalWidth = totalHeight = 0;
    maxWidth = maxHeight = 0;
    for (link = Blt_Chain_FirstLink(mbarPtr->chain); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	Item *itemPtr;

	itemPtr = Blt_Chain_GetValue(link);
	ComputeItemGeometry(itemPtr);
	if (itemPtr->width > maxWidth) {
	    maxWidth = itemPtr->width;
	}
	if (itemPtr->height > maxHeight) {
	    maxHeight = itemPtr->height;
	}
	totalWidth  += itemPtr->width + IPAD;
	totalHeight += itemPtr->height + IPAD;
    }
    if (mbarPtr->flags & VERTICAL) {
	mbarPtr->height = totalHeight;
	mbarPtr->width = maxWidth;
    } else {
	mbarPtr->height = totalHeight;
	mbarPtr->width = maxWidth;
    }
    if (mbarPtr->reqHeight > 0) {
	mbarPtr->width = mbarPtr->reqHeight;
    } 
    if (mbarPtr->reqWidth > 0) {
	mbarPtr->height = mbarPtr->reqWidth;
    }
    mbarPtr->height += 2 * mbarPtr->inset;
    mbarPtr->width += 2 * mbarPtr->inset;
    if ((mbarPtr->width != Tk_ReqWidth(mbarPtr->tkwin)) || 
	(mbarPtr->height != Tk_ReqHeight(mbarPtr->tkwin))) {
	Tk_GeometryRequest(mbarPtr->tkwin, mbarPtr->width, mbarPtr->height);
    }
    mbarPtr->flags &= ~LAYOUT_PENDING;
}

static int
ConfigureMenubar(Tcl_Interp *interp, Menubar *mbarPtr, int objc, 
		 Tcl_Obj *const *objv, int flags)
{
    unsigned int gcMask;
    XGCValues gcValues;
    GC newGC;

    if (Blt_ConfigureWidgetFromObj(interp, mbarPtr->tkwin, menubarSpecs, objc, 
		objv, (char *)mbarPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    gcMask = GCForeground | GCFont;
    gcValues.font = Blt_FontId(mbarPtr->font);
    /* Text GCs. */
    gcValues.foreground = mbarPtr->textNormalColor->pixel;
    newGC = Tk_GetGC(mbarPtr->tkwin, gcMask, &gcValues);
    if (mbarPtr->textNormalGC != NULL) {
	Tk_FreeGC(mbarPtr->display, mbarPtr->textNormalGC);
    }
    mbarPtr->textNormalGC = newGC;

    gcValues.foreground = mbarPtr->textActiveColor->pixel;
    newGC = Tk_GetGC(mbarPtr->tkwin, gcMask, &gcValues);
    if (mbarPtr->textActiveGC != NULL) {
	Tk_FreeGC(mbarPtr->display, mbarPtr->textActiveGC);
    }
    mbarPtr->textActiveGC = newGC;

    gcValues.foreground = mbarPtr->textPostedColor->pixel;
    newGC = Tk_GetGC(mbarPtr->tkwin, gcMask, &gcValues);
    if (mbarPtr->textPostedGC != NULL) {
	Tk_FreeGC(mbarPtr->display, mbarPtr->textPostedGC);
    }
    mbarPtr->textPostedGC = newGC;

    gcValues.foreground = mbarPtr->textDisabledColor->pixel;
    newGC = Tk_GetGC(mbarPtr->tkwin, gcMask, &gcValues);
    if (mbarPtr->textDisabledGC != NULL) {
	Tk_FreeGC(mbarPtr->display, mbarPtr->textDisabledGC);
    }
    mbarPtr->textDisabledGC = newGC;

    /* Focus highlight GCs */
    gcMask = GCForeground;
    gcValues.foreground = mbarPtr->highlightColor->pixel;
    newGC = Tk_GetGC(mbarPtr->tkwin, gcMask, &gcValues);
    if (mbarPtr->highlightGC != NULL) {
	Tk_FreeGC(mbarPtr->display, mbarPtr->highlightGC);
    }
    mbarPtr->highlightGC = newGC;
    if (mbarPtr->highlightBgColor != NULL) {
	gcValues.foreground = mbarPtr->highlightBgColor->pixel;
	newGC = Tk_GetGC(mbarPtr->tkwin, gcMask, &gcValues);
    } else {
	newGC = NULL;
    }
    if (mbarPtr->highlightBgGC != NULL) {
	Tk_FreeGC(mbarPtr->display, mbarPtr->highlightBgGC);
    }
    mbarPtr->highlightBgGC = newGC;
    ComputeMenubarGeometry(mbarPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * DestroyMenubar --
 *
 * 	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release to
 * 	clean up the internal structure of the widget at a safe time (when
 * 	no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Everything associated with the widget is freed up.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroyMenubar(DestroyData dataPtr)	/* Pointer to the widget record. */
{
    Menubar *mbarPtr = (Menubar *)dataPtr;

    Blt_FreeOptions(menubarSpecs, (char *)mbarPtr, mbarPtr->display, 0);
    if (mbarPtr->textNormalGC != NULL) {
	Tk_FreeGC(mbarPtr->display, mbarPtr->textNormalGC);
    }
    if (mbarPtr->textActiveGC != NULL) {
	Tk_FreeGC(mbarPtr->display, mbarPtr->textActiveGC);
    }
    if (mbarPtr->textDisabledGC != NULL) {
	Tk_FreeGC(mbarPtr->display, mbarPtr->textDisabledGC);
    }
    if (mbarPtr->textPostedGC != NULL) {
	Tk_FreeGC(mbarPtr->display, mbarPtr->textPostedGC);
    }
    if (mbarPtr->highlightGC != NULL) {
	Tk_FreeGC(mbarPtr->display, mbarPtr->highlightGC);
    }
    if (mbarPtr->highlightBgGC != NULL) {
	Tk_FreeGC(mbarPtr->display, mbarPtr->highlightBgGC);
    }
    Tcl_DeleteCommandFromToken(mbarPtr->interp, mbarPtr->cmdToken);
    Blt_Free(mbarPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * NewMenubar --
 *
 *---------------------------------------------------------------------------
 */
static Menubar *
NewMenubar(Tcl_Interp *interp, Tk_Window tkwin)
{
    Menubar *mbarPtr;

    mbarPtr = Blt_AssertCalloc(1, sizeof(Menubar));

    mbarPtr->borderWidth = 1;
    mbarPtr->display = Tk_Display(tkwin);
    mbarPtr->flags = (LAYOUT_PENDING | STATE_NORMAL);
    mbarPtr->highlightWidth = 2;
    mbarPtr->interp = interp;
    mbarPtr->relief = TK_RELIEF_RAISED;
    mbarPtr->postedRelief = TK_RELIEF_FLAT;
    mbarPtr->activeRelief = TK_RELIEF_RAISED;
    mbarPtr->tkwin = tkwin;
    mbarPtr->parent = Tk_Parent(tkwin);
    return mbarPtr;
}

static INLINE Item *
FindItemByIndex(Menubar *mbarPtr, long index)
{
    Blt_ChainLink link;

    if ((index < 1) || (index > Blt_Chain_GetLength(mbarPtr->chain))) {
	return NULL;
    }
    link = Blt_Chain_GetNthLink(mbarPtr->chain, index - 1);
    return Blt_Chain_GetValue(link);
}

static INLINE Item *
FindItemByLabel(Menubar *mbarPtr, const char *string)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&mbarPtr->itemTable, string);
    if (hPtr != NULL) {
	return Blt_GetHashValue(hPtr);
    }
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetItemFromObj --
 *
 *	Gets the item associated the given index or label.  Converts a
 *	string representing a item index into an item pointer.  The index
 *	may be in one of the following forms:
 *
 *	 number		Item at index in the list of items.
 *	 @x,y		Item closest to the specified X-Y screen coordinates.
 *	 "active"	Item where mouse pointer is located.
 *	 "posted"       Item is the currently posted cascade item.
 *	 "next"		Next item from the focus item.
 *	 "previous"	Previous item from the focus item.
 *	 "end"		Last item.
 *	 "none"		No item.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.  The
 *	pointer to the node is returned via itemPtrPtr.  Otherwise, TCL_ERROR
 *	is returned and an error message is left in interpreter's result
 *	field.
 *
 *---------------------------------------------------------------------------
 */
static int 
GetItemFromObj(Tcl_Interp *interp, Menubar *mbarPtr, Tcl_Obj *objPtr,
		 Item **itemPtrPtr)
{
    Item *itemPtr;
    const char *string;
    char c;
    long lval;

    if (mbarPtr->flags & LAYOUT_PENDING) {
	ComputeMenubarGeometry(mbarPtr);
    }
    string = Tcl_GetString(objPtr);
    c = string[0];

    itemPtr = NULL;
    if (c == '\0') {
	itemPtr = NULL;
    } else if (Tcl_GetLongFromObj(NULL, objPtr, &lval) == TCL_OK) {
	if (lval < 0) {
	    itemPtr = NULL;
	} else {
	    itemPtr = FindItemByIndex(mbarPtr, (long)lval);
	}
	if (itemPtr == NULL) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "can't find item: bad index \"", 
				 Tcl_GetString(objPtr), "\"", (char *)NULL);
	    }
	    return TCL_ERROR;
	}		
    } else if ((c == 'a') && (strcmp(string, "active") == 0)) {
	itemPtr = mbarPtr->activePtr;
    } else if ((c == 'n') && (strcmp(string, "next") == 0)) {
	itemPtr = NextItem(mbarPtr->activePtr);
	if (itemPtr == NULL) {
	    itemPtr = mbarPtr->activePtr;
	}
    } else if ((c == 'p') && (strcmp(string, "item_previous") == 0)) {
	itemPtr = PrevItem(mbarPtr->activePtr);
	if (itemPtr == NULL) {
	    itemPtr = mbarPtr->activePtr;
	}
    } else if ((c == 'e') && (strcmp(string, "end") == 0)) {
	itemPtr = LastItem(mbarPtr);
    } else if ((c == 'f') && (strcmp(string, "first") == 0)) {
	itemPtr = FirstItem(mbarPtr);
    } else if ((c == 'l') && (strcmp(string, "last") == 0)) {
	itemPtr = LastItem(mbarPtr);
    } else if ((c == 'n') && (strcmp(string, "none") == 0)) {
	itemPtr = NULL;
    } else if (c == '@') {
	int x, y;

	if (Blt_GetXY(mbarPtr->interp, mbarPtr->tkwin, string, &x, &y) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	itemPtr = NearestItem(mbarPtr, x, y, TRUE);
	if ((itemPtr != NULL) && (itemPtr->flags & STATE_DISABLED)) {
	    itemPtr = NextItem(itemPtr);
	}
    } else {
	itemPtr = FindItemByLabel(mbarPtr, string);
	if (itemPtr == NULL) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "can't find item \"", string,
			"\" in \"", Tk_PathName(mbarPtr->tkwin), "\"", 
		(char *)NULL);
	    }
	    return TCL_ERROR;
	}
    }
    *itemPtrPtr = itemPtr;
    return TCL_OK;
}

static void
RenumberItems(Menubar *mbarPtr) 
{
    int count = 0;
    Blt_ChainLink link;

    for (link = Blt_Chain_FirstLink(mbarPtr->chain); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	Item *itemPtr;

	itemPtr = Blt_Chain_GetValue(link);
	itemPtr->index = count;
	count++;
    }
}

static Item *
NewItem(Menubar *mbarPtr, int type)
{
    Item *itemPtr;
    Blt_ChainLink link;
    Blt_HashEntry *hPtr;
    int isNew;

    link = Blt_Chain_AllocLink(sizeof(Item));
    itemPtr = Blt_Chain_GetValue(link);
    memset(itemPtr, 0, sizeof(Item));
    do {
	char string[200];

	sprintf(string, "item%d", mbarPtr->nextId++);
	hPtr = Blt_CreateHashEntry(&mbarPtr->itemTable, string, &isNew);
    } while (!isNew);
    itemPtr->hashPtr = hPtr;
    itemPtr->text = Blt_GetHashKey(&mbarPtr->itemTable, hPtr);
    itemPtr->textLen = strlen(itemPtr->text);
    itemPtr->mbarPtr = mbarPtr;
    itemPtr->flags = STATE_NORMAL | type;
    itemPtr->link = link;
    itemPtr->index = Blt_Chain_GetLength(mbarPtr->chain) + 1;
    itemPtr->menuAnchor = TK_ANCHOR_SW;
    itemPtr->underline = -1;
    return itemPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * DestroyItem --
 *
 * 	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release to
 * 	clean up the internal structure of the widget at a safe time (when
 * 	no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Everything associated with the widget is freed up.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroyItem(Item *itemPtr)
{
    Menubar *mbarPtr = itemPtr->mbarPtr;

    Blt_FreeOptions(itemSpecs, (char *)itemPtr, mbarPtr->display, 0);
    Blt_Chain_DeleteLink(mbarPtr->chain, itemPtr->link);
}

static int
ConfigureItem(Tcl_Interp *interp, Item *itemPtr, int objc, 
		Tcl_Obj *const *objv, int flags)
{
    Menubar *mbarPtr;

    mbarPtr = itemPtr->mbarPtr;
    iconOption.clientData = mbarPtr;
    if (Blt_ConfigureWidgetFromObj(interp, mbarPtr->tkwin, itemSpecs, 
	objc, objv, (char *)itemPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    ComputeItemGeometry(itemPtr);
    return TCL_OK;
}

static int
PostMenu(Item *itemPtr)
{
    const char *menuName;
    Tk_Window menuWin;
    Menubar *mbarPtr;
    Tcl_Interp *interp;

    mbarPtr = itemPtr->mbarPtr;
    interp = itemPtr->mbarPtr->interp;
    if (itemPtr->menuObjPtr == NULL) {
	return TCL_OK;
    }
    if ((itemPtr->flags & (ITEM_BUTTON|STATE_DISABLED|STATE_POSTED)) !=
	ITEM_BUTTON) {
	return TCL_OK;
    }
    menuName = Tcl_GetString(itemPtr->menuObjPtr);
    menuWin = Tk_NameToWindow(interp, menuName, mbarPtr->tkwin);
    if (menuWin == NULL) {
	return TCL_ERROR;
    }
    if (Tk_Parent(menuWin) != mbarPtr->tkwin) {
	Tcl_AppendResult(interp, "can't post \"", Tk_PathName(menuWin), 
		"\": it isn't a descendant of ", Tk_PathName(mbarPtr->tkwin),
		(char *)NULL);
	return TCL_ERROR;
    }
    if (itemPtr->postCmdObjPtr) {
	int result;

	Tcl_Preserve(itemPtr);
	Tcl_IncrRefCount(itemPtr->postCmdObjPtr);
	result = Tcl_EvalObjEx(interp, itemPtr->postCmdObjPtr, 
		TCL_EVAL_GLOBAL);
	Tcl_DecrRefCount(itemPtr->postCmdObjPtr);
	Tcl_Release(itemPtr);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (Tk_IsMapped(mbarPtr->tkwin)) {
	Tcl_Obj *cmd[2];
	int result;

	Tcl_Preserve(mbarPtr);
	cmd[0] = itemPtr->menuObjPtr;
	cmd[1] = Tcl_NewStringObj("post", 4);
	Tcl_IncrRefCount(cmd[0]);
	Tcl_IncrRefCount(cmd[1]);
	result = Tcl_EvalObjv(interp, 2, cmd, 0);
	Tcl_DecrRefCount(cmd[1]);
	Tcl_DecrRefCount(cmd[0]);
	Tcl_Release(mbarPtr);
	if (result == TCL_OK) {
	    itemPtr->flags &= ~STATE_MASK;
	    itemPtr->flags |= STATE_POSTED;
	    mbarPtr->postedPtr = itemPtr;
	}
	EventuallyRedraw(mbarPtr);
	return result;
    }
    return TCL_OK;
}

static int
UnpostMenu(Item *itemPtr)
{
    const char *menuName;
    Tk_Window menuWin;
    Menubar *mbarPtr;
    Tcl_Interp *interp;

    if ((itemPtr->menuObjPtr == NULL) || 
	((itemPtr->flags & STATE_POSTED) == 0)) {
	return TCL_OK;
    }
    mbarPtr = itemPtr->mbarPtr;
    interp = mbarPtr->interp;
    itemPtr->flags &= ~STATE_MASK;
    itemPtr->flags |= STATE_NORMAL;
    menuName = Tcl_GetString(itemPtr->menuObjPtr);
    menuWin = Tk_NameToWindow(interp, menuName, mbarPtr->tkwin);
    if (menuWin == NULL) {
	return TCL_ERROR;
    }
    if (Tk_Parent(menuWin) != mbarPtr->tkwin) {
	Tcl_AppendResult(interp, "can't unpost \"", Tk_PathName(menuWin), 
		"\": it isn't a descendant of ", Tk_PathName(mbarPtr->tkwin), 
		(char *)NULL);
	return TCL_ERROR;
    }
    if (Tk_IsMapped(menuWin)) {
	Tk_UnmapWindow(menuWin);
    }
    return TCL_OK;
}



static void
DestroyFloat(Menubar *mbarPtr)
{
    if (mbarPtr->floatWin != NULL) {
	if (mbarPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayMenubar, mbarPtr);
	}
	Tk_DeleteEventHandler(mbarPtr->floatWin, StructureNotifyMask, 
			      FloatEventProc, mbarPtr);
	if (mbarPtr->tkwin != NULL) {

	    Blt_RelinkWindow(mbarPtr->tkwin, mbarPtr->parent, 0, 0);
	    Tk_MoveResizeWindow(mbarPtr->tkwin, 
				mbarPtr->region.x, mbarPtr->region.y, 
				mbarPtr->region.width, mbarPtr->region.height);
	    if (!Tk_IsMapped(mbarPtr->tkwin)) {
		Tk_MapWindow(mbarPtr->tkwin);
	    }
	}
	Tk_DestroyWindow(mbarPtr->floatWin);
	mbarPtr->floatWin = NULL;
    }
}

static void
FreeFloatProc(DestroyData dataPtr)
{
    Menubar *mbarPtr = (Menubar *)dataPtr;
    DestroyFloat(mbarPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * FloatEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various events on
 * 	the floating menubar.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the tearoff gets deleted, internal structures get
 *	cleaned up.  When it gets resized or exposed, it's redisplayed.
 *
 *---------------------------------------------------------------------------
 */
static void
FloatEventProc(ClientData clientData, XEvent *eventPtr)
{
    Menubar *mbarPtr = clientData;

    if ((mbarPtr == NULL) || (mbarPtr->tkwin == NULL) ||
	(mbarPtr->floatWin == NULL)) {
	return;
    }
    switch (eventPtr->type) {
    case Expose:
	if (eventPtr->xexpose.count == 0) {
	    EventuallyRedraw(mbarPtr);
	}
	break;

    case ConfigureNotify:
	EventuallyRedraw(mbarPtr);
	break;

    case DestroyNotify:
	DestroyFloat(mbarPtr);
	mbarPtr->floatWin = NULL;
	break;

    }
}

/*
 *---------------------------------------------------------------------------
 *
 * FloatCustodyProc --
 *
 *	This procedure is invoked when the menubar has been stolen by another
 *	geometry manager.  The information and memory associated with the 
 *	window is released.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the widget formerly associated with the menubar to
 *	have its layout re-computed and arranged at the next idle point.
 *
 *---------------------------------------------------------------------------
 */
 /* ARGSUSED */
static void
FloatCustodyProc(ClientData clientData, Tk_Window tkwin)
{
    Menubar *mbarPtr = clientData;

    if ((mbarPtr == NULL) || (mbarPtr->tkwin == NULL)) {
	return;
    }
    if (mbarPtr->floatWin != NULL) {
	Tcl_EventuallyFree(mbarPtr, FreeFloatProc);
    }
    /*
     * Mark the floating dock as deleted by dereferencing the Tk window
     * pointer.
     */
    if (mbarPtr->tkwin != NULL) {
	if (Tk_IsMapped(mbarPtr->tkwin)) {
	    mbarPtr->flags |= LAYOUT_PENDING;
	    EventuallyRedraw(mbarPtr);
	}
	Tk_DeleteEventHandler(mbarPtr->floatWin, StructureNotifyMask, 
		FloatEventProc, mbarPtr);
	mbarPtr->floatWin = NULL;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * FloatGeometryProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for the floating
 *	menubar managed by the widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for tkwin, and all its managed siblings, to be repacked and
 *	drawn at the next idle point.
 *
 *---------------------------------------------------------------------------
 */
 /* ARGSUSED */
static void
FloatGeometryProc(ClientData clientData, Tk_Window tkwin)
{
    Menubar *mbarPtr = clientData;

    if ((mbarPtr == NULL) || (mbarPtr->tkwin == NULL)) {
	fprintf(stderr, "%s: line %d \"tkwin is null\"", __FILE__, __LINE__);
	return;
    }
    mbarPtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(mbarPtr);
}

static void
FloatMenubar(ClientData clientData)
{
    Menubar *mbarPtr = clientData;

    Blt_RelinkWindow(mbarPtr->tkwin, mbarPtr->floatWin, 0, 0);
    Tk_MapWindow(mbarPtr->tkwin);
}

static int
CreateFloat(Menubar *mbarPtr, const char *name)
{
    Tk_Window tkwin;
    int width, height;

    tkwin = Tk_CreateWindowFromPath(mbarPtr->interp, mbarPtr->tkwin, name,
	(char *)NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    mbarPtr->floatWin = tkwin;
    if (Tk_WindowId(tkwin) == None) {
	Tk_MakeWindowExist(tkwin);
    }
    Tk_SetClass(tkwin, "MenubarDock");
    Tk_CreateEventHandler(tkwin, (ExposureMask | StructureNotifyMask),
	FloatEventProc, mbarPtr);
    if (Tk_WindowId(mbarPtr->tkwin) == None) {
	Tk_MakeWindowExist(mbarPtr->tkwin);
    }
    Tk_ManageGeometry(mbarPtr->tkwin, &menubarMgrInfo, mbarPtr);
    mbarPtr->region.height = height = Tk_Height(mbarPtr->tkwin);
    mbarPtr->region.width = width = Tk_Width(mbarPtr->tkwin);
    mbarPtr->region.x = Tk_X(mbarPtr->tkwin);
    mbarPtr->region.y = Tk_Y(mbarPtr->tkwin);
    if (width < 2) {
	width = (mbarPtr->reqWidth > 0)
	    ? mbarPtr->reqWidth : Tk_ReqWidth(mbarPtr->tkwin);
    }
    width += 2 * Tk_Changes(mbarPtr->tkwin)->border_width;
    width += 2 * mbarPtr->inset;

    if (height < 2) {
	height = (mbarPtr->reqHeight > 0)
	    ? mbarPtr->reqHeight : Tk_ReqHeight(mbarPtr->tkwin);
    }
    height += 2 * Tk_Changes(mbarPtr->tkwin)->border_width;
    height += 2 * mbarPtr->inset;
    Tk_GeometryRequest(tkwin, width, height);
    /* Tk_MoveWindow(mbarPtr->tkwin, 0, 0); */
    Tcl_SetStringObj(Tcl_GetObjResult(mbarPtr->interp), Tk_PathName(tkwin), -1);
    Tk_UnmapWindow(mbarPtr->tkwin);
#ifdef WIN32
    FloatMenubar(mbarPtr);
#else
    Tcl_DoWhenIdle(FloatMenubar, mbarPtr);
#endif
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawLabel --
 *
 * 	Draws the text associated with the label.  This is used when the
 * 	widget acts like a standard label.
 *
 * Results:
 *	Nothing.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawLabel(Item *itemPtr, Drawable drawable, int x, int y, int w, int h) 
{
    if (itemPtr->image != NULL) {
	int iw, ih;
		
	iw = MIN(w, itemPtr->textWidth) - IPAD;
	ih = MIN(h, itemPtr->textHeight);
	Tk_RedrawImage(IconImage(itemPtr->image), 0, 0, iw, ih, drawable, 
		x + IPAD, y);
    } else {
	GC gc;
	Menubar *mbarPtr;
	TextLayout *layoutPtr;
	TextStyle ts;

	mbarPtr = itemPtr->mbarPtr;
	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, mbarPtr->font);
	Blt_Ts_SetAnchor(ts, TK_ANCHOR_NW);
	Blt_Ts_SetJustify(ts, itemPtr->justify);
	Blt_Ts_SetUnderline(ts, itemPtr->underline);
	layoutPtr = Blt_Ts_CreateLayout(itemPtr->text, -1, &ts);
	if (itemPtr->flags & STATE_POSTED) {
	    gc = mbarPtr->textPostedGC;
	} else if (itemPtr->flags & STATE_ACTIVE) {
	    gc = mbarPtr->textActiveGC;
	} else if (itemPtr->flags & STATE_DISABLED) {
	    gc = mbarPtr->textDisabledGC;
	} else {
	    gc = mbarPtr->textNormalGC;
	}
	Blt_DrawLayout(mbarPtr->tkwin, drawable, gc, mbarPtr->font, 
		Tk_Depth(mbarPtr->tkwin), 0.0f, x + IPAD, y, layoutPtr, w);
	Blt_Free(layoutPtr);
    }
}

static void
DrawSeparator(Item *itemPtr, Drawable drawable, int x, int y, int w, int h)
{
    Menubar *mbarPtr = itemPtr->mbarPtr;
    XPoint points[2];
    Tk_3DBorder border;

    points[1].x = points[0].x = x + w / 2;
    points[0].y = y + YPAD;
    points[1].y = h - 2 * YPAD;

    border = Blt_BackgroundBorder(mbarPtr->normalBg);
    Tk_Draw3DPolygon(mbarPtr->tkwin, drawable, border, points, 2, 1, 
		     TK_RELIEF_SUNKEN);
}

static void
DrawItem(Item *itemPtr, Drawable drawable)
{
    Menubar *mbarPtr = itemPtr->mbarPtr;
    Blt_Background bg;
    int relief;
    int x, y, w, h;

    if (itemPtr->flags & ITEM_FILLER) {
	return;			/* No need to do anything. */
    }
    x = itemPtr->x;
    y = itemPtr->y;
    w = itemPtr->width;
    h = mbarPtr->height;

    if (itemPtr->flags & ITEM_SEPARATOR) {
	DrawSeparator(itemPtr, drawable, x, y, w, h);
	return;
    }

    /* Menubar background (just inside of focus highlight ring). */

    if (itemPtr->flags & STATE_POSTED) {
	bg = mbarPtr->postedBg;
    } else if (itemPtr->flags & STATE_ACTIVE) {
	bg = mbarPtr->activeBg;
    } else if (itemPtr->flags & STATE_DISABLED) {
	bg = mbarPtr->disabledBg;
    } else {
	bg = mbarPtr->normalBg;
    }
    Blt_FillBackgroundRectangle(mbarPtr->tkwin, drawable, bg, 0, 0,
	itemPtr->width, mbarPtr->height, mbarPtr->borderWidth, 
	TK_RELIEF_FLAT);

    /* Label: includes icon and text. */
    if (h > itemPtr->entryHeight) {
	y += (h - itemPtr->entryHeight) / 2;
    }
    x += XPAD;
    /* Draw Icon. */
    if (itemPtr->icon != NULL) {
	int ix, iy, iw, ih;
	
	ix = x;
	iy = y;
	if (itemPtr->iconHeight < itemPtr->entryHeight) {
	    iy += (itemPtr->entryHeight - itemPtr->iconHeight) / 2;
	}
	iw = MIN(w, itemPtr->iconWidth);
	ih = MIN(h, itemPtr->iconHeight);
	Tk_RedrawImage(IconImage(itemPtr->icon), 0, 0, iw, ih, 
		       drawable, ix, iy);
	x += itemPtr->iconWidth + IPAD;
	w -= itemPtr->iconWidth + IPAD;
    }
    if ((w > 0) && (h > 0)) {
	int tx, ty, tw, th;
	
	tx = x + IPAD;
	ty = y;
	if (itemPtr->entryHeight > itemPtr->textHeight) {
	    ty += (itemPtr->entryHeight - itemPtr->textHeight) / 2;
	}
	tw = MIN(w, itemPtr->textWidth);
	th = MIN(h, itemPtr->textHeight);
	DrawLabel(itemPtr, drawable, tx, ty, tw, th);
    }
    /* Draw focus highlight ring. */
    if (mbarPtr->highlightWidth > 0) {
	GC gc;

	if (itemPtr->flags & FOCUS) {
	    gc = mbarPtr->highlightGC;
	} else {
	    gc = mbarPtr->highlightBgGC;
	}
	if (gc == NULL) {
	    Blt_DrawFocusBackground(mbarPtr->tkwin, bg, 
		mbarPtr->highlightWidth, drawable);
	} else {
	    Tk_DrawFocusHighlight(mbarPtr->tkwin, gc, mbarPtr->highlightWidth,
		 drawable);
	}	    
    }
    if (itemPtr->flags & STATE_POSTED) {
	relief = mbarPtr->postedRelief;
    } else if (itemPtr->flags & STATE_ACTIVE) {
	relief = mbarPtr->activeRelief;
    } else {
	relief = mbarPtr->relief;
    }
    if (relief != TK_RELIEF_FLAT) {
	Blt_DrawBackgroundRectangle(mbarPtr->tkwin, drawable, bg, 
		mbarPtr->highlightWidth, mbarPtr->highlightWidth,
		itemPtr->width  - 2 * mbarPtr->highlightWidth,
		itemPtr->height - 2 * mbarPtr->highlightWidth,
		mbarPtr->borderWidth, relief);
    }
}

static void
DrawItems(Menubar *mbarPtr, Drawable drawable)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_FirstLink(mbarPtr->chain); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	Item *itemPtr;
	int x, y, w, h;

	itemPtr = Blt_Chain_GetValue(link);
	x = y = itemPtr->inset;
	w = itemPtr->width  - (2 * mbarPtr->inset);
	h = itemPtr->height - (2 * mbarPtr->inset);
	DrawItem(itemPtr, drawable);
	x += itemPtr->width + IPAD;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ActivateOp --
 *
 *	Activates the designated button.
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *	.mbar activate $item 
 *
 *---------------------------------------------------------------------------
 */
static int
ActivateOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	   Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;
    Item *itemPtr;

    if (GetItemFromObj(interp, mbarPtr, objv[2], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((itemPtr == NULL) || ((itemPtr->flags & ITEM_BUTTON) == 0)) {
	goto done;
    }
    if (itemPtr->flags & (STATE_POSTED|STATE_DISABLED)) {
	return TCL_OK;		/* Writing is currently disabled. */
    }
    itemPtr->flags |= STATE_ACTIVE;
 done:
    mbarPtr->activePtr->flags &= ~STATE_ACTIVE;
    mbarPtr->activePtr = itemPtr;
    EventuallyRedraw(mbarPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * AddOp --
 *
 *	Appends a new item to the menubar.
 *
 * Results:
 *	NULL is always returned.
 *
 *   .mbar add ?type? -text "fred" -tags ""
 *
 *---------------------------------------------------------------------------
 */
static int
AddOp(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;
    Item *itemPtr;
    int length, type;
    char c;
    const char *string;
    
    string = Tcl_GetStringFromObj(objv[2], &length);
    c = string[0];
    type = ITEM_BUTTON;
    if ((c == 'b') && (strncmp(string, "button", length) == 0)) {
	type = ITEM_BUTTON;
	objc--, objv--;
    } else if ((c == 's') && (strncmp(string, "separator", length) == 0)) {
	type = ITEM_SEPARATOR;
	objc--, objv--;
    } else if ((c == 's') && (strncmp(string, "filler", length) == 0)) {
	type = ITEM_FILLER;
	objc--, objv--;
    }
    itemPtr = NewItem(mbarPtr, type);
    if (ConfigureItem(interp, itemPtr, objc - 2, objv + 2, 0) != TCL_OK) {
	DestroyItem(itemPtr);
	return TCL_ERROR;	/* Error configuring the entry. */
    }
    EventuallyRedraw(mbarPtr);
    Blt_Chain_LinkAfter(mbarPtr->chain, itemPtr->link, NULL);
    Tcl_SetLongObj(Tcl_GetObjResult(interp), itemPtr->index);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * CgetOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *	.menubar cget option
 *
 *---------------------------------------------------------------------------
 */
static int
CgetOp(ClientData clientData, Tcl_Interp *interp, int objc, 
       Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;
    iconOption.clientData = mbarPtr;
    return Blt_ConfigureValueFromObj(interp, mbarPtr->tkwin, menubarSpecs,
	(char *)mbarPtr, objv[2], BLT_CONFIG_OBJV_ONLY);
}

/*
 *---------------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *	.menubar configure ?option value?...
 *
 *---------------------------------------------------------------------------
 */
static int
ConfigureOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	    Tcl_Obj *const *objv)
{
    int result;
    Menubar *mbarPtr = clientData;

    iconOption.clientData = mbarPtr;
    if (objc == 2) {
	return Blt_ConfigureInfoFromObj(interp, mbarPtr->tkwin, menubarSpecs, 
		(char *)mbarPtr, (Tcl_Obj *)NULL,  BLT_CONFIG_OBJV_ONLY);
    } else if (objc == 3) {
	return Blt_ConfigureInfoFromObj(interp, mbarPtr->tkwin, menubarSpecs, 
		(char *)mbarPtr, objv[2], BLT_CONFIG_OBJV_ONLY);
    }
    Tcl_Preserve(mbarPtr);
    result = ConfigureMenubar(interp, mbarPtr, objc - 2, objv + 2, 
	BLT_CONFIG_OBJV_ONLY);
    Tcl_Release(mbarPtr);
    if (result == TCL_ERROR) {
	return TCL_ERROR;
    }
    mbarPtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(mbarPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * DockOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *	.menubar dock
 *	.menubar undock
 *---------------------------------------------------------------------------
 */
static int
DockOp(ClientData clientData, Tcl_Interp *interp, int objc, 
       Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;

    if (mbarPtr->floatWin == NULL) {
	CreateFloat(mbarPtr, "dock");
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ItemCgetOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *	.mbar item cget $button option
 *
 *---------------------------------------------------------------------------
 */
static int
ItemCgetOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	     Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;
    Item *itemPtr;

    if (GetItemFromObj(interp, mbarPtr, objv[3], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == NULL) {
	return TCL_OK;
    }
    iconOption.clientData = mbarPtr;
    return Blt_ConfigureValueFromObj(interp, mbarPtr->tkwin, itemSpecs,
	(char *)itemPtr, objv[4], BLT_CONFIG_OBJV_ONLY);
}

/*
 *---------------------------------------------------------------------------
 *
 * ItemConfigureOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *	.mbar item configure $button ?option value?...
 *
 *---------------------------------------------------------------------------
 */
static int
ItemConfigureOp(ClientData clientData, Tcl_Interp *interp, int objc, 
		  Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;
    Item *itemPtr;
    int result;

    if (GetItemFromObj(interp, mbarPtr, objv[3], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == NULL) {
	return TCL_OK;
    }
    iconOption.clientData = mbarPtr;
    if (objc == 4) {
	return Blt_ConfigureInfoFromObj(interp, mbarPtr->tkwin, itemSpecs, 
		(char *)itemPtr, (Tcl_Obj *)NULL,  BLT_CONFIG_OBJV_ONLY);
    } else if (objc == 5) {
	return Blt_ConfigureInfoFromObj(interp, mbarPtr->tkwin, itemSpecs, 
		(char *)itemPtr, objv[4], BLT_CONFIG_OBJV_ONLY);
    }
    Tcl_Preserve(itemPtr);
    result = ConfigureItem(interp, itemPtr, objc - 4, objv + 4, 
			     BLT_CONFIG_OBJV_ONLY);
    Tcl_Release(itemPtr);
    if (result == TCL_ERROR) {
	return TCL_ERROR;
    }
    mbarPtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(mbarPtr);
    return TCL_OK;
}


static Blt_OpSpec itemOps[] =
{
    {"cget",      2, ItemCgetOp,      5, 5, "item option",},
    {"configure", 2, ItemConfigureOp, 4, 0, "item ?option value?...",},
};

static int nItemOps = sizeof(itemOps) / sizeof(Blt_OpSpec);

/*
 *---------------------------------------------------------------------------
 *
 * ItemOp --
 *
 *	This procedure handles item operations.
 *
 * Results:
 *	A standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
static int
ItemOp(
    ClientData clientData,	/* Information about the widget. */
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument vector. */
{
    Tcl_ObjCmdProc *proc;
    Menubar *mbarPtr = clientData;
    int result;

    proc = Blt_GetOpFromObj(interp, nItemOps, itemOps, BLT_OP_ARG1, 
	objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve(mbarPtr);
    result = (*proc) (clientData, interp, objc, objv);
    Tcl_Release(mbarPtr);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * DeleteOp --
 *
 *	Deletes one or more item from the menubar.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	The menubar is redrawn.
 *
 *   .mbar delete but1 but2 but3 but4...
 *
 *---------------------------------------------------------------------------
 */
static int
DeleteOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	 Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;
    int i;

    for (i = 2; i < objc; i++) {
	Item *itemPtr;

	if (GetItemFromObj(interp, mbarPtr, objv[i], &itemPtr) != TCL_OK) {
	    RenumberItems(mbarPtr);
	    EventuallyRedraw(mbarPtr);
	    return TCL_ERROR;
	}
	if (itemPtr != NULL) {
	    DestroyItem(itemPtr);
	}
    }
    RenumberItems(mbarPtr);
    EventuallyRedraw(mbarPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * IndexOp --
 *
 *	Returns the index of the designated item or -1 if it doesn't
 *	exist.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	The menubar is redrawn.
 *
 *   .mbar index $item
 *
 *---------------------------------------------------------------------------
 */
static int
IndexOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;
    Item *itemPtr;
    int index;

    index = -1;
    if (GetItemFromObj(interp, mbarPtr, objv[2], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr != NULL) {
	index = itemPtr->index;
    }
    Tcl_SetLongObj(Tcl_GetObjResult(interp), index);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * InsertOp --
 *
 *	Inserts a new item into the menubar.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	The menubar is redrawn.
 *
 *   .mbar insert ?type? -1 -text "fred" -tags ""
 *
 *---------------------------------------------------------------------------
 */
static int
InsertOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	 Tcl_Obj *const *objv)
{
    Blt_ChainLink before;
    Item *itemPtr;
    Menubar *mbarPtr = clientData;
    char c;
    const char *string;
    int length, type;
    long insertPos;

    string = Tcl_GetStringFromObj(objv[2], &length);
    c = string[0];
    type = ITEM_BUTTON;
    if ((c == 'b') && (strncmp(string, "button", length) == 0)) {
	type = ITEM_BUTTON;
	objc--, objv--;
    } else if ((c == 's') && (strncmp(string, "separator", length) == 0)) {
	type = ITEM_SEPARATOR;
	objc--, objv--;
    } else if ((c == 's') && (strncmp(string, "filler", length) == 0)) {
	type = ITEM_FILLER;
	objc--, objv--;
    }
    if (Blt_GetPositionFromObj(interp, objv[2], &insertPos) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((insertPos == -1) || 
	(insertPos >= Blt_Chain_GetLength(mbarPtr->chain))) {
	before = NULL;		/* Insert at end of list. */
    } else {
	before =  Blt_Chain_GetNthLink(mbarPtr->chain, insertPos);
    }
    itemPtr = NewItem(mbarPtr, type);
    if (ConfigureItem(interp, itemPtr, objc - 3, objv + 3, 0) != TCL_OK) {
	DestroyItem(itemPtr);
	return TCL_ERROR;	/* Error configuring the entry. */
    }
    EventuallyRedraw(mbarPtr);
    Blt_Chain_LinkBefore(mbarPtr->chain, itemPtr->link, before);
    RenumberItems(mbarPtr);
    Tcl_SetLongObj(Tcl_GetObjResult(interp), itemPtr->index);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * InvokeOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *  .mbar invoke $item 
 *
 *---------------------------------------------------------------------------
 */
static int
InvokeOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	 Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;
    Item *itemPtr;
    int result;

    if (GetItemFromObj(interp, mbarPtr, objv[2], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((itemPtr == NULL) || 
	((itemPtr->flags & (STATE_DISABLED|ITEM_BUTTON)) != ITEM_BUTTON)) {
	return TCL_OK;		/* Item is currently disabled. */
    }
    result = TCL_OK;
    if (itemPtr->cmdObjPtr != NULL) {
	Tcl_Preserve(itemPtr);
	Tcl_IncrRefCount(itemPtr->cmdObjPtr);
	result = Tcl_EvalObjEx(interp, itemPtr->cmdObjPtr, TCL_EVAL_GLOBAL);
	Tcl_DecrRefCount(itemPtr->cmdObjPtr);
	Tcl_Release(itemPtr);
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * MoveOp --
 *
 *	Moves a item to a new location.
 *
 *   .mbar move but1 after|before but2
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
MoveOp(ClientData clientData, Tcl_Interp *interp, int objc, 
       Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;
    Item *itemPtr, *b2Ptr;
    char c;
    const char *string;
    int isBefore;
    int length;

    if (GetItemFromObj(interp, mbarPtr, objv[2], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == NULL) {
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
    if (GetItemFromObj(interp, mbarPtr, objv[4], &b2Ptr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == NULL) {
	return TCL_OK;
    }
    Blt_Chain_UnlinkLink(mbarPtr->chain, itemPtr->link);
    if (isBefore) {
	Blt_Chain_LinkBefore(mbarPtr->chain, itemPtr->link, b2Ptr->link);
    } else {
	Blt_Chain_LinkAfter(mbarPtr->chain, itemPtr->link, b2Ptr->link);
    }
    RenumberItems(mbarPtr);
    EventuallyRedraw(mbarPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * NamesOp --
 *
 *	Lists the names of the items in the menubar.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	The menubar is redrawn.
 *
 *   .mbar names ?pattern?
 *
 *---------------------------------------------------------------------------
 */
static int
NamesOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;
    Tcl_Obj *listObjPtr;
    int i;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (i = 2; i < objc; i++) {
	const char *pattern;
	Blt_ChainLink link;

	pattern = Tcl_GetString(objv[i]);
	for (link = Blt_Chain_FirstLink(mbarPtr->chain); link != NULL; 
	     link = Blt_Chain_NextLink(link)) {
	    Item *itemPtr;
	    Tcl_Obj *objPtr;

	    itemPtr = Blt_Chain_GetValue(link);
	    if (Tcl_StringMatch(itemPtr->text, pattern)) {
		if (itemPtr->text == emptyString) {
		    objPtr = Blt_EmptyStringObj();
		} else {
		    objPtr = Tcl_NewStringObj(itemPtr->text, -1);
		}
		Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    }
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * PostOp --
 *
 *	Posts the menu associated with this item.
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *  .mbar post $item 
 *
 *---------------------------------------------------------------------------
 */
static int
PostOp(ClientData clientData, Tcl_Interp *interp, int objc, 
       Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;
    Item *itemPtr;

    if (GetItemFromObj(interp, mbarPtr, objv[2], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == NULL) {
	if (mbarPtr->postedPtr != NULL) {
	    UnpostMenu(mbarPtr->postedPtr);
	}
	return TCL_OK;
    }
    if ((itemPtr->flags & (STATE_POSTED|STATE_DISABLED|ITEM_BUTTON)) !=
	ITEM_BUTTON) {
	return TCL_OK;		/* Item's menu is currently posted or entry
				 * is disabled. */
    }
    if (itemPtr->menuObjPtr == NULL) {
	return TCL_OK;
    }
    if (mbarPtr->postedPtr != NULL) {
	UnpostMenu(mbarPtr->postedPtr);
    }
    if (itemPtr != NULL) {
	PostMenu(itemPtr);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * UndockOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *	.menubar dock
 *	.menubar undock
 *---------------------------------------------------------------------------
 */
static int
UndockOp(ClientData clientData, Tcl_Interp *interp, int objc, 
       Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;

    if (mbarPtr->floatWin != NULL) {
	DestroyFloat(mbarPtr);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * UnpostOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *  .mbar unpost $item
 *
 *---------------------------------------------------------------------------
 */
static int
UnpostOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	 Tcl_Obj *const *objv)
{
    Menubar *mbarPtr = clientData;
    Item *itemPtr;

    if (GetItemFromObj(interp, mbarPtr, objv[2], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == NULL) {
	return TCL_OK;
    }
    if ((itemPtr->flags & (STATE_POSTED|STATE_DISABLED|ITEM_BUTTON)) != 
	ITEM_BUTTON) {
	return TCL_OK;		/* Item's menu is currently posted or entry
				 * is disabled. */
    }
    if (itemPtr->menuObjPtr == NULL) {
	return TCL_OK;
    }
    UnpostMenu(itemPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * MenubarCmd --
 *
 * 	This procedure is invoked to process the "menubar" command.  See
 * 	the user documentation for details on what it does.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */

static Blt_OpSpec menubarOps[] =
{
    {"activate",  2, ActivateOp,  3, 3, "item",},
    {"add",       2, AddOp,       3, 3, "?option value...?",},
    {"cget",      2, CgetOp,      3, 3, "option",},
    {"configure", 2, ConfigureOp, 2, 0, "?option value?...",},
    {"delete",    2, DeleteOp,    2, 0, "item ?item?",},
    {"dock",      2, DockOp,      2, 2, "",},
    {"index",     3, IndexOp,     3, 3, "item",},
    {"insert",    3, InsertOp,    2, 2, "position ?option value...?",},
    {"invoke",    3, InvokeOp,    3, 3, "item",},
    {"item",      3, ItemOp,      2, 0, "args",},
    {"move",      1, MoveOp,      2, 2, "position item ?item?",},
    {"names",     1, NamesOp,     2, 0, "?pattern...?",},
    {"post",      1, PostOp,      3, 3, "item",},
    {"undock",    3, UndockOp,    2, 2, "",},
    {"unpost",    3, UnpostOp,    3, 3, "item",},
};

static int nMenubarOps = sizeof(menubarOps) / sizeof(Blt_OpSpec);

static int
MenubarInstCmdProc(
    ClientData clientData,	/* Information about the widget. */
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument vector. */
{
    Tcl_ObjCmdProc *proc;
    Menubar *mbarPtr = clientData;
    int result;

    proc = Blt_GetOpFromObj(interp, nMenubarOps, menubarOps, BLT_OP_ARG1, 
	objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve(mbarPtr);
    result = (*proc) (clientData, interp, objc, objv);
    Tcl_Release(mbarPtr);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * MenubarInstCmdDeletedProc --
 *
 *	This procedure can be called if the window was destroyed (tkwin will
 *	be NULL) and the command was deleted automatically.  In this case, we
 *	need to do nothing.
 *
 *	Otherwise this routine was called because the command was deleted.
 *	Then we need to clean-up and destroy the widget.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The widget is destroyed.
 *
 *---------------------------------------------------------------------------
 */
static void
MenubarInstCmdDeletedProc(ClientData clientData)
{
    Menubar *mbarPtr = clientData; /* Pointer to widget record. */

    if (mbarPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = mbarPtr->tkwin;
	mbarPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * MenubarCmd --
 *
 * 	This procedure is invoked to process the TCL command that corresponds
 * 	to a widget managed by this module. See the user documentation for
 * 	details on what it does.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side Effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
MenubarCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument strings. */
{
    Menubar *mbarPtr;
    Tcl_CmdInfo cmdInfo;
    Tk_Window tkwin;
    char *path;

    if (objc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", 
		Tcl_GetString(objv[0]), " pathName ?option value?...\"", 
		(char *)NULL);
	return TCL_ERROR;
    }
    /*
     * First time in this interpreter, set up procs and initialize various
     * bindings for the widget.  If the proc doesn't already exist, source it
     * from "$blt_library/menubar.tcl".  We've deferred sourcing this file
     * until now so that the user could reset the variable $blt_library from
     * within her script.
     */
    if (!Tcl_GetCommandInfo(interp, "::blt::Menubar::PostMenu", &cmdInfo)) {
	static char cmd[] = "source [file join $blt_library menubar.tcl]";

	if (Tcl_GlobalEval(interp, cmd) != TCL_OK) {
	    char info[200];
	    sprintf_s(info, 200, "\n    (while loading bindings for %.50s)", 
		    Tcl_GetString(objv[0]));
	    Tcl_AddErrorInfo(interp, info);
	    return TCL_ERROR;
	}
    }
    path = Tcl_GetString(objv[1]);
    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp), path, 
	(char *)NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    mbarPtr = NewMenubar(interp, tkwin);
#define EVENT_MASK	(ExposureMask|StructureNotifyMask|FocusChangeMask)
    Tk_CreateEventHandler(tkwin, EVENT_MASK, MenubarEventProc, mbarPtr);
    Tk_SetClass(tkwin, "Menubar");
    mbarPtr->cmdToken = Tcl_CreateObjCommand(interp, path, 
	MenubarInstCmdProc, mbarPtr, MenubarInstCmdDeletedProc);
    Blt_SetWindowInstanceData(tkwin, mbarPtr);
    if (ConfigureMenubar(interp, mbarPtr, objc-2, objv+2, 0) != TCL_OK) {
	Tk_DestroyWindow(mbarPtr->tkwin);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, objv[1]);
    return TCL_OK;
}

int
Blt_MenubarInitProc(Tcl_Interp *interp)
{
    static Blt_InitCmdSpec cmdSpec = { "menubar", MenubarCmd, };

    return Blt_InitCmd(interp, "::blt", &cmdSpec);
}

#ifdef notdef
/*
 *---------------------------------------------------------------------------
 *
 * DrawLabel --
 *
 * 	Draws the text associated with the label.  This is used when the
 * 	widget acts like a standard label.
 *
 * Results:
 *	Nothing.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawLabel(Menubar *mbarPtr, Drawable drawable, int x, int y, int w, int h) 
{
    if (mbarPtr->image != NULL) {
	int imgWidth, imgHeight;
		
	imgWidth = MIN(w, itemPtr->textWidth) - IPAD;
	imgHeight = MIN(h, itemPtr->textHeight);
	Tk_RedrawImage(IconImage(mbarPtr->image), 0, 0, imgWidth, imgHeight, 
		drawable, x + IPAD, y);
    } else {
	TextStyle ts;
	TextLayout *layoutPtr;
	GC gc;

	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, mbarPtr->font);
	Blt_Ts_SetAnchor(ts, TK_ANCHOR_NW);
	Blt_Ts_SetJustify(ts, mbarPtr->justify);
	Blt_Ts_SetUnderline(ts, mbarPtr->underline);
	layoutPtr = Blt_Ts_CreateLayout(mbarPtr->text, mbarPtr->textLen, &ts);
	if (mbarPtr->flags & STATE_POSTED) {
	    gc = mbarPtr->textPostedGC;
	} else if (mbarPtr->flags & STATE_ACTIVE) {
	    gc = mbarPtr->textActiveGC;
	} else if (mbarPtr->flags & STATE_DISABLED) {
	    gc = mbarPtr->textDisabledGC;
	} else {
	    gc = mbarPtr->textNormalGC;
	}
	Blt_DrawLayout(mbarPtr->tkwin, drawable, gc, mbarPtr->font, 
		Tk_Depth(mbarPtr->tkwin), 0.0f, x + IPAD, y, layoutPtr, w);
	Blt_Free(layoutPtr);
    }
}
#endif

static void
DrawMenubar(Menubar *mbarPtr, Drawable drawable)
{
    Blt_Background bg;
    int x, y;
    int w, h;

    /* Menubar background (just inside of focus highlight ring). */

    if (mbarPtr->flags & STATE_POSTED) {
	bg = mbarPtr->postedBg;
    } else if (mbarPtr->flags & STATE_ACTIVE) {
	bg = mbarPtr->activeBg;
    } else if (mbarPtr->flags & STATE_DISABLED) {
	bg = mbarPtr->disabledBg;
    } else {
	bg = mbarPtr->normalBg;
    }
    Blt_FillBackgroundRectangle(mbarPtr->tkwin, drawable, bg, 0, 0,
	Tk_Width(mbarPtr->tkwin), Tk_Height(mbarPtr->tkwin),
	mbarPtr->borderWidth, TK_RELIEF_FLAT);

    x = y = mbarPtr->inset;
    w  = Tk_Width(mbarPtr->tkwin)  - (2 * mbarPtr->inset);
    h = Tk_Height(mbarPtr->tkwin) - (2 * mbarPtr->inset);

    DrawItems(mbarPtr, drawable);
}

/*
 *---------------------------------------------------------------------------
 *
 * DisplayMenubar --
 *
 *	This procedure is invoked to display a menubar widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the menubar.
 *
 *---------------------------------------------------------------------------
 */
static void
DisplayMenubar(ClientData clientData)
{
    Menubar *mbarPtr = clientData;
    Pixmap drawable;
    int ww, wh;			/* Window width and height. */

    mbarPtr->flags &= ~REDRAW_PENDING;
    if (mbarPtr->tkwin == NULL) {
	return;			/* Window destroyed (should not get here) */
    }
#ifdef notdef
    fprintf(stderr, "Calling DisplayMenubar(%s)\n", 
	    Tk_PathName(mbarPtr->tkwin));
#endif
    ww = Tk_Width(mbarPtr->tkwin);
    wh = Tk_Height(mbarPtr->tkwin);
    if ((ww <= 1) || (wh <=1)) {
	/* Don't bother computing the layout until the window size is
	 * something reasonable. */
	return;
    }
    if (mbarPtr->flags & LAYOUT_PENDING) {
	ComputeMenubarGeometry(mbarPtr);
    }
    if (!Tk_IsMapped(mbarPtr->tkwin)) {
	/* The widget's window isn't displayed, so don't bother drawing
	 * anything.  By getting this far, we've at least computed the
	 * coordinates of the menubar's new layout.  */
	return;
    }

    /*
     * Create a pixmap the size of the window for double buffering.
     */
    drawable = Tk_GetPixmap(mbarPtr->display, Tk_WindowId(mbarPtr->tkwin),
		ww, wh, Tk_Depth(mbarPtr->tkwin));
#ifdef WIN32
    assert(drawable != None);
#endif
    DrawMenubar(mbarPtr, drawable);
    XCopyArea(mbarPtr->display, drawable, Tk_WindowId(mbarPtr->tkwin),
	mbarPtr->textNormalGC, 0, 0, ww, wh, 0, 0);
    Tk_FreePixmap(mbarPtr->display, drawable);
}
