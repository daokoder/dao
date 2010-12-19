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

#include"stdlib.h"
#include"string.h"
#include"math.h"
#include"assert.h"

#include"daoConst.h"
#include"daoMap.h"
#include"daoArray.h"
#include"daoString.h"
#include"daoNumtype.h"

#define RB_RED    0
#define RB_BLACK  1

struct DNodeV1
{
	unsigned int color :  1;
	unsigned int hash  : 31;

	DNode  *parent;
	DNode  *left;
	DNode  *right;
	DNode  *next;

	DNodeData key;
	DNodeData value;

	DValue value1;
};
struct DNodeV2
{
	unsigned int color :  1;
	unsigned int hash  : 31;

	DNode  *parent;
	DNode  *left;
	DNode  *right;
	DNode  *next;

	DNodeData key;
	DNodeData value;

	DValue value1;
	DValue value2;
};

static DNode* DNode_New( DMap *map, int keytype, int valtype )
{
	struct DNodeV2 *nodev2;
	struct DNodeV1 *nodev1;
	DNode *self;
	if( map->first && map->keytype == keytype && map->valtype == valtype ){
		DNode *node = map->first;
		map->first = map->first->next;
		node->next = NULL;
		return node;
	}
	if( keytype != D_VALUE ) keytype = 0;
	if( valtype != D_VALUE ) valtype = 0;
	switch( (keytype<<8) | valtype ){
	case (D_VALUE<<8)|D_VALUE :
		nodev2 = (struct DNodeV2*) dao_calloc( 1, sizeof(struct DNodeV2) );
		nodev2->key.pValue = & nodev2->value1;
		nodev2->value.pValue = & nodev2->value2;
		self = (DNode*) nodev2;
		break;
	case (D_VALUE<<8) :
		nodev1 = (struct DNodeV1*) dao_calloc( 1, sizeof(struct DNodeV1) );
		nodev1->key.pValue = & nodev1->value1;
		self = (DNode*) nodev1;
		break;
	case D_VALUE :
		nodev1 = (struct DNodeV1*) dao_calloc( 1, sizeof(struct DNodeV1) );
		nodev1->value.pValue = & nodev1->value1;
		self = (DNode*) nodev1;
		break;
	default: self = (DNode*) dao_calloc( 1, sizeof(DNode) ); break;
	}
	return self;
}
DNode* DNode_First( DNode *self )
{
	if( self ) while( self->left ) self = self->left;
	return self;
}

DNode* DNode_Next( DNode *self )
{
	DNode* p = self->right;
	if( self->right ){
		while( p->left ) p = p->left;
	}else if( self->parent ){
		if( self == self->parent->left )
			p = self->parent;
		else{
			p = self->parent;
			while( p->parent && p==p->parent->right ) p = p->parent;
			p = p->parent;
		}
	}
	return p;
}
void* DNode_GetKey( DNode *self )
{
	return self->key.pVoid;
}
void* DNode_GetValue( DNode *self )
{
	return self->value.pVoid;
}
DValue* DNode_Key( DNode *self )
{
	return self->key.pValue;
}
DValue* DNode_Value( DNode *self )
{
	return self->value.pValue;
}

DMap* DMap_New( short kt, short vt )
{
	DMap *self = (DMap*) dao_malloc( sizeof( DMap) );
	self->size = 0;
	self->tsize = 0;
	self->first = NULL;
	self->last = NULL;
	self->root = NULL;
	self->table = NULL;
	self->keytype = kt;
	self->valtype = vt;
	self->hashing = 0;
	return self;
}
DMap* DHash_New( short kt, short vt )
{
	DMap *self = DMap_New( kt, vt );
	self->hashing = 1;
	self->tsize = 4;
	self->table = (DNode**) dao_calloc( self->tsize, sizeof(DNode*) );
	return self;
}
double norm_c( const complex16 com );
dint DLong_ToInteger( DLong *self );

#define HASH_SEED  0xda0
unsigned int MurmurHash2 ( const void * key, int len, unsigned int seed );

