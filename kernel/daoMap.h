/*
// Dao Virtual Machine
// http://daoscript.org
//
// Copyright (c) 2006-2017, Limin Fu
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

#ifndef DAO_MAP_H
#define DAO_MAP_H

#include"daoBase.h"

#define DAO_HASH_SEED  0xda0

#ifndef DAO_MAP_ITEM_TYPES
#define DAO_MAP_ITEM_TYPES
#endif

enum KeySearchType
{
	DAO_KEY_EQ,
	DAO_KEY_LE,
	DAO_KEY_GE
};

typedef union
{
	daoint       pInt;
	void        *pVoid;
	dao_complex *pComplex;
	DString     *pString;
	DList       *pList;
	DMap        *pMap;
	DaoValue    *pValue;
	DaoInteger  *pInteger;
	DaoCstruct  *pCstruct;
	DaoClass    *pClass;
	DaoRoutine  *pRoutine;
	DaoType     *pType;
	DaoInode    *pInode;
	DaoCnode    *pCnode;
	DAO_MAP_ITEM_TYPES
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
	DNode   **table;        /* Hash table, each entry is a tree; */
	DNode    *root;         /* Root node; */
	DNode    *list;         /* First node of the free list; */
	size_t    size;         /* Size of the map; */
	uint_t    hashing;      /* Hashing seed; */
	uint_t    tsize2rt;     /* Square root of the table size; */
	uint_t    keytype :  4; /* Key type; */
	uint_t    valtype :  4; /* Value type; */
	uint_t    changes : 24; /* Changes that may change the tree structure(s); */
};

DAO_DLL DMap* DMap_New( short kt, short vt );
DAO_DLL DMap* DHash_New( short kt, short vt );
DAO_DLL DMap* DMap_Copy( DMap *dmap );
DAO_DLL void DMap_Assign( DMap *self, DMap *other );

DAO_DLL void DMap_Delete( DMap *self );
DAO_DLL void DMap_Clear( DMap *self );
DAO_DLL void DMap_Reset( DMap *self );
DAO_DLL void DMap_Erase( DMap *self, void *key );
DAO_DLL void DMap_EraseNode( DMap *self, DNode *node );

DAO_DLL DNode* DMap_Insert( DMap *self, void *key, void *value );
DAO_DLL DNode* DMap_Find( DMap *self, void *key );
DAO_DLL DNode* DMap_FindNode( DMap *self, void *key, int type );
DAO_DLL DNode* DMap_First( DMap *self );
DAO_DLL DNode* DMap_Next( DMap *self, DNode *node );

#define MAP_Insert( s, k, v ) DMap_Insert( (DMap*)(s), (void*)(daoint)(k), (void*)(daoint)(v) )
#define MAP_Erase( s, k ) DMap_Erase( (DMap*)(s), (void*)(daoint)(k) )
#define MAP_Find( s, k ) DMap_Find( (DMap*)(s), (void*)(daoint)(k) )
#define MAP_FindLE( s, k ) DMap_FindLE( (DMap*)(s), (void*)(daoint)(k) )
#define MAP_FindGE( s, k ) DMap_FindGE( (DMap*)(s), (void*)(daoint)(k) )


DAO_DLL unsigned int Dao_Hash( const void *key, int len, unsigned int seed );

#endif
