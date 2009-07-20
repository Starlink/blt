
/*
 * bltDataTable.h --
 *
 *	Copyright 1998-2004 George A Howlett.
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

#ifndef _BLT_DATATABLE_H
#define _BLT_DATATABLE_H

#include <bltChain.h>
#include <bltHash.h>

typedef struct _Blt_DataTableTags *Blt_DataTableTags;

typedef struct _Blt_DataTableHeader {
    const char *label;		/* Label of row or column. */
    long index;			/* Reverse lookup offset-to-index. */
    long offset;
    unsigned int flags;
} *Blt_DataTableHeader;

typedef struct _Blt_DataTableRow {
    const char *label;		/* Label of row or column. */
    long index;			/* Reverse lookup offset-to-index. */
    long offset;
    unsigned int flags;
} *Blt_DataTableRow;

typedef struct _Blt_DataTableColumn {
    char *label;		/* Label of row or column. */
    long index;			/* Reverse lookup offset-to-index. */
    long offset;
    unsigned short flags;
    unsigned short type;
} *Blt_DataTableColumn;

typedef struct {
    const char *name;
    long headerSize;
} Blt_DataTableRowColumnClass;

/*
 * Blt_DataTableRowColumn --
 *
 *	Structure representing a row or column in the table. 
 */
typedef struct _Blt_DataTableRowColumn {
    Blt_DataTableRowColumnClass *classPtr;
    Blt_Pool headerPool;

    long nAllocated;		/* Length of allocated array below. May exceed
				 * the number of row or column headers
				 * used. */
    long nUsed;

    Blt_DataTableHeader *map;	/* Array of row or column headers. */
    
    Blt_Chain freeList;		/* Tracks free row or column headers.  */

    Blt_HashTable labels;	/* Hash table of labels. Maps labels to table
				 * offsets. */

    long nextId;		/* Used to generate default labels. */

} Blt_DataTableRowColumn;

/*
 * Blt_DataTableCore --
 *
 *	Structure representing a table object. 
 *
 *	The table object is an array of column vectors. Each vector is an
 *	array of Tcl_Obj pointers, representing the data for the column.
 *	Empty row entries are designated by NULL values.  Column vectors are
 *	allocated when needed.  Every column in the table has the same length.
 *
 *	Rows and columns are indexed by a map of pointers to headers.  This
 *	map represents the order of the rows or columns.  A table object can
 *	be shared by several clients.  When a client wants to use a table
 *	object, it is given a token that represents the table.  The object
 *	tracks its clients by its token. When all clients have released their
 *	tokens, the tuple object is automatically destroyed.
 */
typedef struct _Blt_DataTableCore {
    Blt_DataTableRowColumn rows, columns;

    Tcl_Obj ***data;		/* Array of column vector pointers */

    unsigned int flags;		/* Internal flags. See definitions below. */

    Blt_Chain clients;		/* List of clients using this table */

    unsigned long mtime, ctime;

    unsigned int notifyFlags;	/* Notification flags. See definitions
				 * below. */
    int notifyHold;
    
} Blt_DataTableCore;

/*
 * Blt_DataTable --
 *
 *	A client is uniquely identified by a combination of its name and the
 *	originating namespace.  Two table objects in the same interpreter can
 *	have similar names but must reside in different namespaces.
 *
 *	Two or more clients can share the same table object.  Each client
 *	structure which acts as a ticket for the underlying table object.
 *	Clients can designate notifier routines that are automatically invoked
 *	by the table object whenever the table is changed is specific ways by
 *	other clients.
 */
typedef struct _Blt_DataTable {
    unsigned int magic;		/* Magic value indicating whether a generic
				 * pointer is really a datatable token or
				 * not. */
    const char *name;		/* Fully namespace-qualified name of the
				 * client. */
    const char *emptyValue;

    Blt_DataTableCore *corePtr;	/* Pointer to the structure containing the
				 * master information about the table used by
				 * the client.  If NULL, this indicates that
				 * the table has been destroyed (but as of
				 * yet, this client hasn't recognized it). */
    Tcl_Interp *interp;

    Blt_HashTable *tablePtr;	/* Interpreter-specific global hash table of
				 * all datatable clients. Each entry is a
				 * chain of clients that are sharing the same
				 * core datatable. */

    Blt_HashEntry *hPtr;	/* This client's entry in the above hash
				 * table. This is a list of clients that all
				 * using the core datatable. */

    Blt_ChainLink link2;	/* This client's entry in the list found in
				 * the above list (hashtable entry). */

    Blt_ChainLink link;		/* Pointer into the server's chain of
				 * clients. */

    Blt_HashTable *rowTags;
    Blt_HashTable *columnTags;

    Blt_Chain traces;		/* Chain of traces. */
    Blt_Chain columnNotifiers;	/* Chain of event handlers. */
    Blt_Chain rowNotifiers;	/* Chain of event handlers. */
    Blt_DataTableTags tags;

    Blt_HashTable *keyTables;	/* Array of primary keys. */
    long nKeys;			/* Length of the above array. */

    Blt_DataTableRow *masterKey;	/* Master key entry. */
    Blt_HashTable masterKeyTable;
    Blt_Chain primaryKeys;
    unsigned int flags;
} *Blt_DataTable;

