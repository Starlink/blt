
/*
 * bltDataTable.h --
 *
 *	Copyright 1998-2004 George A. Howlett.
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

typedef struct _Blt_TableTags *Blt_TableTags;

typedef enum {
    TABLE_COLUMN_TYPE_UNKNOWN=-1, 
    TABLE_COLUMN_TYPE_STRING, 
    TABLE_COLUMN_TYPE_INT, 
    TABLE_COLUMN_TYPE_DOUBLE, 
    TABLE_COLUMN_TYPE_LONG, 
} Blt_TableColumnType;


typedef struct _Blt_TableValue {
    union {				
	long l;
	double d;
	Tcl_WideInt w;
    } datum;				/* Internal representation of data:
					 * used to speed comparisons, sorting,
					 * etc. */
    char *string;			/* String representation of value.  If
					 * NULL, indicates empty value. */
} *Blt_TableValue;

typedef struct _Blt_TableHeader {
    const char *label;			/* Label of row or column. */
    long index;				/* Reverse lookup offset-to-index. */
    long offset;
    unsigned int flags;
} *Blt_TableHeader;

typedef struct _Blt_TableRow {
    const char *label;			/* Label of row or column. */
    long index;				/* Reverse lookup offset-to-index. */
    long offset;
    unsigned int flags;
} *Blt_TableRow;

typedef struct _Blt_TableColumn {
    const char *label;			/* Label of row or column. */
    long index;				/* Reverse lookup offset-to-index. */
    long offset;
    unsigned short flags;
    Blt_TableColumnType type;
} *Blt_TableColumn;

typedef struct {
    const char *name;
    long headerSize;
} Blt_TableRowColumnClass;

/*
 * Blt_TableRowColumn --
 *
 *	Structure representing a row or column in the table. 
 */
typedef struct _Blt_TableRowColumn {
    Blt_TableRowColumnClass *classPtr;
    Blt_Pool headerPool;
    long nAllocated;			/* Length of allocated array
					 * below. May exceed the number of row
					 * or column headers used. */
    long nUsed;
    Blt_TableHeader *map;		/* Array of row or column headers. */
    Blt_Chain freeList;			/* Tracks free row or column
					 * headers.  */
    Blt_HashTable labelTable;		/* Hash table of labels. Maps labels
					 * to table offsets. */
    long nextId;			/* Used to generate default labels. */
} Blt_TableRowColumn;

/*
 * Blt_TableCore --
 *
 *	Structure representing a table object. 
 *
 *	The table object is an array of column vectors. Each vector is an
 *	array of Blt_TableValue's, representing the data for the column.
 *	Empty row entries are designated by 0 length values.  Column vectors are
 *	allocated when needed.  Every column in the table has the same length.
 *
 *	Rows and columns are indexed by a map of pointers to headers.  This
 *	map represents the order of the rows or columns.  A table object can
 *	be shared by several clients.  When a client wants to use a table
 *	object, it is given a token that represents the table.  The object
 *	tracks its clients by its token. When all clients have released their
 *	tokens, the tuple object is automatically destroyed.
 */
typedef struct _Blt_TableCore {
    Blt_TableRowColumn rows, columns;
    Blt_TableValue *data;		/* Array of column vector pointers */
    unsigned int flags;			/* Internal flags. See definitions
					 * below. */
    Blt_Chain clients;			/* List of clients using this table */
    unsigned long mtime, ctime;
    unsigned int notifyFlags;		/* Notification flags. See definitions
					 * below. */
    int notifyHold;
} Blt_TableCore;

