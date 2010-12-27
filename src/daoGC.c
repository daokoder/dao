/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2010, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#include"daoGC.h"
#include"daoMap.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoNumtype.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoRoutine.h"
#include"daoNamespace.h"
#include"daoThread.h"

void GC_Lock();
void GC_Unlock();

DArray *dao_callback_data = NULL;

DaoCallbackData* DaoCallbackData_New( DaoRoutine *callback, DValue userdata )
{
	DaoCallbackData *self;
	if( callback == NULL || callback->type != DAO_ROUTINE ) return NULL;
	self = (DaoCallbackData*) calloc( 1, sizeof(DaoCallbackData) );
	self->callback = callback;
	if( userdata.t >= DAO_ENUM )
		self->userdata = userdata;
	else
		DValue_Copy( & self->userdata, userdata );
	GC_Lock();
	DArray_Append( dao_callback_data, self );
	GC_Unlock();
	return self;
}
static void DaoCallbackData_Delete( DaoCallbackData *self )
{
	if( self->userdata.t < DAO_ENUM ) DValue_Clear( & self->userdata );
	dao_free( self );
}
static void DaoCallbackData_DeleteByCallback( DaoRoutine *callback )
{
	DaoCallbackData *cd = NULL;
	int i;
	if( dao_callback_data->size ==0 ) return;
	GC_Lock();
	for(i=0; i<dao_callback_data->size; i++){
		cd = (DaoCallbackData*) dao_callback_data->items.pBase[i];
		if( cd->callback == callback ){
			DaoCallbackData_Delete( cd );
			DArray_Erase( dao_callback_data, i, 1 );
			i--;
		}
	}
	GC_Unlock();
}
static void DaoCallbackData_DeleteByUserdata( DaoBase *userdata )
{
	DaoCallbackData *cd = NULL;
	int i;
	if( userdata == NULL ) return;
	if( dao_callback_data->size ==0 ) return;
	GC_Lock();
	for(i=0; i<dao_callback_data->size; i++){
		cd = (DaoCallbackData*) dao_callback_data->items.pBase[i];
		if( cd->userdata.t != userdata->type ) continue;
		if( cd->userdata.v.p == userdata ){
			DaoCallbackData_Delete( cd );
			DArray_Erase( dao_callback_data, i, 1 );
			i--;
		}
	}
	GC_Unlock();
}


#define GC_IN_POOL 1
#define GC_MARKED  2

#if DEBUG
#if 0
#define DEBUG_TRACE
#endif
#endif

#ifdef DEBUG_TRACE
#include <execinfo.h>
void print_trace()
{
	void  *array[100];
	size_t i, size = backtrace (array, 100);
	char **strings = backtrace_symbols (array, size);
	FILE *debug = fopen( "debug.txt", "w+" );
	fprintf (debug, "===========================================\n");
	fprintf (debug, "Obtained %zd stack frames.\n", size);
	printf ("===========================================\n");
	printf ("Obtained %zd stack frames.\n", size);
	for (i = 0; i < size; i++){
		printf ("%s\n", strings[i]);
		fprintf (debug,"%s\n", strings[i]);
	}
	/* comment this to cause leaking, so that valgrind will print the trace with line numbers */
	free (strings);
	fflush( debug );
	fclose( debug );
	fflush( stdout );
}
#endif

#define GC_BREAK_REF( p ) { if( p ){ (p)->refCount --; (p) = NULL; } }

static void DaoGC_DecRC2( DaoBase *p, int change );
static void cycRefCountDecrement( DaoBase *dbase );
static void cycRefCountIncrement( DaoBase *dbase );
static void cycRefCountDecrements( DArray * dbases );
static void cycRefCountIncrements( DArray * dbases );
static void directRefCountDecrement( DArray * dbases );
static void cycRefCountDecrementV( DValue value );
static void cycRefCountIncrementV( DValue value );
static void directRefCountDecrementValue( DValue *value );
static void cycRefCountDecrementsV( DVarray * values );
static void cycRefCountIncrementsV( DVarray * values );
static void directRefCountDecrementV( DVarray * values );
static void cycRefCountDecrementsT( DPtrTuple * values );
static void cycRefCountIncrementsT( DPtrTuple * values );
static void directRefCountDecrementT( DPtrTuple * values );
static void cycRefCountDecrementsVT( DVaTuple * values );
static void cycRefCountIncrementsVT( DVaTuple * values );
static void directRefCountDecrementVT( DVaTuple * values );
static void cycRefCountDecrementMapValue( DMap *dmap );
static void cycRefCountIncrementMapValue( DMap *dmap );
static void directRefCountDecrementMapValue( DMap *dmap );
static void cycRefCountDecrementMapValueV( DMap *dmap );
static void cycRefCountIncrementMapValueV( DMap *dmap );
static void directRefCountDecrementMapValueV( DMap *dmap );
static void cycRefCountDecreScan();
static void cycRefCountIncreScan();
static void freeGarbage();
static void InitGC();

#ifdef DAO_WITH_THREAD
static void markAliveObjects( DaoBase *root );
static void recycleGarbage( void * );
static void tryInvoke();
#endif

static void DaoLateDeleter_Init();
static void DaoLateDeleter_Finish();
static void DaoLateDeleter_Update();

struct DaoGarbageCollector
{
	DArray   *pool[2];
	DArray   *objAlive;

	short     work, idle;
	int       gcMin, gcMax;
	int       count, boundary;
	int       ii, jj;
	short     busy;
	short     locked;
	short     workType;
	short     finalizing;

#ifdef DAO_WITH_THREAD
	DThread   thread;

	DMutex    mutex_switch_heap;
	DMutex    mutex_start_gc;
	DMutex    mutex_block_mutator;

	DCondVar  condv_start_gc;
	DCondVar  condv_block_mutator;
#endif
};
static DaoGarbageCollector gcWorker = { { NULL, NULL }, NULL };

extern int ObjectProfile[100];
extern int daoCountMBS;
extern int daoCountArray;

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

void InitGC()
{
	if( gcWorker.objAlive != NULL ) return;
	DaoLateDeleter_Init();

	gcWorker.pool[0] = DArray_New(0);
	gcWorker.pool[1] = DArray_New(0);
	gcWorker.objAlive = DArray_New(0);

	gcWorker.idle = 0;
	gcWorker.work = 1;
	gcWorker.finalizing = 0;

	gcWorker.gcMin = 1000;
	gcWorker.gcMax = 100 * gcWorker.gcMin;
	gcWorker.count = 0;
	gcWorker.workType = 0;
	gcWorker.ii = 0;
	gcWorker.jj = 0;
	gcWorker.busy = 0;
	gcWorker.locked = 0;

#ifdef DAO_WITH_THREAD
	DThread_Init( & gcWorker.thread );
	DMutex_Init( & gcWorker.mutex_switch_heap );
	DMutex_Init( & gcWorker.mutex_start_gc );
	DMutex_Init( & gcWorker.mutex_block_mutator );
	DCondVar_Init( & gcWorker.condv_start_gc );
	DCondVar_Init( & gcWorker.condv_block_mutator );
#endif
}
void DaoStartGC()
{
	InitGC();
#ifdef DAO_WITH_THREAD
	DThread_Start( & gcWorker.thread, recycleGarbage, NULL );
#endif
}
static int DValue_Size( DValue *self, int depth )
{
	DMap *map;
	DNode *it;
	int i, m, n = 0;
	switch( self->t ){
	case DAO_NIL :
	case DAO_INTEGER :
	case DAO_FLOAT :
	case DAO_DOUBLE :
		n = 1;
		break;
	case DAO_LONG :
		n = self->v.l->size;
		break;
	case DAO_COMPLEX :
		n = 2;
		break;
	case DAO_STRING :
		n = DString_Size( self->v.s );
		break;
	case DAO_ARRAY :
		n = self->v.array->size;
		break;
	case DAO_LIST :
		n = m = self->v.list->items->size;
		if( (-- depth) <=0 ) break;
		for(i=0; i<m; i++){
			n += DValue_Size( self->v.list->items->data + i, depth );
			if( n > gcWorker.gcMin ) break;
		}
		break;
	case DAO_TUPLE :
		n = m = self->v.tuple->items->size;
		if( (-- depth) <=0 ) break;
		for(i=0; i<m; i++){
			n += DValue_Size( self->v.tuple->items->data + i, depth );
			if( n > gcWorker.gcMin ) break;
		}
		break;
	case DAO_MAP :
		map = self->v.map->items;
		n = map->size;
		if( (-- depth) <=0 ) break;
		for(it=DMap_First( map ); it; it=DMap_Next( map, it ) ){
			n += DValue_Size( it->key.pValue, depth );
			n += DValue_Size( it->value.pValue, depth );
			if( n > gcWorker.gcMin ) break;
		}
		break;
	case DAO_OBJECT :
		if( self->v.object->objData == NULL ) break;
		n = m = self->v.object->objData->size;
		if( (-- depth) <=0 ) break;
		for(i=1; i<m; i++){ /* skip self object */
			n += DValue_Size( self->v.object->objValues + i, depth );
			if( n > gcWorker.gcMin ) break;
		}
		break;
	case DAO_CONTEXT :
		n = m = self->v.context->regArray->size;
		if( (-- depth) <=0 ) break;
		for(i=0; i<m; i++){
			n += DValue_Size( self->v.context->regArray->data + i, depth );
			if( n > gcWorker.gcMin ) break;
		}
		break;
	default : n = 1; break;
	}
	return n;
}
static void DaoGC_DecRC2( DaoBase *p, int change )
{
	DaoType *tp, *tp2;
	DNode *node;
	const short idle = gcWorker.idle;
	int i, n;
	p->refCount += change;

	if( p->refCount == 0 ){
		DValue value = daoNullValue;
		value.t = p->type;
		value.v.p = p;
		/* free some memory, but do not delete it here,
		 * because it may be a member of DValue,
		 * and the DValue.t is not yet set to zero. */
		switch( p->type ){
		case DAO_LIST :
			{
				DaoList *list = (DaoList*) p;
				n = list->items->size;
				if( list->unitype && list->unitype->nested->size ){
					tp = list->unitype->nested->items.pType[0];
					for(i=0; i<n; i++){
						DValue *it = list->items->data + i;
						if( it->t > DAO_DOUBLE && it->t < DAO_ENUM ) DValue_Clear( it );
					}
				}
				break;
			}
		case DAO_TUPLE :
			{
				DaoTuple *tuple = (DaoTuple*) p;
				n = tuple->items->size;
				for(i=0; i<n; i++){
					DValue *it = tuple->items->data + i;
					if( it->t > DAO_DOUBLE && it->t < DAO_ENUM ) DValue_Clear( it );
				}
				break;
			}
		case DAO_MAP :
			{
				DaoMap *map = (DaoMap*) p;
				tp2 = map->unitype;
				if( tp2 == NULL || tp2->nested->size != 2 ) break;
				tp = tp2->nested->items.pType[0];
				tp2 = tp2->nested->items.pType[1];
				if( tp->tid >DAO_DOUBLE && tp2->tid >DAO_DOUBLE && tp->tid <DAO_ENUM && tp2->tid <DAO_ENUM ){
					/* DaoMap_Clear( map ); this is NOT thread safe, there is RC update scan */
					i = 0;
					node = DMap_First( map->items );
					for( ; node != NULL; node = DMap_Next( map->items, node ) ) {
						DValue_Clear( node->key.pValue );
						DValue_Clear( node->value.pValue );
						node->key.pValue->t = DAO_INTEGER;
						node->key.pValue->v.i = ++ i; /* keep key ordering */
					}
				}else if( tp2->tid >DAO_DOUBLE && tp2->tid < DAO_ENUM ){
					node = DMap_First( map->items );
					for( ; node != NULL; node = DMap_Next( map->items, node ) ) {
						DValue_Clear( node->value.pValue );
					}
				}else if( tp->tid >DAO_DOUBLE && tp->tid < DAO_ENUM ){
					i = 0;
					node = DMap_First( map->items );
					for( ; node != NULL; node = DMap_Next( map->items, node ) ) {
						DValue_Clear( node->key.pValue );
						node->key.pValue->t = DAO_INTEGER;
						node->key.pValue->v.i = ++ i; /* keep key ordering */
					}
				}
				break;
			}
		case DAO_ARRAY :
			{
#ifdef DAO_WITH_NUMARRAY
				DaoArray *array = (DaoArray*) p;
				DaoArray_ResizeVector( array, 0 );
#endif
				break;
			}
#if 0
		case DAO_OBJECT :
			{
				DaoObject *object = (DaoObject*) p;
				if( object->objData == NULL ) break;
				n = object->objData->size;
				m = 0;
				for(i=0; i<n; i++){
					DValue *it = object->objValues + i;
					if( it->t && it->t < DAO_ARRAY ){
						DValue_Clear( it );
						m ++;
					}
				}
				if( m == n ) DVaTuple_Clear( object->objData );
				break;
			}
#endif
		case DAO_CONTEXT :
			{
				DaoContext *ctx = (DaoContext*) p;
				if( ctx->regValues ) dao_free( ctx->regValues );
				ctx->regValues = NULL;
				n = ctx->regArray->size;
				for(i=0; i<n; i++){
					DValue *it = ctx->regArray->data + i;
					if( it->t > DAO_DOUBLE && it->t < DAO_ENUM ) DValue_Clear( it );
				}
				break;
			}
		default : break;
		}
#if 0
		gcWorker.count += DValue_Size( & value, 100 );
#endif
	}
	/* already in idle pool */
	if( p->gcState[ idle ] & GC_IN_POOL ) return;
	p->gcState[ idle ] = GC_IN_POOL;
	DArray_Append( gcWorker.pool[ idle ], p );
#if 0
	if( p->refCount ==0 ) return; /* already counted */
#endif
	switch( p->type ){
	case DAO_LIST :
		{
			DaoList *list = (DaoList*) p;
			gcWorker.count += list->items->size + 1;
			break;
		}
	case DAO_MAP :
		{
			DaoMap *map = (DaoMap*) p;
			gcWorker.count += map->items->size + 1;
			break;
		}
	case DAO_OBJECT :
		{
			DaoObject *obj = (DaoObject*) p;
			if( obj->objData ) gcWorker.count += obj->objData->size + 1;
			break;
		}
	case DAO_ARRAY :
		{
#ifdef DAO_WITH_NUMARRAY
			DaoArray *array = (DaoArray*) p;
			gcWorker.count += array->size + 1;
#endif
			break;
		}
	case DAO_TUPLE :
		{
			DaoTuple *tuple = (DaoTuple*) p;
			gcWorker.count += tuple->items->size;
			break;
		}
	case DAO_CONTEXT :
		{
			DaoContext *ctx = (DaoContext*) p;
			gcWorker.count += ctx->regArray->size;
			break;
		}
	default: gcWorker.count += 1; break;
	}
}
#ifdef DAO_WITH_THREAD

