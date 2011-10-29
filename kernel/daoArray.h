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

#ifndef DAO_ARRAY_H
#define DAO_ARRAY_H

#include<limits.h>
#include"daoBase.h"

typedef union DQuadUByte DQuadUByte;
union DQuadUByte { void *p; struct{ unsigned char a, b, c, d; }X; };

/* Array of pointers or integers: */
struct DArray
{
	union{
		dint          *pInt;
		size_t        *pSize;
		DQuadUByte    *pQUB;

		void         **pVoid;
		DaoValue     **pValue;
		DaoList      **pList;
		DaoTuple     **pTuple;
		DaoClass     **pClass;
		DaoObject    **pObject;
		DaoRoutine   **pRout;
		DRoutine     **pRout2;
		DaoCdata     **pCdata;
		DaoType      **pType;
		DaoNamespace **pNS;

		DString      **pString;
		DArray       **pArray;
		DMap         **pMap;
		DaoInode     **pInode;
		DaoVmCodeX   **pVmc;
		DaoToken     **pToken;

	} items;

	size_t size;
	size_t bufsize;
	size_t offset : CHAR_BIT*sizeof(size_t) - 4;
	size_t type   : 4; /* can be 0 (for integers or pointers), or, D_STRING, D_ARRAY, etc. */
};

/* See daolib.h */
DArray* DArray_New( short type );
DArray* DArray_Copy( DArray *self );
void DArray_Delete( DArray *self );
void DArray_Assign( DArray *left, DArray *right );
void DArray_Swap( DArray *left, DArray *right );
void DArray_Resize( DArray *self, size_t size, void *val );
void DArray_Clear( DArray *self );
void DArray_Insert( DArray *self, void *val, size_t id );
void DArray_InsertArray( DArray *self, size_t at, DArray *array, size_t id, size_t n );
void DArray_AppendArray( DArray *self, DArray *array );
void DArray_Erase( DArray *self, size_t start, size_t n );
void DArray_PushFront( DArray *self, void *val );
void DArray_PopFront( DArray *self );
void DArray_PushBack( DArray *self, void *val );
void DArray_PopBack( DArray *self );

void* DArray_Front( DArray *self );
void* DArray_Back( DArray *self );

#define DArray_Append( self, val )   DArray_PushBack( self, (void*)(size_t)(val) )
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
DaoVmcArray* DaoVmcArray_New();
void DaoVmcArray_Delete( DaoVmcArray *self );
void DaoVmcArray_Clear( DaoVmcArray *self );
void DaoVmcArray_Resize( DaoVmcArray *self, int size );
void DaoVmcArray_PushFront( DaoVmcArray *self, DaoVmCode code );
void DaoVmcArray_PushBack( DaoVmcArray *self, DaoVmCode code );
void DaoVmcArray_PopFront( DaoVmcArray *self );
void DaoVmcArray_PopBack( DaoVmcArray *self );
void DaoVmcArray_Assign( DaoVmcArray *left, DaoVmcArray *right );
void DaoVmcArray_Swap( DaoVmcArray *left, DaoVmcArray *right );
/*DaoVmCode* DaoVmcArray_Top( DaoVmcArray *self ); */

/* Insert code and update jumps */
void DaoVmcArray_Insert( DaoVmcArray *self, DaoVmCode code, size_t pos );
/* Cleanup unused codes and update jumps */
void DaoVmcArray_Cleanup( DaoVmcArray *self );
void DArray_CleanupCodes( DArray *self );

#define DaoVmcArray_Append( self, code )  DaoVmcArray_PushBack( self, code )
#define DaoVmcArray_Pop( self )  DaoVmcArray_PopBack( self )


#endif
