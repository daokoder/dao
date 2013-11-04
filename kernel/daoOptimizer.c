/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2013, Limin Fu
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

#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<assert.h>

#include"daoGC.h"
#include"daoTasklet.h"
#include"daoLexer.h"
#include"daoValue.h"
#include"daoRoutine.h"
#include"daoNamespace.h"
#include"daoVmspace.h"
#include"daoOptimizer.h"


extern DMutex mutex_routine_specialize;

static DaoCnode* DaoCnode_New()
{
	DaoCnode *self = (DaoCnode*) dao_calloc( 1, sizeof(DaoCnode) );
	self->ins = DArray_New(0);
	self->outs = DArray_New(0);
	self->kills = DArray_New(0);
	self->defs = DArray_New(0);
	self->uses = DArray_New(0);
	self->list = DArray_New(0);
	return self;
}
static void DaoCnode_Delete( DaoCnode *self )
{
	DArray_Delete( self->ins );
	DArray_Delete( self->outs );
	DArray_Delete( self->kills );
	DArray_Delete( self->defs );
	DArray_Delete( self->uses );
	DArray_Delete( self->list );
	dao_free( self );
}
static void DaoCnode_Clear( DaoCnode *self )
{
	self->ins->size = self->outs->size = 0;
	self->list->size = 0;
	self->kills->size = 0;
}
void DaoCnode_InitOperands( DaoCnode *self, DaoVmCode *vmc )
{
	uchar_t type = DaoVmCode_GetOpcodeType( vmc );
	int i, k, m;

	self->type = DAO_OP_NONE;
	self->first = self->second = self->third = 0xffff;
	self->lvalue = self->lvalue2 = 0xffff;
	self->exprid = 0xffff;
	switch( type ){
	case DAO_CODE_NOP :
		break;
	case DAO_CODE_GETC :
	case DAO_CODE_GETG :
		self->lvalue = vmc->c;
		break;
	case DAO_CODE_SETG :
		self->type = DAO_OP_SINGLE;
		self->first = vmc->a;
		break;
	case DAO_CODE_GETF :
	case DAO_CODE_MOVE :
	case DAO_CODE_UNARY :
		self->type = DAO_OP_SINGLE;
		self->first = vmc->a;
		self->lvalue = vmc->c;
		break;
	case DAO_CODE_SETU :
		if( vmc->c == 0 ){
			self->type = DAO_OP_SINGLE;
			self->first = vmc->a;
		}else{
			self->type = DAO_OP_PAIR;
			self->first = vmc->a;
			self->second = vmc->b;
			self->lvalue2 = vmc->b;
		}
		break;
	case DAO_CODE_SETF :
		self->type = DAO_OP_PAIR;
		self->first = vmc->a;
		self->second = vmc->c;
		self->lvalue2 = vmc->c;
		break;
	case DAO_CODE_SETI :
		self->type = DAO_OP_TRIPLE;
		self->first = vmc->a;
		self->second = vmc->b;
		self->third = vmc->c;
		self->lvalue2 = vmc->c;
		break;
	case DAO_CODE_GETI :
	case DAO_CODE_BINARY :
		self->type = DAO_OP_PAIR;
		self->first = vmc->a;
		self->second = vmc->b;
		self->lvalue = vmc->c;
		break;
	case DAO_CODE_GETU :
		if( vmc->a != 0 ){
			self->type = DAO_OP_SINGLE;
			self->first = vmc->b;
		}
		self->lvalue = vmc->c;
		break;
	case DAO_CODE_UNARY2 :
		self->type = DAO_OP_SINGLE;
		self->first = vmc->b;
		self->lvalue = vmc->c;
		break;
	case DAO_CODE_GETM :
	case DAO_CODE_ENUM :
	case DAO_CODE_ENUM2 :
	case DAO_CODE_ROUTINE :
		self->type = DAO_OP_RANGE;
		self->first = vmc->a;
		self->second = vmc->a + vmc->b - (type == DAO_CODE_ENUM);
		self->lvalue = vmc->c;
		break;
	case DAO_CODE_SETM:
		self->type = DAO_OP_RANGE2;
		self->first = vmc->c;
		self->second = vmc->c + vmc->b;
		self->third = vmc->a;
		self->lvalue2 = vmc->c;
		break;
	case DAO_CODE_CALL :
		k = vmc->b & 0xff;
		self->type = DAO_OP_RANGE;
		self->first = vmc->a;
		self->second = vmc->a + k;
		self->lvalue = vmc->c;
		break;
	case DAO_CODE_MATRIX :
		k = vmc->b & 0xff;
		m = vmc->b >> 8;
		self->type = DAO_OP_RANGE;
		self->first = vmc->a;
		self->second = vmc->a + k*m - 1;
		self->lvalue = vmc->c;
		break;
	case DAO_CODE_BRANCH :
		self->type = DAO_OP_SINGLE;
		self->first = vmc->a;
		break;
	case DAO_CODE_YIELD :
	case DAO_CODE_EXPLIST :
		self->type = DAO_OP_RANGE;
		self->first = vmc->a;
		self->second = vmc->a + vmc->b - 1;
		if( type == DAO_CODE_YIELD ) self->lvalue = vmc->c;
		break;
	default: break;
	}
	/* Handle zero number of variables in the range: */
	if( self->type == DAO_OP_RANGE && self->second == 0xffff ) self->type = DAO_OP_NONE;
}
int DaoCnode_FindResult( DaoCnode *self, void *key )
{
	int first = 0, last = self->list->size - 1;
	while( first <= last ){
		int mid = (first + last) / 2;
		void *val = self->list->items.pVoid[mid];
		if( val == key ){
			return mid;
		}else if( val < key ){
			first = mid + 1;
		}else{
			last = mid - 1;
		}
	}
	return -1;
}


DaoOptimizer* DaoOptimizer_New()
{
	DaoOptimizer *self = (DaoOptimizer*) dao_malloc( sizeof(DaoOptimizer) );
	self->routine = NULL;
	self->array = DArray_New(0); /* DArray<daoint> */
	self->array2 = DArray_New(0); /* DArray<DaoCnode*> */
	self->array3 = DArray_New(0); /* DArray<DaoCnode*> */
	self->nodeCache  = DArray_New(0); /* DArray<DaoCnode*> */
	self->arrayCache = DArray_New(D_ARRAY); /* DArray<DArray<DaoCnode*>> */
	self->nodes = DArray_New(0);  /* DArray<DaoCnode*> */
	self->enodes = DArray_New(0);  /* DArray<DaoCnode*> */
	self->uses  = DArray_New(0);  /* DArray<DArray<DaoCnode*>> */
	self->refers  = DArray_New(0);  /* DArray<daoint> */
	self->exprs = DHash_New(D_VMCODE,0); /* DMap<DaoVmCode*,int> */
	self->inits = DHash_New(0,0);   /* DMap<DaoCnode*,int> */
	self->finals = DHash_New(0,0);  /* DMap<DaoCnode*,int> */
	self->tmp = DHash_New(0,0);
	self->reverseFlow = 0;
	self->update = NULL;
	return self;
}
void DaoOptimizer_Clear( DaoOptimizer *self )
{
	self->nodes->size = 0;
	self->enodes->size = 0;
	self->uses->size = 0;
	DMap_Reset( self->inits );
	DMap_Reset( self->finals );
	DMap_Reset( self->exprs );
}
void DaoOptimizer_Delete( DaoOptimizer *self )
{
	daoint i;
	for(i=0; i<self->nodeCache->size; i++) DaoCnode_Delete( self->nodeCache->items.pCnode[i] );
	DArray_Delete( self->array );
	DArray_Delete( self->array2 );
	DArray_Delete( self->array3 );
	DArray_Delete( self->nodeCache );
	DArray_Delete( self->arrayCache );
	DArray_Delete( self->enodes );
	DArray_Delete( self->nodes );
	DArray_Delete( self->uses );
	DArray_Delete( self->refers );
	DMap_Delete( self->inits );
	DMap_Delete( self->finals );
	DMap_Delete( self->exprs );
	DMap_Delete( self->tmp );
	dao_free( self );
}

void DaoRoutine_FormatCode( DaoRoutine *self, int i, DaoVmCodeX vmc, DString *output );
static int DaoOptimizer_UpdateRDA( DaoOptimizer *self, DaoCnode *first, DaoCnode *second );
static int DaoOptimizer_UpdateAEA( DaoOptimizer *self, DaoCnode *first, DaoCnode *second );

static void DaoOptimizer_Print( DaoOptimizer *self )
{
	DNode *it;
	DaoRoutine *routine = self->routine;
	DaoStream *stream = routine->nameSpace->vmSpace->stdioStream;
	DaoVmCodeX **vmCodes = routine->body->annotCodes->items.pVmc;
	DString *annot = DString_New(1);
	daoint i, j, k, m, n;

	DaoStream_WriteMBS( stream, "============================================================\n" );
	DaoStream_WriteMBS( stream, daoRoutineCodeHeader );
	for( j=0,n=routine->body->annotCodes->size; j<n; j++){
		DaoCnode *node = self->nodes->items.pCnode[j];
		DaoRoutine_FormatCode( routine, j, *vmCodes[j], annot );
		DString_Chop( annot );
		while( annot->size < 80 ) DString_AppendChar( annot, ' ' );
		DString_AppendMBS( annot, "| " );
		DaoStream_WriteString( stream, annot );
		if( self->update == DaoOptimizer_UpdateAEA ){
			k = -1;
			if( node->exprid != 0xffff ) k = self->enodes->items.pCnode[node->exprid]->index;
			DaoStream_WriteInt( stream, k );
			DaoStream_WriteMBS( stream, " | " );
			for(i=0; i<node->list->size; ++i){
				DaoStream_WriteInt( stream, node->list->items.pCnode[i]->index );
				DaoStream_WriteMBS( stream, ", " );
			}
		}else{
			DaoStream_WriteInt( stream, j );
			DaoStream_WriteMBS( stream, " | " );
			for(i=0; i<node->list->size; ++i){
				DaoStream_WriteInt( stream, node->list->items.pInt[i] );
				DaoStream_WriteMBS( stream, ", " );
			}
		}
		DaoStream_WriteMBS( stream, "\n" );
	}
	DString_Delete( annot );
}



static int DaoAEA_Compare( DaoCnode *node1, DaoCnode *node2, DaoVmCode *codes )
{
	DaoVmCode c1 = codes[node1->index];
	DaoVmCode c2 = codes[node2->index];
	if( c1.code != c2.code ) return c1.code - c2.code;
	if( c1.a != c2.a ) return c1.a - c2.a;
	if( c1.b != c2.b ) return c1.b - c2.b;
	return node1->index - node2->index;
}
static int DaoAEA_Compare2( DaoCnode *node1, DaoCnode *node2, DaoVmCode *codes )
{
	DaoVmCode c1 = codes[node1->index];
	DaoVmCode c2 = codes[node2->index];
	if( c1.code != c2.code ) return c1.code - c2.code;
	if( c1.a != c2.a ) return c1.a - c2.a;
	return c1.b - c2.b;
}
static void DaoAEA_Sort( DaoCnode **nodes, int first, int last, DaoVmCode *codes )
{
	int lower=first+1, upper=last;
	DaoCnode *val;
	DaoCnode *pivot;
	if( first >= last ) return;
	val = nodes[first];
	nodes[first] = nodes[ (first+last)/2 ];
	nodes[ (first+last)/2 ] = val;
	pivot = nodes[ first ];

	while( lower <= upper ){
		while( lower < last && DaoAEA_Compare( nodes[lower], pivot, codes ) < 0 ) lower ++;
		while( upper > first && DaoAEA_Compare( pivot, nodes[upper], codes ) < 0 ) upper --;
		if( lower < upper ){
			val = nodes[lower];
			nodes[lower] = nodes[upper];
			nodes[upper] = val;
			upper --;
		}
		lower ++;
	}
	val = nodes[first];
	nodes[first] = nodes[upper];
	nodes[upper] = val;
	if( first+1 < upper ) DaoAEA_Sort( nodes, first, upper-1, codes );
	if( upper+1 < last  ) DaoAEA_Sort( nodes, upper+1, last, codes );
}
static void DaoOptimizer_InitNodeAEA( DaoOptimizer *self, DaoCnode *node )
{
	node->list->size = 0;
	if( DMap_Find( self->inits, node ) == 0 ){
		DaoVmCode *codes = self->routine->body->vmCodes->data.codes;
		daoint i;
		for(i=0; i<node->index; ++i){
			DaoCnode *node2 = self->nodes->items.pCnode[i];
			if( node2->exprid != 0xffff ) DArray_Append( node->list, node2 );
		}
		DaoAEA_Sort( node->list->items.pCnode, 0, node->list->size - 1, codes );
	}
}
/* Transfer function for Available Expression Analysis: */
static void DaoOptimizer_AEA( DaoOptimizer *self, DaoCnode *node, DArray *out )
{
	DaoVmCode *codes = self->routine->body->vmCodes->data.codes;
	DaoCnode *genAE = node->exprid == 0xffff ? NULL : node;
	daoint i, j, pushed = 0;

	switch( node->type ){
	case DAO_OP_SINGLE :
		if( node->lvalue == node->first ) genAE = NULL;
		break;
	case DAO_OP_PAIR :
		if( node->lvalue == node->first ) genAE = NULL;
		if( node->lvalue == node->second ) genAE = NULL;
		break;
	case DAO_OP_RANGE :
		if( node->first <= node->lvalue && node->lvalue <= node->second ) genAE = NULL;
		break;
	case DAO_OP_TRIPLE :
	case DAO_OP_RANGE2 :
		genAE = NULL;
		break;
	}

#if 0
	DMap_Assign( out, node->set );
	for(i=0; i<node->kills->size; i++) DMap_Erase( out, node->kills->items.pVoid[i] );
	if( genAE != NULL ) DMap_Insert( out, genAE, NULL );
#endif

	out->size = 0;
	for(i=0,j=0; i<node->list->size; ){
		DaoCnode *node1 = node->list->items.pCnode[i];
		/* Skip the node if it is in the kills: */
		if( j < node->kills->size ){
			DaoCnode *node2 = node->kills->items.pCnode[j];
			if( node1->index == node2->index ){
				i += 1;
				j += 1;
				continue;
			}else if( DaoAEA_Compare( node1, node2, codes ) > 0 ){
				j += 1;
				continue;
			}
		}
		i += 1;
		if( genAE != NULL ){
			//if( DaoAEA_Compare2( node1, genAE, codes ) == 0 ) continue;
			if( node1 == genAE ) pushed = 1;
			if( pushed == 0 && DaoAEA_Compare( genAE, node1, codes ) < 0 ){
				DArray_Append( out, genAE );
				pushed = 1;
			}
		}
		DArray_Append( out, node1 );
	}
	if( genAE != NULL && pushed == 0 ) DArray_Append( out, genAE );
}
static int DaoOptimizer_UpdateAEA( DaoOptimizer *self, DaoCnode *first, DaoCnode *second )
{
	DNode *it, *it2;
	DaoCnode **cnodes1, **cnodes2;
	DaoVmCode *codes = self->routine->body->vmCodes->data.codes;
	daoint i, j, k, m, size1, size2, changes = 0;

	DaoOptimizer_AEA( self, first, self->array3 );

	/* Intersection: */
	DArray_Assign( self->array2, second->list );
	cnodes1 = self->array2->items.pCnode;
	cnodes2 = self->array3->items.pCnode;
	size1 = self->array2->size;
	size2 = self->array3->size;
	second->list->size = 0;
	for(i=0, j=0; i<size1 && j<size2; ){
		DaoCnode *node1 = cnodes1[i];
		DaoCnode *node2 = cnodes2[j];
		int cmp = DaoAEA_Compare2( node1, node2, codes );
		if( cmp == 0 ){
			/*
			// Find all the equivalent expressions;
			// And push them only once to second->list;
			*/
			for(k=i+1;  k<size1 && DaoAEA_Compare2( node1, cnodes1[k], codes ) ==0; ) k += 1;
			for(m=j+1;  m<size2 && DaoAEA_Compare2( node2, cnodes2[m], codes ) ==0; ) m += 1;
			while( i < k || j < m ){
				if( i < k && j < m ){
					DaoCnode *node1 = cnodes1[i];
					DaoCnode *node2 = cnodes2[j];
					if( node1->index == node2->index ){
						DArray_Append( second->list, node1 );
						i += 1;
						j += 1;
					}else if( node1->index < node2->index ){
						DArray_Append( second->list, node1 );
						i += 1;
					}else{
						DArray_Append( second->list, node2 );
						changes += 1;
						j += 1;
					}
				}else if( i < k ){
					DArray_Append( second->list, cnodes1[i] );
					i += 1;
				}else{
					DArray_Append( second->list, cnodes2[j] );
					changes += 1;
					j += 1;
				}
			}
		}else if( cmp < 0 ){
			changes += 1;
			i += 1;
		}else{
			j += 1;
		}
	}
	return changes;
}


static void Dao_SortInts( daoint *values, int first, int last )
{
	int lower=first+1, upper=last;
	daoint val;
	daoint pivot;
	if( first >= last ) return;
	val = values[first];
	values[first] = values[ (first+last)/2 ];
	values[ (first+last)/2 ] = val;
	pivot = values[ first ];

	while( lower <= upper ){
		while( lower < last  && values[lower] < pivot ) lower ++;
		while( upper > first && pivot < values[upper] ) upper --;
		if( lower < upper ){
			val = values[lower];
			values[lower] = values[upper];
			values[upper] = val;
			upper --;
		}
		lower ++;
	}
	val = values[first];
	values[first] = values[upper];
	values[upper] = val;
	if( first+1 < upper ) Dao_SortInts( values, first, upper-1 );
	if( upper+1 < last  ) Dao_SortInts( values, upper+1, last );
}
static int Dao_IntsUnion( DArray *first, DArray *second, DArray *output, daoint excluding )
{
	daoint size1 = first->size;
	daoint size2 = second->size;
	daoint i, j, changes = 0;
	output->size = 0;
	for(i=0, j=0; i<size1 || j<size2; ){
		if( i < size1 && first->items.pInt[i] == excluding ){
			i += 1;
			continue;
		}
		if( i < size1 && j < size2 ){
			daoint id1 = first->items.pInt[i];
			daoint id2 = second->items.pInt[j];
			if( id1 == id2 ){
				DArray_Append( output, IntToPointer(id1) );
				i += 1;
				j += 1;
			}else if( id1 < id2 ){
				DArray_Append( output, IntToPointer(id1) );
				i += 1;
			}else{
				DArray_Append( output, IntToPointer(id2) );
				changes += 1;
				j += 1;
			}
		}else if( i < size1 ){
			DArray_Append( output, first->items.pVoid[i] );
			i += 1;
		}else{
			DArray_Append( output, second->items.pVoid[j] );
			changes += 1;
			j += 1;
		}
	}
	return changes;
}

#define RDA_OFFSET  0xffff

/*
// For Reaching Definition Analysis:
// (x,?): is represented by the variable index of "x";
// (x,lab): is represented by the index of "lab" + 0xffff;
*/
static void DaoOptimizer_InitNodeRDA( DaoOptimizer *self, DaoCnode *node )
{
	int i;
	node->list->size = 0;
	if( DMap_Find( self->inits, node ) ){
		for(i=0; i<self->routine->body->regCount; i++)
			DArray_Append( node->list, IntToPointer(i) );
	}
}
/* Transfer function for Reaching Definition Analysis: */
static void DaoOptimizer_RDA( DaoOptimizer *self, DaoCnode *node, DArray *out )
{
	DArray *kills;
	daoint i, j, pushed = 0;

	if( node->lvalue == 0xffff ){
		DArray_Assign( out, node->list );
		return;
	}
	out->size = 0;
	kills = self->uses->items.pArray[node->lvalue];
	for(i=0,j=0; i<node->list->size; ){
		daoint id = node->list->items.pInt[i];
		if( id == node->lvalue ){
			i += 1;
			continue;
		}else if( j < kills->size ){
			/* Skip if it is in the kills: */
			daoint id2 = kills->items.pInt[j];
			if( id == id2 ){
				i += 1;
				j += 1;
				continue;
			}else if( id > id2 ){
				j += 1;
				continue;
			}
		}
		i += 1;
		if( (node->index + RDA_OFFSET) == id ) pushed = 1;
		if( pushed == 0 && (node->index + RDA_OFFSET) < id ){
			DArray_Append( out, IntToPointer( node->index + RDA_OFFSET ) );
			pushed = 1;
		}
		DArray_Append( out, IntToPointer( id ) );
	}
	if( pushed == 0 ) DArray_Append( out, IntToPointer( node->index + RDA_OFFSET ) );
#if 0
	DMap_Erase( out, IntToPointer(node->lvalue) );
	for(i=0; i<kills->size; i++) DMap_Erase( out, kills->items.pVoid[i] );
	DMap_Insert( out, IntToPointer(node->index + RDA_OFFSET), NULL );
#endif
}
static int DaoOptimizer_UpdateRDA( DaoOptimizer *self, DaoCnode *first, DaoCnode *second )
{
	DaoOptimizer_RDA( self, first, self->array3 );
	/* Union: */
#if 0
	for(it=DMap_First(self->tmp); it; it=DMap_Next(self->tmp,it)){
		if( DMap_Find( second->set, it->key.pVoid ) == NULL ){
			DMap_Insert( second->set, it->key.pVoid, NULL );
			changed = 1;
		}
	}
#endif
	DArray_Assign( self->array2, second->list );
	return Dao_IntsUnion( self->array2, self->array3, second->list, -1 );
}


static void DaoCnode_GetOperands( DaoCnode *self, DArray *operands )
{
	int i;
	operands->size = 0;
	switch( self->type ){
	case DAO_OP_SINGLE :
		DArray_Append( operands, IntToPointer(self->first) );
		break;
	case DAO_OP_PAIR   :
		DArray_Append( operands, IntToPointer(self->first) );
		DArray_Append( operands, IntToPointer(self->second) );
		break;
	case DAO_OP_TRIPLE :
		DArray_Append( operands, IntToPointer(self->first) );
		DArray_Append( operands, IntToPointer(self->second) );
		DArray_Append( operands, IntToPointer(self->third) );
		break;
	case DAO_OP_RANGE :
	case DAO_OP_RANGE2 :
		for(i=self->first; i<=self->second; i++) DArray_Append( operands, IntToPointer(i) );
		if( self->type == DAO_OP_RANGE2 ) DArray_Append( operands, IntToPointer(self->third) );
		break;
	}
	Dao_SortInts( operands->items.pInt, 0, operands->size-1 );
}
static void DaoOptimizer_InitNodeLVA( DaoOptimizer *self, DaoCnode *node )
{
	int i;
	node->list->size = 0;
	if( DMap_Find( self->finals, node ) ) DaoCnode_GetOperands( node, node->list );
}
/* Transfer function for Live Variable Analysis: */
static void DaoOptimizer_LVA( DaoOptimizer *self, DaoCnode *node, DArray *out )
{
	ushort_t i;

	DaoCnode_GetOperands( node, self->array2 );
	Dao_IntsUnion( node->list, self->array2, out, node->lvalue );

#if 0
	DMap_Assign( out, node->set );
	if( node->lvalue != 0xffff ) DMap_Erase( out, IntToPointer(node->lvalue) );
	switch( node->type ){
	case DAO_OP_SINGLE :
		DMap_Insert( out, IntToPointer(node->first), NULL );
		break;
	case DAO_OP_PAIR   :
		DMap_Insert( out, IntToPointer(node->first), NULL );
		DMap_Insert( out, IntToPointer(node->second), NULL );
		break;
	case DAO_OP_TRIPLE :
		DMap_Insert( out, IntToPointer(node->first), NULL );
		DMap_Insert( out, IntToPointer(node->second), NULL );
		DMap_Insert( out, IntToPointer(node->third), NULL );
		break;
	case DAO_OP_RANGE :
	case DAO_OP_RANGE2 :
		for(i=node->first; i<=node->second; i++) DMap_Insert( out, IntToPointer(i), NULL );
		if( node->type == DAO_OP_RANGE2 ) DMap_Insert( out, IntToPointer(node->third), NULL );
		break;
	}
#endif
}
static int DaoOptimizer_UpdateLVA( DaoOptimizer *self, DaoCnode *first, DaoCnode *second )
{
	DaoOptimizer_LVA( self, first, self->array3 );

#if 0
	DaoOptimizer_LVA( self, first, self->tmp );
	for(it=DMap_First(self->tmp); it; it=DMap_Next(self->tmp,it)){
		if( DMap_Find( second->set, it->key.pVoid ) == NULL ){
			DMap_Insert( second->set, it->key.pVoid, NULL );
			changed = 1;
		}
	}
#endif
	/* Union: */
	DArray_Assign( self->array2, second->list );
	return Dao_IntsUnion( self->array2, self->array3, second->list, -1 );
}
static void DaoOptimizer_ProcessWorklist( DaoOptimizer *self, DArray *worklist )
{
	daoint i;
	daoint m = 0;
	/* printf( "DaoOptimizer_ProcessWorklist: %i\n", (int)worklist->size ); */
	while( worklist->size ){
		DaoCnode *second = (DaoCnode*) DArray_PopBack( worklist );
		DaoCnode *first = (DaoCnode*) DArray_PopBack( worklist );
		/* if( (++m) % 10000 ==0 ) printf( "%9i  %9i\n", (int)m, (int)worklist->size ); */
		if( self->update( self, first, second ) == 0 ) continue;
		if( self->reverseFlow ){
			for(i=0; i<second->ins->size; i++){
				DArray_PushBack( worklist, second );
				DArray_PushBack( worklist, second->ins->items.pVoid[i] );
			}
		}else{
			for(i=0; i<second->outs->size; i++){
				DArray_PushBack( worklist, second );
				DArray_PushBack( worklist, second->outs->items.pVoid[i] );
			}
		}
	}
}
static void DaoOptimizer_SolveFlowEquation( DaoOptimizer *self )
{
	DArray *worklist = DArray_New(0);
	DaoCnode *node, **nodes = self->nodes->items.pCnode;
	daoint i, j, N = self->nodes->size;
	if( self->reverseFlow ){
		for(i=0; i<N; ++i){
			node = nodes[i];
			self->init( self, node );
			for(j=0; j<node->outs->size; j++){
				DArray_PushBack( worklist, node->outs->items.pVoid[j] );
				DArray_PushBack( worklist, node );
			}
		}
	}else{
		for(i=N-1; i>=0; --i){
			node = nodes[i];
			self->init( self, node );
			for(j=0; j<node->outs->size; j++){
				DArray_PushBack( worklist, node );
				DArray_PushBack( worklist, node->outs->items.pVoid[j] );
			}
		}
	}
	DaoOptimizer_ProcessWorklist( self, worklist );
	DArray_Delete( worklist );
}

static void DaoOptimizer_InitNode( DaoOptimizer *self, DaoCnode *node, DaoVmCode *code );

static int DaoVmCode_MayCreateReference( int code )
{
	switch( code ){
	case DVM_GETI_LI : case DVM_GETI_LSI :
	case DVM_GETI_TI : case DVM_GETF_TX :
	case DVM_GETF_KC : case DVM_GETF_KG :
	case DVM_GETF_OC : case DVM_GETF_OG : case DVM_GETF_OV :
		return 1;
	default : return code < DVM_DEBUG;
	}
	return 0;
}

/* Optimization should be done on routine->body->annotCodes,
// which should not contain opcodes such as DEBUG, JITC or SAFE_GOTO etc. */
static void DaoOptimizer_Init( DaoOptimizer *self, DaoRoutine *routine )
{
	DaoCnode *node, **nodes;
	DaoVmCode *vmc, **codes = (DaoVmCode**)routine->body->annotCodes->items.pVmc;
	daoint N = routine->body->annotCodes->size;
	daoint M = routine->body->regCount;
	daoint i, j, k;

	self->routine = routine;
	DaoOptimizer_Clear( self );
	DArray_Resize( self->refers, M, 0 );
	if( self->nodeCache->size < N ) DArray_Resize( self->nodeCache, N, NULL );
	if( self->arrayCache->size < M ){
		DArray *array = DArray_New(0);
		DArray_Resize( self->arrayCache, M, array );
		DArray_Delete( array );
	}
	for(i=0; i<M; i++){
		DArray *array = self->arrayCache->items.pArray[i];
		DArray_Append( self->uses, array );
		self->refers->items.pInt[i] = 0;
		array->size = 0;
	}
	for(i=0; i<N; i++){
		node = self->nodeCache->items.pCnode[i];
		if( node == NULL ){
			node = DaoCnode_New();
			self->nodeCache->items.pCnode[i] = node;
		}
		node->index = i;
		DaoCnode_Clear( node );
		DaoOptimizer_InitNode( self, node, codes[i] );
		if( node->lvalue != 0xffff ){
			self->refers->items.pInt[node->lvalue] = DaoVmCode_MayCreateReference( codes[i]->code );
		}
		DArray_Append( self->nodes, node );
	}
#if 0
	printf( "number of nodes: %i, number of registers: %i\n", self->nodes->size, M );
	printf( "number of interesting expression: %i\n", self->exprs->size );
#endif
	nodes = self->nodes->items.pCnode;
	DMap_Insert( self->inits, nodes[0], NULL );
	for(i=0; i<N; i++){
		vmc = codes[i];
		node = nodes[i];
		if( i && vmc->code != DVM_CASE ){
			k = codes[i-1]->code;
			if( k != DVM_GOTO && k != DVM_RETURN ){
				DArray_Append( nodes[i-1]->outs, node );
				DArray_Append( node->ins, nodes[i-1] );
			}else if( vmc->code == DVM_SECT || (vmc->code == DVM_GOTO && vmc->c == DVM_SECT) ){
				/* Code section is isolated from the main codes: */
				DArray_Append( nodes[i-1]->outs, node );
				DArray_Append( node->ins, nodes[i-1] );
			}
		}
		switch( vmc->code ){
		case DVM_GOTO : case DVM_CASE :
		case DVM_TEST : case DVM_TEST_I : case DVM_TEST_F : case DVM_TEST_D :
			DArray_Append( node->outs, nodes[vmc->b] );
			DArray_Append( nodes[vmc->b]->ins, node );
			break;
		case DVM_SWITCH :
			DArray_Append( node->outs, nodes[vmc->b] );
			DArray_Append( nodes[vmc->b]->ins, node );
			for(j=1; j<=vmc->c; j++){
				DArray_Append( node->outs, nodes[i+j] );
				DArray_Append( nodes[i+j]->ins, node );
			}
			break;
		case DVM_SECT :
			/*
			// Expressions outside should NOT be available inside code sections.
			// Otherwise, due to Common Sub-expression Elimination, values from
			// outside maybe access by instructions other than GETVH and SETVH.
			// This will break the basic assumption that code sections always
			// accesss outside values through GETVH and SETVH, as a result some
			// code section methods may not work, for example, mt.start::{} in
			// parallel quicksort.
			*/
			DMap_Insert( self->inits, node, NULL );
			break;
		case DVM_RETURN : DMap_Insert( self->finals, node, NULL ); break;
		default : break;
		}
	}
}
static void DaoOptimizer_InitKills( DaoOptimizer *self );
static void DaoOptimizer_InitAEA( DaoOptimizer *self, DaoRoutine *routine )
{
	self->reverseFlow = 0;
	self->init = DaoOptimizer_InitNodeAEA;
	self->update = DaoOptimizer_UpdateAEA;
	DaoOptimizer_Init( self, routine );
	DaoOptimizer_InitKills( self );
}
static void DaoOptimizer_InitNodesRDA( DaoOptimizer *self );
static void DaoOptimizer_InitRDA( DaoOptimizer *self, DaoRoutine *routine )
{
	self->reverseFlow = 0;
	self->init = DaoOptimizer_InitNodeRDA;
	self->update = DaoOptimizer_UpdateRDA;
	DaoOptimizer_Init( self, routine );
	DaoOptimizer_InitNodesRDA( self );
}
static void DaoOptimizer_InitLVA( DaoOptimizer *self, DaoRoutine *routine )
{
	self->reverseFlow = 1;
	self->init = DaoOptimizer_InitNodeLVA;
	self->update = DaoOptimizer_UpdateLVA;
	DaoOptimizer_Init( self, routine );
}
void DaoOptimizer_DoLVA( DaoOptimizer *self, DaoRoutine *routine )
{
	DaoOptimizer_InitLVA( self, routine );
	DaoOptimizer_SolveFlowEquation( self );
}
void DaoOptimizer_DoRDA( DaoOptimizer *self, DaoRoutine *routine )
{
	DaoOptimizer_InitRDA( self, routine );
	DaoOptimizer_SolveFlowEquation( self );
}
void DaoOptimizer_LinkDU( DaoOptimizer *self, DaoRoutine *routine )
{
	DNode *it;
	DaoCnode *node, *node2, **nodes;
	daoint i, j, N, M = routine->body->regCount;

	DaoOptimizer_InitRDA( self, routine );
	DaoOptimizer_SolveFlowEquation( self );

	nodes = self->nodes->items.pCnode;
	N = self->nodes->size;
	for(i=0; i<N; i++){
		node = nodes[i];
		node->defs->size = 0;
		node->uses->size = 0;
	}
	for(i=0; i<N; i++){
		node = nodes[i];
		for(j=0; j<node->list->size; ++j){
			int id = node->list->items.pInt[j];
			int uses = 0;
			if( id < RDA_OFFSET ) continue;
			node2 = self->nodes->items.pCnode[id-RDA_OFFSET];
			switch( node->type ){
			case DAO_OP_SINGLE :
				uses = node->first == node2->lvalue;
				break;
			case DAO_OP_PAIR   :
				uses = node->first == node2->lvalue;
				uses |= node->second == node2->lvalue;
				break;
			case DAO_OP_TRIPLE :
				uses = node->first == node2->lvalue;
				uses |= node->second == node2->lvalue;
				uses |= node->third == node2->lvalue;
				break;
			case DAO_OP_RANGE :
			case DAO_OP_RANGE2 :
				uses = node->first <= node2->lvalue && node2->lvalue <= node->second;
				if( node->type == DAO_OP_RANGE2 ) uses |= node->third == node2->lvalue;
				break;
			}
			if( uses ){
				DArray_Append( node->defs, node2 );
				DArray_Append( node2->uses, node );
			}
		}
	}
#if 0
	DaoOptimizer_Print( self );
	for(i=0; i<N; i++){
		node = nodes[i];
		printf( "%03i: ", i );
		DaoVmCode_Print( routine->body->vmCodes->data.codes[i], NULL );
		for(j=0; j<node->defs->size; j++) printf( "%3i ", node->defs->items.pCnode[j]->index );
		printf("\n");
		for(j=0; j<node->uses->size; j++) printf( "%3i ", node->uses->items.pCnode[j]->index );
		printf("\n\n" );
	}
#endif
}