void GC_Lock()
{
	DMutex_Lock( & gcWorker.mutex_switch_heap );
}
void GC_Unlock()
{
	DMutex_Unlock( & gcWorker.mutex_switch_heap );
}

/* Concurrent Garbage Collector */

void DaoGC_DecRC( DaoBase *p )
{
	const short idle = gcWorker.idle;
	DaoTypeBase *typer;
	if( ! p ) return;

#ifdef DEBUG_TRACE
	if( p == 0x2531c40 ){
		print_trace();
		printf( "rc: %i\n", p->refCount );
	}
#endif
	DMutex_Lock( & gcWorker.mutex_switch_heap );

	DaoGC_DecRC2( p, -1 );

	DMutex_Unlock( & gcWorker.mutex_switch_heap );

	if( gcWorker.pool[ idle ]->size > gcWorker.gcMin ) tryInvoke();
}
void DaoFinishGC()
{
	gcWorker.gcMin = 0;
	gcWorker.finalizing = 1;
	tryInvoke();
	DThread_Join( & gcWorker.thread );

	DArray_Delete( gcWorker.pool[0] );
	DArray_Delete( gcWorker.pool[1] );
	DArray_Delete( gcWorker.objAlive );
	DaoLateDeleter_Finish();
	gcWorker.objAlive = NULL;
}
void DaoGC_IncRC( DaoBase *p )
{
	if( ! p ) return;
#ifdef DEBUG_TRACE
	if( p == 0x1011a0 ) print_trace();
#endif
	if( p->refCount == 0 ){
		p->refCount ++;
		return;
	}

	DMutex_Lock( & gcWorker.mutex_switch_heap );
	p->refCount ++;
	p->cycRefCount ++;
	DMutex_Unlock( & gcWorker.mutex_switch_heap );
}
void DaoGC_ShiftRC( DaoBase *up, DaoBase *down )
{
	const short idle = gcWorker.idle;
	if( up == down ) return;

	DMutex_Lock( & gcWorker.mutex_switch_heap );

	if( up ){
		up->refCount ++;
		up->cycRefCount ++;
	}
	if( down ) DaoGC_DecRC2( down, -1 );

	DMutex_Unlock( & gcWorker.mutex_switch_heap );

	if( down && gcWorker.pool[ idle ]->size > gcWorker.gcMin ) tryInvoke();
}

void DaoGC_IncRCs( DArray *list )
{
	size_t i;
	DaoBase **dbases;

	if( list->size == 0 ) return;
	dbases = list->items.pBase;
	DMutex_Lock( & gcWorker.mutex_switch_heap );
	for( i=0; i<list->size; i++){
		if( dbases[i] ){
			dbases[i]->refCount ++;
			dbases[i]->cycRefCount ++;
		}
	}
	DMutex_Unlock( & gcWorker.mutex_switch_heap );
}
void DaoGC_DecRCs( DArray *list )
{
	size_t i;
	DaoBase **dbases;
	const short idle = gcWorker.idle;
	if( list==NULL || list->size == 0 ) return;
	dbases = list->items.pBase;
	DMutex_Lock( & gcWorker.mutex_switch_heap );
	for( i=0; i<list->size; i++) if( dbases[i] ) DaoGC_DecRC2( dbases[i], -1 );
	DMutex_Unlock( & gcWorker.mutex_switch_heap );
	if( gcWorker.pool[ idle ]->size > gcWorker.gcMin ) tryInvoke();
}

