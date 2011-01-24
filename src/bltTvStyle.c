
/*
 * bltTvStyle.c --
 *
 * This module implements styles for treeview widget cells.
 *
 *	Copyright 1998-2008 George A Howlett.
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

#ifndef NO_TREEVIEW
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "bltOp.h"
#include "bltTreeView.h"
#include "bltList.h"
#include "bltPicture.h"
#include "bltPainter.h"

typedef int (TreeViewCmdProc)(TreeView *viewPtr, Tcl_Interp *interp, 
			      int objc, Tcl_Obj *const *objv);

#define STYLE_GAP		2
#define ARROW_WIDTH		13

static Blt_OptionParseProc ObjToIcon;
static Blt_OptionPrintProc IconToObj;
static Blt_OptionFreeProc FreeIcon;

Blt_CustomOption bltTreeViewIconOption =
{
    ObjToIcon, IconToObj, FreeIcon, NULL,
};

#define DEF_STYLE_HIGHLIGHT_BACKGROUND	STD_NORMAL_BACKGROUND
#define DEF_STYLE_HIGHLIGHT_FOREGROUND	STD_NORMAL_FOREGROUND
#ifdef WIN32
#define DEF_STYLE_ACTIVE_BACKGROUND	RGB_GREY85
#else
#define DEF_STYLE_ACTIVE_BACKGROUND	RGB_GREY95
#endif
#define DEF_STYLE_ACTIVE_FOREGROUND 	STD_ACTIVE_FOREGROUND
#define DEF_STYLE_GAP			"2"

typedef struct {
    int refCount;			/* Usage reference count. */
    unsigned int flags;
    const char *name;			
    TreeViewStyleClass *classPtr;	/* Class-specific routines to manage
					 * style. */
    Blt_HashEntry *hashPtr;
    Blt_ChainLink link;

    /* General style fields. */
    Tk_Cursor cursor;			/* X Cursor */
    TreeViewIcon icon;			/* If non-NULL, is a Tk_Image to be
					 * drawn in the cell. */
    int gap;				/* # pixels gap between icon and
					 * text. */
    Blt_Font font;
    XColor *fgColor;			/* Normal foreground color of cell. */
    XColor *highlightFgColor;		/* Foreground color of cell when
					 * highlighted. */
    XColor *activeFgColor;		/* Foreground color of cell when
					 * active. */
    XColor *selFgColor;			/* Foreground color of a selected
					 * cell. If non-NULL, overrides default
					 * foreground color specification. */
    Blt_Background bg;			/* Normal background color of cell. */
    Blt_Background highlightBg;		/* Background color of cell when
					 * highlighted. */
    Blt_Background activeBg;		/* Background color of cell when
					 * active. */
    Blt_Background selBg;		/* Background color of a selected
					 * cell.  If non-NULL, overrides the
					 * default background color
					 * specification. */
    const char *validateCmd;

    /* TextBox-specific fields */
    GC gc;
    GC highlightGC;
    GC activeGC;
    int side;				/* Position of the text in relation to
					 * the icon.  */
    Blt_TreeKey key;			/* Actual data resides in this tree
					   value. */
    Tcl_Obj *cmdObjPtr;
} TreeViewTextBox;

#ifdef WIN32
#define DEF_TEXTBOX_CURSOR		"arrow"
#else
#define DEF_TEXTBOX_CURSOR		"hand2"
#endif /*WIN32*/
#define DEF_TEXTBOX_SIDE		"left"
#define DEF_TEXTBOX_VALIDATE_COMMAND	(char *)NULL
#define DEF_TEXTBOX_COMMAND		(char *)NULL

#define TextBoxOffset(x)	Blt_Offset(TreeViewTextBox, x)

static Blt_ConfigSpec textBoxSpecs[] =
{
    {BLT_CONFIG_BACKGROUND, "-activebackground", "activeBackground", 
	"ActiveBackground", DEF_STYLE_ACTIVE_BACKGROUND, 
	TextBoxOffset(activeBg), 0},
    {BLT_CONFIG_SYNONYM, "-activebg", "activeBackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-activefg", "activeFackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-activeforeground", "activeForeground", 
	"ActiveForeground", DEF_STYLE_ACTIVE_FOREGROUND, 
	TextBoxOffset(activeFgColor), 0},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	(char *)NULL, TextBoxOffset(bg), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_OBJ, "-command", "command", "Command", DEF_TEXTBOX_COMMAND, 
	TextBoxOffset(cmdObjPtr), 0},
    {BLT_CONFIG_CURSOR, "-cursor", "cursor", "Cursor", DEF_TEXTBOX_CURSOR, 
	TextBoxOffset(cursor), 0},
    {BLT_CONFIG_BITMASK, "-edit", "edit", "Edit", (char *)NULL, 
	TextBoxOffset(flags), BLT_CONFIG_DONT_SET_DEFAULT,
	(Blt_CustomOption *)STYLE_EDITABLE},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_FONT, "-font", "font", "Font", (char *)NULL, 
	TextBoxOffset(font), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground", (char *)NULL,
	TextBoxOffset(fgColor),BLT_CONFIG_NULL_OK },
    {BLT_CONFIG_PIXELS_NNEG, "-gap", "gap", "Gap", DEF_STYLE_GAP, 
	TextBoxOffset(gap), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BACKGROUND, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_STYLE_HIGHLIGHT_BACKGROUND, 
        TextBoxOffset(highlightBg), BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-highlightforeground", "highlightForeground", 
	"HighlightForeground", DEF_STYLE_HIGHLIGHT_FOREGROUND, 
	TextBoxOffset(highlightFgColor), 0},
    {BLT_CONFIG_SYNONYM, "-highlightbg", "highlightBackground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-highlightfg", "highlightForeground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_CUSTOM, "-icon", "icon", "Icon", (char *)NULL, 
	TextBoxOffset(icon), BLT_CONFIG_NULL_OK, &bltTreeViewIconOption},
    {BLT_CONFIG_STRING, "-key", "key", "key", 	(char *)NULL, 
	TextBoxOffset(key), BLT_CONFIG_NULL_OK, 0},
    {BLT_CONFIG_BACKGROUND, "-selectbackground", "selectBackground", 
	"Foreground", (char *)NULL, TextBoxOffset(selBg), 0},
    {BLT_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	(char *)NULL, TextBoxOffset(selFgColor), 0},
    {BLT_CONFIG_SIDE, "-side", "side", "side", DEF_TEXTBOX_SIDE, 
	TextBoxOffset(side), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_STRING, "-validatecommand", "validateCommand", 
	"ValidateCommand", DEF_TEXTBOX_VALIDATE_COMMAND, 
	TextBoxOffset(validateCmd), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 
	0, 0}
};


typedef struct {
    int refCount;			/* Usage reference count. */
    unsigned int flags;			/* Contains style type and update
					 * flags. */
    const char *name;			/* Instance name. */
    TreeViewStyleClass *classPtr;	/* Class-specific routines to manage
					 * style. */
    Blt_HashEntry *hashPtr;	
    Blt_ChainLink link;

    /* General style fields. */
    Tk_Cursor cursor;			/* X Cursor */
    TreeViewIcon icon;			/* If non-NULL, is a Tk_Image to be
					 * drawn in the cell. */
    int gap;				/* # pixels gap between icon and
					 * text. */
    Blt_Font font;
    XColor *fgColor;			/* Normal foreground color of cell. */
    XColor *highlightFgColor;		/* Foreground color of cell when
					 * highlighted. */
    XColor *activeFgColor;		/* Foreground color of cell when
					 * active. */
    XColor *selFgColor;			/* Foreground color of a selected
					 * cell. If non-NULL, overrides
					 * default foreground color
					 * specification. */

    Blt_Background bg;			/* Normal background color of cell. */
    Blt_Background highlightBg;		/* Background color of cell when
					 * highlighted. */
    Blt_Background activeBg;		/* Background color of cell when
					 * active. */
    Blt_Background selBg;		/* Background color of a selected
					 * cell.  If non-NULL, overrides the
					 * default background color
					 * specification. */
    const char *validateCmd;

    /* Checkbox specific fields. */
    GC gc;
    GC highlightGC;
    GC activeGC;
    Blt_TreeKey key;			/* Actual data resides in this tree
					   value. */
    int size;				/* Size of the checkbox. */
    int showValue;			/* If non-zero, display the on/off
					 * value.  */
    const char *onValue;
    const char *offValue;
    int lineWidth;			/* Linewidth of the surrounding
					 * box. */
    XColor *boxColor;			/* Rectangle (box) color (grey). */
    XColor *fillColor;			/* Fill color (white) */
    XColor *checkColor;			/* Check color (red). */

    TextLayout *onPtr, *offPtr;
    
    Blt_Painter painter;
    Blt_Picture selectedPicture;
    Blt_Picture normalPicture;
    Blt_Picture disabledPicture;
    Tcl_Obj *cmdObjPtr;			/* If non-NULL, command to be invoked
					 * when check is clicked. */
} TreeViewCheckBox;

#define DEF_CHECKBOX_BOX_COLOR		(char *)NULL
#define DEF_CHECKBOX_CHECK_COLOR	"red"
#define DEF_CHECKBOX_COMMAND		(char *)NULL
#define DEF_CHECKBOX_FILL_COLOR		(char *)NULL
#define DEF_CHECKBOX_OFFVALUE		"0"
#define DEF_CHECKBOX_ONVALUE		"1"
#define DEF_CHECKBOX_SHOWVALUE		"yes"
#define DEF_CHECKBOX_SIZE		"11"
#define DEF_CHECKBOX_LINEWIDTH		"2"
#define DEF_CHECKBOX_GAP		"4"
#ifdef WIN32
#define DEF_CHECKBOX_CURSOR		"arrow"
#else
#define DEF_CHECKBOX_CURSOR		"hand2"
#endif /*WIN32*/

#define CheckBoxOffset(x)	Blt_Offset(TreeViewCheckBox, x)