static int DValue_Hash( DValue self, unsigned int buf[], int id, int max )
{
	void *data = NULL;
	int i, len = 0;
	int id2 = id;
	unsigned int hash = 0;
	switch( self.t ){
	case DAO_INTEGER :
		data = & self.v.i;  len = sizeof(dint);  break;
	case DAO_FLOAT   : 
		data = & self.v.f;  len = sizeof(float);  break;
	case DAO_DOUBLE  : 
		data = & self.v.d;  len = sizeof(double);  break;
	case DAO_COMPLEX : 
		data = self.v.c;  len = sizeof(complex16);  break;
	case DAO_LONG : 
		data = self.v.l->data;
		len = self.v.l->size*sizeof(short);
		break;
	case DAO_ENUM  : 
		data = self.v.e->type->name->mbs; /* XXX */
		len = self.v.e->type->name->size;
		break;
	case DAO_STRING  : 
		if( self.v.s->mbs ){
			data = self.v.s->mbs;
			len = self.v.s->size;
		}else{
			data = self.v.s->wcs;
			len = self.v.s->size * sizeof(wchar_t);
		}
		break;
	case DAO_ARRAY :
		data = self.v.array->data.p;
		len = self.v.array->size;
		switch( self.v.array->numType ){
		case DAO_INTEGER : len *= sizeof(int); break;
		case DAO_FLOAT   : len *= sizeof(float); break;
		case DAO_DOUBLE  : len *= sizeof(double); break;
		case DAO_COMPLEX : len *= sizeof(complex16); break;
		default : break;
		}
		break;
	case DAO_TUPLE :
		for(i=0; i<self.v.tuple->items->size; i++){
			id = DValue_Hash( self.v.tuple->items->data[i], buf, id, max );
			if( id >= max ) break;
		}
		break;
	default : break;
	}
	if( data ) hash = MurmurHash2( data, len, HASH_SEED);
	if( id == id2 && id < max ){
		buf[id] = hash;
		id += 1;
	}
	return id;
}

