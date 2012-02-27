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

#include"dao.h"
#include"daoStdtype.h"
#include"daoValue.h"
#include"daoGC.h"

struct DaoState
{
	DAO_CDATA_COMMON;

	DaoValue *state;
	DaoMutex *lock;
	DaoMutex *defmtx;
	DaoMap *demands;
};

typedef struct DaoState DaoState;
extern DaoTypeBase stateTyper;

DaoState* DaoState_New( DaoType *type, DaoValue *state )
{
	DaoState *res = dao_malloc( sizeof(DaoState) );
	DaoCdata_InitCommon( (DaoCdata*)res, type );
	DaoValue_Copy( state, &res->state );
	res->lock = DaoMutex_New();
	res->defmtx = DaoMutex_New();
	res->demands = DaoMap_New( 0 );
	DaoGC_IncRC( (DaoValue*)res->lock );
	DaoGC_IncRC( (DaoValue*)res->defmtx );
	DaoGC_IncRC( (DaoValue*)res->demands );
	return res;
}

void DaoState_Delete( DaoState *self )
{
	DaoGC_DecRC( self->state );
	DaoGC_DecRC( (DaoValue*)self->lock );
	DaoGC_DecRC( (DaoValue*)self->defmtx );
	DaoGC_DecRC( (DaoValue*)self->demands );
	DaoCdata_FreeCommon( (DaoCdata*)self );
	dao_free( self );
}
static void DaoState_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int remove )
{
	DaoState *self = (DaoState*)p;
	if( self->state ){
		DArray_Append( values, self->state );
		if( remove ) self->state = NULL;
	}
}

extern DaoTypeBase stateTyper;

static void DaoState_Create( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoType *type = DaoProcess_GetReturnType( proc );
	DaoState *res = DaoState_New( type, p[0] );
	DaoProcess_PutValue( proc, (DaoValue*)res );
}

static void DaoState_Value( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoState *self = (DaoState*)DaoValue_CastCdata( p[0] );
	DaoMutex_Lock( self->lock );
	DaoProcess_PutValue( proc, self->state );
	DaoMutex_Unlock( self->lock );
}

static void DaoState_TestSet( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoState *self = (DaoState*)DaoValue_CastCdata( p[0] );
	int set = 0;
	DNode *node;
	DaoMutex_Lock( self->lock );
	if( !DaoValue_Compare( self->state, p[1] ) ){
		DaoValue_Copy( p[2], &self->state );
		set = 1;
		node = DaoMap_First( self->demands );
		while( node && DaoValue_Compare( DNode_Key( node ), self->state ) )
			node = DaoMap_Next( self->demands, node );
		if( node )
			DaoCondVar_BroadCast( (DaoCondVar*)DNode_Value( node ) );
	}
	DaoMutex_Unlock( self->lock );
	DaoProcess_PutInteger( proc, set );
}

static void DaoState_Set( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoState *self = (DaoState*)DaoValue_CastCdata( p[0] );
	DNode *node;
	DaoMutex_Lock( self->lock );
	DaoValue_Copy( p[1], &self->state );
	node = DaoMap_First( self->demands );
	while( node && DaoValue_Compare( DNode_Key( node ), self->state ) )
		node = DaoMap_Next( self->demands, node );
	if( node ){
		DaoCondVar_BroadCast( (DaoCondVar*)DNode_Value( node ) );
		DaoMap_Erase( self->demands, DNode_Key( node ) );
	}
	DaoMutex_Unlock( self->lock );
}

static void DaoState_WaitFor( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoState *self = (DaoState*)DaoValue_CastCdata( p[0] );
	int eq = 0, res = 1;
	DaoValue *state = p[1];
	float timeout;
	DaoCondVar *condvar = NULL;
	DaoMutex_Lock( self->lock );
	if( !DaoValue_Compare( self->state, state ) )
		eq = 1;
	else{
		condvar = (DaoCondVar*)DaoMap_GetValue( self->demands, state );
		if( !condvar ){
			condvar = DaoCondVar_New();
			DaoMap_Insert( self->demands, state, (DaoValue*)condvar );
		}
	}
	DaoMutex_Unlock( self->lock );
	if( !eq ){
		timeout = DaoValue_TryGetFloat( p[3] );
		if( timeout > 0 )
			do
				res = !DaoCondVar_TimedWait( condvar, (DaoMutex*)p[2], timeout );
			while( res && DaoValue_Compare( self->state, state ) );
		else if( timeout == 0 )
			res = 0;
		else
			do
				DaoCondVar_Wait( condvar, (DaoMutex*)p[2] );
			while( DaoValue_Compare( self->state, state ) );
	}
	DaoProcess_PutInteger( proc, res );
}

