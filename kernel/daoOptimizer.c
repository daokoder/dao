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

#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<assert.h>

#include"daoGC.h"
#include"daoLexer.h"
#include"daoValue.h"
#include"daoRoutine.h"
#include"daoNamespace.h"
#include"daoVmspace.h"
#include"daoOptimizer.h"

extern DMutex mutex_routine_specialize;

DaoCodeNode* DaoCodeNode_New()
{
	DaoCodeNode *self = (DaoCodeNode*) dao_calloc( 1, sizeof(DaoCodeNode) );
	self->ins = DArray_New(0);
	self->outs = DArray_New(0);
	self->set = DHash_New(0,0);
	return self;
}
void DaoCodeNode_Delete( DaoCodeNode *self )
{
	DArray_Delete( self->ins );
	DArray_Delete( self->outs );
	DMap_Delete( self->set );
	dao_free( self );
}


DaoFlowGraph* DaoFlowGraph_New()
{
	DaoFlowGraph *self = (DaoFlowGraph*) dao_malloc( sizeof(DaoFlowGraph) );
	self->nodes = DArray_New(0);
	self->inits = DHash_New(0,0);
	self->finals = DHash_New(0,0);
	self->least = DHash_New(0,0);
	self->extreme = DHash_New(0,0);
	return self;
}
void DaoFlowGraph_Clear( DaoFlowGraph *self )
{
	DArray *nodes = self->nodes;
	daoint i;
	for(i=0; i<nodes->size; i++) DaoCodeNode_Delete( (DaoCodeNode*) nodes->items.pVoid[i] );
	DArray_Clear( nodes );
}
void DaoFlowGraph_Delete( DaoFlowGraph *self )
{
	DaoFlowGraph_Clear( self );
	DArray_Clear( self->nodes );
	dao_free( self );
}

typedef void (*TransferFunction)( DaoCodeNode*, DMap* );

