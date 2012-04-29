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
