/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#ifndef DAO_ARRAY_H
#define DAO_ARRAY_H

#include<limits.h>
#include"daoBase.h"

/* Array of pointers or integers: */
struct DArray
{
	union{
		daoint        *pInt;
		daoint        *pSize;
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

	daoint  size;
	daoint  bufsize;
	size_t  offset : CHAR_BIT*sizeof(size_t) - 4;
	size_t  type   : 4; /* can be 0 (for integers or pointers), or, D_STRING, D_ARRAY, etc. */
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
DAO_DLL void DaoVmcArray_PushFront( DaoVmcArray *self, DaoVmCode code );
DAO_DLL void DaoVmcArray_PushBack( DaoVmcArray *self, DaoVmCode code );
DAO_DLL void DaoVmcArray_PopFront( DaoVmcArray *self );
DAO_DLL void DaoVmcArray_PopBack( DaoVmcArray *self );
DAO_DLL void DaoVmcArray_Assign( DaoVmcArray *left, DaoVmcArray *right );
/*DaoVmCode* DaoVmcArray_Top( DaoVmcArray *self ); */

/* Insert code and update jumps */
DAO_DLL void DaoVmcArray_Insert( DaoVmcArray *self, DaoVmCode code, int pos );
/* Cleanup unused codes and update jumps */
DAO_DLL void DaoVmcArray_Cleanup( DaoVmcArray *self );
DAO_DLL void DArray_CleanupCodes( DArray *self );

#define DaoVmcArray_Append( self, code )  DaoVmcArray_PushBack( self, code )
#define DaoVmcArray_Pop( self )  DaoVmcArray_PopBack( self )


#endif
