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

#ifdef DAO_WITH_THREAD
/* Basic threading interfaces */

static void DSema_SetValue( DSema *self, int n );
static int  DSema_GetValue( DSema *self );

static void DThread_Detach( DThread *self );
static void DThread_Cancel( DThread *self );
static void DThread_TestCancel( DThread *self );

static dao_thdspec_t thdSpecKey = 0;

#ifdef UNIX

#include <sys/time.h>
#include <signal.h>

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
		self->thdSpecData = (DThreadData*)dao_malloc( sizeof(DThreadData) );
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
	/* XXX: version */
#ifdef _MSC_VER
	return TryEnterCriticalSection( & self->myMutex );
#else
	return 1;
#endif
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
	CloseHandle( self->myThread );
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
	if( thd->cleaner ) (*(thd->cleaner))( thd->taskArg );
	thd->running = 0;
	DCondVar_Signal( & thd->condv );
	_endthread();
}

DThreadData* DThread_GetSpecific()
{
	return (DThreadData*) TlsGetValue( thdSpecKey );
}
void DaoInitThreadSys()
{
	/* DThread object for the main thread, used for join() */
	DThread *mainThread = (DThread*)dao_malloc( sizeof(DThread) );
	thdSpecKey = (dao_thdspec_t)TlsAlloc();
	DThread_Init( mainThread );

	mainThread->thdSpecData = (DThreadData*)GlobalAlloc( GPTR, sizeof(DThreadData) );  
	mainThread->thdSpecData->thdObject = mainThread;
	mainThread->thdSpecData->state = 0;

	TlsSetValue( thdSpecKey, mainThread->thdSpecData );
}

#endif

/* Dao threading types: */

static DaoThread* DaoThdMaster_FindRecord( DaoThdMaster *self, dao_thread_t tid );

static DaoThread* GetThisThread( DaoThdMaster *thdMaster )
{
	if( thdMaster ){
		dao_thread_t tid = DThread_Self();
		return DaoThdMaster_FindRecord( thdMaster, tid );
	}
	return NULL;
}
static void DaoMutex_Lib_Lock( DaoContext *ctx, DValue *par[], int N )
{
	DaoMutex *self = (DaoMutex*) par[0]->v.p;
	DaoMutex_Lock( self );
}
static void DaoMutex_Lib_Unlock( DaoContext *ctx, DValue *par[], int N )
{
	DaoMutex *self = (DaoMutex*) par[0]->v.p;
	DaoMutex_Unlock( self );
}
static void DaoMutex_Lib_TryLock( DaoContext *ctx, DValue *par[], int N )
{
	DaoMutex *self = (DaoMutex*) par[0]->v.p;
	DaoContext_PutInteger( ctx, DaoMutex_TryLock( self ) );
}
static DaoFuncItem mutexMeths[] =
{
	{ DaoMutex_Lib_Lock,      "lock( self : mutex )" },
	{ DaoMutex_Lib_Unlock,    "unlock( self : mutex )" },
	{ DaoMutex_Lib_TryLock,   "trylock( self : mutex )=>int" },
	{ NULL, NULL }
};
static void DaoMutex_Delete( DaoMutex *self )
{
	DMutex_Destroy( & self->myMutex );
}

static DaoTypeCore mutexCore =
{
	0, NULL, NULL, NULL, NULL,
	DaoBase_SafeGetField,
	DaoBase_SafeSetField,
	DaoBase_GetItem,
	DaoBase_SetItem,
	DaoBase_Print,
	DaoBase_Copy,
};
DaoTypeBase mutexTyper =
{
	"mutex", & mutexCore, NULL, (DaoFuncItem*) mutexMeths, {0},
	(FuncPtrDel) DaoMutex_Delete, NULL
};

