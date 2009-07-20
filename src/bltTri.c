#include "bltInt.h"
#include "bltChain.h"

#define DELETED		((Edge *)-2)
#define LE 0
#define RE 1

#define Dist(p,q)    \
	hypot((p)->point.x - (q)->point.x, (p)->point.y - (q)->point.y)

typedef struct _HalfEdge HalfEdge;
typedef struct _FreeNode FreeNode;

typedef struct {
    int a, b, c;
    float min, max;
} Triplet;

struct _FreeNode {
    FreeNode *nextPtr;
};

typedef struct {
    FreeNode *headPtr;
    int nodesize;
} FreeList;

typedef struct {
    Point2d point;
    int neighbor;
    int refCount;
} Site;

typedef struct {
    double a, b, c;
    Site *ep[2];
    Site *leftReg, *rightReg;
    int neighbor;
} Edge;

struct _HalfEdge {
    HalfEdge *leftPtr, *rightPtr;
    Edge *edgePtr;
    int refCount;
    int pm;
    Site *vertex;
    double ystar;
    HalfEdge *pqNext;
};

/* Static variables */

typedef struct {
    double xMin, xMax, yMin, yMax, xDelta, yDelta;
    Site *sites;
    int nSites;
    int siteIndex;
    int sqrtNumSites;
    int nVertices;
    Site *bottomsite;
    int nEdges;
    FreeList freeSites;
    FreeList freeEdges;
    FreeList freeHalfEdges;
    HalfEdge *elLeftEnd, *elRightEnd;
    int elHashsize;
    HalfEdge **elHash;
    int pqHashsize;
    HalfEdge *pqHash;
    int pqCount;
    int pqMin;
    Blt_Chain allocChain;
} Voronoi;

static void
FreeInit(FreeList *listPtr, int size)
{
    listPtr->headPtr = NULL;
    listPtr->nodesize = size;
}

static void
InitMemorySystem(Voronoi *v)
{
    if (v->allocChain == NULL) {
	v->allocChain = Blt_Chain_Create();
    }
    FreeInit(&v->freeSites, sizeof(Site));
}

static void *
AllocMemory(Voronoi *v, size_t size)
{
    void *ptr;

    ptr = Blt_Malloc(size);
    if (ptr == NULL) {
	return NULL;
    }
    Blt_Chain_Append(v->allocChain, ptr);
    return ptr;
}
    
static void
ReleaseMemorySystem(Voronoi *v)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_LastLink(v->allocChain); link != NULL;
	link = Blt_Chain_PrevLink(link)) {
	void *ptr;

	ptr = Blt_Chain_GetValue(link);
	if (ptr != NULL) {
	    Blt_Free(ptr);
	}
    }
    Blt_Chain_Destroy(v->allocChain);
    v->allocChain = NULL;
}

INLINE static void
MakeFree(void *item, FreeList *listPtr)
{
    FreeNode *nodePtr = item;

    nodePtr->nextPtr = listPtr->headPtr;
    listPtr->headPtr = nodePtr;
}


static void *
GetFree(Voronoi *v, FreeList *listPtr)
{
    FreeNode *nodePtr;

    if (listPtr->headPtr == NULL) {
	int i;

	nodePtr = AllocMemory(v, v->sqrtNumSites * listPtr->nodesize);
	/* Thread the free nodes as a list */
	for (i = 0; i < v->sqrtNumSites; i++) {
	    MakeFree(((char *)nodePtr + i * listPtr->nodesize), listPtr);
	}
    }
    nodePtr = listPtr->headPtr;
    listPtr->headPtr = listPtr->headPtr->nextPtr;
    return nodePtr;
}

INLINE static void
DecrRefCount(Voronoi *v, Site *vertexPtr)
{
    vertexPtr->refCount--;
    if (vertexPtr->refCount == 0) {
	MakeFree(vertexPtr, &v->freeSites);
    }
}

INLINE static void
IncrRefCount(Site *vertexPtr)
{
    vertexPtr->refCount++;
}

INLINE static HalfEdge *
HECreate(Voronoi *v, Edge *edgePtr, int pm)
{
    HalfEdge *hePtr;

    hePtr = GetFree(v, &v->freeHalfEdges);
    hePtr->edgePtr = edgePtr;
    hePtr->pm = pm;
    hePtr->pqNext = NULL;
    hePtr->vertex = NULL;
    hePtr->refCount = 0;
    return hePtr;
}


