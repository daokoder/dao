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


struct DaoNameSpace
{
  DAO_DATA_COMMON

  DaoVmSpace *vmSpace;
  DaoNameSpace *parent;
  int cstUser;

  /* Global consts: including builtin types, routines, classes, namespaces, plugins etc. */
  DVarray  *cstData;
  DMap     *cstIndex; /* <DString*,size_t> */
  DMap     *cstStatic; /* <DString*,size_t> */

  /* global data in the name space: */
  DVarray  *varData;
  DArray   *varType;   /* <DaoType*> */
  DMap     *varIndex;  /* <DString*,size_t> */
  DMap     *varStatic;  /* <DString*,size_t> */

  DaoRoutine *mainRoutine;

  DArray *mainRoutines; /* stdlib.eval() */
  DArray *definedRoutines; /* for DaoStudio IDE */

  DArray *nsLoaded; /* loaded modules as namespaces */
  DArray *ctypers;
  /* The methods of C types loaded from a C module,
   * used for the purpose of GC. */
  DArray *cmethods; /* <DaoFunction*> */
  DMap   *macros; /* <DString*,DaoMacro*> */
  DMap   *abstypes; /* <DString*,DaoType*> */

  DaoType *udfType1;
  DaoType *udfType2;

  void   *libHandle;
  DString *file;
  DString *path;
  DString *name; /* path + file */
  DString *source;
  ullong_t time;

  DaoVmProcess *vmpEvalConst;
  DaoRoutine   *routEvalConst;
};

DaoNameSpace* DaoNameSpace_New( DaoVmSpace *vms );
void DaoNameSpace_Delete( DaoNameSpace *self );

void DaoNameSpace_SetName( DaoNameSpace *self, const char *name );

int  DaoNameSpace_FindConst( DaoNameSpace *self, DString *name );
void DaoNameSpace_AddConst( DaoNameSpace *self, DString *name, DValue value );
void DaoNameSpace_SetConst( DaoNameSpace *self, int index, DValue value );
DValue DaoNameSpace_GetConst( DaoNameSpace *self, int i );

int  DaoNameSpace_FindVariable( DaoNameSpace *self, DString *name );
void DaoNameSpace_AddVariable( DaoNameSpace *self, DString *name, DValue var, DaoType *tp );
int DaoNameSpace_SetVariable( DaoNameSpace *self, int index, DValue var );
DValue DaoNameSpace_GetVariable( DaoNameSpace *self, int i );

void DaoNameSpace_SetData( DaoNameSpace *self, DString *name, DValue value );
DValue DaoNameSpace_GetData( DaoNameSpace *self, DString *name );

DaoClass* DaoNameSpace_FindClass( DaoNameSpace *self, DString *name );
DaoNameSpace* DaoNameSpace_FindNameSpace( DaoNameSpace *self, DString *name );

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

int DaoNameSpace_PrepareType( DaoNameSpace *self, DaoTypeBase *typer );
#if 0
int DaoNameSpace_PrepareTypes( DaoNameSpace *self, DaoTypeBase *typer[] );
#endif

DaoFunction* DaoNameSpace_ParsePrototype( DaoNameSpace *self, const char *proto, DaoParser *parser );
DaoFunction* DaoNameSpace_MakeFunction( DaoNameSpace *self, const char *proto, DaoParser *parser );

void* DValue_GetTypeID( DValue self );

#endif
