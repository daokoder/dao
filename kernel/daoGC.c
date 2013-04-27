/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2013, Limin Fu
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include<string.h>
#include<math.h>
#include"daoGC.h"
#include"daoMap.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoNumtype.h"
#include"daoProcess.h"
#include"daoRoutine.h"
#include"daoNamespace.h"
#include"daoVmspace.h"
#include"daoThread.h"
#include"daoValue.h"


#if defined(DEBUG) && defined(UNIX)
#if 0
#define DEBUG_TRACE
#endif
#endif

#ifdef DEBUG_TRACE
#include <execinfo.h>
void print_trace( const char *info )
{
	void  *array[100];
	daoint i, size = backtrace (array, 100);
	char **strings = backtrace_symbols (array, size);
	FILE *debug = fopen( "debug.txt", "w+" );
	fprintf (debug, "===========================================\n");
	fprintf (debug, "Obtained " DAO_INT_FORMAT " stack frames.\n", size);
	printf ("=====================%s======================\n", info);
	printf ("Obtained " DAO_INT_FORMAT " stack frames.\n", size);
	for (i = 0; i < size; i++){
		printf ("%s\n", strings[i]);
		fprintf (debug,"%s\n", strings[i]);
	}
	/* comment this to cause leaking, so that valgrind will print the trace with line numbers */
	//free (strings);
	fflush( debug );
	fclose( debug );
	fflush( stdout );
}
#endif

extern int ObjectProfile[100];
extern int daoCountMBS;
extern int daoCountArray;

#ifdef DAO_GC_PROF
static void DaoGC_PrintProfile( DArray *idleList, DArray *workList )
{
	int i;
#warning "-------------------- DAO_GC_PROF is turned on."
	printf("heap[idle] = %zi;\theap[work] = %zi\n", idleList->bufsize, workList->bufsize );
	printf("=======================================\n");
	//printf( "mbs count = %i\n", daoCountMBS );
	printf( "array count = %i\n", daoCountArray );
	for(i=0; i<100; i++){
		if( ObjectProfile[i] != 0 ){
			printf( "type = %3zi; rest = %5i\n", i, ObjectProfile[i] );
		}
	}
}
#else
#define DaoGC_PrintProfile(x,y)
#endif

#if DEBUG
static void DaoGC_PrintValueInfo( DaoValue *value )
{
	if( value->type == DAO_TYPE ){
		printf( "type: %s %i %p\t", value->xType.name->mbs, value->xType.tid, value );
	}else if( value->type == DAO_CDATA || value->type == DAO_CSTRUCT || value->type == DAO_CTYPE ){
		printf( "cdata: %s\t", value->xCdata.ctype->name->mbs );
	}else if( value->type == DAO_CLASS ){
		printf( "class: %s\t", value->xClass.className->mbs );
	}else if( value->type == DAO_TYPEKERNEL ){
		printf( "tkernal: %s\t", ((DaoTypeKernel*)value)->typer->name );
	}
	printf( "%p %i %i\n", value, value->xGC.refCount, value->xGC.cycRefCount );
}
#else
#define DaoGC_PrintValueInfo( value )
#endif

static int DaoGC_DecRC2( DaoValue *p );
static void DaoGC_CycRefCountDecrements( DaoValue **values, daoint size );
static void DaoGC_CycRefCountIncrements( DaoValue **values, daoint size );
static void DaoGC_RefCountDecrements( DaoValue **values, daoint size );
static void cycRefCountDecrement( DaoValue *value );
static void cycRefCountIncrement( DaoValue *value );
static void cycRefCountDecrements( DArray *values );
static void cycRefCountIncrements( DArray *values );
static void directRefCountDecrement( DaoValue **value );
static void directRefCountDecrements( DArray *values );

static int DaoGC_CycRefCountDecScan( DaoValue *value );
static int DaoGC_CycRefCountIncScan( DaoValue *value );
static int DaoGC_RefCountDecScan( DaoValue *value );

static void DaoGC_PrepareCandidates();

static void DaoCGC_FreeGarbage();
static void DaoCGC_CycRefCountDecScan();
static void DaoCGC_CycRefCountIncScan();
static void DaoCGC_DeregisterModules();
static int  DaoCGC_AliveObjectScan();
static void DaoCGC_RefCountDecScan();
static void DaoCGC_Finish();

static void DaoIGC_FreeGarbage();
static void DaoIGC_CycRefCountDecScan();
static void DaoIGC_DeregisterModules();
static void DaoIGC_CycRefCountIncScan();
static int  DaoIGC_AliveObjectScan();
static void DaoIGC_RefCountDecScan();
static void DaoIGC_Finish();
static void DaoIGC_TryInvoke();

static void DaoGC_Init();

#ifdef DAO_WITH_THREAD
static void DaoCGC_Recycle( void * );
static void DaoCGC_TryBlock();
#endif

DaoMap* DaoMetaTables_Get( DaoValue *object, int insert );
DaoMap* DaoMetaTables_Remove( DaoValue *object );


typedef struct DaoGarbageCollector  DaoGarbageCollector;
struct DaoGarbageCollector
{
	DArray   *idleList;
	DArray   *workList;
	DArray   *idleList2;
	DArray   *workList2;
	DArray   *delayList;
	DArray   *freeList;
	DArray   *auxList;
	DArray   *auxList2;
	DArray   *nsList;
	DArray   *cdataValues;
	DArray   *cdataArrays;
	DArray   *cdataMaps;
	DArray   *temporary;
	void     *scanning;

	uchar_t   finalizing;
	uchar_t   delayMask;
	daoint    gcMin, gcMax;
	daoint    ii, jj, kk;
	daoint    cycle;
	daoint    mdelete;
	short     busy;
	short     locked;
	short     workType;
	short     concurrent;

#ifdef DAO_WITH_THREAD
	DThread   thread;

	DMutex    mutex_idle_list;
	DMutex    mutex_start_gc;
	DMutex    mutex_block_mutator;

	DMutex    data_lock;
	DMutex    generic_lock;

	DCondVar  condv_start_gc;
	DCondVar  condv_block_mutator;
#endif
};
static DaoGarbageCollector gcWorker = { NULL, NULL, NULL };


int DaoGC_Min( int n /*=-1*/ )
{
	int prev = gcWorker.gcMin;
	if( n > 0 ) gcWorker.gcMin = n;
	return prev;
}
int DaoGC_Max( int n /*=-1*/ )
{
	int prev = gcWorker.gcMax;
	if( n > 0 ) gcWorker.gcMax = n;
	return prev;
}

void DaoGC_Init()
{
	if( gcWorker.idleList != NULL ) return;

	gcWorker.idleList = DArray_New(0);
	gcWorker.workList = DArray_New(0);
	gcWorker.idleList2 = DArray_New(0);
	gcWorker.workList2 = DArray_New(0);
	gcWorker.delayList = DArray_New(0);
	gcWorker.freeList = DArray_New(0);
	gcWorker.auxList = DArray_New(0);
	gcWorker.auxList2 = DArray_New(0);
	gcWorker.nsList = DArray_New(0);
	gcWorker.cdataValues = DArray_New(0);
	gcWorker.cdataArrays = DArray_New(0);
	gcWorker.cdataMaps = DArray_New(0);
	gcWorker.temporary = DArray_New(0);
	gcWorker.scanning = NULL;

	gcWorker.delayMask = DAO_VALUE_DELAYGC;
	gcWorker.finalizing = 0;
	gcWorker.cycle = 0;
	gcWorker.mdelete = 0;

	gcWorker.gcMin = 1000;
	gcWorker.gcMax = 100 * gcWorker.gcMin;
	gcWorker.workType = 0;
	gcWorker.ii = 0;
	gcWorker.jj = 0;
	gcWorker.kk = 0;
	gcWorker.busy = 0;
	gcWorker.locked = 0;
	gcWorker.concurrent = 0;
}
void DaoGC_Start()
{
	DaoGC_Init();
}
void DaoCGC_Start()
{
	if( gcWorker.concurrent ) return;
#ifdef DAO_WITH_THREAD
	DThread_Init( & gcWorker.thread );
	DMutex_Init( & gcWorker.data_lock );
	DMutex_Init( & gcWorker.generic_lock );
	DMutex_Init( & gcWorker.mutex_idle_list );
	DMutex_Init( & gcWorker.mutex_start_gc );
	DMutex_Init( & gcWorker.mutex_block_mutator );
	DCondVar_Init( & gcWorker.condv_start_gc );
	DCondVar_Init( & gcWorker.condv_block_mutator );
	DaoIGC_Finish();
	gcWorker.concurrent = 1;
	gcWorker.finalizing = 0;
	gcWorker.cycle = 0;
	DThread_Start( & gcWorker.thread, DaoCGC_Recycle, NULL );
#endif
}
static void DaoValue_Delete( DaoValue *self )
{
#ifdef DAO_GC_PROF
	ObjectProfile[self->type] --;
#endif
	if( self->type == DAO_CDATA ){
		DaoCdata_Delete( (DaoCdata*) self );
	}else if( self->type == DAO_CTYPE ){
		DaoCtype_Delete( (DaoCtype*) self );
	}else if( self->type == DAO_ROUTBODY ){
		DaoRoutineBody_Delete( (DaoRoutineBody*) self );
	}else{
		DaoTypeBase *typer = DaoValue_GetTyper( self );
		typer->Delete( self );
	}
}
static void DaoGC_DeleteSimpleData( DaoValue *value )
{
	if( value == NULL || value->xGC.refCount ) return;
#ifdef DAO_GC_PROF
	if( value->type < DAO_ENUM ) ObjectProfile[value->type] --;
#endif
	switch( value->type ){
	case DAO_NONE :
	case DAO_INTEGER :
	case DAO_FLOAT :
	case DAO_DOUBLE :
	case DAO_COMPLEX :
		dao_free( value );
		break;
	case DAO_LONG :
		DaoLong_Delete( & value->xLong );
		break;
	case DAO_STRING :
		DaoString_Delete( & value->xString );
		break;
	default: break;
	}
}

