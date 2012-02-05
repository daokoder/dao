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

enum { DAO_OP_NONE, DAO_OP_SINGLE, DAO_OP_PAIR, DAO_OP_TRIPLE, DAO_OP_RANGE, DAO_OP_RANGE2 };

typedef struct DaoCodeNode  DaoCodeNode;
typedef struct DaoOptimizer DaoOptimizer;

struct DaoCodeNode
{
	ushort_t  index;
	ushort_t  type;  /* use type of variable: DAO_OP_NONE/SINGLE/PAIR/RANGE; */
	ushort_t  first; /* the only (for SINGLE) or the first (for PAIR/RANGE) used variable; */
	ushort_t  second; /* the second (for PAIR) or the last (for RANGE) used variable; */
	ushort_t  third;  /* the third (for TRIPLE) used variable; */
	ushort_t  lvalue; /* variable defined by the instruction; 0xffff for none; */
	ushort_t  exprid; /* expression id; 0xffff for none; */

	DArray   *ins;  /* in nodes in the flow graph; */
	DArray   *outs; /* out nodes in the flow graph; */

	DMap     *set; /* set for the analysis; */
};

DaoCodeNode* DaoCodeNode_New();
void DaoCodeNode_Delete( DaoCodeNode *self );

typedef int (*AnalysisUpdater)( DaoOptimizer*, DaoCodeNode*, DaoCodeNode* );

struct DaoOptimizer
{
	DaoRoutine *routine;

	int reverseFlow;

	AnalysisUpdater updater;

	DArray  *nodes; /* all nodes (labels); */
	DArray  *uses;  /* nodes that use a variable; */

	DMap  *exprs;   /* all expressions; */
	DMap  *inits;   /* init nodes; */
	DMap  *finals;  /* final nodes; */
	DMap  *least;   /* least element; */
	DMap  *extreme; /* extremal value; */

	DMap  *tmp;

	DArray *tmp2;

	DArray  *nodeCache;
	DArray  *arrayCache;
};

DaoOptimizer* DaoOptimizer_New();
void DaoOptimizer_Clear( DaoOptimizer *self );
void DaoOptimizer_Delete( DaoOptimizer *self );

void DaoOptimizer_InitNode( DaoOptimizer *self, DaoCodeNode *node, DaoVmCode *code );

#endif
