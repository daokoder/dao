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


#include"stdio.h"
#include"time.h"
#include"daoType.h"
#include"daoValue.h"
#include"daoThread.h"
#include"daoRoutine.h"
#include"daoObject.h"
#include"daoVmspace.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoGC.h"

#if( defined DAO_WITH_THREAD && defined DAO_WITH_ASYNCLASS )

typedef struct DaoCallThread   DaoCallThread;
typedef struct DaoCallServer   DaoCallServer;

extern DaoVmSpace *mainVmSpace;

struct DaoCallThread
{
	DThread       thread;
	DThreadData  *thdData;
};

static DaoCallThread* DaoCallThread_New();
static void DaoCallThread_Run( DaoCallThread *self );

struct DaoCallServer
{
	DMutex   mutex;
	DCondVar condv;

	int finishing;
	int acquiring;
	int total;
	int idle;

	DArray  *functions; /* list of DThreadTask function pointers */
	DArray  *parameters; /* list of void* */
	DArray  *futures; /* list of DaoFuture* */
	DMap    *active; /* map of DaoObject* or DaoProcess* keys */
	DMap    *pending; /* map of pointers from ::parameters and ::futures */

	DaoVmSpace  *vmspace;
};
static DaoCallServer *daoCallServer = NULL;

static DaoCallThread* DaoCallThread_New()
{
	DaoCallThread *self = (DaoCallThread*)dao_malloc( sizeof(DaoCallThread) );
	self->thdData = NULL;
	DThread_Init( & self->thread );
	return self;
}
static DaoCallServer* DaoCallServer_New( DaoVmSpace *vms )
{
	DaoCallServer *self = (DaoCallServer*)dao_malloc( sizeof(DaoCallServer) );
	DMutex_Init( & self->mutex );
	DCondVar_Init( & self->condv );
	self->finishing = 0;
	self->total = 0;
	self->idle = 0;
	self->functions = DArray_New(0);
	self->parameters = DArray_New(0);
	self->futures = DArray_New(0);
	self->pending = DHash_New(0,0);
	self->active = DHash_New(0,0);
	self->vmspace = vms;
	return self;
}
static void DaoCallServer_AddThread()
{
	DaoCallThread *calth = DaoCallThread_New();
	DMutex_Lock( & daoCallServer->mutex );
	daoCallServer->total += 1;
	DMutex_Unlock( & daoCallServer->mutex );
	DThread_Start( & calth->thread, (DThreadTask) DaoCallThread_Run, calth );
}
static void DaoCallServer_Init( DaoVmSpace *vms )
{
	int i;
	DaoCGC_Start();
	daoCallServer = DaoCallServer_New( vms );
	for(i=0; i<2; i++) DaoCallServer_AddThread(); // TODO: set minimumal number of threads
}