/*
 * Blt_Table --
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
typedef struct _Blt_Table {
    unsigned int magic;			/* Magic value indicating whether a
					 * generic pointer is really a
					 * datatable token or not. */
    const char *name;			/* Fully namespace-qualified name of
					 * the client. */
    Blt_TableCore *corePtr;		/* Pointer to the structure containing
					 * the master information about the
					 * table used by the client.  If NULL,
					 * this indicates that the table has
					 * been destroyed (but as of yet, this
					 * client hasn't recognized it). */
    Tcl_Interp *interp;
    Blt_HashTable *tablePtr;		/* Interpreter-specific global hash
					 * table of all datatable clients.
					 * Each entry is a chain of clients
					 * that are sharing the same core
					 * datatable. */
    Blt_HashEntry *hPtr;		/* This client's entry in the above
					 * hash table. This is a list of
					 * clients that * all using the core
					 * datatable. */
    Blt_ChainLink link2;		/* This client's entry in the list
					 * found in the above list (hashtable
					 * entry). */
    Blt_ChainLink link;			/* Pointer into the server's chain of
					 * clients. */

    Blt_HashTable *rowTags;
    Blt_HashTable *columnTags;

    Blt_Chain traces;			/* Chain of traces. */
    Blt_Chain columnNotifiers;		/* Chain of event handlers. */
    Blt_Chain rowNotifiers;		/* Chain of event handlers. */
    Blt_TableTags tags;

    Blt_HashTable *keyTables;		/* Array of primary keys. */
    long nKeys;				/* Length of the above array. */

    Blt_TableRow *masterKey;		/* Master key entry. */
    Blt_HashTable masterKeyTable;
    Blt_Chain primaryKeys;
    unsigned int flags;
} *Blt_Table;

BLT_EXTERN void Blt_Table_ReleaseTags(Blt_Table table);

BLT_EXTERN int Blt_Table_TableExists(Tcl_Interp *interp, const char *name);
BLT_EXTERN int Blt_Table_CreateTable(Tcl_Interp *interp, const char *name, 
	Blt_Table *tablePtr);
BLT_EXTERN int Blt_Table_Open(Tcl_Interp *interp, const char *name, 
	Blt_Table *tablePtr);
BLT_EXTERN void Blt_Table_Close(Blt_Table table);

BLT_EXTERN int Blt_Table_SameTableObject(Blt_Table table1, Blt_Table table2);

BLT_EXTERN const char *Blt_Table_Name(Blt_Table table);

BLT_EXTERN Blt_TableRow Blt_Table_FindRowByLabel(Blt_Table table, 
	const char *label);
BLT_EXTERN Blt_TableColumn Blt_Table_FindColumnByLabel(Blt_Table table, 
	const char *label);
BLT_EXTERN Blt_TableRow Blt_Table_FindRowByIndex(Blt_Table table, long index);
BLT_EXTERN Blt_TableColumn Blt_Table_FindColumnByIndex(Blt_Table table, 
	long index);

BLT_EXTERN int Blt_Table_SetRowLabel(Tcl_Interp *interp, Blt_Table table, 
	Blt_TableRow row, const char *label);
BLT_EXTERN int Blt_Table_SetColumnLabel(Tcl_Interp *interp, Blt_Table table, 
	Blt_TableColumn column, const char *label);

BLT_EXTERN Blt_TableColumnType Blt_Table_ColumnType(Blt_TableColumn column);
BLT_EXTERN Blt_TableColumnType Blt_Table_GetColumnType(const char *typeName);
BLT_EXTERN int Blt_Table_SetColumnType(Blt_Table table, Blt_TableColumn column,
	Blt_TableColumnType type);
BLT_EXTERN const char *Blt_Table_NameOfType(Blt_TableColumnType type);

BLT_EXTERN int Blt_Table_SetColumnTag(Tcl_Interp *interp, Blt_Table table, 
	Blt_TableColumn column, const char *tagName);
BLT_EXTERN int Blt_Table_SetRowTag(Tcl_Interp *interp, Blt_Table table, 
	Blt_TableRow row, const char *tagName);

BLT_EXTERN Blt_TableRow Blt_Table_CreateRow(Tcl_Interp *interp, Blt_Table table,
	const char *label);
BLT_EXTERN Blt_TableColumn Blt_Table_CreateColumn(Tcl_Interp *interp, 
	Blt_Table table, const char *label);