static Blt_ConfigSpec checkBoxSpecs[] =
{
    {BLT_CONFIG_BACKGROUND, "-activebackground", "activeBackground", 
	"ActiveBackground", DEF_STYLE_ACTIVE_BACKGROUND, 
	CheckBoxOffset(activeBg), 0},
    {BLT_CONFIG_SYNONYM, "-activebg", "activeBackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-activefg", "activeFackground", 
	(char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-activeforeground", "activeForeground", 
	"ActiveForeground", DEF_STYLE_ACTIVE_FOREGROUND, 
	CheckBoxOffset(activeFgColor), 0},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	(char *)NULL, CheckBoxOffset(bg), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_PIXELS_POS, "-boxsize", "boxSize", "BoxSize", DEF_CHECKBOX_SIZE,
	CheckBoxOffset(size), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-command", "command", "Command", DEF_CHECKBOX_COMMAND, 
        CheckBoxOffset(cmdObjPtr), 0},
    {BLT_CONFIG_CURSOR, "-cursor", "cursor", "Cursor", DEF_CHECKBOX_CURSOR, 
	CheckBoxOffset(cursor), 0},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_FONT, "-font", "font", "Font", (char *)NULL, 
	CheckBoxOffset(font), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground", (char *)NULL,
	CheckBoxOffset(fgColor), BLT_CONFIG_NULL_OK },
    {BLT_CONFIG_PIXELS_NNEG, "-gap", "gap", "Gap", DEF_CHECKBOX_GAP, 
	CheckBoxOffset(gap), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BACKGROUND, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_STYLE_HIGHLIGHT_BACKGROUND, 
        CheckBoxOffset(highlightBg), BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-highlightforeground", "highlightForeground", 
	"HighlightForeground", DEF_STYLE_HIGHLIGHT_FOREGROUND, 
	 CheckBoxOffset(highlightFgColor), 0},
    {BLT_CONFIG_SYNONYM, "-highlightbg", "highlightBackground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-highlightfg", "highlightForeground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_CUSTOM, "-icon", "icon", "Icon", (char *)NULL, 
	CheckBoxOffset(icon), BLT_CONFIG_NULL_OK, &bltTreeViewIconOption},
    {BLT_CONFIG_STRING, "-key", "key", "key", (char *)NULL, 
	CheckBoxOffset(key), BLT_CONFIG_NULL_OK, 0},
    {BLT_CONFIG_PIXELS_NNEG, "-linewidth", "lineWidth", "LineWidth",
	DEF_CHECKBOX_LINEWIDTH, CheckBoxOffset(lineWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_COLOR, "-checkcolor", "checkColor", "CheckColor", 
	DEF_CHECKBOX_CHECK_COLOR, CheckBoxOffset(checkColor), 0},
    {BLT_CONFIG_COLOR, "-boxcolor", "boxColor", "BoxColor", 
	DEF_CHECKBOX_BOX_COLOR, CheckBoxOffset(boxColor), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-fillcolor", "fillColor", "FillColor", 
	DEF_CHECKBOX_FILL_COLOR, CheckBoxOffset(fillColor), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_STRING, "-offvalue", "offValue", "OffValue",
	DEF_CHECKBOX_OFFVALUE, CheckBoxOffset(offValue), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_STRING, "-onvalue", "onValue", "OnValue",
	DEF_CHECKBOX_ONVALUE, CheckBoxOffset(onValue), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_STRING, "-key", "key", "key", (char *)NULL, CheckBoxOffset(key),
	BLT_CONFIG_NULL_OK, 0},
    {BLT_CONFIG_BACKGROUND, "-selectbackground", "selectBackground", 
	"Foreground", (char *)NULL, CheckBoxOffset(selBg), 0},
    {BLT_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	(char *)NULL, CheckBoxOffset(selFgColor), 0},
    {BLT_CONFIG_BOOLEAN, "-showvalue", "showValue", "ShowValue",
	DEF_CHECKBOX_SHOWVALUE, CheckBoxOffset(showValue), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

typedef struct {
    int refCount;			/* Usage reference count. */
    unsigned int flags;
    const char *name;			
    TreeViewStyleClass *classPtr;	/* Class-specific style routines. */
    Blt_HashEntry *hashPtr;
    Blt_ChainLink link;

    /* General style fields. */
    Tk_Cursor cursor;			/* X Cursor */
    TreeViewIcon icon;			/* If non-NULL, is a Tk_Image to be
					 * drawn in the cell. */  
    int gap;				/* # pixels gap between icon and
					 * text. */
    Blt_Font font;
    XColor *fgColor;			/* Normal foreground color of cell. */
    XColor *highlightFgColor;		/* Foreground color of cell when
					 * highlighted. */
    XColor *activeFgColor;		/* Foreground color of cell when
					 * active. */
    XColor *selFgColor;			/* Foreground color of a selected
					 * cell. If non-NULL, overrides
					 * default foreground color
					 * specification. */
    Blt_Background bg;			/* Normal background color of cell. */
    Blt_Background highlightBg;		/* Background color of cell when
					 * highlighted. */
    Blt_Background activeBg;		/* Background color of cell when
					 * active. */

    Blt_Background selBg;		/* Background color of a selected
					 * cell.  If non-NULL, overrides the
					 * default background color
					 * specification. */
    const char *validateCmd;

    /* ComboBox-specific fields */
    GC gc;
    GC highlightGC;
    GC activeGC;

    int borderWidth;			/* Width of outer border surrounding
					 * the entire box. */
    int relief;				/* Relief of outer border. */

    Blt_TreeKey key;			/* Actual data resides in this tree
					   value. */

    const char *choices;		/* List of available choices. */
    const char *choiceIcons;		/* List of icons associated with
					 * choices. */
    int scrollWidth;
    int arrow;
    int arrowWidth;
    int arrowBW;			/* Border width of arrow. */
    int arrowRelief;			/* Normal relief of arrow. */
} TreeViewComboBox;

#define DEF_COMBOBOX_BORDERWIDTH	"1"
#define DEF_COMBOBOX_ARROW_BORDERWIDTH	"1"
#define DEF_COMBOBOX_ARROW_RELIEF	"raised"
#define DEF_COMBOBOX_RELIEF		"flat"
#ifdef WIN32
#define DEF_COMBOBOX_CURSOR		"arrow"
#else
#define DEF_COMBOBOX_CURSOR		"hand2"
#endif /*WIN32*/


#define ComboBoxOffset(x)	Blt_Offset(TreeViewComboBox, x)

static Blt_ConfigSpec comboBoxSpecs[] =
{
    {BLT_CONFIG_BACKGROUND, "-activebackground", "activeBackground", 
	"ActiveBackground", DEF_STYLE_ACTIVE_BACKGROUND, 
	ComboBoxOffset(activeBg), 0},
    {BLT_CONFIG_SYNONYM, "-activebg", "activeBackground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-activefg", "activeFackground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_COLOR, "-activeforeground", "activeForeground", 
	"ActiveForeground", DEF_STYLE_ACTIVE_FOREGROUND, 
	ComboBoxOffset(activeFgColor), 0},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	(char *)NULL, ComboBoxOffset(bg), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 
	0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_COMBOBOX_BORDERWIDTH, ComboBoxOffset(borderWidth),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_RELIEF, "-arrowrelief", "arrowRelief", "ArrowRelief",
	DEF_COMBOBOX_ARROW_RELIEF, ComboBoxOffset(arrowRelief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-arrowborderwidth", "arrowBorderWidth", 
	"ArrowBorderWidth", DEF_COMBOBOX_ARROW_BORDERWIDTH, 
	ComboBoxOffset(arrowBW), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CURSOR, "-cursor", "cursor", "Cursor", DEF_COMBOBOX_CURSOR, 
	ComboBoxOffset(cursor), 0},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 
	0, 0},
    {BLT_CONFIG_FONT, "-font", "font", "Font", (char *)NULL, 
	ComboBoxOffset(font), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	(char *)NULL, ComboBoxOffset(fgColor), BLT_CONFIG_NULL_OK },
    {BLT_CONFIG_PIXELS_NNEG, "-gap", "gap", "Gap", DEF_STYLE_GAP, 
	ComboBoxOffset(gap), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BACKGROUND, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_STYLE_HIGHLIGHT_BACKGROUND, 
        ComboBoxOffset(highlightBg), BLT_CONFIG_COLOR_ONLY},
    {BLT_CONFIG_COLOR, "-highlightforeground", "highlightForeground", 
	"HighlightForeground", DEF_STYLE_HIGHLIGHT_FOREGROUND, 
	 ComboBoxOffset(highlightFgColor), 0},
    {BLT_CONFIG_SYNONYM, "-highlightbg", "highlightBackground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-highlightfg", "highlightForeground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {BLT_CONFIG_CUSTOM, "-icon", "icon", "Icon", (char *)NULL, 
	ComboBoxOffset(icon), BLT_CONFIG_NULL_OK, &bltTreeViewIconOption},
    {BLT_CONFIG_STRING, "-key", "key", "key", (char *)NULL, ComboBoxOffset(key),
	BLT_CONFIG_NULL_OK, 0},
    {BLT_CONFIG_RELIEF, "-relief", "relief", "Relief", DEF_COMBOBOX_RELIEF, 
	ComboBoxOffset(relief), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BACKGROUND, "-selectbackground", "selectBackground", 
	"Foreground", (char *)NULL, ComboBoxOffset(selBg), 0},
    {BLT_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	(char *)NULL, ComboBoxOffset(selFgColor), 0},
    {BLT_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

typedef TreeViewStyle *(StyleCreateProc)(TreeView *viewPtr,Blt_HashEntry *hPtr);

static StyleConfigProc ConfigureTextBox, ConfigureCheckBox, ConfigureComboBox;
static StyleCreateProc CreateTextBox, CreateCheckBox, CreateComboBox;
static StyleDrawProc DrawTextBox, DrawCheckBox, DrawComboBox;
static StyleEditProc EditTextBox, EditCheckBox, EditComboBox;
static StyleFreeProc FreeTextBox, FreeCheckBox, FreeComboBox;
static StyleMeasureProc MeasureTextBox, MeasureCheckBox, MeasureComboBox;
#ifdef notdef
static StylePickProc PickCheckBox;
#endif
static StylePickProc PickComboBox;

/*
 *---------------------------------------------------------------------------
 *
 * ObjToIcon --
 *
 *	Convert the name of an icon into a Tk image.
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
ObjToIcon(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Interpreter to report results */
    Tk_Window tkwin,			/* Not used. */
    Tcl_Obj *objPtr,			/* Tcl_Obj representing the value. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    TreeView *viewPtr = clientData;
    TreeViewIcon *iconPtr = (TreeViewIcon *)(widgRec + offset);
    TreeViewIcon icon;
    const char *string;
    int length;

    string = Tcl_GetStringFromObj(objPtr, &length);
    icon = NULL;
    if (length > 0) {
	icon = Blt_TreeView_GetIcon(viewPtr, Tcl_GetString(objPtr));
	if (icon == NULL) {
	    return TCL_ERROR;
	}
    }
    if (*iconPtr != NULL) {
	Blt_TreeView_FreeIcon(viewPtr, *iconPtr);
    }
    *iconPtr = icon;
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
IconToObj(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,			/* Not used. */
    char *widgRec,
    int offset,				/* Offset to field in structure */
    int flags)	
{
    TreeViewIcon icon = *(TreeViewIcon *)(widgRec + offset);

    if (icon == NULL) {
	return Tcl_NewStringObj("", -1);
    } else {
	return Tcl_NewStringObj(Blt_Image_Name((icon)->tkImage), -1);
    }
}

/*ARGSUSED*/
static void
FreeIcon(
    ClientData clientData,
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    TreeViewIcon *iconPtr = (TreeViewIcon *)(widgRec + offset);
    TreeView *viewPtr = clientData;

    if (*iconPtr != NULL) {
	Blt_TreeView_FreeIcon(viewPtr, *iconPtr);
	*iconPtr = NULL;
    }
}

static TreeViewStyleClass textBoxClass = {
    "TextBoxStyle",
    textBoxSpecs,
    ConfigureTextBox,
    MeasureTextBox,
    DrawTextBox,
    NULL,
    EditTextBox,
    FreeTextBox,
};

static TreeViewStyleClass checkBoxClass = {
    "CheckBoxStyle",
    checkBoxSpecs,
    ConfigureCheckBox,
    MeasureCheckBox,
    DrawCheckBox,
    NULL,
    EditCheckBox,
    FreeCheckBox,
};

static TreeViewStyleClass comboBoxClass = {
    "ComboBoxStyle", 
    comboBoxSpecs,
    ConfigureComboBox,
    MeasureComboBox,
    DrawComboBox,
    NULL,
    EditComboBox,
    FreeComboBox,
};

/*
 *---------------------------------------------------------------------------
 *
 * CreateTextBox --
 *
 *	Creates a "textbox" style.
 *
 * Results:
 *	A pointer to the new style structure.
 *
 *---------------------------------------------------------------------------
 */
static TreeViewStyle *
CreateTextBox(TreeView *viewPtr, Blt_HashEntry *hPtr)
{
    TreeViewTextBox *tbPtr;

    tbPtr = Blt_AssertCalloc(1, sizeof(TreeViewTextBox));
    tbPtr->classPtr = &textBoxClass;
    tbPtr->side = SIDE_LEFT;
    tbPtr->gap = STYLE_GAP;
    tbPtr->name = Blt_AssertStrdup(Blt_GetHashKey(&viewPtr->styleTable, hPtr));
    tbPtr->hashPtr = hPtr;
    tbPtr->link = NULL;
    tbPtr->flags = STYLE_TEXTBOX;
    tbPtr->refCount = 1;
    Blt_SetHashValue(hPtr, tbPtr);
    return (TreeViewStyle *)tbPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * ConfigureTextBox --
 *
 *	Configures a "textbox" style.  This routine performs generates the GCs
 *	required for a textbox style.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	GCs are created for the style.
 *
 *---------------------------------------------------------------------------
 */
static void
ConfigureTextBox(TreeView *viewPtr, TreeViewStyle *stylePtr)
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;

    gcMask = GCForeground | GCFont;
    gcValues.font = Blt_FontId(CHOOSE(viewPtr->font, tbPtr->font));

    /* Normal GC */
    gcValues.foreground = CHOOSE(viewPtr->fgColor, tbPtr->fgColor)->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (tbPtr->gc != NULL) {
	Tk_FreeGC(viewPtr->display, tbPtr->gc);
    }
    tbPtr->gc = newGC;

    /* Highlight GC  */
    gcValues.foreground = tbPtr->highlightFgColor->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (tbPtr->highlightGC != NULL) {
	Tk_FreeGC(viewPtr->display, tbPtr->highlightGC);
    }
    tbPtr->highlightGC = newGC;

    /* Active GC  */
    gcValues.foreground = tbPtr->activeFgColor->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (tbPtr->activeGC != NULL) {
	Tk_FreeGC(viewPtr->display, tbPtr->activeGC);
    }
    tbPtr->activeGC = newGC;
    tbPtr->flags |= STYLE_DIRTY;
}

/*
 *---------------------------------------------------------------------------
 *
 * MeasureTextBox --
 *
 *	Determines the space requirements for the "textbox" given the value to
 *	be displayed.  Depending upon whether an icon or text is displayed and
 *	their relative placements, this routine computes the space needed for
 *	the text entry.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The width and height fields of *valuePtr* are set with the computed
 *	dimensions.
 *
 *---------------------------------------------------------------------------
 */
static void
MeasureTextBox(
    TreeView *viewPtr, 
    TreeViewStyle *stylePtr, 
    TreeViewValue *valuePtr)
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;
    int iconWidth, iconHeight;
    int textWidth, textHeight;
    int gap;

    textWidth = textHeight = 0;
    iconWidth = iconHeight = 0;
    valuePtr->width = valuePtr->height = 0;

    if (tbPtr->icon != NULL) {
	iconWidth = TreeView_IconWidth(tbPtr->icon);
	iconHeight = TreeView_IconHeight(tbPtr->icon);
    } 
    if (valuePtr->textPtr != NULL) {
	Blt_Free(valuePtr->textPtr);
	valuePtr->textPtr = NULL;
    }
    if (valuePtr->fmtString != NULL) {	/* New string defined. */
	TextStyle ts;

	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, CHOOSE(viewPtr->font, tbPtr->font));
	valuePtr->textPtr = Blt_Ts_CreateLayout(valuePtr->fmtString, -1, &ts);
    } 
    gap = 0;
    if (valuePtr->textPtr != NULL) {
	textWidth = valuePtr->textPtr->width;
	textHeight = valuePtr->textPtr->height;
	if (tbPtr->icon != NULL) {
	    gap = tbPtr->gap;
	}
    }
    if (tbPtr->side & (SIDE_TOP | SIDE_BOTTOM)) {
	valuePtr->width = MAX(textWidth, iconWidth);
	valuePtr->height = iconHeight + gap + textHeight;
    } else {
	valuePtr->width = iconWidth + gap + textWidth;
	valuePtr->height = MAX(textHeight, iconHeight);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawTextBox --
 *
 *	Draws the "textbox" given the screen coordinates and the value to be
 *	displayed.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The textbox value is drawn.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawTextBox(TreeView *viewPtr, Drawable drawable, TreeViewEntry *entryPtr,
	    TreeViewValue *valuePtr, TreeViewStyle *stylePtr, int x, int y)
{
    TreeViewColumn *colPtr;
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;
    int iconX, iconY, iconWidth, iconHeight;
    int textX, textY, textWidth, textHeight;
    int gap, columnWidth;
    Blt_Background bg;
    XColor *fgColor;

    colPtr = valuePtr->columnPtr;

    if (stylePtr->flags & STYLE_HIGHLIGHT) {
	bg = tbPtr->highlightBg;
	fgColor = tbPtr->highlightFgColor;
    } else {
	if (tbPtr->bg != NULL) {
	    bg = tbPtr->bg;
	} else {
	    bg = ((viewPtr->altBg != NULL) && (entryPtr->flatIndex & 0x1)) 
		? viewPtr->altBg : viewPtr->bg;
	}
	fgColor = CHOOSE(viewPtr->fgColor, tbPtr->fgColor);
    }
    if (Blt_TreeView_EntryIsSelected(viewPtr, entryPtr)) {
	bg = (stylePtr->selBg != NULL) ? stylePtr->selBg : viewPtr->selBg;
    } 
    /*
     * Draw the active or normal background color over the entire label area.
     * This includes both the tab's text and image.  The rectangle should be 2
     * pixels wider/taller than this area. So if the label consists of just an
     * image, we get an halo around the image when the tab is active.
     */
    if (bg != NULL) {
	Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, bg, x, y, 
		colPtr->width, entryPtr->height + 1, 0, TK_RELIEF_FLAT);
    }

    columnWidth = colPtr->width - 
	(2 * colPtr->borderWidth + PADDING(colPtr->pad));
    if (columnWidth > valuePtr->width) {
	switch(colPtr->justify) {
	case TK_JUSTIFY_RIGHT:
	    x += (columnWidth - valuePtr->width);
	    break;
	case TK_JUSTIFY_CENTER:
	    x += (columnWidth - valuePtr->width) / 2;
	    break;
	case TK_JUSTIFY_LEFT:
	    break;
	}
    }

    textX = textY = iconX = iconY = 0;	/* Suppress compiler warning. */
    
    iconWidth = iconHeight = 0;
    if (tbPtr->icon != NULL) {
	iconWidth = TreeView_IconWidth(tbPtr->icon);
	iconHeight = TreeView_IconHeight(tbPtr->icon);
    }
    textWidth = textHeight = 0;
    if (valuePtr->textPtr != NULL) {
	textWidth = valuePtr->textPtr->width;
	textHeight = valuePtr->textPtr->height;
    }
    gap = 0;
    if ((tbPtr->icon != NULL) && (valuePtr->textPtr != NULL)) {
	gap = tbPtr->gap;
    }
    switch (tbPtr->side) {
    case SIDE_RIGHT:
	textX = x;
	textY = y + (entryPtr->height - textHeight) / 2;
	iconX = textX + textWidth + gap;
	iconY = y + (entryPtr->height - iconHeight) / 2;
	break;
    case SIDE_LEFT:
	iconX = x;
	iconY = y + (entryPtr->height - iconHeight) / 2;
	textX = iconX + iconWidth + gap;
	textY = y + (entryPtr->height - textHeight) / 2;
	break;
    case SIDE_TOP:
	iconY = y;
	iconX = x + (columnWidth - iconWidth) / 2;
	textY = iconY + iconHeight + gap;
	textX = x + (columnWidth - textWidth) / 2;
	break;
    case SIDE_BOTTOM:
	textY = y;
	textX = x + (columnWidth - textWidth) / 2;
	iconY = textY + textHeight + gap;
	iconX = x + (columnWidth - iconWidth) / 2;
	break;
    }
    if (tbPtr->icon != NULL) {
	Tk_RedrawImage(TreeView_IconBits(tbPtr->icon), 0, 0, iconWidth, 
		       iconHeight, drawable, iconX, iconY);
    }
    if (valuePtr->textPtr != NULL) {
	TextStyle ts;
	XColor *color;
	Blt_Font font;
	int xMax;

	font = CHOOSE(viewPtr->font, tbPtr->font);
	if (Blt_TreeView_EntryIsSelected(viewPtr, entryPtr)) {
	    if (stylePtr->selFgColor != NULL) {
		color = stylePtr->selFgColor;
	    } else {
		color = viewPtr->selFgColor;
	    }
	} else if (entryPtr->color != NULL) {
	    color = entryPtr->color;
	} else {
	    color = fgColor;
	}
	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, font);
	Blt_Ts_SetForeground(ts, color);
	xMax = colPtr->width - colPtr->titleBW - colPtr->pad.side2;
	Blt_Ts_SetMaxLength(ts, xMax);
	Blt_Ts_DrawLayout(viewPtr->tkwin, drawable, valuePtr->textPtr, &ts, 
		textX, textY);
    }
    stylePtr->flags &= ~STYLE_DIRTY;
}

