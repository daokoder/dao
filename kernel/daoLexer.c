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

#include"stdlib.h"
#include"stdio.h"
#include"string.h"
#include"locale.h"
#include"ctype.h"

#include"daoConst.h"
#include"daoVmcode.h"
#include"daoStream.h"
#include"daoLexer.h"
#include<assert.h>

const char *const dao_oper_tokens[] =
{
	"" ,
	"#{" ,
	"[=[",
	"\'" ,
	"\"" ,
	"#" ,
	"id" ,
	"@id" ,
	"$id" ,
	"0x0" ,
	"0.0" ,
	"0x0" ,
	"0.0" ,
	"0F" ,
	"0D" ,
	"0C" ,
	"0E0" ,
	"[=[]=]",
	"\'\'" ,
	"\"\"" ,
	" " ,
	"\t" ,
	"\n" ,
	";" ,
	"(" ,
	")" ,
	"{" ,
	"}" ,
	"[" ,
	"]" ,
	"=" ,
	"." ,
	" ," ,
	":" ,
	"::" ,
	"+" ,
	"-" ,
	"*" ,
	"/" ,
	"%" ,
	"?" ,
	"??" ,
	"&" ,
	"|" ,
	"^" ,
	"!" ,
	"~" ,
	"$" ,
	"@" ,
	"@@" ,
	"**" ,
	"&&" ,
	"||" ,
	"not in",
	"+=" ,
	"-=" ,
	"*=" ,
	"/=" ,
	"%=" ,
	"&=" ,
	"|=" ,
	"^=" ,
	"==" ,
	"!=" ,
	"<" ,
	">" ,
	"<=" ,
	">=" ,
	"?=" ,
	"?<" ,
	"->" ,
	"=>" ,
	"<<" ,
	">>" ,
	"++" ,
	"--" ,
	"..." ,
	""
};

DIntStringPair dao_keywords[] =
{
	{ 100, "use" } ,
	{ 100, "load" } ,
	{   0, "as" } ,
	{ 100, "syntax" } ,
	{ DAO_CLASS, "class" } ,
	{ DAO_ROUTINE, "routine" } , /* for typing */
	{ 100, "operator" } ,
	{ 100, "interface" } ,
	{ 0, "self" } ,
	{ DAO_TYPE, "type" } ,
	{ DAO_ANY, "any" } ,
	{ DAO_NONE, "none" } ,
	{ DAO_INTEGER, "int" } ,
	{ DAO_FLOAT, "float" } ,
	{ DAO_DOUBLE, "double" } ,
	{ DAO_COMPLEX, "complex" } ,
	{ DAO_LONG, "long" } ,
	{ DAO_STRING, "string" } ,
	{ DAO_ENUM, "enum" } ,
	{ DAO_ARRAY, "array" } ,
	{ DAO_TUPLE, "tuple" } ,
	{ DAO_MAP, "map" } ,
	{ DAO_LIST, "list" } ,
	{ DAO_CDATA, "cdata" } ,
	{   0, "and" } ,
	{   0, "or" } ,
	{   0, "not" } ,
	{ 100, "if" } ,
	{ 100, "else" } ,
	{ 100, "for" } ,
	{   0, "in" } ,
	{ 100, "do" } ,
	{ 100, "while" } ,
	{ 100, "switch" } ,
	{ 100, "case" } ,
	{ 100, "default" } ,
	{ 100, "break" } ,
	{ 100, "skip" } ,
	{ 100, "defer" } ,
	{ 100, "return" } ,
	{   0, "yield" } ,
	{ 100, "const" } ,
	{ 100, "global" } ,
	{ 100, "static" } ,
	{ 100, "var" } ,
	{ 100, "private" } ,
	{ 100, "protected" } ,
	{ 100, "public" } ,
	{ 200, "rand" } ,
	{ 200, "ceil" } ,
	{ 200, "floor" } ,
	{ 200, "abs" } ,
	{ 200, "arg" } ,
	{ 200, "imag" } ,
	{ 200, "norm" } ,
	{ 200, "real" } ,
	{ 200, "acos" } ,
	{ 200, "asin" } ,
	{ 200, "atan" } ,
	{ 200, "cos" } ,
	{ 200, "cosh" } ,
	{ 200, "exp" } ,
	{ 200, "log" } ,
	{ 200, "sin" } ,
	{ 200, "sinh" } ,
	{ 200, "sqrt" } ,
	{ 200, "tan" } ,
	{ 200, "tanh" } ,
	{ 0, NULL }
};

/* by gperf */
enum
{
	TOTAL_KEYWORDS = 68,
	MIN_WORD_LENGTH = 2,
	MAX_WORD_LENGTH = 9,
	MIN_HASH_VALUE = 3,
	MAX_HASH_VALUE = 179
};

static const unsigned char asso_values[] =
{
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180,  15,  15,   5,
	5  ,  10,  40,  40,  95,  50, 180,  40,  30,  85,
	0  ,   0,  60,   0,  20,   5,   0,  10,   5,   5,
	20 ,  65, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180
};

/*
// io.read('daoLexer.h').extract('DKEY_%w+').map{X[5:].tolower()}.iterate{io.writeln(X)}
//
//  s/""/{0,""}/g
//  s/"\(\w\+\)"/{DKEY_\U\1,\L"\1"}/g
*/

static DIntStringPair wordlist[] =
{
	{0,""}, {0,""}, {0,""},
	{DKEY_NOT,"not"},
	{0,""}, {0,""}, {0,""},
	{DKEY_DO,"do"},
	{0,""},
	{DKEY_SQRT,"sqrt"},
	{DKEY_CONST,"const"},
	{0,""}, {0,""},
	{DKEY_COS,"cos"},
	{DKEY_NONE,"none"},
	{0,""},
	{DKEY_STATIC,"static"},
	{0,""},
	{DKEY_TAN,"tan"},
	{DKEY_ATAN,"atan"},
	{0,""},
	{DKEY_DOUBLE,"double"},
	{DKEY_DEFAULT,"default"},
	{DKEY_AND,"and"},
	{DKEY_ASIN,"asin"},
	{DKEY_TUPLE,"tuple"},
	{0,""},
	{DKEY_AS,"as"},
	{DKEY_USE,"use"},
	{DKEY_ACOS,"acos"},
	{DKEY_CDATA,"cdata"},
	{0,""},
	{DKEY_COMPLEX,"complex"},
	{0,""},
	{DKEY_CASE,"case"},
	{0,""},
	{DKEY_RETURN,"return"},
	{DKEY_ROUTINE,"routine"},
	{DKEY_ABS,"abs"},
	{DKEY_LOAD,"load"},
	{DKEY_DEFER,"defer"},
	{0,""},
	{DKEY_OR,"or"},
	{DKEY_VAR,"var"},
	{DKEY_RAND,"rand"},
	{DKEY_CLASS,"class"},
	{0,""}, {0,""}, {0,""},
	{DKEY_CEIL,"ceil"},
	{0,""},
	{DKEY_STRING,"string"},
	{DKEY_IN,"in"},
	{DKEY_INT,"int"},
	{DKEY_ELSE,"else"},
	{0,""}, {0,""}, {0,""},
	{DKEY_SIN,"sin"},
	{DKEY_SELF,"self"},
	{0,""}, {0,""}, {0,""},
	{DKEY_FOR,"for"},
	{DKEY_REAL,"real"},
	{0,""}, {0,""}, {0,""}, {0,""},
	{DKEY_INTERFACE,"interface"},
	{0,""}, {0,""}, {0,""},
	{DKEY_LOG,"log"},
	{DKEY_LONG,"long"},
	{DKEY_FLOAT,"float"},
	{0,""}, {0,""},
	{DKEY_ARG,"arg"},
	{DKEY_TYPE,"type"},
	{DKEY_BREAK,"break"},
	{DKEY_PUBLIC,"public"},
	{0,""},
	{DKEY_ANY,"any"},
	{DKEY_LIST,"list"},
	{0,""}, {0,""}, {0,""},
	{DKEY_OPERATOR,"operator"},
	{DKEY_NORM,"norm"},
	{0,""}, {0,""}, {0,""},
	{DKEY_EXP,"exp"},
	{DKEY_PROTECTED,"protected"},
	{DKEY_FLOOR,"floor"},
	{DKEY_SYNTAX,"syntax"},
	{DKEY_PRIVATE,"private"},
	{0,""},
	{DKEY_ENUM,"enum"},
	{0,""}, {0,""}, {0,""}, {0,""},
	{DKEY_COSH,"cosh"},
	{DKEY_ARRAY,"array"},
	{DKEY_GLOBAL,"global"},
	{0,""}, {0,""},
	{DKEY_SKIP,"skip"},
	{0,""},
	{DKEY_SWITCH,"switch"},
	{0,""}, {0,""},
	{DKEY_TANH,"tanh"},
	{DKEY_WHILE,"while"},
	{0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""},
	{DKEY_YIELD,"yield"},
	{0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""},
	{DKEY_IF,"if"},
	{0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""},
	{0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""},
	{0,""}, {0,""}, {0,""},
	{DKEY_SINH,"sinh"},
	{0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""},
	{DKEY_MAP,"map"},
	{0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""},
	{0,""}, {0,""}, {0,""}, {0,""}, {0,""}, {0,""},
	{DKEY_IMAG,"imag"}
};