static void
ElInitialize(Voronoi *v)
{
    FreeInit(&v->freeHalfEdges, sizeof(HalfEdge));

    v->elHashsize = 2 * v->sqrtNumSites;
    v->elHash = AllocMemory(v, v->elHashsize * sizeof(HalfEdge *));
    assert(v->elHash);
    memset(v->elHash, 0, v->elHashsize * sizeof(HalfEdge *));

    v->elLeftEnd = HECreate(v, (Edge *)NULL, 0);
    v->elRightEnd = HECreate(v, (Edge *)NULL, 0);
    v->elLeftEnd->leftPtr = NULL;
    v->elLeftEnd->rightPtr = v->elRightEnd;
    v->elRightEnd->leftPtr = v->elLeftEnd;
    v->elRightEnd->rightPtr = NULL;
    v->elHash[0] = v->elLeftEnd;
    v->elHash[v->elHashsize - 1] = v->elRightEnd;
}

INLINE static void
ElInsert(HalfEdge *lb, HalfEdge *edgePtr)
{
    edgePtr->leftPtr = lb;
    edgePtr->rightPtr = lb->rightPtr;
    lb->rightPtr->leftPtr = edgePtr;
    lb->rightPtr = edgePtr;
}

static HalfEdge *
ElGetHash(Voronoi *v, int b)
{
    HalfEdge *hePtr;

    if ((b < 0) || (b >= v->elHashsize)) {
	return NULL;
    }
    hePtr = v->elHash[b];
    if ((hePtr == NULL) || (hePtr->edgePtr != DELETED)) {
	return hePtr;
    }
    /* Hash table points to deleted half edge.  Patch as necessary. */

    v->elHash[b] = NULL;
    hePtr->refCount--;
    if (hePtr->refCount == 0) {
	MakeFree(hePtr, &v->freeHalfEdges);
    }
    return NULL;
}

static int
RightOf(HalfEdge *hePtr, Point2d *p)
{
    Edge *e;
    Site *topsite;
    int rightOfSite, above, fast;
    double dxp, dyp, dxs, t1, t2, t3, yl;

    e = hePtr->edgePtr;
    topsite = e->rightReg;
    rightOfSite = p->x > topsite->point.x;
    if ((rightOfSite) && (hePtr->pm == LE)) {
	return 1;
    }
    if ((!rightOfSite) && (hePtr->pm == RE)) {
	return 0;
    }
    if (e->a == 1.0) {
	dyp = p->y - topsite->point.y;
	dxp = p->x - topsite->point.x;
	fast = 0;
	if ((!rightOfSite & e->b < 0.0) | (rightOfSite & e->b >= 0.0)) {
	    above = dyp >= e->b * dxp;
	    fast = above;
	} else {
	    above = p->x + p->y * e->b > e->c;
	    if (e->b < 0.0) {
		above = !above;
	    }
	    if (!above) {
		fast = 1;
	    }
	}
	if (!fast) {
	    dxs = topsite->point.x - (e->leftReg)->point.x;
	    above = e->b * (dxp * dxp - dyp * dyp) <
		dxs * dyp * (1.0 + 2.0 * dxp / dxs + e->b * e->b);
	    if (e->b < 0.0) {
		above = !above;
	    }
	}
    } else {			/* e->b==1.0 */
	yl = e->c - e->a * p->x;
	t1 = p->y - yl;
	t2 = p->x - topsite->point.x;
	t3 = yl - topsite->point.y;
	above = t1 * t1 > t2 * t2 + t3 * t3;
    }
    return (hePtr->pm == LE ? above : !above);
}

static HalfEdge *
ElLeftBnd(Voronoi *v, Point2d *p)
{
    int i, bucket;
    HalfEdge *hePtr;

    /* Use hash table to get close to desired halfedge */

    bucket = (p->x - v->xMin) / v->xDelta * v->elHashsize;
    if (bucket < 0) {
	bucket = 0;
    } else if (bucket >= v->elHashsize) {
	bucket = v->elHashsize - 1;
    }
    hePtr = ElGetHash(v, bucket);
    if (hePtr == NULL) {
	for (i = 1; /* empty */ ; i++) {
	    hePtr = ElGetHash(v, bucket - i);
	    if (hePtr != NULL) {
		break;
	    }
	    hePtr = ElGetHash(v, bucket + i);
	    if (hePtr != NULL) {
		break;
	    }
	}
    }

    /* Now search linear list of halfedges for the correct one */

    if ((hePtr == v->elLeftEnd) || 
	(hePtr != v->elRightEnd && RightOf(hePtr, p))) {
	do {
	    hePtr = hePtr->rightPtr;
	} while ((hePtr != v->elRightEnd) && (RightOf(hePtr, p)));
	hePtr = hePtr->leftPtr;
    } else {
	do {
	    hePtr = hePtr->leftPtr;
	} while ((hePtr != v->elLeftEnd) && (!RightOf(hePtr, p)));
    }

    /* Update hash table and reference counts */

    if ((bucket > 0) && (bucket < (v->elHashsize - 1))) {
	if (v->elHash[bucket] != NULL) {
	    v->elHash[bucket]->refCount--;
	}
	v->elHash[bucket] = hePtr;
	v->elHash[bucket]->refCount++;
    }
    return hePtr;
}

