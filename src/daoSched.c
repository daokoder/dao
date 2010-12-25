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


#include"stdio.h"
#include"time.h"
#include"daoType.h"
#include"daoThread.h"
#include"daoRoutine.h"
#include"daoObject.h"
#include"daoVmspace.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoGC.h"

#if( defined DAO_WITH_THREAD && defined DAO_WITH_SYNCLASS )

typedef struct DaoCallThread   DaoCallThread;
typedef struct DaoCallServer   DaoCallServer;

extern DaoVmProcess *mainVmProcess;

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
	return self;
}
int DaoCallServer_MapStop( DaoCallServer *self )
{
	if( self->finishing == 0 ) return 0; /* program not reach the end yet */
	if( self->total > self->idle ) return 0; /* active thread may invoke new calls */
	if( self->pending->size ) return 0; /* there are pending calls or joining */
	return 1;
}

DaoFuture* DaoCallServer_Add( DaoContext *ctx, DaoVmProcess *proc, DaoFuture *pre )
{
	DaoFuture *future = NULL;
#if 0
	printf( "DaoCallServer_Add( %12p, %12p, %12p )\n", ctx, proc, pre );
#endif
	if( ctx ){ /* new synchronous method call: */
		future = DaoFuture_New();
		future->context = ctx;
		future->process = proc; /* could be null */
		GC_IncRC( ctx );
		GC_IncRC( proc );
		future->state = DAO_CALL_QUEUED;
	}else if( proc ){
		/* joining the process with the future value's own process */
		if( proc->future == NULL ){
			proc->future = DaoFuture_New();
			proc->future->process = proc;
			GC_IncRC( proc );
		}
		future = proc->future;
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
		if( future->context && DMap_Find( active, future->context->object->that ) ) continue;
		DArray_Erase( pending, i, 1 );
		if( future->context ) DMap_Insert( active, future->context->object->that, NULL );
		return future;
	}
	return NULL;
}
void DaoCallThread_Run( DaoCallThread *self )
{
	DaoContext *ctx = NULL;
	DaoVmProcess *proc = NULL;
	DaoCallServer *server = daoCallServer;
	DaoFuture *future = NULL;
	DaoType *type = NULL;
	double wt = 0.001;
	int timeout;

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

		ctx = NULL;
		proc = NULL;
		if( future->state == DAO_CALL_QUEUED ){
			proc = future->process;
			ctx = future->context;
			if( proc == NULL ){
				proc = future->process = DaoVmProcess_New( ctx->vmSpace );
				GC_IncRC( proc );
			}
			DaoVmProcess_PushContext( proc, ctx );
			DaoVmProcess_Execute( proc );
		}else if( future->state == DAO_CALL_PAUSED ){
			DValue *pars[1] = { NULL };
			int npar = 0;
			if( future->precondition ){
				pars[0] = & future->precondition->value;
				npar = 1;
			}
			DaoVmProcess_Resume( future->process, pars, npar, NULL );
			proc = future->process;
			ctx = future->context;
		}
		if( ctx && ctx->object ){
			DMutex_Lock( & server->mutex );
			DMap_Erase( server->active, ctx->object->that );
			DMutex_Unlock( & server->mutex );
		}
		if( proc == NULL ) continue;
		type = future->unitype;
		type = type && type->nested->size ? type->nested->items.pType[0] : NULL;
		switch( proc->status ){
		case DAO_VMPROC_FINISHED :
		case DAO_VMPROC_ABORTED :
			future->state = DAO_CALL_FINISHED;
			DValue_Move( proc->returned, & future->value, type );
			break;
		case DAO_VMPROC_SUSPENDED : future->state = DAO_CALL_PAUSED; break;
		case DAO_VMPROC_RUNNING :
		case DAO_VMPROC_STACKED : future->state = DAO_CALL_RUNNING; break;
		default : break;
		}
		GC_DecRC( future );
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
