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

#ifndef DAO_CONST_H
#define DAO_CONST_H

#define DAO_MAX_PARAM  30
#define DAO_MAX_INDEX  10

#define DAO_MAX_SECTDEPTH  8


#include"dao.h"
#include"daoBase.h"

enum DaoExtraTypes
{
	DAO_VARIANT = END_CORE_TYPES, /* variant or disjoint union type */
	DAO_MACRO ,
	DAO_CONSTANT ,
	DAO_VARIABLE ,
	DAO_ROUTBODY ,
	DAO_TYPEKERNEL ,
	DAO_CODEBLOCK ,

	DAO_PAR_NAMED ,   /* name:type */
	DAO_PAR_DEFAULT , /* name=type */
	DAO_PAR_VALIST , /* ... */

	DAO_VALTYPE , /* value type type */

	/* bit (1<<6) to indicate inexact checking: */
	DAO_ANY = (1<<6)|0, /* any type */
	DAO_THT = (1<<6)|1, /* type holder type */
	DAO_UDT = (1<<6)|2, /* undefined type */

	END_EXTRA_TYPES 
};

enum DaoSubTypes
{
	DAO_PAIR = END_EXTRA_TYPES,
	END_SUB_TYPES
};

enum DaoBasicStruct
{
	D_NULL ,
	D_VALUE ,   /* garbage collectable items, managed by the container; */
	D_VALUE2 ,  /* the same as D_VALUE,  except more strict comparing as map/hash keys; */
	D_VALUE3 ,  /* the same as D_VALUE2, except more strict comparing as map/hash keys; */
	D_VMCODE ,
	D_VMCODE2 , /* for DMap, compare code and operands only; */
	D_TOKEN ,   /* for DArray only; */
	D_STRING ,
	D_VECTOR ,
	D_ARRAY ,
	D_MAP ,
	D_VOID2 /* a pair of pointer; */
};

/* It is for the typing system, to decide when to specialize a routine.
 * when any or ? match to @X in parameter list, no routine specialization.
 *   ls = {};
 *   ls.append( "" );
 *   ls2 = { 1, 3, 4 };
 *   ls2.append( "" );
 */
enum DaoMatchType
{
	DAO_MT_NOT ,
	DAO_MT_NEGLECT , /* for less strict checking when a parameter is any or udf */
	DAO_MT_ANYUDF ,
	DAO_MT_INIT ,
	DAO_MT_UDF ,
	DAO_MT_EMPTY ,
	DAO_MT_ANYX , /* match any to X */
	DAO_MT_ANY , /* match to type "any" */
	DAO_MT_SUB ,
	DAO_MT_SIM , /* int, float, double */
	DAO_MT_EQ ,
	DAO_MT_EXACT /* value to value type */
};

enum DaoVarDeclaration
{
	DAO_DECL_LOCAL      = (1<<0), /* for compiling only */
	DAO_DECL_MEMBER     = (1<<1), /* for compiling only */
	DAO_DECL_GLOBAL     = (1<<2), /* for compiling only */
	DAO_DECL_STATIC     = (1<<3), /* for compiling only */
	DAO_DECL_VAR        = (1<<4), /* for compiling only */
	DAO_DECL_CONST      = (1<<7)  /* using the highest bit in the trait field */
};
enum DaoVarStorage
{
	DAO_LOCAL_VARIABLE  = 0,
	DAO_LOCAL_CONSTANT  = 1, /* lowest bit set to 1 for constant */
	DAO_STATIC_VARIABLE = 2,
	DAO_OBJECT_VARIABLE = 4,
	DAO_CLASS_VARIABLE  = 6,
	DAO_CLASS_CONSTANT  = 7, /* lowest bit set to 1 for constant */
	DAO_GLOBAL_VARIABLE = 8,
	DAO_GLOBAL_CONSTANT = 9  /* lowest bit set to 1 for constant */
};

