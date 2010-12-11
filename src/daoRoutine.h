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

#ifndef DAO_ROUTINE_H
#define DAO_ROUTINE_H

#include"daoType.h"

typedef unsigned int bits_t;

#define \
	DAO_ROUT_COMMON \
uchar_t        attribs; \
uchar_t        minimal; \
uchar_t        minParam; \
uchar_t        parCount; \
ushort_t       defLine; \
uchar_t        tidHost; \
DaoType       *routHost; \
DaoType       *routType; \
DString       *routName; \
DString       *routHelp; \
DString       *parCodes; \
DVarray       *routConsts; \
DArray        *routTable; \
DaoNameSpace  *nameSpace

struct DRoutine
{
	DAO_DATA_COMMON;
	DAO_ROUT_COMMON;
};

DRoutine* DRoutine_New();
void DRoutine_CopyFields( DRoutine *self, DRoutine *from );
void DRoutine_AddOverLoad( DRoutine *self, DRoutine *rout );
int  DRoutine_AddConst( DRoutine *self, DaoBase *data );
int  DRoutine_AddConstValue( DRoutine *self, DValue value );

DRoutine* DRoutine_GetOverLoadByType( DRoutine *self, DaoType *type );
DRoutine* DRoutine_GetOverLoad( DRoutine *self, DValue *obj, DValue *p[], int n, int code );

int DRoutine_PassParams( DRoutine *rout, DValue *obj, DValue *recv[], DValue *p[], DValue *base, int np, int code );
int DRoutine_FastPassParams( DRoutine *routine, DValue *obj, DValue *recv[], DValue *p[], DValue *base, int np, int code );

#define DaoRoutine_AddConst(f,d) DRoutine_AddConst((DRoutine*)(f),(DaoBase*)(d))
#define DaoRoutine_AddConstValue(f,v) DRoutine_AddConstValue((DRoutine*)(f),v)
#define DaoRoutine_GetOverLoad(f,o,p,n,c) (DaoRoutine*)DRoutine_GetOverLoad((DRoutine*)(f),o,p,n,c)
#define DaoRoutine_PassParams(f,o,r,p,b,n,c) DRoutine_PassParam((DRoutine*)(f),o,r,p,b,n,c)
#define DaoRoutine_FastPassParams(f,o,r,p,b,n,c) DRoutine_FastPassParam((DRoutine*)(f),o,r,p,b,n,c)

#define DaoFunction_GetOverLoad(f,o,p,n,c) (DaoFunction*)DRoutine_GetOverLoad((DRoutine*)(f),o,p,n,c)
#define DaoFunction_PassParams(f,o,r,p,b,n,c) DRoutine_PassParam((DRoutine*)(f),o,r,p,b,n,c)
#define DaoFunction_FastPassParams(f,o,r,p,b,n,c) DRoutine_FastPassParam((DRoutine*)(f),o,r,p,b,n,c)

struct DaoRoutine
{
	DAO_DATA_COMMON;
	DAO_ROUT_COMMON;

	/* virtual machine codes: */
	DaoVmcArray *vmCodes;

	/* data type for local registers: */
	DArray *regType; /* <DaoType*> */

	/* VM codes with annotations */
	DArray *annotCodes; /* <DaoVmCodeX*> */

	/* definition of local constants and variables: */
	DArray *defLocals; /* <DaoToken*> */
	DArray *source; /* <DaoToken*> */

	DMap *localVarType; /* <int,DaoType*> local variable types */

	int mode;

	ushort_t locRegCount;
	ushort_t constParam;
	int bodyStart;
	int bodyEnd;

	DMap *abstypes;

	DaoRoutine *original;
	DArray     *specialized;

	DaoRoutine *upRoutine;
	DaoContext *upContext;
	DaoParser  *parser;
	DaoRoutine *revised; /* to support edit & continue */

#ifdef DAO_WITH_JIT
	DArray *binCodes; /* <DString*>: compiled machince codes */
	DArray *jitFuncs; /* <void*>: executable machine codes as function pointers */
	DaoVmcArray *preJit; /* original VM codes */
	DaoJitMemory *jitMemory;
#endif
};

DaoRoutine* DaoRoutine_New();
DaoRoutine* DaoRoutine_Copy( DaoRoutine *self, int overload );
void DaoRoutine_Delete( DaoRoutine *self );

void DaoRoutine_Compile( DaoRoutine *self );
void DaoRoutine_AddOverLoad( DaoRoutine *self, DaoRoutine *rout );
int DaoRoutine_SetVmCodes( DaoRoutine *self, DArray *vmCodes );

void DaoRoutine_SetSource( DaoRoutine *self, DArray *tokens, DaoNameSpace *ns );

void DaoRoutine_PrintCode( DaoRoutine *self, DaoStream *stream );

/* XXX gc */
struct DaoFunction
{
	DAO_DATA_COMMON;
	DAO_ROUT_COMMON;

	DaoFuncPtr   pFunc;

	void  *ffiData; /* Data for Forign Function Interface, for DaoCLoader module */
};

extern DaoFunction* DaoFunction_New();
void DaoFunction_Delete( DaoFunction *self );
DaoFunction* DaoFunction_Copy( DaoFunction *self, int overload );

void DaoFunction_SimpleCall( DaoFunction *self, DaoContext *ctx, DValue *p[], int N );
int DaoFunction_Call( DaoFunction *func, DaoCData *self, DValue *p[], int n );

struct DaoFunCurry
{
	DAO_DATA_COMMON;

	DValue    callable;
	DValue    selfobj;
	DVarray  *params;
};
DaoFunCurry* DaoFunCurry_New( DValue v, DValue o );

#endif
