
typedef unsigned int PointIndex;

typedef struct {
    float x, y;
    int index;
} MeshPoint;

typedef struct {
    PointIndex a, b;		/* Index of two points in x, y, point array */
} Edge;

typedef struct {
    PointIndex ab, bc, ac;	/* Index of three edges representing a
				 * triangle in the edge array. */
    float min, max;
} Triangle;

typedef Mesh *(MeshCreateProc)(void);
typedef void (MeshDestroyProc)(Mesh *meshPtr);
typedef void (MeshConfigureProc)(Mesh *meshPtr);

typedef enum MeshTypes {
    MESH_CLOUD, MESH_REGULAR, MESH_IRREGULAR, MESH_TRIANGULAR
} MeshType;

typedef struct {
    enum MeshTypes type;	/* Indicates type of mesh. */
    const char *name;		/* String representation for type of class. */
    Blt_ConfigSpec *configSpecs; /* Mesh configuration specifications. */
    MeshDestroyProc *destroyProc;
    MeshConfigureProc *configProc;
} MeshClass;

typedef enum SourceTypes {
    SOURCE_LIST, SOURCE_VECTOR, SOURCE_TABLE, SOURCE_NONE
} SourceType;

typedef struct {
    enum SourceTypes type;	/* Selects the type of data populating this
				 * vector: SOURCE_VECTOR,
				 * SOURCE_TABLE, or SOURCE_LIST */
    SourceGetProc *getProc;
    SourceDestroyProc *destroyProc;
    SourcePrintProc *printProc;
} DataSourceClass;

typedef struct {
    float min, max, logMin;
    float *values;
    float nValues;
} DataSourceResult;

typedef struct {
    DataSourceClass *classPtr;
    DataSourceChangedProc *proc;
    ClientData clientData;
} DataSource;

typedef int (SourceGetProc)(Tcl_Interp *interp, DataSource *srcPtr, 
	DataSourceResult *resultPtr);
typedef int (SourceDestroyProc)(DataSource *srcPtr);
typedef Tcl_Obj * (SourcePrintProc)(DataSource *srcPtr);

typedef struct {
    DataSourceClass *classPtr;
    DataSourceChangedProc *proc;
    ClientData clientData;
    Blt_VectorId vector;
} VectorDataSource;

typedef struct {
    DataSourceClass *classPtr;
    DataSourceChangedProc *proc;
    ClientData clientData;
    float *values;
    int nValues;
} ListDataSource;

typedef struct {
    Blt_Table table;
    int refCount;
} TableClient;

typedef struct {
    DataSourceClass *classPtr;
    DataSourceChangedProc *proc;
    ClientData clientData;

    Blt_Table table;	/* Data table. */ 
    Blt_TableColumn column;	/* Column of data used. */
    Blt_TableNotifier notifier; /* Notifier used for column (destroy). */
    Blt_TableTrace trace;	/* Trace used for column (set/get/unset). */
    Blt_HashEntry *hashPtr;	/* Pointer to entry of source in graph's hash
				 * table of datatables. One graph may use
				 * multiple columns from the same data
				 * table. */
} TableDataSource;

typedef struct {
    const char *name;		/* Mesh identifier. */
    MeshClass *classPtr;
    Graph *graphPtr;		/* Graph that the mesh is associated with. */
    unsigned int flags;		/* Indicates if the mesh element is active or
				 * normal */
    int refCount;		/* Reference count for elements using this
				 * mesh. */
    Blt_HashEntry *hashPtr;

    /* Resulting mesh is a triangular grid.  */
    MeshPoint *points;		/* Array of points representing the mesh. */
    int nPoints;		/* # of points in array. */
    int *hull;			/* Array of indices of points representing the
				 * convex hull of the mesh. */
    int nHullPts;
    float xMin, yMin, xMax, yMax, xLogMin, yLogMin;

    Edges *edges;		/* Array of edges. */
    int nEdges;			/* # of edges in array. */
    Triangles *triangles;	/* Array of triangles. */
    int nTriangles;		/* # of triangles in array. */
    DataSource *x, *y;
} Mesh;

typedef struct {
    const char *name;		/* Mesh identifier. */
    MeshClass *classPtr;
    Graph *graphPtr;		/* Graph that the mesh is associated with. */

    unsigned int flags;		/* Indicates if the mesh element is active or
				 * normal */
    int refCount;		/* Reference count for elements using this
				 * mesh. */
    Blt_HashEntry *hashPtr;

    /* Resulting mesh is a triangular grid.  */
    MeshPoint *points;		/* Array of points representing the mesh. */
    int nPoints;		/* # of points in array. */
    int *hull;			/* Array of indices of points representing the
				 * convex hull of the mesh. */
    int nHullPts;
    float xMin, yMin, xMax, yMax, xLogMin, yLogMin;

    Edges *edges;		/* Array of edges. */
    int nEdges;			/* # of edges in array. */
    Triangles *triangles;	/* Array of triangles. */
    int nTriangles;		/* # of triangles in array. */

    DataSource *x, *y;
} RegularMesh;

typedef struct {
    const char *name;		/* Mesh identifier. */
    MeshClass *classPtr;
    Graph *graphPtr;		/* Graph that the mesh is associated with. */

    unsigned int flags;		/* Indicates if the mesh element is active or
				 * normal */
    int refCount;		/* Reference count for elements using this
				 * mesh. */
    Blt_HashEntry *hashPtr;

    /* Resulting mesh is a triangular grid.  */
    MeshPoint *points;		/* Array of points representing the mesh. */
    int nPoints;		/* # of points in array. */
    int *hull;			/* Array of indices of points representing the
				 * convex hull of the mesh. */
    int nHullPts;
    float xMin, yMin, xMax, yMax, xLogMin, yLogMin;

    Edges *edges;		/* Array of edges. */
    int nEdges;			/* # of edges in array. */
    Triangles *triangles;	/* Array of triangles. */
    int nTriangles;		/* # of triangles in array. */

    DataSource *x, *y;
} IrregularMesh;

typedef struct {
    const char *name;		/* Mesh identifier. */
    MeshClass *classPtr;
    Graph *graphPtr;		/* Graph that the mesh is associated with. */

    unsigned int flags;		/* Indicates if the mesh element is active or
				 * normal */
    int refCount;		/* Reference count for elements using this
				 * mesh. */
    Blt_HashEntry *hashPtr;

    /* Resulting mesh is a triangular grid.  */
    MeshPoint *points;		/* Array of points representing the mesh. */
    int nPoints;		/* # of points in array. */
    int *hull;			/* Array of indices of points representing the
				 * convex hull of the mesh. */
    int nHullPts;
    float xMin, yMin, xMax, yMax, xLogMin, yLogMin;

    Edges *edges;		/* Array of edges. */
    int nEdges;			/* # of edges in array. */
    Triangles *triangles;	/* Array of triangles. */
    int nTriangles;		/* # of triangles in array. */

    DataSource *x, *y;
} CloudMesh;

static MeshConfigureProc CloudMeshConfigureProc;
static MeshDestroyProc   CloudMeshDestroyProc;

