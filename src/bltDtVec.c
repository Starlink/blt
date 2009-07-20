
/*
 *
 * bltDtVec.c --
 *
 *	Copyright 1998-2005 George A Howlett.
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

#include <blt.h>

#include "config.h"
#include <assert.h>
#include <tcl.h>
#include <bltSwitch.h>
#include <bltDataTable.h>
#include <bltVector.h>

extern double Blt_NaN(void);

DLLEXPORT extern Tcl_AppInitProc Blt_DataTable_VectorInit;

/*
 * Format	Import		Export
 * csv		file/data	file/data
 * tree		data		data
 * vector	data		data
 * xml		file/data	file/data
 * sql		data		data
 *
 * $table import csv -file fileName -data dataString 
 * $table export csv -file defaultFileName 
 * $table import tree $treeName $node ?switches? 
 * $table export tree $treeName $node "label" "label" "label"
 * $table import vector $vecName label $vecName label...
 * $table export vector label $vecName label $vecName...
 * $table import xml -file fileName -data dataString ?switches?
 * $table export xml -file fileName -data dataString ?switches?
 * $table import sql -host $host -password $pw -db $db -port $port 
 */

static Blt_DataTable_ImportProc ImportVecProc;
static Blt_DataTable_ExportProc ExportVecProc;

/* 
 * $table export vector -file fileName ?switches...?
 * $table export vector ?switches...?
 */
static int
ExportVecProc(Blt_DataTable table, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int i;
    long nRows;
    
    if ((objc - 3) & 1) {
	Tcl_AppendResult(interp, "odd # of column/vector pairs: should be \"", 
		Tcl_GetString(objv[0]), 
		" export vector col vecName ?col vecName?...", (char *)NULL);
	return TCL_ERROR;
    }
    nRows = Blt_DataTable_NumRows(table);
    for (i = 3; i < objc; i += 2) {
	Blt_Vector *vector;
	size_t size;
	double *array;
	int k;
	Blt_DataTableColumn col;

	col = Blt_DataTable_FindColumn(interp, table, objv[i]);
	if (col == NULL) {
	    return TCL_ERROR;
	}
	if (Blt_GetVectorFromObj(interp, objv[i+1], &vector) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Blt_VecLength(vector) != nRows) {
	    if (Blt_ResizeVector(vector, nRows) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	array = Blt_VecData(vector);
	size = Blt_VecSize(vector);
	for (k = 1; k <= Blt_VecLength(vector); k++) {
	    Blt_DataTableRow row;
	    Tcl_Obj *objPtr;

	    row = Blt_DataTable_GetRowByIndex(table, k);
	    assert(row != NULL);
	    objPtr = Blt_DataTable_GetValue(table, row, col);
	    if (objPtr == NULL) {
		array[k-1] = Blt_NaN();
	    } else {
		double value;

		if (Tcl_GetDoubleFromObj(interp, objPtr, &value) != TCL_OK) {
		    return TCL_ERROR;
		}
		array[k-1] = value;
	    }
	}
	if (Blt_ResetVector(vector, array, nRows, size, TCL_STATIC) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ImportVecProc --
 *
 *	Parses the given command line and calls one of several
 *	export-specific operations.
 *	
 * Results:
 *	Returns a standard TCL result.  It is the result of 
 *	operation called.
 *
 *	$table import vector v1 col v2 col v3 col
 *
 *---------------------------------------------------------------------------
 */
static int
ImportVecProc(Blt_DataTable table, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int maxLen;
    int i;

    if ((objc - 3) & 1) {
	Tcl_AppendResult(interp, "odd # of vector/column pairs: should be \"", 
		Tcl_GetString(objv[0]), 
		" import vector vecName col vecName col...", (char *)NULL);
	return TCL_ERROR;
    }
    maxLen = 0;
    for (i = 3; i < objc; i += 2) {
	Blt_DataTableColumn col;
	Blt_Vector *vector;
	
	if (Blt_GetVectorFromObj(interp, objv[i], &vector) != TCL_OK) {
	    return TCL_ERROR;
	}
	col = Blt_DataTable_FindColumn(NULL, table, objv[i+1]);
	if (col == NULL) {
	    const char *label;

	    label = Tcl_GetString(objv[i+1]);
	    col = Blt_DataTable_CreateColumn(interp, table, label);
	    if (col == NULL) {
		return TCL_ERROR;
	    }
	}
	if (Blt_VecLength(vector) > maxLen) {
	    maxLen = Blt_VecLength(vector);
	}
    }
    if (maxLen > Blt_DataTable_NumRows(table)) {
	size_t needed;

	needed = maxLen - Blt_DataTable_NumRows(table);
	if (Blt_DataTable_ExtendRows(interp, table, needed, NULL) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    for (i = 3; i < objc; i += 2) {
	Blt_DataTableColumn col;
	Blt_Vector *vector;
	double *array;
	size_t j, k;
	size_t nElems;

	if (Blt_GetVectorFromObj(interp, objv[i], &vector) != TCL_OK) {
	    return TCL_ERROR;
	}
	col = Blt_DataTable_FindColumn(interp, table, objv[i+1]);
	if (col == NULL) {
	    return TCL_ERROR;
	}
	array = Blt_VecData(vector);
	nElems = Blt_VecLength(vector);
	for (j = 0, k = 1; j < nElems; j++, k++) {
	    Blt_DataTableRow row;

	    row = Blt_DataTable_GetRowByIndex(table, k);
	    if (array[j] == Blt_NaN()) {
		if (Blt_DataTable_UnsetValue(table, row, col) != TCL_OK) {
		    return TCL_ERROR;
		}
	    } else {
		if (Blt_DataTable_SetValue(table, row, col, Tcl_NewDoubleObj(array[j]))
		    != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	}
    }
    return TCL_OK;
}
    
int 
Blt_DataTable_VectorInit(Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 1) == NULL) {
	return TCL_ERROR;
    };
#endif
    if (Tcl_PkgRequire(interp, "blt_core", BLT_VERSION, /*Exact*/1) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgProvide(interp, "blt_datatable_vector", BLT_VERSION) != TCL_OK){ 
	return TCL_ERROR;
    }
    return Blt_DataTable_RegisterFormat(interp,
        "vector",		/* Name of format. */
	ImportVecProc,		/* Import procedure. */
	ExportVecProc);		/* Export procedure. */
}