enum
{
	LEX_ENV_NORMAL ,
	LEX_ENV_COMMENT
};

enum
{
	TOK_RESTART , /* emit token, and change to states[TOKEN_START][ char ] */
	TOK_START ,
	TOK_DIGITS_0 ,
	TOK_DIGITS_0X ,
	TOK_DIGITS_DEC ,
	TOK_DOT_DIGITS , /* .12 */
	TOK_DIGITS_DOT , /* 12. */
	TOK_NUMBER_DEC ,
	TOK_SINGLE_DEC ,
	TOK_DOUBLE_DEC ,
	TOK_NUMBER_HEX ,
	TOK_NUMBER_SCI_E , /* 1.2e */
	TOK_NUMBER_SCI_ES , /* 1.2e+ */
	TOK_NUMBER_SCI ,
	TOK_NUMBER_IMG ,
	TOK_DIGITS_LONG ,
	TOK_VERBATIM , /* @[ or @@[ */
	TOK_STRING_MBS ,
	TOK_STRING_WCS ,
	TOK_IDENTIFIER , /* a...z, A...Z, _, utf... */
	TOK_ID_INITYPE ,
	TOK_ID_SYMBOL ,
	TOK_LSB ,  /* [ */
	TOK_OP_COLON ,
	TOK_OP_ADD ,
	TOK_OP_SUB ,
	TOK_OP_MUL ,
	TOK_OP_DIV ,
	TOK_OP_MOD ,
	TOK_OP_AND ,
	TOK_OP_OR ,
	TOK_OP_XOR ,
	TOK_OP_NOT ,
	TOK_OP_EQ ,
	TOK_OP_LT ,
	TOK_OP_GT ,
	TOK_OP_DOT ,
	TOK_OP_AT , /* @ */
	TOK_OP_AT2 , /* @@, obsolete */
	TOK_OP_BANG2 ,
	TOK_OP_QUEST ,
	TOK_OP_IMG ,
	TOK_OP_TILDE ,
	TOK_OP_DOT2 ,
	TOK_OP_SHARP ,
	TOK_OP_ESC , /* \ */
	TOK_COMT_LINE ,
	TOK_COMT_OPEN ,
	TOK_COMT_CLOSE ,

	TOK_END , /* emit token + char, and change to TOKEN_START */
	TOK_END_CMT ,
	TOK_END_MBS ,
	TOK_END_WCS ,
	TOK_END_LB ,  /* () */
	TOK_END_RB ,
	TOK_END_LCB ,  /* {} */
	TOK_END_RCB ,
	TOK_END_LSB ,  /* [] */
	TOK_END_RSB ,
	TOK_END_COMMA ,  /* , */
	TOK_END_SEMCO ,  /* ; */
	TOK_END_DOTS ,  /* ... */
	TOK_END_POW ,  /* ** */
	TOK_END_AND ,  /* && */
	TOK_END_OR ,  /* || */
	TOK_END_INCR ,  /* ++ */
	TOK_END_DECR ,  /* -- */
	TOK_END_TEQ , /* ?=, Type EQ */
	TOK_END_TISA , /* ?<, Type IS A */
	TOK_END_LSHIFT ,  /* << */
	TOK_END_RSHIFT , /* >> */
	TOK_END_APPXTO , /* ~> */
	TOK_END_ARROW ,  /* -> */
	TOK_END_FIELD ,  /* => */
	TOK_END_COLON2 , /* :: */
	TOK_EQ_ADD ,  /* += */
	TOK_EQ_SUB ,  /* -= */
	TOK_EQ_MUL ,  /* *= */
	TOK_EQ_DIV ,  /* /= */
	TOK_EQ_MOD ,  /* %= */
	TOK_EQ_AND ,  /* &= */
	TOK_EQ_OR  ,  /* |= */
	TOK_EQ_XOR ,  /* ^= */
	TOK_EQ_NOT ,  /* != */
	TOK_EQ_EQ ,  /* == */
	TOK_EQ_LT ,  /* <= */
	TOK_EQ_GT ,  /* >= */
	TOK_ERROR /* emit error */
};
static unsigned char daoSpaceType[128] = {0};
static unsigned char daoLexTable[ TOK_ERROR ][128] = { { TOK_ERROR + 1 } };
static unsigned char daoTokenMap[ TOK_ERROR ] =
{
	DTOK_NONE , /* emit token, and change to states[TOKEN_START][ char ] */
	DTOK_NONE ,
	DTOK_DIGITS_DEC ,
	DTOK_NONE ,
	DTOK_DIGITS_DEC ,
	DTOK_NUMBER_DEC , /* .12 */
	DTOK_NUMBER_DEC , /* 12. */
	DTOK_NUMBER_DEC ,
	DTOK_SINGLE_DEC ,
	DTOK_DOUBLE_DEC ,
	DTOK_NUMBER_HEX ,
	DTOK_NUMBER_SCI , /* 1.2e */
	DTOK_NUMBER_SCI , /* 1.2e+ */
	DTOK_NUMBER_SCI ,
	DTOK_NUMBER_IMG ,
	DTOK_DIGITS_LONG ,
	DTOK_VBT_OPEN ,
	DTOK_MBS_OPEN ,
	DTOK_WCS_OPEN ,
	DTOK_IDENTIFIER , /* a...z, A...Z, _, utf... */
	DTOK_ID_THTYPE ,
	DTOK_ID_SYMBOL ,
	DTOK_LSB ,
	DTOK_COLON ,
	DTOK_ADD ,
	DTOK_SUB ,
	DTOK_MUL ,
	DTOK_DIV ,
	DTOK_MOD ,
	DTOK_AMAND ,
	DTOK_PIPE ,
	DTOK_XOR ,
	DTOK_NOT ,
	DTOK_ASSN ,
	DTOK_LT ,
	DTOK_GT ,
	DTOK_DOT ,
	DTOK_AT , /* @ */
	DTOK_AT2 , /* @@ */
	DTOK_BANG2 ,
	DTOK_QUERY ,
	DTOK_DOLLAR ,
	DTOK_TILDE ,
	DTOK_NONE ,
	DTOK_COMMENT , /* # */
	DTOK_NONE , /* \ */
	DTOK_COMMENT ,
	DTOK_CMT_OPEN ,
	DTOK_COMMENT ,

	DTOK_NONE , /* emit token + char, and change to TOKEN_START */
	DTOK_COMMENT ,
	DTOK_MBS ,
	DTOK_WCS ,
	DTOK_LB ,  /* () */
	DTOK_RB ,
	DTOK_LCB ,  /* {} */
	DTOK_RCB ,
	DTOK_LSB ,  /* [] */
	DTOK_RSB ,
	DTOK_COMMA ,  /* , */
	DTOK_SEMCO ,  /* ; */
	DTOK_DOTS ,  /* ... */
	DTOK_POW ,  /* ** */
	DTOK_AND ,  /* && */
	DTOK_OR ,  /* || */
	DTOK_INCR ,  /* ++ */
	DTOK_DECR ,  /* -- */
	DTOK_TEQ , /* ?=, Type EQ */
	DTOK_TISA , /* ?<, Type IS A */
	DTOK_LSHIFT ,  /* << */
	DTOK_RSHIFT , /* >> */
	DTOK_APPXTO ,  /* ~> */
	DTOK_ARROW ,   /* -> */
	DTOK_FIELD ,   /* => */
	DTOK_COLON2 ,  /* :: */
	DTOK_ADDASN ,  /* += */
	DTOK_SUBASN ,  /* -= */
	DTOK_MULASN ,  /* *= */
	DTOK_DIVASN ,  /* /= */
	DTOK_MODASN ,  /* %= */
	DTOK_ANDASN ,  /* &= */
	DTOK_ORASN  ,  /* |= */
	DTOK_XORASN ,  /* ^= */
	DTOK_NE ,  /* != */
	DTOK_EQ ,  /* == */
	DTOK_LE ,  /* <= */
	DTOK_GE ,  /* >= */
};


