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

#include"daoConst.h"

static const char* const daoCtInfo[] =
{
	"",
	"internal error",
	"exceed parameter number limit",
	"Undefined symbol",
	"Unpaired tokens",
	"is expected",
	"is not expected",
	"is not declared",
	"has been previously defined",
	"Statements not seperated properly",
	"Assignment inside expression",
	"Getter/setter may not be used for",
	"No method to use from",
	"Symbol possibly undefined",
	"Symbol not defined",
	"Symbol was defined",
	"Need symbol of constant",
	"Need symbol of class",
	"Need symbol of class or C type",
	"Need symbol of interface",
	"Need bindable type (Dao class or C type)",
	"Need string token",
	"Need valid name" ,
	"Expecting token" ,
	"Token not found",
	"Token not expected",
	"Tokens not paired",
	"Tokens not expected",
	"Need constant expression for number",
	"Need constant expression",
	"Constant cannot be modified",
	"Interface not compatible",
	"Missing interface method",
	"Failed interface binding",
	"Failed type instantiation",
	"Undefined scope name",
	"Invalid token or unclosed string/comment/verbatim",
	"Invalid path setting",
	"Invalid access permission",
	"Invalid storage type/combination",
	"Invalid access of shared data",
	"Invalid constant literal",
	"Invalid number radix",
	"Invalid number digit",
	"Invalid type name",
	"Invalid type form",
	"Invalid reference to non-local variable",
	"Invalid expression",
	"Invalid statement",
	"Unclosed scope",
	"Invalid scope ending",
	"Invalid target pattern for decorator",
	"Invalid functional method",
	"Invalid definition",
	"Invalid enum definition",
	"Invalid class definition",
	"Invalid function definition",
	"Invalid interface definition",
	"Invalid function decoration",
	"Invalid parent class",
	"Invalid mixin class",
	"Invalid use statement",
	"Invalid type aliasing",
	"Invalid interface binding",
	"Invalid type define",
	"Invalid for loop",
	"Invalid for-in loop",
	"Invalid parameter list for",
	"Invalid parameter",
	"Parameters not separated",
	"Variable-length parameter list not final",
	"Explicit type not expected",
	"Parameter need explicit type",
	"Parameter need default value",
	"Default parameter is not constant",
	"Invalid default parameter",
	"Default value not matching parameter type",
	"Too many parameters",
	"Invalid return type",
	"Too deeply nested code section" ,
	"Statement in class body",
	"Statement in interface body",
	"Statement used out of context",
	"Variable declared out of context",
	"Type not matching",
	"Type of given value",
	"Type of expected value",
	"Type without default value",
	"Function need return type",
	"Invalid decorator definition",
	"Invalid operator for overloading",
	"Invalid first parameter name for the decorator",
	"Constructor cannot return",
	"Method need implementation",
	"Method implementation is redundant",
	"Method not declared",
	"Method was implemented",
	"Method signature was defined",
	"Method signature was declared as",
	"Method signature was used from parent",
	"Method signature not matching",
	"Constructor not defined",
	"Invalid case statement",
	"Case value not constant",
	"Case values not distinctive",
	"Duplicated default case",
	"Constant evaluation aborted with exception(s)",
	"Cyclic loading detected",
	"String pattern matching is disabled",
	"Numeric array is disabled",
	"Function decorator is disabled",
	"invalid load statement",
	"invalid path for loading",
	"invalid variable name for importing",
	"invalid module name for loading",
	"loading failed",
	"loading cancelled",
	"invalid enumeration",
	"exceed matrix enumeration limit",
	"invalid syntax",
	"re-definition of variable",
	"invalid syntax definition",
	"invalid first token in macro defintion",
	"invalid macro group opening",
	"unknown macro variable type",
	"unknown macro repeat type",
	"unknown special token in the macro",
	"invalid indentation marker in the macro",
	"re-definition of special marker",
	"undefine macro marker",
	"invalid constant expression",
	"obsolete syntax",
	""
};

const char* getCtInfo( int tp )
{
	if( tp < 0 || tp >= DAO_CTW_END ) return "";
	return daoCtInfo[ tp ];
}
const char* getRtInfo( int tp )
{
	return "";
}

const char* const daoExceptionInfo[] =
{
	"certain exception" ,
	"none exception" ,
	"any or none exception" ,
	"certain warning" ,
	"certain error" ,
	"invalid field accessing" ,
	"field not exist" ,
	"field not permit" ,
	"invalid floating point operation" ,
	"division by zero" ,
	"floating point overflow" ,
	"floating point underflow" ,
	"invalid index" ,
	"index out of range" ,
	"invalid key" ,
	"key not exist" ,
	"invalid parameter list for the call" ,
	"invalid syntax" ,
	"invalid variable type for the operation" ,
	"invalid variable value for the operation" ,
	"file error",
	"invalid syntax" ,
	"invalid value for the operation"
};

const char* const coreTypeNames[] =
{
	"none", "int", "float", "double", "complex", "string",
	"enum", "array", "list", "map", "tuple"
};
const char *const daoBitBoolArithOpers[] = {
	"!", "-", "~", "&", "+", "-", "*", "/", "%", "**", "&&", "||",
	"<", "<=", "==", "!=", "in", "&", "|", "^", "<<", ">>"
};
const char *const daoBitBoolArithOpers2[] = {
	NULL, NULL, NULL, NULL, "+=", "-=", "*=", "/=", "%=", NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, "&=", "|=", "^=", NULL, NULL
};

const char *daoRoutineCodeHeader =
"   ID :    OPCODE    :     A ,     B ,     C ;  [ LINE ],  NOTES\n";
const char *daoRoutineCodeFormat = "%-11s : %5i , %5i , %5i ;  %4i;   %s\n";

const char utf8_markers[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 00 - 0F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 10 - 1F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 20 - 2F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 30 - 3F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 40 - 4F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 50 - 5F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 60 - 6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 70 - 7F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 80 - 8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 90 - 9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* A0 - AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* B0 - BF */
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* C0 - CF */
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* D0 - DF */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* E0 - EF */
	4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 7  /* F0 - FF */
};
