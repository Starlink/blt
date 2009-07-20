
/*
 *
 * bltDtXml.c --
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
#define USE_NON_const	1
#include <tcl.h>

#include <bltSwitch.h>
#include <bltDataTable.h>
#include <bltTree.h>
#include <bltList.h>
#include <bltAlloc.h>
#include <bltAssert.h>

#ifdef HAVE_STRING_H
#  include <string.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_MEMORY_H
#  include <memory.h>
#endif /* HAVE_MEMORY_H */

DLLEXPORT extern Tcl_AppInitProc Blt_DataTable_XmlInit;

extern const char *Blt_Itoa(int);

#define TRUE 	1
#define FALSE 	0

static Blt_DataTable_ImportProc ImportXmlProc;
static Blt_DataTable_ExportProc ExportXmlProc;

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
 * ImportSwitches --
 */
typedef struct {
    Tcl_Obj *fileObj;	/* Name of file representing the channel. */
    Tcl_Obj *dataObj;
    Tcl_Interp *interp;
    unsigned int flags;
} ImportSwitches;

#define IMPORT_ATTRIBUTES (1<<0)
#define IMPORT_ELEMENTS	  (1<<1)
#define IMPORT_CDATA	  (1<<2)
#define IMPORT_MASK	  (IMPORT_ATTRIBUTES | IMPORT_CDATA | IMPORT_ELEMENTS)

static Blt_SwitchSpec importSwitches[] = 
{
    {BLT_SWITCH_OBJ, "-data", "string", 
         Blt_Offset(ImportSwitches, dataObj), 0, 0},
    {BLT_SWITCH_OBJ, "-file", "fileName",
	Blt_Offset(ImportSwitches, fileObj), 0, 0},
    {BLT_SWITCH_BITMASK_NEG,"-noattrs", "",
	Blt_Offset(ImportSwitches, flags), 0, IMPORT_ATTRIBUTES},
    {BLT_SWITCH_BITMASK_NEG,"-noelems", "",
	Blt_Offset(ImportSwitches, flags), 0, IMPORT_ELEMENTS},
    {BLT_SWITCH_BITMASK_NEG,"-nocdata", "",
	Blt_Offset(ImportSwitches, flags), 0, IMPORT_CDATA},
    {BLT_SWITCH_END}
};

/*
 * ExportSwitches --
 */
typedef struct {
    Blt_DataTableIterator rIter, cIter;
    Tcl_Obj *fileObj;

    /* Private fields. */
    Tcl_Interp *interp;
    unsigned int flags;
    Tcl_Channel channel;	/* If non-NULL, channel to write output to. */
    Tcl_DString *dsPtr;
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
    {BLT_SWITCH_CUSTOM, "-columns", "columns",
	Blt_Offset(ExportSwitches, cIter), 0, 0, &columnIterSwitch},
    {BLT_SWITCH_OBJ, "-file", "fileName",
	Blt_Offset(ExportSwitches, fileObj), 0, 0},
    {BLT_SWITCH_CUSTOM, "-rows", "rows",
	Blt_Offset(ExportSwitches, rIter), 0, 0, &rowIterSwitch},
    {BLT_SWITCH_END}
};

#ifdef HAVE_LIBEXPAT

#include <expat.h>

typedef struct {
    Blt_DataTableRow row;
    Blt_DataTableColumn col;
    Blt_DataTable table;
    Tcl_Interp *interp;
    int flags;
    Blt_HashTable attrTable;
    Blt_HashTable elemTable;
    Blt_HashTable stringTable;
    Blt_List elemList;
    Blt_ListNode node;
} ImportData;

