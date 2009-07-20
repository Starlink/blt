
/*
 * bltGrContour.c --
 *
 * This module implements contour elements for the BLT graph widget.
 *
 *	Copyright 1993-2004 George A Howlett.
 *
 *	Permission is hereby granted, free of charge, to any person
 *	obtaining a copy of this software and associated documentation
 *	files (the "Software"), to deal in the Software without
 *	restriction, including without limitation the rights to use,
 *	copy, modify, merge, publish, distribute, sublicense, and/or
 *	sell copies of the Software, and to permit persons to whom the
 *	Software is furnished to do so, subject to the following
 *	conditions:
 *
 *	The above copyright notice and this permission notice shall be
 *	included in all copies or substantial portions of the
 *	Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 *	KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 *	WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 *	PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 *	OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 *	OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 *	OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "bltGraph.h"
#include <X11/Xutil.h>

#include "bltGrElem.h"

/* 
 * .c element create 
 * .c level create name 0 0 -min -max -foreground -linewidth -background -color 
 * .c level configure 
 * .c level closest x y 
 * .c element closest x y 
 * .c element level add x y z 
 */
typedef struct {
    float x, y;
} MeshKey;

typedef struct {
    float x, y, z;
}  Point3;

typedef struct {
    Point3 *A, Point3 *B;	/* Index of two edge points in x, y, and
				 * z vectors. */
} Edge;

typedef struct {
    Edge *AB, *BC, *AC;
} Triangle;

typedef struct {
    double min, max;		/* Minimum and maximum values for the triangle
				 * indexed at the same value. */
} Limits;

typedef struct {
    ElemValues *x, *y;
} MeshKey;

typedef struct {
    int refCount;		/* Indicates how many contour elements are
				 * using this mesh.  When refCount is zero, it
				 * means that this mesh can be destroyed. */

    ElemValues *x, *y;		/* The x and y vectors containing the points
				 * of the mesh. */

    long nPoints;		/* The number of points in the mesh */
    
    Edge *edges;		/* Array of edges generated from the points. */
    long nEdges;		/* # of edges in above array. */

    Triangle *triangles;	/* Array of triangles generated from the
				 * edges. */
    long nTriangles;		/* # of triangles in the above array. */
} TriMesh;

typedef struct  {
    Point2d a, b, c;		/* Filled triangle. */
    struct Polygon3 *next;
} Polygon3;

typedef struct  {
    Point2d a, b, c, d;		/* Filled quadrilateral. */
    struct Polygon4 *next;
} Polygon4;

typedef struct {
    char *name;			/*  */
    char *label;		/* Label to be displayed in the legend. */
    double min, max;

    int symbol;			/* Symbol to be displayed at each point. */
    int lineWidth;		/* Width of contour line.  */
    Dashes dashes;
    XColor *bgColor, *fgColor;
    GC fillGC;
    GC lineGC;

    Polygon4 *quadilaterals;
    int nQuads;
    Polygon3 *triangles;
    int nTriangles;
    Segment2d *segments;
    int nSegments;
} ContourLevel;

typedef struct {
    GraphObj obj;		/* Must be first field in element. */

    unsigned int flags;		

    Blt_HashEntry *hashPtr;

    /* Fields specific to elements. */

    char *label;		/* Label displayed in legend. There may be
				 * sub-labels for each contour range/value. */
    unsigned short row, col;  	/* Position of the entry in the legend. */

    int labelRelief;		/* Relief of label in legend. */

    Axis2d axes;		/* X-axis and Y-axis mapping the element */

    ElemValues x, y, w;		/* Contains array of floating point graph
				 * coordinate values. Also holds min/max and
				 * the number of coordinates */


    int *activeIndices;		/* Array of indices (malloc-ed) which indicate
				 * which data points are active (drawn with
				 * "active" colors). */

    int nActiveIndices;		/* Number of active data points.  Special
				 * case: if nActiveIndices < 0 and the active
				 * bit is set in "flags", then all data points
				 * are drawn active. */

    ElementProcs *procsPtr;

    Blt_ConfigSpec *configSpecs; /* Configuration specifications. */

    Pen *activePenPtr;		/* Standard Pens */
    Pen *normalPenPtr;
    Pen *builtinPenPtr;

    /* There is no style for a particular x, y */
    Blt_Chain stylePalette;	/* Palette of pens. */

    /* Symbol scaling */
    int scaleSymbols;		/* If non-zero, the symbols will scale in size
				 * as the graph is zoomed in/out.  */

    double xRange, yRange;	/* Initial X-axis and Y-axis ranges: used to
				 * scale the size of element's symbol. */
    int state;

    TriMesh *meshPtr;		/* Mesh associated with contour data set. */
    ElemValues z;
    Limits *limits;		/* Array of limits (min/max) associated with
				 * each triangle in the above mesh. */

} ContourElement;


typedef struct {
    float x1, y1, x2, y2;
} BarRegion;

typedef struct {
    char *name;			/* Pen style identifier.  If NULL pen was
				 * statically allocated. */
    ClassId classId;		/* Type of pen */
    char *typeId;		/* String token identifying the type of pen */
    unsigned int flags;		/* Indicates if the pen element is active or
				 * normal */
    int refCount;		/* Reference count for elements using
				 * this pen. */
    Blt_HashEntry *hashPtr;

    Blt_ConfigSpec *configSpecs; /* Configuration specifications */

    PenConfigureProc *configProc;
    PenDestroyProc *destroyProc;
    Graph *graphPtr;		/* Graph that the pen is associated with. */

    /* Contour Plot specific pen fields start here. */

    XColor *fgColor;		/* Foreground color of bar */
    Blt_Background bg;		/* 3D border and background color */
    int borderWidth;		/* 3D border width of bar */
    int relief;			/* Relief of the bar */
    Pixmap stipple;		/* Stipple */
    GC gc;			/* Graphics context */

    /* Error bar attributes. */
    int errorBarShow;		/* Describes which error bars to display:
				 * none, x, y, or * both. */

    int errorBarLineWidth;	/* Width of the error bar segments. */

    int errorBarCapWidth;
    XColor *errorBarColor;	/* Color of the error bar. */

    GC errorBarGC;		/* Error bar graphics context. */

    /* Show value attributes. */
    int valueShow;		/* Indicates whether to display data value.
				 * Values are x, y, or none. */

    char *valueFormat;		/* A printf format string. */
    TextStyle valueStyle;	/* Text attributes (color, font, rotation,
				 * etc.) of the value. */
    
} ContourPen;

typedef struct {
    Weight weight;		/* Weight range where this pen is valid. */
    ContourPen *penPtr;		/* Pen to use. */

    XRectangle *bars;		/* Indicates starting location in bar array
				 * for this pen. */
    int nBars;			/* Number of bar segments for this pen. */

    GraphSegments xeb, yeb;	/* X and Y error bars. */

    int symbolSize;		/* Size of the pen's symbol scaled to the
				 * current graph size. */
    int errorBarCapWidth;	/* Length of the cap ends on each error
				 * bar. */

} ContourStyle;


typedef struct {
    GraphObj obj;		/* Must be first field in element. */
    unsigned int flags;		
    Blt_HashEntry *hashPtr;

    /* Fields specific to elements. */

    char *label;		/* Label displayed in legend */

    int labelRelief;		/* Relief of label in legend. */

    Axis2d axes;		/* X-axis and Y-axis mapping the element */

    ElemValues x, y, w;		/* Contains array of floating point graph
				 * coordinate values. Also holds min/max and
				 * the number of coordinates */

    int *activeIndices;		/* Array of indices (malloc-ed) which indicate
				 * which data points are active (drawn with
				 * "active" colors). */

    int nActiveIndices;		/* Number of active data points.  Special
				 * case: if nActiveIndices < 0 and the active
				 * bit is set in "flags", then all data points
				 * are drawn active. */

    ElementProcs *procsPtr;

    Blt_ConfigSpec *configSpecs; /* Configuration specifications. */

    ContourPen *activePenPtr;	/* Standard Pens */
    ContourPen *normalPenPtr;
    ContourPen *builtinPenPtr;

    Blt_Chain stylePalette;	/* Palette of pens. */

    /* Symbol scaling */
    int scaleSymbols;		/* If non-zero, the symbols will scale in size
				 * as the graph is zoomed in/out.  */

    double xRange, yRange;	/* Initial X-axis and Y-axis ranges: used to
				 * scale the size of element's symbol. */
    int state;

    /* Fields specific to the contour element */

    float barWidth;

    int *barToData;
    XRectangle *bars;	        /* Array of rectangles comprising the bar
				 * segments of the element. */
    int *activeToData;
    XRectangle *activeRects;

    int nBars;			/* # of visible bar segments for element */
    int nActive;

    ContourPen builtinPen;

    ElemValues z;		/* Contains array of floating point graph
				 * coordinate values. Also holds min/max and
				 * the number of coordinates */
    Triangle *triangles;
    int nTriangles;

} ContourElement;

BLT_EXTERN Blt_CustomOption bltContourPenOption;
BLT_EXTERN Blt_CustomOption bltValuesOption;
BLT_EXTERN Blt_CustomOption bltValuePairsOption;
BLT_EXTERN Blt_CustomOption bltXAxisOption;
BLT_EXTERN Blt_CustomOption bltYAxisOption;
BLT_EXTERN Blt_CustomOption bltColorOption;
BLT_EXTERN Blt_CustomOption bltContourStylesOption;

static Blt_OptionParseProc ObjToBarMode;
static Blt_OptionPrintProc BarModeToObj;
Blt_CustomOption bltBarModeOption =
{
    ObjToBarMode, BarModeToObj, NULL, (ClientData)0
};

#define DEF_CONTOUR_ACTIVE_PEN		"activeBar"
#define DEF_CONTOUR_AXIS_X		"x"
#define DEF_CONTOUR_AXIS_Y		"y"
#define DEF_CONTOUR_BACKGROUND		"navyblue"
#define DEF_CONTOUR_BORDERWIDTH		"2"
#define DEF_CONTOUR_ERRORBAR_COLOR	"defcolor"
#define DEF_CONTOUR_ERRORBAR_LINE_WIDTH	"1"
#define DEF_CONTOUR_ERRORBAR_CAP_WIDTH	"1"
#define DEF_CONTOUR_FOREGROUND		"blue"
#define DEF_CONTOUR_HIDE		"no"
#define DEF_CONTOUR_LABEL_RELIEF	"flat"
#define DEF_CONTOUR_NORMAL_STIPPLE	""
#define DEF_CONTOUR_RELIEF		"raised"
#define DEF_CONTOUR_SHOW_ERRORBARS	"both"
#define DEF_CONTOUR_STATE		"normal"
#define DEF_CONTOUR_STYLES		""
#define DEF_CONTOUR_TAGS		"all"
#define DEF_CONTOUR_WIDTH		"0.0"

#define DEF_PEN_ACTIVE_BACKGROUND	"red"
#define DEF_PEN_ACTIVE_FOREGROUND     	"pink"
#define DEF_PEN_BORDERWIDTH		"2"
#define DEF_PEN_NORMAL_BACKGROUND	"navyblue"
#define DEF_PEN_NORMAL_FOREGROUND	"blue"
#define DEF_PEN_RELIEF			"raised"
#define DEF_PEN_STIPPLE			""
#define DEF_PEN_TYPE			"bar"
#define	DEF_PEN_VALUE_ANCHOR		"s"
#define	DEF_PEN_VALUE_COLOR		RGB_BLACK
#define	DEF_PEN_VALUE_FONT		STD_FONT_SMALL
#define	DEF_PEN_VALUE_FORMAT		"%g"
#define DEF_PEN_SHOW_VALUES		"no"