static int DaoRoutine_IsVolatileParameter( DaoRoutine *self, int id )
{
	DaoType *T;
	if( id >= self->parCount ) return 0;
	if( id >= self->routType->nested->size ) return 1;
	T = self->routType->nested->items.pType[id];
	if( T && (T->tid == DAO_PAR_NAMED || T->tid == DAO_PAR_DEFAULT) ) T = (DaoType*) T->aux;
	if( T == NULL || T->tid == DAO_UDT || T->tid >= DAO_ARRAY  ) return 1;
	return 0;
}
static void DaoOptimizer_AddKill( DaoOptimizer *self, DMap *out, int kill )
{
	daoint i;
	DArray *kills = self->uses->items.pArray[kill];
	for(i=0; i<kills->size; i++){
		DaoCnode *node = kills->items.pCnode[i];
		if( node->exprid != 0xffff ) MAP_Insert( out, node, 0 );
	}
}
static void DaoOptimizer_InitKills( DaoOptimizer *self )
{
	DNode *it;
	DMap *kills = self->tmp;
	DaoType **types = self->routine->body->regType->items.pType;
	DaoCnode *node, **nodes = self->nodes->items.pCnode;
	DaoVmCodeX *vmc, **codes = self->routine->body->annotCodes->items.pVmc;
	DaoVmCode *codes2 = self->routine->body->vmCodes->data.codes;
	daoint i, j, N = self->nodes->size;
	daoint at, bt, ct, code, lvalue, overload;
	for(i=0; i<N; i++){
		vmc = codes[i];
		node = nodes[i];
		DMap_Reset( kills );
		lvalue = node->lvalue != 0xffff ? node->lvalue : node->lvalue2;
		if( lvalue != 0xffff ) DaoOptimizer_AddKill( self, kills, lvalue );

		/* Check if the operation may modify its arguments or operands: */
		code = DaoVmCode_GetOpcodeType( (DaoVmCode*) vmc );
		switch( code ){
		case DAO_CODE_CALL :
		case DAO_CODE_YIELD :
			for(j=node->first; j<=node->second; j++) DaoOptimizer_AddKill( self, kills, j );
			break;
		case DAO_CODE_GETF :
		case DAO_CODE_GETI :
		case DAO_CODE_GETM :
			at = types[node->first] ? types[node->first]->tid : DAO_UDT;
			overload = (at & DAO_ANY) || at == DAO_VARIANT;
			overload |= at >= DAO_OBJECT && at <= DAO_CTYPE;
			if( overload == 0 ) break;
			DaoOptimizer_AddKill( self, kills, node->first );
			if( code == DAO_CODE_GETI ) DaoOptimizer_AddKill( self, kills, node->second );
			/* node->lvalue must already be in kills; */
			if( code != DAO_CODE_GETM ) break;
			for(j=node->first; j<=node->second; j++) DaoOptimizer_AddKill( self, kills, j );
			break;
		case DAO_CODE_SETF :
		case DAO_CODE_SETI :
		case DAO_CODE_SETM :
			ct = types[node->lvalue2] ? types[node->lvalue2]->tid : DAO_UDT;
			overload = (ct & DAO_ANY) || ct == DAO_VARIANT;
			overload |= ct >= DAO_OBJECT && ct <= DAO_CTYPE;
			if( overload == 0 ) break;
			DaoOptimizer_AddKill( self, kills, node->first );
			if( code == DAO_CODE_SETI ) DaoOptimizer_AddKill( self, kills, node->second );
			/* node->lvalue2 must already be in kills; */
			if( code != DAO_CODE_SETM ) break;
			for(j=node->first; j<=node->second; j++) DaoOptimizer_AddKill( self, kills, j );
			DaoOptimizer_AddKill( self, kills, node->third );
			break;
		case DAO_CODE_MOVE :
		case DAO_CODE_UNARY :
			at = types[node->first] ? types[node->first]->tid : DAO_UDT;
			ct = types[node->lvalue] ? types[node->lvalue]->tid : DAO_UDT;
			overload  = (at & DAO_ANY) || at == DAO_VARIANT;
			overload |= (ct & DAO_ANY) || ct == DAO_VARIANT;
			overload |= at >= DAO_OBJECT && at <= DAO_CTYPE;
			overload |= ct >= DAO_OBJECT && ct <= DAO_CTYPE;
			if( overload == 0 ) break;
			DaoOptimizer_AddKill( self, kills, node->first );
			/* node->lvalue must already be in kills; */
			break;
		case DAO_CODE_BINARY :
			at = types[node->first] ? types[node->first]->tid : DAO_UDT;
			bt = types[node->second] ? types[node->second]->tid : DAO_UDT;
			ct = types[node->lvalue] ? types[node->lvalue]->tid : DAO_UDT;
			overload  = (at & DAO_ANY) || at == DAO_VARIANT;
			overload |= (bt & DAO_ANY) || bt == DAO_VARIANT;
			overload |= (ct & DAO_ANY) || ct == DAO_VARIANT;
			overload |= at >= DAO_OBJECT && at <= DAO_CTYPE;
			overload |= bt >= DAO_OBJECT && bt <= DAO_CTYPE;
			overload |= ct >= DAO_OBJECT && ct <= DAO_CTYPE;
			if( overload == 0 ) break;
			DaoOptimizer_AddKill( self, kills, node->first );
			DaoOptimizer_AddKill( self, kills, node->second );
			/* node->lvalue must already be in kills; */
			break;
		}
		node->kills->size = 0;
		for(it=DMap_First(kills); it; it=DMap_Next(kills,it)){
			DArray_Append( node->kills, it->key.pVoid );
		}
		DaoAEA_Sort( node->kills->items.pCnode, 0, node->kills->size - 1, codes2 );
	}
}
static void DaoOptimizer_InitNodesRDA( DaoOptimizer *self )
{
	DaoCnode *node, **nodes = self->nodes->items.pCnode;
	daoint M = self->routine->body->regCount;
	daoint i, j, N = self->nodes->size;
	self->enodes->size = 0;
	for(i=0; i<N; i++){
		node = nodes[i];
		if( node->lvalue == 0xffff ) continue;
		DArray_Append( self->uses->items.pArray[node->lvalue], (node->index + RDA_OFFSET) );
	}
}

static void DaoRoutine_UpdateCodes( DaoRoutine *self )
{
	DArray *annotCodes = self->body->annotCodes;
	DVector *vmCodes = self->body->vmCodes;
	DaoVmCodeX *vmc, **vmcs = annotCodes->items.pVmc;
	int i, C, K, N = annotCodes->size;
	int *ids;

	if( vmCodes->size < N ) DVector_Resize( vmCodes, N );
	ids = (int*)vmCodes->data.codes; /* as temporary buffer; */
	for(i=0,K=0; i<N; i++){
		ids[i] = K;
		K += vmcs[i]->code < DVM_UNUSED;
	}
	for(i=0,K=0; i<N; i++){
		vmc = vmcs[i];
		if( vmc->code >= DVM_UNUSED ) continue;
		switch( vmc->code ){
		case DVM_GOTO : case DVM_CASE : case DVM_SWITCH :
		case DVM_TEST : case DVM_TEST_I : case DVM_TEST_F : case DVM_TEST_D :
			vmc->b = ids[ vmc->b ];
			break;
		default : break;
		}
		*vmcs[K++] = *vmc;
	}
	DArray_Erase( annotCodes, K, -1 );
	vmcs = annotCodes->items.pVmc;
	vmCodes->size = K;
	N = 0;
	for(i=0; i<K; i++){
		vmc = vmcs[i];
		vmCodes->data.codes[i] = *(DaoVmCode*)vmc;
		C = vmc->code;
		if( C == DVM_GOTO || C == DVM_TEST || (C >= DVM_TEST_I && C <= DVM_TEST_D) ){
			if( vmc->b == (i+1) ){
				vmc->code = DVM_UNUSED;
				N = 1;
			}
		}
	}
	if( N ) DaoRoutine_UpdateCodes( self );
	if( annotCodes->size < 0.8 * annotCodes->bufsize ){
		DArray_Resize( annotCodes, annotCodes->size, NULL );
		DVector_Resize( vmCodes, vmCodes->size );
	}
}
static void DaoRoutine_UpdateRegister( DaoRoutine *self, DArray *mapping )
{
	DNode *it;
	DArray *array = DArray_New(D_VALUE);
	DMap *localVarType2 = DMap_New(0,0);
	DMap *localVarType = self->body->localVarType;
	DaoType **types = self->body->regType->items.pType;
	DaoVmCode check, *vmc, *codes = self->body->vmCodes->data.codes;
	DaoVmCode **codes2 = (DaoVmCode**) self->body->annotCodes->items.pVmc;
	daoint i, N = self->body->annotCodes->size;
	daoint k, m = 0, M = self->body->regCount;
	daoint *regmap = mapping->items.pInt;

	for(i=0; i<M; i++){
		k = regmap[i];
		if( (k + 1) >= m && k < M ) m = k + 1;
	}
	DArray_Resize( array, m, 0 );
	for(i=0; i<M; i++){
		k = regmap[i];
		if( k >= m ) continue;
		GC_ShiftRC( types[i], array->items.pType[k] );
		array->items.pType[k] = types[i];
		if( (it = MAP_Find( localVarType, i )) ) MAP_Insert( localVarType2, k, it->value.pVoid );
	}
	self->body->regCount = array->size;
	self->body->localVarType = localVarType2;
	DArray_Assign( self->body->regType, array );
	DArray_Delete( array );
	DMap_Delete( localVarType );

	DaoRoutine_SetupSimpleVars( self );

	for(i=0; i<N; i++){
		vmc = codes2[i];
		check = DaoVmCode_CheckOperands( vmc );
		if( check.a ) vmc->a = regmap[ vmc->a ];
		if( check.b ) vmc->b = regmap[ vmc->b ];
		if( check.c ) vmc->c = regmap[ vmc->c ];
		codes[i] = *vmc;
	}
}
static void DaoOptimizer_UpdateRegister( DaoOptimizer *self, DaoRoutine *routine, DArray *mapping )
{
	DNode *it;
	DaoVmCode *vmc, **codes = (DaoVmCode**) routine->body->annotCodes->items.pVmc;
	DaoCnode *node, **nodes = self->nodes->items.pCnode;
	daoint i, N = routine->body->annotCodes->size;
	daoint k, m = 0, M = routine->body->regCount;
	daoint *regmap = (daoint*)dao_calloc( M, sizeof(daoint) );

	/* Assuming nonmonotonic mapping: */
	for(i=0; i<M; i++){
		k = mapping->items.pInt[i];
		if( k >= 0 && k < M && regmap[k] == 0 ) regmap[k] = ++ m;
	}
	for(i=0; i<M; i++){
		k = mapping->items.pInt[i];
		if( k < 0 || k >= M ) continue;
		k = regmap[k] - 1;
		mapping->items.pInt[i] = k;
	}
	dao_free( regmap );
	regmap = mapping->items.pInt;

	DaoRoutine_UpdateRegister( routine, mapping );

	for(i=0; i<N; i++){
		vmc = codes[i];
		node = nodes[i];
		switch( (k = DaoVmCode_GetOpcodeType( vmc )) ){
		case DAO_CODE_NOP :
			break;
		case DAO_CODE_GETC :
		case DAO_CODE_GETG :
			node->lvalue = vmc->c;
			break;
		case DAO_CODE_SETU :
			node->first = vmc->a;
			if( vmc->c != 0 ){
				node->second = vmc->b;
				node->lvalue2 = vmc->b;
			}
			break;
		case DAO_CODE_SETG :
		case DAO_CODE_BRANCH :
			node->first = vmc->a;
			break;
		case DAO_CODE_EXPLIST :
			if( vmc->b == 0 ) break;
			node->second += vmc->a - node->first;
			node->first = vmc->a;
			break;
		case DAO_CODE_GETF : case DAO_CODE_SETF :
		case DAO_CODE_MOVE : case DAO_CODE_UNARY :
			node->first = vmc->a;
			if( k == DAO_CODE_SETF ){
				node->second = vmc->c;
				node->lvalue2 = vmc->c;
			}else{
				node->lvalue = vmc->c;
			}
			break;
		case DAO_CODE_GETM :
		case DAO_CODE_ENUM2 : case DAO_CODE_MATRIX :
		case DAO_CODE_ROUTINE : case DAO_CODE_CALL :
			node->second += vmc->a - node->first;
			node->first = vmc->a;
			node->lvalue = vmc->c;
			break;
		case DAO_CODE_SETM:
			node->second += vmc->c - node->first;
			node->first = vmc->c;
			node->third = vmc->a;
			node->lvalue2 = vmc->c;
			break;
		case DAO_CODE_ENUM :
		case DAO_CODE_YIELD :
			if( vmc->b || vmc->a < M ){
				node->second += vmc->a - node->first;
				node->first = vmc->a;
			}
			node->lvalue = vmc->c;
			break;
		case DAO_CODE_SETI :
		case DAO_CODE_GETI :
		case DAO_CODE_BINARY :
			node->first = vmc->a;
			node->second = vmc->b;
			if( k == DAO_CODE_SETI ){
				node->third = vmc->c;
				node->lvalue2 = vmc->c;
			}else{
				node->lvalue = vmc->c;
			}
			break;
		case DAO_CODE_GETU :
			node->lvalue = vmc->c;
			if( vmc->a != 0 ) node->second = vmc->b;
			break;
		case DAO_CODE_UNARY2 :
			node->second = vmc->b;
			node->lvalue = vmc->c;
			break;
		default: break;
		}
	}
	self->enodes->size = 0;
	DMap_Reset( self->exprs );
	for(i=0; i<N; i++){
		vmc = codes[i];
		node = nodes[i];
		if( node->exprid == 0xffff ) continue;
		it = MAP_Find( self->exprs, vmc );
		if( it == NULL ){
			it = MAP_Insert( self->exprs, vmc, self->enodes->size );
			DArray_Append( self->enodes, node );
		}
		node->exprid = it->value.pInt;
	}
}
static int DaoCnode_CountDefinitions( DaoCnode *self, int reg )
{
	int i, K = 0, N = self->defs->size;
	for(i=0; i<N; ++i){
		DaoCnode *node = self->defs->items.pCnode[i];
		K += node->lvalue == reg;
	}
	return K;
}
/* Common Subexpression Elimination: */
static void DaoOptimizer_CSE( DaoOptimizer *self, DaoRoutine *routine )
{
	DNode *it;
	DArray *fixed = DArray_New(0);
	DArray *inodes = DArray_New(0);
	DArray *avexprs = DArray_New(0);
	DArray *types = routine->body->regType;
	DArray *annotCodes = routine->body->annotCodes;
	DaoVmCodeX *vmc, *vmc2, **codes = annotCodes->items.pVmc;
	DaoCnode *node, *node2, **nodes;
	daoint i, j, k, m, N = annotCodes->size;
	daoint M = routine->body->regCount;
	int reg, tid, fixed1, fixed2, sametype;

	DaoOptimizer_LinkDU( self, routine );
	DaoOptimizer_InitAEA( self, routine );
	DaoOptimizer_SolveFlowEquation( self );
	DaoRoutine_CodesToInodes( routine, inodes );
	nodes = self->nodes->items.pCnode;

	/* DaoOptimizer_Print( self ); */
	DArray_Resize( fixed, M, 0 );
	for(i=0; i<M; ++i) fixed->items.pInt[i] = 0;
	for(i=0; i<N; ++i){
		node = nodes[i];
		if( node->type != DAO_OP_RANGE && node->type != DAO_OP_RANGE2 ) continue;
		for(j=node->first; j<=node->second; ++j) fixed->items.pInt[j] = 1;
	}

	for(i=0; i<N; ++i){
		DaoInode *vmc2, *vmc = inodes->items.pInode[i];
		DaoVmCode check;
		node = nodes[i];
		if( node->lvalue == 0xffff || node->exprid == 0xffff ) continue;
		if( DaoVmCode_GetOpcodeType( (DaoVmCode*) vmc ) == DAO_CODE_MOVE ) continue;
		if( vmc->code <= DVM_DEBUG ) continue;

		sametype = 1;
		avexprs->size = 0;
		for(j=0; j<node->list->size; ++j){
			node2 = node->list->items.pCnode[j];
			vmc2 = inodes->items.pInode[node2->index];
			if( vmc->code != vmc2->code || vmc->a != vmc2->a || vmc->b != vmc2->b ) continue;
			if( types->items.pType[vmc->c] != types->items.pType[node2->lvalue] ) sametype = 0;
			DArray_Append( avexprs, node2 );
		}
		if( avexprs->size == 0 ) continue;

		node2 = avexprs->items.pCnode[0];
		fixed1 = fixed->items.pInt[vmc->c];
		fixed2 = fixed->items.pInt[node2->lvalue];
		if( avexprs->size == 1 && sametype && fixed1 == 0 && fixed2 == 0 ){
			/*
			// Check for each use of node->lvalue, if they have single definition
			// which is node->lvalue, one can simply update the operands of the
			// use instructions:
			 */
			for(j=0; j<node->uses->size; ++j){
				DaoCnode *use = node->uses->items.pCnode[j];
				if( DaoCnode_CountDefinitions( use, node->lvalue ) > 1 ) break;
			}
			if( j == node->uses->size ){
				for(j=0; j<node->uses->size; ++j){
					DaoCnode *use = node->uses->items.pCnode[j];
					DaoInode *inode = inodes->items.pInode[use->index];
					check = DaoVmCode_CheckOperands( (DaoVmCode*) inode );
					if( check.a && inode->a == vmc->c ) inode->a = node2->lvalue;
					if( check.b && inode->b == vmc->c ) inode->b = node2->lvalue;
					if( check.c && inode->c == vmc->c ) inode->c = node2->lvalue;
				}
				vmc->code = DVM_UNUSED;
				node->type = DAO_OP_NONE;
				node->lvalue = 0xffff;
				node->lvalue2 = 0xffff;
				node->exprid = 0xffff;
				continue;
			}
		}

		reg = types->size;
		tid = types->items.pType[vmc->c]->tid;
		DArray_Append( fixed, IntToPointer(0) );
		DArray_Append( types, types->items.pVoid[vmc->c] );
		for(j=0; j<avexprs->size; ++j){
			DaoCnode *cnode = avexprs->items.pCnode[j];
			DaoInode *prev = inodes->items.pInode[cnode->index];
			DaoInode *next = prev->next;
			DaoInode *inode;

			inode = DaoInode_New();
			inode->level = prev->level;
			inode->line = prev->line;
			inode->first = prev->first;
			inode->middle = prev->middle;
			inode->last = prev->last;
			inode->code = DVM_MOVE;

			switch( types->items.pType[prev->c]->tid == tid ? tid : 0 ){
			case DAO_INTEGER : inode->code = DVM_MOVE_II; break;
			case DAO_FLOAT   : inode->code = DVM_MOVE_FF; break;
			case DAO_DOUBLE  : inode->code = DVM_MOVE_DD; break;
			case DAO_COMPLEX : inode->code = DVM_MOVE_CC; break;
			case DAO_STRING  : inode->code = DVM_MOVE_SS; break;
			}

			inode->c = prev->c;
			prev->c = inode->a = reg;

			prev->next = inode;
			inode->prev = next;
			inode->next = next;
			next->prev = inode;
		}
		vmc->code = DVM_MOVE_XX;
		vmc->a = reg;
		vmc->b = 0;
		node->type = DAO_OP_SINGLE;
		node->lvalue2 = 0xffff;
		switch( types->items.pType[vmc->c]->tid ){
		case DAO_INTEGER : vmc->code = DVM_MOVE_II; break;
		case DAO_FLOAT   : vmc->code = DVM_MOVE_FF; break;
		case DAO_DOUBLE  : vmc->code = DVM_MOVE_DD; break;
		case DAO_COMPLEX : vmc->code = DVM_MOVE_CC; break;
		case DAO_STRING  : vmc->code = DVM_MOVE_SS; break;
		}
	}
	routine->body->regCount = routine->body->regType->size;
	DaoRoutine_SetupSimpleVars( routine );
	DaoRoutine_CodesFromInodes( routine, inodes );
	DaoInodes_Clear( inodes );

	DArray_Delete( inodes );
	DArray_Delete( fixed );
	DArray_Delete( avexprs );
}
/* Dead Code Elimination: */
static void DaoOptimizer_DCE( DaoOptimizer *self, DaoRoutine *routine )
{
	DArray *inodes = DArray_New(0);
	DaoCnode *node, *node2, **nodes;
	daoint N = routine->body->annotCodes->size;
	daoint i, j, k, m;

	DaoOptimizer_LinkDU( self, routine );
	DaoOptimizer_DoLVA( self, routine );
	DaoRoutine_CodesToInodes( routine, inodes );

	nodes = self->nodes->items.pCnode;
	for(i=N-1; i>=0; --i){
		int used = 0;
		DaoInode *vmc = inodes->items.pInode[i];
		node = nodes[i];
		if( node->lvalue == 0xffff ) continue;
		if( node->exprid == 0xffff ) continue; /* this instruction may have side effect; */
		if( DaoRoutine_IsVolatileParameter( self->routine, node->lvalue ) ) continue;
		if( DaoCnode_FindResult( node, IntToPointer(node->lvalue) ) >= 0 ){
			/* Check if the instructions that use node->lvalue are marked as dead: */
			for(j=0; j<node->uses->size; ++j){
				node2 = node->uses->items.pCnode[j];
				if( inodes->items.pInode[node2->index]->code != DVM_UNUSED ){
					used = 1;
					break;
				}
			}
		}
		if( used ) continue;
		inodes->items.pInode[node->index]->code = DVM_UNUSED;
	}
	DaoRoutine_CodesFromInodes( routine, inodes );

	DaoInodes_Clear( inodes );
	DArray_Delete( inodes );
}
/* Simple remapping the used registers to remove the unused ones: */
static void DaoOptimizer_RemapRegister( DaoOptimizer *self, DaoRoutine *routine )
{
	DaoCnode *node, **nodes;
	DArray *array = self->array;
	daoint i, N = routine->body->annotCodes->size;
	daoint j, M = routine->body->regCount;
	daoint *regmap;

	DArray_Resize( array, M, 0 );
	regmap = array->items.pInt;
	for(i=0; i<M; i++) array->items.pInt[i] = M; /* mark all register unused; */

	/* Dead Code Elimination may have produce some dead registers. Remove them first: */
	self->update = NULL;
	DaoOptimizer_Init( self, routine );
	nodes = self->nodes->items.pCnode;
	for(i=0; i<N; i++){
		node = nodes[i];
		switch( node->type ){
		case DAO_OP_SINGLE :
			regmap[ node->first ] = node->first;
			break;
		case DAO_OP_PAIR :
			regmap[ node->first ] = node->first;
			regmap[ node->second ] = node->second;
			break;
		case DAO_OP_TRIPLE :
			regmap[ node->first ] = node->first;
			regmap[ node->second ] = node->second;
			regmap[ node->third ] = node->third;
			break;
		case DAO_OP_RANGE :
		case DAO_OP_RANGE2 :
			for(j=node->first; j<=node->second; j++) regmap[j] = j;
			if( node->type == DAO_OP_RANGE2 ) regmap[ node->third ] = node->third;
			break;
		}
		if( node->lvalue != 0xffff ) regmap[ node->lvalue ] = node->lvalue;
	}
	for(i=0; i<routine->parCount; i++) regmap[i] = i;
	DaoOptimizer_UpdateRegister( self, routine, array );
}
static void DaoOptimizer_RemoveUnreachableCodes( DaoOptimizer *self, DaoRoutine *routine )
{
	DArray *array = self->array;
	DArray *annotCodes = routine->body->annotCodes;
	DaoVmCodeX **codes = annotCodes->items.pVmc;
	DaoCnode *node, *node2, **nodes;
	daoint i, j, m, N = annotCodes->size;

	if( N == 0 ) return;
	self->update = NULL;
	DaoOptimizer_Init( self, routine );
	nodes = self->nodes->items.pCnode;
	for(i=0; i<N; i++) nodes[i]->reachable = 0;
	array->size = 0;
	DArray_Append( array, nodes[0] );
	for(i=0; i<array->size; i++){
		node = array->items.pCnode[i];
		node->reachable = 1;
		for(j=0,m=node->outs->size; j<m; j++){
			node2 = node->outs->items.pCnode[j];
			if( node2->reachable == 0 ) DArray_Append( array, node2 );
		}
	}
	for(i=0; i<N; i++){
		if( nodes[i]->reachable == 0 ) codes[i]->code = DVM_UNUSED;
	}
	DaoRoutine_UpdateCodes( routine );
	DaoOptimizer_RemapRegister( self, routine );
}
/* Remove redundant registers for the same data types: */
static void DaoOptimizer_ReduceRegister( DaoOptimizer *self, DaoRoutine *routine )
{
	DNode *it, *it2, *it3, *it4;
	DMap *same, *one = DHash_New(0,0);
	DMap *sets = DHash_New(0,D_MAP);
	DMap *livemap = DMap_New(0,0);
	DMap *actives = DMap_New(0,0);
	DMap *offsets = DMap_New(0,0);
	DArray *array = DArray_New(0);
	DArray *annotCodes = routine->body->annotCodes;
	DaoVmCodeX *vmc, **codes = annotCodes->items.pVmc;
	DaoCnode *node, *node2, **nodes;
	DaoType *type, *type2, **types;
	daoint i, j, k, m, N = annotCodes->size;
	daoint regCount, M = routine->body->regCount;
	daoint *intervals, *regmap;

	/* Dead Code Elimination may have produce some dead registers. Remove them first: */
	DaoOptimizer_RemapRegister( self, routine );
	regmap = self->array->items.pInt;

	/* DaoRoutine_PrintCode( routine, routine->nameSpace->vmSpace->errorStream ); */

	/* Live Variable Analysis for register reallocation: */
	DaoOptimizer_DoLVA( self, routine );

	/* DaoOptimizer_Print( self ); */

	/* Now use the linear scan algorithm (Poletto and Sarkar) to reallocate registers: */
	DArray_Resize( array, 2*M, 0 );
	intervals = array->items.pInt;
	regCount = routine->parCount;
	M = routine->body->regCount;
	types = routine->body->regType->items.pType;
	for(i=0; i<routine->parCount; i++){
		regmap[i] = i;
		intervals[2*i] = 0; /* initialize the live interval of the register; */
		intervals[2*i+1] = N-1;
	}
	for(i=routine->parCount; i<M; i++){
		type = types[i];
		regmap[i] = -1; /* mark the register not yet mapped; */
		intervals[2*i] = -1; /* initialize the live interval of the register; */
		intervals[2*i+1] = -1;
		/* map untyped register to distinctive new register ids: */
		if( type == NULL || type->tid == DAO_ANY || type->tid == DAO_VARIANT
				|| (type->attrib & (DAO_TYPE_SPEC|DAO_TYPE_UNDEF)) )
			regmap[i] = regCount ++;
	}
	nodes = self->nodes->items.pCnode;
	for(i=0; i<N; i++){
		node = nodes[i];
		vmc = codes[i];
		for(j=0,k=0; j<node->list->size; ++j){
			int id = node->list->items.pInt[j];
			if( intervals[2*id] < 0 ) intervals[2*id] = i;
			intervals[2*id+1] = i + 1; /* plus one because it is alive at the exit; */
			k += 1;
		}
		if( vmc->code == DVM_LOAD || vmc->code == DVM_CAST ){
			/* These opcodes may create a reference (alias) of ::a at the result register (::c),
			// the liveness interval of ::a must be expanded to the point where ::c is used: */
			for(j=i+1; j<N; ++j){
				node2 = nodes[j];
				k = 0;
				switch( node2->type ){
				case DAO_OP_TRIPLE : k = node2->third == vmc->c;   /* fall through; */
				case DAO_OP_PAIR   : k |= node2->second == vmc->c; /* fall through; */
				case DAO_OP_SINGLE : k |= node2->first == vmc->c; break;
				case DAO_OP_RANGE :
				case DAO_OP_RANGE2 : k = node2->first <= vmc->c && vmc->c <= node2->second; break;
				}
				if( k ) break;
			}
			if( j < N && j > intervals[2*vmc->a+1] ) intervals[2*vmc->a+1] = j;
		}
		if( node->lvalue != 0xffff && intervals[2*node->lvalue] < 0 ){
			/* For some DVM_CALL, the lvalue is not alive, but it is needed: */
			intervals[2*node->lvalue] = i;
			intervals[2*node->lvalue+1] = i;
		}
		if( node->lvalue != 0xffff && regmap[ node->lvalue ] < 0 ){
			type = types[ node->lvalue ];
			/* No reduction for register that may hold reference to simple types: */
			if( type && type->tid && (type->tid < DAO_ARRAY || type->tid == DAO_VARIANT) ){
				if( DaoVmCode_MayCreateReference( vmc->code ) ) regmap[node->lvalue] = regCount ++;
			}
		}
		if( node->type != DAO_OP_RANGE && node->type != DAO_OP_RANGE2 ) continue;
		/* map this range of registers to a new range of registers: */
		for(j=node->first; j<=node->second; j++){
			if( regmap[j] >= 0 ) continue;
			regmap[j] = regCount ++;
		}
	}
	for(i=0; i<M; i++){
		type = types[i];
		/* printf( "%3i: %3i %3i\n", i, intervals[2*i], intervals[2*i+1] ); */
		if( regmap[i] >= 0 ) continue;
		/* count the number of registers with the same types: */
		if( (it = MAP_Find( sets, type )) == NULL ) it = MAP_Insert( sets, type, one );
		MAP_Insert( it->value.pMap, it->value.pMap->size, 0 );
	}
	/* The registers of the same type will be mapped to the same group of registers.
	// Now calculate the register index offset for each group: */
	for(it=DMap_First(sets); it; it=DMap_Next(sets,it)){
		MAP_Insert( offsets, it->key.pVoid, regCount );
		regCount += it->value.pMap->size;
	}
	assert( regCount == M );

	/* Sort all the live intervals by the starting point and the register index: */
	for(i=0; i<M; i++){
		size_t key = (intervals[2*i] << 16) | i;
		MAP_Insert( livemap, key, i );
	}
	/* Iterate through the liveness starting points: */
	for(it=DMap_First(livemap); it; it=DMap_Next(livemap,it)){
		daoint start = it->key.pInt >> 16;
		i = it->value.pInt;
		if( regmap[i] >= 0 ) continue;
		/* Remove intervals ended before this point: */
		while( (it2 = DMap_First(actives)) && (it2->key.pInt >> 16) < start ){
			type = types[ it2->key.pInt & 0xffff ];
			it3 = MAP_Find( sets, type );
			MAP_Insert( it3->value.pMap, it2->value.pVoid, 0 ); /* Release the register; */
			DMap_EraseNode( actives, it2 );
		}
		type = types[i];
		it2 = MAP_Find( offsets, type );
		it3 = MAP_Find( sets, type );
		it4 = DMap_First( it3->value.pMap );
		regmap[i] = it2->value.pInt + it4->key.pInt;
		/* add new active interval; */
		MAP_Insert( actives, (intervals[2*i+1]<<16) | i, it4->key.pVoid );
		DMap_EraseNode( it3->value.pMap, it4 );
	}
	DaoOptimizer_UpdateRegister( self, routine, self->array );
	DArray_Delete( array );
	DMap_Delete( livemap );
	DMap_Delete( offsets );
	DMap_Delete( actives );
	DMap_Delete( sets );
	DMap_Delete( one );
	for(i=0; i<N; i++){
		vmc = codes[i];
		if( vmc->code >= DVM_MOVE_II && vmc->code <= DVM_MOVE_XX && vmc->a == vmc->c )
			vmc->code = DVM_UNUSED;
	}
	DaoRoutine_UpdateCodes( routine );
}

static void DaoOptimizer_Optimize( DaoOptimizer *self, DaoRoutine *routine )
{
	DaoType *type, **types = routine->body->regType->items.pType;
	DaoVmSpace *vms = routine->nameSpace->vmSpace;
	daoint i, k, notide = ! (vms->options & DAO_OPTION_IDE);

	if( daoConfig.optimize == 0 ) return;

	/* Do not perform optimization if it may take too much memory: */
	if( (routine->body->vmCodes->size * routine->body->regCount) > 1000000 ) return;

	if( routine->body->simpleVariables->size < routine->body->regCount / 2 ) return;
	for(i=0,k=0; i<routine->body->simpleVariables->size; i++){
		type = types[ routine->body->simpleVariables->items.pInt[i] ];
		k += type ? type->tid >= DAO_INTEGER && type->tid <= DAO_LONG : 0;
	}
	/* Optimize only if there are sufficient amount of numeric calculations: */
	if( k < routine->body->regCount / 2 ) return;

	DaoOptimizer_CSE( self, routine );
	DaoOptimizer_DCE( self, routine );
	DaoOptimizer_ReduceRegister( self, routine );

	/* DaoOptimizer_LinkDU( self, routine ); */

	if( notide && daoConfig.jit && dao_jit.Compile ){
		/* LLVMContext provides no locking guarantees: */
		DMutex_Lock( & mutex_routine_specialize );
		dao_jit.Compile( routine, self );
		DMutex_Unlock( & mutex_routine_specialize );
	}
}