DOper daoArithOper[DAO_NOKEY2];

static int dao_hash( const char *str0, int len)
{
	const unsigned char *str = (const unsigned char*)str0;
	return len + asso_values[str[1]] + asso_values[str[0]] + asso_values[str[len - 1]];
}
int dao_key_hash( const char *str, int len )
{
	if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH) {
		int key = dao_hash (str, len);
		if (key <= MAX_HASH_VALUE && key >= 0) {
			DIntStringPair *s = wordlist + key;
			if (*str == *s->key && !strcmp (str + 1, s->key + 1)) return s->value;
		}
	}
	return 0;
}

static DOper doper(char o, char l, char r, char b)
{ DOper oper; oper.oper=o; oper.left = l, oper.right = r, oper.binary = b; return oper; }

void DaoInitLexTable()
{
	int i, j = DAO_NOKEY1+1;
	if( daoLexTable[0][0] <= TOK_ERROR ) return;
	memset( daoLexTable, TOK_RESTART, 128 * TOK_ERROR * sizeof(char) );
	memset( daoSpaceType, DTOK_NONE, 128 * sizeof(char) );
	memset( daoLexTable[TOK_NUMBER_SCI_E], TOK_ERROR, 128 * sizeof(char) );
	memset( daoLexTable[TOK_NUMBER_SCI_ES], TOK_ERROR, 128 * sizeof(char) );
	daoSpaceType[ (unsigned) ' ' ] = DTOK_BLANK;
	daoSpaceType[ (unsigned) '\t' ] = DTOK_TAB;
	daoSpaceType[ (unsigned) '\n' ] = DTOK_NEWLN;
	daoSpaceType[ (unsigned) '\r' ] = DTOK_NEWLN;
	for(j=0; j<128; j++){
		daoLexTable[ TOK_LSB ][j] = TOK_RESTART;
		daoLexTable[ TOK_OP_ESC ][j] = TOK_END;
		daoLexTable[ TOK_OP_SHARP ][j] = TOK_COMT_LINE;
		daoLexTable[ TOK_COMT_LINE ][j] = TOK_COMT_LINE;
		daoLexTable[ TOK_STRING_MBS ][ j ] = TOK_STRING_MBS;
		daoLexTable[ TOK_STRING_WCS ][ j ] = TOK_STRING_WCS;
		daoLexTable[ TOK_VERBATIM ][ j ] = TOK_ERROR;

		if( isdigit( j ) ){
			daoLexTable[ TOK_START ][ j ] = TOK_DIGITS_DEC;
			daoLexTable[ TOK_DIGITS_DEC ][ j ] = TOK_DIGITS_DEC;
			daoLexTable[ TOK_OP_DOT ][ j ] = TOK_DOT_DIGITS;
			daoLexTable[ TOK_DIGITS_0 ][ j ] = TOK_DIGITS_DEC;
			daoLexTable[ TOK_DIGITS_0X ][ j ] = TOK_NUMBER_HEX;
			daoLexTable[ TOK_DOT_DIGITS ][ j ] = TOK_DOT_DIGITS;
			daoLexTable[ TOK_DIGITS_DOT ][ j ] = TOK_NUMBER_DEC;
			daoLexTable[ TOK_NUMBER_DEC ][ j ] = TOK_NUMBER_DEC;
			daoLexTable[ TOK_NUMBER_HEX ][ j ] = TOK_NUMBER_HEX;
			daoLexTable[ TOK_NUMBER_SCI_E ][ j ] = TOK_NUMBER_SCI;
			daoLexTable[ TOK_NUMBER_SCI_ES ][ j ] = TOK_NUMBER_SCI;
			daoLexTable[ TOK_NUMBER_SCI ][ j ] = TOK_NUMBER_SCI;
			daoLexTable[ TOK_DIGITS_LONG ][ j ] = TOK_DIGITS_LONG;
			daoLexTable[ TOK_IDENTIFIER ][ j ] = TOK_IDENTIFIER;
			daoLexTable[ TOK_ID_INITYPE ][ j ] = TOK_ID_INITYPE;
			daoLexTable[ TOK_ID_SYMBOL ][ j ] = TOK_ID_SYMBOL;
			daoLexTable[ TOK_OP_AT ][ j ] = TOK_ID_INITYPE; /* @3 */
			daoLexTable[ TOK_VERBATIM ][j] = TOK_VERBATIM;
		}else if( isalpha( j ) || j == '_' ){
			daoLexTable[ TOK_START ][ j ] = TOK_IDENTIFIER;
			daoLexTable[ TOK_IDENTIFIER ][ j ] = TOK_IDENTIFIER;
			daoLexTable[ TOK_ID_INITYPE ][ j ] = TOK_ID_INITYPE;
			daoLexTable[ TOK_ID_SYMBOL ][ j ] = TOK_ID_SYMBOL;
			daoLexTable[ TOK_OP_AT ][ j ] = TOK_ID_INITYPE; /* @T */
			daoLexTable[ TOK_OP_IMG ][ j ] = TOK_ID_SYMBOL; /* $S */
			daoLexTable[ TOK_VERBATIM ][j] = TOK_VERBATIM;

			daoLexTable[ TOK_DIGITS_DEC ][ j ] = TOK_ERROR;
			daoLexTable[ TOK_DIGITS_0 ][ j ] = TOK_ERROR;
			daoLexTable[ TOK_DOT_DIGITS ][ j ] = TOK_ERROR;
			daoLexTable[ TOK_DIGITS_DOT ][ j ] = TOK_ERROR;
			daoLexTable[ TOK_NUMBER_DEC ][ j ] = TOK_ERROR;
			daoLexTable[ TOK_NUMBER_SCI_E ][ j ] = TOK_ERROR;
			daoLexTable[ TOK_NUMBER_SCI_ES ][ j ] = TOK_ERROR;
			daoLexTable[ TOK_NUMBER_SCI ][ j ] = TOK_ERROR;
			if( isxdigit( j ) ){
				daoLexTable[ TOK_DIGITS_0X ][ j ] = TOK_NUMBER_HEX;
				daoLexTable[ TOK_NUMBER_HEX ][ j ] = TOK_NUMBER_HEX;
			}else{
				daoLexTable[ TOK_DIGITS_0X ][ j ] = TOK_ERROR;
				daoLexTable[ TOK_NUMBER_HEX ][ j ] = TOK_ERROR;
			}
		}else{
			daoLexTable[ TOK_DIGITS_0X ][ j ] = TOK_ERROR;
		}
	}
	daoLexTable[ TOK_START ][ (unsigned) '(' ] = TOK_END_LB;
	daoLexTable[ TOK_START ][ (unsigned) ')' ] = TOK_END_RB;
	daoLexTable[ TOK_START ][ (unsigned) '{' ] = TOK_END_LCB;
	daoLexTable[ TOK_START ][ (unsigned) '}' ] = TOK_END_RCB;
	daoLexTable[ TOK_START ][ (unsigned) '[' ] = TOK_END_LSB;
	daoLexTable[ TOK_START ][ (unsigned) ']' ] = TOK_END_RSB;
	daoLexTable[ TOK_START ][ (unsigned) ',' ] = TOK_END_COMMA;
	daoLexTable[ TOK_START ][ (unsigned) ';' ] = TOK_END_SEMCO;
	daoLexTable[ TOK_OP_SHARP ][ (unsigned) '\n' ] = TOK_END_CMT;
	daoLexTable[ TOK_OP_SHARP ][ (unsigned) '\r' ] = TOK_END_CMT;
	daoLexTable[ TOK_COMT_LINE ][ (unsigned) '\n' ] = TOK_END_CMT;
	daoLexTable[ TOK_COMT_LINE ][ (unsigned) '\r' ] = TOK_END_CMT;
	daoLexTable[ TOK_START ][ (unsigned) '\'' ] = TOK_STRING_MBS;
	daoLexTable[ TOK_STRING_MBS ][ (unsigned) '\'' ] = TOK_END_MBS;
	daoLexTable[ TOK_START ][ (unsigned) '\"' ] = TOK_STRING_WCS;
	daoLexTable[ TOK_STRING_WCS ][ (unsigned) '\"' ] = TOK_END_WCS;
	daoLexTable[ TOK_START ][ (unsigned) '.' ] = TOK_OP_DOT;
	daoLexTable[ TOK_OP_DOT ][ (unsigned) '.' ] = TOK_OP_DOT2;
	daoLexTable[ TOK_OP_DOT2 ][ (unsigned) '.' ] = TOK_END_DOTS; /* ... */
	daoLexTable[ TOK_DIGITS_0 ][ (unsigned) '.' ] = TOK_DIGITS_DOT;
	daoLexTable[ TOK_DIGITS_DEC ][ (unsigned) '.' ] = TOK_DIGITS_DOT;
	daoLexTable[ TOK_DIGITS_0   ][ (unsigned) 'F' ] = TOK_SINGLE_DEC;
	daoLexTable[ TOK_DIGITS_DEC ][ (unsigned) 'F' ] = TOK_SINGLE_DEC;
	daoLexTable[ TOK_DOT_DIGITS ][ (unsigned) 'F' ] = TOK_SINGLE_DEC;
	daoLexTable[ TOK_DIGITS_DOT ][ (unsigned) 'F' ] = TOK_SINGLE_DEC;
	daoLexTable[ TOK_NUMBER_DEC ][ (unsigned) 'F' ] = TOK_SINGLE_DEC;
	daoLexTable[ TOK_NUMBER_SCI ][ (unsigned) 'F' ] = TOK_SINGLE_DEC;
	daoLexTable[ TOK_DIGITS_0   ][ (unsigned) 'D' ] = TOK_DOUBLE_DEC;
	daoLexTable[ TOK_DIGITS_DEC ][ (unsigned) 'D' ] = TOK_DOUBLE_DEC;
	daoLexTable[ TOK_DOT_DIGITS ][ (unsigned) 'D' ] = TOK_DOUBLE_DEC;
	daoLexTable[ TOK_DIGITS_DOT ][ (unsigned) 'D' ] = TOK_DOUBLE_DEC;
	daoLexTable[ TOK_NUMBER_DEC ][ (unsigned) 'D' ] = TOK_DOUBLE_DEC;
	daoLexTable[ TOK_NUMBER_SCI ][ (unsigned) 'D' ] = TOK_DOUBLE_DEC;
	daoLexTable[ TOK_DIGITS_0   ][ (unsigned) 'C' ] = TOK_NUMBER_IMG;
	daoLexTable[ TOK_DIGITS_DEC ][ (unsigned) 'C' ] = TOK_NUMBER_IMG;
	daoLexTable[ TOK_NUMBER_DEC ][ (unsigned) 'C' ] = TOK_NUMBER_IMG;
	daoLexTable[ TOK_NUMBER_SCI ][ (unsigned) 'C' ] = TOK_NUMBER_IMG;
	daoLexTable[ TOK_DIGITS_0   ][ (unsigned) 'L' ] = TOK_DIGITS_LONG;
	daoLexTable[ TOK_DIGITS_DEC ][ (unsigned) 'L' ] = TOK_DIGITS_LONG;
	daoLexTable[ TOK_NUMBER_HEX ][ (unsigned) 'L' ] = TOK_DIGITS_LONG;
	daoLexTable[ TOK_IDENTIFIER ][ (unsigned) '.' ] = TOK_RESTART;
	daoLexTable[ TOK_ID_INITYPE ][ (unsigned) '.' ] = TOK_RESTART;
	daoLexTable[ TOK_ID_SYMBOL ][ (unsigned) '.' ] = TOK_RESTART;
	daoLexTable[ TOK_OP_SHARP ][ (unsigned) '{' ] = TOK_COMT_OPEN;
	daoLexTable[ TOK_OP_SHARP ][ (unsigned) '}' ] = TOK_COMT_CLOSE;
	daoLexTable[ TOK_START ][ (unsigned) '\\' ] = TOK_OP_ESC;
	daoLexTable[ TOK_START ][ (unsigned) '@' ] = TOK_OP_AT;
	daoLexTable[ TOK_OP_AT ][ (unsigned) '@' ] = TOK_OP_AT2;
	daoLexTable[ TOK_OP_AT ][ (unsigned) '[' ] = TOK_VERBATIM;
	daoLexTable[ TOK_OP_AT2 ][ (unsigned) '[' ] = TOK_VERBATIM;
	daoLexTable[ TOK_VERBATIM ][ (unsigned) ' ' ] = TOK_VERBATIM;
	daoLexTable[ TOK_VERBATIM ][ (unsigned) '.' ] = TOK_VERBATIM;
	daoLexTable[ TOK_VERBATIM ][ (unsigned) ':' ] = TOK_VERBATIM;
	daoLexTable[ TOK_VERBATIM ][ (unsigned) '-' ] = TOK_VERBATIM;
	daoLexTable[ TOK_VERBATIM ][ (unsigned) '=' ] = TOK_VERBATIM;
	daoLexTable[ TOK_VERBATIM ][ (unsigned) '(' ] = TOK_VERBATIM;
	daoLexTable[ TOK_VERBATIM ][ (unsigned) ')' ] = TOK_VERBATIM;
	daoLexTable[ TOK_VERBATIM ][ (unsigned) '+' ] = TOK_VERBATIM;
	daoLexTable[ TOK_START ][ (unsigned) '~' ] = TOK_OP_TILDE;
	daoLexTable[ TOK_START ][ (unsigned) '=' ] = TOK_OP_EQ;

	/*  +=  -=  /=  *=  %=  &=  |=  ^=  !=  ==  <=  >=  */
	for(i=TOK_OP_ADD; i<TOK_OP_GT; i++)
		daoLexTable[i][ (unsigned) '=' ] = i + (TOK_EQ_ADD - TOK_OP_ADD);
	daoLexTable[ TOK_START ][ (unsigned) '>' ] = TOK_OP_GT;
	daoLexTable[ TOK_OP_QUEST ][ (unsigned) '=' ] = TOK_END_TEQ; /* ?= */
	daoLexTable[ TOK_OP_QUEST ][ (unsigned) '<' ] = TOK_END_TISA; /* ?< */
	daoLexTable[ TOK_OP_TILDE ][ (unsigned) '>' ] = TOK_END_APPXTO; /* ~> */
	daoLexTable[ TOK_OP_SUB ][ (unsigned) '>' ] = TOK_END_ARROW; /* -> */
	daoLexTable[ TOK_OP_EQ ][ (unsigned) '>' ] = TOK_END_FIELD; /* => */
	daoLexTable[ TOK_START ][ (unsigned) '<' ] = TOK_OP_LT;

	/* example use of generic types: */
	/* routine<=>int>; list<list<int>>; abc : list<int>={} */
	/* to handle them properly, the lexer should not compose tokens: <=, >>, >= */

	/* to be consistent, the lexer will not compose any tokens start with < or > */

	/* daoLexTable[ TOK_OP_GT ][ '>' ] = TOK_END_RSHIFT; */ /* >> */
	/* daoLexTable[ TOK_OP_LT ][ '<' ] = TOK_END_LSHIFT; */ /* << */

	daoLexTable[ TOK_START ][ (unsigned) '+' ] = TOK_OP_ADD;
	daoLexTable[ TOK_OP_ADD ][ (unsigned) '+' ] = TOK_END_INCR; /* ++ */
	daoLexTable[ TOK_START ][ (unsigned) '-' ] = TOK_OP_SUB;
	daoLexTable[ TOK_OP_SUB ][ (unsigned) '-' ] = TOK_END_DECR; /* -- */
	daoLexTable[ TOK_START ][ (unsigned) '*' ] = TOK_OP_MUL;
	daoLexTable[ TOK_OP_MUL ][ (unsigned) '*' ] = TOK_END_POW; /* ** */
	daoLexTable[ TOK_START ][ (unsigned) '/' ] = TOK_OP_DIV;
	daoLexTable[ TOK_START ][ (unsigned) '&' ] = TOK_OP_AND;
	daoLexTable[ TOK_OP_AND ][ (unsigned) '&' ] = TOK_END_AND; /* && */
	daoLexTable[ TOK_START ][ (unsigned) '|' ] = TOK_OP_OR;
	daoLexTable[ TOK_OP_OR ][ (unsigned) '|' ] = TOK_END_OR; /* || */
	daoLexTable[ TOK_START ][ (unsigned) ':' ] = TOK_OP_COLON;
	daoLexTable[ TOK_OP_COLON ][ (unsigned) ':' ] = TOK_END_COLON2; /* :: */
	daoLexTable[ TOK_OP_NOT ][ (unsigned) '!' ] = TOK_OP_BANG2;
	daoLexTable[ TOK_START ][ (unsigned) '%' ] = TOK_OP_MOD;
	daoLexTable[ TOK_START ][ (unsigned) '!' ] = TOK_OP_NOT;
	daoLexTable[ TOK_START ][ (unsigned) '^' ] = TOK_OP_XOR;
	daoLexTable[ TOK_START ][ (unsigned) '?' ] = TOK_OP_QUEST;
	daoLexTable[ TOK_START ][ (unsigned) '$' ] = TOK_OP_IMG;
	daoLexTable[ TOK_START ][ (unsigned) '#' ] = TOK_OP_SHARP;
	daoLexTable[ TOK_START ][ (unsigned) '0' ] = TOK_DIGITS_0;
	daoLexTable[ TOK_DIGITS_0 ][ (unsigned) 'x' ] = TOK_DIGITS_0X;
	daoLexTable[ TOK_DIGITS_0 ][ (unsigned) 'X' ] = TOK_DIGITS_0X;
	daoLexTable[ TOK_DIGITS_0 ][ (unsigned) 'e' ] = TOK_NUMBER_SCI_E;
	daoLexTable[ TOK_DIGITS_0 ][ (unsigned) 'E' ] = TOK_NUMBER_SCI_E;
	daoLexTable[ TOK_DIGITS_DEC ][ (unsigned) 'e' ] = TOK_NUMBER_SCI_E;
	daoLexTable[ TOK_DIGITS_DEC ][ (unsigned) 'E' ] = TOK_NUMBER_SCI_E;
	daoLexTable[ TOK_DIGITS_DOT ][ (unsigned) 'e' ] = TOK_NUMBER_SCI_E;
	daoLexTable[ TOK_DIGITS_DOT ][ (unsigned) 'E' ] = TOK_NUMBER_SCI_E;
	daoLexTable[ TOK_NUMBER_DEC ][ (unsigned) 'e' ] = TOK_NUMBER_SCI_E;
	daoLexTable[ TOK_NUMBER_DEC ][ (unsigned) 'E' ] = TOK_NUMBER_SCI_E;
	daoLexTable[ TOK_NUMBER_SCI_E ][ (unsigned) '+' ] = TOK_NUMBER_SCI_ES;
	daoLexTable[ TOK_NUMBER_SCI_E ][ (unsigned) '-' ] = TOK_NUMBER_SCI_ES;

	memset( daoArithOper, 0, DAO_NOKEY2*sizeof(DOper) );

	daoArithOper[ DTOK_INCR ]   = doper( DAO_OPER_INCR,     1, 0, 0 );
	daoArithOper[ DTOK_DECR ]   = doper( DAO_OPER_DECR,     1, 0, 0 );
	daoArithOper[ DTOK_ADD ]    = doper( DAO_OPER_ADD,      1, 0, 6 );
	daoArithOper[ DTOK_SUB ]    = doper( DAO_OPER_SUB,      1, 0, 5 );
	daoArithOper[ DTOK_NOT ]    = doper( DAO_OPER_NOT,      1, 0, 0 );
	daoArithOper[ DKEY_NOT ]    = doper( DAO_OPER_NOT,      1, 0, 0 );
	daoArithOper[ DTOK_TILDE ]  = doper( DAO_OPER_TILDE,    1, 0, 10 );
	daoArithOper[ DTOK_AMAND ]  = doper( DAO_OPER_BIT_AND,  0, 0, 1 );
	daoArithOper[ DTOK_ASSN ]   = doper( DAO_OPER_ASSN,     0, 0, 12 );
	daoArithOper[ DTOK_ADDASN ] = doper( DAO_OPER_ASSN_ADD, 0, 0, 11 );
	daoArithOper[ DTOK_SUBASN ] = doper( DAO_OPER_ASSN_SUB, 0, 0, 11 );
	daoArithOper[ DTOK_MULASN ] = doper( DAO_OPER_ASSN_MUL, 0, 0, 11 );
	daoArithOper[ DTOK_DIVASN ] = doper( DAO_OPER_ASSN_DIV, 0, 0, 11 );
	daoArithOper[ DTOK_MODASN ] = doper( DAO_OPER_ASSN_MOD, 0, 0, 11 );
	daoArithOper[ DTOK_ANDASN ] = doper( DAO_OPER_ASSN_AND, 0, 0, 11 );
	daoArithOper[ DTOK_ORASN ]  = doper( DAO_OPER_ASSN_OR,  0, 0, 11 );
	daoArithOper[ DTOK_XORASN ] = doper( DAO_OPER_ASSN_XOR, 0, 0, 11 );
	daoArithOper[ DTOK_QUERY ]  = doper( DAO_OPER_IF,       0, 0, 9 );
	daoArithOper[ DTOK_COLON ]  = doper( DAO_OPER_COLON,    0, 0, 10 );
	daoArithOper[ DTOK_LSHIFT ] = doper( DAO_OPER_LLT,      0, 0, 1 );
	daoArithOper[ DTOK_RSHIFT ] = doper( DAO_OPER_GGT,      0, 0, 1 );
	daoArithOper[ DTOK_PIPE ]   = doper( DAO_OPER_BIT_OR,   0, 0, 1 );
	daoArithOper[ DTOK_XOR ]    = doper( DAO_OPER_BIT_XOR,  0, 0, 1 );
	daoArithOper[ DTOK_AND ]    = doper( DAO_OPER_AND,      0, 0, 8 );
	daoArithOper[ DKEY_AND ]    = doper( DAO_OPER_AND,      0, 0, 8 );
	daoArithOper[ DTOK_OR ]     = doper( DAO_OPER_OR,       0, 0, 8 );
	daoArithOper[ DKEY_OR ]     = doper( DAO_OPER_OR,       0, 0, 8 );
	daoArithOper[ DKEY_IN ]     = doper( DAO_OPER_IN,       0, 0, 7 );
	daoArithOper[ DTOK_NOTIN ]  = doper( DAO_OPER_NOTIN,    0, 0, 7 );
	daoArithOper[ DTOK_LT ]     = doper( DAO_OPER_LT,       0, 0, 7 );
	daoArithOper[ DTOK_GT ]     = doper( DAO_OPER_GT,       0, 0, 7 );
	daoArithOper[ DTOK_EQ ]     = doper( DAO_OPER_EQ,       0, 0, 7 );
	daoArithOper[ DTOK_NE ]     = doper( DAO_OPER_NE,       0, 0, 7 );
	daoArithOper[ DTOK_LE ]     = doper( DAO_OPER_LE,       0, 0, 7 );
	daoArithOper[ DTOK_GE ]     = doper( DAO_OPER_GE,       0, 0, 7 );
	daoArithOper[ DTOK_TEQ ]    = doper( DAO_OPER_TEQ,      0, 0, 7 );
	daoArithOper[ DTOK_TISA ]   = doper( DAO_OPER_TISA,     0, 0, 7 );
	daoArithOper[ DTOK_MUL ]    = doper( DAO_OPER_MUL,      0, 0, 3 );
	daoArithOper[ DTOK_DIV ]    = doper( DAO_OPER_DIV,      0, 0, 3 );
	daoArithOper[ DTOK_MOD ]    = doper( DAO_OPER_MOD,      1, 0, 3 );
	daoArithOper[ DTOK_POW ]    = doper( DAO_OPER_POW,      0, 0, 2 );
}

