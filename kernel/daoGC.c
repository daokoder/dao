/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2011, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

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
#include"daoThread.h"
#include"daoValue.h"

void GC_Lock();
void GC_Unlock();

DArray *dao_callback_data = NULL;

DaoCallbackData* DaoCallbackData_New( DaoMethod *callback, DaoValue *userdata )
{
	DaoCallbackData *self;
	if( callback == NULL ) return NULL;
	if( callback->type < DAO_FUNCTREE || callback->type > DAO_FUNCTION ) return NULL;
	self = (DaoCallbackData*) calloc( 1, sizeof(DaoCallbackData) );
	self->callback = callback;
	DaoValue_Copy( userdata, & self->userdata );
	GC_Lock();
	DArray_Append( dao_callback_data, self );
	GC_Unlock();
	return self;
}
static void DaoCallbackData_Delete( DaoCallbackData *self )
{
	GC_DecRC( self->userdata );
	dao_free( self );
}
static void DaoCallbackData_DeleteByCallback( DaoValue *callback )
{
	DaoCallbackData *cd = NULL;
	int i;
	if( dao_callback_data->size ==0 ) return;
	GC_Lock();
	for(i=0; i<dao_callback_data->size; i++){
		cd = (DaoCallbackData*) dao_callback_data->items.pValue[i];
		if( cd->callback == (DaoMethod*) callback ){
			DaoCallbackData_Delete( cd );
			DArray_Erase( dao_callback_data, i, 1 );
			i--;
		}
	}
	GC_Unlock();
}
static void DaoCallbackData_DeleteByUserdata( DaoValue *userdata )
{
	DaoCallbackData *cd = NULL;
	int i;
	if( userdata == NULL ) return;
	if( dao_callback_data->size ==0 ) return;
	GC_Lock();
	for(i=0; i<dao_callback_data->size; i++){
		cd = (DaoCallbackData*) dao_callback_data->items.pValue[i];
		if( cd->userdata == userdata ){
			DaoCallbackData_Delete( cd );
			DArray_Erase( dao_callback_data, i, 1 );
			i--;
		}
	}
	GC_Unlock();
}


#if DEBUG
#if 0
#endif
#define DEBUG_TRACE
#endif

#ifdef DEBUG_TRACE
#include <execinfo.h>
void print_trace( const char *info )
{
	void  *array[100];
	size_t i, size = backtrace (array, 100);
	char **strings = backtrace_symbols (array, size);
	FILE *debug = fopen( "debug.txt", "w+" );
	fprintf (debug, "===========================================\n");
	fprintf (debug, "Obtained %zd stack frames.\n", size);
	printf ("=====================%s======================\n", info);
	printf ("Obtained %zd stack frames.\n", size);
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
	if( value->type == DAO_FUNCTION ){
		printf( "%s\n", value->xFunction.routName->mbs );
	}else if( value->type == DAO_TYPE ){
		printf( "%s\n", value->xType.name->mbs );
	}else if( value->type == DAO_CDATA ){
		printf( "%s\n", value->xCdata.ctype->name->mbs );
	}else if( value->type == DAO_TYPEKERNEL ){
		printf( "%s\n", ((DaoTypeKernel*)value)->typer->name );
	}
}
#else
#define DaoGC_PrintValueInfo( value )  (value == value)
#endif

static int DaoGC_DecRC2( DaoValue *p );
static void DaoGC_CycRefCountDecrements( DaoValue **values, size_t size );
static void DaoGC_CycRefCountIncrements( DaoValue **values, size_t size );
static void DaoGC_RefCountDecrements( DaoValue **values, size_t size );
static void cycRefCountDecrement( DaoValue *value );
static void cycRefCountIncrement( DaoValue *value );
static void cycRefCountDecrements( DArray *values );
static void cycRefCountIncrements( DArray *values );
static void directRefCountDecrement( DaoValue **value );
static void directRefCountDecrements( DArray *values );
static int cycRefCountDecrementMapValue( DMap *dmap );
static int cycRefCountIncrementMapValue( DMap *dmap );
static int directRefCountDecrementMapValue( DMap *dmap );

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

static void DaoGC_Init();

#ifdef DAO_WITH_THREAD
static void DaoCGC_Recycle( void * );
static void DaoCGC_TryInvoke();
#endif


typedef struct DaoGarbageCollector  DaoGarbageCollector;
struct DaoGarbageCollector
{
	DArray   *idleList;
	DArray   *workList;
	DArray   *auxList;
	DArray   *auxList2;

	int       gcMin, gcMax;
	int       count;
	int       ii, jj, kk;
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

static DaoEnum dummyEnum = {0,0,DAO_DATA_CONST,0,1,1,NULL,0};
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

	gcWorker.finalizing = 0;
	gcWorker.delayMask = 0xf;

