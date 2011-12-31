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

typedef struct DaoRoutineBody DaoRoutineBody;


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
 *
 * For Parametric Specialization, routine constants can be reused.
 */
struct DaoRoutine
{
	DAO_DATA_COMMON;

	uchar_t          attribs;
	uchar_t          parCount; /* number of parameters that can be accepted; */
	ushort_t         defLine;  /* definition line number in the source file; */
	uint_t           refParams; /* bit flags for reference parameters; */
	DString         *routName; /* routine name; */
	DaoType         *routType; /* routine type; */
	DaoType         *routHost; /* host type, for routine that is a method; */
	DaoList         *routConsts; /* default parameters and routine constants; */
	DaoNamespace    *nameSpace; /* definition namespace; */

	DaoRoutineBody  *body; /* data for Dao routines; */
	DaoFuncPtr       pFunc;

	DaoRoutine      *original; /* the original routine of a PS specialized one; */
	DRoutines       *specialized; /* specialization based on parameters; */
	DRoutines       *overloads; /* overloaded routines; */
};

DaoRoutine* DaoRoutine_New( DaoNamespace *nspace, DaoType *host, int body );
DaoRoutine* DaoRoutines_New( DaoNamespace *nspace, DaoType *host, DaoRoutine *init );
DaoRoutine* DaoRoutine_Copy( DaoRoutine *self, int copy_const, int copy_body );
void DaoRoutine_CopyFields( DaoRoutine *self, DaoRoutine *from, int copy_body, int copy_const );
void DaoRoutine_Delete( DaoRoutine *self );
int  DaoRoutine_AddConstant( DaoRoutine *self, DaoValue *value );

void DaoRoutine_Compile( DaoRoutine *self );
int DaoRoutine_SetVmCodes( DaoRoutine *self, DArray *vmCodes );
void DaoRoutine_SetSource( DaoRoutine *self, DArray *tokens, DaoNamespace *ns );

void DaoRoutine_PrintCode( DaoRoutine *self, DaoStream *stream );
int DaoRoutine_DoTypeInference( DaoRoutine *self, int silent );


struct DaoRoutineBody
{
	DAO_DATA_COMMON;

	/* virtual machine codes: */
	DaoVmcArray *vmCodes;

	/* data type for local registers: */
	DArray *regType; /* <DaoType*> */

	/* VM codes with annotations */
	DArray *annotCodes; /* <DaoVmCodeX*> */

	/* definition of local constants and variables: */
	DArray *defLocals; /* <DaoToken*> */
	DArray *source; /* <DaoToken*> */

	DArray *simpleVariables;

	DString *routHelp;

	DMap *localVarType; /* <int,DaoType*> local variable types */

	int mode;

	ushort_t regCount;
	ushort_t codeStart;
	ushort_t codeEnd;

	DMap *abstypes;

	DaoRoutine  *upRoutine;
	DaoProcess  *upProcess;
	DaoParser   *parser;
	DaoRoutine  *revised; /* to support edit & continue */

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



typedef struct DParamNode DParamNode;

struct DParamNode
{
	DaoType     *type;    /* type of the parameter node; */
	DaoType     *type2;   /* name + type; */
	DaoRoutine  *routine; /* routine of a leaf node; */
	DParamNode  *first;   /* first child node; */
	DParamNode  *last;    /* last child node; */
	DParamNode  *next;    /* next sibling node; */
};

/* DRoutines is a structure to organize overloaded/specialized functions into trees (tries),
 * for fast function resolving based on parameter types. */

/* In data structures for namespace and class,
 * each individual function should have its own entry in these structures,
 * and an additional entry of DRoutines should be added for overloaded
 * functions. This will simplify some operations such as deriving methods from
 * parent type or instantiating template classes! */

struct DRoutines
{
	unsigned int   attribs;
	DParamNode    *tree;
	DParamNode    *mtree; /* for routines with self parameter */
	DArray        *routines; /* list of overloaded routines on the trees */
};

DRoutines* DRoutines_New();
void DRoutines_Delete( DRoutines *self );

DaoRoutine* DRoutines_Add( DRoutines *self, DaoRoutine *routine );
void DRoutines_Import( DRoutines *self, DRoutines *other );
void DRoutines_Compile( DRoutines *self );


/* Resolve overloaded, virtual and specialized function: */
DaoRoutine* DaoRoutine_ResolveX( DaoRoutine *self, DaoValue *obj, DaoValue *p[], int n, int code );
DaoRoutine* DaoRoutine_ResolveByType( DaoRoutine *self, DaoType *st, DaoType *t[], int n, int code );
void DaoRoutine_UpdateVtable( DaoRoutine *self, DaoRoutine *routine, DMap *vtable );

#endif
