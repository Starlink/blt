
/*
 *
 * bltDtCsv.c --
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

#ifndef NO_DATATABLE

#include "config.h"
#include <tcl.h>
#include <bltSwitch.h>
#include <bltHash.h>
#include <bltDataTable.h>
#include <bltAlloc.h>

#ifdef HAVE_CTYPE_H
#  include <ctype.h>
#endif /* HAVE_CTYPE_H */

#ifdef HAVE_MEMORY_H
#  include <memory.h>
#endif /* HAVE_MEMORY_H */

DLLEXPORT extern Tcl_AppInitProc Blt_DataTable_CsvInit;

#define TRUE 	1
#define FALSE 	0

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
 * $table export vector label $vecName label $vecName..
 * $table import xml -file fileName -data dataString ?switches?
 * $table export xml -file fileName -data dataString ?switches?
 * $table import sql -host $host -password $pw -db $db -port $port 
 */

/*
 * ImportSwitches --
 */
typedef struct {
    unsigned int flags;
    Tcl_Channel channel;	/* If non-NULL, channel to read from. */
    Tcl_Obj *fileObj;		/* Name of file representing the channel. */
    Tcl_Obj *dataObj;
    char *buffer;		/* Buffer to read data into. */
    int nBytes;			/* # of bytes in the buffer. */
    Tcl_Interp *interp;
    char *quote;		/* Quoted string delimiter. */
    char *sep;			/* Separator character. */
    Blt_HashTable dataTable;
} ImportSwitches;

static Blt_SwitchSpec importSwitches[] = 
{
    {BLT_SWITCH_OBJ,	"-data",     "string",
	Blt_Offset(ImportSwitches, dataObj), 0, 0, NULL},
    {BLT_SWITCH_OBJ,	"-file",     "fileName",
	Blt_Offset(ImportSwitches, fileObj), 0},
    {BLT_SWITCH_STRING, "-quote",    "char",
	Blt_Offset(ImportSwitches, quote), 0},
    {BLT_SWITCH_STRING, "-separator","char",
	Blt_Offset(ImportSwitches, sep), 0},
    {BLT_SWITCH_END}
};

/*
 * ExportSwitches --
 */
typedef struct {
    Blt_DataTableIterator rIter, cIter;
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

extern Blt_SwitchFreeProc Blt_DataTable_ColumnIterFreeProc;
extern Blt_SwitchFreeProc Blt_DataTable_RowIterFreeProc;
extern Blt_SwitchParseProc Blt_DataTable_ColumnIterSwitchProc;
extern Blt_SwitchParseProc Blt_DataTable_RowIterSwitchProc;

static Blt_SwitchCustom columnIterSwitch = {
    Blt_DataTable_ColumnIterSwitchProc, Blt_DataTable_ColumnIterFreeProc, 0,
};
static Blt_SwitchCustom rowIterSwitch = {
    Blt_DataTable_RowIterSwitchProc, Blt_DataTable_RowIterFreeProc, 0,
};

static Blt_SwitchSpec exportSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-columns",   "columns",
	Blt_Offset(ExportSwitches, cIter),   0, 0, &columnIterSwitch},
    {BLT_SWITCH_OBJ,    "-file",      "fileName",
	Blt_Offset(ExportSwitches, fileObj), 0},
    {BLT_SWITCH_STRING, "-quote",     "char",
	Blt_Offset(ExportSwitches, quote),   0},
    {BLT_SWITCH_CUSTOM, "-rows",      "rows",
	Blt_Offset(ExportSwitches, rIter),   0, 0, &rowIterSwitch},
    {BLT_SWITCH_STRING, "-separator", "char",
	Blt_Offset(ExportSwitches, sep),     0},
    {BLT_SWITCH_END}
};

static Blt_DataTable_ImportProc ImportCsvProc;
static Blt_DataTable_ExportProc ExportCsvProc;

static void
StartCsvRecord(ExportSwitches *exportPtr)
{
    if (exportPtr->channel != NULL) {
	Tcl_DStringSetLength(exportPtr->dsPtr, 0);
	exportPtr->length = 0;
    }
    exportPtr->count = 0;
}

