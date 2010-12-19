/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2010, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

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
#include"daoValue.h"

typedef enum{ KEY_EQ=0, KEY_ML/*Max Less*/, KEY_MG/*Min Great*/ } KeySearchType;

typedef union
{
	dint       pInt;
	size_t     pSize;
	void      *pVoid;
	DValue    *pValue;
	DString   *pString;
	DArray    *pArray;
	DaoBase   *pBase;
	DaoClass  *pClass;
	DaoType   *pType;
	DaoInode  *pInode;
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
extern DNode* DNode_Next( DNode *self );
extern DNode* DNode_First( DNode *self );

struct DMap
{
	DNode **table;
	DNode  *root;
	DNode  *first;
	DNode  *last;
	int     size;
	int     tsize;
	char    keytype;
	char    valtype;
	char    hashing;
};
extern DMap* DMap_New( short kt, short vt );
extern DMap* DHash_New( short kt, short vt );
extern DMap* DMap_Copy( DMap *dmap );
extern void DMap_Assign( DMap *self, DMap *other );

extern void DMap_Delete( DMap *self );
extern void DMap_Clear( DMap *self );
extern void DMap_Reset( DMap *self );
/* Insert key/value, and return the previous value if existed. */
extern void DMap_Erase( DMap *self, void *key );
void DMap_EraseNode( DMap *self, DNode *node );

extern DNode* DMap_Insert( DMap *self, void *key, void *value );
extern DNode* DMap_Find( DMap *self, void *key );
extern DNode* DMap_First( DMap *self );
extern DNode* DMap_Next( DMap *self, DNode *node );

extern DNode* DMap_FindML( DMap *self, void *key );
extern DNode* DMap_FindMG( DMap *self, void *key );

#define MAP_Insert( s, k, v ) DMap_Insert( (DMap*)(s), (void*)(size_t)(k), (void*)(size_t)(v) )
#define MAP_Erase( s, k ) DMap_Erase( (DMap*)(s), (void*)(size_t)(k) )
#define MAP_Find( s, k ) DMap_Find( (DMap*)(s), (void*)(size_t)(k) )
#define MAP_FindML( s, k ) DMap_FindML( (DMap*)(s), (void*)(size_t)(k) )
#define MAP_FindMG( s, k ) DMap_FindMG( (DMap*)(s), (void*)(size_t)(k) )

#endif