static int DaoMF_Contains( DMap *A, DMap *B )
{
	DNode *it;
	for(it=DMap_First(B); it; it=DMap_Next(B,it)){
		if( DMap_Find( A, it->key.pVoid ) == NULL ) return 0;
	}
	return 1;
}
/* Transfer function for Live Variable Analysis: */
static void DaoMF_LVA( DaoCodeNode *node, DMap *out )
{
	ushort_t i;
	DMap_Assign( out, node->set );
	if( node->lvalue != 0xffff ) MAP_Erase( out, node->lvalue );
	switch( node->type ){
	case DAO_OP_SINGLE :
		MAP_Insert( out, node->first, 0 );
		break;
	case DAO_OP_PAIR   : 
		MAP_Insert( out, node->first, 0 );
		MAP_Insert( out, node->second, 0 );
		break;
	case DAO_OP_TRIPLE :
		MAP_Insert( out, node->first, 0 );
		MAP_Insert( out, node->second, 0 );
		MAP_Insert( out, node->third, 0 );
		break;
	case DAO_OP_RANGE :
		for(i=node->first; i<=node->second; i++) MAP_Insert( out, i, 0 );
		break;
	}
}
static void DaoMF_ProcessWorklist( DArray *worklist, TransferFunction trans, int reverse )
{
	daoint i;
	DNode *it;
	DMap *set = DHash_New(0,0);
	while( worklist->size ){
		DaoCodeNode *second = (DaoCodeNode*) DArray_PopBack( worklist );
		DaoCodeNode *first = (DaoCodeNode*) DArray_PopBack( worklist );
		DMap_Reset( set );
		(*trans)( first, set );
		if( DaoMF_Contains( second->set, set ) ) continue;
		for(it=DMap_First(set); it; it=DMap_Next(set,it)){
			DMap_Insert( second->set, it->key.pVoid, NULL );
		}
		if( reverse ){
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
	DMap_Delete( set );
}
static void DaoMF_SolveFlowEquation( DaoFlowGraph *flow, TransferFunction trans, int reverse )
{
	DMap *set = DHash_New(0,0);
	DArray *worklist = DArray_New(0);
	DaoCodeNode *node, **nodes = (DaoCodeNode**) flow->nodes->items.pVoid;
	daoint i, j, N = flow->nodes->size;
	for(i=0; i<N; i++){
		node = nodes[i];
		DMap_Reset( node->set );
		if( reverse ){
			if( DMap_Find( flow->finals, node ) ){
				DMap_Assign( node->set, flow->extreme );
			}else{
				DMap_Assign( node->set, flow->least );
			}
			for(j=0; j<node->outs->size; j++){
				DArray_PushBack( worklist, node->outs->items.pVoid[j] );
				DArray_PushBack( worklist, node );
			}
		}else{
			if( DMap_Find( flow->inits, node ) ){
				DMap_Assign( node->set, flow->extreme );
			}else{
				DMap_Assign( node->set, flow->least );
			}
			for(j=0; j<node->outs->size; j++){
				DArray_PushBack( worklist, node );
				DArray_PushBack( worklist, node->outs->items.pVoid[j] );
			}
		}
	}
	DaoMF_ProcessWorklist( worklist, trans, reverse );
	for(i=0; i<N; i++){
		node = nodes[i];
		DMap_Reset( set );
		(*trans)( node, set );
		DMap_Assign( node->set, set );
	}
	DArray_Delete( worklist );
	DMap_Delete( set );
}

void DaoRoutine_BuildFlowGraph( DaoRoutine *self, DaoFlowGraph *graph )
{
	DaoCodeNode *node, **nodes;
	DaoVmCode *vmc, *codes = self->body->vmCodes->codes;
	daoint i, j, k, N = self->body->vmCodes->size;
	DaoFlowGraph_Clear( graph );
	for(i=0; i<N; i++){
		node = DaoCodeNode_New();
		node->index = i;
		DaoRoutine_GetCodeOperands( self, & codes[i], node );
		DArray_Append( graph->nodes, node );
	}
	nodes = (DaoCodeNode**) graph->nodes->items.pVoid;
	DMap_Insert( graph->inits, nodes[0], NULL );
	for(i=0; i<N; i++){
		vmc = codes + i;
		node = nodes[i];
		if( i && vmc->code != DVM_CASE ){
			k = codes[i-1].code;
			if( k != DVM_GOTO && k != DVM_SAFE_GOTO && k != DVM_RETURN ){
				DArray_Append( nodes[i-1]->outs, node );
				DArray_Append( node->ins, nodes[i-1] );
			}
		}
		switch( vmc->code ){
		case DVM_TEST : case DVM_GOTO : case DVM_SAFE_GOTO :
		case DVM_TEST_I : case DVM_TEST_F : case DVM_TEST_D : 
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
		case DVM_CASE : 
			DArray_Append( node->outs, nodes[vmc->b] );
			DArray_Append( nodes[vmc->b]->ins, node );
			break;
		case DVM_JITC :
#warning"==================================="
			break;
		case DVM_CRRE :
#warning"==================================="
			break;
		case DVM_RETURN :
			DMap_Insert( graph->finals, node, NULL );
			break;
		default : break;
		}
	}
}

static void DaoRoutine_Optimize( DaoRoutine *self )
{
	DaoFlowGraph *flow = DaoFlowGraph_New();

	DaoRoutine_BuildFlowGraph( self, flow );
	DaoMF_SolveFlowEquation( flow, DaoMF_LVA, 1 );
}


void DaoRoutine_GetCodeOperands( DaoRoutine *self, DaoVmCode *vmc, DaoCodeNode *node )
{
	int k, m;

	node->type = DAO_OP_NONE;
	node->first = node->second = node->third = 0;
	node->lvalue = 0xffff;
	switch( vmc->code ){
	case DVM_NOP :
	case DVM_DEBUG :
		break;
	case DVM_DATA :
	case DVM_GETCL : case DVM_GETCK : case DVM_GETCG :
	case DVM_GETVO : case DVM_GETVK : case DVM_GETVG :
	case DVM_GETVH : case DVM_GETVL : 
		node->lvalue = vmc->c;
		break;
	case DVM_SETVH : case DVM_SETVL : 
	case DVM_SETVO : case DVM_SETVK : case DVM_SETVG :
		node->type = DAO_OP_SINGLE;
		node->first = vmc->a;
		break;
	case DVM_GETF : case DVM_GETMF : case DVM_GETDI :
	case DVM_LOAD : case DVM_CAST : case DVM_MOVE :
	case DVM_NOT : case DVM_UNMS : case DVM_BITREV :
	case DVM_ITER :
		node->type = DAO_OP_SINGLE;
		node->first = vmc->a;
		node->lvalue = vmc->c;
		break;
	case DVM_SETF : case DVM_SETMF : case DVM_SETDI :
		node->type = DAO_OP_PAIR;
		node->first = vmc->a;
		node->second = vmc->c;
		break;
	case DVM_SETI : case DVM_SETMI :
		node->type = DAO_OP_TRIPLE;
		node->first = vmc->a;
		node->second = vmc->b;
		node->third = vmc->c;
		break;
	case DVM_GETI : case DVM_GETMI :
	case DVM_ADD : case DVM_SUB : case DVM_MUL :
	case DVM_DIV : case DVM_MOD : case DVM_POW :
	case DVM_AND : case DVM_OR : case DVM_LT :
	case DVM_LE :  case DVM_EQ : case DVM_NE : case DVM_IN :
	case DVM_BITAND : case DVM_BITOR : case DVM_BITXOR :
	case DVM_BITLFT : case DVM_BITRIT :
	case DVM_PAIR : case DVM_CHECK :
		node->type = DAO_OP_PAIR;
		node->first = vmc->a;
		node->second = vmc->b;
		node->lvalue = vmc->c;
		break;
	case DVM_NAMEVA :
		node->type = DAO_OP_SINGLE;
		node->first = vmc->b;
		node->lvalue = vmc->c;
		break;
	case DVM_TUPLE : case DVM_MAP : case DVM_HASH :
		node->type = DAO_OP_RANGE;
		node->first = vmc->a;
		node->second = vmc->a + vmc->b - 1;
		node->lvalue = vmc->c;
		break;
	case DVM_LIST : case DVM_ARRAY :
		k = (vmc->b >= 10) ? (vmc->b - 10) : vmc->b;
		node->type = DAO_OP_RANGE;
		node->first = vmc->a;
		node->second = vmc->a + k - 1;
		node->lvalue = vmc->c;
		break;
	case DVM_CURRY : case DVM_MCURRY :
		node->type = DAO_OP_RANGE;
		node->first = vmc->a;
		node->second = vmc->a + vmc->b;
		node->lvalue = vmc->c;
		break;
	case DVM_CALL : case DVM_MCALL :
		k = vmc->b & 0xff;
		if( k == 0 && (vmc->b & DAO_CALL_EXPAR) ){ /* call with caller's parameter */
			k = self->parCount - (self->routType->attrib & DAO_TYPE_SELF) != 0;
		}
		node->type = DAO_OP_RANGE;
		node->first = vmc->a;
		node->second = vmc->a + k;
		node->lvalue = vmc->c;
		break;
	case DVM_MATRIX :
		k = vmc->b & 0xff;
		m = vmc->b >> 8;
		node->type = DAO_OP_RANGE;
		node->first = vmc->a;
		node->second = vmc->a + k*m - 1;
		node->lvalue = vmc->c;
		break;
	case DVM_SWITCH :
	case DVM_TEST :
	case DVM_TEST_I :
	case DVM_TEST_F :
	case DVM_TEST_D :
		node->type = DAO_OP_SINGLE;
		node->first = vmc->a;
		break;
	case DVM_CASE :
		break;
	case DVM_MATH :
		node->type = DAO_OP_SINGLE;
		node->first = vmc->a;
		node->lvalue = vmc->c;
		break;
	case DVM_ROUTINE :
	case DVM_CLASS :
#warning"==================================="
		//ClearChecking( node, checks );
		break;
	case DVM_CRRE :
#warning"==================================="
		break;
	case DVM_RETURN :
	case DVM_YIELD :
		node->type = DAO_OP_RANGE;
		node->first = vmc->a;
		node->second = vmc->a + vmc->b - 1;
		break;
	case DVM_DATA_I : case DVM_DATA_F : case DVM_DATA_D :
	case DVM_GETCL_I : case DVM_GETCL_F : case DVM_GETCL_D : 
	case DVM_GETCK_I : case DVM_GETCK_F : case DVM_GETCK_D : 
	case DVM_GETCG_I : case DVM_GETCG_F : case DVM_GETCG_D : 
	case DVM_GETVH_I : case DVM_GETVH_F : case DVM_GETVH_D : 
	case DVM_GETVL_I : case DVM_GETVL_F : case DVM_GETVL_D : 
	case DVM_GETVO_I : case DVM_GETVO_F : case DVM_GETVO_D : 
	case DVM_GETVK_I : case DVM_GETVK_F : case DVM_GETVK_D : 
	case DVM_GETVG_I : case DVM_GETVG_F : case DVM_GETVG_D : 
		node->lvalue = vmc->c;
		break;
	case DVM_SETVH_II : case DVM_SETVH_IF : case DVM_SETVH_ID : 
	case DVM_SETVH_FI : case DVM_SETVH_FF : case DVM_SETVH_FD : 
	case DVM_SETVH_DI : case DVM_SETVH_DF : case DVM_SETVH_DD : 
	case DVM_SETVL_II : case DVM_SETVL_IF : case DVM_SETVL_ID :
	case DVM_SETVL_FI : case DVM_SETVL_FF : case DVM_SETVL_FD :
	case DVM_SETVL_DI : case DVM_SETVL_DF : case DVM_SETVL_DD :
	case DVM_SETVO_II : case DVM_SETVO_IF : case DVM_SETVO_ID :
	case DVM_SETVO_FI : case DVM_SETVO_FF : case DVM_SETVO_FD :
	case DVM_SETVO_DI : case DVM_SETVO_DF : case DVM_SETVO_DD :
	case DVM_SETVK_II : case DVM_SETVK_IF : case DVM_SETVK_ID :
	case DVM_SETVK_FI : case DVM_SETVK_FF : case DVM_SETVK_FD :
	case DVM_SETVK_DI : case DVM_SETVK_DF : case DVM_SETVK_DD :
	case DVM_SETVG_II : case DVM_SETVG_IF : case DVM_SETVG_ID :
	case DVM_SETVG_FI : case DVM_SETVG_FF : case DVM_SETVG_FD :
	case DVM_SETVG_DI : case DVM_SETVG_DF : case DVM_SETVG_DD :
		node->type = DAO_OP_SINGLE;
		node->first = vmc->a;
		break;
	case DVM_MOVE_II : case DVM_MOVE_FF : case DVM_MOVE_DD :
	case DVM_MOVE_CC : case DVM_MOVE_SS : case DVM_MOVE_PP :
	case DVM_MOVE_IF : case DVM_MOVE_FI :
	case DVM_MOVE_ID : case DVM_MOVE_FD :
	case DVM_MOVE_DI : case DVM_MOVE_DF :
	case DVM_NOT_I : case DVM_UNMS_I : case DVM_BITREV_I :
	case DVM_NOT_F : case DVM_UNMS_F : case DVM_BITREV_F :
	case DVM_NOT_D : case DVM_UNMS_D : case DVM_BITREV_D :
	case DVM_UNMS_C : case DVM_CHECK_ST :
		node->type = DAO_OP_SINGLE;
		node->first = vmc->a;
		node->lvalue = vmc->c;
		break;
	case DVM_ADD_III : case DVM_SUB_III : case DVM_MUL_III : case DVM_DIV_III :
	case DVM_MOD_III : case DVM_POW_III : case DVM_AND_III : case DVM_OR_III  :
	case DVM_LT_III  : case DVM_LE_III  : case DVM_EQ_III : case DVM_NE_III :
	case DVM_BITAND_III : case DVM_BITOR_III  : case DVM_BITXOR_III :
	case DVM_BITLFT_III : case DVM_BITRIT_III :
	case DVM_ADD_FFF : case DVM_SUB_FFF : case DVM_MUL_FFF : case DVM_DIV_FFF :
	case DVM_MOD_FFF : case DVM_POW_FFF : case DVM_AND_FFF : case DVM_OR_FFF  :
	case DVM_LT_FFF  : case DVM_LE_FFF  : case DVM_EQ_FFF :
	case DVM_BITAND_FFF : case DVM_BITOR_FFF  : case DVM_BITXOR_FFF :
	case DVM_BITLFT_FFF : case DVM_BITRIT_FFF :
	case DVM_ADD_DDD : case DVM_SUB_DDD : case DVM_MUL_DDD : case DVM_DIV_DDD :
	case DVM_MOD_DDD : case DVM_POW_DDD : case DVM_AND_DDD : case DVM_OR_DDD  :
	case DVM_LT_DDD  : case DVM_LE_DDD  : case DVM_EQ_DDD :
	case DVM_BITAND_DDD : case DVM_BITOR_DDD  : case DVM_BITXOR_DDD :
	case DVM_BITLFT_DDD : case DVM_BITRIT_DDD :
	case DVM_ADD_CC : case DVM_SUB_CC : case DVM_MUL_CC : case DVM_DIV_CC :
	case DVM_ADD_SS : case DVM_LT_SS : case DVM_LE_SS :
	case DVM_EQ_SS : case DVM_NE_SS :
	case DVM_ADD_FNN : case DVM_SUB_FNN : case DVM_MUL_FNN : case DVM_DIV_FNN :
	case DVM_MOD_FNN : case DVM_POW_FNN : case DVM_AND_FNN : case DVM_OR_FNN  :
	case DVM_LT_FNN  : case DVM_LE_FNN  : case DVM_EQ_FNN :
	case DVM_BITLFT_FNN : case DVM_BITRIT_FNN :
	case DVM_ADD_DNN : case DVM_SUB_DNN : case DVM_MUL_DNN : case DVM_DIV_DNN :
	case DVM_MOD_DNN : case DVM_POW_DNN : case DVM_AND_DNN : case DVM_OR_DNN  :
	case DVM_LT_DNN  : case DVM_LE_DNN  : case DVM_EQ_DNN :
	case DVM_BITLFT_DNN  : case DVM_BITRIT_DNN  :
	case DVM_GETI_SI  : case DVM_GETI_LI :
	case DVM_GETI_LII : case DVM_GETI_LFI : case DVM_GETI_LDI :
	case DVM_GETI_AII : case DVM_GETI_AFI : case DVM_GETI_ADI :
	case DVM_GETI_LSI : case DVM_GETI_TI : case DVM_GETI_ACI :
		node->type = DAO_OP_PAIR;
		node->first = vmc->a;
		node->second = vmc->b;
		node->lvalue = vmc->c;
		break;
	case DVM_SETI_SII : case DVM_SETI_LI :
	case DVM_SETI_LIII : case DVM_SETI_LIIF : case DVM_SETI_LIID :
	case DVM_SETI_LFII : case DVM_SETI_LFIF : case DVM_SETI_LFID :
	case DVM_SETI_LDII : case DVM_SETI_LDIF : case DVM_SETI_LDID :
	case DVM_SETI_AIII : case DVM_SETI_AIIF : case DVM_SETI_AIID :
	case DVM_SETI_AFII : case DVM_SETI_AFIF : case DVM_SETI_AFID :
	case DVM_SETI_ADII : case DVM_SETI_ADIF : case DVM_SETI_ADID :
	case DVM_SETI_LSIS : case DVM_SETI_TI : case DVM_SETI_ACI :
		node->type = DAO_OP_TRIPLE;
		node->first = vmc->a;
		node->second = vmc->b;
		node->third = vmc->c;
		break;
	case DVM_GETF_T :
	case DVM_GETF_TI : case DVM_GETF_TF :
	case DVM_GETF_TD : case DVM_GETF_TS :
	case DVM_GETF_KC : case DVM_GETF_KG :
	case DVM_GETF_OC : case DVM_GETF_OG : case DVM_GETF_OV :
	case DVM_GETF_KCI : case DVM_GETF_KCF : case DVM_GETF_KCD : 
	case DVM_GETF_KGI : case DVM_GETF_KGF : case DVM_GETF_KGD :
	case DVM_GETF_OCI : case DVM_GETF_OCF : case DVM_GETF_OCD : 
	case DVM_GETF_OGI : case DVM_GETF_OGF : case DVM_GETF_OGD : 
	case DVM_GETF_OVI : case DVM_GETF_OVF : case DVM_GETF_OVD :
		node->type = DAO_OP_SINGLE;
		node->first = vmc->a;
		node->lvalue = vmc->c;
		break;
	case DVM_SETF_T :
	case DVM_SETF_TII : case DVM_SETF_TIF : case DVM_SETF_TID :
	case DVM_SETF_TFI : case DVM_SETF_TFF : case DVM_SETF_TFD :
	case DVM_SETF_TDI : case DVM_SETF_TDF : case DVM_SETF_TDD :
	case DVM_SETF_TSS :
	case DVM_SETF_KG : case DVM_SETF_OG : case DVM_SETF_OV :
	case DVM_SETF_KGII : case DVM_SETF_KGIF : case DVM_SETF_KGID : 
	case DVM_SETF_KGFI : case DVM_SETF_KGFF : case DVM_SETF_KGFD : 
	case DVM_SETF_KGDI : case DVM_SETF_KGDF : case DVM_SETF_KGDD : 
	case DVM_SETF_OGII : case DVM_SETF_OGIF : case DVM_SETF_OGID : 
	case DVM_SETF_OGFI : case DVM_SETF_OGFF : case DVM_SETF_OGFD : 
	case DVM_SETF_OGDI : case DVM_SETF_OGDF : case DVM_SETF_OGDD : 
	case DVM_SETF_OVII : case DVM_SETF_OVIF : case DVM_SETF_OVID :
	case DVM_SETF_OVFI : case DVM_SETF_OVFF : case DVM_SETF_OVFD :
	case DVM_SETF_OVDI : case DVM_SETF_OVDF : case DVM_SETF_OVDD :
		node->type = DAO_OP_PAIR;
		node->first = vmc->a;
		node->second = vmc->c;
		break;
	default: break;
	}
}


static int DaoRoutine_CheckType( DaoType *routType, DaoNamespace *ns, DaoType *selftype,
		DaoType *ts[], int np, int code, int def )
{
	int ndef = 0;
	int i, j, match = 1;
	int ifrom, ito;
	int parpass[DAO_MAX_PARAM];
	int npar = np, size = routType->nested->size;
	int selfChecked = 0, selfMatch = 0;
	DaoType  *abtp, **partypes = routType->nested->items.pType;
	DaoType **tps = ts;
	DNode *node;
	DMap *defs;

	defs = DMap_New(0,0);
	if( routType->nested ){
		ndef = routType->nested->size;
		if( ndef ){
			abtp = partypes[ ndef-1 ];
			if( abtp->tid == DAO_PAR_VALIST ) ndef = DAO_MAX_PARAM;
		}
	}

	/*
	   printf( "=====================================\n" );
	   for( j=0; j<npar; j++){
	   DaoType *tp = tps[j];
	   if( tp != NULL ) printf( "tp[ %i ]: %s\n", j, tp->name->mbs );
	   }
	   printf( "%s %i %i\n", routType->name->mbs, ndef, npar );
	   if( selftype ) printf( "%i\n", routType->name->mbs, ndef, npar, selftype );
	 */

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
			for(; ifrom<npar; ifrom++, ito++) parpass[ito] = 1;
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
			else if( tp->tid == DAO_ANY || tp->tid == DAO_UDF )
				parpass[ito] = DAO_MT_NEGLECT;
		}
		if( parpass[ito] == 0 ) goto FinishError;
		if( def ) tps[ifrom] = DaoType_DefineTypes( tp, ns, defs );
	}
	for(ito=0; ito<ndef; ito++){
		i = partypes[ito]->tid;
		if( i == DAO_PAR_VALIST ) break;
		if( parpass[ito] ) continue;
		if( i != DAO_PAR_DEFAULT ) goto FinishError;
		parpass[ito] = 1;
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
static DaoRoutine* MatchByParamType( DaoRoutine *self, DaoType *selftype, DaoType *ts[], int np, int code )
{
	DaoRoutine *rout = DaoRoutine_ResolveByType( self, selftype, ts, np, code );
	if( rout == (DaoRoutine*)self ){ /* parameters not yet checked: */
		if( DaoRoutine_CheckType( rout->routType, rout->nameSpace, selftype, ts, np, code, 0 ) ==0){
			rout = NULL;
		}
	}
	return rout;
}
void DaoRoutine_MapTypes( DaoRoutine *self, DMap *deftypes );
void DaoRoutine_PassParamTypes( DaoRoutine *self, DaoType *selftype, DaoType *ts[], int np, int code, DMap *defs )
{
	int npar = np;
	int ndef = self->parCount;
	int j, ifrom, ito;
	int selfChecked = 0;
	DaoType **tps = ts;
	DaoType  *abtp, *tp;
	DaoType **parType = self->routType->nested->items.pType;
	DMap *mapNames = self->routType->mapNames;
	DNode *node;

	if( npar == ndef && ndef == 0 ) return;
	if( code == DVM_MCALL && ! ( self->routType->attrib & DAO_TYPE_SELF ) ){
		npar --;
		tps ++;
	}else if( selftype && ( self->routType->attrib & DAO_TYPE_SELF) && code != DVM_MCALL ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		abtp = & self->routType->nested->items.pType[0]->aux->xType;
		if( DaoType_MatchTo( selftype, abtp, defs ) ){
			selfChecked = 1;
			DaoType_RenewTypes( selftype, self->nameSpace, defs );
		}
	}
	for(ifrom=0; ifrom<npar; ifrom++){
		ito = ifrom + selfChecked;
		if( ito >= (int)self->routType->nested->size ) break;
		if( ito < ndef && parType[ito]->tid == DAO_PAR_VALIST ) break;
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
	abtp = DaoType_DefineTypes( self->routType, self->nameSpace, defs );
	GC_ShiftRC( abtp, self->routType );
	self->routType = abtp;
	/*
	   printf( "tps1: %p %s\n", self->routType, self->routType->name->mbs );
	   for(node=DMap_First(defs);node;node=DMap_Next(defs,node)){
	   printf( "%i  %i\n", node->key.pType->tid, node->value.pType->tid );
	   printf( "%s  %s\n", node->key.pType->name->mbs, node->value.pType->name->mbs );
	   }
	 */
	for( j=0; j<npar; j++){
		abtp = DaoType_DefineTypes( tps[j], self->nameSpace, defs );
		GC_ShiftRC( abtp, tps[j] );
		tps[j] = abtp;
	}
	if( selftype && ( self->routType->attrib & DAO_TYPE_SELF) ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		abtp = self->routType->nested->items.pType[0];
		if( DaoType_MatchTo( selftype, abtp, defs ) )
			DaoType_RenewTypes( selftype, self->nameSpace, defs );
	}
	// XXX match the specialize routine type to the original routine type to setup type defs!
#if 0
	if( self->body ){
		DaoRoutine *rout = self;
		DMap *tmap = rout->body->localVarType;
		/* Only specialize explicitly declared variables: */
		for(node=DMap_First(tmap); node !=NULL; node = DMap_Next(tmap,node) ){
			DaoType *abtp = DaoType_DefineTypes( node->value.pType, rout->nameSpace, defs );
			GC_ShiftRC( abtp, rout->body->regType->items.pType[ node->key.pInt ] );
			rout->body->regType->items.pType[ node->key.pInt ] = abtp;
		}
	}
#endif
}
void DaoRoutine_PassParamTypes2( DaoRoutine *self, DaoType *selftype,
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
	if( npar == ndef && ndef == 0 ) return;
	if( code == DVM_MCALL && ! ( self->routType->attrib & DAO_TYPE_SELF ) ){
		npar --;
		tps ++;
	}else if( selftype && ( self->routType->attrib & DAO_TYPE_SELF) && code != DVM_MCALL ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		abtp = & self->routType->nested->items.pType[0]->aux->xType;
		if( DaoType_MatchTo( selftype, abtp, defs ) ) selfChecked = 1;
	}
	for(ifrom=0; ifrom<npar; ifrom++){
		ito = ifrom + selfChecked;
		if( ito >= (int)self->routType->nested->size ) break;
		if( ito < ndef && parType[ito]->tid == DAO_PAR_VALIST ) break;
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
	DTE_TYPE_NOT_CONSISTENT ,
	DTE_TYPE_NOT_MATCHING ,
	DTE_TYPE_NOT_INITIALIZED,
	DTE_TYPE_WRONG_CONTAINER ,
	DTE_DATA_CANNOT_CREATE ,
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
	"Inconsistent typing",
	"Types not matching",
	"Variable not initialized",
	"Wrong container type",
	"Data cannot be created",
	"Member not permited",
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
	DaoType *inumt = DaoNamespace_MakeType( ns, "int", DAO_INTEGER, NULL, NULL, 0 );
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
	if( setname && opa == opc && daoBitBoolArithOpers2[code-DVM_MOVE] ){
		DString_SetMBS( mbs, daoBitBoolArithOpers2[code-DVM_MOVE] );
		if( at->tid == DAO_INTERFACE ){
			node = DMap_Find( at->aux->xInterface.methods, mbs );
			rout = node->value.pRoutine;
		}else if( at->tid == DAO_OBJECT ){
			rout = DaoClass_FindOperator( & at->aux->xClass, mbs->mbs, hostClass );
		}else if( at->tid == DAO_CDATA ){
			rout = DaoType_FindFunction( at, mbs );
		}
		if( rout ){
			rout = MatchByParamType( rout, NULL, ts+1, 2, DVM_CALL );
			/* if the operation is used in the overloaded operator,
			   do operation by address */
			if( boolop && rout == self ) return inumt;
			if( rout ) return ct;
		}
	}
	if( setname ) DString_SetMBS( mbs, daoBitBoolArithOpers[code-DVM_MOVE] );
	if( at->tid == DAO_INTERFACE ){
		node = DMap_Find( at->aux->xInterface.methods, mbs );
		rout = node->value.pRoutine;
	}else if( at->tid == DAO_OBJECT ){
		rout = DaoClass_FindOperator( & at->aux->xClass, mbs->mbs, hostClass );
	}else if( at->tid == DAO_CDATA ){
		rout = DaoType_FindFunction( at, mbs );
	}
	if( rout ){
		rout2 = rout;
		rout = MatchByParamType( rout2, NULL, ts+1, 2, DVM_CALL );
		/* if the operation is used in the overloaded operator,
		   do operation by address */
		if( boolop && rout == self ) return inumt;
		if( rout ) ct = & rout->routType->aux->xType;
		if( rout == NULL && ct ) rout = MatchByParamType( rout2, NULL, ts, 3, DVM_CALL );
	}else{
		if( bt->tid == DAO_INTERFACE ){
			node = DMap_Find( bt->aux->xInterface.methods, mbs );
			rout = node->value.pRoutine;
		}else if( bt->tid == DAO_OBJECT ){
			rout = DaoClass_FindOperator( & bt->aux->xClass, mbs->mbs, hostClass );
		}else if( bt->tid == DAO_CDATA ){
			rout = DaoType_FindFunction( bt, mbs );
		}
		if( rout == NULL ) return NULL;
		rout2 = rout;
		rout = MatchByParamType( rout2, NULL, ts+1, 2, DVM_CALL );
		/* if the operation is used in the overloaded operator,
		   do operation by address */
		if( boolop && rout == self ) return inumt;
		if( rout ) ct = & rout->routType->aux->xType;
		if( rout == NULL && ct ) rout = MatchByParamType( rout2, NULL, ts, 3, DVM_CALL );
		if( rout == NULL ) return NULL;
	}
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
static DString* AppendError( DArray *errors, DaoRoutine *rout, size_t type )
{
	DString *s = DString_New(1);
	DArray_Append( errors, rout );
	DArray_Append( errors, s );
	DString_AppendMBS( s, DaoTypingErrorString[ type ] );
	DString_AppendMBS( s, " --- \" " );
	return s;
}
void DaoRoutine_CheckError( DaoRoutine *self, DaoType *selftype, DaoType *ts[], int np, int code, DArray *errors )
{
	DNode *node;
	DMap *defs = DHash_New(0,0);
	DaoType *routType = self->routType;
	DaoType *abtp, **partypes = routType->nested->items.pType;
	int npar = np, size = routType->nested->size;
	int i, j, ndef = 0;
	int ifrom, ito;
	int parpass[DAO_MAX_PARAM];
	int selfChecked = 0, selfMatch = 0;

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
		DString *s = AppendError( errors, self, DTE_PARAM_WRONG_NUMBER );
		DString_AppendMBS( s, "too many parameters \";\n" );
		goto FinishError;
	}

	for( j=selfChecked; j<ndef; j++) parpass[j] = 0;
	for(ifrom=0; ifrom<npar; ifrom++){
		DaoType *tp = ts[ifrom];
		ito = ifrom + selfChecked;
		if( ito < ndef && partypes[ito]->tid == DAO_PAR_VALIST ){
			for(; ifrom<npar; ifrom++) parpass[ifrom+selfChecked] = 1;
			break;
		}
		if( tp == NULL ){
			DString *s = AppendError( errors, self, DTE_PARAM_WRONG_TYPE );
			DString_AppendMBS( s, "unknown parameter type \";\n" );
			goto FinishError;
		}
		if( tp->tid == DAO_PAR_NAMED ){
			node = DMap_Find( routType->mapNames, tp->fname );
			if( node == NULL ){
				DString *s = AppendError( errors, self, DTE_PARAM_WRONG_NAME );
				DString_Append( s, tp->fname );
				DString_AppendMBS( s, " \";\n" );
				goto FinishError;
			}
			ito = node->value.pInt;
			tp = & tp->aux->xType;
		}
		if( tp ==NULL ){
			DString *s = AppendError( errors, self, DTE_PARAM_WRONG_TYPE );
			DString_AppendMBS( s, "unknown parameter type \";\n" );
			goto FinishError;
		}else if( ito >= ndef ){
			DString *s = AppendError( errors, self, DTE_PARAM_WRONG_NUMBER );
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
			else if( tp->tid == DAO_ANY || tp->tid == DAO_UDF )
				parpass[ito] = DAO_MT_NEGLECT;
		}
		if( parpass[ito] == 0 ){
			DString *s = AppendError( errors, self, DTE_PARAM_WRONG_TYPE );
			tp = ts[ifrom];
			abtp = routType->nested->items.pType[ito];
			abtp = DaoType_DefineTypes( abtp, self->nameSpace, defs );
			DString_AppendChar( s, '\'' );
			DString_Append( s, tp->name );
			DString_AppendMBS( s, "\' for \'" );
			DString_Append( s, abtp->name );
			DString_AppendMBS( s, "\' \";\n" );
			goto FinishError;
		}
	}
	for(ito=0; ito<ndef; ito++){
		i = partypes[ito]->tid;
		if( i == DAO_PAR_VALIST ) break;
		if( parpass[ito] ) continue;
		if( i != DAO_PAR_DEFAULT ){
			DString *s = AppendError( errors, self, DTE_PARAM_WRONG_NUMBER );
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
DaoRoutine* DaoValue_Check( DaoRoutine *self, DaoType *selftype, DaoType *ts[], int np, int code, DArray *errors )
{
	int i, n;
	DaoRoutine *rout = MatchByParamType( self, selftype, ts, np, code );
	if( rout ) return rout;
	if( self->overloads == NULL ){
		DaoRoutine_CheckError( self, selftype, ts, np, code, errors );
		return NULL;
	}
	for(i=0,n=self->overloads->routines->size; i<n; i++){
		DaoRoutine *rout = self->overloads->routines->items.pRoutine[i];
		/*
		   printf( "=====================================\n" );
		   printf("ovld %i, %p %s : %s\n", i, rout, self->routName->mbs, rout->routType->name->mbs);
		 */
		DaoRoutine_CheckError( rout, selftype, ts, np, code, errors );
	}
	return NULL;
}

void DaoPrintCallError( DArray *errors, DaoStream *stream )
{
	DString *mbs = DString_New(1);
	int i, k, n;
	for(i=0,n=errors->size; i<n; i+=2){
		DaoRoutine *rout = errors->items.pRoutine[i];
		DaoStream_WriteMBS( stream, "  ** " );
		DaoStream_WriteString( stream, errors->items.pString[i+1] );
		DaoStream_WriteMBS( stream, "     Assuming  : " );
		if( DaoToken_IsValidName( rout->routName->mbs, rout->routName->size ) ){
			DaoStream_WriteMBS( stream, "routine " );
		}else{
			DaoStream_WriteMBS( stream, "operator " );
		}
		k = DString_RFindMBS( rout->routType->name, "=>", rout->routType->name->size );
		DString_Assign( mbs, rout->routName );
		DString_AppendChar( mbs, '(' );
		DString_AppendDataMBS( mbs, rout->routType->name->mbs + 8, k-9 );
		DString_AppendChar( mbs, ')' );
		if( rout->routType->name->mbs[k+1] != '?' ){
			DString_AppendMBS( mbs, "=>" );
			DString_Append( mbs, rout->routType->aux->xType.name );
		}
		DString_AppendMBS( mbs, ";\n" );
		//DaoStream_WritePointer( stream, rout );
		DaoStream_WriteString( stream, mbs );
		DaoStream_WriteMBS( stream, "     Reference : " );
		if( rout->body ){
			DaoStream_WriteMBS( stream, "line " );
			DaoStream_WriteInt( stream, rout->defLine );
			DaoStream_WriteMBS( stream, ", " );
		}
		DaoStream_WriteMBS( stream, "file \"" );
		DaoStream_WriteString( stream, rout->nameSpace->name );
		DaoStream_WriteMBS( stream, "\";\n" );
		DString_Delete( errors->items.pString[i+1] );
	}
	DString_Delete( mbs );
}
int DaoRoutine_DoTypeInference( DaoRoutine *self, int silent )
{
#define NoCheckingType(t) ((t->tid==DAO_UDF)|(t->tid==DAO_ANY)|(t->tid==DAO_INITYPE))

#define AssertPairNumberType( tp ) \
	{ itp = (tp)->nested->items.pType[0]; \
	if( itp->tid == DAO_PAR_NAMED ) itp = & itp->aux->xType; \
	k = itp->tid; if( k == DAO_VALTYPE ) k = itp->aux->type; \
	if( k > DAO_DOUBLE && ! NoCheckingType(itp) ) goto NotMatch; \
	itp = (tp)->nested->items.pType[1]; \
	if( itp->tid == DAO_PAR_NAMED ) itp = & itp->aux->xType; \
	k = itp->tid; if( k == DAO_VALTYPE ) k = itp->aux->type; \
	if( k > DAO_DOUBLE && ! NoCheckingType(itp) ) goto NotMatch; }

#define InsertCodeMoveToInteger( opABC, opcode ) \
	{ vmc2.a = opABC; \
	opABC = self->body->regCount + addRegType->size -1; \
	addCount[i] ++; \
	vmc2.code = opcode; \
	vmc2.c = opABC; \
	DArray_Append( addCode, & vmc2 ); \
	DArray_Append( addRegType, inumt ); }

#define GOTO_ERROR( e1, e2, t1, t2 ) { ec_general = e1; ec_specific = e2; \
	type_source = t1; type_target = t2; goto ErrorTyping; }

#define ErrorTypeNotMatching( e1, t1, t2 ) \
	{ ec_general = e1; ec_specific = DTE_TYPE_NOT_MATCHING; \
		type_source = t1; type_target = t2; goto ErrorTyping; }

#define AssertTypeIdMatching( source, id, gerror ) \
	if( source->tid != id ){ \
		tid_target = id; ErrorTypeNotMatching( gerror, source, NULL ); }

#define AssertTypeMatching( source, target, defs, gerror ) \
	if( source->tid && source->tid != DAO_ANY && DaoType_MatchTo( source, target, defs ) ==0 ) \
		ErrorTypeNotMatching( gerror, source, target );

#define AssertInitialized( reg, ec, first, last ) \
	if( init[reg] == 0 || type[reg] == NULL ){ \
		annot_first = first; annot_last = last; ec_general = ec; \
		ec_specific = DTE_TYPE_NOT_INITIALIZED; goto ErrorTyping; }

#define AnnotateItemExpression( vmc, k ) \
	{k = DaoTokens_FindOpenToken( self->body->source, DTOK_COMMA, \
			vmc->first + k, vmc->first + vmc->last ); \
	if( k < 0 ) k = vmc->last - 1; else k -= vmc->first; }

#define UpdateType( id, tp ) \
	if( type[id] == NULL || (type[id]->attrib & DAO_TYPE_UNDEF) ){ \
		if( type[id] == NULL || DaoType_MatchTo( tp, type[id], NULL ) ){ \
			if( tp->attrib & DAO_TYPE_SPEC ) tp = DaoType_DefineTypes( tp, ns, defs ); \
			GC_ShiftRC( tp, type[id] ); \
			type[id] = tp; } }

	int typed_code = daoConfig.typedcode;
	int i, j, k, m, n, K, cid=0, retinf = 0;
	int N = self->body->vmCodes->size;
	int M = self->body->regCount;
	int min=0, spec=0;
	int TT1, TT2, TT3, TT6;
	int ec = 0, ec_general = 0, ec_specific = 0;
	int annot_first = 0, annot_last = 0, tid_target = 0;
	int tidHost = self->routHost ? self->routHost->tid : 0;
	ushort_t code;
	ushort_t opa, opb, opc;
	DaoNamespace *ns = self->nameSpace;
	DaoVmSpace *vms = ns->vmSpace;
	DaoType **tp, **type;
	DaoType *type_source = NULL, *type_target = NULL;
	DaoType *container = NULL, *indexkey = NULL, *itemvalue = NULL;
	DaoType *at, *bt, *ct, *itp, *tt, *ts[DAO_ARRAY+DAO_MAX_PARAM];
	DaoType *simtps[DAO_ARRAY], *arrtps[DAO_ARRAY];
	DaoType *inumt, *fnumt, *dnumt, *comt, *longt, *enumt, *strt;
	DaoType *ilst, *flst, *dlst, *slst, *iart, *fart, *dart, *cart, *any, *udf;
	DaoVmCodeX **vmcs = self->body->annotCodes->items.pVmc;
	DaoVmCodeX *vmc, vmc2;
	DaoStream  *stream = ns->vmSpace->errorStream;
	DaoRoutine *rout = NULL, *rout2;
	DaoRoutine *meth = NULL;
	DaoClass *hostClass = tidHost==DAO_OBJECT ? & self->routHost->aux->xClass:NULL;
	DaoClass *klass;
	DNode    *node;
	DMap    *defs, *defs2, *defs3;
	DString *str, *mbs, *error = NULL;
	DArray  *typeMaps;
	DArray  *rettypes; /* for code sections */
	DArray  *tparray, *errors = NULL;
	DArray  *vmCodeNew, *addCode, *addRegType;
	DArray  *regConst, *array;
	DArray  *routConsts = & self->routConsts->items;
	DArray  *dataCL[2] = { NULL, NULL };
	DArray  *typeVL[2] = { NULL, NULL };
	DArray  *typeVO[2] = { NULL, NULL };
	DArray  *CSS = hostClass ? hostClass->classes : NULL;
	DArray  *NSS = self->nameSpace->namespaces;
	DaoInteger integer = {DAO_INTEGER,0,0,0,1,0};
	DaoValue  *val = NULL;
	DaoValue **csts;
	DaoValue **pp;
	int notide = ! (vms->options & DAO_EXEC_IDE);
	char *init, char50[50], char200[200];
	int *addCount;
	/* To support Edit&Continue in DaoStudio, some of the features
	 * have to be switched off:
	 * (1) function specialization based on parameter types;
	 * (2) instruction specialization requiring
	 *     additional instructions and vm registers; */

	 dataCL[0] = & self->routConsts->items;
	 if( hostClass ) typeVO[0] = hostClass->objDataType;
	 if( self->body->upRoutine ){
		 if( self->body->upRoutine->body->parser ) DaoRoutine_Compile( self->body->upRoutine );
		 dataCL[1] = & self->body->upRoutine->routConsts->items;
		 typeVL[1] = self->body->upRoutine->body->regType;
	 }

	if( self->body->vmCodes->size ==0 ) return 1;
	defs = DHash_New(0,0);
	defs2 = DHash_New(0,0);
	defs3 = DHash_New(0,0);
	typeMaps = DArray_New(D_MAP);
	init = (char*) dao_malloc( self->body->regCount );
	memset( init, 0, self->body->regCount );
	addCount = (int*) dao_malloc( self->body->vmCodes->size * sizeof(int) );
	memset( addCount, 0, self->body->vmCodes->size * sizeof(int) );
	vmCodeNew = DArray_New( D_VMCODE );
	addCode = DArray_New( D_VMCODE );
	addRegType = DArray_New(0);
	array = DArray_New(0);
	mbs = DString_New(1);

	any = dao_type_any;
	udf = dao_type_udf;
	inumt = DaoNamespace_MakeType( ns, "int", DAO_INTEGER, NULL, NULL, 0 );
	fnumt = DaoNamespace_MakeType( ns, "float", DAO_FLOAT, NULL, NULL, 0 );
	dnumt = DaoNamespace_MakeType( ns, "double", DAO_DOUBLE, NULL, NULL, 0 );
	comt = DaoNamespace_MakeType( ns, "complex", DAO_COMPLEX, NULL, NULL, 0 );
	longt = DaoNamespace_MakeType( ns, "long", DAO_LONG, NULL, NULL, 0 );
	enumt = DaoNamespace_MakeType( ns, "enum", DAO_ENUM, NULL, NULL, 0 );
	strt = DaoNamespace_MakeType( ns, "string", DAO_STRING, NULL, NULL, 0 );
	ilst = DaoNamespace_MakeType( ns, "list", DAO_LIST, NULL, &inumt, 1 );
	flst = DaoNamespace_MakeType( ns, "list", DAO_LIST, NULL, &fnumt, 1 );
	dlst = DaoNamespace_MakeType( ns, "list", DAO_LIST, NULL, &dnumt, 1 );
	slst = DaoNamespace_MakeType( ns, "list", DAO_LIST, NULL, &strt, 1 );
	iart = DaoNamespace_MakeType( ns, "array", DAO_ARRAY, NULL, &inumt, 1 );
	fart = DaoNamespace_MakeType( ns, "array", DAO_ARRAY, NULL, &fnumt, 1 );
	dart = DaoNamespace_MakeType( ns, "array", DAO_ARRAY, NULL, &dnumt, 1 );
	cart = DaoNamespace_MakeType( ns, "array", DAO_ARRAY, NULL, &comt, 1 );

	memset( ts, 0, (DAO_ARRAY + DAO_MAX_PARAM)*sizeof(DaoType*) );
	ts[0] = simtps[0] = arrtps[0] = any;
	simtps[DAO_INTEGER] = inumt;
	simtps[DAO_FLOAT] = fnumt;
	simtps[DAO_DOUBLE] = dnumt;
	simtps[DAO_COMPLEX] = comt;
	simtps[DAO_LONG] = longt;
	simtps[DAO_ENUM] = enumt;
	simtps[DAO_STRING] = strt;
	arrtps[DAO_INTEGER] = iart;
	arrtps[DAO_FLOAT] = fart;
	arrtps[DAO_DOUBLE] = dart;
	arrtps[DAO_COMPLEX] = cart;
	ts[DAO_INTEGER] = inumt;
	ts[DAO_FLOAT] = fnumt;
	ts[DAO_DOUBLE] = dnumt;
	ts[DAO_COMPLEX] = comt;
	ts[DAO_LONG] = longt;
	ts[DAO_ENUM] = enumt;
	ts[DAO_STRING] = strt;

	GC_DecRCs( self->body->regType );
	regConst = DArray_New(0);
	DArray_Clear( self->body->regType );
	DArray_Resize( regConst, self->body->regCount, NULL );
	DArray_Resize( self->body->regType, self->body->regCount, 0 );
	type = self->body->regType->items.pType;
	csts = regConst->items.pValue;

	if( self->routName->mbs[0] == '@' && self->routType->nested->size ){
		DaoType *ftype = self->routType->nested->items.pType[0];
		if( ftype->tid == DAO_PAR_NAMED && ftype->aux->xType.tid == DAO_ROUTINE ){
			ftype = & ftype->aux->xType;
			for(i=0,n=ftype->nested->size; i<n; i++) init[i+self->parCount] = 1;
		}
	}
	for(i=0,n=self->routType->nested->size; i<n; i++){
		init[i] = 1;
		type[i] = self->routType->nested->items.pType[i];
		if( type[i] && type[i]->tid == DAO_PAR_VALIST ) type[i] = NULL;
		tt = type[i];
		if( tt ) type[i] = & tt->aux->xType; /* name:type, name=type */
		node = MAP_Find( self->body->localVarType, i );
		if( node == NULL ) continue;
		if( node->value.pType == NULL || type[i] == NULL ) continue;
		DaoType_MatchTo( type[i], node->value.pType, defs );
	}
	node = DMap_First( self->body->localVarType );
	for( ; node !=NULL; node = DMap_Next(self->body->localVarType,node) ){
		if( node->key.pInt < (int)self->routType->nested->size ) continue;
		type[ node->key.pInt ] = DaoType_DefineTypes( node->value.pType, ns, defs );
	}
	/* reference counts of types should be updated right in place,
	 * not at the end of this funciton), because function specialization
	 * may decrease the reference count of a type before it get increased! */
	DaoGC_IncRCs( self->body->regType );

	/*
	   printf( "DaoRoutine_DoTypeInference() %p %s %i %i\n", self, self->routName->mbs, self->parCount, self->body->regCount );
	   if( self->routType ) printf( "%p %p\n", hostClass, self->routType->aux );
	   DaoRoutine_PrintCode( self, self->nameSpace->vmSpace->errorStream );
	 */

	rettypes = DArray_New(0);
	errors = DArray_New(0);
	DArray_Append( rettypes, 0 );
	DArray_Append( rettypes, self->routType->aux );
	DArray_Append( rettypes, self->routType->aux );
	DArray_PushBack( typeMaps, defs );
	for(i=0; i<N; i++){
		/* adding type to namespace may add constant data as well */
		cid = i;
		error = NULL;
		vmc = vmcs[i];
		vmc2 = * self->body->annotCodes->items.pVmc[i];
		if( vmc->code == DVM_GETMI && vmc->b == 1 ){
			vmc->code = DVM_GETI;
			vmc->b = vmc->a + 1;
		}else if( vmc->code == DVM_SETMI && vmc->b == 1 ){
			vmc->code = DVM_SETI;
			vmc->b = vmc->c + 1;
		}
		code = vmc->code;
		opa = vmc->a;  opb = vmc->b;  opc = vmc->c;
		at = opa < M ? type[opa] : NULL;
		bt = opb < M ? type[opb] : NULL;
		ct = opc < M ? type[opc] : NULL;
		addCount[i] += i ==0 ? 0 : addCount[i-1];

		DMap_Reset( defs );
		DMap_Assign( defs, (DMap*)DArray_Back( typeMaps ) );

#if 0
		DaoTokens_AnnotateCode( self->body->source, vmc2, mbs, 24 );
		printf( "%4i: ", i );DaoVmCodeX_Print( vmc2, mbs->mbs );
#endif
		switch( code ){
		case DVM_NOP :
		case DVM_DEBUG :
			break;
		case DVM_DATA :
			if( csts[opc] ) goto ModifyConstant;
			if( opa > DAO_STRING ) GOTO_ERROR( DTE_DATA_CANNOT_CREATE, 0, NULL, NULL );
			init[opc] = 1;
			at = simtps[ opa ];
			if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ){
				UpdateType( opc, at );
			}else{
				AssertTypeMatching( at, type[opc], defs, 0);
			}
			val = NULL;
			switch( opa ){
			case DAO_NONE : val = dao_none_value; break;
			case DAO_INTEGER : val = (DaoValue*) DaoInteger_New( opb ); break;
			case DAO_FLOAT : val = (DaoValue*) DaoFloat_New( opb ); break;
			case DAO_DOUBLE : val = (DaoValue*) DaoDouble_New( opb ); break;
			}
			GC_ShiftRC( val, csts[opc] );
			csts[opc] = val;
			if( typed_code && at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
				vmc->code = DVM_DATA_I + (at->tid - DAO_INTEGER);
			}
			break;
		case DVM_GETCL : case DVM_GETCK : case DVM_GETCG :
			{
				if( code == DVM_GETCL ) val = dataCL[opa]->items.pValue[opb];
				else if( code == DVM_GETCK ) val = CSS->items.pClass[opa]->cstData->items.pValue[opb];
				else /* code == DVM_GETCG */ val = NSS->items.pNS[opa]->cstData->items.pValue[opb];
				at = DaoNamespace_GetType( ns, val );

				UpdateType( opc, at );
				/*
				   printf( "at %i %i %p, %p\n", at->tid, type[opc]->tid, at, type[opc] );
				 */
				AssertTypeMatching( at, type[opc], defs, 0);
				GC_ShiftRC( val, csts[opc] );
				csts[opc] = val;
				init[opc] = 1;
				if( typed_code && at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
					vmc->code = DVM_GETCL_I + 3*(code-DVM_GETCL) + (at->tid - DAO_INTEGER);
				}
				break;
			}
		case DVM_GETVH : case DVM_GETVL : case DVM_GETVO : case DVM_GETVK : case DVM_GETVG :
			{
				at = 0;
				if( code == DVM_GETVH ) at = type[opb];
				else if( code == DVM_GETVL ) at = typeVL[opa]->items.pType[opb];
				else if( code == DVM_GETVO ) at = typeVO[opa]->items.pType[opb];
				else if( code == DVM_GETVK ) at = CSS->items.pClass[opa]->glbDataType->items.pType[opb];
				else /* code == DVM_GETVG */ at = NSS->items.pNS[opa]->varType->items.pType[opb];
				if( at == NULL ) at = udf;
				UpdateType( opc, at );
				/*
				   printf( "%s\n", at->name->mbs );
				   printf( "%p %p\n", at, type[opc] );
				   printf( "%s %s\n", at->name->mbs, type[opc]->name->mbs );
				 */
				AssertTypeMatching( at, type[opc], defs, 0);
				init[opc] = 1;
				if( typed_code && at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
					vmc->code = DVM_GETVH_I + 3*(code-DVM_GETVH) + (at->tid - DAO_INTEGER);
				}
				break;
			}
		case DVM_SETVH : case DVM_SETVL : case DVM_SETVO : case DVM_SETVK : case DVM_SETVG :
			{
				AssertInitialized( opa, 0, vmc->middle + 1, vmc->last );
				tp = NULL;
				if( code == DVM_SETVH ) tp = type + opb;
				else if( code == DVM_SETVL ) tp = typeVL[opc]->items.pType + opb;
				else if( code == DVM_SETVO ) tp = typeVO[opc]->items.pType + opb;
				else if( code == DVM_SETVK ) tp = CSS->items.pClass[opc]->glbDataType->items.pType + opb;
				else /* code == DVM_SETVG */ tp = NSS->items.pNS[opc]->varType->items.pType + opb;
				at = type[opa];
				if( tp && ( *tp==NULL || (*tp)->tid ==DAO_UDF ) ){
					GC_ShiftRC( at, *tp );
					*tp = at;
				}
				/* less strict checking */
				if( type[opa]->tid == DAO_ANY || type[opa]->tid == DAO_UDF ) break;
				if( tp == 0 ) break;
				/*
				   printf( "%s %s\n", (*tp)->name->mbs, type[opa]->name->mbs );
				   printf( "ns=%p, tp=%p, tps=%p\n", ns, *tp, varTypes[opc] );
				   printf( "%i %i\n", tp[0]->tid - DAO_INTEGER, at->tid - DAO_INTEGER );
				 */
				k = DaoType_MatchTo( type[opa], *tp, defs );
				if( k ==0 ) ErrorTypeNotMatching( 0, type[opa], *tp );
				at = type[opa];
				if( tp[0]->tid >= DAO_INTEGER && tp[0]->tid <= DAO_DOUBLE
						&& at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
					if( typed_code ){
						if( code == DVM_SETVG ){
							DaoValue **p = NSS->items.pNS[opc]->varData->items.pValue + opb;
							if( *p == NULL ) *p = DaoValue_SimpleCopy( at->value );
						}
						vmc->code = 3*(tp[0]->tid - DAO_INTEGER) + (at->tid - DAO_INTEGER) + DVM_SETVH_II;
						vmc->code += 9*(code - DVM_SETVH);
					}
				}else if( k == DAO_MT_SUB && notide ){
					/* global L = { 1.5, 2.5 }; L = { 1, 2 }; L[0] = 3.5 */
					addCount[i] ++;
					vmc2.code = DVM_CAST;
					vmc2.a = opa;
					vmc2.c = self->body->regCount + addRegType->size;
					vmc->a = vmc2.c;
					DArray_Append( addCode, & vmc2 );
					DArray_Append( addRegType, *tp );
				}
				break;
			}
		case DVM_GETI :
		case DVM_GETDI :
			{
				val = csts[opa] ? dao_none_value : NULL;
				GC_ShiftRC( val, csts[opc] );
				csts[opc] = val;
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );

				integer.value = opb;
				val = (DaoValue*)(DaoInteger*)&integer;
				bt = inumt;
				if( code == DVM_GETI ){
					AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
					bt = type[opb];
					val = csts[opb];
				}
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				at = type[opa];
				ct = NULL;
				container = at;
				indexkey = bt;
				k = at->tid != DAO_CLASS && at->tid != DAO_OBJECT && at->tid != DAO_CDATA;
				if( val && val->type == 0 && k && bt->tid == DAO_VALTYPE ){ /* a[] */
					ct = at;
				}else if( NoCheckingType( at ) || NoCheckingType( bt ) ){
					/* allow less strict typing: */
					ct = udf;
				}else if( at->tid == DAO_INTEGER ){
					ct = inumt;
					if( bt->tid > DAO_DOUBLE ) goto InvIndex;
				}else if( at->tid == DAO_STRING ){
					ct = at;
					if( bt->tid >= DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
						ct = inumt;
						if( typed_code && code == DVM_GETI ){
							if( bt->tid == DAO_INTEGER ){
								vmc->code = DVM_GETI_SI;
							}else if( bt->tid == DAO_FLOAT && notide ){
								ct = inumt;
								vmc->code = DVM_GETI_SI;
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_IF );
							}else if( bt->tid == DAO_DOUBLE && notide ){
								ct = inumt;
								vmc->code = DVM_GETI_SI;
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_ID );
							}
						}
					}else if( bt == dao_type_for_iterator ){
						ct = inumt;
					}else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2 ){
						ct = at;
						AssertPairNumberType( bt );
					}else if( bt->tid ==DAO_LIST || bt->tid ==DAO_ARRAY ){
						/* passed */
						k = bt->nested->items.pType[0]->tid;
						if( k > DAO_DOUBLE && k !=DAO_ANY ) goto NotMatch;
					}
				}else if( at->tid == DAO_LONG ){
					ct = inumt; /* XXX slicing */
				}else if( at->tid == DAO_TYPE ){
					at = at->nested->items.pType[0];
					if( at->tid == DAO_ENUM && at->mapNames ){
						ct = at; /* TODO const index */
					}else{
						type_source = at;
						goto NotExist;
					}
				}else if( at->tid == DAO_LIST ){
					/*
					 */
					if( bt->tid == DAO_INTEGER || bt->tid == DAO_FLOAT
							|| bt->tid == DAO_DOUBLE ){
						ct = at->nested->items.pType[0];
						if( typed_code && notide && code == DVM_GETI ){
							if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE ){
								vmc->code = DVM_GETI_LII + ct->tid - DAO_INTEGER;
							}else if( ct->tid == DAO_STRING ){
								vmc->code = DVM_GETI_LSI;
							}else if( ct->tid >= DAO_ARRAY && ct->tid < DAO_ANY ){
								/* for skipping type checking */
								vmc->code = DVM_GETI_LI;
							}

							if( bt->tid == DAO_FLOAT ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_IF );
							}else if( bt->tid == DAO_DOUBLE ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_ID );
							}
						}
					}else if( bt == dao_type_for_iterator ){
						ct = at->nested->items.pType[0];
					}else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2 ){
						ct = at;
						AssertPairNumberType( bt );
					}else if( bt->tid ==DAO_LIST || bt->tid ==DAO_ARRAY ){
						ct = at;
						k = bt->nested->items.pType[0]->tid;
						if( k !=DAO_INTEGER && k !=DAO_FLOAT && k !=DAO_ANY && k !=DAO_UDF )
							goto NotMatch;
					}else{
						goto InvIndex;
					}
				}else if( at->tid == DAO_MAP ){
					DaoType *t0 = at->nested->items.pType[0];
					/*
					   printf( "at %s %s\n", at->name->mbs, bt->name->mbs );
					 */
					if( bt == dao_type_for_iterator ){
						ct = DaoNamespace_MakeType( ns, "tuple", DAO_TUPLE,
								NULL, at->nested->items.pType, 2 );
					}else{
						if( DaoType_MatchTo( bt, t0, defs ) ==0) goto InvKey;
						ct = at->nested->items.pType[1];
					}
				}else if( at->tid == DAO_ARRAY ){
					if( bt->tid == DAO_INTEGER || bt->tid == DAO_FLOAT
							|| bt->tid == DAO_DOUBLE ){
						/* array[i] */
						ct = at->nested->items.pType[0];
						if( typed_code && notide && code == DVM_GETI ){
							if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE )
								vmc->code = DVM_GETI_AII + ct->tid - DAO_INTEGER;
							else if( ct->tid == DAO_COMPLEX )
								vmc->code = DVM_GETI_ACI;

							if( bt->tid == DAO_FLOAT ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_IF );
							}else if( bt->tid == DAO_DOUBLE ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_ID );
							}
						}
					}else if( bt == dao_type_for_iterator ){
						ct = at->nested->items.pType[0];
					}else if( bt->tid ==DAO_TUPLE ){
						if( bt->nested->size != 2 ) goto InvIndex;
						ct = at;
						AssertPairNumberType( bt );
					}else if( bt->tid == DAO_LIST || bt->tid == DAO_ARRAY ){
						/* array[ {1,2,3} ] or array[ [1,2,3] ] */
						ct = at;
						k = bt->nested->items.pType[0]->tid;
						if( k !=DAO_INTEGER && k !=DAO_FLOAT && k !=DAO_ANY && k !=DAO_UDF )
							goto NotMatch;
					}
				}else if( at->tid ==DAO_TUPLE ){
					ct = udf;
					if( val && val->type ){
						if( val->type > DAO_DOUBLE ) goto InvIndex;
						k = DaoValue_GetInteger( val );
						if( k <0 || k >= (int)at->nested->size ) goto InvIndex;
						ct = at->nested->items.pType[ k ];
						if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
						if( typed_code ){
							if( k <= 0xffff ){
								if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE ){
									vmc->b = k;
									vmc->code = DVM_GETF_TI + ( ct->tid - DAO_INTEGER );
								}else if( ct->tid == DAO_STRING ){
									vmc->b = k;
									vmc->code = DVM_GETF_TS;
								}else/*XXX if( ct->tid >= DAO_ARRAY && ct->tid < DAO_ROUTINE )*/{
									/* for skipping type checking */
									vmc->b = k;
									vmc->code = DVM_GETF_T;
								}
							}
						}
					}else if( bt->tid >= DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
						if( typed_code && code == DVM_GETI && bt->tid == DAO_INTEGER ){
							vmc->code = DVM_GETI_TI;
						}else if( typed_code && notide && code == DVM_GETI ){
							vmc->code = DVM_GETI_TI;
							addCount[i] ++;
							vmc2.code = DVM_CAST;
							vmc2.a = opb;
							vmc2.b = 0;
							vmc2.c = self->body->regCount + addRegType->size;
							DArray_Append( addCode, & vmc2 );
							DArray_Append( addRegType, inumt );
							vmc->b = vmc2.c;
						}
					}else if( bt->tid != DAO_UDF && bt->tid != DAO_ANY ){
						goto InvIndex;
					}
				}else if( at->tid == DAO_OBJECT && (meth = DaoClass_FindOperator( & at->aux->xClass, "[]", hostClass )) ){
					rout = DaoValue_Check( meth, at, & bt, 1, DVM_CALL, errors );
					if( rout == NULL ) goto InvIndex;
					ct = & rout->routType->aux->xType;
				}else if( at->tid == DAO_CDATA ){
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
				}else if( at->tid == DAO_UDF || at->tid == DAO_ANY
						|| at->tid == DAO_INITYPE ){
					ct = udf;
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
				UpdateType( opc, ct );
				/*
				   DaoVmCodeX_Print( *vmc, NULL );
				   printf( "at %s %s %i\n", at->name->mbs, bt->name->mbs, at->tid );
				   if(ct) printf( "ct %s %s\n", ct->name->mbs, type[opc]->name->mbs );
				 */
				AssertTypeMatching( ct, type[opc], defs, 0);
				break;
			}
		case DVM_GETMI :
			{
				val = csts[opa] ? dao_none_value : NULL;;
				GC_ShiftRC( val, csts[opc] );
				csts[opc] = val;
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				k = DaoTokens_FindLeftPair( self->body->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
				for(j=0; j<opb; j++){
					AssertInitialized( opa+j+1, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
				}
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ct = at;
				meth = NULL;
				DString_SetMBS( mbs, "[]" );
				if( opb == 0 ){
					ct = at;
				}else if( NoCheckingType( at ) ){
					/* allow less strict typing: */
					ct = udf;
				}else if( at->tid == DAO_ARRAY ){
					ct = at->nested->items.pType[0];
					for(j=1; j<=opb; j++){
						int tid = type[j+opa]->tid;
						if( tid == DAO_VALTYPE ) tid = type[j]->aux->type;
						if( tid ==0 || tid > DAO_DOUBLE ){
							ct = at;
							break;
						}
					}
					if( ct != at ) vmc->code = DVM_GETMI_A;
				}else if( at->tid == DAO_MAP ){
					DaoType *t0 = at->nested->items.pType[0];
					int check1 = NoCheckingType( type[opa+1] ) == 0;
					int check2 = NoCheckingType( type[opa+2] ) == 0;
					if( type[opa+1]->tid == DAO_VALTYPE ) check1 = type[opa+1]->aux->type;
					if( type[opa+2]->tid == DAO_VALTYPE ) check2 = type[opa+2]->aux->type;
					/*
					   printf( "at %s %s\n", at->name->mbs, bt->name->mbs );
					 */
					ts[0] = type[opa+1];
					ts[1] = type[opa+2];
					if( ( check1 && DaoType_MatchTo( ts[0], t0, defs ) ==0 )
							|| ( check2 && DaoType_MatchTo( ts[1], t0, defs ) ==0 ) ){
						goto InvKey;
					}
				}else if( at->tid == DAO_CLASS || at->tid == DAO_OBJECT ){
					meth = DaoClass_FindOperator( & at->aux->xClass, "[]", hostClass );
					if( meth == NULL ) goto WrongContainer;
				}else if( at->tid == DAO_CDATA ){
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
					rout = DaoValue_Check( meth, at, type+opa+1, opb, DVM_CALL, errors );
					if( rout == NULL ) goto InvIndex;
					ct = & rout->routType->aux->xType;
				}
				UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0);
				break;
			}
		case DVM_GETF :
			{
				int ak = 0;
				val = csts[opa] ? dao_none_value : NULL;;
				GC_ShiftRC( val, csts[opc] );
				csts[opc] = val;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ct = NULL;
				val = routConsts->items.pValue[opb];
				if( val == NULL || val->type != DAO_STRING ) goto NotMatch;
				str = val->xString.data;
				at = type[opa];
				ak = at->tid ==DAO_CLASS;
				error = str;
				type_source = at;
				if( NoCheckingType( at ) ){
					/* allow less strict typing: */
					ct = udf;
				}else if( at->tid == DAO_TYPE ){
					at = at->nested->items.pType[0];
					type_source = at;
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
						if( node == NULL ) goto NotExist_TryAux;
						meth = node->value.pRoutine;
						rout = DaoValue_Check( meth, at, & bt, 0, DVM_CALL, errors );
						if( rout == NULL ) goto NotExist_TryAux;
						ct = & rout->routType->aux->xType;
					}
				}else if( at->tid == DAO_CLASS || at->tid == DAO_OBJECT ){
					int getter = 0;
					klass = & at->aux->xClass;
					tp = DaoClass_GetDataType( klass, str, & j, hostClass );
					if( j ){
						DString_SetMBS( mbs, "." );
						DString_Append( mbs, str );
						tp = DaoClass_GetDataType( klass, mbs, & j, hostClass );
						DaoClass_GetData( klass, mbs, & val, hostClass );
						if( j==0 && tp == NULL ) ct = DaoNamespace_GetType( ns, val );
						if( val && ct && ct->tid == DAO_ROUTINE ){
							rout2 = rout = (DaoRoutine*) val;
							rout = DaoValue_Check( rout, at, & bt, 0, DVM_CALL, errors );
							if( rout == NULL ) goto NotMatch;
							ct = & rout->routType->aux->xType;
							getter = 1;
							UpdateType( opc, ct );
							AssertTypeMatching( ct, type[opc], defs, 0);
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
					if( k == DAO_CLASS_VARIABLE ) csts[opc] = NULL;
					if( getter ) break;
					if( tp ==NULL ){
						DaoClass_GetData( klass, str, & val, hostClass );
						ct = DaoNamespace_GetType( ns, val );
						GC_ShiftRC( val, csts[opc] );
						csts[opc] = val;
					}else{
						ct = *tp;
					}
					j = DaoClass_GetDataIndex( klass, str );
					if( typed_code && LOOKUP_UP( j ) == 0 ){
						/* specialize instructions for finalized class/instance: */
						k = LOOKUP_ST( j );
						vmc->b = LOOKUP_ID( j );
						if( ct && ct->tid >=DAO_INTEGER && ct->tid <= DAO_DOUBLE ){
							if( k == DAO_CLASS_CONSTANT )
								vmc->code = ak ? DVM_GETF_KCI : DVM_GETF_OCI;
							else if( k == DAO_CLASS_VARIABLE )
								vmc->code = ak ? DVM_GETF_KGI : DVM_GETF_OGI;
							else if( k == DAO_OBJECT_VARIABLE )
								vmc->code = DVM_GETF_OVI;
							vmc->code += ct->tid - DAO_INTEGER;
						}else{
							if( k == DAO_CLASS_CONSTANT )
								vmc->code = ak ? DVM_GETF_KC : DVM_GETF_OC;
							else if( k == DAO_CLASS_VARIABLE )
								vmc->code = ak ? DVM_GETF_KG : DVM_GETF_OG;
							else if( k == DAO_OBJECT_VARIABLE )
								vmc->code = DVM_GETF_OV;
						}
					}
				}else if( at->tid == DAO_TUPLE ){
					if( at->mapNames == NULL ) goto NotExist_TryAux;
					node = MAP_Find( at->mapNames, str );
					if( node == NULL ) goto NotExist_TryAux;
					k = node->value.pInt;
					if( k <0 || k >= (int)at->nested->size ) goto NotExist_TryAux;
					ct = at->nested->items.pType[ k ];
					if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
					if( typed_code && notide ){
						if( k < 0xffff ){
							if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE ){
								vmc->code = DVM_GETF_TI + ( ct->tid - DAO_INTEGER );
								vmc->b = k;
							}else if( ct->tid == DAO_STRING ){
								vmc->b = k;
								vmc->code = DVM_GETF_TS;
							}else/*XXX if( ct->tid >= DAO_ARRAY && ct->tid < DAO_ROUTINE )*/{
								/* for skipping type checking */
								vmc->code = DVM_GETF_T;
								vmc->b = k;
							}
						}
					}
				}else if( at->tid == DAO_NAMESPACE ){
					ct = udf;
					if( csts[opa] && csts[opa]->type == DAO_NAMESPACE ){
						DaoNamespace *ans = & csts[opa]->xNamespace;
						k = DaoNamespace_FindVariable( ans, str );
						if( k >=0 ){
							ct = DaoNamespace_GetVariableType( ans, k );
						}else{
							k = DaoNamespace_FindConst( ans, str );
							val = DaoNamespace_GetConst( ans, k );
							if( val ) ct = DaoNamespace_GetType( ans, val );
						}
						if( k <0 ) goto NotExist_TryAux;
					}
#if 0
					//}else if( at->tid == DAO_ANY || at->tid == DAO_INITYPE ){
					//  ct = any;
#endif
			}else if( at->typer ){
				val = DaoType_FindValue( at, str );
				if( val && val->type == DAO_ROUTINE ){
					DaoRoutine *func = (DaoRoutine*) val;
					ct = func->routType;
					GC_ShiftRC( val, csts[opc] );
					csts[opc] = val;
				}else if( val ){
					ct = DaoNamespace_GetType( ns, val );
					GC_ShiftRC( val, csts[opc] );
					csts[opc] = val;
				}else{
					DString_SetMBS( mbs, "." );
					DString_Append( mbs, str );
					meth = DaoType_FindFunction( at, mbs );
					if( meth == NULL ) goto NotExist_TryAux;
					rout = DaoValue_Check( meth, at, & bt, 0, DVM_CALL, errors );
					if( rout == NULL ) goto NotMatch;
					ct = & rout->routType->aux->xType;
				}
				if( ct == NULL ) ct = udf;
			}
			UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0);
			break;
NotExist_TryAux:
			val = DaoType_FindAuxMethod( at, str, ns );
			if( val == NULL ) goto NotExist;
			ct = DaoNamespace_GetType( ns, val );
			GC_ShiftRC( val, csts[opc] );
			csts[opc] = val;
			UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0);
			break;
			}
		case DVM_SETI :
		case DVM_SETDI :
			{
				ct = (DaoType*) type[opc];
				if( ct == NULL ) goto ErrorTyping;
				if( csts[opc] ) goto ModifyConstant;
				k = DaoTokens_FindLeftPair( self->body->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
				AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );

				integer.value = opb;
				val = (DaoValue*)(DaoInteger*)&integer;
				bt = inumt;
				if( code == DVM_SETI ){
					AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
					bt = type[opb];
					val = csts[opb];
				}
				at = type[opa];
				container = ct;
				indexkey = bt;
				itemvalue = at;
				if( NoCheckingType(at) || NoCheckingType(bt) || NoCheckingType(ct) ) break;
				switch( ct->tid ){
				case DAO_STRING :
					if( typed_code && notide && code == DVM_SETI ){
						if( ( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE )
								&& ( bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE ) ){
							vmc->code = DVM_SETI_SII;
							if( at->tid ==DAO_FLOAT ){
								InsertCodeMoveToInteger( vmc->a, DVM_MOVE_IF );
							}else if( at->tid ==DAO_DOUBLE ){
								InsertCodeMoveToInteger( vmc->a, DVM_MOVE_ID );
							}
							if( bt->tid ==DAO_FLOAT ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_IF );
							}else if( bt->tid ==DAO_DOUBLE ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_ID );
							}
						}
					}
					/* less strict checking */
					if( at->tid >= DAO_ARRAY && at->tid !=DAO_ANY ) goto NotMatch;

					if( bt->tid==DAO_TUPLE && bt->nested->size == 2
							&& (at->tid==DAO_STRING || at->tid <= DAO_DOUBLE) ){
						/* passed */
						AssertPairNumberType( bt );
					}else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2
							&& (at->tid==DAO_STRING || at->tid <= DAO_DOUBLE) ){
						AssertPairNumberType( bt );
					}else if( bt->tid == DAO_LIST && at->tid <= DAO_DOUBLE ){
					}else if( bt->tid > DAO_DOUBLE && bt->tid !=DAO_ANY ){
						/* less strict checking */
						goto NotMatch;
					}
					break;
				case DAO_LONG :
					ct = inumt; /* XXX slicing */
					break;
				case DAO_LIST :
					ts[0] = ct->nested->items.pType[0];
					if( bt->tid >=DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
						ct = ct->nested->items.pType[0];
						AssertTypeMatching( at, ct, defs, 0);
						if( typed_code && notide && code == DVM_SETI ){
							if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE
									&& at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
								vmc->code = 3 * ( ct->tid - DAO_INTEGER ) + DVM_SETI_LIII
									+ at->tid - DAO_INTEGER;
							}else if( at->tid == DAO_STRING && ct->tid == DAO_STRING ){
								vmc->code = DVM_SETI_LSIS;
							}else{
								if( at == ct || ct->tid == DAO_ANY ) vmc->code = DVM_SETI_LI;
							}
							if( bt->tid ==DAO_FLOAT ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_IF );
							}else if( bt->tid ==DAO_DOUBLE ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_ID );
							}
						}
					}else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2 ){
						AssertPairNumberType( bt );
						AssertTypeMatching( at, ts[0], defs, 0);
					}else if( bt->tid==DAO_LIST || bt->tid==DAO_ARRAY ){
						k = bt->nested->items.pType[0]->tid;
						if( k !=DAO_INTEGER && k !=DAO_FLOAT && k !=DAO_ANY && k !=DAO_UDF )
							ErrorTypeNotMatching( 0, bt->nested->items.pType[0], inumt );
						AssertTypeMatching( at, ts[0], defs, 0);
					}else{
						ErrorTypeNotMatching( 0, bt, inumt );
					}
					break;
				case DAO_MAP :
					{
						DaoType *t0 = ct->nested->items.pType[0];
						DaoType *t1 = ct->nested->items.pType[1];
						/*
						   DaoVmCode_Print( *vmc, NULL );
						   printf( "%p %p %p\n", t0, t1, ct );
						   printf( "ct %p %s\n", ct, ct->name->mbs );
						   printf( "at %i %s %i %s\n", at->tid, at->name->mbs, t1->tid, t1->name->mbs );
						   printf( "bt %s %s\n", bt->name->mbs, t0->name->mbs );
						 */
						AssertTypeMatching( bt, t0, defs, 0);
						AssertTypeMatching( at, t1, defs, 0);
						break;
					}
				case DAO_ARRAY :
					if( bt->tid >=DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
						if( DaoType_MatchTo( at, ct, defs ) ) break;
						ct = ct->nested->items.pType[0];
						/* array[i] */
						if( typed_code && notide && code == DVM_SETI ){
							if( ct->tid >=DAO_INTEGER && ct->tid <= DAO_DOUBLE
									&& at->tid >=DAO_INTEGER && at->tid <= DAO_DOUBLE ){
								vmc->code = 3 * ( ct->tid - DAO_INTEGER ) + DVM_SETI_AIII
									+ at->tid - DAO_INTEGER;
							}else if( ct->tid == DAO_COMPLEX ){
								vmc->code = DVM_SETI_ACI;
							}else if( at->tid !=DAO_UDF && at->tid !=DAO_ANY ){
								AssertTypeMatching( at, ct, defs, 0);
							}
							if( bt->tid ==DAO_FLOAT ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_IF );
							}else if( bt->tid ==DAO_DOUBLE ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_ID );
							}
						}
						AssertTypeMatching( at, ct, defs, 0);
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
					if( val && val->type ){
						if( val->type > DAO_DOUBLE ) goto InvIndex;
						k = DaoValue_GetInteger( val );
						if( k <0 || k >= (int)ct->nested->size ) goto InvIndex;
						ct = ct->nested->items.pType[ k ];
						if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
						AssertTypeMatching( at, ct, defs, 0);
						if( typed_code ){
							if( k <= 0xffff && (at == ct || ct->tid == DAO_ANY) ){
								if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE
										&& at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
									vmc->code = DVM_SETF_TII + 3*( ct->tid - DAO_INTEGER )
										+ (at->tid - DAO_INTEGER);
									vmc->b = k;
								}else if( at->tid ==DAO_STRING && ct->tid ==DAO_STRING ){
									vmc->code = DVM_SETF_TSS;
									vmc->b = k;
								}else if( at->tid >= DAO_ARRAY && at->tid < DAO_ROUTINE ){
									vmc->code = DVM_SETF_T;
									vmc->b = k;
								}
							}
						}
					}else if( bt->tid >= DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
						if( typed_code && bt->tid == DAO_INTEGER ){
							vmc->code = DVM_SETI_TI;
						}else if( typed_code && notide ){
							vmc->code = DVM_SETI_TI;
							addCount[i] ++;
							vmc2.code = DVM_CAST;
							vmc2.a = opb;
							vmc2.c = self->body->regCount + addRegType->size;
							DArray_Append( addCode, & vmc2 );
							DArray_Append( addRegType, inumt );
							vmc->b = vmc2.c;
						}
					}else if( bt->tid != DAO_UDF && bt->tid != DAO_ANY ){
						goto InvIndex;
					}
					break;
				case DAO_CLASS :
				case DAO_OBJECT :
					if( (meth=DaoClass_FindOperator( & ct->aux->xClass, "[]=", hostClass )) == NULL)
						goto InvIndex;
					ts[0] = bt;
					ts[1] = at;
					k = 2;
					if( bt->tid == DAO_TUPLE ){
						if( bt->nested->size + 2 > DAO_MAX_PARAM ) goto InvIndex;
						for(k=0,K=bt->nested->size; k<K; k++) ts[k] = bt->nested->items.pType[k];
						ts[bt->nested->size] = at;
						k = bt->nested->size + 1;
					}
					rout = DaoValue_Check( meth, ct, ts, k, DVM_CALL, errors );
					if( rout == NULL ) goto InvIndex;
					break;
				case DAO_CDATA :
					DString_SetMBS( mbs, "[]=" );
					meth = DaoType_FindFunction( ct, mbs );
					if( meth == NULL ) goto InvIndex;
					ts[0] = bt;
					ts[1] = at;
					k = 2;
					if( bt->tid == DAO_TUPLE ){
						if( bt->nested->size + 2 > DAO_MAX_PARAM ) goto InvIndex;
						for(k=0,K=bt->nested->size; k<K; k++) ts[k] = bt->nested->items.pType[k];
						ts[bt->nested->size] = at;
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
				ct = (DaoType*) type[opc];
				if( ct == NULL ) goto ErrorTyping;
				if( csts[opc] ) goto ModifyConstant;
				k = DaoTokens_FindLeftPair( self->body->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
				AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				for(j=0; j<opb; j++){
					AssertInitialized( opc+j+1, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
				}
				at = type[opa];
				container = ct;
				itemvalue = at;
				if( NoCheckingType( at ) || NoCheckingType( ct ) ) break;
				meth = NULL;
				DString_SetMBS( mbs, "[]=" );
				if( ct->tid == DAO_ARRAY ){
					if( at->tid && at->tid < DAO_DOUBLE ) vmc->code = DVM_SETMI_A;
					for(j=1; j<=opb; j++){
						int tid = type[j+opc]->tid;
						if( tid == DAO_VALTYPE ) tid = type[j]->aux->type;
						if( tid ==0 || tid > DAO_DOUBLE ){
							vmc->code = DVM_SETMI;
							break;
						}
					}
				}else if( at->tid == DAO_MAP ){
					DaoType *t0 = at->nested->items.pType[0];
					DaoType *t1 = at->nested->items.pType[1];
					int check1 = NoCheckingType( type[opc+1] ) == 0;
					int check2 = NoCheckingType( type[opc+2] ) == 0;
					if( type[opc+1]->tid == DAO_VALTYPE ) check1 = type[opc+1]->aux->type;
					if( type[opc+2]->tid == DAO_VALTYPE ) check2 = type[opc+2]->aux->type;
					/*
					   printf( "at %s %s\n", at->name->mbs, bt->name->mbs );
					 */
					ts[0] = type[opc+1];
					ts[1] = type[opc+2];
					if( ( check1 && DaoType_MatchTo( ts[0], t0, defs ) ==0 )
							|| ( check2 && DaoType_MatchTo( ts[1], t0, defs ) ==0 ) ){
						goto InvKey;
					}
					AssertTypeMatching( at, t1, defs, 0);
				}else if( ct->tid == DAO_CLASS || ct->tid == DAO_OBJECT ){
					meth = DaoClass_FindOperator( & ct->aux->xClass, "[]=", hostClass );
					if( meth == NULL ) goto WrongContainer;
				}else if( ct->tid == DAO_CDATA ){
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
					memcpy( ts, type+opc+1, opb*sizeof(DaoType*) );
					ts[opb] = at;
					/* TODO, self type for class? */
					rout = DaoValue_Check( meth, ct, ts, opb+1, DVM_CALL, errors );
					if( rout == NULL ) goto InvIndex;
				}
				break;
			}
		case DVM_GETMF :
			{
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ct = udf;
				val = routConsts->items.pValue[opb];
				if( val == NULL || val->type != DAO_STRING ) goto NotMatch;
				UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0);
				val = csts[opa] ? dao_none_value : NULL;;
				GC_ShiftRC( val, csts[opc] );
				csts[opc] = val;
				break;
			}
		case DVM_SETF :
			{
				int ck;
				ct = type[opc];
				at = type[opa];
				if( csts[opc] && csts[opc]->type != DAO_CLASS && csts[opc]->type != DAO_NAMESPACE ) goto ModifyConstant;
				AssertInitialized( opa, 0, vmc->middle + 1, vmc->last );
				AssertInitialized( opc, 0, 0, vmc->middle - 1 );
				/*
				   printf( "a: %s\n", type[opa]->name->mbs );
				   printf( "c: %s\n", type[opc]->name->mbs );
				 */
				val = routConsts->items.pValue[opb];
				if( val == NULL || val->type != DAO_STRING ){
					printf( "field: %i\n", val ? val->type : 0 );
					goto NotMatch;
				}
				type_source = ct;
				str = val->xString.data;
				switch( ct->tid ){
				case DAO_UDF :
				case DAO_ANY :
				case DAO_INITYPE :
					/* allow less strict typing: */
					break;
				case DAO_CLASS :
				case DAO_OBJECT :
					{
						int setter = 0;
						ck = ct->tid ==DAO_CLASS;
						klass = & type[opc]->aux->xClass;
						tp = DaoClass_GetDataType( klass, str, & j, hostClass );
						if( STRCMP( str, "self" ) ==0 ) goto NotPermit;
						if( j ){
							DString_SetMBS( mbs, "." );
							DString_Append( mbs, str );
							DString_AppendMBS( mbs, "=" );
							tp = DaoClass_GetDataType( klass, mbs, & j, hostClass );
							DaoClass_GetData( klass, mbs, & val, hostClass );
							if( j==0 && tp == NULL ) ct = DaoNamespace_GetType( ns, val );
							if(  ct && ct->tid == DAO_ROUTINE ){
								meth = (DaoRoutine*) val;
								setter = 1;
								ts[0] = type[opc];
								ts[1] = at;
								rout = DaoValue_Check( meth, ct, ts, 2, DVM_MCALL, errors );
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
						if( tp ==NULL ) goto NotPermit;
						if( *tp ==NULL ) *tp = type[opa];
						AssertTypeMatching( type[opa], *tp, defs, 0);
						j = DaoClass_GetDataIndex( klass, str );
						if( typed_code && LOOKUP_UP( j ) == 0 ){
							k = LOOKUP_ST( j );
							vmc->b = LOOKUP_ID( j );
							if( *tp && (*tp)->tid>=DAO_INTEGER && (*tp)->tid<=DAO_DOUBLE
									&& at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE ){
								if( k == DAO_CLASS_VARIABLE )
									vmc->code = ck ? DVM_SETF_KGII : DVM_SETF_OGII;
								else if( k == DAO_OBJECT_VARIABLE )
									vmc->code = DVM_SETF_OVII;
								vmc->code += 3*((*tp)->tid - DAO_INTEGER) + (at->tid - DAO_INTEGER);
							}else if( at == *tp || (*tp)->tid == DAO_ANY ){
								if( k == DAO_CLASS_VARIABLE )
									vmc->code = ck ? DVM_SETF_KG : DVM_SETF_OG;
								else if( k == DAO_OBJECT_VARIABLE )
									vmc->code = DVM_SETF_OV;
							}
						}
						break;
					}
				case DAO_TUPLE :
					{
						if( ct->mapNames == NULL ) goto NotExist;
						node = MAP_Find( ct->mapNames, str );
						if( node == NULL ) goto NotExist;
						k = node->value.pInt;
						if( k <0 || k >= (int)ct->nested->size ) goto InvIndex;
						ct = ct->nested->items.pType[ k ];
						if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
						AssertTypeMatching( at, ct, defs, 0);
						if( typed_code && k < 0xffff && (at == ct || ct->tid == DAO_ANY) ){
							if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE
									&& at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
								vmc->code = DVM_SETF_TII + 3*( ct->tid - DAO_INTEGER )
									+ (at->tid - DAO_INTEGER);
								vmc->b = k;
							}else if( at->tid ==DAO_STRING && ct->tid ==DAO_STRING ){
								vmc->code = DVM_SETF_TSS;
								vmc->b = k;
							}else if( at->tid >= DAO_ARRAY && at->tid < DAO_ROUTINE ){
								vmc->code = DVM_SETF_T;
								vmc->b = k;
							}
						}
						break;
					}
				case DAO_NAMESPACE :
					{
						if( csts[opc] && csts[opc]->type == DAO_NAMESPACE ){
							DaoNamespace *ans = & csts[opc]->xNamespace;
							k = DaoNamespace_FindVariable( ans, str );
							if( k >=0 ){
								ct = DaoNamespace_GetVariableType( ans, k );
							}else{
								k = DaoNamespace_FindConst( ans, str );
								val = DaoNamespace_GetConst( ans, k );
								if( val ) ct = DaoNamespace_GetType( ans, val );
							}
							if( k <0 ) goto NotExist;
							AssertTypeMatching( at, ct, defs, 0);
						}
						break;
					}
				case DAO_CDATA :
					{
						DString_SetMBS( mbs, "." );
						DString_Append( mbs, str );
						DString_AppendMBS( mbs, "=" );
						meth = DaoType_FindFunction( ct, mbs );
						if( meth == NULL ) goto NotMatch;
						ts[0] = ct;
						ts[1] = at;
						rout = DaoValue_Check( meth, ct, ts, 2, DVM_MCALL, errors );
						if( rout == NULL ) goto NotMatch;
						break;
					}
				default: goto InvOper;
				}
				break;
			}
		case DVM_SETMF :
			{
				ct = type[opc];
				at = type[opa];
				if( csts[opc] ) goto ModifyConstant;
				AssertInitialized( opa, 0, vmc->middle + 1, vmc->last );
				AssertInitialized( opc, 0, 0, vmc->middle - 1 );
				/*
				   printf( "a: %s\n", type[opa]->name->mbs );
				   printf( "c: %s\n", type[opc]->name->mbs );
				 */
				val = routConsts->items.pValue[opb];
				if( val == NULL || val->type != DAO_STRING ) goto NotMatch;
				break;
			}
		case DVM_CAST :
			AssertInitialized( opa, 0, vmc->middle + 1, vmc->last );
			init[opc] = 1;
			if( routConsts->items.pValue[opb]->type != DAO_TYPE ) goto ErrorTyping;
			if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
			at = (DaoType*) routConsts->items.pValue[opb];
			UpdateType( opc, at );
			AssertTypeMatching( at, type[opc], defs, 0);
			at = type[opa];
			ct = type[opc];
			if( typed_code ){
				if( at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE
						&& ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE )
					vmc->code = DVM_MOVE_II + 3 * ( ct->tid - DAO_INTEGER )
						+ at->tid - DAO_INTEGER;
				else if( at->tid == DAO_COMPLEX && ct->tid == DAO_COMPLEX )
					vmc->code = DVM_MOVE_CC;
			}
			break;
		case DVM_LOAD :
			AssertInitialized( opa, 0, vmc->middle + 1, vmc->last );
			init[opc] = 1;
			at = type[opa];
			UpdateType( opc, at );
			AssertTypeMatching( at, type[opc], defs, 0);
			at = type[opa];
			ct = type[opc];
			break;
		case DVM_MOVE :
			{
				if( csts[opc] ) goto ModifyConstant;
				AssertInitialized( opa, 0, vmc->middle ? vmc->middle + 1 : 0, vmc->last );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				at = type[opa];
				UpdateType( opc, at );

				ct = type[opc];

				k = DaoType_MatchTo( at, type[opc], defs );

				/*
				   DaoVmCodeX_Print( *vmc, NULL );
				   if( type[opa] ) printf( "a: %s\n", type[opa]->name->mbs );
				   if( type[opc] ) printf( "c: %s\n", type[opc]->name->mbs );
				   printf( "%i  %i\n", DAO_MT_SUB, k );
				 */

#if 0
				if( csts[opa] && csts[opa]->type == DAO_ROUTREE && type[opc] && type[opc]->tid ==DAO_ROUTINE){
					/* a : routine<a:number,...> = overloaded_function; */
					//XXX rout = DRoutines_GetByType( (DaoRoutine*)csts[opa].v.routine, type[opc] );
					//if( rout == NULL ) goto NotMatch;
				}else 
#endif

				if( at->tid ==DAO_UDF || at->tid ==DAO_ANY ){
					/* less strict checking */
				}else if( at != ct && (ct->tid == DAO_OBJECT || ct->tid == DAO_CDATA) ){
					if( ct->tid == DAO_OBJECT ){
						meth = DaoClass_FindOperator( & ct->aux->xClass, "=", hostClass );
					}else{
						meth = DaoType_FindFunctionMBS( ct, "=" );
					}
					if( meth ){
						rout = DaoValue_Check( meth, ct, & at, 1, DVM_CALL, errors );
						if( rout == NULL ) goto NotMatch;
					}else if( k ==0 ){
						ErrorTypeNotMatching( 0, at, type[opc] );
					}
				}else if( at->tid ==DAO_TUPLE && DaoType_MatchTo(type[opc], at, defs)){
					/* less strict checking */
				}else if( k ==0 ){
					ErrorTypeNotMatching( 0, at, type[opc] );
				}

				/* necessary, because the register may be associated with a constant.
				 * beware of control flow: */
				if( vmc->b ){
					GC_ShiftRC( csts[opa], csts[opc] );
					csts[opc] = csts[opa];
				}

				if( k == DAO_MT_SUB && at != ct ){
					/* L = { 1.5, 2.5 }; L = { 1, 2 }; L[0] = 3.5 */
					vmc->code = DVM_CAST;
					break;
				}
				if( vmc->b == 0 ){
					ct = DaoType_DefineTypes( type[opc], ns, defs );
					if( ct ) UpdateType( opc, ct );
				}
				if( typed_code && k == DAO_MT_EQ && at == type[opc] ){
					if( at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE
							&& ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE ){
						vmc->code = DVM_MOVE_II + 3 * ( ct->tid - DAO_INTEGER )
							+ at->tid - DAO_INTEGER;
					}else if( at->tid == DAO_COMPLEX && ct->tid == DAO_COMPLEX ){
						vmc->code = DVM_MOVE_CC;
					}else if( at->tid == DAO_STRING && ct->tid == DAO_STRING ){
						vmc->code = DVM_MOVE_SS;
					}else if( at->tid >= DAO_ARRAY && csts[opa] == NULL ){
						vmc->code = DVM_MOVE_PP;
					}
				}
				break;
			}
		case DVM_ADD : case DVM_SUB : case DVM_MUL :
		case DVM_DIV : case DVM_MOD : case DVM_POW :
			{
				at = type[opa];
				bt = type[opb];
				if( csts[opc] ) goto ModifyConstant;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				init[opc] = 1;
				ct = NULL;
				/*
				   if( type[opa] ) printf( "a: %s\n", type[opa]->name->mbs );
				   if( type[opb] ) printf( "b: %s\n", type[opb]->name->mbs );
				   if( type[opc] ) printf( "c: %s\n", type[opc]->name->mbs );
				 */
				if( NoCheckingType( at ) || NoCheckingType( bt ) ){
					ct = udf;
				}else if( at->tid == DAO_OBJECT || bt->tid == DAO_OBJECT
						|| at->tid == DAO_CDATA || bt->tid == DAO_CDATA
						|| at->tid == DAO_INTERFACE || bt->tid == DAO_INTERFACE ){
					ct = type[opc];
					ct = DaoCheckBinArith( self, vmc, at, bt, ct, hostClass, mbs );
					if( ct == NULL ) goto InvOper;
				}else if( at->tid == bt->tid ){
					ct = at;
					switch( at->tid ){
					case DAO_INTEGER : case DAO_FLOAT : case DAO_DOUBLE :
					case DAO_LONG :
						break;
					case DAO_COMPLEX :
						if( code == DVM_MOD ) goto InvOper; break;
					case DAO_STRING :
						if( code != DVM_ADD ) goto InvOper; break;
					case DAO_ENUM :
						if( code != DVM_ADD && code != DVM_SUB ) goto InvOper;
						if( at->name->mbs[0] =='$' && bt->name->mbs[0] =='$' ){
							ct = NULL;
							if( code == DVM_ADD ){
								ct = DaoNamespace_SymbolTypeAdd( ns, at, bt, NULL );
							}else{
								ct = DaoNamespace_SymbolTypeSub( ns, at, bt, NULL );
							}
							if( ct == NULL ) goto InvOper;
						}else if( at->name->mbs[0] =='$' ){
							ct = bt;
						}
						break;
					case DAO_LIST :
						if( code != DVM_ADD ) goto InvOper;
						AssertTypeMatching( bt, at, defs, 0);
						break;
					case DAO_ARRAY :
						break;
					default : goto InvOper;
					}
				}else if( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE
						&& bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE ){
					ct = at->tid > bt->tid ? at : bt;
				}else if( ( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE )
						&& (bt->tid ==DAO_COMPLEX || bt->tid == DAO_LONG
							|| bt->tid ==DAO_ARRAY) ){
					ct = bt;
				}else if( (at->tid ==DAO_COMPLEX || at->tid == DAO_LONG
							|| at->tid ==DAO_ARRAY)
						&& ( bt->tid >= DAO_INTEGER && bt->tid <=DAO_DOUBLE ) ){
					ct = at;
				}else if( at->tid ==DAO_STRING && bt->tid ==DAO_INTEGER && opa==opc  ){
					ct = at;
				}else if( ( at->tid ==DAO_COMPLEX && bt->tid ==DAO_ARRAY )
						|| ( at->tid ==DAO_ARRAY && bt->tid ==DAO_COMPLEX ) ){
					ct = cart;
				}else{
					goto InvOper;
				}
				UpdateType( opc, ct );
				/* allow less strict typing: */
				if( ct->tid ==DAO_UDF || ct->tid == DAO_ANY ) continue;
				AssertTypeMatching( ct, type[opc], defs, 0 );
				ct = type[opc];
				if( i && typed_code ){
					if( at->tid == bt->tid && at->tid == ct->tid ){
						switch( at->tid ){
						case DAO_INTEGER :
							vmc->code += DVM_ADD_III - DVM_ADD; break;
						case DAO_FLOAT :
							vmc->code += DVM_ADD_FFF - DVM_ADD; break;
						case DAO_DOUBLE :
							vmc->code += DVM_ADD_DDD - DVM_ADD; break;
						case DAO_STRING :
							if( vmc->code == DVM_ADD ) vmc->code = DVM_ADD_SS;
							break;
						case DAO_COMPLEX :
							if( vmc->code <= DVM_DIV ) vmc->code += DVM_ADD_CC - DVM_ADD;
						}
					}else if( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE
							&& bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE ){
						switch( type[opc]->tid ){
						case DAO_FLOAT : vmc->code += DVM_ADD_FNN - DVM_ADD; break;
						case DAO_DOUBLE : vmc->code += DVM_ADD_DNN - DVM_ADD; break;
						default : break;
						}
					}
				}
				break;
			}
		case DVM_AND : case DVM_OR : case DVM_LT :
		case DVM_LE :  case DVM_EQ : case DVM_NE :
			{
				at = type[opa];
				bt = type[opb];
				if( csts[opc] ) goto ModifyConstant;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				init[opc] = 1;
				ct = inumt;
				if( NoCheckingType( at ) || NoCheckingType( bt ) ){
					ct = udf;
				}else if( at->tid == DAO_OBJECT || bt->tid == DAO_OBJECT
						|| at->tid == DAO_CDATA || bt->tid == DAO_CDATA
						|| at->tid == DAO_INTERFACE || bt->tid == DAO_INTERFACE ){
					ct = type[opc];
					ct = DaoCheckBinArith( self, vmc, at, bt, ct, hostClass, mbs );
					if( ct == NULL ) ct = inumt;
				}else if( at->tid == bt->tid ){
					ct = at;
					switch( at->tid ){
					case DAO_INTEGER : case DAO_FLOAT : case DAO_DOUBLE : // XXX ct = inumt;
						break;
					case DAO_COMPLEX :
						ct = inumt;
						if( code < DVM_EQ ) goto InvOper;
						break;
					case DAO_LONG :
					case DAO_STRING :
					case DAO_LIST :
					case DAO_ARRAY :
						if( code > DVM_OR ) ct = inumt; break;
					case DAO_ENUM :
						ct = inumt;
						break;
					case DAO_TUPLE :
						ct = inumt;
						if( code < DVM_LT ){
							ct = at;
							if( DaoType_MatchTo( at, bt, defs ) != DAO_MT_EQ ) ct = udf;
						}
						break;
					default :
						ct = inumt;
						if( code != DVM_EQ && code != DVM_NE ) goto InvOper;
					}
				}else if( at->tid >= DAO_INTEGER && at->tid <= DAO_LONG
						&& bt->tid >= DAO_INTEGER && bt->tid <= DAO_LONG
						&& at->tid != DAO_COMPLEX && bt->tid != DAO_COMPLEX ){
					if( code > DVM_OR ){
						ct = inumt;
					}else{
						ct = at->tid > bt->tid ? at : bt;
					}
				}else if( code != DVM_EQ && code != DVM_NE ){
					goto InvOper;
				}
				UpdateType( opc, ct );
				/* allow less strict typing: */
				if( ct->tid ==DAO_UDF || ct->tid == DAO_ANY ) continue;
				AssertTypeMatching( ct, type[opc], defs, 0 );
				ct = type[opc];
				if( i && typed_code ){
					if( at->tid == bt->tid && at->tid == ct->tid ){
						switch( at->tid ){
						case DAO_INTEGER :
							vmc->code += DVM_AND_III - DVM_AND; break;
						case DAO_FLOAT :
							vmc->code += DVM_AND_FFF - DVM_AND; break;
						case DAO_DOUBLE :
							vmc->code += DVM_AND_DDD - DVM_AND; break;
						case DAO_STRING :
							if( vmc->code >= DVM_LT ) vmc->code += DVM_LT_SS - DVM_LT;
							break;
						}
					}else if( at->tid == bt->tid && bt->tid == DAO_STRING && ct->tid == DAO_INTEGER ){
						if( vmc->code >= DVM_LT ) vmc->code += DVM_LT_SS - DVM_LT;
					}else if( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE
							&& bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE ){
						switch( type[opc]->tid ){
						case DAO_FLOAT : vmc->code += DVM_AND_FNN - DVM_AND; break;
						case DAO_DOUBLE : vmc->code += DVM_AND_DNN - DVM_AND; break;
						default : break;
						}
					}
				}
				break;
			}
		case DVM_IN :
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
			init[opc] = 1;
			ct = inumt;
			if( at->tid != DAO_ENUM && bt->tid == DAO_ENUM ) goto InvOper;
			if( NoCheckingType( bt ) ==0 ){
				if( bt->tid < DAO_STRING ) goto InvOper;
			}
			UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			break;
		case DVM_NOT : case DVM_UNMS : case DVM_BITREV :
			{
				/* force the result of DVM_NOT to be a number? */
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				if( csts[opc] ) goto ModifyConstant;
				UpdateType( opc, at );
				if( NoCheckingType( at ) ) continue;
				AssertTypeMatching( type[opa], type[opc], defs, 0 );
				ct = type[opc];
				/*
				   printf( "a: %s\n", type[opa]->name->mbs );
				   printf( "c: %s\n", type[opc]->name->mbs );
				 */
				if( typed_code ){
					if( at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE
							&& at->tid == type[opc]->tid ){
						vmc->code = 3 * ( vmc->code - DVM_NOT ) + DVM_NOT_I
							+ at->tid - DAO_INTEGER;
					}
				}
				break;
			}
		case DVM_BITAND : case DVM_BITOR : case DVM_BITXOR :
		case DVM_BITLFT : case DVM_BITRIT :
			{
				at = type[opa];
				bt = type[opb];
				if( csts[opc] ) goto ModifyConstant;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				init[opc] = 1;
				ct = NULL;
				if( at->tid == DAO_LIST ){
					if( code != DVM_BITLFT && code != DVM_BITRIT ) goto InvOper;
					ct = at;
					at = at->nested->items.pType[0];
					AssertTypeMatching( bt, at, defs, 0 );
					if( at->tid == DAO_UDF && bt->tid != DAO_UDF ){
						at = DaoNamespace_MakeType( ns, "list", DAO_LIST, NULL, & bt, at!=NULL );
						UpdateType( opa, at );
					}
				}else if( NoCheckingType( at ) || NoCheckingType( bt ) ){
					ct = udf;
				}else if( at->tid == DAO_OBJECT || bt->tid == DAO_OBJECT
						|| at->tid == DAO_CDATA || bt->tid == DAO_CDATA
						|| at->tid == DAO_INTERFACE || bt->tid == DAO_INTERFACE ){
					ct = DaoCheckBinArith( self, vmc, at, bt, ct, hostClass, mbs );
					if( ct == NULL ) goto InvOper;
				}else if( at->tid == bt->tid ){
					ct = at;
					if( at->tid > DAO_DOUBLE && at->tid != DAO_LONG ) goto InvOper;
				}else if( ((at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE)
							|| at->tid == DAO_LONG )
						&& ((bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE)
							|| bt->tid == DAO_LONG) ){
					ct = at->tid > bt->tid ? at : bt;
				}else{
					goto InvOper;
				}
				UpdateType( opc, ct );
				/* allow less strict typing: */
				if( ct->tid ==DAO_UDF || ct->tid == DAO_ANY ) continue;
				AssertTypeMatching( ct, type[opc], defs, 0 );
				ct = type[opc];
				if( typed_code ){
					if( at->tid == bt->tid && at->tid == ct->tid ){
						switch( at->tid ){
						case DAO_INTEGER :
							vmc->code += DVM_BITAND_III - DVM_BITAND; break;
						case DAO_FLOAT :
							vmc->code += DVM_BITAND_FFF - DVM_BITAND; break;
						case DAO_DOUBLE :
							vmc->code += DVM_BITAND_DDD - DVM_BITAND; break;
						}
					}else if( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE
							&& bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE ){
						if( code == DVM_BITLFT || code == DVM_BITRIT ){
							switch( type[opc]->tid ){
							case DAO_FLOAT : vmc->code += DVM_BITLFT_FNN - DVM_BITLFT; break;
							case DAO_DOUBLE : vmc->code += DVM_BITLFT_DNN - DVM_BITLFT; break;
							default : break;
							}
						}
					}
				}
				break;
			}
		case DVM_CHECK :
		case DVM_CHECK_ST :
			{
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				init[opc] = 1;
				UpdateType( opc, inumt );
				AssertTypeMatching( inumt, type[opc], defs, 0 );
				bt = type[opb];
				ct = type[opc];
				k = bt->tid == DAO_TYPE ? bt->nested->items.pType[0]->tid : 0;
				if( k && k <= DAO_STRING && ct->tid == DAO_INTEGER ){
					vmc->code = DVM_CHECK_ST;
				}else if( code == DVM_CHECK_ST ){
					goto ErrorTyping;
				}
				break;
			}
		case DVM_NAMEVA :
			{
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ct = DaoNamespace_MakeType( ns, routConsts->items.pValue[opa]->xString.data->mbs,
						DAO_PAR_NAMED, (DaoValue*) type[opb], 0, 0 );
				UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_PAIR :
			{
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ct = DaoNamespace_MakePairType( ns, type[opa], type[opb] );
				UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_TUPLE :
			{
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ct = DaoType_New( "tuple<", DAO_TUPLE, NULL, NULL );
				k = 1;
				for(j=0; j<opb; j++){
					at = type[opa+j];
					val = csts[opa+j];
					AnnotateItemExpression( vmc, k );
					AssertInitialized( opa+j, 0, 0, k );
					if( j >0 ) DString_AppendMBS( ct->name, "," );
					if( at->tid == DAO_PAR_NAMED ){
						if( ct->mapNames == NULL ) ct->mapNames = DMap_New(D_STRING,0);
						MAP_Insert( ct->mapNames, at->fname, j );
						DString_Append( ct->name, at->name );
					}else{
						DString_Append( ct->name, at->name );
					}
					DArray_Append( ct->nested, at );
				}
				DString_AppendMBS( ct->name, ">" );
				GC_IncRCs( ct->nested );
				bt = DaoNamespace_FindType( ns, ct->name );
				if( bt ){
					DaoType_Delete( ct );
					ct = bt;
				}else{
					DaoType_CheckAttributes( ct );
					DaoType_InitDefault( ct );
					DaoNamespace_AddType( ns, ct->name, ct );
				}
				UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_LIST : case DVM_ARRAY :
			{
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				at = udf;
				if( opb >= 11 ){
					at = type[opa];
					for(j=1; j<opb-10; j++){
						if( DaoType_MatchTo( type[opa+j], at, defs )==0 ){
							at = any;
							break;
						}
						if( at->tid < type[opa+j]->tid ) at = type[opa+j];
					}
					if( code == DVM_ARRAY && at )
						if( at->tid ==0 || at->tid > DAO_COMPLEX ) at = fnumt;
				}else{
					if( opb == 2 ){
						int a = type[opa]->tid;
						int b = type[opa+1]->tid;
						at = type[opa];
						/* only allow {[ number : number ]} */
						if( ( a < DAO_INTEGER && a > DAO_DOUBLE ) ||
								( b < DAO_INTEGER && b > DAO_DOUBLE ) )
							goto ErrorTyping;
					}else if( opb == 3 ){
						at = type[opa];
						if( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE ){
							if( type[opa+1]->tid < DAO_INTEGER
									|| type[opa+1]->tid > DAO_DOUBLE ) goto ErrorTyping;
						}else if( at->tid ==DAO_COMPLEX ){
							if( type[opa+1]->tid < DAO_INTEGER
									|| type[opa+1]->tid > DAO_COMPLEX ) goto ErrorTyping;
						}else if( at->tid == DAO_STRING ){
							if( type[opa+1]->tid != DAO_STRING ) goto ErrorTyping;
						}else if( at->tid == DAO_ARRAY ){
							/* XXX */
						}else{
							goto ErrorTyping;
						}
						if( type[opa+2]->tid < DAO_INTEGER
								|| type[opa+2]->tid > DAO_DOUBLE ) goto ErrorTyping;
						if( vmc->code ==DVM_ARRAY && at->tid ==DAO_STRING ) goto ErrorTyping;
					}else{
						at = udf;
					}
				}
				if( vmc->code == DVM_LIST )
					ct = DaoNamespace_MakeType( ns, "list", DAO_LIST, NULL, &at, at!=NULL );
				else if( at && at->tid >=DAO_INTEGER && at->tid <= DAO_COMPLEX )
					ct = arrtps[ at->tid ];
				else if( at && at->tid == DAO_ARRAY )
					ct = at;
				else
					ct = DaoNamespace_MakeType( ns, "array", DAO_ARRAY,NULL, &at, at!=NULL );
				/* else goto ErrorTyping; */
				UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_MAP :
		case DVM_HASH :
			{
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ts[0] = ts[1] = udf;
				if( opb > 0 ){
					ts[0] = type[opa];
					ts[1] = type[opa+1];
					for(j=2; j<opb; j+=2){
						if( DaoType_MatchTo( type[opa+j], ts[0], defs ) ==0 ) ts[0] = NULL;
						if( DaoType_MatchTo( type[opa+j+1], ts[1], defs ) ==0 ) ts[1] = NULL;
						if( ts[0] ==NULL && ts[1] ==NULL ) break;
					}
				}
				if( ts[0] ==NULL ) ts[0] = opb ? any : udf;
				if( ts[1] ==NULL ) ts[1] = opb ? any : udf;
				ct = DaoNamespace_MakeType( ns, "map", DAO_MAP, NULL, ts, 2 );
				UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_MATRIX :
			{
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				k = ( (0xff00 & vmc->b )>>8 ) * ( 0xff & vmc->b );
				at = udf;
				if( k >0 ){
					at = type[opa];
					for( j=0; j<k; j++){
						min = type[opa+j]->tid;
						if( min == 0 || min > DAO_COMPLEX ) goto ErrorTyping;
						if( type[opa+j]->tid > at->tid ) at = type[opa+j];
					}
				}
				ct = DaoNamespace_MakeType( ns, "array", DAO_ARRAY,NULL,&at, at!=NULL );
				UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_CURRY :
		case DVM_MCURRY :
			{
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				at = type[opa];
				ct = NULL;
				if( at->tid == DAO_TYPE ) at = at->nested->items.pType[0];
				if( at->tid == DAO_ROUTINE ){
					ct = DaoNamespace_MakeType( ns, "curry", DAO_FUNCURRY, NULL, NULL, 0 );
				}else if( at->tid == DAO_CLASS ){
					if( csts[opa] == NULL ) goto NotInit;
					klass = & at->aux->xClass;
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
						bt = type[opa+j];
						val = csts[opa+j];
						if( bt == NULL ) goto ErrorTyping;
						AssertInitialized( opa+j, 0, 0, vmc->middle - 1 );
						if( bt->tid == DAO_PAR_NAMED ){
							if( at->mapNames == NULL ) goto InvField;
							node = MAP_Find( at->mapNames, bt->fname );
							if( node == NULL || node->value.pInt != j-1 ) goto InvField;
							bt = & bt->aux->xType;
						}
						tt = at->nested->items.pType[j-1];
						if( tt->tid == DAO_PAR_NAMED ) tt = & tt->aux->xType;
						AssertTypeMatching( bt, tt, defs, 0 );
					}
				}else{
					ct = udf;
				}
				UpdateType( opc, ct );
				if( at->tid == DAO_ANY || at->tid == DAO_UDF ) break;
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_SWITCH :
			AssertInitialized( opa, 0, vmc->middle + 2, vmc->last-1 );
			j = 0;
			for(k=1; k<=opc; k++){
				DaoValue *cc = routConsts->items.pValue[ vmcs[i+k]->a ];
				j += (cc && cc->type == DAO_ENUM && cc->xEnum.etype->name->mbs[0] == '$');
				bt = DaoNamespace_GetType( ns, cc );
				if( at->name->mbs[0] == '$' && bt->name->mbs[0] == '$' ) continue;
				if( DaoType_MatchValue( at, cc, defs ) ==0 ){
					cid = i + k;
					vmc = vmcs[i + k];
					type_source = DaoNamespace_GetType( ns, cc );
					type_target = at;
					ec_specific = DTE_TYPE_NOT_MATCHING;
					goto ErrorTyping;
				}
			}
			if( csts[opa] && csts[opa]->type ){
				DaoValue *sv = csts[opa];
				int jump = opb;
				for(k=1; k<=opc; k++){
					DaoValue *cc = routConsts->items.pValue[ vmcs[i+k]->a ];
					if( DaoValue_Compare( sv, cc ) ==0 ){
						jump = vmcs[i+k]->b;
						break;
					}
				}
				vmc->code = DVM_GOTO;
				vmc->b = jump;
			}else if( at->tid == DAO_ENUM && at->name->mbs[0] != '$' && j == opc ){
				DaoEnum denum = {DAO_ENUM,0,0,0,0,0,0,NULL};
				DMap *jumps = DMap_New(D_VALUE,0);
				DNode *it, *find;
				int max=0, min=0;
				denum.etype = at;
				for(k=1; k<=opc; k++){
					DaoValue *cc = routConsts->items.pValue[ vmcs[i+k]->a ];
					if( DaoEnum_SetValue( & denum, & cc->xEnum, NULL ) ==0 ){
						cid = i + k;
						vmc = vmcs[i + k];
						type_source = cc->xEnum.etype;
						type_target = at;
						ec_specific = DTE_TYPE_NOT_MATCHING;
						DMap_Delete( jumps );
						goto ErrorTyping;
					}
					if( k ==1 ){
						max = min = denum.value;
					}else{
						if( denum.value > max ) max = denum.value;
						if( denum.value < min ) min = denum.value;
					}
					MAP_Insert( jumps, (DaoValue*) & denum, vmcs[i+k] );
				}
				if( at->flagtype == 0 && opc > 0.75*(max-min+1) ){
					for(it=DMap_First(at->mapNames);it;it=DMap_Next(at->mapNames,it)){
						denum.value = it->value.pInt;
						find = DMap_Find( jumps, (DaoValue*) & denum );
						if( find == NULL ) DMap_Insert( jumps, (DaoValue*) & denum, NULL );
					}
					k = 1;
					for(it=DMap_First(jumps);it;it=DMap_Next(jumps,it)){
						if( it->value.pVoid ){
							vmcs[i+k] = (DaoVmCodeX*) it->value.pVoid;
							vmcs[i+k]->a = routConsts->size;
							vmcs[i+k]->c = DAO_CASE_TABLE;
							k += 1;
						}else{
							vmc2.code = DVM_CASE;
							vmc2.a = routConsts->size;
							vmc2.b = opb;
							vmc2.c = DAO_CASE_TABLE;
							vmc2.first = vmc2.last = 0;
							vmc2.middle = 1;
							addCount[i+k] += 1;
							DArray_Append( addCode, & vmc2 );
						}
						DaoRoutine_AddConstant( self, it->key.pValue );
					}
					vmc->c = jumps->size;
					vmcs[i+1]->c = DAO_CASE_TABLE;
				}else{
					k = 1;
					for(it=DMap_First(jumps);it;it=DMap_Next(jumps,it)){
						vmcs[i+k] = (DaoVmCodeX*) it->value.pVoid;
						vmcs[i+k]->a = routConsts->size;
						DaoRoutine_AddConstant( self, it->key.pValue );
						k += 1;
					}
				}
				DMap_Delete( jumps );
			}else if( j ){
				vmcs[i + 1]->c = DAO_CASE_UNORDERED;
			}
			break;
		case DVM_CASE :
			break;
		case DVM_ITER :
			{
				AssertInitialized( opa, 0, 0, vmc->last );
				init[opc] = 1;
				DString_SetMBS( mbs, "__for_iterator__" );
				ts[0] = dao_type_for_iterator;
				meth = NULL;
				type_source = at;
				if( NoCheckingType( at ) ){
					ct = dao_type_for_iterator;
					UpdateType( opc, ct );
					AssertTypeMatching( ct, type[opc], defs, 0 );
					break;
				}
				switch( at->tid ){
				case DAO_CLASS :
				case DAO_OBJECT :
					klass = & at->aux->xClass;
					tp = DaoClass_GetDataType( klass, mbs, & j, hostClass );
					if( j == DAO_ERROR_FIELD_NOTPERMIT ) goto NotPermit;
					if( j == DAO_ERROR_FIELD_NOTEXIST ) goto NotExist;
					j = DaoClass_GetDataIndex( klass, mbs );
					k = LOOKUP_ST( j );
					if( k == DAO_OBJECT_VARIABLE && at->tid ==DAO_CLASS ) goto NeedInstVar;
					DaoClass_GetData( klass, mbs, & val, hostClass );
					if( val == NULL || val->type != DAO_ROUTINE ) goto NotMatch;
					meth = (DaoRoutine*) val;
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
				if( meth == NULL ) goto NotMatch;
				rout = DaoValue_Check( meth, at, ts, 1, DVM_CALL, errors );
				if( rout == NULL ) goto NotMatch;
				ct = dao_type_for_iterator;
				UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_GOTO :
			break;
		case DVM_TEST :
			{
				/* if( init[opa] ==0 ) goto NotInit;  allow none value for testing! */
				if( type[opa] ==NULL ) goto NotMatch;
				at = type[opa];
				if( typed_code ){
					if( at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE )
						vmc->code = DVM_TEST_I + at->tid - DAO_INTEGER;
				}
				break;
			}
		case DVM_MATH :
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last - 1 );
			init[opc] = 1;
			ct = type[opb];
			if( ct->tid > DAO_COMPLEX && ct->tid != DAO_ANY ) goto InvParam;
			if( opa == DVM_MATH_ARG || opa == DVM_MATH_IMAG || opa == DVM_MATH_NORM || opa == DVM_MATH_REAL ){
				ct = dnumt;
			}else if( opa == DVM_MATH_RAND ){
				/* ct = type[opb]; return the same type as parameter */
			}else if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_FLOAT ){ /* XXX long type */
				ct = dnumt;
			}
			UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			break;
		case DVM_CALL : case DVM_MCALL :
			{
				int ctchecked = 0;
				DaoType *cbtype = NULL;
				DaoVmCodeX *sect = NULL;
				if( (vmc->b & DAO_CALL_BLOCK) && vmcs[i+2]->code == DVM_SECT ){
					sect = vmcs[ i + 2 ];
					for(j=0, k=sect->a; j<sect->b; j++, k++){
						init[k] = 1;
						UpdateType( k, udf );
					}
				}
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				j = type[opa+1] ? type[opa+1]->tid : 0;
				if( code == DVM_MCALL && j >= DAO_ARRAY && j != DAO_ANY ){
					DaoVmCodeX *p = vmcs[i+1];
					if( p->code == DVM_MOVE && p->a == opa+1 ){
						p->code = DVM_NOP;
						if( i+2 < N ){
							p = vmcs[i+2];
							if( p->code >= DVM_SETVH && p->code <= DVM_SETF && p->a == opa+1 )
								p->code = DVM_NOP;
						}
					}
				}
				at = type[opa];
				bt = ct = NULL;
				if( code == DVM_CALL && tidHost == DAO_OBJECT ) bt = hostClass->objType;
				/*
				   DaoVmCodeX_Print( *vmc, NULL );
				   printf( "call: %s %i\n", type[opa]->name->mbs, type[opa]->tid );
				   if(bt) printf( "self: %s\n", bt->name->mbs );
				 */
				ct = type[opa];
				rout = NULL;
				if( at->tid == DAO_CLASS ){
					if( at->aux->xClass.classRoutines->overloads->routines->size ){
						rout = (DaoRoutine*) at->aux->xClass.classRoutines; // XXX
					}else{
						rout = at->aux->xClass.classRoutine;
					}
					ct = at->aux->xClass.objType;
				}else if( at->tid == DAO_CTYPE ){
					rout = DaoType_FindFunction( at, at->name );
					if( rout == NULL ) goto ErrorTyping;
				}else if( csts[opa] && csts[opa]->type == DAO_ROUTINE ){
					rout = (DaoRoutine*) csts[opa];
				}else if( at->tid == DAO_INITYPE || at->tid == DAO_FUNCURRY ){
					UpdateType( opc, any );
					AssertTypeMatching( any, type[opc], defs, 0 );
					goto TryPushBlockReturnType;
				}else if( at->tid == DAO_UDF || at->tid == DAO_ANY ){
					UpdateType( opc, any );
					goto TryPushBlockReturnType;
				}else if( at->tid == DAO_OBJECT ){
					rout = DaoClass_FindOperator( & at->aux->xClass, "()", hostClass );
					if( rout == NULL ) goto ErrorTyping;
				}else if( at->tid == DAO_CDATA ){
					rout = DaoType_FindFunctionMBS( at, "()" );
					if( rout == NULL ) goto ErrorTyping;
				}else if( at->tid != DAO_ROUTINE ){
					goto ErrorTyping;
				}
				pp = csts+opa+1;
				tp = type+opa+1;
				j = vmc->b & 0xff;
				if( j == 0 && (vmc->b & DAO_CALL_EXPAR) ){ /* call with caller's parameter */
					k = (self->routType->attrib & DAO_TYPE_SELF) != 0;
					j = self->parCount - k;
					pp = csts + k;
					tp = type + k;
				}else{
					for(k=0; k<j; k++){
						tt = DaoType_DefineTypes( tp[k], ns, defs );
						GC_ShiftRC( tt, tp[k] );
						tp[k] = tt;
						if( pp[k] && pp[k]->type == DAO_ROUTINE ) DaoRoutine_Compile( & pp[k]->xRoutine );
						assert( i >= (k+1) );
					}
					m = 1;
					for(k=i+1; k<N; k++){
						DaoVmCodeX *ret = vmcs[k];
						if( ret->code == DVM_NOP ) continue;
						if( ret->code == DVM_RETURN ){
							m &= ret->c ==0 && (ret->b ==0 || (ret->b ==1 && ret->a == vmc->c));
							break;
						}
						m = 0;
						break;
					}
					if( m ) vmc->b |= DAO_CALL_TAIL;
				}
				if( at->tid == DAO_ROUTINE && at->overloads ) rout = (DaoRoutine*)at->aux;
				DMap_Reset( defs2 );
				DMap_Assign( defs2, defs );
				if( rout == NULL && at->aux == NULL ){
					/* "routine" type: */
					ct = any;
					ctchecked = 1;
				}else if( self->routName->mbs[0] == '@' && (vmc->b & DAO_CALL_EXPAR) ){
					/* inside decorator: */
					ct = (DaoType*) at->aux;
					ctchecked = 1;
				}else if( rout == NULL ){
					if( DaoRoutine_CheckType( at, ns, bt, tp, j, code, 0 ) ==0 ){
						goto ErrorTyping;
					}
					if( at->name->mbs[0] == '@' ){
						ct = tp[0];
						if( pp[0] && pp[0]->type == DAO_ROUTINE ) ct = pp[0]->xRoutine.routType;
						UpdateType( opc, ct );
						AssertTypeMatching( ct, type[opc], defs, 0 );
						goto TryPushBlockReturnType;
					}
					cbtype = at->cbtype;
					DaoRoutine_CheckType( at, ns, bt, tp, j, code, 1 );
					ct = type[opa];
				}else{
					error = rout->routName;
					if( rout->type != DAO_ROUTINE ) goto ErrorTyping;
					rout2 = rout;
					/* rout can be DRoutines: */
					rout = DaoValue_Check( rout, bt, tp, j, code, errors );
					if( rout == NULL ) goto ErrorTyping;
					if( rout->routName->mbs[0] == '@' ){
						ct = tp[0];
						if( pp[0] && pp[0]->type == DAO_ROUTINE ){
							ct = pp[0]->xRoutine.routType;
							if( pp[0]->xRoutine.overloads ){
								DaoType *ft = & rout->routType->nested->items.pType[0]->aux->xType;
								DaoType **pts = ft->nested->items.pType;
								int nn = ft->nested->size;
								int cc = DVM_CALL + (ft->attrib & DAO_TYPE_SELF);
								rout = DaoValue_Check( (DaoRoutine*)pp[0], NULL, pts, nn, cc, errors );
								if( rout == NULL ) goto ErrorTyping;
								ct = rout->routType;
							}
						}
						UpdateType( opc, ct );
						AssertTypeMatching( ct, type[opc], defs, 0 );
						goto TryPushBlockReturnType;
					}

					if( rout2->overloads && rout2->overloads->routines->size > 1 ){
						DArray *routines = rout2->overloads->routines;
						m = DaoRoutine_CheckType( rout->routType, ns, bt, tp, j, code, 1 );
						if( m <= DAO_MT_ANY ){
							/* For situations like:
							//
							// routine test( x :int ){ io.writeln(1); return 1; }
							// routine test( x ){ io.writeln(2); return 'abc'; }
							// a :any = 1;
							// b = test( a );
							//
							// The function call may be resolved at compiling time as test(x),
							// which returns a string. But a runtime, the function call will
							// be resolved as test(x:int), which return an integer.
							// Such discrepancy need to be solved here:
							 */
							DArray_Clear( array );
							for(k=0,K=routines->size; k<K; k++){
								DaoType *type = routines->items.pRoutine[k]->routType;
								m = DaoRoutine_CheckType( type, ns, bt, tp, j, code, 1 );
								if( m == 0 ) continue;
								type = (DaoType*) type->aux;
								if( type == NULL ) type = dao_type_none;
								if( type && type->tid == DAO_ANY ){
									ctchecked = 1;
									ct = any;
									break;
								}
								for(m=0,K=array->size; m<K; m++){
									if( array->items.pType[m] == type ) break;
								}
								if( m >= (int)array->size ) DArray_Append( array, type );
							}
							if( array->size > 1 ){
								ctchecked = 1;
								ct = any;
							}
						}
					}

					tt = rout->routType;
					cbtype = tt->cbtype;
					if( tt->aux == NULL || (tt->attrib & DAO_TYPE_UNDEF) ){
						if( rout->body && rout->body->parser ) DaoRoutine_Compile( rout );
					}

					if( at->tid == DAO_CTYPE && at->kernel->sptree ){
						/* For type holder specialization: */
						k = DaoType_MatchTo( at, at->kernel->abtype->aux->xCdata.ctype, defs2 );
					}

					k = defs2->size;
					DaoRoutine_PassParamTypes2( rout, bt, tp, j, code, defs2 );
					if( notide && rout != self && defs2->size && ((int)defs2->size > k || rout->routType->aux->xType.tid == 0) ){
						DaoRoutine *orig, *drout;
						if( rout->original ) rout = rout->original;
						if( rout->body && rout->body->parser ) DaoRoutine_Compile( rout );
						/* rout may has only been declared */
						orig = rout;
						drout = DaoRoutine_Copy( rout, 0, 0 );
						DaoRoutine_PassParamTypes( drout, bt, tp, j, code, defs2 );

						DMutex_Lock( & mutex_routine_specialize );
						if( orig->specialized == NULL ) orig->specialized = DRoutines_New();
						DMutex_Unlock( & mutex_routine_specialize );

						GC_ShiftRC( orig, drout->original );
						DRoutines_Add( orig->specialized, drout );
						drout->original = orig;
						if( rout->body && drout->routType->aux->xType.tid == 0 ){
							DaoRoutineBody *body = DaoRoutineBody_Copy( drout->body );
							GC_ShiftRC( body, drout->body );
							drout->body = body;
							DMap_Reset( defs3 );
							DaoType_MatchTo( drout->routType, orig->routType, defs3 );
							DaoRoutine_MapTypes( drout, defs3 );
							/* to infer returned type */ 
							if( DaoRoutine_DoTypeInference( drout, silent ) ==0 ) goto InvParam;
						}
						rout = drout;
					}
					if( at->tid != DAO_CLASS && ! ctchecked ) ct = rout->routType;
					/*
					   printf( "ct2 = %s\n", ct ? ct->name->mbs : "" );
					 */
				}
				k = self->routType->attrib & ct->attrib;
				if( (k & DAO_TYPE_COROUTINE) && ! (opb & DAO_CALL_COROUT) ){
					if( DaoType_MatchTo( (DaoType*) ct->aux, (DaoType*) self->routType->aux, defs ) == 0 ){
						printf( "Coroutine calls a coroutine that may yield different values!\n" );
						goto InvOper;
					}
				}
				if( at->tid != DAO_CLASS && ! ctchecked && ! (opb & DAO_CALL_COROUT) ) ct = & ct->aux->xType;
				if( ct ) ct = DaoType_DefineTypes( ct, ns, defs2 );
				if( (opb & DAO_CALL_COROUT) && (ct->attrib & DAO_TYPE_COROUTINE) ){
					DaoType **ts = ct->nested->items.pType;
					ct = DaoNamespace_MakeType( ns, "routine", DAO_ROUTINE, ct->aux, ts, ct->nested->size );
				}

#ifdef DAO_WITH_CONCURRENT
				if( code == DVM_MCALL && tp[0]->tid == DAO_OBJECT
						&& (tp[0]->aux->xClass.attribs & DAO_CLS_ASYNCHRONOUS) ){
					ct = DaoNamespace_MakeType( ns, "future", DAO_FUTURE, NULL, &ct, 1 );
				}
#endif
				if( type[opc] && type[opc]->tid == DAO_ANY ) goto TryPushBlockReturnType;
				if( ct == NULL ) ct = DaoNamespace_GetType( ns, dao_none_value );
				UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				/*
				if( rout && strcmp( rout->routName->mbs, "values" ) ==0 ){
				   DaoVmCodeX_Print( *vmc, NULL );
				   printf( "ct = %s, %s %s\n", ct->name->mbs, self->routName->mbs, self->routType->name->mbs );
				   printf( "%p  %s\n", type[opc], type[opc] ? type[opc]->name->mbs : "----------" );
				}
				 */

TryPushBlockReturnType:
				if( sect && cbtype && cbtype->nested ){
					for(j=0, k=sect->a; j<sect->b; j++, k++){
						if( j >= (int)cbtype->nested->size ){
							if( j < sect->c ) printf( "Unsupported code section parameter!\n" );
							break;
						}// XXX better warning
						tt = cbtype->nested->items.pType[j];
						if( tt->tid == DAO_PAR_NAMED || tt->tid == DAO_PAR_DEFAULT ) tt = (DaoType*)tt->aux;
						tt = DaoType_DefineTypes( tt, ns, defs2 );
						UpdateType( k, tt );
					}
					tt = DaoType_DefineTypes( (DaoType*)cbtype->aux, ns, defs2 );
					DArray_Append( rettypes, opc );
					DArray_Append( rettypes, tt );
					DArray_Append( rettypes, tt );
					DArray_PushBack( typeMaps, defs2 );
				}else if( sect && cbtype == NULL ){
					if( NoCheckingType( type[opc] ) == 0 ){
						printf( "Unused code section at line %i!\n", vmc->line );
					}
					DArray_Append( rettypes, opc );
					DArray_Append( rettypes, NULL );
					DArray_Append( rettypes, NULL );
					DArray_PushBack( typeMaps, defs2 );
				}
				break;
			}
		case DVM_ROUTINE :
			{
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				if( type[opa]->tid != DAO_ROUTINE ) goto ErrorTyping;
				/* close on types */
				at = type[opa];
				tparray = DArray_New(0);
#if 0
				XXX typing
					DArray_Resize( tparray, at->parCount, 0 );
				for(j=0; j<vmc->b; j+=2){
					int loc = csts[vmc->a+j+2].v.i;
					if( loc >= at->parCount ) break;
					tparray->items.pType[loc] = type[opa+j+1];
				}
#endif
				at = DaoNamespace_MakeRoutType( ns, at, NULL, tparray->items.pType, NULL );
				DArray_Delete( tparray );

				UpdateType( opc, at );
				AssertTypeMatching( at, type[opc], defs, 0 );
				GC_ShiftRC( csts[opa], csts[opc] );
				csts[opc] = csts[opa];
				break;
			}
		case DVM_CLASS :
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			if( at->tid != DAO_TUPLE ) goto InvParam;
			if( at->nested->size ){
				bt = at->nested->items.pType[0];
				if( bt->tid == DAO_PAR_NAMED ) bt = & bt->aux->xType;
				if( bt->tid != DAO_STRING ) goto InvParam;
			}
			if( at->nested->size > 1 ){
				bt = at->nested->items.pType[1];
				if( bt->tid == DAO_PAR_NAMED ) bt = & bt->aux->xType;
				k = DaoType_MatchTo( bt, dao_list_any, defs ) == 0;
				if( k && DaoType_MatchTo( bt, dao_map_any, defs ) == 0 ) goto InvParam;
			}
			for(j=2,K=at->nested->size; j<K; j++){
				bt = at->nested->items.pType[j];
				if( bt->tid == DAO_PAR_NAMED ) bt = & bt->aux->xType;
				if( DaoType_MatchTo( bt, dao_list_any, defs ) == 0 ) goto InvParam;
			}
			init[opc] = 1;
			ct = udf;
			UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			break;
		case DVM_RETURN :
		case DVM_YIELD :
			{
				int redef = 0, popped = 0;
				DaoType *ct2;
				assert( rettypes->size >= 3 );
				ct = rettypes->items.pType[ rettypes->size - 1 ];
				ct2 = rettypes->items.pType[ rettypes->size - 2 ];
				redef = rettypes->items.pInt[ rettypes->size - 3 ];
				DMap_Reset( defs2 );
				DMap_Assign( defs2, defs );
				if( (i+1) < (int)self->body->annotCodes->size ){
					int nop = vmcs[i+1]->code == DVM_NOP;
					if( vmcs[i+nop+1]->code == DVM_GOTO && vmcs[i+nop+1]->c == DVM_SECT ){
						DArray_Erase( rettypes, rettypes->size - 3, -1 );
						DArray_PopBack( typeMaps );
						popped = 1;
					}
				}
				if( i && vmcs[i-1]->code == DVM_TUPLE && vmcs[i-1]->c == vmc->a && vmc->b == 1 ){
					vmc->a = vmcs[i-1]->a;
					vmc->b = vmcs[i-1]->b;
					vmcs[i-1]->code = DVM_UNUSED;
					opa = vmc->a;
					opb = vmc->b;
					opc = vmc->c;
				}
				/*
				   printf( "%p %i %s %s\n", self, self->routType->nested->size, self->routType->name->mbs, ct?ct->name->mbs:"" );
				 */
				if( code == DVM_YIELD && self->routType->cbtype ){ /* yield in functional method: */
					if( vmc->b == 0 ){
						if( self->routType->cbtype->aux ) goto ErrorTyping;
						break;
					}
					tt = self->routType->cbtype;
					tp = tt->nested->items.pType;
					at = DaoNamespace_MakeType( ns, "tuple", DAO_TUPLE, NULL, type+opa, vmc->b);
					ct = DaoNamespace_MakeType( ns, "tuple", DAO_TUPLE, NULL, tp, tt->nested->size );
					if( DaoType_MatchTo( at, ct, defs2 ) == 0 ) goto ErrorTyping;
					ct = (DaoType*) self->routType->cbtype->aux;
					if( ct ){
						init[opc] = 1;
						UpdateType( opc, ct );
						AssertTypeMatching( ct, type[opc], defs2, 0 );
					}
					break;
				}
				if( code == DVM_YIELD && !(self->routType->attrib & DAO_TYPE_COROUTINE) ){
					printf( "Cannot yield from normal function!\n" );
					goto InvOper;
				}
				if( vmc->b ==0 ){
					/* less strict checking for type holder as well (case mt.start()): */
					if( ct && ct->tid == DAO_UDF ){
						ct = DaoNamespace_MakeValueType( ns, dao_none_value );
						rettypes->items.pType[ rettypes->size - 1 ] = ct;
						ct = DaoNamespace_MakeRoutType( ns, self->routType, NULL, NULL, ct );
						GC_ShiftRC( ct, self->routType );
						self->routType = ct;
						continue;
					}
					if( ct && NoCheckingType( ct ) ) continue;
					if( ct && ct->tid == DAO_VALTYPE && ct->aux->type == DAO_NONE ) continue;
					if( ct && ! (self->attribs & DAO_ROUT_INITOR) ) goto ErrorTyping;
				}else{
					at = type[opa];
					if( at ==NULL ) goto ErrorTyping;
					if( vmc->b >1 )
						at = DaoNamespace_MakeType( ns, "tuple", DAO_TUPLE, NULL, type+opa, vmc->b);

					if( ct && DaoType_MatchTo( at, ct, defs2 ) == 0 ) goto ErrorTyping;
					if( ct != ct2 && ct->tid != DAO_UDF && ct->tid != DAO_INITYPE ){
						int mac = DaoType_MatchTo( at, ct, defs2 );
						int mca = DaoType_MatchTo( ct, at, defs2 );
						if( mac==0 && mca==0 ){
							goto ErrorTyping;
						}else if( mac==0 ){
							if( rettypes->size == 3 && popped == 0 ){
								tt = DaoNamespace_MakeRoutType( ns, self->routType, NULL, NULL, at );
								GC_ShiftRC( tt, self->routType );
								self->routType = tt;
							}else{
								ct = DaoType_DefineTypes( ct, ns, defs2 );
								if( ct ){
									tt = DaoType_DefineTypes( type[redef], ns, defs2 );
									UpdateType( redef, tt );
								}
								if( popped == 0 ) rettypes->items.pType[ rettypes->size - 1 ] = ct;
							}
						}
					}else if( ct && !( ct->attrib & DAO_TYPE_UNDEF) ){
						if( notide && DaoType_MatchTo( at, ct, defs2 ) == DAO_MT_SUB ){
							if( ct->tid == DAO_TUPLE && DaoType_MatchTo( ct, at, defs2 ) ){
								/* typedef tuple<x:float,y:float> Point2D
								 * routine Test()=>Point2D{ return (1.0,2.0); } */
								addCount[i] ++;
								vmc2.code = DVM_CAST;
								vmc2.a = opa;
								vmc2.b = 0;
								vmc2.c = self->body->regCount + addRegType->size;
								vmc->a = vmc2.c;
								DArray_Append( addCode, & vmc2 );
								DArray_Append( addRegType, ct );
							}else{
								goto ErrorTyping;
							}
						}
					}else{
						if( rettypes->size == 3 && popped == 0 ){
							tt = DaoNamespace_MakeRoutType( ns, self->routType, NULL, NULL, at );
							GC_ShiftRC( tt, self->routType );
							self->routType = tt;
							rettypes->items.pType[ rettypes->size - 1 ] = (DaoType*)tt->aux;
						}else{
							ct = DaoType_DefineTypes( ct, ns, defs2 );
							if( ct ){
								tt = DaoType_DefineTypes( type[redef], ns, defs2 );
								UpdateType( redef, tt );
							}
							if( popped == 0 ) rettypes->items.pType[ rettypes->size - 1 ] = ct;
						}
					}
				}
				if( code == DVM_YIELD ){
					init[opc] = 1;
					tt = self->routType;
					if( tt->nested->size ==1 ){
						ct = tt->nested->items.pType[0];
						if( ct->tid == DAO_PAR_NAMED || ct->tid == DAO_PAR_DEFAULT )
							ct = & ct->aux->xType;
					}else if( tt->nested->size ){
						ct = DaoNamespace_MakeType(ns, "tuple", DAO_TUPLE, NULL,
								tt->nested->items.pType, tt->nested->size );
					}else{
						ct = udf;
					}
					UpdateType( opc, ct );
					AssertTypeMatching( ct, type[opc], defs2, 0 );
					AssertTypeMatching( at, & tt->aux->xType, defs2, 0 );
				}
				break;
			}
#define USE_TYPED_OPCODE 1
#if USE_TYPED_OPCODE
		case DVM_DATA_I : case DVM_DATA_F : case DVM_DATA_D : 
			TT1 = DAO_INTEGER + (code - DVM_DATA_I);
			if( ct == NULL ){
				GC_ShiftRC( simtps[TT1], type[opc] );
				ct = type[opc] = simtps[TT1];
			}
			AssertTypeIdMatching( ct, TT1, 0 );
			init[opc] = 1;
			break;
		case DVM_GETCL_I : case DVM_GETCL_F : case DVM_GETCL_D : 
			val = dataCL[opa]->items.pValue[opb];
			TT1 = DAO_INTEGER + (code - DVM_GETCL_I);
			at = DaoNamespace_GetType( ns, val );
			if( ct == NULL ){
				GC_ShiftRC( simtps[TT1], type[opc] );
				ct = type[opc] = simtps[TT1];
			}
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( ct, TT1, 0 );
			init[opc] = 1;
			break;
		case DVM_GETCK_I : case DVM_GETCK_F : case DVM_GETCK_D : 
			val = CSS->items.pClass[opa]->cstData->items.pValue[opb];
			TT1 = DAO_INTEGER + (code - DVM_GETCK_I);
			at = DaoNamespace_GetType( ns, val );
			if( ct == NULL ){
				GC_ShiftRC( simtps[TT1], type[opc] );
				ct = type[opc] = simtps[TT1];
			}
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( ct, TT1, 0 );
			init[opc] = 1;
			break;
		case DVM_GETCG_I : case DVM_GETCG_F : case DVM_GETCG_D : 
			val = NSS->items.pNS[opa]->cstData->items.pValue[opb];
			TT1 = DAO_INTEGER + (code - DVM_GETCG_I);
			at = DaoNamespace_GetType( ns, val );
			if( ct == NULL ){
				GC_ShiftRC( simtps[TT1], type[opc] );
				ct = type[opc] = simtps[TT1];
			}
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( ct, TT1, 0 );
			init[opc] = 1;
			break;
		case DVM_GETVH_I : case DVM_GETVH_F : case DVM_GETVH_D : 
			TT1 = DAO_INTEGER + (code - DVM_GETVH_I);
			at = type[opb];
			if( ct == NULL ){
				GC_ShiftRC( simtps[TT1], type[opc] );
				ct = type[opc] = simtps[TT1];
			}
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( ct, TT1, 0 );
			init[opc] = 1;
			break;
		case DVM_GETVL_I : case DVM_GETVL_F : case DVM_GETVL_D : 
			TT1 = DAO_INTEGER + (code - DVM_GETVL_I);
			at = typeVL[opa]->items.pType[opb];
			if( ct == NULL ){
				GC_ShiftRC( simtps[TT1], type[opc] );
				ct = type[opc] = simtps[TT1];
			}
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( ct, TT1, 0 );
			init[opc] = 1;
			break;
		case DVM_GETVO_I : case DVM_GETVO_F : case DVM_GETVO_D : 
			TT1 = DAO_INTEGER + (code - DVM_GETVO_I);
			at = typeVO[opa]->items.pType[opb];
			if( ct == NULL ){
				GC_ShiftRC( simtps[TT1], type[opc] );
				ct = type[opc] = simtps[TT1];
			}
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( ct, TT1, 0 );
			init[opc] = 1;
			break;
		case DVM_GETVK_I : case DVM_GETVK_F : case DVM_GETVK_D : 
			TT1 = DAO_INTEGER + (code - DVM_GETVK_I);
			at = CSS->items.pClass[opa]->glbDataType->items.pType[opb];
			if( ct == NULL ){
				GC_ShiftRC( simtps[TT1], type[opc] );
				ct = type[opc] = simtps[TT1];
			}
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( ct, TT1, 0 );
			init[opc] = 1;
			break;
		case DVM_GETVG_I : case DVM_GETVG_F : case DVM_GETVG_D : 
			TT1 = DAO_INTEGER + (code - DVM_GETVG_I);
			at = NSS->items.pNS[opa]->varType->items.pType[opb];
			if( ct == NULL ){
				GC_ShiftRC( simtps[TT1], type[opc] );
				ct = type[opc] = simtps[TT1];
			}
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( ct, TT1, 0 );
			init[opc] = 1;
			break;
		case DVM_SETVH_II : case DVM_SETVH_IF : case DVM_SETVH_ID :
		case DVM_SETVH_FI : case DVM_SETVH_FF : case DVM_SETVH_FD :
		case DVM_SETVH_DI : case DVM_SETVH_DF : case DVM_SETVH_DD :
			tp = type + opb;
			if( *tp==NULL || (*tp)->tid ==DAO_UDF ){
				GC_ShiftRC( type[opa], *tp );
				*tp = type[opa];
			}
			TT1 = DAO_INTEGER + (code - DVM_SETVH_II) % 3;
			TT3 = DAO_INTEGER + ((code - DVM_SETVH_II)/3) % 3;
			AssertTypeMatching( type[opa], *tp, defs, 0 );
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( tp[0], TT3, 0 );
			break;
		case DVM_SETVL_II : case DVM_SETVL_IF : case DVM_SETVL_ID :
		case DVM_SETVL_FI : case DVM_SETVL_FF : case DVM_SETVL_FD :
		case DVM_SETVL_DI : case DVM_SETVL_DF : case DVM_SETVL_DD :
			tp = typeVL[opc]->items.pType + opb;
			if( *tp==NULL || (*tp)->tid ==DAO_UDF ){
				GC_ShiftRC( type[opa], *tp );
				*tp = type[opa];
			}
			TT1 = DAO_INTEGER + (code - DVM_SETVL_II) % 3;
			TT3 = DAO_INTEGER + ((code - DVM_SETVL_II)/3) % 3;
			AssertTypeMatching( type[opa], *tp, defs, 0 );
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( tp[0], TT3, 0 );
			break;
		case DVM_SETVO_II : case DVM_SETVO_IF : case DVM_SETVO_ID :
		case DVM_SETVO_FI : case DVM_SETVO_FF : case DVM_SETVO_FD :
		case DVM_SETVO_DI : case DVM_SETVO_DF : case DVM_SETVO_DD :
			if( tidHost != DAO_OBJECT ) goto ErrorTyping;
			tp = typeVO[opc]->items.pType + opb;
			if( *tp==NULL || (*tp)->tid ==DAO_UDF ){
				GC_ShiftRC( type[opa], *tp );
				*tp = type[opa];
			}
			TT1 = DAO_INTEGER + (code - DVM_SETVO_II) % 3;
			TT3 = DAO_INTEGER + ((code - DVM_SETVO_II)/3) % 3;
			AssertTypeMatching( type[opa], *tp, defs, 0 );
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( tp[0], TT3, 0 );
			break;
		case DVM_SETVK_II : case DVM_SETVK_IF : case DVM_SETVK_ID :
		case DVM_SETVK_FI : case DVM_SETVK_FF : case DVM_SETVK_FD :
		case DVM_SETVK_DI : case DVM_SETVK_DF : case DVM_SETVK_DD :
			tp = CSS->items.pClass[opc]->glbDataType->items.pType + opb;
			if( *tp==NULL || (*tp)->tid ==DAO_UDF ){
				GC_ShiftRC( type[opa], *tp );
				*tp = type[opa];
			}
			TT1 = DAO_INTEGER + (code - DVM_SETVK_II) % 3;
			TT3 = DAO_INTEGER + ((code - DVM_SETVK_II)/3) % 3;
			AssertTypeMatching( type[opa], *tp, defs, 0 );
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( tp[0], TT3, 0 );
			break;
		case DVM_SETVG_II : case DVM_SETVG_IF : case DVM_SETVG_ID :
		case DVM_SETVG_FI : case DVM_SETVG_FF : case DVM_SETVG_FD :
		case DVM_SETVG_DI : case DVM_SETVG_DF : case DVM_SETVG_DD :
			tp = NSS->items.pNS[opc]->varType->items.pType + opb;
			if( *tp==NULL || (*tp)->tid ==DAO_UDF ){
				GC_ShiftRC( type[opa], *tp );
				*tp = type[opa];
			}
			TT1 = DAO_INTEGER + (code - DVM_SETVG_II) % 3;
			TT3 = DAO_INTEGER + ((code - DVM_SETVG_II)/3) % 3;
			AssertTypeMatching( type[opa], *tp, defs, 0 );
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( tp[0], TT3, 0 );
			break;
		case DVM_MOVE_II : case DVM_MOVE_IF : case DVM_MOVE_ID : 
		case DVM_MOVE_FI : case DVM_MOVE_FF : case DVM_MOVE_FD :
		case DVM_MOVE_DI : case DVM_MOVE_DF : case DVM_MOVE_DD : 
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, at );
			TT1 = DAO_INTEGER + (code - DVM_MOVE_II) % 3;
			TT3 = DAO_INTEGER + ((code - DVM_MOVE_II)/3) % 3;
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( type[opc], TT3, 0 );
			init[opc] = 1;
			break;
		case DVM_NOT_I : case DVM_NOT_F : case DVM_NOT_D : 
		case DVM_UNMS_I : case DVM_UNMS_F : case DVM_UNMS_D : 
		case DVM_BITREV_I : case DVM_BITREV_F : case DVM_BITREV_D :
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, at );
			TT1 = TT3 = DAO_INTEGER + (code - DVM_MOVE_II) % 3;
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( type[opc], TT3, 0 );
			init[opc] = 1;
			break;
		case DVM_UNMS_C :
		case DVM_MOVE_CC :
		case DVM_MOVE_SS :
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, at );
			TT1 = TT3 = code == DVM_MOVE_SS ? DAO_STRING : DAO_COMPLEX;
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( type[opc], TT3, 0 );
			init[opc] = 1;
			break;
		case DVM_MOVE_PP :
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			if( at->tid < DAO_ARRAY || at->tid >= DAO_ANY ) goto NotMatch;
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, at );
			if( type[opc]->tid != at->tid ) goto NotMatch;
			init[opc] = 1;
			if( opb ){
				GC_ShiftRC( csts[opa], csts[opc] );
				csts[opc] = csts[opa];
			}
			break;
		case DVM_ADD_III : case DVM_SUB_III : case DVM_MUL_III : case DVM_DIV_III :
		case DVM_MOD_III : case DVM_POW_III : case DVM_AND_III : case DVM_OR_III  :
		case DVM_LT_III  : case DVM_LE_III  : case DVM_EQ_III : case DVM_NE_III :
		case DVM_BITAND_III  : case DVM_BITOR_III  : case DVM_BITXOR_III :
		case DVM_BITLFT_III  : case DVM_BITRIT_III  :
			if( csts[opc] ) goto ModifyConstant;
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, inumt );
			AssertTypeIdMatching( at, DAO_INTEGER, 0 );
			AssertTypeIdMatching( bt, DAO_INTEGER, 0 );
			AssertTypeIdMatching( type[opc], DAO_INTEGER, 0 );
			init[opc] = 1;
			break;
		case DVM_ADD_FFF : case DVM_SUB_FFF : case DVM_MUL_FFF : case DVM_DIV_FFF :
		case DVM_MOD_FFF : case DVM_POW_FFF : case DVM_AND_FFF : case DVM_OR_FFF  :
		case DVM_LT_FFF  : case DVM_LE_FFF  : case DVM_EQ_FFF :
		case DVM_BITAND_FFF  : case DVM_BITOR_FFF  : case DVM_BITXOR_FFF :
		case DVM_BITLFT_FFF  : case DVM_BITRIT_FFF  :
			if( csts[opc] ) goto ModifyConstant;
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, fnumt );
			AssertTypeIdMatching( at, DAO_FLOAT, 0 );
			AssertTypeIdMatching( bt, DAO_FLOAT, 0 );
			AssertTypeIdMatching( type[opc], DAO_FLOAT, 0 );
			init[opc] = 1;
			break;
		case DVM_ADD_DDD : case DVM_SUB_DDD : case DVM_MUL_DDD : case DVM_DIV_DDD :
		case DVM_MOD_DDD : case DVM_POW_DDD : case DVM_AND_DDD : case DVM_OR_DDD  :
		case DVM_LT_DDD  : case DVM_LE_DDD  : case DVM_EQ_DDD :
		case DVM_BITAND_DDD  : case DVM_BITOR_DDD  : case DVM_BITXOR_DDD :
		case DVM_BITLFT_DDD  : case DVM_BITRIT_DDD  :
			if( csts[opc] ) goto ModifyConstant;
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, dnumt );
			AssertTypeIdMatching( at, DAO_DOUBLE, 0 );
			AssertTypeIdMatching( bt, DAO_DOUBLE, 0 );
			AssertTypeIdMatching( type[opc], DAO_DOUBLE, 0 );
			init[opc] = 1;
			break;
		case DVM_ADD_CC : case DVM_SUB_CC : case DVM_MUL_CC : case DVM_DIV_CC :
			if( csts[opc] ) goto ModifyConstant;
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, comt );
			AssertTypeIdMatching( at, DAO_COMPLEX, 0 );
			AssertTypeIdMatching( bt, DAO_COMPLEX, 0 );
			AssertTypeIdMatching( type[opc], DAO_COMPLEX, 0 );
			init[opc] = 1;
			break;
		case DVM_ADD_SS : case DVM_LT_SS : case DVM_LE_SS :
		case DVM_EQ_SS : case DVM_NE_SS :
			if( csts[opc] ) goto ModifyConstant;
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
			tt = code == DVM_ADD_SS ? strt : inumt;
			TT3 = code == DVM_ADD_SS ? DAO_STRING : DAO_INTEGER;
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, tt );
			AssertTypeIdMatching( at, DAO_STRING, 0 );
			AssertTypeIdMatching( bt, DAO_STRING, 0 );
			AssertTypeIdMatching( type[opc], TT3, 0 );
			init[opc] = 1;
			break;
		case DVM_ADD_FNN : case DVM_SUB_FNN : case DVM_MUL_FNN : case DVM_DIV_FNN :
		case DVM_MOD_FNN : case DVM_POW_FNN : case DVM_AND_FNN : case DVM_OR_FNN  :
		case DVM_LT_FNN  : case DVM_LE_FNN  : case DVM_EQ_FNN :
		case DVM_BITLFT_FNN  : case DVM_BITRIT_FNN  :
			if( csts[opc] ) goto ModifyConstant;
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
			if( at->tid ==0 || at->tid > DAO_DOUBLE ) goto NotMatch;
			if( bt->tid ==0 || bt->tid > DAO_DOUBLE ) goto NotMatch;
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, fnumt );
			if( type[opc]->tid != DAO_FLOAT ) goto NotMatch;
			AssertTypeIdMatching( type[opc], DAO_FLOAT, 0 );
			init[opc] = 1;
			break;
		case DVM_ADD_DNN : case DVM_SUB_DNN : case DVM_MUL_DNN : case DVM_DIV_DNN :
		case DVM_MOD_DNN : case DVM_POW_DNN : case DVM_AND_DNN : case DVM_OR_DNN  :
		case DVM_LT_DNN  : case DVM_LE_DNN  : case DVM_EQ_DNN :
		case DVM_BITLFT_DNN  : case DVM_BITRIT_DNN  :
			if( csts[opc] ) goto ModifyConstant;
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
			if( at->tid ==0 || at->tid > DAO_DOUBLE ) goto NotMatch;
			if( bt->tid ==0 || bt->tid > DAO_DOUBLE ) goto NotMatch;
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, dnumt );
			if( type[opc]->tid != DAO_DOUBLE ) goto NotMatch;
			AssertTypeIdMatching( type[opc], DAO_DOUBLE, 0 );
			init[opc] = 1;
			break;
		case DVM_GETI_SI :
			AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
			AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
			AssertTypeIdMatching( at, DAO_STRING, 0 );
			if( code == DVM_GETI_SI && bt->tid != DAO_INTEGER ) goto NotMatch;
			UpdateType( opc, inumt );
			AssertTypeIdMatching( type[opc], DAO_INTEGER, 0 );
			init[opc] = 1;
			break;
		case DVM_SETI_SII :
			if( csts[opc] ) goto ModifyConstant;
			k = DaoTokens_FindLeftPair( self->body->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
			AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
			AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
			AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
			AssertTypeIdMatching( at, DAO_INTEGER, 0 );
			AssertTypeIdMatching( bt, DAO_INTEGER, 0 );
			AssertTypeIdMatching( ct, DAO_STRING, 0 );
			break;
		case DVM_GETI_LI :
			AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
			AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
			AssertTypeIdMatching( at, DAO_LIST, 0 );
			AssertTypeIdMatching( bt, DAO_INTEGER, 0 );
			at = type[opa]->nested->items.pType[0];
			if( at->tid < DAO_ARRAY || at->tid >= DAO_ANY ) goto NotMatch;
			UpdateType( opc, at );
			AssertTypeMatching( at, type[opc], defs, 0 );
			init[opc] = 1;
			break;
		case DVM_GETI_LII : case DVM_GETI_LFI : case DVM_GETI_LDI :
		case DVM_GETI_AII : case DVM_GETI_AFI : case DVM_GETI_ADI :
		case DVM_GETI_LSI :
			AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
			AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
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
			UpdateType( opc, at );
			AssertTypeIdMatching( type[opc], TT1, 0 );
			init[opc] = 1;
			break;
		case DVM_GETMI_A :
			AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
			for(j=0; j<opb; j++){
				AssertInitialized( opb+j+1, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
				bt = type[opa + j + 1];
				if( bt->tid == 0 || bt->tid > DAO_DOUBLE ) goto InvIndex;
			}
			at = at->nested->items.pType[0];
			UpdateType( opc, at );
			AssertTypeMatching( at, type[opc], defs, 0 );
			init[opc] = 1;
			break;
		case DVM_SETI_LI :
			if( csts[opc] ) goto ModifyConstant;
			k = DaoTokens_FindLeftPair( self->body->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
			AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
			AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
			AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
			AssertTypeIdMatching( bt, DAO_INTEGER, 0 );
			AssertTypeIdMatching( ct, DAO_LIST, 0 );
			ct = type[opc]->nested->items.pType[0];
			if( at != ct && ct->tid != DAO_ANY ) goto NotMatch;
			break;
		case DVM_SETI_LIII : case DVM_SETI_LIIF : case DVM_SETI_LIID :
		case DVM_SETI_LFII : case DVM_SETI_LFIF : case DVM_SETI_LFID :
		case DVM_SETI_LDII : case DVM_SETI_LDIF : case DVM_SETI_LDID :
		case DVM_SETI_AIII : case DVM_SETI_AIIF : case DVM_SETI_AIID :
		case DVM_SETI_AFII : case DVM_SETI_AFIF : case DVM_SETI_AFID :
		case DVM_SETI_ADII : case DVM_SETI_ADIF : case DVM_SETI_ADID :
		case DVM_SETI_LSIS :
			if( csts[opc] ) goto ModifyConstant;
			k = DaoTokens_FindLeftPair( self->body->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
			AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
			AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
			AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
			TT2 = DAO_INTEGER;
			TT1 = TT3 = TT6 = 0;
			if( code >= DVM_SETI_AIII ){
				TT6 = DAO_ARRAY;
				TT1 = DAO_INTEGER + (code - DVM_SETI_AIII)%3;
				TT3 = DAO_INTEGER + (code - DVM_SETI_AIII)/3;
			}else if( code != DVM_SETI_LSIS ){
				TT6 = DAO_LIST;
				TT1 = DAO_INTEGER + (code - DVM_SETI_LIII)%3;
				TT3 = DAO_INTEGER + (code - DVM_SETI_LIII)/3;
			}else{
				TT6 = DAO_LIST;
				TT1 = TT3 = DAO_STRING;
			}
			if( ct->tid != TT6 || bt->tid != TT2 || at->tid != TT1 ) goto NotMatch;
			if( ct->nested->size !=1 || ct->nested->items.pType[0]->tid != TT3 ) goto NotMatch;
			break;
		case DVM_SETMI_A :
			if( csts[opc] ) goto ModifyConstant;
			k = DaoTokens_FindLeftPair( self->body->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
			AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
			AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
			for(j=0; j<opb; j++){
				AssertInitialized( opa+j+1, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
				bt = type[opa + j + 1];
				if( bt->tid == 0 || bt->tid > DAO_DOUBLE ) goto InvIndex;
			}
			if( at->tid == 0 || at->tid > DAO_DOUBLE ) goto NotMatch;
			break;
		case DVM_GETI_TI :
			AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
			AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
			if( at->tid != DAO_TUPLE || bt->tid != DAO_INTEGER ) goto NotMatch;
			UpdateType( opc, any );
			init[opc] = 1;
			break;
		case DVM_SETI_TI :
			if( csts[opc] ) goto ModifyConstant;
			k = DaoTokens_FindLeftPair( self->body->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
			AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
			AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
			AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
			if( ct->tid != DAO_TUPLE || bt->tid != DAO_INTEGER ) goto NotMatch;
			break;
		case DVM_SETF_T :
			if( csts[opc] ) goto ModifyConstant;
			if( init[opa] ==0 || init[opc] ==0 ) goto NotInit;
			if( at ==NULL || ct ==NULL || ct->tid != DAO_TUPLE ) goto NotMatch;
			if( opb >= ct->nested->size ) goto InvIndex;
			tt = ct->nested->items.pType[opb];
			if( tt->tid == DAO_PAR_NAMED ) tt = & tt->aux->xType;
			if( at != tt && tt->tid != DAO_ANY ) goto NotMatch;
			break;
		case DVM_GETF_T :
		case DVM_GETF_TI : case DVM_GETF_TF :
		case DVM_GETF_TD : case DVM_GETF_TS :
			if( init[opa] ==0 ) goto NotInit;
			if( at ==NULL || at->tid != DAO_TUPLE ) goto NotMatch;
			if( opb >= at->nested->size ) goto InvIndex;
			ct = at->nested->items.pType[opb];
			if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
			UpdateType( opc, ct );
			if( code != DVM_GETF_T ){
				TT3 = code == DVM_GETF_TS ? DAO_STRING : DAO_INTEGER + (code - DVM_GETF_TI);
				if( ct ==NULL || ct->tid != TT3 ) goto NotMatch;
				if( type[opc]->tid != TT3 ) goto NotMatch;
			}else{
				AssertTypeMatching( ct, type[opc], defs, 0 );
			}
			init[opc] = 1;
			break;
		case DVM_SETF_TII : case DVM_SETF_TIF : case DVM_SETF_TID :
		case DVM_SETF_TFI : case DVM_SETF_TFF : case DVM_SETF_TFD :
		case DVM_SETF_TDI : case DVM_SETF_TDF : case DVM_SETF_TDD :
		case DVM_SETF_TSS :
			if( csts[opc] ) goto ModifyConstant;
			if( init[opa] ==0 || init[opc] ==0 ) goto NotInit;
			if( at ==NULL || ct ==NULL ) goto NotMatch;
			TT1 = TT3 = 0;
			if( code == DVM_SETF_TSS ){
				TT1 = TT3 = DAO_STRING;
			}else{
				TT1 = DAO_INTEGER + (code - DVM_SETF_TII)%3;
				TT3 = DAO_INTEGER + (code - DVM_SETF_TII)/3;
			}
			if( ct->tid != DAO_TUPLE || at->tid != TT1 ) goto NotMatch;
			if( opb >= ct->nested->size ) goto InvIndex;
			tt = ct->nested->items.pType[opb];
			if( tt->tid == DAO_PAR_NAMED ) tt = & tt->aux->xType;
			if( tt->tid != TT3 ) goto NotMatch;
			break;
		case DVM_GETI_ACI :
			AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
			AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
			AssertTypeMatching( type[opa], cart, defs, 0 );
			bt = type[opb];
			if( bt->tid != DAO_INTEGER ) goto NotMatch;
			UpdateType( opc, comt );
			if( type[opc]->tid != DAO_COMPLEX ) goto NotMatch;
			init[opc] = 1;
			break;
		case DVM_SETI_ACI :
			if( csts[opc] ) goto ModifyConstant;
			k = DaoTokens_FindLeftPair( self->body->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
			AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
			AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
			AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
			if( type[opa]->tid != DAO_COMPLEX ) goto NotMatch;
			if( type[opb]->tid != DAO_INTEGER ) goto NotMatch;
			AssertTypeMatching( type[opc], cart, defs, 0 );
			break;
		case DVM_GETF_KC :
		case DVM_GETF_KCI : case DVM_GETF_KCF : case DVM_GETF_KCD : 
			if( init[opa] ==0 || type[opa] == NULL ) goto NotInit;
			init[opc] = 1;
			if( type[opa]->tid != DAO_CLASS ) goto NotMatch;
			klass = & type[opa]->aux->xClass;
			ct = DaoNamespace_GetType( ns, klass->cstData->items.pValue[ opb ] );
			UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			if( code == DVM_GETF_KC ) break;
			if( ct->tid != (DAO_INTEGER + code - DVM_GETF_KCI) ) goto NotMatch;
			break;
		case DVM_GETF_KG :
		case DVM_GETF_KGI : case DVM_GETF_KGF : case DVM_GETF_KGD :
			if( init[opa] ==0 || type[opa] == NULL ) goto NotInit;
			init[opc] = 1;
			if( type[opa]->tid != DAO_CLASS ) goto NotMatch;
			klass = & type[opa]->aux->xClass;
			ct = klass->glbDataType->items.pType[ opb ];
			UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			if( code == DVM_GETF_KG ) break;
			if( ct->tid != (DAO_INTEGER + code - DVM_GETF_KGI) ) goto NotMatch;
			break;
		case DVM_GETF_OC :
		case DVM_GETF_OCI : case DVM_GETF_OCF : case DVM_GETF_OCD : 
			if( init[opa] ==0 || type[opa] == NULL ) goto NotInit;
			init[opc] = 1;
			if( type[opa]->tid != DAO_OBJECT ) goto NotMatch;
			klass = & type[opa]->aux->xClass;
			ct = DaoNamespace_GetType( ns, klass->cstData->items.pValue[ opb ] );
			UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			if( code == DVM_GETF_OC ) break;
			if( ct->tid != (DAO_INTEGER + code - DVM_GETF_OCI) ) goto NotMatch;
			break;
		case DVM_GETF_OG :
		case DVM_GETF_OGI : case DVM_GETF_OGF : case DVM_GETF_OGD : 
			if( init[opa] ==0 || type[opa] == NULL ) goto NotInit;
			init[opc] = 1;
			if( type[opa]->tid != DAO_OBJECT ) goto NotMatch;
			klass = & type[opa]->aux->xClass;
			ct = klass->glbDataType->items.pType[ opb ];
			UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			if( code == DVM_GETF_OG ) break;
			if( ct->tid != (DAO_INTEGER + code - DVM_GETF_OGI) ) goto NotMatch;
			break;
		case DVM_GETF_OV :
		case DVM_GETF_OVI : case DVM_GETF_OVF : case DVM_GETF_OVD :
			if( init[opa] ==0 || type[opa] == NULL ) goto NotInit;
			init[opc] = 1;
			if( type[opa]->tid != DAO_OBJECT ) goto NotMatch;
			klass = & type[opa]->aux->xClass;
			ct = klass->objDataType->items.pType[ opb ];
			UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			if( code == DVM_GETF_OV ) break;
			if( ct->tid != (DAO_INTEGER + code - DVM_GETF_OVI) ) goto NotMatch;
			break;
		case DVM_SETF_KG : 
		case DVM_SETF_KGII : case DVM_SETF_KGIF : case DVM_SETF_KGID : 
		case DVM_SETF_KGFI : case DVM_SETF_KGFF : case DVM_SETF_KGFD : 
		case DVM_SETF_KGDI : case DVM_SETF_KGDF : case DVM_SETF_KGDD : 
			ct = (DaoType*) type[opc];
			if( ct == NULL ) goto ErrorTyping;
			if( csts[opc] ) goto ModifyConstant;
			if( init[opa] ==0 || init[opc] ==0 ) goto NotInit;
			if( type[opa] ==NULL || type[opc] ==NULL ) goto NotMatch;
			if( ct->tid != DAO_CLASS ) goto NotMatch;
			ct = ct->aux->xClass.glbDataType->items.pType[ opb ];
			if( code == DVM_SETF_KG ){
				if( at != ct && ct->tid != DAO_ANY ) goto NotMatch;
				break;
			}
			AssertTypeMatching( at, ct, defs, 0 );
			if( at->tid != (DAO_INTEGER + (code - DVM_SETF_KGII)%3) ) goto NotMatch;
			if( ct->tid != (DAO_INTEGER + (code - DVM_SETF_KGII)/3) ) goto NotMatch;
			break;
		case DVM_SETF_OG : 
		case DVM_SETF_OGII : case DVM_SETF_OGIF : case DVM_SETF_OGID : 
		case DVM_SETF_OGFI : case DVM_SETF_OGFF : case DVM_SETF_OGFD : 
		case DVM_SETF_OGDI : case DVM_SETF_OGDF : case DVM_SETF_OGDD : 
			ct = (DaoType*) type[opc];
			if( ct == NULL ) goto ErrorTyping;
			if( csts[opc] ) goto ModifyConstant;
			if( init[opa] ==0 || init[opc] ==0 ) goto NotInit;
			if( type[opa] ==NULL || type[opc] ==NULL ) goto NotMatch;
			if( ct->tid != DAO_OBJECT ) goto NotMatch;
			ct = ct->aux->xClass.glbDataType->items.pType[ opb ];
			if( code == DVM_SETF_OG ){
				if( at != ct && ct->tid != DAO_ANY ) goto NotMatch;
				break;
			}
			AssertTypeMatching( at, ct, defs, 0 );
			if( at->tid != (DAO_INTEGER + (code - DVM_SETF_OGII)%3) ) goto NotMatch;
			if( ct->tid != (DAO_INTEGER + (code - DVM_SETF_OGII)/3) ) goto NotMatch;
			break;
		case DVM_SETF_OV :
		case DVM_SETF_OVII : case DVM_SETF_OVIF : case DVM_SETF_OVID :
		case DVM_SETF_OVFI : case DVM_SETF_OVFF : case DVM_SETF_OVFD :
		case DVM_SETF_OVDI : case DVM_SETF_OVDF : case DVM_SETF_OVDD :
			ct = (DaoType*) type[opc];
			if( ct == NULL ) goto ErrorTyping;
			if( csts[opc] ) goto ModifyConstant;
			if( init[opa] ==0 || init[opc] ==0 ) goto NotInit;
			if( type[opa] ==NULL || type[opc] ==NULL ) goto NotMatch;
			if( ct->tid != DAO_OBJECT ) goto NotMatch;
			ct = ct->aux->xClass.objDataType->items.pType[ opb ];
			if( code == DVM_SETF_OV ){
				if( at != ct && ct->tid != DAO_ANY ) goto NotMatch;
				break;
			}
			AssertTypeMatching( at, ct, defs, 0 );
			if( at->tid != (DAO_INTEGER + (code - DVM_SETF_OVII)%3) ) goto NotMatch;
			if( ct->tid != (DAO_INTEGER + (code - DVM_SETF_OVII)/3) ) goto NotMatch;
			break;
#endif
		default : break;
		}
	}
	for(i=0,n=addRegType->size; i<n; i++){
		GC_IncRC( addRegType->items.pVoid[i] );
		DArray_Append( self->body->regType, addRegType->items.pVoid[i] );
	}
	DArray_Clear( self->body->simpleVariables );
	for(i=self->parCount,n=self->body->regType->size; i<n; i++){
		DaoType *tp = self->body->regType->items.pType[i];
		if( tp && tp->tid >= DAO_INTEGER && tp->tid <= DAO_ENUM ){
			DArray_Append( self->body->simpleVariables, (daoint)i );
		}
	}
	self->body->regCount = self->body->regType->size;
	for(j=0; j<addCount[0]; j++){
		DArray_Append( vmCodeNew, addCode->items.pVmc[0] );
		DArray_PopFront( addCode );
	}
	DArray_Append( vmCodeNew, self->body->annotCodes->items.pVmc[0] );
	for(i=1; i<N; i++){
		int c;
		DaoVmCodeX *vmc = self->body->annotCodes->items.pVmc[i];
		c = vmc->code;
		k = addCount[i] - addCount[i-1];
		for( j=0; j<k; j ++ ){
			DArray_Append( vmCodeNew, addCode->items.pVmc[0] );
			DArray_PopFront( addCode );
		}
		if( c ==DVM_GOTO || c ==DVM_TEST || c ==DVM_SWITCH || c == DVM_CASE
				|| ( c >=DVM_TEST_I && c <=DVM_TEST_D ) ){
			if( vmc->b >0 ) vmc->b += addCount[vmc->b-1];
		}else if( c ==DVM_CRRE && vmc->c >0 ){
			vmc->c += addCount[vmc->c-1];
		}
		DArray_Append( vmCodeNew, self->body->annotCodes->items.pVmc[i] );
	}
	DArray_CleanupCodes( vmCodeNew );
	DaoVmcArray_Resize( self->body->vmCodes, vmCodeNew->size );
	for(i=0,n=vmCodeNew->size; i<n; i++){
		self->body->vmCodes->codes[i] = * (DaoVmCode*) vmCodeNew->items.pVmc[i];
	}
	DArray_Swap( self->body->annotCodes, vmCodeNew );
	DArray_Delete( errors );
	DArray_Delete( vmCodeNew );
	DArray_Delete( addCode );
	DArray_Delete( rettypes );
	/*
	   DaoRoutine_PrintCode( self, self->nameSpace->vmSpace->errorStream );
	 */
	if( notide && daoConfig.jit && dao_jit.Compile ) dao_jit.Compile( self );

	GC_DecRCs( regConst );
	DArray_Delete( regConst );
	DArray_Delete( typeMaps );
	DArray_Delete( array );
	DMap_Delete( defs );
	DMap_Delete( defs2 );
	DMap_Delete( defs3 );
	DArray_Delete( addRegType );
	DString_Delete( mbs );
	dao_free( init );
	dao_free( addCount );
	return 1;

NotMatch :
	 ec = DTE_TYPE_NOT_MATCHING;
	 goto ErrorTyping;
NotInit :
	 ec = DTE_TYPE_NOT_INITIALIZED;
	 goto ErrorTyping;
NotPermit :
	 ec = DTE_FIELD_NOT_PERMIT;
	 goto ErrorTyping;
NotExist :
	 ec = DTE_FIELD_NOT_EXIST;
	 goto ErrorTyping;
NeedInstVar :
	 ec = DTE_FIELD_OF_INSTANCE;
	 goto ErrorTyping;
WrongContainer :
	 ec = DTE_TYPE_WRONG_CONTAINER;
	 goto ErrorTyping;
ModifyConstant:
	 ec = DTE_CONST_WRONG_MODIFYING;
	 goto ErrorTyping;
InvIndex :
	 ec = DTE_INDEX_NOT_VALID;
	 goto ErrorTyping;
InvKey :
	 ec = DTE_KEY_NOT_VALID;
	 goto ErrorTyping;
InvField :
	 ec = DTE_KEY_NOT_VALID;
	 goto ErrorTyping;
InvOper :
	 ec = DTE_OPERATION_NOT_VALID;
	 goto ErrorTyping;
InvParam :
	 ec = DTE_PARAM_ERROR;
	 goto ErrorTyping;
#if 0
	 FunctionNotImplemented:
	 ec = DTE_ROUT_NOT_IMPLEMENTED; goto ErrorTyping;
#endif

ErrorTyping:
	 if( silent ) goto SilentError;
	 vmc = self->body->annotCodes->items.pVmc[cid];
	 sprintf( char200, "%s:%i,%i,%i", getOpcodeName( vmc->code ), vmc->a, vmc->b, vmc->c );

	 DaoStream_WriteMBS( stream, "[[ERROR]] in file \"" );
	 DaoStream_WriteString( stream, self->nameSpace->name );
	 DaoStream_WriteMBS( stream, "\":\n" );
	 sprintf( char50, "  At line %i : ", self->defLine );
	 DaoStream_WriteMBS( stream, char50 );
	 DaoStream_WriteMBS( stream, "Invalid function definition --- \" " );
	 DaoStream_WriteString( stream, self->routName );
	 DaoStream_WriteMBS( stream, "() \";\n" );
	 sprintf( char50, "  At line %i : ", vmc->line );
	 DaoStream_WriteMBS( stream, char50 );
	 DaoStream_WriteMBS( stream, "Invalid virtual machine instruction --- \" " );
	 DaoStream_WriteMBS( stream, char200 );
	 DaoStream_WriteMBS( stream, " \";\n" );
	 if( ec_general == 0 ) ec_general = ec;
	 if( ec_general == 0 ) ec_general = DTE_OPERATION_NOT_VALID;
	 if( ec_general ){
		 DaoStream_WriteMBS( stream, char50 );
		 DaoStream_WriteMBS( stream, DaoTypingErrorString[ec_general] );
		 DaoStream_WriteMBS( stream, " --- \" " );
		 DaoTokens_AnnotateCode( self->body->source, *vmc, mbs, 32 );
		 DaoStream_WriteString( stream, mbs );
		 if( ec_general == DTE_FIELD_NOT_EXIST ){
			 DaoStream_WriteMBS( stream, " for " );
			 DaoStream_WriteMBS( stream, type_source->name->mbs );
		 }
		 DaoStream_WriteMBS( stream, " \";\n" );
	 }
	 if( ec_specific ){
		 DaoVmCodeX vmc2 = *vmc;
		 DaoStream_WriteMBS( stream, char50 );
		 DaoStream_WriteMBS( stream, DaoTypingErrorString[ec_specific] );
		 DaoStream_WriteMBS( stream, " --- \" " );
		 if( ec_specific == DTE_TYPE_NOT_INITIALIZED ){
			 vmc2.middle = 0;
			 vmc2.first += annot_first;
			 vmc2.last = annot_last > annot_first ? annot_last - annot_first : 0;
			 DaoTokens_AnnotateCode( self->body->source, vmc2, mbs, 32 );
		 }else if( ec_specific == DTE_TYPE_NOT_MATCHING ){
			 DString_SetMBS( mbs, "'" );
			 DString_AppendMBS( mbs, type_source ? type_source->name->mbs : "none" );
			 DString_AppendMBS( mbs, "' for '" );
			 if( type_target )
				 DString_AppendMBS( mbs, type_target->name->mbs );
			 else if( tid_target <= DAO_STREAM )
				 DString_AppendMBS( mbs, coreTypeNames[tid_target] );
			 DString_AppendChar( mbs, '\'' );
		 }else{
			 DaoTokens_AnnotateCode( self->body->source, *vmc, mbs, 32 );
		 }
		 DaoStream_WriteString( stream, mbs );
		 DaoStream_WriteMBS( stream, " \";\n" );
	 }
	 DaoPrintCallError( errors, stream );
SilentError:
	 DArray_Delete( errors );
	 dao_free( init );
	 dao_free( addCount );
	 for(i=0,n=addRegType->size; i<n; i++){
		 GC_IncRC( addRegType->items.pVoid[i] );
		 DArray_Append( self->body->regType, addRegType->items.pVoid[i] );
	 }
	 GC_DecRCs( regConst );
	 DArray_CleanupCodes( vmCodeNew );
	 DArray_Delete( rettypes );
	 DArray_Delete( regConst );
	 DArray_Delete( vmCodeNew );
	 DArray_Delete( addCode );
	 DArray_Delete( addRegType );
	 DArray_Delete( typeMaps );
	 DArray_Delete( array );
	 DString_Delete( mbs );
	 DMap_Delete( defs );
	 DMap_Delete( defs2 );
	 DMap_Delete( defs3 );
	 return 0;
}
