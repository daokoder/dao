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

#define DVM_MAKE_OBJECT (1<<6)
#define DVM_SPEC_RUN (1<<7)

#define DVM_MAX_TRY_DEPTH 16

struct DaoStackFrame
{
	ushort_t    entry;     /* entry code id */
	ushort_t    state;     /* context state */
	ushort_t    returning; /* return register id */
	ushort_t    depth;
	ushort_t    ranges[DVM_MAX_TRY_DEPTH][2];

	size_t      stackBase;
	DaoRoutine *routine;
	DaoContext *context;

	DaoStackFrame *prev;
	DaoStackFrame *next;
	DaoStackFrame *rollback;
};

struct DaoProcess
{
	DAO_DATA_COMMON;

	DaoVmSpace *vmSpace;

	DaoStackFrame *firstFrame; /* the first frame, never active */
	DaoStackFrame *topFrame; /* top call frame */

	DaoValue  **stackValues;
	size_t      stackSize;
	size_t      stackTop;

	DaoValue *returned;
	DaoType  *abtype; /* for coroutine */
	DArray   *parResume;/* for coroutine */
	DArray   *parYield;
	DArray   *exceptions;

	char pauseType;
	char status;
	char stopit;

	DaoFuture *future;

	DString *mbstring;
	DMap    *mbsRegex; /* <DString*,DString*> */
	DMap    *wcsRegex; /* <DString*,DString*> */
};

/* Create a new virtual machine process */
DaoProcess* DaoProcess_New( DaoVmSpace *vms );
void DaoProcess_Delete( DaoProcess *self );

/* Push a routine into the calling stack of the VM process, new context is created */
void DaoProcess_PushRoutine( DaoProcess *self, DaoRoutine *routine );
/* Push an initialized context into the calling stack of the VM process */
void DaoProcess_PushContext( DaoProcess *self, DaoContext *context );
DaoContext* DaoProcess_MakeContext( DaoProcess *self, DaoRoutine *routine );
void DaoProcess_PopContext( DaoProcess *self );

int DaoProcess_Call( DaoProcess *self, DaoMethod *f, DaoValue *o, DaoValue *p[], int n );
/* Execute from the top of the calling stack */
int DaoProcess_Execute( DaoProcess *self );
int DaoProcess_ExecuteSection( DaoProcess *self, int entry );

DaoProcess* DaoProcess_Create( DaoContext *ctx, DaoValue *par[], int N );

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


typedef struct DaoJIT DaoJIT;
typedef void (*DaoJIT_InitFPT)( DaoVmSpace*, DaoJIT* );
typedef void (*DaoJIT_QuitFPT)();
typedef void (*DaoJIT_FreeFPT)( DaoRoutine *routine );
typedef void (*DaoJIT_CompileFPT)( DaoRoutine *routine );
typedef void (*DaoJIT_ExecuteFPT)( DaoContext *context, int jitcode );

struct DaoJIT
{
	void (*Quit)();
	void (*Free)( DaoRoutine *routine );
	void (*Compile)( DaoRoutine *routine );
	void (*Execute)( DaoContext *context, int jitcode );
};

extern struct DaoJIT dao_jit;

#endif
