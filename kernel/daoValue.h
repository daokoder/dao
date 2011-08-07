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
	DaoCData       xCdata;
	DaoClass       xClass;
	DaoCtype       xCtype;
	DaoInterface   xInterface;
	DaoFunctree    xFunctree;
	DaoRoutine     xRoutine;
	DaoFunction    xFunction;
	DaoContext     xContext;
	DaoVmProcess   xProcess;
	DaoNameSpace   xNamespace;
	DaoNameValue   xNameValue;
	DaoType        xType;
};

/* Copy when self is a simple data type (with type <= DAO_ENUM),
 * or it is a constant array, list, map or tuple. */
DaoValue* DaoValue_SimpleCopy( DaoValue *self );
DaoValue* DaoValue_SimpleCopyWithType( DaoValue *self, DaoType *type );

int DaoValue_Compare( DaoValue *left, DaoValue *right );

void DaoValue_Copy( DValue *src, DValue **dest );
int DaoValue_Move( DaoValue *src, DaoValue **dest, DaoType *destype );
int DaoValue_Move2( DaoValue *src, DaoValue **dest, DaoType *destype );
void DaoValue_SimpleMove( DValue *src, DValue **dest );

void DaoValue_MarkConst( DaoValue *self );

int DaoValue_IsZero( DaoValue *self );
llong_t DaoValue_GetLongLong( DaoValue *self );
llong_t DaoValue_GetInteger( DaoValue *self );
float  DaoValue_GetFloat( DaoValue *self );
double DaoValue_GetDouble( DaoValue *self );
complex16 DaoValue_GetComplex( DaoValue *self );
DLong* DaoValue_GetLong( DaoValue *self, DLong *lng );
DString* DaoValue_GetString( DaoValue *self, DString *str );

int DValue_FromString( DValue *self, DString *str, int type );

int DValue_Serialize( DValue *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc );
int DValue_Deserialize( DValue *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc );

int DValue_IsNumber( DValue self );
void DValue_Print( DValue self, DaoContext *ctx, DaoStream *stream, DMap *cycData );

void DValue_IncRCs( DValue *v, int n );

#define DValue_Type( x ) ( (x).t ? (x).t : (x).v.p ? (x).v.p->type : 0 )


#endif
