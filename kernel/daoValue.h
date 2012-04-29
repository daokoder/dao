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


struct DaoConstant
{
	DAO_DATA_COMMON;

	DaoValue *value;
};

struct DaoVariable
{
	DAO_DATA_COMMON;

	DaoValue *value;
	DaoType  *dtype;
};

DAO_DLL DaoConstant* DaoConstant_New( DaoValue *value );
DAO_DLL DaoVariable* DaoVariable_New( DaoValue *value, DaoType *type );

DAO_DLL void DaoConstant_Delete( DaoConstant *self );
DAO_DLL void DaoVariable_Delete( DaoVariable *self );



union DaoValue
{
	uchar_t        type;
	DaoNone        xBase;
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
	DaoConstant    xConst;
	DaoVariable    xVar;
	DaoType        xType;

	struct {
		uchar_t  type;
		uchar_t  subtype;
		uchar_t  trait;
		uchar_t  work  : 1; /* mark objects in the work buffer; */
		uchar_t  alive : 1; /* mark alive objects (scanned for reachable objects); */
		uchar_t  delay : 1; /* mark objects in the delayed list; */
		uchar_t  dummy : 5;
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
DAO_DLL daoint DaoValue_GetInteger( DaoValue *self );
DAO_DLL float  DaoValue_GetFloat( DaoValue *self );
DAO_DLL double DaoValue_GetDouble( DaoValue *self );
DAO_DLL complex16 DaoValue_GetComplex( DaoValue *self );
DAO_DLL DLong* DaoValue_GetLong( DaoValue *self, DLong *lng );
DAO_DLL DString* DaoValue_GetString( DaoValue *self, DString *str );

DAO_DLL int DaoValue_IsNumber( DaoValue *self );
DAO_DLL void DaoValue_Print( DaoValue *self, DaoProcess *ctx, DaoStream *stream, DMap *cycData );


#endif
