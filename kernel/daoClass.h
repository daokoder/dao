/*
// This file is part of the virtual machine for the Dao programming language.
// Copyright (C) 2006-2012, Limin Fu. Email: daokoder@gmail.com
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this 
// software and associated documentation files (the "Software"), to deal in the Software 
// without restriction, including without limitation the rights to use, copy, modify, merge, 
// publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons 
// to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or 
// substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef DAO_CLASS_H
#define DAO_CLASS_H

#include"daoType.h"

#define DAO_MAX_PARENT 16

struct DaoClass
{
	DAO_DATA_COMMON;

	/* Holding index of class members, including data from its parents: */
	/* negative index indicates an inaccessible private member from a parent? XXX */
	DMap    *lookupTable; /* <DString*,size_t> */

	/* Holding class consts and routines - class data: */
	/* For both this class and its parents: */
	DArray  *constants; /* <DaoConstant*>, constants; */
	DArray  *variables; /* <DaoVariable*>, static variables; */

	DArray  *cstDataName;  /* <DString*>: keep track field declaration order: */
	DArray  *glbDataName;  /* <DString*>: keep track field declaration order: */
	DArray  *objDataName;  /* <DString*>: keep tracking field declaration order: */

	DArray  *objDataType;  /* <DaoType*> */
	DArray  *objDataDefault; /* <DaoValue*>, NULL: no default, not for parent classes */

	DArray  *superClass; /* <DaoClass/DaoCData*>: direct super classes. */
	DArray  *superAlias;

	/* Routines with overloading signatures: */
	/* They are inserted into constants, no refCount updating for this. */
	DMap  *ovldRoutMap; /* <DString*,DaoRoutine*> */

	/* Map virtual methods of parent classes to its reimplementation in this class: */
	DMap  *vtable; /* <DRoutine*,DRoutine*> */

	DaoRoutine  *classRoutine; /* Default class constructor. */
	DaoRoutine  *classRoutines; /* All explicitly defined constructors; GC handled in constants; */

	DString  *className;
	DString  *classHelp;

	DaoType  *clsType;
	DaoType  *objType; /* GC handled in constants; */
	DMap     *abstypes;
	DMap     *deflines;

	/* When DaoClass is used as a proto-class structure,
	 * protoValues map upvalue register ids to member names.
	 * so that those upvalues can be used to set the constant or
	 * default values of the fields in the classes created from 
	 * this proto-class. */
	DMap     *protoValues; /* <int,DString*> */

	/* for template class: class name<@S,@T=some_type> */
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

DAO_DLL DaoClass* DaoClass_New();
DAO_DLL void DaoClass_Delete( DaoClass *self );

DAO_DLL void DaoClass_PrintCode( DaoClass *self, DaoStream *stream );
DAO_DLL void DaoClass_AddReference( DaoClass *self, void *reference );

DAO_DLL int DaoClass_CopyField( DaoClass *self, DaoClass *other, DMap *deftypes );
DAO_DLL void DaoClass_SetName( DaoClass *self, DString *name, DaoNamespace *ns );
DAO_DLL void DaoClass_DeriveClassData( DaoClass *self );
DAO_DLL void DaoClass_DeriveObjectData( DaoClass *self );
DAO_DLL void DaoClass_ResetAttributes( DaoClass *self );

DAO_DLL DaoClass* DaoClass_Instantiate( DaoClass *self, DArray *types );

DAO_DLL int  DaoClass_FindSuper( DaoClass *self, DaoValue *super );
DAO_DLL int  DaoClass_ChildOf( DaoClass *self, DaoValue *super );
DAO_DLL void DaoClass_AddSuperClass( DaoClass *self, DaoValue *super, DString *alias );
DAO_DLL DaoValue* DaoClass_MapToParent( DaoClass *self, DaoType *parent );

DAO_DLL int  DaoClass_FindConst( DaoClass *self, DString *name );
DAO_DLL DaoValue* DaoClass_GetConst( DaoClass *self, int id );
DAO_DLL void DaoClass_SetConst( DaoClass *self, int id, DaoValue *value );
DAO_DLL int DaoClass_GetData( DaoClass *self, DString *name, DaoValue **value, DaoClass *thisClass/*=0*/ );

DAO_DLL DaoType** DaoClass_GetDataType( DaoClass *self, DString *name, int *res, DaoClass *thisClass );
DAO_DLL int DaoClass_GetDataIndex( DaoClass *self, DString *name );

DAO_DLL int DaoClass_AddConst( DaoClass *self, DString *name, DaoValue *value, int s, int l );
DAO_DLL int DaoClass_AddGlobalVar( DaoClass *self, DString *name, DaoValue *value, DaoType *t, int s, int l );
DAO_DLL int DaoClass_AddObjectVar( DaoClass *self, DString *name, DaoValue *deft, DaoType *t, int s, int l );

DAO_DLL int DaoClass_AddType( DaoClass *self, DString *name, DaoType *tp );

DAO_DLL void DaoClass_AddOverloadedRoutine( DaoClass *self, DString *signature, DaoRoutine *rout );
DAO_DLL DaoRoutine* DaoClass_GetOverloadedRoutine( DaoClass *self, DString *signature );

DAO_DLL DaoRoutine* DaoClass_FindOperator( DaoClass *self, const char *oper, DaoClass *scoped );

#endif
