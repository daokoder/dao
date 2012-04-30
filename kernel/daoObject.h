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
// * Redistributions of source code must retain the above copyright notice, this list
//   of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright notice, this list
//   of conditions and the following disclaimer in the documentation and/or other materials
//   provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
// TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef DAO_OBJECT_H
#define DAO_OBJECT_H

#include"daoType.h"

struct DaoObject
{
	DAO_DATA_COMMON;

	uchar_t     isRoot     : 1;
	uchar_t     isDefault  : 1;
	uchar_t     baseCount  : 6; /* number of base objects; may be set to zero by GC; */
	uchar_t     baseCount2 : 6; /* used to determine if objValues is allocated with the object; */
	ushort_t    valueCount;

	DaoClass   *defClass; /* definition class; */
	DaoObject  *rootObject; /* root object for safe down-casting; */
	DaoValue  **objValues; /* instance variable values; */
	DaoValue   *parents[1]; /* the actual size is equal to ::baseCount; */
};

DAO_DLL DaoObject* DaoObject_Allocate( DaoClass *klass, int value_count );
DAO_DLL DaoObject* DaoObject_New( DaoClass *klass );
DAO_DLL void DaoObject_Init( DaoObject *self, DaoObject *that, int offset );
DAO_DLL void DaoObject_Delete( DaoObject *self );

DAO_DLL int DaoObject_ChildOf( DaoValue *self, DaoValue *obj );

DAO_DLL DaoValue* DaoObject_CastToBase( DaoObject *self, DaoType *host );
DAO_DLL DaoObject* DaoObject_SetParentCdata( DaoObject *self, DaoCdata *parent );

DAO_DLL void DaoObject_AddData( DaoObject *self, DString *name, DaoValue *data );

DAO_DLL int DaoObject_SetData( DaoObject *self, DString *name, DaoValue *value, DaoObject *objThis );
DAO_DLL int DaoObject_GetData( DaoObject *self, DString *name, DaoValue **data, DaoObject *objThis );

#endif