BLT_EXTERN void Blt_DataTable_ReleaseTags(Blt_DataTable table);

BLT_EXTERN int Blt_DataTable_TableExists(Tcl_Interp *interp, const char *name);
BLT_EXTERN int Blt_DataTable_CreateTable(Tcl_Interp *interp, const char *name, 
	Blt_DataTable *tablePtr);
BLT_EXTERN int Blt_DataTable_Open(Tcl_Interp *interp, const char *name, 
	Blt_DataTable *tablePtr);
BLT_EXTERN void Blt_DataTable_Close(Blt_DataTable table);

BLT_EXTERN int Blt_DataTable_SameTableObject(Blt_DataTable table1, 
	Blt_DataTable table2);

BLT_EXTERN const char *Blt_DataTable_Name(Blt_DataTable table);

BLT_EXTERN Blt_DataTableRow Blt_DataTable_GetRowByLabel(Blt_DataTable table, 
	const char *label);
BLT_EXTERN Blt_DataTableColumn Blt_DataTable_GetColumnByLabel(
	Blt_DataTable table, const char *label);

BLT_EXTERN Blt_DataTableRow Blt_DataTable_GetRowByIndex(Blt_DataTable table, 
	long index);
BLT_EXTERN Blt_DataTableColumn Blt_DataTable_GetColumnByIndex(
	Blt_DataTable table, long index);

BLT_EXTERN int Blt_DataTable_SetRowLabel(Tcl_Interp *interp, 
	Blt_DataTable table, Blt_DataTableRow row, const char *label);
BLT_EXTERN int Blt_DataTable_SetColumnLabel(Tcl_Interp *interp, 
	Blt_DataTable table, Blt_DataTableColumn column, const char *label);

BLT_EXTERN int Blt_DataTable_ParseColumnType(const char *typeName);

BLT_EXTERN const char *Blt_DataTable_NameOfColumnType(int type);


BLT_EXTERN int Blt_DataTable_GetColumnType(Blt_DataTableColumn column);
BLT_EXTERN int Blt_DataTable_SetColumnType(Blt_DataTableColumn column, 
	int type);

BLT_EXTERN int Blt_DataTable_SetColumnTag(Tcl_Interp *interp, 
	Blt_DataTable table, Blt_DataTableColumn column, const char *tagName);
BLT_EXTERN int Blt_DataTable_SetRowTag(Tcl_Interp *interp, Blt_DataTable table, 
	Blt_DataTableRow row, const char *tagName);

BLT_EXTERN Blt_DataTableRow Blt_DataTable_CreateRow(Tcl_Interp *interp, 
	Blt_DataTable table, const char *label);
BLT_EXTERN Blt_DataTableColumn Blt_DataTable_CreateColumn(Tcl_Interp *interp, 
	Blt_DataTable table, const char *label);

BLT_EXTERN int Blt_DataTable_ExtendRows(Tcl_Interp *interp, Blt_DataTable table,
	size_t n, Blt_DataTableRow *rows);
BLT_EXTERN int Blt_DataTable_ExtendColumns(Tcl_Interp *interp, 
	Blt_DataTable table, size_t n, Blt_DataTableColumn *columms);
BLT_EXTERN int Blt_DataTable_DeleteRow(Blt_DataTable table, 
	Blt_DataTableRow row);
BLT_EXTERN int Blt_DataTable_DeleteColumn(Blt_DataTable table, 
	Blt_DataTableColumn column);
BLT_EXTERN int Blt_DataTable_MoveRows(Tcl_Interp *interp, Blt_DataTable table, 
	Blt_DataTableRow from, Blt_DataTableRow to, size_t n);
