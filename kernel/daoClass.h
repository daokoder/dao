/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2014, Limin Fu
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
// THIS SOFTWARE IS PROVIDED  BY THE COPYRIGHT HOLDERS AND  CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED  WARRANTIES,  INCLUDING,  BUT NOT LIMITED TO,  THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL  THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE FOR ANY DIRECT,
// INDIRECT,  INCIDENTAL, SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL  DAMAGES (INCLUDING,
// BUT NOT LIMITED TO,  PROCUREMENT OF  SUBSTITUTE  GOODS OR  SERVICES;  LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY OF
// LIABILITY,  WHETHER IN CONTRACT,  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef DAO_CLASS_H
#define DAO_CLASS_H

#include"daoType.h"

#define DAO_CLASS_CONST_CSTOR  1


struct DaoClass
{
	DAO_VALUE_COMMON;

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

	DaoValue  *parent;     /* DaoClass or DaoCData; */

	DArray  *mixinBases;   /* <DaoClass*>: direct mixin classes; */
	DArray  *allBases;     /* <DaoClass/DaoCData*>: mixin or parent classes; */

	DArray  *mixins;  /* <DaoClass*>: mixin classes; */
	DVector *ranges;  /* <ushort_t>: ranges of the fields of the mixin classes; */
	DVector *offsets; /* <ushort_t>: offsets of the fields from parent classes; */

	ushort_t  cstMixinStart;
	ushort_t  glbMixinStart;
	ushort_t  objMixinStart;
	ushort_t  cstMixinEnd;
	ushort_t  glbMixinEnd;
	ushort_t  objMixinEnd;
	ushort_t  cstMixinEnd2;
	ushort_t  glbMixinEnd2;
	ushort_t  objMixinEnd2;
	ushort_t  cstParentStart;
	ushort_t  glbParentStart;
	ushort_t  cstParentEnd;
	ushort_t  glbParentEnd;

	/* Routines with overloading signatures: */
	/* They are inserted into constants, no refCount updating for this. */
	DMap  *methSignatures; /* <DString*,DaoRoutine*> */

	DaoRoutine  *classRoutine; /* Default class constructor. */
	DaoRoutine  *classRoutines; /* All explicit constructors; GC handled in constants; */
	DaoRoutine  *castRoutines; /* All user defined cast methods; */

	DString  *className;

	DaoType  *clsType;
	DaoType  *objType; /* GC handled in constants; */
	DMap     *abstypes;

	DArray *decoTargets;

	/* for GC */
	DArray *references;

	uint_t    attribs;
	ushort_t  objDefCount;
	ushort_t  derived;
};

DAO_DLL DaoClass* DaoClass_New();
DAO_DLL void DaoClass_Delete( DaoClass *self );

DAO_DLL void DaoClass_PrintCode( DaoClass *self, DaoStream *stream );
DAO_DLL void DaoClass_AddReference( DaoClass *self, void *reference );

DAO_DLL void DaoClass_SetName( DaoClass *self, DString *name, DaoNamespace *ns );

DAO_DLL int DaoClass_CopyField( DaoClass *self, DaoClass *other, DMap *deftypes );
DAO_DLL int DaoClass_DeriveClassData( DaoClass *self );
DAO_DLL void DaoClass_DeriveObjectData( DaoClass *self );
DAO_DLL void DaoClass_UpdateMixinConstructors( DaoClass *self );
DAO_DLL void DaoClass_ResetAttributes( DaoClass *self );
DAO_DLL void DaoClass_MakeInterface( DaoClass *self );
DAO_DLL int DaoClass_UseMixinDecorators( DaoClass *self );

DAO_DLL int  DaoClass_ChildOf( DaoClass *self, DaoValue *super );
DAO_DLL void DaoClass_AddMixinClass( DaoClass *self, DaoClass *mixin );
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
