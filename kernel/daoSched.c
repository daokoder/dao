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

extern DaoProcess *mainProcess;

struct DaoCallThread
{
	DThread       thread;
	DThreadData  *thdData;
};

DaoCallThread* DaoCallThread_New();
void DaoCallThread_Run( DaoCallThread *self );

struct DaoCallServer
{
	DMutex   mutex;
	DCondVar condv;

	int finishing;
	int total;
	int idle;

	DArray  *pending; /* list of DaoFuture */
	DMap    *active; /* map of DaoObject keys */

	DaoVmSpace  *vmspace;
};
static DaoCallServer *daoCallServer;

DaoCallThread* DaoCallThread_New()
{
	DaoCallThread *self = (DaoCallThread*)dao_malloc( sizeof(DaoCallThread) );
	self->thdData = NULL;
	DThread_Init( & self->thread );
	return self;
}
DaoCallServer* DaoCallServer_New( DaoVmSpace *vms, int count )
{
	DaoCallServer *self = (DaoCallServer*)dao_malloc( sizeof(DaoCallServer) );
	DMutex_Init( & self->mutex );
	DCondVar_Init( & self->condv );
	self->finishing = 0;
	self->total = count;
	self->idle = 0;
	self->pending = DArray_New(0);
	self->active = DMap_New(0,0);
	self->vmspace = vms;
	return self;
}
int DaoCallServer_MapStop( DaoCallServer *self )
{
	if( self->finishing == 0 ) return 0; /* program not reach the end yet */
	if( self->total > self->idle ) return 0; /* active thread may invoke new calls */
	if( self->pending->size ) return 0; /* there are pending calls or joining */
	return 1;
}

DaoFuture* DaoCallServer_Add( DaoProcess *call, DaoProcess *wait, DaoFuture *pre )
{
	DaoFuture *future = NULL;
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
		DMutex_Lock( & daoCallServer->mutex );
		DArray_Append( daoCallServer->pending, future );
		GC_IncRC( future );
		DCondVar_Signal( & daoCallServer->condv );
		DMutex_Unlock( & daoCallServer->mutex );
	}
	return future;
}
DaoFuture* DaoFutures_GetFirstExecutable( DArray *pending, DMap *active )
{
	int i;
	DaoFuture *future = (DaoFuture*) pending->items.pVoid[0];
	DaoFuture *precond = future->precondition;
	DaoFuture *first = future;
	while( precond && (precond->state == DAO_CALL_QUEUED || precond->state == DAO_CALL_PAUSED) ){
		DArray_PopFront( pending );
		DArray_PushBack( pending, future );
		future = (DaoFuture*) pending->items.pVoid[0];
		precond = future->precondition;
		if( future == first ) break;
	}
	for(i=0; i<pending->size; i++){
		future = (DaoFuture*) pending->items.pVoid[i];
		if( future->precondition && future->precondition->state != DAO_CALL_FINISHED ) continue;
		if( future->object && DMap_Find( active, future->object->rootObject ) ) continue;
		DArray_Erase( pending, i, 1 );
		if( future->object ) DMap_Insert( active, future->object->rootObject, NULL );
		return future;
	}
	return NULL;
}
void DaoCallThread_Run( DaoCallThread *self )
{
	DaoCallServer *server = daoCallServer;
	DaoProcess *proc = NULL;
	DaoFuture *future = NULL;
	DaoType *type = NULL;
	double wt = 0.001;
	int i, timeout;

	self->thdData = DThread_GetSpecific();
	while(1){
		self->thdData->state = 0;
		DMutex_Lock( & server->mutex );
		server->idle += 1;
		while( server->pending->size == 0 ){
			if( server->finishing && server->idle == server->total ) break;
			timeout = DCondVar_TimedWait( & server->condv, & server->mutex, wt );
		}
		DMutex_Unlock( & server->mutex );
		if( server->pending->size == 0 && server->finishing && server->idle == server->total ) break;
		if( server->pending->size == 0 ) continue;

		DMutex_Lock( & server->mutex );
		server->idle -= 1;
		future = DaoFutures_GetFirstExecutable( server->pending, server->active );
		DMutex_Unlock( & server->mutex );

		if( future == NULL ) continue;
		//printf( "future = %p %i: %i\n", future, server->pending->size, future->state );

		proc = NULL;
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
		type = future->unitype;
		type = type && type->nested->size ? type->nested->items.pType[0] : NULL;
		switch( proc->status ){
		case DAO_VMPROC_FINISHED :
		case DAO_VMPROC_ABORTED :
			future->state = DAO_CALL_FINISHED;
			DaoValue_Move( proc->stackValues[0], & future->value, type );
			break;
		case DAO_VMPROC_SUSPENDED : future->state = DAO_CALL_PAUSED; break;
		case DAO_VMPROC_RUNNING :
		case DAO_VMPROC_STACKED : future->state = DAO_CALL_RUNNING; break;
		default : break;
		}
		GC_DecRC( future );
		if( future->state == DAO_CALL_FINISHED ){
			GC_DecRC( proc->future );
			proc->future = NULL;
			DaoVmSpace_ReleaseProcess( server->vmspace, proc );
		}
	}
}
void DaoCallServer_Init( DaoVmSpace *vms )
{
	int i, N = 2;
	daoCallServer = DaoCallServer_New( vms, N );
	for(i=0; i<N; i++){
		DaoCallThread *calth = DaoCallThread_New();
		DThread_Start( & calth->thread, (DThreadTask) DaoCallThread_Run, calth );
	}
}
void DaoCallServer_Join( DaoVmSpace *vmSpace )
{
	DCondVar condv;
	DCondVar_Init( & condv );
	daoCallServer->finishing = 1;
	DMutex_Lock( & daoCallServer->mutex );
	while( daoCallServer->pending->size || daoCallServer->idle != daoCallServer->total ){
		DCondVar_TimedWait( & condv, & daoCallServer->mutex, 0.01 );
	}
	DMutex_Unlock( & daoCallServer->mutex );
}

#endif