BLT_EXTERN int Blt_Table_ExtendRows(Tcl_Interp *interp, Blt_Table table,
	size_t n, Blt_TableRow *rows);
BLT_EXTERN int Blt_Table_ExtendColumns(Tcl_Interp *interp, Blt_Table table, 
	size_t n, Blt_TableColumn *columms);
BLT_EXTERN int Blt_Table_DeleteRow(Blt_Table table, Blt_TableRow row);
BLT_EXTERN int Blt_Table_DeleteColumn(Blt_Table table, Blt_TableColumn column);
BLT_EXTERN int Blt_Table_MoveRows(Tcl_Interp *interp, Blt_Table table, 
	Blt_TableRow from, Blt_TableRow to, size_t n);
BLT_EXTERN int Blt_Table_MoveColumns(Tcl_Interp *interp, Blt_Table table, 
	Blt_TableColumn from, Blt_TableColumn to, size_t n);

BLT_EXTERN Tcl_Obj *Blt_Table_GetObj(Blt_Table table, Blt_TableRow row, 
	Blt_TableColumn column);
BLT_EXTERN int Blt_Table_SetObj(Blt_Table table, Blt_TableRow row,
	Blt_TableColumn column, Tcl_Obj *objPtr);

BLT_EXTERN const char *Blt_Table_GetString(Blt_Table table, Blt_TableRow row, 
	Blt_TableColumn column);
BLT_EXTERN int Blt_Table_SetString(Blt_Table table, Blt_TableRow row,
	Blt_TableColumn column, const char *string, int length);
BLT_EXTERN int Blt_Table_AppendString(Tcl_Interp *interp, Blt_Table table, 
	Blt_TableRow row, Blt_TableColumn column, const char *string, 
	int length);

BLT_EXTERN double Blt_Table_GetDouble(Blt_Table table, Blt_TableRow row, 
	Blt_TableColumn column);
BLT_EXTERN int Blt_Table_SetDouble(Blt_Table table, Blt_TableRow row, 
	Blt_TableColumn column, double value);
BLT_EXTERN long Blt_Table_GetLong(Blt_Table table, Blt_TableRow row, 
	Blt_TableColumn column, long defValue);
BLT_EXTERN int Blt_Table_SetLong(Blt_Table table, Blt_TableRow row, 
	Blt_TableColumn column, long value);

BLT_EXTERN Blt_TableValue Blt_Table_GetValue(Blt_Table table, Blt_TableRow row, 
	Blt_TableColumn column);
BLT_EXTERN int Blt_Table_SetValue(Blt_Table table, Blt_TableRow row, 
	Blt_TableColumn column, Blt_TableValue value);
BLT_EXTERN int Blt_Table_UnsetValue(Blt_Table table, Blt_TableRow row, 
	Blt_TableColumn column);
BLT_EXTERN int Blt_Table_ValueExists(Blt_Table table, Blt_TableRow row, 
	Blt_TableColumn column);

BLT_EXTERN Blt_HashTable *Blt_Table_FindRowTagTable(Blt_Table table, 
	const char *tagName);
BLT_EXTERN Blt_HashTable *Blt_Table_FindColumnTagTable(Blt_Table table, 
	const char *tagName);
BLT_EXTERN Blt_Chain Blt_Table_RowTags(Blt_Table table, Blt_TableRow row);
BLT_EXTERN Blt_Chain Blt_Table_ColumnTags(Blt_Table table, 
	Blt_TableColumn column);

BLT_EXTERN Blt_Chain Blt_Table_Traces(Blt_Table table);
BLT_EXTERN int Blt_Table_TagsAreShared(Blt_Table table);

BLT_EXTERN int Blt_Table_HasRowTag(Blt_Table table, Blt_TableRow row, 
	const char *tagName);
BLT_EXTERN int Blt_Table_HasColumnTag(Blt_Table table, Blt_TableColumn column, 
	const char *tagName);
BLT_EXTERN void Blt_Table_AddColumnTag(Blt_Table table, Blt_TableColumn column,
	const char *tagName);
