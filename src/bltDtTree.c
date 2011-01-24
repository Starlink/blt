
/*
 *
 * bltDtTree.c --
 *
 *	Copyright 1998-2005 George A Howlett.
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

#include <blt.h>

#include "config.h"
#ifndef NO_DATATABLE

#include <tcl.h>
#include <bltDataTable.h>
#include <bltTree.h>
#include <bltSwitch.h>
#ifdef HAVE_MEMORY_H
#  include <memory.h>
#endif /* HAVE_MEMORY_H */

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

/*
 * ExportSwitches --
 */
typedef struct {
    /* Private data. */
    Blt_TreeNode node;

    /* Public fields */
    Blt_TableIterator rIter, cIter;
    Blt_TableIterator hIter;
    Tcl_Obj *nodeObjPtr;
} ExportSwitches;

BLT_EXTERN Blt_SwitchFreeProc Blt_Table_ColumnIterFreeProc;
BLT_EXTERN Blt_SwitchFreeProc Blt_Table_RowIterFreeProc;
BLT_EXTERN Blt_SwitchParseProc Blt_Table_ColumnIterSwitchProc;
BLT_EXTERN Blt_SwitchParseProc Blt_Table_RowIterSwitchProc;

static Blt_SwitchCustom columnIterSwitch = {
    Blt_Table_ColumnIterSwitchProc, Blt_Table_ColumnIterFreeProc, 0,
};
static Blt_SwitchCustom rowIterSwitch = {
    Blt_Table_RowIterSwitchProc, Blt_Table_RowIterFreeProc, 0,
};

static Blt_SwitchSpec exportSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-columns", "columns",
	Blt_Offset(ExportSwitches, cIter), 0, 0, &columnIterSwitch},
    {BLT_SWITCH_CUSTOM, "-hierarchy", "columns",
	Blt_Offset(ExportSwitches, hIter), 0, 0, &columnIterSwitch},
    {BLT_SWITCH_OBJ, "-node", "node",
	Blt_Offset(ExportSwitches, nodeObjPtr), 0},
    {BLT_SWITCH_CUSTOM, "-rows", "rows",
        Blt_Offset(ExportSwitches, rIter), 0, 0, &rowIterSwitch},
    {BLT_SWITCH_END}
};

DLLEXPORT extern Tcl_AppInitProc Blt_Table_TreeInit;

static Blt_TableImportProc ImportTreeProc;
static Blt_TableExportProc ExportTreeProc;

