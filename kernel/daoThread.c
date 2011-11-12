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
#include<math.h>

#include"daoThread.h"
#include"daoMap.h"
#include"daoGC.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoVmspace.h"
#include"daoRoutine.h"
#include"daoObject.h"
#include"daoClass.h"
#include"daoValue.h"
#include"daoSched.h"

#ifdef DAO_WITH_THREAD
/* Basic threading interfaces */

static void DSema_SetValue( DSema *self, int n );
static int  DSema_GetValue( DSema *self );

static void DThread_Detach( DThread *self );
static void DThread_Cancel( DThread *self );
static void DThread_TestCancel( DThread *self );

static dao_thdspec_t thdSpecKey = 0;

#ifdef UNIX

void DMutex_Init( DMutex *self )
{
	pthread_mutex_init( & self->myMutex, NULL );
}
void DMutex_Destroy( DMutex *self )
{
	pthread_mutex_destroy( & self->myMutex );
}
void DMutex_Lock( DMutex *self )
{
	pthread_mutex_lock( & self->myMutex );
}
void DMutex_Unlock( DMutex *self )
{
	pthread_mutex_unlock( & self->myMutex );
}
int DMutex_TryLock( DMutex *self )
{
	return (pthread_mutex_trylock( & self->myMutex ) ==0);
}

void DCondVar_Init( DCondVar *self )
{
	pthread_cond_init( & self->myCondVar, NULL );
}
void DCondVar_Destroy( DCondVar *self )
{
	pthread_cond_destroy( & self->myCondVar );
}
void DCondVar_Wait( DCondVar *self, DMutex *mutex )
{
	pthread_cond_wait( & self->myCondVar, & mutex->myMutex );
}
int DCondVar_TimedWait( DCondVar *self, DMutex *mutex, double seconds )
{
	long sec = floor( seconds );
	long nsec = (long)(( seconds - sec ) * 1E9);
	struct timeval now;
	struct timespec timeout;
	int retc = 0;

	gettimeofday(&now, NULL);
	timeout.tv_sec = now.tv_sec + sec;
	timeout.tv_nsec = now.tv_usec * 1000 + nsec;
	if( timeout.tv_nsec >= 1E9 ){
		timeout.tv_sec ++;
		timeout.tv_nsec -= 1E9;
	}

	retc = pthread_cond_timedwait( & self->myCondVar, & mutex->myMutex, & timeout );
	return ( retc == ETIMEDOUT );
}

void DCondVar_Signal( DCondVar *self )
{
	pthread_cond_signal( & self->myCondVar );
}
void DCondVar_BroadCast( DCondVar *self )
{
	pthread_cond_broadcast( & self->myCondVar );
}

void DSema_Init( DSema *self, int n )
{
	sem_init( & self->mySema, 0, n );
}
void DSema_Destroy( DSema *self )
{
	sem_destroy( & self->mySema );
}
void DSema_Wait( DSema *self )
{
	sem_wait( & self->mySema );
}
void DSema_Post( DSema *self )
{
	sem_post( & self->mySema );
}

void DSema_SetValue( DSema *self, int n )
{
	sem_init( & self->mySema, 0, n );
}
int  DSema_GetValue( DSema *self )
{
	int n;
	sem_getvalue( & self->mySema, & n );
	return n;
}

void DThread_Init( DThread *self )
{
	self->cleaner = NULL;
	self->myThread = 0;
	self->taskFunc = NULL;
	self->taskArg = NULL;
	self->thdSpecData = NULL;
	DCondVar_Init( & self->condv );
}
void DThread_Destroy( DThread *self )
{
	DCondVar_Destroy( & self->condv );
}

void DThread_Wrapper( DThread *self )
{
	if( self->thdSpecData == NULL ){
		self->thdSpecData = (DThreadData*)dao_calloc( 1, sizeof(DThreadData) );
		self->thdSpecData->thdObject = self;
	}
	self->thdSpecData->state = 0;
	pthread_setspecific( thdSpecKey, self->thdSpecData );

	if( self->cleaner ){
		pthread_cleanup_push( self->cleaner, self->taskArg );
		if( self->taskFunc ) self->taskFunc( self->taskArg );
		pthread_cleanup_pop( 1 );
	}else{
		if( self->taskFunc ) self->taskFunc( self->taskArg );
	}
	pthread_exit( 0 );
}

typedef void* (*DThreadCast)(void*);

int DThread_Start( DThread *self, DThreadTask task, void *arg )
{
	pthread_attr_t tattr;
	/*
	   size_t stacksize = 0;
	   int ret;
	   ret = pthread_attr_getstacksize(&tattr, &stacksize);
	 */

	self->taskFunc = task;
	self->taskArg = arg;
	pthread_attr_init(&tattr);
	pthread_attr_setstacksize(&tattr, 0xffff);
	return ( 0 == pthread_create( & self->myThread, &tattr, 
				(DThreadCast) &DThread_Wrapper, (void*)self ) );
}
void DThread_Join( DThread *self )
{
	pthread_join( self->myThread, NULL );
}
void DThread_Detach( DThread *self )
{
	pthread_detach( self->myThread );
}
void DThread_Cancel( DThread *self )
{
	pthread_cancel( self->myThread );
}
void DThread_TestCancel( DThread *self )
{
	pthread_testcancel();
}
void DThread_Exit( DThread *self )
{
	pthread_exit( NULL );
}

dao_thread_t DThread_Self()
{
	return pthread_self();
}
int DThread_Equal( dao_thread_t x, dao_thread_t y )
{
	return pthread_equal( x, y );
}
DThreadData* DThread_GetSpecific()
{
	return (DThreadData*) pthread_getspecific( thdSpecKey );
}