BLT_EXTERN int Blt_DataTable_MoveColumns(Tcl_Interp *interp, 
	Blt_DataTable table, Blt_DataTableColumn from,  Blt_DataTableColumn to,
	size_t n);

BLT_EXTERN Tcl_Obj *Blt_DataTable_GetValue(Blt_DataTable table, 
	Blt_DataTableRow row, Blt_DataTableColumn col);
BLT_EXTERN int Blt_DataTable_SetValue(Blt_DataTable table, Blt_DataTableRow row,
	Blt_DataTableColumn column, Tcl_Obj *objPtr);
BLT_EXTERN int Blt_DataTable_UnsetValue(Blt_DataTable table, 
	Blt_DataTableRow row, Blt_DataTableColumn column);
BLT_EXTERN int Blt_DataTable_ValueExists(Blt_DataTable table, 
	Blt_DataTableRow row, Blt_DataTableColumn column);
BLT_EXTERN int Blt_DataTable_GetArrayValue(Blt_DataTable table, 
	Blt_DataTableRow row, Blt_DataTableColumn column, const char *key, 
	Tcl_Obj **objPtrPtr);
BLT_EXTERN int Blt_DataTable_SetArrayValue(Blt_DataTable table, 
	Blt_DataTableRow row, Blt_DataTableColumn column, const char *key, 
	Tcl_Obj *objPtr);
BLT_EXTERN int Blt_DataTable_UnsetArrayValue(Blt_DataTable table, 
	Blt_DataTableRow row, Blt_DataTableColumn column, const char *key);
BLT_EXTERN int Blt_DataTable_ArrayValueExists(Blt_DataTable table, 
	Blt_DataTableRow row, Blt_DataTableColumn column, const char *key);
BLT_EXTERN Tcl_Obj *Blt_DataTable_ArrayNames(Blt_DataTable table, 
	Blt_DataTableRow row, Blt_DataTableColumn column);

BLT_EXTERN Blt_HashTable *Blt_DataTable_GetRowTagTable(Blt_DataTable table, 
	const char *tagName);
BLT_EXTERN Blt_HashTable *Blt_DataTable_GetColumnTagTable(Blt_DataTable table, 
	const char *tagName);
BLT_EXTERN Blt_Chain Blt_DataTable_GetRowTags(Blt_DataTable table, 
	Blt_DataTableRow row);
BLT_EXTERN Blt_Chain Blt_DataTable_GetColumnTags(Blt_DataTable table, 
	Blt_DataTableColumn column);

BLT_EXTERN long Blt_DataTable_GetNumColumns(Blt_DataTable table);
BLT_EXTERN Blt_Chain Blt_DataTable_Traces(Blt_DataTable table);
BLT_EXTERN int Blt_DataTable_TagsAreShared(Blt_DataTable table);

BLT_EXTERN int Blt_DataTable_HasRowTag(Blt_DataTable table, 
	Blt_DataTableRow row, const char *tagName);
BLT_EXTERN int Blt_DataTable_HasColumnTag(Blt_DataTable table, 
	Blt_DataTableColumn column, const char *tagName);
BLT_EXTERN void Blt_DataTable_AddColumnTag(Blt_DataTable table, 
	Blt_DataTableColumn column, const char *tagName);
BLT_EXTERN void Blt_DataTable_AddRowTag(Blt_DataTable table, 
	Blt_DataTableRow row, const char *tagName);
BLT_EXTERN int Blt_DataTable_ForgetRowTag(Tcl_Interp *interp, 
	Blt_DataTable table, const char *tagName);
BLT_EXTERN int Blt_DataTable_ForgetColumnTag(Tcl_Interp *interp, 
	Blt_DataTable table, const char *tagName);
BLT_EXTERN int Blt_DataTable_UnsetRowTag(Tcl_Interp *interp, 
	Blt_DataTable table, Blt_DataTableRow row, const char *tagName);
BLT_EXTERN int Blt_DataTable_UnsetColumnTag(Tcl_Interp *interp, 
	Blt_DataTable table, Blt_DataTableColumn column, const char *tagName);
BLT_EXTERN Blt_HashEntry *Blt_DataTable_FirstRowTag(Blt_DataTable table, 
	Blt_HashSearch *cursorPtr);
BLT_EXTERN Blt_HashEntry *Blt_DataTable_FirstColumnTag(Blt_DataTable table, 
	Blt_HashSearch *cursorPtr);

