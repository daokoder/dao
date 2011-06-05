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

#ifndef DAO_ROUTINE_H
#define DAO_ROUTINE_H

#include"daoType.h"

#define \
	DAO_ROUT_COMMON \
uchar_t        attribs; \
uchar_t        parCount; \
ushort_t       defLine; \
DaoType       *routHost; \
DaoType       *routType; \
DString       *routName; \
DString       *routHelp; \
DString       *parCodes; \
DVarray       *routConsts; \
DaoNameSpace  *nameSpace

#define ROUT_HOST_TID( t ) ((t)->routHost ? (t)->routHost->tid : 0)

struct DRoutine
{
	DAO_DATA_COMMON;
	DAO_ROUT_COMMON;
};

DRoutine* DRoutine_New();
void DRoutine_CopyFields( DRoutine *self, DRoutine *from );
int  DRoutine_AddConst( DRoutine *self, DaoBase *data );
int  DRoutine_AddConstValue( DRoutine *self, DValue value );

/* Return 0 if failed, otherwise return 1 plus number passed parameters: */
int DRoutine_PassParams( DRoutine *rout, DValue *obj, DValue *recv[], DValue *p[], int np, int code );

#define DaoRoutine_AddConst(f,d) DRoutine_AddConst((DRoutine*)(f),(DaoBase*)(d))
#define DaoRoutine_AddConstValue(f,v) DRoutine_AddConstValue((DRoutine*)(f),v)
#define DaoRoutine_PassParams(f,o,r,p,n,c) DRoutine_PassParam((DRoutine*)(f),o,r,p,n,c)

#define DaoFunction_PassParams(f,o,r,p,n,c) DRoutine_PassParam((DRoutine*)(f),o,r,p,n,c)

struct DaoRoutine
{
	DAO_DATA_COMMON;
	DAO_ROUT_COMMON;

	/* virtual machine codes: */
	DaoVmcArray *vmCodes;

	/* modes of each virtual register */
	DString *regMode;

	/* data type for local registers: */
	DArray *regType; /* <DaoType*> */

	/* VM codes with annotations */
	DArray *annotCodes; /* <DaoVmCodeX*> */

	/* definition of local constants and variables: */
	DArray *defLocals; /* <DaoToken*> */
	DArray *source; /* <DaoToken*> */

	DMap *localVarType; /* <int,DaoType*> local variable types */

	int mode;

	ushort_t regCount;
	ushort_t bodyStart;
	ushort_t bodyEnd;

	DMap *abstypes;

	DaoRoutine     *original;
	DaoMetaRoutine *specialized;

	DaoRoutine   *upRoutine;
	DaoContext   *upContext;
	DaoParser    *parser;
	DaoRoutine   *revised; /* to support edit & continue */

	void *jitData;
};

DaoRoutine* DaoRoutine_New();
DaoRoutine* DaoRoutine_Copy( DaoRoutine *self );
void DaoRoutine_Delete( DaoRoutine *self );

void DaoRoutine_Compile( DaoRoutine *self );
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
int DaoFunction_Call( DaoFunction *self, DaoContext *ctx, DValue *obj, DValue *p[], int n );

struct DaoFunCurry
{
	DAO_DATA_COMMON;

	DValue    callable;
	DValue    selfobj;
	DVarray  *params;
};
DaoFunCurry* DaoFunCurry_New( DValue v, DValue o );

typedef struct DMetaParam DMetaParam;

struct DMetaParam
{
	DaoType  *type;
	DArray   *nexts; /* <DMetaParam*> */
	DMap     *names; /* <DaoType*,DMetaParam*> */
	DRoutine *routine;
};
DMetaParam* DMetaParam_New();
void DMetaParam_Delete( DMetaParam *self );

/* DaoMetaRoutine is a structure to organize overloaded functions into trees,
 * for fast function resolving based on parameter types. */

/* In data structures for namespace and class,
 * each individual function should have its own entry in these structures,
 * and an additional entry of DaoMetaRoutine should be added for overloaded
 * functions. This will simplify some operations such as deriving methods from
 * parent type or instantiating template classes! */

struct DaoMetaRoutine
{
	DAO_DATA_COMMON;

	unsigned int   attribs;
	DaoNameSpace  *space;
	DaoType       *host;
	DaoType       *unitype;
	DString       *name;
	DMetaParam    *tree;
	DMetaParam    *mtree; /* for routines with self parameter */
	DArray        *routines; /* list of overloaded routines on the trees */
};

DaoMetaRoutine* DaoMetaRoutine_New( DaoNameSpace *nameSpace, DString *name );
void DaoMetaRoutine_Delete( DaoMetaRoutine *self );

void DaoMetaRoutine_UpdateVtable( DaoMetaRoutine *self, DRoutine *routine, DMap *vtable );
DRoutine* DaoMetaRoutine_Add( DaoMetaRoutine *self, DRoutine *routine );
DRoutine* DaoMetaRoutine_Lookup( DaoMetaRoutine *self, DValue *obj, DValue *p[], int n, int code );
DRoutine* DaoMetaRoutine_LookupByType( DaoMetaRoutine *self, DaoType *st, DaoType *t[], int n, int c );
void DaoMetaRoutine_Import( DaoMetaRoutine *self, DaoMetaRoutine *other );
void DaoMetaRoutine_Compile( DaoMetaRoutine *self );

/* Resolve overloaded, virtual and specialized function: */
/* "self" must be one of: DRoutine, DaoRoutine, DaoFunction, DaoMetaRoutine. */
DRoutine* DRoutine_Resolve( DaoBase *self, DValue *obj, DValue *p[], int n, int code );
DRoutine* DRoutine_ResolveByType( DaoBase *self, DaoType *st, DaoType *t[], int n, int code );

#endif
