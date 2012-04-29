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

#ifndef DAO_MACRO_H
#define DAO_MACRO_H

#include"daoType.h"

typedef struct DMacroUnit    DMacroUnit;
typedef struct DMacroGroup   DMacroGroup;

enum DMacroUnitTypes
{
	DMACRO_TOK , /* normal identifers, and operators */
	DMACRO_VAR , /* $VAR prefixed identifier: create a unique identifier name */
	DMACRO_EXP , /* $EXP prefixed identifier: expression tokens */
	DMACRO_ID  , /* $ID  prefixed identifier: identifier token */
	DMACRO_OP  , /* $OP  prefixed identifier: operator token */
	DMACRO_BL  , /* $BL  prefixed identifier: code block tokens */
	DMACRO_IBL , /* $IBL prefixed identifier: indented code block tokens */
	DMACRO_BR  , /* '<' '>' '[' ']' '{' '}' */
	DMACRO_GRP , /* \( ... \) */
	DMACRO_ALT   /* \( ... \| ... \) */
};

enum DaoMacroGroupRepeatTypes
{
	DMACRO_ZERO ,          /* \( ... \) \!  */
	DMACRO_ZERO_OR_ONE ,   /* \( ... \) \? OR \[ ... \] */
	DMACRO_ZERO_OR_MORE ,  /* \( ... \) \* OR \{ ... \} */
	DMACRO_ONE_OR_MORE ,   /* \( ... \) \+  */
	DMACRO_ONE ,           /* \( ... \)     */
	DMACRO_AUTO            /* implicit group in (), [], {} */
};

struct DMacroUnit
{
	uchar_t     type;
	uchar_t     dummy1;
	uchar_t     dummy2;
	uchar_t     indent;
	DArray     *stops;
	DaoToken   *marker;
};

/* \( mykey1 $EXP1 mykey2 \) \? \* \+ */
/* \( mykey1 $EXP1 mykey2 \| mykey3 $EXP2 mykey3 \) \? \* \+ */
struct DMacroGroup
{
	uchar_t       type;
	uchar_t       repeat;
	uchar_t       cpos;
	uchar_t       indent;
	DArray       *stops;
	DArray       *units; /* DArray<DMacroUnit*> */
	DArray       *variables; /* DArray<DaoToken*> */
	DMacroGroup  *parent;
};

struct DaoMacro
{
	DAO_DATA_COMMON;

	DMacroGroup  *macroMatch;
	DMacroGroup  *macroApply;

	DArray       *keyListApply;

	DArray       *macroList; /* overloaded macros */
	DaoMacro     *firstMacro;
};

DaoMacro* DaoMacro_New();
void DaoMacro_Delete( DaoMacro *self );

int DaoParser_ParseMacro( DaoParser *self, int start );
int DaoParser_MacroTransform( DaoParser *self, DaoMacro *macro, int start, int tag );

#endif
