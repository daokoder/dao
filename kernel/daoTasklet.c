/*
// Dao Virtual Machine
// http://daoscript.org
//
// Copyright (c) 2006-2017, Limin Fu
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
// THIS SOFTWARE IS PROVIDED  BY THE COPYRIGHT HOLDERS AND  CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED  WARRANTIES,  INCLUDING,  BUT NOT LIMITED TO,  THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL  THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE FOR ANY DIRECT,
// INDIRECT,  INCIDENTAL, SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL  DAMAGES (INCLUDING,
// BUT NOT LIMITED TO,  PROCUREMENT OF  SUBSTITUTE  GOODS OR  SERVICES;  LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY OF
// LIABILITY,  WHETHER IN CONTRACT,  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include<stdio.h>
#include<string.h>
#include<time.h>
#include<math.h>
#include<assert.h>
#include"daoType.h"
#include"daoValue.h"
#include"daoThread.h"
#include"daoRoutine.h"
#include"daoObject.h"
#include"daoVmspace.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoTasklet.h"

#define MIN_TIME  1E-27


enum DaoTaskletEventType
{
	DAO_EVENT_RESUME_TASKLET ,  /* Resume the tasklet; */
	DAO_EVENT_WAIT_TASKLET   ,  /* Wait for another tasklet; */
	DAO_EVENT_WAIT_RECEIVING ,  /* Wait for receiving from a channel; */
	DAO_EVENT_WAIT_SENDING   ,  /* Wait after sending to a channel; */
	DAO_EVENT_WAIT_SELECT       /* Wait for multiple futures or channels; */
};
enum DaoTaskletEventState
{
	DAO_EVENT_WAIT ,
	DAO_EVENT_RESUME  /* Ensure the processing of timed-out events; */
};



typedef struct DaoTaskletServer  DaoTaskletServer;
typedef struct DaoTaskletThread  DaoTaskletThread;
typedef struct DaoTaskletEvent   DaoTaskletEvent;


/*
// Task event for scheduling.
//
// A task event can be generated in different situation:
// 1. Starting of a new tasklet by calling mt.start::{} or asynchronous methods:
//    DaoTaskletEvent {
//        type = DAO_EVENT_RESUME_TASKLET;
//        future = future value for the new tasklet;
//        channel = NULL;
//    };
// 2. Waiting for a tasklet (future value):
//    DaoTaskletEvent {
//        type = DAO_EVENT_WAIT_TASKLET/DAO_EVENT_RESUME_TASKLET;
//        future = future value for the waiting tasklet;
//        channel = NULL;
//    };
// 3. Waiting to Receive message from a channel:
//    DaoTaskletEvent {
//        type = DAO_EVENT_WAIT_RECEIVING;
//        future = future value for the waiting tasklet;
//        channel = channel for receiving;
//    };
// 4. Waiting after sending message to a channel:
//    DaoTaskletEvent {
//        type = DAO_EVENT_WAIT_SENDING;
//        future = future value for the sending tasklet;
//        channel = channel for sending;
//    };
// 5. Waiting for one of the future values or channels becoming ready:
//    DaoTaskletEvent {
//        type = DAO_EVENT_WAIT_SELECT;
//        future = future value for the waiting tasklet;
//        channel = NULL;
//        channels = select list of channels;
//    };
//
*/
struct DaoTaskletEvent
{
	uchar_t      type;
	uchar_t      state;
	uchar_t      timeout;
	uchar_t      auxiliary;
	double       expiring;  /* expiring time for a timeout event; */
	DaoFuture   *future;
	DaoChannel  *channel;
	DaoValue    *message;
	DaoValue    *selected;
	DaoMap      *selects;  /* DHash<DaoFuture*|DaoChannel*,0|1>; */
	DaoVmSpace  *vmspace;
};



#ifdef DAO_WITH_CONCURRENT


DaoTaskletEvent* DaoTaskletEvent_New( DaoVmSpace *vmspace )
{
	DaoTaskletEvent *self = (DaoTaskletEvent*) dao_calloc( 1, sizeof(DaoTaskletEvent) );
	self->vmspace = vmspace;
	return self;
}
void DaoTaskletEvent_Reset( DaoTaskletEvent *self )
{
	self->type = 0;
	self->state = 0;
	self->timeout = 0;
	self->auxiliary = 0;
	self->expiring = -1.0;
	GC_DecRC( self->future );
	GC_DecRC( self->channel );
	GC_DecRC( self->selected );
	GC_DecRC( self->message );
	GC_DecRC( self->selects );
	self->future = NULL;
	self->channel = NULL;
	self->selected = NULL;
	self->message = NULL;
	self->selects = NULL;
}
void DaoTaskletEvent_Delete( DaoTaskletEvent *self )
{
	DaoTaskletEvent_Reset( self );
	dao_free( self );
}
void DaoTaskletEvent_Init( DaoTaskletEvent *self, int T, int S, DaoFuture *F, DaoChannel *C )
{
	GC_Assign( & self->future, F );
	GC_Assign( & self->channel, C );
	self->type = T;
	self->state = S;
}



DaoChannel* DaoChannel_New( DaoNamespace *ns, DaoType *type, int dtype )
{
	DaoChannel *self = (DaoChannel*) dao_calloc( 1, sizeof(DaoChannel) );
	if( dtype ){
		type = DaoType_Specialize( ns->vmSpace->typeChannel, & type, type != NULL, ns );
	}
	DaoCstruct_Init( (DaoCstruct*) self, type );
	self->buffer = DList_New( DAO_DATA_VALUE );
	return self;
}




static int DaoValue_CheckCtype( DaoValue *self, DaoType *type )
{
	if( self->type < DAO_CSTRUCT || self->type > DAO_CDATA ) return 0;
	if( self->type != type->tid ) return 0;
	return self->xCstruct.ctype->core == type->core;
}






struct DaoTaskletThread
{
	DaoTaskletServer  *server;
	DThread            thread;
	DThread           *thdData;
	DThreadTask        taskFunc;  /* first task; */
	void              *taskParam;
	void              *taskOwner;
};

static DaoTaskletThread* DaoTaskletThread_New( DaoTaskletServer *server, DThreadTask func, void *param );
static void DaoTaskletThread_Run( DaoTaskletThread *self );



struct DaoTaskletServer
{
	DThread  timer;
	DMutex   mutex;
	DCondVar condv;
	DCondVar condv2;

	volatile int finishing;
	volatile int timing;
	volatile int total;
	volatile int vacant;  /* Not used; */
	volatile int idle;    /* Not active; */
	volatile int stopped;

	DList  *threads;

	DList  *functions;  /* list of DThreadTask function pointers */
	DList  *parameters; /* list of void* */
	DList  *owners;     /* list of void* */
	DList  *events;     /* list of DaoTaskletEvent* */
	DList  *events2;    /* list of DaoTaskletEvent* */
	DMap   *waitings;   /* timed waiting: <dao_complex,DaoTaskletEvent*> */
	DMap   *active;     /* map of DaoObject* or DaoProcess* keys */
	DMap   *pending;    /* map of pointers from ::parameters, ::events and ::events2 */

	DList  *caches;

	dao_complex   timestamp;  /* (time,index); */
	DaoVmSpace   *vmspace;
};

