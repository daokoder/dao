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
	DArray  *instvars;  /* <DaoVariable*>, instance variable types and default values; */

	DArray  *cstDataName;  /* <DString*>: keep track field declaration order: */
	DArray  *glbDataName;  /* <DString*>: keep track field declaration order: */
	DArray  *objDataName;  /* <DString*>: keep tracking field declaration order: */

	DArray  *superClass; /* <DaoClass/DaoCData*>: direct super classes. */

	/* Routines with overloading signatures: */
	/* They are inserted into constants, no refCount updating for this. */
	DMap  *ovldRoutMap; /* <DString*,DaoRoutine*> */

	/* Map virtual methods of parent classes to its reimplementation in this class: */
	DMap  *vtable; /* <DRoutine*,DRoutine*> */

	DaoRoutine  *classRoutine; /* Default class constructor. */
	DaoRoutine  *classRoutines; /* All explicitly defined constructors; GC handled in constants; */

	DString  *className;

	DaoType  *clsType;
	DaoType  *objType; /* GC handled in constants; */
	DMap     *abstypes;

	/* When DaoClass is used as a proto-class structure,
	 * protoValues map upvalue register ids to member names.
	 * so that those upvalues can be used to set the constant or
	 * default values of the fields in the classes created from 
	 * this proto-class. */
	DMap     *protoValues; /* <int,DString*> */

#ifdef DAO_WITH_DYNCLASS
	/* for template class: class name<@S,@T=some_type> */
	DArray   *typeHolders; /* @S, @T */
	DArray   *typeDefaults; /* some_type */
	DMap     *instanceClasses; /* instantiated classes */
	DaoClass *templateClass; /* for incomplete instantiation */
#endif

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
DAO_DLL void DaoClass_AddSuperClass( DaoClass *self, DaoValue *super );
DAO_DLL DaoValue* DaoClass_CastToBase( DaoClass *self, DaoType *parent );

DAO_DLL int  DaoClass_FindConst( DaoClass *self, DString *name );
DAO_DLL DaoValue* DaoClass_GetConst( DaoClass *self, int id );
DAO_DLL void DaoClass_SetConst( DaoClass *self, int id, DaoValue *value );
DAO_DLL int DaoClass_GetData( DaoClass *self, DString *name, DaoValue **value, DaoClass *thisClass/*=0*/ );

DAO_DLL DaoType** DaoClass_GetDataType( DaoClass *self, DString *name, int *res, DaoClass *thisClass );
DAO_DLL int DaoClass_GetDataIndex( DaoClass *self, DString *name );

DAO_DLL int DaoClass_AddConst( DaoClass *self, DString *name, DaoValue *value, int pm );
DAO_DLL int DaoClass_AddGlobalVar( DaoClass *self, DString *name, DaoValue *val, DaoType *tp, int pm );
DAO_DLL int DaoClass_AddObjectVar( DaoClass *self, DString *name, DaoValue *val, DaoType *tp, int pm );

DAO_DLL int DaoClass_AddType( DaoClass *self, DString *name, DaoType *tp );

DAO_DLL void DaoClass_AddOverloadedRoutine( DaoClass *self, DString *signature, DaoRoutine *rout );
DAO_DLL DaoRoutine* DaoClass_GetOverloadedRoutine( DaoClass *self, DString *signature );

DAO_DLL DaoRoutine* DaoClass_FindOperator( DaoClass *self, const char *oper, DaoClass *scoped );

#endif
