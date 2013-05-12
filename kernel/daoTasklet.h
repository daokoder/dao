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

#ifndef DAO_SCHED_H
#define DAO_SCHED_H

#include"daoVmspace.h"



enum DaoTaskEventType
{
	DAO_EVENT_NONE ,
	DAO_EVENT_START_TASKLET  ,  /* Start new tasklet; */
	DAO_EVENT_WAIT_TASKLET   ,  /* Wait for another tasklet; */
	DAO_EVENT_WAIT_RECEIVING ,  /* Wait for receiving from a channel; */
	DAO_EVENT_WAIT_SENDING   ,  /* Wait after sending to a channel; */
	DAO_EVENT_QUEUE_MESSAGE  ,  /* Queue message for processing; */
	DAO_EVENT_HANDLE_MESSAGE
};

enum DaoTaskStatus
{
	DAO_CALL_RUNNING ,
	DAO_CALL_PAUSED ,
	DAO_CALL_FINISHED
};

enum DaoFutureResultType
{
	DAO_FUTRES_NONE ,
	DAO_FUTRES_STATUS ,
	DAO_FUTRES_VALUE
};


typedef struct DaoTaskEvent  DaoTaskEvent;

/*
// Task event for scheduling.
//
// A task event can be generated in different situation:
// 1. Starting of a new tasklet by calling mt.start::{} or asynchronous methods:
//    DaoTaskEvent {
//        type = DAO_EVENT_START_TASKLET;
//        state = DAO_CALL_PAUSED;
//        future = future value for the new tasklet;
//        channel = NULL;
//        value = NULL;
//    };
// 2. Waiting for a tasklet (future value):
//    DaoTaskEvent {
//        type = DAO_EVENT_WAIT_TASKLET;
//        state = DAO_CALL_PAUSED;
//        future = future value for the waiting tasklet;
//        channel = NULL;
//        value = NULL;
//    };
// 3. Waiting to Receive message from a channel:
//    DaoTaskEvent {
//        type = DAO_EVENT_WAIT_RECEIVING;
//        state = DAO_CALL_PAUSED;
//        future = future value for the waiting tasklet;
//        channel = channel for receiving;
//        value = NULL;
//    };
// 4. Waiting after sending message to a channel:
//    DaoTaskEvent {
//        type = DAO_EVENT_WAIT_SENDING;
//        state = DAO_CALL_PAUSED;
//        future = future value for the sending tasklet;
//        channel = channel for sending;
//        value = NULL;
//    };
// 5. Queuing message sent to a channel:
//    DaoTaskEvent {
//        type = DAO_EVENT_QUEUE_MESSAGE;
//        state = DAO_CALL_PAUSED;
//        future = future value for the sending tasklet;
//        channel = channel for sending;
//        value = data for sending;
//    };
//
// Note for channel:
// -- Messages sent to a channel are also queued in both in the channel
//    buffer and the event list as event of type DAO_EVENT_QUEUE_MESSAGE.
// -- When an event of type DAO_EVENT_WAIT_RECEIVING is processed,
//    the channel buffer is checked, and the first event/message will
//    be taken and updated to an event of type DAO_EVENT_HANDLE_MESSAGE,
//    with the sender being changed to receiver.
*/
struct DaoTaskEvent
{
	ushort_t       type;
	ushort_t       state;
	DaoFuture     *future;
	DaoChannel    *channel;
	DaoValue      *value;
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

	daoint       cap;     /* capacity limit of the channel; */
	DArray      *buffer;  /* DArray<DaoTaskEvent*>; */
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
	uchar_t      restype;
	DaoValue    *value;
	DaoValue    *message;
	DaoObject   *actor;
	DaoProcess  *process;
	DaoFuture   *precondition; /* the future value on which this one waits; */
};


#ifdef DAO_WITH_CONCURRENT

DAO_DLL DaoType *dao_type_channel;
DAO_DLL DaoType *dao_type_future;
DAO_DLL DaoChannel* DaoChannel_New( DaoType *type, int dtype );
DAO_DLL DaoFuture*  DaoFuture_New( DaoType *type, int vatype );
DAO_DLL void DaoProcess_ReturnFutureValue( DaoProcess *self, DaoFuture *future );


DAO_DLL void DaoCallServer_Join();
DAO_DLL void DaoCallServer_Stop();
DAO_DLL void DaoCallServer_AddTask( DThreadTask func, void *param, int now );
DAO_DLL void DaoCallServer_AddWait( DaoProcess *wait, DaoFuture *future, double timeout, int restype );
DAO_DLL void DaoCallServer_AddCall( DaoProcess *call );
#endif

#endif