typedef struct DaoToken2{ DaoToken token; DString string; } DaoToken2;

extern void DString_DeleteData( DString *self );

DaoToken* DaoToken_New()
{
	DaoToken *self = (DaoToken*) dao_calloc( 1, sizeof(DaoToken) );
	DString_Init( & self->string, 1 );
	return self;
}
void DaoToken_Delete( DaoToken *self )
{
	DString_DeleteData( & self->string );
	dao_free( self );
}
void DaoToken_Assign( DaoToken *self, DaoToken *other )
{
	self->type = other->type;
	self->name = other->name;
	self->cpos = other->cpos;
	self->line = other->line;
	self->index = other->index;
	DString_Assign( & self->string, & other->string );
}
DaoToken* DaoToken_Copy( DaoToken *self )
{
	DaoToken* copy = DaoToken_New();
	DaoToken_Assign( copy, self );
	return copy;
}
void DaoToken_Set( DaoToken *self, int type, int name, int index, const char *s )
{
	if( name == DTOK_ID_THTYPE || name == DTOK_ID_SYMBOL ) type = DTOK_IDENTIFIER;
	self->type = type;
	self->name = name;
	self->index = index;
	if( s ) DString_SetMBS( & self->string, s );
}

const char* DaoToken_NameToString( unsigned char name )
{
	if( name <= DTOK_NONE2 ) return dao_oper_tokens[name];
	if( name < DAO_NOKEY2 ) return dao_keywords[ name - DKEY_USE ].key;
	return "";
}
int DaoToken_IsNumber( const char *src, int size )
{
	int t, n = 0;
	if( size ==0 ) size = strlen( src );
	while( isspace( *src ) ) src += 1, size -= 1;
	while( size && isspace( src[size-1] ) ) size -= 1;
	if( size ==0 ) return 0;
	if( src[0] == '+' || src[0] == '-' ){
		src += 1;
		size -= 1;
	}
	t = DaoToken_Check( src, size, & n );
	return t >= DTOK_DIGITS_DEC && t <= DTOK_NUMBER_SCI && n == size;
}
int DaoToken_IsValidName( const char *src, int size )
{
	int t, n = 0;
	if( size ==0 ) size = strlen( src );
	t = DaoToken_Check( src, size, & n );
	return t == DTOK_IDENTIFIER && n == size;
}
/* return token type, store token length in length */
int DaoToken_Check( const char *src, int size, int *length )
{
	int old, type = 0;
	int it = 0, state = TOK_START;
	char ch = 0;
	while( it < size ){
		ch = src[it];
		if( state == TOK_STRING_MBS || state == TOK_STRING_WCS ){
			if( ch == '\\' ){
				it ++;
			}else if( ch == '\'' && state == TOK_STRING_MBS ){
				type = DTOK_MBS;
				break;
			}else if( ch == '\"' && state == TOK_STRING_WCS ){
				type = DTOK_WCS;
				break;
			}
		}else if( state == TOK_OP_SHARP ){
			if( ch == '{' ){
				state = TOK_COMT_OPEN;
			}else if( ch == '}' ){
				type = DTOK_COMMENT;
				break;
			}else if( ch == '\n' ){
				type = DTOK_COMMENT;
				break;
			}else{
				state = TOK_COMT_LINE;
			}
		}else if( state == TOK_COMT_LINE ){
			if( ch == '\n' ){
				type = DTOK_COMMENT;
				break;
			}
		}else if( state == TOK_COMT_OPEN ){
			if( ch == '#' && (it+1) < size && src[it+1] == '}' ){
				it ++;
				type = DTOK_COMMENT;
				break;
			}
		}else{
			old = state;
			if( ch >=0 ){
				state = daoLexTable[ state ][ (int)ch ];
			}else if( state <= TOK_START ){
				state = TOK_RESTART;
			}else if( state != TOK_IDENTIFIER && state != TOK_STRING_MBS
					&& state != TOK_STRING_WCS
					&& state != TOK_COMT_LINE && state != TOK_COMT_OPEN ){
				state = TOK_RESTART;
			}
			if( state >= TOK_END ){
				type = daoTokenMap[ state ];
				if( type == DTOK_NONE && isspace( src[0] ) )
					type = daoSpaceType[ (int)src[0] ];
				break;
			}else if( state == TOK_RESTART ){
				if( it ){
					it --;
					type = daoTokenMap[old];
				}else if( type == DTOK_NONE && isspace( ch ) ){
					type = daoSpaceType[ (int)ch ];
				}
				break;
			}
		}
		it ++;
	}
	if( type ==0 ){
		switch( state ){
		case TOK_VERBATIM : type = DTOK_VBT_OPEN; break;
		case TOK_STRING_MBS : type = DTOK_MBS_OPEN; break;
		case TOK_STRING_WCS : type = DTOK_WCS_OPEN; break;
		case TOK_OP_SHARP : type = DTOK_COMMENT; break;
		case TOK_COMT_OPEN : type = DTOK_CMT_OPEN; break;
		default : type = daoTokenMap[ state ]; it--; break;
		}
	}
	if( length ) *length = it < size ? it + 1 : size;
	return type;
}