BLT_EXTERN void Blt_Table_AddRowTag(Blt_Table table, Blt_TableRow row, 
	const char *tagName);
BLT_EXTERN int Blt_Table_ForgetRowTag(Tcl_Interp *interp, Blt_Table table, 
	const char *tagName);
BLT_EXTERN int Blt_Table_ForgetColumnTag(Tcl_Interp *interp, Blt_Table table, 
	const char *tagName);
BLT_EXTERN int Blt_Table_UnsetRowTag(Tcl_Interp *interp, Blt_Table table, 
	Blt_TableRow row, const char *tagName);
BLT_EXTERN int Blt_Table_UnsetColumnTag(Tcl_Interp *interp, Blt_Table table, 
	Blt_TableColumn column, const char *tagName);
BLT_EXTERN Blt_HashEntry *Blt_Table_FirstRowTag(Blt_Table table, 
	Blt_HashSearch *cursorPtr);
BLT_EXTERN Blt_HashEntry *Blt_Table_FirstColumnTag(Blt_Table table, 
	Blt_HashSearch *cursorPtr);

BLT_EXTERN Blt_TableColumn Blt_Table_FirstColumn(Blt_Table table);
BLT_EXTERN Blt_TableColumn Blt_Table_NextColumn(Blt_Table table, 
	Blt_TableColumn column);
BLT_EXTERN Blt_TableRow Blt_Table_FirstRow(Blt_Table table);
BLT_EXTERN Blt_TableRow Blt_Table_NextRow(Blt_Table table, Blt_TableRow row);

typedef enum { 
    TABLE_SPEC_UNKNOWN, 
    TABLE_SPEC_INDEX, 
    TABLE_SPEC_RANGE, 
    TABLE_SPEC_LABEL, 
    TABLE_SPEC_TAG, 
} Blt_TableRowColumnSpec;

BLT_EXTERN Blt_TableRowColumnSpec Blt_Table_RowSpec(Blt_Table table, 
	Tcl_Obj *objPtr, const char **stringPtr);
BLT_EXTERN Blt_TableRowColumnSpec Blt_Table_ColumnSpec(Blt_Table table, 
	Tcl_Obj *objPtr, const char **stringPtr);

/*
 * Blt_TableIterator --
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
    TABLE_ITERATOR_INDEX, 
    TABLE_ITERATOR_LABEL, 
    TABLE_ITERATOR_TAG, 
    TABLE_ITERATOR_RANGE, 
    TABLE_ITERATOR_ALL, 
    TABLE_ITERATOR_CHAIN
} Blt_TableIteratorType;

typedef struct _Blt_TableIterator {
    Blt_Table table;			/* Table that we're iterating over. */

    Blt_TableIteratorType type;		/* Type of iteration:
					 * TABLE_ITERATOR_TAG  by row or column tag.
					 * TABLE_ITERATOR_ALL 
					 *		by every row or column.
					 * TABLE_ITERATOR_INDEX single item: either 
					 *		    label or index.
					 * TABLE_ITERATOR_RANGE over a consecutive 
					 *		 range of indices.
					 * TABLE_ITERATOR_CHAIN over an expanded,
					 *		 non-overlapping
					 *		 list of tags, labels,
					 *		 and indices.
					 */

    const char *tagName;		/* Used by notification routines to
					 * determine if a tag is being
					 * used. */
    long start;				/* Starting index.  Starting point of
					 * search, saved if iterator is
					 * reused.  Used for TABLE_ITERATOR_ALL and
					 * TABLE_ITERATOR_INDEX searches. */
    long end;				/* Ending index (inclusive). */

    long next;				/* Next index. */

    /* For tag-based searches. */
    Blt_HashTable *tablePtr;		/* Pointer to tag hash table. */
    Blt_HashSearch cursor;		/* Iterator for tag hash table. */

    /* For chain-based searches (multiple tags). */
    Blt_Chain chain;			/* This chain, unlike the above hash
					 * table must be freed after its
					 * use. */
    Blt_ChainLink link;			/* Search iterator for chain. */
} Blt_TableIterator;