BLT_EXTERN Blt_HashTable *Blt_DataTable_GetTagHashTable(Blt_DataTable table, 
	const char *tagName);

BLT_EXTERN int Blt_DataTable_TagTableIsShared(Blt_DataTable table);

#define DT_COLUMN_UNKNOWN	(0) 
#define DT_COLUMN_STRING	(1<<0)
#define DT_COLUMN_INTEGER	(1<<1)
#define DT_COLUMN_DOUBLE	(1<<2)
#define DT_COLUMN_BINARY	(1<<3)
#define DT_COLUMN_IMAGE		(1<<4)
#define DT_COLUMN_MASK		(DT_COLUMN_INTEGER | DT_COLUMN_BINARY | \
				 DT_COLUMN_DOUBLE | DT_COLUMN_STRING | \
				 DT_COLUMN_IMAGE)

typedef enum { 
    DT_SPEC_UNKNOWN, DT_SPEC_INDEX, DT_SPEC_RANGE, DT_SPEC_LABEL, DT_SPEC_TAG, 
} Blt_DataTableRowColumnSpec;

BLT_EXTERN Blt_DataTableRowColumnSpec Blt_DataTable_GetRowSpec(
	Blt_DataTable table, Tcl_Obj *objPtr, const char **stringPtr);
BLT_EXTERN Blt_DataTableRowColumnSpec Blt_DataTable_GetColumnSpec(
	Blt_DataTable table, Tcl_Obj *objPtr, const char **stringPtr);

/*
 * Blt_DataTableIterator --
 *
 *	Structure representing a trace used by a client of the table.
 *
 *	Table rows and columns may be tagged with strings.  A row may
 *	have many tags.  The same tag may be used for many rows.  Tags
 *	are used and stored by clients of a table.  Tags can also be
 *	shared between clients of the same table.
 *	
 *	Both rowTable and columnTable are hash tables keyed by the
 *	physical row or column location in the table respectively.
 *	This is not the same as the client's view (the order of rows
 *	or columns as seen by the client).  This is so that clients
 *	(which may have different views) can share tags without
 *	sharing the same view.
 */


typedef enum { 
    DT_ITER_INDEX, DT_ITER_LABEL, DT_ITER_TAG, 
    DT_ITER_RANGE, DT_ITER_ALL, DT_ITER_CHAIN
} Blt_DataTableIteratorType;

typedef struct _Blt_DataTableIterator {
    Blt_DataTable table;		/* Table that we're iterating over. */

    Blt_DataTableIteratorType type;	/* Type of iteration:
				 * DT_ITER_TAG	by row or column tag.
				 * DT_ITER_ALL	by every row or column.
				 * DT_ITER_INDEX	single item: either 
				 *			label or index.
				 * DT_ITER_RANGE	over a consecutive 
				 *			range of indices.
				 * DT_ITER_CHAIN	over an expanded,
				 *			non-overlapping
				 *			list of tags, labels,
				 *			and indices.
				 */

    const char *tagName;	/* Used by notification routines to determine
				 * if a tag is being used. */
    long start;			/* Starting index.  Starting point of search,
				 * saved if iterator is reused.  Used for
				 * DT_ITER_ALL and DT_ITER_INDEX searches. */
    long end;			/* Ending index (inclusive). */

    long next;			/* Next index. */

    /* For tag-based searches. */
    Blt_HashTable *tablePtr;	/* Pointer to tag hash table. */
    Blt_HashSearch cursor;	/* Search iterator for tag hash table. */

    /* For chain-based searches (multiple tags). */
    Blt_Chain chain;		/* This chain, unlike the above hash table
				 * must be freed after it's use. */
    Blt_ChainLink link;		/* Search iterator for chain. */
} Blt_DataTableIterator;

BLT_EXTERN int Blt_DataTable_IterateRows(Tcl_Interp *interp, 
	Blt_DataTable table, Tcl_Obj *objPtr, Blt_DataTableIterator *iter);

BLT_EXTERN int Blt_DataTable_IterateColumns(Tcl_Interp *interp, 
	Blt_DataTable table, Tcl_Obj *objPtr, Blt_DataTableIterator *iter);

BLT_EXTERN int Blt_DataTable_IterateRowsObjv(Tcl_Interp *interp, 
	Blt_DataTable table, int objc, Tcl_Obj *const *objv, 
	Blt_DataTableIterator *iterPtr);