static Tcl_Obj *
GetStringObj(ImportData *importPtr, const char *string, size_t length)
{
    Blt_HashEntry *hPtr;
    int isNew;
    char *key;
#define MAX_STATIC_STRING 1023
    char store[MAX_STATIC_STRING+1];

    if (length > MAX_STATIC_STRING) {
	key = Blt_AssertMalloc(length + 1);
    } else {
	key = store;
    }
    memcpy(key, string, length);
    key[length] = '\0';
    hPtr = Blt_CreateHashEntry(&importPtr->stringTable, key, &isNew);
    if (length > MAX_STATIC_STRING) {
	Blt_Free(key);
    }
    if (isNew) {
	Tcl_Obj *objPtr;

	objPtr = Tcl_NewStringObj(string, length);
	Tcl_IncrRefCount(objPtr);
	Blt_SetHashValue(hPtr, objPtr);
	return objPtr;
    }
    return Blt_GetHashValue(hPtr);
}

static void
DumpStringTable(Blt_HashTable *tablePtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;

    for (hPtr = Blt_FirstHashEntry(tablePtr, &cursor); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&cursor)) {
	Tcl_Obj *objPtr;

	objPtr = Blt_GetHashValue(hPtr);
	Tcl_DecrRefCount(objPtr);
    }
    Blt_DeleteHashTable(tablePtr);
}

static void
GetXmlCharacterData(void *userData, const XML_Char *string, int length) 
{
    ImportData *importPtr = userData;

    assert(length >= 0);
    if (importPtr->flags & IMPORT_CDATA) {
	Tcl_Obj *objPtr;
	Blt_ListNode node;

	/* Replicate the data for each sub-element. */
	objPtr = GetStringObj(importPtr, string, length);
	assert(importPtr->node != NULL);
	Blt_List_SetValue(importPtr->node, objPtr);
	for (node = Blt_List_FirstNode(importPtr->elemList); node != NULL;
	     node = Blt_List_NextNode(node)) {
	    Blt_DataTableColumn col;

	    objPtr = Blt_List_GetValue(node);
	    col = (Blt_DataTableColumn)Blt_List_GetKey(node);
	    if (Blt_DataTable_SetValue(importPtr->table, importPtr->row, col, objPtr) 
		!= TCL_OK) {
		Tcl_BackgroundError(importPtr->interp);
	    }
	}
    }
}

static void
StartXmlTag(void *userData, const char *element, const char **attr) 
{
    Blt_DataTable table;
    Blt_DataTableRow row;
    ImportData *importPtr = userData;
    Tcl_Interp *interp;

    interp = importPtr->interp;
    table = importPtr->table;
    importPtr->node = NULL;
    if (importPtr->flags & IMPORT_ELEMENTS) {
	Blt_DataTableColumn col;
	Blt_HashEntry *hPtr;
	int isNew;

	/* 
	 * If this is the first time we're seeing this element, create a new
	 * column labeled as the element.  This is different than the table's
	 * set of labels, because the table may already have a label by the
	 * same name.  We want to create a new column in this case.
	 */
	hPtr = Blt_CreateHashEntry(&importPtr->elemTable, element, &isNew);
	if (isNew) {
	    col = Blt_DataTable_CreateColumn(interp, table, element);
	    if (col == NULL) {
		goto error;
	    }
	    Blt_SetHashValue(hPtr, col);
	} else {
	    col = Blt_GetHashValue(hPtr);
	}
	importPtr->col = col;
	importPtr->node = Blt_List_Append(importPtr->elemList, (char *)col,NULL);
    }
    if (Blt_DataTable_ExtendRows(interp, table, 1, &row) != TCL_OK) {
	goto error;
    }
    importPtr->row = row;
    if (importPtr->flags & IMPORT_ATTRIBUTES) {
	const char **p;
	
	for (p = attr; *p != NULL; p += 2) {
	    Blt_DataTableColumn col;
	    Blt_HashEntry *hPtr;
	    Tcl_Obj *objPtr;
	    const char *name, *value;
	    int isNew;

	    name = *p, value = *(p+1);
	    /* 
	     * If this is the first time we're seeing this attribute, create a
	     * new column labeled as the attribute.
	     */
	    hPtr = Blt_CreateHashEntry(&importPtr->attrTable, name, &isNew);
	    if (isNew) {
		col = Blt_DataTable_CreateColumn(interp, table, name);
		if (col == NULL) {
		    goto error;
		}
		Blt_SetHashValue(hPtr, col);
	    } else {
		col = Blt_GetHashValue(hPtr);
	    }
	    /* Set the attribute value as the cell value. */
	    objPtr = GetStringObj(importPtr, value, strlen(value));
	    if (Blt_DataTable_SetValue(table, row, col, objPtr) != TCL_OK) {
		goto error;
	    }
	}
    }
    return;
 error:
    Tcl_BackgroundError(importPtr->interp);
}