static DaoTaskletThread* DaoTaskletThread_New( DaoTaskletServer *server, DThreadTask func, void *param )
{
	DaoTaskletThread *self = (DaoTaskletThread*)dao_calloc( 1, sizeof(DaoTaskletThread) );
	self->server = server;
	self->taskFunc = func;
	self->taskParam = param;
	self->thdData = & self->thread;
	DThread_Init( & self->thread );
	return self;
}
static void DaoTaskletThread_Delete( DaoTaskletThread *self )
{
	DThread_Destroy( & self->thread );
	dao_free( self );
}
static DaoTaskletServer* DaoTaskletServer_New( DaoVmSpace *vms )
{
	DaoTaskletServer *self = (DaoTaskletServer*)dao_malloc( sizeof(DaoTaskletServer) );
	DMutex_Init( & self->mutex );
	DCondVar_Init( & self->condv );
	DCondVar_Init( & self->condv2 );
	DThread_Init( & self->timer );
	self->finishing = 0;
	self->timing = 0;
	self->total = 0;
	self->vacant = 0;
	self->idle = 0;
	self->stopped = 0;
	self->threads = DList_New(0);
	self->functions = DList_New(0);
	self->parameters = DList_New(0);
	self->owners = DList_New(0);
	self->events = DList_New(0);
	self->events2 = DList_New(0);
	self->waitings = DMap_New( DAO_DATA_COMPLEX, 0 );
	self->pending = DHash_New(0,0);
	self->active = DHash_New(0,0);
	self->caches = DList_New(0);
	self->vmspace = vms;
	self->timestamp.real = 0.0;
	self->timestamp.imag = 0.0;
	return self;
}
static void DaoTaskletServer_Delete( DaoTaskletServer *self )
{
	daoint i;
	for(i=0; i<self->threads->size; i++){
		DaoTaskletThread_Delete( (DaoTaskletThread*)self->threads->items.pVoid[i] );
	}
	for(i=0; i<self->caches->size; ++i){
		DaoTaskletEvent_Delete( (DaoTaskletEvent*) self->caches->items.pVoid[i] );
	}
	DList_Delete( self->threads );
	DList_Delete( self->functions );
	DList_Delete( self->parameters );
	DList_Delete( self->owners );
	DList_Delete( self->events );
	DList_Delete( self->events2 );
	DList_Delete( self->caches );
	DMap_Delete( self->waitings );
	DMap_Delete( self->pending );
	DMap_Delete( self->active );
	DMutex_Destroy( & self->mutex );
	DCondVar_Destroy( & self->condv );
	DCondVar_Destroy( & self->condv2 );
	DThread_Destroy( & self->timer );
	dao_free( self );
}

static void DaoTaskletServer_Timer( DaoTaskletServer *self );

static void DaoTaskletServer_Init( DaoVmSpace *vms )
{
	DaoTaskletServer *server;

	DaoCGC_Start();

	if( vms->taskletServer ) return;

	server = DaoTaskletServer_New( vms );
	/* Set it here, so that DaoVmSpace_StopTasklets() will not stop prematurally: */
	server->timing = 1;
	if( DThread_Start( & server->timer, (DThreadTask) DaoTaskletServer_Timer, server ) ==0 ){
		dao_abort( "failed to create the timer thread" );
	}
	vms->taskletServer = server;
}

static DaoTaskletServer* DaoTaskletServer_TryInit( DaoVmSpace *vms )
{
	if( vms->taskletServer == NULL ) DaoTaskletServer_Init( vms );
	return (DaoTaskletServer*) vms->taskletServer;
}

static DaoTaskletEvent* DaoTaskletServer_MakeEvent( DaoTaskletServer *self )
{
	DaoTaskletEvent *event;
	DMutex_Lock( & self->mutex );
	event = (DaoTaskletEvent*) DList_PopBack( self->caches );
	if( event == NULL ) event = DaoTaskletEvent_New( self->vmspace );
	DMutex_Unlock( & self->mutex );
	return event;
}
/* Lock self::mutex before calling this function: */
static void DaoTaskletServer_CacheEvent( DaoTaskletServer *self, DaoTaskletEvent *event )
{
	DaoTaskletEvent_Reset( event );
	DList_PushBack( self->caches, event );
}
int DaoVmSpace_GetThreadCount( DaoVmSpace *self )
{
	DaoTaskletServer *server = DaoTaskletServer_TryInit( self );
	return server->total;
}
void DaoVmSpace_AddTaskletThread( DaoVmSpace *self, DThreadTask func, void *param, void *proc )
{
	DaoTaskletThread *taskthd;
	DaoTaskletServer *server = DaoTaskletServer_TryInit( self );
	taskthd = DaoTaskletThread_New( server, func, param );
	taskthd->taskOwner = proc;
	DMutex_Lock( & server->mutex );
	server->total += 1;
	DList_Append( server->threads, taskthd );
	DMutex_Unlock( & server->mutex );
	if( DThread_Start( & taskthd->thread, (DThreadTask) DaoTaskletThread_Run, taskthd ) == 0 ){
		if( func != NULL || server->total == 0 ){
			dao_abort( "failed to create a task thread" );
		}
	}
}

static void DaoVmSpace_TryAddTaskletThread( DaoVmSpace *self, DThreadTask func, void *param, int todo )
{
	DaoTaskletServer *server = DaoTaskletServer_TryInit( self );
	int max = daoConfig.cpu > 2 ? daoConfig.cpu : 2;
	int total = server->total;
	if( total < 2 ) DaoVmSpace_AddTaskletThread( self, NULL, NULL, NULL );
	if( todo == 0 ) return;
	if( total < max || todo > pow( total + max + 10, 5 ) ){
		DaoVmSpace_AddTaskletThread( self, NULL, NULL, NULL );
	}
}

static int DaoTaskletEvent_CheckSelect( DaoTaskletEvent *self )
{
	DaoType *chatype = self->vmspace->typeChannel;
	DNode *it;
	int closed = 0;
	int move = 0;

	for(it=DaoMap_First(self->selects); it; it=DaoMap_Next(self->selects,it)){
		if( DaoValue_CheckCtype( it->key.pValue, chatype ) ){
			DaoChannel *chan = (DaoChannel*) it->key.pValue;
			move = chan->buffer->size > 0;
			closed += chan->cap == 0;
		}else{
			DaoFuture *fut = (DaoFuture*) it->key.pValue;
			move = fut->state == DAO_TASKLET_FINISHED;
		}
		if( move ) break;
	}
	if( self->selects->value->size == closed ) move = 1;
	return move;
}