void DaoOptimizer_InitNode( DaoOptimizer *self, DaoCnode *node, DaoVmCode *vmc )
{
	DNode *it;
	DArray **uses = self->uses->items.pArray;
	DaoRoutine *routine = self->routine;
	DaoType **types = routine->body->regType->items.pType;
	DMap *localVarType = routine->body->localVarType;
	uchar_t type = DaoVmCode_GetOpcodeType( vmc );
	int i, k, m, at, bt, ct, code = vmc->code;

	DaoCnode_InitOperands( node, vmc );

	/* Exclude expression that does not yield new value: */
	if( node->lvalue == 0xffff ) return;
	if( self->update == DaoOptimizer_UpdateRDA ) return;
	switch( type ){
	case DAO_CODE_GETG :
	case DAO_CODE_GETU :
	case DAO_CODE_ROUTINE :
	case DAO_CODE_CALL :
	case DAO_CODE_YIELD :
		/* Exclude expressions that access global data or must be evaluated: */
		return;
	case DAO_CODE_MOVE :
		if( vmc->code == DVM_LOAD ) return;
		if( DaoRoutine_IsVolatileParameter( routine, vmc->c ) ) return;
		/*
		// Exclude MOVE if the destination is a potential reference (list.back()=1).
		// Note:
		// routine->body->localVarType still contains registers of explicitly declared
		// variables even after optimizations, because registers from instructions that
		// may create references are not merged or reused for register reduction.
		// See also DaoVmCode_MayCreateReference().
		*/
		if( self->refers->items.pInt[vmc->c] ) return;
		break;
	case DAO_CODE_UNARY2 :
		/* Exclude expressions that may have side effects: */
		if( code == DVM_MATH && vmc->a == DVM_MATH_RAND ) return;
		if( (code >= DVM_MATH_I && code <= DVM_MATH_D) && vmc->a == DVM_MATH_RAND ) return;
		if( DaoRoutine_IsVolatileParameter( routine, vmc->b ) ) return;
		break;
	case DAO_CODE_GETF :
	case DAO_CODE_UNARY :
		/* Exclude expressions that may have side effects by operator overloading: */
		at = types[vmc->a] ? types[vmc->a]->tid : DAO_UDT;
		ct = types[vmc->c] ? types[vmc->c]->tid : DAO_UDT;
		if( (at & DAO_ANY) || at == DAO_VARIANT ) return;
		if( (ct & DAO_ANY) || ct == DAO_VARIANT ) return;
		if( at >= DAO_OBJECT && at <= DAO_CTYPE ) return;
		if( ct >= DAO_OBJECT && ct <= DAO_CTYPE ) return;
		if( DaoRoutine_IsVolatileParameter( routine, vmc->a ) ) return;
		break;
	case DAO_CODE_BINARY :
		/* Exclude binary operations if the destination is a potential reference (list.back()+=1): */
		if( vmc->a == vmc->c || vmc->b == vmc->c )
			if( MAP_Find( localVarType, vmc->c ) == NULL ) return;
		/* Fall through for further checking; */
	case DAO_CODE_GETI :
		/* Exclude expressions that may have side effects by operator overloading: */
		at = types[vmc->a] ? types[vmc->a]->tid : DAO_UDT;
		bt = types[vmc->b] ? types[vmc->b]->tid : DAO_UDT;
		ct = types[vmc->c] ? types[vmc->c]->tid : DAO_UDT;
		/* Need to be executed in any case to finish a loop: */
		if( types[vmc->b] == dao_type_for_iterator ) return;
		if( (at & DAO_ANY) || at == DAO_VARIANT ) return;
		if( (bt & DAO_ANY) || bt == DAO_VARIANT ) return;
		if( (ct & DAO_ANY) || ct == DAO_VARIANT ) return;
		if( at >= DAO_OBJECT && at <= DAO_CTYPE ) return;
		if( bt >= DAO_OBJECT && bt <= DAO_CTYPE ) return;
		if( ct >= DAO_OBJECT && ct <= DAO_CTYPE ) return;
		if( DaoRoutine_IsVolatileParameter( routine, vmc->a ) ) return;
		if( DaoRoutine_IsVolatileParameter( routine, vmc->b ) ) return;
		break;
	}

	it = MAP_Find( self->exprs, vmc );
	if( it == NULL ){
		it = MAP_Insert( self->exprs, vmc, self->enodes->size );
		DArray_Append( self->enodes, node );
	}
	node->exprid = it->value.pInt;

	if( node->lvalue != 0xffff ) DArray_Append( uses[node->lvalue], node );
	if( node->lvalue2 != 0xffff ) DArray_Append( uses[node->lvalue2], node );
	switch( node->type ){
	case DAO_OP_SINGLE :
		DArray_Append( uses[node->first], node );
		break;
	case DAO_OP_PAIR   :
		DArray_Append( uses[node->first], node );
		DArray_Append( uses[node->second], node );
		break;
	case DAO_OP_RANGE :
		for(i=node->first; i<=node->second; i++) DArray_Append( uses[i], node );
		break;
	}
}



DaoInode* DaoInode_New()
{
	DaoInode *self = (DaoInode*) dao_calloc( 1, sizeof(DaoInode) );
	return self;
}
void DaoInode_Delete( DaoInode *self )
{
	free( self );
}
void DaoInode_Print( DaoInode *self, int index )
{
	const char *name = DaoVmCode_GetOpcodeName( self->code );
	static const char *fmt = "%3i: %-8s : %5i, %5i, %5i;  [%3i] [%2i] %9p %9p %9p, %s\n";
	if( index < 0 ) index = self->index;
	printf( fmt, index, name, self->a, self->b, self->c, self->line, self->level,
			self, self->jumpTrue, self->jumpFalse, "" );
}



void DaoInodes_Clear( DArray *inodes )
{
	DaoInode *tmp, *inode = (DaoInode*) DArray_Front( inodes );
	while( inode && inode->prev ) inode = inode->prev;
	while( inode ){
		tmp = inode;
		inode = inode->next;
		DaoInode_Delete( tmp );
	}
	DArray_Clear( inodes );
}

void DaoRoutine_CodesToInodes( DaoRoutine *self, DArray *inodes )
{
	DaoInode *inode, *inode2;
	DaoVmCodeX *vmc, **vmcs = self->body->annotCodes->items.pVmc;
	daoint i, n, N = self->body->annotCodes->size;

	for(i=0; i<N; i++){
		inode2 = (DaoInode*) DArray_Back( inodes );
		inode = DaoInode_New();
		vmc = vmcs[i];
		if( vmc->code == DVM_GETMI && vmc->b == 1 ){
			vmc->code = DVM_GETI;
			vmc->b = vmc->a + 1;
		}else if( vmc->code == DVM_SETMI && vmc->b == 1 ){
			vmc->code = DVM_SETI;
			vmc->b = vmc->c + 1;
		}
		*(DaoVmCodeX*)inode = *vmc;
		inode->index = i;
		if( inode2 ){
			inode2->next = inode;
			inode->prev = inode2;
		}
		DArray_PushBack( inodes, inode );
	}
	for(i=0; i<N; i++){
		vmc = vmcs[i];
		inode = inodes->items.pInode[i];
		switch( vmc->code ){
		case DVM_GOTO : case DVM_CASE : case DVM_SWITCH :
		case DVM_TEST : case DVM_TEST_I : case DVM_TEST_F : case DVM_TEST_D :
			inode->jumpFalse = inodes->items.pInode[vmc->b];
			break;
		default : break;
		}
	}
}
void DaoRoutine_CodesFromInodes( DaoRoutine *self, DArray *inodes )
{
	int i, n, count = 0;
	DaoRoutineBody *body = self->body;
	DaoInode *it, *first = (DaoInode*) DArray_Front( inodes );
	while( first->prev ) first = first->prev;
	for(it=first; it; it=it->next){
		it->index = count;
		count += it->code != DVM_UNUSED;
		while( it->jumpFalse && it->jumpFalse->extra ) it->jumpFalse = it->jumpFalse->extra;
	}
	DVector_Clear( body->vmCodes );
	DArray_Clear( body->annotCodes );
	for(it=first,count=0; it; it=it->next){
		/* DaoInode_Print( it ); */
		switch( it->code ){
		case DVM_GOTO : case DVM_CASE : case DVM_SWITCH :
		case DVM_TEST : case DVM_TEST_I : case DVM_TEST_F : case DVM_TEST_D :
			it->b = it->jumpFalse->index;
			break;
		default : break;
		}
		if( it->code >= DVM_UNUSED ) continue;
		DVector_PushCode( body->vmCodes, *(DaoVmCode*) it );
		DArray_PushBack( body->annotCodes, (DaoVmCodeX*) it );
	}
}
void DaoRoutine_SetupSimpleVars( DaoRoutine *self )
{
	DMap *refers = DMap_New(0,0);
	DaoRoutineBody *body = self->body;
	DaoVmCodeX **vmcs = body->annotCodes->items.pVmc;
	int i, n;

	self->attribs &= ~DAO_ROUT_REUSABLE;
	for(i=0,n=body->annotCodes->size; i<n; ++i){
		if( DaoVmCode_MayCreateReference( vmcs[i]->code ) ){
			DMap_Insert( refers, IntToPointer( vmcs[i]->code ), 0 );
		}
	}

	DArray_Clear( body->simpleVariables );
	for(i=self->parCount,n=body->regType->size; i<n; ++i){
		DaoType *tp = body->regType->items.pType[i];
		if( tp && tp->tid <= DAO_ENUM ){
			DArray_Append( body->simpleVariables, (daoint)i );
			if( DMap_Find( refers, IntToPointer(i) ) != NULL ){
				self->attribs |= DAO_ROUT_REUSABLE;
			}
		}
	}
	DMap_Delete( refers );
}



DaoInferencer* DaoInferencer_New()
{
	DaoInferencer *self = (DaoInferencer*) dao_calloc( 1, sizeof(DaoInferencer) );
	self->inodes = DArray_New(0);
	self->consts = DArray_New(D_VALUE);
	self->types = DArray_New(D_VALUE);
	self->inited = DString_New(1);
	self->usedtypes = DArray_New(0);
	self->rettypes = DArray_New(0);
	self->typeMaps = DArray_New(D_MAP);
	self->errors = DArray_New(0);
	self->array = DArray_New(0);
	self->array2 = DArray_New(0);
	self->defs = DHash_New(0,0);
	self->defs2 = DHash_New(0,0);
	self->defs3 = DHash_New(0,0);
	self->mbstring = DString_New(1);
	return self;
}
void DaoInferencer_Reset( DaoInferencer *self )
{
	DaoInodes_Clear( self->inodes );
	DArray_Clear( self->consts );
	DArray_Clear( self->types );
	DArray_Clear( self->typeMaps );
	DString_Reset( self->inited, 0 );
	DMap_Reset( self->defs );
	DMap_Reset( self->defs2 );
	DMap_Reset( self->defs3 );
	self->usedtypes->size = 0;
	self->rettypes->size = 0;
	self->errors->size = 0;
	self->array->size = 0;
	self->array2->size = 0;
	self->error = 0;
	self->annot_first = 0;
	self->annot_last = 0;
	self->tid_target = 0;
	self->type_source = NULL;
	self->type_target = NULL;
}
void DaoInferencer_Delete( DaoInferencer *self )
{
	DaoInferencer_Reset( self );
	DArray_Delete( self->inodes );
	DArray_Delete( self->consts );
	DArray_Delete( self->types );
	DString_Delete( self->inited );
	DArray_Delete( self->usedtypes );
	DArray_Delete( self->rettypes );
	DArray_Delete( self->typeMaps );
	DArray_Delete( self->errors );
	DArray_Delete( self->array );
	DArray_Delete( self->array2 );
	DString_Delete( self->mbstring );
	DMap_Delete( self->defs );
	DMap_Delete( self->defs2 );
	DMap_Delete( self->defs3 );
	dao_free( self );
}
void DaoInferencer_Init( DaoInferencer *self, DaoRoutine *routine, int silent )
{
	DNode *node;
	DMap *defs = self->defs;
	DaoType *type, **types;
	DaoNamespace *NS = routine->nameSpace;
	DArray *partypes = routine->routType->nested;
	daoint i, n, M = routine->body->regCount;
	char *inited;

	DaoInferencer_Reset( self );
	self->silent = silent;
	self->routine = routine;
	self->tidHost = routine->routHost ? routine->routHost->tid : 0;
	self->hostClass = self->tidHost == DAO_OBJECT ? & routine->routHost->aux->xClass:NULL;

	DaoRoutine_CodesToInodes( routine, self->inodes );

	DString_Resize( self->inited, M );
	DArray_Resize( self->consts, M, NULL );
	/*
	// Allocate more memory so that the "types" and "typeVH" variables in
	// DaoInferencer_DoInference() will not be invalidated by inserting instructions.
	*/
	DArray_Resize( self->types, 3*M, NULL );
	self->types->size = M;
	inited = self->inited->mbs;
	types = self->types->items.pType;

	for(i=0; i<routine->parCount; ++i) inited[i] = 1;
	if( (routine->attribs & DAO_ROUT_DECORATOR) && partypes->size ) inited[routine->parCount] = 1;
	for(i=0,n=partypes->size; i<n; i++){
		types[i] = partypes->items.pType[i];
		if( types[i] && types[i]->tid == DAO_PAR_VALIST ){
			DaoType *vltype = (DaoType*) types[i]->aux;
			while( i < DAO_MAX_PARAM ) types[i++] = vltype;
			break;
		}
		type = types[i];
		if( type ) types[i] = & type->aux->xType; /* name:type, name=type */
		node = MAP_Find( routine->body->localVarType, i );
		if( node == NULL ) continue;
		if( node->value.pType == NULL || types[i] == NULL ) continue;
		DaoType_MatchTo( types[i], node->value.pType, defs );
	}
	node = DMap_First( routine->body->localVarType );
	for( ; node !=NULL; node = DMap_Next(routine->body->localVarType,node) ){
		if( node->key.pInt < (int)partypes->size ) continue;
		types[ node->key.pInt ] = DaoType_DefineTypes( node->value.pType, NS, defs );
	}
	for(i=0; i<self->types->size; i++) GC_IncRC( types[i] );
	DArray_PushBack( self->typeMaps, defs );

	self->typeLong = DaoNamespace_MakeType( NS, "long", DAO_LONG, NULL, NULL, 0 );
	self->typeEnum = DaoNamespace_MakeType( NS, "enum", DAO_ENUM, NULL, NULL, 0 );
	self->typeString = DaoNamespace_MakeType( NS, "string", DAO_STRING, NULL, NULL, 0 );
	self->basicTypes[DAO_NONE] = dao_type_none;
	self->basicTypes[DAO_INTEGER] = dao_type_int;
	self->basicTypes[DAO_FLOAT] = dao_type_float;
	self->basicTypes[DAO_DOUBLE] = dao_type_double;
	self->basicTypes[DAO_COMPLEX] = dao_type_complex;
	self->basicTypes[DAO_LONG] = self->typeLong;
	self->basicTypes[DAO_ENUM] = self->typeEnum;
	self->basicTypes[DAO_STRING] = self->typeString;
}


static int DaoRoutine_CheckTypeX( DaoType *routType, DaoNamespace *ns, DaoType *selftype,
		DaoType *ts[], int np, int code, int def, int *parpass, int passdefault )
{
	int ndef = 0;
	int i, j, match = 1;
	int ifrom, ito;
	int npar = np, size = routType->nested->size;
	int selfChecked = 0, selfMatch = 0;
	DaoType  *abtp, **partypes = routType->nested->items.pType;
	DaoType **tps = ts;
	DNode *node;
	DMap *defs;

	/* Check for explicit self parameter: */
	if( np && (ts[0]->attrib & DAO_TYPE_SELFNAMED) ){
		selftype = NULL;
		code = DVM_MCALL;
	}

	defs = DMap_New(0,0);
	if( routType->nested ){
		ndef = routType->nested->size;
		if( ndef ){
			abtp = partypes[ ndef-1 ];
			if( abtp->tid == DAO_PAR_VALIST ) ndef = DAO_MAX_PARAM;
		}
	}

#if 0
	   printf( "=====================================\n" );
	   for( j=0; j<npar; j++){
	   DaoType *tp = tps[j];
	   if( tp != NULL ) printf( "tp[ %i ]: %s\n", j, tp->name->mbs );
	   }
	   printf( "%s %i %i\n", routType->name->mbs, ndef, npar );
	   if( selftype ) printf( "%i\n", routType->name->mbs, ndef, npar, selftype );
#endif

	if( code == DVM_MCALL && ! (routType->attrib & DAO_TYPE_SELF) ){
		npar --;
		tps ++;
	}else if( selftype && (routType->attrib & DAO_TYPE_SELF) && code != DVM_MCALL ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		abtp = & partypes[0]->aux->xType;
		selfMatch = DaoType_MatchTo( selftype, abtp, defs );
		if( selfMatch ){
			selfChecked = 1;
			parpass[0] = selfMatch;
		}
	}
	if( npar == ndef && ndef == 0 ) goto FinishOK;
	if( npar > ndef && (size == 0 || partypes[size-1]->tid != DAO_PAR_VALIST ) ){
		goto FinishError;
	}

	for(j=selfChecked; j<ndef; j++) parpass[j] = 0;
	for(ifrom=0; ifrom<npar; ifrom++){
		DaoType *tp = tps[ifrom];
		ito = ifrom + selfChecked;
		if( ito >= ndef ) goto FinishError;
		if( partypes[ito]->tid == DAO_PAR_VALIST ){
			DaoType *vlt = (DaoType*) partypes[ito]->aux;
			for(; ifrom<npar; ifrom++, ito++){
				parpass[ito] = 1;
				if( vlt && DaoType_MatchTo( tp, vlt, defs ) == 0 ) goto FinishError;
			}
			break;
		}
		if( tp == NULL ) goto FinishError;
		if( tp->tid == DAO_PAR_NAMED ){
			node = DMap_Find( routType->mapNames, tp->fname );
			if( node == NULL ) goto FinishError;
			ito = node->value.pInt;
			tp = & tp->aux->xType;
		}
		if( ito >= ndef || tp ==NULL )  goto FinishError;
		abtp = routType->nested->items.pType[ito];
		if( abtp->tid == DAO_PAR_NAMED || abtp->tid == DAO_PAR_DEFAULT ) abtp = & abtp->aux->xType;
		parpass[ito] = DaoType_MatchTo( tp, abtp, defs );

#if 0
		printf( "%p %s %p %s\n", tp->aux, tp->name->mbs, abtp->aux, abtp->name->mbs );
		printf( "%i:  %i\n", ito, parpass[ito] );
#endif

		/* less strict */
		if( tp && parpass[ito] ==0 ){
			if( tp->tid == DAO_ANY && abtp->tid == DAO_ANY )
				parpass[ito] = DAO_MT_ANY;
			else if( tp->tid == DAO_ANY || tp->tid == DAO_UDT )
				parpass[ito] = DAO_MT_NEGLECT;
		}
		if( parpass[ito] == 0 ) goto FinishError;
		if( def ) tps[ifrom] = DaoType_DefineTypes( tps[ifrom], ns, defs );
	}
	if( passdefault ){
		for(ito=0; ito<ndef; ito++){
			i = partypes[ito]->tid;
			if( i == DAO_PAR_VALIST ) break;
			if( parpass[ito] ) continue;
			if( i != DAO_PAR_DEFAULT ) goto FinishError;
			parpass[ito] = 1;
		}
	}
	match = DAO_MT_EQ;
	for(j=0; j<(npar+selfChecked); j++) if( match > parpass[j] ) match = parpass[j];

#if 0
	printf( "%s %i %i %i\n", routType->name->mbs, match, ndef, npar );
#endif

FinishOK:
	DMap_Delete( defs );
	return match;
FinishError:
	DMap_Delete( defs );
	return 0;
}
static int DaoRoutine_CheckType( DaoType *routType, DaoNamespace *ns, DaoType *selftype,
		DaoType *ts[], int np, int codemode, int def )
{
	int parpass[DAO_MAX_PARAM];
	int code = codemode & 0xffff;
	int mode = codemode >> 16;
	int b1 = ((codemode>>16) & DAO_CALL_BLOCK) != 0;
	int b2 = (routType->attrib & DAO_TYPE_CODESECT) != 0;
	if( b1 != b2 ) return 0;
	return DaoRoutine_CheckTypeX( routType, ns, selftype, ts, np, code, def, parpass, 1 );
}

DaoType* DaoRoutine_PartialCheck( DaoNamespace *NS, DaoType *routype, DArray *routines, DArray *partypes, int call, int *which, int *matched )
{
	DaoType *type, **types;
	DArray *routypes = DArray_New(0);
	int parpass[DAO_MAX_PARAM];
	int npar = partypes->size;
	int j, k, max = 0;

	if( routines ){
		for(j=0; j<routines->size; j++){
			type = routines->items.pRoutine[j]->routType;
			DArray_Append( routypes, type );
		}
	}else{
		DArray_Append( routypes, routype );
	}
	*matched = 0;
	routype = NULL;
	for(j=0; j<routypes->size; j++){
		type = routypes->items.pType[j];
		k = type->nested->size;
		partypes->size = npar;
		while( partypes->size < k ) DArray_Append( partypes, dao_type_any );
		k = DaoRoutine_CheckTypeX( type, NS, NULL, partypes->items.pType, k, call, 0, parpass, 0 );
		*matched += k != 0 && k == max;
		if( k > max ){
			if( routines ) *which = j;
			*matched = 0;
			routype = type;
			max = k;
		}
	}
	DArray_Delete( routypes );
	if( routype == NULL ) return NULL;
	DaoRoutine_CheckTypeX( routype, NS, NULL, partypes->items.pType, npar, call, 0, parpass, 0 );
	partypes->size = 0;
	k = routype->nested->size - (routype->variadic != 0);
	for(j=0; j<k; j++){
		if( parpass[j] ) continue;
		DArray_Append( partypes, routype->nested->items.pType[j] );
	}
	if( routype->variadic ) DArray_Append( partypes, DArray_Back( routype->nested ) );
	k = partypes->size;
	types = partypes->items.pType;
	return DaoNamespace_MakeType( NS, "routine", DAO_ROUTINE, routype->aux, types, k );
}

DaoRoutine* DaoRoutine_ResolveByTypeX( DaoRoutine *self, DaoType *st, DaoType *t[], int n, int codemode );
DaoRoutine* DaoRoutine_ResolveByType( DaoRoutine *self, DaoType *selftype, DaoType *ts[], int np, int codemode )
{
	DaoRoutine *rout = DaoRoutine_ResolveByTypeX( self, selftype, ts, np, codemode );
	if( rout == (DaoRoutine*)self ){ /* parameters not yet checked: */
		if( DaoRoutine_CheckType( rout->routType, rout->nameSpace, selftype, ts, np, codemode, 0 ) ==0){
			rout = NULL;
		}
	}
	return rout;
}
void DaoRoutine_MapTypes( DaoRoutine *self, DMap *deftypes );
void DaoRoutine_PassParamTypes( DaoRoutine *self, DaoType *selftype,
		DaoType *ts[], int np, int code, DMap *defs )
{
	int npar = np;
	int ndef = self->parCount;
	int ifrom, ito;
	int selfChecked = 0;
	DaoType **parType = self->routType->nested->items.pType;
	DaoType **tps = ts;
	DaoType  *abtp, *tp;
	DNode *node;
	DMap *mapNames = self->routType->mapNames;
	/*
	   printf( "%s %s\n", self->routName->mbs, self->routType->name->mbs );
	 */
	/* Check for explicit self parameter: */
	if( np && (ts[0]->attrib & DAO_TYPE_SELFNAMED) ) selftype = NULL;
	if( npar == ndef && ndef == 0 ) return;

	/* Remove type holder bindings for the self parameter: */
	if( self->routType->attrib & DAO_TYPE_SELF ){
		abtp = & self->routType->nested->items.pType[0]->aux->xType;
		DaoType_ResetTypeHolders( abtp, defs );
	}

	if( code == DVM_MCALL && ! (self->routType->attrib & DAO_TYPE_SELF) ){
		npar --;
		tps ++;
	}else if( selftype && (self->routType->attrib & DAO_TYPE_SELF) && code != DVM_MCALL ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		abtp = & self->routType->nested->items.pType[0]->aux->xType;
		if( DaoType_MatchTo( selftype, abtp, defs ) ) selfChecked = 1;
	}
	for(ifrom=0; ifrom<npar; ifrom++){
		ito = ifrom + selfChecked;
		if( ito >= (int)self->routType->nested->size ) break;
		if( ito < ndef && parType[ito]->tid == DAO_PAR_VALIST ){
			DaoType *vlt = (DaoType*) parType[ito]->aux;
			while( ifrom < npar ) DaoType_MatchTo( tps[ifrom++], vlt, defs );
			break;
		}
		tp = tps[ifrom];
		if( tp == NULL ) break;
		if( tp->tid == DAO_PAR_NAMED ){
			node = DMap_Find( mapNames, tp->fname );
			if( node == NULL ) break;
			ito = node->value.pInt;
			tp = & tp->aux->xType;
		}
		abtp = parType[ito];
		if( ito >= ndef || tp ==NULL || abtp ==NULL )  break;
		if( abtp->tid == DAO_PAR_NAMED || abtp->tid == DAO_PAR_DEFAULT ) abtp = & abtp->aux->xType;
		DaoType_MatchTo( tp, abtp, defs );
	}
	/*
	   for(node=DMap_First(defs);node;node=DMap_Next(defs,node))
	   printf( "binding:  %s  %s\n", node->key.pType->name->mbs, node->value.pType->name->mbs );
	 */
	return;
}

static DaoType* DaoType_DeepItemType( DaoType *self )
{
	int i, t=0, n = self->nested ? self->nested->size : 0;
	DaoType *type = self;
	switch( self->tid ){
	case DAO_ARRAY :
	case DAO_LIST :
	case DAO_TUPLE :
		for(i=0; i<n; i++){
			DaoType *tp = DaoType_DeepItemType( self->nested->items.pType[i] );
			if( tp->tid > t ){
				t = tp->tid;
				type = self;
			}
		}
		break;
	default: break;
	}
	return type;
}

enum DaoTypingErrorCode
{
	DTE_TYPE_AMBIGIOUS_PFA = 1,
	DTE_TYPE_NOT_CONSISTENT ,
	DTE_TYPE_NOT_MATCHING ,
	DTE_TYPE_NOT_INITIALIZED,
	DTE_TYPE_WRONG_CONTAINER ,
	DTE_DATA_CANNOT_CREATE ,
	DTE_CALL_INVALID ,
	DTE_CALL_NOT_PERMIT ,
	DTE_CALL_WITHOUT_INSTANCE ,
	DTE_FIELD_NOT_PERMIT ,
	DTE_FIELD_NOT_EXIST ,
	DTE_FIELD_OF_INSTANCE ,
	DTE_ITEM_WRONG_ACCESS ,
	DTE_INDEX_NOT_VALID ,
	DTE_INDEX_WRONG_TYPE ,
	DTE_KEY_NOT_VALID ,
	DTE_KEY_WRONG_TYPE ,
	DTE_OPERATION_NOT_VALID ,
	DTE_PARAM_ERROR ,
	DTE_PARAM_WRONG_NUMBER ,
	DTE_PARAM_WRONG_TYPE ,
	DTE_PARAM_WRONG_NAME ,
	DTE_CONST_WRONG_MODIFYING ,
	DTE_ROUT_NOT_IMPLEMENTED
};
static const char*const DaoTypingErrorString[] =
{
	"",
	"Ambigious partial function application on overloaded functions",
	"Inconsistent typing",
	"Types not matching",
	"Variable not initialized",
	"Wrong container type",
	"Data cannot be created",
	"Invalid call",
	"Call not permitted",
	"Calling nonstatic method without instance",
	"Member not permitted",
	"Member not exist",
	"Need class instance",
	"Invalid index/key access",
	"Invalid index access",
	"Invalid index type",
	"Invalid key acess",
	"Invalid key type",
	"Invalid operation on the type",
	"Invalid parameters for the call",
	"Invalid number of parameter",
	"Invalid parameter type",
	"Invalid parameter name",
	"Constant should not be modified",
	"Call to un-implemented function"
};

static DaoType* DaoCheckBinArith0( DaoRoutine *self, DaoVmCodeX *vmc,
		DaoType *at, DaoType *bt, DaoType *ct, DaoClass *hostClass,
		DString *mbs, int setname )
{
	DaoNamespace *ns = self->nameSpace;
	DaoType *dao_type_int = DaoNamespace_MakeType( ns, "int", DAO_INTEGER, NULL, NULL, 0 );
	DaoType *ts[3];
	DaoRoutine *rout = NULL;
	DaoRoutine *rout2 = NULL;
	DNode *node;
	int opa = vmc->a;
	int opc = vmc->c;
	int code = vmc->code;
	int boolop = code >= DVM_AND && code <= DVM_NE;
	ts[0] = ct;
	ts[1] = at;
	ts[2] = bt;
	if( setname && opa == opc && daoBitBoolArithOpers2[code-DVM_NOT] ){
		/* Check composite assignment operator first: */
		DString_SetMBS( mbs, daoBitBoolArithOpers2[code-DVM_NOT] );
		if( at->tid == DAO_INTERFACE ){
			node = DMap_Find( at->aux->xInterface.methods, mbs );
			rout = node->value.pRoutine;
		}else if( at->tid == DAO_OBJECT ){
			rout = DaoClass_FindOperator( & at->aux->xClass, mbs->mbs, hostClass );
		}else if( at->tid == DAO_CDATA || at->tid == DAO_CSTRUCT ){
			rout = DaoType_FindFunction( at, mbs );
		}
		if( rout ){
			rout2 = rout;
			/* Check the method with self parameter first, then other methods: */
			rout = DaoRoutine_ResolveByType( rout, ts[1], ts+2, 1, DVM_CALL );
			if( rout == NULL ) rout = DaoRoutine_ResolveByType( rout, NULL, ts+1, 2, DVM_CALL );
			/* if the operation is used in the overloaded operator, do operation by address */
			if( boolop && rout == self ) return dao_type_int;
			if( rout ) return ct;
		}
	}
	if( setname ) DString_SetMBS( mbs, daoBitBoolArithOpers[code-DVM_NOT] );
	if( at->tid == DAO_INTERFACE ){
		node = DMap_Find( at->aux->xInterface.methods, mbs );
		rout = node ? node->value.pRoutine : NULL;
	}else if( at->tid == DAO_OBJECT ){
		rout = DaoClass_FindOperator( & at->aux->xClass, mbs->mbs, hostClass );
	}else if( at->tid == DAO_CDATA || at->tid == DAO_CSTRUCT ){
		rout = DaoType_FindFunction( at, mbs );
	}
	if( rout == NULL ) return ct;
	rout2 = rout;
	rout = NULL;
	if( ct ){ /* Check methods that can take all three parameters: */
		/* Check the method with self parameter first, then other methods: */
		rout = DaoRoutine_ResolveByType( rout2, ts[0], ts+1, 2, DVM_CALL );
		if( rout == NULL ) rout = DaoRoutine_ResolveByType( rout2, NULL, ts, 3, DVM_CALL );
	}
	/* Check the method with self parameter first, then other methods: */
	if( rout == NULL ) rout = DaoRoutine_ResolveByType( rout2, ts[1], ts+2, 1, DVM_CALL );
	if( rout == NULL ) rout = DaoRoutine_ResolveByType( rout2, NULL, ts+1, 2, DVM_CALL );
	/* if the operation is used in the overloaded operator, do operation by address */
	if( boolop && rout == self ) return dao_type_int;
	if( rout ) ct = & rout->routType->aux->xType;
	return ct;
}
static DaoType* DaoCheckBinArith( DaoRoutine *self, DaoVmCodeX *vmc,
		DaoType *at, DaoType *bt, DaoType *ct, DaoClass *hostClass, DString *mbs )
{
	DaoType *rt = DaoCheckBinArith0( self, vmc, at, bt, ct, hostClass, mbs, 1 );
	if( rt == NULL && (vmc->code == DVM_LT || vmc->code == DVM_LE) ){
		DString_SetMBS( mbs, vmc->code == DVM_LT ? ">" : ">=" );
		return DaoCheckBinArith0( self, vmc, bt, at, ct, hostClass, mbs, 0 );
	}
	return rt;
}
static DString* AppendError( DArray *errors, DaoValue *rout, size_t type )
{
	DString *s = DString_New(1);
	DArray_Append( errors, rout );
	DArray_Append( errors, s );
	DString_AppendMBS( s, DaoTypingErrorString[ type ] );
	DString_AppendMBS( s, " --- \" " );
	return s;
}
static void DString_AppendTypeError( DString *self, DaoType *from, DaoType *to )
{
	DString_AppendChar( self, '\'' );
	DString_Append( self, from->name );
	DString_AppendMBS( self, "\' for \'" );
	DString_Append( self, to->name );
	DString_AppendMBS( self, "\' \";\n" );
}
void DaoRoutine_CheckError( DaoNamespace *ns, DaoRoutine *rout, DaoType *routType, DaoType *selftype, DaoType *ts[], int np, int codemode, DArray *errors )
{
	DNode *node;
	DMap *defs = DHash_New(0,0);
	DaoType *abtp, **partypes = routType->nested->items.pType;
	DaoValue *routobj = rout ? (DaoValue*)rout : (DaoValue*)routType;
	int npar = np, size = routType->nested->size;
	int i, j, ndef = 0;
	int ifrom, ito;
	int parpass[DAO_MAX_PARAM];
	int selfChecked = 0, selfMatch = 0;
	int code = codemode & 0xffff;
	int b1 = ((codemode>>16) & DAO_CALL_BLOCK) != 0;
	int b2 = (routType->attrib & DAO_TYPE_CODESECT) != 0;

	if( b1 == 0 && b2 != 0 ){
		DString *s = AppendError( errors, routobj, DTE_CALL_INVALID );
		DString_AppendMBS( s, "calling code section method without code section \";\n" );
		goto FinishError;
	}else if( b1 != 0 && b2 == 0 ){
		DString *s = AppendError( errors, routobj, DTE_CALL_INVALID );
		DString_AppendMBS( s, "calling normal method with code section \";\n" );
		goto FinishError;
	}

	if( routType->nested ){
		ndef = routType->nested->size;
		if( ndef ){
			abtp = partypes[ ndef-1 ];
			if( abtp->tid == DAO_PAR_VALIST ) ndef = DAO_MAX_PARAM;
		}
	}

#if 0
	printf( "=====================================\n" );
	printf( "%s\n", self->routName->mbs );
	for( j=0; j<npar; j++){
		DaoType *tp = ts[j];
		if( tp != NULL ) printf( "tp[ %i ]: %s\n", j, tp->name->mbs );
	}
	printf( "%s %i %i\n", routType->name->mbs, ndef, npar );
#endif

	if( code == DVM_MCALL && ! ( routType->attrib & DAO_TYPE_SELF ) ){
		npar --;
		ts ++;
	}else if( selftype && ( routType->attrib & DAO_TYPE_SELF) && code != DVM_MCALL ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		abtp = & partypes[0]->aux->xType;
		selfMatch = DaoType_MatchTo( selftype, abtp, defs );
		if( selfMatch ){
			selfChecked = 1;
			parpass[0] = selfMatch;
		}
	}
	if( npar == ndef && ndef == 0 ) goto FinishOK;
	if( npar > ndef && (size == 0 || partypes[size-1]->tid != DAO_PAR_VALIST ) ){
		DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_NUMBER );
		DString_AppendMBS( s, "too many parameters \";\n" );
		goto FinishError;
	}

	for( j=selfChecked; j<ndef; j++) parpass[j] = 0;
	for(ifrom=0; ifrom<npar; ifrom++){
		DaoType *tp = ts[ifrom];
		ito = ifrom + selfChecked;
		if( ito < ndef && partypes[ito]->tid == DAO_PAR_VALIST ){
			DaoType *vlt = (DaoType*) partypes[ito]->aux;
			for(; ifrom<npar; ifrom++){
				tp = ts[ifrom];
				parpass[ifrom+selfChecked] = vlt ? DaoType_MatchTo( tp, vlt, defs ) : 1;
				if( parpass[ifrom+selfChecked] == 0 ){
					DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_TYPE );
					abtp = DaoType_DefineTypes( vlt, ns, defs );
					DString_AppendTypeError( s, tp, abtp );
					goto FinishError;
				}
			}
			break;
		}
		if( tp == NULL ){
			DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_TYPE );
			DString_AppendMBS( s, "unknown parameter type \";\n" );
			goto FinishError;
		}
		if( tp->tid == DAO_PAR_NAMED ){
			node = DMap_Find( routType->mapNames, tp->fname );
			if( node == NULL ){
				DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_NAME );
				DString_Append( s, tp->fname );
				DString_AppendMBS( s, " \";\n" );
				goto FinishError;
			}
			ito = node->value.pInt;
			tp = & tp->aux->xType;
		}
		if( tp ==NULL ){
			DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_TYPE );
			DString_AppendMBS( s, "unknown parameter type \";\n" );
			goto FinishError;
		}else if( ito >= ndef ){
			DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_NUMBER );
			DString_AppendMBS( s, "too many parameters \";\n" );
			goto FinishError;
		}
		abtp = routType->nested->items.pType[ito];
		if( abtp->tid == DAO_PAR_NAMED || abtp->tid == DAO_PAR_DEFAULT ) abtp = & abtp->aux->xType;
		parpass[ito] = DaoType_MatchTo( tp, abtp, defs );

