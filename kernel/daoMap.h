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

#ifndef DAO_MAP_H
#define DAO_MAP_H

#include"daoBase.h"

typedef enum{ KEY_EQ=0, KEY_LE, KEY_GE } KeySearchType;

typedef union
{
	daoint       pInt;
	daoint       pSize;
	void        *pVoid;
	DString     *pString;
	DArray      *pArray;
	DaoValue    *pValue;
	DaoClass    *pClass;
	DaoRoutine  *pRoutine;
	DaoType     *pType;
	DaoInode    *pInode;
}DNodeData;

struct DNode
{
	unsigned int color :  1;
	unsigned int hash  : 31;

	DNode  *parent;
	DNode  *left;
	DNode  *right;
	DNode  *next;

	DNodeData key;
	DNodeData value;
};

typedef DMap DHash;

struct DMap
{
	DNode **table;
	DNode  *root;
	DNode  *first;
	DNode  *last;
	daoint  size;
	daoint  tsize;
	uint_t  hashing;
	char    keytype;
	char    valtype;
};

DAO_DLL DMap* DMap_New( short kt, short vt );
DAO_DLL DMap* DHash_New( short kt, short vt );
DAO_DLL DMap* DMap_Copy( DMap *dmap );
DAO_DLL void DMap_Assign( DMap *self, DMap *other );

DAO_DLL void DMap_Delete( DMap *self );
DAO_DLL void DMap_Clear( DMap *self );
DAO_DLL void DMap_Reset( DMap *self );
/* Insert key/value, and return the previous value if existed. */
DAO_DLL void DMap_Erase( DMap *self, void *key );
DAO_DLL void DMap_EraseNode( DMap *self, DNode *node );

DAO_DLL DNode* DMap_Insert( DMap *self, void *key, void *value );
DAO_DLL DNode* DMap_Find( DMap *self, void *key );
DAO_DLL DNode* DMap_First( DMap *self );
DAO_DLL DNode* DMap_Next( DMap *self, DNode *node );

DAO_DLL DNode* DMap_FindLE( DMap *self, void *key );
DAO_DLL DNode* DMap_FindGE( DMap *self, void *key );

#define MAP_Insert( s, k, v ) DMap_Insert( (DMap*)(s), (void*)(daoint)(k), (void*)(daoint)(v) )
#define MAP_Erase( s, k ) DMap_Erase( (DMap*)(s), (void*)(daoint)(k) )
#define MAP_Find( s, k ) DMap_Find( (DMap*)(s), (void*)(daoint)(k) )
#define MAP_FindLE( s, k ) DMap_FindLE( (DMap*)(s), (void*)(daoint)(k) )
#define MAP_FindGE( s, k ) DMap_FindGE( (DMap*)(s), (void*)(daoint)(k) )

#endif