static int DHash_HashIndex( DMap *self, void *key )
{
#define HASH_MAX  32
	DString *s;
	DArray *array;
	unsigned int buf[HASH_MAX];
	unsigned int T = self->tsize;
	unsigned id = 0;
	void *data;
	int m;

	switch( self->keytype ){
	case D_STRING :
		s = (DString*)key;
		m = s->size;
		data = NULL;
		if( s->mbs ){
			data = s->mbs;
		}else{
			data = s->wcs;
			m *= sizeof(wchar_t);
		}
		id = MurmurHash2( data, m, HASH_SEED) % T;
		break;
	case D_VALUE :
		m = DValue_Hash( *(DValue*) key, buf, 0, HASH_MAX );
		if( m ==1 ){
			id = buf[0] % T;
		}else{
			id = MurmurHash2( buf, m*sizeof(unsigned int), HASH_SEED) % T;
		}
		break;
	case D_ARRAY :
		array = (DArray*)key;
		m = array->size * sizeof(void*);
		id = MurmurHash2( array->items.pVoid, m, HASH_SEED) % T;
		break;
	case D_VOID2 :
		id = MurmurHash2( key, 2*sizeof(void*), HASH_SEED) % T;
		break;
	default : 
		id = MurmurHash2( & key, sizeof(void*), HASH_SEED) % T;
		break;
	}
	return (int)id;
}
static DNode* DMap_SimpleInsert( DMap *self, DNode *node );
static void DMap_InsertNode( DMap *self, DNode *node );
static void DMap_InsertTree( DMap *self, DNode *node )
{
	DNode *left = node->left;
	DNode *right = node->right;
	node->hash = DHash_HashIndex( self, node->key.pVoid );
	node->parent = node->left = node->right = NULL;
	self->root = self->table[ node->hash ];
	if( self->root == NULL ){
		node->color = RB_BLACK;
		self->table[ node->hash ] = node;
		self->size += 1;
	}else{
		DMap_SimpleInsert( self, node );
		DMap_InsertNode( self, node );
		self->table[ node->hash ] = self->root;
	}
	if( left ) DMap_InsertTree( self, left );
	if( right ) DMap_InsertTree( self, right );
}
static void DHash_ResetTable( DMap *self )
{
	DNode **nodes = self->table;
	int i, tsize = self->tsize;

	if( self->hashing ==0 ) return;
	self->tsize = 2 * self->size + 1;
	self->table = (DNode**)dao_calloc( self->tsize, sizeof(DNode*) );
	self->size = 0;
	for(i=0; i<tsize; i++) if( nodes[i] ) DMap_InsertTree( self, nodes[i] );
	if( nodes ) dao_free( nodes );
}
DMap* DMap_Copy( DMap *other )
{
	DMap *self = NULL;
	if( other->hashing ){
		self = DHash_New( other->keytype, other->valtype );
		self->tsize = other->tsize;
		self->table = (DNode**)dao_realloc( self->table, other->tsize*sizeof(DNode*) );
	}else{
		self = DMap_New( other->keytype, other->valtype );
	}
	DMap_Assign( self, other );
	return self;
}
void DMap_Assign( DMap *self, DMap *other )
{
	DNode *node = DMap_First( other );
	DMap_Clear( self );
	while( node ){
		DMap_Insert( self, node->key.pVoid, node->value.pVoid );
		node = DMap_Next( other, node );
	}
}
void DMap_Delete( DMap *self )
{
	DMap_Clear( self );
	if( self->table ) dao_free( self->table );
	dao_free( self );
}
static void DMap_SwapNode( DMap *self, DNode *node, DNode *extreme )
{
	void *key, *value;
	DValue value1, value2;
	struct DNodeV1 *nodev1 = (struct DNodeV1*) node;
	struct DNodeV2 *nodev2 = (struct DNodeV2*) node;
	struct DNodeV1 *extremev1 = (struct DNodeV1*) extreme;
	struct DNodeV2 *extremev2 = (struct DNodeV2*) extreme;
	int hash = extreme->hash;
	int keytype = self->keytype;
	int valtype = self->valtype;
	if( keytype != D_VALUE ) keytype = 0;
	if( valtype != D_VALUE ) valtype = 0;
	extreme->hash = node->hash;
	node->hash = hash;
	switch( (self->keytype<<8) | self->valtype ){
	case (D_VALUE<<8)|D_VALUE :
		value1 = extremev2->value1;
		value2 = extremev2->value2;
		extremev2->value1 = nodev2->value1;
		extremev2->value2 = nodev2->value2;
		nodev2->value1 = value1;
		nodev2->value2 = value2;
		break;
	case (D_VALUE<<8) :
		value1 = extremev1->value1;
		extremev1->value1 = nodev1->value1;
		nodev1->value1 = value1;
		value = extremev1->value.pVoid;
		extremev1->value.pVoid = nodev1->value.pVoid;
		nodev1->value.pVoid = value;
		break;
	case D_VALUE :
		value1 = extremev1->value1;
		extremev1->value1 = nodev1->value1;
		nodev1->value1 = value1;
		key = extremev1->key.pVoid;
		extremev1->key.pVoid = nodev1->key.pVoid;
		nodev1->key.pVoid = key;
		break;
	default:
		key = extreme->key.pVoid;
		value = extreme->value.pVoid;
		extreme->key.pVoid = node->key.pVoid;
		extreme->value.pVoid = node->value.pVoid;
		node->key.pVoid = key;
		node->value.pVoid = value;
		break;
	}
}
static void DMap_CopyItem( void **dest, void *item, short type )
{
	int n = 2*sizeof(void*);
	if( *dest == NULL ){
		switch( type ){
		case D_STRING : *dest = DString_Copy( (DString*) item ); break;
		case D_ARRAY  : *dest = DArray_Copy( (DArray*) item ); break;
		case D_MAP    : *dest = DMap_Copy( (DMap*) item ); break;
		case D_VALUE  : assert( *dest != NULL ); break; /* should never happen */
		case D_VOID2  : *dest = dao_malloc(n); memcpy(*dest, item, n); break;
		default : *dest = item; break;
		}
	}else{
		switch( type ){
		case D_STRING : DString_Assign( (DString*)(*dest), (DString*) item ); break;
		case D_ARRAY  : DArray_Assign( (DArray*)(*dest), (DArray*) item ); break;
		case D_MAP    : DMap_Assign( (DMap*)(*dest), (DMap*) item ); break;
		case D_VALUE  : DValue_Copy( (DValue*)(*dest), *(DValue*) item ); break;
		case D_VOID2  : memcpy(*dest, item, n); break;
		default : *dest = item; break;
		}
	}
}
static void DMap_DeleteItem( void *item, short type )
{
	switch( type ){
	case D_STRING : DString_Delete( (DString*) item ); break;
	case D_ARRAY  : DArray_Delete( (DArray*) item ); break;
	case D_MAP    : DMap_Delete( (DMap*) item ); break;
	case D_VALUE  : DValue_Clear( (DValue*) item ); break;
	case D_VOID2  : dao_free( item ); break;
	default : break;
	}
}
static void DMap_BufferNode( DMap *self, DNode *node )
{
	node->parent = node->left = node->right = node->next = NULL;
	if( self->first == NULL ){
		self->first = self->last = node;
		return;
	}
	self->last->next = node;
	self->last = node;
}
static void DMap_BufferTree( DMap *self, DNode *node )
{
	if( node == NULL ) return;
	DMap_BufferTree( self, node->left );
	DMap_BufferTree( self, node->right );
	DMap_BufferNode( self, node );
}
static void DMap_DeleteNode( DMap *self, DNode *node )
{
	DMap_DeleteItem( node->key.pVoid, self->keytype );
	DMap_DeleteItem( node->value.pVoid, self->valtype );
	dao_free( node );
}
static void DMap_DeleteTree( DMap *self, DNode *node )
{
	if( node == NULL ) return;
	DMap_DeleteTree( self, node->left );
	DMap_DeleteTree( self, node->right );
	DMap_DeleteNode( self, node );
}
void DMap_Clear( DMap *self )
{
	int i;
	if( self->hashing ){
		for(i=0; i<self->tsize; i++) DMap_DeleteTree( self, self->table[i] );
		if( self->table ) dao_free( self->table );
		self->tsize = 4;
		self->table = (DNode**) dao_calloc( self->tsize, sizeof(DNode*) );
	}else{
		DMap_DeleteTree( self, self->root );
	}
	self->root = NULL;
	self->size = 0;
}
void DMap_Reset( DMap *self )
{
	int i;
	if( self->hashing ){
		for(i=0; i<self->tsize; i++) DMap_BufferTree( self, self->table[i] );
		memset( self->table, 0, self->tsize*sizeof(DNode*) );
		self->tsize = 4;
	}else{
		DMap_BufferTree( self, self->root );
	}
	self->root = NULL;
	self->size = 0;
}