static void
EndXmlTag(void *userData, const char *element) 
{
    ImportData *importPtr = userData;

    if (importPtr->node != NULL) {
	Blt_ListNode prev;

	/* Pop the tag from the stack. */
	prev = Blt_List_PrevNode(importPtr->node);
	Blt_List_DeleteNode(importPtr->node);
	importPtr->node = prev;
    }
}

static int
ReadXmlFromFile(Tcl_Interp *interp, XML_Parser parser, const char *fileName)
{
    int closeChannel;
    int done;
    Tcl_Channel channel;

    closeChannel = TRUE;
    if ((fileName[0] == '@') && (fileName[1] != '\0')) {
	int mode;
	
	channel = Tcl_GetChannel(interp, fileName+1, &mode);
	if (channel == NULL) {
	    return FALSE;
	}
	if ((mode & TCL_READABLE) == 0) {
	    Tcl_AppendResult(interp, "channel \"", fileName, 
		"\" not opened for reading", (char *)NULL);
	    return FALSE;
	}
	closeChannel = FALSE;
    } else {
	channel = Tcl_OpenFileChannel(interp, fileName, "r", 0);
	if (channel == NULL) {
	    return FALSE;	/* Can't open dump file. */
	}
    }
    done = FALSE;
    while (!done) {
	int length;
#define BUFFSIZE	8192
	char buffer[BUFFSIZE];
	
	length = Tcl_Read(channel, buffer, sizeof(char) * BUFFSIZE);
	if (length < 0) {
	    Tcl_AppendResult(interp, "\nread error: ", Tcl_PosixError(interp),
			     (char *)NULL);
	    if (closeChannel) {
		Tcl_Close(interp, channel);
	    }
	    return FALSE;
	}
	done = Tcl_Eof(channel);
	if (!XML_Parse(parser, buffer, length, done)) {
	    Tcl_AppendResult(interp, "\n", fileName, ":",
			Blt_Itoa(XML_GetCurrentLineNumber(parser)), ": ",
			"error: ", 
			XML_ErrorString(XML_GetErrorCode(parser)), 
			(char *)NULL);
	    if (closeChannel) {
		Tcl_Close(interp, channel);
	    }
	    return FALSE;
	}
    }
    if (closeChannel) {
	Tcl_Close(interp, channel);
    }
    return TRUE;
}

static int
GetXmlExternalEntityRef(XML_Parser parser, const XML_Char *context,
    const XML_Char *base, const XML_Char *systemId, const XML_Char *publicId)
{
    ImportData *dataPtr;
    Tcl_DString dString;
    Tcl_Interp *interp;
    XML_Parser newParser;
    int result;

    dataPtr = XML_GetUserData(parser);
    assert(dataPtr != NULL);
    interp = dataPtr->interp;
    Tcl_DStringInit(&dString);
    if ((base != NULL) && (Tcl_GetPathType(systemId) == TCL_PATH_RELATIVE)) {
	const char **argv;
	const char **baseArr, **sysIdArr;
	int argc;
	int i, j;
	int nBase, nSysId;

	Tcl_SplitPath(base, &nBase, &baseArr);
	Tcl_SplitPath(systemId, &nSysId, &sysIdArr);
	argc = nBase + nSysId;
	argv = Blt_Malloc(sizeof(char *) * (argc + 1));
	if (argv == NULL) {
	    return FALSE;
	}
	for (i = 0; i < nBase; i++) {
	    argv[i] = baseArr[i];
	}
	for (j = 0; j < nSysId; j++, i++) {
	    argv[i] = sysIdArr[j];
	}
	argv[i] = NULL;
	Tcl_JoinPath(argc, argv, &dString);
	Blt_Free(baseArr);
	Blt_Free(sysIdArr);
	Blt_Free(argv);
    } else {
	Tcl_DStringAppend(&dString, systemId, -1);
    }
    newParser = XML_ExternalEntityParserCreate(parser, context, NULL);
    if (newParser == NULL) {
	Tcl_AppendResult(interp, "can't create external entity ref parser", 
			 (char *)NULL);
	return FALSE;
    }
    result = ReadXmlFromFile(interp, newParser, Tcl_DStringValue(&dString));
    Tcl_DStringFree(&dString);
    XML_ParserFree(newParser);
    return result;
}

