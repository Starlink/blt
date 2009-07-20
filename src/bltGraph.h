
/*
 * bltGraph.h --
 *
 *	Copyright 1993-2004 George A Howlett.
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

#ifndef _BLT_GRAPH_H
#define _BLT_GRAPH_H

#include "bltInt.h"
#include "bltHash.h"
#include "bltBind.h"
#include "bltChain.h"
#include "bltPs.h"
#include "bltBgStyle.h"

typedef struct _Graph Graph;
typedef struct _Element Element;
typedef struct _Legend Legend;

typedef enum {
    CID_NONE, 
    CID_AXIS_X,
    CID_AXIS_Y,
    CID_ELEM_BAR,
    CID_ELEM_CONTOUR,
    CID_ELEM_LINE,
    CID_ELEM_STRIP,
    CID_MARKER_BITMAP,
    CID_MARKER_IMAGE,
    CID_MARKER_LINE,
    CID_MARKER_POLYGON,
    CID_MARKER_TEXT,
    CID_MARKER_WINDOW,
    CID_LEGEND_ENTRY,
} ClassId;

typedef struct {
    /* Generic fields common to all graph objects. */
    ClassId classId;		/* Class type of object. */
    const char *name;		/* Identifier to refer the object. */
    const char *className;	/* Class name of object. */

    Graph *graphPtr;		/* Graph containing of the object. */

    const char **tags;		/* Binding tags for the object. */
} GraphObj;

#include "bltGrAxis.h"
#include "bltGrLegd.h"

#define MARKER_UNDER	1	/* Draw markers designated to lie underneath
				 * elements, grids, legend, etc. */
#define MARKER_ABOVE	0	/* Draw markers designated to rest above
				 * elements, grids, legend, etc. */

#define PADX		2	/* Padding between labels/titles */
#define PADY    	2	/* Padding between labels */

#define MINIMUM_MARGIN	20	/* Minimum margin size */


#define BOUND(x, lo, hi)	 \
	(((x) > (hi)) ? (hi) : ((x) < (lo)) ? (lo) : (x))

/*
 *---------------------------------------------------------------------------
 *
 * 	Graph component structure definitions
 *
 *---------------------------------------------------------------------------
 */
#define PointInGraph(g,x,y) \
	(((x) <= (g)->right) && ((x) >= (g)->left) && \
	 ((y) <= (g)->bottom) && ((y) >= (g)->top))

/*
 * Mask values used to selectively enable GRAPH or BARCHART entries in the
 * various configuration specs.
 */
#define GRAPH			(BLT_CONFIG_USER_BIT << 1)
#define STRIPCHART		(BLT_CONFIG_USER_BIT << 2)
#define BARCHART		(BLT_CONFIG_USER_BIT << 3)
#define LINE_GRAPHS		(GRAPH | STRIPCHART)
#define ALL_GRAPHS		(GRAPH | BARCHART | STRIPCHART)

#define ACTIVE_PEN		(BLT_CONFIG_USER_BIT << 16)
#define NORMAL_PEN		(BLT_CONFIG_USER_BIT << 17)
#define ALL_PENS		(NORMAL_PEN | ACTIVE_PEN)

typedef struct {
    Segment2d *segments;
    int length;
    int *map;
} GraphSegments;

typedef struct {
    Point2d *points;
    int length;
    int *map;
} GraphPoints;


/*
 *---------------------------------------------------------------------------
 *
 * FreqInfo --
 *
 *---------------------------------------------------------------------------
 */

typedef struct {
    int freq;			/* Number of occurrences of x-coordinate */
    Axis2d axes;		/* Indicates which x and y axis are mapped to
				 * the x-value */
    float sum;			/* Sum of the ordinates of each duplicate
				 * abscissa */
    int count;
    float lastY;

} FreqInfo;

/*
 *---------------------------------------------------------------------------
 *
 * FreqKey --
 *
 *
 *---------------------------------------------------------------------------
 */
typedef struct {
    float value;		/* Duplicated abscissa */
    Axis2d axes;		/* Axis mapping of element */
} FreqKey;

/*
 * BarModes --
 *
 *	Bar elements are displayed according to their x-y coordinates.  If two
 *	bars have the same abscissa (x-coordinate), the bar segments will be
 *	drawn according to one of the following modes:
 */