static int
EndCsvRecord(ExportSwitches *exportPtr)
{
    int nWritten;
    char *line;

    Tcl_DStringAppend(exportPtr->dsPtr, "\n", 1);
    exportPtr->length++;
    line = Tcl_DStringValue(exportPtr->dsPtr);
    if (exportPtr->channel != NULL) {
	nWritten = Tcl_Write(exportPtr->channel, line, exportPtr->length);
	if (nWritten != exportPtr->length) {
	    Tcl_AppendResult(exportPtr->interp, "can't write csv record: ",
			     Tcl_PosixError(exportPtr->interp), (char *)NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

static void
AppendCsvRecord(
    ExportSwitches *exportPtr, 
    const char *field, 
    int length, 
    int type)
{
    const char *fp;
    char *p;
    int extra, doQuote;

    doQuote = ((type == DT_COLUMN_STRING) || (type == DT_COLUMN_UNKNOWN));
    extra = 0;
    if (field == NULL) {
	length = 0;
    } else {
	for (fp = field; *fp != '\0'; fp++) {
	    if ((*fp == '\n') || (*fp == exportPtr->sep[0]) || 
		(*fp == ' ') || (*fp == '\t')) {
		doQuote = TRUE;
	    } else if (*fp == exportPtr->quote[0]) {
		doQuote = TRUE;
		extra++;
	    }
	}
	if (doQuote) {
	    extra += 2;
	}
	if (length < 0) {
	    length = fp - field;
	}
    }
    if (exportPtr->count > 0) {
	Tcl_DStringAppend(exportPtr->dsPtr, exportPtr->sep, 1);
	exportPtr->length++;
    }
    length = length + extra + exportPtr->length;
    Tcl_DStringSetLength(exportPtr->dsPtr, length);
    p = Tcl_DStringValue(exportPtr->dsPtr) + exportPtr->length;
    exportPtr->length = length;
    if (field != NULL) {
	if (doQuote) {
	    *p++ = exportPtr->quote[0];
	}
	for (fp = field; *fp != '\0'; fp++) {
	    if (*fp == exportPtr->quote[0]) {
		*p++ = exportPtr->quote[0];
	    }
	    *p++ = *fp;
	}
	if (doQuote) {
	    *p++ = exportPtr->quote[0];
	}
    }
    exportPtr->count++;
}

static int
ExportCsvRows(Blt_DataTable table, ExportSwitches *exportPtr)
{
    Blt_DataTableRow row;

    for (row = Blt_DataTable_FirstRow(&exportPtr->rIter); row != NULL; 
	 row = Blt_DataTable_NextRow(&exportPtr->rIter)) {
	Blt_DataTableColumn col;
	const char *field;
	    
	StartCsvRecord(exportPtr);
	field = Blt_DataTable_RowLabel(row);
	AppendCsvRecord(exportPtr, field, -1, 0);
	for (col = Blt_DataTable_FirstColumn(&exportPtr->cIter); col != NULL; 
	     col = Blt_DataTable_NextColumn(&exportPtr->cIter)) {
	    Tcl_Obj *objPtr;
	    
	    objPtr = Blt_DataTable_GetValue(table, row, col);
	    if (objPtr == NULL) {
		AppendCsvRecord(exportPtr, NULL, -1, 0);
	    } else {
		int length;
		int type;
		
		type = Blt_DataTable_ColumnType(col);
		field = Tcl_GetStringFromObj(objPtr, &length);
		AppendCsvRecord(exportPtr, field, length, type);
	    }
	}
	if (EndCsvRecord(exportPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

static int
ExportCsvColumns(ExportSwitches *exportPtr)
{
    Blt_DataTableColumn col;

    StartCsvRecord(exportPtr);
    AppendCsvRecord(exportPtr, "*BLT*", 5, 0);
    for (col = Blt_DataTable_FirstColumn(&exportPtr->cIter); col != NULL; 
	 col = Blt_DataTable_NextColumn(&exportPtr->cIter)) {
	AppendCsvRecord(exportPtr, Blt_DataTable_ColumnLabel(col), -1, 0);
    }
    return EndCsvRecord(exportPtr);
}

/* 
 * $table exportfile fileName ?switches...?
 * $table exportdata ?switches...?
 */
static int
ExportCsvProc(
    Blt_DataTable table, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const *objv)
{
    ExportSwitches switches;
    Tcl_Channel channel;
    Tcl_DString ds;
    int closeChannel;
    int result;

    closeChannel = FALSE;
    channel = NULL;

    Tcl_DStringInit(&ds);
    memset(&switches, 0, sizeof(switches));
    switches.sep = Blt_AssertStrdup(",");
    switches.quote = Blt_AssertStrdup("\"");
    rowIterSwitch.clientData = table;
    columnIterSwitch.clientData = table;
    Blt_DataTable_IterateAllRows(table, &switches.rIter);
    Blt_DataTable_IterateAllColumns(table, &switches.cIter);
    if (Blt_ParseSwitches(interp, exportSwitches, objc - 3, objv + 3, &switches,
	BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    result = TCL_ERROR;
    if (switches.fileObj != NULL) {
	const char *fileName;

	closeChannel = TRUE;
	fileName = Tcl_GetString(switches.fileObj);
	if ((fileName[0] == '@') && (fileName[1] != '\0')) {
	    int mode;
	    
	    channel = Tcl_GetChannel(interp, fileName+1, &mode);
	    if (channel == NULL) {
		goto error;
	    }
	    if ((mode & TCL_WRITABLE) == 0) {
		Tcl_AppendResult(interp, "channel \"", fileName, 
				 "\" not opened for writing", (char *)NULL);
		goto error;
	    }
	    closeChannel = FALSE;
	} else {
	    channel = Tcl_OpenFileChannel(interp, fileName, "w", 0666);
	    if (channel == NULL) {
		goto error;	/* Can't open export file. */
	    }
	}
    }
    switches.interp = interp;
    switches.dsPtr = &ds;
    switches.channel = channel;
    result = ExportCsvColumns(&switches); 
    if (result == TCL_OK) {
	result = ExportCsvRows(table, &switches);
    }
    if ((switches.channel == NULL) && (result == TCL_OK)) {
	Tcl_DStringResult(interp, &ds);
    } 
 error:
    Tcl_DStringFree(&ds);
    if (closeChannel) {
	Tcl_Close(interp, channel);
    }
    Blt_FreeSwitches(exportSwitches, (char *)&switches, 0);
    return result;
}

static Tcl_Obj *
GetStringObj(ImportSwitches *importPtr, const char *string, size_t length)
{
    Blt_HashEntry *hPtr;
    int isNew;

    hPtr = Blt_CreateHashEntry(&importPtr->dataTable, string, &isNew);
    if (isNew) {
	Tcl_Obj *objPtr;

	objPtr = Tcl_NewStringObj(string, length);
	Blt_SetHashValue(hPtr, objPtr);
	return objPtr;
    }
    return Blt_GetHashValue(hPtr);
}

static int
ImportGetBuffer(
    Tcl_Interp *interp, 
    ImportSwitches *importPtr, 
    char **bufferPtr,
    int *nBytesPtr)
{
    if (importPtr->channel != NULL) {
	int nBytes;

	if (Tcl_Eof(importPtr->channel)) {
	    *nBytesPtr = -1;
	    return TCL_OK;
	}
#define BUFFSIZE	8191
	nBytes = Tcl_Read(importPtr->channel, importPtr->buffer, 
		sizeof(char) * BUFFSIZE);
	*nBytesPtr = nBytes;
	if (nBytes < 0) {
	    Tcl_AppendResult(interp, "error reading file: ", 
			     Tcl_PosixError(interp), (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	*nBytesPtr =  importPtr->nBytes;
	importPtr->nBytes = -1;
    }
    *bufferPtr = importPtr->buffer;
    return TCL_OK;
}

static int
ImportCsv(Tcl_Interp *interp, Blt_DataTable table, ImportSwitches *importPtr)
{
    Tcl_DString dString;
    char *fp, *field;
    int fieldSize;
    int inQuotes, isQuoted, isPath;
    int result;
    size_t i;
    Blt_DataTableRow row;
    Blt_DataTableColumn col;

    result = TCL_ERROR;
    isPath = isQuoted = inQuotes = FALSE;
    row = NULL;
    i = 1;
    Tcl_DStringInit(&dString);
    fieldSize = 128;
    Tcl_DStringSetLength(&dString, fieldSize + 1);
    fp = field = Tcl_DStringValue(&dString);
    for (;;) {
	char *bp, *bend;
	int nBytes;

	result = ImportGetBuffer(interp, importPtr, &bp, &nBytes);
	if (result != TCL_OK) {
	    goto error;		/* I/O Error. */
	}
	if (nBytes < 0) {
	    break;		/* Eof */
	}
	for (bend = bp + nBytes; bp < bend; bp++) {
	    switch (*bp) {
	    case '\t':
	    case ' ':
		/* 
		 * Add whitespace only if it's not leading or we're inside of
		 * quotes or a path.
		 */
		if ((fp != field) || (inQuotes) || (isPath)) {
		    *fp++ = *bp; 
		}
		break;

	    case '\\':
		/* 
		 * Handle special case CSV files that allow unquoted paths.
		 * Example:  ...,\this\path " should\have been\quoted\,...
		 */
		if (fp == field) {
		    isPath = TRUE; 
		}
		*fp++ = *bp;
		break;

	    case '"':
		if (inQuotes) {
		    if (*(bp+1) == '"') {
			*fp++ = '"';
			bp++;
		    } else {
			inQuotes = FALSE;
		    }
		} else {
		    /* 
		     * If the quote doesn't start a field, then treat all
		     * quotes in the field as ordinary characters.
		     */
		    if (fp == field) {
			isQuoted = inQuotes = TRUE; 
		    } else {
			*fp++ = *bp;
		    }
		}
		break;

	    case ',':
	    case '\n':
		if (inQuotes) {
		    *fp++ = *bp; /* Copy the comma or newline. */
		} else {
		    Blt_DataTableColumn col;
		    char *last;
		    Tcl_Obj *objPtr;

		    if ((isPath) && (*bp == ',') && (fp != field) && 
			(*(fp - 1) != '\\')) {
			*fp++ = *bp; /* Copy the comma or newline. */
			break;
		    }    
		    /* "last" points to the character after the last character
		     * in the field. */
		    last = fp;	

		    /* Remove trailing spaces only if the field wasn't
		     * quoted. */
		    if ((!isQuoted) && (!isPath)) {
			while ((last > field) && (isspace(*(last - 1)))) {
			    last--;
			}
		    }
		    if (row == NULL) {
			if (*bp == '\n') {
			    break; /* Ignore empty lines. */
			}
			if (Blt_DataTable_ExtendRows(interp, table, 1, &row) 
			    != TCL_OK) {
			    goto error;
			}
		    }
		    /* End of field. Append field to row. */
		    col = Blt_DataTable_GetColumnByIndex(table, i);
		    if (col == NULL) {
			if (Blt_DataTable_ExtendColumns(interp, table, 1, &col) 
			    != TCL_OK) {
			    goto error;
			}
		    }			
		    if ((last > field) || (isQuoted)) {
			int result;

			objPtr = GetStringObj(importPtr, field, last - field);
			Tcl_IncrRefCount(objPtr);
			result = Blt_DataTable_SetValue(table, row, col, objPtr);
			Tcl_DecrRefCount(objPtr);
			if (result != TCL_OK) {
			    goto error;
			}
		    }
		    i++;
		    if (*bp == '\n') {
			row = NULL;
			i = 1;
		    }
		    fp = field;
		    isPath = isQuoted = FALSE;
		}
		break;

	    default:
		*fp++ = *bp;	/* Copy the character. */
	    }
	    if ((fp - field) >= fieldSize) {
		int offset;

		/* 
		 * We've exceeded the current maximum size of the field.
		 * Double the size of the field, but make sure to reset the
		 * pointers to the (possibly) new memory location.
		 */
		offset = fp - field;
		fieldSize += fieldSize;
		Tcl_DStringSetLength(&dString, fieldSize + 1);
		field = Tcl_DStringValue(&dString);
		fp = field + offset;
	    }
	}
	if (nBytes < 1) {
	    /* 
	     * We're reached the end of input. But there may not have been a
	     * final newline to trigger the final append. So check if a last
	     * field is still needs appending the the last row.
	     */
	    if (fp != field) {
		char *last;
		Tcl_Obj *objPtr;

		last = fp;
		/* Remove trailing spaces */
		while (isspace(*(last - 1))) {
		    last--;
		}
		if (row == NULL) {
		    if (Blt_DataTable_ExtendRows(interp, table, 1, &row) != TCL_OK) {
			goto error;
		    }
		}
		col = Blt_DataTable_GetColumnByIndex(table, i);
		if (col == NULL) {
		    if (Blt_DataTable_ExtendColumns(interp, table, 1, &col)!= TCL_OK) {
			goto error;
		    }
		}			
		if ((last > field) || (isQuoted)) {
		    int result;

		    objPtr = GetStringObj(importPtr, field, last - field);
		    Tcl_IncrRefCount(objPtr);
		    result = Blt_DataTable_SetValue(table, row, col, objPtr);
		    Tcl_DecrRefCount(objPtr);
		    if (result != TCL_OK) {
			goto error;
		    }
		}		
	    }    
	    break;
	}
    }
    result = TCL_OK;
 error:
    Tcl_DStringFree(&dString);
    return result;
}

static int
ImportCsvProc(Blt_DataTable table, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int result;
    ImportSwitches switches;

    memset(&switches, 0, sizeof(switches));
    switches.sep = Blt_AssertStrdup(",");
    switches.quote = Blt_AssertStrdup("\"");
    Blt_InitHashTable(&switches.dataTable, BLT_STRING_KEYS);
    if (Blt_ParseSwitches(interp, importSwitches, objc - 3 , objv + 3, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    result = TCL_ERROR;
    if ((switches.dataObj != NULL) && (switches.fileObj != NULL)) {
	Tcl_AppendResult(interp, "can't set both -file and -data switches.",
			 (char *)NULL);
	goto error;
    }
    if (switches.dataObj != NULL) {
	int nBytes;

	switches.channel = NULL;
	switches.buffer = Tcl_GetStringFromObj(switches.dataObj, &nBytes);
	switches.nBytes = nBytes;
	switches.fileObj = NULL;
	result = ImportCsv(interp, table, &switches);
    } else {
	int closeChannel;
	Tcl_Channel channel;
	const char *fileName;
	char buffer[BUFFSIZE+1];

	closeChannel = TRUE;
	if (switches.fileObj == NULL) {
	    fileName = "out.csv";
	} else {
	    fileName = Tcl_GetString(switches.fileObj);
	}
	if ((fileName[0] == '@') && (fileName[1] != '\0')) {
	    int mode;
	    
	    channel = Tcl_GetChannel(interp, fileName+1, &mode);
	    if (channel == NULL) {
		goto error;
	    }
	    if ((mode & TCL_READABLE) == 0) {
		Tcl_AppendResult(interp, "channel \"", fileName, 
				 "\" not opened for reading", (char *)NULL);
		goto error;
	    }
	    closeChannel = FALSE;
	} else {
	    channel = Tcl_OpenFileChannel(interp, fileName, "r", 0);
	    if (channel == NULL) {
		goto error;
	    }
	}
	switches.channel = channel;
	switches.buffer = buffer;
	result = ImportCsv(interp, table, &switches);
	if (closeChannel) {
	    Tcl_Close(interp, channel);
	}
    }
 error:
    Blt_FreeSwitches(importSwitches, (char *)&switches, 0);
    Blt_DeleteHashTable(&switches.dataTable);
    return result;
}
    
int 
Blt_DataTable_CsvInit(Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 1) == NULL) {
	return TCL_ERROR;
    };
#endif
    if (Tcl_PkgRequire(interp, "blt_core", BLT_VERSION, /*Exact*/1) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgProvide(interp, "blt_datatable_csv", BLT_VERSION) != TCL_OK) { 
	return TCL_ERROR;
    }
    return Blt_DataTable_RegisterFormat(interp,
        "csv",			/* Name of format. */
	ImportCsvProc,		/* Import procedure. */
	ExportCsvProc);		/* Export procedure. */

}
#endif /* NO_DATATABLE */