static int DaoGC_DecRC2( DaoValue *p )
{
	daoint i, n;
	p->xGC.refCount --;
	if( p->xGC.refCount == 0 ){
#ifdef DAO_GC_PROF
		if( p->type < DAO_ENUM ) ObjectProfile[p->type] --;
#endif
		switch( p->xGC.type ){
		case DAO_NONE :
		case DAO_INTEGER :
		case DAO_FLOAT :
		case DAO_DOUBLE :
		case DAO_COMPLEX :
		case DAO_LONG :
		case DAO_STRING :
#if 1
			if( gcWorker.concurrent ){
				DArray_Append( gcWorker.idleList2, p );
			}else{
				DaoGC_DeleteSimpleData( p );
			}
#else
			DaoGC_DeleteSimpleData( p );
#endif
			return 0;
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			DaoArray_ResizeVector( & p->xArray, 0 ); break;
#endif
		case DAO_TUPLE :
			if( p->xTuple.unitype && p->xTuple.unitype->simtype ){
				DaoTuple *tuple = & p->xTuple;
				for(i=0,n=tuple->size; i<n; i++)
					if( tuple->items[i] ) DaoGC_DecRC2( tuple->items[i] );
				tuple->size = 0;
			}
			break;
		case DAO_LIST : // TODO same for map
			if( p->xList.unitype && p->xList.unitype->simtype ){
				DArray *array = & p->xList.items;
				DaoValue **items = array->items.pValue;
				for(i=0,n=array->size; i<n; i++) if( items[i] ) DaoGC_DecRC2( items[i] );
				array->size = 0;
				DArray_Clear( array );
			}
			break;
		default :
			/* No safe way to delete other types of objects here, since they might be
			 * being concurrently scanned by the GC! */
			break;
		}
	}

	/* never push simple data types into GC queue,
	 * because they cannot form cyclic referencing structure: */
	if( p->type < DAO_ENUM ) return 0;
	if( p->xGC.delay ) return 0;
	DArray_Append( gcWorker.idleList, p );
	return 1;
}

void DaoGC_Finish()
{
	if( gcWorker.concurrent ){
#ifdef DAO_WITH_THREAD
		DaoCGC_Finish();
#endif
	}else{
		DaoIGC_Finish();
	}

	DArray_Delete( gcWorker.idleList );
	DArray_Delete( gcWorker.workList );
	DArray_Delete( gcWorker.idleList2 );
	DArray_Delete( gcWorker.workList2 );
	DArray_Delete( gcWorker.delayList );
	DArray_Delete( gcWorker.freeList );
	DArray_Delete( gcWorker.auxList );
	DArray_Delete( gcWorker.auxList2 );
	DArray_Delete( gcWorker.nsList );
	DArray_Delete( gcWorker.cdataValues );
	DArray_Delete( gcWorker.cdataArrays );
	DArray_Delete( gcWorker.cdataMaps );
	DArray_Delete( gcWorker.temporary );
	gcWorker.idleList = NULL;
}

#ifdef DAO_WITH_THREAD
void DaoGC_IncRC( DaoValue *value )
{
#if 0
	if( value == 0x49be70 ) free(123);
#endif
	if( value == NULL ) return;
	if( value->type >= DAO_ENUM ) value->xGC.cycRefCount ++;
	if( gcWorker.concurrent ){
		DMutex_Lock( & gcWorker.mutex_idle_list );
		value->xGC.refCount ++;
		DMutex_Unlock( & gcWorker.mutex_idle_list );
		return;
	}
	value->xGC.refCount ++;
}
void DaoGC_DecRC( DaoValue *value )
{
	if( value == NULL ) return;
	if( gcWorker.concurrent ){
		int bl;
		DMutex_Lock( & gcWorker.mutex_idle_list );
		bl = DaoGC_DecRC2( value );
		DMutex_Unlock( & gcWorker.mutex_idle_list );
		if( bl ) DaoCGC_TryBlock();
		return;
	}
	if( DaoGC_DecRC2( value ) ) DaoIGC_TryInvoke();
}
void DaoGC_ShiftRC( DaoValue *up, DaoValue *down )
{
	if( up == down ) return;
#if 0
	if( up == 0x49be70 ) free(123);
#endif
	if( up && up->type >= DAO_ENUM ) up->xGC.cycRefCount ++;
	if( gcWorker.concurrent ){
		int bl = 0;
		DMutex_Lock( & gcWorker.mutex_idle_list );
		if( up ) up->xGC.refCount ++;
		if( down ) bl = DaoGC_DecRC2( down );
		DMutex_Unlock( & gcWorker.mutex_idle_list );
		if( bl ) DaoCGC_TryBlock();
		return;
	}
	if( up ) up->xGC.refCount ++;
	if( down ){
		if( DaoGC_DecRC2( down ) ) DaoIGC_TryInvoke();
	}
}
void DaoGC_TryInvoke()
{
	if( gcWorker.concurrent ){
		DaoCGC_TryBlock();
		return;
	}
	DaoIGC_TryInvoke();
}
#else

void DaoGC_IncRC( DaoValue *value )
{
	if( value ){
		value->xGC.refCount ++;
		if( value->type >= DAO_ENUM ) value->xGC.cycRefCount ++;
	}
}
void DaoGC_DecRC( DaoValue *value )
{
	if( value ){
		if( DaoGC_DecRC2( value ) ) DaoIGC_TryInvoke();
	}
}
void DaoGC_ShiftRC( DaoValue *up, DaoValue *down )
{
	if( up && up->type >= DAO_ENUM ) up->xGC.cycRefCount ++;
	if( up ) up->xGC.refCount ++;
	if( down ){
		if( DaoGC_DecRC2( down ) ) DaoIGC_TryInvoke();
	}
}
void DaoGC_TryInvoke()
{
	DaoIGC_TryInvoke();
}
#endif

void DaoGC_TryDelete( DaoValue *value )
{
	GC_IncRC( value );
	GC_DecRC( value );
}

static void DaoGC_FreeSimple()
{
	DaoValue *value;
	DArray *workList2 = gcWorker.workList2;
	daoint i, k = 0;
	for(i=0,k=0; i<workList2->size; i++){
		value = workList2->items.pValue[i];
		if( value->xGC.work ) continue;
		value->xGC.work = 1;
		workList2->items.pValue[k++] = value;
	}
	for(i=0; i<k; i++) DaoGC_DeleteSimpleData( workList2->items.pValue[i] );
	workList2->size = 0;
}

#define DAO_FULL_GC_SCAN_CYCLE 16

