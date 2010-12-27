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

#include"stdlib.h"
#include"stdio.h"
#include"string.h"
#include"locale.h"
#include"ctype.h"

#include"daoConst.h"
#include"daoOpcode.h"

#include"daoParser.h"
#include"daoRegex.h"
#include"daoMacro.h"

#include"daoNumtype.h"
#include"daoRoutine.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoVmspace.h"
#include"daoNamespace.h"
#include"daoStream.h"

#include"daoStdlib.h"

#include"daoContext.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoBase.h"
#include<assert.h>

typedef struct DStringIntPair
{
	const char *key;
	int   value;
}DStringIntPair;

extern DOper daoArithOper[DAO_NOKEY2];
extern DIntStringPair dao_keywords[];

static DStringIntPair mapCallMode[]=
{
	{ "init",       DAO_CALL_INIT } ,
	{ NULL, 0 }
};
static const int countCallMode = 1;

static const int mapAithOpcode[]=
{
	200, 200, 200, /* padding for regex opers */

	DVM_NAMEVA, /* => */

	DVM_MOVE , /* DAO_OPER_ASSN */
	-DVM_ADD , /* DAO_OPER_ASSN_ADD */
	-DVM_SUB , /* DAO_OPER_ASSN_SUB */
	-DVM_MUL , /* DAO_OPER_ASSN_DIV */
	-DVM_DIV , /* DAO_OPER_ASSN_MUL */
	-DVM_MOD , /* DAO_OPER_ASSN_MOD */
	-DVM_BITAND , /* DAO_OPER_ASSN_AND */
	-DVM_BITOR  , /* DAO_OPER_ASSN_OR */

	200,
	DVM_PAIR , /* DAO_OPER_COLON */

	DVM_BITLFT, /* << */
	DVM_BITRIT, /* >> */
	DVM_BITAND, /* & */
	DVM_BITOR,  /* | */
	DVM_BITXOR, /* ^ */

	DVM_AND , /* DAO_OPER_AND */
	DVM_OR  , /* DAO_OPER_OR */

	DVM_IN  , /* DAO_OPER_IN */

	DVM_LT  , /* DAO_OPER_LT */
	-DVM_LT , /* DAO_OPER_GT */
	DVM_EQ  , /* DAO_OPER_EQ */
	DVM_NE  , /* DAO_OPER_NE */
	DVM_LE  , /* DAO_OPER_LE */
	-DVM_LE , /* DAO_OPER_GE */
	DVM_CHECK ,
	DVM_CHECK ,
	200 , /* padding for assertion operator */

	DVM_ADD , /* DAO_OPER_ADD */
	DVM_SUB , /* DAO_OPER_SUB */
	DVM_DIV , /* DAO_OPER_DIV */
	DVM_MUL , /* DAO_OPER_MUL */
	DVM_MOD , /* DAO_OPER_MOD */
	DVM_POW , /* ** */

	DVM_NOT /* DAO_OPER_NOT */
};

static const DStringIntPair mapCompOptKey[]=
{
	{ "MBS", DAO_EXEC_MBS_ONLY },
	{ "WCS", DAO_EXEC_WCS_ONLY },
	{ NULL, 0 }
};

enum
{
	EXP_IN_CALL = 1
};

/* Binary searching:
 * pairs is assumed to be sorted by keys:
 */
static int KeyBinSearch( const DStringIntPair pairs[], int size, DString *key )
{
	int left = 0;
	int right = size - 1;
	int mid, res;
	while( left <= right ){
		mid = ( left + right ) / 2;
		res = strcmp( pairs[ mid ].key, key->mbs );
		if( res == 0 ){
			return pairs[ mid ].value;
		}else if( res > 0){
			right = mid - 1;
		}else{
			left = mid + 1;
		}
	}
	return -1;
}
/* Sequential searching: */
static int KeySeqSearch( const DStringIntPair pairs[], DString *key )
{
	int i = 0;
	while( pairs[i].key != NULL ){
		if( strcmp( pairs[i].key, key->mbs ) == 0 ) return pairs[i].value;
		i ++;
	}
	return -1;
}

DaoInode* DaoInode_New( DaoParser *p )
{
	DaoInode *self = dao_malloc( sizeof(DaoInode) );
	memset( self, 0, sizeof(DaoInode) );
	return self;
}
void DaoInode_Delete( DaoInode *self )
{
	free( self );
}
static void DaoInode_Print( DaoInode *self )
{
	const char *name = getOpcodeName( self->code );
	static const char *fmt = "%3i: %-8s : %5i, %5i, %5i;  [%3i] %9p %9p %9p, %s\n";
	printf( fmt, self->id, name, self->a, self->b, self->c, self->line,
			self, self->jumpTrue, self->jumpFalse, "" );
}
static void DString_AppendToken( DString *self, DaoToken *token )
{
	if( self == NULL ) return;
	if( token->type == DTOK_MBS || token->type == DTOK_WCS ){
		if( token->string->size > 10 ){
			DString_AppendDataMBS( self, token->string->mbs, 6 );
			DString_AppendMBS( self, "..." );
			DString_AppendChar( self, token->string->mbs[0] );
		}else{
			DString_Append( self, token->string );
		}
	}else{
		DString_Append( self, token->string );
	}
}

DaoParser* DaoParser_New()
{
	DaoParser *self = (DaoParser*) dao_calloc( 1, sizeof(DaoParser) );

	self->fileName = DString_New(1);
	self->tokens = DArray_New(D_TOKEN);
	self->partoks = DArray_New(D_TOKEN);
	self->comments = DMap_New(0,D_STRING);

	self->vmCodes = DArray_New(D_VMCODE);
	self->vmcBase = DaoInode_New( self );
	self->vmcFirst = self->vmcLast = self->vmcTop = self->vmcBase;
	self->vmcBase->code = DVM_UNUSED;

	self->allConsts = DHash_New(D_STRING,0);

	self->regLines = DArray_New(0);
	self->regRefers = DArray_New(0);
	self->varFunctional = DHash_New(D_STRING,0);
	self->initTypes = DMap_New(0,0);

	self->scoping = DArray_New(0);
	self->errors = DArray_New(D_TOKEN);
	self->decoFuncs = DArray_New(0);
	self->decoParams = DArray_New(0);

	self->nullValue = -1;
	self->integerZero = -1;
	self->integerOne = -1;
	self->imaginaryOne = -1;
	self->imgone.real = 0.0;
	self->imgone.imag = 1.0;

	self->routCompilable = DArray_New(0);

	self->routName = DString_New(1);
	self->mbs = DString_New(1);
	self->mbs2 = DString_New(1);
	self->str = DString_New(1);
	self->bigint = DLong_New();
	self->denum = DEnum_New(NULL,0);

	self->toks = DArray_New(0);
	self->lvm = DMap_New(D_STRING,0);
	self->localVarMap = DArray_New(D_MAP);
	self->localCstMap = DArray_New(D_MAP);
	self->switchMaps = DArray_New(D_MAP);
	self->enumTypes = DArray_New(0);
	DArray_Append( self->localVarMap, self->lvm );
	DArray_Append( self->localCstMap, self->lvm );

	self->indent = 1;
	return self;
}
void DaoParser_ClearCodes( DaoParser *self );
void DaoParser_Delete( DaoParser *self )
{
	DString_Delete( self->fileName );
	DString_Delete( self->routName );
	DString_Delete( self->mbs );
	DString_Delete( self->mbs2 );
	DString_Delete( self->str );
	DEnum_Delete( self->denum );
	DArray_Delete( self->decoFuncs );
	DArray_Delete( self->decoParams );
	DArray_Delete( self->tokens );
	DArray_Delete( self->partoks );
	DArray_Delete( self->toks );
	DArray_Delete( self->localVarMap );
	DArray_Delete( self->localCstMap );
	DArray_Delete( self->routCompilable );
	DArray_Delete( self->switchMaps );
	DArray_Delete( self->enumTypes );
	DArray_Delete( self->scoping );
	DArray_Delete( self->errors );
	DArray_Delete( self->regLines );
	DArray_Delete( self->regRefers );
	DArray_Delete( self->vmCodes );
	DMap_Delete( self->comments );
	if( self->uplocs ) DArray_Delete( self->uplocs );
	if( self->bindtos ) DArray_Delete( self->bindtos );
	if( self->allConsts ) DMap_Delete( self->allConsts );
	DMap_Delete( self->varFunctional );
	DMap_Delete( self->initTypes );
	DMap_Delete( self->lvm );
	DLong_Delete( self->bigint );
	DaoParser_ClearCodes( self );
	DaoInode_Delete( self->vmcBase );
	dao_free( self );
}

static void DaoParser_PrintCodes( DaoParser *self )
{
	DaoInode *it = self->vmcFirst;
	int i = 0;
	while( it ){
		it->id = i ++;
		DaoInode_Print( it );
		it = it->next;
	}
}

void DaoParser_ClearCodes( DaoParser *self )
{
	DaoInode *it = self->vmcFirst;
	while( it != self->vmcBase ){
		it = it->next;
		DaoInode_Delete( it->prev );
	}
	it = self->vmcLast;
	while( it != self->vmcBase ){
		it = it->prev;
		DaoInode_Delete( it->next );
	}
	self->vmcBase->prev = self->vmcBase->next = self->vmcBase->below = NULL;
	self->vmcFirst = self->vmcLast = self->vmcTop = self->vmcBase;
	self->vmcCount = 0;
}
static void DaoParser_PopFrontCode( DaoParser *self )
{
	if( self->vmcFirst == NULL || self->vmcFirst == self->vmcBase ) return;
	self->vmcFirst = self->vmcFirst->next;
	DaoInode_Delete( self->vmcFirst->prev );
	self->vmcFirst->prev = NULL;
	self->vmcCount --;
}
static void DaoParser_PopBackCode( DaoParser *self )
{
	if( self->vmcLast == NULL || self->vmcLast == self->vmcBase ) return;
	if( self->vmcLast == self->vmcTop ) self->vmcTop = self->vmcTop->below;
	self->vmcLast = self->vmcLast->prev;
	DaoInode_Delete( self->vmcLast->next );
	self->vmcLast->next = NULL;
	self->vmcCount --;
}
static int DaoParser_PopCodes( DaoParser *self, DaoInode *front, DaoInode *back )
{
	int count = 0;
	DaoInode *node = NULL;
	while( (node=self->vmcFirst) != front ) DaoParser_PopFrontCode( self ), count ++;
	while( (node=self->vmcLast) != back ) DaoParser_PopBackCode( self ), count ++;
	return count;
}
static void DaoParser_PushFrontCode( DaoParser *self, DaoVmCodeX *vmc )
{
	DaoInode *node = DaoInode_New( self );
	DaoInode *second = self->vmcFirst->next;
	memcpy( node, vmc, sizeof(DaoVmCode) );
	node->level = vmc->level;
	node->line = vmc->line;
	node->first = vmc->first;
	node->middle = vmc->middle;
	node->last = vmc->last;
	node->next = self->vmcFirst;
	self->vmcFirst->prev= node;
	self->vmcFirst = node;
	self->vmcCount ++;
}
static void DaoParser_AddCode2( DaoParser *self, ushort_t code,
		ushort_t a, ushort_t b, ushort_t c, int first, int mid, int last );
static void DaoParser_PushBackCode( DaoParser *self, DaoVmCodeX *vmc )
{
	DaoInode *node = DaoInode_New( self );
	memcpy( node, vmc, sizeof(DaoVmCode) );
	node->level = vmc->level;
	node->line = vmc->line;
	node->first = vmc->first;
	node->middle = vmc->middle;
	node->last = vmc->last;
	if( self->vmcLast->line != vmc->line && (self->vmSpace->options & DAO_EXEC_IDE) ){
		if( self->vmcLast != self->vmcBase && self->vmcLast->code != DVM_NOP )
			DaoParser_AddCode2( self, DVM_NOP, 0,0,0, vmc->first, 0,0 );
	}

	self->vmcLast->next = node;
	node->prev = self->vmcLast;
	self->vmcLast = node;
	self->vmcCount ++;
}

static void DaoParser_Warn2( DaoParser *self, int code, DString *ext )
{
	DaoStream *stream = self->vmSpace->stdStream;
	DaoStream_WriteMBS( stream, "  At line " );
	DaoStream_WriteInt( stream, self->curLine );
	DaoStream_WriteMBS( stream, " : " );
	DaoStream_WriteMBS( stream, getCtInfo( code ) );
	if( ext && ext->size ){
		DaoStream_WriteMBS( stream, " --- \"" );
		DaoStream_WriteString( stream, ext );
		DaoStream_WriteMBS( stream, "\"" );
	}
	DaoStream_WriteMBS( stream, ";\n" );
}
void DaoParser_Warn( DaoParser *self, int code, DString *ext )
{
	DaoStream *stream = self->vmSpace->stdStream;
	DaoStream_WriteMBS( stream, "[[WARNING]] in file \"" );
	DaoStream_WriteString( stream, self->fileName );
	DaoStream_WriteMBS( stream, "\":\n" );
	DaoParser_Warn2( self, code, ext );
}
void DaoParser_Error( DaoParser *self, int code, DString *ext )
{
	DaoTokens_Append( self->errors, code, self->curLine, ext ? ext->mbs : "" );
	self->error = code;
}
void DaoParser_SumTokens( DaoParser *self, DString *sum, int m, int n, int single_line )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoToken *tok, *tok0=NULL;
	int i, line = self->curLine;
	DString_Clear( sum );
	if( m < 0 ) m = 0;
	if( n >= self->tokens->size ) n = self->tokens->size - 1;
	if( m < n ) line = tokens[m]->line;
	for(i=m; i<=n; i++){
		tok = tokens[i];
		if( single_line && tok->line > line ) break;
		if( tok0 && (tok->line != tok0->line || tok->cpos > (tok0->cpos + tok0->string->size)) )
			DString_AppendChar( sum, ' ' );
		tok0 = tok;
		DString_Append( sum, tokens[i]->string );
		if( i<n && sum->size > 30 ){
			DString_AppendMBS( sum, " ..." );
			break;
		}
	}
}
/* tokens from m to n as message */
void DaoParser_Error2( DaoParser *self, int code, int m, int n, int single_line )
{
	DaoToken *last;
	DaoTokens_Append( self->errors, code, self->curLine, "" );
	last = self->errors->items.pToken[ self->errors->size-1 ];
	self->error = code;
	DaoParser_SumTokens( self, last->string, m, n, single_line );
}
/* tokens from m until the end of the line as message */
void DaoParser_Error3( DaoParser *self, int code, int m )
{
	DaoToken *last;
	DaoTokens_Append( self->errors, code, self->curLine, "" );
	last = self->errors->items.pToken[ self->errors->size-1 ];
	self->error = code;
	DaoParser_SumTokens( self, last->string, m, self->tokens->size-1, 1 );
}
void DaoParser_Suggest( DaoParser *self, const char *suggestion )
{
	DaoStream_WriteMBS( self->vmSpace->stdStream, "suggestion:\n" );
	DaoStream_WriteMBS( self->vmSpace->stdStream, suggestion );
	DaoStream_WriteChar( self->vmSpace->stdStream, '\n' );
}
void DaoParser_PrintError( DaoParser *self, int line, int code, DString *ext )
{
	int i;
	DaoStream *stream = self->vmSpace->stdStream;

	if( code ) DaoTokens_Append( self->errors, code, line, ext ? ext->mbs : "" );
	if( self->errors->size ==0 ) return;
	DaoStream_WriteMBS( stream, "[[ERROR]] in file \"" );
	DaoStream_WriteString( stream, self->fileName );
	DaoStream_WriteMBS( stream, "\":\n" );

	for(i=self->errors->size-1; i>=0; i--){
		DaoToken *tok = self->errors->items.pToken[i];
		if( i < self->errors->size-1 ){
			DaoToken *tok2 = self->errors->items.pToken[i+1];
			if( tok->line == tok2->line && tok->name == tok2->name ){
				if( DString_EQ( tok->string, tok2->string ) ) continue;
			}
		}
		DaoStream_WriteMBS( stream, "  At line " );
		DaoStream_WriteInt( stream, tok->line );
		DaoStream_WriteMBS( stream, " : " );
		DaoStream_WriteMBS( stream, getCtInfo( tok->name ) );
		if( tok->string && tok->string->size ){
			DaoStream_WriteMBS( stream, " --- \" " );
			DaoStream_WriteString( stream, tok->string );
			DaoStream_WriteMBS( stream, " \"" );
		}
		DaoStream_WriteMBS( stream, ";\n" );
	}
	DArray_Clear( self->errors );
}
static void DaoParser_StatementError( DaoParser *self, DaoParser *parser, int code )
{
	DaoInode *inode = parser->vmcLast;
	while( inode != parser->vmcBase ){
		int end = inode->first + inode->last;
		if( inode->line != self->curLine ){
			DaoToken *last;
			DaoTokens_Append( self->errors, code, inode->line, "" );
			last = self->errors->items.pToken[ self->errors->size-1 ];
			self->error = code;
			DaoParser_SumTokens( parser, last->string, inode->first, end, 0 );
		}
		inode = inode->prev;
	}
}

#define MAX_INDENT_STEP 16
int DaoParser_LexCode( DaoParser *self, const char *src, int replace )
{
	DArray *indstack = NULL, *indents = NULL, *lines = NULL;
	DArray *tokens = self->tokens;
	DString *mbs = self->mbs;
	DaoToken  tok = { DTOK_SEMCO, DTOK_SEMCO, 0, 0, 0, NULL };
	DaoToken *t, *t2;
	int counts[MAX_INDENT_STEP];
	int i, j, k, last = 1;
	tok.string = mbs;
	self->lineCount = DaoToken_Tokenize( self->tokens, src, replace, 1, 0 );
	if( self->lineCount ==0 ) return 0;
	if( daoConfig.chindent ){
		indents = DArray_New(0);
		lines = DArray_New(0);
		indstack = DArray_New(0);
		DArray_Append( indstack, 1 );
		DArray_Append( indstack, 1 );
	}
	memset( counts, 0, MAX_INDENT_STEP*sizeof(int) );
	for(i=0; i<tokens->size; i++ ){
		t = tokens->items.pToken[i];
		if( daoConfig.chindent && t->name != DTOK_COMMENT ){
			if( i ==0 || t->line != tokens->items.pToken[i-1]->line ){
				while( t->cpos < DArray_TopInt( indstack ) ) DArray_PopBack( indstack );
				if( t->cpos > DArray_TopInt( indstack ) ) DArray_Append( indstack, (size_t)t->cpos );
				k = t->cpos - indstack->items.pInt[ indstack->size - 2 ];
				if( k >= 16 ) k = 16;
				if( k >0 ) counts[k-1] += 1;
				if( k >0 ){
					DArray_Append( indents, k );
					DArray_Append( lines, t->line );
				}
			}
		}
		if( t->name == DKEY_RAISE || t->name == DKEY_RETURN ){
			if( i+1 >= tokens->size || tokens->items.pToken[i+1]->line > t->line ){
				tok.line = t->line;
				DString_SetMBS( mbs, ";" );
				DArray_Insert( tokens, & tok, i+1 );
			}
		}else if( (t->name == DTOK_MBS || t->name == DTOK_WCS) && i+1<tokens->size ){
			t2 = tokens->items.pToken[i+1];
			if( t->name == t2->name ){
				int len = t->string->size;
				DString_Erase( t->string, len-1, MAXSIZE );
				DString_AppendMBS( t->string, t2->string->mbs + 1 );
				DArray_Erase( tokens, i+1, 1 );
				i --;
			}
		}else if( t->name == DTOK_COMMENT ){
			MAP_Insert( self->comments, t->line, t->string );
			DArray_Erase( tokens, i, 1 );
			i --;
		}
	}
	if( daoConfig.chindent ){
		k = 0;
		for(j=1; j<=MAX_INDENT_STEP; j++){
			/*
			   printf( "j = %i:  %3i\n", j, counts[j-1] );
			 */
			if( counts[j-1] >= k ){
				k = counts[j-1];
				self->indent = j;
			}
		}
		for(i=0; i<indents->size; i++){
			if( indents->items.pInt[i] % self->indent ){
				printf( "Warning: improper indentation of %i spaces at line: %i.\n",
						(int)indents->items.pInt[i], (int)lines->items.pInt[i] );
			}
		}
		DArray_Delete( indstack );
		DArray_Delete( indents );
		DArray_Delete( lines );
	}

	/*
XXX: comment the following.
printf( "indent = %i\n", self->indent );
printf( "tokens : %5i\n", self->tokens->size );
for(i=0; i<tokens->size; i++) printf("%3i : %3i,  %5i  %3i,  %s\n", i, tokens->items.pToken[i]->name, tokens->items.pToken[i]->line, tokens->items.pToken[i]->cpos, DString_GetMBS( tokens->items.pToken[i]->string ) );
	 */
	return 1;
}

int DaoParser_FindOpenToken( DaoParser *self, uchar_t tok, int start, int end/*=-1*/, int warn/*=1*/ )
{
	int i, n1, n2, n3, n4;
	DaoToken **tokens = self->tokens->items.pToken;

	if( start < 0 ) goto ErrorTokenExpect;
	if( end == -1 || end >= self->tokens->size ) end = self->tokens->size-1;

	n1 = n2 = n3 = n4 = 0;
	for( i=start; i<=end; i++){
		uchar_t tki = tokens[i]->name;
		if( ! ( n1 | n2 | n3 | n4 ) && tki == tok ){
			return i;
		}else if( n1 <0 || n2 <0 || n3 <0 || n4 <0 ){
			goto ErrorTokenExpect;
		}else{
			switch( tki ){
			case DTOK_LCB : n1 ++; break;
			case DTOK_RCB : n1 --; break;
			case DTOK_LB  : n2 ++; break;
			case DTOK_RB  : n2 --; break;
			case DTOK_LSB : n3 ++; break;
			case DTOK_RSB : n3 --; break;
			case DTOK_LT : if( self->pairLtGt ) n4 ++; break;
			case DTOK_GT : if( self->pairLtGt ) n4 --; break;
			}
		}
	}
ErrorTokenExpect:
	DString_SetMBS( self->mbs, DaoToken_NameToString( tok ) );
	if( warn ) DaoParser_Error( self, DAO_TOKEN_NOT_FOUND, self->mbs );
	return -10000;
}
int DaoParser_FindPairToken( DaoParser *self,  uchar_t lw, uchar_t rw, int start, int stop/*=-1*/ )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int k = 0;
	int i = start;
	int found = 0;
	uchar_t tk;

	if( start <0 ) goto ErrorUnPaired;
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
ErrorUnPaired:
	if( self->vmSpace ){
		DString_SetMBS( self->mbs, DaoToken_NameToString( lw ) );
		if( k ==0 ){
			DaoParser_Error( self, DAO_TOKEN_NOT_FOUND, self->mbs );
		}else{
			DString_AppendChar( self->mbs, ' ' );
			DString_AppendMBS( self->mbs, DaoToken_NameToString( rw ) );
			DaoParser_Error( self, DAO_TOKENS_NOT_PAIRED, self->mbs );
		}
	}
	return -100;
}
static int DaoParser_FindPairToken2( DaoParser *self,  uchar_t l, uchar_t r, int m, int n )
{
	DaoToken **tokens = self->tokens->items.pToken;
	if( tokens[m]->name != l ){
		DaoParser_Error( self, DAO_TOKEN_NOT_FOUND, self->mbs );
		return -1;
	}
	return DaoParser_FindPairToken( self, l, r, m, n );
}

int DaoParser_FindPhraseEnd( DaoParser *self, int start, int end )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int i = start;
	unsigned char tk, tkp, prev = 0;
	const int size = self->tokens->size;
	if( end < 0 ) end = size-1;

	if( i >= end ) return end;
	if( tokens[i]->name == DKEY_RETURN ) i ++;
	if( i >0 ) prev = tokens[i-1]->name;

	while( i <= end ){
		tk = tokens[i]->name;
		tkp = tokens[i]->type;
		if( tk == DTOK_DOT || tk == DTOK_COLON2 ){
			i += 1; /* thread.my[], obj.skip(). */
		}else if( tk == DKEY_AND || tk == DKEY_OR || tk == DKEY_NOT || tk == DKEY_IN ){
			i ++;
		}else if( tk == DTOK_ARROW ){
			i ++;
			if( tokens[i]->name == DTOK_PIPE ){
				i ++;
				while( tokens[i]->type == DTOK_IDENTIFIER ){
					i ++;
					if( tokens[i]->name == DTOK_PIPE ) break;
					if( tokens[i]->name == DTOK_COMMA ) i ++;
				}
				if( tokens[i]->name == DTOK_PIPE ) i++;
			}
		}else if( tk == DTOK_SEMCO || tk == DTOK_AT2 ){
			return i - 1;
		}else if( tk == DTOK_LCB ){
			int rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, i, -1 );
			if( rb < 0 ) return -100;
			if( rb >= end ) return rb;
			tk = tokens[rb+1]->name;
			if( tk == DTOK_COMMA ){
				switch( prev ){
				case DKEY_RETURN : case DKEY_YIELD :
				case DKEY_RAISE : case DKEY_RESCUE : case DKEY_CATCH :
					i = rb + 1; break;
				default : break;
				}
			}
			if( i == rb + 1 ) continue;
			if( tk != DTOK_DOT && tk != DTOK_ARROW ) return rb;
			i = rb + 1;
		}else if( tk == DTOK_LB ){
			int rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, i, -1 );
			if( rb < 0 ) return -100;
			if( rb >= end ) return end;
			/* type casting:
			 * (int)a
			 * (array<float>)[1,2,3];
			 */
			i = rb;
			if( i+1 <= end && tokens[i]->line == tokens[i+1]->line ) i ++;
		}else if( tk == DTOK_LSB ){
			int rb = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, i, -1 );
			if( rb < 0 ) return -100;
			i = rb;
		}else if( tk == DTOK_RB || tk == DTOK_RSB ){
			if( i+1 > end ) break;
			tk = tokens[i+1]->type;
			if( tk == DTOK_DOT ){
				i ++;
			}else if( tk >= DTOK_IDENTIFIER && tk <= DTOK_WCS ){
				return i;
			}else{
				i ++;
			}
		}else if( tk == DTOK_RCB ){
			if( i+1 > end ){
				if( start < i ) return i-1;
				return end;
			}
			return i-1;
		}else if( tkp >= DTOK_IDENTIFIER && tkp <= DTOK_WCS ){
			int old = tk;
			int old2 = tkp;
			/* two consecutive valid literals mark a phrase ending. */
			if( i+1 > end ) break;
			tk = tokens[i+1]->name;
			tkp = tokens[i+1]->type;
			if( old >= DKEY_ENUM && old <= DKEY_LIST && tkp == DTOK_LT ){
				int rb = DaoParser_FindPairToken( self, DTOK_LT, DTOK_GT, i, end );
				if( rb < 0 ) return -1;
				i = rb + 1;
			}else if( tkp == DTOK_DOT ){
				i ++;
			}else if( tk == DKEY_AND || tk == DKEY_OR || tk == DKEY_NOT || tk == DKEY_IN ){
				i ++;
			}else if( tkp >= DTOK_IDENTIFIER && tkp <= DTOK_WCS ){
				return i;
			}else if( tkp == DTOK_LB && tokens[i+1]->line > tokens[i]->line ){
				/* a, b
				 * (a,b) = ....
				 */
				return i;
			}else if( old > DTOK_IDENTIFIER && tkp == DTOK_LCB ){
				return i;
			}else{
				i ++;
			}
		}else if( tk == DTOK_INCR || tk == DTOK_DECR ){
			if( i+1 > end ) break;
			if( tokens[i]->line != tokens[i+1]->line ) return i;
			i ++;
		}else{
			i ++;
		}
	}
	return end;
}

static void DaoParser_ExtractComments( DaoParser *self, DString *docString,
		int lnstart, int lnend )
{
	DNode *node;
	size_t j;

	node = MAP_FindML( self->comments, lnstart );
	if( node == NULL ) node = DMap_First( self->comments );

	for(; node!=NULL; node = DMap_Next(self->comments, node) ){
		DString *s = node->value.pString;
		if( node->key.pInt < lnstart ) continue;
		if( node->key.pInt > lnend ) break;
		if( s->mbs[1] == '{' ){
			DString_AppendDataMBS( docString, s->mbs+2, s->size-4 );
		}else{
			DString_AppendMBS( docString, s->mbs+1 );
		}
	}
	j = DString_FindMBS( docString, "%P", 0 );
	while( j != MAXSIZE ){
		DString_Replace( docString, self->nameSpace->file, j, 2 );
		j = DString_FindMBS( docString, "%P", 0 );
	}
}
static void DaoTokens_AppendInitSuper( DArray *self, DaoClass *klass, int line, int flags )
{
	DString *info;
	int i;
	for(i=0; i<klass->superAlias->size; i++){
		DString *sup = klass->superAlias->items.pString[i];
		DaoCData *cdata = (DaoCData*) klass->superClass->items.pBase[i];
		if( flags & (1<<i) ) continue;
		if( cdata->type == DAO_CDATA ){
			DRoutine *func = (DRoutine*) DaoFindFunction( cdata->typer, sup );
			if( func ) func = DRoutine_GetOverLoad( func, NULL, NULL, 0, DVM_CALL );
			if( func ) goto AppendInitSuper;
			info = DaoTokens_AddRaiseStatement( self, "Error", "", line );
			DString_SetMBS( info, "'No default constructor for parent type \"" );
			DString_Append( info, sup );
			DString_AppendMBS( info, "\"'" );
			continue;
		}
AppendInitSuper:
		/* need to be compiled into DVM_GETCK, for proper class instantiation: */
		DaoTokens_Append( self, DTOK_IDENTIFIER, line, sup->mbs );
		DaoTokens_Append( self, DTOK_LB, line, "(" );
		DaoTokens_Append( self, DTOK_RB, line, ")" );
		DaoTokens_Append( self, DTOK_IDENTIFIER, line, "init" );
		DaoTokens_Append( self, DTOK_SEMCO, line, ";" );
	}
}
static int DaoParser_FindScopedData( DaoParser *self, int start, DValue *scope,
		DValue *nested, int local, DString *fullname );
static int DaoParser_ParseInitSuper( DaoParser *self, DaoParser *module, int start )
{
	DaoRoutine *routine = module->routine;
	DaoClass *klass = module->hostClass;
	DaoToken **tokens = self->tokens->items.pToken;
	DString *name = NULL;
	DArray *init = NULL;
	size_t i, size = self->tokens->size;
	int isconstru = klass && DString_EQ( routine->routName, klass->className );
	int line = 0, flags = 0; /* XXX number of super classes */
	int dlm = start;
	int rb = 0;
	if( isconstru == 0 ) return start;
	name = DString_New(1);
	init = DArray_New( D_TOKEN );
	if( tokens[start]->name == DTOK_COLON ){
		do {
			DValue scope = daoNullValue;
			DValue value = daoNullValue;
			int found = -1;
			int pos = DaoParser_FindScopedData( self, dlm+1, & scope, & value, 0, name );
			if( pos <0 || tokens[pos+1]->type != DTOK_LB ) goto ErrorRoutine;
			if( value.t != DAO_CLASS && value.t != DAO_CDATA ) goto ErrorRoutine;
			for(i=0; i<klass->superClass->size; i++){
				if( value.v.p == klass->superClass->items.pBase[i] ){
					found = i;
					break;
				}
			}
			if( found <0 ) goto ErrorRoutine;
			flags |= 1<<found;
			line = tokens[dlm]->line;
			DaoTokens_Append( init, DKEY_SELF, line, "self" );
			DaoTokens_Append( init, DTOK_DOT, line, "." );
			rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, dlm, -1 );
			if( rb <0 ) goto ErrorRoutine;
			DArray_Append( init, tokens[pos] );
			DString_Assign( init->items.pToken[ init->size-1 ]->string, name );
			for(i=pos+1; i<=rb; i++) DArray_Append( init, tokens[i] );
			DaoTokens_Append( init, DTOK_IDENTIFIER, line, "init" );
			DaoTokens_Append( init, DTOK_SEMCO, line, ";" );
			dlm = rb + 1;
		} while( dlm < size && tokens[dlm]->name == DTOK_COMMA );
		start = dlm;
		if( tokens[start]->name != DTOK_LCB ) goto ErrorRoutine;
	}
	if( tokens[start]->name == DTOK_LCB ){
		for(i=0; i<init->size; i++) DArray_Append( module->tokens, init->items.pToken[i] );
		DaoTokens_AppendInitSuper( module->tokens, klass, line, flags );
	}
	if( name ) DString_Delete( name );
	if( init ) DArray_Delete( init );
	return start;
ErrorRoutine:
	if( name ) DString_Delete( name );
	if( init ) DArray_Delete( init );
	return -1;
}
int DaoParser_ParsePrototype( DaoParser *self, DaoParser *module, int key, int start )
{
	DaoRoutine *routine = module->routine;
	DaoClass *klass = module->hostClass;
	DaoToken **tokens = self->tokens->items.pToken;
	DArray *init = NULL;
	int isconstru = klass && DString_EQ( routine->routName, klass->className );
	int i, right, size = self->tokens->size;
	int line = 0; /* XXX number of super classes */
	int e1=start, e2=size-1, ec = 0;

	DString_Assign( module->routName, tokens[start]->string  );
	DString_Assign( routine->routName, tokens[start]->string );
	DString_Assign( module->fileName, self->fileName );
	GC_ShiftRC( self->nameSpace, routine->nameSpace );
	module->nameSpace = self->nameSpace;
	routine->nameSpace = self->nameSpace;
	if( start + 2 >= size ) return -1;
	start ++;
	if( key == DKEY_OPERATOR ){
		int lb = 0;
		if( tokens[start-1]->name == DTOK_LB ){
			if( tokens[start]->name != DTOK_RB ) goto ErrorUnsupportedOperator;
			lb = DaoParser_FindOpenToken( self, DTOK_LB, start+1, -1, 1 );
		}else if( tokens[start-1]->name == DTOK_LSB ){
			if( tokens[start]->name != DTOK_RSB ) goto ErrorUnsupportedOperator;
			lb = DaoParser_FindOpenToken( self, DTOK_LB, start+1, -1, 1 );
		}else{
			lb = DaoParser_FindOpenToken( self, DTOK_LB, start, -1, 1 );
		}
		if( lb <0 ) goto ErrorUnsupportedOperator;
		for(i=start; i<lb; i++) DString_Append( routine->routName, tokens[i]->string );
		DString_Assign( module->routName, routine->routName );
		start = lb;
	}

	if( tokens[start]->name != DTOK_LB ) return -1;
	right = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
	if( right < 0 ) return -1;
	if( module->partoks->size ==0 )
		for( i=start; i<=right; i++ ) DArray_Append( module->partoks, tokens[i] );
	module->parStart = start;
	module->parEnd = right;
	if( right+1 >= size ) return right;
	if( tokens[right+1]->name == DKEY_CONST ){
		routine->attribs |= DAO_ROUT_ISCONST;
		right ++;
	}
	module->parEnd = right;
	e1 = e2 = right + 1;
	if( right+1 >= size ) return right;
	if( tokens[right+1]->name == DTOK_FIELD ){
		if( isconstru ) goto ErrorConstructorReturn; /* class constructor should not return a value */
		right ++;
		DArray_Append( module->partoks, tokens[right] );
		e2 = right + 1;
		if( right+1 >= size || tokens[right+1]->name == DTOK_LCB ) goto ErrorNeedReturnType;
		right ++;
		DArray_Append( module->partoks, tokens[right] );
		while( right+2 < size && tokens[right+1]->name == DTOK_COLON2 ){ /* scoping */
			e2 = right + 2;
			if( tokens[right+2]->type != DTOK_IDENTIFIER ) goto ErrorInvalidTypeForm;
			DArray_Append( module->partoks, tokens[right+1] );
			DArray_Append( module->partoks, tokens[right+2] );
			right += 2;
		}
		if( tokens[right]->type != DTOK_IDENTIFIER ) goto ErrorInvalidTypeForm;
		if( right+1 < size && tokens[right+1]->name == DTOK_LT ){
			start = right + 1;
			right = DaoParser_FindPairToken( self, DTOK_LT, DTOK_GT, right, -1 );
			for( i=start; i<=right; i++ ) DArray_Append( module->partoks, tokens[i] );
		}
	}
	module->parEnd = right;
	if( routine->type == DAO_FUNCTION ) return right;

	if( right+1 >= size ) return right;
	if( isconstru ){
		right = DaoParser_ParseInitSuper( self, module, right + 1 );
		if( right <0 ) return -1;
		right --;
	}else if( tokens[right+1]->name == DTOK_COLON ){
		goto ErrorRoutine;
	}
	if( tokens[right+1]->name != DTOK_LCB ) return right;

	DString_Clear( self->mbs );
	DaoParser_ExtractComments( self, self->mbs, tokens[start]->line, tokens[right+1]->line );
	if( self->mbs->size ){
		if( routine->routHelp == NULL ) routine->routHelp = DString_New(1);
		DString_Assign( routine->routHelp, self->mbs );
	}
	start = right;

	e2 = start + 1;
	if( tokens[start+1]->name == DTOK_LCB ){
		right = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, right, -1 );
		if(right<0) goto ErrorRoutine;

		DArray_Append( routine->nameSpace->definedRoutines, routine );
		routine->bodyStart = tokens[start+1]->line;
		routine->bodyEnd = tokens[right]->line;
		for(i=start+2; i<right; i++ ) DArray_Append( module->tokens, tokens[i] );
		DaoTokens_Append( module->tokens, DTOK_SEMCO, line, ";" );
		module->defined = 1;
	}
	if( init ) DArray_Delete( init );
	return right;
ErrorUnsupportedOperator: ec = DAO_ROUT_INVALID_OPERATOR; goto ErrorRoutine;
ErrorConstructorReturn: ec = DAO_ROUT_CONSTRU_RETURN; goto ErrorRoutine;
ErrorNeedReturnType:  ec = DAO_ROUT_NEED_RETURN_TYPE; goto ErrorRoutine;
ErrorInvalidTypeForm: ec = DAO_INVALID_TYPE_FORM; goto ErrorRoutine;
ErrorRoutine:
	if( ec ){
		if( e2 >= size ) e2 = size - 1;
		DString_Clear( self->mbs );
		for(i=e1; i<=e2; i++){
			DString_Append( self->mbs, tokens[i]->string );
			if( self->mbs->size > 20 ) break;
		}
		DaoParser_Error( self, ec, self->mbs );
	}
	if( init ) DArray_Delete( init );
	return -1;
}

static DaoType* DaoType_FindType( DString *name, DaoNameSpace *ns, DaoClass *klass, DaoRoutine *rout )
{
	DNode *node = NULL;
	if( rout && rout->type == DAO_ROUTINE && rout->minimal ==0 )
		node = MAP_Find( rout->abstypes, name );
	if( node == NULL && klass ) node = MAP_Find( klass->abstypes, name );
	if( node ) return node->value.pType;
	if( ns ) return DaoNameSpace_FindType( ns, name );
	return NULL;
}
static DaoType* DaoParser_ParseUserType( DaoParser *self, int start, int end, int *newpos )
{
	DaoNameSpace *ns = self->nameSpace;
	DaoType *type = NULL;
	DValue scope = daoNullValue;
	DValue value = daoNullValue;
	int k = DaoParser_FindScopedData( self, start, &scope, &value, 0, NULL );
	if( k <0 && value.t == 0 ) return NULL;
	*newpos = k + 1;
	switch( value.t ){
	case DAO_CLASS : type = value.v.klass->objType; break;
	case DAO_TYPE  : type = (DaoType*) value.v.p; break;
	case DAO_INTERFACE : type = value.v.inter->abtype; break;
	default : type = DaoNameSpace_GetTypeV( ns, value );
	}
	return type;
}
static DaoType* DaoParser_ParsePlainType( DaoParser *self, int start, int end, int *newpos )
{
	DaoType *type = NULL;
	DaoCData *cdata = & cptrCData;
	DaoNameSpace *ns = self->nameSpace;
	DaoClass *klass = self->hostClass;
	DaoRoutine *routine = self->routine;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoToken *token = tokens[start];
	DString *name = token->string;
	int i = token->name > DKEY_USE ? dao_keywords[ token->name - DKEY_USE ].value : 0;

	if( end > start && token->name == DTOK_IDENTIFIER ){
		type = DaoParser_ParseUserType( self, start, end, newpos );
		if( type ) return type;
	}

	*newpos = start + 1;
	type = DaoType_FindType( name, ns, klass, routine );
	if( type ) return type;
	if( i > 0 && i < 100 ){
		DaoBase *pbasic = token->name == DKEY_CDATA ? (DaoBase*) cdata : NULL;
		type = DaoNameSpace_MakeType( ns, name->mbs, i, pbasic, 0,0 );
	}else if( token->name == DTOK_ID_INITYPE ){
		type = DaoNameSpace_MakeType( ns, name->mbs, DAO_INITYPE, 0,0,0 );
	}else if( token->name == DTOK_QUES ){
		type = DaoNameSpace_MakeType( ns, "?", DAO_UDF, 0,0,0 );
	}else if( token->name == DTOK_DOTS ){
		type = DaoNameSpace_MakeType( ns, "...", DAO_UDF, 0,0,0 );
	}else{
		/* scoped type or user defined template class */
		type = DaoParser_ParseUserType( self, start, end, newpos );
	}
	return type;
InvalidTypeName:
	return NULL;
}
static DaoType* DaoParser_ParseType( DaoParser *self, int start, int end, int *newpos, DArray *types );
static DaoType* DaoParser_ParseTypeItems( DaoParser *self, int start, int end, DArray *types )
{
	DaoNameSpace *ns = self->nameSpace;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoType *type = NULL;
	DString *name;
	int tid, t, i = start;
	while( i <= end ){
		tid = 0;
		name = NULL;
		t = (i+1 <= end) ? tokens[i+1]->type : 0;
		if( i == start && tokens[i]->type == DTOK_FIELD ) goto ReturnType;
		if( tokens[i]->type != DTOK_IDENTIFIER ) goto InvalidTypeForm;
		if( t == DTOK_COLON || t == DTOK_ASSN ){
			name = tokens[i]->string;
			tid = (t == DTOK_COLON) ? DAO_PAR_NAMED : DAO_PAR_DEFAULT;
			if( i + 2 > end ) goto InvalidTypeForm;
			i = i + 2;
		}
		type = DaoParser_ParseType( self, i, end, & i, types );
		if( type == NULL ) return NULL;
		if( name ) type = DaoNameSpace_MakeType( ns, name->mbs, tid, (DaoBase*)type, NULL,0 );
		DArray_Append( types, type );
		if( i > end ) break;
ReturnType:
		if( tokens[i]->type == DTOK_FIELD ){
			if( i+1 > end ) goto InvalidTypeForm;
			type = DaoParser_ParseType( self, i+1, end, & i, types );
			if( type == NULL ) return NULL;
			if( i <= end ) goto InvalidTypeForm;
			return type;
		}else if( tokens[i]->type != DTOK_COMMA ){
			goto InvalidTypeForm;
		}
		i += 1;
	}
	return NULL;
InvalidTypeForm:
	DaoTokens_Append( self->errors, DAO_INVALID_TYPE_FORM, tokens[i]->line, tokens[i]->string->mbs );
	return NULL;
}
static DaoType* DaoParser_ParseEnumTypeItems( DaoParser *self, int start, int end )
{
	DaoType *type;
	DaoToken *tok;
	DaoToken **tokens = self->tokens->items.pToken;
	DString *field = NULL;
	uchar_t sep = 0;
	dint value = 0;
	int k, set=0, sign = 1;
	char c;

	type = DaoType_New( "enum<", DAO_ENUM, NULL, NULL );
	type->mapNames = DMap_New(D_STRING,0);
	for(k=start; k<=end; k++){
		tok = tokens[k];
		field = tok->string;
		c = tok->string->mbs[0];
		sign = 1;
		if( tok->type != DTOK_IDENTIFIER ) break;
		if( tok->name == DTOK_ID_INITYPE || tok->name == DTOK_ID_SYMBOL ) break;
		if( k+1 <= end && tokens[k+1]->type == DTOK_ASSN ){
			k += 1;
			if( k+1 <= end ){
				c = tokens[k+1]->type;
				if( c == DTOK_ADD || c == DTOK_SUB ){
					k += 1;
					if( c == DTOK_SUB ) sign = -1;
				}
			}
			if( k+1 > end ) break;
			c = tokens[k+1]->type;
			if( c >= DTOK_DIGITS_HEX && c <= DTOK_NUMBER_HEX ){
				k += 1;
				set = 1;
				value = strtoll( tokens[k]->string->mbs, 0, 0 );
			}else break;
		}
		if( sep ==0 && (k+1) <= end ){
			sep = tokens[k+1]->type;
			if( sep != DTOK_COMMA && sep != DTOK_SEMCO ) break;
			if( sep == DTOK_SEMCO && set == 0 ) value = 1;
		}
		if( DMap_Find( type->mapNames, field ) ) goto WrongForm;
		if( sign < 0 ) value = - value;
		DMap_Insert( type->mapNames, field, (void*)value );
		if( sep == DTOK_SEMCO ){
			value <<= 1;
		}else{
			value += 1;
		}
		if( k+1 > end ) break;
		k += 1;
		tok = tokens[k];
		if( tok->type != DTOK_COMMA && tok->type != DTOK_SEMCO ) break;
		if( sep != tok->type ) break;
	}
	if( sep == DTOK_SEMCO ) type->flagtype = 1;
	if( k < end ) goto WrongForm;
	for(k=start; k<=end; k++) DString_Append( type->name, tokens[k]->string );
	DString_AppendChar( type->name, '>' );
	/*
	   printf( "%i  %i  %s\n", end, i, type->name->mbs );
	 */
	return type;
WrongForm:
	DaoTokens_Append( self->errors, DAO_INVALID_TYPE_FORM, tokens[k]->line, tokens[k]->string->mbs );
	return type;
}
static DaoType* DaoParser_ParseType( DaoParser *self, int start, int end, int *newpos, DArray *types )
{
	DaoType *type = NULL;
	DaoBase *retype = NULL;
	DaoType **nested = NULL;
	DaoNameSpace *ns = self->nameSpace;
	DaoClass *klass = self->hostClass;
	DaoRoutine *routine = self->routine;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoToken *tok = tokens[start];
	DValue scope = daoNullValue;
	DValue value = daoNullValue;
	DString *tks = tok->string;
	DArray *old = types;
	int i, k, t = tokens[start]->name;
	int gt, tid, count, count2;

#if 0
	for(i=start; i<=end; i++) printf("%s  ", tokens[i]->string->mbs); printf("\n\n");
#endif

	*newpos = start + 1;
	if( start == end || t == DTOK_QUES || t == DTOK_DOTS || (t == DTOK_ID_INITYPE ) ){
		return DaoParser_ParsePlainType( self, start, end, newpos );
	}
	if( types == NULL ) types = DArray_New(0);
	count = types->size;
	if( tokens[start]->type != DTOK_IDENTIFIER ) goto InvalidTypeName;
	if( tokens[start]->name == DTOK_IDENTIFIER && strcmp( tokens[start]->string->mbs, "future" ) !=0 ){
		/* scoped type or user defined template class */
		type = DaoParser_ParseUserType( self, start, end, newpos );
		if( type == NULL ) goto InvalidTypeName;
	}else if( tokens[start]->name == DKEY_ENUM && tokens[start+1]->name == DTOK_LT ){
		gt = DaoParser_FindPairToken( self, DTOK_LT, DTOK_GT, start, end );
		if( gt < 0 ) goto InvalidTypeForm;
		*newpos = gt + 1;
		type = DaoParser_ParseEnumTypeItems( self, start+2, gt-1 );
	}else if( tokens[start+1]->name == DTOK_LT ){
		gt = DaoParser_FindPairToken( self, DTOK_LT, DTOK_GT, start, end );
		if( gt < 0 ) goto InvalidTypeForm;
		*newpos = gt + 1;
		type = DaoParser_ParseTypeItems( self, start+2, gt-1, types );
		if( self->errors->size ) goto InvalidTypeForm;
		if( type && tokens[start]->name != DKEY_ROUTINE ) goto InvalidTypeForm;
		count2 = types->size - count;
		retype = NULL;
		tid = 0;
		switch( tokens[start]->name ){
		case DKEY_ARRAY :
			tid = DAO_ARRAY;
			if( count2 != 1 ) goto InvalidTypeForm;
			type = types->items.pType[ count ];
			if( type->tid > DAO_COMPLEX && type->tid != DAO_INITYPE && type->tid != DAO_ANY )
				goto InvalidTypeForm;
			break;
		case DKEY_LIST :
			tid = DAO_LIST;
			if( count2 != 1 ) goto InvalidTypeForm;
			break;
		case DKEY_MAP  :
			tid = DAO_MAP;
			if( count2 != 2 ) goto InvalidTypeForm;
			break;
		case DKEY_TUPLE :
			tid = DAO_TUPLE;
			break;
		case DKEY_ROUTINE :
			tid = DAO_ROUTINE;
			if( type == NULL ) type = DaoType_New( "?", DAO_UDF, NULL, NULL );
			retype = (DaoBase*) type;
			break;
		default : 
			if( strcmp( tokens[start]->string->mbs, "future" ) ==0 ) tid = DAO_FUTURE;
			break;
		}
		tks = tokens[start]->string;
		nested = types->items.pType + count;
		type = DaoNameSpace_MakeType( ns, tks->mbs, tid, retype, nested, count2 );
		if( type == NULL ) goto InvalidTypeForm;
		for(i=0; i<count2; i++){
			GC_IncRC( nested[i] );
			GC_DecRC( nested[i] );
		}
		DArray_Erase( types, count, count2 );
		GC_IncRC( retype );
		GC_DecRC( retype );
	}else if( tokens[start]->type == DTOK_IDENTIFIER ){
		type = DaoParser_ParsePlainType( self, start, end, newpos );
		if( type == NULL ) goto InvalidTypeName;
	}else{
		goto InvalidTypeForm;
	}
#if 0
	printf( "%s %i\n", type->name->mbs, *newpos );
#endif
	GC_IncRCs( types );
	GC_DecRCs( types );
	if( old == NULL ) DArray_Delete( types );
	return type;
InvalidTypeName:
InvalidTypeForm:
InvalidType:
	GC_IncRCs( types );
	GC_DecRCs( types );
	if( old == NULL ) DArray_Delete( types );
	DaoTokens_Append( self->errors, DAO_INVALID_TYPE_FORM, tokens[start]->line, tks->mbs );
	return NULL;
}

int DaoParser_ParseRoutine( DaoParser *self );
static int DaoParser_ParseLoadStatement( DaoParser *self, int start, int end );

static int DaoParser_GetRegister( DaoParser *self, DaoToken *name );
static DValue DaoParser_GetVariable( DaoParser *self, int reg );

static DaoClass* DaoParse_InstantiateClass( DaoParser *self, DaoClass *klass, int start, int end )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoRoutine *rout = self->routine;
	DaoNameSpace *ns = self->nameSpace;
	DaoClass *cls = self->hostClass;
	DaoType *cd = self->hostCData;
	DaoType *type;
	DArray *types = DArray_New(0);
	DArray *holders = klass->typeHolders;
	DNode *node;
	int i = start;
	if( klass->typeHolders == NULL ) goto FailedInstantiation;
	while( i <= end ){
		type = DaoParser_ParseType( self, i, end, &i, NULL );
		if( type == NULL ) goto FailedInstantiation;
		DArray_Append( types, type );
		//printf( "%s\n", type->name->mbs );
		if( i <= end && tokens[i]->type != DTOK_COMMA ){
			DaoParser_Error( self, DAO_TOKEN_NOT_EXPECTED, tokens[i]->string );
			goto FailedInstantiation;
		}
		i += 1;
	}
	cls = DaoClass_Instantiate( klass, types );
	GC_IncRCs( types );
	GC_DecRCs( types );
	DArray_Delete( types );
	return cls;
FailedInstantiation:
	GC_IncRCs( types );
	GC_DecRCs( types );
	DArray_Delete( types );
	return NULL;
}

static int DaoParser_FindScopedData( DaoParser *self, int start, DValue *scope,
		DValue *nested, int local, DString *fullname )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoNameSpace *myNS = self->nameSpace;
	DString *name = tokens[start]->string;
	DValue str = daoNullString;
	DValue res = daoNullValue;
	int i = DaoParser_GetRegister( self, tokens[start] );
	int st = LOOKUP_ST( i );
	int N = self->tokens->size;
	int j, lb = 0;
	res = daoNullValue;
	str.v.s = name;
	*scope = res;
	*nested = str;
	if( fullname ) DString_Assign( fullname, name );
	if( st > DAO_LOCAL_CONSTANT && self->routine != myNS->mainRoutine
			&& tokens[start+1]->name !=  DTOK_COLON2 ){
		/* local class */
		if( local ) return start;
	}
	/* lowest bit is set to 1 for constant */
	if( st & 1 ) res = DaoParser_GetVariable( self, i );
	*scope = res;
	if( res.t ==0 ){
		if( self->hostClass ){
			i = DaoClass_FindConst( self->hostClass, name );
			if( i >=0 ) res = DaoClass_GetConst( self->hostClass, i );
		}
		if( res.t ==0 && self->hostCData ){
			res = DaoFindValueOnly( self->hostCData->typer, name );
		}
		if( res.t ==0 ) res = DaoNameSpace_GetData( myNS, name );
	}
	if( res.t == DAO_CLASS && start+1 < N && tokens[start+1]->name ==  DTOK_LT ){
		if( res.v.klass->typeHolders ){
			i = DaoParser_FindPairToken( self, DTOK_LT, DTOK_GT, start, N );
			if( i >=0 ){
				res.v.klass = DaoParse_InstantiateClass( self, res.v.klass, start+2, i-1 );
				if( res.v.klass ){
					if( fullname ) DString_Assign( fullname, res.v.klass->objType->name );
					start = i;
				}
			}
		}
	}
	while( res.t && start+1 < N && tokens[start+1]->name ==  DTOK_COLON2 ){
		start += 2;
		if( start >= N ){
			start = N-1;
			return -1;
		}
		*scope = res;
		name = tokens[start]->string;
		if( tokens[start]->type != DTOK_IDENTIFIER ){
			if( start+2 >= N ) return -1;
			name = self->mbs;
			if( tokens[start+1]->name == DTOK_RB ){
				lb = DaoParser_FindOpenToken( self, DTOK_LB, start+2, -1, 1 );
			}else{
				lb = DaoParser_FindOpenToken( self, DTOK_LB, start+1, -1, 1 );
			}
			DString_Clear( name );
			for(j=start; j<lb; j++) DString_Append( name, tokens[j]->string );
		}
		if( fullname ){
			DString_Append( fullname, tokens[start-1]->string );
			DString_Append( fullname, name );
		}
		str.v.s = name;
		if( res.t == DAO_NAMESPACE ){
			res = DaoNameSpace_GetData( res.v.ns, name );
		}else if( res.t == DAO_CLASS ){
			i = DaoClass_FindConst( res.v.klass, name );
			res = (i >=0) ? DaoClass_GetConst( res.v.klass, i ) : daoNullValue;
		}else if( res.t == DAO_CDATA ){
			res = DaoFindValueOnly( res.v.cdata->typer, name );
		}else{
			res = daoNullValue;
			break;
		}
		if( tokens[start]->type != DTOK_IDENTIFIER ) return start;
		if( res.t == DAO_CLASS && start+1 < N && tokens[start+1]->name ==  DTOK_LT ){
			if( res.v.klass->typeHolders ){
				i = DaoParser_FindPairToken( self, DTOK_LT, DTOK_GT, start, N );
				if( i >=0 ){
					res.v.klass = DaoParse_InstantiateClass( self, res.v.klass, start+2, i-1 );
					if( res.v.klass ){
						if( fullname ) DString_Assign( fullname, res.v.klass->objType->name );
						start = i;
					}
				}
			}
		}
	}
	if( start+1 < N && tokens[start+1]->name ==  DTOK_COLON2 ){
		DaoParser_Error( self, DAO_SYMBOL_POSSIBLY_UNDEFINED, tokens[start]->string );
		return -1;
	}
	*nested = res;
	if( res.t ==0 ) *nested = str;
	return start;
}

static int DaoParser_Preprocess( DaoParser *self );
int DaoParser_ParseScript( DaoParser *self )
{
	DaoNameSpace *ns = self->nameSpace;
	DaoVmSpace   *vmSpace = self->vmSpace;
	DValue value = daoNullRoutine;
	int bl;
	DaoRoutine *routMain = self->routine; /* could be set in DaoVmSpace_Eval() */

	self->error = 0;
	if( routMain == NULL ) routMain = DaoRoutine_New();

	/*
	   printf("DaoParser_ParseScript() ns=%p, rout=%p, %s\n", ns, routMain, self->fileName->mbs );
	 */

	value.v.p = (DaoBase*) routMain;
	if( routMain->routType == NULL ){
		routMain->routType = dao_routine;
		GC_IncRC( dao_routine );
	}
	routMain->attribs |= DAO_ROUT_MAIN;
	ns->mainRoutine = routMain;
	DaoNameSpace_SetConst( ns, DVR_NSC_MAIN, value );
	DString_SetMBS( self->routName, "::main" );
	GC_IncRC( routMain );
	DArray_Append( ns->mainRoutines, routMain );
	/* the name of routMain will be set in DaoParser_ParseRoutine() */

	routMain->bodyStart = 1;
	routMain->bodyEnd = self->lineCount;
	self->routine = routMain;
	self->vmSpace = vmSpace;
	self->nameSpace = ns;
	GC_ShiftRC( ns, routMain->nameSpace );
	routMain->nameSpace = ns;

	bl = DaoParser_Preprocess( self );
	if( bl ==0 ) DaoParser_PrintError( self, 0, 0, NULL );
	return bl && DaoParser_ParseRoutine( self );
}

static void DaoVmCode_Set( DaoVmCodeX *self, ushort_t code, ushort_t a, ushort_t b,
		ushort_t c, uchar_t lev, int line, int first, int mid, int last )
{
	if( mid >= first ) mid -= first;
	if( last >= first ) last -= first;
	self->code = code;
	self->a = a;
	self->b = b;
	self->c = c;
	self->level = lev;
	self->line = line;
	self->first = first;
	self->middle = mid;
	self->last = last;
}
static void DaoParser_AddCode( DaoParser *self, ushort_t code,
		ushort_t a, ushort_t b, ushort_t c, int first, int mid, int last )
{
	int line = 0;
	if( first < self->tokens->size ) line = self->tokens->items.pToken[first]->line;
	else if( self->tokens->size ) line = self->tokens->items.pToken[self->tokens->size-1]->line;
	if( self->vmcLast->line != line && (self->vmSpace->options & DAO_EXEC_IDE) )
		DaoParser_AddCode2( self, DVM_NOP, 0,0,0, first, mid, last );
	DaoParser_AddCode2( self, code, a, b, c, first, mid, last );
}
static void DaoParser_AddCode2( DaoParser *self, ushort_t code,
		ushort_t a, ushort_t b, ushort_t c, int first, int mid, int last )
{
	DaoInode *node = DaoInode_New( self );
	DaoInode *top = self->vmcTop;
	DaoInode *it = top;
	DaoInode *it2, *aux, *step = NULL;
	DRoutine *routine = (DRoutine*) self->routine;
	DValue *cst = routine->routConsts->data;
	DValue key = daoNullValue;
	DNode *iter;
	DMap *map;
	dint i, min, max, count, tc = top->code;
	int line = 0;
	if( first < self->tokens->size ) line = self->tokens->items.pToken[first]->line;
	else if( self->tokens->size ) line = self->tokens->items.pToken[self->tokens->size-1]->line;

	if( mid >= first ) mid -= first;
	if( last >= first ) last -= first;

	node->code = code;
	node->a = a;
	node->b = b;
	node->c = c;
	node->line = line;
	node->first = first;
	node->middle = mid;
	node->last = last;
	node->level = self->lexLevel;
	node->prev = self->vmcLast;
	self->vmcLast->next = node;
	self->vmcLast = node;
	self->vmcCount ++;

#if 0
	printf( "stack:\n" );
	it2 = top;
	while( it2 ){
		DaoInode_Print( it2 );
		it2 = it2->below;
	}
	DaoParser_PrintCodes( self );
#endif

	switch( code ){
	case DVM_FOR : case DVM_SWITCH : case DVM_DO :
	case DVM_IF : case DVM_TRY :
		node->below = top, self->vmcTop = node;
		break;
	case DVM_ELIF :
		if( tc != DVM_IF && tc != DVM_ELIF ) goto ErrorSyntax;
		node->below = top, self->vmcTop = node;
		break;
	case DVM_ELSE :
		if( tc != DVM_IF && tc != DVM_ELIF ) goto ErrorSyntax;
		if( tc == DVM_ELSE ) goto ErrorSyntax;
		node->below = top, self->vmcTop = node;
		break;
	case DVM_WHILE :
		node->below = top, self->vmcTop = node;
		while( it->code != DVM_WHILE_AUX ) it = it->below;
		node->jumpTrue = it;
		break;
	case DVM_CASETAG :
		node->below = top, self->vmcTop = node;
		while( it && it->code != DVM_SWITCH ) it = it->below;
		/* in IDE mode, DVM_NOP could be added in between DVM_SWITCH and DVM_LBRA */
		/* if( it == NULL || it->next->code != DVM_LBRA ) goto ErrorSyntax; */
		node->jumpTrue = it;

		map = self->switchMaps->items.pMap[ it->b ];
		key = DaoParser_GetVariable( self, LOOKUP_BIND_LC( node->a ) );
		DMap_Insert( map, & key, node );
		break;
	case DVM_DEFAULT :
		node->below = top, self->vmcTop = node;
		while( it && it->code != DVM_SWITCH ) it = it->below;
		/* if( it == NULL || it->next->code != DVM_LBRA ) goto ErrorSyntax; */
		node->jumpTrue = it;
		it->jumpTrue = node; /* default */
		break;
	case DVM_BREAK :
	case DVM_SKIP :
		while( it != self->vmcBase ){
			if( it->code == DVM_DO || it->code == DVM_FOR
					|| it->code == DVM_WHILE || it->code == DVM_SWITCH ){
				node->jumpTrue = it;
				break;
			}
			it = it->below;
		}
		if( node->jumpTrue == NULL ) goto ErrorSyntax;
		break;
	case DVM_WHILE_AUX :
	case DVM_FOR_AUX :
	case DVM_FOR_STEP :
	case DVM_LBRA :
	case DVM_LBRA2 :
	case DVM_SCBEGIN :
		node->below = top, self->vmcTop = node;
		break;
	case DVM_UNTIL :
		node->jumpTrue = self->vmcTop;
		self->vmcTop->jumpFalse = node;
		self->vmcTop = self->vmcTop->below;
		break;
	case DVM_RBRA2 :
		while( top->code != DVM_LBRA2 ) top = top->below;
		self->vmcTop = top->below;
		break;
	case DVM_SCEND :
		while( top->code != DVM_SCBEGIN || top->a != node->a ) top = top->below;
		top->jumpTrue = node;
		break;
	case DVM_RBRA :
		node->c = code; /* required by DaoRoutine_OptimizeCSE() */
		while( top->code != DVM_LBRA ) top = top->below;
		if( top->code != DVM_LBRA ) goto ErrorSyntax;
		top->jumpTrue = node;
		node->jumpTrue = top;
		self->vmcTop = it = top->below;
		switch( it->code ){ /* for break/skip */
		case DVM_FOR : case DVM_WHILE : case DVM_SWITCH :
			self->vmcTop = it->below; break;
		default : break;
		}

		if( it->code == DVM_FOR ){
			aux = it;
			while( aux->code != DVM_FOR_AUX ){
				if( aux->code == DVM_FOR_STEP ) step = aux;
				aux = aux->below;
			}
			if( step ){
				it2 = it->prev;
				it->prev = step->prev;
				step->prev->next = it;
				it2->next = node;
				node->prev->next = step;
				step->prev = node->prev;
				node->prev = it2;
				aux->jumpTrue = step;
			}else{
				aux->jumpTrue = aux;
			}
			it->jumpTrue = aux;
		}else if( it->code == DVM_SWITCH ){
			int direct = 0;
			min = max = 0;
			count = 0;
			map = self->switchMaps->items.pMap[ it->b ];
			for(iter=DMap_First(map); iter !=NULL; iter=DMap_Next(map, iter) ){
				key = cst[iter->value.pInode->a];
				if( key.t == DAO_INTEGER ){
					if( count == 0 ) min = max = key.v.i;
					if( min > key.v.i ) min = key.v.i;
					if( max < key.v.i ) max = key.v.i;
					count ++;
				}
			}
			if( count == map->size && count > 0.75 * (max - min) ){
				for(i=min+1; i<max; i++){
					key.v.i = i;
					if( DMap_Find( map, &key ) ==NULL ) DMap_Insert( map, &key, NULL );
				}
				direct = 1;
			}
			it->c = map->size;
			aux = it;
			for(iter=DMap_First(map); iter !=NULL; iter=DMap_Next(map, iter) ){
				it2 = DaoInode_New( self );
				it2->code = DVM_CASE;
				if( iter->value.pInode ){
					it2->a = iter->value.pInode->a;
					it2->jumpTrue = iter->value.pInode;
					it2->first = iter->value.pInode->first;
					it2->middle = iter->value.pInode->middle;
					it2->last = iter->value.pInode->last;
				}else{
					it2->a = DRoutine_AddConstValue( routine, *iter->key.pValue );
					it2->jumpFalse = node;
				}
				/* mark integer jump table */
				if( aux == it ) it2->c = direct ? DAO_CASE_TABLE : DAO_CASE_ORDERED;
				it2->prev = aux;
				aux->next = it2;
				it2->next = top;
				top->prev = it2;
				aux = it2;
			}
			cst = routine->routConsts->data;
		}
		switch( it->code ){
		case DVM_DO :
			it->jumpTrue = node;
			node->jumpTrue = it;
			break;
		case DVM_FOR :
			it->jumpFalse = node;
			node->jumpTrue = it;
			while( it->code != DVM_FOR_AUX ) it = it->below;
			break;
		case DVM_WHILE :
			it->jumpFalse = node;
			node->jumpTrue = it;
			while( it->code != DVM_WHILE_AUX ) it = it->below;
			break;
		case DVM_SWITCH :
			it->jumpFalse = node;
			node->jumpTrue = it;
			if( it->jumpTrue == NULL ) it->jumpTrue = node; /* default */
			break;
		case DVM_IF : case DVM_ELIF : case DVM_ELSE :
			it->jumpFalse = node;
			while( it->code ==DVM_IF || it->code ==DVM_ELIF || it->code ==DVM_ELSE ){
				it->jumpTrue = node;
				if( it->code == DVM_IF ) break;
				it = it->below;
			}
			break;
		case DVM_TRY : case DVM_RESCUE :
			it->jumpFalse = node;
			while( it->code ==DVM_TRY || it->code ==DVM_RESCUE ){
				it->jumpTrue = node;
				if( it->code == DVM_TRY ) break;
				it = it->below;
			}
			break;
		default : node->jumpTrue = node; break;
		}
		break;
	case DVM_RETRY :
	case DVM_RAISE :
		while( it && it->code != DVM_TRY ) it = it->below;
		if( code == DVM_RETRY && it == NULL ) goto ErrorSyntax;
		node->jumpTrue = it;
		break;
	case DVM_RESCUE :
		if( tc != DVM_TRY && tc != DVM_RESCUE ) goto ErrorSyntax;
		node->below = top, self->vmcTop = node;
		break;
	default : break;
	}
	return;
ErrorSyntax:
	DString_SetMBS( self->mbs, getOpcodeName( code ) );
	DaoParser_Error( self, DAO_STATEMENT_OUT_OF_CONTEXT, self->mbs );
	return;
}
static void DaoParser_AddNullCode( DaoParser *self, int first, int last )
{
	DaoInode *node = DaoInode_New( self );
	int line = 0;
	if( first < self->tokens->size ) line = self->tokens->items.pToken[first]->line;
	else if( self->tokens->size ) line = self->tokens->items.pToken[self->tokens->size-1]->line;

	if( last >= first ) last -= first;
	node->code = DVM_UNUSED;
	node->a = node->b = node->c = 0;
	node->line = line;
	node->first = first;
	node->middle = 0;
	node->last = last;
	node->level = 0;
	node->prev = self->vmcLast;
	self->vmcLast->next = node;
	self->vmcLast = node;
	self->vmcCount ++;
}

static void DaoParser_DeclareVariable( DaoParser *self, DaoToken *tok, int vt, DaoType *tp );
static int DaoParser_MakeWhileLogic( DaoParser *self, ushort_t opcode, int start );
static int DaoParser_MakeForLoop( DaoParser *self, int start, int end );

static int DaoParser_PostParsing( DaoParser *self );

static int  DaoParser_MakeArithTree( DaoParser *self, int start, int end,
		int *cst, int regFix/*=-1*/, int state );
static int DaoParser_MakeArithArray( DaoParser *self, int left, int right, int *N, int *cst, uchar_t sep1, uchar_t sep2/*=0*/, DArray *cid, int state );

void DaoType_MapNames( DaoType *self );

DaoType* DaoParser_ParseTypeName( const char *name, DaoNameSpace *ns, DaoClass *cls, DaoRoutine *rout )
{
	DArray *tokens = DArray_New( D_TOKEN );
	DaoParser *parser = DaoParser_New();
	DaoType *type = NULL;
	int i = 0;
	if( ! DaoToken_Tokenize( parser->tokens, name, 1, 0, 0 ) ) goto ErrorType;
	parser->nameSpace = ns;
	parser->hostClass = cls;
	parser->routine = rout;
	parser->vmSpace = ns->vmSpace;
	type = DaoParser_ParseType( parser, 0, tokens->size-1, &i, NULL );
	if( i < tokens->size && type ){
		if( type->refCount == 0 ) DaoType_Delete( type );
		type = NULL;
	}
	DArray_Delete( tokens );
	DaoParser_Delete( parser );
	return type;
ErrorType:
	DArray_Delete( tokens );
	DaoParser_Delete( parser );
	return NULL;
}
static void DaoParser_PushRegister( DaoParser *self )
{
	int line;
	self->locRegCount ++;
	if( self->routine == NULL || self->routine->type != DAO_ROUTINE ) return;
	if( self->routine->minimal ==1 ) return;
	line = self->curLine - self->routine->bodyStart - 1;
	DArray_Append( self->regLines, line );
}
static void DaoParser_PushRegisters( DaoParser *self, int n )
{
	int i, line;
	if( n <0 ) return;
	self->locRegCount += n;
	if( self->routine == NULL || self->routine->type != DAO_ROUTINE ) return;
	if( self->routine->minimal ==1 ) return;
	line = self->curLine - self->routine->bodyStart - 1;
	for(i=0; i<n; i++) DArray_Append( self->regLines, line );
}
static void DaoParser_PopRegister( DaoParser *self )
{
	self->locRegCount --;
	DArray_Pop( self->regLines );
}
static void DaoParser_PopRegisters( DaoParser *self, int n )
{
	int i;
	if( n <0 ) return;
	for(i=0; i<n; i++) DArray_Pop( self->regLines );
	self->locRegCount -= n;
}
int DaoParser_ParseParams( DaoParser *self )
{
	DaoToken **tokens = self->partoks->items.pToken;
	DaoParser *defparser = self->defParser;
	DaoNameSpace *myNS = self->nameSpace;
	DaoInterface *inter = self->hostInter;
	DaoRoutine *routine = self->routine;
	DaoClass  *klass = self->hostClass;
	DaoType  *cdata = self->hostCData;
	DaoType  *abstype = NULL, *abtp, *tp;
	DArray  *nested = DArray_New(0);
	DValue  dft = daoNullValue;
	DString *pname = DString_New(1);
	DString *mbs = DString_New(1);
	DString *tok;
	DNode  *node;
	DArray *tokArray;
	DMap *initTypes = self->initTypes;
	const char *hostname = NULL;
	int start = 0, end = self->partoks->size - 1;
	int isMeth, notStatic, notConstr;
	int i, j, k, rb, m1=0, m2=end;
	int ec = 0, line = tokens[0]->line;
	int hasdeft = 0;
	unsigned char tki;

	self->error = 0;
	self->initTypes = NULL;
	if( routine->routType ) goto Finalizing;
	if( self->defParser && self->parStart < self->parEnd ){
		tokens = defparser->tokens->items.pToken;
		start = self->parStart;
		end = self->parEnd;
		m2 = end;
	}else{
		defparser = self;
		DArray_Swap( self->partoks, self->tokens );
	}
	tokArray = self->tokens;

	/*
	   printf("routine proto, size: %i ; %i\n", self->tokens->size, self->tokens->size );
	   for(i=start; i<=end; i++) printf("%s  ", tokens[i]->string->mbs); printf("\n\n");
	 */
	if( start >= end ) goto ErrorParamParsing;
	if( tokens[start]->name != DTOK_LB ) goto ErrorParamParsing;
	rb = DaoParser_FindPairToken( defparser, DTOK_LB, DTOK_RB, start, end );
	if( rb < 0 ) goto ErrorParamParsing;

	for(i=start; i<=rb; i++) DString_Append( routine->parCodes, tokens[i]->string );

	if( inter ) hostname = inter->abtype->name->mbs;
	else if( klass ) hostname = klass->className->mbs;
	else if( cdata ) hostname = cdata->typer->name;

	if( routine->routName->mbs[0] == '@' ) DString_AppendChar( pname, '@' );
	DString_AppendMBS( pname, "routine<" );
	i = start + 1;
	tki = tokens[i]->name;
	routine->parCount = 0;
	if( tki == DKEY_SELF ) routine->attribs |= DAO_ROUT_PARSELF;

	isMeth = klass && routine != klass->classRoutine;
	notStatic = (routine->attribs & DAO_ROUT_STATIC) ==0;
	notConstr = hostname && strcmp( routine->routName->mbs, hostname ) != 0;
	notConstr &= strcmp( routine->routName->mbs, "@class" ) != 0;
	if( (isMeth || inter) && tki != DKEY_SELF && notStatic && notConstr ){
		DaoType *hostype = self->hostType;
		DaoToken *tk;
		DString_SetMBS( mbs, "self" );
		MAP_Insert( DArray_Top( self->localVarMap ), mbs, self->locRegCount );
		if( routine->type == DAO_ROUTINE && routine->minimal ==0 ){
			DArray_Append( routine->defLocals, tokens[i] );
			tk = (DaoToken*) DArray_Back( routine->defLocals );
			DaoToken_Set( tk, 1, 0, routine->parCount, "self" );
		}
		dft = daoNullValue;
		dft.ndef = 1;

		if( self->outParser && self->outParser->isClassBody ){
			hostype = DaoNameSpace_MakeType( myNS, "@class", DAO_INITYPE, NULL,NULL,0 );
		}

		abstype = DaoType_New( "self", DAO_PAR_NAMED, (DaoBase*)hostype, NULL );
		DArray_Append( nested, (void*) abstype );
		DRoutine_AddConstValue( (DRoutine*) routine, dft );
		DString_AppendMBS( pname, abstype->name->mbs );
		if( routine->type == DAO_ROUTINE && routine->minimal ==0 )
			MAP_Insert( self->routine->localVarType, self->locRegCount, abstype );
		DaoParser_PushRegister( self );
		routine->parCount ++;
	}
	while( i < rb ){
		int comma;
		int regCount = self->locRegCount;

		m1 = i;
		m2 = rb;
		self->curLine = defparser->curLine = tokens[i]->line;
		tki = tokens[i]->name;
		tok = tokens[i]->string;
		if( tokens[i]->type == DTOK_IDENTIFIER ){
			/*
			   printf( "name = %s; regid = %i\n", tokens[i]->string->mbs, self->locRegCount );
			 */
			if( routine->parCount && tokens[i-1]->type == DTOK_IDENTIFIER ) goto ErrorNeedSeparator;

			MAP_Insert( DArray_Top( self->localVarMap ), tok, self->locRegCount );
			if( routine->type == DAO_ROUTINE && routine->minimal ==0 ){
				DaoToken *tk;
				DArray_Append( routine->defLocals, tokens[i] );
				tk = (DaoToken*) DArray_Back( routine->defLocals );
				DaoToken_Set( tk, 1, 0, routine->parCount, NULL );
			}
			DaoParser_PushRegister( self );
			routine->parCount ++;
		}
		/*
		   printf( "%s\n", tokens[i]->string->mbs );
		 */

		j = i;
		dft = daoNullValue;
		dft.ndef = 1;
		abtp = abstype = NULL;
		if( tki == DTOK_DOTS ){
			routine->parCount = DAO_MAX_PARAM;
			self->locRegCount = DAO_MAX_PARAM;
			DArray_Resize( self->regLines, DAO_MAX_PARAM, (void*)(size_t)-1 );
			abstype = DaoNameSpace_MakeType( myNS, "...", DAO_PAR_VALIST, 0,0,0 );
			m1 = i;  m2 = rb;
			if( i+1 != rb ) goto ErrorMiddleValist;
		}else if( tki == DTOK_ID_INITYPE ){
			abstype = DaoNameSpace_MakeType( myNS, tok->mbs, DAO_INITYPE, 0,0,0 );
			abstype = DaoNameSpace_GetType( myNS, (DaoBase*) abstype );
		}else if( i+1<rb && (tokens[i+1]->name == DTOK_COLON
					|| tokens[i+1]->name == DTOK_ASSN
					|| tokens[i+1]->name == DTOK_CASSN)){
			i ++;
			if( tokens[i]->name == DTOK_COLON ){
				if( tokens[i+1]->name == DKEY_CONST ){
					if( routine->type == DAO_ROUTINE ) routine->constParam |= 1<<(routine->parCount-1);
					i ++;
				}
				if( i+1 >= rb || tokens[i+1]->type != DTOK_IDENTIFIER ) goto ErrorNeedType;
				abstype = DaoParser_ParseType( defparser, i+1, rb-1, &i, NULL );
				if( abstype == NULL ) goto ErrorParamParsing;
			}
			if( tokens[i]->name == DTOK_CASSN ){
				if( abstype ) goto ErrorRedundantType;
				abstype = DaoNameSpace_MakeType( myNS, "any", DAO_ANY, 0,0,0 );
			}
			if( tokens[i]->name == DTOK_ASSN || tokens[i]->name == DTOK_CASSN ){
				int reg=1, cst = 0;
				hasdeft = i;
				if( i+1 >= rb || tokens[i+1]->name == DTOK_COMMA ) goto ErrorNeedDefault;
				comma = DaoParser_FindOpenToken( defparser, DTOK_COMMA, i, -1, 0 );
				if( comma < 0 ) comma = rb;
				m1 = i + 1;
				m2 = comma - 1;
				if( abstype && abstype->tid == DAO_CDATA ){
					/* par : UserData = 0 */
					DString *zero = tokens[i+1]->string;
					if( i+1 == comma-1 && zero->size ==1 && zero->mbs[0] =='0' ){
						/* generate a constant of null UserData pointer */
						dft.t = DAO_CDATA;
						dft.v.cdata = DaoCData_New( abstype->typer, NULL );
						dft.v.cdata->attribs = 0;
						abtp = abstype;
						cst = 1;
					}
				}
#if 0
				printf( "cst = %i;  reg = %i, %s\n", cst, reg, abstype?abstype->name->mbs:"" );
				for(j=i+1; j<comma; j++) printf( "%s\n", tokens[j]->string->mbs );
#endif
				/* QWidget( parent : QWidget=0, f : int=0 )=>QWidget */
				DArray_PushFront( defparser->enumTypes, abstype );
				if( ! cst ) reg = DaoParser_MakeArithTree( defparser, i+1, comma-1, & cst, -1, 0 );
				DArray_PopFront( defparser->enumTypes );
				if( reg < 0 ) goto ErrorInvalidDefault;
				if( cst ){
					if( cst > 1 ){
						dft = DaoParser_GetVariable( defparser, cst );
						abtp = DaoNameSpace_GetTypeV( myNS, dft );
					}
				}else if( self->uplocs ){
					int loc = routine->routConsts->size;
					DArray_Append( self->uplocs, reg );
					DArray_Append( self->uplocs, loc );
					DArray_Append( self->uplocs, i+1 );
					DArray_Append( self->uplocs, comma-1 );
					abtp = abstype = DaoType_New( "?", DAO_UDF, 0,0 );
				}else{
					goto ErrorVariableDefault;
				}
				if( abtp == NULL ) goto ErrorInvalidDefault;
				if( abstype && DaoType_MatchValue( abstype, dft, NULL ) ==0 )
					goto ErrorImproperDefault;
				if( abstype == NULL ) abstype = abtp;
				i = comma;
			}
		}else if( tokens[i]->type == DTOK_IDENTIFIER ){
			abstype = DaoType_New( "?", DAO_UDF, 0,0 );
		}else if( tki == DTOK_COMMA ){
			i ++;
			continue;
		}else{
			goto ErrorInvalidParam;
		}
		if( hasdeft && dft.t ==0 && dft.ndef && self->outParser == NULL ){
			m1 = hasdeft;  m2 = rb;
			goto ErrorInvalidDefault;
		}
		i ++;

		if( routine->type == DAO_ROUTINE && routine->minimal ==0 )
			MAP_Insert( self->routine->localVarType, regCount, abstype );
		if( abstype->tid != DAO_PAR_VALIST ){
			m2 = abtp ? DAO_PAR_DEFAULT : DAO_PAR_NAMED;
			abstype = DaoType_New( tok->mbs, m2, (DaoBase*) abstype, NULL );
		}
		/* e.g.: spawn( pid :string, src :string, timeout=-1, ... ) */
		DArray_Append( nested, (void*) abstype );
		DRoutine_AddConstValue( (DRoutine*) routine, dft );
		k = pname->size >0 ? pname->mbs[pname->size-1] : 0;
		if( k !='<' && k != '(' ) DString_AppendMBS( pname, "," );
		DString_AppendMBS( pname, abstype->name->mbs );
	}
	i = rb + 1;
	if( i <= end && tokens[i]->name == DKEY_CONST ){
		routine->attribs |= DAO_ROUT_ISCONST;
		i += 1;
	}
	abstype = NULL;
	if( i <= end ){
		m1 = i;
		m2 = end;
		if( tokens[i]->name != DTOK_FIELD || i+1 > end ) goto ErrorInvalidReturn;
		abstype = DaoParser_ParseType( defparser, i+1, end, &i, NULL );
		if( abstype == NULL || i < end ) goto ErrorInvalidReturn;
		DString_AppendMBS( pname, "=>" );
		DString_Append( pname, abstype->name );
	}else if( klass && routine->routHost == klass->objType && DString_EQ( routine->routName, klass->className ) ){
		abstype = klass->objType;
		DString_AppendMBS( pname, "=>" );
		DString_Append( pname, abstype->name );
	}else{
		abstype = DaoType_New( "?", DAO_UDF, 0,0 );
		DString_AppendMBS( pname, "=>?" );
	}
	DString_AppendMBS( pname, ">" );
	GC_IncRCs( nested );
	GC_IncRC( abstype );
	tp = DaoType_New( pname->mbs, DAO_ROUTINE, (DaoBase*) abstype, nested );
	routine->routType = DaoNameSpace_FindType( self->nameSpace, pname );
	if( DaoType_MatchTo( tp, routine->routType, NULL ) == DAO_MT_EQ ){
		DaoType_Delete( tp );
	}else{
		routine->routType = tp;
		DaoType_MapNames( routine->routType );
		DaoNameSpace_AddType( self->nameSpace, pname, routine->routType );
		DString_SetMBS( mbs, "self" );
		node = MAP_Find( routine->routType->mapNames, mbs );
		if( node && node->value.pInt == 0 ) routine->routType->attrib |= DAO_TYPE_SELF;
	}
	/* printf( "%i  %s\n", routine->parCount, routine->routType->name->mbs ); */
	/* for(j=0; j<nested->size; j++) printf( "%s\n", nested->items.pType[j]->name->mbs ); */
	if( routine->attribs & DAO_ROUT_PARSELF ) routine->routType->attrib |= DAO_ROUT_PARSELF;
	GC_IncRC( routine->routType );
	DArray_Append( nested, (void*) abstype );
	if( self == defparser ) DArray_Swap( self->partoks, self->tokens );
	/*  remove vmcode for consts */
	DaoParser_ClearCodes( self );
	/* one parse might be used to compile multiple C functions: */
	if( routine->type == DAO_FUNCTION ) DMap_Clear( self->allConsts );

	if( i < rb ) goto ErrorInvalidParam;
	if( routine->parCount > DAO_MAX_PARAM ) goto ErrorTooManyParams;

Finalizing:
	self->initTypes = initTypes;
	for(i=0; i<routine->routType->nested->size; i++){
		abtp = routine->routType->nested->items.pType[i];
		if( abtp ) DaoType_GetTypes( abtp, self->initTypes );
	}
	/*
	   for(i=0; i<nested->size; i++)
	   if( nested->items.pType[i] )
	   printf( "abtp: %i %s\n", nested->items.pType[i]->refCount,
	   nested->items.pType[i]->name->mbs );
	 */
	GC_DecRCs( nested );
	DArray_Delete( nested );
	DString_Delete( pname );
	DString_Delete( mbs );
	return 1;

ErrorTooManyParams:  ec = DAO_PARAM_TOO_MANY; goto ErrorParamParsing;
ErrorInvalidParam:   ec = DAO_PARAM_INVALID; goto ErrorParamParsing;
ErrorNeedSeparator:  ec = DAO_PARAM_NEED_SEPARATOR; goto ErrorParamParsing;
ErrorMiddleValist:   ec = DAO_PARAM_MIDDLE_VALIST; goto ErrorParamParsing;
ErrorRedundantType:  ec = DAO_PARAM_REDUNDANT_TYPE; goto ErrorParamParsing;
ErrorNeedType:       ec = DAO_PARAM_NEED_TYPE;    goto ErrorParamParsing;
ErrorNeedDefault:    ec = DAO_PARAM_NEED_DEFAULT; goto ErrorParamParsing;
ErrorInvalidDefault: ec = DAO_PARAM_INVALID_DEFAULT; goto ErrorParamParsing;
ErrorVariableDefault: ec = DAO_PARAM_VARIABLE_DEFAULT; goto ErrorParamParsing;
ErrorImproperDefault: ec = DAO_PARAM_IMPROPER_DEFAULT; goto ErrorParamParsing;
ErrorInvalidReturn:  ec = DAO_PARAM_INVALID_RETURN; goto ErrorParamParsing;
ErrorParamParsing:
	DString_Clear( mbs );
	for(k=m1; k<=m2; k++ ){
		if( k > m1 ) DString_AppendChar( mbs, ' ' );
		DString_Append( mbs, tokens[k]->string );
	}
	if( ec ) DaoParser_Error( self, ec, mbs );
	DaoParser_PrintError( self, routine->defLine, DAO_INVALID_PARAM_LIST, routine->routName );
	self->initTypes = initTypes;
	GC_IncRCs( nested );
	GC_DecRCs( nested );
	DArray_Delete( nested );
	DString_Delete( pname );
	DString_Delete( mbs );
	return 0;
}

static void DaoParser_AddScope( DaoParser *self, int type, int start )
{
	int mid = (self->tokens->items.pToken[start]->type != DTOK_LCB);
	if( mid ) mid += start;
	self->lexLevel ++;
	DArray_Append( self->scoping, type );
	DArray_Append( self->localVarMap, self->lvm );
	DArray_Append( self->localCstMap, self->lvm );
	if( type != DVM_UNUSED2 ) DaoParser_AddCode( self, DVM_LBRA, 0, 0, 0, start, mid, 0 );
}
static int DaoParser_DelScope( DaoParser *self, int type, int tokid )
{
	DaoToken *tok = self->tokens->items.pToken[tokid];
	int mid = (tok->type != DTOK_RCB);
	if( mid ) mid += tokid;
	self->lexLevel --;
	if( self->lexLevel < 0 || DArray_TopInt( self->scoping ) != type ){
		DaoParser_Error( self, DAO_TOKEN_NOT_EXPECTED, tok->string );
		return 0;
	}
	DArray_Pop( self->localVarMap );
	DArray_Pop( self->localCstMap );
	DArray_Pop( self->scoping );
	if( type != DVM_UNUSED2 ) DaoParser_AddCode( self, DVM_RBRA, 0,0,0, tokid, mid, 0 );
	return 1;
}
static int DaoParser_CompleteScope( DaoParser *self, int tokid )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int tc, tk = (tokid+1 < self->tokens->size) ? tokens[tokid+1]->name : 0;
	while( self->scoping->size >0 ){
		dint type = (dint)DArray_Top( self->scoping );
		if( type == DVM_UNUSED ){
			if( DaoParser_DelScope( self, DVM_UNUSED, tokid ) == 0 ) return 0;
		}else if( type == DVM_UNUSED2 ){
			if( DaoParser_DelScope( self, DVM_UNUSED2, tokid ) == 0 ) return 0;
		}else{
			return 1;
		}
		tc = self->vmcTop->code;
		if( tk == DKEY_ELIF && (tc == DVM_IF || tc ==DVM_ELIF) ) return 1;
		if( tk == DKEY_ELSE && (tc == DVM_IF || tc ==DVM_ELIF) ) return 1;
		if( tk == DKEY_RESCUE && (tc == DVM_TRY || tc == DVM_RESCUE) ) return 1;
	}
	return 1;
}

static int DaoParser_Preprocess( DaoParser *self )
{
	DaoVmSpace *vmSpace = self->vmSpace;
	DaoToken **tokens = self->tokens->items.pToken;
	DNode *node;

	int i, end, temp;
	int bropen1 = 0, bropen2 = 0, bropen3 = 0;
	int k, right, start = 0;
	int tag = 0;
	unsigned char tki;

	char buffer[512];

	/*
	   printf("routine = %p\n", self->routine );
	   for(i=0; i<self->tokens->size; i++) printf("%s  ", tokens[i]->string->mbs); printf("\n\n");
	 */

	while( start >=0 && start < self->tokens->size ){
		self->curLine = tokens[start]->line;
		/*
		   printf( "start = %i\n", start );
		   printf("At tokPos : %i, %s\n", tokPos[start], tokChr[ start ] );
		 */

		tki = tokens[start]->name;
		if( tki >= DTOK_LB && tki <= DTOK_RSB ){
			switch( tki ){
			case DTOK_LCB :  bropen1 ++;  break;
			case DTOK_RCB :  bropen1 --;  break;
			case DTOK_LB :   bropen2 ++;  break;
			case DTOK_RB :   bropen2 --;  break;
			case DTOK_LSB :  bropen3 ++;  break;
			case DTOK_RSB :  bropen3 --;  break;
			default : break;
			}
			start ++;
			continue;
		}else if( bropen1 ==0 && bropen2 ==0 && bropen3 ==0 ){
			if( tki == DTOK_AT2 ){
				int from = start;
				int cmd = start + 1;
				int rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
				if( tokens[start+2]->name != DTOK_LB || rb < 0 ){
					/*vmSpace->warning( "(...)", DAO_CTW_IS_EXPECTED ); */
					start ++;
					continue;
				}
				start += 3;
				if( TOKCMP( tokens[ cmd ], "PATH" )==0 ){
					int ptk = start;
					char *chs = tokens[ptk]->string->mbs;
					tki = tokens[start]->name;
					if( tki == DTOK_ADD || tki == DTOK_SUB ) ptk ++;
					chs = tokens[ptk]->string->mbs;
					if( chs[0] != '"' && chs[0] != '\'' ){
						DaoParser_Error( self, DAO_TOKEN_NEED_STRING, tokens[ptk]->string );
						DaoParser_Error( self, DAO_INVALID_PATH, tokens[cmd]->string );
						return 0;
					}
					strcpy( buffer, chs + 1 );
					buffer[ strlen(buffer)-1 ] = 0; /* " stripped; */
					if( tki == DTOK_ADD )
						DaoVmSpace_AddPath( vmSpace, buffer );
					else if( tki == DTOK_SUB )
						DaoVmSpace_DelPath( vmSpace, buffer );
					else
						DaoVmSpace_SetPath( vmSpace, buffer );

				}else if( TOKCMP( tokens[ cmd ], "LOCALE" ) ==0 ){
					int len;
					char *info;
					strcpy( buffer, tokens[start]->string->mbs );
					len = (int)strlen(buffer);
					if( buffer[len-1]=='\"' || buffer[len-1]=='\'' )buffer[ len-1 ] = 0;
					info = buffer;
					if( info[0]=='\"' || info[0] == '\'' ) info = info + 1;
					setlocale( LC_CTYPE, info );
				}else if( TOKCMP( tokens[ cmd ], "ON" )==0 ){
					temp = KeySeqSearch( mapCompOptKey, tokens[ cmd +2 ]->string );
					if( temp > 0 ){
						self->cmpOptions |= temp;
						vmSpace->options |= temp;
					}else{
						DaoStream_WriteMBS( vmSpace->stdStream, "unkown parameter\n" );
					}
					start = rb + 1;
				}else if( TOKCMP( tokens[ cmd ], "OFF" )==0 ){
					temp = KeySeqSearch( mapCompOptKey, tokens[ cmd +2 ]->string );
					if( temp > 0 ){
						self->cmpOptions = self->cmpOptions & ~ temp;
						vmSpace->options = vmSpace->options & ~ temp;
					}else{
						DaoStream_WriteMBS( vmSpace->stdStream, "unkown parameter\n" );
					}
				}else{
					from = rb+1;
				}
				DArray_Erase( self->tokens, from, rb-from+1 );
				tokens = self->tokens->items.pToken;
				start = from;
				continue;
			}else if( tki == DKEY_SYNTAX ){
#ifdef DAO_WITH_MACRO
				right = DaoParser_ParseMacro( self, start );
				/*
				   printf( "macro : %s %i\n", tokens[start+2]->string->mbs, right );
				 */
				if( right <0 ){
					DaoParser_Error3( self, DAO_INVALID_STATEMENT, start );
					return 0;
				}
				DArray_Erase( self->tokens, start, right-start+1 );
				tokens = self->tokens->items.pToken;
#else
				DaoStream_WriteMBS( vmSpace->stdStream, "macro is not enabled.\n" );
				return 0;
#endif
			}else if( tki == DKEY_LOAD && start+1<self->tokens->size
					&& tokens[start+1]->name != DTOK_LB ){
				/* only for top level "load", for macros in the module  */
				end = DaoParser_FindOpenToken( self, DTOK_SEMCO, start, -1, 1 );
				if( end < 0 ) return 0;
				if( ! DaoParser_ParseLoadStatement( self, start, end ) ) return 0;
				DArray_Erase( self->tokens, start, end-start+1 );
				tokens = self->tokens->items.pToken;
			}else{
				start ++;
			}
		}else{
			start ++;
		}
	}
	start = self->tokens->size-1;
#ifdef DAO_WITH_MACRO
	while( start >=0 && start < self->tokens->size ){
		self->curLine = tokens[start]->line;
		node = MAP_Find( self->nameSpace->macros, tokens[start]->string );
		if( node ){
			DaoMacro *macro = (DaoMacro*) node->value.pVoid;
			/*
			   printf( "macro %i %s\n", start, tokStr[start]->mbs );
			 */
			for( i=0; i<macro->macroList->size; i++){
				DaoMacro *m = (DaoMacro*) macro->macroList->items.pVoid[i];
				end = DaoParser_MacroTransform( self, m, start, tag );
				/*
				   printf( "overloaded: %i, %i\n", i, end );
				 */
				if( end >0 ){
					tag ++;
					tokens = self->tokens->items.pToken;
					node = NULL;
					for(k=0; k<m->keyListApply->size; k++){
						/* printf( "%i, %s\n", k, m->keyListApply->items.pString[k]->mbs ); */
						node = MAP_Find( self->nameSpace->macros, m->keyListApply->items.pString[k] );
						if( node ) break;
					}
					if( node ){
						/* printf( "found ====================\n" ); */
						start = end;
					}
					break;
				}
			}
		}
		start --;
	}
#endif
	/*
XXX: comment the following.
{
DArray *tokens = self->tokens;
printf( "tokens : %5i\n", self->tokens->size );
for(i=0; i<tokens->size; i++) printf("%3i : %3i,  %5i,%3i,  %s\n", i, tokens->items.pToken[i]->name, tokens->items.pToken[i]->line, tokens->items.pToken[i]->cpos, DString_GetMBS( tokens->items.pToken[i]->string ) );
for(i=0; i<tokens->size; i++) printf("%s  ", DString_GetMBS( tokens->items.pToken[i]->string ) );
}
	 */
return 1;
}
static void DaoParser_AddToScope( DaoParser *self, DValue scope,
		DString *name, DValue value, DaoType *abtype, int store, int line )
{
	DaoNameSpace *myNS = self->nameSpace;
	DaoRoutine *routine = self->routine;
	int perm = self->permission;
	if( scope.t == DAO_CLASS ){
		DaoClass_AddType( scope.v.klass, name, abtype );
		DaoClass_AddConst( scope.v.klass, name, value, perm, line );
	}else if( scope.t == DAO_NAMESPACE ){
		DaoNameSpace_AddType( scope.v.ns, name, abtype );
		DaoNameSpace_AddConst( scope.v.ns, name, value, perm );
	}else{
		if( routine == myNS->mainRoutine ){
			DaoNameSpace_AddConst( myNS, name, value, perm );
			DaoNameSpace_AddType( myNS, name, abtype );
		}else if( self->isClassBody && self->hostClass ){
			DaoClass_AddType( self->hostClass, name, abtype );
			DaoClass_AddConst( self->hostClass, name, value, perm, line );
		}
		GC_IncRC( abtype );
		MAP_Insert( routine->abstypes, name, abtype );
		MAP_Insert( DArray_Top( self->localCstMap ), name, routine->routConsts->size );
		DRoutine_AddConstValue( (DRoutine*)routine, value );
	}
}
static int DaoParser_UseConstructor( DaoParser *self, DRoutine *rout, int t1, int t2 )
{
	DaoClass *host = self->hostClass;
	DaoRoutine *classRoutine = host->classRoutine;
	DaoType *hostType = host->objType;
	DString *s1 = DString_Copy( rout->routType->name );
	DString *s2 = DString_New(1);
	int perm = self->permission;
	int i, k = DString_Find( s1, rout->routType->X.abtype->name, 0 );
	if( k != MAXSIZE ) DString_Erase( s1, k, -1 );
	for(i=0; i<classRoutine->routTable->size; i++){
		DaoRoutine *rt = classRoutine->routTable->items.pRout[i];
		DString_Assign( s2, rt->routType->name );
		k = DString_Find( s2, rt->routType->X.abtype->name, 0 );
		if( k != MAXSIZE ) DString_Erase( s2, k, -1 );
		if( DString_EQ( s1, s2 ) ){
			DaoParser_SumTokens( self, s2, t1, t2-1, 1 );
			DaoParser_Warn( self, DAO_INVALID_USE_STMT, s2 );
			DString_Assign( s1, rout->routType->name );
			if( rt->routHost == hostType ){
				DaoParser_Warn2( self, DAO_ROUT_DEFINED_SIGNATURE, s1 );
			}else{
				DaoParser_Warn2( self, DAO_ROUT_USED_SIGNATURE, s1 );
			}
			DString_Delete( s1 );
			DString_Delete( s2 );
			return 0;
		}
	}
	DRoutine_AddOverLoad( (DRoutine*)host->classRoutine, rout );
	DString_Assign( s1, host->className );
	DString_Append( s1, rout->parCodes );
	DaoClass_AddOvldRoutine( host, s1, (DaoRoutine*) rout );
	DString_Delete( s1 );
	DString_Delete( s2 );
	return 1;
}
static int DaoParser_ParseUseStatement( DaoParser *self, int start, int to )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoNameSpace *inNS = self->vmSpace->nsInternal;
	DaoNameSpace *myNS = self->nameSpace;
	DaoTypeBase *typer;
	DString *str;
	DValue value;
	int perm = self->permission;
	int use = start;
	start ++;
	if( start >to || tokens[start]->type != DTOK_IDENTIFIER ){
		DaoParser_Error( self, DAO_TOKEN_NEED_NAME, tokens[start]->string );
		DaoParser_Error2( self, DAO_INVALID_USE_STMT, use, start, 1 );
		return -1;
	}
	if( self->isClassBody ){
		DString *signature = self->mbs;
		DString *name = tokens[start]->string;
		DaoClass *klass = NULL, *host = self->hostClass;
		DaoCData *cdata = NULL;
		int i;
		for(i=0; i<host->superClass->size; i++){
			if( host->superClass->items.pBase[i]->type == DAO_CLASS ){
				klass = host->superClass->items.pClass[i];
				if( ! DString_EQ( klass->className, name ) ) klass = NULL;
				if( klass ) break;
			}else if( host->superClass->items.pBase[i]->type == DAO_CDATA ){
				cdata = host->superClass->items.pCData[i];
				if( strcmp( cdata->typer->name, name->mbs ) !=0 ) cdata = NULL;
				if( cdata ) break;
			}
		}
		DString_Clear( signature );
		if( start+1 <= to && tokens[start+1]->type == DTOK_LB ){
			int rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, to );
			if( rb <0 ) return -1;
			DString_Append( signature, name );
			for(i=start+1; i<=rb; i++) DString_Append( signature, tokens[i]->string );
			start = rb + 1;
		}
		if( klass ){
			if( signature->size ){
				DRoutine *rs = (DRoutine*)DaoClass_GetOvldRoutine( klass, signature );
				if( rs == NULL ){
					DaoParser_Error2( self, DAO_ROUT_WRONG_SIGNATURE, use+1, start-1, 1 );
					DaoParser_Error2( self, DAO_INVALID_USE_STMT, use, start-1, 1 );
					return -1;
				}
				DaoParser_UseConstructor( self, rs, use, start );
			}else{
				DArray *routTable = klass->classRoutine->routTable;
				for(i=0; i<routTable->size; i++){
					DRoutine *rs = (DRoutine*) routTable->items.pRout[i];
					DaoParser_UseConstructor( self, rs, use, start );
				}
			}
		}else if( cdata ){
			DaoFunction *func = DaoFindFunction( cdata->typer, name );
			if( func == NULL ){
				DaoParser_Error( self, DAO_CONSTR_NOT_DEFINED, name );
				DaoParser_Error2( self, DAO_INVALID_USE_STMT, use, start, 1 );
				return -1;
			}
			if( signature->size ) DString_Erase( signature, 0, name->size );
			for(i=0; i<func->routTable->size; i++){
				DRoutine *rs = (DRoutine*) func->routTable->items.pRout[i];
				if( signature->size ==0 || DString_EQ( signature, rs->parCodes ) ){
					/* printf( "%s\n", rs->parCodes->mbs ); */
					DaoParser_UseConstructor( self, rs, use, start );
					if( signature->size ) break;
				}
			}
		}else{
			DaoParser_Error( self, DAO_SYMBOL_NEED_CLASS_CTYPE, tokens[start]->string );
			DaoParser_Error2( self, DAO_INVALID_USE_STMT, use, start, 1 );
			return -1;
		}
		return start;
	}
	value = DaoNameSpace_GetData( myNS, tokens[start]->string );
	if( value.t ==0 ) value = DaoNameSpace_GetData( inNS, tokens[start]->string );
	if( value.t ==0 ){
		DaoParser_Error( self, DAO_SYMBOL_POSSIBLY_UNDEFINED, tokens[start]->string );
		DaoParser_Error( self, DAO_SYMBOL_NEED_CONSTANT, tokens[start]->string );
		DaoParser_Error2( self, DAO_INVALID_USE_STMT, use, start, 1 );
		return -1;
	}
	typer = DValue_GetTyper( value );
	start ++;
	if( start <= to && tokens[start]->name == DTOK_COLON ){
		int prev = tokens[start]->line;
		start ++;
		while( start <= to && tokens[start]->type == DTOK_IDENTIFIER ){
			str = tokens[start]->string;
			value = DaoFindValue( typer, str );
			if( value.t ==0 ){
				DaoParser_Error( self, DAO_SYMBOL_NOT_DEFINED, tokens[start]->string );
				DaoParser_Error2( self, DAO_INVALID_USE_STMT, use, start, 1 );
				return -1;
			}
			DaoNameSpace_AddConst( myNS, str, value, perm );
			start ++;
			if( start <= to && tokens[start]->name == DTOK_COMMA ) start ++;
			if( start > to || tokens[start]->name == DTOK_SEMCO ) break;
			if( tokens[start]->line > prev ) break;
		}
	}else{
		DMap *hash = typer->priv->methods;
		DNode *it;
		char *prev = "???";
		if( typer->priv->nspace && typer->priv->methods == NULL ){
			DaoNameSpace_SetupMethods( typer->priv->nspace, typer );
			hash = typer->priv->methods;
		}
		if( hash == NULL || hash->size ==0 ){
			DaoParser_Warn( self, DAO_NO_METHOD_TO_USE, tokens[start-1]->string );
			return start;
		}
		value.t = DAO_FUNCTION;
		for(it=DMap_First(hash); it; it=DMap_Next(hash,it)){
			value.v.func = (DaoFunction*) it->value.pVoid;
			if( STRCMP( value.v.func->routName, prev ) ==0 ) return start;
			prev = value.v.func->routName->mbs;
			DaoNameSpace_AddConst( myNS, value.v.func->routName, value, perm );
		}
		if( typer->priv->values )
			DaoNameSpace_SetupValues( typer->priv->nspace, typer );
		hash = typer->priv->values;
		for(it=DMap_First(hash); it; it=DMap_Next(hash,it)){
			DaoNameSpace_AddConst( myNS, it->key.pString, *it->value.pValue, perm );
		}
	}
	return start;
}
static DaoRoutine* DaoRoutine_GetDecorator( DaoRoutine *self, DaoRoutine *deco, DaoTuple *param )
{
	DaoRoutine *dc, *best = NULL;
	DaoType *ft, *tp, **types, **stypes = self->routType->nested->items.pType;
	DMap *mapNames, *mapids = DMap_New(0,D_STRING);
	DArray *nested;
	DNode *it, *node;
	int parpass[DAO_MAX_PARAM];
	float sum, sum2, max = 0;
	int i, j, k, match;
	for(i=0; i<deco->routTable->size; i++){
		dc = deco->routTable->items.pRout[i];
		nested = dc->routType->nested;
		types = nested->items.pType;
		if( param && param->items->size >= nested->size ) continue;
		ft = types[0];
		if( ft->tid != DAO_PAR_NAMED || ft->X.abtype->tid != DAO_ROUTINE ) continue;
		ft = ft->X.abtype;
		if( ft->nested->size > self->routType->nested->size ) continue;
		sum = sum2 = 0;
		mapNames = ft->mapNames;
		for(it=DMap_First(mapNames); it; it=DMap_Next(mapNames,it)){
			tp = ft->nested->items.pType[it->value.pInt];
			node = DMap_Find( self->routType->mapNames, it->key.pVoid );
			if( node == NULL ) goto NextDecorator;
			match = DaoType_MatchTo( tp, stypes[node->value.pInt], NULL );
			if( match ==0 ) goto NextDecorator;
			sum += match;
		}
		if( mapNames->size ==0 ) sum = 1;
		for(j=0; j<nested->size; j++) parpass[j] = 0;
		if( param ){
			DMap_Clear( mapids );
			mapNames = param->unitype->mapNames;
			for(it=DMap_First(mapNames); it; it=DMap_Next(mapNames,it)){
				DMap_Insert( mapids, it->value.pVoid, it->key.pVoid );
			}
			k = 1;
			for(j=0; j<param->items->size; j++){
				DValue pv = param->items->data[j];
				node = MAP_Find( mapids, j );
				if( node ){
					node = DMap_Find( dc->routType->mapNames, node->value.pString );
					if( node == NULL ) goto NextDecorator;
					k = node->value.pInt;
					if( k ==0 ) goto NextDecorator;
				}
				match = DaoType_MatchValue( types[k]->X.abtype, pv, NULL );
				if( match ==0 ) goto NextDecorator;
				sum2 += match;
				parpass[k] = 1;
				k += 1;
			}
		}
		for(j=1; j<nested->size; j++){
			k = types[j]->tid;
			if( k == DAO_PAR_VALIST ) break;
			if( parpass[j] ) continue;
			if( k != DAO_PAR_DEFAULT ) goto NextDecorator;
			parpass[j] = 1;
			sum2 += 1;
		}
		sum = sum / (self->routType->nested->size + 1) + sum2 / nested->size;
		if( sum > max ){
			max = sum;
			best = dc;
		}
NextDecorator:
		continue;
	}
	DMap_Delete( mapids );
	return best;
}
static void DaoRoutine_Decorate( DaoRoutine *self, DaoRoutine *decoFunc, DaoTuple *decoParam )
{
	DArray *nested = decoFunc->routType->nested;
	DaoType **decotypes = nested->items.pType;
	DaoParser *parser = DaoParser_New();
	DaoRoutine *routine = DaoRoutine_New();
	DaoRoutine tmp = *self;
	DMap *mapids = DMap_New(0,D_STRING);
	int parpass[DAO_MAX_PARAM];
	int i, j, k;
	parser->routine = routine;
	parser->nameSpace = routine->nameSpace = decoFunc->nameSpace;
	parser->vmSpace = routine->nameSpace->vmSpace;
	GC_IncRC( routine->nameSpace );
	routine->parCount = self->parCount;
	routine->attribs = self->attribs;
	routine->tidHost = self->tidHost;
	GC_ShiftRC( self->routHost, routine->routHost );
	GC_ShiftRC( self->routType, routine->routType );
	routine->routHost = self->routHost;
	routine->routType = self->routType;
	DString_Assign( routine->routName, self->routName );
	DString_Assign( routine->parCodes, self->parCodes );
	for(i=0; i<self->routType->nested->size; i++){
		DaoType *type = self->routType->nested->items.pType[i];
		DRoutine_AddConstValue( (DRoutine*)routine, self->routConsts->data[i] );
		if( type->tid == DAO_PAR_VALIST ) break;
		MAP_Insert( DArray_Top( parser->localVarMap ), type->fname, i );
		DArray_Append( routine->defLocals, self->defLocals->items.pToken[i] );
	}
	parser->locRegCount = self->parCount;
	i = DRoutine_AddConst( (DRoutine*)routine, (DaoBase*) routine );
	MAP_Insert( DArray_Top( parser->localCstMap ), decotypes[0]->fname, i );

	k = 1;
	for(i=0; i<nested->size; i++) parpass[i] = 0;
	if( decoParam ){
		DMap *mapNames = decoParam->unitype->mapNames;
		DNode *it, *node;
		for(it=DMap_First(mapNames); it; it=DMap_Next(mapNames,it)){
			DMap_Insert( mapids, it->value.pVoid, it->key.pVoid );
		}
		for(i=0; i<decoParam->items->size; i++){
			DValue pv = decoParam->items->data[i];
			node = MAP_Find( mapids, i );
			if( node ){
				it = node;
				node = DMap_Find( decoFunc->routType->mapNames, node->value.pString );
				if( node == NULL ) continue;
				k = node->value.pInt;
				if( k ==0 ) continue;
			}
			j = DRoutine_AddConstValue( (DRoutine*)routine, pv );
			MAP_Insert( DArray_Top( parser->localCstMap ), decotypes[k]->fname, j );
			parpass[k] = 1;
			k += 1;
		}
	}
	for(i=1; i<nested->size; i++){
		k = decotypes[i]->tid;
		if( k == DAO_PAR_VALIST ) break;
		if( parpass[i] ) continue;
		if( k != DAO_PAR_DEFAULT ) continue;
		k = DRoutine_AddConstValue( (DRoutine*)routine, decoFunc->routConsts->data[i] );
		MAP_Insert( DArray_Top( parser->localCstMap ), decotypes[i]->fname, k );
		parpass[i] = 1;
	}

	/* if( decoFunc->parser ) DaoRoutine_Compile( decoFunc ); */
	DArray_Assign( parser->tokens, decoFunc->source );
	if( DaoParser_ParseRoutine( parser ) ==0 ) goto ErrorDecorator;
	/* DaoRoutine_PrintCode( routine, self->nameSpace->vmSpace->stdStream ); */
	*self = *routine;
	*routine = tmp;
	nested = self->routTable;
	self->routTable = routine->routTable;
	routine->routTable = nested;
	DaoParser_Delete( parser );
	return;
ErrorDecorator:
	DaoRoutine_Delete( routine );
	DaoParser_Delete( parser );
	DMap_Delete( mapids );
}
static void DaoParser_DecorateRoutine( DaoParser *self, DaoRoutine *rout )
{
	int i, count = self->decoFuncs->size;
	for(i=0; i<count; i++){
		DaoRoutine *decoFunc = self->decoFuncs->items.pRout[i];
		DaoTuple *decoParam = self->decoParams->items.pTuple[i];
		decoFunc = DaoRoutine_GetDecorator( rout, decoFunc, decoParam );
		if( decoFunc == NULL ){
			DaoParser_Error( self, DAO_INVALID_FUNCTION_DECORATION, rout->routName );
			return;
		}
		DaoRoutine_Decorate( rout, decoFunc, decoParam );
		/* printf( "%s\n", decoFunc->routType->name->mbs ); */
	}
}
static int DaoParser_ParseRoutineDefinition( DaoParser *self, int start,
		int from, int to, int storeType )
{
	DaoToken *ptok, **tokens = self->tokens->items.pToken;
	DaoNameSpace *myNS = self->nameSpace;
	DaoTypeBase *typer;
	DaoRoutine *rout = NULL;
	DaoParser *parser = NULL;
	DaoParser *newparser = NULL;
	DaoClass *klass;
	DString *str, *mbs = self->mbs;
	DValue value, scope;
	int perm = self->permission;
	int tki = tokens[start]->name;
	int k, rb, right, virt = 0, stat = 0;
	int errorStart = start;
	if( start > from ){
		int ttkk = tokens[start-1]->name;
		if( ttkk == DKEY_VIRTUAL ) virt = DAO_ROUT_VIRTUAL;
		if( ttkk == DKEY_STATIC ) stat = DAO_ROUT_STATIC;
	}
	if( start > from + 1 ){
		int ttkk = tokens[start-2]->name;
		if( virt && ttkk == DKEY_STATIC ){
			stat = DAO_ROUT_STATIC;
		}else if( stat && ttkk == DKEY_VIRTUAL ){
			virt = DAO_ROUT_VIRTUAL;
		}
	}
	if( (virt | stat) && self->hostClass ==NULL && self->hostInter ==NULL ){
		DaoParser_Error( self, DAO_INVALID_FUNCTION_DEFINITION, tokens[start-1]->string );
		return -1;
	}
	right = -1;
	if( start+2 <= to && ((k=tokens[start+2]->name) == DTOK_COLON2 || k == DTOK_LT) ){
		/* For functions define outside the class body: */
		int oldpos = start + 1;
		int r1 = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 ); /* parameter list. */
		klass = NULL;
		if( r1 < 0 || r1+1 >= self->tokens->size ) return -1;

		start = DaoParser_FindScopedData( self, start+1, & scope, & value, 1, NULL );
		if( start < 0 || scope.t != DAO_CLASS ){
			ptok = tokens[ oldpos ];
			self->curLine = ptok->line;
			if( start >=0 ) DaoParser_Error( self, DAO_SYMBOL_NEED_CLASS, ptok->string );
			DaoParser_Error3( self, DAO_INVALID_FUNCTION_DEFINITION, errorStart );
			return -1;
		}
		DString_Clear( mbs );
		if( tki == DKEY_OPERATOR && r1 == start + 1 ){
			r1 = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start + 2, -1 );
		}
		for( k=start; k<=r1; k++ ) DString_Append( mbs, tokens[k]->string );
		rout= DaoClass_GetOvldRoutine( scope.v.klass, mbs );
		if( ! rout ){
			DMap *hash = scope.v.klass->ovldRoutMap;
			DNode *it = DMap_First( hash );
			int defined = 0;
			ptok = tokens[ start+1 ];
			self->curLine = ptok->line;
			DString_Assign( mbs, tokens[start]->string );
			if( tokens[start]->type == DTOK_LB ){
				int m = start + 1;
				while( m < r1 && tokens[m]->type != DTOK_LB ){
					DString_Assign( mbs, tokens[m]->string );
					m += 1;
				}
			}
			for(;it;it=DMap_Next(hash,it)){
				DaoRoutine *meth = (DaoRoutine*) it->value.pBase;
				if( DString_EQ( meth->routName, mbs ) && meth->type == DAO_ROUTINE ){
					if( DaoParser_ParseRoutine( meth->parser ) ==0 ) return -1;
					if( meth->bodyStart <0 ){
						defined = 1;
						DaoParser_Error( self, DAO_ROUT_NEED_IMPLEMENTATION, it->key.pString );
					}
				}
			}
			if( defined ){
				DaoParser_Error2( self, DAO_ROUT_WRONG_SIGNATURE, start, r1, 0 );
			}else{
				DaoParser_Error2( self, DAO_ROUT_NOT_DECLARED, errorStart+1, r1, 0 );
			}
			DaoParser_Error3( self, DAO_INVALID_FUNCTION_DEFINITION, errorStart );
			return -1;
		}
		parser = rout->parser;
	}else if( start < to ){
		klass = NULL;
		rout = NULL;
		right = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
		start ++;
		DString_Assign( self->mbs2, tokens[start]->string );
		if( tki == DKEY_OPERATOR && right == start+1 ){
			right = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start+2, -1 );
			DString_Append( self->mbs2, tokens[start+1]->string );
		}else if( tki == DKEY_OPERATOR ){
			rb = DaoParser_FindOpenToken( self, DTOK_LB, start, -1, 1 );
			for( k=start+1; k<rb; k++ ) DString_Append( self->mbs2, tokens[k]->string );
		}
		if( right < 0 ){
			ptok = tokens[ start ];
			self->curLine = ptok->line;
			DaoParser_Error( self, DAO_INVALID_FUNCTION_DEFINITION, ptok->string );
			return -1;
		}
		DString_Clear( mbs );
		for( k=start; k<=right; k++ ) DString_Append( mbs, tokens[k]->string );

		if( self->isClassBody ){
			klass = self->hostClass;
			rout = DaoClass_GetOvldRoutine( klass, mbs );
		}else{
			/* XXX support: seperation of declaration and definition */
		}

		if( rout == NULL ){
			if( self->isInterBody ){
				rout = (DaoRoutine*) DRoutine_New();
				rout->nameSpace = self->nameSpace;
				GC_IncRC( rout->nameSpace );
				DRoutine_AddOverLoad( (DRoutine*)rout, (DRoutine*)rout );
				GC_ShiftRC( self->hostInter->abtype, rout->routHost );
				rout->routHost = self->hostInter->abtype;
				if( rout->routHost ) rout->attribs |= stat;
			}else{
				rout = DaoRoutine_New();
				if( self->isClassBody ){
					GC_ShiftRC( self->hostClass->objType, rout->routHost );
					rout->tidHost = DAO_OBJECT;
					rout->attribs |= virt | stat;
					rout->routHost = self->hostClass->objType;
				}
			}
			rout->defLine = tokens[start]->line;
			parser = newparser = DaoParser_New();
			parser->routine = rout;
			parser->vmSpace = self->vmSpace;
			parser->hostType = self->hostType;
			parser->levelBase = self->levelBase + self->lexLevel + 1;
			DString_Assign( rout->routName, self->mbs2 );
			if( STRCMP( rout->routName, "main" ) ==0 ) rout->attribs |= DAO_ROUT_MAIN;
		}else{
			parser = rout->parser;
		}

		if( self->isClassBody ){
			DValue *ref = NULL;
			GC_ShiftRC( klass->objType, rout->routHost );
			parser->hostClass = klass;
			rout->tidHost = DAO_OBJECT;
			rout->routHost = klass->objType;
			DaoClass_AddOvldRoutine( klass, mbs, rout );

			value = daoNullValue;
			if( DString_Compare( self->mbs2, klass->className ) == 0 ){
				/* overloading constructor */
				value.t = DAO_ROUTINE;
				value.v.routine = klass->classRoutine;
				rout->attribs |= DAO_ROUT_INITOR;
				/* printf( "%i %p %s\n", self->inherit, rout, self->mbs2->mbs ); */
			}else{
				DaoClass_GetData( klass, self->mbs2, & value, klass, & ref );
			}

			if( value.t == DAO_ROUTINE ){
				DRoutine_AddOverLoad( (DRoutine*) value.v.p, (DRoutine*)rout );
				value.v.routine = rout;
				DArray_Append( klass->cstDataName, rout->routName );
				DVarray_Append( klass->cstData, value );
#if 0
				if( value.v.routine->routHost != klass->objType ){
					/* value.v.routine a dummy routine derived from parent class */
					DArray *atmp = rout->routTable;
					rout->routTable = value.v.routine->routTable;
					value.v.routine->routTable = atmp;
					GC_ShiftRC( rout, value.v.routine );
					ref->v.routine = rout;
				}
#endif
			}else{
				value.t = DAO_ROUTINE;
				value.v.routine = rout;
				DaoClass_AddConst( klass, self->mbs2, value, perm, rout->defLine );
			}
		}else if( self->isInterBody ){
			DNode *node;
			GC_ShiftRC( self->hostInter->abtype, rout->routHost );
			parser->hostInter = self->hostInter;
			rout->routHost = self->hostInter->abtype;
			node = DMap_Find( self->hostInter->methods, self->mbs2 );

			if( node ){
				DRoutine *existing = (DRoutine*) node->value.pVoid;
				DRoutine_AddOverLoad( (DRoutine*) existing, (DRoutine*)rout );
#if 0
				if( existing->routHost != self->hostInter->abtype ){
					/* value.v.routine a dummy routine derived from parent class */
					DArray *atmp = rout->routTable;
					rout->routTable = existing->routTable;
					existing->routTable = atmp;
					GC_ShiftRC( rout, existing );
					node->value.pVoid = rout;
				}
#endif
			}else{
				DMap_Insert( self->hostInter->methods, self->mbs2, rout );
				GC_IncRC( rout );
			}
		}else{
			int rg = DaoNameSpace_FindConst( myNS, self->mbs2 );
			value = DaoNameSpace_GetConst( myNS, rg );
			if( value.t == DAO_ROUTINE ){
				DRoutine_AddOverLoad( (DRoutine*) value.v.p, (DRoutine*)rout );
			}else{
				value.t = DAO_ROUTINE;
				value.v.routine = rout;
				DaoNameSpace_AddConst( myNS, self->mbs2, value, perm );
			}
		}
	}
	if( rout->routHost ) rout->tidHost = rout->routHost->tid;
	/**/
	parser->defParser = self;
	right = DaoParser_ParsePrototype( self, parser, tki, start );
	if( right < 0 ){
		ptok = tokens[ start ];
		self->curLine = ptok->line;
		DaoParser_Error( self, DAO_INVALID_FUNCTION_DEFINITION, ptok->string );
		return -1;
	}
	k = tokens[right]->name == DTOK_RCB;
	if( self->isClassBody && self->isDynamicClass ){
		DArray *uplocs = DArray_New(0);
		int i, regCall;
		if( k == 0 ){
			DaoParser_Error( self, DAO_INVALID_FUNCTION_DEFINITION, NULL );
			return -1;
		}
		parser->uplocs = uplocs;
		parser->outParser = self;
		if( DaoParser_ParseParams( parser ) == 0 ) return -1;
		parser->outParser = NULL;
		if( ! DaoParser_ParseRoutine( parser ) ){
			DString_SetMBS( mbs, "invalid method in anonymous class" );
			DaoParser_Error( self, DAO_INVALID_FUNCTION_DEFINITION, mbs );
			return -1;
		}
		regCall = self->locRegCount;
		DaoParser_PushRegister( self );
		for( i=0; i<uplocs->size; i+=4 ){
			int up = uplocs->items.pInt[i];
			int loc = uplocs->items.pInt[i+1];
			int first = uplocs->items.pInt[i+2];
			int last = uplocs->items.pInt[i+3];
			DaoParser_AddCode( self, DVM_MOVE, up, 0, regCall+i+1, first, 0, last );
			DaoParser_AddCode( self, DVM_DATA, DAO_INTEGER, loc, regCall+i+2, first,0,last );
		}
		DaoParser_PushRegisters( self, uplocs->size/2 );

		i = DRoutine_AddConst( (DRoutine*)self->routine, (DaoBase*)rout );
		DaoParser_AddCode( self, DVM_GETCL, 0, i, regCall, start, parser->parEnd, right );

		k = self->locRegCount;
		DaoParser_PushRegister( self );
		/* DVM_ROUTINE rout_proto, upv1, upv2, ..., regFix */
		DaoParser_AddCode( self, DVM_ROUTINE, regCall, uplocs->size/2, k, start, parser->parEnd, right );
		MAP_Insert( self->hostClass->protoValues, k, rout->routName );
		if( newparser ) DaoParser_Delete( newparser );
		return right + 1;
	}
	if( DaoParser_ParseParams( parser ) == 0 ) return -1;
	if( k && self->decoFuncs->size ){ /* with body */
		if( DaoParser_ParseRoutine( parser ) ==0 ) return -1;
		DaoParser_DecorateRoutine( self, rout );
	}else if( k && rout->routName->mbs[0] == '@' ){ /* with body */
		if( DaoParser_ParseRoutine( parser ) ==0 ) return -1;
	}else if( k ){ /* with body */
		DArray_Append( self->routCompilable, rout );
		rout->parser = parser;
		newparser = NULL;
	}else if( rout->minimal == 0 ){
		rout->parser = parser;
		newparser = NULL;
	}
	if( newparser ) DaoParser_Delete( newparser );
	return right+1;
}
static int DaoParser_ParseCodeSect( DaoParser *self, int from, int to );
static int DaoParser_ParseInterfaceDefinition( DaoParser *self, int start, int to, int storeType )
{
	DaoToken *ptok, **tokens = self->tokens->items.pToken;
	DaoRoutine *routine = self->routine;
	DaoNameSpace *myNS = self->nameSpace;
	DaoNameSpace *ns = NULL;
	DaoInterface *inter = NULL;
	DaoParser *parser = NULL;
	DaoToken *tokName;
	DString *interName;
	DString *mbs = self->mbs;
	DString *ename = NULL;
	DValue value, scope;
	int i, right, ec = 0, errorStart = start;
	parser = NULL;
	if( start+1 > to ) goto ErrorInterfaceDefinition;
	tokName = tokens[start+1];
	interName = ename = tokName->string;
	start = DaoParser_FindScopedData( self, start+1, & scope, & value, 1, NULL );
	if( start <0 ) goto ErrorInterfaceDefinition;
	if( value.t == DAO_STRING ){
		int line = tokens[start]->line;
		interName = value.v.s;
		inter = DaoInterface_New( interName->mbs );
		if( routine != myNS->mainRoutine ) ns = NULL;
		value.t = DAO_INTERFACE;
		value.v.inter = inter;
		DaoParser_AddToScope( self, scope, interName, value, inter->abtype, storeType, line );

		if( tokens[start+2]->name == DTOK_SEMCO ){
			start += 3;
			return start;
		}
	}else{
		ec = DAO_SYMBOL_WAS_DEFINED;
		goto ErrorInterfaceDefinition;
	}
	start += 1; /* token after class name. */
	if( tokens[start]->name == DTOK_COLON ){
		/* interface AB : NS::BB, CC{ } */
		unsigned char sep = DTOK_COLON;
		while( tokens[start]->name == sep ){
			start = DaoParser_FindScopedData( self, start+1, & scope, & value, 0, mbs );
			if( start < 0 ) goto ErrorInterfaceDefinition;
			ename = tokens[start]->string;
			start ++;
			if( value.t != DAO_INTERFACE ){
				ec = DAO_SYMBOL_NEED_INTERFACE;
				if( value.t == 0 || value.t == DAO_STRING ) ec = DAO_SYMBOL_POSSIBLY_UNDEFINED;
				goto ErrorInterfaceDefinition;
			}
			/* Add a reference to its super interfaces: */
			DArray_Append( inter->supers, value.v.inter );
			GC_IncRC( value.v.inter );
			sep = DTOK_COMMA;
		}
	}
	parser = DaoParser_New();
	parser->routine = myNS->routEvalConst;
	parser->vmSpace = self->vmSpace;
	parser->nameSpace = myNS;
	parser->hostInter = inter;
	parser->isInterBody = 1;
	parser->levelBase = self->levelBase + self->lexLevel + 1;
	parser->hostType = inter->abtype;

	DString_Assign( parser->fileName, self->fileName );

	right = tokens[start]->name == DTOK_LCB ?
		DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, -1 ) : -1 ;
	if( right < 0 ) goto ErrorInterfaceDefinition;
#if 0
	//DaoParser_ExtractComments( self, inter->docString,
	//    rout->defLine, tokens[right]->line );
#endif

	DaoInterface_DeriveMethods( inter );
	for(i=start+1; i<right; i++) DArray_Append( parser->tokens, tokens[i] );
	DaoTokens_Append( parser->tokens, DTOK_SEMCO, tokens[right-1]->line, ";" );
	parser->defined = 1;

	if( DaoParser_ParseCodeSect( parser, 0, parser->tokens->size-1 )==0 ){
		if( DString_EQ( self->fileName, parser->fileName ) )
			DArray_AppendArray( self->errors, parser->errors );
		else
			DaoParser_PrintError( parser, 0, 0, NULL );
		goto ErrorInterfaceDefinition;
	}
	if( parser->vmcLast != parser->vmcBase ){
		DArray_AppendArray( self->errors, parser->errors );
		DaoParser_StatementError( self, parser, DAO_STATEMENT_IN_INTERFACE );
		goto ErrorInterfaceDefinition;
	}
	DaoParser_Delete( parser );
	return right + 1;
ErrorInterfaceDefinition:
	if( parser ) DaoParser_Delete( parser );
	if( ec ) DaoParser_Error( self, ec, ename );
	if( ec == DAO_SYMBOL_POSSIBLY_UNDEFINED )
		DaoParser_Error( self, DAO_SYMBOL_NEED_INTERFACE, ename );
	DaoParser_Error2( self, DAO_INVALID_INTERFACE_DEFINITION, errorStart, to, 0 );
	return -1;
}
static int DaoParser_ParseClassDefinition( DaoParser *self, int start, int to, int storeType )
{
	DaoToken *ptok, **tokens = self->tokens->items.pToken;
	DaoNameSpace *myNS = self->nameSpace;
	DaoNameSpace *ns = NULL;
	DaoRoutine *routine = self->routine;
	DaoRoutine *rout = NULL;
	DaoParser *parser = NULL;
	DaoClass *klass = NULL;
	DaoTypeBase *typer;
	DaoToken *tokName;
	DString *str, *mbs = self->mbs;
	DString *className, *ename = NULL;
	DValue value, scope;
	int begin, line = self->curLine;
	int tki = tokens[start]->name;
	int i, k, rb, right, virt = 0, stat = 0;
	int errorStart = start;
	int final = 0;
	int ec = 0;
	if( tki == DKEY_FINAL ){
		final = DAO_CLS_FINAL;
		start ++;
	}
	if( start+1 > to ) goto ErrorClassDefinition;
	tokName = tokens[start+1];
	className = ename = tokName->string;
	start = DaoParser_FindScopedData( self, start+1, & scope, & value, 1, NULL );
	if( start <0 ) goto ErrorClassDefinition;
	ename = tokens[start]->string;
	if( value.t == DAO_STRING ){
		int line = tokens[start]->line;
		klass = DaoClass_New();
		klass->attribs |= final;

		className = klass->className;
		DString_Assign( className, value.v.s );
		if( className->mbs[0] == '@' ){
			DString_Erase( className, 0, 1 );
			klass->attribs |= DAO_CLS_SYNCHRONOUS;
		}

		if( start+1 <= to && tokens[start+1]->name == DTOK_LT ){
			rb = DaoParser_FindPairToken( self, DTOK_LT, DTOK_GT, start+1, -1 );
			if( rb <= start + 2 ) goto ErrorClassDefinition;
			klass->typeHolders = DArray_New(0);
			klass->typeDefaults = DArray_New(0);
			klass->instanceClasses = DMap_New(D_STRING,0);
			DString_AppendChar( className, '<' );
			i = start + 2;
			while( i < rb ){
				DaoType *holder, *deft = NULL;
				str = tokens[i]->string;
				if( tokens[i]->name != DTOK_ID_INITYPE ){
					DaoParser_Error( self, DAO_TOKEN_NOT_EXPECTED, str );
					goto ErrorClassDefinition;
				}
				holder = DaoNameSpace_MakeType( myNS, str->mbs, DAO_INITYPE, 0,0,0 );
				if( holder == NULL ) goto ErrorClassDefinition;
				if( klass->typeHolders->size ) DString_AppendChar( className, ',' );
				DString_Append( className, holder->name );
				i += 1;
				if( i < rb && tokens[i]->type == DTOK_ASSN ){
					deft = DaoParser_ParseType( self, i+1, rb-1, &i, NULL );
					if( deft == NULL ) goto ErrorClassDefinition;
					GC_IncRC( deft );
					i += 1;
				}
				GC_IncRC( holder );
				DArray_Append( klass->typeHolders, holder );
				DArray_Append( klass->typeDefaults, deft );
				DaoClass_AddReference( klass, holder );
				DaoClass_AddReference( klass, deft );
				if( i < rb && tokens[i]->type != DTOK_COMMA ){
					DaoParser_Error( self, DAO_TOKEN_NOT_EXPECTED, tokens[i]->string );
					goto ErrorClassDefinition;
				}
				i += 1;
			}
			DString_AppendChar( className, '>' );
			DaoClass_SetName( klass, className );
			klass->objType->nested = DArray_Copy( klass->typeHolders );
			DMap_Insert( klass->instanceClasses, className, klass );
			DString_Assign( klass->className, value.v.s );
			if( className->mbs[0] == '@' ) DString_Erase( className, 0, 1 );
			GC_IncRCs( klass->objType->nested );
			GC_IncRC( klass );
			start = rb;
		}else{
			DaoClass_SetName( klass, className );
		}
		DArray_Append( myNS->definedRoutines, klass->classRoutine );
		if( routine != myNS->mainRoutine ) ns = NULL;
		value.t = DAO_CLASS;
		value.v.klass = klass;
		DaoParser_AddToScope( self, scope, className, value, klass->objType, storeType, line );

		if( start+1 <= to && tokens[start+1]->name == DTOK_SEMCO ){
			start += 2;
			return start;
		}
	}else if( value.t != DAO_CLASS ){
		ec = DAO_SYMBOL_WAS_DEFINED;
		goto ErrorClassDefinition;
	}else if( value.v.klass->derived ){
		ec = DAO_SYMBOL_WAS_DEFINED;
		goto ErrorClassDefinition;
	}

	rout = klass->classRoutine;
	rout->defLine = tokens[start]->line;
	GC_ShiftRC( myNS, rout->nameSpace );
	rout->nameSpace = myNS;

	parser = DaoParser_New();
	parser->vmSpace = self->vmSpace;
	parser->routine = rout;
	parser->isClassBody = 1;
	parser->hostClass = klass;
	parser->nameSpace = myNS;
	parser->levelBase = self->levelBase + self->lexLevel + 1;
	parser->hostType = klass->objType;

	DString_Assign( parser->fileName, self->fileName );

	DaoTokens_Append( parser->partoks, DTOK_LB, tokens[start]->line, "(" );
	DaoTokens_Append( parser->partoks, DTOK_RB, tokens[start]->line, ")" );

	start ++; /* token after class name. */
	if( start > to || tokens[start]->name == DTOK_LB ) goto ErrorClassDefinition;
	if( tokens[start]->name == DTOK_COLON ){
		/* class AA : NS::BB, CC{ } */
		unsigned char sep = DTOK_COLON;
		if( klass->typeHolders ){
			for(k=0; k<klass->typeHolders->size; k++){
				DaoType *tp = klass->typeHolders->items.pType[k];
				if( DMap_Find( self->initTypes, tp ) ) continue;
				DMap_Insert( self->initTypes, tp, tp );
			}
		}
		while( tokens[start]->name == sep ){
			DaoClass *super = 0;
			start = DaoParser_FindScopedData( self, start+1, & scope, & value, 0, mbs );
			super = NULL;
			if( start <0 ) goto ErrorClassDefinition;
			ename = tokens[start]->string;
			if( value.t != DAO_CLASS && value.t != DAO_CDATA ){
				ec = DAO_SYMBOL_NEED_CLASS_CTYPE;
				if( value.t == 0 || value.t == DAO_STRING ) ec = DAO_SYMBOL_POSSIBLY_UNDEFINED;
				goto ErrorClassDefinition;
			}
			super = value.v.klass;
			if( super->type ==DAO_CLASS && ( super->attribs & DAO_CLS_FINAL ) ){
				ec = DAO_CLASS_DERIVE_FINAL;
				goto ErrorClassDefinition;
			}
			start ++;

			if( tokens[start]->name == DTOK_LB ) goto ErrorClassDefinition;
			if( super == NULL ){
				ec = DAO_SYMBOL_POSSIBLY_UNDEFINED;
				goto ErrorClassDefinition;
			}
			/* Add a reference to its super classes: */
			DaoClass_AddSuperClass( klass, (DaoBase*) super, mbs );
			sep = DTOK_COMMA;
		}
		if( klass->typeHolders ){
			for(k=0; k<klass->typeHolders->size; k++){
				DaoType *tp = klass->typeHolders->items.pType[k];
				DNode *node = DMap_Find( self->initTypes, tp );
				if( node && node->key.pVoid == node->value.pVoid )
					DMap_EraseNode( self->initTypes, node );
			}
		}
	}/* end parsing super classes */
	begin = start;
	right = tokens[start]->name == DTOK_LCB ?
		DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, -1 ) : -1 ;

	if( right < 0 ) goto ErrorClassDefinition;
	DaoParser_ExtractComments( self, klass->classHelp, rout->defLine, tokens[right]->line );

	for(i=begin+1; i<right; i++) DArray_Append( parser->tokens, tokens[i] );
	DaoTokens_Append( parser->tokens, DTOK_SEMCO, tokens[right-1]->line, ";" );
	parser->defined = 1;

	DaoClass_DeriveClassData( klass );
	DaoParser_ParseParams( parser );
	if( DaoParser_ParseCodeSect( parser, 0, parser->tokens->size-1 )==0 ){
		if( DString_EQ( self->fileName, parser->fileName ) )
			DArray_AppendArray( self->errors, parser->errors );
		else
			DaoParser_PrintError( parser, 0, 0, NULL );
		goto ErrorClassDefinition;
	}
	DaoClass_DeriveObjectData( klass );
	if( parser->vmcLast != parser->vmcBase ){
#if 0
		DaoParser_PrintCodes( parser );
#endif
		DArray_AppendArray( self->errors, parser->errors );
		DaoParser_StatementError( self, parser, DAO_STATEMENT_IN_CLASS );
		goto ErrorClassDefinition;
	}
	DaoClass_ResetAttributes( klass );
	DArray_Clear( parser->tokens );
	DaoTokens_AppendInitSuper( parser->tokens, klass, line, 0 );
	DaoParser_ParseRoutine( parser );
	rout->parser = NULL;
	DaoParser_Delete( parser );
	/* TODO: compile routines if it is not in incremental compiling mode */

	return right + 1;
ErrorClassDefinition:
	if( parser ) DaoParser_Delete( parser );
	if( ec ) DaoParser_Error( self, ec, ename );
	ec = DAO_INVALID_CLASS_DEFINITION + ((klass->attribs & DAO_CLS_SYNCHRONOUS) !=0);
	DaoParser_Error2( self, ec, errorStart, to, 0 );
	return -1;
}
static int DaoParser_ParseEnumDefinition( DaoParser *self, int start, int to, int storeType )
{
	DaoToken *ptok, **tokens = self->tokens->items.pToken;
	DaoNameSpace *myNS = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DaoType *abtp, *abtp2;
	DString *str, *alias = NULL;
	DValue dv, iv = daoZeroInt;
	int global = storeType & (DAO_DATA_GLOBAL|DAO_DATA_STATIC);
	int stat = storeType & DAO_DATA_STATIC;
	int sep = DTOK_COMMA, value = 0;
	int id, rb, comma, semco, explicit=0;
	int reg, cst = 0;
	char buf[32];
	rb = -1;
	abtp = NULL;
	if( self->levelBase + self->lexLevel ==0 ) global = 1;
	rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start+1, -1 );
	if( tokens[start+2]->type != DTOK_LCB || rb <0 ) goto ErrorEnumDefinition;
	ptok = tokens[start+1];
	if( ptok->type != DTOK_IDENTIFIER ) goto ErrorEnumDefinition;
	alias = ptok->string;
	if( ptok->name != DTOK_ID_SYMBOL && ptok->name != DTOK_ID_INITYPE ) start += 1;
	if( (id = DaoParser_GetRegister( self, ptok )) >=0 ){
		DaoParser_Error( self, DAO_SYMBOL_WAS_DEFINED, alias );
		goto ErrorEnumDefinition;
	}
	if( DaoNameSpace_FindType( self->nameSpace, alias) ){
		DaoParser_Error( self, DAO_SYMBOL_WAS_DEFINED, alias );
		goto ErrorEnumDefinition;
	}

	abtp = DaoType_New( "enum<", DAO_ENUM, NULL, NULL );
	abtp->mapNames = DMap_New(D_STRING,0);
	comma = DaoParser_FindOpenToken( self, DTOK_COMMA, start+2, -1, 0 );
	semco = DaoParser_FindOpenToken( self, DTOK_SEMCO, start+2, -1, 0 );
	if( comma >=0 && semco >=0 ){
		if( semco < comma ){
			sep = DTOK_SEMCO;
			comma = semco;
		}
	}else if( semco >=0 ){
		sep = DTOK_SEMCO;
		comma = semco;
	}
	if( comma <0 ) comma = rb;
	if( sep == DTOK_SEMCO ){
		value = 1;
		abtp->flagtype = 1;
	}
	start = start + 2;
	while( comma >=0 ){
		if( start >= comma ) break;
		if( tokens[start]->type != DTOK_IDENTIFIER ){
			DaoParser_Error( self, DAO_TOKEN_NEED_NAME, tokens[start]->string );
			goto ErrorEnumDefinition;
		}
		str = tokens[start]->string;
		explicit = 0;
		if( tokens[start+1]->type == DTOK_ASSN ){
			explicit = 1;
			reg = DaoParser_MakeArithTree( self, start+2, comma-1, & cst, -1, 0 );
			if( reg < 0 ) goto ErrorEnumDefinition;
			dv = daoNullValue;
			if( cst ) dv = DaoParser_GetVariable( self, cst );
			if( dv.t < DAO_INTEGER || dv.t > DAO_DOUBLE ){
				DaoParser_Error( self, DAO_EXPR_NEED_CONST_NUMBER, tokens[start+2]->string );
				goto ErrorEnumDefinition;
			}
			value = DValue_GetInteger( dv );
		}else if( start+1 != rb && tokens[start+1]->type != sep ){
			DaoParser_Error( self, DAO_TOKEN_NOT_EXPECTED, tokens[start+1]->string );
			goto ErrorEnumDefinition;
		}
		if( DMap_Find( abtp->mapNames, str ) ){
			DaoParser_Error( self, DAO_SYMBOL_WAS_DEFINED, str );
			goto ErrorEnumDefinition;
		}
		if( abtp->mapNames->size ){
			DString_AppendChar( abtp->name, sep == DTOK_SEMCO ? ';' : ',' );
		}
		DString_Append( abtp->name, str );
		if( explicit ){
			sprintf( buf, "=%i", value );
			DString_AppendMBS( abtp->name, buf );
		}
		DMap_Insert( abtp->mapNames, str, (void*)(size_t)value );
		if( sep == DTOK_SEMCO ) value <<= 1; else value += 1;
		if( comma == rb ) break;
		start = comma + 1;
		comma = DaoParser_FindOpenToken( self, sep, comma+1, -1, 0 );
		if( comma <0 ) comma = rb;
	}
	DString_AppendChar( abtp->name, '>' );
	abtp2 = DaoNameSpace_FindType( self->nameSpace, abtp->name );
	if( abtp2 ){
		DaoType_Delete( abtp );
	}else{
		DaoNameSpace_AddType( self->nameSpace, abtp->name, abtp );
		abtp2 = abtp;
	}
	if( alias ){
		abtp2 = DaoType_Copy( abtp2 );
		DString_Assign( abtp2->name, alias );
		DaoNameSpace_AddType( self->nameSpace, abtp2->name, abtp2 );
	}
	return rb + 1;
ErrorEnumDefinition:
	if( rb >=0 )
		DaoParser_Error2( self, DAO_INVALID_ENUM_DEFINITION, start, rb, 0 );
	else
		DaoParser_Error3( self, DAO_INVALID_ENUM_DEFINITION, start );
	if( abtp ) DaoType_Delete( abtp );
	return 0;
}
static int DaoParser_GetNormRegister( DaoParser *self, int reg, int first, int mid, int last );
static int DaoParser_ParseCodeSect( DaoParser *self, int from, int to )
{
	DaoNameSpace *myNS = self->nameSpace;
	DaoVmSpace *vmSpace = self->vmSpace;
	DaoRoutine *routine = self->routine;
	DaoClass *hostClass = self->hostClass;

	DaoToken token = { DTOK_IDENTIFIER, DTOK_IDENTIFIER, 0 };
	DaoToken **tokens = self->tokens->items.pToken;
	DaoToken *ptok;
	DMap *switchMap;
	int rb, reg1;

	int storeType = 0;
	int storeType2 = 0;
	int i, rbrack, end, temp, temp2, decl;
	int reg, N = 0;
	int cst = 0;

	unsigned char tki, tki2;
	char buffer[512];

	int k, right, start = from;
	int decStart, decEnd, expStart, expEnd;
	int colon, eq, decl2;
	int comma, last, errorStart, needName;
	int empty_decos = 0;

	DaoClass *klass = NULL;
	DaoParser *parser = NULL;
	DaoRoutine *rout = NULL;

	DaoInode *front = self->vmcFirst;
	DaoInode *back = self->vmcLast;

	DString *mbs = self->mbs;
	DString *str;
	DaoTypeBase *typer;
	DaoFunction *meth;
	DaoType  *abtp;
	DValue    value, scope;
	DValue    nil = daoNullValue;

	token.string = mbs;
	self->error = 0;
	self->permission = DAO_DATA_PUBLIC;
	myNS->vmpEvalConst->topFrame->context->vmSpace = vmSpace;
	myNS->vmpEvalConst->vmSpace = vmSpace;

	if( from ==0 && (to+1) == self->tokens->size ){
		for(i=0; i<self->tokens->size; i++) tokens[i]->index = i;
	}

#if 0
	printf("routine = %p; %i, %i\n", routine, start, to );
	for(i=start; i<=to; i++) printf("%s  ", tokens[i]->string->mbs); printf("\n\n");
#endif

	while( start >= from && start <= to ){

		self->curLine = tokens[start]->line;
		ptok = tokens[start];
		tki = tokens[start]->name;
#if 0
		printf("At tokPos : %i, %i, %p\n", start,ptok->line, ptok->string );
		printf("At tokPos : %i, %i, %s\n", start,ptok->line, ptok->string->mbs );
#endif
		if( self->errors->size ) return 0;
		if( empty_decos ){
			DArray_Clear( self->decoFuncs );
			DArray_Clear( self->decoParams );
		}
		DArray_Clear( self->enumTypes );
		errorStart = start;
		if( ! self->isClassBody ) self->permission = DAO_DATA_PUBLIC;
		if( tki >= DKEY_PRIVATE && tki <= DKEY_PUBLIC ){
			self->permission = tki - DKEY_PRIVATE + DAO_DATA_PRIVATE;
			start ++;
			if( start > to ) break;
			tki = tokens[start]->name;
		}

		storeType = 0;
		storeType2 = 0;
		needName = 0;
		while( tki >= DKEY_CONST && tki <= DKEY_VAR ){
			int comb = ( storeType & DAO_DATA_VAR );
			needName = 1;
			switch( tki ){
			case DKEY_CONST :
				storeType |= DAO_DATA_CONST;
				storeType2 |= DAO_DATA_CONST;
				if( ! (storeType & DAO_DATA_LOCAL) ){
					if( self->levelBase + self->lexLevel ==0 ) storeType |= DAO_DATA_GLOBAL;
					else if( self->isClassBody ) storeType |= DAO_DATA_MEMBER;
				}
				break;
			case DKEY_GLOBAL :
				comb |= ( storeType & (DAO_DATA_LOCAL|DAO_DATA_CONST) );
				storeType |= DAO_DATA_GLOBAL;
				storeType2 |= DAO_DATA_GLOBAL;
				break;
			case DKEY_STATIC :
				comb |= ( storeType & (DAO_DATA_LOCAL|DAO_DATA_CONST) );
				storeType |= DAO_DATA_STATIC;
				storeType2 |= DAO_DATA_STATIC;
				if( self->levelBase ==0 ) storeType |= DAO_DATA_GLOBAL;
				if( self->isClassBody ) storeType |= DAO_DATA_MEMBER;
				break;
			case DKEY_VAR :
				comb |= storeType;
				storeType |= DAO_DATA_VAR;
				storeType2 |= DAO_DATA_VAR;
				if( self->isClassBody )
					storeType |= DAO_DATA_MEMBER;
				else
					storeType |= DAO_DATA_LOCAL;
				break;
			default : break;
			}
			if( comb || ((storeType & DAO_DATA_STATIC) && ! self->isClassBody) ){
				if( comb ==0 ) DaoParser_Error3( self, DAO_STATEMENT_OUT_OF_CONTEXT, start );
				DaoParser_Error2( self, DAO_INVALID_STORAGE, errorStart, start, 0 );
				return 0;
			}
			start ++;
			ptok = tokens[start];
			tki = ptok->name;
		}
		if( myNS->options & DAO_NS_AUTO_GLOBAL ){
			if( self->levelBase + self->lexLevel ==0 && !(storeType & DAO_DATA_LOCAL) ){
				storeType |= DAO_DATA_GLOBAL;
			}
		}
		if( self->isClassBody ){
			if( hostClass->attribs & DAO_CLS_SYNCHRONOUS ){
				if( storeType & DAO_DATA_STATIC ){
					DaoParser_Error2( self, DAO_INVALID_ACCESS, errorStart, start, 0 );
				}
				if( (storeType & DAO_DATA_VAR) && self->permission == DAO_DATA_PUBLIC ){
					DaoParser_Error2( self, DAO_INVALID_STORAGE, errorStart, start, 0 );
				}
				if( self->errors->size ) return 0;
			}
		}
		tki = tokens[start]->name;
		tki2 = start+1 <= to ? tokens[start+1]->name : 0;
		if( needName && (ptok->type != DTOK_IDENTIFIER || (tki != DKEY_ENUM 
						&& tki > DAO_NOKEY1 && tki < DKEY_EACH )) ){
			if( tki < DKEY_SUB || tki > DKEY_OPERATOR || storeType2 != DAO_DATA_STATIC ){
				DaoParser_Error( self, DAO_TOKEN_NEED_NAME, tokens[start]->string );
				DaoParser_Error3( self, DAO_INVALID_STATEMENT, errorStart );
				return 0;
			}
		}
		if( tki == DTOK_ID_INITYPE ){
			DaoRoutine *decfunc = NULL;
			DaoTuple *dectup = NULL;
			empty_decos = 0;
			reg = DaoParser_GetRegister( self, tokens[start] );
			if( reg < 0 ) goto DecoratorError;
			if( !(LOOKUP_ST( reg ) & 1) ) goto DecoratorError;
			value = DaoParser_GetVariable( self, reg );
			if( value.t != DAO_ROUTINE ) goto DecoratorError;
			decfunc = value.v.routine;
			if( start+1 <= to && tokens[start+1]->name == DTOK_LB ){
				rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
				if( rb < 0 ) goto DecoratorError;
				tokens[start]->name = DKEY_TUPLE;
				tokens[start+1]->name = tokens[start+1]->type = DTOK_LCB;
				tokens[rb]->name = tokens[rb]->type = DTOK_RCB;
				reg = DaoParser_MakeArithTree( self, start, rb, &cst, -1, 0 );
				tokens[start]->name = DTOK_IDENTIFIER;
				tokens[start+1]->name = tokens[start+1]->type = DTOK_LB;
				tokens[rb]->name = tokens[rb]->type = DTOK_RB;
				if( reg < 0 ) goto DecoratorError;
				if( cst ==0 ){
					DaoParser_Error2( self, DAO_EXPR_NEED_CONST_EXPR, start+2, rb-1, 0 );
					goto DecoratorError;
				}
				value = DaoParser_GetVariable( self, cst );
				dectup = value.v.tuple;
				start = rb;
			}
			DArray_PushFront( self->decoFuncs, decfunc );
			DArray_PushFront( self->decoParams, dectup );
			start ++;
			continue;
DecoratorError:
			DaoParser_Error3( self, DAO_CTW_INVA_SYNTAX, start );
			return 0;
		}
		empty_decos = 1;
		if( tki == DTOK_SEMCO ){
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			start ++;
			continue;
		}else if( tki == DTOK_AT2 ){
			int from = start;
			int cmd = start + 1;
			int rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
			if( tokens[start+2]->name != DTOK_LB || rb < 0 ){
				/*vmSpace->warning( "(...)", DAO_CTW_IS_EXPECTED ); */
				start ++;
				continue;
			}
			start += 3;
			if( TOKCMP( tokens[ cmd ], "ECHO" ) ==0 ){
				int len;
				char *info;
				strcpy( buffer, tokens[start]->string->mbs );
				len = (int)strlen(buffer);
				if( buffer[len-1]=='\"' || buffer[len-1]=='\'' ) buffer[ len-1 ] = 0;
				info = buffer;
				if( info[0]=='\"' || info[0] == '\'' ) info = info + 1;
				if( TOKCMP( tokens[ cmd ], "ECHO" ) ==0 ){
					DaoStream_WriteMBS( vmSpace->stdStream, info );
					DaoStream_WriteMBS( vmSpace->stdStream, "\n" );
				}
			}
			start = rb + 1;
			continue;
		}else if( tki == DKEY_VIRTUAL ){
			int t3 = start+2 <= to ? tokens[start+2]->name : 0;
			if( tki2 >= DKEY_SUB && tki2 <= DKEY_FUNCTION ){
				start ++;
				continue;
			}else if( tki2 == DKEY_STATIC && t3 >= DKEY_SUB && t3 <= DKEY_FUNCTION ){
				start ++;
				continue;
			}else{
				DaoParser_Error3( self, DAO_STATEMENT_OUT_OF_CONTEXT, start );
				return 0;
			}
		}else if( tki == DKEY_LOAD && tki2 != DTOK_LB ){
			end = DaoParser_FindOpenToken( self, DTOK_SEMCO, start, -1, 1 );
			if( end < 0 ) return 0;
			if( ! DaoParser_ParseLoadStatement( self, start, end ) ) return 0;
			start = end + 1;
			continue;
		}else if( tki == DKEY_USE ){
			start = DaoParser_ParseUseStatement( self, start, to );
			if( start <0 ) return 0;
			continue;
		}else if( tki == DKEY_BIND ){
			DaoInterface *inter;
			DRoutine *fail = NULL;
			int bl, old = start;
			int ito = DaoParser_FindOpenToken( self, DKEY_TO, start, to, 1 );
			if( ito <0 ){
				DaoParser_Error3( self, DAO_INVALID_BINDING, old );
				return 0;
			}
			start = DaoParser_FindScopedData( self, start+1, & scope, & value, 0, NULL );
			if( ito == old + 1 || ito != start + 1 || value.t != DAO_INTERFACE ){
				DaoParser_Error( self, DAO_SYMBOL_NEED_INTERFACE, tokens[start]->string );
				DaoParser_Error2( self, DAO_INVALID_BINDING, old, start, 1 );
				return 0;
			}
			inter = value.v.inter;
			if( tokens[ito+1]->name == DKEY_ANY ){
				start = ito + 1;
				inter->bindany = 1;
				continue;
			}
			start = DaoParser_FindScopedData( self, ito+1, & scope, & value, 0, NULL );
			if( self->bindtos == NULL ) self->bindtos = DArray_New(0);
			if( value.t == DAO_CLASS ){
				DArray_Append( self->bindtos, inter );
				DArray_Append( self->bindtos, value.v.klass->objType );
				DArray_Append( self->bindtos, old );
				DArray_Append( self->bindtos, start );
				DArray_Append( self->bindtos, 0 );
			}else if( value.t == DAO_CDATA ){
				DArray_Append( self->bindtos, inter );
				DArray_Append( self->bindtos, value.v.cdata->typer->priv->abtype );
				DArray_Append( self->bindtos, old );
				DArray_Append( self->bindtos, start );
				DArray_Append( self->bindtos, 0 );
			}else{
				DaoParser_Error( self, DAO_SYMBOL_NEED_BINDABLE, tokens[start]->string );
				DaoParser_Error2( self, DAO_INVALID_BINDING, old, start, 1 );
				return 0;
			}
			continue;
		}else if( tki == DKEY_TYPEDEF ){
			abtp = DaoParser_ParseType( self, start+1, to, & start, NULL );
			if( abtp == NULL ){
				DaoParser_Error3( self, DAO_INVALID_TYPEDEF, errorStart );
				return 0;
			}
			if( tokens[start]->type != DTOK_IDENTIFIER ){
				DaoParser_Error( self, DAO_INVALID_TYPE_NAME, tokens[start]->string );
				DaoParser_Error3( self, DAO_INVALID_TYPEDEF, errorStart );
				return 0;
			}
			str = tokens[start]->string;
			if( DaoType_FindType( str, myNS, self->hostClass, routine ) ){
				DaoParser_Error( self, DAO_SYMBOL_WAS_DEFINED, str );
				DaoParser_Error3( self, DAO_INVALID_TYPEDEF, errorStart );
				return 0;
			}
			value.t = DAO_TYPE;
			abtp = DaoType_Copy( abtp );
			value.v.p = (DaoBase*) abtp;
			DString_Assign( abtp->name, str );
			/*  XXX typedef in routine or class */
			DaoNameSpace_AddType( myNS, str, abtp );
			DaoNameSpace_AddConst( myNS, str, value, DAO_DATA_PUBLIC );
			/* printf( "%s\n", abtp->name->mbs ); */
			start ++;
			continue;
		}else if( tki == DKEY_ENUM && (tki2 == DTOK_LCB || tki2 == DTOK_IDENTIFIER) ){
			start = DaoParser_ParseEnumDefinition( self, start, to, storeType );
			if( start <0 ) return 0;
			continue;
		}

		/* parsing routine definition */
		if( (tki >= DKEY_SUB && tki <= DKEY_FUNCTION && tki2 != DTOK_LB) || tki == DKEY_OPERATOR ){
			start = DaoParser_ParseRoutineDefinition( self, start, from, to, storeType );
			if( start <0 ) return 0;
			continue;
		}else if( tki == DKEY_INTERFACE ){
			start = DaoParser_ParseInterfaceDefinition( self, start, to, storeType );
			if( start <0 ) return 0;
			continue;
		}else if( ( tki == DKEY_FINAL && start+1 <= to
					&& tokens[start+1]->name == DKEY_CLASS ) || tki == DKEY_CLASS ){
			/* parsing class definition */
			start = DaoParser_ParseClassDefinition( self, start, to, storeType );
			if( start <0 ) return 0;
			continue;
		}

		tki = tokens[start]->name;
		switch( tki ){
		case DKEY_WHILE :
			if( self->vmcTop->code == DVM_DO ){
				if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
				if( self->scoping->size >0 && DArray_TopInt( self->scoping ) == DVM_DO )
					if( DaoParser_DelScope( self, DVM_DO, start ) == 0 ) return 0;
				if( ( rbrack = DaoParser_MakeWhileLogic( self, DVM_DOWHILE, start ) ) <0 ) return 0;
			}else{
				DaoParser_AddCode( self, DVM_WHILE_AUX, 0, 0, 0, start, 0,0 );
				if( ( rbrack = DaoParser_MakeWhileLogic( self, DVM_WHILE, start ) ) <0 ) return 0;
			}
			start = rbrack+1;
			continue;
		case DKEY_IF :
			if( ( rbrack = DaoParser_MakeWhileLogic( self, DVM_IF, start ) ) <0 ) return 0;
			start = rbrack+1;
			continue;
		case DKEY_ELIF : case DKEY_ELSEIF :
			if( ( rbrack = DaoParser_MakeWhileLogic( self, DVM_ELIF, start ) ) <0 ) return 0;
			start = rbrack+1;
			continue;
		case DKEY_ELSE :
			if( tokens[start+1]->name == DKEY_IF ){
				if( ( rbrack = DaoParser_MakeWhileLogic( self, DVM_ELIF, start+1 ) ) <0 ) return 0;
				start = rbrack+1;
			}else{
				DaoParser_AddCode( self, DVM_ELSE, 0, self->vmcCount, DVM_ELSE, start, 0,0 );
				if( tokens[start+1]->name != DTOK_LCB) DaoParser_AddScope( self, DVM_UNUSED, start );
				start += 1;
			}
			continue;
		case DKEY_FOR :
			start = DaoParser_MakeForLoop( self, start, to );
			if( start < 0 ){
				DaoParser_Error3( self, DAO_INVALID_FOR, errorStart );
				return 0;
			}
			continue;
		case DKEY_DO :
			DaoParser_AddCode( self, DVM_DO, 0, 0, 0, start, 0,0 );
			if( tokens[start+1]->name != DTOK_LCB )
				DaoParser_AddScope( self, DVM_DO, start );
			start ++;
			continue;
		case DKEY_UNTIL :
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			if( self->scoping->size >0 && DArray_TopInt( self->scoping ) == DVM_DO )
				if( DaoParser_DelScope( self, DVM_DO, start ) == 0 ) return 0;
			if( (rbrack = DaoParser_MakeWhileLogic( self, DVM_UNTIL, start )) <0 ) return 0;
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			start = rbrack+1;
			continue;
		case DKEY_SWITCH :
			rb = DaoParser_FindOpenToken( self, DTOK_LB, start, -1, 1 );
			if( rb < 0 ) return 0;
			rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
			if( rb < 0 ) return 0;
			reg1 = DaoParser_MakeArithTree( self, start+2, rb-1, & cst, -1, 0 );
			if( reg1 < 0 ) return 0;
			switchMap = DMap_New(D_VALUE,0);
			DaoParser_AddCode( self, DVM_SWITCH, reg1, self->switchMaps->size, 0, start,0,rb );
			DArray_Append( self->switchMaps, switchMap );
			DMap_Delete( switchMap );
			start = rb + 1;
			continue;
		case DKEY_CASE :
			colon = DaoParser_FindOpenToken( self, DTOK_COLON, start, -1, 1 );
			comma = DaoParser_FindOpenToken( self, DTOK_COMMA, start, colon, 0 );
			last = start + 1;
			if( colon < 0 ){
				DaoParser_Error2( self, DAO_CASE_NOT_VALID, start, to, 1 );
				return 0;
			}
			if( comma < 0 ) comma = colon;
			while( last < colon ){
				int dots = DaoParser_FindOpenToken( self, DTOK_DOTS, last, comma, 0 );
				front = self->vmcFirst;
				back = self->vmcLast;
				if( dots <0 ){
					reg1 = DaoParser_MakeArithTree( self, last, comma-1, & cst, -1, 0 );
					if( reg1 < 0 ){
						DaoParser_Error2( self, DAO_CASE_NOT_VALID, start, colon, 1 );
						return 0;
					}
				}else{
					int c1 = 0, c2 = 0;
					int r1 = DaoParser_MakeArithTree( self, last, dots-1, & c1, -1, 0 );
					int r2 = DaoParser_MakeArithTree( self, dots+1, comma-1, & c2, -1, 0 );
					DValue v1 = daoNullValue, v2 = daoNullValue;
					DaoPair *pair;
					if( r1 < 0 || r2 < 0 ){
						DaoParser_Error2( self, DAO_CASE_NOT_VALID, start, colon, 1 );
						return 0;
					}
					if( c1 * c2 ==0 ){
						if( ! c2 ) DaoParser_Error2( self, DAO_CASE_NOT_CONSTANT, dots+1, comma-1, 0 );
						if( ! c1 ) DaoParser_Error2( self, DAO_CASE_NOT_CONSTANT, last, dots-1, 0 );
						DaoParser_Error2( self, DAO_CASE_NOT_VALID, start, colon, 1 );
						return 0;
					}
					v1 = DaoParser_GetVariable( self, c1 );
					v2 = DaoParser_GetVariable( self, c2 );
					pair = DaoPair_New( v1, v2 );
					cst = DaoRoutine_AddConst( routine, pair );
				}
				if( ! cst ){
					DaoParser_Error2( self, DAO_CASE_NOT_CONSTANT, last, comma-1, 0 );
					DaoParser_Error2( self, DAO_CASE_NOT_VALID, start, colon, 1 );
					return 0;
				}else if( LOOKUP_ST( cst ) != DAO_LOCAL_CONSTANT ){
					value = DaoParser_GetVariable( self, cst );
					cst = DRoutine_AddConstValue( (DRoutine*) routine, value );
				}else if( LOOKUP_UP( cst ) != 0 ){
					value = DaoParser_GetVariable( self, cst );
					cst = DRoutine_AddConstValue( (DRoutine*) routine, value );
				}else{
					cst = LOOKUP_ID( cst );
				}
				/* remove GETC so that CASETAG will be together,
				   which is neccessary to properly setup switch table: */
				k = DaoParser_PopCodes( self, front, back );
				DaoParser_PopRegisters( self, k-1 );
				DaoParser_AddCode( self, DVM_CASETAG, cst, 0, 0, last, 0, colon );
				last = comma + 1;
				comma = DaoParser_FindOpenToken( self, DTOK_COMMA, last, colon, 0 );
				if( comma < 0 ) comma = colon;
			}
			DaoParser_AddCode( self, DVM_UNUSED, 0, 0, 0, start,0,0 );
			start = colon + 1;
			continue;
		case DKEY_DEFAULT :
			DaoParser_AddCode( self, DVM_DEFAULT, 0, self->vmcCount,
					tokens[start-1]->name == DTOK_COLON, start,0,0 );
			start += 2;
			continue;
		case DKEY_TRY :
			DaoParser_AddCode( self, DVM_TRY, 0, self->vmcCount, DVM_TRY, start,0,0 );
			if( tokens[start+1]->name != DTOK_LCB) DaoParser_AddScope( self, DVM_UNUSED, start );
			start += 1;
			continue;
		case DKEY_RETRY :
			DaoParser_AddCode( self, DVM_RETRY, 0, self->vmcCount, DVM_RETRY, start,0,0 );
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			start += 1;
			continue;
		case DKEY_BREAK :
			DaoParser_AddCode( self, DVM_BREAK, 0, self->vmcCount, DVM_BREAK, start,0,0 );
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			start += 1;
			continue;
		case DKEY_SKIP : case DKEY_CONTINUE :
			DaoParser_AddCode( self, DVM_SKIP, 0, self->vmcCount, DVM_SKIP, start,0,0 );
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			start += 1;
			continue;
		case DKEY_RESCUE : case DKEY_CATCH : case DKEY_RAISE :
			temp = tki == DKEY_RAISE ? DVM_RAISE : DVM_RESCUE;
			end = start;
			N = 0;
			if( temp == DVM_RESCUE ){
				if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
				if( tokens[start+1]->name == DTOK_LB ){
					/* rescue( ... ) */
					end = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
					if( end <0 ) return 0;
					start ++;
					reg = DaoParser_MakeArithArray( self, start+1, end-1, & N, & cst, DTOK_COMMA, 0, NULL, 0 );
				}else if( tokens[start+1]->name == DTOK_LCB ){
					/* rescue */
					reg = 1;
					end = start;
				}else{
					DaoParser_Error3( self, DAO_INVALID_STATEMENT, errorStart );
					return 0;
				}
			}else{
				end = DaoParser_FindPhraseEnd( self, start + 1, to );
				if( end < 0 ) return 0;
				reg = DaoParser_MakeArithArray( self, start+1, end, & N, & cst, DTOK_COMMA, 0, NULL, 0 );
			}
			if( reg < 0 ) return 0;

			DaoParser_AddCode( self, temp, reg, N+1, 0, start, 0, end );
			if( temp ==DVM_RAISE && DaoParser_CompleteScope( self, start ) ==0) return 0;
			start = end + 1;
			continue;
		}

		if( tokens[start]->name == DTOK_LCB ){
			int rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, -1 );
			if( rb < 0 ) return 0;
			if( rb==to || ( tokens[rb+1]->name != DTOK_ASSN && tokens[rb+1]->name != DTOK_CASSN ) ){
				int code = self->vmcLast->code;
				if( code != DVM_IF && code != DVM_ELIF && code != DVM_ELSE
						&& code != DVM_WHILE && code != DVM_FOR && code != DVM_DO
						&& code != DVM_SWITCH && code != DVM_TRY && code != DVM_RESCUE ){
					if( start +1 == rb ){
						reg = DaoParser_MakeArithTree( self, start, rb, & cst, -1, 0 );
						if( cst ) DaoParser_GetNormRegister( self, cst, start, 0, rb );
						start = rb +1;
						continue;
					}else{
						end = DaoParser_FindPhraseEnd( self, start+1, to );
						if( end+1 == rb || tokens[end+1]->name == DTOK_COMMA ){
							reg = DaoParser_MakeArithTree( self, start, rb, & cst, -1, 0 );
							if( cst ) DaoParser_GetNormRegister( self, cst, start, 0, rb );
							start = rb +1;
							continue;
						}
					}
				}
				DaoParser_AddScope( self, DVM_LBRA, start );
				start++;
				continue;
			}
		}else if( tokens[start]->name == DTOK_RCB ){
			if( DaoParser_DelScope( self, DVM_LBRA, start ) == 0 ) return 0;
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			start++;
			continue;
		}

		/* Parse: var1,var2,var3; global var1,var2; */
		end = DaoParser_FindPhraseEnd( self, start, to );
		if( end < 0 ) return 0;

#if 0
		int mm;
		for(mm=start; mm<=end; mm++) printf( "%s ", tokens[mm]->string->mbs );
		printf("\n");
#endif

		if( tokens[start]->name == DKEY_RETURN ){
			reg = DaoParser_MakeArithArray( self, start+1, end, & N, & cst,
					DTOK_COMMA, 0, NULL, 0 );
			if( reg < 0 ) return 0;

			DaoParser_AddCode( self, DVM_RETURN, reg, N, 0, start, 0, end );
			start = end + 1;
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			continue;
		}
		if( ptok->type != DTOK_IDENTIFIER && ptok->type != DTOK_LB ){
			reg = DaoParser_MakeArithTree( self, start, end, & cst, -1, 0 );
			if( reg < 0 ) return 0;
			start = end + 1;
			continue;
		}

		decStart = expStart = start;
		decEnd = -1;
		expEnd = end;
		abtp = NULL;
		decl = decl2 = 0;
		colon = DaoParser_FindOpenToken( self, DTOK_COLON, start, end, 0 );
		eq = DaoParser_FindOpenToken( self, DTOK_ASSN, start, end, 0 );
		if( eq < 0 ){
			eq = DaoParser_FindOpenToken( self, DTOK_CASSN, start, end, 0 );
			if( eq >=0 && colon >=0 ){
				DaoParser_Error2( self, DAO_TOKENS_NOT_EXPECTED, colon, eq-1, 0 );
				DaoParser_Error2( self, DAO_INVALID_STATEMENT, errorStart, end, 0 );
				return 0;
			}
			if( eq >=0 ) abtp = DaoNameSpace_MakeType( myNS, "any", DAO_ANY, 0,0,0 );
		}

		/* POSITIVE:
		 * var;
		 * var1, var2;
		 * var : type;
		 * var1, var2 ; type;
		 * var : type = value;
		 * var1, var2 : type = value;
		 *
		 * NEGATIVE:
		 * var1 + var2;
		 * var1.meth();
		 * var = e1 ? e2 : e3;
		 * ( e1, e2 ) = ( ... );
		 */
		tki = 0;
		temp = 0;
		if( start < to ){
			tki = tokens[start+1]->name;
			temp = tokens[start+1]->line;
		}
		if( start == end || temp > tokens[start]->line
				|| tki == DTOK_COMMA || tki == DTOK_COLON || tki == DTOK_ASSN
				|| tki == DTOK_CASSN || tki == DTOK_SEMCO ){
			decl2 = 1;
			if( eq >= 0 && colon >= 0 && colon < eq ){
				decl = 1;
				abtp = DaoParser_ParseType( self, colon+1, end, & temp2, NULL );
				if( abtp == NULL ) return 0;
				decEnd = colon - 1;
				expStart = temp2 + 1;
				eq = tokens[temp2]->name == DTOK_ASSN || tokens[temp2]->name == DTOK_CASSN ? temp2 : -1 ;
				if( temp2 < end && eq <0 ){
					DaoParser_Error2( self, DAO_INVALID_STATEMENT, errorStart, end, 0 );
					return 0;
				}
			}else if( eq >= 0 ){
				decEnd = eq -1;
				expStart = eq + 1;
			}else if( colon >= 0 ){
				decl = 1;
				decEnd = colon - 1;
				abtp = DaoParser_ParseType( self, colon+1, end, & temp2, NULL );
				if( abtp == NULL ) return 0;
			}else{
				decEnd = end;
				abtp = DaoNameSpace_MakeType( myNS, "any", DAO_ANY, 0,0,0 );
			}
			if( decl && end+1<=to && tokens[end]->type == DTOK_IDENTIFIER
					&& tokens[end+1]->type == DTOK_IDENTIFIER
					&& tokens[end+1]->line == tokens[end]->line ){
				DaoParser_Error2( self, DAO_INVALID_STATEMENT, errorStart, end, 0 );
				return 0;
			}
		}
		if( expEnd < to && tokens[expEnd+1]->line == tokens[expEnd]->line ){
			temp = tokens[expEnd+1]->name;
			if( temp < DKEY_USE && temp != DTOK_COMMA && temp != DTOK_SEMCO && temp != DTOK_RCB ){
				DString_SetMBS( mbs, "it is recommend to seperate two statements properly" );
				DaoParser_Warn( self, DAO_CTW_NULL, mbs );
			}
		}
		if( abtp && abtp->tid != DAO_ANY ) DArray_PushFront( self->enumTypes, abtp );

#if 0
		printf( "start %i %s\n", start, tokens[start]->string->mbs );
		printf( "decStart %i %s\n", decStart, tokens[decStart]->string->mbs );
		printf( "decEnd %i %s\n", decEnd, decEnd>=0 ? tokens[decEnd]->string->mbs : "" );
		printf( "expStart %i %s\n", expStart, tokens[expStart]->string->mbs );
		printf( "expEnd %i %s\n", expEnd, tokens[expEnd]->string->mbs );
#endif

		reg = -1;
		cst = 0;
		front = self->vmcFirst;
		back = self->vmcLast;
		/* expStart should be one token before the expression */
		if( decl2 && expStart <= expEnd && eq >= 0 ){
			reg = DaoParser_MakeArithTree( self, expStart, expEnd, & cst, -1, 0 );
			if( reg < 0 ){
				DaoParser_Error2( self, DAO_INVALID_STATEMENT, start, end, 0 );
				return 0;
			}
		}
		value = nil;
		if( cst ){
			value = DaoParser_GetVariable( self, cst );
			if( value.t >= DAO_ARRAY ) value.v.p->trait |= DAO_DATA_CONST;
		}
		if( abtp ==0 && value.t ) abtp = DaoNameSpace_GetTypeV( myNS, value );

		DArray_Clear( self->toks );
		for( i=decStart; i<=decEnd; i+=2 ){
			int var;
			/*
			   printf( "var = %s, %s %i\n", tokens[i]->string->mbs, tokens[i+1]->string->mbs, storeType );
			 */
			DaoToken *vtok = tokens[i];
			str = tokens[i]->string;
			if( tokens[i]->type != DTOK_IDENTIFIER ){
				DaoParser_Error( self, DAO_TOKEN_NEED_NAME, str );
				return 0;
			}
			if( i+1 <= decEnd && tokens[i+1]->name != DTOK_COMMA ){
				DaoParser_Error( self, DAO_INVALID_EXPRESSION, str );
				return 0;
			}
			DArray_Append( self->toks, vtok );
			var = DaoParser_GetRegister( self, vtok );
			if( self->error ) return 0;
			if( var < 0 ){
				DaoParser_DeclareVariable( self, vtok, storeType, abtp );
			}else if( ( storeType & DAO_DATA_MEMBER ) && self->hostClass ){
				/* class Klass( var ){ my var; self.var = var; } */
				DaoClass *klass = self->hostClass;
				DNode *node = DMap_Find( klass->deflines, str );
				if( node == NULL ){
					DaoParser_DeclareVariable( self, vtok, storeType, abtp );
				}else{
					DaoParser_Error( self, DAO_SYMBOL_WAS_DEFINED, str );
					return 0;
				}
			}else if( decl ){
				DaoParser_Error( self, DAO_SYMBOL_WAS_DEFINED, str );
				return 0;
			}
		}
		if( expStart > to ) break;
		if( tokens[expStart]->name == DTOK_SEMCO ){
			if( DaoParser_CompleteScope( self, expStart ) == 0 ) return 0;
			start = expStart + 1;
			continue;
		}
		/* if( abtp ) printf( "abtp = %s\n", abtp->name->mbs ); */

		/*
		   printf( "==============================%s %i %i\n", varTok->mbs, nVarDec, reg );
		 */
		str = tokens[expStart]->string;
		if( self->toks->size ==0 ){
			int tpos = tokens[expStart]->line;
			if( tokens[expStart]->name == DTOK_LB && eq >=0 ){
				int id, comma = DaoParser_FindOpenToken( self, DTOK_COMMA, expStart+1, eq, 0 );
				DaoToken *exptok;
				if( comma < 0 ) comma = eq -1;
				reg = DaoParser_MakeArithTree( self, eq+1, expEnd, & cst, -1, 0 );
				if( reg < 0 ){
					DaoParser_Error2( self, DAO_INVALID_STATEMENT, start, end, 0 );
					return 0;
				}
				k = 0;
				expStart ++;
				exptok = tokens[expStart];
				str = exptok->string;
				while( comma >= 0 ){
					sprintf( buffer, "%i", k );
					DString_SetMBS( mbs, buffer );
					if( MAP_Find( self->allConsts, mbs ) ==NULL ){
						MAP_Insert( self->allConsts, mbs, routine->routConsts->size );
						value.t = DAO_INTEGER;
						value.v.i = k;
						DRoutine_AddConstValue( (DRoutine*)routine, value );
					}
					id = LOOKUP_BIND_LC( MAP_Find( self->allConsts, mbs )->value.pInt );
					id = DaoParser_GetNormRegister( self, id, expStart, 0, expStart+1 );
					/* 
					   printf( "name = %s,  %i\n", str->mbs, id );
					 */
					if( expStart == comma ){
						DaoParser_Error( self, DAO_INVALID_EXPRESSION, str );
						return 0;
					}else if( expStart == comma -1 ){
						int regC = DaoParser_GetRegister( self, exptok );
						int st = LOOKUP_ST( regC );
						int up = LOOKUP_UP( regC );
						int ic = LOOKUP_ID( regC );
						int it = self->locRegCount;
						if( DaoToken_IsValidName( str->mbs, str->size )==0 ){
							DaoParser_Error( self, DAO_TOKEN_NEED_NAME, str );
							return 0;
						}
						if( regC <0 ){
							DaoParser_DeclareVariable( self, tokens[expStart], 0, NULL );
							regC = DaoParser_GetRegister( self, exptok );
							st = LOOKUP_ST( regC );
							up = LOOKUP_UP( regC );
							ic = LOOKUP_ID( regC );
						}
						switch( st ){
						case DAO_LOCAL_VARIABLE :
							if( up ){
								DaoParser_PushRegister( self );
								DaoParser_AddCode( self, DVM_GETI, reg, id, it, expStart, eq, end );
								DaoParser_AddCode( self, DVM_SETVL, it, ic, up, expStart, eq, end );
							}else{
								DaoParser_AddCode( self, DVM_GETI, reg, id, ic, expStart, eq, end );
							}
							break;
						case DAO_OBJECT_VARIABLE :
							DaoParser_PushRegister( self );
							DaoParser_AddCode( self, DVM_GETI, reg, id, it, expStart, eq, end );
							DaoParser_AddCode( self, DVM_SETVO, it, ic, 0, expStart, eq, end );
							break;
						case DAO_CLASS_VARIABLE :
							DaoParser_PushRegister( self );
							DaoParser_AddCode( self, DVM_GETI, reg, id, it, expStart, eq, end );
							DaoParser_AddCode( self, DVM_SETVK, it, ic, up, expStart, eq, end );
							break;
						case DAO_GLOBAL_VARIABLE :
							DaoParser_PushRegister( self );
							DaoParser_AddCode( self, DVM_GETI, reg, id, it, expStart, eq, end );
							DaoParser_AddCode( self, DVM_SETVG, it, ic, up, expStart, eq, end );
							break;
						default:
							DaoParser_Error2( self, DAO_EXPR_MODIFY_CONSTANT, expStart, comma-1, 0 );
							return 0;
						}
					}else{
						int regC = DaoParser_MakeArithTree( self, expStart, comma -1, & cst, -1, 0 );
						DaoInode *vmc = self->vmcLast;
						if( regC <0 || cst || vmc == self->vmcBase
								|| ( vmc->code != DVM_GETI && vmc->code != DVM_GETF
									&& vmc->code != DVM_GETMF ) ){
							DaoParser_Error2( self, DAO_INVALID_EXPRESSION, expStart, comma-1, 0 );
							return 0;
						}
						regC = vmc->c;
						DaoParser_AddCode( self, DVM_GETI, reg, id, regC, expStart, eq, end );
						DaoParser_AddCode( self, DVM_GETI, regC, vmc->b, vmc->a, expStart, eq, end );
						self->vmcLast->code = DVM_SETI + (vmc->code - DVM_GETI);
						vmc->code = DVM_UNUSED;
					}
					k ++;
					expStart = comma + 1;
					exptok = tokens[expStart];
					str = exptok->string;
					if( comma < eq-1 ){
						comma = DaoParser_FindOpenToken( self, DTOK_COMMA, expStart, eq, 0 );
						if( comma < 0 ) comma = eq -1;
					}else{
						comma = -1;
					}
				}
			}else{
				self->warnAssn = 0;
				if( DaoParser_MakeArithTree( self, expStart, expEnd, & cst, -1, 0 ) <0 ){
					if( expStart != start || expEnd != end )
						DaoParser_Error2( self, DAO_INVALID_STATEMENT, start, end, 0 );
					return 0;
				}
			}
			if( DaoParser_CompleteScope( self, expEnd ) == 0 ) return 0;
		}else if( eq >= 0 ){
			int tpos = tokens[expStart]->line;
			int remove = 1;
			if( reg < 0 ){
				DaoParser_Error2( self, DAO_INVALID_EXPRESSION, expStart, expEnd, 0 );
				return 0;
			}else if( storeType & DAO_DATA_CONST ){
				if( ! self->isDynamicClass && ! cst ){
					DaoParser_Error2( self, DAO_EXPR_NEED_CONST_EXPR, eq + 1, end, 0 );
					return 0;
				}
			}
			for(k=0; k<self->toks->size; k++){
				DaoToken *varTok = self->toks->items.pToken[k];
				int id = 0;
				if( storeType & DAO_DATA_CONST ){
					if( storeType & DAO_DATA_GLOBAL ){
						DaoNameSpace *ns = ( storeType & DAO_DATA_GLOBAL )
							? myNS : vmSpace->nsInternal;
						id = DaoNameSpace_FindConst( ns, varTok->string );
						if( id < 0 ){
							DaoParser_Error( self, DAO_SYMBOL_NOT_DEFINED, varTok->string );
							return 0;
						}
						DaoNameSpace_SetConst( ns, id, value );
					}else if( self->isClassBody && hostClass ){
						if( cst ){
							id = DaoClass_FindConst( hostClass, varTok->string );
							DaoClass_SetConst( hostClass, id, value );
						}else if( self->isDynamicClass ){
							MAP_Insert( hostClass->protoValues, reg, varTok->string );
						}else{
							DaoParser_Error2( self, DAO_EXPR_NEED_CONST_EXPR, eq + 1, end, 0 );
							return 0;
						}
					}else{
						id = LOOKUP_ID( DaoParser_GetRegister( self, varTok) );
						DValue_SimpleMove( value, routine->routConsts->data + id );
						DValue_MarkConst( & routine->routConsts->data[id] );
					}
				}else{
					int regC = DaoParser_GetRegister( self, varTok );
					int st = LOOKUP_ST( regC );
					int up = LOOKUP_UP( regC );
					int id = LOOKUP_ID( regC );
					int isov = st == DAO_OBJECT_VARIABLE;
					int isdecl = self->isClassBody && (storeType & DAO_DATA_MEMBER);
					int first = varTok->index;
					remove = 0;
					switch( st ){
					case DAO_LOCAL_VARIABLE :
						if( up ){
							DaoParser_AddCode( self, DVM_SETVL, reg, id, up, first, eq, end );
						}else{
							DaoParser_AddCode( self, DVM_MOVE, reg, 0, id, first, eq, end );
						}
						break;
					case DAO_OBJECT_VARIABLE :
						if( isdecl && cst ){
							DaoType *tp1 = hostClass->objDataType->items.pType[ id ];
							if( tp1 && DaoType_MatchValue( tp1, value, 0 ) ==0 ){
								DaoType *tp2 = DaoNameSpace_GetTypeV( myNS, value );
								self->curLine = tokens[expStart]->line;
								DaoParser_Error( self, DAO_TYPE_PRESENTED, tp2->name );
								self->curLine = tokens[decStart]->line;
								DaoParser_Error( self, DAO_TYPE_EXPECTED, tp1->name );
								DaoParser_Error2( self, DAO_TYPE_NOT_MATCHING, decStart, end, 0 );
								return 0;
							}
							remove = 1;
							DValue_SimpleMove( value, hostClass->objDataDefault->data + id );
							DValue_MarkConst( hostClass->objDataDefault->data + id );
						}else if( isdecl && self->isDynamicClass ){
							MAP_Insert( hostClass->protoValues, reg, varTok->string );
						}else if( ! self->isClassBody ){
							DaoParser_AddCode( self, DVM_SETVO, reg, id, 0, first, eq, end );
						}else{
							DaoParser_Error2( self, DAO_EXPR_NEED_CONST_EXPR, eq + 1, end, 0 );
							return 0;
						}
						break;
					case DAO_CLASS_VARIABLE :
						if( isdecl && cst ){
							DaoType *type = hostClass->glbTypeTable->items.pArray[up]->items.pType[id];
							DValue *data = hostClass->glbDataTable->items.pVarray[up]->data + id;
							if( data->t == DAO_NIL ){
								DValue_Move( value, data, type );
								remove = 1;
							}
						}else if( isdecl && self->isDynamicClass ){
							MAP_Insert( hostClass->protoValues, reg, varTok->string );
						}else if( ! self->isClassBody ){
							DaoParser_AddCode( self, DVM_SETVK, reg, id, up, first, eq, end );
						}else{
							DaoParser_Error2( self, DAO_EXPR_NEED_CONST_EXPR, eq + 1, end, 0 );
							return 0;
						}
						break;
					case DAO_GLOBAL_VARIABLE :
						DaoParser_AddCode( self, DVM_SETVG, reg, id, up, first, eq, end );
						break;
					default :
						DaoParser_Error( self, DAO_EXPR_MODIFY_CONSTANT, varTok->string );
						return 0;
					}
				}
			}
			if( cst && remove ){
				k = DaoParser_PopCodes( self, front, back );
				DaoParser_PopRegisters( self, k-1 );
			}
			if( DaoParser_CompleteScope( self, expEnd ) == 0 ) return 0;
		}

		storeType = 0;
		start = expEnd + 1;
		if( start <= to && tokens[start]->name == DTOK_SEMCO ){
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			start ++;
		}
	}
	return 1;
}
void DaoParser_SetupBranching( DaoParser *self )
{
	DaoInode *it, *it2;
	int rm, id;
	if( self->vmcLast->code != DVM_RETURN ){
		int first = self->vmcLast->first + self->vmcLast->middle + self->vmcLast->last + 1;
		if( self->vmSpace->options & DAO_EXEC_IDE ){
			/* for setting break points in DaoStudio */
			DaoParser_AddCode( self, DVM_NOP, 0, 0, 0, first,0,0 );
		}
		DaoParser_AddCode( self, DVM_RETURN, 0, 0, 0, first,0,0 );
	}

	it = self->vmcFirst;
	rm = 0, id = 0;
	while( it ){
		it->id = id ++;
		it = it->next;
	}
	it = self->vmcFirst;
	/*
	   DaoParser_PrintCodes( self );
	 */
	while( it ){
		if( it->code == DVM_SKIP ){
			int code = it->jumpTrue->code;
			if( code == DVM_FOR ){
				it->jumpTrue = it->jumpTrue->jumpTrue->jumpTrue;
				it->b = it->jumpTrue->id + 1;
			}else if( code == DVM_WHILE || code == DVM_SWITCH ){
				it->jumpTrue = it->jumpTrue->jumpTrue;
				it->b = it->jumpTrue->id + 1;
			}else if( code == DVM_DO ){
				it->b = it->jumpTrue->jumpTrue->id + 1;
				/* needed for updating ::b after removing unused codes: */
				it->jumpTrue = it->jumpTrue->jumpTrue;
			}
			it->code = DVM_GOTO;
		}
		it = it->next;
	}
	it = self->vmcFirst;
	while( it ){
		/*
		DaoInode_Print( it );
		*/
		it->extra = 0;
		switch( it->code ){
		case DVM_GOTO :
			if( it->b == it->id + 1 ) it->code = DVM_UNUSED;
			break;
		case DVM_SWITCH :
			it->b = it->jumpTrue->id + 1; /* default */
			break;
		case DVM_CASE :
			if( it->jumpTrue ){
				it2 = it->jumpTrue;
				while( it2->code == DVM_CASETAG ){
					it->jumpTrue = it2;
					it2 = it2->next;
				}
				it->b = it2->id;
			}else{
				it->b = it->jumpFalse->id + 1; /* filled cases */
				it->jumpTrue = it->jumpFalse;
			}
			break;
		case DVM_IF :
		case DVM_ELIF :
			it2 = it->jumpFalse;
			it->code = DVM_TEST;
			it->b = it2->id + 1;
			it2->code = DVM_GOTO;
			it2->b = it->jumpTrue->id + 1;

			it2->jumpTrue = it->jumpTrue;
			it->jumpTrue = it2;
			break;
		case DVM_DO :
			it->code = DVM_NOP;
			break;
		case DVM_FOR :
		case DVM_WHILE :
			if( it->code == DVM_FOR && it->c ){
				it->code = DVM_UNUSED;
			}else{
				it->code = DVM_TEST;
			}
			it->b = it->jumpFalse->id + 1;
			it->jumpFalse->code = DVM_GOTO;
			it->jumpFalse->b = it->jumpTrue->id + 1;

			it->jumpFalse->jumpTrue = it->jumpTrue;
			it->jumpTrue = it->jumpFalse;
			break;
		case DVM_UNTIL :
			it->code = DVM_TEST;
			it->b = it->jumpTrue->id;
			break;
		case DVM_BREAK :
			it->code = DVM_GOTO;
			it->b = it->jumpTrue->jumpFalse->id + 1;
			it->jumpTrue = it->jumpTrue->jumpFalse;
			break;
		case DVM_SKIP :
			break;
		case DVM_CASETAG :
		case DVM_DEFAULT :
			it->code = DVM_GOTO;
			it->b = it->jumpTrue->jumpFalse->id + 1;
			it->jumpTrue = it->jumpTrue->jumpFalse;
			id = it->next->code;
			if( id == DVM_CASETAG || id == DVM_DEFAULT ) it->code = DVM_UNUSED;
			break;
		case DVM_TRY :
		case DVM_RESCUE :
			if( it->code == DVM_TRY ) it->a = it->b = 0;
			it->code = DVM_CRRE;
			it->jumpFalse->code = DVM_GOTO;
			it->jumpFalse->b = it->jumpTrue->id + 1;
			it->jumpFalse->jumpTrue = it->jumpTrue;
			it->c = it->jumpFalse->id + 1;
			it->jumpTrue = it->jumpFalse;
			break;
		case DVM_RAISE :
			it->code = DVM_CRRE;
			break;
		case DVM_RETRY :
			it->code = DVM_GOTO;
			it->b = it->jumpTrue->id;
			break;
		case DVM_SCBEGIN :
			it->code = DVM_GOTO;
			it->b = it->jumpTrue->id + 1;
			break;
		case DVM_SCEND :
			it->code = DVM_RETURN;
			it->a = it->b;
			it->b = 0;
			it->c = 1;
			break;
		case DVM_LBRA :
		case DVM_LBRA2 :
			/* NOP for checking cancellation */
			it->code = it->prev->code == DVM_TEST ? DVM_NOP : DVM_UNUSED;
			break;
		case DVM_RBRA :
		case DVM_RBRA2 :
		case DVM_ELSE : case DVM_WHILE_AUX :
		case DVM_FOR_STEP : case DVM_FOR_AUX :
			it->code = DVM_UNUSED;
			break;
		default : break;
		}
		it = it->next;
	}
	/* DaoParser_PrintCodes( self ); */
	it = self->vmcFirst;
	while( it ){
		/* DaoInode_Print( it ); */
		if( it->prev ) it->extra = it->prev->extra;
		if( it->code >= DVM_UNUSED ) it->extra ++;
		it = it->next;
	}
	it = self->vmcFirst;
	while( it ){
		it->id -= it->extra;
		switch( it->code ){
		case DVM_GOTO : case DVM_TEST :
		case DVM_SWITCH : case DVM_CASE :
			it->b -= it->jumpTrue->extra;
			break;
		case DVM_CRRE :
			if( it->c ) it->c -= it->jumpTrue->extra;
			break;
		default : break;
		}
		if( it->code >= DVM_UNUSED ) rm ++;
		it = it->next;
	}
	DArray_Clear( self->vmCodes );
	it = self->vmcFirst;
	while( it ){
		if( it->code != DVM_UNUSED ) DArray_Append( self->vmCodes, (DaoVmCodeX*) it );
		it = it->next;
	}
#if 0
	//self->locRegCount ++; /* TODO: check */
#endif
	DaoParser_PushRegister( self );
}
int DaoParser_ParseRoutine( DaoParser *self )
{
	DaoNameSpace *myNS = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DaoInode *it, *it2;
	const int tokChrCount = self->tokens->size;
	int rm, id, defLine = routine->defLine;

	if( self->parsed ) return 1;
	GC_ShiftRC( myNS, routine->nameSpace );
	routine->nameSpace = myNS;

	self->parsed  = 1;
	self->error = 0;

	if( routine->routName->mbs[0] == '@' && routine->routType->nested->size ){
		DaoType *ftype = routine->routType->nested->items.pType[0];
		DaoToken tok = { DTOK_IDENTIFIER, DTOK_IDENTIFIER, 0, 0, 0, NULL };
		if( ftype->tid == DAO_PAR_NAMED && ftype->X.abtype->tid == DAO_ROUTINE ){
			DMap *names;
			DNode *it;
			ftype = ftype->X.abtype;
			names = ftype->mapNames;
			assert( routine->parCount == self->locRegCount );
			for(id=0,it=DMap_First(names); it; it=DMap_Next(names,it),id++){
				DaoType *tp = ftype->nested->items.pType[it->value.pInt];
				tok.string = it->key.pString;
				if( tp->tid == DAO_PAR_NAMED || tp->tid == DAO_PAR_DEFAULT )
					tp = tp->X.abtype;
				DaoParser_DeclareVariable( self, & tok, 0, tp );
				self->locRegCount = routine->parCount + (id+1);
			}
		}
	}

	if( DaoParser_ParseCodeSect( self, 0, tokChrCount-1 )==0 ){
		DaoParser_PrintError( self, 0, 0, NULL );
		return 0;
	}
	routine->defLine = defLine;

	if( self->error ) return 0;
	return DaoParser_PostParsing( self );
}
static int DaoParser_NullValue( DaoParser *self, int start )
{
	if( self->nullValue >= 0 ) return self->nullValue;
	self->nullValue = self->locRegCount;
	DaoParser_AddCode( self, DVM_DATA, DAO_NIL, 0, self->nullValue, start,start+1,0 );
	DaoParser_PushRegister( self );
	return self->nullValue;
}
static int DaoParser_IntegerZero( DaoParser *self, int start )
{
	/* can not reuse the data, because it might not be executed! */
	/* if( self->integerZero >= 0 ) return self->integerZero; */
	self->integerZero = self->locRegCount;
	DaoParser_AddCode( self, DVM_DATA, DAO_INTEGER, 0, self->integerZero, start,start+1,0 );
	DaoParser_PushRegister( self );
	return self->integerZero;
}
static int DaoParser_IntegerOne( DaoParser *self, int start )
{
	/* if( self->integerOne >= 0 ) return self->integerOne; */
	self->integerOne = self->locRegCount;
	DaoParser_AddCode( self, DVM_DATA, DAO_INTEGER, 1, self->integerOne, start,start+1,0 );
	DaoParser_PushRegister( self );
	return self->integerOne;
}
static int DaoParser_ImaginaryOne( DaoParser *self, int start )
{
	/* if( self->imaginaryOne >= 0 ) return self->imaginaryOne; */
	self->imaginaryOne = self->locRegCount;
	DaoParser_AddCode( self, DVM_DATA, DAO_COMPLEX, 1, self->imaginaryOne, start,0,0 );
	DaoParser_PushRegister( self );
	return self->imaginaryOne;
}
void DaoParser_DeclareVariable( DaoParser *self, DaoToken *tok, int storeType, DaoType *abtp )
{
	DaoNameSpace *nameSpace = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DString *name = tok->string;
	int perm = self->permission;
	int found;

	if( self->isInterBody ){
		DaoParser_Error3( self, DAO_VARIABLE_OUT_OF_CONTEXT, tok->index );
		return;
	}

	if( storeType & DAO_DATA_LOCAL ){
		if( MAP_Find( DArray_Top( self->localVarMap ), name ) == NULL ){
			int id = self->locRegCount;
			MAP_Insert( self->routine->localVarType, id, abtp );
			MAP_Insert( DArray_Top( self->localVarMap ), name, id );
			DaoParser_PushRegister( self );
		}
	}else if( storeType & DAO_DATA_MEMBER ){
		if( self->hostClass ){
			DaoClass *hostClass = self->hostClass;
			DNode *node = MAP_Find( hostClass->deflines, name );
			if( node ){
				DaoParser_Warn( self, DAO_SYMBOL_WAS_DEFINED, name );
				return;
			}else{
				if( self->isClassBody ){
					int ln = tok->line;
					int ec = 0;
					if( storeType & DAO_DATA_CONST ){
						ec = DaoClass_AddConst( hostClass, name, daoNullValue, perm, ln );
					}else if( storeType & DAO_DATA_STATIC ){
						ec = DaoClass_AddGlobalVar( hostClass, name, daoNullValue, abtp, perm, ln );
					}else{
						ec = DaoClass_AddObjectVar( hostClass, name, daoNullValue, abtp, perm, ln );
						routine->attribs |= DAO_ROUT_NEEDSELF;
					}
					if( ec ) DaoParser_Warn( self, ec, name );
				}else{
					DaoParser_Warn( self, DAO_VARIABLE_OUT_OF_CONTEXT, name );
				}
			}
		}else{
			DaoParser_Warn( self, DAO_VARIABLE_OUT_OF_CONTEXT, name );
		}
	}
	found = DaoParser_GetRegister( self, tok );
	if( found >= 0 ) return;

	if( ( storeType & DAO_DATA_GLOBAL ) && ( storeType & DAO_DATA_CONST) ){
		DaoNameSpace_AddConst( nameSpace, name, daoNullValue, perm );
	}else if( storeType & DAO_DATA_GLOBAL ){
		DaoNameSpace_AddVariable( nameSpace, name, daoNullValue, abtp, perm );
	}else{
		int id = 0;
		DArray_Append( self->routine->defLocals, tok );
		if( storeType & DAO_DATA_CONST ){
			id = routine->routConsts->size;
			MAP_Insert( DArray_Top( self->localCstMap ), name, id );
			DaoRoutine_AddConstValue( routine, daoNullValue );
		}else{
			id = self->locRegCount;
			MAP_Insert( self->routine->localVarType, id, abtp );
			MAP_Insert( DArray_Top( self->localVarMap ), name, id );
			DaoParser_PushRegister( self );
		}
		tok = (DaoToken*) DArray_Back( self->routine->defLocals );
		DaoToken_Set( tok, !(storeType & DAO_DATA_CONST), self->lexLevel, id, NULL );
	}
}
int DaoParser_GetRegister( DaoParser *self, DaoToken *nametok )
{
	DaoNameSpace *ns = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DString *name = nametok->string;
	DNode *node = NULL;
	int i;

	if( self->hostCData ){
		/* QStyleOption( version : int = QStyleOption::Version, ... ) */
		DValue it = DaoFindValueOnly( self->hostCData->typer, name );
		if( it.t ){
			i = routine->routConsts->size;
			MAP_Insert( DArray_Top( self->localCstMap ), name, i );
			DaoRoutine_AddConstValue( routine, it );
			return LOOKUP_BIND_LC( i );
		}
	}
	if( self->varFunctional->size ){ /* compiling functional blocks */
		node = MAP_Find( self->varFunctional, name );
		if( node ) return node->value.pInt;
	}

	/* Look for local variable: */
	for( i=self->lexLevel; i>=0; i-- ){
		node = MAP_Find( self->localVarMap->items.pMap[i], name );
		if( node ) return node->value.pInt;
	}
	/* Look for local constant: */
	for( i=self->lexLevel; i>=0; i-- ){
		node = MAP_Find( self->localCstMap->items.pMap[i], name );
		if( node ) return LOOKUP_BIND_LC( node->value.pInt );
	}

	/* Look for variable in class: */
	if( self->hostClass && (node = MAP_Find( self->hostClass->lookupTable, name )) ){
		int id = LOOKUP_ID( node->value.pSize );
		int st = LOOKUP_ST( node->value.pSize );
		if( st == DAO_OBJECT_VARIABLE ) routine->attribs |= DAO_ROUT_NEEDSELF;
		return node->value.pSize;
	}

	if( (i = DaoNameSpace_FindVariable( ns, name )) >= 0 ) return i;

	if( ( node = MAP_Find( self->allConsts, name ) ) )
		return LOOKUP_BIND_LC( node->value.pInt );

	if( (i = DaoNameSpace_FindConst( ns, name )) >= 0 ) return i;

	if( self->outParser ){ /* search upvalues before globals/class members??? */
		int st, up, id;
		i = DaoParser_GetRegister( self->outParser, nametok );
		st = LOOKUP_ST( i );
		up = LOOKUP_UP( i );
		id = LOOKUP_ID( i );
		if( st > DAO_LOCAL_CONSTANT && st < DAO_GLOBAL_VARIABLE ){
			DaoParser_Error( self, DAO_CTW_INVA_SYNTAX, name );
			DaoParser_Suggest( self,
					"only can access up-level local constants or variables" );
			return -1;
		}
		if( i >=0 ){
			routine->upRoutine = self->outParser->routine;
			GC_IncRC( routine->upRoutine );
			MAP_Insert( self->routine->localVarType, self->locRegCount, NULL );
			MAP_Insert( DArray_Top( self->localVarMap ), name, self->locRegCount );
			if( st == DAO_LOCAL_VARIABLE ){
				DaoParser_AddCode( self, DVM_GETVL, 1, id, self->locRegCount, nametok->index,0,0 );
			}else{
				DaoParser_AddCode( self, DVM_GETCL, 1, id, self->locRegCount, nametok->index,0,0);
			}
			DaoParser_PushRegister( self );
			return self->locRegCount -1;
		}
	}
	return -1;
}
int DaoParser_GetClassMember( DaoParser *self, DString *name )
{
	DaoRoutine *routine = self->routine;
	DaoClass *hostClass = self->hostClass;
	DNode *node = NULL;

	/* Look for variable in class: */
	if( hostClass == NULL ) return -1;
	node = MAP_Find( hostClass->lookupTable, name );
	if( node == NULL ) return -1;

	switch( LOOKUP_ST( node->value.pSize ) ){
	case DAO_OBJECT_VARIABLE : routine->attribs |= DAO_ROUT_NEEDSELF;
	case DAO_CLASS_VARIABLE : 
	case DAO_CLASS_CONSTANT : return node->value.pSize;
	default : break;
	}
	return -1;
}
DValue DaoParser_GetVariable( DaoParser *self, int reg )
{
	DaoNameSpace *nameSpace = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DaoClass *hostClass = self->hostClass;
	DValue val = daoNullValue;
	int st = LOOKUP_ST( reg );
	int up = LOOKUP_UP( reg );
	int id = LOOKUP_ID( reg );

	if( st == DAO_LOCAL_VARIABLE ){
		if( reg == self->integerZero ){
			val = daoZeroInt;
		}else if( reg == self->integerOne ){
			val = daoZeroInt;
			val.v.i = 1;
		}else if( reg == self->imaginaryOne ){
			val = daoNullComplex;
			val.v.c = & self->imgone;
		}
		return val;
	}
	switch( st ){
	case DAO_LOCAL_CONSTANT :
		val = routine->routConsts->data[id]; break; /*XXX up*/
	case DAO_CLASS_CONSTANT :
		val = hostClass->cstDataTable->items.pVarray[up]->data[id]; break;
	case DAO_GLOBAL_VARIABLE :
		val = nameSpace->varDataTable->items.pVarray[up]->data[id]; break;
	case DAO_GLOBAL_CONSTANT :
		val = nameSpace->cstDataTable->items.pVarray[up]->data[id]; break;
	default : break;
	}

#if 0
	if( hostClass && reg < DVR_MAX )
		return 0;
	else if( reg == DVR_GLB_CST + DVR_NSC_NIL )
		return & nil;
#endif
	return val;
}
int DaoParser_GetNormRegister( DaoParser *self, int reg, int first, int mid, int last )
{
	DaoClass *hostClass = self->hostClass;
	DaoVmCodeX vmc;
	int notide = ! (self->vmSpace->options & DAO_EXEC_IDE);
	int line = self->tokens->items.pToken[first]->line;
	int st = LOOKUP_ST( reg );
	int up = LOOKUP_UP( reg );
	int id = LOOKUP_ID( reg );
	int regc = self->locRegCount;
	/* To support Edit&Continue in DaoStudio,
	 * the DVM_GETC must be put at where it is used,
	 * so that it will not be skipped when the execution point
	 * is changed manually, or a variable is changed to a constant. */

	/* printf( "reg = %x\n", reg ); */
	if( notide && (st & 1) && up ==0 ){
		DaoInode *it = self->vmcLast;
		while( it ){
			int code = it->code, opa = it->a, opb = it->b, opc = it->c;
			it = it->prev;
			if( opa != 0 || opb != id ) continue;
			if( st == DAO_LOCAL_CONSTANT && code == DVM_GETCL ) return opc;
			if( st == DAO_CLASS_CONSTANT && code == DVM_GETCK ) return opc;
			if( st == DAO_GLOBAL_CONSTANT && code == DVM_GETCG ) return opc;
		}
	}
	DaoVmCode_Set( & vmc, 0, up, id, regc, self->lexLevel, line, first, mid, last );
	switch( st ){
	case DAO_LOCAL_VARIABLE :
		if( up ==0 ) return id;
		vmc.code = DVM_GETVL;
		break;
	case DAO_LOCAL_CONSTANT  : vmc.code = DVM_GETCL; break;
	case DAO_OBJECT_VARIABLE : vmc.code = DVM_GETVO; break;
	case DAO_CLASS_VARIABLE  : vmc.code = DVM_GETVK; break;
	case DAO_CLASS_CONSTANT  : vmc.code = DVM_GETCK; break;
	case DAO_GLOBAL_VARIABLE : vmc.code = DVM_GETVG; break;
	case DAO_GLOBAL_CONSTANT : vmc.code = DVM_GETCG; break;
	default : break;
	}
	/*
	   printf( "i = %i %s %i\n", i, getOpcodeName(get), leftval );
	 */
	/* if( set >=0 ) vmc.code = DVM_GETV; */
	DaoParser_PushRegister( self );
	if( notide && (st & 1) && up == 0 ){
		/* constant folding for list/array may pop front vm codes. */
		DaoParser_PushFrontCode( self, & vmc );
	}else{
		DaoParser_PushBackCode( self, & vmc );
	}
	return regc;
}

int DaoParser_PostParsing( DaoParser *self )
{
	DaoRoutine *routine = self->routine;
	DaoVmCodeX **vmCodes, *vmc;
	DNode *node;
	int i, j, k;
	int notide = ! (self->vmSpace->options & DAO_EXEC_IDE);

	DaoRoutine_SetSource( routine, self->tokens, routine->nameSpace );
	DaoParser_SetupBranching( self );

	if( self->bindtos ){
		DArray *fails = DArray_New(0);
		if( DaoInterface_Bind( self->bindtos, fails ) ==0 ){
			k = 0;
			for(i=0; i<self->bindtos->size; i+=5){
				DaoInterface *inter = (DaoInterface*) self->bindtos->items.pBase[i];
				DaoType *type = (DaoType*) self->bindtos->items.pBase[i+1];
				int first = self->bindtos->items.pInt[i+2];
				int last  = self->bindtos->items.pInt[i+3];
				int count = self->bindtos->items.pInt[i+4];
				for(j=0; j<count; j++){
					DRoutine *fail = fails->items.pRout2[j+k];
					DString_Assign( self->mbs, fail->routName );
					DString_AppendMBS( self->mbs, "() " );
					DString_Append( self->mbs, fail->routType->name );
					self->curLine = fail->defLine;
					DaoParser_Error( self, DAO_MISSING_INTERFACE_METHOD, self->mbs );
				}
				k += count;
				self->curLine = routine->source->items.pToken[ first ]->line;
				DString_SetMBS( self->mbs, "type \'" );
				DString_Append( self->mbs, type->name );
				DString_AppendMBS( self->mbs, "\' for interface \'" );
				DString_Append( self->mbs, inter->abtype->name );
				DString_AppendMBS( self->mbs, "\'" );
				DaoParser_Error( self, DAO_INTERFACE_NOT_COMPATIBLE, self->mbs );
				DaoParser_Error2( self, DAO_FAILED_INTERFACE_BIND, first, last, 0 );
			}
			DArray_Delete( fails );
			DaoParser_PrintError( self, 0, 0, NULL );
			return 0;
		}
		DArray_Delete( fails );
	}

	routine->locRegCount = self->locRegCount + 1;
	/*  XXX DaoParser_PushRegister( self ); */

	vmCodes = self->vmCodes->items.pVmc;

	node = DMap_First( self->routine->localVarType );
	for( ; node !=NULL; node = DMap_Next(self->routine->localVarType, node) ){
		/* XXX */
		//DMap_Insert( routine->localVarType, node->key.pVoid, node->value.pVoid );
	}
	if( notide ){
		k = -1;
		for( j=0; j<self->vmCodes->size; j++){
			if( vmCodes[j]->code > DVM_GETCL ){
				k = j;
				break;
			}
		}
		for(j=0; j<k/2; j++){
			vmc = vmCodes[j];
			vmCodes[j] = vmCodes[k-1-j];
			vmCodes[k-1-j] = vmc;
		}
	}
	if( self->hostClass && (self->hostClass->attribs & DAO_CLS_SYNCHRONOUS) ){
		for( j=0; j<self->vmCodes->size; j++){
			DaoVmCodeX *vmc = vmCodes[j];
			int c = vmc->code;
			if( c == DVM_GETVG || c == DVM_SETVG ){
				DaoParser_Error3( self, DAO_INVALID_SHARED, vmc->first );
			}
		}
		if( self->errors->size ){
			DaoParser_PrintError( self, 0, 0, NULL );
			return 0;
		}
	}
	/* DArray_Swap( self->regLines, routine->regLines ); */
	if( DaoRoutine_SetVmCodes( routine, self->vmCodes ) ==0) return 0;
	/*
	   DaoRoutine_PrintCode( routine, self->vmSpace->stdStream );
	 */
	if( routine->parser == NULL ) return 1; /* recursive function */
	if( daoConfig.incompile ) return 1;
	for(i=0; i<self->routCompilable->size; i++){
		DaoRoutine* rout = (DaoRoutine*) self->routCompilable->items.pBase[i];
		/* could be set to null in DaoRoutine_Compile() for recursive routines */
		if( rout->parser == NULL ) continue;
		if( rout->minimal ==1 ) continue;
		if( DaoParser_ParseRoutine( rout->parser ) ==0 ) return 0;
		/* could be set to null in DaoRoutine_Compile() for recursive routines */
		if( rout->parser == NULL ) continue;
		DaoParser_Delete( rout->parser );
		rout->parser = NULL;
	}
	return 1;
}
int DaoParser_ParseLoadStatement( DaoParser *self, int start, int end )
{
	DaoNameSpace *ns, *mod, *nameSpace = self->nameSpace;
	DaoRoutine *mainRout = nameSpace->mainRoutine;
	DaoVmSpace *vmSpace = self->vmSpace;
	DaoClass *hostClass = self->hostClass;
	DaoToken **tokens = self->tokens->items.pToken;
	DString *modname = NULL;
	DArray *varImport = DArray_New(D_STRING);
	DArray *nsRequire = DArray_New(D_STRING);
	DValue value;
	int i = start+1, j, code = 0;
	int perm = self->permission;
	unsigned char tki = tokens[i]->name;

	DString_Clear( self->mbs );

	if( tki == DTOK_MBS || tki == DTOK_WCS ){
		DString_SubString( tokens[i]->string, self->mbs, 1, tokens[i]->string->size-2 );
		i ++;
	}else if( tki == DKEY_AS ){
		code = DAO_CTW_LOAD_INVALID;
		goto ErrorLoad;
	}else{
		while( i < end ){
			if( tokens[i]->type != DTOK_IDENTIFIER ){
				code = DAO_CTW_PATH_INVALID;
				goto ErrorLoad;
			}
			DString_Append( self->mbs, tokens[i]->string );
			i ++;
			tki = tokens[i]->name;
			if( tki == DTOK_COLON2 || tki == DTOK_DOT ){
				i ++;
				DString_AppendMBS( self->mbs, "/" );
			}else break;
		}
	}
	tki = tokens[i]->name;
	if( tki != DTOK_LCB && tki != DTOK_LSB && tki != DTOK_SEMCO && tki != DKEY_BY
			&& tki != DKEY_REQUIRE && tki != DKEY_IMPORT && tki != DKEY_AS ){
		code = DAO_CTW_LOAD_INVALID;
		goto ErrorLoad;
	}
	for(j=0; j<2; j++){
		DArray *array = NULL;
		if( tokens[i]->name == DKEY_REQUIRE ){
			array = nsRequire;
		}else if( tokens[i]->name == DKEY_IMPORT ){
			array = varImport;
		}
		if( array == NULL ) continue;
		do{
			i ++;
			if( array == varImport && tokens[i]->type != DTOK_IDENTIFIER ){
				code = DAO_CTW_LOAD_INVALID_VAR;
				goto ErrorLoad;
			}
			if( tokens[i]->type == DTOK_MBS || tokens[i]->type == DTOK_WCS ){
				DString_SubString( tokens[i]->string, self->str, 1, tokens[i]->string->size-2 );
			}else{
				DString_Assign( self->str, tokens[i]->string );
			}
			DArray_Append( array, self->str );
			i ++;
		}while( i < end && tokens[i]->name == DTOK_COMMA );
	}
	while( tokens[i]->name == DTOK_LCB ){
		int r = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, i, end );
		if( r < 0 ){
			code = DAO_CTW_LOAD_INVALID;
			goto ErrorLoad;
		}
		for( j=i+1; j<r; j++){
			tki = tokens[j]->name;
			if( tki == DTOK_COMMA || tki == DTOK_SEMCO ) continue;
			if( tokens[j]->type == DTOK_IDENTIFIER ){
				code = DAO_CTW_LOAD_INVALID_VAR;
				goto ErrorLoad;
			}
			DArray_Append( varImport, tokens[j]->string );
		}
		i = r + 1;
	}
	if( tokens[i]->name == DKEY_AS ){
		if( tokens[i+1]->type != DTOK_IDENTIFIER ){
			code = DAO_CTW_LOAD_INVA_MOD_NAME;
			goto ErrorLoad;
		}
		modname = tokens[i+1]->string;
		i += 2;
	}

	ns = nameSpace;
	if( modname != NULL ){
		ns = DaoNameSpace_New( vmSpace );
		value.t = DAO_NAMESPACE;
		value.v.ns = ns;
		if( hostClass && self->isClassBody ){
			DaoClass_AddConst( hostClass, modname, value, perm, tokens[i-1]->line );
		}else{
			DaoNameSpace_AddConst( nameSpace, modname, value, perm );
		}
	}

	mod = 0;

	DaoVmSpace_Lock( self->vmSpace );
	if( tokens[i]->name == DKEY_BY ){
		int reg = DaoParser_GetRegister( self, tokens[i+1] );
		DValue value = DaoParser_GetVariable( self, reg );
		if( value.t == DAO_CDATA ){
			typedef DaoNameSpace* (*LoaderFuncPtr)( DaoVmSpace *vms, const char *name );
			DaoCData *cdata = value.v.cdata;
			LoaderFuncPtr loader = (LoaderFuncPtr)cdata->data;
			if( loader ) mod = (*loader)( self->vmSpace, self->mbs->mbs );
			if( mod && mod->type != DAO_NAMESPACE ) mod = 0;
		}
		i += 2;
	}else if( (mod = DaoNameSpace_FindNameSpace(nameSpace, self->mbs)) ==NULL ){
		/* self->mbs could be changed by DaoVmSpace_LoadModule() */
		DString_Assign( self->str, self->mbs );
		mod = DaoVmSpace_LoadModule( vmSpace, self->mbs, nsRequire );
		MAP_Insert( vmSpace->modRequire, self->str, mod );
		if( modname ) MAP_Insert( vmSpace->modRequire, modname, mod );
	}
	DaoVmSpace_Unlock( self->vmSpace );

	value.t = DAO_ROUTINE;
	value.v.routine = nameSpace->mainRoutine = mainRout;
	DaoNameSpace_SetConst( nameSpace, DVR_NSC_MAIN, value );
	if( mod == 0 ){
		code = DAO_CTW_LOAD_FAILED;
		if( vmSpace->stopit ) code = DAO_CTW_LOAD_CANCELLED;
		goto ErrorLoad;
	}
	if( modname == NULL && varImport->size ==0 ){
		DaoNameSpace_AddParent( nameSpace, mod );
	}else if( ns != mod ) DaoNameSpace_Import( ns, mod, varImport );
	if( ns == nameSpace ){
		DArray_Append( nameSpace->nsLoaded, mod );
	}else{
		DArray_Append( ns->nsLoaded, mod );
		DArray_Append( nameSpace->nsLoaded, ns );
	}

	/*
	   printf("ns=%p; mod=%p; myns=%p\n", ns, mod, nameSpace);
	 */

	if( varImport->size >0 ){
		int k;
		DString_Clear( self->mbs );
		for( k=0; k<varImport->size; k++){
			DString_Append( self->mbs, varImport->items.pString[k] );
			DString_AppendMBS( self->mbs, " " );
		}
		DaoParser_Warn( self, DAO_CTW_LOAD_VAR_NOT_FOUND, self->mbs );
	}
	if( i != end ) DaoParser_Warn( self, DAO_CTW_LOAD_REDUNDANT, NULL );
	DArray_Delete( varImport );
	DArray_Delete( nsRequire );
	return 1;
ErrorLoad:
	DaoParser_Error( self, code, NULL );
	DArray_Delete( varImport );
	DArray_Delete( nsRequire );
	return 0;
}

int DaoParser_MakeForLoop( DaoParser *self, int start, int end )
{
	DaoToken *tok;
	DaoToken **tokens = self->tokens->items.pToken;
	int semic1, semic2, reg1, reg2, fromCode, colon1, colon2;
	int cst, forever = 0;
	int rb = -1;
	int in = -1;
	if( start+1 >= self->tokens->size ) return -1;
	if( tokens[start+1]->name == DTOK_LB )
		rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
	if( rb >= 0 ) in = DaoParser_FindOpenToken( self, DKEY_IN, start+2, rb, 0 );

	if( (rb < 0 || rb >= end) && in < 0 ) return -1;

	DaoParser_AddScope( self, DVM_UNUSED2, start );
	if( in >= 0 ){
		int k, L, elem, semic, regItemt, reg;
		int first;
		DArray *regIter0; /* registers for iterators */
		DArray *regItem0; /* registers for items */
		DArray *regList0; /* registers for list or iteratable data */
		DArray *tokFirst0;
		DArray *tokLast0;
		size_t  *regIter, *regItem, *regList, *tokFirst, *tokLast;

		elem = start + 2;
		semic = DaoParser_FindOpenToken( self, DTOK_SEMCO, start+2, rb, 0 );
		if( semic < 0 && elem < rb ) semic = rb;
		first = 1;
		regItemt = 0;
		regIter0 = DArray_New(0);
		regItem0 = DArray_New(0);
		regList0 = DArray_New(0);
		tokFirst0 = DArray_New(0);
		tokLast0 = DArray_New(0);
		while( semic >=0 ){
			if( tokens[elem+1]->name != DKEY_IN ){
				DaoParser_Error( self, DAO_CTW_FORIN_INVALID, NULL );
				goto CleanUp;
			}
			tok = tokens[elem];
			reg = DaoParser_GetRegister( self, tok );
			if( reg < 0 ) DaoParser_DeclareVariable( self, tok, 0, NULL );
			reg = DaoParser_GetRegister( self, tok );
			if( LOOKUP_ST( reg ) & 1 ){
				DaoParser_Error( self, DAO_CTW_MODIFY_CONST, tok->string );
				goto CleanUp;
			}
			reg = DaoParser_GetNormRegister( self, reg, elem, 0, elem ); /* register for item */
			cst = 0;
			reg1 = DaoParser_MakeArithTree( self, elem+2, semic-1, & cst, -1, 0 );
			if( reg1 < 0 ){
				DaoParser_Error( self, DAO_CTW_FORIN_INVALID, NULL );
				goto CleanUp;
			}
			DArray_Append( tokFirst0, elem );
			DArray_Append( tokLast0, semic-1 );
			DArray_Append( regItem0, reg );
			DArray_Append( regList0, reg1 );
			DArray_Append( regIter0, self->locRegCount );
			DaoParser_PushRegister( self );

			elem = semic + 1;
			semic = DaoParser_FindOpenToken( self, DTOK_SEMCO, elem, rb, 0 );
			if( semic < 0 && elem < rb ) semic = rb;
			first = 0;
		}
		L = tokens[rb]->line;
		regIter = regIter0->items.pSize;
		regItem = regItem0->items.pSize;
		regList = regList0->items.pSize;
		tokFirst = tokFirst0->items.pSize;
		tokLast = tokLast0->items.pSize;
		fromCode = self->vmcCount;
		for( k=0; k<regList0->size; k++){
			size_t first = tokFirst[k];
			size_t last = tokLast[k];
			DaoParser_AddCode( self, DVM_ITER, regList[k], 0, regIter[k], first, first+1,last );
		}
		DaoParser_AddCode( self, DVM_FOR_AUX, 0, 0, 0, start, in, rb );
		/* When all items have been looped,
		 * set the value of the data at regItemt in a way so that TEST on it will fail.
		 */
		regItemt = regIter[0];
		DaoParser_AddCode( self, DVM_FOR, regItemt, fromCode, 0, start, in, rb );
		DaoParser_AddCode( self, DVM_LBRA, 0, 0, 0, rb, rb+1, 0 );
		for( k=0; k<regList0->size; k++){
			size_t first = tokFirst[k];
			size_t last = tokLast[k];
			DaoParser_AddCode( self, DVM_GETI, regList[k], regIter[k], self->locRegCount, first, first+1, last );
			DaoParser_AddCode( self, DVM_MOVE, self->locRegCount, regItemt, regItem[k], first, 0, first );
			DaoParser_PushRegister( self );
		}

		start = rb+1;
		if( tokens[rb+1]->name == DTOK_LCB ){
			start ++;
			DArray_Append( self->scoping, DVM_LBRA );
		}else{
			DArray_Append( self->scoping, DVM_UNUSED );
		}
		DArray_Delete( regIter0 );
		DArray_Delete( regItem0 );
		DArray_Delete( regList0 );
		DArray_Delete( tokFirst0 );
		DArray_Delete( tokLast0 );
		self->lexLevel ++;
		DArray_Append( self->localVarMap, self->lvm );
		DArray_Append( self->localCstMap, self->lvm );
		return start;
CleanUp:
		DArray_Delete( regIter0 );
		DArray_Delete( regItem0 );
		DArray_Delete( regList0 );
		DArray_Delete( tokFirst0 );
		DArray_Delete( tokLast0 );
		return -1;
	}
	colon1 = DaoParser_FindOpenToken( self, DTOK_COLON, start+2, rb, 0 );
	if( colon1 >=0 ){
		int eq, index, first, step, last = 0;
		int loc, pos, st, up, id, set = 0;
		eq = DaoParser_FindOpenToken( self, DTOK_ASSN, start+2, colon1, 1 );
		if( eq < 0 ) return -1;
		if( start+2 != eq-1 ){
			DString_SetMBS( self->mbs, "need a variable" );
			DaoParser_Error( self, DAO_CTW_FOR_INVALID, self->mbs );
			return -1;
		}
		tok = tokens[start+2];
		index = DaoParser_GetRegister( self, tok );
		st = LOOKUP_ST( index );
		up = LOOKUP_UP( index );
		id = LOOKUP_ID( index );
		loc = index;
		if( index < 0 ){
			DaoParser_DeclareVariable( self, tok, 0, NULL );
			index = DaoParser_GetRegister( self, tok );
			loc = index;
		}else if( st & 1 ){
			DString_SetMBS( self->mbs, "can not modify constant" );
			DaoParser_Error( self, DAO_CTW_FOR_INVALID, self->mbs );
			return -1;
		}else if( st >= DAO_LOCAL_CONSTANT ){
			loc = self->locRegCount;
			DaoParser_PushRegister( self );
		}
		first = DaoParser_MakeArithTree( self, eq+1, colon1-1, & cst, -1, 0 );
		if( index < 0 || first <0 ) return -1;
		pos = tokens[eq]->line;
		if( colon1 + 1 == rb ){
			/* infinite looping */
			forever = 1;
			step = DaoParser_IntegerOne( self, colon1 );
		}else{
			colon2 = DaoParser_FindOpenToken( self, DTOK_COLON, colon1+1, rb, 0 );
			if( colon2 >= 0 ){
				step = DaoParser_MakeArithTree( self, colon1+1, colon2-1, & cst, -1, 0 );
				last = DaoParser_MakeArithTree( self, colon2+1, rb-1, & cst, -1, 0 );
			}else{
				step = DaoParser_IntegerOne( self, colon1 );
				last = DaoParser_MakeArithTree( self, colon1+1, rb-1, & cst, -1, 0 );
			}
			if( step < 0 || last <0 ) return -1;
		}
		DaoParser_AddCode( self, DVM_MOVE, first, 0, loc, start+2, eq, colon1-1 );
		switch( st ){
		case DAO_LOCAL_VARIABLE  : if( up ) set = DVM_SETVL; break;
		case DAO_OBJECT_VARIABLE : set = DVM_SETVO; break;
		case DAO_CLASS_VARIABLE  : set = DVM_SETVK; break;
		case DAO_GLOBAL_VARIABLE : set = DVM_SETVG; break;
		}
		if( set ) DaoParser_AddCode( self, set, loc, id, up, start+2, eq, colon1 );

		pos = tokens[colon1]->line;
		DaoParser_AddCode( self, DVM_FOR_AUX, 0, 0, 0, start, 0,0 );
		if( !forever ) DaoParser_AddCode( self, DVM_LE, loc, last, self->locRegCount, start+2, eq, rb-1 );
		DaoParser_AddCode( self, DVM_FOR_STEP, 0, 0, 0, start+2, eq, rb-1 );
		DaoParser_AddCode( self, DVM_ADD, loc, step, loc, start+2, eq, rb-1 );
		if( set ) DaoParser_AddCode( self, set, loc, id, up, start+2, eq, rb-1 );
		DaoParser_AddCode( self, DVM_FOR, self->locRegCount, 0, forever, start, colon1, rb );
		DaoParser_PushRegister( self );
		goto AddScope;
	}
	semic1 = DaoParser_FindOpenToken( self, DTOK_SEMCO, start+2, rb, 1 );
	semic2 = DaoParser_FindOpenToken( self, DTOK_SEMCO, semic1+1, rb, 1 );
	if( rb <0 || semic1 <0 || semic2 <0 ){
		DaoParser_Error( self, DAO_CTW_FOR_INVALID, NULL );
		return -1;
	}
	/* init arith; */
	cst = 0;
	self->warnAssn = 0;
	reg1 = DaoParser_MakeArithTree( self, start+2, semic1-1, & cst, -1, 0 );
	if( reg1 < 0 ) return -1;
	/* cond airth */
	DaoParser_AddCode( self, DVM_FOR_AUX, 0, 0, 0, start, 0, 0 );
	if( semic1 + 1 == semic2 ){
		forever = 1;
	}else{
		reg1 = DaoParser_MakeArithTree( self, semic1+1, semic2-1, & cst, -1, 0 );
		if( reg1 < 0 ) return -1;
	}
	DaoParser_AddCode( self, DVM_FOR_STEP, 0, 0, 0, semic2, 0, rb-1 );
	/* step arith */
	reg2 = DaoParser_MakeArithTree( self, semic2+1, rb-1, & cst, -1, 0 );
	if( reg2 < 0 ) return -1;

	DaoParser_AddCode( self, DVM_FOR, reg1, 0, forever, start, semic1, rb );

AddScope:

	if( tokens[rb+1]->name != DTOK_LCB ) DaoParser_AddScope( self, DVM_UNUSED, rb );
	return rb + 1;
}
int DaoParser_MakeWhileLogic( DaoParser *self, ushort_t opcode, int start )
{
	DValue value;
	DaoToken **tokens = self->tokens->items.pToken;
	int lb = start, rb = -1;
	int from = self->vmcCount;
	int reg, cst = 0;
	int tokPos = tokens[ start ]->line;

	if( tokens[start+1]->name == DTOK_LB ){
		rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
	}else{
		DString_SetMBS( self->mbs, "()" );
		DaoParser_Error( self, DAO_CTW_IS_EXPECTED, self->mbs );
	}
	if( lb<0 || rb<0 ) return -1;

	/*for(int i=lb;i<=rb;i++) printf( "%s  ", tokChr[i].c_str() ); printf("\n"); */

	DaoParser_AddScope( self, DVM_UNUSED2, start );
	reg = DaoParser_MakeArithTree( self, lb+2, rb-1, & cst, -1, 0 );
	if( reg < 0 ) return -1;
	if( opcode == DVM_DOWHILE ){
		opcode = DVM_UNTIL;
		DaoParser_AddCode( self, DVM_NOT, reg, 0, self->locRegCount, lb+2, 0, rb-1 );
		reg = self->locRegCount;
		DaoParser_PushRegister( self );
	}

	/* from stores where the condition's instructions start. */
	if( cst && 0 ){ /* XXX */
		value = DaoParser_GetVariable( self, cst );
		if( value.t && DValue_GetDouble( value ) ){
			DaoParser_AddCode( self, opcode+2, reg, from, opcode, start, 0, rb );
		}else{
			DaoParser_AddCode( self, opcode+1, reg, from, opcode, start, 0, rb );
		}
	}else{
		DaoParser_AddCode( self, opcode, reg, from, opcode, start, 0, rb );
	}
	if( opcode !=DVM_UNTIL && rb+1<self->tokens->size && tokens[rb+1]->name != DTOK_LCB )
		DaoParser_AddScope( self, DVM_UNUSED, rb );

	return rb;
}
static int DaoParser_MakeArithLeaf( DaoParser *self, int start, int *cst );

static int DaoParser_AddFieldConst( DaoParser *self, DString *field )
{
	DValue value = daoNullString;
	value.v.s = field;
	DString_SetMBS( self->mbs, "." );
	DString_Append( self->mbs, field );
	if( MAP_Find( self->allConsts, self->mbs )==NULL ){
		MAP_Insert( self->allConsts, self->mbs, self->routine->routConsts->size );
		DRoutine_AddConstValue( (DRoutine*)self->routine, value );
	}
	return MAP_Find( self->allConsts, self->mbs )->value.pInt;
}

#if 0
/*
   Compiling method:
   c = map( a, b )->|x,y|{ x + 1, y - 10 }->{ x * y };

   it will be compiled into something like the following:

6: MOVE        :      1 ,      1 ,      4 ; # a
7: MOVE        :      3 ,      1 ,      5 ; # b
8: TUPLE       :      4 ,      2 ,      6 ; # ( a, b ) for FUNCT instruction
9: GOTO        :      5 ,     18 ,   1027 ; # skip the functional code part

# declare sub-index register:
10: DATA       :      1 ,      0 ,      7 ; # integer value 0

# some auxilary instructions may added here to reserver register
# or to be used for type checking.

13: SECT       :      5 ,      0 ,     56 ; # code section begin
11: GETI       :      4 ,      7 ,      8 ; # a[i]
12: GETI       :      5 ,      7 ,      9 ; # b[i]
14: ADD        :      8 ,     12 ,     10 ; # x+1
15: SUB        :      9 ,     13 ,     11 ; # y-10

# in 16 "x" refers to "x+1" of 14;
# in 16 "y" refers to "y-10" of 14;
16: MUL        :     10 ,     11 ,     14 ; # x*y

17: RETURN     :      0 ,      0 ,      1 ;
18: FUNCT      :      1 ,      6 ,     15 ; # map

1. Variables "x", "y", "z" will be declared automatically for
list/array elements as functional variables.
2. Variables "i", "j" will also be declared automatically for
apply() as array indices.
3. Function composition is done by compiling, the variables
in a follow-up functional block refers directly the results
of the previous block.
 */
#endif

static int DaoParser_MakeFunctional( DaoParser *self, int *left, int rb, int right, int regFix )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoInode *vmcSect = NULL;
	const char *const xyz[] = { "x", "y", "z" };
	const char *const ijk[] = { "i", "j", "k" };
	int pos, start = *left;
	int regLast = -1;
	int cst = 0;
	unsigned char tki = tokens[start]->name;

	/* built-in functional methods: */
	DArray *fregs = NULL, *fnames = NULL;
	int i, j, N = 0, mark = 0, func = 0, var = 0;
	int p0 = 0, p1 = 0;
	int ic = self->vmcCount + 1;
	int arrows, ret;
	if( tokens[rb+1]->name != DTOK_ARROW ){
		DaoParser_Error( self, DAO_CTW_INVA_SYNTAX, NULL );
		return -1;
	}
	switch( tki ){
	case DKEY_APPLY  : func = DVM_FUNCT_APPLY;  break;
	case DKEY_SORT   : func = DVM_FUNCT_SORT;   break;
	case DKEY_MAP    : func = DVM_FUNCT_MAP;    break;
	case DKEY_FOLD   : func = DVM_FUNCT_FOLD;   break;
	case DKEY_UNFOLD : func = DVM_FUNCT_UNFOLD; break;
	case DKEY_SELECT : func = DVM_FUNCT_SELECT; break;
	case DKEY_INDEX  : func = DVM_FUNCT_INDEX;  break;
	case DKEY_COUNT  : func = DVM_FUNCT_COUNT;  break;
	case DKEY_EACH   : func = DVM_FUNCT_EACH;   break;
	case DKEY_REPEAT : func = DVM_FUNCT_REPEAT;  break;
	case DKEY_STRING : func = DVM_FUNCT_STRING;  break;
	case DKEY_ARRAY  : func = DVM_FUNCT_ARRAY;  break;
	case DKEY_LIST   : func = DVM_FUNCT_LIST;  break;
	default : break;
	}
#ifndef DAO_WITH_ARRAY
	if( func == DVM_FUNCT_ARRAY ){
		printf( "Error: numeric array is NOT enabled!\n" );
		return -1;
	}
#endif
	p0 = p1 = DaoParser_MakeArithArray( self, start+2, rb-1, & N, & cst, DTOK_COMMA, 0, NULL, 0 );
	if( p0 <0 ) return -1;
	if( N == 0 ) goto InvalidParameter;
	pos = tokens[start]->line;
	if( N > 1 ){
		if( tki == DKEY_APPLY ) goto InvalidParameter;
		/* multiple parameters passed as a single tuple to the DVM_FUNCT instruction: */
		DaoParser_AddCode( self, DVM_TUPLE, p0, N, self->locRegCount, start, 0, rb );
		p1 = self->locRegCount;
		DaoParser_PushRegister( self );
	}
	DaoParser_AddCode( self, DVM_SCBEGIN, ic, 0, DVM_SCBEGIN, rb, rb-1, 0 );

	fregs = DArray_New(0);
	fnames = DArray_New(D_STRING);
	mark = self->locRegCount;
	/* reserve an index register, and multiple auxilary register may
	   be allocated after it. */
	DaoParser_AddCode( self, DVM_DATA, DAO_INTEGER, 0, mark, rb, rb+1, 0 );
	DaoParser_PushRegister( self );
	var = self->locRegCount;
	/* adding automatically declared variables and their default names: */
	switch( tki ){
	case DKEY_APPLY :
		DString_SetMBS( self->mbs, "x" );
		DArray_Append( fregs, var );
		DArray_Append( fnames, self->mbs );
		for(i=0; i<2; i++){ /* i, j */
			DString_SetMBS( self->mbs, ijk[i] );
			DArray_Append( fnames, self->mbs );
			DArray_Append( fregs, var+i+1 );
			DaoParser_AddCode( self, DVM_DATA, DAO_INTEGER, 0, var+i+1, rb, 0, 0 );
		}
		DaoParser_PushRegisters( self, 3 );
		DaoParser_AddCode( self, DVM_SECT, ic, 0, DVM_SECT, rb, rb+1, 0 );
		vmcSect = self->vmcLast;
		DaoParser_AddCode( self, DVM_GETI, p0, mark, var, rb, rb+1, 0 ); /* set x */
		break;
	case DKEY_FOLD :
		if( N ==0 || N >2 ) goto InvalidParameter;
		/* by default: x as list/array item, y as init and accumulator: */
		for(i=0; i<2; i++){
			DString_SetMBS( self->mbs, xyz[i] );
			DArray_Append( fregs, var+i );
			DArray_Append( fnames, self->mbs );
		}
		/* for type checking: */
		if( tki == DKEY_FOLD && N >1 ) DaoParser_AddCode( self, DVM_MOVE, p0+1, 0, var+1, rb, 0, 0 );
		DaoParser_AddCode( self, DVM_GETI, p0, mark, var+1, rb, rb+1, 0 );
		DaoParser_AddCode( self, DVM_SECT, ic, 0, DVM_SECT, rb, rb+1, 0 );
		DaoParser_AddCode( self, DVM_GETI, p0, mark, var, rb, rb+1, 0 ); /* set x */
		DaoParser_PushRegisters( self, 2 );
		DString_SetMBS( self->mbs, "i" );
		DArray_Append( fregs, var-1 );
		DArray_Append( fnames, self->mbs );
		break;
	case DKEY_UNFOLD :
		if( N != 1 ) goto InvalidParameter;
		DString_SetMBS( self->mbs, "x" );
		DArray_Append( fregs, var );
		DArray_Append( fnames, self->mbs );
		DaoParser_AddCode( self, DVM_MOVE, p0, 0, var, rb, 0, 0 );
		DaoParser_AddCode( self, DVM_SECT, ic, 0, DVM_SECT, rb, rb+1, 0 );
		DaoParser_PushRegister( self );
		break;
	case DKEY_SORT   :
		if( N ==0 || N >2 ) goto InvalidParameter;
		for(i=0; i<2; i++){
			DString_SetMBS( self->mbs, xyz[i] );
			DArray_Append( fregs, var+i );
			DArray_Append( fnames, self->mbs );
			DaoParser_AddCode( self, DVM_GETI, p0, mark, var+i, rb, rb+1, 0 );
		}
		DaoParser_PushRegisters( self, 2 );
		DaoParser_AddCode( self, DVM_SECT, ic, 0, DVM_SECT, rb, rb+1, 0 );
		break;
	case DKEY_MAP    :
	case DKEY_SELECT :
	case DKEY_INDEX  :
	case DKEY_COUNT  :
	case DKEY_EACH  :
		DaoParser_AddCode( self, DVM_SECT, ic, 0, DVM_SECT, rb, rb+1, 0 );
		for(i=0; i<N; i++){
			if( i < 3 ){
				DString_SetMBS( self->mbs, xyz[i] );
				DArray_Append( fnames, self->mbs );
			}
			DArray_Append( fregs, var+i );
			DaoParser_AddCode( self, DVM_GETI, p0+i, mark, var+i, rb, rb+1, 0 );
		}
		DaoParser_PushRegisters( self, N );
		DString_SetMBS( self->mbs, "i" );
		DArray_Append( fregs, var-1 );
		DArray_Append( fnames, self->mbs );
		break;
	case DKEY_REPEAT :
	case DKEY_STRING :
	case DKEY_ARRAY :
	case DKEY_LIST :
		DString_SetMBS( self->mbs, "i" );
		DArray_Append( fregs, var-1 );
		DArray_Append( fnames, self->mbs );
		DaoParser_AddCode( self, DVM_SECT, ic, 0, DVM_SECT, rb, rb+1, 0 );
		break;
	default : break;
	}
	if( rb+1 >= right || tokens[rb+1]->name != DTOK_ARROW ) goto InvalidFunctionalSyntax;

	arrows = 0;
	while( rb+1 < right && tokens[rb+1]->name == DTOK_ARROW ){
		int islast = 0;
		start = rb + 2;
		arrows ++;
		j = 0;
		if( tokens[start]->name == DTOK_PIPE ){
			/* -> | u, v, w | {} */
			/* declare explicit variables from available registers saved before: */
			i = start + 1;
			while( i < right ){
				if( tokens[i]->type != DTOK_IDENTIFIER ) goto InvalidFunctionalSyntax;
				if( j < fregs->size ){
					MAP_Insert( self->varFunctional, tokens[i]->string, fregs->items.pInt[j] );
				}else if( arrows == 1 && tki == DKEY_APPLY ){
					DaoInode *node = DaoInode_New( self );
					node->code = DVM_DATA;
					node->a = DAO_INTEGER;
					node->c = self->locRegCount;
					node->level = vmcSect->level;
					node->line = vmcSect->line;
					node->first = i;
					node->middle = node->last = 0;
					vmcSect->prev->next = node;
					node->prev = vmcSect->prev;
					vmcSect->prev = node;
					node->next = vmcSect;
					MAP_Insert( self->varFunctional, tokens[i]->string, self->locRegCount );
					DaoParser_PushRegister( self );
				}
				j ++;
				if( tokens[i+1]->name == DTOK_PIPE ) break;
				if( tokens[i+1]->name != DTOK_COMMA ) goto InvalidFunctionalSyntax;
				i += 2;
			}
			start = i + 2;
		}else{
			/* declare implicit variables with default names: */
			for(j=0; j<fnames->size; j++){
				MAP_Insert( self->varFunctional, fnames->items.pString[j], fregs->items.pInt[j] );
			}
		}
		/* if( j < fregs->size ) printf( "Warning: unused paraemter\n" ); */
		if( j > fregs->size && tki != DKEY_APPLY ) goto InvalidFunctionalSyntax; /*  XXX */

		if( tokens[start]->name != DTOK_LCB ) goto InvalidFunctionalSyntax;
		DaoParser_AddScope( self, DVM_FUNCT, start );

		rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, right );
		if( rb < 0 ) goto InvalidFunctional;
		islast = rb+1 >= right || (tokens[rb+1]->name != DTOK_ARROW);

		/* ->{ block return expressions } */
		ret = DaoParser_FindOpenToken( self, DKEY_RETURN, start+1, rb, 0 );
		pos = tokens[rb]->line;
		/* parse block */
		if( islast && (tki == DKEY_EACH || tki == DKEY_REPEAT) ){
			if( DaoParser_ParseCodeSect( self, start+1, rb-1 )==0 ) goto InvalidFunctional;
			p0 = 0;
			goto Finish;
		}if( ret >=0 ){
			if( tki == DKEY_EACH || tki == DKEY_REPEAT ) goto InvalidFunctionalSyntax;
			if( tki == DKEY_UNFOLD && islast ){
				int wh = DaoParser_FindOpenToken( self, DKEY_WHILE, start+1, ret, 0 );
				int c=0, wh2 = wh;
				while( wh2 >=0 ){
					wh = wh2;
					wh2 = DaoParser_FindOpenToken( self, DKEY_WHILE, wh+1, ret, 0 );
				}
				if( wh <0 ) goto InvalidFunctionalSyntax;
				wh2 = DaoParser_FindPhraseEnd( self, wh + 1, ret );
				if( wh2 + 1 != ret ) goto InvalidFunctionalSyntax;
				if( DaoParser_ParseCodeSect( self, start+1, wh-1 )==0 ) goto InvalidFunctional;
				wh2 = DaoParser_MakeArithTree( self, wh+1, ret-1, & c, var-1, 0 );
				if( wh2 <0 ) goto InvalidFunctional;
				if( c && DValue_GetInteger( DaoParser_GetVariable( self, c ) ) ){
					DString_SetMBS( self->mbs, "infinite unfold!" );
					DaoParser_Error( self, DAO_CTW_INVA_SYNTAX, self->mbs );
					goto InvalidFunctional; /* XXX infinite unfold! */
				}
			}else{
				if( DaoParser_ParseCodeSect( self, start+1, ret-1 )==0 ) goto InvalidFunctional;
			}
			/* parse expressions */
			if( tokens[rb-1]->name == DTOK_SEMCO )
				p0 = DaoParser_MakeArithArray( self, ret+1, rb-2, & N, & cst, DTOK_COMMA, 0, NULL, 0 );
			else
				p0 = DaoParser_MakeArithArray( self, ret+1, rb-1, & N, & cst, DTOK_COMMA, 0, NULL, 0 );
		}else{
			/* parse expressions */
			if( tki == DKEY_UNFOLD && islast  ) goto InvalidFunctionalSyntax;
			p0 = DaoParser_MakeArithArray( self, start+1, rb-1, & N, & cst, DTOK_COMMA, 0, NULL, 0 );
		}
		DMap_Clear( self->varFunctional );
		if( p0 <0 ) goto InvalidFunctional;

		DArray_Clear( fregs );
		DArray_Clear( fnames );
		for(i=0; i<N; i++){
			if( i < 3 ){
				DString_SetMBS( self->mbs, xyz[i] );
				DArray_Append( fnames, self->mbs );
			}
			DArray_Append( fregs, p0+i );
		}
		DaoParser_DelScope( self, DVM_FUNCT, rb );
	}
	if( N > 1 ){
		if( tki != DKEY_MAP && tki != DKEY_ARRAY && tki != DKEY_LIST ) goto InvalidFunctional;
		DaoParser_AddCode( self, DVM_TUPLE, p0, N, self->locRegCount, start+1, 0, rb-1 );
		p0 = self->locRegCount;
		DaoParser_PushRegister( self );
	}else if( tki == DKEY_FOLD || tki == DKEY_UNFOLD ){
		if( N != 1 ) goto InvalidFunctional;
		DaoParser_AddCode( self, DVM_MOVE, p0, 0, var + (tki==DKEY_FOLD), rb, 0, 0 );
	}
Finish:
	DMap_Clear( self->varFunctional );
	DaoParser_AddCode( self, DVM_SCEND, ic, p0, DVM_SCEND, rb, rb+1, 0 );
	regLast = regFix;
	if( regFix <0 ){
		regLast = self->locRegCount;
		DaoParser_PushRegister( self );
	}
	DaoParser_AddCode( self, DVM_FUNCT, func, p1, regLast, *left, 0, rb );
	DArray_Delete( fregs );
	DArray_Delete( fnames );
	*left = rb + 1;
	return regLast;
InvalidParameter:
	DaoParser_Error( self, DAO_CTW_PAR_INVALID, NULL );
	goto InvalidFunctional;
InvalidFunctionalSyntax:
	DaoParser_Error( self, DAO_CTW_INVA_SYNTAX, NULL );
InvalidFunctional:
	printf( "invalid functional method;\n" ); /* XXX */
	if( fregs ) DArray_Delete( fregs );
	if( fnames ) DArray_Delete( fnames );
	return -1;
}
static int DaoParser_MakeConst( DaoParser *self, DaoInode *front, DaoInode *back,
		DArray *cid, int regcount, int N, int start, int mid, int end, int *cst )
{
	DaoType *type = self->enumTypes->size ? self->enumTypes->items.pType[0] : NULL;
	DaoNameSpace *myNS = self->nameSpace;
	DaoContext *ctx = myNS->vmpEvalConst->topFrame->context;
	DaoVmCodeX vmcValue;
	DValue value;
	int i, opB, code = self->vmcLast->code;
	int tokPos = self->tokens->items.pToken[start]->line;
	/* For register allocation: */
	if( ctx->regArray->size < N+1 ){
		DVaTuple_Resize( ctx->regArray, N+1, daoNullValue );
		ctx->regValues = dao_realloc( ctx->regValues, (N+1) * sizeof(DValue*) );
		for(i=0; i<N+1; i++) ctx->regValues[i] = ctx->regArray->data + i;
	}

	/* printf( "code = %s, %i\n", getOpcodeName( code ), N ); */
	/* Prepare registers for the instruction. */
	for( i=0; i<N; i++ ){
		/* printf( "reg = %i\n", cid->items.pInt[i] ); */
		/* No need GC here: */
		DValue v = DaoParser_GetVariable( self, cid->items.pInt[i] );
		DValue_SimpleMove( v, ctx->regValues[i+1] );
	}
	/* for( i=0; i<size; i++) DaoVmCode_Print( *vmCodes->items.pVmc[i], NULL ); */
	opB = self->vmcLast->b;
	DaoParser_PopCodes( self, front, back );
	self->locRegCount = regcount;
	DArray_Resize( self->regLines, regcount, 0 );
	/* Execute the instruction to get the const result: */
	DaoVmCode_Set( & vmcValue, code, 1, opB, 0, self->lexLevel, tokPos, start, mid, end );
	value = DaoVmProcess_MakeEnumConst( myNS->vmpEvalConst, 
			(DaoVmCode*)(void*) & vmcValue, N+1, type );
	*cst = LOOKUP_BIND_LC( DRoutine_AddConstValue( (DRoutine*)self->routine, value ));
	return DaoParser_GetNormRegister( self, *cst, start, 0, end );
}
static int DaoParser_MakeChain( DaoParser *self, int left, int right, int *cst, int regFix )
{
	DaoInode *vmc = NULL;
	DaoToken **tokens = self->tokens->items.pToken;
	unsigned char tki = tokens[left+1]->type;
	int rbrack=right, regC, cstlast;
	int start=left, N=0;
	int regLast = -1;
	int reg, reg2, rb, i;
	ushort_t opB;

	/*
	   for(i=left;i<=right;i++) printf("%s  ", tokens[i]->string->mbs);printf("\n");
	 */
	if( left == right ){
		self->curLine = tokens[left]->line;
		DaoParser_Error( self, DAO_CTW_INTERNAL, NULL );
		return -1;
	}
	*cst = 0;
	if( tokens[left]->type == DTOK_IDENTIFIER && (tki == DTOK_COLON2 || tki == DTOK_LT) ){
		DString *name = DString_New(1);
		DValue scope = daoNullValue;
		DValue value = daoNullValue;
		int pos = DaoParser_FindScopedData( self, start, & scope, & value, 0, name );
		if( pos >=0 && value.t != DAO_STRING ){
			regLast = DRoutine_AddConstValue( (DRoutine*)self->routine, value );
			*cst = LOOKUP_BIND_LC( regLast );
			regLast = DaoParser_GetNormRegister( self, *cst, start, 0, pos );
			start = pos + 1;
		}else if( pos >=0 && scope.t && value.t == DAO_STRING ){
			int k = DString_RFindMBS( name, "::", name->size );
			if( k != MAXSIZE ) DString_Erase( name, 0, k+1 );
			reg = self->locRegCount;
			DaoParser_PushRegister( self );
			regLast = DRoutine_AddConstValue( (DRoutine*)self->routine, scope );
			DaoParser_AddCode( self, DVM_GETCL, 0, regLast, reg, start, 0, pos );
			reg2 = DaoParser_AddFieldConst( self, name );
			regLast = self->locRegCount;
			DaoParser_PushRegister( self );
			DaoParser_AddCode( self, DVM_GETF, reg, reg2, regLast, start, 0, pos+1 );
			start = pos + 1;
		}
		DString_Delete( name );
	}
	while( start <= right ){
		int pos = tokens[start]->line;
		self->curLine = tokens[start]->line;
		pos = tokens[start]->line;
		cstlast = *cst;
		*cst = 0;
		tki = tokens[start]->name;
		if( start == left ){
			if( tki == DTOK_LB ){
				rbrack = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, right );
				if( rbrack < 0 ) return -1;
				regLast = DaoParser_MakeArithTree( self, start+1, rbrack-1, cst, -1, 0 );
				start = rbrack;
			}else if( tki == DTOK_LCB ){
				rbrack = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, right );
				if( rbrack < 0 ) return -1;
				regLast = DaoParser_MakeArithTree( self, start, rbrack, cst, -1, 0 );
				start = rbrack;
			}else if( tki == DTOK_LSB ){
				rbrack = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, start, right );
				if( rbrack < 0 ) return -1;
				regLast = DaoParser_MakeArithTree( self, start, rbrack, cst, -1, 0 );
				start = rbrack;
			}else if( tki == DKEY_SELF && start+2 <= right && tokens[start+1]->name == DTOK_DOT
					&& (start+3 > right || tokens[start+3]->name != DTOK_LB) ){
				int st;
				if( self->hostClass ==NULL && !(self->routine->attribs & DAO_ROUT_PARSELF) ){
					DaoParser_Error( self, DAO_CTW_EXPR_INVALID, NULL );
					return -1;
				}
				i = DaoParser_GetClassMember( self, tokens[start+2]->string );
				st = LOOKUP_ST( i );
				if( st >= DAO_OBJECT_VARIABLE && st <= DAO_CLASS_CONSTANT ){
					regLast = DaoParser_GetNormRegister( self, i, start+2, 0, start+2 );
					start += 2;
				}else{ /* for operator .field, i will be <0 */
					regLast = DaoParser_MakeArithLeaf( self, start, cst );
				}
			}else if( tki == DKEY_YIELD ){
				if( start+1 > right || tokens[start+1]->name != DTOK_LB ){
					DaoParser_Error( self, DAO_CTW_EXPR_INVALID, NULL );
					return -1;
				}
				rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, right );
				if( rb < 0 ) return -1;
				reg = DaoParser_MakeArithArray( self, start+2, rb-1, & N, cst,
						DTOK_COMMA, 0, NULL, 0 );
				if( reg < 0 ) return -1;
				regLast = regFix;
				if( regFix <0 ){
					regLast = self->locRegCount;
					DaoParser_PushRegister( self );
				}
				DaoParser_AddCode( self, DVM_YIELD, reg, N, regLast, start, 0, rb );
				start = rb;
			}else if( ( (tki >= DKEY_EACH && tki <= DKEY_TANH) || tki == DKEY_STRING
						|| tki == DKEY_ARRAY || tki == DKEY_LIST || tki == DKEY_MAP )
					&& tokens[start+1]->name == DTOK_LB){
				rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, right );
				if( rb < 0 || (rb == start+2 && tki != DKEY_RAND) ){
					if( rb == start+2 ) DaoParser_Error( self, DAO_CTW_PAR_INVALID, NULL );
					return -1;
				}
				if( tki >= DKEY_ABS && tki <= DKEY_TANH ){
					/* built-in math functions */
					if( rb == start+2 /* && tki == DKEY_RAND */ ){
						reg = self->locRegCount;
						DaoParser_AddCode( self, DVM_DATA, DAO_DOUBLE, 1, reg, start, 0, rb );
						DaoParser_PushRegister( self );
					}else{
						reg = DaoParser_MakeArithTree( self, start+2, rb-1, cst, -1, 0 );
					}
					if( reg <0 ) return -1;
					if( *cst ){
						DaoVmProcess *vmp = self->nameSpace->vmpEvalConst;
						DaoContext *ctx = vmp->topFrame->context;
						DValue value = DaoParser_GetVariable( self, *cst );
						DaoVmCode vmc = { DVM_MATH, 0, 2, 0 };
						int i;
						vmc.a = tki - DKEY_ABS;
						DVaTuple_Clear( ctx->regArray );
						DVaTuple_Resize( ctx->regArray, 3, daoNullValue );
						ctx->regValues = dao_realloc( ctx->regValues, 3 * sizeof(DValue*) );
						for(i=0; i<3; i++) ctx->regValues[i] = ctx->regArray->data + i;

						DValue_Copy( ctx->regValues[2], value );
						ctx->vmSpace = self->vmSpace;
						ctx->vmc = & vmc;
						value = DaoVmProcess_MakeConst( vmp );
						if( value.t == 0 ){
							DaoParser_Error( self, DAO_CTW_INV_CONST_EXPR, NULL );
							return -1;
						}
						*cst = DRoutine_AddConstValue( (DRoutine*)self->routine, value );
						*cst = LOOKUP_BIND_LC( *cst );
						regLast = DaoParser_GetNormRegister( self, *cst, start, 0, rb );
						if( regFix >=0 && regLast != regFix ){
							DaoParser_AddCode( self, DVM_MOVE, regLast, 0, regFix, start, 0, rb );
							regLast = regFix;
						}
					}else{
						regLast = regFix;
						if( regFix <0 ){
							regLast = self->locRegCount;
							DaoParser_PushRegister( self );
						}
						DaoParser_AddCode( self, DVM_MATH, tki-DKEY_ABS, reg, regLast, start, 0, rb );
					}
					start = rb;
				}else{
					/* built-in functional methods: */
					regLast = DaoParser_MakeFunctional( self, & start, rb, right, regFix );
					if( regLast < 0 ) return -1;
				}
			}else if( tokens[start]->name == DTOK_ID_INITYPE && tokens[start+1]->name == DTOK_LB ){
				DaoToken tok = *tokens[start];
				regLast = DaoParser_GetRegister( self, & tok );
				if( regLast <0 ){
					DString_SetMBS( self->mbs, tokens[start]->string->mbs + 1 );
					tok.string = self->mbs;
					regLast = DaoParser_GetRegister( self, & tok );
					if( regLast < 0 ){
						DaoParser_Error( self, DAO_SYMBOL_NOT_DEFINED, self->mbs );
						return -1;
					}
				}
				regLast = DaoParser_GetNormRegister( self, regLast, start, 0, start );
			}else{
				regLast = DaoParser_MakeArithLeaf( self, start, cst );
			}
			if( regLast < 0 ) return -1;
			start ++;
		}else if( tki == DTOK_LB ){
			/* routine rout( a, b ){ ... } */
			int rb, regC, isC = 0, mode = 0, N = 0, code = DVM_CALL;
			DaoInode *mov = NULL, *set = NULL;
			/* obj . routine( A, B ) . more : */
			rbrack = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, right );
			if( rbrack < 0 ) return -1;

			rb = rbrack;
			while( rbrack +1 <= right ){
				int m = KeyBinSearch( mapCallMode, countCallMode, tokens[rbrack+1]->string );
				if( m >0 ){
					mode |= m;
					rbrack ++;
				}else{
					break;
				}
			}
			if( tokens[start-1]->name == DTOK_ID_INITYPE ){
				if( DaoParser_GetRegister( self, tokens[start-1] ) <0 ){
					mode |= DAO_CALL_COROUT;
					rbrack ++;
				}
			}
			if( tokens[rb-1]->name == DTOK_DOTS ){
				if( rb == start + 2 ){
					N = DAO_CALLER_PARAM;
					regC = self->locRegCount;
					if( regFix >=0 && rbrack == right ) regC = regFix;
					DaoParser_AddCode( self, DVM_CALL, regLast, N, regC, left, 0, rb );
					if( regC == self->locRegCount ) DaoParser_PushRegister( self );
					regLast = regC;
					start = rbrack + 1;
					continue;
				}
				mode |= DAO_CALL_EXPAR;
				rb --;
				if( tokens[rb-1]->name == DTOK_COMMA ) rb --;
			}
			if( start== left+1 ){
				/* foo(); foo(); */
				DaoParser_AddCode( self, DVM_MOVE, regLast, 1, self->locRegCount, left, 0, 0 );
				regLast = self->locRegCount;
				DaoParser_PushRegister( self );
			}
			opB = 0;
			if( self->vmcLast != self->vmcBase && start >= left + 2){
				int fop = tokens[start-2]->name;
				if( fop == DTOK_DOT || fop == DTOK_ARROW ){
					vmc = self->vmcLast;
					if( (vmc->code == DVM_GETF || vmc->code == DVM_GETMF) && vmc->c == regLast ){
						DaoInode *p = vmc->prev;
						if( p->code >= DVM_GETVL && p->code <= DVM_GETMF && p->c == vmc->a ) set = p;

						opB ++;
						code = DVM_MCALL;
						DaoParser_AddCode( self, DVM_LOAD, vmc->a, 0, self->locRegCount, start-3, 0,0 );
						DaoParser_PushRegister( self );
						mov = self->vmcLast;
					}
				}
			}
			if( DaoParser_MakeArithArray( self, start+1, rb-1, & N, & isC, DTOK_COMMA, 0, NULL, EXP_IN_CALL ) <0 )
				return -1;
			opB += N;

			if( N > DAO_MAX_PARAM ){
				DaoParser_Error( self, DAO_CTW_LIMIT_PAR_NUM, NULL );
				return -1;
			}

			regC = self->locRegCount;
			if( regFix >=0 && rbrack == right ) regC = regFix;
			if( cstlast && N ==0 ){
				DValue v = DaoParser_GetVariable( self, cstlast );
				if( v.t == DAO_CLASS && (v.v.klass->attribs & DAO_CLS_AUTO_DEFAULT) ){
					DaoObject *o = DaoObject_New( v.v.klass, NULL, 0 );
					*cst = DRoutine_AddConst( (DRoutine*)self->routine, (DaoBase*)o );
					*cst = LOOKUP_BIND_LC( *cst );
					regLast = DaoParser_GetNormRegister( self, *cst, start, 0, rbrack-1 );
					if( regFix >=0 && rbrack == right )
						DaoParser_AddCode( self, DVM_MOVE, regLast, 0, regFix, start, 0, rbrack );
					code = 0;
				}
			}
			if( code ){
				if( tokens[start-1]->type == DTOK_IDENTIFIER ){
					DaoParser_AddCode( self, code, regLast, opB|mode, regC, start-1, 0, rbrack );
				}else{
					DaoParser_AddCode( self, code, regLast, opB|mode, regC, start, 0, rbrack );
				}
#if 0
				if( mov ) DaoParser_AddCode( self, DVM_MOVE, regLast+1, 0, mov->a, pos );
				if( set ){
					code = set->code + (DVM_SETV - DVM_GETV);
					DaoParser_AddCode( self, code, regLast+1, set->b, set->a, pos );
				}
#endif

				/* put the result at the current last position in the stack. */
				regLast = regC;
				if( regC == self->locRegCount ) DaoParser_PushRegister( self );
			}

			start = rbrack+1;
		}else if( tki == DTOK_LCB ){
			/* dao_class{ members } enumeration,
			 * or routine{ parameters } */
			DArray *cid;
			DaoInode *front = self->vmcFirst;
			DaoInode *back = self->vmcLast;
			int regcount = self->locRegCount;
			int rb, N = 0, isC = 0;
			int code = DVM_CURRY;
			rbrack = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, right );
			if( rbrack < 0 ) return -1;

			cid = DArray_New(0);
			DArray_Append( cid, cstlast );
			if( start== left+1 ){
				DaoParser_AddCode( self, DVM_MOVE, regLast, 1, self->locRegCount, start, 0, rbrack );
				regLast = self->locRegCount;
				DaoParser_PushRegister( self );
			}
			rb = rbrack;
			if( tokens[rb-1]->name ==DTOK_SEMCO || tokens[rb-1]->name ==DTOK_COMMA ) rb --;

			opB = 0;
			vmc = self->vmcLast;
			if( (vmc->code == DVM_GETF || vmc->code == DVM_GETMF) && vmc->c == regLast ){
				opB ++;
				code = DVM_MCURRY;
				DaoParser_AddCode( self, DVM_MOVE, vmc->a, 0, self->locRegCount, start, 0, 0 );
				DaoParser_PushRegister( self );
			}
			if( DaoParser_MakeArithArray( self, start+1, rb-1, & N, & isC,
						DTOK_COMMA, DTOK_SEMCO, cid, 0 ) <0 )
				return -1;
			opB += N;
			regC = self->locRegCount;
			if( regFix >=0 && rbrack == right ) regC = regFix;

			if( tokens[start-1]->type == DTOK_IDENTIFIER ){
				DaoParser_AddCode( self, code, regLast, opB, regC, start-1, 0, rbrack );
			}else{
				DaoParser_AddCode( self, code, regLast, opB, regC, start, 0, rbrack );
			}

			/* put the result at the current last position in the stack. */
			regLast = regC;
			if( regC == self->locRegCount ) DaoParser_PushRegister( self );

			if( code == DVM_CURRY && cstlast && isC )
				regLast = DaoParser_MakeConst( self, front, back, cid, regcount, N+1, start, 0, rbrack, cst );
			DArray_Delete( cid );

			start = rbrack+1;
		}else if( tki == DTOK_LSB ){
			/*  map | vector [ i ] : */
			int N = 0, isC = 0;
			rbrack = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, start, right );
			if( rbrack < 0 ) return -1;

			if( DaoParser_FindOpenToken( self, DTOK_COMMA, start+1, rbrack, 0 ) < 0 ){
				int reg = DaoParser_MakeArithTree( self, start+1, rbrack-1, & isC, -1, 0 );
				if( reg < 0 ) return -1;
				if( cstlast && isC ){
					DValue v1 = DaoParser_GetVariable( self, cstlast );
					DValue v2 = DaoParser_GetVariable( self, isC );
					int id = -1;
					switch( v2.t ){
					case DAO_INTEGER : id = v2.v.i; break;
					case DAO_FLOAT   : id = v2.v.f; break;
					case DAO_DOUBLE  : id = v2.v.d; break;
					default : break;
					}
					if( v1.t == DAO_MAP || id >=0 ){
						DValue value = daoNullValue;
						if( v1.t == DAO_STRING ){
							DString *str = v1.v.s;
							value.t = DAO_INTEGER;
							if( str->mbs ){
								if( id >= 0 && id < str->size )
									value.v.i = (uchar_t)str->mbs[id];
							}else{
								if( id >= 0 && id < str->size )
									value.v.i = str->wcs[id];
							}
						}else if( v1.t == DAO_LIST ){
							DaoList *list = v1.v.list;
							if( id >=0 && id < list->items->size ) value = list->items->data[id];
						}else if( v1.t == DAO_MAP ){
							DaoMap *map = v1.v.map;
							DNode *node = DMap_Find( map->items, & v2 );
							if( node ) value = node->value.pValue[0];
#ifdef DAO_WITH_NUMARRAY
						}else if( v1.t == DAO_ARRAY ){
							DaoArray *array = v1.v.array;
							if( id >= 0 && id < array->size ){
								value.t = DAO_INTEGER + array->numType - DAO_INTEGER;
								if( array->numType == DAO_COMPLEX ) value.t = DAO_COMPLEX;
								switch( array->numType ){
								case DAO_INTEGER : value.v.i = array->data.i[id] ; break;
								case DAO_FLOAT : value.v.f = array->data.f[id] ; break;
								case DAO_DOUBLE : value.v.d = array->data.d[id] ; break;
								case DAO_COMPLEX : value.v.c = & self->combuf;
												   self->combuf = array->data.c[id];
												   break;
								default : break;
								}
							}
#endif
						}
						if( value.t ){
							*cst = DRoutine_AddConstValue( (DRoutine*)self->routine, value );
							*cst = LOOKUP_BIND_LC( *cst );
							regLast = DaoParser_GetNormRegister( self, *cst, start+1, 0, rbrack-1 );
							if( regFix >=0 && rbrack == right )
								DaoParser_AddCode( self, DVM_MOVE, regLast, 0, regFix, start, 0, rbrack );
						}
					}
				}
				if( *cst ==0 ){
					regC = self->locRegCount;
					if( regFix >=0 && rbrack == right ) regC = regFix;
					if( tokens[start-1]->type == DTOK_IDENTIFIER ){
						DaoParser_AddCode( self, DVM_GETI, regLast, reg, regC, start-1, start, rbrack );
					}else{
						DaoParser_AddCode( self, DVM_GETI, regLast, reg, regC, left, start, rbrack );
					}
					regLast = regC;
					if( regC == self->locRegCount )
						DaoParser_PushRegister( self );
				}
			}else{
				int regEn;
				regEn = DaoParser_MakeArithArray( self,
						start+1, rbrack-1, & N, & isC, DTOK_COMMA, 0, NULL, 0 );
				if( regEn < 0 ) return -1;
				regC = self->locRegCount;
				DaoParser_AddCode( self, DVM_TUPLE, regEn, N, regC, start, 0, rbrack );
				DaoParser_PushRegister( self );
				regEn = regC;

				regC = self->locRegCount;
				if( regFix >=0 && rbrack == right ) regC = regFix;
				if( tokens[start-1]->type == DTOK_IDENTIFIER ){
					DaoParser_AddCode( self, DVM_GETI, regLast, regEn, regC, start-1, start, rbrack );
				}else{
					DaoParser_AddCode( self, DVM_GETI, regLast, regEn, regC, left, start, rbrack );
				}
				regLast = regC;
				if( regC == self->locRegCount )
					DaoParser_PushRegister( self );
			}

			start = rbrack+1;
		}else if( tki == DTOK_DOT || tki == DTOK_COLON2 ){
			DValue it = daoNullValue;
			DString *name;
			int start2 = start;
			int regB = -1;
			if( tki == DTOK_COLON2 && start+1 < right && tokens[start+1]->name == DTOK_LB ){
				DaoType *abtp;
				int kk, mm, newpos = 0;
				mm = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start+1, right );
				if( mm <0 ) return -1;
				abtp = DaoParser_ParseType( self, start+1, mm-1, & newpos, NULL );
				if( abtp == NULL || newpos != mm ) return -1;
				regC = regFix;
				if( regFix <0 || mm < right ){
					regC = self->locRegCount;
					DaoParser_PushRegister( self );
				}
				MAP_Insert( self->routine->localVarType, regC, abtp );
				kk = DaoRoutine_AddConst( self->routine, abtp );
				DaoParser_AddCode( self, DVM_CAST, regLast, kk, regC, start, 0, mm );
				regLast = regC;
				start = mm + 1;
				continue;
			}
			if( start >= right ){
				DaoParser_Error2( self, DAO_INVALID_EXPRESSION, start2, right, 0 );
				return -1;
			}
			name = tokens[start+1]->string;
			/* printf( "%s  %i\n", name->mbs, cstlast ); */
			if( cstlast ){
				DValue ov = DaoParser_GetVariable( self, cstlast );
				DaoType *tp = (DaoType*) ov.v.p;
				DaoTypeBase *typer;
				/*
				   printf( "%s  %i\n", name->mbs, ov.t );
				 */
				switch( ov.t ){
				case DAO_TYPE :
					if( tp->tid == DAO_TYPE ) tp = tp->nested->items.pType[0];
					if( tp && tp->tid == DAO_ENUM && tp->mapNames ){
						DNode *node = DMap_Find( tp->mapNames, name );
						if( node ){
							it.t = DAO_ENUM;
							it.v.e = self->denum;
							self->denum->value = node->value.pInt;
							GC_ShiftRC( tp, self->denum->type );
							self->denum->type = tp;
						}
					}
					break;
				case DAO_NAMESPACE :
					regB = DaoNameSpace_FindConst( ov.v.ns, name );
					if( regB >=0 ) it = DaoNameSpace_GetConst( ov.v.ns, regB );
					break;
				case DAO_CLASS :
					regB = DaoClass_FindConst( ov.v.klass, name );
					if( regB >=0 ) it = DaoClass_GetConst( ov.v.klass, regB );
					break;
				default :
					typer = DValue_GetTyper( ov );
					it = DaoFindValueOnly( typer, name );
					/*
					   if( it.t ==0 ) printf( "%s  %i: in %s\n", name->mbs, it.t, typer->name );
					 */
					break;
				}
			}
			if( it.t && it.t < DAO_ARRAY ){
				*cst = DRoutine_AddConstValue( (DRoutine*)self->routine, it );
				*cst = LOOKUP_BIND_LC( *cst );
				regLast = DaoParser_GetNormRegister( self, *cst, start, 0, start+1 );
				if( regFix >=0 && rbrack == right )
					DaoParser_AddCode( self, DVM_MOVE, regLast, 0, regFix, start, 0, start+1 );
			}else{
				regB = DaoParser_AddFieldConst( self, name );
				regC = self->locRegCount;
				if( regFix >=0 && start+1 == right ) regC = regFix;
				if( tokens[start-1]->type == DTOK_IDENTIFIER ){
					DaoParser_AddCode( self, DVM_GETF, regLast, regB, regC, start-1, 0, start+1 );
				}else{
					DaoParser_AddCode( self, DVM_GETF, regLast, regB, regC, start, 0, start+1 );
				}
				regLast = regC;
			}
			if( regLast == self->locRegCount ) DaoParser_PushRegister( self );
			start += 2;
		}else if( tki == DTOK_ARROW ){
			DString *name = tokens[start+1]->string;
			int regB;
			if( start >= right ){
				DaoParser_Error2( self, DAO_INVALID_EXPRESSION, start, right, 0 );
				return -1;
			}
			regB = DaoParser_AddFieldConst( self, name );
			regC = self->locRegCount;
			if( regFix >=0 && start+1 == right ) regC = regFix;
			if( tokens[start-1]->type == DTOK_IDENTIFIER ){
				DaoParser_AddCode( self, DVM_GETMF, regLast, regB, regC, start-1, 0, start+1 );
			}else{
				DaoParser_AddCode( self, DVM_GETMF, regLast, regB, regC, start, 0, start+1 );
			}
			regLast = regC;
			if( regLast == self->locRegCount ) DaoParser_PushRegister( self );
			start += 2;
		}else{
			DaoParser_Error2( self, DAO_INVALID_EXPRESSION, start, right, 0 );
			start++;
			return -1;
		}
	}
	return regLast;
}
static int DaoParser_CheckDoubleEnum( DaoParser *self, int L, int R, uchar_t sepR, uchar_t sepC )
{
	int i, comma, Nsep, last2, nColm;
	int last = L;
	int semi = DaoParser_FindOpenToken( self, sepR, L, R, 0 );
	size_t *colms;
	DArray *colms0 = DArray_New(0);
	while( semi >= 0 ){
		Nsep = 1;
		last2 = last;
		comma = DaoParser_FindOpenToken( self, sepC, last2, semi, 0 );
		while( comma >= 0 ){
			Nsep ++;
			last2 = comma+1;
			comma = DaoParser_FindOpenToken( self, sepC, last2, semi, 0 );
		}
		DArray_Append( colms0, Nsep );

		last = semi;
		semi = DaoParser_FindOpenToken( self, sepR, last+1, R, 0 );
	}
	Nsep = 1;
	last2 = last;
	comma = DaoParser_FindOpenToken( self, sepC, last2, R, 0 );
	while( comma >= 0 ){
		Nsep ++;
		last2 = comma+1;
		comma = DaoParser_FindOpenToken( self, sepC, last2, R, 0 );
	}
	DArray_Append( colms0, Nsep );
	colms = colms0->items.pSize;
	nColm = colms[0];
	for( i=1; i<colms0->size; i++){
		if( nColm != colms[i] ){
			DArray_Delete( colms0 );
			return -1;
		}
	}
	DArray_Delete( colms0 );
	return nColm;
}
static int DaoParser_MakeEnumMap( DaoParser *self, int vmc, int L, int R,
		int *N, int *cst, int regFix, DArray *cid )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int toksym = vmc == DVM_MAP ? DTOK_FIELD : DTOK_COLON;
	int nColm = DaoParser_CheckDoubleEnum( self, L, R, DTOK_COMMA, toksym );
	int reg, regC;
	if( nColm != 2 ){
		DaoParser_Error( self, DAO_CTW_ENUM_INVALID, NULL );
		return -1;
	}

	*N = 0;
	reg = DaoParser_MakeArithArray( self, L, R, N, cst, DTOK_COMMA, toksym, cid, 0 );
	if( reg < 0 ) return -1;

	regC = self->locRegCount;
	/*printf( "self->locRegCount = %i; regFix = %i\n", self->locRegCount, regFix ); */
	if( regFix>=0 ) regC = regFix;
	DaoParser_AddCode( self, vmc, reg, *N, regC, L-1, 0, R+1 );

	if( regC == self->locRegCount ) DaoParser_PushRegister( self );
	return regC;
}
static int DaoParser_MakeEnumMatrix( DaoParser *self, int L, int R, int *N, int *cst,
		int regFix, DArray *cid )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int nColm = DaoParser_CheckDoubleEnum( self, L, R, DTOK_SEMCO, DTOK_COMMA );
	int reg, regC, row, Brc;
	if( nColm<0 ){
		DaoParser_Error( self, DAO_CTW_ENUM_INVALID, NULL );
		return -1;
	}

	*N = 0;
	reg = DaoParser_MakeArithArray( self, L, R, N, cst, DTOK_COMMA, DTOK_SEMCO, cid, 0 );
	if( reg < 0 ) return -1;

	row = (*N) / nColm;
	if( row>=255 || nColm>=255 ) DaoParser_Error( self, DAO_CTW_ENUM_LIMIT, NULL );
	Brc = (row<<8) | nColm;

	regC = self->locRegCount;
	if( regFix>=0 ) regC = regFix;
	DaoParser_AddCode( self, DVM_MATRIX, reg, Brc, regC, L-1, 0, R+1 );

	if( regC == self->locRegCount ) DaoParser_PushRegister( self );
	return regC;
}
static void DaoParser_PushItemType( DaoParser *self, DaoType *type, int id, uchar_t sep1 )
{
	if( type && type->nested && type->nested->size ){
		DaoType *itp = NULL;
		switch( type->tid ){
		case DAO_ARRAY :
			if( sep1 == DTOK_COLON && id == 0 ){
				itp = type->nested->items.pType[0];
			}
			break;
		case DAO_LIST :
			if( sep1 == DTOK_COLON && id == 0 ){
				itp = type->nested->items.pType[0];
			}else{
				itp = type->nested->items.pType[0];
			}
			break;
		case DAO_MAP :
			if( type->nested->size > 1 ) itp = type->nested->items.pType[id%2];
			break;
		case DAO_TUPLE :
			itp = type->nested->items.pType[id];
			break;
		default : break;
		}
		DArray_PushFront( self->enumTypes, itp );
	}else{
		DArray_PushFront( self->enumTypes, NULL );
	}
}
int DaoParser_MakeArithArray( DaoParser *self, int left, int right, int *N,
		int *cst, uchar_t sep1, uchar_t sep2, DArray *cid, int state )
{
	DaoType *type = self->enumTypes->size ? self->enumTypes->items.pType[0] : NULL;
	DaoToken **tokens = self->tokens->items.pToken;
	int i, pos, id=0, regFix = self->locRegCount;
	int field = 0;
	int tok1, tok2;
	uchar_t nsep = 1 + (sep2 != 0);
	uchar_t seps[2];

	seps[0] = sep1;
	seps[1] = sep2;
	*cst = 1; /* fine, it is not used as a register id */
	*N = 0; /* [] */
	if( right >= left ){
		int comma, last, reg, c0;

		*N = 1;
		for(i=0; i<nsep; i++ ){
			last = left;
			comma = -1;
			comma = DaoParser_FindOpenToken( self, seps[i], left, right, 0);
			while( comma >= 0 && comma < right ){
				(*N) ++;
				last = comma + 1;
				comma = DaoParser_FindOpenToken( self, seps[i], last, right, 0);
			}
		}
		/*
		   printf( "self->locRegCount = %i; N = %i\n", self->locRegCount, *N );
		 */

		DaoParser_PushRegisters( self, *N );

		comma = right+1;
		last = left;
		pos = tokens[last]->line;
		for( i=0; i<nsep; i++ ){
			int com = DaoParser_FindOpenToken( self, seps[i], left, right, 0);
			if( com >= 0 && com<comma ) comma = com;
		}
		while( comma < right ){
			tok1 = tokens[last]->type;
			tok2 = last+1 <= right ? tokens[last+1]->type : 0;
			if( field && (tok1 != DTOK_IDENTIFIER || tok2 != DTOK_FIELD) ){
				DString_Clear( self->mbs );
				for(i=last; i<comma; i++) DString_Append( self->mbs, tokens[i]->string );
				DString_AppendMBS( self->mbs, ", name=>expression" );
				DaoParser_Error( self, DAO_CTW_IS_EXPECTED, self->mbs );
				return -1;
			}
			if( state == EXP_IN_CALL && tok1 == DTOK_IDENTIFIER && tok2 == DTOK_FIELD ){
				field = 1;
			}
			DaoParser_PushItemType( self, type, id++, sep1 );
			c0 = 0;
			reg = DaoParser_MakeArithTree( self, last, comma-1, & c0, regFix, state );
			DArray_PopFront( self->enumTypes );
			if( last > comma-1 || reg <0 ) return -1;
			if( ! c0 ) *cst = 0;

			/* Comment add_DVM_MOVE :
			 * If _reg_ is a variable or const result, add instruction to load it.
			 * else, _reg_ is an intermediate data, an instruction must have been added.
			 */
			if( reg != regFix ) DaoParser_AddCode( self, DVM_MOVE, reg, 1, regFix, last, 0, comma-1 );
			if( cid ) DArray_Append( cid, c0 );

			last = comma + 1;
			comma = right+1;
			pos = tokens[last]->line;
			for( i=0; i<nsep; i++ ){
				int com = DaoParser_FindOpenToken( self, seps[i], last, right, 0);
				if( com >= 0 && com<comma ) comma = com;
			}
			regFix ++;
		}
		tok1 = tokens[last]->type;
		tok2 = last+1 <= right ? tokens[last+1]->type : 0;
		if( field && (tok1 != DTOK_IDENTIFIER || tok2 != DTOK_FIELD) ){
			DString_Clear( self->mbs );
			for(i=last; i<=right; i++) DString_Append( self->mbs, tokens[i]->string );
			DString_AppendMBS( self->mbs, ", name=>expression" );
			DaoParser_Error( self, DAO_CTW_IS_EXPECTED, self->mbs );
			return -1;
		}
		DaoParser_PushItemType( self, type, id, sep1 );
		c0 = 0;
		reg = DaoParser_MakeArithTree( self, last, right, & c0, regFix, state );
		DArray_PopFront( self->enumTypes );
		if( reg < 0 ) return -1;
		if( ! c0 ) *cst = 0;

		/* See Comment add_DVM_MOVE : */
		if( reg != regFix ) DaoParser_AddCode( self, DVM_MOVE, reg, 1, regFix, last, 0, right );
		if( cid ) DArray_Append( cid, c0 );
	}
	return regFix-(*N)+1;
}
static int DaoParser_FindRootOper( DaoParser *self, int start, int end, int *optype )
{
	DaoToken **tokens = self->tokens->items.pToken;
	unsigned char p, max = 0;
	int imax=0, bc1, bc2, bc3, i = 0;
	bc1 = bc2 = bc3 = 0;
	for(i=start; i<=end; i++){
		int tname = tokens[i]->name;
		int t = i < end ? tokens[i+1]->type : 0;
		if( tname == DTOK_ARROW ){
			i ++;
			if( tokens[i]->name == DTOK_PIPE ){
				i ++;
				while( tokens[i]->type == DTOK_IDENTIFIER ){
					i ++;
					if( tokens[i]->type == DTOK_PIPE ) break;
					if( tokens[i]->type == DTOK_COMMA ) i ++;
				}
				if( tokens[i]->type == DTOK_PIPE ) i ++;
			}
			if( i > end ) break;
		}else if( tname >= DKEY_ENUM && tname <= DKEY_LIST && t == DTOK_LT ){
			/* type names: list<int>, enum<a,b,c> ... */
			i = DaoParser_FindPairToken( self, DTOK_LT, DTOK_GT, i, end );
			if( i < 0 ) return -1000;
			continue;
		}else if( tokens[i]->type == DTOK_IDENTIFIER && (t == DTOK_COLON2 || t == DTOK_LT) ){
			DValue scope = daoNullValue;
			DValue value = daoNullValue;
			int pos = DaoParser_FindScopedData( self, i, & scope, & value, 0, NULL );
			if( pos >= 0 ){
				i = pos;
				continue;
			}
		}
		switch( tokens[i]->name ){
		case DTOK_LB : bc1 ++; break;
		case DTOK_RB : bc1 --; break;
		case DTOK_LCB : bc2 ++; break;
		case DTOK_RCB : bc2 --; break;
		case DTOK_LSB : bc3 ++; break;
		case DTOK_RSB : bc3 --; break;
		default : break;
		}
		if( bc1 | bc2 | bc3 ) continue;
		/* except :, binary operator can not take empty operand: */
		if( tokens[i]->name != DTOK_COLON && ( i == start || i == end ) ) continue;
		/* operator near . is not regarded as a binary operator: */
		if( i > start && tokens[i-1]->name == DTOK_DOT ) continue;
		if( i < end && tokens[i+1]->name == DTOK_DOT ) continue;

		p = daoArithOper[ tokens[i]->name ].binary;
		if( p >= max ){
			max = p;
			imax = i;
		}
	}
	i = imax;
	if( max ){
		int adjusted = 0;
		int t1 = tokens[i-1]->name;
		int t2 = tokens[i]->name;
		int d1 = tokens[i]->cpos - tokens[i-1]->cpos;
		int d2 = tokens[i+1]->cpos - tokens[i]->cpos;
		int e1 = tokens[i]->name == tokens[i-1]->name || tokens[i]->name == DTOK_ASSN;
		int e2 = tokens[i+1]->name == tokens[i]->name || tokens[i+1]->name == DTOK_ASSN;

		/* adjust tokens start with < or > : */
		if( (e1 && t1 == DTOK_LT && d1 ==1) || (e2 && t2 == DTOK_LT && d2 ==1) ){
			i = imax - (e1 && t1 == DTOK_LT && d1 ==1);
			if( tokens[i+1]->name == DTOK_LT ){ /* << */
				tokens[i]->type = DTOK_LSHIFT;
				tokens[i]->name = DTOK_LSHIFT;
				adjusted = 1;
			}else if( tokens[i+1]->name == DTOK_ASSN ){ /* <= */
				tokens[i]->type = DTOK_LE;
				tokens[i]->name = DTOK_LE;
				adjusted = 1;
			}
		}else if( (e1 && t1 == DTOK_GT && d1 ==1) || (e2 && t2 == DTOK_GT && d2 ==1) ){
			i = imax - (e1 && t1 == DTOK_GT && d1 ==1);
			if( tokens[i+1]->name == DTOK_GT ){ /* >> */
				tokens[i]->type = DTOK_RSHIFT;
				tokens[i]->name = DTOK_RSHIFT;
				adjusted = 1;
			}else if( tokens[i+1]->name == DTOK_ASSN ){ /* >= */
				tokens[i]->type = DTOK_GE;
				tokens[i]->name = DTOK_GE;
				adjusted = 1;
			}
		}
		if( adjusted ){
			tokens[i+1]->type = DTOK_BLANK;
			tokens[i+1]->name = DTOK_BLANK;
			/* priority could be different, search again */
			return DaoParser_FindRootOper( self, start, end, optype );
		}
		*optype = daoArithOper[ tokens[imax]->name ].oper;
		return imax;
	}
	return -1000;
}
extern DValue DaoParseNumber( DaoParser *self, DaoToken *tok, DLong *bigint )
{
	char *str = tok->string->mbs;
	DValue value = daoNullValue;
	size_t pl = 0;
	if( tok->name == DTOK_NUMBER_SCI ){
		if( DString_FindChar( tok->string, 'e', 0 ) != MAXSIZE ){
			value.t = DAO_FLOAT;
			value.v.f = strtod( str, 0 );
		}else{
			value.t = DAO_DOUBLE;
			value.v.d = strtod( str, 0 );
		}
	}else if( tok->name == DTOK_DOUBLE_DEC ){
		value.t = DAO_DOUBLE;
		/*errno = 0;*/
		value.v.d = strtod( str, 0 );
	}else if( tok->name == DTOK_NUMBER_DEC ){
		value.t = DAO_FLOAT;
		value.v.f = strtod( str, 0 );
	}else if( bigint && (pl = DString_FindChar(tok->string, 'L', 0)) != MAXSIZE ){
		char ec = DLong_FromString( bigint, tok->string );
		if( ec ){
			if( ec == 'L' ){
				DString_SetMBS( self->mbs, tok->string->mbs + pl );
				DaoParser_Error( self, DAO_INVALID_RADIX, self->mbs );
			}else{
				DString_Clear( self->mbs );
				DString_AppendChar( self->mbs, ec );
				DaoParser_Error( self, DAO_INVALID_DIGIT, self->mbs );
			}
			DaoParser_Error( self, DAO_INVALID_LITERAL, tok->string );
			return value;
		}
		value.t = DAO_LONG;
		value.v.l = bigint;
	}else{
		value.t = DAO_INTEGER;
		value.v.i = (sizeof(dint) == 4) ? strtol( str, 0, 0 ) : strtoll( str, 0, 0 );
	}
	return value;
}
static int DaoParser_MakeArithLeaf( DaoParser *self, int start, int *cst )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoRoutine *routine = self->routine;
	DString *str = tokens[start]->string;
	DValue value = daoNullValue;
	DNode *node;
	int varReg, pos = tokens[start]->line;
	char *tok = tokens[start]->string->mbs;
	unsigned char tki = 0;

	/*printf("DaoParser_makeArithLeaf()\n"); */

	*cst = 0;
	varReg = DaoParser_GetRegister( self, tokens[start] );
	tki = tokens[start]->name;
	/*
	   printf("name=%s; %i\n", tok, varReg);
	 */
	if( varReg >= 0 ){
		value = DaoParser_GetVariable( self, varReg );
		if( LOOKUP_ST( varReg ) & 1 ) *cst = varReg;
		/*
		   printf("value = %i; %i; c : %i\n", value.t, varReg, *cst );
		 */
	}else if( tki == DTOK_MBS || tki == DTOK_WCS ){
		if( ( node = MAP_Find( self->allConsts, str ) )==NULL ){
			value.t = DAO_STRING;
			value.v.s = self->str;
			DString_ToMBS( self->str );
			DString_SetDataMBS( self->str, tok + 1, str->size-2 );
			if( ! ( self->vmSpace->options & DAO_EXEC_MBS_ONLY ) ){
				if( tok[0] == '"' ) DString_ToWCS( self->str );
			}
			if( self->vmSpace->options & DAO_EXEC_WCS_ONLY ) DString_ToWCS( self->str );
			MAP_Insert( self->allConsts, str, routine->routConsts->size );
			DRoutine_AddConstValue( (DRoutine*)routine, value );
		}
		varReg = LOOKUP_BIND_LC( MAP_Find( self->allConsts, str )->value.pInt );
		*cst = varReg;
	}else if( tki >= DTOK_DIGITS_HEX && tki <= DTOK_NUMBER_SCI ){
		if( ( node = MAP_Find( self->allConsts, str ) )==NULL ){
			value = DaoParseNumber( self, tokens[start], self->bigint );
			if( value.t == 0 ) return -1;
			MAP_Insert( self->allConsts, str, routine->routConsts->size );
			DRoutine_AddConstValue( (DRoutine*)routine, value );
		}
		varReg = LOOKUP_BIND_LC( MAP_Find( self->allConsts, str )->value.pInt );
		*cst = varReg;
	}else if( tki == DTOK_ID_SYMBOL ){
		DaoType *type = DaoNameSpace_FindType( self->nameSpace, str );
		if( type == NULL ){
			type = DaoType_New( str->mbs, DAO_ENUM, NULL, NULL );
			type->mapNames = DMap_New(D_STRING,0);
			DString_Assign( self->mbs, str );
			DString_Erase( self->mbs, 0, 1 );
			DMap_Insert( type->mapNames, self->mbs, (void*)0 );
			DaoNameSpace_AddType( self->nameSpace, str, type );
		}
		value = daoNullValue;
		value.t = DAO_ENUM;
		value.v.e = self->denum;
		self->denum->value = 0;
		DEnum_SetType( self->denum, type );
		varReg = DaoNameSpace_AddConst( self->nameSpace, str, value, DAO_DATA_PUBLIC );
		if( varReg <0 ) return -1;
		*cst = varReg;
	}else if( tki == DTOK_DOLLAR ){
		varReg = DaoParser_ImaginaryOne( self, start );
		*cst = varReg;
	}else if( tki == DTOK_COLON ){
		if( ( node = MAP_Find( self->allConsts, str ) )==NULL ){
			DaoPair *pair = DaoPair_New( daoNullValue, daoNullValue );
			pair->trait |= DAO_DATA_CONST;
			MAP_Insert( self->allConsts, str, routine->routConsts->size );
			DRoutine_AddConst( (DRoutine*)routine, (DaoBase*)pair );
		}
		varReg = LOOKUP_BIND_LC( MAP_Find( self->allConsts, str )->value.pInt );
		*cst = varReg;
	}else{
		*cst = 0;
		DaoParser_Error( self, DAO_SYMBOL_NOT_DEFINED, str );
		return -1;
	}
	return DaoParser_GetNormRegister( self, varReg, start, 0, start );
}

static int DaoParser_StripParenthesis( DaoParser *self, int *start, int *end );

static int DaoParser_MakeArithUnary( DaoParser *self, int oper, int start, int end,
		int *cst, int regFix, int state, int left_unary )
{
	int c = 0;
	int cgeto = -1, cgeta = -1;
	int reg, pos = self->tokens->items.pToken[ start ]->line;
	int first = start - (left_unary != 0);
	int last = end + (left_unary == 0);
	ushort_t opimag = 0;
	ushort_t  opc = 0;
	DaoNameSpace *myNS = self->nameSpace;
	DValue value = daoNullValue;
	DString *str = self->tokens->items.pToken[start]->string;
	complex16 com = { 0.0, 1.0 };
	DaoInode *vmc = self->vmcFirst;
	DaoInode *front = self->vmcFirst;
	DaoInode *back = self->vmcLast;
	cgeto = vmc->code;
	cgeta = vmc->a;

	reg = DaoParser_MakeArithTree( self, start, end, & c, -1, state );
	if( reg < 0 ) return -1;
	if( c ){
		if( oper ==DAO_OPER_INCR || oper ==DAO_OPER_DECR || oper ==DAO_OPER_BIT_AND ){
			DaoParser_Error( self, DAO_CTW_MODIFY_CONST, NULL );
			return -1;
		}
	}

	switch( oper ){
	case DAO_OPER_NOT :
		opc = DVM_NOT; break;
	case DAO_OPER_INCR :
		opc = DVM_INCR; break;
	case DAO_OPER_DECR :
		opc = DVM_DECR; break;
	case DAO_OPER_SUB :
		opc = DVM_UNMS; break;
	case DAO_OPER_IMAGIN :
		opimag = DaoParser_ImaginaryOne( self, start );
		opc = DVM_MUL;
		value = daoNullComplex;
		value.v.c = & com;
		break;
	case DAO_OPER_ADD :
		if( c ) *cst = c;
		return DaoParser_GetNormRegister( self, c, start, 0, end );
		break;
	case DAO_OPER_TILDE :
		opc = DVM_BITREV;
		break;
	case DAO_OPER_BIT_AND :
		break;
	case DAO_OPER_ASSERT :
		return -1;
	default :
		DaoParser_Error( self, DAO_CTW_EXPR_INVALID, str );
		return -1;
	}

	if( c && opc != DVM_INCR && opc != DVM_DECR ){
		int i = DaoParser_PopCodes( self, front, back );
		DaoParser_PopRegisters( self, i );
		value = DaoVmProcess_MakeArithConst( myNS->vmpEvalConst, opc,
				DaoParser_GetVariable( self, c ), value );
		if( value.t == 0 ){
			DaoParser_Error( self, DAO_CTW_INV_CONST_EXPR, NULL );
			return -1;
		}
		*cst = DRoutine_AddConstValue( (DRoutine*)self->routine, value );
		*cst = LOOKUP_BIND_LC( *cst );
		return DaoParser_GetNormRegister( self, *cst, start, 0, end );
	}else if( opc == DVM_INCR || opc == DVM_DECR ){
		int regB, regLast = reg;
		DaoInode *vmc = self->vmcLast;
		if( regFix >= 0 ) regLast = regFix;
		if( opc==DVM_INCR ) opc = DVM_ADD; else opc = DVM_SUB;
		regB = DaoParser_IntegerOne( self, start );
		DaoParser_AddCode( self, opc, reg, regB, reg, first, 0, last );
		if( reg != regLast ) DaoParser_AddCode( self, DVM_MOVE, reg, 2, regLast, first, 0, last );
		if( vmc->code >= DVM_GETI && vmc->code <= DVM_GETMF ){
			DaoParser_PushBackCode( self, (DaoVmCodeX*) vmc );
			vmc = self->vmcLast;
			regB = vmc->a;
			vmc->a = vmc->c;
			vmc->c = regB;
			if( vmc->code == DVM_GETI )
				vmc->code = DVM_SETI;
			else if( vmc->code == DVM_GETF )
				vmc->code = DVM_SETF;
			else if( vmc->code == DVM_GETMF )
				vmc->code = DVM_SETMF;
		}
		return regLast;
	}else if( oper == DAO_OPER_BIT_AND ){
		int st = 0, up = 0;
		int ok = 0;
		if( regFix >=0 && state == EXP_IN_CALL ){
			if( ! DaoParser_StripParenthesis( self, & start, & end ) ) return -1;
			reg = -1;
			if( start == end ){
				reg = DaoParser_GetRegister( self, self->tokens->items.pToken[start] );
				st = LOOKUP_ST( reg );
				up = LOOKUP_UP( reg );
			}
			if( reg >=0 && st == DAO_LOCAL_VARIABLE && up ==0
					&& MAP_Find( self->routine->localVarType, reg ) ){
				ok = 1;
				DaoParser_AddCode( self, DVM_LOAD, reg, 1, regFix, first, 0, last );
			}
		}
		if( ok ==0 ){
			DString_SetMBS( self->mbs,
					"reference can only be used for local variables in parameter list!" );
			DaoParser_Error( self, DAO_CTW_INVA_SYNTAX, self->mbs );
			return -1;
		}
		return regFix;

	}else if( oper != DAO_OPER_ADD ){
		int regLast = self->locRegCount;
		if( regFix >= 0 ) regLast = regFix;
		DaoParser_AddCode( self, opc, reg, opimag, regLast, first, 0, last );
		if( regLast == self->locRegCount ) DaoParser_PushRegister( self );
		return regLast;
	}else{
		return reg;
	}
	return 0;
}
static int DaoParser_StripParenthesis( DaoParser *self, int *start, int *end )
{
	DaoToken **tokens = self->tokens->items.pToken;
	/* adjusted for <<, >>, <= or >= as two tokens */
	if( tokens[*start]->name == DTOK_BLANK ) (*start) ++;
	if( tokens[*end]->name == DTOK_BLANK ) (*end) --;
	while( tokens[*start]->name == DTOK_LB && tokens[*end]->name == DTOK_RB ){
		int rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, *start, *end );
		int comma = DaoParser_FindOpenToken( self, DTOK_COMMA, *start+1, *end, 0 );
		if( rb == *end && comma <0 ){
			(*start) ++;
			(*end) --;
		}else if( rb < 0 ){
			return 0;
		}else{
			return 1;
		}
	}
	return 1;
}
/* enclosed function definition:
 *
 * routine foo()
 * {
 *   bar = @( x : TYPE, y = DEFAULT ){ # bar = routine( ... ){ ... }
 *     code_block;
 *   }
 *   return bar;
 * }
 *
 */
static int DaoParser_ExpClosure( DaoParser *self, int start, int end, int regFix )
{
	int i;
	int rb, lb2 = -1, rb2 = -1, npar = 0, regCall;
	int tokPos = self->tokens->items.pToken[ start ]->line;
	DaoNameSpace *myNS = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DaoClass  *klass = self->hostClass;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoRoutine *rout;
	DaoParser *parser;
	DArray *uplocs;
	DString *mbs = DString_New(1);
	DaoToken tok = { DTOK_SEMCO, DTOK_SEMCO, 0, 0, 0, NULL };

	tok.line = tokPos;
	tok.string = mbs;
	rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, end );
	if( rb >0  ) lb2 = DaoParser_FindOpenToken( self, DTOK_LCB, rb+1, end, 1 );
	if( lb2 >0 ) rb2 = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, rb+1, end );
	if( rb < 0 || lb2 < 0 || rb2 < 0 ||
			(tokens[rb+1]->name != DTOK_LCB && tokens[rb+1]->name != DTOK_COLON) ){
		DaoParser_Error( self, DAO_CTW_INVA_SYNTAX, NULL );
		return -1;
	}
	parser = DaoParser_New();
	rout = DaoRoutine_New();
	parser->routine = rout;
	parser->levelBase = self->levelBase + self->lexLevel + 1;
	GC_ShiftRC( self->nameSpace, rout->nameSpace );
	parser->nameSpace = rout->nameSpace = self->nameSpace;
	parser->vmSpace = self->vmSpace;
	parser->outParser = self;
	parser->uplocs = uplocs = DArray_New(0);
	DString_Assign( parser->fileName, self->fileName );
	if( self->hostClass ){
		GC_ShiftRC( self->hostClass->objType, rout->routHost );
		rout->tidHost = DAO_OBJECT;
		rout->routHost = self->hostClass->objType;
	}
	DArray_Append( myNS->definedRoutines, rout );

	parser->defParser = self;
	parser->parStart = start + 1;
	parser->parEnd = lb2 - 1;
	if( DaoParser_ParseParams( parser ) == 0 ) goto ErrorParamParsing;

	for( i=lb2+1; i<rb2; i++ ) DArray_Append( parser->tokens, tokens[i] );
	if( tokens[rb2-1]->name != DTOK_SEMCO ){
		DString_SetMBS( mbs, ";" );
		tok.line = tokens[rb2-1]->line;
		tok.string = mbs;
		DArray_Append( parser->tokens, & tok );
	}
	if( ! DaoParser_ParseRoutine( parser ) ){
		DString_SetMBS( mbs, "invalid anonymous function" );
		goto ErrorParamParsing;
	}

	regCall = self->locRegCount;
	DaoParser_PushRegister( self );
	for( i=0; i<uplocs->size; i+=4 ){
		int up = uplocs->items.pInt[i];
		int loc = uplocs->items.pInt[i+1];
		int first = uplocs->items.pInt[i+2];
		int last = uplocs->items.pInt[i+3];
		DaoParser_AddCode( self, DVM_MOVE, up, 0, regCall+i+1, first, 0, last );
		DaoParser_AddCode( self, DVM_DATA, DAO_INTEGER, loc, regCall+i+2, first,0,last );
	}
	DaoParser_PushRegisters( self, uplocs->size/2 );

	i = DRoutine_AddConst( (DRoutine*)routine, (DaoBase*)rout );
	DaoParser_AddCode( self, DVM_GETCL, 0, i, regCall, start, rb, end );

	if( regFix < 0 ){
		regFix = self->locRegCount;
		DaoParser_PushRegister( self );
	}
	/* DVM_ROUTINE rout_proto, upv1, upv2, ..., regFix */
	DaoParser_AddCode( self, DVM_ROUTINE, regCall, uplocs->size/2, regFix, start, rb, end );
	DString_Delete( mbs );
	DaoParser_Delete( parser );
	return regFix;
ErrorParamParsing:
	DaoParser_Delete( parser );
	GC_IncRC( rout );
	GC_DecRC( rout );
	DString_Delete( mbs );
	return -1;
}
static int DaoParser_ClassExpressionBody( DaoParser *self, int start, int end )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoNameSpace *myNS = self->nameSpace;
	DaoInterface *oldHostInter = self->hostInter;
	DaoType *oldHostCData = self->hostCData;
	DaoType *oldHostType = self->hostType;
	DaoClass *oldHostClass = self->hostClass;
	DaoClass *klass = DaoClass_New();
	DaoRoutine *rout;
	DaoVmCodeX vmcx = {DVM_RETURN,0,0,0,0,0,0,0,0};
	DaoVmCode  vmc = {DVM_RETURN,0,0,0};
	char buf[50];
	int i;

	sprintf( buf, "AnonymousClass%p", klass );
	DString_SetMBS( klass->className, buf );

	DaoClass_SetName( klass, klass->className );
	DaoClass_DeriveClassData( klass );
	rout = klass->classRoutine;
	rout->defLine = tokens[start]->line;
	GC_ShiftRC( myNS, rout->nameSpace );
	rout->nameSpace = myNS;

	vmcx.line = rout->defLine;
	DArray_Append( rout->annotCodes, & vmcx );
	DaoVmcArray_Append( rout->vmCodes, vmc );

	klass->protoValues = DMap_New(0,D_STRING);
	self->hostClass = klass;
	self->hostType = klass->objType;
	self->isClassBody += 1;
	self->isDynamicClass += 1;
	DaoParser_AddScope( self, DVM_UNUSED, start );
	if( DaoParser_ParseCodeSect( self, start, end )==0 ){
		GC_IncRC( klass );
		GC_DecRC( klass );
		return 0;
	}
	DaoParser_CompleteScope( self, start );
	DaoClass_DeriveObjectData( klass );
	self->isClassBody -= 1;
	self->isDynamicClass -= 1;
	self->hostInter = oldHostInter;
	self->hostCData = oldHostCData;
	self->hostClass = oldHostClass;
	self->hostType = oldHostType;

	return DRoutine_AddConst( (DRoutine*)self->routine, (DaoBase*)klass ) + 1;
}
static int DaoParser_ClassExpression( DaoParser *self, int start, int end, int regFix )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int rb, mid, reg1, reg2 = 0, regC, tki, cst = 0;
	tki = tokens[start+1]->name;
	rb = DaoParser_FindPairToken2( self, DTOK_LB, DTOK_RB, start+1, end );
	if( rb <0 ){
		return -1;
	}
	tokens[start]->name = DKEY_TUPLE;
	tokens[start+1]->type = tokens[start+1]->name = DTOK_LCB;
	tokens[rb]->type = tokens[rb]->name = DTOK_RCB;
	reg1 = DaoParser_MakeArithTree( self, start, rb, & cst, -1, 0 );
	tokens[start]->name = DKEY_CLASS;
	tokens[start+1]->type = tokens[start+1]->name = DTOK_LB;
	tokens[rb]->type = tokens[rb]->name = DTOK_RB;
	if( reg1 <0 ){
		return -1;
	}
	mid = rb;
	if( rb < end && tokens[rb+1]->name == DTOK_LCB ){
		rb = DaoParser_FindPairToken2( self, DTOK_LCB, DTOK_RCB, rb+1, end );
		if( rb <0 ) return -1;
		reg2 = DaoParser_ClassExpressionBody( self, mid + 2, rb - 1 );
		if( reg2 ==0 ) return -1;
	}
	regC = regFix;
	if( regFix < 0 ){
		regC = self->locRegCount;
		DaoParser_PushRegister( self );
	}
	DaoParser_AddCode( self, DVM_CLASS, reg1, reg2, regC, start, mid, rb );
	return regC;
}
static int DaoParser_MakeArithTree2( DaoParser *self, int start, int end,
		int *cst, int regFix, int state )
{
	int i, rb, optype, pos, tokPos, reg1, reg2, reg3, regC;
	int cgeto = -1, cgetc = -1;
	int was, c1, c2, comma;
	DaoToken **tokens = self->tokens->items.pToken;
	DString *mbs = self->mbs;
	DaoInode *front = self->vmcFirst;
	DaoInode *back = self->vmcLast;
	DaoInode *vmc;
	DaoVmCodeX vmcValue;

	DaoNameSpace *myNS = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DArray *cid = NULL;
	DValue value;
	unsigned char tki, tki2;
	unsigned char typed_data_enum = 0;
	unsigned short mid = 0;

	*cst = 0;
	if( start > end ){
		*cst = DaoParser_NullValue( self, start );
		return *cst;
	}

	if( ! DaoParser_StripParenthesis( self, & start, & end ) ) goto ParsingError;

	if( start == end ){
		DaoInode *first = self->vmcFirst;
		DaoInode *last = self->vmcLast;
		regC = DaoParser_MakeArithLeaf( self, start, cst );
		return regC;
	}

	tki = tokens[start]->name;
	tki2 = (start+1 <= end) ? tokens[start+1]->name : 0;
	if( tki >= DKEY_ARRAY && tki <= DKEY_LIST && tki2 == DTOK_LCB ){
		typed_data_enum = tki;
		tki = tki2;
		start += 1;
	}
	if( tki == DTOK_AT || tki == DKEY_ROUTINE || tki == DKEY_FUNCTION || tki == DKEY_SUB ){
		/* closure expression */
		tki = tokens[start+1]->name;
		if( tki == DTOK_LB ){
			return DaoParser_ExpClosure( self, start, end, regFix );
		}else{
			DString_SetMBS( self->mbs, "using removed feature" );
			DaoParser_Error( self, DAO_CTW_INVA_SYNTAX, self->mbs );
			return -1;
		}
	}else if( tki == DKEY_CLASS ){
		return DaoParser_ClassExpression( self, start, end, regFix );
	}
	vmc = self->vmcFirst;
	cgeto = vmc->code;
	cgetc = vmc->c;

	pos = -1;
	optype = -1;
	pos = DaoParser_FindRootOper( self, start, end , & optype );
	if( optype == DAO_OPER_LLT || optype == DAO_OPER_SUB || optype == DAO_OPER_DIV ){
		int opt = 0;
		int t = DaoParser_FindRootOper( self, pos+1, end , & opt );
		while( opt == optype && t > 0 ){
			pos = t;
			t = DaoParser_FindRootOper( self, pos+1, end , & opt );
		}
	}
	if( pos >=0 ) mid = pos;

	/* decide the register for result: */
	regC = self->locRegCount;
	if( regFix >= 0 ) regC = regFix;

	was = self->warnAssn;
	self->warnAssn = 1;
	tokPos = pos < 0 ? tokens[start]->line : tokens[ pos ]->line;

	tki = tokens[start]->name;
	if( pos < 0 && tki == DTOK_LB ){
		rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, end );
		if( rb < 0 ) goto ParsingError;
		comma = DaoParser_FindOpenToken( self, DTOK_COMMA, start+1, rb, 0 );
		if( comma <0 || rb < end ){
			tki = tokens[rb+1]->name;
			if( tki == DTOK_LB || tki == DTOK_LSB || tki == DTOK_LCB || tki >DAO_NOKEY1
					|| (tki >= DTOK_IDENTIFIER && tki <= DTOK_WCS ) ){
				/* type casting expression */
				int it, newpos = 0;
				DaoType *abtp = DaoParser_ParseType( self, start+1, rb-1, & newpos, NULL );
				if( abtp == NULL || newpos != rb ) goto ParsingError; /*XXX abtp memory*/
				regC = regFix;
				if( regFix < 0 ){
					regC = self->locRegCount;
					DaoParser_PushRegister( self );
				}
				MAP_Insert( self->routine->localVarType, regC, abtp );
				reg1 = DaoParser_MakeArithTree( self, rb+1, end, & c1, -1, state );
				it = DaoRoutine_AddConst( self->routine, abtp );
				DaoParser_AddCode( self, DVM_CAST, reg1, it, regC, start, rb, end );
				return regC;
			}
		}
	}

	tki2 = tokens[end]->name;
	if( pos > start || optype == DAO_OPER_COLON || optype == DAO_OPER_FIELD ){

		int notin = 0;
		int end2   = pos-1;
		int start2 = pos+1;
		short code = mapAithOpcode[optype];

		/* empty operand expression for binary operator only allowed for
		   colon as default index start/end range */
		if( optype != DAO_OPER_COLON && (start == pos || pos == end) ) goto ParsingError;

		c1 = c2 = 0;
		reg1 = reg2 = -1;
		if( optype >= DAO_OPER_ASSN && optype <= DAO_OPER_ASSN_OR && pos > start+1 ){
			short tkt = tokens[pos-1]->type;
			short tkn = tokens[pos-1]->name;
			short tkn2 = tokens[pos-2]->name;
			regC = (optype == DAO_OPER_ASSN) ? regFix : -1;
			c1 = 0;
			if( tokens[pos]->name == DTOK_ASSN && was )
				DaoParser_Warn( self, DAO_CTW_ASSIGN_INSIDE, NULL );
			if( tkt == DTOK_IDENTIFIER && (tkn2 == DTOK_DOT || tkn2 == DTOK_COLON2 || tkn2 == DTOK_ARROW) ){
				i = DaoParser_GetClassMember( self, tokens[pos-1]->string );
				/* for operator .field, i will be <0 */
				if( i >=0 && start == pos-3 && tokens[start]->name == DKEY_SELF ){
					int st, up, id;
					reg2 = DaoParser_MakeArithTree( self, pos+1, end, & c2, regC, state );
					if( reg2 < 0 ) goto ParsingError;
					st = LOOKUP_ST( i );
					up = LOOKUP_UP( i );
					id = LOOKUP_ID( i );
					if( st & 1 ){
						DaoParser_Error( self, DAO_CTW_MODIFY_CONST, tokens[pos-1]->string );
						return -1;
					}else if( st == DAO_CLASS_VARIABLE ){
						DaoParser_AddCode( self, DVM_SETVK, reg2, id, up, start, mid, end );
					}else if( st == DAO_OBJECT_VARIABLE ){
						DaoParser_AddCode( self, DVM_SETVO, reg2, id, up, start, mid, end );
					}else{
						DString_SetMBS( self->mbs, "not a class member" );
						DaoParser_Error( self, DAO_CTW_EXPR_INVALID, self->mbs );
						return -1;
					}
				}else{
					int fget = tkn2 == DTOK_ARROW ? DVM_GETMF : DVM_GETF;
					int fset = tkn2 == DTOK_ARROW ? DVM_SETMF : DVM_SETF;
					reg1 = DaoParser_MakeArithTree( self, start, pos-3, & c1, -1, state );
					if( reg1 < 0 ) goto ParsingError;
					reg2 = DaoParser_MakeArithTree( self, pos+1, end, & c2, regC, state );
					if( reg2 < 0 ) goto ParsingError;
					reg3 = DaoParser_AddFieldConst( self, tokens[pos-1]->string );
					if( optype == DAO_OPER_ASSN ){
						DaoParser_AddCode( self, fset, reg2, reg3, reg1, start, mid, end );
					}else{
						regC = self->locRegCount;
						if( regFix >= 0 ) regC = regFix;
						DaoParser_AddCode( self, fget, reg1, reg3, regC, start, 0, mid-1 );
						DaoParser_AddCode( self, -code, regC, reg2, regC, start, mid, end );
						DaoParser_AddCode( self, fset, regC, reg3, reg1, start, mid, end );
						if( regC == self->locRegCount ) DaoParser_PushRegister( self );
					}
				}
				return self->vmcLast->a;
			}else if( tkn == DTOK_RSB ){
				int lb, bal = -1;
				for(lb=pos-2; lb>start; lb--){
					if( tokens[lb]->name == DTOK_LSB ){
						bal ++;
						if( bal == 0 ) break;
					}else if( tokens[lb]->name == DTOK_RSB ){
						bal --;
					}
				}
				if( lb == start ) goto ParsingError;
				reg1 = DaoParser_MakeArithTree( self, start, lb-1, & c1, -1, state );
				if( reg1 < 0 ) goto ParsingError;
				if( DaoParser_FindOpenToken( self, DTOK_COMMA, lb+1, pos-2, 0 ) < 0 ){
					reg2 = DaoParser_MakeArithTree( self, lb+1, pos-2, & c2, -1, state );
				}else{
					int regEn, N;
					regEn = DaoParser_MakeArithArray( self, lb+1, pos-2, & N, & c2, DTOK_COMMA, 0, NULL, 0 );
					if( regEn < 0 ) goto ParsingError;
					reg2 = self->locRegCount;
					DaoParser_AddCode( self, DVM_TUPLE, regEn, N, reg2, start, mid, end );
					DaoParser_PushRegister( self );
				}
				if( reg2 < 0 ) goto ParsingError;
				reg3 = DaoParser_MakeArithTree( self, pos+1, end, & c2, regC, state );
				if( reg3 < 0 ) goto ParsingError;
				if( optype == DAO_OPER_ASSN ){
					DaoParser_AddCode( self, DVM_SETI, reg3, reg2, reg1, start, mid, end );
				}else{
					regC = self->locRegCount;
					if( regFix >= 0 ) regC = regFix;
					DaoParser_AddCode( self, DVM_GETI, reg1, reg2, regC, start, 0, mid-1 );
					DaoParser_AddCode( self, -code, regC, reg3, regC, start, mid, end );
					DaoParser_AddCode( self, DVM_SETI, regC, reg2, reg1, start, mid, end );
					if( regC == self->locRegCount ) DaoParser_PushRegister( self );
				}
				return self->vmcLast->a;
			}
			goto ParsingError;
		}else if( optype >= DAO_OPER_ASSN && optype <= DAO_OPER_ASSN_OR ){
			int st, up, id, opb, opc, get = DVM_GETVL, set = DVM_SETVL;
			DaoParser_DeclareVariable( self, tokens[start], 0, NULL );
			reg1 = DaoParser_GetRegister( self, tokens[start] );
			if( reg1 < 0 ) goto ParsingError;
			st = LOOKUP_ST( reg1 );
			up = LOOKUP_UP( reg1 );
			id = LOOKUP_ID( reg1 );
			opb = id; opc = up;
			if( st == DAO_LOCAL_VARIABLE && up ==0 ){
				opb = 0;
				opc = id;
				get = DVM_MOVE;
				set = DVM_MOVE;
			}else if( st == DAO_LOCAL_VARIABLE ){
				opb = id;
				opc = 1;
				get = DVM_GETVL;
				set = DVM_SETVL;
			}else if( st == DAO_GLOBAL_VARIABLE ){
				get = DVM_GETVG;
				set = DVM_SETVG;
			}else if( st == DAO_CLASS_VARIABLE ){
				get = DVM_GETVK;
				set = DVM_SETVK;
			}else if( st == DAO_OBJECT_VARIABLE ){
				get = DVM_GETVO;
				set = DVM_SETVO;
			}else{
				DaoParser_Error( self, DAO_CTW_MODIFY_CONST, NULL );
				goto ParsingError;
			}
			regC = (optype == DAO_OPER_ASSN || get == set) ? regFix : -1;
			reg2 = DaoParser_MakeArithTree( self, pos+1, end, & c2, regC, state );
			if( reg2 < 0 ) goto ParsingError;
			if( optype == DAO_OPER_ASSN ){
				DaoParser_AddCode( self, set, reg2, opb, opc, start, mid, end );
			}else if( get == set ){
				DaoParser_AddCode( self, -code, opc, reg2, opc, start, mid, end );
			}else{
				regC = self->locRegCount;
				if( regFix >= 0 ) regC = regFix;
				DaoParser_AddCode( self, get, opc, opb, regC, start, 0, mid-1 );
				DaoParser_AddCode( self, -code, regC, reg2, regC, start, mid, end );
				if( set < DVM_SETVL || set > DVM_SETVG ) /* no need SETV, since GETV get reference */
					DaoParser_AddCode( self, set, regC, opb, opc, start, mid, end );
				if( regC == self->locRegCount ) DaoParser_PushRegister( self );
			}
			return self->vmcLast->a;
		}
		if( optype == DAO_OPER_IF ){
			DaoInode *vmcQuestion, *vmcColon;
			short ifType = 0;
			int c3 = 0;
			int colon;
			if( regC == self->locRegCount ) DaoParser_PushRegister( self );
			reg1 = DaoParser_MakeArithTree( self, start, end2, & c1, -1, state );
			if( c1 ){
				ifType = 1; /* const false */
				value = DaoParser_GetVariable( self, c1 );
				if( value.t && DValue_GetDouble( value ) ) ifType = 2; /* const true */
			}
			/* adding another layer of {} to avoid messing up the
			 * explicit control statements stack */
			DaoParser_AddCode( self, DVM_LBRA2, 0, 0, 0, mid, mid+1,0 );
			DaoParser_AddCode( self, DVM_IF, reg1, 0, 0, start, 0, end2 );
			vmcQuestion = self->vmcLast;
			DaoParser_AddCode( self, DVM_LBRA, 0, 0, 0, start, start+1, 0 );

			vmcColon = self->vmcLast;
			colon = DaoParser_FindOpenToken( self, DTOK_COLON, start, end, 1 );
			if( colon < 0 ) goto ParsingError;
			tokPos = tokens[ colon ]->line;

			/* in any case compile it to check syntax: */
			reg2 = DaoParser_MakeArithTree( self, start2, colon-1, & c2, -1, state );
			if( reg2 < 0 ) goto ParsingError;
			if( ifType == 1 ){
				while( self->vmcLast != vmcQuestion ) DaoParser_PopBackCode( self );
			}else if( ifType == 2 ){
				vmcQuestion->code = DVM_UNUSED;
				vmcColon->code = DVM_UNUSED;
				DaoParser_AddCode( self, DVM_MOVE, reg2, 0, regC, start, mid, end );
			}else{
				DaoParser_AddCode( self, DVM_MOVE, reg2, 0, regC, mid+1, 0, colon-1 );
				DaoParser_AddCode( self, DVM_RBRA, 0, 0, 0, start, start+1, 0 );
				DaoParser_AddCode( self, DVM_ELSE, 0, 0, 0, colon, 0, 0 );
				DaoParser_AddCode( self, DVM_LBRA, 0, 0, 0, mid, mid+1, 0 );
			}
			vmcColon = self->vmcLast;

			reg3 = -1;
			reg3 = DaoParser_MakeArithTree( self, colon+1, end, & c3, -1, state );
			if( reg3 < 0 ) goto ParsingError;
			if( ifType == 2 ){
				while( self->vmcLast != vmcColon ) DaoParser_PopBackCode( self );
			}else if( ifType == 1 ){
				DaoParser_AddCode( self, DVM_MOVE, reg3, 0, regC, start, mid, end );
			}else if( ifType == 0 ){
				DaoParser_AddCode( self, DVM_MOVE, reg3, 0, regC, colon+1, 0, end );
				DaoParser_AddCode( self, DVM_RBRA, 0, 0, 0, end, end+1, 0 );
			}
			DaoParser_AddCode( self, DVM_RBRA2, 0, 0, 0, end, end+1, 0 );
			if( regC == self->locRegCount ) DaoParser_PushRegister( self );
			return regC;
		}else if( optype == DAO_OPER_ASSERT ){
			DaoParser_AddCode( self, DVM_TRY, 0, self->vmcCount, DVM_TRY, start, 0, mid );
			DaoParser_AddCode( self, DVM_LBRA, 0, 0, 0, start, start+1, 0 );
			reg1 = DaoParser_MakeArithTree( self, start, end2, & c1, -1, state );
			if( reg1 <0 ) goto ParsingError;
			DaoParser_AddCode( self, DVM_RBRA, 0, 0, 0, start, start+1, 0 );
			DaoParser_AddCode( self, DVM_RESCUE, 0, 1, 0, mid, 0, 0 );
			DaoParser_AddCode( self, DVM_LBRA, 0, 0, 0, mid, mid+1, 0 );
			reg2 = DaoParser_MakeArithTree( self, start2, end, & c2, -1, state );
			if( reg2 <0 ) goto ParsingError;
			DaoParser_AddCode( self, DVM_MOVE, reg2, 0, reg1, start, 0, mid-1 );
			DaoParser_AddCode( self, DVM_RBRA, 0, 0, 0, mid, mid+1, 0 );
			regC = self->locRegCount;
			if( regFix >= 0 ) regC = regFix;
			if( regC == self->locRegCount ) DaoParser_PushRegister( self );
			DaoParser_AddCode( self, DVM_MOVE, reg1, 0, regC, mid+1, 0, end );
			return regC;
		}else{
			if( tokens[end2]->name == DKEY_NOT ){
				end2 -= 1;
				notin = 1;
			}
			if( optype == DAO_OPER_FIELD){
				DString *field = tokens[start]->string;
				DString_Assign( mbs, field );
				DString_AppendMBS( mbs, "=>" );
				MAP_Insert( self->allConsts, mbs, routine->routConsts->size );
				if( start != end2 || DaoToken_IsValidName( field->mbs, field->size ) ==0 ){
					DaoParser_Error( self, DAO_CTW_PAR_INVA_NAMED, NULL );/* XXX */
					goto ParsingError;
				}
				value = daoNullString;
				value.v.s = field;
				reg1 = DRoutine_AddConstValue( (DRoutine*)routine, value );
				c1 = LOOKUP_BIND_LC( reg1 );
				reg2 = DaoParser_MakeArithTree( self, start2, end, & c2, -1, state );
			}else{
				reg1 = DaoParser_MakeArithTree( self, start, end2, & c1, -1, state );

				if( optype == DAO_OPER_AND ){
					/* adding another layer of {} to avoid messing up the
					 * explicit control statements stack */
					DaoParser_AddCode( self, DVM_LBRA2, 0, 0, 0, start, start+1, 0 );
					DaoParser_AddCode( self, DVM_IF, reg1, 0, 0, start, 0, mid-1 );
					DaoParser_AddCode( self, DVM_LBRA, 0, 0, 0, mid-1, mid, 0 );
					reg2 = DaoParser_MakeArithTree( self, start2, end, & c2, -1, state );
					DaoParser_AddCode( self, DVM_RBRA, 0, 0, 0, mid, mid+1, 0 );
					DaoParser_AddCode( self, DVM_ELSE, 0, 0, 0, mid, 0,0 );
					DaoParser_AddCode( self, DVM_LBRA, 0, 0, 0, mid, mid+1, 0 );
					DaoParser_AddCode( self, DVM_RBRA, 0, 0, 0, end-1, end, 0 );
					DaoParser_AddCode( self, DVM_RBRA2, 0, 0, 0, end-1, end, 0 );
				}else if( optype == DAO_OPER_OR ){
					reg2 = self->locRegCount;
					DaoParser_PushRegister( self );
					DaoParser_AddCode( self, DVM_LBRA2, 0, 0, 0, start, start+1, 0 );
					DaoParser_AddCode( self, DVM_IF, reg1, 0, 0, start, 0, mid-1 );
					DaoParser_AddCode( self, DVM_LBRA, 0, 0, 0, mid-1, mid, 0 );
					DaoParser_AddCode( self, DVM_RBRA, 0, 0, 0, mid-1, mid, 0 );
					DaoParser_AddCode( self, DVM_ELSE, 0, 0, 0, mid, 0,0 );
					DaoParser_AddCode( self, DVM_LBRA, 0, 0, 0, mid, mid+1, 0 );
					reg2 = DaoParser_MakeArithTree( self, start2, end, & c2, reg2, state );
					DaoParser_AddCode( self, DVM_RBRA, 0, 0, 0, end-1, end, 0 );
					DaoParser_AddCode( self, DVM_RBRA2, 0, 0, 0, end-1, end, 0 );
				}else if( optype == DAO_OPER_TISA ){
					int m = 0;
					DaoType *type = DaoParser_ParseType( self, start2, end, & m, NULL );
					if( type == NULL || end+1 != m ) goto ParsingError;
					type = DaoNameSpace_GetType( myNS, (DaoBase*) type );
					c2 = LOOKUP_BIND_LC( DaoRoutine_AddConst( routine, type ) );
					reg2 = DaoParser_GetNormRegister( self, c2, start2, 0, end );
				}else{
					reg2 = DaoParser_MakeArithTree( self, start2, end, & c2, -1, state );
				}
			}
		}
		if( reg1 < 0 || reg2 < 0 ) goto ParsingError;

		/* regC could have been used by sub expressions, update it: */
		regC = self->locRegCount;
		if( regFix >= 0 ) regC = regFix;

		if( c1 && c2 && ( optype < DAO_OPER_ASSN || optype>DAO_OPER_ASSN_OR) ){
			if( optype == DAO_OPER_COLON ) code = DVM_PAIR;
			if( code >= 0 ){
				value = DaoVmProcess_MakeArithConst( myNS->vmpEvalConst, code,
						DaoParser_GetVariable( self, c1 ), DaoParser_GetVariable( self, c2 ) );
			}else{
				value = DaoVmProcess_MakeArithConst( myNS->vmpEvalConst, -code,
						DaoParser_GetVariable( self, c2 ), DaoParser_GetVariable( self, c1 ) );
			}
			if( value.t ==0 ){
				DaoParser_Error( self, DAO_CTW_INV_CONST_EXPR, NULL );
				goto ParsingError;
			}
			if( notin ){
				int v = DValue_GetInteger( value );
				value = daoZeroInt;
				value.v.i = ! v;
			}
			/* for( i=0; i<self->vmCodes->size; i++) DaoVmCodeX_Print( *self->vmCodes->items.pVmc[i], NULL ); */
			i = DaoParser_PopCodes( self, front, back );
			DaoParser_PopRegisters( self, i );
			*cst = DRoutine_AddConstValue( (DRoutine*)self->routine, value );
			*cst = LOOKUP_BIND_LC( *cst );
			return DaoParser_GetNormRegister( self, *cst, start, 0, end );
		}else if( optype ==DAO_OPER_COLON ){
			DaoParser_AddCode( self, DVM_PAIR, reg1, reg2, regC, start, mid, end );
		}else{
			short code = mapAithOpcode[optype];
			int lev = self->lexLevel;
			int st = LOOKUP_ST( regC );
			int up = LOOKUP_UP( regC );
			int adjust = (regC == reg1 || regC == reg2) && st == DAO_LOCAL_VARIABLE;
			/* Added for better temporary numarray allocation. */
			/*XXX*/
			if( adjust && up ==0 && MAP_Find( self->routine->localVarType, regC ) !=NULL ) regC ++;

			DaoVmCode_Set( & vmcValue, (ushort_t)code, reg1, reg2, regC, lev, tokPos, start, mid, end );
			if( code < 0 ){
				code = abs(code);
				DaoVmCode_Set( & vmcValue, code, reg2, reg1, regC, lev, tokPos, start, mid, end );
				DaoParser_PushBackCode( self, & vmcValue );
				regC = vmcValue.c;
				if( regC == self->locRegCount ) DaoParser_PushRegister( self );
				return regC;
			}
			if( notin ){
				vmcValue.c = self->locRegCount;
				DaoParser_PushRegister( self );
				DaoParser_PushBackCode( self, & vmcValue );
				DaoParser_AddCode( self, DVM_NOT, vmcValue.c, 0, regC, start, mid, end );
			}else{
				DaoParser_PushBackCode( self, & vmcValue );
			}
		}
	}else if( daoArithOper[ tokens[end]->name ].right ){
		i = daoArithOper[ tokens[end]->name ].oper;
		if( i == DAO_OPER_ASSERT ){
			regC = self->locRegCount;
			if( regFix >= 0 ) regC = regFix;
			if( regC == self->locRegCount ) DaoParser_PushRegister( self );
			DaoParser_AddCode( self, DVM_TRY, 0, self->vmcCount, DVM_TRY, start, mid, end );
			DaoParser_AddCode( self, DVM_LBRA, 0, 0, 0, start, start+1, 0 );
			reg1 = DaoParser_MakeArithTree( self, start, end-1, cst, -1, state );
			if( reg1 <0 ) goto ParsingError;
			DaoParser_AddCode( self, DVM_DATA, DAO_INTEGER, 1, regC, start, start+1, 0 );
			DaoParser_AddCode( self, DVM_RBRA, 0, 0, 0, start, start+1, 0 );
			DaoParser_AddCode( self, DVM_RESCUE, 0, 1, 0, end, 0, 0 );
			DaoParser_AddCode( self, DVM_LBRA, 0, 0, 0, end, end+1, 0 );
			DaoParser_AddCode( self, DVM_DATA, DAO_INTEGER, 0, regC, end, end+1, 0 );
			DaoParser_AddCode( self, DVM_RBRA, 0, 0, 0, end, end+1, 0 );
			return regC;
		}
		regC = DaoParser_MakeArithUnary( self, i, start, end-1, cst, regFix, state, 0 );
		return regC;
	}else if( daoArithOper[ tokens[start]->name ].left ){
		i = daoArithOper[ tokens[start]->name ].oper;
		regC = DaoParser_MakeArithUnary( self, i, start+1, end, cst, regFix, state, 1 );
		return regC;
	}else if( (tki == DTOK_LB && tki2 == DTOK_RB)
			|| (tki == DTOK_LCB && tki2 == DTOK_RCB)
			|| (tki == DTOK_LSB && tki2 == DTOK_RSB) ){
		/* list, map, matrix, tuple expressions: */
		/* { a=>1, b=>[] }; {1,2,10}; {1:2:10}; {1:10}; [1,2,10]; [1:2:10]; [1:10] */
		DaoType *tp = self->enumTypes->size ? self->enumTypes->items.pType[0] : 0;
		int regcount = self->locRegCount;
		int enumcode = DVM_LIST;
		int lb = start+1;
		int rb = end-1;
		int pto = DaoParser_FindOpenToken( self, DTOK_FIELD, lb, rb, 0 );
		int colon = DaoParser_FindOpenToken( self, DTOK_COLON, lb, rb, 0 );
		int semi = DaoParser_FindOpenToken( self, DTOK_SEMCO, lb, rb, 0 );
		int comma = DaoParser_FindOpenToken( self, DTOK_COMMA, lb, rb, 0 );
		int isempty = 0;
		int reg, N = 0;
		ushort_t opB = 0;
		if( tki == DTOK_LSB ) enumcode = DVM_ARRAY;
		if( typed_data_enum == DKEY_ARRAY ) enumcode = DVM_ARRAY;
		tokPos = tokens[ lb ]->line;
		cid = DArray_New(0);

#ifndef DAO_WITH_NUMARRAY
		if( enumcode == DVM_ARRAY ){
			printf( "Error: numeric array is NOT enabled!\n" );
			goto ParsingError;
		}
#endif

		if( typed_data_enum == DKEY_TUPLE || tki == DTOK_LB ){
			/* ( a, b ) */
			if( tp && tp->tid != DAO_TUPLE ) goto ParsingError;
			reg = DaoParser_MakeArithArray( self, lb, rb, & N, cst, DTOK_COMMA, 0, cid, 0 );
			if( reg < 0 ) goto ParsingError;
			regC = self->locRegCount;
			enumcode = DVM_TUPLE;
			if( regFix >= 0 ) regC = regFix;
			DaoParser_AddCode( self, DVM_TUPLE, reg, N, regC, start, mid, end );
		}else if( (typed_data_enum ==0  && (pto >= 0 || (colon >= 0 && tki == DTOK_LCB)) ) || typed_data_enum == DKEY_MAP ){
			/* { a=>1, b=>[] }; {=>}; */
			/* { a: 1, b: [] }; {:}; */
			if( tp && tp->tid != DAO_MAP ) goto ParsingError;
			enumcode = pto >= 0 ? DVM_MAP : DVM_HASH;
			isempty = lb >= rb;
			if( typed_data_enum == DKEY_MAP && colon <0 ) enumcode = DVM_MAP;
			if( lb >= rb ){
				DaoMap *hm = DaoMap_New(colon>=0);
				hm->unitype = dao_map_empty;
				GC_IncRC( dao_map_empty );
				regC = DRoutine_AddConst( (DRoutine*) self->routine, (DaoBase*) hm );
				*cst = LOOKUP_BIND_LC( regC );
				regC = DaoParser_GetNormRegister( self, *cst, start, 0, end );
			}else{
				regC = DaoParser_MakeEnumMap( self, enumcode, lb, rb, & N, cst, regFix, cid );
			}
		}else if( colon >= 0 && comma < 0 ){
			/* [1:2:10]; [1:10] */
			if( tp && (enumcode == DVM_LIST && tp->tid != DAO_LIST) ) goto ParsingError;
			if( tp && (enumcode == DVM_ARRAY && tp->tid != DAO_ARRAY) ) goto ParsingError;
			reg = DaoParser_MakeArithArray( self, lb, rb, & N, cst, DTOK_COLON, 0, cid, 0 );
			isempty = lb > rb;
			if( reg < 0 || N < 2 || N > 3 ){
				DaoParser_Error( self, DAO_CTW_ENUM_INVALID, NULL );
				goto ParsingError;
			}
			regC = self->locRegCount;
			if( regFix >= 0 ) regC = regFix;
			DaoParser_AddCode( self, enumcode, reg, N, regC, start, mid, end );

		}else if( semi < 0 ){
			/* [a,b,c] */
			if( tp && (enumcode == DVM_LIST && tp->tid != DAO_LIST) ) goto ParsingError;
			if( tp && (enumcode == DVM_ARRAY && tp->tid != DAO_ARRAY) ) goto ParsingError;
			reg = DaoParser_MakeArithArray( self, lb, rb, & N, cst, DTOK_COMMA, 0, cid, 0 );
			if( reg < 0 ) goto ParsingError;
			isempty = lb > rb;
			regC = self->locRegCount;
			if( regFix >= 0 ) regC = regFix;
			DaoParser_AddCode( self, enumcode, reg, N+10, regC, start, mid, end );
		}else if( typed_data_enum == 0 || typed_data_enum == DKEY_ARRAY ){
			/* [1,2; 3,4] */
			isempty = lb > rb;
			enumcode = DVM_MATRIX;
			if( tp && (enumcode == DVM_ARRAY && tp->tid != DAO_ARRAY) ) goto ParsingError;
			regC = DaoParser_MakeEnumMatrix( self, lb, rb, & N, cst, regFix, cid );
		}else{
			regC = -1;
		}
		if( regC < 0 ){
			DaoParser_Error( self, DAO_CTW_ENUM_INVALID, NULL );
			goto ParsingError;
		}
		if( colon >= 0 && tki != DTOK_LCB ) *cst = 0;
		if( self->enumTypes->size ==0 && isempty ){
			tp = NULL;
			switch( enumcode ){
			case DVM_LIST : tp = dao_list_empty; break;
			case DVM_ARRAY : case DVM_MATRIX : tp = dao_array_empty; break;
			case DVM_MAP : case DVM_HASH : tp = dao_map_empty; break;
			}
			if( tp ) DArray_PushFront( self->enumTypes, tp );
		}
		if( *cst == 1 ){
			regC = DaoParser_MakeConst( self, front, back, cid, regcount, N, start, mid, end, cst );
		}else if( self->enumTypes->size ){
			tp = self->enumTypes->items.pType[0];
			if( tp && tp->tid != DAO_ANY ) MAP_Insert( self->routine->localVarType, regC, tp );
		}
	}else{
		DArray_PushFront( self->enumTypes, NULL );
		regC = DaoParser_MakeChain( self, start, end, cst, regFix );
		DArray_PopFront( self->enumTypes );
		if( regC <0 ) DaoParser_Error2( self, DAO_INVALID_EXPRESSION, start, end, 0 );
		return regC;
	}
	/* If _regC_ is the one after the last register, increase register number: */
	/* !!! It is NOT equivalent to if( regFix < 0 ). */
	if( regC == self->locRegCount ) DaoParser_PushRegister( self );
	if( cid ) DArray_Delete( cid );
	return regC;

ParsingError:
	printf( "pasing error:\n" );
	if( typed_data_enum ) printf( "%s  ", tokens[start-1]->string->mbs );
	for( i=start;i<=end;i++) printf("%s  ", tokens[i]->string->mbs); printf("\n");
	if( cid ) DArray_Delete( cid );
	return -1;
}
int DaoParser_MakeArithTree( DaoParser *self, int start, int end,
		int *cst, int regFix, int state )
{
#if 0
	int i;
	DaoToken **tokens = self->tokens->items.pToken;
	printf("MakeArithTree(): start = %i; end = %i;\n", start, end );
	for( i=start;i<=end;i++) printf("%s  ", tokens[i]->string->mbs); printf("\n");
#endif
	int reg = DaoParser_MakeArithTree2( self, start, end, cst, regFix, state );
	if( reg <0 && state ==0 ) DaoParser_Error2( self, DAO_INVALID_EXPRESSION, start, end, 0 );
	return reg;
}