DaoMutex* DaoMutex_New( DaoVmSpace *vms )
{
	DaoMutex* self = (DaoMutex*) dao_malloc( sizeof(DaoMutex) );
	DaoBase_Init( self, DAO_MUTEX );
	DMutex_Init( & self->myMutex );
	self->thdMaster = NULL;
	if( vms ) self->thdMaster = vms->thdMaster;
	return self;
}
void DaoMutex_Lock( DaoMutex *self )
{
	DaoThread *thread = GetThisThread( self->thdMaster );
	if( thread ){
		DNode *node = DMap_Find( thread->mutexUsed, self );
		if( node == NULL ){
			GC_IncRC( self );
			DMap_Insert( thread->mutexUsed, self, (void*)1 );
		}
	}
	DMutex_Lock( & self->myMutex );
}
void DaoMutex_Unlock( DaoMutex *self )
{
	DaoThread *thread = GetThisThread( self->thdMaster );
	DMutex_Unlock( & self->myMutex );
	if( thread ){
		DNode *node = DMap_Find( thread->mutexUsed, self );
		if( node != NULL ){
			GC_IncRC( self );
			DMap_Erase( thread->mutexUsed, self );
		}
	}
}
int DaoMutex_TryLock( DaoMutex *self )
{
	DaoThread *thread = GetThisThread( self->thdMaster );
	int locked = DMutex_TryLock( & self->myMutex );
	if( thread && locked ){
		DNode *node = DMap_Find( thread->mutexUsed, self );
		if( node != NULL ){
			GC_IncRC( self );
			DMap_Erase( thread->mutexUsed, self );
		}
	}
	return locked;
}
/* Condition variable */
static void DaoCondV_Lib_Wait( DaoContext *ctx, DValue *par[], int N )
{
	DaoCondVar *self = (DaoCondVar*) par[0]->v.p;
	DaoMutex *mutex = (DaoMutex*) par[1]->v.p;
	if( mutex->type != DAO_MUTEX ){
		DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "need mutex" );
		return;
	}
	DCondVar_Wait( & self->myCondVar, & mutex->myMutex );
}
static void DaoCondV_Lib_TimedWait( DaoContext *ctx, DValue *par[], int N )
{
	DaoCondVar *self = (DaoCondVar*) par[0]->v.p;
	DaoMutex *mutex = (DaoMutex*) par[1]->v.p;
	if( mutex->type != DAO_MUTEX ){
		DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "need mutex" );
		return;
	}
	DaoContext_PutInteger( ctx, 
			DCondVar_TimedWait( & self->myCondVar, & mutex->myMutex, par[2]->v.i ) );
}
static void DaoCondV_Lib_Signal( DaoContext *ctx, DValue *par[], int N )
{
	DaoCondVar *self = (DaoCondVar*) par[0]->v.p;
	DCondVar_Signal( & self->myCondVar );
}
static void DaoCondV_Lib_BroadCast( DaoContext *ctx, DValue *par[], int N )
{
	DaoCondVar *self = (DaoCondVar*) par[0]->v.p;
	DCondVar_BroadCast( & self->myCondVar );
}
static DaoFuncItem condvMeths[] =
{
	{ DaoCondV_Lib_Wait,     "wait( self : condition, mtx : mutex )" },
	{ DaoCondV_Lib_TimedWait,"timedwait( self : condition, mtx : mutex, seconds :float )=>int" },
	{ DaoCondV_Lib_Signal,    "signal( self : condition )" },
	{ DaoCondV_Lib_BroadCast, "broadcast( self : condition )" },
	{ NULL, NULL }
};