DaoLexer* DaoLexer_New()
{
	DaoLexer *self = (DaoLexer*) dao_malloc( sizeof(DaoLexer) );
	self->tokens = DArray_New(D_TOKEN);
	self->tokbuf = DArray_New(0);
	return self;
}
void DaoLexer_Delete( DaoLexer *self )
{
	daoint i;
	for(i=0; i<self->tokbuf->size; ++i) DaoToken_Delete( self->tokbuf->items.pToken[i] );
	DArray_Delete( self->tokens );
	DArray_Delete( self->tokbuf );
	dao_free( self );
}

void DaoLexer_Reset( DaoLexer *self )
{
	daoint i;
	for(i=0; i<self->tokens->size; ++i){
		/* No copying of tokens: */
		DaoToken *token = self->tokens->items.pToken[i];
		if( token->string.size > 64 ) DString_Clear( & token->string );
		DArray_Append( self->tokbuf, token );
	}
	self->tokens->size = 0;
}
void DaoLexer_AppendToken( DaoLexer *self, DaoToken *token )
{
	DaoToken *tok;
	if( self->tokbuf->size == 0 ){
		DArray_Append( self->tokens, token );
		return;
	}
	tok = (DaoToken*) DArray_Back( self->tokbuf );
	self->tokbuf->size -= 1;
	DaoToken_Assign( tok, token );
	DArray_Append( self->tokens, NULL );  /* avoid copying; */
	self->tokens->items.pToken[self->tokens->size-1] = tok;
}
void DaoLexer_Append( DaoLexer *self, int name, int line, const char *data )
{
	DaoToken token = { 0, 0, 0, 0, 0 };

	token.type = token.name = name;
	token.line = line;
	if( name > DAO_NOKEY1 ) token.type = DTOK_IDENTIFIER;
	if( name == DTOK_ID_THTYPE || name == DTOK_ID_SYMBOL ) token.type = DTOK_IDENTIFIER;
	token.string = DString_WrapMBS( data );

	DaoLexer_AppendToken( self, & token );
}
int DaoLexer_Tokenize( DaoLexer *self, const char *src, int flags )
{
	DString *source = DString_New(1);
	DVector *lexenvs = DVector_New( sizeof(int) );
	DaoToken *token = DaoToken_New();
	DString *literal = & token->string;
	char ch, *ss, hex[11] = "0x00000000";
	int replace = flags & DAO_LEX_ESCAPE;
	int comment = flags & DAO_LEX_COMMENT;
	int space = flags & DAO_LEX_SPACE;
	int srcSize = (int)strlen( src );
	int old=0, state = TOK_START;
	int lexenv = LEX_ENV_NORMAL;
	int line = 1;
	int cpos = 0;
	int ret = 1;
	int it = 0;
	int i, m = 4;

	DString_SetSharing( literal, 0 );
	DaoLexer_Reset( self );

	DVector_PushInt( lexenvs, LEX_ENV_NORMAL );
	it = 0;
	token->cpos = 0;
	while( it < srcSize ){
#if 0
		printf( "tok: %i %i  %i  %c    %s\n", srcSize, it, ch, ch, literal->mbs );
#endif
		token->type = state;
		token->name = 0;
		token->line = line;
		ch = src[it];
		cpos += ch == '\t' ? daoConfig.tabspace : 1;
		if( ch == '\n' ) cpos = 0, line ++;
		if( literal->size == 0 ) token->cpos = cpos;
		if( state == TOK_STRING_MBS || state == TOK_STRING_WCS ){
			if( ch == '\\' ){
				it ++;
				if( replace == 0 ){
					DString_AppendChar( literal, ch );
					if( it < srcSize ){
						if( src[it] == '\n' ) cpos = 0, line ++;
						DString_AppendChar( literal, src[it] );
					}
					it ++;
					continue;
				}
				if( it >= srcSize ){
					ret = 0;
					printf( "error: incomplete string at line %i.\n", line );
					break;
				}
				if( src[it] == '\n' ) cpos = 0, line ++;
				switch( src[it] ){
				case '0' : case '1' : case '2' : case '3' :
				case '4' : case '5' : case '6' : case '7' : /* \ooo */
					i = 2;
					while( i < 5 && it < srcSize && src[it] >= '0' && src[it] < '8' ){
						hex[i] = src[it++];
						hex[++i] = 0;
					}
					DString_AppendChar( literal, (char) strtol( hex+2, NULL, 8 ) );
					it --;
					break;
				case '8' : case '9' :
					DString_AppendChar( literal, (char) (src[it] - '0') );
					break;
				case 'x' :
				case 'u' :
				case 'U' :
					i = 2;
					switch( src[it] ){
					case 'x' : m = 4;  break; /* \xhh: max 2 hex digit; */
					case 'u' : m = 6;  break; /* \uhhhh: max 4 hex digit; */
					case 'U' : m = 10; break; /* \Uhhhhhhhh: max 8 hex digit; */
					}
					while( i < m && (it+1) < srcSize && isxdigit( src[it+1] ) ){
						hex[i] = src[++it];
						hex[++i] = 0;
					}
					DString_AppendWChar( literal, (wchar_t) strtol( hex, NULL, 0 ) );
					break;
				case 't' : DString_AppendChar( literal, '\t' ); break;
				case 'n' : DString_AppendChar( literal, '\n' ); break;
				case 'r' : DString_AppendChar( literal, '\r' ); break;
				case '\'' : DString_AppendChar( literal, '\'' ); break;
				case '\"' : DString_AppendChar( literal, '\"' ); break;
				default : DString_AppendChar( literal, src[it] ); break;
				}
			}else if( ch == '\'' && state == TOK_STRING_MBS ){
				DString_AppendChar( literal, ch );
				state = TOK_RESTART;
				token->type = token->name = DTOK_MBS;
				DaoLexer_AppendToken( self, token );
				DString_Clear( literal );
			}else if( ch == '\"' && state == TOK_STRING_WCS ){
				DString_AppendChar( literal, ch );
				state = TOK_RESTART;
				token->type = token->name = DTOK_WCS;
				DaoLexer_AppendToken( self, token );
				DString_Clear( literal );
			}else{
				DString_AppendChar( literal, ch );
			}
		}else if( ch == ']' && state == TOK_VERBATIM ){
			int len = srcSize - it - 1;
			DString_AppendChar( literal, ']' );
			token->type = token->name = DTOK_VBT_OPEN;
			if( (ss = strstr( src + it + 1, literal->mbs )) != NULL ){
				len = (ss - src) - it - 1 + literal->size;
				token->type = token->name = DTOK_VERBATIM;
			}
			for(i=0; i<len; i++) if( src[it+1+i] == '\n' ) line += 1;
			DString_AppendDataMBS( literal, src + it + 1, len );
			state = TOK_RESTART;
			DaoLexer_AppendToken( self, token );
			DString_Clear( literal );
			it += len;
		}else if( lexenv == LEX_ENV_NORMAL ){
			old = state;
			if( ch >=0 ){
				state = daoLexTable[ state ][ (int)ch ];
			}else if( state <= TOK_START ){
				state = TOK_RESTART;
			}else if( state != TOK_IDENTIFIER && state != TOK_STRING_MBS
					&& state != TOK_STRING_WCS
					&& state != TOK_COMT_LINE && state != TOK_COMT_OPEN ){
				state = TOK_RESTART;
			}
			if( state >= TOK_END ){
				DString_AppendChar( literal, ch );
				token->type = token->name = daoTokenMap[ state ];
				if( token->type == DTOK_ID_THTYPE || token->type == DTOK_ID_SYMBOL )
					token->type = DTOK_IDENTIFIER;
				if( space || comment || token->type != DTOK_COMMENT ){
					if( isspace( token->string.mbs[0] ) )
						token->type = token->name = daoSpaceType[ (int)token->string.mbs[0] ];
					DaoLexer_AppendToken( self, token );
				}
				/* may be a token before the line break; */
				DString_Clear( literal );
				state = TOK_START;
			}else if( state == TOK_RESTART ){
				if( literal->size ){
					if( old == TOK_IDENTIFIER ){
						token->name = dao_key_hash( literal->mbs, literal->size );
						token->type = DTOK_IDENTIFIER;
						if( token->name == 0 ) token->name = DTOK_IDENTIFIER;
						DaoLexer_AppendToken( self, token );
					}else if( old > TOK_RESTART && old != TOK_END ){
						token->type = token->name = daoTokenMap[ old ];
						if( token->type == DTOK_ID_THTYPE || token->type == DTOK_ID_SYMBOL )
							token->type = DTOK_IDENTIFIER;
						DaoLexer_AppendToken( self, token );
					}else if( space ){
						if( isspace( token->string.mbs[0] ) )
							token->type = token->name = daoSpaceType[ (int)token->string.mbs[0] ];
						DaoLexer_AppendToken( self, token );
					}
					DString_Clear( literal );
					token->cpos = cpos;
				}
				DString_AppendChar( literal, ch );
				if( ch >=0 )
					state = daoLexTable[ TOK_START ][ (int)ch ];
				else
					state = TOK_IDENTIFIER;

			}else if( state == TOK_COMT_OPEN ){
				DString_AppendChar( literal, ch );
				lexenv = LEX_ENV_COMMENT;
				DVector_PushInt( lexenvs, LEX_ENV_COMMENT );
			}else{
				DString_AppendChar( literal, ch );
			}
		}else if( lexenv == LEX_ENV_COMMENT ){
			DString_AppendChar( literal, ch );
			if( ch == '#' ){
				state = TOK_OP_SHARP;
			}else if( ch == '{' && state == TOK_OP_SHARP ){
				state = TOK_COMT_OPEN;
				DVector_PushInt( lexenvs, LEX_ENV_COMMENT );
			}else if( ch == '}' && state == TOK_OP_SHARP ){
				state = TOK_COMT_CLOSE;
				DVector_Pop( lexenvs );
				lexenv = lexenvs->data.ints[lexenvs->size-1];
				if( lexenv != LEX_ENV_COMMENT ){
					token->type = token->name = DTOK_COMMENT;
					if( comment ) DaoLexer_AppendToken( self, token );
					DString_Clear( literal );
					state = TOK_RESTART;
				}
			}else{
				state = TOK_START;
			}
		}
		it ++;
	}
	if( literal->size ){
		token->type = token->name = daoTokenMap[ state ];
		if( lexenv == LEX_ENV_COMMENT ) token->type = token->name = DTOK_CMT_OPEN;
		switch( state ){
		case TOK_STRING_MBS : token->type = token->name = DTOK_MBS_OPEN; break;
		case TOK_STRING_WCS : token->type = token->name = DTOK_WCS_OPEN; break;
		}
		if( token->type == DTOK_IDENTIFIER ){
			token->name = dao_key_hash( literal->mbs, literal->size );
			if( token->name == 0 ) token->name = DTOK_IDENTIFIER;
		}else if( token->type == DTOK_ID_THTYPE || token->type == DTOK_ID_SYMBOL ){
			token->type = DTOK_IDENTIFIER;
		}
		if( token->type || space ){
			if( isspace( token->string.mbs[0] ) )
				token->type = token->name = daoSpaceType[ (int)token->string.mbs[0] ];
			DaoLexer_AppendToken( self, token );
		}
	}
	DaoToken_Delete( token );
	DVector_Delete( lexenvs );
	DString_Delete( source );
#if 0
	for(i=0; i<self->tokens->size; i++){
		DaoToken *tk = self->tokens->items.pToken[i];
		printf( "%4i: %4i %4i , %4i,  %s\n", i, tk->type, tk->name, tk->cpos, tk->string.mbs );
	}
#endif
	return ret ? line : 0;
}