/*
 *---------------------------------------------------------------------------
 *
 * EditCombobox --
 *
 *	Edits the "combobox".
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The checkbox value is drawn.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
EditTextBox(TreeView *viewPtr, TreeViewEntry *entryPtr, TreeViewColumn *colPtr,
	    TreeViewStyle *stylePtr)
{
    return Blt_TreeView_CreateTextbox(viewPtr, entryPtr, colPtr);
}


/*
 *---------------------------------------------------------------------------
 *
 * FreeTextBox --
 *
 *	Releases resources allocated for the textbox. The resources freed by
 *	this routine are specific only to the "textbox".  Other resources
 *	(common to all styles) are freed in the Blt_TreeView_FreeStyle
 *	routine.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	GCs allocated for the textbox are freed.
 *
 *---------------------------------------------------------------------------
 */
static void
FreeTextBox(TreeView *viewPtr, TreeViewStyle *stylePtr)
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;

    if (tbPtr->highlightGC != NULL) {
	Tk_FreeGC(viewPtr->display, tbPtr->highlightGC);
    }
    if (tbPtr->activeGC != NULL) {
	Tk_FreeGC(viewPtr->display, tbPtr->activeGC);
    }
    if (tbPtr->gc != NULL) {
	Tk_FreeGC(viewPtr->display, tbPtr->gc);
    }
    if (tbPtr->icon != NULL) {
	Blt_TreeView_FreeIcon(viewPtr, tbPtr->icon);
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * CreateCheckbox --
 *
 *	Creates a "checkbox" style.
 *
 * Results:
 *	A pointer to the new style structure.
 *
 *---------------------------------------------------------------------------
 */
static TreeViewStyle *
CreateCheckBox(TreeView *viewPtr, Blt_HashEntry *hPtr)
{
    TreeViewCheckBox *cbPtr;

    cbPtr = Blt_AssertCalloc(1, sizeof(TreeViewCheckBox));
    cbPtr->classPtr = &checkBoxClass;
    cbPtr->gap = 4;
    cbPtr->size = 18;
    cbPtr->lineWidth = 2;
    cbPtr->showValue = TRUE;
    cbPtr->name = Blt_AssertStrdup(Blt_GetHashKey(&viewPtr->styleTable, hPtr));
    cbPtr->hashPtr = hPtr;
    cbPtr->link = NULL;
    cbPtr->flags = STYLE_CHECKBOX;
    cbPtr->refCount = 1;
    Blt_SetHashValue(hPtr, cbPtr);
    return (TreeViewStyle *)cbPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * ConfigureCheckbox --
 *
 *	Configures a "checkbox" style.  This routine performs generates the
 *	GCs required for a checkbox style.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	GCs are created for the style.
 *
 *---------------------------------------------------------------------------
 */
static void
ConfigureCheckBox(TreeView *viewPtr, TreeViewStyle *stylePtr)
{
    GC newGC;
    TreeViewCheckBox *cbPtr = (TreeViewCheckBox *)stylePtr;
    XColor *bgColor;
    XGCValues gcValues;
    unsigned long gcMask;

    gcMask = GCForeground | GCBackground | GCFont;
    gcValues.font = Blt_FontId(CHOOSE(viewPtr->font, cbPtr->font));
    bgColor = Blt_BackgroundBorderColor(CHOOSE(viewPtr->bg, cbPtr->bg));

    gcValues.background = bgColor->pixel;
    gcValues.foreground = CHOOSE(viewPtr->fgColor, cbPtr->fgColor)->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->gc != NULL) {
	Tk_FreeGC(viewPtr->display, cbPtr->gc);
    }
    cbPtr->gc = newGC;
    gcValues.background = Blt_BackgroundBorderColor(cbPtr->highlightBg)->pixel;
    gcValues.foreground = cbPtr->highlightFgColor->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->highlightGC != NULL) {
	Tk_FreeGC(viewPtr->display, cbPtr->highlightGC);
    }
    cbPtr->highlightGC = newGC;

    gcValues.background = Blt_BackgroundBorderColor(cbPtr->activeBg)->pixel;
    gcValues.foreground = cbPtr->activeFgColor->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->activeGC != NULL) {
	Tk_FreeGC(viewPtr->display, cbPtr->activeGC);
    }
    cbPtr->activeGC = newGC;

    cbPtr->flags |= STYLE_DIRTY;
}