/* The implementation makes sure the GC fields are modified only by the GC thread! */
void DaoGC_PrepareCandidates()
{
	DaoValue *value;
	DArray *workList = gcWorker.workList;
	DArray *freeList = gcWorker.freeList;
	DArray *delayList = gcWorker.delayList;
	DArray *types = gcWorker.temporary;
	uchar_t cycle = (++gcWorker.cycle) % DAO_FULL_GC_SCAN_CYCLE;
	uchar_t delay = cycle && gcWorker.finalizing == 0 ? DAO_VALUE_DELAYGC : 0;
	daoint i, k = 0;
	int delay2;
	for(i=0; i<freeList->size; ++i) freeList->items.pValue[i]->xGC.work = 1;
	gcWorker.delayMask = delay;
	/* damping to avoid "delay2" changing too dramatically: */
	gcWorker.mdelete = 0.5*gcWorker.mdelete + 0.5*freeList->size;
	delay2 = gcWorker.cycle % (1 + 100 / (1 + gcWorker.mdelete));
	if( gcWorker.finalizing ) delay2 = 0;
	if( delay == 0 ){
		/* push delayed objects into the working list for full GC scan: */
		for(i=0; i<delayList->size; ++i){
			value = delayList->items.pValue[i];
			if( value->xGC.work ) continue;
			value->xGC.delay = 0;
			DArray_Append( workList, value );
		}
		delayList->size = 0;
	}else if( freeList->size ){
		for(i=0,k=0; i<delayList->size; ++i){
			value = delayList->items.pValue[i];
			if( value->xGC.work ) continue;
			delayList->items.pValue[k++] = value;
		}
		delayList->size = k;
	}
	/* Remove possible redundant items: */
	for(i=0,k=0; i<workList->size; ++i){
		value = workList->items.pValue[i];
		if( value->xGC.work | value->xGC.delay ) continue;
		if( (value->xBase.trait & delay) || (delay2 && value->xBase.refCount) ){
			/*
			// for non full scan cycles, delay scanning on objects with DAO_VALUE_DELAYGC trait;
			// and delay scanning on objects with reference count >= 1:
			*/
			value->xGC.delay = 1;
			DArray_Append( delayList, value );
			continue;
		}
		workList->items.pValue[k++] = value;
		value->xGC.cycRefCount = value->xGC.refCount;
		value->xGC.work = 1;
		value->xGC.alive = 0;
	}
#if 0
	printf( "%9i %6i %9i %9i\n", gcWorker.cycle, delay, workList->size, k );
#endif
	workList->size = k;
	types->size = 0;
	for(i=0; i<freeList->size; i++){
		if( freeList->items.pValue[i]->type == DAO_TYPE ){
			DArray_Append( types, freeList->items.pValue[i] ); /* should be freed after cdata; */
			continue;
		}
		DaoValue_Delete( freeList->items.pValue[i] );
	}
	freeList->size = 0;
	for(i=0; i<types->size; ++i) DaoValue_Delete( types->items.pValue[i] );
}

enum DaoGCActions{ DAO_GC_DEC, DAO_GC_INC, DAO_GC_BREAK };

static void DaoGC_LockData()
{
	if( gcWorker.concurrent == 0 ) return;
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & gcWorker.data_lock );
#endif
}
static void DaoGC_UnlockData()
{
	if( gcWorker.concurrent == 0 ) return;
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & gcWorker.data_lock );
#endif
}
int DaoGC_LockArray( DArray *array )
{
	if( gcWorker.concurrent == 0 ) return 0;
	if( array->type != D_VALUE ) return 0;
	array->mutating = 1;
	if( gcWorker.scanning != array ) return 0;
	/* real locking, only if the GC is scanning the array: */
	DaoGC_LockData();
	return 1;
}
void DaoGC_UnlockArray( DArray *array, int locked )
{
	array->mutating = 0;
	if( locked ) DaoGC_UnlockData();
}
int DaoGC_LockMap( DMap *map )
{
	if( gcWorker.concurrent == 0 ) return 0;
	if( map->keytype != D_VALUE && map->valtype != D_VALUE ) return 0;
	map->mutating = 1;
	if( gcWorker.scanning != map ) return 0;
	/* real locking, only if the GC is scanning the map: */
	DaoGC_LockData();
	return 1;
}
void DaoGC_UnlockMap( DMap *map, int locked )
{
	map->mutating = 0;
	if( locked ) DaoGC_UnlockData();
}
static void DaoGC_ScanArray( DArray *array, int action )
{
	if( array == NULL || array->size == 0 ) return;
	if( array->type != 0 && array->type != D_VALUE ) return;
	switch( action ){
	case DAO_GC_DEC : cycRefCountDecrements( array ); break;
	case DAO_GC_INC : cycRefCountIncrements( array ); break;
	case DAO_GC_BREAK : directRefCountDecrements( array ); array->size = 0; break;
	}
}
static void DaoGC_ScanValue( DaoValue **value, int action )
{
	switch( action ){
	case DAO_GC_DEC : cycRefCountDecrement( *value ); break;
	case DAO_GC_INC : cycRefCountIncrement( *value ); break;
	case DAO_GC_BREAK : directRefCountDecrement( value ); break;
	}
}
static int DaoGC_ScanMap( DMap *map, int action, int gckey, int gcvalue )
{
	int count = 0;
	DNode *it;
	if( map == NULL || map->size == 0 ) return 0;
	gckey &= map->keytype == 0 || map->keytype == D_VALUE;
	gcvalue &= map->valtype == 0 || map->valtype == D_VALUE;
	if( action != DAO_GC_BREAK ){ /* if action == DAO_GC_BREAK, no mutator can access this map: */
		gcWorker.scanning = map;
		while( map->mutating );
		DaoGC_LockData();
	}
	for(it = DMap_First( map ); it != NULL; it = DMap_Next( map, it ) ){
		if( gckey ) DaoGC_ScanValue( & it->key.pValue, action );
		if( gcvalue ) DaoGC_ScanValue( & it->value.pValue, action );
		count += gckey + gcvalue;
	}
	if( action == DAO_GC_BREAK ){
		if( map->keytype == D_VALUE ) map->keytype = 0;
		if( map->valtype == D_VALUE ) map->valtype = 0;
		DMap_Clear( map );
	}else{
		DaoGC_UnlockData();
		gcWorker.scanning = NULL;
	}
	return count;
}
static void DaoGC_ScanCdata( DaoCdata *cdata, int action )
{
	DaoTypeBase *typer = cdata->ctype ? cdata->ctype->typer : NULL;
	DArray *cvalues = gcWorker.cdataValues;
	DArray *carrays = gcWorker.cdataArrays;
	DArray *cmaps = gcWorker.cdataMaps;
	daoint i, n;

	if( cdata->type == DAO_CTYPE || cdata->subtype == DAO_CDATA_PTR ) return;
	if( typer == NULL || typer->GetGCFields == NULL ) return;
	cvalues->size = carrays->size = cmaps->size = 0;
	if( cdata->type == DAO_CSTRUCT ){
		typer->GetGCFields( cdata, cvalues, carrays, cmaps, action == DAO_GC_BREAK );
	}else if( cdata->data ){
		typer->GetGCFields( cdata->data, cvalues, carrays, cmaps, action == DAO_GC_BREAK );
	}else{
		return;
	}
	DaoGC_ScanArray( cvalues, action );
	for(i=0,n=carrays->size; i<n; i++) DaoGC_ScanArray( carrays->items.pArray[i], action );
	for(i=0,n=cmaps->size; i<n; i++) DaoGC_ScanMap( cmaps->items.pMap[i], action, 1, 1 );
}

#ifdef DAO_WITH_THREAD

void GC_Lock()
{
	if( gcWorker.concurrent ) DMutex_Lock( & gcWorker.generic_lock );
}
void GC_Unlock()
{
	if( gcWorker.concurrent ) DMutex_Unlock( & gcWorker.generic_lock );
}

/* Concurrent Garbage Collector */


void DaoCGC_Finish()
{
	gcWorker.gcMin = 0;
	gcWorker.finalizing = 1;
	DThread_Join( & gcWorker.thread );

	DThread_Destroy( & gcWorker.thread );
	DMutex_Destroy( & gcWorker.data_lock );
	DMutex_Destroy( & gcWorker.generic_lock );
	DMutex_Destroy( & gcWorker.mutex_idle_list );
	DMutex_Destroy( & gcWorker.mutex_start_gc );
	DMutex_Destroy( & gcWorker.mutex_block_mutator );
	DCondVar_Destroy( & gcWorker.condv_start_gc );
	DCondVar_Destroy( & gcWorker.condv_block_mutator );
}
void DaoCGC_TryBlock()
{
	if( gcWorker.idleList->size >= gcWorker.gcMax ){
		DThreadData *thdData = DThread_GetSpecific();
		if( thdData && ! ( thdData->state & DTHREAD_NO_PAUSE ) ){
			DMutex_Lock( & gcWorker.mutex_block_mutator );
			DCondVar_TimedWait( & gcWorker.condv_block_mutator, & gcWorker.mutex_block_mutator, 0.001 );
			DMutex_Unlock( & gcWorker.mutex_block_mutator );
		}
	}
}