BLT_EXTERN int Blt_Table_IterateRows(Tcl_Interp *interp, Blt_Table table, 
	Tcl_Obj *objPtr, Blt_TableIterator *iter);

BLT_EXTERN int Blt_Table_IterateColumns(Tcl_Interp *interp, Blt_Table table, 
	Tcl_Obj *objPtr, Blt_TableIterator *iter);

BLT_EXTERN int Blt_Table_IterateRowsObjv(Tcl_Interp *interp, Blt_Table table, 
	int objc, Tcl_Obj *const *objv, Blt_TableIterator *iterPtr);

BLT_EXTERN int Blt_Table_IterateColumnsObjv(Tcl_Interp *interp, Blt_Table table,
	int objc, Tcl_Obj *const *objv, Blt_TableIterator *iterPtr);

BLT_EXTERN void Blt_Table_FreeIteratorObjv(Blt_TableIterator *iterPtr);

BLT_EXTERN void Blt_Table_IterateAllRows(Blt_Table table, 
	Blt_TableIterator *iterPtr);

BLT_EXTERN void Blt_Table_IterateAllColumns(Blt_Table table, 
	Blt_TableIterator *iterPtr);

BLT_EXTERN Blt_TableRow Blt_Table_FirstTaggedRow(Blt_TableIterator *iter);

BLT_EXTERN Blt_TableColumn Blt_Table_FirstTaggedColumn(Blt_TableIterator *iter);

BLT_EXTERN Blt_TableRow Blt_Table_NextTaggedRow(Blt_TableIterator *iter);

BLT_EXTERN Blt_TableColumn Blt_Table_NextTaggedColumn(Blt_TableIterator *iter);

BLT_EXTERN Blt_TableRow Blt_Table_FindRow(Tcl_Interp *interp, Blt_Table table, 
	Tcl_Obj *objPtr);

BLT_EXTERN Blt_TableColumn Blt_Table_FindColumn(Tcl_Interp *interp, 
	Blt_Table table, Tcl_Obj *objPtr);

BLT_EXTERN int Blt_Table_ListRows(Tcl_Interp *interp, Blt_Table table, 
	int objc, Tcl_Obj *const *objv, Blt_Chain chain);

BLT_EXTERN int Blt_Table_ListColumns(Tcl_Interp *interp, Blt_Table table, 
	int objc, Tcl_Obj *const *objv, Blt_Chain chain);

/*
 * Blt_TableTraceEvent --
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
    Tcl_Interp *interp;			/* Interpreter to report to */
    Blt_Table table;			/* Table object client that received
					 * the event. */
    Blt_TableRow row;			/* Matching row and column. */
    Blt_TableColumn column;
    unsigned int mask;			/* Type of event received. */
} Blt_TableTraceEvent;

typedef int (Blt_TableTraceProc)(ClientData clientData, 
	Blt_TableTraceEvent *eventPtr);

typedef void (Blt_TableTraceDeleteProc)(ClientData clientData);