/*
 *---------------------------------------------------------------------------
 *
 * MeasureCheckbox --
 *
 *	Determines the space requirements for the "checkbox" given the value
 *	to be displayed.  Depending upon whether an icon or text is displayed
 *	and their relative placements, this routine computes the space needed
 *	for the text entry.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The width and height fields of *valuePtr* are set with the
 *	computed dimensions.
 *
 *---------------------------------------------------------------------------
 */
static void
MeasureCheckBox(TreeView *viewPtr, TreeViewStyle *stylePtr, 
		TreeViewValue *valuePtr)
{
    TreeViewCheckBox *cbPtr = (TreeViewCheckBox *)stylePtr;
    int iconWidth, iconHeight;
    int textWidth, textHeight;
    int gap;
    int boxWidth, boxHeight;

    boxWidth = boxHeight = ODD(cbPtr->size);

    textWidth = textHeight = iconWidth = iconHeight = 0;
    valuePtr->width = valuePtr->height = 0;
    if (cbPtr->icon != NULL) {
	iconWidth = TreeView_IconWidth(cbPtr->icon);
	iconHeight = TreeView_IconHeight(cbPtr->icon);
    } 
    if (cbPtr->onPtr != NULL) {
	Blt_Free(cbPtr->onPtr);
	cbPtr->onPtr = NULL;
    }
    if (cbPtr->offPtr != NULL) {
	Blt_Free(cbPtr->offPtr);
	cbPtr->offPtr = NULL;
    }
    gap = 0;
    if (cbPtr->showValue) {
	TextStyle ts;
	const char *string;

	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, CHOOSE(viewPtr->font, cbPtr->font));
	string = (cbPtr->onValue != NULL) ? cbPtr->onValue : 
	    valuePtr->fmtString;
	cbPtr->onPtr = Blt_Ts_CreateLayout(string, -1, &ts);
	string = (cbPtr->offValue != NULL) ? cbPtr->offValue : 
	    valuePtr->fmtString;
	cbPtr->offPtr = Blt_Ts_CreateLayout(string, -1, &ts);
	textWidth = MAX(cbPtr->offPtr->width, cbPtr->onPtr->width);
	textHeight = MAX(cbPtr->offPtr->height, cbPtr->onPtr->height);
	if (cbPtr->icon != NULL) {
	    gap = cbPtr->gap;
	}
    }
    valuePtr->width = cbPtr->gap * 2 + boxWidth + iconWidth + gap + textWidth;
    valuePtr->height = MAX3(boxHeight, textHeight, iconHeight) | 0x1;
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawCheckbox --
 *
 *	Draws the "checkbox" given the screen coordinates and the
 *	value to be displayed.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The checkbox value is drawn.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawCheckBox(TreeView *viewPtr, Drawable drawable, TreeViewEntry *entryPtr, 
	     TreeViewValue *valuePtr, TreeViewStyle *stylePtr, int x, int y)
{
    Blt_Background bg;
    Blt_Font font;
    TextLayout *textPtr;
    TreeViewCheckBox *cbPtr = (TreeViewCheckBox *)stylePtr;
    TreeViewColumn *colPtr;
    XColor *fgColor;
    int bool;
    int borderWidth, relief;
    int gap, columnWidth;
    int iconX, iconY, iconWidth, iconHeight;
    int textX, textY, textHeight;
    int xBox, yBox, boxWidth, boxHeight;

    font = CHOOSE(viewPtr->font, cbPtr->font);
    colPtr = valuePtr->columnPtr;
    borderWidth = 0;
    relief = TK_RELIEF_FLAT;
    if (valuePtr == viewPtr->activeValuePtr) {
	bg = cbPtr->activeBg;
	fgColor = cbPtr->activeFgColor;
	borderWidth = 1;
	relief = TK_RELIEF_RAISED;
    } else if (stylePtr->flags & STYLE_HIGHLIGHT) {
	bg = cbPtr->highlightBg;
	fgColor = cbPtr->highlightFgColor;
    } else {
	/* If a background was specified, override the current background.
	 * Otherwise, use the standard background taking into consideration if
	 * its the odd or even color. */
	if (cbPtr->bg != NULL) {
	    bg = cbPtr->bg;
	} else {
	    bg = ((viewPtr->altBg != NULL) && (entryPtr->flatIndex & 0x1)) 
		? viewPtr->altBg : viewPtr->bg;
	}
	fgColor = CHOOSE(viewPtr->fgColor, cbPtr->fgColor);
    }
    columnWidth = colPtr->width - PADDING(colPtr->pad);

    /*
     * Draw the active or normal background color over the entire label area.
     * This includes both the tab's text and image.  The rectangle should be 2
     * pixels wider/taller than this area. So if the label consists of just an
     * image, we get an halo around the image when the tab is active.
     */
    if (Blt_TreeView_EntryIsSelected(viewPtr, entryPtr)) {
	bg = CHOOSE(viewPtr->selBg, stylePtr->selBg);
    }
    Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, bg, x, y+1, 
	columnWidth, entryPtr->height - 2, borderWidth, relief);

    if (columnWidth > valuePtr->width) {
	switch(colPtr->justify) {
	case TK_JUSTIFY_RIGHT:
	    x += (columnWidth - valuePtr->width);
	    break;
	case TK_JUSTIFY_CENTER:
	    x += (columnWidth - valuePtr->width) / 2;
	    break;
	case TK_JUSTIFY_LEFT:
	    break;
	}
    }

    bool = (strcmp(valuePtr->fmtString, cbPtr->onValue) == 0);
    textPtr = (bool) ? cbPtr->onPtr : cbPtr->offPtr;

    /*
     * Draw the box and check. 
     *
     *		+-----------+
     *		|           |
     *		|         * |
     *          |        *  |
     *          | *     *   |
     *          |  *   *    |
     *          |   * *     |
     *		|    *      |
     *		+-----------+
     */
    boxWidth = boxHeight = ODD(cbPtr->size);
    xBox = x + cbPtr->gap;
    yBox = y + (entryPtr->height - boxHeight) / 2;
    {
	Blt_Picture picture;
	
	if (0) {
	    if (cbPtr->disabledPicture == NULL) {
		cbPtr->disabledPicture = Blt_PaintCheckbox(boxWidth, boxHeight, 
			cbPtr->fillColor, cbPtr->boxColor, cbPtr->boxColor, 
			bool);
	    } 
	    picture = cbPtr->disabledPicture;
	} else if (bool) {
	    if (cbPtr->selectedPicture == NULL) {
		cbPtr->selectedPicture = Blt_PaintCheckbox(boxWidth, boxHeight, 
			cbPtr->fillColor, cbPtr->boxColor, cbPtr->checkColor, 
			TRUE);
	    } 
	    picture = cbPtr->selectedPicture;
	} else {
	    if (cbPtr->normalPicture == NULL) {
		cbPtr->normalPicture = Blt_PaintCheckbox(boxWidth, boxHeight, 
			cbPtr->fillColor, cbPtr->boxColor, cbPtr->checkColor, 
			FALSE);
	    } 
	    picture = cbPtr->normalPicture;
	}
	if (cbPtr->painter == NULL) {
	    cbPtr->painter = Blt_GetPainter(viewPtr->tkwin, 1.0);
	}
	Blt_PaintPicture(cbPtr->painter, drawable, picture, 0, 0, 
			 boxWidth, boxHeight, xBox, yBox, 0);
    }
    iconWidth = iconHeight = 0;
    if (cbPtr->icon != NULL) {
	iconWidth = TreeView_IconWidth(cbPtr->icon);
	iconHeight = TreeView_IconHeight(cbPtr->icon);
    }
    textHeight = 0;
    gap = 0;
    if (cbPtr->showValue) {
	textHeight = textPtr->height;
	if (cbPtr->icon != NULL) {
	    gap = cbPtr->gap;
	}
    }
    x = xBox + boxWidth + cbPtr->gap;

    /* The icon sits to the left of the text. */
    iconX = x;
    iconY = y + (entryPtr->height - iconHeight) / 2;
    textX = iconX + iconWidth + gap;
    textY = y + (entryPtr->height - textHeight) / 2;
    if (cbPtr->icon != NULL) {
	Tk_RedrawImage(TreeView_IconBits(cbPtr->icon), 0, 0, iconWidth, 
		       iconHeight, drawable, iconX, iconY);
    }
    if ((cbPtr->showValue) && (textPtr != NULL)) {
	TextStyle ts;
	XColor *color;
	int xMax;

	if (Blt_TreeView_EntryIsSelected(viewPtr, entryPtr)) {
	    if (stylePtr->selFgColor != NULL) {
		color = stylePtr->selFgColor;
	    } else {
		color = viewPtr->selFgColor;
	    }
	} else if (entryPtr->color != NULL) {
	    color = entryPtr->color;
	} else {
	    color = fgColor;
	}
	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, font);
	Blt_Ts_SetForeground(ts, color);
	Blt_Ts_SetMaxLength(ts, xMax - textX);
	xMax = SCREENX(viewPtr, colPtr->worldX) + colPtr->width - 
	    colPtr->titleBW - colPtr->pad.side2;
	Blt_Ts_DrawLayout(viewPtr->tkwin, drawable, textPtr, &ts, textX, textY);
    }
    stylePtr->flags &= ~STYLE_DIRTY;
}

