/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2010, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#ifndef DAO_PARSER_H
#define DAO_PARSER_H

#include"daoType.h"
#include"daoLexer.h"

struct DaoInode
{
	unsigned short  code; /* opcode */
	unsigned short  a, b, c; /* register ids for operands */
	unsigned short  level; /* lexical level */
	unsigned short  line; /* line number in source file */
	unsigned int    first;
	unsigned short  middle;
	unsigned short  last;

	int id;
	int extra;

	DaoInode *jumpTrue;
	DaoInode *jumpFalse;

	DaoInode *prev;
	DaoInode *next;
	DaoInode *below;
};
DaoInode* DaoInode_New();

struct DaoParser
{
	DaoVmSpace   *vmSpace;
	DaoNameSpace *nameSpace;

	DString *fileName;

	DaoParser *defParser;
	int parStart;
	int parEnd;

	DArray  *tokens;
	DArray  *partoks;
	DMap    *comments; /* <int,DString*> */

	/* DArray<DaoVmCodeX*>: need to be store as pointers, because in code generation,
	 * it may be necessary to modify previously generated codes, for this,
	 * it is much easier to use pointers. */
	DArray  *vmCodes;

	DaoInode *vmcBase;
	DaoInode *vmcFirst;
	DaoInode *vmcLast;
	DaoInode *vmcTop;
	int vmcCount;

	/* Stack of maps: mapping local variable names to virtual register ids at each level: */
	DArray  *localVarMap; /* < DMap<DString*,int> > */
	DArray  *localCstMap; /* < DMap<DString*,int> > */
	DArray  *switchMaps;
	DArray  *enumTypes; /* <DaoType*> */

	/* the line number where a register is first used;
	 * with respect to the first line in the routine body;
	 * -1 is used for register for parameters */
	DArray *regLines; /* <int> : obsolete */
	DArray *regRefers;

	short levelBase;
	short lexLevel;

	DMap  *allConsts; /* <DString*,int>: implicit and explict constants; */

	DArray *routCompilable; /* list of defined routines with bodies */

	int    locRegCount;
	DMap  *varFunctional; /* <DString*,int>: variables in functional blocks. */
	DMap  *initTypes; /* type holders @T from parameters and the up routine */

	int nullValue;
	int integerZero;
	int integerOne;
	int imaginaryOne;

	int cmpOptions;
	int exeOptions;

	DaoRoutine *routine;
	DString    *routName;

	/* if 1, variables not nested in any scope are declared as global */
	char topAsGlobal;
	char isClassBody;
	char isInterBody;
	char isDynamicClass;
	char permission;
	char warnAssn;
	char pairLtGt; /* <> */

	DaoInterface *hostInter;
	DaoClass     *hostClass;
	DaoType      *hostCData;
	DaoType      *hostType;
	DaoParser    *outParser;

	int curLine;
	int lineCount;
	short indent;
	short defined;
	short error;
	short parsed;
	DArray *scoping; /* <size_t> */
	DArray *errors;
	DArray *bindtos;
	DArray *uplocs;
	DArray *decoFuncs;
	DArray *decoParams;

	/* members for convenience */
	DLong     *bigint;
	DEnum     *denum;
	DString   *mbs;
	DString   *mbs2;
	DString   *str;
	DMap      *lvm; /* <DString*,int>, for localVarMap; */
	DArray    *toks;
	complex16  combuf;
	complex16  imgone;
};

DaoParser* DaoParser_New();
void DaoParser_Delete( DaoParser *self );

int DaoParser_LexCode( DaoParser *self, const char *source, int replace );
int DaoParser_ParsePrototype( DaoParser *self, DaoParser *module, int key, int start );
int DaoParser_ParseParams( DaoParser *self );
int DaoParser_ParseScript( DaoParser *self );
int DaoParser_ParseRoutine( DaoParser *self );

DaoType* DaoParser_ParseTypeName( const char *type, DaoNameSpace *ns, DaoClass *cls, DaoRoutine *rout );

#endif
