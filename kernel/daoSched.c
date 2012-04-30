/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2012, Limin Fu
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, this list
//   of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright notice, this list
//   of conditions and the following disclaimer in the documentation and/or other materials
//   provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
// TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include"stdio.h"
#include"time.h"
#include"daoType.h"
#include"daoValue.h"
#include"daoThread.h"
#include"daoRoutine.h"
#include"daoObject.h"
#include"daoVmspace.h"
#include"daoProcess.h"
#include"daoGC.h"

#ifdef DAO_WITH_CONCURRENT

typedef struct DaoCallThread   DaoCallThread;
typedef struct DaoCallServer   DaoCallServer;


struct DaoCallThread
{
	DThread       thread;
	DThreadData  *thdData;
};

static DaoCallThread* DaoCallThread_New();
static void DaoCallThread_Run( DaoCallThread *self );

struct DaoCallServer
{
	DThread  timer;
	DMutex   mutex;
	DCondVar condv;
	DCondVar condv2;

	int finishing;
	int timing;
	int total;
	int idle;
	int stopped;

	DArray  *threads;

	DArray  *functions; /* list of DThreadTask function pointers */
	DArray  *parameters; /* list of void* */
	DArray  *futures; /* list of DaoFuture* */
	DArray  *timeouts; /* list of timeout waitings */
	DMap    *waitings; /* timed waiting: <DaoDouble,DaoFuture*> */
	DMap    *active; /* map of DaoObject* or DaoProcess* keys */
	DMap    *pending; /* map of pointers from ::parameters and ::futures */