#ifdef notdef
/*
 *---------------------------------------------------------------------------
 *
 * PickCheckbox --
 *
 *	Draws the "checkbox" given the screen coordinates and the value to be
 *	displayed.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The checkbox value is drawn.
 *
 *---------------------------------------------------------------------------
 */
static int
PickCheckBox(TreeViewEntry *entryPtr, TreeViewValue *valuePtr, 
	     TreeViewStyle *stylePtr, int worldX, int worldY)
{
    TreeViewColumn *colPtr;
    TreeViewCheckBox *cbPtr = (TreeViewCheckBox *)stylePtr;
    int columnWidth;
    int x, y, width, height;

    colPtr = valuePtr->columnPtr;
    columnWidth = colPtr->width - 
	(2 * colPtr->borderWidth + PADDING(colPtr->pad));
    if (columnWidth > valuePtr->width) {
	switch(colPtr->justify) {
	case TK_JUSTIFY_RIGHT:
	    worldX += (columnWidth - valuePtr->width);
	    break;
	case TK_JUSTIFY_CENTER:
	    worldX += (columnWidth - valuePtr->width) / 2;
	    break;
	case TK_JUSTIFY_LEFT:
	    break;
	}
    }
    width = height = ODD(cbPtr->size) + 2 * cbPtr->lineWidth;
    x = colPtr->worldX + colPtr->pad.side1 + cbPtr->gap - cbPtr->lineWidth;
    y = entryPtr->worldY + (entryPtr->height - height) / 2;
    if ((worldX >= x) && (worldX < (x + width)) && 
	(worldY >= y) && (worldY < (y + height))) {
	return TRUE;
    }
    return FALSE;
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * EditCheckbox --
 *
 *	Edits the "checkbox".
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The checkbox value is drawn.
 *
 *---------------------------------------------------------------------------
 */
static int
EditCheckBox(TreeView *viewPtr, TreeViewEntry *entryPtr, 
	     TreeViewColumn *colPtr, TreeViewStyle *stylePtr)
{
    TreeViewCheckBox *cbPtr = (TreeViewCheckBox *)stylePtr;
    Tcl_Obj *objPtr;

    if (Blt_Tree_GetValueByKey(viewPtr->interp, viewPtr->tree, entryPtr->node, 
	colPtr->key, &objPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (strcmp(Tcl_GetString(objPtr), cbPtr->onValue) == 0) {
	objPtr = Tcl_NewStringObj(cbPtr->offValue, -1);
    } else {
	objPtr = Tcl_NewStringObj(cbPtr->onValue, -1);
    }
    entryPtr->flags |= ENTRY_DIRTY;
    viewPtr->flags |= (DIRTY | LAYOUT_PENDING | SCROLL_PENDING);
    if (Blt_Tree_SetValueByKey(viewPtr->interp, viewPtr->tree, entryPtr->node, 
	colPtr->key, objPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (cbPtr->cmdObjPtr != NULL) {
	Tcl_Obj *cmdObjPtr;
	int result;

	cmdObjPtr = Tcl_DuplicateObj(cbPtr->cmdObjPtr);
	Tcl_ListObjAppendElement(viewPtr->interp, cmdObjPtr, 
		Tcl_NewLongObj(Blt_Tree_NodeId(entryPtr->node)));
	Tcl_IncrRefCount(cmdObjPtr);
	result = Tcl_EvalObjEx(viewPtr->interp, cmdObjPtr, TCL_EVAL_GLOBAL);
	Tcl_DecrRefCount(cmdObjPtr);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * FreeCheckbox --
 *
 *	Releases resources allocated for the checkbox. The resources freed by
 *	this routine are specific only to the "checkbox".  Other resources
 *	(common to all styles) are freed in the Blt_TreeView_FreeStyle
 *	routine.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	GCs allocated for the checkbox are freed.
 *
 *---------------------------------------------------------------------------
 */
static void
FreeCheckBox(TreeView *viewPtr, TreeViewStyle *stylePtr)
{
    TreeViewCheckBox *cbPtr = (TreeViewCheckBox *)stylePtr;

    if (cbPtr->highlightGC != NULL) {
	Tk_FreeGC(viewPtr->display, cbPtr->highlightGC);
    }
    if (cbPtr->activeGC != NULL) {
	Tk_FreeGC(viewPtr->display, cbPtr->activeGC);
    }
    if (cbPtr->gc != NULL) {
	Tk_FreeGC(viewPtr->display, cbPtr->gc);
    }
    if (cbPtr->icon != NULL) {
	Blt_TreeView_FreeIcon(viewPtr, cbPtr->icon);
    }
    if (cbPtr->offPtr != NULL) {
	Blt_Free(cbPtr->offPtr);
    }
    if (cbPtr->onPtr != NULL) {
	Blt_Free(cbPtr->onPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * CreateComboBox --
 *
 *	Creates a "combobox" style.
 *
 * Results:
 *	A pointer to the new style structure.
 *
 *---------------------------------------------------------------------------
 */
static TreeViewStyle *
CreateComboBox(TreeView *viewPtr, Blt_HashEntry *hPtr)
{
    TreeViewComboBox *cbPtr;

    cbPtr = Blt_AssertCalloc(1, sizeof(TreeViewComboBox));
    cbPtr->classPtr = &comboBoxClass;
    cbPtr->gap = STYLE_GAP;
    cbPtr->arrowRelief = TK_RELIEF_RAISED;
    cbPtr->arrowBW = 1;
    cbPtr->borderWidth = 1;
    cbPtr->relief = TK_RELIEF_FLAT;
    cbPtr->name = Blt_AssertStrdup(Blt_GetHashKey(&viewPtr->styleTable, hPtr));
    cbPtr->hashPtr = hPtr;
    cbPtr->link = NULL;
    cbPtr->flags = STYLE_COMBOBOX;
    cbPtr->refCount = 1;
    Blt_SetHashValue(hPtr, cbPtr);
    return (TreeViewStyle *)cbPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * ConfigureComboBox --
 *
 *	Configures a "combobox" style.  This routine performs generates the
 *	GCs required for a combobox style.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	GCs are created for the style.
 *
 *---------------------------------------------------------------------------
 */
static void
ConfigureComboBox(TreeView *viewPtr, TreeViewStyle *stylePtr)
{
    GC newGC;
    TreeViewComboBox *cbPtr = (TreeViewComboBox *)stylePtr;
    XColor *bgColor;
    XGCValues gcValues;
    unsigned long gcMask;

    gcValues.font = Blt_FontId(CHOOSE(viewPtr->font, cbPtr->font));
    bgColor = Blt_BackgroundBorderColor(CHOOSE(viewPtr->bg, cbPtr->bg));
    gcMask = GCForeground | GCBackground | GCFont;

    /* Normal foreground */
    gcValues.background = bgColor->pixel;
    gcValues.foreground = CHOOSE(viewPtr->fgColor, cbPtr->fgColor)->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->gc != NULL) {
	Tk_FreeGC(viewPtr->display, cbPtr->gc);
    }
    cbPtr->gc = newGC;

    /* Highlight foreground */
    gcValues.background = Blt_BackgroundBorderColor(cbPtr->highlightBg)->pixel;
    gcValues.foreground = cbPtr->highlightFgColor->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->highlightGC != NULL) {
	Tk_FreeGC(viewPtr->display, cbPtr->highlightGC);
    }
    cbPtr->highlightGC = newGC;

    /* Active foreground */
    gcValues.background = Blt_BackgroundBorderColor(cbPtr->activeBg)->pixel;
    gcValues.foreground = cbPtr->activeFgColor->pixel;
    newGC = Tk_GetGC(viewPtr->tkwin, gcMask, &gcValues);
    if (cbPtr->activeGC != NULL) {
	Tk_FreeGC(viewPtr->display, cbPtr->activeGC);
    }
    cbPtr->activeGC = newGC;
    cbPtr->flags |= STYLE_DIRTY;
}

/*
 *---------------------------------------------------------------------------
 *
 * MeasureComboBox --
 *
 *	Determines the space requirements for the "combobox" given the value
 *	to be displayed.  Depending upon whether an icon or text is displayed
 *	and their relative placements, this routine computes the space needed
 *	for the text entry.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The width and height fields of *valuePtr* are set with the computed
 *	dimensions.
 *
 *---------------------------------------------------------------------------
 */
static void
MeasureComboBox(TreeView *viewPtr, TreeViewStyle *stylePtr, 
		TreeViewValue *valuePtr)
{
    TreeViewComboBox *cbPtr = (TreeViewComboBox *)stylePtr;
    int iconWidth, iconHeight;
    int textWidth, textHeight;
    int gap;
    Blt_Font font;

    textWidth = textHeight = 0;
    iconWidth = iconHeight = 0;
    valuePtr->width = valuePtr->height = 0;

    if (cbPtr->icon != NULL) {
	iconWidth = TreeView_IconWidth(cbPtr->icon);
	iconHeight = TreeView_IconHeight(cbPtr->icon);
    } 
    if (valuePtr->textPtr != NULL) {
	Blt_Free(valuePtr->textPtr);
	valuePtr->textPtr = NULL;
    }
    font = CHOOSE(viewPtr->font, cbPtr->font);
    if (valuePtr->fmtString != NULL) {	/* New string defined. */
	TextStyle ts;

	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, font);
	valuePtr->textPtr = Blt_Ts_CreateLayout(valuePtr->fmtString, -1, &ts);
    } 
    gap = 0;
    if (valuePtr->textPtr != NULL) {
	textWidth = valuePtr->textPtr->width;
	textHeight = valuePtr->textPtr->height;
	if (cbPtr->icon != NULL) {
	    gap = cbPtr->gap;
	}
    }
    cbPtr->arrowWidth = Blt_TextWidth(font, "0", 1);
    cbPtr->arrowWidth += 2 * cbPtr->arrowBW;
    valuePtr->width = 2 * cbPtr->borderWidth + iconWidth + 4 * gap + 
	cbPtr->arrowWidth + textWidth;
    valuePtr->height = MAX(textHeight, iconHeight) + 2 * cbPtr->borderWidth;
}


/*
 *---------------------------------------------------------------------------
 *
 * DrawComboBox --
 *
 *	Draws the "combobox" given the screen coordinates and the
 *	value to be displayed.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The combobox value is drawn.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawComboBox(TreeView *viewPtr, Drawable drawable, TreeViewEntry *entryPtr,
	     TreeViewValue *valuePtr, TreeViewStyle *stylePtr, int x, int y)
{
    Blt_Background bg;
    TreeViewColumn *colPtr;
    TreeViewComboBox *cbPtr = (TreeViewComboBox *)stylePtr;
    XColor *fgColor;
    int arrowX, arrowY;
    int gap, columnWidth;
    int iconX, iconY, iconWidth, iconHeight;
    int textX, textY, textHeight;
    int relief;
    int borderWidth;

    borderWidth = 0;
    relief = TK_RELIEF_FLAT;
    colPtr = valuePtr->columnPtr;
    if (valuePtr == viewPtr->activeValuePtr) {
	bg = cbPtr->activeBg;
	fgColor = cbPtr->activeFgColor;
	borderWidth = 1;
	relief = TK_RELIEF_RAISED;
    } else if (stylePtr->flags & STYLE_HIGHLIGHT) {
	bg = cbPtr->highlightBg;
	fgColor = cbPtr->highlightFgColor;
    } else {
	/* If a background was specified, override the current background.
	 * Otherwise, use the standard background taking into consideration if
	 * its the odd or even color. */
	if (cbPtr->bg != NULL) {
	    bg = cbPtr->bg;
	} else {
	    bg = ((viewPtr->altBg != NULL) && (entryPtr->flatIndex & 0x1)) 
		? viewPtr->altBg : viewPtr->bg;
	}
	fgColor = CHOOSE(viewPtr->fgColor, cbPtr->fgColor);
    }

    columnWidth = colPtr->width - PADDING(colPtr->pad);
    /* if (valuePtr == viewPtr->activeValuePtr) */ {
	/*
	 * Draw the active or normal background color over the entire label
	 * area.  This includes both the tab's text and image.  The rectangle
	 * should be 2 pixels wider/taller than this area. So if the label
	 * consists of just an image, we get an halo around the image when the
	 * tab is active.
	 */
	if (Blt_TreeView_EntryIsSelected(viewPtr, entryPtr)) {
	    if (stylePtr->selBg != NULL) {
		bg = stylePtr->selBg;
	    } else {
		bg = viewPtr->selBg;
	    }
	    Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, bg, x, y+1, 
		columnWidth, entryPtr->height - 2, borderWidth, relief);
	} else {
	    Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, bg, x, y+1, 
		columnWidth, entryPtr->height - 2, borderWidth, relief);
	}
    }   
    if (Blt_TreeView_EntryIsSelected(viewPtr, entryPtr)) {
	fgColor = viewPtr->selFgColor;
    }
    arrowX = x + colPtr->width;
    arrowX -= colPtr->pad.side2 + cbPtr->borderWidth  + cbPtr->arrowWidth + 
	cbPtr->gap;
    arrowY = y;

    if (columnWidth > valuePtr->width) {
	switch(colPtr->justify) {
	case TK_JUSTIFY_RIGHT:
	    x += (columnWidth - valuePtr->width);
	    break;
	case TK_JUSTIFY_CENTER:
	    x += (columnWidth - valuePtr->width) / 2;
	    break;
	case TK_JUSTIFY_LEFT:
	    break;
	}
    }

#ifdef notdef
    textX = textY = iconX = iconY = 0;	/* Suppress compiler warning. */
#endif
    
    iconWidth = iconHeight = 0;
    if (cbPtr->icon != NULL) {
	iconWidth = TreeView_IconWidth(cbPtr->icon);
	iconHeight = TreeView_IconHeight(cbPtr->icon);
    }
    textHeight = 0;
    if (valuePtr->textPtr != NULL) {
	textHeight = valuePtr->textPtr->height;
    }
    gap = 0;
    if ((cbPtr->icon != NULL) && (valuePtr->textPtr != NULL)) {
	gap = cbPtr->gap;
    }

    iconX = x + gap;
    iconY = y + (entryPtr->height - iconHeight) / 2;
    textX = iconX + iconWidth + gap;
    textY = y + (entryPtr->height - textHeight) / 2;

    if (cbPtr->icon != NULL) {
	Tk_RedrawImage(TreeView_IconBits(cbPtr->icon), 0, 0, iconWidth, 
	       iconHeight, drawable, iconX, iconY);
    }
    if (valuePtr->textPtr != NULL) {
	TextStyle ts;
	XColor *color;
	Blt_Font font;
	int xMax;
	
	font = CHOOSE(viewPtr->font, cbPtr->font);
	if (Blt_TreeView_EntryIsSelected(viewPtr, entryPtr)) {
	    color = CHOOSE(viewPtr->selFgColor, stylePtr->selFgColor);
	} else if (entryPtr->color != NULL) {
	    color = entryPtr->color;
	} else {
	    color = fgColor;
	}
	Blt_Ts_InitStyle(ts);
	Blt_Ts_SetFont(ts, font);
	Blt_Ts_SetForeground(ts, color);
	Blt_Ts_SetMaxLength(ts, xMax - textX);
	xMax = SCREENX(viewPtr, colPtr->worldX) + colPtr->width - 
	    colPtr->titleBW - colPtr->pad.side2 - cbPtr->arrowWidth;
	Blt_Ts_DrawLayout(viewPtr->tkwin, drawable, valuePtr->textPtr, &ts, 
		textX, textY);
    }
    if (valuePtr == viewPtr->activeValuePtr) {
	bg = stylePtr->activeBg;
    } else {
	bg = colPtr->titleBg;
#ifdef notdef
	bg = CHOOSE(viewPtr->bg, cbPtr->bg);
#endif
    }
#ifdef notdef
    Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, bg, arrowX, 
	arrowY + cbPtr->borderWidth, cbPtr->arrowWidth, 
	entryPtr->height - 2 * cbPtr->borderWidth, cbPtr->arrowBW, 
	cbPtr->arrowRelief); 
#endif
    Blt_DrawArrow(viewPtr->display, drawable, fgColor, arrowX, arrowY - 1, 
	cbPtr->arrowWidth, entryPtr->height, cbPtr->arrowBW, ARROW_DOWN);
    stylePtr->flags &= ~STYLE_DIRTY;
}

/*
 *---------------------------------------------------------------------------
 *
 * PickCombobox --
 *
 *	Draws the "checkbox" given the screen coordinates and the
 *	value to be displayed.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The checkbox value is drawn.
 *
 *---------------------------------------------------------------------------
 */
static int
PickComboBox(TreeViewEntry *entryPtr, TreeViewValue *valuePtr,
	     TreeViewStyle *stylePtr, int worldX, int worldY)
{
    TreeViewColumn *colPtr;
    TreeViewComboBox *cbPtr = (TreeViewComboBox *)stylePtr;
    int x, y, width, height;

    colPtr = valuePtr->columnPtr;
    width = colPtr->width;
    height = entryPtr->height - 4;
    x = colPtr->worldX + cbPtr->borderWidth;
    y = entryPtr->worldY + cbPtr->borderWidth;
    if ((worldX >= x) && (worldX < (x + width)) && 
	(worldY >= y) && (worldY < (y + height))) {
	return TRUE;
    }
    return FALSE;
}

/*
 *---------------------------------------------------------------------------
 *
 * EditCombobox --
 *
 *	Edits the "combobox".
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The checkbox value is drawn.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
EditComboBox(
    TreeView *viewPtr,
    TreeViewEntry *entryPtr,
    TreeViewColumn *colPtr,
    TreeViewStyle *stylePtr)		/* Not used. */
{
    return Blt_TreeView_CreateTextbox(viewPtr, entryPtr, colPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * FreeComboBox --
 *
 *	Releases resources allocated for the combobox. The resources freed by
 *	this routine are specific only to the "combobox".  Other resources
 *	(common to all styles) are freed in the Blt_TreeView_FreeStyle
 *	routine.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	GCs allocated for the combobox are freed.
 *
 *---------------------------------------------------------------------------
 */
static void
FreeComboBox(TreeView *viewPtr, TreeViewStyle *stylePtr)
{
    TreeViewComboBox *cbPtr = (TreeViewComboBox *)stylePtr;

    if (cbPtr->highlightGC != NULL) {
	Tk_FreeGC(viewPtr->display, cbPtr->highlightGC);
    }
    if (cbPtr->activeGC != NULL) {
	Tk_FreeGC(viewPtr->display, cbPtr->activeGC);
    }
    if (cbPtr->gc != NULL) {
	Tk_FreeGC(viewPtr->display, cbPtr->gc);
    }
    if (cbPtr->icon != NULL) {
	Blt_TreeView_FreeIcon(viewPtr, cbPtr->icon);
    }
}

static TreeViewStyle *
GetStyle(Tcl_Interp *interp, TreeView *viewPtr, const char *styleName)
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&viewPtr->styleTable, styleName);
    if (hPtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find cell style \"", styleName, 
		"\"", (char *)NULL);
	}
	return NULL;
    }
    return Blt_GetHashValue(hPtr);
}

int
Blt_TreeView_GetStyle(Tcl_Interp *interp, TreeView *viewPtr, 
		      const char *styleName, TreeViewStyle **stylePtrPtr)
{
    TreeViewStyle *stylePtr;

    stylePtr = GetStyle(interp, viewPtr, styleName);
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    stylePtr->refCount++;
    *stylePtrPtr = stylePtr;
    return TCL_OK;
}

#ifdef notdef
int
Blt_TreeView_GetStyleFromObj(Tcl_Interp *interp, TreeView *viewPtr, 
	Tcl_Obj *objPtr, TreeViewStyle **stylePtrPtr)
{
    return Blt_TreeView_GetStyle(interp, viewPtr, Tcl_GetString(objPtr), stylePtrPtr);
}
#endif

static TreeViewStyle *
CreateStyle(
     Tcl_Interp *interp,
     TreeView *viewPtr,			/* Blt_TreeView_ widget. */
     int type,				/* Type of style: either
					 * STYLE_TEXTBOX,
					 * STYLE_COMBOBOX, or
					 * STYLE_CHECKBOX */
    const char *styleName,		/* Name of the new style. */
    int objc,
    Tcl_Obj *const *objv)
{    
    Blt_HashEntry *hPtr;
    int isNew;
    TreeViewStyle *stylePtr;
    
    hPtr = Blt_CreateHashEntry(&viewPtr->styleTable, styleName, &isNew);
    if (!isNew) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "cell style \"", styleName, 
			     "\" already exists", (char *)NULL);
	}
	return NULL;
    }
    /* Create the new marker based upon the given type */
    switch (type) {
    case STYLE_TEXTBOX:
	stylePtr = CreateTextBox(viewPtr, hPtr);
	break;
    case STYLE_COMBOBOX:
	stylePtr = CreateComboBox(viewPtr, hPtr);
	break;
    case STYLE_CHECKBOX:
	stylePtr = CreateCheckBox(viewPtr, hPtr);
	break;
    default:
	return NULL;
    }
    bltTreeViewIconOption.clientData = viewPtr;
    if (Blt_ConfigureComponentFromObj(interp, viewPtr->tkwin, styleName, 
	stylePtr->classPtr->className, stylePtr->classPtr->specsPtr, 
	objc, objv, (char *)stylePtr, 0) != TCL_OK) {
	Blt_TreeView_FreeStyle(viewPtr, stylePtr);
	return NULL;
    }
    return stylePtr;
}

