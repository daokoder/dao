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


#include"stdio.h"
#include"string.h"
#include"time.h"
#include"math.h"
#include"assert.h"
#include"daoType.h"
#include"daoValue.h"
#include"daoThread.h"
#include"daoRoutine.h"
#include"daoObject.h"
#include"daoVmspace.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoTasklet.h"

#ifdef DAO_WITH_CONCURRENT


DaoTaskEvent* DaoTaskEvent_New()
{
	DaoTaskEvent *self = (DaoTaskEvent*) dao_calloc( 1, sizeof(DaoTaskEvent) );
	return self;
}
void DaoTaskEvent_Reset( DaoTaskEvent *self )
{
	self->type = 0;
	self->state = 0;
	GC_DecRC( self->future );
	GC_DecRC( self->channel );
	GC_DecRC( self->value );
	self->future = NULL;
	self->channel = NULL;
	self->value = NULL;
}
void DaoTaskEvent_Delete( DaoTaskEvent *self )
{
	DaoTaskEvent_Reset( self );
	dao_free( self );
}
void DaoTaskEvent_Init( DaoTaskEvent *self, int T, int S, DaoFuture *F, DaoChannel *C )
{
	GC_ShiftRC( F, self->future );
	GC_ShiftRC( C, self->channel );
	self->type = T;
	self->state = S;
	self->future = F;
	self->channel = C;
}



DaoChannel* DaoChannel_New( DaoType *type, int dtype )
{
	DaoChannel *self = (DaoChannel*) dao_calloc( 1, sizeof(DaoChannel) );
	if( dtype ) type = DaoCdataType_Specialize( dao_type_channel, & type, type != NULL );
	DaoCstruct_Init( (DaoCstruct*) self, type );
	self->buffer = DArray_New(D_VALUE);
	return self;
}




DaoFuture* DaoFuture_New( DaoType *type, int vatype )
{
	DaoFuture *self = (DaoFuture*) dao_calloc( 1, sizeof(DaoFuture) );
	if( vatype ) type = DaoCdataType_Specialize( dao_type_future, & type, type != NULL );
	DaoCstruct_Init( (DaoCstruct*) self, type );
	GC_IncRC( dao_none_value );
	self->state = DAO_CALL_PAUSED;
	self->restype = DAO_FUTRES_NONE;
	self->value = dao_none_value;
	return self;
}





typedef struct DaoCallThread   DaoCallThread;
typedef struct DaoCallServer   DaoCallServer;


struct DaoCallThread
{
	DThread       thread;
	DThreadData  *thdData;

	DThreadTask   taskFunc;  /* first task; */
	void         *taskParam;
};

static DaoCallThread* DaoCallThread_New( DThreadTask func, void *param );
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
	DArray  *events;   /* list of DaoTaskEvent* */
	DMap    *waitings; /* timed waiting: <DaoComplex,DaoTaskEvent*> */
	DMap    *active; /* map of DaoObject* or DaoProcess* keys */
	DMap    *pending; /* map of pointers from ::parameters and ::events */

	DArray  *caches;

	DaoComplex   timestamp;  /* (time,index); */
	DaoVmSpace  *vmspace;
};
static DaoCallServer *daoCallServer = NULL;

static DaoCallThread* DaoCallThread_New( DThreadTask func, void *param )
{
	DaoCallThread *self = (DaoCallThread*)dao_malloc( sizeof(DaoCallThread) );
	self->thdData = NULL;
	self->taskFunc = func;
	self->taskParam = param;
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
	DaoComplex com = {DAO_COMPLEX,0,0,0,1,{0.0,0.0}};
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
	self->events = DArray_New(0);
	self->waitings = DMap_New(D_VALUE,0);
	self->pending = DHash_New(0,0);
	self->active = DHash_New(0,0);
	self->caches = DArray_New(0);
	self->vmspace = vms;
	self->timestamp = com;
	return self;
}
static void DaoCallServer_Delete( DaoCallServer *self )
{
	daoint i;
	for(i=0; i<self->threads->size; i++){
		DaoCallThread_Delete( (DaoCallThread*)self->threads->items.pVoid[i] );
	}
	for(i=0; i<self->caches->size; ++i){
		DaoTaskEvent_Delete( (DaoTaskEvent*) self->caches->items.pVoid[i] );
	}
	DArray_Delete( self->threads );
	DArray_Delete( self->functions );
	DArray_Delete( self->parameters );
	DArray_Delete( self->events );
	DMap_Delete( self->waitings );
	DMap_Delete( self->pending );
	DMap_Delete( self->active );
	DMutex_Destroy( & self->mutex );
	DCondVar_Destroy( & self->condv );
	DCondVar_Destroy( & self->condv2 );
	DThread_Destroy( & self->timer );
	dao_free( self );
}

