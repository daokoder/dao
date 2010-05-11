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

#ifndef DAO_CLASS_H
#define DAO_CLASS_H

#include"daoType.h"

#define LOOKUP_BIND( sto, perm, index )  (((sto)<<24)|((perm)<<16)|index)
#define LOOKUP_ST( one )  ((one)>>24)
#define LOOKUP_PM( one )  (((one)>>16)&0xff)
#define LOOKUP_ID( one )  ((short)((one)&0xffff))

enum DaoClassStorage
{
  DAO_CLASS_CONST ,
  DAO_CLASS_GLOBAL ,
  DAO_CLASS_VARIABLE
};

struct DaoClass
{
  DAO_DATA_COMMON

  /* Holding index of class members, including data from its parents: */
  /* negative index indicates an inaccessible private member from a parent. XXX */
  DMap *lookupTable; /* <DString*,size_t>: (storage<<24)|(permission<<16)|index */

  DArray   *objDataName;  /* <DString*>: keep tracking field declaration order: */
  DArray   *objDataType;  /* <DaoAbsType*> */
  DVarray  *objDataDefault; /* <DValue>, NULL: no default, not for parent classes */

  DArray   *cstDataName;  /* <DString*>: keep track field declaration order: */
  /* Holding class consts and routines - class data: */
  /* For both this class and its parents: */
  DVarray  *cstData;

  DArray   *glbDataName;  /* <DString*>: keep track field declaration order: */
  DArray   *glbDataType;  /* <DaoAbsType*> */
  DVarray  *glbData;      /* <DValue> */

  DArray *superClass; /* <DaoClass/DaoCData*>: direct super classes. */
  DArray *superAlias;

  /* Routines with overloading signatures: */
  /* They are inserted into cstData, no refCount updating for this. */
  DMap   *ovldRoutMap; /* <DString*,DaoRoutine*> */

  DaoRoutine *classRoutine; /* Class constructor. */

  DString *className;
  DString *docString;

  DaoAbsType *clsType;
  DaoAbsType *objType;
  DMap       *abstypes;

  int       derived;
  uint_t    attribs;
};

DaoClass* DaoClass_New();
void DaoClass_Delete( DaoClass *self );

void DaoClass_PrintCode( DaoClass *self, DaoStream *stream );

void DaoClass_SetName( DaoClass *self, DString *name, DaoNameSpace *ns );
void DaoClass_DeriveClassData( DaoClass *self );

int  DaoClass_FindSuper( DaoClass *self, DaoBase *super );
int  DaoClass_ChildOf( DaoClass *self, DaoBase *super );
void DaoClass_AddSuperClass( DaoClass *self, DaoBase *super, DString *alias );

int  DaoClass_FindConst( DaoClass *self, DString *name );
void DaoClass_SetConst( DaoClass *self, int id, DValue value );
int DaoClass_GetData( DaoClass *self, DString *name, DValue *value, DaoClass *thisClass/*=0*/, DValue **d2 );

DaoAbsType** DaoClass_GetDataType( DaoClass *self, DString *name, 
    int *res, DaoClass *thisClass );
int DaoClass_GetDataIndex( DaoClass *self, DString *name, int *type );

int DaoClass_AddConst( DaoClass *self, DString *name, DValue value, int s );
int DaoClass_AddGlobalVar( DaoClass *self, DString *name, DValue value, int s, DaoAbsType *t );
int DaoClass_AddObjectVar( DaoClass *self, DString *name, DValue deft, int s, DaoAbsType *t );

int DaoClass_AddAbsType( DaoClass *self, DString *name, DaoAbsType *tp );

void DaoClass_AddOvldRoutine( DaoClass *self, DString *signature, DaoRoutine *rout );
DaoRoutine* DaoClass_GetOvldRoutine( DaoClass *self, DString *signature );

DaoRoutine* DaoClass_FindOperator( DaoClass *self, const char *oper, DaoClass *scoped );

#endif
