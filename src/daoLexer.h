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

#ifndef DAO_LEXER_H
#define DAO_LEXER_H

#include"daoType.h"

enum DaoTokNames
{
  DTOK_NONE ,
  DTOK_CMT_OPEN , /* used by DaoStudio for code highlighting */
  DTOK_MBS_OPEN ,
  DTOK_WCS_OPEN ,
  DTOK_COMMENT ,
  DTOK_IDENTIFIER ,
  DTOK_DIGITS_HEX ,
  DTOK_DIGITS_DEC ,
  DTOK_NUMBER_HEX ,
  DTOK_NUMBER_DEC , /* 123. 123.5 */
  DTOK_DOUBLE_DEC , /* 345D, 123.25D */
  DTOK_NUMBER_SCI ,
  DTOK_MBS , /* MBS */
  DTOK_WCS , /* WCS */
  DTOK_BLANK , /*  */
  DTOK_TAB , /* \t */
  DTOK_NEWLN , /* \n */
  DTOK_LB , /* ( */
  DTOK_RB , /* ) */
  DTOK_LCB , /* { */
  DTOK_RCB , /* } */
  DTOK_LSB , /* [ */
  DTOK_RSB , /* ] */
  DTOK_ASSN , /* = */
  DTOK_DOT , /* . */
  DTOK_COMMA , /* , */
  DTOK_COLON , /* : */
  DTOK_SEMCO , /* ; */
  DTOK_COLON2 , /* :: */
  DTOK_ADD , /* + */
  DTOK_SUB , /* - */
  DTOK_MUL , /* * */
  DTOK_DIV , /* / */
  DTOK_MOD , /* % */
  DTOK_QUES , /* ? */
  DTOK_ASSERT , /* ?? */
  DTOK_AMAND , /* & */
  DTOK_PIPE , /* | */
  DTOK_NOT , /* ! */
  DTOK_XOR , /* ^ */
  DTOK_TILDE , /* ~ */
  DTOK_DOLLAR , /* $ */
  DTOK_AT , /* @ */
  DTOK_AT2 , /* @@ */
  DTOK_POW , /* ** */
  DTOK_AND , /* && */
  DTOK_OR , /* || */
  DTOK_CASSN , /* := */
  DTOK_ADDASN , /* += */
  DTOK_SUBASN , /* -= */
  DTOK_MULASN , /* *= */
  DTOK_DIVASN , /* /= */
  DTOK_MODASN , /* %= */
  DTOK_ANDASN , /* &= */
  DTOK_ORASN , /* |= */
  DTOK_EQ , /* == */
  DTOK_NE , /* != */
  DTOK_LT , /* < */
  DTOK_GT , /* > */
  DTOK_LE , /* <= */
  DTOK_GE , /* >= */
  DTOK_TEQ , /* ?=, Type EQ */
  DTOK_TISA , /* ?<, Type IS A */
  DTOK_ARROW , /* -> */
  DTOK_FIELD , /* => */
  DTOK_LSHIFT , /* << */
  DTOK_RSHIFT , /* >> */
  DTOK_INCR , /* ++ */
  DTOK_DECR , /* -- */
  DTOK_DOTS , /* ... */
  DTOK_ESC_LB ,  /* \( */
  DTOK_ESC_RB ,  /* \] */
  DTOK_ESC_LCB ,  /* \{ */
  DTOK_ESC_RCB ,  /* \} */
  DTOK_ESC_LSB ,  /* \[ */
  DTOK_ESC_RSB ,  /* \] */
  DTOK_ESC_PIPE ,  /* \| */
  DTOK_ESC_EXCLA ,  /* \! */
  DTOK_ESC_QUES ,  /* \? */
  DTOK_ESC_STAR ,  /* \* */
  DTOK_ESC_PLUS ,  /* \+ */
  DTOK_ESC_SQUO ,  /* \' */
  DTOK_ESC_DQUO ,  /* \" */
  DTOK_NONE2
};
enum DaoKeyNames
{
  DAO_NOKEY1 = DTOK_NONE2 ,
  DKEY_USE ,
  DKEY_LOAD ,
  DKEY_IMPORT ,
  DKEY_REQUIRE ,
  DKEY_BY ,
  DKEY_AS ,
  DKEY_SYNTAX ,
  DKEY_TYPEDEF ,
  DKEY_NAMESPACE ,
  DKEY_FINAL ,
  DKEY_CLASS ,
  DKEY_SUB ,
  DKEY_ROUTINE ,
  DKEY_FUNCTION ,
  DKEY_OPERATOR ,
  DKEY_SELF ,
  DKEY_INT ,
  DKEY_FLOAT ,
  DKEY_DOUBLE ,
  DKEY_COMPLEX ,
  DKEY_STRING ,
  DKEY_LONG ,
  DKEY_ARRAY ,
  DKEY_TUPLE ,
  DKEY_MAP ,
  DKEY_LIST ,
  DKEY_ANY ,
  DKEY_PAIR ,
  DKEY_CDATA ,
  DKEY_STREAM ,
  DKEY_IO ,
  DKEY_STD ,
  DKEY_STDIO ,
  DKEY_STDLIB ,
  DKEY_MATH ,
  DKEY_REFLECT ,
  DKEY_COROUTINE ,
  DKEY_NETWORK ,
  DKEY_MPI ,
  DKEY_MTLIB ,
  DKEY_AND ,
  DKEY_OR ,
  DKEY_NOT ,
  DKEY_IF ,
  DKEY_ELSE ,
  DKEY_ELIF ,
  DKEY_ELSEIF ,
  DKEY_FOR ,
  DKEY_IN ,
  DKEY_DO ,
  DKEY_WHILE ,
  DKEY_UNTIL ,
  DKEY_SWITCH ,
  DKEY_CASE ,
  DKEY_DEFAULT ,
  DKEY_BREAK ,
  DKEY_CONTINUE ,
  DKEY_SKIP ,
  DKEY_RETURN ,
  DKEY_YIELD ,
  DKEY_ENUM ,
  DKEY_CONST ,
  DKEY_GLOBAL ,
  DKEY_STATIC ,
  DKEY_VAR ,
  DKEY_PRIVATE ,
  DKEY_PROTECTED ,
  DKEY_PUBLIC ,
  DKEY_VIRTUAL ,
  DKEY_TRY ,
  DKEY_RETRY ,
  DKEY_CATCH ,
  DKEY_RESCUE ,
  DKEY_RAISE ,
  DKEY_ASYNC ,
  DKEY_HURRY ,
  DKEY_JOIN ,
  DKEY_EXTERN ,
  DKEY_EACH ,
  DKEY_REPEAT ,
  DKEY_APPLY ,
  DKEY_FOLD ,
  DKEY_REDUCE ,
  DKEY_UNFOLD ,
  DKEY_SORT ,
  DKEY_SELECT ,
  DKEY_INDEX ,
  DKEY_COUNT ,
  DKEY_ABS ,
  DKEY_ACOS ,
  DKEY_ARG ,
  DKEY_ASIN ,
  DKEY_ATAN ,
  DKEY_CEIL ,
  DKEY_COS ,
  DKEY_COSH ,
  DKEY_EXP ,
  DKEY_FLOOR ,
  DKEY_IMAG ,
  DKEY_LOG ,
  DKEY_NORM ,
  DKEY_RAND ,
  DKEY_REAL ,
  DKEY_SIN ,
  DKEY_SINH ,
  DKEY_SQRT ,
  DKEY_TAN ,
  DKEY_TANH ,
  DAO_NOKEY2 ,
  DKEY_HERE ,
  DKEY_A
};
int dao_key_hash( const char *str, int len );

typedef struct DIntStringPair
{
  int   value;
  const char *key;
}DIntStringPair;

typedef struct {
  unsigned char oper;
  unsigned char left;
  unsigned char right;
  unsigned char binary;
} DOper;

struct DaoToken
{
  uchar_t   type; /* token type: take value in DaoTokNames */
  uchar_t   name; /* token name: may take value in DaoKeyNames */
  ushort_t  line; /* file line position of the token */
  ushort_t  cpos; /* charactor position in the line */ 
  ushort_t  index; /* used for DaoRoutine, for DaoStudio */
  DString  *string; /* token string */
  /* When DaoToken is used in an array to store the definitions
   * of local constants and variables in a routine,
   * (1) type field indicates if it is a constant=0, or varaible=1;
   * (2) name field indicates the lixical level of the cst/var;
   * (3) index field indicates the index of the cst/var value;
   * (4) string field stores the name.
   */
};

DaoToken* DaoToken_New();
void DaoToken_Delete( DaoToken *self );
int DaoToken_Tokenize( DArray *tokens, const char *src, int repl, int comment, int space );

void DaoTokens_Append( DArray *self, int name, int line, const char *data );

#endif