static void DaoTaskletServer_ActivateEvents( DaoTaskletServer *self )
{
	char message[128];
	daoint i, j, count = 0;

	if( self->finishing == 0 ) return;
	if( self->idle != self->total ) return;
	if( self->events->size != 0 ) return;
	if( self->events2->size == 0 ) return;

#ifdef DEBUG
	sprintf( message, "WARNING: try activating events (%i,%i,%i,%i)!\n", self->total,
			self->idle, (int)self->events->size, (int)self->events2->size );
	DaoStream_WriteChars( self->vmspace->errorStream, message );
#endif
	for(i=0; i<self->events2->size; ++i){
		DaoTaskletEvent *event = (DaoTaskletEvent*) self->events2->items.pVoid[i];
		DaoChannel *chan = event->channel;
		DaoFuture *fut = event->future;
		int move = 0, closed = 0;
		switch( event->type ){
		case DAO_EVENT_WAIT_TASKLET :
			move = fut->precond == NULL || fut->precond->state == DAO_TASKLET_FINISHED;
			break;
		case DAO_EVENT_WAIT_RECEIVING :
			move = chan->buffer->size > 0;
			if( chan->cap <= 0 && chan->buffer->size == 0 ) move = 1;
			break;
		case DAO_EVENT_WAIT_SENDING :
			move = chan->buffer->size < chan->cap;
			break;
		case DAO_EVENT_WAIT_SELECT :
			if( event->selects == NULL ) continue;
			move = DaoTaskletEvent_CheckSelect( event );
			break;
		default: break;
		}
		if( move ){
			DList_Append( self->events, event );
			DList_Erase( self->events2, i, 1 );
			count += 1;
			i -= 1;
		}
	}
	DCondVar_Signal( & self->condv );
	if( count == 0 ){
		DaoStream *stream = self->vmspace->errorStream;
		DaoStream_WriteChars( stream, "ERROR: All tasklets are suspended - deadlock!\n" );
#if DEBUG
		fprintf( stderr, "ERROR: All tasklets are suspended - deadlock!\n" );
#endif
		exit(1);
	}
}
static void DaoTaskletServer_Timer( DaoTaskletServer *self )
{
	double time = 0.0;
	daoint i, timeout;

	while( self->finishing == 0 || self->stopped != self->total ){
		DMutex_Lock( & self->mutex );
		while( self->waitings->size == 0 ){
			if( self->idle == self->total && self->events2->size ){
				DaoTaskletServer_ActivateEvents( self );
			}
			if( self->finishing && self->stopped == self->total ) break;
			DCondVar_TimedWait( & self->condv2, & self->mutex, 0.01 );
		}
		if( self->waitings->size ){
			DNode *node = DMap_First( self->waitings );
			time = node->key.pComplex->real;
			time -= Dao_GetCurrentTime();
			/* wait the right amount of time for the closest arriving timeout: */
			if( time > 0 ) DCondVar_TimedWait( & self->condv2, & self->mutex, time );
		}
		DMutex_Unlock( & self->mutex );
		if( self->finishing && self->stopped == self->total ) break;

		DMutex_Lock( & self->mutex );
		if( self->waitings->size ){ /* a new wait timed out: */
			DNode *node = DMap_First( self->waitings );
			time = Dao_GetCurrentTime();
			if( node->key.pComplex->real < time ){
				DaoTaskletEvent *event = (DaoTaskletEvent*) node->value.pVoid;
				event->state = DAO_EVENT_RESUME;
				event->timeout = 1;
				event->expiring = MIN_TIME;
				DList_Append( self->events, node->value.pVoid );
				DMap_EraseNode( self->waitings, node );
			}
		}
		DCondVar_Signal( & self->condv );
		DMutex_Unlock( & self->mutex );
	}
	self->timing = 0;
}

void DaoVmSpace_AddTaskletJob( DaoVmSpace *self, DThreadTask func, void *param, void *proc )
{
	int scheduled = 0;
	DaoTaskletServer *server = DaoTaskletServer_TryInit( self );
	DMutex_Lock( & server->mutex );
	if( server->vacant > server->parameters->size || proc == NULL ){
		scheduled = 1;
		DList_Append( server->functions, func );
		DList_Append( server->parameters, param );
		DList_Append( server->owners, proc );
		DMap_Insert( server->pending, param, NULL );
		DCondVar_Signal( & server->condv );
	}
	DMutex_Unlock( & server->mutex );
	if( scheduled ){
		if( proc == NULL ){
			DaoVmSpace_TryAddTaskletThread( self, NULL, NULL, server->parameters->size );
		}
	}else{
		DaoVmSpace_AddTaskletThread( self, func, param, proc );
	}
}
static void DaoTaskletServer_AddEvent( DaoTaskletServer *self, DaoTaskletEvent *event )
{
	DList_Append( self->events, event );
	DMap_Insert( self->pending, event, NULL );
}
static void DaoVmSpace_AddEvent( DaoVmSpace *self, DaoTaskletEvent *event )
{
	DaoTaskletServer *server = DaoTaskletServer_TryInit( self );
	DMutex_Lock( & server->mutex );
	DaoTaskletServer_AddEvent( server, event );
	DCondVar_Signal( & server->condv );
	DMutex_Unlock( & server->mutex );
	DaoVmSpace_TryAddTaskletThread( self, NULL, NULL, server->pending->size );
}
#endif

void DaoProcess_ReturnFutureValue( DaoProcess *self, DaoFuture *future )
{
	DaoType *type;
	if( future == NULL ) return;
	type = future->ctype;
	type = type && type->args->size ? type->args->items.pType[0] : NULL;
	switch( self->status ){
	case DAO_PROCESS_ABORTED :
		future->state = DAO_TASKLET_ABORTED;
		break;
	case DAO_PROCESS_FINISHED :
		DaoValue_Move( self->stackValues[0], & future->value, type );
		future->state = DAO_TASKLET_FINISHED;
		break;
	case DAO_PROCESS_SUSPENDED : future->state = DAO_TASKLET_PAUSED; break;
	case DAO_PROCESS_RUNNING :
	case DAO_PROCESS_STACKED : future->state = DAO_TASKLET_RUNNING; break;
	default : break;
	}
}