	DaoTuple    *tuple;
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
static void DaoCallThread_Delete( DaoCallThread *self )
{
	// XXX self->thdData
	DThread_Destroy( & self->thread );
	dao_free( self );
}
static DaoCallServer* DaoCallServer_New( DaoVmSpace *vms )
{
	DaoCallServer *self = (DaoCallServer*)dao_malloc( sizeof(DaoCallServer) );
	DMutex_Init( & self->mutex );
	DCondVar_Init( & self->condv );
	DCondVar_Init( & self->condv2 );
	DThread_Init( & self->timer );
	self->finishing = 0;
	self->timing = 0;
	self->total = 0;
	self->idle = 0;
	self->stopped = 0;
	self->threads = DArray_New(0);
	self->functions = DArray_New(0);
	self->parameters = DArray_New(0);
	self->futures = DArray_New(0);
	self->timeouts = DArray_New(0);
	self->waitings = DMap_New(D_VALUE,0);
	self->pending = DHash_New(0,0);
	self->active = DHash_New(0,0);
	self->vmspace = vms;
	self->tuple = DaoTuple_New(2);
	self->tuple->items[0] = (DaoValue*) DaoDouble_New(0);
	self->tuple->items[1] = (DaoValue*) DaoDouble_New(0);
	DaoValue_MarkConst( (DaoValue*) self->tuple );
	GC_IncRC( self->tuple->items[0] );
	GC_IncRC( self->tuple->items[1] );
	GC_IncRC( self->tuple );
	return self;
}
static void DaoCallServer_Delete( DaoCallServer *self )
{
	size_t i, n = self->threads->size;
	for(i=0; i<n; i++) DaoCallThread_Delete( (DaoCallThread*)self->threads->items.pVoid[i] );
	GC_DecRC( self->tuple );
	DArray_Delete( self->threads );
	DArray_Delete( self->functions );
	DArray_Delete( self->parameters );
	DArray_Delete( self->futures );
	DArray_Delete( self->timeouts );
	DMap_Delete( self->waitings );
	DMap_Delete( self->pending );
	DMap_Delete( self->active );
	DMutex_Destroy( & self->mutex );
	DCondVar_Destroy( & self->condv );
	DCondVar_Destroy( & self->condv2 );
	DThread_Destroy( & self->timer );
	dao_free( self );
}
static void DaoCallServer_AddThread()
{
	DaoCallThread *calth = DaoCallThread_New();
	DMutex_Lock( & daoCallServer->mutex );
	daoCallServer->total += 1;
	DArray_Append( daoCallServer->threads, calth );
	DMutex_Unlock( & daoCallServer->mutex );
	DThread_Start( & calth->thread, (DThreadTask) DaoCallThread_Run, calth );
}
static double DaoGetCurrentTime()
{
#ifdef WIN32
	return timeGetTime();
#else
	struct timeval tv;
	gettimeofday( & tv, NULL);
	return tv.tv_sec + (double)tv.tv_usec * 1.0E-9;
#endif
}
static void DaoCallServer_Timer( void *p )
{
	DaoCallServer *server = daoCallServer;
	double time = 0.0;
	int i, timeout;

	server->timing = 1;
	while( server->finishing == 0 || server->stopped != server->total ){
		DMutex_Lock( & server->mutex );
		while( server->waitings->size == 0 && server->timeouts->size == 0 ){
			if( server->finishing && server->stopped == server->total ) break;
			DCondVar_TimedWait( & server->condv2, & server->mutex, 0.01 );
		}
		if( server->waitings->size ){
			DNode *node = DMap_First( server->waitings );
			time = node->key.pValue->xTuple.items[0]->xDouble.value;
			time -= DaoGetCurrentTime();
			/* wait the right amount of time for the closest arriving timeout: */
			if( time > 0 ) DCondVar_TimedWait( & server->condv2, & server->mutex, time );
		}else if( server->timeouts->size ){
			/* there is at least one pending timeout to be handled,
			 * wait for some time before creating a new thread to handle it: */
			DCondVar_TimedWait( & server->condv2, & server->mutex, 0.1 );
		}
		DMutex_Unlock( & server->mutex );
		if( server->finishing && server->stopped == server->total ) break;

		/* create new thread for unhandled timeout: */
		if( server->timeouts->size ) DaoCallServer_AddThread();

		DMutex_Lock( & server->mutex );
		if( server->waitings->size ){ /* a new wait timed out: */
			DNode *node = DMap_First( server->waitings );
			time = DaoGetCurrentTime();
			if( node->key.pValue->xTuple.items[0]->xDouble.value < time ){
				DArray_Append( server->timeouts, node->value.pVoid );
				DMap_EraseNode( server->waitings, node );
			}
		}
		DCondVar_Signal( & server->condv );
		DMutex_Unlock( & server->mutex );
	}
	server->timing = 0;
}
static void DaoCallServer_Init( DaoVmSpace *vms )
{
	int i;
	DaoCGC_Start();
	daoCallServer = DaoCallServer_New( vms );
	DThread_Start( & daoCallServer->timer, (DThreadTask) DaoCallServer_Timer, NULL );
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
static void DaoCallServer_Add( DaoFuture *future )
{
	DaoCallServer *server;
	if( daoCallServer == NULL ) DaoCallServer_Init( mainVmSpace );
	server = daoCallServer;
	if( server->pending->size > server->total * server->total ) DaoCallServer_AddThread();
	DMutex_Lock( & server->mutex );
	DArray_Append( server->futures, future );
	DMap_Insert( server->pending, future, NULL );
	GC_IncRC( future );
	DCondVar_Signal( & server->condv );
	DMutex_Unlock( & server->mutex );
}
DaoFuture* DaoCallServer_AddCall( DaoProcess *call )
{
	DaoFuture *future = DaoFuture_New();
	DaoValue **params = call->stackValues + call->topFrame->stackBase;
	int i, count = call->topFrame->parCount;

	for(i=0; i<count; i++) DaoValue_Copy( params[i], & future->params[i] );
	future->parCount = count;
	future->state = DAO_CALL_QUEUED;
	future->object = call->topFrame->object;
	future->routine = call->topFrame->routine;
	GC_IncRC( future->routine );
	GC_IncRC( future->object );
	DaoCallServer_Add( future );
	return future;
}
void DaoCallServer_AddWait( DaoProcess *wait, DaoFuture *pre, double timeout, short state )
{
	DaoFuture *future;
	DaoCallServer *server;
	if( daoCallServer == NULL ) DaoCallServer_Init( mainVmSpace );
	server = daoCallServer;

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
	future->state2 = state;

	DMutex_Lock( & server->mutex );
	if( timeout >0 ){
		server->tuple->items[0]->xDouble.value = timeout + DaoGetCurrentTime();
		server->tuple->items[1]->xDouble.value += 1;
		DMap_Insert( server->waitings, server->tuple, future );
		DMap_Insert( server->pending, future, NULL );
		GC_IncRC( future );
		DCondVar_Signal( & server->condv2 );
	}else{
		//DaoCallServer_Add( future );
		DArray_Append( server->futures, future );
		DMap_Insert( server->pending, future, NULL );
		GC_IncRC( future );
		DCondVar_Signal( & server->condv );
	}
	if( wait->condv ){
		/*
		// Need to suspend the native thread, for suspending inside code sections
		// for functional methods such as std.iterate(), mt.iterate() etc:
		*/
		wait->pauseType = DAO_VMP_NATIVE_SUSPENSION;
		if( timeout > 0 ){
			DCondVar_TimedWait( wait->condv, & server->mutex, timeout );
		}else{
			DCondVar_Wait( wait->condv, & server->mutex );
		}
		wait->status = DAO_VMPROC_RUNNING;
	}
	DMutex_Unlock( & server->mutex );
}
static DaoFuture* DaoCallServer_GetNextFuture()
{
	DaoCallServer *server = daoCallServer;
	DaoFuture *first, *future, *precond;
	DArray *timeouts = server->timeouts;
	DArray *futures = server->futures;
	DMap *pending = server->pending;
	DMap *active = server->active;
	int i;

	for(i=0; i<timeouts->size; i++){
		future = (DaoFuture*) timeouts->items.pVoid[i];
		if( future->object && DMap_Find( active, future->object->rootObject ) ) continue;
		if( future->process && DMap_Find( active, future->process ) ) continue;
		DArray_Erase( timeouts, i, 1 );
		DMap_Erase( pending, future );
		if( future->object ) DMap_Insert( active, future->object->rootObject, NULL );
		if( future->process ) DMap_Insert( active, future->process, NULL );
		return future;
	}
	if( futures->size == 0 ) return NULL;
	future = (DaoFuture*) futures->items.pVoid[0];
	precond = future->precondition;
	first = future;
	while( precond && precond->state != DAO_CALL_FINISHED ){
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
	DArray *array = DArray_New(0);
	double wt = 0.001;
	int i, timeout;

	self->thdData = DThread_GetSpecific();
	while(1){
		DaoProcess *proc = NULL;
		DaoProcess *proc2 = NULL;
		DaoFuture *future = NULL;
		DThreadTask function = NULL;
		void *parameter = NULL;

		self->thdData->state = 0;
		DMutex_Lock( & server->mutex );
		server->idle += 1;
		while( server->pending->size == server->waitings->size ){
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
			future = DaoCallServer_GetNextFuture();
		}
		DMutex_Unlock( & server->mutex );

		if( parameter ) (*function)( parameter );
		if( future == NULL ) continue;

		if( future->state == DAO_CALL_QUEUED ){
			int n = future->parCount;
			if( future->process == NULL ){
				proc2 = future->process = DaoVmSpace_AcquireProcess( server->vmspace );
				GC_IncRC( future->process );
			}
			proc = future->process;
			for(i=0; i<n; i++) DaoValue_Copy( future->params[i], & proc->freeValues[i] );
			future->parCount = 0;
			DaoValue_ClearAll( future->params, n );
			DaoProcess_PushRoutine( proc, future->routine, future->object );
			proc->topFrame->parCount = n;
			future->state = DAO_CALL_RUNNING;
			DaoProcess_Execute( proc );
		}else if( future->state == DAO_CALL_PAUSED ){
			if( future->process->pauseType == DAO_VMP_NATIVE_SUSPENSION ){
				DMutex_Lock( & server->mutex );
				future->process->pauseType = 0;
				DCondVar_Signal( future->process->condv );
				DMutex_Unlock( & server->mutex );
			}else{
				future->state = DAO_CALL_RUNNING;
				DaoProcess_Execute( future->process );
			}
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

		DaoProcess_ReturnFutureValue( proc, future );
		if( future->state == DAO_CALL_FINISHED ){
			DNode *node;
			array->size = 0;
			DMutex_Lock( & server->mutex );
			for(node=DMap_First(server->waitings); node; node=DMap_Next(server->waitings,node)){
				DaoFuture *fut = (DaoFuture*) node->value.pValue;
				if( fut->precondition == future ){ /* remove from timed waiting list: */
					DArray_Append( server->timeouts, fut );
					DArray_Append( array, node->key.pVoid );
				}
			}
			for(i=0; i<array->size; i++) DMap_Erase( server->waitings, array->items.pVoid[i] );
			DCondVar_Signal( & server->condv2 );
			DMutex_Unlock( & server->mutex );
			if( proc2 ) DaoVmSpace_ReleaseProcess( server->vmspace, proc2 );
		}
		GC_DecRC( future );
	}
	DArray_Delete( array );
	DMutex_Lock( & server->mutex );
	server->stopped += 1;
	DMutex_Unlock( & server->mutex );
}
void DaoCallServer_Join()
{
	DCondVar condv;
	if( daoCallServer == NULL ) return;
	DCondVar_Init( & condv );
	DMutex_Lock( & daoCallServer->mutex );
	while( daoCallServer->pending->size || daoCallServer->idle != daoCallServer->total ){
		DCondVar_TimedWait( & condv, & daoCallServer->mutex, 0.01 );
	}
	DMutex_Unlock( & daoCallServer->mutex );
	DCondVar_Destroy( & condv );
}
void DaoCallServer_Stop()
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
	while( daoCallServer->stopped != daoCallServer->total || daoCallServer->timing ){
		DCondVar_TimedWait( & condv, & daoCallServer->mutex, 0.01 );
	}
	DMutex_Unlock( & daoCallServer->mutex );
	DCondVar_Destroy( & condv );
	DaoCallServer_Delete( daoCallServer );
	daoCallServer = NULL;
}

#endif
