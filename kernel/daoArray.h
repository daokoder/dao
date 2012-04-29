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

#ifndef DAO_ARRAY_H
#define DAO_ARRAY_H

#include<limits.h>
#include"daoBase.h"

/* Array of pointers or integers: */
struct DArray
{
	union{
		daoint        *pInt;
		void         **pVoid;

		DaoValue     **pValue;
		DaoInteger   **pInteger;
		DaoFloat     **pFloat;
		DaoDouble    **pDouble;
		DaoComplex   **pComplex;
		DaoList      **pList;
		DaoTuple     **pTuple;
		DaoClass     **pClass;
		DaoObject    **pObject;
		DaoRoutine   **pRoutine;
		DaoCdata     **pCdata;
		DaoType      **pType;
		DaoConstant  **pConst;
		DaoVariable  **pVar;
		DaoNamespace **pNS;

		DString      **pString;
		DArray       **pArray;
		DMap         **pMap;
		DaoInode     **pInode;
		DaoCnode     **pCnode;
		DaoVmCodeX   **pVmc;
		DaoToken     **pToken;

	} items;

	daoint    size;
	daoint    bufsize;
	ushort_t  offset;
	uchar_t   type; /* can be 0 (for integers or pointers), or, D_STRING, D_ARRAY, etc. */
	uchar_t   mutating;
};

DAO_DLL DArray* DArray_New( short type );
DAO_DLL DArray* DArray_Copy( DArray *self );
DAO_DLL void DArray_Delete( DArray *self );
DAO_DLL void DArray_Assign( DArray *left, DArray *right );
DAO_DLL void DArray_Swap( DArray *left, DArray *right );
DAO_DLL void DArray_Resize( DArray *self, daoint size, void *val );
DAO_DLL void DArray_Clear( DArray *self );
DAO_DLL void DArray_Insert( DArray *self, void *val, daoint id );
DAO_DLL void DArray_InsertArray( DArray *self, daoint at, DArray *array, daoint id, daoint n );
DAO_DLL void DArray_AppendArray( DArray *self, DArray *array );
DAO_DLL void DArray_Erase( DArray *self, daoint start, daoint n );
DAO_DLL void DArray_PushFront( DArray *self, void *val );
DAO_DLL void DArray_PushBack( DArray *self, void *val );

DAO_DLL void* DArray_PopFront( DArray *self );
DAO_DLL void* DArray_PopBack( DArray *self );
DAO_DLL void* DArray_Front( DArray *self );
DAO_DLL void* DArray_Back( DArray *self );

#define DArray_Append( self, val )   DArray_PushBack( self, (void*)(daoint)(val) )
#define DArray_Pop( self )           DArray_PopBack( self )
#define DArray_Top( self )           DArray_Back( self )
#define DArray_TopInt( self )        (self)->items.pInt[ (self)->size -1 ]


struct DaoVmcArray
{
	DaoVmCode *codes;
	DaoVmCode *buf;
	ushort_t   size;
	ushort_t   bufsize;
};
DAO_DLL DaoVmcArray* DaoVmcArray_New();
DAO_DLL void DaoVmcArray_Delete( DaoVmcArray *self );
DAO_DLL void DaoVmcArray_Clear( DaoVmcArray *self );
DAO_DLL void DaoVmcArray_Resize( DaoVmcArray *self, int size );
DAO_DLL void DaoVmcArray_PushBack( DaoVmcArray *self, DaoVmCode code );
DAO_DLL void DaoVmcArray_Assign( DaoVmcArray *left, DaoVmcArray *right );


#endif