void DaoVmSpace_AddTaskletCall( DaoVmSpace *self, DaoProcess *caller )
{
	DaoFuture *future;
	DaoTaskletEvent *event;
	DaoProcess *callee = DaoVmSpace_AcquireProcess( caller->vmSpace );
	DaoStackFrame *frame = caller->topFrame;
	DaoRoutine *routine = frame->routine;
	DaoType *type = (DaoType*) routine->routType->aux;
	DaoValue **params = caller->stackValues + caller->topFrame->stackBase;
	int i, count = caller->topFrame->parCount;

	if( caller->activeCode->b & DAO_CALL_BLOCK ){
		DaoValue **calleeValues, **callerValues = caller->activeValues;
		DaoStackFrame *sectFrame = DaoProcess_FindSectionFrame( caller );
		DaoStackFrame *callerFrame = caller->topFrame->prev;
		DaoVmCode *vmc, *end, *sect;
		if( sectFrame != callerFrame ){
			DaoVmSpace_ReleaseProcess( caller->vmSpace, callee );
			DaoProcess_RaiseError( caller, NULL, "Invalid code section" );
			return;
		}
		if( routine->body ){
			DaoProcess_PushRoutine( callee, callerFrame->routine, callerFrame->object );
			callerValues = caller->stackValues + callerFrame->stackBase;
		}else{
			DaoProcess_PushRoutine( callee, caller->activeRoutine, caller->activeObject );
		}
		DaoProcess_SetActiveFrame( callee, callee->topFrame );
		calleeValues = callee->stackValues + callee->topFrame->stackBase;
		callee->activeCode = caller->activeCode;
		vmc = callerFrame->routine->body->vmCodes->data.codes + callerFrame->entry;
		end = callerFrame->routine->body->vmCodes->data.codes + vmc->b;
		sect = vmc + 1;
		for(vmc=sect; vmc!=end; vmc++){
			int i = -1, code = vmc->code;
			if( code == DVM_GETVH || (code >= DVM_GETVH_I && code <= DVM_GETVH_C) ){
				i = vmc->b;
			}else if( code == DVM_SETVH || (code >= DVM_SETVH_II && code <= DVM_SETVH_CC) ){
				i = vmc->b;
			}
			if( i >= 0 ) DaoValue_Move( callerValues[i], & calleeValues[i], NULL );
		}
	}

	future = DaoFuture_New( caller->activeNamespace, type, 1 );
	future->state = DAO_TASKLET_PAUSED;
	future->actor = caller->topFrame->object;
	GC_IncRC( future->actor );

	GC_Assign( & future->process, callee );
 	GC_Assign( & callee->future, future );

	callee->parCount = count;
	/* Use routine->parCount instead of caller->topFrame->parCount, for default parameters: */
	for(i=0; i<routine->parCount; ++i) DaoValue_Copy( params[i], & callee->paramValues[i] );
	if( routine->body ){
		DaoProcess_PushRoutine( callee, routine, future->actor );
	}else{
		DaoProcess_PushFunction( callee, routine );
		callee->activeNamespace = caller->activeNamespace;
	}
	if( caller->activeCode->b & DAO_CALL_BLOCK ){
		callee->topFrame->host = callee->topFrame;
		callee->topFrame->retmode = DVM_RET_PROCESS;
		callee->topFrame->returning = 0;
	}

#ifdef DAO_WITH_CONCURRENT
	event = DaoTaskletServer_MakeEvent( DaoTaskletServer_TryInit( self ) );
	DaoTaskletEvent_Init( event, DAO_EVENT_RESUME_TASKLET, DAO_EVENT_RESUME, future, NULL );

	DaoProcess_PopFrame( caller );
	DaoProcess_PutValue( caller, (DaoValue*) future );

	DaoVmSpace_AddEvent( self, event );
#else
	DaoProcess_PopFrame( caller );
	DaoProcess_PutValue( caller, (DaoValue*) future );
	DaoProcess_InterceptReturnValue( callee );
	DaoProcess_Execute( callee );
	DaoProcess_ReturnFutureValue( callee, future );
	DaoVmSpace_ReleaseProcess( caller->vmSpace, callee );
#endif
}

#ifdef DAO_WITH_CONCURRENT
DaoFuture* DaoProcess_GetInitFuture( DaoProcess *self )
{
	DaoFuture *future;
	if( self->future ) return self->future;

	future = DaoFuture_New( self->activeNamespace, NULL, 1 );
	GC_Assign( & self->future, future );
	GC_Assign( & future->process, self );
	return future;
}

void DaoProcess_MarkActiveTasklet( DaoProcess *self, int active )
{
	DaoTaskletServer *server = DaoTaskletServer_TryInit( self->vmSpace );

	if( self->active == active ) return;

	DMutex_Lock( & server->mutex );
	if( active ){
		DMap_Insert( server->active, self, NULL );
		self->active = 1;
	}else{
		DMap_Erase( server->active, self );
		self->active = 0;
	}
	DMutex_Unlock( & server->mutex );
}
void DaoTaskletServer_AddTimedWait( DaoTaskletServer *self, DaoProcess *wait, DaoTaskletEvent *event, double timeout )
{
	/*
	// The "wait" process may not be running in the thread pool,
	// so it may have not been added to active process list.
	// It is necessary to add it to the active list now,
	// to avoid it being activated immediately after it is blocked.
	// Activating it immediately may cause a race condition,
	// because it may have not been blocked completely
	// (namely, it may be still running).
	*/
	DaoProcess_MarkActiveTasklet( wait, 1 );

	DMutex_Lock( & self->mutex );
	if( timeout >= 1E-27 ){
		self->timestamp.real = timeout + Dao_GetCurrentTime();
		self->timestamp.imag += 1;
		event->expiring = self->timestamp.real;
		DMap_Insert( self->waitings, & self->timestamp, event );
		DMap_Insert( self->pending, event, NULL );
		DCondVar_Signal( & self->condv2 );
	}else{
		event->expiring = -1.0;
		DaoTaskletServer_AddEvent( self, event );
		DCondVar_Signal( & self->condv );
	}
	DMutex_Unlock( & self->mutex );
}

void DaoVmSpace_AddTaskletWait( DaoVmSpace *self, DaoProcess *wait, DaoFuture *pre, double timeout )
{
	DaoTaskletEvent *event;
	DaoTaskletServer *server = DaoTaskletServer_TryInit( self );
	DaoFuture *future = DaoProcess_GetInitFuture( wait );

	GC_Assign( & future->precond, pre );
	future->state = DAO_TASKLET_PAUSED;

	event = DaoTaskletServer_MakeEvent( server );
	DaoTaskletEvent_Init( event, DAO_EVENT_WAIT_TASKLET, DAO_EVENT_WAIT, future, NULL );

	DaoTaskletServer_AddTimedWait( server, wait, event, timeout );
}

static int DaoTaskletServer_CheckEvent( DaoTaskletEvent *event, DaoFuture *fut, DaoChannel *chan )
{
	DaoTaskletEvent event2 = *event;
	daoint i, move = 0, closed = 0;
	switch( event->type ){
	case DAO_EVENT_WAIT_TASKLET :
		move = event->future->precond == fut;
		break;
	case DAO_EVENT_WAIT_RECEIVING :
		if( event->channel == chan ){
			move = chan->buffer->size > 0;
			move |= chan->cap <= 0 && chan->buffer->size == 0;
		}
		break;
	case DAO_EVENT_WAIT_SENDING :
		move = event->channel == chan && chan->buffer->size < chan->cap;
		break;
	case DAO_EVENT_WAIT_SELECT :
		if( event->selects == NULL ) break;
		if( fut  ) move |= DMap_Find( event->selects->value, fut ) != NULL;
		if( chan ) move |= DMap_Find( event->selects->value, chan ) != NULL;
		//move = DaoTaskletEvent_CheckSelect( event );
		break;
	default: break;
	}
	return move;
}
/*
// Only activate one event per channel:
*/
void DaoChannel_ActivateEvent( DaoChannel *self, int type, DaoTaskletServer *server )
{
	DNode *node;
	daoint i;

	for(i=0; i<server->events2->size; ++i){
		DaoTaskletEvent *event = (DaoTaskletEvent*) server->events2->items.pVoid[i];
		if( event->type != type ) continue;
		if( DaoTaskletServer_CheckEvent( event, NULL, self ) ){
			DList_Append( server->events, event );
			DList_Erase( server->events2, i, 1 );
			return;
		}
	}
	for(node=DMap_First(server->waitings); node; node=DMap_Next(server->waitings,node)){
		DaoTaskletEvent *event = (DaoTaskletEvent*) node->value.pValue;
		if( event->type != type ) continue;
		if( DaoTaskletServer_CheckEvent( event, NULL, self ) ){
			DList_Append( server->events, event );
			DMap_EraseNode( server->waitings, node );
			return;
		}
	}
}

