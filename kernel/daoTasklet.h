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

#ifndef DAO_TASKLET_H
#define DAO_TASKLET_H

#include"daoVmspace.h"


enum DaoTaskletStatus
{
	DAO_TASKLET_RUNNING ,
	DAO_TASKLET_PAUSED ,
	DAO_TASKLET_FINISHED ,
	DAO_TASKLET_ABORTED
};


/*
// Channel for synchronous and asynchronous communication between tasklet.
//
// Each channel has a data buffer that holds data send to the channel.
// Each buffer has a cap/capacity limit, and if the number of data items
// has reached the cap, a sender will block when it sends data to this channel.
//
// Each time a receiver of a channel reads out one data item of the
// channel buffer, which will effectively move the rest data items
// forward and may cause one data item to enter the cap region.
// When this happens, the sender of this data item will be unblocked.
//
// If the buffer cap is zero, it will effectively block any sender,
// which is unblock only when the data item it sent has been read out.
*/
struct DaoChannel
{
	DAO_CSTRUCT_COMMON;

	daoint   cap;     /* capacity limit of the channel; */
	DList   *buffer;  /* DList<DaoValue*>; */
};



/*
// Future value for tasklet.
//
// Each tasklet is represented by a future value.
*/
struct DaoFuture
{
	DAO_CSTRUCT_COMMON;

	uchar_t      state;
	uchar_t      timeout;
	uchar_t      aux1;
	uchar_t      aux2;
	DaoValue    *value;
	DaoValue    *message;
	DaoValue    *selected;
	DaoObject   *actor;
	DaoProcess  *process;
	DaoFuture   *precond; /* the future value on which this one waits; */
};

DAO_DLL DaoFuture*  DaoFuture_New( DaoNamespace *ns, DaoType *type, int vatype );


DAO_DLL void DaoVmSpace_AddTaskletCall( DaoVmSpace *self, DaoProcess *call );

#ifdef DAO_WITH_CONCURRENT

DAO_DLL DaoChannel* DaoChannel_New( DaoNamespace *ns, DaoType *type, int dtype );

DAO_DLL void DaoFuture_ActivateEvent( DaoFuture *self, DaoVmSpace *vmspace );

DAO_DLL void DaoProcess_MarkActiveTasklet( DaoProcess *self, int active );

DAO_DLL void DaoProcess_ReturnFutureValue( DaoProcess *self, DaoFuture *future );

DAO_DLL int  DaoVmSpace_GetThreadCount( DaoVmSpace *self );
DAO_DLL void DaoVmSpace_JoinTasklets( DaoVmSpace *self );
DAO_DLL void DaoVmSpace_StopTasklets( DaoVmSpace *self );

/*
// If "proc" is not NULL, obtain (or create) a thread exclusively for
// a tasklet that is identified by "proc" (virtual process).
// The thread will become available to other tasklets only after this
// tasklet is completed, namely, when proc->status = DAO_PROCESS_FINISHED
// or proc->status = DAO_PROCESS_ABORTED after DaoProcess_Start(proc).
// the virtual process "proc".
*/
DAO_DLL void DaoVmSpace_AddTaskletThread( DaoVmSpace *self, DThreadTask func, void *param, void *proc );
DAO_DLL void DaoVmSpace_AddTaskletJob( DaoVmSpace *self, DThreadTask func, void *param, void *proc );
DAO_DLL void DaoVmSpace_AddTaskletWait( DaoVmSpace *self, DaoProcess *wait, DaoFuture *future, double timeout );

#endif

#endif