/*
 * Blt_TableTrace --
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
typedef struct _Blt_TableTrace {
    unsigned int flags;
    const char *rowTag, *colTag;
    Blt_TableRow row;
    Blt_TableColumn column;
    Blt_TableTraceProc *proc;
    Blt_TableTraceDeleteProc *deleteProc;
    ClientData clientData;
    Blt_Chain chain;
    Blt_ChainLink link;
} *Blt_TableTrace;


#define TABLE_TRACE_READS	(1<<0)
#define TABLE_TRACE_CREATES	(1<<1)
#define TABLE_TRACE_WRITES	(1<<2)
#define TABLE_TRACE_UNSETS	(1<<3)
#define TABLE_TRACE_ALL		(TABLE_TRACE_UNSETS | TABLE_TRACE_WRITES | \
				 TABLE_TRACE_READS  | TABLE_TRACE_CREATES)
#define TABLE_TRACE_MASK	(TRACE_ALL)

#define TABLE_TRACE_FOREIGN_ONLY (1<<8)
#define TABLE_TRACE_ACTIVE	(1<<9)
#define TABLE_TRACE_SELF	(1<<10)
#define TABLE_TRACE_DESTROYED	(1<<11)

BLT_EXTERN void Blt_Table_ClearRowTags(Blt_Table table, Blt_TableRow row);

BLT_EXTERN void Blt_Table_ClearColumnTags(Blt_Table table, 
	Blt_TableColumn column);

BLT_EXTERN void Blt_Table_ClearRowTraces(Blt_Table table, Blt_TableRow row);

BLT_EXTERN void Blt_Table_ClearColumnTraces(Blt_Table table, 
	Blt_TableColumn column);

BLT_EXTERN Blt_TableTrace Blt_Table_CreateTrace(Blt_Table table, 
	Blt_TableRow row, Blt_TableColumn column, const char *rowTag, 
	const char *columnTag, unsigned int mask, Blt_TableTraceProc *proc, 
	Blt_TableTraceDeleteProc *deleteProc, ClientData clientData);

BLT_EXTERN Blt_TableTrace Blt_Table_CreateColumnTrace(Blt_Table table, 
	Blt_TableColumn column, unsigned int mask, Blt_TableTraceProc *proc, 
	Blt_TableTraceDeleteProc *deleteProc, ClientData clientData);

BLT_EXTERN Blt_TableTrace Blt_Table_CreateColumnTagTrace(Blt_Table table, 
	const char *tag, unsigned int mask, Blt_TableTraceProc *proc, 
	Blt_TableTraceDeleteProc *deleteProc, ClientData clientData);

BLT_EXTERN Blt_TableTrace Blt_Table_CreateRowTrace(Blt_Table table,
	Blt_TableRow row, unsigned int mask, Blt_TableTraceProc *proc, 
	Blt_TableTraceDeleteProc *deleteProc, ClientData clientData);

BLT_EXTERN Blt_TableTrace Blt_Table_CreateRowTagTrace(Blt_Table table, 
	const char *tag, unsigned int mask, Blt_TableTraceProc *proc, 
	Blt_TableTraceDeleteProc *deleteProc, ClientData clientData);

BLT_EXTERN void Blt_Table_DeleteTrace(Blt_TableTrace trace);

/*
 * Blt_TableNotifyEvent --
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
    Tcl_Interp *interp;			/* Interpreter to report results. */
    Blt_Table table;			/* Table object client that received
					 * the event. */
    Blt_TableHeader header;		/* Matching row or column. */
    int self;				/* Indicates if this table client
					 * generated the event. */
    int type;			        /* Indicates type of event
					 * received. */
} Blt_TableNotifyEvent;

typedef int (Blt_TableNotifyEventProc)(ClientData clientData, 
	Blt_TableNotifyEvent *eventPtr);

typedef void (Blt_TableNotifierDeleteProc)(ClientData clientData);

typedef struct _Blt_TableNotifier {
    Blt_Table table;
    Blt_ChainLink link;
    Blt_Chain chain;
    Blt_TableNotifyEvent event;
    Blt_TableNotifyEventProc *proc;
    Blt_TableNotifierDeleteProc *deleteProc;
    ClientData clientData;
    Tcl_Interp *interp;
    Blt_TableHeader header;
    char *tag;
    unsigned int flags;
} *Blt_TableNotifier;


#define TABLE_NOTIFY_ROW_CREATED	(1<<0)
#define TABLE_NOTIFY_COLUMN_CREATED	(1<<1)
#define TABLE_NOTIFY_CREATE		(TABLE_NOTIFY_COLUMN_CREATED | \
					 TABLE_NOTIFY_ROW_CREATED)
#define TABLE_NOTIFY_ROW_DELETED	(1<<2)
#define TABLE_NOTIFY_COLUMN_DELETED	(1<<3)
#define TABLE_NOTIFY_DELETE		(TABLE_NOTIFY_COLUMN_DELETED | \
					 TABLE_NOTIFY_ROW_DELETED)