void DaoLexer_AnnotateCode( DArray *self, DaoVmCodeX vmc, DString *annot, int max )
{
	DaoToken *t1, *t2, **tokens;
	daoint i, k, len, pos, m = max/(vmc.middle + vmc.last + 2);
	int max2 = max/2;
	if( m < 5 ) m = 5;
	DString_Clear( annot );
	if( self == NULL ) return; /* DaoRoutine::source could be null */
	if( vmc.middle > vmc.last ) return;
	tokens = self->items.pToken;
	for(i=0; i<vmc.middle; i++){
		k = i + vmc.first;
		if( k >= self->size ) break;
		t2 = tokens[k];
		if( k != (daoint)vmc.first ){
			t1 = tokens[k-1];
			pos = t1->cpos + t1->string.size;
			if( t1->line != t2->line || pos < t2->cpos ) DString_AppendChar( annot, ' ' );
		}
		len = t2->string.size;
		if( t2->type == DTOK_IDENTIFIER ){
			if( len > max2 ) len = max2 - 3;
		}else{
			if( len > m+3 ) len = m;
		}
		if( annot->size + len >= max2 ) len = max2 - annot->size;
		DString_AppendDataMBS( annot, t2->string.mbs, len );
		if( len != t2->string.size ){
			DString_AppendMBS( annot, "..." );
			if( t2->type == DTOK_MBS ) DString_AppendChar( annot, '\'' );
			else if( t2->type == DTOK_WCS ) DString_AppendChar( annot, '\"' );
			else break;
		}
		if( (i+1) < vmc.middle && annot->size >= max2 ){
			DString_AppendMBS( annot, "..." );
			break;
		}
	}
	for(i=vmc.middle; i<=vmc.last; i++){
		k = i + vmc.first;
		if( k >= self->size ) break;
		t2 = tokens[k];
		if( k != (daoint)vmc.first ){
			t1 = tokens[k-1];
			pos = t1->cpos + t1->string.size;
			if( t1->line != t2->line || pos < t2->cpos ) DString_AppendChar( annot, ' ' );
		}
		len = t2->string.size;
		if( t2->type == DTOK_IDENTIFIER ){
			if( len > max2 ) len = max2-3;
		}else{
			if( len > m+3 ) len = m;
		}
		if( annot->size + len >= max ) len = max - annot->size;
		DString_AppendDataMBS( annot, t2->string.mbs, len );
		if( len != t2->string.size ){
			DString_AppendMBS( annot, "..." );
			if( t2->type == DTOK_MBS ) DString_AppendChar( annot, '\'' );
			else if( t2->type == DTOK_WCS ) DString_AppendChar( annot, '\"' );
			else break;
		}
		if( i < vmc.last && annot->size >= max ){
			DString_AppendMBS( annot, "..." );
			break;
		}
	}
	DString_ChangeMBS( annot, "{{\n}}", "\\n", 0 );
}
int DaoLexer_FindOpenToken( DaoLexer *self, uchar_t tok, int start, int end )
{
	int i, n1, n2, n3;
	DaoToken **tokens = self->tokens->items.pToken;

	if( start < 0 ) return -1;
	if( end == -1 || end >= self->tokens->size ) end = self->tokens->size-1;

	n1 = n2 = n3 = 0;
	for( i=start; i<=end; i++){
		uchar_t tki = tokens[i]->name;
		if( ! ( n1 | n2 | n3 ) && tki == tok ){
			return i;
		}else if( n1 <0 || n2 <0 || n3 <0 ){
			return -1;
		}else{
			switch( tki ){
			case DTOK_LCB : n1 ++; break;
			case DTOK_RCB : n1 --; break;
			case DTOK_LB  : n2 ++; break;
			case DTOK_RB  : n2 --; break;
			case DTOK_LSB : n3 ++; break;
			case DTOK_RSB : n3 --; break;
			}
		}
	}
	return -1;
}
int DaoLexer_FindLeftPair( DaoLexer *self,  uchar_t lw, uchar_t rw, int start, int stop )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int k = 0;
	int i = start;
	int found = 0;
	uchar_t tk;

	if( start >= self->tokens->size ) start = self->tokens->size - 1;
	if( stop <0 ) stop = 0;

	while( i >= stop ){
		tk = tokens[i]->name;
		if( tk == rw ){
			k++;
		}else if( tk == lw ){
			k--;
			found = 1;
		}
		if( k==0 && found ) return i;
		i--;
	}
	return -1;
}
int DaoLexer_FindRightPair( DaoLexer *self, uchar_t lw, uchar_t rw, int start, int stop )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int k = 0;
	int i = start;
	int found = 0;
	uchar_t tk;

	if( start <0 ) return -1;
	if( stop== -1 ) stop = self->tokens->size-1;

	while(1){
		if( i > stop ) break;
		if( i >= (int) self->tokens->size ) break;

		tk = tokens[i]->name;
		if( tk == lw )
			k++;
		else if( tk == rw ){
			k--;
			found = 1;
		}

		if( k==0 && found ) return i;
		i++;
	}
	return -1;
}

DString* DaoLexer_AddRaiseStatement( DaoLexer *self, const char *type, const char *info, int line )
{
	DaoLexer_Append( self, DTOK_IDENTIFIER, line, "panic" );
	DaoLexer_Append( self, DTOK_LB, line, "(" );
	DaoLexer_Append( self, DTOK_IDENTIFIER, line, "Exception" );
	DaoLexer_Append( self, DTOK_COLON2, line, "::" );
	DaoLexer_Append( self, DTOK_IDENTIFIER, line, type );
	DaoLexer_Append( self, DTOK_LB, line, "(" );
	DaoLexer_Append( self, DTOK_MBS, line, info );
	DaoLexer_Append( self, DTOK_RB, line, ")" );
	DaoLexer_Append( self, DTOK_RB, line, ")" );
	DaoLexer_Append( self, DTOK_SEMCO, line, ";" );
	return & self->tokens->items.pToken[ self->tokens->size - 4 ]->string;
}