/*
// Activate all events waiting on a future value:
*/
void DaoFuture_ActivateEvent( DaoFuture *self, DaoVmSpace *vmspace )
{
	DaoTaskletServer *server = DaoTaskletServer_TryInit( vmspace );
	DList *array = DList_New(0);
	DNode *node;
	daoint i;

	DMutex_Lock( & server->mutex );
	for(i=0; i<server->events2->size; ++i){
		DaoTaskletEvent *event = (DaoTaskletEvent*) server->events2->items.pVoid[i];
		if( DaoTaskletServer_CheckEvent( event, self, NULL ) ){
			event->state = DAO_EVENT_RESUME;
			DList_Append( server->events, event );
			DList_Erase( server->events2, i, 1 );
			i -= 1;
		}
	}
	for(node=DMap_First(server->waitings); node; node=DMap_Next(server->waitings,node)){
		DaoTaskletEvent *event = (DaoTaskletEvent*) node->value.pValue;
		/* remove from timed waiting list: */
		if( DaoTaskletServer_CheckEvent( event, self, NULL ) ){
			event->state = DAO_EVENT_RESUME;
			DList_Append( server->events, event );
			DList_Append( array, node->key.pVoid );
		}
	}
	for(i=0; i<array->size; i++) DMap_Erase( server->waitings, array->items.pVoid[i] );
	DCondVar_Signal( & server->condv );
	DMutex_Unlock( & server->mutex );
	DList_Delete( array );
}
static DaoFuture* DaoTaskletServer_GetNextFuture( DaoTaskletServer *self )
{
	DaoFuture *first, *future, *precond;
	DList *events = self->events;
	DMap *pending = self->pending;
	DMap *active = self->active;
	DNode *it;
	daoint i, j;

	for(i=0; i<events->size; i++){
		DaoTaskletEvent *event = (DaoTaskletEvent*) events->items.pVoid[i];
		DaoFuture *future = event->future;
		DaoObject *actor = future->actor;
		DaoChannel *channel = event->channel;
		DaoChannel *closed = NULL;
		DaoChannel *chselect = NULL;
		DaoFuture *futselect = NULL;
		DaoValue *selected = NULL;
		DaoValue *message = NULL;
		int type = event->type;

		if( event->state == DAO_EVENT_WAIT && future->precond != NULL ){
			if( future->precond->state != DAO_TASKLET_FINISHED ) goto MoveToWaiting;
		}
		switch( event->type ){
		case DAO_EVENT_WAIT_SENDING :
			if( channel->buffer->size >= channel->cap ){
				if( event->state == DAO_EVENT_WAIT ){
					DaoChannel_ActivateEvent( channel, DAO_EVENT_WAIT_RECEIVING, self );
					DaoChannel_ActivateEvent( channel, DAO_EVENT_WAIT_SELECT, self );
					goto MoveToWaiting;
				}
			}
			event->type = DAO_EVENT_RESUME_TASKLET;
			break;
		case DAO_EVENT_WAIT_RECEIVING :
			if( channel->buffer->size == 0 ){
				if( channel->cap > 0 && event->state == DAO_EVENT_WAIT ){
					DaoChannel_ActivateEvent( channel, DAO_EVENT_WAIT_SENDING, self );
					goto MoveToWaiting;
				}
				message = dao_none_value;
			}else{
				message = channel->buffer->items.pValue[0];
			}
			GC_Assign( & event->message, message );
			event->auxiliary = channel->cap <= 0 && channel->buffer->size == 0;
			event->type = DAO_EVENT_RESUME_TASKLET;
			DList_PopFront( channel->buffer );
			if( channel->buffer->size < channel->cap ){
				DaoChannel_ActivateEvent( channel, DAO_EVENT_WAIT_SENDING, self );
			}
			if( channel->buffer->size ){
				DaoChannel_ActivateEvent( channel, DAO_EVENT_WAIT_RECEIVING, self );
			}
			break;
		case DAO_EVENT_WAIT_SELECT :
			message = dao_none_value;
			for(it=DaoMap_First(event->selects); it; it=DaoMap_Next(event->selects,it)){
				if( DaoValue_CheckCtype( it->key.pValue, self->vmspace->typeChannel ) ){
					DaoChannel *chan = (DaoChannel*) it->key.pValue;
					if( chan->buffer->size > 0 ){
						chselect = chan;
						selected = it->key.pValue;
						message = chan->buffer->items.pValue[0];
						closed = NULL;
						break;
					}else if( chan->cap == 0 ){
						closed = chan;
					}
				}else{
					DaoFuture *fut = (DaoFuture*) it->key.pValue;
					if( fut->state == DAO_TASKLET_FINISHED ){
						futselect = fut;
						selected = it->key.pValue;
						message = fut->value;
						break;
					}
				}
			}
			if( selected == NULL ) selected = (DaoValue*) closed;
			if( event->state == DAO_EVENT_WAIT && event->selects->value->size ){
				if( selected == NULL ) goto MoveToWaiting;
			}

			GC_Assign( & event->message, message );
			GC_Assign( & event->selected, selected );
			event->auxiliary = event->selects->value->size == 0;
			event->type = DAO_EVENT_RESUME_TASKLET;
			/* change status to not finished: */
			if( chselect != NULL || futselect != NULL ) event->auxiliary = 0;
			if( chselect ){
				DList_PopFront( chselect->buffer );
				if( chselect->buffer->size < chselect->cap ){
					DaoChannel_ActivateEvent( chselect, DAO_EVENT_WAIT_SENDING, self );
				}
				if( chselect->buffer->size ){
					DaoChannel_ActivateEvent( chselect, DAO_EVENT_WAIT_SELECT, self );
				}
			}
			if( futselect != NULL || closed != NULL ){
				void *key = futselect ? (void*)futselect : (void*)closed;
				DMap_Erase( event->selects->value, key );
			}
			break;
		default: break;
		}
		if( actor ){
			DNode *it = DMap_Find( active, actor->rootObject );
			if( actor->rootObject->isAsync ){
				if( it && it->value.pVoid != (void*) future ) continue;
			}else if( it ){
				continue;
			}
		}
		if( future->process && DMap_Find( active, future->process ) ) continue;
		DList_Erase( events, i, 1 );
		DMap_Erase( pending, event );
		if( actor ){
			void *value = actor->rootObject->isAsync ? future : NULL;
			DMap_Insert( active, actor->rootObject, value );
		}
		if( future->process ){
			DMap_Insert( active, future->process, NULL );
			future->process->active = 1;
		}

		/*
		// DaoValue_Move() should be used instead of GC_Assign() for thread safety.
		// Because using GC_Assign() here, may caused "future->message" of primitive
		// type being deleted, right after DaoFuture_HandleGC() has retrieved it
		// for GC scanning.
		 */
		DaoValue_Move( event->message, & future->message, NULL );
		DaoValue_Move( event->selected, & future->selected, NULL );
		future->aux1 = event->auxiliary;
		future->timeout = event->timeout;

		GC_IncRC( future ); /* To be decreased at the end of tasklet; */
		DaoTaskletServer_CacheEvent( self, event );
		return future;
MoveToWaiting:
		if( event->expiring >= 0.0 && event->expiring < MIN_TIME ) continue;
		if( event->expiring >= MIN_TIME ){
			dao_complex com = {0.0,0.0};
			com.real = event->expiring;
			DMap_Insert( self->waitings, & com, event );
			DCondVar_Signal( & self->condv2 );
		}else{
			DList_Append( self->events2, event );
		}
		DList_Erase( self->events, i, 1 );
		i -= 1;
	}
	return NULL;
}

