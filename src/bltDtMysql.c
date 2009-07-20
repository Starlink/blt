
/*
 *
 * bltDtMysql.c --
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

#ifndef NO_DATATABLE

#include "config.h"
#ifdef HAVE_LIBMYSQL
#include <tcl.h>
#include <bltDataTable.h>
#include <bltAlloc.h>
#include <bltSwitch.h>
#include <mysql/mysql.h>
#ifdef HAVE_MEMORY_H
#  include <memory.h>
#endif /* HAVE_MEMORY_H */

DLLEXPORT extern Tcl_AppInitProc Blt_DataTable_MysqlInit;

/*
 * Format	Import		Export
 * csv		file/data	file/data
 * tree		data		data
 * vector	data		data
 * xml		file/data	file/data
 * mysql	data		data
 *
 * $table import csv -file fileName -data dataString 
 * $table export csv -file defaultFileName 
 * $table import tree $treeName $node ?switches? 
 * $table export tree $treeName $node "label" "label" "label"
 * $table import vector $vecName label $vecName label...
 * $table export vector label $vecName label $vecName...
 * $table import xml -file fileName -data dataString ?switches?
 * $table export xml -file fileName -data dataString ?switches?
 * $table import mysql -host $host -password $pw -db $db -port $port 
 */
/*
 * ImportSwitches --
 */
typedef struct {
    char *host;			/* If non-NULL, name of remote host of
				 * MySql server.  Otherwise "localhost"
				 * is used. */
    char *user;			/* If non-NULL, name of user account
				 * to access MySql server.  Otherwise
				 * the current username is used. */
    char *pw;			/* If non-NULL, is password to use to
				 * access MySql server. */
    char *db;			/* If non-NULL, name of MySql database
				 * to access. */
    Tcl_Obj *query;		/* If non-NULL, query to make. */
    int port;			/* Port number to use. */

    /* Private data. */
    Tcl_Interp *interp;
    unsigned int flags;
    char *buffer;		/* Buffer to read data into. */
    int nBytes;			/* # of bytes in the buffer. */
} ImportSwitches;

static Blt_SwitchSpec importSwitches[] = 
{
    {BLT_SWITCH_STRING, "-db",       "dbName",
	Blt_Offset(ImportSwitches, db), 0, 0},
    {BLT_SWITCH_STRING, "-host",     "hostName",
	Blt_Offset(ImportSwitches, host), 0, 0},
    {BLT_SWITCH_STRING, "-user",     "userName",
	Blt_Offset(ImportSwitches, user), 0, 0},
    {BLT_SWITCH_STRING, "-password", "password",
	Blt_Offset(ImportSwitches, pw), 0, 0},
    {BLT_SWITCH_INT_NNEG, "-port",     "number",
	Blt_Offset(ImportSwitches, port), 0, 0},
    {BLT_SWITCH_OBJ,    "-query",    "string",
	Blt_Offset(ImportSwitches, query), 0, 0},
    {BLT_SWITCH_END}
};

#ifdef EXPORT_MYSQL
/*
 * ExportSwitches --
 */
typedef struct {
    Blt_Chain rowChain;
    Blt_Chain colChain;
    Tcl_Obj *rows, *cols;	/* Selected rows and columns to export. */
    unsigned int flags;
    Tcl_Obj *fileObj;
    Tcl_Channel channel;	/* If non-NULL, channel to write output to. */
    Tcl_DString *dsPtr;
    int length;			/* Length of dynamic string. */
    int count;			/* Number of fields in current record. */
    Tcl_Interp *interp;
    char *quote;		/* Quoted string delimiter. */
    char *sep;			/* Separator character. */
} ExportSwitches;

static Blt_SwitchSpec exportSwitches[] = 
{
    {BLT_SWITCH_OBJ, "-columns", "columns",
	Blt_Offset(ExportSwitches, cols), 0, 0},
    {BLT_SWITCH_OBJ, "-file", "fileName",
	Blt_Offset(ExportSwitches, fileObj), 0, 0},
    {BLT_SWITCH_STRING, "-quote", "char",
	Blt_Offset(ExportSwitches, quote), 0, 0},
    {BLT_SWITCH_OBJ, "-rows", "rows",
	Blt_Offset(ExportSwitches, rows), 0, 0},
    {BLT_SWITCH_STRING, "-separator", "char",
	Blt_Offset(ExportSwitches, sep), 0, 0},
    {BLT_SWITCH_END}
};

static Blt_DataTable_ExportProc ExportMysqlProc;
#endif

#define DEF_CLIENT_FLAGS (CLIENT_MULTI_STATEMENTS|CLIENT_MULTI_RESULTS)

static Blt_DataTable_ImportProc ImportMysqlProc;

