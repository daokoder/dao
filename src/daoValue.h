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

#ifndef DAO_VALUE_H
#define DAO_VALUE_H

#include"daoBase.h"

/* t = 0, DAO_INTEGER, DAO_FLOAT, DAO_DOUBLE, DAO_COMPLEX, DAO_STRING
 * when t==0, v.p should not be one of these primitive data.
 * t2 should be set to the type of the data.
 */

const extern DValue daoNullValue;
const extern DValue daoZeroInt;
const extern DValue daoZeroFloat;
const extern DValue daoZeroDouble;
const extern DValue daoNullComplex;
const extern DValue daoNullString;
const extern DValue daoNullEnum;
const extern DValue daoNullArray;
const extern DValue daoNullList;
const extern DValue daoNullMap;
const extern DValue daoNullPair;
const extern DValue daoNullTuple;
const extern DValue daoNullClass;
const extern DValue daoNullObject;
const extern DValue daoNullRoutine;
const extern DValue daoNullFunction;
const extern DValue daoNullCData;
const extern DValue daoNullStream;
const extern DValue daoNullType;

void DValue_Clear( DValue *self );
void DValue_Init( DValue *self, DaoBase *p );
void DValue_CopyExt( DValue *self, DValue from, int copy );
void DValue_Copy( DValue *self, DValue from );
int DValue_Compare( DValue left, DValue right );

int DValue_Move( DValue from, DValue *to, DaoType *totype );
void DValue_SimpleMove( DValue from, DValue *to );

void DValue_MarkConst( DValue *self );

llong_t DValue_GetLongLong( DValue val );
llong_t DValue_GetInteger( DValue val );
float  DValue_GetFloat( DValue val );
double DValue_GetDouble( DValue val );
complex16 DValue_GetComplex( DValue val );
DLong* DValue_GetLong( DValue val, DLong *lng );
DString* DValue_GetString( DValue val, DString *str );

int DValue_FromString( DValue *self, DString *str, int type );

int DValue_IsNumber( DValue self );
void DValue_Print( DValue self, DaoContext *ctx, DaoStream *stream, DMap *cycData );

void DValue_IncRCs( DValue *v, int n );

#define DValue_Type( x ) ( (x).t ? (x).t : (x).v.p ? (x).v.p->type : 0 )

struct DVarray
{
	DValue   *data;
	DValue   *buf;

	size_t size;
	size_t bufsize;
};

DVarray* DVarray_New();
void DVarray_Delete( DVarray *self );
void DVarray_Resize( DVarray *self, size_t size, DValue val );
void DVarray_Clear( DVarray *self );
/* for array of int, float and double only */
void DVarray_FastClear( DVarray *self );
void DVarray_Insert( DVarray *self, DValue val, size_t id );
void DVarray_Erase( DVarray *self, size_t start, size_t n );
void DVarray_PushFront( DVarray *self, DValue val );
void DVarray_PopFront( DVarray *self );
void DVarray_PushBack( DVarray *self, DValue val );
void DVarray_PopBack( DVarray *self );
void DVarray_Swap( DVarray *left, DVarray *right );
void DVarray_Assign( DVarray *left, DVarray *right );

DaoBase* DVarray_GetItem( DVarray *self, size_t id );
void DVarray_SetItem( DVarray *self, DaoBase *it, size_t id );
void DVarray_AppendItem( DVarray *self, DaoBase *it );

#define DVarray_Append( self, val )   DVarray_PushBack( self, val )
#define DVarray_Pop( self )           DVarray_PopBack( self )
#define DVarray_Top( self )           (self)->data[ (self)->size -1 ]
#define DVarray_TopInt( self )        (self)->data[ (self)->size -1 ]

struct DVaTuple
{
	DValue *data;
	size_t  size;
};

DVaTuple* DVaTuple_New( size_t size, DValue val );
void DVaTuple_Delete( DVaTuple *self );
void DVaTuple_Clear( DVaTuple *self );
void DVaTuple_Resize( DVaTuple *self, size_t size, DValue val );

#endif