void tryInvoke()
{
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

void recycleGarbage( void *p )
{
	while(1){
		if( gcWorker.finalizing && (gcWorker.pool[0]->size + gcWorker.pool[1]->size) ==0 ) break;
		if( ! gcWorker.finalizing ){
			DMutex_Lock( & gcWorker.mutex_block_mutator );
			DCondVar_BroadCast( & gcWorker.condv_block_mutator );
			DMutex_Unlock( & gcWorker.mutex_block_mutator );

			DMutex_Lock( & gcWorker.mutex_start_gc );
			DCondVar_TimedWait( & gcWorker.condv_start_gc, & gcWorker.mutex_start_gc, 0.1 );
			DMutex_Unlock( & gcWorker.mutex_start_gc );
			if( gcWorker.count < gcWorker.gcMin ) continue;
		}
		DaoLateDeleter_Update();

		DMutex_Lock( & gcWorker.mutex_switch_heap );
		gcWorker.count = 0;
		gcWorker.work = gcWorker.idle;
		gcWorker.idle = ! gcWorker.work;
		DMutex_Unlock( & gcWorker.mutex_switch_heap );

		cycRefCountDecreScan();
		cycRefCountIncreScan();
		freeGarbage();

	}
	DThread_Exit( & gcWorker.thread );
}

void cycRefCountDecreScan()
{
	DArray *pool = gcWorker.pool[ gcWorker.work ];
	DNode *node;
	size_t i, k;
	for( i=0; i<pool->size; i++ )
		pool->items.pBase[i]->cycRefCount = pool->items.pBase[i]->refCount;

	for( i=0; i<pool->size; i++ ){
		DaoBase *dbase = pool->items.pBase[i];
		switch( dbase->type ){
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) dbase;
				cycRefCountDecrement( (DaoBase*) array->unitype );
				cycRefCountDecrement( (DaoBase*) array->meta );
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple = (DaoTuple*) dbase;
				cycRefCountDecrement( (DaoBase*) tuple->unitype );
				cycRefCountDecrement( (DaoBase*) tuple->meta );
				cycRefCountDecrementsVT( tuple->items );
				break;
			}
		case DAO_LIST :
			{
				DaoList *list = (DaoList*) dbase;
				cycRefCountDecrement( (DaoBase*) list->unitype );
				cycRefCountDecrement( (DaoBase*) list->meta );
				cycRefCountDecrementsV( list->items );
				break;
			}
		case DAO_MAP :
			{
				DaoMap *map = (DaoMap*) dbase;
				cycRefCountDecrement( (DaoBase*) map->unitype );
				cycRefCountDecrement( (DaoBase*) map->meta );
				node = DMap_First( map->items );
				for( ; node != NULL; node = DMap_Next( map->items, node ) ) {
					cycRefCountDecrementV( node->key.pValue[0] );
					cycRefCountDecrementV( node->value.pValue[0] );
				}
				break;
			}
		case DAO_OBJECT :
			{
				DaoObject *obj = (DaoObject*) dbase;
				cycRefCountDecrementsT( obj->superObject );
				cycRefCountDecrementsVT( obj->objData );
				cycRefCountDecrement( (DaoBase*) obj->meta );
				cycRefCountDecrement( (DaoBase*) obj->myClass );
				break;
			}
		case DAO_CDATA :
			{
				DaoCData *cdata = (DaoCData*) dbase;
				cycRefCountDecrement( (DaoBase*) cdata->meta );
				cycRefCountDecrement( (DaoBase*) cdata->daoObject );
				break;
			}
		case DAO_ROUTINE :
		case DAO_FUNCTION :
			{
				DaoRoutine *rout = (DaoRoutine*)dbase;
				cycRefCountDecrement( (DaoBase*) rout->routType );
				cycRefCountDecrement( (DaoBase*) rout->routHost );
				cycRefCountDecrement( (DaoBase*) rout->nameSpace );
				cycRefCountDecrementsV( rout->routConsts );
				cycRefCountDecrements( rout->routTable );
				if( rout->type == DAO_ROUTINE && rout->minimal ==0 ){
					cycRefCountDecrement( (DaoBase*) rout->upRoutine );
					cycRefCountDecrement( (DaoBase*) rout->upContext );
					cycRefCountDecrement( (DaoBase*) rout->original );
					cycRefCountDecrements( rout->specialized );
					cycRefCountDecrements( rout->regType );
					cycRefCountDecrementMapValue( rout->abstypes );
				}
				break;
			}
		case DAO_CLASS :
			{
				DaoClass *klass = (DaoClass*)dbase;
				cycRefCountDecrementMapValue(klass->abstypes  );
				cycRefCountDecrement( (DaoBase*) klass->clsType );
				cycRefCountDecrementsV( klass->cstData );
				cycRefCountDecrementsV( klass->glbData );
				cycRefCountDecrements( klass->superClass );
				cycRefCountDecrements( klass->objDataType );
				cycRefCountDecrements( klass->glbDataType );
				cycRefCountDecrements( klass->references );
				break;
			}
		case DAO_INTERFACE :
			{
				DaoInterface *inter = (DaoInterface*)dbase;
				cycRefCountDecrementMapValue( inter->methods );
				cycRefCountDecrements( inter->supers );
				cycRefCountDecrement( (DaoBase*) inter->abtype );
				break;
			}
		case DAO_CONTEXT :
			{
				DaoContext *ctx = (DaoContext*)dbase;
				cycRefCountDecrement( (DaoBase*) ctx->object );
				cycRefCountDecrement( (DaoBase*) ctx->routine );
				cycRefCountDecrementsVT( ctx->regArray );
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNameSpace *ns = (DaoNameSpace*) dbase;
				cycRefCountDecrementsV( ns->cstData );
				cycRefCountDecrementsV( ns->varData );
				cycRefCountDecrements( ns->varType );
				cycRefCountDecrements( ns->cmodule->cmethods );
				cycRefCountDecrements( ns->mainRoutines );
				cycRefCountDecrementMapValue( ns->abstypes );
				for(k=0; k<ns->cmodule->ctypers->size; k++){
					DaoTypeBase *typer = (DaoTypeBase*)ns->cmodule->ctypers->items.pBase[k];
					if( typer->priv == NULL ) continue;
					cycRefCountDecrementMapValueV( typer->priv->values );
					cycRefCountDecrementMapValue( typer->priv->methods );
				}
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) dbase;
				cycRefCountDecrement( abtp->X.extra );
				cycRefCountDecrements( abtp->nested );
				if( abtp->interfaces ){
					node = DMap_First( abtp->interfaces );
					for( ; node != NULL; node = DMap_Next( abtp->interfaces, node ) )
						cycRefCountDecrement( node->key.pBase );
				}
				break;
			}
		case DAO_FUTURE :
			{
				DaoFuture *future = (DaoFuture*) dbase;
				cycRefCountDecrementV( future->value );
				cycRefCountDecrement( (DaoBase*) future->unitype );
				cycRefCountDecrement( (DaoBase*) future->context );
				cycRefCountDecrement( (DaoBase*) future->process );
				cycRefCountDecrement( (DaoBase*) future->precondition );
				break;
			}
		case DAO_VMPROCESS :
			{
				DaoVmProcess *vmp = (DaoVmProcess*) dbase;
				DaoVmFrame *frame = vmp->firstFrame;
				cycRefCountDecrementV( vmp->returned );
				cycRefCountDecrementsV( vmp->parResume );
				cycRefCountDecrementsV( vmp->parYield );
				cycRefCountDecrementsV( vmp->exceptions );
				while( frame ){
					cycRefCountDecrement( (DaoBase*) frame->context );
					frame = frame->next;
				}
				break;
			}
		default: break;
		}
	}
}
void cycRefCountIncreScan()
{
	const short work = gcWorker.work;
	DArray *pool = gcWorker.pool[ gcWorker.work ];
	size_t i, j;

	for(j=0; j<2; j++){
		for( i=0; i<pool->size; i++ ){
			DaoBase *dbase = pool->items.pBase[i];
			if( dbase->gcState[work] & GC_MARKED ) continue;
			if( dbase->type == DAO_CDATA && (dbase->refCount ==0 || dbase->cycRefCount ==0) ){
				DaoCData *cdata = (DaoCData*) dbase;
				DaoCDataCore *core = (DaoCDataCore*)cdata->typer->priv;
				if( !(cdata->attribs & DAO_CDATA_FREE) ) continue;
				if( cdata->data == NULL || core == NULL ) continue;
				if( core->DelTest == NULL ) continue;
				if( core->DelTest( cdata->data ) ) continue;
				DaoCData_SetExtReference( cdata, 1 );
			}
			if( dbase->cycRefCount >0 ) markAliveObjects( dbase );
		}
	}
}
void markAliveObjects( DaoBase *root )
{
	const short work = gcWorker.work;
	DNode *node;
	size_t i, k;
	DArray *objAlive = gcWorker.objAlive;
	objAlive->size = 0;
	root->gcState[work] |= GC_MARKED;
	DArray_Append( objAlive, root );

	for( i=0; i<objAlive->size; i++){
		DaoBase *dbase = objAlive->items.pBase[i];
		switch( dbase->type ){
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) dbase;
				cycRefCountIncrement( (DaoBase*) array->unitype );
				cycRefCountIncrement( (DaoBase*) array->meta );
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple= (DaoTuple*) dbase;
				cycRefCountIncrement( (DaoBase*) tuple->unitype );
				cycRefCountIncrement( (DaoBase*) tuple->meta );
				cycRefCountIncrementsVT( tuple->items );
				break;
			}
		case DAO_LIST :
			{
				DaoList *list= (DaoList*) dbase;
				cycRefCountIncrement( (DaoBase*) list->unitype );
				cycRefCountIncrement( (DaoBase*) list->meta );
				cycRefCountIncrementsV( list->items );
				break;
			}
		case DAO_MAP :
			{
				DaoMap *map = (DaoMap*)dbase;
				cycRefCountIncrement( (DaoBase*) map->unitype );
				cycRefCountIncrement( (DaoBase*) map->meta );
				node = DMap_First( map->items );
				for( ; node != NULL; node = DMap_Next( map->items, node ) ){
					cycRefCountIncrementV( node->key.pValue[0] );
					cycRefCountIncrementV( node->value.pValue[0] );
				}
				break;
			}
		case DAO_OBJECT :
			{
				DaoObject *obj = (DaoObject*) dbase;
				cycRefCountIncrementsT( obj->superObject );
				cycRefCountIncrementsVT( obj->objData );
				cycRefCountIncrement( (DaoBase*) obj->meta );
				cycRefCountIncrement( (DaoBase*) obj->myClass );
				break;
			}
		case DAO_CDATA :
			{
				DaoCData *cdata = (DaoCData*) dbase;
				cycRefCountIncrement( (DaoBase*) cdata->meta );
				cycRefCountIncrement( (DaoBase*) cdata->daoObject );
				break;
			}
		case DAO_ROUTINE :
		case DAO_FUNCTION :
			{
				DaoRoutine *rout = (DaoRoutine*) dbase;
				cycRefCountIncrement( (DaoBase*) rout->routType );
				cycRefCountIncrement( (DaoBase*) rout->routHost );
				cycRefCountIncrement( (DaoBase*) rout->nameSpace );
				cycRefCountIncrementsV( rout->routConsts );
				cycRefCountIncrements( rout->routTable );
				if( rout->type == DAO_ROUTINE && rout->minimal ==0 ){
					cycRefCountIncrement( (DaoBase*) rout->upRoutine );
					cycRefCountIncrement( (DaoBase*) rout->upContext );
					cycRefCountIncrement( (DaoBase*) rout->original );
					cycRefCountIncrements( rout->specialized );
					cycRefCountIncrements( rout->regType );
					cycRefCountIncrementMapValue( rout->abstypes );
				}
				break;
			}
		case DAO_CLASS :
			{
				DaoClass *klass = (DaoClass*) dbase;
				cycRefCountIncrementMapValue( klass->abstypes );
				cycRefCountIncrement( (DaoBase*) klass->clsType );
				cycRefCountIncrementsV( klass->cstData );
				cycRefCountIncrementsV( klass->glbData );
				cycRefCountIncrements( klass->superClass );
				cycRefCountIncrements( klass->objDataType );
				cycRefCountIncrements( klass->glbDataType );
				cycRefCountDecrements( klass->references );
				break;
			}
		case DAO_INTERFACE :
			{
				DaoInterface *inter = (DaoInterface*)dbase;
				cycRefCountIncrementMapValue( inter->methods );
				cycRefCountIncrements( inter->supers );
				cycRefCountIncrement( (DaoBase*) inter->abtype );
				break;
			}
		case DAO_CONTEXT :
			{
				DaoContext *ctx = (DaoContext*)dbase;
				cycRefCountIncrement( (DaoBase*) ctx->object );
				cycRefCountIncrement( (DaoBase*) ctx->routine );
				cycRefCountIncrementsVT( ctx->regArray );
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNameSpace *ns = (DaoNameSpace*) dbase;
				cycRefCountIncrementsV( ns->cstData );
				cycRefCountIncrementsV( ns->varData );
				cycRefCountIncrements( ns->varType );
				cycRefCountIncrements( ns->cmodule->cmethods );
				cycRefCountIncrements( ns->mainRoutines );
				cycRefCountIncrementMapValue( ns->abstypes );
				for(k=0; k<ns->cmodule->ctypers->size; k++){
					DaoTypeBase *typer = (DaoTypeBase*)ns->cmodule->ctypers->items.pBase[k];
					if( typer->priv == NULL ) continue;
					cycRefCountIncrementMapValueV( typer->priv->values );
					cycRefCountIncrementMapValue( typer->priv->methods );
				}
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) dbase;
				cycRefCountIncrement( abtp->X.extra );
				cycRefCountIncrements( abtp->nested );
				if( abtp->interfaces ){
					node = DMap_First( abtp->interfaces );
					for( ; node != NULL; node = DMap_Next( abtp->interfaces, node ) )
						cycRefCountIncrement( node->key.pBase );
				}
				break;
			}
		case DAO_FUTURE :
			{
				DaoFuture *future = (DaoFuture*) dbase;
				cycRefCountIncrementV( future->value );
				cycRefCountIncrement( (DaoBase*) future->unitype );
				cycRefCountIncrement( (DaoBase*) future->context );
				cycRefCountIncrement( (DaoBase*) future->process );
				cycRefCountIncrement( (DaoBase*) future->precondition );
				break;
			}
		case DAO_VMPROCESS :
			{
				DaoVmProcess *vmp = (DaoVmProcess*) dbase;
				DaoVmFrame *frame = vmp->firstFrame;
				cycRefCountIncrementV( vmp->returned );
				cycRefCountIncrementsV( vmp->parResume );
				cycRefCountIncrementsV( vmp->parYield );
				cycRefCountIncrementsV( vmp->exceptions );
				while( frame ){
					cycRefCountIncrement( (DaoBase*) frame->context );
					frame = frame->next;
				}
				break;
			}
		default: break;
		}
	}
}

