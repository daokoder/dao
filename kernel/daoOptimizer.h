/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2012, Limin Fu
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
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __DAO_OPTIMIZER_H__
#define __DAO_OPTIMIZER_H__

#include"daoBase.h"

#define GET_BIT( bits, id ) (bits[id/8] & (1<<(id%8)))
#define SET_BIT0( bits, id ) bits[id/8] &= ~(1<<(id%8))
#define SET_BIT1( bits, id ) bits[id/8] |= (1<<(id%8))

enum DaoCnodeOperandType
{
	DAO_OP_NONE,
	DAO_OP_SINGLE,
	DAO_OP_PAIR,
	DAO_OP_TRIPLE,
	DAO_OP_RANGE,
	DAO_OP_RANGE2
};


/* Code Node */
struct DaoCnode
{
	uchar_t   type;       /* use type of operands; */
	uchar_t   reachable;  /* reachable status; */
	ushort_t  index;      /* index of the node; */
	ushort_t  first;      /* the only (for SINGLE) or the first (for PAIR/RANGE) used variable; */
	ushort_t  second;     /* the second (for PAIR) or the last (for RANGE) used variable; */
	ushort_t  third;      /* the third (for TRIPLE) used variable; */
	ushort_t  lvalue;     /* variable defined by the instruction; 0xffff for none; */
	ushort_t  lvalue2;    /* C operand for SETF, SETI, SETDI, SETMI instructions; */
	ushort_t  exprid;     /* expression id; 0xffff for none; */
	ushort_t  ones;       /* number of ones in the bit array; */

	DArray   *ins;   /* in nodes in the flow graph; */
	DArray   *outs;  /* out nodes in the flow graph; */
	DArray   *kills; /* expressions that are killed by this one; */

	DArray   *defs; /* definitions for this use node; */
	DArray   *uses; /* uses for this definition node; */

	DString  *bits; /* bit array for the analysis; */
};

DAO_DLL void DaoCnode_InitOperands( DaoCnode *self, DaoVmCode *code );

typedef void (*AnalysisInit)( DaoOptimizer*, DaoCnode* );
typedef int (*AnalysisUpdate)( DaoOptimizer*, DaoCnode*, DaoCnode* );

struct DaoOptimizer
{
	DaoRoutine *routine;

	int bitCount;
	int byteCount;
	int reverseFlow;

	AnalysisInit    init;
	AnalysisUpdate  update;

	DArray  *nodes; /* all nodes (labels); */
	DArray  *enodes; /* expression nodes (labels); */
	DArray  *uses;  /* nodes that use a variable; */

	DMap    *exprs;   /* all expressions; */
	DMap    *inits;   /* init nodes; */
	DMap    *finals;  /* final nodes; */

	DMap    *tmp;
	DArray  *tmp2;
	DString *tmp3;

	DArray  *array;
	DArray  *nodeCache;
	DArray  *arrayCache;
};

DAO_DLL DaoOptimizer* DaoOptimizer_New();
DAO_DLL void DaoOptimizer_Clear( DaoOptimizer *self );
DAO_DLL void DaoOptimizer_Delete( DaoOptimizer *self );

DAO_DLL void DaoOptimizer_DoLVA( DaoOptimizer *self, DaoRoutine *routine );
DAO_DLL void DaoOptimizer_DoRDA( DaoOptimizer *self, DaoRoutine *routine );

/*
// Link Definition-Use and Use-Definition:
// The results are stored in each node:
// node->ins:  node is the use, node->ins are the defintions;
// node->outs: node is the defintion, node->outs are the uses;
*/
DAO_DLL void DaoOptimizer_LinkDU( DaoOptimizer *self, DaoRoutine *routine );



/* Instruction Node */
struct DaoInode
{
	unsigned short  code;    /* opcode */
	unsigned short  a, b, c; /* register ids for operands */
	unsigned short  level;   /* lexical level */
	unsigned short  line;    /* line number in source file */
	unsigned int    first;   /* index of the first token of the expression */
	unsigned short  middle;  /* the middle token, relative to "first" */
	unsigned short  last;    /* the last token, relative to "first" */

	unsigned short  index;   /* index of the instruction */

	DaoInode *jumpTrue;
	DaoInode *jumpFalse;
	DaoInode *extra;

	DaoInode *prev;
	DaoInode *next;
};

DaoInode* DaoInode_New();

typedef struct DaoInferencer DaoInferencer;

struct DaoInferencer
{
	unsigned char  tidHost;
	unsigned char  silent;
	unsigned char  error;

	unsigned short  currentIndex;

	DaoRoutine  *routine;
	DaoClass    *hostClass;

	DArray      *inodes;
	DArray      *consts;
	DArray      *types;
	DString     *inited;

	DArray      *rettypes;
	DArray      *typeMaps;
	DArray      *errors;
	DArray      *array;
	DArray      *array2;

	DMap        *defs;
	DMap        *defs2;
	DMap        *defs3;
	DString     *mbstring;

	DaoType     *type_source;
	DaoType     *type_target;
	int          tid_target;
	int          annot_first;
	int          annot_last;

	DaoType     *typeLong;
	DaoType     *typeEnum;
	DaoType     *typeString;
	DaoType     *basicTypes[DAO_ARRAY];
};

DaoInferencer* DaoInferencer_New();

#endif