static void DMap_RotateLeft( DMap *self, DNode *child )
{
	DNode *grandpa = child->parent;
	DNode *parent = child->right;

	if( grandpa ){
		if( child == grandpa->right )
			grandpa->right = parent;
		else
			grandpa->left = parent;
	}else{
		self->root = parent;
	}
	parent->parent = grandpa;

	child->right = parent->left;
	if( child->right ) child->right->parent = child;

	parent->left = child;
	child->parent = parent;
}
static void DMap_RotateRight( DMap *self, DNode *parent )
{
	DNode *grandpa = parent->parent;
	DNode *child = parent->left;

	if( grandpa ){
		if( parent == grandpa->right )
			grandpa->right = child;
		else
			grandpa->left = child;
	}else{
		self->root = child;
	}
	child->parent = grandpa;

	parent->left = child->right;
	if( parent->left ) parent->left->parent = parent;

	child->right = parent;
	parent->parent = child;
}
void DMap_InsertNode( DMap *self, DNode *node )
{
	DNode *grandpa, *parent, *uncle;

	node->color = RB_RED;
	self->size ++;

	while( node->parent != NULL ){
		parent = node->parent;
		grandpa = parent->parent;
		if( parent->color == RB_RED ){ /* insert_case2() */
			/* grandpa can't be NULL, since parent is RED and can't be root. */
			uncle = ( parent == grandpa->left ? grandpa->right : grandpa->left );
			if( uncle != NULL && uncle->color == RB_RED ){ /* insert_case3(): */
				parent->color = RB_BLACK;
				uncle->color  = RB_BLACK;
				grandpa->color = RB_RED;
				node = grandpa;
				continue; /* insert_case1() */
			}else{
				if( node == parent->right && parent == grandpa->left ){
					DMap_RotateLeft( self, parent );
					node = node->left;
				}else if( node == parent->left && parent == grandpa->right ){
					/* rotate right around parent: */
					DMap_RotateRight( self, parent );
					node = node->right;
				}
				/* node changed, update parent and grandpa. */
				parent = node->parent;
				grandpa = parent->parent;
				/* insert_case5() */

				parent->color = RB_BLACK;
				grandpa->color = RB_RED;
				if( node == parent->left && parent == grandpa->left )
					DMap_RotateRight( self, grandpa );
				else
					DMap_RotateLeft( self, grandpa );
			}
		}
		break;
	}
	/* insert_case1() as in Wikipedia term: Red-black tree. */
	if( node->parent == NULL){
		self->root = node;
		node->color = RB_BLACK;
	}
}
static void DMap_EraseChild( DMap *self, DNode *node )
{
	DNode *extreme = node;
	DNode *child = 0;

	if( node == NULL ) return;
	self->size --;

	/* deletion by coping */
	if( node->left ){
		extreme = node->left;
		while( extreme->right ) extreme = extreme->right;
		child = extreme->left;
	}else if( node->right ){
		extreme = node->right;
		while( extreme->left ) extreme = extreme->left;
		child = extreme->right;
	}
	DMap_SwapNode( self, node, extreme );

	if( child ){
		/* replace node */
		child->parent = extreme->parent;
		if( extreme->parent ){
			if( extreme == extreme->parent->left )
				extreme->parent->left = child;
			else
				extreme->parent->right = child;
		}
		if( extreme->color == RB_BLACK ){
			if( child->color == RB_RED )
				child->color = RB_BLACK;
			else{
				node = child;
				while( node->parent ){ /* delete_case1() */

					DNode *parent = node->parent;
					DNode *sibling
						= ( node == parent->left ? parent->right : parent->left );
					if( sibling && sibling->color == RB_RED ){ /* delete_case2() */
						parent->color = RB_RED;
						sibling->color = RB_BLACK;
						if( node == parent->left )
							DMap_RotateLeft( self, parent );
						else
							DMap_RotateRight( self, parent );
					}
					/* node relationship changed, update parent and sibling: */
					parent = node->parent;
					sibling = ( node == parent->left ? parent->right : parent->left );
					if( ! sibling ) break;
					/* delete_case3() */
					if( parent->color == RB_BLACK && sibling->color == RB_BLACK
							&& ( ! sibling->left || sibling->left->color == RB_BLACK )
							&& ( ! sibling->right|| sibling->right->color == RB_BLACK)){
						sibling->color = RB_RED;
						node = node->parent;
						continue; /* delete_case1() */
					}else{
						/* delete_case4() */
						if( parent->color == RB_RED && sibling->color == RB_BLACK
								&& ( ! sibling->left || sibling->left->color == RB_BLACK )
								&& ( ! sibling->right|| sibling->right->color == RB_BLACK)){
							sibling->color = RB_RED;
							parent->color = RB_BLACK;
						}else{
							/* delete_case5() */
							if( node == parent->left && sibling->color == RB_BLACK
									&&( sibling->left && sibling->left->color == RB_RED )
									&&( !sibling->right|| sibling->right->color == RB_BLACK)){
								sibling->color = RB_RED;
								sibling->left->color = RB_BLACK;
								DMap_RotateRight( self, sibling );
							}else if( node == parent->right && sibling->color == RB_BLACK
									&&( sibling->right && sibling->right->color == RB_RED )
									&&( !sibling->left || sibling->left->color == RB_BLACK)){
								sibling->color = RB_RED;
								sibling->right->color = RB_BLACK;
								DMap_RotateLeft( self, sibling );
							}
							/* node relationship changed, update parent and sibling: */
							parent = node->parent;
							sibling = ( node==parent->left ? parent->right:parent->left );
							/* delete_case6() */
							sibling->color = parent->color;
							parent->color = RB_BLACK;
							if( node == parent->left ){
								sibling->right->color = RB_BLACK;
								DMap_RotateLeft( self, parent );
							}else{
								sibling->left->color = RB_BLACK;
								DMap_RotateRight( self, parent );
							}
						} /* end if */
					} /* end if */
				} /* end while */
			}
		}
	}else if( extreme->parent ){
		if( extreme == extreme->parent->left )
			extreme->parent->left = NULL;
		else
			extreme->parent->right = NULL;
	}else{
		self->root = NULL;
	}
	DMap_BufferNode( self, extreme );
}
void DMap_EraseNode( DMap *self, DNode *node )
{
	if( node == NULL ) return;
	if( self->hashing ){
		int hash = node->hash;
		self->root = self->table[ hash ];
		if( self->root == NULL ) return;
		DMap_EraseChild( self, node );
		self->table[ hash ] = self->root;
		if( self->size < 0.25*self->tsize ) DHash_ResetTable( self );
	}else{
		DMap_EraseChild( self, node );
	}
}
static int DArray_Compare( DArray *k1, DArray *k2 )
{
	int i = 0, n = k1->size;
	dint *v1 = k1->items.pInt;
	dint *v2 = k2->items.pInt;
	if( n != k2->size ) return (int)(n - k2->size);
	while( i < n && v1[i] == v2[i] ) i += 1;
	if( i < n ) return v1[i] - v2[i];
	return 0;
}
static int DVoid2_Compare( void **k1, void **k2 )
{
	if( k1[0] != k2[0] ) return (int)( (size_t)k1[0] - (size_t)k2[0] );
	return (int)( (size_t)k1[1] - (size_t)k2[1] );
}
static int DMap_CompareKeys( DMap *self, void *k1, void *k2 )
{
	switch( self->keytype ){
	case D_STRING : return DString_Compare( (DString*) k1, (DString*) k2 );
	case D_VALUE  : return DValue_Compare( *(DValue*) k1, *(DValue*) k2 );
	case D_ARRAY  : return DArray_Compare( (DArray*) k1, (DArray*) k2 );
	case D_VOID2  : return DVoid2_Compare( (void**) k1, (void**) k2 );
	default : return (int)( (size_t)k1 - (size_t)k2 );
	}
	return 0;
}