static void DaoTaskletThread_Run( DaoTaskletThread *self )
{
	DaoTaskletServer *server = self->server;
	double wt = 0.001;
	daoint i, count, timeout;

	if( self->taskFunc ){
		self->taskFunc( self->taskParam );
		self->taskOwner = NULL;
	}
	while( server->vmspace->stopit == 0 ){
		DaoProcess *process = NULL;
		DaoFuture *future = NULL;
		DThreadTask function = NULL;
		void *parameter = NULL;

		self->thdData->state = 0;
		DMutex_Lock( & server->mutex );
		server->idle += 1;
		server->vacant += self->taskOwner == NULL;
		while( server->pending->size == (server->events2->size + server->waitings->size) ){
			//printf( "%p %i %i %i %i\n", self, server->events->size, server->pending->size, server->events2->size, server->waitings->size );
			if( server->vmspace->stopit ) break;
			if( server->finishing && server->vacant == server->total ){
				if( (server->events2->size + server->waitings->size) == 0 ) break;
			}
			wt = 0.01*(server->idle == server->total) + 0.001;
			timeout = DCondVar_TimedWait( & server->condv, & server->mutex, wt );
		}
		for(i=0; i<server->parameters->size; ++i){
			void *param = server->parameters->items.pVoid[i];
			if( DMap_Find( server->active, param ) ) continue;
			DMap_Insert( server->active, param, NULL );
			self->taskOwner = server->owners->items.pVoid[i];
			function = (DThreadTask) server->functions->items.pVoid[i];
			parameter = param;
			DList_Erase( server->functions, i, 1 );
			DList_Erase( server->parameters, i, 1 );
			DList_Erase( server->owners, i, 1 );
			DMap_Erase( server->pending, parameter );
			server->idle -= 1;
			server->vacant -= 1;
			break;
		}
		DMutex_Unlock( & server->mutex );

		if( server->vmspace->stopit ) break;
		if( function ){
			(*function)( parameter );
			self->taskOwner = NULL;
			DMutex_Lock( & server->mutex );
			DMap_Erase( server->active, parameter );
			DMutex_Unlock( & server->mutex );
			continue;
		}

		if( server->pending->size == 0 && server->finishing && server->vacant == server->total ) break;

		DMutex_Lock( & server->mutex );
		server->idle -= 1;
		server->vacant -= self->taskOwner == NULL;
		future = DaoTaskletServer_GetNextFuture( server );
		DMutex_Unlock( & server->mutex );

		if( future == NULL ) continue;

		process = future->process;
		if( process == NULL ){
			GC_DecRC( future );
			continue;
		}

		count = process->exceptions->size;
		future->state = DAO_TASKLET_RUNNING;
		DaoProcess_InterceptReturnValue( process );
		DaoProcess_Start( process );
		if( process->exceptions->size > count ) DaoProcess_PrintException( process, NULL, 1 );
		if( process->status <= DAO_PROCESS_ABORTED ) self->taskOwner = NULL;

		if( future->actor ){
			int erase = 1;
			DMutex_Lock( & server->mutex );
			if( future->actor->rootObject->isAsync ){
				erase = process->status == DAO_PROCESS_FINISHED;
			}
			if( erase ) DMap_Erase( server->active, future->actor->rootObject );
			DMutex_Unlock( & server->mutex );
		}
		DMutex_Lock( & server->mutex );
		DMap_Erase( server->active, process );
		process->active = 0;
		DMutex_Unlock( & server->mutex );

		DaoProcess_ReturnFutureValue( process, future );
		if( future->state == DAO_TASKLET_FINISHED ){
			DaoFuture_ActivateEvent( future, server->vmspace );
		}
		GC_DecRC( future );
	}
	DMutex_Lock( & server->mutex );
	server->stopped += 1;
	DMutex_Unlock( & server->mutex );
}

void DaoVmSpace_JoinTasklets( DaoVmSpace *self )
{
	DaoTaskletServer *server = (DaoTaskletServer*) self->taskletServer;
	DCondVar condv;

	if( server == NULL ) return;

	DCondVar_Init( & condv );
	DMutex_Lock( & server->mutex );
	while( server->pending->size || server->vacant != server->total ){
		DCondVar_TimedWait( & condv, & server->mutex, 0.01 );
	}
	DMutex_Unlock( & server->mutex );
	DCondVar_Destroy( & condv );
}
void DaoVmSpace_StopTasklets( DaoVmSpace *self )
{
	DaoTaskletServer *server = (DaoTaskletServer*) self->taskletServer;
	DaoTaskletThread *taskthd;
	DCondVar condv;

	if( server == NULL ) return;

	DCondVar_Init( & condv );
	server->finishing = 1;

	taskthd = DaoTaskletThread_New( server, NULL, NULL );
	taskthd->thdData = DThread_GetCurrent();
	DMutex_Lock( & server->mutex );
	server->total += 1;
	DMutex_Unlock( & server->mutex );

	DaoTaskletThread_Run( taskthd );  /* process tasks in the main thread; */

	DMutex_Lock( & server->mutex );
	while( server->stopped != server->total || server->timing ){
		DCondVar_TimedWait( & condv, & server->mutex, 0.01 );
	}
	DMutex_Unlock( & server->mutex );

	DCondVar_Destroy( & condv );
	DaoTaskletThread_Delete( taskthd );
	DaoTaskletServer_Delete( server );
	self->taskletServer = NULL;
}






static int DaoType_CheckPrimitiveType( DaoType *self )
{
	daoint i;

	if( self == NULL || self->tid == DAO_NONE ) return 0;
	if( self->tid <= DAO_ARRAY ) return 1;
	if( self->tid > DAO_TUPLE && self->tid != DAO_VARIANT ) return 0;

	if( self->tid != DAO_TUPLE && (self->args == NULL || self->args->size == 0) ) return 0;
	for(i=0; i<self->args->size; ++i){
		DaoType *type = self->args->items.pType[i];
		if( type == NULL ) return 0;
		if( type->tid == DAO_PAR_NAMED ) type = (DaoType*) type->aux;
		if( DaoType_CheckPrimitiveType( type ) == 0 ) return 0;
	}
	return 1;
}
static DaoValue* DaoValue_DeepCopy( DaoValue *self )
{
	DNode *it;
	daoint i;
	if( self == NULL ) return NULL;
	if( self->type <= DAO_ENUM ) return self; /* simple types will be copied at use; */
	if( self->type == DAO_ARRAY ) return (DaoValue*) DaoArray_Copy( (DaoArray*) self, NULL );
	if( self->type == DAO_LIST ){
		DaoList *list = (DaoList*) self;
		DaoList *copy = DaoList_New();
		GC_Assign( & copy->ctype, list->ctype );
		for(i=0; i<list->value->size; ++i){
			DaoValue *value = DaoValue_DeepCopy( list->value->items.pValue[i] );
			DaoList_Append( copy, value );
		}
		return (DaoValue*) copy;
	}else if( self->type == DAO_MAP ){
		DaoMap *map = (DaoMap*) self;
		DaoMap *copy = DaoMap_New( map->value->hashing );
		GC_Assign( & copy->ctype, map->ctype );
		for(it=DMap_First(map->value); it; it=DMap_Next(map->value,it)){
			DaoValue *key = DaoValue_DeepCopy( it->key.pValue );
			DaoValue *value = DaoValue_DeepCopy( it->value.pValue );
			DaoMap_Insert( copy, key, value );
		}
		return (DaoValue*) copy;
	}else if( self->type == DAO_TUPLE ){
		DaoTuple *tuple = (DaoTuple*) self;
		DaoTuple *copy = DaoTuple_New( tuple->size );
		GC_Assign( & copy->ctype, tuple->ctype );
		for(i=0; i<tuple->size; ++i){
			DaoValue *value = DaoValue_DeepCopy( tuple->values[i] );
			DaoTuple_SetItem( copy, value, i );
		}
		return (DaoValue*) copy;
	}
	return NULL;
}