BLT_EXTERN int Blt_DataTable_IterateColumnsObjv(Tcl_Interp *interp, 
	Blt_DataTable table, int objc, Tcl_Obj *const *objv, 
	Blt_DataTableIterator *iterPtr);

BLT_EXTERN void Blt_DataTable_FreeIteratorObjv(Blt_DataTableIterator *iterPtr);

BLT_EXTERN void Blt_DataTable_IterateAllRows(Blt_DataTable table, 
	Blt_DataTableIterator *iterPtr);

BLT_EXTERN void Blt_DataTable_IterateAllColumns(Blt_DataTable table, 
	Blt_DataTableIterator *iterPtr);

BLT_EXTERN Blt_DataTableRow Blt_DataTable_FirstRow(Blt_DataTableIterator *iter);

BLT_EXTERN Blt_DataTableColumn Blt_DataTable_FirstColumn(
	Blt_DataTableIterator *iter);

BLT_EXTERN Blt_DataTableRow Blt_DataTable_NextRow(Blt_DataTableIterator *iter);

BLT_EXTERN Blt_DataTableColumn Blt_DataTable_NextColumn(
	Blt_DataTableIterator *iter);

BLT_EXTERN Blt_DataTableRow Blt_DataTable_FindRow(Tcl_Interp *interp, 
	Blt_DataTable table, Tcl_Obj *objPtr);

BLT_EXTERN Blt_DataTableColumn Blt_DataTable_FindColumn(Tcl_Interp *interp, 
	Blt_DataTable table, Tcl_Obj *objPtr);

BLT_EXTERN int Blt_DataTable_ListRows(Tcl_Interp *interp, Blt_DataTable table, 
	int objc, Tcl_Obj *const *objv, Blt_Chain chain);

BLT_EXTERN int Blt_DataTable_ListColumns(Tcl_Interp *interp, 
	Blt_DataTable table, int objc, Tcl_Obj *const *objv, Blt_Chain chain);

/*
 * Blt_DataTableTraceEvent --
 *
 *	Structure representing an event matching a trace set by a client of
 *	the table.
 *
 *	Table rows and columns may be tagged with strings.  A row may have
 *	many tags.  The same tag may be used for many rows.  Tags are used and
 *	stored by clients of a table.  Tags can also be shared between clients
 *	of the same table.
 *	
 *	Both rowTable and columnTable are hash tables keyed by the physical
 *	row or column location in the table respectively.  This is not the
 *	same as the client's view (the order of rows or columns as seen by the
 *	client).  This is so that clients (which may have different views) can
 *	share tags without sharing the same view.
 */
typedef struct {
    Tcl_Interp *interp;		/* Interpreter to report to */
    Blt_DataTable table;	/* Table object client that received the
				 * event. */  // 
    Blt_DataTableRow row;	/* Matching row and column. */
    Blt_DataTableColumn column;
    unsigned int mask;		/* Indicates type of event received. */
} Blt_DataTableTraceEvent;

typedef int (Blt_DataTableTraceProc)(ClientData clientData, 
	Blt_DataTableTraceEvent *eventPtr);

typedef void (Blt_DataTableTraceDeleteProc)(ClientData clientData);

/*
 * Blt_DataTableTrace --
 *
 *	Structure representing a trace used by a client of the table.
 *
 *	Table rows and columns may be tagged with strings.  A row may have
 *	many tags.  The same tag may be used for many rows.  Tags are used and
 *	stored by clients of a table.  Tags can also be shared between clients
 *	of the same table.
 *	
 *	Both rowTable and columnTable are hash tables keyed by the physical
 *	row or column location in the table respectively.  This is not the
 *	same as the client's view (the order of rows or columns as seen by the
 *	client).  This is so that clients (which may have different views) can
 *	share tags without sharing the same view.
 */
typedef struct _Blt_DataTableTrace {
    unsigned int flags;
    char *rowTag, *colTag;
    Blt_DataTableRow row;
    Blt_DataTableColumn column;
    Blt_DataTableTraceProc *proc;
    Blt_DataTableTraceDeleteProc *deleteProc;
    ClientData clientData;
    Blt_Chain chain;
    Blt_ChainLink link;
} *Blt_DataTableTrace;


#define DT_TRACE_READS		(1<<0)
#define DT_TRACE_CREATES	(1<<1)
#define DT_TRACE_WRITES		(1<<2)
#define DT_TRACE_UNSETS		(1<<3)
#define DT_TRACE_ALL		(DT_TRACE_UNSETS | DT_TRACE_WRITES | \
				 DT_TRACE_READS  | DT_TRACE_CREATES)