void freeGarbage()
{
	DaoTypeBase *typer;
	DArray *pool = gcWorker.pool[ gcWorker.work ];
	DNode *node;
	size_t i, k;
	const short work = gcWorker.work;
	const short idle = gcWorker.idle;

	for( i=0; i<pool->size; i++ ){
		DaoBase *dbase = pool->items.pBase[i];
		dbase->gcState[work] = 0;

		if( dbase->cycRefCount == 0 || dbase->refCount ==0 ){

			DMutex_Lock( & gcWorker.mutex_switch_heap );
			switch( dbase->type ){

#ifdef DAO_WITH_NUMARRAY
			case DAO_ARRAY :
				{
					DaoArray *array = (DaoArray*) dbase;
					GC_BREAK_REF( array->unitype );
					GC_BREAK_REF( array->meta );
					break;
				}
#endif
			case DAO_TUPLE :
				{
					DaoTuple *tuple = (DaoTuple*) dbase;
					directRefCountDecrementVT( tuple->items );
					GC_BREAK_REF( tuple->unitype );
					GC_BREAK_REF( tuple->meta );
					break;
				}
			case DAO_LIST :
				{
					DaoList *list = (DaoList*) dbase;
					directRefCountDecrementV( list->items );
					GC_BREAK_REF( list->unitype );
					GC_BREAK_REF( list->meta );
					break;
				}
			case DAO_MAP :
				{
					DaoMap *map = (DaoMap*) dbase;
					node = DMap_First( map->items );
					for( ; node != NULL; node = DMap_Next( map->items, node ) ){
						if( node->key.pValue->t >= DAO_ARRAY ){
							node->key.pValue->v.p->refCount --;
							node->key.pValue->t = 0;
						}else{
							if( node->key.pValue->t == DAO_ENUM && node->key.pValue->v.e->type ){
								node->key.pValue->v.e->type->refCount --;
								node->key.pValue->v.e->type = NULL;
							}
							DValue_Clear( node->key.pValue );
						}
						if( node->value.pValue->t >= DAO_ARRAY ){
							node->value.pValue->v.p->refCount --;
							node->value.pValue->t = 0;
						}else{
							if( node->value.pValue->t == DAO_ENUM && node->value.pValue->v.e->type ){
								node->value.pValue->v.e->type->refCount --;
								node->value.pValue->v.e->type = NULL;
							}
							DValue_Clear( node->value.pValue );
						}
					}
					DMap_Clear( map->items );
					GC_BREAK_REF( map->unitype );
					GC_BREAK_REF( map->meta );
					break;
				}
			case DAO_OBJECT :
				{
					DaoObject *obj = (DaoObject*) dbase;
					directRefCountDecrementT( obj->superObject );
					directRefCountDecrementVT( obj->objData );
					GC_BREAK_REF( obj->meta );
					GC_BREAK_REF( obj->myClass );
					break;
				}
			case DAO_CDATA :
				{
					DaoCData *cdata = (DaoCData*) dbase;
					GC_BREAK_REF( cdata->meta );
					GC_BREAK_REF( cdata->daoObject );
					break;
				}
			case DAO_ROUTINE :
			case DAO_FUNCTION :
				{
					DaoRoutine *rout = (DaoRoutine*)dbase;
					GC_BREAK_REF( rout->nameSpace );
					GC_BREAK_REF( rout->routType );
					GC_BREAK_REF( rout->routHost );
					directRefCountDecrementV( rout->routConsts );
					directRefCountDecrement( rout->routTable );
					if( rout->type == DAO_ROUTINE && rout->minimal ==0 ){
						GC_BREAK_REF( rout->upRoutine );
						GC_BREAK_REF( rout->upContext );
						GC_BREAK_REF( rout->original );
						directRefCountDecrement( rout->specialized );
						directRefCountDecrement( rout->regType );
						directRefCountDecrementMapValue( rout->abstypes );
					}
					break;
				}
			case DAO_CLASS :
				{
					DaoClass *klass = (DaoClass*)dbase;
					GC_BREAK_REF( klass->clsType );
					directRefCountDecrementMapValue( klass->abstypes );
					directRefCountDecrementV( klass->cstData );
					directRefCountDecrementV( klass->glbData );
					directRefCountDecrement( klass->superClass );
					directRefCountDecrement( klass->objDataType );
					directRefCountDecrement( klass->glbDataType );
					directRefCountDecrement( klass->references );
					break;
				}
			case DAO_INTERFACE :
				{
					DaoInterface *inter = (DaoInterface*)dbase;
					directRefCountDecrementMapValue( inter->methods );
					directRefCountDecrement( inter->supers );
					GC_BREAK_REF( inter->abtype );
					break;
				}
			case DAO_CONTEXT :
				{
					DaoContext *ctx = (DaoContext*)dbase;
					GC_BREAK_REF( ctx->object );
					GC_BREAK_REF( ctx->routine );
					directRefCountDecrementVT( ctx->regArray );
					break;
				}
			case DAO_NAMESPACE :
				{
					DaoNameSpace *ns = (DaoNameSpace*) dbase;
					directRefCountDecrementV( ns->cstData );
					directRefCountDecrementV( ns->varData );
					directRefCountDecrement( ns->varType );
					directRefCountDecrement( ns->cmodule->cmethods );
					directRefCountDecrement( ns->mainRoutines );
					directRefCountDecrementMapValue( ns->abstypes );
					for(k=0; k<ns->cmodule->ctypers->size; k++){
						DaoTypeBase *typer = (DaoTypeBase*)ns->cmodule->ctypers->items.pBase[k];
						if( typer->priv == NULL ) continue;
						directRefCountDecrementMapValueV( typer->priv->values );
						directRefCountDecrementMapValue( typer->priv->methods );
					}
					break;
				}
			case DAO_TYPE :
				{
					DaoType *abtp = (DaoType*) dbase;
					directRefCountDecrement( abtp->nested );
					GC_BREAK_REF( abtp->X.extra );
					if( abtp->interfaces ){
						node = DMap_First( abtp->interfaces );
						for( ; node != NULL; node = DMap_Next( abtp->interfaces, node ) )
							node->key.pBase->refCount --;
						DMap_Clear( abtp->interfaces );
					}
					break;
				}
			case DAO_FUTURE :
				{
					DaoFuture *future = (DaoFuture*) dbase;
					directRefCountDecrementValue( & future->value );
					GC_BREAK_REF( future->unitype );
					GC_BREAK_REF( future->context );
					GC_BREAK_REF( future->process );
					GC_BREAK_REF( future->precondition );
					break;
				}
			case DAO_VMPROCESS :
				{
					DaoVmProcess *vmp = (DaoVmProcess*) dbase;
					DaoVmFrame *frame = vmp->firstFrame;
					if( vmp->returned.t >= DAO_ARRAY ){
						vmp->returned.v.p->refCount --;
					}else{
						if( vmp->returned.t == DAO_ENUM && vmp->returned.v.e->type ){
							vmp->returned.v.e->type->refCount --;
							vmp->returned.v.e->type = NULL;
						}
						DValue_Clear( & vmp->returned );
					}
					vmp->returned.t = 0;
					directRefCountDecrementV( vmp->parResume );
					directRefCountDecrementV( vmp->parYield );
					directRefCountDecrementV( vmp->exceptions );
					while( frame ){
						if( frame->context ) frame->context->refCount --;
						frame->context = NULL;
						frame = frame->next;
					}
					break;
				}
			default: break;
			}
			DMutex_Unlock( & gcWorker.mutex_switch_heap );
		}
	}

	for( i=0; i<pool->size; i++ ){
		DaoBase *dbase = pool->items.pBase[i];
		if( dbase->cycRefCount==0 || dbase->refCount==0 ){
			if( dbase->refCount !=0 ){
				printf(" refCount not zero %i: %i\n", dbase->type, dbase->refCount );

#if DEBUG
				if( dbase->type == DAO_FUNCTION ){
					DaoFunction *func = (DaoFunction*)dbase;
					printf( "%s\n", func->routName->mbs );
				}else if( dbase->type == DAO_TYPE ){
					DaoType *func = (DaoType*)dbase;
					printf( "%s\n", func->name->mbs );
				}
#endif
				DMutex_Lock( & gcWorker.mutex_switch_heap );
				if( ! ( dbase->gcState[ idle ] & GC_IN_POOL ) ){
					dbase->gcState[ idle ] = GC_IN_POOL;
					DArray_Append( gcWorker.pool[idle], dbase );
				}
				DMutex_Unlock( & gcWorker.mutex_switch_heap );
				continue;
			}
			if( ! ( dbase->gcState[idle] & GC_IN_POOL ) ){
#ifdef DAO_GC_PROF
				ObjectProfile[dbase->type] --;
#endif
				/*
				   if( dbase->type <= DAO_VMPROCESS )
				   if( dbase->type == DAO_STRING ){
				   DaoString *s = (DaoString*) dbase;
				   if( s->chars->mbs && s->chars->mbs->refCount > 1 ){
				   printf( "delete mbstring!!! %i\n", s->chars->mbs->refCount );
				   }
				   if( s->chars->wcs && s->chars->wcs->refCount > 1 ){
				   printf( "delete wcstring!!! %i\n", s->chars->wcs->refCount );
				   }
				   }
				   if( dbase->type < DAO_STRING )
				   if( dbase->type != DAO_CONTEXT )
				 */
				if( dbase->type == DAO_ROUTINE )
					DaoCallbackData_DeleteByCallback( (DaoRoutine*) dbase );
				DaoCallbackData_DeleteByUserdata( dbase );
				typer = DaoBase_GetTyper( dbase );
				typer->Delete( dbase );
			}
		}
	}
#ifdef DAO_GC_PROF
#warning "-------------------- DAO_GC_PROF is turned on."
	printf("heap[idle] = %i;\theap[work] = %i\n", gcWorker.pool[ idle ]->size, gcWorker.pool[ work ]->size );
	printf("=======================================\n");
	printf( "mbs count = %i\n", daoCountMBS );
	printf( "array count = %i\n", daoCountArray );
	for(i=0; i<100; i++){
		if( ObjectProfile[i] != 0 ){
			printf( "type = %3i; rest = %5i\n", i, ObjectProfile[i] );
		}
	}
#endif
	DArray_Clear( pool );
}
void cycRefCountDecrement( DaoBase *dbase )
{
	const short work = gcWorker.work;
	if( !dbase ) return;
	if( ! ( dbase->gcState[work] & GC_IN_POOL ) ){
		DArray_Append( gcWorker.pool[work], dbase );
		dbase->gcState[work] = GC_IN_POOL;
		dbase->cycRefCount = dbase->refCount;
	}
	dbase->cycRefCount --;

	if( dbase->cycRefCount<0 ){
		/*
		   printf("cycRefCount<0 : %i\n", dbase->type);
		 */
		dbase->cycRefCount = 0;
	}
}
void cycRefCountIncrement( DaoBase *dbase )
{
	const short work = gcWorker.work;
	if( dbase ){
		dbase->cycRefCount++;
		if( ! ( dbase->gcState[work] & GC_MARKED ) ){
			dbase->gcState[work] |= GC_MARKED;
			DArray_Append( gcWorker.objAlive, dbase );
		}
	}
}