void
Blt_TreeView_UpdateStyleGCs(TreeView *viewPtr, TreeViewStyle *stylePtr)
{
    (*stylePtr->classPtr->configProc)(viewPtr, stylePtr);
    stylePtr->flags |= STYLE_DIRTY;
    Blt_TreeView_EventuallyRedraw(viewPtr);
}

TreeViewStyle *
Blt_TreeView_CreateStyle(
     Tcl_Interp *interp,
     TreeView *viewPtr,	/* Blt_TreeView_ widget. */
     int type,			/* Type of style: either
				 * STYLE_TEXTBOX,
				 * STYLE_COMBOBOX, or
				 * STYLE_CHECKBOX */
    const char *styleName)	/* Name of the new style. */
{    
    TreeViewStyle *stylePtr;

    stylePtr = CreateStyle(interp, viewPtr, type, styleName, 0, (Tcl_Obj **)NULL);
    if (stylePtr == NULL) {
	return NULL;
    }
    return stylePtr;
}

void
Blt_TreeView_FreeStyle(TreeView *viewPtr, TreeViewStyle *stylePtr)
{
    stylePtr->refCount--;
#ifdef notdef
    fprintf(stderr, "Blt_TreeView_FreeStyle %s count=%d\n", stylePtr->name,
	     stylePtr->refCount);
#endif
    /* Remove the style from the hash table so that it's name can be used.*/
    /* If no cell is using the style, remove it.*/
    if (stylePtr->refCount <= 0) {
#ifdef notdef
	fprintf(stderr, "freeing %s\n", stylePtr->name);
#endif
	bltTreeViewIconOption.clientData = viewPtr;
	Blt_FreeOptions(stylePtr->classPtr->specsPtr, (char *)stylePtr, 
			   viewPtr->display, 0);
	(*stylePtr->classPtr->freeProc)(viewPtr, stylePtr); 
	if (stylePtr->hashPtr != NULL) {
	    Blt_DeleteHashEntry(&viewPtr->styleTable, stylePtr->hashPtr);
	} 
	if (stylePtr->link != NULL) {
	    /* Only user-generated styles will be in the list. */
	    Blt_Chain_DeleteLink(viewPtr->userStyles, stylePtr->link);
	}
	if (stylePtr->name != NULL) {
	    Blt_Free(stylePtr->name);
	}
	Blt_Free(stylePtr);
    } 
}