static int
ImportTree(Tcl_Interp *interp, Blt_Table table, Blt_Tree tree, 
	   Blt_TreeNode top)
{
    Blt_TreeNode node;
    int maxDepth, topDepth;
    long iRow, nCols;

    /* 
     * Fill in the table data in 2 passes.  We need to know the
     * maximum depth of the leaf nodes, to generate columns for each
     * level of the hierarchy.  We have to do this before adding
     * node data values.
     */

    /* Pass 1.  Create entries for all the nodes. Add entries for 
     *          the node and it's ancestor's labels. */
    maxDepth = topDepth = Blt_Tree_NodeDepth(top);
    nCols = Blt_Table_NumColumns(table);
    for (node = Blt_Tree_NextNode(top, top); node != NULL;
	 node = Blt_Tree_NextNode(top, node)) {
	Blt_TreeNode parent;
	int depth;
	Blt_TableRow row;
	size_t iCol;

	depth = Blt_Tree_NodeDepth(node);
	if (depth > maxDepth) {
	    Blt_TableColumn col;

	    if (Blt_Table_ExtendColumns(interp, table, 1, &col) != TCL_OK) {
		return TCL_ERROR;
	    }
	    iCol = Blt_Table_ColumnIndex(col);
	    maxDepth = depth;
	} else {
	    iCol = depth - topDepth;
	}
	if (Blt_Table_ExtendRows(interp, table, 1, &row) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (parent = node; parent != top; 
	     parent = Blt_Tree_ParentNode(parent)){
	    const char *label;
	    Blt_TableColumn col;

	    col = Blt_Table_FindColumnByIndex(table, iCol);
	    label = Blt_Tree_NodeLabel(parent);
	    if (Blt_Table_SetString(table, row, col, label, -1)!=TCL_OK) {
		return TCL_ERROR;
	    }
	    iCol--;
	}
    }
    /* Pass 2.  Fill in entries for all the data fields found. */
    for (iRow = 1, node = Blt_Tree_NextNode(top, top); node != NULL;
	 node = Blt_Tree_NextNode(top, node), iRow++) {
	Blt_TreeKey key;
	Blt_TreeKeyIterator iter;
	Blt_TableRow row;

	row = Blt_Table_FindRowByIndex(table, iRow);
	for (key = Blt_Tree_FirstKey(tree, node, &iter); key != NULL;
	     key = Blt_Tree_NextKey(tree, &iter)) {
	    Blt_TableColumn col;
	    Tcl_Obj *objPtr;

	    if (Blt_Tree_GetValue(interp, tree, node, key, &objPtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    col = Blt_Table_FindColumnByLabel(table, key);
	    if (col == NULL) {
		col = Blt_Table_CreateColumn(interp, table, key);
		if (col == NULL) {
		    return TCL_ERROR;
		}
	    }
	    if (Blt_Table_SetObj(table, row, col, objPtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

static int
ImportTreeProc(Blt_Table table, Tcl_Interp *interp, int objc, 
	       Tcl_Obj *const *objv)
{
    Blt_Tree tree;
    Blt_TreeNode node;

    /* FIXME: 
     *	 2. Export *GetNode tag parsing routines from bltTreeCmd.c,
     *	    instead of using node id to select the top node.
     */
    tree = Blt_Tree_Open(interp, Tcl_GetString(objv[3]), 0);
    if (tree == NULL) {
	return TCL_ERROR;
    }
    if (objc == 5) {
	long inode;

	if (Tcl_GetLongFromObj(interp, objv[4], &inode) != TCL_OK) {
	    return TCL_ERROR;
	}
	node = Blt_Tree_GetNode(tree, inode);
	if (node == NULL) {
	    return TCL_ERROR;
	}
    } else {
	node = Blt_Tree_RootNode(tree);
    }
    return ImportTree(interp, table, tree, node);
}

static int
ExportTree(Tcl_Interp *interp, Blt_Table table, Blt_Tree tree, 
	   Blt_TreeNode parent, ExportSwitches *switchesPtr) 
{
    Blt_TableRow row;

    for (row = Blt_Table_FirstTaggedRow(&switchesPtr->rIter); row != NULL;
	 row = Blt_Table_NextTaggedRow(&switchesPtr->rIter)) {
	Blt_TableColumn col;
	Blt_TreeNode node;
	const char *label;

	label = Blt_Table_RowLabel(row);
	node = Blt_Tree_CreateNode(tree, parent, label, -1);
	for (col = Blt_Table_FirstTaggedColumn(&switchesPtr->cIter); col != NULL;
	     col = Blt_Table_NextTaggedColumn(&switchesPtr->cIter)) {
	    Tcl_Obj *objPtr;
	    const char *key;

	    objPtr = Blt_Table_GetObj(table, row, col);
	    key = Blt_Table_ColumnLabel(col);
	    if (Blt_Tree_SetValue(interp, tree, node, key, objPtr) != TCL_OK) {
		return TCL_ERROR;
	    }		
	}
    }
    return TCL_OK;
}

static int
ExportTreeProc(Blt_Table table, Tcl_Interp *interp, int objc, 
	       Tcl_Obj *const *objv)
{
    Blt_Tree tree;
    Blt_TreeNode node;
    ExportSwitches switches;
    int result;

    if (objc < 4) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", 
		Tcl_GetString(objv[0]), " export tree treeName ?switches?\"",
		(char *)NULL);
	return TCL_ERROR;
    }
    tree = Blt_Tree_Open(interp, Tcl_GetString(objv[3]), 0);
    if (tree == NULL) {
	return TCL_ERROR;
    }
    memset(&switches, 0, sizeof(switches));
    rowIterSwitch.clientData = table;
    columnIterSwitch.clientData = table;
    Blt_Table_IterateAllRows(table, &switches.rIter);
    Blt_Table_IterateAllColumns(table, &switches.cIter);
    if (Blt_ParseSwitches(interp, exportSwitches, objc - 4, objv + 4, &switches,
	BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if (switches.nodeObjPtr != NULL) {
	long inode;

	if (Tcl_GetLongFromObj(interp, switches.nodeObjPtr, &inode) != TCL_OK) {
	    return TCL_ERROR;
	}
	node = Blt_Tree_GetNode(tree, inode);
	if (node == NULL) {
	    return TCL_ERROR;
	}
    } else {
	node = Blt_Tree_RootNode(tree);
    }
    result = ExportTree(interp, table, tree, node, &switches);
    Blt_FreeSwitches(exportSwitches, (char *)&switches, 0);
    return result;
}

int 
Blt_Table_TreeInit(Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 1) == NULL) {
	return TCL_ERROR;
    };
#endif
    if (Tcl_PkgRequire(interp, "blt_core", BLT_VERSION, /*Exact*/1) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgProvide(interp, "blt_datatable_tree", BLT_VERSION) != TCL_OK) { 
	return TCL_ERROR;
    }
    return Blt_Table_RegisterFormat(interp,
        "tree",			/* Name of format. */
	ImportTreeProc,		/* Import procedure. */
	ExportTreeProc);	/* Export procedure. */

}
#endif /* NO_DATATABLE */

