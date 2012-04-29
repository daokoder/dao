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

#ifndef DAO_MAP_H
#define DAO_MAP_H

#include"daoBase.h"

#define HASH_SEED  0xda0

typedef enum{ KEY_EQ=0, KEY_LE, KEY_GE } KeySearchType;

typedef union
{
	daoint       pInt;
	void        *pVoid;
	DString     *pString;
	DArray      *pArray;
	DMap        *pMap;
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

	DNodeData key;
	DNodeData value;
};

typedef DMap DHash;

struct DMap
{
	DNode  **table;    /* hash table, each entry is a tree; */
	DNode   *root;     /* root node; */
	DNode   *list;     /* first node of the free list; */
	daoint   size;     /* size of the map; */
	daoint   tsize;    /* size of the table; */
	uint_t   hashing;  /* hashing seed; */
	uchar_t  keytype;  /* key type; */
	uchar_t  valtype;  /* value type; */
	uchar_t  mutating;
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
