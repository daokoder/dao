/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2012, Limin Fu
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

#ifndef DAO_ROUTINE_H
#define DAO_ROUTINE_H

#include"daoLexer.h"
#include"daoType.h"


#define ROUT_HOST_TID( t ) ((t)->routHost ? (t)->routHost->tid : 0)

typedef struct DaoRoutineBody DaoRoutineBody;


/*
// Two types of specializatins may happen to a routine:
// 1. Method Specialization (MS) for specialized template-like types;
// 2. Parametric Specialization (PS) according to parameter types;
//
// For C methods specialization of cdata types, only the routine type
// needs specialization.
//
// For Dao routines, only the original routines have type inference done
// at compiling time. Routine specialization on parameters at compiling time
// is only done for the routine type (DaoRoutine::routType), such shallowly
// specialized routine will share the same routine body (DaoRoutine::body)
// as the original one. Deep specialization with type inference can be performed
// at runtime.
//
// For Parametric Specialization, routine constants can be reused.
//
// For a Dao routine, "body" is not NULL;
// For a C function, "pFunc" is not NULL;
// For abstract routine in interface, "body", "pFunc" and "original" are NULL;
//
// For a partially applied function, "body" and "pFunc" are NULL, but "original" is not;
// and "routConsts" holds partially applied parameters.
*/
struct DaoRoutine
{
	DAO_DATA_COMMON;

	uchar_t          attribs;
	uchar_t          parCount; /* number of parameters that can be accepted; */
	ushort_t         defLine;  /* definition line number in the source file; */
	DString         *routName; /* routine name; */
	DaoType         *routType; /* routine type; */
	DaoType         *routHost; /* host type, for routine that is a method; */
	DaoList         *routConsts; /* default parameters and routine constants; */
	DaoNamespace    *nameSpace; /* definition namespace; */

	DaoRoutineBody  *body; /* data for Dao routines; */
	DaoCFunction     pFunc;

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

int DaoRoutine_SetVmCodes( DaoRoutine *self, DArray *vmCodes );
void DaoRoutine_SetSource( DaoRoutine *self, DaoLexer *lexer );

void DaoRoutine_PrintCode( DaoRoutine *self, DaoStream *stream );

int DaoRoutine_DoTypeInference( DaoRoutine *self, int silent );

struct DaoRoutineBody
{
	DAO_DATA_COMMON;

	/* virtual machine codes: */
	DPlainArray *vmCodes;

	/* data type for local registers: */
	DArray *regType; /* <DaoType*> */

	/* VM codes with annotations */
	DArray *annotCodes; /* <DaoVmCodeX*> */

	DaoLexer  *source;    /* source code tokens; */ 
	DaoLexer  *defLocals; /* definition tokens of local constants and variables; */

	DArray *simpleVariables;

	DMap *localVarType; /* <int,DaoType*> local variable types */

	int mode;

	ushort_t regCount;
	ushort_t codeStart;
	ushort_t codeEnd;

	DMap *abstypes;

	DaoRoutine  *upRoutine;
	DaoProcess  *upProcess;
	DaoRoutine  *revised; /* to support edit & continue */

	void *jitData;
};

DaoRoutineBody* DaoRoutineBody_New();
DaoRoutineBody* DaoRoutineBody_Copy( DaoRoutineBody *self );
void DaoRoutineBody_Delete( DaoRoutineBody *self );



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


/*
// DRoutines is a structure to organize overloaded/specialized functions into trees (tries),
// for fast function resolving based on parameter types.
//
//
// In data structures for namespace and class,
// each individual function should have its own entry in these structures,
// and an additional entry of DRoutines should be added for overloaded
// functions. This will simplify some operations such as deriving methods from
// parent type or instantiating template classes!
*/

struct DRoutines
{
	unsigned int   attribs;
	DParamNode    *tree;
	DParamNode    *mtree;    /* for routines with self parameter */
	DArray        *routines; /* list of overloaded routines on both trees */
	DArray        *array;    /* list of all added routines (may not be on the trees) */
	DArray        *array2;
};

DRoutines* DRoutines_New();
void DRoutines_Delete( DRoutines *self );

DaoRoutine* DRoutines_Add( DRoutines *self, DaoRoutine *routine );

void DaoRoutines_Import( DaoRoutine *self, DRoutines *other );


/* Resolve overloaded, virtual and specialized function: */
DaoRoutine* DaoRoutine_ResolveX( DaoRoutine *self, DaoValue *obj, DaoValue *p[], int n, int code );
DaoRoutine* DaoRoutine_ResolveByType( DaoRoutine *self, DaoType *st, DaoType *t[], int n, int code );
void DaoRoutine_UpdateVtable( DaoRoutine *self, DaoRoutine *routine, DMap *vtable );

#endif
