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
	DArray  *auxData; /* mainly for GC */

	DaoRoutine *mainRoutine;
    DaoList    *argParams;

	DArray *mainRoutines; /* stdlib.eval() */
	DArray *definedRoutines; /* for DaoStudio IDE */

	DArray *nsLoaded; /* loaded modules as namespaces */

	void  *libHandle;

	DMap   *localMacros; /* <DString*,DaoMacro*> */
	DMap   *globalMacros; /* <DString*,DaoMacro*> */
	DMap   *abstypes; /* <DString*,DaoType*> */
	DMap   *moduleLoaders; /* <DString*,DaoModuleLoader> */
	DMap   *codeInliners; /* <DString*,DaoCodeInliner> */

	DaoType *udfType1;
	DaoType *udfType2;

	DString *file;
	DString *path;
	DString *name; /* path + file */
	DString *inputs; /* interactive inputs (load statements and some definitions) */
	DArray  *sources;
	DMap    *tokens; /* hash<DString,int> */
	ulong_t  time;

	DaoProcess  *constEvalProcess;
	DaoRoutine  *constEvalRoutine;
	DArray      *tempTypes;
};

DAO_DLL DaoNamespace* DaoNamespace_New( DaoVmSpace *vms, const char *name );
DAO_DLL void DaoNamespace_Delete( DaoNamespace *self );

DAO_DLL void DaoNamespace_SetName( DaoNamespace *self, const char *name );

DAO_DLL int DaoNamespace_FindConst( DaoNamespace *self, DString *name );
DAO_DLL int DaoNamespace_AddConst( DaoNamespace *self, DString *name, DaoValue *value, int pm );
DAO_DLL void DaoNamespace_SetConst( DaoNamespace *self, int index, DaoValue *value );
DAO_DLL DaoValue* DaoNamespace_GetConst( DaoNamespace *self, int i );

DAO_DLL int DaoNamespace_FindVariable( DaoNamespace *self, DString *name );
DAO_DLL int DaoNamespace_AddVariable( DaoNamespace *self, DString *name, DaoValue *var, DaoType *tp, int pm );
DAO_DLL int DaoNamespace_SetVariable( DaoNamespace *self, int index, DaoValue *var );
DAO_DLL DaoValue* DaoNamespace_GetVariable( DaoNamespace *self, int i );
DAO_DLL DaoType* DaoNamespace_GetVariableType( DaoNamespace *self, int i );

DAO_DLL void DaoNamespace_SetData( DaoNamespace *self, DString *name, DaoValue *value );
DAO_DLL DaoValue* DaoNamespace_GetData( DaoNamespace *self, DString *name );

DAO_DLL DaoClass* DaoNamespace_FindClass( DaoNamespace *self, DString *name );
DAO_DLL DaoNamespace* DaoNamespace_FindNamespace( DaoNamespace *self, DString *name );

DAO_DLL int DaoNamespace_AddParent( DaoNamespace *self, DaoNamespace *parent );
DAO_DLL void DaoNamespace_Import( DaoNamespace *self, DaoNamespace *ns, DArray *varImport );

DAO_DLL void DaoNamespace_AddConstNumbers( DaoNamespace *self, DaoNumItem *items );

DAO_DLL void DaoNamespace_AddMacro( DaoNamespace *self, DString *name, DaoMacro *macro, int local );
DAO_DLL DaoMacro* DaoNamespace_FindMacro( DaoNamespace *self, DString *name );

DAO_DLL DaoModuleLoader DaoNamespace_FindModuleLoader( DaoNamespace *self, DString *name );
DAO_DLL DaoCodeInliner DaoNamespace_FindCodeInliner( DaoNamespace *self, DString *name );

DAO_DLL DaoType* DaoNamespace_FindType( DaoNamespace *self, DString *name );
DAO_DLL void DaoNamespace_AddType( DaoNamespace *self, DString *name, DaoType *tp );
DAO_DLL void DaoNamespace_AddTypeConstant( DaoNamespace *self, DString *name, DaoType *tp );

DAO_DLL DaoType* DaoNamespace_GetType( DaoNamespace *self, DaoValue *p );
DAO_DLL DaoType* DaoNamespace_MakeType( DaoNamespace *self, const char *name,
		uint_t basic, DaoValue *pb, DaoType *nest[], int N );
DAO_DLL DaoType* DaoNamespace_MakeRoutType( DaoNamespace *self, DaoType *routype,
		DaoValue *vals[], DaoType *types[], DaoType *retp );

DAO_DLL DaoType* DaoNamespace_MakeValueType( DaoNamespace *self, DaoValue *value );
DAO_DLL DaoType* DaoNamespace_MakePairType( DaoNamespace *self, DaoType *first, DaoType *second );
DAO_DLL DaoType* DaoNamespace_MakePairValueType( DaoNamespace *self, DaoValue *first, DaoValue *second );
DAO_DLL DaoTuple* DaoNamespace_MakePair( DaoNamespace *self, DaoValue *first, DaoValue *second );

DAO_DLL DaoType* DaoNamespace_MakeEnumType( DaoNamespace *self, const char *symbols );
DAO_DLL DaoType* DaoNamespace_SymbolTypeAdd( DaoNamespace *self, DaoType *t1, DaoType *t2, dint *value );
DAO_DLL DaoType* DaoNamespace_SymbolTypeSub( DaoNamespace *self, DaoType *t1, DaoType *t2, dint *value );

DAO_DLL int DaoNamespace_SetupValues( DaoNamespace *self, DaoTypeBase *typer );
DAO_DLL void DaoMethods_Insert( DMap *methods, DRoutine *rout, DaoType *host );
DAO_DLL int DaoNamespace_SetupMethods( DaoNamespace *self, DaoTypeBase *typer );
DAO_DLL DaoType* DaoNamespace_SetupType( DaoNamespace *self, DaoTypeBase *typer );

DAO_DLL DaoFunction* DaoNamespace_ParsePrototype( DaoNamespace *self, const char *proto, DaoParser *parser );
DAO_DLL DaoFunction* DaoNamespace_MakeFunction( DaoNamespace *self, const char *proto, DaoParser *parser );

DAO_DLL void DaoNamespace_Backup( DaoNamespace *self, DaoProcess *proc, FILE *fout, int limit );
DAO_DLL void DaoNamespace_Restore( DaoNamespace *self, DaoProcess *proc, FILE *fin );

#endif
