
/*
 *
 * bltDtCmd.c --
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

/*
  blt::datatable create t0 t1 t2
  blt::datatable names
  t0 destroy
     -or-
  blt::datatable destroy t0
  blt::datatable copy table@node table@node -recurse -tags

  table move node after|before|into t2@node

  $t apply -recurse $root command arg arg			

  $t attach tablename				

  $t children $n
  t0 copy node1 node2 node3 node4 node5 destName 
  $t delete $n...				
  $t delete 0 
  $t delete 0 10
  $t delete all
  $t depth $n
  $t dump 
  $t dump -file fileName
  $t dup $t2		
  $t find $root -name pat -name pattern
  $t firstchild $n
  $t get $n $key
  $t get $n $key(abc)
  $t index $n
  $t insert $parent $switches?
  $t isancestor $n1 $n2
  $t isbefore $n1 $n2
  $t isleaf $n
  $t lastchild $n
  $t move $n1 after|before|into $n2
  $t next $n
  $t nextsibling $n
  $t path $n1 $n2 $n3...
  $t parent $n
  $t previous $n
  $t prevsibling $n
  $t restore $root data -overwrite
  $t root ?$n?

  $t set $n $key $value ?$key $value?
  $t size $n
  $t slink $n $t2@$node				???
  $t sort -recurse $root		

  $t tag delete tag1 tag2 tag3...
  $t tag names
  $t tag nodes $tag
  $t tag set $n tag1 tag2 tag3...
  $t tag unset $n tag1 tag2 tag3...

  $t trace create $n $key how command		
  $t trace delete id1 id2 id3...
  $t trace names
  $t trace info $id

  $t unset $n key1 key2 key3...
  
  $t row notify $row ?flags? command 
  $t column notify $column ?flags? command arg arg arg 
  $t notify create -oncreate -ondelete -onmove command 
  $t notify create -oncreate -ondelete -onmove -onsort command arg arg arg 
  $t notify delete id1 id2 id3
  $t notify names
  $t notify info id

  for { set n [$t firstchild $node] } { $n >= 0 } { 
        set n [$t nextsibling $n] } {
  }
  foreach n [$t children $node] { 
	  
  }
  set n [$t next $node]
  set n [$t previous $node]
*/

/*
 *
 *  datatable create ?name?
 *  datatable names
 *  datatable destroy $table
 *  
 *  $t column label ?newLabel?
 *  $t column labels ?index newLabel...?
 * 
 *  $t column index $c
 *  $t column indices $c
 *  $t column get $c
 *  $t column delete $r $r $r $r
 *  $t column extend label label label... 
 *  $t column get $column
 *  $t column insert $column "label"
 *  $t column select $expr
 *  $t column set {1 20} $list
 *  $t column set 1-20 $list
 *  $t column set 1:20 $list
 *  $t column trace $row rwu proc
 *  $t column unset $column
 *  $t copy fromTable
 *  $t copy newTable
 *  $t dup newTable -includerows $tag $tag $tag -excludecolumns $tag $tag 
 *  $t get $row $column $value ?$defValue?
 *  $t pack 
 *  $t row delete row row row row 
 *  $t row create -tags $tags -label $label -before -after 
 *  $t row extend 1
 *  $t row get $row -nolabels 
 *  $t row create $column "label" "label"
 *  $t row select $expr
 *  $t row set $row  $list -nolabels
 *  $t row trace $row  rwu proc
 *  $t row unset $row
 *  $t row tag add where expr 
 *  $t row tag forget $tag $tag
 *  $t row tag delete $tag $row $row $row
 *  $t row tag find $row ?pattern*?
 *  $t row tag add $row tag tag tag tag 
 *  $t row tag unset $row tag tag tag 
 *  $t row find $expr
 *  $t row unique $rowexpr
 *  $t select $expr
 *  $t set $row $column $value 
 *  $t trace $row $column rwu proc
 *  $t unset $row $column 
 *  $t column find $expr
 *  $t import -format {csv} -overwrite fileName
 *  $t export -format {csv,xml} fileName
 *  $t dumpdata -deftags -deflabels -columns "tags" -rows "tags" string
 *  $t dump -notags -nolabels -columns "tags" -rows "tags" fileName
 *  $t dup $destTable -notags -nolabels -columns "tags" -rows "tags"
 *  $t restore -overwrite -notags -nolabels string
 *  $t restoredata -overwrite -notags -nolabels -data string
 *  $t restore -overwrite -notags -nolabels fileName
 *
 *  $t row hide label
 *  $t row unhide label
 *
 *  $t emptyvalue ?value?
 *  $t key set ?key key key?
 *  $t key lookup ?key key key?
 */
/* 
 * $t import -format tree {tree 10} 
 * $t import -format csv $fileName.csv
 *
 * $t import tree $tree 10 ?switches?
 * $t import csv ?switches? fileName.xml
 * $t importdata csv -separator , -quote " $fileName.csv
 * $t import csv -separator , -quote " -data string
 * $t exportdata xml ?switches? ?$fileName.xml?
 * $t export csv -separator \t -quote ' $fileName.csv
 * $t export csv -separator \t -quote ' 
 * $t export -format dbm $fileName.dbm
 * $t export -format db $fileName.db
 * $t export -format csv $fileName.csv
 */
/*
 * $vector attach "$t c $column"
 * $vector detach 
 * $graph element create x -x "${table} column ${column}" \
 *		"table0 r abc"
 * $tree attach 0 $t
 */

#include <bltInt.h>
#include "bltOp.h"
#include <bltNsUtil.h>
#include <bltSwitch.h>
#include <bltHash.h>
#include <bltVar.h>

#include <bltDataTable.h>

#include <ctype.h>

#define TABLE_THREAD_KEY "BLT DataTable Command Interface"

/*
 * TableCmdInterpData --
 *
 *	Structure containing global data, used on a interpreter by interpreter
 *	basis.
 *
 *	This structure holds the hash table of instances of datatable commands
 *	associated with a particular interpreter.
 */
typedef struct {
    Blt_HashTable instTable;		/* Tracks tables in use. */
    Tcl_Interp *interp;
    Blt_HashTable fmtTable;
    Blt_HashTable findTable;		/* Tracks temporary "find" search
					 * information keyed by a specific
					 * namespace. */
} TableCmdInterpData;

typedef struct {
    long freq;				/* Frequency of value */
    const char *value;			/* String representation of value. */
} UniqueValue;

/*
 * Cmd --
 *
 *	Structure representing the TCL command used to manipulate the
 *	underlying table object.
 *
 *	This structure acts as a shell for a table object.  A table object
 *	maybe shared by more than one client.  This shell provides Tcl
 *	commands to access and change values as well as the structure of the
 *	table itself.  It manages the traces and notifier events that it
 *	creates, providing a TCL interface to those facilities. It also
 *	provides a user-selectable value for empty-cell values.
 */
typedef struct {
    Tcl_Interp *interp;			/* Interpreter this command is
					 * associated with. */
    Blt_Table table;			/* Handle representing the client
					 * table. */
    Tcl_Command cmdToken;		/* Token for TCL command representing
					 * this table. */
    const char *emptyValue;		/* String representing an empty value in
					 * the table. */
    Blt_HashTable *tablePtr;		/* Pointer to hash table containing a
					 * pointer to this structure.  Used to
					 * delete * this table entry from the
					 * table. */
    Blt_HashEntry *hPtr;		/* Pointer to the hash table entry for
					 * this table in the interpreter
					 * specific hash table. */
    int nextTraceId;			/* Used to generate trace id
					 * strings.  */
    Blt_HashTable traceTable;		/* Table of active traces. Maps trace
					 * ids back to their TraceInfo
					 * records. */
    int nextNotifyId;			/* Used to generate notify id
					 * strings. */
    Blt_HashTable notifyTable;		/* Table of event handlers. Maps
					 * notify ids back to their NotifyInfo
					 * records. */
} Cmd;

typedef struct {
    const char *name;			/* Name of format. */
    int isLoaded;			
    Blt_TableImportProc    *importProc;
    Blt_TableExportProc    *exportProc;
} DataFormat;


typedef struct {
    Tcl_Obj *cmd0;
    Tcl_Interp *interp;
} CompareData;

/*
 * TraceInfo --
 *
 *	Structure containing information about a trace set from this command
 *	shell.
 *
 *	This auxillary structure houses data to be used for a callback to a
 *	Tcl procedure when a table object trace fires.  It is stored in a hash
 *	table in the Dt_Cmd structure to keep track of traces issued by this
 *	shell.
 */
typedef struct {
    Blt_TableTrace trace;
    Cmd *cmdPtr;
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    int type;
    int cmdc;
    Tcl_Obj **cmdv;
} TraceInfo;

/*
 * NotifierInfo --
 *
 *	Structure containing information about a notifier set from
 *	this command shell.
 *
 *	This auxillary structure houses data to be used for a callback
 *	to a TCL procedure when a table object notify event fires.  It
 *	is stored in a hash table in the Dt_Cmd structure to keep
 *	track of notifiers issued by this shell.
 */
typedef struct {
    Blt_TableNotifier notifier;
    Cmd *cmdPtr;
    Blt_HashEntry *hPtr;
    int cmdc;
    Tcl_Obj **cmdv;
} NotifierInfo;

BLT_EXTERN Blt_SwitchFreeProc Blt_Table_ColumnIterFreeProc;
BLT_EXTERN Blt_SwitchFreeProc Blt_Table_RowIterFreeProc;
BLT_EXTERN Blt_SwitchParseProc Blt_Table_ColumnIterSwitchProc;
BLT_EXTERN Blt_SwitchParseProc Blt_Table_RowIterSwitchProc;
static Blt_SwitchParseProc TableSwitchProc;
static Blt_SwitchFreeProc TableFreeProc;
static Blt_SwitchParseProc PositionSwitch;
static Blt_SwitchParseProc TypeSwitchProc;

static Blt_SwitchCustom columnIterSwitch = {
    Blt_Table_ColumnIterSwitchProc, Blt_Table_ColumnIterFreeProc, 0,
};
static Blt_SwitchCustom rowIterSwitch = {
    Blt_Table_RowIterSwitchProc, Blt_Table_RowIterFreeProc, 0,
};
static Blt_SwitchCustom tableSwitch = {
    TableSwitchProc, TableFreeProc, 0,
};
static Blt_SwitchCustom typeSwitch = {
    TypeSwitchProc, NULL, 0,
};

#define INSERT_BEFORE	(ClientData)(1<<0)
#define INSERT_AFTER	(ClientData)(1<<1)

#define INSERT_ROW	(BLT_SWITCH_USER_BIT<<1)
#define INSERT_COL	(BLT_SWITCH_USER_BIT<<2)

static Blt_SwitchCustom beforeSwitch = {
    PositionSwitch, NULL, INSERT_BEFORE,
};
static Blt_SwitchCustom afterSwitch = {
    PositionSwitch, NULL, INSERT_AFTER,
};

typedef struct {
    Cmd *cmdPtr;
    Blt_TableRow row;			/* Index where to install new row or
					 * column. */
    Blt_TableColumn column;
    const char *label;			/* New label. */
    Tcl_Obj *tags;			/* List of tags to be applied to this
					 * row or column. */
    Blt_TableColumnType type;
} InsertSwitches;

static Blt_SwitchSpec insertSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-after",  "column",
	Blt_Offset(InsertSwitches, column), INSERT_COL, 0, &afterSwitch},
    {BLT_SWITCH_CUSTOM, "-after",  "row",
	Blt_Offset(InsertSwitches, row),    INSERT_ROW, 0, &afterSwitch},
    {BLT_SWITCH_CUSTOM, "-before", "column",
	Blt_Offset(InsertSwitches, column), INSERT_COL, 0, &beforeSwitch},
    {BLT_SWITCH_CUSTOM, "-before", "row",
         Blt_Offset(InsertSwitches, row),   INSERT_ROW, 0, &beforeSwitch},
    {BLT_SWITCH_STRING, "-label",  "string",
	Blt_Offset(InsertSwitches, label),  INSERT_ROW | INSERT_COL},
    {BLT_SWITCH_OBJ,    "-tags",   "tags",
	Blt_Offset(InsertSwitches, tags),   INSERT_ROW | INSERT_COL},
    {BLT_SWITCH_CUSTOM, "-type",   "type", 
	Blt_Offset(InsertSwitches, type),   INSERT_COL, 0, &typeSwitch},
    {BLT_SWITCH_END}
};

typedef struct {
    unsigned int flags;
    Blt_Table table;
} CopySwitches;

#define COPY_NOTAGS	(1<<1)
#define COPY_LABEL	(1<<3)

static Blt_SwitchSpec copySwitches[] = 
{
    {BLT_SWITCH_BITMASK, "-notags", "",
	Blt_Offset(CopySwitches, flags), 0, COPY_NOTAGS},
    {BLT_SWITCH_CUSTOM, "-table", "srcTable",
	Blt_Offset(CopySwitches, table), 0, 0, &tableSwitch},
    {BLT_SWITCH_END}
};

typedef struct {
    unsigned int flags;
} AddSwitches;

#define ADD_NOTAGS	(1<<1)

static Blt_SwitchSpec addSwitches[] = 
{
    {BLT_SWITCH_BITMASK, "-notags", "",
	Blt_Offset(CopySwitches, flags), 0, ADD_NOTAGS},
    {BLT_SWITCH_END}
};

typedef struct {
    /* Private data */
    Tcl_Channel channel;
    Tcl_DString *dsPtr;
    unsigned int flags;

    /* Public fields */
    Blt_TableIterator ri, ci;
    Tcl_Obj *fileObjPtr;
} DumpSwitches;

static Blt_SwitchSpec dumpSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-rows",    "rows",
	Blt_Offset(DumpSwitches, ri),      0, 0, &rowIterSwitch},
    {BLT_SWITCH_CUSTOM, "-columns", "columns",
	Blt_Offset(DumpSwitches, ci),      0, 0, &columnIterSwitch},
    {BLT_SWITCH_OBJ,    "-file",    "fileName",
	Blt_Offset(DumpSwitches, fileObjPtr), 0},
    {BLT_SWITCH_END}
};

typedef struct {
    Blt_HashTable idTable;

    Tcl_Obj *fileObjPtr;
    Tcl_Obj *dataObjPtr;
    unsigned int flags;
} RestoreSwitches;

static Blt_SwitchSpec restoreSwitches[] = 
{
    {BLT_SWITCH_OBJ, "-data", "string",
	Blt_Offset(RestoreSwitches, dataObjPtr), 0, 0},
    {BLT_SWITCH_OBJ, "-file", "fileName",
	Blt_Offset(RestoreSwitches, fileObjPtr), 0, 0},
    {BLT_SWITCH_BITMASK, "-notags", "",
	Blt_Offset(RestoreSwitches, flags), 0, TABLE_RESTORE_NO_TAGS},
    {BLT_SWITCH_BITMASK, "-overwrite", "",
	Blt_Offset(RestoreSwitches, flags), 0, TABLE_RESTORE_OVERWRITE},
    {BLT_SWITCH_END}
};

typedef struct {
    unsigned int flags;
} SortSwitches;

static Blt_SwitchSpec sortSwitches[] = 
{
    {BLT_SWITCH_BITMASK, "-decreasing", "",
	Blt_Offset(SortSwitches, flags), 0, SORT_DECREASING},
    {BLT_SWITCH_BITMASK, "-list", "",
	Blt_Offset(SortSwitches, flags), 0, SORT_LIST},
    {BLT_SWITCH_END}
};

typedef struct {
    unsigned int flags;
} NotifySwitches;

static Blt_SwitchSpec notifySwitches[] = 
{
    {BLT_SWITCH_BITMASK, "-allevents", "",
	Blt_Offset(NotifySwitches, flags), 0, TABLE_NOTIFY_ALL_EVENTS},
    {BLT_SWITCH_BITMASK, "-create", "",
	Blt_Offset(NotifySwitches, flags), 0, TABLE_NOTIFY_CREATE},
    {BLT_SWITCH_BITMASK, "-delete", "",
	Blt_Offset(NotifySwitches, flags), 0, TABLE_NOTIFY_DELETE},
    {BLT_SWITCH_BITMASK, "-move", "", 
	Blt_Offset(NotifySwitches, flags), 0, TABLE_NOTIFY_MOVE},
    {BLT_SWITCH_BITMASK, "-whenidle", "",
	Blt_Offset(NotifySwitches, flags), 0, TABLE_NOTIFY_WHENIDLE},
    {BLT_SWITCH_END}
};

typedef struct {
    Blt_Table table;			/* Table to be evaluated */
    Blt_TableRow row;			/* Current row. */
    Blt_HashTable varTable;		/* Variable cache. */
    Blt_TableIterator iter;

    /* Public values */
    Tcl_Obj *emptyValueObjPtr;
    const char *tag;
    unsigned int flags;
} FindSwitches;

#define FIND_INVERT	(1<<0)

static Blt_SwitchSpec findSwitches[] = 
{
    {BLT_SWITCH_CUSTOM,  "-rows",	"rows",
	Blt_Offset(FindSwitches, iter),  0, 0, &rowIterSwitch},
    {BLT_SWITCH_OBJ,     "-emptyvalue", "string",
	Blt_Offset(FindSwitches, emptyValueObjPtr), 0},
    {BLT_SWITCH_STRING,  "-addtag",	"tagName",
	Blt_Offset(FindSwitches, tag),   0},
    {BLT_SWITCH_BITMASK, "-invert",	"",
	Blt_Offset(FindSwitches, flags), 0, FIND_INVERT},
    {BLT_SWITCH_END}
};

typedef struct {
    int flags;
} UniqueSwitches;

static Blt_SwitchSpec uniqueSwitches[] = 
{
    {BLT_SWITCH_BITMASK,   "-ascii",	"",
	Blt_Offset(UniqueSwitches, flags),    0, SORT_ASCII},
    {BLT_SWITCH_BITMASK, "-decreasing", "",
	Blt_Offset(SortSwitches, flags),      0, SORT_DECREASING},
    {BLT_SWITCH_BITMASK,   "-dictionary", "",
	Blt_Offset(UniqueSwitches, flags),    0, SORT_DICTIONARY},
    {BLT_SWITCH_BITMASK, "-freq",       "",
	Blt_Offset(UniqueSwitches, flags),    0, SORT_FREQUENCY},
    {BLT_SWITCH_END}
};

static Blt_TableTraceProc TraceProc;
static Blt_TableTraceDeleteProc TraceDeleteProc;

static Blt_TableNotifyEventProc NotifyProc;
static Blt_TableNotifierDeleteProc NotifierDeleteProc;

static Tcl_CmdDeleteProc TableInstDeleteProc;
static Tcl_InterpDeleteProc TableInterpDeleteProc;
static Tcl_ObjCmdProc TableInstObjCmd;
static Tcl_ObjCmdProc TableObjCmd;

typedef int (CmdProc)(Cmd *cmdPtr, Tcl_Interp *interp, int objc, 
	Tcl_Obj *const *objv);

