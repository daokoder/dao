/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2011, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

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
	"No method to use from",
	"No public field is allowed in asynclass",
	"No static field is allowed in asynclass",
	"Symbol possibly undefined",
	"Symbol not defined",
	"Symbol was defined",
	"Need symbol of constant",
	"Need symbol of class",
	"Need symbol of class or C type",
	"Need symbol of asynchronous class",
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
	"Invalid scope ending",
	"Invalid functional method",
	"Invalid definition",
	"Invalid enum definition",
	"Invalid class definition",
	"Invalid asynchronous class definition",
	"Invalid function definition",
	"Invalid interface definition",
	"Invalid function decoration",
	"Invalid parent class",
	"Invalid use statement",
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
	"Invalid operator for overloading",
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
	"Cyclic loading detected",
	"Numeric array is disabled",
	"Serialization is disabled",
	"Synchronous class is disabled",
	"Template class is disabled",
	"Runtime class creation is disabled",
	"Function decorator is disabled",
	"default parameter is not const",
	"invalid parameter list",
	"parameter number not correct",
	"invalid named parameter",
	"invalid parameter default",
	"my, is used in non-constructor routine",
	"uncompatible variable prefix",
	"invalid for statement",
	"invalid for-in-do statement",
	"invalid case for switch",
	"case with non-const value",
	"case is not directly nested by switch",
	"invalid load statement",
	"invalid path for loading",
	"invalid variable name for importing",
	"invalid module name for loading",
	"loading failed",
	"loading cancelled",
	"variable(s) not found for importing",
	"redundant code in load statement",
	"attempt to init const var from non-const expr",
	"attempt to modify const",
	"invalid expression",
	"invalid enumeration",
	"exceed matrix enumeration limit",
	"invalid multi-assignment",
	"invalid string literal",
	"un-paired quotation symbol",
	"for assignment inside an expression, use :=",
	"different version of dao.h is used",
	"invalid Dao script embedding",
	"invalid syntax",
	"unmatched assignment",
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
	"unmatched grouping level" ,
	"invalid type form",
	"invalid type name",
	"invalid constant expression",
	"not permited in the safe running mode",
	"type not matched",
	"interface binding failed",
	"is not enabled",
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

const char* const daoExceptionName[] =
{
	"Exception" ,
	"Exception.None" ,
	"Exception.Any" ,
	"Exception.Warning" ,
	"Exception.Error" ,
	"Exception.Error.Field" ,
	"Exception.Error.Field.NotExist" ,
	"Exception.Error.Field.NotPermit" ,
	"Exception.Error.Float" ,
	"Exception.Error.Float" ,
	"Exception.Error.Float" ,
	"Exception.Error.Float" ,
	"Exception.Error.Index" ,
	"Exception.Error.Index.OutOfRange" ,
	"Exception.Error.Key" ,
	"Exception.Error.Key.NotExist" ,
	"Exception.Error.Param" ,
	"Exception.Error.Syntax" ,
	"Exception.Error.Type" ,
	"Exception.Error.Value" ,
	"Exception.Warning.Syntax" ,
	"Exception.Warning.Value"
};
const char* const daoExceptionInfo[] =
{
	"undefined exception" ,
	"none exception" ,
	"any or none exception" ,
	"undefined error" ,
	"undefined error" ,
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
	"none", "int", "float", "double", "complex", "long", "string", 
	"enum", "array", "list", "map", "tuple", "stream"
};
const char *const daoBitBoolArithOpers[] = {
	"=", "!", "-", "~", "+", "-", "*", "/", "%", "**", 
	"&&", "||", "<", "<=", "==", "!=", "in", "&", "|", "^", "<<", ">>"
};
const char *const daoBitBoolArithOpers2[] = {
	NULL, NULL, NULL, NULL, "+=", "-=", "*=", "/=", "%=", NULL, 
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, "&=", "|=", "^=", NULL, NULL
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