#else

void GC_Lock(){}
void GC_Unlock(){}

/* Incremental Garbage Collector */
enum DaoGCWorkType
{
	GC_INIT_RC ,
	GC_DEC_RC ,
	GC_INC_RC ,
	GC_INC_RC2 ,
	GC_DIR_DEC_RC ,
	GC_FREE
};

static void InitRC();
static void directDecRC();
static void SwitchGC();
static void ContinueGC();

#include"daoVmspace.h"
extern DaoVmSpace *mainVmSpace;
void DaoGC_IncRC( DaoBase *p )
{
	const short work = gcWorker.work;
	if( ! p ) return;
#ifdef DEBUG_TRACE
	if( p == 0x736d010 ){
		print_trace();
	}
#endif
	if( p->refCount == 0 ){
		p->refCount ++;
		return;
	}

	p->refCount ++;
	p->cycRefCount ++;
	if( ! ( p->gcState[work] & GC_IN_POOL ) && gcWorker.workType == GC_INC_RC ){
		DArray_Append( gcWorker.pool[work], p );
		p->gcState[work] = GC_IN_POOL;
	}
}
static int counts = 100;
void DaoGC_DecRC( DaoBase *p )
{
	const short idle = gcWorker.idle;
	if( ! p ) return;

#ifdef DEBUG_TRACE
	if( p == 0x27aed90 ) print_trace();
#endif
#if 0
	if( p->type == DAO_TYPE ){
		DaoType *abtp = (DaoType*) p;
		if( abtp->tid == DAO_LIST )
			return;
	}

#include"assert.h"
	printf( "DaoGC_DecRC: %i\n", p->type );
	assert( p->type != 48 );
	printf( "here: %i %i\n", gcWorker.pool[ ! idle ]->size, gcWorker.pool[ idle ]->size );
#endif

	DaoGC_DecRC2( p, -1 );

	if( gcWorker.busy ) return;
	counts --;
	if( gcWorker.count < gcWorker.gcMax ){
		if( counts ) return;
		counts = 100;
	}else{
		if( counts ) return;
		counts = 10;
	}

	if( gcWorker.pool[ ! idle ]->size )
		ContinueGC();
	else if( gcWorker.pool[ idle ]->size > gcWorker.gcMin )
		SwitchGC();
}
void DaoGC_IncRCs( DArray *list )
{
	size_t i;
	DaoBase **data;
	if( list->size == 0 ) return;
	data = list->items.pBase;
	for( i=0; i<list->size; i++) DaoGC_IncRC( data[i] );
}
void DaoGC_DecRCs( DArray *list )
{
	size_t i;
	DaoBase **data;
	if( list == NULL || list->size == 0 ) return;
	data = list->items.pBase;
	for( i=0; i<list->size; i++) DaoGC_DecRC( data[i] );
}
void DaoGC_ShiftRC( DaoBase *up, DaoBase *down )
{
	if( up == down ) return;
	if( up ) DaoGC_IncRC( up );
	if( down ) DaoGC_DecRC( down );
}

