/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#include<string.h>
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
#endif
#define DEBUG_TRACE
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
#define DaoGC_PrintProfile(x,y) (1==1)
#endif

#if DEBUG
static void DaoGC_PrintValueInfo( DaoValue *value )
{
	if( value->type == DAO_TYPE ){
		printf( "%s %i\n", value->xType.name->mbs, value->xType.tid );
	}else if( value->type == DAO_CDATA ){
		printf( "%s\n", value->xCdata.ctype->name->mbs );
	}else if( value->type == DAO_CLASS ){
		printf( "%s\n", value->xClass.className->mbs );
	}else if( value->type == DAO_TYPEKERNEL ){
		printf( "%s\n", ((DaoTypeKernel*)value)->typer->name );
	}
}
#else
#define DaoGC_PrintValueInfo( value )  (value == value)
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

static void DaoCGC_DecRC( DaoValue *p );
static void DaoCGC_IncRC( DaoValue *p );
static void DaoCGC_ShiftRC( DaoValue *up, DaoValue *down );
static void DaoCGC_DecRCs( DArray *list );
static void DaoCGC_IncRCs( DArray *list );
static void DaoCGC_FreeGarbage();
static void DaoCGC_CycRefCountDecScan();
static void DaoCGC_CycRefCountIncScan();
static int  DaoCGC_AliveObjectScan();
static void DaoCGC_RefCountDecScan();
static void DaoCGC_Finish();

static void DaoIGC_DecRC( DaoValue *p );
static void DaoIGC_IncRC( DaoValue *p );
static void DaoIGC_ShiftRC( DaoValue *up, DaoValue *down );
static void DaoIGC_DecRCs( DArray *list );
static void DaoIGC_IncRCs( DArray *list );
static void DaoIGC_FreeGarbage();
static void DaoIGC_CycRefCountDecScan();
static void DaoIGC_CycRefCountIncScan();
static int  DaoIGC_AliveObjectScan();
static void DaoIGC_RefCountDecScan();
static void DaoIGC_Finish();
static void DaoIGC_TryInvoke();

static void DaoGC_Init();

#ifdef DAO_WITH_THREAD
static void DaoCGC_Recycle( void * );
static void DaoCGC_TryInvoke();
#endif

DaoMap* DaoMetaTables_Get( DaoValue *object, int insert );
DaoMap* DaoMetaTables_Remove( DaoValue *object );


typedef struct DaoGarbageCollector  DaoGarbageCollector;
struct DaoGarbageCollector
{
	DArray   *idleList;
	DArray   *workList;
	DArray   *auxList;
	DArray   *auxList2;
	DArray   *nsList;
	DArray   *cdataValues;
	DArray   *cdataArrays;
	DArray   *cdataMaps;

	daoint    gcMin, gcMax;
	daoint    ii, jj, kk;
	int       finalizing; /* set at any time; */
	int       delayMask; /* set at cycle boundary; */
	short     busy;
	short     locked;
	short     workType;
	short     concurrent;

#ifdef DAO_WITH_THREAD
	DThread   thread;

	DMutex    mutex_idle_list;
	DMutex    mutex_start_gc;
	DMutex    mutex_block_mutator;

	DCondVar  condv_start_gc;
	DCondVar  condv_block_mutator;
#endif
};
static DaoGarbageCollector gcWorker = { NULL, NULL, NULL };

static DaoEnum dummyEnum = {0,0,DAO_VALUE_CONST,0,1,1,0,NULL};
static DaoEnum *dummyEnum2 = & dummyEnum;
static DaoValue *dummyValue = NULL;