void DaoInitThreadSys()
{
	pthread_key_create( & thdSpecKey, free );
}

#elif WIN32

#if _WIN32_WINNT < 0x0400
#define _WIN32_WINNT 0x0400
#endif

void DMutex_Init( DMutex *self )
{
	InitializeCriticalSection( & self->myMutex );
}
void DMutex_Destroy( DMutex *self )
{
	DeleteCriticalSection( & self->myMutex );
}
void DMutex_Lock( DMutex *self )
{
	EnterCriticalSection( & self->myMutex );
}
void DMutex_Unlock( DMutex *self )
{
	LeaveCriticalSection( & self->myMutex );
}
int DMutex_TryLock( DMutex *self )
{
	return TryEnterCriticalSection( & self->myMutex );
}

void DCondVar_Init( DCondVar *self )
{
	self->thdWaiting = DArray_New(0);
	DMutex_Init( & self->thdMutex );
	/* manual reset, when signaled, all waiting threads will be waked up: */
	self->myCondVar = CreateEvent( NULL, TRUE, FALSE, NULL );
}
void DCondVar_Destroy( DCondVar *self )
{
	DArray_Delete( self->thdWaiting );
	DMutex_Destroy( & self->thdMutex );  
	CloseHandle( self->myCondVar );
}

void DCondVar_Wait( DCondVar *self, DMutex *mtx )
{
	DThreadData *p = (DThreadData*)TlsGetValue( thdSpecKey );

	DMutex_Lock( & self->thdMutex );
	DArray_PushBack( self->thdWaiting, (void*) p->thdObject );
	DMutex_Unlock( & self->thdMutex );

	if( mtx ) DMutex_Unlock( mtx );
	WaitForSingleObject( p->thdObject->condv.myCondVar, INFINITE );
	ResetEvent( p->thdObject->condv.myCondVar );
	if( mtx ) DMutex_Lock( mtx );

	if( p->state & DTHREAD_CANCELED ) DThread_Exit( p->thdObject );
}
int DCondVar_TimedWait( DCondVar *self, DMutex *mtx, double seconds )
{
	DWORD retc;
	DThreadData *p = (DThreadData*)TlsGetValue( thdSpecKey );

	DMutex_Lock( & self->thdMutex );
	DArray_PushBack( self->thdWaiting, (void*) p->thdObject );
	DMutex_Unlock( & self->thdMutex );

	if( mtx ) DMutex_Unlock( mtx );
	retc = WaitForSingleObject( p->thdObject->condv.myCondVar, (DWORD)( seconds * 1000 ) );
	ResetEvent( p->thdObject->condv.myCondVar );
	if( mtx ) DMutex_Lock( mtx );

	if( p->state & DTHREAD_CANCELED ) DThread_Exit( p->thdObject );
	return ( retc == WAIT_TIMEOUT );
}
void DCondVar_Signal( DCondVar *self )
{
	DThread *thread;
	DMutex_Lock( & self->thdMutex );
	if( self->thdWaiting->size > 0 ){
		thread = (DThread*) self->thdWaiting->items.pVoid[0];
		SetEvent( thread->condv.myCondVar );
		DArray_PopFront( self->thdWaiting );
	}
	DMutex_Unlock( & self->thdMutex );

}
void DCondVar_BroadCast( DCondVar *self )
{
	DThread *thread;
	int i;
	DMutex_Lock( & self->thdMutex );
	for( i=0; i<self->thdWaiting->size; i++ ){
		thread = (DThread*) self->thdWaiting->items.pVoid[i];
		SetEvent( thread->condv.myCondVar );
	}
	DArray_Clear( self->thdWaiting );
	DMutex_Unlock( & self->thdMutex );
}

void DSema_Init( DSema *self, int n )
{
	self->mySema = CreateSemaphore( NULL, n, n, NULL );
	self->count = n;
}
void DSema_Destroy( DSema *self )
{
	CloseHandle( self->mySema );
}
void DSema_Wait( DSema *self )
{
	WaitForSingleObject ( self->mySema, INFINITE );
	self->count --;
}
void DSema_Post( DSema *self )
{
	ReleaseSemaphore( self->mySema, 1, NULL );
	self->count ++;
}
int DSema_GetValue( DSema *self )
{
	return self->count;
}
void DSema_SetValue( DSema *self, int n )
{
	CloseHandle( self->mySema );
	self->mySema = CreateSemaphore( NULL, 0, n, NULL );
	self->count = n;
}