void SwitchGC()
{
	if( gcWorker.busy ) return;
	gcWorker.work = gcWorker.idle;
	gcWorker.idle = ! gcWorker.work;
	gcWorker.workType = 0;
	gcWorker.ii = 0;
	gcWorker.jj = 0;
	ContinueGC();
}
void ContinueGC()
{
	if( gcWorker.busy ) return;
	gcWorker.busy = 1;
	switch( gcWorker.workType ){
	case GC_INIT_RC :
		DaoLateDeleter_Update();
		InitRC();
		break;
	case GC_DEC_RC :
		cycRefCountDecreScan();
		break;
	case GC_INC_RC :
	case GC_INC_RC2 :
		cycRefCountIncreScan();
		break;
	case GC_DIR_DEC_RC :
		directDecRC();
		break;
	case GC_FREE :
		freeGarbage();
		break;
	default : break;
	}
	gcWorker.busy = 0;
}
void DaoFinishGC()
{
	short idle = gcWorker.idle;
	while( gcWorker.pool[ idle ]->size || gcWorker.pool[ ! idle ]->size ){
		while( gcWorker.pool[ ! idle ]->size ) ContinueGC();
		if( gcWorker.pool[ idle ]->size ) SwitchGC();
		idle = gcWorker.idle;
		while( gcWorker.pool[ ! idle ]->size ) ContinueGC();
	}

	DArray_Delete( gcWorker.pool[0] );
	DArray_Delete( gcWorker.pool[1] );
	DArray_Delete( gcWorker.objAlive );
	DaoLateDeleter_Finish();
	gcWorker.objAlive = NULL;
}
void InitRC()
{
	DArray *pool = gcWorker.pool[ gcWorker.work ];
	size_t i = gcWorker.ii;
	size_t k = gcWorker.ii + gcWorker.gcMin / 2;
	for( ; i<pool->size; i++ ){
		pool->items.pBase[i]->cycRefCount = pool->items.pBase[i]->refCount;
		if( i > k ) break;
	}
	if( i >= pool->size ){
		gcWorker.ii = 0;
		gcWorker.workType = GC_DEC_RC;
	}else{
		gcWorker.ii = i+1;
	}
}
void cycRefCountDecreScan()
{
	DArray *pool = gcWorker.pool[ gcWorker.work ];
	DNode *node;
	size_t i = gcWorker.ii;
	size_t j = 0, k;

	for( ; i<pool->size; i++ ){
		DaoBase *dbase = pool->items.pBase[i];
		switch( dbase->type ){
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) dbase;
				cycRefCountDecrement( (DaoBase*) array->unitype );
				cycRefCountDecrement( (DaoBase*) array->meta );
				j ++;
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple = (DaoTuple*) dbase;
				cycRefCountDecrement( (DaoBase*) tuple->unitype );
				cycRefCountDecrement( (DaoBase*) tuple->meta );
				cycRefCountDecrementsVT( tuple->items );
				j += tuple->items->size;
				break;
			}
		case DAO_LIST :
			{
				DaoList *list = (DaoList*) dbase;
				cycRefCountDecrement( (DaoBase*) list->unitype );
				cycRefCountDecrement( (DaoBase*) list->meta );
				cycRefCountDecrementsV( list->items );
				j += list->items->size;
				break;
			}
		case DAO_MAP :
			{
				DaoMap *map = (DaoMap*) dbase;
				cycRefCountDecrement( (DaoBase*) map->unitype );
				cycRefCountDecrement( (DaoBase*) map->meta );
				node = DMap_First( map->items );
				for( ; node != NULL; node = DMap_Next( map->items, node ) ) {
					cycRefCountDecrementV( node->key.pValue[0] );
					cycRefCountDecrementV( node->value.pValue[0] );
				}
				j += map->items->size;
				break;
			}
		case DAO_OBJECT :
			{
				DaoObject *obj = (DaoObject*) dbase;
				cycRefCountDecrementsT( obj->superObject );
				cycRefCountDecrementsVT( obj->objData );
				if( obj->superObject ) j += obj->superObject->size;
				if( obj->objData ) j += obj->objData->size;
				cycRefCountDecrement( (DaoBase*) obj->meta );
				cycRefCountDecrement( (DaoBase*) obj->myClass );
				break;
			}
		case DAO_CDATA :
			{
				DaoCData *cdata = (DaoCData*) dbase;
				cycRefCountDecrement( (DaoBase*) cdata->meta );
				cycRefCountDecrement( (DaoBase*) cdata->daoObject );
				break;
			}
		case DAO_ROUTINE :
		case DAO_FUNCTION :
			{
				DaoRoutine *rout = (DaoRoutine*)dbase;
				cycRefCountDecrement( (DaoBase*) rout->routType );
				cycRefCountDecrement( (DaoBase*) rout->routHost );
				cycRefCountDecrement( (DaoBase*) rout->nameSpace );
				cycRefCountDecrementsV( rout->routConsts );
				cycRefCountDecrements( rout->routTable );
				j += rout->routConsts->size + rout->routTable->size;
				if( rout->type == DAO_ROUTINE && rout->minimal ==0 ){
					j += rout->regType->size + rout->abstypes->size;
					cycRefCountDecrement( (DaoBase*) rout->upRoutine );
					cycRefCountDecrement( (DaoBase*) rout->upContext );
					cycRefCountDecrement( (DaoBase*) rout->original );
					cycRefCountDecrements( rout->specialized );
					cycRefCountDecrements( rout->regType );
					cycRefCountDecrementMapValue( rout->abstypes );
				}
				break;
			}
		case DAO_CLASS :
			{
				DaoClass *klass = (DaoClass*)dbase;
				cycRefCountDecrementMapValue( klass->abstypes );
				cycRefCountDecrement( (DaoBase*) klass->clsType );
				cycRefCountDecrementsV( klass->cstData );
				cycRefCountDecrementsV( klass->glbData );
				cycRefCountDecrements( klass->superClass );
				cycRefCountDecrements( klass->objDataType );
				cycRefCountDecrements( klass->glbDataType );
				cycRefCountDecrements( klass->references );
				j += klass->cstData->size + klass->glbData->size;
				j += klass->superClass->size + klass->abstypes->size;
				j += klass->objDataType->size + klass->glbDataType->size;
				j += klass->references->size + klass->abstypes->size;
				break;
			}
		case DAO_INTERFACE :
			{
				DaoInterface *inter = (DaoInterface*)dbase;
				cycRefCountDecrementMapValue( inter->methods );
				cycRefCountDecrements( inter->supers );
				cycRefCountDecrement( (DaoBase*) inter->abtype );
				j += inter->supers->size + inter->methods->size;
				break;
			}
		case DAO_CONTEXT :
			{
				DaoContext *ctx = (DaoContext*)dbase;
				cycRefCountDecrement( (DaoBase*) ctx->object );
				cycRefCountDecrement( (DaoBase*) ctx->routine );
				cycRefCountDecrementsVT( ctx->regArray );
				j += ctx->regArray->size + 3;
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNameSpace *ns = (DaoNameSpace*) dbase;
				cycRefCountDecrementsV( ns->cstData );
				cycRefCountDecrementsV( ns->varData );
				cycRefCountDecrements( ns->varType );
				cycRefCountDecrements( ns->cmodule->cmethods );
				cycRefCountDecrements( ns->mainRoutines );
				j += ns->cstData->size + ns->varData->size + ns->abstypes->size;
				cycRefCountDecrementMapValue( ns->abstypes );
				for(k=0; k<ns->cmodule->ctypers->size; k++){
					DaoTypeBase *typer = (DaoTypeBase*)ns->cmodule->ctypers->items.pBase[k];
					if( typer->priv == NULL ) continue;
					cycRefCountDecrementMapValueV( typer->priv->values );
					cycRefCountDecrementMapValue( typer->priv->methods );
				}
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) dbase;
				cycRefCountDecrement( abtp->X.extra );
				cycRefCountDecrements( abtp->nested );
				if( abtp->interfaces ){
					node = DMap_First( abtp->interfaces );
					for( ; node != NULL; node = DMap_Next( abtp->interfaces, node ) )
						cycRefCountDecrement( node->key.pBase );
				}
				break;
		case DAO_FUTURE :
			{
				DaoFuture *future = (DaoFuture*) dbase;
				cycRefCountDecrementV( future->value );
				cycRefCountDecrement( (DaoBase*) future->unitype );
				cycRefCountDecrement( (DaoBase*) future->context );
				cycRefCountDecrement( (DaoBase*) future->process );
				cycRefCountDecrement( (DaoBase*) future->precondition );
				break;
			}
			}
		case DAO_VMPROCESS :
			{
				DaoVmProcess *vmp = (DaoVmProcess*) dbase;
				DaoVmFrame *frame = vmp->firstFrame;
				cycRefCountDecrementV( vmp->returned );
				cycRefCountDecrementsV( vmp->parResume );
				cycRefCountDecrementsV( vmp->parYield );
				cycRefCountDecrementsV( vmp->exceptions );
				while( frame ){
					cycRefCountDecrement( (DaoBase*) frame->context );
					frame = frame->next;
				}
				break;
			}
		default: break;
		}
		if( j >= gcWorker.gcMin ) break;
	}
	if( i >= pool->size ){
		gcWorker.ii = 0;
		gcWorker.workType = GC_INC_RC;
	}else{
		gcWorker.ii = i+1;
	}
}
void cycRefCountIncreScan()
{
	const short work = gcWorker.work;
	DArray *pool = gcWorker.pool[ gcWorker.work ];
	DNode *node;
	size_t i = gcWorker.ii;
	size_t j = 0, k;

	for( ; i<pool->size; i++ ){
		DaoBase *dbase = pool->items.pBase[i];
		j ++;
		if( dbase->type == DAO_CDATA && dbase->cycRefCount ==0 ){
			DaoCData *cdata = (DaoCData*) dbase;
			DaoCDataCore *core = (DaoCDataCore*)cdata->typer->priv;
			if( !(cdata->attribs & DAO_CDATA_FREE) ) continue;
			if( cdata->data == NULL || core == NULL ) continue;
			if( core->DelTest == NULL ) continue;
			if( core->DelTest( cdata->data ) ) continue;
			DaoCData_SetExtReference( cdata, 1 );
		}
		if( dbase->cycRefCount >0 && ! ( dbase->gcState[work] & GC_MARKED ) ){
			dbase->gcState[work] |= GC_MARKED;
			switch( dbase->type ){
#ifdef DAO_WITH_NUMARRAY
			case DAO_ARRAY :
				{
					DaoArray *array = (DaoArray*) dbase;
					cycRefCountIncrement( (DaoBase*) array->unitype );
					cycRefCountIncrement( (DaoBase*) array->meta );
					break;
				}
#endif
			case DAO_TUPLE :
				{
					DaoTuple *tuple= (DaoTuple*) dbase;
					cycRefCountIncrement( (DaoBase*) tuple->unitype );
					cycRefCountIncrement( (DaoBase*) tuple->meta );
					cycRefCountIncrementsVT( tuple->items );
					j += tuple->items->size;
					break;
				}
			case DAO_LIST :
				{
					DaoList *list= (DaoList*) dbase;
					cycRefCountIncrement( (DaoBase*) list->unitype );
					cycRefCountIncrement( (DaoBase*) list->meta );
					cycRefCountIncrementsV( list->items );
					j += list->items->size;
					break;
				}
			case DAO_MAP :
				{
					DaoMap *map = (DaoMap*)dbase;
					cycRefCountIncrement( (DaoBase*) map->unitype );
					cycRefCountIncrement( (DaoBase*) map->meta );
					node = DMap_First( map->items );
					for( ; node != NULL; node = DMap_Next( map->items, node ) ){
						cycRefCountIncrementV( node->key.pValue[0] );
						cycRefCountIncrementV( node->value.pValue[0] );
					}
					j += map->items->size;
					break;
				}
			case DAO_OBJECT :
				{
					DaoObject *obj = (DaoObject*) dbase;
					cycRefCountIncrementsT( obj->superObject );
					cycRefCountIncrementsVT( obj->objData );
					cycRefCountIncrement( (DaoBase*) obj->meta );
					cycRefCountIncrement( (DaoBase*) obj->myClass );
					if( obj->superObject ) j += obj->superObject->size;
					if( obj->objData ) j += obj->objData->size;
					break;
				}
			case DAO_CDATA :
				{
					DaoCData *cdata = (DaoCData*) dbase;
					cycRefCountIncrement( (DaoBase*) cdata->meta );
					cycRefCountIncrement( (DaoBase*) cdata->daoObject );
					break;
				}
			case DAO_ROUTINE :
			case DAO_FUNCTION :
				{
					DaoRoutine *rout = (DaoRoutine*) dbase;
					cycRefCountIncrement( (DaoBase*) rout->routType );
					cycRefCountIncrement( (DaoBase*) rout->routHost );
					cycRefCountIncrement( (DaoBase*) rout->nameSpace );
					cycRefCountIncrementsV( rout->routConsts );
					cycRefCountIncrements( rout->routTable );
					if( rout->type == DAO_ROUTINE && rout->minimal ==0 ){
						j += rout->abstypes->size;
						cycRefCountIncrement( (DaoBase*) rout->upRoutine );
						cycRefCountIncrement( (DaoBase*) rout->upContext );
						cycRefCountIncrement( (DaoBase*) rout->original );
						cycRefCountIncrements( rout->specialized );
						cycRefCountIncrements( rout->regType );
						cycRefCountIncrementMapValue( rout->abstypes );
					}
					j += rout->routConsts->size + rout->routTable->size;
					break;
				}
			case DAO_CLASS :
				{
					DaoClass *klass = (DaoClass*) dbase;
					cycRefCountIncrementMapValue( klass->abstypes );
					cycRefCountIncrement( (DaoBase*) klass->clsType );
					cycRefCountIncrementsV( klass->cstData );
					cycRefCountIncrementsV( klass->glbData );
					cycRefCountIncrements( klass->superClass );
					cycRefCountIncrements( klass->objDataType );
					cycRefCountIncrements( klass->glbDataType );
					cycRefCountIncrements( klass->references );
					j += klass->cstData->size + klass->glbData->size;
					j += klass->superClass->size + klass->abstypes->size;
					j += klass->objDataType->size + klass->glbDataType->size;
					j += klass->references->size + klass->abstypes->size;
					break;
				}
			case DAO_INTERFACE :
				{
					DaoInterface *inter = (DaoInterface*)dbase;
					cycRefCountIncrementMapValue( inter->methods );
					cycRefCountIncrements( inter->supers );
					cycRefCountIncrement( (DaoBase*) inter->abtype );
					j += inter->supers->size + inter->methods->size;
					break;
				}
			case DAO_CONTEXT :
				{
					DaoContext *ctx = (DaoContext*)dbase;
					cycRefCountIncrement( (DaoBase*) ctx->object );
					cycRefCountIncrement( (DaoBase*) ctx->routine );
					cycRefCountIncrementsVT( ctx->regArray );
					j += ctx->regArray->size + 3;
					break;
				}
			case DAO_NAMESPACE :
				{
					DaoNameSpace *ns = (DaoNameSpace*) dbase;
					cycRefCountIncrementsV( ns->cstData );
					cycRefCountIncrementsV( ns->varData );
					cycRefCountIncrements( ns->varType );
					cycRefCountIncrements( ns->cmodule->cmethods );
					cycRefCountIncrements( ns->mainRoutines );
					j += ns->cstData->size + ns->varData->size + ns->abstypes->size;
					cycRefCountIncrementMapValue( ns->abstypes );
					for(k=0; k<ns->cmodule->ctypers->size; k++){
						DaoTypeBase *typer = (DaoTypeBase*)ns->cmodule->ctypers->items.pBase[k];
						if( typer->priv == NULL ) continue;
						cycRefCountIncrementMapValueV( typer->priv->values );
						cycRefCountIncrementMapValue( typer->priv->methods );
					}
					break;
				}
			case DAO_TYPE :
				{
					DaoType *abtp = (DaoType*) dbase;
					cycRefCountIncrement( abtp->X.extra );
					cycRefCountIncrements( abtp->nested );
					if( abtp->interfaces ){
						node = DMap_First( abtp->interfaces );
						for( ; node != NULL; node = DMap_Next( abtp->interfaces, node ) )
							cycRefCountIncrement( node->key.pBase );
					}
					break;
				}
			case DAO_FUTURE :
				{
					DaoFuture *future = (DaoFuture*) dbase;
					cycRefCountIncrementV( future->value );
					cycRefCountIncrement( (DaoBase*) future->unitype );
					cycRefCountIncrement( (DaoBase*) future->context );
					cycRefCountIncrement( (DaoBase*) future->process );
					cycRefCountIncrement( (DaoBase*) future->precondition );
					break;
				}
			case DAO_VMPROCESS :
				{
					DaoVmProcess *vmp = (DaoVmProcess*) dbase;
					DaoVmFrame *frame = vmp->firstFrame;
					cycRefCountIncrementV( vmp->returned );
					cycRefCountIncrementsV( vmp->parResume );
					cycRefCountIncrementsV( vmp->parYield );
					cycRefCountIncrementsV( vmp->exceptions );
					while( frame ){
						cycRefCountIncrement( (DaoBase*) frame->context );
						frame = frame->next;
					}
					break;
				}
			default: break;
			}
		}
		if( j >= gcWorker.gcMin ) break;
	}
	if( i >= pool->size ){
		gcWorker.ii = 0;
		gcWorker.workType ++;
		gcWorker.boundary = pool->size;
	}else{
		gcWorker.ii = i+1;
	}
}
void directDecRC()
{
	DArray *pool = gcWorker.pool[ gcWorker.work ];
	DNode *node;
	const short work = gcWorker.work;
	size_t boundary = gcWorker.boundary;
	size_t i = gcWorker.ii;
	size_t j = 0, k;

	for( ; i<boundary; i++ ){
		DaoBase *dbase = pool->items.pBase[i];
		dbase->gcState[work] = 0;
		j ++;
		if( dbase->cycRefCount == 0 ){

			switch( dbase->type ){

#ifdef DAO_WITH_NUMARRAY
			case DAO_ARRAY :
				{
					DaoArray *array = (DaoArray*) dbase;
					GC_BREAK_REF( array->unitype );
					GC_BREAK_REF( array->meta );
					break;
				}
#endif
			case DAO_TUPLE :
				{
					DaoTuple *tuple = (DaoTuple*) dbase;
					j += tuple->items->size;
					directRefCountDecrementVT( tuple->items );
					GC_BREAK_REF( tuple->unitype );
					GC_BREAK_REF( tuple->meta );
					break;
				}
			case DAO_LIST :
				{
					DaoList *list = (DaoList*) dbase;
					j += list->items->size;
					directRefCountDecrementV( list->items );
					GC_BREAK_REF( list->unitype );
					GC_BREAK_REF( list->meta );
					break;
				}
			case DAO_MAP :
				{
					DaoMap *map = (DaoMap*) dbase;
					node = DMap_First( map->items );
					for( ; node != NULL; node = DMap_Next( map->items, node ) ){
						if( node->key.pValue->t >= DAO_ARRAY ){
							node->key.pValue->v.p->refCount --;
							node->key.pValue->t = 0;
						}else{
							DValue_Clear( node->key.pValue );
						}
						if( node->value.pValue->t >= DAO_ARRAY ){
							node->value.pValue->v.p->refCount --;
							node->value.pValue->t = 0;
						}else{
							DValue_Clear( node->value.pValue );
						}
					}
					j += map->items->size;
					DMap_Clear( map->items );
					GC_BREAK_REF( map->unitype );
					GC_BREAK_REF( map->meta );
					break;
				}
			case DAO_OBJECT :
				{
					DaoObject *obj = (DaoObject*) dbase;
					if( obj->superObject ) j += obj->superObject->size;
					if( obj->objData ) j += obj->objData->size;
					directRefCountDecrementT( obj->superObject );
					directRefCountDecrementVT( obj->objData );
					GC_BREAK_REF( obj->meta );
					GC_BREAK_REF( obj->myClass );
					break;
				}
			case DAO_CDATA :
				{
					DaoCData *cdata = (DaoCData*) dbase;
					GC_BREAK_REF( cdata->meta );
					GC_BREAK_REF( cdata->daoObject );
					break;
				}
			case DAO_ROUTINE :
			case DAO_FUNCTION :
				{
					DaoRoutine *rout = (DaoRoutine*)dbase;
					GC_BREAK_REF( rout->nameSpace );
					/* may become NULL, if it has already become garbage 
					 * in the last cycle */
					GC_BREAK_REF( rout->routType );
					/* may become NULL, if it has already become garbage 
					 * in the last cycle */
					GC_BREAK_REF( rout->routHost );

					j += rout->routConsts->size + rout->routTable->size;
					directRefCountDecrementV( rout->routConsts );
					directRefCountDecrement( rout->routTable );
					if( rout->type == DAO_ROUTINE && rout->minimal ==0 ){
						j += rout->abstypes->size;
						GC_BREAK_REF( rout->upRoutine );
						GC_BREAK_REF( rout->upContext );
						GC_BREAK_REF( rout->original );
						directRefCountDecrement( rout->specialized );
						directRefCountDecrement( rout->regType );
						directRefCountDecrementMapValue( rout->abstypes );
					}
					break;
				}
			case DAO_CLASS :
				{
					DaoClass *klass = (DaoClass*)dbase;
					j += klass->cstData->size + klass->glbData->size;
					j += klass->superClass->size + klass->abstypes->size;
					j += klass->objDataType->size + klass->glbDataType->size;
					j += klass->references->size + klass->abstypes->size;
					GC_BREAK_REF( klass->clsType );
					directRefCountDecrementMapValue( klass->abstypes );
					directRefCountDecrementV( klass->cstData );
					directRefCountDecrementV( klass->glbData );
					directRefCountDecrement( klass->superClass );
					directRefCountDecrement( klass->objDataType );
					directRefCountDecrement( klass->glbDataType );
					directRefCountDecrement( klass->references );
					break;
				}
			case DAO_INTERFACE :
				{
					DaoInterface *inter = (DaoInterface*)dbase;
					j += inter->supers->size + inter->methods->size;
					directRefCountDecrementMapValue( inter->methods );
					directRefCountDecrement( inter->supers );
					GC_BREAK_REF( inter->abtype );
					break;
				}
			case DAO_CONTEXT :
				{
					DaoContext *ctx = (DaoContext*)dbase;
					GC_BREAK_REF( ctx->object );
					GC_BREAK_REF( ctx->routine );
					j += ctx->regArray->size + 3;
					directRefCountDecrementVT( ctx->regArray );
					break;
				}
			case DAO_NAMESPACE :
				{
					DaoNameSpace *ns = (DaoNameSpace*) dbase;
					j += ns->cstData->size + ns->varData->size + ns->abstypes->size;
					directRefCountDecrementV( ns->cstData );
					directRefCountDecrementV( ns->varData );
					directRefCountDecrement( ns->varType );
					directRefCountDecrement( ns->cmodule->cmethods );
					directRefCountDecrement( ns->mainRoutines );
					directRefCountDecrementMapValue( ns->abstypes );
					for(k=0; k<ns->cmodule->ctypers->size; k++){
						DaoTypeBase *typer = (DaoTypeBase*)ns->cmodule->ctypers->items.pBase[k];
						if( typer->priv == NULL ) continue;
						directRefCountDecrementMapValueV( typer->priv->values );
						directRefCountDecrementMapValue( typer->priv->methods );
					}
					break;
				}
			case DAO_TYPE :
				{
					DaoType *abtp = (DaoType*) dbase;
					directRefCountDecrement( abtp->nested );
					GC_BREAK_REF( abtp->X.extra );
					if( abtp->interfaces ){
						node = DMap_First( abtp->interfaces );
						for( ; node != NULL; node = DMap_Next( abtp->interfaces, node ) )
							node->key.pBase->refCount --;
						DMap_Clear( abtp->interfaces );
					}
					break;
				}
			case DAO_FUTURE :
				{
					DaoFuture *future = (DaoFuture*) dbase;
					directRefCountDecrementValue( & future->value );
					GC_BREAK_REF( future->unitype );
					GC_BREAK_REF( future->context );
					GC_BREAK_REF( future->process );
					GC_BREAK_REF( future->precondition );
					break;
				}
			case DAO_VMPROCESS :
				{
					DaoVmProcess *vmp = (DaoVmProcess*) dbase;
					DaoVmFrame *frame = vmp->firstFrame;
					if( vmp->returned.t >= DAO_ARRAY )
						vmp->returned.v.p->refCount --;
					else DValue_Clear( & vmp->returned );
					vmp->returned.t = 0;
					directRefCountDecrementV( vmp->parResume );
					directRefCountDecrementV( vmp->parYield );
					directRefCountDecrementV( vmp->exceptions );
					while( frame ){
						if( frame->context ) frame->context->refCount --;
						frame->context = NULL;
						frame = frame->next;
					}
					break;
				}
			default: break;
			}
		}
		if( j >= gcWorker.gcMin ) break;
	}
	if( i >= boundary ){
		gcWorker.ii = 0;
		gcWorker.workType = GC_FREE;
	}else{
		gcWorker.ii = i+1;
	}
}

