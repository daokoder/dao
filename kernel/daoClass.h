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

#ifndef DAO_CLASS_H
#define DAO_CLASS_H

#include"daoType.h"

#define DAO_MAX_PARENT 32

struct DaoClass
{
	DAO_DATA_COMMON;

	/* Holding index of class members, including data from its parents: */
	/* negative index indicates an inaccessible private member from a parent? XXX */
	DMap  *lookupTable; /* <DString*,size_t> */

	DArray  *cstDataTable; /* <DVarray*> */
	DArray  *glbDataTable; /* <DVarray*> */
	DArray  *glbTypeTable; /* <DArray*> */

	DArray  *objDataName;  /* <DString*>: keep tracking field declaration order: */
	DArray  *objDataType;  /* <DaoType*> */
	DArray  *objDataDefault; /* <DaoValue*>, NULL: no default, not for parent classes */

	DArray  *cstDataName;  /* <DString*>: keep track field declaration order: */
	/* Holding class consts and routines - class data: */
	/* For both this class and its parents: */
	DArray  *cstData;

	DArray  *glbDataName;  /* <DString*>: keep track field declaration order: */
	DArray  *glbDataType;  /* <DaoType*> */
	DArray  *glbData;      /* <DaoValue*> */

	DArray  *superClass; /* <DaoClass/DaoCData*>: direct super classes. */
	DArray  *superAlias;

	/* Routines with overloading signatures: */
	/* They are inserted into cstData, no refCount updating for this. */
	DMap  *ovldRoutMap; /* <DString*,DaoRoutine*> */
	DMap  *vtable; /* <DRoutine*,DRoutine*> */

	DaoRoutine   *classRoutine; /* Default class constructor. */
	DaoFunctree  *classRoutines; /* All explicitly defined constructors */

	DString  *className;
	DString  *classHelp;

	DaoType  *clsType;
	DaoType  *objType;
	DMap     *abstypes;
	DMap     *deflines;

	/* When DaoClass is used as a proto-class structure,
	 * protoValues map upvalue register ids to member names.
	 * so that those upvalues can be used to set the constant or
	 * default values of the fields in the classes created from 
	 * this proto-class. */
	DMap     *protoValues; /* <int,DString*> */

	/* for template class: class name[@S,@T=some_type] */
	DArray   *typeHolders; /* @S, @T */
	DArray   *typeDefaults; /* some_type */
	DMap     *instanceClasses; /* instantiated classes */
	DaoClass *templateClass; /* for incomplete instantiation */

	/* for GC */
	DArray *references;

	ushort_t  derived;
	ushort_t  attribs;
	ushort_t  objDefCount;
};

DaoClass* DaoClass_New();
void DaoClass_Delete( DaoClass *self );

void DaoClass_PrintCode( DaoClass *self, DaoStream *stream );
void DaoClass_AddReference( DaoClass *self, void *reference );

void DaoClass_CopyField( DaoClass *self, DaoClass *other, DMap *deftypes );
void DaoClass_SetName( DaoClass *self, DString *name, DaoNamespace *ns );
void DaoClass_DeriveClassData( DaoClass *self );
void DaoClass_DeriveObjectData( DaoClass *self );
void DaoClass_ResetAttributes( DaoClass *self );

DaoClass* DaoClass_Instantiate( DaoClass *self, DArray *types );

int  DaoClass_FindSuper( DaoClass *self, DaoValue *super );
int  DaoClass_ChildOf( DaoClass *self, DaoValue *super );
void DaoClass_AddSuperClass( DaoClass *self, DaoValue *super, DString *alias );
DaoValue* DaoClass_MapToParent( DaoClass *self, DaoType *parent );

int  DaoClass_FindConst( DaoClass *self, DString *name );
DaoValue* DaoClass_GetConst( DaoClass *self, int id );
void DaoClass_SetConst( DaoClass *self, int id, DaoValue *value );
int DaoClass_GetData( DaoClass *self, DString *name, DaoValue **value, DaoClass *thisClass/*=0*/ );

DaoType** DaoClass_GetDataType( DaoClass *self, DString *name, int *res, DaoClass *thisClass );
int DaoClass_GetDataIndex( DaoClass *self, DString *name );

int DaoClass_AddConst( DaoClass *self, DString *name, DaoValue *value, int s, int l );
int DaoClass_AddGlobalVar( DaoClass *self, DString *name, DaoValue *value, DaoType *t, int s, int l );
int DaoClass_AddObjectVar( DaoClass *self, DString *name, DaoValue *deft, DaoType *t, int s, int l );

int DaoClass_AddType( DaoClass *self, DString *name, DaoType *tp );

void DaoClass_AddOvldRoutine( DaoClass *self, DString *signature, DaoRoutine *rout );
DaoRoutine* DaoClass_GetOvldRoutine( DaoClass *self, DString *signature );

DaoValue* DaoClass_FindOperator( DaoClass *self, const char *oper, DaoClass *scoped );

#endif
