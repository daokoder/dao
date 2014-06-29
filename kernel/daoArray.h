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
		DaoInterface **pInter;
		DaoRoutine   **pRoutine;
		DaoCdata     **pCdata;
		DaoCtype     **pCtype;
		DaoType      **pType;
		DaoConstant  **pConst;
		DaoVariable  **pVar;
		DaoNamespace **pNS;

		DString      **pString;
		DVector      **pVector;
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

	volatile uchar_t   mutating;
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
DAO_DLL void* DArray_PushFront( DArray *self, void *val );
DAO_DLL void* DArray_PushBack( DArray *self, void *val );

DAO_DLL void* DArray_PopFront( DArray *self );
DAO_DLL void* DArray_PopBack( DArray *self );
DAO_DLL void* DArray_Front( DArray *self );
DAO_DLL void* DArray_Back( DArray *self );

#define DArray_Append( self, val )   DArray_PushBack( self, (void*)(daoint)(val) )
#define DArray_Pop( self )           DArray_PopBack( self )
#define DArray_Top( self )           DArray_Back( self )
#define DArray_TopInt( self )        (self)->items.pInt[ (self)->size -1 ]
#define DArray_Item( self, i )       (self)->items.pVoid[i]
#define DArray_String( self, i )     (self)->items.pString[i]



/*
// Vector container for different data types/sizes:
// A typical use:
//   DVector *vector = DVector_New( sizeof(SomeType) );
//   SomeType *item = (SomeType*) DVector_Push( vector );
//   do-something-with-item;
*/
struct DVector
{
	union {
		void       *base;
		char       *chars;
		int        *ints;
		daoint     *daoints;
		float      *floats;
		double     *doubles;
		uint_t     *uints;
		wchar_t    *wchars;
		ushort_t   *ushorts;
		complex16  *complexes;
		DString    *strings;
		DaoToken   *tokens;
		DaoVmCode  *codes;
		DaoValue  **values;
	} data;

	daoint  size;      /* Number of data items in the vector; */
	daoint  capacity;  /* Total number of data items that can be stored in the vector; */
	short   stride;    /* Data item size in bytes; */
	short   type;      /* Reserved; */
};

DAO_DLL DVector* DVector_New( int stride );
DAO_DLL DVector* DVector_Copy( DVector *self );
DAO_DLL void DVector_Delete( DVector *self );
DAO_DLL void DVector_Clear( DVector *self );

DAO_DLL void DVector_Resize( DVector *self, daoint size );
DAO_DLL void DVector_Reserve( DVector *self, daoint size );
DAO_DLL void DVector_Reset( DVector *self, daoint size );

DAO_DLL void DVector_Assign( DVector *left, DVector *right );

DAO_DLL void* DVector_Insert( DVector *self, daoint i, daoint n );
DAO_DLL void* DVector_Push( DVector *self );
DAO_DLL void* DVector_Pop( DVector *self );
DAO_DLL void* DVector_Back( DVector *self );
DAO_DLL void* DVector_Get( DVector *self, daoint i );

DAO_DLL void DVector_Erase( DVector *self, daoint i, daoint n );

DAO_DLL int* DVector_PushInt( DVector *self, int value );
DAO_DLL daoint* DVector_PushDaoInt( DVector *self, daoint value );
DAO_DLL float* DVector_PushFloat( DVector *self, float value );
DAO_DLL ushort_t* DVector_PushUshort( DVector *self, ushort_t value );



#endif