static void CHANNEL_SetCap( DaoChannel *self, DaoValue *value, DaoProcess *proc )
{
	self->cap = value->xInteger.value;
	if( self->cap > 0 ) return;
	self->cap = 1;
	DaoProcess_RaiseError( proc, "Param", "channel capacity must be greater than 0" );
}
static void CHANNEL_New( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoType *retype = DaoProcess_GetReturnType( proc );
	DaoChannel *self = DaoChannel_New( proc->activeNamespace, retype, 0 );
	CHANNEL_SetCap( self, par[0], proc );
	if( DaoType_CheckPrimitiveType( retype->args->items.pType[0] ) == 0 ){
		DString *s = DString_New();
		DString_AppendChars( s, "data type " );
		DString_Append( s, retype->args->items.pType[0]->name );
		DString_AppendChars( s, " is not supported for channel" );
		DaoProcess_RaiseError( proc, NULL, s->chars );
		DString_Delete( s );
	}
	DaoProcess_PutValue( proc, (DaoValue*) self );
	DaoTaskletServer_TryInit( masterVmSpace );
}
static void CHANNEL_Buffer( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoChannel *self = (DaoChannel*) par[0];
	DaoProcess_PutInteger( proc, self->buffer->size );
}
static void CHANNEL_Cap( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoTaskletServer *server = DaoTaskletServer_TryInit( proc->vmSpace );
	DaoChannel *self = (DaoChannel*) par[0];
	daoint i;

	DaoProcess_PutInteger( proc, self->cap );
	if( N == 1 ) return;

	/* Closing the channel: */
	DMutex_Lock( & server->mutex );
	self->cap = par[1]->xInteger.value;
	if( self->cap == 0 ){
		DaoChannel_ActivateEvent( self, DAO_EVENT_WAIT_RECEIVING, server );
		DaoChannel_ActivateEvent( self, DAO_EVENT_WAIT_SELECT, server );
		DCondVar_Signal( & server->condv );
	}
	DMutex_Unlock( & server->mutex );
}
void DaoChannel_Send( DaoChannel *self, DaoValue *data, DaoTaskletServer *server )
{
	DMutex_Lock( & server->mutex );
	DList_Append( self->buffer, data );
	DaoChannel_ActivateEvent( self, DAO_EVENT_WAIT_RECEIVING, server );
	DaoChannel_ActivateEvent( self, DAO_EVENT_WAIT_SELECT, server );
	DCondVar_Signal( & server->condv );
	DMutex_Unlock( & server->mutex );
}
static void CHANNEL_Send( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoValue *data;
	DaoTaskletServer *server = DaoTaskletServer_TryInit( proc->vmSpace );
	DaoFuture *future = DaoProcess_GetInitFuture( proc );
	DaoChannel *self = (DaoChannel*) par[0];
	float timeout = par[2]->xFloat.value;

	DaoProcess_PutBoolean( proc, 1 );
	if( self->cap <= 0 ){
		DaoProcess_RaiseError( proc, "Param", "channel is closed" );
		return;
	}

	data = DaoValue_DeepCopy( par[1] );
	if( data == NULL ){
		DaoProcess_RaiseError( proc, "Param", "invalid data for the channel" );
		return;
	}

	//printf( "CHANNEL_Send: %p\n", event );
	DaoChannel_Send( self, data, server );

	if( self->buffer->size >= self->cap ){
		DaoTaskletEvent *event = DaoTaskletServer_MakeEvent( server );
		DaoTaskletEvent_Init( event, DAO_EVENT_WAIT_SENDING, DAO_EVENT_WAIT, future, self );
		proc->status = DAO_PROCESS_SUSPENDED;
		proc->pauseType = DAO_PAUSE_CHANNEL_SEND;
		DaoTaskletServer_AddTimedWait( server, proc, event, timeout );
	}
}
static void CHANNEL_Receive( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoTaskletEvent *event = NULL;
	DaoTaskletServer *server = DaoTaskletServer_TryInit( proc->vmSpace );
	DaoFuture *future = DaoProcess_GetInitFuture( proc );
	DaoChannel *self = (DaoChannel*) par[0];
	float timeout = par[1]->xFloat.value;

	event = DaoTaskletServer_MakeEvent( server );
	DaoTaskletEvent_Init( event, DAO_EVENT_WAIT_RECEIVING, DAO_EVENT_WAIT, future, self );
	proc->status = DAO_PROCESS_SUSPENDED;
	proc->pauseType = DAO_PAUSE_CHANNEL_RECEIVE;
	DaoTaskletServer_AddTimedWait( server, proc, event, timeout );

	/* Message may have been sent before this call: */
	if( self->buffer->size ){
		DMutex_Lock( & server->mutex );
		DaoChannel_ActivateEvent( self, DAO_EVENT_WAIT_RECEIVING, server );
		DCondVar_Signal( & server->condv );
		DMutex_Unlock( & server->mutex );
	}
}

static DaoFunctionEntry daoChannelMeths[] =
{
	{ CHANNEL_New,      "Channel<@V>( cap = 1 )" },
	{ CHANNEL_Buffer,   "buffer( self: Channel<@V> ) => int" },
	{ CHANNEL_Cap,      "cap( self: Channel<@V> ) => int" },
	{ CHANNEL_Cap,      "cap( self: Channel<@V>, cap: int ) => int" },
	{ CHANNEL_Send,     "send( self: Channel<@V>, data: @V, timeout: float = -1 ) => bool" },
	{ CHANNEL_Receive,  "receive( self: Channel<@V>, timeout: float = -1 ) => tuple<data: @V|none, status: enum<received,timeout,finished>>" },
	{ NULL, NULL }
};

static void DaoChannel_Delete( DaoChannel *self )
{
	DaoCstruct_Free( (DaoCstruct*) self );
	DList_Delete( self->buffer );
	dao_free( self );
}

static void DaoChannel_HandleGC( DaoValue *p, DList *vs, DList *lists, DList *ms, int rm )
{
	DaoChannel *self = (DaoChannel*) p;
	DList_Append( lists, self->buffer );
}