void DThread_Init( DThread *self )
{
	self->myThread = 0;
	self->thdSpecData = NULL;
	self->cleaner = NULL;
	self->taskFunc = NULL;
	self->taskArg = NULL;
	DCondVar_Init( & self->condv );
}
void DThread_Destroy( DThread *self )
{
	if( self->myThread ) CloseHandle( self->myThread );
	DCondVar_Destroy( & self->condv );
	GlobalFree( self->thdSpecData );
	self->thdSpecData = NULL;
}
void DThread_Wrapper( void *object )
{
	DThread *self = (DThread*)object;
	self->running = 1;

	if( self->thdSpecData == NULL ){
		self->thdSpecData = (DThreadData*)GlobalAlloc( GPTR, sizeof(DThreadData) );
		self->thdSpecData->thdObject = self;
	}
	self->thdSpecData->state = 0;
	TlsSetValue( thdSpecKey, self->thdSpecData );

	if( self->taskFunc ) self->taskFunc( self->taskArg );
	DThread_Exit( self );
}
int DThread_Start( DThread *self, DThreadTask task, void *arg )
{
	self->taskFunc = task;
	self->taskArg = arg;
	self->myThread = (HANDLE)_beginthread( DThread_Wrapper, 0, (void*)self );
	return (self->myThread != 0);
}
void DThread_Join( DThread *self )
{
	if( self->running ) DCondVar_Wait( & self->condv, NULL );
}
void DThread_Detach( DThread *self )
{
	DCondVar_Signal( & self->condv );
}
void DThread_Cancel( DThread *self )
{
	self->thdSpecData->state |= DTHREAD_CANCELED;
	DCondVar_Signal( & self->condv );
}
void DThread_TestCancel( DThread *self )
{
	if( self->thdSpecData->state & DTHREAD_CANCELED ){
		self->thdSpecData->state = 0;
		DThread_Exit( self );
	}
}
dao_thread_t DThread_Self()
{
	return GetCurrentThread();
}
int DThread_Equal( dao_thread_t x, dao_thread_t y )
{
	return ( x == y );
}
void DThread_Exit( DThread *thd )
{
	thd->running = 0;
	DCondVar_Signal( & thd->condv );
	if( thd->cleaner ) (*(thd->cleaner))( thd->taskArg );
	thd->myThread = NULL; /* it will be closed by _endthread() */
	_endthread();
}

DThreadData* DThread_GetSpecific()
{
	return (DThreadData*) TlsGetValue( thdSpecKey );
}
void DaoInitThreadSys()
{
	/* DThread object for the main thread, used for join() */
	DThread *mainThread = (DThread*)dao_calloc( 1, sizeof(DThread) );
	thdSpecKey = (dao_thdspec_t)TlsAlloc();
	DThread_Init( mainThread );

	mainThread->thdSpecData = (DThreadData*)GlobalAlloc( GPTR, sizeof(DThreadData) );  
	mainThread->thdSpecData->thdObject = mainThread;
	mainThread->thdSpecData->state = 0;

	TlsSetValue( thdSpecKey, mainThread->thdSpecData );
}

#endif

/* Dao threading types: */

static int DaoMT_PushSectionFrame( DaoProcess *proc )
{
	if( DaoProcess_PushSectionFrame( proc ) == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "code section not found!" );
		return 0;
	}
	return 1;
}

static void DaoMutex_Lib_Lock( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoMutex *self = (DaoMutex*) par[0];
	DaoMutex_Lock( self );
}
static void DaoMutex_Lib_Unlock( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoMutex *self = (DaoMutex*) par[0];
	DaoMutex_Unlock( self );
}
static void DaoMutex_Lib_TryLock( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoMutex *self = (DaoMutex*) par[0];
	DaoProcess_PutInteger( proc, DaoMutex_TryLock( self ) );
}
static void DaoMutex_Lib_Protect( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMutex *self = (DaoMutex*) p[0];
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	if( sect == NULL || DaoMT_PushSectionFrame( proc ) == 0 ) return;
	DaoMutex_Lock( self );
	DaoProcess_Execute( proc );
	DaoMutex_Unlock( self );
	DaoProcess_PopFrame( proc );
}
static DaoFuncItem mutexMeths[] =
{
	{ DaoMutex_Lib_Lock,      "lock( self : mutex )" }, // XXX remove???
	{ DaoMutex_Lib_Unlock,    "unlock( self : mutex )" },
	{ DaoMutex_Lib_TryLock,   "trylock( self : mutex )=>int" },
	{ DaoMutex_Lib_Protect,   "protect( self : mutex )[]" },
	// ??? TODO: protect( self : mutex, try=0 )[locked:int]
	{ NULL, NULL }
};
static void DaoMutex_Delete( DaoMutex *self )
{
	DMutex_Destroy( & self->myMutex );
	dao_free( self );
}

static DaoTypeCore mutexCore =
{
	NULL,
	DaoValue_SafeGetField,
	DaoValue_SafeSetField,
	DaoValue_GetItem,
	DaoValue_SetItem,
	DaoValue_Print,
	DaoValue_NoCopy,
};
DaoTypeBase mutexTyper =
{
	"mutex", & mutexCore, NULL, (DaoFuncItem*) mutexMeths, {0}, {0},
	(FuncPtrDel) DaoMutex_Delete, NULL
};

DaoMutex* DaoMutex_New()
{
	DaoMutex* self = (DaoMutex*) dao_calloc( 1, sizeof(DaoMutex) );
	DaoValue_Init( self, DAO_MUTEX );
	DMutex_Init( & self->myMutex );
	return self;
}
void DaoMutex_Lock( DaoMutex *self )
{
	DMutex_Lock( & self->myMutex );
}
void DaoMutex_Unlock( DaoMutex *self )
{
	DMutex_Unlock( & self->myMutex );
}
int DaoMutex_TryLock( DaoMutex *self )
{
	return DMutex_TryLock( & self->myMutex );
}
/* Condition variable */
static void DaoCondV_Lib_Wait( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoCondVar *self = (DaoCondVar*) par[0];
	DaoMutex *mutex = (DaoMutex*) par[1];
	if( mutex->type != DAO_MUTEX ){
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "need mutex" );
		return;
	}
	DCondVar_Wait( & self->myCondVar, & mutex->myMutex );
}
static void DaoCondV_Lib_TimedWait( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoCondVar *self = (DaoCondVar*) par[0];
	DaoMutex *mutex = (DaoMutex*) par[1];
	if( mutex->type != DAO_MUTEX ){
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "need mutex" );
		return;
	}
	DaoProcess_PutInteger( proc, 
			DCondVar_TimedWait( & self->myCondVar, & mutex->myMutex, par[2]->xFloat.value ) );
}
static void DaoCondV_Lib_Signal( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoCondVar *self = (DaoCondVar*) par[0];
	DCondVar_Signal( & self->myCondVar );
}
static void DaoCondV_Lib_BroadCast( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoCondVar *self = (DaoCondVar*) par[0];
	DCondVar_BroadCast( & self->myCondVar );
}
static DaoFuncItem condvMeths[] =
{
	{ DaoCondV_Lib_Wait,      "wait( self : condition, mtx : mutex )" },
	{ DaoCondV_Lib_TimedWait, "timedwait( self : condition, mtx : mutex, seconds :float )=>int" },
	{ DaoCondV_Lib_Signal,    "signal( self : condition )" },
	{ DaoCondV_Lib_BroadCast, "broadcast( self : condition )" },
	{ NULL, NULL }
};