static void DaoCallServer_Timer( void *p );

static void DaoCallServer_Init( DaoVmSpace *vms )
{
	DaoCGC_Start();
	daoCallServer = DaoCallServer_New( vms );
	DThread_Start( & daoCallServer->timer, (DThreadTask) DaoCallServer_Timer, NULL );
}

static DaoTaskEvent* DaoCallServer_MakeEvent()
{
	DaoTaskEvent *event;
	DaoCallServer *server;
	if( daoCallServer == NULL ) DaoCallServer_Init( mainVmSpace );
	server = daoCallServer;
	DMutex_Lock( & server->mutex );
	event = (DaoTaskEvent*) DArray_PopBack( server->caches );
	if( event == NULL ) event = DaoTaskEvent_New();
	DMutex_Unlock( & server->mutex );
	return event;
}
/* Lock daoCallServer::mutex before calling this function: */
static void DaoCallServer_CacheEvent( DaoTaskEvent *event )
{
	DaoCallServer *server = daoCallServer;
	DaoTaskEvent_Reset( event );
	DArray_PushBack( server->caches, event );
}
static void DaoCallServer_AddThread( DThreadTask func, void *param )
{
	DaoCallThread *calth;
	if( daoCallServer == NULL ) DaoCallServer_Init( mainVmSpace );
	calth = DaoCallThread_New( func, param );
	DMutex_Lock( & daoCallServer->mutex );
	daoCallServer->total += 1;
	DArray_Append( daoCallServer->threads, calth );
	DMutex_Unlock( & daoCallServer->mutex );
	DThread_Start( & calth->thread, (DThreadTask) DaoCallThread_Run, calth );
}
static void DaoCallServer_TryAddThread( DThreadTask func, void *param, int todo )
{
	int max = daoConfig.cpu > 2 ? daoConfig.cpu : 2;
	int total = daoCallServer->total;
	if( total < 2 ) DaoCallServer_AddThread( NULL, NULL );
	if( todo == 0 ) return;
	if( total < max || todo > pow( total + max + 10, 5 ) ) DaoCallServer_AddThread( NULL, NULL );
}
static void DaoCallServer_Timer( void *p )
{
	DaoCallServer *server = daoCallServer;
	double time = 0.0;
	int i, timeout;

	server->timing = 1;
	while( server->finishing == 0 || server->stopped != server->total ){
		DMutex_Lock( & server->mutex );
		while( server->waitings->size == 0 ){
			if( server->finishing && server->stopped == server->total ) break;
			DCondVar_TimedWait( & server->condv2, & server->mutex, 0.01 );
		}
		if( server->waitings->size ){
			DNode *node = DMap_First( server->waitings );
			time = node->key.pValue->xComplex.value.real;
			time -= Dao_GetCurrentTime();
			/* wait the right amount of time for the closest arriving timeout: */
			if( time > 0 ) DCondVar_TimedWait( & server->condv2, & server->mutex, time );
		}
		DMutex_Unlock( & server->mutex );
		if( server->finishing && server->stopped == server->total ) break;

		DMutex_Lock( & server->mutex );
		if( server->waitings->size ){ /* a new wait timed out: */
			DNode *node = DMap_First( server->waitings );
			time = Dao_GetCurrentTime();
			if( node->key.pValue->xComplex.value.real < time ){
				DaoTaskEvent *event = (DaoTaskEvent*) node->value.pVoid;
				event->state = DAO_EVENT_RESUME;
				DArray_Append( server->events, node->value.pVoid );
				DMap_EraseNode( server->waitings, node );
			}
		}
		DCondVar_Signal( & server->condv );
		DMutex_Unlock( & server->mutex );
	}
	server->timing = 0;
}