void freeGarbage()
{
	DArray *pool = gcWorker.pool[ gcWorker.work ];
	DaoTypeBase *typer;
	const short work = gcWorker.work;
	const short idle = gcWorker.idle;
	size_t boundary = gcWorker.boundary;
	size_t i = gcWorker.ii;
	size_t j = 0;

	for( ; i<boundary; i++ ){
		DaoBase *dbase = pool->items.pBase[i];
		dbase->gcState[work] = 0;
		j ++;
		if( dbase->cycRefCount==0 ){
			if( dbase->refCount !=0 ){
				printf(" refCount not zero %p %i: %i, %i\n", dbase, dbase->type, dbase->refCount, dbase->trait);
#if DEBUG
				if( dbase->type == DAO_FUNCTION ){
					DaoFunction *func = (DaoFunction*)dbase;
					printf( "%s\n", func->routName->mbs );
				}else if( dbase->type == DAO_TYPE ){
					DaoType *func = (DaoType*)dbase;
					printf( "%s\n", func->name->mbs );
				}
#endif
				if( ! ( dbase->gcState[ idle ] & GC_IN_POOL ) ){
					dbase->gcState[ idle ] = GC_IN_POOL;
					DArray_Append( gcWorker.pool[idle], dbase );
				}
				continue;
			}
			if( ! ( dbase->gcState[idle] & GC_IN_POOL ) ){
#ifdef DAO_GC_PROF
				ObjectProfile[dbase->type] --;
#endif
				/*
				   if( dbase->type <= DAO_VMPROCESS )
				   if( dbase->type == DAO_STRING ){
				   DaoString *s = (DaoString*) dbase;
				   if( s->chars->mbs && s->chars->mbs->refCount > 1 ){
				   printf( "delete mbstring!!! %i\n", s->chars->mbs->refCount );
				   }
				   if( s->chars->wcs && s->chars->wcs->refCount > 1 ){
				   printf( "delete wcstring!!! %i\n", s->chars->wcs->refCount );
				   }
				   }
				   if( dbase->type == DAO_FUNCTION ) printf( "here\n" );
				   if( dbase->type < DAO_STRING )
				 */
				if( dbase->type == DAO_ROUTINE )
					DaoCallbackData_DeleteByCallback( (DaoRoutine*) dbase );
				DaoCallbackData_DeleteByUserdata( dbase );
				typer = DaoBase_GetTyper( dbase );
				typer->Delete( dbase );
			}
		}
		if( j >= gcWorker.gcMin ) break;
	}
#ifdef DAO_GC_PROF
	printf("heap[idle] = %i;\theap[work] = %i\n", gcWorker.pool[ idle ]->size, gcWorker.pool[ work ]->size );
	printf("=======================================\n");
	printf( "mbs count = %i\n", daoCountMBS );
	printf( "array count = %i\n", daoCountArray );
	for(k=0; k<100; k++){
		if( ObjectProfile[k] > 0 ){
			printf( "type = %3i; rest = %5i\n", k, ObjectProfile[k] );
		}
	}
#endif
	if( i >= boundary ){
		gcWorker.ii = 0;
		gcWorker.workType = 0;
		gcWorker.count = 0;
		DArray_Clear( pool );
	}else{
		gcWorker.ii = i+1;
	}
}
void cycRefCountDecrement( DaoBase *dbase )
{
	const short work = gcWorker.work;
	if( !dbase ) return;
	if( ! ( dbase->gcState[work] & GC_IN_POOL ) ){
		DArray_Append( gcWorker.pool[work], dbase );
		dbase->gcState[work] = GC_IN_POOL;
		dbase->cycRefCount = dbase->refCount;
	}
	dbase->cycRefCount --;

	if( dbase->cycRefCount<0 ){
		/*
		   printf("cycRefCount<0 : %i\n", dbase->type);
		 */
		dbase->cycRefCount = 0;
	}
}
void cycRefCountIncrement( DaoBase *dbase )
{
	const short work = gcWorker.work;
	if( dbase ){
		dbase->cycRefCount++;
		if( ! ( dbase->gcState[work] & GC_MARKED ) ){
			DArray_Append( gcWorker.pool[work], dbase );
		}
	}
}
#endif
void cycRefCountDecrements( DArray *list )
{
	DaoBase **dbases;
	size_t i;
	if( list == NULL ) return;
	dbases = list->items.pBase;
	for( i=0; i<list->size; i++ ) cycRefCountDecrement( dbases[i] );
}
void cycRefCountIncrements( DArray *list )
{
	DaoBase **dbases;
	size_t i;
	if( list == NULL ) return;
	dbases = list->items.pBase;
	for( i=0; i<list->size; i++ ) cycRefCountIncrement( dbases[i] );
}
void directRefCountDecrement( DArray *list )
{
	DaoBase **dbases;
	size_t i;
	if( list == NULL ) return;
	dbases = list->items.pBase;
	for( i=0; i<list->size; i++ ) if( dbases[i] ) dbases[i]->refCount --;
	DArray_Clear( list );
}
void cycRefCountDecrementMapValue( DMap *dmap )
{
	DNode *it;
	if( dmap == NULL ) return;
	for( it = DMap_First( dmap ); it != NULL; it = DMap_Next( dmap, it ) )
		cycRefCountDecrement( it->value.pBase );
}
void cycRefCountIncrementMapValue( DMap *dmap )
{
	DNode *it;
	if( dmap == NULL ) return;
	for( it = DMap_First( dmap ); it != NULL; it = DMap_Next( dmap, it ) )
		cycRefCountIncrement( it->value.pBase );
}
void directRefCountDecrementMapValue( DMap *dmap )
{
	DNode *it;
	if( dmap == NULL ) return;
	for( it = DMap_First( dmap ); it != NULL; it = DMap_Next( dmap, it ) )
		it->value.pBase->refCount --;
	DMap_Clear( dmap );
}
void cycRefCountDecrementMapValueV( DMap *dmap )
{
	DNode *it;
	if( dmap == NULL ) return;
	for( it = DMap_First( dmap ); it != NULL; it = DMap_Next( dmap, it ) )
		cycRefCountIncrementV( it->value.pValue[0] );
}
void cycRefCountIncrementMapValueV( DMap *dmap )
{
	DNode *it;
	if( dmap == NULL ) return;
	for( it = DMap_First( dmap ); it != NULL; it = DMap_Next( dmap, it ) )
		cycRefCountIncrementV( it->value.pValue[0] );
}
void directRefCountDecrementMapValueV( DMap *dmap )
{
	DNode *it;
	if( dmap == NULL ) return;
	for( it = DMap_First( dmap ); it != NULL; it = DMap_Next( dmap, it ) ){
		if( it->value.pValue->t >= DAO_ARRAY ){
			it->value.pValue->v.p->refCount --;
			it->value.pValue->t = 0;
		}else{
			if( it->value.pValue->t == DAO_ENUM && it->value.pValue->v.e->type ){
				it->value.pValue->v.e->type->refCount --;
				it->value.pValue->v.e->type = NULL;
			}
			DValue_Clear( it->value.pValue );
		}
	}
	DMap_Clear( dmap );
}
void cycRefCountDecrementV( DValue value )
{
	if( value.t == DAO_ENUM ) cycRefCountDecrement( (DaoBase*)value.v.e->type );
	if( value.t < DAO_ARRAY ) return;
	cycRefCountDecrement( value.v.p );
}
void cycRefCountIncrementV( DValue value )
{
	if( value.t == DAO_ENUM ) cycRefCountIncrement( (DaoBase*)value.v.e->type );
	if( value.t < DAO_ARRAY ) return;
	cycRefCountIncrement( value.v.p );
}
void cycRefCountDecrementsV( DVarray *list )
{
	size_t i;
	DValue *data;
	if( list == NULL ) return;
	data = list->data;
	for( i=0; i<list->size; i++ ) cycRefCountDecrementV( data[i] );
}
void cycRefCountIncrementsV( DVarray *list )
{
	size_t i;
	DValue *data;
	if( list == NULL ) return;
	data = list->data;
	for( i=0; i<list->size; i++ ) cycRefCountIncrementV( data[i] );
}
void directRefCountDecrementValue( DValue *value )
{
	if( value->t >= DAO_ARRAY ){
		value->v.p->refCount --;
	}else{
		if( value->t == DAO_ENUM && value->v.e->type ){
			value->v.e->type->refCount --;
			value->v.e->type = NULL;
		}
		DValue_Clear( value );
	}
	value->t = 0;
}
void directRefCountDecrementV( DVarray *list )
{
	size_t i;
	DValue *data;
	if( list == NULL ) return;
	data = list->data;
	for( i=0; i<list->size; i++ ){
		if( data[i].t >= DAO_ARRAY ){
			data[i].v.p->refCount --;
			data[i].t = 0;
		}else if( data[i].t == DAO_ENUM && data[i].v.e->type ){
			data[i].v.e->type->refCount --;
			data[i].v.e->type = NULL;
		}
	}
	DVarray_Clear( list );
}
void cycRefCountDecrementsT( DPtrTuple *list )
{
	size_t i;
	DaoBase **dbases;
	if( list ==NULL ) return;
	dbases = list->items.pBase;
	for( i=0; i<list->size; i++ ) cycRefCountDecrement( dbases[i] );
}
void cycRefCountIncrementsT( DPtrTuple *list )
{
	size_t i;
	DaoBase **dbases;
	if( list ==NULL ) return;
	dbases = list->items.pBase;
	for( i=0; i<list->size; i++ ) cycRefCountIncrement( dbases[i] );
}
void directRefCountDecrementT( DPtrTuple *list )
{
	size_t i;
	DaoBase **dbases;
	if( list ==NULL ) return;
	dbases = list->items.pBase;
	for( i=0; i<list->size; i++ ) if( dbases[i] ) dbases[i]->refCount --;
	DPtrTuple_Clear( list );
}
void cycRefCountDecrementsVT( DVaTuple *list )
{
	size_t i;
	DValue *data;
	if( list ==NULL ) return;
	data = list->data;
	for( i=0; i<list->size; i++ ) cycRefCountDecrementV( data[i] );
}
void cycRefCountIncrementsVT( DVaTuple *list )
{
	size_t i;
	DValue *data;
	if( list ==NULL ) return;
	data = list->data;
	for( i=0; i<list->size; i++ ) cycRefCountIncrementV( data[i] );
}
void directRefCountDecrementVT( DVaTuple *list )
{
	size_t i;
	DValue *data;
	if( list ==NULL ) return;
	data = list->data;
	for( i=0; i<list->size; i++ ){
		if( data[i].t >= DAO_ARRAY ){
			data[i].v.p->refCount --;
			data[i].t = 0;
		}else if( data[i].t == DAO_ENUM && data[i].v.e->type ){
			data[i].v.e->type->refCount --;
			data[i].v.e->type = NULL;
		}
	}
	DVaTuple_Clear( list );
}