void DaoCGC_Recycle( void *p )
{
	DArray *works = gcWorker.workList;
	DArray *idles = gcWorker.idleList;
	DArray *works2 = gcWorker.workList2;
	DArray *idles2 = gcWorker.idleList2;
	DArray *frees = gcWorker.freeList;
	DArray *delays = gcWorker.delayList;
	daoint N;
	while(1){
		N = idles->size + works->size + idles2->size + works2->size + frees->size + delays->size;
		if( gcWorker.finalizing && N == 0 ) break;
		gcWorker.busy = 0;
		while( ! gcWorker.finalizing && (idles->size + idles->size) < gcWorker.gcMin ){
			daoint gcount = idles->size + idles2->size;
			double wtime = 3.0 * gcount / (double)gcWorker.gcMin;
			wtime = 0.01 * exp( - wtime * wtime );
			DMutex_Lock( & gcWorker.mutex_start_gc );
			DCondVar_TimedWait( & gcWorker.condv_start_gc, & gcWorker.mutex_start_gc, wtime );
			DMutex_Unlock( & gcWorker.mutex_start_gc );
			if( idles2->size > 10 ){
				DMutex_Lock( & gcWorker.mutex_idle_list );
				DArray_Swap( idles2, works2 );
				DMutex_Unlock( & gcWorker.mutex_idle_list );
				DaoGC_FreeSimple();
			}
		}
		gcWorker.busy = 1;

		DMutex_Lock( & gcWorker.mutex_idle_list );
		DArray_Swap( idles, works );
		DArray_Swap( idles2, works2 );
		DMutex_Unlock( & gcWorker.mutex_idle_list );
		DaoGC_FreeSimple();

		gcWorker.kk = 0;
		DaoGC_PrepareCandidates();
		DaoCGC_CycRefCountDecScan();
		DaoCGC_CycRefCountIncScan();
		DaoCGC_DeregisterModules();
		DaoCGC_CycRefCountIncScan();
		DaoCGC_RefCountDecScan();
		DaoCGC_FreeGarbage();
	}
	DThread_Exit( & gcWorker.thread );
}
void DaoCGC_CycRefCountDecScan()
{
	DArray *workList = gcWorker.workList;
	uchar_t delay = gcWorker.delayMask;
	daoint i, k;

	for(i=0; i<workList->size; i++){
		DaoValue *value = workList->items.pValue[i];
		if( (value->xBase.trait & delay) | value->xGC.delay ) continue;
		if( value->xBase.trait & DAO_VALUE_WIMETA ){
			cycRefCountDecrement( (DaoValue*) DaoMetaTables_Get( value, 0 ) );
		}
		DaoGC_CycRefCountDecScan( value );
	}
}
void DaoCGC_DeregisterModules()
{
	daoint i;
	DArray *workList = gcWorker.workList;

	for( i=0; i<workList->size; i++ ){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.alive ) continue;
		if( value->type == DAO_NAMESPACE ){
			DaoNamespace *NS= (DaoNamespace*) value;
			DaoVmSpace_Lock( NS->vmSpace );
			if( NS->cycRefCount == 0 ) DMap_Erase( NS->vmSpace->nsModules, NS->name );
			DaoVmSpace_Unlock( NS->vmSpace );
		}
	}
}
void DaoCGC_CycRefCountIncScan()
{
	daoint i;
	DArray *workList = gcWorker.workList;
	DArray *auxList = gcWorker.auxList;

#if 0
	if( gcWorker.finalizing ){
		for( i=0; i<workList->size; i++ ){
			DaoValue *value = workList->items.pValue[i];
			if( value->xGC.cycRefCount >0 ) DaoGC_PrintValueInfo( value );
		}
	}
#endif
	for( i=0; i<workList->size; i++ ){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.alive ) continue;
		if( value->xGC.cycRefCount >0 ){
			auxList->size = 0;
			value->xGC.alive = 1;
			DArray_Append( auxList, value );
			DaoCGC_AliveObjectScan();
		}
	}
}
int DaoCGC_AliveObjectScan()
{
	DArray *auxList = gcWorker.auxList;
	uchar_t delay = gcWorker.delayMask;
	daoint i, k;

	for( i=0; i<auxList->size; i++){
		DaoValue *value = auxList->items.pValue[i];
		if( (value->xBase.trait & delay) | value->xGC.delay ) continue;
		if( value->xBase.trait & DAO_VALUE_WIMETA ){
			cycRefCountIncrement( (DaoValue*) DaoMetaTables_Get( value, 0 ) );
		}
		DaoGC_CycRefCountIncScan( value );
	}
	return auxList->size;
}

void DaoCGC_RefCountDecScan()
{
	DArray *workList = gcWorker.workList;
	uchar_t delay = gcWorker.delayMask;
	daoint i, k;

	for( i=0; i<workList->size; i++ ){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.cycRefCount && value->xGC.refCount ) continue;
		if( (value->xBase.trait & delay) | value->xGC.delay ) continue;
		if( value->xBase.trait & DAO_VALUE_WIMETA ){
			DaoMap *table = DaoMetaTables_Remove( value );
			if( table ) table->refCount --;
		}

		DMutex_Lock( & gcWorker.mutex_idle_list );
		DaoGC_RefCountDecScan( value );
		DMutex_Unlock( & gcWorker.mutex_idle_list );
	}
}
static void DaoCGC_FreeGarbage()
{
	DArray *idleList = gcWorker.idleList;
	DArray *workList = gcWorker.workList;
	uchar_t delay = gcWorker.delayMask;
	daoint i, n = 0;

	for(i=0; i<gcWorker.auxList2->size; i++) gcWorker.auxList2->items.pValue[i]->xGC.alive = 0;
	gcWorker.auxList2->size = 0;

	for(i=0; i<workList->size; i++){
		DaoValue *value = workList->items.pValue[i];
		value->xGC.work = value->xGC.alive = 0;
		if( value->xGC.cycRefCount && value->xGC.refCount ) continue;
		if( value->xGC.refCount !=0 ){
			printf(" refCount not zero %i: %i\n", value->type, value->xGC.refCount );
			DaoGC_PrintValueInfo( value );

			value->xGC.delay = 1;
			DArray_Append( gcWorker.delayList, value );
			continue;
		}
		DArray_Append( gcWorker.freeList, value );
	}
	DaoGC_PrintProfile( idleList, workList );
	workList->size = 0;
}

#else

void GC_Lock(){}
void GC_Unlock(){}

#endif

/* Incremental Garbage Collector */
enum DaoGCWorkType
{
	GC_RESET_RC ,
	GC_DEC_RC ,
	GC_INC_RC ,
	GC_DEREG ,
	GC_INC_RC2 ,
	GC_DIR_DEC_RC ,
	GC_FREE
};

static void DaoIGC_Switch();
static void DaoIGC_Continue();
static void DaoIGC_RefCountDecScan();

static int counts = 1000;
static void DaoIGC_TryInvoke()
{
	if( gcWorker.busy ) return;
	if( -- counts ) return;
	if( gcWorker.idleList->size < gcWorker.gcMax ){
		counts = 1000;
	}else{
		counts = 100;
	}

	if( gcWorker.workList->size )
		DaoIGC_Continue();
	else if( gcWorker.idleList->size > gcWorker.gcMin )
		DaoIGC_Switch();
}