typedef enum BarModes {
    MODE_INFRONT,		/* Each successive segment is drawn in front
				 * of the previous. */
    MODE_STACKED,		/* Each successive segment is drawn stacked
				 * above the previous. */
    MODE_ALIGNED,		/* Each successive segment is drawn aligned to
				 * the previous from right-to-left. */
    MODE_OVERLAP		/* Like "aligned", each successive segment is
				 * drawn from right-to-left. In addition the
				 * segments will overlap each other by a small
				 * amount */
} BarMode;

typedef struct _Pen Pen;
typedef struct _Marker Marker;

typedef Pen *(PenCreateProc)(void);
typedef int (PenConfigureProc)(Graph *graphPtr, Pen *penPtr);
typedef void (PenDestroyProc)(Graph *graphPtr, Pen *penPtr);

struct _Pen {
    const char *name;		/* Pen style identifier.  If NULL pen was
				 * statically allocated. */
    ClassId classId;		/* Type element using this pen */
    const char *typeId;		/* String token identifying the type of pen */
    unsigned int flags;		/* Indicates if the pen element is active or
				 * normal */
    int refCount;		/* Reference count for elements using this
				 * pen. */
    Blt_HashEntry *hashPtr;

    Blt_ConfigSpec *configSpecs; /* Configuration specifications */

    PenConfigureProc *configProc;
    PenDestroyProc *destroyProc;
    Graph *graphPtr;		/* Graph that the pen is associated with. */
};

/*
 *---------------------------------------------------------------------------
 *
 * Crosshairs
 *
 *	Contains the line segments positions and graphics context used to
 *	simulate crosshairs (by XOR-ing) on the graph.
 *
 *---------------------------------------------------------------------------
 */
typedef struct _Crosshairs Crosshairs;

typedef struct {
    short int width, height;	/* Extents of the margin */

    short int axesOffset;
    short int axesTitleLength;	/* Width of the widest title to be shown.
				 * Multiple titles are displayed in another
				 * margin. This is the minimum space
				 * requirement. */
    unsigned int nAxes;		/* Number of axes to be displayed */
    Blt_Chain axes;		/* Extra axes associated with this margin */

    const char *varName;	/* If non-NULL, name of variable to be updated
				 * when the margin size changes */

    int reqSize;		/* Requested size of margin */
    int site;			/* Indicates where margin is located:
				 * left/right/top/bottom. */
} Margin;

#define MARGIN_NONE	-1
#define MARGIN_BOTTOM	0	
#define MARGIN_LEFT	1 
#define MARGIN_TOP	2			
#define MARGIN_RIGHT	3

#define rightMargin	margins[MARGIN_RIGHT]
#define leftMargin	margins[MARGIN_LEFT]
#define topMargin	margins[MARGIN_TOP]
#define bottomMargin	margins[MARGIN_BOTTOM]

/*
 *---------------------------------------------------------------------------
 *
 * Graph --
 *
 *	Top level structure containing everything pertaining to the graph.
 *
 *---------------------------------------------------------------------------
 */
struct _Graph {
    unsigned int flags;		/* Flags;  see below for definitions. */
    Tcl_Interp *interp;		/* Interpreter associated with graph */
    Tk_Window tkwin;		/* Window that embodies the graph.  NULL means
				 * that the window has been destroyed but the
				 * data structures haven't yet been cleaned
				 * up. */
    Display *display;		/* Display containing widget; needed, among
				 * other things, to release resources after
				 * tkwin has already gone away. */
    Tcl_Command cmdToken;	/* Token for graph's widget command. */

    const char *data;		/* This value isn't used in C code.  It may be
				 * used in TCL bindings to associate extra
				 * data. */

    Tk_Cursor cursor;

    int inset;			/* Sum of focus highlight and 3-D border.
				 * Indicates how far to offset the graph from
				 * outside edge of the window. */

    int borderWidth;		/* Width of the exterior border */
    int relief;			/* Relief of the exterior border */
    Blt_Background normalBg;	/* 3-D border used to delineate the plot
				 * surface and outer edge of window */
    
