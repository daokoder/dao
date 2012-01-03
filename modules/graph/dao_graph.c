/*=========================================================================================
  This file is a part of the Dao standard modules.
  Copyright (C) 2011, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#include"daoGC.h"
#include"daoValue.h"
#include"dao_graph.h"

DAO_INIT_MODULE


/* @W: type of weights (U1, U2, U3) in nodes, (W1, W2, W3) in edges; */
/* @N: type of user data for nodes; */
/* @E: type of user data for edges; */

#define TYPE_PARAMS "<@W<none|int|float|double>=none,@N=none,@E=none>"

DaoxNode* DaoxNode_New( DaoxGraph *graph )
{
	DaoxNode *self = (DaoxNode*) dao_calloc( 1, sizeof(DaoxNode) );
	DaoCdata_InitCommon( (DaoCdata*) self, graph->nodeType );
	self->edges = DArray_New(D_VALUE);
	self->graph = graph;
	GC_IncRC( graph );
	return self;
}
void DaoxNode_Delete( DaoxNode *self )
{
	DaoCdata_FreeCommon( (DaoCdata*) self );
	DArray_Delete( self->edges );
	GC_DecRC( self->graph );
	dao_free( self );
}
void DaoxNode_SetValue( DaoxNode *self, DaoValue *value )
{
	DaoValue_Move( value, & self->value, self->ctype->nested->items.pType[1] );
}

DaoxEdge* DaoxEdge_New( DaoxGraph *graph )
{
	DaoxEdge *self = (DaoxEdge*) dao_calloc( 1, sizeof(DaoxEdge) );
	DaoCdata_InitCommon( (DaoCdata*) self, graph->edgeType );
	self->first = self->second = NULL;
	self->graph = graph;
	GC_IncRC( graph );
	return self;
}
void DaoxEdge_Delete( DaoxEdge *self )
{
	DaoCdata_FreeCommon( (DaoCdata*) self );
	GC_DecRC( self->graph );
	GC_DecRC( self->first );
	GC_DecRC( self->second );
	dao_free( self );
}
void DaoxEdge_SetValue( DaoxEdge *self, DaoValue *value )
{
	DaoValue_Move( value, & self->value, self->ctype->nested->items.pType[1] );
}

DaoxGraph* DaoxGraph_New( DaoType *type, int directed )
{
	DaoxGraph *self = (DaoxGraph*) dao_calloc( 1, sizeof(DaoxGraph) );
	DaoCdata_InitCommon( (DaoCdata*) self, type );
	self->nodes = DArray_New(D_VALUE);
	self->edges = DArray_New(D_VALUE);
	self->directed = directed;
	self->wtype = 0;
	self->nodeType = NULL;
	self->edgeType = NULL;
	if( type ){
		if( type->nested->size ) self->wtype = type->nested->items.pType[0]->tid;
		self->nodeType = DaoCdataType_Specialize( daox_node_template_type, type->nested );
		self->edgeType = DaoCdataType_Specialize( daox_edge_template_type, type->nested );
	}
	if( self->wtype > DAO_DOUBLE ) self->wtype = DAO_DOUBLE;
	return self;
}
void DaoxGraph_Delete( DaoxGraph *self )
{
	DaoCdata_FreeCommon( (DaoCdata*) self );
	DArray_Delete( self->nodes );
	DArray_Delete( self->edges );
	dao_free( self );
}

DaoxNode* DaoxGraph_AddNode( DaoxGraph *self )
{
	DaoxNode *node = DaoxNode_New( self );
	DArray_Append( self->nodes, node );
	return node;
}
DaoxEdge* DaoxGraph_AddEdge( DaoxGraph *self, DaoxNode *first, DaoxNode *second )
{
	DaoxEdge *edge = DaoxEdge_New( self );
	if( self->directed ){
		DArray_PushFront( first->edges, edge );
	}else{
		DArray_PushBack( first->edges, edge );
	}
	DArray_PushBack( second->edges, edge );

	DArray_Append( self->edges, edge );
	GC_ShiftRC( first, edge->first );
	GC_ShiftRC( second, edge->second );
	edge->first = first;
	edge->second = second;
	return edge;
}

