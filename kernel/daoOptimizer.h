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

#ifndef __DAO_OPTIMIZER_H__
#define __DAO_OPTIMIZER_H__

#include"daoBase.h"

enum { DAO_OP_NONE, DAO_OP_SINGLE, DAO_OP_PAIR, DAO_OP_TRIPLE, DAO_OP_RANGE };

typedef struct DaoCodeNode  DaoCodeNode;
typedef struct DaoFlowGraph DaoFlowGraph;

struct DaoCodeNode
{
	ushort_t  index;
	ushort_t  type;  /* use type of variable: DAO_OP_NONE/SINGLE/PAIR/RANGE; */
	ushort_t  first; /* the only (for SINGLE) or the first (for PAIR/RANGE) used variable; */
	ushort_t  second; /* the second (for PAIR) or the last (for RANGE) used variable; */
	ushort_t  third;  /* the third (for TRIPLE) used variable; */
	ushort_t  lvalue; /* variable defined by the instruction; 0xffff for none; */

	DArray   *ins;  /* in nodes in the flow graph; */
	DArray   *outs; /* out nodes in the flow graph; */

	DMap     *set; /* set for the analysis; */
};

DaoCodeNode* DaoCodeNode_New();
void DaoCodeNode_Delete( DaoCodeNode *self );


struct DaoFlowGraph
{
	DArray  *nodes;  /* all nodes (labels); */

	DMap  *inits;   /* init nodes; */
	DMap  *finals;  /* final nodes; */
	DMap  *least;   /* least element; */
	DMap  *extreme; /* extremal value; */
};

DaoFlowGraph* DaoFlowGraph_New();
void DaoFlowGraph_Clear( DaoFlowGraph *self );
void DaoFlowGraph_Delete( DaoFlowGraph *self );


void DaoRoutine_GetCodeOperands( DaoRoutine *self, DaoVmCode *code, DaoCodeNode *node );
void DaoRoutine_BuildFlowGraph( DaoRoutine *self, DaoFlowGraph *graph );

#endif