static void DaoState_WaitFor2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoState *self = (DaoState*)DaoValue_CastCdata( p[0] );
	int eq = 0, res = 1;
	DaoValue *state = p[1];
	float timeout;
	DaoCondVar *condvar = NULL;
	DaoMutex_Lock( self->lock );
	if( !DaoValue_Compare( self->state, state ) )
		eq = 1;
	else{
		condvar = (DaoCondVar*)DaoMap_GetValue( self->demands, state );
		if( !condvar ){
			condvar = DaoCondVar_New();
			DaoMap_Insert( self->demands, state, (DaoValue*)condvar );
		}
	}
	DaoMutex_Unlock( self->lock );
	if( !eq ){
		DaoMutex_Lock( self->defmtx );
		timeout = DaoValue_TryGetFloat( p[2] );
		if( timeout > 0 )
			do
				res = !DaoCondVar_TimedWait( condvar, self->defmtx, timeout );
			while( res && DaoValue_Compare( self->state, state ) );
		else if( timeout == 0 )
			res = 0;
		else
			do
				DaoCondVar_Wait( condvar, self->defmtx );
			while( DaoValue_Compare( self->state, state ) );
		DaoMutex_Unlock( self->defmtx );
	}
	DaoProcess_PutInteger( proc, res );
}

static void DaoState_Waitlist( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoState *self = (DaoState*)DaoValue_CastCdata( p[0] );
	DaoList *list = DaoProcess_PutList( proc );
	DNode *node;
	DaoMutex_Lock( self->lock );
	node = DaoMap_First( self->demands );
	while( node ){
		DaoList_PushBack( list, DNode_Key( node ) );
		node = DaoMap_Next( self->demands, node );
	}
	DaoMutex_Unlock( self->lock );
}

static DaoFuncItem stateMeths[] =
{
	{ DaoState_Create,   "state<@T>( value: @T )" },
	{ DaoState_Value,    "value( self: state<@T> ) => @T" },
	{ DaoState_Set,      "set( self: state<@T>, value: @T )" },
	{ DaoState_TestSet,  "alter( self: state<@T>, from: @T, into: @T ) => int" },
	{ DaoState_WaitFor,  "wait( self: state<@T>, value: @T, mtx: mutex, timeout: float = 0 ) => int" },
	{ DaoState_WaitFor2, "wait( self: state<@T>, value: @T, timeout: float = 0 ) => int" },
	{ DaoState_Waitlist, "waitlist( self: state<@T> ) => list<@T>" },
	{ NULL, NULL }
};

DaoTypeBase stateTyper = {
	"state<@T>", NULL, NULL, stateMeths, {NULL}, {0}, 
	(FuncPtrDel)DaoState_Delete, DaoState_GetGCFields
};

struct QueueItem
{
	DaoValue *value;
	struct QueueItem *next;
	struct QueueItem *previous;
};

typedef struct QueueItem QueueItem;

struct DaoQueue
{
	DAO_CDATA_COMMON;

	QueueItem *head;
	QueueItem *tail;
	volatile int size;
	int capacity;
	DaoMutex *mtx;
	DaoCondVar *pushvar;
	DaoCondVar *popvar;
};

typedef struct DaoQueue DaoQueue;
extern DaoTypeBase queueTyper;

DaoQueue* DaoQueue_New( DaoType *type, int capacity )
{
	DaoQueue *res = (DaoQueue*)dao_malloc( sizeof(DaoQueue) );
	DaoCdata_InitCommon( (DaoCdata*)res, type );
	res->head = res->tail = NULL;
	res->size = 0;
	res->capacity = ( ( capacity < 0 )? 0 : capacity );
	res->mtx = DaoMutex_New();
	res->pushvar = DaoCondVar_New();
	res->popvar = DaoCondVar_New();
	DaoGC_IncRC( (DaoValue*)res->mtx );
	DaoGC_IncRC( (DaoValue*)res->pushvar );
	DaoGC_IncRC( (DaoValue*)res->popvar );
	return res;
}