static DaoTypeCore condvCore =
{
	NULL,
	DaoValue_SafeGetField,
	DaoValue_SafeSetField,
	DaoValue_GetItem,
	DaoValue_SetItem,
	DaoValue_Print,
	DaoValue_NoCopy,
};
DaoTypeBase condvTyper =
{
	"condition", & condvCore, NULL, (DaoFuncItem*) condvMeths, {0}, {0},
	(FuncPtrDel) DaoCondVar_Delete, NULL
};
DaoCondVar* DaoCondVar_New()
{
	DaoCondVar* self = (DaoCondVar*) dao_calloc( 1, sizeof(DaoCondVar) );
	DaoValue_Init( self, DAO_CONDVAR );
	DCondVar_Init( & self->myCondVar );
	return self;
}
void DaoCondVar_Delete( DaoCondVar *self )
{
	DCondVar_Destroy( & self->myCondVar );
	dao_free( self );
}

void DaoCondVar_Wait( DaoCondVar *self, DaoMutex *mutex )
{
	DCondVar_Wait( & self->myCondVar, & mutex->myMutex );
}
int  DaoCondVar_TimedWait( DaoCondVar *self, DaoMutex *mutex, double seconds )
{
	return DCondVar_TimedWait( & self->myCondVar, & mutex->myMutex, seconds );
}

void DaoCondVar_Signal( DaoCondVar *self )
{
	DCondVar_Signal( & self->myCondVar );
}
void DaoCondVar_BroadCast( DaoCondVar *self )
{
	DCondVar_BroadCast( & self->myCondVar );
}
/* Semaphore */
static void DaoSema_Lib_Wait( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoSema *self = (DaoSema*) par[0];
	DSema_Wait( & self->mySema );
}
static void DaoSema_Lib_Post( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoSema *self = (DaoSema*) par[0];
	DSema_Post( & self->mySema );
}
static void DaoSema_Lib_SetValue( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoSema *self = (DaoSema*) par[0];
	DSema_SetValue( & self->mySema, par[1]->xInteger.value );
}
static void DaoSema_Lib_GetValue( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoSema *self = (DaoSema*) par[0];
	DaoProcess_PutInteger( proc, DSema_GetValue( & self->mySema ) );
}
static void DaoSema_Lib_Protect( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoSema *self = (DaoSema*) p[0];
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	if( sect == NULL || DaoMT_PushSectionFrame( proc ) == 0 ) return;
	DSema_Wait( & self->mySema );
	DaoProcess_Execute( proc );
	DSema_Post( & self->mySema );
	DaoProcess_PopFrame( proc );
}
static DaoFuncItem semaMeths[] =
{
	{ DaoSema_Lib_Wait,      "wait( self : semaphore )" },
	{ DaoSema_Lib_Post,      "post( self : semaphore )" },
	{ DaoSema_Lib_SetValue,  "setvalue( self : semaphore, n :int )" },
	{ DaoSema_Lib_GetValue,  "getvalue( self : semaphore )=>int" },
	{ DaoSema_Lib_Protect,   "protect( self : semaphore )[]" },
	{ NULL, NULL }
};
static DaoTypeCore semaCore =
{
	NULL,
	DaoValue_SafeGetField,
	DaoValue_SafeSetField,
	DaoValue_GetItem,
	DaoValue_SetItem,
	DaoValue_Print,
	DaoValue_NoCopy,
};
DaoTypeBase semaTyper =
{
	"semaphore", & semaCore, NULL, (DaoFuncItem*) semaMeths, {0}, {0},
	(FuncPtrDel) DaoSema_Delete, NULL
};
DaoSema* DaoSema_New( int n )
{
	DaoSema* self = (DaoSema*) dao_calloc( 1, sizeof(DaoSema) );
	DaoValue_Init( self, DAO_SEMA );
	DSema_Init( & self->mySema, ( n < 0 )? 0 : n );
	return self;
}
void DaoSema_Delete( DaoSema *self )
{
	DSema_Destroy( & self->mySema );
	dao_free( self );
}

void DaoSema_Wait( DaoSema *self )
{
	DSema_Wait( & self->mySema );
}
void DaoSema_Post( DaoSema *self )
{
	DSema_Post( & self->mySema );
}

void DaoSema_SetValue( DaoSema *self, int n )
{
	DSema_SetValue( & self->mySema, ( n < 0 )? 0 : n );
}
int  DaoSema_GetValue( DaoSema *self )
{
	return DSema_GetValue( & self->mySema );
}