#define TABLE_NOTIFY_ROW_MOVED		(1<<4)
#define TABLE_NOTIFY_COLUMN_MOVED	(1<<5)
#define TABLE_NOTIFY_MOVE		(TABLE_NOTIFY_COLUMN_MOVED | \
					 TABLE_NOTIFY_ROW_MOVED)
#define TABLE_NOTIFY_COLUMN_CHANGED \
	(TABLE_NOTIFY_COLUMN_CREATED | TABLE_NOTIFY_COLUMN_DELETED | \
	 TABLE_NOTIFY_COLUMN_MOVED)
#define TABLE_NOTIFY_ROW_CHANGED \
	(TABLE_NOTIFY_ROW_CREATED | TABLE_NOTIFY_ROW_DELETED | \
	 TABLE_NOTIFY_ROW_MOVED)
    
#define TABLE_NOTIFY_ALL_EVENTS (TABLE_NOTIFY_ROW_CHANGED | \
				 TABLE_NOTIFY_COLUMN_CHANGED)
#define TABLE_NOTIFY_ROW	(1<<6)
#define TABLE_NOTIFY_COLUMN	(1<<7)
#define TABLE_NOTIFY_TYPE_MASK	(TABLE_NOTIFY_ROW | TABLE_NOTIFY_COLUMN)

#define TABLE_NOTIFY_EVENT_MASK	TABLE_NOTIFY_ALL_EVENTS
#define TABLE_NOTIFY_MASK	(TABLE_NOTIFY_EVENT_MASK | \
				 TABLE_NOTIFY_TYPE_MASK)

#define TABLE_NOTIFY_WHENIDLE	(1<<10)
#define TABLE_NOTIFY_FOREIGN_ONLY (1<<11)
#define TABLE_NOTIFY_PENDING	(1<<12)
#define TABLE_NOTIFY_ACTIVE	(1<<13)
#define TABLE_NOTIFY_DESTROYED	(1<<14)

#define TABLE_NOTIFY_ALL	(NULL)

BLT_EXTERN Blt_TableNotifier Blt_Table_CreateRowNotifier(Tcl_Interp *interp, 
	Blt_Table table, Blt_TableRow row, unsigned int mask, 
	Blt_TableNotifyEventProc *proc, Blt_TableNotifierDeleteProc *deleteProc,
	ClientData clientData);

BLT_EXTERN Blt_TableNotifier Blt_Table_CreateRowTagNotifier(Tcl_Interp *interp,
	Blt_Table table, const char *tag, unsigned int mask, 
	Blt_TableNotifyEventProc *proc, Blt_TableNotifierDeleteProc *deleteProc,
	ClientData clientData);

BLT_EXTERN Blt_TableNotifier Blt_Table_CreateColumnNotifier(
	Tcl_Interp *interp, Blt_Table table, Blt_TableColumn column, 
	unsigned int mask, Blt_TableNotifyEventProc *proc, 
	Blt_TableNotifierDeleteProc *deleteProc, ClientData clientData);

BLT_EXTERN Blt_TableNotifier Blt_Table_CreateColumnTagNotifier(
	Tcl_Interp *interp, Blt_Table table, const char *tag, 
	unsigned int mask, Blt_TableNotifyEventProc *proc, 
	Blt_TableNotifierDeleteProc *deleteProc, ClientData clientData);


BLT_EXTERN void Blt_Table_DeleteNotifier(Blt_TableNotifier notifier);

/*
 * Blt_TableSortOrder --
 *
 */
typedef int (Blt_TableSortProc)(ClientData clientData, 
	Blt_TableValue value1, Blt_TableValue value2);

typedef struct {
    int type;				/* Type of sort to be performed: see
					 * flags below. */
    Blt_TableSortProc *sortProc;	/* Procedures to be called to compare
					 * two entries in the same row or
					 * column. */
    Blt_TableSortProc *userProc;	/* Procedures to be called to compare
					 * two entries in the same row or
					 * column. */
    ClientData clientData;		/* One word of data passed to the sort
					 * comparison procedure above. */
    Blt_TableColumn column;		/* Column to be compared. */
} Blt_TableSortOrder;