static uchar_t type_gc_delay[ END_NOT_TYPES ];


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

	dummyValue = (DaoValue*) dummyEnum2;

	gcWorker.idleList = DArray_New(0);
	gcWorker.workList = DArray_New(0);
	gcWorker.auxList = DArray_New(0);
	gcWorker.auxList2 = DArray_New(0);
	gcWorker.nsList = DArray_New(0);
	gcWorker.cdataValues = DArray_New(0);
	gcWorker.cdataArrays = DArray_New(0);
	gcWorker.cdataMaps = DArray_New(0);

	gcWorker.finalizing = 0;
	gcWorker.delayMask = 0xf;

	gcWorker.gcMin = 1000;
	gcWorker.gcMax = 100 * gcWorker.gcMin;
	gcWorker.workType = 0;
	gcWorker.ii = 0;
	gcWorker.jj = 0;
	gcWorker.kk = 0;
	gcWorker.busy = 0;
	gcWorker.locked = 0;
	gcWorker.concurrent = 0;

	memset( type_gc_delay, 0, END_NOT_TYPES*sizeof(uchar_t) );
	type_gc_delay[ DAO_CLASS ] = 1;
	type_gc_delay[ DAO_CTYPE ] = 1;
	type_gc_delay[ DAO_INTERFACE ] = 1;
	type_gc_delay[ DAO_ROUTINE ] = 1;
	type_gc_delay[ DAO_PROCESS ] = 1;
	type_gc_delay[ DAO_NAMESPACE ] = 1;
	type_gc_delay[ DAO_TYPE ] = 1;
	type_gc_delay[ DAO_TYPEKERNEL ] = 1;
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
	DMutex_Init( & gcWorker.mutex_idle_list );
	DMutex_Init( & gcWorker.mutex_start_gc );
	DMutex_Init( & gcWorker.mutex_block_mutator );
	DCondVar_Init( & gcWorker.condv_start_gc );
	DCondVar_Init( & gcWorker.condv_block_mutator );
	DaoIGC_Finish();
	gcWorker.concurrent = 1;
	gcWorker.finalizing = 0;
	gcWorker.delayMask = 0xf;
	DThread_Start( & gcWorker.thread, DaoCGC_Recycle, NULL );