void DaoCallServer_AddTask( DThreadTask func, void *param )
{
	DaoCallServer *server;
	if( daoCallServer == NULL ) DaoCallServer_Init( mainVmSpace );
	server = daoCallServer;
	DMutex_Lock( & server->mutex );
	DArray_Append( server->functions, func );
	DArray_Append( server->parameters, param );
	DMap_Insert( server->pending, param, NULL );
	DCondVar_Signal( & server->condv );
	DMutex_Unlock( & server->mutex );
	if( server->parameters->size ) DaoCallServer_AddThread();
}
static void test( void *p )
{
	int i;
	for(i=0; i<1000000; i++){
		if( i % 100000 == 0 ) printf( "%9i  %p\n", i, p );
	}
}
DaoFuture* DaoCallServer_Add( DaoProcess *call, DaoProcess *wait, DaoFuture *pre )
{
	DaoFuture *future = NULL;

	if( daoCallServer == NULL ) DaoCallServer_Init( mainVmSpace );

#if 0
	printf( "DaoCallServer_Add( %12p, %12p, %12p )\n", call, wait, pre );
#endif
	if( call ){ /* new synchronous method call: */
		DaoValue **params = call->stackValues + call->topFrame->stackBase;
		int i, count = call->topFrame->parCount;
		future = DaoFuture_New();
		for(i=0; i<count; i++) DaoValue_Copy( params[i], & future->params[i] );
		future->parCount = count;
		future->state = DAO_CALL_QUEUED;
		future->object = call->topFrame->object;
		future->routine = call->topFrame->routine;
		GC_IncRC( future->routine );
		GC_IncRC( future->object );
	}else if( wait ){
		/* joining the process with the future value's own process */
		if( wait->future == NULL ){
			wait->future = DaoFuture_New();
			wait->future->process = wait;
			GC_IncRC( wait->future );
			GC_IncRC( wait );
		}
		future = wait->future;
		GC_ShiftRC( pre, future->precondition );
		future->precondition = pre;
		future->state = DAO_CALL_PAUSED;
	}
	if( future ){
		DaoCallServer *server = daoCallServer;
		if( server->pending->size > server->total * server->total ) DaoCallServer_AddThread();
		DMutex_Lock( & server->mutex );
		DArray_Append( server->futures, future );
		DMap_Insert( server->pending, future, NULL );
		GC_IncRC( future );
		DCondVar_Signal( & server->condv );
		DMutex_Unlock( & server->mutex );
	}
	//DaoCallServer_AddTask( test, daoCallServer->futures->items.pVoid + daoCallServer->futures->size );
	return future;
}
static DaoFuture* DaoFutures_GetFirstExecutable( DArray *futures, DMap *pending, DMap *active )
{
	int i;
	DaoFuture *first, *future, *precond;

	if( futures->size == 0 ) return NULL;
	future = (DaoFuture*) futures->items.pVoid[0];
	precond = future->precondition;
	first = future;
	while( precond && (precond->state == DAO_CALL_QUEUED || precond->state == DAO_CALL_PAUSED) ){
		DArray_PopFront( futures );
		DArray_PushBack( futures, future );
		future = (DaoFuture*) futures->items.pVoid[0];
		precond = future->precondition;
		if( future == first ) break;
	}
	for(i=0; i<futures->size; i++){
		future = (DaoFuture*) futures->items.pVoid[i];
		if( future->precondition && future->precondition->state != DAO_CALL_FINISHED ) continue;
		if( future->object && DMap_Find( active, future->object->rootObject ) ) continue;
		if( future->process && DMap_Find( active, future->process ) ) continue;
		DArray_Erase( futures, i, 1 );
		DMap_Erase( pending, future );
		if( future->object ) DMap_Insert( active, future->object->rootObject, NULL );
		if( future->process ) DMap_Insert( active, future->process, NULL );
		return future;
	}
	return NULL;
}
static void DaoCallThread_Run( DaoCallThread *self )
{
	DaoCallServer *server = daoCallServer;
	DaoType *type = NULL;
	double wt = 0.001;
	int i, timeout;

	self->thdData = DThread_GetSpecific();
	while(1){
		DaoProcess *proc = NULL;
		DaoFuture *future = NULL;
		DThreadTask function = NULL;
		void *parameter = NULL;

		self->thdData->state = 0;
		DMutex_Lock( & server->mutex );
		server->idle += 1;
		while( server->pending->size == 0 ){
			if( server->finishing && server->idle == server->total ) break;
			timeout = DCondVar_TimedWait( & server->condv, & server->mutex, wt );
		}
		DMutex_Unlock( & server->mutex );
		if( server->pending->size == 0 && server->finishing && server->idle == server->total ) break;

		DMutex_Lock( & server->mutex );
		server->idle -= 1;
		if( server->parameters->size ){
			function = (DThreadTask) DArray_Front( server->functions );
			parameter = DArray_Front( server->parameters );
			DArray_PopFront( server->functions );
			DArray_PopFront( server->parameters );
			DMap_Erase( server->pending, parameter );
		}else{
			future = DaoFutures_GetFirstExecutable( server->futures, server->pending, server->active );
		}
		DMutex_Unlock( & server->mutex );

		if( parameter ) (*function)( parameter );
		if( future == NULL ) continue;
		//printf( "future = %p %i: %i\n", future, server->pending->size, future->state );

		if( future->state == DAO_CALL_QUEUED ){
			int n = future->parCount;
			if( future->process == NULL ){
				future->process = DaoVmSpace_AcquireProcess( server->vmspace );
				GC_IncRC( future->process );
			}
			proc = future->process;
			for(i=0; i<n; i++) DaoValue_Copy( future->params[i], & proc->freeValues[i] );
			future->parCount = 0;
			DaoValue_ClearAll( future->params, n );
			DaoProcess_PushRoutine( proc, future->routine, future->object );
			proc->topFrame->parCount = n;
			DaoProcess_Execute( proc );
		}else if( future->state == DAO_CALL_PAUSED ){
			DaoValue *pars[1] = { NULL };
			if( future->precondition ) pars[0] = future->precondition->value;
			DaoProcess_Resume( future->process, pars, future->precondition != NULL, NULL );
			proc = future->process;
		}
		if( future->object ){
			DMutex_Lock( & server->mutex );
			DMap_Erase( server->active, future->object->rootObject );
			DMutex_Unlock( & server->mutex );
		}
		if( proc == NULL ) continue;
		DMutex_Lock( & server->mutex );
		DMap_Erase( server->active, proc );
		DMutex_Unlock( & server->mutex );

		type = future->unitype;
		type = type && type->nested->size ? type->nested->items.pType[0] : NULL;
		switch( proc->status ){
		case DAO_VMPROC_FINISHED :
		case DAO_VMPROC_ABORTED :
			DaoValue_Move( proc->stackValues[0], & future->value, type );
			future->state = DAO_CALL_FINISHED;
			break;
		case DAO_VMPROC_SUSPENDED : future->state = DAO_CALL_PAUSED; break;
		case DAO_VMPROC_RUNNING :
		case DAO_VMPROC_STACKED : future->state = DAO_CALL_RUNNING; break;
		default : break;
		}
		if( future->state == DAO_CALL_FINISHED ){
			GC_DecRC( proc->future );
			proc->future = NULL;
			DaoVmSpace_ReleaseProcess( server->vmspace, proc );
		}
		GC_DecRC( future );
	}
}
void DaoCallServer_Join( DaoVmSpace *vmSpace )
{
	DCondVar condv;
	if( daoCallServer == NULL ) return;
	DCondVar_Init( & condv );
	daoCallServer->finishing = 1;
	DMutex_Lock( & daoCallServer->mutex );
	while( daoCallServer->pending->size || daoCallServer->idle != daoCallServer->total ){
		/* printf( "finalizing: %3i %3i\n", daoCallServer->idle, daoCallServer->total ); */
		DCondVar_TimedWait( & condv, & daoCallServer->mutex, 0.01 );
	}
	DMutex_Unlock( & daoCallServer->mutex );
}

#endif
