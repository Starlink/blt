
/*
 * bltComboMenu.c --
 *
 * This module implements a combomenu widget for the BLT toolkit.
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
#include "bltBind.h"
#include "bltImage.h"
#include "bltPicture.h"
#include "bltFont.h"
#include "bltText.h"
#include "bltChain.h"
#include "bltHash.h"
#include "bltBgStyle.h"
#include "bltPainter.h"

static const char emptyString[] = "";

#define CM_ALLOC_MAX_DOUBLE_SIZE	(1<<16)
#define CM_ALLOC_MAX_CHUNK		(1<<16)

#define CM_REDRAW		(1<<0)
#define CM_LAYOUT		(1<<1)
#define CM_FOCUS		(1<<2)
#define CM_XSCROLL		(1<<3)
#define CM_YSCROLL		(1<<4)
#define CM_SCROLL		(CM_XSCROLL|CM_YSCROLL)
#define CM_SELECT		(1<<5)
#define CM_MAPPED		(1<<6)

#define CM_INSTALL_SCROLLBAR_X	(1<<8)
#define CM_INSTALL_SCROLLBAR_Y	(1<<9)

#define VAR_FLAGS (TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS)

#define PIXMAPX(cm, wx)	((wx) - (cm)->xOffset)
#define PIXMAPY(cm, wy)	((wy) - (cm)->yOffset)

#define SCREENX(cm, wx)	((wx) - (cm)->xOffset + (cm)->borderWidth)
#define SCREENY(cm, wy)	((wy) - (cm)->yOffset + (cm)->borderWidth)

#define WORLDX(cm, sx)	((sx) - (cm)->borderWidth + (cm)->xOffset)
#define WORLDY(cm, sy)	((sy) - (cm)->borderWidth + (cm)->yOffset)

#define VPORTWIDTH(cm)	\
    (Tk_Width((cm)->tkwin) - 2 * (cm)->borderWidth - (cm)->yScrollbarWidth)
#define VPORTHEIGHT(cm) \
    (Tk_Height((cm)->tkwin) - 2 * (cm)->borderWidth - (cm)->xScrollbarHeight)

#define FCLAMP(x)	((((x) < 0.0) ? 0.0 : ((x) > 1.0) ? 1.0 : (x)))

#define ITEM_IPAD	   3
#define ITEM_XPAD	   2
#define ITEM_YPAD	   1
#define ITEM_SEP_HEIGHT	   6
#define ITEM_L_IND_WIDTH  19
#define ITEM_L_IND_HEIGHT 19
#define ITEM_R_IND_WIDTH  17
#define ITEM_R_IND_HEIGHT 17

#define ITEM_MAP	  (1<<1)  /* Item needs to be remapped  */
#define ITEM_REDRAW	  (1<<2)  /* Item needs to be redrawn. */
#define ITEM_SELECTED	  (1<<4)  /* Radiobutton/checkbutton is selected. */

/* Item state. */
#define ITEM_NORMAL	  (1<<5)  /* Draw item normally. */
#define ITEM_DISABLED	  (1<<6)  /* Item is disabled. */
#define ITEM_ACTIVE	  (1<<7)  /* Item is currently active. */
#define ITEM_STATE_MASK   ((ITEM_DISABLED)|(ITEM_ACTIVE)|(ITEM_NORMAL))

/* Item type. */
#define ITEM_BUTTON	  (1<<9)  /* Item is command button. */
#define ITEM_RADIOBUTTON  (1<<10) /* Item is radiobutton. */
#define ITEM_CHECKBUTTON  (1<<11) /* Item is checkbutton. */
#define ITEM_CASCADE	  (1<<12) /* Item is cascade. */
#define ITEM_SEPARATOR	  (1<<13) /* Item is separator. */
#define ITEM_TYPE_MASK    ((ITEM_BUTTON)|(ITEM_RADIOBUTTON)|(ITEM_CHECKBUTTON)|\
			   (ITEM_CASCADE)|(ITEM_SEPARATOR))

#define DEF_COMBO_BORDERWIDTH	    "1"
#define DEF_COMBO_CURSOR            ((char *)NULL)
#define DEF_COMBO_HEIGHT	    "0"
#define DEF_COMBO_ICON_VARIABLE	    ((char *)NULL)
#define DEF_COMBO_POST_CMD	    ((char *)NULL)
#define DEF_COMBO_RELIEF	    "solid"
#define DEF_COMBO_SCROLLBAR	    ((char *)NULL)
#define DEF_COMBO_SCROLL_CMD	    ((char *)NULL)
#define DEF_COMBO_SCROLL_INCR	    "2"
#define DEF_COMBO_TAKE_FOCUS        "1"
#define DEF_COMBO_TEXT_VARIABLE	    ((char *)NULL)
#define DEF_COMBO_WIDTH             "0"
#define DEF_ITEM_ACCELERATOR	    ((char *)NULL)
#define DEF_ITEM_BITMAP             ((char *)NULL)
#define DEF_ITEM_COMMAND	    ((char *)NULL)
#define DEF_ITEM_DATA		    ((char *)NULL)
#define DEF_ITEM_ICON               ((char *)NULL)
#define DEF_ITEM_IMAGE              ((char *)NULL)
#define DEF_ITEM_INDENT		    "0"
#define DEF_ITEM_MENU               ((char *)NULL)
#define DEF_ITEM_OFF_VALUE          "0"
#define DEF_ITEM_ON_VALUE           "1"
#define DEF_ITEM_STATE		    "normal"
#define DEF_ITEM_STYLE		    "default"
#define DEF_ITEM_TAGS		    ((char *)NULL)
#define DEF_ITEM_TEXT               ((char *)NULL)
#define DEF_ITEM_TIP		    ((char *)NULL)
#define DEF_ITEM_TYPE		    "command"
#define DEF_ITEM_UNDERLINE	    "-1"
#define DEF_ITEM_VALUE              ((char *)NULL)
#define DEF_ITEM_VARIABLE	    ((char *)NULL)
#define DEF_STYLE_ACCEL_ACTIVE_FG   RGB_WHITE
#define DEF_STYLE_ACCEL_FG	    RGB_BLACK
#define DEF_STYLE_ACCEL_FONT	    STD_FONT_SMALL
#define DEF_STYLE_ACTIVE_BG	    RGB_SKYBLUE4
#define DEF_STYLE_ACTIVE_FG	    RGB_WHITE
#define DEF_STYLE_ACTIVE_RELIEF	    "flat"
#define DEF_STYLE_BG		    RGB_WHITE
#define DEF_STYLE_BORDERWIDTH	    "0"
#define DEF_STYLE_DISABLED_ACCEL_FG STD_DISABLED_FOREGROUND
#define DEF_STYLE_DISABLED_BG       DISABLED_BACKGROUND
#define DEF_STYLE_DISABLED_FG       DISABLED_FOREGROUND
#define DEF_STYLE_FG                RGB_BLACK
#define DEF_STYLE_FONT		    STD_FONT_SMALL
#define DEF_STYLE_IND_BG	    RGB_WHITE
#define DEF_STYLE_IND_FG	    RGB_BLACK
#define DEF_STYLE_IND_SIZE	    "12"
#define DEF_STYLE_RELIEF	    "flat"
#define DEF_STYLE_SEL_BG	    RGB_GREY82
#define DEF_STYLE_SEL_FG	    RGB_BLACK
#define DISABLED_BACKGROUND	    RGB_GREY90
#define DISABLED_FOREGROUND         RGB_GREY82


static Blt_OptionFreeProc FreeStyleProc;
static Blt_OptionParseProc ObjToStyleProc;
static Blt_OptionPrintProc StyleToObjProc;
static Blt_CustomOption styleOption = {
    ObjToStyleProc, StyleToObjProc, FreeStyleProc, (ClientData)0
};

static Blt_OptionFreeProc FreeTagsProc;
static Blt_OptionParseProc ObjToTagsProc;
static Blt_OptionPrintProc TagsToObjProc;
static Blt_CustomOption tagsOption = {
    ObjToTagsProc, TagsToObjProc, FreeTagsProc, (ClientData)0
};

static Blt_OptionParseProc ObjToTypeProc;
static Blt_OptionPrintProc TypeToObjProc;
static Blt_CustomOption typeOption = {
    ObjToTypeProc, TypeToObjProc, NULL, (ClientData)0
};

static Blt_OptionFreeProc FreeLabelProc;
static Blt_OptionParseProc ObjToLabelProc;
static Blt_OptionPrintProc LabelToObjProc;
static Blt_CustomOption labelOption = {
    ObjToLabelProc, LabelToObjProc, FreeLabelProc, (ClientData)0
};

static Blt_OptionFreeProc FreeIconProc;
static Blt_OptionParseProc ObjToIconProc;
static Blt_OptionPrintProc IconToObjProc;
static Blt_CustomOption iconOption = {
    ObjToIconProc, IconToObjProc, FreeIconProc, (ClientData)0
};

static Blt_OptionFreeProc FreeTraceVarProc;
static Blt_OptionParseProc ObjToTraceVarProc;
static Blt_OptionPrintProc TraceVarToObjProc;
static Blt_CustomOption traceVarOption = {
    ObjToTraceVarProc, TraceVarToObjProc, FreeTraceVarProc, (ClientData)0
};

static Blt_OptionParseProc ObjToStateProc;
static Blt_OptionPrintProc StateToObjProc;
static Blt_CustomOption stateOption = {
    ObjToStateProc, StateToObjProc, NULL, (ClientData)0
};

typedef struct _ComboMenu ComboMenu;

/*
 * Icon --
 *
 *	Since instances of the same Tk image can be displayed in different
 *	windows with possibly different color palettes, Tk internally stores
 *	each instance in a linked list.  But if the instances are used in the
 *	same widget and therefore use the same color palette, this adds a lot
 *	of overhead, especially when deleting instances from the linked list.
 *
 *	For the combomenu widget, we never need more than a single instance of
 *	an image, regardless of how many times it's used.  Cache the image,
 *	maintaining a reference count for each image used in the widget.  It's
 *	likely that the comboview widget will use many instances of the same
 *	image.
 */

typedef struct _Icon {
    Tk_Image tkImage;		/* The Tk image being cached. */

    Blt_HashEntry *hPtr;	/* Hash table pointer to the image. */

    int refCount;		/* Reference count for this image. */

    short int width, height;	/* Dimensions of the cached image. */
} *Icon;

#define IconHeight(i)	((i)->height)
#define IconWidth(i)	((i)->width)
#define IconImage(i)	((i)->tkImage)
#define IconName(i)	(Blt_NameOfImage(IconImage(i)))

typedef struct {
    const char *name;
    Blt_HashEntry *hPtr;
    ComboMenu *comboPtr;
    int refCount;		/* Indicates if the style is currently
				 * in use in the combomenu. */
    int borderWidth;
    int relief;
    int activeRelief;
    int reqIndWidth;

    Blt_Background normalBg;
    Blt_Background activeBg;
    Blt_Background disabledBg;

    XColor *normalFgColor;
    XColor *activeFgColor;
    XColor *disabledFgColor;

    Blt_Font accelFont;		/* Font of accelerator text. */
    XColor *accelNormalColor;	/* Color of accelerator text. */
    XColor *accelDisabledColor;	/* Color of accelerator text. */
    XColor *accelActiveColor;	/* Color of accelerator background. */

    Blt_Font labelFont;		/* Font of the label */
    XColor *labelNormalColor;	/* Color of label text. */
    XColor *labelDisabledColor;	/* Color of label background. */
    XColor *labelActiveColor;	/* Color of label background. */

    /* Radiobuttons, checkbuttons, and cascades. */
    Blt_Picture radiobutton[3];
    Blt_Picture checkbutton[3];

    XColor *indBgColor;
    XColor *indFgColor;
    GC indBgGC;
    GC indFgGC;
    XColor *selBgColor;
    XColor *selFgColor;
    
    GC accelActiveGC;
    GC accelDisabledGC;
    GC accelNormalGC;
    GC labelActiveGC;
    GC labelDisabledGC;
    GC labelNormalGC;
} Style;