static void DaoxNode_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int remove )
{
	DaoxNode *self = (DaoxNode*) p;
	if( self->graph ) DArray_Append( values, self->graph );
	if( self->value ) DArray_Append( values, self->value );
	DArray_Append( arrays, self->edges );
	if( remove ) self->graph = NULL;
	if( remove ) self->value = NULL;
}
static void DaoxEdge_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int remove )
{
	DaoxEdge *self = (DaoxEdge*) p;
	if( self->graph ) DArray_Append( values, self->graph );
	if( self->first ) DArray_Append( values, self->first );
	if( self->second ) DArray_Append( values, self->second );
	if( self->value ) DArray_Append( values, self->value );
	if( remove ){
		self->graph = NULL;
		self->first = NULL;
		self->second = NULL;
		self->value = NULL;
	}
}
static void DaoxGraph_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int remove )
{
	DaoxGraph *self = (DaoxGraph*) p;
	DArray_Append( arrays, self->nodes );
	DArray_Append( arrays, self->edges );
}


/*****************************************************************/
/* Maximum Flow: Relabel-second-front algorithm, with FIFO heuristic */
/*****************************************************************/

/* DaoxNode: U1.i, height; */
/* DaoxNode: U2, excess; */
/* DaoxEdge: W1, capacity; */
/* DaoxEdge: W2, forward flow; */
/* DaoxEdge: W3, backward flow; */

