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


#define ROUT_HOST_TID( t ) ((t)->routHost ? (t)->routHost->tid : 0)


/* Two types of specializatins may happen to a routine:
 * 1. Method Specialization (MS) for specialized template-like types;
 * 2. Parametric Specialization (PS) according to parameter types;
 *
 * For C methods specialization of cdata types, only the routine type
 * needs specialization.
 *
 * For Dao routines, only the original routines have type inference done
 * at compiling time. Routine specialization on parameters at compiling time
 * is only done for the routine type (DaoRoutine::routType), such shallowly
 * specialized routine will share the same routine body (DaoRoutine::body)
 * as the original one. Deep specialization with type inference can be performed
 * at runtime.
 */
struct DaoRoutine
{
	DAO_DATA_COMMON;

	uchar_t          attribs;
	uchar_t          parCount; /* number of parameters that can be accepted; */
	ushort_t         defLine;  /* definition line number in the source file; */
	uint_t           refParams; /* bit flags for reference parameters; */
	DArray          *defaults; /* default parameters; */
	DString         *routName; /* routine name; */
	DaoType         *routType; /* routine type; */
	DaoType         *routHost; /* host type, for routine that is a method; */
	DaoRoutine      *original; /* the original routine of a PS specialized one; */
	DaoRoutree      *specialized; /* specialization based on parameters; */
	DaoNamespace    *nameSpace; /* definition namespace; */
	DaoRoutineBody  *body; /* data for Dao routines; */
	DaoFuncPtr       pFunc;
};

DaoRoutine* DaoRoutine_New();
DaoRoutine* DaoRoutine_Copy( DaoRoutine *self );
void DaoRoutine_Delete( DaoRoutine *self );
void DaoRoutine_CopyFields( DaoRoutine *self, DaoRoutine *from );
int  DaoRoutine_AddConstant( DaoRoutine *self, DaoValue *value );

void DaoRoutine_Compile( DaoRoutine *self );
int DaoRoutine_SetVmCodes( DaoRoutine *self, DArray *vmCodes );
void DaoRoutine_SetSource( DaoRoutine *self, DArray *tokens, DaoNamespace *ns );

void DaoRoutine_PrintCode( DaoRoutine *self, DaoStream *stream );


struct DaoRoutineBody
{
	DAO_DATA_COMMON;

	/* virtual machine codes: */
	DaoVmcArray *vmCodes;

	DArray *routConsts;

	/* data type for local registers: */
	DArray *regType; /* <DaoType*> */

	/* VM codes with annotations */
	DArray *annotCodes; /* <DaoVmCodeX*> */

	/* definition of local constants and variables: */
	DArray *defLocals; /* <DaoToken*> */
	DArray *source; /* <DaoToken*> */

	DArray *simpleVariables;

	DMap *localVarType; /* <int,DaoType*> local variable types */

	int mode;

	ushort_t regCount;
	ushort_t bodyStart;
	ushort_t bodyEnd;

	DMap *abstypes;

	DaoRoutine   *upRoutine;
	DaoProcess   *upProcess;
	DaoParser    *parser;
	DaoRoutine   *revised; /* to support edit & continue */

	void *jitData;
};

DaoRoutineBody* DaoRoutineBody_New();
DaoRoutineBody* DaoRoutineBody_Copy( DaoRoutineBody *self );
void DaoRoutineBody_Delete( DaoRoutineBody *self );



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

/* DaoRoutree is a structure to organize overloaded functions into trees (tries),
 * for fast function resolving based on parameter types. */

/* In data structures for namespace and class,
 * each individual function should have its own entry in these structures,
 * and an additional entry of DaoRoutree should be added for overloaded
 * functions. This will simplify some operations such as deriving methods from
 * parent type or instantiating template classes! */

struct DaoRoutree
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

DaoRoutree* DaoRoutree_New( DaoNamespace *nameSpace, DString *name );
void DaoRoutree_Delete( DaoRoutree *self );

void DaoRoutree_UpdateVtable( DaoRoutree *self, DRoutine *routine, DMap *vtable );
DRoutine* DaoRoutree_Add( DaoRoutree *self, DRoutine *routine );
DRoutine* DaoRoutree_Lookup( DaoRoutree *self, DaoValue *obj, DaoValue *p[], int n, int code );
DRoutine* DaoRoutree_LookupByType( DaoRoutree *self, DaoType *st, DaoType *t[], int n, int c );
void DaoRoutree_Import( DaoRoutree *self, DaoRoutree *other );
void DaoRoutree_Compile( DaoRoutree *self );

/* Resolve overloaded, virtual and specialized function: */
/* "self" must be one of: DRoutine, DaoRoutine, DaoFunction, DaoRoutree. */
DRoutine* DRoutine_Resolve( DaoValue *self, DaoValue *obj, DaoValue *p[], int n, int code );
DRoutine* DRoutine_ResolveByType( DaoValue *self, DaoType *st, DaoType *t[], int n, int code );

#endif
