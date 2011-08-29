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

#ifndef DAO_NAMESPACE_H
#define DAO_NAMESPACE_H

#include"daoBase.h"
#include"daoConst.h"
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

struct DaoNamespace
{
	DAO_DATA_COMMON;

	DaoVmSpace *vmSpace;

	/* Namespaces that should be used to resolve un-resolved symbols: */
	/* 1. vmSpace.nsInternal; */
	/* 2. loaded namespaces by: load name, without "import" or "as" etc. */
	/* No GC, these namespaces are also referenced by ::cstData. */
	DArray *parents; /* DArray<DaoNamespace*> */

	DMap   *lookupTable; /* <DString*,size_t> */
	DArray *cstDataTable;
	DArray *varDataTable;
	DArray *varTypeTable;
	DArray *nsTable;

	int cstUser;
	int options;

	DArray  *cstData; /* <DaoValue*>, global constants; */
	DArray  *varData; /* <DaoValue*>, global variables; */
	DArray  *varType; /* <DaoType*>, types of global variables */

	DaoRoutine *mainRoutine;
    DaoList    *argParams;

	DArray *mainRoutines; /* stdlib.eval() */
	DArray *definedRoutines; /* for DaoStudio IDE */

	DArray *nsLoaded; /* loaded modules as namespaces */

	DaoCModule *cmodule;

	DMap   *localMacros; /* <DString*,DaoMacro*> */
	DMap   *globalMacros; /* <DString*,DaoMacro*> */
	DMap   *abstypes; /* <DString*,DaoType*> */

	DaoType *udfType1;
	DaoType *udfType2;

	DString *file;
	DString *path;
	DString *name; /* path + file */
	DString *inputs; /* interactive inputs (load statements and some definitions) */
	DArray  *sources;
	DMap    *tokens; /* hash<DString,int> */
	ullong_t time;

	DaoProcess  *constEvalProcess;
	DaoRoutine  *constEvalRoutine;
	DArray      *tempTypes;
};

DaoNamespace* DaoNamespace_New( DaoVmSpace *vms, const char *name );
void DaoNamespace_Delete( DaoNamespace *self );

void DaoNamespace_SetName( DaoNamespace *self, const char *name );

int DaoNamespace_FindConst( DaoNamespace *self, DString *name );
int DaoNamespace_AddConst( DaoNamespace *self, DString *name, DaoValue *value, int pm );
void DaoNamespace_SetConst( DaoNamespace *self, int index, DaoValue *value );
DaoValue* DaoNamespace_GetConst( DaoNamespace *self, int i );

int DaoNamespace_FindVariable( DaoNamespace *self, DString *name );
int DaoNamespace_AddVariable( DaoNamespace *self, DString *name, DaoValue *var, DaoType *tp, int pm );
int DaoNamespace_SetVariable( DaoNamespace *self, int index, DaoValue *var );
DaoValue* DaoNamespace_GetVariable( DaoNamespace *self, int i );
DaoType* DaoNamespace_GetVariableType( DaoNamespace *self, int i );

void DaoNamespace_SetData( DaoNamespace *self, DString *name, DaoValue *value );
DaoValue* DaoNamespace_GetData( DaoNamespace *self, DString *name );

DaoClass* DaoNamespace_FindClass( DaoNamespace *self, DString *name );
DaoNamespace* DaoNamespace_FindNameSpace( DaoNamespace *self, DString *name );

int DaoNamespace_AddParent( DaoNamespace *self, DaoNamespace *parent );
void DaoNamespace_Import( DaoNamespace *self, DaoNamespace *ns, DArray *varImport );

void DaoNamespace_AddConstNumbers( DaoNamespace *self, DaoNumItem *items );

void DaoNamespace_AddMacro( DaoNamespace *self, DString *name, DaoMacro *macro, int local );
DaoMacro* DaoNamespace_FindMacro( DaoNamespace *self, DString *name );

DaoType* DaoNamespace_FindType( DaoNamespace *self, DString *name );
void DaoNamespace_AddType( DaoNamespace *self, DString *name, DaoType *tp );
void DaoNamespace_AddTypeConstant( DaoNamespace *self, DString *name, DaoType *tp );

DaoType* DaoNamespace_GetType( DaoNamespace *self, DaoValue *p );
DaoType* DaoNamespace_MakeType( DaoNamespace *self, const char *name,
		uchar_t basic, DaoValue *pb, DaoType *nest[], int N );
DaoType* DaoNamespace_MakeRoutType( DaoNamespace *self, DaoType *routype,
		DaoValue *vals[], DaoType *types[], DaoType *retp );

DaoType* DaoNamespace_MakeValueType( DaoNamespace *self, DaoValue *value );
DaoType* DaoNamespace_MakePairType( DaoNamespace *self, DaoType *first, DaoType *second );
DaoType* DaoNamespace_MakePairValueType( DaoNamespace *self, DaoValue *first, DaoValue *second );
DaoTuple* DaoNamespace_MakePair( DaoNamespace *self, DaoValue *first, DaoValue *second );

DaoType* DaoNamespace_MakeEnumType( DaoNamespace *self, const char *symbols );
DaoType* DaoNamespace_SymbolTypeAdd( DaoNamespace *self, DaoType *t1, DaoType *t2, dint *value );
DaoType* DaoNamespace_SymbolTypeSub( DaoNamespace *self, DaoType *t1, DaoType *t2, dint *value );

int DaoNamespace_SetupValues( DaoNamespace *self, DaoTypeBase *typer );
void DaoMethods_Insert( DMap *methods, DRoutine *rout, DaoType *host );
int DaoNamespace_SetupMethods( DaoNamespace *self, DaoTypeBase *typer );
DaoType* DaoNamespace_SetupType( DaoNamespace *self, DaoTypeBase *typer );

DaoFunction* DaoNamespace_ParsePrototype( DaoNamespace *self, const char *proto, DaoParser *parser );
DaoFunction* DaoNamespace_MakeFunction( DaoNamespace *self, const char *proto, DaoParser *parser );

void DaoNamespace_Backup( DaoNamespace *self, DaoProcess *proc, FILE *fout, int limit );
void DaoNamespace_Restore( DaoNamespace *self, DaoProcess *proc, FILE *fin );

#endif
