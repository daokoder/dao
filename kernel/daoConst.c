/*
// Dao Virtual Machine
// http://daoscript.org
//
// Copyright (c) 2006-2017, Limin Fu
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
	"is expected",
	"has been previously defined",
	"Statements not seperated properly",
	"Assignment inside expression",
	"Getter/setter may not be used for",
	"Value was used",
	"Symbol possibly undefined",
	"Symbol not defined (auto declaration is disabled, please add \"var/invar\" before the declaration, or use --autovar to turn auto declaration on)",
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
	"R-value cannot be modified",
	"Interface not compatible",
	"Missing interface method",
	"Failed interface binding",
	"Failed type instantiation",
	"Undefined scope name",
	"Field not exists",
	"Field access not permitted",
	"Invalid field accessing",
	"Invalid token or unclosed string/comment/verbatim",
	"Invalid path setting",
	"Invalid access permission",
	"Invalid storage type/combination",
	"Invalid access of shared data",
	"Invalid constant literal",
	"Invalid index",
	"Invalid type name",
	"Invalid type form",
	"Invalid reference to non-local variable",
	"Invalid expression",
	"Invalid statement",
	"Invalid self access in static method",
	"Unclosed scope",
	"Invalid scope ending",
	"Invalid functional method",
	"Invalid declaration",
	"Invalid definition",
	"Invalid enum definition",
	"Invalid class definition",
	"Invalid function definition",
	"Invalid namespace definition",
	"Invalid interface definition",
	"Invalid interface target type",
	"Invalid parent interface",
	"Invalid parent class",
	"Invalid mixin class",
	"Invalid import statement",
	"Invalid type aliasing",
	"Invalid interface binding",
	"Invalid type define",
	"Invalid for loop",
	"Invalid for-in loop",
	"Invalid parameter list for",
	"Invalid parameter",
	"Expecting primitive or immutable parameter types",
	"Parameters not separated",
	"Variable-length parameter list not final",
	"Explicit type not expected",
	"Parameter need explicit type",
	"Parameter need default value",
	"Default parameter is not constant",
	"Invalid default parameter",
	"Invalid default parameter before non-default parameter(s)",
	"Default value not matching parameter type",
	"Too many parameters",
	"Invalid return type",
	"Incomplete concrete interface implementation",
	"Too many parent types",
	"Too deeply nested code section" ,
	"Statement in class body",
	"Statement in interface body",
	"Statement used out of context",
	"Variable declared out of context",
	"Variable declared without initialization",
	"Type not matching",
	"Type of given value",
	"Type of expected value",
	"Type without default value",
	"Function need return type",
	"Invalid operator for overloading",
	"Invalid return for the constructor or defer block",
	"Method need implementation",
	"Method implementation is redundant",
	"Method implementation is not properly placed",
	"Method not declared",
	"Method was implemented",
	"Method signature was defined",
	"Method signature was declared as",
	"Method signature was used from parent",
	"Method signature not matching",
	"Constructor not defined",
	"Invalid case statement",
	"Case type not valid",
	"Case value not constant",
	"Case values not distinctive",
	"Duplicated default case",
	"Constant evaluation aborted with exception(s)",
	"Cyclic loading detected",
	"String pattern matching is disabled",
	"Numeric array is disabled",
	"Invalid load statement",
	"Invalid module name for loading",
	"Loading failed",
	"Loading cancelled",
	"Invalid enumeration",
	"Exceed matrix enumeration limit",
	"Invalid syntax",
	"Invalid constant expression",
	"Obsolete syntax",
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

const char* const daoExceptionNames[] =
{
	"OK" ,
	"Error" ,
	"Error::Field" ,
	"Error::Field::Absent" ,
	"Error::Field::Hidden" ,
	"Error::Index" ,
	"Error::Index::Range" ,
	"Error::Key" ,
	"Error::Key::Absent" ,
	"Error::Argument" ,
	"Error::Type" ,
	"Error::Value" ,
	"Error::Float" 
};
const char* const daoExceptionTitles[] =
{
	"No Error" ,
	"General Error" ,
	"Invalid Field" ,
	"Field Absent" ,
	"Field Hidden" ,
	"Invalid Index" ,
	"Index Out of Range" ,
	"Invalid Key" ,
	"Key Absent" ,
	"Invalid Argument" ,
	"Invalid Type" ,
	"Invalid Value" ,
	"Floating Point Error" 
};

const char* const coreTypeNames[] =
{
	"none", "bool", "int", "float", "complex", "string",
	"enum", "array", "list", "map", "tuple"
};

const char *daoRoutineCodeHeader =
"   ID :    OPCODE    :     A ,     B ,     C ;  [ LINE ],  NOTES\n";
const char *daoRoutineCodeFormat = "%-11s : %5i , %5i , %5i ;  %4i;   %s\n";