/* 
 * This delete routine can't reclaim node, since pointers from hash table may
 * be present.
 */
INLINE static void
ElDelete(HalfEdge *hePtr)
{
    hePtr->leftPtr->rightPtr = hePtr->rightPtr;
    hePtr->rightPtr->leftPtr = hePtr->leftPtr;
    hePtr->edgePtr = DELETED;
}

INLINE static Site *
LeftRegion(Voronoi *v, HalfEdge *hePtr)
{
    if (hePtr->edgePtr == NULL) {
	return v->bottomsite;
    }
    return (hePtr->pm == LE) ? hePtr->edgePtr->leftReg : hePtr->edgePtr->rightReg;
}

INLINE static Site *
RightRegion(Voronoi *v, HalfEdge *hePtr)
{
    if (hePtr->edgePtr == NULL) {
	return v->bottomsite;
    }
    return (hePtr->pm == LE) ? hePtr->edgePtr->rightReg : hePtr->edgePtr->leftReg;
}

static void
GeomInit(Voronoi *v)
{
    FreeInit(&v->freeEdges, sizeof(Edge));
    v->nVertices = v->nEdges = 0;
    v->sqrtNumSites = sqrt((double)(v->nSites + 4));
    v->yDelta = v->xMax - v->xMax;
    v->xDelta = v->yMax - v->yMin;
}

static Edge *
Bisect(Voronoi *v, Site *s1, Site *s2)
{
    double dx, dy, adx, ady;
    Edge *edgePtr;

    edgePtr = GetFree(v, &v->freeEdges);

    edgePtr->leftReg = s1;
    edgePtr->rightReg = s2;
    IncrRefCount(s1);
    IncrRefCount(s2);
    edgePtr->ep[0] = edgePtr->ep[1] = NULL;

    dx = s2->point.x - s1->point.x;
    dy = s2->point.y - s1->point.y;
    adx = FABS(dx);
    ady = FABS(dy);
    edgePtr->c = (s1->point.x * dx) + (s1->point.y * dy) +
	((dx * dx) + (dy * dy)) * 0.5;
    if (adx > ady) {
	edgePtr->a = 1.0;
	edgePtr->b = dy / dx;
	edgePtr->c /= dx;
    } else {
	edgePtr->b = 1.0;
	edgePtr->a = dx / dy;
	edgePtr->c /= dy;
    }

    edgePtr->neighbor = v->nEdges;
    v->nEdges++;
    return edgePtr;
}

static Site *
Intersect(Voronoi *v, HalfEdge *hePtr1, HalfEdge *hePtr2)
{
    Edge *e1, *e2, *e;
    HalfEdge *hePtr;
    double d, xint, yint;
    int rightOfSite;
    Site *sitePtr;

    e1 = hePtr1->edgePtr;
    e2 = hePtr2->edgePtr;
    if ((e1 == NULL) || (e2 == NULL)) {
	return NULL;
    }
    if (e1->rightReg == e2->rightReg) {
	return NULL;
    }
    d = (e1->a * e2->b) - (e1->b * e2->a);
    if ((-1.0e-10 < d) && (d < 1.0e-10)) {
	return NULL;
    }
    xint = ((e1->c * e2->b) - (e2->c * e1->b)) / d;
    yint = ((e2->c * e1->a) - (e1->c * e2->a)) / d;

    if ((e1->rightReg->point.y < e2->rightReg->point.y) ||
	((e1->rightReg->point.y == e2->rightReg->point.y) &&
	    (e1->rightReg->point.x < e2->rightReg->point.x))) {
	hePtr = hePtr1;
	e = e1;
    } else {
	hePtr = hePtr2;
	e = e2;
    }
    rightOfSite = (xint >= e->rightReg->point.x);
    if ((rightOfSite && hePtr->pm == LE) || (!rightOfSite && hePtr->pm == RE)) {
	return NULL;
    }
    sitePtr = GetFree(v, &v->freeSites);
    sitePtr->refCount = 0;
    sitePtr->point.x = xint;
    sitePtr->point.y = yint;
    return sitePtr;
}