#endif
}
static void DaoValue_Delete( DaoValue *self )
{
	DaoTypeBase *typer = DaoValue_GetTyper( self );
#ifdef DAO_GC_PROF
	ObjectProfile[self->type] --;
#endif
	if( self->type == DAO_CDATA && self->xCdata.subtype != DAO_CDATA_DAO ){
		DaoCdata_Delete( (DaoCdata*) self );
	}else if( self->type == DAO_ROUTBODY ){
		DaoRoutineBody_Delete( (DaoRoutineBody*) self );
	}else{
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
	if( p->xGC.refCount == 1 ){
#ifdef DAO_GC_PROF
		if( p->type < DAO_ENUM ) ObjectProfile[p->type] --;
#endif
		switch( p->xGC.type ){
		case DAO_NONE :
		case DAO_INTEGER :
		case DAO_FLOAT :
		case DAO_DOUBLE :
		case DAO_COMPLEX : dao_free( p ); return 0;
		case DAO_LONG : DaoLong_Delete( & p->xLong ); return 0;
		case DAO_STRING : DaoString_Delete( & p->xString ); return 0;
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY : DaoArray_ResizeVector( & p->xArray, 0 ); break;
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
		default : break;
		/* No safe way to delete other types of objects here, since they might be
		 * being concurrently scanned by the GC! */
		}
	}
	p->xGC.refCount --;

	/* never push simple data types into GC queue,
	 * because they cannot form cyclic referencing structure: */
	if( (p->type < DAO_ENUM) | p->xGC.idle ) return 0;

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
	DArray_Delete( gcWorker.auxList );
	DArray_Delete( gcWorker.auxList2 );
	DArray_Delete( gcWorker.nsList );
	DArray_Delete( gcWorker.cdataValues );
	DArray_Delete( gcWorker.cdataArrays );
	DArray_Delete( gcWorker.cdataMaps );
	gcWorker.idleList = NULL;
}

#ifdef DAO_WITH_THREAD
void DaoGC_IncRC( DaoValue *value )
{
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
		DMutex_Lock( & gcWorker.mutex_idle_list );
		DaoGC_DecRC2( value );
		DMutex_Unlock( & gcWorker.mutex_idle_list );
		DaoCGC_TryInvoke();
		return;
	}
	if( DaoGC_DecRC2( value ) ) DaoIGC_TryInvoke();
}
void DaoGC_ShiftRC( DaoValue *up, DaoValue *down )
{
	if( up == down ) return;
	if( up && up->type >= DAO_ENUM ) up->xGC.cycRefCount ++;
	if( gcWorker.concurrent ){
		DMutex_Lock( & gcWorker.mutex_idle_list );
		if( up ) up->xGC.refCount ++;
		if( down ) DaoGC_DecRC2( down );
		DMutex_Unlock( & gcWorker.mutex_idle_list );
		if( down ) DaoCGC_TryInvoke();
		return;
	}
	if( up ) up->xGC.refCount ++;
	if( down ){
		if( DaoGC_DecRC2( down ) ) DaoIGC_TryInvoke();
	}
}
void DaoGC_IncRCs( DArray *values )
{
	if( gcWorker.concurrent ){
		DaoCGC_IncRCs( values );
		return;
	}
	DaoIGC_IncRCs( values );
}
void DaoGC_DecRCs( DArray *values )
{
	if( gcWorker.concurrent ){
		DaoCGC_DecRCs( values );
		return;
	}
	DaoIGC_DecRCs( values );
}
void DaoGC_TryInvoke()
{
	if( gcWorker.concurrent ){
		DaoCGC_TryInvoke();
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
void DaoGC_IncRCs( DArray *values )
{
	DaoIGC_IncRCs( values );
}
void DaoGC_DecRCs( DArray *values )
{
	DaoIGC_DecRCs( values );
}
void DaoGC_TryInvoke()
{
	DaoIGC_TryInvoke();
}
#endif

/* The implementation makes sure the GC fields are modified only by the GC thread! */
void DaoGC_PrepareCandidates()
{
	DaoValue *value;
	DArray *workList = gcWorker.workList;
	daoint i, k = 0;
	/* Remove possible redundant items: */
	for(i=0; i<workList->size; i++){
		value = workList->items.pValue[i];
		if( value->xGC.work ) continue;
		workList->items.pValue[k++] = value;
		value->xGC.cycRefCount = value->xGC.refCount;
		value->xGC.work = 1;
		value->xGC.idle = 0;
		value->xGC.alive = 0;
		value->xGC.delay += type_gc_delay[value->type];
	}
	workList->size = k;
}
static void DaoIGC_MarkIdleItems()
{
	DArray *idleList = gcWorker.idleList;
	daoint i, n = idleList->size;
	for(i=gcWorker.kk; i<n; i++) idleList->items.pValue[i]->xGC.idle = 1;
	idleList->size = n;
	gcWorker.kk = n;
}

enum DaoGCActions{ DAO_GC_DEC, DAO_GC_INC, DAO_GC_BREAK };

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
	for(it = DMap_First( map ); it != NULL; it = DMap_Next( map, it ) ){
		if( gckey ) DaoGC_ScanValue( & it->key.pValue, action );
		if( gcvalue ) DaoGC_ScanValue( & it->value.pValue, action );
		count += gckey + gcvalue;
	}
	for(it=map->first; it; it=it->next){
		/* for other key/value types, the values in the cached node must
		// must have their reference count handled: */
		if( map->keytype == D_VALUE ) DaoGC_ScanValue( & it->key.pValue, action );
		if( map->valtype == D_VALUE ) DaoGC_ScanValue( & it->value.pValue, action );
		count += (map->keytype == D_VALUE) + (map->valtype == D_VALUE);
	}
	if( action == DAO_GC_BREAK ){
		if( map->keytype == D_VALUE ) map->keytype = 0;
		if( map->valtype == D_VALUE ) map->valtype = 0;
		DMap_Clear( map );
	}
	return count;
}
static void DaoGC_ScanCdata( DaoCdata *cdata, int action )
{
	DArray *cvalues = gcWorker.cdataValues;
	DArray *carrays = gcWorker.cdataArrays;
	DArray *cmaps = gcWorker.cdataMaps;
	daoint i, n;

	if( cdata->type == DAO_CTYPE || cdata->subtype == DAO_CDATA_PTR ) return;
	if( cdata->typer == NULL || cdata->typer->GetGCFields == NULL ) return;
	cvalues->size = carrays->size = cmaps->size = 0;
	if( cdata->subtype == DAO_CDATA_DAO ){
		cdata->typer->GetGCFields( cdata, cvalues, carrays, cmaps, action == DAO_GC_BREAK );
	}else if( cdata->data ){
		cdata->typer->GetGCFields( cdata->data, cvalues, carrays, cmaps, action == DAO_GC_BREAK );
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
	if( gcWorker.concurrent ) DMutex_Lock( & gcWorker.mutex_idle_list );
}
void GC_Unlock()
{
	if( gcWorker.concurrent ) DMutex_Unlock( & gcWorker.mutex_idle_list );
}

/* Concurrent Garbage Collector */

void DaoCGC_DecRC( DaoValue *p )
{
	if( p ){
		DMutex_Lock( & gcWorker.mutex_idle_list );
		DaoGC_DecRC2( p );
		DMutex_Unlock( & gcWorker.mutex_idle_list );
		DaoCGC_TryInvoke();
	}
}
void DaoCGC_IncRC( DaoValue *p )
{
	if( p ){
		DMutex_Lock( & gcWorker.mutex_idle_list );
		p->xGC.refCount ++;
		if( p->type >= DAO_ENUM ) p->xGC.cycRefCount ++;
		DMutex_Unlock( & gcWorker.mutex_idle_list );
	}
}
void DaoCGC_ShiftRC( DaoValue *up, DaoValue *down )
{
	if( up == down ) return;

	DMutex_Lock( & gcWorker.mutex_idle_list );
	if( up ){
		up->xGC.refCount ++;
		if( up->type >= DAO_ENUM ) up->xGC.cycRefCount ++;
	}
	if( down ) DaoGC_DecRC2( down );
	DMutex_Unlock( & gcWorker.mutex_idle_list );

	if( down ) DaoCGC_TryInvoke();
}

void DaoCGC_IncRCs( DArray *list )
{
	daoint i;
	DaoValue **values;

	if( list->size == 0 ) return;
	values = list->items.pValue;
	DMutex_Lock( & gcWorker.mutex_idle_list );
	for( i=0; i<list->size; i++){
		if( values[i] ){
			values[i]->xGC.refCount ++;
			if( values[i]->type >= DAO_ENUM ) values[i]->xGC.cycRefCount ++;
		}
	}
	DMutex_Unlock( & gcWorker.mutex_idle_list );
}
void DaoCGC_DecRCs( DArray *list )
{
	daoint i;
	DaoValue **values;
	if( list==NULL || list->size == 0 ) return;
	values = list->items.pValue;
	DMutex_Lock( & gcWorker.mutex_idle_list );
	for( i=0; i<list->size; i++) if( values[i] ) DaoGC_DecRC2( values[i] );
	DMutex_Unlock( & gcWorker.mutex_idle_list );
	DaoCGC_TryInvoke();
}
void DaoCGC_Finish()
{
	gcWorker.gcMin = 0;
	gcWorker.finalizing = 1;
	DaoCGC_TryInvoke();
	DThread_Join( & gcWorker.thread );

	DThread_Destroy( & gcWorker.thread );
	DMutex_Destroy( & gcWorker.mutex_idle_list );
	DMutex_Destroy( & gcWorker.mutex_start_gc );
	DMutex_Destroy( & gcWorker.mutex_block_mutator );
	DCondVar_Destroy( & gcWorker.condv_start_gc );
	DCondVar_Destroy( & gcWorker.condv_block_mutator );
}

void DaoCGC_TryInvoke()
{
	if( gcWorker.busy ) return;
	if( gcWorker.idleList->size < gcWorker.gcMin ) return;

	DMutex_Lock( & gcWorker.mutex_start_gc );
	if( gcWorker.idleList->size >= gcWorker.gcMin ) DCondVar_Signal( & gcWorker.condv_start_gc );
	DMutex_Unlock( & gcWorker.mutex_start_gc );

	DMutex_Lock( & gcWorker.mutex_block_mutator );
	if( gcWorker.idleList->size >= gcWorker.gcMax ){
		DThreadData *thdData = DThread_GetSpecific();
		if( thdData && ! ( thdData->state & DTHREAD_NO_PAUSE ) )
			DCondVar_TimedWait( & gcWorker.condv_block_mutator, & gcWorker.mutex_block_mutator, 0.001 );
	}
	DMutex_Unlock( & gcWorker.mutex_block_mutator );
}

static daoint DaoCGC_MarkIdleItems()
{
	DMutex_Lock( & gcWorker.mutex_idle_list );
	DaoIGC_MarkIdleItems();
	DMutex_Unlock( & gcWorker.mutex_idle_list );
	return gcWorker.kk;
}
void DaoCGC_Recycle( void *p )
{
	while(1){
		daoint count = gcWorker.idleList->size + gcWorker.workList->size;
		if( gcWorker.finalizing && count ==0 ) break;
		gcWorker.busy = 0;
		if( ! gcWorker.finalizing ){
			DMutex_Lock( & gcWorker.mutex_block_mutator );
			DCondVar_BroadCast( & gcWorker.condv_block_mutator );
			DMutex_Unlock( & gcWorker.mutex_block_mutator );

			DMutex_Lock( & gcWorker.mutex_start_gc );
			DCondVar_TimedWait( & gcWorker.condv_start_gc, & gcWorker.mutex_start_gc, 0.1 );
			DMutex_Unlock( & gcWorker.mutex_start_gc );
			if( gcWorker.idleList->size < gcWorker.gcMin ) continue;
		}
		gcWorker.busy = 1;

		DMutex_Lock( & gcWorker.mutex_idle_list );
		DArray_Swap( gcWorker.idleList, gcWorker.workList );
		DMutex_Unlock( & gcWorker.mutex_idle_list );

		gcWorker.kk = 0;
		gcWorker.delayMask = gcWorker.finalizing ? 0 : 0xf;
		DaoGC_PrepareCandidates();
		DaoCGC_MarkIdleItems();
		DaoCGC_CycRefCountDecScan();
		DaoCGC_CycRefCountIncScan();
		DaoCGC_RefCountDecScan();
		DaoCGC_FreeGarbage();
	}
	DThread_Exit( & gcWorker.thread );
}
void DaoCGC_CycRefCountDecScan()
{
	DArray *workList = gcWorker.workList;
	uchar_t delayMask = gcWorker.delayMask;
	daoint i, k;

	for(i=0; i<workList->size; i++){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.delay & delayMask ) continue;
		if( value->xNone.trait & DAO_VALUE_WIMETA ){
			cycRefCountDecrement( (DaoValue*) DaoMetaTables_Get( value, 0 ) );
		}
		DaoGC_CycRefCountDecScan( value );
	}
	DaoCGC_MarkIdleItems();
}
void DaoCGC_CycRefCountIncScan()
{
	daoint i, j;
	DArray *workList = gcWorker.workList;
	DArray *auxList = gcWorker.auxList;

	for(j=0; j<2; j++){
		for( i=0; i<workList->size; i++ ){
			DaoValue *value = workList->items.pValue[i];
			if( value->xGC.alive ) continue;
			if( j && value->type == DAO_NAMESPACE ){
				DaoNamespace *NS= (DaoNamespace*) value;
				DaoVmSpace_Lock( NS->vmSpace );
				DMap_Erase( NS->vmSpace->nsModules, NS->name );
				DaoVmSpace_Unlock( NS->vmSpace );
			}
			if( value->xGC.cycRefCount >0 ){
				auxList->size = 0;
				value->xGC.alive = 1;
				DArray_Append( auxList, value );
				DaoCGC_AliveObjectScan();
			}
		}
		DaoCGC_MarkIdleItems();
	}
}
int DaoCGC_AliveObjectScan()
{
	daoint i, k;
	uchar_t delayMask = gcWorker.delayMask;
	DArray *auxList = gcWorker.auxList;

	for( i=0; i<auxList->size; i++){
		DaoValue *value = auxList->items.pValue[i];
		if( value->xGC.delay & delayMask ) continue;
		if( value->xNone.trait & DAO_VALUE_WIMETA ){
			cycRefCountIncrement( (DaoValue*) DaoMetaTables_Get( value, 0 ) );
		}
		DaoGC_CycRefCountIncScan( value );
	}
	return auxList->size;
}

void DaoCGC_RefCountDecScan()
{
	DArray *workList = gcWorker.workList;
	uchar_t delayMask = gcWorker.delayMask;
	daoint i, k;

	for( i=0; i<workList->size; i++ ){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.cycRefCount && value->xGC.refCount ) continue;
		if( value->xGC.delay & delayMask ) continue;
		if( value->xNone.trait & DAO_VALUE_WIMETA ){
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
	uchar_t delayMask = gcWorker.delayMask;
	daoint i, n = 0, old = DaoCGC_MarkIdleItems();

	for(i=0; i<gcWorker.auxList2->size; i++) gcWorker.auxList2->items.pValue[i]->xGC.alive = 0;
	gcWorker.auxList2->size = 0;

	for(i=0; i<workList->size; i++){
		DaoValue *value = workList->items.pValue[i];
		value->xGC.work = value->xGC.alive = 0;
		if( value->xGC.cycRefCount && value->xGC.refCount ) continue;
		if( (value->xGC.cycRefCount && value->xGC.refCount) || value->xGC.idle ){
			if( value->xGC.delay & delayMask ){
				if( value->xGC.idle ==0 ){
					DMutex_Lock( & gcWorker.mutex_idle_list );
					DArray_Append( gcWorker.idleList, value );
					value->xGC.idle = 1;
					DMutex_Unlock( & gcWorker.mutex_idle_list );
				}
			}
			continue;
		}
		if( old != idleList->size ) old = DaoCGC_MarkIdleItems();
		if( value->xGC.idle ) continue;
		if( value->xGC.refCount !=0 ){
			printf(" refCount not zero %i: %i\n", value->type, value->xGC.refCount );
			DaoGC_PrintValueInfo( value );

			DMutex_Lock( & gcWorker.mutex_idle_list );
			DArray_Append( gcWorker.idleList, value );
			value->xGC.idle = 1;
			DMutex_Unlock( & gcWorker.mutex_idle_list );
			continue;
		}
		DaoValue_Delete( value );
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
	GC_INC_RC2 ,
	GC_DIR_DEC_RC ,
	GC_FREE
};

static void DaoIGC_Switch();
static void DaoIGC_Continue();
static void DaoIGC_RefCountDecScan();

void DaoIGC_IncRC( DaoValue *p )
{
	if( ! p ) return;
	p->xGC.refCount ++;
	if( p->type >= DAO_ENUM ) p->xGC.cycRefCount ++;
}
static int counts = 1000;
void DaoIGC_DecRC( DaoValue *p )
{
	if( p ){
		if( DaoGC_DecRC2( p ) ) DaoIGC_TryInvoke();
	}
}
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
void DaoIGC_IncRCs( DArray *list )
{
	daoint i;
	DaoValue **data;
	if( list->size == 0 ) return;
	data = list->items.pValue;
	for( i=0; i<list->size; i++) DaoIGC_IncRC( data[i] );
}
void DaoIGC_DecRCs( DArray *list )
{
	daoint i;
	DaoValue **data;
	if( list == NULL || list->size == 0 ) return;
	data = list->items.pValue;
	for( i=0; i<list->size; i++) DaoIGC_DecRC( data[i] );
	DaoIGC_TryInvoke();
}
void DaoIGC_ShiftRC( DaoValue *up, DaoValue *down )
{
	if( up == down ) return;
	if( up ) DaoIGC_IncRC( up );
	if( down ){
		if( DaoGC_DecRC2( down ) ) DaoIGC_TryInvoke();
	}
}

void DaoIGC_Switch()
{
	if( gcWorker.busy ) return;
	DArray_Swap( gcWorker.idleList, gcWorker.workList );
	gcWorker.workType = 0;
	gcWorker.ii = 0;
	gcWorker.jj = 0;
	gcWorker.kk = 0;
	gcWorker.delayMask = gcWorker.finalizing ? 0 : 0xf;
	DaoIGC_Continue();
}
void DaoIGC_Continue()
{
	if( gcWorker.busy ) return;
	gcWorker.busy = 1;
	DaoIGC_MarkIdleItems();
	switch( gcWorker.workType ){
	case GC_RESET_RC :
		DaoGC_PrepareCandidates();
		gcWorker.workType = GC_DEC_RC;
		gcWorker.ii = 0;
		break;
	case GC_DEC_RC :
		DaoIGC_CycRefCountDecScan();
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
	gcWorker.finalizing = 1;
	while( gcWorker.idleList->size || gcWorker.workList->size ){
		while( gcWorker.workList->size ) DaoIGC_Continue();
		if( gcWorker.idleList->size ) DaoIGC_Switch();
		while( gcWorker.workList->size ) DaoIGC_Continue();
	}
}
void DaoIGC_CycRefCountDecScan()
{
	DArray *workList = gcWorker.workList;
	uchar_t delayMask = gcWorker.delayMask;
	daoint min = workList->size >> 2;
	daoint i = gcWorker.ii;
	daoint j = 0, k;

	if( min < gcWorker.gcMin ) min = gcWorker.gcMin;
	for( ; i<workList->size; i++ ){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.delay & delayMask ) continue;
		if( value->xNone.trait & DAO_VALUE_WIMETA ){
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
void DaoIGC_CycRefCountIncScan()
{
	DArray *workList = gcWorker.workList;
	DArray *auxList = gcWorker.auxList;
	daoint second = gcWorker.workType == GC_INC_RC2;
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
		if( second && value->type == DAO_NAMESPACE ){
			DaoNamespace *NS= (DaoNamespace*) value;
			DaoVmSpace_Lock( NS->vmSpace );
			DMap_Erase( NS->vmSpace->nsModules, NS->name );
			DaoVmSpace_Unlock( NS->vmSpace );
		}
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
	uchar_t delayMask = gcWorker.delayMask;
	DArray *auxList = gcWorker.auxList;

	if( min < gcWorker.gcMin ) min = gcWorker.gcMin;
	for( ; j<auxList->size; j++){
		DaoValue *value = auxList->items.pValue[j];
		if( value->xGC.delay & delayMask ) continue;
		if( value->xNone.trait & DAO_VALUE_WIMETA ){
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
	uchar_t delayMask = gcWorker.delayMask;
	daoint min = workList->size >> 2;
	daoint i = gcWorker.ii;
	daoint j = 0, k;

	if( min < gcWorker.gcMin ) min = gcWorker.gcMin;
	for(; i<workList->size; i++, j++){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.cycRefCount && value->xGC.refCount ) continue;
		if( value->xGC.delay & delayMask ) continue;
		if( value->xNone.trait & DAO_VALUE_WIMETA ){
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
	uchar_t delayMask = gcWorker.delayMask;
	daoint min = workList->size >> 2;
	daoint i = gcWorker.ii;
	daoint j = 0;
	daoint old;

	if( min < gcWorker.gcMin ) min = gcWorker.gcMin;
	for(; i<workList->size; i++, j++){
		DaoValue *value = workList->items.pValue[i];
		value->xGC.work = value->xGC.alive = 0;
		if( (value->xGC.cycRefCount && value->xGC.refCount) || value->xGC.idle ){
			if( value->xGC.delay & delayMask ){
				if( value->xGC.idle ==0 ){
					value->xGC.idle = 1;
					DArray_Append( gcWorker.idleList, value );
				}
			}
			continue;
		}
		if( value->xGC.refCount !=0 ){
			printf(" refCount not zero %p %i: %i\n", value, value->type, value->xGC.refCount);
			DaoGC_PrintValueInfo( value );
			DArray_Append( gcWorker.idleList, value );
			value->xGC.idle = 1;
			continue;
		}
		old = gcWorker.idleList->size;
		DaoValue_Delete( value );
		if( old != gcWorker.idleList->size ) DaoIGC_MarkIdleItems();
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
	if( value == NULL ) return;
	/* do not scan simple data types, as they cannot from cyclic structure: */
	if( value->type < DAO_ENUM ) return;
	if( ! value->xGC.work ){
		DArray_Append( gcWorker.workList, value );
		value->xGC.cycRefCount = value->xGC.refCount;
		value->xGC.work = 1;
		value->xGC.delay += type_gc_delay[value->type];
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
		if( (value->xGC.idle|value->xGC.work) ==0 ) DArray_Append( gcWorker.auxList2, value );
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
	DaoGC_CycRefCountDecrements( list->items.pValue, list->size );
}
void cycRefCountIncrements( DArray *list )
{
	if( list == NULL ) return;
	DaoGC_CycRefCountIncrements( list->items.pValue, list->size );
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
	case DAO_CDATA : case DAO_CTYPE :
		{
			DaoCdata *cdata = (DaoCdata*) value;
			cycRefCountDecrement( (DaoValue*) cdata->object );
			cycRefCountDecrement( (DaoValue*) cdata->ctype );
			if( value->type == DAO_CDATA ){
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
			cycRefCountDecrement( (DaoValue*) rout->upRoutine );
			cycRefCountDecrement( (DaoValue*) rout->upProcess );
			cycRefCountDecrements( rout->regType );
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
			cycRefCountDecrements( klass->objDataDefault );
			cycRefCountDecrements( klass->superClass );
			cycRefCountDecrements( klass->objDataType );
			cycRefCountDecrements( klass->references );
			count += klass->constants->size + klass->variables->size;
			count += klass->objDataDefault->size + klass->objDataType->size;
			count += klass->superClass->size + klass->abstypes->size;
			count += klass->references->size + klass->abstypes->size;
			break;
		}
	case DAO_INTERFACE :
		{
			DaoInterface *inter = (DaoInterface*)value;
			cycRefCountDecrements( inter->supers );
			cycRefCountDecrement( (DaoValue*) inter->abtype );
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
	case DAO_CDATA : case DAO_CTYPE :
		{
			DaoCdata *cdata = (DaoCdata*) value;
			cycRefCountIncrement( (DaoValue*) cdata->object );
			cycRefCountIncrement( (DaoValue*) cdata->ctype );
			if( value->type == DAO_CDATA ){
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
			cycRefCountIncrement( (DaoValue*) rout->upRoutine );
			cycRefCountIncrement( (DaoValue*) rout->upProcess );
			cycRefCountIncrements( rout->regType );
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
			cycRefCountIncrements( klass->objDataDefault );
			cycRefCountIncrements( klass->superClass );
			cycRefCountIncrements( klass->objDataType );
			cycRefCountIncrements( klass->references );
			count += klass->constants->size + klass->variables->size;
			count += klass->objDataDefault->size + klass->objDataType->size;
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
	case DAO_CDATA : case DAO_CTYPE :
		{
			DaoValue *value2 = value;
			DaoCdata *cdata = (DaoCdata*) value;
			directRefCountDecrement( (DaoValue**) & cdata->object );
			directRefCountDecrement( (DaoValue**) & cdata->ctype );
			if( value->type == DAO_CDATA ){
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
			directRefCountDecrement( (DaoValue**) & rout->upRoutine );
			directRefCountDecrement( (DaoValue**) & rout->upProcess );
			directRefCountDecrements( rout->regType );
			DaoGC_ScanMap( rout->abstypes, DAO_GC_BREAK, 0, 1 );
			break;
		}
	case DAO_CLASS :
		{
			DaoClass *klass = (DaoClass*)value;
			count += klass->constants->size + klass->variables->size;
			count += klass->objDataDefault->size + klass->objDataType->size;
			count += klass->superClass->size + klass->abstypes->size;
			count += klass->references->size + klass->abstypes->size;
			count += DaoGC_ScanMap( klass->abstypes, DAO_GC_BREAK, 0, 1 );
			directRefCountDecrement( (DaoValue**) & klass->clsType );
			directRefCountDecrement( (DaoValue**) & klass->classRoutine );
			directRefCountDecrements( klass->constants );
			directRefCountDecrements( klass->variables );
			directRefCountDecrements( klass->objDataDefault );
			directRefCountDecrements( klass->superClass );
			directRefCountDecrements( klass->objDataType );
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
