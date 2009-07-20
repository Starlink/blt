
/*
 * ----------------------------------------------------------------------
 *
 * Blt_Pad --
 *
 * 	Specifies vertical and horizontal padding.
 *
 *	Padding can be specified on a per side basis.  The fields
 *	side1 and side2 refer to the opposite sides, either
 *	horizontally or vertically.
 *
 *		side1	side2
 *              -----   -----
 *          x | left    right
 *	    y | top     bottom
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    unsigned short int side1, side2;
} Blt_Pad;

#define padLeft  	xPad.side1
#define padRight  	xPad.side2
#define padTop		yPad.side1
#define padBottom	yPad.side2
#define PADDING(x)	((x).side1 + (x).side2)

/*
 * ----------------------------------------------------------------------
 *
 * The following enumerated values are used as bit flags.
 *	FILL_NONE		Neither coordinate plane is specified 
 *	FILL_X			Horizontal plane.
 *	FILL_Y			Vertical plane.
 *	FILL_BOTH		Both vertical and horizontal planes.
 *
 * ----------------------------------------------------------------------
 */
#define FILL_NONE	0
#define FILL_X		1
#define FILL_Y		2
#define FILL_BOTH	3

/*
 * ----------------------------------------------------------------------
 *
 * Blt_Dashes --
 *
 * 	List of dash values (maximum 11 based upon PostScript limit).
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    unsigned char values[12];
    int offset;
} Blt_Dashes;

#define LineIsDashed(d) ((d).values[0] != 0)

/*
 * -------------------------------------------------------------------
 *
 * Point2D --
 *
 *	2-D coordinate.
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    double x, y;
} Point2D;

/*
 * -------------------------------------------------------------------
 *
 * Point3D --
 *
 *	3-D coordinate.
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    double x, y, z;
} Point3D;

/*
 * -------------------------------------------------------------------
 *
 * Segment2D --
 *
 *	2-D line segment.
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    Point2D p, q;		/* The two end points of the segment. */
} Segment2D;

/*
 * -------------------------------------------------------------------
 *
 * Dim2D --
 *
 *	2-D dimension.
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    short int width, height;
} Dim2D;

/*
 *----------------------------------------------------------------------
 *
 * Region2D --
 *
 *      2-D region.  Used to copy parts of images.
 *
 *----------------------------------------------------------------------
 */
typedef struct {
    double left, right, top, bottom;
} Region2D;

typedef struct {
    double left, right, top, bottom, front, back;
} Region3D;

#define RegionWidth(r)		((r)->right - (r)->left + 1)
#define RegionHeight(r)		((r)->bottom - (r)->top + 1)

#define PointInRegion(e,x,y) \
	(((x) <= (e)->right) && ((x) >= (e)->left) && \
	 ((y) <= (e)->bottom) && ((y) >= (e)->top))

#define PointInRectangle(r,x0,y0) \
	(((x0) <= (int)((r)->x + (r)->width - 1)) && ((x0) >= (int)(r)->x) && \
	 ((y0) <= (int)((r)->y + (r)->height - 1)) && ((y0) >= (int)(r)->y))