void DaoQueue_Delete( DaoQueue *self )
{
	QueueItem *item;
	while( self->tail != NULL ){
		item = self->tail;
		self->tail = item->previous;
		DaoGC_DecRC( item->value );
		dao_free( item );
	}
	DaoGC_DecRC( (DaoValue*)self->mtx );
	DaoGC_DecRC( (DaoValue*)self->pushvar );
	DaoGC_DecRC( (DaoValue*)self->popvar );
	DaoCdata_FreeCommon( (DaoCdata*)self );
	dao_free( self );
}

static void DaoQueue_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int remove )
{
	DaoQueue *self = (DaoQueue*)p;
	while( self->tail != NULL ){
		QueueItem *item = self->tail;
		self->tail = item->previous;
		if( item->value ){
			DArray_Append( values, item->value );
			if( remove ) item->value = NULL;
		}
	}
}

static void DaoQueue_Size( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoQueue *self = (DaoQueue*)DaoValue_CastCdata( p[0] );
	int size;
	DaoMutex_Lock( self->mtx );
	size = self->size;
	DaoMutex_Unlock( self->mtx );
	DaoProcess_PutInteger( proc, size );
}

static void DaoQueue_Capacity( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoQueue *self = (DaoQueue*)DaoValue_CastCdata( p[0] );
	DaoProcess_PutInteger( proc, self->capacity );
}

static void DaoQueue_Merge( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoQueue *self = (DaoQueue*)DaoValue_CastCdata( p[0] );
	DaoQueue *other = (DaoQueue*)DaoValue_CastCdata( p[1] );
	int merged = 0;
	DaoMutex_Lock( self->mtx );
	DaoMutex_Lock( other->mtx );
	if( !self->capacity || self->size + other->size <= self->capacity ){
		if( self->size && other->size ){
			self->tail->next = other->head;
			other->head->previous = self->tail;
		}
		else if( !self->size ){
			self->head = other->head;
			self->tail = other->tail;
			DaoCondVar_BroadCast( self->popvar );
		}
		self->size += other->size;
		if( other->capacity && other->size == other->capacity )
			DaoCondVar_BroadCast( other->pushvar );
		other->size = 0;
		other->head = other->tail = NULL;
		merged = 1;
	}
	DaoMutex_Unlock( self->mtx );
	DaoMutex_Unlock( other->mtx );
	if( !merged )
		DaoProcess_RaiseException( proc, DAO_ERROR, "Merging exceeds the queue capacity" );
}

static void DaoQueue_Push( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoQueue *self = (DaoQueue*)DaoValue_CastCdata( p[0] );
	QueueItem *item = (QueueItem*)dao_malloc( sizeof(QueueItem) );
	item->value = NULL;
	DaoValue_Copy( p[1], &item->value );
	item->next = NULL;
	DaoMutex_Lock( self->mtx );
	while( self->capacity && self->size == self->capacity )
		DaoCondVar_Wait( self->pushvar, self->mtx );
	item->previous = self->tail;
	if( self->tail )
		self->tail->next = item;
	else{
		self->head = item;
		DaoCondVar_Signal( self->popvar );
	}
	self->tail = item;
	self->size++;
	DaoMutex_Unlock( self->mtx );
}

static void DaoQueue_TryPush( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoQueue *self = (DaoQueue*)DaoValue_CastCdata( p[0] );
	QueueItem *item = (QueueItem*)dao_malloc( sizeof(QueueItem) );
	float timeout = DaoValue_TryGetFloat( p[2] );
	int pushable = 0, timed = 0;
	item->value = NULL;
	DaoValue_Copy( p[1], &item->value );
	item->next = NULL;
	DaoMutex_Lock( self->mtx );
	if( timeout == 0 )
		pushable = ( !self->capacity || self->size < self->capacity );
	else if( timeout < 0 ){
		while( self->capacity && self->size == self->capacity )
			DaoCondVar_Wait( self->pushvar, self->mtx );
		pushable = 1;
	}
	else{
		while( !timed && self->capacity && self->size == self->capacity )
			timed = DaoCondVar_TimedWait( self->pushvar, self->mtx, timeout );
		pushable = !timed;
	}
	if( pushable ){
		item->previous = self->tail;
		if( self->tail )
			self->tail->next = item;
		else{
			self->head = item;
			DaoCondVar_Signal( self->popvar );
		}
		self->tail = item;
		self->size++;
	}
	DaoMutex_Unlock( self->mtx );
	if( !pushable ){
		DaoGC_DecRC( item->value );
		dao_free( item );
	}
	DaoProcess_PutInteger( proc, pushable );
}