static int
MySqlFieldToColumnType(int type) 
{
    switch (type) {
    case FIELD_TYPE_DECIMAL:
    case FIELD_TYPE_TINY:
    case FIELD_TYPE_SHORT:
    case FIELD_TYPE_LONG:
    case FIELD_TYPE_LONGLONG:
    case FIELD_TYPE_INT24:
	return DT_COLUMN_INTEGER;
    case FIELD_TYPE_FLOAT:
    case FIELD_TYPE_DOUBLE:
	return DT_COLUMN_DOUBLE;
    case FIELD_TYPE_TINY_BLOB:
    case FIELD_TYPE_MEDIUM_BLOB:
    case FIELD_TYPE_LONG_BLOB:
    case FIELD_TYPE_BLOB:
	return DT_COLUMN_BINARY;
    case FIELD_TYPE_NULL:
    case FIELD_TYPE_TIMESTAMP:
    case FIELD_TYPE_DATE:
    case FIELD_TYPE_TIME:
    case FIELD_TYPE_DATETIME:
    case FIELD_TYPE_YEAR:
    case FIELD_TYPE_NEWDATE:
    case FIELD_TYPE_ENUM:
    case FIELD_TYPE_SET:
    case FIELD_TYPE_VAR_STRING:
    case FIELD_TYPE_STRING:
    default:
	return DT_COLUMN_STRING;
    }
}

