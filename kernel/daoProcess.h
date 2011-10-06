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

#ifndef DAO_PROCESS_H
#define DAO_PROCESS_H

#include"daoVmcode.h"
#include"daoType.h"
#include"time.h"
#include"stdlib.h"

#define DVM_MAKE_OBJECT (1<<5)
#define DVM_FRAME_SECT  (1<<6)
#define DVM_FRAME_KEEP  (1<<7)

#define DVM_MAX_TRY_DEPTH 16

struct DaoStackFrame
{
	ushort_t    entry;     /* entry code id */
	ushort_t    state;     /* frame state */
	ushort_t    returning; /* return register id */
	ushort_t    depth; /* depth of exception scopes */
	ushort_t    ranges[DVM_MAX_TRY_DEPTH][2]; /* ranges of exception scopes */

	ushort_t      parCount;
	size_t        stackBase;
	DaoType     **types;
	DaoVmCode    *codes; /* = routine->vmCodes->codes */
	DaoRoutine   *routine;
	DaoFunction  *function;
	DaoObject    *object;
	DaoProcess   *outer;

	DaoStackFrame  *active;
	DaoStackFrame  *sect; /* original frame of a code section frame */
	DaoStackFrame  *prev;
	DaoStackFrame  *next;
};

/*
   The stack structure of a Dao virtual machine process:

   1. The call/stack frames are organized into a linked list structure;

   2. The first frame is auxialiary frame that contains one stack value,
   which will be used to hold the returned value of the process (when the 
   DVM_RETURN instruction is executed while the_current_frame->returning==-1); 

   3. The stack values are stored in a dynamic array which can grow when
   a new frame is pushed into the stack;

   4. When the value stack grows, it must have extra space that can hold
   the maximum number of parameters (namely, @stackSize > @stackTop + DAO_MAX_PARAM); 

   5. When a Dao function or C function is called, the parameters must be
   passed to the stack values starting from @stackTop, then a new frame can
   be push. In this way, it will avoid of the problem of invalidating some of
   the pointers when the stack is growed;

   6. After the value stack is expanded, the expanded part should be set to zero;
   the rest should be kept intact. The values from @stackTop to @stackSize can be
   collected when it is convenient, not each time when a frame is popped off.
 */

struct DaoProcess
{
	DAO_DATA_COMMON;

	DaoVmSpace *vmSpace;

	DaoStackFrame *firstFrame; /* the first frame */
	DaoStackFrame *topFrame; /* top call frame */

	DaoVmCode     *activeCode;
	DaoRoutine    *activeRoutine;
	DaoObject     *activeObject;
	DaoNamespace  *activeNamespace;

	DaoType   **activeTypes;
	DaoValue  **activeValues;

	DaoValue  **freeValues; /* = stackValues + stackTop */
	DaoValue  **stackValues;
	size_t      stackSize; /* maximum number of values that can be hold by stackValues; */
	size_t      stackTop; /* one past the last active stack value; */

	DaoType  *abtype; /* for coroutine */
	DArray   *parResume;/* for coroutine */
	DArray   *parYield;
	DArray   *exceptions;

	char pauseType;
	char status;
	char stopit;

	DaoFuture *future;

	DaoType   *dummyType;
	DaoVmCode  dummyCode;

	DString *mbstring;
	DMap    *mbsRegex; /* <DString*,DString*> */
	DMap    *wcsRegex; /* <DString*,DString*> */
};

/* Create a new virtual machine process */
DaoProcess* DaoProcess_New( DaoVmSpace *vms );
void DaoProcess_Delete( DaoProcess *self );

DaoStackFrame* DaoProcess_PushFrame( DaoProcess *self, int size );
DaoStackFrame* DaoProcess_PushSectionFrame( DaoProcess *self );
void DaoProcess_PopFrame( DaoProcess *self );
void DaoProcess_PopFrames( DaoProcess *self, DaoStackFrame *rollback );

void DaoProcess_InitTopFrame( DaoProcess *self, DaoRoutine *routine, DaoObject *object );
void DaoProcess_SetActiveFrame( DaoProcess *self, DaoStackFrame *frame );

void DaoProcess_PushRoutine( DaoProcess *self, DaoRoutine *routine, DaoObject *object );
void DaoProcess_PushFunction( DaoProcess *self, DaoFunction *function );
int DaoProcess_PushCallable( DaoProcess *self, DaoValue *M, DaoValue *O, DaoValue *P[], int N );

void DaoProcess_InterceptReturnValue( DaoProcess *self );

int DaoProcess_Call( DaoProcess *self, DaoMethod *f, DaoValue *o, DaoValue *p[], int n );

void DaoProcess_CallFunction( DaoProcess *self, DaoFunction *func, DaoValue *p[], int n );

/* Execute from the top of the calling stack */
int DaoProcess_Execute( DaoProcess *self );

DaoValue* DaoProcess_SetValue( DaoProcess *self, ushort_t reg, DaoValue *value );

DaoProcess* DaoProcess_Create( DaoProcess *self, DaoValue *par[], int N );

/* Resume a coroutine */
/* coroutine.yeild( a, b, ... ); store object a,b,... in "DaoList *list"
 * 
 * param = coroutine.resume( corout, a, b, ... ); pass "DaoValue par[]" as a,b,...
 * they become addition result from yeild().
 */
int DaoProcess_Resume( DaoProcess *self, DaoValue *par[], int N, DaoList *list );
void DaoProcess_Yield( DaoProcess *self, DaoValue *par[], int N, DaoList *list );

void DaoProcess_PrintException( DaoProcess *self, int clear );

DaoValue* DaoProcess_MakeConst( DaoProcess *self );


typedef struct CastBuffer CastBuffer;
struct CastBuffer
{
	DLong    *lng;
	DString  *str;
};


typedef struct DaoJIT DaoJIT;
typedef void (*DaoJIT_InitFPT)( DaoVmSpace*, DaoJIT* );
typedef void (*DaoJIT_QuitFPT)();
typedef void (*DaoJIT_FreeFPT)( DaoRoutine *routine );
typedef void (*DaoJIT_CompileFPT)( DaoRoutine *routine );
typedef void (*DaoJIT_ExecuteFPT)( DaoProcess *context, int jitcode );

struct DaoJIT
{
	void (*Quit)();
	void (*Free)( DaoRoutine *routine );
	void (*Compile)( DaoRoutine *routine );
	void (*Execute)( DaoProcess *context, int jitcode );
};

extern struct DaoJIT dao_jit;

#endif