void
Blt_TreeView_SetStyleIcon(TreeView *viewPtr, TreeViewStyle *stylePtr, 
			  TreeViewIcon icon)
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;

    if (tbPtr->icon != NULL) {
	Blt_TreeView_FreeIcon(viewPtr, tbPtr->icon);
    }
    tbPtr->icon = icon;
}

GC
Blt_TreeView_GetStyleGC(TreeViewStyle *stylePtr)
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;
    return tbPtr->gc;
}

Blt_Background
Blt_TreeView_GetStyleBackground(TreeView *viewPtr, TreeViewStyle *stylePtr)
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;
    Blt_Background bg;

    bg = (tbPtr->flags & STYLE_HIGHLIGHT) ? tbPtr->highlightBg : tbPtr->bg;
    return (bg != NULL) ? bg : viewPtr->bg;
}

Blt_Font
Blt_TreeView_GetStyleFont(TreeView *viewPtr, TreeViewStyle *stylePtr)
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;

    if (tbPtr->font != NULL) {
	return tbPtr->font;
    }
    return viewPtr->font;
}

XColor *
Blt_TreeView_GetStyleFg(TreeView *viewPtr, TreeViewStyle *stylePtr)
{
    TreeViewTextBox *tbPtr = (TreeViewTextBox *)stylePtr;

    if (tbPtr->fgColor != NULL) {
	return tbPtr->fgColor;
    }
    return viewPtr->fgColor;
}