DaoTypeCore daoChannelCore =
{
	"Channel<@V>",                                     /* name */
	sizeof(DaoChannel),                                /* size */
	{ NULL },                                          /* bases */
	{ NULL },                                          /* casts */
	NULL,                                              /* numbers */
	daoChannelMeths,                                   /* methods */
	DaoCstruct_CheckGetField,  DaoCstruct_DoGetField,  /* GetField */
	NULL,                      NULL,                   /* SetField */
	NULL,                      NULL,                   /* GetItem */
	NULL,                      NULL,                   /* SetItem */
	NULL,                      NULL,                   /* Unary */
	NULL,                      NULL,                   /* Binary */
	NULL,                      NULL,                   /* Conversion */
	NULL,                      NULL,                   /* ForEach */
	NULL,                                              /* Print */
	NULL,                                              /* Slice */
	NULL,                                              /* Compare */
	NULL,                                              /* Hash */
	NULL,                                              /* Create */
	NULL,                                              /* Copy */
	(DaoDeleteFunction) DaoChannel_Delete,             /* Delete */
	DaoChannel_HandleGC                                /* HandleGC */
};





void DaoMT_Select( DaoProcess *proc, DaoValue *par[], int n )
{
	DNode *it;
	DaoTaskletEvent *event = NULL;
	DaoTaskletServer *server = DaoTaskletServer_TryInit( proc->vmSpace );
	DaoFuture *future = DaoProcess_GetInitFuture( proc );
	DaoMap *selects = (DaoMap*) par[0];
	float timeout = par[1]->xFloat.value;

	for(it=DaoMap_First(selects); it; it=DaoMap_Next(selects,it)){
		DaoValue *value = it->key.pValue;
		int isfut = DaoValue_CheckCtype( value, proc->vmSpace->typeFuture );
		int ischan = DaoValue_CheckCtype( value, proc->vmSpace->typeChannel );
		if( isfut == 0 && ischan == 0 ){
			DaoProcess_RaiseError( proc, "Param", "invalid type selection" );
			return;
		}
	}

	event = DaoTaskletServer_MakeEvent( server );
	DaoTaskletEvent_Init( event, DAO_EVENT_WAIT_SELECT, DAO_EVENT_WAIT, future, NULL );
	GC_Assign( & event->selects, selects );
	proc->status = DAO_PROCESS_SUSPENDED;
	proc->pauseType = DAO_PAUSE_CHANFUT_SELECT;
	DaoTaskletServer_AddTimedWait( server, proc, event, timeout );

	/* Message may have been sent before this call: */
	DMutex_Lock( & server->mutex );
	DaoChannel_ActivateEvent( NULL, DAO_EVENT_WAIT_SELECT, server );
	DCondVar_Signal( & server->condv );
	DMutex_Unlock( & server->mutex );
}

#endif





DaoFuture* DaoFuture_New( DaoNamespace *ns, DaoType *type, int vatype )
{
	DaoFuture *self = (DaoFuture*) dao_calloc( 1, sizeof(DaoFuture) );
	if( vatype ){
		type = DaoType_Specialize( ns->vmSpace->typeFuture, & type, type != NULL, ns );
	}
	DaoCstruct_Init( (DaoCstruct*) self, type );
	GC_IncRC( dao_none_value );
	self->value = dao_none_value;
	self->state = DAO_TASKLET_PAUSED;
	return self;
}
static void DaoFuture_Delete( DaoFuture *self )
{
	DaoCstruct_Free( (DaoCstruct*) self );
	GC_DecRC( self->value );
	GC_DecRC( self->actor );
	GC_DecRC( self->message );
	GC_DecRC( self->selected );
	GC_DecRC( self->process );
	GC_DecRC( self->precond );
	dao_free( self );
}


static void FUTURE_Value( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoFuture *self = (DaoFuture*) par[0];
	if( self->state == DAO_TASKLET_FINISHED ){
		DaoProcess_PutValue( proc, self->value );
		return;
	}
#ifdef DAO_WITH_CONCURRENT
	proc->status = DAO_PROCESS_SUSPENDED;
	proc->pauseType = DAO_PAUSE_FUTURE_VALUE;
	DaoVmSpace_AddTaskletWait( proc->vmSpace, proc, self, -1 );
#else
	DaoProcess_RaiseError( proc, NULL, "Invalid future value" );
#endif
}
static void FUTURE_Wait( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoFuture *self = (DaoFuture*) par[0];
	float timeout = par[1]->xFloat.value;
	DaoProcess_PutBoolean( proc, self->state == DAO_TASKLET_FINISHED );
	if( self->state == DAO_TASKLET_FINISHED || timeout == 0 ) return;
#ifdef DAO_WITH_CONCURRENT
	proc->status = DAO_PROCESS_SUSPENDED;
	proc->pauseType = DAO_PAUSE_FUTURE_WAIT;
	DaoVmSpace_AddTaskletWait( proc->vmSpace, proc, self, timeout );
#else
	DaoProcess_RaiseError( proc, NULL, "Invalid future value" );
#endif
}
static DaoFunctionEntry daoFutureMeths[] =
{
	{ FUTURE_Value,   "value( self: Future<@V> )=>@V" },
	{ FUTURE_Wait,    "wait( self: Future<@V>, timeout: float = -1 ) => bool" },
	{ NULL, NULL }
};

static void DaoFuture_HandleGC( DaoValue *p, DList *values, DList *lists, DList *maps, int remove )
{
	DaoFuture *self = (DaoFuture*) p;
	if( self->value ) DList_Append( values, self->value );
	if( self->actor ) DList_Append( values, self->actor );
	if( self->message ) DList_Append( values, self->message );
	if( self->selected ) DList_Append( values, self->selected );
	if( self->process ) DList_Append( values, self->process );
	if( self->precond ) DList_Append( values, self->precond );
	if( remove ){
		self->value = NULL;
		self->actor = NULL;
		self->message = NULL;
		self->selected = NULL;
		self->process = NULL;
		self->precond = NULL;
	}
}


DaoTypeCore daoFutureCore =
{
	"Future<@V=none>",                                 /* name */
	sizeof(DaoFuture),                                 /* size */
	{ NULL },                                          /* bases */
	{ NULL },                                          /* casts */
	NULL,                                              /* numbers */
	daoFutureMeths,                                    /* methods */
	DaoCstruct_CheckGetField,  DaoCstruct_DoGetField,  /* GetField */
	NULL,                      NULL,                   /* SetField */
	NULL,                      NULL,                   /* GetItem */
	NULL,                      NULL,                   /* SetItem */
	NULL,                      NULL,                   /* Unary */
	NULL,                      NULL,                   /* Binary */
	NULL,                      NULL,                   /* Conversion */
	NULL,                      NULL,                   /* ForEach */
	NULL,                                              /* Print */
	NULL,                                              /* Slice */
	NULL,                                              /* Compare */
	NULL,                                              /* Hash */
	NULL,                                              /* Create */
	NULL,                                              /* Copy */
	(DaoDeleteFunction) DaoFuture_Delete,              /* Delete */
	DaoFuture_HandleGC                                 /* HandleGC */
};

