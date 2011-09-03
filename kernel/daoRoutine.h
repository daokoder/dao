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
uint_t         refParams; \
DaoType       *routHost; \
DaoType       *routType; \
DString       *routName; \
DString       *routHelp; \
DString       *parCodes; \
DArray        *routConsts; \
DaoNamespace  *nameSpace

#define ROUT_HOST_TID( t ) ((t)->routHost ? (t)->routHost->tid : 0)

struct DRoutine
{
	DAO_DATA_COMMON;
	DAO_ROUT_COMMON;
};

DRoutine* DRoutine_New();
void DRoutine_CopyFields( DRoutine *self, DRoutine *from );
int  DRoutine_AddConstant( DRoutine *self, DaoValue *value );

/* Return 0 if failed, otherwise return 1 plus number passed parameters: */
int DRoutine_PassParams( DRoutine *rout, DaoValue *obj, DaoValue *recv[], DaoValue *p[], int np, int code );

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

	DArray *simple;

	DMap *localVarType; /* <int,DaoType*> local variable types */

	int mode;

	ushort_t regCount;
	ushort_t bodyStart;
	ushort_t bodyEnd;

	DMap *abstypes;

	DaoRoutine   *original;
	DaoFunctree  *specialized;

	DaoRoutine   *upRoutine;
	DaoProcess   *upContext;
	DaoParser    *parser;
	DaoRoutine   *revised; /* to support edit & continue */

	void *jitData;
};

DaoRoutine* DaoRoutine_New();
DaoRoutine* DaoRoutine_Copy( DaoRoutine *self );
void DaoRoutine_Delete( DaoRoutine *self );

void DaoRoutine_Compile( DaoRoutine *self );
int DaoRoutine_SetVmCodes( DaoRoutine *self, DArray *vmCodes );
void DaoRoutine_SetSource( DaoRoutine *self, DArray *tokens, DaoNamespace *ns );

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

struct DaoFunCurry
{
	DAO_DATA_COMMON;

	DaoValue  *callable;
	DaoValue  *selfobj;
	DArray    *params;
};
DaoFunCurry* DaoFunCurry_New( DaoValue *v, DaoValue *o );

typedef struct DParNode DParNode;

struct DParNode
{
	DaoType  *type;
	DArray   *nexts; /* <DParNode*> */
	DMap     *names; /* <DaoType*,DParNode*> */
	DRoutine *routine;
};
DParNode* DParNode_New();
void DParNode_Delete( DParNode *self );

/* DaoFunctree is a structure to organize overloaded functions into trees,
 * for fast function resolving based on parameter types. */

/* In data structures for namespace and class,
 * each individual function should have its own entry in these structures,
 * and an additional entry of DaoFunctree should be added for overloaded
 * functions. This will simplify some operations such as deriving methods from
 * parent type or instantiating template classes! */

struct DaoFunctree
{
	DAO_DATA_COMMON;

	unsigned int   attribs;
	DaoNamespace  *space;
	DaoType       *host;
	DaoType       *unitype;
	DString       *name;
	DParNode      *tree;
	DParNode      *mtree; /* for routines with self parameter */
	DArray        *routines; /* list of overloaded routines on the trees */
};

DaoFunctree* DaoFunctree_New( DaoNamespace *nameSpace, DString *name );
void DaoFunctree_Delete( DaoFunctree *self );

void DaoFunctree_UpdateVtable( DaoFunctree *self, DRoutine *routine, DMap *vtable );
DRoutine* DaoFunctree_Add( DaoFunctree *self, DRoutine *routine );
DRoutine* DaoFunctree_Lookup( DaoFunctree *self, DaoValue *obj, DaoValue *p[], int n, int code );
DRoutine* DaoFunctree_LookupByType( DaoFunctree *self, DaoType *st, DaoType *t[], int n, int c );
void DaoFunctree_Import( DaoFunctree *self, DaoFunctree *other );
void DaoFunctree_Compile( DaoFunctree *self );

/* Resolve overloaded, virtual and specialized function: */
/* "self" must be one of: DRoutine, DaoRoutine, DaoFunction, DaoFunctree. */
DRoutine* DRoutine_Resolve( DaoValue *self, DaoValue *obj, DaoValue *p[], int n, int code );
DRoutine* DRoutine_ResolveByType( DaoValue *self, DaoType *st, DaoType *t[], int n, int code );

#endif
