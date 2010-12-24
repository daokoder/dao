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

#if( defined DAO_WITH_THREAD && ( defined DAO_WITH_MPI || defined DAO_WITH_AFC ) )

#define DAO_SCHED_MIN     2
#define DAO_SCHED_MAX     1E2
#define DAO_MESSAGE_LIFE  1E3

typedef struct DaoTaskThread  DaoTaskThread;
typedef struct DaoTaskData    DaoTaskData;

extern DaoClass *daoClassFutureValue;
extern DaoVmProcess *mainVmProcess;

struct DaoTaskThread
{
	DaoThread    *thread;
	DThreadData  *thdData;
	DaoTaskData  *sched;

	/* should be used to run C functions only:
	 * since "foo() async join" or mpi.receive() could be called in any place 
	 * inside Dao routines, a seperated VM process is need for them. */
	DaoVmProcess *vmproc;
	DaoContext   *context;
};

DaoTaskThread* DaoTaskThread_New( DaoTaskData *scheduler );
void DaoTaskThread_Run( DaoTaskThread *self );

struct DaoTaskData
{
	DMutex   mutex;
	DCondVar condv;
	DCondVar condv2;
	DCondVar condv3;

	int allThdCount;
	int idleThdCount;
	int hurryCount;

	DaoThread    *thread;
	DaoVmSpace   *vmSpace;
	DaoThdMaster *thdMaster;

	DArray  *messages;
	DArray  *modes;
	DArray  *senders;
	DArray  *futures;
	DArray  *lifepoints;

	DArray  *timeActors; /* <DaoVmProcess*,int> processs called stdlib.receive( timeout ) */
	DMap    *busyActors;
	DMap    *idleActors; /* <DaoVmProcess*,int> virtual processs called stdlib.receive() */
	DMap    *friendPids; /* <DString*,DaoVmProcess*> */

	DString *astring;
};

static DaoTaskData *daoTaskData;

DaoTaskData* DaoTaskData_New( DaoVmSpace *vms, int count );

void DaoTaskData_AddMessage( DaoTaskData *self, DVarray *msg, int mode, 
		DaoVmProcess *sender, DaoObject *future );
void DaoTaskData_Run( DaoTaskData *self );

/*===================
  ====================*/