void DaoIGC_Switch()
{
	if( gcWorker.busy ) return;
	DArray_Swap( gcWorker.idleList, gcWorker.workList );
	gcWorker.workType = 0;
	gcWorker.ii = 0;
	gcWorker.jj = 0;
	gcWorker.kk = 0;
	DaoIGC_Continue();
}
void DaoIGC_Continue()
{
	if( gcWorker.busy ) return;
	gcWorker.busy = 1;
	switch( gcWorker.workType ){
	case GC_RESET_RC :
		DaoGC_PrepareCandidates();
		gcWorker.workType = GC_DEC_RC;
		gcWorker.ii = 0;
		break;
	case GC_DEC_RC :
		DaoIGC_CycRefCountDecScan();
#if 0
		if( gcWorker.finalizing && gcWorker.workType == GC_INC_RC ){
			daoint i;
			for( i=0; i<gcWorker.workList->size; i++ ){
				DaoValue *value = gcWorker.workList->items.pValue[i];
				if( value->xGC.cycRefCount >0 ) DaoGC_PrintValueInfo( value );
			}
		}
#endif
		break;
	case GC_DEREG :
		DaoIGC_DeregisterModules();
		break;
	case GC_INC_RC :
	case GC_INC_RC2 :
		DaoIGC_CycRefCountIncScan();
		break;
	case GC_DIR_DEC_RC :
		DaoIGC_RefCountDecScan();
		break;
	case GC_FREE :
		DaoIGC_FreeGarbage();
		break;
	default : break;
	}
	gcWorker.busy = 0;
}
void DaoIGC_Finish()
{
	DArray *works = gcWorker.workList;
	DArray *idles = gcWorker.idleList;
	DArray *works2 = gcWorker.workList2;
	DArray *idles2 = gcWorker.idleList2;
	DArray *frees = gcWorker.freeList;
	DArray *delays = gcWorker.delayList;
	gcWorker.finalizing = 1;
	while( idles->size + works->size + idles2->size + works2->size + frees->size + delays->size ){
		while( works->size ) DaoIGC_Continue();
		DaoIGC_Switch();
	}
}
void DaoIGC_CycRefCountDecScan()
{
	DArray *workList = gcWorker.workList;
	uchar_t delay = gcWorker.delayMask;
	daoint min = workList->size >> 2;
	daoint i = gcWorker.ii;
	daoint j = 0, k;

	if( min < gcWorker.gcMin ) min = gcWorker.gcMin;
	for( ; i<workList->size; i++ ){
		DaoValue *value = workList->items.pValue[i];
		if( (value->xBase.trait & delay) | value->xGC.delay ) continue;
		if( value->xBase.trait & DAO_VALUE_WIMETA ){
			cycRefCountDecrement( (DaoValue*) DaoMetaTables_Get( value, 0 ) );
		}
		j += DaoGC_CycRefCountDecScan( value );
		if( (++j) >= min ) break;
	}
	if( i >= workList->size ){
		gcWorker.ii = 0;
		gcWorker.workType = GC_INC_RC;
	}else{
		gcWorker.ii = i+1;
	}
}
void DaoIGC_DeregisterModules()
{
	DArray *workList = gcWorker.workList;
	daoint min = workList->size >> 2;
	daoint i = gcWorker.ii;
	daoint k = 0;

	if( min < gcWorker.gcMin ) min = gcWorker.gcMin;

	for( ; i<workList->size; i++ ){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.alive ) continue;
		if( value->type == DAO_NAMESPACE ){
			DaoNamespace *NS= (DaoNamespace*) value;
			DaoVmSpace_Lock( NS->vmSpace );
			if( NS->cycRefCount == 0 ) DMap_Erase( NS->vmSpace->nsModules, NS->name );
			DaoVmSpace_Unlock( NS->vmSpace );
		}
	}
	if( i >= workList->size ){
		gcWorker.ii = 0;
		gcWorker.workType ++;
	}else{
		gcWorker.ii = i+1;
	}
}
void DaoIGC_CycRefCountIncScan()
{
	DArray *workList = gcWorker.workList;
	DArray *auxList = gcWorker.auxList;
	daoint min = workList->size >> 2;
	daoint i = gcWorker.ii;
	daoint k = 0;

	if( min < gcWorker.gcMin ) min = gcWorker.gcMin;
	if( gcWorker.jj ){
		k += DaoIGC_AliveObjectScan();
		if( gcWorker.jj ) return;
	}

	for( ; i<workList->size; i++ ){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.alive ) continue;
		if( value->xGC.cycRefCount >0 ){
			auxList->size = 0;
			value->xGC.alive = 1;
			DArray_Append( auxList, value );
			k += DaoIGC_AliveObjectScan();
			if( gcWorker.jj || k >= min ) break;
		}
	}
	if( i >= workList->size ){
		gcWorker.ii = 0;
		gcWorker.workType ++;
	}else{
		gcWorker.ii = i+1;
	}
}
int DaoIGC_AliveObjectScan()
{
	daoint i, k = 9;
	daoint j = gcWorker.jj;
	daoint min = gcWorker.workList->size >> 2;
	uchar_t delay = gcWorker.delayMask;
	DArray *auxList = gcWorker.auxList;

	if( min < gcWorker.gcMin ) min = gcWorker.gcMin;
	for( ; j<auxList->size; j++){
		DaoValue *value = auxList->items.pValue[j];
		if( (value->xBase.trait & delay) | value->xGC.delay ) continue;
		if( value->xBase.trait & DAO_VALUE_WIMETA ){
			cycRefCountIncrement( (DaoValue*) DaoMetaTables_Get( value, 0 ) );
		}
		k += DaoGC_CycRefCountIncScan( value );
		if( (++k) >= min ) break;
	}
	if( j >= auxList->size ){
		gcWorker.jj = 0;
	}else{
		gcWorker.jj = j+1;
	}
	return k;
}
void DaoIGC_RefCountDecScan()
{
	DArray *workList = gcWorker.workList;
	uchar_t delay = gcWorker.delayMask;
	daoint min = workList->size >> 2;
	daoint i = gcWorker.ii;
	daoint j = 0, k;

	if( min < gcWorker.gcMin ) min = gcWorker.gcMin;
	for(; i<workList->size; i++, j++){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.cycRefCount && value->xGC.refCount ) continue;
		if( (value->xBase.trait & delay) | value->xGC.delay ) continue;
		if( value->xBase.trait & DAO_VALUE_WIMETA ){
			DaoMap *table = DaoMetaTables_Remove( value );
			if( table ) table->refCount --;
		}
		j += DaoGC_RefCountDecScan( value );
		if( j >= min ) break;
	}
	if( i >= workList->size ){
		gcWorker.ii = 0;
		gcWorker.workType = GC_FREE;
	}else{
		gcWorker.ii = i+1;
	}
}

void DaoIGC_FreeGarbage()
{
	DArray *idleList = gcWorker.idleList;
	DArray *workList = gcWorker.workList;
	daoint min = workList->size >> 2;
	daoint i = gcWorker.ii;
	daoint j = 0;

	if( min < gcWorker.gcMin ) min = gcWorker.gcMin;
	for(; i<workList->size; i++, j++){
		DaoValue *value = workList->items.pValue[i];
		value->xGC.work = value->xGC.alive = 0;
		if( value->xGC.cycRefCount && value->xGC.refCount ) continue;
		if( value->xGC.refCount !=0 ){
			printf(" refCount not zero %p %i: %i\n", value, value->type, value->xGC.refCount);
			DaoGC_PrintValueInfo( value );
			DArray_Append( gcWorker.idleList, value );
			continue;
		}
		DArray_Append( gcWorker.freeList, value );
		if( j >= min ) break;
	}
	if( i >= workList->size ){
		gcWorker.ii = 0;
		gcWorker.workType = GC_RESET_RC;
		workList->size = 0;
	}else{
		gcWorker.ii = i+1;
	}
	for(i=0; i<gcWorker.auxList2->size; i++) gcWorker.auxList2->items.pValue[i]->xGC.alive = 0;
	gcWorker.auxList2->size = 0;
}
void cycRefCountDecrement( DaoValue *value )
{
#if 0
	if( value == 0x49be70 ) free(123);
#endif
	if( value == NULL ) return;
	/* do not scan simple data types, as they cannot from cyclic structure: */
	if( value->type < DAO_ENUM ) return;
	if( ! value->xGC.work ){
		DArray_Append( gcWorker.workList, value );
		value->xGC.cycRefCount = value->xGC.refCount;
		value->xGC.work = 1;
	}
	value->xGC.cycRefCount --;

	if( value->xGC.cycRefCount<0 ){
#if DEBUG
		printf( "cycRefCount<0 : %2i %p\n", value->type, value );
		DaoGC_PrintValueInfo( value );
#endif
		/*
		 */
		value->xGC.cycRefCount = 0;
	}
}
void cycRefCountIncrement( DaoValue *value )
{
	if( value == NULL ) return;
	/* do not scan simple data types, as they cannot from cyclic structure: */
	if( value->type < DAO_ENUM ) return;
	value->xGC.cycRefCount++;
	if( ! value->xGC.alive ){
		value->xGC.alive = 1;
		DArray_Append( gcWorker.auxList, value );
		DArray_Append( gcWorker.auxList2, value );
	}
}
void DaoGC_CycRefCountDecrements( DaoValue **values, daoint size )
{
	daoint i;
	for(i=0; i<size; i++) cycRefCountDecrement( values[i] );
}
void DaoGC_CycRefCountIncrements( DaoValue **values, daoint size )
{
	daoint i;
	for(i=0; i<size; i++) cycRefCountIncrement( values[i] );
}
void DaoGC_RefCountDecrements( DaoValue **values, daoint size )
{
	daoint i;
	for(i=0; i<size; i++){
		DaoValue *p = values[i];
		if( p == NULL ) continue;
		p->xGC.refCount --;
		if( p->xGC.refCount == 0 && p->type < DAO_ENUM ) DaoGC_DeleteSimpleData( p );
		values[i] = 0;
	}
}
void cycRefCountDecrements( DArray *list )
{
	if( list == NULL ) return;
	gcWorker.scanning = list;
	while( list->mutating );
	DaoGC_LockData();
	DaoGC_CycRefCountDecrements( list->items.pValue, list->size );
	DaoGC_UnlockData();
	gcWorker.scanning = NULL;
}
void cycRefCountIncrements( DArray *list )
{
	if( list == NULL ) return;
	gcWorker.scanning = list;
	while( list->mutating );
	DaoGC_LockData();
	DaoGC_CycRefCountIncrements( list->items.pValue, list->size );
	DaoGC_UnlockData();
	gcWorker.scanning = NULL;
}
void directRefCountDecrement( DaoValue **value )
{
	DaoValue *p = *value;
	if( p == NULL ) return;
	p->xGC.refCount --;
	*value = NULL;
	if( p->xGC.refCount == 0 && p->type < DAO_ENUM ) DaoGC_DeleteSimpleData( p );
}
void directRefCountDecrements( DArray *list )
{
	if( list == NULL ) return;
	DaoGC_RefCountDecrements( list->items.pValue, list->size );
	list->size = 0;
}