static DaoTypeCore condvCore =
{
	0, NULL, NULL, NULL, NULL,
	DaoBase_SafeGetField,
	DaoBase_SafeSetField,
	DaoBase_GetItem,
	DaoBase_SetItem,
	DaoBase_Print,
	DaoBase_Copy,
};
DaoTypeBase condvTyper =
{
	"condition", & condvCore, NULL, (DaoFuncItem*) condvMeths, {0},
	(FuncPtrDel) DaoCondVar_Delete, NULL
};
DaoCondVar* DaoCondVar_New( DaoThdMaster *thdm )
{
	DaoCondVar* self = (DaoCondVar*) dao_malloc( sizeof(DaoCondVar) );
	DaoBase_Init( self, DAO_CONDVAR );
	DCondVar_Init( & self->myCondVar );
	self->thdMaster = thdm;
	return self;
}
void DaoCondVar_Delete( DaoCondVar *self )
{
	DCondVar_Destroy( & self->myCondVar );
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
static void DaoSema_Lib_Wait( DaoContext *ctx, DValue *par[], int N )
{
	DaoSema *self = (DaoSema*) par[0]->v.p;
	DSema_Wait( & self->mySema );
}
static void DaoSema_Lib_Post( DaoContext *ctx, DValue *par[], int N )
{
	DaoSema *self = (DaoSema*) par[0]->v.p;
	DSema_Post( & self->mySema );
}
static void DaoSema_Lib_SetValue( DaoContext *ctx, DValue *par[], int N )
{
	DaoSema *self = (DaoSema*) par[0]->v.p;
	DSema_SetValue( & self->mySema, par[1]->v.i );
}
static void DaoSema_Lib_GetValue( DaoContext *ctx, DValue *par[], int N )
{
	DaoSema *self = (DaoSema*) par[0]->v.p;
	DaoContext_PutInteger( ctx, DSema_GetValue( & self->mySema ) );
}
static DaoFuncItem semaMeths[] =
{
	{ DaoSema_Lib_Wait,      "wait( self : semaphore )" },
	{ DaoSema_Lib_Post,      "post( self : semaphore )" },
	{ DaoSema_Lib_SetValue,  "setvalue( self : semaphore, n :int )" },
	{ DaoSema_Lib_GetValue,  "getvalue( self : semaphore )=>int" },
	{ NULL, NULL }
};
static DaoTypeCore semaCore =
{
	0, NULL, NULL, NULL, NULL,
	DaoBase_SafeGetField,
	DaoBase_SafeSetField,
	DaoBase_GetItem,
	DaoBase_SetItem,
	DaoBase_Print,
	DaoBase_Copy,
};
DaoTypeBase semaTyper =
{
	"semaphore", & semaCore, NULL, (DaoFuncItem*) semaMeths, {0},
	(FuncPtrDel) DaoSema_Delete, NULL
};
DaoSema* DaoSema_New( int n )
{
	DaoSema* self = (DaoSema*) dao_malloc( sizeof(DaoSema) );
	DaoBase_Init( self, DAO_SEMA );
	DSema_Init( & self->mySema, n );
	return self;
}
void DaoSema_Delete( DaoSema *self )
{
	DSema_Destroy( & self->mySema );
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
	DSema_SetValue( & self->mySema, n );
}
int  DaoSema_GetValue( DaoSema *self )
{
	return DSema_GetValue( & self->mySema );
}

/* Thread */
static void DaoThdMaster_EraseRecord( DaoThdMaster *self, DaoThread *thd );
static void DaoCleanThread( void *arg )
{
	DaoThread *self = (DaoThread*) arg;
	DNode *node = DMap_First( self->mutexUsed );
	for( ; node != NULL; node = DMap_Next(self->mutexUsed, node) ){
		DaoMutex_Unlock( (DaoMutex*)node->key.pVoid );
		GC_DecRC( node->key.pVoid );
	}

	if( self->thdMaster ) DaoThdMaster_EraseRecord( self->thdMaster, self );
	if( self->exitRefCount ) GC_DecRC( self );
	self->isRunning = 0;
}

static void DaoThread_Lib_MyData( DaoContext *ctx, DValue *par[], int N )
{
	DaoThread *self = (DaoThread*) par[0]->v.p;
	DaoContext_SetResult( ctx, (DaoBase*)self->myMap );
}
static void DaoThread_Lib_Join( DaoContext *ctx, DValue *par[], int N )
{
	DaoThread *self = (DaoThread*) par[0]->v.p;
	DThread_Join( & self->myThread );
}
static void DaoThread_Lib_Detach( DaoContext *ctx, DValue *par[], int N )
{
	DaoThread *self = (DaoThread*) par[0]->v.p;
	DThread_Detach( & self->myThread );
}
static void DaoThread_Lib_Cancel( DaoContext *ctx, DValue *par[], int N )
{
	DaoThread *self = (DaoThread*) par[0]->v.p;
	DThread_Cancel( & self->myThread );
}
static DaoFuncItem threadMeths[] =
{
	{ DaoThread_Lib_MyData,      "mydata( self : thread )=>map<string,any>" },
	{ DaoThread_Lib_Join,        "join( self : thread )" },
	{ DaoThread_Lib_Detach,      "detach( self : thread )" },
	{ DaoThread_Lib_Cancel,      "cancel( self : thread )" },
	{ NULL, NULL }
};

static void DaoThread_Delete( DaoThread *self );

static DaoTypeCore threadCore =
{
	0, NULL, NULL, NULL, NULL,
	DaoBase_SafeGetField,
	DaoBase_SafeSetField,
	DaoBase_GetItem,
	DaoBase_SetItem,
	DaoBase_Print,
	DaoBase_Copy,
};
DaoTypeBase threadTyper =
{
	"thread", & threadCore, NULL, (DaoFuncItem*) threadMeths, {0},
	(FuncPtrDel) DaoThread_Delete, NULL
};

DaoThread* DaoThread_New( DaoThdMaster *thdm )
{
	DaoThread* self = (DaoThread*) dao_malloc( sizeof(DaoThread) );
	DaoBase_Init( self, DAO_THREAD );
	DThread_Init( & self->myThread );
	self->process = NULL;
	self->thdMaster = thdm;
	self->myMap = DaoMap_New(1);
	self->myMap->refCount ++;
	self->mutexUsed = DMap_New(0,0);
	self->myThread.cleaner = DaoCleanThread;
	self->exitRefCount = 0;
	self->isRunning = 0;
	return self;
}
void DaoThread_Delete( DaoThread *self )
{
	DThread_Destroy( & self->myThread );
}

static void DaoThdMaster_InsertRecord( DaoThdMaster *self, DaoThread *thd );

static void DaoThread_Wrapper( void *p )
{
	DaoThread *self = (DaoThread*)p;
	self->taskFunc( self->taskArg );
	self->isRunning = 0;
}

int DaoThread_Start( DaoThread *self, DThreadTask task, void *arg )
{
	if( self->thdMaster ) DaoThdMaster_InsertRecord( self->thdMaster, self );
	self->isRunning = 1;
	self->taskFunc = task;
	self->taskArg = arg;
	return DThread_Start( & self->myThread, DaoThread_Wrapper, self );
}
void DaoThread_TestCancel( DaoThread *self )
{
	DThread_TestCancel( & self->myThread );
}

typedef struct DaoCFunctionCallData DaoCFunctionCallData;
struct DaoCFunctionCallData
{
	DaoContext  *context;
	DaoFunction *function;
	DValue selfpar;
	DValue par[DAO_MAX_PARAM];
	DValue *par2[DAO_MAX_PARAM];
	int npar;
};
static void DaoCFunction_Execute( DaoCFunctionCallData *self )
{
	int i, npar = self->npar;
	if( self->selfpar.t && (self->function->attribs & DAO_ROUT_PARSELF) ) npar ++;
	self->context->thisFunction = self->function;
	self->function->pFunc( self->context, self->par2, npar );
	self->context->thisFunction = NULL;
	for(i=0; i<npar; i++) DValue_Clear( self->par + i );
	DValue_Clear( & self->selfpar );
	DaoVmProcess_Delete( self->context->process );
	GC_DecRC( self->context );
	GC_DecRC( self->function );
	dao_free( self );
}
static void DaoVmProcess_Execute2( DaoVmProcess *self )
{
	DaoVmProcess_Execute( self );
	DaoVmProcess_Delete( self );
}

/* thread master */
static void DaoThdMaster_Lib_Create( DaoContext *ctx, DValue *par[], int N )
{ 
	DaoThdMaster *self = (DaoThdMaster*) par[0]->v.p;
	DaoThread *thread;
	DaoVmProcess *vmProc;
	DaoContext *thdCtx = 0;
	DaoObject *obj;
	DRoutine *rout;
	DValue *buffer[DAO_MAX_PARAM];
	DValue **params = par + 2;
	DValue rov = *par[1];
	DValue selfobj = daoNullValue;
	int i;

	if( rov.t == DAO_PAIR ){
		DaoPair *pair = rov.v.pair;
		rov = pair->second;
		selfobj = pair->first;
	}
	N -= 2;
	if( rov.t == DAO_FUNCURRY ){
		DaoFunCurry *curry = (DaoFunCurry*) rov.v.p;
		selfobj = curry->selfobj;
		rov = curry->callable;
		N = curry->params->size;
		if( N > DAO_MAX_PARAM ) N = DAO_MAX_PARAM; /* XXX warning */
		for(i=0; i<N; i++) buffer[i] = curry->params->data + i;
		params = buffer;
	}
	if( rov.t != DAO_ROUTINE && rov.t != DAO_FUNCTION ) goto ErrorParam;
	rout = DRoutine_GetOverLoad( (DRoutine*)rov.v.routine, & selfobj, params, N, DVM_CALL );
	if( rout == NULL ) goto ErrorParam;
	if( rout->type == DAO_ROUTINE ){
		DaoRoutine *drout = (DaoRoutine*) rout;
		if( drout->parser ) DaoRoutine_Compile( drout );
		if( rout->attribs & DAO_ROUT_NEEDSELF ){
			if( selfobj.t != DAO_OBJECT || drout->routHost ==NULL ) goto ErrorParam;
			obj = selfobj.v.object;
			if( ! DaoClass_ChildOf( obj->myClass, drout->routHost->X.extra ) ) goto ErrorParam;
		}
	}
	thread = DaoThread_New( self );
	vmProc = DaoVmProcess_New( ctx->vmSpace );
	DaoContext_SetResult( ctx, (DaoBase*)thread );
	thdCtx = DaoContext_New();
	thread->process = vmProc;
	thdCtx->vmSpace = ctx->vmSpace;
	if( rout->attribs & DAO_ROUT_NEEDSELF ){
		thdCtx->object = selfobj.v.object;
		GC_IncRC( thdCtx->object );
	}
	DaoContext_Init( thdCtx, (DaoRoutine*) rout );
	thdCtx->process = vmProc;
	if( rout->type == DAO_ROUTINE ){
		DaoVmProcess_PushContext( vmProc, thdCtx );
		if( ! DRoutine_PassParams( (DRoutine*)rout, & selfobj, 
					thdCtx->regValues, params, NULL, N, DVM_CALL ) ){
			DaoVmProcess_Delete( vmProc );
			goto ErrorParam;
		}
		GC_IncRC( thread );
		thread->exitRefCount = 1;
		DaoThread_Start( thread, (DThreadTask) DaoVmProcess_Execute2, vmProc );
	}else if( rout->type == DAO_FUNCTION ){
		DaoCFunctionCallData *calldata = dao_calloc( 1, sizeof(DaoCFunctionCallData) );
		for(i=0; i<DAO_MAX_PARAM; i++) calldata->par2[i] = calldata->par + i;
		if( N > DAO_MAX_PARAM ) N = DAO_MAX_PARAM; /* XXX warning */
		calldata->npar = N;
		if( ! DRoutine_PassParams( (DRoutine*)rout, & selfobj, 
					calldata->par2, params, NULL, N, DVM_CALL ) ){
			DValue_ClearAll( calldata->par, N );
			DaoVmProcess_Delete( vmProc );
			dao_free( calldata );
			goto ErrorParam;
		}
		DValue_Copy( & calldata->selfpar, selfobj );
		calldata->function = (DaoFunction*) rout;
		calldata->context = thdCtx;
		GC_IncRC( rout );
		GC_IncRC( thdCtx );
		GC_IncRC( thread );
		thread->exitRefCount = 1;
		DaoThread_Start( thread, (DThreadTask) DaoCFunction_Execute, calldata );
	}
	return;
ErrorParam:
	DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "invalid parameter for creating thread" );
	return;
}
static void DaoThdMaster_Lib_Exit( DaoContext *ctx, DValue *par[], int N )
{
	DaoThdMaster *self = (DaoThdMaster*) par[0]->v.p;
	DaoThread *thread = GetThisThread( self );
	DThread_Exit( & thread->myThread );
}
static void DaoThdMaster_Lib_TestCancel( DaoContext *ctx, DValue *par[], int N )
{
	DaoThdMaster *self = (DaoThdMaster*) par[0]->v.p;
	DaoThread *thread = GetThisThread( self );
	DThread_TestCancel( & thread->myThread );
}
static void DaoThdMaster_Lib_Self( DaoContext *ctx, DValue *par[], int N )
{
	DaoThdMaster *self = (DaoThdMaster*) par[0]->v.p;
	DaoContext_SetResult( ctx, (DaoBase*)GetThisThread( self ) );
}
static void DaoThdMaster_Lib_MyData( DaoContext *ctx, DValue *par[], int N )
{
	DaoThdMaster *self = (DaoThdMaster*) par[0]->v.p;
	DaoThread *thread = GetThisThread( self );
	DaoContext_SetResult( ctx, (DaoBase*)thread->myMap );
}
static void DaoThdMaster_Lib_Mutex( DaoContext *ctx, DValue *par[], int N )
{
	DaoThdMaster *self = (DaoThdMaster*) par[0]->v.p;
	DaoMutex *mutex = DaoMutex_New( NULL );
	mutex->thdMaster = self;
	DaoContext_SetResult( ctx, (DaoBase*) mutex );
}
static void DaoThdMaster_Lib_CondVar( DaoContext *ctx, DValue *par[], int N )
{
	DaoThdMaster *self = (DaoThdMaster*) par[0]->v.p;
	DaoContext_SetResult( ctx, (DaoBase*)DaoCondVar_New( self ) );
}
static void DaoThdMaster_Lib_Sema( DaoContext *ctx, DValue *par[], int N )
{
	DaoContext_SetResult( ctx, (DaoBase*)DaoSema_New( 0 ) );
}