DaoLateDeleter dao_late_deleter;
#ifdef DAO_WITH_THREAD
DMutex dao_late_deleter_mutex;
#endif

void DaoLateDeleter_Init()
{
	dao_late_deleter.lock = 0;
	dao_late_deleter.safe = 1;
	dao_late_deleter.version = 0;
	dao_late_deleter.buffer = DArray_New(0);
#ifdef DAO_WITH_THREAD
	DMutex_Init( & dao_late_deleter_mutex );
#endif
}
void DaoLateDeleter_Finish()
{
	dao_late_deleter.safe = 0;
	dao_late_deleter.lock = 0;
	DaoLateDeleter_Update();
	DArray_Delete( dao_late_deleter.buffer );
}
void DaoLateDeleter_Push( void *p )
{
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & dao_late_deleter_mutex );
#endif
	DArray_Append( dao_late_deleter.buffer, p );
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & dao_late_deleter_mutex );
#endif
}
void DaoLateDeleter_Update()
{
	DaoLateDeleter *self = & dao_late_deleter;
	DArray *buffer = self->buffer;
	size_t i;
	switch( (self->safe<<1)|self->lock ){
	case 2 : /* safe=1, lock=0 */
		if( self->buffer->size < 10000 ) break;
		self->safe = 0;
		self->lock = 0;
		self->version += 1;
		break;
	case 0 : /* safe=0, lock=0 */
		self->lock = 1;
#ifdef DAO_WITH_THREAD
		DMutex_Lock( & dao_late_deleter_mutex );
#endif
		for(i=0; i<buffer->size; i++) dao_free( buffer->items.pVoid[i] );
		buffer->size = 0;
#ifdef DAO_WITH_THREAD
		DMutex_Unlock( & dao_late_deleter_mutex );
#endif
		break;
	case 1 : /* safe=0, lock=1 */
		self->safe = 1;
		self->lock = 0;
		break;
	default : break;
	}
}
