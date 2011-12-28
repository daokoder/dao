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

#ifndef DAO_VALUE_H
#define DAO_VALUE_H

#include"daoType.h"
#include"daoStdtype.h"
#include"daoNumtype.h"
#include"daoStream.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoRoutine.h"
#include"daoNamespace.h"
#include"daoProcess.h"

union DaoValue
{
	uchar_t        type;
	DaoNone        xNone;
	DaoInteger     xInteger;
	DaoFloat       xFloat;
	DaoDouble      xDouble;
	DaoComplex     xComplex;
	DaoLong        xLong;
	DaoString      xString;
	DaoEnum        xEnum;
	DaoArray       xArray;
	DaoList        xList;
	DaoMap         xMap;
	DaoTuple       xTuple;
	DaoStream      xStream;
	DaoObject      xObject;
	DaoCdata       xCdata;
	DaoCtype       xCtype;
	DaoClass       xClass;
	DaoInterface   xInterface;
	DaoRoutine     xRoutine;
	DaoProcess     xProcess;
	DaoNamespace   xNamespace;
	DaoNameValue   xNameValue;
	DaoType        xType;

	struct { DAO_DATA_CORE; } xCore;
	struct {
		uchar_t  type;
		uchar_t  subtype;
		uchar_t  trait;
		uchar_t  delay : 4; /* used to avoid frequent scanning of persistent objects; */
		uchar_t  idle  : 1; /* used to mark objects in the idle buffer; */
		uchar_t  work  : 1; /* used to mark objects in the work buffer; */
		uchar_t  alive : 1; /* mark alive objects (scanned for reachable objects); */
		uchar_t  dummy : 1;
		int  refCount;
		int  cycRefCount;
	} xGC;
};

/* Copy when self is a simple data type (with type <= DAO_ENUM),
 * or it is a constant array, list, map or tuple. */
DAO_DLL DaoValue* DaoValue_SimpleCopy( DaoValue *self );
DAO_DLL DaoValue* DaoValue_SimpleCopyWithType( DaoValue *self, DaoType *type );

DAO_DLL void DaoValue_Clear( DaoValue **self );

DAO_DLL int DaoValue_Compare( DaoValue *left, DaoValue *right );

DAO_DLL void DaoValue_Copy( DaoValue *src, DaoValue **dest );
DAO_DLL int DaoValue_Move( DaoValue *src, DaoValue **dest, DaoType *destype );
DAO_DLL int DaoValue_Move2( DaoValue *src, DaoValue **dest, DaoType *destype );
DAO_DLL void DaoValue_SimpleMove( DaoValue *src, DaoValue **dest );

DAO_DLL void DaoValue_MarkConst( DaoValue *self );

DAO_DLL int DaoValue_IsZero( DaoValue *self );
DAO_DLL long_t DaoValue_GetInteger( DaoValue *self );
DAO_DLL float  DaoValue_GetFloat( DaoValue *self );
DAO_DLL double DaoValue_GetDouble( DaoValue *self );
DAO_DLL complex16 DaoValue_GetComplex( DaoValue *self );
DAO_DLL DLong* DaoValue_GetLong( DaoValue *self, DLong *lng );
DAO_DLL DString* DaoValue_GetString( DaoValue *self, DString *str );

DAO_DLL int DaoValue_IsNumber( DaoValue *self );
DAO_DLL void DaoValue_Print( DaoValue *self, DaoProcess *ctx, DaoStream *stream, DMap *cycData );

DAO_DLL void DaoValue_IncRCs( DaoValue *v, int n );

#endif