static Blt_ConfigSpec contourPenConfigSpecs[] =
{
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	DEF_PEN_ACTIVE_BACKGROUND, Blt_Offset(ContourPen, bg),
	BLT_CONFIG_NULL_OK | ACTIVE_PEN},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	DEF_PEN_NORMAL_BACKGROUND, Blt_Offset(ContourPen, bg),
	BLT_CONFIG_NULL_OK | NORMAL_PEN},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL,
	(char *)NULL, 0, ALL_PENS},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL,
	(char *)NULL, 0, ALL_PENS},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_PEN_BORDERWIDTH, Blt_Offset(ContourPen, borderWidth), ALL_PENS},
    {BLT_CONFIG_CUSTOM, "-errorbarcolor", "errorBarColor", "ErrorBarColor",
	DEF_CONTOUR_ERRORBAR_COLOR, Blt_Offset(ContourPen, errorBarColor), 
	ALL_PENS, &bltColorOption},
    {BLT_CONFIG_PIXELS_NNEG, "-errorbarwidth", "errorBarWidth","ErrorBarWidth",
	DEF_CONTOUR_ERRORBAR_LINE_WIDTH, Blt_Offset(ContourPen, errorBarLineWidth),
        ALL_PENS | BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-errorbarcap", "errorBarCap", "ErrorBarCap", 
	DEF_CONTOUR_ERRORBAR_CAP_WIDTH, Blt_Offset(ContourPen, errorBarCapWidth),
        ALL_PENS | BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL,
	(char *)NULL, 0, ALL_PENS},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_PEN_ACTIVE_FOREGROUND, Blt_Offset(ContourPen, fgColor),
	ACTIVE_PEN | BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_PEN_NORMAL_FOREGROUND, Blt_Offset(ContourPen, fgColor),
	NORMAL_PEN |  BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_PEN_RELIEF, Blt_Offset(ContourPen, relief), ALL_PENS},
    {BLT_CONFIG_FILL, "-showerrorbars", "showErrorBars", "ShowErrorBars",
	DEF_CONTOUR_SHOW_ERRORBARS, Blt_Offset(ContourPen, errorBarShow),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_FILL, "-showvalues", "showValues", "ShowValues",
	DEF_PEN_SHOW_VALUES, Blt_Offset(ContourPen, valueShow),
        ALL_PENS | BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BITMAP, "-stipple", "stipple", "Stipple", DEF_PEN_STIPPLE, 
	Blt_Offset(ContourPen, stipple), ALL_PENS | BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_STRING, "-type", (char *)NULL, (char *)NULL, DEF_PEN_TYPE, 
	Blt_Offset(ContourPen, typeId), ALL_PENS | BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_ANCHOR, "-valueanchor", "valueAnchor", "ValueAnchor",
	DEF_PEN_VALUE_ANCHOR, Blt_Offset(ContourPen, valueStyle.anchor), 
	ALL_PENS},
    {BLT_CONFIG_COLOR, "-valuecolor", "valueColor", "ValueColor",
	DEF_PEN_VALUE_COLOR, Blt_Offset(ContourPen, valueStyle.color), 
	ALL_PENS},
    {BLT_CONFIG_FONT, "-valuefont", "valueFont", "ValueFont",
	DEF_PEN_VALUE_FONT, Blt_Offset(ContourPen, valueStyle.font), 
	ALL_PENS},
    {BLT_CONFIG_STRING, "-valueformat", "valueFormat", "ValueFormat",
	DEF_PEN_VALUE_FORMAT, Blt_Offset(ContourPen, valueFormat),
	ALL_PENS | BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_FLOAT, "-valuerotate", "valueRotate", "ValueRotate",
	(char *)NULL, Blt_Offset(ContourPen, valueStyle.angle), ALL_PENS},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};