/* For integer type weighted network: */
static void MaxFlow_PushInt( DaoxNode *node, DaoxEdge *edge )
{
	DaoxNode *U = node;
	DaoxNode *V = edge->second;
	dint  CUV =   edge->W1.I;
	dint *FUV = & edge->W2.I;
	dint *FVU = & edge->W3.I;
	dint send;
	if( node == edge->second ){
		V = edge->first;
		CUV = 0;
		FUV = & edge->W3.I;
		FVU = & edge->W2.I;
	}
	send = CUV - (*FUV);
	if( U->U2.I < send ) send = U->U2.I;
	*FUV += send;
	*FVU -= send;
	U->U2.I -= send;
	V->U2.I += send;
}
static void MaxFlow_RelabelInt( DaoxNode *U )
{
	dint min_height = 100 * U->graph->nodes->size;
	size_t i, n;
	for(i=0,n=U->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) U->edges->items.pValue[i];
		if( U == edge->first ){ /* out edges */
			DaoxNode *V = edge->second;
			if( (edge->W1.I > edge->W2.I) && (V->U1.I < min_height) ) min_height = V->U1.I;
		}else{ /* in edges */
			DaoxNode *V = edge->first;
			if( (0 > edge->W3.I) && (V->U1.I < min_height) ) min_height = V->U1.I;
		}
	}
	U->U1.I = min_height + 1;
}
static void MaxFlow_DischargeInt( DaoxNode *U )
{
	size_t i, n;
	while( U->U2.I > 0 ){
		for(i=0,n=U->edges->size; i<n; i++){
			DaoxEdge *edge = (DaoxEdge*) U->edges->items.pValue[i];
			if( U == edge->first ){ /* out edges */
				DaoxNode *V = edge->second;
				if( (edge->W1.I > edge->W2.I) && (U->U1.I > V->U1.I) ) MaxFlow_PushInt( U, edge );
			}else{ /* in edges */
				DaoxNode *V = edge->first;
				if( (0 > edge->W3.I) && (U->U1.I > V->U1.I) ) MaxFlow_PushInt( U, edge );
			}
		}
		MaxFlow_RelabelInt( U );
	}
}
dint DaoxGraph_MaxFlow_PRTF_Int( DaoxGraph *self, DaoxNode *source, DaoxNode *sink )
{
	size_t i, n;
	dint inf = 0;
	DArray *list = DArray_New(0);

	for(i=0,n=source->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->edges->items.pValue[i];
		if( source == edge->first ) inf += edge->W1.I;
	}
	for(i=0,n=self->nodes->size; i<n; i++){
		DaoxNode *node = (DaoxNode*) self->nodes->items.pValue[i];
		node->U1.I = n;
		node->U2.I = inf;
	}
	for(i=0,n=self->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) self->edges->items.pValue[i];
		edge->W2.I = edge->W3.I = 0;
	}
	for(i=0,n=source->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->edges->items.pValue[i];
		if( source == edge->first ) MaxFlow_PushInt( source, edge );
	}
	i = 0;
	while( i < list->size ){
		DaoxNode *U = (DaoxNode*) list->items.pValue[i];
		dint old_height = U->U1.I;
		MaxFlow_DischargeInt( U );
		if( U->U1.I > old_height ){
			DArray_Erase( list, i, 1 );
			DArray_PushFront( list, U );
			i = 0;
		}else{
			i += 1;
		}
	}
	DArray_Delete( list );
	inf = 0;
	for(i=0,n=source->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->edges->items.pValue[i];
		if( source == edge->first ) inf += edge->W2.I;
	}
	return inf;
}
/* For float type weighted network: */
static void MaxFlow_PushFloat( DaoxNode *node, DaoxEdge *edge )
{
	DaoxNode *U = node;
	DaoxNode *V = edge->second;
	float  CUV =   edge->W1.F;
	float *FUV = & edge->W2.F;
	float *FVU = & edge->W3.F;
	float send;
	if( node == edge->second ){
		V = edge->first;
		CUV = 0;
		FUV = & edge->W3.F;
		FVU = & edge->W2.F;
	}
	send = CUV - (*FUV);
	if( U->U2.F < send ) send = U->U2.F;
	*FUV += send;
	*FVU -= send;
	U->U2.F -= send;
	V->U2.F += send;
}
static void MaxFlow_RelabelFloat( DaoxNode *U )
{
	dint min_height = 100 * U->graph->nodes->size;
	size_t i, n;
	for(i=0,n=U->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) U->edges->items.pValue[i];
		if( U == edge->first ){ /* out edges */
			DaoxNode *V = edge->second;
			if( (edge->W1.F > edge->W2.F) && (V->U1.I < min_height) ) min_height = V->U1.I;
		}else{ /* in edges */
			DaoxNode *V = edge->first;
			if( (0 > edge->W3.F) && (V->U1.I < min_height) ) min_height = V->U1.I;
		}
	}
	U->U1.I = min_height + 1;
}
static void MaxFlow_DischargeFloat( DaoxNode *U )
{
	size_t i, n;
	while( U->U2.F > 0 ){
		for(i=0,n=U->edges->size; i<n; i++){
			DaoxEdge *edge = (DaoxEdge*) U->edges->items.pValue[i];
			if( U == edge->first ){ /* out edges */
				DaoxNode *V = edge->second;
				if( (edge->W1.F > edge->W2.F) && (U->U1.I > V->U1.I) ) MaxFlow_PushFloat( U, edge );
			}else{ /* in edges */
				DaoxNode *V = edge->first;
				if( (0 > edge->W3.F) && (U->U1.I > V->U1.I) ) MaxFlow_PushFloat( U, edge );
			}
		}
		MaxFlow_RelabelFloat( U );
	}
}
float DaoxGraph_MaxFlow_PRTF_Float( DaoxGraph *self, DaoxNode *source, DaoxNode *sink )
{
	size_t i, n;
	float inf = 0;
	DArray *list = DArray_New(0);

	for(i=0,n=source->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->edges->items.pValue[i];
		if( source == edge->first ) inf += edge->W1.F;
	}
	for(i=0,n=self->nodes->size; i<n; i++){
		DaoxNode *node = (DaoxNode*) self->nodes->items.pValue[i];
		node->U1.I = n;
		node->U2.F = inf;
	}
	for(i=0,n=self->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) self->edges->items.pValue[i];
		edge->W2.F = edge->W3.F = 0;
	}
	for(i=0,n=source->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->edges->items.pValue[i];
		if( source == edge->first ) MaxFlow_PushFloat( source, edge );
	}
	i = 0;
	while( i < list->size ){
		DaoxNode *U = (DaoxNode*) list->items.pValue[i];
		dint old_height = U->U1.I;
		MaxFlow_DischargeFloat( U );
		if( U->U1.I > old_height ){
			DArray_Erase( list, i, 1 );
			DArray_PushFront( list, U );
			i = 0;
		}else{
			i += 1;
		}
	}
	DArray_Delete( list );
	inf = 0;
	for(i=0,n=source->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->edges->items.pValue[i];
		if( source == edge->first ) inf += edge->W2.F;
	}
	return inf;
}
/* For double type weighted network: */
static void MaxFlow_PushDouble( DaoxNode *node, DaoxEdge *edge )
{
	DaoxNode *U = node;
	DaoxNode *V = edge->second;
	double  CUV =   edge->W1.D;
	double *FUV = & edge->W2.D;
	double *FVU = & edge->W3.D;
	double send;
	if( node == edge->second ){
		V = edge->first;
		CUV = 0;
		FUV = & edge->W3.D;
		FVU = & edge->W2.D;
	}
	send = CUV - (*FUV);
	if( U->U2.D < send ) send = U->U2.D;
	*FUV += send;
	*FVU -= send;
	U->U2.D -= send;
	V->U2.D += send;
}
static void MaxFlow_RelabelDouble( DaoxNode *U )
{
	dint min_height = 100 * U->graph->nodes->size;
	size_t i, n;
	for(i=0,n=U->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) U->edges->items.pValue[i];
		if( U == edge->first ){ /* out edges */
			DaoxNode *V = edge->second;
			if( (edge->W1.D > edge->W2.D) && (V->U1.I < min_height) ) min_height = V->U1.I;
		}else{ /* in edges */
			DaoxNode *V = edge->first;
			if( (0 > edge->W3.D) && (V->U1.I < min_height) ) min_height = V->U1.I;
		}
	}
	U->U1.I = min_height + 1;
}
static void MaxFlow_DischargeDouble( DaoxNode *U )
{
	size_t i, n;
	while( U->U2.D > 0 ){
		for(i=0,n=U->edges->size; i<n; i++){
			DaoxEdge *edge = (DaoxEdge*) U->edges->items.pValue[i];
			if( U == edge->first ){ /* out edges */
				DaoxNode *V = edge->second;
				if( (edge->W1.D > edge->W2.D) && (U->U1.I > V->U1.I) ) MaxFlow_PushDouble( U, edge );
			}else{ /* in edges */
				DaoxNode *V = edge->first;
				if( (0 > edge->W3.D) && (U->U1.I > V->U1.I) ) MaxFlow_PushDouble( U, edge );
			}
		}
		MaxFlow_RelabelDouble( U );
	}
}
double DaoxGraph_MaxFlow_PRTF_Double( DaoxGraph *self, DaoxNode *source, DaoxNode *sink )
{
	size_t i, n;
	double inf = 0;
	DArray *list = DArray_New(0);

	for(i=0,n=source->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->edges->items.pValue[i];
		if( source == edge->first ) inf += edge->W1.D;
	}
	for(i=0,n=self->nodes->size; i<n; i++){
		DaoxNode *node = (DaoxNode*) self->nodes->items.pValue[i];
		node->U1.I = n;
		node->U2.D = inf;
	}
	for(i=0,n=self->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) self->edges->items.pValue[i];
		edge->W2.D = edge->W3.D = 0;
	}
	for(i=0,n=source->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->edges->items.pValue[i];
		if( source == edge->first ) MaxFlow_PushDouble( source, edge );
	}
	i = 0;
	while( i < list->size ){
		DaoxNode *U = (DaoxNode*) list->items.pValue[i];
		dint old_height = U->U1.I;
		MaxFlow_DischargeDouble( U );
		if( U->U1.I > old_height ){
			DArray_Erase( list, i, 1 );
			DArray_PushFront( list, U );
			i = 0;
		}else{
			i += 1;
		}
	}
	DArray_Delete( list );
	inf = 0;
	for(i=0,n=source->edges->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->edges->items.pValue[i];
		if( source == edge->first ) inf += edge->W2.D;
	}
	return inf;
}
double DaoxGraph_MaxFlow_PushRelabelToFront( DaoxGraph *self, DaoxNode *source, DaoxNode *sink )
{
	switch( self->wtype ){
	case DAO_INTEGER : return DaoxGraph_MaxFlow_PRTF_Int( self, source, sink );
	case DAO_FLOAT   : return DaoxGraph_MaxFlow_PRTF_Float( self, source, sink );
	case DAO_DOUBLE  : return DaoxGraph_MaxFlow_PRTF_Double( self, source, sink );
	}
	return 0;
}