static MeshConfigureProc IrregularMeshConfigureProc;
static MeshDestroyProc   IrregularMeshDestroyProc;

static MeshConfigureProc RegularMeshConfigureProc;
static MeshDestroyProc   RegularMeshDestroyProc;

static MeshConfigureProc TriangularMeshConfigureProc;
static MeshDestroyProc   TriangularMeshDestroyProc;

static MeshClass cloudMeshClass = {
    MESH_CLOUD, "Cloud",
    CloudMeshConfigureProc,
    CloudMeshDestroyProc,
};

static MeshClass regularMeshClass = {
    MESH_REGULAR, "Regular",
    RegularMeshConfigureProc,
    RegularMeshDestroyProc,
};

static MeshClass triangularMeshClass = {
    MESH_TRIANGULAR, "Triangular",
    TriangularMeshConfigureProc,
    TriangularMeshDestroyProc,
};

static MeshClass irregularMeshClass = {
    MESH_IRREGULAR, "Irregular",
    IrregularMeshConfigureProc,
    IrregularMeshDestroyProc,
};

static SourceGetProc     ListDataSourceGetProc;
static SourcePrintProc   ListDataSourcePrintProc;
static SourceDestroyProc ListDataSourceDestroyProc;

static SourceGetProc     TableDataSourceGetProc;
static SourcePrintProc   TableDataSourcePrintProc;
static SourceDestroyProc TableDataSourceDestroyProc;

static SourceGetProc     VectorDataSourceGetProc;
static SourcePrintProc   VectorDataSourcePrintProc;
static SourceDestroyProc VectorDataSourceDestroyProc;

static DataSourceClass listDataSourceClass = {
    SOURCE_LIST, "List",
    ListDataSourceGetProc,
    ListDataSourceDestroyProc,
    ListDataSourcePrintProc
};

static DataSourceClass vectorDataSourceClass = {
    SOURCE_VECTOR, "Vector",
    VectorDataSourceGetProc,
    VectorDataSourceDestroyProc,
    VectorDataSourcePrintProc
};

static DataSourceClass tableDataSourceClass = {
    SOURCE_TABLE, "Table",
    TableDataSourceGetProc,
    TableDataSourceDestroyProc,
    TableDataSourcePrintProc
};

static Blt_OptionFreeProc FreeDataSource;
static Blt_OptionParseProc ObjToDataSource;
static Blt_OptionPrintProc DataSourceToObj;
Blt_CustomOption bltDataSourceOption =
{
    ObjToDataSourceProc, DataSourceToObjProc, FreeDataSourceProc, (ClientData)0
};

