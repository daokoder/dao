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
