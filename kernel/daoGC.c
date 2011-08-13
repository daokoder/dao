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


#define GC_IN_POOL 1
#define GC_MARKED  2

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
	free (strings);
	fflush( debug );
	fclose( debug );
	fflush( stdout );
}
#endif

static void DaoGC_DecRC2( DaoValue *p, int change );
static void cycRefCountDecrement( DaoValue *value );
static void cycRefCountIncrement( DaoValue *value );
static void cycRefCountDecrements( DArray * values );
static void cycRefCountIncrements( DArray * values );
static void directRefCountDecrement( DaoValue **value );
static void directRefCountDecrements( DArray * values );
static void cycRefCountDecrementsT( DTuple * values );
static void cycRefCountIncrementsT( DTuple * values );
static void directRefCountDecrementT( DTuple * values );
static void cycRefCountDecrementMapValue( DMap *dmap );
static void cycRefCountIncrementMapValue( DMap *dmap );
static void directRefCountDecrementMapValue( DMap *dmap );
static void cycRefCountDecreScan();
static void cycRefCountIncreScan();
static void freeGarbage();
static void InitGC();

#ifdef DAO_WITH_THREAD
static void markAliveObjects( DaoValue *root );
static void recycleGarbage( void * );
static void tryInvoke();
#else
static int markAliveObjects();
#endif

static void DaoLateDeleter_Init();
static void DaoLateDeleter_Finish();
static void DaoLateDeleter_Update();

typedef struct DaoGarbageCollector  DaoGarbageCollector;
struct DaoGarbageCollector
{
	DArray   *pool[2];
	DArray   *objAlive;

