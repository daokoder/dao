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