/* thread master */
static void DaoThdMaster_Lib_Mutex( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoMutex *mutex = DaoMutex_New();
	DaoProcess_PutValue( proc, (DaoValue*) mutex );
}
static void DaoThdMaster_Lib_CondVar( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoProcess_PutValue( proc, (DaoValue*)DaoCondVar_New() );
}
static void DaoThdMaster_Lib_Sema( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoProcess_PutValue( proc, (DaoValue*)DaoSema_New( par[0]->xInteger.value ) );
}

typedef struct DaoTaskData DaoTaskData;
struct DaoTaskData
{
	DaoValue    *param; // parameter container: list, map or array;
	DaoValue    *result; // result container: list or array;
	DaoProcess  *proto; // caller's process;
	DaoProcess  *clone; // spawned process;
	DaoVmCode   *sect; // DVM_SECT

	uint_t   funct; // type of functional;
	uint_t   entry; // entry code;
	uint_t   first; // first index;
	uint_t   step; // index step;
	uint_t   status; // execution status;
	uint_t  *joined; // number of joined threads;
	uint_t  *index; // smallest index found by all threads;
	DNode  **node; // smallest key found by all threads;
};

void DaoProcess_ReturnFutureValue( DaoProcess *self, DaoFuture *future )
{
	DaoType *type;
	if( future == NULL ) return;
	type = future->unitype;
	type = type && type->nested->size ? type->nested->items.pType[0] : NULL;
	switch( self->status ){
	case DAO_VMPROC_FINISHED :
	case DAO_VMPROC_ABORTED :
		DaoValue_Move( self->stackValues[0], & future->value, type );
		future->state = DAO_CALL_FINISHED;
		break;
	case DAO_VMPROC_SUSPENDED : future->state = DAO_CALL_PAUSED; break;
	case DAO_VMPROC_RUNNING :
	case DAO_VMPROC_STACKED : future->state = DAO_CALL_RUNNING; break;
	default : break;
	}
}
static void DaoMT_InitProcess( DaoProcess *proto, DaoProcess *clone )
{
	DaoProcess_PushRoutine( clone, proto->activeRoutine, proto->activeObject );
	clone->activeCode = proto->activeCode;
	DaoProcess_PushFunction( clone, proto->topFrame->function );
	DaoProcess_SetActiveFrame( clone, clone->topFrame );
	DaoProcess_PushSectionFrame( clone );
	clone->topFrame->outer = proto;
	clone->topFrame->returning = -1;
}
static void DaoMT_RunIterateFunctional( void *p )
{
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger tidint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *index = (DaoValue*)(void*)&idint;
	DaoValue *threadid = (DaoValue*)(void*)&tidint;
	DaoTaskData *self = (DaoTaskData*)p;
	DaoProcess *clone = self->clone;
	DaoVmCode *sect = self->sect;
	dint i, n = self->param->xInteger.value;

	DaoMT_InitProcess( self->proto, clone );
	tidint.value = self->first;
	for(i=self->first; i<n; i+=self->step){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( clone, sect->a, index );
		if( sect->b >1 ) DaoProcess_SetValue( clone, sect->a+1, threadid );
		clone->topFrame->entry = self->entry;
		DaoProcess_Execute( clone );
		if( clone->status == DAO_VMPROC_SUSPENDED ){
			DMutex_Lock( clone->mutex );
			while( clone->status != DAO_VMPROC_FINISHED && clone->status != DAO_VMPROC_ABORTED )
				DCondVar_TimedWait( clone->condv, clone->mutex, 0.01 );
			DMutex_Unlock( clone->mutex );
		}
		if( clone->status != DAO_VMPROC_FINISHED ) break;
	}
}
static void DaoMT_RunListFunctional( void *p )
{
	DaoValue *res;
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger tidint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *index = (DaoValue*)(void*)&idint;
	DaoValue *threadid = (DaoValue*)(void*)&tidint;
	DaoTaskData *self = (DaoTaskData*)p;
	DaoList *list = (DaoList*) self->param;
	DaoList *list2 = (DaoList*) self->result;
	DaoProcess *clone = self->clone;
	DaoVmCode *sect = self->sect;
	DaoValue **items = list->items.items.pValue;
	size_t i, n = list->items.size;

	DaoMT_InitProcess( self->proto, clone );
	tidint.value = self->first;
	for(i=self->first; i<n; i+=self->step){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( clone, sect->a, items[i] );
		if( sect->b >1 ) DaoProcess_SetValue( clone, sect->a+1, index );
		if( sect->b >2 ) DaoProcess_SetValue( clone, sect->a+2, threadid );
		clone->topFrame->entry = self->entry;
		DaoProcess_Execute( clone );
		if( clone->status == DAO_VMPROC_SUSPENDED ){
			DMutex_Lock( clone->mutex );
			while( clone->status != DAO_VMPROC_FINISHED && clone->status != DAO_VMPROC_ABORTED )
				DCondVar_TimedWait( clone->condv, clone->mutex, 0.01 );
			DMutex_Unlock( clone->mutex );
		}
		if( clone->status != DAO_VMPROC_FINISHED ) break;
		res = clone->stackValues[0];
		if( self->funct == DVM_FUNCT_MAP ){
			self->status |= DaoList_SetItem( list2, res, i );
		}else if( self->funct == DVM_FUNCT_APPLY ){
			self->status |= DaoList_SetItem( list, res, i );
		}else if( self->funct == DVM_FUNCT_FIND ){
			if( *self->index < i ) break;
			if( res->xInteger.value ){
				DMutex_Lock( clone->mutex );
				if( i < *self->index ) *self->index = i;
				DMutex_Unlock( clone->mutex );
				break;
			}
		}
	}
}
static void DaoMT_RunMapFunctional( void *p )
{
	DaoValue *res;
	DaoInteger tidint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *threadid = (DaoValue*)(void*)&tidint;
	DaoTaskData *self = (DaoTaskData*)p;
	DaoMap *map = (DaoMap*) self->param;
	DaoList *list2 = (DaoList*) self->result;
	DaoProcess *clone = self->clone;
	DaoVmCode *sect = self->sect;
	DaoType *type = map->unitype;
	DNode *node = NULL;
	size_t i = 0;

	DaoMT_InitProcess( self->proto, clone );
	tidint.value = self->first;
	type = type && type->nested->size > 1 ? type->nested->items.pType[1] : NULL;
	for(node=DMap_First( map->items ); node; node=DMap_Next(map->items, node) ){
		if( (i++) % self->step != self->first ) continue;
		if( sect->b >0 ) DaoProcess_SetValue( clone, sect->a, node->key.pValue );
		if( sect->b >1 ) DaoProcess_SetValue( clone, sect->a+1, node->value.pValue );
		if( sect->b >2 ) DaoProcess_SetValue( clone, sect->a+2, threadid );
		clone->topFrame->entry = self->entry;
		DaoProcess_Execute( clone );
		if( clone->status == DAO_VMPROC_SUSPENDED ){
			DMutex_Lock( clone->mutex );
			while( clone->status != DAO_VMPROC_FINISHED && clone->status != DAO_VMPROC_ABORTED )
				DCondVar_TimedWait( clone->condv, clone->mutex, 0.01 );
			DMutex_Unlock( clone->mutex );
		}
		if( clone->status != DAO_VMPROC_FINISHED ) break;
		res = clone->stackValues[0];
		if( self->funct == DVM_FUNCT_MAP ){
			self->status |= DaoList_SetItem( list2, res, i-1 );
		}else if( self->funct == DVM_FUNCT_APPLY ){
			self->status |= DaoValue_Move( res, & node->value.pValue, type ) == 0;
		}else if( self->funct == DVM_FUNCT_FIND ){
			DNode **p = self->node;
			if( *p && DaoValue_Compare( (*p)->key.pValue, node->key.pValue ) < 0 ) break;
			if( res->xInteger.value ){
				DMutex_Lock( clone->mutex );
				if( *p == NULL || DaoValue_Compare( (*p)->key.pValue, node->key.pValue ) >0 ) *p = node;
				DMutex_Unlock( clone->mutex );
				break;
			}
		}
	}
}