#if 0
		printf( "%p %s %p %s\n", tp->aux, tp->name->mbs, abtp->aux, abtp->name->mbs );
		printf( "%i:  %i\n", ito, parpass[ito] );
#endif

		/* less strict */
		if( tp && parpass[ito] ==0 ){
			if( tp->tid == DAO_ANY && abtp->tid == DAO_ANY )
				parpass[ito] = DAO_MT_ANY;
			else if( tp->tid == DAO_ANY || tp->tid == DAO_UDT )
				parpass[ito] = DAO_MT_NEGLECT;
		}
		if( parpass[ito] == 0 ){
			DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_TYPE );
			tp = ts[ifrom];
			abtp = routType->nested->items.pType[ito];
			abtp = DaoType_DefineTypes( abtp, ns, defs );
			DString_AppendTypeError( s, tp, abtp );
			goto FinishError;
		}
	}
	for(ito=0; ito<ndef; ito++){
		i = partypes[ito]->tid;
		if( i == DAO_PAR_VALIST ) break;
		if( parpass[ito] ) continue;
		if( i != DAO_PAR_DEFAULT ){
			DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_NUMBER );
			DString_AppendMBS( s, "too few parameters \";\n" );
			goto FinishError;
		}
		parpass[ito] = 1;
	}

	/*
	   printf( "%s %i\n", routType->name->mbs, *min );
	 */
FinishOK:
FinishError:
	DMap_Delete( defs );
}
DaoRoutine* DaoValue_Check( DaoRoutine *self, DaoType *selftype, DaoType *ts[], int np, int codemode, DArray *errors )
{
	int i, n;
	DaoRoutine *rout = DaoRoutine_ResolveByType( self, selftype, ts, np, codemode );
	if( rout ) return rout;
	if( self->overloads == NULL ){
		DaoRoutine_CheckError( self->nameSpace, self, self->routType, selftype, ts, np, codemode, errors );
		return NULL;
	}
	for(i=0,n=self->overloads->routines->size; i<n; i++){
		DaoRoutine *rout = self->overloads->routines->items.pRoutine[i];
		/*
		   printf( "=====================================\n" );
		   printf("ovld %i, %p %s : %s\n", i, rout, self->routName->mbs, rout->routType->name->mbs);
		 */
		DaoRoutine_CheckError( rout->nameSpace, rout, rout->routType, selftype, ts, np, codemode, errors );
	}
	return NULL;
}

void DaoPrintCallError( DArray *errors, DaoStream *stream )
{
	DString *mbs = DString_New(1);
	int i, k, n;
	for(i=0,n=errors->size; i<n; i+=2){
		DaoType *routType = errors->items.pType[i];
		DaoRoutine *rout = NULL;
		if( routType->type == DAO_ROUTINE ){
			rout = errors->items.pRoutine[i];
			routType = rout->routType;
		}
		DaoStream_WriteMBS( stream, "  ** " );
		DaoStream_WriteString( stream, errors->items.pString[i+1] );
		DaoStream_WriteMBS( stream, "     Assuming  : " );
		if( rout ){
			if( DaoToken_IsValidName( rout->routName->mbs, rout->routName->size ) ){
				DaoStream_WriteMBS( stream, "routine " );
			}else{
				DaoStream_WriteMBS( stream, "operator " );
			}
			k = DString_RFindMBS( routType->name, "=>", routType->name->size );
			DString_Assign( mbs, rout->routName );
			DString_AppendChar( mbs, '(' );
			DString_AppendDataMBS( mbs, routType->name->mbs + 8, k-9 );
			DString_AppendChar( mbs, ')' );
			if( routType->aux && routType->aux->type == DAO_TYPE ){
				DString_AppendMBS( mbs, "=>" );
				DString_Append( mbs, routType->aux->xType.name );
			}
		}else{
			DaoStream_WriteString( stream, routType->name );
		}
		DString_AppendMBS( mbs, ";\n" );
		DaoStream_WriteString( stream, mbs );
		if( rout ){
			DaoStream_WriteMBS( stream, "     Reference : " );
			if( rout->body ){
				DaoStream_WriteMBS( stream, "line " );
				DaoStream_WriteInt( stream, rout->defLine );
				DaoStream_WriteMBS( stream, ", " );
			}
			DaoStream_WriteMBS( stream, "file \"" );
			DaoStream_WriteString( stream, rout->nameSpace->name );
			DaoStream_WriteMBS( stream, "\";\n" );
		}
		DString_Delete( errors->items.pString[i+1] );
	}
	DString_Delete( mbs );
}

static DaoInode* DaoInferencer_InsertNode( DaoInferencer *self, DaoInode *inode, int code, int addreg, DaoType *type )
{
	DaoInode *next = inode;
	DaoInode *prev = inode->prev;

	inode = DaoInode_New();
	*(DaoVmCodeX*)inode = *(DaoVmCodeX*)next;
	inode->code = code;
	if( addreg ){
		inode->c = self->types->size;
		DArray_Append( self->types, type );
	}
	if( prev ){
		prev->next = inode;
		inode->prev = next;
	}
	inode->next = next;
	next->prev = inode;
	/* For proper setting up the jumps: */
	if( next->extra == NULL ) next->extra = inode;
	return inode;
}
static DaoInode* DaoInferencer_InsertMove( DaoInferencer *self, DaoInode *inode, unsigned short *op, DaoType *at, DaoType *ct )
{
	int code = DVM_MOVE_II + 3*(ct->tid - DAO_INTEGER) + (at->tid - DAO_INTEGER);
	DaoInode *move = DaoInferencer_InsertNode( self, inode, code, 1, ct );
	move->a = *op;
	*op = move->c;
	return move;
}
static DaoInode* DaoInferencer_InsertCast( DaoInferencer *self, DaoInode *inode, unsigned short *op, DaoType *ct )
{
	DaoInode *cast = DaoInferencer_InsertNode( self, inode, DVM_CAST, 1, ct );
	cast->a = *op;
	*op = cast->c;
	return cast;
}
static void DaoInferencer_InsertMove2( DaoInferencer *self, DaoInode *inode, DaoType *at, DaoType *ct )
{
	unsigned short k = inode->c;
	DaoVmCodeX vmc2 = *(DaoVmCodeX*)inode;
	DaoInode *inode2 = DaoInferencer_InsertMove( self, inode, & k, at, ct );
	*(DaoVmCodeX*)inode = *(DaoVmCodeX*)inode2;
	*(DaoVmCodeX*)inode2 = vmc2;
	inode2->c = inode->c;
	k = inode->a;
	inode->a = inode->c;
	inode->c = k;
}
static void DaoInferencer_Finalize( DaoInferencer *self )
{
	DaoRoutineBody *body = self->routine->body;

	DaoRoutine_CodesFromInodes( self->routine, self->inodes );
	DArray_Assign( body->regType, self->types );

	body->regCount = body->regType->size;
	DaoRoutine_SetupSimpleVars( self->routine );
	body->specialized = 1;
}
static DaoType* DaoInferencer_UpdateType( DaoInferencer *self, int id, DaoType *type )
{
	DaoNamespace *NS = self->routine->nameSpace;
	DaoType **types = self->types->items.pType;
	DMap *defs = (DMap*)DArray_Back( self->typeMaps );
	/*
	// Do NOT update types that have been inferred (even as undefined types):
	// Because if it has been inferred, some instructions may have been
	// specialized according to this inferred type. If it is allowed to
	// be updated here, other instructions may be specialized differently.
	// So the previously specialized instruction and the currently specialized
	// instruction will assume different types of the same register!
	//
	// This happens for short curcuit evaluation of boolean operations:
	// expression_produces_string_but_inferred_as_undefined && expression_produces_int
	*/
	if( types[id] != NULL ) return types[id];

	/*
	// Routine type should not be specialized implicitly?
	// a = {1}
	// b = {2, 3, 4}
	// a.iterate::{
	//     b.iterate::{}
	// }
	// Otherwise, the type of b.iterate will be specialized for int type
	// due to the association of type holder @T to int by a.iterate::{}.
	// But the methods for list are not specialized, so at runtime,
	// b.iterate will return the original routine with type holders,
	// which will not match to the specialized routine type for int!
	*/
	if( type->tid != DAO_ROUTINE && (type->attrib & DAO_TYPE_SPEC) ){
		type = DaoType_DefineTypes( type, NS, defs );
	}
	GC_ShiftRC( type, types[id] );
	types[id] = type;
	return types[id];
}
static void DaoInferencer_WriteErrorHeader( DaoInferencer *self )
{
	char char50[50], char200[200];
	DaoRoutine *routine = self->routine;
	DaoStream  *stream = routine->nameSpace->vmSpace->errorStream;
	DaoVmCodeX *vmc;

	self->error = 1;
	if( self->silent ) return;

	vmc = routine->body->annotCodes->items.pVmc[self->currentIndex];
	sprintf( char200, "%s:%i,%i,%i", DaoVmCode_GetOpcodeName( vmc->code ), vmc->a, vmc->b, vmc->c );

	DaoStream_WriteMBS( stream, "[[ERROR]] in file \"" );
	DaoStream_WriteString( stream, routine->nameSpace->name );
	DaoStream_WriteMBS( stream, "\":\n" );
	sprintf( char50, "  At line %i : ", routine->defLine );
	DaoStream_WriteMBS( stream, char50 );
	DaoStream_WriteMBS( stream, "Invalid function definition --- \" " );
	DaoStream_WriteString( stream, routine->routName );
	DaoStream_WriteMBS( stream, "() \";\n" );
	sprintf( char50, "  At line %i : ", vmc->line );
	DaoStream_WriteMBS( stream, char50 );
	DaoStream_WriteMBS( stream, "Invalid virtual machine instruction --- \" " );
	DaoStream_WriteMBS( stream, char200 );
	DaoStream_WriteMBS( stream, " \";\n" );
}
static void DaoInferencer_WriteErrorGeneral( DaoInferencer *self, int error )
{
	char char50[50], char200[200];
	DaoRoutine *routine = self->routine;
	DaoStream  *stream = routine->nameSpace->vmSpace->errorStream;
	DaoVmCodeX *vmc = self->inodes->items.pVmc[self->currentIndex];
	DString *mbs;

	if( error == 0 ) return;

	self->error = 1;
	if( self->silent ) return;
	sprintf( char50, "  At line %i : ", vmc->line );

	mbs = DString_New(1);
	DaoStream_WriteMBS( stream, char50 );
	DaoStream_WriteMBS( stream, DaoTypingErrorString[error] );
	DaoStream_WriteMBS( stream, " --- \" " );
	DaoLexer_AnnotateCode( routine->body->source, *vmc, mbs, 32 );
	DaoStream_WriteString( stream, mbs );
	if( error == DTE_FIELD_NOT_EXIST ){
		DaoStream_WriteMBS( stream, " for " );
		DaoStream_WriteMBS( stream, self->type_source->name->mbs );
	}
	DaoStream_WriteMBS( stream, " \";\n" );
	DString_Delete( mbs );
}
static void DaoInferencer_WriteErrorSpecific( DaoInferencer *self, int error )
{
	char char50[50], char200[200];
	int annot_first = self->annot_first;
	int annot_last = self->annot_last;
	DaoRoutine *routine = self->routine;
	DaoStream  *stream = routine->nameSpace->vmSpace->errorStream;
	DaoVmCodeX *vmc = self->inodes->items.pVmc[self->currentIndex];
	DaoVmCodeX vmc2 = *vmc;
	DString *mbs;

	if( error == 0 ) return;

	self->error = 1;
	if( self->silent ) return;

	mbs = DString_New(1);
	DaoStream_WriteMBS( stream, char50 );
	DaoStream_WriteMBS( stream, DaoTypingErrorString[error] );
	DaoStream_WriteMBS( stream, " --- \" " );
	if( error == DTE_TYPE_NOT_INITIALIZED ){
		vmc2.middle = 0;
		vmc2.first = annot_first;
		vmc2.last = annot_last > annot_first ? annot_last - annot_first : 0;
		DaoLexer_AnnotateCode( routine->body->source, vmc2, mbs, 32 );
	}else if( error == DTE_TYPE_NOT_MATCHING ){
		DString_SetMBS( mbs, "'" );
		DString_AppendMBS( mbs, self->type_source ? self->type_source->name->mbs : "none" );
		DString_AppendMBS( mbs, "' for '" );
		if( self->type_target ){
			DString_AppendMBS( mbs, self->type_target->name->mbs );
		}else if( self->tid_target <= DAO_TUPLE ){
			DString_AppendMBS( mbs, coreTypeNames[self->tid_target] );
		}
		DString_AppendChar( mbs, '\'' );
	}else{
		DaoLexer_AnnotateCode( routine->body->source, *vmc, mbs, 32 );
	}
	DaoStream_WriteString( stream, mbs );
	DaoStream_WriteMBS( stream, " \";\n" );
	DString_Delete( mbs );
}
static int DaoInferencer_Error( DaoInferencer *self, int error )
{
	DaoInferencer_WriteErrorHeader( self );
	if( self->errors->size ){
		DaoPrintCallError( self->errors, self->routine->nameSpace->vmSpace->errorStream );
		return 0;
	}
	DaoInferencer_WriteErrorGeneral( self, error );
	return 0;
}
static int DaoInferencer_ErrorModifyConst( DaoInferencer *self )
{
	return DaoInferencer_Error( self, DTE_CONST_WRONG_MODIFYING );
}
static int DaoInferencer_ErrorTypeNotMatching( DaoInferencer *self, DaoType *S, DaoType *T )
{
	if( S ) self->type_source = S;
	if( T ) self->type_target = T;
	DaoInferencer_WriteErrorHeader( self );
	DaoInferencer_WriteErrorGeneral( self, DTE_TYPE_NOT_MATCHING );
	DaoInferencer_WriteErrorSpecific( self, DTE_TYPE_NOT_MATCHING );
	return 0;
}
static int DaoInferencer_ErrorTypeID( DaoInferencer *self, int tid )
{
	self->tid_target = tid;
	DaoInferencer_WriteErrorHeader( self );
	DaoInferencer_WriteErrorGeneral( self, DTE_TYPE_NOT_MATCHING );
	DaoInferencer_WriteErrorSpecific( self, DTE_TYPE_NOT_MATCHING );
	return 0;
}
static int DaoInferencer_ErrorNotInitialized( DaoInferencer *self, int error, int first, int last )
{
	self->annot_first = first;
	self->annot_last = last;
	DaoInferencer_WriteErrorHeader( self );
	DaoInferencer_WriteErrorGeneral( self, error );
	DaoInferencer_WriteErrorSpecific( self, DTE_TYPE_NOT_INITIALIZED );
	return 0;
}
static int DaoInferencer_ErrorInvalidIndex( DaoInferencer *self )
{
	return DaoInferencer_Error( self, DTE_INDEX_NOT_VALID );
}

#define NoCheckingType(t) (t->tid & DAO_ANY)

static int DaoInferencer_AssertPairNumberType( DaoInferencer *self, DaoType *type )
{
	int k;
	DaoType *itp = type->nested->items.pType[0];
	if( itp->tid == DAO_PAR_NAMED ) itp = & itp->aux->xType;
	k = itp->tid;
	if( k == DAO_VALTYPE ) k = itp->aux->type;
	if( k > DAO_DOUBLE && ! NoCheckingType(itp) ) return 0;
	itp = type->nested->items.pType[1];
	if( itp->tid == DAO_PAR_NAMED ) itp = & itp->aux->xType;
	k = itp->tid;
	if( k == DAO_VALTYPE ) k = itp->aux->type;
	if( k > DAO_DOUBLE && ! NoCheckingType(itp) ) return 0;
	return 1;
}


#define AssertTypeMatching( source, target, defs ) \
	if( !(source->tid & DAO_ANY ) && DaoType_MatchTo( source, target, defs ) ==0 ) \
		return DaoInferencer_ErrorTypeNotMatching( self, source, target );

#define AssertTypeIdMatching( source, id ) \
	if( source == NULL || source->tid != id ) return DaoInferencer_ErrorTypeID( self, id );

#define AssertInitialized( reg, ec, first, last ) { \
	if( types[reg] && types[reg]->isempty1 ) DArray_Append( usedtypes, types[reg] ); \
	if( inited[reg] == 0 || types[reg] == NULL ) \
		return DaoInferencer_ErrorNotInitialized( self, ec, first, last ); }

#define AssertPairNumberType( tp ) \
	if( DaoInferencer_AssertPairNumberType( self, tp ) == 0 ) \
		return DaoInferencer_ErrorTypeNotMatching( self, NULL, NULL );


