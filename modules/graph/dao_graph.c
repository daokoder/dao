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


/* TUW: type of weights (U1, U2, U3) in nodes, (W1, W2, W3) in edges; */
/* TNV: type of user data for nodes; */
/* TEV: type of user data for edges; */

#define TYPE_PARAMS "<@TUW<none|int|float|double>=none,@TNV=none,@TEV=none>"

DaoxNode* DaoxNode_New( DaoxGraph *graph )
{
	DaoxNode *self = (DaoxNode*) dao_calloc( 1, sizeof(DaoxNode) );
	DaoCdata_InitCommon( (DaoCdata*) self, & DaoxNode_Typer );
	self->ins = DArray_New(D_VALUE);
	self->outs = DArray_New(D_VALUE);
	self->graph = graph;
	GC_IncRC( graph );
	return self;
}
void DaoxNode_Delete( DaoxNode *self )
{
	DaoCdata_FreeCommon( (DaoCdata*) self );
	DArray_Delete( self->ins );
	DArray_Delete( self->outs );
	GC_DecRC( self->graph );
	dao_free( self );
}

DaoxEdge* DaoxEdge_New( DaoxGraph *graph )
{
	DaoxEdge *self = (DaoxEdge*) dao_calloc( 1, sizeof(DaoxEdge) );
	DaoCdata_InitCommon( (DaoCdata*) self, & DaoxEdge_Typer );
	self->from = self->to = NULL;
	self->graph = graph;
	GC_IncRC( graph );
	return self;
}
void DaoxEdge_Delete( DaoxEdge *self )
{
	DaoCdata_FreeCommon( (DaoCdata*) self );
	GC_DecRC( self->graph );
	GC_DecRC( self->from );
	GC_DecRC( self->to );
	dao_free( self );
}

DaoxGraph* DaoxGraph_New( int wtype, int directed )
{
	DaoxGraph *self = (DaoxGraph*) dao_calloc( 1, sizeof(DaoxGraph) );
	DaoCdata_InitCommon( (DaoCdata*) self, & DaoxGraph_Typer );
	self->nodes = DArray_New(D_VALUE);
	self->edges = DArray_New(D_VALUE);
	self->wtype = wtype;
	self->directed = directed;
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
	DaoType *type = DaoCdataType_Specialize( node->ctype, self->ctype->nested );
	if( type ){
		GC_ShiftRC( type, node->ctype );
		node->ctype = type;
	}
	DArray_Append( self->nodes, node );
	return node;
}
DaoxEdge* DaoxGraph_AddEdge( DaoxGraph *self, DaoxNode *from, DaoxNode *to )
{
	DaoxEdge *edge = DaoxEdge_New( self );
	DaoType *type = DaoCdataType_Specialize( edge->ctype, self->ctype->nested );
	if( type ){
		GC_ShiftRC( type, edge->ctype );
		edge->ctype = type;
	}
	DArray_Append( self->edges, edge );
	DArray_Append( from->outs, edge );
	DArray_Append( to->ins, edge );
	GC_ShiftRC( from, edge->from );
	GC_ShiftRC( to, edge->to );
	edge->from = from;
	edge->to = to;
	return edge;
}

static void DaoxNode_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int remove )
{
	DaoxNode *self = (DaoxNode*) p;
	if( self->graph ) DArray_Append( values, self->graph );
	DArray_Append( arrays, self->ins );
	DArray_Append( arrays, self->outs );
	if( remove ) self->graph = NULL;
}
static void DaoxEdge_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int remove )
{
	DaoxEdge *self = (DaoxEdge*) p;
	if( self->graph ) DArray_Append( values, self->graph );
	if( self->from ) DArray_Append( values, self->from );
	if( self->to ) DArray_Append( values, self->to );
	if( remove ){
		self->graph = NULL;
		self->from = NULL;
		self->to = NULL;
	}
}
static void DaoxGraph_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int remove )
{
	DaoxGraph *self = (DaoxGraph*) p;
	DArray_Append( arrays, self->nodes );
	DArray_Append( arrays, self->edges );
}


