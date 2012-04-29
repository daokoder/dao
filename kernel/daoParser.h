/*
// This file is part of the virtual machine for the Dao programming language.
// Copyright (C) 2006-2012, Limin Fu. Email: daokoder@gmail.com
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this 
// software and associated documentation files (the "Software"), to deal in the Software 
// without restriction, including without limitation the rights to use, copy, modify, merge, 
// publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons 
// to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or 
// substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef DAO_PARSER_H
#define DAO_PARSER_H

#include"daoType.h"
#include"daoLexer.h"


struct DaoParser
{
	DaoVmSpace   *vmSpace;
	DaoNamespace *nameSpace;

	DString *fileName;

	DaoParser *defParser;
	int parStart;
	int parEnd;

	int curToken;

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
	int vmcCount;

	/* Stack of maps: mapping local variable names to virtual register ids at each level: */
	DArray  *localVarMap; /* < DMap<DString*,int> > */
	DArray  *localCstMap; /* < DMap<DString*,int> > */
	DArray  *localDecMap; /* < DMap<DString*,int> > */
	DArray  *switchMaps;
	DArray  *enumTypes; /* <DaoType*> */

	/* the line number where a register is first used;
	 * with respect to the first line in the routine body;
	 * -1 is used for register for parameters */
	DArray *regLines; /* <int> : obsolete */

	short levelBase;
	short lexLevel;
	short needConst;

	DMap  *allConsts; /* <DString*,int>: implicit and explict constants; */

	DArray *routCompilable; /* list of defined routines with bodies */

	int    regCount;
	int    lastValue;
	DMap  *initTypes; /* type holders @T from parameters and the up routine */

	int noneValue;
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
	char isFunctional;

	DaoInterface *hostInter;
	DaoClass     *hostClass;
	DaoType      *hostCdata;
	DaoType      *hostType;
	DaoParser    *outParser;

	DaoType      *cblockType;
	DaoType      *returnType;

	DaoToken  *argName;

	int curLine;
	int lineCount;
	short indent;
	short defined;
	short error;
	short parsed;
	DArray *scopeOpenings; /* <DaoInode*> */
	DArray *scopeClosings; /* <DaoInode*> */
	DArray *errors;
	DArray *warnings;
	DArray *bindtos;
	DArray *uplocs;
	DArray *outers;
	DArray *decoFuncs;
	DArray *decoParams;
	DArray *tempTypes;

	/* Proto-values for a proto class: upvalue register ids to class member ids */
	DMap  *protoValues; /* <int,int> */

	/* members for convenience */
	DaoEnum   *denum;
	DLong     *bigint;
	DString   *mbs;
	DString   *mbs2;
	DString   *str;
	DMap      *lvm; /* <DString*,int>, for localVarMap; */
	DArray    *toks;

	DArray  *strings;
	DArray  *arrays;
};

DAO_DLL DaoParser* DaoParser_New();
DAO_DLL void DaoParser_Delete( DaoParser *self );

DAO_DLL int DaoParser_LexCode( DaoParser *self, const char *source, int replace );
DAO_DLL int DaoParser_ParsePrototype( DaoParser *self, DaoParser *module, int key, int start );
DAO_DLL int DaoParser_ParseScript( DaoParser *self );
DAO_DLL int DaoParser_ParseRoutine( DaoParser *self );

DAO_DLL DaoType* DaoParser_ParseTypeName( const char *type, DaoNamespace *ns, DaoClass *cls );
DAO_DLL int DaoParser_FindPairToken( DaoParser *self,  uchar_t lw, uchar_t rw, int start, int stop/*=-1*/ );
DAO_DLL DaoType* DaoParser_ParseType( DaoParser *self, int start, int end, int *newpos, DArray *types );

#endif