static int
MySqlImportLabels(Tcl_Interp *interp, Blt_DataTable table, MYSQL_RES *myResults, 
		  size_t nCols, Blt_DataTableColumn *cols) 
{
    size_t i;

    for (i = 0; i < nCols; i++) {
	MYSQL_FIELD *fp;
	int type;

	fp = mysql_fetch_field(myResults);
	if (Blt_DataTable_SetColumnLabel(interp, table, cols[i], fp->name) != TCL_OK) {
	    return TCL_ERROR;
	}
	type = MySqlFieldToColumnType(fp->type);
	if (Blt_DataTable_SetColumnType(cols[i], type) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

static int
MySqlImportRows(Tcl_Interp *interp, Blt_DataTable table, MYSQL_RES *myResults, 
		size_t nCols, Blt_DataTableColumn *cols) 
{
    size_t nRows;
    size_t i;

    nRows = mysql_num_rows(myResults);
    if (nRows > Blt_DataTable_NumRows(table)) {
	size_t needed;

	/* Add the number of rows needed */
	needed = nRows - Blt_DataTable_NumRows(table);
	if (Blt_DataTable_ExtendRows(interp, table, needed, NULL) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    for (i = 1; /*empty*/; i++) {
	Blt_DataTableRow row;
	size_t j;
	MYSQL_ROW myRow;
	unsigned long *fieldLengths;

	myRow = mysql_fetch_row(myResults);
	if (myRow == NULL) {
	    if (i < nRows) {
		Tcl_AppendResult(interp, "didn't complete fetching all rows",
				 (char *)NULL);
		return TCL_ERROR;
	    }
	    break;
	}
	fieldLengths = mysql_fetch_lengths(myResults);
	row = Blt_DataTable_GetRowByIndex(table, i);
	for (j = 0; j < nCols; j++) {
	    Tcl_Obj *objPtr;

	    objPtr = Tcl_NewByteArrayObj((unsigned char *)myRow[j], 
					 (int)fieldLengths[j]);
	    if (Blt_DataTable_SetValue(table, row, cols[j], objPtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

static void
MySqlDisconnect(MYSQL *cp) 
{
    mysql_close(cp);
}

static int
MySqlQueryFromObj(Tcl_Interp *interp, MYSQL *cp, Tcl_Obj *objPtr) 
{
    int nBytes;
    const char *query;
    
    query = Tcl_GetStringFromObj(objPtr, &nBytes);
    if (mysql_real_query(cp, query, (unsigned long)nBytes) != 0) {
	Tcl_AppendResult(interp, "error in query \"", query, "\": ", 
			 mysql_error(cp), (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static void
MySqlFreeResults(MYSQL_RES *myResults)
{
    mysql_free_result (myResults);
}

static int
MySqlResults(Tcl_Interp *interp, MYSQL *cp, MYSQL_RES **resultsPtr, 
	     long *nFieldsPtr) 
{
    MYSQL_RES *results;

    results = mysql_store_result(cp);
    if (results != NULL) {
	*nFieldsPtr = mysql_num_fields(results);
    } else if (mysql_field_count(cp) == 0) {
	*nFieldsPtr = 0;
    } else {
	Tcl_AppendResult(interp, "error collecting results: ", mysql_error(cp), 
			 (char *)NULL);
	return TCL_ERROR;
    }
    *resultsPtr = results;
    return TCL_OK;
}

static int
MySqlConnect(Tcl_Interp *interp, const char *host, const char *user, 
	     const char *pw, const char *db, unsigned int port,
	     unsigned long flags, MYSQL **cpPtr)			
{
    MYSQL  *cp;			/* Connection handler. */

    cp = mysql_init(NULL); 
    if (cp == NULL) {
	Tcl_AppendResult(interp, "can't initialize mysql connection.",
		(char *)NULL);
	return TCL_ERROR;
    }
    if (host == NULL) {
	host = "localhost";
    }
    cp->reconnect = 1;
#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 32200 /* 3.22 and up */
    if (mysql_real_connect(cp, host, user, pw, db, port, NULL, flags) == NULL) {
	Tcl_AppendResult(interp, "can't connect to mysql server on \"", host, 
			"\": ", mysql_error(cp), (char *)NULL);
	return TCL_ERROR;
    }
#else              /* pre-3.22 */
    if (mysql_real_connect (cp, host, user, pw, port, NULL, flags) == NULL) {
	Tcl_AppendResult(interp, "can't connect to mysql server on \"",
			 host, "\": ", mysql_error(cp), (char *)NULL);
	return TCL_ERROR;
    }
    if (db != NULL) {
	if (mysql_select_db(cp, db) != 0) {
	    Tcl_AppendResult(interp, "can't select database \"", db, "\": ", 
			     mysql_error(cp), (char *)NULL);
            mysql_close(cp);
	    cp = NULL;
	    return TCL_ERROR;
	}
    }
#endif
    *cpPtr = cp;
    return TCL_OK;
}

static int
ImportMysqlProc(Blt_DataTable table, Tcl_Interp *interp, int objc, 
		Tcl_Obj *const *objv)
{
    ImportSwitches switches;
    MYSQL *cp;
    MYSQL_RES *myResults;
    long nCols;
    Blt_DataTableColumn *cols;

    myResults = NULL;
    cp = NULL;
    cols = NULL;
    memset(&switches, 0, sizeof(switches));
    if (Blt_ParseSwitches(interp, importSwitches, objc - 3, objv + 3, 
		&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if (MySqlConnect(interp, switches.host, switches.user, switches.pw,
	switches.db, switches.port, DEF_CLIENT_FLAGS, &cp) != TCL_OK) {
	goto error;
    }
    if (switches.query == NULL) {
	goto done;
    }
    if (MySqlQueryFromObj(interp, cp, switches.query) != TCL_OK) {
	goto error;
    }
    if (MySqlResults(interp, cp, &myResults, &nCols) != TCL_OK) {
	goto error;
    }
    /* Step 1. Create columns to hold the new values.  Label
     *	       the columns using the title. */
    cols = Blt_AssertMalloc(nCols * sizeof(Blt_DataTableColumn));
    if (Blt_DataTable_ExtendColumns(interp, table, nCols, cols) != TCL_OK) {
	goto error;
    }
    if (MySqlImportLabels(interp, table, myResults, nCols, cols) != TCL_OK) {
	goto error;
    }
    if (MySqlImportRows(interp, table, myResults, nCols, cols) != TCL_OK) {
	goto error;
    }
    Blt_Free(cols);
    MySqlFreeResults(myResults);
 done:
    MySqlDisconnect(cp);
    Blt_FreeSwitches(importSwitches, &switches, 0);
    return TCL_OK;
 error:
    if (myResults != NULL) {
	MySqlFreeResults(myResults);
    }
    if (cols != NULL) {
	Blt_Free(cols);
    }
    if (cp != NULL) {
	MySqlDisconnect(cp);
    }
    Blt_FreeSwitches(importSwitches, &switches, 0);
    return TCL_ERROR;
}

#ifdef EXPORT_MYSQL
static int
ExportMysqlProc(Blt_DataTable table, Tcl_Interp *interp, int objc, 
		Tcl_Obj *const *objv)
{
    return TCL_OK;
}
#endif

int 
Blt_DataTable_MysqlInit(Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 1) == NULL) {
	return TCL_ERROR;
    };
#endif
    if (Tcl_PkgRequire(interp, "blt_core", BLT_VERSION, /*Exact*/1) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgProvide(interp, "blt_datatable_mysql", BLT_VERSION) != TCL_OK) { 
	return TCL_ERROR;
    }
    return Blt_DataTable_RegisterFormat(interp,
        "mysql",		/* Name of format. */
	ImportMysqlProc,	/* Import procedure. */
	NULL);			/* Export procedure. */

}
#endif /* HAVE_LIBMYSQL */
#endif /* NO_DATATABLE */