    int highlightWidth;		/* Width in pixels of highlight to draw around
				 * widget when it has the focus.  <= 0 means
				 * don't draw a highlight. */
    XColor *highlightBgColor;	/* Color for drawing traversal highlight area
				 * when highlight is off. */
    XColor *highlightColor;	/* Color for drawing traversal highlight. */

    const char *title;
    short int titleX, titleY;
    short int titleWidth, titleHeight;
    TextStyle titleTextStyle;	/* Graph title */
    
    const char *takeFocus;
    Axis *focusPtr;		/* The axis that currently has focus */
    
    int reqWidth, reqHeight;	/* Requested size of graph window */
    int width, height;		/* Size of graph window or PostScript page */
    
    Blt_HashTable penTable;	/* Table of pens */
    
    struct Component {
	Blt_HashTable table;	/* Hash table of ids. */
	Blt_Chain displayList;	/* Display list. */
	Blt_HashTable tagTable;	/* Table of bind tags. */
    } elements, markers, axes;

    Blt_HashTable dataTables;	/* Hash table of datatable clients. */
    
    ClassId classId;		/* Default element type */

    Blt_BindTable bindTable;
    int nextMarkerId;		/* Tracks next marker identifier available */
    
    Blt_Chain axisChain[4];	/* Chain of axes for each of the margins.
				 * They're separate from the margin structures
				 * to make it easier to invert the X-Y axes by
				 * simply switching chain pointers. */
    Margin margins[4];
    
    PageSetup *pageSetup;	/* Page layout options: see bltGrPS.c */
    Legend *legend;		/* Legend information: see bltGrLegd.c */
    Crosshairs *crosshairs;	/* Crosshairs information: see bltGrHairs.c */

    int halo;			/* Maximum distance allowed between points
				 * when searching for a point */
    int inverted;		/* If non-zero, indicates the x and y axis
				 * positions should be inverted. */
    int stackAxes;		/* If non-zero, indicates to stack mulitple
				 * axes in a margin, rather than layering them
				 * one on top of another. */

    GC drawGC;			/* GC for drawing on the margins. This
				 * includes the axis lines */

    int plotBorderWidth;	/* Width of interior 3-D border. */
    int plotRelief;		/* 3-d effect: TK_RELIEF_RAISED etc. */
    Blt_Background plotBg;	/* Color of plotting surface */

    /* If non-zero, force plot to conform to aspect ratio W/H */
    float aspect;

    short int left, right;	/* Coordinates of plot bbox */
    short int top, bottom;	

    Blt_Pad xPad;		/* Vertical padding for plotarea */
    int vRange, vOffset;	/* Vertical axis range and offset from the
				 * left side of the graph window. Used to
				 * transform coordinates to vertical axes. */
    Blt_Pad yPad;		/* Horizontal padding for plotarea */
    int hRange, hOffset;	/* Horizontal axis range and offset from the
				 * top of the graph window. Used to transform
				 * horizontal axes */
    float vScale, hScale;

    int doubleBuffer;		/* If non-zero, draw the graph into a pixmap
				 * first to reduce flashing. */
    int backingStore;		/* If non-zero, cache elements by drawing them
				 * into a pixmap */
    Pixmap backPixmap;		/* Pixmap used to cache elements displayed.
				 * If *backingStore* is non-zero, each element
				 * is drawn into this pixmap before it is
				 * copied onto the screen.  The pixmap then
				 * acts as a cache (only the pixmap is
				 * redisplayed if the none of elements have
				 * changed). This is done so that markers can
				 * be redrawn quickly over elements without
				 * redrawing each element. */
    int backWidth, backHeight;	/* Size of element backing store pixmap. */

    /*
     * barchart specific information
     */
    float baseline;		/* Baseline from bar chart.  */
    float barWidth;		/* Default width of each bar in graph units.
				 * The default width is 1.0 units. */
    BarMode mode;		/* Mode describing how to display bars with
				 * the same x-coordinates. Mode can be
				 * "stack", "align", or "normal" */
    FreqInfo *freqArr;		/* Contains information about duplicate
				 * x-values in bar elements (malloc-ed).  This
				 * information can also be accessed by the
				 * frequency hash table */
    Blt_HashTable freqTable;	/* */
    int nStacks;		/* Number of entries in frequency array.  If
				 * zero, indicates nothing special needs to be
				 * done for "stack" or "align" modes */
    const char *dataCmd;	/* New data callback? */

};

