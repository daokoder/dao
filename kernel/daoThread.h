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

#ifndef DAO_THREAD_H
#define DAO_THREAD_H

#include"daoType.h"

#ifdef DAO_WITH_THREAD

/* Basic threading interfaces */

#ifdef UNIX

#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>

#define dao_mutex_t    pthread_mutex_t
#define dao_cond_t     pthread_cond_t
#define dao_sema_t     sem_t
#define dao_thread_t   pthread_t
#define dao_thdspec_t  pthread_key_t
#define dao_retcode_t  int

#elif WIN32

#include <windows.h>
#include <process.h>

#define dao_mutex_t CRITICAL_SECTION
#define dao_cond_t  HANDLE
#define dao_sema_t  HANDLE
#define dao_thread_t    HANDLE
#define dao_thdspec_t   DWORD
#define dao_retcode_t   DWORD

#endif

typedef struct DMutex       DMutex;
typedef struct DCondVar     DCondVar;
typedef struct DSema        DSema;
typedef struct DThreadData  DThreadData;
typedef struct DThread      DThread;

struct DMutex
{
	dao_mutex_t myMutex;
};
DAO_DLL void DMutex_Init( DMutex *self );
DAO_DLL void DMutex_Destroy( DMutex *self );
DAO_DLL void DMutex_Lock( DMutex *self );
DAO_DLL void DMutex_Unlock( DMutex *self );
DAO_DLL int DMutex_TryLock( DMutex *self );

struct DCondVar
{
	dao_cond_t myCondVar;
#ifdef WIN32
	DMutex thdMutex;
	/* manual-reset, auto-reset and an auxilary holder. */
	DArray *thdWaiting;
#endif
};
DAO_DLL void DCondVar_Init( DCondVar *self );
DAO_DLL void DCondVar_Destroy( DCondVar *self );
DAO_DLL void DCondVar_Wait( DCondVar *self, DMutex *mutex );
DAO_DLL int  DCondVar_TimedWait( DCondVar *self, DMutex *mutex, double seconds );
/* return true if time out. */

DAO_DLL void DCondVar_Signal( DCondVar *self );
DAO_DLL void DCondVar_BroadCast( DCondVar *self );

struct DSema
{
	dao_sema_t  mySema;
	int         count;
};
DAO_DLL void DSema_Init( DSema *self, int n );
DAO_DLL void DSema_Destroy( DSema *self );
DAO_DLL void DSema_Wait( DSema *self );
DAO_DLL void DSema_Post( DSema *self );

enum DThreadState
{
	DTHREAD_CANCELED = 1,
	DTHREAD_NO_PAUSE = (1<<1)
};

struct DThreadData
{
	DThread *thdObject;
	int      state;
};

typedef void (*DThreadCleanUp)( void *thread );

struct DThread
{
	dao_thread_t     myThread;
	DThreadCleanUp   cleaner;

	/* in windows, condv will signal when the thread need to be cancelled,
	   used to emulate pthread: */
	DCondVar         condv;

	DThreadData     *thdSpecData;
	int running;

	DThreadTask      taskFunc;
	void            *taskArg;
};
DAO_DLL void DThread_Init( DThread *self );
DAO_DLL void DThread_Destroy( DThread *self );

DAO_DLL int DThread_Start( DThread *self, DThreadTask task, void *arg );
DAO_DLL void DThread_Exit( DThread *self );
DAO_DLL void DThread_Join( DThread *self );
DAO_DLL dao_thread_t DThread_Self();
DAO_DLL int DThread_Equal( dao_thread_t x, dao_thread_t y );

DAO_DLL DThreadData* DThread_GetSpecific();

DAO_DLL void DaoInitThread();

#else

typedef int DMutex;

#define DMutex_Lock( x ) {}
#define DMutex_Unlock( x ) {}

#endif /* DAO_WITH_THREAD */


#ifdef DAO_WITH_CONCURRENT

/* Dao threading types: */
struct DaoMutex
{
	DAO_CSTRUCT_COMMON;

	DMutex  myMutex;
};
DAO_DLL DaoType *dao_type_mutex;

DAO_DLL DaoMutex* DaoMutex_New();
DAO_DLL void DaoMutex_Lock( DaoMutex *self );
DAO_DLL void DaoMutex_Unlock( DaoMutex *self );
DAO_DLL int DaoMutex_TryLock( DaoMutex *self );

struct DaoCondVar
{
	DAO_CSTRUCT_COMMON;

	DCondVar  myCondVar;
};
DAO_DLL DaoType *dao_type_condvar;

DAO_DLL DaoCondVar* DaoCondVar_New();
DAO_DLL void DaoCondVar_Delete( DaoCondVar *self );

DAO_DLL void DaoCondVar_Wait( DaoCondVar *self, DaoMutex *mutex );
DAO_DLL int  DaoCondVar_TimedWait( DaoCondVar *self, DaoMutex *mutex, double seconds );
/* return true if time out. */

DAO_DLL void DaoCondVar_Signal( DaoCondVar *self );
DAO_DLL void DaoCondVar_BroadCast( DaoCondVar *self );

struct DaoSema
{
	DAO_CSTRUCT_COMMON;

	DSema  mySema;
};
DAO_DLL DaoType *dao_type_sema;

DAO_DLL DaoSema* DaoSema_New( int n );
DAO_DLL void DaoSema_Delete( DaoSema *self );

DAO_DLL void DaoSema_Wait( DaoSema *self );
DAO_DLL void DaoSema_Post( DaoSema *self );

DAO_DLL void DaoSema_SetValue( DaoSema *self, int n );
DAO_DLL int  DaoSema_GetValue( DaoSema *self );

DAO_DLL DaoType *dao_type_channel;
DAO_DLL DaoType *dao_type_future;
DAO_DLL DaoFuture* DaoFuture_New( DaoType *vatype );
DAO_DLL void DaoProcess_ReturnFutureValue( DaoProcess *self, DaoFuture *future );

#endif /* DAO_WITH_CONCURRENT */

enum{ DAO_CALL_QUEUED, DAO_CALL_RUNNING, DAO_CALL_PAUSED, DAO_CALL_FINISHED };
enum{ DAO_FUTURE_VALUE, DAO_FUTURE_WAIT };



struct DaoChannel
{
	DAO_CSTRUCT_COMMON;

	DaoType     *mtype;
	DArray      *messages;
};



struct DaoFuture
{
	DAO_CSTRUCT_COMMON;

	uchar_t      state;
	uchar_t      state2;
	DArray      *params;
	DaoValue    *value;
	DaoObject   *actor;
	DaoRoutine  *routine;
	DaoProcess  *process;
	DaoFuture   *precondition; /* the future value on which this one waits; */
	DaoFuture   *sorting; /* the future value of the previous task on the same object; */

	DaoFuture   *prev;
	DaoFuture   *next;
};

#endif