DaoTaskThread* DaoTaskThread_New( DaoTaskData *sched )
{
	DaoTaskThread *self = (DaoTaskThread*)dao_malloc( sizeof(DaoTaskThread) );
	DaoRoutine *dummy = DaoRoutine_New();
	dummy->locRegCount = 3;
	self->thdData = NULL;
	self->sched  = sched;
	self->thread = DaoThread_New( sched->thdMaster );
	self->context = DaoContext_New();
	GC_IncRC( self->context );
	DaoContext_Init( self->context, dummy );
	self->vmproc = DaoVmProcess_New( sched->vmSpace );
	self->context->vmSpace = sched->vmSpace;
	GC_IncRC( self->vmproc );
	return self;
}
DaoTaskData* DaoTaskData_New( DaoVmSpace *vms, int count )
{
	DaoTaskData *self = (DaoTaskData*)dao_malloc( sizeof(DaoTaskData) );
	DMutex_Init( & self->mutex );
	DCondVar_Init( & self->condv );
	DCondVar_Init( & self->condv2 );
	DCondVar_Init( & self->condv3 );
	self->allThdCount = count;
	self->idleThdCount = 0;
	self->hurryCount = 0;
	self->vmSpace = vms;
	self->thdMaster = DaoThdMaster_New();
	self->thread = DaoThread_New( self->thdMaster );
	self->messages = DArray_New(D_VARRAY);
	self->modes = DArray_New(0);
	self->senders = DArray_New(0);
	self->futures = DArray_New(0);
	self->lifepoints = DArray_New(0);
	self->timeActors = DArray_New(0);
	self->busyActors = DMap_New(0,0);
	self->idleActors = DMap_New(0,0);
	self->friendPids = DMap_New(D_STRING,0);
	self->astring = DString_New(1);
	return self;
}
void DaoSched_StartWaiting( DaoVmProcess *vmp )
{
	DMutex_Lock( & daoTaskData->mutex );
#ifdef WIN32
	if( vmp->mpiData->timeout >1E-6 )
		DArray_Append( daoTaskData->timeActors, vmp );
	else
		MAP_Insert( daoTaskData->idleActors, vmp, 0 );
#else
	//printf( "DaoSched_StartWaiting %i %i\n", vmp->mpiData->timeout.tv_sec, vmp->mpiData->timeout.tv_usec );
	if( vmp->mpiData->timeout.tv_sec >0 || vmp->mpiData->timeout.tv_usec >0 )
		DArray_Append( daoTaskData->timeActors, vmp );
	else
		MAP_Insert( daoTaskData->idleActors, vmp, 0 );
#endif
	DMutex_Unlock( & daoTaskData->mutex );
}
void DaoTaskThread_Run( DaoTaskThread *self )
{
	DaoTaskData *sched = self->sched;
	DaoVmProcess *vmproc, *vmp;
	DaoVmProcess *sender = NULL;
	DaoObject    *future = NULL;
	DaoContext *ctx;
	DaoBase  *pactor;
	DVarray  *msg;
	DVarray  *message = DVarray_New();
	DValue *pars;
	DValue  p = daoNullValue, value = daoNullValue;
	DValue  actor = daoNullValue;
	struct timeval now;
	double wt = 0.001;
	double curtime;
	int timeout, hurry;
	int i, npar, mode=0, nactor=0;

	if( getenv( "PROC_NAME" ) ) nactor = 1; /*Proxy_Receiver*/

	self->thdData = DThread_GetSpecific();

	while(1){
		DVarray_Clear( message );
		self->thdData->state = 0;

		DMutex_Lock( & sched->mutex );
		DCondVar_Signal( & sched->condv2 );

		timeout = 0;
		if( actor.t == DAO_CONTEXT || actor.t == DAO_CDATA || actor.t == DAO_VMPROCESS ){
			if( actor.t ==DAO_CONTEXT && actor.v.context->object ){
				ctx = actor.v.context;
				if( ctx->object ) MAP_Erase( sched->busyActors, ctx->object );
			}else{
				MAP_Erase( sched->busyActors, actor.v.p );
			}
		}
		sched->idleThdCount ++;
		/*
		   printf( "%p before waiting %i\n", self, sched->idleThdCount );
		 */
		while( timeout ==0 && sched->messages->size ==0 && sched->timeActors->size ==0
				&& sched->idleActors->size ==0 ){
			timeout = DCondVar_TimedWait( & sched->condv, & sched->mutex, wt );
		}
		/*
		   printf( "%p after waiting: %p %p %p\n", self, vmproc, mainVmProcess, sched );
		   printf( "%p timeout = %i, status: %i, message: %i, timeActors: %i, idleActors : %i\n",
		   sched, timeout, mainVmProcess ? mainVmProcess->status : 0, sched->messages->size, 
		   sched->timeActors->size, sched->idleActors->size );
		 */

		if( timeout && mainVmProcess && mainVmProcess->status <= DAO_VMPROC_ABORTED
				&& sched->messages->size ==0 && sched->timeActors->size ==0
				&& sched->idleActors->size ==0 ){
			//printf( "%p status = %i: %i %i\n", self, mainVmProcess->status, sched->idleThdCount, sched->allThdCount );
			if( sched->idleThdCount + nactor == sched->allThdCount ){
				//printf( "before signal\n" );
				DCondVar_Signal( & sched->condv3 );
				//printf( "after signal\n" );
			}
			sched->idleThdCount --;
			sched->allThdCount --;
			DMutex_Unlock( & sched->mutex );
			break;
		}
		//printf( "after waiting %i\n", sched->idleThdCount );
		//    printf( "pactor 0 = %p\n", pactor );

		sched->idleThdCount --;
		hurry = (sched->hurryCount --) > 0;
		DMutex_Unlock( & sched->mutex );

		actor = daoNullValue;
		pactor = NULL;
		future = NULL;
		sender = NULL;
		/* look for a waiting actor that is timed out: */
		if( hurry ==0 && sched->timeActors->size ){
			DMutex_Lock( & sched->mutex ); // mutex for timed actors
#ifdef WIN32
			curtime = timeGetTime();
			for(i=0; i<sched->timeActors->size; ){
				vmp = (DaoVmProcess*)sched->timeActors->items.pBase[i];
				if( vmp->mpiData->timeout <1E-6 ){
					pactor = vmp;
					vmp->mpiData->timeout = 0.0;
					DArray_Erase( sched->timeActors, i, 1 );
					continue;
				}else if( vmp->mpiData->timeout < curtime ){
					pactor = vmp;
					vmp->mpiData->timeout = 0.0;
					DArray_Erase( sched->timeActors, i, 1 );
					break;
				}
				i ++;
			}
#else
			gettimeofday( & now, NULL );
			for(i=0; i<sched->timeActors->size; ){
				vmp = (DaoVmProcess*)sched->timeActors->items.pBase[i];
				if( vmp->mpiData->timeout.tv_sec == 0 ){
					pactor = (DaoBase*) vmp;
					DArray_Erase( sched->timeActors, i, 1 );
					continue;
				}else if( ( vmp->mpiData->timeout.tv_sec == now.tv_sec && vmp->mpiData->timeout.tv_usec <= now.tv_usec )
						|| vmp->mpiData->timeout.tv_sec < now.tv_sec ){
					pactor = (DaoBase*) vmp;
					vmp->mpiData->timeout.tv_sec = 0;
					DArray_Erase( sched->timeActors, i, 1 );
					break;
				}
				i ++;
			}
#endif
			DMutex_Unlock( & sched->mutex );
		}
		//printf( "pactor 1 = %p\n", pactor );
		/* waiting actors are created by calling mpi.receive(),
		 * such actors will never be in sched->busyActors: */
		if( pactor ){
			/* check if there is message for it: */
			DMutex_Lock( & sched->mutex );
			for(i=0; i<sched->messages->size; i++){
				msg = sched->messages->items.pVarray[i];
				sender = (DaoVmProcess*) sched->senders->items.pBase[i];
				if( msg->data[0].v.p != pactor ) continue;
				vmp = (DaoVmProcess*) pactor;
				/* if the actor started waiting by calling mpi.receive( pid_name, ... ),
				 * check if the sender of the message has name equal to pid_name: */
				if( DString_Size( vmp->mpiData->pidAwaited ) && msg->size >1 ){
					p = msg->data[1];
					/* message sent by mpi.send() will have the sender's name
					 * as the second item in the message: */
					if( p.t != DAO_STRING ) continue;
					/* if it is not the intended sender, ignore */
					if( DString_Compare( p.v.s, vmp->mpiData->pidAwaited ) !=0 ) continue;
				}
				DVarray_Swap( message, sched->messages->items.pVarray[i] );
				DVarray_Erase( message, 0, 1 );
				future = (DaoObject*)sched->futures->items.pBase[i];
				mode = sched->modes->items.pInt[i];
				DArray_Erase( sched->messages, i, 1 );
				DArray_Erase( sched->modes, i, 1 );
				DArray_Erase( sched->senders, i, 1 );
				DArray_Erase( sched->futures, i, 1 );
				DArray_Erase( sched->lifepoints, i, 1 );
				break;
			}
			DMutex_Unlock( & sched->mutex );
		}else if( hurry ==0 && sched->idleActors->size ){
			/* check if there is a message matchs an actor that is waiting: */
			DMutex_Lock( & sched->mutex );
			for(i=0; i<sched->messages->size; i++){
				msg = sched->messages->items.pVarray[i];
				sender = (DaoVmProcess*) sched->senders->items.pBase[i];
				if( msg->data[0].t != DAO_VMPROCESS ) continue;
				vmp = (DaoVmProcess*) msg->data[0].v.p;
				if( MAP_Find( sched->idleActors, vmp ) ==NULL ) continue;
				if( DString_Size( vmp->mpiData->pidAwaited ) && msg->size >1 ){
					/* the vmp is waiting for messages from a specific vmp */
					p = msg->data[1];
					if( p.t != DAO_STRING ) continue;
					//printf( "%s\n", DString_GetMBS( p.v.s ) );
					if( DString_Compare( p.v.s, vmp->mpiData->pidAwaited ) !=0 ) continue;
				}
				pactor = (DaoBase*) vmp;
				MAP_Erase( sched->idleActors, vmp );
				DVarray_Swap( message, sched->messages->items.pVarray[i] );
				DVarray_Erase( message, 0, 1 );
				future = (DaoObject*)sched->futures->items.pBase[i];
				mode = sched->modes->items.pInt[i];
				DArray_Erase( sched->messages, i, 1 );
				DArray_Erase( sched->modes, i, 1 );
				DArray_Erase( sched->senders, i, 1 );
				DArray_Erase( sched->futures, i, 1 );
				DArray_Erase( sched->lifepoints, i, 1 );
				break;
			}
			DMutex_Unlock( & sched->mutex );
		}
		//printf( "%p pactor 2 = %p\n", sched, pactor );
		/* if no waiting actor can be activated for execution,
		 * dispatch one message from the queue, and activate its corresponding actor: */
		if( pactor ==NULL && sched->messages->size ){
			// search for one message for an actor that is not busy.
			DMutex_Lock( & sched->mutex );
			for(i=0; i<sched->messages->size; i++){
				msg = sched->messages->items.pVarray[i];
				sender = (DaoVmProcess*) sched->senders->items.pBase[i];
				p = msg->data[0];
				if( p.t == DAO_CONTEXT || p.t == DAO_CDATA || p.t == DAO_VMPROCESS ){
					if( p.t == DAO_VMPROCESS ){
						vmp = (DaoVmProcess*) p.v.p;
						if( DString_Size( vmp->mpiData->pidAwaited ) && msg->size >1 ){
							/* the vmp is waiting for messages from a specific vmp */
							value = msg->data[1];
							if( value.t != DAO_STRING ) continue;
							if( DString_Compare( value.v.s, vmp->mpiData->pidAwaited ) !=0 ) continue;
						}
					}
					/* Check if the actor is processing its previous message:
					 * if yes, postpone this message: */
					pactor = p.v.p;
					if( p.t == DAO_CONTEXT ){
						ctx = p.v.context;
						if( ctx->object ) pactor = (DaoBase*)ctx->object;
					}
					if( MAP_Find( sched->busyActors, pactor ) ){
						if( p.t == DAO_VMPROCESS && p.v.vmp->pauseType == DAO_VMP_NOPAUSE ){
							int lifepoints = sched->lifepoints->items.pInt[i] -1;
							if( lifepoints ==0 ){ /* the life of this message is ended */
								//printf( "message removed\n" );
								DArray_Erase( sched->messages, i, 1 );
								DArray_Erase( sched->modes, i, 1 );
								DArray_Erase( sched->senders, i, 1 );
								DArray_Erase( sched->futures, i, 1 );
								DArray_Erase( sched->lifepoints, i, 1 );
								i --;
							}
						}
						pactor = NULL;
						continue;
					}else{
						MAP_Insert( sched->busyActors, pactor, 0 );
					}
				}
				pactor = p.v.p;
				GC_IncRC( pactor ); //XXX
				DVarray_Swap( message, sched->messages->items.pVarray[i] );
				DVarray_Erase( message, 0, 1 );
				future = (DaoObject*)sched->futures->items.pBase[i];
				mode = sched->modes->items.pInt[i];
				DArray_Erase( sched->messages, i, 1 );
				DArray_Erase( sched->modes, i, 1 );
				DArray_Erase( sched->senders, i, 1 );
				DArray_Erase( sched->futures, i, 1 );
				DArray_Erase( sched->lifepoints, i, 1 );
				break;
			}
			DMutex_Unlock( & sched->mutex );
		}
		//printf( "pactor 3 = %p\n", pactor );
		if( pactor ==NULL ){
			DMutex_Lock( & sched->mutex );
			timeout = DCondVar_TimedWait( & sched->condv, & sched->mutex, wt );
			DMutex_Unlock( & sched->mutex );
			continue;
		}
		actor.t = pactor->type;
		actor.v.p = pactor;

		npar = message->size;
		pars = message->data;
		vmproc = NULL;
		vmp = NULL;
		/*
		   printf( "actor = %i, npar = %i, %p -------------\n", actor.t, npar, actor.v.p );
		 */
		//printf( "future ==================== : %p %i\n", self->future, actor.t );
		switch( actor.t ){
		case DAO_CONTEXT :
			//printf( "context========================\n" );
			vmp = DaoVmProcess_New( sched->vmSpace );
			//MAP_Insert( self->sender->mpiData->asynCalls, (void*)vmp, 0 );
			if( ( mode & DAO_CALL_JOIN ) && sender ) sender->mpiData->asynJoin = 1;
			vmproc = vmp;
			GC_IncRC( vmp );
			DaoVmProcess_PushContext( vmp, actor.v.context );
			DaoVmProcess_Execute(  vmp );
			break;
		case DAO_FUNCTION :
			vmproc = self->vmproc;
			//MAP_Insert( self->sender->mpiData->asynCalls, (void*)vmproc, 0 );
			if( ( mode & DAO_CALL_JOIN ) && sender ) sender->mpiData->asynJoin = 1;
			self->context->thisFunction = actor.v.func;
			actor.v.func->pFunc( self->context, pars, npar );
			break;
		case DAO_VMPROCESS :
			vmproc = (DaoVmProcess*)actor.v.p;
			DaoVmProcess_Resume( vmproc, pars, npar, NULL );
			break;
		default : break;
		}
		if( vmproc == NULL ){
			printf( "actor (type=%i) not executable!\n", actor.t );
			continue;
		}
		//printf( "finished %p\n", vmproc );
		vmproc->mpiData->asynCreator = sender;
		vmproc->mpiData->future = future;
		GC_IncRC( sender );
		GC_IncRC( future );
		/*
		   printf( "future2 =============================== : %p\n", self->mpiData->future );
		   printf( "future : %p %i %p\n", self->future, vmproc->parYield->size, vmproc );
		 */
		DaoVmProcess_PrintException( vmproc, 1 );
		if( future && vmproc->status==DAO_VMPROC_FINISHED ){
			DValue_Copy( future->objData->data + 1, vmproc->returned );
			GC_DecRC( future );
		}

		/*
		   printf( "size : %p, %i\n", sender, sender ? sender->mpiData->asynCalls->size : 0 );
		   printf( "status : %p %i\n", vmproc, vmproc->status );
		 */
		if( sender && vmproc->status==DAO_VMPROC_FINISHED ){
			sender->mpiData->asynCount --;
			//printf( "asynCount %i\n", sender->mpiData->asynCount );
			//MAP_Erase( self->sender->mpiData->asynCalls, (void*)vmproc );
			//printf( "size : %i %i\n", self->sender->mpiData->asynCalls->size, self->sender->mpiData->asynJoin );
			//printf( "actor : %p\n", actor );
			//printf( "********************* : %p %p\n", self->sender, self->sender->mpiData->asynCreator );
			if( sender->mpiData->asynCount ==0 && sender->mpiData->asynJoin ){
				sender->mpiData->asynJoin = 0;
				value.t = sender->type;
				value.v.p = (DaoBase*) sender;
				DVarray_Clear( message );
				DVarray_Append( message, value );
				DaoSched_Send( message, DAO_CALL_ASYNC, sender->mpiData->asynCreator, sender->mpiData->future );
				DVarray_Clear( message );
				//printf( "future3 =============================== : %p\n", self->sender->mpiData->future );
			}
		}
		if( vmp && ( vmp->status==DAO_VMPROC_FINISHED || vmp->status==DAO_VMPROC_ABORTED ) )
			GC_DecRC( vmp );
	}
	DVarray_Delete( message );
}
void DaoSched_Register( DString *name, DaoVmProcess *vmproc )
{
	MAP_Insert( daoTaskData->friendPids, name, vmproc );
}
void DaoTaskData_AddMessage( DaoTaskData *self, DVarray *msg, int mode,
		DaoVmProcess *sender, DaoObject *future )
{
	int pd;
	DString *name;
	DNode *node = NULL;
	DValue p = msg->data[0];

	/*
	   if( msg->size ==0 )
	   printf( "send(): %i, %i, %p\n", p.t, msg->size, p.v.p );
	 */
	if( p.t == DAO_STRING ){
		name = p.v.s;
		DString_ToMBS( name );
		//printf( "Sched_Send(): name = %s\n", name->mbs );
		pd = DString_FindMBS( name, "::", 0 );
		if( pd != MAXSIZE ){
			DString_Substr( name, self->astring, 0, pd );
			DString_Erase( name, 0, pd+2 );
			/* mutex protection XXX */
			node = MAP_Find( self->friendPids, self->astring );
			/*
			   printf( "tmp = %s, %p\n", tmp->mbs, node );
			 */
		}else{
			node = MAP_Find( self->friendPids, name );
			if( node ) DVarray_PopFront( msg );
		}
		/*
		   printf( "name = %s; %i; node = %p\n", name->mbs, msg->size, node );
		 */
		if( node ){
			p.t = node->value.pBase->type;
			p.v.p = node->value.pBase;
			DVarray_PushFront( msg, p );
		}else{
			printf( "actor not found: %s\n", name->mbs );
			/*XXX*/
		}
	}
	/* reference counts should have been increased before this call;
	 * the reference counts are decreased after the actor has processed them. */
	//printf( "send() before lock\n" );
	//printf( "send self = %p %i\n", self, msg->size );
	DMutex_Lock( & self->mutex );
	if( mode & DAO_CALL_HURRY ){
		self->hurryCount ++;
		DArray_PushFront( self->messages, (void*)msg );
		DArray_PushFront( self->modes, (void*)(size_t)mode );
		DArray_PushFront( self->senders, (void*)sender );
		DArray_PushFront( self->futures, (void*)future );
		DArray_PushFront( self->lifepoints, (void*)(size_t)DAO_MESSAGE_LIFE );
	}else{
		DArray_PushBack( self->messages, (void*)msg );
		DArray_PushBack( self->modes, (void*)(size_t)mode );
		DArray_PushBack( self->senders, (void*)sender );
		DArray_PushBack( self->futures, (void*)future );
		DArray_PushBack( self->lifepoints, (void*)(size_t)DAO_MESSAGE_LIFE );
	}
	DCondVar_Signal( & self->condv );
	//printf( "send() after signal\n" );
	DMutex_Unlock( & self->mutex );

	if( (mode & DAO_CALL_HURRY) && self->idleThdCount ==0 ){
		DaoTaskThread *taskthd = DaoTaskThread_New( self );
		DaoThread_Start( taskthd->thread, (DThreadTask) DaoTaskThread_Run, taskthd );
		DMutex_Lock( & self->mutex );
		self->allThdCount ++;
		DMutex_Unlock( & self->mutex );
	}
	//printf( "send() after lock\n" );
	/*
	   printf( "send\n" );
	 */
}
void DaoTaskData_Run( DaoTaskData *self )
{
	int timeout = 0;
	while( self->allThdCount ){
		timeout = 0;
		DMutex_Lock( & self->mutex );
		/*
		   printf( "allThdCount = %i\n", self->allThdCount );
		   printf( "idleThdCount = %i\n", self->idleThdCount );
		 */
		timeout = DCondVar_TimedWait( & self->condv2, & self->mutex, 0.01 );
		DMutex_Unlock( & self->mutex );
		if( timeout && (self->messages->size + self->timeActors->size) ){
			/* if no thread has finished within this time interval,
			 * and there are messages not handled yet,
			 * create a new task thread: */
			DaoTaskThread *taskthd = DaoTaskThread_New( self );
			DaoThread_Start( taskthd->thread, (DThreadTask) DaoTaskThread_Run, taskthd );
			DMutex_Lock( & self->mutex );
			self->allThdCount ++;
			DMutex_Unlock( & self->mutex );
			//printf( "self->allThdCount = %i\n", self->allThdCount );
		}
	}
}