static DNode* DMap_FindChild( DMap *self, DNode *root, void *key, KeySearchType type )
{
	DNode *p = root;
	DNode *m = 0;
	int compare;

	if( root == NULL ) return NULL;

	for(;;){
		compare = DMap_CompareKeys( self, key, p->key.pVoid );
		if( compare == 0 ) return p;

		if( compare < 0 ){
			if( type == KEY_MG ) m = p;
			if( p->left ) p = p->left; else break;
		}else{
			if( type == KEY_ML ) m = p;
			if( p->right ) p = p->right; else break;
		}
	}
	return m;
}
static DNode* DMap_FindNode( DMap *self, void *key, KeySearchType type )
{
	DNode *root = self->root;
	int id;
	if( self->hashing ){
		id = DHash_HashIndex( self, key );
		root = self->table[id];
		if( root == NULL ) return NULL;
		/* if( DMap_CompareKeys( self, key, root->key.pVoid ) ==0 ) return root; */
	}
	return DMap_FindChild( self, root, key, type );
}
static DNode* DMap_SimpleInsert( DMap *self, DNode *node )
{
	DNode *p = self->root;
	int compare;
	node->color = RB_RED;
	if( self->root == NULL ) return node;
	for(;;){
		compare = DMap_CompareKeys( self, node->key.pVoid, p->key.pVoid );
		if( compare == 0 ){
			return p;
		}else if( compare < 0 ){
			if( p->left ){
				p = p->left;
			}else{
				p->left = node;
				node->parent = p;
				break;
			}
		}else{
			if( p->right ){
				p = p->right;
			}else{
				p->right = node;
				node->parent = p;
				break;
			}
		}
	}
	return node;
}
DNode* DMap_Insert( DMap *self, void *key, void *value )
{
	DNode *p, *node = DNode_New( self, self->keytype, self->valtype );
	void *okey = node->key.pVoid;
	void *ovalue = node->value.pVoid;
	int id = 0;
	if( self->hashing ){
		id = DHash_HashIndex( self, key );
		node->hash = id;
		self->root = self->table[id];
		if( self->root ==NULL ){
			self->size += 1;
			self->table[id] = node;
			node->color = RB_BLACK;
			DMap_CopyItem( & node->key.pVoid, key, self->keytype );
			DMap_CopyItem( & node->value.pVoid, value, self->valtype );
			return node;
		}
	}
	node->key.pVoid = key;
	node->value.pVoid = value;
	p = DMap_SimpleInsert( self, node );
	node->key.pVoid = okey;
	node->value.pVoid = ovalue;
	if( p == node ){ /* key not exist: */
		DMap_CopyItem( & node->key.pVoid, key, self->keytype );
		DMap_CopyItem( & node->value.pVoid, value, self->valtype );
		DMap_InsertNode( self, node );
		if( self->hashing ){
			self->table[id] = self->root;
			if( self->size >= self->tsize ) DHash_ResetTable( self );
		}
	}else{
		DMap_DeleteItem( p->value.pVoid, self->valtype );
		if( self->valtype != D_VALUE ) p->value.pVoid = NULL;
		DMap_CopyItem( & p->value.pVoid, value, self->valtype );
		dao_free( node );
	}
	return p;
}
void DMap_Erase( DMap *self, void *key )
{
	DMap_EraseNode( self, DMap_FindNode( self, key, KEY_EQ ) );
}
DNode* DMap_Find( DMap *self, void *key )
{
	return DMap_FindNode( self, key, KEY_EQ );
}
DNode* DMap_FindML( DMap *self, void *key )
{
	return DMap_FindNode( self, key, KEY_ML );
}
DNode* DMap_FindMG( DMap *self, void *key )
{
	return DMap_FindNode( self, key, KEY_MG );
}
DNode* DMap_First( DMap *self )
{
	DNode *node = NULL;
	int i = 0;
	if( self == NULL ) return NULL;
	if( self->hashing ){
		while( i < self->tsize && self->table[i] == NULL ) i += 1;
		if( i < self->tsize ) node = DNode_First( self->table[i] );
	}
	if( node == NULL && self->root ) node = DNode_First( self->root );
	return node;
}
DNode* DMap_Next( DMap *self, DNode *node )
{
	DNode *next;
	if( node == NULL ) return NULL;
	next = DNode_Next( node );
	if( next == NULL && self->hashing ){
		int i = node->hash + 1;
		while( i < self->tsize && self->table[i] == NULL ) i += 1;
		if( i < self->tsize ) next = DNode_First( self->table[i] );
	}
	return next;
}