static DaoFuncItem thdMasterMeths[] =
{
	{ DaoThdMaster_Lib_Create,      "thread( self : mtlib, object, ... )const=>thread" },
	{ DaoThdMaster_Lib_Mutex,       "mutex( self : mtlib )const=>mutex" },
	{ DaoThdMaster_Lib_CondVar,     "condition( self : mtlib )const=>condition" },
	{ DaoThdMaster_Lib_Sema,        "semaphore( self : mtlib )const=>semaphore" },
	{ DaoThdMaster_Lib_Exit,        "exit( self : mtlib )const" },
	{ DaoThdMaster_Lib_TestCancel,  "testcancel( self : mtlib )const" },
	{ DaoThdMaster_Lib_Self,        "self( self : mtlib )const=>thread" },
	{ DaoThdMaster_Lib_MyData,      "mydata( self : mtlib )const=>map<string,any>" },
	{ NULL, NULL }
};

static void DaoThdMaster_Delete( DaoThdMaster *self );

static DaoTypeCore thdMasterCore =
{
	0, NULL, NULL, NULL, NULL,
	DaoBase_SafeGetField,
	DaoBase_SafeSetField,
	DaoBase_GetItem,
	DaoBase_SetItem,
	DaoBase_Print,
	DaoBase_Copy,
};
DaoTypeBase thdMasterTyper =
{
	"mtlib", & thdMasterCore, NULL, (DaoFuncItem*) thdMasterMeths, {0},
	(FuncPtrDel) DaoThdMaster_Delete, NULL
};