void DaoSched_Init( DaoVmSpace *vms )
{
	int i;
	daoTaskData = DaoTaskData_New( vms, DAO_SCHED_MIN );
	for(i=0; i<DAO_SCHED_MIN; i++){
		DaoTaskThread *taskthd = DaoTaskThread_New( daoTaskData );
		DaoThread_Start( taskthd->thread, (DThreadTask) DaoTaskThread_Run, taskthd );
	}
	DaoThread_Start( daoTaskData->thread, (DThreadTask) DaoTaskData_Run, daoTaskData );
}
void DaoSched_Send( DVarray *msg, int mode, DaoVmProcess *sender, DaoObject *future )
{
	DaoTaskData_AddMessage( daoTaskData, msg, mode, sender, future );
}
void DaoSched_Join( DaoVmSpace *vmSpace )
{
	int nactor = 0;
	if( getenv( "PROC_NAME" ) ) nactor = 1; /*Proxy_Receiver*/
	mainVmProcess = vmSpace->mainProcess;
	//printf( "before join %i\n", nactor );
	DMutex_Lock( & daoTaskData->mutex );
	/*
	   printf( "allThdCount = %i\n", daoTaskData->allThdCount );
	   printf( "idleThdCount = %i\n", daoTaskData->idleThdCount );
	 */
	while( ((daoTaskData->idleThdCount + nactor) < daoTaskData->allThdCount)
			|| (mainVmProcess->status > DAO_VMPROC_ABORTED)
			|| daoTaskData->messages->size || daoTaskData->timeActors->size
			|| daoTaskData->idleActors->size ){
		DCondVar_TimedWait( & daoTaskData->condv3, & daoTaskData->mutex, 0.01 );
		if( daoTaskData->allThdCount ==0 ) break;
	}
	DMutex_Unlock( & daoTaskData->mutex );
	/*
	   printf( "after join\n" );
	 */
}

#endif



#if( defined DAO_WITH_THREAD )

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