static Blt_ConfigSpec contourElemConfigSpecs[] =
{
    {BLT_CONFIG_CUSTOM, "-activepen", "activePen", "ActivePen",
	DEF_CONTOUR_ACTIVE_PEN, Blt_Offset(ContourElement, activePenPtr), 
	BLT_CONFIG_NULL_OK, &bltContourPenOption},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
	DEF_CONTOUR_BACKGROUND, Blt_Offset(ContourElement, builtinPen.bg),
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_FLOAT, "-barwidth", "barWidth", "BarWidth", DEF_CONTOUR_WIDTH,
	Blt_Offset(ContourElement, barWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL,(char *)NULL, 0, 0},
    {BLT_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_LIST, "-bindtags", "bindTags", "BindTags", DEF_CONTOUR_TAGS, 
	Blt_Offset(ContourElement, obj.tags), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_CONTOUR_BORDERWIDTH, 
	Blt_Offset(ContourElement, builtinPen.borderWidth), 0},
    {BLT_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {BLT_CONFIG_CUSTOM, "-data", "data", "Data", (char *)NULL, 0, 0, 
	&bltValuePairsOption},
    {BLT_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_CONTOUR_FOREGROUND, Blt_Offset(ContourElement, builtinPen.fgColor), 
	BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_STRING, "-label", "label", "Label", (char *)NULL, 
	Blt_Offset(ContourElement, label), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_RELIEF, "-labelrelief", "labelRelief", "LabelRelief",
	DEF_CONTOUR_LABEL_RELIEF, Blt_Offset(ContourElement, labelRelief),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BITMASK, "-hide", "hide", "Hide", DEF_CONTOUR_HIDE, 
         Blt_Offset(ContourElement, flags), BLT_CONFIG_DONT_SET_DEFAULT,
        (Blt_CustomOption *)HIDE},
    {BLT_CONFIG_CUSTOM, "-mapx", "mapX", "MapX", DEF_CONTOUR_AXIS_X, 
	Blt_Offset(ContourElement, axes.x), 0, &bltXAxisOption},
    {BLT_CONFIG_CUSTOM, "-mapy", "mapY", "MapY", DEF_CONTOUR_AXIS_Y, 
	Blt_Offset(ContourElement, axes.y), 0, &bltYAxisOption},
    {BLT_CONFIG_CUSTOM, "-pen", "pen", "Pen", (char *)NULL, 
	Blt_Offset(ContourElement, normalPenPtr), BLT_CONFIG_NULL_OK, 
	&bltContourPenOption},
    {BLT_CONFIG_RELIEF, "-relief", "relief", "Relief", DEF_CONTOUR_RELIEF, 
	Blt_Offset(ContourElement, builtinPen.relief), 0},
    {BLT_CONFIG_FILL, "-showvalues", "showValues", "ShowValues",
	DEF_PEN_SHOW_VALUES, Blt_Offset(ContourElement, builtinPen.valueShow),
	BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_STATE, "-state", "state", "State", DEF_CONTOUR_STATE, 
	Blt_Offset(ContourElement, state), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BITMAP, "-stipple", "stipple", "Stipple",
	DEF_CONTOUR_NORMAL_STIPPLE, 
	Blt_Offset(ContourElement, builtinPen.stipple), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-styles", "styles", "Styles", DEF_CONTOUR_STYLES, 
	Blt_Offset(ContourElement, stylePalette), 0, &bltContourStylesOption},
    {BLT_CONFIG_ANCHOR, "-valueanchor", "valueAnchor", "ValueAnchor",
	DEF_PEN_VALUE_ANCHOR, Blt_Offset(ContourElement, builtinPen.valueStyle.anchor), 0},
    {BLT_CONFIG_COLOR, "-valuecolor", "valueColor", "ValueColor",
	DEF_PEN_VALUE_COLOR, 
	Blt_Offset(ContourElement, builtinPen.valueStyle.color), 0},
    {BLT_CONFIG_FONT, "-valuefont", "valueFont", "ValueFont", 
	DEF_PEN_VALUE_FONT, 
	Blt_Offset(ContourElement, builtinPen.valueStyle.font), 0},
    {BLT_CONFIG_STRING, "-valueformat", "valueFormat", "ValueFormat",
	DEF_PEN_VALUE_FORMAT, 
	Blt_Offset(ContourElement, builtinPen.valueFormat),
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_FLOAT, "-valuerotate", "valueRotate", "ValueRotate",
	(char *)NULL, Blt_Offset(ContourElement, builtinPen.valueStyle.angle), 
	0},
    {BLT_CONFIG_CUSTOM, "-weights", "weights", "Weights", (char *)NULL, 
	Blt_Offset(ContourElement, w), 0, &bltValuesOption},
    {BLT_CONFIG_CUSTOM, "-x", "xdata", "Xdata", (char *)NULL, 
	Blt_Offset(ContourElement, x), 0, &bltValuesOption},
    {BLT_CONFIG_CUSTOM, "-y", "ydata", "Ydata", (char *)NULL, 
	Blt_Offset(ContourElement, y), 0, &bltValuesOption},
    {BLT_CONFIG_CUSTOM, "-z", "zdata", "Zdata", (char *)NULL, 
	Blt_Offset(ContourElement, z), 0, &bltValuesOption},
    {BLT_CONFIG_CUSTOM, "-xdata", "xdata", "Xdata", (char *)NULL, 
	Blt_Offset(ContourElement, x), 0, &bltValuesOption},
    {BLT_CONFIG_CUSTOM, "-ydata", "ydata", "Ydata", (char *)NULL, 
	Blt_Offset(ContourElement, y), 0, &bltValuesOption},
    {BLT_CONFIG_CUSTOM, "-zdata", "zdata", "Zdata", (char *)NULL, 
	Blt_Offset(ContourElement, z), 0, &bltValuesOption},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/* Forward declarations */
static PenConfigureProc ConfigureContourPenProc;
static PenDestroyProc DestroyContourPenProc;
static ElementClosestProc ClosestBarProc;
static ElementConfigProc ConfigureBarProc;
static ElementDestroyProc DestroyBarProc;
static ElementDrawProc DrawActiveBarProc;
static ElementDrawProc DrawNormalBarProc;
static ElementDrawSymbolProc DrawSymbolProc;
static ElementExtentsProc GetContourExtentsProc;
static ElementToPostScriptProc ActiveBarToPostScriptProc;
static ElementToPostScriptProc NormalBarToPostScriptProc;
static ElementSymbolToPostScriptProc SymbolToPostScriptProc;
static ElementMapProc MapBarProc;

INLINE static int
Round(double x)
{
    return (int) (x + ((x < 0.0) ? -0.5 : 0.5));
}

/*
 *---------------------------------------------------------------------------
 * Custom option parse and print procedures
 *---------------------------------------------------------------------------
 */

/*
 *---------------------------------------------------------------------------
 *
 * NameOfBarMode --
 *
 *	Converts the integer representing the mode style into a string.
 *
 *---------------------------------------------------------------------------
 */
static char *
NameOfBarMode(BarMode mode)
{
    switch (mode) {
    case MODE_INFRONT:
	return "infront";
    case MODE_OVERLAP:
	return "overlap";
    case MODE_STACKED:
	return "stacked";
    case MODE_ALIGNED:
	return "aligned";
    default:
	return "unknown mode value";
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToMode --
 *
 *	Converts the mode string into its numeric representation.
 *
 *	Valid mode strings are:
 *
 *      "infront"   Draw a full bar at each point in the element.
 *
 * 	"stacked"   Stack bar segments vertically. Each stack is defined
 *		    by each ordinate at a particular abscissa. The height
 *		    of each segment is represented by the sum the previous
 *		    ordinates.
 *
 *	"aligned"   Align bar segments as smaller slices one next to
 *		    the other.  Like "stacks", aligned segments are
 *		    defined by each ordinate at a particular abscissa.
 *
 * Results:
 *	A standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToBarMode(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* Mode style string */
    char *widgRec,		/* Cubicle structure record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    BarMode *modePtr = (BarMode *)(widgRec + offset);
    int length;
    char c;
    char *string;
    
    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[0];
    if ((c == 'n') && (strncmp(string, "normal", length) == 0)) {
	*modePtr = MODE_INFRONT;
    } else if ((c == 'i') && (strncmp(string, "infront", length) == 0)) {
	*modePtr = MODE_INFRONT;
    } else if ((c == 's') && (strncmp(string, "stacked", length) == 0)) {
	*modePtr = MODE_STACKED;
    } else if ((c == 'a') && (strncmp(string, "aligned", length) == 0)) {
	*modePtr = MODE_ALIGNED;
    } else if ((c == 'o') && (strncmp(string, "overlap", length) == 0)) {
	*modePtr = MODE_OVERLAP;
    } else {
	Tcl_AppendResult(interp, "bad mode argument \"", string,
	    "\": should be \"infront\", \"stacked\", \"overlap\", or \"aligned\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * BarModeToObj --
 *
 *	Returns the mode style string based upon the mode flags.
 *
 * Results:
 *	The mode style string is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
BarModeToObj(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Row/column structure record */
    int offset,			/* Offset to field in structure */
    int flags)	
{
    BarMode mode = *(BarMode *)(widgRec + offset);

    return Tcl_NewStringObj(NameOfBarMode(mode), -1);
}


/* 
 * Zero out the style's number of bars and errorbars. 
 */
static void
ResetStylePalette(Blt_Chain stylePalette)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_FirstLink(stylePalette); link != NULL; 
	 link = Blt_Chain_NextLink(link)) {
	ContourStyle *stylePtr;

	stylePtr = Blt_Chain_GetValue(link);
	stylePtr->xeb.length = stylePtr->yeb.length = 0;
	stylePtr->nBars = 0;
    }
}

static int
ConfigureContourPen(Graph *graphPtr, ContourPen *penPtr)
{
    XGCValues gcValues;
    unsigned long gcMask;
    GC newGC;
    long defColor;

    gcMask = GCForeground;
    if (penPtr->fgColor != NULL) {
	defColor = penPtr->fgColor->pixel;
	gcValues.foreground = penPtr->fgColor->pixel;
    } else if (penPtr->bg != NULL) {
	defColor = Blt_BackgroundBorderColor(penPtr->bg)->pixel;
	gcValues.foreground = defColor;
    } else {
	defColor = BlackPixel(graphPtr->display, 
			      Tk_ScreenNumber(graphPtr->tkwin));
    }
    if (penPtr->stipple != None) {
	gcValues.stipple = penPtr->stipple;
	gcValues.fill_style = FillStippled;
	gcMask |= (GCStipple | GCFillStyle);
    }
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (penPtr->gc != NULL) {
	Tk_FreeGC(graphPtr->display, penPtr->gc);
    }
    penPtr->gc = newGC;

    gcMask = GCForeground | GCLineWidth;
    if (penPtr->errorBarColor == COLOR_DEFAULT) {
	gcValues.foreground = defColor;
    } else {
	gcValues.foreground = penPtr->errorBarColor->pixel;
    }
    gcValues.line_width = LineWidth(penPtr->errorBarLineWidth);
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (penPtr->errorBarGC != NULL) {
	Tk_FreeGC(graphPtr->display, penPtr->errorBarGC);
    }
    penPtr->errorBarGC = newGC;
    return TCL_OK;
}

static void
DestroyContourPen(Graph *graphPtr, ContourPen *penPtr)
{
    Blt_Ts_FreeStyle(graphPtr->display, &penPtr->valueStyle);
    if (penPtr->gc != NULL) {
	Tk_FreeGC(graphPtr->display, penPtr->gc);
    }
    if (penPtr->errorBarGC != NULL) {
	Tk_FreeGC(graphPtr->display, penPtr->errorBarGC);
    }
}

static int
ConfigureContourPenProc(Graph *graphPtr, Pen *basePtr)
{
    return ConfigureContourPen(graphPtr, (ContourPen *)basePtr);
}

static void
DestroyContourPenProc(Graph *graphPtr, Pen *basePtr)
{
    DestroyContourPen(graphPtr, (ContourPen *)basePtr);
}


static void
InitContourPen(ContourPen *penPtr)
{
    /* Generic fields common to all pen types. */
    penPtr->configProc = ConfigureContourPenProc;
    penPtr->destroyProc = DestroyContourPenProc;
    penPtr->flags = NORMAL_PEN;
    penPtr->configSpecs = contourPenConfigSpecs;

    /* Initialize fields specific to contour pens. */
    Blt_Ts_InitStyle(penPtr->valueStyle);
    penPtr->relief = TK_RELIEF_RAISED;
    penPtr->valueShow = SHOW_NONE;
    penPtr->borderWidth = 2;
    penPtr->errorBarShow = SHOW_BOTH;
}

Pen *
Blt_ContourPen(char *penName)
{
    ContourPen *penPtr;

    penPtr = Blt_AssertCalloc(1, sizeof(ContourPen));
    InitContourPen(penPtr);
    penPtr->name = Blt_AssertStrdup(penName);
    if (strcmp(penName, "activeBar") == 0) {
	penPtr->flags = ACTIVE_PEN;
    }
    return (Pen *)penPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * CheckStacks --
 *
 *	Check that the data limits are not superseded by the heights
 *	of stacked bar segments.  The heights are calculated by
 *	Blt_ComputeStacks.
 *
 * Results:
 *	If the y-axis limits need to be adjusted for stacked segments,
 *	*minPtr* or *maxPtr* are updated.
 *
 * Side effects:
 *	Autoscaling of the y-axis is affected.
 *
 *---------------------------------------------------------------------------
 */
static void
CheckStacks(Graph *graphPtr, Axis2d *pairPtr, double *minPtr, double *maxPtr)
{
    FreqInfo *infoPtr;
    int i;

    if ((graphPtr->mode != MODE_STACKED) || (graphPtr->nStacks == 0)) {
	return;
    }
    infoPtr = graphPtr->freqArr;
    for (i = 0; i < graphPtr->nStacks; i++) {
	if ((infoPtr->axes.x == pairPtr->x) && 
	    (infoPtr->axes.y == pairPtr->y)) {
	    /*

	     * Check if any of the y-values (because of stacking) are
	     * greater than the current limits of the graph.
	     */
	    if (infoPtr->sum < 0.0) {
		if (*minPtr > infoPtr->sum) {
		    *minPtr = infoPtr->sum;
		}
	    } else {
		if (*maxPtr < infoPtr->sum) {
		    *maxPtr = infoPtr->sum;
		}
	    }
	}
	infoPtr++;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ConfigureBarProc --
 *
 *	Sets up the appropriate configuration parameters in the GC.
 *      It is assumed the parameters have been previously set by
 *	a call to Blt_ConfigureWidget.
 *
 * Results:
 *	The return value is a standard TCL result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information such as bar foreground/background
 *	color and stipple etc. get set in a new GC.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ConfigureBarProc(Graph *graphPtr, Element *basePtr)
{
    ContourElement *elemPtr = (ContourElement *)basePtr;
    Blt_ChainLink link;
    ContourStyle *stylePtr;

    if (ConfigureContourPen(graphPtr, elemPtr->builtinPenPtr)!= TCL_OK) {
	return TCL_ERROR;
    }
    /*
     * Point to the static normal pen if no external pens have
     * been selected.
     */
    link = Blt_Chain_FirstLink(elemPtr->stylePalette);
    if (link == NULL) {
	link = Blt_Chain_AllocLink(sizeof(ContourStyle));
	Blt_Chain_LinkAfter(elemPtr->stylePalette, link, NULL);
    }
    stylePtr = Blt_Chain_GetValue(link);
    stylePtr->penPtr = NORMALPEN(elemPtr);

    if (Blt_ConfigModified(elemPtr->configSpecs, "-barwidth", "-*data",
	    "-map*", "-label", "-hide", "-x", "-y", (char *)NULL)) {
	elemPtr->flags |= MAP_ITEM;
    }
    return TCL_OK;
}

static void
GetContourExtentsProc(Element *basePtr, Region2d *extsPtr)
{
    ContourElement *elemPtr = (ContourElement *)basePtr;
    Mesh *meshPtr;

    extsPtr->top = extsPtr->left = DBL_MAX;
    extsPtr->bottom = extsPtr->right = -DBL_MAX;

    meshPtr = elemPtr->meshPtr;
    if ((meshPtr == NULL) || (elemPtr->values == NULL)) {
	return;			/* No mesh or values configured. */
    }
    if (meshPtr->nPoints < 3) {
	return;
    }
    extsPtr->left = (elemPtr->axes.x->logScale) ? 
	meshPtr->xLogMin : meshPtr->xMin;
    extsPtr->top = (elemPtr->axes.y->logScale) ? 
	meshPtr->yLogMin : meshPtr->yMin;
    extsPtr->right = meshPtr->xMax;
    extsPtr->bottom = meshPtr->yMax;
}

/*
 *---------------------------------------------------------------------------
 *
 * ClosestBar --
 *
 *	Find the bar segment closest to the window coordinates	point
 *	specified.
 *
 *	Note:  This does not return the height of the stacked segment
 *	       (in graph coordinates) properly.
 *
 * Results:
 *	Returns 1 if the point is width any bar segment, otherwise 0.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
ClosestBarProc(
    Graph *graphPtr,		/* Graph widget record */
    Element *basePtr,		/* Bar element */
    ClosestSearch *searchPtr)	/* Info of closest point in element */
{
    ContourElement *elemPtr = (ContourElement *)basePtr;
    XRectangle *bp;
    double minDist;
    int imin;
    int i;

    minDist = searchPtr->dist;
    imin = 0;
    
    for (bp = elemPtr->bars, i = 0; i < elemPtr->nBars; i++, bp++) {
	Point2d *pp, *pend;
	Point2d outline[5];
	double left, right, top, bottom;

	if (PointInRectangle(bp, searchPtr->x, searchPtr->y)) {
	    imin = elemPtr->barToData[i];
	    minDist = 0.0;
	    break;
	}
	left = bp->x, top = bp->y;
	right = (double)(bp->x + bp->width);
	bottom = (double)(bp->y + bp->height);
	outline[4].x = outline[3].x = outline[0].x = left;
	outline[4].y = outline[1].y = outline[0].y = top;
	outline[2].x = outline[1].x = right;
	outline[3].y = outline[2].y = bottom;

	for (pp = outline, pend = outline + 4; pp < pend; pp++) {
	    Point2d t;
	    double dist;

	    t = Blt_GetProjection(searchPtr->x, searchPtr->y, pp, pp + 1);
	    if (t.x > right) {
		t.x = right;
	    } else if (t.x < left) {
		t.x = left;
	    }
	    if (t.y > bottom) {
		t.y = bottom;
	    } else if (t.y < top) {
		t.y = top;
	    }
	    dist = hypot((t.x - searchPtr->x), (t.y - searchPtr->y));
	    if (dist < minDist) {
		minDist = dist;
		imin = elemPtr->barToData[i];
	    }
	}
    }
    if (minDist < searchPtr->dist) {
	searchPtr->elemPtr = (Element *)elemPtr;
	searchPtr->dist = minDist;
	searchPtr->index = imin;
	searchPtr->point.x = (double)elemPtr->x.values[imin];
	searchPtr->point.y = (double)elemPtr->y.values[imin];
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * MergePens --
 *
 *	Reorders the both arrays of points and errorbars to merge pens.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The old arrays are freed and new ones allocated containing
 *	the reordered points and errorbars.
 *
 *---------------------------------------------------------------------------
 */
static void
MergePens(ContourElement *elemPtr, ContourStyle **dataToStyle)
{
    if (Blt_Chain_GetLength(elemPtr->stylePalette) < 2) {
	Blt_ChainLink link;
	ContourStyle *stylePtr;

	link = Blt_Chain_FirstLink(elemPtr->stylePalette);

	stylePtr = Blt_Chain_GetValue(link);
	stylePtr->nBars = elemPtr->nBars;
	stylePtr->bars = elemPtr->bars;
	stylePtr->symbolSize = elemPtr->bars->width / 2;
	stylePtr->xeb.length = elemPtr->xeb.length;
	stylePtr->xeb.segments = elemPtr->xeb.segments;
	stylePtr->yeb.length = elemPtr->yeb.length;
	stylePtr->yeb.segments = elemPtr->yeb.segments;
	return;
    }
    /* We have more than one style. Group bar segments of like pen
     * styles together.  */

    if (elemPtr->nBars > 0) {
	Blt_ChainLink link;
	XRectangle *bars, *bp;
	int *ip, *barToData;

	bars = Blt_AssertMalloc(elemPtr->nBars * sizeof(XRectangle));
	barToData = Blt_AssertMalloc(elemPtr->nBars * sizeof(int));
	bp = bars, ip = barToData;
	for (link = Blt_Chain_FirstLink(elemPtr->stylePalette); link != NULL; 
	     link = Blt_Chain_NextLink(link)) {
	    ContourStyle *stylePtr;
	    int i;

	    stylePtr = Blt_Chain_GetValue(link);
	    stylePtr->symbolSize = bp->width / 2;
	    stylePtr->bars = bp;
	    for (i = 0; i < elemPtr->nBars; i++) {
		int iData;

		iData = elemPtr->barToData[i];
		if (dataToStyle[iData] == stylePtr) {
		    *bp++ = elemPtr->bars[i];
		    *ip++ = iData;
		}
	    }
	    stylePtr->nBars = bp - stylePtr->bars;
	}
	Blt_Free(elemPtr->bars);
	Blt_Free(elemPtr->barToData);
	elemPtr->bars = bars;
	elemPtr->barToData = barToData;
    }

    if (elemPtr->xeb.length > 0) {
	Blt_ChainLink link;
	Segment2d *bars, *sp;
	int *map, *ip;

	bars = Blt_AssertMalloc(elemPtr->xeb.length * sizeof(Segment2d));
	map = Blt_AssertMalloc(elemPtr->xeb.length * sizeof(int));
	sp = bars, ip = map;
	for (link = Blt_Chain_FirstLink(elemPtr->stylePalette); 
	     link != NULL; link = Blt_Chain_NextLink(link)) {
	    ContourStyle *stylePtr;
	    int i;

	    stylePtr = Blt_Chain_GetValue(link);
	    stylePtr->xeb.segments = sp;
	    for (i = 0; i < elemPtr->xeb.length; i++) {
		int iData;

		iData = elemPtr->xeb.map[i];
		if (dataToStyle[iData] == stylePtr) {
		    *sp++ = elemPtr->xeb.segments[i];
		    *ip++ = iData;
		}
	    }
	    stylePtr->xeb.length = sp - stylePtr->xeb.segments;
	}
	Blt_Free(elemPtr->xeb.segments);
	elemPtr->xeb.segments = bars;
	Blt_Free(elemPtr->xeb.map);
	elemPtr->xeb.map = map;
    }
    if (elemPtr->yeb.length > 0) {
	Blt_ChainLink link;
	Segment2d *bars, *sp;
	int *map, *ip;

	bars = Blt_AssertMalloc(elemPtr->yeb.length * sizeof(Segment2d));
	map = Blt_AssertMalloc(elemPtr->yeb.length * sizeof(int));
	sp = bars, ip = map;
	for (link = Blt_Chain_FirstLink(elemPtr->stylePalette); link != NULL; 
	     link = Blt_Chain_NextLink(link)) {
	    ContourStyle *stylePtr;
	    int i;

	    stylePtr = Blt_Chain_GetValue(link);
	    stylePtr->yeb.segments = sp;
	    for (i = 0; i < elemPtr->yeb.length; i++) {
		int iData;

		iData = elemPtr->yeb.map[i];
		if (dataToStyle[iData] == stylePtr) {
		    *sp++ = elemPtr->yeb.segments[i];
		    *ip++ = iData;
		}
	    }
	    stylePtr->yeb.length = sp - stylePtr->yeb.segments;
	}
	Blt_Free(elemPtr->yeb.segments);
	elemPtr->yeb.segments = bars;
	Blt_Free(elemPtr->yeb.map);
	elemPtr->yeb.map = map;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * MapActiveBars --
 *
 *	Creates an array of points of the active graph coordinates.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed and allocated for the active point array.
 *
 *---------------------------------------------------------------------------
 */
static void
MapActiveBars(ContourElement *elemPtr)
{
    if (elemPtr->activeRects != NULL) {
	Blt_Free(elemPtr->activeRects);
	elemPtr->activeRects = NULL;
    }
    if (elemPtr->activeToData != NULL) {
	Blt_Free(elemPtr->activeToData);
	elemPtr->activeToData = NULL;
    }
    elemPtr->nActive = 0;

    if (elemPtr->nActiveIndices > 0) {
	XRectangle *activeRects;
	int *activeToData;
	int i;
	int count;

	activeRects = Blt_AssertMalloc(sizeof(XRectangle) * 
				       elemPtr->nActiveIndices);
	activeToData = Blt_AssertMalloc(sizeof(int) * 
					elemPtr->nActiveIndices);
	count = 0;
	for (i = 0; i < elemPtr->nBars; i++) {
	    int *ip, *iend;

	    for (ip = elemPtr->activeIndices, 
		     iend = ip + elemPtr->nActiveIndices; ip < iend; ip++) {
		if (elemPtr->barToData[i] == *ip) {
		    activeRects[count] = elemPtr->bars[i];
		    activeToData[count] = i;
		    count++;
		}
	    }
	}
	elemPtr->nActive = count;
	elemPtr->activeRects = activeRects;
	elemPtr->activeToData = activeToData;
    }
    elemPtr->flags &= ~ACTIVE_PENDING;
}

static void
ResetBar(ContourElement *elemPtr)
{
    /* Release any storage associated with the display of the bar */
    ResetStylePalette(elemPtr->stylePalette);
    if (elemPtr->activeRects != NULL) {
	Blt_Free(elemPtr->activeRects);
    }
    if (elemPtr->activeToData != NULL) {
	Blt_Free(elemPtr->activeToData);
    }
    if (elemPtr->xeb.segments != NULL) {
	Blt_Free(elemPtr->xeb.segments);
    }
    if (elemPtr->xeb.map != NULL) {
	Blt_Free(elemPtr->xeb.map);
    }
    if (elemPtr->yeb.segments != NULL) {
	Blt_Free(elemPtr->yeb.segments);
    }
    if (elemPtr->yeb.map != NULL) {
	Blt_Free(elemPtr->yeb.map);
    }
    if (elemPtr->bars != NULL) {
	Blt_Free(elemPtr->bars);
    }
    if (elemPtr->barToData != NULL) {
	Blt_Free(elemPtr->barToData);
    }
    elemPtr->activeToData = elemPtr->xeb.map = elemPtr->yeb.map = 
	elemPtr->barToData = NULL;
    elemPtr->activeRects = elemPtr->bars = NULL;
    elemPtr->xeb.segments = elemPtr->yeb.segments = NULL;
    elemPtr->nActive = elemPtr->xeb.length = elemPtr->yeb.length = 
	elemPtr->nBars = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_MapErrorBars --
 *
 *	Creates two arrays of points and pen indices, filled with
 *	the screen coordinates of the visible
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed and allocated for the index array.
 *
 *---------------------------------------------------------------------------
 */
static void
MapErrorBars(Graph *graphPtr, ContourElement *elemPtr, ContourStyle **dataToStyle)
{
    int n, nPoints;
    Region2d exts;

    Blt_GraphExtents(graphPtr, &exts);
    nPoints = NUMBEROFPOINTS(elemPtr);
    if (elemPtr->xError.nValues > 0) {
	n = MIN(elemPtr->xError.nValues, nPoints);
    } else {
	n = MIN3(elemPtr->xHigh.nValues, elemPtr->xLow.nValues, nPoints);
    }
    if (n > 0) {
	Segment2d *bars;
	Segment2d *segPtr;
	int *map;
	int *indexPtr;
	int i;
		
	segPtr = bars = Blt_AssertMalloc(n * 3 * sizeof(Segment2d));
	indexPtr = map = Blt_AssertMalloc(n * 3 * sizeof(int));
	for (i = 0; i < n; i++) {
	    double x, y;
	    double high, low;
	    ContourStyle *stylePtr;

	    x = elemPtr->x.values[i];
	    y = elemPtr->y.values[i];
	    stylePtr = dataToStyle[i];
	    if ((FINITE(x)) && (FINITE(y))) {
		if (elemPtr->xError.nValues > 0) {
		    high = x + elemPtr->xError.values[i];
		    low = x - elemPtr->xError.values[i];
		} else {
		    high = elemPtr->xHigh.values[i];
		    low = elemPtr->xLow.values[i];
		}
		if ((FINITE(high)) && (FINITE(low)))  {
		    Point2d p, q;

		    p = Blt_Map2D(graphPtr, high, y, &elemPtr->axes);
		    q = Blt_Map2D(graphPtr, low, y, &elemPtr->axes);
		    segPtr->p = p;
		    segPtr->q = q;
		    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
			segPtr++;
			*indexPtr++ = i;
		    }
		    /* Left cap */
		    segPtr->p.x = segPtr->q.x = p.x;
		    segPtr->p.y = p.y - stylePtr->errorBarCapWidth;
		    segPtr->q.y = p.y + stylePtr->errorBarCapWidth;
		    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
			segPtr++;
			*indexPtr++ = i;
		    }
		    /* Right cap */
		    segPtr->p.x = segPtr->q.x = q.x;
		    segPtr->p.y = q.y - stylePtr->errorBarCapWidth;
		    segPtr->q.y = q.y + stylePtr->errorBarCapWidth;
		    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
			segPtr++;
			*indexPtr++ = i;
		    }
		}
	    }
	}
	elemPtr->xeb.segments = bars;
	elemPtr->xeb.length = segPtr - bars;
	elemPtr->xeb.map = map;
    }
    if (elemPtr->yError.nValues > 0) {
	n = MIN(elemPtr->yError.nValues, nPoints);
    } else {
	n = MIN3(elemPtr->yHigh.nValues, elemPtr->yLow.nValues, nPoints);
    }
    if (n > 0) {
	Segment2d *bars;
	Segment2d *segPtr;
	int *map;
	int *indexPtr;
	int i;
		
	segPtr = bars = Blt_AssertMalloc(n * 3 * sizeof(Segment2d));
	indexPtr = map = Blt_AssertMalloc(n * 3 * sizeof(int));
	for (i = 0; i < n; i++) {
	    double x, y;
	    double high, low;
	    ContourStyle *stylePtr;

	    x = elemPtr->x.values[i];
	    y = elemPtr->y.values[i];
	    stylePtr = dataToStyle[i];
	    if ((FINITE(x)) && (FINITE(y))) {
		if (elemPtr->yError.nValues > 0) {
		    high = y + elemPtr->yError.values[i];
		    low = y - elemPtr->yError.values[i];
		} else {
		    high = elemPtr->yHigh.values[i];
		    low = elemPtr->yLow.values[i];
		}
		if ((FINITE(high)) && (FINITE(low)))  {
		    Point2d p, q;
		    
		    p = Blt_Map2D(graphPtr, x, high, &elemPtr->axes);
		    q = Blt_Map2D(graphPtr, x, low, &elemPtr->axes);
		    segPtr->p = p;
		    segPtr->q = q;
		    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
			segPtr++;
			*indexPtr++ = i;
		    }
		    /* Top cap. */
		    segPtr->p.y = segPtr->q.y = p.y;
		    segPtr->p.x = p.x - stylePtr->errorBarCapWidth;
		    segPtr->q.x = p.x + stylePtr->errorBarCapWidth;
		    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
			segPtr++;
			*indexPtr++ = i;
		    }
		    /* Bottom cap. */
		    segPtr->p.y = segPtr->q.y = q.y;
		    segPtr->p.x = q.x - stylePtr->errorBarCapWidth;
		    segPtr->q.x = q.x + stylePtr->errorBarCapWidth;
		    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
			segPtr++;
			*indexPtr++ = i;
		    }
		}
	    }
	}
	elemPtr->yeb.segments = bars;
	elemPtr->yeb.length = segPtr - bars;
	elemPtr->yeb.map = map;
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * MapContourElementProc --
 *
 *	Calculates the actual window coordinates of the bar element.
 *	The window coordinates are saved in the bar element structure.
 *
 * Results:
 *	None.
 *
 * Notes:
 *	A bar can have multiple segments (more than one x,y pairs).
 *	In this case, the bar can be represented as either a set of
 *	non-contiguous bars or a single multi-segmented (stacked) bar.
 *
 *	The x-axis layout for a contour plot may be presented in one of
 *	two ways.  If abscissas are used, the bars are placed at those
 *	coordinates.  Otherwise, the range will represent the number
 *	of values.
 *
 *---------------------------------------------------------------------------
 */
static void
MapContourElementProc(Graph *graphPtr, Element *basePtr)
{
    ContourElement *elemPtr = (ContourElement *)basePtr;
    ContourStyle **dataToStyle;
    double *x, *y;
    double barWidth, barOffset;
    double baseline;
    int *barToData;		/* Maps bars to data point indices */
    int invertBar;
    int nPoints, count;
    XRectangle *rp, *bars;
    int i;
    int size;

    ResetBar(elemPtr);
    nPoints = NUMBEROFPOINTS(elemPtr);
    if (nPoints < 1) {
	return;			/* No data points */
    }
    barWidth = graphPtr->barWidth;
    if (elemPtr->barWidth > 0.0) {
	barWidth = elemPtr->barWidth;
    }
    baseline = (elemPtr->axes.y->logScale) ? 1.0 : graphPtr->baseline;
    barOffset = barWidth * 0.5;

    /*
     * Create an array of bars representing the screen coordinates
     * of all the segments in the bar.
     */
    bars = Blt_AssertCalloc(nPoints, sizeof(XRectangle));
    barToData = Blt_AssertCalloc(nPoints, sizeof(int));

    x = elemPtr->x.values, y = elemPtr->y.values;
    count = 0;
    for (rp = bars, i = 0; i < nPoints; i++) {
	Point2d c1, c2;		/* Two opposite corners of the rectangle
				 * in graph coordinates. */
	double dx, dy;
	int height;

	if (((x[i] - barWidth) > elemPtr->axes.x->axisRange.max) ||
	    ((x[i] + barWidth) < elemPtr->axes.x->axisRange.min)) {
	    continue;		/* Abscissa is out of range of the x-axis */
	}
	c1.x = x[i] - barOffset;
	c1.y = y[i];
	c2.x = c1.x + barWidth;
	c2.y = baseline;

	/*
	 * If the mode is "aligned" or "stacked" we need to adjust the
	 * x or y coordinates of the two corners.
	 */

	if ((graphPtr->nStacks > 0) && (graphPtr->mode != MODE_INFRONT)) {
	    Blt_HashEntry *hPtr;
	    FreqKey key;

	    key.value = x[i];
	    key.axes = elemPtr->axes;
	    hPtr = Blt_FindHashEntry(&graphPtr->freqTable, (char *)&key);
	    if (hPtr != NULL) {
		FreqInfo *infoPtr;
		double slice, width;

		infoPtr = Blt_GetHashValue(hPtr);
		switch (graphPtr->mode) {
		case MODE_STACKED:
		    c2.y = infoPtr->lastY;
		    c1.y += c2.y;
		    infoPtr->lastY = c1.y;
		    break;

		case MODE_ALIGNED:
		    infoPtr->count++;
		    slice = barWidth / (double)infoPtr->freq;
		    c1.x += slice * (infoPtr->freq - infoPtr->count);
		    c2.x = c1.x + slice;
		    break;

		case MODE_OVERLAP:
		    infoPtr->count++;
		    slice = barWidth / (double)(infoPtr->freq * 2);
		    width = slice * (infoPtr->freq + 1);
		    c1.x += slice * (infoPtr->freq - infoPtr->count);
		    c2.x = c1.x + width;
		    break;
		case MODE_INFRONT:
		    break;
		}
	    }
	}
	invertBar = FALSE;
	if (c1.y < c2.y) {
	    double temp;

	    /* Handle negative bar values by swapping ordinates */
	    temp = c1.y, c1.y = c2.y, c2.y = temp;
	    invertBar = TRUE;
	}
	/*
	 * Get the two corners of the bar segment and compute the rectangle
	 */
	c1 = Blt_Map2D(graphPtr, c1.x, c1.y, &elemPtr->axes);
	c2 = Blt_Map2D(graphPtr, c2.x, c2.y, &elemPtr->axes);

	/* Bound the bars horizontally by the width of the graph window */
	/* Bound the bars vertically by the position of the axis. */
	if (graphPtr->stackAxes) {
	    int right, left, top, bottom;

	    if (graphPtr->inverted) {
		left = elemPtr->axes.y->screenMin;
		right = elemPtr->axes.y->screenMin + 
		    elemPtr->axes.y->screenRange;
		top = graphPtr->top;
		bottom = graphPtr->bottom;
	    } else {
		top = elemPtr->axes.y->screenMin;
		bottom = elemPtr->axes.y->screenMin + 
		    elemPtr->axes.y->screenRange;
		left = graphPtr->left;
		right = graphPtr->right;
	    }
	    if (c1.y < (double)top) {
		c1.y = (double)top;
	    } else if (c1.y > (double)bottom) {
		c1.y = (double)bottom;
	    }
	    if (c2.y < (double)top) {
		c2.y = (double)top;
	    } else if (c2.y > (double)bottom) {
		c2.y = (double)bottom;
	    }
	    if (c1.x < (double)left) {
		c1.x = (double)left;
	    } else if (c1.x > (double)right) {
		c1.x = (double)right;
	    }
	    if (c2.x < (double)left) {
		c2.x = (double)left;
	    } else if (c2.x > (double)right) {
		c2.x = (double)right;
	    }
	} else {
	    int right, left, top, bottom;

	    if (graphPtr->inverted) {
		top = graphPtr->left;
		bottom = graphPtr->right;
		left = graphPtr->top;
		right = graphPtr->bottom;
	    } else {
		top = graphPtr->top;
		bottom = graphPtr->bottom;
		left = graphPtr->left;
		right = graphPtr->right;
	    }

	    if (c1.y < 0.0) {
		c1.y = 0.0;
	    } else if (c1.y > (double)graphPtr->height) {
		c1.y = (double)graphPtr->height;
	    }
	    if (c2.y < 0.0) {
		c2.y = 0.0;
	    } else if (c2.y > (double)graphPtr->height) {
		c2.y = (double)graphPtr->height;
	    }
	}
	dx = FABS(c1.x - c2.x);
	dy = FABS(c1.y - c2.y);
	if ((dx == 0) || (dy == 0)) {
	    continue;
	}
	height = (int)dy;
	if (invertBar) {
	    rp->y = (short int)MIN(c1.y, c2.y);
	} else {
	    rp->y = (short int)(MAX(c1.y, c2.y)) - height;
	}
	rp->x = (short int)MIN(c1.x, c2.x);
	rp->width = (short int)dx + 1;
	if (rp->width < 1) {
	    rp->width = 1;
	}
	rp->height = height + 1;
	if (rp->height < 1) {
	    rp->height = 1;
	}
	barToData[count] = i;	/* Save the data index corresponding to the
				 * rectangle */
	count++;
	rp++;
    }
    elemPtr->nBars = count;
    elemPtr->bars = bars;
    elemPtr->barToData = barToData;
    if (elemPtr->nActiveIndices > 0) {
	MapActiveBars(elemPtr);
    }
	
    size = 20;
    if (count > 0) {
	size = bars->width;
    }
    {
	Blt_ChainLink link;

	/* Set the symbol size of all the pen styles. */
	for (link = Blt_Chain_FirstLink(elemPtr->stylePalette); link != NULL;
	     link = Blt_Chain_NextLink(link)) {
	    ContourStyle *stylePtr;
	    
	    stylePtr = Blt_Chain_GetValue(link);
	    stylePtr->symbolSize = size;
	    stylePtr->errorBarCapWidth = 
		(stylePtr->penPtr->errorBarCapWidth > 0) 
		? stylePtr->penPtr->errorBarCapWidth : (size * 66666) / 100000;
	    stylePtr->errorBarCapWidth /= 2;
	}
    }
    dataToStyle = (ContourStyle **)Blt_StyleMap((Element *)elemPtr);
    if (((elemPtr->yHigh.nValues > 0) && (elemPtr->yLow.nValues > 0)) ||
	((elemPtr->xHigh.nValues > 0) && (elemPtr->xLow.nValues > 0)) ||
	(elemPtr->xError.nValues > 0) || (elemPtr->yError.nValues > 0)) {
	MapErrorBars(graphPtr, elemPtr, dataToStyle);
    }
    MergePens(elemPtr, dataToStyle);
    Blt_Free(dataToStyle);
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawSymbolProc --
 *
 * 	Draw a symbol centered at the given x,y window coordinate
 *	based upon the element symbol type and size.
 *
 * Results:
 *	None.
 *
 * Problems:
 *	Most notable is the round-off errors generated when
 *	calculating the centered position of the symbol.
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
DrawSymbolProc(
    Graph *graphPtr,
    Drawable drawable,		/* Pixmap or window to draw into */
    Element *basePtr,
    int x, int y,
    int size)
{
    ContourElement *elemPtr = (ContourElement *)basePtr;
    ContourPen *penPtr;
    int radius;

    penPtr = NORMALPEN(elemPtr);
    if ((penPtr->bg == NULL) && (penPtr->fgColor == NULL)) {
	return;
    }
    radius = (size / 2);
    size--;

    x -= radius;
    y -= radius;
    XSetTSOrigin(graphPtr->display, penPtr->gc, x, y);
    XFillRectangle(graphPtr->display, drawable, penPtr->gc, x, y, size, size);
    XSetTSOrigin(graphPtr->display, penPtr->gc, 0, 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawContourSegments --
 *
 * 	Draws each of the rectangular segments for the element.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawContourSegments(
    Graph *graphPtr,
    Drawable drawable,		/* Pixmap or window to draw into */
    ContourPen *penPtr,
    XRectangle *bars,
    int nBars)
{

    if ((penPtr->bg == NULL) && (penPtr->fgColor == NULL)) {
	return;
    }
    if (penPtr->bg != NULL) {
	XRectangle *rp, *rend;

	for (rp = bars, rend = rp + nBars; rp < rend; rp++) {
	    Blt_FillBackgroundRectangle(graphPtr->tkwin, drawable, penPtr->bg,
		rp->x, rp->y, rp->width, rp->height, penPtr->borderWidth, 
		penPtr->relief);
	}
    }
    if (penPtr->fgColor != NULL) {
	XFillRectangles(graphPtr->display, drawable, penPtr->gc, bars, nBars);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawContourValues --
 *
 * 	Draws the numeric value of the bar.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawContourValues(
    Graph *graphPtr, 
    Drawable drawable, 
    ContourElement *elemPtr,
    ContourPen *penPtr,
    XRectangle *bars,
    int nBars,
    int *barToData)
{
    XRectangle *rp, *rend;
    int count;
    char *fmt;
    
    fmt = penPtr->valueFormat;
    if (fmt == NULL) {
	fmt = "%g";
    }
    count = 0;
    for (rp = bars, rend = rp + nBars; rp < rend; rp++) {
	Point2d anchorPos;
	double x, y;
	char string[TCL_DOUBLE_SPACE * 2 + 2];

	x = elemPtr->x.values[barToData[count]];
	y = elemPtr->y.values[barToData[count]];

	count++;
	if (penPtr->valueShow == SHOW_X) {
	    sprintf_s(string, TCL_DOUBLE_SPACE, fmt, x); 
	} else if (penPtr->valueShow == SHOW_Y) {
	    sprintf_s(string, TCL_DOUBLE_SPACE, fmt, y); 
	} else if (penPtr->valueShow == SHOW_BOTH) {
	    sprintf_s(string, TCL_DOUBLE_SPACE, fmt, x);
	    strcat(string, ",");
	    sprintf_s(string + strlen(string), TCL_DOUBLE_SPACE, fmt, y);
	}
	if (graphPtr->inverted) {
	    anchorPos.y = rp->y + rp->height * 0.5;
	    anchorPos.x = rp->x + rp->width;
	    if (y < graphPtr->baseline) {
		anchorPos.x -= rp->width;
	    } 
	} else {
	    anchorPos.x = rp->x + rp->width * 0.5;
	    anchorPos.y = rp->y;
	    if (y < graphPtr->baseline) {			
		anchorPos.y += rp->height;
	    }
	}
	Blt_DrawText(graphPtr->tkwin, drawable, string, &penPtr->valueStyle, 
		     (int)anchorPos.x, (int)anchorPos.y);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawNormalContourProc --
 *
 *	Draws the rectangle representing the bar element.  If the
 *	relief option is set to "raised" or "sunken" and the bar
 *	borderwidth is set (borderwidth > 0), a 3D border is drawn
 *	around the bar.
 *
 *	Don't draw bars that aren't visible (i.e. within the limits
 *	of the axis).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	X drawing commands are output.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawNormalContourProc(Graph *graphPtr, Drawable drawable, Element *basePtr)
{
    ContourElement *elemPtr = (ContourElement *)basePtr;
    int count;
    Blt_ChainLink link;

    count = 0;
    for (link = Blt_Chain_FirstLink(elemPtr->stylePalette); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	ContourStyle *stylePtr;
	ContourPen *penPtr;

	stylePtr = Blt_Chain_GetValue(link);
	penPtr = stylePtr->penPtr;
	if (stylePtr->nBars > 0) {
	    DrawBarSegments(graphPtr, drawable, penPtr, stylePtr->bars,
		stylePtr->nBars);
	}
	if ((stylePtr->xeb.length > 0) && (penPtr->errorBarShow & SHOW_X)) {
	    Blt_Draw2DSegments(graphPtr->display, drawable, penPtr->errorBarGC, 
		       stylePtr->xeb.segments, stylePtr->xeb.length);
	}
	if ((stylePtr->yeb.length > 0) && (penPtr->errorBarShow & SHOW_Y)) {
	    Blt_Draw2DSegments(graphPtr->display, drawable, penPtr->errorBarGC, 
		       stylePtr->yeb.segments, stylePtr->yeb.length);
	}
	if (penPtr->valueShow != SHOW_NONE) {
	    DrawBarValues(graphPtr, drawable, elemPtr, penPtr, 
			stylePtr->bars, stylePtr->nBars, 
			elemPtr->barToData + count);
	}
	count += stylePtr->nBars;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawActiveContourProc --
 *
 *	Draws contours representing the active segments of the
 *	bar element.  If the -relief option is set (other than "flat")
 *	and the borderwidth is greater than 0, a 3D border is drawn
 *	around the each bar segment.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	X drawing commands are output.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawActiveContourProc(Graph *graphPtr, Drawable drawable, Element *basePtr)
{
    ContourElement *elemPtr = (ContourElement *)basePtr;

    if (elemPtr->activePenPtr != NULL) {
	ContourPen *penPtr = elemPtr->activePenPtr;

	if (elemPtr->nActiveIndices > 0) {
	    if (elemPtr->flags & ACTIVE_PENDING) {
		MapActiveBars(elemPtr);
	    }
	    DrawBarSegments(graphPtr, drawable, penPtr, elemPtr->activeRects, 
			 elemPtr->nActive);
	    if (penPtr->valueShow != SHOW_NONE) {
		DrawBarValues(graphPtr, drawable, elemPtr, penPtr, 
			elemPtr->activeRects, elemPtr->nActive, 
			elemPtr->activeToData);
	    }
	} else if (elemPtr->nActiveIndices < 0) {
	    DrawBarSegments(graphPtr, drawable, penPtr, elemPtr->bars, 
			 elemPtr->nBars);
	    if (penPtr->valueShow != SHOW_NONE) {
		DrawBarValues(graphPtr, drawable, elemPtr, penPtr, 
			elemPtr->bars, elemPtr->nBars, elemPtr->barToData);
	    }
	}
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * SymbolToPostScript --
 *
 * 	Draw a symbol centered at the given x,y window coordinate
 *	based upon the element symbol type and size.
 *
 * Results:
 *	None.
 *
 * Problems:
 *	Most notable is the round-off errors generated when
 *	calculating the centered position of the symbol.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
SymbolToPostScriptProc(
    Graph *graphPtr,
    Blt_Ps ps,
    Element *basePtr,
    double x, double y,
    int size)
{
    ContourElement *elemPtr = (ContourElement *)basePtr;
    ContourPen *penPtr;

    penPtr = NORMALPEN(elemPtr);
    if ((penPtr->bg == NULL) && (penPtr->fgColor == NULL)) {
	return;
    }
    /*
     * Build a PostScript procedure to draw the fill and outline of
     * the symbol after the path of the symbol shape has been formed
     */
    Blt_Ps_VarAppend(ps, "\n",
		     "/DrawSymbolProc {\n",
		     "  gsave\n    ", (char *)NULL);
    if (penPtr->stipple != None) {
	if (penPtr->bg != NULL) {
	    Blt_Ps_XSetBackground(ps, Blt_BackgroundBorderColor(penPtr->bg));
	    Blt_Ps_Append(ps, "    gsave fill grestore\n    ");
	}
	if (penPtr->fgColor != NULL) {
	    Blt_Ps_XSetForeground(ps, penPtr->fgColor);
	} else {
	    Blt_Ps_XSetForeground(ps, Blt_BackgroundBorderColor(penPtr->bg));
	}
	Blt_Ps_XSetStipple(ps, graphPtr->display, penPtr->stipple);
    } else if (penPtr->fgColor != NULL) {
	Blt_Ps_XSetForeground(ps, penPtr->fgColor);
	Blt_Ps_Append(ps, "    fill\n");
    }
    Blt_Ps_Append(ps, "  grestore\n");
    Blt_Ps_Append(ps, "} def\n\n");
    Blt_Ps_Format(ps, "%g %g %d Sq\n", x, y, size);
}

static void
SegmentsToPostScript(
    Graph *graphPtr,
    Blt_Ps ps,
    ContourPen *penPtr,
    XRectangle *bars,
    int nBars)
{
    XRectangle *rp, *rend;

    if ((penPtr->bg == NULL) && (penPtr->fgColor == NULL)) {
	return;
    }
    for (rp = bars, rend = rp + nBars; rp < rend; rp++) {
	if ((rp->width < 1) || (rp->height < 1)) {
	    continue;
	}
	if (penPtr->stipple != None) {
	    Blt_Ps_Rectangle(ps, rp->x, rp->y, rp->width - 1, rp->height - 1);
	    if (penPtr->bg != NULL) {
		Blt_Ps_XSetBackground(ps,Blt_BackgroundBorderColor(penPtr->bg));
		Blt_Ps_Append(ps, "gsave fill grestore\n");
	    }
	    if (penPtr->fgColor != NULL) {
		Blt_Ps_XSetForeground(ps, penPtr->fgColor);
	    } else {
		Blt_Ps_XSetForeground(ps,Blt_BackgroundBorderColor(penPtr->bg));
	    }
	    Blt_Ps_XSetStipple(ps, graphPtr->display, penPtr->stipple);
	} else if (penPtr->fgColor != NULL) {
	    Blt_Ps_XSetForeground(ps, penPtr->fgColor);
	    Blt_Ps_XFillRectangle(ps, (double)rp->x, (double)rp->y, 
		(int)rp->width - 1, (int)rp->height - 1);
	}
	if ((penPtr->bg != NULL) && (penPtr->borderWidth > 0) && 
	    (penPtr->relief != TK_RELIEF_FLAT)) {
	    Blt_Ps_Draw3DRectangle(ps, Blt_BackgroundBorder(penPtr->bg), 
		(double)rp->x, (double)rp->y, (int)rp->width, (int)rp->height,
		penPtr->borderWidth, penPtr->relief);
	}
    }
}

static void
BarValuesToPostScript(
    Graph *graphPtr,
    Blt_Ps ps,
    ContourElement *elemPtr,
    ContourPen *penPtr,
    XRectangle *bars,
    int nBars,
    int *barToData)
{
    XRectangle *rp, *rend;
    int count;
    char *fmt;
    char string[TCL_DOUBLE_SPACE * 2 + 2];
    double x, y;
    Point2d anchorPos;
    
    count = 0;
    fmt = penPtr->valueFormat;
    if (fmt == NULL) {
	fmt = "%g";
    }
    for (rp = bars, rend = rp + nBars; rp < rend; rp++) {
	x = elemPtr->x.values[barToData[count]];
	y = elemPtr->y.values[barToData[count]];
	count++;
	if (penPtr->valueShow == SHOW_X) {
	    sprintf_s(string, TCL_DOUBLE_SPACE, fmt, x); 
	} else if (penPtr->valueShow == SHOW_Y) {
	    sprintf_s(string, TCL_DOUBLE_SPACE, fmt, y); 
	} else if (penPtr->valueShow == SHOW_BOTH) {
	    sprintf_s(string, TCL_DOUBLE_SPACE, fmt, x);
	    strcat(string, ",");
	    sprintf_s(string + strlen(string), TCL_DOUBLE_SPACE, fmt, y);
	}
	if (graphPtr->inverted) {
	    anchorPos.y = rp->y + rp->height * 0.5;
	    anchorPos.x = rp->x + rp->width;
	    if (y < graphPtr->baseline) {
		anchorPos.x -= rp->width;
	    } 
	} else {
	    anchorPos.x = rp->x + rp->width * 0.5;
	    anchorPos.y = rp->y;
	    if (y < graphPtr->baseline) {			
		anchorPos.y += rp->height;
	    }
	}
	Blt_Ps_DrawText(ps, string, &penPtr->valueStyle, anchorPos.x, 
		anchorPos.y);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ActiveBarToPostScript --
 *
 *	Similar to the NormalBarToPostScript procedure, generates
 *	PostScript commands to display the bars representing the
 *	active bar segments of the element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	PostScript pen width, dashes, and color settings are changed.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
ActiveBarToPostScriptProc(
    Graph *graphPtr,
    Blt_Ps ps,
    Element *basePtr)
{
    ContourElement *elemPtr = (ContourElement *)basePtr;

    if (elemPtr->activePenPtr != NULL) {
	ContourPen *penPtr = elemPtr->activePenPtr;
	
	if (elemPtr->nActiveIndices > 0) {
	    if (elemPtr->flags & ACTIVE_PENDING) {
		MapActiveBars(elemPtr);
	    }
	    SegmentsToPostScript(graphPtr, ps, penPtr, elemPtr->activeRects,
		elemPtr->nActive);
	    if (penPtr->valueShow != SHOW_NONE) {
		BarValuesToPostScript(graphPtr, ps, elemPtr, penPtr, 
		   elemPtr->activeRects, elemPtr->nActive, elemPtr->activeToData);
	    }
	} else if (elemPtr->nActiveIndices < 0) {
	    SegmentsToPostScript(graphPtr, ps, penPtr, elemPtr->bars, 
		elemPtr->nBars);
	    if (penPtr->valueShow != SHOW_NONE) {
		BarValuesToPostScript(graphPtr, ps, elemPtr, penPtr, 
		   elemPtr->bars, elemPtr->nBars, elemPtr->barToData);
	    }
	}
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * NormalBarToPostScript --
 *
 *	Generates PostScript commands to form the bars
 *	representing the segments of the bar element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	PostScript pen width, dashes, and color settings are changed.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
NormalBarToPostScriptProc(
    Graph *graphPtr,
    Blt_Ps ps,
    Element *basePtr)
{
    ContourElement *elemPtr = (ContourElement *)basePtr;
    Blt_ChainLink link;
    int count;

    count = 0;
    for (link = Blt_Chain_FirstLink(elemPtr->stylePalette); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	ContourStyle *stylePtr;
	ContourPen *penPtr;
	XColor *colorPtr;

	stylePtr = Blt_Chain_GetValue(link);
	penPtr = stylePtr->penPtr;
	if (stylePtr->nBars > 0) {
	    SegmentsToPostScript(graphPtr, ps, penPtr, stylePtr->bars, 
		stylePtr->nBars);
	}
	colorPtr = penPtr->errorBarColor;
	if (colorPtr == COLOR_DEFAULT) {
	    colorPtr = penPtr->fgColor;
	}
	if ((stylePtr->xeb.length > 0) && (penPtr->errorBarShow & SHOW_X)) {
	    Blt_Ps_XSetLineAttributes(ps, colorPtr, 
		penPtr->errorBarLineWidth, NULL, CapButt, JoinMiter);
	    Blt_Ps_Draw2DSegments(ps, stylePtr->xeb.segments,
		stylePtr->xeb.length);
	}
	if ((stylePtr->yeb.length > 0) && (penPtr->errorBarShow & SHOW_Y)) {
	    Blt_Ps_XSetLineAttributes(ps, colorPtr, 
		penPtr->errorBarLineWidth, NULL, CapButt, JoinMiter);
	    Blt_Ps_Draw2DSegments(ps, stylePtr->yeb.segments,
		stylePtr->yeb.length);
	}
	if (penPtr->valueShow != SHOW_NONE) {
	    BarValuesToPostScript(graphPtr, ps, elemPtr, penPtr, 
		stylePtr->bars, stylePtr->nBars, elemPtr->barToData + count);
	}
	count += stylePtr->nBars;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * DestroyBar --
 *
 *	Release memory and resources allocated for the bar element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the bar element is freed up.
 *
 *---------------------------------------------------------------------------
 */

static void
DestroyBarProc(Graph *graphPtr, Element *basePtr)
{
    ContourElement *elemPtr = (ContourElement *)basePtr;

    DestroyContourPen(graphPtr, elemPtr->builtinPenPtr);
    if (elemPtr->activePenPtr != NULL) {
	Blt_FreePen((Pen *)elemPtr->activePenPtr);
    }
    ResetBar(elemPtr);
    if (elemPtr->stylePalette != NULL) {
	Blt_FreeStylePalette(elemPtr->stylePalette);
	Blt_Chain_Destroy(elemPtr->stylePalette);
    }
    if (elemPtr->activeIndices != NULL) {
	Blt_Free(elemPtr->activeIndices);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_ContourElement --
 *
 *	Allocate memory and initialize methods for the new bar element.
 *
 * Results:
 *	The pointer to the newly allocated element structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the bar element structure.
 *
 *---------------------------------------------------------------------------
 */

static ElementProcs barProcs =
{
    ClosestBarProc,
    ConfigureBarProc,
    DestroyBarProc,
    DrawActiveBarProc,
    DrawNormalBarProc,
    DrawSymbolProc,
    GetBarExtentsProc,
    ActiveBarToPostScriptProc,
    NormalBarToPostScriptProc,
    SymbolToPostScriptProc,
    MapBarProc,
};


Element *
Blt_ContourElement(Graph *graphPtr, char *name, ClassId classId)
{
    ContourElement *elemPtr;

    elemPtr = Blt_AssertCalloc(1, sizeof(ContourElement));
    elemPtr->procsPtr = &barProcs;
    elemPtr->configSpecs = contourElemConfigSpecs;
    elemPtr->labelRelief = TK_RELIEF_FLAT;
    Blt_GraphSetObjectClass(&elemPtr->object, classId);
    elemPtr->obj.name = Blt_AssertStrdup(name);
    elemPtr->obj.graphPtr = graphPtr;

    /* By default, an element's name and label are the same. */
    elemPtr->label = Blt_AssertStrdup(name);
    elemPtr->builtinPenPtr = &elemPtr->builtinPen;
    InitContourPen(elemPtr->builtinPenPtr);
    elemPtr->stylePalette = Blt_Chain_Create();
    bltContourStylesOption.clientData = (ClientData)sizeof(ContourStyle);
    return (Element *)elemPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_InitFreqTable --
 *
 *	Generate a table of abscissa frequencies.  Duplicate
 *	x-coordinates (depending upon the bar drawing mode) indicate
 *	that something special should be done with each bar segment
 *	mapped to the same abscissa (i.e. it should be stacked,
 *	aligned, or overlay-ed with other segments)
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is allocated for the bar element structure.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_InitFreqTable(Graph *graphPtr)
{
    Blt_ChainLink link;
    int nStacks, nSegs;
    Blt_HashTable freqTable;

    /*
     * Free resources associated with a previous frequency table. This
     * includes the array of frequency information and the table itself
     */
    if (graphPtr->freqArr != NULL) {
	Blt_Free(graphPtr->freqArr);
	graphPtr->freqArr = NULL;
    }
    if (graphPtr->nStacks > 0) {
	Blt_DeleteHashTable(&graphPtr->freqTable);
	graphPtr->nStacks = 0;
    }
    if (graphPtr->mode == MODE_INFRONT) {
	return;			/* No frequency table is needed for
				 * "infront" mode */
    }
    Blt_InitHashTable(&graphPtr->freqTable, sizeof(FreqKey) / sizeof(int));

    /*
     * Initialize a hash table and fill it with unique abscissas.
     * Keep track of the frequency of each x-coordinate and how many
     * abscissas have duplicate mappings.
     */
    Blt_InitHashTable(&freqTable, sizeof(FreqKey) / sizeof(int));
    nSegs = nStacks = 0;
    for (link = Blt_Chain_FirstLink(graphPtr->elements.displayList);
	link != NULL; link = Blt_Chain_NextLink(link)) {
	ContourElement *elemPtr;
	double *xArr;
	int nPoints;
	int i;

	elemPtr = Blt_Chain_GetValue(link);
	if ((elemPtr->flags & (HIDE|DELETE_PENDING) || 
	    (elemPtr->obj.classId != CID_ELEM_CONTOUR)) {
	    continue;
	}
	nSegs++;
	xArr = elemPtr->x.values;
	nPoints = NUMBEROFPOINTS(elemPtr);
	for (i = 0; i < nPoints; i++) {
	    Blt_HashEntry *hPtr;
	    FreqKey key;
	    int isNew;
	    size_t count;

	    key.value = xArr[i];
	    key.axes = elemPtr->axes;
	    hPtr = Blt_CreateHashEntry(&freqTable, (char *)&key, &isNew);
	    if (isNew) {
		count = 1;
	    } else {
		count = (size_t)Blt_GetHashValue(hPtr);
		if (count == 1) {
		    nStacks++;
		}
		count++;
	    }
	    Blt_SetHashValue(hPtr, (ClientData)count);
	}
    }
    if (nSegs == 0) {
	return;			/* No bar elements to be displayed */
    }
    if (nStacks > 0) {
	FreqInfo *fp;
	Blt_HashEntry *h1;
	Blt_HashSearch cursor;

	graphPtr->freqArr = Blt_AssertCalloc(nStacks, sizeof(FreqInfo));
	fp = graphPtr->freqArr;
	for (h1 = Blt_FirstHashEntry(&freqTable, &cursor); h1 != NULL;
	    h1 = Blt_NextHashEntry(&cursor)) {
	    FreqKey *keyPtr;
	    size_t count;

	    count = (size_t)Blt_GetHashValue(h1);
	    keyPtr = (FreqKey *)Blt_GetHashKey(&freqTable, h1);
	    if (count > 1) {
		Blt_HashEntry *h2;
		int isNew;

		h2 = Blt_CreateHashEntry(&graphPtr->freqTable, (char *)keyPtr, 
			&isNew);
		count = (size_t)Blt_GetHashValue(h1);
		fp->freq = count;
		fp->axes = keyPtr->axes;
		Blt_SetHashValue(h2, fp);
		fp++;
	    }
	}
    }
    Blt_DeleteHashTable(&freqTable);
    graphPtr->nStacks = nStacks;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_ComputeStacks --
 *
 *	Determine the height of each stack of bar segments.  A stack
 *	is created by designating two or more points with the same
 *	abscissa.  Each ordinate defines the height of a segment in
 *	the stack.  This procedure simply looks at all the data points
 *	summing the heights of each stacked segment. The sum is saved
 *	in the frequency information table.  This value will be used
 *	to calculate the y-axis limits (data limits aren't sufficient).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The heights of each stack is computed. CheckStacks will
 *	use this information to adjust the y-axis limits if necessary.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_ComputeStacks(Graph *graphPtr)
{
    Blt_ChainLink link;

    if ((graphPtr->mode != MODE_STACKED) || (graphPtr->nStacks == 0)) {
	return;
    }

    /* Reset the sums for all duplicate values to zero. */
    {
	FreqInfo *infoPtr;
	int i;

	infoPtr = graphPtr->freqArr;
	for (i = 0; i < graphPtr->nStacks; i++) {
	    infoPtr->sum = 0.0;
	    infoPtr++;
	}
    }

    /* Look at each bar point, adding the ordinates of duplicate abscissas */

    for (link = Blt_Chain_FirstLink(graphPtr->elements.displayList); 
	 link != NULL; link = Blt_Chain_NextLink(link)) {
	ContourElement *elemPtr;
	double *xArr, *yArr;
	int nPoints;
	int i;

	elemPtr = Blt_Chain_GetValue(link);
	if ((elemPtr->flags & (HIDE|DELETE_PENDING) || 
	    (elemPtr->obj.classId != CID_ELEM_CONTOUR)) {
	    continue;
	}
	xArr = elemPtr->x.values;
	yArr = elemPtr->y.values;
	nPoints = NUMBEROFPOINTS(elemPtr);
	for (i = 0; i < nPoints; i++) {
	    Blt_HashEntry *hPtr;
	    FreqKey key;
	    FreqInfo *infoPtr;

	    key.value = xArr[i];
	    key.axes = elemPtr->axes;
	    hPtr = Blt_FindHashEntry(&graphPtr->freqTable, (char *)&key);
	    if (hPtr == NULL) {
		continue;
	    }
	    infoPtr = Blt_GetHashValue(hPtr);
	    infoPtr->sum += yArr[i];
	}
    }
}

void
Blt_ResetStacks(Graph *graphPtr)
{
    FreqInfo *fp, *fend;

    for (fp = graphPtr->freqArr, fend = fp+graphPtr->nStacks; fp < fend; fp++) {
	fp->lastY = 0.0;
	fp->count = 0;
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * GetScreenPoints --
 *
 *	Generates a coordinate array of transformed screen coordinates from
 *	the data points.
 *
 * Results:
 *	The transformed screen coordinates are returned.
 *
 * Side effects:
 *	Memory is allocated for the coordinate array.
 *
 *---------------------------------------------------------------------------
 */
static void
GetScreenPoints(Graph *graphPtr, LineElement *elemPtr, MapInfo *mapPtr)
{
    double *x, *y;
    int i, n;
    int count;
    Point3 *points;
    int *map;
    MeshKey key;

    n = MIN3(elemPtr->x.nValues, elemPtr->y.nValues, elemPtr->z.nValues);
    x = elemPtr->x.values;
    y = elemPtr->y.values;
    z = elemPtr->z.values;
    points = Blt_AssertMalloc(sizeof(Point3) * n);
    map = Blt_AssertMalloc(sizeof(int) * n);
    count = 0;			/* Count the valid screen coordinates */

    next = 0;
    zmin = zmax = 0.0;		/* Suppress compiler warning. */

    Blt_InitHashTable(&table, sizeof(MeshKey) / sizeof(int));
    p = points;
    p->x = x[0];
    p->y = y[0];
    p->z = z[0];
    zmin = zmax = p->z;
    p++;
    /*
     * Generate array of points. Check for duplicates.  This is a quick+dirty
     * (slow) solution.
     */
    hPtr = Blt_CreateHashEntry(&table, (char *)&key, &isNew);
    for (i = 1; i < n; i++) {
	Blt_HashEntry *hPtr;
	MeshKey key;

	key.x = x[i];
	key.y = y[i];

	hPtr = Blt_CreateHashEntry(&table, (char *)&key, &isNew);
	if (!isNew) {
	    continue;		/* Already have coordinate pair. */
	}
	p->x = key.x;
	p->y = key.y;
	p->z = z[i];
	if (p->z < zMin) {
	    zMin = p->z;
	} else if (p->z > zMax) {
	    zMax = p->z;
	}
	p++;
    }
    Tcl_DeleteHashTable(&table);
    nPnts = points - p;
    tol = (zMax - zMin) * tol;
    
    /*
     * Mesh the points to get triangles 
     */
    R = Blt_Malloc(2 * nPnts * sizeof(Triplet));
    if (R == NULL) {
	Tcl_AppendResult(interp, "can't allocate ", Itoa(nPnts), " triplets",
	    (char *)NULL);
	return TCL_ERROR;
    }
    nTriangles = Triangulate(nPnts, points, R);
    assert(nTriangles <= 2 * nPnts);

    if (graphPtr->inverted) {
	for (i = 0; i < n; i++) {
	    if ((FINITE(x[i])) && (FINITE(y[i]))) {
 		points[count].x = Blt_HMap(elemPtr->axes.y, y[i]);
		points[count].y = Blt_VMap(elemPtr->axes.x, x[i]);
		map[count] = i;
		count++;
	    }
	}
    } else {
	for (i = 0; i < n; i++) {
	    if ((FINITE(x[i])) && (FINITE(y[i]))) {
		points[count].x = Blt_HMap(elemPtr->axes.x, x[i]);
		points[count].y = Blt_VMap(elemPtr->axes.y, y[i]);
		map[count] = i;
		count++;
	    }
	}
    }
    mapPtr->screenPts = points;
    mapPtr->nScreenPts = count;
    mapPtr->map = map;
}



/*
 *---------------------------------------------------------------------------
 *
 * IsolineOp --
 *
 *	.g isoline create -name $name -value $value -color $color -pen $pen
 *	.g isoline configure $name -value $value -color $color -hide no
 *	.g isoline delete $name
 *	.g isoline names
 *
 *---------------------------------------------------------------------------
 */

/*
 *---------------------------------------------------------------------------
 *
 * IsolineOp --
 *
 *	.g isoline create -name $name -value $value -color $color
 *	.g isoline configure $name -value $value -color $color -hide no
 *	.g isoline 
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
IsolineOp(
    Graph *graphPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    if (objc == 3) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch iter;
	char *tagName;
	Tcl_Obj *listObjPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
	for (hPtr = Blt_FirstHashEntry(&graphPtr->elements.tagTable, &iter);
	     hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	    tagName = Blt_GetHashKey(&graphPtr->elements.tagTable, hPtr);
	    Tcl_ListObjAppendElement(interp, listObjPtr, 
				     Tcl_NewStringObj(tagName, -1));
	}
	Tcl_SetObjResult(interp, listObjPtr);
	return TCL_OK;
    }
    return Blt_ConfigureBindingsFromObj(interp, graphPtr->bindTable,
	Blt_MakeElementTag(graphPtr, Tcl_GetString(objv[3])), 
	objc - 4, objv + 4);
}


/* make up an edge */
static INLINE void
MakeEdgeKey(Edge *keyPtr, int a, int b) 
{
    if (a < b) {
	keyPtr->a = a;
	keyPtr->b = b;
    } else {
	keyPtr->a = b;
	keyPtr->b = a;
    }
}

/* interpolate 2 points and add possible result to points array */
static int 
Interpolate(ContourElement *contPtr, int a, int b, double isovalue) 
{
    int ab, junk;
    Edge *edgePtr;
    EdgeInfo *eiPtr;
    Blt_HashEntry *hPtr;
    double zA, zB, r, absr;
    Point2f *points = contPtr->meshPtr->points;
    EdgeKey key;

    /* Create the edge key. */
    MakeEdgeKey(&key, a, b);

    hPtr = Blt_CreateHashEntry(contPtr->edgeTable, (char *)key, &isNew);
    if (!isNew) {
	int index;
 
	index = (int)Blt_GetHashValue(hPtr);
#if DEBUG
	printf("A=%d(%g,%g,%g) B=%d(%g,%g,%g) isovalue=%g -> old %d\n",
	       a, points[a].x, points[a].y, values[a],
	       b, points[b].x, points[b].y, values[b],
	       isovalue, index);
#endif
	return index;
    }

    /* No, then do the interpolation */
    zA = contPtr->values[a] - isovalue;
    if (fabs(zA/isovalue) < contPtr->fuzz) {
	zA = 0.0;
    }
    zB = contPtr->values[b] - isovalue;
    if (fabs(zB/isovalue) < contPtr->fuzz) {
	zB = 0.0;
    }
    /* Does there exist an intermediate point? */
    if (sign(zA) != sign(zB)) {
	double t;
	int i;

	i = contPtr->nAdditional;
	t = zA / (zA - zB);
	points[i].x = points[a].x + t * (points[b].x - points[a].x);
	points[i].y = points[a].y + t * (points[b].y - points[a].y);
	values[i] = isovalue;
	ab = i;
	contPtr->nAdditional++;
    } else {
	ab = 0;
    }
#if DEBUG
    printf("A=%d(%g,%g,%g) B=%d(%g,%g,%g) isovalue=%g -> new %d\n",
	   a, points[a].x, points[a].y, values[a],
	   b, points[b].x, points[b].y, values[b],
	   isovalue, ab);
#endif
    /* add the new point to the hash table */
    Blt_SetHashValue(hPtr, (ClientData)ab);
    return ab;
}

/* process a Cont triangle  */
void 
ProcessTriangle(ContourElement *contPtr, Triangle *t, float isovalue) 
{
    int ab, bc, ca;

    if (isovalue <= t->min) {
	if ((t->a != t->b) && (t->b != t->c) && (t->a != t->c)) {
	    AddTriangle(t->a, t->b, t->c);
	}
	return;
    }
    /* Interpolate the three sides of the triangle. */
    ab = Interpolate(contPtr, t->a, t->b, isovalue);
    bc = Interpolate(contPtr, t->b, t->c, isovalue);
    ca = Interpolate(contPtr, t->c, t->a, isovalue);
    if ((ab) && (bc) && (!ca)) {
	if (contPtr->values[t->b] > isovalue) {
	    AddTriangle(ab, t->b, bc);
	} else {
	    AddQuadrilateral(t->a, ab, bc, t->c);
	}
	return;
    }
    if ((ab) && (!bc) && (ca)) {
	if (contPtr->values[t->a] > isovalue) {
	    AddTriangle(t->a, ab, ca);
	} else {
	    AddQuadrilateral(ab, t->b, t->c, ca);
	}
	return;
    }
    if ((!ab) && (bc) && (ca)) {
	if (contPtr->values[t->c] > isovalue) {
	    AddTriangle(bc, t->c, ca);
	} else {
	    AddQuadrilateral(t->b, bc, ca, t->a);
	}
	return;
    }
}


static int
CompareTriangles(const void *a, const void *b)
{
    const Triangle *t1 = a;
    const Triangle *t2 = b;

    /* Sort by the minimum. */
    if (t1->min < t2->min) {
	return -1;
    } else if (t1->min > t2->min) {
	return 1;
    }
    /* Secondarily sort by the maximum. */
    if (t1->max < t2->max) {
	return -1;
    } else if (t1->max > t2->max) {
	return 1;
    }
    return 0;
}

static void
SortTriangles(Mesh *meshPtr, int nValues, float *values)
{
    /* Compute the min/max of the z-values of the triangle */
    for (i = 0; i < meshPtr->nTriangles; i++) {
	Triangle *t;

	t = meshPtr->triangles + i;
	t->min = t->max = values[t->a];
	if (values[t->b] > t->max) {
	    t->max = values[t->b];
	} else if (values[t->b] < t->min) {
	    t->min = values[t->b];
	}
	if (values[t->c] > t->max) {
	    t->max = values[t->c];
	} else if (values[t->c] < t->min) {
	    t->min = values[t->c];
	}
    }
    /* Sort the triangles by the current set of values associates with the
     * mesh. */
    qsort(meshPtr->triangles, meshPtr->nTriangles, sizeof(Triangle), 
	  CompareTriangles);
}

/* Display a cont widget */
static int
MapContour(Tcl_Interp *interp, Element *elemPtr)
{
    ContourElement *contPtr = (ContourElement *)elemPtr;
    Polygon *P;
    int i, k, N, M;
    Tcl_Obj *listObjPtr, *subListObjPtr, *objPtr;

    init_extra_points(contPtr);
    init_poly_table();

    SortTriangles(contPtr->meshPtr, contPtr->nValues, contPtr->values);
    for (i = 0; i < contPtr->nIsolines; i++) {
	double z;

	z = contPtr->isolines[i];
	for (j = 0; j < contPtr->meshPtr->nTriangles; j++) {
	    Triangle *t;

	    t = contPtr->meshPtr->triangles + j;
	    if (z < t->min) {
		break;		/* No more triangles in range. */
	    }
	    if (z > t->max) {
		continue;	/* Not in range. */
	    }
	    ProcessTriangle(contPtr, t, z);
	}
    }
}