void DaoCallServer_AddTask( DThreadTask func, void *param, int now )
{
	int scheduled = 0;
	DaoCallServer *server;
	if( daoCallServer == NULL ) DaoCallServer_Init( mainVmSpace );
	server = daoCallServer;
	DMutex_Lock( & server->mutex );
	if( server->idle > server->parameters->size || now == 0 ){
		scheduled = 1;
		DArray_Append( server->functions, func );
		DArray_Append( server->parameters, param );
		DMap_Insert( server->pending, param, NULL );
		DCondVar_Signal( & server->condv );
	}
	DMutex_Unlock( & server->mutex );
	if( scheduled ){
		if( now == 0 ) DaoCallServer_TryAddThread( NULL, NULL, server->parameters->size );
	}else{
		DaoCallServer_AddThread( func, param );
	}
}
static void DaoCallServer_AddEvent( DaoTaskEvent *event )
{
	DaoCallServer *server = daoCallServer;
	DArray_Append( server->events, event );
	DMap_Insert( server->pending, event, NULL );
}
static void DaoCallServer_Add( DaoTaskEvent *event )
{
	DaoCallServer *server;
	if( daoCallServer == NULL ) DaoCallServer_Init( mainVmSpace );
	server = daoCallServer;
	DMutex_Lock( & server->mutex );
	DaoCallServer_AddEvent( event );
	DCondVar_Signal( & server->condv );
	DMutex_Unlock( & server->mutex );
	DaoCallServer_TryAddThread( NULL, NULL, server->pending->size );
}
void DaoCallServer_AddCall( DaoProcess *caller )
{
	DaoProcess *callee = DaoVmSpace_AcquireProcess( caller->vmSpace );
	DaoStackFrame *frame = caller->topFrame;
	DaoTaskEvent *event = DaoCallServer_MakeEvent();
	DaoType *type = (DaoType*) frame->routine->routType->aux;
	DaoFuture *future = DaoFuture_New( type, 1 );
	DaoValue **params = caller->stackValues + caller->topFrame->stackBase;
	int i, count = caller->topFrame->parCount;

	future->state = DAO_CALL_PAUSED;
	future->actor = caller->topFrame->object;
	GC_IncRC( future->actor );

	GC_ShiftRC( future, callee->future );
	callee->future = future;
	future->process = callee;
	GC_IncRC( future->process );

	callee->parCount = count;
	for(i=0; i<count; ++i) DaoValue_Copy( params[i], & callee->paramValues[i] );
	DaoProcess_PushRoutine( callee, caller->topFrame->routine, future->actor );

	DaoTaskEvent_Init( event, DAO_EVENT_RESUME_TASKLET, DAO_EVENT_RESUME, future, NULL );

	DaoProcess_PopFrame( caller );
	DaoProcess_PutValue( caller, (DaoValue*) future );

	DaoCallServer_Add( event );
}
DaoFuture* DaoProcess_GetInitFuture( DaoProcess *self )
{
	if( self->future ) return self->future;

	self->future = DaoFuture_New( NULL, 1 );
	self->future->process = self;
	GC_IncRC( self->future );
	GC_IncRC( self );
	return self->future;
}
void DaoCallServer_AddTimedWait( DaoProcess *wait, DaoTaskEvent *event, double timeout )
{
	DaoCallServer *server;
	if( daoCallServer == NULL ) DaoCallServer_Init( mainVmSpace );
	server = daoCallServer;

	DMutex_Lock( & server->mutex );
	if( timeout >0 ){
		server->timestamp.value.real = timeout + Dao_GetCurrentTime();
		server->timestamp.value.imag += 1;
		DMap_Insert( server->waitings, & server->timestamp, event );
		DMap_Insert( server->pending, event, NULL );
		DCondVar_Signal( & server->condv2 );
	}else{
		DaoCallServer_AddEvent( event );
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
void DaoCallServer_AddWait( DaoProcess *wait, DaoFuture *pre, double timeout, int restype )
{
	DaoFuture *future;
	DaoTaskEvent *event;
	DaoCallServer *server;
	if( daoCallServer == NULL ) DaoCallServer_Init( mainVmSpace );
	server = daoCallServer;

	future = DaoProcess_GetInitFuture( wait );
	GC_ShiftRC( pre, future->precond );
	future->precond = pre;
	future->state = DAO_CALL_PAUSED;
	future->restype = restype;

	event = DaoCallServer_MakeEvent();
	DaoTaskEvent_Init( event, DAO_EVENT_WAIT_TASKLET, DAO_EVENT_WAIT, future, NULL );

	DaoCallServer_AddTimedWait( wait, event, timeout );
}
static void DaoCallServer_UpdateTimedWaits( DaoFuture *future, DaoChannel *channel )
{
	DaoCallServer *server = daoCallServer;
	DArray *array = DArray_New(0);
	DNode *node;
	daoint i;

	DMutex_Lock( & server->mutex );
	for(node=DMap_First(server->waitings); node; node=DMap_Next(server->waitings,node)){
		DaoTaskEvent *event = (DaoTaskEvent*) node->value.pValue;
		int move = 0;
		switch( event->type ){
		case DAO_EVENT_WAIT_TASKLET :
			move = event->future->precond == future;
			break;
		case DAO_EVENT_WAIT_RECEIVING :
			move = event->channel == channel && channel->buffer->size > 0;
			break;
		case DAO_EVENT_WAIT_SENDING :
			move = event->channel == channel;
			break;
		default: break;
		}
		/* remove from timed waiting list: */
		if( move ){
			event->state = DAO_EVENT_RESUME;
			DArray_Append( server->events, event );
			DArray_Append( array, node->key.pVoid );
		}
	}
	for(i=0; i<array->size; i++) DMap_Erase( server->waitings, array->items.pVoid[i] );
	DCondVar_Signal( & server->condv2 );
	DMutex_Unlock( & server->mutex );
	DArray_Delete( array );
}
static DaoFuture* DaoCallServer_GetNextFuture()
{
	DaoCallServer *server = daoCallServer;
	DaoFuture *first, *future, *precond;
	DArray *events = server->events;
	DMap *pending = server->pending;
	DMap *active = server->active;
	daoint i;

	for(i=0; i<events->size; i++){
		DaoTaskEvent *event = (DaoTaskEvent*) events->items.pVoid[i];
		DaoFuture *future = event->future;
		DaoValue *message = NULL;

		switch( event->type ){
		case DAO_EVENT_WAIT_SENDING :
			if( event->channel->buffer->size >= event->channel->cap ){
				if( event->state == DAO_EVENT_WAIT ) continue;
			}
			break;
		case DAO_EVENT_WAIT_RECEIVING :
			if( event->channel->buffer->size == 0 ){
				if( event->state == DAO_EVENT_WAIT ) continue;
				message = dao_none_value;
			}else{
				message = event->channel->buffer->items.pValue[0];
			}
			GC_ShiftRC( message, event->future->message );
			event->future->message = message;
			event->future->restype = DAO_FUTRES_VALUE;
			DArray_PopFront( event->channel->buffer );
			break;
		default: break;
		}
		if( event->state == DAO_EVENT_WAIT ){
			if( future->precond && future->precond->state != DAO_CALL_FINISHED ) continue;
		}
		if( future->actor && DMap_Find( active, future->actor->rootObject ) ) continue;
		if( future->process && DMap_Find( active, future->process ) ) continue;
		DArray_Erase( events, i, 1 );
		DMap_Erase( pending, event );
		if( future->actor ) DMap_Insert( active, future->actor->rootObject, NULL );
		if( future->process ) DMap_Insert( active, future->process, NULL );
		GC_IncRC( future ); /* To be decreased at the end of tasklet; */
		DaoCallServer_CacheEvent( event );
		return future;
	}
	return NULL;
}
static void DaoCallThread_Run( DaoCallThread *self )
{
	DaoCallServer *server = daoCallServer;
	double wt = 0.001;
	int i, timeout;

	self->thdData = DThread_GetSpecific();
	if( self->taskFunc ) self->taskFunc( self->taskParam );
	while(1){
		DaoProcess *process = NULL;
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
		if( server->parameters->size ){
			function = (DThreadTask) DArray_Front( server->functions );
			parameter = DArray_Front( server->parameters );
			DArray_PopFront( server->functions );
			DArray_PopFront( server->parameters );
			DMap_Erase( server->pending, parameter );
			server->idle -= 1;
		}
		DMutex_Unlock( & server->mutex );

		if( function ){
			(*function)( parameter );
			continue;
		}

		if( server->pending->size == 0 && server->finishing && server->idle == server->total ) break;

		DMutex_Lock( & server->mutex );
		server->idle -= 1;
		future = DaoCallServer_GetNextFuture();
		DMutex_Unlock( & server->mutex );

		if( future == NULL ) continue;

		process = future->process;
		if( process == NULL ){
			GC_DecRC( future );
			continue;
		}
		if( process->pauseType == DAO_VMP_NATIVE_SUSPENSION ){
			DMutex_Lock( & server->mutex );
			process->pauseType = 0;
			DCondVar_Signal( process->condv );
			DMutex_Unlock( & server->mutex );
		}else{
			int count = process->exceptions->size;
			future->state = DAO_CALL_RUNNING;
			DaoProcess_InterceptReturnValue( process );
			DaoProcess_Execute( process );
			if( process->exceptions->size > count ) DaoProcess_PrintException( process, 1 );
		}

		if( future->actor ){
			DMutex_Lock( & server->mutex );
			DMap_Erase( server->active, future->actor->rootObject );
			DMutex_Unlock( & server->mutex );
		}
		DMutex_Lock( & server->mutex );
		DMap_Erase( server->active, process );
		DMutex_Unlock( & server->mutex );

		DaoProcess_ReturnFutureValue( process, future );
		if( future->state == DAO_CALL_FINISHED ) DaoCallServer_UpdateTimedWaits( future, NULL );
		GC_DecRC( future );
	}
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





static void CHANNEL_New( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoType *retype = DaoProcess_GetReturnType( proc );
	DaoChannel *self = DaoChannel_New( retype, 0 );
	self->cap = par[0]->xInteger.value;
	DaoProcess_PutValue( proc, (DaoValue*) self );
	if( daoCallServer == NULL ) DaoCallServer_Init( mainVmSpace );
}
static void CHANNEL_Send( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoFuture *future = DaoProcess_GetInitFuture( proc );
	DaoChannel *self = (DaoChannel*) par[0];
	float timeout = par[2]->xFloat.value;

	//printf( "CHANNEL_Send: %p\n", event );
	DMutex_Lock( & daoCallServer->mutex );
	DArray_Append( self->buffer, par[1] );
	DCondVar_Signal( & daoCallServer->condv );
	DMutex_Unlock( & daoCallServer->mutex );
	DaoCallServer_UpdateTimedWaits( NULL, self );

	if( self->buffer->size > self->cap ){
		DaoTaskEvent *event = DaoCallServer_MakeEvent();
		DaoTaskEvent_Init( event, DAO_EVENT_WAIT_SENDING, DAO_EVENT_WAIT, future, self );
		DaoCallServer_AddTimedWait( proc, event, timeout );
		future->restype = DAO_FUTRES_STATUS;
		proc->status = DAO_VMPROC_SUSPENDED;
		proc->pauseType = DAO_VMP_ASYNC;
	}
}
static void CHANNEL_Receive( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoTaskEvent *event = NULL;
	DaoFuture *future = DaoProcess_GetInitFuture( proc );
	DaoChannel *self = (DaoChannel*) par[0];
	float timeout = par[1]->xFloat.value;

	event = DaoCallServer_MakeEvent();
	DaoTaskEvent_Init( event, DAO_EVENT_WAIT_RECEIVING, DAO_EVENT_WAIT, future, self );
	DaoCallServer_AddTimedWait( proc, event, timeout );
	proc->status = DAO_VMPROC_SUSPENDED;
	proc->pauseType = DAO_VMP_ASYNC;
	/* Message may have been sent before this call: */
	if( self->buffer->size ) DaoCallServer_UpdateTimedWaits( NULL, self );
}
static DaoFuncItem channelMeths[] =
{
	{ CHANNEL_New,      "channel<@V>( cap = 1 )" },
	{ CHANNEL_Send,     "send( self :channel<@V>, data :@V, timeout :float = 0 ) => int" },
	{ CHANNEL_Receive,  "receive( self :channel<@V>, timeout :float = -1 ) => @V|none" },
	{ NULL, NULL }
};
static void DaoChannel_Delete( DaoChannel *self )
{
	DaoCstruct_Free( (DaoCstruct*) self );
	DArray_Delete( self->buffer );
	dao_free( self );
}
static void DaoChannel_GetGCFields( void *p, DArray *vs, DArray *arrays, DArray *ms, int rm )
{
	DaoChannel *self = (DaoChannel*) p;
	DArray_Append( arrays, self->buffer );
}

DaoTypeBase channelTyper =
{
	"channel<@V=none>", NULL, NULL, (DaoFuncItem*) channelMeths, {0}, {0},
	(FuncPtrDel) DaoChannel_Delete, DaoChannel_GetGCFields
};





static void FUTURE_Value( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoFuture *self = (DaoFuture*) par[0];
	if( self->state == DAO_CALL_FINISHED ){
		DaoProcess_PutValue( proc, self->value );
		return;
	}
	proc->status = DAO_VMPROC_SUSPENDED;
	proc->pauseType = DAO_VMP_ASYNC;
	DaoCallServer_AddWait( proc, self, -1, DAO_FUTRES_VALUE );
}
static void FUTURE_Wait( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoFuture *self = (DaoFuture*) par[0];
	float timeout = par[1]->xFloat.value;
	DaoProcess_PutInteger( proc, self->state == DAO_CALL_FINISHED );
	if( self->state == DAO_CALL_FINISHED || timeout == 0 ) return;
	proc->status = DAO_VMPROC_SUSPENDED;
	proc->pauseType = DAO_VMP_ASYNC;
	DaoCallServer_AddWait( proc, self, timeout, DAO_FUTRES_STATUS );
}
static DaoFuncItem futureMeths[] =
{
	{ FUTURE_Value,   "value( self : future<@V> )=>@V" },
	{ FUTURE_Wait,    "wait( self : future<@V>, timeout : float = -1 )=>int" },
	{ NULL, NULL }
};
static void DaoFuture_Delete( DaoFuture *self )
{
	DaoCstruct_Free( (DaoCstruct*) self );
	GC_DecRC( self->value );
	GC_DecRC( self->actor );
	GC_DecRC( self->message );
	GC_DecRC( self->process );
	GC_DecRC( self->precond );
	dao_free( self );
}
static void DaoFuture_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int remove )
{
	DaoFuture *self = (DaoFuture*) p;
	if( self->value ) DArray_Append( values, self->value );
	if( self->actor ) DArray_Append( values, self->actor );
	if( self->message ) DArray_Append( values, self->message );
	if( self->process ) DArray_Append( values, self->process );
	if( self->precond ) DArray_Append( values, self->precond );
	if( remove ){
		self->value = NULL;
		self->actor = NULL;
		self->message = NULL;
		self->process = NULL;
		self->precond = NULL;
	}
}

DaoTypeBase futureTyper =
{
	"future<@V=none>", NULL, NULL, (DaoFuncItem*) futureMeths, {0}, {0},
	(FuncPtrDel) DaoFuture_Delete, DaoFuture_GetGCFields
};



#endif