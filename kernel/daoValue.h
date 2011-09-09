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
#include"daoContext.h"

union DaoValue
{
	uchar_t        type;
	DaoNull        xNull;
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
	DaoClass       xClass;
	DaoInterface   xInterface;
	DaoFunctree    xFunctree;
	DaoRoutine     xRoutine;
	DaoFunction    xFunction;
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
DaoValue* DaoValue_SimpleCopy( DaoValue *self );
DaoValue* DaoValue_SimpleCopyWithType( DaoValue *self, DaoType *type );

void DaoValue_Clear( DaoValue **self );

int DaoValue_Compare( DaoValue *left, DaoValue *right );

void DaoValue_Copy( DaoValue *src, DaoValue **dest );
int DaoValue_Move( DaoValue *src, DaoValue **dest, DaoType *destype );
int DaoValue_Move2( DaoValue *src, DaoValue **dest, DaoType *destype );
void DaoValue_SimpleMove( DaoValue *src, DaoValue **dest );

void DaoValue_MarkConst( DaoValue *self );

int DaoValue_IsZero( DaoValue *self );
long_t DaoValue_GetInteger( DaoValue *self );
float  DaoValue_GetFloat( DaoValue *self );
double DaoValue_GetDouble( DaoValue *self );
complex16 DaoValue_GetComplex( DaoValue *self );
DLong* DaoValue_GetLong( DaoValue *self, DLong *lng );
DString* DaoValue_GetString( DaoValue *self, DString *str );

int DaoValue_FromString( DaoValue *self, DString *str, int type );

int DaoValue_Serialize( DaoValue *self, DString *serial, DaoNamespace *ns, DaoProcess *proc );
int DaoValue_Deserialize( DaoValue **self, DString *serial, DaoNamespace *ns, DaoProcess *proc );

int DaoValue_IsNumber( DaoValue *self );
void DaoValue_Print( DaoValue *self, DaoProcess *ctx, DaoStream *stream, DMap *cycData );

void DaoValue_IncRCs( DaoValue *v, int n );

#endif