	gcWorker.gcMin = 1000;
	gcWorker.gcMax = 100 * gcWorker.gcMin;
	gcWorker.count = 0;
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
	type_gc_delay[ DAO_FUNCTREE ] = 1;
	type_gc_delay[ DAO_ROUTINE ] = 1;
	type_gc_delay[ DAO_FUNCTION ] = 1;
	type_gc_delay[ DAO_PROCESS ] = 1;
	type_gc_delay[ DAO_NAMESPACE ] = 1;
	type_gc_delay[ DAO_TYPE ] = 1;
	type_gc_delay[ DAO_ABROUTINE ] = 1;
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
static void DaoGC_DeleteSimpleData( DaoValue *value )
{
	if( value == NULL || value->xGC.refCount ) return;
#ifdef DAO_GC_PROF
	if( value->type < DAO_ENUM ) ObjectProfile[value->type] --;
#endif
	switch( value->type ){
	case DAO_NULL :
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
	p->xGC.refCount --;

	if( p->xGC.refCount == 0 ){
#ifdef DAO_GC_PROF
		if( p->type < DAO_ENUM ) ObjectProfile[p->type] --;
#endif
		switch( p->xGC.type ){
		case DAO_NULL :
		case DAO_INTEGER :
		case DAO_FLOAT :
		case DAO_DOUBLE :
		case DAO_COMPLEX : dao_free( p ); return 1;
		case DAO_LONG : DaoLong_Delete( & p->xLong ); return 1;
		case DAO_STRING : DaoString_Delete( & p->xString ); return 1;
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY : DaoArray_ResizeVector( & p->xArray, 0 ); break;
#endif
		default : break;
		/* No safe way to delete other types of objects here, since they might be
		 * being concurrently scanned by the GC! */
		}
	}
	/* never push simple data types into GC queue,
	 * because they cannot form cyclic referencing structure: */
	if( (p->type < DAO_ENUM) | p->xGC.idle ) return 1;

	DArray_Append( gcWorker.idleList, p );
	return 0;
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
	gcWorker.idleList = NULL;
}

#ifdef DAO_WITH_THREAD
void DaoGC_IncRC( DaoValue *value )
{
	if( gcWorker.concurrent ){
		DaoCGC_IncRC( value );
		return;
	}
	DaoIGC_IncRC( value );
}
void DaoGC_DecRC( DaoValue *value )
{
	if( gcWorker.concurrent ){
		DaoCGC_DecRC( value );
		return;
	}
	DaoIGC_DecRC( value );
}
void DaoGC_ShiftRC( DaoValue *up, DaoValue *down )
{
	if( gcWorker.concurrent ){
		DaoCGC_ShiftRC( up, down );
		return;
	}
	DaoIGC_ShiftRC( up, down );
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
#else

void DaoGC_IncRC( DaoValue *value )
{
	DaoIGC_IncRC( value );
}
void DaoGC_DecRC( DaoValue *value )
{
	DaoIGC_DecRC( value );
}
void DaoGC_ShiftRC( DaoValue *up, DaoValue *down )
{
	DaoIGC_ShiftRC( up, down );
}
void DaoGC_IncRCs( DArray *values )
{
	DaoIGC_IncRCs( values );
}
void DaoGC_DecRCs( DArray *values )
{
	DaoIGC_DecRCs( values );
}
#endif


void DaoGC_PrepareCandidates()
{
	DaoValue *value;
	DArray *workList = gcWorker.workList;
	size_t i, k = 0;
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
	size_t i, n = idleList->size;
	for(i=gcWorker.kk; i<n; i++) idleList->items.pValue[i]->xGC.idle = 1;
	idleList->size = n;
	gcWorker.kk = n;
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
	if( ! p ) return;

	DMutex_Lock( & gcWorker.mutex_idle_list );

	DaoGC_DecRC2( p );

	DMutex_Unlock( & gcWorker.mutex_idle_list );

	if( gcWorker.idleList->size > gcWorker.gcMin ) DaoCGC_TryInvoke();
}
void DaoCGC_IncRC( DaoValue *p )
{
	if( ! p ) return;

	DMutex_Lock( & gcWorker.mutex_idle_list );
	p->xGC.refCount ++;
	if( p->type >= DAO_ENUM ) p->xGC.cycRefCount ++;
	DMutex_Unlock( & gcWorker.mutex_idle_list );
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

	if( down && gcWorker.idleList->size > gcWorker.gcMin ) DaoCGC_TryInvoke();
}

void DaoCGC_IncRCs( DArray *list )
{
	size_t i;
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
	size_t i;
	DaoValue **values;
	if( list==NULL || list->size == 0 ) return;
	values = list->items.pValue;
	DMutex_Lock( & gcWorker.mutex_idle_list );
	for( i=0; i<list->size; i++) if( values[i] ) DaoGC_DecRC2( values[i] );
	DMutex_Unlock( & gcWorker.mutex_idle_list );
	if( gcWorker.idleList->size > gcWorker.gcMin ) DaoCGC_TryInvoke();
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
	if( gcWorker.busy == 0 ) return;

	DMutex_Lock( & gcWorker.mutex_start_gc );
	if( gcWorker.count >= gcWorker.gcMin ) DCondVar_Signal( & gcWorker.condv_start_gc );
	DMutex_Unlock( & gcWorker.mutex_start_gc );

	DMutex_Lock( & gcWorker.mutex_block_mutator );
	if( gcWorker.count >= gcWorker.gcMax ){
		DThreadData *thdData = DThread_GetSpecific();
		if( thdData && ! ( thdData->state & DTHREAD_NO_PAUSE ) )
			DCondVar_TimedWait( & gcWorker.condv_block_mutator, & gcWorker.mutex_block_mutator, 0.001 );
	}
	DMutex_Unlock( & gcWorker.mutex_block_mutator );
}

static size_t DaoCGC_MarkIdleItems()
{
	DMutex_Lock( & gcWorker.mutex_idle_list );
	DaoIGC_MarkIdleItems();
	DMutex_Unlock( & gcWorker.mutex_idle_list );
	return gcWorker.kk;
}
static void DaoValue_Delete( DaoValue *self )
{
	DaoTypeBase *typer = DaoValue_GetTyper( self );
#ifdef DAO_GC_PROF
	ObjectProfile[self->type] --;
#endif
	if( self->type >= DAO_FUNCTREE && self->type <= DAO_FUNCTION )
		DaoCallbackData_DeleteByCallback( self );
	DaoCallbackData_DeleteByUserdata( self );
	typer->Delete( self );
}
void DaoCGC_Recycle( void *p )
{
	while(1){
		size_t count = gcWorker.idleList->size + gcWorker.workList->size;
		if( gcWorker.finalizing && count ==0 ) break;
		if( ! gcWorker.finalizing ){
			DMutex_Lock( & gcWorker.mutex_block_mutator );
			DCondVar_BroadCast( & gcWorker.condv_block_mutator );
			DMutex_Unlock( & gcWorker.mutex_block_mutator );

			DMutex_Lock( & gcWorker.mutex_start_gc );
			DCondVar_TimedWait( & gcWorker.condv_start_gc, & gcWorker.mutex_start_gc, 0.1 );
			DMutex_Unlock( & gcWorker.mutex_start_gc );
			if( gcWorker.count < gcWorker.gcMin ) continue;
		}

		DMutex_Lock( & gcWorker.mutex_idle_list );
		gcWorker.count = 0;
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
	DNode *node;
	DArray *workList = gcWorker.workList;
	uchar_t delayMask = gcWorker.delayMask;
	size_t i, k;

	for(i=0; i<workList->size; i++){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.delay & delayMask ) continue;
		switch( value->type ){
		case DAO_ENUM :
			{
				DaoEnum *en = (DaoEnum*) value;
				cycRefCountDecrement( (DaoValue*) en->etype );
				break;
			}
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) value;
				cycRefCountDecrement( (DaoValue*) array->unitype );
				//cycRefCountDecrement( (DaoValue*) array->meta );
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple = (DaoTuple*) value;
				cycRefCountDecrement( (DaoValue*) tuple->unitype );
				//cycRefCountDecrement( (DaoValue*) tuple->meta );
				DaoGC_CycRefCountDecrements( tuple->items, tuple->size );
				break;
			}
		case DAO_LIST :
			{
				DaoList *list = (DaoList*) value;
				cycRefCountDecrement( (DaoValue*) list->unitype );
				cycRefCountDecrement( (DaoValue*) list->meta );
				cycRefCountDecrements( list->items );
				break;
			}
		case DAO_MAP :
			{
				DaoMap *map = (DaoMap*) value;
				cycRefCountDecrement( (DaoValue*) map->unitype );
				cycRefCountDecrement( (DaoValue*) map->meta );
				node = DMap_First( map->items );
				for( ; node != NULL; node = DMap_Next( map->items, node ) ) {
					cycRefCountDecrement( node->key.pValue );
					cycRefCountDecrement( node->value.pValue );
				}
				break;
			}
		case DAO_OBJECT :
			{
				DaoObject *obj = (DaoObject*) value;
				if( obj->isRoot ) DaoGC_CycRefCountDecrements( obj->objValues, obj->valueCount );
				DaoGC_CycRefCountDecrements( obj->parents, obj->baseCount );
				cycRefCountDecrement( (DaoValue*) obj->rootObject );
				cycRefCountDecrement( (DaoValue*) obj->defClass );
				//cycRefCountDecrement( (DaoValue*) obj->meta );
				break;
			}
		case DAO_CDATA : case DAO_CTYPE :
			{
				DaoCdata *cdata = (DaoCdata*) value;
				cycRefCountDecrement( (DaoValue*) cdata->object );
				cycRefCountDecrement( (DaoValue*) cdata->ctype );
				break;
			}
		case DAO_FUNCTREE :
			{
				DaoFunctree *meta = (DaoFunctree*) value;
				cycRefCountDecrement( (DaoValue*) meta->space );
				cycRefCountDecrement( (DaoValue*) meta->host );
				cycRefCountDecrement( (DaoValue*) meta->unitype );
				cycRefCountDecrements( meta->routines );
				break;
			}
		case DAO_ROUTINE :
		case DAO_FUNCTION :
		case DAO_ABROUTINE :
			{
				DaoRoutine *rout = (DaoRoutine*)value;
				cycRefCountDecrement( (DaoValue*) rout->routType );
				cycRefCountDecrement( (DaoValue*) rout->routHost );
				cycRefCountDecrement( (DaoValue*) rout->nameSpace );
				cycRefCountDecrements( rout->routConsts );
				if( rout->type == DAO_ROUTINE ){
					cycRefCountDecrement( (DaoValue*) rout->upRoutine );
					cycRefCountDecrement( (DaoValue*) rout->upContext );
					cycRefCountDecrement( (DaoValue*) rout->original );
					cycRefCountDecrement( (DaoValue*) rout->specialized );
					cycRefCountDecrements( rout->regType );
					cycRefCountDecrementMapValue( rout->abstypes );
				}
				break;
			}
		case DAO_CLASS :
			{
				DaoClass *klass = (DaoClass*)value;
				cycRefCountDecrementMapValue(klass->abstypes );
				cycRefCountDecrement( (DaoValue*) klass->clsType );
				cycRefCountDecrement( (DaoValue*) klass->classRoutine );
				cycRefCountDecrements( klass->cstData );
				cycRefCountDecrements( klass->glbData );
				cycRefCountDecrements( klass->objDataDefault );
				cycRefCountDecrements( klass->superClass );
				cycRefCountDecrements( klass->objDataType );
				cycRefCountDecrements( klass->glbDataType );
				cycRefCountDecrements( klass->references );
				break;
			}
		case DAO_INTERFACE :
			{
				DaoInterface *inter = (DaoInterface*)value;
				cycRefCountDecrementMapValue( inter->methods );
				cycRefCountDecrements( inter->supers );
				cycRefCountDecrement( (DaoValue*) inter->abtype );
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNamespace *ns = (DaoNamespace*) value;
				cycRefCountDecrements( ns->cstData );
				cycRefCountDecrements( ns->varData );
				cycRefCountDecrements( ns->varType );
				cycRefCountDecrements( ns->auxData );
				cycRefCountDecrements( ns->mainRoutines );
				cycRefCountDecrementMapValue( ns->abstypes );
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) value;
				cycRefCountDecrement( abtp->aux );
				cycRefCountDecrement( abtp->value );
				cycRefCountDecrement( (DaoValue*) abtp->kernel );
				cycRefCountDecrements( abtp->nested );
				cycRefCountDecrementMapValue( abtp->interfaces );
				break;
			}
		case DAO_TYPEKERNEL :
			{
				DaoTypeKernel *kernel = (DaoTypeKernel*) value;
				cycRefCountDecrement( (DaoValue*) kernel->abtype );
				cycRefCountDecrement( (DaoValue*) kernel->nspace );
				cycRefCountDecrementMapValue( kernel->values );
				cycRefCountDecrementMapValue( kernel->methods );
				cycRefCountDecrementMapValue( kernel->instances );
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
				DaoGC_CycRefCountDecrements( vmp->stackValues, vmp->stackSize );
				while( frame ){
					cycRefCountDecrement( (DaoValue*) frame->routine );
					cycRefCountDecrement( (DaoValue*) frame->function );
					cycRefCountDecrement( (DaoValue*) frame->object );
					frame = frame->next;
				}
				break;
			}
		default: break;
		}
	}
	DaoCGC_MarkIdleItems();
}
void DaoCGC_CycRefCountIncScan()
{
	size_t i, j;
	DArray *workList = gcWorker.workList;
	DArray *auxList = gcWorker.auxList;

	for(j=0; j<2; j++){
		for( i=0; i<workList->size; i++ ){
			DaoValue *value = workList->items.pValue[i];
			if( value->xGC.alive ) continue;
			if( value->type == DAO_CDATA && (value->xGC.refCount ==0 || value->xGC.cycRefCount ==0) ){
				DaoCdata *cdata = (DaoCdata*) value;
				DaoCdataCore *core = (DaoCdataCore*)cdata->ctype->kernel->core;
				if( !(cdata->attribs & DAO_CDATA_FREE) ) continue;
				if( cdata->data == NULL || core == NULL ) continue;
				if( core->DelTest == NULL ) continue;
				if( core->DelTest( cdata->data ) ) continue;
				DaoCdata_SetExtReference( cdata, 1 );
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
	size_t i, k;
	uchar_t delayMask = gcWorker.delayMask;
	DArray *auxList = gcWorker.auxList;
	DNode *node;

	for( i=0; i<auxList->size; i++){
		DaoValue *value = auxList->items.pValue[i];
		if( value->xGC.delay & delayMask ) continue;
		switch( value->type ){
		case DAO_ENUM :
			{
				DaoEnum *en = (DaoEnum*) value;
				cycRefCountIncrement( (DaoValue*) en->etype );
				break;
			}
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) value;
				cycRefCountIncrement( (DaoValue*) array->unitype );
				//cycRefCountIncrement( (DaoValue*) array->meta );
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple= (DaoTuple*) value;
				cycRefCountIncrement( (DaoValue*) tuple->unitype );
				//cycRefCountIncrement( (DaoValue*) tuple->meta );
				DaoGC_CycRefCountIncrements( tuple->items, tuple->size );
				break;
			}
		case DAO_LIST :
			{
				DaoList *list= (DaoList*) value;
				cycRefCountIncrement( (DaoValue*) list->unitype );
				cycRefCountIncrement( (DaoValue*) list->meta );
				cycRefCountIncrements( list->items );
				break;
			}
		case DAO_MAP :
			{
				DaoMap *map = (DaoMap*)value;
				cycRefCountIncrement( (DaoValue*) map->unitype );
				cycRefCountIncrement( (DaoValue*) map->meta );
				node = DMap_First( map->items );
				for( ; node != NULL; node = DMap_Next( map->items, node ) ){
					cycRefCountIncrement( node->key.pValue );
					cycRefCountIncrement( node->value.pValue );
				}
				break;
			}
		case DAO_OBJECT :
			{
				DaoObject *obj = (DaoObject*) value;
				if( obj->isRoot ) DaoGC_CycRefCountIncrements( obj->objValues, obj->valueCount );
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
				break;
			}
		case DAO_FUNCTREE :
			{
				DaoFunctree *meta = (DaoFunctree*) value;
				cycRefCountIncrement( (DaoValue*) meta->space );
				cycRefCountIncrement( (DaoValue*) meta->host );
				cycRefCountIncrement( (DaoValue*) meta->unitype );
				cycRefCountIncrements( meta->routines );
				break;
			}
		case DAO_ROUTINE :
		case DAO_FUNCTION :
		case DAO_ABROUTINE :
			{
				DaoRoutine *rout = (DaoRoutine*) value;
				cycRefCountIncrement( (DaoValue*) rout->routType );
				cycRefCountIncrement( (DaoValue*) rout->routHost );
				cycRefCountIncrement( (DaoValue*) rout->nameSpace );
				cycRefCountIncrements( rout->routConsts );
				if( rout->type == DAO_ROUTINE ){
					cycRefCountIncrement( (DaoValue*) rout->upRoutine );
					cycRefCountIncrement( (DaoValue*) rout->upContext );
					cycRefCountIncrement( (DaoValue*) rout->original );
					cycRefCountIncrement( (DaoValue*) rout->specialized );
					cycRefCountIncrements( rout->regType );
					cycRefCountIncrementMapValue( rout->abstypes );
				}
				break;
			}
		case DAO_CLASS :
			{
				DaoClass *klass = (DaoClass*) value;
				cycRefCountIncrementMapValue( klass->abstypes );
				cycRefCountIncrement( (DaoValue*) klass->clsType );
				cycRefCountIncrement( (DaoValue*) klass->classRoutine );
				cycRefCountIncrements( klass->cstData );
				cycRefCountIncrements( klass->glbData );
				cycRefCountIncrements( klass->objDataDefault );
				cycRefCountIncrements( klass->superClass );
				cycRefCountIncrements( klass->objDataType );
				cycRefCountIncrements( klass->glbDataType );
				cycRefCountIncrements( klass->references );
				break;
			}
		case DAO_INTERFACE :
			{
				DaoInterface *inter = (DaoInterface*)value;
				cycRefCountIncrementMapValue( inter->methods );
				cycRefCountIncrements( inter->supers );
				cycRefCountIncrement( (DaoValue*) inter->abtype );
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNamespace *ns = (DaoNamespace*) value;
				cycRefCountIncrements( ns->cstData );
				cycRefCountIncrements( ns->varData );
				cycRefCountIncrements( ns->varType );
				cycRefCountIncrements( ns->auxData );
				cycRefCountIncrements( ns->mainRoutines );
				cycRefCountIncrementMapValue( ns->abstypes );
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) value;
				cycRefCountIncrement( abtp->aux );
				cycRefCountIncrement( abtp->value );
				cycRefCountIncrement( (DaoValue*) abtp->kernel );
				cycRefCountIncrements( abtp->nested );
				cycRefCountIncrementMapValue( abtp->interfaces );
				break;
			}
		case DAO_TYPEKERNEL :
			{
				DaoTypeKernel *kernel = (DaoTypeKernel*) value;
				cycRefCountIncrement( (DaoValue*) kernel->abtype );
				cycRefCountIncrement( (DaoValue*) kernel->nspace );
				cycRefCountIncrementMapValue( kernel->values );
				cycRefCountIncrementMapValue( kernel->methods );
				cycRefCountIncrementMapValue( kernel->instances );
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
				DaoGC_CycRefCountIncrements( vmp->stackValues, vmp->stackSize );
				while( frame ){
					cycRefCountIncrement( (DaoValue*) frame->routine );
					cycRefCountIncrement( (DaoValue*) frame->function );
					cycRefCountIncrement( (DaoValue*) frame->object );
					frame = frame->next;
				}
				break;
			}
		default: break;
		}
	}
	return auxList->size;
}

