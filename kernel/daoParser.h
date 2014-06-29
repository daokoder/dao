/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2014, Limin Fu
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

#ifndef DAO_PARSER_H
#define DAO_PARSER_H

#include"daoType.h"
#include"daoLexer.h"
#include"daoBytecode.h"

enum DaoExpressionListTypes
{
	DAO_EXPRLIST_ARRAY = 1,
	DAO_EXPRLIST_TUPLE = 2,
	DAO_EXPRLIST_PARAM = 4,
	DAO_EXPRLIST_SLICE = 8,
	DAO_EXPRLIST_SCOPE = 16, /* just for convenience; */
};


struct DaoParser
{
	DaoVmSpace   *vmSpace;
	DaoNamespace *nameSpace;

	DString *fileName;

	DaoParser *defParser;

	int  curToken;
	int  curLine;
	int  lineCount;

	DaoLexer  *lexer;
	DArray    *tokens; /* lexer->tokens; */

	/*
	// DArray<DaoVmCodeX*>: need to be store as pointers, because in code generation,
	// it may be necessary to modify previously generated codes, for this,
	// it is much easier to use pointers.
	*/
	DArray  *vmCodes;

	DaoInode *vmcBase;   /* the node before the ::vmcFirst; */
	DaoInode *vmcFirst;  /* the first instruction node; */
	DaoInode *vmcLast;   /* the last instruction node; */
	DaoInode *vmcFree;   /* the first node in the free list; */
	DaoInode *vmcValue;  /* the last instruction node; */

	int  vmcCount;
	int  regCount;

	DArray  *scopeOpenings; /* <DaoInode*> */
	DArray  *scopeClosings; /* <DaoInode*> */
	DArray  *lookupTables; /* DArray<DMap<DString*,int>*>: lookup table for each level; */
	DArray  *switchTables; /* DArray<DMap<DaoValue*,DaoInode*>> */
	DArray  *switchNames;  /* DArray<DString*>: (var name, invar name) */
	DArray  *enumTypes;    /* DArray<DaoType*> */

	DMap  *allConsts;  /* DMap<DString*,int>: implicit and explict local constants; */
	DMap  *initTypes;  /* type holders @T from parameters and the up routine */

	short  levelBase;
	short  lexLevel;
	short  needConst;
	short  evalMode;
	short  numSections;

	int  noneValue;
	int  integerZero;
	int  integerOne;
	int  imaginaryOne;

	DaoRoutine *routine;

	/* if 1, variables not nested in any scope are declared as global */
	char  autoReturn;
	char  isClassBody;
	char  isInterBody;
	char  permission;
	char  isFunctional;
	char  usingGlobal;
	char  invarArg;
	char  invarDecoArg;

	DaoType       *hostType;
	DaoCtype      *hostCtype;
	DaoClass      *hostClass;
	DaoInterface  *hostInter;
	DaoParser     *outerParser;
	DaoParser     *innerParser;

	DaoByteCoder  *byteCoder;
	DaoByteBlock  *byteBlock;

	DaoType      *returnType;
	DaoToken     *argName;
	DaoToken     *decoArgName;

	DArray *uplocs;
	DArray *outers;
	DArray *decoFuncs;
	DArray *decoFuncs2;
	DArray *decoParams;
	DArray *decoParams2;
	DArray *routCompilable; /* list of defined routines with bodies */
	DArray *routReInferable;

	DaoLexer  *elexer;
	DaoLexer  *wlexer;
	DArray    *errors;
	DArray    *warnings;

	/* members for convenience */
	DaoEnum   *denum;
	DString   *string;
	DString   *string2;
	DString   *str;
	DMap      *table; /* <DString*,int>, for lookupTables; */
	DArray    *toks;

	DArray  *typeItems;
	DArray  *strings;
	DArray  *arrays;
	uint_t   usedString;
	uint_t   usedArray;
};

DAO_DLL DaoParser* DaoParser_New();
DAO_DLL void DaoParser_Delete( DaoParser *self );
DAO_DLL void DaoParser_Reset( DaoParser *self );

DAO_DLL int DaoParser_LexCode( DaoParser *self, const char *source, int replace );
DAO_DLL int DaoParser_ParseSignature( DaoParser *self, DaoParser *module, int key, int start );
DAO_DLL int DaoParser_ParseScript( DaoParser *self );
DAO_DLL int DaoParser_ParseRoutine( DaoParser *self );

DAO_DLL DaoType* DaoParser_ParseTypeName( const char *type, DaoNamespace *ns, DaoClass *cls );
DAO_DLL DaoType* DaoParser_ParseType( DaoParser *self, int start, int end, int *newpos, DArray *types );

#endif
