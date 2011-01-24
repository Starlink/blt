
/*
 * bltTree.c --
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

#include "bltInt.h"

/* TODO:
 *	Traces and notifiers should be in one list in tree object.
 *	Notifier is always fired.
 *	Incorporate first/next tag routines ?
 */

#include "bltNsUtil.h"
#include "bltArrayObj.h"
#include "bltTree.h"

static Tcl_InterpDeleteProc TreeInterpDeleteProc;
static Blt_TreeApplyProc SizeApplyProc;
static Tcl_IdleProc NotifyIdleProc;
static Tcl_IdleProc TraceIdleProc;

#define TREE_THREAD_KEY		"BLT Tree Data"
#define TREE_MAGIC		((unsigned int) 0x46170277)
#define TREE_DESTROYED		(1<<0)

#define TREE_NODE_REBUILD_SIZE	3U

typedef struct _Blt_TreeNode Node;
typedef struct _Blt_Tree Tree;
typedef struct _Blt_TreeObject TreeObject;
typedef struct _Blt_TreeValue Value;

/*
 * Blt_TreeValue --
 *
 *	A tree node may have zero or more data fields or values that are
 *	represented by these container structures.  Each data field has both
 *	the name of the field (Blt_TreeKey) and its data (Tcl_Obj).  Values
 *	are private or public.  Private values are only be seen by the tree
 *	client that created the field.
 * 
 *	Values are organized in two ways. They are stored in a linked list in
 *	order that they were created.  In addition, they may be placed into a
 *	hash table when the number of values reaches a high-water mark.
 *
 */
struct _Blt_TreeValue {
    Blt_TreeKey key;		/* String identifying the data field */
    Tcl_Obj *objPtr;		/* Data representation. */
    Blt_Tree owner;		/* Non-NULL if privately owned. */
    Blt_TreeValue next;		/* Next value in the chain. */
    Blt_TreeValue hnext;	/* Next value in hash table. */
};

typedef struct {
    Tree *treePtr;
    unsigned int flags;
    Node *rootPtr;
    Blt_HashTable idTable;
    int nLines;
    Blt_HashTable dataTable;
} RestoreInfo;

#include <stdio.h>
#include <string.h>
/* The following header is required for LP64 compilation */
#include <stdlib.h>

#include "bltHash.h"

static void TreeDestroyValues(Blt_TreeNode node);

static Value *TreeFindValue(Blt_TreeNode node, Blt_TreeKey key);
static Value *TreeCreateValue(Blt_TreeNode node, Blt_TreeKey key, int *newPtr);

static int TreeDeleteValue(Blt_TreeNode node, Blt_TreeValue value);

static Value *TreeFirstValue(Blt_TreeNode, Blt_TreeKeyIterator *iterPtr);

static Value *TreeNextValue(Blt_TreeKeyIterator *iterPtr);

/*
 * When there are this many entries per bucket, on average, rebuild the hash
 * table to make it larger.
 */
#define REBUILD_MULTIPLIER  3
#define START_LOGSIZE       5	/* Initial hash table size is 32. */
#define HASH_HIGH_WATER	    20	/* Start a hash table when a node has this
				 * many values or child nodes. */
#define HASH_LOW_WATER	    (HASH_HIGH_WATER << 1)

#if (SIZEOF_VOID_P == 8)
#define RANDOM_INDEX(i)		HashOneWord(mask, downshift, i)
static Blt_Hash HashOneWord(uint64_t mask, unsigned int downshift, 
	const void *key);
#define BITSPERWORD		64
#else 

/*
 * The following macro takes a preliminary integer hash value and produces an
 * index into a hash tables bucket list.  The idea is to make it so that
 * preliminary values that are arbitrarily similar will end up in different
 * buckets.  The hash function was taken from a random-number generator.
 */
#define RANDOM_INDEX(i) \
    (((((long) (i))*1103515245) >> downshift) & mask)
#define BITSPERWORD		32
#endif /* SIZEOF_VOID_P == 8 */

#define DOWNSHIFT_START		(BITSPERWORD - 2) 

/*
 * The hash table below is used to keep track of all the Blt_TreeKeys created
 * so far.
 */
typedef struct _Blt_TreeInterpData {
    Tcl_Interp *interp;
    Blt_HashTable treeTable;	/* Table of trees. */
    Blt_HashTable keyTable;	/* Table of string keys, shared among all the
				 * trees within an interpreter. */
    unsigned int nextId;	/* Identifier used to generate automatic tree
				 * names. */
} TreeInterpData;

typedef struct {
    Tcl_Interp *interp;
    ClientData clientData;
    Blt_TreeKey key;
    Blt_TreeNotifyEventProc *proc;
    Blt_TreeNotifyEvent event;
    unsigned int mask;
    int notifyPending;
} NotifyEventHandler;

typedef struct {
    /* This must match _Blt_TreeTrace in bltTree.h */
    ClientData clientData;
    const char *keyPattern;		/* Pattern to be matched */
    Node *nodePtr;			/* Node to be matched.  */
    unsigned int mask;
    Blt_TreeTraceProc *proc;

    /* Private fields */
    const char *withTag;
    Tree *treePtr;
    Blt_ChainLink link;
    Blt_HashTable idleTable;
    Tcl_Interp *interp;
} TraceHandler;

typedef struct {
    TraceHandler *tracePtr;		/* Trace trace matched. */
    Tcl_Interp *interp;			/* Source interpreter. */
    Blt_TreeKey key;			/* Key that matched. */
    int flags;				/* Flags that matched. */
    long inode;				/* Node that matched. */
    Blt_HashEntry *hashPtr;
} TraceWhenIdle;

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_GetInterpData --
 *
 *	Creates or retrieves data associated with tree data objects for a
 *	particular thread.  We're using Tcl_GetAssocData rather than the Tcl
 *	thread routines so BLT can work with pre-8.0 TCL versions that don't
 *	have thread support.
 *
 * Results:
 *	Returns a pointer to the tree interpreter data.
 *
 *---------------------------------------------------------------------------
 */
static Blt_TreeInterpData *
Blt_Tree_GetInterpData(Tcl_Interp *interp)
{
    Tcl_InterpDeleteProc *proc;
    TreeInterpData *dataPtr;

    dataPtr = (TreeInterpData *)
	Tcl_GetAssocData(interp, TREE_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_AssertMalloc(sizeof(TreeInterpData));
	dataPtr->interp = interp;
	Tcl_SetAssocData(interp, TREE_THREAD_KEY, TreeInterpDeleteProc,
		 dataPtr);
	Blt_InitHashTable(&dataPtr->treeTable, BLT_STRING_KEYS);
	Blt_InitHashTable(&dataPtr->keyTable, BLT_STRING_KEYS);
    }
    return dataPtr;
}

const char *
Blt_Tree_NodeIdAscii(Node *nodePtr)
{
    static char stringRep[200];

    sprintf_s(stringRep, 200, "%ld", nodePtr->inode);
    return stringRep;
}

static Tree *
FirstClient(TreeObject *corePtr)
{
    Blt_ChainLink link;

    link = Blt_Chain_FirstLink(corePtr->clients); 
    if (link == NULL) {
	return NULL;
    }
    return Blt_Chain_GetValue(link);
}

static Tree *
NextClient(Tree *treePtr)
{
    Blt_ChainLink link;

    if (treePtr == NULL) {
	return NULL;
    }
    link = Blt_Chain_NextLink(treePtr->link); 
    if (link == NULL) {
	return NULL;
    }
    return Blt_Chain_GetValue(link);
}

/*
 *---------------------------------------------------------------------------
 *
 * NewNode --
 *
 *	Creates a new node in the tree without installing it.  The number of
 *	nodes in the tree is incremented and a unique serial number is
 *	generated for the node.
 *
 *	Also, all nodes have a label.  If no label was provided (name is NULL)
 *	then automatically generate one in the form "nodeN" where N is the
 *	serial number of the node.
 *
 * Results:
 *	Returns a pointer to the new node.
 *
 * -------------------------------------------------------------- 
 */
static Node *
NewNode(TreeObject *corePtr, const char *name, long inode)
{
    Node *np;

    /* Create the node structure */
    np = Blt_PoolAllocItem(corePtr->nodePool, sizeof(Node));
    np->inode = inode;
    np->corePtr = corePtr;
    np->parent = NULL;
    np->depth = 0;
    np->flags = 0;
    np->next = np->prev = NULL;
    np->first = np->last = NULL;
    np->nChildren = 0;
    np->values = NULL;     
    np->valueTable = NULL;     
    np->valueTableSize2 = 0;
    np->nValues = 0;
    np->nodeTable = NULL;
    np->nodeTableSize2 = 0;
    np->hnext = NULL;

    np->label = NULL;
    if (name != NULL) {
	np->label = Blt_Tree_GetKeyFromNode(np, name);
    }
    corePtr->nNodes++;
    return np;
}

/*
 *---------------------------------------------------------------------------
 *
 * ReleaseTagTable --
 *
 *---------------------------------------------------------------------------
 */