DaoThdMaster* DaoThdMaster_New()
{
	DaoThdMaster *self = (DaoThdMaster*) dao_malloc( sizeof(DaoThdMaster) );
	DaoBase_Init( (DaoBase*)self, DAO_THDMASTER );
	DMutex_Init( & self->recordMutex );
	self->thdRecords = DArray_New(0);
	return self;
}

static void DaoThdMaster_Delete( DaoThdMaster *self )
{
	DMutex_Destroy( & self->recordMutex );
	DArray_Delete( self->thdRecords );
	dao_free( self );
}
static DaoThread* DaoThdMaster_FindRecord( DaoThdMaster *self, dao_thread_t tid )
{
	DaoThread *thd = 0;
	int i;
	DaoThread **threads;

	DMutex_Lock( & self->recordMutex );
	threads = (DaoThread**)self->thdRecords->items.pVoid;
	for(i=0; i<self->thdRecords->size; i++ ){
		if( DThread_Equal( tid, threads[i]->myThread.myThread ) ){
			thd = threads[i];
			break;
		}
	}
	DMutex_Unlock( & self->recordMutex );
	return thd;
}
static void DaoThdMaster_InsertRecord( DaoThdMaster *self, DaoThread *thd )
{
	DMutex_Lock( & self->recordMutex );
	DArray_Append( self->thdRecords, thd );
	DMutex_Unlock( & self->recordMutex );
}
static void DaoThdMaster_EraseRecord( DaoThdMaster *self, DaoThread *thd )
{
	int i;
	DaoThread **threads;

	DMutex_Lock( & self->recordMutex );
	threads = (DaoThread**)self->thdRecords->items.pVoid;
	for(i=0; i<self->thdRecords->size; i++ )
		if( DThread_Equal( thd->myThread.myThread, threads[i]->myThread.myThread ) ) break;

	DArray_Erase( self->thdRecords, i, 1 );
	DMutex_Unlock( & self->recordMutex );
}

void DaoInitThread()
{
	DaoInitThreadSys();
}
void DaoStopThread( DaoThdMaster *self )
{
	DArray *threads = DArray_New(0);
	DaoThread *thread;
	int i, T;

	DMutex_Lock( & self->recordMutex );
	T = self->thdRecords->size;
	for(i=0; i<T; i++){
		thread = (DaoThread*)self->thdRecords->items.pVoid[i];
		GC_IncRC( thread ); /* avoid it being deleted */
		DArray_Append( threads, thread );
	}
	DMutex_Unlock( & self->recordMutex );

	if( T ) printf( "Warning: terminating for un-joined thread(s) ...\n" );
	for(i=0; i<T; i++){
		thread = (DaoThread*)threads->items.pVoid[i];
		thread->process->stopit = 1;
		DThread_Join( & thread->myThread );
		GC_DecRC( thread );
	}
	DArray_Delete( threads );
}
#endif