enum DaoValueTrait
{
	DAO_VALUE_CONST   = (1<<1), /* constant data object */
	DAO_VALUE_NOCOPY  = (1<<2), /* data object not for copying */
	DAO_VALUE_DELAYGC = (1<<3), /* objects with this trait are scanned less frequently by GC */
	DAO_VALUE_BROKEN  = (1<<4) /* reference already broken (may have not been set to NULL) by GC */
};
enum DaoTypeAttribs
{
	DAO_TYPE_SPEC = (1<<0),  /* specializable type, with at least one type holder; */
	DAO_TYPE_UNDEF = (1<<1), /* undefined type, with at least one undefined type; */
	DAO_TYPE_SELF  = (1<<2), /* routine type that has self parameter; */
	DAO_TYPE_VARIADIC = (1<<3), /* variadic type (routine or tuple); */
	DAO_TYPE_SELFNAMED = (1<<4),
	DAO_TYPE_CODESECT = (1<<5)
};
enum DaoCaseMode
{
	DAO_CASE_ORDERED ,
	DAO_CASE_UNORDERED ,
	DAO_CASE_INTS , /* ordered integer cases; TODO optimize, enums */
	DAO_CASE_TABLE
};

enum DaoCallMode
{
	DAO_CALL_INIT   = (1<<8),  /* call to initialize a parent object; */
	DAO_CALL_NOSELF = (1<<9),  /* call without implicit self; */
	DAO_CALL_EXPAR  = (1<<10), /* expand the last parameter of tuple type; */
	DAO_CALL_BLOCK  = (1<<11), /* call with code block; */
	DAO_CALL_DECSUB = (1<<12), /* call decorated function; */
	DAO_CALL_ASYNC  = (1<<13), /* asynchronous call; */
	DAO_CALL_TAIL   = (1<<14), /* may do tail call; */
	DAO_CALL_FAST   = (1<<15)  /* may do fast call; */
};
enum DaoProcessPauseType
{
	DAO_PAUSE_NONE ,
	DAO_PAUSE_FUTURE_VALUE ,    /* future::value(); */
	DAO_PAUSE_FUTURE_WAIT ,     /* future::wait(); */
	DAO_PAUSE_CHANNEL_SEND ,    /* channel::send(); */
	DAO_PAUSE_CHANNEL_RECEIVE , /* channel::send(); */
	DAO_PAUSE_CHANFUT_SELECT ,  /* mt::select(); */
	DAO_PAUSE_COROUTINE_YIELD   /* coroutine; */
};

enum DaoDataPermission
{
	DAO_DATA_PRIVATE = 1,
	DAO_DATA_PROTECTED ,
	DAO_DATA_PUBLIC
};
enum DaoClassAttrib
{
	DAO_CLS_AUTO_INITOR   = 1,
	DAO_CLS_PRIVATE_VAR   = 2,
	DAO_CLS_PROTECTED_VAR = 4
};
enum DaoRoutineAttrib
{
	DAO_ROUT_PARSELF   = 1,       /* need self parameter */
	DAO_ROUT_STATIC    = (1<<1),  /* static function */
	DAO_ROUT_PRIVATE   = (1<<2),  /* private method */
	DAO_ROUT_PROTECTED = (1<<3),  /* protected method */
	DAO_ROUT_DEFERRED  = (1<<4),  /* deferred closure */
	DAO_ROUT_PASSRET   = (1<<5),  /* pass returned value to deferred closure */
	DAO_ROUT_CODESECT  = (1<<6),  /* code section routine */
	DAO_ROUT_DECORATOR = (1<<7),  /* function decorator */
	DAO_ROUT_INITOR    = (1<<8),  /* class constructor */
	DAO_ROUT_MAIN      = (1<<9),  /* main function */
	DAO_ROUT_REUSABLE  = (1<<10)  /* stack data for the routine is reusable */
};

#define DAO_TYPER_PRIV_FREE  (DAO_ROUT_REUSABLE<<1)
#define DAO_OPER_OVERLOADED  (DAO_TYPER_PRIV_FREE<<1)