/*
   PUBLIC DOMAIN CODES
http://sites.google.com/site/murmurhash/
http://www.burtleburtle.net/bob/hash/doobs.html
 */

/* -----------------------------------------------------------------------------
   MurmurHash2, by Austin Appleby

   Note - This code makes a few assumptions about how your machine behaves -
   1. We can read a 4-byte value from any address without crashing
   2. sizeof(int) == 4

   And it has a few limitations -
   1. It will not work incrementally.
   2. It will not produce the same results on little-endian and big-endian
   machines.
 */
unsigned int MurmurHash2 ( const void * key, int len, unsigned int seed )
{
	/* 'm' and 'r' are mixing constants generated offline.
	   They're not really 'magic', they just happen to work well. */
	const unsigned int m = 0x5bd1e995;
	const int r = 24;

	/* Initialize the hash to a 'random' value */
	unsigned int h = seed ^ len;

	/* Mix 4 bytes at a time into the hash */
	const unsigned char * data = (const unsigned char *)key;

	while(len >= 4) {
		unsigned int k = *(unsigned int *)data;

		k *= m; 
		k ^= k >> r; 
		k *= m; 

		h *= m; 
		h ^= k;

		data += 4;
		len -= 4;
	}

	/* Handle the last few bytes of the input array */
	switch(len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
			h *= m;
	};

	/* Do a few final mixes of the hash to ensure the last few
	   bytes are well-incorporated. */
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}