static void DaoQueue_Pop( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoQueue *self = (DaoQueue*)DaoValue_CastCdata( p[0] );
	QueueItem *item = NULL;
	DaoMutex_Lock( self->mtx );
	while( !self->size )
		DaoCondVar_Wait( self->popvar, self->mtx );
	item = self->head;
	self->head = item->next;
	if( !self->head )
		self->tail = NULL;
	else
		self->head->previous = NULL;
	if( self->capacity && self->size == self->capacity )
		DaoCondVar_Signal( self->pushvar );
	self->size--;
	DaoMutex_Unlock( self->mtx );
	DaoProcess_PutValue( proc, item->value );
	DaoGC_DecRC( item->value );
	dao_free( item );
}

static void DaoQueue_TryPop( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoQueue *self = (DaoQueue*)DaoValue_CastCdata( p[0] );
	QueueItem *item = NULL;
	float timeout = DaoValue_TryGetFloat( p[1] );
	int popable = 0, timed = 0;
	DaoMutex_Lock( self->mtx );
	if( timeout == 0 )
		popable = self->size;
	else if( timeout < 0 ){
		while( !self->size )
			DaoCondVar_Wait( self->popvar, self->mtx );
		popable = 1;
	}
	else{
		while( !timed && !self->size )
			timed = DaoCondVar_TimedWait( self->popvar, self->mtx, timeout );
		popable = !timed;
	}
	if( popable ){
		item = self->head;
		self->head = item->next;
		if( !self->head )
			self->tail = NULL;
		else
			self->head->previous = NULL;
		if( self->capacity && self->size == self->capacity )
			DaoCondVar_Signal( self->pushvar );
		self->size--;
	}
	DaoMutex_Unlock( self->mtx );
	DaoProcess_PutValue( proc, item? item->value : dao_none_value );
	if( item ){
		DaoGC_DecRC( item->value );
		dao_free( item );
	}
}

static void DaoQueue_Create( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoType *type = DaoProcess_GetReturnType( proc );
	DaoQueue *res = DaoQueue_New( type, DaoValue_TryGetInteger( p[0] ) );
	DaoProcess_PutValue( proc, (DaoValue*)res );
}

static DaoFuncItem queueMeths[] =
{
	{ DaoQueue_Create,   "queue<@T>( capacity = 0 )" },
	{ DaoQueue_Size,     "size( self: queue<@T> ) => int" },
	{ DaoQueue_Capacity, "capacity( self: queue<@T> ) => int" },
	{ DaoQueue_Push,     "push( self: queue<@T>, value: @T )" },
	{ DaoQueue_TryPush,  "trypush( self: queue<@T>, value: @T, timeout: float = 0 ) => int" },
	{ DaoQueue_Pop,      "pop( self: queue<@T> ) => @T" },
	{ DaoQueue_TryPop,   "trypop( self: queue<@T>, timeout: float = 0 ) => @T|none" },
	{ DaoQueue_Merge,    "merge( self: queue<@T>, other: queue<@T> )" },
	{ NULL, NULL }
};

DaoTypeBase queueTyper = {
	"queue<@T>", NULL, NULL, queueMeths, {NULL}, {0}, 
	(FuncPtrDel)DaoQueue_Delete, DaoQueue_GetGCFields
};

#ifdef DAO_INLINE_SYNC
int DaoSync_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
#else
int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
#endif
{
	DaoNamespace_WrapType( ns, &stateTyper, 0 );
	DaoNamespace_WrapType( ns, &queueTyper, 0 );
	return 0;
}