#define DT_TRACE_MASK		(TRACE_ALL)

#define DT_TRACE_FOREIGN_ONLY	(1<<8)
#define DT_TRACE_ACTIVE		(1<<9)
#define DT_TRACE_SELF		(1<<10)
#define DT_TRACE_DESTROYED	(1<<11)

BLT_EXTERN void Blt_DataTable_ClearRowTags(Blt_DataTable table, 
	Blt_DataTableRow row);

BLT_EXTERN void Blt_DataTable_ClearColumnTags(Blt_DataTable table, 
	Blt_DataTableColumn column);

BLT_EXTERN void Blt_DataTable_ClearRowTraces(Blt_DataTable table, 
	Blt_DataTableRow row);

BLT_EXTERN void Blt_DataTable_ClearColumnTraces(Blt_DataTable table, 
	Blt_DataTableColumn column);

BLT_EXTERN Blt_DataTableTrace Blt_DataTable_CreateTrace(Blt_DataTable table, 
	Blt_DataTableRow row, Blt_DataTableColumn column, const char *rowTag, 
	const char *columnTag, unsigned int mask, Blt_DataTableTraceProc *proc, 
	Blt_DataTableTraceDeleteProc *deleteProc, ClientData clientData);

BLT_EXTERN Blt_DataTableTrace Blt_DataTable_CreateColumnTrace(
	Blt_DataTable table, Blt_DataTableColumn col, unsigned int mask, 
	Blt_DataTableTraceProc *proc, Blt_DataTableTraceDeleteProc *deleteProc,
	ClientData clientData);

BLT_EXTERN Blt_DataTableTrace Blt_DataTable_CreateColumnTagTrace(
	Blt_DataTable table, const char *tag, unsigned int mask, 
	Blt_DataTableTraceProc *proc, Blt_DataTableTraceDeleteProc *deleteProc,
	ClientData clientData);

BLT_EXTERN Blt_DataTableTrace Blt_DataTable_CreateRowTrace(Blt_DataTable table,
	Blt_DataTableRow row, unsigned int mask, Blt_DataTableTraceProc *proc, 
	Blt_DataTableTraceDeleteProc *deleteProc, ClientData clientData);

BLT_EXTERN Blt_DataTableTrace Blt_DataTable_CreateRowTagTrace(
	Blt_DataTable table, const char *tag, unsigned int mask, 
	Blt_DataTableTraceProc *proc, Blt_DataTableTraceDeleteProc *deleteProc,
	ClientData clientData);

BLT_EXTERN void Blt_DataTable_DeleteTrace(Blt_DataTableTrace trace);

/*
 * Blt_DataTableNotifyEvent --
 *
 *	Structure representing a trace used by a client of the table.
 *
 *	Table rows and columns may be tagged with strings.  A row may have
 *	many tags.  The same tag may be used for many rows.  Tags are used and
 *	stored by clients of a table.  Tags can also be shared between clients
 *	of the same table.
 *	
 *	Both rowTable and columnTable are hash tables keyed by the physical
 *	row or column location in the table respectively.  This is not the
 *	same as the client's view (the order of rows or columns as seen by the
 *	client).  This is so that clients (which may have different views) can
 *	share tags without sharing the same view.
 */
typedef struct {
    Tcl_Interp *interp;		/* Interpreter to report to */
    Blt_DataTable table;	/* Table object client that received the
				 * event. */
    Blt_DataTableHeader header;	/* Matching row or column. */
    int self;			/* Indicates if this table client generated
				 * the event. */
    int type;			/* Indicates type of event received. */

} Blt_DataTableNotifyEvent;

typedef int (Blt_DataTableNotifyEventProc)(ClientData clientData, 
	Blt_DataTableNotifyEvent *eventPtr);

typedef void (Blt_DataTableNotifierDeleteProc)(ClientData clientData);

typedef struct _Blt_DataTableNotifier {
    Blt_DataTable table;
    Blt_ChainLink link;
    Blt_Chain chain;
    Blt_DataTableNotifyEvent event;
    Blt_DataTableNotifyEventProc *proc;
    Blt_DataTableNotifierDeleteProc *deleteProc;
    ClientData clientData;
    Tcl_Interp *interp;
    Blt_DataTableHeader header;
    char *tag;
    unsigned int flags;
} *Blt_DataTableNotifier;


