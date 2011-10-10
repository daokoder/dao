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

static DaoThread* DaoThdMaster_FindRecord( DaoThdMaster *self, dao_thread_t tid );

static DaoThread* GetThisThread( DaoThdMaster *thdMaster )
{
	if( thdMaster ){
		dao_thread_t tid = DThread_Self();
		return DaoThdMaster_FindRecord( thdMaster, tid );
	}
	return NULL;
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

DaoMutex* DaoMutex_New( DaoVmSpace *vms )
{
	DaoMutex* self = (DaoMutex*) dao_calloc( 1, sizeof(DaoMutex) );
	DaoValue_Init( self, DAO_MUTEX );
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
DaoCondVar* DaoCondVar_New( DaoThdMaster *thdm )
{
	DaoCondVar* self = (DaoCondVar*) dao_calloc( 1, sizeof(DaoCondVar) );
	DaoValue_Init( self, DAO_CONDVAR );
	DCondVar_Init( & self->myCondVar );
	self->thdMaster = thdm;
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
	DSema_Init( & self->mySema, n );
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

static void DaoThread_Lib_MyData( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoThread *self = (DaoThread*) par[0];
	DaoProcess_PutValue( proc, (DaoValue*)self->myMap );
}
static void DaoThread_Lib_Join( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoThread *self = (DaoThread*) par[0];
	DThread_Join( & self->myThread );
}
static void DaoThread_Lib_Detach( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoThread *self = (DaoThread*) par[0];
	DThread_Detach( & self->myThread );
}
static void DaoThread_Lib_Cancel( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoThread *self = (DaoThread*) par[0];
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
	NULL,
	DaoValue_SafeGetField,
	DaoValue_SafeSetField,
	DaoValue_GetItem,
	DaoValue_SetItem,
	DaoValue_Print,
	DaoValue_NoCopy,
};
DaoTypeBase threadTyper =
{
	"thread", & threadCore, NULL, (DaoFuncItem*) threadMeths, {0}, {0},
	(FuncPtrDel) DaoThread_Delete, NULL
};

DaoThread* DaoThread_New( DaoThdMaster *thdm )
{
	DaoThread* self = (DaoThread*) dao_calloc( 1, sizeof(DaoThread) );
	DaoValue_Init( self, DAO_THREAD );
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
	GC_DecRC( self->process );
	GC_DecRC( self->myMap );
	DMap_Delete( self->mutexUsed );
	dao_free( self );
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

static void DaoProcess_Execute2( DaoProcess *self )
{
	DaoProcess_Execute( self );
}

/* thread master */
static void DaoThdMaster_Lib_Create( DaoProcess *proc, DaoValue *par[], int N )
{ 
	DaoThread *thread;
	DaoThdMaster *self = (DaoThdMaster*) par[0];
	DaoProcess *vmProc = NULL;
	DaoValue **params = par + 2;
	DaoValue *callable = par[1];
	DaoValue *selfobj = NULL;
	DRoutine *rout;
	int i;

	DaoCGC_Start();

	N -= 2;
	if( callable && callable->type == DAO_FUNCURRY ){
		DaoFunCurry *curry = (DaoFunCurry*) callable;
		selfobj = curry->selfobj;
		callable = curry->callable;
		N = curry->params->size;
		if( N > DAO_MAX_PARAM ) N = DAO_MAX_PARAM; /* XXX warning */
		params = curry->params->items.pValue;
	}
	vmProc = DaoProcess_New( proc->vmSpace );
	GC_IncRC( vmProc );
	if( DaoProcess_PushCallable( vmProc, callable, selfobj, params, N ) ){
		GC_DecRC( vmProc );
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "invalid parameter for creating thread" );
		return;
	}
	thread = DaoThread_New( self );
	thread->process = vmProc;
	thread->exitRefCount = 1;
	DaoProcess_PutValue( proc, (DaoValue*)thread );
	DaoThread_Start( thread, (DThreadTask) DaoProcess_Execute2, vmProc );
}
static void DaoThdMaster_Lib_Exit( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoThdMaster *self = (DaoThdMaster*) par[0];
	DaoThread *thread = GetThisThread( self );
	DThread_Exit( & thread->myThread );
}
static void DaoThdMaster_Lib_TestCancel( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoThdMaster *self = (DaoThdMaster*) par[0];
	DaoThread *thread = GetThisThread( self );
	DThread_TestCancel( & thread->myThread );
}
static void DaoThdMaster_Lib_Self( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoThdMaster *self = (DaoThdMaster*) par[0];
	DaoProcess_PutValue( proc, (DaoValue*)GetThisThread( self ) );
}
static void DaoThdMaster_Lib_MyData( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoThdMaster *self = (DaoThdMaster*) par[0];
	DaoThread *thread = GetThisThread( self );
	DaoProcess_PutValue( proc, (DaoValue*)thread->myMap );
}
static void DaoThdMaster_Lib_Mutex( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoThdMaster *self = (DaoThdMaster*) par[0];
	DaoMutex *mutex = DaoMutex_New( NULL );
	mutex->thdMaster = self;
	DaoProcess_PutValue( proc, (DaoValue*) mutex );
}
static void DaoThdMaster_Lib_CondVar( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoThdMaster *self = (DaoThdMaster*) par[0];
	DaoProcess_PutValue( proc, (DaoValue*)DaoCondVar_New( self ) );
}
static void DaoThdMaster_Lib_Sema( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoProcess_PutValue( proc, (DaoValue*)DaoSema_New( 0 ) );
}

typedef struct DaoTaskData DaoTaskData;
struct DaoTaskData
{
	DaoValue    *param; // parameter container: list, map or array;
	DaoValue    *result; // result container: list or array;
	DaoProcess  *proto; // caller's process;
	DaoProcess  *clone; // spawned process;
	DaoVmCode   *sect; // DVM_SECT
	DCondVar    *condv;
	DMutex      *mutex;

	DNode   *node; // last executed key value node;
	uint_t   index; // last executed index;

	uint_t   funct; // type of functional;
	uint_t   entry; // entry code;
	uint_t   first; // first index;
	uint_t   step; // index step;
	uint_t   status; // execution status;
	uint_t  *joined; // number of joined threads
};

static void DaoMT_InitProcess( DaoProcess *proto, DaoProcess *clone )
{
	DaoProcess_PushRoutine( clone, proto->activeRoutine, proto->activeObject );
	clone->activeCode = proto->activeCode;
	DaoProcess_PushFunction( clone, proto->topFrame->function );
	DaoProcess_SetActiveFrame( clone, clone->topFrame );
	DaoProcess_PushSectionFrame( clone );
	clone->topFrame->outer = proto;
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
	DaoValue **items = list->items->items.pValue;
	size_t i, n = list->items->size;

	DaoMT_InitProcess( self->proto, clone );
	tidint.value = self->first;
	for(i=self->first; i<n; i+=self->step){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( clone, sect->a, items[i] );
		if( sect->b >1 ) DaoProcess_SetValue( clone, sect->a+1, index );
		if( sect->b >2 ) DaoProcess_SetValue( clone, sect->a+2, threadid );
		clone->topFrame->entry = self->entry;
		DaoProcess_Execute( clone );
		if( clone->status == DAO_VMPROC_ABORTED ) break;
		res = clone->stackValues[0];
		if( self->funct == DVM_FUNCT_MAP ){
			self->status |= DaoList_SetItem( list2, res, i );
		}else if( self->funct == DVM_FUNCT_FIND && res->xInteger.value ){
			break;
		}
	}
	self->status |= clone->status == DAO_VMPROC_ABORTED;
	self->index = i;
	DMutex_Lock( self->mutex );
	*self->joined += 1;
	DCondVar_Signal( self->condv );
	DMutex_Unlock( self->mutex );
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
	DNode *node = NULL;
	size_t i = 0;
	DaoMT_InitProcess( self->proto, clone );
	tidint.value = self->first;
	for(node=DMap_First( map->items ); node; node=DMap_Next(map->items, node) ){
		if( (i++) % self->step != self->first ) continue;
		if( sect->b >0 ) DaoProcess_SetValue( clone, sect->a, node->key.pValue );
		if( sect->b >1 ) DaoProcess_SetValue( clone, sect->a+1, node->value.pValue );
		if( sect->b >2 ) DaoProcess_SetValue( clone, sect->a+2, threadid );
		clone->topFrame->entry = self->entry;
		DaoProcess_Execute( clone );
		if( clone->status == DAO_VMPROC_ABORTED ) break;
		res = clone->stackValues[0];
		if( self->funct == DVM_FUNCT_MAP ){
			self->status |= DaoList_SetItem( list2, res, i-1 );
		}else if( self->funct == DVM_FUNCT_FIND && res->xInteger.value ){
			break;
		}
	}
	self->status |= clone->status == DAO_VMPROC_ABORTED;
	self->node = node;
	DMutex_Lock( self->mutex );
	*self->joined += 1;
	DCondVar_Signal( self->condv );
	DMutex_Unlock( self->mutex );
}
static void DaoMT_RunFunctional( void *p )
{
	DaoTaskData *self = (DaoTaskData*)p;
	switch( self->param->type ){
	case DAO_LIST  : DaoMT_RunListFunctional( p ); break;
	case DAO_MAP   : DaoMT_RunMapFunctional( p ); break;
	//case DAO_ARRAY : DaoMT_RunArrayFunctional( p ); break;
	}
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
	case DVM_FUNCT_FIND : DaoProcess_PutValue( proc, null ); break;
	}
	if( threads <= 0 ) threads = 2;
	if( sect == NULL ) return;
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	if( list ){
		DArray_Clear( list->items );
		if( param->type == DAO_LIST ) DArray_Resize( list->items, param->xList.items->size, NULL );
		if( param->type == DAO_MAP ) DArray_Resize( list->items, param->xMap.items->size, NULL );
	}else if( array ){
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
		task->condv = & condv;
		task->mutex = & mutex;
		task->proto = proc;
		task->sect = sect;
		task->funct = F;
		task->entry = entry;
		task->first = i;
		task->step = threads;
		task->joined = & joined;
		task->clone = DaoVmSpace_AcquireProcess( proc->vmSpace );
		if( i ) DaoCallServer_AddTask( DaoMT_RunFunctional, task );
	}
	DaoMT_RunFunctional( tasks );

	DMutex_Lock( & mutex );
	while( joined < threads ) DCondVar_TimedWait( & condv, & mutex, 0.01 );
	DMutex_Unlock( & mutex );

	for(i=0; i<threads; i++){
		DaoTaskData *task = tasks + i;
		status |= task->status;
		if( task->index < index ) index = task->index;
		if( task->node == NULL ) continue; 
		if( node == NULL || DaoValue_Compare( node->key.pValue, task->node->key.pValue ) > 0 ){
			node = task->node;
		}
	}
	if( F == DVM_FUNCT_FIND ){
		DaoTuple *tuple = DaoProcess_PutTuple( proc );
		if( param->type == DAO_LIST && index != -1 ){
			DaoValue **items = param->xList.items->items.pValue;
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
}
static void DaoMT_Run( DaoProcess *proc, DaoValue *p[], int n )
{
	//DaoMT_Functional( proc, NULL, p[0]->xInteger.value, 0, p[1]->xInteger.value );
}
static void DaoMT_ListEach( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_EACH );
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
static void DaoMT_MapEach( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoMT_Functional( proc, p, n, DVM_FUNCT_EACH );
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

static DaoFuncItem thdMasterMeths[] =
{
	{ DaoThdMaster_Lib_Create,      "thread( self : mt, object, ... )=>thread" },
	{ DaoThdMaster_Lib_Mutex,       "mutex( self : mt )=>mutex" },
	{ DaoThdMaster_Lib_CondVar,     "condition( self : mt )=>condition" },
	{ DaoThdMaster_Lib_Sema,        "semaphore( self : mt )=>semaphore" },
	{ DaoThdMaster_Lib_Exit,        "exit( self : mt )" },
	{ DaoThdMaster_Lib_TestCancel,  "testcancel( self : mt )" },
	{ DaoThdMaster_Lib_Self,        "self( self : mt )=>thread" },
	{ DaoThdMaster_Lib_MyData,      "mydata( self : mt )=>map<string,any>" },

	{ DaoMT_Run, "run( times=1, threads=2 )[index:int,threadid:int]" },

	{ DaoMT_ListEach, "each( alist :list<@T>, threads=2 )[item:@T,index:int,threadid:int]" },
	{ DaoMT_ListMap, "map( alist :list<@T>, threads=2 )[item:@T,index:int,threadid:int =>@T2] =>list<@T2>" },
	{ DaoMT_ListApply, "apply( alist :list<@T>, threads=2 )[item:@T,index:int,threadid:int =>@T] =>list<@T>" },
	{ DaoMT_ListFind, "find( alist :list<@T>, threads=2 )[item:@T,index:int,threadid:int =>int] =>tuple<index:int,item:@T>|null" },

	{ DaoMT_MapEach, "each( amap :map<@K,@V>, threads=2 )[key:@K,value:@V,threadid:int]" },
	{ DaoMT_MapMap, "map( amap :map<@K,@V>, threads=2 )[key:@K,value:@V,threadid:int =>@T] =>list<@T>" },
	{ DaoMT_MapApply, "apply( amap :map<@K,@V>, threads=2 )[key:@K,value:@V,threadid:int =>@V] =>map<@K,@V>" },
	{ DaoMT_MapFind, "find( amap :map<@K,@V>, threads=2 )[key:@K,value:@V,threadid:int =>int] =>tuple<key:@K,value:@V>|null" },
	{ NULL, NULL }
};

static void DaoThdMaster_Delete( DaoThdMaster *self );

static DaoTypeCore thdMasterCore =
{
	NULL,
	DaoValue_SafeGetField,
	DaoValue_SafeSetField,
	DaoValue_GetItem,
	DaoValue_SetItem,
	DaoValue_Print,
	DaoValue_NoCopy,
};
DaoTypeBase thdMasterTyper =
{
	"mt", & thdMasterCore, NULL, (DaoFuncItem*) thdMasterMeths, {0}, {0},
	(FuncPtrDel) DaoThdMaster_Delete, NULL
};

DaoThdMaster* DaoThdMaster_New()
{
	DaoThdMaster *self = (DaoThdMaster*) dao_calloc( 1, sizeof(DaoThdMaster) );
	DaoValue_Init( (DaoValue*)self, DAO_THDMASTER );
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