#define SORT_DECREASING		(1<<0)
#define SORT_LIST		(1<<1)

#define SORT_TYPE_MASK		(3<<2)
#define SORT_NONE		(0)
#define SORT_ASCII		(1<<2)
#define SORT_DICTIONARY		(2<<2)
#define SORT_FREQUENCY		(3<<2)

BLT_EXTERN Blt_TableRow *Blt_Table_SortRows(Blt_Table table, 
	Blt_TableSortOrder *order, size_t nCompares, unsigned int flags);

BLT_EXTERN Blt_TableRow *Blt_Table_RowMap(Blt_Table table);
BLT_EXTERN Blt_TableColumn *Blt_Table_ColumnMap(Blt_Table table);

BLT_EXTERN void Blt_Table_SetRowMap(Blt_Table table, Blt_TableRow *map);
BLT_EXTERN void Blt_Table_SetColumnMap(Blt_Table table, Blt_TableColumn *map);

#define TABLE_RESTORE_NO_TAGS	    (1<<0)
#define TABLE_RESTORE_OVERWRITE	    (1<<1)

BLT_EXTERN int Blt_Table_Restore(Tcl_Interp *interp, Blt_Table table, 
	char *string, unsigned int flags);
BLT_EXTERN int Blt_Table_FileRestore(Tcl_Interp *interp, Blt_Table table, 
	const char *fileName, unsigned int flags);
BLT_EXTERN int Blt_Table_Dump(Tcl_Interp *interp, Blt_Table table, 
	Blt_TableRow *rowMap, Blt_TableColumn *colMap, Tcl_DString *dsPtr);
BLT_EXTERN int Blt_Table_FileDump(Tcl_Interp *interp, Blt_Table table, 
	Blt_TableRow *rowMap, Blt_TableColumn *colMap, const char *fileName);

typedef int (Blt_TableImportProc)(Blt_Table table, Tcl_Interp *interp, int objc,
	Tcl_Obj *const *objv);

typedef int (Blt_TableExportProc)(Blt_Table table, Tcl_Interp *interp,
	int objc, Tcl_Obj *const *objv);

BLT_EXTERN int Blt_Table_RegisterFormat(Tcl_Interp *interp, const char *name, 
	Blt_TableImportProc *importProc, Blt_TableExportProc *exportProc);

BLT_EXTERN void Blt_Table_UnsetKeys(Blt_Table table);
BLT_EXTERN Blt_Chain Blt_Table_GetKeys(Blt_Table table);
BLT_EXTERN int Blt_Table_SetKeys(Blt_Table table, Blt_Chain keys, int unique);
BLT_EXTERN int Blt_Table_KeyLookup(Tcl_Interp *interp, Blt_Table table,
	int objc, Tcl_Obj *const *objv, Blt_TableRow *rowPtr);


#define Blt_Table_NumRows(t)	   ((t)->corePtr->rows.nUsed)
#define Blt_Table_RowIndex(r)	   ((r)->index)
#define Blt_Table_RowLabel(r)	   ((r)->label)
#define Blt_Table_Row(t,i)  \
    (Blt_TableRow)((t)->corePtr->rows.map[(i)-1])

#define Blt_Table_NumColumns(t)	   ((t)->corePtr->columns.nUsed)
#define Blt_Table_ColumnIndex(c)   ((c)->index)
#define Blt_Table_ColumnLabel(c)   ((c)->label)
#define Blt_Table_Column(t,i) \
	(Blt_TableColumn)((t)->corePtr->columns.map[(i)-1])

#define Blt_Table_TableName(t)	   ((t)->name)
#define Blt_Table_EmptyValue(t)	   ((t)->emptyValue)
#define Blt_Table_ColumnType(c)	   ((c)->type)

#endif /* BLT_DATATABLE_H */