/*
 * Bit flags definitions:
 *
 * 	All kinds of state information kept here.  All these things happen
 * 	when the window is available to draw into (DisplayGraph). Need the
 * 	window width and height before we can calculate graph layout (i.e. the
 * 	screen coordinates of the axes, elements, titles, etc). But we want to
 * 	do this only when we have to, not every time the graph is redrawn.
 *
 *	Same goes for maintaining a pixmap to double buffer graph elements.
 *	Need to mark when the pixmap needs to updated.
 *
 *
 *	MAP_ITEM		Indicates that the element/marker/axis
 *				configuration has changed such that
 *				its layout of the item (i.e. its
 *				position in the graph window) needs
 *				to be recalculated.
 *
 *	MAP_ALL			Indicates that the layout of the axes and 
 *				all elements and markers and the graph need 
 *				to be recalculated. Otherwise, the layout
 *				of only those markers and elements that
 *				have changed will be reset. 
 *
 *	GET_AXIS_GEOMETRY	Indicates that the size of the axes needs 
 *				to be recalculated. 
 *
 *	RESET_AXES		Flag to call to Blt_ResetAxes routine.  
 *				This routine recalculates the scale offset
 *				(used for mapping coordinates) of each axis.
 *				If an axis limit has changed, then it sets 
 *				flags to re-layout and redraw the entire 
 *				graph.  This needs to happend before the axis
 *				can compute transformations between graph and 
 *				screen coordinates. 
 *
 *	LAYOUT_NEEDED		
 *
 *	REDRAW_BACKING_STORE	If set, redraw all elements into the pixmap 
 *				used for buffering elements. 
 *
 *	REDRAW_PENDING		Non-zero means a DoWhenIdle handler has 
 *				already been queued to redraw this window. 
 *
 *	DRAW_LEGEND		Non-zero means redraw the legend. If this is 
 *				the only DRAW_* flag, the legend display 
 *				routine is called instead of the graph 
 *				display routine. 
 *
 *	DRAW_MARGINS		Indicates that the margins bordering 
 *				the plotting area need to be redrawn. 
 *				The possible reasons are:
 *
 *				1) an axis configuration changed
 *				2) an axis limit changed
 *				3) titles have changed
 *				4) window was resized. 
 *
 *	GRAPH_FOCUS	
 */

#define HIDE			(1<<0) /* 0x0001 */
#define DELETE_PENDING		(1<<1) /* 0x0002 */
#define REDRAW_PENDING		(1<<2) /* 0x0004 */
#define	ACTIVE_PENDING		(1<<3) /* 0x0008 */
#define	MAP_ITEM		(1<<4) /* 0x0010 */
#define DIRTY			(1<<5) /* 0x0020 */
#define ACTIVE			(1<<6) /* 0x0040 */
#define FOCUS			(1<<7) /* 0x0080 */

#define	MAP_ALL			(1<<8) /* 0x0100 */
#define LAYOUT_NEEDED		(1<<9) /* 0x0200 */
#define RESET_AXES		(1<<10)/* 0x0400 */
#define	GET_AXIS_GEOMETRY	(1<<11)/* 0x0800 */

#define DRAW_LEGEND		(1<<12)/* 0x1000 */
#define DRAW_MARGINS		(1<<13)/* 0x2000 */
#define	REDRAW_BACKING_STORE	(1<<14)/* 0x4000 */

#define	MAP_WORLD		(MAP_ALL|RESET_AXES|GET_AXIS_GEOMETRY)
#define REDRAW_WORLD		(DRAW_LEGEND)
#define RESET_WORLD		(REDRAW_WORLD | MAP_WORLD)


/*
 * ---------------------- Forward declarations ------------------------
 */

BLT_EXTERN int Blt_Graph_CreatePageSetup(Graph *graphPtr);

BLT_EXTERN int Blt_Graph_CreateCrosshairs (Graph *graphPtr);

BLT_EXTERN double Blt_InvHMap(Axis *axisPtr, double x);