static Blt_ConfigSpec styleConfigSpecs[] =
{
    {BLT_CONFIG_FONT, "-acceleratorfont", (char *)NULL, (char *)NULL, 
	DEF_STYLE_ACCEL_FONT, Blt_Offset(Style, accelFont), 0},
    {BLT_CONFIG_COLOR, "-acceleratorforeground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_ACCEL_FG, Blt_Offset(Style, accelNormalColor), 0},
    {BLT_CONFIG_COLOR, "-activeacceleratorforeground", (char *)NULL, 
	(char *)NULL, DEF_STYLE_ACCEL_ACTIVE_FG, 
	Blt_Offset(Style, accelActiveColor), 0},
    {BLT_CONFIG_BACKGROUND, "-activebackground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_ACTIVE_BG, Blt_Offset(Style, activeBg), 0},
    {BLT_CONFIG_COLOR, "-activeforeground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_ACTIVE_FG, Blt_Offset(Style, labelActiveColor), 0},
    {BLT_CONFIG_RELIEF, "-activerelief", (char *)NULL, (char *)NULL, 
	DEF_STYLE_ACTIVE_RELIEF, Blt_Offset(Style, activeRelief), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BACKGROUND, "-background", (char *)NULL, (char *)NULL,
	DEF_STYLE_BG, Blt_Offset(Style, normalBg), 0},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0,0},
    {BLT_CONFIG_SYNONYM, "-bg", (char *)NULL, (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", (char *)NULL, (char *)NULL,
	DEF_STYLE_BORDERWIDTH, Blt_Offset(Style, borderWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_COLOR, "-disabledacceleratorforeground", (char *)NULL, 
	(char *)NULL, DEF_STYLE_DISABLED_ACCEL_FG, 
	Blt_Offset(Style, accelDisabledColor), 0},
    {BLT_CONFIG_BACKGROUND, "-disabledbackground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_DISABLED_BG, Blt_Offset(Style, disabledBg), 0},
    {BLT_CONFIG_COLOR, "-disabledforeground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_DISABLED_FG, Blt_Offset(Style, labelDisabledColor), 0},
    {BLT_CONFIG_SYNONYM, "-fg", (char *)NULL, (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-foreground", (char *)NULL, (char *)NULL, DEF_STYLE_FG, 
	Blt_Offset(Style, labelNormalColor), 0},
    {BLT_CONFIG_FONT, "-font", (char *)NULL, (char *)NULL, DEF_STYLE_FONT, 
        Blt_Offset(Style, labelFont), 0},
    {BLT_CONFIG_COLOR, "-indicatorbackground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_IND_BG, Blt_Offset(Style, indBgColor), 0},
    {BLT_CONFIG_COLOR, "-indicatorforeground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_IND_FG, Blt_Offset(Style, indFgColor), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-indicatorsize", (char *)NULL, (char *)NULL, 
	DEF_STYLE_IND_SIZE, Blt_Offset(Style, reqIndWidth), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_RELIEF, "-relief", (char *)NULL, (char *)NULL, 
	DEF_STYLE_RELIEF, Blt_Offset(Style, relief), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_COLOR, "-selectbackground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_SEL_BG, Blt_Offset(Style, selBgColor), 0},
    {BLT_CONFIG_COLOR, "-selectforeground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_SEL_FG, Blt_Offset(Style, selFgColor), 0},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 
	0, 0}
};

/*
 *
 *  [left indicator] [icon] [label] [right indicator/accel] [scrollbar]
 *   
 * left indicator:	checkbutton or radiobutton entries.
 * icon:		all entries.
 * label:		all entries.
 * accel:		checkbutton, radiobutton, or button entries.
 * right indicator:     cascade item only.
 */
typedef struct  {
    ComboMenu *comboPtr;	/* Combomenu containing this item. */
    long index;			/* Index of the item numbered from 1. */

    int xWorld, yWorld;		/* Upper left world-coordinate of item
				 * in menu. */
    
    Style *stylePtr;		/* Style used by this item. */

    unsigned int flags;		/* Contains various bits of
				 * information about the item, such as
				 * type, state. */
    Blt_ChainLink link;
    int relief;

    int underline;		/* Underlined character. */

    int indent;			/* # of pixels to indent the icon. */

    Icon image;			/* If non-NULL, image to be displayed instead
				 * of text label. */

    Icon icon;			/* Button, RadioButton, and CheckButton
				 * entries. */

    const char *label;		/* Label to be displayed. */

    const char *accel;		/* Accelerator text. May be NULL.*/


    Tcl_Obj *cmdObjPtr;		/* Command to be invoked when item is
				 * clicked. */

    Tcl_Obj *dataObjPtr;	/* User-data associated with this item. */

    Tcl_Obj *varNameObjPtr;	/* Variable associated with the item value. */

    Tcl_Obj *valueObjPtr;	/* Radiobutton value. */

    /* Checkbutton on and off values. */
    Tcl_Obj *onValueObjPtr;	/* Checkbutton on-value. */
    Tcl_Obj *offValueObjPtr;	/* Checkbutton off-value. */

    /* Cascade menu. */
    Tcl_Obj *menuObjPtr;	/* Name of the sub-menu. */

    Tcl_Obj *tagsObjPtr;

    Tcl_Obj *tipObjPtr;

    short int labelWidth, labelHeight;
    short int iconWidth, iconHeight;
    short int leftIndWidth, leftIndHeight;
    short int rightIndWidth, rightIndHeight;
    short int width, height;

} Item;

static Blt_ConfigSpec itemConfigSpecs[] =
{
    {BLT_CONFIG_STRING, "-accelerator", (char *)NULL, (char *)NULL, 
	DEF_ITEM_ACCELERATOR, Blt_Offset(Item, accel), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_OBJ, "-command", (char *)NULL, (char *)NULL, DEF_ITEM_COMMAND, 
	Blt_Offset(Item, cmdObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_OBJ, "-data", (char *)NULL, (char *)NULL, DEF_ITEM_DATA, 
	Blt_Offset(Item, dataObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-icon", (char *)NULL, (char *)NULL, DEF_ITEM_ICON, 
	Blt_Offset(Item, icon), BLT_CONFIG_NULL_OK, &iconOption},
    {BLT_CONFIG_CUSTOM, "-image", (char *)NULL, (char *)NULL, DEF_ITEM_IMAGE, 
	Blt_Offset(Item, image), BLT_CONFIG_NULL_OK, &iconOption},
    {BLT_CONFIG_PIXELS_NNEG, "-indent", (char *)NULL, (char *)NULL, 
	DEF_ITEM_INDENT, Blt_Offset(Item, indent), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-menu", (char *)NULL, (char *)NULL, DEF_ITEM_MENU, 
	Blt_Offset(Item, menuObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_OBJ, "-offvalue", (char *)NULL, (char *)NULL, 
	DEF_ITEM_OFF_VALUE, Blt_Offset(Item, offValueObjPtr), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_OBJ, "-onvalue", (char *)NULL, (char *)NULL, DEF_ITEM_ON_VALUE, 
	Blt_Offset(Item, onValueObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-state", (char *)NULL, (char *)NULL, DEF_ITEM_STATE, 
	Blt_Offset(Item, flags), BLT_CONFIG_DONT_SET_DEFAULT, &stateOption},
    {BLT_CONFIG_CUSTOM, "-style", (char *)NULL, (char *)NULL, DEF_ITEM_STYLE, 
	Blt_Offset(Item, stylePtr), 0, &styleOption},
     {BLT_CONFIG_CUSTOM, "-tags", (char *)NULL, (char *)NULL,
        DEF_ITEM_TAGS, 0, BLT_CONFIG_NULL_OK, &tagsOption},
    {BLT_CONFIG_CUSTOM, "-text", (char *)NULL, (char *)NULL, DEF_ITEM_TEXT, 
        Blt_Offset(Item, label), BLT_CONFIG_NULL_OK, &labelOption},
    {BLT_CONFIG_OBJ, "-tooltip", (char *)NULL, (char *)NULL, DEF_ITEM_TIP, 
         Blt_Offset(Item, tipObjPtr), BLT_CONFIG_NULL_OK},
     {BLT_CONFIG_CUSTOM, "-type", (char *)NULL, (char *)NULL, DEF_ITEM_TYPE, 
	Blt_Offset(Item, flags), BLT_CONFIG_DONT_SET_DEFAULT, &typeOption},
    {BLT_CONFIG_INT, "-underline", (char *)NULL, (char *)NULL, 
	DEF_ITEM_UNDERLINE, Blt_Offset(Item, underline), 
	BLT_CONFIG_DONT_SET_DEFAULT },
    {BLT_CONFIG_OBJ, "-value", (char *)NULL, (char *)NULL, DEF_ITEM_VALUE, 
         Blt_Offset(Item, valueObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-variable", (char *)NULL, (char *)NULL, 
	DEF_ITEM_VARIABLE, Blt_Offset(Item, varNameObjPtr), BLT_CONFIG_NULL_OK,
	&traceVarOption},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 
	0, 0}
};

struct _ComboMenu {

    /*
     * This works around a bug in the Tk API.  Under under Win32, Tk tries to
     * read the widget record of toplevel windows (TopLevel or Frame widget),
     * to get its menu name field.  What this means is that we must carefully
     * arrange the fields of this widget so that the menuName field is at the
     * same offset in the structure.
     */

    Tk_Window tkwin;		/* Window that embodies the frame.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up. */

    Display *display;		/* Display containing widget.  Used, among
				 * other things, so that resources can be
				 * freed even after tkwin has gone away. */

    Tcl_Interp *interp;		/* Interpreter associated with widget.  Used
				 * to delete widget command. */

    Tcl_Command cmdToken;	/* Token for widget's command. */

    Tcl_Obj *postCmdObjPtr;	/* If non-NULL, command to be executed when
				 * this menu is posted. */
    unsigned int flags;

    Tcl_Obj *iconVarObjPtr;	/* Name of TCL variable.  If non-NULL, this
				 * variable will be set to the name of the Tk
				 * image representing the icon of the selected
				 * item.  */

    Tcl_Obj *textVarObjPtr;	/* Name of TCL variable.  If non-NULL, this
				 * variable will be set to the text string of
				 * the label of the selected item. */

    Tcl_Obj *takeFocusObjPtr;	/* Value of -takefocus option; not used in the
				 * C code, but used by keyboard traversal
				 * scripts. */

    char *menuName;		/* Textual description of menu to use for
				 * menubar. Malloc-ed, may be NULL. */

    Tk_Cursor cursor;		/* Current cursor for window or None. */

    int reqWidth, reqHeight;     
    int relief;
    int borderWidth;

    Style defStyle;		/* Default style. */

    int xScrollUnits, yScrollUnits;

    /* Names of scrollbars to embed into the widget window. */
    Tcl_Obj *xScrollbarObjPtr, *yScrollbarObjPtr;

    /* Commands to control horizontal and vertical scrollbars. */
    Tcl_Obj *xScrollCmdObjPtr, *yScrollCmdObjPtr;

    Blt_HashTable tagTable;	/* Table of tags. */
    Blt_HashTable labelTable;	/* Table of labels. */
    Blt_HashTable iconTable;	/* Table of icons. */

    Blt_Chain chain;

    Item *activePtr;		/* If non-NULL, item that is currently active.
				 * If a cascade item, a submenu may be
				 * posted. */
    Item *postedPtr;

    Item *firstPtr, *lastPtr;	/* Defines the range of visible items. */

    int xOffset, yOffset;	/* Scroll offsets of viewport in world. */ 
    int worldWidth, worldHeight;  /* Dimension of entire menu. */

    Tk_Window xScrollbar;	/* Horizontal scrollbar to be used if
				 * necessary. If NULL, no x-scrollbar is
				 * used. */
    Tk_Window yScrollbar;	/* Vertical scrollbar to be used if
				 * necessary. If NULL, no y-scrollbar is
				 * used. */

    short int yScrollbarWidth, xScrollbarHeight;
    short int leftIndWidth, rightIndWidth;
    short int labelWidth, iconWidth;

    Blt_HashTable styleTable;	/* Table of styles used in this menu. */

    Icon rbIcon;	
    Icon cbIcon;	
    Icon casIcon;	

    /*
     * Scanning Information:
     */
    int scanAnchorX;		/* Horizontal scan anchor in screen
				 * x-coordinates. */
    int scanX;			/* x-offset of the start of the horizontal
				 * scan in world coordinates.*/
    int scanAnchorY;		/* Vertical scan anchor in screen
				 * y-coordinates. */
    int scanY;			/* y-offset of the start of the vertical scan
				 * in world coordinates.*/

    short int width, height;
    Blt_Painter painter;
};

static Blt_ConfigSpec comboConfigSpecs[] =
{
    {BLT_CONFIG_FONT, "-acceleratorfont", "acceleratorFont", "AcceleratorFont", 
	DEF_STYLE_ACCEL_FONT, Blt_Offset(ComboMenu, defStyle.accelFont), 0},
    {BLT_CONFIG_COLOR, "-acceleratorforeground", "acceleratorForeground", 
	"AcceleratorForeground", DEF_STYLE_ACCEL_FG, 
	Blt_Offset(ComboMenu, defStyle.accelNormalColor), 0},
    {BLT_CONFIG_COLOR, "-activeacceleratorforeground", 
	"activeAcceleratorForeground", "ActiveAcceleratorForeground", 
	DEF_STYLE_ACCEL_ACTIVE_FG, 
	Blt_Offset(ComboMenu, defStyle.accelActiveColor), 0},
    {BLT_CONFIG_BACKGROUND, "-activebackground", "activeBackground",
	"ActiveBackground", DEF_STYLE_ACTIVE_BG, 
	Blt_Offset(ComboMenu, defStyle.activeBg), 0},
    {BLT_CONFIG_COLOR, "-activeforeground", "activeForeground",
	"ActiveForeground", DEF_STYLE_ACTIVE_FG, 
	Blt_Offset(ComboMenu, defStyle.labelActiveColor), 0},
    {BLT_CONFIG_RELIEF, "-activerelief", "activeRelief", "ActiveRelief", 
	DEF_STYLE_ACTIVE_RELIEF, Blt_Offset(ComboMenu, defStyle.activeRelief), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	DEF_STYLE_BG, Blt_Offset(ComboMenu, defStyle.normalBg), 0},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0,0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_COMBO_BORDERWIDTH, Blt_Offset(ComboMenu, borderWidth), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor", DEF_COMBO_CURSOR, 
	Blt_Offset(ComboMenu, cursor), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-disabledacceleratorforeground", 
	"disabledAcceleratorForeground", "DisabledAcceleratorForeground", 
	DEF_STYLE_DISABLED_ACCEL_FG, 
	Blt_Offset(ComboMenu, defStyle.accelDisabledColor), 0},
    {BLT_CONFIG_BACKGROUND, "-disabledbackground", "disabledBackground",
	"DisabledBackground", DEF_STYLE_DISABLED_BG, 
        Blt_Offset(ComboMenu, defStyle.disabledBg), 0},
    {BLT_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_STYLE_DISABLED_FG, 
	Blt_Offset(ComboMenu, defStyle.labelDisabledColor), 0},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_STYLE_FG, Blt_Offset(ComboMenu, defStyle.labelNormalColor), 0},
    {BLT_CONFIG_FONT, "-font", "font", "Font", DEF_STYLE_FONT, 
	Blt_Offset(ComboMenu, defStyle.labelFont), 0},
    {BLT_CONFIG_PIXELS, "-height", "height", "Height", DEF_COMBO_HEIGHT, 
	Blt_Offset(ComboMenu, reqHeight), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-iconvariable", "iconVariable", "IconVariable", 
	DEF_COMBO_ICON_VARIABLE, Blt_Offset(ComboMenu, iconVarObjPtr), 
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_NNEG, "-itemborderwidth", "itemBorderWidth", 
	"ItemBorderWidth", DEF_STYLE_BORDERWIDTH, 
	Blt_Offset(ComboMenu, defStyle.borderWidth), 
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-postcommand", "postCommand", "PostCommand", 
	DEF_COMBO_POST_CMD, Blt_Offset(ComboMenu, postCmdObjPtr), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_RELIEF, "-relief", "relief", "Relief", DEF_COMBO_RELIEF, 
	Blt_Offset(ComboMenu, relief), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-textvariable", "textVariable", "TextVariable", 
	DEF_COMBO_TEXT_VARIABLE, Blt_Offset(ComboMenu, textVarObjPtr), 
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_OBJ, "-xscrollbar", "xScrollbar", "Scrollbar", 
	DEF_COMBO_SCROLLBAR, Blt_Offset(ComboMenu, xScrollbarObjPtr), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_OBJ, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
        DEF_COMBO_SCROLL_CMD, Blt_Offset(ComboMenu, xScrollCmdObjPtr), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_POS, "-xscrollincrement", "xScrollIncrement",
	"ScrollIncrement", DEF_COMBO_SCROLL_INCR, 
         Blt_Offset(ComboMenu, xScrollUnits), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-yscrollbar", "yScrollbar", "Scrollbar", 
	DEF_COMBO_SCROLLBAR, Blt_Offset(ComboMenu, yScrollbarObjPtr), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_OBJ, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
        DEF_COMBO_SCROLL_CMD, Blt_Offset(ComboMenu, yScrollCmdObjPtr), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_POS, "-yscrollincrement", "yScrollIncrement",
	"ScrollIncrement", DEF_COMBO_SCROLL_INCR, 
         Blt_Offset(ComboMenu, yScrollUnits),BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-takefocus", "takeFocus", "TakeFocus",
	DEF_COMBO_TAKE_FOCUS, Blt_Offset(ComboMenu, takeFocusObjPtr), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS, "-width", "width", "Width", DEF_COMBO_WIDTH, 
	Blt_Offset(ComboMenu, reqWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_COLOR, "-indicatorbackground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_IND_BG, Blt_Offset(ComboMenu, defStyle.indBgColor), 0},
    {BLT_CONFIG_COLOR, "-indicatorforeground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_IND_FG, Blt_Offset(ComboMenu, defStyle.indFgColor), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-indicatorsize", (char *)NULL, (char *)NULL, 
	DEF_STYLE_IND_SIZE, Blt_Offset(ComboMenu, defStyle.reqIndWidth), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-radioimage", (char *)NULL, (char *)NULL, 
	DEF_ITEM_IMAGE, Blt_Offset(ComboMenu, rbIcon), BLT_CONFIG_NULL_OK, 
        &iconOption},
    {BLT_CONFIG_CUSTOM, "-checkimage", (char *)NULL, (char *)NULL, 
	DEF_ITEM_IMAGE, Blt_Offset(ComboMenu, cbIcon), BLT_CONFIG_NULL_OK, 
        &iconOption},
    {BLT_CONFIG_CUSTOM, "-cascadeimage", (char *)NULL, (char *)NULL, 
	DEF_ITEM_IMAGE, Blt_Offset(ComboMenu, casIcon), BLT_CONFIG_NULL_OK,
        &iconOption},
    {BLT_CONFIG_COLOR, "-selectbackground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_SEL_BG, Blt_Offset(ComboMenu, defStyle.selBgColor), 0},
    {BLT_CONFIG_COLOR, "-selectforeground", (char *)NULL, (char *)NULL, 
	DEF_STYLE_SEL_FG, Blt_Offset(ComboMenu, defStyle.selFgColor), 0},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 
	0, 0}
};

/*
 * ItemIterator --
 *
 *	Items may be tagged with strings.  An item may have many tags.  The
 *	same tag may be used for many items.
 *	
 */

typedef enum { 
    ITER_INDEX, ITER_RANGE, ITER_ALL, ITER_TAG, ITER_TYPE
} IteratorType;

typedef struct _Iterator {
    ComboMenu *comboPtr;	/* ComboMenu that we're iterating over. */

    IteratorType type;		/* Type of iteration:
				 * ITER_TAG		By item tag.
				 * ITER_ALL		By every item.
				 * ITER_INDEX		Single item: either 
				 *			tag or index.
				 * ITER_RANGE		Over a consecutive 
				 *			range of indices.
				 * ITER_TYPE		Over a single item 
				 *			type.
				 */

    Item *startPtr, *last;	/* Starting and ending item.  Starting point
				 * of search, saved if iterator is reused.
				 * Used for ITER_ALL and ITER_INDEX
				 * searches. */
    Item *endPtr;		/* Ending item (inclusive). */

    Item *nextPtr;		/* Next item. */

    int itemType;
				/* For tag-based searches. */
    char *tagName;		/* If non-NULL, is the tag that we are
				 * currently iterating over. */

    Blt_HashTable *tablePtr;	/* Pointer to tag hash table. */

    Blt_HashSearch cursor;	/* Search iterator for tag hash table. */
} ItemIterator;

static Tk_GeomRequestProc ScrollbarGeometryProc;
static Tk_GeomLostSlaveProc ScrollbarCustodyProc;
static Tk_GeomMgr comboMgrInfo = {
    (char *)"combomenu",	/* Name of geometry manager used by winfo */
    ScrollbarGeometryProc,	/* Procedure to for new geometry requests */
    ScrollbarCustodyProc,	/* Procedure when scrollbar is taken away */
};

static Tcl_IdleProc DisplayItem;
static Tcl_IdleProc DisplayComboMenu;
static Tcl_FreeProc DestroyComboMenu;
static Tk_EventProc ScrollbarEventProc;
static Tk_EventProc ComboMenuEventProc;
static Tcl_ObjCmdProc ComboMenuInstCmdProc;
static Tcl_CmdDeleteProc ComboMenuInstCmdDeletedProc;
static Tcl_VarTraceProc ItemVarTraceProc;
static Tk_ImageChangedProc IconChangedProc;

static int GetItemIterator(Tcl_Interp *interp, ComboMenu *comboPtr,
	Tcl_Obj *objPtr, ItemIterator *iterPtr);
static int GetItemFromObj(Tcl_Interp *interp, ComboMenu *comboPtr,
	Tcl_Obj *objPtr, Item **itemPtrPtr);

typedef int (ComboMenuCmdProc)(ComboMenu *comboPtr, Tcl_Interp *interp, 
	int objc, Tcl_Obj *const *objv);

/*
 *---------------------------------------------------------------------------
 *
 * EventuallyRedraw --
 *
 *	Tells the Tk dispatcher to call the combomenu display routine at the
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
EventuallyRedraw(ComboMenu *comboPtr) 
{
    if ((comboPtr->tkwin != NULL) && !(comboPtr->flags & CM_REDRAW)) {
	Tcl_DoWhenIdle(DisplayComboMenu, comboPtr);
	comboPtr->flags |= CM_REDRAW;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * EventuallyRedrawItem --
 *
 *	Tells the Tk dispatcher to call the combomenu display routine at the
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
EventuallyRedrawItem(Item *itemPtr) 
{
    ComboMenu *comboPtr;

    comboPtr = itemPtr->comboPtr;
    if ((comboPtr->tkwin != NULL) && 
	((comboPtr->flags & CM_REDRAW) == 0) &&
	((itemPtr->flags & ITEM_REDRAW) == 0)) {
	Tcl_DoWhenIdle(DisplayItem, itemPtr);
	itemPtr->flags |= ITEM_REDRAW;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ReleaseTags --
 *
 *	Releases the tags used by this item.  
 *
 *---------------------------------------------------------------------------
 */
static void
ReleaseTags(ComboMenu *comboPtr, Item *itemPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;

    for (hPtr = Blt_FirstHashEntry(&comboPtr->tagTable, &iter); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&iter)) {
	Blt_HashTable *tagTablePtr;
	Blt_HashEntry *h2Ptr;

	tagTablePtr = Blt_GetHashValue(hPtr); 
	h2Ptr = Blt_FindHashEntry(tagTablePtr, (char *)itemPtr->index);
	if (h2Ptr != NULL) {
	    Blt_DeleteHashEntry(tagTablePtr, h2Ptr);
	}
    }
}

static void
DestroyItem(Item *itemPtr)
{
    ComboMenu *comboPtr = itemPtr->comboPtr;

    ReleaseTags(comboPtr, itemPtr);
    iconOption.clientData = comboPtr;
    Blt_FreeOptions(itemConfigSpecs, (char *)itemPtr, comboPtr->display, 0);
    if (itemPtr->link != NULL) {
	Blt_Chain_DeleteLink(comboPtr->chain, itemPtr->link);
    }
}

static void
DestroyItems(ComboMenu *comboPtr)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_FirstLink(comboPtr->chain); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	Item *itemPtr;

	itemPtr = Blt_Chain_GetValue(link);
	itemPtr->link = NULL;
	DestroyItem(itemPtr);
    }
    Blt_Chain_Destroy(comboPtr->chain);
}

static Item *
NewItem(ComboMenu *comboPtr)
{
    Item *itemPtr;
    Blt_ChainLink link;

    link = Blt_Chain_AllocLink(sizeof(Item));
    itemPtr = Blt_Chain_GetValue(link);
    itemPtr->comboPtr = comboPtr;
    itemPtr->flags |= (ITEM_BUTTON | ITEM_NORMAL);
    itemPtr->link = link;
    itemPtr->index = Blt_Chain_GetLength(comboPtr->chain) + 1;
    Blt_Chain_LinkAfter(comboPtr->chain, link, NULL);
    itemPtr->label = (char *)emptyString;
    itemPtr->underline = -1;
    return itemPtr;
}

static INLINE Item *
FindItemByIndex(ComboMenu *comboPtr, long index)
{
    Blt_ChainLink link;

    if ((index < 1) || (index > Blt_Chain_GetLength(comboPtr->chain))) {
	return NULL;
    }
    link = Blt_Chain_GetNthLink(comboPtr->chain, index - 1);
    return Blt_Chain_GetValue(link);
}

static INLINE Item *
FirstItem(ComboMenu *comboPtr)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_FirstLink(comboPtr->chain); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	Item *itemPtr;

	itemPtr = Blt_Chain_GetValue(link);
	if ((itemPtr->flags & ITEM_DISABLED) == 0) {
	    return itemPtr;
	}
    }
    return NULL;
}

static INLINE Item *
LastItem(ComboMenu *comboPtr)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_LastLink(comboPtr->chain); link != NULL;
	 link = Blt_Chain_PrevLink(link)) {
	Item *itemPtr;

	itemPtr = Blt_Chain_GetValue(link);
	if ((itemPtr->flags & ITEM_DISABLED) == 0) {
	    return itemPtr;
	}
    }
    return NULL;
}


static Item *
NextItem(Item *itemPtr)
{
    if (itemPtr != NULL) {
	Blt_ChainLink link;

	for (link = Blt_Chain_NextLink(itemPtr->link); link != NULL; 
	     link = Blt_Chain_NextLink(link)) {
	    itemPtr = Blt_Chain_GetValue(link);
	    if ((itemPtr->flags & (ITEM_SEPARATOR|ITEM_DISABLED)) == 0) {
		return itemPtr;
	    }
	}
    }
    return NULL;
}

static INLINE Item *
PrevItem(Item *itemPtr)
{
    if (itemPtr != NULL) {
	Blt_ChainLink link;
	
	for (link = Blt_Chain_PrevLink(itemPtr->link); link != NULL; 
	     link = Blt_Chain_PrevLink(link)) {
	    itemPtr = Blt_Chain_GetValue(link);
	    if ((itemPtr->flags & (ITEM_SEPARATOR|ITEM_DISABLED)) == 0) {
		return itemPtr;
	    }
	}
    }
    return NULL;
}

static INLINE Item *
BeginItem(ComboMenu *comboPtr)
{
    Blt_ChainLink link;

    link = Blt_Chain_FirstLink(comboPtr->chain); 
    if (link != NULL) {
	return Blt_Chain_GetValue(link);
    }
    return NULL;
}

static INLINE Item *
EndItem(ComboMenu *comboPtr)
{
    Blt_ChainLink link;

    link = Blt_Chain_LastLink(comboPtr->chain); 
    if (link != NULL) {
	return Blt_Chain_GetValue(link);
    }
    return NULL;
}

static Item *
StepItem(Item *itemPtr)
{
    if (itemPtr != NULL) {
	Blt_ChainLink link;

	link = Blt_Chain_NextLink(itemPtr->link); 
	if (link != NULL) {
	    return Blt_Chain_GetValue(link);
	}
    }
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * ActivateItem --
 *
 *	Marks the designated item as active.  The item is redrawn with its
 *	active colors.  The previously active item is deactivated.  If the new
 *	item is NULL, then this means that no new item is to be activated.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Menu items may be scheduled to be drawn.
 *
 *---------------------------------------------------------------------------
 */
static void
ActivateItem(ComboMenu *comboPtr, Item *itemPtr) 
{
    if ((comboPtr->activePtr == itemPtr) && (itemPtr != NULL)) {
	return;		/* Item is already active. */
    }
    if (comboPtr->activePtr != NULL) {
	comboPtr->activePtr->flags &= ~ITEM_STATE_MASK;
	comboPtr->activePtr->flags |= ITEM_NORMAL;
	EventuallyRedrawItem(comboPtr->activePtr);
    }
    comboPtr->activePtr = itemPtr;
    if (itemPtr != NULL) {
	itemPtr->flags &= ~ITEM_STATE_MASK;
	itemPtr->flags |= ITEM_ACTIVE;
	EventuallyRedrawItem(itemPtr);
    }
}

static void
ComputeMenuCoords(ComboMenu *comboPtr, int menuAnchor, int *xPtr, int *yPtr)
{
    int rootX, rootY, x, y, w, h;
    int screenWidth, screenHeight;
    Tk_Window parent;

    parent = Tk_Parent(comboPtr->tkwin);
    screenWidth = WidthOfScreen(Tk_Screen(comboPtr->tkwin));
    screenHeight = HeightOfScreen(Tk_Screen(comboPtr->tkwin));

    Tk_GetRootCoords(parent, &rootX, &rootY);
    if (rootX < 0) {
	rootX = 0;
    }
    if (rootY < 0) {
	rootY = 0;
    }

    x = rootX, y = rootY;
    w = Tk_Width(comboPtr->tkwin);
    if (w <= 1) {
	w = Tk_ReqWidth(comboPtr->tkwin);
    }
    h = Tk_Height(comboPtr->tkwin);
    if (h <= 1) {
	h = Tk_Height(comboPtr->tkwin);
    }

    switch(menuAnchor) {
    case TK_ANCHOR_W:
    case TK_ANCHOR_SW:
	y += Tk_Height(parent);
	if ((y + h) > screenHeight) {
	    /* If we go offscreen on the bottom, show as 'NW'. */
	    y = rootY - h;
	    if (y < 0) {
		y = 0;
	    }
	}
	fprintf(stderr, "W,SW x=%d, w=%d, mrw=%d\n", x, Tk_Width(parent), w);
	if ((x + w) > screenWidth) {
	    x = screenWidth - w;
	}
	break;
    case TK_ANCHOR_CENTER:
    case TK_ANCHOR_S:
	x += (Tk_Width(parent) - w) / 2;
	y += Tk_Height(parent);
	if ((y + h) > screenHeight) {
	    /* If we go offscreen on the bottom, show as 'N'. */
	    y = rootY - h;
	    if (y < 0) {
		y = 0;
	    }
	}
	fprintf(stderr, "C,S x=%d, w=%d, mrw=%d\n", x, Tk_Width(parent), w);
	if (x < 0) {
	    x = 0;
	} else if ((x + w) > screenWidth) {
	    x = screenWidth - w;
	}
	break;
    case TK_ANCHOR_E:
    case TK_ANCHOR_SE:
	x += Tk_Width(parent) - w;
	y += Tk_Height(parent);
	if ((y + h) > screenHeight) {
	    /* If we go offscreen on the bottom, show as 'NE'. */
	    y = rootY - h;
	    if (y < 0) {
		y = 0;
	    }
	}
	fprintf(stderr, "E,SE x=%d, y=%d w=%d, mrw=%d\n", x, y, 
		Tk_Width(parent), w);
	if (x < 0) {
	    x = 0;
	}
	break;
    case TK_ANCHOR_NW:
	y -= h;
	if (y < 0) {
	    /* If we go offscreen on the top, show as 'SW'. */
	    y = rootY + Tk_Height(parent);
	    if ((y + h) > screenHeight) {
		y = screenHeight - h;
	    }
	}
	fprintf(stderr, "NW x=%d, w=%d, mrw=%d\n", x, Tk_Width(parent), w);
	if ((x + w) > screenWidth) {
	    x = screenWidth - w;
	}
	break;
    case TK_ANCHOR_N:
	x += (Tk_Width(parent) - w) / 2;
	y -= h;
	if (y < 0) {
	    /* If we go offscreen on the top, show as 'S'. */
	    y = rootY + Tk_Height(parent);
	    if ((y + h) > screenHeight) {
		y = screenHeight - h;
	    }
	}
	fprintf(stderr, "N x=%d, w=%d, mrw=%d\n", x, Tk_Width(parent), w);
	if (x < 0) {
	    x = 0;
	} else if ((x + w) > screenWidth) {
	    x = screenWidth - w;
	}
	break;
    case TK_ANCHOR_NE:
	x += Tk_Width(parent) - w;
	y -= h;
	if (y < 0) {
	    /* If we go offscreen on the top, show as 'SE'. */
	    y = rootY + Tk_Height(parent);
	    if ((y + h) > screenHeight) {
		y = screenHeight - h;
	    }
	}
	fprintf(stderr, "NE x=%d, w=%d, mrw=%d\n", x, Tk_Width(parent), w);
	if (x < 0) {
	    x = 0;
	}
	break;
    }    
    *xPtr = x;
    *yPtr = y;
}

static void
ComputeItemGeometry(ComboMenu *comboPtr, Item *itemPtr)
{

    /* Determine the height of the item.  It's the maximum height of all it's
     * components: left gadget (radiobutton or checkbutton), icon, label,
     * right gadget (cascade), and accelerator. */
    itemPtr->labelWidth = itemPtr->labelHeight = 0;
    itemPtr->leftIndWidth = itemPtr->leftIndHeight = 0;
    itemPtr->rightIndWidth = itemPtr->rightIndHeight = 0;
    itemPtr->iconWidth = itemPtr->iconHeight = 0;
    itemPtr->width = itemPtr->height = 0;
    if (itemPtr->flags & ITEM_SEPARATOR) {
	itemPtr->height = ITEM_SEP_HEIGHT;
	itemPtr->width = 0;
    } else {
	if (itemPtr->flags & (ITEM_RADIOBUTTON | ITEM_CHECKBUTTON)) {
	    Blt_FontMetrics fm;

	    Blt_GetFontMetrics(itemPtr->stylePtr->labelFont, &fm);
	    itemPtr->leftIndWidth = itemPtr->leftIndHeight =
		(fm.linespace) + 4 * ITEM_YPAD;
#ifdef notdef
	    itemPtr->leftIndWidth = ITEM_L_IND_WIDTH;
	    itemPtr->leftIndHeight = ITEM_L_IND_HEIGHT;
#endif
	    if (itemPtr->stylePtr->reqIndWidth > 0) {
		itemPtr->leftIndWidth = itemPtr->leftIndHeight = 
		    itemPtr->stylePtr->reqIndWidth + 2 * ITEM_YPAD;
	    }
	    if (itemPtr->height < itemPtr->leftIndHeight) {
		itemPtr->height = itemPtr->leftIndHeight;
	    }
	    itemPtr->width += itemPtr->leftIndWidth + 2 * ITEM_IPAD;
	}
	if (itemPtr->icon != NULL) {
	    itemPtr->iconWidth = IconWidth(itemPtr->icon);
	    itemPtr->iconHeight = IconHeight(itemPtr->icon);
	    if (itemPtr->height < IconHeight(itemPtr->icon)) {
		itemPtr->height = IconHeight(itemPtr->icon);
	    }
	    itemPtr->width += itemPtr->iconWidth + ITEM_IPAD;
	}
	if (itemPtr->image != NULL) {
	    itemPtr->labelWidth = IconWidth(itemPtr->image);
	    itemPtr->labelHeight = IconHeight(itemPtr->image);
	    if (itemPtr->height < itemPtr->labelHeight) {
		itemPtr->height = itemPtr->labelHeight;
	    }
	} else if (itemPtr->label != emptyString) {
	    unsigned int w, h;
	    
	    Blt_GetTextExtents(itemPtr->stylePtr->labelFont, 0, itemPtr->label,
		 -1, &w, &h);
	    itemPtr->labelWidth = w;
	    itemPtr->labelHeight = h;
	    if (itemPtr->height < itemPtr->labelHeight) {
		itemPtr->height = itemPtr->labelHeight;
	    }
	}
	if (itemPtr->labelWidth > 0) {
	    itemPtr->width += itemPtr->labelWidth + ITEM_IPAD;
	}
	if (itemPtr->flags & ITEM_CASCADE) {
	    itemPtr->rightIndWidth = ITEM_R_IND_WIDTH;
	    itemPtr->rightIndHeight = ITEM_R_IND_HEIGHT;
	} else if (itemPtr->accel != NULL) {
	    unsigned int w, h;
	    
	    Blt_GetTextExtents(itemPtr->stylePtr->accelFont, 0, itemPtr->accel,
		 -1, &w, &h);
	    itemPtr->rightIndWidth = w;
	    itemPtr->rightIndHeight = h;
	}
	if (itemPtr->height < itemPtr->rightIndHeight) {
	    itemPtr->height = itemPtr->rightIndHeight;
	}
    }
    itemPtr->width += 2 * itemPtr->stylePtr->borderWidth;
    itemPtr->height += 2 * (ITEM_XPAD + itemPtr->stylePtr->borderWidth);
#ifdef notdef
    fprintf(stderr, "%s= w=%d,h=%d leftInd w=%d,h=%d icon w=%d,h=%d label w=%d,h=%d rightInd w=%d,h=%d\n", 
	    itemPtr->label, itemPtr->width, itemPtr->height, 
	    itemPtr->leftIndWidth, itemPtr->leftIndHeight, 
	    itemPtr->iconWidth, itemPtr->iconHeight, itemPtr->labelWidth,
	    itemPtr->labelHeight, itemPtr->rightIndWidth, 
	    itemPtr->rightIndHeight);
#endif
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
SearchForItem(ComboMenu *comboPtr, Item *firstPtr, Item *lastPtr, int yOffset)
{
    Blt_ChainLink first, last, link, next;

    first = (firstPtr == NULL) ? 
	Blt_Chain_FirstLink(comboPtr->chain): firstPtr->link;
    last = (lastPtr == NULL) ? 
	Blt_Chain_LastLink(comboPtr->chain) : lastPtr->link;

    for (link = first; link != NULL; link = next) {
	Item *itemPtr;

	next = Blt_Chain_NextLink(link);
	itemPtr = Blt_Chain_GetValue(link);
	if ((yOffset >= itemPtr->yWorld) && 
	    (yOffset < (itemPtr->yWorld + itemPtr->height))) {
	    return itemPtr;
	}
	if (link == last) {
	    break;
	}
    }
    return NULL;
}

static void
ComputeVisibleItems(ComboMenu *comboPtr)
{
    Item *itemPtr;

    if (Blt_Chain_GetLength(comboPtr->chain) == 0) {
	comboPtr->firstPtr = comboPtr->lastPtr = NULL;
	return;
    }
    itemPtr = SearchForItem(comboPtr, NULL, NULL, comboPtr->yOffset);
    if (itemPtr == NULL) {
	Blt_ChainLink link;

	link = Blt_Chain_LastLink(comboPtr->chain);
	comboPtr->firstPtr = comboPtr->lastPtr = Blt_Chain_GetValue(link);
	return;
    }
    comboPtr->firstPtr = itemPtr;

    itemPtr = SearchForItem(comboPtr, comboPtr->firstPtr, NULL, 
			    comboPtr->yOffset + VPORTHEIGHT(comboPtr)); 
    if (itemPtr == NULL) {
	Blt_ChainLink link;

	link = Blt_Chain_LastLink(comboPtr->chain);
	comboPtr->lastPtr = Blt_Chain_GetValue(link);
	return;
    } 
    comboPtr->lastPtr = itemPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * NearestItem --
 *
 *	Find the item closest to the x-y screen coordinate given.  The item
 *	must be (visible) in the viewport.
 *
 * Results:
 *	Returns the closest item.  If selectOne is set, then always returns an
 *	item (unless the menu is empty).  Otherwise, NULL is returned is the
 *	pointer is not over an item.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Item *
NearestItem(ComboMenu *comboPtr, int x, int y, int selectOne)
{
    Item *itemPtr;

    if (comboPtr->firstPtr == NULL) {
	return NULL;		/* No visible entries. */
    }
    if ((x < 0) || (x >= Tk_Width(comboPtr->tkwin)) || 
	(y < 0) || (y >= Tk_Height(comboPtr->tkwin))) {
	return NULL;		/* Screen coordinates are outside of menu. */
    }
    /*
     * Item positions are saved in world coordinates. Convert the text point
     * screen y-coordinate to a world coordinate.
     */
    itemPtr = SearchForItem(comboPtr, comboPtr->firstPtr, comboPtr->lastPtr, 
	WORLDY(comboPtr, y));
    if (itemPtr == NULL) {
	if (!selectOne) {
	    return NULL;
	}
	if (y < comboPtr->borderWidth) {
	    return FirstItem(comboPtr);
	}
	return LastItem(comboPtr);
    }
    return itemPtr;
}

static void
ComputeCascadeMenuCoords(ComboMenu *comboPtr, Item *itemPtr, Tk_Window menuWin, 
			 int *xPtr, int *yPtr)
{
    int rootX, rootY, x, y;
    int screenWidth, screenHeight;

    x = Tk_Width(comboPtr->tkwin);
    y = SCREENY(comboPtr, itemPtr->yWorld);
    screenWidth = WidthOfScreen(Tk_Screen(menuWin));
    screenHeight = HeightOfScreen(Tk_Screen(menuWin));
    Tk_GetRootCoords(comboPtr->tkwin, &rootX, &rootY);
    if (rootX < 0) {
	rootX = 0;
    }
    if (rootY < 0) {
	rootY = 0;
    }
    x += rootX, y += rootY;
    if ((y + Tk_ReqHeight(menuWin)) > screenHeight) {
	/* If we go offscreen on the bottom, raised the menu. */
	y = screenHeight - Tk_ReqHeight(menuWin) - 10;
	if (y < 0) {
	    y = 0;
	}
    }
    if ((x + Tk_ReqWidth(menuWin)) > screenWidth) {
	/* If we go offscreen on the bottom, try the menu on the other side. */
	x = rootX - Tk_ReqWidth(menuWin);
	if (x < 0) {
	    x = 0;
	}
    }
    *xPtr = x;
    *yPtr = y;
}

/*
 *---------------------------------------------------------------------------
 *
 * UnpostCascade --
 *
 *	This procedure arranges for the currently active item's cascade menu
 *	to be unposted (i.e. the submenu is unmapped).  Only the active item
 *	can have it's submenu unposted.
 *
 * Results:
 *	A standard TCL return result.  Errors may occur in the TCL commands
 *	generated to unpost submenus.
 *
 * Side effects:
 *	The currently active item's submenu is unposted.
 *
 *---------------------------------------------------------------------------
 */
static int
UnpostCascade(
    Tcl_Interp *interp,		/* Used for invoking "unpost" commands and
				 * reporting errors. */
    ComboMenu *comboPtr)	/* Information about the menu. */
{
    Item *itemPtr = comboPtr->postedPtr;
    char *menuName;
    Tk_Window menuWin;

    if ((itemPtr == NULL) || (itemPtr->menuObjPtr == NULL)) {
	return TCL_OK;		/* No item currenly posted or no menu
				 * designated for cascade item. */
    }
    comboPtr->postedPtr = NULL;
    assert((itemPtr != NULL) && (itemPtr->flags & ITEM_CASCADE));
    menuName = Tcl_GetString(itemPtr->menuObjPtr);
    menuWin = Tk_NameToWindow(interp, menuName, comboPtr->tkwin);
    if (menuWin == NULL) {
	return TCL_ERROR;
    }
    if (Tk_Parent(menuWin) != comboPtr->tkwin) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't unpost \"", Tk_PathName(menuWin), 
			"\": it isn't a descendant of ", 
			Tk_PathName(comboPtr->tkwin), (char *)NULL);
	}
	return TCL_ERROR;
    }
    Blt_UnmapToplevelWindow(menuWin);
    /*
     * Note: when unposting a submenu, we have to redraw the entire
     * parent menu.  This is because of a combination of the following
     * things:
     * (a) the submenu partially overlaps the parent.
     * (b) the submenu specifies "save under", which causes the X
     *     server to make a copy of the information under it when it
     *     is posted.  When the submenu is unposted, the X server
     *     copies this data back and doesn't generate any Expose
     *     events for the parent.
     * (c) the parent may have redisplayed itself after the submenu
     *     was posted, in which case the saved information is no
     *     longer correct.
     * The simplest solution is just force a complete redisplay of
     * the parent.
     */
    
    EventuallyRedraw(comboPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * PostCascade --
 *
 *	This procedure arranges for the currently active item's cascade menu
 *	to be posted.  Only the active item can have it's submenu posted.
 *
 * Results:
 *	A standard TCL return result.  Errors may occur in the TCL commands
 *	generated to post submenus.
 *
 * Side effects:
 *	The new submenu is posted.
 *
 *---------------------------------------------------------------------------
 */
static int
PostCascade(
    Tcl_Interp *interp,		/* Used for invoking "post" command
				 * and reporting errors. */
    ComboMenu *comboPtr,	/* Information about the menu. */
    Item *itemPtr)		/* Cascade item   */
{
    char *menuName;
    Tk_Window menuWin;

    assert((itemPtr != NULL) && (itemPtr->flags & ITEM_CASCADE));
    if (itemPtr->menuObjPtr == NULL) {
	return TCL_OK;		/* No menu was designated for this
				 * cascade item. */
    }
    if (comboPtr->postedPtr == itemPtr) {
#ifdef notdef
	fprintf(stderr, "postcascade: %s is already posted\n",
		Tcl_GetString(itemPtr->menuObjPtr));
#endif
	return TCL_OK;		/* Item is already posted. */
    }
    menuName = Tcl_GetString(itemPtr->menuObjPtr);
    menuWin = Tk_NameToWindow(interp, menuName, comboPtr->tkwin);
    if (menuWin == NULL) {
	return TCL_ERROR;
    }
    if (Tk_Parent(menuWin) != comboPtr->tkwin) {
	Tcl_AppendResult(interp, "can't post \"", Tk_PathName(menuWin), 
		"\": it isn't a descendant of ", Tk_PathName(comboPtr->tkwin),
		(char *)NULL);
	return TCL_ERROR;
    }
    if (Tk_IsMapped(comboPtr->tkwin)) {
	Tcl_Obj *objv[4];
	int result, x, y;

	/*
	 * Position the cascade with its upper left corner slightly below and
	 * to the left of the upper right corner of the menu entry (this is an
	 * attempt to match Motif behavior).
	 *
	 * The menu has to redrawn so that the entry can change relief.
	 */
	ComputeCascadeMenuCoords(comboPtr, itemPtr, menuWin, &x, &y);
	objv[0] = itemPtr->menuObjPtr;
	objv[1] = Tcl_NewStringObj("post", 4);
	objv[2] = Tcl_NewIntObj(x);
	objv[3] = Tcl_NewIntObj(y);
	Tcl_IncrRefCount(objv[0]);
	Tcl_IncrRefCount(objv[1]);
	Tcl_IncrRefCount(objv[2]);
	Tcl_IncrRefCount(objv[3]);
	result = Tcl_EvalObjv(interp, 4, objv, 0);
	Tcl_DecrRefCount(objv[3]);
	Tcl_DecrRefCount(objv[2]);
	Tcl_DecrRefCount(objv[1]);
	Tcl_DecrRefCount(objv[0]);
	if (result != TCL_OK) {
	    return result;
	}
	EventuallyRedrawItem(itemPtr);
    }
    comboPtr->postedPtr = itemPtr;
    return TCL_OK;
}

static void
ComputeMenuGeometry(ComboMenu *comboPtr)
{
    int xWorld, yWorld;
    Blt_ChainLink link;
    int w, h;

    /* 
     * Step 1.  Collect the maximum widths of the items in their individual
     *		columns.
     */
    comboPtr->worldHeight = 0;
    comboPtr->width = comboPtr->height = 0;
    comboPtr->leftIndWidth = comboPtr->rightIndWidth = 0;
    comboPtr->iconWidth = comboPtr->labelWidth = 0;
    xWorld = yWorld = 0;
    for (link = Blt_Chain_FirstLink(comboPtr->chain); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	Item *itemPtr;

	itemPtr = Blt_Chain_GetValue(link);
	ComputeItemGeometry(comboPtr, itemPtr);
	if (itemPtr->leftIndWidth > comboPtr->leftIndWidth) {
	    comboPtr->leftIndWidth = itemPtr->leftIndWidth;
	}
	if (itemPtr->iconWidth > comboPtr->iconWidth) {
	    comboPtr->iconWidth = itemPtr->iconWidth;
	}
	if (itemPtr->labelWidth > comboPtr->labelWidth) {
	    comboPtr->labelWidth = itemPtr->labelWidth;
	}
	if (itemPtr->rightIndWidth > comboPtr->rightIndWidth) {
	    comboPtr->rightIndWidth = itemPtr->rightIndWidth;
	}
	comboPtr->worldHeight += itemPtr->height;
	itemPtr->xWorld = xWorld, itemPtr->yWorld = yWorld;
	yWorld += itemPtr->height;
    }
    comboPtr->worldWidth = comboPtr->leftIndWidth + comboPtr->iconWidth + 
	comboPtr->labelWidth + comboPtr->rightIndWidth;
    if (comboPtr->leftIndWidth > 0) {
	comboPtr->worldWidth += ITEM_IPAD;
    }
    if (comboPtr->iconWidth > 0) {
	comboPtr->worldWidth += ITEM_IPAD;
    }
    if (comboPtr->labelWidth > 0) {
	comboPtr->worldWidth += ITEM_IPAD;
    }
    if (comboPtr->rightIndWidth > 0) {
	comboPtr->worldWidth += 4 * ITEM_IPAD;
    }
    comboPtr->worldWidth += ITEM_IPAD;
    comboPtr->xScrollbarHeight = comboPtr->yScrollbarWidth = 0;
    if (comboPtr->reqWidth > 0) {
	w = comboPtr->reqWidth;
    } else {
	Tk_Window parent;

	w = comboPtr->worldWidth;
	parent = Tk_Parent(comboPtr->tkwin);
	if (Tk_Class(parent) != Tk_GetUid("ComboMenu")) {
	    w = Tk_Width(parent);
	    if (w <= 1) {
		w = Tk_ReqWidth(parent);
	    }
	}
	if (comboPtr->reqWidth < 0) {
	    int w2;
	    w2 = MIN(-comboPtr->reqWidth, comboPtr->worldWidth);
	    if (w < w2) {
		w = w2;
	    }
	}
	if (WidthOfScreen(Tk_Screen(comboPtr->tkwin)) < w) {
	    w = WidthOfScreen(Tk_Screen(comboPtr->tkwin));
	}
    }
    if ((comboPtr->worldWidth > w) && (comboPtr->xScrollbar != NULL)) {
	comboPtr->xScrollbarHeight = Tk_ReqHeight(comboPtr->xScrollbar);
    }
    if (comboPtr->reqHeight > 0) {
	h = comboPtr->reqHeight;
    } else if (comboPtr->reqHeight < 0) {
	h = MIN(-comboPtr->reqHeight, comboPtr->worldHeight);
    } else {
	h=MIN(HeightOfScreen(Tk_Screen(comboPtr->tkwin)),comboPtr->worldHeight);
    }
    if ((comboPtr->worldHeight > h) && (comboPtr->yScrollbar != NULL)) {
	comboPtr->yScrollbarWidth = Tk_ReqWidth(comboPtr->yScrollbar);
    }
#ifndef notdef
    if (comboPtr->reqWidth == 0) {
#ifdef notdef
	w += 2 * (comboPtr->borderWidth + 2 * ITEM_IPAD);
#endif
	w += comboPtr->yScrollbarWidth;
    }
#endif
    if (comboPtr->reqHeight == 0) {
	h += comboPtr->xScrollbarHeight;
	h += 2 * comboPtr->borderWidth;
    }
    comboPtr->width = w;
    comboPtr->height = h;
#ifdef notdef
    fprintf(stderr, "%s= w=%d leftInd w=%d icon w=%d label w=%d rightInd w=%d\n", 
	    Tk_PathName(comboPtr->tkwin), comboPtr->width, 
	    comboPtr->leftIndWidth, comboPtr->iconWidth, comboPtr->labelWidth,
	    comboPtr->rightIndWidth);
#endif
    if (w != Tk_ReqWidth(comboPtr->tkwin)) {
	comboPtr->xOffset = 0;
    }
    if (h != Tk_ReqHeight(comboPtr->tkwin)) {
	comboPtr->yOffset = 0;
    }

    if ((w != Tk_ReqWidth(comboPtr->tkwin)) || 
	(h != Tk_ReqHeight(comboPtr->tkwin))) {
	Tk_GeometryRequest(comboPtr->tkwin, w, h);
    }
    comboPtr->flags |= CM_SCROLL | CM_LAYOUT;
}

static void
DestroyStyle(Style *stylePtr)
{
    ComboMenu *comboPtr;
    int i;

    stylePtr->refCount--;
    if (stylePtr->refCount > 0) {
	return;
    }
    comboPtr = stylePtr->comboPtr;
    iconOption.clientData = comboPtr;
    Blt_FreeOptions(styleConfigSpecs, (char *)stylePtr, comboPtr->display, 0);
    if (stylePtr->labelActiveGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->labelActiveGC);
    }
    if (stylePtr->labelDisabledGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->labelDisabledGC);
    }
    if (stylePtr->labelNormalGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->labelNormalGC);
    }
    if (stylePtr->accelActiveGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->accelActiveGC);
    }
    if (stylePtr->accelDisabledGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->accelDisabledGC);
    }
    if (stylePtr->accelNormalGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->accelNormalGC);
    }
    if (stylePtr->hPtr != NULL) {
	Blt_DeleteHashEntry(&stylePtr->comboPtr->styleTable, stylePtr->hPtr);
    }
    for (i = 0; i < 3; i++) {
	if (stylePtr->radiobutton[i] != NULL) {
	    Blt_FreePicture(stylePtr->radiobutton[i]);
	}
	if (stylePtr->checkbutton[i] != NULL) {
	    Blt_FreePicture(stylePtr->checkbutton[i]);
	}
    }
    if (stylePtr != &stylePtr->comboPtr->defStyle) {
	Blt_Free(stylePtr);
    }
	
}

static Style *
AddDefaultStyle(Tcl_Interp *interp, ComboMenu *comboPtr)
{
    Blt_HashEntry *hPtr;
    int isNew;
    Style *stylePtr;

    hPtr = Blt_CreateHashEntry(&comboPtr->styleTable, "default", &isNew);
    if (!isNew) {
	Tcl_AppendResult(interp, "combomenu style \"", "default", 
		"\" already exists.", (char *)NULL);
	return NULL;
    }
    stylePtr = &comboPtr->defStyle;
    assert(stylePtr);
    stylePtr->refCount = 1;
    stylePtr->name = Blt_GetHashKey(&comboPtr->styleTable, hPtr);
    stylePtr->hPtr = hPtr;
    stylePtr->comboPtr = comboPtr;
    stylePtr->borderWidth = 0;
    stylePtr->activeRelief = TK_RELIEF_FLAT;
    Blt_SetHashValue(hPtr, stylePtr);
    return TCL_OK;
}

static void
DestroyStyles(ComboMenu *comboPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;

    for (hPtr = Blt_FirstHashEntry(&comboPtr->styleTable, &iter); 
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	Style *stylePtr;

	stylePtr = Blt_GetHashValue(hPtr);
	stylePtr->hPtr = NULL;
	stylePtr->refCount = 0;
	DestroyStyle(stylePtr);
    }
    Blt_DeleteHashTable(&comboPtr->styleTable);
}

/*
 *---------------------------------------------------------------------------
 *
 * GetStyleFromObj --
 *
 *	Gets the style associated with the given name.  
 *
 *---------------------------------------------------------------------------
 */
static int 
GetStyleFromObj(Tcl_Interp *interp, ComboMenu *comboPtr, Tcl_Obj *objPtr,
		Style **stylePtrPtr)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&comboPtr->styleTable, Tcl_GetString(objPtr));
    if (hPtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find style \"", 
		Tcl_GetString(objPtr), "\" in combomenu \"", 
		Tk_PathName(comboPtr->tkwin), "\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    *stylePtrPtr = Blt_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SetTag --
 *
 *	Associates a tag with a given row.  Individual row tags are
 *	stored in hash tables keyed by the tag name.  Each table is in
 *	turn stored in a hash table keyed by the row location.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A tag is stored for a particular row.
 *
 *---------------------------------------------------------------------------
 */
static int
SetTag(Tcl_Interp *interp, Item *itemPtr, const char *tagName)
{
    Blt_HashEntry *hPtr;
    Blt_HashTable *tagTablePtr;
    ComboMenu *comboPtr;
    int isNew;
    long dummy;
    
    if ((strcmp(tagName, "all") == 0) || (strcmp(tagName, "end") == 0)) {
	return TCL_OK;		/* Don't need to create reserved
				 * tags. */
    }
    if (tagName[0] == '\0') {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "tag \"", tagName, "\" can't be empty.", 
		(char *)NULL);
	}
	return TCL_ERROR;
    }
    if (tagName[0] == '-') {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "tag \"", tagName, 
		"\" can't start with a '-'.", (char *)NULL);
	}
	return TCL_ERROR;
    }
    if (Tcl_GetLong(NULL, (char *)tagName, &dummy) == TCL_OK) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "tag \"", tagName, "\" can't be a number.",
			     (char *)NULL);
	}
	return TCL_ERROR;
    }
    comboPtr = itemPtr->comboPtr;
    hPtr = Blt_CreateHashEntry(&comboPtr->tagTable, tagName, &isNew);
    if (hPtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't add tag \"", tagName, 
			 "\": out of memory", (char *)NULL);
	}
	return TCL_ERROR;
    }
    if (isNew) {
	tagTablePtr = Blt_AssertMalloc(sizeof(Blt_HashTable));
	Blt_InitHashTable(tagTablePtr, BLT_ONE_WORD_KEYS);
	Blt_SetHashValue(hPtr, tagTablePtr);
    } else {
	tagTablePtr = Blt_GetHashValue(hPtr);
    }
    hPtr = Blt_CreateHashEntry(tagTablePtr, (char *)itemPtr->index, &isNew);
    if (isNew) {
	Blt_SetHashValue(hPtr, itemPtr);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetTagTable --
 *
 *	Returns the hash table containing row indices for a tag.
 *
 * Results:
 *	Returns a pointer to the hash table containing indices for the
 *	given tag.  If the row has no tags, then NULL is returned.
 *
 *---------------------------------------------------------------------------
 */
static Blt_HashTable *
GetTagTable(ComboMenu *comboPtr, const char *tagName)		
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&comboPtr->tagTable, tagName);
    if (hPtr == NULL) {
	return NULL;		/* No tag by that name. */
    }
    return Blt_GetHashValue(hPtr);
}