#ifdef notdef
static int
FirstOption(int objc, Tcl_Obj *const *objv, int start)
{
    int i;

    for (i = start; i < objc; i++) {
	const char *string;

	string = Tcl_GetString(objv[i]);
	if (string[0] == '-') {
	    break;
	}
    }
    return i;
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * PositionSwitch --
 *
 *	Convert a Tcl_Obj representing an offset in the table.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PositionSwitch(
    ClientData clientData,		/* Flag indicating if the node is
					 * considered before or after the
					 * insertion position. */
    Tcl_Interp *interp,			/* Interpreter to report results. */
    const char *switchName,		/* Not used. */
    Tcl_Obj *objPtr,			/* String representation */
    char *record,			/* Structure record */
    int offset,				/* Not used. */
    int flags)				/* Indicates whether this is a row or
					 * column index. */
{
    InsertSwitches *insertPtr = (InsertSwitches *)record;
    Blt_Table table;

    table = insertPtr->cmdPtr->table;
    if (flags & INSERT_COL) {
	Blt_TableColumn col;

	col = Blt_Table_FindColumn(interp, table, objPtr);
	if (col == NULL) {
	    return TCL_ERROR;
	}
	if (clientData == INSERT_AFTER) {
	    col = Blt_Table_NextColumn(table, col);
	}
	insertPtr->column = col;
    } else if (flags & INSERT_ROW) {
	Blt_TableRow row;

	row = Blt_Table_FindRow(interp, table, objPtr);
	if (row == NULL) {
	    return TCL_ERROR;
	}
	if (clientData == INSERT_AFTER) {
	    row = Blt_Table_NextRow(table, row);
	}
	insertPtr->row = row;
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * Blt_Table_ColumnIterFreeProc --
 *
 *	Free the storage associated with the -columns switch.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
void
Blt_Table_ColumnIterFreeProc(char *record, int offset, int flags)
{
    Blt_TableIterator *iterPtr = (Blt_TableIterator *)(record + offset);

    Blt_Table_FreeIteratorObjv(iterPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Table_ColumnIterSwitchProc --
 *
 *	Convert a Tcl_Obj representing an offset in the table.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_Table_ColumnIterSwitchProc(
    ClientData clientData,		/* Flag indicating if the node is
					 * considered before or after the
					 * insertion position. */
    Tcl_Interp *interp,			/* Interpreter to report results. */
    const char *switchName,		/* Not used. */
    Tcl_Obj *objPtr,			/* String representation */
    char *record,			/* Structure record */
    int offset,				/* Offset to field in structure */
    int flags)				/* Not used. */
{
    Blt_TableIterator *iterPtr = (Blt_TableIterator *)(record + offset);
    Blt_Table table;
    Tcl_Obj **objv;
    int objc;

    table = clientData;
    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_Table_IterateColumnsObjv(interp, table, objc, objv, iterPtr)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * FreeRowIter --
 *
 *	Free the storage associated with the -rows switch.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
void
Blt_Table_RowIterFreeProc(char *record, int offset, int flags)
{
    Blt_TableIterator *iterPtr = (Blt_TableIterator *)(record + offset);

    Blt_Table_FreeIteratorObjv(iterPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * RowIterSwitch --
 *
 *	Convert a Tcl_Obj representing an offset in the table.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_Table_RowIterSwitchProc(
    ClientData clientData,		/* Flag indicating if the node is
					 * considered before or after the
					 * insertion position. */
    Tcl_Interp *interp,			/* Interpreter to report results. */
    const char *switchName,		/* Not used. */
    Tcl_Obj *objPtr,			/* String representation */
    char *record,			/* Structure record */
    int offset,				/* Offset to field in structure */
    int flags)				/* Not used. */
{
    Blt_TableIterator *iterPtr = (Blt_TableIterator *)(record + offset);
    Blt_Table table;
    Tcl_Obj **objv;
    int objc;

    table = clientData;
    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_Table_IterateRowsObjv(interp, table, objc, objv, iterPtr)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TableSwitchProc --
 *
 *	Convert a Tcl_Obj representing an offset in the table.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TableSwitchProc(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Interpreter to report result. */
    const char *switchName,		/* Not used. */
    Tcl_Obj *objPtr,			/* String representation */
    char *record,			/* Structure record */
    int offset,				/* Offset to field in structure */
    int flags)				/* Not used. */
{
    Blt_Table *tablePtr = (Blt_Table *)(record + offset);
    Blt_Table table;

    if (Blt_Table_Open(interp, Tcl_GetString(objPtr), &table) != TCL_OK) {
	return TCL_ERROR;
    }
    *tablePtr = table;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TableFreeProc --
 *
 *	Free the storage associated with the -table switch.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
TableFreeProc(char *record, int offset, int flags)
{
    Blt_Table table = *(Blt_Table *)(record + offset);

    Blt_Table_Close(table);
}

/*
 *---------------------------------------------------------------------------
 *
 * TypeSwitchProc --
 *
 *	Convert a Tcl_Obj representing the type of the values in a table
 *	column.
 *
 * Results:
 *	The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TypeSwitchProc(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Interpreter to report results. */
    const char *switchName,		/* Not used. */
    Tcl_Obj *objPtr,			/* String representation */
    char *record,			/* Structure record */
    int offset,				/* Offset to field in structure */
    int flags)				/* Not used. */
{
    Blt_TableColumnType *typePtr = (Blt_TableColumnType *)(record + offset);
    Blt_TableColumnType type;

    type = Blt_Table_GetColumnType(Tcl_GetString(objPtr));
    if (type == TABLE_COLUMN_TYPE_UNKNOWN) {
	Tcl_AppendResult(interp, "unknown table column type \"",
		Tcl_GetString(objPtr), "\"", (char *)NULL);
	return TCL_OK;
    }
    *typePtr = type;
    return TCL_OK;
}

static int
MakeRows(Tcl_Interp *interp, Blt_Table table, Tcl_Obj *objPtr)
{
    const char *string;
    Blt_TableRowColumnSpec spec;
    long n;

    spec = Blt_Table_RowSpec(table, objPtr, &string);
    switch(spec) {
    case TABLE_SPEC_UNKNOWN:
    case TABLE_SPEC_LABEL:
	Tcl_ResetResult(interp);
	if (Blt_Table_CreateRow(interp, table, string) == NULL) {
	    return TCL_ERROR;
	}
	break;
    case TABLE_SPEC_INDEX:
	Tcl_ResetResult(interp);
	if (Tcl_GetLong(interp, string, &n) != TCL_OK) {
	    return TCL_ERROR;
	}
	n -= Blt_Table_NumRows(table);
	Blt_Table_ExtendRows(interp, table, n, NULL);
	break;
    default:
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
CompareValueFreq(const void *a, const void *b)
{
    const UniqueValue *fPtr1, *fPtr2;

    fPtr1 = a; 
    fPtr2 = b;
    return (fPtr1->freq - fPtr2->freq);
}

static int
CompareValueAscii(const void *a, const void *b)
{
    const UniqueValue *fPtr1, *fPtr2;

    fPtr1 = a; 
    fPtr2 = b;
    return strcmp(fPtr1->value, fPtr2->value);
}

static int
CompareValueDictionary(const void *a, const void *b)
{
    const UniqueValue *fPtr1, *fPtr2;

    fPtr1 = a; 
    fPtr2 = b;
    return Blt_DictionaryCompare(fPtr1->value, fPtr2->value);
}

static Tcl_Obj *
SortColumnValues(Tcl_Interp *interp, Blt_HashTable *tablePtr, int flags)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    QSortCompareProc *proc;
    Tcl_Obj *listObjPtr;
    UniqueValue *p, *pend, *array;
    long i;

    i = 0;
    array = Blt_Malloc(tablePtr->numEntries * sizeof(UniqueValue));
    if (array == NULL) {
	return NULL;
    }
    p = array;
    for (hPtr = Blt_FirstHashEntry(tablePtr, &iter); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&iter)) {
	p->value = Blt_GetHashKey(tablePtr, hPtr);
	p->freq = (long)Blt_GetHashValue(hPtr);
	p++;
    }
    switch (flags & SORT_TYPE_MASK) {
    case SORT_ASCII:
	proc = CompareValueAscii;	break;
    case SORT_FREQUENCY:
	proc = CompareValueFreq;	break;

    case SORT_DICTIONARY:
	proc = CompareValueDictionary;	break;
    case SORT_NONE:
	proc = NULL;			break;
    }
    if (proc != NULL) {
	qsort(array, tablePtr->numEntries, sizeof(UniqueValue), proc);
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    if (flags & SORT_DECREASING) {
	for (p = array + tablePtr->numEntries, pend = array; p > pend;) {
	    Tcl_Obj *objPtr;
	    
	    p--;
	    objPtr = Tcl_NewStringObj(p->value, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    } else {
	for (p = array, pend = p + tablePtr->numEntries; p < pend; p++) {
	    Tcl_Obj *objPtr;
	    
	    objPtr = Tcl_NewStringObj(p->value, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    Blt_Free(array);
    return listObjPtr;
}


static int
IterateRows(Tcl_Interp *interp, Blt_Table table, Tcl_Obj *objPtr, 
	    Blt_TableIterator *iterPtr)
{
    if (Blt_Table_IterateRows(interp, table, objPtr, iterPtr) != TCL_OK) {
	/* 
	 * We could not parse the row descriptor. If the row specification is
	 * a label or index that doesn't exist, create the new rows and try to
	 * load the iterator again.
	 */
	if (MakeRows(interp, table, objPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Blt_Table_IterateRows(interp, table, objPtr, iterPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

static int
MakeColumns(Tcl_Interp *interp, Blt_Table table, Tcl_Obj *objPtr)
{
    const char *string;
    Blt_TableRowColumnSpec spec;
    long n;

    spec = Blt_Table_ColumnSpec(table, objPtr, &string);
    switch(spec) {
    case TABLE_SPEC_UNKNOWN:
    case TABLE_SPEC_LABEL:
	Tcl_ResetResult(interp);
	if (Blt_Table_CreateColumn(interp, table, string) == NULL) {
	    return TCL_ERROR;
	}
	break;
    case TABLE_SPEC_INDEX:
	Tcl_ResetResult(interp);
	if (Tcl_GetLong(interp, string, &n) != TCL_OK) {
	    return TCL_ERROR;
	}
	n -= Blt_Table_NumColumns(table);
	Blt_Table_ExtendColumns(interp, table, n, NULL);
	break;
    default:
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
IterateColumns(Tcl_Interp *interp, Blt_Table table, Tcl_Obj *objPtr, 
	       Blt_TableIterator *iterPtr)
{
    if (Blt_Table_IterateColumns(interp, table, objPtr, iterPtr) != TCL_OK) {
	/* 
	 * We could not parse column descriptor.  If the column specification
	 * is a label that doesn't exist, create a new column with that label
	 * and try to load the iterator again.
	 */
	if (MakeColumns(interp, table, objPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Blt_Table_IterateColumns(interp, table, objPtr, iterPtr) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetTableCmdInterpData --
 *
 *---------------------------------------------------------------------------
 */

static TableCmdInterpData *
GetTableCmdInterpData(Tcl_Interp *interp)
{
    TableCmdInterpData *dataPtr;
    Tcl_InterpDeleteProc *proc;

    dataPtr = (TableCmdInterpData *)
	Tcl_GetAssocData(interp, TABLE_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_AssertMalloc(sizeof(TableCmdInterpData));
	dataPtr->interp = interp;
	Tcl_SetAssocData(interp, TABLE_THREAD_KEY, TableInterpDeleteProc, 
		dataPtr);
	Blt_InitHashTable(&dataPtr->instTable, BLT_STRING_KEYS);
	Blt_InitHashTable(&dataPtr->fmtTable, BLT_STRING_KEYS);
	Blt_InitHashTable(&dataPtr->findTable, BLT_ONE_WORD_KEYS);
    }
    return dataPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * NewTableCmd --
 *
 *	This is a helper routine used by TableCreateOp.  It create a
 *	new instance of a table command.  Memory is allocated for the
 *	command structure and a new TCL command is created (same as
 *	the instance name).  All table commands have hash table
 *	entries in a global (interpreter-specific) registry.
 *	
 * Results:
 *	Returns a pointer to the newly allocated table command structure.
 *
 * Side Effects:
 *	Memory is allocated for the structure and a hash table entry is
 *	added.  
 *
 *---------------------------------------------------------------------------
 */
static Cmd *
NewTableCmd(Tcl_Interp *interp, Blt_Table table, const char *name)
{
    Cmd *cmdPtr;
    TableCmdInterpData *dataPtr;
    int isNew;

    cmdPtr = Blt_AssertCalloc(1, sizeof(Cmd));
    cmdPtr->table = table;
    cmdPtr->interp = interp;
    cmdPtr->emptyValue = Blt_AssertStrdup("");

    Blt_InitHashTable(&cmdPtr->traceTable, BLT_STRING_KEYS);
    Blt_InitHashTable(&cmdPtr->notifyTable, BLT_STRING_KEYS);

    cmdPtr->cmdToken = Tcl_CreateObjCommand(interp, name, TableInstObjCmd, 
	cmdPtr, TableInstDeleteProc);
    dataPtr = GetTableCmdInterpData(interp);
    cmdPtr->tablePtr = &dataPtr->instTable;
    cmdPtr->hPtr = Blt_CreateHashEntry(&dataPtr->instTable, name, &isNew);
    Blt_SetHashValue(cmdPtr->hPtr, cmdPtr);
    return cmdPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * GenerateName --
 *
 *	Generates an unique table command name.  Table names are in the form
 *	"datatableN", where N is a non-negative integer. Check each name
 *	generated to see if it is already a table. We want to recycle names if
 *	possible.
 *	
 * Results:
 *	Returns the unique name.  The string itself is stored in the dynamic
 *	string passed into the routine.
 *
 *---------------------------------------------------------------------------
 */
static const char *
GenerateName(Tcl_Interp *interp, const char *prefix, const char *suffix,
	     Tcl_DString *resultPtr)
{

    int n;
    const char *instName;

    /* 
     * Parse the command and put back so that it's in a consistent format.
     *
     *	t1         <current namespace>::t1
     *	n1::t1     <current namespace>::n1::t1
     *	::t1	   ::t1
     *  ::n1::t1   ::n1::t1
     */
    instName = NULL;			/* Suppress compiler warning. */
    for (n = 0; n < INT_MAX; n++) {
	Blt_ObjectName objName;
	Tcl_CmdInfo cmdInfo;
	Tcl_DString ds;
	char string[200];

	Tcl_DStringInit(&ds);
	Tcl_DStringAppend(&ds, prefix, -1);
	sprintf_s(string, 200, "datatable%d", n);
	Tcl_DStringAppend(&ds, string, -1);
	Tcl_DStringAppend(&ds, suffix, -1);
	if (!Blt_ParseObjectName(interp, Tcl_DStringValue(&ds), 
				 &objName, 0)) {
	    return NULL;
	}
	instName = Blt_MakeQualifiedName(&objName, resultPtr);
	Tcl_DStringFree(&ds);
	/* 
	 * Check if the command already exists. 
	 */
	if (Tcl_GetCommandInfo(interp, (char *)instName, &cmdInfo)) {
	    continue;
	}
	if (!Blt_Table_TableExists(interp, instName)) {
	    /* 
	     * We want the name of the table command and the underlying table
	     * object to be the same. Check that the free command name isn't
	     * an already a table object name.
	     */
	    break;
	}
    }
    return instName;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetTableCmd --
 *
 *	Find the table command associated with the TCL command "string".
 *	
 *	We have to perform multiple lookups to get this right.  
 *
 *	The first step is to generate a canonical command name.  If an
 *	unqualified command name (i.e. no namespace qualifier) is given, we
 *	should search first the current namespace and then the global one.
 *	Most TCL commands (like Tcl_GetCmdInfo) look only at the global
 *	namespace.
 *
 *	Next check if the string is 
 *		a) a TCL command and 
 *		b) really is a command for a table object.  
 *	Tcl_GetCommandInfo will get us the objClientData field that should be
 *	a cmdPtr.  We verify that by searching our hashtable of cmdPtr
 *	addresses.
 *
 * Results:
 *	A pointer to the table command.  If no associated table command can be
 *	found, NULL is returned.  It's up to the calling routines to generate
 *	an error message.
 *
 *---------------------------------------------------------------------------
 */
static Cmd *
GetTableCmd(Tcl_Interp *interp, const char *name)
{
    Blt_HashEntry *hPtr;
    Tcl_DString ds;
    TableCmdInterpData *dataPtr;
    Blt_ObjectName objName;
    const char *qualName;

    /* Put apart the table name and put is back together in a standard
     * format. */
    if (!Blt_ParseObjectName(interp, name, &objName, BLT_NO_ERROR_MSG)) {
	return NULL;		/* No such parent namespace. */
    }
    /* Rebuild the fully qualified name. */
    qualName = Blt_MakeQualifiedName(&objName, &ds);
    dataPtr = GetTableCmdInterpData(interp);
    hPtr = Blt_FindHashEntry(&dataPtr->instTable, qualName);
    Tcl_DStringFree(&ds);
    if (hPtr == NULL) {
	return NULL;
    }
    return Blt_GetHashValue(hPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * GetTraceFlags --
 *
 *	Parses a string representation of the trace bit flags and returns the
 *	mask.
 *
 * Results:
 *	The trace mask is returned.
 *
 *---------------------------------------------------------------------------
 */
static int
GetTraceFlags(const char *string)
{
    const char *p;
    unsigned int flags;

    flags = 0;
    for (p = string; *p != '\0'; p++) {
	switch (toupper(UCHAR(*p))) {
	case 'R':
	    flags |= TABLE_TRACE_READS;	break;
	case 'W':
	    flags |= TABLE_TRACE_WRITES;	break;
	case 'U':
	    flags |= TABLE_TRACE_UNSETS;	break;
	case 'C':
	    flags |= TABLE_TRACE_CREATES;	break;
	default:
	    return -1;
	}
    }
    return flags;
}

/*
 *---------------------------------------------------------------------------
 *
 * PrintTraceFlags --
 *
 *	Generates a string representation of the trace bit flags.  It's
 *	assumed that the provided string is at least 5 bytes.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The bitflag information is written to the provided string.
 *
 *---------------------------------------------------------------------------
 */
static void
PrintTraceFlags(unsigned int flags, char *string)
{
    char *p;

    p = string;
    if (flags & TABLE_TRACE_READS) {
	*p++ = 'r';
    } 
    if (flags & TABLE_TRACE_WRITES) {
	*p++ = 'w';
    } 
    if (flags & TABLE_TRACE_UNSETS) {
	*p++ = 'u';
    } 
    if (flags & TABLE_TRACE_CREATES) {
	*p++ = 'c';
    } 
    *p = '\0';
}

static void
PrintTraceInfo(Tcl_Interp *interp, TraceInfo *tiPtr, Tcl_Obj *listObjPtr)
{
    Tcl_Obj *objPtr;
    int i;
    char string[5];
    struct _Blt_TableTrace *tracePtr;
    Blt_Table table;

    table = tiPtr->cmdPtr->table;
    Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewStringObj("id", 2));
    Tcl_ListObjAppendElement(interp, listObjPtr, 
			     Tcl_NewStringObj(tiPtr->hPtr->key.string, -1));
    tracePtr = tiPtr->trace;
    if (tracePtr->rowTag != NULL) {
	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewStringObj("row", 3));
	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewStringObj(tracePtr->rowTag, -1));
    }
    if (tracePtr->row != NULL) {
	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewStringObj("row", 3));
	Tcl_ListObjAppendElement(interp, listObjPtr, 
			Tcl_NewLongObj(Blt_Table_RowIndex(tracePtr->row)));
    }
    if (tracePtr->colTag != NULL) {
	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewStringObj("column", 6));
	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewStringObj(tracePtr->colTag, -1));
    }
    if (tracePtr->column != NULL) {
	Tcl_ListObjAppendElement(interp, listObjPtr, 
				 Tcl_NewStringObj("column", 6));
	Tcl_ListObjAppendElement(interp, listObjPtr, 
		Tcl_NewLongObj(Blt_Table_ColumnIndex(tracePtr->column)));
    }
    PrintTraceFlags(tracePtr->flags, string);
    Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewStringObj("flags", 5));
    Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewStringObj(string, -1));

    Tcl_ListObjAppendElement(interp, listObjPtr, 
			     Tcl_NewStringObj("command", 7));
    objPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (i = 0; i < tiPtr->cmdc; i++) {
	Tcl_ListObjAppendElement(interp, objPtr, tiPtr->cmdv[i]);
    }
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * FreeNotifierInfo --
 *
 *	This is a helper routine used to delete notifiers.  It releases the
 *	Tcl_Objs used in the notification callback command and the actual
 *	table notifier.  Memory for the notifier is also freed.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Memory is deallocated and the notitifer is no longer active.
 *
 *---------------------------------------------------------------------------
 */
static void
FreeNotifierInfo(NotifierInfo *niPtr)
{
    int i;

    for (i = 0; i < niPtr->cmdc; i++) {
	Tcl_DecrRefCount(niPtr->cmdv[i]);
    }
    Blt_Table_DeleteNotifier(niPtr->notifier);
    Blt_Free(niPtr->cmdv);
    Blt_Free(niPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TraceProc --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TraceProc(ClientData clientData, Blt_TableTraceEvent *eventPtr)
{
    TraceInfo *tracePtr = clientData; 
    char string[5];
    int result;
    int i;

    i = tracePtr->cmdc;		/* Add extra command arguments starting at
				 * this index. */
    tracePtr->cmdv[i+1]=Tcl_NewLongObj(Blt_Table_RowIndex(eventPtr->row));
    tracePtr->cmdv[i+2]=Tcl_NewLongObj(Blt_Table_ColumnIndex(eventPtr->column));

    PrintTraceFlags(eventPtr->mask, string);
    tracePtr->cmdv[i+3] = Tcl_NewStringObj(string, -1);

    Tcl_IncrRefCount(tracePtr->cmdv[i+1]);
    Tcl_IncrRefCount(tracePtr->cmdv[i+2]);
    Tcl_IncrRefCount(tracePtr->cmdv[i+3]);
    result = Tcl_EvalObjv(eventPtr->interp, i + 4, tracePtr->cmdv, 0);
    Tcl_DecrRefCount(tracePtr->cmdv[i+3]);
    Tcl_DecrRefCount(tracePtr->cmdv[i+2]);
    Tcl_DecrRefCount(tracePtr->cmdv[i+1]);
    if (result != TCL_OK) {
	Tcl_BackgroundError(eventPtr->interp);
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * TraceDeleteProc --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
TraceDeleteProc(ClientData clientData)
{
    TraceInfo *tiPtr = clientData; 
    int i;

    for (i = 0; i <= tiPtr->cmdc; i++) {
	Tcl_DecrRefCount(tiPtr->cmdv[i]);
    }
    Blt_Free(tiPtr->cmdv);
    if (tiPtr->hPtr != NULL) {
	Blt_DeleteHashEntry(tiPtr->tablePtr, tiPtr->hPtr);
    }
    Blt_Free(tiPtr);
}

static const char *
GetEventName(int type)
{
    if (type & TABLE_NOTIFY_CREATE) {
	return "-create";
    } 
    if (type & TABLE_NOTIFY_DELETE) {
	return "-delete";
    }
    if (type & TABLE_NOTIFY_MOVE) {
	return "-move";
    }
    return "???";
}

/*
 *---------------------------------------------------------------------------
 *
 * NotifierDeleteProc --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
NotifierDeleteProc(ClientData clientData)
{
    NotifierInfo *niPtr; 

    niPtr = clientData; 
    FreeNotifierInfo(niPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * NotifyProc --
 *
 *---------------------------------------------------------------------------
 */
static int
NotifyProc(ClientData clientData, Blt_TableNotifyEvent *eventPtr)
{
    NotifierInfo *niPtr; 
    Tcl_Interp *interp;
    int i, result;
    long index;

    niPtr = clientData; 
    interp = niPtr->cmdPtr->interp;
    i = niPtr->cmdc;
    niPtr->cmdv[i] = Tcl_NewStringObj(GetEventName(eventPtr->type), -1);

    if (eventPtr->type & TABLE_NOTIFY_ROW) {
	index = Blt_Table_RowIndex(eventPtr->header);
    } else {
	index = Blt_Table_ColumnIndex(eventPtr->header);
    }	
    niPtr->cmdv[i+1] = Tcl_NewLongObj(index);
    Tcl_IncrRefCount(niPtr->cmdv[i]);
    Tcl_IncrRefCount(niPtr->cmdv[i+1]);
    Tcl_IncrRefCount(niPtr->cmdv[i+2]);
    result = Tcl_EvalObjv(interp, niPtr->cmdc + 3, niPtr->cmdv, 0);
    Tcl_DecrRefCount(niPtr->cmdv[i+2]);
    Tcl_DecrRefCount(niPtr->cmdv[i+1]);
    Tcl_DecrRefCount(niPtr->cmdv[i]);
    if (result != TCL_OK) {
	Tcl_BackgroundError(interp);
	return TCL_ERROR;
    }
    Tcl_ResetResult(interp);
    return TCL_OK;
}

static int
ColumnVarResolver(
    Tcl_Interp *interp,			/* Current interpreter. */
    const char *name,			/* Variable name being resolved. */
    Tcl_Namespace *nsPtr,		/* Current namespace context. */
    int flags,				/* TCL_LEAVE_ERR_MSG => leave error
					 * message. */
    Tcl_Var *varPtr)			/* (out) Resolved variable. */ 
{
    Blt_HashEntry *hPtr;
    Blt_TableColumn col;
    FindSwitches *findPtr;
    TableCmdInterpData *dataPtr;
    Tcl_Interp *errInterp;
    Tcl_Obj *valueObjPtr, *nameObjPtr;

    dataPtr = GetTableCmdInterpData(interp);
    hPtr = Blt_FindHashEntry(&dataPtr->findTable, nsPtr);
    if (hPtr == NULL) {
	/* This should never happen.  We can't find in data associated with
	 * the current namespace.  But this routine should never be called
	 * unless we're in a namespace that with linked with this variable
	 * resolver. */
	return TCL_CONTINUE;	
    }
    findPtr = Blt_GetHashValue(hPtr);

    if (flags & TCL_LEAVE_ERR_MSG) {
	errInterp = interp;
    } else {
	errInterp = NULL;
    }
    /* Look up the column from the variable name given. */
    nameObjPtr = Tcl_NewStringObj(name, -1);
    col = Blt_Table_FindColumn(NULL, findPtr->table, nameObjPtr);
    Tcl_DecrRefCount(nameObjPtr);

    if (col == NULL) {
	/* Variable name doesn't refer to any column. Pass it back to the Tcl
	 * interpreter and let it resolve it normally. */
	return TCL_CONTINUE;
    }
    valueObjPtr = Blt_Table_GetObj(findPtr->table, findPtr->row, col);
    if (valueObjPtr == NULL) {
	valueObjPtr = findPtr->emptyValueObjPtr;
	if (valueObjPtr == NULL) {
	    return TCL_CONTINUE;
	}
    }
    Tcl_IncrRefCount(valueObjPtr);
    *varPtr = Blt_GetCachedVar(&findPtr->varTable, name, valueObjPtr);
    return TCL_OK;
}

static int
EvaluateExpr(Tcl_Interp *interp, Blt_Table table, Tcl_Obj *exprObjPtr, 
	     FindSwitches *findPtr, int *boolPtr)
{
    Tcl_Obj *resultObjPtr;
    int bool;

    if (Tcl_ExprObj(interp, exprObjPtr, &resultObjPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_GetBooleanFromObj(interp, resultObjPtr, &bool) != TCL_OK) {
	return TCL_ERROR;
    }
    if (findPtr->flags & FIND_INVERT) {
	bool = !bool;
    }
    if ((bool) && (findPtr->tag != NULL)) {
	Blt_Table_SetRowTag(interp, table, findPtr->row, findPtr->tag);
    }
    Tcl_DecrRefCount(resultObjPtr);
    *boolPtr = bool;
    return TCL_OK;
}

static int
FindRows(Tcl_Interp *interp, Blt_Table table, Tcl_Obj *objPtr, 
	 FindSwitches *findPtr)
{
    Blt_HashEntry *hPtr;
    TableCmdInterpData *dataPtr;
    Tcl_CallFrame frame;
    Tcl_Namespace *nsPtr;
    const char *name;
    int isNew;
    int result = TCL_OK;

    name = Blt_Table_TableName(table);
    nsPtr = Tcl_FindNamespace(interp, name, NULL, TCL_GLOBAL_ONLY);
    if (nsPtr != NULL) {
	/* This limits us to only one expression evaluated per table at a
	 * time--no concurrent expressions in the same table.  Otherwise we
	 * need to generate unique namespace names. That's a bit harder with
	 * the current TCL namespace API. */
	Tcl_AppendResult(interp, "can't evaluate expression: namespace \"",
			 name, "\" exists.", (char *)NULL);
	return TCL_ERROR;
    }

    /* Create a namespace from which to evaluate the expression. */
    nsPtr = Tcl_CreateNamespace(interp, name, NULL, NULL);
    if (nsPtr == NULL) {
	return TCL_ERROR;
    }
    /* Register our variable resolver in this namespace to link table values
     * with TCL variables. */
    Tcl_SetNamespaceResolvers(nsPtr, (Tcl_ResolveCmdProc*)NULL,
        ColumnVarResolver, (Tcl_ResolveCompiledVarProc*)NULL);

    /* Make this namespace the current one.  */
    Tcl_PushCallFrame(interp, &frame, nsPtr, /* isProcCallFrame */ FALSE);

    dataPtr = GetTableCmdInterpData(interp);
    hPtr = Blt_CreateHashEntry(&dataPtr->findTable, (char *)nsPtr, &isNew);
    assert(isNew);
    Blt_SetHashValue(hPtr, findPtr);

    /* Now process each row, evaluating the expression. */
    {
	Blt_TableRow row;
	Tcl_Obj *listObjPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	for (row = Blt_Table_FirstTaggedRow(&findPtr->iter); row != NULL; 
	     row = Blt_Table_NextTaggedRow(&findPtr->iter)) {
	    int bool;
	    
	    findPtr->row = row;
	    result = EvaluateExpr(interp, table, objPtr, findPtr, &bool);
	    if (result != TCL_OK) {
		break;
	    }
	    if (bool) {
		Tcl_ListObjAppendElement(interp, listObjPtr, 
			Tcl_NewLongObj(Blt_Table_RowIndex(row)));
	    }
	}
	if (result != TCL_OK) {
	    Tcl_DecrRefCount(listObjPtr);
	} else {
	    Tcl_SetObjResult(interp, listObjPtr);
	}
    }
    /* Clean up. */
    Tcl_PopCallFrame(interp);
    Tcl_DeleteNamespace(nsPtr);
    Blt_DeleteHashEntry(&dataPtr->findTable, hPtr);
    Blt_FreeCachedVars(&findPtr->varTable);
    return result;
}

static int
CopyColumn(Tcl_Interp *interp, Blt_Table srcTable, Blt_Table destTable,
    Blt_TableColumn src,		/* Column in the source table. */
    Blt_TableColumn dest)		/* Column in the destination table. */
{
    long i;

    if ((Blt_Table_SameTableObject(srcTable, destTable)) && (src == dest)) {
	return TCL_OK;		/* Source and destination are the same. */
    }
    if (Blt_Table_NumRows(srcTable) >  Blt_Table_NumRows(destTable)) {
	long need;

	need = (Blt_Table_NumRows(srcTable) - Blt_Table_NumRows(destTable));
	if (Blt_Table_ExtendRows(interp, destTable, need, NULL) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    Blt_Table_SetColumnType(destTable, dest, Blt_Table_ColumnType(src));
    for (i = 1; i <= Blt_Table_NumRows(srcTable); i++) {
	Blt_TableRow row;
	Blt_TableValue value;

	row = Blt_Table_Row(srcTable, i);
	value = Blt_Table_GetValue(srcTable, row, src);
	if (value == NULL) {
	    continue;
	}
	row = Blt_Table_Row(destTable, i);
	if (Blt_Table_SetValue(destTable, row, dest, value) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}	    

static void
CopyColumnTags(Blt_Table srcTable, Blt_Table destTable,
	       Blt_TableColumn src,	/* Column in the source table. */
	       Blt_TableColumn dest)	/* Column in the destination table. */
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;

    /* Find all tags for with this column index. */
    for (hPtr = Blt_Table_FirstColumnTag(srcTable, &iter); hPtr != NULL; 
	 hPtr = Blt_NextHashEntry(&iter)) {
	Blt_HashTable *tablePtr;
	Blt_HashEntry *h2Ptr;
	
	tablePtr = Blt_GetHashValue(hPtr);
	h2Ptr = Blt_FindHashEntry(tablePtr, (char *)src);
	if (h2Ptr != NULL) {
	    /* We know the tag tables are keyed by strings, so we don't need
	     * to call Blt_GetHashKey or use the hash table pointer to
	     * retrieve the key. */
	    Blt_Table_SetColumnTag(NULL, destTable, dest, hPtr->key.string);
	}
    }
}	    

/************* Column Operations ****************/

/*
 *---------------------------------------------------------------------------
 *
 * ColumnCopyOp --
 *
 *	Copies the specified columns to the table.  A different table may be
 *	selected as the source.
 * 
 * Results:
 *	A standard TCL result. If the tag or column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *	$dest column copy $srccol $destcol ?-table srcTable?
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnCopyOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    CopySwitches switches;
    Blt_Table srcTable, destTable;
    int result;
    Blt_TableColumn src, dest;

    /* Process switches following the column names. */
    memset(&switches, 0, sizeof(switches));
    result = TCL_ERROR;
    if (Blt_ParseSwitches(interp, copySwitches, objc - 5, objv + 5, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	goto error;
    }
    srcTable = destTable = cmdPtr->table;
    if (switches.table != NULL) {
	srcTable = switches.table;
    }
    src = Blt_Table_FindColumn(interp, srcTable, objv[3]);
    if (src == NULL) {
	goto error;
    }
    dest = Blt_Table_FindColumn(interp, destTable, objv[4]);
    if (dest == NULL) {
	dest = Blt_Table_CreateColumn(interp, destTable, 
		Tcl_GetString(objv[4]));
	if (dest == NULL) {
	    goto error;
	}
    }
    if (CopyColumn(interp, srcTable, destTable, src, dest) != TCL_OK) {
	goto error;
    }
    if ((switches.flags & COPY_NOTAGS) == 0) {
	CopyColumnTags(srcTable, destTable, src, dest);
    }
    result = TCL_OK;
 error:
    Blt_FreeSwitches(copySwitches, &switches, 0);
    return result;
    
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnDeleteOp --
 *
 *	Deletes the columns designated.  One or more columns may be deleted
 *	using a tag.
 * 
 * Results:
 *	A standard TCL result. If the tag or column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *	$t column delete ?column?...
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnDeleteOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_TableIterator iter;
    Blt_TableColumn col;
    int result;

    result = TCL_ERROR;
    if (Blt_Table_IterateColumnsObjv(interp, cmdPtr->table, objc - 3, objv + 3, 
	&iter) != TCL_OK) {
	return TCL_ERROR;
    }
    /* 
     * Walk through the list of column offsets, deleting each column.
     */
    for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
	 col = Blt_Table_NextTaggedColumn(&iter)) {
	if (Blt_Table_DeleteColumn(cmdPtr->table, col) != TCL_OK) {
	    goto error;
	}
    }
    result = TCL_OK;
 error:
    Blt_Table_FreeIteratorObjv(&iter);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnDupOp --
 *
 *	Duplicates the specified columns in the table.  This differs from
 *	ColumnCopyOp, since the same table is the source and destination.
 * 
 * Results:
 *	A standard TCL result. If the tag or column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *	$dest column dup column... 
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnDupOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Tcl_Obj *listObjPtr;
    Blt_TableIterator iter;
    Blt_TableColumn src;

    table = cmdPtr->table;
    listObjPtr = NULL;
    if (Blt_Table_IterateColumnsObjv(interp, table, objc - 3, objv + 3, &iter) 
	!= TCL_OK) {
	goto error;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (src = Blt_Table_FirstTaggedColumn(&iter); src != NULL; 
	 src = Blt_Table_NextTaggedColumn(&iter)) {
	long i;
	Blt_TableColumn dest;

	dest = Blt_Table_CreateColumn(interp, table,Blt_Table_ColumnLabel(src));
	if (dest == NULL) {
	    goto error;
	}
	if (CopyColumn(interp, table, table, src, dest) != TCL_OK) {
	    goto error;
	}
	CopyColumnTags(table, table, src, dest);
	i = Blt_Table_ColumnIndex(dest);
	Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewLongObj(i));
    }
    Blt_Table_FreeIteratorObjv(&iter);
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
 error:
    Blt_Table_FreeIteratorObjv(&iter);
    if (listObjPtr != NULL) {
	Tcl_DecrRefCount(listObjPtr);
    }
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnExistsOp --
 *
 *	Indicates is the given column exists.  The column description can be
 *	either an index, label, or single tag.
 *
 *	Problem: The Blt_Table_IterateColumns function checks both for
 *		 1) valid/invalid indices, labels, and tags and 2) 
 *		 syntax errors.
 * 
 * Results:
 *	A standard TCL result. If the tag or column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *	$t column exists n
 *	
 *---------------------------------------------------------------------------
 */
static int
ColumnExistsOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_TableColumn col;
    int bool;

    col = Blt_Table_FindColumn(NULL, cmdPtr->table, objv[3]);
    bool = (col != NULL);
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), bool);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnExtendOp --
 *
 *	Extends the table by the given number of columns.
 * 
 * Results:
 *	A standard TCL result. If the tag or column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *	$t column extend n 
 *	
 *---------------------------------------------------------------------------
 */
static int
ColumnExtendOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Tcl_Obj *listObjPtr;
    Blt_TableColumn *cols;
    long i, n;
    int addLabels;

    table = cmdPtr->table;
    if (objc == 3) {
	return TCL_OK;
    }
    n = 0;
    addLabels = FALSE;
    if (objc == 4) {
	long lcount;

	if (Tcl_GetLongFromObj(NULL, objv[3], &lcount) == TCL_OK) {
	    if (lcount < 0) {
		Tcl_AppendResult(interp, "bad count \"", Blt_Itoa(lcount), 
			"\": # columns can't be negative.", (char *)NULL);
		return TCL_ERROR;
	    }
	    n = lcount;
	} else {
	    addLabels = TRUE;
	    n = 1;
	} 
    } else {
	addLabels = TRUE;
	n = objc - 3;
    }	    
    if (n == 0) {
	return TCL_OK;
    }
    cols = Blt_AssertMalloc(n * sizeof(Blt_TableColumn));
    if (Blt_Table_ExtendColumns(interp, table, n, cols) != TCL_OK) {
	goto error;
    }
    if (addLabels) {
	long j;

	for (i = 0, j = 3; i < n; i++, j++) {
	    if (Blt_Table_SetColumnLabel(interp, table, cols[i], 
			Tcl_GetString(objv[j])) != TCL_OK) {
		goto error;
	    }
	}
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (i = 0; i < n; i++) {
	Tcl_Obj *objPtr;

	objPtr = Tcl_NewLongObj(Blt_Table_ColumnIndex(cols[i]));
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    Blt_Free(cols);
    return TCL_OK;
 error:
    Blt_Free(cols);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnGetOp --
 *
 *	Retrieves a column of values.  The column argument can be either a
 *	tag, label, or column index.  If it is a tag, it must refer to exactly
 *	one column.  If row arguments exist they must refer to label or row.
 *	We always return the row label.
 * 
 * Results:
 *	A standard TCL result.  If successful, a list of values is returned in
 *	the interpreter result.  If the column index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t column get -labels $c ?row...? 
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnGetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableColumn col;
    Tcl_Obj *listObjPtr;
    const char *string;
    int needLabels;

    string = Tcl_GetString(objv[3]);
    needLabels = FALSE;
    if (strcmp(string, "-labels") == 0) {
	objv++, objc--;
	needLabels = TRUE;
    }
    table = cmdPtr->table;
    col = Blt_Table_FindColumn(interp, cmdPtr->table, objv[3]);
    if (col == NULL) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    if (objc == 4) {
	Blt_TableRow row;

	for (row = Blt_Table_FirstRow(cmdPtr->table); row != NULL;
	     row = Blt_Table_NextRow(cmdPtr->table, row)) {
	    Tcl_Obj *objPtr;

	    if (needLabels) {
		objPtr = Tcl_NewStringObj(Blt_Table_RowLabel(row), -1);
	    } else {
		objPtr = Tcl_NewLongObj(Blt_Table_RowIndex(row));
	    }
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    objPtr = Blt_Table_GetObj(cmdPtr->table, row, col);
	    if (objPtr == NULL) {
		objPtr = Tcl_NewStringObj(cmdPtr->emptyValue, -1);
	    }
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    } else {
	Blt_TableIterator iter;
	Blt_TableRow row;

	if (Blt_Table_IterateRowsObjv(interp, table, objc - 4, objv + 4, &iter) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
	     row = Blt_Table_NextTaggedRow(&iter)) {
	    Tcl_Obj *objPtr;
	    
	    if (needLabels) {
		objPtr = Tcl_NewStringObj(Blt_Table_RowLabel(row), -1);
	    } else {
		objPtr = Tcl_NewLongObj(Blt_Table_RowIndex(row));
	    }
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    objPtr = Blt_Table_GetObj(cmdPtr->table, row, col);
	    if (objPtr == NULL) {
		objPtr = Tcl_NewStringObj(cmdPtr->emptyValue, -1);
	    }
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnIndexOp --
 *
 *	Returns the column index of the given column tag, label, or index.  A
 *	tag can't represent more than one column.
 * 
 * Results:
 *	A standard TCL result. If the tag or column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *	$t column index $column
 *	
 *---------------------------------------------------------------------------
 */
static int
ColumnIndexOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_TableColumn col;
    long i;

    i = -1;
    col = Blt_Table_FindColumn(NULL, cmdPtr->table, objv[3]);
    if (col != NULL) {
	i = Blt_Table_ColumnIndex(col);
    }
    Tcl_SetLongObj(Tcl_GetObjResult(interp), i);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnIndicesOp --
 *
 *	Returns a list of indices for the given columns.  
 * 
 * Results:
 *	A standard TCL result. If the tag or column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *	$t column indices $col $col
 *	
 *---------------------------------------------------------------------------
 */
static int
ColumnIndicesOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_Obj *listObjPtr;
    Blt_TableIterator iter;
    Blt_TableColumn col;

    if (Blt_Table_IterateColumnsObjv(interp, cmdPtr->table, objc - 3, objv + 3,
		&iter) != TCL_OK) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
	 col = Blt_Table_NextTaggedColumn(&iter)) {
	Tcl_Obj *objPtr;

	objPtr = Tcl_NewLongObj(Blt_Table_ColumnIndex(col));
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    Blt_Table_FreeIteratorObjv(&iter);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnCreateOp --
 *
 *	Creates a single new column in the table.  The location of the new
 *	column may be specified by -before or -after switches.  By default the
 *	new column is added to the end of the table.
 * 
 * Results:
 *	A standard TCL result. If the tag or column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *	$t column create -before 0 -after 1 -label label
 *	
 *---------------------------------------------------------------------------
 */
static int
ColumnCreateOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    InsertSwitches switches;
    Blt_TableColumn col;
    unsigned int flags;

    memset(&switches, 0, sizeof(switches));
    switches.cmdPtr = cmdPtr;
    switches.type = TABLE_COLUMN_TYPE_STRING;

    flags = INSERT_COL;
    if (Blt_ParseSwitches(interp, insertSwitches, objc - 3, objv + 3, 
	&switches, flags) < 0) {
	goto error;
    }
    col = Blt_Table_CreateColumn(interp, cmdPtr->table, switches.label);
    if (col == NULL) {
	goto error;
    }
    Blt_Table_SetColumnType(cmdPtr->table, col, switches.type);
    if (switches.column != NULL) {
	if (Blt_Table_MoveColumns(interp, cmdPtr->table, col, 
		switches.column, 1) != TCL_OK) {
	    goto error;
	}
    }
    if (switches.tags != NULL) {
	Tcl_Obj **elv;
	int elc;
	int i;

	if (Tcl_ListObjGetElements(interp, switches.tags, &elc, &elv) 
	    != TCL_OK) {
	    goto error;
	}
	for (i = 0; i < elc; i++) {
	    if (Blt_Table_SetColumnTag(interp, cmdPtr->table, col, 
			Tcl_GetString(elv[i])) != TCL_OK) {
		goto error;
	    }
	}
    }
    Tcl_SetObjResult(interp, Tcl_NewLongObj(Blt_Table_ColumnIndex(col)));
    Blt_FreeSwitches(insertSwitches, &switches, flags);
    return TCL_OK;
 error:
    Blt_FreeSwitches(insertSwitches, &switches, flags);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnLabelOp --
 *
 *	Gets/sets one or more column labels.  
 * 
 * Results:
 *	A standard TCL result.  If successful, the old column label is
 *	returned in the interpreter result.  If the column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *	
 * Example:
 *	$t column label column ?label? ?column label? 
 *	$t column label 1
 *	$t column label 1 newLabel 
 *	$t column label 1 lab1 2 lab2 3 lab3 5 lab5
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnLabelOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;

    table = cmdPtr->table;
    if (objc == 4) {
	const char *label;
	Blt_TableColumn col;

	col = Blt_Table_FindColumn(interp, table, objv[3]);
	if (col == NULL) {
	    return TCL_ERROR;
	}
	label = Blt_Table_ColumnLabel(col);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), label, -1);
    }  else {
	int i;
	
	if ((objc - 3) & 1) {
	    Tcl_AppendResult(interp,"odd # of column/label pairs: should be \"",
		Tcl_GetString(objv[0]), " column label ?column label?...", 
			     (char *)NULL);
	    return TCL_ERROR;
	}
	for (i = 3; i < objc; i += 2) {
	    Blt_TableColumn col;

	    col = Blt_Table_FindColumn(interp, table, objv[i]);
	    if (col == NULL) {
		return TCL_ERROR;
	    }
	    if (Blt_Table_SetColumnLabel(interp, table, col, 
			Tcl_GetString(objv[i+1])) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ColumnLabelsOp --
 *
 *	Gets/sets all the column labels in the table.  
 * 
 * Results:
 *	A standard TCL result.  If successful, a list of values is returned in
 *	the interpreter result.  
 *	
 * Example:
 *	$t column labels ?labelList?
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnLabelsOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;

    table = cmdPtr->table;
    if (objc == 3) {
	Blt_TableColumn col;
	Tcl_Obj *listObjPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	for (col = Blt_Table_FirstColumn(cmdPtr->table); col != NULL;
	     col = Blt_Table_NextColumn(cmdPtr->table, col)) {
	    const char *label;
	    Tcl_Obj *objPtr;
	    
	    label = Blt_Table_ColumnLabel(col);
	    objPtr = Tcl_NewStringObj(label, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	Tcl_SetObjResult(interp, listObjPtr);
    } else {
	Tcl_Obj **elv;
	int elc, n;
	int i;

	if (Tcl_ListObjGetElements(interp, objv[3], &elc, &elv) != TCL_OK) {
	    return TCL_ERROR;
	}
	n = MIN(elc, Blt_Table_NumColumns(table));
	for (i = 0; i < n; i++) {
	    Blt_TableColumn col;

	    col = Blt_Table_Column(table, i + 1);
	    if (Blt_Table_SetColumnLabel(interp, table, col, 
			Tcl_GetString(elv[i])) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnLengthOp --
 *
 *	Returns the number of columns the client sees.
 * 
 * Results:
 *	A standard TCL result.  If successful, the old column label is
 *	returned in the interpreter result.  If the column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *	
 * Example:
 *	$t column length 
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnLengthOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_SetLongObj(Tcl_GetObjResult(interp), 
		   Blt_Table_NumColumns(cmdPtr->table));
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ColumnMoveOp --
 *
 *	Moves the given number of columns to another location in the table.
 * 
 * Results:
 *	A standard TCL result. If the column index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t column move from to ?n?
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnMoveOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_TableColumn from, to;
    long count;

    from = Blt_Table_FindColumn(interp, cmdPtr->table, objv[3]);
    if (from == NULL) {
	return TCL_ERROR;
    }
    to = Blt_Table_FindColumn(interp, cmdPtr->table, objv[4]);
    if (to == NULL) {
	return TCL_ERROR;
    }
    count = 1;
    if (objc == 6) {
	long lcount;

	if (Tcl_GetLongFromObj(interp, objv[5], &lcount) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (lcount == 0) {
	    return TCL_OK;
	}
	if (lcount < 0) {
	    Tcl_AppendResult(interp, 
			"can't move columns: # of columns can't be negative",
			(char *)NULL);
	    return TCL_ERROR;
	}
	count = lcount;
    }
    return Blt_Table_MoveColumns(interp, cmdPtr->table, from, to, count);
}


/*
 *---------------------------------------------------------------------------
 *
 * ColumnNamesOp --
 *
 *	Reports the labels of all columns.  
 * 
 * Results:
 *	Always returns TCL_OK.  The interpreter result is a list of column
 *	labels.
 *	
 * Example:
 *	$t column names pattern 
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnNamesOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_Obj *listObjPtr;
    Blt_TableColumn col;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (col = Blt_Table_FirstColumn(cmdPtr->table); col != NULL;
	 col = Blt_Table_NextColumn(cmdPtr->table, col)) {
	const char *label;
	int match;
	int i;

	label = Blt_Table_ColumnLabel(col);
	match = (objc == 3);
	for (i = 3; i < objc; i++) {
	    char *pattern;

	    pattern = Tcl_GetString(objv[i]);
	    if (Tcl_StringMatch(label, pattern)) {
		match = TRUE;
		break;
	    }
	}
	if (match) {
	    Tcl_Obj *objPtr;

	    objPtr = Tcl_NewStringObj(label, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnNotifyOp --
 *
 *	Creates a notifier for this instance.  Notifiers represent a bitmask
 *	of events and a command prefix to be invoked when a matching event
 *	occurs.
 *
 *	The command prefix is parsed and saved in an array of Tcl_Objs. Extra
 *	slots are allocated for the
 *
 * Results:
 *	A standard TCL result.  The name of the new notifier is returned in
 *	the interpreter result.  Otherwise, if it failed to parse a switch,
 *	then TCL_ERROR is returned and an error message is left in the
 *	interpreter result.
 *
 * Example:
 *	table0 column notify col ?flags? command arg
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnNotifyOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableColumn col;
    Blt_TableRowColumnSpec spec;
    NotifierInfo *niPtr;
    NotifySwitches switches;
    const char *tag, *string;
    int count, i;
    int nArgs;

    table = cmdPtr->table;
    spec = Blt_Table_ColumnSpec(table, objv[3], &string);
    col = NULL;
    tag = NULL;
    if (spec == TABLE_SPEC_TAG) {
	tag = string;
    } else {
	col = Blt_Table_FindColumn(interp, table, objv[3]);
	if (col == NULL) {
	    return TCL_ERROR;
	}
    }
    count = 0;
    for (i = 4; i < objc; i++) {
	const char *string;

	string = Tcl_GetString(objv[i]);
	if (string[0] != '-') {
	    break;
	}
	count++;
    }
    switches.flags = 0;
    /* Process switches  */
    if (Blt_ParseSwitches(interp, notifySwitches, count, objv + 4, &switches, 
	0) < 0) {
	return TCL_ERROR;
    }
    niPtr = Blt_AssertMalloc(sizeof(NotifierInfo));
    niPtr->cmdPtr = cmdPtr;
    if (tag == NULL) {
	niPtr->notifier = Blt_Table_CreateColumnNotifier(interp, cmdPtr->table,
		col, switches.flags, NotifyProc, NotifierDeleteProc, niPtr);
    } else {
	niPtr->notifier = Blt_Table_CreateColumnTagNotifier(interp, 
		cmdPtr->table, tag, switches.flags, NotifyProc, 
		NotifierDeleteProc, niPtr);
    }	
    nArgs = (objc - i) + 2;
    /* Stash away the command in structure and pass that to the notifier. */
    niPtr->cmdv = Blt_AssertMalloc(nArgs * sizeof(Tcl_Obj *));
    for (count = 0; i < objc; i++, count++) {
	Tcl_IncrRefCount(objv[i]);
	niPtr->cmdv[count] = objv[i];
    }
    niPtr->cmdc = nArgs;
    if (switches.flags == 0) {
	switches.flags = TABLE_NOTIFY_ALL_EVENTS;
    }
    {
	char notifyId[200];
	Blt_HashEntry *hPtr;
	int isNew;

	sprintf_s(notifyId, 200, "notify%d", cmdPtr->nextNotifyId++);
	hPtr = Blt_CreateHashEntry(&cmdPtr->notifyTable, notifyId, &isNew);
	assert(isNew);
	Blt_SetHashValue(hPtr, niPtr);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), notifyId, -1);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnSetOp --
 *
 *	Sets one of values in a column.  One or more columns may be set using
 *	a tag.  The row order is always the table's current view of the table.
 *	There may be less values than needed.
 * 
 * Results:
 *	A standard TCL result. If the tag or column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *	
 * Example:
 *	$t column set $column a 1 b 2 c 3
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnSetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_TableIterator iter;
    Blt_TableColumn col;
    Blt_Table table;

    table = cmdPtr->table;
    /* May set more than one row with the same values. */
    if (IterateColumns(interp, table, objv[3], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 4) {
	return TCL_OK;
    }
    if ((objc - 4) & 1) {
	Tcl_AppendResult(interp, "odd # of row/value pairs: should be \"", 
		Tcl_GetString(objv[0]), " column assign col row value...", 
		(char *)NULL);
	return TCL_ERROR;
    }
    for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
	 col = Blt_Table_NextTaggedColumn(&iter)) {
	long i;

	/* The remaining arguments are index/value pairs. */
	for (i = 4; i < objc; i += 2) {
	    Blt_TableRow row;

	    row = Blt_Table_FindRow(interp, table, objv[i]);
	    if (row == NULL) {
		/* Can't find the row. Create it and try to find it again. */
		if (MakeRows(interp, table, objv[i]) != TCL_OK) {
		    return TCL_ERROR;
		}
		row = Blt_Table_FindRow(interp, table, objv[i]);
	    }
	    if (Blt_Table_SetObj(table, row, col, objv[i+1]) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ColumnTagAddOp --
 *
 *	Adds a given tag to one or more columns.  Tag names can't start with a
 *	digit (to distinquish them from node ids) and can't be a reserved tag
 *	("all" or "end").
 *
 *	.t column tag add tag ?column...?
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnTagAddOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    int i;
    const char *tagName;

    table = cmdPtr->table;
    tagName = Tcl_GetString(objv[4]);
    if (Blt_Table_SetColumnTag(interp, table, NULL, tagName) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 5; i < objc; i++) {
	Blt_TableColumn col;
	Blt_TableIterator iter;

	if (Blt_Table_IterateColumns(interp, table, objv[i], &iter) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
	     col = Blt_Table_NextTaggedColumn(&iter)) {
	    if (Blt_Table_SetColumnTag(interp, table, col, tagName) != TCL_OK) {
		return TCL_ERROR;
	    }
	}    
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ColumnTagDeleteOp --
 *
 *	Removes a given tag from one or more columns. If a tag doesn't exist or
 *	is a reserved tag ("all" or "end"), nothing will be done and no error
 *	message will be returned.
 *
 *	.t column tag delete tag ?column...?
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnTagDeleteOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, 
		  Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableIterator iter;
    int i;
    const char *tagName;

    table = cmdPtr->table;
    tagName = Tcl_GetString(objv[4]);
    for (i = 5; i < objc; i++) {
	Blt_TableColumn col;
	
	if (Blt_Table_IterateColumns(interp, table, objv[i], &iter) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
	     col = Blt_Table_NextTaggedColumn(&iter)) {
	    if (Blt_Table_UnsetColumnTag(interp, table, col, tagName)!=TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnTagExistsOp --
 *
 *	Returns the existence of a tag in the table.  If a column is 
 *	specified then the tag is search for for that column.
 *
 *	.t tag column exists tag ?column?
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColumnTagExistsOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, 
		  Tcl_Obj *const *objv)
{
    int bool;
    const char *tagName;
    Blt_Table table;

    tagName = Tcl_GetString(objv[3]);
    table = cmdPtr->table;
    bool = (Blt_Table_FindColumnTagTable(table, tagName) != NULL);
    if (objc == 5) {
	Blt_TableColumn col;

	col = Blt_Table_FindColumn(interp, table, objv[4]);
	if (col == NULL) {
	    bool = FALSE;
	} else {
	    bool = Blt_Table_HasColumnTag(table, col, tagName);
	}
    } 
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), bool);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnTagForgetOp --
 *
 *	Removes the given tags from all nodes.
 *
 * Example:
 *	$t column tag forget tag1 tag2 tag3...
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColumnTagForgetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, 
		  Tcl_Obj *const *objv)
{
    int i;

    for (i = 4; i < objc; i++) {
	if (Blt_Table_ForgetColumnTag(interp, cmdPtr->table, 
		Tcl_GetString(objv[i])) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnTagGetOp --
 *
 *	Returns the tag names for a given column.  If one of more pattern
 *	arguments are provided, then only those matching tags are returned.
 *
 *	.t column tag get column pat1 pat2...
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnTagGetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch hsearch;
    Blt_HashTable tagTable;
    Blt_Table table;
    Blt_TableColumn col;
    Blt_TableIterator iter;
    Tcl_Obj *listObjPtr;

    table = cmdPtr->table;
    if (Blt_Table_IterateColumns(interp, table, objv[4], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);

    Blt_InitHashTable(&tagTable, BLT_STRING_KEYS);
    
    /* Collect all the tags into a hash table. */
    for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
	 col = Blt_Table_NextTaggedColumn(&iter)) {
	Blt_Chain chain;
	Blt_ChainLink link;

	chain = Blt_Table_ColumnTags(table, col);
	for (link = Blt_Chain_FirstLink(chain); link != NULL;
	     link = Blt_Chain_NextLink(link)) {
	    const char *tagName;
	    int isNew;

	    tagName = Blt_Chain_GetValue(link);
	    Blt_CreateHashEntry(&tagTable, tagName, &isNew);
	}
	Blt_Chain_Destroy(chain);
    }
    for (hPtr = Blt_FirstHashEntry(&tagTable, &hsearch); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&hsearch)) {
	int match;
	const char *tagName;

	tagName = Blt_GetHashKey(&tagTable, hPtr);
	match = TRUE;
	if (objc > 5) {
	    int i;

	    match = FALSE;
	    for (i = 5; i < objc; i++) {
		if (Tcl_StringMatch(tagName, Tcl_GetString(objv[i]))) {
		    match = TRUE;
		}
	    }
	}
	if (match) {
	    Tcl_Obj *objPtr;

	    objPtr = Tcl_NewStringObj(tagName, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    Blt_DeleteHashTable(&tagTable);
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}


static unsigned char *
GetColumnTagMatches(Tcl_Interp *interp, Blt_Table table, int objc, 
		    Tcl_Obj *const *objv)
{
    long nCols;
    int i;
    unsigned char *matches;

    nCols = Blt_Table_NumColumns(table);
    matches = Blt_AssertCalloc(nCols + 1, sizeof(unsigned char));
    /* Handle the reserved tags "all" or "end". */
    for (i = 0; i < objc; i++) {
	char *tagName;
	long j;

	tagName = Tcl_GetString(objv[i]);
	if (strcmp("all", tagName) == 0) {
	    for (j = 1; j <= nCols; j++) {
		matches[j] = TRUE;
	    }
	    return matches;		/* Don't care other tags. */
	} 
	if (strcmp("end", tagName) == 0) {
	    matches[nCols] = TRUE;
	}
    }
    /* Now check user-defined tags. */
    for (i = 0; i < objc; i++) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch iter;
	Blt_HashTable *tagTablePtr;
	const char *tagName;
	
	tagName = Tcl_GetString(objv[i]);
	if ((strcmp("all", tagName) == 0) || (strcmp("end", tagName) == 0)) {
	    continue;
	}
	tagTablePtr = Blt_Table_FindColumnTagTable(table, tagName);
	if (tagTablePtr == NULL) {
	    Tcl_AppendResult(interp, "unknown column tag \"", tagName, "\"",
		(char *)NULL);
	    Blt_Free(matches);
	    return NULL;
	}
	for (hPtr = Blt_FirstHashEntry(tagTablePtr, &iter); hPtr != NULL; 
	     hPtr = Blt_NextHashEntry(&iter)) {
	    Blt_TableColumn col;
	    long j;

	    col = Blt_GetHashValue(hPtr);
	    j = Blt_Table_ColumnIndex(col);
	    assert(j >= 0);
	    matches[j] = TRUE;
	}
    }
    return matches;
}


/*
 *---------------------------------------------------------------------------
 *
 * ColumnTagIndicesOp --
 *
 *	Returns column indices names for the given tags.  If one of more tag
 *	names are provided, then only those matching indices are returned.
 *
 * Example:
 *	.t column tag indices tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnTagIndicesOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, 
		   Tcl_Obj *const *objv)
{
    Tcl_Obj *listObjPtr;
    long j;
    long nCols;
    unsigned char *matches;

    matches = GetColumnTagMatches(interp, cmdPtr->table, objc - 4, objv + 4);
    if (matches == NULL) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    nCols = Blt_Table_NumColumns(cmdPtr->table);
    for (j = 1; j <= nCols; j++) {
	if (matches[j]) {
	    Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewLongObj(j));
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    Blt_Free(matches);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnTagLabelsOp --
 *
 *	Returns column labels for the given tags.  If one of more tag
 *	names are provided, then only those matching indices are returned.
 *
 * Example:
 *	.t column tag labels tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnTagLabelsOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, 
		  Tcl_Obj *const *objv)
{
    Tcl_Obj *listObjPtr;
    long j;
    long nCols;
    unsigned char *matches;

    matches = GetColumnTagMatches(interp, cmdPtr->table, objc - 4, objv + 4);
    if (matches == NULL) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    nCols = Blt_Table_NumColumns(cmdPtr->table);
    for (j = 1; j <= nCols; j++) {
	if (matches[j]) {
	    Blt_TableColumn col;
	    Tcl_Obj *objPtr;
	    
	    col = Blt_Table_Column(cmdPtr->table, j);
	    objPtr = Tcl_NewStringObj(Blt_Table_ColumnLabel(col), -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    Blt_Free(matches);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ColumnTagRangeOp --
 *
 *	Adds one or more tags for a given column.  Tag names can't start with
 *	a digit (to distinquish them from node ids) and can't be a reserved
 *	tag ("all" or "end").
 *
 * Example:
 *	.t column tag range $from $to tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnTagRangeOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, 
		 Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableColumn from, to;
    int i;

    table = cmdPtr->table;
    from = Blt_Table_FindColumn(interp, table, objv[4]);
    if (from == NULL) {
	return TCL_ERROR;
    }
    to = Blt_Table_FindColumn(interp, table, objv[5]);
    if (to == NULL) {
	return TCL_ERROR;
    }
    if (Blt_Table_ColumnIndex(from) > Blt_Table_ColumnIndex(to)) {
	Blt_TableColumn tmp;
	tmp = to, to = from, from = tmp;
    }
    for (i = 6; i < objc; i++) {
	const char *tagName;
	long j;
	
	tagName = Tcl_GetString(objv[i]);
	for (j = Blt_Table_ColumnIndex(from); j <= Blt_Table_ColumnIndex(to); 
	     j++) {
	    Blt_TableColumn col;

	    col = Blt_Table_Column(table, j);
	    if (Blt_Table_SetColumnTag(interp, table, col, tagName) != TCL_OK) {
		return TCL_ERROR;
	    }
	}    
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnTagSearchOp --
 *
 *	Returns tag names for a given column.  If one of more pattern
 *	arguments are provided, then only those matching tags are returned.
 *
 * Example:
 *	.t column tag find $column pat1 pat2...
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnTagSearchOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, 
		  Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableIterator iter;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Tcl_Obj *listObjPtr;

    table = cmdPtr->table;
    if (Blt_Table_IterateColumns(interp, table, objv[4], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Blt_Table_FirstColumnTag(table, &cursor); hPtr != NULL; 
	hPtr = Blt_NextHashEntry(&cursor)) {
	Blt_HashTable *tablePtr;
	Blt_TableColumn col;

	tablePtr = Blt_GetHashValue(hPtr);
	for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
	     col = Blt_Table_NextTaggedColumn(&iter)) {
	    Blt_HashEntry *h2Ptr;
	
	    h2Ptr = Blt_FindHashEntry(tablePtr, (char *)col);
	    if (h2Ptr != NULL) {
		const char *tagName;
		int match;
		int i;

		match = (objc == 5);
		tagName = hPtr->key.string;
		for (i = 5; i < objc; i++) {
		    if (Tcl_StringMatch(tagName, Tcl_GetString(objv[i]))) {
			match = TRUE;
			break;	/* Found match. */
		    }
		}
		if (match) {
		    Tcl_Obj *objPtr;

		    objPtr = Tcl_NewStringObj(tagName, -1);
		    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
		    break;	/* Tag matches this column. Don't care if it
				 * matches any other columns. */
		}
	    }
	}
    }

    /* Handle reserved tags specially. */
    {
	int i;
	int allMatch, endMatch;

	endMatch = allMatch = (objc == 5);
	for (i = 5; i < objc; i++) {
	    if (Tcl_StringMatch("all", Tcl_GetString(objv[i]))) {
		allMatch = TRUE;
	    }
	    if (Tcl_StringMatch("end", Tcl_GetString(objv[i]))) {
		endMatch = TRUE;
	    }
	}
	if (allMatch) {
	    Tcl_ListObjAppendElement(interp, listObjPtr,
		Tcl_NewStringObj("all", 3));
	}
	if (endMatch) {
	    Blt_TableColumn col, lastCol;
	    
	    lastCol = Blt_Table_Column(table, Blt_Table_NumColumns(table));
	    for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
		 col = Blt_Table_NextTaggedColumn(&iter)) {
		if (col == lastCol) {
		    Tcl_ListObjAppendElement(interp, listObjPtr,
			Tcl_NewStringObj("end", 3));
		    break;
		}
	    }
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnTagSetOp --
 *
 *	Adds one or more tags for a given column.  Tag names can't start with a
 *	digit (to distinquish them from node ids) and can't be a reserved tag
 *	("all" or "end").
 *
 *	.t column tag set $column tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnTagSetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableIterator iter;
    int i;

    table = cmdPtr->table;
    if (Blt_Table_IterateColumns(interp, table, objv[4], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 5; i < objc; i++) {
	const char *tagName;
	Blt_TableColumn col;

	tagName = Tcl_GetString(objv[i]);
	for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
	     col = Blt_Table_NextTaggedColumn(&iter)) {
	    if (Blt_Table_SetColumnTag(interp, table, col, tagName) != TCL_OK) {
		return TCL_ERROR;
	    }
	}    
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ColumnTagUnsetOp --
 *
 *	Removes one or more tags from a given column. If a tag doesn't exist or
 *	is a reserved tag ("all" or "end"), nothing will be done and no error
 *	message will be returned.
 *
 *	.t column tag unset $column tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnTagUnsetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, 
		 Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableIterator iter;
    int i;

    table = cmdPtr->table;
    if (Blt_Table_IterateColumns(interp, table, objv[4], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 5; i < objc; i++) {
	const char *tagName;
	Blt_TableColumn col;
	
	tagName = Tcl_GetString(objv[i]);
	for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
	     col = Blt_Table_NextTaggedColumn(&iter)) {
	    if (Blt_Table_UnsetColumnTag(interp, table, col, tagName)!=TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }    
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ColumnTagOp --
 *
 * 	This procedure is invoked to process tag operations.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side Effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec columnTagOps[] =
{
    {"add",     1, ColumnTagAddOp,     5, 0, "tag ?column...?",},
    {"delete",  1, ColumnTagDeleteOp,  5, 0, "tag ?column...?",},
    {"exists",  1, ColumnTagExistsOp,  4, 5, "tag ?column?",},
    {"forget",  1, ColumnTagForgetOp,  4, 0, "?tag...?",},
    {"get",     1, ColumnTagGetOp,     5, 0, "column ?pattern...?",},
    {"indices", 1, ColumnTagIndicesOp, 4, 0, "?tag...?",},
    {"labels",  1, ColumnTagLabelsOp,  4, 0, "?tag...?",},
    {"range",   1, ColumnTagRangeOp,   6, 0, "from to ?tag...?",},
    {"search",  3, ColumnTagSearchOp,  5, 6, "column ?pattern?",},
    {"set",     3, ColumnTagSetOp,     5, 0, "column tag...",},
    {"unset",   1, ColumnTagUnsetOp,   5, 0, "column tag...",},

};

static int nColumnTagOps = sizeof(columnTagOps) / sizeof(Blt_OpSpec);

static int
ColumnTagOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    CmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nColumnTagOps, columnTagOps, BLT_OP_ARG3,
	objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc)(cmdPtr, interp, objc, objv);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnTraceOp --
 *
 *	Creates a trace for this instance.  Traces represent list of keys, a
 *	bitmask of trace flags, and a command prefix to be invoked when a
 *	matching trace event occurs.
 *
 *	The command prefix is parsed and saved in an array of Tcl_Objs. The
 *	qualified name of the instance is saved also.
 *
 * Results:
 *	A standard TCL result.  The name of the new trace is returned in the
 *	interpreter result.  Otherwise, if it failed to parse a switch, then
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *	$t column trace tag rwx proc 
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColumnTraceOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_TableIterator iter;
    Blt_TableTrace trace;
    TraceInfo *tiPtr;
    const char *tag;
    int flags;
    Blt_TableColumn col;
    Blt_Table table;

    table = cmdPtr->table;
    if (Blt_Table_IterateColumns(interp, table, objv[3], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    flags = GetTraceFlags(Tcl_GetString(objv[4]));
    if (flags < 0) {
	Tcl_AppendResult(interp, "unknown flag in \"", Tcl_GetString(objv[4]), 
		"\"", (char *)NULL);
	return TCL_ERROR;
    }
    col = NULL;
    tag = NULL;
    if (iter.type == TABLE_ITERATOR_RANGE) {
	Tcl_AppendResult(interp, "can't trace range of columns: use a tag", 
		(char *)NULL);
	return TCL_ERROR;
    }
    if ((iter.type == TABLE_ITERATOR_INDEX) || 
	(iter.type == TABLE_ITERATOR_LABEL)) {
	col = Blt_Table_FirstTaggedColumn(&iter);
    } else {
	tag = iter.tagName;
    } 
    tiPtr = Blt_Malloc(sizeof(TraceInfo));
    if (tiPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate trace: out of memory", 
		(char *)NULL);
	return TCL_ERROR;
    }
    trace = Blt_Table_CreateTrace(table, NULL, col, NULL, tag, flags, 
	TraceProc, TraceDeleteProc, tiPtr);
    if (trace == NULL) {
	Tcl_AppendResult(interp, "can't create column trace: out of memory", 
		(char *)NULL);
	Blt_Free(tiPtr);
	return TCL_ERROR;
    }
    /* Initialize the trace information structure. */
    tiPtr->cmdPtr = cmdPtr;
    tiPtr->trace = trace;
    tiPtr->tablePtr = &cmdPtr->traceTable;
    {
	Tcl_Obj **elv, **cmdv;
	int elc, i;

	if (Tcl_ListObjGetElements(interp, objv[5], &elc, &elv) != TCL_OK) {
	    return TCL_ERROR;
	}
	cmdv = Blt_AssertCalloc(elc + 1 + 3 + 1, sizeof(Tcl_Obj *));
	for(i = 0; i < elc; i++) {
	    cmdv[i] = elv[i];
	    Tcl_IncrRefCount(cmdv[i]);
	}
	cmdv[i] = Tcl_NewStringObj(cmdPtr->hPtr->key.string, -1);
	Tcl_IncrRefCount(cmdv[i]);
	tiPtr->cmdc = elc;
	tiPtr->cmdv = cmdv;
    }
    {
	char traceId[200];
	int isNew;

	sprintf_s(traceId, 200, "trace%d", cmdPtr->nextTraceId++);
	tiPtr->hPtr = Blt_CreateHashEntry(&cmdPtr->traceTable, traceId, &isNew);
	Blt_SetHashValue(tiPtr->hPtr, tiPtr);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), traceId, -1);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnTypeOp --
 *
 *	Reports and/or sets the type of a column.  
 * 
 * Results:
 *	A standard TCL result.  If successful, the old column label is
 *	returned in the interpreter result.  If the column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *	
 * Example:
 *	$t column type column ?newType?
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnTypeOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_TableIterator iter;
    Tcl_Obj *listObjPtr;
    Blt_TableColumn col;
    Blt_Table table;
    Blt_TableColumnType type;

    table = cmdPtr->table;
    if (Blt_Table_IterateColumns(interp, table, objv[3], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    if (objc == 5) {
	type = Blt_Table_GetColumnType(Tcl_GetString(objv[4]));
	if (type == TABLE_COLUMN_TYPE_UNKNOWN) {
	    Tcl_AppendResult(interp, "unknown column type \"", 
			     Tcl_GetString(objv[4]), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
	 col = Blt_Table_NextTaggedColumn(&iter)) {
	Tcl_Obj *objPtr;

	if (objc == 5) {
	    if (Blt_Table_SetColumnType(table, col, type) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	type = Blt_Table_ColumnType(col);
	objPtr = Tcl_NewStringObj(Blt_Table_NameOfType(type), -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnUnsetOp --
 *
 *	Unsets one or more columns of values.  One or more columns may be
 *	unset (using tags or multiple arguments). It's an error if the column
 *	doesn't exist.
 * 
 * Results:
 *	A standard TCL result. If the tag or column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *	
 * Example:
 *	$t column unset column ?column?
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnUnsetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int i;
    Blt_Table table;

    table = cmdPtr->table;
    for (i = 3; i < objc; i++) {
	Blt_TableIterator iter;
	Blt_TableColumn col;

	if (Blt_Table_IterateColumns(interp, table, objv[i], &iter) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
	     col = Blt_Table_NextTaggedColumn(&iter)) {
	    Blt_TableRow row;

	    for (row = Blt_Table_FirstRow(table); row != NULL;
		 row = Blt_Table_NextRow(table, row)) {
		if (Blt_Table_UnsetValue(table, row, col) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnUniqueOp --
 *
 *	Reports the unique values for a given column.  
 * 
 * Results:
 *	A standard TCL result. If the tag or column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *	$t column unique column
 *	
 *---------------------------------------------------------------------------
 */
static int
ColumnUniqueOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_HashTable values;
    Blt_Table table;
    Tcl_Obj *listObjPtr;
    Blt_TableColumn col;
    Blt_TableRow row;
    UniqueSwitches switches;

    table = cmdPtr->table;
    col = Blt_Table_FindColumn(interp, table, objv[3]);
    if (col == NULL) {
	return TCL_ERROR;
    }
    switches.flags = 0;
    if (Blt_ParseSwitches(interp, uniqueSwitches, objc - 4, objv + 4, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    Blt_InitHashTableWithPool(&values, BLT_STRING_KEYS);
    for (row = Blt_Table_FirstRow(table); row != NULL;
	 row = Blt_Table_NextRow(table, row)) {
	Blt_HashEntry *hPtr;
	const char *string;
	int isNew;
	long refCount;
	
	string = Blt_Table_GetString(table, row, col);
	if (string == NULL) {
	    string = cmdPtr->emptyValue;
	}
	hPtr = Blt_CreateHashEntry(&values, string, &isNew);
	if (isNew) {
	    refCount = 0;
	} else {
	    refCount = (long)Blt_GetHashValue(hPtr);
	}
	refCount++;
	Blt_SetHashValue(hPtr, (long)refCount);
    }
    listObjPtr = SortColumnValues(interp, &values, switches.flags);
    Blt_DeleteHashTable(&values);
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * ColumnValuesOp --
 *
 *	Retrieves a column of values.  The column argument can be either a
 *	tag, label, or column index.  If it is a tag, it must refer to exactly
 *	one column.
 * 
 * Results:
 *	A standard TCL result.  If successful, a list of values is returned in
 *	the interpreter result.  If the column index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t column values column ?valueList?
 *
 *---------------------------------------------------------------------------
 */
static int
ColumnValuesOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableColumn col;

    table = cmdPtr->table;
    col = Blt_Table_FindColumn(interp, cmdPtr->table, objv[3]);
    if (col == NULL) {
	return TCL_ERROR;
    }
    if (objc == 4) {
	Blt_TableRow row;
	Tcl_Obj *listObjPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	for (row = Blt_Table_FirstRow(cmdPtr->table); row != NULL;
	     row = Blt_Table_NextRow(cmdPtr->table, row)) {
	    Tcl_Obj *objPtr;
	    
	    objPtr = Blt_Table_GetObj(cmdPtr->table, row, col);
	    if (objPtr == NULL) {
		objPtr = Tcl_NewStringObj(cmdPtr->emptyValue, -1);
	    }
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	Tcl_SetObjResult(interp, listObjPtr);
    } else {
	Tcl_Obj **elv;
	int elc;
	int i;

	if (Tcl_ListObjGetElements(interp, objv[4], &elc, &elv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (elc > Blt_Table_NumRows(table)) {
	    long needed;

	    needed = elc - Blt_Table_NumRows(table);
	    if (Blt_Table_ExtendRows(interp, table, needed, NULL) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	for (i = 0; i < elc; i++) {
	    Blt_TableRow row;

	    row = Blt_Table_Row(cmdPtr->table, i + 1);
	    if (Blt_Table_SetObj(table, row, col, elv[i]) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnOp --
 *
 *	Parses the given command line and calls one of several column-specific
 *	operations.
 *	
 * Results:
 *	Returns a standard TCL result.  It is the result of operation called.
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec columnOps[] =
{
    {"copy",   2, ColumnCopyOp,    4, 0, "src dest ?switches...?",},
    {"create", 2, ColumnCreateOp,  3, 0, "?switches?",},
    {"delete", 2, ColumnDeleteOp,  4, 0, "column...",},
    {"dup",    2, ColumnDupOp,     3, 0, "column...",},
    {"exists", 3, ColumnExistsOp,  4, 4, "column",},
    {"extend", 3, ColumnExtendOp,  4, 0, "label ?label...?",},
    {"get",    1, ColumnGetOp,     4, 0, "column ?switches?",},
    {"index",  4, ColumnIndexOp,   4, 4, "column",},
    {"indices",4, ColumnIndicesOp, 3, 0, "column ?column...?",},
    {"label",  5, ColumnLabelOp,   4, 0, "column ?label?",},
    {"labels", 6, ColumnLabelsOp,  3, 4, "?labelList?",},
    {"length", 2, ColumnLengthOp,  3, 3, "",},
    {"move",   1, ColumnMoveOp,    5, 6, "from to ?count?",},
    {"names",  2, ColumnNamesOp,   3, 0, "?pattern...?",},
    {"notify", 2, ColumnNotifyOp,  5, 0, "column ?flags? command",},
    {"set",    2, ColumnSetOp,     5, 0, "column row value...",},
    {"tag",    2, ColumnTagOp,     3, 0, "op args...",},
    {"trace",  2, ColumnTraceOp,   6, 6, "column how command",},
    {"type",   2, ColumnTypeOp,    4, 5, "column ?type?",},
    {"unique", 3, ColumnUniqueOp,  4, 0, "column ?switches?",},
    {"unset",  3, ColumnUnsetOp,   4, 0, "column...",},
    {"values", 1, ColumnValuesOp,  4, 5, "column ?valueList?",},
};

static int nColumnOps = sizeof(columnOps) / sizeof(Blt_OpSpec);

static int
ColumnOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    CmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nColumnOps, columnOps, BLT_OP_ARG2, objc, 
	objv, BLT_OP_LINEAR_SEARCH);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (cmdPtr, interp, objc, objv);
    return result;
}

/************ Row Operations ***************/


static int
CopyRow(Tcl_Interp *interp, Blt_Table srcTable, Blt_Table destTable,
    Blt_TableRow srcRow,		/* Row offset in the source table. */
    Blt_TableRow destRow)		/* Row offset in the destination. */
{
    long i;

    if ((Blt_Table_SameTableObject(srcTable, destTable)) && 
	(srcRow == destRow)) {
	return TCL_OK;		/* Source and destination are the same. */
    }
    if (Blt_Table_NumColumns(srcTable) > Blt_Table_NumColumns(destTable)) {
	long needed;

	needed = Blt_Table_NumColumns(srcTable) - 
	    Blt_Table_NumColumns(destTable);
	if (Blt_Table_ExtendColumns(interp, destTable, needed, NULL)!=TCL_OK) {
	    return TCL_ERROR;
	}
    }
    for (i = 1; i <= Blt_Table_NumColumns(srcTable); i++) {
	Blt_TableColumn col;
	Blt_TableValue value;

	col = Blt_Table_Column(srcTable, i);
	value = Blt_Table_GetValue(srcTable, srcRow, col);
	col = Blt_Table_Column(destTable, i);
	if (Blt_Table_SetValue(destTable, destRow, col, value)!= TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}	    

static void
CopyRowTags(Blt_Table srcTable, Blt_Table destTable,
    Blt_TableRow srcRow,		/* Row offset in the source table. */
    Blt_TableRow destRow)		/* Row offset in the destination. */
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;

    /* Find all tags for with this row index. */
    for (hPtr = Blt_Table_FirstRowTag(srcTable, &iter); hPtr != NULL; 
	 hPtr = Blt_NextHashEntry(&iter)) {
	Blt_HashTable *tablePtr;
	Blt_HashEntry *h2Ptr;
	
	tablePtr = Blt_GetHashValue(hPtr);
	h2Ptr = Blt_FindHashEntry(tablePtr, (char *)srcRow);
	if (h2Ptr != NULL) {
	    /* We know the tag tables are keyed by strings, so we don't need
	     * to call Blt_GetHashKey and hence the hash table pointer to
	     * retrieve the key. */
	    Blt_Table_SetRowTag(NULL, destTable, destRow, hPtr->key.string);
	}
    }
}	    

/*
 *---------------------------------------------------------------------------
 *
 * RowCopyOp --
 *
 *	Copies the specified rows to the table.  A different table may be
 *	selected as the source.
 * 
 * Results:
 *	A standard TCL result. If the tag or row index is invalid, TCL_ERROR
 *	is returned and an error message is left in the interpreter result.
 *
 * Example:
 *	$dest row copy $srcrow $destrow ?-table srcTable?
 *
 *---------------------------------------------------------------------------
 */
static int
RowCopyOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    CopySwitches switches;
    Blt_Table srcTable, destTable;
    int result;
    Blt_TableRow src, dest;

    /* Process switches following the row names. */
    switches.flags = 0;
    switches.table = NULL;
    result = TCL_ERROR;
    if (Blt_ParseSwitches(interp, copySwitches, objc - 5, objv + 5, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	goto error;
    }
    srcTable = destTable = cmdPtr->table;
    if (switches.table != NULL) {
	srcTable = switches.table;
    }
    src = Blt_Table_FindRow(interp, srcTable, objv[3]);
    if (src == NULL) {
	goto error;
    }
    dest = Blt_Table_FindRow(interp, destTable, objv[4]);
    if (dest == NULL) {
	dest = Blt_Table_CreateRow(interp, destTable, Tcl_GetString(objv[4]));
	if (dest == NULL) {
	    goto error;
	}
    }
    if (CopyRow(interp, srcTable, destTable, src, dest) != TCL_OK) {
	goto error;
    }
    if ((switches.flags & COPY_NOTAGS) == 0) {
	CopyRowTags(srcTable, destTable, src, dest);
    }
    result = TCL_OK;
 error:
    Blt_FreeSwitches(copySwitches, &switches, 0);
    return result;
    
}

/*
 *---------------------------------------------------------------------------
 *
 * RowDeleteOp --
 *
 *	Deletes the rows designated.  One or more rows may be deleted using a
 *	tag.
 * 
 * Results:
 *	A standard TCL result. If the tag or row index is invalid, TCL_ERROR
 *	is returned and an error message is left in the interpreter result.
 *
 * Example:
 *	$t row delete ?row?...
 *
 *---------------------------------------------------------------------------
 */
static int
RowDeleteOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_TableIterator iter;
    Blt_TableRow row;
    int result;

    result = TCL_ERROR;
    if (Blt_Table_IterateRowsObjv(interp, cmdPtr->table, objc - 3, objv + 3, 
	&iter) != TCL_OK) {
	goto error;
    }
    /* Walk through the list of row offsets, deleting each row. */
    for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
	 row = Blt_Table_NextTaggedRow(&iter)) {
	if (Blt_Table_DeleteRow(cmdPtr->table, row) != TCL_OK) {
	    goto error;
	}
    }
    result = TCL_OK;
 error:
    Blt_Table_FreeIteratorObjv(&iter);
    return result;
}


/*
 *---------------------------------------------------------------------------
 *
 * RowDupOp --
 *
 *	Duplicates the specified rows in the table.  This differs from
 *	RowCopyOp, since the same table is always the source and destination.
 * 
 * Results:
 *	A standard TCL result. If the tag or row index is invalid, TCL_ERROR
 *	is returned and an error message is left in the interpreter result.
 *
 * Example:
 *	$dest row dup label ?label?... 
 *
 *---------------------------------------------------------------------------
 */
static int
RowDupOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_Obj *listObjPtr;
    Blt_TableIterator iter;
    Blt_TableRow src;
    int result;

    if (Blt_Table_IterateRowsObjv(interp, cmdPtr->table, objc - 3, objv + 3, 
	&iter) != TCL_OK) {
	return TCL_ERROR;
    }
    result = TCL_ERROR;
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (src = Blt_Table_FirstTaggedRow(&iter); src != NULL; 
	 src = Blt_Table_NextTaggedRow(&iter)) {
	const char *label;
	long j;
	Blt_TableRow dest;

	label = Blt_Table_RowLabel(src);
	dest = Blt_Table_CreateRow(interp, cmdPtr->table, label);
	if (dest == NULL) {
	    goto error;
	}
	if (CopyRow(interp, cmdPtr->table, cmdPtr->table, src, dest)!= TCL_OK) {
	    goto error;
	}
	CopyRowTags(cmdPtr->table, cmdPtr->table, src, dest);
	j = Blt_Table_RowIndex(dest);
	Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewLongObj(j));
    }
    Tcl_SetObjResult(interp, listObjPtr);
    result = TCL_OK;
 error:
    Blt_Table_FreeIteratorObjv(&iter);
    if (result != TCL_OK) {
	Tcl_DecrRefCount(listObjPtr);
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowExistsOp --
 *
 *	Indicates is the given row exists.
 * 
 * Results:
 *	A standard TCL result. If the tag or row index is invalid, TCL_ERROR
 *	is returned and an error message is left in the interpreter result.
 *
 * Example:
 *	$t row exists n
 *	
 *---------------------------------------------------------------------------
 */
static int
RowExistsOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int bool;
    Blt_TableRow row;

    row = Blt_Table_FindRow(NULL, cmdPtr->table, objv[3]);
    bool = (row != NULL);
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), bool);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowExtendOp --
 *
 *	Extends the table by the given number of rows.
 * 
 * Results:
 *	A standard TCL result. If the tag or row index is invalid, TCL_ERROR
 *	is returned and an error message is left in the interpreter result.
 *
 * Example:
 *	$t row extend n
 *	
 *---------------------------------------------------------------------------
 */
static int
RowExtendOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_Obj *listObjPtr;
    Blt_TableRow *rows;
    long i, n;
    int addLabels;

    addLabels = FALSE;
    if (objc == 3) {
	return TCL_OK;
    }
    n = 0;
    if ((objc > 4) || (Tcl_GetLongFromObj(NULL, objv[3], &n) != TCL_OK)) {
	n = objc - 3;
	addLabels = TRUE;
    }	    
    if (n == 0) {
	return TCL_OK;
    }
    if (n < 0) {
	Tcl_AppendResult(interp, "bad count \"", Blt_Itoa(n), 
			 "\": # rows can't be negative.", (char *)NULL);
	return TCL_ERROR;
    }
    rows = Blt_AssertMalloc(n * sizeof(Blt_TableRow));
    if (Blt_Table_ExtendRows(interp, cmdPtr->table, n, rows) != TCL_OK) {
	Blt_Free(rows);
	goto error;
    }
    if (addLabels) {
	long j;

	for (i = 0, j = 3; i < n; i++, j++) {
	    if (Blt_Table_SetRowLabel(interp, cmdPtr->table, rows[i], 
			Tcl_GetString(objv[j])) != TCL_OK) {
		goto error;
	    }
	}
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (i = 0; i < n; i++) {
	Tcl_Obj *objPtr;

	objPtr = Tcl_NewLongObj(Blt_Table_RowIndex(rows[i]));
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    Blt_Free(rows);
    return TCL_OK;
 error:
    Blt_Free(rows);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowGetOp --
 *
 *	Retrieves the values from a given row.  The row argument can be either
 *	a tag, label, or row index.  If it is a tag, it must refer to exactly
 *	one row.  An optional argument specifies how to return empty values.
 *	By default, the global empty value representation is used.
 * 
 * Results:
 *	A standard TCL result.  If successful, a list of values is returned in
 *	the interpreter result.  If the row index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t row get row ?col...?
 *
 *---------------------------------------------------------------------------
 */
static int
RowGetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_Obj *listObjPtr;
    Blt_TableRow row;
    Blt_Table table;
    const char *string;
    int needLabels;

    string = Tcl_GetString(objv[3]);
    needLabels = FALSE;
    if (strcmp(string, "-labels") == 0) {
	objv++, objc--;
	needLabels = TRUE;
    }
    table = cmdPtr->table;
    row = Blt_Table_FindRow(interp, table, objv[3]);
    if (row == NULL) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    if (objc == 4) {
	Blt_TableColumn col;

	for (col = Blt_Table_FirstColumn(table); col != NULL;
	     col = Blt_Table_NextColumn(table, col)) {
	    Tcl_Obj *objPtr;
	    
	    if (needLabels) {
		objPtr = Tcl_NewStringObj(Blt_Table_ColumnLabel(col), -1);
	    } else {
		objPtr = Tcl_NewLongObj(Blt_Table_ColumnIndex(col));
	    }
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    objPtr = Blt_Table_GetObj(table, row, col);
	    if (objPtr == NULL) {
		objPtr = Tcl_NewStringObj(cmdPtr->emptyValue, -1);
	    }
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    } else {
	Blt_TableIterator iter;
	Blt_TableColumn col;

	if (Blt_Table_IterateColumnsObjv(interp, table, objc - 4, objv + 4, 
		&iter) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (col = Blt_Table_FirstTaggedColumn(&iter); col != NULL; 
	     col = Blt_Table_NextTaggedColumn(&iter)) {
	    Tcl_Obj *objPtr;
	    
	    if (needLabels) {
		objPtr = Tcl_NewStringObj(Blt_Table_ColumnLabel(col), -1);
	    } else {
		objPtr = Tcl_NewLongObj(Blt_Table_ColumnIndex(col));
	    }
	    objPtr = Tcl_NewLongObj(Blt_Table_ColumnIndex(col));
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	    objPtr = Blt_Table_GetObj(table, row, col);
	    if (objPtr == NULL) {
		objPtr = Tcl_NewStringObj(cmdPtr->emptyValue, -1);
	    }
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * RowIndexOp --
 *
 *	Returns the row index of the given row tag, label, or index.  A tag
 *	can't represent more than one row.
 * 
 * Results:
 *	A standard TCL result. If the tag or row index is invalid, TCL_ERROR
 *	is returned and an error message is left in the interpreter result.
 *
 * Example:
 *	$t row index $row
 *	
 *---------------------------------------------------------------------------
 */
static int
RowIndexOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_TableRow row;
    long i;

    i = -1;
    row = Blt_Table_FindRow(NULL, cmdPtr->table, objv[3]);
    if (row != NULL) {
	i = Blt_Table_RowIndex(row);
    }
    Tcl_SetLongObj(Tcl_GetObjResult(interp), i);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowIndicesOp --
 *
 *	Returns a list of indices for the given rows.  
 * 
 * Results:
 *	A standard TCL result. If the tag or row index is invalid, TCL_ERROR
 *	is returned and an error message is left in the interpreter result.
 *
 * Example:
 *	$t row indices $row $row
 *	
 *---------------------------------------------------------------------------
 */
static int
RowIndicesOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_TableIterator iter;
    Blt_TableRow row;
    Tcl_Obj *listObjPtr;

    if (Blt_Table_IterateRowsObjv(interp, cmdPtr->table, objc - 3, objv + 3, 
	&iter) != TCL_OK) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
	 row = Blt_Table_NextTaggedRow(&iter)) {
	Tcl_Obj *objPtr;

	objPtr = Tcl_NewLongObj(Blt_Table_RowIndex(row));
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    Blt_Table_FreeIteratorObjv(&iter);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowCreateOp --
 *
 *	Creates a single new row into the table.  The location of the new row
 *	may be specified by -before or -after switches.  By default the new
 *	row is added to to the end of the table.
 * 
 * Results:
 *	A standard TCL result. If the tag or row index is invalid, TCL_ERROR
 *	is returned and an error message is left in the interpreter result.
 *
 * Example:
 *	$t row create -before 0 -after 1 -label label
 *	
 *---------------------------------------------------------------------------
 */
static int
RowCreateOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableRow row;
    InsertSwitches switches;
    unsigned int flags;
    
    switches.row = NULL;
    switches.label = NULL;
    switches.tags = NULL;
    switches.cmdPtr = cmdPtr;
    table = cmdPtr->table;

    flags = INSERT_ROW;
    if (Blt_ParseSwitches(interp, insertSwitches, objc - 3, objv + 3, 
		&switches, flags) < 0) {
	goto error;
    }
    row = Blt_Table_CreateRow(interp, table, switches.label);
    if (row == NULL) {
	goto error;
    }
    if (switches.row != NULL) {
	if (Blt_Table_MoveRows(interp, table, row, switches.row, 1) != TCL_OK) {
	    goto error;
	}
    }
    if (switches.tags != NULL) {
	Tcl_Obj **elv;
	int elc;
	int i;

	if (Tcl_ListObjGetElements(interp, switches.tags, &elc, &elv) 
	    != TCL_OK) {
	    goto error;
	}
	for (i = 0; i < elc; i++) {
	    if (Blt_Table_SetRowTag(interp, table, row, Tcl_GetString(elv[i])) 
		!= TCL_OK) {
		goto error;
	    }
	}
    }
    Tcl_SetObjResult(interp, Tcl_NewLongObj(Blt_Table_RowIndex(row)));
    Blt_FreeSwitches(insertSwitches, &switches, flags);
    return TCL_OK;
 error:
    Blt_FreeSwitches(insertSwitches, &switches, flags);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowLabelOp --
 *
 *	Gets/sets a label for one or more rows.  
 * 
 * Results:
 *	A standard TCL result.  If successful, the old row label is returned
 *	in the interpreter result.  If the row index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t row label row ?label? ?row label? 
 *	$t row label 1
 *	$t row label 1 newLabel 
 *	$t row label 1 lab1 2 lab2 3 lab3 5 lab5
 *
 *---------------------------------------------------------------------------
 */
static int
RowLabelOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;

    table = cmdPtr->table;
    if (objc == 4) {
	const char *label;
	Blt_TableRow row;

	row = Blt_Table_FindRow(interp, table, objv[3]);
	if (row == NULL) {
	    return TCL_ERROR;
	}
	label = Blt_Table_RowLabel(row);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), label, -1);
    } else {
	int i;
	
	if ((objc - 3) & 1) {
	    Tcl_AppendResult(interp,"odd # of row/label pairs: should be \"",
		Tcl_GetString(objv[0]), " row label ?row label?...", 
			     (char *)NULL);
	    return TCL_ERROR;
	}
	for (i = 3; i < objc; i += 2) {
	    Blt_TableRow row;
	    const char *label;

	    row = Blt_Table_FindRow(interp, table, objv[i]);
	    if (row == NULL) {
		return TCL_ERROR;
	    }
	    label = Tcl_GetString(objv[i+1]);
	    if (Blt_Table_SetRowLabel(interp, table, row, label) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * RowLabelsOp --
 *
 *	Gets/sets all the row label in the table.  
 * 
 * Results:
 *	A standard TCL result.  If successful, a list of values is returned in
 *	the interpreter result.  If the row index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t row labels ?labelList?
 *
 *---------------------------------------------------------------------------
 */
static int
RowLabelsOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;

    table = cmdPtr->table;
    if (objc == 3) {
	Blt_TableRow row;
	Tcl_Obj *listObjPtr;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	for (row = Blt_Table_FirstRow(table); row != NULL;
	     row = Blt_Table_NextRow(table, row)) {
	    Tcl_Obj *objPtr;
	    
	    objPtr = Tcl_NewStringObj(Blt_Table_RowLabel(row), -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	Tcl_SetObjResult(interp, listObjPtr);
    } else if (objc == 4) {
	Tcl_Obj **elv;
	int elc, n;
	int i;
	Blt_TableRow row;

	if (Tcl_ListObjGetElements(interp, objv[3], &elc, &elv) != TCL_OK) {
	    return TCL_ERROR;
	}
	n = MIN(elc, Blt_Table_NumRows(table));
	for (i = 0, row = Blt_Table_FirstRow(table); (row != NULL) && (i < n); 
	     row = Blt_Table_NextRow(table, row), i++) {
	    const char *label;

	    label = Tcl_GetString(elv[i]);
	    if (Blt_Table_SetRowLabel(interp, table, row, label) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowLengthOp --
 *
 *	Returns the number of rows the client sees.
 * 
 * Results:
 *	A standard TCL result.  If successful, the old row label is returned
 *	in the interpreter result.  If the row index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t row label row ?newLabel?
 *
 *---------------------------------------------------------------------------
 */
static int
RowLengthOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_SetLongObj(Tcl_GetObjResult(interp), Blt_Table_NumRows(cmdPtr->table));
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * RowMoveOp --
 *
 *	Moves the given number of rows to another location in the table.
 * 
 * Results:
 *	A standard TCL result.  If successful, a list of values is returned in
 *	the interpreter result.  If the row index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t row move from to ?n?
 *
 *---------------------------------------------------------------------------
 */
static int
RowMoveOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_TableRow from, to;
    long count;

    from = Blt_Table_FindRow(interp, cmdPtr->table, objv[3]);
    if (from == NULL) {
	return TCL_ERROR;
    }
    to = Blt_Table_FindRow(interp, cmdPtr->table, objv[4]);
    if (to == NULL) {
	return TCL_ERROR;
    }
    count = 1;
    if (objc == 6) {
	long lcount;

	if (Tcl_GetLongFromObj(interp, objv[5], &lcount) != TCL_OK) {
	    return TCL_ERROR;

	}
	if (lcount == 0) {
	    return TCL_OK;
	}
	if (lcount < 0) {
	    Tcl_AppendResult(interp, "# of rows can't be negative",
			     (char *)NULL);
	    return TCL_ERROR;
	}
	count = lcount;
    }
    return Blt_Table_MoveRows(interp, cmdPtr->table, from, to, count);
}


/*
 *---------------------------------------------------------------------------
 *
 * RowNamesOp --
 *
 *	Reports the labels of all rows.  
 * 
 * Results:
 *	Always returns TCL_OK.  The interpreter result is a list of row
 *	labels.
 *	
 * Example:
 *	$t row names pattern...
 *
 *---------------------------------------------------------------------------
 */
static int
RowNamesOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Tcl_Obj *listObjPtr;
    Blt_TableRow row;

    table = cmdPtr->table;
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (row = Blt_Table_FirstRow(table); row != NULL;
	 row = Blt_Table_NextRow(table, row)) {
	const char *label;
	int match;
	int i;

	label = Blt_Table_RowLabel(row);
	match = (objc == 3);
	for (i = 3; i < objc; i++) {
	    const char *pattern;

	    pattern = Tcl_GetString(objv[i]);
	    if (Tcl_StringMatch(label, pattern)) {
		match = TRUE;
		break;
	    }
	}
	if (match) {
	    Tcl_Obj *objPtr;

	    objPtr = Tcl_NewStringObj(label, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowNotifyOp --
 *
 *	Creates a notifier for this instance.  Notifiers represent a bitmask
 *	of events and a command prefix to be invoked when a matching event
 *	occurs.
 *
 *	The command prefix is parsed and saved in an array of Tcl_Objs. Extra
 *	slots are allocated for the
 *
 * Results:
 *	A standard TCL result.  The name of the new notifier is returned in
 *	the interpreter result.  Otherwise, if it failed to parse a switch,
 *	then TCL_ERROR is returned and an error message is left in the
 *	interpreter result.
 *
 * Example:
 *	table0 row notify row ?flags? command arg
 *
 *---------------------------------------------------------------------------
 */
static int
RowNotifyOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    NotifierInfo *niPtr;
    NotifySwitches switches;
    const char *tag, *string;
    int count;
    int i;
    int nArgs;
    Blt_TableRow row;
    Blt_TableRowColumnSpec spec;

    table = cmdPtr->table;
    spec = Blt_Table_RowSpec(table, objv[3], &string);
    row = NULL;
    tag = NULL;
    if (spec == TABLE_SPEC_TAG) {
	tag = string;
    } else {
	row = Blt_Table_FindRow(interp, table, objv[3]);
	if (row == NULL) {
	    return TCL_ERROR;
	}
    }
    count = 0;
    for (i = 4; i < objc; i++) {
	const char *string;

	string = Tcl_GetString(objv[i]);
	if (string[0] != '-') {
	    break;
	}
	count++;
    }
    switches.flags = 0;
    /* Process switches  */
    if (Blt_ParseSwitches(interp, notifySwitches, count, objv + 4, 
	     &switches, 0) < 0) {
	return TCL_ERROR;
    }
    niPtr = Blt_AssertMalloc(sizeof(NotifierInfo));
    niPtr->cmdPtr = cmdPtr;
    if (tag == NULL) {
	niPtr->notifier = Blt_Table_CreateRowNotifier(interp, cmdPtr->table,
		row, switches.flags, NotifyProc, NotifierDeleteProc, niPtr);
    } else {
	niPtr->notifier = Blt_Table_CreateRowTagNotifier(interp, 
		cmdPtr->table, tag, switches.flags, NotifyProc, 
		NotifierDeleteProc, niPtr);
    }	
    nArgs = (objc - i) + 2;
    /* Stash away the command in structure and pass that to the notifier. */
    niPtr->cmdv = Blt_AssertMalloc(nArgs * sizeof(Tcl_Obj *));
    for (count = 0; i < objc; i++, count++) {
	Tcl_IncrRefCount(objv[i]);
	niPtr->cmdv[count] = objv[i];
    }
    niPtr->cmdc = nArgs;
    if (switches.flags == 0) {
	switches.flags = TABLE_NOTIFY_ALL_EVENTS;
    }
    {
	char notifyId[200];
	Blt_HashEntry *hPtr;
	int isNew;

	sprintf_s(notifyId, 200, "notify%d", cmdPtr->nextNotifyId++);
	hPtr = Blt_CreateHashEntry(&cmdPtr->notifyTable, notifyId, &isNew);
	assert(isNew);
	Blt_SetHashValue(hPtr, niPtr);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), notifyId, -1);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowSetOp --
 *
 *	Sets a row of values.  One or more rows may be set using a tag.  The
 *	column order is always the table's current view of the table.  There
 *	may be less values than needed.
 * 
 * Results:
 *	A standard TCL result. If the tag or row index is invalid, TCL_ERROR
 *	is returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t row set row ?switches? ?column value?...
 *	$t row set row ?switches? ?column value?...
 *
 *---------------------------------------------------------------------------
 */
static int
RowSetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableIterator iter;
    long nCols;
    Blt_TableRow row;

    table = cmdPtr->table;
    /* May set more than one row with the same values. */
    if (IterateRows(interp, table, objv[3], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 4) {
	return TCL_OK;
    }
    nCols = objc - 4;
    if (nCols & 1) {
	Tcl_AppendResult(interp, "odd # of column/value pairs: should be \"", 
		Tcl_GetString(objv[0]), " row set column value...", 
			 (char *)NULL);
	return TCL_ERROR;
    }
    for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
	 row = Blt_Table_NextTaggedRow(&iter)) {
	long i;

	/* The remaining arguments are index/value pairs. */
	for (i = 4; i < objc; i += 2) {
	    Blt_TableColumn col;
	    
	    col = Blt_Table_FindColumn(interp, table, objv[i]);
	    if (col == NULL) {
		/* Can't find the column. Create it and try to find it
		 * again. */
		if (MakeColumns(interp, table, objv[i]) != TCL_OK) {
		    return TCL_ERROR;
		}
		col = Blt_Table_FindColumn(interp, table, objv[i]);
	    }
	    if (Blt_Table_SetObj(table, row, col, objv[i+1]) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowTagAddOp --
 *
 *	Adds a given tag to one or more rows.  Tag names can't start with a
 *	digit (to distinquish them from node ids) and can't be a reserved tag
 *	("all" or "end").
 *
 *	.t row tag add tag ?row...?
 *
 *---------------------------------------------------------------------------
 */
static int
RowTagAddOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    int i;
    const char *tagName;

    table = cmdPtr->table;
    tagName = Tcl_GetString(objv[4]);
    if (Blt_Table_SetRowTag(interp, table, NULL, tagName) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 5; i < objc; i++) {
	Blt_TableRow row;
	Blt_TableIterator iter;

	if (Blt_Table_IterateRows(interp, table, objv[i], &iter) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
	     row = Blt_Table_NextTaggedRow(&iter)) {
	    if (Blt_Table_SetRowTag(interp, table, row, tagName) != TCL_OK) {
		return TCL_ERROR;
	    }
	}    
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * RowTagDeleteOp --
 *
 *	Removes a given tag from one or more rows. If a tag doesn't exist or
 *	is a reserved tag ("all" or "end"), nothing will be done and no error
 *	message will be returned.
 *
 *	.t row tag delete tag ?row...?
 *
 *---------------------------------------------------------------------------
 */
static int
RowTagDeleteOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableIterator iter;
    int i;
    const char *tagName;

    table = cmdPtr->table;
    tagName = Tcl_GetString(objv[4]);
    for (i = 5; i < objc; i++) {
	Blt_TableRow row;
	
	if (Blt_Table_IterateRows(interp, table, objv[i], &iter) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
	     row = Blt_Table_NextTaggedRow(&iter)) {
	    if (Blt_Table_UnsetRowTag(interp, table, row, tagName)!=TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowTagExistsOp --
 *
 *	Returns the existence of a tag in the table.  If a row is specified
 *	then the tag is search for for that row.
 *
 *	.t tag row exists tag ?row?
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
RowTagExistsOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int bool;
    const char *tagName;

    tagName = Tcl_GetString(objv[3]);
    bool = (Blt_Table_FindRowTagTable(cmdPtr->table, tagName) != NULL);
    if (objc == 5) {
	Blt_TableRow row;

	row = Blt_Table_FindRow(interp, cmdPtr->table, objv[4]);
	if (row == NULL) {
	    bool = FALSE;
	} else {
	    bool = Blt_Table_HasRowTag(cmdPtr->table, row, tagName);
	}
    } 
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), bool);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowTagForgetOp --
 *
 *	Removes the given tags from all nodes.
 *
 *	$t row tag forget tag1 tag2 tag3...
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
RowTagForgetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    int i;

    table = cmdPtr->table;
    for (i = 4; i < objc; i++) {
	if (Blt_Table_ForgetRowTag(interp, table, Tcl_GetString(objv[i])) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowTagGetOp --
 *
 *	Returns the tag names for a given row.  If one of more pattern
 *	arguments are provided, then only those matching tags are returned.
 *
 *	.t row tag get row pat1 pat2...
 *
 *---------------------------------------------------------------------------
 */
static int
RowTagGetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch hsearch;
    Blt_HashTable tagTable;
    Blt_Table table;
    Blt_TableIterator iter;
    Blt_TableRow row;
    Tcl_Obj *listObjPtr;

    table = cmdPtr->table;
    if (Blt_Table_IterateRows(interp, table, objv[4], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);

    Blt_InitHashTable(&tagTable, BLT_STRING_KEYS);
    
    /* Collect all the tags into a hash table. */
    for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
	 row = Blt_Table_NextTaggedRow(&iter)) {
	Blt_Chain chain;
	Blt_ChainLink link;

	chain = Blt_Table_RowTags(table, row);
	for (link = Blt_Chain_FirstLink(chain); link != NULL;
	     link = Blt_Chain_NextLink(link)) {
	    const char *tagName;
	    int isNew;

	    tagName = Blt_Chain_GetValue(link);
	    Blt_CreateHashEntry(&tagTable, tagName, &isNew);
	}
	Blt_Chain_Destroy(chain);
    }
    for (hPtr = Blt_FirstHashEntry(&tagTable, &hsearch); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&hsearch)) {
	int match;
	const char *tagName;

	tagName = Blt_GetHashKey(&tagTable, hPtr);
	match = TRUE;
	if (objc > 5) {
	    int i;

	    match = FALSE;
	    for (i = 5; i < objc; i++) {
		if (Tcl_StringMatch(tagName, Tcl_GetString(objv[i]))) {
		    match = TRUE;
		}
	    }
	}
	if (match) {
	    Tcl_Obj *objPtr;

	    objPtr = Tcl_NewStringObj(tagName, -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    Blt_DeleteHashTable(&tagTable);
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

static unsigned char *
GetRowTagMatches(Tcl_Interp *interp, Blt_Table table, int objc, 
		 Tcl_Obj *const *objv)
{
    long nRows;
    int i;
    unsigned char *matches;

    nRows = Blt_Table_NumRows(table);
    matches = Blt_AssertCalloc(nRows + 1, sizeof(unsigned char));
    /* Handle the reserved tags "all" or "end". */
    for (i = 0; i < objc; i++) {
	char *tagName;
	long j;

	tagName = Tcl_GetString(objv[i]);
	if (strcmp("all", tagName) == 0) {
	    for (j = 1; j <= nRows; j++) {
		matches[j] = TRUE;
	    }
	    return matches;		/* Don't care other tags. */
	} 
	if (strcmp("end", tagName) == 0) {
	    matches[nRows] = TRUE;
	}
    }
    /* Now check user-defined tags. */
    for (i = 0; i < objc; i++) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch iter;
	Blt_HashTable *tagTablePtr;
	const char *tagName;
	
	tagName = Tcl_GetString(objv[i]);
	if ((strcmp("all", tagName) == 0) || (strcmp("end", tagName) == 0)) {
	    continue;
	}
	tagTablePtr = Blt_Table_FindRowTagTable(table, tagName);
	if (tagTablePtr == NULL) {
	    Tcl_AppendResult(interp, "unknown row tag \"", tagName, "\"",
		(char *)NULL);
	    Blt_Free(matches);
	    return NULL;
	}
	for (hPtr = Blt_FirstHashEntry(tagTablePtr, &iter); hPtr != NULL; 
	     hPtr = Blt_NextHashEntry(&iter)) {
	    Blt_TableRow row;
	    long j;

	    row = Blt_GetHashValue(hPtr);
	    j = Blt_Table_RowIndex(row);
	    assert(j >= 0);
	    matches[j] = TRUE;
	}
    }
    return matches;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowTagIndicesOp --
 *
 *	Returns row indices names for the given tags.  If one of more tag
 *	names are provided, then only those matching indices are returned.
 *
 *	.t row tag indices tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
RowTagIndicesOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_Obj *listObjPtr;
    long j;
    long nRows;
    unsigned char *matches;

    matches = GetRowTagMatches(interp, cmdPtr->table, objc - 4, objv + 4);
    if (matches == NULL) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    nRows = Blt_Table_NumRows(cmdPtr->table);
    for (j = 1; j <= nRows; j++) {
	if (matches[j]) {
	    Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewLongObj(j));
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    Blt_Free(matches);
    return TCL_OK;
}
/*
 *---------------------------------------------------------------------------
 *
 * RowTagLabelsOp --
 *
 *	Returns row labels for the given tags.  If one of more tag
 *	names are provided, then only those matching indices are returned.
 *
 * Example:
 *	.t row tag labels tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
RowTagLabelsOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_Obj *listObjPtr;
    long j;
    long nRows;
    unsigned char *matches;

    matches = GetRowTagMatches(interp, cmdPtr->table, objc - 4, objv + 4);
    if (matches == NULL) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    nRows = Blt_Table_NumRows(cmdPtr->table);
    for (j = 1; j <= nRows; j++) {
	if (matches[j]) {
	    Blt_TableRow row;
	    Tcl_Obj *objPtr;
	    
	    row = Blt_Table_Row(cmdPtr->table, j);
	    objPtr = Tcl_NewStringObj(Blt_Table_RowLabel(row), -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    Blt_Free(matches);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowTagRangeOp --
 *
 *	Adds one or more tags for a given row.  Tag names can't start with a
 *	digit (to distinquish them from node ids) and can't be a reserved tag
 *	("all" or "end").
 *
 *	.t row tag range $from $to tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
RowTagRangeOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    int i;
    Blt_TableRow from, to;

    table = cmdPtr->table;
    from = Blt_Table_FindRow(interp, table, objv[4]);
    if (from == NULL) {
	return TCL_ERROR;
    }
    to = Blt_Table_FindRow(interp, table, objv[5]);
    if (to == NULL) {
	return TCL_ERROR;
    }
    if (Blt_Table_RowIndex(from) > Blt_Table_RowIndex(to)) {
	Blt_TableRow tmp;
	tmp = to, to = from, from = tmp;
    }
    for (i = 6; i < objc; i++) {
	const char *tagName;
	long j;
	
	tagName = Tcl_GetString(objv[i]);
	for (j = Blt_Table_RowIndex(from); j <= Blt_Table_RowIndex(to); j++) {
	    Blt_TableRow row;

	    row = Blt_Table_Row(table, j);
	    if (Blt_Table_SetRowTag(interp, table, row, tagName) != TCL_OK) {
		return TCL_ERROR;
	    }
	}    
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * RowTagSearchOp --
 *
 *	Returns tag names for a given row.  If one of more pattern arguments
 *	are provided, then only those matching tags are returned.
 *
 *	.t row tag find $row pat1 pat2...
 *
 *---------------------------------------------------------------------------
 */
static int
RowTagSearchOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Blt_TableIterator iter;
    Tcl_Obj *listObjPtr;

    table = cmdPtr->table;
    if (Blt_Table_IterateRows(interp, table, objv[4], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Blt_Table_FirstRowTag(table, &cursor); hPtr != NULL; 
	hPtr = Blt_NextHashEntry(&cursor)) {
	Blt_HashTable *tablePtr;
	Blt_TableRow row;
	
	tablePtr = Blt_GetHashValue(hPtr);
	for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
	     row = Blt_Table_NextTaggedRow(&iter)) {
	    Blt_HashEntry *h2Ptr;
	
	    h2Ptr = Blt_FindHashEntry(tablePtr, (char *)row);
	    if (h2Ptr != NULL) {
		const char *tagName;
		int match;
		int i;

		match = (objc == 5);
		tagName = hPtr->key.string;
		for (i = 5; i < objc; i++) {
		    if (Tcl_StringMatch(tagName, Tcl_GetString(objv[i]))) {
			match = TRUE;
			break;	/* Found match. */
		    }
		}
		if (match) {
		    Tcl_Obj *objPtr;

		    objPtr = Tcl_NewStringObj(tagName, -1);
		    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
		    break;	/* Tag matches this row. Don't care if it
				 * matches any other rows. */
		}
	    }
	}
    }

    /* Handle reserved tags specially. */
    {
	int i;
	int allMatch, endMatch;

	endMatch = allMatch = (objc == 5);
	for (i = 5; i < objc; i++) {
	    if (Tcl_StringMatch("all", Tcl_GetString(objv[i]))) {
		allMatch = TRUE;
	    }
	    if (Tcl_StringMatch("end", Tcl_GetString(objv[i]))) {
		endMatch = TRUE;
	    }
	}
	if (allMatch) {
	    Tcl_ListObjAppendElement(interp, listObjPtr,
		Tcl_NewStringObj("all", 3));
	}
	if (endMatch) {
	    Blt_TableRow row, lastRow;
	    
	    lastRow = Blt_Table_Row(table, Blt_Table_NumRows(table));
	    for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
		 row = Blt_Table_NextTaggedRow(&iter)) {
		if (row == lastRow) {
		    Tcl_ListObjAppendElement(interp, listObjPtr,
			Tcl_NewStringObj("end", 3));
		    break;
		}
	    }
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowTagSetOp --
 *
 *	Adds one or more tags for a given row.  
 *
 *	.t row tag set $row tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
RowTagSetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableIterator iter;
    int i;

    table = cmdPtr->table;
    if (Blt_Table_IterateRows(interp, table, objv[4], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 5; i < objc; i++) {
	const char *tagName;
	Blt_TableRow row;

	tagName = Tcl_GetString(objv[i]);
	for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
	     row = Blt_Table_NextTaggedRow(&iter)) {
	    if (Blt_Table_SetRowTag(interp, table, row, tagName) != TCL_OK) {
		return TCL_ERROR;
	    }
	}    
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * RowTagUnsetOp --
 *
 *	Removes one or more tags from a given row. 
 *
 *	.t row tag unset $row tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
RowTagUnsetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableIterator iter;
    int i;

    table = cmdPtr->table;
    if (Blt_Table_IterateRows(interp, table, objv[4], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 5; i < objc; i++) {
	const char *tagName;
	Blt_TableRow row;
	
	tagName = Tcl_GetString(objv[i]);
	for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
	     row = Blt_Table_NextTaggedRow(&iter)) {
	    if (Blt_Table_UnsetRowTag(interp, table, row, tagName)!=TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }    
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowTagOp --
 *
 * 	This procedure is invoked to process tag operations.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side Effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec rowTagOps[] =
{
    {"add",     1, RowTagAddOp,     5, 0, "tag ?row...?",},
    {"delete",  1, RowTagDeleteOp,  5, 0, "tag ?row...?",},
    {"exists",  1, RowTagExistsOp,  5, 6, "tag ?row?",},
    {"forget",  1, RowTagForgetOp,  4, 0, "?tag...?",},
    {"get",     1, RowTagGetOp,     5, 0, "row ?pattern...?",},
    {"indices", 1, RowTagIndicesOp, 4, 0, "?tag...?",},
    {"labels",  1, RowTagLabelsOp,  4, 0, "?tag...?",},
    {"range",   1, RowTagRangeOp,   6, 0, "from to ?tag...?",},
    {"search",  3, RowTagSearchOp,  5, 6, "row ?pattern?",},
    {"set",     3, RowTagSetOp,     5, 0, "row tag...",},
    {"unset",   1, RowTagUnsetOp,   5, 0, "row tag...",},
};

static int nRowTagOps = sizeof(rowTagOps) / sizeof(Blt_OpSpec);

static int 
RowTagOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    CmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nRowTagOps, rowTagOps, BLT_OP_ARG3, 
			    objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (cmdPtr, interp, objc, objv);
    return result;
}


/*
 *---------------------------------------------------------------------------
 *
 * RowTraceOp --
 *
 *	Creates a trace for this instance.  Traces represent list of keys, a
 *	bitmask of trace flags, and a command prefix to be invoked when a
 *	matching trace event occurs.
 *
 *	The command prefix is parsed and saved in an array of Tcl_Objs. The
 *	qualified name of the instance is saved also.
 *
 * Results:
 *	A standard TCL result.  The name of the new trace is returned in the
 *	interpreter result.  Otherwise, if it failed to parse a switch, then
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *	$t row trace tag rwx proc 
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
RowTraceOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableIterator iter;
    Blt_TableTrace trace;
    TraceInfo *tiPtr;
    const char *tag;
    int flags;
    Blt_TableRow row;

    table = cmdPtr->table;
    if (Blt_Table_IterateRows(interp, table, objv[3], &iter) != TCL_OK) {
	return TCL_ERROR;
    }
    flags = GetTraceFlags(Tcl_GetString(objv[4]));
    if (flags < 0) {
	Tcl_AppendResult(interp, "unknown flag in \"", Tcl_GetString(objv[4]), 
		"\"", (char *)NULL);
	return TCL_ERROR;
    }
    row = NULL;
    tag = NULL;
    if (iter.type == TABLE_ITERATOR_RANGE) {
	Tcl_AppendResult(interp, "can't trace range of rows: use a tag", 
		(char *)NULL);
	return TCL_ERROR;
    }
    if ((iter.type == TABLE_ITERATOR_INDEX) || 
	(iter.type == TABLE_ITERATOR_LABEL)) {
	row = Blt_Table_FirstTaggedRow(&iter);
    } else {
	tag = iter.tagName;
    }
    tiPtr = Blt_Malloc(sizeof(TraceInfo));
    if (tiPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate trace: out of memory", 
		(char *)NULL);
	return TCL_ERROR;
    }
    trace = Blt_Table_CreateTrace(table, row, NULL, tag, NULL, flags, 
	TraceProc, TraceDeleteProc, tiPtr);
    if (trace == NULL) {
	Tcl_AppendResult(interp, "can't create row trace: out of memory", 
		(char *)NULL);
	Blt_Free(tiPtr);
	return TCL_ERROR;
    }
    /* Initialize the trace information structure. */
    tiPtr->cmdPtr = cmdPtr;
    tiPtr->trace = trace;
    tiPtr->tablePtr = &cmdPtr->traceTable;
    {
	Tcl_Obj **elv, **cmdv;
	int elc, i;

	if (Tcl_ListObjGetElements(interp, objv[5], &elc, &elv) != TCL_OK) {
	    return TCL_ERROR;
	}
	cmdv = Blt_AssertCalloc(elc + 1 + 3 + 1, sizeof(Tcl_Obj *));
	for(i = 0; i < elc; i++) {
	    cmdv[i] = elv[i];
	    Tcl_IncrRefCount(cmdv[i]);
	}
	cmdv[i] = Tcl_NewStringObj(cmdPtr->hPtr->key.string, -1);
	Tcl_IncrRefCount(cmdv[i]);
	tiPtr->cmdc = elc;
	tiPtr->cmdv = cmdv;
    }
    {
	char traceId[200];
	int isNew;

	sprintf_s(traceId, 200, "trace%d", cmdPtr->nextTraceId++);
	tiPtr->hPtr = Blt_CreateHashEntry(&cmdPtr->traceTable, traceId, &isNew);
	Blt_SetHashValue(tiPtr->hPtr, tiPtr);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), traceId, -1);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowUnsetOp --
 *
 *	Unsets one or more rows of values.  One or more rows may be unset
 *	(using tags or multiple arguments).
 * 
 * Results:
 *	A standard TCL result. If the tag or row index is invalid, TCL_ERROR
 *	is returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t row unset row ?row?
 *
 *---------------------------------------------------------------------------
 */
static int
RowUnsetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    int i;

    table = cmdPtr->table;
    for (i = 3; i < objc; i++) {
	Blt_TableIterator iter;
	Blt_TableRow row;

	if (Blt_Table_IterateRows(interp, table, objv[i], &iter) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
	     row = Blt_Table_NextTaggedRow(&iter)) {
	    Blt_TableColumn col;

	    for (col = Blt_Table_FirstColumn(table); col != NULL;
		 col = Blt_Table_NextColumn(table, col)) {
		if (Blt_Table_UnsetValue(table, row, col) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowUniqueOp --
 *
 *	Reports the unique values for a given row.  
 * 
 * Results:
 *	A standard TCL result. If the tag or row index is invalid, TCL_ERROR
 *	is returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t row unique row
 *
 *---------------------------------------------------------------------------
 */
static int
RowUniqueOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    int i;
    Blt_HashTable values;

    table = cmdPtr->table;
    Blt_InitHashTableWithPool(&values, BLT_STRING_KEYS);
    for (i = 3; i < objc; i++) {
	Blt_TableIterator iter;
	Blt_TableRow row;

	if (Blt_Table_IterateRows(interp, table, objv[i], &iter) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (row = Blt_Table_FirstTaggedRow(&iter); row != NULL; 
	     row = Blt_Table_NextTaggedRow(&iter)) {
	    Blt_TableColumn col;

	    for (col = Blt_Table_FirstColumn(table); col != NULL;
		 col = Blt_Table_NextColumn(table, col)) {
		const char *string;
		Blt_HashEntry *hPtr;
		int isNew;

		string = Blt_Table_GetString(table, row, col);
		if (string == NULL) {
		    string = cmdPtr->emptyValue;
		}
		hPtr = Blt_CreateHashEntry(&values, string, &isNew);
		if (hPtr == NULL) {
		    return TCL_ERROR;
		}
	    }
	}
    }
    {
	Blt_HashEntry *hPtr;
	Blt_HashSearch iter;

	for (hPtr = Blt_FirstHashEntry(&values, &iter); hPtr != NULL;
	     hPtr = Blt_NextHashEntry(&iter)) {
	    Tcl_AppendElement(interp, Blt_GetHashKey(&values, hPtr));
	}
    }
    Blt_DeleteHashTable(&values);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RowValuesOp --
 *
 *	Retrieves a row of values.  The row argument can be either a tag,
 *	label, or row index.  If it is a tag, it must refer to exactly one
 *	row.
 * 
 * Results:
 *	A standard TCL result.  If successful, a list of values is returned in
 *	the interpreter result.  If the row index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t row listget row ?defValue?
 *
 *---------------------------------------------------------------------------
 */
static int
RowValuesOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableRow row;
    Tcl_Obj *listObjPtr;

    table = cmdPtr->table;
    row = Blt_Table_FindRow(interp, cmdPtr->table, objv[3]);
    if (row == NULL) {
	return TCL_ERROR;
    }
    if (objc == 4) {
	Blt_TableColumn col;

	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	for (col = Blt_Table_FirstColumn(cmdPtr->table); col != NULL;
	     col = Blt_Table_NextColumn(cmdPtr->table, col)) {
	    Tcl_Obj *objPtr;
	    
	    objPtr = Blt_Table_GetObj(cmdPtr->table, row, col);
	    if (objPtr == NULL) {
		objPtr = Tcl_NewStringObj(cmdPtr->emptyValue, -1);
	    }
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	Tcl_SetObjResult(interp, listObjPtr);
    } else {
	Tcl_Obj **elv;
	int elc;
	long i, j;

	if (Tcl_ListObjGetElements(interp, objv[4], &elc, &elv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (elc > Blt_Table_NumColumns(table)) {
	    long n;

	    n = elc - Blt_Table_NumColumns(table);
	    if (Blt_Table_ExtendColumns(interp, table, n, NULL) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	for (i = 0, j = 1; i < elc; i++, j++) {
	    Blt_TableColumn col;

	    col = Blt_Table_Column(table, j);
	    if (Blt_Table_SetObj(table, row, col, elv[i]) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * RowOp --
 *
 *	Parses the given command line and calls one of several row-specific
 *	operations.
 *	
 * Results:
 *	Returns a standard TCL result.  It is the result of operation called.
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec rowOps[] =
{
    {"copy",    2, RowCopyOp,   4, 0, "src dest ?switches...?",},
    {"create",  2, RowCreateOp, 3, 0, "?switches...?",},
    {"delete",  2, RowDeleteOp, 4, 0, "row...",},
    {"dup",     2, RowDupOp,    3, 0, "row...",},
    {"exists",  3, RowExistsOp, 4, 4, "row",},
    {"extend",  3, RowExtendOp, 4, 0, "label ?label...?",},
    {"get",     1, RowGetOp,    4, 0, "row ?switches?",},
    {"index",   4, RowIndexOp,  4, 4, "row",},
    {"indices", 4, RowIndicesOp,3, 0, "row ?row...?",},
    {"label",   5, RowLabelOp,  4, 0, "row ?label?",},
    {"labels",  6, RowLabelsOp, 3, 4, "?labelList?",},
    {"length",  2, RowLengthOp, 3, 3, "",},
    {"move",    1, RowMoveOp,   5, 6, "from to ?count?",},
    {"names",   2, RowNamesOp,  3, 0, "?pattern...?",},
    {"notify",  2, RowNotifyOp, 5, 0, "row ?flags? command",},
    {"set",     2, RowSetOp,    5, 0, "row column value...",},
    {"tag",     2, RowTagOp,    3, 0, "op args...",},
    {"trace",   2, RowTraceOp,  6, 6, "row how command",},
    {"unique",  3, RowUniqueOp, 4, 4, "row",},
    {"unset",   3, RowUnsetOp,  4, 0, "row...",},
    {"values",  1, RowValuesOp, 4, 5, "row ?valueList?",},
};

static int nRowOps = sizeof(rowOps) / sizeof(Blt_OpSpec);

static int
RowOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    CmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nRowOps, rowOps, BLT_OP_ARG2, objc, 
	objv, BLT_OP_LINEAR_SEARCH);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (cmdPtr, interp, objc, objv);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * AddOp --
 *
 *	Concatenates the source table onto the destination.
 * 
 * Results:
 *	A standard TCL result. If the tag or column index is invalid,
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *
 *	$dest add $src ?switches?
 *
 *---------------------------------------------------------------------------
 */
static int
AddOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    AddSwitches switches;
    Blt_Table srcTable;
    size_t oldSize, extra;
    int result;
    Blt_TableColumn src;

    /* Process switches following the column names. */
    if (Blt_Table_Open(interp, Tcl_GetString(objv[2]), &srcTable) != TCL_OK) {
	return TCL_ERROR;
    }
    switches.flags = 0;
    result = TCL_ERROR;
    if (Blt_ParseSwitches(interp, addSwitches, objc - 3, objv + 3, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	goto error;
    }
    oldSize = Blt_Table_NumRows(cmdPtr->table);
    extra = Blt_Table_NumRows(srcTable);
    Blt_Table_ExtendRows(interp, cmdPtr->table, extra, NULL);
    for (src = Blt_Table_FirstColumn(srcTable); src != NULL;
	 src = Blt_Table_NextColumn(srcTable, src)) {
	const char *label;
	Blt_TableColumn dest;
	long i;

	label = Blt_Table_ColumnLabel(src);
	dest = Blt_Table_FindColumnByLabel(cmdPtr->table, label);
	if (dest == NULL) {
	    /* If column doesn't exist in destination table, create a new
	     * column, copying the label and the column type. */
	    if (Blt_Table_ExtendColumns(interp, cmdPtr->table, 1, &dest) 
		!= TCL_OK) {
		goto error;
	    }
	    if (Blt_Table_SetColumnLabel(interp, cmdPtr->table, dest, label) 
		!= TCL_OK) {
		goto error;
	    }
	    Blt_Table_SetColumnType(cmdPtr->table, dest, 
		Blt_Table_ColumnType(src));
	}
	for (i = 1; i <= Blt_Table_NumRows(srcTable); i++) {
	    Blt_TableRow row;
	    Blt_TableValue value;

	    row = Blt_Table_Row(srcTable, i);
	    value = Blt_Table_GetValue(srcTable, row, src);
	    if (value == NULL) {
		continue;
	    }
	    row = Blt_Table_Row(cmdPtr->table, i+oldSize);
	    if (Blt_Table_SetValue(cmdPtr->table, row, dest, value) != TCL_OK) {
		goto error;
	    }
	}
	if ((switches.flags & COPY_NOTAGS) == 0) {
	    CopyColumnTags(srcTable, cmdPtr->table, src, dest);
	}
    }
    result = TCL_OK;
 error:
    Blt_Table_Close(srcTable);
    Blt_FreeSwitches(addSwitches, &switches, 0);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * AppendOp --
 *
 *
 *	Appends one or more values to the current value at the given
 *	location. If the column or row doesn't already exist, it will
 *	automatically be created.
 * 
 * Results:
 *	A standard TCL result. If the tag or index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t append $row $column $value ?value...?
 *
 *---------------------------------------------------------------------------
 */
static int
AppendOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableIterator ri, ci;
    Blt_TableColumn col;
    int i, extra;

    table = cmdPtr->table;
    if (IterateRows(interp, table, objv[2], &ri) != TCL_OK) {
	return TCL_ERROR;
    }
    if (IterateColumns(interp, table, objv[3], &ci) != TCL_OK) {
	return TCL_ERROR;
    }
    extra = 0;
    for (i = 4; i < objc; i++) {
	int length;

	Tcl_GetStringFromObj(objv[i], &length);
	extra += length;
    }
    if (extra == 0) {
	return TCL_OK;
    }
    for (col = Blt_Table_FirstTaggedColumn(&ci); col != NULL; 
	 col = Blt_Table_NextTaggedColumn(&ci)) {
	Blt_TableRow row;
	
	for (row = Blt_Table_FirstTaggedRow(&ri); row != NULL; 
	     row = Blt_Table_NextTaggedRow(&ri)) {
	    int i;

	    for (i = 4; i < objc; i++) {
		const char *s;
		int length;
		
		s = Tcl_GetStringFromObj(objv[i], &length);
		if (Blt_Table_AppendString(interp, table, row, col, s, length) 
		    != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	}
    }	    
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ExportOp --
 *
 *	Parses the given command line and calls one of several export-specific
 *	operations.
 *	
 * Results:
 *	Returns a standard TCL result.  It is the result of operation called.
 *
 *---------------------------------------------------------------------------
 */

static int
ExportOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    DataFormat *fmtPtr;
    TableCmdInterpData *dataPtr;

    dataPtr = GetTableCmdInterpData(interp);
    if (objc == 2) {
	Blt_HashSearch iter;

	for (hPtr = Blt_FirstHashEntry(&dataPtr->fmtTable, &iter); 
	     hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	    fmtPtr = Blt_GetHashValue(hPtr);
	    if (fmtPtr->exportProc != NULL) {
		Tcl_AppendElement(interp, fmtPtr->name);
	    }
	}
	return TCL_OK;
    }
    hPtr = Blt_FindHashEntry(&dataPtr->fmtTable, Tcl_GetString(objv[2]));
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "can't export \"", Tcl_GetString(objv[2]),
			 "\": format not registered", (char *)NULL);
	return TCL_ERROR;
    }
    fmtPtr = Blt_GetHashValue(hPtr);
    if (fmtPtr->exportProc == NULL) {
	Tcl_AppendResult(interp, "no export procedure registered for \"", 
			 fmtPtr->name, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    return (*fmtPtr->exportProc) (cmdPtr->table, interp, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * KeysOp --
 *
 * 	This procedure is invoked to process key operations.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side Effects:
 *	See the user documentation.
 *
 * Example:
 *	$t keys key key key key
 *	$t keys 
 *
 *---------------------------------------------------------------------------
 */
static int
KeysOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Chain keys;
    Blt_Table table;
    int i;

    if (objc == 2) {
	Tcl_Obj *listObjPtr;
	Blt_ChainLink link;

	keys = Blt_Table_GetKeys(cmdPtr->table);
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	for (link = Blt_Chain_FirstLink(keys); link != NULL; 
	     link = Blt_Chain_NextLink(link)) {
	    Blt_TableColumn col;
	    Tcl_Obj *objPtr;
	    
	    col = Blt_Chain_GetValue(link);
	    objPtr = Tcl_NewStringObj(Blt_Table_ColumnLabel(col), -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	Tcl_SetObjResult(interp, listObjPtr);
	return TCL_OK;
    }
    table = cmdPtr->table;
    keys = Blt_Chain_Create();
    for (i = 2; i < objc; i++) {
	Blt_TableColumn col;

	col = Blt_Table_FindColumn(interp, table, objv[i]);
	if (col == NULL) {
	    Blt_Chain_Destroy(keys);
	    return TCL_ERROR;
	}
	Blt_Chain_Append(keys, col);
    }
    Blt_Table_SetKeys(table, keys, 0);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * LappendOp --
 *
 *
 *	Appends one or more elements to the list at the given row, column
 *	location. If the column or row doesn't already exist, it will
 *	automatically be created.
 * 
 * Results:
 *	A standard TCL result. If the tag or index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t append $row $column $value ?value...?
 *
 *---------------------------------------------------------------------------
 */
static int
LappendOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableIterator ri, ci;
    Blt_TableColumn col;

    table = cmdPtr->table;
    if (IterateRows(interp, table, objv[2], &ri) != TCL_OK) {
	return TCL_ERROR;
    }
    if (IterateColumns(interp, table, objv[3], &ci) != TCL_OK) {
	return TCL_ERROR;
    }
    for (col = Blt_Table_FirstTaggedColumn(&ci); col != NULL; 
	 col = Blt_Table_NextTaggedColumn(&ci)) {
	Blt_TableRow row;
	
	for (row = Blt_Table_FirstTaggedRow(&ri); row != NULL; 
	     row = Blt_Table_NextTaggedRow(&ri)) {
	    Tcl_Obj *listObjPtr;
	    int i, result;

	    listObjPtr = Blt_Table_GetObj(table, row, col);
	    if (listObjPtr == NULL) {
		listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	    }
	    Tcl_IncrRefCount(listObjPtr);
	    for (i = 4; i < objc; i++) {
		Tcl_ListObjAppendElement(interp, listObjPtr, objv[i]);
	    }
	    result = Blt_Table_SetObj(table, row, col, listObjPtr);
	    Tcl_DecrRefCount(listObjPtr);
	    if (result != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }	    
    return TCL_OK;
}

static int
LookupOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    long nKeys;
    Blt_Chain keys;
    Blt_TableRow row;
    Blt_Table table;
    long i;

    keys = Blt_Table_GetKeys(cmdPtr->table);
    nKeys = Blt_Chain_GetLength(keys);
    if ((objc - 2) != nKeys) {
	Blt_ChainLink link;

	Tcl_AppendResult(interp, "wrong # of keys: should be \"", (char *)NULL);
	for (link = Blt_Chain_FirstLink(keys); link != NULL; 
	     link = Blt_Chain_NextLink(link)) {
	    Blt_TableColumn col;

	    col = Blt_Chain_GetValue(link);
	    Tcl_AppendResult(interp, Blt_Table_ColumnLabel(col), " ", 
			     (char *)NULL);
	}
	Tcl_AppendResult(interp, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    table = cmdPtr->table;
    if (Blt_Table_KeyLookup(interp, table, objc - 2, objv + 2, &row)!=TCL_OK) {
	return TCL_ERROR;
    }
    i = (row == NULL) ? -1 : Blt_Table_RowIndex(row);
    Tcl_SetLongObj(Tcl_GetObjResult(interp), i);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ImportOp --
 *
 *	Parses the given command line and calls one of several import-specific
 *	operations.
 *	
 * Results:
 *	Returns a standard TCL result.  It is the result of operation called.
 *
 *---------------------------------------------------------------------------
 */
static int
ImportOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    DataFormat *fmtPtr;
    TableCmdInterpData *dataPtr;

    dataPtr = GetTableCmdInterpData(interp);
    if (objc == 2) {
	Blt_HashSearch iter;

	for (hPtr = Blt_FirstHashEntry(&dataPtr->fmtTable, &iter); 
	     hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	    fmtPtr = Blt_GetHashValue(hPtr);
	    if (fmtPtr->importProc != NULL) {
		Tcl_AppendElement(interp, fmtPtr->name);
	    }
	}
	return TCL_OK;
    }
    hPtr = Blt_FindHashEntry(&dataPtr->fmtTable, Tcl_GetString(objv[2]));
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "can't import \"", Tcl_GetString(objv[2]),
			 "\": format not registered", (char *)NULL);
	return TCL_ERROR;
    }
    fmtPtr = Blt_GetHashValue(hPtr);
    if (fmtPtr->importProc == NULL) {
	Tcl_AppendResult(interp, "no import procedure registered for \"", 
		fmtPtr->name, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    return (*fmtPtr->importProc) (cmdPtr->table, interp, objc, objv);
}

/**************** Notify Operations *******************/

/*
 *---------------------------------------------------------------------------
 *
 * NotifyDeleteOp --
 *
 *	Deletes one or more notifiers.  
 *
 * Results:
 *	A standard TCL result.  If a name given doesn't represent a notifier,
 *	then TCL_ERROR is returned and an error message is left in the
 *	interpreter result.
 *
 *---------------------------------------------------------------------------
 */
static int
NotifyDeleteOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, 
	       Tcl_Obj *const *objv)
{
    int i;

    for (i = 3; i < objc; i++) {
	Blt_HashEntry *hPtr;
	NotifierInfo *niPtr;

	hPtr = Blt_FindHashEntry(&cmdPtr->notifyTable, Tcl_GetString(objv[i]));
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "unknown notifier id \"", 
		Tcl_GetString(objv[i]), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	niPtr = Blt_GetHashValue(hPtr);
	Blt_DeleteHashEntry(&cmdPtr->notifyTable, hPtr);
	FreeNotifierInfo(niPtr);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * NotifierInfoOp --
 *
 *	Returns the details for a given notifier.  The string id of the
 *	notifier is passed as an argument.
 *
 * Results:
 *	A standard TCL result.  If the name given doesn't represent a
 *	notifier, then TCL_ERROR is returned and an error message is left in
 *	the interpreter result.  Otherwise the details of the notifier handler
 *	are returned as a list of three elements: notifier id, flags, and
 *	command.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
NotifyInfoOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_HashEntry *hPtr;
    NotifierInfo *niPtr;
    const char *what;
    Tcl_Obj *listObjPtr, *subListObjPtr, *objPtr;
    int i;
    struct _Blt_TableNotifier *notifierPtr;

    table = cmdPtr->table;
    hPtr = Blt_FindHashEntry(&cmdPtr->notifyTable, Tcl_GetString(objv[3]));
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown notifier id \"", 
		Tcl_GetString(objv[3]), "\"", (char *)NULL);
	return TCL_ERROR;
    }

    niPtr = Blt_GetHashValue(hPtr);
    notifierPtr = niPtr->notifier;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    Tcl_ListObjAppendElement(interp, listObjPtr, objv[3]); /* Copy notify Id */

    subListObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    if (notifierPtr->flags & TABLE_NOTIFY_CREATE) {
	objPtr = Tcl_NewStringObj("-create", -1);
	Tcl_ListObjAppendElement(interp, subListObjPtr, objPtr);
    }
    if (notifierPtr->flags & TABLE_NOTIFY_DELETE) {
	objPtr = Tcl_NewStringObj("-delete", -1);
	Tcl_ListObjAppendElement(interp, subListObjPtr, objPtr);
    }
    if (notifierPtr->flags & TABLE_NOTIFY_WHENIDLE) {
	objPtr = Tcl_NewStringObj("-whenidle", -1);
	Tcl_ListObjAppendElement(interp, subListObjPtr, objPtr);
    }
    Tcl_ListObjAppendElement(interp, listObjPtr, subListObjPtr);

    what = (notifierPtr->flags & TABLE_NOTIFY_ROW) ? "row" : "column";
    Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewStringObj(what, -1));
    if (notifierPtr->tag != NULL) {
	Tcl_ListObjAppendElement(interp, listObjPtr, 
			Tcl_NewStringObj(notifierPtr->tag, -1));
    } else {
	Tcl_ListObjAppendElement(interp, listObjPtr, 
	    Tcl_NewLongObj(Blt_Table_RowIndex(notifierPtr->header)));
    }
    subListObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (i = 0; i < niPtr->cmdc; i++) {
	Tcl_ListObjAppendElement(interp, subListObjPtr, niPtr->cmdv[i]);
    }
    Tcl_ListObjAppendElement(interp, listObjPtr, subListObjPtr);

    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * NotifyNamesOp --
 *
 *	Returns the names of all the notifiers in use by this instance.
 *	Notifiers issues by other instances or object clients are not
 *	reported.
 *
 * Results:
 *	Always TCL_OK.  A list of notifier names is left in the interpreter
 *	result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
NotifyNamesOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    Tcl_Obj *listObjPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Blt_FirstHashEntry(&cmdPtr->notifyTable, &iter); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&iter)) {
	Tcl_Obj *objPtr;
	const char *name;

	name = Blt_GetHashKey(&cmdPtr->notifyTable, hPtr);
	objPtr = Tcl_NewStringObj(name, -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * NotifyOp --
 *
 *	Parses the given command line and calls one of several notifier
 *	specific operations.
 *	
 * Results:
 *	Returns a standard TCL result.  It is the result of operation called.
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec notifyOps[] =
{
    {"delete", 1, NotifyDeleteOp, 3, 0, "notifyId...",},
    {"info",   1, NotifyInfoOp,   4, 4, "notifyId",},
    {"names",  1, NotifyNamesOp,  3, 3, "",},
};

static int nNotifyOps = sizeof(notifyOps) / sizeof(Blt_OpSpec);

static int
NotifyOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    CmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nNotifyOps, notifyOps, BLT_OP_ARG2, objc, 
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc)(cmdPtr, interp, objc, objv);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * SortOp --
 *  
 *	$t sort -dictionary -decreasing -list -ascii a b c 
 *
 *---------------------------------------------------------------------------
 */
static int
SortOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableSortOrder *sp, *order;
    SortSwitches switches;
    int i;
    int result;
    long n;
    Blt_TableRow *map;

    table = cmdPtr->table;
    result = TCL_ERROR;

    /* Process switches  */
    switches.flags = 0;
    i = Blt_ParseSwitches(interp, sortSwitches, objc - 2, objv + 2, &switches, 
		BLT_SWITCH_OBJV_PARTIAL);
    if (i < 0) {
	return TCL_ERROR;
    }
    n = objc - i;
    sp = order = Blt_AssertCalloc(n, sizeof(Blt_TableSortOrder));
    for (/*empty*/; i < objc; i++) {
	Blt_TableColumn col;

	col = Blt_Table_FindColumn(interp, table, objv[i]);
	if (col == NULL) {
	    goto error;
	}
	sp->column = col;
    }
    map = Blt_Table_SortRows(table, order, sp - order, switches.flags);
    if (map == NULL) {
	Tcl_AppendResult(interp, "out of memory: can't allocate sort map",
			 (char *)NULL);
	goto error;
    }
    if (switches.flags & SORT_LIST) {
	Tcl_Obj *listObjPtr;
	Blt_TableRow *ip, *iend;
	
	/* Return the new row order as a list. */
	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	for (ip = map, iend = ip + Blt_Table_NumRows(table); ip < iend; ip++) {
	    Tcl_Obj *objPtr;

	    /* Convert the table offset back to a client index. */
	    objPtr = Tcl_NewLongObj(Blt_Table_RowIndex(*ip));
	    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
	}
	Tcl_SetObjResult(interp, listObjPtr);
	Blt_Free(map);
    } else {
	/* Make row order permanent. */
	Blt_Table_SetRowMap(table, map);
    }
    result = TCL_OK;
 error:
    Blt_Free(order);
    return result;
}


/************* Trace Operations *****************/

/*
 *---------------------------------------------------------------------------
 *
 * TraceCreateOp --
 *
 *	Creates a trace for this instance.  Traces represent list of keys, a
 *	bitmask of trace flags, and a command prefix to be invoked when a
 *	matching trace event occurs.
 *
 *	The command prefix is parsed and saved in an array of Tcl_Objs. The
 *	qualified name of the instance is saved also.
 *
 * Results:
 *	A standard TCL result.  The name of the new trace is returned in the
 *	interpreter result.  Otherwise, if it failed to parse a switch, then
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 * Example:
 *	$t trace create row column how command
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TraceCreateOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    Blt_TableIterator ri, ci;
    Blt_TableTrace trace;
    TraceInfo *tiPtr;
    const char *rowTag, *colTag;
    int flags;
    Blt_TableRow row;
    Blt_TableColumn col;
    
    table = cmdPtr->table;
    if (Blt_Table_IterateRows(interp, table, objv[3], &ri) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_Table_IterateColumns(interp, table, objv[4], &ci) != TCL_OK) {
	return TCL_ERROR;
    }
    flags = GetTraceFlags(Tcl_GetString(objv[5]));
    if (flags < 0) {
	Tcl_AppendResult(interp, "unknown flag in \"", Tcl_GetString(objv[5]), 
		"\"", (char *)NULL);
	return TCL_ERROR;
    }
    row = NULL;
    col = NULL;
    colTag = rowTag = NULL;
    if (ri.type == TABLE_ITERATOR_RANGE) {
	Tcl_AppendResult(interp, "can't trace range of rows: use a tag", 
		(char *)NULL);
	return TCL_ERROR;
    }
    if (ci.type == TABLE_ITERATOR_RANGE) {
	Tcl_AppendResult(interp, "can't trace range of columns: use a tag", 
		(char *)NULL);
	return TCL_ERROR;
    }
    if ((ri.type == TABLE_ITERATOR_INDEX) || 
	(ri.type == TABLE_ITERATOR_LABEL)) {
	row = Blt_Table_FirstTaggedRow(&ri);
    } else {
	rowTag = ri.tagName;
    }
    if ((ci.type == TABLE_ITERATOR_INDEX) || 
	(ci.type == TABLE_ITERATOR_LABEL)) {
	col = Blt_Table_FirstTaggedColumn(&ci);
    } else {
	colTag = ci.tagName;
    }
    tiPtr = Blt_Malloc(sizeof(TraceInfo));
    if (tiPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate trace: out of memory", 
		(char *)NULL);
	return TCL_ERROR;
    }
    trace = Blt_Table_CreateTrace(table, row, col, rowTag, colTag, 
	flags, TraceProc, TraceDeleteProc, tiPtr);
    if (trace == NULL) {
	Tcl_AppendResult(interp, "can't create individual trace: out of memory",
		(char *)NULL);
	Blt_Free(tiPtr);
	return TCL_ERROR;
    }
    /* Initialize the trace information structure. */
    tiPtr->cmdPtr = cmdPtr;
    tiPtr->trace = trace;
    tiPtr->tablePtr = &cmdPtr->traceTable;
    {
	Tcl_Obj **elv, **cmdv;
	int elc, i;

	if (Tcl_ListObjGetElements(interp, objv[6], &elc, &elv) != TCL_OK) {
	    return TCL_ERROR;
	}
	/* 
	 * Command + tableName + row + column + flags + NULL
	 */
	cmdv = Blt_AssertCalloc(elc + 1 + 3 + 1, sizeof(Tcl_Obj *));
	for(i = 0; i < elc; i++) {
	    cmdv[i] = elv[i];
	    Tcl_IncrRefCount(cmdv[i]);
	}
	cmdv[i] = Tcl_NewStringObj(cmdPtr->hPtr->key.string, -1);
	Tcl_IncrRefCount(cmdv[i]);
	tiPtr->cmdc = elc;
	tiPtr->cmdv = cmdv;
    }
    {
	int isNew;
	char traceId[200];
	Blt_HashEntry *hPtr;

	do {
	    sprintf_s(traceId, 200, "trace%d", cmdPtr->nextTraceId++);
	    hPtr = Blt_CreateHashEntry(&cmdPtr->traceTable, traceId, &isNew);
	} while (!isNew);
	tiPtr->hPtr = hPtr;
	Blt_SetHashValue(hPtr, tiPtr);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), traceId, -1);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TraceDeleteOp --
 *
 *	Deletes one or more traces.  Can be any type of trace.
 *
 * Results:
 *	A standard TCL result.  If a name given doesn't represent a trace,
 *	then TCL_ERROR is returned and an error message is left in the
 *	interpreter result.
 *
 * Example:
 *	$t trace delete $id...
 *---------------------------------------------------------------------------
 */
static int
TraceDeleteOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int i;

    for (i = 3; i < objc; i++) {
	Blt_HashEntry *hPtr;
	TraceInfo *tiPtr;

	hPtr = Blt_FindHashEntry(&cmdPtr->traceTable, Tcl_GetString(objv[i]));
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "unknown trace \"", 
			     Tcl_GetString(objv[i]), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	tiPtr = Blt_GetHashValue(hPtr);
	Blt_Table_DeleteTrace(tiPtr->trace);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TraceInfoOp --
 *
 *	Returns the details for a given trace.  The name of the trace is
 *	passed as an argument.  The details are returned as a list of
 *	key-value pairs: trace name, tag, row index, keys, flags, and the
 *	command prefix.
 *
 * Results:
 *	A standard TCL result.  If the name given doesn't represent a trace,
 *	then TCL_ERROR is returned and an error message is left in the
 *	interpreter result.
 *
 * Example:
 *	$t trace info $trace
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TraceInfoOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    TraceInfo *tiPtr;
    Tcl_Obj *listObjPtr;

    hPtr = Blt_FindHashEntry(&cmdPtr->traceTable, Tcl_GetString(objv[3]));
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown trace \"", Tcl_GetString(objv[3]), 
		"\"", (char *)NULL);
	return TCL_ERROR;
    }
    tiPtr = Blt_GetHashValue(hPtr);
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    PrintTraceInfo(interp, tiPtr, listObjPtr);
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TraceNamesOp --
 *
 *	Returns the names of all the traces in use by this instance.  Traces
 *	created by other instances or object clients are not reported.
 *
 * Results:
 *	Always TCL_OK.  A list of trace names is left in the interpreter
 *	result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TraceNamesOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    Tcl_Obj *listObjPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Blt_FirstHashEntry(&cmdPtr->traceTable, &iter);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
	Tcl_Obj *objPtr;

	objPtr = Tcl_NewStringObj(Blt_GetHashKey(&cmdPtr->traceTable, hPtr),-1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TraceOp --
 *
 * 	This procedure is invoked to process trace operations.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side Effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec traceOps[] =
{
    {"create", 1, TraceCreateOp, 7, 7, "row column how command",},
    {"delete", 1, TraceDeleteOp, 3, 0, "traceId...",},
    {"info",   1, TraceInfoOp,   4, 4, "traceId",},
    {"names",  1, TraceNamesOp,  3, 3, "",},
};

static int nTraceOps = sizeof(traceOps) / sizeof(Blt_OpSpec);

static int
TraceOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    CmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nTraceOps, traceOps, BLT_OP_ARG2, objc, 
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (cmdPtr, interp, objc, objv);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * SetOp --
 *
 *	Sets one or more key-value pairs for tables.  One or more tables may
 *	be set.  If any of the columns (keys) given don't already exist, the
 *	columns will be automatically created.  The same holds true for rows.
 *	If a row index is beyond the end of the table (tags are always in
 *	range), new rows are allocated.
 * 
 * Results:
 *	A standard TCL result. If the tag or index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t set $row $column $value ?row column value?...
 *
 *---------------------------------------------------------------------------
 */
static int
SetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    int i;

    if (((objc - 2) % 3) != 0) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", 
		Tcl_GetString(objv[0]), 
		" set ?row column value?...\"", (char *)NULL);
	return TCL_ERROR;
    }
    table = cmdPtr->table;
    for (i = 2; i < objc; i += 3) {
	Blt_TableIterator ri, ci;
	Blt_TableColumn col;

	if (IterateRows(interp, table, objv[i], &ri) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (IterateColumns(interp, table, objv[i + 1], &ci) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (col = Blt_Table_FirstTaggedColumn(&ci); col != NULL; 
	     col = Blt_Table_NextTaggedColumn(&ci)) {
	    Blt_TableRow row;

	    for (row = Blt_Table_FirstTaggedRow(&ri); row != NULL; 
		 row = Blt_Table_NextTaggedRow(&ri)) {
		if (Blt_Table_SetObj(table, row, col, objv[i+2]) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	}	    
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * UnsetOp --
 *
 *	$t unset row column ?row column?
 *
 *	Unsets one or more values.  One or more tables may be unset (using
 *	tags).  It's not an error if one of the key names (columns) doesn't
 *	exist.  The same holds true for rows.  If a row index is beyond the
 *	end of the table (tags are always in range), it is also ignored.
 * 
 * Results:
 *	A standard TCL result. If the tag or index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 *---------------------------------------------------------------------------
 */
static int
UnsetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    int i;

    if ((objc - 2) & 1) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", 
		Tcl_GetString(objv[0]), " unset ?row column?...", 
		(char *)NULL);
	return TCL_ERROR;
    }
    table = cmdPtr->table;
    for (i = 2; i < objc; i += 2) {
	Blt_TableIterator ri, ci;
	Blt_TableColumn col;

	if (Blt_Table_IterateRows(NULL, table, objv[i], &ri) != TCL_OK) {
	    return TCL_OK;
	}
	if (Blt_Table_IterateColumns(NULL, table, objv[i+1], &ci) != TCL_OK) {
	    return TCL_OK;
	}
	for (col = Blt_Table_FirstTaggedColumn(&ci); col != NULL; 
	     col = Blt_Table_NextTaggedColumn(&ci)) {
	    Blt_TableRow row;

	    for (row = Blt_Table_FirstTaggedRow(&ri); row != NULL; 
		 row = Blt_Table_NextTaggedRow(&ri)) {

		if (Blt_Table_UnsetValue(table, row, col) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	}	    
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RestoreOp --
 *
 * $t restore $string -overwrite -notags
 * $t restorefile $fileName -overwrite -notags
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
RestoreOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    RestoreSwitches switches;
    int result;

    memset((char *)&switches, 0, sizeof(switches));
    if (Blt_ParseSwitches(interp, restoreSwitches, objc - 2, objv + 2, 
		&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    if ((switches.dataObjPtr != NULL) && (switches.fileObjPtr != NULL)) {
	Tcl_AppendResult(interp, "can't set both -file and -data switches.",
			 (char *)NULL);
	return TCL_ERROR;
    }
    if (switches.dataObjPtr != NULL) {
	result = Blt_Table_Restore(interp, cmdPtr->table, 
		Tcl_GetString(switches.dataObjPtr), switches.flags);
    } else if (switches.fileObjPtr != NULL) {
	result = Blt_Table_FileRestore(interp, cmdPtr->table, 
		Tcl_GetString(switches.fileObjPtr), switches.flags);
    } else {
	result = Blt_Table_FileRestore(interp, cmdPtr->table, "out.dump", 
		switches.flags);
    }
    Blt_FreeSwitches(restoreSwitches, &switches, 0);
    return result;
}

static int
WriteRecord(Tcl_Channel channel, Tcl_DString *dsPtr)
{
    int length, nWritten;
    char *line;

    length = Tcl_DStringLength(dsPtr);
    line = Tcl_DStringValue(dsPtr);
#if HAVE_UTF
#ifdef notdef
    nWritten = Tcl_WriteChars(channel, line, length);
#endif
    nWritten = Tcl_Write(channel, line, length);
#else
    nWritten = Tcl_Write(channel, line, length);
#endif
    if (nWritten != length) {
	return FALSE;
    }
    Tcl_DStringSetLength(dsPtr, 0);
    return TRUE;
}


/*
 *---------------------------------------------------------------------------
 *
 * DumpHeader --
 *
 *	Prints the info associated with a column into a dynamic
 *	string.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static int
DumpHeader(DumpSwitches *dumpPtr, long nRows, long nCols)
{
    /* i rows columns ctime mtime \n */
    Tcl_DStringAppendElement(dumpPtr->dsPtr, "i");

    /* # of rows and columns may be a subset of the table. */
    Tcl_DStringAppendElement(dumpPtr->dsPtr, Blt_Ltoa(nRows));
    Tcl_DStringAppendElement(dumpPtr->dsPtr, Blt_Ltoa(nCols));

    Tcl_DStringAppendElement(dumpPtr->dsPtr, Blt_Ltoa(0));
    Tcl_DStringAppendElement(dumpPtr->dsPtr, Blt_Ltoa(0));
    Tcl_DStringAppend(dumpPtr->dsPtr, "\n", 1);
    if (dumpPtr->channel != NULL) {
	return WriteRecord(dumpPtr->channel, dumpPtr->dsPtr);
    }
    return TRUE;
}


/*
 *---------------------------------------------------------------------------
 *
 * DumpValue --
 *
 *	Retrieves all tags for a given row or column into a tcl list.  
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static int
DumpValue(Blt_Table table, DumpSwitches *dumpPtr, Blt_TableRow row, 
	  Blt_TableColumn col)
{
    const char *string;

    string = Blt_Table_GetString(table, row, col);
    if (string == NULL) {
	return TRUE;
    }
    /* d row column value \n */
    Tcl_DStringAppendElement(dumpPtr->dsPtr, "d");
    Tcl_DStringAppendElement(dumpPtr->dsPtr, Blt_Ltoa(Blt_Table_RowIndex(row)));
    Tcl_DStringAppendElement(dumpPtr->dsPtr, Blt_Ltoa(Blt_Table_ColumnIndex(col)));
    Tcl_DStringAppendElement(dumpPtr->dsPtr, string);
    Tcl_DStringAppend(dumpPtr->dsPtr, "\n", 1);
    if (dumpPtr->channel != NULL) {
	return WriteRecord(dumpPtr->channel, dumpPtr->dsPtr);
    }
    return TRUE;
}

/*
 *---------------------------------------------------------------------------
 *
 * DumpColumn --
 *
 *	Prints the info associated with a column into a dynamic string.
 *
 *---------------------------------------------------------------------------
 */
static int
DumpColumn(Blt_Table table, DumpSwitches *dumpPtr, Blt_TableColumn col)
{
    Blt_Chain colTags;
    Blt_ChainLink link;
    const char *name;

    /* c index label type tags \n */
    Tcl_DStringAppendElement(dumpPtr->dsPtr, "c");
    Tcl_DStringAppendElement(dumpPtr->dsPtr, Blt_Ltoa(Blt_Table_ColumnIndex(col)));
    Tcl_DStringAppendElement(dumpPtr->dsPtr, Blt_Table_ColumnLabel(col));
    name = Blt_Table_NameOfType(Blt_Table_ColumnType(col));
    if (name == NULL) {
	name = "";
    }
    Tcl_DStringAppendElement(dumpPtr->dsPtr, name);

    colTags = Blt_Table_ColumnTags(table, col);
    Tcl_DStringStartSublist(dumpPtr->dsPtr);
    for (link = Blt_Chain_FirstLink(colTags); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	const char *tagName;

	tagName = Blt_Chain_GetValue(link);
	Tcl_DStringAppendElement(dumpPtr->dsPtr, tagName);
    }
    Blt_Chain_Destroy(colTags);
    Tcl_DStringEndSublist(dumpPtr->dsPtr);
    Tcl_DStringAppend(dumpPtr->dsPtr, "\n", 1);
    if (dumpPtr->channel != NULL) {
	return WriteRecord(dumpPtr->channel, dumpPtr->dsPtr);
    }
    return TRUE;
}

/*
 *---------------------------------------------------------------------------
 *
 * DumpRow --
 *
 *	Prints the info associated with a row into a dynamic string.
 *
 *---------------------------------------------------------------------------
 */
static int
DumpRow(Blt_Table table, DumpSwitches *dumpPtr, Blt_TableRow row)
{
    Blt_Chain rowTags;
    Blt_ChainLink link;

    /* r index label tags \n */
    Tcl_DStringAppendElement(dumpPtr->dsPtr, "r");
    Tcl_DStringAppendElement(dumpPtr->dsPtr, Blt_Ltoa(Blt_Table_RowIndex(row)));
    Tcl_DStringAppendElement(dumpPtr->dsPtr, (char *)Blt_Table_RowLabel(row));
    Tcl_DStringStartSublist(dumpPtr->dsPtr);
    rowTags = Blt_Table_RowTags(table, row);
    for (link = Blt_Chain_FirstLink(rowTags); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	const char *tagName;

	tagName = Blt_Chain_GetValue(link);
	Tcl_DStringAppendElement(dumpPtr->dsPtr, tagName);
    }
    Blt_Chain_Destroy(rowTags);
    Tcl_DStringEndSublist(dumpPtr->dsPtr);
    Tcl_DStringAppend(dumpPtr->dsPtr, "\n", 1);
    if (dumpPtr->channel != NULL) {
	return WriteRecord(dumpPtr->channel, dumpPtr->dsPtr);
    }
    return TRUE;
}


/*
 *---------------------------------------------------------------------------
 *
 * DumpTable --
 *
 *	Dumps data from the given table based upon the row and column maps
 *	provided which describe what rows and columns are to be dumped. The
 *	dump information is written to the file named. If the file name starts
 *	with an '@', then it is the name of an already opened channel to be
 *	used.
 *	
 * Results:
 *	A standard TCL result.  If the dump was successful, TCL_OK is
 *	returned.  Otherwise, TCL_ERROR is returned and an error message is
 *	left in the interpreter result.
 *
 * Side Effects:
 *	Dump information is written to the named file.
 *
 *---------------------------------------------------------------------------
 */
static int
DumpTable(Blt_Table table, DumpSwitches *dumpPtr)
{
    int result;
    long nCols, nRows;
    Blt_TableColumn col;
    Blt_TableRow row;

    if (dumpPtr->ri.chain != NULL) {
	nRows = Blt_Chain_GetLength(dumpPtr->ri.chain);
    } else {
	nRows = Blt_Table_NumRows(table);
    }
    if (dumpPtr->ci.chain != NULL) {
	nCols = Blt_Chain_GetLength(dumpPtr->ci.chain);
    } else {
	nCols = Blt_Table_NumColumns(table);
    }
    result = DumpHeader(dumpPtr, nRows, nCols);
    for (col = Blt_Table_FirstTaggedColumn(&dumpPtr->ci); 
	 (result) && (col != NULL); 
	 col = Blt_Table_NextTaggedColumn(&dumpPtr->ci)) {
	result = DumpColumn(table, dumpPtr, col);
    }
    for (row = Blt_Table_FirstTaggedRow(&dumpPtr->ri); 
	 (result) && (row != NULL); 
	 row = Blt_Table_NextTaggedRow(&dumpPtr->ri)) {
	result = DumpRow(table, dumpPtr, row);
    }
    for (col = Blt_Table_FirstTaggedColumn(&dumpPtr->ci); 
	 (result) && (col != NULL); 
	 col = Blt_Table_NextTaggedColumn(&dumpPtr->ci)) {
	for (row = Blt_Table_FirstTaggedRow(&dumpPtr->ri); 
	     (result) && (row != NULL); 
	     row = Blt_Table_NextTaggedRow(&dumpPtr->ri)) {
	    result = DumpValue(table, dumpPtr, row, col);
	}
    }
    return (result) ? TCL_OK : TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * DumpOp --
 *
 * set data [$t dump -rows {} -columns {}]
 * $t dump -file fileName -rows {} -columns {} 
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DumpOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Blt_Table table;
    DumpSwitches switches;
    int result;
    Tcl_DString ds;
    int closeChannel;
    Tcl_Channel channel;

    closeChannel = FALSE;
    channel = NULL;
    table = cmdPtr->table;
    result = TCL_ERROR;
    memset(&switches, 0, sizeof(switches));
    switches.channel = channel;
    switches.dsPtr = &ds;
    rowIterSwitch.clientData = cmdPtr->table;
    columnIterSwitch.clientData = cmdPtr->table;
    Blt_Table_IterateAllRows(table, &switches.ri);
    Blt_Table_IterateAllColumns(table, &switches.ci);
    if (Blt_ParseSwitches(interp, dumpSwitches, objc - 2, objv + 2, &switches, 
	BLT_SWITCH_DEFAULTS) < 0) {
	goto error;
    }
    if (switches.fileObjPtr != NULL) {
	const char *fileName;

	fileName = Tcl_GetString(switches.fileObjPtr);

	closeChannel = TRUE;
	if ((fileName[0] == '@') && (fileName[1] != '\0')) {
	    int mode;
	    
	    channel = Tcl_GetChannel(interp, fileName+1, &mode);
	    if (channel == NULL) {
		goto error;
	    }
	    if ((mode & TCL_WRITABLE) == 0) {
		Tcl_AppendResult(interp, "can't dump table: channel \"", 
			fileName, "\" not opened for writing", (char *)NULL);
		goto error;
	    }
	    closeChannel = FALSE;
	} else {
	    channel = Tcl_OpenFileChannel(interp, fileName, "w", 0666);
	    if (channel == NULL) {
		goto error;
	    }
	}
	switches.channel = channel;
    }
    Tcl_DStringInit(&ds);
    result = DumpTable(table, &switches);
    if ((switches.channel == NULL) && (result == TCL_OK)) {
	Tcl_DStringResult(interp, &ds);
    }
    Tcl_DStringFree(&ds);
 error:
    if (closeChannel) {
	Tcl_Close(interp, channel);
    }
    Blt_FreeSwitches(dumpSwitches, &switches, 0);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * EmptyValueOp --
 *
 *	$t emptyvalue ?$value?
 *
 *---------------------------------------------------------------------------
 */
static int
EmptyValueOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_SetStringObj(Tcl_GetObjResult(interp), cmdPtr->emptyValue, -1);
    if (objc == 3) {
	if (cmdPtr->emptyValue != NULL) {
	    Blt_Free(cmdPtr->emptyValue);
	    cmdPtr->emptyValue = Blt_AssertStrdup(Tcl_GetString(objv[2]));
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ExistsOp --
 *
 *	$t exists $row $column
 *
 *---------------------------------------------------------------------------
 */
static int
ExistsOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    int bool;
    Blt_TableRow row;
    Blt_TableColumn col;

    bool = FALSE;
    row = Blt_Table_FindRow(NULL, cmdPtr->table, objv[2]);
    col = Blt_Table_FindColumn(NULL, cmdPtr->table, objv[3]);
    if ((row != NULL) && (col != NULL)) {
	bool = Blt_Table_ValueExists(cmdPtr->table, row, col);
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), bool);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * FindOp --
 *
 *	Parses the given command line and calls one of several export-specific
 *	operations.
 *	
 * Results:
 *	Returns a standard TCL result.  It is the result of operation called.
 *
 * Example:
 *	$t find expr ?switches?
 *
 *---------------------------------------------------------------------------
 */
static int
FindOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    FindSwitches switches;
    int result;

    memset(&switches, 0, sizeof(switches));
    rowIterSwitch.clientData = cmdPtr->table;
    Blt_Table_IterateAllRows(cmdPtr->table, &switches.iter);

    if (Blt_ParseSwitches(interp, findSwitches, objc - 3, objv + 3, 
	&switches, BLT_SWITCH_DEFAULTS) < 0) {
	return TCL_ERROR;
    }
    switches.table = cmdPtr->table;
    Blt_InitHashTable(&switches.varTable, BLT_ONE_WORD_KEYS);
    result = FindRows(interp, cmdPtr->table, objv[2], &switches);
    Blt_FreeSwitches(findSwitches, &switches, 0);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetOp --
 *
 *	Retrieves the value from a given table for a designated row,column
 *	location.
 *
 *	Normally it's an error if the column or row key is invalid or the data
 *	slot is empty (the Tcl_Obj is NULL). But if an extra argument is
 *	provided, then it is returned as a default value.
 * 
 * Results:
 *	A standard TCL result. If the tag or index is invalid, TCL_ERROR is
 *	returned and an error message is left in the interpreter result.
 *	
 * Example:
 *	$t get row column ?defValue?
 *
 *---------------------------------------------------------------------------
 */
static int
GetOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Tcl_Obj *objPtr;
    Blt_TableRow row;
    Blt_TableColumn col;

    row = Blt_Table_FindRow(interp, cmdPtr->table, objv[2]);
    if (row == NULL) {
	if (objc == 5) {
	    objPtr = objv[4];
	    goto done;
	}
	return TCL_ERROR;
    } 
    col = Blt_Table_FindColumn(interp, cmdPtr->table, objv[3]);
    if (col == NULL) {
	if (objc == 5) {
	    objPtr = objv[4];
	    goto done;
	}
	return TCL_ERROR;
    } 
    objPtr = Blt_Table_GetObj(cmdPtr->table, row, col);
    if (objPtr == NULL) {
	if (objc == 5) {
	    objPtr = objv[4];
	} else {
	    objPtr = Tcl_NewStringObj(cmdPtr->emptyValue, -1);
	}
    }
 done:
    Tcl_SetObjResult(interp, objPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * AttachOp --
 *
 *---------------------------------------------------------------------------
 */
static int
AttachOp(Cmd *cmdPtr, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    if (objc == 3) {
	const char *qualName;
	Blt_ObjectName objName;
	Blt_Table table;
	Tcl_DString ds;
	int result;

	if (!Blt_ParseObjectName(interp, Tcl_GetString(objv[2]), &objName, 0)) {
	    return TCL_ERROR;
	}
	qualName = Blt_MakeQualifiedName(&objName, &ds);
	result = Blt_Table_Open(interp, qualName, &table);
	Tcl_DStringFree(&ds);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	if (cmdPtr->table != NULL) {
	    Blt_HashEntry *hPtr;
	    Blt_HashSearch iter;
	    
	    Blt_Table_Close(cmdPtr->table);
	    /* Free the current set of tags, traces, and notifiers. */
	    for (hPtr = Blt_FirstHashEntry(&cmdPtr->traceTable, &iter); 
		 hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
		TraceInfo *tiPtr;

		tiPtr = Blt_GetHashValue(hPtr);
		Blt_Table_DeleteTrace(tiPtr->trace);
	    }
	    Blt_DeleteHashTable(&cmdPtr->traceTable);
	    Blt_InitHashTable(&cmdPtr->traceTable, TCL_STRING_KEYS);
	    for (hPtr = Blt_FirstHashEntry(&cmdPtr->notifyTable, &iter); 
		hPtr != NULL; hPtr = Blt_NextHashEntry(&iter)) {
		NotifierInfo *niPtr;

		niPtr = Blt_GetHashValue(hPtr);
		FreeNotifierInfo(niPtr);
	    }
	    Blt_DeleteHashTable(&cmdPtr->notifyTable);
	    Blt_InitHashTable(&cmdPtr->notifyTable, TCL_STRING_KEYS);
	}
	cmdPtr->table = table;
    }
    Tcl_SetStringObj(Tcl_GetObjResult(interp), 
		     Blt_Table_TableName(cmdPtr->table), -1);
    return TCL_OK;
}

static Blt_OpSpec tableOps[] =
{
    {"add",        2, AddOp,        3, 0, "table ?switches?",},
    {"append",     2, AppendOp,     5, 0, "row column value ?value...?",},
    {"attach",     2, AttachOp,     3, 0, "args...",},
    {"column",     3, ColumnOp,     3, 0, "op args...",},
#ifdef notdef
    {"dup",        1, DupOp,        3, 0, "args...",},
#endif
    {"dump",       1, DumpOp,       2, 0, "?switches?",},
    {"emptyvalue", 2, EmptyValueOp, 2, 3, "?newValue?",},
    {"exists",     3, ExistsOp,     4, 4, "row column",},
    {"export",     3, ExportOp,     2, 0, "format args...",},
    {"find",	   1, FindOp,	    3, 0, "expr ?switches?",},
    {"get",        1, GetOp,        4, 5, "row column ?defValue?",},
    {"import",     1, ImportOp,     2, 0, "format args...",},
    {"keys",       1, KeysOp,       2, 0, "?column...?",},
    {"lappend",    2, LappendOp,    5, 0, "row column value ?value...?",},
    {"lookup",     2, LookupOp,     2, 0, "?value...?",},
    {"notify",     1, NotifyOp,     2, 0, "op args...",},
    {"restore",    2, RestoreOp,    2, 0, "?switches?",},
    {"row",        2, RowOp,        3, 0, "op args...",},
    {"set",        2, SetOp,        3, 0, "?row column value?...",},
    {"sort",       2, SortOp,       3, 0, "?flags...?",},
    {"trace",      2, TraceOp,      2, 0, "op args...",},
    {"unset",      1, UnsetOp,      4, 0, "row column ?row column?",},
#ifdef notplanned
    {"-apply",     1, ApplyOp,      3, 0, "first last ?switches?",},
    {"copy",       2, CopyOp,       4, 0, 
	"srcNode ?destTable? destNode ?switches?",},
#endif
};

static int nTableOps = sizeof(tableOps) / sizeof(Blt_OpSpec);

/*
 *---------------------------------------------------------------------------
 *
 * TableInstObjCmd --
 *
 * 	This procedure is invoked to process commands on behalf of * the
 * 	instance of the table-object.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side Effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
static int
TableInstObjCmd(
    ClientData clientData,		/* Pointer to the datatable command
					 * structure. */
    Tcl_Interp *interp,			/* Interpreter to report errors. */
    int objc,				/* # of arguments. */
    Tcl_Obj *const *objv)		/* Vector of argument strings. */
{
    CmdProc *proc;
    Cmd *cmdPtr = clientData;
    int result;

    proc = Blt_GetOpFromObj(interp, nTableOps, tableOps, BLT_OP_ARG1, objc, 
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve(cmdPtr);
    result = (*proc) (cmdPtr, interp, objc, objv);
    Tcl_Release(cmdPtr);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * TableInstDeleteProc --
 *
 *	Deletes the command associated with the table.  This is called only
 *	when the command associated with the table is destroyed.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The table object is released and bookkeeping data for traces and
 *	notifiers are freed.
 *
 *---------------------------------------------------------------------------
 */
static void
TableInstDeleteProc(ClientData clientData)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    Cmd *cmdPtr = clientData;

    for (hPtr = Blt_FirstHashEntry(&cmdPtr->traceTable, &iter); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&iter)) {
	TraceInfo *tiPtr;

	tiPtr = Blt_GetHashValue(hPtr);
	Blt_Table_DeleteTrace(tiPtr->trace);
    }
    Blt_DeleteHashTable(&cmdPtr->traceTable);
    for (hPtr = Blt_FirstHashEntry(&cmdPtr->notifyTable, &iter); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&iter)) {
	NotifierInfo *niPtr;

	niPtr = Blt_GetHashValue(hPtr);
	FreeNotifierInfo(niPtr);
    }
    if (cmdPtr->emptyValue != NULL) {
	Blt_Free(cmdPtr->emptyValue);
    }
    Blt_DeleteHashTable(&cmdPtr->notifyTable);
    if (cmdPtr->hPtr != NULL) {
	Blt_DeleteHashEntry(cmdPtr->tablePtr, cmdPtr->hPtr);
    }
    Blt_Table_Close(cmdPtr->table);
    Blt_Free(cmdPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TableCreateOp --
 *
 *	Creates a new instance of a table object.  
 *
 *	This routine insures that instance and object names are the same.  For
 *	example, you can't create an instance with the name of an object that
 *	already exists.  And because each instance has a TCL command
 *	associated with it (used to access the object), we additionally check
 *	more that it's not an existing TCL command.
 *
 *	Instance names are namespace-qualified.  If the given name doesn't
 *	have a namespace qualifier, that instance will be created in the
 *	current namespace (not the global namespace).
 *	
 * Results:
 *	A standard TCL result.  If the instance is successfully created, the
 *	namespace-qualified name of the instance is returned. If not, then
 *	TCL_ERROR is returned and an error message is left in the interpreter
 *	result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TableCreateOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	      Tcl_Obj *const *objv)
{
    const char *instName;
    Tcl_DString ds;
    Blt_Table table;

    instName = NULL;
    if (objc == 3) {
	instName = Tcl_GetString(objv[2]);
    }
    Tcl_DStringInit(&ds);
    if (instName == NULL) {
	instName = GenerateName(interp, "", "", &ds);
    } else {
	char *p;

	p = strstr(instName, "#auto");
	if (p != NULL) {
	    *p = '\0';
	    instName = GenerateName(interp, instName, p + 5, &ds);
	    *p = '#';
	} else {
	    Blt_ObjectName objName;
	    Tcl_CmdInfo cmdInfo;

	    /* 
	     * Parse the command and put back so that it's in a consistent
	     * format.  
	     *
	     *	t1         <current namespace>::t1
	     *	n1::t1     <current namespace>::n1::t1
	     *	::t1	   ::t1
	     *  ::n1::t1   ::n1::t1
	     */
	    if (!Blt_ParseObjectName(interp, instName, &objName, 0)) {
		return TCL_ERROR;
	    }
	    instName = Blt_MakeQualifiedName(&objName, &ds);
	    /* 
	     * Check if the command already exists. 
	     */
	    if (Tcl_GetCommandInfo(interp, (char *)instName, &cmdInfo)) {
		Tcl_AppendResult(interp, "a command \"", instName,
				 "\" already exists", (char *)NULL);
		goto error;
	    }
	    if (Blt_Table_TableExists(interp, instName)) {
		Tcl_AppendResult(interp, "a table \"", instName, 
			"\" already exists", (char *)NULL);
		goto error;
	    }
	} 
    } 
    if (instName == NULL) {
	goto error;
    }
    if (Blt_Table_CreateTable(interp, instName, &table) == TCL_OK) {
	Cmd *cmdPtr;

	cmdPtr = NewTableCmd(interp, table, instName);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), instName, -1);
	Tcl_DStringFree(&ds);
	return TCL_OK;
    }
 error:
    Tcl_DStringFree(&ds);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * TableDestroyOp --
 *
 *	Deletes one or more instances of table objects.  The deletion process
 *	is done by deleting the TCL command associated with the object.
 *	
 * Results:
 *	A standard TCL result.  If one of the names given doesn't represent an
 *	instance, TCL_ERROR is returned and an error message is left in the
 *	interpreter result.
 *
 *---------------------------------------------------------------------------
 */
static int
TableDestroyOp(ClientData clientData, Tcl_Interp *interp, int objc,
	       Tcl_Obj *const *objv)
{
    int i;

    for (i = 2; i < objc; i++) {
	Cmd *cmdPtr;

	cmdPtr = GetTableCmd(interp, Tcl_GetString(objv[i]));
	if (cmdPtr == NULL) {
	    Tcl_AppendResult(interp, "can't find table \"", 
		Tcl_GetString(objv[i]), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	Tcl_DeleteCommandFromToken(interp, cmdPtr->cmdToken);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TableNamesOp --
 *
 *	Returns the names of all the table-object instances matching a given
 *	pattern.  If no pattern argument is provided, then all object names
 *	are returned.  The names returned are namespace qualified.
 *	
 * Results:
 *	Always returns TCL_OK.  The names of the matching objects is returned
 *	via the interpreter result.
 *
 * Example:
 *	$t names ?pattern?
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TableNamesOp(ClientData clientData, Tcl_Interp *interp, int objc,
	     Tcl_Obj *const *objv)
{
    TableCmdInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    Tcl_Obj *listObjPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Blt_FirstHashEntry(&dataPtr->instTable, &iter); hPtr != NULL;
	 hPtr = Blt_NextHashEntry(&iter)) {
	const char *name;
	int match;
	int i;

	name = Blt_GetHashKey(&dataPtr->instTable, hPtr);
	match = TRUE;
	for (i = 2; i < objc; i++) {
	    match = Tcl_StringMatch(name, Tcl_GetString(objv[i]));
	    if (match) {
		break;
	    }
	}
	if (!match) {
	    continue;
	}
	Tcl_ListObjAppendElement(interp, listObjPtr,
		Tcl_NewStringObj(name, -1));
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
TableLoadOp(ClientData clientData, Tcl_Interp *interp, int objc, 
	    Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    TableCmdInterpData *dataPtr = clientData;
    Tcl_DString libName;
    const char *fmt;
    char *safeProcName, *initProcName;
    int result;
    int length;

    fmt = Tcl_GetStringFromObj(objv[2], &length);
    hPtr = Blt_FindHashEntry(&dataPtr->fmtTable, fmt);
    if (hPtr != NULL) {
	return TCL_OK;		/* Converter is already loaded. */
    }
    Tcl_DStringInit(&libName);
    {
	Tcl_DString pathName;
	const char *path;

	Tcl_DStringInit(&pathName);
	path = Tcl_TranslateFileName(interp, Tcl_GetString(objv[3]), &pathName);
	if (path == NULL) {
	    Tcl_DStringFree(&pathName);
	    return TCL_ERROR;
	}
	Tcl_DStringAppend(&libName, path, -1);
	Tcl_DStringFree(&pathName);
    }
    Tcl_DStringAppend(&libName, "/", -1);
    Tcl_UtfToTitle((char *)fmt);
    Tcl_DStringAppend(&libName, "Table", 5);
    Tcl_DStringAppend(&libName, fmt, -1);
    Tcl_DStringAppend(&libName, Blt_Itoa(BLT_MAJOR_VERSION), 1);
    Tcl_DStringAppend(&libName, Blt_Itoa(BLT_MINOR_VERSION), 1);
    Tcl_DStringAppend(&libName, BLT_LIB_SUFFIX, -1);
    Tcl_DStringAppend(&libName, BLT_SO_EXT, -1);

    initProcName = Blt_AssertMalloc(10 + length + 4 + 1);
    sprintf_s(initProcName, 10 + length + 4 + 1, "Blt_Table_%sInit", fmt);
    safeProcName = Blt_AssertMalloc(10 + length + 8 + 1);
    sprintf_s(safeProcName, 10 + length + 8 + 1, "Blt_Table_%sSafeInit",fmt);

    result = Blt_LoadLibrary(interp, Tcl_DStringValue(&libName), initProcName, 
	safeProcName); 
    Tcl_DStringFree(&libName);
    if (safeProcName != NULL) {
	Blt_Free(safeProcName);
    }
    if (initProcName != NULL) {
	Blt_Free(initProcName);
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * TableObjCmd --
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec tableCmdOps[] =
{
    {"create",  1, TableCreateOp,  2, 3, "?name?",},
    {"destroy", 1, TableDestroyOp, 3, 0, "name...",},
    {"load",    1, TableLoadOp,    4, 4, "name libpath",},
    {"names",   1, TableNamesOp,   2, 3, "?pattern?...",},
};

static int nCmdOps = sizeof(tableCmdOps) / sizeof(Blt_OpSpec);

/*ARGSUSED*/
static int
TableObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, 
	    Tcl_Obj *const *objv)
{
    CmdProc *proc;

    proc = Blt_GetOpFromObj(interp, nCmdOps, tableCmdOps, BLT_OP_ARG1, objc, 
	objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc) (clientData, interp, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * TableInterpDeleteProc --
 *
 *	This is called when the interpreter registering the "datatable"
 *	command is deleted.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Removes the hash table managing all table names.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
TableInterpDeleteProc(ClientData clientData, Tcl_Interp *interp)
{
    TableCmdInterpData *dataPtr = clientData;

    /* All table instances should already have been destroyed when their
     * respective TCL commands were deleted. */
    Blt_DeleteHashTable(&dataPtr->instTable);
    Blt_DeleteHashTable(&dataPtr->fmtTable);
    Blt_DeleteHashTable(&dataPtr->findTable);
    Tcl_DeleteAssocData(interp, TABLE_THREAD_KEY);
    Blt_Free(dataPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_TableCmdInitProc --
 *
 *	This procedure is invoked to initialize the "dtable" command.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Creates the new command and adds a new entry into a global Tcl
 *	associative array.
 *
 *---------------------------------------------------------------------------
 */
int
Blt_TableCmdInitProc(Tcl_Interp *interp)
{
    static Blt_InitCmdSpec cmdSpec = { "datatable", TableObjCmd, };

    cmdSpec.clientData = GetTableCmdInterpData(interp);
    if (Blt_InitCmd(interp, "::blt", &cmdSpec) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/* Dump table to dbm */
/* Convert node data to datablock */

int
Blt_Table_RegisterFormat(Tcl_Interp *interp, const char *fmt, 
			 Blt_TableImportProc *importProc, 
			 Blt_TableExportProc *exportProc)
{
    Blt_HashEntry *hPtr;
    DataFormat *fmtPtr;
    TableCmdInterpData *dataPtr;
    int isNew;

    dataPtr = GetTableCmdInterpData(interp);
    hPtr = Blt_CreateHashEntry(&dataPtr->fmtTable, fmt, &isNew);
    if (isNew) {
	fmtPtr = Blt_AssertMalloc(sizeof(DataFormat));
	fmtPtr->name = Blt_AssertStrdup(fmt);
	Blt_SetHashValue(hPtr, fmtPtr);
    } else {
	fmtPtr = Blt_GetHashValue(hPtr);
    }
    fmtPtr->isLoaded = TRUE;
    fmtPtr->importProc = importProc;
    fmtPtr->exportProc = exportProc;
    return TCL_OK;
}