	short     work, idle;
	int       gcMin, gcMax;
	int       count;
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
static void DaoGC_DeleteSimpleData( DaoValue *value )
{
	if( value == NULL || value->xGC.refCount || value->type >= DAO_ENUM ) return;
	switch( value->type ){
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
	}
}
static void DaoGC_DecRC2( DaoValue *p, int change )
{
	DNode *node;
	const short idle = gcWorker.idle;
	const short work = gcWorker.work;
	int i, n;

	if( p == NULL ) return;
	p->xGC.refCount += change;

	if( p->xGC.refCount == 0 ){
		switch( p->xGC.type ){
		case DAO_INTEGER :
		case DAO_FLOAT :
		case DAO_DOUBLE :
		case DAO_COMPLEX :
			dao_free( p );
			return;
		case DAO_LONG :
			DaoLong_Delete( & p->xLong );
			return;
		case DAO_STRING :
			DaoString_Delete( & p->xString );
			return;
		case DAO_ENUM : break;
		case DAO_LIST :
			{
				DaoList *list = (DaoList*) p;
				if( list->gcState[ work ] & GC_IN_POOL ) break;
				n = list->items->size;
				for(i=0; i<n; i++) DaoGC_DecRC2( list->items->items.pValue[i], -1 );
				DaoGC_DecRC2( (DaoValue*) list->unitype, -1 );
				DaoGC_DecRC2( (DaoValue*) list->meta, -1 );
				list->unitype = NULL;
				list->meta = NULL;
				list->items->size = 0;
				if( list->gcState[ idle ] & GC_IN_POOL ) break;
				DaoList_Delete( list );
				return;
			}
		case DAO_TUPLE :
			{
				DaoTuple *tuple = (DaoTuple*) p;
				if( tuple->gcState[ work ] & GC_IN_POOL ) break;
				n = tuple->items->size;
				for(i=0; i<n; i++) DaoGC_DecRC2( tuple->items->items.pValue[i], -1 );
				DaoGC_DecRC2( (DaoValue*) tuple->unitype, -1 );
				DaoGC_DecRC2( (DaoValue*) tuple->meta, -1 );
				tuple->unitype = NULL;
				tuple->meta = NULL;
				tuple->items->size = 0;
				if( tuple->gcState[ idle ] & GC_IN_POOL ) break;
				DaoTuple_Delete( tuple );
				return;
			}
		case DAO_MAP :
			{
				DaoMap *map = (DaoMap*) p;
				if( map->gcState[ work ] & GC_IN_POOL ) break;
				node = DMap_First( map->items );
				i = 0;
				for( ; node != NULL; node = DMap_Next( map->items, node ) ) {
					DaoGC_DecRC2( node->key.pValue, -1 );
					DaoGC_DecRC2( node->value.pValue, -1 );
					node->key.pInt = ++i; /* keep key ordering */
				}
				map->items->keytype = 0;
				map->items->valtype = 0;
				DMap_Clear( map->items );
				DaoGC_DecRC2( (DaoValue*) map->unitype, -1 );
				DaoGC_DecRC2( (DaoValue*) map->meta, -1 );
				map->unitype = NULL;
				map->meta = NULL;
				if( map->gcState[ idle ] & GC_IN_POOL ) break;
				DaoMap_Delete( map );
				return;
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
				if( ctx->gcState[ work ] & GC_IN_POOL ) break;
				n = ctx->regArray->size;
				for(i=0; i<n; i++) DaoGC_DecRC2( ctx->regArray->items.pValue[i], -1 );
				ctx->regArray->size = 0;
				break;
			}
		default : break;
		}
	}
	/* never push simple data types into GC queue,
	 * because they cannot form cyclic referencing structure: */
	if( p->type < DAO_ENUM ) return;

	/* already in idle pool */
	if( p->xGC.gcState[ idle ] & GC_IN_POOL ) return;
	p->xGC.gcState[ idle ] = GC_IN_POOL;
	DArray_Append( gcWorker.pool[ idle ], p );
#if 0
	if( p->xGC.refCount ==0 ) return; /* already counted */
#endif
	switch( p->type ){
	case DAO_OBJECT :
		if( p->xObject.objData ) gcWorker.count += p->xObject.objData->size + 1;
		break;
	case DAO_LIST : gcWorker.count += p->xList.items->size + 1; break;
	case DAO_MAP  : gcWorker.count += p->xMap.items->size + 1; break;
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY : gcWorker.count += p->xArray.size + 1; break;
#endif
	case DAO_TUPLE : gcWorker.count += p->xTuple.items->size; break;
	case DAO_CONTEXT : gcWorker.count += p->xContext.regArray->size; break;
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

void DaoGC_DecRC( DaoValue *p )
{
	const short idle = gcWorker.idle;
	if( ! p ) return;

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
void DaoGC_IncRC( DaoValue *p )
{
	if( ! p ) return;
	if( p->xGC.refCount == 0 ){
		p->xGC.refCount ++;
		return;
	}

	DMutex_Lock( & gcWorker.mutex_switch_heap );
	p->xGC.refCount ++;
	p->xGC.cycRefCount ++;
	DMutex_Unlock( & gcWorker.mutex_switch_heap );
}
void DaoGC_ShiftRC( DaoValue *up, DaoValue *down )
{
	const short idle = gcWorker.idle;
	if( up == down ) return;

	DMutex_Lock( & gcWorker.mutex_switch_heap );

	if( up ){
		up->xGC.refCount ++;
		up->xGC.cycRefCount ++;
	}
	if( down ) DaoGC_DecRC2( down, -1 );

	DMutex_Unlock( & gcWorker.mutex_switch_heap );

	if( down && gcWorker.pool[ idle ]->size > gcWorker.gcMin ) tryInvoke();
}

void DaoGC_IncRCs( DArray *list )
{
	size_t i;
	DaoValue **values;

	if( list->size == 0 ) return;
	values = list->items.pValue;
	DMutex_Lock( & gcWorker.mutex_switch_heap );
	for( i=0; i<list->size; i++){
		if( values[i] ){
			values[i]->xGC.refCount ++;
			values[i]->xGC.cycRefCount ++;
		}
	}
	DMutex_Unlock( & gcWorker.mutex_switch_heap );
}
void DaoGC_DecRCs( DArray *list )
{
	size_t i;
	DaoValue **values;
	const short idle = gcWorker.idle;
	if( list==NULL || list->size == 0 ) return;
	values = list->items.pValue;
	DMutex_Lock( & gcWorker.mutex_switch_heap );
	for( i=0; i<list->size; i++) if( values[i] ) DaoGC_DecRC2( values[i], -1 );
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
		pool->items.pValue[i]->xGC.cycRefCount = pool->items.pValue[i]->xGC.refCount;

	for( i=0; i<pool->size; i++ ){
		DaoValue *value = pool->items.pValue[i];
		switch( value->type ){
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) value;
				cycRefCountDecrement( (DaoValue*) array->unitype );
				cycRefCountDecrement( (DaoValue*) array->meta );
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple = (DaoTuple*) value;
				cycRefCountDecrement( (DaoValue*) tuple->unitype );
				cycRefCountDecrement( (DaoValue*) tuple->meta );
				cycRefCountDecrementsT( tuple->items );
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
				cycRefCountDecrementsT( obj->superObject );
				cycRefCountDecrementsT( obj->objData );
				cycRefCountDecrement( (DaoValue*) obj->meta );
				cycRefCountDecrement( (DaoValue*) obj->myClass );
				break;
			}
		case DAO_CDATA : case DAO_CTYPE :
			{
				DaoCData *cdata = (DaoCData*) value;
				cycRefCountDecrement( (DaoValue*) cdata->meta );
				cycRefCountDecrement( (DaoValue*) cdata->daoObject );
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
		case DAO_CONTEXT :
			{
				DaoContext *ctx = (DaoContext*)value;
				cycRefCountDecrement( (DaoValue*) ctx->object );
				cycRefCountDecrement( (DaoValue*) ctx->routine );
				cycRefCountDecrementsT( ctx->regArray );
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNameSpace *ns = (DaoNameSpace*) value;
				cycRefCountDecrements( ns->cstData );
				cycRefCountDecrements( ns->varData );
				cycRefCountDecrements( ns->varType );
				cycRefCountDecrements( ns->cmodule->cmethods );
				cycRefCountDecrements( ns->mainRoutines );
				cycRefCountDecrementMapValue( ns->abstypes );
				for(k=0; k<ns->cmodule->ctypers->size; k++){
					DaoTypeBase *typer = (DaoTypeBase*)ns->cmodule->ctypers->items.pValue[k];
					if( typer->priv == NULL ) continue;
					cycRefCountDecrementMapValue( typer->priv->values );
					cycRefCountDecrementMapValue( typer->priv->methods );
				}
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) value;
				cycRefCountDecrement( abtp->aux );
				cycRefCountDecrement( abtp->value );
				cycRefCountDecrements( abtp->nested );
				cycRefCountDecrementMapValue( abtp->interfaces );
				break;
			}
		case DAO_FUTURE :
			{
				DaoFuture *future = (DaoFuture*) value;
				cycRefCountDecrement( future->value );
				cycRefCountDecrement( (DaoValue*) future->unitype );
				cycRefCountDecrement( (DaoValue*) future->context );
				cycRefCountDecrement( (DaoValue*) future->process );
				cycRefCountDecrement( (DaoValue*) future->precondition );
				break;
			}
		case DAO_VMPROCESS :
			{
				DaoVmProcess *vmp = (DaoVmProcess*) value;
				DaoVmFrame *frame = vmp->firstFrame;
				cycRefCountDecrement( vmp->returned );
				cycRefCountDecrements( vmp->parResume );
				cycRefCountDecrements( vmp->parYield );
				cycRefCountDecrements( vmp->exceptions );
				while( frame ){
					cycRefCountDecrement( (DaoValue*) frame->context );
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
	size_t i, j;
	const short work = gcWorker.work;
	DArray *pool = gcWorker.pool[ gcWorker.work ];

	for(j=0; j<2; j++){
		for( i=0; i<pool->size; i++ ){
			DaoValue *value = pool->items.pValue[i];
			if( value->xGC.gcState[work] & GC_MARKED ) continue;
			if( value->type == DAO_CDATA && (value->xGC.refCount ==0 || value->xGC.cycRefCount ==0) ){
				DaoCData *cdata = (DaoCData*) value;
				DaoCDataCore *core = (DaoCDataCore*)cdata->typer->priv;
				if( !(cdata->attribs & DAO_CDATA_FREE) ) continue;
				if( cdata->data == NULL || core == NULL ) continue;
				if( core->DelTest == NULL ) continue;
				if( core->DelTest( cdata->data ) ) continue;
				DaoCData_SetExtReference( cdata, 1 );
			}
			if( value->xGC.cycRefCount >0 ) markAliveObjects( value );
		}
	}
}
void markAliveObjects( DaoValue *root )
{
	size_t i, k;
	const short work = gcWorker.work;
	DArray *objAlive = gcWorker.objAlive;
	DNode *node;

	objAlive->size = 0;
	root->xGC.gcState[work] |= GC_MARKED;
	DArray_Append( objAlive, root );

	for( i=0; i<objAlive->size; i++){
		DaoValue *value = objAlive->items.pValue[i];
		switch( value->type ){
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) value;
				cycRefCountIncrement( (DaoValue*) array->unitype );
				cycRefCountIncrement( (DaoValue*) array->meta );
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple= (DaoTuple*) value;
				cycRefCountIncrement( (DaoValue*) tuple->unitype );
				cycRefCountIncrement( (DaoValue*) tuple->meta );
				cycRefCountIncrementsT( tuple->items );
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
				cycRefCountIncrementsT( obj->superObject );
				cycRefCountIncrementsT( obj->objData );
				cycRefCountIncrement( (DaoValue*) obj->meta );
				cycRefCountIncrement( (DaoValue*) obj->myClass );
				break;
			}
		case DAO_CDATA : case DAO_CTYPE :
			{
				DaoCData *cdata = (DaoCData*) value;
				cycRefCountIncrement( (DaoValue*) cdata->meta );
				cycRefCountIncrement( (DaoValue*) cdata->daoObject );
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
				cycRefCountDecrements( klass->references );
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
		case DAO_CONTEXT :
			{
				DaoContext *ctx = (DaoContext*)value;
				cycRefCountIncrement( (DaoValue*) ctx->object );
				cycRefCountIncrement( (DaoValue*) ctx->routine );
				cycRefCountIncrementsT( ctx->regArray );
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNameSpace *ns = (DaoNameSpace*) value;
				cycRefCountIncrements( ns->cstData );
				cycRefCountIncrements( ns->varData );
				cycRefCountIncrements( ns->varType );
				cycRefCountIncrements( ns->cmodule->cmethods );
				cycRefCountIncrements( ns->mainRoutines );
				cycRefCountIncrementMapValue( ns->abstypes );
				for(k=0; k<ns->cmodule->ctypers->size; k++){
					DaoTypeBase *typer = (DaoTypeBase*)ns->cmodule->ctypers->items.pValue[k];
					if( typer->priv == NULL ) continue;
					cycRefCountIncrementMapValue( typer->priv->values );
					cycRefCountIncrementMapValue( typer->priv->methods );
				}
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) value;
				cycRefCountIncrement( abtp->aux );
				cycRefCountIncrement( abtp->value );
				cycRefCountIncrements( abtp->nested );
				cycRefCountIncrementMapValue( abtp->interfaces );
				break;
			}
		case DAO_FUTURE :
			{
				DaoFuture *future = (DaoFuture*) value;
				cycRefCountIncrement( future->value );
				cycRefCountIncrement( (DaoValue*) future->unitype );
				cycRefCountIncrement( (DaoValue*) future->context );
				cycRefCountIncrement( (DaoValue*) future->process );
				cycRefCountIncrement( (DaoValue*) future->precondition );
				break;
			}
		case DAO_VMPROCESS :
			{
				DaoVmProcess *vmp = (DaoVmProcess*) value;
				DaoVmFrame *frame = vmp->firstFrame;
				cycRefCountIncrement( vmp->returned );
				cycRefCountIncrements( vmp->parResume );
				cycRefCountIncrements( vmp->parYield );
				cycRefCountIncrements( vmp->exceptions );
				while( frame ){
					cycRefCountIncrement( (DaoValue*) frame->context );
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
		DaoValue *value = pool->items.pValue[i];
		value->xGC.gcState[work] = 0;

		if( value->xGC.cycRefCount == 0 || value->xGC.refCount ==0 ){

			DMutex_Lock( & gcWorker.mutex_switch_heap );
			switch( value->type ){

#ifdef DAO_WITH_NUMARRAY
			case DAO_ARRAY :
				{
					DaoArray *array = (DaoArray*) value;
					directRefCountDecrement( (DaoValue**) & array->unitype );
					directRefCountDecrement( (DaoValue**) & array->meta );
					break;
				}
#endif
			case DAO_TUPLE :
				{
					DaoTuple *tuple = (DaoTuple*) value;
					directRefCountDecrementT( tuple->items );
					directRefCountDecrement( (DaoValue**) & tuple->unitype );
					directRefCountDecrement( (DaoValue**) & tuple->meta );
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
					directRefCountDecrementT( obj->superObject );
					directRefCountDecrementT( obj->objData );
					directRefCountDecrement( (DaoValue**) & obj->meta );
					directRefCountDecrement( (DaoValue**) & obj->myClass );
					break;
				}
			case DAO_CDATA : case DAO_CTYPE :
				{
					DaoCData *cdata = (DaoCData*) value;
					directRefCountDecrement( (DaoValue**) & cdata->meta );
					directRefCountDecrement( (DaoValue**) & cdata->daoObject );
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
			case DAO_CONTEXT :
				{
					DaoContext *ctx = (DaoContext*)value;
					directRefCountDecrement( (DaoValue**) & ctx->object );
					directRefCountDecrement( (DaoValue**) & ctx->routine );
					directRefCountDecrementT( ctx->regArray );
					break;
				}
			case DAO_NAMESPACE :
				{
					DaoNameSpace *ns = (DaoNameSpace*) value;
					directRefCountDecrements( ns->cstData );
					directRefCountDecrements( ns->varData );
					directRefCountDecrements( ns->varType );
					directRefCountDecrements( ns->cmodule->cmethods );
					directRefCountDecrements( ns->mainRoutines );
					directRefCountDecrementMapValue( ns->abstypes );
					for(k=0; k<ns->cmodule->ctypers->size; k++){
						DaoTypeBase *typer = (DaoTypeBase*)ns->cmodule->ctypers->items.pValue[k];
						if( typer->priv == NULL ) continue;
						directRefCountDecrementMapValue( typer->priv->values );
						directRefCountDecrementMapValue( typer->priv->methods );
					}
					break;
				}
			case DAO_TYPE :
				{
					DaoType *abtp = (DaoType*) value;
					directRefCountDecrements( abtp->nested );
					directRefCountDecrement( (DaoValue**) & abtp->aux );
					directRefCountDecrement( (DaoValue**) & abtp->value );
					directRefCountDecrementMapValue( abtp->interfaces );
					break;
				}
			case DAO_FUTURE :
				{
					DaoFuture *future = (DaoFuture*) value;
					directRefCountDecrement( (DaoValue**) & future->value );
					directRefCountDecrement( (DaoValue**) & future->unitype );
					directRefCountDecrement( (DaoValue**) & future->context );
					directRefCountDecrement( (DaoValue**) & future->process );
					directRefCountDecrement( (DaoValue**) & future->precondition );
					break;
				}
			case DAO_VMPROCESS :
				{
					DaoVmProcess *vmp = (DaoVmProcess*) value;
					DaoVmFrame *frame = vmp->firstFrame;
					directRefCountDecrement( (DaoValue**) & vmp->returned );
					directRefCountDecrements( vmp->parResume );
					directRefCountDecrements( vmp->parYield );
					directRefCountDecrements( vmp->exceptions );
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
		DaoValue *value = pool->items.pValue[i];
		if( value->xGC.cycRefCount==0 || value->xGC.refCount==0 ){
			if( value->xGC.refCount !=0 ){
				printf(" refCount not zero %i: %i\n", value->type, value->xGC.refCount );

#if DEBUG
				if( value->type == DAO_FUNCTION ){
					DaoFunction *func = (DaoFunction*)value;
					printf( "%s\n", func->routName->mbs );
				}else if( value->type == DAO_TYPE ){
					DaoType *func = (DaoType*)value;
					printf( "%s\n", func->name->mbs );
				}
#endif
				DMutex_Lock( & gcWorker.mutex_switch_heap );
				if( ! ( value->xGC.gcState[ idle ] & GC_IN_POOL ) ){
					value->xGC.gcState[ idle ] = GC_IN_POOL;
					DArray_Append( gcWorker.pool[idle], value );
				}
				DMutex_Unlock( & gcWorker.mutex_switch_heap );
				continue;
			}
			if( ! ( value->xGC.gcState[idle] & GC_IN_POOL ) ){
#ifdef DAO_GC_PROF
				ObjectProfile[value->type] --;
#endif
				/*
				   if( value->type <= DAO_VMPROCESS )
				   if( value->type == DAO_STRING ){
				   DaoString *s = (DaoString*) value;
				   if( s->chars->mbs && s->chars->mbs->xGC.refCount > 1 ){
				   printf( "delete mbstring!!! %i\n", s->chars->mbs->xGC.refCount );
				   }
				   if( s->chars->wcs && s->chars->wcs->xGC.refCount > 1 ){
				   printf( "delete wcstring!!! %i\n", s->chars->wcs->xGC.refCount );
				   }
				   }
				   if( value->type < DAO_STRING )
				   if( value->type != DAO_CONTEXT )
				 */
				if( value->type >= DAO_FUNCTREE && value->type <= DAO_FUNCTION )
					DaoCallbackData_DeleteByCallback( value );
				DaoCallbackData_DeleteByUserdata( value );
				typer = DaoValue_GetTyper( value );
				typer->Delete( value );
			}
		}
	}
#ifdef DAO_GC_PROF
#warning "-------------------- DAO_GC_PROF is turned on."
	printf("heap[idle] = %i;\theap[work] = %i\n", gcWorker.pool[ idle ]->size, gcWorker.pool[ work ]->size );
	printf("=======================================\n");
	//printf( "mbs count = %i\n", daoCountMBS );
	printf( "array count = %i\n", daoCountArray );
	for(i=0; i<100; i++){
		if( ObjectProfile[i] != 0 ){
			printf( "type = %3i; rest = %5i\n", i, ObjectProfile[i] );
		}
	}
#endif
	DArray_Clear( pool );
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
void DaoGC_IncRC( DaoValue *p )
{
	const short work = gcWorker.work;
	if( ! p ) return;
	if( p->xGC.refCount == 0 ){
		p->xGC.refCount ++;
		return;
	}

	p->xGC.refCount ++;
	p->xGC.cycRefCount ++;
	return;
	if( ! ( p->xGC.gcState[work] & GC_IN_POOL ) && gcWorker.workType == GC_INC_RC ){
		if( p->type >= DAO_ENUM ){
			DArray_Append( gcWorker.pool[work], p );
			p->xGC.gcState[work] = GC_IN_POOL;
		}
	}
}
static int counts = 100;
void DaoGC_DecRC( DaoValue *p )
{
	const short idle = gcWorker.idle;
	if( ! p ) return;

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
	DaoValue **data;
	if( list->size == 0 ) return;
	data = list->items.pValue;
	for( i=0; i<list->size; i++) DaoGC_IncRC( data[i] );
}
void DaoGC_DecRCs( DArray *list )
{
	size_t i;
	DaoValue **data;
	if( list == NULL || list->size == 0 ) return;
	data = list->items.pValue;
	for( i=0; i<list->size; i++) DaoGC_DecRC( data[i] );
}
void DaoGC_ShiftRC( DaoValue *up, DaoValue *down )
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
		pool->items.pValue[i]->xGC.cycRefCount = pool->items.pValue[i]->xGC.refCount;
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
		DaoValue *value = pool->items.pValue[i];
		switch( value->type ){
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) value;
				cycRefCountDecrement( (DaoValue*) array->unitype );
				cycRefCountDecrement( (DaoValue*) array->meta );
				j ++;
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple = (DaoTuple*) value;
				cycRefCountDecrement( (DaoValue*) tuple->unitype );
				cycRefCountDecrement( (DaoValue*) tuple->meta );
				cycRefCountDecrementsT( tuple->items );
				j += tuple->items->size;
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
				cycRefCountDecrementsT( obj->superObject );
				cycRefCountDecrementsT( obj->objData );
				if( obj->superObject ) j += obj->superObject->size;
				if( obj->objData ) j += obj->objData->size;
				cycRefCountDecrement( (DaoValue*) obj->meta );
				cycRefCountDecrement( (DaoValue*) obj->myClass );
				break;
			}
		case DAO_CDATA : case DAO_CTYPE :
			{
				DaoCData *cdata = (DaoCData*) value;
				cycRefCountDecrement( (DaoValue*) cdata->meta );
				cycRefCountDecrement( (DaoValue*) cdata->daoObject );
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
				cycRefCountDecrementMapValue( inter->methods );
				cycRefCountDecrements( inter->supers );
				cycRefCountDecrement( (DaoValue*) inter->abtype );
				j += inter->supers->size + inter->methods->size;
				break;
			}
		case DAO_CONTEXT :
			{
				DaoContext *ctx = (DaoContext*)value;
				cycRefCountDecrement( (DaoValue*) ctx->object );
				cycRefCountDecrement( (DaoValue*) ctx->routine );
				cycRefCountDecrementsT( ctx->regArray );
				j += ctx->regArray->size + 3;
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNameSpace *ns = (DaoNameSpace*) value;
				cycRefCountDecrements( ns->cstData );
				cycRefCountDecrements( ns->varData );
				cycRefCountDecrements( ns->varType );
				cycRefCountDecrements( ns->cmodule->cmethods );
				cycRefCountDecrements( ns->mainRoutines );
				j += ns->cstData->size + ns->varData->size + ns->abstypes->size;
				cycRefCountDecrementMapValue( ns->abstypes );
				for(k=0; k<ns->cmodule->ctypers->size; k++){
					DaoTypeBase *typer = (DaoTypeBase*)ns->cmodule->ctypers->items.pValue[k];
					if( typer->priv == NULL ) continue;
					cycRefCountDecrementMapValue( typer->priv->values );
					cycRefCountDecrementMapValue( typer->priv->methods );
				}
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) value;
				cycRefCountDecrement( abtp->aux );
				cycRefCountDecrement( abtp->value );
				cycRefCountDecrements( abtp->nested );
				cycRefCountDecrementMapValue( abtp->interfaces );
				break;
		case DAO_FUTURE :
			{
				DaoFuture *future = (DaoFuture*) value;
				cycRefCountDecrement( future->value );
				cycRefCountDecrement( (DaoValue*) future->unitype );
				cycRefCountDecrement( (DaoValue*) future->context );
				cycRefCountDecrement( (DaoValue*) future->process );
				cycRefCountDecrement( (DaoValue*) future->precondition );
				break;
			}
			}
		case DAO_VMPROCESS :
			{
				DaoVmProcess *vmp = (DaoVmProcess*) value;
				DaoVmFrame *frame = vmp->firstFrame;
				cycRefCountDecrement( vmp->returned );
				cycRefCountDecrements( vmp->parResume );
				cycRefCountDecrements( vmp->parYield );
				cycRefCountDecrements( vmp->exceptions );
				while( frame ){
					cycRefCountDecrement( (DaoValue*) frame->context );
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
	size_t k = 0;
	size_t i = gcWorker.ii;
	const short work = gcWorker.work;
	DArray *pool = gcWorker.pool[ gcWorker.work ];
	DArray *objAlive = gcWorker.objAlive;

	if( gcWorker.jj ){
		k += markAliveObjects();
		if( gcWorker.jj ) return;
	}

	for( ; i<pool->size; i++ ){
		DaoValue *value = pool->items.pValue[i];
		if( value->xGC.gcState[work] & GC_MARKED ) continue;
		if( value->type == DAO_CDATA && (value->xGC.refCount ==0 || value->xGC.cycRefCount ==0) ){
			DaoCData *cdata = (DaoCData*) value;
			DaoCDataCore *core = (DaoCDataCore*)cdata->typer->priv;
			if( !(cdata->attribs & DAO_CDATA_FREE) ) continue;
			if( cdata->data == NULL || core == NULL ) continue;
			if( core->DelTest == NULL ) continue;
			if( core->DelTest( cdata->data ) ) continue;
			DaoCData_SetExtReference( cdata, 1 );
		}
		if( value->xGC.cycRefCount >0 ){
			objAlive->size = 0;
			value->xGC.gcState[work] |= GC_MARKED;
			DArray_Append( objAlive, value );
			k += markAliveObjects();
			if( gcWorker.jj || k >= gcWorker.gcMin ) break;
		}
	}
	if( i >= pool->size ){
		gcWorker.ii = 0;
		gcWorker.workType ++;
	}else{
		gcWorker.ii = i+1;
	}
}
int markAliveObjects()
{
	size_t i, k = 9;
	size_t j = gcWorker.jj;
	const short work = gcWorker.work;
	DArray *objAlive = gcWorker.objAlive;
	DNode *node;

	for( ; j<objAlive->size; j++){
		DaoValue *value = objAlive->items.pValue[j];
		k ++;
		switch( value->type ){
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			{
				DaoArray *array = (DaoArray*) value;
				cycRefCountIncrement( (DaoValue*) array->unitype );
				cycRefCountIncrement( (DaoValue*) array->meta );
				break;
			}
#endif
		case DAO_TUPLE :
			{
				DaoTuple *tuple= (DaoTuple*) value;
				cycRefCountIncrement( (DaoValue*) tuple->unitype );
				cycRefCountIncrement( (DaoValue*) tuple->meta );
				cycRefCountIncrementsT( tuple->items );
				k += tuple->items->size;
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
				cycRefCountIncrementsT( obj->superObject );
				cycRefCountIncrementsT( obj->objData );
				cycRefCountIncrement( (DaoValue*) obj->meta );
				cycRefCountIncrement( (DaoValue*) obj->myClass );
				if( obj->superObject ) k += obj->superObject->size;
				if( obj->objData ) k += obj->objData->size;
				break;
			}
		case DAO_CDATA : case DAO_CTYPE :
			{
				DaoCData *cdata = (DaoCData*) value;
				cycRefCountIncrement( (DaoValue*) cdata->meta );
				cycRefCountIncrement( (DaoValue*) cdata->daoObject );
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
		case DAO_CONTEXT :
			{
				DaoContext *ctx = (DaoContext*)value;
				cycRefCountIncrement( (DaoValue*) ctx->object );
				cycRefCountIncrement( (DaoValue*) ctx->routine );
				cycRefCountIncrementsT( ctx->regArray );
				k += ctx->regArray->size + 3;
				break;
			}
		case DAO_NAMESPACE :
			{
				DaoNameSpace *ns = (DaoNameSpace*) value;
				cycRefCountIncrements( ns->cstData );
				cycRefCountIncrements( ns->varData );
				cycRefCountIncrements( ns->varType );
				cycRefCountIncrements( ns->cmodule->cmethods );
				cycRefCountIncrements( ns->mainRoutines );
				k += ns->cstData->size + ns->varData->size + ns->abstypes->size;
				cycRefCountIncrementMapValue( ns->abstypes );
				for(k=0; k<ns->cmodule->ctypers->size; k++){
					DaoTypeBase *typer = (DaoTypeBase*)ns->cmodule->ctypers->items.pValue[k];
					if( typer->priv == NULL ) continue;
					cycRefCountIncrementMapValue( typer->priv->values );
					cycRefCountIncrementMapValue( typer->priv->methods );
				}
				break;
			}
		case DAO_TYPE :
			{
				DaoType *abtp = (DaoType*) value;
				cycRefCountIncrement( abtp->aux );
				cycRefCountIncrement( abtp->value );
				cycRefCountIncrements( abtp->nested );
				cycRefCountIncrementMapValue( abtp->interfaces );
				break;
			}
		case DAO_FUTURE :
			{
				DaoFuture *future = (DaoFuture*) value;
				cycRefCountIncrement( future->value );
				cycRefCountIncrement( (DaoValue*) future->unitype );
				cycRefCountIncrement( (DaoValue*) future->context );
				cycRefCountIncrement( (DaoValue*) future->process );
				cycRefCountIncrement( (DaoValue*) future->precondition );
				break;
			}
		case DAO_VMPROCESS :
			{
				DaoVmProcess *vmp = (DaoVmProcess*) value;
				DaoVmFrame *frame = vmp->firstFrame;
				cycRefCountIncrement( vmp->returned );
				cycRefCountIncrements( vmp->parResume );
				cycRefCountIncrements( vmp->parYield );
				cycRefCountIncrements( vmp->exceptions );
				while( frame ){
					cycRefCountIncrement( (DaoValue*) frame->context );
					frame = frame->next;
				}
				break;
			}
		default: break;
		}
		if( k >= gcWorker.gcMin ) break;
	}
	if( j >= objAlive->size ){
		gcWorker.jj = 0;
	}else{
		gcWorker.jj = j+1;
	}
	return k;
}
void directDecRC()
{
	DArray *pool = gcWorker.pool[ gcWorker.work ];
	DNode *node;
	const short work = gcWorker.work;
	size_t i = gcWorker.ii;
	size_t j = 0, k;

	for( ; i<pool->size; i++ ){
		DaoValue *value = pool->items.pValue[i];
		j ++;
		if( value->xGC.cycRefCount == 0 ){

			switch( value->type ){

#ifdef DAO_WITH_NUMARRAY
			case DAO_ARRAY :
				{
					DaoArray *array = (DaoArray*) value;
					directRefCountDecrement( (DaoValue**) & array->unitype );
					directRefCountDecrement( (DaoValue**) & array->meta );
					break;
				}
#endif
			case DAO_TUPLE :
				{
					DaoTuple *tuple = (DaoTuple*) value;
					j += tuple->items->size;
					directRefCountDecrementT( tuple->items );
					directRefCountDecrement( (DaoValue**) & tuple->unitype );
					directRefCountDecrement( (DaoValue**) & tuple->meta );
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
					if( obj->superObject ) j += obj->superObject->size;
					if( obj->objData ) j += obj->objData->size;
					directRefCountDecrementT( obj->superObject );
					directRefCountDecrementT( obj->objData );
					directRefCountDecrement( (DaoValue**) & obj->meta );
					directRefCountDecrement( (DaoValue**) & obj->myClass );
					break;
				}
			case DAO_CDATA : case DAO_CTYPE :
				{
					DaoCData *cdata = (DaoCData*) value;
					directRefCountDecrement( (DaoValue**) & cdata->meta );
					directRefCountDecrement( (DaoValue**) & cdata->daoObject );
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
					j += inter->supers->size + inter->methods->size;
					directRefCountDecrementMapValue( inter->methods );
					directRefCountDecrements( inter->supers );
					directRefCountDecrement( (DaoValue**) & inter->abtype );
					break;
				}
			case DAO_CONTEXT :
				{
					DaoContext *ctx = (DaoContext*)value;
					directRefCountDecrement( (DaoValue**) & ctx->object );
					directRefCountDecrement( (DaoValue**) & ctx->routine );
					j += ctx->regArray->size + 3;
					directRefCountDecrementT( ctx->regArray );
					break;
				}
			case DAO_NAMESPACE :
				{
					DaoNameSpace *ns = (DaoNameSpace*) value;
					j += ns->cstData->size + ns->varData->size + ns->abstypes->size;
					directRefCountDecrements( ns->cstData );
					directRefCountDecrements( ns->varData );
					directRefCountDecrements( ns->varType );
					directRefCountDecrements( ns->cmodule->cmethods );
					directRefCountDecrements( ns->mainRoutines );
					directRefCountDecrementMapValue( ns->abstypes );
					for(k=0; k<ns->cmodule->ctypers->size; k++){
						DaoTypeBase *typer = (DaoTypeBase*)ns->cmodule->ctypers->items.pValue[k];
						if( typer->priv == NULL ) continue;
						directRefCountDecrementMapValue( typer->priv->values );
						directRefCountDecrementMapValue( typer->priv->methods );
					}
					break;
				}
			case DAO_TYPE :
				{
					DaoType *abtp = (DaoType*) value;
					directRefCountDecrements( abtp->nested );
					directRefCountDecrement( (DaoValue**) & abtp->aux );
					directRefCountDecrement( (DaoValue**) & abtp->value );
					directRefCountDecrementMapValue( abtp->interfaces );
					break;
				}
			case DAO_FUTURE :
				{
					DaoFuture *future = (DaoFuture*) value;
					directRefCountDecrement( (DaoValue**) & future->value );
					directRefCountDecrement( (DaoValue**) & future->unitype );
					directRefCountDecrement( (DaoValue**) & future->context );
					directRefCountDecrement( (DaoValue**) & future->process );
					directRefCountDecrement( (DaoValue**) & future->precondition );
					break;
				}
			case DAO_VMPROCESS :
				{
					DaoVmProcess *vmp = (DaoVmProcess*) value;
					DaoVmFrame *frame = vmp->firstFrame;
					directRefCountDecrement( (DaoValue**) & vmp->returned );
					directRefCountDecrements( vmp->parResume );
					directRefCountDecrements( vmp->parYield );
					directRefCountDecrements( vmp->exceptions );
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
	if( i >= pool->size ){
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
	size_t i = gcWorker.ii;
	size_t j = 0;

	for( ; i<pool->size; i++ ){
		DaoValue *value = pool->items.pValue[i];
		value->xGC.gcState[work] = 0;
		j ++;
		if( value->xGC.cycRefCount==0 ){
			if( value->xGC.refCount !=0 ){
				printf(" refCount not zero %p %i: %i, %i\n", value, value->type, value->xGC.refCount, value->xGC.trait);
#if DEBUG
				if( value->type == DAO_FUNCTION ){
					DaoFunction *func = (DaoFunction*)value;
					printf( "%s\n", func->routName->mbs );
				}else if( value->type == DAO_TYPE ){
					DaoType *func = (DaoType*)value;
					printf( "%s\n", func->name->mbs );
				}else if( value->type == DAO_CDATA ){
					DaoCData *cdata = (DaoCData*) value;
					printf( "%s\n", cdata->typer->name );
				}
#endif
				if( ! ( value->xGC.gcState[ idle ] & GC_IN_POOL ) ){
					value->xGC.gcState[ idle ] = GC_IN_POOL;
					DArray_Append( gcWorker.pool[idle], value );
				}
				continue;
			}
			if( ! ( value->xGC.gcState[idle] & GC_IN_POOL ) ){
#ifdef DAO_GC_PROF
				ObjectProfile[value->type] --;
#endif
				/*
				   if( value->type <= DAO_VMPROCESS )
				   if( value->type == DAO_STRING ){
				   DaoString *s = (DaoString*) value;
				   if( s->chars->mbs && s->chars->mbs->refCount > 1 ){
				   printf( "delete mbstring!!! %i\n", s->chars->mbs->refCount );
				   }
				   if( s->chars->wcs && s->chars->wcs->refCount > 1 ){
				   printf( "delete wcstring!!! %i\n", s->chars->wcs->refCount );
				   }
				   }
				   if( value->type == DAO_FUNCTION ) printf( "here\n" );
				   if( value->type < DAO_STRING )
				 */
				if( value->type >= DAO_FUNCTREE && value->type <= DAO_FUNCTION )
					DaoCallbackData_DeleteByCallback( value );
				DaoCallbackData_DeleteByUserdata( value );
				typer = DaoValue_GetTyper( value );
				typer->Delete( value );
			}
		}
		if( j >= gcWorker.gcMin ) break;
	}
#ifdef DAO_GC_PROF
	printf("heap[idle] = %i;\theap[work] = %i\n", gcWorker.pool[ idle ]->size, gcWorker.pool[ work ]->size );
	printf("=======================================\n");
	//printf( "mbs count = %i\n", daoCountMBS );
	printf( "array count = %i\n", daoCountArray );
	int k;
	for(k=0; k<100; k++){
		if( ObjectProfile[k] > 0 ){
			printf( "type = %3i; rest = %5i\n", k, ObjectProfile[k] );
		}
	}
#endif
	if( i >= pool->size ){
		gcWorker.ii = 0;
		gcWorker.workType = 0;
		gcWorker.count = 0;
		DArray_Clear( pool );
	}else{
		gcWorker.ii = i+1;
	}
}
#endif
void cycRefCountDecrement( DaoValue *value )
{
	const short work = gcWorker.work;
	if( value == NULL ) return;
	/* do not scan simple data types, as they cannot from cyclic structure: */
	if( value->type < DAO_ENUM ) return;
	if( ! ( value->xGC.gcState[work] & GC_IN_POOL ) ){
		DArray_Append( gcWorker.pool[work], value );
		value->xGC.gcState[work] = GC_IN_POOL;
		value->xGC.cycRefCount = value->xGC.refCount;
	}
	value->xGC.cycRefCount --;

	if( value->xGC.cycRefCount<0 ){
		   printf("cycRefCount<0 : %i\n", value->type);
		/*
		 */
		value->xGC.cycRefCount = 0;
	}
}
void cycRefCountIncrement( DaoValue *value )
{
	const short work = gcWorker.work;
	if( value == NULL ) return;
	/* do not scan simple data types, as they cannot from cyclic structure: */
	if( value->type < DAO_ENUM ) return;
	value->xGC.cycRefCount++;
	if( ! ( value->xGC.gcState[work] & GC_MARKED ) ){
		value->xGC.gcState[work] |= GC_MARKED;
		DArray_Append( gcWorker.objAlive, value );
	}
}
void cycRefCountDecrements( DArray *list )
{
	DaoValue **values;
	size_t i;
	if( list == NULL ) return;
	values = list->items.pValue;
	for( i=0; i<list->size; i++ ) cycRefCountDecrement( values[i] );
}
void cycRefCountIncrements( DArray *list )
{
	DaoValue **values;
	size_t i;
	if( list == NULL ) return;
	values = list->items.pValue;
	for( i=0; i<list->size; i++ ) cycRefCountIncrement( values[i] );
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
	DaoValue **values;
	size_t i;
	if( list == NULL ) return;
	values = list->items.pValue;
	for( i=0; i<list->size; i++ ){
		DaoValue *p = values[i];
		if( p == NULL ) continue;
		p->xGC.refCount --;
		if( p->xGC.refCount == 0 && p->type < DAO_ENUM ) DaoGC_DeleteSimpleData( p );
	}
	list->size = 0;
	DArray_Clear( list );
}
void cycRefCountDecrementMapValue( DMap *dmap )
{
	DNode *it;
	if( dmap == NULL ) return;
	for( it = DMap_First( dmap ); it != NULL; it = DMap_Next( dmap, it ) )
		cycRefCountDecrement( it->value.pValue );
}
void cycRefCountIncrementMapValue( DMap *dmap )
{
	DNode *it;
	if( dmap == NULL ) return;
	for( it = DMap_First( dmap ); it != NULL; it = DMap_Next( dmap, it ) )
		cycRefCountIncrement( it->value.pValue );
}
void directRefCountDecrementMapValue( DMap *dmap )
{
	DNode *it;
	if( dmap == NULL ) return;
	for( it = DMap_First( dmap ); it != NULL; it = DMap_Next( dmap, it ) ){
		DaoValue *p = it->value.pValue;
		if( p == NULL ) continue;
		p->xGC.refCount --;
		if( p->xGC.refCount == 0 && p->type < DAO_ENUM ) DaoGC_DeleteSimpleData( p );
	}
	dmap->valtype = 0;
	DMap_Clear( dmap );
}
void cycRefCountDecrementsT( DTuple *list )
{
	size_t i;
	DaoValue **values;
	if( list ==NULL ) return;
	values = list->items.pValue;
	for( i=0; i<list->size; i++ ) cycRefCountDecrement( values[i] );
}
void cycRefCountIncrementsT( DTuple *list )
{
	size_t i;
	DaoValue **values;
	if( list ==NULL ) return;
	values = list->items.pValue;
	for( i=0; i<list->size; i++ ) cycRefCountIncrement( values[i] );
}
void directRefCountDecrementT( DTuple *list )
{
	size_t i;
	DaoValue **values;
	if( list ==NULL ) return;
	values = list->items.pValue;
	for( i=0; i<list->size; i++ ){
		DaoValue *p = values[i];
		if( p == NULL ) continue;
		p->xGC.refCount --;
		if( p->xGC.refCount == 0 && p->type < DAO_ENUM ) DaoGC_DeleteSimpleData( p );
	}
	list->size = 0;
	DTuple_Clear( list );
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