static void
UnmanageScrollbar(ComboMenu *comboPtr, Tk_Window scrollbar)
{
    if (scrollbar != NULL) {
	Tk_DeleteEventHandler(scrollbar, StructureNotifyMask,
	      ScrollbarEventProc, comboPtr);
	Tk_ManageGeometry(scrollbar, (Tk_GeomMgr *)NULL, comboPtr);
	if (Tk_IsMapped(scrollbar)) {
	    Tk_UnmapWindow(scrollbar);
	}
    }
}

static void
ManageScrollbar(ComboMenu *comboPtr, Tk_Window scrollbar)
{
    if (scrollbar != NULL) {
	Tk_CreateEventHandler(scrollbar, StructureNotifyMask, 
		ScrollbarEventProc, comboPtr);
	Tk_ManageGeometry(scrollbar, &comboMgrInfo, comboPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * InstallScrollbar --
 *
 *	Convert the string representation of a color into a XColor pointer.
 *
 * Results:
 *	The return value is a standard TCL result.  The color pointer is
 *	written into the widget record.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
InstallScrollbar(
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    ComboMenu *comboPtr,
    Tcl_Obj *objPtr,		/* String representing scrollbar window. */
    Tk_Window *tkwinPtr)
{
    Tk_Window tkwin;

    if (objPtr == NULL) {
	*tkwinPtr = NULL;
	return;
    }
    tkwin = Tk_NameToWindow(interp, Tcl_GetString(objPtr), comboPtr->tkwin);
    if (tkwin == NULL) {
	Tcl_BackgroundError(interp);
	return;
    }
    if (Tk_Parent(tkwin) != comboPtr->tkwin) {
	Tcl_AppendResult(interp, "scrollbar \"", Tk_PathName(tkwin), 
			 "\" must be a child of combomenu.", (char *)NULL);
	Tcl_BackgroundError(interp);
	return;
    }
    ManageScrollbar(comboPtr, tkwin);
    *tkwinPtr = tkwin;
    return;
}

static void
InstallXScrollbar(ClientData clientData)
{
    ComboMenu *comboPtr = clientData;

    comboPtr->flags &= ~CM_INSTALL_SCROLLBAR_X;
    InstallScrollbar(comboPtr->interp, comboPtr, comboPtr->xScrollbarObjPtr,
		     &comboPtr->xScrollbar);
}

static void
InstallYScrollbar(ClientData clientData)
{
    ComboMenu *comboPtr = clientData;

    comboPtr->flags &= ~CM_INSTALL_SCROLLBAR_Y;
    InstallScrollbar(comboPtr->interp, comboPtr, comboPtr->yScrollbarObjPtr,
		     &comboPtr->yScrollbar);
}


static int
GetIconFromObj(Tcl_Interp *interp, ComboMenu *comboPtr, Tcl_Obj *objPtr, 
	       Icon *iconPtr)
{
    Blt_HashEntry *hPtr;
    struct _Icon *iPtr;
    int isNew;
    const char *iconName;

    iconName = Tcl_GetString(objPtr);
    hPtr = Blt_CreateHashEntry(&comboPtr->iconTable, iconName, &isNew);
    if (isNew) {
	Tk_Image tkImage;
	int w, h;

	tkImage = Tk_GetImage(interp, comboPtr->tkwin, (char *)iconName, 
		IconChangedProc, comboPtr);
	if (tkImage == NULL) {
	    Blt_DeleteHashEntry(&comboPtr->iconTable, hPtr);
	    return TCL_ERROR;
	}
	Tk_SizeOfImage(tkImage, &w, &h);
	iPtr = Blt_AssertMalloc(sizeof(struct _Icon ));
	iPtr->tkImage = tkImage;
	iPtr->hPtr = hPtr;
	iPtr->refCount = 1;
	iPtr->width = w;
	iPtr->height = h;
	Blt_SetHashValue(hPtr, iPtr);
    } else {
	iPtr = Blt_GetHashValue(hPtr);
	iPtr->refCount++;
    }
    *iconPtr = iPtr;
    return TCL_OK;
}

static void
FreeIcon(ComboMenu *comboPtr, struct _Icon *iPtr)
{
    iPtr->refCount--;
    if (iPtr->refCount == 0) {
	Blt_DeleteHashEntry(&comboPtr->iconTable, iPtr->hPtr);
	Tk_FreeImage(iPtr->tkImage);
	Blt_Free(iPtr);
    }
}

static void
DestroyIcons(ComboMenu *comboPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;

    for (hPtr = Blt_FirstHashEntry(&comboPtr->iconTable, &iter);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	Icon icon;

	icon = Blt_GetHashValue(hPtr);
	Tk_FreeImage(IconImage(icon));
	Blt_Free(icon);
    }
    Blt_DeleteHashTable(&comboPtr->iconTable);
}

static INLINE Item *
FindItemByLabel(ComboMenu *comboPtr, const char *label)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&comboPtr->labelTable, label);
    if (hPtr != NULL) {
	Blt_Chain chain;
	Blt_ChainLink link;

	chain = Blt_GetHashValue(hPtr);
	link = Blt_Chain_FirstLink(chain);
	return Blt_Chain_GetValue(link);
    }
    return NULL;
}

static char *
NewLabel(Item *itemPtr, const char *label)
{
    Blt_Chain chain;
    Blt_HashEntry *hPtr;
    int isNew;
    ComboMenu *comboPtr;

    comboPtr = itemPtr->comboPtr;
    hPtr = Blt_CreateHashEntry(&comboPtr->labelTable, label, &isNew);
    if (isNew) {
	chain = Blt_Chain_Create();
	Blt_SetHashValue(hPtr, chain);
    } else {
	chain = Blt_GetHashValue(hPtr);
    }
    Blt_Chain_Append(chain, itemPtr);
    return Blt_GetHashKey(&comboPtr->labelTable, hPtr);
}

static void
RemoveLabel(ComboMenu *comboPtr, Item *itemPtr)
{
    Blt_HashEntry *hPtr;
	
    hPtr = Blt_FindHashEntry(&comboPtr->labelTable, itemPtr->label);
    if (hPtr != NULL) {
	Blt_Chain chain;
	Blt_ChainLink link;
	
	chain = Blt_GetHashValue(hPtr);
	for (link = Blt_Chain_FirstLink(chain); link != NULL;
	     link = Blt_Chain_NextLink(link)) {
	    Item *ip;
	    
	    ip = Blt_Chain_GetValue(link);
	    if (ip == itemPtr) {
		itemPtr->label = (char *)emptyString;
		Blt_Chain_DeleteLink(chain, link);
		if (Blt_Chain_GetLength(chain) == 0) {
		    Blt_Chain_Destroy(chain);
		    Blt_DeleteHashEntry(&comboPtr->labelTable, hPtr);
		    return;
		}
	    }
	}
    }
}

static void
DestroyLabels(ComboMenu *comboPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;

    for (hPtr = Blt_FirstHashEntry(&comboPtr->labelTable, &iter); 
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	Blt_Chain chain;
	
	chain = Blt_GetHashValue(hPtr);
	Blt_Chain_Destroy(chain);
    }
    Blt_DeleteHashTable(&comboPtr->labelTable);
}

static void
MoveItem(ComboMenu *comboPtr, Item *itemPtr, int dir, Item *wherePtr)
{
    if (Blt_Chain_GetLength(comboPtr->chain) == 1) {
	return;			/* Can't rearrange one item. */
    }
    Blt_Chain_UnlinkLink(comboPtr->chain, itemPtr->link);
    switch(dir) {
    case 0:			/* After */
	Blt_Chain_LinkAfter(comboPtr->chain, itemPtr->link, wherePtr->link);
	break;
    case 1:			/* At */
	Blt_Chain_LinkAfter(comboPtr->chain, itemPtr->link, wherePtr->link);
	break;
    default:
    case 2:			/* Before */
	Blt_Chain_LinkBefore(comboPtr->chain, itemPtr->link, wherePtr->link);
	break;
    }
    {
	Blt_ChainLink link;
	long count;

	for (count = 1, link = Blt_Chain_FirstLink(comboPtr->chain); 
	     link != NULL; link = Blt_Chain_NextLink(link), count++) {
	    itemPtr = Blt_Chain_GetValue(link);
	    itemPtr->index = count;
	}
    }
}

static int 
GetTypeFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int *typePtr)
{
    char *string;
    int length, flag;
    char c;

    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[0];
    if ((c == 'r') && (length > 1) && 
	(strncmp(string, "radiobutton", length) == 0)) {
	flag = ITEM_RADIOBUTTON;
    } else if ((c == 'c') && (length > 1) && 
	(strncmp(string, "command", length) == 0)) {
	flag = ITEM_BUTTON;
    } else if ((c == 'c') && (length > 1) && 
	(strncmp(string, "cascade", length) == 0)) {
	flag = ITEM_CASCADE;
    } else if ((c == 'c') && (length > 1) && 
	(strncmp(string, "checkbutton", length) == 0)) {
	flag = ITEM_CHECKBUTTON;
    } else if ((c == 's') && (length > 1) && 
	(strncmp(string, "separator", length) == 0)) {
	flag = ITEM_SEPARATOR;
    } else {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "unknown item type \"", string, 
			     "\": should be command, checkbutton, cascade, ",
			     "radiobutton, or separator.", (char *)NULL);
	}
	return TCL_ERROR;
    }
    *typePtr = flag;
    return TCL_OK;
}