/***************************/
/* Interface second Dao        */
/***************************/

static void NODE_GetWeight( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxNode *self = (DaoxNode*) p[0];
	switch( self->graph->wtype ){
	case DAO_INTEGER : DaoProcess_PutInteger( proc, self->U1.I ); break;
	case DAO_FLOAT   : DaoProcess_PutFloat( proc, self->U1.F ); break;
	case DAO_DOUBLE  : DaoProcess_PutDouble( proc, self->U1.D ); break;
	}
}
static void NODE_SetWeight( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxNode *self = (DaoxNode*) p[0];
	switch( self->graph->wtype ){
	case DAO_INTEGER : self->U1.I = p[1]->xInteger.value; break;
	case DAO_FLOAT   : self->U1.F = p[1]->xFloat.value; break;
	case DAO_DOUBLE  : self->U1.D = p[1]->xDouble.value; break;
	}
}
static void NODE_GetWeights( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoTuple *tuple = DaoProcess_PutTuple( proc );
	DaoxNode *self = (DaoxNode*) p[0];
	switch( self->graph->wtype ){
	case DAO_INTEGER :
		tuple->items[0]->xInteger.value = self->U1.I;
		tuple->items[1]->xInteger.value = self->U2.I;
		tuple->items[2]->xInteger.value = self->U3.I;
		break;
	case DAO_FLOAT   :
		tuple->items[0]->xFloat.value = self->U1.F;
		tuple->items[1]->xFloat.value = self->U2.F;
		tuple->items[2]->xFloat.value = self->U3.F;
		break;
	case DAO_DOUBLE  :
		tuple->items[0]->xDouble.value = self->U1.D;
		tuple->items[1]->xDouble.value = self->U2.D;
		tuple->items[2]->xDouble.value = self->U3.D;
		break;
	}
}
static void NODE_SetWeights( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxNode *self = (DaoxNode*) p[0];
	DaoTuple *tuple = (DaoTuple*) p[1];
	switch( self->graph->wtype ){
	case DAO_INTEGER :
		self->U1.I = tuple->items[0]->xInteger.value;
		self->U2.I = tuple->items[1]->xInteger.value;
		self->U3.I = tuple->items[2]->xInteger.value;
		break;
	case DAO_FLOAT   :
		self->U1.F = tuple->items[0]->xFloat.value;
		self->U2.F = tuple->items[1]->xFloat.value;
		self->U3.F = tuple->items[2]->xFloat.value;
		break;
	case DAO_DOUBLE  :
		self->U1.D = tuple->items[0]->xDouble.value;
		self->U2.D = tuple->items[1]->xDouble.value;
		self->U3.D = tuple->items[2]->xDouble.value;
		break;
	}
}
static void NODE_GetValue( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxNode *self = (DaoxNode*) p[0];
	DaoProcess_PutValue( proc, self->value );
}
static void NODE_SetValue( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxNode_SetValue( (DaoxNode*) p[0], p[1] );
}
static void NODE_GetEdges( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxNode *self = (DaoxNode*) p[0];
	DaoList *res = DaoProcess_PutList( proc );
	size_t i, n;
	if( p[1]->xEnum.value == 0 ){
		for(i=0,n=self->edges->size; i>0; i--){
			DaoxEdge *edge = (DaoxEdge*) self->edges->items.pValue[i-1];
			if( self != edge->second ) break;
			DaoList_PushBack( res, (DaoValue*) edge );
		}
	}else{
		for(i=0; i<self->edges->size; i++){
			DaoxEdge *edge = (DaoxEdge*) self->edges->items.pValue[i];
			if( self != edge->first ) break;
			DaoList_PushBack( res, (DaoValue*) edge );
		}
	}
}
static void EDGE_GetWeight( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxEdge *self = (DaoxEdge*) p[0];
	switch( self->graph->wtype ){
	case DAO_INTEGER : DaoProcess_PutInteger( proc, self->W1.I ); break;
	case DAO_FLOAT   : DaoProcess_PutFloat( proc, self->W1.F ); break;
	case DAO_DOUBLE  : DaoProcess_PutDouble( proc, self->W1.D ); break;
	}
}
static void EDGE_SetWeight( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxEdge *self = (DaoxEdge*) p[0];
	switch( self->graph->wtype ){
	case DAO_INTEGER : self->W1.I = p[1]->xInteger.value; break;
	case DAO_FLOAT   : self->W1.F = p[1]->xFloat.value; break;
	case DAO_DOUBLE  : self->W1.D = p[1]->xDouble.value; break;
	}
}
static void EDGE_GetWeights( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoTuple *tuple = DaoProcess_PutTuple( proc );
	DaoxEdge *self = (DaoxEdge*) p[0];
	switch( self->graph->wtype ){
	case DAO_INTEGER :
		tuple->items[0]->xInteger.value = self->W1.I;
		tuple->items[1]->xInteger.value = self->W2.I;
		tuple->items[2]->xInteger.value = self->W3.I;
		break;
	case DAO_FLOAT   :
		tuple->items[0]->xFloat.value = self->W1.F;
		tuple->items[1]->xFloat.value = self->W2.F;
		tuple->items[2]->xFloat.value = self->W3.F;
		break;
	case DAO_DOUBLE  :
		tuple->items[0]->xDouble.value = self->W1.D;
		tuple->items[1]->xDouble.value = self->W2.D;
		tuple->items[2]->xDouble.value = self->W3.D;
		break;
	}
}
static void EDGE_SetWeights( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxEdge *self = (DaoxEdge*) p[0];
	DaoTuple *tuple = (DaoTuple*) p[1];
	switch( self->graph->wtype ){
	case DAO_INTEGER :
		self->W1.I = tuple->items[0]->xInteger.value;
		self->W2.I = tuple->items[1]->xInteger.value;
		self->W3.I = tuple->items[2]->xInteger.value;
		break;
	case DAO_FLOAT   :
		self->W1.F = tuple->items[0]->xFloat.value;
		self->W2.F = tuple->items[1]->xFloat.value;
		self->W3.F = tuple->items[2]->xFloat.value;
		break;
	case DAO_DOUBLE  :
		self->W1.D = tuple->items[0]->xDouble.value;
		self->W2.D = tuple->items[1]->xDouble.value;
		self->W3.D = tuple->items[2]->xDouble.value;
		break;
	}
}
static void EDGE_GetValue( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxEdge *self = (DaoxEdge*) p[0];
	DaoProcess_PutValue( proc, self->value );
}
static void EDGE_SetValue( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxEdge_SetValue( (DaoxEdge*) p[0], p[1] );
}
static void EDGE_GetNodes( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxEdge *self = (DaoxEdge*) p[0];
	DaoTuple *res = DaoProcess_PutTuple( proc );
	DaoTuple_SetItem( res, (DaoValue*)self->first, 0 );
	DaoTuple_SetItem( res, (DaoValue*)self->second, 1 );
}
static void GRAPH_Graph( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoType *retype = DaoProcess_GetReturnType( proc );
	DaoxGraph *graph = DaoxGraph_New( retype, p[0]->xEnum.value );
	DaoValue *res = DaoProcess_PutValue( proc, (DaoValue*) graph );
}
static void GRAPH_GetNodes( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxGraph *self = (DaoxGraph*) p[0];
	DaoList *res = DaoProcess_PutList( proc );
	size_t i;
	for(i=0; i<self->nodes->size; i++) DaoList_PushBack( res, self->nodes->items.pValue[i] );
}
static void GRAPH_GetEdges( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxGraph *self = (DaoxGraph*) p[0];
	DaoList *res = DaoProcess_PutList( proc );
	size_t i;
	for(i=0; i<self->edges->size; i++) DaoList_PushBack( res, self->edges->items.pValue[i] );
}
static void GRAPH_AddNode( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxNode *node = DaoxGraph_AddNode( (DaoxGraph*) p[0] );
	DaoProcess_PutValue( proc, (DaoValue*) node );
}
static void GRAPH_AddEdge( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxEdge *edge = DaoxGraph_AddEdge( (DaoxGraph*) p[0], (DaoxNode*) p[1], (DaoxNode*) p[2] );
	DaoProcess_PutValue( proc, (DaoValue*) edge );
}
static void GRAPH_MaxFlow( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxGraph *self = (DaoxGraph*) p[0];
	DaoxNode *source = (DaoxNode*) p[1];
	DaoxNode *sink = (DaoxNode*) p[2];
	if( self->wtype == DAO_INTEGER ){
		dint maxflow = DaoxGraph_MaxFlow_PushRelabelToFront( self, source, sink );
		DaoProcess_PutInteger( proc, maxflow );
	}else if( self->wtype == DAO_FLOAT ){
		float maxflow = DaoxGraph_MaxFlow_PushRelabelToFront( self, source, sink );
		DaoProcess_PutFloat( proc, maxflow );
	}else{
		double maxflow = DaoxGraph_MaxFlow_PushRelabelToFront( self, source, sink );
		DaoProcess_PutDouble( proc, maxflow );
	}
}