BLT_EXTERN double Blt_InvVMap(Axis *axisPtr, double x);

BLT_EXTERN double Blt_HMap(Axis *axisPtr, double x);

BLT_EXTERN double Blt_VMap(Axis *axisPtr, double y);

BLT_EXTERN Point2d Blt_InvMap2D (Graph *graphPtr, double x, double y, 
	Axis2d *pairPtr);

BLT_EXTERN Point2d Blt_Map2D (Graph *graphPtr, double x, double y, 
	Axis2d *pairPtr);

BLT_EXTERN Graph *Blt_GetGraphFromWindowData (Tk_Window tkwin);

BLT_EXTERN void Blt_AdjustAxisPointers (Graph *graphPtr);

BLT_EXTERN int Blt_PolyRectClip (Region2d *extsPtr, Point2d *inputPts,
	int nInputPts, Point2d *outputPts);

BLT_EXTERN void Blt_ComputeStacks (Graph *graphPtr);

BLT_EXTERN void Blt_Graph_ConfigureCrosshairs (Graph *graphPtr);

BLT_EXTERN void Blt_DestroyAxes (Graph *graphPtr);

BLT_EXTERN void Blt_Graph_DestroyCrosshairs (Graph *graphPtr);

BLT_EXTERN void Blt_DestroyElements (Graph *graphPtr);

BLT_EXTERN void Blt_DestroyMarkers (Graph *graphPtr);

BLT_EXTERN void Blt_DestroyPageSetup(Graph *graphPtr);

BLT_EXTERN void Blt_DrawAxes (Graph *graphPtr, Drawable drawable);

BLT_EXTERN void Blt_DrawAxisLimits (Graph *graphPtr, Drawable drawable);

BLT_EXTERN void Blt_DrawElements (Graph *graphPtr, Drawable drawable);

BLT_EXTERN void Blt_DrawActiveElements (Graph *graphPtr, Drawable drawable);

BLT_EXTERN void Blt_DrawGraph(Graph *graphPtr, Drawable drawable, int store);

BLT_EXTERN void Blt_DrawMarkers (Graph *graphPtr, Drawable drawable, int under);

BLT_EXTERN void Blt_Draw2DSegments (Display *display, Drawable drawable, GC gc, 
	Segment2d *segments, int nSegments);

BLT_EXTERN int Blt_GetCoordinate (Tcl_Interp *interp, const char *string, 
	double *valuePtr);

BLT_EXTERN void Blt_InitFreqTable (Graph *graphPtr);

BLT_EXTERN void Blt_LayoutGraph (Graph *graphPtr);

BLT_EXTERN void Blt_LayoutMargins (Graph *graphPtr);

BLT_EXTERN void Blt_EventuallyRedrawGraph (Graph *graphPtr);

BLT_EXTERN void Blt_EventuallyRedrawLegend (Legend *legendPtr);

BLT_EXTERN void Blt_ResetAxes (Graph *graphPtr);

BLT_EXTERN void Blt_ResetStacks (Graph *graphPtr);

BLT_EXTERN void Blt_GraphExtents (Graph *graphPtr, Region2d *extsPtr);

BLT_EXTERN void Blt_Graph_DisableCrosshairs (Graph *graphPtr);

BLT_EXTERN void Blt_Graph_EnableCrosshairs (Graph *graphPtr);

BLT_EXTERN void Blt_MapAxes (Graph *graphPtr);

BLT_EXTERN void Blt_MapElements (Graph *graphPtr);

BLT_EXTERN void Blt_MapGraph (Graph *graphPtr);

BLT_EXTERN void Blt_MapMarkers (Graph *graphPtr);

BLT_EXTERN void Blt_Graph_UpdateCrosshairs (Graph *graphPtr);

BLT_EXTERN void Blt_DestroyPens (Graph *graphPtr);

BLT_EXTERN int Blt_GetPenFromObj (Tcl_Interp *interp, Graph *graphPtr, 
	Tcl_Obj *objPtr, ClassId classId, Pen **penPtrPtr);

BLT_EXTERN Pen *Blt_BarPen(const char *penName);

BLT_EXTERN Pen *Blt_LinePen(const char *penName);