INLINE static void
EndPoint(Voronoi *v, Edge *e, int lr, Site *s)
{
    e->ep[lr] = s;
    IncrRefCount(s);
    if (e->ep[RE - lr] == NULL) {
	return;
    }
    DecrRefCount(v, e->leftReg);
    DecrRefCount(v, e->rightReg);
    MakeFree(e, &v->freeEdges);
}

INLINE static void
MakeVertex(Voronoi *v, Site *vertex)
{
    vertex->neighbor = v->nVertices;
    v->nVertices++;
}

static int
PQBucket(Voronoi *v, HalfEdge *hePtr)
{
    int bucket;

    bucket = (hePtr->ystar - v->yMin) / v->yDelta * v->pqHashsize;
    if (bucket < 0) {
	bucket = 0;
    }
    if (bucket >= v->pqHashsize) {
	bucket = v->pqHashsize - 1;
    }
    if (bucket < v->pqMin) {
	v->pqMin = bucket;
    }
    return bucket;
}

static void
PQInsert(Voronoi *v, HalfEdge *hePtr, Site *vertex, double offset)
{
    HalfEdge *last, *next;

    hePtr->vertex = vertex;
    IncrRefCount(vertex);
    hePtr->ystar = vertex->point.y + offset;
    last = v->pqHash + PQBucket(v, hePtr);
    while (((next = last->pqNext) != NULL) &&
	   ((hePtr->ystar > next->ystar) || 
	    ((hePtr->ystar == next->ystar) &&
	     (vertex->point.x > next->vertex->point.x)))) {
	last = next;
    }
    hePtr->pqNext = last->pqNext;
    last->pqNext = hePtr;
    v->pqCount++;
}

static void
PQDelete(Voronoi *v, HalfEdge *hePtr)
{
    if (hePtr->vertex != NULL) {
	HalfEdge *last;

	last = v->pqHash + PQBucket(v, hePtr);
	while (last->pqNext != hePtr) {
	    last = last->pqNext;
	}
	last->pqNext = hePtr->pqNext;
	v->pqCount--;
	DecrRefCount(v, hePtr->vertex);
	hePtr->vertex = NULL;
    }
}

INLINE static int
PQEmpty(Voronoi *v)
{
    return (v->pqCount == 0);
}

INLINE static Point2d
PQMin(Voronoi *v)
{
    Point2d p;

    while (v->pqHash[v->pqMin].pqNext == NULL) {
	v->pqMin++;
    }
    p.x = v->pqHash[v->pqMin].pqNext->vertex->point.x;
    p.y = v->pqHash[v->pqMin].pqNext->ystar;
    return p;
}

INLINE static HalfEdge *
PQExtractMin(Voronoi *v)
{
    HalfEdge *curr;

    curr = v->pqHash[v->pqMin].pqNext;
    v->pqHash[v->pqMin].pqNext = curr->pqNext;
    v->pqCount--;
    return curr;
}

static void
PQInitialize(Voronoi *v)
{
    size_t nBytes;

    v->pqCount = v->pqMin = 0;
    v->pqHashsize = 4 * v->sqrtNumSites;
    nBytes = v->pqHashsize * sizeof(HalfEdge);
    v->pqHash = AllocMemory(v, nBytes);
    assert(v->pqHash);
    memset(v->pqHash, 0, nBytes);
}

INLINE static Site *
NextSite(Voronoi *v)
{
    if (v->siteIndex < v->nSites) {
	Site *s;

	s = v->sites + v->siteIndex;
	v->siteIndex++;
	return s;
    }
    return NULL;
}

