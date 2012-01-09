/*=========================================================================================
  This file is a part of the Dao standard modules.
  Copyright (C) 2011-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#ifndef __DAO_GRAPH_H__
#define __DAO_GRAPH_H__

#include"daoStdtype.h"

typedef struct DaoxNode   DaoxNode;
typedef struct DaoxEdge   DaoxEdge;
typedef struct DaoxGraph  DaoxGraph;

extern DaoTypeBase DaoxNode_Typer;
extern DaoTypeBase DaoxEdge_Typer;
extern DaoTypeBase DaoxGraph_Typer;

struct DaoxNode
{
	DAO_CDATA_COMMON;

	DaoxGraph  *graph;
	/* For directed graph, out edges are pushed to front; in edges are pushed to back: */
	DArray     *edges;  /* <DaoxEdge*>; */
	DaoValue   *value;

	union {
		dint   I;
		float  F;
		double D;
	} U1, U2, U3;
};

DAO_DLL DaoxNode* DaoxNode_New( DaoxGraph *graph );
DAO_DLL void DaoxNode_Delete( DaoxNode *self );

struct DaoxEdge
{
	DAO_CDATA_COMMON;

	DaoxGraph  *graph;
	DaoxNode   *first;
	DaoxNode   *second;
	DaoValue   *value;

	union {
		dint   I;
		float  F;
		double D;
	} W1, W2, W3;
};

DAO_DLL DaoxEdge* DaoxEdge_New( DaoxGraph *graph );
DAO_DLL void DaoxEdge_Delete( DaoxEdge *self );

struct DaoxGraph
{
	DAO_CDATA_COMMON;

	DArray  *nodes; /* <DaoxNode*>; */
	DArray  *edges; /* <DaoxEdge*>; */
	short    wtype; /* weight type: DAO_NONE, DAO_INTEGER, DAO_FLOAT, DAO_DOUBLE; */
	short    directed; /* directed graph; */

	DaoType  *nodeType;
	DaoType  *edgeType;
};
DAO_DLL extern DaoType *daox_node_template_type;
DAO_DLL extern DaoType *daox_edge_template_type;
DAO_DLL extern DaoType *daox_graph_template_type;

DAO_DLL DaoxGraph* DaoxGraph_New( DaoType *type, int directed );
DAO_DLL void DaoxGraph_Delete( DaoxGraph *self );

DAO_DLL DaoxNode* DaoxGraph_AddNode( DaoxGraph *self );
DAO_DLL DaoxEdge* DaoxGraph_AddEdge( DaoxGraph *self, DaoxNode *first, DaoxNode *second );

DAO_DLL double DaoxGraph_MaxFlow_PushRelabelToFront( DaoxGraph *self, DaoxNode *source, DaoxNode *sink );

#endif