void DaoCGC_RefCountDecScan()
{
	DNode *node;
	DArray *workList = gcWorker.workList;
	uchar_t delayMask = gcWorker.delayMask;
	size_t i, k;

	for( i=0; i<workList->size; i++ ){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.cycRefCount && value->xGC.refCount ) continue;
		if( value->xGC.delay & delayMask ) continue;

		DMutex_Lock( & gcWorker.mutex_idle_list );
		switch( value->type ){
		case DAO_ENUM :
			{
				DaoEnum *en = (DaoEnum*) value;
				directRefCountDecrement( (DaoValue**) & en->etype );
				break;
			}
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) value;
				directRefCountDecrement( (DaoValue**) & array->unitype );
				//directRefCountDecrement( (DaoValue**) & array->meta );
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple = (DaoTuple*) value;
				directRefCountDecrement( (DaoValue**) & tuple->unitype );
				//directRefCountDecrement( (DaoValue**) & tuple->meta );
				DaoGC_RefCountDecrements( tuple->items, tuple->size );
				tuple->size = 0;
				break;
			}
		case DAO_LIST :
			{
				DaoList *list = (DaoList*) value;
				directRefCountDecrements( list->items );
				directRefCountDecrement( (DaoValue**) & list->unitype );
				directRefCountDecrement( (DaoValue**) & list->meta );
				break;
			}
		case DAO_MAP :
			{
				DaoMap *map = (DaoMap*) value;
				node = DMap_First( map->items );
				for( ; node != NULL; node = DMap_Next( map->items, node ) ){
					node->key.pValue->xGC.refCount --;
					node->value.pValue->xGC.refCount --;
					DaoGC_DeleteSimpleData( node->key.pValue );
					DaoGC_DeleteSimpleData( node->value.pValue );
				}
				map->items->keytype = map->items->valtype = 0;
				DMap_Clear( map->items );
				directRefCountDecrement( (DaoValue**) & map->unitype );
				directRefCountDecrement( (DaoValue**) & map->meta );
				break;
			}
		case DAO_OBJECT :
			{
				DaoObject *obj = (DaoObject*) value;
				if( obj->isRoot ) DaoGC_RefCountDecrements( obj->objValues, obj->valueCount );
				DaoGC_RefCountDecrements( obj->parents, obj->baseCount );
				directRefCountDecrement( (DaoValue**) & obj->rootObject );
				directRefCountDecrement( (DaoValue**) & obj->defClass );
				obj->valueCount = obj->baseCount = 0;
				break;
			}
		case DAO_CDATA : case DAO_CTYPE :
			{
				DaoCdata *cdata = (DaoCdata*) value;
				DaoTypeBase *typer = cdata->typer;
				/* Do not use ctype->kernel, since this reference may have been broken: */
				value = (DaoValue*) (typer && typer->core ? typer->core->kernel : NULL);
				if( value && value->xGC.idle ==0 ){
					/* To make sure that the DaoTypeKernel will be deleted after: */
					value->xGC.idle = 1;
					DArray_Append( gcWorker.idleList, value );
				}
				directRefCountDecrement( (DaoValue**) & cdata->object );
				directRefCountDecrement( (DaoValue**) & cdata->ctype );
				break;
			}
		case DAO_FUNCTREE :
			{
				DaoFunctree *meta = (DaoFunctree*) value;
				directRefCountDecrement( (DaoValue**) & meta->space );
				directRefCountDecrement( (DaoValue**) & meta->host );
				directRefCountDecrement( (DaoValue**) & meta->unitype );
				directRefCountDecrements( meta->routines );
				break;
			}
		case DAO_ROUTINE :
		case DAO_FUNCTION :
		case DAO_ABROUTINE :
			{
				DaoRoutine *rout = (DaoRoutine*)value;
				directRefCountDecrement( (DaoValue**) & rout->nameSpace );
				directRefCountDecrement( (DaoValue**) & rout->routType );
				directRefCountDecrement( (DaoValue**) & rout->routHost );
				directRefCountDecrements( rout->routConsts );
				if( rout->type == DAO_ROUTINE ){
					directRefCountDecrement( (DaoValue**) & rout->upRoutine );
					directRefCountDecrement( (DaoValue**) & rout->upContext );
					directRefCountDecrement( (DaoValue**) & rout->original );
					directRefCountDecrement( (DaoValue**) & rout->specialized );
					directRefCountDecrements( rout->regType );
					directRefCountDecrementMapValue( rout->abstypes );
				}
				break;
			}
		case DAO_CLASS :
			{
				DaoClass *klass = (DaoClass*)value;
				directRefCountDecrement( (DaoValue**) & klass->clsType );
				directRefCountDecrement( (DaoValue**) & klass->classRoutine );
				directRefCountDecrementMapValue( klass->abstypes );
				directRefCountDecrements( klass->cstData );
				directRefCountDecrements( klass->glbData );
				directRefCountDecrements( klass->objDataDefault );
				directRefCountDecrements( klass->superClass );
				directRefCountDecrements( klass->objDataType );
				directRefCountDecrements( klass->glbDataType );
				directRefCountDecrements( klass->references );
				break;
			}
		case DAO_INTERFACE :
			{
				DaoInterface *inter = (DaoInterface*)value;
				directRefCountDecrementMapValue( inter->methods );
				directRefCountDecrements( inter->supers );
				directRefCountDecrement( (DaoValue**) & inter->abtype );
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNamespace *ns = (DaoNamespace*) value;
				directRefCountDecrements( ns->cstData );
				directRefCountDecrements( ns->varData );
				directRefCountDecrements( ns->varType );
				directRefCountDecrements( ns->auxData );
				directRefCountDecrements( ns->mainRoutines );
				directRefCountDecrementMapValue( ns->abstypes );
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) value;
				directRefCountDecrements( abtp->nested );
				directRefCountDecrement( (DaoValue**) & abtp->aux );
				directRefCountDecrement( (DaoValue**) & abtp->value );
				directRefCountDecrement( (DaoValue**) & abtp->kernel );
				directRefCountDecrementMapValue( abtp->interfaces );
				break;
			}
		case DAO_TYPEKERNEL :
			{
				DaoTypeKernel *kernel = (DaoTypeKernel*) value;
				directRefCountDecrement( (DaoValue**) & kernel->abtype );
				directRefCountDecrement( (DaoValue**) & kernel->nspace );
				directRefCountDecrementMapValue( kernel->values );
				directRefCountDecrementMapValue( kernel->methods );
				directRefCountDecrementMapValue( kernel->instances );
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
				DaoGC_RefCountDecrements( vmp->stackValues, vmp->stackSize );
				vmp->stackSize = 0;
				while( frame ){
					if( frame->routine ) frame->routine->refCount --;
					if( frame->function ) frame->function->refCount --;
					if( frame->object ) frame->object->refCount --;
					frame->routine = NULL;
					frame->function = NULL;
					frame->object = NULL;
					frame = frame->next;
				}
				break;
			}
		default: break;
		}
		DMutex_Unlock( & gcWorker.mutex_idle_list );
	}
}
static void DaoCGC_FreeGarbage()
{
	DArray *idleList = gcWorker.idleList;
	DArray *workList = gcWorker.workList;
	uchar_t delayMask = gcWorker.delayMask;
	size_t i, n = 0, old = DaoCGC_MarkIdleItems();

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
static int counts = 100;
void DaoIGC_DecRC( DaoValue *p )
{
	if( ! p ) return;

	if( DaoGC_DecRC2( p ) ) return;

	if( gcWorker.busy ) return;
	counts --;
	if( gcWorker.idleList->size < gcWorker.gcMax ){
		if( counts ) return;
		counts = 100;
	}else{
		if( counts ) return;
		counts = 10;
	}

	if( gcWorker.workList->size )
		DaoIGC_Continue();
	else if( gcWorker.idleList->size > gcWorker.gcMin )
		DaoIGC_Switch();
}
void DaoIGC_IncRCs( DArray *list )
{
	size_t i;
	DaoValue **data;
	if( list->size == 0 ) return;
	data = list->items.pValue;
	for( i=0; i<list->size; i++) DaoIGC_IncRC( data[i] );
}
void DaoIGC_DecRCs( DArray *list )
{
	size_t i;
	DaoValue **data;
	if( list == NULL || list->size == 0 ) return;
	data = list->items.pValue;
	for( i=0; i<list->size; i++) DaoIGC_DecRC( data[i] );
}
void DaoIGC_ShiftRC( DaoValue *up, DaoValue *down )
{
	if( up == down ) return;
	if( up ) DaoIGC_IncRC( up );
	if( down ) DaoIGC_DecRC( down );
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
	DNode *node;
	DArray *workList = gcWorker.workList;
	uchar_t delayMask = gcWorker.delayMask;
	size_t i = gcWorker.ii;
	size_t j = 0, k;

	for( ; i<workList->size; i++ ){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.delay & delayMask ) continue;
		switch( value->type ){
		case DAO_ENUM :
			{
				DaoEnum *en = (DaoEnum*) value;
				cycRefCountDecrement( (DaoValue*) en->etype );
				break;
			}
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) value;
				cycRefCountDecrement( (DaoValue*) array->unitype );
				//cycRefCountDecrement( (DaoValue*) array->meta );
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple = (DaoTuple*) value;
				cycRefCountDecrement( (DaoValue*) tuple->unitype );
				//cycRefCountDecrement( (DaoValue*) tuple->meta );
				DaoGC_CycRefCountDecrements( tuple->items, tuple->size );
				j += tuple->size;
				break;
			}
		case DAO_LIST :
			{
				DaoList *list = (DaoList*) value;
				cycRefCountDecrement( (DaoValue*) list->unitype );
				cycRefCountDecrement( (DaoValue*) list->meta );
				cycRefCountDecrements( list->items );
				j += list->items->size;
				break;
			}
		case DAO_MAP :
			{
				DaoMap *map = (DaoMap*) value;
				cycRefCountDecrement( (DaoValue*) map->unitype );
				cycRefCountDecrement( (DaoValue*) map->meta );
				node = DMap_First( map->items );
				for( ; node != NULL; node = DMap_Next( map->items, node ) ) {
					cycRefCountDecrement( node->key.pValue );
					cycRefCountDecrement( node->value.pValue );
				}
				j += map->items->size;
				break;
			}
		case DAO_OBJECT :
			{
				DaoObject *obj = (DaoObject*) value;
				if( obj->isRoot ){
					DaoGC_CycRefCountDecrements( obj->objValues, obj->valueCount );
					j += obj->valueCount;
				}
				j += obj->baseCount;
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
				break;
			}
		case DAO_FUNCTREE :
			{
				DaoFunctree *meta = (DaoFunctree*) value;
				cycRefCountDecrement( (DaoValue*) meta->space );
				cycRefCountDecrement( (DaoValue*) meta->host );
				cycRefCountDecrement( (DaoValue*) meta->unitype );
				cycRefCountDecrements( meta->routines );
				break;
			}
		case DAO_ROUTINE :
		case DAO_FUNCTION :
		case DAO_ABROUTINE :
			{
				DaoRoutine *rout = (DaoRoutine*)value;
				cycRefCountDecrement( (DaoValue*) rout->routType );
				cycRefCountDecrement( (DaoValue*) rout->routHost );
				cycRefCountDecrement( (DaoValue*) rout->nameSpace );
				cycRefCountDecrements( rout->routConsts );
				j += rout->routConsts->size;
				if( rout->type == DAO_ROUTINE ){
					j += rout->regType->size + rout->abstypes->size;
					cycRefCountDecrement( (DaoValue*) rout->upRoutine );
					cycRefCountDecrement( (DaoValue*) rout->upContext );
					cycRefCountDecrement( (DaoValue*) rout->original );
					cycRefCountDecrement( (DaoValue*) rout->specialized );
					cycRefCountDecrements( rout->regType );
					cycRefCountDecrementMapValue( rout->abstypes );
				}
				break;
			}
		case DAO_CLASS :
			{
				DaoClass *klass = (DaoClass*)value;
				cycRefCountDecrementMapValue( klass->abstypes );
				cycRefCountDecrement( (DaoValue*) klass->clsType );
				cycRefCountDecrement( (DaoValue*) klass->classRoutine );
				cycRefCountDecrements( klass->cstData );
				cycRefCountDecrements( klass->glbData );
				cycRefCountDecrements( klass->objDataDefault );
				cycRefCountDecrements( klass->superClass );
				cycRefCountDecrements( klass->objDataType );
				cycRefCountDecrements( klass->glbDataType );
				cycRefCountDecrements( klass->references );
				j += klass->cstData->size + klass->glbData->size;
				j += klass->cstData->size + klass->objDataDefault->size;
				j += klass->superClass->size + klass->abstypes->size;
				j += klass->objDataType->size + klass->glbDataType->size;
				j += klass->references->size + klass->abstypes->size;
				break;
			}
		case DAO_INTERFACE :
			{
				DaoInterface *inter = (DaoInterface*)value;
				cycRefCountDecrements( inter->supers );
				cycRefCountDecrement( (DaoValue*) inter->abtype );
				j += cycRefCountDecrementMapValue( inter->methods );
				j += inter->supers->size + inter->methods->size;
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNamespace *ns = (DaoNamespace*) value;
				cycRefCountDecrements( ns->cstData );
				cycRefCountDecrements( ns->varData );
				cycRefCountDecrements( ns->varType );
				cycRefCountDecrements( ns->auxData );
				cycRefCountDecrements( ns->mainRoutines );
				j += cycRefCountDecrementMapValue( ns->abstypes );
				j += ns->cstData->size + ns->varData->size + ns->abstypes->size;
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) value;
				cycRefCountDecrement( abtp->aux );
				cycRefCountDecrement( abtp->value );
				cycRefCountDecrement( (DaoValue*) abtp->kernel );
				cycRefCountDecrements( abtp->nested );
				j += cycRefCountDecrementMapValue( abtp->interfaces );
				break;
			}
		case DAO_TYPEKERNEL :
			{
				DaoTypeKernel *kernel = (DaoTypeKernel*) value;
				cycRefCountDecrement( (DaoValue*) kernel->abtype );
				cycRefCountDecrement( (DaoValue*) kernel->nspace );
				j += cycRefCountDecrementMapValue( kernel->values );
				j += cycRefCountDecrementMapValue( kernel->methods );
				j += cycRefCountDecrementMapValue( kernel->instances );
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
				DaoGC_CycRefCountDecrements( vmp->stackValues, vmp->stackSize );
				j += vmp->stackSize;
				while( frame ){
					j += 3;
					cycRefCountDecrement( (DaoValue*) frame->routine );
					cycRefCountDecrement( (DaoValue*) frame->function );
					cycRefCountDecrement( (DaoValue*) frame->object );
					frame = frame->next;
				}
				break;
			}
		default: break;
		}
		if( (++j) >= gcWorker.gcMin ) break;
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
	size_t k = 0;
	size_t i = gcWorker.ii;
	DArray *workList = gcWorker.workList;
	DArray *auxList = gcWorker.auxList;

	if( gcWorker.jj ){
		k += DaoIGC_AliveObjectScan();
		if( gcWorker.jj ) return;
	}

	for( ; i<workList->size; i++ ){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.alive ) continue;
		if( value->type == DAO_CDATA && (value->xGC.refCount ==0 || value->xGC.cycRefCount ==0) ){
			DaoCdata *cdata = (DaoCdata*) value;
			DaoCdataCore *core = (DaoCdataCore*)cdata->ctype->kernel->core;
			if( !(cdata->attribs & DAO_CDATA_FREE) ) continue;
			if( cdata->data == NULL || core == NULL ) continue;
			if( core->DelTest == NULL ) continue;
			if( core->DelTest( cdata->data ) ) continue;
			DaoCdata_SetExtReference( cdata, 1 );
		}
		if( value->xGC.cycRefCount >0 ){
			auxList->size = 0;
			value->xGC.alive = 1;
			DArray_Append( auxList, value );
			k += DaoIGC_AliveObjectScan();
			if( gcWorker.jj || k >= gcWorker.gcMin ) break;
		}
	}
	if( i >= workList->size ){
		gcWorker.ii = 0;
		gcWorker.workType ++;
		if( gcWorker.workType == GC_DIR_DEC_RC ) DaoGC_PrintProfile( gcWorker.idleList, gcWorker.workList );
	}else{
		gcWorker.ii = i+1;
	}
}
int DaoIGC_AliveObjectScan()
{
	size_t i, k = 9;
	size_t j = gcWorker.jj;
	uchar_t delayMask = gcWorker.delayMask;
	DArray *auxList = gcWorker.auxList;
	DNode *node;

	for( ; j<auxList->size; j++){
		DaoValue *value = auxList->items.pValue[j];
		if( value->xGC.delay & delayMask ) continue;
		switch( value->type ){
		case DAO_ENUM :
			{
				DaoEnum *en = (DaoEnum*) value;
				cycRefCountIncrement( (DaoValue*) en->etype );
				break;
			}
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) value;
				cycRefCountIncrement( (DaoValue*) array->unitype );
				//cycRefCountIncrement( (DaoValue*) array->meta );
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple= (DaoTuple*) value;
				cycRefCountIncrement( (DaoValue*) tuple->unitype );
				//cycRefCountIncrement( (DaoValue*) tuple->meta );
				DaoGC_CycRefCountIncrements( tuple->items, tuple->size );
				k += tuple->size;
				break;
			}
		case DAO_LIST :
			{
				DaoList *list= (DaoList*) value;
				cycRefCountIncrement( (DaoValue*) list->unitype );
				cycRefCountIncrement( (DaoValue*) list->meta );
				cycRefCountIncrements( list->items );
				k += list->items->size;
				break;
			}
		case DAO_MAP :
			{
				DaoMap *map = (DaoMap*)value;
				cycRefCountIncrement( (DaoValue*) map->unitype );
				cycRefCountIncrement( (DaoValue*) map->meta );
				node = DMap_First( map->items );
				for( ; node != NULL; node = DMap_Next( map->items, node ) ){
					cycRefCountIncrement( node->key.pValue );
					cycRefCountIncrement( node->value.pValue );
				}
				k += map->items->size;
				break;
			}
		case DAO_OBJECT :
			{
				DaoObject *obj = (DaoObject*) value;
				if( obj->isRoot ){
					DaoGC_CycRefCountIncrements( obj->objValues, obj->valueCount );
					k += obj->valueCount;
				}
				k += obj->baseCount;
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
				break;
			}
		case DAO_FUNCTREE :
			{
				DaoFunctree *meta = (DaoFunctree*) value;
				cycRefCountIncrement( (DaoValue*) meta->space );
				cycRefCountIncrement( (DaoValue*) meta->host );
				cycRefCountIncrement( (DaoValue*) meta->unitype );
				cycRefCountIncrements( meta->routines );
				break;
			}
		case DAO_ROUTINE :
		case DAO_FUNCTION :
		case DAO_ABROUTINE :
			{
				DaoRoutine *rout = (DaoRoutine*) value;
				cycRefCountIncrement( (DaoValue*) rout->routType );
				cycRefCountIncrement( (DaoValue*) rout->routHost );
				cycRefCountIncrement( (DaoValue*) rout->nameSpace );
				cycRefCountIncrements( rout->routConsts );
				if( rout->type == DAO_ROUTINE ){
					k += rout->abstypes->size;
					cycRefCountIncrement( (DaoValue*) rout->upRoutine );
					cycRefCountIncrement( (DaoValue*) rout->upContext );
					cycRefCountIncrement( (DaoValue*) rout->original );
					cycRefCountIncrement( (DaoValue*) rout->specialized );
					cycRefCountIncrements( rout->regType );
					cycRefCountIncrementMapValue( rout->abstypes );
				}
				k += rout->routConsts->size;
				break;
			}
		case DAO_CLASS :
			{
				DaoClass *klass = (DaoClass*) value;
				cycRefCountIncrementMapValue( klass->abstypes );
				cycRefCountIncrement( (DaoValue*) klass->clsType );
				cycRefCountIncrement( (DaoValue*) klass->classRoutine );
				cycRefCountIncrements( klass->cstData );
				cycRefCountIncrements( klass->glbData );
				cycRefCountIncrements( klass->objDataDefault );
				cycRefCountIncrements( klass->superClass );
				cycRefCountIncrements( klass->objDataType );
				cycRefCountIncrements( klass->glbDataType );
				cycRefCountIncrements( klass->references );
				k += klass->cstData->size + klass->glbData->size;
				k += klass->cstData->size + klass->objDataDefault->size;
				k += klass->superClass->size + klass->abstypes->size;
				k += klass->objDataType->size + klass->glbDataType->size;
				k += klass->references->size + klass->abstypes->size;
				break;
			}
		case DAO_INTERFACE :
			{
				DaoInterface *inter = (DaoInterface*)value;
				cycRefCountIncrementMapValue( inter->methods );
				cycRefCountIncrements( inter->supers );
				cycRefCountIncrement( (DaoValue*) inter->abtype );
				k += inter->supers->size + inter->methods->size;
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNamespace *ns = (DaoNamespace*) value;
				cycRefCountIncrements( ns->cstData );
				cycRefCountIncrements( ns->varData );
				cycRefCountIncrements( ns->varType );
				cycRefCountIncrements( ns->auxData );
				cycRefCountIncrements( ns->mainRoutines );
				k += cycRefCountIncrementMapValue( ns->abstypes );
				k += ns->cstData->size + ns->varData->size + ns->abstypes->size;
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) value;
				cycRefCountIncrement( abtp->aux );
				cycRefCountIncrement( abtp->value );
				cycRefCountIncrement( (DaoValue*) abtp->kernel );
				cycRefCountIncrements( abtp->nested );
				k += cycRefCountIncrementMapValue( abtp->interfaces );
				break;
			}
		case DAO_TYPEKERNEL :
			{
				DaoTypeKernel *kernel = (DaoTypeKernel*) value;
				cycRefCountIncrement( (DaoValue*) kernel->abtype );
				cycRefCountIncrement( (DaoValue*) kernel->nspace );
				k += cycRefCountIncrementMapValue( kernel->values );
				k += cycRefCountIncrementMapValue( kernel->methods );
				k += cycRefCountIncrementMapValue( kernel->instances );
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
				DaoGC_CycRefCountIncrements( vmp->stackValues, vmp->stackSize );
				k += vmp->stackSize;
				while( frame ){
					k += 3;
					cycRefCountIncrement( (DaoValue*) frame->routine );
					cycRefCountIncrement( (DaoValue*) frame->function );
					cycRefCountIncrement( (DaoValue*) frame->object );
					frame = frame->next;
				}
				break;
			}
		default: break;
		}
		if( (++k) >= gcWorker.gcMin ) break;
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
	DNode *node;
	DArray *workList = gcWorker.workList;
	uchar_t delayMask = gcWorker.delayMask;
	size_t i = gcWorker.ii;
	size_t j = 0, k;

	for(; i<workList->size; i++, j++){
		DaoValue *value = workList->items.pValue[i];
		if( value->xGC.cycRefCount && value->xGC.refCount ) continue;
		if( value->xGC.delay & delayMask ) continue;
		switch( value->type ){
		case DAO_ENUM :
			{
				DaoEnum *en = (DaoEnum*) value;
				directRefCountDecrement( (DaoValue**) & en->etype );
				break;
			}
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) value;
				directRefCountDecrement( (DaoValue**) & array->unitype );
				//directRefCountDecrement( (DaoValue**) & array->meta );
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple = (DaoTuple*) value;
				j += tuple->size;
				directRefCountDecrement( (DaoValue**) & tuple->unitype );
				//directRefCountDecrement( (DaoValue**) & tuple->meta );
				DaoGC_RefCountDecrements( tuple->items, tuple->size );
				tuple->size = 0;
				break;
			}
		case DAO_LIST :
			{
				DaoList *list = (DaoList*) value;
				j += list->items->size;
				directRefCountDecrements( list->items );
				directRefCountDecrement( (DaoValue**) & list->unitype );
				directRefCountDecrement( (DaoValue**) & list->meta );
				break;
			}
		case DAO_MAP :
			{
				DaoMap *map = (DaoMap*) value;
				node = DMap_First( map->items );
				for( ; node != NULL; node = DMap_Next( map->items, node ) ){
					node->key.pValue->xGC.refCount --;
					node->value.pValue->xGC.refCount --;
					DaoGC_DeleteSimpleData( node->key.pValue );
					DaoGC_DeleteSimpleData( node->value.pValue );
				}
				j += map->items->size;
				map->items->keytype = map->items->valtype = 0;
				DMap_Clear( map->items );
				directRefCountDecrement( (DaoValue**) & map->unitype );
				directRefCountDecrement( (DaoValue**) & map->meta );
				break;
			}
		case DAO_OBJECT :
			{
				DaoObject *obj = (DaoObject*) value;
				if( obj->isRoot ){
					DaoGC_RefCountDecrements( obj->objValues, obj->valueCount );
					j += obj->valueCount;
				}
				j += obj->baseCount;
				DaoGC_RefCountDecrements( obj->parents, obj->baseCount );
				directRefCountDecrement( (DaoValue**) & obj->rootObject );
				directRefCountDecrement( (DaoValue**) & obj->defClass );
				obj->valueCount = obj->baseCount = 0;
				break;
			}
		case DAO_CDATA : case DAO_CTYPE :
			{
				DaoCdata *cdata = (DaoCdata*) value;
				DaoTypeBase *typer = cdata->typer;
				/* Do not use ctype->kernel, since this reference may have been broken: */
				value = (DaoValue*) (typer && typer->core ? typer->core->kernel : NULL);
				if( value && value->xGC.idle ==0 ){
					/* To make sure that the DaoTypeKernel will be deleted after: */
					value->xGC.idle = 1;
					DArray_Append( gcWorker.idleList, value );
				}
				directRefCountDecrement( (DaoValue**) & cdata->object );
				directRefCountDecrement( (DaoValue**) & cdata->ctype );
				break;
			}
		case DAO_FUNCTREE :
			{
				DaoFunctree *meta = (DaoFunctree*) value;
				j += meta->routines->size;
				directRefCountDecrement( (DaoValue**) & meta->space );
				directRefCountDecrement( (DaoValue**) & meta->host );
				directRefCountDecrement( (DaoValue**) & meta->unitype );
				directRefCountDecrements( meta->routines );
				break;
			}
		case DAO_ROUTINE :
		case DAO_FUNCTION :
		case DAO_ABROUTINE :
			{
				DaoRoutine *rout = (DaoRoutine*)value;
				directRefCountDecrement( (DaoValue**) & rout->nameSpace );
				/* may become NULL, if it has already become garbage 
				 * in the last cycle */
				directRefCountDecrement( (DaoValue**) & rout->routType );
				/* may become NULL, if it has already become garbage 
				 * in the last cycle */
				directRefCountDecrement( (DaoValue**) & rout->routHost );

				j += rout->routConsts->size;
				directRefCountDecrements( rout->routConsts );
				if( rout->type == DAO_ROUTINE ){
					j += rout->abstypes->size;
					directRefCountDecrement( (DaoValue**) & rout->upRoutine );
					directRefCountDecrement( (DaoValue**) & rout->upContext );
					directRefCountDecrement( (DaoValue**) & rout->original );
					directRefCountDecrement( (DaoValue**) & rout->specialized );
					directRefCountDecrements( rout->regType );
					directRefCountDecrementMapValue( rout->abstypes );
				}
				break;
			}
		case DAO_CLASS :
			{
				DaoClass *klass = (DaoClass*)value;
				j += klass->cstData->size + klass->glbData->size;
				j += klass->cstData->size + klass->objDataDefault->size;
				j += klass->superClass->size + klass->abstypes->size;
				j += klass->objDataType->size + klass->glbDataType->size;
				j += klass->references->size + klass->abstypes->size;
				j += directRefCountDecrementMapValue( klass->abstypes );
				directRefCountDecrement( (DaoValue**) & klass->clsType );
				directRefCountDecrement( (DaoValue**) & klass->classRoutine );
				directRefCountDecrements( klass->cstData );
				directRefCountDecrements( klass->glbData );
				directRefCountDecrements( klass->objDataDefault );
				directRefCountDecrements( klass->superClass );
				directRefCountDecrements( klass->objDataType );
				directRefCountDecrements( klass->glbDataType );
				directRefCountDecrements( klass->references );
				break;
			}
		case DAO_INTERFACE :
			{
				DaoInterface *inter = (DaoInterface*)value;
				j += inter->supers->size + inter->methods->size;
				j += directRefCountDecrementMapValue( inter->methods );
				directRefCountDecrements( inter->supers );
				directRefCountDecrement( (DaoValue**) & inter->abtype );
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNamespace *ns = (DaoNamespace*) value;
				j += ns->cstData->size + ns->varData->size + ns->abstypes->size;
				j += directRefCountDecrementMapValue( ns->abstypes );
				directRefCountDecrements( ns->cstData );
				directRefCountDecrements( ns->varData );
				directRefCountDecrements( ns->varType );
				directRefCountDecrements( ns->auxData );
				directRefCountDecrements( ns->mainRoutines );
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) value;
				directRefCountDecrements( abtp->nested );
				directRefCountDecrement( (DaoValue**) & abtp->aux );
				directRefCountDecrement( (DaoValue**) & abtp->value );
				directRefCountDecrement( (DaoValue**) & abtp->kernel );
				j += directRefCountDecrementMapValue( abtp->interfaces );
				break;
			}
		case DAO_TYPEKERNEL :
			{
				DaoTypeKernel *kernel = (DaoTypeKernel*) value;
				directRefCountDecrement( (DaoValue**) & kernel->abtype );
				directRefCountDecrement( (DaoValue**) & kernel->nspace );
				j += directRefCountDecrementMapValue( kernel->values );
				j += directRefCountDecrementMapValue( kernel->methods );
				j += directRefCountDecrementMapValue( kernel->instances );
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
				DaoGC_RefCountDecrements( vmp->stackValues, vmp->stackSize );
				j += vmp->stackSize;
				vmp->stackSize = 0;
				while( frame ){
					j += 3;
					if( frame->routine ) frame->routine->refCount --;
					if( frame->function ) frame->function->refCount --;
					if( frame->object ) frame->object->refCount --;
					frame->routine = NULL;
					frame->function = NULL;
					frame->object = NULL;
					frame = frame->next;
				}
				break;
			}
		default: break;
		}
		if( j >= gcWorker.gcMin ) break;
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
	size_t i = gcWorker.ii;
	size_t j = 0;
	size_t old;

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
		if( j >= gcWorker.gcMin ) break;
	}
	if( i >= workList->size ){
		gcWorker.ii = 0;
		gcWorker.workType = GC_RESET_RC;
		gcWorker.count = 0;
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
void DaoGC_CycRefCountDecrements( DaoValue **values, size_t size )
{
	size_t i;
	for(i=0; i<size; i++) cycRefCountDecrement( values[i] );
}
void DaoGC_CycRefCountIncrements( DaoValue **values, size_t size )
{
	size_t i;
	for(i=0; i<size; i++) cycRefCountIncrement( values[i] );
}
void DaoGC_RefCountDecrements( DaoValue **values, size_t size )
{
	size_t i;
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
int cycRefCountDecrementMapValue( DMap *dmap )
{
	DNode *it;
	if( dmap == NULL ) return 0;
	for(it = DMap_First( dmap ); it != NULL; it = DMap_Next( dmap, it ) )
		cycRefCountDecrement( it->value.pValue );
	return dmap->size;
}
int cycRefCountIncrementMapValue( DMap *dmap )
{
	DNode *it;
	if( dmap == NULL ) return 0;
	for( it = DMap_First( dmap ); it != NULL; it = DMap_Next( dmap, it ) )
		cycRefCountIncrement( it->value.pValue );
	return dmap->size;
}
int directRefCountDecrementMapValue( DMap *dmap )
{
	int n;
	DNode *it;
	if( dmap == NULL ) return 0;
	for( it = DMap_First( dmap ); it != NULL; it = DMap_Next( dmap, it ) ){
		DaoValue *p = it->value.pValue;
		if( p == NULL ) continue;
		p->xGC.refCount --;
		if( p->xGC.refCount == 0 && p->type < DAO_ENUM ) DaoGC_DeleteSimpleData( p );
	}
	n = dmap->size;
	dmap->valtype = 0;
	DMap_Clear( dmap );
	return n;
}

