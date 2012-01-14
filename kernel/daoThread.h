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

#define DMutex_Lock( x ) (1 == 1)
#define DMutex_Unlock( x ) (1 == 1)

#endif /* DAO_WITH_THREAD */


#ifdef DAO_WITH_CONCURRENT

/* Dao threading types: */
struct DaoMutex
{
	DAO_DATA_COMMON;

	DMutex         myMutex;
};
DAO_DLL DaoMutex* DaoMutex_New();
DAO_DLL void DaoMutex_Lock( DaoMutex *self );
DAO_DLL void DaoMutex_Unlock( DaoMutex *self );
DAO_DLL int DaoMutex_TryLock( DaoMutex *self );

struct DaoCondVar
{
	DAO_DATA_COMMON;

	DCondVar       myCondVar;
};
DAO_DLL DaoCondVar* DaoCondVar_New();
DAO_DLL void DaoCondVar_Delete( DaoCondVar *self );

DAO_DLL void DaoCondVar_Wait( DaoCondVar *self, DaoMutex *mutex );
DAO_DLL int  DaoCondVar_TimedWait( DaoCondVar *self, DaoMutex *mutex, double seconds );
/* return true if time out. */

DAO_DLL void DaoCondVar_Signal( DaoCondVar *self );
DAO_DLL void DaoCondVar_BroadCast( DaoCondVar *self );

struct DaoSema
{
	DAO_DATA_COMMON;

	DSema     mySema;
};
DAO_DLL DaoSema* DaoSema_New( int n );
DAO_DLL void DaoSema_Delete( DaoSema *self );

DAO_DLL void DaoSema_Wait( DaoSema *self );
DAO_DLL void DaoSema_Post( DaoSema *self );

DAO_DLL void DaoSema_SetValue( DaoSema *self, int n );
DAO_DLL int  DaoSema_GetValue( DaoSema *self );

DAO_DLL DaoFuture* DaoFuture_New();
DAO_DLL void DaoProcess_ReturnFutureValue( DaoProcess *self, DaoFuture *future );

#endif /* DAO_WITH_CONCURRENT */

enum{ DAO_CALL_QUEUED, DAO_CALL_RUNNING, DAO_CALL_PAUSED, DAO_CALL_FINISHED };
enum{ DAO_FUTURE_VALUE, DAO_FUTURE_WAIT };

struct DaoFuture
{
	DAO_DATA_COMMON;

	uchar_t      state;
	uchar_t      state2;
	short        parCount;
	DaoType     *unitype;
	DaoValue    *value;
	DaoValue    *params[DAO_MAX_PARAM];
	DaoObject   *object;
	DaoRoutine  *routine;
	DaoProcess  *process;
	DaoFuture   *precondition;
};

#endif