static DaoFuncItem DaoxNodeMeths[]=
{
	{ NODE_GetWeight, "GetWeight( self :Node<@W,@N,@E> ) => @W" },
	{ NODE_SetWeight, "SetWeight( self :Node<@W,@N,@E>, weight :@W )" },
	{ NODE_GetWeights, "GetWeights( self :Node<@W,@N,@E> ) => tuple<U1:@W,U2:@W,U3:@W>" },
	{ NODE_SetWeights, "SetWeights( self :Node<@W,@N,@E>, weights :tuple<U1:@W,U2:@W,U3:@W> )" },
	{ NODE_GetValue, "GetValue( self :Node<@W,@N,@E> ) => @N" },
	{ NODE_SetValue, "SetValue( self :Node<@W,@N,@E>, value :@N )" },
	{ NODE_GetEdges, "Edges( self :Node<@W,@N,@E>, set :enum<in,out> = $out ) => list<Edge<@W,@N,@E>>" },
	{ NULL, NULL }
};

DaoTypeBase DaoxNode_Typer =
{
	"Node"TYPE_PARAMS, NULL, NULL, (DaoFuncItem*) DaoxNodeMeths, {0}, {0},
	(FuncPtrDel)DaoxNode_Delete, DaoxNode_GetGCFields
};