static const char *
NameOfType(unsigned int flags)
{
    if (flags & ITEM_BUTTON) {
	return "command";
    }
    if (flags & ITEM_RADIOBUTTON) {
	return "radiobutton";
    }
    if (flags & ITEM_CHECKBUTTON) {
	return "checkbutton";
    }
    if (flags & ITEM_CASCADE) {
	return "cascade";
    }
    if (flags & ITEM_SEPARATOR) {
	return "separator";
    }
    return "???";
}	

/*
 *---------------------------------------------------------------------------
 *
 * NextTaggedItem --
 *
 *	Returns the next item derived from the given tag.
 *
 * Results:
 *	Returns the row location of the first item.  If no more rows can be
 *	found, then NULL is returned.
 *
 *---------------------------------------------------------------------------
 */
static Item *
NextTaggedItem(ItemIterator *iterPtr)
{
    if (iterPtr->type == ITER_TAG) {
	Blt_HashEntry *hPtr;

	hPtr = Blt_NextHashEntry(&iterPtr->cursor); 
	if (hPtr != NULL) {
	    return Blt_GetHashValue(hPtr);
	}
    } else if (iterPtr->type == ITER_TYPE) {
	Item *itemPtr;

	itemPtr = iterPtr->nextPtr;
	if (itemPtr == NULL) {
	    return itemPtr;
	}
	while (itemPtr != iterPtr->endPtr) {
	    if (itemPtr->flags & iterPtr->itemType) {
		break;
	    }
	    itemPtr = StepItem(itemPtr);
	}
	if (itemPtr == iterPtr->endPtr) {
	    iterPtr->nextPtr = NULL;
	} else {
	    iterPtr->nextPtr = StepItem(itemPtr);
	}
	return itemPtr;
    } else if (iterPtr->nextPtr != NULL) {
	Item *itemPtr;
	
	itemPtr = iterPtr->nextPtr;
	if (itemPtr == iterPtr->endPtr) {
	    iterPtr->nextPtr = NULL;
	} else {
	    iterPtr->nextPtr = StepItem(itemPtr);
	}
	return itemPtr;
    }	
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * FirstTaggedItem --
 *
 *	Returns the first item derived from the given tag.
 *
 * Results:
 *	Returns the row location of the first item.  If no more rows can be
 *	found, then -1 is returned.
 *
 *---------------------------------------------------------------------------
 */
static Item *
FirstTaggedItem(ItemIterator *iterPtr)
{
    if (iterPtr->type == ITER_TAG) {
	Blt_HashEntry *hPtr;
	
	hPtr = Blt_FirstHashEntry(iterPtr->tablePtr, &iterPtr->cursor);
	if (hPtr == NULL) {
	    return NULL;
	}
	return Blt_GetHashValue(hPtr);
    } else if (iterPtr->type == ITER_TYPE) {
	Item *itemPtr;

	itemPtr = iterPtr->startPtr;
	if (itemPtr == NULL) {
	    return itemPtr;
	}
	while (itemPtr != iterPtr->endPtr) {
	    if (itemPtr->flags & iterPtr->itemType) {
		break;
	    }
	    itemPtr = StepItem(itemPtr);
	}
	if (itemPtr == iterPtr->endPtr) {
	    iterPtr->nextPtr = NULL;
	} else {
	    iterPtr->nextPtr = StepItem(itemPtr);
	}
	return itemPtr;
    } else if (iterPtr->startPtr != NULL) {
	Item *itemPtr;
	
	itemPtr = iterPtr->startPtr;
	iterPtr->nextPtr = NextTaggedItem(iterPtr);
	return itemPtr;
    } 
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetItemFromObj --
 *
 *	Gets the item associated the given index, tag, or label.  This routine
 *	is used when you want only one item.  It's an error if more than one
 *	item is specified (e.g. "all" tag or range "1:4").  It's also an error
 *	if the tag is empty (no items are currently tagged).
 *
 *---------------------------------------------------------------------------
 */
static int 
GetItemFromObj(
    Tcl_Interp *interp, 
    ComboMenu *comboPtr,
    Tcl_Obj *objPtr,
    Item **itemPtrPtr)
{
    ItemIterator iter;
    Item *firstPtr;

    if (GetItemIterator(interp, comboPtr, objPtr, &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    firstPtr = FirstTaggedItem(&iter);
    if (firstPtr != NULL) {
	Item *nextPtr;

	nextPtr = NextTaggedItem(&iter);
	if (nextPtr != NULL) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "multiple items specified by \"", 
			Tcl_GetString(objPtr), "\"", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
    }
    *itemPtrPtr = firstPtr;
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * GetItemIterator --
 *
 *	Converts a string representing a item index into an item pointer.  The
 *	index may be in one of the following forms:
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
GetItemIterator(
    Tcl_Interp *interp,
    ComboMenu *comboPtr,
    Tcl_Obj *objPtr,
    ItemIterator *iterPtr)
{
    Item *itemPtr, *startPtr, *endPtr;
    char *string;
    char c;
    int badrange;
    int nBytes;
    char *p, *pend, *rp;
    long lval;

    iterPtr->comboPtr = comboPtr;
    iterPtr->type = ITER_INDEX;
    iterPtr->tagName = Tcl_GetStringFromObj(objPtr, &nBytes);
    iterPtr->nextPtr = NULL;
    iterPtr->startPtr = iterPtr->endPtr = NULL;

    if (comboPtr->flags & CM_LAYOUT) {
	ComputeMenuGeometry(comboPtr);
    }
    if (comboPtr->flags & CM_SCROLL) {
	ComputeVisibleItems(comboPtr);
    } 
    rp = NULL;
    for (p = iterPtr->tagName, pend = p + nBytes; p < pend; p++) {
	if (*p != '-') {
	    continue;
	}
	if (rp != NULL) {
	    /* Found more than one range specifier. We'll assume it's not a
	     * range and try is as a regular index, tag, or label. */
	    rp = NULL;
	    break;
	}
	rp = p;
    } 
    badrange = FALSE;
    if ((rp != NULL) && (rp != iterPtr->tagName) && (rp != (pend - 1))) {
	long length;
	Tcl_Obj *objPtr1, *objPtr2;
	Item *fromPtr, *toPtr;
	int result;
	
	length = rp - iterPtr->tagName;
	objPtr1 = Tcl_NewStringObj(iterPtr->tagName, length);
	rp++;
	objPtr2 = Tcl_NewStringObj(rp, pend - rp);
	result = GetItemFromObj(interp, comboPtr, objPtr1, &fromPtr);
	if ((fromPtr != NULL) && (result == TCL_OK)) {
	    result = GetItemFromObj(interp, comboPtr, objPtr2, &toPtr);
	}
	Tcl_DecrRefCount(objPtr1);
	Tcl_DecrRefCount(objPtr2);
	if ((toPtr != NULL) && (result == TCL_OK)) {
	    iterPtr->startPtr = fromPtr;
	    iterPtr->endPtr = toPtr;
	    iterPtr->type = ITER_RANGE;
	    return TCL_OK;
	}
	badrange = TRUE;
    }
    string = Tcl_GetString(objPtr);
    c = string[0];
    iterPtr->startPtr = iterPtr->endPtr = comboPtr->activePtr;
    startPtr = endPtr = itemPtr = NULL;
    if (c == '\0') {
	startPtr = endPtr = NULL;
    } else if (Tcl_GetLongFromObj(NULL, objPtr, &lval) == TCL_OK) {
	if (lval < 0) {
	    itemPtr = NULL;
	} else {
	    itemPtr = FindItemByIndex(comboPtr, (long)lval);
	}
	if (itemPtr == NULL) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "can't find item: bad index \"", 
				 Tcl_GetString(objPtr), "\"", (char *)NULL);
	    }
	    return TCL_ERROR;
	}		
	startPtr = endPtr = itemPtr;
    } else if ((c == 'a') && (strcmp(iterPtr->tagName, "active") == 0)) {
	startPtr = endPtr = comboPtr->activePtr;
    } else if ((c == 'n') && (strcmp(iterPtr->tagName, "next") == 0)) {
	itemPtr = NextItem(comboPtr->activePtr);
	if (itemPtr == NULL) {
	    itemPtr = comboPtr->activePtr;
	}
	startPtr = endPtr = itemPtr;
    } else if ((c == 'p') && (strcmp(iterPtr->tagName, "previous") == 0)) {
	itemPtr = PrevItem(comboPtr->activePtr);
	if (itemPtr == NULL) {
	    itemPtr = comboPtr->activePtr;
	}
	startPtr = endPtr = itemPtr;
    } else if ((c == 'e') && (strcmp(iterPtr->tagName, "end") == 0)) {
	startPtr = endPtr = LastItem(comboPtr);
    } else if ((c == 'f') && (strcmp(iterPtr->tagName, "first") == 0)) {
	startPtr = endPtr = FirstItem(comboPtr);
    } else if ((c == 'l') && (strcmp(iterPtr->tagName, "last") == 0)) {
	startPtr = endPtr = LastItem(comboPtr);
    } else if ((c == 'v') && (strcmp(iterPtr->tagName, "view.top") == 0)) {
	startPtr = endPtr = comboPtr->firstPtr;
    } else if ((c == 'v') && (strcmp(iterPtr->tagName, "view.bottom") == 0)) {
	startPtr = endPtr = comboPtr->lastPtr;
    } else if ((c == 'n') && (strcmp(iterPtr->tagName, "none") == 0)) {
	startPtr = endPtr = NULL;
    } else if ((c == 'a') && (strcmp(iterPtr->tagName, "all") == 0)) {
	iterPtr->type  = ITER_ALL;
	startPtr = BeginItem(comboPtr);
	endPtr   = EndItem(comboPtr);
    } else if (c == '@') {
	int x, y;

	if (Blt_GetXY(comboPtr->interp, comboPtr->tkwin, iterPtr->tagName, 
		      &x, &y) != TCL_OK) {
	    return TCL_ERROR;
	}
	itemPtr = NearestItem(comboPtr, x, y, TRUE);
	if ((itemPtr != NULL) && (itemPtr->flags & ITEM_DISABLED)) {
	    itemPtr = NextItem(itemPtr);
	}
	startPtr = endPtr = itemPtr;
    } else {
	int type;

	itemPtr = FindItemByLabel(comboPtr, iterPtr->tagName);
	if (itemPtr != NULL) {
	    startPtr = endPtr = itemPtr;
	} else if (GetTypeFromObj(NULL, objPtr, &type) == TCL_OK) {
	    iterPtr->type = ITER_TYPE;
	    iterPtr->itemType = type;
	    iterPtr->startPtr = BeginItem(comboPtr);
	    iterPtr->endPtr   = EndItem(comboPtr);
	    return TCL_OK;
	} else {
	    iterPtr->tablePtr = GetTagTable(comboPtr, iterPtr->tagName);
	    if (iterPtr->tablePtr != NULL) {
		iterPtr->type = ITER_TAG;
		return TCL_OK;
	    }
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "can't find tag or item \"", string,
			"\" in \"", Tk_PathName(comboPtr->tkwin), "\"", 
		(char *)NULL);
	    }
	    return TCL_ERROR;
	}
    }
    if (startPtr != NULL) {
	iterPtr->startPtr = startPtr;
    }
    if (endPtr != NULL) {
	iterPtr->endPtr = endPtr;
    }
    iterPtr->startPtr = startPtr;
    iterPtr->endPtr = endPtr;
#ifdef notdef
    if (iterPtr->startPtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find tag or combomenu item \"", 
		string,
		"\" in \"", Tk_PathName(comboPtr->tkwin), "\"", (char *)NULL);
	}
	return TCL_ERROR;
    }	
#endif
    return TCL_OK;
}

    
static int
ConfigureItem(
    Tcl_Interp *interp,
    Item *itemPtr,
    int objc,
    Tcl_Obj *const *objv,
    int flags)
{
    ComboMenu *comboPtr;

    comboPtr = itemPtr->comboPtr;
    iconOption.clientData = comboPtr;
    if (Blt_ConfigureWidgetFromObj(interp, comboPtr->tkwin, itemConfigSpecs, 
	objc, objv, (char *)itemPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    ComputeItemGeometry(comboPtr, itemPtr);
    return TCL_OK;
}

static int
ConfigureStyle(
    Tcl_Interp *interp, 
    Style *stylePtr,
    int objc,
    Tcl_Obj *const *objv,
    int flags)
{
    ComboMenu *comboPtr = stylePtr->comboPtr;
    unsigned int gcMask;
    XGCValues gcValues;
    GC newGC;

    if (Blt_ConfigureWidgetFromObj(interp, comboPtr->tkwin, styleConfigSpecs, 
	objc, objv, (char *)stylePtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Normal label */
    gcMask = GCForeground | GCFont | GCLineWidth;
    gcValues.line_width = 0;
    gcValues.foreground = stylePtr->labelNormalColor->pixel;
    gcValues.font = Blt_FontId(stylePtr->labelFont);
    newGC = Tk_GetGC(comboPtr->tkwin, gcMask, &gcValues);
    if (stylePtr->labelNormalGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->labelNormalGC);
    }
    stylePtr->labelNormalGC = newGC;
	
    /* Disabled label */
    gcMask = GCForeground | GCFont;
    gcValues.foreground = stylePtr->labelDisabledColor->pixel;
    gcValues.font = Blt_FontId(stylePtr->labelFont);
    newGC = Tk_GetGC(comboPtr->tkwin, gcMask, &gcValues);
    if (stylePtr->labelDisabledGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->labelDisabledGC);
    }
    stylePtr->labelDisabledGC = newGC;
	
    /* Active label */
    gcMask = GCForeground | GCFont;
    gcValues.foreground = stylePtr->labelActiveColor->pixel;
    gcValues.font = Blt_FontId(stylePtr->labelFont);
    newGC = Tk_GetGC(comboPtr->tkwin, gcMask, &gcValues);
    if (stylePtr->labelActiveGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->labelActiveGC);
    }
    stylePtr->labelActiveGC = newGC;

    /* Normal accelerator */
    gcMask = GCForeground | GCFont;
    gcValues.foreground = stylePtr->accelNormalColor->pixel;
    gcValues.font = Blt_FontId(stylePtr->accelFont);
    newGC = Tk_GetGC(comboPtr->tkwin, gcMask, &gcValues);
    if (stylePtr->accelNormalGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->accelNormalGC);
    }
    stylePtr->accelNormalGC = newGC;
	
    /* Disabled accelerator */
    gcMask = GCForeground | GCFont;
    gcValues.foreground = stylePtr->accelDisabledColor->pixel;
    gcValues.font = Blt_FontId(stylePtr->accelFont);
    newGC = Tk_GetGC(comboPtr->tkwin, gcMask, &gcValues);
    if (stylePtr->accelDisabledGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->accelDisabledGC);
    }
    stylePtr->accelDisabledGC = newGC;
	
    /* Active accelerator */
    gcMask = GCForeground | GCFont;
    gcValues.foreground = stylePtr->accelActiveColor->pixel;
    gcValues.font = Blt_FontId(stylePtr->accelFont);
    newGC = Tk_GetGC(comboPtr->tkwin, gcMask, &gcValues);
    if (stylePtr->accelActiveGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->accelActiveGC);
    }
    stylePtr->accelActiveGC = newGC;

    /* Indicator foreground. */
    gcMask = GCForeground;
    gcValues.foreground = stylePtr->indFgColor->pixel;
    newGC = Tk_GetGC(comboPtr->tkwin, gcMask, &gcValues);
    if (stylePtr->indFgGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->indFgGC);
    }
    stylePtr->indFgGC = newGC;

    /* Indicator background */
    gcMask = GCForeground;
    gcValues.foreground = stylePtr->indBgColor->pixel;
    newGC = Tk_GetGC(comboPtr->tkwin, gcMask, &gcValues);
    if (stylePtr->indBgGC != NULL) {
	Tk_FreeGC(comboPtr->display, stylePtr->indBgGC);
    }
    stylePtr->indBgGC = newGC;