BLT_EXTERN Pen *Blt_CreatePen (Graph *graphPtr, const char *penName, 
	ClassId classId, int objc, Tcl_Obj *const *objv);

BLT_EXTERN int Blt_InitLinePens (Graph *graphPtr);

BLT_EXTERN int Blt_InitBarPens (Graph *graphPtr);

BLT_EXTERN void Blt_FreePen (Pen *penPtr);

BLT_EXTERN int Blt_VirtualAxisOp (Graph *graphPtr, Tcl_Interp *interp, 
	int objc, Tcl_Obj *const *objv);

BLT_EXTERN int Blt_AxisOp (Tcl_Interp *interp, Graph *graphPtr, int margin, 
	int objc, Tcl_Obj *const *objv);

BLT_EXTERN int Blt_ElementOp (Graph *graphPtr, Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv, ClassId classId);

BLT_EXTERN int Blt_CrosshairsOp (Graph *graphPtr, Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv);

BLT_EXTERN int Blt_MarkerOp (Graph *graphPtr, Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv);

BLT_EXTERN int Blt_PenOp (Graph *graphPtr, Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv);

BLT_EXTERN int Blt_PointInPolygon (Point2d *samplePtr, Point2d *screenPts, 
	int nScreenPts);

BLT_EXTERN int Blt_RegionInPolygon (Region2d *extsPtr, Point2d *points, 
	int nPoints, int enclosed);

BLT_EXTERN int Blt_PointInSegments (Point2d *samplePtr, Segment2d *segments, 
	int nSegments, double halo);

BLT_EXTERN int Blt_PostScriptOp (Graph *graphPtr, Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv);

BLT_EXTERN int Blt_GraphUpdateNeeded (Graph *graphPtr);

BLT_EXTERN int Blt_DefaultAxes (Graph *graphPtr);

BLT_EXTERN Axis *Blt_GetFirstAxis (Blt_Chain chain);

BLT_EXTERN void Blt_UpdateAxisBackgrounds (Graph *graphPtr);

BLT_EXTERN Marker *Blt_NearestMarker (Graph *graphPtr, int x, int y, int under);

BLT_EXTERN Axis *Blt_NearestAxis (Graph *graphPtr, int x, int y);

typedef ClientData (MakeTagProc) (Graph *graphPtr, const char *tagName);

BLT_EXTERN MakeTagProc Blt_MakeElementTag;
BLT_EXTERN MakeTagProc Blt_MakeMarkerTag;
BLT_EXTERN MakeTagProc Blt_MakeAxisTag;
BLT_EXTERN Blt_BindTagProc Blt_GraphTags;
BLT_EXTERN Blt_BindTagProc Blt_AxisTags;

BLT_EXTERN int Blt_GraphType(Graph *graphPtr);

BLT_EXTERN void Blt_UpdateGraph(ClientData clientData);

BLT_EXTERN void Blt_GraphSetObjectClass(GraphObj *graphObjPtr,ClassId classId);

BLT_EXTERN void Blt_MarkersToPostScript(Graph *graphPtr, Blt_Ps ps, int under);
BLT_EXTERN void Blt_ElementsToPostScript(Graph *graphPtr, Blt_Ps ps);
BLT_EXTERN void Blt_ActiveElementsToPostScript(Graph *graphPtr, Blt_Ps ps);
BLT_EXTERN void Blt_LegendToPostScript(Legend *legendPtr, Blt_Ps ps);
BLT_EXTERN void Blt_AxesToPostScript(Graph *graphPtr, Blt_Ps ps);
BLT_EXTERN void Blt_AxisLimitsToPostScript(Graph *graphPtr, Blt_Ps ps);

BLT_EXTERN Element *Blt_LineElement(Graph *graphPtr, const char *name, 
	ClassId classId);
BLT_EXTERN Element *Blt_BarElement(Graph *graphPtr, const char *name, 
	ClassId classId);

BLT_EXTERN void Blt_DrawGrids(Graph *graphPtr, Drawable drawable);

BLT_EXTERN void Blt_GridsToPostScript(Graph *graphPtr, Blt_Ps ps);


/* ---------------------- Global declarations ------------------------ */

BLT_EXTERN const char *Blt_GraphClassName(ClassId classId);

#endif /* _BLT_GRAPH_H */