static DaoFuncItem DaoxEdgeMeths[]=
{
	{ EDGE_GetWeight, "GetWeight( self :Edge<@W,@N,@E> ) => @W" },
	{ EDGE_SetWeight, "SetWeight( self :Edge<@W,@N,@E>, weight :@W )" },
	{ EDGE_GetWeights, "GetWeights( self :Edge<@W,@N,@E> ) => tuple<W1:@W,W2:@W,W3:@W>" },
	{ EDGE_SetWeights, "SetWeights( self :Edge<@W,@N,@E>, weights :tuple<W1:@W,W2:@W,W3:@W> )" },
	{ EDGE_GetValue, "GetValue( self :Edge<@W,@N,@E> ) => @E" },
	{ EDGE_SetValue, "SetValue( self :Edge<@W,@N,@E>, value :@E )" },
	{ EDGE_GetNodes, "Nodes( self :Edge<@W,@N,@E> ) => tuple<first:Node<@W,@N,@E>,second:Node<@W,@N,@E>>" },
	{ NULL, NULL }
};

DaoTypeBase DaoxEdge_Typer =
{
	"Edge"TYPE_PARAMS, NULL, NULL, (DaoFuncItem*) DaoxEdgeMeths, {0}, {0},
	(FuncPtrDel)DaoxEdge_Delete, DaoxEdge_GetGCFields
};