static int DaoGC_CycRefCountDecScan( DaoValue *value )
{
	int count = 1;
	switch( value->type ){
	case DAO_ENUM :
		{
			DaoEnum *en = (DaoEnum*) value;
			cycRefCountDecrement( (DaoValue*) en->etype );
			break;
		}
	case DAO_CONSTANT :
		{
			cycRefCountDecrement( value->xConst.value );
			break;
		}
	case DAO_VARIABLE :
		{
			cycRefCountDecrement( value->xVar.value );
			cycRefCountDecrement( (DaoValue*) value->xVar.dtype );
			break;
		}
	case DAO_PAR_NAMED :
		{
			cycRefCountDecrement( value->xNameValue.value );
			cycRefCountDecrement( (DaoValue*) value->xNameValue.unitype );
			break;
		}
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		{
			DaoArray *array = (DaoArray*) value;
			cycRefCountDecrement( (DaoValue*) array->original );
			break;
		}
#endif
	case DAO_TUPLE :
		{
			DaoTuple *tuple = (DaoTuple*) value;
			cycRefCountDecrement( (DaoValue*) tuple->unitype );
			if( tuple->unitype == NULL || tuple->unitype->simtype ==0 ){
				DaoGC_CycRefCountDecrements( tuple->items, tuple->size );
				count += tuple->size;
			}
			break;
		}
	case DAO_LIST :
		{
			DaoList *list = (DaoList*) value;
			cycRefCountDecrement( (DaoValue*) list->unitype );
			if( list->unitype == NULL || list->unitype->simtype ==0 ){
				cycRefCountDecrements( & list->items );
				count += list->items.size;
			}
			break;
		}
	case DAO_MAP :
		{
			DaoMap *map = (DaoMap*) value;
			cycRefCountDecrement( (DaoValue*) map->unitype );
			count += DaoGC_ScanMap( map->items, DAO_GC_DEC, 1, 1 );
			break;
		}
	case DAO_OBJECT :
		{
			DaoObject *obj = (DaoObject*) value;
			if( obj->isRoot ){
				DaoGC_CycRefCountDecrements( obj->objValues, obj->valueCount );
				count += obj->valueCount;
			}
			count += obj->baseCount;
			DaoGC_CycRefCountDecrements( obj->parents, obj->baseCount );
			cycRefCountDecrement( (DaoValue*) obj->rootObject );
			cycRefCountDecrement( (DaoValue*) obj->defClass );
			break;
		}
	case DAO_CSTRUCT : case DAO_CDATA : case DAO_CTYPE :
		{
			DaoCdata *cdata = (DaoCdata*) value;
			cycRefCountDecrement( (DaoValue*) cdata->object );
			cycRefCountDecrement( (DaoValue*) cdata->ctype );
			if( value->type == DAO_CDATA || value->type == DAO_CSTRUCT ){
				DaoGC_ScanCdata( cdata, DAO_GC_DEC );
			}else{
				cycRefCountDecrement( (DaoValue*) value->xCtype.cdtype );
			}
			break;
		}
	case DAO_ROUTINE :
		{
			DaoRoutine *rout = (DaoRoutine*)value;
			cycRefCountDecrement( (DaoValue*) rout->routType );
			cycRefCountDecrement( (DaoValue*) rout->routHost );
			cycRefCountDecrement( (DaoValue*) rout->nameSpace );
			cycRefCountDecrement( (DaoValue*) rout->original );
			cycRefCountDecrement( (DaoValue*) rout->routConsts );
			cycRefCountDecrement( (DaoValue*) rout->body );
			if( rout->overloads ) cycRefCountDecrements( rout->overloads->array );
			if( rout->specialized ) cycRefCountDecrements( rout->specialized->array );
			break;
		}
	case DAO_ROUTBODY :
		{
			DaoRoutineBody *rout = (DaoRoutineBody*)value;
			count += rout->regType->size + rout->abstypes->size;
			cycRefCountDecrements( rout->regType );
			cycRefCountDecrements( rout->svariables );
			DaoGC_ScanMap( rout->abstypes, DAO_GC_DEC, 0, 1 );
			break;
		}
	case DAO_CLASS :
		{
			DaoClass *klass = (DaoClass*)value;
			DaoGC_ScanMap( klass->abstypes, DAO_GC_DEC, 0, 1 );
			cycRefCountDecrement( (DaoValue*) klass->clsType );
			cycRefCountDecrement( (DaoValue*) klass->classRoutine );
			cycRefCountDecrements( klass->constants );
			cycRefCountDecrements( klass->variables );
			cycRefCountDecrements( klass->instvars );
			cycRefCountDecrements( klass->superClass );
			cycRefCountDecrements( klass->references );
			count += klass->constants->size + klass->variables->size + klass->instvars->size;
			count += klass->superClass->size + klass->abstypes->size;
			count += klass->references->size + klass->abstypes->size;
			break;
		}
	case DAO_INTERFACE :
		{
			DaoInterface *inter = (DaoInterface*)value;
			cycRefCountDecrements( inter->supers );
			cycRefCountDecrement( (DaoValue*) inter->abtype );
			cycRefCountDecrement( (DaoValue*) inter->nspace );
			count += DaoGC_ScanMap( inter->methods, DAO_GC_DEC, 0, 1 );
			count += inter->supers->size + inter->methods->size;
			break;
		}
	case DAO_NAMESPACE :
		{
			DaoNamespace *ns = (DaoNamespace*) value;
			cycRefCountDecrements( ns->constants );
			cycRefCountDecrements( ns->variables );
			cycRefCountDecrements( ns->auxData );
			cycRefCountDecrements( ns->mainRoutines );
			count += DaoGC_ScanMap( ns->abstypes, DAO_GC_DEC, 0, 1 );
			count += ns->constants->size + ns->variables->size + ns->abstypes->size;
			break;
		}
	case DAO_TYPE :
		{
			DaoType *abtp = (DaoType*) value;
			cycRefCountDecrement( abtp->aux );
			cycRefCountDecrement( abtp->value );
			cycRefCountDecrement( (DaoValue*) abtp->kernel );
			cycRefCountDecrement( (DaoValue*) abtp->cbtype );
			cycRefCountDecrements( abtp->nested );
			cycRefCountDecrements( abtp->bases );
			count += DaoGC_ScanMap( abtp->interfaces, DAO_GC_DEC, 0, 1 );
			break;
		}
	case DAO_TYPEKERNEL :
		{
			DaoTypeKernel *kernel = (DaoTypeKernel*) value;
			cycRefCountDecrement( (DaoValue*) kernel->abtype );
			cycRefCountDecrement( (DaoValue*) kernel->nspace );
			count += DaoGC_ScanMap( kernel->values, DAO_GC_DEC, 0, 1 );
			count += DaoGC_ScanMap( kernel->methods, DAO_GC_DEC, 0, 1 );
			if( kernel->sptree ){
				cycRefCountDecrements( kernel->sptree->holders );
				cycRefCountDecrements( kernel->sptree->defaults );
				cycRefCountDecrements( kernel->sptree->sptypes );
			}
			break;
		}
	case DAO_FUTURE :
		{
			DaoFuture *future = (DaoFuture*) value;
			cycRefCountDecrement( future->value );
			cycRefCountDecrement( (DaoValue*) future->object );
			cycRefCountDecrement( (DaoValue*) future->unitype );
			cycRefCountDecrement( (DaoValue*) future->routine );
			cycRefCountDecrement( (DaoValue*) future->process );
			cycRefCountDecrement( (DaoValue*) future->precondition );
			cycRefCountDecrement( (DaoValue*) future->sorting );
			DaoGC_CycRefCountDecrements( future->params, future->parCount );
			break;
		}
	case DAO_PROCESS :
		{
			DaoProcess *vmp = (DaoProcess*) value;
			DaoStackFrame *frame = vmp->firstFrame;
			cycRefCountDecrement( (DaoValue*) vmp->abtype );
			cycRefCountDecrement( (DaoValue*) vmp->future );
			cycRefCountDecrements( vmp->exceptions );
			cycRefCountDecrements( vmp->factory );
			DaoGC_CycRefCountDecrements( vmp->stackValues, vmp->stackSize );
			count += vmp->stackSize;
			while( frame ){
				count += 3;
				cycRefCountDecrement( (DaoValue*) frame->routine );
				cycRefCountDecrement( (DaoValue*) frame->object );
				cycRefCountDecrement( (DaoValue*) frame->retype );
				frame = frame->next;
			}
			break;
		}
	default: break;
	}
	return count;
}
static int DaoGC_CycRefCountIncScan( DaoValue *value )
{
	int count = 1;
	switch( value->type ){
	case DAO_ENUM :
		{
			DaoEnum *en = (DaoEnum*) value;
			cycRefCountIncrement( (DaoValue*) en->etype );
			break;
		}
	case DAO_CONSTANT :
		{
			cycRefCountIncrement( value->xConst.value );
			break;
		}
	case DAO_VARIABLE :
		{
			cycRefCountIncrement( value->xVar.value );
			cycRefCountIncrement( (DaoValue*) value->xVar.dtype );
			break;
		}
	case DAO_PAR_NAMED :
		{
			cycRefCountIncrement( value->xNameValue.value );
			cycRefCountIncrement( (DaoValue*) value->xNameValue.unitype );
			break;
		}
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		{
			DaoArray *array = (DaoArray*) value;
			cycRefCountIncrement( (DaoValue*) array->original );
			break;
		}
#endif
	case DAO_TUPLE :
		{
			DaoTuple *tuple= (DaoTuple*) value;
			cycRefCountIncrement( (DaoValue*) tuple->unitype );
			if( tuple->unitype == NULL || tuple->unitype->simtype ==0 ){
				DaoGC_CycRefCountIncrements( tuple->items, tuple->size );
				count += tuple->size;
			}
			break;
		}
	case DAO_LIST :
		{
			DaoList *list= (DaoList*) value;
			cycRefCountIncrement( (DaoValue*) list->unitype );
			if( list->unitype == NULL || list->unitype->simtype ==0 ){
				cycRefCountIncrements( & list->items );
				count += list->items.size;
			}
			break;
		}
	case DAO_MAP :
		{
			DaoMap *map = (DaoMap*)value;
			cycRefCountIncrement( (DaoValue*) map->unitype );
			count += DaoGC_ScanMap( map->items, DAO_GC_INC, 1, 1 );
			break;
		}
	case DAO_OBJECT :
		{
			DaoObject *obj = (DaoObject*) value;
			if( obj->isRoot ){
				DaoGC_CycRefCountIncrements( obj->objValues, obj->valueCount );
				count += obj->valueCount;
			}
			count += obj->baseCount;
			DaoGC_CycRefCountIncrements( obj->parents, obj->baseCount );
			cycRefCountIncrement( (DaoValue*) obj->rootObject );
			cycRefCountIncrement( (DaoValue*) obj->defClass );
			break;
		}
	case DAO_CSTRUCT : case DAO_CDATA : case DAO_CTYPE :
		{
			DaoCdata *cdata = (DaoCdata*) value;
			cycRefCountIncrement( (DaoValue*) cdata->object );
			cycRefCountIncrement( (DaoValue*) cdata->ctype );
			if( value->type == DAO_CDATA || value->type == DAO_CSTRUCT ){
				DaoGC_ScanCdata( cdata, DAO_GC_INC );
			}else{
				cycRefCountIncrement( (DaoValue*) value->xCtype.cdtype );
			}
			break;
		}
	case DAO_ROUTINE :
		{
			DaoRoutine *rout = (DaoRoutine*) value;
			cycRefCountIncrement( (DaoValue*) rout->routType );
			cycRefCountIncrement( (DaoValue*) rout->routHost );
			cycRefCountIncrement( (DaoValue*) rout->nameSpace );
			cycRefCountIncrement( (DaoValue*) rout->original );
			cycRefCountIncrement( (DaoValue*) rout->routConsts );
			cycRefCountIncrement( (DaoValue*) rout->body );
			if( rout->overloads ) cycRefCountIncrements( rout->overloads->array );
			if( rout->specialized ) cycRefCountIncrements( rout->specialized->array );
			break;
		}
	case DAO_ROUTBODY :
		{
			DaoRoutineBody *rout = (DaoRoutineBody*)value;
			count += rout->abstypes->size;
			cycRefCountIncrements( rout->regType );
			cycRefCountIncrements( rout->svariables );
			DaoGC_ScanMap( rout->abstypes, DAO_GC_INC, 0, 1 );
			break;
		}
	case DAO_CLASS :
		{
			DaoClass *klass = (DaoClass*) value;
			DaoGC_ScanMap( klass->abstypes, DAO_GC_INC, 0, 1 );
			cycRefCountIncrement( (DaoValue*) klass->clsType );
			cycRefCountIncrement( (DaoValue*) klass->classRoutine );
			cycRefCountIncrements( klass->constants );
			cycRefCountIncrements( klass->variables );
			cycRefCountIncrements( klass->instvars );
			cycRefCountIncrements( klass->superClass );
			cycRefCountIncrements( klass->references );
			count += klass->constants->size + klass->variables->size + klass->instvars->size;
			count += klass->superClass->size + klass->abstypes->size;
			count += klass->references->size + klass->abstypes->size;
			break;
		}
	case DAO_INTERFACE :
		{
			DaoInterface *inter = (DaoInterface*)value;
			DaoGC_ScanMap( inter->methods, DAO_GC_INC, 0, 1 );
			cycRefCountIncrements( inter->supers );
			cycRefCountIncrement( (DaoValue*) inter->abtype );
			cycRefCountIncrement( (DaoValue*) inter->nspace );
			count += inter->supers->size + inter->methods->size;
			break;
		}
	case DAO_NAMESPACE :
		{
			DaoNamespace *ns = (DaoNamespace*) value;
			cycRefCountIncrements( ns->constants );
			cycRefCountIncrements( ns->variables );
			cycRefCountIncrements( ns->auxData );
			cycRefCountIncrements( ns->mainRoutines );
			count += DaoGC_ScanMap( ns->abstypes, DAO_GC_INC, 0, 1 );
			count += ns->constants->size + ns->variables->size + ns->abstypes->size;
			break;
		}
	case DAO_TYPE :
		{
			DaoType *abtp = (DaoType*) value;
			cycRefCountIncrement( abtp->aux );
			cycRefCountIncrement( abtp->value );
			cycRefCountIncrement( (DaoValue*) abtp->kernel );
			cycRefCountIncrement( (DaoValue*) abtp->cbtype );
			cycRefCountIncrements( abtp->nested );
			cycRefCountIncrements( abtp->bases );
			count += DaoGC_ScanMap( abtp->interfaces, DAO_GC_INC, 0, 1 );
			break;
		}
	case DAO_TYPEKERNEL :
		{
			DaoTypeKernel *kernel = (DaoTypeKernel*) value;
			cycRefCountIncrement( (DaoValue*) kernel->abtype );
			cycRefCountIncrement( (DaoValue*) kernel->nspace );
			count += DaoGC_ScanMap( kernel->values, DAO_GC_INC, 0, 1 );
			count += DaoGC_ScanMap( kernel->methods, DAO_GC_INC, 0, 1 );
			if( kernel->sptree ){
				cycRefCountIncrements( kernel->sptree->holders );
				cycRefCountIncrements( kernel->sptree->defaults );
				cycRefCountIncrements( kernel->sptree->sptypes );
			}
			break;
		}
	case DAO_FUTURE :
		{
			DaoFuture *future = (DaoFuture*) value;
			cycRefCountIncrement( future->value );
			cycRefCountIncrement( (DaoValue*) future->object );
			cycRefCountIncrement( (DaoValue*) future->unitype );
			cycRefCountIncrement( (DaoValue*) future->routine );
			cycRefCountIncrement( (DaoValue*) future->process );
			cycRefCountIncrement( (DaoValue*) future->precondition );
			cycRefCountIncrement( (DaoValue*) future->sorting );
			DaoGC_CycRefCountIncrements( future->params, future->parCount );
			break;
		}
	case DAO_PROCESS :
		{
			DaoProcess *vmp = (DaoProcess*) value;
			DaoStackFrame *frame = vmp->firstFrame;
			cycRefCountIncrement( (DaoValue*) vmp->abtype );
			cycRefCountIncrement( (DaoValue*) vmp->future );
			cycRefCountIncrements( vmp->exceptions );
			cycRefCountIncrements( vmp->factory );
			DaoGC_CycRefCountIncrements( vmp->stackValues, vmp->stackSize );
			count += vmp->stackSize;
			while( frame ){
				count += 3;
				cycRefCountIncrement( (DaoValue*) frame->routine );
				cycRefCountIncrement( (DaoValue*) frame->object );
				cycRefCountIncrement( (DaoValue*) frame->retype );
				frame = frame->next;
			}
			break;
		}
	default: break;
	}
	return count;
}
static int DaoGC_RefCountDecScan( DaoValue *value )
{
	int count = 1;
	switch( value->type ){
	case DAO_ENUM :
		{
			DaoEnum *en = (DaoEnum*) value;
			directRefCountDecrement( (DaoValue**) & en->etype );
			break;
		}
	case DAO_CONSTANT :
		{
			directRefCountDecrement( & value->xConst.value );
			break;
		}
	case DAO_VARIABLE :
		{
			directRefCountDecrement( & value->xVar.value );
			directRefCountDecrement( (DaoValue**) & value->xVar.dtype );
			break;
		}
	case DAO_PAR_NAMED :
		{
			directRefCountDecrement( & value->xNameValue.value );
			directRefCountDecrement( (DaoValue**) & value->xNameValue.unitype );
			break;
		}
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		{
			DaoArray *array = (DaoArray*) value;
			directRefCountDecrement( (DaoValue**) & array->original );
			break;
		}
#endif
	case DAO_TUPLE :
		{
			DaoTuple *tuple = (DaoTuple*) value;
			count += tuple->size;
			directRefCountDecrement( (DaoValue**) & tuple->unitype );
			DaoGC_RefCountDecrements( tuple->items, tuple->size );
			tuple->size = 0;
			break;
		}
	case DAO_LIST :
		{
			DaoList *list = (DaoList*) value;
			count += list->items.size;
			directRefCountDecrements( & list->items );
			directRefCountDecrement( (DaoValue**) & list->unitype );
			break;
		}
	case DAO_MAP :
		{
			DaoMap *map = (DaoMap*) value;
			count += DaoGC_ScanMap( map->items, DAO_GC_BREAK, 1, 1 );
			directRefCountDecrement( (DaoValue**) & map->unitype );
			break;
		}
	case DAO_OBJECT :
		{
			DaoObject *obj = (DaoObject*) value;
			if( obj->isRoot ){
				DaoGC_RefCountDecrements( obj->objValues, obj->valueCount );
				count += obj->valueCount;
			}
			count += obj->baseCount;
			DaoGC_RefCountDecrements( obj->parents, obj->baseCount );
			directRefCountDecrement( (DaoValue**) & obj->rootObject );
			directRefCountDecrement( (DaoValue**) & obj->defClass );
			obj->valueCount = obj->baseCount = 0;
			break;
		}
	case DAO_CSTRUCT : case DAO_CDATA : case DAO_CTYPE :
		{
			DaoValue *value2 = value;
			DaoCdata *cdata = (DaoCdata*) value;
			DaoType *ctype = cdata->ctype;
			directRefCountDecrement( (DaoValue**) & cdata->object );
			directRefCountDecrement( (DaoValue**) & cdata->ctype );
			cdata->ctype = ctype;
			cdata->trait |= DAO_VALUE_BROKEN;
			if( value->type == DAO_CDATA || value->type == DAO_CSTRUCT ){
				DaoGC_ScanCdata( cdata, DAO_GC_BREAK );
			}else{
				directRefCountDecrement( (DaoValue**) & value->xCtype.cdtype );
			}
			break;
		}
	case DAO_ROUTINE :
		{
			DaoRoutine *rout = (DaoRoutine*)value;
			directRefCountDecrement( (DaoValue**) & rout->nameSpace );
			/* may become NULL, if it has already become garbage
			 * in the last cycle */
			directRefCountDecrement( (DaoValue**) & rout->routType );
			/* may become NULL, if it has already become garbage
			 * in the last cycle */
			directRefCountDecrement( (DaoValue**) & rout->routHost );
			directRefCountDecrement( (DaoValue**) & rout->original );
			directRefCountDecrement( (DaoValue**) & rout->routConsts );
			directRefCountDecrement( (DaoValue**) & rout->body );
			if( rout->overloads ) directRefCountDecrements( rout->overloads->array );
			if( rout->specialized ) directRefCountDecrements( rout->specialized->array );
			break;
		}
	case DAO_ROUTBODY :
		{
			DaoRoutineBody *rout = (DaoRoutineBody*)value;
			count += rout->abstypes->size;
			directRefCountDecrements( rout->regType );
			directRefCountDecrements( rout->svariables );
			DaoGC_ScanMap( rout->abstypes, DAO_GC_BREAK, 0, 1 );
			break;
		}
	case DAO_CLASS :
		{
			DaoClass *klass = (DaoClass*)value;
			count += klass->constants->size + klass->variables->size + klass->instvars->size;
			count += klass->superClass->size + klass->abstypes->size;
			count += klass->references->size + klass->abstypes->size;
			count += DaoGC_ScanMap( klass->abstypes, DAO_GC_BREAK, 0, 1 );
			directRefCountDecrement( (DaoValue**) & klass->clsType );
			directRefCountDecrement( (DaoValue**) & klass->classRoutine );
			directRefCountDecrements( klass->constants );
			directRefCountDecrements( klass->variables );
			directRefCountDecrements( klass->instvars );
			directRefCountDecrements( klass->superClass );
			directRefCountDecrements( klass->references );
			break;
		}
	case DAO_INTERFACE :
		{
			DaoInterface *inter = (DaoInterface*)value;
			count += inter->supers->size + inter->methods->size;
			count += DaoGC_ScanMap( inter->methods, DAO_GC_BREAK, 0, 1 );
			directRefCountDecrements( inter->supers );
			directRefCountDecrement( (DaoValue**) & inter->abtype );
			directRefCountDecrement( (DaoValue**) & inter->nspace );
			break;
		}
	case DAO_NAMESPACE :
		{
			DaoNamespace *ns = (DaoNamespace*) value;
			count += ns->constants->size + ns->variables->size + ns->abstypes->size;
			count += DaoGC_ScanMap( ns->abstypes, DAO_GC_BREAK, 0, 1 );
			directRefCountDecrements( ns->constants );
			directRefCountDecrements( ns->variables );
			directRefCountDecrements( ns->auxData );
			directRefCountDecrements( ns->mainRoutines );
			break;
		}
	case DAO_TYPE :
		{
			DaoType *abtp = (DaoType*) value;
			directRefCountDecrements( abtp->nested );
			directRefCountDecrements( abtp->bases );
			directRefCountDecrement( (DaoValue**) & abtp->aux );
			directRefCountDecrement( (DaoValue**) & abtp->value );
			directRefCountDecrement( (DaoValue**) & abtp->kernel );
			directRefCountDecrement( (DaoValue**) & abtp->cbtype );
			count += DaoGC_ScanMap( abtp->interfaces, DAO_GC_BREAK, 0, 1 );
			break;
		}
	case DAO_TYPEKERNEL :
		{
			DaoTypeKernel *kernel = (DaoTypeKernel*) value;
			directRefCountDecrement( (DaoValue**) & kernel->abtype );
			directRefCountDecrement( (DaoValue**) & kernel->nspace );
			count += DaoGC_ScanMap( kernel->values, DAO_GC_BREAK, 0, 1 );
			count += DaoGC_ScanMap( kernel->methods, DAO_GC_BREAK, 0, 1 );
			if( kernel->sptree ){
				directRefCountDecrements( kernel->sptree->holders );
				directRefCountDecrements( kernel->sptree->defaults );
				directRefCountDecrements( kernel->sptree->sptypes );
			}
			break;
		}
	case DAO_FUTURE :
		{
			DaoFuture *future = (DaoFuture*) value;
			directRefCountDecrement( (DaoValue**) & future->value );
			directRefCountDecrement( (DaoValue**) & future->object );
			directRefCountDecrement( (DaoValue**) & future->unitype );
			directRefCountDecrement( (DaoValue**) & future->routine );
			directRefCountDecrement( (DaoValue**) & future->process );
			directRefCountDecrement( (DaoValue**) & future->precondition );
			directRefCountDecrement( (DaoValue**) & future->sorting );
			DaoGC_RefCountDecrements( future->params, future->parCount );
			future->parCount = 0;
			break;
		}
	case DAO_PROCESS :
		{
			DaoProcess *vmp = (DaoProcess*) value;
			DaoStackFrame *frame = vmp->firstFrame;
			directRefCountDecrement( (DaoValue**) & vmp->abtype );
			directRefCountDecrement( (DaoValue**) & vmp->future );
			directRefCountDecrements( vmp->exceptions );
			directRefCountDecrements( vmp->factory );
			DaoGC_RefCountDecrements( vmp->stackValues, vmp->stackSize );
			count += vmp->stackSize;
			vmp->stackSize = 0;
			while( frame ){
				count += 3;
				if( frame->routine ) frame->routine->refCount --;
				if( frame->object ) frame->object->refCount --;
				if( frame->retype ) frame->retype->refCount --;
				frame->routine = NULL;
				frame->object = NULL;
				frame->retype = NULL;
				frame = frame->next;
			}
			break;
		}
	default: break;
	}
	return count;
}