static Blt_ConfigSpec cloudMeshSpecs[] =
{
    {BLT_CONFIG_STRING, "-name", "name", "name", (char *)NULL, 
        Blt_Offset(Cloud, name), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-x", "xData", "XData", (char *)NULL, 
        Blt_Offset(Cloud, x), 0, &bltDataSourceOption},
    {BLT_CONFIG_CUSTOM, "-y", "yData", "YData", (char *)NULL, 
        Blt_Offset(Cloud, y), 0, &bltDataSourceOption},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static Blt_ConfigSpec regularMeshSpecs[] =
{
    {BLT_CONFIG_STRING, "-name", "name", "name", (char *)NULL, 
        Blt_Offset(Cloud, name), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-x", "xData", "XData", (char *)NULL, 
        Blt_Offset(Cloud, x), 0, &bltDataSourceOption},
    {BLT_CONFIG_CUSTOM, "-y", "yData", "YData", (char *)NULL, 
        Blt_Offset(Cloud, y), 0, &bltDataSourceOption},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static Blt_ConfigSpec irregularMeshSpecs[] =
{
    {BLT_CONFIG_STRING, "-name", "name", "name", (char *)NULL, 
        Blt_Offset(Cloud, name), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-x", "xData", "XData", (char *)NULL, 
        Blt_Offset(Cloud, x), 0, &bltDataSourceOption},
    {BLT_CONFIG_CUSTOM, "-y", "yData", "YData", (char *)NULL, 
        Blt_Offset(Cloud, y), 0, &bltDataSourceOption},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static Blt_ConfigSpec triangularMeshSpecs[] =
{
    {BLT_CONFIG_STRING, "-name", "name", "name", (char *)NULL, 
        Blt_Offset(Cloud, name), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-x", "xData", "XData", (char *)NULL, 
        Blt_Offset(Cloud, x), 0, &bltDataSourceOption},
    {BLT_CONFIG_CUSTOM, "-y", "yData", "YData", (char *)NULL, 
        Blt_Offset(Cloud, y), 0, &bltDataSourceOption},
    {BLT_CONFIG_CUSTOM, "-triangles", "triangles", "Triangles", (char *)NULL, 
        Blt_Offset(Cloud, triangles), 0, &trianglesOption},
    {BLT_CONFIG_CUSTOM, "-edges", "edges", "Edges", (char *)NULL, 
        Blt_Offset(Cloud, edges), 0, &edgesOption},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static Tcl_Obj *
VectorDataSourcePrintProc(DataSource *dataSrcPtr)
{
    VectorDataSource *srcPtr = (VectorDataSource *)dataSrcPtr;
	    
    return Tcl_NewStringObj(Blt_NameOfVectorId(srcPtr->vector), -1);
}

static void
VectorDataSourceDestroyProc(DataSource *dataSrcPtr)
{
    VectorDataSource *srcPtr = (VectorDataSource *)dataSrcPtr;

    Blt_SetVectorChangedProc(srcPtr->vector, NULL, NULL);
    if (srcPtr->vector != NULL) { 
	Blt_FreeVectorId(srcPtr->vector); 
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * VectorChangedProc --
 *
 * Results:
 *     	None.
 *
 * Side Effects:
 *	Graph is redrawn.
 *
 *---------------------------------------------------------------------------
 */
static void
VectorChangedProc(Tcl_Interp *interp, ClientData clientData, 
		  Blt_VectorNotify notify)
{
    VectorDataSource *srcPtr = clientData;

    if (notify == BLT_VECTOR_NOTIFY_DESTROY) {
	DestroyDataSource(srcPtr);
	return;
    } 
    (*srcPtr->proc)(srcPtr->clientData);
}

static DataSource *
CreateVectorDataSource(Tcl_Interp *interp, const char *name)
{
    Blt_Vector *vecPtr;
    VectorDataSource *srcPtr;
    
    srcPtr = Blt_AssertCalloc(sizeof(VectorDataSource));
    srcPtr->type = SOURCE_VECTOR;
    srcPtr->vector = Blt_AllocVectorId(interp, name);
    if (Blt_GetVectorById(interp, srcPtr->vector, &vecPtr) != TCL_OK) {
	Blt_Free(srcPtr);
	return NULL;
    }
    Blt_SetVectorChangedProc(srcPtr->vector, VectorChangedProc, srcPtr);
    return (DataSource *)srcPtr;
}

static int
VectorDataSourceGetProc(Tcl_Interp *interp, DataSource *dataSrcPtr, 
			DataSourceResult *resultPtr)
{
    VectorDataSource *srcPtr = (VectorDataSource *)dataSrcPtr;
    size_t nBytes;
    Blt_Vector *vector;

    if (Blt_GetVectorById(interp, srcPtr->vector, &vector) != TCL_OK) {
	return TCL_ERROR;
    }
    nBytes = Blt_VecLength(vector) * sizeof(float);
    values = Blt_Malloc(nBytes);
    if (values == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't allocate new vector", (char *)NULL);
	}
	return TCL_ERROR;
    }
    {
	float *values;
	float min, max, logMin;
	double *data;
	double *p; 
	int i;

	p = Blt_VecData(vector);
	logMin = min = max = (float)*p++;
	for (i = 0; i < Blt_VecLength(vector); i++, p++) {
	    values[i] = (float)*p;
	    if (*p > max) {
		max = (float)*p;
	    } else if (*p < min) {
		min = (float)*p;
	    }
	    if ((*p > 0.0f) && (*p < logMin)) {
		logMin = (float)*p;
	    }
	}
	resultPtr->min = min;
	resultPtr->max = max;
	resultPtr->logMin = logMin;
    }
    resultPtr->values = values;
    resultPtr->nValues = Blt_VecLength(vector);
    return TCL_OK;
}

static Tcl_Obj *
ListDataSourcePrintProc(DataSource *dataSrcPtr)
{
    ListDataSource *srcPtr = (ListDataSource *)dataSrcPtr;
    Tcl_Obj *listObjPtr;
    float *vp, *vend; 
    
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (vp = srcPtr->values, vend = vp + srcPtr->nValues; vp < vend; vp++) {
	Tcl_ListObjAppendElement(interp, listObjPtr, 
		Tcl_NewDoubleObj((double)*vp));
    }
    return listObjPtr;
}

static void
ListDataSourceDestroyProc(DataSource *dataSrcPtr)
{
    ListDataSource *srcPtr = (ListDataSource *)dataSrcPtr;

    if (srcPtr->values != NULL) {
	Blt_Free(srcPtr->values);
    }
    (*srcPtr->proc)(srcPtr, srcPtr->clientData);
}

static DataSource *
CreateListDataSource(Tcl_Interp *interp, int objc, Tcl_Obj **objv)
{
    float *values;
    int nValues;
    ListDataSource *srcPtr;

    srcPtr = Blt_AssertMalloc(sizeof(ListDataSource));
    srcPtr->type = SOURCE_LIST;

    nValues = 0;
    values = NULL;

    if (objc > 0) {
	float *p;
	int i;

	values = Blt_Malloc(sizeof(float) * objc);
	if (values == NULL) {
	    Tcl_AppendResult(interp, "can't allocate new vector", (char *)NULL);
	    return NULL;
	}
	for (p = values, i = 0; i < objc; i++, p++) {
	    double value;

	    if (Blt_ExprDoubleFromObj(interp, objv[i], &value) != TCL_OK) {
		Blt_Free(array);
		return TCL_ERROR;
	    }
	    *p = (float)value;
	}
	srcPtr->values = values;
	srcPtr->nValues = objc;
    }
    return (DataSource *)srcPtr;
}

static int
ListDataSourceGetProc(Tcl_Interp *interp, SourceData *dataSrcPtr, float *minPtr,
		      DataSourceResult *resultPtr)
{
    long i, j;
    float *values;
    float min, max, logMin;
    ListDataSource *srcPtr = (ListDataSource *)dataSrcPtr;

    values = Blt_Malloc(sizeof(float) * srcPtr->nValues);
    if (values == NULL) {
	return TCL_ERROR;
    }
    sp = srcPtr->values;
    logMin = min = max = values[0] = *sp++;
    for (send = srcPtr->values + srcPtr->nValues; sp < send; sp++) {
	if (*sp > max) {
	    max = *sp;
	} else if (*sp < min) {
	    min = *sp;
	}
	if ((*sp > 0.0f) && (*sp < logMin)) {
	    logMin = *sp;
	}
	values[j] = *sp;
    }
    resultPtr->min = min;
    resultPtr->max = max;
    resultPtr->logMin = logMin;
    resultPtr->values = values;
    resultPtr->nValues = srcPtr->nValues;
    return TCL_OK;
}


static void
TableDataSourceDestroyProc(DataSource *dataSrcPtr)
{
    TableDataSource *srcPtr = (TableDataSource *)dataSrcPtr;

    if (srcPtr->trace != NULL) {
	Blt_Table_DeleteTrace(srcPtr->trace);
    }
    if (srcPtr->notifier != NULL) {
	Blt_Table_DeleteNotifier(srcPtr->notifier);
    }
    if (srcPtr->hashPtr != NULL) {
	TableClient *clientPtr;

	clientPtr = Blt_GetHashValue(srcPtr->hashPtr);
	clientPtr->refCount--;
	if (clientPtr->refCount == 0) {
	    if (srcPtr->table != NULL) {
		Blt_Table_Close(srcPtr->table);
	    }
	    Blt_Free(clientPtr);
	    Blt_DeleteHashEntry(&graphPtr->dataTables, srcPtr->hashPtr);
	}
	(*srcPtr->proc)(srcPtr, srcPtr->clientData);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * TableNotifyProc --
 *
 *
 * Results:
 *     	None.
 *
 * Side Effects:
 *	Graph is redrawn.
 *
 *---------------------------------------------------------------------------
 */
static int
TableNotifyProc(ClientData clientData, Blt_TableNotifyEvent *eventPtr)
{
    TableDataSource *srcPtr = clientData;

    if (eventPtr->type == TABLE_NOTIFY_COLUMN_DELETED) {
	(*srcPtr->classPtr->destroyProc)(srcPtr);
	srcPtr->classPtr = NULL;
	return TCL_ERROR;
    } 
    (*srcPtr->proc)(srcPtr->clientData);
    return TCL_OK;
}
 
/*
 *---------------------------------------------------------------------------
 *
 * TableTraceProc --
 *
 *
 * Results:
 *     	None.
 *
 * Side Effects:
 *	Graph is redrawn.
 *
 *---------------------------------------------------------------------------
 */
static int
TableTraceProc(ClientData clientData, Blt_TableTraceEvent *eventPtr)
{
    TableDataSource *srcPtr = clientData;

    assert(eventPtr->column == srcPtr->column);
    (*srcPtr->proc)(srcPtr->clientData);
    return TCL_OK;
}


static DataSource *
CreateTableDataSource(Tcl_Interp *interp, const char *name, Tcl_Obj *colObjPtr)
{
    TableDataSource *srcPtr;
    TableClient *clientPtr;
    int isNew;

    srcPtr = Blt_AssertMalloc(sizeof(TableDataSource));
    srcPtr->type = SOURCE_TABLE;

    /* See if the graph is already using this table. */
    srcPtr->hashPtr = Blt_CreateHashEntry(&graphPtr->dataTables, name, &isNew);
    if (isNew) {
	if (Blt_Table_Open(interp, name, &srcPtr->table) != TCL_OK) {
	    return TCL_ERROR;
	}
	clientPtr = Blt_AssertMalloc(sizeof(TableClient));
	clientPtr->table = srcPtr->table;
	clientPtr->refCount = 1;
	Blt_SetHashValue(srcPtr->hashPtr, clientPtr);
    } else {
	clientPtr = Blt_GetHashValue(srcPtr->hashPtr);
	srcPtr->table = clientPtr->table;
	clientPtr->refCount++;
    }
    srcPtr->column = Blt_Table_FindColumn(interp, srcPtr->table, colObjPtr);
    if (srcPtr->column == NULL) {
	goto error;
    }
    srcPtr->notifier = Blt_Table_CreateColumnNotifier(interp, srcPtr->table,
	srcPtr->column, TABLE_NOTIFY_COLUMN_CHANGED, TableNotifyProc, 
	(Blt_TableNotifierDeleteProc *)NULL, srcPtr);
    srcPtr->trace = Blt_Table_CreateColumnTrace(srcPtr->table, 
	srcPtr->column, 
	(TABLE_TRACE_WRITES | TABLE_TRACE_UNSETS | TABLE_TRACE_CREATES), TableTraceProc,
	(Blt_TableTraceDeleteProc *)NULL, srcPtr);
    return (DataSource *)srcPtr;
 error:
    DestroyDataSource(srcPtr);
    return NULL;
}

static Tcl_Obj *
TableDataSourcePrintProc(DataSource *dataSrcPtr)
{
    TableDataSource *srcPtr = (TableDataSource *)dataSrcPtr;
    Tcl_Obj *listObjPtr;
    const char *name;
    long index;
    
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    name = Blt_Table_TableName(srcPtr->table);
    Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewStringObj(name, -1));
    
    index = Blt_Table_ColumnIndex(srcPtr->column);
    Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewLongObj(index));
    return listObjPtr;
}

static int
TableDataSourceGetProc(Tcl_Interp *interp, SourceData *dataSrcPtr, 
		       DataSourceResult *resultPtr)
{
    Blt_Table table;
    float *values;
    float min, max, logMin;
    long i, j;

    table = srcPtr->table;
    values = Blt_Malloc(sizeof(float) * Blt_Table_NumRows(table));
    if (values == NULL) {
	return TCL_ERROR;
    }
    logMin = min = FLT_MAX, max = -FLT_MAX;
    for (j = 0, i = 1; i <= Blt_Table_NumRows(table); i++) {
	Blt_TableRow row;
	Tcl_Obj *objPtr;
	double value;

	row = Blt_Table_GetRowByIndex(table, i);
	objPtr  = Blt_Table_GetObj(table, row, col);
	if (objPtr == NULL) {
	    continue;			/* Ignore empty values. */
	}
	if (Tcl_GetDoubleFromObj(interp, objPtr, &value) != TCL_OK) {
	    return TCL_ERROR;
	}
	values[j] = (float)value;
	if (values[j] < min) {
	    min = values[j];
	}
	if (values[j] > max) {
	    max = values[j];
	}
	if ((values[j] > 0.0f) && (values[j] < logMin)) {
	    logMin = values[j];
	}
	j++;
    }
    resultPtr->min = min;
    resultPtr->max = max;
    resultPtr->logMin = logMin;
    resultPtr->values = values;
    resultPtr->nValues = j;
    return TCL_OK;
}

static void
DestroyDataSource(DataSource *dataSrcPtr)
{
    if ((dataSrcPtr->classPtr != NULL) && 
	(dataSrcPtr->classPtr->destroyProc != NULL)) {
	(*dataSrcPtr->classPtr->destroyProc)(dataSrcPtr);
    }
    Blt_Free(dataSrcPtr);
}


/*ARGSUSED*/
static void
FreeDataSourceProc(
    ClientData clientData,	/* Not used. */
    Display *display,		/* Not used. */
    char *widgRec,
    int offset)
{
    DataSource *srcPtr = (DataSource *)(widgRec + offset);

    DestroyDataSource(srcPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToDataSourceProc --
 *
 *	Given a string representation of a data source, converts it into its
 *	equivalent data source.  A data source may be a list of numbers, a
 *	vector name, or a two element list of table name and column.
 *
 * Results:
 *	The return value is a standard TCL result.  The data source is passed
 *	back via the srcPtr.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToDataSourceProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Interpreter to send results back to */
    Tk_Window tkwin,		/* Not used. */
    Tcl_Obj *objPtr,		/* TCL list of expressions */
    char *widgRec,		/* Element record */
    int offset,			/* Offset to field in structure */
    int flags)			/* Not used. */
{
    Mesh *meshPtr = clientData;
    DataSource *srcPtr;
    DataSource **dataSrcPtrPtr = (DataSource **)(widgRec + offset);
    Tcl_Obj **objv;
    int objc;
    const char *string;

    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 0) {
	if (*dataSrcPtrPtr != NULL) {
	    DestroyDataSource(*dataSrcPtrPtr);
	}
	*dataSrcPtrPtr = NULL;
	return TCL_OK;
    }
    string = Tcl_GetString(objv[0]);
    if ((objc == 1) && (Blt_VectorExists2(interp, string))) {
	srcPtr = CreateVectorDataSource(interp, string);
    } else if ((objc == 2) && (Blt_Table_TableExists(interp, string))) {
	srcPtr = CreateTableDataSource(interp, string, objv[1]);
    } else {
	srcPtr = CreateListDataSource(interp, objc, objv);
    }
    srcPtr->clientData = meshPtr;
    srcPtr->proc = meshPtr->classPtr->configProc;
    if (*dataSrcPtrPtr != NULL) {
	DestroyDataSource(*dataSrcPtrPtr);
    }
    *dataSrcPtrPtr = srcPtr;
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * DataSourceToObjProc --
 *
 *	Converts the data source to its equivalent string representation.
 *	The data source may be a table, vector, or list.
 *
 * Results:
 *	The string representation of the data source is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
DataSourceToObjProc(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Not used. */
    char *widgRec,		/* Element record */
    int offset,			/* Offset to field in structure */
    int flags)			/* Not used. */
{
    DataSource *srcPtr = (DataSource *)(widgRec + offset);
    
    return (srcPtr->classPtr->printProc)(srcPtr);
}

static void
DestroyMesh(Mesh *meshPtr)
{
    Graph *graphPtr = meshPtr->graphPtr;

    if (meshPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(&graphPtr->meshTable, meshPtr->hashPtr);
    }
    if (meshPtr->link != NULL) {
	Blt_Chain_DeleteLink(&graphPtr->meshChain, meshPtr->link);
    }
    if (meshPtr->classPtr->destroyProc != NULL) {
	(*meshPtr->classPtr->destroyProc)(meshPtr);
    }
    Blt_FreeOptions(meshPtr->classPtr->configSpecs, (char *)meshPtr,
	graphPtr->display, 0);
    if (meshPtr->triangles != NULL) {
	Blt_Free(meshPtr->triangles);
    }
    if (meshPtr->edges != NULL) {
	Blt_Free(meshPtr->edges);
    }
    if (meshPtr->points != NULL) {
	Blt_Free(meshPtr->points);
    }
    if (meshPtr->hull != NULL) {
	Blt_Free(meshPtr->hull);
    }
    if (meshPtr->x != NULL) {
	DestroyDataSource(meshPtr->x);
    }
    if (meshPtr->y != NULL) {
	DestroyDataSource(meshPtr->y);
    }
    Blt_Free(meshPtr);
    return TCL_OK;
}

static Mesh *
GetMeshFromObj(Tcl_Interp *interp, Graph *graphPtr, Tcl_Obj *objPtr, 
	       Mesh **meshPtrPtr)
{
    const char *string;
    Blt_HashEntry *hPtr;

    string = Tcl_GetString(objPtr);
    hPtr = Blt_FindHashEntry(&graphPtr->meshTable, string);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "can't find mesh \"", string, 
		"\" in graph \"", Tk_PathName(graphPtr->tkwin), "\"",
		(char *)NULL);
	return TCL_ERROR;
    }
    *meshPtrPtr = Blt_GetHashValue(hPtr);
    return TCL_OK;
}

static int
RegularMeshConfigureProc(Mesh *meshPtr)
{
    Tcl_Interp *interp;
    RegularMesh *regPtr = (RegularMesh *)meshPtr;
    MeshPoint *points;
    int i;
    float xStep, yStep;
    int count;
    DataSourceResult x, y;

    interp = meshPtr->interp;
    if ((regPtr->x->classPtr == NULL) || (regPtr->y->classPtr == NULL)) {
	return TCL_OK;
    }
    if ((*regPtr->x->classPtr->getProc)(interp, regPtr->x, &x) != TCL_OK) {
	return TCL_ERROR;
    }
    if (x.nValues != 3) {
	Tcl_AppendResult(interp, 
		"wrong # of elements for x regular mesh description.",
		(char *)NULL);
	return TCL_ERROR;
    }
    if ((*regPtr->y->classPtr->getProc)(interp, regPtr->y, &y) != TCL_OK) {
	return TCL_ERROR;
    }
    if (y.nValues != 3) {
	Tcl_AppendResult(interp, 
		"wrong # of elements for y rectangular mesh description.",
		(char *)NULL);
	return TCL_ERROR;
    }

    regPtr->xMin = x.values[0];
    regPtr->xMax = x.values[1];
    regPtr->xNum = (int)x.values[2];
    regPtr->yMin = y.values[0];
    regPtr->yMax = y.values[1];
    regPtr->yNum = (int)y.values[2];
    regPtr->xLogMin = x.logMin;
    regPtr->yLogMin = y.logMin;
    Blt_Free(x.values);
    Blt_Free(y.values);
    
    if (regPtr->xNum) {
	Tcl_AppendResult(interp, "# of x-values for rectangular mesh",
			 (char *)NULL);
	return TCL_ERROR;
    }
    if (regPtr->yNum) {
	Tcl_AppendResult(interp, "# of y-values for rectangular mesh",
			 (char *)NULL);
	return TCL_ERROR;
    }
    if (regPtr->xMin == regPtr->xMax) {
	return TCL_ERROR;
    }
    if (regPtr->yMin == regPtr->yMax) {
	return TCL_ERROR;
    }
    nPoints = x.nValues * y.nValues;
    points = Blt_Malloc(nPoints * sizeof(MeshPoint));
    if (points == NULL) {
	Tcl_AppendResult(interp, "can't allocate ", Itoa(nPoints), 
		" points", (char *)NULL);
	return TCL_ERROR;
    }
    xStep = (regPtr->xMax - regPtr->xMin) / (float)(regPtr->xNum - 1);
    yStep = (regPtr->yMax - regPtr->yMin) / (float)(regPtr->yNum - 1);
    {
	int count;
	MeshPoint *p;

	p = points;
	count = 0;
	for (i = 0; i < regPtr->yNum; i++) {
	    float y0;
	    int j;
	    
	    y0 = regPtr->yMin + (yStep * i);
	    for (j = 0; j < regPtr->xNum; j++) {
		p->x = regPtr->xMin + (xStep * j);
		p->y = y;
		p->index = count;
		count++;
		p++;
	    }
	}
    }
    if (regPtr->points != NULL) {
	Blt_Free(regPtr->points);
    }
    regPtr->points = points;
    regPtr->nPoints = nPoints;

    if (Triangulate(interp, regPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
IrregularMeshConfigureProc(Mesh *meshPtr)
{
    IrregularMesh *irregPtr = (IrregularMesh *)meshPtr;
    MeshPoint *points;
    int i;
    DataSourceResult x, y;

    if ((irregPtr->x->classPtr == NULL) || (irregPtr->y->classPtr == NULL)) {
	return TCL_OK;
    }
    if ((*irregPtr->x->classPtr->getProc)(irregPtr->interp, irregPtr->x, &x) 
	!= TCL_OK) {
	return TCL_ERROR;
    }
    if (x.nValues < 2) {
	Tcl_AppendResult(interp, 
		"wrong # of elements for x rectangular mesh description.",
		(char *)NULL);
	return TCL_ERROR;
    }
    irregPtr->xMin = x.min;
    irregPtr->xMax = x.max;
    irregPtr->xLogMin = x.logMin;
    if ((*irregPtr->y->classPtr->getProc)(irregPtr->interp, irregPtr->y, &y) 
	!= TCL_OK) {
	return TCL_ERROR;
    }
    if (y.nValues != 2) {
	Tcl_AppendResult(interp, 
		"wrong # of elements for y rectangular mesh description.",
		(char *)NULL);
	return TCL_ERROR;
    }
    irregPtr->yMin = y.min;
    irregPtr->yMax = y.max;
    irregPtr->yLogMin = y.logMin;

    nPoints = x.nValues * y.nValues;
    points = Blt_Malloc(nPoints * sizeof(MeshPoint));
    if (points == NULL) {
	Tcl_AppendResult(interp, "can't allocate ", Itoa(nPoints), 
		" points", (char *)NULL);
	return TCL_ERROR;
    }
    {
	int i, count;
	MeshPoint *p;

	p = points;
	count = 0;
	for (i = 0; i < y.nValues; i++) {
	    int j;
	    
	    for (j = 0; j < x.nValues; j++) {
		p->x = x.values[j];
		p->y = y.values[i];
		p->index = count;
		count++;
		p++;
	    }
	}
    }
    Blt_Free(x.values);
    Blt_Free(y.values);
    if (irregPtr->points != NULL) {
	Blt_Free(irregPtr->points);
    }
    irregPtr->points = points;
    irregPtr->nPoints = nPoints;
    irregPtr->xMin = x.min;
    irregPtr->xLogMin = x.logMin;
    irregPtr->xMax = x.max;
    irregPtr->yMin = y.min;
    irregPtr->yLogMin = y.logMin;
    irregPtr->yMax = y.max;
    if (Triangulate(interp, irregPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
CloudMeshConfigureProc(Mesh *meshPtr)
{
    CloudMesh *cloudPtr = (CloudMesh *)meshPtr;
    MeshPoint *points;
    DataSourceResult x, y;
    int i, nPoints;

    if ((cloudPtr->x->classPtr == NULL) || (cloudPtr->y->classPtr == NULL)) {
	return TCL_OK;
    }
    if ((*cloudPtr->x->classPtr->getProc)(cloudPtr->interp, cloudPtr->x, &x) 
	!= TCL_OK) {
	return TCL_ERROR;
    }
    if (x.nValues < 3) {
	Tcl_AppendResult(interp, "too few values for x cloud mesh description.",
		(char *)NULL);
	return TCL_ERROR;
    }
    cloudPtr->xMin = x.min, cloudPtr->xMax = x.max;
    cloudPtr->logMin = x.logMin;
    if ((*cloudPtr->y->classPtr->getProc)(cloudPtr->interp, cloudPtr->y, &y) 
	!= TCL_OK) {
	return TCL_ERROR;
    }
    if (y.nValues < 3) {
	Tcl_AppendResult(interp, 
		"too few values for y cloud mesh description.",
		(char *)NULL);
	return TCL_ERROR;
    }
    cloudPtr->yMin = y.min, cloudPtr->yMax = y.max;
    cloudPtr->logMin = y.logMin;
    if (x.nValues != y.nValues) {
	Tcl_AppendResult(interp, " # of values for x and y do not match.",
		(char *)NULL);
	return TCL_ERROR;
    }
    nPoints = x.nValues;
    points = Blt_Malloc(nPoints * sizeof(MeshPoint));
    if (points == NULL) {
	Tcl_AppendResult(interp, "can't allocate ", Itoa(nPoints), 
			 " points", (char *)NULL);
	return TCL_ERROR;
    }
    if (cloudPtr->points != NULL) {
	Blt_Free(cloudPtr->points);
    }
    for (i = 0; i < nPoints; i++) {
	p->x = x.values[i];
	p->y = y.values[i];
	p->index = i;
	p++;
    }
    Blt_Free(x.values);
    Blt_Free(y.values);
    if (cloudPtr->points != NULL) {
	Blt_Free(cloudPtr->points);
    }
    cloudPtr->points = points;
    cloudPtr->nPoints = nPoints;

    if (Triangulate(interp, cloudPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
TriangularMeshConfigureProc(Mesh *meshPtr)
{
    return TCL_OK;
}

static Mesh *
CreateTriangularMesh(const char *name)
{
    TriangularMesh *meshPtr;

    /* Allocate memory for the new mesh. */
    meshPtr = Blt_AssertCalloc(1, sizeof(TriangularMesh));
    meshPtr->classPtr = &triangularMeshClass;
    meshPtr->name = Blt_AssertStrdup(name);
    return (Mesh *)meshPtr;
}

static Mesh *
CreateRegularMesh(const char *name)
{
    RegularMesh *meshPtr;

    /* Allocate memory for the new mesh. */
    meshPtr = Blt_AssertCalloc(1, sizeof(RegularMesh));
    meshPtr->classPtr = &regularMeshClass;
    meshPtr->name = Blt_AssertStrdup(name);
    return (Mesh *)meshPtr;
}

static Mesh *
CreateIrregularMesh(const char *name)
{
    IrregularMesh *meshPtr;

    /* Allocate memory for the new mesh. */
    meshPtr = Blt_AssertCalloc(1, sizeof(IrregularMesh));
    meshPtr->classPtr = &irregularMeshClass;
    meshPtr->name = Blt_AssertStrdup(name);
    return (Mesh *)meshPtr;
}

static Mesh *
CreateCloudMesh(const char *name)
{
    CloudMesh *meshPtr;

    /* Allocate memory for the new mesh. */
    meshPtr = Blt_AssertCalloc(1, sizeof(CloudMesh));
    meshPtr->classPtr = &cloudMeshClass;
    meshPtr->name = Blt_AssertStrdup(name);
    return (Mesh *)meshPtr;
}

static int
TypeOp(Graph *graphPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Mesh *meshPtr;

    if (GetMeshFromObj(interp, graphPtr, objv[3], &meshPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetStringObj(Tcl_GetObjResult(interp), meshPtr->classPtr->type, -1);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * NamesOp --
 *
 *	Returns a list of marker identifiers in interp->result;
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
static int
NamesOp(Graph *graphPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_Obj *listObjPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (objc == 3) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch iter;

	for (hPtr = Blt_FirstHashEntry(&graphPtr->meshTable, &iter); 
	     hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	    Mesh *meshPtr;

	    meshPtr = Blt_GetHashValue(hPtr);
	    Tcl_ListObjAppendElement(interp, listObjPtr, 
		Tcl_NewStringObj(meshPtr->name, -1));
	}
    } else {
	Blt_HashEntry *hPtr;
	Blt_HashSearch iter;

	for (hPtr = Blt_FirstHashEntry(&graphPtr->meshTable, &iter); 
	     hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	    Marker *markerPtr;
	    int i;

	    markerPtr = Blt_GetHashValue(hPtr);
	    for (i = 3; i < objc; i++) {
		const char *pattern;

		pattern = Tcl_GetString(objv[i]);
		if (Tcl_StringMatch(meshPtr->name, pattern)) {
		    Tcl_ListObjAppendElement(interp, listObjPtr,
			Tcl_NewStringObj(meshPtr->name, -1));
		    break;
		}
	    }
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

static int
ConfigureOp(Graph *graphPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Mesh *meshPtr;

    if (GetMeshFromObj(interp, graphPtr, objv[3], &meshPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 0) {
	return Blt_ConfigureInfoFromObj(interp, graphPtr->tkwin, 
		meshPtr->classPtr->configSpecs, (char *)meshPtr, 
		(Tcl_Obj *)NULL, flags);
    } else if (objc == 1) {
	return Blt_ConfigureInfoFromObj(interp, graphPtr->tkwin, 
		meshPtr->classPtr->configSpecs, (char *)meshPtr, 
		objv[4], flags);
    }
    bltDataSourceOption.clientData = meshPtr;
    if (Blt_ConfigureWidgetFromObj(interp, graphPtr->tkwin, 
		meshPtr->classPtr->configSpecs, objc - 3, objv + 3, 
		(char *)meshPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((*meshPtr->classPtr->configProc)(meshPtr) != TCL_OK) {
	return TCL_ERROR;	/* Failed to configure element */
    }
    return TCL_OK;
}

static int
CreateOp(Graph *graphPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Mesh *meshPtr;
    Blt_HashEntry *hPtr;
    char ident[200];
    char *name;
    int i;
    char c;
    char *string;
    int length;

    /* Scan for "-name" option. We need it for the component name */
    name = NULL;
    for (i = 4; i < objc; i += 2) {
	string = Tcl_GetStringFromObj(objv[i], &length);
	if ((length > 1) && (strncmp(string, "-name", length) == 0)) {
	    name = Tcl_GetString(objv[i + 1]);
	    break;
	}
    }
    /* If no name was given for the marker, make up one. */
    if (name == NULL) {
	sprintf_s(ident, 200, "mesh%d", graphPtr->nextMeshId++);
	name = ident;
    } else if (name[0] == '-') {
	Tcl_AppendResult(interp, "name of marker \"", name, 
		"\" can't start with a '-'", (char *)NULL);
	return TCL_ERROR;
    }

    string = Tcl_GetString(objv[3]);
    c = string[0];
    length = strlen(string);
    if ((c == 't') && (strncmp(string, "triangle", length) == 0)) {
	meshPtr = CreateTrigangularMesh(name);
    } else if ((c == 'r') && (strncmp(string, "regular", length) == 0)) {
	meshPtr = CreateRegularMesh(name);
    } else if ((c == 'i') && (strncmp(string, "irregular", length) == 0)) {
	meshPtr = CreateIrregularMesh(name);
    } else if ((c == 'c') && (strncmp(string, "cloud", length) == 0)) {
	meshPtr = CreateCloudMesh(name);
    } else {
	Tcl_AppendResult(interp, "unknown mesh type \"", string, "\"",
			 (char *)NULL);
	return TCL_ERROR;
    }
    meshPtr->graphPtr = graphPtr;
    meshPtr->interp = interp;
    meshPtr->refCount = 1;

    /* Parse the configuration options. */
    if (Blt_ConfigureComponentFromObj(interp, graphPtr->tkwin, name,
	meshPtr->classPtr->name, meshPtr->classPtr->configSpecs, 
	objc - 4, objv + 4, (char *)meshPtr, 0) != TCL_OK) {
	DestroyMesh(meshPtr);
	return TCL_ERROR;
    }
    hPtr = Blt_CreateHashEntry(&graphPtr->meshTable, meshPtr->name, &isNew);
    if (!isNew) {
	Mesh *oldMeshPtr;
	oldMeshPtr = Blt_GetHashValue(hPtr);
	if ((oldMeshPtr->flags & DELETE_PENDING) == 0) {
	    Tcl_AppendResult(graphPtr->interp, "mesh \"", meshPtr->name,
		"\" already exists in \"", Tk_PathName(graphPtr->tkwin), "\"",
		(char *)NULL);
	    DestroyMesh(meshPtr);
	    return TCL_ERROR;
	}
	oldMeshPtr->hashPtr = NULL; /* Remove the mesh from the table. */
    }
    Blt_SetHashValue(hPtr, meshPtr);
    meshPtr->hashPtr = hPtr;
    if ((meshPtr->classPtr->configProc)(meshPtr) != TCL_OK) {
	DestroyMesh(meshPtr);
	return TCL_ERROR;
    }
    meshPtr->link = Blt_Chain_Append(graphPtr->meshChain, meshPtr);
    return TCL_OK;
}

static int
DeleteOp(Graph *graphPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int i;

    for (i = 3; i < objc; i++) {
	Mesh *meshPtr;

	if (GetMeshFromObj(interp, graphPtr, objv[i], &meshPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (meshPtr->flags & DELETE_PENDING) {
	    Tcl_AppendResult(interp, "can't find mesh \"", 
		Tcl_GetString(objv[i]), "\" in \"", 
		Tk_PathName(graphPtr->tkwin), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	meshPtr->flags |= DELETE_PENDING;
	if (meshPtr->refCount == 0) {
	    DestroyMesh(meshPtr);
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * MeshOp --
 *
 *	.g mesh create regular -name $name -x {x0 xN n} -y {y0 yN n}
 *	.g mesh create irregular -name $name -x $xvalues -y $yvalues 
 *	.g mesh create triangular -name $name -x x -y y -edges $edges \
 *		-triangles $triangles
 *	.g mesh create cloud -name $name -x x -y y 
 *	.g mesh type $name
 *	.g mesh names ?pattern?
 *	.g mesh delete $name
 *	.g mesh configure $name -hide no -linewidth 1 -color black
 *
 *---------------------------------------------------------------------------
 */

static Blt_OpSpec meshOps[] =
{
    {"cget",        2, CgetOp,       4, 4, "meshName option",},
    {"create",      2, CreateOp,     4, 0, "type ?option value?...",},
    {"configure",   2, ConfigureOp,  3, 0, "meshName ?option value?...",},
    {"delete",      1, DeleteOp,     3, 0, "?meshName?...",},
    {"names",       1, NamesOp,      3, 0, "?pattern?...",},
    {"type",        2, TypeOp,       4, 4, "meshName",},
};
static int nMeshOps = sizeof(meshOps) / sizeof(Blt_OpSpec);

int
Blt_MeshOp(Graph *graphPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_ObjCmdProc *proc;

    proc = Blt_GetOpFromObj(interp, nMeshOps, meshOps, BLT_OP_ARG2, 
	objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc) (interp, graphPtr, objc, objv);
}

void
Blt_DestroyMeshes(Graph *graphPtr)
{
    Blt_ChainLink link, next;

    for (link = Blt_Chain_FirstLink(graphPtr->meshChain); link != NULL;
	 link = next) {
	Mesh *meshPtr;

	next = Blt_Chain_NextLink(link);
	meshPtr = Blt_Chain_GetValue(link);
	DestroyMesh(meshPtr);
    }
}

void
Blt_FreeMesh(Mesh *meshPtr)
{
    if (meshPtr != NULL) {
	meshPtr->refCount--;
	if ((meshPtr->refCount == 0) && (meshPtr->flags & DELETE_PENDING)) {
	    DestroyMesh(meshPtr);
	}
    }
}

int
Blt_GetMeshFromObj(Tcl_Interp *interp, Graph *graphPtr, Tcl_Obj *objPtr, 
		   Mesh **meshPtrPtr)
{
    Mesh *meshPtr;

    if (GetMeshFromObj(interp, graphPtr, objPtr, &meshPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    meshPtr->refCount++;
    *meshPtrPtr = meshPtr;
    return TCL_OK;
}


static void
ComputeRanges(int nTriangles, Triangle *triangles, float *values)
{
    Triangle *p, *tend;

    for (p = triangles, tend = triangles + nTriangles; p < tend; p++) {
	float za, zb, zc;

	za = values[points[p->a].index].z;
	zb = values[points[p->b].index].z;
	zc = values[points[p->c].index].z;

	p->min = p->max = za;
	if (zb < p->min) {
	    p->min = zb;
	}
	if (zc < p->min) {
	    p->min = zc;
	}
	if (zb > p->max) {
	    p->max = zb;
	}
	if (zc > p->max) {
	    p->max = zc;
	}
    }
}

static INLINE float 
IsLeft(MeshPoint *points, int i, int j, int k) 
{
    return (((points[i].x - points[j].x) * (points[k].y - points[j].y)) -
	    ((points[i].y - points[j].y) * (points[k].x - points[j].x)));
}

/*
 *---------------------------------------------------------------------------
 *
 * ChainHull2d --
 *
 *	Computes the convex hull from the points of the mesh.  
 *
 *	Notes: 1. The array of points is assumed to be sorted.  
 *	       2. An array to contain the points is assumed.  This is 
 *		  allocated by the caller.
 *	
 * Results:
 *     	The number of points in the convex hull is returned. The coordinates
 *	of the hull will be written in the given point array.
 *
 *	Copyright 2001, softSurfer (www.softsurfer.com)
 *	This code may be freely used and modified for any purpose
 *	providing that this copyright notice is included with it.
 *	SoftSurfer makes no warranty for this code, and cannot be held
 *	liable for any real or imagined damage resulting from its use.
 *	Users of this code must verify correctness for their application.
 *
 *---------------------------------------------------------------------------
 */

#define Push(index) (hull[top] = (index), top++)
#define Pop()	    (top--)

static int 
ChainHull2d(int nPts, MeshPoint *pts, int *hull) 
{
    int bot, top, i; 
    int minMin, minMax;
    int maxMin, maxMax;
    float xMin, xMax;

    bot = top = 0;		/* Points to next location on stack. */
    minMin = 0;

    /* 
     * Step 1. Get the indices of points with max x-coord and min|max
     *	       y-coord .
     */

    xMin = pts[0].x;
    for (i = 1; i < nPts; i++) {
	if (pts[i].x != xMin) {
	    break;
	}
    }
    minMax = i - 1;
    
    if (minMax == (nPts - 1)) {       
	/* Degenerate case: all x-coords == xMin */
	Push(minMin)
	if (pts[minMax].y != pts[minMin].y) {
	    Push(minMax);	    /* A nontrivial segment. */
	}
	Push(minMin);
	return top;
    }

    maxMax = nPts - 1;
    xMax = pts[maxMax].x;
    for (i = maxMax - 1; i >= 0; i--) {
	if (pts[i].x != xMax) {
	    break;
	}
    }
    maxMin = i+1;
    
    /*
     * Step 2. Compute the lower hull on the stack.
     */
    Push(minMin);		/* push minMin point onto stack */
    i = minMax;

    while (++i <= maxMin) {
	/* The lower line joins pts[minMin] with pts[maxMin]. */
	if ((IsLeft(pts[minMin], pts[maxMin], pts[i]) >= 0.0f) && 
	    (i < maxMin)) {
	    continue;	/* Ignore pts[i] above or on the lower line */
	}
	while (top > 1) {
	    /* There are at least 2 pts on the stack. */
	    
	    /* Test if pts[i] is left of the line at the stack top. */
	    if (IsLeft(pts[hull[top-2]], pts[hull[top-1]], pts[i]) > 0.0f) {
		break;         /* Pts[i] is a new hull vertex. */
	    } else {
		Pop();         /* Pop top point off stack */
	    }
	}
	Push(i);		/* Push point[i] onto stack. */
    }

    /* 
     * Step 3. Compute the upper hull on the stack above the bottom hull.
     */

    if (maxMax != maxMin)  {
	// if distinct xMax points
        Push(maxMax);		/* Push maxMax point onto stack. */
    }

    bot = top - 1;		/* The bottom point of the upper hull stack. */
    i = maxMin;

    while (--i >= minMax) {
        /* The upper line joins pts[maxMax] with pts[minMax]. */
        if ((IsLeft(pts[maxMax], pts[minMax], pts[i]) >= 0) && 
	    (i > minMax)) {
            continue;          /* Ignore pts[i] below or on the upper
				* line. */
	}
        while (top >= bot) {
	    /* There are at least 2 points on the upper stack. */

            /*  Test if points[i] is left of the line at the stack top. */
            if (Isleft(pts[hull[top-2]], pts[hull[top-1]], pts[i]) > 0) {
                break;         /* pts[i] is a new hull vertex. */
	    } else {
                Pop();         /* Pop top point off stack. */
	    }
        }
        Push(i);		/* Push pts[i] onto stack. */
    }
    if (minMax != minMin) {
        Push(minMin);  /* Push joining endpoint onto stack. */
    }
    return top;
}

static int
ConvexHull(Tcl_Interp *interp, Mesh *meshPtr) 
{
    int *hull;
    int n;

    /* Allocate worst-case storage initially for the hull. */
    hull = Blt_Malloc(meshPtr->nPoints * sizeof(int));
    if (hull == NULL) {
	Tcl_AppendResult(interp, "can't allocate memory for convex hull ", 
		(char *)NULL);
	return TCL_ERROR;
    }

    /* Compute the convex hull. */
    n = ChainHull2d(meshPtr->nPoints, meshPtr->points, hull);

    /* Resize the hull array to the actual # of boundary points. */
    if (n < meshPtr->nPoints) {
	hull = Blt_Realloc(hull, n * sizeof(int));
	if (hull == NULL) {
	    Tcl_AppendResult(interp, "can't reallocate memory for convex hull ",
		(char *)NULL);
	    return TCL_ERROR;
	}
    }
    if (meshPtr->hull != NULL) {
	Blt_Free(meshPtr->hull);
    }
    meshPtr->hull = hull;
    meshPtr->nHullPts = n;
    return TCL_OK;
}

static int
ComparePoints(const void *a, const void *b)
{
    const MeshPoint *s1 = a;
    const MeshPoint *s2 = b;

    if (s1->y < s2->y) {
	return -1;
    }
    if (s1->y > s2->y) {
	return 1;
    }
    if (s1->x < s2->x) {
	return -1;
    }
    if (s1->x > s2->x) {
	return 1;
    }
    return 0;
}

static void
Triangulate(Tcl_Interp *interp, Mesh *meshPtr)
{
    Triangle *triangles;
    int nTriangles;

    /* Sort the points in the mesh. This is useful for both computing the
     * convex hull and triangulating the mesh. */
    qsort(meshPtr->points, meshPtr->nPoints, sizeof(MeshPoint), ComparePoints);

    /* Compute the convex hull first, this will provide an estimate for the
     * boundary points and therefore the number of triangles and edges. */
    if (ConvexHull(interp, meshPtr) != TCL_OK) {
	return TCL_ERROR;
    }

    /* Determine the number of triangles and edges. */
    meshPtr->nTriangles = 2 * meshPtr->nPoints - 2 - meshPtr->nHullPts;
    meshPtr->nEdges = 3 * meshPtr->nPoints - 3 - meshPtr->nHullPts;
    triangles = Blt_Malloc(meshPtr->nTriangles * sizeof(Triangle));
    if (triangles == NULL) {
	Tcl_AppendResult(interp, "can't allocate ", 
		Blt_Itoa(meshPtr->nTriangles), " triplets", (char *)NULL);
	return TCL_ERROR;
    }

    /*
     * Mesh the points to get triangles.
     */
    nTriangles = Blt_Triangulate(meshPtr->nPoints, meshPtr->points, TRUE, 
	triangles);
    if (meshPtr->triangles != NULL) {
	Blt_Free(meshPtr->triangles);
    }
    meshPtr->nTriangles = nTriangles;
    meshPtr->triangles = triangles;
}