#define DT_NOTIFY_ROW_CREATED		(1<<0)
#define DT_NOTIFY_COLUMN_CREATED	(1<<1)
#define DT_NOTIFY_CREATE		(DT_NOTIFY_COLUMN_CREATED | \
					 DT_NOTIFY_ROW_CREATED)
#define DT_NOTIFY_ROW_DELETED		(1<<2)
#define DT_NOTIFY_COLUMN_DELETED	(1<<3)
#define DT_NOTIFY_DELETE		(DT_NOTIFY_COLUMN_DELETED | \
					 DT_NOTIFY_ROW_DELETED)
#define DT_NOTIFY_ROW_MOVED		(1<<4)
#define DT_NOTIFY_COLUMN_MOVED		(1<<5)
#define DT_NOTIFY_MOVE			(DT_NOTIFY_COLUMN_MOVED | \
					 DT_NOTIFY_ROW_MOVED)
#define DT_NOTIFY_COLUMN_CHANGED \
	(DT_NOTIFY_COLUMN_CREATED | DT_NOTIFY_COLUMN_DELETED | \
		DT_NOTIFY_COLUMN_MOVED)
#define DT_NOTIFY_ROW_CHANGED \
	(DT_NOTIFY_ROW_CREATED | DT_NOTIFY_ROW_DELETED | \
		DT_NOTIFY_ROW_MOVED)
    
#define DT_NOTIFY_ALL_EVENTS (DT_NOTIFY_ROW_CHANGED | DT_NOTIFY_COLUMN_CHANGED)
#define DT_NOTIFY_ROW		(1<<6)
#define DT_NOTIFY_COLUMN	(1<<7)
#define DT_NOTIFY_TYPE_MASK	(DT_NOTIFY_ROW | DT_NOTIFY_COLUMN)

#define DT_NOTIFY_EVENT_MASK	DT_NOTIFY_ALL_EVENTS
#define DT_NOTIFY_MASK		(DT_NOTIFY_EVENT_MASK | DT_NOTIFY_TYPE_MASK)

#define DT_NOTIFY_WHENIDLE	(1<<10)
#define DT_NOTIFY_FOREIGN_ONLY	(1<<11)
#define DT_NOTIFY_PENDING	(1<<12)
#define DT_NOTIFY_ACTIVE	(1<<13)
#define DT_NOTIFY_DESTROYED	(1<<14)

#define DT_NOTIFY_ALL		(NULL)

BLT_EXTERN Blt_DataTableNotifier Blt_DataTable_CreateRowNotifier(
	Tcl_Interp *interp, Blt_DataTable table, Blt_DataTableRow row, 
	unsigned int mask, Blt_DataTableNotifyEventProc *proc, 
	Blt_DataTableNotifierDeleteProc *deleteProc, ClientData clientData);

BLT_EXTERN Blt_DataTableNotifier Blt_DataTable_CreateRowTagNotifier(
	Tcl_Interp *interp, Blt_DataTable table, const char *tag, 
	unsigned int mask, Blt_DataTableNotifyEventProc *proc, 
	Blt_DataTableNotifierDeleteProc *deleteProc, ClientData clientData);

BLT_EXTERN Blt_DataTableNotifier Blt_DataTable_CreateColumnNotifier(
	Tcl_Interp *interp, Blt_DataTable table, Blt_DataTableColumn col, 
	unsigned int mask, Blt_DataTableNotifyEventProc *proc, 
	Blt_DataTableNotifierDeleteProc *deleteProc, ClientData clientData);

BLT_EXTERN Blt_DataTableNotifier Blt_DataTable_CreateColumnTagNotifier(
	Tcl_Interp *interp, Blt_DataTable table, const char *tag, 
	unsigned int mask, Blt_DataTableNotifyEventProc *proc, 
	Blt_DataTableNotifierDeleteProc *deleteProc, ClientData clientData);


BLT_EXTERN void Blt_DataTable_DeleteNotifier(Blt_DataTableNotifier notifier);

/*
 * Blt_DataTableSortOrder --
 *
 */

typedef int (Blt_DataTableSortProc)(ClientData clientData, Tcl_Obj *objPtr1, 
	Tcl_Obj *objPtr2);

typedef struct {
    int type;			/* Type of sort to be performed: see flags
				 * below. */
    Blt_DataTableSortProc *proc; /* Procedures to be called to compare two
				 * entries in the same row or column. */
    ClientData clientData;	/* One word of data passed to the sort
				 * comparison procedure above. */
    Blt_DataTableColumn column;	/* Column to be compared. */

} Blt_DataTableSortOrder;