static void
ReleaseTagTable(Blt_TreeTagTable *tablePtr)
{
    tablePtr->refCount--;
    if (tablePtr->refCount <= 0) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;

	for(hPtr = Blt_FirstHashEntry(&tablePtr->tagTable, &cursor); 
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    Blt_TreeTagEntry *tePtr;

	    tePtr = Blt_GetHashValue(hPtr);
	    Blt_DeleteHashTable(&tePtr->nodeTable);
	    Blt_Free(tePtr);
	}
	Blt_DeleteHashTable(&tablePtr->tagTable);
	Blt_Free(tablePtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ResetDepths --
 *
 *	Called after moving a node, resets the depths of each node for the
 *	entire branch (node and it's decendants).
 *
 * Results: 
 *	None.
 *
 * ---------------------------------------------------------------------- 
 */
static void
ResetDepths(Node *branchPtr, long depth)
{
    Node *np;

    branchPtr->depth = depth;

    /* Also reset the depth for each descendant node. */
    for (np = branchPtr->first; np != NULL; np = np->next) {
	ResetDepths(np, depth + 1);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * RebuildNodeTable --
 *
 *	This procedure is invoked when the ratio of entries to hash buckets
 *	becomes too large.  It creates a new table with a larger bucket array
 *	and moves all of the entries into the new table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets reallocated and entries get re-hashed to new
 *	buckets.
 *
 *---------------------------------------------------------------------------
 */
static void
RebuildNodeTable(Node *parentPtr) /* Table to enlarge. */
{
    Node **bp, **bend;
    unsigned int downshift;
    unsigned long mask;
    Node **buckets;
    size_t nBuckets;

    nBuckets = (1 << parentPtr->nodeTableSize2);
    bend = parentPtr->nodeTable + nBuckets;

    /*
     * Allocate and initialize the new bucket array, and set up hashing
     * constants for new array size.
     */
    parentPtr->nodeTableSize2 += 2;
    nBuckets = (1 << parentPtr->nodeTableSize2);
    buckets = Blt_AssertCalloc(nBuckets, sizeof(Node *));
    /*
     * Move all of the existing entries into the new bucket array, based on
     * their new hash values.
     */
    mask = nBuckets - 1;
    downshift = DOWNSHIFT_START - parentPtr->nodeTableSize2;
    for (bp = parentPtr->nodeTable; bp < bend; bp++) {
	Node *np, *nextPtr;

	for (np = *bp; np != NULL; np = nextPtr) {
	    Node **bucketPtr;
    
	    nextPtr = np->hnext;
	    bucketPtr = buckets + RANDOM_INDEX(np->label);
	    np->hnext = *bucketPtr;
	    *bucketPtr = np;
	}
    }
    Blt_Free(parentPtr->nodeTable);
    parentPtr->nodeTable = buckets;
}

/*
 *---------------------------------------------------------------------------
 *
 * MakeNodeTable --
 *
 *	Generates a hash table from the nodes list of children.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Hash table for child nodes is created.
 *
 *---------------------------------------------------------------------------
 */
static void
MakeNodeTable(Node *parentPtr)
{
    Node **buckets;
    Node *np, *nextPtr;
    int downshift;
    unsigned int mask;
    unsigned int nBuckets;

    assert(parentPtr->nodeTable == NULL);
    parentPtr->nodeTableSize2 = START_LOGSIZE;
    nBuckets = 1 << parentPtr->nodeTableSize2;
    buckets = Blt_AssertCalloc(nBuckets, sizeof(Node *));
    mask = nBuckets - 1;
    downshift = DOWNSHIFT_START - parentPtr->nodeTableSize2;
    for (np = parentPtr->first; np != NULL; np = nextPtr) {
	Node **bucketPtr;

	nextPtr = np->next;
	bucketPtr = buckets + RANDOM_INDEX(np->label);
	np->hnext = *bucketPtr;
	*bucketPtr = np;
    }
    parentPtr->nodeTable = buckets;
}

/*
 *---------------------------------------------------------------------------
 *
 * LinkBefore --
 *
 *	Inserts a link preceding a given link.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
LinkBefore(
    Node *parentPtr,	/* Parent to hold the new entry. */
    Node *nodePtr,	/* New node to be inserted. */
    Node *beforePtr)	/* Node to link before. */
{
    if (parentPtr->first == NULL) {
	parentPtr->last = parentPtr->first = nodePtr;
    } else if (beforePtr == NULL) { /* Append onto the end of the chain */
	nodePtr->next = NULL;
	nodePtr->prev = parentPtr->last;
	parentPtr->last->next = nodePtr;
	parentPtr->last = nodePtr;
    } else {
	nodePtr->prev = beforePtr->prev;
	nodePtr->next = beforePtr;
	if (beforePtr == parentPtr->first) {
	    parentPtr->first = nodePtr;
	} else {
	    beforePtr->prev->next = nodePtr;
	}
	beforePtr->prev = nodePtr;
    }
    parentPtr->nChildren++;
    nodePtr->parent = parentPtr;

    /* 
     * Check if there as so many children that an addition hash table should
     * be created.
     */
    if (parentPtr->nodeTable == NULL) {
	if (parentPtr->nChildren > HASH_HIGH_WATER) {
	    MakeNodeTable(parentPtr);
	}
    } else {
	Node **bucketPtr;
	size_t nBuckets;
	unsigned int downshift;
	unsigned long mask;

	nBuckets = (1 << parentPtr->nodeTableSize2);
	mask = nBuckets - 1;
	downshift = DOWNSHIFT_START - parentPtr->nodeTableSize2;
	bucketPtr = parentPtr->nodeTable + RANDOM_INDEX(nodePtr->label);
	nodePtr->hnext = *bucketPtr;
	*bucketPtr = nodePtr;
	/*
	 * If the table has exceeded a decent size, rebuild it with many more
	 * buckets.
	 */
	if (parentPtr->nChildren >= (nBuckets * TREE_NODE_REBUILD_SIZE)) {
	    RebuildNodeTable(parentPtr);
	}
    } 
}


/*
 *---------------------------------------------------------------------------
 *
 * UnlinkNode --
 *
 *	Unlinks a link from the chain. The link is not deallocated, but only
 *	removed from the chain.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
UnlinkNode(Node *nodePtr)
{
    Node *parentPtr;
    int unlinked;		/* Indicates if the link is actually removed
				 * from the chain. */
    parentPtr = nodePtr->parent;
    unlinked = FALSE;
    if (parentPtr->first == nodePtr) {
	parentPtr->first = nodePtr->next;
	unlinked = TRUE;
    }
    if (parentPtr->last == nodePtr) {
	parentPtr->last = nodePtr->prev;
	unlinked = TRUE;
    }
    if (nodePtr->next != NULL) {
	nodePtr->next->prev = nodePtr->prev;
	unlinked = TRUE;
    }
    if (nodePtr->prev != NULL) {
	nodePtr->prev->next = nodePtr->next;
	unlinked = TRUE;
    }
    if (unlinked) {
	parentPtr->nChildren--;
    }
    nodePtr->prev = nodePtr->next = NULL;
    if (parentPtr->nodeTable != NULL) {
	Node **bucketPtr;
	unsigned int downshift;
	unsigned long mask;

	mask = (1 << parentPtr->nodeTableSize2) - 1;
	downshift = DOWNSHIFT_START - parentPtr->nodeTableSize2;
	bucketPtr = parentPtr->nodeTable + RANDOM_INDEX(nodePtr->label);
	if (*bucketPtr == nodePtr) {
	    *bucketPtr = nodePtr->hnext;
	} else {
	    Node *np;

	    for (np = *bucketPtr; /*empty*/; np = np->hnext) {
		if (np == NULL) {
		    return;	/* Can't find node in hash bucket. */
		}
		if (np->hnext == nodePtr) {
		    np->hnext = nodePtr->hnext;
		    break;
		}
	    }
	}
    } 
    nodePtr->hnext = NULL;
    if (parentPtr->nChildren < HASH_LOW_WATER) {
	Blt_Free(parentPtr->nodeTable);
	parentPtr->nodeTable = NULL;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * FreeNode --
 *
 *	Unlinks a given node from the tree, removes its data, and frees memory
 *	allocated to the node.
 *
 * Results:
 *	None.
 *
 * -------------------------------------------------------------- 
 */
static void
FreeNode(TreeObject *corePtr, Node *nodePtr)
{
    Blt_HashEntry *hPtr;

    /* Destroy any data fields associated with this node. */
    if (nodePtr->values != NULL) { 
	TreeDestroyValues(nodePtr);
    }
    if (nodePtr->nodeTable != NULL) {
	Blt_Free(nodePtr->nodeTable);
    } 
    UnlinkNode(nodePtr);
    corePtr->nNodes--;
    hPtr = Blt_FindHashEntry(&corePtr->nodeTable, (char *)nodePtr->inode);
    assert(hPtr);
    Blt_DeleteHashEntry(&corePtr->nodeTable, hPtr);
    Blt_PoolFreeItem(corePtr->nodePool, nodePtr);
}


/*
 *---------------------------------------------------------------------------
 *
 * TeardownTree --
 *
 *	Destroys an entire branch.  This is a special purpose routine used to
 *	speed up the final clean up of the tree.
 *
 * Results: 
 *	None.
 *
 * ---------------------------------------------------------------------- 
 */
static void
TeardownTree(TreeObject *corePtr, Node *branchPtr)
{
    Node *np, *nextPtr;
    
    if (branchPtr->nodeTable != NULL) {
	Blt_Free(branchPtr->nodeTable);
	branchPtr->nodeTable = NULL;
    } 
    if (branchPtr->values != NULL) {
	TreeDestroyValues(branchPtr);
    }
    for (np = branchPtr->first; np != NULL; np = nextPtr) {
	nextPtr = np->next;
	TeardownTree(corePtr, np);
    }
    Blt_PoolFreeItem(corePtr->nodePool, branchPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * DestroyTreeObject --
 *
 *	Destroys a tree object, freeing its nodes and memory.
 *
 * Results: 
 *	None.
 *
 * ---------------------------------------------------------------------- 
 */
static void
DestroyTreeObject(TreeObject *corePtr)
{
    corePtr->flags |= TREE_DESTROYED;
    corePtr->nNodes = 0;

    assert(Blt_Chain_GetLength(corePtr->clients) == 0);
    Blt_Chain_Destroy(corePtr->clients);

    TeardownTree(corePtr, corePtr->root);
    Blt_PoolDestroy(corePtr->nodePool);
    Blt_PoolDestroy(corePtr->valuePool);
    Blt_DeleteHashTable(&corePtr->nodeTable);
    Blt_Free(corePtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * ReleaseTreeObject --
 *
 *	Removes the client from the core tree object's list of clients.  If
 *	there are no more clients, then the tree object itself is destroyed.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The client is detached from the tree object.  The tree object is
 *	possibly destroyed, freeing memory.
 *
 *---------------------------------------------------------------------------
 */
static void
ReleaseTreeObject(Tree *treePtr) 
{
    if ((treePtr->link != NULL) && (treePtr->corePtr != NULL)) {
	/* Remove the client from the core's list */
	Blt_Chain_DeleteLink(treePtr->corePtr->clients, treePtr->link);
	if (Blt_Chain_GetLength(treePtr->corePtr->clients) == 0) {
	    DestroyTreeObject(treePtr->corePtr);
	}
	treePtr->corePtr = NULL;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * NewTreeObject --
 *
 *	Creates and initializes a new tree object. Trees always contain a root
 *	node, so one is allocated here.
 *
 * Results:
 *	Returns a pointer to the new tree object is successful, NULL
 *	otherwise.  If a tree can't be generated, interp->result will contain
 *	an error message.
 *
 *---------------------------------------------------------------------------
 */
static TreeObject *
NewTreeObject(TreeInterpData *dataPtr)
{
    TreeObject *corePtr;
    int isNew;
    Blt_HashEntry *hPtr;

    corePtr = Blt_Calloc(1, sizeof(TreeObject));
    if (corePtr == NULL) {
	return NULL;
    }
    corePtr->dataPtr = dataPtr;
    corePtr->valuePool = Blt_PoolCreate(BLT_FIXED_SIZE_ITEMS);
    corePtr->nodePool = Blt_PoolCreate(BLT_FIXED_SIZE_ITEMS);
    corePtr->clients = Blt_Chain_Create();
    corePtr->depth = 1;
    corePtr->notifyFlags = 0;
    Blt_InitHashTableWithPool(&corePtr->nodeTable, BLT_ONE_WORD_KEYS);
    hPtr = Blt_CreateHashEntry(&corePtr->nodeTable, (char *)0, &isNew);
    corePtr->root = NewNode(corePtr, "", 0);
    Blt_SetHashValue(hPtr, corePtr->root);
    return corePtr;
}

static Tree *
FindClientInNamespace(TreeInterpData *dataPtr, Blt_ObjectName *objNamePtr)
{
    Tcl_DString ds;
    const char *qualName;
    Blt_HashEntry *hPtr;

    qualName = Blt_MakeQualifiedName(objNamePtr, &ds);
    hPtr = Blt_FindHashEntry(&dataPtr->treeTable, qualName);
    Tcl_DStringFree(&ds);
    if (hPtr == NULL) {
	return NULL;
    }
    return Blt_GetHashValue(hPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * GetTree --
 *
 *	Searches for the tree object associated by the name given.
 *
 * Results:
 *	Returns a pointer to the tree if found, otherwise NULL.
 *
 *---------------------------------------------------------------------------
 */
static Tree *
GetTree(TreeInterpData *dataPtr, const char *name, int flags)
{
    Blt_ObjectName objName;
    Tree *treePtr;
    Tcl_Interp *interp;

    treePtr = NULL;
    interp = dataPtr->interp;
    if (!Blt_ParseObjectName(interp, name, &objName, BLT_NO_DEFAULT_NS)) {
	return NULL;
    }
    if (objName.nsPtr != NULL) { 
	treePtr = FindClientInNamespace(dataPtr, &objName);
    } else { 
	if (flags & NS_SEARCH_CURRENT) {
	    /* Look first in the current namespace. */
	    objName.nsPtr = Tcl_GetCurrentNamespace(interp);
	    treePtr = FindClientInNamespace(dataPtr, &objName);
	}
	if ((treePtr == NULL) && (flags & NS_SEARCH_GLOBAL)) {
	    objName.nsPtr = Tcl_GetGlobalNamespace(interp);
	    treePtr = FindClientInNamespace(dataPtr, &objName);
	}
    }
    return treePtr;
}

static void
ResetTree(Tree *treePtr) 
{
    Blt_ChainLink link;

    /* Remove any traces that may be set. */
    for (link = Blt_Chain_FirstLink(treePtr->traces); link != NULL;
	 link = Blt_Chain_NextLink(link)) {
	TraceHandler *tracePtr;
	
	tracePtr = Blt_Chain_GetValue(link);
	if (tracePtr->keyPattern != NULL) {
	    Blt_Free(tracePtr->keyPattern);
	}
	if (tracePtr->idleTable.numEntries > 0) {
	    Tcl_CancelIdleCall(TraceIdleProc, tracePtr);
	}
	Blt_Free(tracePtr);
    }
    Blt_Chain_Reset(treePtr->traces);

    /* And any event handlers. */
    for(link = Blt_Chain_FirstLink(treePtr->events); 
	link != NULL; link = Blt_Chain_NextLink(link)) {
	NotifyEventHandler *notifyPtr;

	notifyPtr = Blt_Chain_GetValue(link);
	if (notifyPtr->notifyPending) {
	    Tcl_CancelIdleCall(NotifyIdleProc, notifyPtr);
	}
	Blt_Free(notifyPtr);
    }
    Blt_Chain_Reset(treePtr->events);
}

static void
DestroyTree(Tree *treePtr) 
{
    TreeInterpData *dataPtr;

    dataPtr = treePtr->corePtr->dataPtr;
    if (treePtr->tagTablePtr != NULL) {
	ReleaseTagTable(treePtr->tagTablePtr);
    }
    ResetTree(treePtr);
    if (treePtr->hPtr != NULL) {
	Blt_DeleteHashEntry(&dataPtr->treeTable, treePtr->hPtr);
    }
    Blt_Chain_Destroy(treePtr->traces);
    Blt_Chain_Destroy(treePtr->events);
    treePtr->magic = 0;
    ReleaseTreeObject(treePtr);
    Blt_Free(treePtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeInterpDeleteProc --
 *
 *	This is called when the interpreter hosting the tree object is deleted
 *	from the interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys all remaining trees and removes the hash table used to
 *	register tree names.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
TreeInterpDeleteProc(ClientData clientData, Tcl_Interp *interp)
{
    TreeInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    
    for (hPtr = Blt_FirstHashEntry(&dataPtr->treeTable, &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	Tree *treePtr;

	treePtr = Blt_GetHashValue(hPtr);
	treePtr->hPtr = NULL;
	DestroyTree(treePtr);
    }
    Blt_DeleteHashTable(&dataPtr->treeTable);
    Blt_DeleteHashTable(&dataPtr->keyTable);
    Tcl_DeleteAssocData(interp, TREE_THREAD_KEY);
    Blt_Free(dataPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * NotifyIdleProc --
 *
 *	Used to invoke event handler routines at some idle point.  This
 *	routine is called from the TCL event loop.  Errors generated by the
 *	event handler routines are backgrounded.
 *	
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
NotifyIdleProc(ClientData clientData)
{
    NotifyEventHandler *notifyPtr = clientData;
    int result;

    notifyPtr->notifyPending = FALSE;
    notifyPtr->mask |= TREE_NOTIFY_ACTIVE;
    result = (*notifyPtr->proc)(notifyPtr->clientData, &notifyPtr->event);
    notifyPtr->mask &= ~TREE_NOTIFY_ACTIVE;
    if (result != TCL_OK) {
	Tcl_BackgroundError(notifyPtr->interp);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * CheckEventHandlers --
 *
 *	Traverses the list of client event callbacks and checks if one matches
 *	the given event.  A client may trigger an action that causes the tree
 *	to notify it.  The can be prevented by setting the
 *	TREE_NOTIFY_FOREIGN_ONLY bit in the event handler.
 *
 *	If a matching handler is found, a callback may be called either
 *	immediately or at the next idle time depending upon the
 *	TREE_NOTIFY_WHENIDLE bit.
 *
 *	Since a handler routine may trigger yet another call to itself,
 *	callbacks are ignored while the event handler is executing.
 *	
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
CheckEventHandlers(Tree *treePtr, int isSource, Blt_TreeNotifyEvent *eventPtr)
{
    Blt_ChainLink link, next;

    eventPtr->tree = treePtr;
    for (link = Blt_Chain_FirstLink(treePtr->events); link != NULL; 
	link = next) {
	NotifyEventHandler *notifyPtr;

	next = Blt_Chain_NextLink(link);
	notifyPtr = Blt_Chain_GetValue(link);
	if ((notifyPtr->mask & TREE_NOTIFY_ACTIVE) ||
	    (notifyPtr->mask & eventPtr->type) == 0) {
	    continue;			/* Ignore callbacks that are generated
					 * inside of a notify handler
					 * routine. */
	}
	if ((isSource) && (notifyPtr->mask & TREE_NOTIFY_FOREIGN_ONLY)) {
	    continue;			/* Don't notify yourself. */
	}
	if (notifyPtr->mask & TREE_NOTIFY_WHENIDLE) {
	    if (!notifyPtr->notifyPending) {
		notifyPtr->notifyPending = TRUE;
		notifyPtr->event = *eventPtr;
		Tcl_DoWhenIdle(NotifyIdleProc, notifyPtr);
	    }
	} else {
	    int result;

	    notifyPtr->mask |= TREE_NOTIFY_ACTIVE;
	    result = (*notifyPtr->proc) (notifyPtr->clientData, eventPtr);
	    notifyPtr->mask &= ~TREE_NOTIFY_ACTIVE;
	    if (result != TCL_OK) {
		Tcl_BackgroundError(notifyPtr->interp);
	    }
	}
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * NotifyClients --
 *
 *	Traverses the list of clients for a particular tree and notifies each
 *	client that an event occurred.  Clients indicate interest in a
 *	particular event through a bit flag.
 *
 *---------------------------------------------------------------------------
 */
static void
NotifyClients(Tree *sourcePtr, TreeObject *corePtr, Node *nodePtr, 
	      int eventFlag)
{
    Blt_TreeNotifyEvent event;
    Tree *treePtr;

    event.type = eventFlag;
    event.inode = nodePtr->inode;
    event.node = nodePtr;
    /* 
     * Issue callbacks to each client indicating that a new node has been
     * created.
     */
    for (treePtr = FirstClient(corePtr); treePtr != NULL; 
	 treePtr = NextClient(treePtr)) {
	int isSource;

	isSource = (treePtr == sourcePtr);
	CheckEventHandlers(treePtr, isSource, &event);
    }
}

static void
FreeValue(Node *nodePtr, Value *valuePtr)
{
    if (valuePtr->objPtr != NULL) {
	Tcl_DecrRefCount(valuePtr->objPtr);
    }
    Blt_PoolFreeItem(nodePtr->corePtr->valuePool, valuePtr);
}



#if (SIZEOF_VOID_P == 8)
/*
 *---------------------------------------------------------------------------
 *
 * HashOneWord --
 *
 *	Compute a one-word hash value of a 64-bit word, which then can be used
 *	to generate a hash index.
 *
 *	From Knuth, it's a multiplicative hash.  Multiplies an unsigned 64-bit
 *	value with the golden ratio (sqrt(5) - 1) / 2.  The downshift value is
 *	64 - n, when n is the log2 of the size of the hash table.
 *		
 * Results:
 *	The return value is a one-word summary of the information in 64 bit
 *	word.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static Blt_Hash
HashOneWord(uint64_t mask, unsigned int downshift, const void *key)
{
    uint64_t a0, a1;
    uint64_t y0, y1;
    uint64_t y2, y3; 
    uint64_t p1, p2;
    uint64_t result;

    /* Compute key * GOLDEN_RATIO in 128-bit arithmetic */
    a0 = (uint64_t)key & 0x00000000FFFFFFFF; 
    a1 = (uint64_t)key >> 32;
    
    y0 = a0 * 0x000000007f4a7c13;
    y1 = a0 * 0x000000009e3779b9; 
    y2 = a1 * 0x000000007f4a7c13;
    y3 = a1 * 0x000000009e3779b9; 
    y1 += y0 >> 32;		/* Can't carry */ 
    y1 += y2;			/* Might carry */
    if (y1 < y2) {
	y3 += (1LL << 32);	/* Propagate */ 
    }

    /* 128-bit product: p1 = loword, p2 = hiword */
    p1 = ((y1 & 0x00000000FFFFFFFF) << 32) + (y0 & 0x00000000FFFFFFFF);
    p2 = y3 + (y1 >> 32);
    
    /* Left shift the value downward by the size of the table */
    if (downshift > 0) { 
	if (downshift < 64) { 
	    result = ((p2 << (64 - downshift)) | (p1 >> (downshift & 63))); 
	} else { 
	    result = p2 >> (downshift & 63); 
	} 
    } else { 
	result = p1;
    } 
    /* Finally mask off the high bits */
    return (Blt_Hash)(result & mask);
}

#endif /* SIZEOF_VOID_P == 8 */

/*
 *---------------------------------------------------------------------------
 *
 * RebuildTable --
 *
 *	This procedure is invoked when the ratio of entries to hash buckets
 *	becomes too large.  It creates a new table with a larger bucket array
 *	and moves all of the entries into the new table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets reallocated and entries get re-hashed to new buckets.
 *
 *---------------------------------------------------------------------------
 */
static void
RebuildValueTable(Node *nodePtr)		/* Table to enlarge. */
{
    Value **bp, **bend, **buckets, **oldBuckets;
    size_t nBuckets;
    unsigned int downshift;
    unsigned long mask;

    oldBuckets = nodePtr->valueTable;
    nBuckets = (1 << nodePtr->valueTableSize2);
    bend = oldBuckets + nBuckets;

    /*
     * Allocate and initialize the new bucket array, and set up hashing
     * constants for new array size.
     */
    nodePtr->valueTableSize2 += 2;
    nBuckets = (1 << nodePtr->valueTableSize2);
    buckets = Blt_AssertCalloc(nBuckets, sizeof(Value *));

    /*
     * Move all of the existing entries into the new bucket array, based on
     * their hash values.
     */
    mask = nBuckets - 1;
    downshift = DOWNSHIFT_START - nodePtr->valueTableSize2;
    for (bp = oldBuckets; bp < bend; bp++) {
	Value *vp, *nextPtr;

	for (vp = *bp; vp != NULL; vp = nextPtr) {
	    Value **bucketPtr;

	    nextPtr = vp->hnext;
	    bucketPtr = buckets + RANDOM_INDEX(vp->key);
	    vp->hnext = *bucketPtr;
	    *bucketPtr = vp;
	}
    }
    nodePtr->valueTable = buckets;
    Blt_Free(oldBuckets);
}

static void
MakeValueTable(Node *nodePtr)
{
    unsigned int nBuckets;
    Value **buckets;
    unsigned int mask;
    int downshift;
    Value *vp, *nextPtr;

    assert(nodePtr->valueTable == NULL);

    /* Generate hash table from list of values. */
    nodePtr->valueTableSize2 = START_LOGSIZE;
    nBuckets = 1 << nodePtr->valueTableSize2;
    buckets = Blt_AssertCalloc(nBuckets, sizeof(Value *));
    mask = nBuckets - 1;
    downshift = DOWNSHIFT_START - nodePtr->valueTableSize2;
    for (vp = nodePtr->values; vp != NULL; vp = nextPtr) {
	Value **bucketPtr;

	nextPtr = vp->next;
	bucketPtr = buckets + RANDOM_INDEX(vp->key);
	vp->hnext = *bucketPtr;
	*bucketPtr = vp;
    }
    nodePtr->valueTable = buckets;
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeDeleteValue --
 *
 *	Remove a single entry from a hash table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The entry given by entryPtr is deleted from its table and should never
 *	again be used by the caller.  It is up to the caller to free the
 *	clientData field of the entry, if that is relevant.
 *
 *---------------------------------------------------------------------------
 */
static int
TreeDeleteValue(Node *nodePtr, Blt_TreeValue value)
{
    Value *vp, *prevPtr;
    
    if (nodePtr->valueTable != NULL) {
	Value **bucketPtr;
	unsigned int downshift;
	unsigned long mask;

	mask = (1 << nodePtr->valueTableSize2) - 1;
	downshift = DOWNSHIFT_START - nodePtr->valueTableSize2;
	bucketPtr = nodePtr->valueTable + RANDOM_INDEX(((Value *)value)->key);
	if (*bucketPtr == value) {
	    *bucketPtr = ((Value *)value)->hnext;
	} else {
	    Value *pp;

	    for (pp = *bucketPtr; /*empty*/; pp = pp->hnext) {
		if (pp == NULL) {
		    return TCL_ERROR; /* Can't find value in hash bucket. */
		}
		if (pp->hnext == value) {
		    pp->hnext = ((Value *)value)->hnext;
		    break;
		}
	    }
	}
    } 
    prevPtr = NULL;
    for (vp = nodePtr->values; vp != NULL; vp = vp->next) {
	if (vp == value) {
	    break;
	}
	prevPtr = vp;
    }
    if (vp == NULL) {
	return TCL_ERROR;	/* Can't find value in list. */
    }
    if (prevPtr == NULL) {
	nodePtr->values = vp->next;
    } else {
	prevPtr->next = vp->next;
    }
    nodePtr->nValues--;
    FreeValue(nodePtr, value);
    if (nodePtr->nValues < HASH_LOW_WATER) {
	Blt_Free(nodePtr->valueTable);
	nodePtr->valueTable = NULL;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeDestroyValues --
 *
 *	Free up everything associated with a hash table except for the record
 *	for the table itself.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The hash table is no longer useable.
 *
 *---------------------------------------------------------------------------
 */
static void
TreeDestroyValues(Node *nodePtr)
{
    Value *vp;
    Value *nextPtr;

    /* Free value hash table. */
    if (nodePtr->valueTable != NULL) {
	Blt_Free(nodePtr->valueTable);
    } 

    /* Free all the entries in the value list. */
    for (vp = nodePtr->values; vp != NULL; vp = nextPtr) {
	nextPtr = vp->next;
	FreeValue(nodePtr, vp);
    }
    nodePtr->values = NULL;
    nodePtr->valueTable = NULL;
    nodePtr->nValues = 0;
    nodePtr->valueTableSize2 = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeFirstValue --
 *
 *	Locate the first entry in a hash table and set up a record that can be
 *	used to step through all the remaining entries of the table.
 *
 * Results:
 *	The return value is a pointer to the first value in tablePtr, or NULL
 *	if tablePtr has no entries in it.  The memory at *searchPtr is
 *	initialized so that subsequent calls to Blt_Tree_NextValue will return
 *	all of the values in the table, one at a time.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static Value *
TreeFirstValue(
    Node *nodePtr,
    Blt_TreeKeyIterator *iterPtr) /* Place to store information about progress
				   * through the table. */
{
    iterPtr->node = nodePtr;
    iterPtr->nextIndex = 0;
    iterPtr->nextValue = nodePtr->values;
    return TreeNextValue(iterPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeNextValue --
 *
 *	Once a hash table enumeration has been initiated by calling
 *	Blt_Tree_FirstValue, this procedure may be called to return successive
 *	elements of the table.
 *
 * Results:
 *	The return value is the next entry in the hash table being enumerated,
 *	or NULL if the end of the table is reached.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static Value *
TreeNextValue(
    Blt_TreeKeyIterator *iterPtr) /* Place to store information about progress
				   * through the table.  Must have been
				   * initialized by calling
				   * Blt_Tree_FirstValue. */
{
    Value *valuePtr;

    valuePtr = iterPtr->nextValue;
    if (valuePtr != NULL) {
	iterPtr->nextValue = valuePtr->next;
    }
    return valuePtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeFindValue --
 *
 *	Given a hash table with one-word keys, and a one-word key, find the
 *	entry with a matching key.
 *
 * Results:
 *	The return value is a token for the matching entry in the hash table,
 *	or NULL if there was no matching entry.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static Value *
TreeFindValue(
    Node *nodePtr,
    Blt_TreeKey key)		/* Key to use to find matching entry. */
{
    Value *vp;

    if (nodePtr->valueTable != NULL) {
	unsigned int downshift;
	unsigned long mask;
	Value *bucket;

	mask = (1 << nodePtr->valueTableSize2) - 1;
	downshift = DOWNSHIFT_START - nodePtr->valueTableSize2;
	bucket = nodePtr->valueTable[RANDOM_INDEX(key)];

	/* Search all of the entries in the appropriate bucket. */
	for (vp = bucket; (vp != NULL) && (vp->key != key); vp = vp->hnext) {
	    /* empty */;
	}
    } else {
	for (vp = nodePtr->values; (vp != NULL) && (vp->key != key); 
	     vp = vp->next) {
	    /* empty */;
	}
    }
    return vp;
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeCreateValue --
 *
 *	Find the value with a matching key.  If there is no matching value,
 *	then create a new one.
 *
 * Results:
 *	The return value is a pointer to the matching value.  If this is a
 *	newly-created value, then *newPtr will be set to a non-zero value;
 *	otherwise *newPtr will be set to 0.
 *
 * Side effects:
 *	A new value may be added to the hash table.
 *
 *---------------------------------------------------------------------------
 */
static Value *
TreeCreateValue(
    Node *nodePtr,
    Blt_TreeKey key,		/* Key to use to find or create matching
				 * entry. */
    int *isNewPtr)		/* (out) If non-zero, indicates a new hash
				 * entry was created. */
{
    Value *vp, *prevPtr;
    
    prevPtr = NULL;
    *isNewPtr = FALSE;
    for (vp = nodePtr->values; vp != NULL; vp = vp->next) {
	if (vp->key == key) {
	    return vp;
	}
	prevPtr = vp;
    }
    /* Value not found. Add a new value to the list. */
    *isNewPtr = TRUE;
    vp = Blt_PoolAllocItem(nodePtr->corePtr->valuePool, sizeof(Value));
    vp->key = key;
    vp->owner = NULL;
    vp->next = NULL;
    vp->hnext = NULL;
    vp->objPtr = NULL;
    if (prevPtr == NULL) {
	nodePtr->values = vp;
    } else {
	prevPtr->next = vp;
    }
    nodePtr->nValues++;

    if (nodePtr->valueTable == NULL) {
	/* 
	 * If we reach a threshold number of values, create a hash table of
	 * values.
	 */
	if (nodePtr->nValues > HASH_HIGH_WATER) {
	    MakeValueTable(nodePtr);
	}
    } else {
	Value **bucketPtr;
	size_t nBuckets;
	unsigned int downshift;
	unsigned long mask;

	nBuckets = (1 << nodePtr->valueTableSize2);
	mask = nBuckets - 1;
	downshift = DOWNSHIFT_START - nodePtr->valueTableSize2;
	bucketPtr = nodePtr->valueTable + RANDOM_INDEX((void *)key);
	vp->hnext = *bucketPtr;
	*bucketPtr = vp;
	/*
	 * If the table has exceeded a decent size, rebuild it with many more
	 * buckets.
	 */
	if ((unsigned int)nodePtr->nValues >= (nBuckets * 3)) {
	    RebuildValueTable(nodePtr);
	}
    } 
    return vp;
}


/*
 *---------------------------------------------------------------------------
 *
 * ParseDumpRecord --
 *
 *	Gets the next full record in the dump string, returning the record as
 *	a list. Blank lines and comments are ignored.
 *
 * Results: 
 *	TCL_RETURN	The end of the string is reached.
 *	TCL_ERROR	An error occurred and an error message 
 *			is left in the interpreter result.  
 *	TCL_OK		The next record has been successfully parsed.
 *
 *---------------------------------------------------------------------------
 */
static int
ParseDumpRecord(
    Tcl_Interp *interp,
    const char **stringPtr,	/* (in/out) points to current location in in
				 * dump string. Updated after parsing
				 * record. */
    int *argcPtr,		/* (out) Will contain the length of the
				 * record's list. */
    const char ***argvPtr,	/* (out) Will contain the list representing
				 * the dump record of the node. */
    RestoreInfo *restorePtr)
{
    char *entry, *eol;
    char saved;
    int result;

    entry = (char *)*stringPtr;
    /* Get first line, ignoring blank lines and comments. */
    for (;;) {
	char *first;

	first = NULL;
	restorePtr->nLines++;
	/* Find the end of the first line. */
	for (eol = entry; (*eol != '\n') && (*eol != '\0'); eol++) {
	    if ((first == NULL) && (!isspace(UCHAR(*eol)))) {
		first = eol;	/* Track first non-whitespace character. */
	    }
	}
	if (first == NULL) {
	    if (*eol == '\0') {
		return TCL_RETURN;
	    }
	} else if (*first != '#') {
	    break;		/* Not a comment or blank line. */
	}
	entry = eol + 1;
    }
    saved = *eol;
    *eol = '\0';
    while (!Tcl_CommandComplete(entry)) {
	*eol = saved;
	if (*eol == '\0') {
	    Tcl_AppendResult(interp, "incomplete dump record: \"", entry, 
		"\"", (char *)NULL);
	    return TCL_ERROR;	/* Found EOF (incomplete entry) or error. */
	}
	/* Get the next line. */
	for (eol = eol + 1; (*eol != '\n') && (*eol != '\0'); eol++) {
	    /*empty*/
	}
	restorePtr->nLines++;
	saved = *eol;
	*eol = '\0';
    }
    if (entry == eol) {
	return TCL_RETURN;
    }
    result = Tcl_SplitList(interp, entry, argcPtr, argvPtr);
    *eol = saved;
    *stringPtr = eol + 1;
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * ReadDumpRecord --
 *
 *	Reads the next full record from the given channel, returning the
 *	record as a list. Blank lines and comments are ignored.
 *
 * Results: 
 *	TCL_RETURN	The end of the file has been reached.
 *	TCL_ERROR	A read error has occurred and an error message 
 *			is left in the interpreter result.  
 *	TCL_OK		The next record has been successfully parsed.
 *
 *---------------------------------------------------------------------------
 */
static int
ReadDumpRecord(
    Tcl_Interp *interp,
    Tcl_Channel channel,	/* Channel from which to read the next
				 * record. */
    int *argcPtr,		/* (out) Will contain the length of the
				 * record's list. */
    const char ***argvPtr,	/* (out) Will contain the list representing
				 * the dump record of the node. */
    RestoreInfo *restorePtr)
{
    int result;
    Tcl_DString ds;

    Tcl_DStringInit(&ds);
    /* Get first line, ignoring blank lines and comments. */
    for (;;) {
	char *cp;
	int nBytes;

	Tcl_DStringSetLength(&ds, 0);
	nBytes = Tcl_Gets(channel, &ds);
	if (nBytes < 0) {
	    if (Tcl_Eof(channel)) {
		return TCL_RETURN;
	    }
	    return TCL_ERROR;
	}
	restorePtr->nLines++;
	for (cp = Tcl_DStringValue(&ds); *cp != '\0'; cp++) {
	    if (!isspace(UCHAR(*cp))) {
		break;
	    }
	}
	if ((*cp != '\0') && (*cp != '#')) {
	    break;		/* Not a comment or blank line. */
	}
    }

    Tcl_DStringAppend(&ds, "\n", 1);
    while (!Tcl_CommandComplete(Tcl_DStringValue(&ds))) {
	int nBytes;

	if (Tcl_Eof(channel)) {
	    Tcl_AppendResult(interp, "unexpected EOF: short record.", 
			     (char *)NULL);
	    Tcl_DStringFree(&ds);
	    return TCL_ERROR;	/* Found EOF (incomplete entry) or error. */
	}
	/* Process additional lines if needed */
	nBytes = Tcl_Gets(channel, &ds);
	if (nBytes < 0) {
	    Tcl_AppendResult(interp, "read error: ", 
			     Tcl_PosixError(interp), (char *)NULL);
	    Tcl_DStringFree(&ds);
	    return TCL_ERROR;	/* Found EOF (incomplete entry) or error. */
	}
	restorePtr->nLines++;
	Tcl_DStringAppend(&ds, "\n", 1);
    }
    result = Tcl_SplitList(interp, Tcl_DStringValue(&ds), argcPtr, argvPtr);
    Tcl_DStringFree(&ds);
    return result;
}

static Tcl_Obj *
GetStringObj(RestoreInfo *restorePtr, const char *string, int length)
{
    Blt_HashEntry *hPtr;
    int isNew;

    hPtr = Blt_CreateHashEntry(&restorePtr->dataTable, string, &isNew);
    if (isNew) {
	Tcl_Obj *objPtr;

	if (length == -1) {
	    length = strlen(string);
	}
	objPtr = Tcl_NewStringObj(string, length);
	Blt_SetHashValue(hPtr, objPtr);
	return objPtr;
    }
    return Blt_GetHashValue(hPtr);
}

static int
RestoreValues(
    RestoreInfo *restorePtr,
    Tcl_Interp *interp, 
    Node *nodePtr, 
    int nValues, 
    const char **values)
{
    int i;

    for (i = 0; i < nValues; i += 2) {
	Tcl_Obj *valueObjPtr;
	int result;

	if ((i + 1) < nValues) {
	    valueObjPtr = GetStringObj(restorePtr, values[i + 1], -1);
	} else {
	    valueObjPtr = Tcl_NewStringObj("", -1);
	}
	Tcl_IncrRefCount(valueObjPtr);
	result = Blt_Tree_SetValue(interp, restorePtr->treePtr, nodePtr, 
		values[i], valueObjPtr);
	Tcl_DecrRefCount(valueObjPtr);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

static int 
RestoreTags(
    Tree *treePtr, 
    Node *nodePtr, 
    int nTags, 
    const char **tags) 
{
    int i;

    for (i = 0; i < nTags; i++) {
	Blt_Tree_AddTag(treePtr, nodePtr, tags[i]);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * RestoreNode5 --
 *
 *	Parses and creates a node based upon the first 3 fields of a five
 *	field entry.  This is the new restore file format.
 *
 *	   parentId nodeId pathList dataList tagList ?attrList?
 *
 *	The purpose is to attempt to save and restore the node ids embedded in
 *	the restore file information.  The old format could not distinquish
 *	between two sibling nodes with the same label unless they were both
 *	leaves.  I'm trying to avoid dependencies upon labels.
 *
 *	If you're starting from an empty tree, this obviously should work
 *	without a hitch.  We only need to map the file's root id to 0.  It's a
 *	little more complicated when adding node to an already full tree.
 *
 *	First see if the node id isn't already in use.  Otherwise, map the
 *	node id (via a hashtable) to the real node. We'll need it later when
 *	subsequent entries refer to their parent id.
 *
 *	If a parent id is unknown (the restore file may be out of order), then
 *	follow plan B and use its path.
 *	
 *---------------------------------------------------------------------------
 */
static int
RestoreNode5(
    Tcl_Interp *interp,
    int argc, 			/* Not used. */
    const char **argv, 
    RestoreInfo *restorePtr)
{
    Tree *treePtr;
    Blt_HashEntry *hPtr;
    Node *nodePtr, *parentPtr;
    int isNew;
    long pid, id;
    const char **attrs, **tags, **values, **names;
    int nTags, nValues, nNames;

    treePtr = restorePtr->treePtr;

    /* 
     * The second and first fields respectively are the ids of the node and
     * its parent.  The parent id of the root node is always -1.
     */

    if ((Blt_GetLong(interp, argv[0], &pid) != TCL_OK) ||
	(Blt_GetLong(interp, argv[1], &id) != TCL_OK)) {
	return TCL_ERROR;
    }
    names = values = tags = attrs = NULL;
    nodePtr = NULL;

    /* 
     * The third, fourth, and fifth fields respectively are the list of
     * component names representing the path to the node including the name of
     * the node, a key-value list of data values, and a list of tag names.
     */     

    if ((Tcl_SplitList(interp, argv[2], &nNames, &names) != TCL_OK) ||
	(Tcl_SplitList(interp, argv[3], &nValues, &values) != TCL_OK)  || 
	(Tcl_SplitList(interp, argv[4], &nTags, &tags) != TCL_OK)) {
	goto error;
    }    

    /* Get the parent of the node. */

    if (pid == -1) {	/* Map -1 id to the root node of the subtree. */
	nodePtr = restorePtr->rootPtr;
	hPtr = Blt_CreateHashEntry(&restorePtr->idTable, (char *)id, &isNew);
	Blt_SetHashValue(hPtr, nodePtr);
	Blt_Tree_RelabelNode(treePtr, nodePtr, names[0]);
    } else {

	/* 
	 * Check if the parent has been mapped to another id in the tree.
	 * This can happen when there's a id collision with an existing node.
	 */

	hPtr = Blt_FindHashEntry(&restorePtr->idTable, (char *)pid);
	if (hPtr != NULL) {
	    parentPtr = Blt_GetHashValue(hPtr);
	} else {
	    parentPtr = Blt_Tree_GetNode(treePtr, pid);
	    if (parentPtr == NULL) {
		/* 
		 * Normally the parent node should already exist in the tree,
		 * but in a partial restore it might not.  "Plan B" is to use
		 * the list of path components to create the missing
		 * components, including the parent.
		 */
		if (nNames == 0) {
		    parentPtr = restorePtr->rootPtr;
		} else {
		    int i;

		    for (i = 1; i < (nNames - 2); i++) {
			nodePtr = Blt_Tree_FindChild(parentPtr, names[i]);
			if (nodePtr == NULL) {
			    nodePtr = Blt_Tree_CreateNode(treePtr, parentPtr, 
				names[i], -1);
			}
			parentPtr = nodePtr;
		    }
		    /* 
		     * If there's a node with the same label as the parent,
		     * we'll use that node. Otherwise, try to create a new
		     * node with the desired parent id.
		     */
		    nodePtr = Blt_Tree_FindChild(parentPtr, names[nNames - 2]);
		    if (nodePtr == NULL) {
			nodePtr = Blt_Tree_CreateNodeWithId(treePtr, parentPtr,
				names[nNames - 2], pid, -1);
			if (nodePtr == NULL) {
			    goto error;
			}
		    }
		    parentPtr = nodePtr;
		}
	    }
	} 

	/* 
	 * It's an error if the desired id has already been remapped.  That
	 * means there were two nodes in the dump with the same id.
	 */
	hPtr = Blt_FindHashEntry(&restorePtr->idTable, (char *)id);
 	if (hPtr != NULL) {
	    Tcl_AppendResult(interp, "node \"", Blt_Ltoa(id), 
		"\" has already been restored", (char *)NULL);
	    goto error;
	}


	if (restorePtr->flags & TREE_RESTORE_OVERWRITE) {
	    /* Can you find the child by name. */
	    nodePtr = Blt_Tree_FindChild(parentPtr, names[nNames - 1]);
	    if (nodePtr != NULL) {
		hPtr = Blt_CreateHashEntry(&restorePtr->idTable, (char *)id,
					   &isNew);
		Blt_SetHashValue(hPtr, nodePtr);
	    }
	}

	if (nodePtr == NULL) {
	    nodePtr = Blt_Tree_GetNode(treePtr, id);
	    if (nodePtr == NULL) {
		nodePtr = Blt_Tree_CreateNodeWithId(treePtr, parentPtr, 
			names[nNames - 1], id, -1);
	    } else {
		nodePtr = Blt_Tree_CreateNode(treePtr, parentPtr, 
			names[nNames - 1], -1);
		hPtr = Blt_CreateHashEntry(&restorePtr->idTable, (char *)id,
			&isNew);
		Blt_SetHashValue(hPtr, nodePtr);
	    }
	}
    } 
	
    if (nodePtr == NULL) {
	goto error;		/* Couldn't create node with requested id. */
    }
    Blt_Free(names);
    names = NULL;

    /* Values */
    if (RestoreValues(restorePtr, interp, nodePtr, nValues, values) != TCL_OK) {
	goto error;
    }
    Blt_Free(values);
    values = NULL;

    /* Tags */
    if (!(restorePtr->flags & TREE_RESTORE_NO_TAGS)) {
	RestoreTags(treePtr, nodePtr, nTags, tags);
    }
    Blt_Free(tags);
    tags = NULL;
    return TCL_OK;

 error:
    if (attrs != NULL) {
	Blt_Free(attrs);
    }
    if (tags != NULL) {
	Blt_Free(tags);
    }
    if (values != NULL) {
	Blt_Free(values);
    }
    if (names != NULL) {
	Blt_Free(names);
    }
    if (nodePtr != NULL) {
	Blt_Tree_DeleteNode(treePtr, nodePtr);
    }
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * RestoreNode3 --
 *
 *	Parses and creates a node based upon the first field of a three field
 *	entry.  This is the old restore file format.
 *
 *		pathList dataList tagList
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
RestoreNode3(
    Tcl_Interp *interp, 
    int argc,			/* Not used. */
    const char **argv, 
    RestoreInfo *restorePtr)
{
    Tree *treePtr;
    Node *nodePtr, *parentPtr;
    int i;
    const char **names, **values, **tags;
    int nNames, nValues, nTags;

    /* The first field is a list of component names representing the path to
     * the node, including the name of the node. */

    if (Tcl_SplitList(interp, argv[0], &nNames, &names) != TCL_OK) {
	return TCL_ERROR;
    }
    nodePtr = parentPtr = restorePtr->rootPtr;
    treePtr = restorePtr->treePtr;
    /* Automatically create ancestor nodes as needed. */
    for (i = 0; i < (nNames - 1); i++) {
	nodePtr = Blt_Tree_FindChild(parentPtr, names[i]);
	if (nodePtr == NULL) {
	    nodePtr = Blt_Tree_CreateNode(treePtr, parentPtr, names[i], -1);
	}
	parentPtr = nodePtr;
    }
    if (nNames > 0) {

	/* 
	 * By default duplicate nodes (two sibling nodes with the same label)
	 * unless the -overwrite switch was given.
	 */

	nodePtr = NULL;
	if (restorePtr->flags & TREE_RESTORE_OVERWRITE) {
	    nodePtr = Blt_Tree_FindChild(parentPtr, names[i]);
	}
	if (nodePtr == NULL) {
	    nodePtr = Blt_Tree_CreateNode(treePtr, parentPtr, names[i], -1);
	}
    }
    Blt_Free(names);

    /* The second field is a key-value list of the node's values. */

    if (Tcl_SplitList(interp, argv[1], &nValues, &values) != TCL_OK) {
	return TCL_ERROR;
    }
    if (RestoreValues(restorePtr, interp, nodePtr, nValues, values) != TCL_OK) {
	goto error;
    }
    Blt_Free(values);

    /* The third field is a list of tags. */

    if (!(restorePtr->flags & TREE_RESTORE_NO_TAGS)) {
	/* Parse the tag list. */
	if (Tcl_SplitList(interp, argv[2], &nTags, &tags) != TCL_OK) {
	    goto error;
	}
	RestoreTags(treePtr, nodePtr, nTags, tags);
	Blt_Free(tags);
    }
    return TCL_OK;

 error:
    Blt_Free(argv);
    Blt_Tree_DeleteNode(treePtr, nodePtr);
    return TCL_ERROR;
}

/* Public Routines */

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_GetKey --
 *
 *	Given a string, returns a unique identifier for the string.
 *
 *---------------------------------------------------------------------------
 */
Blt_TreeKey
Blt_Tree_GetKey(Tree *treePtr, const char *string) /* String to convert. */
{
    Blt_HashEntry *hPtr;
    int isNew;
    Blt_HashTable *tablePtr;

    tablePtr = &treePtr->corePtr->dataPtr->keyTable;
    hPtr = Blt_CreateHashEntry(tablePtr, string, &isNew);
    return (Blt_TreeKey)Blt_GetHashKey(tablePtr, hPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_GetKeyFromNode --
 *
 *	Given a string, returns a unique identifier for the string.
 *
 *---------------------------------------------------------------------------
 */
Blt_TreeKey
Blt_Tree_GetKeyFromNode(Node *nodePtr, const char *string)
{
    Blt_HashEntry *hPtr;
    int isNew;
    Blt_HashTable *tablePtr;

    tablePtr = &nodePtr->corePtr->dataPtr->keyTable;
    hPtr = Blt_CreateHashEntry(tablePtr, string, &isNew);
    return (Blt_TreeKey)Blt_GetHashKey(tablePtr, hPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_GetKeyFromInterp --
 *
 *	Given a string, returns a unique identifier for the string.
 *
 *---------------------------------------------------------------------------
 */
Blt_TreeKey
Blt_Tree_GetKeyFromInterp(Tcl_Interp *interp, const char *string)
{
    Blt_HashEntry *hPtr;
    TreeInterpData *dataPtr;
    int isNew;

    dataPtr = Blt_Tree_GetInterpData(interp);
    hPtr = Blt_CreateHashEntry(&dataPtr->keyTable, string, &isNew);
    return (Blt_TreeKey)Blt_GetHashKey(&dataPtr->keyTable, hPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_CreateNode --
 *
 *	Creates a new node in the given parent node.  The name and position in
 *	the parent are also provided.
 *
 *---------------------------------------------------------------------------
 */
Blt_TreeNode
Blt_Tree_CreateNode(
    Tree *treePtr,		/* The tree client that is creating this node.
				 * If NULL, indicates to trigger notify events
				 * on behalf of the initiating client also. */
    Node *parentPtr,		/* Parent node where the new node will be
				 * inserted. */
    const char *name,		/* Name of node. */
    long position)		/* Position in the parent's list of children
				 * where to insert the new node. */
{
    Blt_HashEntry *hPtr;
    Node *beforePtr;
    Node *nodePtr;	/* Node to be inserted. */
    TreeObject *corePtr;
    long inode;
    int isNew;

    corePtr = parentPtr->corePtr;

    /* Generate an unique serial number for this node.  */
    do {
	inode = corePtr->nextInode++;
	hPtr = Blt_CreateHashEntry(&corePtr->nodeTable,(char *)inode, 
		   &isNew);
    } while (!isNew);
    nodePtr = NewNode(corePtr, name, inode);
    Blt_SetHashValue(hPtr, nodePtr);

    if ((position == -1) || ((size_t)position >= parentPtr->nChildren)) {
	beforePtr = NULL;
    } else {
	beforePtr = parentPtr->first;
	while ((position > 0) && (beforePtr != NULL)) {
	    position--;
	    beforePtr = beforePtr->next;
	}
    }
    LinkBefore(parentPtr, nodePtr, beforePtr);
    nodePtr->depth = parentPtr->depth + 1;
    /* 
     * Issue callbacks to each client indicating that a new node has been
     * created.
     */
    NotifyClients(treePtr, corePtr, nodePtr, TREE_NOTIFY_CREATE);
    return nodePtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_CreateNodeWithId --
 *
 *	Like Blt_Tree_CreateNode, but provides a specific id to use for the
 *	node.  If the tree already contains a node by that id, NULL is
 *	returned.
 *
 *---------------------------------------------------------------------------
 */
Blt_TreeNode
Blt_Tree_CreateNodeWithId(
    Tree *treePtr,
    Node *parentPtr,		/* Parent node where the new node will be
				 * inserted. */
    const char *name,		/* Name of node. */
    long inode,			/* Requested id of the new node. If a node by
				 * this id already exists in the tree, no node
				 * is created. */
    long position)		/* Position in the parent's list of children
				 * where to insert the new node. */
{
    Blt_HashEntry *hPtr;
    Node *beforePtr;
    Node *nodePtr;	/* Node to be inserted. */
    TreeObject *corePtr;
    int isNew;

    corePtr = parentPtr->corePtr;
    hPtr = Blt_CreateHashEntry(&corePtr->nodeTable, (char *)inode, &isNew);
    if (!isNew) {
	return NULL;
    }
    nodePtr = NewNode(corePtr, name, inode);
    Blt_SetHashValue(hPtr, nodePtr);

    if ((position == -1) || ((size_t)position >= parentPtr->nChildren)) {
	beforePtr = NULL;
    } else {
	beforePtr = parentPtr->first;
	while ((position > 0) && (beforePtr != NULL)) {
	    position--;
	    beforePtr = beforePtr->next;
	}
    }
    LinkBefore(parentPtr, nodePtr, beforePtr);
    nodePtr->depth = parentPtr->depth + 1;
    /* 
     * Issue callbacks to each client indicating that a new node has been
     * created.
     */
    NotifyClients(treePtr, corePtr, nodePtr, TREE_NOTIFY_CREATE);
    return nodePtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_MoveNode --
 *
 *	Move an entry into a new location in the hierarchy.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_Tree_MoveNode(Tree *treePtr, Node *nodePtr, Node *parentPtr, 
		  Node *beforePtr)
{
    TreeObject *corePtr = nodePtr->corePtr;
    size_t newDepth;

    if (nodePtr == beforePtr) {
	return TCL_ERROR;
    }
    if ((beforePtr != NULL) && (beforePtr->parent != parentPtr)) {
	return TCL_ERROR;
    }
    if (nodePtr->parent == NULL) {
	return TCL_ERROR;	/* Can't move root. */
    }
    /* Verify that the node isn't an ancestor of the new parent. */
    if (Blt_Tree_IsAncestor(nodePtr, parentPtr)) {
	return TCL_ERROR;
    }
    UnlinkNode(nodePtr);

    /* Relink the node as a child of the new parent. */
    LinkBefore(parentPtr, nodePtr, beforePtr);
    newDepth = parentPtr->depth + 1;
    if (nodePtr->depth != newDepth) { 
	/* Recursively reset the depths of all descendant nodes. */
	ResetDepths(nodePtr, newDepth);
    }

    /* 
     * Issue callbacks to each client indicating that a node has been moved.
     */
    NotifyClients(treePtr, corePtr, nodePtr, TREE_NOTIFY_MOVE);
    return TCL_OK;
}

int
Blt_Tree_DeleteNode(Tree *treePtr, Node *nodePtr)
{
    TreeObject *corePtr = nodePtr->corePtr;
    Node *np, *nextPtr;

    /* In depth-first order, delete each descendant node. */
    for (np = nodePtr->first; np != NULL; np = nextPtr) {
	nextPtr = np->next;
	Blt_Tree_DeleteNode(treePtr, np);
    }
    /* 
     * Issue callbacks to each client indicating that the node can no longer
     * be used.
     */
    NotifyClients(treePtr, corePtr, nodePtr, TREE_NOTIFY_DELETE);

    /* Now remove the actual node. */
    FreeNode(corePtr, nodePtr);
    return TCL_OK;
}

Blt_TreeNode
Blt_Tree_GetNode(Tree *treePtr, long inode)
{
    TreeObject *corePtr = treePtr->corePtr;
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&corePtr->nodeTable, (char *)inode);
    if (hPtr != NULL) {
	return Blt_GetHashValue(hPtr);
    }
    return NULL;
}

Blt_TreeTrace
Blt_Tree_CreateTrace(Tree *treePtr, Node *nodePtr, const char *keyPattern,
		     const char *tagName, unsigned int mask, 
		     Blt_TreeTraceProc *proc, ClientData clientData)
{
    TraceHandler *tracePtr;

    tracePtr = Blt_AssertCalloc(1, sizeof (TraceHandler));
    tracePtr->link = Blt_Chain_Append(treePtr->traces, tracePtr);
    if (keyPattern != NULL) {
	tracePtr->keyPattern = Blt_AssertStrdup(keyPattern);
    }
    if (tagName != NULL) {
	tracePtr->withTag = Blt_AssertStrdup(tagName);
    }
    tracePtr->treePtr = treePtr;
    tracePtr->proc = proc;
    tracePtr->clientData = clientData;
    tracePtr->mask = mask;
    tracePtr->nodePtr = nodePtr;
    tracePtr->interp = treePtr->interp;
    Blt_InitHashTable(&tracePtr->idleTable, sizeof(TraceWhenIdle)/sizeof(int));
    return (Blt_TreeTrace)tracePtr;
}

void
Blt_Tree_DeleteTrace(Blt_TreeTrace trace)
{
    TraceHandler *tracePtr = (TraceHandler *)trace;

    Blt_Chain_DeleteLink(tracePtr->treePtr->traces, tracePtr->link);
    if (tracePtr->keyPattern != NULL) {
	Blt_Free(tracePtr->keyPattern);
    }
    if (tracePtr->withTag != NULL) {
	Blt_Free(tracePtr->withTag);
    }
    Blt_Free(tracePtr);
}

void
Blt_Tree_RelabelNodeWithoutNotify(Node *nodePtr, const char *string)
{
    Blt_TreeKey oldLabel;
    Node **bucketPtr;
    Node *parentPtr;
    Node *np;
    unsigned int downshift;
    unsigned long mask;

    oldLabel = nodePtr->label;
    nodePtr->label = Blt_Tree_GetKeyFromNode(nodePtr, string);
    parentPtr = nodePtr->parent;
    if ((parentPtr == NULL) || (parentPtr->nodeTable == NULL)) {
	return;			/* Root node. */
    }
    /* Changing the node's name requires that we rehash the node in the
     * parent's table of children. */
    mask = (1 << parentPtr->nodeTableSize2) - 1;
    downshift = DOWNSHIFT_START - parentPtr->nodeTableSize2;
    bucketPtr = parentPtr->nodeTable + RANDOM_INDEX(oldLabel);
    if (*bucketPtr == nodePtr) {
	*bucketPtr = nodePtr->hnext;
    } else {
	for (np = *bucketPtr; /*empty*/; np = np->hnext) {
	    if (np == NULL) {
		return;	/* Can't find node in hash bucket. */
	    }
	    if (np->hnext == nodePtr) {
		np->hnext = nodePtr->hnext;
		break;
	    }
	}
    }
    bucketPtr = parentPtr->nodeTable + RANDOM_INDEX(nodePtr->label);
    nodePtr->hnext = *bucketPtr;
    *bucketPtr = nodePtr;
} 

void
Blt_Tree_RelabelNode(Tree *treePtr, Node *nodePtr, const char *string)
{
    Blt_Tree_RelabelNodeWithoutNotify(nodePtr, string);
    NotifyClients(treePtr, treePtr->corePtr, nodePtr, 
	TREE_NOTIFY_RELABEL);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_FindChild --
 *
 *	Searches for the named node in a parent's chain of siblings.  
 *
 *
 * Results:
 *	If found, the child node is returned, otherwise NULL.
 *
 *---------------------------------------------------------------------------
 */
Blt_TreeNode
Blt_Tree_FindChild(Node *parentPtr, const char *string)
{
    Blt_TreeKey key;
    Node *np;
    
    key = Blt_Tree_GetKeyFromNode(parentPtr, string);
    if (parentPtr->nodeTable != NULL) {
	unsigned int downshift;
	unsigned long mask;
	Node *bucket;

	mask = (1 << parentPtr->nodeTableSize2) - 1;
	downshift = DOWNSHIFT_START - parentPtr->nodeTableSize2;
	bucket = parentPtr->nodeTable[RANDOM_INDEX(key)];

	/* Search all of the entries in the appropriate bucket. */
	for (np = bucket; np != NULL; np = np->hnext) {
	    if (key == np->label) {
		return np;
	    }
	}
    } else {
	for (np = parentPtr->first; np != NULL; np = np->next) {
	    if (key == np->label) {
		return np;
	    }
	}
    }
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_NodePosition --
 *
 *	Returns the position of the node in its parent's list of children.
 *	The root's position is 0.
 *
 *---------------------------------------------------------------------------
 */
long
Blt_Tree_NodePosition(Node *nodePtr)
{
    Node *parentPtr;
    long count;

    count = 0;
    parentPtr = nodePtr->parent;
    if (parentPtr != NULL) {
	Node *np;

	for (np = parentPtr->first; np != NULL; np = np->next) {
	    if (nodePtr == np) {
		break;
	    }
	    count++;
	}
    }
    return count;
}

Blt_TreeNode 
Blt_Tree_FirstChild(Node *parentPtr)
{
    return parentPtr->first;
}

Blt_TreeNode 
Blt_Tree_LastChild(Node *parentPtr)
{
    return parentPtr->last;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_PrevNode --
 *
 *	Returns the "previous" node in the tree.  This node (in depth-first
 *	order) is its parent, if the node has no siblings that are previous to
 *	it.  Otherwise it is the last descendant of the last sibling.  In this
 *	case, descend the sibling's hierarchy, using the last child at any
 *	ancestor, with we we find a leaf.
 *
 *---------------------------------------------------------------------------
 */
Blt_TreeNode
Blt_Tree_PrevNode(
    Node *rootPtr,		/* Root of subtree. If NULL, indicates the
				 * tree's root. */
    Node *nodePtr)		/* Current node in subtree. */
{
    Node *prevPtr;

    if (rootPtr == NULL) {
	rootPtr = nodePtr->corePtr->root;
    }
    if (nodePtr == rootPtr) {
	return NULL;		/* The root is the first node. */
    }
    prevPtr = nodePtr->prev;
    if (prevPtr == NULL) {
	/* There are no siblings previous to this one, so pick the parent. */
	return nodePtr->parent;
    }
    /*
     * Traverse down the right-most thread, in order to select the next entry.
     * Stop when we reach a leaf.
     */
    nodePtr = prevPtr;
    while ((prevPtr = nodePtr->last) != NULL) {
	nodePtr = prevPtr;
    }
    return nodePtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_NextNode --
 *
 *	Returns the "next" node in relation to the given node.  The next node
 *	(in depth-first order) is either the first child of the given node the
 *	next sibling if the node has no children (the node is a leaf).  If the
 *	given node is the last sibling, then try it's parent next sibling.
 *	Continue until we either find a next sibling for some ancestor or we
 *	reach the root node.  In this case the current node is the last node
 *	in the tree.
 *
 *---------------------------------------------------------------------------
 */
Blt_TreeNode
Blt_Tree_NextNode(
    Node *rootPtr,		/* Root of subtree. If NULL, indicates the
				 * tree's root. */
    Node *nodePtr)		/* Current node in subtree. */
{
    Node *nextPtr;

    /* Pick the first sub-node. */
    nextPtr = nodePtr->first;
    if (nextPtr != NULL) {
	return nextPtr;
    }
    /* 
     * Back up until we can find a level where we can pick a "next sibling".
     * For the last entry we'll thread our way back to the root.
     */
    if (rootPtr == NULL) {
	rootPtr = nodePtr->corePtr->root;
    }
    while (nodePtr != rootPtr) {
	nextPtr = nodePtr->next;
	if (nextPtr != NULL) {
	    return nextPtr;
	}
	nodePtr = nodePtr->parent;
    }
    return NULL;		/* At root, no next node. */
}


int
Blt_Tree_IsBefore(Node *n1Ptr, Node *n2Ptr)
{
    long depth;
    long i;
    Node *nodePtr;

    if (n1Ptr == n2Ptr) {
	return FALSE;
    }
    depth = MIN(n1Ptr->depth, n2Ptr->depth);
    if (depth == 0) {		/* One of the nodes is root. */
	return (n1Ptr->parent == NULL);
    }
    /* 
     * Traverse back from the deepest node, until both nodes are at the same
     * depth.  Check if this ancestor node is the same for both nodes.
     */
    for (i = n1Ptr->depth; i > depth; i--) {
	n1Ptr = n1Ptr->parent;
    }
    if (n1Ptr == n2Ptr) {
	return FALSE;
    }
    for (i = n2Ptr->depth; i > depth; i--) {
	n2Ptr = n2Ptr->parent;
    }
    if (n2Ptr == n1Ptr) {
	return TRUE;
    }

    /* 
     * First find the mutual ancestor of both nodes.  Look at each preceding
     * ancestor level-by-level for both nodes.  Eventually we'll find a node
     * that's the parent of both ancestors.  Then find the first ancestor in
     * the parent's list of subnodes.
     */
    for (i = depth; i > 0; i--) {
	if (n1Ptr->parent == n2Ptr->parent) {
	    break;
	}
	n1Ptr = n1Ptr->parent;
	n2Ptr = n2Ptr->parent;
    }
    for (nodePtr = n1Ptr->parent->first; nodePtr != NULL; 
	 nodePtr = nodePtr->next) {
	if (nodePtr == n1Ptr) {
	    return TRUE;
	} else if (nodePtr == n2Ptr) {
	    return FALSE;
	}
    }
    return FALSE;
}

/*
 *---------------------------------------------------------------------------
 *
 * TraceIdleProc --
 *
 *	Used to invoke event handler routines at some idle point.  This
 *	routine is called from the TCL event loop.  Errors generated by the
 *	event handler routines are backgrounded.
 *	
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static void
TraceIdleProc(ClientData clientData)
{
    Blt_HashEntry *hashPtr = clientData;
    TraceWhenIdle *idlePtr;
    TraceHandler *tracePtr;
    int result;
    Node *nodePtr;
    Tcl_Interp *interp;
    Blt_TreeKey key;
    int flags;

    idlePtr = Blt_GetHashValue(hashPtr);
    tracePtr = idlePtr->tracePtr;
    interp = idlePtr->interp;

    /* Get the node from the inode since the node may be been deleted in the
     * time between the idle proc was issued and invoked. */
    nodePtr = Blt_Tree_GetNode(tracePtr->treePtr, idlePtr->inode);
    if (nodePtr != NULL) {
	key = idlePtr->key;
	flags = idlePtr->flags;
	Blt_DeleteHashEntry(&tracePtr->idleTable, hashPtr);
	result = (*tracePtr->proc) (tracePtr->clientData, interp, nodePtr, 
		key, flags);
	if (result != TCL_OK) {
	    Tcl_BackgroundError(interp);
	}
	nodePtr->flags &= ~TREE_TRACE_ACTIVE;
    }
}

static void
CallTraces(
    Tcl_Interp *interp,
    Tree *sourcePtr,		/* Client holding a reference to the tree.  If
				 * NULL, indicates to execute all handlers,
				 * including those of the caller. */
    TreeObject *corePtr,	/* Tree that was changed. */
    Node *nodePtr,		/* Node that received the event. */
    Blt_TreeKey key,
    unsigned int flags)
{
    Tree *treePtr;

    for(treePtr = FirstClient(corePtr); treePtr != NULL; 
	treePtr = NextClient(treePtr)) {
	Blt_ChainLink link;

	for(link = Blt_Chain_FirstLink(treePtr->traces); link != NULL; 
	    link = Blt_Chain_NextLink(link)) {
	    TraceHandler *tracePtr;	

	    tracePtr = Blt_Chain_GetValue(link);
	    if ((tracePtr->keyPattern != NULL) && 
		(!Tcl_StringMatch(key, tracePtr->keyPattern))) {
		continue;		/* Key pattern doesn't match. */
	    }
	    if ((tracePtr->withTag != NULL) && 
		(!Blt_Tree_HasTag(treePtr, nodePtr, tracePtr->withTag))) {
		continue;		/* Doesn't have the tag. */
	    }
	    if ((tracePtr->mask & flags) == 0) {
		continue;		/* Flags don't match. */
	    }
	    if ((treePtr == sourcePtr) && 
		(tracePtr->mask & TREE_TRACE_FOREIGN_ONLY)) {
		continue;		/* This client initiated the trace. */
	    }
	    if ((tracePtr->nodePtr != NULL) && (tracePtr->nodePtr != nodePtr)) {
		continue;		/* Nodes don't match. */
	    }
	    if (tracePtr->mask & TREE_TRACE_WHENIDLE) {
		TraceWhenIdle idle;
		int isNew;
		Blt_HashEntry *hPtr;

		memset(&idle, 0, sizeof(idle));
		idle.interp = sourcePtr->interp;
		idle.tracePtr = tracePtr;
		idle.key = key;
		idle.flags = flags;
		idle.inode = nodePtr->inode;
		hPtr = Blt_CreateHashEntry(&tracePtr->idleTable, (char *)&idle, 
			&isNew);
		if (isNew) {
		    Blt_SetHashValue(hPtr, 
			Blt_GetHashKey(&tracePtr->idleTable, hPtr));
		    Tcl_DoWhenIdle(TraceIdleProc, hPtr);
		}
	    } else {
		nodePtr->flags |= TREE_TRACE_ACTIVE;
		if ((*tracePtr->proc) (tracePtr->clientData, sourcePtr->interp, 
				       nodePtr, key, flags) != TCL_OK) {
		    if (interp != NULL) {
			Tcl_BackgroundError(interp);
		    }
		}
		nodePtr->flags &= ~TREE_TRACE_ACTIVE;
	    }
	}
    }
}

static Value *
GetTreeValue(Tcl_Interp *interp, Tree *treePtr, Node *nodePtr, Blt_TreeKey key)
{
    Value *valuePtr;

    valuePtr = TreeFindValue(nodePtr, key); 
    if (valuePtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find field \"", key, "\"", 
			     (char *)NULL);
	}
	return NULL;
    }	
    if ((valuePtr->owner != NULL) && (valuePtr->owner != treePtr)) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't access private field \"", 
			     key, "\"", (char *)NULL);
	}
	return NULL;
    }
    return valuePtr;
}

int
Blt_Tree_PrivateValue(Tcl_Interp *interp, Tree *treePtr, Node *nodePtr,
		      Blt_TreeKey key)
{
    Value *valuePtr;

    valuePtr = TreeFindValue(nodePtr, key); 
    if (valuePtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find field \"", key, "\".", 
			     (char *)NULL);
	}
	return TCL_ERROR;
    }
    valuePtr->owner = treePtr;
    return TCL_OK;
}

int
Blt_Tree_PublicValue(Tcl_Interp *interp, Tree *treePtr, Node *nodePtr,
		     Blt_TreeKey key)
{
    Value *valuePtr;

    valuePtr = TreeFindValue(nodePtr, key); 
    if (valuePtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find field \"", key, "\"", 
			     (char *)NULL);
	}
	return TCL_ERROR;
    }
    if (valuePtr->owner != treePtr) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "not the owner of \"", key, "\"", 
		     (char *)NULL);
	}
	return TCL_ERROR;
    }
    valuePtr->owner = NULL;
    return TCL_OK;
}

int
Blt_Tree_ValueExistsByKey(Tree *treePtr, Node *nodePtr, Blt_TreeKey key)
{
    Value *valuePtr;

    valuePtr = GetTreeValue((Tcl_Interp *)NULL, treePtr, nodePtr, key);
    if (valuePtr == NULL) {
	return FALSE;
    }
    return TRUE;
}

int
Blt_Tree_GetValueByKey(Tcl_Interp *interp, Tree *treePtr, Node *nodePtr,
		       Blt_TreeKey key, Tcl_Obj **valueObjPtrPtr)
{
    Value *valuePtr;
    TreeObject *corePtr = nodePtr->corePtr;

    valuePtr = GetTreeValue(interp, treePtr, nodePtr, key);
    if (valuePtr == NULL) {
	return TCL_ERROR;
    }
    *valueObjPtrPtr = valuePtr->objPtr;
    if (!(nodePtr->flags & TREE_TRACE_ACTIVE)) {
	CallTraces(interp, treePtr, corePtr, nodePtr, key, 
		   TREE_TRACE_READ);
    }
    return TCL_OK;
}

int
Blt_Tree_SetValueByKey(
    Tcl_Interp *interp,
    Tree *treePtr,
    Node *nodePtr,			/* Node to be updated. */
    Blt_TreeKey key,			/* Identifies the field key. */
    Tcl_Obj *valueObjPtr)		/* New value of field. */
{
    TreeObject *corePtr = nodePtr->corePtr;
    Value *valuePtr;
    int isNew;
    unsigned int flags;

    if (valueObjPtr == NULL) {
	return Blt_Tree_UnsetValueByKey(interp, treePtr, nodePtr, key);
    }
    valuePtr = TreeCreateValue(nodePtr, key, &isNew);
    if ((valuePtr->owner != NULL) && (valuePtr->owner != treePtr)) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't set private field \"", 
			     key, "\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    if (valueObjPtr != valuePtr->objPtr) {
	Tcl_IncrRefCount(valueObjPtr);
	if (valuePtr->objPtr != NULL) {
	    Tcl_DecrRefCount(valuePtr->objPtr);
	}
	valuePtr->objPtr = valueObjPtr;
    }
    flags = TREE_TRACE_WRITE;
    if (isNew) {
	flags |= TREE_TRACE_CREATE;
    }
    if (!(nodePtr->flags & TREE_TRACE_ACTIVE)) {
	CallTraces(interp, treePtr, corePtr, nodePtr, valuePtr->key, flags);
    }
    return TCL_OK;
}

int
Blt_Tree_UnsetValueByKey(
    Tcl_Interp *interp,
    Tree *treePtr,
    Node *nodePtr,		/* Node to be updated. */
    Blt_TreeKey key)		/* Name of field in node. */
{
    TreeObject *corePtr = nodePtr->corePtr;
    Value *valuePtr;

    valuePtr = TreeFindValue(nodePtr, key);
    if (valuePtr == NULL) {
	return TCL_OK;		/* It's okay to unset values that don't exist
				 * in the node. */
    }
    if ((valuePtr->owner != NULL) && (valuePtr->owner != treePtr)) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't unset private field \"", 
			     key, "\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    TreeDeleteValue(nodePtr, valuePtr);
    CallTraces(interp, treePtr, corePtr, nodePtr, key, TREE_TRACE_UNSET);
    return TCL_OK;
}

static int
ParseParentheses(Tcl_Interp *interp, const char *string, 
		 char **leftPtr, char **rightPtr)
{
    char *p;
    char *left, *right;

    left = right = NULL;
    for (p = (char *)string; *p != '\0'; p++) {
	if (*p == '(') {
	    left = p;
	} else if (*p == ')') {
	    right = p;
	}
    }
    if (left != right) {
	if (((left != NULL) && (right == NULL)) ||
	    ((left == NULL) && (right != NULL)) ||
	    (left > right) || (right != (p - 1))) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "bad array specification \"", string,
			     "\"", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
    }
    *leftPtr = left;
    *rightPtr = right;
    return TCL_OK;
}

static Tree *
NewTree(TreeInterpData *dataPtr, TreeObject *corePtr, const char *qualName)
{
    Tree *treePtr;
    int isNew;

    treePtr = Blt_Calloc(1, sizeof(Tree));
    if (treePtr == NULL) {
	return NULL;
    }

    treePtr->magic = TREE_MAGIC;
    treePtr->interp = dataPtr->interp;
    /* Add client to table object's list of clients. */
    treePtr->link = Blt_Chain_Append(corePtr->clients, treePtr);
    treePtr->corePtr = corePtr;
    treePtr->root = corePtr->root;

    /* By default, use own sets of tags. */
    Blt_Tree_NewTagTable(treePtr);

    treePtr->hPtr = Blt_CreateHashEntry(&dataPtr->treeTable, qualName, &isNew);
    Blt_SetHashValue(treePtr->hPtr, treePtr);
    treePtr->name = Blt_GetHashKey(&dataPtr->treeTable, treePtr->hPtr);
    treePtr->events = Blt_Chain_Create();
    treePtr->traces = Blt_Chain_Create();
    return treePtr;
}

static const char *
MakeTreeName(TreeInterpData *dataPtr, char *string)
{
    /* Generate a unique tree name in the current namespace. */
    do  {
	sprintf_s(string, 200, "tree%d", dataPtr->nextId++);
    } while (GetTree(dataPtr, string, NS_SEARCH_CURRENT) != NULL);
    return string;
} 

/*
 *---------------------------------------------------------------------------
 *
 * ShareTagTable --
 *
 *---------------------------------------------------------------------------
 */
static void
ShareTagTable(Tree *sourcePtr, Tree *targetPtr)
{
    sourcePtr->tagTablePtr->refCount++;
    if (targetPtr->tagTablePtr != NULL) {
	ReleaseTagTable(targetPtr->tagTablePtr);
    }
    targetPtr->tagTablePtr = sourcePtr->tagTablePtr;
}

int
Blt_Tree_GetValue(
    Tcl_Interp *interp,
    Tree *treePtr,
    Node *nodePtr,
    const char *string,		/* String identifying the field in node. */
    Tcl_Obj **valueObjPtrPtr)
{
    char *left, *right;
    int result;

    if (ParseParentheses(interp, string, &left, &right) != TCL_OK) {
	return TCL_ERROR;
    }
    if (left != NULL) {
	*left = *right = '\0';
	result = Blt_Tree_GetArrayValue(interp, treePtr, nodePtr, string, 
		left + 1, valueObjPtrPtr);
	*left = '(', *right = ')';
    } else {
	result = Blt_Tree_GetValueByKey(interp, treePtr, nodePtr, 
		Blt_Tree_GetKey(treePtr, string), valueObjPtrPtr);
    }
    return result;
}

int
Blt_Tree_SetValue(
    Tcl_Interp *interp,
    Tree *treePtr,
    Node *nodePtr,		/* Node to be updated. */
    const char *string,		/* String identifying the field in node. */
    Tcl_Obj *valueObjPtr)	/* New value of field. If NULL, field is
				 * deleted. */
{
    char *left, *right;
    int result;

    if (ParseParentheses(interp, string, &left, &right) != TCL_OK) {
	return TCL_ERROR;
    }
    if (left != NULL) {
	*left = *right = '\0';
	result = Blt_Tree_SetArrayValue(interp, treePtr, nodePtr, string, 
		left + 1, valueObjPtr);
	*left = '(', *right = ')';
    } else {
	result = Blt_Tree_SetValueByKey(interp, treePtr, nodePtr, 
		Blt_Tree_GetKey(treePtr, string), valueObjPtr);
    }
    return result;
}

int
Blt_Tree_UnsetValue(
    Tcl_Interp *interp,
    Tree *treePtr,
    Node *nodePtr,		/* Node to be updated. */
    const char *string)		/* String identifying the field in node. */
{
    char *left, *right;
    int result;

    if (ParseParentheses(interp, string, &left, &right) != TCL_OK) {
	return TCL_ERROR;
    }
    if (left != NULL) {
	*left = *right = '\0';
	result = Blt_Tree_UnsetArrayValue(interp, treePtr, nodePtr, string, 
		left + 1);
	*left = '(', *right = ')';
    } else {
	result = Blt_Tree_UnsetValueByKey(interp, treePtr, nodePtr, 
		Blt_Tree_GetKey(treePtr, string));
    }
    return result;
}

int
Blt_Tree_ValueExists(Tree *treePtr, Node *nodePtr, const char *string)
{
    char *left, *right;
    int result;

    if (ParseParentheses((Tcl_Interp *)NULL, string, &left, &right) != TCL_OK) {
	return FALSE;
    }
    if (left != NULL) {
	*left = *right = '\0';
	result = Blt_Tree_ArrayValueExists(treePtr, nodePtr, string, left + 1);
	*left = '(', *right = ')';
    } else {
	result = Blt_Tree_ValueExistsByKey(treePtr, nodePtr, 
		Blt_Tree_GetKey(treePtr, string));
    }
    return result;
}

Blt_TreeKey
Blt_Tree_FirstKey(Tree *treePtr, Node *nodePtr, Blt_TreeKeyIterator *iterPtr)
{
    Value *valuePtr;
    
    valuePtr = TreeFirstValue(nodePtr, iterPtr);
    if (valuePtr == NULL) {
	return NULL;
    }
    while ((valuePtr->owner != NULL) && (valuePtr->owner != treePtr)) {
	valuePtr = TreeNextValue(iterPtr);
	if (valuePtr == NULL) {
	    return NULL;
	}
    }
    return valuePtr->key;
}

Blt_TreeKey
Blt_Tree_NextKey(Tree *treePtr, Blt_TreeKeyIterator *iterPtr)
{
    Value *valuePtr;

    valuePtr = TreeNextValue(iterPtr);
    if (valuePtr == NULL) {
	return NULL;
    }
    while ((valuePtr->owner != NULL) && (valuePtr->owner != treePtr)) {
	valuePtr = TreeNextValue(iterPtr);
	if (valuePtr == NULL) {
	    return NULL;
	}
    }
    return valuePtr->key;
}

int
Blt_Tree_IsAncestor(Node *n1Ptr, Node *n2Ptr)
{
    if (n2Ptr != NULL) {
	n2Ptr = n2Ptr->parent;
	while (n2Ptr != NULL) {
	    if (n2Ptr == n1Ptr) {
		return TRUE;
	    }
	    n2Ptr = n2Ptr->parent;
	}
    }
    return FALSE;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_SortNode --
 *
 *	Sorts the subnodes at a given node.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *---------------------------------------------------------------------------
 */
int
Blt_Tree_SortNode(Tree *treePtr, Node *nodePtr, Blt_TreeCompareNodesProc *proc)
{
    Node **nodes;
    int nNodes;
    Node *np, **npp;

    nNodes = nodePtr->nChildren;
    if (nNodes < 2) {
	return TCL_OK;
    }
    nodes = Blt_Malloc((nNodes + 1) * sizeof(Node *));
    if (nodes == NULL) {
	return TCL_ERROR;	/* Out of memory. */
    }
    for (npp = nodes, np = nodePtr->first; np != NULL; np = np->next, npp++) {
	*npp = np;
    }
    *npp = NULL;

    qsort(nodes, nNodes, sizeof(Node *), (QSortCompareProc *)proc);
    for (npp = nodes; *npp != NULL; npp++) {
	UnlinkNode(*npp);
	LinkBefore(nodePtr, *npp, (Blt_TreeNode)NULL);
    }
    Blt_Free(nodes);
    NotifyClients(treePtr, nodePtr->corePtr, nodePtr, TREE_NOTIFY_SORT);
    return TCL_OK;
}

#define TEST_RESULT(result) \
	switch (result) { \
	case TCL_CONTINUE: \
	    return TCL_OK; \
	case TCL_OK: \
	    break; \
	default: \
	    return (result); \
	}

int
Blt_Tree_Apply(
    Node *branchPtr,		/* Root node of subtree. */
    Blt_TreeApplyProc *proc,	/* Procedure to call for each node. */
    ClientData clientData)	/* One-word of data passed when calling
				 * proc. */
{
    Node *np, *nextPtr;

    for (np = branchPtr->first; np != NULL; np = nextPtr) {
	int result;

	/* 
	 * Get the next link in the chain before calling Blt_TreeApply
	 * recursively.  This is because the apply callback may delete the
	 * node and its link.
	 */
	nextPtr = np->next;

	result = Blt_Tree_Apply(np, proc, clientData);
	TEST_RESULT(result);
    }
    return (*proc) (branchPtr, clientData, TREE_POSTORDER);
}

int
Blt_Tree_ApplyDFS(
    Node *branchPtr,		/* Root node of subtree. */
    Blt_TreeApplyProc *proc,	/* Procedure to call for each node. */
    ClientData clientData,	/* One-word of data passed when calling
				 * proc. */
    int order)			/* Order of traversal. */
{
    Node *nodePtr, *nextPtr;
    int result;

    if (order & TREE_PREORDER) {
	result = (*proc) (branchPtr, clientData, TREE_PREORDER);
	TEST_RESULT(result);
    }
    nodePtr = branchPtr->first;
    if (order & TREE_INORDER) {
	if (nodePtr != NULL) {
	    result = Blt_Tree_ApplyDFS(nodePtr, proc, clientData, order);
	    TEST_RESULT(result);
	    nodePtr = nodePtr->next;
	}
	result = (*proc) (branchPtr, clientData, TREE_INORDER);
	TEST_RESULT(result);
    }
    for (/* empty */; nodePtr != NULL; nodePtr = nextPtr) {
	/* 
	 * Get the next link in the chain before calling Blt_Tree_Apply
	 * recursively.  This is because the apply callback may delete the
	 * node and its link.
	 */
	nextPtr = nodePtr->next;
	result = Blt_Tree_ApplyDFS(nodePtr, proc, clientData, order);
	TEST_RESULT(result);
    }
    if (order & TREE_POSTORDER) {
	return (*proc) (branchPtr, clientData, TREE_POSTORDER);
    }
    return TCL_OK;
}

int
Blt_Tree_ApplyBFS(
    Node *branchPtr,		/* Root node of subtree. */
    Blt_TreeApplyProc *proc,	/* Procedure to call for each node. */
    ClientData clientData)	/* One-word of data passed when calling
				 * proc. */
{
    Blt_Chain queue;
    Blt_ChainLink link, next;
    Node *nodePtr;
    int result;

    queue = Blt_Chain_Create();
    link = Blt_Chain_Append(queue, branchPtr);
    while (link != NULL) {
	Node *np;

	nodePtr = Blt_Chain_GetValue(link);
	/* Add the children to the queue. */
	for (np = nodePtr->first; np != NULL; np = np->next) {
	    Blt_Chain_Append(queue, np);
	}
	/* Process the node. */
	result = (*proc) (nodePtr, clientData, TREE_BREADTHFIRST);
	switch (result) { 
	case TCL_CONTINUE: 
	    Blt_Chain_Destroy(queue);
	    return TCL_OK; 
	case TCL_OK: 
	    break; 
	default: 
	    Blt_Chain_Destroy(queue);
	    return result; 
	}
	/* Remove the node from the queue. */
	next = Blt_Chain_NextLink(link);
	Blt_Chain_DeleteLink(queue, link);
	link = next;
    }
    Blt_Chain_Destroy(queue);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_Attach --
 *
 *	Attaches the tree object of the named tree to the current one.  This
 *	lets a tree client change its core tree object.  If the name of the
 *	tree (used to get the tree object) is the empty string (""), then
 *	create a new tree object.
 *
 * Results:
 *	Returns a standard TCL result.  If an error occurs, TCL_ERROR is
 *	returned and an error message if left in the interpreter result.
 *
 * Side Effects:
 *	The tree's traces and notifiers are deactivated and removed.  The
 *	tree's old core tree object is released.  This may destroy the old
 *	tree object is no client is still using it.  The tag table is also
 *	reset or shared with the named client.
 *
 *---------------------------------------------------------------------------
 */
int
Blt_Tree_Attach(Tcl_Interp *interp, Tree *treePtr, const char *name)
{
    TreeObject *corePtr;
    Blt_ChainLink link;
    TreeInterpData *dataPtr;

    dataPtr = treePtr->corePtr->dataPtr;
    if (name[0] == '\0') {
	/* Create a new tree object. */
	corePtr = NewTreeObject(dataPtr);
	if (corePtr == NULL) {
	    Tcl_AppendResult(interp, "can't allocate a new tree object.", 
		(char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	Tree *newPtr;

	newPtr = GetTree(dataPtr, name, NS_SEARCH_BOTH);
	if ((newPtr == NULL) || (newPtr->corePtr == NULL)) {
	    Tcl_AppendResult(interp, "can't find a tree named \"", name, "\"", 
			     (char *)NULL);
	    return TCL_ERROR;
	}
	corePtr = newPtr->corePtr;
	ShareTagTable(newPtr, treePtr);
    }
    /* 
     * Be sure to add the client to new tree object, before releasing the old
     * tree object. This is so that reattaching to the same tree object
     * doesn't delete the tree object.
     */
    link = Blt_Chain_Append(corePtr->clients, treePtr);
    ReleaseTreeObject(treePtr);
    ResetTree(treePtr);		/* Reset the client's traces and notifiers. */
    /* Update pointers in the client. */
    treePtr->link = link;
    treePtr->corePtr = corePtr;
    treePtr->root = corePtr->root;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_Open --
 *
 *	Creates a tree using an existing or new tree object.  The returned
 *	tree must be freed by the caller using Blt_Tree_Close.  If the name of
 *	the tree (used to get the tree object) is the empty string (""), then
 *	create a new tree object.  The following flags may be used:
 *
 *	TREE_CREATE	Create a tree named "name". A new tree object is 
 *			automatically created. If the "name" is NULL, then 
 *			automatically generate a new name.
 *	
 *	TREE_ATTACH     Create a tree and use the tree object from an 
 *			existing tree given by "name". By default
 *			the tag table is shared. 
 *
 *	TREE_NEWTAGS    By default the tag table is shared.  This flag 
 *			indicates to not share tags and to start with an 
 *			empty tag table.
 *
 * Results:
 *	Returns a standard TCL result.  If an error occurs, TCL_ERROR
 *	is returned and an error message if left in the interpreter result.
 *
 * Side Effects:
 *	The tree's traces and notifiers are deactivated and removed.  The
 *	tree's old core tree object is release.  This may destroy the tree
 *	object is no client is still using it.  The tag table is also reset or
 *	shared with the named client.
 *
 *---------------------------------------------------------------------------
 */
Blt_Tree
Blt_Tree_Open(
    Tcl_Interp *interp,		/* Interpreter to report errors back to. */
    const char *name,		/* Name of tree in namespace. */
    int flags)			/* TREE_ATTACH|TREE_NEWTAGS */
{
    Blt_ObjectName objName;
    Tree *srcPtr, *destPtr;
    TreeObject *corePtr;
    Tcl_DString ds;
    TreeInterpData *dataPtr;
    const char *qualName;
    char string[200];

    dataPtr = Blt_Tree_GetInterpData(interp);
    srcPtr = NULL;
    /* If there's a name available, see it's a tree. */
    if (name != NULL) {
	srcPtr = GetTree(dataPtr, name, NS_SEARCH_BOTH);
    } else if ((flags & TREE_CREATE) == 0) {
	Tcl_AppendResult(interp, "no tree name given to attach", (char *)NULL);
	return NULL;
    }	
    /* Either create a core tree object or use the one from the named tree.*/
    if (flags & TREE_CREATE) {
	if (srcPtr != NULL) {
	    Tcl_AppendResult(interp, "tree \"", name, "\" already exists", 
			     (char *)NULL);
	    return NULL;
	}
	corePtr = NewTreeObject(dataPtr);
	if (corePtr == NULL) {
	    Tcl_AppendResult(interp, "can't allocate tree object.", 
			     (char *)NULL);
	    return NULL;
	}
    } else {
	if ((srcPtr == NULL) || (srcPtr->corePtr == NULL)) {	
	    Tcl_AppendResult(interp, "can't find a tree named \"", name, "\"", 
			     (char *)NULL);
	    return NULL;
	}
	corePtr = srcPtr->corePtr;
    }    
    /* Generate a new tree name if one wasn't already provided. */
    if (name == NULL) {
	name = MakeTreeName(dataPtr, string);
    }
    /* 
     * Tear apart and put back together the namespace-qualified name of the
     * tree. This is to ensure that naming is consistent.
     */ 
    if (!Blt_ParseObjectName(interp, name, &objName, 0)) {
	return NULL;
    }
    qualName = Blt_MakeQualifiedName(&objName, &ds);
    destPtr = NewTree(dataPtr, corePtr, qualName);
    Tcl_DStringFree(&ds);
    if (destPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate tree token", (char *)NULL);
	return NULL;
    }
    if (((flags & TREE_NEWTAGS) == 0) && (srcPtr != NULL)) {
	ShareTagTable(srcPtr, destPtr);
    }
    return destPtr;
}

void
Blt_Tree_Close(Tree *treePtr)
{
    if (treePtr->magic != TREE_MAGIC) {
	fprintf(stderr, "invalid tree object token 0x%lx\n", 
		(unsigned long)treePtr);
	return;
    }
    DestroyTree(treePtr);
}

int
Blt_Tree_Exists(Tcl_Interp *interp, const char *name)
{
    TreeInterpData *dataPtr;

    dataPtr = Blt_Tree_GetInterpData(interp);
    return (GetTree(dataPtr, name, NS_SEARCH_BOTH) != NULL);
}

/*ARGSUSED*/
static int
SizeApplyProc(Node *nodePtr, ClientData clientData, int order)
{
    int *sumPtr = clientData;
    *sumPtr = *sumPtr + 1;
    return TCL_OK;
}

int
Blt_Tree_Size(Node *nodePtr)
{
    int sum;

    sum = 0;
    Blt_Tree_Apply(nodePtr, SizeApplyProc, &sum);
    return sum;
}


void
Blt_Tree_CreateEventHandler(Tree *treePtr, unsigned int mask, 
			    Blt_TreeNotifyEventProc *proc, 
			    ClientData clientData)
{
    Blt_ChainLink link;
    NotifyEventHandler *notifyPtr;

    notifyPtr = NULL;		/* Suppress compiler warning. */

    /* Check if the event is already handled. */
    for(link = Blt_Chain_FirstLink(treePtr->events); 
	link != NULL; link = Blt_Chain_NextLink(link)) {
	notifyPtr = Blt_Chain_GetValue(link);
	if ((notifyPtr->proc == proc) && 
	    (notifyPtr->mask == mask) &&
	    (notifyPtr->clientData == clientData)) {
	    break;
	}
    }
    if (link == NULL) {
	notifyPtr = Blt_AssertMalloc(sizeof (NotifyEventHandler));
	link = Blt_Chain_Append(treePtr->events, notifyPtr);
    }
    if (proc == NULL) {
	Blt_Chain_DeleteLink(treePtr->events, link);
	Blt_Free(notifyPtr);
    } else {
	notifyPtr->proc = proc;
	notifyPtr->clientData = clientData;
	notifyPtr->mask = mask;
	notifyPtr->notifyPending = FALSE;
	notifyPtr->interp = treePtr->interp;
    }
}

void
Blt_Tree_DeleteEventHandler(Tree *treePtr, unsigned int mask, 
			    Blt_TreeNotifyEventProc *proc, 
			    ClientData clientData)
{
    Blt_ChainLink link;

    for(link = Blt_Chain_FirstLink(treePtr->events); link != NULL; 
	link = Blt_Chain_NextLink(link)) {
	NotifyEventHandler *notifyPtr;

	notifyPtr = Blt_Chain_GetValue(link);
	if ((notifyPtr->proc == proc) && (notifyPtr->mask == mask) &&
	    (notifyPtr->clientData == clientData)) {
	    if (notifyPtr->notifyPending) {
		Tcl_CancelIdleCall(NotifyIdleProc, notifyPtr);
	    }
	    Blt_Chain_DeleteLink(treePtr->events, link);
	    Blt_Free(notifyPtr);
	    return;
	}
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_PathFromNode --
 *
 *---------------------------------------------------------------------------
 */
const char *
Blt_Tree_NodeRelativePath(
    Node *rootPtr,		/* Root of subtree. */
    Node *nodePtr,		/* Node whose path is to be returned. */
    const char *separator,	/* Character string to separator elements. */
    unsigned int flags,		/* Indicates how to print the path. */
    Tcl_DString *dsPtr)		/* (out) Contains the path of the node. */
{
    const char **names;		/* Used to stack the component names. */
    const char *staticSpace[64];
    long i;
    long nLevels;

    if (rootPtr == NULL) {
	rootPtr = nodePtr->corePtr->root;
    }
    nLevels = Blt_Tree_NodeDepth(nodePtr) - Blt_Tree_NodeDepth(rootPtr);
    if (flags & TREE_INCLUDE_ROOT) {
	nLevels++;
    }
    if (nLevels > 64) {
	names = Blt_AssertMalloc(nLevels * sizeof(const char *));
    } else {
	names = staticSpace;
    }
    for (i = nLevels; i > 0; i--) {
	/* Save the name of each ancestor in the name array.  Note that we
	 * ignore the root. */
	names[i - 1] = nodePtr->label;
	nodePtr = nodePtr->parent;
    }
    /* Append each the names in the array. */
    if ((nLevels > 0) && (separator != NULL)) {
	Tcl_DStringAppend(dsPtr, names[0], -1);
	for (i = 1; i < nLevels; i++) {
	    Tcl_DStringAppend(dsPtr, separator, -1);
	    Tcl_DStringAppend(dsPtr, names[i], -1);
	}
    } else {
	for (i = 0; i < nLevels; i++) {
	    Tcl_DStringAppendElement(dsPtr, names[i]);
	}
    }
    if (names != staticSpace) {
	Blt_Free(names);
    }
    return Tcl_DStringValue(dsPtr);
}
    
/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_NodePath --
 *
 *---------------------------------------------------------------------------
 */
const char *
Blt_Tree_NodePath(Node *nodePtr, Tcl_DString *dsPtr)
{
    Blt_TreeNode root;

    root = nodePtr->corePtr->root;
    return Blt_Tree_NodeRelativePath(root, nodePtr, NULL, 0, dsPtr);
}

int
Blt_Tree_ArrayValueExists(Tree *treePtr, Node *nodePtr, const char *arrayName, 
			  const char *elemName)
{
    Blt_TreeKey key;
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    Value *valuePtr;

    key = Blt_Tree_GetKey(treePtr, arrayName);
    valuePtr = GetTreeValue((Tcl_Interp *)NULL, treePtr, nodePtr, key);
    if (valuePtr == NULL) {
	return FALSE;
    }
    if (Tcl_IsShared(valuePtr->objPtr)) {
	Tcl_DecrRefCount(valuePtr->objPtr);
	valuePtr->objPtr = Tcl_DuplicateObj(valuePtr->objPtr);
	Tcl_IncrRefCount(valuePtr->objPtr);
    }
    if (Blt_GetArrayFromObj((Tcl_Interp *)NULL, valuePtr->objPtr, &tablePtr) 
	!= TCL_OK) {
	return FALSE;
    }
    hPtr = Blt_FindHashEntry(tablePtr, elemName);
    return (hPtr != NULL);
}

int
Blt_Tree_GetArrayValue(Tcl_Interp *interp, Tree *treePtr, Node *nodePtr,
		       const char *arrayName, const char *elemName,
		       Tcl_Obj **valueObjPtrPtr)
{
    Blt_TreeKey key;
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    Value *valuePtr;

    key = Blt_Tree_GetKey(treePtr, arrayName);
    valuePtr = GetTreeValue(interp, treePtr, nodePtr, key);
    if (valuePtr == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_IsShared(valuePtr->objPtr)) {
	Tcl_DecrRefCount(valuePtr->objPtr);
	valuePtr->objPtr = Tcl_DuplicateObj(valuePtr->objPtr);
	Tcl_IncrRefCount(valuePtr->objPtr);
    }
    if (Blt_GetArrayFromObj(interp, valuePtr->objPtr, &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    hPtr = Blt_FindHashEntry(tablePtr, elemName);
    if (hPtr == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't find \"", arrayName, "(",
			     elemName, ")\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    *valueObjPtrPtr = Blt_GetHashValue(hPtr);

    /* Reading any element of the array can cause a trace to fire. */
    if (!(nodePtr->flags & TREE_TRACE_ACTIVE)) {
	CallTraces(interp, treePtr, nodePtr->corePtr, nodePtr, key, 
		   TREE_TRACE_READ);
    }
    return TCL_OK;
}

int
Blt_Tree_SetArrayValue(Tcl_Interp *interp, Tree *treePtr, Node *nodePtr,
		       const char *arrayName, const char *elemName,
		       Tcl_Obj *valueObjPtr)
{
    Blt_TreeKey key;
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    Value *valuePtr;
    unsigned int flags;
    int isNew;

    assert(valueObjPtr != NULL);

    /* 
     * Search for the array in the list of data fields.  If one doesn't exist,
     * create it.
     */
    key = Blt_Tree_GetKey(treePtr, arrayName);
    valuePtr = TreeCreateValue(nodePtr, key, &isNew);
    if ((valuePtr->owner != NULL) && (valuePtr->owner != treePtr)) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't set private field \"", 
			     key, "\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    flags = TREE_TRACE_WRITE;
    if (isNew) {
	valuePtr->objPtr = Blt_NewArrayObj(0, (Tcl_Obj **)NULL);
	Tcl_IncrRefCount(valuePtr->objPtr);
	flags |= TREE_TRACE_CREATE;
    } else if (Tcl_IsShared(valuePtr->objPtr)) {
	Tcl_DecrRefCount(valuePtr->objPtr);
	valuePtr->objPtr = Tcl_DuplicateObj(valuePtr->objPtr);
	Tcl_IncrRefCount(valuePtr->objPtr);
    }
    if (Blt_GetArrayFromObj(interp, valuePtr->objPtr, &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_InvalidateStringRep(valuePtr->objPtr);
    hPtr = Blt_CreateHashEntry(tablePtr, elemName, &isNew);
    Tcl_IncrRefCount(valueObjPtr);
    if (!isNew) {
	Tcl_Obj *oldValueObjPtr;

	/* An element by the same name already exists. Decrement the reference
	 * count of the old value. */

	oldValueObjPtr = Blt_GetHashValue(hPtr);
	if (oldValueObjPtr != NULL) {
	    Tcl_DecrRefCount(oldValueObjPtr);
	}
    }
    Blt_SetHashValue(hPtr, valueObjPtr);

    /*
     * We don't handle traces on a per array element basis.  Setting any
     * element can fire traces for the value.
     */
    if (!(nodePtr->flags & TREE_TRACE_ACTIVE)) {
	CallTraces(interp, treePtr, nodePtr->corePtr, nodePtr, 
		valuePtr->key, flags);
    }
    return TCL_OK;
}

int
Blt_Tree_UnsetArrayValue(Tcl_Interp *interp, Tree *treePtr, Node *nodePtr,
			 const char *arrayName, const char *elemName)
{
    Blt_TreeKey key;		/* Name of field in node. */
    Blt_HashEntry *hPtr;
    Blt_HashTable *tablePtr;
    Tcl_Obj *valueObjPtr;
    Value *valuePtr;

    key = Blt_Tree_GetKey(treePtr, arrayName);
    valuePtr = TreeFindValue(nodePtr, key);
    if (valuePtr == NULL) {
	return TCL_OK;
    }
    if ((valuePtr->owner != NULL) && (valuePtr->owner != treePtr)) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "can't unset private field \"", 
			     key, "\"", (char *)NULL);
	}
	return TCL_ERROR;
    }
    if (Tcl_IsShared(valuePtr->objPtr)) {
	Tcl_DecrRefCount(valuePtr->objPtr);
	valuePtr->objPtr = Tcl_DuplicateObj(valuePtr->objPtr);
	Tcl_IncrRefCount(valuePtr->objPtr);
    }
    if (Blt_GetArrayFromObj(interp, valuePtr->objPtr, &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    hPtr = Blt_FindHashEntry(tablePtr, elemName);
    if (hPtr == NULL) {
	return TCL_OK;		/* Element doesn't exist, Ok. */
    }
    valueObjPtr = Blt_GetHashValue(hPtr);
    Tcl_DecrRefCount(valueObjPtr);
    Blt_DeleteHashEntry(tablePtr, hPtr);

    /*
     * Un-setting any element in the array can cause the trace on the value to
     * fire.
     */
    if (!(nodePtr->flags & TREE_TRACE_ACTIVE)) {
	CallTraces(interp, treePtr, nodePtr->corePtr, nodePtr, 
		valuePtr->key, TREE_TRACE_WRITE);
    }
    return TCL_OK;
}

int
Blt_Tree_ArrayNames(Tcl_Interp *interp, Tree *treePtr, Node *nodePtr,
		    const char *arrayName, Tcl_Obj *listObjPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Blt_HashTable *tablePtr;
    Value *valuePtr;
    const char *key;

    key = Blt_Tree_GetKey(treePtr, arrayName);
    valuePtr = GetTreeValue(interp, treePtr, nodePtr, key);
    if (valuePtr == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_IsShared(valuePtr->objPtr)) {
	Tcl_DecrRefCount(valuePtr->objPtr);
	valuePtr->objPtr = Tcl_DuplicateObj(valuePtr->objPtr);
	Tcl_IncrRefCount(valuePtr->objPtr);
    }
    if (Blt_GetArrayFromObj(interp, valuePtr->objPtr, &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    tablePtr = (Blt_HashTable *)valuePtr->objPtr;
    for (hPtr = Blt_FirstHashEntry(tablePtr, &cursor); hPtr != NULL; 
	 hPtr = Blt_NextHashEntry(&cursor)) {
	Tcl_Obj *objPtr;

	objPtr = Tcl_NewStringObj(Blt_GetHashKey(tablePtr, hPtr), -1);
	Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    return TCL_OK;
}

void
Blt_Tree_NewTagTable(Tree *treePtr)
{
    Blt_TreeTagTable *tagTablePtr;

    if (treePtr->tagTablePtr != NULL) {
	ReleaseTagTable(treePtr->tagTablePtr);
    }
    /* By default, use own sets of tags. */
    tagTablePtr = Blt_AssertMalloc(sizeof(Blt_TreeTagTable));
    tagTablePtr->refCount = 1;
    Blt_InitHashTable(&tagTablePtr->tagTable, BLT_STRING_KEYS);
    treePtr->tagTablePtr = tagTablePtr;
}

int
Blt_Tree_TagTableIsShared(Tree *treePtr)
{
    return (treePtr->tagTablePtr->refCount > 1);
}   

void
Blt_Tree_ClearTags(Tree *treePtr, Node *nodePtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    
    for (hPtr = Blt_FirstHashEntry(&treePtr->tagTablePtr->tagTable, &cursor); 
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	Blt_TreeTagEntry *tePtr;
	Blt_HashEntry *h2Ptr;

	tePtr = Blt_GetHashValue(hPtr);
	h2Ptr = Blt_FindHashEntry(&tePtr->nodeTable, (char *)nodePtr);
	if (h2Ptr != NULL) {
	    Blt_DeleteHashEntry(&tePtr->nodeTable, h2Ptr);
	}
    }
}

Blt_TreeTagEntry *
Blt_Tree_RememberTag(Tree *treePtr, const char *tagName)
{
    int isNew;
    Blt_HashEntry *hPtr;
    Blt_TreeTagEntry *tePtr;
    Blt_HashTable *tablePtr;

    tablePtr = &treePtr->tagTablePtr->tagTable;
    hPtr = Blt_CreateHashEntry(tablePtr, tagName, &isNew);
    if (isNew) {
	tePtr = Blt_AssertMalloc(sizeof(Blt_TreeTagEntry));
	Blt_InitHashTable(&tePtr->nodeTable, BLT_ONE_WORD_KEYS);
	Blt_SetHashValue(hPtr, tePtr);
	tePtr->hashPtr = hPtr;
	tePtr->tagName = Blt_GetHashKey(tablePtr, hPtr);
    } else {
	tePtr = Blt_GetHashValue(hPtr);
    }
    return tePtr;
}

void
Blt_Tree_ForgetTag(Tree *treePtr, const char *tagName)
{
    Blt_HashEntry *hPtr;
    Blt_TreeTagEntry *tePtr;

    if ((strcmp(tagName, "all") == 0) || (strcmp(tagName, "root") == 0)) {
	return;
    }
	
    hPtr = Blt_FindHashEntry(&treePtr->tagTablePtr->tagTable, tagName);
    if (hPtr == NULL) {
	return;
    }
    tePtr = Blt_GetHashValue(hPtr);
    Blt_DeleteHashTable(&tePtr->nodeTable);
    Blt_Free(tePtr);
    Blt_DeleteHashEntry(&treePtr->tagTablePtr->tagTable, hPtr);
}

int
Blt_Tree_HasTag(
    Tree *treePtr,
    Node *nodePtr,
    const char *tagName)
{
    Blt_HashEntry *hPtr;
    Blt_TreeTagEntry *tePtr;

    if (strcmp(tagName, "all") == 0) {
	return TRUE;
    }
    if ((strcmp(tagName, "root") == 0) && 
	(nodePtr == Blt_Tree_RootNode(treePtr))) {
	return TRUE;
    }
    hPtr = Blt_FindHashEntry(&treePtr->tagTablePtr->tagTable, tagName);
    if (hPtr == NULL) {
	return FALSE;
    }
    tePtr = Blt_GetHashValue(hPtr);
    hPtr = Blt_FindHashEntry(&tePtr->nodeTable, (char *)nodePtr);
    if (hPtr == NULL) {
	return FALSE;
    }
    return TRUE;
}

void
Blt_Tree_AddTag(Tree *treePtr, Node *nodePtr, const char *tagName)
{
    Blt_TreeTagEntry *tePtr;

    if ((strcmp(tagName, "all") == 0) || (strcmp(tagName, "root") == 0)) {
	return;
    }
    tePtr = Blt_Tree_RememberTag(treePtr, tagName);
    if (nodePtr != NULL) {
	Blt_HashEntry *hPtr;
	int isNew;

	hPtr = Blt_CreateHashEntry(&tePtr->nodeTable, (char *)nodePtr, &isNew);
	if (isNew) {
	    Blt_SetHashValue(hPtr, nodePtr);
	}
    }
}

void
Blt_Tree_RemoveTag(Tree *treePtr, Node *nodePtr, const char *tagName)
{
    Blt_HashEntry *hPtr;
    Blt_TreeTagEntry *tePtr;
    
    if (strcmp(tagName, "all") == 0) {
	return;			/* Can't remove tag "all". */
    }
    if ((strcmp(tagName, "root") == 0) && 
	(nodePtr == Blt_Tree_RootNode(treePtr))) {
	return;			/* Can't remove tag "root" from root node. */
    }
    hPtr = Blt_FindHashEntry(&treePtr->tagTablePtr->tagTable, tagName);
    if (hPtr == NULL) {
	return;			/* No such tag. */
    }
    tePtr = Blt_GetHashValue(hPtr);
    hPtr = Blt_FindHashEntry(&tePtr->nodeTable, (char *)nodePtr);
    if (hPtr == NULL) {
	return;			/* Node isn't tagged. */
    }
    Blt_DeleteHashEntry(&tePtr->nodeTable, hPtr);
    return;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_TagHashTable --
 *
 *---------------------------------------------------------------------------
 */
Blt_HashTable *
Blt_Tree_TagHashTable(Tree *treePtr, const char *tagName)
{
    Blt_HashEntry *hPtr;
   
    hPtr = Blt_FindHashEntry(&treePtr->tagTablePtr->tagTable, tagName);
    if (hPtr != NULL) {
	Blt_TreeTagEntry *tePtr;
	
	tePtr = Blt_GetHashValue(hPtr);
	return &tePtr->nodeTable;
    }
    return NULL;
}

Blt_HashEntry *
Blt_Tree_FirstTag(Tree *treePtr, Blt_HashSearch *cursorPtr)
{
    return Blt_FirstHashEntry(&treePtr->tagTablePtr->tagTable, cursorPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_DumpNode --
 *
 *---------------------------------------------------------------------------
 */
void
Blt_Tree_DumpNode(Tree *treePtr, Node *rootPtr, Node *nodePtr, 
		  Tcl_DString *dsPtr)
{
    if (nodePtr == rootPtr) {
	Tcl_DStringAppendElement(dsPtr, "-1");
    } else {
	Tcl_DStringAppendElement(dsPtr, 
		Blt_Tree_NodeIdAscii(nodePtr->parent));
    }	
    Tcl_DStringAppendElement(dsPtr, Blt_Tree_NodeIdAscii(nodePtr));

    Tcl_DStringStartSublist(dsPtr);
    Blt_Tree_NodeRelativePath(rootPtr, nodePtr, NULL, TREE_INCLUDE_ROOT, dsPtr);
    Tcl_DStringEndSublist(dsPtr);

    Tcl_DStringStartSublist(dsPtr);
    {
	Blt_TreeKeyIterator iter;
	Blt_TreeKey key;

	/* Add list of key-value pairs. */
	for (key = Blt_Tree_FirstKey(treePtr, nodePtr, &iter); key != NULL; 
	     key = Blt_Tree_NextKey(treePtr, &iter)) {
	    Tcl_Obj *objPtr;

	    if (Blt_Tree_GetValueByKey((Tcl_Interp *)NULL, treePtr, nodePtr, 
			key, &objPtr) == TCL_OK) {
		Tcl_DStringAppendElement(dsPtr, key);
		Tcl_DStringAppendElement(dsPtr, Tcl_GetString(objPtr));
	    }
	}	    
    }
    Tcl_DStringEndSublist(dsPtr);
    Tcl_DStringStartSublist(dsPtr);
    {
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;

	/* Add list of tags. */
	for (hPtr = Blt_Tree_FirstTag(treePtr, &cursor); hPtr != NULL; 
	     hPtr = Blt_NextHashEntry(&cursor)) {
	    Blt_TreeTagEntry *tePtr;

	    tePtr = Blt_GetHashValue(hPtr);
	    if (Blt_FindHashEntry(&tePtr->nodeTable, (char *)nodePtr) != NULL) {
		Tcl_DStringAppendElement(dsPtr, tePtr->tagName);
	    }
	}
    }
    Tcl_DStringEndSublist(dsPtr);
    Tcl_DStringAppend(dsPtr, "\n", -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_Dump --
 *
 *	Dumps node information recursively from the given tree based starting
 *	at *rootPtr*. The dump information is written to the TCL dynamic
 *	string provided. It the caller's responsibility to initialize and free
 *	the dynamic string.
 *	
 * Results:
 *	Always returns TCL_OK.
 *
 * Side Effects:
 *	Dump information is written to the dynamic string provided.
 *
 *---------------------------------------------------------------------------
 */
int
Blt_Tree_Dump(Tree *treePtr, Node *rootPtr, Tcl_DString *dsPtr)
{
    Node *np;

    for (np = rootPtr; np != NULL; np = Blt_Tree_NextNode(rootPtr, np)) {
	Blt_Tree_DumpNode(treePtr, rootPtr, np, dsPtr);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_DumpToFile --
 *
 *	Dumps node information recursively from the given tree based starting
 *	at *rootPtr*. The dump information is written to the file named. If
 *	the file name starts with an '@', then it is the name of an already
 *	opened channel to be used.
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
int
Blt_Tree_DumpToFile(
    Tcl_Interp *interp,
    Tree *treePtr,
    Node *rootPtr,			/* Root node of subtree. */
    const char *fileName)
{
    Tcl_Channel channel;
    Node *np;
    Tcl_DString ds;
    int closeChannel;
    
    closeChannel = TRUE;
    if ((fileName[0] == '@') && (fileName[1] != '\0')) {
	int mode;
	
	channel = Tcl_GetChannel(interp, fileName+1, &mode);
	if (channel == NULL) {
	    return TCL_ERROR;
	}
	if ((mode & TCL_WRITABLE) == 0) {
	    Tcl_AppendResult(interp, "channel \"", fileName, 
		"\" not opened for writing", (char *)NULL);
	    return TCL_ERROR;
	}
	closeChannel = FALSE;
    } else {
	channel = Tcl_OpenFileChannel(interp, fileName, "w", 0666);
	if (channel == NULL) {
	    return TCL_ERROR;
	}
    }
    Tcl_DStringInit(&ds);
    for (np = rootPtr; np != NULL; np = Blt_Tree_NextNode(rootPtr, np)) {
	int nWritten, length;

	Tcl_DStringSetLength(&ds, 0);
	Blt_Tree_DumpNode(treePtr, rootPtr, np, &ds);
	length = Tcl_DStringLength(&ds);
#if HAVE_UTF
	nWritten = Tcl_WriteChars(channel, Tcl_DStringValue(&ds), length);
#else
	nWritten = Tcl_Write(channel, Tcl_DStringValue(&ds), length);
#endif
	if (nWritten < 0) {
	    Tcl_AppendResult(interp, fileName, ": write error:", 
			     Tcl_PosixError(interp), (char *)NULL);
	    Tcl_DStringFree(&ds);
	    if (closeChannel) {
		Tcl_Close(interp, channel);
	    }
	    return TCL_ERROR;
	}
    }
    Tcl_DStringFree(&ds);
    if (closeChannel) {
	Tcl_Close(interp, channel);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_RestoreFromFile --
 *
 *	Restores nodes to the given tree based upon the dump file
 *	provided. The dump file should have been generated by Blt_Tree_Dump or
 *	Blt_Tree_DumpToFile.  If the file name starts with an '@', then it is
 *	the name of an already opened channel to be used. Nodes are added
 *	relative to the node *rootPtr* as the root of the sub-tree.  Two bit
 *	flags may be set.
 *	
 *	TREE_RESTORE_NO_TAGS	Don't restore tag information.
 *	TREE_RESTORE_OVERWRITE	Look for nodes with the same label.
 *				Overwrite if necessary.
 *
 * Results:
 *	A standard TCL result.  If the restore was successful, TCL_OK is
 *	returned.  Otherwise, TCL_ERROR is returned and an error message is
 *	left in the interpreter result.
 *
 * Side Effects:
 *	New nodes are created in the tree and may possibly generate notify
 *	callbacks.
 *
 *---------------------------------------------------------------------------
 */
int
Blt_Tree_RestoreFromFile(
    Tcl_Interp *interp,
    Tree *treePtr,
    Node *rootPtr,		     /* Root node of branch to be restored. */
    const char *fileName,
    unsigned int flags)
{
    Tcl_Channel channel;
    RestoreInfo restore;
    int argc;
    const char **argv;
    int closeChannel;
    int result;

    closeChannel = TRUE;
    if ((fileName[0] == '@') && (fileName[1] != '\0')) {
	int mode;
	
	channel = Tcl_GetChannel(interp, fileName+1, &mode);
	if (channel == NULL) {
	    return TCL_ERROR;
	}
	if ((mode & TCL_READABLE) == 0) {
	    Tcl_AppendResult(interp, "channel \"", fileName, 
		"\" not opened for reading", (char *)NULL);
	    return TCL_ERROR;
	}
	closeChannel = FALSE;
    } else {
	channel = Tcl_OpenFileChannel(interp, fileName, "r", 0);
	if (channel == NULL) {
	    return TCL_ERROR;		/* Can't open dump file. */
	}
    }
    memset((char *)&restore, 0, sizeof(restore));
    Blt_InitHashTable(&restore.idTable, BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&restore.dataTable, BLT_STRING_KEYS);
    restore.rootPtr = rootPtr;
    restore.flags = flags;
    restore.treePtr = treePtr;

    argv = NULL;
    result = TCL_ERROR;		
    for (;;) {
	result = ReadDumpRecord(interp, channel, &argc, &argv, &restore);
	if (result != TCL_OK) {
	    break;			/* Found error or EOF */
	}
	if (argc == 0) {
	    result = TCL_OK;		/* Do nothing. */
	} else if (argc == 3) {
	    result = RestoreNode3(interp, argc, argv, &restore);
	} else if ((argc == 5) || (argc == 6)) {
	    result = RestoreNode5(interp, argc, argv, &restore);
	} else {
	    Tcl_AppendResult(interp, "line #", Blt_Itoa(restore.nLines), 
		": wrong # elements in restore entry", (char *)NULL);
	    result = TCL_ERROR;
	}
	Blt_Free(argv);
	if (result != TCL_OK) {
	    break;
	}
    } 
    if (closeChannel) {
	Tcl_Close(interp, channel);
    }
    Blt_DeleteHashTable(&restore.idTable);
    Blt_DeleteHashTable(&restore.dataTable);
    if (result == TCL_ERROR) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_Tree_Restore --
 *
 *	Restores nodes to the given tree based upon the dump string.  The dump
 *	string should have been generated by Blt_Tree_Dump.  Nodes are added
 *	relative to the node *rootPtr* as the root of the sub-tree.  Two bit
 *	flags may be set.
 *	
 *	TREE_RESTORE_NO_TAGS	Don't restore tag information.
 *	TREE_RESTORE_OVERWRITE	Look for nodes with the same label.
 *				Overwrite if necessary.
 *
 * Results:
 *	A standard TCL result.  If the restore was successful, TCL_OK is
 *	returned.  Otherwise, TCL_ERROR is returned and an error message is
 *	left in the interpreter result.
 *
 * Side Effects:
 *	New nodes are created in the tree and may possibly generate
 *	notify callbacks.
 *
 *---------------------------------------------------------------------------
 */
int
Blt_Tree_Restore(
    Tcl_Interp *interp,
    Tree *treePtr,
    Node *rootPtr,		/* Root node of branch to be restored. */
    const char *string,
    unsigned int flags)
{
    RestoreInfo restore;
    int result;
	
    memset((char *)&restore, 0, sizeof(restore));
    Blt_InitHashTable(&restore.idTable, BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&restore.dataTable, BLT_STRING_KEYS);
    restore.treePtr = treePtr;
    restore.rootPtr = rootPtr;
    restore.flags = flags;
    result = TCL_ERROR;
    for (;;) {
	const char **argv;
	int argc;

	result = ParseDumpRecord(interp, &string, &argc, &argv, &restore);
	if (result != TCL_OK) {
	    break;		/* Found error or EOF */
	}
	if (argc == 0) {
	    result = TCL_OK; /* Do nothing. */
	} else if (argc == 3) {
	    result = RestoreNode3(interp, argc, argv, &restore);
	} else if ((argc == 5) || (argc == 6)) {
	    result = RestoreNode5(interp, argc, argv, &restore);
	} else {
	    Tcl_AppendResult(interp, "line #", Blt_Itoa(restore.nLines), 
		": wrong # elements in restore entry", (char *)NULL);
	    result = TCL_ERROR;
	}
	Blt_Free(argv);
	if (result != TCL_OK) {
	    break;
	}
    } 
    Blt_DeleteHashTable(&restore.idTable);
    Blt_DeleteHashTable(&restore.dataTable);

    /* result will be TCL_RETURN if successful, TCL_ERROR otherwise. */
    if (result == TCL_ERROR) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