enum DaoGlobalConstOffset
{
	DVR_NSC_NONE = LOOKUP_BIND( DAO_GLOBAL_CONSTANT, DAO_DATA_PUBLIC, 0, 1 ) ,
	DVR_NSC_MAIN = LOOKUP_BIND( DAO_GLOBAL_CONSTANT, DAO_DATA_PUBLIC, 0, 2 )
};
enum DaoGlobalVarOffset
{
	DVR_NSV_EXCEPTIONS
};

enum DaoArithOperType{

	DAO_OPER_REGEX_EQ  =0,
	DAO_OPER_REGEX_NE  =1,
	DAO_OPER_REGEX_ALL =2,

	DAO_OPER_ASSN ,
	DAO_OPER_ASSN_ADD ,
	DAO_OPER_ASSN_SUB ,
	DAO_OPER_ASSN_MUL ,
	DAO_OPER_ASSN_DIV ,
	DAO_OPER_ASSN_MOD ,
	DAO_OPER_ASSN_AND ,
	DAO_OPER_ASSN_OR ,
	DAO_OPER_ASSN_XOR ,

	DAO_OPER_IF ,
	DAO_OPER_COLON ,

	DAO_OPER_LLT,
	DAO_OPER_GGT,

	DAO_OPER_BIT_AND ,
	DAO_OPER_BIT_OR ,
	DAO_OPER_BIT_XOR ,

	DAO_OPER_AND ,
	DAO_OPER_OR ,

	DAO_OPER_IN ,
	DAO_OPER_NOTIN ,

	DAO_OPER_LT ,
	DAO_OPER_GT ,
	DAO_OPER_EQ ,
	DAO_OPER_NE ,
	DAO_OPER_LE ,
	DAO_OPER_GE ,
	DAO_OPER_TEQ ,
	DAO_OPER_TISA ,
	DAO_OPER_ASSERT ,

	DAO_OPER_ADD ,
	DAO_OPER_SUB ,
	DAO_OPER_DIV ,
	DAO_OPER_MUL ,
	DAO_OPER_MOD ,
	DAO_OPER_POW ,

	DAO_OPER_NOT ,
	DAO_OPER_INCR ,
	DAO_OPER_DECR ,
	DAO_OPER_NEGAT ,
	DAO_OPER_TILDE

};