#define DT_SORT_NONE		0
#define DT_SORT_INTEGER		(1<<0)
#define DT_SORT_DOUBLE		(1<<1)
#define DT_SORT_DICTIONARY	(1<<2)
#define DT_SORT_ASCII		(1<<3)
#define DT_SORT_CUSTOM		(1<<4)
#define DT_SORT_DECREASING	(1<<6)

BLT_EXTERN Blt_DataTableRow *Blt_DataTable_SortRows(Blt_DataTable table, 
	Blt_DataTableSortOrder *order, size_t nCompares, unsigned int flags);

BLT_EXTERN Blt_DataTableRow *Blt_DataTable_RowMap(Blt_DataTable table);
BLT_EXTERN Blt_DataTableColumn *Blt_DataTable_ColumnMap(Blt_DataTable table);

BLT_EXTERN void Blt_DataTable_SetRowMap(Blt_DataTable table, 
	Blt_DataTableRow *map);
BLT_EXTERN void Blt_DataTable_SetColumnMap(Blt_DataTable table, 
	Blt_DataTableColumn *map);

#define DT_RESTORE_NO_TAGS	    (1<<0)
#define DT_RESTORE_OVERWRITE	    (1<<1)

BLT_EXTERN int Blt_DataTable_Restore(Tcl_Interp *interp, Blt_DataTable table, 
	char *string, unsigned int flags);
BLT_EXTERN int Blt_DataTable_FileRestore(Tcl_Interp *interp, 
	Blt_DataTable table, const char *fileName, unsigned int flags);
BLT_EXTERN int Blt_DataTable_Dump(Tcl_Interp *interp, Blt_DataTable table, 
	Blt_DataTableRow *rowMap, Blt_DataTableColumn *colMap, 
	Tcl_DString *dsPtr);
BLT_EXTERN int Blt_DataTable_FileDump(Tcl_Interp *interp, Blt_DataTable table, 
	Blt_DataTableRow *rowMap, Blt_DataTableColumn *colMap, 
	const char *fileName);

typedef int (Blt_DataTable_ImportProc)(Blt_DataTable table, Tcl_Interp *interp,
	int objc, Tcl_Obj *const *objv);

typedef int (Blt_DataTable_ExportProc)(Blt_DataTable table, Tcl_Interp *interp,
	int objc, Tcl_Obj *const *objv);

BLT_EXTERN int Blt_DataTable_RegisterFormat(Tcl_Interp *interp, 
	const char *name, Blt_DataTable_ImportProc *importProc, 
	Blt_DataTable_ExportProc *exportProc);

BLT_EXTERN void Blt_DataTable_UnsetKeys(Blt_DataTable table);
BLT_EXTERN Blt_Chain Blt_DataTable_GetKeys(Blt_DataTable table);
BLT_EXTERN int Blt_DataTable_SetKeys(Blt_DataTable table, Blt_Chain keys, 
	int unique);
BLT_EXTERN int Blt_DataTable_KeyLookup(Tcl_Interp *interp, Blt_DataTable table,
	int objc, Tcl_Obj *const *objv, Blt_DataTableRow *rowPtr);

BLT_EXTERN void Blt_DataTable_SetEmptyValue(Blt_DataTable table, 
	const char *emptyValue);


#define Blt_DataTable_NumRows(t)	   ((t)->corePtr->rows.nUsed)
#define Blt_DataTable_RowIndex(r)	   ((r)->index)
#define Blt_DataTable_RowLabel(r)	   ((r)->label)
#define Blt_DataTable_GetRow(t,i)	\
    (Blt_DataTableRow)((t)->corePtr->rows.map[(i)-1])

#define Blt_DataTable_NumColumns(t)	   ((t)->corePtr->columns.nUsed)
#define Blt_DataTable_ColumnIndex(c)       ((c)->index)
#define Blt_DataTable_ColumnLabel(c)	   ((c)->label)
#define Blt_DataTable_ColumnType(c)	   ((c)->type)
#define Blt_DataTable_GetColumn(t,i)	\
    (Blt_DataTableColumn)((t)->corePtr->columns.map[(i)-1])

#define Blt_DataTable_TableName(t)	   ((t)->name)
#define Blt_DataTable_EmptyValue(t)	   ((t)->emptyValue)


#endif /* BLT_DATATABLE_H */
