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
extern void DMutex_Init( DMutex *self );
extern void DMutex_Destroy( DMutex *self );
extern void DMutex_Lock( DMutex *self );
extern void DMutex_Unlock( DMutex *self );
extern int DMutex_TryLock( DMutex *self );

struct DCondVar
{
	dao_cond_t myCondVar;
#ifdef WIN32
	DMutex thdMutex;
	/* manual-reset, auto-reset and an auxilary holder. */
	DArray *thdWaiting;
#endif
};
extern void DCondVar_Init( DCondVar *self );
extern void DCondVar_Destroy( DCondVar *self );
extern void DCondVar_Wait( DCondVar *self, DMutex *mutex );
extern int  DCondVar_TimedWait( DCondVar *self, DMutex *mutex, double seconds );
/* return true if time out. */

extern void DCondVar_Signal( DCondVar *self );
extern void DCondVar_BroadCast( DCondVar *self );

struct DSema
{
	dao_sema_t  mySema;
	int         count;
};
extern void DSema_Init( DSema *self, int n );
extern void DSema_Destroy( DSema *self );
extern void DSema_Wait( DSema *self );
extern void DSema_Post( DSema *self );

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
extern void DThread_Init( DThread *self );
extern void DThread_Destroy( DThread *self );

extern int DThread_Start( DThread *self, DThreadTask task, void *arg );
extern void DThread_Exit( DThread *self );
extern void DThread_Join( DThread *self );
extern dao_thread_t DThread_Self();
extern int DThread_Equal( dao_thread_t x, dao_thread_t y );

DThreadData* DThread_GetSpecific();


/* Dao threading types: */
struct DaoMutex
{
	DAO_DATA_COMMON;

	DMutex         myMutex;
	DaoThdMaster  *thdMaster;
};
extern DaoMutex* DaoMutex_New( DaoVmSpace *vms );
extern void DaoMutex_Lock( DaoMutex *self );
extern void DaoMutex_Unlock( DaoMutex *self );
extern int DaoMutex_TryLock( DaoMutex *self );

struct DaoCondVar
{
	DAO_DATA_COMMON;

	DCondVar       myCondVar;
	DaoThdMaster  *thdMaster;
};
extern DaoCondVar* DaoCondVar_New( DaoThdMaster *thdm );
extern void DaoCondVar_Delete( DaoCondVar *self );

extern void DaoCondVar_Wait( DaoCondVar *self, DaoMutex *mutex );
extern int  DaoCondVar_TimedWait( DaoCondVar *self, DaoMutex *mutex, double seconds );
/* return true if time out. */

extern void DaoCondVar_Signal( DaoCondVar *self );
extern void DaoCondVar_BroadCast( DaoCondVar *self );

struct DaoSema
{
	DAO_DATA_COMMON;

	DSema     mySema;
};
extern DaoSema* DaoSema_New( int n );
extern void DaoSema_Delete( DaoSema *self );

extern void DaoSema_Wait( DaoSema *self );
extern void DaoSema_Post( DaoSema *self );

extern void DaoSema_SetValue( DaoSema *self, int n );
extern int  DaoSema_GetValue( DaoSema *self );

typedef void (*CleanerFunc)( void * );

struct DaoThread
{
	DAO_DATA_COMMON;

	DThread        myThread;

	DThreadTask    taskFunc;
	void          *taskArg;
	short          exitRefCount;
	short          isRunning;

	DaoVmProcess  *process;
	DaoThdMaster  *thdMaster;
	DMap          *mutexUsed; /* <DaoMutex*,int> */
	DaoMap        *myMap;
};
extern DaoThread* DaoThread_New( DaoThdMaster *thdm );

extern int DaoThread_Start( DaoThread *self, DThreadTask task, void *arg );
extern void DaoThread_TestCancel( DaoThread *self );


struct DaoThdMaster
{
	DAO_DATA_COMMON;

	DMutex   recordMutex;

	DArray  *thdRecords;
};

extern DaoThdMaster* DaoThdMaster_New();

extern void DaoInitThread();
extern void DaoStopThread( DaoThdMaster *self );

#endif

#endif