static int
ComputeVoronoi(Voronoi *v, Triplet *triplets)
{
    Site *newsite, *bot, *top, *temp, *p;
    Site *vertex;
    Point2d newintstar;
    int pm, count = 0;
    HalfEdge *lbnd, *rbnd, *llbnd, *rrbnd, *bisector;
    Edge *e;

    PQInitialize(v);
    v->bottomsite = NextSite(v);
    ElInitialize(v);

    newsite = NextSite(v);
    for (;;) {
	if (!PQEmpty(v)) {
	    newintstar = PQMin(v);
	}
	if ((newsite != NULL)
	    && ((PQEmpty(v)) || 
		(newsite->point.y < newintstar.y) ||
		(newsite->point.y == newintstar.y) && 
		(newsite->point.x < newintstar.x))) {

	    /* New site is smallest */

	    lbnd = ElLeftBnd(v, &newsite->point);
	    rbnd = lbnd->rightPtr;
	    bot = RightRegion(v, lbnd);
	    e = Bisect(v, bot, newsite);
	    bisector = HECreate(v, e, LE);
	    ElInsert(lbnd, bisector);
	    p = Intersect(v, lbnd, bisector);
	    if (p != NULL) {
		PQDelete(v, lbnd);
		PQInsert(v, lbnd, p, Dist(p, newsite));
	    }
	    lbnd = bisector;
	    bisector = HECreate(v, e, RE);
	    ElInsert(lbnd, bisector);
	    p = Intersect(v, bisector, rbnd);
	    if (p != NULL) {
		PQInsert(v, bisector, p, Dist(p, newsite));
	    }
	    newsite = NextSite(v);
	} else if (!PQEmpty(v)) {

	    /* Intersection is smallest */

	    lbnd = PQExtractMin(v);
	    llbnd = lbnd->leftPtr;
	    rbnd = lbnd->rightPtr;
	    rrbnd = rbnd->rightPtr;
	    bot = LeftRegion(v, lbnd);
	    top = RightRegion(v, rbnd);
	    triplets[count].a = bot->neighbor;
	    triplets[count].b = top->neighbor;
	    triplets[count].c = RightRegion(v, lbnd)->neighbor;
	    ++count;
	    vertex = lbnd->vertex;
	    MakeVertex(v, vertex);
	    EndPoint(v, lbnd->edgePtr, lbnd->pm, vertex);
	    EndPoint(v, rbnd->edgePtr, rbnd->pm, vertex);
	    ElDelete(lbnd);
	    PQDelete(v, rbnd);
	    ElDelete(rbnd);
	    pm = LE;
	    if (bot->point.y > top->point.y) {
		temp = bot, bot = top, top = temp;
		pm = RE;
	    }
	    e = Bisect(v, bot, top);
	    bisector = HECreate(v, e, pm);
	    ElInsert(llbnd, bisector);
	    EndPoint(v, e, RE - pm, vertex);
	    DecrRefCount(v, vertex);
	    p = Intersect(v, llbnd, bisector);
	    if (p != NULL) {
		PQDelete(v, llbnd);
		PQInsert(v, llbnd, p, Dist(p, bot));
	    }
	    p = Intersect(v, bisector, rrbnd);
	    if (p != NULL) {
		PQInsert(v, bisector, p, Dist(p, bot));
	    }
	} else {
	    break;
	}
    }
    return count;
}

static int
CompareSites(const void *a, const void *b)
{
    const Site *s1 = a;
    const Site *s2 = b;

    if (s1->point.y < s2->point.y) {
	return -1;
    }
    if (s1->point.y > s2->point.y) {
	return 1;
    }
    if (s1->point.x < s2->point.x) {
	return -1;
    }
    if (s1->point.x > s2->point.x) {
	return 1;
    }
    return 0;
}

int
Blt_Triangulate(Tcl_Interp *interp, size_t nPoints, Point2f *points, 
		int sorted, int *nTrianglesPtr, Triplet **trianglesPtr)
{
    int i;
    Site *sp, *send;
    int n;
    Voronoi v;
    Triplet *triangles;
    
    memset(&v, 0, sizeof(v));

    InitMemorySystem(&v);

    v.nSites = nPoints;
    v.sites = AllocMemory(&v, nPoints * sizeof(Site));
    
    for (sp = v.sites, send = sp + nPoints, i = 0; sp < send; i++, sp++) {
	sp->point.x = points[i].x;
	sp->point.y = points[i].y;
	sp->neighbor = i;
	sp->refCount = 0;
    }
    if (!sorted) {
	qsort(v.sites, v.nSites, sizeof(Site), CompareSites);
    }
    sp = v.sites;
    v.xMin = v.xMax = sp->point.x;
    v.yMin = sp->point.y;
    v.yMax = v.sites[nPoints - 1].point.y;
    for (sp++, send = v.sites + nPoints; sp < send; sp++) {
	if (sp->point.x < v.xMin) {
	    v.xMin = sp->point.x;
	} else if (sp->point.x > v.xMax) {
	    v.xMax = sp->point.x;
	}
    }
    v.siteIndex = 0;
    GeomInit(&v);
    n = ComputeVoronoi(&v, triangles);
    /* Release memory allocated for triangulation */
    ReleaseMemorySystem(&v);
    *nTrianglesPtr = n;
    *trianglesPtr = triangles;
    return TCL_OK;
}