static int
ImportXmlFile(Tcl_Interp *interp, Blt_DataTable table, Tcl_Obj *fileObjPtr,
	      unsigned int flags) 
{
    ImportData import;
    XML_Parser parser;
    int result;
    char *fileName;

    parser = XML_ParserCreate(NULL);
    if (parser == NULL) {
	Tcl_AppendResult(interp, "can't create XML parser", (char *)NULL);
	return TCL_ERROR;
    }
    import.table = table;
    import.row = NULL;
    import.interp = interp;
    import.flags = flags;
    Blt_InitHashTable(&import.stringTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&import.attrTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&import.elemTable, BLT_STRING_KEYS);
    import.elemList = Blt_List_Create(BLT_ONE_WORD_KEYS);

    XML_SetUserData(parser, &import);

    fileName = Tcl_GetString(fileObjPtr);
    {
	Tcl_DString dString;
	int argc;
	const char **argv;

	Tcl_DStringInit(&dString);
	Tcl_SplitPath(fileName, &argc, &argv);
	Tcl_JoinPath(argc - 1, argv, &dString);
	XML_SetBase(parser, Tcl_DStringValue(&dString));
	Blt_Free(argv);
	Tcl_DStringFree(&dString);
    }
    XML_SetElementHandler(parser, StartXmlTag, EndXmlTag);
    XML_SetCharacterDataHandler(parser, GetXmlCharacterData);
    XML_SetExternalEntityRefHandler(parser, GetXmlExternalEntityRef);
    result = ReadXmlFromFile(interp, parser, fileName);
    XML_ParserFree(parser);
    Blt_DeleteHashTable(&import.attrTable);
    Blt_DeleteHashTable(&import.elemTable);
    DumpStringTable(&import.stringTable);
    Blt_List_Destroy(import.elemList);
    return (result) ? TCL_OK : TCL_ERROR;
} 


static int
ImportXmlData(Tcl_Interp *interp, Blt_DataTable table, Tcl_Obj *dataObjPtr,
	      unsigned int flags) 
{
    ImportData import;
    XML_Parser parser;
    char *string;
    int length;
    int result;

    parser = XML_ParserCreate(NULL);
    if (parser == NULL) {
	Tcl_AppendResult(interp, "can't create parser", (char *)NULL);
	return TCL_ERROR;
    }
    import.table = table;
    import.row = NULL;
    import.interp = interp;
    import.flags = flags;
    Blt_InitHashTable(&import.attrTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&import.elemTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&import.stringTable, BLT_STRING_KEYS);
    import.elemList = Blt_List_Create(BLT_ONE_WORD_KEYS);

    XML_SetUserData(parser, &import);
    XML_SetElementHandler(parser, StartXmlTag, EndXmlTag);
    XML_SetCharacterDataHandler(parser, GetXmlCharacterData);
    string = Tcl_GetStringFromObj(dataObjPtr, &length);
    result = XML_Parse(parser, string, length, 1);
    if (!result) {
	Tcl_AppendResult(interp, "\nparse error at line ",
			 Blt_Itoa(XML_GetCurrentLineNumber(parser)), ":  ",
			 XML_ErrorString(XML_GetErrorCode(parser)),
			 (char *)NULL);
    }
    XML_ParserFree(parser);
    Blt_DeleteHashTable(&import.attrTable);
    Blt_DeleteHashTable(&import.elemTable);
    DumpStringTable(&import.stringTable);
    Blt_List_Destroy(import.elemList);
    return (result) ? TCL_OK : TCL_ERROR;
} 

