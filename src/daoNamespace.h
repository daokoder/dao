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

#ifndef DAO_NAMESPACE_H
#define DAO_NAMESPACE_H

#include"daoConst.h"
#include"daolib.h"
#include"daoBase.h"
#include"daoString.h"
#include"daoArray.h"
#include"daoMap.h"
#include"daoType.h"

struct DaoCModule
{
	DAO_DATA_COMMON;

	void    *libHandle;

	DArray  *ctypers;

	/* The methods of C types loaded from a C module,
	 * used for the purpose of GC. */
	DArray  *cmethods; /* <DaoFunction*> */
};
DaoCModule* DaoCModule_New();
void DaoCModule_Delete( DaoCModule *self );

struct DaoNameSpace
{
	DAO_DATA_COMMON;

	DaoVmSpace *vmSpace;

	/* Namespaces that should be used to resolve un-resolved symbols: */
	/* 1. vmSpace.nsInternal; */
	/* 2. loaded namespaces by: load name, without "import" or "as" etc. */
	/* No GC, these namespaces are also referenced by ::cstData. */
	DArray *parents; /* DArray<DaoNameSpace*> */

	DMap   *lookupTable; /* <DString*,size_t> */
	DArray *cstDataTable;
	DArray *varDataTable;
	DArray *varTypeTable;
	DArray *nsTable;

	int cstUser;
	int options;

	/* Global consts: including builtin types, routines, classes, namespaces, plugins etc. */
	DVarray  *cstData;
	DVarray  *varData; /* global data in the name space: */
	DArray   *varType; /* <DaoType*> */

	DaoRoutine *mainRoutine;
    DaoList    *argParams;

	DArray *mainRoutines; /* stdlib.eval() */
	DArray *definedRoutines; /* for DaoStudio IDE */

	DArray *nsLoaded; /* loaded modules as namespaces */

	DaoCModule *cmodule;

	DMap   *macros; /* <DString*,DaoMacro*> */
	DMap   *abstypes; /* <DString*,DaoType*> */

	DaoType *udfType1;
	DaoType *udfType2;

	DString *file;
	DString *path;
	DString *name; /* path + file */
	DArray  *sources;
	DMap    *tokens; /* hash<DString,int> */
	ullong_t time;

	DaoVmProcess *vmpEvalConst;
	DaoRoutine   *routEvalConst;
};

DaoNameSpace* DaoNameSpace_New( DaoVmSpace *vms );
void DaoNameSpace_Delete( DaoNameSpace *self );

void DaoNameSpace_SetName( DaoNameSpace *self, const char *name );

int DaoNameSpace_FindConst( DaoNameSpace *self, DString *name );
int DaoNameSpace_AddConst( DaoNameSpace *self, DString *name, DValue value, int pm );
void DaoNameSpace_SetConst( DaoNameSpace *self, int index, DValue value );
DValue DaoNameSpace_GetConst( DaoNameSpace *self, int i );

int DaoNameSpace_FindVariable( DaoNameSpace *self, DString *name );
int DaoNameSpace_AddVariable( DaoNameSpace *self, DString *name, DValue var, DaoType *tp, int pm );
int DaoNameSpace_SetVariable( DaoNameSpace *self, int index, DValue var );
DValue DaoNameSpace_GetVariable( DaoNameSpace *self, int i );
DaoType* DaoNameSpace_GetVariableType( DaoNameSpace *self, int i );

void DaoNameSpace_SetData( DaoNameSpace *self, DString *name, DValue value );
DValue DaoNameSpace_GetData( DaoNameSpace *self, DString *name );

DaoClass* DaoNameSpace_FindClass( DaoNameSpace *self, DString *name );
DaoNameSpace* DaoNameSpace_FindNameSpace( DaoNameSpace *self, DString *name );

void DaoNameSpace_AddParent( DaoNameSpace *self, DaoNameSpace *parent );
void DaoNameSpace_Import( DaoNameSpace *self, DaoNameSpace *ns, DArray *varImport );

void DaoNameSpace_AddConstNumbers( DaoNameSpace *self0, DaoNumItem *items );

void DaoNameSpace_AddMacro( DaoNameSpace *self, DString *name, DaoMacro *macro );

DaoType* DaoNameSpace_FindType( DaoNameSpace *self, DString *name );
int DaoNameSpace_AddType( DaoNameSpace *self, DString *name, DaoType *tp );
DaoType* DaoNameSpace_GetType( DaoNameSpace *self, DaoBase *p );
DaoType* DaoNameSpace_MakeType( DaoNameSpace *self, const char *name, 
		uchar_t basic, DaoBase *pb, DaoType *nest[], int N );
DaoType* DaoNameSpace_MakeRoutType( DaoNameSpace *self, DaoType *routype,
		DValue *vals, DaoType *types[], DaoType *retp );
DaoType* DaoNameSpace_GetTypeV( DaoNameSpace *self, DValue val );

DaoType* DaoNameSpace_MakeEnumType( DaoNameSpace *self, const char *symbols );
DaoType* DaoNameSpace_SymbolTypeAdd( DaoNameSpace *self, DaoType *t1, DaoType *t2, dint *value );
DaoType* DaoNameSpace_SymbolTypeSub( DaoNameSpace *self, DaoType *t1, DaoType *t2, dint *value );

int DaoNameSpace_SetupValues( DaoNameSpace *self, DaoTypeBase *typer );
int DaoNameSpace_SetupMethods( DaoNameSpace *self, DaoTypeBase *typer );

DaoFunction* DaoNameSpace_ParsePrototype( DaoNameSpace *self, const char *proto, DaoParser *parser );
DaoFunction* DaoNameSpace_MakeFunction( DaoNameSpace *self, const char *proto, DaoParser *parser );

void* DValue_GetTypeID( DValue self );

#endif