static void
DrawValue(TreeView *viewPtr, TreeViewEntry *entryPtr, TreeViewValue *valuePtr)
{
    int srcX, srcY, x, y;
    int w, h;
    int pixWidth, pixHeight;
    int x1, x2, y1, y2;
    TreeViewColumn *colPtr;
    TreeViewStyle *stylePtr;
    Blt_Background bg;
    int overlap;

    stylePtr = valuePtr->stylePtr;
    if (stylePtr == NULL) {
	stylePtr = valuePtr->columnPtr->stylePtr;
    }
    if (stylePtr->cursor != None) {
	if (valuePtr == viewPtr->activeValuePtr) {
	    Tk_DefineCursor(viewPtr->tkwin, stylePtr->cursor);
	} else {
	    if (viewPtr->cursor != None) {
		Tk_DefineCursor(viewPtr->tkwin, viewPtr->cursor);
	    } else {
		Tk_UndefineCursor(viewPtr->tkwin);
	    }
	}
    }
    colPtr = valuePtr->columnPtr;
    x = SCREENX(viewPtr, colPtr->worldX) + colPtr->pad.side1;
    y = SCREENY(viewPtr, entryPtr->worldY);
    h = entryPtr->height - 2;
    w = valuePtr->columnPtr->width - PADDING(colPtr->pad);

    y1 = viewPtr->titleHeight + viewPtr->inset;
    y2 = Tk_Height(viewPtr->tkwin) - viewPtr->inset;
    x1 = viewPtr->inset;
    x2 = Tk_Width(viewPtr->tkwin) - viewPtr->inset;

    if (((x + w) < x1) || (x > x2) || ((y + h) < y1) || (y > y2)) {
	return;				/* Value is entirely clipped. */
    }

    /* Draw the background of the value. */
    if ((valuePtr == viewPtr->activeValuePtr) ||
	(!Blt_TreeView_EntryIsSelected(viewPtr, entryPtr))) {
	bg = Blt_TreeView_GetStyleBackground(viewPtr, viewPtr->stylePtr);
    } else {
	bg = CHOOSE(viewPtr->selBg, stylePtr->selBg);
    }
    /*FIXME*/
    /* bg = CHOOSE(viewPtr->selBg, stylePtr->selBg);  */
    overlap = FALSE;
    /* Clip the drawable if necessary */
    srcX = srcY = 0;
    pixWidth = w, pixHeight = h;
    if (x < x1) {
	pixWidth -= x1 - x;
	srcX += x1 - x;
	x = x1;
	overlap = TRUE;
    }
    if ((x + w) >= x2) {
	pixWidth -= (x + w) - x2;
	overlap = TRUE;
    }
    if (y < y1) {
	pixHeight -= y1 - y;
	srcY += y1 - y;
	y = y1;
	overlap = TRUE;
    }
    if ((y + h) >= y2) {
	pixHeight -= (y + h) - y2;
	overlap = TRUE;
    }
    if (overlap) {
	Drawable drawable;

	drawable = Tk_GetPixmap(viewPtr->display, Tk_WindowId(viewPtr->tkwin), 
		pixWidth, pixHeight, Tk_Depth(viewPtr->tkwin));
	Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, bg, 0, 0, 
		pixWidth, pixHeight, 0, TK_RELIEF_FLAT);
	Blt_TreeView_DrawValue(viewPtr, entryPtr, valuePtr, drawable, srcX, srcY);
	XCopyArea(viewPtr->display, drawable, Tk_WindowId(viewPtr->tkwin), 
		  viewPtr->lineGC, 0, 0, pixWidth, pixHeight, x, y+1);
	Tk_FreePixmap(viewPtr->display, drawable);
    } else {
	Drawable drawable;

	drawable = Tk_WindowId(viewPtr->tkwin);
	Blt_FillBackgroundRectangle(viewPtr->tkwin, drawable, bg, x, y+1, w, h, 
		0, TK_RELIEF_FLAT);
	Blt_TreeView_DrawValue(viewPtr, entryPtr, valuePtr, drawable, x, y);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * StyleActivateOp --
 *
 * 	Turns on/off highlighting for a particular style.
 *
 *	  .t style activate entry column
 *
 * Results:
 *	A standard TCL result.  If TCL_ERROR is returned, then interp->result
 *	contains an error message.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleActivateOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
		Tcl_Obj *const *objv)
{
    TreeViewValue *oldValuePtr;

    oldValuePtr = viewPtr->activeValuePtr;
    if (objc == 3) {
	Tcl_Obj *listObjPtr; 
	TreeViewEntry *ep;
	TreeViewValue *vp;

	vp = viewPtr->activeValuePtr;
	ep = viewPtr->activePtr;
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	if ((ep != NULL) && (vp != NULL)) {
	    Tcl_Obj *objPtr; 
	    objPtr = Tcl_NewLongObj(Blt_Tree_NodeId(ep->node));
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    objPtr = Tcl_NewStringObj(vp->columnPtr->key, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	} 
	Tcl_SetObjResult(interp, listObjPtr);
    } else if (objc == 4) {
	viewPtr->activeValuePtr = NULL;
	if ((oldValuePtr != NULL)  && (viewPtr->activePtr != NULL)) {
	    DrawValue(viewPtr, viewPtr->activePtr, oldValuePtr);
	}
    } else {
	TreeViewColumn *colPtr;
	TreeViewEntry *ep;
	TreeViewValue *vp;

	if (Blt_TreeView_GetEntry(viewPtr, objv[3], &ep) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Blt_TreeView_GetColumn(interp, viewPtr, objv[4], &colPtr) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	vp = Blt_TreeView_FindValue(ep, colPtr);
	if (vp != NULL) {
	    viewPtr->activePtr = ep;
	    viewPtr->activeColumnPtr = colPtr;
	    oldValuePtr = viewPtr->activeValuePtr;
	    viewPtr->activeValuePtr = vp;
	    if (vp != oldValuePtr) {
		if (oldValuePtr != NULL) {
		    DrawValue(viewPtr, ep, oldValuePtr);
		}
		DrawValue(viewPtr, ep, vp);
	    }
	}
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * StyleCgetOp --
 *
 *	  .t style cget "styleName" -background
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleCgetOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	    Tcl_Obj *const *objv)
{
    TreeViewStyle *stylePtr;

    stylePtr = GetStyle(interp, viewPtr, Tcl_GetString(objv[3]));
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    return Blt_ConfigureValueFromObj(interp, viewPtr->tkwin, 
	stylePtr->classPtr->specsPtr, (char *)viewPtr, objv[4], 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * StyleCheckBoxOp --
 *
 *	  .t style checkbox "styleName" -background blue
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleCheckBoxOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
		Tcl_Obj *const *objv)
{
    TreeViewStyle *stylePtr;

    stylePtr = CreateStyle(interp, viewPtr, STYLE_CHECKBOX, 
	Tcl_GetString(objv[3]), objc - 4, objv + 4);
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    stylePtr->link = Blt_Chain_Append(viewPtr->userStyles, stylePtr);
    Blt_TreeView_UpdateStyleGCs(viewPtr, stylePtr);
    Tcl_SetObjResult(interp, objv[3]);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * StyleComboBoxOp --
 *
 *	  .t style combobox "styleName" -background blue
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleComboBoxOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
		Tcl_Obj *const *objv)
{
    TreeViewStyle *stylePtr;

    stylePtr = CreateStyle(interp, viewPtr, STYLE_COMBOBOX, 
	Tcl_GetString(objv[3]), objc - 4, objv + 4);
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    stylePtr->link = Blt_Chain_Append(viewPtr->userStyles, stylePtr);
    Blt_TreeView_UpdateStyleGCs(viewPtr, stylePtr);
    Tcl_SetObjResult(interp, objv[3]);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * StyleConfigureOp --
 *
 * 	This procedure is called to process a list of configuration options
 * 	database, in order to reconfigure a style.
 *
 *	  .t style configure "styleName" option value
 *
 * Results:
 *	A standard TCL result.  If TCL_ERROR is returned, then interp->result
 *	contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font, etc. get
 *	set for stylePtr; old resources get freed, if there were any.
 *
 *---------------------------------------------------------------------------
 */
static int
StyleConfigureOp(TreeView *viewPtr, Tcl_Interp *interp, int objc,
		 Tcl_Obj *const *objv)
{
    TreeViewStyle *stylePtr;

    stylePtr = GetStyle(interp, viewPtr, Tcl_GetString(objv[3]));
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    if (objc == 4) {
	return Blt_ConfigureInfoFromObj(interp, viewPtr->tkwin, 
	    stylePtr->classPtr->specsPtr, (char *)stylePtr, (Tcl_Obj *)NULL, 0);
    } else if (objc == 5) {
	return Blt_ConfigureInfoFromObj(interp, viewPtr->tkwin, 
		stylePtr->classPtr->specsPtr, (char *)stylePtr, objv[5], 0);
    }
    bltTreeViewIconOption.clientData = viewPtr;
    if (Blt_ConfigureWidgetFromObj(interp, viewPtr->tkwin, 
	stylePtr->classPtr->specsPtr, objc - 4, objv + 4, (char *)stylePtr, 
	BLT_CONFIG_OBJV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    (*stylePtr->classPtr->configProc)(viewPtr, stylePtr);
    stylePtr->flags |= STYLE_DIRTY;
    viewPtr->flags |= (LAYOUT_PENDING | DIRTY);
    Blt_TreeView_EventuallyRedraw(viewPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * StyleForgetOp --
 *
 * 	Eliminates one or more style names.  A style still may be in use after
 * 	its name has been officially removed.  Only its hash table entry is
 * 	removed.  The style itself remains until its reference count returns
 * 	to zero (i.e. no one else is using it).
 *
 *	  .t style forget "styleName"...
 *
 * Results:
 *	A standard TCL result.  If TCL_ERROR is returned, then interp->result
 *	contains an error message.
 *
 *---------------------------------------------------------------------------
 */
static int
StyleForgetOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	      Tcl_Obj *const *objv)
{
    TreeViewStyle *stylePtr;
    int i;

    for (i = 3; i < objc; i++) {
	stylePtr = GetStyle(interp, viewPtr, Tcl_GetString(objv[i]));
	if (stylePtr == NULL) {
	    return TCL_ERROR;
	}
	/* 
	 * Removing the style from the hash tables frees up the style
	 * name again.  The style itself may not be removed until it's
	 * been released by everything using it.
	 */
	if (stylePtr->hashPtr != NULL) {
	    Blt_DeleteHashEntry(&viewPtr->styleTable, stylePtr->hashPtr);
	    stylePtr->hashPtr = NULL;
	} 
	Blt_TreeView_FreeStyle(viewPtr, stylePtr);
    }
    Blt_TreeView_EventuallyRedraw(viewPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * StyleHighlightOp --
 *
 * 	Turns on/off highlighting for a particular style.
 *
 *	  .t style highlight styleName on|off
 *
 * Results:
 *	A standard TCL result.  If TCL_ERROR is returned, then interp->result
 *	contains an error message.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleHighlightOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
		 Tcl_Obj *const *objv)
{
    TreeViewStyle *stylePtr;
    int bool, oldBool;

    stylePtr = GetStyle(interp, viewPtr, Tcl_GetString(objv[3]));
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_GetBooleanFromObj(interp, objv[4], &bool) != TCL_OK) {
	return TCL_ERROR;
    }
    oldBool = ((stylePtr->flags & STYLE_HIGHLIGHT) != 0);
    if (oldBool != bool) {
	if (bool) {
	    stylePtr->flags |= STYLE_HIGHLIGHT;
	} else {
	    stylePtr->flags &= ~STYLE_HIGHLIGHT;
	}
	Blt_TreeView_EventuallyRedraw(viewPtr);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * StyleNamesOp --
 *
 * 	Lists the names of all the current styles in the treeview widget.
 *
 *	  .t style names
 *
 * Results:
 *	Always TCL_OK.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleNamesOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	     Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Tcl_Obj *listObjPtr, *objPtr;
    TreeViewStyle *stylePtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (hPtr = Blt_FirstHashEntry(&viewPtr->styleTable, &cursor); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&cursor)) {
	stylePtr = Blt_GetHashValue(hPtr);
	objPtr = Tcl_NewStringObj(stylePtr->name, -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * StyleSetOp --
 *
 * 	Sets a style for a given key for all the ids given.
 *
 *	  .t style set styleName key node...
 *
 * Results:
 *	A standard TCL result.  If TCL_ERROR is returned, then interp->result
 *	contains an error message.
 *
 *---------------------------------------------------------------------------
 */
static int
StyleSetOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	   Tcl_Obj *const *objv)
{
    Blt_TreeKey key;
    TreeViewStyle *stylePtr;
    int i;

    stylePtr = GetStyle(interp, viewPtr, Tcl_GetString(objv[3]));
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    key = Blt_Tree_GetKey(viewPtr->tree, Tcl_GetString(objv[4]));
    stylePtr->flags |= STYLE_LAYOUT;
    for (i = 5; i < objc; i++) {
	TreeViewEntry *entryPtr;
	TreeViewTagIter iter;

	if (Blt_TreeView_FindTaggedEntries(viewPtr, objv[i], &iter) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (entryPtr = Blt_TreeView_FirstTaggedEntry(&iter); entryPtr != NULL; 
	     entryPtr = Blt_TreeView_NextTaggedEntry(&iter)) {
	    TreeViewValue *vp;

	    for (vp = entryPtr->values; vp != NULL; vp = vp->nextPtr) {
		if (vp->columnPtr->key == key) {
		    TreeViewStyle *oldStylePtr;

		    stylePtr->refCount++;
		    oldStylePtr = vp->stylePtr;
		    vp->stylePtr = stylePtr;
		    if (oldStylePtr != NULL) {
			Blt_TreeView_FreeStyle(viewPtr, oldStylePtr);
		    }
		    break;
		}
	    }
	}
    }
    Blt_TreeView_EventuallyRedraw(viewPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * StyleTextBoxOp --
 *
 *	  .t style text "styleName" -background blue
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StyleTextBoxOp(TreeView *viewPtr, Tcl_Interp *interp, int objc, 
	       Tcl_Obj *const *objv)
{
    TreeViewStyle *stylePtr;

    stylePtr = CreateStyle(interp, viewPtr, STYLE_TEXTBOX, 
	Tcl_GetString(objv[3]), objc - 4, objv + 4);
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    stylePtr->link = Blt_Chain_Append(viewPtr->userStyles, stylePtr);
    Blt_TreeView_UpdateStyleGCs(viewPtr, stylePtr);
    Tcl_SetObjResult(interp, objv[3]);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * StyleUnsetOp --
 *
 * 	Removes a style for a given key for all the ids given.
 *	The cell's style is returned to its default state.
 *
 *	  .t style unset styleName key node...
 *
 * Results:
 *	A standard TCL result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 *---------------------------------------------------------------------------
 */
static int
StyleUnsetOp(TreeView *viewPtr, Tcl_Interp *interp, int objc,
	     Tcl_Obj *const *objv)
{
    Blt_TreeKey key;
    TreeViewStyle *stylePtr;
    int i;

    stylePtr = GetStyle(interp, viewPtr, Tcl_GetString(objv[3]));
    if (stylePtr == NULL) {
	return TCL_ERROR;
    }
    key = Blt_Tree_GetKey(viewPtr->tree, Tcl_GetString(objv[4]));
    stylePtr->flags |= STYLE_LAYOUT;
    for (i = 5; i < objc; i++) {
	TreeViewTagIter iter;
	TreeViewEntry *entryPtr;

	if (Blt_TreeView_FindTaggedEntries(viewPtr, objv[i], &iter) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (entryPtr = Blt_TreeView_FirstTaggedEntry(&iter); entryPtr != NULL; 
	     entryPtr = Blt_TreeView_NextTaggedEntry(&iter)) {
	    TreeViewValue *vp;

	    for (vp = entryPtr->values; vp != NULL; vp = vp->nextPtr) {
		if (vp->columnPtr->key == key) {
		    if (vp->stylePtr != NULL) {
			Blt_TreeView_FreeStyle(viewPtr, vp->stylePtr);
			vp->stylePtr = NULL;
		    }
		    break;
		}
	    }
	}
    }
    Blt_TreeView_EventuallyRedraw(viewPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * StyleOp --
 *
 *	.t style activate $node $column
 *	.t style activate 
 *	.t style cget "highlight" -foreground
 *	.t style configure "highlight" -fg blue -bg green
 *	.t style checkbox "highlight"
 *	.t style highlight "highlight" on|off
 *	.t style combobox "highlight"
 *	.t style text "highlight"
 *	.t style forget "highlight"
 *	.t style get "mtime" $node
 *	.t style names
 *	.t style set "mtime" "highlight" all
 *	.t style unset "mtime" all
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec styleOps[] = {
    {"activate",  1, StyleActivateOp,  3, 5, "entry column",},
    {"cget",      2, StyleCgetOp,      5, 5, "styleName option",},
    {"checkbox",  2, StyleCheckBoxOp,  4, 0, "styleName options...",},
    {"combobox",  3, StyleComboBoxOp,  4, 0, "styleName options...",},
    {"configure", 3, StyleConfigureOp, 4, 0, "styleName options...",},
    {"forget",    1, StyleForgetOp,    3, 0, "styleName...",},
    {"highlight", 1, StyleHighlightOp, 5, 5, "styleName boolean",},
    {"names",     1, StyleNamesOp,     3, 3, "",}, 
    {"set",       1, StyleSetOp,       6, 6, "key styleName tagOrId...",},
    {"textbox",   1, StyleTextBoxOp,   4, 0, "styleName options...",},
    {"unset",     1, StyleUnsetOp,     5, 5, "key tagOrId",},
};

static int nStyleOps = sizeof(styleOps) / sizeof(Blt_OpSpec);

int
Blt_TreeView_StyleOp(TreeView *viewPtr, Tcl_Interp *interp, int objc,
		     Tcl_Obj *const *objv)
{
    TreeViewCmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nStyleOps, styleOps, BLT_OP_ARG2, objc, 
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc)(viewPtr, interp, objc, objv);
    return result;
}
#endif /* NO_TREEVIEW */