int DaoInferencer_DoInference( DaoInferencer *self )
{
	DNode *node;
	DMap *defs = self->defs;
	DMap *defs2 = self->defs2;
	DMap *defs3 = self->defs3;
	DArray *errors = self->errors;
	DString *str, *mbs = self->mbstring;
	DaoRoutine *closure;
	DaoVmCodeX *vmc, *vmc2;
	DaoType *at, *bt, *ct, *tt, *catype, *ts[DAO_ARRAY+DAO_MAX_PARAM];
	DaoType *type, **tp, **type2, **types = self->types->items.pType;
	DaoValue *value, **pp, **consts = self->consts->items.pValue;
	DaoInode *inode, *inode2, **inodes = self->inodes->items.pInode;
	DaoRoutine *rout, *rout2, *meth, *routine = self->routine;
	DaoStream  *stream = routine->nameSpace->vmSpace->errorStream;
	DaoClass *klass, *hostClass = self->hostClass;
	DaoNamespace *NS = routine->nameSpace;
	DaoRoutineBody *body = routine->body;
	DaoInteger integer = {DAO_INTEGER,0,0,0,1,0};
	DaoType **typeVH[DAO_MAX_SECTDEPTH+1] = { NULL };
	DArray  *usedtypes = self->usedtypes;
	DArray  *rettypes = self->rettypes;
	DArray  *routConsts = & routine->routConsts->items;
	daoint i, n, N = routine->body->annotCodes->size;
	daoint j, k, m, J, K, M = routine->body->regCount;
	char codetype, *inited = self->inited->mbs;
	int typed_code = daoConfig.typedcode;
	int notide = ! (NS->vmSpace->options & DAO_OPTION_IDE);
	int code, opa, opb, opc, first, middle, last;
	int TT1, TT2, TT3, TT4, TT5, TT6;

	if( self->inodes->size == 0 ) return 1;
	/* DaoRoutine_PrintCode( routine, routine->nameSpace->vmSpace->errorStream ); */

	catype = DaoNamespace_MakeType( NS, "array", DAO_ARRAY, NULL, & dao_type_complex, 1 );

	for(i=1; i<=DAO_MAX_SECTDEPTH; i++) typeVH[i] = types;

	usedtypes->size = 0;
	DArray_Append( rettypes, IntToPointer( N ) );
	DArray_Append( rettypes, NULL );
	DArray_Append( rettypes, routine->routType->aux );
	DArray_Append( rettypes, routine->routType->aux );
	for(i=0; i<N; i++){
		types = self->types->items.pType;
		self->currentIndex = i;
		inode = inodes[i];
		vmc = (DaoVmCodeX*) inode;
		code = vmc->code;
		opa = vmc->a;  opb = vmc->b;  opc = vmc->c;
		at = opa < M ? types[opa] : NULL;
		bt = opb < M ? types[opb] : NULL;
		ct = opc < M ? types[opc] : NULL;
		first = vmc->first;
		middle = first + vmc->middle;
		last = middle + vmc->last;

		if( i >= rettypes->items.pInt[ rettypes->size - 4 ] ){
			DArray_Erase( rettypes, rettypes->size - 4, -1 );
			DArray_PopBack( self->typeMaps );
		}
		DMap_Reset( defs );
		DMap_Assign( defs, (DMap*)DArray_Back( self->typeMaps ) );

#if 0
		DaoLexer_AnnotateCode( routine->body->source, *(DaoVmCodeX*)inode, mbs, 24 );
		printf( "%4i: ", i );DaoVmCodeX_Print( *(DaoVmCodeX*)inode, mbs->mbs );
#endif

		for(j=0; j<usedtypes->size; ++j) usedtypes->items.pType[j]->isempty1 = 0;
		usedtypes->size = 0;

		K = DaoVmCode_GetOpcodeType( (DaoVmCode*) inode );
		/* No need to check for operands in expression list,
		// they must have been checked by other instructions. */
		switch( K ){
		case DAO_CODE_GETU :
			if( inode->a != 0 ) AssertInitialized( inode->b, 0, middle, middle );
			break;
		case DAO_CODE_UNARY2 :
			AssertInitialized( inode->b, 0, middle, middle );
			break;
		case DAO_CODE_GETF :
		case DAO_CODE_BRANCH :
			AssertInitialized( inode->a, 0, first, first );
			break;
		case DAO_CODE_SETG :
		case DAO_CODE_SETU :
			AssertInitialized( inode->a, 0, first, first );
			break;
		case DAO_CODE_MOVE :
		case DAO_CODE_UNARY :
			AssertInitialized( inode->a, 0, first, last );
			break;
		case DAO_CODE_SETF :
			AssertInitialized( inode->a, 0, last+1, last+1 );
			AssertInitialized( inode->c, 0, first, first );
			break;
		case DAO_CODE_GETI :
			AssertInitialized( inode->a, DTE_ITEM_WRONG_ACCESS, first, first );
			AssertInitialized( inode->b, DTE_ITEM_WRONG_ACCESS, middle, middle );
			break;
		case DAO_CODE_BINARY :
			AssertInitialized( inode->a, 0, first, first );
			AssertInitialized( inode->b, 0, middle, middle );
			break;
		case DAO_CODE_SETI :
			AssertInitialized( inode->c, DTE_ITEM_WRONG_ACCESS, first, first );
			AssertInitialized( inode->b, DTE_ITEM_WRONG_ACCESS, middle, middle );
			AssertInitialized( inode->a, DTE_ITEM_WRONG_ACCESS, last+1, last+1 );
			break;
		case DAO_CODE_GETM :
		case DAO_CODE_ENUM2 :
		case DAO_CODE_ROUTINE :
			if( code == DVM_EVAL ){
				if( opb == 1 ) AssertInitialized( opa, 0, first, first );
				break;
			}
			for(j=0; j<=opb; ++j) AssertInitialized( opa+j, 0, first, first );
			break;
		case DAO_CODE_SETM :
			AssertInitialized( inode->c, DTE_ITEM_WRONG_ACCESS, first, first );
			for(j=0; j<=opb; ++j) AssertInitialized( opc+j, 0, first, first );
			break;
		case DAO_CODE_MATRIX :
			J=(opb>>8)*(opb&0xff);
			for(j=0; j<J; ++j) AssertInitialized( opa+j, 0, first, first );
			break;
		case DAO_CODE_CALL :
			for(j=0,J=opb&0xff; j<=J; ++j) AssertInitialized( opa+j, 0, middle, last );
			break;
		case DAO_CODE_ENUM :
		case DAO_CODE_YIELD :
		case DAO_CODE_EXPLIST :
			for(j=0; j<opb; ++j) AssertInitialized( opa+j, 0, first, first );
			break;
		}
		if( K && K < DAO_CODE_EXPLIST && K != DAO_CODE_SETG && K != DAO_CODE_SETU ){
			if( K != DAO_CODE_SETM ) inited[inode->c] = 1;
			if( consts[inode->c] && K == DAO_CODE_SETF ){
				k = consts[inode->c]->type;
				if( k != DAO_CLASS && k != DAO_NAMESPACE ) goto ModifyConstant;
			}else if( consts[inode->c] && K > DAO_CODE_GETG ) goto ModifyConstant;
		}

		switch( code ){
		case DVM_NOP :
		case DVM_DEBUG :
			break;
		case DVM_DATA :
			if( opa > DAO_STRING ) return DaoInferencer_Error( self, DTE_DATA_CANNOT_CREATE );
			at = self->basicTypes[ opa ];
			if( types[opc]== NULL || types[opc]->tid == DAO_UDT ){
				DaoInferencer_UpdateType( self, opc, at );
			}else{
				AssertTypeMatching( at, types[opc], defs );
			}
			value = NULL;
			switch( opa ){
			case DAO_NONE : value = dao_none_value; break;
			case DAO_INTEGER : value = (DaoValue*) DaoInteger_New( opb ); break;
			case DAO_FLOAT : value = (DaoValue*) DaoFloat_New( opb ); break;
			case DAO_DOUBLE : value = (DaoValue*) DaoDouble_New( opb ); break;
			}
			GC_ShiftRC( value, consts[opc] );
			consts[opc] = value;
			if( typed_code && at->tid >= DAO_INTEGER && at->tid <= DAO_COMPLEX ){
				vmc->code = DVM_DATA_I + (at->tid - DAO_INTEGER);
			}
			break;
		case DVM_GETCL :
		case DVM_GETCK :
		case DVM_GETCG :
			switch( code ){
			case DVM_GETCL : value = routConsts->items.pValue[opb]; break;
			case DVM_GETCK : value = hostClass->constants->items.pConst[opb]->value; break;
			case DVM_GETCG : value = NS->constants->items.pConst[opb]->value; break;
			}
			at = DaoNamespace_GetType( NS, value );
			DaoInferencer_UpdateType( self, opc, at );
			/*
			   printf( "at %i %i %p, %p\n", at->tid, types[opc]->tid, at, types[opc] );
			 */
			AssertTypeMatching( at, types[opc], defs );
			GC_ShiftRC( value, consts[opc] );
			consts[opc] = value;
			if( typed_code && at->tid >= DAO_INTEGER && at->tid <= DAO_COMPLEX ){
				vmc->code = DVM_GETCL_I + 4*(code-DVM_GETCL) + (at->tid - DAO_INTEGER);
			}
			break;
		case DVM_GETVH :
		case DVM_GETVS :
		case DVM_GETVO :
		case DVM_GETVK :
		case DVM_GETVG :
			at = 0;
			switch( code ){
			case DVM_GETVH : at = typeVH[opa][opb]; break;
			case DVM_GETVS : at = body->svariables->items.pVar[opb]->dtype; break;
			case DVM_GETVO : at = hostClass->instvars->items.pVar[opb]->dtype; break;
			case DVM_GETVK : at = hostClass->variables->items.pVar[opb]->dtype; break;
			case DVM_GETVG : at = NS->variables->items.pVar[opb]->dtype; break;
			}
			if( at == NULL ) at = dao_type_udf;
			DaoInferencer_UpdateType( self, opc, at );
			/*
			   printf( "%s\n", at->name->mbs );
			   printf( "%p %p\n", at, types[opc] );
			   printf( "%s %s\n", at->name->mbs, types[opc]->name->mbs );
			 */
			AssertTypeMatching( at, types[opc], defs );
			if( typed_code && at->tid >= DAO_INTEGER && at->tid <= DAO_COMPLEX ){
				vmc->code = DVM_GETVH_I + 4*(code-DVM_GETVH) + (at->tid - DAO_INTEGER);
			}
			break;
		case DVM_SETVH :
		case DVM_SETVS :
		case DVM_SETVO :
		case DVM_SETVK :
		case DVM_SETVG :
			type2 = NULL;
			switch( code ){
			case DVM_SETVH : type2 = typeVH[opc] + opb; break;
			case DVM_SETVS : type2 = & body->svariables->items.pVar[opb]->dtype; break;
			case DVM_SETVO : type2 = & hostClass->instvars->items.pVar[opb]->dtype; break;
			case DVM_SETVK : type2 = & hostClass->variables->items.pVar[opb]->dtype; break;
			case DVM_SETVG : type2 = & NS->variables->items.pVar[opb]->dtype; break;
			}
			at = types[opa];
			if( type2 && ( *type2 == NULL || (*type2)->tid == DAO_UDT ) ){
				GC_ShiftRC( at, *type2 );
				*type2 = at;
			}
			/* less strict checking */
			if( types[opa]->tid & DAO_ANY ) break;
			if( type2 == 0 ) break;

			k = DaoType_MatchTo( types[opa], *type2, defs );
			if( k ==0 ) return DaoInferencer_ErrorTypeNotMatching( self, types[opa], *type2 );
			at = types[opa];
			if( type2[0]->tid && type2[0]->tid <= DAO_COMPLEX && at->tid && at->tid <= DAO_COMPLEX ){
				if( typed_code ){
					if( code == DVM_SETVG ){
						DaoVariable *var = NS->variables->items.pVar[opb];
						if( var->value == NULL || var->value->type != at->value->type ){
							GC_DecRC( var->value );
							var->value = DaoValue_SimpleCopy( at->value );
							GC_IncRC( var->value );
						}
					}
					if( at->tid != type2[0]->tid ){
						DaoInferencer_InsertMove( self, inode, & inode->a, at, *type2 );
					}
					vmc->code = DVM_SETVH_II + type2[0]->tid - DAO_INTEGER;
					vmc->code += 4*(code - DVM_SETVH);
				}
			}else if( k == DAO_MT_SUB && notide ){
				/* global L = { 1.5, 2.5 }; L = { 1, 2 }; L[0] = 3.5 */
				DaoInferencer_InsertCast( self, inode, & inode->a, *type2 );
			}
			break;
		case DVM_GETI :
		case DVM_GETDI :
			value = consts[opa] ? dao_none_value : NULL;

			integer.value = opb;
			value = (DaoValue*)(DaoInteger*)&integer;
			bt = dao_type_int;
			if( code == DVM_GETI ){
				bt = types[opb];
				value = consts[opb];
			}
			ct = NULL;
			k = at->tid != DAO_CLASS && at->tid != DAO_OBJECT;
			k = k && at->tid != DAO_CDATA && at->tid != DAO_CSTRUCT;
			if( value && value->type == 0 && k && bt->tid == DAO_VALTYPE ){ /* a[] */
				ct = at;
			}else if( NoCheckingType( at ) || NoCheckingType( bt ) ){
				/* allow less strict typing: */
				ct = dao_type_udf;
			}else if( at->tid == DAO_INTEGER ){
				ct = dao_type_int;
				if( bt->tid > DAO_DOUBLE ) return DaoInferencer_ErrorInvalidIndex( self );
			}else if( at->tid == DAO_STRING ){
				ct = at;
				if( bt->realnum ){
					ct = dao_type_int;
					if( typed_code && code == DVM_GETI ){
						if( bt->tid != DAO_INTEGER && notide ){
							DaoInferencer_InsertMove( self, inode, & inode->b, bt, dao_type_int );
							bt = dao_type_int;
						}
						if( bt->tid == DAO_INTEGER ) vmc->code = DVM_GETI_SI;
					}
				}else if( bt == dao_type_for_iterator ){
					ct = dao_type_int;
				}else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2 ){
					ct = at;
					AssertPairNumberType( bt );
				}else if( bt->tid ==DAO_LIST || bt->tid ==DAO_ARRAY ){
					/* passed */
					k = bt->nested->items.pType[0]->tid;
					if( k > DAO_DOUBLE && k !=DAO_ANY ) goto NotMatch;
				}
			}else if( at->tid == DAO_LONG ){
				ct = dao_type_int; /* XXX slicing */
			}else if( at->tid == DAO_TYPE ){
				at = at->nested->items.pType[0];
				if( at->tid == DAO_ENUM && at->mapNames ){
					ct = at; /* TODO const index */
				}else{
					self->type_source = at;
					goto NotExist;
				}
			}else if( at->tid == DAO_LIST ){
				if( bt->realnum ){
					ct = at->nested->items.pType[0];
					if( typed_code && notide && code == DVM_GETI ){
						if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_COMPLEX ){
							vmc->code = DVM_GETI_LII + ct->tid - DAO_INTEGER;
						}else if( ct->tid == DAO_STRING ){
							vmc->code = DVM_GETI_LSI;
						}else if( ct->tid >= DAO_ARRAY && ct->tid < DAO_ANY ){
							/* for skipping type checking */
							vmc->code = DVM_GETI_LI;
						}
						if( bt->tid != DAO_INTEGER )
							DaoInferencer_InsertMove( self, inode, & inode->b, bt, dao_type_int );
					}
				}else if( bt == dao_type_for_iterator ){
					ct = at->nested->items.pType[0];
				}else if( bt->tid == DAO_TUPLE && bt->nested->size == 2 ){
					ct = at;
					AssertPairNumberType( bt );
				}else if( bt->tid == DAO_LIST || bt->tid == DAO_ARRAY ){
					ct = at;
					k = bt->nested->items.pType[0]->tid;
					if( k != DAO_INTEGER && k != DAO_ANY && k != DAO_UDT ) goto NotMatch;
				}else{
					goto InvIndex;
				}
			}else if( at->tid == DAO_MAP ){
				DaoType *t0 = at->nested->items.pType[0];
				/*
				   printf( "at %s %s\n", at->name->mbs, bt->name->mbs );
				 */
				if( bt == dao_type_for_iterator ){
					ct = DaoNamespace_MakeType( NS, "tuple", DAO_TUPLE,
							NULL, at->nested->items.pType, 2 );
				}else if( bt->tid == DAO_TUPLE ){  /* Check slicing: */
					DaoType *start, *end, **kts = bt->nested->items.pType;
					int openStart = 0, openEnd = 0;
					if( bt->nested->size != 2 ) goto InvKey;
					start = kts[0]->tid == DAO_PAR_NAMED ? (DaoType*) kts[0]->aux : kts[0];
					end   = kts[1]->tid == DAO_PAR_NAMED ? (DaoType*) kts[1]->aux : kts[1];
					openStart = start->tid == DAO_VALTYPE && start->aux->type == DAO_NONE;
					openEnd   = end->tid == DAO_VALTYPE   && end->aux->type == DAO_NONE;
					if( !openStart && DaoType_MatchTo( start, t0, defs ) == 0 ) goto InvKey;
					if( !openEnd && DaoType_MatchTo( end, t0, defs ) == 0 ) goto InvKey;
					ct = at;
				}else{
					if( DaoType_MatchTo( bt, t0, defs ) == 0 ) goto InvKey;
					ct = at->nested->items.pType[1];
				}
			}else if( at->tid == DAO_ARRAY ){
				if( bt->realnum ){
					/* array[i] */
					ct = at->nested->items.pType[0];
					if( typed_code && notide && code == DVM_GETI ){
						if( ct->realnum || ct->tid == DAO_COMPLEX )
							vmc->code = DVM_GETI_AII + ct->tid - DAO_INTEGER;
						if( bt->tid != DAO_INTEGER )
							DaoInferencer_InsertMove( self, inode, & inode->b, bt, dao_type_int );
					}
				}else if( bt == dao_type_for_iterator ){
					ct = at->nested->items.pType[0];
				}else if( bt->tid == DAO_TUPLE ){
					if( bt->nested->size != 2 ) goto InvIndex;
					ct = at;
					AssertPairNumberType( bt );
				}else if( bt->tid == DAO_LIST || bt->tid == DAO_ARRAY ){
					/* array[ {1,2,3} ] or array[ [1,2,3] ] */
					ct = at;
					k = bt->nested->items.pType[0]->tid;
					if( k != DAO_INTEGER && k != DAO_ANY && k != DAO_UDT ) goto NotMatch;
				}
			}else if( at->tid == DAO_TUPLE ){
				DaoTuple *tupidx = DaoValue_CastTuple( value );
				ct = dao_type_udf;
				/* tuple slicing with constant index range, will produce a tuple with type
				// determined from the index range. For variable range, it produces tuple<...>. */
				if( tupidx && tupidx->subtype == DAO_PAIR ){
					DaoValue *first = value->xTuple.items[0];
					DaoValue *second = value->xTuple.items[1];
					daoint start = DaoValue_GetInteger( first );
					daoint end = DaoValue_GetInteger( second );
					/* Note: a tuple may contain more items than what are explicitly typed. */
					if( start < 0 || end < 0 ) goto InvIndex; /* No support for negative index; */
					if( at->variadic == 0 && start >= at->nested->size ) goto InvIndex;
					if( at->variadic == 0 && end >= at->nested->size ) goto InvIndex;
					if( first->type > DAO_DOUBLE || second->type > DAO_DOUBLE ) goto InvIndex;
					if( first->type == DAO_NONE && second->type == DAO_NONE ){
						ct = at;
					}else{
						end = second->type == DAO_NONE ? at->nested->size : end + 1;
						if( end >= at->nested->size ) end = at->nested->size;
						if( start >= at->nested->size ) end = start;
						k = at->variadic && end >= at->nested->size ? DAO_TYPE_VARIADIC : 0;
						k = (k<<16) | DAO_TUPLE;
						tp = at->nested->items.pType + start;
						ct = DaoNamespace_MakeType( NS, "tuple", k, NULL, tp, end-start );
					}
				}else if( value && value->type == DAO_NONE ){
					ct = at;
				}else if( value && value->type ){
					if( value->type > DAO_DOUBLE ) goto InvIndex;
					k = DaoValue_GetInteger( value );
					if( k < 0 ) goto InvIndex; /* No support for negative index; */
					if( at->variadic && k >= (at->nested->size - 1) ){
						type = (DaoType*) at->nested->items.pType[at->nested->size-1]->aux;
						ct = type ? type : dao_type_any;
					}else{
						if( k >= at->nested->size ) goto InvIndex;
						ct = at->nested->items.pType[ k ];
						if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
						DaoInferencer_UpdateType( self, opc, ct );
						if( typed_code && ct == types[opc] && k <= 0xffff ){
							vmc->b = k;
							if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_COMPLEX ){
								vmc->code = DVM_GETF_TI + ( ct->tid - DAO_INTEGER );
							}else{
								/* for skipping type checking */
								vmc->code = DVM_GETF_TX;
							}
						}
					}
				}else if( bt == dao_type_for_iterator ){
					int j;
					if( at->nested->size == 0 ) goto InvIndex;
					ct = at->nested->items.pType[0];
					for(j=1; j<at->nested->size; ++j){
						DaoType *it = at->nested->items.pType[j];
						if( it->tid >= DAO_PAR_NAMED && it->tid <= DAO_PAR_VALIST ){
							it = (DaoType*) it->aux;
						}
						if( DaoType_MatchTo( it, ct, defs ) < DAO_MT_EQ ){
							ct = dao_type_any;
							break;
						}
					}
				}else if( bt->realnum ){
					if( typed_code && code == DVM_GETI ){
						vmc->code = DVM_GETI_TI;
						if( bt->tid != DAO_INTEGER )
							DaoInferencer_InsertMove( self, inode, & inode->b, bt, dao_type_int );
					}
				}else if( bt->tid == DAO_TUPLE && bt->nested->size == 2 ){
					ct = dao_type_tuple;
				}else if( bt->tid != DAO_UDT && bt->tid != DAO_ANY ){
					goto InvIndex;
				}
			}else if( at->tid == DAO_OBJECT && (meth = DaoClass_FindOperator( & at->aux->xClass, "[]", hostClass )) ){
				rout = DaoValue_Check( meth, at, & bt, 1, DVM_CALL, errors );
				if( rout == NULL ) goto InvIndex;
				ct = & rout->routType->aux->xType;
			}else if( at->tid == DAO_CDATA || at->tid == DAO_CSTRUCT ){
				DString_SetMBS( mbs, "[]" );
				meth = DaoType_FindFunction( at, mbs );
				if( meth == NULL ) goto WrongContainer;
				rout = DaoValue_Check( meth, at, & bt, 1, DVM_CALL, errors );
				if( rout == NULL ) goto InvIndex;
				ct = & rout->routType->aux->xType;
			}else if( at->tid == DAO_INTERFACE ){
				DString_SetMBS( mbs, "[]" );
				node = DMap_Find( at->aux->xInterface.methods, mbs );
				if( node == NULL ) goto WrongContainer;
				meth = node->value.pRoutine;
				rout = DaoValue_Check( meth, at, & bt, 1, DVM_CALL, errors );
				if( rout == NULL ) goto InvIndex;
				ct = & rout->routType->aux->xType;
			}else if( at->tid & DAO_ANY ){
				ct = dao_type_udf;
			}else if( at->typer ){
				/* Use at->typer instead of at->kernel, because at->kernel may still be NULL,
				 * if the type is created before the setup of the typer structure. */
				DString_SetMBS( mbs, "[]" );
				meth = DaoType_FindFunction( at, mbs );
				if( meth == NULL ) goto WrongContainer;
				rout = DaoValue_Check( meth, at, & bt, 1, DVM_CALL, errors );
				if( rout == NULL ) goto InvIndex;
				ct = & rout->routType->aux->xType;
			}else{
				goto WrongContainer;
			}
			if( ct == NULL ) goto InvKey;
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			break;
		case DVM_GETMI :
			{
				value = consts[opa] ? dao_none_value : NULL;
				ct = at;
				meth = NULL;
				DString_SetMBS( mbs, "[]" );
				if( opb == 0 ){
					ct = at;
				}else if( NoCheckingType( at ) ){
					/* allow less strict typing: */
					ct = dao_type_udf;
				}else if( at->tid == DAO_ARRAY ){
					int max = DAO_NONE, min = DAO_COMPLEX;
					ct = type = at->nested->items.pType[0];
					for(j=1; j<=opb; j++){
						int tid = types[j+opa]->tid;
						if( tid == DAO_VALTYPE ) tid = types[j+opa]->aux->type;
						if( tid > max ) max = tid;
						if( tid < min ) min = tid;
					}
					if( min == DAO_NONE || max > DAO_DOUBLE ) ct = at;
					DaoInferencer_UpdateType( self, opc, ct );
					ct = types[opc];
					if( type->tid && type->tid <= DAO_COMPLEX && ct->tid && ct->tid <= DAO_COMPLEX ){
						if( min >= DAO_INTEGER && max <= DAO_DOUBLE ){
							inode->code = DVM_GETMI_AII + (type->tid - DAO_INTEGER);
							if( max > DAO_INTEGER ){
								inode2 = DaoInferencer_InsertNode( self, inode, DVM_MOVE_PP, 1, at );
								inode2->a = inode->a;
								inode->a = self->types->size - 1;
								for(j=1; j<=opb; j++){
									unsigned short op = j+opa;
									DaoInferencer_InsertMove( self, inode, & op, types[j+opa], dao_type_int );
								}
							}
							if( type->tid != ct->tid ) DaoInferencer_InsertMove2( self, inode, type, ct );
						}
					}
				}else if( at->tid == DAO_MAP ){
					DaoType *t0 = at->nested->items.pType[0];
					int check1 = NoCheckingType( types[opa+1] ) == 0;
					int check2 = NoCheckingType( types[opa+2] ) == 0;
					if( types[opa+1]->tid == DAO_VALTYPE ) check1 = types[opa+1]->aux->type;
					if( types[opa+2]->tid == DAO_VALTYPE ) check2 = types[opa+2]->aux->type;
					if( check1 && DaoType_MatchTo( types[opa+1], t0, defs ) ==0 ) goto InvKey;
					if( check2 && DaoType_MatchTo( types[opa+2], t0, defs ) ==0 ) goto InvKey;
				}else if( at->tid == DAO_CLASS || at->tid == DAO_OBJECT ){
					meth = DaoClass_FindOperator( & at->aux->xClass, "[]", hostClass );
					if( meth == NULL ) goto WrongContainer;
				}else if( at->tid == DAO_CDATA || at->tid == DAO_CSTRUCT ){
					meth = DaoType_FindFunction( at, mbs );
					if( meth == NULL ) goto WrongContainer;
				}else if( at->tid == DAO_INTERFACE ){
					node = DMap_Find( at->aux->xInterface.methods, mbs );
					if( node == NULL ) goto WrongContainer;
					meth = node->value.pRoutine;
				}else if( at->typer ){
					meth = DaoType_FindFunction( at, mbs );
					if( meth == NULL ) goto WrongContainer;
				}
				if( meth ){
					/* TODO, self type for class? */
					rout = DaoValue_Check( meth, at, types+opa+1, opb, DVM_CALL, errors );
					if( rout == NULL ) goto InvIndex;
					ct = & rout->routType->aux->xType;
				}
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_GETF :
			{
				DaoType **pars = NULL;
				int npar = 0;
				int ak = 0;
				ct = NULL;
				value = routConsts->items.pValue[opb];
				if( value == NULL || value->type != DAO_STRING ) goto NotMatch;
				str = value->xString.data;
				ak = at->tid == DAO_CLASS;
				self->type_source = at;
				if( NoCheckingType( at ) ){
					/* allow less strict typing: */
					ct = dao_type_udf;
				}else if( at->tid == DAO_COMPLEX ){
					if( strcmp( str->mbs, "real" ) && strcmp( str->mbs, "imag" ) ) goto NotExist_TryAux;
					ct = DaoInferencer_UpdateType( self, opc, dao_type_double );
					if( ct->realnum ){
						inode->code = DVM_GETF_CX;
						inode->b = strcmp( str->mbs, "imag" ) == 0;
						if( ct->tid != DAO_DOUBLE )
							DaoInferencer_InsertMove2( self, inode, dao_type_double, ct );
					}
				}else if( at->tid == DAO_TYPE ){
					at = at->nested->items.pType[0];
					self->type_source = at;
					if( at->tid == DAO_ENUM && at->mapNames ){
						if( DMap_Find( at->mapNames, str ) == NULL ) goto NotExist_TryAux;
						ct = at;
					}else{
						goto NotExist_TryAux;
					}
				}else if( at->tid == DAO_INTERFACE ){
					node = DMap_Find( at->aux->xInterface.methods, str );
					if( node ){
						ct = node->value.pRoutine->routType;
					}else{
						DString_SetMBS( mbs, "." );
						DString_Append( mbs, str );
						node = DMap_Find( at->aux->xInterface.methods, mbs );
						if( node == NULL ){
							pars = & dao_type_string;
							npar = 1;
							DString_SetMBS( mbs, "." );
							node = DMap_Find( at->aux->xInterface.methods, mbs );
						}
						if( node == NULL ) goto NotExist_TryAux;
						meth = node->value.pRoutine;
						rout = DaoValue_Check( meth, at, pars, npar, DVM_CALL, errors );
						if( rout == NULL ) goto NotExist_TryAux;
						ct = & rout->routType->aux->xType;
					}
				}else if( at->tid == DAO_CLASS || at->tid == DAO_OBJECT ){
					int j, getter = 0;
					klass = & at->aux->xClass;
					type2 = DaoClass_GetDataType( klass, str, & j, hostClass );
					if( j ){
						value = NULL;
						DString_SetMBS( mbs, "." );
						DString_Append( mbs, str );
						DaoClass_GetDataType( klass, mbs, & j, hostClass );
						DaoClass_GetData( klass, mbs, & value, hostClass );
						if( j == DAO_ERROR_FIELD_NOTEXIST ){
							pars = & dao_type_string;
							npar = 1;
							value = NULL;
							DString_SetMBS( mbs, "." );
							DaoClass_GetDataType( klass, mbs, & j, hostClass );
							DaoClass_GetData( klass, mbs, & value, hostClass );
						}
						if( j == 0 && value && value->type == DAO_ROUTINE ){
							rout2 = rout = (DaoRoutine*) value;
							rout = DaoValue_Check( rout, at, pars, npar, DVM_CALL, errors );
							if( rout == NULL ) goto NotMatch;
							ct = & rout->routType->aux->xType;
							getter = 1;
							DaoInferencer_UpdateType( self, opc, ct );
							AssertTypeMatching( ct, types[opc], defs );
						}
					}
					DString_Assign( mbs, at->name );
					DString_AppendChar( mbs, '.' );
					DString_Append( mbs, str );
					if( j == DAO_ERROR_FIELD_NOTPERMIT ) goto NotPermit;
					if( j == DAO_ERROR_FIELD_NOTEXIST ) goto NotExist_TryAux;
					j = DaoClass_GetDataIndex( klass, str );
					k = LOOKUP_ST( j );
					if( k == DAO_OBJECT_VARIABLE && at->tid ==DAO_CLASS ) goto NeedInstVar;
					if( k == DAO_CLASS_VARIABLE ) consts[opc] = NULL;
					if( getter ) break;
					if( type2 == NULL ){
						DaoClass_GetData( klass, str, & value, hostClass );
						ct = DaoNamespace_GetType( NS, value );
						GC_ShiftRC( value, consts[opc] );
						consts[opc] = value;
					}else{
						ct = *type2;
					}
					j = DaoClass_GetDataIndex( klass, str );
					if( typed_code ){
						int code = vmc->code;
						/* specialize instructions for finalized class/instance: */
						k = LOOKUP_ST( j );
						vmc->b = LOOKUP_ID( j );
						if( ct && ct->tid >= DAO_INTEGER && ct->tid <= DAO_COMPLEX ){
							switch( k ){
							case DAO_CLASS_CONSTANT : code = ak ? DVM_GETF_KCI : DVM_GETF_OCI; break;
							case DAO_CLASS_VARIABLE : code = ak ? DVM_GETF_KGI : DVM_GETF_OGI; break;
							case DAO_OBJECT_VARIABLE : code = DVM_GETF_OVI; break;
							}
							code += ct->tid - DAO_INTEGER;
						}else{
							switch( k ){
							case DAO_CLASS_CONSTANT : code = ak ? DVM_GETF_KC : DVM_GETF_OC; break;
							case DAO_CLASS_VARIABLE : code = ak ? DVM_GETF_KG : DVM_GETF_OG; break;
							case DAO_OBJECT_VARIABLE : code = DVM_GETF_OV; break;
							}
						}
						vmc->code = code;
					}
				}else if( at->tid == DAO_TUPLE ){
					if( at->mapNames == NULL ) goto NotExist_TryAux;
					node = MAP_Find( at->mapNames, str );
					if( node == NULL ) goto NotExist_TryAux;
					k = node->value.pInt;
					if( k <0 || k >= (int)at->nested->size ) goto NotExist_TryAux;
					ct = at->nested->items.pType[ k ];
					if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
					DaoInferencer_UpdateType( self, opc, ct );
					if( typed_code && notide && ct == types[opc] && k < 0xffff ){
						if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_COMPLEX ){
							vmc->code = DVM_GETF_TI + ( ct->tid - DAO_INTEGER );
							vmc->b = k;
						}else{
							/* for skipping type checking */
							vmc->code = DVM_GETF_TX;
							vmc->b = k;
						}
					}
				}else if( at->tid == DAO_NAMESPACE ){
					ct = dao_type_udf;
					if( consts[opa] && consts[opa]->type == DAO_NAMESPACE ){
						DaoNamespace *ans = & consts[opa]->xNamespace;
						k = DaoNamespace_FindVariable( ans, str );
						if( k >=0 ){
							ct = DaoNamespace_GetVariableType( ans, k );
						}else{
							k = DaoNamespace_FindConst( ans, str );
							value = DaoNamespace_GetConst( ans, k );
							if( value ) ct = DaoNamespace_GetType( ans, value );
						}
						if( k <0 ) goto NotExist_TryAux;
					}
			}else if( at->typer ){
				value = DaoType_FindValue( at, str );
				if( value && value->type == DAO_ROUTINE ){
					DaoRoutine *func = (DaoRoutine*) value;
					ct = func->routType;
					GC_ShiftRC( value, consts[opc] );
					consts[opc] = value;
				}else if( value ){
					ct = DaoNamespace_GetType( NS, value );
					GC_ShiftRC( value, consts[opc] );
					consts[opc] = value;
				}else{
					DString_SetMBS( mbs, "." );
					DString_Append( mbs, str );
					meth = DaoType_FindFunction( at, mbs );
					if( meth == NULL ){
						pars = & dao_type_string;
						npar = 1;
						DString_SetMBS( mbs, "." );
						meth = DaoType_FindFunction( at, mbs );
					}
					if( meth == NULL ) goto NotExist_TryAux;
					rout = DaoValue_Check( meth, at, pars, npar, DVM_CALL, errors );
					if( rout == NULL ) goto NotMatch;
					ct = & rout->routType->aux->xType;
				}
				if( ct == NULL ) ct = dao_type_udf;
			}
			if( ct && ct->tid == DAO_ROUTINE && (ct->attrib & DAO_TYPE_SELF) ){
				DaoType *selftype = (DaoType*) ct->nested->items.pType[0]->aux;
				/* Remove type holder bindings for the self parameter: */
				DaoType_ResetTypeHolders( selftype, defs );
				DaoType_MatchTo( at, selftype, defs );
				//ct = DaoType_DefineTypes( ct, NS, defs );
			}
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			break;
NotExist_TryAux:
			value = DaoType_FindAuxMethod( at, str, NS );
			if( value == NULL ) goto NotExist;
			ct = DaoNamespace_GetType( NS, value );
			GC_ShiftRC( value, consts[opc] );
			consts[opc] = value;
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			break;
			}
		case DVM_SETI :
		case DVM_SETDI :
			{
				if( ct == NULL ) goto ErrorTyping;
				integer.value = opb;
				value = (DaoValue*)(DaoInteger*)&integer;
				bt = dao_type_int;
				if( code == DVM_SETI ){
					bt = types[opb];
					value = consts[opb];
				}
				if( NoCheckingType(at) || NoCheckingType(bt) || NoCheckingType(ct) ) break;
				switch( ct->tid ){
				case DAO_STRING :
					if( typed_code && notide && code == DVM_SETI ){
						if( at->realnum && bt->realnum ){
							vmc->code = DVM_SETI_SII;
							if( at->tid != DAO_INTEGER )
								DaoInferencer_InsertMove( self, inode, & inode->a, at, dao_type_int );
							if( bt->tid != DAO_INTEGER )
								DaoInferencer_InsertMove( self, inode, & inode->b, bt, dao_type_int );
						}
					}
					/* less strict checking */
					if( at->tid >= DAO_ARRAY && at->tid != DAO_ANY ) goto NotMatch;

					if( bt->tid == DAO_TUPLE && bt->nested->size == 2
							&& (at->tid == DAO_STRING || at->tid <= DAO_DOUBLE) ){
						/* passed */
						AssertPairNumberType( bt );
					}else if( bt->tid == DAO_LIST && at->tid <= DAO_DOUBLE ){
						/* passed */
					}else if( bt->tid > DAO_DOUBLE && bt->tid != DAO_ANY ){
						/* less strict checking */
						goto NotMatch;
					}
					break;
				case DAO_LONG :
					ct = dao_type_int; /* XXX slicing */
					break;
				case DAO_LIST :
					type = ct->nested->items.pType[0];
					if( bt->tid >= DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
						ct = ct->nested->items.pType[0];
						AssertTypeMatching( at, ct, defs );
						if( typed_code && notide && code == DVM_SETI ){
							if( ct->tid && ct->tid <= DAO_COMPLEX && at->tid && at->tid <= DAO_COMPLEX ){
								if( at->tid != ct->tid )
									DaoInferencer_InsertMove( self, inode, & inode->a, at, ct );
								vmc->code = DVM_SETI_LIII + ct->tid - DAO_INTEGER;
							}else if( at->tid == DAO_STRING && ct->tid == DAO_STRING ){
								vmc->code = DVM_SETI_LSIS;
							}else{
								if( at == ct || ct->tid == DAO_ANY ) vmc->code = DVM_SETI_LI;
							}
							if( bt->tid != DAO_INTEGER )
								DaoInferencer_InsertMove( self, inode, & inode->b, bt, dao_type_int );
						}
					}else if( bt->tid == DAO_TUPLE && bt->nested->size ==2 ){
						AssertPairNumberType( bt );
						AssertTypeMatching( at, type, defs );
					}else if( bt->tid == DAO_LIST || bt->tid == DAO_ARRAY ){
						tt = bt->nested->items.pType[0];
						if( tt->tid != DAO_INTEGER && tt->tid != DAO_ANY && tt->tid != DAO_UDT )
							return DaoInferencer_ErrorTypeNotMatching( self, tt, dao_type_int );
						AssertTypeMatching( at, type, defs );
					}else{
						return DaoInferencer_ErrorTypeNotMatching( self, bt, dao_type_int );
					}
					break;
				case DAO_MAP :
					{
						DaoType *t0 = ct->nested->items.pType[0];
						DaoType *t1 = ct->nested->items.pType[1];
						AssertTypeMatching( bt, t0, defs );
						AssertTypeMatching( at, t1, defs );
						break;
					}
				case DAO_ARRAY :
					if( bt->tid >= DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
						if( DaoType_MatchTo( at, ct, defs ) ) break;
						ct = ct->nested->items.pType[0];
						/* array[i] */
						if( typed_code && notide && code == DVM_SETI ){
							if( ct->realnum && at->realnum ){
								if( at->tid != ct->tid )
									DaoInferencer_InsertMove( self, inode, & inode->a, at, ct );
								vmc->code = DVM_SETI_AIII + ct->tid - DAO_INTEGER;
							}else if( ct->tid == DAO_COMPLEX && at->tid && at->tid <= DAO_COMPLEX ){
								if( at->tid != DAO_COMPLEX )
									DaoInferencer_InsertMove( self, inode, & inode->a, at, ct );
								vmc->code = DVM_SETI_ACIC;
							}else if( at->tid != DAO_UDT && at->tid != DAO_ANY ){
								AssertTypeMatching( at, ct, defs );
							}
							if( bt->tid != DAO_INTEGER )
								DaoInferencer_InsertMove( self, inode, & inode->b, bt, dao_type_int );
						}
						AssertTypeMatching( at, ct, defs );
					}else if( bt->tid == DAO_LIST || bt->tid == DAO_ARRAY ){
						k = bt->nested->items.pType[0]->tid;
						if( k >=DAO_DOUBLE && k !=DAO_ANY ) goto NotMatch;
						/* imprecise checking */
						if( DaoType_MatchTo( at, ct->nested->items.pType[0], defs )==0
								&& DaoType_MatchTo( at, ct, defs )==0 )
							goto NotMatch;
					}
					break;
				case DAO_TUPLE :
					if( value && value->type ){
						if( value->type > DAO_DOUBLE ) goto InvIndex;
						k = DaoValue_GetInteger( value );
						if( k < 0 ) goto InvIndex;
						if( ct->variadic == 0 && k >= (int)ct->nested->size ) goto InvIndex;
						if( ct->variadic && k >= (ct->nested->size - 1) ){
							ct = ct->nested->items.pType[ ct->nested->size - 1 ];
							ct = ct->aux ? (DaoType*) ct->aux : dao_type_any;
						}else{
							ct = ct->nested->items.pType[ k ];
						}
						if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
						AssertTypeMatching( at, ct, defs );
						if( typed_code && k <= 0xffff ){
							if( ct->tid && ct->tid <= DAO_COMPLEX && at->tid && at->tid <= DAO_COMPLEX ){
								vmc->b = k;
								if( at->tid != ct->tid )
									DaoInferencer_InsertMove( self, inode, & inode->a, at, ct );
								vmc->code = DVM_SETF_TII + ct->tid - DAO_INTEGER;
							}else if( at == ct || ct->tid == DAO_ANY ){
								vmc->b = k;
								if( at->tid ==DAO_STRING && ct->tid ==DAO_STRING ){
									vmc->code = DVM_SETF_TSS;
								}else if( at->tid >= DAO_ARRAY && at->tid <= DAO_TYPE && consts[opa] == NULL ){
									vmc->code = DVM_SETF_TPP;
								}else{
									vmc->code = DVM_SETF_TXX;
								}
							}
						}
					}else if( bt->realnum ){
						if( typed_code ){
							vmc->code = DVM_SETI_TI;
							if( bt->tid != DAO_INTEGER )
								DaoInferencer_InsertMove( self, inode, & inode->b, bt, dao_type_int );
						}
					}else if( bt->tid != DAO_UDT && bt->tid != DAO_ANY ){
						goto InvIndex;
					}
					break;
				case DAO_CLASS :
				case DAO_OBJECT :
					if( (meth=DaoClass_FindOperator( & ct->aux->xClass, "[]=", hostClass )) == NULL)
						goto InvIndex;
					ts[0] = at;
					ts[1] = bt;
					k = 2;
					if( bt->tid == DAO_TUPLE ){
						if( bt->nested->size + 2 > DAO_MAX_PARAM ) goto InvIndex;
						ts[0] = at;
						for(k=0,K=bt->nested->size; k<K; k++) ts[k+1] = bt->nested->items.pType[k];
						k = bt->nested->size + 1;
					}
					rout = DaoValue_Check( meth, ct, ts, k, DVM_CALL, errors );
					if( rout == NULL ) goto InvIndex;
					break;
				case DAO_CDATA :
				case DAO_CSTRUCT :
					DString_SetMBS( mbs, "[]=" );
					meth = DaoType_FindFunction( ct, mbs );
					if( meth == NULL ) goto InvIndex;
					ts[0] = at;
					ts[1] = bt;
					k = 2;
					if( bt->tid == DAO_TUPLE ){
						if( bt->nested->size + 2 > DAO_MAX_PARAM ) goto InvIndex;
						ts[0] = at;
						for(k=0,K=bt->nested->size; k<K; k++) ts[k+1] = bt->nested->items.pType[k];
						k = bt->nested->size + 1;
					}
					rout = DaoValue_Check( meth, ct, ts, k, DVM_CALL, errors );
					if( rout == NULL ) goto InvIndex;
					break;
				default : break;
				}
				break;
			}
		case DVM_SETMI :
			{
				if( ct == NULL ) goto ErrorTyping;
				if( NoCheckingType( at ) || NoCheckingType( ct ) ) break;
				meth = NULL;
				DString_SetMBS( mbs, "[]=" );
				if( ct->tid == DAO_ARRAY ){
					int max = DAO_NONE, min = DAO_COMPLEX;
					type = ct->nested->items.pType[0];
					for(j=1; j<=opb; j++){
						int tid = types[j+opc]->tid;
						if( tid == DAO_VALTYPE ) tid = types[j+opc]->aux->type;
						if( tid > max ) max = tid;
						if( tid < min ) min = tid;
					}
					if( at->tid == DAO_NONE || (at->tid & DAO_ANY) || (type->tid & DAO_ANY) ) break;
					if( type->tid <= DAO_COMPLEX && at->tid <= DAO_COMPLEX ){
						if( at->tid == DAO_COMPLEX &&  type->tid != DAO_COMPLEX ) goto ErrorTyping;
						if( min >= DAO_INTEGER && max <= DAO_DOUBLE ){
							inode->code = DVM_SETMI_AIII + (type->tid - DAO_INTEGER);
							if( at->tid != type->tid )
								DaoInferencer_InsertMove( self, inode, & inode->a, at, type );
							if( max > DAO_INTEGER ){
								inode2 = DaoInferencer_InsertNode( self, inode, DVM_MOVE_PP, 1, ct );
								inode2->c = inode->c;
								inode->c = self->types->size - 1;
								for(j=1; j<=opb; j++){
									unsigned short op = j+opc;
									DaoInferencer_InsertMove( self, inode, & op, types[j+opc], dao_type_int );
								}
							}
						}
					}
				}else if( at->tid == DAO_MAP ){
					DaoType *t0 = at->nested->items.pType[0];
					DaoType *t1 = at->nested->items.pType[1];
					int check1 = NoCheckingType( types[opc+1] ) == 0;
					int check2 = NoCheckingType( types[opc+2] ) == 0;
					if( types[opc+1]->tid == DAO_VALTYPE ) check1 = types[opc+1]->aux->type;
					if( types[opc+2]->tid == DAO_VALTYPE ) check2 = types[opc+2]->aux->type;
					if( check1 && DaoType_MatchTo( types[opc+1], t0, defs ) ==0 ) goto InvKey;
					if( check2 && DaoType_MatchTo( types[opc+2], t0, defs ) ==0 ) goto InvKey;
					AssertTypeMatching( at, t1, defs );
				}else if( ct->tid == DAO_CLASS || ct->tid == DAO_OBJECT ){
					meth = DaoClass_FindOperator( & ct->aux->xClass, "[]=", hostClass );
					if( meth == NULL ) goto WrongContainer;
				}else if( ct->tid == DAO_CDATA || ct->tid == DAO_CSTRUCT ){
					meth = DaoType_FindFunction( ct, mbs );
					if( meth == NULL ) goto WrongContainer;
				}else if( ct->tid == DAO_INTERFACE ){
					node = DMap_Find( ct->aux->xInterface.methods, mbs );
					if( node == NULL ) goto WrongContainer;
					meth = node->value.pRoutine;
				}else if( ct->typer ){
					meth = DaoType_FindFunction( ct, mbs );
					if( meth == NULL ) goto WrongContainer;
				}
				if( meth ){
					ts[0] = at;
					memcpy( ts + 1, types+opc+1, opb*sizeof(DaoType*) );
					rout = DaoValue_Check( meth, ct, ts, opb+1, DVM_CALL, errors );
					if( rout == NULL ) goto InvIndex;
				}
				break;
			}
		case DVM_SETF :
			{
				DaoType *pars[2] = { NULL, NULL };
				int j, setter = 0;
				int npar = 1;
				int ck = 0;
#if 0
				printf( "a: %s\n", types[opa]->name->mbs );
				printf( "c: %s\n", types[opc]->name->mbs );
#endif
				pars[0] = pars[1] = types[opa];
				value = routConsts->items.pValue[opb];
				if( value == NULL || value->type != DAO_STRING ) goto NotMatch;
				self->type_source = ct;
				str = value->xString.data;
				switch( ct->tid ){
				case DAO_UDT :
				case DAO_ANY :
				case DAO_THT :
					/* allow less strict typing: */
					break;
				case DAO_COMPLEX :
					if( strcmp( str->mbs, "real" ) && strcmp( str->mbs, "imag" ) ) goto NotExist;
					if( at->realnum == 0 && !(at->tid & DAO_ANY) ) goto NotMatch;
					if( at->tid & DAO_ANY ) break;
					if( at->tid != DAO_DOUBLE )
						DaoInferencer_InsertMove( self, inode, & inode->a, at, dao_type_double );
					inode->code = DVM_SETF_CX;
					inode->b = strcmp( str->mbs, "imag" ) == 0;
					break;
				case DAO_INTERFACE :
					node = DMap_Find( at->aux->xInterface.methods, str );
					if( node ){
						ct = node->value.pRoutine->routType;
					}else{
						DString_SetMBS( mbs, "." );
						DString_Append( mbs, str );
						node = DMap_Find( at->aux->xInterface.methods, mbs );
						if( node == NULL ){
							pars[0] = dao_type_string;
							npar = 2;
							DString_SetMBS( mbs, "." );
							node = DMap_Find( at->aux->xInterface.methods, mbs );
						}
						if( node == NULL ) goto NotExist_TryAux;
						meth = node->value.pRoutine;
						rout = DaoValue_Check( meth, at, pars, npar, DVM_CALL, errors );
						if( rout == NULL ) goto NotExist_TryAux;
						ct = & rout->routType->aux->xType;
					}
					break;
				case DAO_CLASS :
				case DAO_OBJECT :
					ck = ct->tid ==DAO_CLASS;
					klass = & types[opc]->aux->xClass;
					type2 = DaoClass_GetDataType( klass, str, & j, hostClass );
					if( STRCMP( str, "self" ) ==0 ) goto NotPermit;
					if( j ){
						value = NULL;
						DString_SetMBS( mbs, "." );
						DString_Append( mbs, str );
						DString_AppendMBS( mbs, "=" );
						DaoClass_GetDataType( klass, mbs, & j, hostClass );
						DaoClass_GetData( klass, mbs, & value, hostClass );
						if( j == DAO_ERROR_FIELD_NOTEXIST ){
							pars[0] = dao_type_string;
							npar = 2;
							value = NULL;
							DString_SetMBS( mbs, ".=" );
							DaoClass_GetDataType( klass, mbs, & j, hostClass );
							DaoClass_GetData( klass, mbs, & value, hostClass );
						}
						if( j == 0 && value && value->type == DAO_ROUTINE ){
							meth = (DaoRoutine*) value;
							setter = 1;
							rout = DaoValue_Check( meth, ct, pars, npar, DVM_CALL, errors );
							if( rout == NULL ) goto NotMatch;
						}
					}
					if( j == DAO_ERROR_FIELD_NOTPERMIT ) goto NotPermit;
					if( j == DAO_ERROR_FIELD_NOTEXIST ) goto NotExist;
					j = DaoClass_GetDataIndex( klass, str );
					k = LOOKUP_ST( j );
					if( k == DAO_CLASS_CONSTANT ) goto InvOper;
					if( k == DAO_OBJECT_VARIABLE && ct->tid ==DAO_CLASS ) goto NeedInstVar;
					if( setter ) break;
					if( type2 == NULL ) goto NotPermit;
					if( *type2 == NULL || (*type2)->tid == DAO_UDT ){
						GC_ShiftRC( types[opa], *type2 );
						*type2 = types[opa];
					}
					AssertTypeMatching( types[opa], *type2, defs );
					j = DaoClass_GetDataIndex( klass, str );
					if( typed_code ){
						k = LOOKUP_ST( j );
						if( *type2 && (*type2)->realnum && at->realnum ){
							vmc->code = ck ? DVM_SETF_KGII : DVM_SETF_OGII;
							if( k == DAO_OBJECT_VARIABLE ) vmc->code = DVM_SETF_OVII;
							if( at->tid != (*type2)->tid )
								DaoInferencer_InsertMove( self, inode, & inode->a, at, *type2 );
							vmc->code += at->tid - DAO_INTEGER;
							vmc->b = LOOKUP_ID( j );
						}else if( *type2 && (*type2)->tid == DAO_COMPLEX && at->tid && at->tid <= DAO_COMPLEX ){
							vmc->b = LOOKUP_ID( j );
							vmc->code = ck ? DVM_SETF_KGCC : DVM_SETF_OGCC;
							if( k == DAO_OBJECT_VARIABLE ) vmc->code = DVM_SETF_OVCC;
							if( at->tid != (*type2)->tid )
								DaoInferencer_InsertMove( self, inode, & inode->a, at, *type2 );
						}else if( at == *type2 || (*type2)->tid == DAO_ANY ){
							vmc->b = LOOKUP_ID( j );
							vmc->code = ck ? DVM_SETF_KG : DVM_SETF_OG;
							if( k == DAO_OBJECT_VARIABLE ) vmc->code = DVM_SETF_OV;
						}
					}
					break;
				case DAO_TUPLE :
					{
						if( ct->mapNames == NULL ) goto NotExist;
						node = MAP_Find( ct->mapNames, str );
						if( node == NULL ) goto NotExist;
						k = node->value.pInt;
						if( k <0 || k >= (int)ct->nested->size ) goto InvIndex;
						ct = ct->nested->items.pType[ k ];
						if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
						AssertTypeMatching( at, ct, defs );
						if( typed_code && k < 0xffff ){
							if( ct->tid && ct->tid <= DAO_COMPLEX && at->tid && at->tid <= DAO_COMPLEX ){
								if( at->tid != ct->tid )
									DaoInferencer_InsertMove( self, inode, & inode->a, at, ct );
								vmc->code = DVM_SETF_TII + ct->tid - DAO_INTEGER;
								vmc->b = k;
							}else if( at->tid == DAO_STRING && ct->tid == DAO_STRING ){
								vmc->code = DVM_SETF_TSS;
								vmc->b = k;
							}else if( at == ct || ct->tid == DAO_ANY ){
								vmc->b = k;
								if( at->tid >= DAO_ARRAY && at->tid <= DAO_TYPE && consts[opa] == NULL ){
									vmc->code = DVM_SETF_TPP;
								}else{
									vmc->code = DVM_SETF_TXX;
								}
							}
						}
						break;
					}
				case DAO_NAMESPACE :
					{
						if( consts[opc] && consts[opc]->type == DAO_NAMESPACE ){
							DaoNamespace *ans = & consts[opc]->xNamespace;
							k = DaoNamespace_FindVariable( ans, str );
							if( k >=0 ){
								ct = DaoNamespace_GetVariableType( ans, k );
							}else{
								k = DaoNamespace_FindConst( ans, str );
								value = DaoNamespace_GetConst( ans, k );
								if( value ) ct = DaoNamespace_GetType( ans, value );
							}
							if( k <0 ) goto NotExist;
							AssertTypeMatching( at, ct, defs );
						}
						break;
					}
				case DAO_CDATA :
				case DAO_CSTRUCT :
					{
						DString_SetMBS( mbs, "." );
						DString_Append( mbs, str );
						DString_AppendMBS( mbs, "=" );
						meth = DaoType_FindFunction( ct, mbs );
						if( meth == NULL ){
							pars[0] = dao_type_string;
							npar = 2;
							DString_SetMBS( mbs, ".=" );
							meth = DaoType_FindFunction( ct, mbs );
						}
						if( meth == NULL ) goto NotMatch;
						rout = DaoValue_Check( meth, ct, pars, npar, DVM_CALL, errors );
						if( rout == NULL ) goto NotMatch;
						break;
					}
				default: goto InvOper;
				}
				break;
			}
		case DVM_CAST :
			if( routConsts->items.pValue[opb]->type != DAO_TYPE ) goto ErrorTyping;
			at = (DaoType*) routConsts->items.pValue[opb];
			DaoInferencer_UpdateType( self, opc, at );
			AssertTypeMatching( at, types[opc], defs );
			at = types[opa];
			ct = types[opc];
			if( typed_code ){
				if( at->realnum && ct->realnum )
					vmc->code = DVM_MOVE_II + 3*(ct->tid - DAO_INTEGER) + at->tid - DAO_INTEGER;
				else if( at->tid == DAO_COMPLEX && ct->tid == DAO_COMPLEX )
					vmc->code = DVM_MOVE_CC;
			}
			break;
		case DVM_LOAD :
			DaoInferencer_UpdateType( self, opc, at );
			AssertTypeMatching( at, types[opc], defs );
			if( at == types[opc] && at->tid >= DAO_ARRAY && at->tid <= DAO_TYPE && consts[opa] == NULL ){
				vmc->code = DVM_MOVE_PP;
			}
			break;
		case DVM_MOVE :
			{
				DaoInferencer_UpdateType( self, opc, at );
				k = DaoType_MatchTo( at, types[opc], defs );
				ct = types[opc];

				/*
				   DaoVmCodeX_Print( *vmc, NULL );
				   if( types[opa] ) printf( "a: %s\n", types[opa]->name->mbs );
				   if( types[opc] ) printf( "c: %s\n", types[opc]->name->mbs );
				   printf( "%i  %i\n", DAO_MT_SUB, k );
				 */

#if 0
				if( consts[opa] && consts[opa]->type == DAO_ROUTREE && types[opc] && types[opc]->tid ==DAO_ROUTINE){
					/* a : routine<a:number,...> = overloaded_function; */
					//XXX rout = DRoutines_GetByType( (DaoRoutine*)consts[opa].v.routine, types[opc] );
					//if( rout == NULL ) goto NotMatch;
				}
#endif

				if( at->tid == DAO_UDT || at->tid == DAO_ANY ){
					/* less strict checking */
				}else if( at != ct && (ct->tid == DAO_OBJECT || ct->tid == DAO_CDATA || ct->tid == DAO_CSTRUCT) ){
					if( ct->tid == DAO_OBJECT ){
						meth = DaoClass_FindOperator( & ct->aux->xClass, "=", hostClass );
					}else{
						meth = DaoType_FindFunctionMBS( ct, "=" );
					}
					if( meth ){
						rout = DaoValue_Check( meth, ct, & at, 1, DVM_CALL, errors );
						if( rout == NULL ) goto NotMatch;
					}else if( k ==0 ){
						return DaoInferencer_ErrorTypeNotMatching( self, at, types[opc] );
					}
				}else if( at->tid ==DAO_TUPLE && DaoType_MatchTo(types[opc], at, defs)){
					/* less strict checking */
				}else if( k ==0 ){
					return DaoInferencer_ErrorTypeNotMatching( self, at, types[opc] );
				}

				/* necessary, because the register may be associated with a constant.
				 * beware of control flow: */
				if( vmc->b ){
					GC_ShiftRC( consts[opa], consts[opc] );
					consts[opc] = consts[opa];
				}

				if( k == DAO_MT_SUB && at != ct ){
					/* L = { 1.5, 2.5 }; L = { 1, 2 }; L[0] = 3.5 */
					vmc->code = DVM_CAST;
					if( at->tid && at->tid <= DAO_COMPLEX && types[opc]->tid == DAO_COMPLEX )
						vmc->code = DVM_MOVE_CI + (at->tid - DAO_INTEGER);
					break;
				}
				if( vmc->b == 0 ){
					ct = DaoType_DefineTypes( types[opc], NS, defs );
					if( ct ) DaoInferencer_UpdateType( self, opc, ct );
				}
				ct = types[opc];
				if( typed_code ){
					if( at->realnum && ct->realnum ){
						vmc->code = DVM_MOVE_II + 3*(ct->tid - DAO_INTEGER) + at->tid - DAO_INTEGER;
					}else if( at->tid && at->tid <= DAO_COMPLEX && ct->tid == DAO_COMPLEX ){
						vmc->code = DVM_MOVE_CI + (at->tid - DAO_INTEGER);
					}else if( at->tid == DAO_STRING && ct->tid == DAO_STRING ){
						vmc->code = DVM_MOVE_SS;
					}else if( at == ct || ct->tid == DAO_ANY ){
						if( at->tid >= DAO_ARRAY && at->tid <= DAO_TYPE && consts[opa] == NULL ){
							vmc->code = DVM_MOVE_PP;
						}else{
							vmc->code = DVM_MOVE_XX;
						}
					}
				}
				break;
			}
		case DVM_ADD : case DVM_SUB : case DVM_MUL :
		case DVM_DIV : case DVM_MOD : case DVM_POW :
		case DVM_AND : case DVM_OR :
			{
				ct = NULL;
				/*
				   if( types[opa] ) printf( "a: %s\n", types[opa]->name->mbs );
				   if( types[opb] ) printf( "b: %s\n", types[opb]->name->mbs );
				   if( types[opc] ) printf( "c: %s\n", types[opc]->name->mbs );
				 */
				if( NoCheckingType( at ) || NoCheckingType( bt ) ){
					ct = dao_type_udf;
				}else if( at->tid == DAO_OBJECT || bt->tid == DAO_OBJECT
						|| at->tid == DAO_CDATA || bt->tid == DAO_CDATA
						|| at->tid == DAO_CSTRUCT || bt->tid == DAO_CSTRUCT
						|| at->tid == DAO_INTERFACE || bt->tid == DAO_INTERFACE ){
					ct = DaoCheckBinArith( routine, vmc, at, bt, types[opc], hostClass, mbs );
					if( ct == NULL ) goto InvOper;
				}else if( at->tid == bt->tid ){
					ct = at;
					switch( at->tid ){
					case DAO_INTEGER : case DAO_FLOAT : case DAO_DOUBLE :
					case DAO_LONG :
						break;
					case DAO_COMPLEX :
						if( code == DVM_MOD || code >= DVM_AND ) goto InvOper;
						break;
					case DAO_STRING :
						if( code != DVM_ADD ) goto InvOper;
						break;
					case DAO_ENUM :
						if( code != DVM_ADD && code != DVM_SUB ) goto InvOper;
						if( at->name->mbs[0] =='$' && bt->name->mbs[0] =='$' ){
							ct = NULL;
							if( code == DVM_ADD ){
								ct = DaoNamespace_SymbolTypeAdd( NS, at, bt, NULL );
							}else{
								ct = DaoNamespace_SymbolTypeSub( NS, at, bt, NULL );
							}
							if( ct == NULL ) goto InvOper;
						}else if( at->name->mbs[0] =='$' ){
							ct = bt;
						}
						break;
					case DAO_LIST :
						if( code != DVM_ADD ) goto InvOper;
						AssertTypeMatching( bt, at, defs );
						break;
					case DAO_ARRAY :
						if( code >= DVM_AND ) goto InvOper;
						break;
					default : goto InvOper;
					}
				}else if( at->realnum && bt->realnum ){
					ct = at->tid > bt->tid ? at : bt;
				}else if( at->tid != bt->tid && (code == DVM_AND || code == DVM_OR) ){
					goto InvOper;
				}else if( at->realnum && (bt->tid ==DAO_COMPLEX || bt->tid == DAO_LONG
							|| bt->tid ==DAO_ARRAY) ){
					ct = bt;
				}else if( (at->tid ==DAO_COMPLEX || at->tid == DAO_LONG || at->tid ==DAO_ARRAY)
						&& bt->realnum ){
					ct = at;
				}else if( at->tid ==DAO_STRING && bt->tid ==DAO_INTEGER && opa==opc  ){
					ct = at;
				}else if( ( at->tid ==DAO_COMPLEX && bt->tid ==DAO_ARRAY )
						|| ( at->tid ==DAO_ARRAY && bt->tid ==DAO_COMPLEX ) ){
					ct = catype;
				}else{
					goto InvOper;
				}
				DaoInferencer_UpdateType( self, opc, ct );
				/* allow less strict typing: */
				if( ct->tid == DAO_UDT || ct->tid == DAO_ANY ) continue;
				AssertTypeMatching( ct, types[opc], defs );
				ct = types[opc];
				if( typed_code && at->realnum && bt->realnum && ct->realnum ){
					DaoType *max = ct;
					if( at->tid > max->tid ) max = at;
					if( bt->tid > max->tid ) max = bt;
					if( at->tid != max->tid ){
						DaoInferencer_InsertMove( self, inode, & inode->a, at, max );
						if( opa == opb ) inode->b = inode->a;
					}
					if( opa != opb && bt->tid != max->tid )
						DaoInferencer_InsertMove( self, inode, & inode->b, bt, max );

					switch( max->tid ){
					case DAO_INTEGER : vmc->code += DVM_ADD_III - DVM_ADD; break;
					case DAO_FLOAT  : vmc->code += DVM_ADD_FFF - DVM_ADD; break;
					case DAO_DOUBLE : vmc->code += DVM_ADD_DDD - DVM_ADD; break;
					}
					if( max->tid != ct->tid ) DaoInferencer_InsertMove2( self, inode, max, ct );
				}else if( typed_code && ct->tid == DAO_COMPLEX && code <= DVM_DIV ){
					if( at->tid && at->tid <= DAO_COMPLEX && bt->tid && bt->tid <= DAO_COMPLEX ){
						if( at->tid != DAO_COMPLEX ){
							DaoInferencer_InsertMove( self, inode, & inode->a, at, dao_type_complex );
							if( opa == opb ) inode->b = inode->a;
						}
						if( opa != opb && bt->tid != DAO_COMPLEX )
							DaoInferencer_InsertMove( self, inode, & inode->b, bt, dao_type_complex );
						vmc->code += DVM_ADD_CCC - DVM_ADD;
					}
				}else if( typed_code && at->tid == bt->tid && ct->tid == at->tid ){
					if( ct->tid == DAO_STRING && code == DVM_ADD ) vmc->code = DVM_ADD_SSS;
				}
				break;
			}
		case DVM_LT : case DVM_LE :
		case DVM_EQ : case DVM_NE :
			{
				ct = dao_type_int;
				if( NoCheckingType( at ) || NoCheckingType( bt ) ){
					ct = dao_type_udf;
				}else if( at->tid == DAO_OBJECT || bt->tid == DAO_OBJECT
						|| at->tid == DAO_CDATA || bt->tid == DAO_CDATA
						|| at->tid == DAO_CSTRUCT || bt->tid == DAO_CSTRUCT
						|| at->tid == DAO_INTERFACE || bt->tid == DAO_INTERFACE ){
					ct = DaoCheckBinArith( routine, vmc, at, bt, types[opc], hostClass, mbs );
					if( ct == NULL ) ct = dao_type_int;
				}else if( at->tid == bt->tid ){
					if( at->tid == DAO_COMPLEX && code < DVM_EQ ) goto InvOper;
					if( at->tid > DAO_TUPLE && code != DVM_EQ && code != DVM_NE ) goto InvOper;
				}else if( at->tid >= DAO_INTEGER && at->tid <= DAO_LONG
						&& bt->tid >= DAO_INTEGER && bt->tid <= DAO_LONG
						&& at->tid != DAO_COMPLEX && bt->tid != DAO_COMPLEX ){
					/* pass */
				}else if( code != DVM_EQ && code != DVM_NE ){
					goto InvOper;
				}
				DaoInferencer_UpdateType( self, opc, ct );
				/* allow less strict typing: */
				if( ct->tid == DAO_UDT || ct->tid == DAO_ANY ) continue;
				AssertTypeMatching( ct, types[opc], defs );
				ct = types[opc];
				if( typed_code && at->realnum && bt->realnum && ct->realnum ){
					DaoType *max = at->tid > bt->tid ? at : bt;
					if( at->tid != max->tid ){
						DaoInferencer_InsertMove( self, inode, & inode->a, at, max );
						if( opa == opb ) inode->b = inode->a;
					}
					if( opa != opb && bt->tid != max->tid )
						DaoInferencer_InsertMove( self, inode, & inode->b, bt, max );

					switch( max->tid ){
					case DAO_INTEGER : vmc->code += DVM_LT_III - DVM_LT; break;
					case DAO_FLOAT  : vmc->code += DVM_LT_IFF - DVM_LT; break;
					case DAO_DOUBLE : vmc->code += DVM_LT_IDD - DVM_LT; break;
					}
					if( ct->tid != DAO_INTEGER )
						DaoInferencer_InsertMove2( self, inode, dao_type_int, ct );
				}else if( typed_code && ct->realnum && at->tid == bt->tid && bt->tid == DAO_COMPLEX ){
					vmc->code += DVM_EQ_ICC - DVM_EQ;
					if( ct->tid != DAO_INTEGER )
						DaoInferencer_InsertMove2( self, inode, dao_type_int, ct );
				}else if( typed_code && ct->realnum && at->tid == bt->tid && bt->tid == DAO_STRING ){
					vmc->code += DVM_LT_ISS - DVM_LT;
					if( ct->tid != DAO_INTEGER )
						DaoInferencer_InsertMove2( self, inode, dao_type_int, ct );
				}
				break;
			}
		case DVM_IN :
			ct = dao_type_int;
			if( at->tid != DAO_ENUM && bt->tid == DAO_ENUM ) goto InvOper;
			if( NoCheckingType( bt ) ==0 ){
				if( bt->tid < DAO_STRING ) goto InvOper;
			}
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			break;
		case DVM_NOT :
			ct = NoCheckingType( at ) ? dao_type_udf : dao_type_int;
			ct = DaoInferencer_UpdateType( self, opc, ct );
			if( NoCheckingType( at ) ) continue;
			AssertTypeMatching( dao_type_int, ct, defs );
			if( at->realnum ){
				if( typed_code == 0 ) continue;
				if( ct->realnum ) inode->code = DVM_NOT_I + (at->tid - DAO_INTEGER);
				if( ct != dao_type_int ) DaoInferencer_InsertMove2( self, inode, ct, dao_type_int );
				continue;
			}
			if( at->tid == DAO_LONG || at->tid == DAO_ENUM || at->tid == DAO_ARRAY ) continue;
			/* TODO: check overloading? */
			if( at->tid >= DAO_OBJECT && at->tid <= DAO_CTYPE ) continue;
			goto InvOper;
			break;
		case DVM_MINUS :
			ct = DaoInferencer_UpdateType( self, opc, at );
			if( NoCheckingType( at ) ) continue;
			AssertTypeMatching( at, ct, defs );
			if( at->tid >= DAO_INTEGER && at->tid <= DAO_COMPLEX ){
				if( typed_code ){
					if( at != ct ) DaoInferencer_InsertMove( self, inode, & inode->a, at, ct );
					inode->code = DVM_MINUS_I + (ct->tid - DAO_INTEGER);
				}
				continue;
			}
			if( at->tid == DAO_LONG || at->tid == DAO_ARRAY ) continue;
			/* TODO: check overloading? */
			if( at->tid >= DAO_OBJECT && at->tid <= DAO_CTYPE ) continue;
			goto InvOper;
			break;
		case DVM_TILDE :
			{
				ct = DaoInferencer_UpdateType( self, opc, at );
				if( NoCheckingType( at ) ) continue;
				AssertTypeMatching( at, ct, defs );
				if( typed_code && at->realnum && ct->realnum ){
					if( at->tid != DAO_INTEGER )
						DaoInferencer_InsertMove( self, inode, & inode->a, at, dao_type_int );
					vmc->code = DVM_TILDE_I;
					if( ct->tid != DAO_INTEGER )
						DaoInferencer_InsertMove2( self, inode, dao_type_int, ct );
				}else if( typed_code && at->tid == DAO_COMPLEX && ct->tid == DAO_COMPLEX ){
					vmc->code = DVM_TILDE_C;
				}
				break;
			}
		case DVM_SIZE :
			{
				ct = DaoInferencer_UpdateType( self, opc, dao_type_int );
				if( NoCheckingType( at ) ) continue;
				AssertTypeMatching( dao_type_int, ct, defs );
				if( at->tid >= DAO_INTEGER && at->tid <= DAO_COMPLEX ){
					vmc->code = DVM_DATA_I;
					vmc->a = DAO_INTEGER;
					switch( at->tid ){
					case DAO_INTEGER : vmc->b = sizeof(daoint); break;
					case DAO_FLOAT   : vmc->b = sizeof(float); break;
					case DAO_DOUBLE  : vmc->b = sizeof(double); break;
					case DAO_COMPLEX : vmc->b = sizeof(complex16); break;
					}
				}
				break;
			}
		case DVM_BITAND : case DVM_BITOR : case DVM_BITXOR :
		case DVM_BITLFT : case DVM_BITRIT :
			{
				ct = NULL;
				if( at->tid == DAO_LIST ){
					if( code != DVM_BITLFT && code != DVM_BITRIT ) goto InvOper;
					ct = at;
					at = at->nested->items.pType[0];
					AssertTypeMatching( bt, at, defs );
					if( at->tid == DAO_UDT && bt->tid != DAO_UDT ){
						at = DaoNamespace_MakeType( NS, "list", DAO_LIST, NULL, & bt, at!=NULL );
						DaoInferencer_UpdateType( self, opa, at );
					}
				}else if( NoCheckingType( at ) || NoCheckingType( bt ) ){
					ct = dao_type_udf;
				}else if( at->tid == DAO_OBJECT || bt->tid == DAO_OBJECT
						|| at->tid == DAO_CDATA || bt->tid == DAO_CDATA
						|| at->tid == DAO_CSTRUCT || bt->tid == DAO_CSTRUCT
						|| at->tid == DAO_INTERFACE || bt->tid == DAO_INTERFACE ){
					ct = DaoCheckBinArith( routine, vmc, at, bt, ct, hostClass, mbs );
					if( ct == NULL ) goto InvOper;
				}else if( at->tid == bt->tid && at->tid == DAO_ENUM ){
					if( code != DVM_BITAND && code != DVM_BITOR ) goto InvOper;
					if( at != bt ) goto InvOper;
					ct = at;
				}else if( at->tid == bt->tid ){
					ct = at;
					if( at->tid > DAO_DOUBLE && at->tid != DAO_LONG ) goto InvOper;
				}else if( (at->realnum || at->tid == DAO_LONG)
						&& (bt->realnum || bt->tid == DAO_LONG) ){
					ct = at->tid > bt->tid ? at : bt;
				}else{
					goto InvOper;
				}
				if( at->realnum && bt->realnum ) ct = dao_type_int;
				DaoInferencer_UpdateType( self, opc, ct );
				/* allow less strict typing: */
				if( ct->tid == DAO_UDT || ct->tid == DAO_ANY ) continue;
				AssertTypeMatching( ct, types[opc], defs );
				ct = types[opc];
				if( typed_code && at->realnum && bt->realnum && ct->realnum ){
					vmc->code += DVM_BITAND_III - DVM_BITAND;
					if( at->tid != DAO_INTEGER ){
						DaoInferencer_InsertMove( self, inode, & inode->a, at, dao_type_int );
						if( opa == opb ) inode->b = inode->a;
					}
					if( opa != opb && bt->tid != DAO_INTEGER )
						DaoInferencer_InsertMove( self, inode, & inode->b, bt, dao_type_int );
					if( ct->tid != DAO_INTEGER )
						DaoInferencer_InsertMove2( self, inode, dao_type_int, ct );
				}
				break;
			}
		case DVM_CHECK :
		case DVM_CHECK_ST :
			{
				DaoInferencer_UpdateType( self, opc, dao_type_int );
				AssertTypeMatching( dao_type_int, types[opc], defs );
				ct = types[opc];
				k = bt->tid == DAO_TYPE ? bt->nested->items.pType[0]->tid : DAO_UDT;
				if( k <= DAO_STRING && ct->tid == DAO_INTEGER ){
					vmc->code = DVM_CHECK_ST;
				}else if( code == DVM_CHECK_ST ){
					goto ErrorTyping;
				}
				break;
			}
		case DVM_NAMEVA :
			{
				ct = DaoNamespace_MakeType( NS, routConsts->items.pValue[opa]->xString.data->mbs,
						DAO_PAR_NAMED, (DaoValue*) types[opb], 0, 0 );
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_PAIR :
			{
				if( types[opc] && types[opc]->tid == DAO_ANY ) continue;
				ct = DaoNamespace_MakePairType( NS, types[opa], types[opb] );
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_TUPLE :
			{
				if( opa == 0 && opb == routine->parCount ){
					k = routine->routType->nested->size;
					tp = routine->routType->nested->items.pType;
					ct = DaoNamespace_MakeType( NS, "tuple", DAO_TUPLE, 0, tp, k );
				}else{
					ct = DaoNamespace_MakeType( NS, "tuple", DAO_TUPLE, 0, types + opa, opb );
				}
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_LIST : case DVM_VECTOR :
		case DVM_APLIST : case DVM_APVECTOR :
			{
				int tid = (code == DVM_LIST || code == DVM_APLIST) ? DAO_LIST : DAO_ARRAY;
				if( types[opc] && types[opc]->tid == tid ){
					if( types[opc]->nested && types[opc]->nested->size == 1 ){
						DaoType *it = types[opc]->nested->items.pType[0];
						int n = opb - (code == DVM_APLIST || code == DVM_APVECTOR);
						for(j=0; j<n; ++j) AssertTypeMatching( types[opa+j], it, defs );
						continue;
					}
				}
				at = dao_type_udf;
				if( code == DVM_VECTOR && opb ){
					at = types[opa];
					for(j=1; j<opb; j++){
						if( DaoType_MatchTo( types[opa+j], at, defs )==0 ) goto ErrorTyping;
					}
					if( at->tid == DAO_ARRAY ) at = at->nested->items.pType[0];
					if( at->tid == DAO_NONE || at->tid > DAO_COMPLEX ) at = dao_type_float;
				}else if( code == DVM_LIST && opb ){
					at = types[opa];
					for(j=1; j<opb; j++){
						if( DaoType_MatchTo( types[opa+j], at, defs )==0 ){
							at = dao_type_any;
							break;
						}
						if( at->tid < types[opa+j]->tid ) at = types[opa+j];
					}
				}else if( code == DVM_APLIST || code == DVM_APVECTOR ){
					int num = types[opa+1+(opb==3)]->tid;
					int init = types[opa]->tid;
					at = types[opa];
					if( num == 0 || (num > DAO_DOUBLE && (num & DAO_ANY) == 0) ) goto ErrorTyping;
					if( opb == 3 && (init & DAO_ANY) == 0 && (types[opa+1]->tid & DAO_ANY) == 0 ){
						int step = types[opa+1]->tid;
						if( step == 0 ) goto ErrorTyping;
						if( types[opa]->realnum ){
							if( types[opa+1]->realnum == 0 ) goto ErrorTyping;
						}else if( init == DAO_COMPLEX ){
							if( step > DAO_COMPLEX ) goto ErrorTyping;
						}else if( init == DAO_STRING ){
							if( step != DAO_STRING ) goto ErrorTyping;
						}else if( init == DAO_ARRAY ){
							if( step > DAO_COMPLEX && step != DAO_ARRAY ) goto ErrorTyping;
						}else{
							goto ErrorTyping;
						}
						if( vmc->code == DVM_VECTOR && init == DAO_STRING ) goto ErrorTyping;
					}
				}else if( opb == 0 && types[opc] != NULL ){
					if( types[opc]->tid == DAO_LIST ){
						if( code == DVM_LIST || code == DVM_APLIST ) break;
					}else if( types[opc]->tid == DAO_ARRAY ){
						if( code == DVM_VECTOR || code == DVM_APVECTOR ) break;
					}
				}
				if( code == DVM_LIST || code == DVM_APLIST )
					ct = DaoNamespace_MakeType( NS, "list", DAO_LIST, NULL, &at, at!=NULL );
				else if( at && at->tid >=DAO_INTEGER && at->tid <= DAO_COMPLEX )
					ct = DaoNamespace_MakeType( NS, "array", DAO_ARRAY, NULL, &at, 1 );
				else if( at && at->tid == DAO_ARRAY )
					ct = at;
				else
					ct = DaoNamespace_MakeType( NS, "array", DAO_ARRAY,NULL, &at, at!=NULL );
				/* else goto ErrorTyping; */
				if( opb == 0 ){
					if( code == DVM_LIST || code == DVM_APLIST ){
						ct = DaoNamespace_MakeType( NS, "list", DAO_LIST, NULL, & dao_type_udf, 1 );
					}else{
						ct = DaoNamespace_MakeType( NS, "array", DAO_ARRAY, NULL, & dao_type_udf, 1 );
					}
					ct = DaoType_Copy( ct );
					ct->isempty1 = 1;
					ct->isempty2 = 1;
					DArray_Append( NS->auxData, ct );
				}
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_MAP :
		case DVM_HASH :
			{
				if( types[opc] && types[opc]->tid == DAO_ANY ) continue;
				if( types[opc] && types[opc]->tid == DAO_MAP ){
					if( types[opc]->nested && types[opc]->nested->size == 2 ){
						DaoType *kt = types[opc]->nested->items.pType[0];
						DaoType *vt = types[opc]->nested->items.pType[1];
						for(j=0; j<opb; j+=2){
							AssertTypeMatching( types[opa+j], kt, defs );
							AssertTypeMatching( types[opa+j+1], vt, defs );
						}
						continue;
					}
				}
				ts[0] = ts[1] = dao_type_udf;
				if( opb > 0 ){
					ts[0] = types[opa];
					ts[1] = types[opa+1];
					for(j=2; j<opb; j+=2){
						if( DaoType_MatchTo( types[opa+j], ts[0], defs ) ==0 ) ts[0] = NULL;
						if( DaoType_MatchTo( types[opa+j+1], ts[1], defs ) ==0 ) ts[1] = NULL;
						if( ts[0] ==NULL && ts[1] ==NULL ) break;
					}
				}else if( opb == 0 && types[opc] != NULL && types[opc]->tid == DAO_MAP ){
					continue;
				}
				if( ts[0] ==NULL ) ts[0] = opb ? dao_type_any : dao_type_udf;
				if( ts[1] ==NULL ) ts[1] = opb ? dao_type_any : dao_type_udf;
				ct = DaoNamespace_MakeType( NS, "map", DAO_MAP, NULL, ts, 2 );
				if( opb == 0 ){
					ct = DaoType_Copy( ct );
					ct->isempty1 = 1;
					ct->isempty2 = 1;
					DArray_Append( NS->auxData, ct );
				}
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_MATRIX :
			{
				k = (vmc->b >> 8) * (0xff & vmc->b);
				if( types[opc] && types[opc]->tid == DAO_ANY ) continue;
				if( k == 0 && types[opc] != NULL ) continue;
				at = k > 0 ? types[opa] : dao_type_udf;
				for( j=0; j<k; j++){
					if( DaoType_MatchTo( types[opa+j], at, defs )==0 ) goto ErrorTyping;
				}
				ct = DaoNamespace_MakeType( NS, "array", DAO_ARRAY,NULL,&at, at!=NULL );
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_CURRY :
		case DVM_MCURRY :
			{
				ct = NULL;
				if( at->tid == DAO_TYPE ) at = at->nested->items.pType[0];
				if( at->tid == DAO_ROUTINE ){
					int wh = 0, mc = 0, call = DVM_CALL + (code - DVM_CURRY);
					DArray *routines;
					rout = (DaoRoutine*)consts[opa];
					if( rout == NULL && at->overloads ) rout = (DaoRoutine*) at->aux;
					routines = (rout && rout->overloads) ? rout->overloads->routines : NULL;
					self->array->size = 0;
					for(j=1; j<=opb; j++) DArray_Append( self->array, types[opa+j] );
					ct = DaoRoutine_PartialCheck( NS, at, routines, self->array, call, & wh, & mc );
					if( mc > 1 ) return DaoInferencer_Error( self, DTE_TYPE_AMBIGIOUS_PFA );
					if( ct == NULL ) goto InvOper;
				}else if( at->tid == DAO_CLASS ){
					DArray *routines;
					if( consts[opa] == NULL ) goto NotInit;
					klass = & at->aux->xClass;
					if( !(klass->attribs & DAO_CLS_AUTO_DEFAULT) ) goto InvOper;
					str = klass->className;
					ct = klass->objType;
					/* XXX: check field names */
				}else if( at->tid == DAO_TUPLE ){
					ct = at;
					if( code == DVM_MCURRY && (at->nested->size+1) == opb ){
						opa += 1;
						opb -= 1;
					}
					if( at->nested->size != opb ) goto NotMatch;
					for(j=1; j<=opb; j++){
						bt = types[opa+j];
						if( bt == NULL ) goto ErrorTyping;
						if( bt->tid == DAO_PAR_NAMED ){
							if( at->mapNames == NULL ) goto InvField;
							node = MAP_Find( at->mapNames, bt->fname );
							if( node == NULL || node->value.pInt != j-1 ) goto InvField;
							bt = & bt->aux->xType;
						}
						tt = at->nested->items.pType[j-1];
						if( tt->tid == DAO_PAR_NAMED ) tt = & tt->aux->xType;
						AssertTypeMatching( bt, tt, defs );
					}
				}else{
					ct = dao_type_udf;
				}
				DaoInferencer_UpdateType( self, opc, ct );
				if( at->tid == DAO_ANY || at->tid == DAO_UDT ) break;
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_SWITCH :
			j = 0;
			for(k=1; k<=opc; k++){
				DaoValue *cc = routConsts->items.pValue[ inodes[i+k]->a ];
				j += (cc && cc->type == DAO_ENUM && cc->xEnum.etype->name->mbs[0] == '$');
				bt = DaoNamespace_GetType( NS, cc );
				if( at->name->mbs[0] == '$' && bt->name->mbs[0] == '$' ) continue;
				if( DaoType_MatchValue( at, cc, defs ) ==0 ){
					int matched = 0;
					if( cc->type == DAO_TUPLE && cc->xBase.subtype == DAO_PAIR ){
						matched = DaoType_MatchValue( at, cc->xTuple.items[0], defs );
						matched &= DaoType_MatchValue( at, cc->xTuple.items[1], defs );
					}
					if( matched == 0 ){
						self->currentIndex = i + k;
						type = DaoNamespace_GetType( NS, cc );
						return DaoInferencer_ErrorTypeNotMatching( self, type, at );
					}
				}
			}
			if( consts[opa] && consts[opa]->type ){
				DaoValue *sv = consts[opa];
				for(k=1; k<=opc; k++){
					DaoValue *cc = routConsts->items.pValue[ inodes[i+k]->a ];
					if( DaoValue_Compare( sv, cc ) ==0 ){
						inode->code = DVM_GOTO;
						inode->jumpFalse = inodes[i+k];
						break;
					}
				}
			}else if( at->tid == DAO_ENUM && at->name->mbs[0] != '$' && j == opc ){
				DaoInode *front = inodes[i];
				DaoInode *back = inodes[i+opc+1];
				DaoEnum denum = {DAO_ENUM,0,0,0,0,0,0,NULL};
				DMap *jumps = DMap_New(D_VALUE,0);
				DNode *it, *find;
				int max=0, min=0;
				denum.etype = at;
				for(k=1; k<=opc; k++){
					DaoValue *cc = routConsts->items.pValue[ inodes[i+k]->a ];
					if( DaoEnum_SetValue( & denum, & cc->xEnum, NULL ) == 0 ){
						self->currentIndex = i + k;
						DMap_Delete( jumps );
						return DaoInferencer_ErrorTypeNotMatching( self, cc->xEnum.etype, at );
					}
					if( k ==1 ){
						max = min = denum.value;
					}else{
						if( denum.value > max ) max = denum.value;
						if( denum.value < min ) min = denum.value;
					}
					MAP_Insert( jumps, (DaoValue*) & denum, inodes[i+k] );
				}
				if( at->flagtype == 0 && opc > 0.75*(max-min+1) ){
					for(it=DMap_First(at->mapNames); it; it=DMap_Next(at->mapNames,it)){
						if( it->value.pInt < min || it->value.pInt > max ) continue;
						denum.value = it->value.pInt;
						find = DMap_Find( jumps, (DaoValue*) & denum );
						if( find == NULL ){
							inode2 = DaoInferencer_InsertNode( self, inodes[i+1], DVM_CASE, 0, 0 );
							inode2->jumpFalse = inode->jumpFalse;
							inode2->a = routConsts->size;
							inode2->c = DAO_CASE_TABLE;
							inodes[i+1]->extra = NULL;
							DMap_Insert( jumps, (DaoValue*) & denum, inode2 );
						}else{
							find->value.pInode->a = routConsts->size;
							find->value.pInode->c = DAO_CASE_TABLE;
						}
						DaoRoutine_AddConstant( routine, (DaoValue*) & denum );
					}
					vmc->c = jumps->size;
				}
				front->next = back;
				back->prev = front;
				for(it=DMap_First(jumps); it; it=DMap_Next(jumps,it)){
					inode2 = it->value.pInode;
					front->next = inode2;
					back->prev = inode2;
					inode2->prev = front;
					inode2->next = back;
					front = inode2;
				}
				DMap_Delete( jumps );
			}else if( j ){
				inodes[i + 1]->c = DAO_CASE_UNORDERED;
			}
			break;
		case DVM_CASE :
			break;
		case DVM_ITER :
			{
				int j, skip = 0;
				if( vmc->b ){
					for(j=0; j<vmc->b; ++j){
						AssertTypeMatching( types[opa+j], dao_type_for_iterator, defs );
					}
					DaoInferencer_UpdateType( self, opc, dao_type_int );
					break;
				}
				ts[0] = dao_type_for_iterator;
				meth = NULL;
				self->type_source = at;
				ct = dao_type_for_iterator;
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				if( NoCheckingType( at ) ) break;
				DString_SetMBS( mbs, "__for_iterator__" );
				switch( at->tid ){
				case DAO_STRING :
				case DAO_ARRAY :
				case DAO_LIST :
				case DAO_MAP :
				case DAO_TUPLE :
					skip = 1;
					break;
				case DAO_CLASS :
				case DAO_OBJECT :
					klass = & at->aux->xClass;
					type2 = DaoClass_GetDataType( klass, mbs, & j, hostClass );
					if( j == DAO_ERROR_FIELD_NOTPERMIT ) goto NotPermit;
					if( j == DAO_ERROR_FIELD_NOTEXIST ) goto NotExist;
					j = DaoClass_GetDataIndex( klass, mbs );
					k = LOOKUP_ST( j );
					if( k == DAO_OBJECT_VARIABLE && at->tid == DAO_CLASS ) goto NeedInstVar;
					DaoClass_GetData( klass, mbs, & value, hostClass );
					if( value == NULL || value->type != DAO_ROUTINE ) goto NotMatch;
					meth = (DaoRoutine*) value;
					break;
				case DAO_INTERFACE :
					node = DMap_Find( at->aux->xInterface.methods, mbs );
					if( node == NULL ) goto NotExist;
					meth = node->value.pRoutine;
					break;
				default :
					if( at->typer ) meth = DaoType_FindFunction( at, mbs );
					break;
				}
				if( skip ) break;
				if( meth == NULL ) goto NotMatch;
				rout = DaoValue_Check( meth, at, ts, 1, DVM_CALL, errors );
				if( rout == NULL ) goto NotMatch;
				ct = dao_type_for_iterator;
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_GOTO :
			break;
		case DVM_TEST :
			{
				/* if( inited[opa] ==0 ) goto NotInit;  allow none value for testing! */
				if( types[opa] == NULL ) goto NotMatch;
				if( at->tid == DAO_STRING ) goto NotMatch;
				if( at->tid >= DAO_ARRAY && at->tid <= DAO_TUPLE ) goto NotMatch;
				if( consts[opa] && consts[opa]->type <= DAO_LONG ){
					vmc->code =  DaoValue_IsZero( consts[opa] ) ? (int)DVM_GOTO : (int)DVM_UNUSED;
					continue;
				}
				if( typed_code ){
					if( at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE )
						vmc->code = DVM_TEST_I + at->tid - DAO_INTEGER;
				}
				break;
			}
		case DVM_MATH :
			if( bt->tid == DAO_NONE ) goto InvParam;
			if( bt->tid > DAO_COMPLEX && !(bt->tid & DAO_ANY) ) goto InvParam;
			ct = bt; /* return the same type as the argument by default; */
			K = bt->realnum ? DVM_MATH_I + (bt->tid - DAO_INTEGER) : DVM_MATH;
			if( opa == DVM_MATH_RAND ){
				if( bt->tid == DAO_COMPLEX ) goto InvParam;
				DaoInferencer_UpdateType( self, opc, ct );
				if( bt->realnum && types[opc]->tid == bt->tid ) code = K;
			}else if( opa <= DVM_MATH_FLOOR ){
				DaoInferencer_UpdateType( self, opc, ct );
				if( bt->tid <= DAO_COMPLEX && types[opc]->tid == bt->tid ) code = K;
			}else if( opa == DVM_MATH_ABS ){
				if( bt->tid == DAO_COMPLEX ) ct = dao_type_double; /* return double; */
				DaoInferencer_UpdateType( self, opc, ct );
				if( bt->tid == DAO_COMPLEX && types[opc]->tid == DAO_DOUBLE ) code = K;
				if( bt->realnum && types[opc]->tid == bt->tid ) code = K;
			}else if( opa <= DVM_MATH_REAL ){
				if( bt->tid != DAO_COMPLEX && !(bt->tid & DAO_ANY) ) goto InvParam;
				ct = dao_type_double; /* return double; */
				DaoInferencer_UpdateType( self, opc, ct );
				if( bt->tid == DAO_COMPLEX && types[opc]->tid == DAO_DOUBLE ) code = K;
			}else if( bt->tid == DAO_INTEGER || bt->tid == DAO_FLOAT ){
				ct = dao_type_float; /* return float; */
				DaoInferencer_UpdateType( self, opc, ct );
				if( types[opc]->tid == DAO_FLOAT ) code = K;
			}else{
				DaoInferencer_UpdateType( self, opc, ct );
				if( bt->tid <= DAO_COMPLEX && types[opc]->tid == bt->tid ) code = K;
			}
			AssertTypeMatching( ct, types[opc], defs );
			inode->code = K;
			break;
		case DVM_CALL : case DVM_MCALL :
			{
				int checkfast = 0;
				int ctchecked = 0;
				int argc = vmc->b & 0xff;
				int codemode = code | ((int)vmc->b<<16);
				DaoType *cbtype = NULL;
				DaoInode *sect = NULL;
				if( (vmc->b & DAO_CALL_BLOCK) && inodes[i+2]->code == DVM_SECT ){
					sect = inodes[ i + 2 ];
					for(j=0, k=sect->a; j<sect->b; j++, k++){
						inited[k] = 1;
						DaoInferencer_UpdateType( self, k, dao_type_udf );
					}
				}
				j = types[opa+1] ? types[opa+1]->tid : DAO_UDT;
				if( code == DVM_MCALL && j >= DAO_ARRAY && j != DAO_ANY ){
					DaoInode *p = inodes[i+1];
					if( p->code == DVM_MOVE && p->a == opa+1 ){
						p->code = DVM_NOP;
						if( i+2 < N ){
							p = inodes[i+2];
							if( p->code >= DVM_SETVH && p->code <= DVM_SETF && p->a == opa+1 )
								p->code = DVM_NOP;
						}
					}
				}
				at = types[opa];
				bt = ct = NULL;
				if( code == DVM_CALL && self->tidHost == DAO_OBJECT ) bt = hostClass->objType;

#if 0
				DaoVmCodeX_Print( *vmc, NULL );
				printf( "call: %s %i\n", types[opa]->name->mbs, types[opa]->tid );
				if(bt) printf( "self: %s\n", bt->name->mbs );
#endif

				pp = consts+opa+1;
				tp = types+opa+1;
				for(k=0; k<argc; k++){
					tt = DaoType_DefineTypes( tp[k], NS, defs );
					GC_ShiftRC( tt, tp[k] );
					tp[k] = tt;
				}
				m = 1; /* tail call; */
				for(k=i+1; k<N; k++){
					DaoInode *ret = inodes[k];
					if( ret->code == DVM_NOP ) continue;
					if( ret->code == DVM_RETURN ){
						m &= ret->c ==0 && (ret->b ==0 || (ret->b ==1 && ret->a == vmc->c));
						break;
					}
					m = 0;
					break;
				}
				if( m ) vmc->b |= DAO_CALL_TAIL;
				if( (vmc->b & DAO_CALL_EXPAR) && argc && tp[argc-1]->tid == DAO_TUPLE ){
					DArray *its = tp[argc-1]->nested;
					DArray_Clear( self->array2 );
					for(k=0; k<(argc-1); k++) DArray_Append( self->array2, tp[k] );
					for(k=0; k<its->size; k++){
						DaoType *it = its->items.pType[k];
						if( it->tid == DAO_PAR_NAMED ) it = (DaoType*) it->aux;
						DArray_Append( self->array2, it );
					}
					tp = self->array2->items.pType;
					argc = self->array2->size;
				}

				ct = types[opa];
				rout = NULL;
				if( at->tid == DAO_CLASS ){
					if( at->aux->xClass.classRoutines->overloads->routines->size ){
						rout = (DaoRoutine*) at->aux->xClass.classRoutines; /* XXX */
					}else{
						rout = at->aux->xClass.classRoutine;
					}
					ct = at->aux->xClass.objType;
				}else if( at->tid == DAO_CTYPE ){
					rout = DaoType_FindFunction( at, at->name );
					if( rout == NULL ) goto ErrorTyping;
				}else if( consts[opa] && consts[opa]->type == DAO_ROUTINE ){
					rout = (DaoRoutine*) consts[opa];
				}else if( at->tid == DAO_THT ){
					DaoInferencer_UpdateType( self, opc, dao_type_any );
					AssertTypeMatching( dao_type_any, types[opc], defs );
					goto TryPushBlockReturnType;
				}else if( at->tid == DAO_UDT || at->tid == DAO_ANY ){
					DaoInferencer_UpdateType( self, opc, dao_type_any );
					goto TryPushBlockReturnType;
				}else if( at->tid == DAO_OBJECT ){
					rout = DaoClass_FindOperator( & at->aux->xClass, "()", hostClass );
					if( rout == NULL ) goto ErrorTyping;
				}else if( at->tid == DAO_CDATA || at->tid == DAO_CSTRUCT ){
					rout = DaoType_FindFunctionMBS( at, "()" );
					if( rout == NULL ) goto ErrorTyping;
				}else if( at->tid != DAO_ROUTINE ){
					goto ErrorTyping;
				}
				if( at->tid == DAO_ROUTINE && at->overloads ) rout = (DaoRoutine*)at->aux;
				if( rout == NULL && at->aux == NULL ){ /* "routine" type: */
					/* DAO_CALL_INIT: mandatory passing the implicit self parameter. */
					if( !(vmc->b & DAO_CALL_INIT) ) vmc->b |= DAO_CALL_NOSELF;
					ct = dao_type_any;
					ctchecked = 1;
				}else if( rout == NULL ){
					if( !(vmc->b & DAO_CALL_INIT) ) vmc->b |= DAO_CALL_NOSELF;
					if( DaoRoutine_CheckType( at, NS, NULL, tp, argc, codemode, 0 ) ==0 ){
						DaoRoutine_CheckError( NS, NULL, at, NULL, tp, argc, codemode, errors );
						goto ErrorTyping;
					}
					if( at->name->mbs[0] == '@' ){
						ct = tp[0];
						if( pp[0] && pp[0]->type == DAO_ROUTINE ) ct = pp[0]->xRoutine.routType;
						DaoInferencer_UpdateType( self, opc, ct );
						AssertTypeMatching( ct, types[opc], defs );
						goto TryPushBlockReturnType;
					}
					cbtype = at->cbtype;
					DaoRoutine_CheckType( at, NS, NULL, tp, argc, codemode, 1 );
					ct = types[opa];
				}else{
					if( rout->type != DAO_ROUTINE ) goto ErrorTyping;
					rout2 = rout;
					/* rout can be DRoutines: */
					rout = DaoValue_Check( rout, bt, tp, argc, codemode, errors );
					if( rout == NULL ) goto ErrorTyping;
					if( rout->attribs & DAO_ROUT_PRIVATE ){
						if( rout->routHost && rout->routHost != routine->routHost ) goto CallNotPermit;
						if( rout->routHost == NULL && rout->nameSpace != NS ) goto CallNotPermit;
					}else if( rout->attribs & DAO_ROUT_PROTECTED ){
						if( rout->routHost && routine->routHost == NULL ) goto CallNotPermit;
					}else if( vmc->code == DVM_CALL && routine->routHost ){
						if( DaoType_ChildOf( routine->routHost, rout->routHost ) ){
							int att1 = routine->attribs & DAO_ROUT_STATIC;
							int att2 = rout->attribs & DAO_ROUT_STATIC;
							int att3 = rout->attribs & DAO_ROUT_INITOR;
							if( att1 != 0 && att2 == 0 && att3 == 0 ) goto CallWithoutInst;
						}
					}
					checkfast = DVM_CALL && ((vmc->b & 0xff00) & ~DAO_CALL_TAIL) == 0;
					checkfast &= at->tid == DAO_ROUTINE && argc >= rout2->parCount;
					checkfast &= rout2->routHost == NULL;
					checkfast &= rout2->overloads == NULL && rout2->specialized == NULL;
					checkfast &= rout2->body != NULL || rout2->pFunc != NULL; /* not curry; */
					if( checkfast ){
						int fast = 1;
						for(k=0; fast && k<argc; ++k) fast &= tp[k]->tid != DAO_PAR_NAMED;
						for(k=0; fast && k<rout2->routType->nested->size; ++k){
							DaoType *part = rout2->routType->nested->items.pType[k];
							DaoType *argt = tp[k];
							if( part->tid >= DAO_PAR_NAMED && part->tid <= DAO_PAR_VALIST ){
								part = (DaoType*) part->aux;
							}
							if( part->tid == DAO_ANY ) continue;
							fast &= DaoType_MatchTo( argt, part, NULL ) >= DAO_MT_EQ;
						}
						if( fast ) vmc->b |= DAO_CALL_FAST;
					}
					if( rout->attribs & DAO_ROUT_DECORATOR ){
						ct = tp[0];
						if( pp[0] && pp[0]->type == DAO_ROUTINE ){
							ct = pp[0]->xRoutine.routType;
							if( pp[0]->xRoutine.overloads ){
								DaoType *ft = & rout->routType->nested->items.pType[0]->aux->xType;
								DaoType **pts = ft->nested->items.pType;
								int nn = ft->nested->size;
								int cc = DVM_CALL + (ft->attrib & DAO_TYPE_SELF);
								rout = DaoValue_Check( (DaoRoutine*)pp[0], NULL, pts, nn, cc|((int)vmc->b<<16), errors );
								if( rout == NULL ) goto ErrorTyping;
							}
						}
						DaoInferencer_UpdateType( self, opc, ct );
						AssertTypeMatching( ct, types[opc], defs );
						goto TryPushBlockReturnType;
					}

					if( rout2->overloads && rout2->overloads->routines->size > 1 ){
						DArray *routines = rout2->overloads->routines;
						m = DaoRoutine_CheckType( rout->routType, NS, bt, tp, argc, codemode, 1 );
						if( m <= DAO_MT_ANY ){
							/* For situations like:
							//
							// routine test( x :int ){ io.writeln(1); return 1; }
							// routine test( x ){ io.writeln(2); return 'abc'; }
							// a :dao_type_any = 1;
							// b = test( a );
							//
							// The function call may be resolved at compiling time as test(x),
							// which returns a string. But a runtime, the function call will
							// be resolved as test(x:int), which return an integer.
							// Such discrepancy need to be solved here:
							 */
							DArray_Clear( self->array );
							for(k=0,K=routines->size; k<K; k++){
								DaoType *type = routines->items.pRoutine[k]->routType;
								m = DaoRoutine_CheckType( type, NS, bt, tp, argc, codemode, 1 );
								if( m == 0 ) continue;
								type = (DaoType*) type->aux;
								if( type == NULL ) type = dao_type_none;
								if( type && type->tid == DAO_ANY ){
									ctchecked = 1;
									ct = dao_type_any;
									break;
								}
								for(m=0,K=self->array->size; m<K; m++){
									if( self->array->items.pType[m] == type ) break;
								}
								if( m >= self->array->size ) DArray_Append( self->array, type );
							}
							if( self->array->size > 1 ){
								ctchecked = 1;
								ct = dao_type_any; /* XXX variant type? */
							}
						}
					}

					tt = rout->routType;
					cbtype = tt->cbtype;

					DMap_Reset( defs2 );
					if( at->tid == DAO_CTYPE && at->kernel->sptree ){
						/* For type holder specialization: */
						k = DaoType_MatchTo( at, at->kernel->abtype->aux->xCdata.ctype, defs2 );
					}

					k = defs2->size;
					DaoRoutine_PassParamTypes( rout, bt, tp, argc, code, defs2 );
					if( rout != routine && defs2->size && (defs2->size > k || rout->routType->aux->xType.tid == DAO_UDT) ){
						DaoRoutine *orig = rout, *rout2 = rout;
						if( rout->original ) rout = orig = rout->original;

						/* Do not share function body. It may be thread unsafe to share: */
						rout = DaoRoutine_Copy( rout, 0, 1, 0 );
						DaoRoutine_Finalize( rout, NULL, defs2 );

						if( rout->routType->attrib & DAO_TYPE_SPEC ){
							DaoGC_TryDelete( (DaoValue*) rout );
							rout = rout2;
						}else{
							DMutex_Lock( & mutex_routine_specialize );
							if( orig->specialized == NULL ) orig->specialized = DRoutines_New();
							DMutex_Unlock( & mutex_routine_specialize );

							GC_ShiftRC( orig, rout->original );
							rout->original = orig;
							/*
							// Need to add before specializing the body,
							// to avoid possible infinite recursion:
							 */
							body->specialized = 0;
							DRoutines_Add( orig->specialized, rout );

							/* rout may has only been declared */
							/* forward declared routine may have an empty routine body: */
							if( notide && rout->body && rout->body->vmCodes->size ){
								DMap_Reset( defs3 );
								DaoType_MatchTo( rout->routType, orig->routType, defs3 );
								DaoRoutine_MapTypes( rout, defs3 );

								/* to infer returned type */
								if( DaoRoutine_DoTypeInference( rout, self->silent ) ==0 ) goto InvParam;
							}
						}
					}
					if( at->tid != DAO_CLASS && ! ctchecked ) ct = rout->routType;
					/*
					   printf( "ct2 = %s\n", ct ? ct->name->mbs : "" );
					 */
				}
				k = routine->routType->attrib & ct->attrib;
				if( at->tid != DAO_CLASS && ! ctchecked ) ct = & ct->aux->xType;
				if( ct ) ct = DaoType_DefineTypes( ct, NS, defs2 );

#ifdef DAO_WITH_CONCURRENT
				if( (vmc->b & DAO_CALL_ASYNC) && at->tid != DAO_CLASS ){
					ct = DaoCdataType_Specialize( dao_type_future, & ct, ct != NULL );
				}
#endif
				if( types[opc] && types[opc]->tid == DAO_ANY ) goto TryPushBlockReturnType;
				if( ct == NULL ) ct = DaoNamespace_GetType( NS, dao_none_value );
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				/*
				   if( rout && strcmp( rout->routName->mbs, "values" ) ==0 ){
				   DaoVmCodeX_Print( *vmc, NULL );
				   printf( "ct = %s, %s %s\n", ct->name->mbs, self->routName->mbs, routine->routType->name->mbs );
				   printf( "%p  %s\n", types[opc], types[opc] ? types[opc]->name->mbs : "----------" );
				   }
				 */

TryPushBlockReturnType:
				if( sect && cbtype && cbtype->nested ){
					for(j=0, k=sect->a; j<sect->b; j++, k++){
						if( j >= (int)cbtype->nested->size ){
							if( j < sect->c ) printf( "Unsupported code section parameter!\n" );
							break;
						}/* XXX better warning */
						tt = cbtype->nested->items.pType[j];
						if( tt->tid == DAO_PAR_NAMED || tt->tid == DAO_PAR_DEFAULT ) tt = (DaoType*)tt->aux;
						tt = DaoType_DefineTypes( tt, NS, defs2 );
						GC_DecRC( types[k] );
						types[k] = NULL;
						DaoInferencer_UpdateType( self, k, tt );
					}
					tt = DaoType_DefineTypes( (DaoType*)cbtype->aux, NS, defs2 );
					DArray_Append( rettypes, IntToPointer( inodes[i+1]->b ) );
					DArray_Append( rettypes, inode ); /* type at "opc" to be redefined; */
					DArray_Append( rettypes, tt );
					DArray_Append( rettypes, tt );
					DArray_PushBack( self->typeMaps, defs2 );
				}else if( sect && cbtype == NULL ){
					if( NoCheckingType( types[opc] ) == 0 ){
						printf( "Unused code section at line %i!\n", vmc->line );
					}
					DArray_Append( rettypes, IntToPointer( inodes[i+1]->b ) );
					DArray_Append( rettypes, inode );
					DArray_Append( rettypes, NULL );
					DArray_Append( rettypes, NULL );
					DArray_PushBack( self->typeMaps, defs2 );
				}
				break;
			}
		case DVM_ROUTINE :
			if( types[opa]->tid != DAO_ROUTINE ) goto ErrorTyping;
			/* close on types */
			at = types[opa];
			closure = (DaoRoutine*) consts[opa];
			DaoRoutine_DoTypeInference( closure, self->silent );

			self->array->size = 0;
			DArray_Resize( self->array, closure->parCount, 0 );
			K = vmc->b - closure->body->svariables->size;
			for(j=0,k=0; j<closure->parCount; j+=1){
				DaoType *partype = closure->routType->nested->items.pType[j];
				self->array->items.pType[j] = partype;
				if( partype->tid != DAO_PAR_DEFAULT ) continue;
				if( closure->routConsts->items.items.pValue[j] != NULL ) continue;
				if( k >= K ) goto ErrorTyping;
				self->array->items.pType[j] = types[opa+1+k];
				k += 1;
			}
			m = (closure->attribs & DAO_ROUT_PASSRET) != 0;
			if( m ){
				DaoType *retype = (DaoType*) routine->routType->aux;
				GC_ShiftRC( retype, closure->body->svariables->items.pVar[0]->dtype );
				closure->body->svariables->items.pVar[0]->dtype = retype;
			}
			for(j=m; j<closure->body->svariables->size; ++j){
				DaoType *uptype = types[opa+1+k+j-m];
				GC_ShiftRC( uptype, closure->body->svariables->items.pVar[j]->dtype );
				closure->body->svariables->items.pVar[j]->dtype = uptype;
			}
			at = DaoNamespace_MakeRoutType( NS, at, NULL, self->array->items.pType, NULL );

			DaoInferencer_UpdateType( self, opc, at );
			AssertTypeMatching( at, types[opc], defs );
			break;

		case DVM_RETURN :
		case DVM_YIELD :
			{
				DaoInode *redef;
				DaoType *ct2;
				ct = rettypes->items.pType[ rettypes->size - 1 ];
				ct2 = rettypes->items.pType[ rettypes->size - 2 ];
				redef = rettypes->items.pInode[ rettypes->size - 3 ];
				DMap_Reset( defs2 );
				DMap_Assign( defs2, defs );

				/*
				// DO NOT CHANGE
				// FROM: return (e1, e2, e3, ... )
				// TO:   return e1, e2, e3, ...
				//
				// Because they bear different semantic meaning.
				// For example, if "e1" is in the form of "name=>expression",
				// the name is not stored in the tuple value but in the tuple type for
				// the first. For the second, it should be part of the returned value.
				//
				// The following code should NOT be used!
				*/
#if 0
				if( i && inodes[i-1]->code == DVM_TUPLE && inodes[i-1]->c == vmc->a && vmc->b == 1 ){
					vmc->a = inodes[i-1]->a;
					vmc->b = inodes[i-1]->b;
					inodes[i-1]->code = DVM_UNUSED;
					opa = vmc->a;
					opb = vmc->b;
					opc = vmc->c;
				}
#endif

				/*
				   printf( "%p %i %s %s\n", self, routine->routType->nested->size, routine->routType->name->mbs, ct?ct->name->mbs:"" );
				 */
				if( code == DVM_YIELD && routine->routType->cbtype ){ /* yield in functional method: */
					if( vmc->b == 0 ){
						if( routine->routType->cbtype->aux ) goto ErrorTyping;
						break;
					}
					tt = routine->routType->cbtype;
					tp = tt->nested->items.pType;
					at = DaoNamespace_MakeType( NS, "tuple", DAO_TUPLE, NULL, types+opa, vmc->b);
					ct = DaoNamespace_MakeType( NS, "tuple", DAO_TUPLE, NULL, tp, tt->nested->size );
					if( DaoType_MatchTo( at, ct, defs2 ) == 0 ) goto ErrorTyping;
					ct = (DaoType*) routine->routType->cbtype->aux;
					if( ct ){
						DaoInferencer_UpdateType( self, opc, ct );
						AssertTypeMatching( ct, types[opc], defs2 );
					}
					break;
				}
				if( vmc->b ==0 ){
					/* less strict checking for type holder as well (case mt.start()): */
					if( ct && ct->tid == DAO_UDT ){
						ct = DaoNamespace_MakeValueType( NS, dao_none_value );
						rettypes->items.pType[ rettypes->size - 1 ] = ct;
						ct = DaoNamespace_MakeRoutType( NS, routine->routType, NULL, NULL, ct );
						GC_ShiftRC( ct, routine->routType );
						routine->routType = ct;
						continue;
					}
					if( ct && NoCheckingType( ct ) ) continue;
					if( ct && ct->tid == DAO_VALTYPE && ct->aux->type == DAO_NONE ) continue;
					if( ct && ! (routine->attribs & DAO_ROUT_INITOR) ) goto ErrorTyping;
				}else{
					at = types[opa];
					if( at ==NULL ) goto ErrorTyping;
					if( vmc->b >1 )
						at = DaoNamespace_MakeType( NS, "tuple", DAO_TUPLE, NULL, types+opa, vmc->b);

					if( ct && DaoType_MatchTo( at, ct, defs2 ) == 0 ) goto ErrorTyping;
					if( ct != ct2 && ct->tid != DAO_UDT && ct->tid != DAO_THT ){
						int mac = DaoType_MatchTo( at, ct, defs2 );
						int mca = DaoType_MatchTo( ct, at, defs2 );
						if( mac==0 && mca==0 ){
							goto ErrorTyping;
						}else if( mac==0 ){
							if( rettypes->size == 4 ){
								if( at && at->tid != DAO_UDT ){
									tt = DaoNamespace_MakeRoutType( NS, routine->routType, NULL, NULL, at );
									GC_ShiftRC( tt, routine->routType );
									routine->routType = tt;
								}
							}else{
								ct = DaoType_DefineTypes( ct, NS, defs2 );
								if( ct != NULL && redef != NULL ){
									tt = DaoType_DefineTypes( types[redef->c], NS, defs2 );
									GC_DecRC( types[redef->c] );
									types[redef->c] = NULL;
									DaoInferencer_UpdateType( self, redef->c, tt );
								}
								rettypes->items.pType[ rettypes->size - 1 ] = ct;
							}
						}
					}else if( ct == NULL || ( ct->attrib & (DAO_TYPE_SPEC|DAO_TYPE_UNDEF)) ){
						if( rettypes->size == 4 ){
							if( at && at->tid != DAO_UDT ){
								tt = DaoNamespace_MakeRoutType( NS, routine->routType, NULL, NULL, at );
								GC_ShiftRC( tt, routine->routType );
								routine->routType = tt;
							}
							rettypes->items.pType[ rettypes->size - 1 ] = (DaoType*)routine->routType->aux;
						}else{
							ct = DaoType_DefineTypes( ct, NS, defs2 );
							if( ct != NULL && redef != NULL ){
								tt = DaoType_DefineTypes( types[redef->c], NS, defs2 );
								GC_DecRC( types[redef->c] );
								types[redef->c] = NULL;
								DaoInferencer_UpdateType( self, redef->c, tt );
							}
							rettypes->items.pType[ rettypes->size - 1 ] = ct;
						}
					}
					if( redef != NULL && redef->code == DVM_EVAL ){
						GC_DecRC( types[redef->c] );
						types[redef->c] = NULL;
						DaoInferencer_UpdateType( self, redef->c, at );
					}
				}
				if( code == DVM_YIELD ){
					tt = routine->routType;
					if( tt->nested->size ==1 ){
						ct = tt->nested->items.pType[0];
						if( ct->tid == DAO_PAR_NAMED || ct->tid == DAO_PAR_DEFAULT )
							ct = & ct->aux->xType;
					}else if( tt->nested->size ){
						ct = DaoNamespace_MakeType(NS, "tuple", DAO_TUPLE, NULL,
								tt->nested->items.pType, tt->nested->size );
					}else{
						ct = dao_type_udf;
					}
					DaoInferencer_UpdateType( self, opc, ct );
					AssertTypeMatching( ct, types[opc], defs2 );
					AssertTypeMatching( at, & tt->aux->xType, defs2 );
				}
				break;
			}
		case DVM_EVAL :
			if( vmc->b != 1 ) at = NULL;
			DArray_Append( rettypes, IntToPointer( inodes[i+1+(vmc->b==2)]->b ) );
			DArray_Append( rettypes, inode );
			DArray_Append( rettypes, at );
			DArray_Append( rettypes, at );
			DArray_PushBack( self->typeMaps, defs2 );
			break;

#define USE_TYPED_OPCODE 1

#if USE_TYPED_OPCODE
		case DVM_DATA_I : case DVM_DATA_F : case DVM_DATA_D : case DVM_DATA_C :
			TT1 = DAO_INTEGER + (code - DVM_DATA_I);
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETCL_I : case DVM_GETCL_F : case DVM_GETCL_D : case DVM_GETCL_C :
			value = routConsts->items.pValue[opb];
			TT1 = DAO_INTEGER + (code - DVM_GETCL_I);
			at = DaoNamespace_GetType( NS, value );
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETCK_I : case DVM_GETCK_F : case DVM_GETCK_D : case DVM_GETCK_C :
			value = hostClass->constants->items.pConst[opb]->value;
			TT1 = DAO_INTEGER + (code - DVM_GETCK_I);
			at = DaoNamespace_GetType( NS, value );
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETCG_I : case DVM_GETCG_F : case DVM_GETCG_D : case DVM_GETCG_C :
			value = NS->constants->items.pConst[opb]->value;
			TT1 = DAO_INTEGER + (code - DVM_GETCG_I);
			at = DaoNamespace_GetType( NS, value );
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETVH_I : case DVM_GETVH_F : case DVM_GETVH_D : case DVM_GETVH_C :
			TT1 = DAO_INTEGER + (code - DVM_GETVH_I);
			at = typeVH[opa][opb];
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETVS_I : case DVM_GETVS_F : case DVM_GETVS_D : case DVM_GETVS_C :
			TT1 = DAO_INTEGER + (code - DVM_GETVS_I);
			at = body->svariables->items.pVar[opb]->dtype;
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETVO_I : case DVM_GETVO_F : case DVM_GETVO_D : case DVM_GETVO_C :
			TT1 = DAO_INTEGER + (code - DVM_GETVO_I);
			at = hostClass->instvars->items.pVar[opb]->dtype;
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETVK_I : case DVM_GETVK_F : case DVM_GETVK_D : case DVM_GETVK_C :
			TT1 = DAO_INTEGER + (code - DVM_GETVK_I);
			at = hostClass->variables->items.pVar[opb]->dtype;
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETVG_I : case DVM_GETVG_F : case DVM_GETVG_D : case DVM_GETVG_C :
			TT1 = DAO_INTEGER + (code - DVM_GETVG_I);
			at = NS->variables->items.pVar[opb]->dtype;
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_SETVH_II : case DVM_SETVH_FF : case DVM_SETVH_DD : case DVM_SETVH_CC :
			tp = typeVH[opc] + opb;
			if( *tp == NULL || (*tp)->tid == DAO_UDT ){
				GC_ShiftRC( types[opa], *tp );
				*tp = types[opa];
			}
			TT1 = DAO_INTEGER + (code - DVM_SETVH_II);
			AssertTypeMatching( types[opa], *tp, defs );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( tp[0], TT1 );
			break;
		case DVM_SETVS_II : case DVM_SETVS_FF : case DVM_SETVS_DD : case DVM_SETVS_CC :
			tp = & body->svariables->items.pVar[opb]->dtype;
			if( *tp == NULL || (*tp)->tid == DAO_UDT ){
				GC_ShiftRC( types[opa], *tp );
				*tp = types[opa];
			}
			TT1 = DAO_INTEGER + (code - DVM_SETVS_II);
			AssertTypeMatching( types[opa], *tp, defs );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( tp[0], TT1 );
			break;
		case DVM_SETVO_II : case DVM_SETVO_FF : case DVM_SETVO_DD : case DVM_SETVO_CC :
			if( self->tidHost != DAO_OBJECT ) goto ErrorTyping;
			tp = & hostClass->instvars->items.pVar[opb]->dtype;
			if( *tp == NULL || (*tp)->tid == DAO_UDT ){
				GC_ShiftRC( types[opa], *tp );
				*tp = types[opa];
			}
			TT1 = DAO_INTEGER + (code - DVM_SETVO_II);
			AssertTypeMatching( types[opa], *tp, defs );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( tp[0], TT1 );
			break;
		case DVM_SETVK_II : case DVM_SETVK_FF : case DVM_SETVK_DD : case DVM_SETVK_CC :
			tp = & hostClass->variables->items.pVar[opb]->dtype;
			if( *tp == NULL || (*tp)->tid == DAO_UDT ){
				GC_ShiftRC( types[opa], *tp );
				*tp = types[opa];
			}
			TT1 = DAO_INTEGER + (code - DVM_SETVK_II);
			AssertTypeMatching( types[opa], *tp, defs );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( tp[0], TT1 );
			break;
		case DVM_SETVG_II : case DVM_SETVG_FF : case DVM_SETVG_DD : case DVM_SETVG_CC :
			tp = & NS->variables->items.pVar[opb]->dtype;
			if( *tp == NULL || (*tp)->tid == DAO_UDT ){
				GC_ShiftRC( types[opa], *tp );
				*tp = types[opa];
			}
			TT1 = DAO_INTEGER + (code - DVM_SETVG_II);
			AssertTypeMatching( types[opa], *tp, defs );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( tp[0], TT1 );
			break;
		case DVM_MOVE_II : case DVM_MOVE_IF : case DVM_MOVE_ID :
		case DVM_MOVE_FI : case DVM_MOVE_FF : case DVM_MOVE_FD :
		case DVM_MOVE_DI : case DVM_MOVE_DF : case DVM_MOVE_DD :
		case DVM_MOVE_CI : case DVM_MOVE_CF : case DVM_MOVE_CD :
			TT1 = DAO_INTEGER + (code - DVM_MOVE_II) % 3;
			TT3 = DAO_INTEGER + ((code - DVM_MOVE_II)/3) % 3;
			DaoInferencer_UpdateType( self, opc, self->basicTypes[TT3] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( types[opc], TT3 );
			break;
		case DVM_NOT_I : case DVM_NOT_F : case DVM_NOT_D :
		case DVM_MINUS_I : case DVM_MINUS_F : case DVM_MINUS_D :
			DaoInferencer_UpdateType( self, opc, at );
			TT1 = TT3 = DAO_INTEGER + (code - DVM_NOT_I) % 3;
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( types[opc], TT3 );
			break;
		case DVM_TILDE_I :
			DaoInferencer_UpdateType( self, opc, at );
			AssertTypeIdMatching( at, DAO_INTEGER );
			AssertTypeIdMatching( types[opc], DAO_INTEGER );
			break;
		case DVM_TILDE_C :
			DaoInferencer_UpdateType( self, opc, at );
			AssertTypeIdMatching( at, DAO_COMPLEX );
			AssertTypeIdMatching( types[opc], DAO_COMPLEX );
			break;
		case DVM_MINUS_C :
		case DVM_MOVE_CC :
		case DVM_MOVE_SS :
			DaoInferencer_UpdateType( self, opc, at );
			TT1 = TT3 = code == DVM_MOVE_SS ? DAO_STRING : DAO_COMPLEX;
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( types[opc], TT3 );
			break;
		case DVM_MOVE_PP :
		case DVM_MOVE_XX :
			if( code == DVM_MOVE_PP ){
				if( consts[opc] ) goto InvOper;
				if( at->tid && (at->tid < DAO_ARRAY || at->tid > DAO_TYPE) ) goto NotMatch;
				/* if( DaoInferencer_UpdateType( self, opc, at ) != at ) goto NotMatch; */
				DaoInferencer_UpdateType( self, opc, at );
				if( DaoType_MatchTo( types[opc], at, NULL ) != DAO_MT_EQ ) goto NotMatch;
			}else if( types[opc] == NULL || types[opc]->tid != DAO_ANY ){
				/* if( DaoInferencer_UpdateType( self, opc, at ) != at ) goto NotMatch; */
				DaoInferencer_UpdateType( self, opc, at );
				if( DaoType_MatchTo( types[opc], at, NULL ) != DAO_MT_EQ ) goto NotMatch;
			}
			if( opb ){
				GC_ShiftRC( consts[opa], consts[opc] );
				consts[opc] = consts[opa];
			}
			break;
		case DVM_ADD_III : case DVM_SUB_III : case DVM_MUL_III : case DVM_DIV_III :
		case DVM_MOD_III : case DVM_POW_III : case DVM_AND_III : case DVM_OR_III  :
		case DVM_LT_III  : case DVM_LE_III  : case DVM_EQ_III : case DVM_NE_III :
		case DVM_BITAND_III  : case DVM_BITOR_III  : case DVM_BITXOR_III :
		case DVM_BITLFT_III  : case DVM_BITRIT_III  :
			DaoInferencer_UpdateType( self, opc, dao_type_int );
			AssertTypeIdMatching( at, DAO_INTEGER );
			AssertTypeIdMatching( bt, DAO_INTEGER );
			AssertTypeIdMatching( types[opc], DAO_INTEGER );
			break;
		case DVM_ADD_FFF : case DVM_SUB_FFF : case DVM_MUL_FFF : case DVM_DIV_FFF :
		case DVM_MOD_FFF : case DVM_POW_FFF : case DVM_AND_FFF : case DVM_OR_FFF  :
		case DVM_LT_IFF  : case DVM_LE_IFF  : case DVM_EQ_IFF : case DVM_NE_IFF :
			ct = (code < DVM_LT_IFF) ? dao_type_float : dao_type_int;
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeIdMatching( at, DAO_FLOAT );
			AssertTypeIdMatching( bt, DAO_FLOAT );
			AssertTypeIdMatching( types[opc], ct->tid );
			break;
		case DVM_ADD_DDD : case DVM_SUB_DDD : case DVM_MUL_DDD : case DVM_DIV_DDD :
		case DVM_MOD_DDD : case DVM_POW_DDD : case DVM_AND_DDD : case DVM_OR_DDD  :
		case DVM_LT_IDD  : case DVM_LE_IDD  : case DVM_EQ_IDD : case DVM_NE_IDD :
			ct = (code < DVM_LT_IDD) ? dao_type_double : dao_type_int;
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeIdMatching( at, DAO_DOUBLE );
			AssertTypeIdMatching( bt, DAO_DOUBLE );
			AssertTypeIdMatching( types[opc], ct->tid );
			break;
		case DVM_ADD_CCC : case DVM_SUB_CCC : case DVM_MUL_CCC : case DVM_DIV_CCC :
		case DVM_EQ_ICC : case DVM_NE_ICC :
			ct = code >= DVM_EQ_ICC ? dao_type_complex : dao_type_int;
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeIdMatching( at, DAO_COMPLEX );
			AssertTypeIdMatching( bt, DAO_COMPLEX );
			AssertTypeIdMatching( types[opc], ct->tid );
			break;
		case DVM_ADD_SSS : case DVM_LT_ISS : case DVM_LE_ISS :
		case DVM_EQ_ISS : case DVM_NE_ISS :
			ct = code == DVM_ADD_SSS ? self->typeString : dao_type_int;
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeIdMatching( at, DAO_STRING );
			AssertTypeIdMatching( bt, DAO_STRING );
			AssertTypeIdMatching( types[opc], ct->tid );
			break;
		case DVM_GETI_SI :
			AssertTypeIdMatching( at, DAO_STRING );
			if( code == DVM_GETI_SI && bt->tid != DAO_INTEGER ) goto NotMatch;
			DaoInferencer_UpdateType( self, opc, dao_type_int );
			AssertTypeIdMatching( types[opc], DAO_INTEGER );
			break;
		case DVM_SETI_SII :
			AssertTypeIdMatching( at, DAO_INTEGER );
			AssertTypeIdMatching( bt, DAO_INTEGER );
			AssertTypeIdMatching( ct, DAO_STRING );
			break;
		case DVM_GETI_LI :
			AssertTypeIdMatching( at, DAO_LIST );
			AssertTypeIdMatching( bt, DAO_INTEGER );
			at = types[opa]->nested->items.pType[0];
			if( at->tid < DAO_ARRAY || at->tid >= DAO_ANY ) goto NotMatch;
			DaoInferencer_UpdateType( self, opc, at );
			AssertTypeMatching( at, types[opc], defs );
			break;
		case DVM_GETI_LII : case DVM_GETI_LFI : case DVM_GETI_LDI : case DVM_GETI_LCI :
		case DVM_GETI_AII : case DVM_GETI_AFI : case DVM_GETI_ADI : case DVM_GETI_ACI :
		case DVM_GETI_LSI :
			TT1 = TT3 = 0;
			if( code >= DVM_GETI_AII ){
				TT3 = DAO_ARRAY;
				TT1 = DAO_INTEGER + (code - DVM_GETI_AII);
			}else if( code != DVM_GETI_LSI ){
				TT3 = DAO_LIST;
				TT1 = DAO_INTEGER + (code - DVM_GETI_LII);
			}else{
				TT3 = DAO_LIST;
				TT1 = DAO_STRING;
			}
			if( at->tid != TT3 || at->nested->size ==0 ) goto NotMatch;
			at = at->nested->items.pType[0];
			if( at ==NULL || at->tid != TT1 ) goto NotMatch;
			if( bt ==NULL || bt->tid != DAO_INTEGER ) goto NotMatch;
			DaoInferencer_UpdateType( self, opc, at );
			AssertTypeIdMatching( types[opc], TT1 );
			break;
		case DVM_GETMI_AII : case DVM_GETMI_AFI :
		case DVM_GETMI_ADI : case DVM_GETMI_ACI :
			for(j=0; j<opb; j++){
				bt = types[opa + j + 1];
				if( bt->tid == DAO_NONE || bt->tid > DAO_DOUBLE ) goto InvIndex;
			}
			at = at->nested->items.pType[0];
			DaoInferencer_UpdateType( self, opc, at );
			AssertTypeMatching( at, types[opc], defs );
			break;
		case DVM_SETI_LI :
			AssertTypeIdMatching( bt, DAO_INTEGER );
			AssertTypeIdMatching( ct, DAO_LIST );
			ct = types[opc]->nested->items.pType[0];
			if( at != ct && ct->tid != DAO_ANY ) goto NotMatch;
			break;
		case DVM_SETI_LIII : case DVM_SETI_LFIF : case DVM_SETI_LDID : case DVM_SETI_LCIC :
		case DVM_SETI_AIII : case DVM_SETI_AFIF : case DVM_SETI_ADID : case DVM_SETI_ACIC :
		case DVM_SETI_LSIS :
			TT2 = DAO_INTEGER;
			TT1 = TT6 = 0;
			if( code >= DVM_SETI_AIII ){
				TT6 = DAO_ARRAY;
				TT1 = DAO_INTEGER + (code - DVM_SETI_AIII)%3;
			}else if( code != DVM_SETI_LSIS ){
				TT6 = DAO_LIST;
				TT1 = DAO_INTEGER + (code - DVM_SETI_LIII)%3;
			}else{
				TT6 = DAO_LIST;
				TT1 = DAO_STRING;
			}
			if( ct->tid != TT6 || bt->tid != TT2 || at->tid != TT1 ) goto NotMatch;
			if( ct->nested->size !=1 || ct->nested->items.pType[0]->tid != TT1 ) goto NotMatch;
			break;
		case DVM_SETMI_AIII : case DVM_SETMI_AFIF :
		case DVM_SETMI_ADID : case DVM_SETMI_ACIC :
			for(j=0; j<opb; j++){
				bt = types[opa + j + 1];
				if( bt->tid == DAO_NONE || bt->tid > DAO_DOUBLE ) goto InvIndex;
			}
			if( at->tid == DAO_NONE || at->tid > DAO_DOUBLE ) goto NotMatch;
			break;
		case DVM_GETI_TI :
			if( at->tid != DAO_TUPLE || bt->tid != DAO_INTEGER ) goto NotMatch;
			DaoInferencer_UpdateType( self, opc, dao_type_any );
			break;
		case DVM_SETI_TI :
			if( ct->tid != DAO_TUPLE || bt->tid != DAO_INTEGER ) goto NotMatch;
			break;
		case DVM_SETF_TPP :
		case DVM_SETF_TXX :
			if( at ==NULL || ct ==NULL || ct->tid != DAO_TUPLE ) goto NotMatch;
			if( opb >= ct->nested->size ) goto InvIndex;
			tt = ct->nested->items.pType[opb];
			if( tt->tid == DAO_PAR_NAMED ) tt = & tt->aux->xType;
			if( at != tt && tt->tid != DAO_ANY ) goto NotMatch;
			if( code == DVM_SETF_TPP && consts[opa] ) goto InvOper;
			break;
		case DVM_GETF_TI : case DVM_GETF_TF :
		case DVM_GETF_TD : case DVM_GETF_TC :
		case DVM_GETF_TX :
			if( at ==NULL || at->tid != DAO_TUPLE ) goto NotMatch;
			if( opb >= at->nested->size ) goto InvIndex;
			ct = at->nested->items.pType[opb];
			if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
			DaoInferencer_UpdateType( self, opc, ct );
			if( code != DVM_GETF_TX ){
				TT3 = DAO_INTEGER + (code - DVM_GETF_TI);
				if( ct ==NULL || ct->tid != TT3 ) goto NotMatch;
				if( types[opc]->tid != TT3 ) goto NotMatch;
			}else{
				AssertTypeMatching( ct, types[opc], defs );
			}
			break;
		case DVM_SETF_TII : case DVM_SETF_TFF :
		case DVM_SETF_TDD : case DVM_SETF_TCC :
		case DVM_SETF_TSS :
			if( at ==NULL || ct ==NULL ) goto NotMatch;
			TT1 = 0;
			if( code == DVM_SETF_TSS ){
				TT1 = DAO_STRING;
			}else{
				TT1 = DAO_INTEGER + (code - DVM_SETF_TII);
			}
			if( ct->tid != DAO_TUPLE || at->tid != TT1 ) goto NotMatch;
			if( opb >= ct->nested->size ) goto InvIndex;
			tt = ct->nested->items.pType[opb];
			if( tt->tid == DAO_PAR_NAMED ) tt = & tt->aux->xType;
			if( tt->tid != TT1 ) goto NotMatch;
			break;
		case DVM_GETF_CX :
			if( at->tid != DAO_COMPLEX ) goto NotMatch;
			ct = DaoInferencer_UpdateType( self, opc, dao_type_double );
			if( ct->tid != DAO_DOUBLE ) goto NotMatch;
			break;
		case DVM_SETF_CX :
			if( at->tid != DAO_DOUBLE ) goto NotMatch;
			if( ct->tid != DAO_COMPLEX ) goto NotMatch;
			break;
		case DVM_GETF_KCI : case DVM_GETF_KCF :
		case DVM_GETF_KCD : case DVM_GETF_KCC :
		case DVM_GETF_KC :
			if( types[opa]->tid != DAO_CLASS ) goto NotMatch;
			klass = & types[opa]->aux->xClass;
			ct = DaoNamespace_GetType( NS, klass->constants->items.pConst[ opb ]->value );
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			if( code == DVM_GETF_KC ) break;
			if( ct->tid != (DAO_INTEGER + code - DVM_GETF_KCI) ) goto NotMatch;
			break;
		case DVM_GETF_KGI : case DVM_GETF_KGF :
		case DVM_GETF_KGD : case DVM_GETF_KGC :
		case DVM_GETF_KG :
			if( types[opa]->tid != DAO_CLASS ) goto NotMatch;
			klass = & types[opa]->aux->xClass;
			ct = klass->variables->items.pVar[ opb ]->dtype;
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			if( code == DVM_GETF_KG ) break;
			if( ct->tid != (DAO_INTEGER + code - DVM_GETF_KGI) ) goto NotMatch;
			break;
		case DVM_GETF_OCI : case DVM_GETF_OCF :
		case DVM_GETF_OCD : case DVM_GETF_OCC :
		case DVM_GETF_OC :
			if( types[opa]->tid != DAO_OBJECT ) goto NotMatch;
			klass = & types[opa]->aux->xClass;
			ct = DaoNamespace_GetType( NS, klass->constants->items.pConst[ opb ]->value );
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			if( code == DVM_GETF_OC ){
				value = klass->constants->items.pConst[opb]->value;
				GC_ShiftRC( value, consts[opc] );
				consts[opc] = value;
				break;
			}
			if( ct->tid != (DAO_INTEGER + code - DVM_GETF_OCI) ) goto NotMatch;
			break;
		case DVM_GETF_OGI : case DVM_GETF_OGF :
		case DVM_GETF_OGD : case DVM_GETF_OGC :
		case DVM_GETF_OG :
			if( types[opa]->tid != DAO_OBJECT ) goto NotMatch;
			klass = & types[opa]->aux->xClass;
			ct = klass->variables->items.pVar[ opb ]->dtype;
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			if( code == DVM_GETF_OG ) break;
			if( ct->tid != (DAO_INTEGER + code - DVM_GETF_OGI) ) goto NotMatch;
			break;
		case DVM_GETF_OVI : case DVM_GETF_OVF :
		case DVM_GETF_OVD : case DVM_GETF_OVC :
		case DVM_GETF_OV :
			if( types[opa]->tid != DAO_OBJECT ) goto NotMatch;
			klass = & types[opa]->aux->xClass;
			ct = klass->instvars->items.pVar[ opb ]->dtype;
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			if( code == DVM_GETF_OV ) break;
			if( ct->tid != (DAO_INTEGER + code - DVM_GETF_OVI) ) goto NotMatch;
			break;
		case DVM_SETF_KGII : case DVM_SETF_KGFF :
		case DVM_SETF_KGDD : case DVM_SETF_KGCC :
		case DVM_SETF_KG :
			if( ct == NULL ) goto ErrorTyping;
			if( types[opa] ==NULL || types[opc] ==NULL ) goto NotMatch;
			if( ct->tid != DAO_CLASS ) goto NotMatch;
			ct = ct->aux->xClass.variables->items.pVar[ opb ]->dtype;
			if( code == DVM_SETF_KG ){
				if( at != ct && ct->tid != DAO_ANY ) goto NotMatch;
				break;
			}
			AssertTypeMatching( at, ct, defs );
			if( at->tid != (DAO_INTEGER + (code - DVM_SETF_KGII)%3) ) goto NotMatch;
			if( ct->tid != (DAO_INTEGER + (code - DVM_SETF_KGII)/3) ) goto NotMatch;
			break;
		case DVM_SETF_OGII : case DVM_SETF_OGFF :
		case DVM_SETF_OGDD : case DVM_SETF_OGCC :
		case DVM_SETF_OG :
			if( ct == NULL ) goto ErrorTyping;
			if( types[opa] ==NULL || types[opc] ==NULL ) goto NotMatch;
			if( ct->tid != DAO_OBJECT ) goto NotMatch;
			ct = ct->aux->xClass.variables->items.pVar[ opb ]->dtype;
			if( code == DVM_SETF_OG ){
				if( at != ct && ct->tid != DAO_ANY ) goto NotMatch;
				break;
			}
			if( at->tid != ct->tid ) goto NotMatch;
			if( at->tid != (DAO_INTEGER + (code - DVM_SETF_OGII)) ) goto NotMatch;
			break;
		case DVM_SETF_OVII : case DVM_SETF_OVFF :
		case DVM_SETF_OVDD : case DVM_SETF_OVCC :
		case DVM_SETF_OV :
			if( ct == NULL ) goto ErrorTyping;
			if( types[opa] ==NULL || types[opc] ==NULL ) goto NotMatch;
			if( ct->tid != DAO_OBJECT ) goto NotMatch;
			ct = ct->aux->xClass.instvars->items.pVar[ opb ]->dtype;
			if( code == DVM_SETF_OV ){
				if( ct->tid == DAO_ANY ) break;
				if( DaoType_MatchTo( at, ct, NULL ) != DAO_MT_EQ ) goto NotMatch;
				/* Same type may be represented by different type objects by different namespaces; */
				/* if( at != ct && ct->tid != DAO_ANY ) goto NotMatch; */
				break;
			}
			if( at->tid != ct->tid ) goto NotMatch;
			if( at->tid != (DAO_INTEGER + (code - DVM_SETF_OVII)) ) goto NotMatch;
			break;
		case DVM_MATH_I :
		case DVM_MATH_F :
		case DVM_MATH_D :
			TT1 = DAO_INTEGER + (code - DVM_MATH_I);
			type = self->basicTypes[TT1];
			if( opa <= DVM_MATH_ABS ){
				ct = DaoInferencer_UpdateType( self, opc, type );
				if( bt->tid != TT1 || ct->tid != TT1 ) goto NotMatch;
			}else if( bt->tid == DAO_INTEGER || bt->tid == DAO_FLOAT ){
				ct = DaoInferencer_UpdateType( self, opc, dao_type_float );
				if( ct->tid != DAO_FLOAT ) goto NotMatch;
			}else{
				if( bt->tid == DAO_NONE || bt->tid > DAO_DOUBLE ) goto NotMatch;
				DaoInferencer_UpdateType( self, opc, dao_type_double );
				if( ct->tid != DAO_DOUBLE ) goto NotMatch;
			}
			break;
#endif
		default : break;
		}
	}
	DaoInferencer_Finalize( self );
	return 1;
NotMatch : return DaoInferencer_ErrorTypeNotMatching( self, NULL, NULL );
NotInit : return DaoInferencer_ErrorNotInitialized( self, 0, 0, 0 );
NotPermit : return DaoInferencer_Error( self, DTE_FIELD_NOT_PERMIT );
NotExist : return DaoInferencer_Error( self, DTE_FIELD_NOT_EXIST );
NeedInstVar : return DaoInferencer_Error( self, DTE_FIELD_OF_INSTANCE );
WrongContainer : return DaoInferencer_Error( self, DTE_TYPE_WRONG_CONTAINER );
ModifyConstant: return DaoInferencer_Error( self, DTE_CONST_WRONG_MODIFYING );
InvIndex : return DaoInferencer_Error( self, DTE_INDEX_NOT_VALID );
InvKey : return DaoInferencer_Error( self, DTE_KEY_NOT_VALID );
InvField : return DaoInferencer_Error( self, DTE_KEY_NOT_VALID );
InvOper : return DaoInferencer_Error( self, DTE_OPERATION_NOT_VALID );
InvParam : return DaoInferencer_Error( self, DTE_PARAM_ERROR );
CallNotPermit : return DaoInferencer_Error( self, DTE_CALL_NOT_PERMIT );
CallWithoutInst : return DaoInferencer_Error( self, DTE_CALL_WITHOUT_INSTANCE );
ErrorTyping: return DaoInferencer_Error( self, DTE_TYPE_NOT_MATCHING );
}
int DaoRoutine_DoTypeInference( DaoRoutine *self, int silent )
{
	DaoInferencer *inferencer;
	DaoOptimizer *optimizer;
	DaoVmSpace *vmspace = self->nameSpace->vmSpace;
	int retc, decorator = self->attribs & DAO_ROUT_DECORATOR;

	if( self->body->vmCodes->size == 0 ) return 1;

	optimizer = DaoVmSpace_AcquireOptimizer( vmspace );
	DArray_Resize( self->body->regType, self->body->regCount, NULL );
	if( ! decorator ) DaoOptimizer_RemoveUnreachableCodes( optimizer, self );

	inferencer = DaoVmSpace_AcquireInferencer( vmspace );
	DaoInferencer_Init( inferencer, self, silent );
	retc = DaoInferencer_DoInference( inferencer );
	DaoVmSpace_ReleaseInferencer( vmspace, inferencer );
	/*
	// Do not optimize decorators now, because there are reverved
	// registers for decoration, but not used in the codes.
	// Optimization may lose those registers, and lead to error
	// during decorator application.
	*/
	if( retc && ! decorator ) DaoOptimizer_Optimize( optimizer, self );
	/* DaoRoutine_PrintCode( self, self->nameSpace->vmSpace->errorStream ); */
	DaoVmSpace_ReleaseOptimizer( vmspace, optimizer );
	return retc;
}

#ifdef DAO_WITH_DECORATOR
/*
// Function decoration is done in the following way:
// 1. Use the decoration parameters to find the right decorator, if overloaded;
// 2. Use the decorator's parameter to determine the right (OLD) function, if overloaded;
// 3. The decorator function is copied to form the basis of the result (NEW) function;
// 4. Then the NEW function is adjusted to take the same parameters as the OLD function;
// 5. Arguments to the decorator are stored and accessed in the same way as local constants;
// 6. Code is added at the beginning of the NEW function to access decorator arguments;
// 7. Code is added to create a tuple (named args) from the parameters of the NEW function;
// 8. The registers from the decorate function will be mapped to higher indexes to reserve
//    indexes for the parameters of the NEW function.
*/
DaoRoutine* DaoRoutine_Decorate( DaoRoutine *self, DaoRoutine *decorator, DaoValue *p[], int n, int ip )
{
	int i, j, k, m, code, decolen, hasself = 0;
	int parpass[DAO_MAX_PARAM];
	DArray *annotCodes, *added = NULL, *regmap = NULL;
	DArray *nested, *ptypes;
	DaoValue *selfpar = NULL;
	DaoObject object, *obj = & object;
	DaoType *ftype, **decotypes;
	DaoRoutine *newfn, *oldfn = self;
	DaoVmCodeX *vmc;

	/* No in place decoration of overloaded function: */
	if( self->overloads && ip ) return NULL;
	if( self->overloads ){
		DArray *routs = DArray_New(0);
		for(i=0; i<self->overloads->routines->size; i++){
			DaoRoutine *rout = self->overloads->routines->items.pRoutine[i];
			rout = DaoRoutine_Decorate( rout, decorator, p, n, 0 );
			if( rout ) DArray_Append( routs, rout );
		}
		if( routs->size == 0 ){
			DArray_Delete( routs );
			return NULL;
		}else if( routs->size == 1 ){
			newfn = routs->items.pRoutine[0];
			DArray_Delete( routs );
			return newfn;
		}
		newfn = DaoRoutine_Copy( self, 0, 0, 1 );
		newfn->overloads = DRoutines_New();
		for(i=0; i<routs->size; i++) DRoutines_Add( newfn->overloads, routs->items.pRoutine[i] );
		DArray_Delete( routs );
		return newfn;
	}

	if( self->routHost ){
		/* To circumvent the default object issue for type matching: */
		object = *(DaoObject*) self->routHost->value;
		selfpar = (DaoValue*) obj;
	}

	decorator = DaoRoutine_Resolve( decorator, selfpar, p, n );
	if( decorator == NULL || decorator->type != DAO_ROUTINE ) return NULL;

	nested = decorator->routType->nested;
	decotypes = nested->items.pType;
	decolen = nested->size;
	if( decotypes[0]->attrib & DAO_TYPE_SELFNAMED ){
		/* Non-static decorator can only be applied to methods of the same class: */
		if( decorator->routHost != self->routHost ) return NULL;
		if( decolen == 1 ) return NULL;
		decotypes += 1;
		decolen -= 1;
		hasself = 1;
	}
	if( decolen == 0 ) return NULL;

	ftype = (DaoType*) decotypes[0]->aux;
	ptypes = ftype->nested;
	code = DVM_CALL + (ftype->attrib & DAO_TYPE_SELF);
	/* ftype->aux is NULL for type "routine": */
	if( ftype->aux )
		oldfn = DaoRoutine_ResolveByType( self, NULL, ptypes->items.pType, ptypes->size, code );
	if( oldfn == NULL ) return NULL;

	newfn = DaoRoutine_Copy( decorator, 1, 1, 1 );
	added = DArray_New(D_VMCODE);
	regmap = DArray_New(0);

	DArray_Resize( regmap, decorator->body->regCount + oldfn->parCount, 0 );
	for(i=0,m=decorator->body->regCount; i<m; i++) regmap->items.pInt[i] = i + oldfn->parCount;
	for(i=0,m=oldfn->parCount; i<m; i++) regmap->items.pInt[i + decorator->body->regCount] = i;

	DArray_Resize( newfn->body->regType, newfn->body->regCount + oldfn->parCount, NULL );
	for(i=0,m=oldfn->routType->nested->size; i<m; i++){
		DaoType *T = oldfn->routType->nested->items.pType[i];
		DaoType *T2 = newfn->body->regType->items.pType[i + newfn->body->regCount];
		if( T->tid == DAO_PAR_NAMED || T->tid == DAO_PAR_DEFAULT ) T = (DaoType*) T->aux;
		GC_ShiftRC( T, T2 );
		newfn->body->regType->items.pType[i + newfn->body->regCount] = T;
		/* DArray_Append( newfn->body->defLocals, oldfn->body->defLocals->items.pToken[i] ); */
	}
	newfn->body->regCount += oldfn->parCount;
	annotCodes = newfn->body->annotCodes;
	k = hasself;
	for(i=0; i<decolen; i++) parpass[i] = 0;
	for(i=0; i<n; i++){
		DaoValue *pv = p[i];
		if( i == 0 ){
			/*
			// This should be the function to be called from inside of the new function.
			// If the new function does not override the old one, the old function will be called;
			// Otherwise, the function bodies of the old and the new will be swapped,
			// and the new function will be called.
			*/
			pv = (DaoValue*)(ip ? newfn : oldfn);
		}
		if( pv->type == DAO_PAR_NAMED ){
			DaoNameValue *nameva = & pv->xNameValue;
			DNode *node = DMap_Find( decorator->routType->mapNames, nameva->name );
			if( node == NULL ) goto ErrorDecorator;
			pv = nameva->value;
			k = node->value.pInt;
		}
		parpass[k] = 1;
		DArray_PushBack( added, annotCodes->items.pVoid[0] );
		vmc = added->items.pVmc[added->size-1];
		vmc->code = DVM_GETCL;
		vmc->b = DaoRoutine_AddConstant( newfn, pv );
		vmc->c = k++;
	}
	for(i=1; i<decolen; i++){
		k = decotypes[i]->tid;
		if( k == DAO_PAR_VALIST ) break;
		if( parpass[i] ) continue;
		if( k != DAO_PAR_DEFAULT ) continue;
		DArray_PushBack( added, annotCodes->items.pVoid[0] );
		vmc = added->items.pVmc[added->size-1];
		vmc->code = DVM_GETCL;
		vmc->b = DaoRoutine_AddConstant( newfn, decorator->routConsts->items.items.pValue[i] );
		vmc->c = i;
	}
	DArray_PushBack( added, annotCodes->items.pVoid[0] ); /* XXX */
	vmc = added->items.pVmc[added->size-1];
	vmc->code = DVM_TUPLE;
	vmc->a = decorator->body->regCount;
	vmc->b = oldfn->parCount;
	vmc->c = decorator->parCount;
	if( vmc->b == 0 ) vmc->a = 0;
	for(i=0,m=annotCodes->size; i<m; i++){
		vmc = annotCodes->items.pVmc[i];
		k = DaoVmCode_GetOpcodeType( (DaoVmCode*) vmc );
		if( k == DAO_CODE_BRANCH || k == DAO_CODE_JUMP ) vmc->b += added->size;
	}
	DArray_InsertArray( annotCodes, 0, added, 0, added->size );
	DVector_Resize( newfn->body->vmCodes, annotCodes->size );
	for(i=0,m=annotCodes->size; i<m; i++){
		vmc = annotCodes->items.pVmc[i];
		newfn->body->vmCodes->data.codes[i] = *(DaoVmCode*) vmc;
	}

	GC_ShiftRC( oldfn->routType, newfn->routType );
	newfn->routType = oldfn->routType;
	newfn->parCount = oldfn->parCount;
	newfn->attribs = oldfn->attribs;
	DString_Assign( newfn->routName, oldfn->routName );
	/* Decorator should have reserved spaces for up to DAO_MAX_PARAM default parameters: */
	assert( newfn->routConsts->items.size >= DAO_MAX_PARAM );
	i = oldfn->routConsts->items.size;
	m = oldfn->parCount < i ? oldfn->parCount : i;
	for(i=0; i<m; i++){
		DaoValue *value = oldfn->routConsts->items.items.pValue[i];
		if( value ) DaoValue_Copy( value, newfn->routConsts->items.items.pValue + i );
	}

	DaoRoutine_UpdateRegister( newfn, regmap );
	if( DaoRoutine_DoTypeInference( newfn, 0 ) ==  0 ) goto ErrorDecorator;

	for(i=0,m=annotCodes->size; i<m; i++){
		vmc = annotCodes->items.pVmc[i];
		if( vmc->code == DVM_CALL && (vmc->b & DAO_CALL_DECSUB) ){
			/*
			// Call to the decorated function was marked with DAO_CALL_NOSELF
			// by the type inferencer, to not pass an implicit self parameter.
			// But for decorating constructors, it is necessary to pass the
			// implicit self parameter, because the self parameter is not in
			// the parameter list.
			*/
			if( oldfn->attribs & DAO_ROUT_INITOR ){
				vmc->b &= ~ DAO_CALL_NOSELF; /* Allow passing implicit self; */
				vmc->b |= DAO_CALL_INIT; /* Avoid resetting DAO_CALL_NOSELF; */
			}
			vmc->b &= ~ DAO_CALL_DECSUB;
			newfn->body->vmCodes->data.codes[i] = *(DaoVmCode*) vmc;
		}
	}
#if 0
	printf( "###################################\n" );
	printf( "################################### %s\n", oldfn->routName->mbs );
	printf( "###################################\n" );
	DaoRoutine_PrintCode( decorator, decorator->nameSpace->vmSpace->errorStream );
	DaoRoutine_PrintCode( newfn, newfn->nameSpace->vmSpace->errorStream );
#endif

	if( ip ){
		/* For in place decoration, override the old function by swapping the function
		// bodies and other associated data: */
		DaoRoutineBody *body = oldfn->body;
		DaoNamespace *ns = oldfn->nameSpace;
		DaoList *clist = oldfn->routConsts;
		oldfn->routConsts = newfn->routConsts;
		oldfn->nameSpace = newfn->nameSpace;
		oldfn->body = newfn->body;
		newfn->routConsts = clist;
		newfn->nameSpace = ns;
		newfn->body = body;
	}
	DArray_Delete( added );
	DArray_Delete( regmap );

	return newfn;
ErrorDecorator:
	if( added ) DArray_Delete( added );
	if( regmap ) DArray_Delete( regmap );
	DaoGC_TryDelete( (DaoValue*) newfn );
	return NULL;
}
#endif