static int
ImportXmlProc(Blt_DataTable table, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int result;
    ImportSwitches switches;

    memset(&switches, 0, sizeof(switches));
    switches.flags = IMPORT_MASK;
    if (Blt_ParseSwitches(interp, importSwitches, objc - 3, objv + 3, &switches,
	BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    result = TCL_ERROR;
    if ((switches.dataObj != NULL) && (switches.fileObj != NULL)) {
	Tcl_AppendResult(interp, "can't set both -file and -data switches.",
			 (char *)NULL);
	goto error;
    }
    if ((switches.flags & IMPORT_MASK) == 0) {
	Tcl_AppendResult(interp, 
		"can't set both -noelems and -noattrs switches.", (char *)NULL);
	goto error;
    }
    if (switches.fileObj != NULL) {
	result = ImportXmlFile(interp, table, switches.fileObj, switches.flags);
    } else {
	result = ImportXmlData(interp, table, switches.dataObj, switches.flags);
    }
 error:
    Blt_FreeSwitches(importSwitches, (char *)&switches, 0);
    return result;
}

#endif /* HAVE_LIBEXPAT */

static int
XmlFlush(ExportSwitches *exportPtr) 
{
    int nWritten;
    char *line;
    int length;

    line = Tcl_DStringValue(exportPtr->dsPtr);
    length = Tcl_DStringLength(exportPtr->dsPtr);
    nWritten = Tcl_Write(exportPtr->channel, line, length);
    if (nWritten != length) {
	Tcl_AppendResult(exportPtr->interp, "can't write xml element: ",
		Tcl_PosixError(exportPtr->interp), (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_DStringSetLength(exportPtr->dsPtr, 0);
    return TCL_OK;
}

static void
XmlPutEscapeString(const char *from, size_t length, ExportSwitches *exportPtr)
{
    const char *p, *pend;

    for (p = from, pend = from + length; p < pend; /*empty*/) {
	switch (*p) {
	case '\'': 
	    if (p > from) {
		Tcl_DStringAppend(exportPtr->dsPtr, from, p - from);
	    }
	    from = ++p;
	    Tcl_DStringAppend(exportPtr->dsPtr, "&apos;", 6);
	    break;
	case '&':  
	    if (p > from) {
		Tcl_DStringAppend(exportPtr->dsPtr, from, p - from);
	    }
	    from = ++p;
	    Tcl_DStringAppend(exportPtr->dsPtr, "&amp;", 5);
	    break;
	case '>':  
	    if (p > from) {
		Tcl_DStringAppend(exportPtr->dsPtr, from, p - from);
	    }
	    from = ++p;
	    Tcl_DStringAppend(exportPtr->dsPtr, "&gt;", 4);
	    break; 
	case '<':  
	    if (p > from) {
		Tcl_DStringAppend(exportPtr->dsPtr, from, p - from);
	    }
	    from = ++p;
	    Tcl_DStringAppend(exportPtr->dsPtr, "&lt;", 4);
	    break; 
	case '"':  
	    if (p > from) {
		Tcl_DStringAppend(exportPtr->dsPtr, from, p - from);
	    }
	    from = ++p;
	    Tcl_DStringAppend(exportPtr->dsPtr, "&quot;", 6);
	    break;
	default:  
	    p++;
	    break;
	}
    }	
    if (p > from) {
	Tcl_DStringAppend(exportPtr->dsPtr, from, p - from);
    }
}

static int
XmlStartTable(ExportSwitches *exportPtr, const char *tableName)
{
    Tcl_DStringSetLength(exportPtr->dsPtr, 0);
    Tcl_DStringAppend(exportPtr->dsPtr, "<", 1);
    Tcl_DStringAppend(exportPtr->dsPtr, tableName, -1);
    Tcl_DStringAppend(exportPtr->dsPtr, ">\n", 2);
    if (exportPtr->channel != NULL) {
	return XmlFlush(exportPtr);
    }
    return TCL_OK;
}

static int
XmlEndTable(ExportSwitches *exportPtr, const char *tableName)
{
    Tcl_DStringAppend(exportPtr->dsPtr, "</", 2);
    Tcl_DStringAppend(exportPtr->dsPtr, tableName, -1);
    Tcl_DStringAppend(exportPtr->dsPtr, ">\n", 2);
    if (exportPtr->channel != NULL) {
	return XmlFlush(exportPtr);
    }
    return TCL_OK;
}

static void
XmlStartElement(ExportSwitches *exportPtr, const char *elemName)
{
    if (exportPtr->channel != NULL) {
	Tcl_DStringSetLength(exportPtr->dsPtr, 0);
    }
    Tcl_DStringAppend(exportPtr->dsPtr, "  <", 3);
    Tcl_DStringAppend(exportPtr->dsPtr, elemName, -1);
}

static int
XmlEndElement(ExportSwitches *exportPtr)
{
    Tcl_DStringAppend(exportPtr->dsPtr, "/>\n", 3);
    if (exportPtr->channel != NULL) {
	return XmlFlush(exportPtr);
    }
    return TCL_OK;
}

static void
XmlAppendAttrib(ExportSwitches *exportPtr, const char *attrName, 
		const char *value, int length, int type)
{
    size_t valueLen;

    if (length < 0) {
	valueLen = strlen(value);
    } else {
	valueLen = (size_t)length;
    }
    Tcl_DStringAppend(exportPtr->dsPtr, " ", 1);
    Tcl_DStringAppend(exportPtr->dsPtr, attrName, -1);
    Tcl_DStringAppend(exportPtr->dsPtr, "=", 1);
    Tcl_DStringAppend(exportPtr->dsPtr, "\"", 1);
    XmlPutEscapeString(value, valueLen, exportPtr);
    Tcl_DStringAppend(exportPtr->dsPtr, "\"", 1);
}

static int
XmlExport(Blt_DataTable table, ExportSwitches *exportPtr)
{
    Blt_DataTableRow row;

    XmlStartTable(exportPtr, "root");
    for (row = Blt_DataTable_FirstRow(&exportPtr->rIter); row != NULL; 
	 row = Blt_DataTable_NextRow(&exportPtr->rIter)) {
	Blt_DataTableColumn col;
	const char *label;
	    
	XmlStartElement(exportPtr, "row");
	label = Blt_DataTable_RowLabel(row);
	XmlAppendAttrib(exportPtr, "name", label, -1, DT_COLUMN_STRING);
	for (col = Blt_DataTable_FirstColumn(&exportPtr->cIter); col != NULL; 
	     col = Blt_DataTable_NextColumn(&exportPtr->cIter)) {
	    Tcl_Obj *objPtr;

	    label = Blt_DataTable_ColumnLabel(col);
	    objPtr = Blt_DataTable_GetValue(table, row, col);
	    if (objPtr != NULL) {
		int length, type;
		const char *value;

		type = Blt_DataTable_ColumnType(col);
		value = Tcl_GetStringFromObj(objPtr, &length);
		XmlAppendAttrib(exportPtr, label, value, length, type);
	    }
	}
	if (XmlEndElement(exportPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    XmlEndTable(exportPtr, "root");
    return TCL_OK;
}

static int
ExportXmlProc(Blt_DataTable table, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
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
	char *fileName;

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
    result = XmlExport(table, &switches); 
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

int 
Blt_DataTable_XmlInit(Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 1) == NULL) {
	return TCL_ERROR;
    };
#endif
    if (Tcl_PkgRequire(interp, "blt_core", BLT_VERSION, /*Exact*/1) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgProvide(interp, "blt_datatable_xml", BLT_VERSION) != TCL_OK) { 
	return TCL_ERROR;
    }
    return Blt_DataTable_RegisterFormat(interp,
        "xml",			/* Name of format. */
#ifdef HAVE_LIBEXPAT
	ImportXmlProc,		/* Import procedure. */
#else
	NULL,			/* Import procedure. */
#endif /* HAVE_LIBEXPAT */
	ExportXmlProc);		/* Export procedure. */

}