/*****************************************************************/
/* Maximum Flow: Relabel-to-front algorithm, with FIFO heuristic */
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
	DaoxNode *V = edge->to;
	dint  CUV =   edge->W1.I;
	dint *FUV = & edge->W2.I;
	dint *FVU = & edge->W3.I;
	dint send;
	if( node == edge->to ){
		V = edge->from;
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
	for(i=0,n=U->outs->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) U->outs->items.pValue[i];
		DaoxNode *V = edge->to;
		if( (edge->W1.I > edge->W2.I) && (V->U1.I < min_height) ) min_height = V->U1.I;
	}
	for(i=0,n=U->ins->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) U->ins->items.pValue[i];
		DaoxNode *V = edge->from;
		if( (0 > edge->W3.I) && (V->U1.I < min_height) ) min_height = V->U1.I;
	}
	U->U1.I = min_height + 1;
}
static void MaxFlow_DischargeInt( DaoxNode *U )
{
	size_t i, n;
	while( U->U2.I > 0 ){
		for(i=0,n=U->outs->size; i<n; i++){
			DaoxEdge *edge = (DaoxEdge*) U->outs->items.pValue[i];
			DaoxNode *V = edge->to;
			if( (edge->W1.I > edge->W2.I) && (U->U1.I > V->U1.I) ) MaxFlow_PushInt( U, edge );
		}
		for(i=0,n=U->ins->size; i<n; i++){
			DaoxEdge *edge = (DaoxEdge*) U->ins->items.pValue[i];
			DaoxNode *V = edge->from;
			if( (0 > edge->W3.I) && (U->U1.I > V->U1.I) ) MaxFlow_PushInt( U, edge );
		}
		MaxFlow_RelabelInt( U );
	}
}
dint DaoxGraph_MaxFlow_PRTF_Int( DaoxGraph *self, DaoxNode *source, DaoxNode *sink )
{
	size_t i, n;
	dint inf = 0;
	DArray *list = DArray_New(0);

	for(i=0,n=source->outs->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->outs->items.pValue[i];
		inf += edge->W1.I;
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
	for(i=0,n=source->outs->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->outs->items.pValue[i];
		MaxFlow_PushInt( source, edge );
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
	for(i=0,n=source->outs->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->outs->items.pValue[i];
		inf += edge->W2.I;
	}
	return inf;
}
/* For float type weighted network: */
static void MaxFlow_PushFloat( DaoxNode *node, DaoxEdge *edge )
{
	DaoxNode *U = node;
	DaoxNode *V = edge->to;
	float  CUV =   edge->W1.F;
	float *FUV = & edge->W2.F;
	float *FVU = & edge->W3.F;
	float send;
	if( node == edge->to ){
		V = edge->from;
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
	for(i=0,n=U->outs->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) U->outs->items.pValue[i];
		DaoxNode *V = edge->to;
		if( (edge->W1.F > edge->W2.F) && (V->U1.I < min_height) ) min_height = V->U1.I;
	}
	for(i=0,n=U->ins->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) U->ins->items.pValue[i];
		DaoxNode *V = edge->from;
		if( (0 > edge->W3.F) && (V->U1.I < min_height) ) min_height = V->U1.I;
	}
	U->U1.I = min_height + 1;
}
static void MaxFlow_DischargeFloat( DaoxNode *U )
{
	size_t i, n;
	while( U->U2.F > 0 ){
		for(i=0,n=U->outs->size; i<n; i++){
			DaoxEdge *edge = (DaoxEdge*) U->outs->items.pValue[i];
			DaoxNode *V = edge->to;
			if( (edge->W1.F > edge->W2.F) && (U->U1.I > V->U1.I) ) MaxFlow_PushFloat( U, edge );
		}
		for(i=0,n=U->ins->size; i<n; i++){
			DaoxEdge *edge = (DaoxEdge*) U->ins->items.pValue[i];
			DaoxNode *V = edge->from;
			if( (0 > edge->W3.F) && (U->U1.I > V->U1.I) ) MaxFlow_PushFloat( U, edge );
		}
		MaxFlow_RelabelFloat( U );
	}
}
float DaoxGraph_MaxFlow_PRTF_Float( DaoxGraph *self, DaoxNode *source, DaoxNode *sink )
{
	size_t i, n;
	float inf = 0;
	DArray *list = DArray_New(0);

	for(i=0,n=source->outs->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->outs->items.pValue[i];
		inf += edge->W1.F;
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
	for(i=0,n=source->outs->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->outs->items.pValue[i];
		MaxFlow_PushFloat( source, edge );
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
	for(i=0,n=source->outs->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->outs->items.pValue[i];
		inf += edge->W2.F;
	}
	return inf;
}
/* For double type weighted network: */
static void MaxFlow_PushDouble( DaoxNode *node, DaoxEdge *edge )
{
	DaoxNode *U = node;
	DaoxNode *V = edge->to;
	double  CUV =   edge->W1.D;
	double *FUV = & edge->W2.D;
	double *FVU = & edge->W3.D;
	double send;
	if( node == edge->to ){
		V = edge->from;
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
	for(i=0,n=U->outs->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) U->outs->items.pValue[i];
		DaoxNode *V = edge->to;
		if( (edge->W1.D > edge->W2.D) && (V->U1.I < min_height) ) min_height = V->U1.I;
	}
	for(i=0,n=U->ins->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) U->ins->items.pValue[i];
		DaoxNode *V = edge->from;
		if( (0 > edge->W3.D) && (V->U1.I < min_height) ) min_height = V->U1.I;
	}
	U->U1.I = min_height + 1;
}
static void MaxFlow_DischargeDouble( DaoxNode *U )
{
	size_t i, n;
	while( U->U2.D > 0 ){
		for(i=0,n=U->outs->size; i<n; i++){
			DaoxEdge *edge = (DaoxEdge*) U->outs->items.pValue[i];
			DaoxNode *V = edge->to;
			if( (edge->W1.D > edge->W2.D) && (U->U1.I > V->U1.I) ) MaxFlow_PushDouble( U, edge );
		}
		for(i=0,n=U->ins->size; i<n; i++){
			DaoxEdge *edge = (DaoxEdge*) U->ins->items.pValue[i];
			DaoxNode *V = edge->from;
			if( (0 > edge->W3.D) && (U->U1.I > V->U1.I) ) MaxFlow_PushDouble( U, edge );
		}
		MaxFlow_RelabelDouble( U );
	}
}
double DaoxGraph_MaxFlow_PRTF_Double( DaoxGraph *self, DaoxNode *source, DaoxNode *sink )
{
	size_t i, n;
	double inf = 0;
	DArray *list = DArray_New(0);

	for(i=0,n=source->outs->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->outs->items.pValue[i];
		inf += edge->W1.D;
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
	for(i=0,n=source->outs->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->outs->items.pValue[i];
		MaxFlow_PushDouble( source, edge );
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
	for(i=0,n=source->outs->size; i<n; i++){
		DaoxEdge *edge = (DaoxEdge*) source->outs->items.pValue[i];
		inf += edge->W2.D;
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
/* Interface to Dao        */
/***************************/

static void NODE_GetEdges( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxNode *self = (DaoxNode*) p[0];
	DaoList *res = DaoProcess_PutList( proc );
	size_t i;
	if( p[1]->xEnum.value == 0 ){
		for(i=0; i<self->ins->size; i++) DaoList_PushBack( res, self->ins->items.pValue[i] );
	}else{
		for(i=0; i<self->outs->size; i++) DaoList_PushBack( res, self->outs->items.pValue[i] );
	}
}
static void EDGE_SetWeight( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxEdge *self = (DaoxEdge*) p[0];
	switch( self->graph->wtype ){
	case DAO_INTEGER : self->W1.I = DaoValue_GetInteger( p[1] ); break;
	case DAO_FLOAT   : self->W1.F = DaoValue_GetFloat( p[1] ); break;
	case DAO_DOUBLE  : self->W1.D = DaoValue_GetDouble( p[1] ); break;
	}
}
static void EDGE_GetNodes( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxEdge *self = (DaoxEdge*) p[0];
	DaoTuple *res = DaoProcess_PutTuple( proc );
	DaoTuple_SetItem( res, (DaoValue*)self->from, 0 );
	DaoTuple_SetItem( res, (DaoValue*)self->to, 1 );
}
static void GRAPH_Graph( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoType *retype = DaoProcess_GetReturnType( proc );
	DaoxGraph *graph = DaoxGraph_New( 0, p[0]->xEnum.value );
	GC_ShiftRC( retype, graph->ctype );
	graph->ctype = retype;
	if( retype->nested->size && retype->nested->items.pType[0]->tid <= DAO_DOUBLE )
		graph->wtype = retype->nested->items.pType[0]->tid;
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
	//{ NODE_GetIntegers, "integers() => tuple<U1:int,U2:int,U3:int>" },
	//{ NODE_GetFloats,   "floats() => tuple<U1:float,U2:float,U3:float>" },
	//{ NODE_GetDoubles,  "doubles() => tuple<U1:double,U2:double,U3:double>" },
	{ NODE_GetEdges, "edges( self :Node<@TUW,@TNV,@TEV>, set :enum<in,out> = $out ) => list<Edge<@TUW,@TNV,@TEV>>" },
	{ NULL, NULL }
};

DaoTypeBase DaoxNode_Typer =
{
	"Node"TYPE_PARAMS, NULL, NULL, (DaoFuncItem*) DaoxNodeMeths, {0}, {0},
	(FuncPtrDel)DaoxNode_Delete, DaoxNode_GetGCFields
};

static DaoFuncItem DaoxEdgeMeths[]=
{
	{ EDGE_SetWeight, "set_weight( self :Edge<@TUW,@TNV,@TEV>, w :@TUW )" },
	{ EDGE_GetNodes, "nodes( self :Edge<@TUW,@TNV,@TEV> ) => tuple<from:Node<@TUW,@TNV,@TEV>,to:Node<@TUW,@TNV,@TEV>>" },
	{ NULL, NULL }
};

DaoTypeBase DaoxEdge_Typer =
{
	"Edge"TYPE_PARAMS, NULL, NULL, (DaoFuncItem*) DaoxEdgeMeths, {0}, {0},
	(FuncPtrDel)DaoxEdge_Delete, DaoxEdge_GetGCFields
};


static DaoFuncItem DaoxGraphMeths[]=
{
	/* allocators must have names identical to the typer name: */
	{ GRAPH_Graph,    "Graph"TYPE_PARAMS"( dir :enum<undirected,directed>=$undirected )" },
	{ GRAPH_GetNodes, "nodes( self :Graph<@TUW,@TNV,@TEV> ) => list<Node<@TUW,@TNV,@TEV>>" },
	{ GRAPH_GetEdges, "edges( self :Graph<@TUW,@TNV,@TEV> ) => list<Edge<@TUW,@TNV,@TEV>>" },
	{ GRAPH_AddNode, "add_node( self :Graph<@TUW,@TNV,@TEV> ) => Node<@TUW,@TNV,@TEV>" },
	{ GRAPH_AddEdge, "add_edge( self :Graph<@TUW,@TNV,@TEV>, from :Node<@TUW,@TNV,@TEV>, to :Node<@TUW,@TNV,@TEV> ) => Edge<@TUW,@TNV,@TEV>" },
	{ GRAPH_MaxFlow, "maxflow( self :Graph<@TUW,@TNV,@TEV>, source :Node<@TUW,@TNV,@TEV>, sink :Node<@TUW,@TNV,@TEV> ) => @TUW" },
	{ NULL, NULL }
};

DaoTypeBase DaoxGraph_Typer =
{
	"Graph"TYPE_PARAMS, NULL, NULL, (DaoFuncItem*) DaoxGraphMeths, {0}, {0},
	(FuncPtrDel)DaoxGraph_Delete, DaoxGraph_GetGCFields
};

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace_WrapType( ns, & DaoxNode_Typer, 0 );
	DaoNamespace_WrapType( ns, & DaoxEdge_Typer, 0 );
	DaoNamespace_WrapType( ns, & DaoxGraph_Typer, 0 );
	return 0;
}