static DaoFuncItem DaoxGraphMeths[]=
{
	/* allocaters must have names identical second the typer name: */
	{ GRAPH_Graph,    "Graph"TYPE_PARAMS"( dir :enum<undirected,directed>=$undirected )" },
	{ GRAPH_GetNodes, "Nodes( self :Graph<@W,@N,@E> ) => list<Node<@W,@N,@E>>" },
	{ GRAPH_GetEdges, "Edges( self :Graph<@W,@N,@E> ) => list<Edge<@W,@N,@E>>" },
	{ GRAPH_AddNode, "AddNode( self :Graph<@W,@N,@E> ) => Node<@W,@N,@E>" },
	{ GRAPH_AddEdge, "AddEdge( self :Graph<@W,@N,@E>, first :Node<@W,@N,@E>, second :Node<@W,@N,@E> ) => Edge<@W,@N,@E>" },
	{ GRAPH_MaxFlow, "MaxFlow( self :Graph<@W,@N,@E>, source :Node<@W,@N,@E>, sink :Node<@W,@N,@E> ) => @W" },
	{ NULL, NULL }
};

DaoTypeBase DaoxGraph_Typer =
{
	"Graph"TYPE_PARAMS, NULL, NULL, (DaoFuncItem*) DaoxGraphMeths, {0}, {0},
	(FuncPtrDel)DaoxGraph_Delete, DaoxGraph_GetGCFields
};

DaoType *daox_node_template_type = NULL;
DaoType *daox_edge_template_type = NULL;
DaoType *daox_graph_template_type = NULL;

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	daox_node_template_type = DaoNamespace_WrapType( ns, & DaoxNode_Typer, 0 );
	daox_edge_template_type = DaoNamespace_WrapType( ns, & DaoxEdge_Typer, 0 );
	daox_graph_template_type = DaoNamespace_WrapType( ns, & DaoxGraph_Typer, 0 );
	return 0;
}
