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

DaoxNode* DaoxNode_New( DaoxGraph *graph )
{
	DaoxNode *self = (DaoxNode*) dao_malloc( sizeof(DaoxNode) );
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
	DaoxEdge *self = (DaoxEdge*) dao_malloc( sizeof(DaoxEdge) );
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
	DaoxGraph *self = (DaoxGraph*) dao_malloc( sizeof(DaoxGraph) );
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
	DArray_Append( self->nodes, node );
	return node;
}
DaoxEdge* DaoxGraph_AddEdge( DaoxGraph *self, DaoxNode *from, DaoxNode *to )
{
	DaoxEdge *edge = DaoxEdge_New( self );
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
static void EDGE_GetNodes( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxEdge *self = (DaoxEdge*) p[0];
	DaoTuple *res = DaoProcess_PutTuple( proc );
	DaoTuple_SetItem( res, (DaoValue*)self->from, 0 );
	DaoTuple_SetItem( res, (DaoValue*)self->to, 1 );
}
static void GRAPH_Graph( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutValue( proc, (DaoValue*) DaoxGraph_New( p[0]->xEnum.value, p[1]->xEnum.value ) );
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


static DaoFuncItem DaoxNodeMeths[]=
{
	{ NODE_GetEdges, "edges( self :Node, set :enum<in,out> = $out ) => list<Edge>" },
	{ NULL, NULL }
};

DaoTypeBase DaoxNode_Typer =
{
	"Node", NULL, NULL, (DaoFuncItem*) DaoxNodeMeths, {0}, {0},
	(FuncPtrDel)DaoxNode_Delete, DaoxNode_GetGCFields
};

static DaoFuncItem DaoxEdgeMeths[]=
{
	{ EDGE_GetNodes, "nodes( self :Edge ) => tuple<from:Node,to:Node>" },
	{ NULL, NULL }
};

DaoTypeBase DaoxEdge_Typer =
{
	"Edge", NULL, NULL, (DaoFuncItem*) DaoxEdgeMeths, {0}, {0},
	(FuncPtrDel)DaoxEdge_Delete, DaoxEdge_GetGCFields
};

static DaoFuncItem DaoxGraphMeths[]=
{
	{ GRAPH_Graph, "Graph( wtype :enum<none,int,float,double>=$none, dir :enum<undirected,directed>=$undirected ) => Graph" },
	{ GRAPH_GetNodes, "nodes( self :Graph ) => list<Node>" },
	{ GRAPH_GetEdges, "edges( self :Graph ) => list<Edge>" },
	{ GRAPH_AddNode, "add_node( self :Graph ) => Node" },
	{ GRAPH_AddEdge, "add_edge( self :Graph, from :Node, to :Node ) => Edge" },
	{ NULL, NULL }
};

DaoTypeBase DaoxGraph_Typer =
{
	"Graph", NULL, NULL, (DaoFuncItem*) DaoxGraphMeths, {0}, {0},
	(FuncPtrDel)DaoxGraph_Delete, DaoxGraph_GetGCFields
};

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace_WrapType( ns, & DaoxNode_Typer, 0 );
	DaoNamespace_WrapType( ns, & DaoxEdge_Typer, 0 );
	DaoNamespace_WrapType( ns, & DaoxGraph_Typer, 0 );
	return 0;
}