void DaoArray_GetSliceShape( DaoArray *self, size_t **dims, short *ndim );
int DaoArray_SliceSize( DaoArray *self );
int DaoArray_IndexFromSlice( DaoArray *self, DArray *slice, int sid );
DaoValue* DaoArray_GetValue( DaoArray *self, int i, DaoValue *res );
void DaoArray_SetValue( DaoArray *self, int i, DaoValue *value );

static void DaoMT_RunArrayFunctional( void *p )
{
	DaoValue **idval;
	DaoValue *elem, *res = NULL;
	DaoInteger tidint = {DAO_INTEGER,0,0,0,0,0};
	DaoComplex com = {DAO_COMPLEX,0,0,0,1,{0.0,0.0}};
	DaoValue *threadid = (DaoValue*)(void*)&tidint;
	DaoTaskData *self = (DaoTaskData*)p;
	DaoProcess *clone = self->clone;
	DaoVmCode *sect = self->sect;
	DaoArray *param = (DaoArray*) self->param;
	DaoArray *result = (DaoArray*) self->result;
	DaoArray *original = param->original;
	DaoArray *array = original ? original : param;
	DArray *slices = param->slices;
	size_t *dims = array->dims;
	size_t i, id, id2, n = DaoArray_SliceSize( param );
	int j, D = array->ndim;
	int isvec = (D == 2 && (dims[0] ==1 || dims[1] == 1));
	int stackBase, vdim = sect->b - 1;

	DaoMT_InitProcess( self->proto, clone );
	tidint.value = self->first;

	stackBase = clone->topFrame->active->stackBase;
	idval = clone->activeValues + sect->a + 1;
	for(j=0; j<vdim; j++) idval[j]->xInteger.value = 0;
	for(i=self->first; i<n; i+=self->step){
		idval = clone->stackValues + stackBase + sect->a + 1;
		id = id2 = (original ? DaoArray_IndexFromSlice( original, slices, i ) : i);
		if( isvec ){
			if( vdim >0 ) idval[0]->xInteger.value = id2;
			if( vdim >1 ) idval[1]->xInteger.value = id2;
		}else{
			for( j=D-1; j>=0; j--){
				int k = id2 % dims[j];
				id2 /= dims[j];
				if( j < vdim ) idval[j]->xInteger.value = k;
			}
		}
		elem = clone->stackValues[ stackBase + sect->a ];
		if( elem == NULL || elem->type != array->etype ){
			elem = (DaoValue*)(void*) &com;
			elem->type = array->etype;
			elem = DaoProcess_SetValue( clone, sect->a, elem );
		}
		DaoArray_GetValue( array, id, elem );
		if( sect->b > 6 ) DaoProcess_SetValue( clone, sect->a+6, threadid );
		clone->topFrame->entry = self->entry;
		DaoProcess_Execute( clone );
		if( clone->status == DAO_VMPROC_SUSPENDED ){
			DMutex_Lock( clone->mutex );
			while( clone->status != DAO_VMPROC_FINISHED && clone->status != DAO_VMPROC_ABORTED )
				DCondVar_TimedWait( clone->condv, clone->mutex, 0.01 );
			DMutex_Unlock( clone->mutex );
		}
		if( clone->status != DAO_VMPROC_FINISHED ) break;
		res = clone->stackValues[0];
		if( self->funct == DVM_FUNCT_MAP ){
			DaoArray_SetValue( result, i, res );
		}else if( self->funct == DVM_FUNCT_APPLY ){
			DaoArray_SetValue( array, id, res );
		}
	}
}
static void DaoMT_RunFunctional( void *p )
{
	DaoTaskData *self = (DaoTaskData*)p;
	DaoProcess *clone = self->clone;
	switch( self->param->type ){
	case DAO_INTEGER : DaoMT_RunIterateFunctional( p ); break;
	case DAO_LIST  : DaoMT_RunListFunctional( p ); break;
	case DAO_MAP   : DaoMT_RunMapFunctional( p ); break;
	case DAO_ARRAY : DaoMT_RunArrayFunctional( p ); break;
	}
	self->status |= clone->status != DAO_VMPROC_FINISHED;
	DMutex_Lock( clone->mutex );
	*self->joined += 1;
	if( clone->exceptions->size ) DaoProcess_PrintException( clone, 1 );
	DCondVar_Signal( clone->condv );
	DMutex_Unlock( clone->mutex );
}
static void DaoMT_Functional( DaoProcess *proc, DaoValue *P[], int N, int F )
{
	DMutex mutex;
	DCondVar condv;
	DaoTaskData *tasks;
	DaoValue *param = P[0];
	DaoValue *result = NULL;
	DaoList *list = NULL;
	DaoArray *array = NULL;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	int i, entry, threads = P[1]->xInteger.value;
	uint_t index = -1, status = 0, joined = 0;
	DNode *node = NULL;

	switch( F ){
	case DVM_FUNCT_MAP :
		if( param->type == DAO_ARRAY ){
			array = DaoProcess_PutArray( proc );
			result = (DaoValue*) array;
		}else{
			list = DaoProcess_PutList( proc );
			result = (DaoValue*) list;
		}
		break;
	case DVM_FUNCT_APPLY : DaoProcess_PutValue( proc, param ); break;
	case DVM_FUNCT_FIND : DaoProcess_PutValue( proc, dao_none_value ); break;
	}
	if( threads <= 0 ) threads = 2;
	if( sect == NULL || DaoMT_PushSectionFrame( proc ) == 0 ) return;
	if( list ){
		DArray_Clear( & list->items );
		if( param->type == DAO_LIST ) DArray_Resize( & list->items, param->xList.items.size, NULL );
		if( param->type == DAO_MAP ) DArray_Resize( & list->items, param->xMap.items->size, NULL );
	}else if( array && F == DVM_FUNCT_MAP ){
		DaoArray_GetSliceShape( (DaoArray*) param, & array->dims, & array->ndim );
		DaoArray_ResizeArray( array, array->dims, array->ndim );
	}

	DMutex_Init( & mutex );
	DCondVar_Init( & condv );
	entry = proc->topFrame->entry;
	tasks = (DaoTaskData*) dao_calloc( threads, sizeof(DaoTaskData) );
	DaoProcess_PopFrame( proc );
	for(i=0; i<threads; i++){
		DaoTaskData *task = tasks + i;
		task->param = param;
		task->result = result;
		task->proto = proc;
		task->sect = sect;
		task->funct = F;
		task->entry = entry;
		task->first = i;
		task->step = threads;
		task->index = & index;
		task->node = & node;
		task->joined = & joined;
		task->clone = DaoVmSpace_AcquireProcess( proc->vmSpace );
		task->clone->condv = & condv;
		task->clone->mutex = & mutex;
		if( i ) DaoCallServer_AddTask( DaoMT_RunFunctional, task );
	}
	DaoMT_RunFunctional( tasks );

	DMutex_Lock( & mutex );
	while( joined < threads ) DCondVar_TimedWait( & condv, & mutex, 0.01 );
	DMutex_Unlock( & mutex );

	for(i=0; i<threads; i++){
		DaoTaskData *task = tasks + i;
		DaoVmSpace_ReleaseProcess( proc->vmSpace, task->clone );
		status |= task->status;
	}
	if( F == DVM_FUNCT_FIND ){
		DaoTuple *tuple = DaoProcess_PutTuple( proc );
		if( param->type == DAO_LIST && index != -1 ){
			DaoValue **items = param->xList.items.items.pValue;
			GC_ShiftRC( items[index], tuple->items[1] );
			tuple->items[1] = items[index];
			tuple->items[0]->xInteger.value = index;
		}else if( param->type == DAO_MAP && node ){
			GC_ShiftRC( node->key.pValue, tuple->items[0] );
			GC_ShiftRC( node->value.pValue, tuple->items[1] );
			tuple->items[0] = node->key.pValue;
			tuple->items[1] = node->value.pValue;
		}
	}
	if( status ) DaoProcess_RaiseException( proc, DAO_ERROR, "code section execution failed!" );
	DMutex_Destroy( & mutex );
	DCondVar_Destroy( & condv );
	dao_free( tasks );
}
static void DaoMT_Start0( void *p )
{
	DaoProcess *proc = (DaoProcess*)p;
	DaoProcess_Execute( proc );
	DaoProcess_ReturnFutureValue( proc, proc->future );
	if( proc->future->state == DAO_CALL_FINISHED ){
		DaoVmSpace_ReleaseProcess( proc->vmSpace, proc );
	}
}
static void DaoMT_Start( DaoProcess *proc, DaoValue *p[], int n )
{
	int entry;
	DaoProcess *clone;
	DaoVmCode *vmc, *end;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoFuture *future = DaoFuture_New();
	DaoType *type = proc->activeTypes[proc->activeCode->c]->nested->items.pType[0];

	type = DaoNamespace_MakeType( proc->activeNamespace, "future", DAO_FUTURE, NULL, &type, 1 );
	GC_ShiftRC( type, future->unitype );
	future->unitype = type;
	DaoProcess_PutValue( proc, (DaoValue*) future );
	if( sect == NULL || DaoMT_PushSectionFrame( proc ) == 0 ) return;

	entry = proc->topFrame->entry;
	end = proc->activeRoutine->vmCodes->codes + proc->activeCode[1].b;
	clone = DaoVmSpace_AcquireProcess( proc->vmSpace );
	DaoProcess_PopFrame( proc );
	DaoProcess_SetActiveFrame( proc, proc->topFrame );
	DaoMT_InitProcess( proc, clone );
	clone->topFrame->entry = entry;
	clone->topFrame->outer = clone;
	future->process = clone;
	GC_IncRC( clone );
	GC_ShiftRC( future, clone->future );
	clone->future = future;
	future->state = DAO_CALL_RUNNING;
	for(vmc=sect; vmc!=end; vmc++){
		int i = -1, code = vmc->code;
		if( code == DVM_GETVH || (code >= DVM_GETVH_I && code <= DVM_GETVH_D) ){
			if( vmc->a == 0 ) i = vmc->b;
		}else if( code == DVM_SETVH || (code >= DVM_SETVH_II && code <= DVM_SETVH_DD) ){
			if( vmc->c == 0 ) i = vmc->b;
		}
		if( i >= 0 ){
			GC_ShiftRC( proc->activeValues[i], clone->activeValues[i] );
			clone->activeValues[i] = proc->activeValues[i];
		}
	}
	DaoCallServer_AddTask( DaoMT_Start0, clone );
}
static void DaoMT_Iterate( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_NULL );
}
static void DaoMT_ListIterate( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_ITERATE );
}
static void DaoMT_ListMap( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_MAP );
}
static void DaoMT_ListApply( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_APPLY );
}
static void DaoMT_ListFind( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_FIND );
}
static void DaoMT_MapIterate( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_ITERATE );
}
static void DaoMT_MapMap( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_MAP );
}
static void DaoMT_MapApply( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_APPLY );
}
static void DaoMT_MapFind( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_FIND );
}
static void DaoMT_ArrayIterate( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_ITERATE );
}
static void DaoMT_ArrayMap( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_MAP );
}
static void DaoMT_ArrayApply( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_APPLY );
}
static void DaoMT_Critical( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	if( sect == NULL || DaoMT_PushSectionFrame( proc ) == 0 ) return;
	if( proc->mutex ) DMutex_Lock( proc->mutex );
	DaoProcess_Execute( proc );
	if( proc->mutex ) DMutex_Unlock( proc->mutex );
	DaoProcess_PopFrame( proc );
}

