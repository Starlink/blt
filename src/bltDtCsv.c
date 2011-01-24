
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

DLLEXPORT extern Tcl_AppInitProc Blt_Table_CsvInit;

#define TRUE 	1
#define FALSE 	0

#define EXPORT_ROWLABELS	(1<<0)
#define EXPORT_COLUMNLABELS	(1<<1)

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
    Tcl_Channel channel;		/* If non-NULL, channel to read
					 * from. */
    char *buffer;			/* Buffer to read data into. */
    int nBytes;				/* # of bytes in the buffer. */
    Tcl_DString ds;			/* Dynamic string used to read the
					 * file line by line. */
    Tcl_Interp *interp;
    Blt_HashTable dataTable;
    Tcl_Obj *fileObjPtr;		/* Name of file representing the
					 * channel used as the input
					 * source. */
    Tcl_Obj *dataObjPtr;		/* If non-NULL, data object to use as
					 * input source. */
    const char *quote;			/* Quoted string delimiter. */
    const char *separators;		/* Separator characters. */
    const char *comment;		/* Comment character. */
    int maxRows;			/* Stop processing after this many
					 * rows have been found. */
} ImportSwitches;

static Blt_SwitchSpec importSwitches[] = 
{
    {BLT_SWITCH_STRING, "-comment",     "char",
	Blt_Offset(ImportSwitches, comment), 0},
    {BLT_SWITCH_OBJ,	"-data",      "string",
	Blt_Offset(ImportSwitches, dataObjPtr), 0, 0, NULL},
    {BLT_SWITCH_OBJ,	"-file",      "fileName",
	Blt_Offset(ImportSwitches, fileObjPtr), 0},
    {BLT_SWITCH_INT_NNEG, "-maxrows", "integer",
	Blt_Offset(ImportSwitches, maxRows), 0},
    {BLT_SWITCH_STRING, "-quote",     "char",
	Blt_Offset(ImportSwitches, quote), 0},
    {BLT_SWITCH_STRING, "-separators", "characters",
	Blt_Offset(ImportSwitches, separators), 0},
    {BLT_SWITCH_END}
};

/*
 * ExportSwitches --
 */
typedef struct {
    Blt_TableIterator ri, ci;
    unsigned int flags;
    Tcl_Obj *fileObjPtr;
    Tcl_Channel channel;		/* If non-NULL, channel to write
					 * output to. */
    Tcl_DString *dsPtr;
    int length;				/* Length of dynamic string. */
    int count;				/* # of fields in current record. */
    Tcl_Interp *interp;
    char *quote;			/* Quoted string delimiter. */
    char *separator;			/* Separator character. */
} ExportSwitches;

extern Blt_SwitchFreeProc Blt_Table_ColumnIterFreeProc;
extern Blt_SwitchFreeProc Blt_Table_RowIterFreeProc;
extern Blt_SwitchParseProc Blt_Table_ColumnIterSwitchProc;
extern Blt_SwitchParseProc Blt_Table_RowIterSwitchProc;

static Blt_SwitchCustom columnIterSwitch = {
    Blt_Table_ColumnIterSwitchProc, Blt_Table_ColumnIterFreeProc, 0,
};
static Blt_SwitchCustom rowIterSwitch = {
    Blt_Table_RowIterSwitchProc, Blt_Table_RowIterFreeProc, 0,
};

static Blt_SwitchSpec exportSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-columns",   "columns",
	Blt_Offset(ExportSwitches, ci),   0, 0, &columnIterSwitch},
    {BLT_SWITCH_OBJ,    "-file",      "fileName",
	Blt_Offset(ExportSwitches, fileObjPtr), 0},
    {BLT_SWITCH_BITMASK, "-rowlabels",  "",
        Blt_Offset(ExportSwitches, flags), 0, EXPORT_ROWLABELS},
    {BLT_SWITCH_BITMASK, "-columnlabels",  "",
        Blt_Offset(ExportSwitches, flags), 0, EXPORT_COLUMNLABELS},
    {BLT_SWITCH_STRING, "-quote",     "char",
	Blt_Offset(ExportSwitches, quote),   0},
    {BLT_SWITCH_CUSTOM, "-rows",      "rows",
	Blt_Offset(ExportSwitches, ri),   0, 0, &rowIterSwitch},
    {BLT_SWITCH_STRING, "-separator", "char",
	Blt_Offset(ExportSwitches, separator),     0},
    {BLT_SWITCH_END}
};