#ifdef notdef
    if (itemPtr->flags & (ITEM_RADIOBUTTON | ITEM_CHECKBUTTON)) {
	itemPtr->leftIndWidth = ITEM_L_IND_WIDTH + 2 * ITEM_IPAD;
	itemPtr->leftIndHeight = ITEM_L_IND_HEIGHT;
    }
#endif
    return TCL_OK;
}

static int
ConfigureComboMenu(
    Tcl_Interp *interp, 
    ComboMenu *comboPtr,
    int objc,
    Tcl_Obj *const *objv,
    int flags)
{
    if (Blt_ConfigureWidgetFromObj(interp, comboPtr->tkwin, comboConfigSpecs, 
	objc, objv, (char *)comboPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    if (ConfigureStyle(interp, &comboPtr->defStyle, 0, NULL, 
		       BLT_CONFIG_OBJV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }	

    /* Install the embedded scrollbars as needed.  We defer installing the
     * scrollbars so the scrollbar widgets don't have to exist when they are
     * specified by the -xscrollbar and -yscrollbar options respectively. The
     * down-side is that errors found in the scrollbar name will be
     * backgrounded. */
    if (Blt_ConfigModified(comboConfigSpecs, "-xscrollbar", (char *)NULL)) {
	if (comboPtr->xScrollbar != NULL) {
	    UnmanageScrollbar(comboPtr, comboPtr->xScrollbar);
	    comboPtr->xScrollbar = NULL;
	}
	if ((comboPtr->flags & CM_INSTALL_SCROLLBAR_X) == 0) {
	    Tcl_DoWhenIdle(InstallXScrollbar, comboPtr);
	    comboPtr->flags |= CM_INSTALL_SCROLLBAR_X;
	}	    
    }
    if (Blt_ConfigModified(comboConfigSpecs, "-yscrollbar", (char *)NULL)) {
	if (comboPtr->yScrollbar != NULL) {
	    UnmanageScrollbar(comboPtr, comboPtr->yScrollbar);
	    comboPtr->yScrollbar = NULL;
	}
	if ((comboPtr->flags & CM_INSTALL_SCROLLBAR_Y) == 0) {
	    Tcl_DoWhenIdle(InstallYScrollbar, comboPtr);
	    comboPtr->flags |= CM_INSTALL_SCROLLBAR_Y;
	}	    
    }
    return TCL_OK;
}

/* Widget Callbacks */

/*
 *---------------------------------------------------------------------------
 *
 * ComboMenuEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various events on
 * 	comboentry widgets.
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
ComboMenuEventProc(ClientData clientData, XEvent *eventPtr)
{
    ComboMenu *comboPtr = clientData;

    if (eventPtr->type == Expose) {
	if (eventPtr->xexpose.count == 0) {
	    EventuallyRedraw(comboPtr);
	}
    } else if (eventPtr->type == UnmapNotify) {
	if (comboPtr->lastPtr != NULL) {
	    UnpostCascade((Tcl_Interp *)NULL, comboPtr);
	    EventuallyRedraw(comboPtr);
	}
    } else if (eventPtr->type == ConfigureNotify) {
	comboPtr->flags |= (CM_SCROLL | CM_LAYOUT);
	EventuallyRedraw(comboPtr);
    } else if ((eventPtr->type == FocusIn) || (eventPtr->type == FocusOut)) {
	if (eventPtr->xfocus.detail == NotifyInferior) {
	    return;
	}
	if (eventPtr->type == FocusIn) {
	    comboPtr->flags |= CM_FOCUS;
	} else {
	    comboPtr->flags &= ~CM_FOCUS;
	}
	EventuallyRedraw(comboPtr);
    } else if (eventPtr->type == DestroyNotify) {
	if (comboPtr->tkwin != NULL) {
	    comboPtr->tkwin = NULL; 
	}
	if (comboPtr->flags & CM_REDRAW) {
	    Tcl_CancelIdleCall(DisplayComboMenu, comboPtr);
	}
	Tcl_EventuallyFree(comboPtr, DestroyComboMenu);
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * ScrollbarEventProc --
 *
 *	This procedure is invoked by the Tk event handler when StructureNotify
 *	events occur in a scrollbar managed by the widget.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
ScrollbarEventProc(
    ClientData clientData,	/* Pointer to Entry structure for widget
				 * referred to by eventPtr. */
    XEvent *eventPtr)		/* Describes what just happened. */
{
    ComboMenu *comboPtr = clientData;

    if (eventPtr->type == ConfigureNotify) {
	EventuallyRedraw(comboPtr);
    } else if (eventPtr->type == DestroyNotify) {
	if (eventPtr->xany.window == Tk_WindowId(comboPtr->yScrollbar)) {
	    comboPtr->yScrollbar = NULL;
	} else if (eventPtr->xany.window == Tk_WindowId(comboPtr->xScrollbar)) {
	    comboPtr->xScrollbar = NULL;
	} 
	comboPtr->flags |= CM_LAYOUT;;
	EventuallyRedraw(comboPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ScrollbarCustodyProc --
 *
 * 	This procedure is invoked when a scrollbar has been stolen by another
 * 	geometry manager.
 *
 * Results:
 *	None.
 *
 * Side effects:
  *	Arranges for the combomenu to have its layout re-arranged at the next
 *	idle point.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
ScrollbarCustodyProc(
    ClientData clientData,	/* Information about the combomenu. */
    Tk_Window tkwin)		/* Scrollbar stolen by another geometry
				 * manager. */
{
    ComboMenu *comboPtr = (ComboMenu *)clientData;

    if (tkwin == comboPtr->yScrollbar) {
	comboPtr->yScrollbar = NULL;
	comboPtr->yScrollbarWidth = 0;
    } else if (tkwin == comboPtr->xScrollbar) {
	comboPtr->xScrollbar = NULL;
	comboPtr->xScrollbarHeight = 0;
    } else {
	return;		
    }
    Tk_UnmaintainGeometry(tkwin, comboPtr->tkwin);
    comboPtr->flags |= CM_LAYOUT;
    EventuallyRedraw(comboPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * ScrollbarGeometryProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for scrollbars managed
 *	by the combomenu.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the combomenu to have its layout re-computed and
 *	re-arranged at the next idle point.
 *
 * -------------------------------------------------------------------------- */
/* ARGSUSED */
static void
ScrollbarGeometryProc(
    ClientData clientData,	/* ComboMenu widget record.  */
    Tk_Window tkwin)		/* Scrollbar whose geometry has changed. */
{
    ComboMenu *comboPtr = (ComboMenu *)clientData;

    comboPtr->flags |= CM_LAYOUT;
    EventuallyRedraw(comboPtr);
}

/*
 *---------------------------------------------------------------------------
 * 
 * ItemVarTraceProc --
 *
 *	This procedure is invoked when someone changes the state variable
 *	associated with a radiobutton or checkbutton entry.  The entry's
 *	selected state is set to match the value of the variable.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The combobox entry may become selected or deselected.
 *
 *---------------------------------------------------------------------------
 */
static char *
ItemVarTraceProc(
    ClientData clientData,	/* Information about the item. */
    Tcl_Interp *interp,		/* Interpreter containing variable. */
    const char *name1,		/* First part of variable's name. */
    const char *name2,		/* Second part of variable's name. */
    int flags)			/* Describes what just happened. */
{
    Item *itemPtr = clientData;
    Tcl_Obj *valueObjPtr;
    int bool;

    assert(itemPtr->varNameObjPtr != NULL);
    if (flags & TCL_INTERP_DESTROYED) {
    	return NULL;		/* Interpreter is going away. */

    }
    /*
     * If the variable is being unset, then re-establish the trace.
     */
    if (flags & TCL_TRACE_UNSETS) {
	itemPtr->flags &= ~ITEM_SELECTED;
	if (flags & TCL_TRACE_DESTROYED) {
	    char *varName;

	    varName = Tcl_GetString(itemPtr->varNameObjPtr);
	    Tcl_TraceVar(interp, varName, VAR_FLAGS, ItemVarTraceProc, 
		clientData);
	}
	goto done;
    }

    if ((itemPtr->flags & (ITEM_RADIOBUTTON | ITEM_CHECKBUTTON)) == 0) {
	return NULL;		/* Not a radiobutton or checkbutton. */
    }

    /*
     * Use the value of the variable to update the selected status of the
     * item.
     */
    valueObjPtr = Tcl_ObjGetVar2(interp, itemPtr->varNameObjPtr, NULL, 
		TCL_GLOBAL_ONLY);
    if (valueObjPtr == NULL) {
	return NULL;		/* Can't get value of variable. */
    }
    bool = 0;
    if (itemPtr->flags & ITEM_RADIOBUTTON) {
	const char *string;

	if (itemPtr->valueObjPtr == NULL) {
	    string = itemPtr->label;
	} else {
	    string = Tcl_GetString(itemPtr->valueObjPtr);
	}
	if (string == NULL) {
	    return NULL;
	}
	bool = (strcmp(string, Tcl_GetString(valueObjPtr)) == 0);
    } else if (itemPtr->flags & ITEM_CHECKBUTTON) {
	if (itemPtr->onValueObjPtr == NULL) {
	    if (Tcl_GetBooleanFromObj(NULL, valueObjPtr, &bool) != TCL_OK) {
		return NULL;
	    }
	} else {
	    bool =  (strcmp(Tcl_GetString(valueObjPtr), 
			    Tcl_GetString(itemPtr->onValueObjPtr)) == 0);
	}
    }
    if (bool) {
       if (itemPtr->flags & ITEM_SELECTED) {
	   return NULL;	/* Already selected. */
       }
       itemPtr->flags |= ITEM_SELECTED;
    } else if (itemPtr->flags & ITEM_SELECTED) {
	itemPtr->flags &= ~ITEM_SELECTED;
    } else {
	return NULL;		/* Already deselected. */
    }
 done:
    EventuallyRedraw(itemPtr->comboPtr);
    return NULL;		/* Done. */
}

/*ARGSUSED*/
static void
FreeTraceVarProc(ClientData clientData, Display *display, char *widgRec, 
		 int offset)
{
    Item *itemPtr = (Item *)(widgRec);

    if (itemPtr->varNameObjPtr != NULL) {
	char *varName;
	ComboMenu *comboPtr;

	comboPtr = itemPtr->comboPtr;
	varName = Tcl_GetString(itemPtr->varNameObjPtr);
	Tcl_UntraceVar(comboPtr->interp, varName, VAR_FLAGS, ItemVarTraceProc, 
		itemPtr);
	Tcl_DecrRefCount(itemPtr->varNameObjPtr);
	itemPtr->varNameObjPtr = NULL;
    }
}

/*
 *---------------------------------------------------------------------------

 * ObjToTraceVar --
 *
 *	Convert the string representation of a color into a XColor pointer.
 *
 * Results:
 *	The return value is a standard TCL result.  The color pointer is
 *	written into the widget record.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToTraceVarProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representing style. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Item *itemPtr = (Item *)(widgRec);
    char *varName;

    /* Remove the current trace on the variable. */
    if (itemPtr->varNameObjPtr != NULL) {
	varName = Tcl_GetString(itemPtr->varNameObjPtr);
	Tcl_UntraceVar(interp, varName, VAR_FLAGS, ItemVarTraceProc, itemPtr);
	Tcl_DecrRefCount(itemPtr->varNameObjPtr);
	itemPtr->varNameObjPtr = NULL;
    }
    varName = Tcl_GetString(objPtr);
    if ((varName[0] == '\0') && (flags & BLT_CONFIG_NULL_OK)) {
	return TCL_OK;
    }
    itemPtr->varNameObjPtr = objPtr;
    Tcl_IncrRefCount(objPtr);
    Tcl_TraceVar(interp, varName, VAR_FLAGS, ItemVarTraceProc, itemPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TraceVarToObjProc --
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
TraceVarToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget information record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Item *itemPtr = (Item *)(widgRec);
    Tcl_Obj *objPtr;

    if (itemPtr->varNameObjPtr == NULL) {
	objPtr = Tcl_NewStringObj("", -1);
    } else {
	objPtr = itemPtr->varNameObjPtr;
    }
    return objPtr;
}

/*ARGSUSED*/
static void
FreeStyleProc(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    Style *stylePtr = *(Style **)(widgRec + offset);

    if ((stylePtr != NULL) && (stylePtr != &stylePtr->comboPtr->defStyle)) {
	DestroyStyle(stylePtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToStyleProc --
 *
 *	Convert the string representation of a color into a XColor pointer.
 *
 * Results:
 *	The return value is a standard TCL result.  The color pointer is
 *	written into the widget record.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToStyleProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representing style. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    ComboMenu *comboPtr;
    Item *itemPtr = (Item *)widgRec;
    Style **stylePtrPtr = (Style **)(widgRec + offset);
    Style *stylePtr;
    char *string;

    string = Tcl_GetString(objPtr);
    comboPtr = itemPtr->comboPtr;
    if ((string[0] == '\0') && (flags & BLT_CONFIG_NULL_OK)) {
	stylePtr = NULL;
    } else if (GetStyleFromObj(interp, comboPtr, objPtr, &stylePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Release the old style. */
    if ((*stylePtrPtr != NULL) && (*stylePtrPtr != &comboPtr->defStyle)) {
	DestroyStyle(*stylePtrPtr);
    }
    stylePtr->refCount++;
    *stylePtrPtr = stylePtr;
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * StyleToObjProc --
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
StyleToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget information record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Style *stylePtr = *(Style **)(widgRec + offset);
    Tcl_Obj *objPtr;

    if (stylePtr == NULL) {
	objPtr = Tcl_NewStringObj("", -1);
    } else {
	objPtr = Tcl_NewStringObj(stylePtr->name, -1);
    }
    return objPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToStateProc --
 *
 *	Convert the string representation of an item state into a flag.
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
    char *string;
    ComboMenu *comboPtr;
    int flag;

    string = Tcl_GetString(objPtr);
    if (strcmp(string, "active") == 0) {
	flag = ITEM_ACTIVE;
    } else if (strcmp(string, "disabled") == 0) {
	flag = ITEM_DISABLED;
    } else if (strcmp(string, "normal") == 0) {
	flag = ITEM_NORMAL;
    } else {
	Tcl_AppendResult(interp, "unknown state \"", string, 
		"\": should be active, disabled, or normal.", (char *)NULL);
	return TCL_ERROR;
    }
    if (itemPtr->flags & flag) {
	return TCL_OK;		/* State is already set to value. */
    }
    comboPtr = itemPtr->comboPtr;
    if (comboPtr->activePtr != itemPtr) {
	ActivateItem(comboPtr, NULL);
	comboPtr->activePtr = NULL;
    }
    *flagsPtr &= ~ITEM_STATE_MASK;
    *flagsPtr |= flag;
    if (flag == ITEM_ACTIVE) {
	ActivateItem(comboPtr, itemPtr);
	comboPtr->activePtr = itemPtr;
    }
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
    Tcl_Obj *objPtr;

    if (state & ITEM_NORMAL) {
	objPtr = Tcl_NewStringObj("normal", -1);
    } else if (state & ITEM_ACTIVE) {
	objPtr = Tcl_NewStringObj("active", -1);
    } else if (state & ITEM_DISABLED) {
	objPtr = Tcl_NewStringObj("disabled", -1);
    } else {
	objPtr = Tcl_NewStringObj("???", -1);
    }
    return objPtr;
}

/*ARGSUSED*/
static void
FreeTagsProc(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    ComboMenu *comboPtr;
    Item *itemPtr = (Item *)widgRec;

    comboPtr = itemPtr->comboPtr;
    ReleaseTags(comboPtr, itemPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToTagsProc --
 *
 *	Convert the string representation of a list of tags.
 *
 * Results:
 *	The return value is a standard TCL result.  The tags are
 *	save in the widget.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToTagsProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representing style. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    ComboMenu *comboPtr;
    Item *itemPtr = (Item *)widgRec;
    int i;
    char *string;
    int objc;
    Tcl_Obj **objv;

    comboPtr = itemPtr->comboPtr;
    ReleaseTags(comboPtr, itemPtr);
    string = Tcl_GetString(objPtr);
    if ((string[0] == '\0') && (flags & BLT_CONFIG_NULL_OK)) {
	return TCL_OK;
    }
    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 0; i < objc; i++) {
	SetTag(interp, itemPtr, Tcl_GetString(objv[i]));
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagsToObjProc --
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
TagsToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget information record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    ComboMenu *comboPtr;
    Item *itemPtr = (Item *)widgRec;
    Tcl_Obj *listObjPtr;

    comboPtr = itemPtr->comboPtr;
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (hPtr = Blt_FirstHashEntry(&comboPtr->tagTable, &iter); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&iter)) {
	Blt_HashTable *tagTablePtr;
	Blt_HashEntry *h2Ptr;

	tagTablePtr = Blt_GetHashValue(hPtr); 
	h2Ptr = Blt_FindHashEntry(tagTablePtr, (char *)itemPtr->index);
	if (h2Ptr != NULL) {
	    Tcl_Obj *objPtr;
	    const char *name;

	    name = Tcl_GetHashKey(&comboPtr->tagTable, hPtr);
	    objPtr = Tcl_NewStringObj(name, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    return listObjPtr;
}

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
    ComboMenu *comboPtr = clientData;

    comboPtr->flags |= (CM_LAYOUT | CM_SCROLL);
    EventuallyRedraw(comboPtr);
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
    ComboMenu *comboPtr = clientData;
    Icon *iconPtr = (Icon *)(widgRec + offset);
    Icon icon;

    if (GetIconFromObj(interp, comboPtr, objPtr, &icon) != TCL_OK) {
	return TCL_ERROR;
    }
    if (*iconPtr != NULL) {
	FreeIcon(comboPtr, *iconPtr);
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
	objPtr =Tcl_NewStringObj(Blt_NameOfImage(IconImage(icon)), -1);
    }
    return objPtr;
}

/*ARGSUSED*/
static void
FreeIconProc(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    Icon *iconPtr = (Icon *)(widgRec + offset);

    if (*iconPtr != NULL) {
	ComboMenu *comboPtr = clientData;

	FreeIcon(comboPtr, *iconPtr);
	*iconPtr = NULL;
    }
}

/*ARGSUSED*/
static void
FreeLabelProc(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    Item *itemPtr = (Item *)widgRec;

    if (itemPtr->label != emptyString) {
	RemoveLabel(itemPtr->comboPtr, itemPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToLabelProc --
 *
 *	Save the label and add the item to the label hashtable.
 *
 * Results:
 *	A standard TCL result. 
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToLabelProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representing style. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Item *itemPtr = (Item *)(widgRec);
    char *string;

    if (itemPtr->label != emptyString) {
	RemoveLabel(itemPtr->comboPtr, itemPtr);
    }
    string = Tcl_GetString(objPtr);
    if ((string[0] == '\0') && (flags & BLT_CONFIG_NULL_OK)) {
	return TCL_OK;
    }
    itemPtr->label = NewLabel(itemPtr, string);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * LabelToObjProc --
 *
 *	Return the label of the item.
 *
 * Results:
 *	The label is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
LabelToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget information record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    Item *itemPtr = (Item *)(widgRec);
    Tcl_Obj *objPtr;

    if (itemPtr->label == emptyString) {
	objPtr = Blt_EmptyStringObj();
    } else {
	objPtr = Tcl_NewStringObj(itemPtr->label, -1);
    }
    return objPtr;
}


/*
 *---------------------------------------------------------------------------
 *
 * ObjToTypeProc --
 *
 *	Convert the string representation of an item into a value.
 *
 * Results:
 *	A standard TCL result.  The type pointer is written into the
 *	widget record.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToTypeProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* String representing type. */
    char *widgRec,		/* Widget record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    unsigned int *typePtr = (unsigned int *)(widgRec + offset);
    int flag;

    if (GetTypeFromObj(interp, objPtr, &flag) != TCL_OK) {
	return TCL_ERROR;
    }
    *typePtr &= ~ITEM_TYPE_MASK;
    *typePtr |= flag;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TypeToObjProc --
 *
 *	Return the name of the type.
 *
 * Results:
 *	The name representing the style is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
TypeToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Widget information record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    int type = *(int *)(widgRec + offset);
    
    return Tcl_NewStringObj(NameOfType(type), -1);
}

/* Widget Operations */

/*
 *---------------------------------------------------------------------------
 *
 * ActivateOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *	.cm activate item
 *
 *---------------------------------------------------------------------------
 */
static int
ActivateOp(ComboMenu *comboPtr, Tcl_Interp *interp, int objc, 
	   Tcl_Obj *const *objv)
{
    Item *itemPtr;

    if (GetItemFromObj(NULL, comboPtr, objv[2], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    } 
    if (comboPtr->activePtr == itemPtr) {
	return TCL_OK;		/* Item is already active. */
    }
    ActivateItem(comboPtr, NULL);
    comboPtr->activePtr = NULL;
    if ((itemPtr != NULL) && 
	((itemPtr->flags & (ITEM_DISABLED|ITEM_SEPARATOR)) == 0)) {
	ActivateItem(comboPtr, itemPtr);
	comboPtr->activePtr = itemPtr;
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * AddOp --
 *
 *	Appends a new item to the combomenu.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The combomenu entry may become selected or deselected.
 *
 *   .cm add radiobutton -text "fred" -tags ""
 *
 *---------------------------------------------------------------------------
 */
static int
AddOp(ComboMenu *comboPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Item *itemPtr;

    itemPtr = NewItem(comboPtr);
    if (ConfigureItem(interp, itemPtr, objc - 2, objv + 2, 0) != TCL_OK) {
	DestroyItem(itemPtr);
	return TCL_ERROR;	/* Error configuring the entry. */
    }
    EventuallyRedraw(comboPtr);
    Tcl_SetLongObj(Tcl_GetObjResult(interp), itemPtr->index);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * AddListOp --
 *
 *	Appends a list of items to the combomenu.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	New items are added to the combomenu.
 *
 *   .cm add labelList -type radiobutton -text "fred" -tags ""
 *
 *---------------------------------------------------------------------------
 */
static int
AddListOp(ComboMenu *comboPtr, Tcl_Interp *interp, int objc, 
	  Tcl_Obj *const *objv)
{
    int i;
    int iobjc;
    Tcl_Obj **iobjv;
    Tcl_Obj *listObjPtr;

    if (Tcl_ListObjGetElements(interp, objv[2], &iobjc, &iobjv) != TCL_OK) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (i = 0; i < iobjc; i++) {
	Tcl_Obj *objPtr;
	Item *itemPtr;

	itemPtr = NewItem(comboPtr);
	if (ConfigureItem(interp, itemPtr, objc - 3, objv + 3, 0) != TCL_OK) {
	    DestroyItem(itemPtr);
	    return TCL_ERROR;	
	}
	itemPtr->label = NewLabel(itemPtr, Tcl_GetString(iobjv[i]));
	objPtr = Tcl_NewLongObj(itemPtr->index);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    EventuallyRedraw(comboPtr);
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
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
 *	.cm configure ?option value?...
 *
 *---------------------------------------------------------------------------
 */
static int
ConfigureOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    int result;

    iconOption.clientData = comboPtr;
    if (objc == 2) {
	return Blt_ConfigureInfoFromObj(interp, comboPtr->tkwin, 
		comboConfigSpecs, (char *)comboPtr, (Tcl_Obj *)NULL,  0);
    } else if (objc == 3) {
	return Blt_ConfigureInfoFromObj(interp, comboPtr->tkwin, 
		comboConfigSpecs, (char *)comboPtr, objv[2], 0);
    }
    Tcl_Preserve(comboPtr);
    result = ConfigureComboMenu(interp, comboPtr, objc - 2, objv + 2, 
		BLT_CONFIG_OBJV_ONLY);
    Tcl_Release(comboPtr);
    if (result == TCL_ERROR) {
	return TCL_ERROR;
    }
    comboPtr->flags |= (CM_LAYOUT | CM_SCROLL);
    EventuallyRedraw(comboPtr);
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
 *	.cm cget option
 *
 *---------------------------------------------------------------------------
 */
static int
CgetOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    iconOption.clientData = comboPtr;
    return Blt_ConfigureValueFromObj(interp, comboPtr->tkwin, comboConfigSpecs,
	(char *)comboPtr, objv[2], 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * DeleteOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *	.cm delete item...
 *
 *---------------------------------------------------------------------------
 */
static int
DeleteOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    int i;

    for (i = 2; i < objc; i++) {
	ItemIterator iter;
	Item *itemPtr, *nextPtr;

	if (GetItemIterator(interp, comboPtr, objv[i], &iter) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (itemPtr = FirstTaggedItem(&iter); itemPtr != NULL; 
	     itemPtr = nextPtr) {
	    nextPtr = NextTaggedItem(&iter);
	    DestroyItem(itemPtr);
	}
    }
    EventuallyRedraw(comboPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * FindOp --
 *
 *	Search for an item according to the string given.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The combomenu entry may become selected or deselected.
 *
 *   .cm find next|previous|underline pattern
 *
 *---------------------------------------------------------------------------
 */
static int
FindOp(ComboMenu *comboPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    const char *how, *pattern;
    long index;

    index = -1;
    how = Tcl_GetString(objv[2]);
    pattern = Tcl_GetString(objv[3]);
    if (strcmp(how, "underline") == 0) {
	Tcl_UniChar want;
	Item *itemPtr;
	
	itemPtr = comboPtr->activePtr;
	itemPtr = (itemPtr == NULL) ? FirstItem(comboPtr) : NextItem(itemPtr);
	want = Tcl_UniCharAtIndex(pattern, 0);
	want = Tcl_UniCharToLower(want);
	for (/*empty*/; itemPtr != NULL; itemPtr = NextItem(itemPtr)) {
	    if (itemPtr->underline >= 0) {
		Tcl_UniChar have;
		
		have = Tcl_UniCharAtIndex(itemPtr->label, itemPtr->underline);
		have = Tcl_UniCharToLower(have);
		if (want == have) {
		    index = itemPtr->index;
		    break;
		}
	    }
	}
	if (itemPtr == NULL) {
	    for (itemPtr = FirstItem(comboPtr); itemPtr != NULL; 
		 itemPtr = NextItem(itemPtr)) {
		if (itemPtr->underline >= 0) {
		    Tcl_UniChar have;
		    
		    have = Tcl_UniCharAtIndex(itemPtr->label, 
					      itemPtr->underline);
		    have = Tcl_UniCharToLower(have);
		    if (want == have) {
			index = itemPtr->index;
			break;
		    }
		}
		if (itemPtr == comboPtr->activePtr) {
		    break;
		}
	    }
	}
    } else if (strcmp(how, "next") == 0) {
	Item *itemPtr;
	
	itemPtr = comboPtr->activePtr;
	itemPtr = (itemPtr == NULL) ? FirstItem(comboPtr) : NextItem(itemPtr);
	for (/*empty*/; itemPtr != NULL; itemPtr = NextItem(itemPtr)) {
	    if (Tcl_StringMatch(itemPtr->label, pattern)) {
		index = itemPtr->index;
		break;
	    }
	}
    } else if (strcmp(how, "previous") == 0) {
	Item *itemPtr;
	
	itemPtr = comboPtr->activePtr;
	itemPtr = (itemPtr == NULL) ? LastItem(comboPtr) : PrevItem(itemPtr);
	for (/*empty*/; itemPtr != NULL; itemPtr = PrevItem(itemPtr)) {
	    if (Tcl_StringMatch(itemPtr->label, pattern)) {
		index = itemPtr->index;
		break;
	    }
	}
    } else {
	Tcl_AppendResult(interp, "unknown directive \"", how, "\": ",
			 "should be next, last, or underline.", (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_SetLongObj(Tcl_GetObjResult(interp), index);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * IndexOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *	.cm index item 
 *
 *---------------------------------------------------------------------------
 */
static int
IndexOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    Item *itemPtr;
    int index;

    index = -1;
    if (GetItemFromObj(NULL, comboPtr, objv[2], &itemPtr) == TCL_OK) {
	if (itemPtr != NULL) {
	    index = itemPtr->index;
	}
    }
    Tcl_SetLongObj(Tcl_GetObjResult(interp), index);
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
 *  .cm invoke item 
 *
 *---------------------------------------------------------------------------
 */
static int
InvokeOp(ComboMenu *comboPtr, Tcl_Interp *interp, int objc, 
	 Tcl_Obj *const *objv)
{
    int result;
    Item *itemPtr;

    if (GetItemFromObj(interp, comboPtr, objv[2], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((itemPtr == NULL) || (itemPtr->flags & ITEM_DISABLED)) {
	return TCL_OK;		/* Item is currently disabled. */
    }
    result = TCL_OK;
    Tcl_Preserve((ClientData)itemPtr);
    if ((itemPtr->flags & (ITEM_CASCADE|ITEM_SEPARATOR)) == 0) {
	if ((comboPtr->iconVarObjPtr != NULL) && (itemPtr->icon != NULL)) {
	    Tcl_Obj *objPtr;

	    objPtr = Tcl_NewStringObj(IconName(itemPtr->icon), -1);
	    if (Tcl_ObjSetVar2(interp, comboPtr->iconVarObjPtr, NULL, objPtr, 
			       TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		return TCL_ERROR;
	    }
	}
	if ((comboPtr->textVarObjPtr != NULL) && 
	    (itemPtr->label != emptyString)) {
	    Tcl_Obj *objPtr;

	    objPtr = Tcl_NewStringObj(itemPtr->label, -1);
	    if (Tcl_ObjSetVar2(interp, comboPtr->textVarObjPtr, NULL, objPtr, 
			       TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		return TCL_ERROR;
	    }
	}
    }
    if ((itemPtr->varNameObjPtr != NULL) &&
	(itemPtr->flags & (ITEM_CHECKBUTTON|ITEM_RADIOBUTTON|ITEM_BUTTON))) {
	Tcl_Obj *valueObjPtr;

	valueObjPtr = NULL;
	if (itemPtr->flags & ITEM_CHECKBUTTON) {
	    valueObjPtr = (itemPtr->flags & ITEM_SELECTED) ?
		itemPtr->offValueObjPtr : itemPtr->onValueObjPtr;
	} else {
	    valueObjPtr = itemPtr->valueObjPtr;
	    if (valueObjPtr == NULL) {
		valueObjPtr = Tcl_NewStringObj(itemPtr->label, -1);
	    }
	}
	if (valueObjPtr != NULL) {
	    Tcl_IncrRefCount(valueObjPtr);
	    if (Tcl_ObjSetVar2(interp, itemPtr->varNameObjPtr, NULL, 
		valueObjPtr, TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    }
	    Tcl_DecrRefCount(valueObjPtr);
	}
    }
    /*
     * We check nItems in addition to whether the item has a command because
     * that goes to zero if the combomenu is deleted (e.g., during command
     * evaluation).
     */
    if ((Blt_Chain_GetLength(comboPtr->chain) > 0) && (result == TCL_OK) && 
	(itemPtr->cmdObjPtr != NULL)) {
	Tcl_IncrRefCount(itemPtr->cmdObjPtr);
	result = Tcl_EvalObjEx(interp, itemPtr->cmdObjPtr, TCL_EVAL_GLOBAL);
	Tcl_DecrRefCount(itemPtr->cmdObjPtr);
    }
    Tcl_Release((ClientData)itemPtr);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * InsertOp --
 *
 *	Inserts a new item into the combomenu at the given index.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The combomenu gets a new item.
 *
 *   .cm insert before 0 after 1 -text label 
 *
 *---------------------------------------------------------------------------
 */
static int
InsertOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    Item *itemPtr, *wherePtr;
    int dir;
    static const char *dirs[] = { "after", "at", "before" , NULL};

    if (Tcl_GetIndexFromObj(interp, objv[2], dirs, "key", 0, &dir) != TCL_OK) {
	return TCL_ERROR;
    }
    if (GetItemFromObj(interp, comboPtr, objv[3], &wherePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (wherePtr == NULL) {
	Tcl_AppendResult(interp, "can't insert item: no index \"", 
			 Tcl_GetString(objv[3]), "\"", (char *)NULL);
    	return TCL_ERROR;	/* No item. */
    }
    itemPtr = NewItem(comboPtr);
    if (ConfigureItem(interp, itemPtr, objc - 4, objv + 4, 0) != TCL_OK) {
	DestroyItem(itemPtr);
	return TCL_ERROR;	/* Error configuring the entry. */
    }
    MoveItem(comboPtr, itemPtr, dir, wherePtr);
    EventuallyRedraw(comboPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ItemConfigureOp --
 *
 *	This procedure handles item operations.
 *
 * Results:
 *	A standard TCL result.
 *
 *	.cm item configure item ?option value?...
 *
 *---------------------------------------------------------------------------
 */
static int
ItemConfigureOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    Item *itemPtr;
    ItemIterator iter;

    if (GetItemIterator(interp, comboPtr, objv[3], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    iconOption.clientData = comboPtr;
    for (itemPtr = FirstTaggedItem(&iter); itemPtr != NULL; 
	 itemPtr = NextTaggedItem(&iter)) {
	int result;
	unsigned int flags;

	flags = BLT_CONFIG_OBJV_ONLY;
	if (objc == 4) {
	    return Blt_ConfigureInfoFromObj(interp, comboPtr->tkwin, 
		itemConfigSpecs, (char *)itemPtr, (Tcl_Obj *)NULL, flags);
	} else if (objc == 5) {
	    return Blt_ConfigureInfoFromObj(interp, comboPtr->tkwin, 
		itemConfigSpecs, (char *)itemPtr, objv[4], flags);
	}
	Tcl_Preserve(itemPtr);
	result = ConfigureItem(interp, itemPtr, objc - 4, objv + 4,  flags);
	Tcl_Release(itemPtr);
	if (result == TCL_ERROR) {
	    return TCL_ERROR;
	}
    }
    comboPtr->flags |= (CM_LAYOUT | CM_SCROLL);
    EventuallyRedraw(comboPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ItemCgetOp --
 *
 *	This procedure handles item operations.
 *
 * Results:
 *	A standard TCL result.
 *
 *	.cm item cget item option
 *
 *---------------------------------------------------------------------------
 */
static int
ItemCgetOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    Item *itemPtr;

    if (GetItemFromObj(interp, comboPtr, objv[3], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == NULL) {
	Tcl_AppendResult(interp, "can't retrieve item \"", 
			 Tcl_GetString(objv[3]), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    iconOption.clientData = comboPtr;
    return Blt_ConfigureValueFromObj(interp, comboPtr->tkwin, itemConfigSpecs,
	(char *)itemPtr, objv[4], 0);
}

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
static Blt_OpSpec itemOps[] = {
    {"cget",      2, ItemCgetOp,      5, 5, "item option",},
    {"configure", 2, ItemConfigureOp, 4, 0, "item ?option value?...",},
};
    
static int nItemOps = sizeof(itemOps) / sizeof(Blt_OpSpec);

static int
ItemOp(ComboMenu *comboPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    ComboMenuCmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nItemOps, itemOps, BLT_OP_ARG2, objc, objv, 
		0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (comboPtr, interp, objc, objv);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * NamesOp --
 *
 *	.cm names pattern...
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
NamesOp(ComboMenu *comboPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_Obj *listObjPtr;
    int i;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (i = 2; i < objc; i++) {
	const char *pattern;
	Blt_ChainLink link;

	pattern = Tcl_GetString(objv[i]);
	for (link = Blt_Chain_FirstLink(comboPtr->chain); link != NULL; 
	     link = Blt_Chain_NextLink(link)) {
	    Item *itemPtr;
	    Tcl_Obj *objPtr;

	    itemPtr = Blt_Chain_GetValue(link);
	    if (Tcl_StringMatch(itemPtr->label, pattern)) {
		if (itemPtr->label == emptyString) {
		    objPtr = Blt_EmptyStringObj();
		} else {
		    objPtr = Tcl_NewStringObj(itemPtr->label, -1);
		}
		Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    }
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
NearestOp(
    ComboMenu *comboPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *const *objv)
{
    int x, y;			/* Screen coordinates of the test point. */
    int wx, wy;			/* Screen coordinates of the test point. */
    Item *itemPtr;
    int isRoot;
    char *string;

    isRoot = FALSE;
    string = Tcl_GetString(objv[2]);
    if (strcmp("-root", string) == 0) {
	isRoot = TRUE;
	objv++, objc--;
    } 
    if (objc < 4) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", 
		Tcl_GetString(objv[0]), " ", Tcl_GetString(objv[1]), 
		" ?-root? x y\"", (char *)NULL);
	return TCL_ERROR;
			 
    }
    if ((Tk_GetPixelsFromObj(interp, comboPtr->tkwin, objv[2], &x) != TCL_OK) ||
	(Tk_GetPixelsFromObj(interp, comboPtr->tkwin, objv[3], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (isRoot) {
	int rootX, rootY;

	Tk_GetRootCoords(comboPtr->tkwin, &rootX, &rootY);
	x -= rootX;
	y -= rootY;
    }
    itemPtr = NearestItem(comboPtr, x, y, TRUE);
    if (itemPtr == NULL) {
	return TCL_OK;
    }
    x = WORLDX(comboPtr, x);
    y = WORLDY(comboPtr, y);
    wx = itemPtr->xWorld + ITEM_XPAD;
    wy = itemPtr->xWorld + ITEM_XPAD;
    if (objc > 4) {
	const char *where;

	where = "";
	if (itemPtr->flags & (ITEM_RADIOBUTTON | ITEM_CHECKBUTTON)) {
	    int bx, by, bw, bh;

	    bx = wx;
	    by = wy;
	    if (itemPtr->flags & ITEM_RADIOBUTTON) {
		bw = IconWidth(comboPtr->rbIcon);
		bh = IconHeight(comboPtr->rbIcon);
	    } else {
		bw = IconWidth(comboPtr->cbIcon);
		bh = IconHeight(comboPtr->cbIcon);
	    }
	    wx += comboPtr->leftIndWidth + ITEM_IPAD;
	    if ((x >= bx) && (x < (bx + bw)) && (y >= by) && (y < (by + bh))) {
		if (itemPtr->flags & ITEM_RADIOBUTTON) {
		    where = "radiobutton";
		} else {
		    where = "checkbutton";
		}		    
		goto done;
	    }
	} 
	if (itemPtr->icon != NULL) {
	    int ix, iy, iw, ih;
	    
	    ih = IconHeight(itemPtr->icon);
	    iw = IconWidth(itemPtr->icon);
	    ix = wx;
	    iy = wy;
	    wx += comboPtr->iconWidth + ITEM_IPAD;
	    if ((x >= ix) && (x <= (ix + iw)) && (y >= iy) && (y < (iy + ih))) {
		where = "icon";
		goto done;
	    }
	}
	if ((itemPtr->label != emptyString) || (itemPtr->image != NULL)) {
	    int lx, ly;

	    lx = wx;
	    ly = wy;

	    wx += comboPtr->labelWidth + ITEM_IPAD;
	    if ((x >= lx) && (x < (lx + itemPtr->labelWidth)) &&
		(y >= ly) && (y < (ly + itemPtr->labelHeight))) {
		where = "label";
		goto done;
	    }
	}
	if ((itemPtr->accel != NULL) || (itemPtr->flags & ITEM_CASCADE)) {
	    int ax, ay, aw, ah;

	    ax = wx;
	    ay = wy;
	    aw = itemPtr->rightIndWidth;
	    ah = itemPtr->rightIndHeight;
	    if ((x >= ax) && (x < (ax + aw)) && (y >= ay) && (y < (ay + ah))) {
		if (itemPtr->flags & ITEM_CASCADE) {
		    where = "cascade";
		} else {
		    where = "accelerator";
		}
		goto done;
	    }

	}
    done:
	if (Tcl_SetVar(interp, Tcl_GetString(objv[4]), where, 
		TCL_LEAVE_ERR_MSG) == NULL) {
	    return TCL_ERROR;
	}
    }
    Tcl_SetLongObj(Tcl_GetObjResult(interp), itemPtr->index);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * NextOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *	.cm next item 
 *
 *---------------------------------------------------------------------------
 */
static int
NextOp(ComboMenu *comboPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Item *itemPtr;
    int index;

    index = -1;
    if (GetItemFromObj(NULL, comboPtr, objv[2], &itemPtr) == TCL_OK) {
	itemPtr = NextItem(itemPtr);
	if (itemPtr != NULL) {
	    index = itemPtr->index;
	}
    }
    Tcl_SetLongObj(Tcl_GetObjResult(interp), index);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * PostOp --
 *
 *	Posts this menu at the given root screen coordinates.
 *
 *  .cm post x y
 *
 *---------------------------------------------------------------------------
 */
static int
PostOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    int x, y;

    if (objc == 4) {
	if ((Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK) || 
	    (Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK)) {
	    return TCL_ERROR;
	} 
    } else if (objc == 2) {
	if (comboPtr->flags & CM_LAYOUT) {
	    ComputeMenuGeometry(comboPtr);
	}
	ComputeMenuCoords(comboPtr, TK_ANCHOR_SE, &x, &y);
    } else {
	Tcl_AppendResult(interp, "wrong # of args: should be \"", 
		Tcl_GetString(objv[0]), " post ?x y?\"", (char *)NULL);
	return TCL_ERROR;
    }
#ifdef notdef
    if (Tk_IsMapped(comboPtr->tkwin)) {
	return TCL_OK;		/* This menu is already posted. */
    }
#endif
    if (comboPtr->flags & CM_LAYOUT) {
	ComputeMenuGeometry(comboPtr);
    }

    /*
     * If there is a post command for the menu, execute it.  This may change
     * the size of the menu, so be sure to recompute the menu's geometry if
     * needed.
     */
    if (comboPtr->postCmdObjPtr != NULL) {
	int result;

	Tcl_IncrRefCount(comboPtr->postCmdObjPtr);
	result = Tcl_EvalObjEx(interp, comboPtr->postCmdObjPtr,TCL_EVAL_GLOBAL);
	Tcl_DecrRefCount(comboPtr->postCmdObjPtr);
	if (result != TCL_OK) {
	    return result;
	}
	/*
	 * The post commands could have deleted the menu, which means we are
	 * dead and should go away.
	 */
	if (comboPtr->tkwin == NULL) {
	    return TCL_OK;
	}
	if (comboPtr->flags & CM_LAYOUT) {
	    ComputeMenuGeometry(comboPtr);
	}
    }

    /*
     * Adjust the position of the menu if necessary to keep it visible on the
     * screen.  There are two special tricks to make this work right:
     *
     * 1. If a virtual root window manager is being used then
     *    the coordinates are in the virtual root window of
     *    menuPtr's parent;  since the menu uses override-redirect
     *    mode it will be in the *real* root window for the screen,
     *    so we have to map the coordinates from the virtual root
     *    (if any) to the real root.  Can't get the virtual root
     *    from the menu itself (it will never be seen by the wm)
     *    so use its parent instead (it would be better to have an
     *    an option that names a window to use for this...).
     * 2. The menu may not have been mapped yet, so its current size
     *    might be the default 1x1.  To compute how much space it
     *    needs, use its requested size, not its actual size.
     *
     * Note that this code assumes square screen regions and all positive
     * coordinates. This does not work on a Mac with multiple monitors. But
     * then again, Tk has other problems with this.
     */
    {
	int vx, vy, vw, vh;
	int tmp;
	Screen *screenPtr;

	Tk_GetVRootGeometry(Tk_Parent(comboPtr->tkwin), &vx, &vy, &vw, &vh);
	x += vx;
	y += vy;
	screenPtr = Tk_Screen(comboPtr->tkwin);
	tmp = WidthOfScreen(screenPtr) - Tk_ReqWidth(comboPtr->tkwin);
	if (x > tmp) {
	    x = tmp;
	}
	if (x < 0) {
	    x = 0;
	}
	tmp = HeightOfScreen(screenPtr) - Tk_ReqHeight(comboPtr->tkwin);
	if (y > tmp) {
	    y = tmp;
	}
	if (y < 0) {
	    y = 0;
	}
	Tk_MoveToplevelWindow(comboPtr->tkwin, x, y);
	if (!Tk_IsMapped(comboPtr->tkwin)) {
	    Tk_MapWindow(comboPtr->tkwin);
	}
	Blt_MapToplevelWindow(comboPtr->tkwin);
	Blt_RaiseToplevelWindow(comboPtr->tkwin);
#ifdef notdef
	TkWmRestackToplevel(comboPtr->tkwin, Above, NULL);
#endif
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * PostCascadeOp --
 *
 *	Posts the menu of a cascade item.  If the item is a cascade menu, then
 *	the submenu is requested to be posted.
 *
 * Results: 
 *	A standard TCL result.
 *
 * Side effects:  
 *	The item's submenu may be posted.
 *
 *  .cm postcascade item
 *
 *---------------------------------------------------------------------------
 */
static int
PostCascadeOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    Item *itemPtr;
    char *string;

    string = Tcl_GetString(objv[2]);
    if (GetItemFromObj(interp, comboPtr, objv[2], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == comboPtr->postedPtr) {
	return TCL_OK;		/* Nothing to do, submenu is already posted. */
    }
    if (UnpostCascade(interp, comboPtr) != TCL_OK) {
	return TCL_ERROR;	/* Error unposting submenu. */
    }
    if ((itemPtr != NULL) && (itemPtr->menuObjPtr != NULL) && 
	((itemPtr->flags & (ITEM_CASCADE|ITEM_DISABLED)) == ITEM_CASCADE)) {
	return PostCascade(interp, comboPtr, itemPtr);
    }
    return TCL_OK;		/* No menu to post. */
}

/*
 *---------------------------------------------------------------------------
 *
 * PreviousOp --
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	Commands may get excecuted; variables may get set; sub-menus may
 *	get posted.
 *
 *	.cm previous item 
 *
 *---------------------------------------------------------------------------
 */
static int
PreviousOp(ComboMenu *comboPtr, Tcl_Interp *interp, int objc, 
	   Tcl_Obj *const *objv)
{
    Item *itemPtr;
    int index;

    index = -1;
    if (GetItemFromObj(NULL, comboPtr, objv[2], &itemPtr) == TCL_OK) {
	itemPtr = PrevItem(itemPtr);
	if (itemPtr != NULL) {
	    index = itemPtr->index;
	}
    }
    Tcl_SetLongObj(Tcl_GetObjResult(interp), index);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ScanOp --
 *
 *	Implements the quick scan.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ScanOp(
    ComboMenu *comboPtr,
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *const *objv)
{
    int oper;
    int x, y;

#define SCAN_MARK	1
#define SCAN_DRAGTO	2
    {
	char *string;
	char c;
	int length;
	
	string = Tcl_GetStringFromObj(objv[2], &length);
	c = string[0];
	if ((c == 'm') && (strncmp(string, "mark", length) == 0)) {
	    oper = SCAN_MARK;
	} else if ((c == 'd') && (strncmp(string, "dragto", length) == 0)) {
	    oper = SCAN_DRAGTO;
	} else {
	    Tcl_AppendResult(interp, "bad scan operation \"", string,
		"\": should be either \"mark\" or \"dragto\"", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    if ((Blt_GetPixelsFromObj(interp, comboPtr->tkwin, objv[3], PIXELS_ANY, &x) 
	 != TCL_OK) ||
	(Blt_GetPixelsFromObj(interp, comboPtr->tkwin, objv[4], PIXELS_ANY, &y) 
	 != TCL_OK)) {
	return TCL_ERROR;
    }
    if (oper == SCAN_MARK) {
	comboPtr->scanAnchorX = x;
	comboPtr->scanAnchorY = y;
	comboPtr->scanX = comboPtr->xOffset;
	comboPtr->scanY = comboPtr->yOffset;
    } else {
	int xWorld, yWorld;
	int viewWidth, viewHeight;
	int dx, dy;

	dx = comboPtr->scanAnchorX - x;
	dy = comboPtr->scanAnchorY - y;
	xWorld = comboPtr->scanX + (10 * dx);
	yWorld = comboPtr->scanY + (10 * dy);

	viewWidth = VPORTWIDTH(comboPtr);
	if (xWorld > (comboPtr->worldWidth - viewWidth)) {
	    xWorld = comboPtr->worldWidth - viewWidth;
	}
	if (xWorld < 0) {
	    xWorld = 0;
	}
	viewHeight = VPORTHEIGHT(comboPtr);
	if (yWorld > (comboPtr->worldHeight - viewHeight)) {
	    yWorld = comboPtr->worldHeight - viewHeight;
	}
	if (yWorld < 0) {
	    yWorld = 0;
	}
	comboPtr->xOffset = xWorld;
	comboPtr->yOffset = yWorld;
	comboPtr->flags |= CM_SCROLL;
	EventuallyRedraw(comboPtr);
    }
    return TCL_OK;
}


/*ARGSUSED*/
static int
SeeOp(
    ComboMenu *comboPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Item *itemPtr;
    int x, y, w, h;
    Tk_Anchor anchor;
    int left, right, top, bottom;
    char *string;

    string = Tcl_GetString(objv[2]);
    anchor = TK_ANCHOR_W;	/* Default anchor is West */
    if ((string[0] == '-') && (strcmp(string, "-anchor") == 0)) {
	if (objc == 3) {
	    Tcl_AppendResult(interp, "missing \"-anchor\" argument",
		(char *)NULL);
	    return TCL_ERROR;
	}
	if (Tk_GetAnchorFromObj(interp, objv[3], &anchor) != TCL_OK) {
	    return TCL_ERROR;
	}
	objc -= 2, objv += 2;
    }
    if (objc == 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", objv[0],
	    "see ?-anchor anchor? item\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (GetItemFromObj(interp, comboPtr, objv[2], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == NULL) {
	return TCL_OK;
    }

    w = VPORTWIDTH(comboPtr);
    h = VPORTHEIGHT(comboPtr);

    /*
     * XVIEW:	If the entry is left or right of the current view, adjust
     *		the offset.  If the entry is nearby, adjust the view just
     *		a bit.  Otherwise, center the entry.
     */
    left = comboPtr->xOffset;
    right = comboPtr->xOffset + w;

    switch (anchor) {
    case TK_ANCHOR_W:
    case TK_ANCHOR_NW:
    case TK_ANCHOR_SW:
	x = 0;
	break;
    case TK_ANCHOR_E:
    case TK_ANCHOR_NE:
    case TK_ANCHOR_SE:
	x = itemPtr->xWorld + itemPtr->width - w;
	break;
    default:
	if (itemPtr->xWorld < left) {
	    x = itemPtr->xWorld;
	} else if ((itemPtr->xWorld + itemPtr->width) > right) {
	    x = itemPtr->xWorld + itemPtr->width - w;
	} else {
	    x = comboPtr->xOffset;
	}
	break;
    }

    /*
     * YVIEW:	If the entry is above or below the current view, adjust
     *		the offset.  If the entry is nearby, adjust the view just
     *		a bit.  Otherwise, center the entry.
     */
    top = comboPtr->yOffset;
    bottom = comboPtr->yOffset + h;

    switch (anchor) {
    case TK_ANCHOR_N:
	y = comboPtr->yOffset;
	break;
    case TK_ANCHOR_NE:
    case TK_ANCHOR_NW:
	y = itemPtr->yWorld - (h / 2);
	break;
    case TK_ANCHOR_S:
    case TK_ANCHOR_SE:
    case TK_ANCHOR_SW:
	y = itemPtr->yWorld + itemPtr->height - h;
	break;
    default:
	if (itemPtr->yWorld < top) {
	    y = itemPtr->yWorld;
	} else if ((itemPtr->yWorld + itemPtr->height) > bottom) {
	    y = itemPtr->yWorld + itemPtr->height - h;
	} else {
	    y = comboPtr->yOffset;
	}
	break;
    }
    if ((y != comboPtr->yOffset) || (x != comboPtr->xOffset)) {
	comboPtr->xOffset = x;
	comboPtr->yOffset = y;
	comboPtr->flags |= CM_SCROLL;
    }
    EventuallyRedraw(comboPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
SizeOp(ComboMenu *comboPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_SetLongObj(Tcl_GetObjResult(interp), 
		   Blt_Chain_GetLength(comboPtr->chain));
    return TCL_OK;
}

/* .m style create name option value option value */
    
static int
StyleCreateOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    Style *stylePtr;
    Blt_HashEntry *hPtr;
    int isNew;

    hPtr = Blt_CreateHashEntry(&comboPtr->styleTable, Tcl_GetString(objv[3]),
		&isNew);
    if (!isNew) {
	Tcl_AppendResult(interp, "combomenu style \"", Tcl_GetString(objv[3]),
		"\" already exists.", (char *)NULL);
	return TCL_ERROR;
    }
    stylePtr = Blt_AssertCalloc(1, sizeof(Style));
    stylePtr->name = Blt_GetHashKey(&comboPtr->styleTable, hPtr);
    stylePtr->hPtr = hPtr;
    stylePtr->comboPtr = comboPtr;
    stylePtr->borderWidth = 2;
    stylePtr->activeRelief = TK_RELIEF_RAISED;
    Blt_SetHashValue(hPtr, stylePtr);
    iconOption.clientData = comboPtr;
    if (ConfigureStyle(interp, stylePtr, objc - 4, objv + 4, 0) != TCL_OK) {
	DestroyStyle(stylePtr);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
StyleCgetOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    Style *stylePtr;

    if (GetStyleFromObj(interp, comboPtr, objv[3], &stylePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    iconOption.clientData = comboPtr;
    return Blt_ConfigureValueFromObj(interp, comboPtr->tkwin, styleConfigSpecs,
	(char *)stylePtr, objv[4], 0);
}

static int
StyleConfigureOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    int result, flags;
    Style *stylePtr;

    if (GetStyleFromObj(interp, comboPtr, objv[3], &stylePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    iconOption.clientData = comboPtr;
    flags = BLT_CONFIG_OBJV_ONLY;
    if (objc == 1) {
	return Blt_ConfigureInfoFromObj(interp, comboPtr->tkwin, 
		styleConfigSpecs, (char *)stylePtr, (Tcl_Obj *)NULL, flags);
    } else if (objc == 2) {
	return Blt_ConfigureInfoFromObj(interp, comboPtr->tkwin, 
		styleConfigSpecs, (char *)stylePtr, objv[2], flags);
    }
    Tcl_Preserve(stylePtr);
    result = ConfigureStyle(interp, stylePtr, objc - 4, objv + 4, flags);
    Tcl_Release(stylePtr);
    if (result == TCL_ERROR) {
	return TCL_ERROR;
    }
    comboPtr->flags |= (CM_LAYOUT | CM_SCROLL);
    EventuallyRedraw(comboPtr);
    return TCL_OK;
}

static int
StyleDeleteOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    Style *stylePtr;

    if (GetStyleFromObj(interp, comboPtr, objv[3], &stylePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (stylePtr->refCount > 0) {
	Tcl_AppendResult(interp, "can't destroy combomenu style \"", 
			 stylePtr->name, "\": style in use.", (char *)NULL);
	return TCL_ERROR;
    }
    DestroyStyle(stylePtr);
    return TCL_OK;
}

static int
StyleNamesOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    Tcl_Obj *listObjPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (hPtr = Blt_FirstHashEntry(&comboPtr->styleTable, &iter); 
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	Style *stylePtr;
	int found;
	int i;

	found = TRUE;
	stylePtr = Blt_GetHashValue(hPtr);
	for (i = 3; i < objc; i++) {
	    const char *pattern;

	    pattern = Tcl_GetString(objv[i]);
	    found = Tcl_StringMatch(stylePtr->name, pattern);
	    if (found) {
		break;
	    }
	}
	if (found) {
	    Tcl_ListObjAppendElement(interp, listObjPtr,
		Tcl_NewStringObj(stylePtr->name, -1));
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

static Blt_OpSpec styleOps[] =
{
    {"cget",        2, StyleCgetOp,        5, 5, "name option",},
    {"configure",   2, StyleConfigureOp,   4, 0, "name ?option value?...",},
    {"create",      2, StyleCreateOp,      4, 0, "name ?option value?...",},
    {"delete",      1, StyleDeleteOp,      3, 0, "?name...?",},
    {"names",       1, StyleNamesOp,       3, 0, "?pattern...?",},
};

static int nStyleOps = sizeof(styleOps) / sizeof(Blt_OpSpec);

static int
StyleOp(ComboMenu *comboPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    ComboMenuCmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nStyleOps, styleOps, BLT_OP_ARG2, 
	objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (comboPtr, interp, objc, objv);
    return result;
}

static int
TypeOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    Item *itemPtr;

    if (GetItemFromObj(interp, comboPtr, objv[2], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr != NULL) {
	const char *name;

	name = NameOfType(itemPtr->flags);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), name, -1);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * UnpostOp --
 *
 *	Unposts this menu.
 *
 *  .cm post 
 *
 *---------------------------------------------------------------------------
 */
static int
UnpostOp(
    ComboMenu *comboPtr, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    if (!Tk_IsMapped(comboPtr->tkwin)) {
	return TCL_OK;		/* This menu is already unposted. */
    }
    /* Deactivate the current item. */
    if (comboPtr->postedPtr != NULL) {
	if (UnpostCascade(interp, comboPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	comboPtr->postedPtr = NULL;
    }
    if (Tk_IsMapped(comboPtr->tkwin)) {
	Tk_UnmapWindow(comboPtr->tkwin);
    }
#ifdef notdef
    {
	int result;

	Tcl_Obj *cmd[2];
	char *path;
	
	path = Tk_PathName(Tk_Parent(comboPtr->tkwin));
	cmd[0] = Tcl_NewStringObj(path, -1);
	cmd[1] = Tcl_NewStringObj("unpost", 6);
	Tcl_IncrRefCount(cmd[0]);
	Tcl_IncrRefCount(cmd[1]);
	result = Tcl_EvalObjv(interp, 2, cmd, 0);
	Tcl_DecrRefCount(cmd[1]);
	Tcl_DecrRefCount(cmd[0]);
	return result;
    }
#else
    return TCL_OK;
#endif
}

static int
XpositionOp(
    ComboMenu *comboPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Item *itemPtr;

    if (GetItemFromObj(interp, comboPtr, objv[3], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == NULL) {
	Tcl_AppendResult(interp, "can't get x-position of item: no item \"", 
			 Tcl_GetString(objv[3]), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), itemPtr->xWorld-comboPtr->xOffset);
    return TCL_OK;
}

static int
XviewOp(
    ComboMenu *comboPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    int w;

    w = VPORTWIDTH(comboPtr);
    if (objc == 2) {
	double fract;
	Tcl_Obj *listObjPtr, *objPtr;

	/* Report first and last fractions */
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	/*
	 * Note: we are bounding the fractions between 0.0 and 1.0 to support
	 * the "canvas"-style of scrolling.
	 */
	fract = (double)comboPtr->xOffset / comboPtr->worldHeight;
	objPtr = Tcl_NewDoubleObj(FCLAMP(fract));
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	fract = (double)(comboPtr->xOffset + w) / comboPtr->worldWidth;
	objPtr = Tcl_NewDoubleObj(FCLAMP(fract));
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	Tcl_SetObjResult(interp, listObjPtr);
	return TCL_OK;
    }
    if (Blt_GetScrollInfoFromObj(interp, objc - 2, objv + 2, &comboPtr->xOffset,
	comboPtr->worldWidth, w, comboPtr->xScrollUnits, 
	BLT_SCROLL_MODE_CANVAS) != TCL_OK) {
	return TCL_ERROR;
    }
    comboPtr->flags |= CM_SCROLL;
    EventuallyRedraw(comboPtr);
    return TCL_OK;
}

static int
YpositionOp(
    ComboMenu *comboPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Item *itemPtr;

    if (GetItemFromObj(interp, comboPtr, objv[3], &itemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == NULL) {
	Tcl_AppendResult(interp, "can't get y-position of item: such index \"", 
			 Tcl_GetString(objv[3]), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), itemPtr->yWorld-comboPtr->yOffset);
    return TCL_OK;
}

static int
YviewOp(
    ComboMenu *comboPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    int height;

    height = VPORTHEIGHT(comboPtr);
    if (objc == 2) {
	double fract;
	Tcl_Obj *listObjPtr, *objPtr;

	/* Report first and last fractions */
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	/*
	 * Note: we are bounding the fractions between 0.0 and 1.0 to support
	 * the "canvas"-style of scrolling.
	 */
	fract = (double)comboPtr->yOffset / comboPtr->worldHeight;
	objPtr = Tcl_NewDoubleObj(FCLAMP(fract));
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	fract = (double)(comboPtr->yOffset + height) / comboPtr->worldHeight;
	objPtr = Tcl_NewDoubleObj(FCLAMP(fract));
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	Tcl_SetObjResult(interp, listObjPtr);
	return TCL_OK;
    }
    if (Blt_GetScrollInfoFromObj(interp, objc - 2, objv + 2, &comboPtr->yOffset,
	comboPtr->worldHeight, height, comboPtr->yScrollUnits, 
	BLT_SCROLL_MODE_CANVAS) != TCL_OK) {
	return TCL_ERROR;
    }
    comboPtr->flags |= CM_SCROLL;
    EventuallyRedraw(comboPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * DestroyComboMenu --
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
DestroyComboMenu(DestroyData dataPtr)	/* Pointer to the widget record. */
{
    ComboMenu *comboPtr = (ComboMenu *)dataPtr;

    DestroyItems(comboPtr);
    DestroyStyles(comboPtr);
    DestroyLabels(comboPtr);
    Blt_DeleteHashTable(&comboPtr->tagTable);
    DestroyIcons(comboPtr);
    if (comboPtr->painter != NULL) {
	Blt_FreePainter(comboPtr->painter);
    }
    iconOption.clientData = comboPtr;
    Blt_FreeOptions(comboConfigSpecs, (char *)comboPtr, comboPtr->display, 0);
    Tcl_DeleteCommandFromToken(comboPtr->interp, comboPtr->cmdToken);
    Blt_Free(comboPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * NewComboMenu --
 *
 *---------------------------------------------------------------------------
 */
static ComboMenu *
NewComboMenu(Tcl_Interp *interp, Tk_Window tkwin)
{
    ComboMenu *comboPtr;

    comboPtr = Blt_AssertCalloc(1, sizeof(ComboMenu));

    Tk_SetClass(tkwin, "ComboMenu");

    comboPtr->tkwin = tkwin;
    comboPtr->display = Tk_Display(tkwin);
    comboPtr->interp = interp;
    comboPtr->flags |= (CM_LAYOUT | CM_SCROLL);
    comboPtr->relief = TK_RELIEF_SOLID;
    comboPtr->xScrollUnits = 2;
    comboPtr->yScrollUnits = 2;
    comboPtr->borderWidth = 1;
    comboPtr->chain = Blt_Chain_Create();
    comboPtr->painter = Blt_GetPainter(tkwin, 1.0);
    Blt_InitHashTable(&comboPtr->iconTable,  BLT_STRING_KEYS);
    Blt_InitHashTable(&comboPtr->labelTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&comboPtr->styleTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&comboPtr->tagTable, BLT_STRING_KEYS);
    AddDefaultStyle(interp, comboPtr);
    Blt_SetWindowInstanceData(tkwin, comboPtr);
    return comboPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * ComboMenuCmd --
 *
 * 	This procedure is invoked to process the "combomenu" command.  See the
 * 	user documentation for details on what it does.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec menuOps[] =
{
    {"activate",    2, ActivateOp,    3, 3, "item",},
    {"add",         2, AddOp,         2, 0, "?option value?",},
    {"cget",        2, CgetOp,        3, 3, "option",},
    {"configure",   2, ConfigureOp,   2, 0, "?option value?...",},
    {"delete",      1, DeleteOp,      2, 0, "items...",},
    {"find",        1, FindOp,        4, 4, "next|previous|underline string",},
    {"index",       3, IndexOp,       3, 3, "item",},
    {"insert",      3, InsertOp,      3, 0, 
	"after|at|before index ?option value?",},
    {"invoke",      3, InvokeOp,      3, 3, "item",},
    {"item",        2, ItemOp,        2, 0, "oper args",},
    {"listadd",     1, AddListOp,     3, 0, "labelList ?option value?",},
    {"names",       2, NamesOp,       2, 0, "?pattern...?",},
    {"nearest",     3, NearestOp,     4, 4, "x y",},
    {"next",        3, NextOp,        3, 3, "item",},
    {"post",        4, PostOp,        2, 4, "?x y?",},
    {"postcascade", 5, PostCascadeOp, 3, 3, "item",},
    {"previous",    2, PreviousOp,    3, 3, "item",},
    {"scan",        2, ScanOp,        5, 5, "dragto|mark x y",},
    {"see",         2, SeeOp,         3, 5, "item",},
    {"size",        2, SizeOp,        2, 2, "",},
    {"style",       2, StyleOp,       2, 0, "op ?args...?",},
    {"type",        1, TypeOp,        3, 3, "item",},
    {"unpost",      1, UnpostOp,      2, 2, "",},
    {"xposition",   2, XpositionOp,   3, 3, "item",},
    {"xview",       2, XviewOp,       2, 5, 
	"?moveto fract? ?scroll number what?",},
    {"yposition",   2, YpositionOp,   3, 3, "item",},
    {"yview",       2, YviewOp,       2, 5, 
	"?moveto fract? ?scroll number what?",},
};

static int nMenuOps = sizeof(menuOps) / sizeof(Blt_OpSpec);

typedef int (ComboInstOp)(ComboMenu *comboPtr, Tcl_Interp *interp, int objc,
			  Tcl_Obj *const *objv);

static int
ComboMenuInstCmdProc(
    ClientData clientData,	/* Information about the widget. */
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument vector. */
{
    ComboInstOp *proc;
    ComboMenu *comboPtr = clientData;
    int result;

    proc = Blt_GetOpFromObj(interp, nMenuOps, menuOps, BLT_OP_ARG1, objc, objv,
	0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve(comboPtr);
    result = (*proc) (comboPtr, interp, objc, objv);
    Tcl_Release(comboPtr);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * ComboMenuInstCmdDeletedProc --
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
ComboMenuInstCmdDeletedProc(ClientData clientData)
{
    ComboMenu *comboPtr = clientData; /* Pointer to widget record. */

    if (comboPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = comboPtr->tkwin;
	comboPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ComboMenuCmd --
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
ComboMenuCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument strings. */
{
    ComboMenu *comboPtr;
    Tcl_CmdInfo cmdInfo;
    Tk_Window tkwin;
    XSetWindowAttributes attrs;
    char *path;
    unsigned int mask;

    if (objc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", 
		Tcl_GetString(objv[0]), " pathName ?option value?...\"", 
		(char *)NULL);
	return TCL_ERROR;
    }
    /*
     * First time in this interpreter, invoke a procedure to initialize
     * various bindings on the combomenu widget.  If the procedure doesn't
     * already exist, source it from "$blt_library/combomenu.tcl".  We
     * deferred sourcing the file until now so that the variable $blt_library
     * could be set within a script.
     */
    if (!Tcl_GetCommandInfo(interp, "::blt::ComboMenu::PostMenu", &cmdInfo)) {
	static char cmd[] = "source [file join $blt_library combomenu.tcl]";

	if (Tcl_GlobalEval(interp, cmd) != TCL_OK) {
	    char info[200];
	    sprintf_s(info, 200, "\n    (while loading bindings for %.50s)", 
		    Tcl_GetString(objv[0]));
	    Tcl_AddErrorInfo(interp, info);
	    return TCL_ERROR;
	}
    }
    path = Tcl_GetString(objv[1]);
#define TOP_LEVEL_SCREEN ""
    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp), path, 
	TOP_LEVEL_SCREEN);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    comboPtr = NewComboMenu(interp, tkwin);
    if (ConfigureComboMenu(interp, comboPtr, objc - 2, objv + 2, 0) != TCL_OK) {
	Tk_DestroyWindow(comboPtr->tkwin);
	return TCL_ERROR;
    }
    mask = (ExposureMask | StructureNotifyMask | FocusChangeMask);
    Tk_CreateEventHandler(tkwin, mask, ComboMenuEventProc, comboPtr);
    comboPtr->cmdToken = Tcl_CreateObjCommand(interp, path, 
	ComboMenuInstCmdProc, comboPtr, ComboMenuInstCmdDeletedProc);

    attrs.override_redirect = True;
    attrs.backing_store = WhenMapped;
    attrs.save_under = True;
    mask = (CWOverrideRedirect | CWSaveUnder | CWBackingStore);
    Tk_ChangeWindowAttributes(tkwin, mask, &attrs);

    Tk_MakeWindowExist(tkwin);
    Tcl_SetObjResult(interp, objv[1]);
    return TCL_OK;
}

int
Blt_ComboMenuInitProc(Tcl_Interp *interp)
{
    static Blt_InitCmdSpec cmdSpec = { "combomenu", ComboMenuCmd, };

    return Blt_InitCmd(interp, "::blt", &cmdSpec);
}


static void
DrawItemBackground(Item *itemPtr, Drawable drawable, int x, int y)
{
    Blt_Background bg;
    Style *stylePtr;
    ComboMenu *comboPtr;    
    int relief;
    int w, h;

    stylePtr = itemPtr->stylePtr;
    comboPtr = itemPtr->comboPtr;
    relief = itemPtr->relief;
    if ((itemPtr->flags & (ITEM_DISABLED|ITEM_SEPARATOR)) == ITEM_DISABLED) {
	bg = stylePtr->disabledBg;
    } else if (itemPtr->flags & ITEM_ACTIVE) {
	bg = stylePtr->activeBg;
	relief = stylePtr->activeRelief;
    } else {
	bg = stylePtr->normalBg;
    }	    
    w = VPORTWIDTH(comboPtr);
    w = MAX(comboPtr->worldWidth, w);
    h = itemPtr->height;
#ifndef notdef
    if (y == 0) {
	int xOrigin, yOrigin;
	int px, py;

	px = PIXMAPX(comboPtr, itemPtr->xWorld);
	py = PIXMAPY(comboPtr, itemPtr->yWorld);
	Blt_GetBackgroundOrigin(bg, &xOrigin, &yOrigin);
	Blt_SetBackgroundOrigin(comboPtr->tkwin, bg, px, py);
	Blt_FillBackgroundRectangle(comboPtr->tkwin, drawable, bg, x, y, 
		w, h, stylePtr->borderWidth, relief);
	Blt_SetBackgroundOrigin(comboPtr->tkwin, bg, xOrigin, yOrigin);
    } else {
	Blt_FillBackgroundRectangle(comboPtr->tkwin, drawable, bg, x, y, 
		w, h, stylePtr->borderWidth, relief);
    }	
#else
    Blt_FillBackgroundRectangle(comboPtr->tkwin, drawable, bg, x, y, w, h,
	stylePtr->borderWidth, relief);
#endif
}

static void
DrawSeparator(Item *itemPtr, Drawable drawable, int x, int y, int w, int h)
{
    XPoint points[2];
    Tk_3DBorder border;

    points[0].x = x + ITEM_XPAD;
    points[0].y = y + h / 2;
    points[1].x = w - 2 * ITEM_XPAD;
    points[1].y = points[0].y;

    border = Blt_BackgroundBorder(itemPtr->stylePtr->normalBg);
    Tk_Draw3DPolygon(itemPtr->comboPtr->tkwin, drawable, border, points, 2, 1,
	    TK_RELIEF_SUNKEN);
}

static void
DrawCheck(Item *itemPtr, Drawable drawable, int x, int y, int w, int h)
{
    Style *stylePtr;
    Display *display;
    ComboMenu *comboPtr;

    comboPtr = itemPtr->comboPtr;
#ifdef notdef
    stylePtr = itemPtr->stylePtr;
    display = itemPtr->comboPtr->display;
    if (itemPtr->flags & ITEM_DISABLED) {
	XDrawRectangle(display, drawable, stylePtr->labelDisabledGC, x, y, w,h);
    } else {
	XFillRectangle(display, drawable, stylePtr->indBgGC, x, y, w, h);
	XDrawRectangle(display, drawable, stylePtr->indFgGC, x, y, w, h);
    }
    /*
     * Use the value of the variable to update the selected status of the
     * item.
     */
    if (itemPtr->flags & ITEM_SELECTED) {
	int ax, ay, bx, by, cx, cy;
	int i;
	GC gc;

	ax = x + 2, ay = y + 2 + (h / 3);
	bx = x + (w / 2) - 2, by = y + h - 4;
	cx = x + w, cy = y;
	
	if (itemPtr->flags & ITEM_DISABLED) {
	    gc = stylePtr->labelDisabledGC;
	} else {
	    gc = stylePtr->indFgGC;
	}
	for (i = 0; i < 3; i++) {
	    XDrawLine(display, drawable, gc, ax, ay, bx, by);
	    XDrawLine(display, drawable, gc, bx, by, cx, cy);
	    ay++, by++, cy++;
	}
    }
#else
    Blt_Pixel fill, check, outline;
    Blt_Picture picture;
    int on;
    stylePtr = itemPtr->stylePtr;
    display = itemPtr->comboPtr->display;
    
    check = Blt_XColorToPixel(stylePtr->indFgColor);
    on = (itemPtr->flags & ITEM_SELECTED);
    if (itemPtr->flags & ITEM_DISABLED) {
	if (stylePtr->checkbutton[0] == NULL) {
	    XColor *colorPtr;

	    colorPtr = Blt_BackgroundBorderColor(stylePtr->disabledBg);
	    outline = Blt_XColorToPixel(stylePtr->labelDisabledColor);
	    fill = Blt_XColorToPixel(colorPtr);
	    stylePtr->checkbutton[0] = 
		Blt_PaintCheckbox(w, h, fill.u32, outline.u32, outline.u32, on);
	} 
	picture = stylePtr->checkbutton[0];
    } else if (itemPtr->flags & ITEM_SELECTED) {
	if (stylePtr->checkbutton[1] == NULL) {
	    fill = Blt_XColorToPixel(stylePtr->indBgColor);
	    outline = Blt_XColorToPixel(stylePtr->indFgColor);
	    check = Blt_XColorToPixel(stylePtr->selBgColor);
	    stylePtr->checkbutton[1] = 
		Blt_PaintCheckbox(w, h, 0xFFFFFFFF, 0xFF000000, check.u32, TRUE);
	} 
	picture = stylePtr->checkbutton[1];
    } else {
	if (stylePtr->checkbutton[2] == NULL) {
	    fill = Blt_XColorToPixel(stylePtr->indBgColor);
	    outline = Blt_XColorToPixel(stylePtr->indFgColor);
	    stylePtr->checkbutton[2] = 
		Blt_PaintCheckbox(w, h, 0xFFFFFFFF, 0xFF000000, check.u32, 0);
	} 
	picture = stylePtr->checkbutton[2];
    }
    Blt_PaintPicture(comboPtr->painter, drawable, picture, 0, 0, w, h, x, y, 0);
#endif
}

static void
DrawRadio(Item *itemPtr, Drawable drawable, int x, int y, int w, int h)
{
    Style *stylePtr;
    Display *display;
    ComboMenu *comboPtr;

    comboPtr = itemPtr->comboPtr;
#ifdef notdef
    stylePtr = itemPtr->stylePtr;
    display = itemPtr->comboPtr->display;
    if (itemPtr->flags & ITEM_DISABLED) {
	XDrawArc(display, drawable, stylePtr->labelDisabledGC, x, y, w, h, 0, 
		 23040);
	if (itemPtr->flags & ITEM_SELECTED) {
	    XFillArc(display, drawable, stylePtr->labelDisabledGC, x + 3, 
		y + 3, w - 6, h - 6, 0, 23040);
	}
    } else {
	XFillArc(display, drawable, stylePtr->indBgGC, x, y, w, h, 0, 23040);
	XDrawArc(display, drawable, stylePtr->indFgGC, x, y, w, h, 0, 23040);
	if (itemPtr->flags & ITEM_SELECTED) {
	    XFillArc(display, drawable, stylePtr->indFgGC, x + 3, y + 3, w - 6, 
		 h - 6, 0, 23040);
	}
    }
#else 
    Blt_Pixel fill, check, outline;
    Blt_Picture picture;
    int on;
    stylePtr = itemPtr->stylePtr;
    display = itemPtr->comboPtr->display;

    on = (itemPtr->flags & ITEM_SELECTED);
    check = Blt_XColorToPixel(stylePtr->indFgColor);
    if (itemPtr->flags & ITEM_DISABLED) {
	picture = stylePtr->radiobutton[0];
	if (picture == NULL) {
	    XColor *colorPtr;

	    colorPtr = Blt_BackgroundBorderColor(stylePtr->disabledBg);
	    outline = Blt_XColorToPixel(stylePtr->labelDisabledColor);
	    fill = Blt_XColorToPixel(colorPtr);
	    picture = Blt_PaintRadioButton(w, h, fill.u32, outline.u32, 
					   outline.u32, FALSE);
	    stylePtr->radiobutton[0] = picture;
	} 
    } else if (itemPtr->flags & ITEM_SELECTED) {
	picture = stylePtr->radiobutton[1];
	if (picture == NULL) {
	    fill = Blt_XColorToPixel(stylePtr->selBgColor);
	    outline = Blt_XColorToPixel(stylePtr->selFgColor);
	    picture = Blt_PaintRadioButton(w, h, fill.u32, outline.u32, 
					   check.u32, TRUE);
	    stylePtr->radiobutton[1] = picture;
	} 
    } else {
	picture = stylePtr->radiobutton[2];
	if (picture == NULL) {
	    fill = Blt_XColorToPixel(stylePtr->indBgColor);
	    outline = Blt_XColorToPixel(stylePtr->indFgColor);
	    picture = Blt_PaintRadioButton(w, h, fill.u32, outline.u32, 
					   check.u32, FALSE);
	    stylePtr->radiobutton[2] = picture;
	} 
    }
    Blt_PaintPicture(comboPtr->painter, drawable, picture, 0, 0, w, h, x, y, 0);
#endif
}

static void
DrawItem(Item *itemPtr, Drawable drawable, int x, int y)
{
    ComboMenu *comboPtr;    
    Style *stylePtr;
    int x0, w, h;

    itemPtr->flags &= ~ITEM_REDRAW;
    stylePtr = itemPtr->stylePtr;
    comboPtr = itemPtr->comboPtr;
    x0 = x;
    w = VPORTWIDTH(comboPtr) - 2 * stylePtr->borderWidth;
    x += stylePtr->borderWidth;
    h = itemPtr->height - 2 * stylePtr->borderWidth;
    y += stylePtr->borderWidth;
    if (itemPtr->flags & ITEM_SEPARATOR) {
	DrawSeparator(itemPtr, drawable, x, y, w, h);
	y += ITEM_SEP_HEIGHT;
    } else {	    
	x += ITEM_IPAD;
	/* Radiobutton or checkbutton. */
	if (itemPtr->flags & (ITEM_RADIOBUTTON | ITEM_CHECKBUTTON)) {
	    if (itemPtr->flags & ITEM_RADIOBUTTON) {
		DrawRadio(itemPtr, drawable, x,
			  y + (h - itemPtr->leftIndHeight) / 2,
			  itemPtr->leftIndWidth, itemPtr->leftIndHeight);
	    } else if (itemPtr->flags & ITEM_CHECKBUTTON) {
		DrawCheck(itemPtr, drawable, x, 
			  y + (h - itemPtr->leftIndHeight) / 2,
			  itemPtr->leftIndWidth, itemPtr->leftIndHeight);
	    }		
	}
	x += itemPtr->indent;
	if (comboPtr->leftIndWidth > 0) {
	    x += comboPtr->leftIndWidth + ITEM_IPAD;
	}
	/* Icon. */
	if (itemPtr->icon != NULL) {
	    Tk_RedrawImage(IconImage(itemPtr->icon), 0, 0, 
		IconWidth(itemPtr->icon), IconHeight(itemPtr->icon), drawable, 
			x + (comboPtr->iconWidth - itemPtr->iconWidth) / 2, 
			y + (h - IconHeight(itemPtr->icon)) / 2);
	}
	if (comboPtr->iconWidth > 0) {
	    x += comboPtr->iconWidth + ITEM_IPAD;
	}
	/* Image or label. */
	if (itemPtr->image != NULL) {
	    Tk_RedrawImage(IconImage(itemPtr->image), 0, 0, 
		IconWidth(itemPtr->image), IconHeight(itemPtr->image), 
		drawable, x, y + (h - IconHeight(itemPtr->image)) / 2);
	} else if (itemPtr->label != emptyString) {
	    TextStyle ts;
	    XColor *fg;
	    
	    if (itemPtr->flags & ITEM_DISABLED) {
		fg = stylePtr->labelDisabledColor;
	    } else if (itemPtr->flags & ITEM_ACTIVE) {
		fg = stylePtr->labelActiveColor;
	    } else {
		fg = stylePtr->labelNormalColor;
	    }
	    Blt_Ts_InitStyle(ts);
	    Blt_Ts_SetFont(ts, stylePtr->labelFont);
	    Blt_Ts_SetForeground(ts, fg);
	    Blt_Ts_SetAnchor(ts, TK_ANCHOR_NW);
	    Blt_Ts_SetUnderline(ts, itemPtr->underline);
	    Blt_Ts_SetJustify(ts, TK_JUSTIFY_LEFT);
	    Blt_DrawText(comboPtr->tkwin, drawable, (char *)itemPtr->label, &ts,
		x, y + (h - itemPtr->labelHeight) / 2);
	}
	x = x0 + MAX(comboPtr->worldWidth, VPORTWIDTH(comboPtr)) - 2 * ITEM_IPAD;
	/* Accelerator or submenu arrow. */
	if (itemPtr->flags & ITEM_CASCADE) {
	    GC gc;

	    if (itemPtr->flags & ITEM_DISABLED) {
		gc = stylePtr->labelDisabledGC;
	    } else if (itemPtr->flags & ITEM_ACTIVE) {
		gc = stylePtr->labelActiveGC;
	    } else {
		gc = stylePtr->labelNormalGC;
	    }
	    x -= ITEM_R_IND_WIDTH;
	    Blt_DrawArrow(comboPtr->display, drawable, gc, x + ITEM_IPAD, y, 
		ITEM_R_IND_WIDTH, h, 1, ARROW_RIGHT);
	} else if (itemPtr->accel != NULL) {
	    TextStyle ts;
	    XColor *fg;
	    
	    if (itemPtr->flags & ITEM_DISABLED) {
		fg = stylePtr->accelDisabledColor;
	    } else if (itemPtr->flags & ITEM_ACTIVE) {
		fg = stylePtr->accelActiveColor;
	    } else {
		fg = stylePtr->accelNormalColor;
	    }
	    Blt_Ts_InitStyle(ts);
	    Blt_Ts_SetForeground(ts, fg);
	    Blt_Ts_SetFont(ts, stylePtr->accelFont);
	    Blt_Ts_SetAnchor(ts, TK_ANCHOR_NW);
	    Blt_Ts_SetJustify(ts, TK_JUSTIFY_LEFT);
	    x -= itemPtr->rightIndWidth;
	    Blt_DrawText(comboPtr->tkwin, drawable, (char *)itemPtr->accel, &ts,
		x, y + (h - itemPtr->rightIndHeight) / 2);
	}
    }
}

    
static void
DrawComboMenu(ComboMenu *comboPtr, Drawable drawable)
{

    /* Draw each visible item. */
    if (comboPtr->firstPtr != NULL) {
	Blt_ChainLink first, last, link;

	first = comboPtr->firstPtr->link;
	last = comboPtr->lastPtr->link;
	for (link = first; link != NULL; link = Blt_Chain_NextLink(link)) {
	    int x, y;
	    Item *itemPtr;

	    itemPtr = Blt_Chain_GetValue(link);
	    x = PIXMAPX(comboPtr, itemPtr->xWorld);
	    y = PIXMAPY(comboPtr, itemPtr->yWorld);
	    DrawItemBackground(itemPtr, drawable, x, y);
	    DrawItem(itemPtr, drawable, x, y);
	    if (link == last) {
		break;
	    }
	}
    }
    /* Manage the geometry of the scrollbars. */

    if (comboPtr->yScrollbarWidth > 0) {
	int x, y;
	int yScrollbarHeight;

	x = Tk_Width(comboPtr->tkwin) - comboPtr->borderWidth -
	    comboPtr->yScrollbarWidth;
	y = comboPtr->borderWidth;
	yScrollbarHeight = Tk_Height(comboPtr->tkwin) - 
	    comboPtr->xScrollbarHeight - 2 * comboPtr->borderWidth;
	if ((Tk_Width(comboPtr->yScrollbar) != comboPtr->yScrollbarWidth) ||
	    (Tk_Height(comboPtr->yScrollbar) != yScrollbarHeight) ||
	    (x != Tk_X(comboPtr->yScrollbar)) || 
	    (y != Tk_Y(comboPtr->yScrollbar))) {
	    Tk_MoveResizeWindow(comboPtr->yScrollbar, x, y, 
		comboPtr->yScrollbarWidth, yScrollbarHeight);
	}
	if (!Tk_IsMapped(comboPtr->yScrollbar)) {
	    Tk_MapWindow(comboPtr->yScrollbar);
	}
    } else if ((comboPtr->yScrollbar != NULL) &&
	       (Tk_IsMapped(comboPtr->yScrollbar))) {
	Tk_UnmapWindow(comboPtr->yScrollbar);
    }
    if (comboPtr->xScrollbarHeight > 0) {
	int x, y;
	int xScrollbarWidth;

	x = comboPtr->borderWidth;
	y = Tk_Height(comboPtr->tkwin) - comboPtr->xScrollbarHeight - 
	    comboPtr->borderWidth;
	xScrollbarWidth = Tk_Width(comboPtr->tkwin) - 
	    comboPtr->yScrollbarWidth - 2 * comboPtr->borderWidth;
	if ((Tk_Width(comboPtr->xScrollbar) != xScrollbarWidth) ||
	    (Tk_Height(comboPtr->xScrollbar) != comboPtr->xScrollbarHeight) ||
	    (x != Tk_X(comboPtr->xScrollbar)) || 
	    (y != Tk_Y(comboPtr->xScrollbar))) {
	    Tk_MoveResizeWindow(comboPtr->xScrollbar, x, y, xScrollbarWidth,
		comboPtr->xScrollbarHeight);
	}
	if (!Tk_IsMapped(comboPtr->xScrollbar)) {
	    Tk_MapWindow(comboPtr->xScrollbar);
	}
    } else if ((comboPtr->xScrollbar != NULL) && 
	       (Tk_IsMapped(comboPtr->xScrollbar))) {
	Tk_UnmapWindow(comboPtr->xScrollbar);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * DisplayItem --
 *
 *	This procedure is invoked to display an item in the combomenu widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the item.
 *
 *---------------------------------------------------------------------------
 */
static void
DisplayItem(ClientData clientData)
{
    Item *itemPtr = clientData;
    int x, y, w, h, d, sy;
    Pixmap drawable;
    ComboMenu *comboPtr;

    /*
     * Create a pixmap the size of the item for double buffering.
     */
    comboPtr = itemPtr->comboPtr;
    h = itemPtr->height;
    w = VPORTWIDTH(comboPtr);
    drawable = Tk_GetPixmap(comboPtr->display, Tk_WindowId(comboPtr->tkwin),
	w, h, Tk_Depth(comboPtr->tkwin));
#ifdef WIN32
    assert(drawable != None);
#endif
    DrawItemBackground(itemPtr, drawable, -comboPtr->xOffset, 0);
    DrawItem(itemPtr, drawable, -comboPtr->xOffset, 0);
    x = PIXMAPX(comboPtr, itemPtr->xWorld) + comboPtr->borderWidth;
    y = PIXMAPY(comboPtr, itemPtr->yWorld) + comboPtr->borderWidth;
    sy = 0;
    d = comboPtr->borderWidth - y;
    if (d > 0) {
	h -= d;
	sy = d;
	y += d;
    }
    d = (y + h) - (Tk_Height(comboPtr->tkwin) - comboPtr->borderWidth);
    if (d > 0) {
	h -= d;
    }
    XCopyArea(comboPtr->display, drawable, Tk_WindowId(comboPtr->tkwin),
	comboPtr->defStyle.labelNormalGC, 0, sy, w, h, comboPtr->borderWidth,y);
    Tk_FreePixmap(comboPtr->display, drawable);
}

/*
 *---------------------------------------------------------------------------
 *
 * DisplayComboMenu --
 *
 *	This procedure is invoked to display a combomenu widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the menu.
 *
 *---------------------------------------------------------------------------
 */
static void
DisplayComboMenu(ClientData clientData)
{
    ComboMenu *comboPtr = clientData;
    Pixmap drawable;
    int w, h;			/* Window width and height. */

    comboPtr->flags &= ~CM_REDRAW;
    if (comboPtr->tkwin == NULL) {
	return;			/* Window destroyed (should not get here) */
    }
#ifdef notdef
    fprintf(stderr, "Calling DisplayComboMenu(%s)\n", 
	    Tk_PathName(comboPtr->tkwin));
#endif
    if (comboPtr->flags & CM_LAYOUT) {
	ComputeMenuGeometry(comboPtr);
    }
    if ((Tk_Width(comboPtr->tkwin) <= 1) || (Tk_Height(comboPtr->tkwin) <= 1)){
	/* Don't bother computing the layout until the window size is
	 * something reasonable. */
	return;
    }
    if (!Tk_IsMapped(comboPtr->tkwin)) {
	/* The menu's window isn't displayed, so don't bother drawing
	 * anything.  By getting this far, we've at least computed the
	 * coordinates of the combomenu's new layout.  */
	return;
    }

    if (comboPtr->flags & CM_SCROLL) {
	int vw, vh;		/* Viewport width and height. */
	/* 
	 * The view port has changed. The visible items need to be
	 * recomputed and the scrollbars updated.
	 */
	ComputeVisibleItems(comboPtr);
	vw = VPORTWIDTH(comboPtr);
	vh = VPORTHEIGHT(comboPtr);
	if ((comboPtr->xScrollCmdObjPtr != NULL) &&
	    (comboPtr->flags & CM_XSCROLL)) {
	    Blt_UpdateScrollbar(comboPtr->interp, comboPtr->xScrollCmdObjPtr,
		(double)comboPtr->xOffset / comboPtr->worldWidth,
		(double)(comboPtr->xOffset + vw) / comboPtr->worldWidth);
	}
	if ((comboPtr->yScrollCmdObjPtr != NULL) && 
	    (comboPtr->flags & CM_YSCROLL)) {
	    Blt_UpdateScrollbar(comboPtr->interp, comboPtr->yScrollCmdObjPtr,
		(double)comboPtr->yOffset / comboPtr->worldHeight,
		(double)(comboPtr->yOffset + vh)/comboPtr->worldHeight);
	}
	comboPtr->flags &= ~CM_SCROLL;
    }
    /*
     * Create a pixmap the size of the window for double buffering.
     */
    w = Tk_Width(comboPtr->tkwin) - 2 * comboPtr->borderWidth - 
	comboPtr->yScrollbarWidth;
    h = Tk_Height(comboPtr->tkwin) - 2 * comboPtr->borderWidth - 
	comboPtr->xScrollbarHeight;
    drawable = Tk_GetPixmap(comboPtr->display, Tk_WindowId(comboPtr->tkwin),
	w, h, Tk_Depth(comboPtr->tkwin));
#ifdef WIN32
    assert(drawable != None);
#endif
    if (comboPtr->activePtr == NULL) {
	ActivateItem(comboPtr, comboPtr->firstPtr);
    }
    /* 
     * Shadowed menu.  Request window size slightly bigger than menu.
     * Get snapshot of background from root menu. 
     */
    /* Background */
    Blt_FillBackgroundRectangle(comboPtr->tkwin, drawable, 
	comboPtr->defStyle.normalBg, 0, 0, w, h, 0, TK_RELIEF_FLAT);
    DrawComboMenu(comboPtr, drawable);
    XCopyArea(comboPtr->display, drawable, Tk_WindowId(comboPtr->tkwin),
	comboPtr->defStyle.labelNormalGC, 0, 0, w, h, 
	      comboPtr->borderWidth, comboPtr->borderWidth);
    Tk_FreePixmap(comboPtr->display, drawable);
    if ((comboPtr->xScrollbarHeight > 0) && (comboPtr->yScrollbarWidth > 0)) {
	/* Draw the empty corner. */
	Blt_FillBackgroundRectangle(comboPtr->tkwin, 
		Tk_WindowId(comboPtr->tkwin), comboPtr->defStyle.disabledBg, 
		w + comboPtr->borderWidth, h + comboPtr->borderWidth, 
		comboPtr->yScrollbarWidth, comboPtr->xScrollbarHeight,
		0, TK_RELIEF_FLAT);
    }
    Blt_DrawBackgroundRectangle(comboPtr->tkwin, Tk_WindowId(comboPtr->tkwin), 
	comboPtr->defStyle.normalBg, 0, 0, Tk_Width(comboPtr->tkwin), 
	Tk_Height(comboPtr->tkwin), comboPtr->borderWidth, comboPtr->relief);
}