enum DaoCtInfoId
{
	DAO_CTW_NULL =0,
	DAO_CTW_INTERNAL ,
	DAO_CTW_LIMIT_PAR_NUM ,
	DAO_CTW_UN_DEFINED ,
	DAO_CTW_UN_PAIRED ,
	DAO_CTW_IS_EXPECTED ,
	DAO_CTW_UN_EXPECTED ,
	DAO_CTW_UN_DECLARED ,
	DAO_CTW_WAS_DEFINED ,
	DAO_WARN_STATEMENT_SEPERATION ,
	DAO_WARN_ASSIGNMENT ,
	DAO_WARN_GET_SETTER ,
	DAO_NO_METHOD_TO_USE ,
	DAO_SYMBOL_POSSIBLY_UNDEFINED ,
	DAO_SYMBOL_NOT_DEFINED ,
	DAO_SYMBOL_WAS_DEFINED ,
	DAO_SYMBOL_NEED_CONSTANT ,
	DAO_SYMBOL_NEED_CLASS ,
	DAO_SYMBOL_NEED_CLASS_CTYPE ,
	DAO_SYMBOL_NEED_INTERFACE ,
	DAO_SYMBOL_NEED_BINDABLE ,
	DAO_TOKEN_NEED_STRING ,
	DAO_TOKEN_NEED_NAME ,
	DAO_TOKEN_EXPECTING ,
	DAO_TOKEN_NOT_FOUND ,
	DAO_TOKEN_NOT_EXPECTED ,
	DAO_TOKENS_NOT_PAIRED ,
	DAO_TOKENS_NOT_EXPECTED ,
	DAO_EXPR_NEED_CONST_NUMBER ,
	DAO_EXPR_NEED_CONST_EXPR ,
	DAO_EXPR_MODIFY_CONSTANT ,
	DAO_INTERFACE_NOT_COMPATIBLE,
	DAO_MISSING_INTERFACE_METHOD,
	DAO_FAILED_INTERFACE_BIND ,
	DAO_FAILED_INSTANTIATION ,
	DAO_UNDEFINED_SCOPE_NAME ,
	DAO_INVALID_TOKEN ,
	DAO_INVALID_PATH ,
	DAO_INVALID_ACCESS ,
	DAO_INVALID_STORAGE ,
	DAO_INVALID_SHARED ,
	DAO_INVALID_LITERAL ,
	DAO_INVALID_RADIX ,
	DAO_INVALID_DIGIT ,
	DAO_INVALID_TYPE_NAME ,
	DAO_INVALID_TYPE_FORM ,
	DAO_INVALID_REFERENCE ,
	DAO_INVALID_EXPRESSION ,
	DAO_INVALID_STATEMENT ,
	DAO_INVALID_UNCLOSED_SCOPE ,
	DAO_INVALID_SCOPE_ENDING ,
	DAO_INVALID_FUNCTIONAL ,
	DAO_INVALID_DECO_PATTERN ,
	DAO_INVALID_DEFINITION ,
	DAO_INVALID_ENUM_DEFINITION ,
	DAO_INVALID_CLASS_DEFINITION ,
	DAO_INVALID_FUNCTION_DEFINITION ,
	DAO_INVALID_INTERFACE_DEFINITION ,
	DAO_INVALID_FUNCTION_DECORATION ,
	DAO_INVALID_PARENT_CLASS ,
	DAO_INVALID_MIXIN_CLASS ,
	DAO_INVALID_USE_STMT ,
	DAO_INVALID_TYPE_ALIAS ,
	DAO_INVALID_BINDING ,
	DAO_INVALID_TYPEDEF ,
	DAO_INVALID_FOR ,
	DAO_INVALID_FORIN ,
	DAO_INVALID_PARAM_LIST ,
	DAO_PARAM_INVALID ,
	DAO_PARAM_NEED_SEPARATOR ,
	DAO_PARAM_MIDDLE_VALIST ,
	DAO_PARAM_REDUNDANT_TYPE ,
	DAO_PARAM_NEED_TYPE ,
	DAO_PARAM_NEED_DEFAULT ,
	DAO_PARAM_VARIABLE_DEFAULT ,
	DAO_PARAM_INVALID_DEFAULT ,
	DAO_PARAM_IMPROPER_DEFAULT ,
	DAO_PARAM_TOO_MANY ,
	DAO_PARAM_INVALID_RETURN ,
	DAO_SECTION_TOO_DEEP ,
	DAO_STATEMENT_IN_CLASS ,
	DAO_STATEMENT_IN_INTERFACE ,
	DAO_STATEMENT_OUT_OF_CONTEXT ,
	DAO_VARIABLE_OUT_OF_CONTEXT ,
	DAO_TYPE_NOT_MATCHING ,
	DAO_TYPE_PRESENTED ,
	DAO_TYPE_EXPECTED ,
	DAO_TYPE_NO_DEFAULT ,
	DAO_ROUT_NEED_RETURN_TYPE ,
	DAO_ROUT_INVALID_DECORATOR ,
	DAO_ROUT_INVALID_OPERATOR ,
	DAO_ROUT_INVALID_DECO_PARAM ,
	DAO_ROUT_CONSTRU_RETURN ,
	DAO_ROUT_NEED_IMPLEMENTATION ,
	DAO_ROUT_REDUNDANT_IMPLEMENTATION ,
	DAO_ROUT_NOT_DECLARED ,
	DAO_ROUT_WAS_IMPLEMENTED ,
	DAO_ROUT_DEFINED_SIGNATURE ,
	DAO_ROUT_DECLARED_SIGNATURE ,
	DAO_ROUT_USED_SIGNATURE ,
	DAO_ROUT_WRONG_SIGNATURE ,
	DAO_CONSTR_NOT_DEFINED ,
	DAO_CASE_NOT_VALID ,
	DAO_CASE_NOT_CONSTANT ,
	DAO_CASE_DUPLICATED ,
	DAO_DEFAULT_DUPLICATED ,
	DAO_EVAL_EXCEPTION ,
	DAO_LOAD_CYCLIC ,
	DAO_DISABLED_REGEX ,
	DAO_DISABLED_LONGINT ,
	DAO_DISABLED_NUMARRAY ,
	DAO_DISABLED_DECORATOR ,
	DAO_CTW_PAR_NOT_CST_DEF ,
	DAO_CTW_PAR_INVALID ,
	DAO_CTW_PAR_INVA_NUM ,
	DAO_CTW_PAR_INVA_NAMED ,
	DAO_CTW_PAR_INVA_CONST ,
	DAO_CTW_MY_NOT_CONSTR ,
	DAO_CTW_UNCOMP_PREFIX ,
	DAO_CTW_FOR_INVALID ,
	DAO_CTW_FORIN_INVALID ,
	DAO_CTW_CASE_INVALID ,
	DAO_CTW_CASE_NOT_CST ,
	DAO_CTW_CASE_NOT_SWITH ,
	DAO_CTW_LOAD_INVALID ,
	DAO_CTW_PATH_INVALID ,
	DAO_CTW_LOAD_INVALID_VAR ,
	DAO_CTW_LOAD_INVA_MOD_NAME ,
	DAO_CTW_LOAD_FAILED ,
	DAO_CTW_LOAD_CANCELLED ,
	DAO_CTW_LOAD_VAR_NOT_FOUND ,
	DAO_CTW_LOAD_REDUNDANT ,
	DAO_CTW_CST_INIT_NOT_CST ,
	DAO_CTW_MODIFY_CONST ,
	DAO_CTW_EXPR_INVALID ,
	DAO_CTW_ENUM_INVALID ,
	DAO_CTW_ENUM_LIMIT ,
	DAO_CTW_INVA_MUL_ASSN ,
	DAO_CTW_INVA_LITERAL ,
	DAO_CTW_INVA_QUOTES ,
	DAO_CTW_ASSIGN_INSIDE ,
	DAO_CTW_DAO_H_UNMATCH ,
	DAO_CTW_INVA_EMBED ,
	DAO_CTW_INVA_SYNTAX ,
	DAO_CTW_ASSN_UNMATCH ,
	DAO_CTW_VAR_REDEF ,
	DAO_CTW_INV_MAC_DEFINE ,
	DAO_CTW_INV_MAC_FIRSTOK ,
	DAO_CTW_INV_MAC_OPEN ,
	DAO_CTW_INV_MAC_VARIABLE ,
	DAO_CTW_INV_MAC_REPEAT ,
	DAO_CTW_INV_MAC_SPECTOK ,
	DAO_CTW_INV_MAC_INDENT ,
	DAO_CTW_REDEF_MAC_MARKER ,
	DAO_CTW_UNDEF_MAC_MARKER ,
	DAO_CTW_INV_MAC_LEVEL ,
	DAO_CTW_INV_TYPE_FORM ,
	DAO_CTW_INV_TYPE_NAME ,
	DAO_CTW_INV_CONST_EXPR ,
	DAO_CTW_TYPE_NOMATCH ,
	DAO_CTW_FAIL_BINDING ,
	DAO_CTW_FEATURE_DISABLED ,
	DAO_CTW_OBSOLETE_SYNTAX ,
	DAO_CTW_END
};

extern const char* getCtInfo( int tp );
extern const char* getRtInfo( int tp );

typedef struct DaoExceptionTripple
{
	int         code;
	const char *name;
	const char *info;
}DaoExceptionTripple;

extern const char* const daoExceptionName[];
extern const char* const daoExceptionInfo[];

DAO_DLL const char* getExceptName( int id );

extern const char* const coreTypeNames[];
extern const char *const daoBitBoolArithOpers[];
extern const char *const daoBitBoolArithOpers2[];

extern const char *daoRoutineCodeHeader;
extern const char *daoRoutineCodeFormat;

extern const char utf8_markers[256];

#endif