static Blt_TableImportProc ImportCsvProc;
static Blt_TableExportProc ExportCsvProc;

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
AppendCsvRecord(ExportSwitches *exportPtr, const char *field, int length, 
		Blt_TableColumnType type)
{
    const char *fp;
    char *p;
    int extra, doQuote;

    doQuote = (type == TABLE_COLUMN_TYPE_STRING);
    extra = 0;
    if (field == NULL) {
	length = 0;
    } else {
	for (fp = field; *fp != '\0'; fp++) {
	    if ((*fp == '\n') || (*fp == exportPtr->separator[0]) || 
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
	Tcl_DStringAppend(exportPtr->dsPtr, exportPtr->separator, 1);
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
ExportCsvRows(Blt_Table table, ExportSwitches *exportPtr)
{
    Blt_TableRow row;

    for (row = Blt_Table_FirstTaggedRow(&exportPtr->ri); row != NULL; 
	 row = Blt_Table_NextTaggedRow(&exportPtr->ri)) {
	Blt_TableColumn col;
	    
	StartCsvRecord(exportPtr);
	if (exportPtr->flags & EXPORT_ROWLABELS) {
	    const char *field;

	    field = Blt_Table_RowLabel(row);
	    AppendCsvRecord(exportPtr, field, -1, TABLE_COLUMN_TYPE_STRING);
	}
	for (col = Blt_Table_FirstTaggedColumn(&exportPtr->ci); col != NULL; 
	     col = Blt_Table_NextTaggedColumn(&exportPtr->ci)) {
	    const char *string;
	    Blt_TableColumnType type;
		
	    type = Blt_Table_ColumnType(col);
	    string = Blt_Table_GetString(table, row, col);
	    AppendCsvRecord(exportPtr, string, -1, type);
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
    if (exportPtr->flags & EXPORT_COLUMNLABELS) {
	Blt_TableColumn col;

	StartCsvRecord(exportPtr);
	if (exportPtr->flags & EXPORT_ROWLABELS) {
	    AppendCsvRecord(exportPtr, "*BLT*", 5, TABLE_COLUMN_TYPE_STRING);
	}
	for (col = Blt_Table_FirstTaggedColumn(&exportPtr->ci); col != NULL; 
	     col = Blt_Table_NextTaggedColumn(&exportPtr->ci)) {
	    AppendCsvRecord(exportPtr, Blt_Table_ColumnLabel(col), -1, 
		TABLE_COLUMN_TYPE_STRING);
	}
	return EndCsvRecord(exportPtr);
    }
    return TCL_OK;
}

/* 
 * $table exportfile fileName ?switches...?
 * $table exportdata ?switches...?
 */
static int
ExportCsvProc(Blt_Table table, Tcl_Interp *interp, int objc, 
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
    switches.separator = Blt_AssertStrdup(",");
    switches.quote = Blt_AssertStrdup("\"");
    rowIterSwitch.clientData = table;
    columnIterSwitch.clientData = table;
    Blt_Table_IterateAllRows(table, &switches.ri);
    Blt_Table_IterateAllColumns(table, &switches.ci);
    if (Blt_ParseSwitches(interp, exportSwitches, objc - 3, objv + 3, &switches,
	BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    result = TCL_ERROR;
    if (switches.fileObjPtr != NULL) {
	const char *fileName;

	closeChannel = TRUE;
	fileName = Tcl_GetString(switches.fileObjPtr);
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

/* 
 * ImportGetLine -- 
 *
 *	Get a single line from the input buffer or file. The resulting buffer
 *	always contains a new line unless an error occurs or we hit EOF.
 *
 */
static int
ImportGetLine(Tcl_Interp *interp, ImportSwitches *importPtr, char **bufferPtr,
		int *nBytesPtr)
{
    if (importPtr->channel != NULL) {
	int nBytes;

	if (Tcl_Eof(importPtr->channel)) {
	    *nBytesPtr = 0;
	    return TCL_OK;
	}
	Tcl_DStringSetLength(&importPtr->ds, 0);
	nBytes = Tcl_Gets(importPtr->channel, &importPtr->ds);
	if (nBytes < 0) {
	    if (Tcl_Eof(importPtr->channel)) {
		*nBytesPtr = 0;
		return TCL_OK;
	    }
	    *nBytesPtr = nBytes;
	    Tcl_AppendResult(interp, "error reading file: ", 
			     Tcl_PosixError(interp), (char *)NULL);
	    return TCL_ERROR;
	}
	Tcl_DStringAppend(&importPtr->ds, "\n", 1);
	*nBytesPtr = nBytes + 1;
	*bufferPtr = Tcl_DStringValue(&importPtr->ds);
    } else {
	const char *bp, *bend;
	int nBytes;

	for (bp = importPtr->buffer, bend = bp + importPtr->nBytes; bp < bend;
	     bp++) {
	    if (*bp == '\n') {
		bp++;
		break;
	    }
	}
	nBytes = bp - importPtr->buffer;
	*nBytesPtr = nBytes;
	*bufferPtr = importPtr->buffer;
	importPtr->nBytes -= nBytes;
	importPtr->buffer += nBytes;
    }
    return TCL_OK;
}

static INLINE int
IsSeparator(ImportSwitches *importPtr, const char c)
{
    const char *p;

    for (p = importPtr->separators; *p != '\0'; p++) {
	if (*p == c) {
	    return TRUE;
	}
    }
    return FALSE;
}

static int
ImportCsv(Tcl_Interp *interp, Blt_Table table, ImportSwitches *importPtr)
{
    Tcl_DString dString;
    char *fp, *field;
    int fieldSize;
    int inQuotes, isQuoted, isPath;
    int result;
    size_t i;
    Blt_TableRow row;
    Blt_TableColumn col;
    int tabIsSeparator;
    const char quote = importPtr->quote[0];
    const char comment = importPtr->comment[0];

    result = TCL_ERROR;
    isPath = isQuoted = inQuotes = FALSE;
    row = NULL;
    i = 1;
    Tcl_DStringInit(&dString);
    fieldSize = 128;
    Tcl_DStringSetLength(&dString, fieldSize + 1);
    fp = field = Tcl_DStringValue(&dString);
    tabIsSeparator = IsSeparator(importPtr, '\t');
    for (;;) {
	char *bp, *bend;
	int nBytes;

	result = ImportGetLine(interp, importPtr, &bp, &nBytes);
	if (result != TCL_OK) {
	    goto error;			/* I/O Error. */
	}
	if (nBytes == 0) {
	    break;			/* EOF */
	}
	bend = bp + nBytes;
	while ((bp < bend) && (isspace(*bp))) {
	    bp++;			/* Skip leading spaces. */
	}
	if ((*bp == '\0') || (*bp == comment)) {
	    continue;			/* Ignore blank or comment lines */
	}
	for (/*empty*/; bp < bend; bp++) {
	    if ((*bp == ' ') || ((*bp == '\t') && (!tabIsSeparator))) {
		/* 
		 * Include whitespace in the field only if it's not leading or
		 * we're inside of quotes or a path.
		 */
		if ((fp != field) || (inQuotes) || (isPath)) {
		    *fp++ = *bp; 
		}
	    } else if (*bp == '\\') {
		/* 
		 * Handle special case CSV files that allow unquoted paths.
		 * Example:  ...,\this\path " should\have been\quoted\,...
		 */
		if (fp == field) {
		    isPath = TRUE; 
		}
		*fp++ = *bp;
	    } else if (*bp == quote) {
		if (inQuotes) {
		    if (*(bp+1) == quote) {
			*fp++ = quote;
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
	    } else if ((IsSeparator(importPtr, *bp)) || (*bp == '\n')) {
		if (inQuotes) {
		    *fp++ = *bp;	/* Copy the comma or newline. */
		} else {
		    Blt_TableColumn col;
		    char *last;

		    if ((isPath) && (IsSeparator(importPtr, *bp)) && 
			(fp != field) && (*(fp - 1) != '\\')) {
			*fp++ = *bp;	/* Copy the comma or newline. */
			goto done;
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
			if ((*bp == '\n') &&  (fp == field)) {
			    goto done;	/* Ignore empty lines. */
			}
			if (Blt_Table_ExtendRows(interp, table, 1, &row) 
			    != TCL_OK) {
			    goto error;
			}
			if ((importPtr->maxRows > 0) && 
			    (Blt_Table_NumRows(table) > importPtr->maxRows)) {
			    bp = bend;
			    goto done;
			}
		    }
		    /* End of field. Append field to row. */
		    if (i > Blt_Table_NumColumns(table)) {
			if (Blt_Table_ExtendColumns(interp, table, 1, &col) 
			    != TCL_OK) {
			    goto error;
			}
		    } else {
			col = Blt_Table_Column(table, i);
		    }
		    if ((last > field) || (isQuoted)) {
			if (Blt_Table_SetString(table, row, col, field, 
				last - field) != TCL_OK) {
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
	    done:
		;
	    } else {
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

		last = fp;
		/* Remove trailing spaces */
		while (isspace(*(last - 1))) {
		    last--;
		}
		if (row == NULL) {
		    if (Blt_Table_ExtendRows(interp, table, 1, &row) 
			!= TCL_OK) {
			goto error;
		    }
		}
		col = Blt_Table_FindColumnByIndex(table, i);
		if (col == NULL) {
		    if (Blt_Table_ExtendColumns(interp, table, 1, &col) 
			!= TCL_OK) {
			goto error;
		    }
		}			
		if ((last > field) || (isQuoted)) {
		    if (Blt_Table_SetString(table, row, col, field, 
			last - field) != TCL_OK) {
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
ImportCsvProc(Blt_Table table, Tcl_Interp *interp, int objc, 
	      Tcl_Obj *const *objv)
{
    int result;
    ImportSwitches switches;

    memset(&switches, 0, sizeof(switches));
    switches.separators = Blt_AssertStrdup(",\t");
    switches.quote = Blt_AssertStrdup("\"");
    switches.comment = Blt_AssertStrdup("");
    Blt_InitHashTable(&switches.dataTable, BLT_STRING_KEYS);
    if (Blt_ParseSwitches(interp, importSwitches, objc - 3 , objv + 3, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    result = TCL_ERROR;
    if ((switches.dataObjPtr != NULL) && (switches.fileObjPtr != NULL)) {
	Tcl_AppendResult(interp, "can't set both -file and -data switches.",
			 (char *)NULL);
	goto error;
    }
    if (switches.dataObjPtr != NULL) {
	int nBytes;

	switches.channel = NULL;
	switches.buffer = Tcl_GetStringFromObj(switches.dataObjPtr, &nBytes);
	switches.nBytes = nBytes;
	switches.fileObjPtr = NULL;
	result = ImportCsv(interp, table, &switches);
    } else {
	int closeChannel;
	Tcl_Channel channel;
	const char *fileName;

	closeChannel = TRUE;
	if (switches.fileObjPtr == NULL) {
	    fileName = "out.csv";
	} else {
	    fileName = Tcl_GetString(switches.fileObjPtr);
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
	Tcl_DStringInit(&switches.ds);
	result = ImportCsv(interp, table, &switches);
	Tcl_DStringFree(&switches.ds);
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
Blt_Table_CsvInit(Tcl_Interp *interp)
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
    return Blt_Table_RegisterFormat(interp,
        "csv",			/* Name of format. */
	ImportCsvProc,		/* Import procedure. */
	ExportCsvProc);		/* Export procedure. */

}
#endif /* NO_DATATABLE */

