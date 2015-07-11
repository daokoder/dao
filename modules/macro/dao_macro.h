/*
// Dao Standard Modules
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

#ifndef DAO_MACRO_H
#define DAO_MACRO_H

#include"daoType.h"

typedef struct DMacroUnit    DMacroUnit;
typedef struct DMacroGroup   DMacroGroup;
typedef struct DaoMacro      DaoMacro;

enum DMacroUnitTypes
{
	DMACRO_TOK , /* normal identifers, and operators */
	DMACRO_VAR , /* $VAR prefixed identifier: create a unique identifier name */
	DMACRO_EXP , /* $EXP prefixed identifier: expression tokens */
	DMACRO_ID  , /* $ID  prefixed identifier: identifier token */
	DMACRO_OP  , /* $OP  prefixed identifier: operator token */
	DMACRO_BL  , /* $BL  prefixed identifier: code block tokens */
	DMACRO_GRP , /* ( ... ) */
	DMACRO_ALT , /* ( ... | ... ) */
	DMACRO_STR , /* $STR identifier: stringify token */
	DMACRO_CAT   /* $CAT identifier: concatenate tokens */
};

enum DaoMacroGroupRepeatTypes
{
	DMACRO_ZERO ,          /* ( ... ) !              */
	DMACRO_ZERO_OR_ONE ,   /* ( ... ) ?  OR  [ ... ] */
	DMACRO_ZERO_OR_MORE ,  /* ( ... ) *  OR  { ... } */
	DMACRO_ONE_OR_MORE ,   /* ( ... ) +              */
	DMACRO_ONE ,           /* ( ... )                */
	DMACRO_AUTO            /* implicit group in '(' ')', '[' ']', '{' '}' */
};

struct DMacroUnit
{
	uchar_t     type;
	uchar_t     dummy1;
	uchar_t     dummy2;
	DList      *stops;
	DaoToken   *marker;
};

/* ( mykey1 $EXP1 mykey2 ) ? * + */
/* ( mykey1 $EXP1 mykey2 | mykey3 $EXP2 mykey3 ) ? * + */
struct DMacroGroup
{
	uchar_t       type;
	uchar_t       repeat;
	uchar_t       cpos;
	DList        *stops;
	DList        *units; /* DList<DMacroUnit*> */
	DList        *variables; /* DList<DaoToken*> */
	DMacroGroup  *parent;
};

struct DaoMacro
{
	DAO_CSTRUCT_COMMON;

	DMacroGroup  *macroMatch;
	DMacroGroup  *macroApply;

	DList        *keyListApply;

	DList        *macroList; /* overloaded macros */
	DaoMacro     *firstMacro;
};

DaoMacro* DaoMacro_New();
void DaoMacro_Delete( DaoMacro *self );

int DaoParser_ParseMacro( DaoParser *self, int start );
int DaoParser_MacroTransform( DaoParser *self, DaoMacro *macro, int start, int tag );

#endif