static DaoFuncItem thdMasterMeths[] =
{
	{ DaoThdMaster_Lib_Mutex,       "mutex()=>mutex" },
	{ DaoThdMaster_Lib_CondVar,     "condition()=>condition" },
	{ DaoThdMaster_Lib_Sema,        "semaphore( value = 0 )=>semaphore" },

	{ DaoMT_Critical, "critical()[]" },
	{ DaoMT_Start, "start()[=>@V] =>future<@V>" },
	{ DaoMT_Iterate, "iterate( times :int, threads=2 )[index:int,threadid:int]" },

	{ DaoMT_ListIterate, "iterate( alist :list<@T>, threads=2 )[item:@T,index:int,threadid:int]" },
	{ DaoMT_ListMap, "map( alist :list<@T>, threads=2 )[item:@T,index:int,threadid:int =>@T2] =>list<@T2>" },
	{ DaoMT_ListApply, "apply( alist :list<@T>, threads=2 )[item:@T,index:int,threadid:int =>@T] =>list<@T>" },
	{ DaoMT_ListFind, "find( alist :list<@T>, threads=2 )[item:@T,index:int,threadid:int =>int] =>tuple<index:int,item:@T>|none" },

	{ DaoMT_MapIterate, "iterate( amap :map<@K,@V>, threads=2 )[key:@K,value:@V,threadid:int]" },
	{ DaoMT_MapMap, "map( amap :map<@K,@V>, threads=2 )[key:@K,value:@V,threadid:int =>@T] =>list<@T>" },
	{ DaoMT_MapApply, "apply( amap :map<@K,@V>, threads=2 )[key:@K,value:@V,threadid:int =>@V] =>map<@K,@V>" },
	{ DaoMT_MapFind, "find( amap :map<@K,@V>, threads=2 )[key:@K,value:@V,threadid:int =>int] =>tuple<key:@K,value:@V>|none" },

	{ DaoMT_ArrayIterate, "iterate( aarray :array<@T>, threads=2 )[item:@T,I:int,J:int,K:int,L:int,M:int,threadid:int]" },
	{ DaoMT_ArrayMap, "map( aarray :array<@T>, threads=2 )[item:@T,I:int,J:int,K:int,L:int,M:int,threadid:int =>@T2] =>array<@T2>" },
	{ DaoMT_ArrayApply, "apply( aarray :array<@T>, threads=2 )[item:@T,I:int,J:int,K:int,L:int,M:int,threadid:int =>@T] =>array<@T>" },
	{ NULL, NULL }
};

DaoTypeBase thdMasterTyper =
{
	"mt", NULL, NULL, (DaoFuncItem*) thdMasterMeths, {0}, {0}, NULL, NULL
};


void DaoInitThread()
{
	DaoInitThreadSys();
}
#endif

