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
#include<assert.h>

#include"daoConst.h"
#include"daoVmcode.h"
#include"daoParser.h"
#include"daoRegex.h"
#include"daoNumtype.h"
#include"daoRoutine.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoVmspace.h"
#include"daoNamespace.h"
#include"daoStream.h"
#include"daoStdlib.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoBase.h"
#include"daoValue.h"



/* Expression node */
typedef struct DaoEnode DaoEnode;
struct DaoEnode
{
	int reg;    /* vm register id, for the value produced by the expression */
	int konst;  /* constant id, for the value of a constant expression */
	int count;  /* number of expressions in an expression list */
	DaoInode *prev;   /* the previous instruction, should never be NULL */
	DaoInode *first;  /* the first instruction node for the expression */
	DaoInode *last;   /* the last instruction node for the expression */
	DaoInode *update; /* the last instruction that updates the value of "reg" */
};


DAO_DLL void DaoParser_Warn2( DaoParser *self, int code, int start, int end );
DAO_DLL void DaoParser_Warn( DaoParser *self, int code, DString *ext );
DAO_DLL void DaoParser_Error( DaoParser *self, int code, DString *ext );
DAO_DLL void DaoParser_Error2( DaoParser *self, int code, int m, int n, int single_line );
DAO_DLL void DaoParser_Error3( DaoParser *self, int code, int m );
DAO_DLL void DaoParser_MakeCodes( DaoParser *self, int start, int end, DString *output );
DAO_DLL int DaoParser_ParseLoadStatement( DaoParser *self, int start, int end );
DAO_DLL int DaoParser_FindOpenToken( DaoParser *self, uchar_t tok, int start, int end, int warn );
DAO_DLL int DaoParser_FindPairToken( DaoParser *self,  uchar_t lw, uchar_t rw, int start, int stop );


int DaoParser_GetOperPrecedence( DaoParser *self );
int DaoParser_CurrentTokenType( DaoParser *self );
int DaoParser_CurrentTokenName( DaoParser *self );
int DaoParser_NextTokenType( DaoParser *self );
int DaoParser_NextTokenName( DaoParser *self );
static int DaoParser_CheckTokenType( DaoParser *self, int tok, const char *str );
static DaoEnode DaoParser_ParsePrimary( DaoParser *self, int stop, int eltype );
static DaoEnode DaoParser_ParseExpression( DaoParser *self, int stop );
static DaoEnode DaoParser_ParseExpression2( DaoParser *self, int stop, int eltype, int warn );
static DaoEnode DaoParser_ParseExpressionList( DaoParser *self, int, DaoInode*, DArray* );
static DaoEnode DaoParser_ParseExpressionList2( DaoParser *self, int, DaoInode*, DArray*, int);
static DaoEnode DaoParser_ParseExpressionLists( DaoParser *self, int, int, int*, DArray* );
static DaoEnode DaoParser_ParseEnumeration( DaoParser *self, int etype, int btype, int lb, int rb );

DaoProcess* DaoNamespace_ReserveFoldingOperands( DaoNamespace *self, int N );
static int DaoParser_MakeEnumConst( DaoParser *self, DaoEnode *enode, DArray *cid, int regcount );
static int DaoParser_MakeArithConst( DaoParser *self, ushort_t, DaoValue*, DaoValue*, int*, DaoInode*, int );
static int DaoParser_ExpClosure( DaoParser *self, int start );
static DaoValue* DaoParser_EvalConst( DaoParser *self, DaoProcess *proc, int nvalues );


typedef struct DStringIntPair
{
	const char *key;
	int   value;
}DStringIntPair;

extern DOper daoArithOper[DAO_NOKEY2];
extern DIntStringPair dao_keywords[];

static const int mapAithOpcode[]=
{
	200, 200, 200, /* padding for regex opers */

	DVM_MOVE , /* DAO_OPER_ASSN */
	DVM_ADD , /* DAO_OPER_ASSN_ADD */
	DVM_SUB , /* DAO_OPER_ASSN_SUB */
	DVM_MUL , /* DAO_OPER_ASSN_DIV */
	DVM_DIV , /* DAO_OPER_ASSN_MUL */
	DVM_MOD , /* DAO_OPER_ASSN_MOD */
	DVM_BITAND , /* DAO_OPER_ASSN_AND */
	DVM_BITOR  , /* DAO_OPER_ASSN_OR */
	DVM_BITXOR , /* DAO_OPER_ASSN_XOR */

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
	-DVM_IN , /* DAO_OPER_NOTIN */

	DVM_LT  , /* DAO_OPER_LT */
	-DVM_LT , /* DAO_OPER_GT */
	DVM_EQ  , /* DAO_OPER_EQ */
	DVM_NE  , /* DAO_OPER_NE */
	DVM_LE  , /* DAO_OPER_LE */
	-DVM_LE , /* DAO_OPER_GE */
	DVM_SAME ,
	DVM_ISA ,
	200 , /* padding for assertion operator */

	DVM_ADD , /* DAO_OPER_ADD */
	DVM_SUB , /* DAO_OPER_SUB */
	DVM_DIV , /* DAO_OPER_DIV */
	DVM_MUL , /* DAO_OPER_MUL */
	DVM_MOD , /* DAO_OPER_MOD */
	DVM_POW , /* ** */

	DVM_NOT /* DAO_OPER_NOT */
};

void DaoInode_Print( DaoInode *self, int index );
void DaoInode_Delete( DaoInode *self );

DaoParser* DaoParser_New()
{
	DaoParser *self = (DaoParser*) dao_calloc( 1, sizeof(DaoParser) );

	self->levelBase = 0;
	self->lexLevel = 0;

	self->fileName = DString_New(1);
	self->lexer = DaoLexer_New();
	self->tokens = self->lexer->tokens;

	self->vmCodes = DArray_New(D_VMCODE);
	self->vmcBase = DaoInode_New();
	self->vmcFirst = self->vmcLast = self->vmcBase;
	self->vmcBase->code = DVM_UNUSED;
	self->vmcFree = NULL;

	self->allConsts = DHash_New(D_STRING,0);

	self->initTypes = DMap_New(D_STRING,D_VALUE);

	self->scopeOpenings = DArray_New(0);
	self->scopeClosings = DArray_New(0);
	self->decoFuncs   = DArray_New(0);
	self->decoFuncs2  = DArray_New(0);
	self->decoParams  = DArray_New(D_VALUE);
	self->decoParams2 = DArray_New(D_VALUE);

	self->elexer = DaoLexer_New();
	self->wlexer = DaoLexer_New();
	self->errors = self->elexer->tokens;
	self->warnings = self->wlexer->tokens;

	self->noneValue = -1;
	self->integerZero = -1;
	self->integerOne = -1;
	self->imaginaryOne = -1;

	self->routName = DString_New(1);
	self->mbs = DString_New(1);
	self->mbs2 = DString_New(1);
	self->str = DString_New(1);
	self->denum = DaoEnum_New(NULL,0);

	self->toks = DArray_New(0);
	self->lvm = DMap_New(D_STRING,0);
	self->arrays = DArray_New(D_ARRAY);
	self->strings = DArray_New(D_STRING);
	self->localDataMaps = DArray_New(D_MAP);
	self->switchMaps = DArray_New(D_MAP);
	self->enumTypes = DArray_New(0);
	self->routCompilable = DArray_New(0);
	self->routReInferable = DArray_New(0);
	DArray_Append( self->localDataMaps, self->lvm );
	DArray_Append( self->strings, self->mbs );
	DArray_Append( self->arrays, self->toks );

	self->typeItems = DArray_New(0);
	self->usedString = 0;
	self->usedArray = 0;

	self->indent = 1;
	return self;
}
void DaoParser_ClearCodes( DaoParser *self );
void DaoParser_Delete( DaoParser *self )
{
	DaoInode *node;

	DaoEnum_Delete( self->denum );
	DString_Delete( self->fileName );
	DString_Delete( self->routName );
	DString_Delete( self->mbs );
	DString_Delete( self->mbs2 );
	DString_Delete( self->str );
	DArray_Delete( self->decoFuncs );
	DArray_Delete( self->decoFuncs2 );
	DArray_Delete( self->decoParams );
	DArray_Delete( self->decoParams2 );
	DArray_Delete( self->toks );
	DArray_Delete( self->localDataMaps );
	DArray_Delete( self->switchMaps );
	DArray_Delete( self->enumTypes );
	DArray_Delete( self->scopeOpenings );
	DArray_Delete( self->scopeClosings );
	DArray_Delete( self->vmCodes );
	DArray_Delete( self->strings );
	DArray_Delete( self->arrays );
	DArray_Delete( self->routCompilable );
	DArray_Delete( self->routReInferable );
	DArray_Delete( self->typeItems );

	DaoLexer_Delete( self->lexer );
	DaoLexer_Delete( self->elexer );
	DaoLexer_Delete( self->wlexer );
	if( self->argName ) DaoToken_Delete( self->argName );
	if( self->decoArgName ) DaoToken_Delete( self->decoArgName );
	if( self->uplocs ) DArray_Delete( self->uplocs );
	if( self->outers ) DArray_Delete( self->outers );
	if( self->allConsts ) DMap_Delete( self->allConsts );
	if( self->protoValues ) DMap_Delete( self->protoValues );
	DMap_Delete( self->initTypes );
	DMap_Delete( self->lvm );
	DaoParser_ClearCodes( self );
	node = self->vmcFree;
	while( node ){
		DaoInode *node2 = node;
		node = node->next;
		DaoInode_Delete( node2 );
	}
	DaoInode_Delete( self->vmcBase );
	dao_free( self );
}
void DaoParser_Reset( DaoParser *self )
{
	int i;
	for(i=0; i<=self->lexLevel; ++i) DMap_Reset( self->localDataMaps->items.pMap[i] );
	self->autoReturn = 0;
	self->topAsGlobal = 0;
	self->isClassBody = 0;
	self->isInterBody = 0;
	self->permission = 0;
	self->isFunctional = 0;
	self->needConst = 0;
	self->usingGlobal = 0;
	self->numSections = 0;

	self->curToken = 0;
	self->regCount = 0;
	self->levelBase = 0;
	self->lexLevel = 0;
	self->usedString = 0;
	self->usedArray = 0;

	self->curLine = 0;
	self->lineCount = 0;
	self->indent = 1;
	self->defined = 0;
	self->parsed = 0;

	self->noneValue = -1;
	self->integerZero = -1;
	self->integerOne = -1;
	self->imaginaryOne = -1;

	DString_Reset( self->routName, 0 );
	DString_Reset( self->mbs, 0 );
	DString_Reset( self->mbs2, 0 );
	DString_Reset( self->str, 0 );

	self->typeItems->size = 0;
	self->decoFuncs->size = 0;
	self->decoFuncs2->size = 0;
	self->toks->size = 0;

	self->scopeOpenings->size = 0;
	self->scopeClosings->size = 0;

	self->nameSpace = NULL;
	self->defParser = NULL;
	self->routine = NULL;
	self->hostInter = NULL;
	self->hostClass = NULL;
	self->hostCdata = NULL;
	self->hostType = NULL;
	self->outerParser = NULL;
	self->innerParser = NULL;
	self->byteCoder = NULL;
	self->byteBlock = NULL;
	self->returnType = NULL;
	self->vmcValue = NULL;

	DArray_Clear( self->decoParams );
	DArray_Clear( self->decoParams2 );
	DArray_Clear( self->switchMaps );
	DArray_Clear( self->enumTypes );
	DArray_Clear( self->vmCodes );
	DArray_Clear( self->routCompilable );
	DArray_Clear( self->routReInferable );

	DaoLexer_Reset( self->lexer );
	DaoLexer_Reset( self->elexer );
	DaoLexer_Reset( self->wlexer );

	if( self->uplocs ) DArray_Clear( self->uplocs );
	if( self->outers ) DArray_Clear( self->outers );
	if( self->allConsts ) DMap_Reset( self->allConsts );
	if( self->protoValues ) DMap_Reset( self->protoValues );
	if( self->argName ) DaoToken_Delete( self->argName );
	if( self->decoArgName ) DaoToken_Delete( self->decoArgName );
	self->argName = NULL;
	self->decoArgName = NULL;

	DMap_Clear( self->initTypes );
	DMap_Reset( self->lvm );
	DaoParser_ClearCodes( self );
}

static DString* DaoParser_GetString( DaoParser *self )
{
	if( self->usedString >= self->strings->size )
		DArray_Append( self->strings, self->strings->items.pString[0] );
	self->usedString += 1;
	self->strings->items.pString[ self->usedString - 1 ]->size = 0;
	return self->strings->items.pString[ self->usedString - 1 ];
}
static DArray* DaoParser_GetArray( DaoParser *self )
{
	if( self->usedArray >= self->arrays->size )
		DArray_Append( self->arrays, self->arrays->items.pArray[0] );
	self->usedArray += 1;
	self->arrays->items.pArray[ self->usedArray - 1 ]->size = 0;
	return self->arrays->items.pArray[ self->usedArray - 1 ];
}
static void DaoParser_PushLevel( DaoParser *self )
{
	self->lexLevel ++;
	if( self->lexLevel >= self->localDataMaps->size ){
		DArray_Append( self->localDataMaps, self->lvm );
	}
}
static void DaoParser_PopLevel( DaoParser *self )
{
	DMap_Reset( self->localDataMaps->items.pMap[ self->lexLevel ] );
	self->lexLevel --;
}
DMap* DaoParser_GetCurrentDataMap( DaoParser *self )
{
	return self->localDataMaps->items.pMap[ self->lexLevel ];
}
static int DaoParser_PushOuterRegOffset( DaoParser *self, int start, int end )
{
	if( self->outers == NULL ) self->outers = DArray_New(0);
	if( self->outers->size >= DAO_MAX_SECTDEPTH ){
		DaoParser_Error2( self, DAO_SECTION_TOO_DEEP, start, end, 0 );
		return 0;
	}
	DArray_PushBack( self->outers, (void*)(daoint)self->regCount );
	return 1;
}
static int DaoParser_GetOuterLevel( DaoParser *self, int reg )
{
	int i = 0;
	if( self->outers == NULL ) return 0;
	while( i < self->outers->size && reg >= self->outers->items.pInt[i] ) i += 1;
	if( i >= self->outers->size ) return 0;
	return self->outers->size - i;
}

static void DaoParser_PrintCodes( DaoParser *self )
{
	DaoInode *it = self->vmcFirst;
	int i = 0;
	while( it ){
		DaoInode_Print( it, i++ );
		it = it->next;
	}
}

void DaoParser_CacheNode( DaoParser *self, DaoInode *node )
{
	node->prev = NULL;
	node->extra = NULL;
	node->next = self->vmcFree;
	self->vmcFree = node;
}
DaoInode* DaoParser_NewNode( DaoParser *self )
{
	if( self->vmcFree ){
		DaoInode *node = self->vmcFree;
		self->vmcFree = self->vmcFree->next;
		node->next = NULL;
		return node;
	}
	return DaoInode_New();
}
void DaoParser_ClearCodes( DaoParser *self )
{
	DaoInode *it = self->vmcFirst;
	while( it != self->vmcBase ){
		it = it->next;
		DaoParser_CacheNode( self, it->prev );
	}
	it = self->vmcLast;
	while( it != self->vmcBase ){
		it = it->prev;
		DaoParser_CacheNode( self, it->next );
	}
	self->vmcBase->prev = self->vmcBase->next = NULL;
	self->vmcFirst = self->vmcLast = self->vmcBase;
	self->vmcCount = 0;
}
static void DaoParser_PopBackCode( DaoParser *self )
{
	if( self->vmcLast == NULL || self->vmcLast == self->vmcBase ) return;
	self->vmcLast = self->vmcLast->prev;
	DaoParser_CacheNode( self, self->vmcLast->next );
	self->vmcLast->next = NULL;
	self->vmcCount --;
}
static int DaoParser_PopCodes( DaoParser *self, DaoInode *back )
{
	int count = 0;
	DaoInode *node = NULL;
	while( (node=self->vmcLast) != back ) DaoParser_PopBackCode( self ), count ++;
	self->vmcValue = NULL;
	return count;
}
/* In constant folding, do not actually remove the codes, which may invalidate
 * some references in DaoEnode structures: */
static int DaoParser_PopCodes2( DaoParser *self, DaoInode *back )
{
	int count = 0;
	DaoInode *node = self->vmcLast;
	while( node != back ){
		node->code = DVM_UNUSED;
		node = node->prev;
		count ++;
	}
	self->vmcValue = NULL;
	return count;
}
static void DaoParser_AppendCode( DaoParser *self, DaoInode *inode )
{
	if( inode == self->vmcLast ) return;
	if( inode->next ){
		inode->prev->next = inode->next;
		inode->next->prev = inode->prev;
	}
	inode->prev = self->vmcLast;
	self->vmcLast->next = inode;
	self->vmcLast = inode;
	inode->next = NULL;
}
static DaoInode* DaoParser_AddCode2( DaoParser *self, ushort_t code,
		ushort_t a, ushort_t b, ushort_t c, int first, int mid, int last );
static DaoInode* DaoParser_InsertCode( DaoParser *self, DaoInode *after, int code, int a, int b, int c, int first );
static DaoInode* DaoParser_PushBackCode( DaoParser *self, DaoVmCodeX *vmc )
{
	DaoInode *node = DaoParser_NewNode( self );
	memcpy( node, vmc, sizeof(DaoVmCode) );
	node->level = vmc->level;
	node->line = vmc->line;
	node->first = vmc->first;
	node->middle = vmc->middle;
	node->last = vmc->last;

	self->vmcLast->next = node;
	node->prev = self->vmcLast;
	self->vmcLast = node;
	self->vmcCount ++;
	return self->vmcLast;
}

void DaoParser_Warn( DaoParser *self, int code, DString *ext )
{
	if( ext && ext->size > 100 ) DString_Erase( ext, 100, -1 );
	DaoLexer_Append( self->wlexer, code, self->curLine, ext ? ext->mbs : "" );
}
void DaoParser_Error( DaoParser *self, int code, DString *ext )
{
	if( code != DAO_EVAL_EXCEPTION &&  ext && ext->size > 100 ) DString_Erase( ext, 100, -1 );
	DaoLexer_Append( self->elexer, code, self->curLine, ext ? ext->mbs : "" );
}
void DaoParser_SumTokens( DaoParser *self, DString *sum, int m, int n, int single_line )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoToken *tok, *tok0 = NULL;
	int i, line = self->curLine;
	DString_Clear( sum );
	if( m < 0 ) m = 0;
	if( n >= self->tokens->size ) n = self->tokens->size - 1;
	if( m < n ) line = tokens[m]->line;
	for(i=m; i<=n; i++){
		tok = tokens[i];
		if( single_line && (int)tok->line > line ) break;
		if( tok0 && (tok->line != tok0->line || tok->cpos > (tok0->cpos + tok0->string.size)) )
			DString_AppendChar( sum, ' ' );
		tok0 = tok;
		DString_Append( sum, & tok->string );
		if( i<n && sum->size > 30 ){
			DString_AppendMBS( sum, " ..." );
			break;
		}
	}
}
void DaoParser_Warn2( DaoParser *self, int code, int start, int end )
{
	DaoParser_SumTokens( self, self->mbs, start, end, 0 );
	DaoParser_Warn( self, code, self->mbs );
}
/* tokens from m to n as message */
void DaoParser_Error2( DaoParser *self, int code, int m, int n, int single_line )
{
	DString *mbs = DaoParser_GetString( self );
	DaoParser_SumTokens( self, mbs, m, n, single_line );
	DaoLexer_Append( self->elexer, code, self->curLine, mbs->mbs );
}
/* tokens from m until the end of the line as message */
void DaoParser_Error3( DaoParser *self, int code, int m )
{
	DString *mbs = DaoParser_GetString( self );
	DaoParser_SumTokens( self, mbs, m, self->tokens->size-1, 1 );
	DaoLexer_Append( self->elexer, code, self->curLine, mbs->mbs );
}
void DaoParser_Error4( DaoParser *self, int code, int line, const char *msg )
{
	DaoLexer_Append( self->elexer, code, line, msg );
}
void DaoParser_ErrorToken( DaoParser *self, int code, DaoToken *token )
{
	DaoParser_Error4( self, code, token->line, token->string.mbs );
}
void DaoParser_Suggest( DaoParser *self, const char *suggestion )
{
	DaoStream_WriteMBS( self->vmSpace->errorStream, "suggestion:\n" );
	DaoStream_WriteMBS( self->vmSpace->errorStream, suggestion );
	DaoStream_WriteChar( self->vmSpace->errorStream, '\n' );
}
void DaoParser_PrintInformation( DaoParser *self, DArray *infolist, const char *header )
{
	int i;
	DaoStream *stream = self->vmSpace->errorStream;

	if( infolist->size ==0 ) return;
	DaoStream_WriteMBS( stream, header );
	DaoStream_WriteMBS( stream, " in file \"" );
	if( self->fileName->size )
		DaoStream_WriteString( stream, self->fileName );
	else
		DaoStream_WriteString( stream, self->nameSpace->name );
	DaoStream_WriteMBS( stream, "\":\n" );

	for(i=infolist->size-1; i>=0; i--){
		DaoToken *tok = infolist->items.pToken[i];
		if( i < infolist->size-1 ){
			DaoToken *tok2 = infolist->items.pToken[i+1];
			if( tok->line == tok2->line && tok->name == tok2->name ){
				if( DString_EQ( & tok->string, & tok2->string ) ) continue;
			}
		}
		if( tok->name == 0 ){
			DaoStream_WriteMBS( stream, "  From file : " );
		}else{
			DaoStream_WriteMBS( stream, "  At line " );
			DaoStream_WriteInt( stream, tok->line );
			DaoStream_WriteMBS( stream, " : " );
			DaoStream_WriteMBS( stream, getCtInfo( tok->name ) );
			if( tok->string.size ) DaoStream_WriteMBS( stream, " --- " );
		}
		if( tok->name == DAO_EVAL_EXCEPTION ){
			DaoStream_WriteMBS( stream, "\n" );
			DaoStream_WriteString( stream, & tok->string );
			DaoStream_WriteMBS( stream, "\n" );
			continue;
		}
		if( tok->string.size ){
			DaoStream_WriteMBS( stream, "\" " );
			DaoStream_WriteString( stream, & tok->string );
			DaoStream_WriteMBS( stream, " \"" );
		}
		DaoStream_WriteMBS( stream, ";\n" );
	}
	DArray_Clear( infolist );
}
static void DaoParser_PrintWarnings( DaoParser *self )
{
	DaoParser_PrintInformation( self, self->warnings, "[[WARNING]]" );
}
void DaoParser_PrintError( DaoParser *self, int line, int code, DString *ext )
{
	DaoParser_PrintWarnings( self );
	if( code ) DaoParser_Error4( self, code, line, ext ? ext->mbs : "" );
	DaoParser_PrintInformation( self, self->errors, "[[ERROR]]" );
}
static void DaoParser_StatementError( DaoParser *self, DaoParser *parser, int code )
{
	DaoInode *prev = NULL, *inode = parser->vmcLast;
	while( inode != parser->vmcBase ){
		int end = inode->first + inode->last;
		if( prev == NULL || inode->line != prev->line ){
			DString *mbs = DaoParser_GetString( self );
			DaoParser_SumTokens( parser, mbs, inode->first, end, 0 );
			DaoParser_Error4( self, code, inode->line, mbs->mbs );
		}
		prev = inode;
		inode = inode->prev;
	}
}

#define MAX_INDENT_STEP 16
int DaoParser_LexCode( DaoParser *self, const char *src, int replace )
{
	DVector *indstack = NULL, *indents = NULL, *lines = NULL;
	DString *mbs = self->mbs;
	DaoToken *t, *t2;
	int counts[MAX_INDENT_STEP];
	int i, j, k, flags = replace ? DAO_LEX_ESCAPE : 0;

	flags |= DAO_LEX_COMMENT;
	self->lineCount = DaoLexer_Tokenize( self->lexer, src, flags );
	if( self->lineCount ==0 ) return 0;
	if( daoConfig.chindent ){
		indents = DVector_New( sizeof(int) );
		lines = DVector_New( sizeof(int) );
		indstack = DVector_New( sizeof(int) );
		DVector_PushInt( indstack, 1 );
		DVector_PushInt( indstack, 1 );
	}
	memset( counts, 0, MAX_INDENT_STEP*sizeof(int) );
	for(i=0; i<self->tokens->size; i++ ){
		t = self->tokens->items.pToken[i];
		if( daoConfig.chindent && t->name != DTOK_COMMENT ){
			if( i ==0 || t->line != self->tokens->items.pToken[i-1]->line ){
				while( indstack->size && t->cpos < indstack->data.ints[indstack->size-1] ){
					indstack->size -= 1;
				}
				if( indstack->size && t->cpos > indstack->data.ints[indstack->size-1] ){
					DVector_PushInt( indstack, t->cpos );
				}
				k = t->cpos - indstack->data.ints[ indstack->size - 2 ];
				if( k >= 16 ) k = 16;
				if( k >0 ) counts[k-1] += 1;
				if( k >0 ){
					DVector_PushInt( indents, k );
					DVector_PushInt( lines, t->line );
				}
			}
		}
		if( t->name == DTOK_COMMENT ){
			DArray_Erase( self->tokens, i, 1 );
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
			if( indents->data.ints[i] % self->indent ){
				printf( "Warning: improper indentation of %i spaces at line: %i.\n",
						indents->data.ints[i], lines->data.ints[i] );
			}
		}
		DVector_Delete( indstack );
		DVector_Delete( indents );
		DVector_Delete( lines );
	}
#if 0
	for(i=0; i<self->tokens->size; i++){
		DaoToken *tk = self->tokens->items.pToken[i];
		printf( "%4i: %4i %4i, %4i,  %s\n", i, tk->type, tk->name, tk->cpos, tk->string.mbs );
	}
#endif
	return 1;
}

static int DaoParser_CheckTokenType( DaoParser *self, int tok, const char *str )
{
	daoint cur = self->curToken;
	DaoToken **tokens = self->tokens->items.pToken;
	if( cur < self->tokens->size && tokens[cur]->type == tok ) return 1;
	DaoParser_Error4( self, DAO_TOKEN_EXPECTING, tokens[cur]->line, str );
	return 0;
}
int DaoParser_CurrentTokenType( DaoParser *self )
{
	if( self->curToken >= self->tokens->size ) return 0;
	return self->tokens->items.pToken[self->curToken]->type;
}
int DaoParser_CurrentTokenName( DaoParser *self )
{
	if( self->curToken >= self->tokens->size ) return 0;
	return self->tokens->items.pToken[self->curToken]->name;
}
int DaoParser_NextTokenType( DaoParser *self )
{
	if( (self->curToken+1) >= self->tokens->size ) return 0;
	return self->tokens->items.pToken[self->curToken+1]->type;
}
int DaoParser_NextTokenName( DaoParser *self )
{
	if( (self->curToken+1) >= self->tokens->size ) return 0;
	return self->tokens->items.pToken[self->curToken+1]->name;
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

static int DaoParser_PushRegister( DaoParser *self );
static DaoInode* DaoParser_AddCode( DaoParser *self, ushort_t code,
		ushort_t a, ushort_t b, ushort_t c, int first, int mid, int last );

static int DaoClass_BaseCstrOffset( DaoClass *self, DaoClass *base, int idx )
{
	daoint j, offset = 0;
	if( idx < self->mixinBases->size ){
		for(j=0; j<self->mixins->size; ++j){
			if( self->mixins->items.pClass[j] == base ){
				offset = self->ranges->data.ushorts[6*j] + DAO_CLASS_CONST_CSTOR;
				break;
			}
		}
	}else{
		offset = self->offsets->data.ushorts[2*(idx - self->mixinBases->size)];
		offset += (base->type == DAO_CLASS ? DAO_CLASS_CONST_CSTOR : 0);
	}
	return offset;
}
static int DaoParser_AddDefaultInitializer( DaoParser *self, DaoClass *klass, int flags )
{
	daoint i, j;
	for(i=0; i<klass->allBases->size; i++){
		DaoClass *base = (DaoClass*) klass->allBases->items.pValue[i];
		DaoCdata *cdata = (DaoCdata*) klass->allBases->items.pValue[i];
		DaoInode *inode;
		int reg, opb = 0;
		if( flags & (1<<i) ) continue;
		inode = DaoParser_AddCode( self, DVM_GETCK, 1, 0, 0, 0, 0, 0 );
		inode->b = DaoClass_BaseCstrOffset( klass, base, i );
		inode->c = DaoParser_PushRegister( self );
		reg = DaoParser_PushRegister( self );
		DaoParser_AddCode( self, DVM_CALL, inode->c, DAO_CALL_INIT, reg, 0,0,0 );
	}
	return 1;
}

static DaoValue* DaoParser_GetVariable( DaoParser *self, int reg );
static int DaoParser_GetRegister( DaoParser *self, DaoToken *name );
int DaoParser_FindScopedConstant2( DaoParser *self, DaoValue **value, int start, int stop, int type )
{
	DaoEnode enode;
	self->curToken = start;
	enode = DaoParser_ParsePrimary( self, stop, type );
	if( enode.reg < 0 || enode.konst == 0 ){
		DaoParser_Error3( self, DAO_EXPR_NEED_CONST_EXPR, start );
		return -1;
	}
	*value = DaoParser_GetVariable( self, enode.konst );
	return self->curToken - 1;
}
int DaoParser_FindScopedConstant( DaoParser *self, DaoValue **value, int start )
{
	return DaoParser_FindScopedConstant2( self, value, start, DTOK_LCB, 0 );
}
int DaoParser_ParseScopedName( DaoParser *self, DaoValue **value, int start )
{
	DaoToken **tokens = self->tokens->items.pToken;
	if( (start+1) >= self->tokens->size || tokens[start+1]->type != DTOK_COLON2 ){
		int reg = DaoParser_GetRegister( self, tokens[start] );
		if( reg >= 0 ){
			*value = DaoParser_GetVariable( self, reg );
			return start;
		}
		return start;
	}
	return DaoParser_FindScopedConstant2( self, value, start, DTOK_LCB, DAO_EXPRLIST_SCOPE );
}

static int DaoParser_ParseInitSuper( DaoParser *self, DaoParser *module, int start )
{
	DaoLexer *init = NULL;
	DaoRoutine *routine = module->routine;
	DaoClass *klass = module->hostClass;
	DaoToken **tokens = self->tokens->items.pToken;
	daoint i, size = self->tokens->size;
	int isconstru = klass && DString_EQ( routine->routName, klass->className );
	int line = 0, flags = 0; /* XXX number of super classes */
	int dlm = start;
	int rb = 0;
	if( isconstru == 0 ) return start;
	init = DaoLexer_New();
	if( tokens[start]->name == DTOK_COLON ){
		do {
			DaoEnode enode;
			DaoInode *inode;
			DaoValue *value = NULL;
			DaoLexer *lexer = module->lexer;
			DaoInode *back = self->vmcLast;
			int pos = DaoParser_FindScopedConstant2( self, & value, dlm+1, DTOK_LB, 0 );
			int reg, offset, count, found = -1;

			DaoParser_PopCodes( self, back );
			if( value == NULL ) goto ErrorRoutine;
			if( value->type != DAO_CLASS && value->type != DAO_CTYPE ) goto ErrorRoutine;
			if( pos < 0 || tokens[pos+1]->type != DTOK_LB ) goto ErrorRoutine;

			for(i=0; i<klass->allBases->size; ++i){
				if( value == klass->allBases->items.pValue[i] ){
					found = i;
					break;
				}
			}
			if( found < 0 ) goto ErrorRoutine;
			flags |= 1<<found;

			rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, dlm, -1 );
			if( rb < 0 ) goto ErrorRoutine;

			offset = klass->offsets->data.ushorts[2*(found - klass->mixinBases->size)];
			inode = DaoParser_AddCode( module, DVM_GETCK, 1, 0, 0, 0, 0, 0 );
			inode->b = DaoClass_BaseCstrOffset( klass, (DaoClass*) value, found );
			inode->c = DaoParser_PushRegister( module );
			inode->first = inode->middle = start + 1;
			inode->last = pos - 1;

			if( rb > (pos + 2) ){
				module->lexer = self->lexer;
				module->tokens = self->lexer->tokens;
				module->curToken = pos + 2;
				enode = DaoParser_ParseExpressionList( module, DTOK_COMMA, inode, NULL );
				module->lexer = lexer;
				module->tokens = lexer->tokens;
				if( enode.reg < 0 ) goto ErrorRoutine;
			}else{
				enode.reg = inode->c;
				enode.count = 1;
			}

			reg = DaoParser_PushRegister( module );
			inode = DaoParser_AddCode( module, DVM_CALL, enode.reg, 0, reg, dlm+1,pos,rb );
			inode->b = (enode.count-1)|DAO_CALL_INIT;
			dlm = rb + 1;
		} while( dlm < size && tokens[dlm]->name == DTOK_COMMA );
		start = dlm;
		if( tokens[start]->name != DTOK_LCB ) goto ErrorRoutine;
	}
	if( tokens[start]->name == DTOK_LCB ){
		DaoParser_AddDefaultInitializer( module, klass, flags );
	}
	if( init ) DaoLexer_Delete( init );
	return start;
ErrorRoutine:
	if( init ) DaoLexer_Delete( init );
	return -1;
}

static int DaoParser_PushRegister( DaoParser *self )
{
	int line, reg = self->regCount;
	self->regCount += 1;
	if( self->routine == NULL || self->routine->body == NULL ) return reg;
	line = self->curLine - self->routine->body->codeStart - 1;
	return reg;
}
static int DaoParser_PushRegisters( DaoParser *self, int n )
{
	int i, line, reg = self->regCount;
	self->regCount += n;
	if( self->routine == NULL || self->routine->type != DAO_ROUTINE ) return reg;
	line = self->curLine - self->routine->body->codeStart - 1;
	return reg;
}
static void DaoParser_PopRegister( DaoParser *self )
{
	self->regCount --;
	MAP_Erase( self->routine->body->localVarType, self->regCount );
}
static void DaoParser_PopRegisters( DaoParser *self, int n )
{
	int i;
	if( n <0 ) return;
	for(i=0; i<n; i++){
		MAP_Erase( self->routine->body->localVarType, self->regCount - i - 1 );
	}
	self->regCount -= n;
}
static void DaoParser_Restore( DaoParser *self, DaoInode *back, int regCount )
{
	DaoLexer_Reset( self->elexer );
	DaoLexer_Reset( self->wlexer );
	DaoParser_PopCodes( self, back );
	DaoParser_PopRegisters( self, self->regCount - regCount );
}

void DaoType_MapNames( DaoType *self );
DaoType* DaoParser_ParseTypeItems( DaoParser *self, int start, int end, DArray *types );
static int DaoParser_MakeArithTree( DaoParser *self, int start, int end, int *cst );

static DaoType* DaoParser_ParseCodeBlockType( DaoParser *self, int start, int *next )
{
	DaoType *type;
	DaoNamespace *ns = self->nameSpace;
	DArray *types = self->typeItems;
	int tcount = types->size;
	int ecount = self->errors->size;
	int rb = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, start, -1 );
	if( rb < 0 ) return NULL;
	type = DaoParser_ParseTypeItems( self, start+1, rb-1, types );
	if( self->errors->size > ecount ) return NULL;
	*next = rb + 1;
	type = DaoNamespace_MakeType( ns, "", DAO_CODEBLOCK, (DaoValue*) type, types->items.pType + tcount, types->size - tcount );
	types->size = tcount;
	return type;
}

static DaoType* DaoParser_MakeIndexedHolder( DaoParser *self, int index )
{
	DaoType *type;
	char name[20];
	sprintf( name, "@%i", index );
	type = DaoType_New( name, DAO_THT, NULL, NULL );
	DMap_Insert( self->initTypes, type->name, type );
	return type;
}
DaoToken* DaoToken_Copy( DaoToken *self );

static int DaoParser_ParseDecoTargets( DaoParser *self, int start, int to, DArray *targets )
{
	DaoToken **tks = self->tokens->items.pToken;
	int i = start;
	while( i <= to && (tks[i]->name == DTOK_IDENTIFIER || tks[i]->name == DTOK_TILDE) ){
		DString *pat = (DString*) DArray_PushBack( targets, & tks[i]->string );
		int expecting = tks[i]->name == DTOK_IDENTIFIER ? DTOK_TILDE : DTOK_IDENTIFIER;
		int expecting2 = tks[i]->name == DTOK_IDENTIFIER ? DTOK_IDENTIFIER : 0;
		if( ++i <= to && tks[i]->name == expecting ){
			DString_Append( pat, & tks[i]->string );
			if( ++i <= to && tks[i]->name == expecting2 ){
				DString_Append( pat, & tks[i++]->string );
			}
		}
		if( tks[i]->name != DTOK_COMMA ) break;
	}
	if( tks[i]->name != DTOK_LCB ) return -1;
	return i;
}
static int DaoParser_ExtractRoutineBody( DaoParser *self, DaoParser *parser, int left )
{
	DaoRoutine *routine = parser->routine;
	DaoToken **tokens = self->tokens->items.pToken;
	int i, right = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, left, -1 );
	if( right < 0 ) return -1;

	DArray_Append( routine->nameSpace->definedRoutines, routine );
	routine->body->codeStart = tokens[left]->line;
	routine->body->codeEnd = tokens[right]->line;
	for(i=left+1; i<right; ++i) DaoLexer_AppendToken( parser->lexer, tokens[i] );
	parser->defined = 1;
	return right;
}

int DaoParser_ParseSignature( DaoParser *self, DaoParser *module, int key, int start )
{
	DNode *node;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoToken *tk, *tok, *nameTok = tokens[start];
	DaoNamespace *NS = self->nameSpace;
	DaoInterface *inter = module->hostInter;
	DaoRoutine *routine = module->routine;
	DaoClass  *klass = module->hostClass;
	DaoType  *cdata = module->hostCdata;
	DaoType *hostype = module->hostType;
	DaoType *type, *type_default, *cbtype = NULL, *retype = NULL;
	DArray *types = NULL, *nested = NULL;
	DString *hostname = NULL;
	DString *pname = NULL;
	DString *mbs = NULL;
	int size = self->tokens->size;
	int i, j, k, right;
	int line = 0; /* XXX number of super classes */
	int e1=start, e2=size-1, ec = 0;
	int isMeth, notStatic, notConstr;
	int hasdeft = 0, selfpar = 0;
	int isconstru = klass != NULL;

	DString_Assign( routine->routName, & nameTok->string );
	DString_Assign( module->routName, routine->routName  );
	DString_Assign( module->fileName, self->fileName );
	GC_ShiftRC( self->nameSpace, routine->nameSpace );
	module->nameSpace = self->nameSpace;
	routine->nameSpace = self->nameSpace;
	self->returnType = NULL;
	if( start + 2 >= size ) return -1;
	start ++;
	if( key == DKEY_OPERATOR ){
		int lb = 0;
		if( tokens[start-1]->name == DTOK_LB ){ /* operator () */
			if( tokens[start]->name != DTOK_RB ) goto ErrorUnsupportedOperator;
			lb = DaoParser_FindOpenToken( self, DTOK_LB, start+1, -1, 1 );
		}else if( tokens[start-1]->name == DTOK_LSB ){ /* operator [] */
			if( tokens[start]->name != DTOK_RSB ) goto ErrorUnsupportedOperator;
			lb = DaoParser_FindOpenToken( self, DTOK_LB, start+1, -1, 1 );
		}else{
			lb = DaoParser_FindOpenToken( self, DTOK_LB, start, -1, 1 );
		}
		if( lb <0 ) goto ErrorUnsupportedOperator;
		for(i=start; i<lb; i++) DString_Append( routine->routName, & tokens[i]->string );
		DString_Assign( module->routName, routine->routName );
		start = lb;
	}else if( tokens[start]->name == DTOK_LT ){ /* constructor of template types */
		int lb = DaoParser_FindPairToken( self, DTOK_LT, DTOK_GT, start, -1 );
		if( lb < 0 ) return -1;
		for(i=start; i<=lb; i++) DString_Append( routine->routName, & tokens[i]->string );
		DString_Assign( module->routName, routine->routName );
		start = lb + 1;
	}
	if( klass ) isconstru &= DString_EQ( routine->routName, klass->className );

	if( tokens[start]->name != DTOK_LB ) return -1;
	right = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
	if( right < 0 ) return -1;

	if( inter ) hostname = inter->abtype->name;
	else if( klass ) hostname = klass->className;
	else if( cdata ) hostname = cdata->name;

	mbs = DaoParser_GetString( self );
	pname = DaoParser_GetString( self );
	nested = DaoParser_GetArray( self );
	DString_Reserve( mbs, 128 );
	DString_Reserve( pname, 128 );
	if( nameTok->name == DTOK_ID_THTYPE && isconstru == 0 ){
		routine->attribs |= DAO_ROUT_DECORATOR;
		DString_AppendChar( pname, '@' );
	}
	DString_AppendMBS( pname, "routine<" );
	routine->parCount = 0;
	if( tokens[start+1]->name == DKEY_SELF ) selfpar = 1;

	isMeth = klass && routine != klass->classRoutine;
	notStatic = (routine->attribs & DAO_ROUT_STATIC) ==0;
	notConstr = hostname && DString_EQ( routine->routName, hostname ) == 0;
	if( self->isClassBody && notConstr == 0 ) routine->attribs |= DAO_ROUT_INITOR;
	if( (isMeth || inter) && tokens[start+1]->name != DKEY_SELF && notStatic && notConstr ){
		type = DaoNamespace_MakeType( NS, "self", DAO_PAR_NAMED, (DaoValue*)hostype, NULL, 0 );
		DArray_Append( nested, (void*) type ); /* self parameter type */
		DaoRoutine_AddConstant( routine, NULL ); /* no default parameter; */
		DString_AppendMBS( pname, type->name->mbs );
		if( routine->body ){
			tok = DArray_Append( routine->body->defLocals, tokens[start] );
			DaoToken_Set( tok, 1, 0, routine->parCount, "self" );
			MAP_Insert( routine->body->localVarType, module->regCount, type );
		}
		DString_SetMBS( mbs, "self" );
		MAP_Insert( DaoParser_GetCurrentDataMap( module ), mbs, module->regCount );
		DaoParser_PushRegister( module );
		routine->parCount ++;
		selfpar = 1;
	}
	if( selfpar ) routine->attribs |= DAO_ROUT_PARSELF;
	DMap_Clear( module->initTypes );
	if( hostype ) DaoType_GetTypeHolders( hostype, module->initTypes );
	self->innerParser = module;
	type = NULL;
	i = start + 1;
	while( i < right ){
		unsigned char tki, tki2;
		int comma, regCount = module->regCount;
		DaoInode *back = self->vmcLast;
		DaoValue *dft = NULL;
		DString *tks = NULL;

		e1 = i;
		e2 = right;
		module->curLine = self->curLine = tokens[i]->line;
		tks = & tokens[i]->string;
		tki = tokens[i]->name;
		tki2 = tokens[i+1]->name;
		if( tokens[i]->type == DTOK_IDENTIFIER ){
			/*
			   printf( "name = %s; reg = %i\n", tokens[i]->string->mbs, module->regCount );
			 */
			if( routine->parCount && tokens[i-1]->type == DTOK_IDENTIFIER ) goto ErrorNeedSeparator;
			if( routine->body ){
				tk = DArray_Append( routine->body->defLocals, tokens[i] );
				DaoToken_Set( tk, 1, 0, routine->parCount, NULL );
			}
			MAP_Insert( DaoParser_GetCurrentDataMap( module ), tks, module->regCount );
			DaoParser_PushRegister( module );
			routine->parCount ++;
			if( (routine->attribs & DAO_ROUT_DECORATOR) && i == start + 1 ){
				if( i+3 >= right ) goto ErrorInvalidDecoParam;
				if( tokens[i+1]->name != DTOK_LB ) goto ErrorInvalidDecoParam;
				if( tokens[i+2]->name != DTOK_IDENTIFIER ) goto ErrorInvalidDecoParam;
				if( tokens[i+3]->name != DTOK_RB ) goto ErrorInvalidDecoParam;
				module->decoArgName = DaoToken_Copy( tokens[i+2] );
				tki2 = tokens[i+4]->name;
				i += 3;
			}
		}
		if( type && type->tid == DAO_PAR_VALIST ){
			e1 = i;  e2 = right;
			goto ErrorMiddleValist;
		}

		type_default = type = NULL;
		if( tki == DTOK_DOTS ){
			routine->parCount = DAO_MAX_PARAM;
			module->regCount = DAO_MAX_PARAM;
			i += 1;
			if( tokens[i]->name == DTOK_COLON ){
				if( i+1 >= right || tokens[i+1]->type != DTOK_IDENTIFIER ) goto ErrorNeedType;
				type = DaoParser_ParseType( self, i+1, right-1, &i, NULL );
				if( type == NULL ) goto ErrorParamParsing;
			}
			type = DaoNamespace_MakeType( NS, "...", DAO_PAR_VALIST, (DaoValue*)type, NULL, 0 );
		}else if( tki == DTOK_ID_THTYPE ){
			type = DaoParser_ParseType( self, i, right-1, &i, NULL );
			if( type == NULL ) goto ErrorInvalidParam;
			type = DaoNamespace_GetType( NS, (DaoValue*) type );
		}else if( tki2 == DTOK_COLON || tki2 == DTOK_ASSN ){
			i ++;
			if( tokens[i]->name == DTOK_COLON ){
				if( i+1 >= right || tokens[i+1]->type != DTOK_IDENTIFIER ) goto ErrorNeedType;
				type = DaoParser_ParseType( self, i+1, right-1, &i, NULL );
				if( type == NULL ) goto ErrorParamParsing;
			}
			if( tokens[i]->name == DTOK_ASSN ){
				int reg=1, cst = 0;
				hasdeft = i;
				if( i+1 >= right || tokens[i+1]->name == DTOK_COMMA ) goto ErrorNeedDefault;
				comma = DaoParser_FindOpenToken( self, DTOK_COMMA, i, -1, 0 );
				if( comma < 0 ) comma = right;
				e1 = i + 1;
				e2 = comma - 1;
#if 0
				printf( "cst = %i;  reg = %i, %s\n", cst, reg, type?type->name->mbs:"" );
				for(j=i+1; j<comma; j++) printf( "%s\n", tokens[j]->string->mbs );
#endif
				self->needConst += 1;
				DArray_PushFront( self->enumTypes, type );
				reg = DaoParser_MakeArithTree( self, i+1, comma-1, & cst );
				DArray_PopFront( self->enumTypes );
				self->needConst -= 1;
				if( reg < 0 ) goto ErrorInvalidDefault;
				if( cst ){
					dft = DaoParser_GetVariable( self, cst );
					type_default = DaoNamespace_GetType( NS, dft );
				}else if( module->uplocs ){
					int loc = routine->routConsts->items.size;
					DArray_Append( module->uplocs, reg );
					DArray_Append( module->uplocs, loc );
					DArray_Append( module->uplocs, i+1 );
					DArray_Append( module->uplocs, comma-1 );
					type_default = DaoParser_MakeIndexedHolder( module, routine->parCount );
				}else{
					goto ErrorVariableDefault;
				}
				if( type_default == NULL ) goto ErrorInvalidDefault;
				if( cst && type && DaoType_MatchValue( type, dft, NULL ) ==0 ) goto ErrorImproperDefault;
				if( type == NULL ) type = type_default;
				i = comma;
			}
		}else if( tokens[i]->type == DTOK_IDENTIFIER ){
			type = DaoParser_MakeIndexedHolder( module, routine->parCount );
			i += 1;
		}else{
			goto ErrorInvalidParam;
		}
		if( hasdeft && dft == NULL && type_default == NULL && module->outerParser == NULL ){
			e1 = hasdeft;  e2 = right;
			goto ErrorInvalidDefault;
		}
		if( nameTok->name == DTOK_ID_THTYPE ){
			if( nested->size == selfpar && type->tid != DAO_ROUTINE ) goto ErrorInvalidParam;
		}

		if( routine->body ) MAP_Insert( routine->body->localVarType, regCount, type );
		if( type->tid != DAO_PAR_VALIST ){
			j = type_default ? DAO_PAR_DEFAULT : DAO_PAR_NAMED;
			type = DaoNamespace_MakeType( NS, tks->mbs, j, (DaoValue*) type, NULL, 0 );
		}

		DArray_Append( nested, (void*) type );
		DaoRoutine_AddConstant( routine, dft );
		k = pname->size >0 ? pname->mbs[pname->size-1] : 0;
		if( k !='<' ) DString_AppendMBS( pname, "," );
		DString_AppendMBS( pname, type->name->mbs );
		if( module->outerParser == NULL ) DaoParser_PopCodes( self, back );

		if( i >= right ) break;
		if( tokens[i]->name == DKEY_AS ){
			if( (i+1) >= right || tokens[i+1]->type != DTOK_IDENTIFIER ) goto ErrorParamParsing;
			if( module->argName ) goto ErrorParamParsing; // TODO: error, duplicate "as";
			module->argName = DaoToken_Copy( tokens[i+1] );
			i += 2;
			if( i < right ) goto ErrorParamParsing;
		}else if( tokens[i]->name != DTOK_COMMA ){
			goto ErrorParamParsing;
		}
		i ++;
	}
	if( routine->parCount > DAO_MAX_PARAM ) goto ErrorTooManyParams;


	e1 = right + 1;
	e2 = size - 1;
	if( right+1 < size && tokens[right+1]->name == DTOK_LSB ){
		cbtype = DaoParser_ParseCodeBlockType( self, right+1, & start );
		if( cbtype == NULL ) goto ErrorInvalidTypeForm;
		right = start - 1;
	}
	if( right+1 < size && tokens[right+1]->name == DTOK_FIELD ){
		e1 += 1;
		start = right + 1;
		if( routine->attribs & DAO_ROUT_DECORATOR ) goto ErrorInvalidReturn;
		if( isconstru ) goto ErrorConstructorReturn; /* class constructor should not return a value */
		retype = DaoParser_ParseType( self, start + 1, self->tokens->size-1, & right, NULL );
		if( retype == NULL ) goto ErrorInvalidTypeForm;
		right -= 1;
	}
	self->innerParser = NULL;

	k = pname->size;
	if( notConstr == 0 ){
		if( klass && routine->routHost == klass->objType ){
			retype = klass->objType;
		}else if( cdata && routine->routHost == cdata ){
			retype = cdata;
		}
	}
	if( retype == NULL ){
		if( routine->body == NULL ){
			retype = DaoNamespace_MakeValueType( NS, dao_none_value );
		}else{
			retype = dao_type_udf;
		}
	}
	DString_AppendMBS( pname, "=>" );
	DString_Append( pname, retype->name );
	if( key == DKEY_OPERATOR && strcmp( routine->routName->mbs, "cast" ) ==0 ){
		DaoType *tt;
		if( nested->size > selfpar ) goto ErrorTooManyParams;
		if( retype == NULL || retype->tid == DAO_UDT ) goto ErrorInvalidReturn;
		tt = DaoNamespace_GetType( NS, (DaoValue*) retype );
		DString_Erase( pname, k, -1 );
		DString_AppendChar( pname, ',' );
		DString_Append( pname, tt->name );
		DString_AppendMBS( pname, "=>" );
		DString_Append( pname, retype->name );
		tt = DaoType_New( "", DAO_PAR_NAMED, (DaoValue*) tt, NULL );
		DArray_Append( NS->auxData, (void*) tt );
		DArray_Append( nested, (void*) tt );
		DaoRoutine_AddConstant( routine, NULL );
		DaoParser_PushRegister( module );
		routine->parCount ++;
	}
	DString_AppendMBS( pname, ">" );
	type = DaoType_New( pname->mbs, DAO_ROUTINE, (DaoValue*) retype, nested );
	DArray_Append( NS->auxData, type );
	if( cbtype ){
		GC_ShiftRC( cbtype, type->cbtype );
		type->cbtype = cbtype;
		type->attrib |= DAO_TYPE_CODESECT;
		routine->attribs |= DAO_ROUT_CODESECT;
		DString_Append( type->name, cbtype->name );
		DString_Append( pname, cbtype->name );
	}
	assert( routine->routType == NULL );
	routine->routType = DaoNamespace_FindType( NS, pname );
	if( DaoType_MatchTo( type, routine->routType, NULL ) != DAO_MT_EQ ){
		routine->routType = type;
		DaoNamespace_AddType( NS, pname, routine->routType );
		DString_SetMBS( mbs, "self" );
		node = MAP_Find( routine->routType->mapNames, mbs );
		if( node && node->value.pInt == 0 ) routine->routType->attrib |= DAO_TYPE_SELF;
	}
	/* printf( "%i  %s\n", routine->parCount, routine->routType->name->mbs ); */
	/* for(j=0; j<nested->size; j++) printf( "%s\n", nested->items.pType[j]->name->mbs ); */
	GC_IncRC( routine->routType );
	/*  remove vmcode for consts */
	DaoParser_ClearCodes( module );
	/* one parse might be used to compile multiple C functions: */
	if( routine->body == NULL ) DMap_Reset( module->allConsts );

	/* Resever enough space for default parameters for function decoration: */
	if( (routine->attribs & DAO_ROUT_DECORATOR) && routine->routConsts->items.size < DAO_MAX_PARAM )
		DArray_Resize( & routine->routConsts->items, DAO_MAX_PARAM, NULL );

	module->parEnd = right;
	if( routine->body == NULL || right+1 >= size ) return right;

	if( isconstru ){
		right = DaoParser_ParseInitSuper( self, module, right + 1 );
		if( right <0 ) return -1;
		right --;
	}else if( tokens[right+1]->name == DTOK_COLON ){
		goto ErrorRoutine;
	}
	right += 1;
	if( tokens[right]->name == DKEY_FOR ){
		DaoRoutineBody *body = routine->body;
		ec = DAO_INVALID_DECO_PATTERN;
		if( !(routine->attribs & DAO_ROUT_DECORATOR) ) goto ErrorRoutine;
		if( body->decoTargets == NULL ) body->decoTargets = DArray_New(D_STRING);
		right = DaoParser_ParseDecoTargets( self, right+1, size-1, body->decoTargets );
		if( right < 0 ) goto ErrorRoutine;
		ec = 0;
	}
	if( tokens[right]->name != DTOK_LCB ) return right;

	start = right;
	e2 = start;
	if( tokens[start]->name == DTOK_LCB ){
		right = DaoParser_ExtractRoutineBody( self, module, start );
		if( right < 0 ) goto ErrorRoutine;
	}
	return right;
ErrorInvalidDecorator: ec = DAO_ROUT_INVALID_DECORATOR; goto ErrorRoutine;
ErrorUnsupportedOperator: ec = DAO_ROUT_INVALID_OPERATOR; goto ErrorRoutine;
ErrorConstructorReturn: ec = DAO_ROUT_CONSTRU_RETURN; goto ErrorRoutine;
ErrorNeedReturnType:  ec = DAO_ROUT_NEED_RETURN_TYPE; goto ErrorRoutine;
ErrorInvalidDecoParam:   ec = DAO_ROUT_INVALID_DECO_PARAM; goto ErrorRoutine;
ErrorInvalidTypeForm: ec = DAO_INVALID_TYPE_FORM; goto ErrorRoutine;
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
ErrorRoutine:
	if( ec ){
		if( e2 >= size ) e2 = size - 1;
		DString_Clear( self->mbs );
		for(i=e1; i<=e2; i++){
			DString_Append( self->mbs, & tokens[i]->string );
			if( self->mbs->size > 20 ) break;
		}
		DaoParser_Error( self, ec, self->mbs );
	}
	return -1;
}

static void DaoParser_ByteEncodeGetConst( DaoParser *self, DString *name )
{
	DaoByteBlock *block = self->byteBlock;
	DaoNamespace *ns = self->nameSpace;
	DaoValue *value = NULL;
	int id, opcode = 0;
	if( self->hostClass && (id = DaoClass_FindConst( self->hostClass, name )) >= 0 ){
		value = DaoClass_GetConst( self->hostClass, id );
		opcode = DVM_GETCK;
	}else if( (id = DaoNamespace_FindConst( ns, name )) >= 0 ){
		value = DaoNamespace_GetConst( ns, id );
		opcode = DVM_GETCG;
	}
	if( value != NULL && DaoByteBlock_FindObjectBlock( block, value ) == NULL ){
		DaoByteBlock *namebk = DaoByteBlock_EncodeString( block, name );
		DaoByteBlock *eval = DaoByteBlock_AddEvalBlock( block, value, opcode, 1, NULL );
		DaoByteBlock_InsertBlockIndex( eval, eval->end, namebk );
	}
}
static DaoType* DaoType_FindType( DString *name, DaoNamespace *ns, DaoType *ctype, DaoClass *klass, DaoRoutine *rout )
{
	DNode *node = NULL;
	if( rout && rout->body ) node = MAP_Find( rout->body->abstypes, name );
	if( node == NULL && klass ) node = MAP_Find( klass->abstypes, name );
	if( node == NULL && ctype && ctype->kernel && ctype->kernel->values ){
		node = MAP_Find( ctype->kernel->values, name );
		if( node && node->value.pValue->type == DAO_TYPE ) return node->value.pType;
		node = NULL;
	}
	if( node ) return node->value.pType;
	if( ns ) return DaoNamespace_FindType( ns, name );
	return NULL;
}
static int DaoParser_ParseAtomicExpression( DaoParser *self, int start, int *cst );
static DaoType* DaoParser_ParseValueType( DaoParser *self, int start )
{
	DaoValue *value;
	int cst = 0;
	self->needConst += 1;
	DaoParser_ParseAtomicExpression( self, start, & cst );
	self->needConst -= 1;
	if( cst ==0 ) return NULL;
	value = DaoParser_GetVariable( self, cst );
	return DaoNamespace_MakeValueType( self->nameSpace, value );
}
static DaoType* DaoParser_ParseUserType( DaoParser *self, int start, int end, int *newpos )
{
	DaoInode *back = self->vmcLast;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoNamespace *ns = self->nameSpace;
	DaoType *type = NULL;
	DaoValue *value = NULL;
	DString *name = & tokens[start]->string;
	int t, k = DaoParser_FindScopedConstant( self, &value, start );
	if( self->byteBlock && k == start ){
		DaoParser_ByteEncodeGetConst( self, name );
	}
	DaoParser_PopCodes( self, back );
	if( k <0 ) k = start;
	*newpos = k + 1;
	switch( value ? value->type : 0 ){
	case DAO_CLASS : type = value->xClass.objType; break;
	case DAO_CTYPE : type = value->xCtype.cdtype; break; /* get its cdata type */
	case DAO_TYPE  : type = & value->xType; break;
	case DAO_INTERFACE : type = value->xInterface.abtype; break;
	default : break;
	}
	if( type ) return type;
	type = DaoType_FindType( name, ns, self->hostCdata, self->hostClass, self->routine );
	if( type ) return type;
	if( value == NULL ) return NULL;
	return DaoNamespace_MakeValueType( ns, value );
}
static DaoType* DaoParser_FindTypeHolder( DaoParser *self, DString *name )
{
	DNode *it = NULL;
	if( self->innerParser ) it = DMap_Find( self->innerParser->initTypes, name );
	if( it == NULL ) it = DMap_Find( self->initTypes, name );
	if( it ) return it->value.pType;
	if( self->outerParser ) return DaoParser_FindTypeHolder( self->outerParser, name );
	return NULL;
}
static DaoType* DaoParser_ParsePlainType( DaoParser *self, int start, int end, int *newpos )
{
	DaoType *type = NULL;
	DaoCdata *cdata = & dao_default_cdata;
	DaoNamespace *ns = self->nameSpace;
	DaoClass *klass = self->hostClass;
	DaoRoutine *routine = self->routine;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoToken *token = tokens[start];
	DString *name = & token->string;
	int i = token->name > DKEY_USE ? dao_keywords[ token->name - DKEY_USE ].value : 0;

	if( self->byteBlock && end >= start && token->name == DTOK_IDENTIFIER ){
		DaoParser_ByteEncodeGetConst( self, name );
	}

	if( end > start && token->name == DTOK_IDENTIFIER ){
		type = DaoParser_ParseUserType( self, start, end, newpos );
		if( type ) return type;
	}

	*newpos = start + 1;
	if( i > 0 && i < 100 ){
		/* Always compile unscoped builtin type names as builtin types: */
		DaoValue *pbasic = token->name == DKEY_CDATA ? cdata->ctype->aux : NULL;
		type = DaoNamespace_MakeType( self->vmSpace->nsInternal, name->mbs, i, pbasic, 0,0 );
		if( type->tid == DAO_TUPLE ) type->variadic = 1; /* "tuple" is variadic; */
		return type;
	}else if( token->name == DTOK_ID_THTYPE ){
		type = DaoParser_FindTypeHolder( self, & token->string );
		if( type ) return type;
	}
	type = DaoType_FindType( name, ns, self->hostCdata, klass, routine );
	if( type && type->tid == DAO_CTYPE ) type = type->kernel->abtype; /* get its cdata type */
	if( type ) return type;
	if( i > 0 && i < 100 ){
		DaoValue *pbasic = token->name == DKEY_CDATA ? (DaoValue*) cdata : NULL;
		type = DaoNamespace_MakeType( ns, name->mbs, i, pbasic, 0,0 );
	}else if( token->name == DKEY_NONE ){
		type = DaoNamespace_MakeValueType( ns, dao_none_value );
	}else if( token->name == DTOK_ID_SYMBOL ){
		type = DaoParser_ParseValueType( self, start );
	}else if( token->name == DTOK_ID_THTYPE ){
		DMap *initypes = self->innerParser ? self->innerParser->initTypes : self->initTypes;
		type = DaoType_New( token->string.mbs, DAO_THT, NULL, NULL );
		DMap_Insert( initypes, type->name, type );
	}else if( token->name == DTOK_QUERY ){
		type = dao_type_udf;
	}else{
		/* scoped type or user defined template class */
		type = DaoParser_ParseUserType( self, start, end, newpos );
		if( type == NULL ) goto InvalidTypeName;
	}
	return type;
InvalidTypeName:
	DaoParser_ErrorToken( self, DAO_INVALID_TYPE_NAME, tokens[start] );
	return NULL;
}

DaoType* DaoParser_ParseTypeItems( DaoParser *self, int start, int end, DArray *types )
{
	DaoNamespace *ns = self->nameSpace;
	DaoToken **tokens = self->tokens->items.pToken;
	int i = start;
	while( i <= end ){
		DaoType *type = NULL;
		DString *name = NULL;
		int tid = DAO_NONE;
		int t = (i+1 <= end) ? tokens[i+1]->type : 0;

		if( i == start && tokens[i]->type == DTOK_FIELD ) goto ReturnType;
		if( tokens[i]->type >= DTOK_ID_SYMBOL && tokens[i]->type <= DTOK_WCS ){
			type = DaoParser_ParseValueType( self, i );
			i += 1;
		}else if( tokens[i]->type == DTOK_DOTS ){
			i += 1;
			if( tokens[i]->type == DTOK_COLON )
				type = DaoParser_ParseType( self, i+1, end, & i, types );
			type = DaoNamespace_MakeType( ns, "...", DAO_PAR_VALIST, (DaoValue*) type, NULL, 0 );
		}else{
			if( tokens[i]->type == DTOK_IDENTIFIER && (t == DTOK_COLON || t == DTOK_ASSN) ){
				name = & tokens[i]->string;
				tid = (t == DTOK_COLON) ? DAO_PAR_NAMED : DAO_PAR_DEFAULT;
				if( i + 2 > end ) goto InvalidTypeForm;
				i = i + 2;
			}
			type = DaoParser_ParseType( self, i, end, & i, types );
		}
		if( type == NULL ) return NULL;
		if( name ) type = DaoNamespace_MakeType( ns, name->mbs, tid, (DaoValue*)type, NULL,0 );
		DArray_Append( types, type );
		if( i > end ) break;
ReturnType:
		if( tokens[i]->type == DTOK_FIELD ){
			type = DaoParser_ParseType( self, i+1, end, & i, NULL );
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
	DaoParser_ErrorToken( self, DAO_INVALID_TYPE_FORM, tokens[i] );
	return NULL;
}
int DaoParser_ParseTemplateParams( DaoParser *self, int start, int end, DArray *holders, DArray *defaults, DString *name )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int i = start;
	while( i < end ){
		DaoType *holder, *deft = NULL;
		DString *str = & tokens[i]->string;
#if 0
		if( tokens[i]->name != DTOK_ID_THTYPE ){
			DaoParser_Error( self, DAO_TOKEN_NOT_EXPECTED, str );
			return 0;
		}
#endif
		if( tokens[i]->type >= DTOK_ID_SYMBOL && tokens[i]->type <= DTOK_WCS ){
			holder = DaoParser_ParseValueType( self, i );
			i += 1;
		}else{
			holder = DaoParser_ParseType( self, i, end-1, &i, NULL );
		}
		if( holder == NULL ) return 0;
		if( name ){
			if( holders->size ) DString_AppendChar( name, ',' );
			DString_Append( name, holder->name );
		}
		if( i < end && tokens[i]->type == DTOK_ASSN ){
			deft = DaoParser_ParseType( self, i+1, end-1, &i, NULL );
			if( deft == NULL ) return 0;
		}
		if( deft == NULL && DArray_Back( defaults ) != NULL ){
			DaoParser_Error( self, DAO_PARAM_NEED_DEFAULT, & tokens[i-1]->string );
			return 0;
		}
		DArray_Append( holders, holder );
		DArray_Append( defaults, deft );
		if( i < end && tokens[i]->type != DTOK_COMMA ){
			DaoParser_Error( self, DAO_TOKEN_NOT_EXPECTED, & tokens[i]->string );
			return 0;
		}
		i += 1;
	}
	return 1;
}

static DaoType* DaoParser_ParseEnumTypeItems( DaoParser *self, int start, int end )
{
	DaoType *type, *type2;
	DaoToken *tok;
	DaoToken **tokens = self->tokens->items.pToken;
	DString *field = NULL;
	uchar_t sep = 0;
	daoint value = 0;
	int k, set=0, sign = 1;
	char c;

	type = DaoType_New( "enum<", DAO_ENUM, NULL, NULL );
	type->mapNames = DMap_New(D_STRING,0);
	DString_Reserve( type->name, 128 );
	for(k=start; k<=end; k++){
		tok = tokens[k];
		field = & tok->string;
		c = tok->string.mbs[0];
		sign = 1;
		if( tok->type != DTOK_IDENTIFIER ) break;
		if( tok->name == DTOK_ID_THTYPE || tok->name == DTOK_ID_SYMBOL ) break;
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
			if( c >= DTOK_DIGITS_DEC && c <= DTOK_NUMBER_HEX ){
				k += 1;
				set = 1;
				value = strtoll( tokens[k]->string.mbs, 0, 0 );
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
	for(k=start; k<=end; k++) DString_Append( type->name, & tokens[k]->string );
	DString_AppendChar( type->name, '>' );
	/*
	   printf( "%i  %i  %s\n", end, i, type->name->mbs );
	 */
	if( (type2 = DaoNamespace_FindType( self->nameSpace, type->name )) ){
		DaoType_Delete( type );
		type = type2;
	}else{
		DaoNamespace_AddType( self->nameSpace, type->name, type );
	}
	return type;
WrongForm:
	DaoParser_ErrorToken( self, DAO_INVALID_TYPE_FORM, tokens[k] );
	DaoType_Delete( type );
	return NULL;
}
static DaoType* DaoParser_ParseType2( DaoParser *self, int start, int end, int *newpos, DArray *types )
{
	DaoType *type = NULL;
	DaoType *type2 = NULL;
	DaoValue *retype = NULL;
	DaoType **nested = NULL;
	DaoNamespace *ns = self->nameSpace;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoToken *tok = tokens[start];
	DString *tks = & tok->string;
	int i, j, t = tokens[start]->name;
	int gt, tid, count, count2;
	int daons = 0, tokname = 0;

#if 0
	for(i=start; i<=end; i++) printf("%s  ", tokens[i]->string.mbs); printf("\n\n");
#endif

	*newpos = start + 1;
	if( start == end || t == DTOK_QUERY || t == DTOK_ID_THTYPE ){
		DaoValue *initype = NULL;
		DaoType *vartype = NULL;
		type = DaoParser_ParsePlainType( self, start, end, newpos );
		if( type == NULL ) return type;
		initype = (DaoValue*) type;
		if( type->tid == DAO_THT && start < end && tokens[start+1]->name == DTOK_LT ){
			DString *name;
			DaoParser *scope = self->innerParser ? self->innerParser : self;
			int gt = DaoParser_FindPairToken( self, DTOK_LT, DTOK_GT, start+1, end );
			if( gt < 0 ) goto WrongType;
			vartype = DaoParser_ParseType( self, start + 2, gt, newpos, types );
			if( vartype == NULL || *newpos != gt ) goto WrongType;
			name = DaoParser_GetString( self );
			DString_Assign( name, type->name );
			DString_AppendChar( name, '<' );
			DString_Append( name, vartype->name );
			DString_AppendChar( name, '>' );
			type2 = DaoParser_FindTypeHolder( self, name );
			if( type2 == NULL ) type2 = DaoType_New( name->mbs, DAO_THT, (DaoValue*) vartype, NULL );
			DMap_Insert( scope->initTypes, type2->name, type2 );
			DMap_Insert( scope->initTypes, type->name, type2 );
			type = type2;
			*newpos = gt + 1;
			if( type2 == NULL ) goto WrongType;
		}
		return type;
WrongType:
		DaoParser_ErrorToken( self, DAO_INVALID_TYPE_FORM, tokens[start] );
		return NULL;
	}
	count = types->size;
	if( tokens[start]->type != DTOK_IDENTIFIER ) goto InvalidTypeName;
	if( (start+1) < end && tokens[start+1]->type == DTOK_COLON2 ){
		if( strcmp( tokens[start]->string.mbs, "dao" ) ==0 ){
			daons = 1;
			start += 2;
			ns = self->vmSpace->nsInternal;
			tok = tokens[start];
			if( tok->name > DKEY_USE ) tokname = dao_keywords[ tok->name - DKEY_USE ].value;
			if( tok->type != DTOK_IDENTIFIER ) goto InvalidTypeName;
		}
	}
	if( tokens[start]->name == DTOK_IDENTIFIER ){
		/* scoped type or user defined template class */
		type = DaoParser_ParseUserType( self, start, end, newpos );
		if( type == NULL ) goto InvalidTypeName;
	}else if( start < end && tokens[start]->name == DKEY_ENUM && tokens[start+1]->name == DTOK_LT ){
		gt = DaoParser_FindPairToken( self, DTOK_LT, DTOK_GT, start, end );
		if( gt < 0 ) goto InvalidTypeForm;
		*newpos = gt + 1;
		type = DaoParser_ParseEnumTypeItems( self, start+2, gt-1 );
	}else if( start < end && tokens[start+1]->name == DTOK_LT ){
		int ecount = self->errors->size;
		gt = DaoParser_FindPairToken( self, DTOK_LT, DTOK_GT, start, end );
		if( gt < 0 ) goto InvalidTypeForm;
		*newpos = gt + 1;
		type = DaoParser_ParseTypeItems( self, start+2, gt-1, types );
		if( self->errors->size > ecount ) goto InvalidTypeForm;
		if( type && tokens[start]->name != DKEY_ROUTINE ) goto InvalidTypeForm;
		count2 = types->size - count;
		retype = NULL;
		tid = DAO_NONE;
		switch( tokens[start]->name ){
		case DKEY_TYPE :
			tid = DAO_TYPE;
			if( count2 != 1 ) goto InvalidTypeForm;
			break;
		case DKEY_ARRAY :
			tid = DAO_ARRAY;
			if( count2 != 1 ) goto InvalidTypeForm;
			type = types->items.pType[ count ];
			if( type->tid == 0 ) goto InvalidTypeForm;
			if( type->tid > DAO_COMPLEX && type->tid != DAO_THT ) goto InvalidTypeForm;
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
			if( type == NULL ) type = DaoNamespace_MakeValueType( ns, dao_none_value );
			if( type->tid >= DAO_PAR_NAMED && type->tid <= DAO_PAR_VALIST ){
				goto InvalidTypeForm;
			}
			retype = (DaoValue*) type;
			break;
		case DKEY_CLASS :
			if( count2 != 1 ) goto InvalidTypeForm;
			type = types->items.pType[ count ];
			DArray_Erase( types, count, count2 );
			switch( type ? type->tid : 0 ){
			case DAO_CDATA :
			case DAO_CSTRUCT : return type->aux->xCtype.ctype;
			case DAO_OBJECT : return type->aux->xClass.clsType;
			}
			goto InvalidTypeForm;
		case DKEY_INTERFACE :
			if( count2 != 1 ) goto InvalidTypeForm;
			type = types->items.pType[ count ];
			DArray_Erase( types, count, count2 );
			if( type == NULL || type->tid != DAO_OBJECT ) goto InvalidTypeForm;
			return type->aux->xClass.inter->abtype;
		default : break;
		}
		if( tid != DAO_TUPLE && tid != DAO_ROUTINE && tid != DAO_CODEBLOCK ){
			for(i=count; i<types->size; ++i){
				DaoType *tp = types->items.pType[i];
				if( tp->tid >= DAO_PAR_NAMED && tp->tid <= DAO_PAR_VALIST ){
					goto InvalidTypeForm;
				}
			}
		}
		tks = & tokens[start]->string;
		nested = types->items.pType + count;
		type = DaoNamespace_MakeType( ns, tks->mbs, tid, retype, nested, count2 );
		if( type == NULL ) goto InvalidTypeForm;
		if( tid == DAO_ROUTINE ){
			DString sname = DString_WrapMBS( "self" );
			DNode *node = MAP_Find( type->mapNames, & sname );
			if( node && node->value.pInt == 0 ) type->attrib |= DAO_TYPE_SELF;
		}
		if( tid == DAO_ROUTINE && gt < end && tokens[gt+1]->type == DTOK_LSB ){
			DaoType *tt, *cbtype = DaoParser_ParseCodeBlockType( self, gt+1, newpos );
			DString *name = DaoParser_GetString( self );
			if( cbtype == NULL ) goto InvalidTypeForm;
			DString_Assign( name, type->name );
			DString_Append( name, cbtype->name );
			tt = DaoNamespace_FindType( ns, name );
			if( tt ){
				type = tt;
			}else{
				type = DaoType_Copy( type );
				//type->attrib = 0; // XXX
				type->attrib |= DAO_TYPE_CODESECT;
				DString_Assign( type->name, name );
				DArray_Append( ns->auxData, type );
			}
		}
		DArray_Erase( types, count, count2 );
	}else if( tokname > 0 && tokname < 100 ){
		DaoCdata *cdata = & dao_default_cdata;
		DaoValue *pbasic = tok->name == DKEY_CDATA ? (DaoValue*) cdata : NULL;
		DString *name = DString_New(1);
		DString_AppendMBS( name, "dao::" );
		DString_Append( name, & tok->string );
		type = DaoNamespace_MakeType( ns, name->mbs, tokname, pbasic, 0,0 );
		DString_Delete( name );
		*newpos = start + 1;
	}else if( tokens[start]->type == DTOK_IDENTIFIER ){
		if( daons ) start -= 2;
		type = DaoParser_ParsePlainType( self, start, end, newpos );
		if( type == NULL ) goto InvalidTypeName;
	}else{
		goto InvalidTypeForm;
	}
#if 0
	printf( "%s %i\n", type->name->mbs, *newpos );
#endif
	return type;
InvalidTypeName:
InvalidTypeForm:
	DaoParser_ErrorToken( self, DAO_INVALID_TYPE_FORM, tokens[start] );
	return NULL;
}
DaoType* DaoParser_ParseType( DaoParser *self, int start, int end, int *next, DArray *types )
{
	DaoNamespace *ns = self->nameSpace;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoType *type = NULL;
	int count;
	if( types == NULL ) types = self->typeItems;
	count = types->size;
	type = DaoParser_ParseType2( self, start, end, next, types );
	if( type == NULL ) goto InvalidType;
	DArray_Append( types, type );
	while( type && *next <= end && tokens[*next]->name == DTOK_PIPE ){
		type = DaoParser_ParseType2( self, *next + 1, end, next, types );
		if( type == NULL ) goto InvalidType;
		DArray_Append( types, type );
	}
	if( types->size == count + 1 ){
		type = types->items.pType[count];
		DArray_PopBack( types );
	}else{
		DaoType **nested = types->items.pType + count;
		int i, count2 = types->size - count;
		type = DaoNamespace_MakeType( ns, "", DAO_VARIANT, NULL, nested, count2 );
		DArray_Erase( types, count, count2 );
		if( type == NULL ) goto InvalidType;
	}
	if( self->byteBlock ) DaoByteBlock_EncodeType( self->byteBlock, type );
	return type;
InvalidType:
	return NULL;
}

int DaoParser_ParseRoutine( DaoParser *self );
DAO_DLL int DaoParser_ParseLoadStatement( DaoParser *self, int start, int end );

static DaoValue* DaoParse_InstantiateType( DaoParser *self, DaoValue *tpl, int start, int end )
{
	DArray *types = DArray_New(0);
	DaoCtype *ctype = (DaoCtype*) tpl;
	DaoType *sptype;

	if( tpl == NULL || tpl->type != DAO_CTYPE ) goto FailedInstantiation;
	DaoParser_ParseTypeItems( self, start, end, types );
	if( self->errors->size ) goto FailedInstantiation;
	sptype = DaoType_Specialize( ctype->cdtype, types->items.pType, types->size );
	if( sptype == NULL ) goto FailedInstantiation;
	if( self->byteBlock ){
		DaoType **ts = types->items.pType;
		DaoByteBlock_EncodeCtype( self->byteBlock, (DaoCtype*) sptype->aux, ctype, ts, types->size );
		DaoByteBlock_EncodeType( self->byteBlock, sptype );
	}

DoneInstantiation:
	DArray_Delete( types );
	return sptype->aux;
FailedInstantiation:
	DArray_Delete( types );
	return NULL;
}


static int DaoParser_Preprocess( DaoParser *self );
int DaoParser_ParseScript( DaoParser *self )
{
	DaoNamespace *ns = self->nameSpace;
	DaoVmSpace   *vmSpace = self->vmSpace;
	DaoRoutine *routMain = self->routine; /* could be set in DaoVmSpace_Eval() */
	daoint i, bl;

	if( routMain == NULL ) routMain = DaoRoutine_New( ns, NULL, 1 );

	/*
	   printf("DaoParser_ParseScript() ns=%p, rout=%p, %s\n", ns, routMain, self->fileName->mbs );
	 */

	if( self->byteBlock ){
		GC_ShiftRC( routMain, self->byteBlock->value );
		self->byteBlock->value = (DaoValue*) routMain;
	}
	if( routMain->routType == NULL ){
		routMain->routType = dao_type_routine;
		GC_IncRC( dao_type_routine );
	}
	routMain->attribs |= DAO_ROUT_MAIN;
	ns->mainRoutine = routMain;
	DaoNamespace_SetConst( ns, DVR_NSC_MAIN, (DaoValue*) routMain );
	DString_SetMBS( self->routName, "__main__" );
	DString_SetMBS( routMain->routName, "__main__" );
	DArray_Append( ns->mainRoutines, routMain );
	/* the name of routMain will be set in DaoParser_ParseRoutine() */

	routMain->body->codeStart = 1;
	routMain->body->codeEnd = self->lineCount;
	self->routine = routMain;
	self->vmSpace = vmSpace;
	self->nameSpace = ns;

	if( DaoParser_Preprocess( self ) == 0 || DaoParser_ParseRoutine( self ) == 0 ){
		DaoParser_PrintError( self, 0, 0, NULL );
		return 0;
	}
	/*
	// Currently the __main__ routine is inferenced after other routines.
	// As a result, some code in those routines cannot be specialized,
	// because the type information for some global variable may not be
	// available. Redoing inference on those routines may allow these
	// code to be specialized.
	//
	// Do not redo such inference on compiling to bytecode.
	// Because the type information for some global variables (declare
	// without explicit types) are not yet available when those routines
	// are decoded. Without proper type information, type errors may arise
	// for specialized code that use global variables.
	//
	// Example:
	//   global glb = rand(100)
	//   routine func() 
	//   {
	//     return glb + 456 
	//   }
	*/
	if( self->byteBlock ) return 1;
	for(i=0; i<self->routReInferable->size; i++){
		DaoRoutine *rout = (DaoRoutine*) self->routReInferable->items.pValue[i];
		DaoRoutine_DoTypeInference( rout, 0 );
	}
	self->routReInferable->size = 0;
	return 1;
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
static DaoInode* DaoParser_AddCode( DaoParser *self, ushort_t code,
		ushort_t a, ushort_t b, ushort_t c, int first, int mid, int last )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int line = 0;
	if( first < self->tokens->size ) line = tokens[first]->line;
	else if( self->tokens->size ) line = tokens[self->tokens->size-1]->line;
	return DaoParser_AddCode2( self, code, a, b, c, first, mid, last );
}
static DaoInode* DaoParser_AddCode2( DaoParser *self, ushort_t code,
		ushort_t a, ushort_t b, ushort_t c, int first, int mid, int last )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoInode *node = DaoParser_NewNode( self );
	int line = 0;
	if( first < self->tokens->size ) line = tokens[first]->line;
	else if( self->tokens->size ) line = tokens[self->tokens->size-1]->line;

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

	return node;
}

static void DaoParser_DeclareVariable( DaoParser *self, DaoToken *tok, int vt, DaoType *tp );
static int DaoParser_ParseCondition( DaoParser *self, int start, int dec, DaoInode *opening );
static int DaoParser_ParseForLoop( DaoParser *self, int start, int end );
static int DaoParser_PostParsing( DaoParser *self );

DaoType* DaoParser_ParseTypeName( const char *name, DaoNamespace *ns, DaoClass *cls )
{
	DaoParser *parser = DaoVmSpace_AcquireParser( ns->vmSpace );
	DaoType *type = NULL;
	int i = 0;
	if( ! DaoLexer_Tokenize( parser->lexer, name, DAO_LEX_ESCAPE ) ) goto ErrorType;
	DaoNamespace_InitConstEvalData( ns );
	parser->nameSpace = ns;
	parser->hostClass = cls;
	parser->routine = ns->constEvalRoutine;
	parser->vmSpace = ns->vmSpace;
	type = DaoParser_ParseType( parser, 0, parser->tokens->size-1, &i, NULL );
	if( i < parser->tokens->size && type ) type = NULL;
	DaoVmSpace_ReleaseParser( ns->vmSpace, parser );
	return type;
ErrorType:
	DaoVmSpace_ReleaseParser( ns->vmSpace, parser );
	return NULL;
}
static void DaoParser_SetupSwitch( DaoParser *self, DaoInode *opening )
{
	DaoValue *key, **cst = self->routine->routConsts->items.items.pValue;
	DaoInode *node = opening->jumpTrue;
	DaoInode *it2, *aux;
	DMap *map;
	DNode *iter;
	int i, min, max, count, direct = 0, casemode = 0;
	min = max = 0;
	count = 0;
	map = self->switchMaps->items.pMap[ node->b ];
	for(iter=DMap_First(map); iter !=NULL; iter=DMap_Next(map, iter) ){
		key = cst[iter->value.pInode->a];
		if( key->type == DAO_INTEGER ){
			if( count == 0 ) min = max = key->xInteger.value;
			if( min > key->xInteger.value ) min = key->xInteger.value;
			if( max < key->xInteger.value ) max = key->xInteger.value;
			count ++;
		}
	}
	if( count == map->size && count > 0.75 * (max - min) ){
		DaoValue temp = {DAO_INTEGER};
		key = (DaoValue*) & temp;
		for(i=min+1; i<max; i++){
			key->xInteger.value = i;
			if( DMap_Find( map, key ) ==NULL ) DMap_Insert( map, key, NULL );
		}
		direct = 1;
	}
	node->c = map->size;
	aux = node;
	casemode = direct ? DAO_CASE_TABLE : DAO_CASE_ORDERED;
	for(iter=DMap_First(map); iter !=NULL; iter=DMap_Next(map, iter) ){
		it2 = DaoParser_NewNode( self );
		it2->code = DVM_CASE;
		it2->c = casemode; /* mark integer jump table */
		if( iter->value.pInode ){
			it2->a = iter->value.pInode->a;
			it2->jumpTrue = iter->value.pInode;
			it2->first = iter->value.pInode->first;
			it2->middle = iter->value.pInode->middle;
			it2->last = iter->value.pInode->last;
		}else{
			it2->a = DaoRoutine_AddConstant( self->routine, iter->key.pValue );
			it2->jumpTrue = node->jumpFalse; /* jump to default */
		}
		it2->prev = aux;
		it2->next = aux->next;
		aux->next->prev = it2;
		aux->next = it2;
		aux = it2;
	}
}
static DaoInode* DaoParser_AddScope( DaoParser *self, int code, DaoInode *closing )
{
	DaoInode *node = DaoParser_AddCode( self, code, 0, 0, 0, 0, 0, 0 );
	DArray_Append( self->scopeOpenings, node );
	DArray_Append( self->scopeClosings, closing );
	node->jumpFalse = closing;
	DaoParser_PushLevel( self );
	return node;
}
static int DaoParser_AddScope2( DaoParser *self, int at )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int token = at < self->tokens->size ? tokens[at]->name : 0;
	int code = token == DTOK_LCB ? (int)DVM_LBRA : (int)DVM_NOP;
	DaoParser_AddScope( self, code, NULL );
	return token == DTOK_LCB;
}
static int DaoParser_DelScope( DaoParser *self, DaoInode *node )
{
#if 0
	printf( "DaoParser_DelScope() %i %li\n", self->lexLevel, self->scopeOpenings->size );
	DaoParser_PrintCodes( self );
#endif
	DaoInode *opening = (DaoInode*) DArray_Back( self->scopeOpenings );
	DaoInode *closing = (DaoInode*) DArray_Back( self->scopeClosings );
	DaoParser_PopLevel( self );
	self->vmcValue = NULL;
	if( self->lexLevel < 0 || self->scopeOpenings->size == 0 ){
		DaoParser_Error3( self, DAO_INVALID_SCOPE_ENDING, self->curToken );
		return 0;
	}
	if( opening->code == DVM_BRANCH && closing->c == DVM_SWITCH ){
		DaoInode *branch = opening->jumpTrue; /* condition test */
		DaoParser_PopLevel( self );
		DaoParser_SetupSwitch( self, opening );
	}else if( opening->code == DVM_BRANCH ){
		DaoInode *branch = opening->jumpTrue; /* condition test */
		branch->jumpFalse = closing;
	}else if( opening->code == DVM_LOOP ){
		DaoInode *branch = opening->jumpTrue; /* condition test */
		node = DaoParser_AddCode( self, DVM_GOTO, 0, 0, 0, 0, 0, 0 );
		branch->jumpFalse = closing;
		node->jumpTrue = opening;
	}else if( opening->code == DVM_LBRA ){
		node = DaoParser_AddCode( self, DVM_RBRA, 0, 0, 0, 0, 0, 0 );
	}
	if( closing && closing->next ){
		closing->prev->next = closing->next;
		closing->next->prev = closing->prev;
		closing->prev = self->vmcLast;
		closing->next = NULL;
		self->vmcLast->next = closing;
		self->vmcLast = closing;
	}
	DArray_Pop( self->scopeOpenings );
	DArray_Pop( self->scopeClosings );
	return 1;
}
static int DaoParser_CompleteScope( DaoParser *self, int at )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int token, next = at + 1, size = self->tokens->size;
	while( next < size && tokens[next]->name == DTOK_SEMCO ) next += 1;
	token = next < size ? tokens[next]->name : 0;
	while( self->scopeOpenings->size >0 ){
		DaoInode *back = (DaoInode*) DArray_Back( self->scopeOpenings );
		DaoInode *close = (DaoInode*) DArray_Back( self->scopeClosings );
		if( back->code == DVM_LBRA ) break;
		if( close != NULL && close->a ){
			if( close->a == token ) break;
			if( close->a == DKEY_WHILE ){ /* Too many statements between do-while: */
				DaoParser_Error3( self, DAO_INVALID_SCOPE_ENDING, self->curToken );
				return 0;
			}
		}
		if( DaoParser_DelScope( self, NULL ) ==0 ) return 0;
	}
	return 1;
}
static DaoInode* DaoParser_GetBreakableScope( DaoParser *self )
{
	int i;
	for(i=(int)self->scopeOpenings->size-1; i>=0; i--){
		DaoInode *opening = self->scopeOpenings->items.pInode[i];
		DaoInode *closing = self->scopeClosings->items.pInode[i];
		if( closing && closing->b ) return opening;
	}
	return NULL;
}
void DaoParser_MakeCodes( DaoParser *self, int start, int end, DString *output )
{
	DaoToken *tok, **tokens = self->tokens->items.pToken;
	int i, cpos = 0, line = -1;

	tok = start ? tokens[start-1] : NULL;
	if( tok && tok->name >= DKEY_PRIVATE && tok->name <= DKEY_PUBLIC ){
		DString_Append( output, & tok->string );
		cpos = tok->cpos;
		line = tok->line;
	}
	for(i=start; i<end; i++){
		tok = tokens[i];
		if( (int)tok->line != line ) DString_AppendChar( output, '\n' );
		if( tok->cpos > cpos+1 ) DString_AppendChar( output, ' ' );
		DString_Append( output, & tok->string );
		cpos = tok->cpos;
		line = tok->line;
	}
}
static int DaoParser_HandleVerbatim( DaoParser *self, int start )
{
	DaoNamespace *ns = self->nameSpace;
	DString *verbatim = & self->tokens->items.pToken[start]->string;
	int line = self->tokens->items.pToken[start]->line;
	daoint pstart = 0, pend = verbatim->size-1, wcs = verbatim->mbs[1] == '@';
	const char *pat = "^ @{1,2} %[ %s* %w+ %s* ( %( %s* (|%w+) %s* %) | %])";

	self->curLine = line;
	if( verbatim->mbs[2+wcs] == '+' ){ /* string interuptions */
		DString *body = self->mbs;
		DString *delim = self->mbs2;
		DString *source = DaoParser_GetString( self );
		DString *codes = DaoParser_GetString( self );
		DaoLexer *lexer = DaoLexer_New();
		DArray *tokens = lexer->tokens;
		daoint k, m, rb = DString_FindChar( verbatim, ']', 0 );

		DString_Reset( source, 0 );
		DString_SetDataMBS( body, verbatim->mbs + (rb+1), verbatim->size - 2*(rb+1) );
		DString_SubString( verbatim, delim, 0, rb + 1 );
		DString_InsertChar( delim, 'x', 2+wcs );
		DString_Append( source, delim );
		for(k=0; k<body->size; k++){
			char ch = body->mbs[k];
			if( ch == '@' && body->mbs[k+1] == '{' ){
				daoint open = 1, last = k + 2;
				while( open ){
					daoint rb2 = DString_FindChar( body, '}', last );
					if( rb2 < 0 ) return -1; //XXX
					DString_SubString( body, codes, k+2, rb2 - k - 2 );
					DaoLexer_Tokenize( lexer, codes->mbs, 0 );
					open = tokens->size && tokens->items.pToken[tokens->size-1]->type <= DTOK_WCS_OPEN;
					if( open == 0 ){
						daoint i, n, c1 = 0, c2 = 0, c3 = 0;
						for(i=0,n=tokens->size; i<n; i++){
							switch( tokens->items.pToken[i]->type ){
							case DTOK_LB  : c1 --; break;
							case DTOK_RB  : c1 ++; break;
							case DTOK_LCB : c2 --; break;
							case DTOK_RCB : c2 ++; break;
							case DTOK_LSB : c3 --; break;
							case DTOK_RSB : c3 ++; break;
							}
						}
						if( c1 > 0 || c2 > 0 || c3 > 0 ) return -1; //XXX
						if( c1 < 0 || c2 < 0 || c3 < 0 ) open = 1;
					}
					last = rb2 + 1;
				}
				DString_Append( source, delim );
				DString_AppendMBS( source, "+(string)(" );
				DString_Append( source, codes );
				DString_AppendMBS( source, ")+" );
				DString_Append( source, delim );
				k += codes->size + 2;
			}else{
				DString_AppendChar( source, ch );
			}
		}
		DString_Append( source, delim );
		DaoLexer_Tokenize( lexer, source->mbs, 0 );
		DArray_Erase( self->tokens, start, 1 );
		DArray_InsertArray( self->tokens, start, tokens, 0, -1 );
		DaoLexer_Delete( lexer );
	}else if( DString_MatchMBS( verbatim, pat, & pstart, & pend ) ){ /* code inlining */
		DaoCodeInliner inliner;
		daoint lb = DString_FindChar( verbatim, '(', 0 );
		if( lb > pend ) lb = MAXSIZE;

		if( lb != MAXSIZE ){
			daoint rb = DString_FindChar( verbatim, ')', 0 );
			DString_SetDataMBS( self->mbs, verbatim->mbs + 2 + wcs, lb - 2 - wcs );
			DString_SetDataMBS( self->mbs2, verbatim->mbs + lb + 1, rb - lb - 1 );
			DString_Trim( self->mbs2 );
		}else{
			daoint rb = DString_FindChar( verbatim, ']', 0 );
			DString_SetDataMBS( self->mbs, verbatim->mbs + 2 + wcs, rb - 2 - wcs );
			DString_Clear( self->mbs2 );
		}
		DString_Trim( self->mbs );
		inliner = DaoNamespace_FindCodeInliner( ns, self->mbs );
		if( inliner == NULL ){
			if( lb != MAXSIZE )
				printf( "inlined code not handled, inliner \"%s\" not found\n", self->mbs->mbs );
			start ++;
			return start;
		}
		if( self->byteBlock ){
			DaoByteBlock_EncodeVerbatim( self->byteBlock, self->mbs, self->mbs2, verbatim, line );
		}
		DString_Clear( self->mbs );
		if( (*inliner)( ns, self->mbs2, verbatim, self->mbs, line ) ){
			DString_InsertMBS( self->mbs, "code inlining failed: ", 0, 0, 0 );
			DaoParser_Error( self, DAO_CTW_INTERNAL, self->mbs );
			return -1;
		}
		DArray_Erase( self->tokens, start, 1 );
		if( self->mbs->size ){
			DaoLexer *lexer = DaoLexer_New();
			DArray *tokens = lexer->tokens;
			DaoLexer_Tokenize( lexer, self->mbs->mbs, 0 );
			DArray_InsertArray( self->tokens, start, tokens, 0, -1 );
			DaoLexer_Delete( lexer );
		}
	}else{
		start ++;
	}
	return start;
}

static int DaoParser_ParseUseStatement( DaoParser *self, int start, int to );

static int DaoParser_ApplyTokenFilters( DaoParser *self, DaoNamespace *nspace )
{
	int i, ret = 1;
	for(i=1; i<nspace->namespaces->size; ++i){
		ret &= DaoParser_ApplyTokenFilters( self, nspace->namespaces->items.pNS[i] );
	}
	for(i=0; i<nspace->tokenFilters->size; ++i){
		DaoTokenFilter filter = (DaoTokenFilter) nspace->tokenFilters->items.pVoid[i];
		ret &= (*filter)( self );
	}
	return ret;
}
static int DaoParser_Preprocess( DaoParser *self )
{
	DaoNamespace *ns = self->nameSpace;
	DaoVmSpace *vmSpace = self->vmSpace;
	DaoToken **tokens = self->tokens->items.pToken;
	int cons = (vmSpace->options & DAO_OPTION_INTERUN) && (ns->options & DAO_NS_AUTO_GLOBAL);
	int i, end, tag = 0;
	int k, right, start = 0;
	unsigned char tki, tki2;

#if 0
	printf("routine = %p\n", self->routine );
	for(i=0; i<self->tokens->size; i++) printf("%s  ", tokens[i]->string->mbs); printf("\n\n");
#endif

	while( start >=0 && start < self->tokens->size ){
		self->curLine = tokens[start]->line;
#if 0
		printf( "start = %i\n", start );
		printf("At tokPos : %i, %s\n", tokens[start]->index, tokens[ start ]->string->mbs );
#endif

		tki = tokens[start]->name;
		tki2 = start+1 < self->tokens->size ? tokens[start+1]->name : 0;
		if( tki == DKEY_LOAD && tki2 != DTOK_LB ){
			/* only for top level "load", for macros in the module  */
			end = DaoParser_ParseLoadStatement( self, start, self->tokens->size-1 );
			if( end < 0 ) return 0;
			if( cons ) DaoParser_MakeCodes( self, start, end, ns->inputs );
			DArray_Erase( self->tokens, start, end-start );
			tokens = self->tokens->items.pToken;
		}else if( tki == DTOK_VERBATIM ){
			start = DaoParser_HandleVerbatim( self, start );
			if( start < 0 ) return 0;
			tokens = self->tokens->items.pToken;
		}else{
			start ++;
		}
	}

	if( DaoParser_ApplyTokenFilters( self, ns ) == 0 ) return 0;

#if 0
	for(i=0; i<self->tokens->size; i++) printf("%s  ", tokens[i]->string.mbs); printf("\n\n");
#endif

	/* Join string literals after handling macro: */
	for(i=0; i<self->tokens->size; i++ ){
		DaoToken *t = self->tokens->items.pToken[i];
		if( (t->name == DTOK_MBS || t->name == DTOK_WCS) && i+1<self->tokens->size ){
			DaoToken *t2 = self->tokens->items.pToken[i+1];
			if( t->name == t2->name ){
				int len = t->string.size;
				DString_Erase( & t->string, len-1, MAXSIZE );
				DString_AppendMBS( & t->string, t2->string.mbs + 1 );
				DArray_Erase( self->tokens, i+1, 1 );
				i --;
			}
		}
	}
	return 1;
}
static void DaoParser_AddToScope( DaoParser *self, DString *name, DaoValue *value, DaoType *abtype, int store, int line )
{
	DaoNamespace *myNS = self->nameSpace;
	DaoRoutine *routine = self->routine;
	int id, perm = self->permission;
	if( routine == myNS->mainRoutine ){
		DaoNamespace_AddConst( myNS, name, value, perm );
		DaoNamespace_AddType( myNS, name, abtype );
	}else if( self->isClassBody && self->hostClass ){
		DaoClass_AddType( self->hostClass, name, abtype );
		DaoClass_AddConst( self->hostClass, name, value, perm );
	}
	id = routine->routConsts->items.size;
	MAP_Insert( routine->body->abstypes, name, abtype );
	MAP_Insert( DaoParser_GetCurrentDataMap( self ), name, LOOKUP_BIND_LC( id ) );
	DaoRoutine_AddConstant( routine, value );
}
static int DaoParser_UseConstructor( DaoParser *self, DaoRoutine *rout, int t1, int t2 )
{
	DaoClass *host = self->hostClass;
	DaoRoutine *classRoutines = host->classRoutines;
	DaoType *hostType = host->objType;
	DString *s1 = DString_Copy( rout->routType->name );
	DString *s2 = DString_New(1);
	int i, k = DString_Find( s1, rout->routType->aux->xType.name, 0 );
	if( k != MAXSIZE ) DString_Erase( s1, k, -1 );
	for(i=0; i<classRoutines->overloads->routines->size; i++){
		DaoRoutine *rt = classRoutines->overloads->routines->items.pRoutine[i];
		DString_Assign( s2, rt->routType->name );
		k = DString_Find( s2, rt->routType->aux->xType.name, 0 );
		if( k != MAXSIZE ) DString_Erase( s2, k, -1 );
		if( DString_EQ( s1, s2 ) ){
			DString_Assign( s1, rout->routType->name );
			if( rt->routHost == hostType ){
				DaoParser_Warn( self, DAO_ROUT_DEFINED_SIGNATURE, s1 );
			}else{
				DaoParser_Warn( self, DAO_ROUT_USED_SIGNATURE, s1 );
			}
			DaoParser_SumTokens( self, s2, t1, t2-1, 1 );
			DaoParser_Warn( self, DAO_INVALID_USE_STMT, s2 );
			DString_Delete( s1 );
			DString_Delete( s2 );
			return 0;
		}
	}
	if( self->byteBlock ){
		DaoByteBlock_EncodeUseStmt( self->byteBlock, (DaoValue*) rout, DAO_ROUTINE );
	}
	DRoutines_Add( host->classRoutines->overloads, rout );
	DString_Assign( s1, host->className );
	DString_AppendChar( s1, ':' );
	DString_Append( s1, rout->routType->name );
	DaoClass_AddOverloadedRoutine( host, s1, rout );
	DString_Delete( s1 );
	DString_Delete( s2 );
	return 1;
}
static DaoParser* DaoParser_NewRoutineParser( DaoParser *self, int start, int attribs );

static int DaoParser_CheckNameToken( DaoParser *self, int start, int to, int ecode, int estart )
{
	DaoToken **tokens = self->tokens->items.pToken;
	if( start >to || tokens[start]->type != DTOK_IDENTIFIER ){
		DaoParser_Error( self, DAO_TOKEN_NEED_NAME, & tokens[start]->string );
		DaoParser_Error2( self, ecode, estart, start, 1 );
		return 0;
	}
	return 1;
}
static int DaoParser_AddConstant( DaoParser *self, DString *name, DaoValue *value, DaoToken *tok )
{
	DaoNamespace *myNS = self->nameSpace;
	DaoRoutine *routine = self->routine;
	int perm = self->permission;
	int line = tok->line;
	if( self->isClassBody ){
		DaoClass_AddConst( self->hostClass, name, value, perm );
	}else if( self->levelBase + self->lexLevel == 0 ){
		DaoNamespace_AddConst( myNS, name, value, perm );
	}else{
		daoint id = routine->routConsts->items.size;
		MAP_Insert( DaoParser_GetCurrentDataMap( self ), name, LOOKUP_BIND_LC(id) );
		DaoRoutine_AddConstant( routine, value );
	}
	/* TODO was defined warning: */
	return 0;
}
static int DaoParser_ParseUseConstructor( DaoParser *self, int start, int to )
{
	DaoType *type;
	DaoRoutine *routine = self->routine;
	DaoRoutine *tmpRoutine;
	DaoParser *tmpParser;
	DaoCtype *ctype = NULL;
	DaoClass *klass = NULL, *host = self->hostClass;
	DaoToken **tokens = self->tokens->items.pToken;
	DString *name = & tokens[start]->string;
	DString *signature = self->mbs;
	int i, right, found = 0;
	int use = start - 2;

	if( self->isClassBody == 0 ) goto InvalidUse;
	if( DaoParser_CheckNameToken( self, start, to, DAO_INVALID_USE_STMT, use ) ==0 ) goto InvalidUse;

	if( host->parent && host->parent->type == DAO_CLASS ){
		klass = (DaoClass*) host->parent;
		if( DString_EQ( host->parent->xClass.className, name ) == 0 ) klass = NULL;
	}else if( host->parent && host->parent->type == DAO_CTYPE ){
		ctype = (DaoCtype*) host->parent;
		if( DString_EQ( host->parent->xCtype.ctype->name, name ) == 0 ) ctype = NULL;
	}
	if( klass == NULL && ctype == NULL ){
		DaoParser_Error( self, DAO_SYMBOL_NEED_CLASS_CTYPE, & tokens[start]->string );
		DaoParser_Error2( self, DAO_INVALID_USE_STMT, use, start, 1 );
		return -1;
	}
	type = klass ? klass->objType : ctype->ctype->kernel->abtype;
	tmpParser = DaoParser_NewRoutineParser( self, start, 0 );
	tmpRoutine = tmpParser->routine;
	GC_ShiftRC( type, tmpRoutine->routHost );
	tmpRoutine->routHost = type;
	tmpParser->hostType = type;
	tmpParser->hostClass = klass;
	right = DaoParser_ParseSignature( self, tmpParser, DKEY_ROUTINE, start );
	if( right < 0 ){
		DaoParser_Error2( self, DAO_INVALID_USE_STMT, use, start, 1 );
		return -1;
	}
	DString_Assign( signature, tmpRoutine->routName );
	DString_AppendChar( signature, ':' );
	DString_Append( signature, tmpRoutine->routType->name );
	DaoVmSpace_ReleaseParser( self->vmSpace, tmpParser );
	DaoRoutine_Delete( tmpRoutine );
	start = right + 1;
	if( klass ){
		if( signature->size ){
			DaoRoutine *rs = DaoClass_GetOverloadedRoutine( klass, signature );
			if( rs ) found |= DaoParser_UseConstructor( self, rs, use, start );
		}else{
			DArray *routTable = klass->classRoutines->overloads->routines;
			for(i=0; i<routTable->size; i++){
				DaoRoutine *rs = routTable->items.pRoutine[i];
				found |= DaoParser_UseConstructor( self, rs, use, start );
			}
		}
	}else if( ctype ){
		DaoRoutine *func = DaoType_FindFunction( ctype->ctype, name );
		if( func == NULL ){
			DaoParser_Error( self, DAO_CONSTR_NOT_DEFINED, name );
			DaoParser_Error2( self, DAO_INVALID_USE_STMT, use, start, 1 );
			return -1;
		}
		if( signature->size ) DString_Erase( signature, 0, name->size + 1 );
		if( func->overloads == NULL ){
			if( signature->size ==0 || DString_EQ( signature, func->routType->name ) ){
				/* printf( "%s\n", func->routType->name->mbs ); */
				found |= DaoParser_UseConstructor( self, func, use, start );
			}
		}else{
			for(i=0; i<func->overloads->routines->size; i++){
				DaoRoutine *rs = func->overloads->routines->items.pRoutine[i];
				if( signature->size ==0 || DString_EQ( signature, rs->routType->name ) ){
					/* printf( "%s\n", rs->routType->name->mbs ); */
					found |= DaoParser_UseConstructor( self, rs, use, start );
					if( signature->size ) break;
				}
			}
		}
		if( found == 0 ){
			DaoParser_Error2( self, DAO_ROUT_WRONG_SIGNATURE, use+1, start-1, 1 );
			DaoParser_Error2( self, DAO_INVALID_USE_STMT, use, start-1, 1 );
			return -1;
		}
	}
	return start;
InvalidUse:
	DaoParser_Error3( self, DAO_INVALID_USE_STMT, use );
	return -1;
}
static int DaoParser_ParseUseStatement( DaoParser *self, int start, int to )
{
	DaoValue *scope = NULL;
	DaoValue *value = NULL;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoNamespace *myNS = self->nameSpace;
	DaoType *type;
	DString *str;
	DNode *node;
	int estart = start;
	int use = start;
	start ++;
	if( DaoParser_CheckNameToken( self, start, to, DAO_INVALID_USE_STMT, use ) ==0 ) return -1;
	if( tokens[start]->name == DKEY_ROUTINE ){
		return DaoParser_ParseUseConstructor( self, start+1, to );
	}else if( tokens[start]->name == DKEY_ENUM ){
		start = DaoParser_FindScopedConstant( self, & value, start + 1 );
		if( start > 0 && value && value->type == DAO_TYPE && value->xType.tid == DAO_ENUM ){
			DaoValue item = {DAO_INTEGER};
			type = (DaoType*) value;
			if( type->mapNames == NULL ) goto InvalidUse;
			for(node=DMap_First(type->mapNames);node;node=DMap_Next(type->mapNames,node)){
				item.xInteger.value = node->value.pInt;
				DaoParser_AddConstant( self, node->key.pString, & item, tokens[start] );
			}
			if( self->byteBlock ){
				DaoByteBlock_EncodeUseStmt( self->byteBlock, value, DAO_ENUM );
			}
			return start + 1;
		}
	}else{
		start = DaoParser_FindScopedConstant( self, & value, start );
		if( start > 0 && value && value->type == DAO_NAMESPACE ){
			DaoNamespace_AddParent( myNS, (DaoNamespace*) value );
			if( self->byteBlock ){
				DaoByteBlock_EncodeUseStmt( self->byteBlock, value, DAO_NAMESPACE );
			}
			return start + 1;
		}
	}
InvalidUse:
	DaoParser_Error3( self, DAO_INVALID_USE_STMT, estart );
	return -1;
}
static int DaoParser_ParseTypeAliasing( DaoParser *self, int start, int to )
{
	DaoToken *nameTok, **tokens = self->tokens->items.pToken;
	DaoNamespace *myNS = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DaoRoutine *tmpRoutine;
	DaoParser *tmpParser;
	DaoType *type, *old;
	DString *str;
	DNode *node;
	int estart = start;
	int use = start;
	start ++;
	if( DaoParser_CheckNameToken( self, start, to, DAO_INVALID_USE_STMT, use ) ==0 ) return -1;
	nameTok = tokens[start];
	str = & nameTok->string;
	start += 2;
	if( start >to || tokens[start]->type != DTOK_IDENTIFIER ) goto InvalidAliasing;
	if( DaoType_FindType( str, myNS, self->hostCdata, self->hostClass, routine ) ){
		/* Redundant ??? */
		DaoParser_Error( self, DAO_SYMBOL_WAS_DEFINED, str );
		goto InvalidAliasing;
	}
	if( DaoParser_GetRegister( self, nameTok ) >= 0 ){
		DaoParser_Error( self, DAO_SYMBOL_WAS_DEFINED, str );
		goto InvalidAliasing;
	}
	type = old = DaoParser_ParseType( self, start, to, & start, NULL );
	if( type == NULL ) goto InvalidAliasing;
	type = DaoType_Copy( type );
	DString_Assign( type->name, str );
	/*  XXX typedef in routine or class */
	DaoNamespace_AddType( myNS, str, type );
	DaoNamespace_AddTypeConstant( myNS, str, type );
	if( self->byteBlock ){
		DaoByteBlock_EncodeTypeAlias( self->byteBlock, old, type, str );
	}
	return start;
InvalidAliasing:
	DaoParser_Error3( self, DAO_INVALID_TYPE_ALIAS, estart );
	return -1;
}
#ifdef DAO_WITH_DECORATOR
static void DaoParser_DecorateRoutine( DaoParser *self, DaoRoutine *rout )
{
	DaoValue *selfpar = NULL;
	DaoValue *params[DAO_MAX_PARAM+1];
	DaoObject object, *obj = & object;
	int i, j, n, count = self->decoFuncs->size;

	if( self->byteBlock && count ){
		DaoByteBlock_EncodeDecorators( self->byteBlock, self->decoFuncs, self->decoParams );
		return;
	}

	if( rout->routHost ){
		/* To circumvent the default object issue for type matching: */
		object = *(DaoObject*) rout->routHost->value;
		selfpar = (DaoValue*) obj;
	}
	params[0] = (DaoValue*) rout;
	for(i=0; i<count; i++){
		DaoRoutine *decoFunc = self->decoFuncs->items.pRoutine[i];
		DaoList *decoParam = self->decoParams->items.pList[i];
		n = decoParam->items.size;
		for(j=0; j<n; j++) params[j+1] = decoParam->items.items.pValue[j];
		decoFunc = DaoRoutine_Resolve( decoFunc, selfpar, params, n+1 );
		if( decoFunc == NULL || DaoRoutine_Decorate( rout, decoFunc, params, n+1, 1 ) == NULL ){
			DaoParser_Error( self, DAO_INVALID_FUNCTION_DECORATION, rout->routName );
			return;
		}
	}
}
#endif
static DaoParser* DaoParser_NewRoutineParser( DaoParser *self, int start, int attribs )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoRoutine *rout = NULL;
	DaoParser *parser;
	if( self->isInterBody ){
		rout = DaoRoutine_New( self->nameSpace, self->hostInter->abtype, 0 );
	}else if( self->isClassBody ){
		rout = DaoRoutine_New( self->nameSpace, self->hostClass->objType, 1 );
		rout->attribs |= attribs;
	}else{
		rout = DaoRoutine_New( self->nameSpace, NULL, 1 );
	}
	rout->defLine = tokens[start]->line;
	parser = DaoVmSpace_AcquireParser( self->vmSpace );
	parser->routine = rout;
	parser->vmSpace = self->vmSpace;
	parser->hostType = self->hostType;
	parser->hostClass = self->hostClass;
	parser->hostInter = self->hostInter;
	parser->levelBase = self->levelBase + self->lexLevel + 1;
	parser->defParser = self;
	DArray_Assign( parser->decoFuncs, self->decoFuncs2 );
	DArray_Assign( parser->decoParams, self->decoParams2 );
	return parser;
}
static int DaoParser_ParseRoutineDefinition( DaoParser *self, int start, int from, int to, int store )
{
	DaoToken *ptok, **tokens = self->tokens->items.pToken;
	DaoNamespace *myNS = self->nameSpace;
	DaoRoutine *rout = NULL;
	DaoParser *parser = NULL;
	DaoParser *tmpParser = NULL;
	DaoRoutine *tmpRoutine = NULL;
	DaoValue *value = NULL, *scope = NULL;
	DaoClass *klass = NULL;
	DString *mbs = self->mbs;
	DString *mbs2 = self->mbs2;
	int perm = self->permission;
	int tki = tokens[start]->name;
	int k, right, virt = 0, stat = 0;
	int errorStart = start;
	if( start > from ){
		int ttkk = tokens[start-1]->name;
		if( ttkk == DKEY_STATIC ) stat = DAO_ROUT_STATIC;
	}
	if( start > from + 1 ){
		int ttkk = tokens[start-2]->name;
		if( virt && ttkk == DKEY_STATIC ) stat = DAO_ROUT_STATIC;
	}
	right = -1;
	if( start+2 <= to && ((k=tokens[start+2]->name) == DTOK_COLON2 || k == DTOK_LT) ){
		/* For functions define outside the class body: */
		int oldpos = start + 1;
		int r1 = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 ); /* parameter list. */
		if( r1 < 0 || r1+1 >= self->tokens->size ) goto InvalidDefinition;

		start = DaoParser_FindScopedConstant2( self, & value, start+1, DTOK_LB, 0 );
		if( start < 0 || value == NULL ){
			ptok = tokens[ oldpos ];
			self->curLine = ptok->line;
			if( start >=0 ) DaoParser_Error( self, DAO_SYMBOL_NEED_CLASS, & ptok->string );
			goto InvalidDefinition;
		}else if( value->type == DAO_CLASS ){
			value = (DaoValue*) value->xClass.classRoutine;
		}else if( value->type != DAO_ROUTINE ){
			goto InvalidDefinition;
		//}else if( value->xRoutine.body || value->xRoutine.pFunc ){
		//	goto InvalidDefinition;
		}else if( value->xRoutine.routHost == NULL ){
			goto InvalidDefinition;
		}
		// XXX: improve;
		klass = (DaoClass*) value->xRoutine.routHost->aux;
		tmpParser = DaoParser_NewRoutineParser( self, start, virt | stat );
		tmpRoutine = tmpParser->routine;
		GC_ShiftRC( klass->objType, tmpRoutine->routHost );
		tmpRoutine->attribs |= stat;
		tmpRoutine->routHost = klass->objType;
		tmpParser->hostType = klass->objType;
		tmpParser->hostClass = klass;
		right = DaoParser_ParseSignature( self, tmpParser, tki, start );
		if( right < 0 ) goto InvalidDefinition;
		DString_Assign( mbs2, tmpRoutine->routName );
		DString_Assign( mbs, tmpRoutine->routName );
		DString_AppendChar( mbs, ':' );
		DString_Append( mbs, tmpRoutine->routType->name );
		rout = DaoClass_GetOverloadedRoutine( klass, mbs );
		if( ! rout ){
			DaoNamespace *nsdef = NULL;
			DMap *hash = klass->ovldRoutMap;
			DNode *it = DMap_First( hash );
			int defined = 0;
			for(;it;it=DMap_Next(hash,it)){
				DaoRoutine *meth = (DaoRoutine*) it->value.pValue;
				if( DString_EQ( meth->routName, mbs2 ) && meth->body ){
					if( meth->body->codeStart ==0 ){
						nsdef = meth->nameSpace;
						self->curLine = meth->defLine;
						DaoParser_Error( self, DAO_ROUT_DECLARED_SIGNATURE, it->key.pString );
					}
					defined = 1;
				}
			}
			self->curLine = tokens[ start+1 ]->line;
			if( defined ){
				if( nsdef && nsdef != myNS ) DaoParser_Error( self, 0, nsdef->name );
				DaoParser_Error( self, DAO_ROUT_WRONG_SIGNATURE, mbs );
			}else{
				DaoParser_Error2( self, DAO_ROUT_NOT_DECLARED, errorStart+1, r1, 0 );
			}
			goto InvalidDefinition;
		}else if( rout->body->codeStart > 0 ){
			self->curLine = rout->defLine;
			DaoParser_Error2( self, DAO_ROUT_WAS_IMPLEMENTED, errorStart+1, r1, 0 );
			if( rout->nameSpace != myNS ) DaoParser_Error( self, 0, rout->nameSpace->name );
			self->curLine = tokens[ start+1 ]->line;
			DaoParser_Error2( self, DAO_ROUT_REDUNDANT_IMPLEMENTATION, errorStart+1, r1, 0 );
			goto InvalidDefinition;
		}
		parser = tmpParser;
		parser->routine = rout;
		DaoRoutine_Delete( tmpRoutine );
	}else if( start < to ){
		rout = NULL;
		right = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
		start ++;
		tmpParser = DaoParser_NewRoutineParser( self, start, virt | stat );
		tmpRoutine = tmpParser->routine;
		right = DaoParser_ParseSignature( self, tmpParser, tki, start );
		if( right < 0 ) goto InvalidDefinition;
		DString_Assign( mbs, tmpRoutine->routName );
		DString_AppendChar( mbs, ':' );
		DString_Append( mbs, tmpRoutine->routType->name );
		if( self->isClassBody ){
			klass = self->hostClass;
			rout = DaoClass_GetOverloadedRoutine( klass, mbs );
			if( rout && rout->body == NULL ) rout = NULL;
			if( rout && rout->routHost != klass->objType ) rout = NULL;
		}else if( self->isInterBody == 0 ){
			int id = DaoNamespace_FindConst( myNS, tmpRoutine->routName );
			DaoRoutine *declared = (DaoRoutine*) DaoNamespace_GetConst( myNS, id );
			DaoType *routype2, *routype = tmpRoutine->routType;
			if( declared && declared->type != DAO_ROUTINE ) declared = NULL;
			if( declared && declared->overloads ){
				for(id=0; id<declared->overloads->routines->size; ++id){
					DaoRoutine *R = declared->overloads->routines->items.pRoutine[id];
					if( DaoType_MatchTo( routype, R->routType, NULL ) == DAO_MT_EQ ){
						rout = R;
						break;
					}
				}
			}else if( declared ){
				routype2 = declared->routType;
				if( DaoType_MatchTo( routype, routype2, NULL ) == DAO_MT_EQ ) rout = declared;
			}
		}

		if( rout == NULL ){
			rout = tmpRoutine;
			parser = tmpParser;
			if( STRCMP( rout->routName, "main" ) ==0 ) rout->attribs |= DAO_ROUT_MAIN;
		}else{
			parser = tmpParser;
			parser->routine = rout;
			DaoRoutine_Delete( tmpRoutine );
		}

		value = (DaoValue*) rout;
		switch( perm ){
		case DAO_DATA_PRIVATE   : rout->attribs |= DAO_ROUT_PRIVATE; break;
		case DAO_DATA_PROTECTED : rout->attribs |= DAO_ROUT_PROTECTED; break;
		}
		if( self->isClassBody ){
			DaoClass_AddOverloadedRoutine( klass, mbs, rout );
			if( rout->attribs & DAO_ROUT_INITOR ){ /* overloading constructor */
				/* DRoutines_Add( klass->classRoutines->overloads, rout ); */
				DaoClass_AddConst( klass, klass->classRoutine->routName, value, perm );
			}else{
				DaoClass_AddConst( klass, rout->routName, value, perm );
			}
		}else if( self->isInterBody ){
			GC_ShiftRC( self->hostInter->abtype, rout->routHost );
			parser->hostInter = self->hostInter;
			rout->routHost = self->hostInter->abtype;
			DaoMethods_Insert( self->hostInter->methods, rout, myNS, rout->routHost );
		}else if( rout == tmpRoutine ){
			DaoNamespace_AddConst( myNS, rout->routName, value, perm );
		}
	}
	if( (virt | stat) && rout->routHost == NULL ){
		int efrom = errorStart - (virt != 0) - (stat != 0);
		DaoParser_Error2( self, DAO_INVALID_STORAGE, efrom, errorStart+1, 0 );
		goto InvalidDefinition;
	}
	if( self->byteBlock ){
		parser->byteCoder = self->byteCoder;
		parser->byteBlock = DaoByteBlock_AddRoutineBlock( self->byteBlock, parser->routine, perm );
	}
	if( self->isClassBody && tokens[right]->name == DTOK_RCB ){
		DArray_Append( self->routCompilable, parser );
		return right+1;
	}
	if( tokens[right]->name == DTOK_RCB ){ /* with body */
		if( rout->body == NULL ){
			DaoParser_Error2( self, DAO_ROUT_REDUNDANT_IMPLEMENTATION, errorStart+1, k, 0 );
			goto InvalidDefinition;
		}
		if( DaoParser_ParseRoutine( parser ) == 0 ) goto Failed;
		if( parser->usingGlobal ) DArray_Append( self->routReInferable, parser->routine );
	}
	if( parser ) DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	return right+1;
InvalidDefinition:
	DaoParser_Error3( self, DAO_INVALID_FUNCTION_DEFINITION, errorStart );
Failed:
	if( parser ) DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	return -1;
}
static int DaoParser_ParseCodes( DaoParser *self, int from, int to );
static int DaoParser_ParseInterfaceDefinition( DaoParser *self, int start, int to, int storeType )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoRoutine *routine = self->routine;
	DaoNamespace *myNS = self->nameSpace;
	DaoNamespace *ns = NULL;
	DaoInterface *inter = NULL;
	DaoParser *parser = NULL;
	DaoValue *value = NULL;
	DaoToken *tokName;
	DString *interName;
	DString *ename = NULL;
	int i, right, ec = 0, errorStart = start;
	parser = NULL;
	if( start+1 > to ) goto ErrorInterfaceDefinition;
	tokName = tokens[start+1];
	interName = ename = & tokName->string;
	start = DaoParser_ParseScopedName( self, & value, start + 1 );
	if( start <0 ) goto ErrorInterfaceDefinition;
	if( value == NULL || value->type == 0 ){
		int line = tokens[start]->line;
		int t = tokens[start]->name;
		if( (t != DTOK_IDENTIFIER && t < DKEY_ABS) || t > DKEY_TANH ) goto ErrorInterfaceDefinition;
		interName = & tokens[start]->string;
		inter = DaoInterface_New( myNS, interName->mbs );
		if( routine != myNS->mainRoutine ) ns = NULL;
		value = (DaoValue*) inter;
		DaoParser_AddToScope( self, interName, value, inter->abtype, storeType, line );

		if( start+1 <= to && tokens[start+1]->name == DTOK_SEMCO ){
			if( self->byteBlock ){
				DaoByteBlock_AddInterfaceBlock( self->byteBlock, inter, self->permission );
			}
			start += 2;
			return start;
		}
	}else if( value->type != DAO_INTERFACE ){
		ec = DAO_SYMBOL_WAS_DEFINED;
		goto ErrorInterfaceDefinition;
	}else if( value->xInterface.derived ){
		ec = DAO_SYMBOL_WAS_DEFINED;
		goto ErrorInterfaceDefinition;
	}else{
		inter = & value->xInterface;
	}
	start += 1; /* token after class name. */
	if( tokens[start]->name == DTOK_COLON ){
		/* interface AB : NS::BB, CC{ } */
		unsigned char sep = DTOK_COLON;
		while( tokens[start]->name == sep ){
			ename = & tokens[start+1]->string;
			start = DaoParser_FindScopedConstant( self, & value, start+1 );
			if( start < 0 ) goto ErrorInterfaceBase;
			ename = & tokens[start]->string;
			start ++;
			if( value == NULL || value->type != DAO_INTERFACE ){
				ec = DAO_SYMBOL_NEED_INTERFACE;
				goto ErrorInterfaceBase;
			}
			/* Add a reference to its super interfaces: */
			DArray_Append( inter->supers, value );
			sep = DTOK_COMMA;
		}
	}
	DaoNamespace_InitConstEvalData( myNS );
	parser = DaoVmSpace_AcquireParser( self->vmSpace );
	parser->routine = myNS->constEvalRoutine;
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

	if( self->byteBlock ){
		parser->byteBlock = DaoByteBlock_AddInterfaceBlock( self->byteBlock, inter, self->permission );
		parser->byteCoder = self->byteCoder;
	}

	DaoInterface_DeriveMethods( inter );
	for(i=start+1; i<right; i++) DaoLexer_AppendToken( parser->lexer, tokens[i] );
	parser->defined = 1;

	if( DaoParser_ParseCodes( parser, 0, parser->tokens->size-1 )==0 ){
		if( DString_EQ( self->fileName, parser->fileName ) )
			DArray_InsertArray( self->errors, self->errors->size, parser->errors, 0, -1 );
		else
			DaoParser_PrintError( parser, 0, 0, NULL );
		goto ErrorInterfaceDefinition;
	}
	if( parser->vmcLast != parser->vmcBase ){
		DArray_InsertArray( self->errors, self->errors->size, parser->errors, 0, -1 );
		DaoParser_StatementError( self, parser, DAO_STATEMENT_IN_INTERFACE );
		goto ErrorInterfaceDefinition;
	}
	DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	return right + 1;
ErrorInterfaceBase:
	DaoParser_Error( self, DAO_SYMBOL_NEED_INTERFACE, ename );
ErrorInterfaceDefinition:
	if( parser ) DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	if( ec ) DaoParser_Error( self, ec, ename );
	DaoParser_Error2( self, DAO_INVALID_INTERFACE_DEFINITION, errorStart, to, 0 );
	return -1;
}
static int DaoParser_CompileRoutines( DaoParser *self )
{
	daoint i, error = 0;
	for(i=0; i<self->routCompilable->size; i++){
		DaoParser *parser = (DaoParser*) self->routCompilable->items.pValue[i];
		DaoRoutine *rout = parser->routine;
		if( self->byteBlock ){
			parser->byteCoder = self->byteCoder;
			parser->byteBlock = DaoByteBlock_AddRoutineBlock( self->byteBlock, parser->routine, 0 );
		}
		error |= DaoParser_ParseRoutine( parser ) == 0;
		if( parser->usingGlobal ) DArray_Append( self->routReInferable, rout );
		DaoVmSpace_ReleaseParser( self->vmSpace, parser );
		if( error ) break;
	}
	self->routCompilable->size = 0;
	return error == 0;
}
int DArray_MatchAffix( DArray *self, DString *name );
int DaoClass_UseMixinDecorators( DaoClass *self );
static int DaoParser_ParseClassDefinition( DaoParser *self, int start, int to, int storeType )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoNamespace *myNS = self->nameSpace;
	DaoNamespace *ns = NULL;
	DaoRoutine *routine = self->routine;
	DaoRoutine *rout = NULL;
	DaoParser *parser = NULL;
	DaoClass *klass = NULL;
	DaoValue *value = NULL;
	DaoToken *tokName;
	DString *str, *mbs = DaoParser_GetString( self );
	DString *className, *ename = NULL;
	DArray *holders, *defaults;
	daoint begin, line = self->curLine;
	daoint i, k, rb, right, error = 0;
	int errorStart = start;
	int pm1, pm2, ec = 0;

	if( start+1 > to ) goto ErrorClassDefinition;
	tokName = tokens[start+1];
	className = ename = & tokName->string;
	start = DaoParser_ParseScopedName( self, & value, start+1 );
	if( start <0 ) goto ErrorClassDefinition;
	ename = & tokens[start]->string;
	if( value == NULL || value->type == 0 ){
		DString *name = & tokens[start]->string;
		int line = tokens[start]->line;
		int t = tokens[start]->name;
		if( t != DTOK_IDENTIFIER && t != DTOK_ID_THTYPE && t < DKEY_ABS ) goto ErrorClassDefinition;
		klass = DaoClass_New();

		className = klass->className;
		DString_Assign( className, name );
		DaoClass_SetName( klass, className, myNS );

		DArray_Append( myNS->definedRoutines, klass->classRoutine );
		if( routine != myNS->mainRoutine ) ns = NULL;
		value = (DaoValue*) klass;
		DaoParser_AddToScope( self, className, value, klass->objType, storeType, line );

		if( self->byteBlock ){
			int pm = self->permission;
			DaoByteBlock *block = self->byteBlock;
			DaoByteBlock *classbk = DaoByteBlock_AddClassBlock( block, klass, pm );
			DaoByteBlock *interbk = DaoByteBlock_AddInterfaceBlock( block, klass->inter, pm );
			DaoByteBlock_InsertBlockIndex( interbk, interbk->begin, classbk );
		}

		if( start+1 <= to ){
			int tkn = tokens[start+1]->name;
			if( tkn != DTOK_LB && tkn != DTOK_COLON && tkn != DTOK_LCB && tkn != DKEY_FOR ){
				start += 1;
				return start;
			}
		}else{
			return start;
		}
	}else if( value->type != DAO_CLASS ){
		ec = DAO_SYMBOL_WAS_DEFINED;
		goto ErrorClassDefinition;
	}else if( value->xClass.derived ){
		klass = & value->xClass;
		ec = DAO_SYMBOL_WAS_DEFINED;
		goto ErrorClassDefinition;
	}else{
		klass = & value->xClass;
	}

	rout = klass->classRoutine;
	rout->defLine = tokens[start]->line;
	GC_ShiftRC( myNS, rout->nameSpace );
	rout->nameSpace = myNS;

	parser = DaoVmSpace_AcquireParser( self->vmSpace );
	parser->vmSpace = self->vmSpace;
	parser->routine = rout;
	parser->isClassBody = 1;
	parser->hostClass = klass;
	parser->nameSpace = myNS;
	parser->levelBase = self->levelBase + self->lexLevel + 1;
	parser->hostType = klass->objType;

	DString_Assign( parser->fileName, self->fileName );

	start ++; /* token after class name. */
	if( start > to ) goto ErrorClassDefinition;

	/* Apply aspects (to normal classes only): */
	if( klass->className->mbs[0] != '@' ){
		DNode *it;
		for(it=DMap_First(myNS->lookupTable); it; it=DMap_Next(myNS->lookupTable,it)){
			int id = LOOKUP_ID( it->value.pInt );
			DaoClass *mixin = (DaoClass*) myNS->constants->items.pConst[id]->value;
			if( LOOKUP_ST( it->value.pInt ) != DAO_GLOBAL_CONSTANT ) continue;
			if( LOOKUP_UP( it->value.pInt ) > 1 ) continue; /* skip indirectly loaded; */
			if( mixin->type != DAO_CLASS ) continue;
			if( mixin->parent != NULL ) continue;
			if( mixin->className->mbs[0] != '@' ) continue; /* Not an aspect class; */
			if( DArray_MatchAffix( mixin->decoTargets, klass->className ) == 0 ) continue;
			DaoClass_AddMixinClass( klass, mixin );
		}
	}

	if( start <= to && tokens[start]->name == DTOK_LB ){
		int rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
		unsigned char sep = DTOK_LB;
		while( tokens[start]->name == sep ){
			DaoClass *mixin;
			start = DaoParser_FindScopedConstant( self, & value, start+1 );
			if( start <0 ) goto ErrorClassDefinition;
			ename = & tokens[start]->string;
			if( value == NULL || value->type != DAO_CLASS ){
				ec = DAO_SYMBOL_NEED_CLASS;
				if( value == NULL || value->type == 0 || value->type == DAO_STRING )
					ec = DAO_SYMBOL_POSSIBLY_UNDEFINED;
				goto ErrorClassDefinition;
			}
			mixin = (DaoClass*) value;
			if( mixin->parent != NULL ){
				/* Class with parent classes cannot be used as mixin: */
				ec = DAO_INVALID_MIXIN_CLASS;
				goto ErrorClassDefinition;
			}
			DaoClass_AddMixinClass( klass, mixin );
			sep = DTOK_COMMA;
			start ++;
		}
		start += 1;
	}

	if( tokens[start]->name == DTOK_COLON ){
		/* class AA : NS::BB { } */
		DaoClass *super = NULL;
		start = DaoParser_FindScopedConstant( self, & value, start+1 );
		if( start <0 ) goto ErrorClassDefinition;
		ename = & tokens[start]->string;
		if( value == NULL || (value->type != DAO_CLASS && value->type != DAO_CTYPE) ){
			ec = DAO_SYMBOL_NEED_CLASS_CTYPE;
			if( value == NULL || value->type == 0 || value->type == DAO_STRING )
				ec = DAO_SYMBOL_POSSIBLY_UNDEFINED;
			goto ErrorClassDefinition;
		}
		super = & value->xClass;
		start ++;

		if( tokens[start]->name == DTOK_LB ) goto ErrorClassDefinition; /* old syntax; */
		if( tokens[start]->name == DTOK_COMMA ) goto ErrorClassDefinition; /* old syntax; */
		if( super == NULL ){
			ec = DAO_SYMBOL_POSSIBLY_UNDEFINED;
			goto ErrorClassDefinition;
		}
		/* Add a reference to its super classes: */
		DaoClass_AddSuperClass( klass, (DaoValue*) super );
	}/* end parsing super classes */
	if( tokens[start]->name == DKEY_FOR ){
		ec = DAO_INVALID_DECO_PATTERN;
		if( klass->className->mbs[0] != '@' ) goto ErrorClassDefinition;
		if( klass->decoTargets == NULL ) klass->decoTargets = DArray_New(D_STRING);
		start = DaoParser_ParseDecoTargets( self, start+1, to, klass->decoTargets );
		if( start < 0 ) goto ErrorClassDefinition;
		ec = 0;
	}
	if( self->byteBlock ){
		parser->byteBlock = DaoByteBlock_AddClassBlock( self->byteBlock, klass, self->permission );
		parser->byteCoder = self->byteCoder;
	}
	begin = start;
	right = tokens[start]->name == DTOK_LCB ?
		DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, -1 ) : -1 ;

	if( right < 0 ) goto ErrorClassDefinition;

	parser->defined = 1;
	if( DaoClass_DeriveClassData( klass ) == 0 ) goto ErrorClassDefinition;
	DaoClass_DeriveObjectData( klass );

	for(i=begin+1; i<right; i++) DaoLexer_AppendToken( parser->lexer, tokens[i] );
	if( DaoParser_ParseCodes( parser, 0, parser->tokens->size-1 )==0 ){
		if( DString_EQ( self->fileName, parser->fileName ) )
			DArray_InsertArray( self->errors, self->errors->size, parser->errors, 0, -1 );
		else
			DaoParser_PrintError( parser, 0, 0, NULL );
		goto ErrorClassDefinition;
	}
	DaoList_Clear( klass->classRoutine->routConsts );
	if( parser->vmcLast != parser->vmcBase ){
#if 0
		DaoParser_PrintCodes( parser );
#endif
		DArray_InsertArray( self->errors, self->errors->size, parser->errors, 0, -1 );
		DaoParser_StatementError( self, parser, DAO_STATEMENT_IN_CLASS );
		goto ErrorClassDefinition;
	}
	for(i=0; i<klass->cstDataName->size; i++){
		DNode *it1, *it2;
		value = klass->constants->items.pConst[i]->value;
		if( value == NULL || value->type != DAO_ROUTINE ) continue;
		if( value->xRoutine.routName->mbs[0] != '.' ) continue;
		DString_SetMBS( mbs, value->xRoutine.routName->mbs + 1 );
		if( mbs->mbs[ mbs->size - 1 ] == '=' ) DString_Erase( mbs, mbs->size - 1, 1 );
		it1 = MAP_Find( klass->lookupTable, value->xRoutine.routName );
		it2 = MAP_Find( klass->lookupTable, mbs );
		if( it1 == NULL || it2 == NULL ) continue;
		pm1 = LOOKUP_PM( it1->value.pInt );
		pm2 = LOOKUP_PM( it2->value.pInt );
		if( pm1 <= pm2 || pm2 != DAO_DATA_PRIVATE ){
			self->curLine = value->xRoutine.defLine;
			DaoParser_Warn( self, DAO_WARN_GET_SETTER, mbs );
		}
	}
	DaoClass_ResetAttributes( klass );
	if( parser->byteBlock ){
		DaoByteCoder_EncodeUInt32( parser->byteBlock->begin+4, klass->attribs );
	}
	error = DaoParser_CompileRoutines( parser ) == 0;
	if( error == 0 && klass->classRoutines->overloads->routines->size == 0 ){
		DString *ctorname = klass->cstDataName->items.pString[DAO_CLASS_CONST_CSTOR];
		DArray_Clear( parser->tokens );
		DaoParser_AddDefaultInitializer( parser, klass, 0 );
		error |= DaoParser_ParseRoutine( parser ) == 0;
		DaoClass_AddConst( klass, ctorname, (DaoValue*)klass->classRoutine, DAO_DATA_PUBLIC );
		if( parser->byteBlock ){
			DaoByteBlock *bk = parser->byteBlock;
			bk = DaoByteBlock_AddRoutineBlock( bk, klass->classRoutine, DAO_DATA_PUBLIC );
			DaoByteCoder_FinalizeRoutineBlock( self->byteCoder, bk );
		}
	}
	DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	DaoClass_UpdateMixinConstructors( klass );
	if( error ) return -1;

	if( self->byteBlock == NULL ){
		if( DaoClass_UseMixinDecorators( klass ) == 0 ) goto ErrorClassDefinition;
	}

	return right + 1;
ErrorClassDefinition:
	if( parser ) DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	if( ec ) DaoParser_Error( self, ec, ename );
	ec = DAO_INVALID_CLASS_DEFINITION;
	DaoParser_Error2( self, ec, errorStart, to, 0 );
	return -1;
}
static int DaoParser_ParseEnumDefinition( DaoParser *self, int start, int to, int storeType )
{
	DaoToken *ptok, **tokens = self->tokens->items.pToken;
	DaoType *abtp, *abtp2;
	DString *str, *alias = NULL;
	DaoValue *dv = NULL;
	int sep = DTOK_COMMA, value = 0;
	int id, rb, comma, semco, explicitdef=0;
	int enumkey = start + 1;
	int reg, cst = 0;
	char buf[32];
	rb = -1;
	abtp = NULL;
	rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start+1, -1 );
	if( tokens[start+2]->type != DTOK_LCB || rb <0 ) goto ErrorEnumDefinition;
	ptok = tokens[start+1];
	if( ptok->type != DTOK_IDENTIFIER ) goto ErrorEnumDefinition;
	alias = & ptok->string;
	if( ptok->name != DTOK_ID_SYMBOL && ptok->name != DTOK_ID_THTYPE ) start += 1;
	if( (id = DaoParser_GetRegister( self, ptok )) >=0 ){
		DaoParser_Error( self, DAO_SYMBOL_WAS_DEFINED, alias );
		goto ErrorEnumDefinition;
	}
	if( DaoNamespace_FindType( self->nameSpace, alias) ){
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
			DaoParser_Error( self, DAO_TOKEN_NEED_NAME, & tokens[start]->string );
			goto ErrorEnumDefinition;
		}
		str = & tokens[start]->string;
		explicitdef = 0;
		if( tokens[start+1]->type == DTOK_ASSN ){
			explicitdef = 1;
			reg = DaoParser_MakeArithTree( self, start+2, comma-1, & cst );
			if( reg < 0 ) goto ErrorEnumDefinition;
			dv = NULL;
			if( cst ) dv = DaoParser_GetVariable( self, cst );
			if( dv == NULL || dv->type < DAO_INTEGER || dv->type > DAO_DOUBLE ){
				DaoParser_Error( self, DAO_EXPR_NEED_CONST_NUMBER, & tokens[start+2]->string );
				goto ErrorEnumDefinition;
			}
			value = DaoValue_GetInteger( dv );
		}else if( start+1 != rb && tokens[start+1]->type != sep ){
			DaoParser_Error( self, DAO_TOKEN_NOT_EXPECTED, & tokens[start+1]->string );
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
		if( explicitdef ){
			sprintf( buf, "=%i", value );
			DString_AppendMBS( abtp->name, buf );
		}
		DMap_Insert( abtp->mapNames, str, (void*)(daoint)value );
		if( sep == DTOK_SEMCO ) value <<= 1; else value += 1;
		if( comma == rb ) break;
		start = comma + 1;
		comma = DaoParser_FindOpenToken( self, sep, comma+1, -1, 0 );
		if( comma <0 ) comma = rb;
	}
	DString_AppendChar( abtp->name, '>' );
	abtp2 = DaoNamespace_FindType( self->nameSpace, abtp->name );
	if( abtp2 ){
		DaoType_Delete( abtp );
	}else{
		DaoNamespace_AddType( self->nameSpace, abtp->name, abtp );
		abtp2 = abtp;
	}
	if( alias ){
		DaoType *old = abtp2;
		abtp2 = DaoType_Copy( abtp2 );
		DString_Assign( abtp2->name, alias );
		DaoNamespace_AddType( self->nameSpace, abtp2->name, abtp2 );
		DaoParser_AddConstant( self, abtp2->name, (DaoValue*) abtp2, tokens[enumkey] );
		if( self->byteBlock ){
			DaoByteBlock_EncodeTypeAlias( self->byteBlock, old, abtp2, alias );
		}
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
static int DaoParser_CheckDefault( DaoParser *self, DaoType *type, int estart )
{
	DaoValue *value = type->value;
	int mt = 0;
	if( value ){
		if( value->type == DAO_CTYPE || value->type == DAO_CDATA || value->type == DAO_CSTRUCT ){
			mt = DaoType_MatchTo( value->xCdata.ctype, type, NULL );
		}else if( value->type == 0 ){
			mt = 1;
		}else{
			mt = DaoType_MatchValue( type, value, NULL );
		}
	}
	if( mt == 0 ) DaoParser_Error3( self, DAO_TYPE_NO_DEFAULT, estart );
	return mt;
}
static void DaoParser_CheckStatementSeparation( DaoParser *self, int check, int end )
{
	DaoInode *close = (DaoInode*) DArray_Back( self->scopeClosings );
	DaoToken **tokens = self->tokens->items.pToken;
	if( check >= end ) return;
	self->curLine = tokens[check]->line;
	if( tokens[check]->line != tokens[check+1]->line ) return;
	switch( tokens[check+1]->name ){
	case DTOK_RCB : case DTOK_SEMCO : case DKEY_ELSE : break;
	case DKEY_WHILE : if( close && close->a == DKEY_WHILE ) break; /* else fall through; */
	default : DaoParser_Warn( self, DAO_WARN_STATEMENT_SEPERATION, NULL ); break;
	}
}
static int DaoParser_ParseVarExpressions( DaoParser *self, int start, int to, int var, int st, int st2 );
static int DaoParser_ParseCodes( DaoParser *self, int from, int to )
{
	DaoNamespace *ns = self->nameSpace;
	DaoVmSpace *vmSpace = self->vmSpace;
	DaoRoutine *routine = self->routine;
	DaoClass *hostClass = self->hostClass;

	DaoEnode enode = {-1,0,0,NULL,NULL,NULL,NULL};
	DaoToken **tokens = self->tokens->items.pToken;
	DaoToken *ptok;
	DMap *switchMap;
	int cons = (vmSpace->options & DAO_OPTION_INTERUN) && (ns->options & DAO_NS_AUTO_GLOBAL);
	int i, k, rb, end, start = from, N = 0;
	int reg, reg1, cst = 0, topll = 0;
	int colon, comma, last, errorStart, needName;
	int storeType = 0, storeType2 = 0;
	int empty_decos = 0;
	unsigned char tki, tki2;

	DaoInode *back = self->vmcLast;
	DaoInode *inode, *opening, *closing;
	DString *mbs = self->mbs;
	DaoValue *value;

	self->permission = DAO_DATA_PUBLIC;

	if( from ==0 && (to+1) == self->tokens->size ){
		for(i=0; i<self->tokens->size; i++) tokens[i]->index = i;
	}

#if 0
	printf("routine = %p; %i, %i\n", routine, start, to );
	for(i=start; i<=to; i++) printf("%s  ", tokens[i]->string.mbs); printf("\n\n");
#endif

	self->vmcValue = NULL;
	while( start >= from && start <= to ){

		self->vmcValue = NULL;
		self->usedString = 0;
		self->usedArray = 0;
		self->curLine = tokens[start]->line;
		ptok = tokens[start];
		tki = tokens[start]->name;
		topll = (self->levelBase + self->lexLevel) ==0;
#if 0
		printf("At tokPos : %i, %i, %p\n", start, ptok->line, ptok->string );
		printf("At tokPos : %i, %i, %s\n", tki, ptok->line, ptok->string.mbs );
#endif
		if( self->warnings->size ) DaoParser_PrintWarnings( self );
		if( self->errors->size ) return 0;
		if( empty_decos && self->decoFuncs2->size ){
			DArray_Clear( self->decoFuncs2 );
			DArray_Clear( self->decoParams2 );
		}
		if( self->enumTypes->size ) DArray_Clear( self->enumTypes );
		errorStart = start;
		if( ! self->isClassBody ) self->permission = DAO_DATA_PUBLIC;
		if( tki >= DKEY_PRIVATE && tki <= DKEY_PUBLIC ){
			self->permission = tki - DKEY_PRIVATE + DAO_DATA_PRIVATE;
			start += 1;
			if( start <= to && tokens[start]->name == DTOK_COLON ) start += 1;
			if( start > to ) break;
			tki = tokens[start]->name;
			continue;
		}

		storeType = 0;
		storeType2 = 0;
		needName = 0;
		while( tki >= DKEY_CONST && tki <= DKEY_VAR ){
			int comb = ( storeType & DAO_DECL_VAR );
			needName = 1;
			switch( tki ){
			case DKEY_CONST :
				storeType |= DAO_DECL_CONST;
				storeType2 |= DAO_DECL_CONST;
				if( ! (storeType & DAO_DECL_LOCAL) ){
					if( self->levelBase + self->lexLevel ==0 ) storeType |= DAO_DECL_GLOBAL;
					else if( self->isClassBody ) storeType |= DAO_DECL_MEMBER;
				}
				break;
			case DKEY_GLOBAL :
				comb |= ( storeType & (DAO_DECL_LOCAL|DAO_DECL_CONST) );
				if( self->levelBase + self->lexLevel == 0 ){
					storeType |= DAO_DECL_GLOBAL;
					storeType2 |= DAO_DECL_GLOBAL;
				}else if( self->isClassBody == 0 ){
					DString_SetMBS( self->mbs, "global has to be declared at top level!" );
					DaoParser_Error( self, DAO_INVALID_STORAGE, self->mbs );
					return 0;
				}
				break;
			case DKEY_STATIC :
				comb |= ( storeType & (DAO_DECL_LOCAL|DAO_DECL_CONST) );
				storeType |= DAO_DECL_STATIC;
				storeType2 |= DAO_DECL_STATIC;
				if( self->isClassBody ) storeType |= DAO_DECL_MEMBER;
				break;
			case DKEY_VAR :
				comb |= storeType;
				storeType |= DAO_DECL_VAR;
				storeType2 |= DAO_DECL_VAR;
				if( self->isClassBody )
					storeType |= DAO_DECL_MEMBER;
				else
					storeType |= DAO_DECL_LOCAL;
				break;
			default : break;
			}
			if( comb && (start == to || tokens[start+1]->name != DKEY_ROUTINE) ){
				if( comb ==0 ) DaoParser_Error3( self, DAO_STATEMENT_OUT_OF_CONTEXT, start );
				DaoParser_Error2( self, DAO_INVALID_STORAGE, errorStart, start, 0 );
				return 0;
			}
			start ++;
			ptok = tokens[start];
			tki = ptok->name;
		}
		if( ns->options & DAO_NS_AUTO_GLOBAL ){
			if( self->levelBase + self->lexLevel ==0 && !(storeType & DAO_DECL_LOCAL) ){
				storeType |= DAO_DECL_GLOBAL;
			}
		}

		rb = -1;
		tki = tokens[start]->type;
		if( tki == DTOK_LB ) rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
		if( rb >0 && (rb+1) <= to && tokens[rb+1]->type == DTOK_ASSN ){
			/* multiple assignment: */
			DArray *inodes = DArray_New(0);
			int foldConst = storeType & (DAO_DECL_CONST|DAO_DECL_STATIC);
			self->curToken = start + 1;
			while( self->curToken < rb ){
				int tid = self->curToken;
				int cur = tokens[tid]->name;
				int nxt = tokens[tid+1]->name;
				cur = cur == DTOK_IDENTIFIER || cur >= DKEY_ABS;
				nxt = nxt == DTOK_COMMA || nxt == DTOK_RB;
				if( cur && nxt ){
					k = DaoParser_GetRegister( self, tokens[tid] );
					if( k < 0 ) DaoParser_DeclareVariable( self, tokens[tid], storeType, NULL );
				}
				enode = DaoParser_ParseExpression( self, 0 );
				if( enode.reg < 0 ) goto InvalidMultiAssignment;
				if( enode.update == NULL ){
					DaoParser_AddCode( self, DVM_MOVE, 0,0, enode.reg, tid, 0, 0 );
					DArray_Append( inodes, self->vmcLast );
				}else{
					int code = enode.update->code;
					if( code < DVM_GETVH || code > DVM_GETF ){
						DaoParser_Error3( self, DAO_INVALID_STATEMENT, errorStart );
						goto InvalidMultiAssignment;
					}
					enode.update->code += DVM_SETVH - DVM_GETVH;
					enode.update->c = enode.update->a;
					DaoParser_PopRegister( self );
					DArray_Append( inodes, enode.update );
				}
				if( DaoParser_CurrentTokenType( self ) == DTOK_COMMA ) self->curToken += 1;
			}
			self->curToken = rb + 2;
			if( foldConst ) self->needConst += 1;
			enode = DaoParser_ParseExpression( self, 0 );
			if( foldConst ) self->needConst -= 1;
			if( enode.reg < 0 ) goto InvalidMultiAssignment;
			if( foldConst && enode.konst == 0 ){
				DaoParser_Error2( self, DAO_EXPR_NEED_CONST_EXPR, start, end, 0 );
				goto InvalidMultiAssignment;
			}
			i = DaoParser_PushRegister( self );
			for(k=0; k<inodes->size; k++){
				int p1 = inodes->items.pInode[k]->first;
				int p2 = p1 + inodes->items.pInode[k]->last;
				reg = DaoParser_PushRegister( self );
				DaoParser_AddCode( self, DVM_GETDI, enode.reg, k, reg, p1, 0, p2 );
				DaoParser_AppendCode( self, inodes->items.pInode[k] );
				self->vmcLast->a = reg;
			}
			DArray_Delete( inodes );
			start = self->curToken;
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			continue;
InvalidMultiAssignment: DArray_Delete( inodes ); return 0;
		}

		tki = tokens[start]->name;
		tki2 = start+1 <= to ? tokens[start+1]->name : 0;
		if( needName && (ptok->type != DTOK_IDENTIFIER || (tki != DKEY_ENUM
						&& tki > DAO_NOKEY1 && tki < DKEY_ABS )) ){
			if( tki < DKEY_ROUTINE || tki > DKEY_OPERATOR || storeType2 != DAO_DECL_STATIC ){
				DaoParser_Error( self, DAO_TOKEN_NEED_NAME, & tokens[start]->string );
				DaoParser_Error3( self, DAO_INVALID_STATEMENT, errorStart );
				return 0;
			}
		}
		if( tki == DTOK_ID_THTYPE ){
#ifdef DAO_WITH_DECORATOR
			DaoInode *back = self->vmcLast;
			DaoRoutine *decfunc = NULL;
			DaoList *declist = NULL;
			DArray *cid = DaoParser_GetArray( self );
			empty_decos = 0;
			reg = DaoParser_GetRegister( self, tokens[start] );
			if( reg < 0 ) goto DecoratorError;
			if( LOOKUP_ISCST( reg ) == 0 ) goto DecoratorError;
			value = DaoParser_GetVariable( self, reg );
			if( value == NULL || value->type != DAO_ROUTINE ) goto DecoratorError;
			decfunc = & value->xRoutine;
			declist = DaoList_New();
			if( start+1 <= to && tokens[start+1]->name == DTOK_LB ){
				self->curToken = start + 2;
				enode = DaoParser_ParseExpressionList( self, DTOK_COMMA, NULL, cid );
				if( enode.reg < 0 ) goto DecoratorError;
				if( DaoParser_CheckTokenType( self, DTOK_RB, ")" ) == 0 ) goto DecoratorError;
				rb = self->curToken;
				if( enode.konst ==0 ){
					DaoParser_Error2( self, DAO_EXPR_NEED_CONST_EXPR, start+2, rb-1, 0 );
					goto DecoratorError;
				}
				if( cid->size >= DAO_MAX_PARAM ){
					DaoParser_Error2( self, DAO_PARAM_TOO_MANY, start+2, rb-1, 0 );
					goto DecoratorError;
				}
				for(k=0; k<cid->size; k++ ){
					DaoValue *v = DaoParser_GetVariable( self, cid->items.pInt[k] );
					DaoList_Append( declist, v );
				}
				start = rb;
			}
			if( self->byteBlock ){
				DaoByteBlock_EncodeValue( self->byteBlock, (DaoValue*) decfunc );
				DaoByteBlock_EncodeValue( self->byteBlock, (DaoValue*) declist );
			}
			DArray_PushFront( self->decoFuncs2, decfunc );
			DArray_PushFront( self->decoParams2, declist );
			DaoParser_PopCodes( self, back );
			start ++;
			continue;
DecoratorError:
			if( declist ) DaoList_Delete( declist );
			DaoParser_Error3( self, DAO_CTW_INVA_SYNTAX, start );
			return 0;
#else
			DaoParser_Error( self, DAO_DISABLED_DECORATOR, NULL );
			return 0;
#endif
		}
		empty_decos = 1;
		if( tki == DTOK_SEMCO ){
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			start ++;
			continue;
		}else if( tki == DKEY_USE ){
			start = DaoParser_ParseUseStatement( self, start, to );
			if( start <0 ) return 0;
			if( cons && topll ) DaoParser_MakeCodes( self, errorStart, start, ns->inputs );
			continue;
		}else if( tki == DKEY_TYPE ){
			start = DaoParser_ParseTypeAliasing( self, start, to );
			if( start <0 ) return 0;
			if( cons && topll ) DaoParser_MakeCodes( self, errorStart, start, ns->inputs );
			continue;
		}else if( tki == DKEY_ENUM && (tki2 == DTOK_LCB || tki2 == DTOK_IDENTIFIER) ){
			start = DaoParser_ParseEnumDefinition( self, start, to, storeType );
			if( start <0 ) return 0;
			if( cons && topll ) DaoParser_MakeCodes( self, errorStart, start, ns->inputs );
			continue;
		}

		/* parsing routine definition */
		if( (tki == DKEY_ROUTINE && tki2 != DTOK_LB) || tki == DKEY_OPERATOR ){
			start = DaoParser_ParseRoutineDefinition( self, start, from, to, storeType );
			if( start <0 ) return 0;
			if( cons && topll ) DaoParser_MakeCodes( self, errorStart, start, ns->inputs );
			continue;
		}else if( tki == DKEY_INTERFACE ){
			start = DaoParser_ParseInterfaceDefinition( self, start, to, storeType );
			if( start <0 ) return 0;
			if( cons && topll ) DaoParser_MakeCodes( self, errorStart, start, ns->inputs );
			continue;
		}else if( tki == DKEY_CLASS ){
			/* parsing class definition */
			start = DaoParser_ParseClassDefinition( self, start, to, storeType );
			if( start <0 ) return 0;
			if( cons && topll ) DaoParser_MakeCodes( self, errorStart, start, ns->inputs );
			continue;
		}

		self->curToken = start;
		tki = tokens[start]->name;
		switch( tki ){
		case DTOK_LCB :
			DaoParser_AddScope2( self, start );
			start++;
			continue;
		case DTOK_RCB :
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			if( DaoParser_DelScope( self, NULL ) == 0 ) return 0;
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			start++;
			continue;
		case DKEY_WHILE :
			opening = (DaoInode*) DArray_Back( self->scopeOpenings );
			closing = (DaoInode*) DArray_Back( self->scopeClosings );
			if( closing && closing->c == DVM_DO ){
				if( DaoParser_CompleteScope( self, start-1 ) == 0 ) return 0;
				inode = self->vmcLast;
				if( (rb = DaoParser_ParseCondition( self, start+1, 0, NULL )) <0 ) return 0;
				opening->jumpTrue = inode->next; /* first instruction in the condition */
				self->vmcLast->jumpFalse = closing; /* jump for failed testing */
				inode = DaoParser_AddCode( self, DVM_GOTO, 0, 0, 0, start, 0, 0 );
				inode->jumpTrue = opening; /* looping back */
				if( DaoParser_DelScope( self, NULL ) == 0 ) return 0;
				start = rb+1;
			}else{
				/* see comments in case DKEY_IF: */
				closing = DaoParser_AddCode( self, DVM_LABEL, 0, 1, 0, start, 0,0 );
				opening = DaoParser_AddScope( self, DVM_LOOP, closing );
				if( (rb = DaoParser_ParseCondition( self, start+1, 1, opening )) <0 ) return 0;
				opening->jumpTrue = self->vmcLast;
				start = 1 + rb + DaoParser_AddScope2( self, rb+1 );
			}
			continue;
		case DKEY_IF :
			/*
			// Add an auxiliary scope to simplify the handling of branchings.
			// Such scoping is marked by an opening inode and a closing inode.
			// The closing inode will be moved to the place where the scope is closed.
			//
			// opening->jumpTrue shall point to the start of the condition expression.
			// opening->jumpFalse = closing.
			//
			// closing->a holds the token name that will allow the scope to be extended.
			// closing->b indicates if scope is break-able (loop/switch): 0 (no), 1 (yes).
			//
			// In this case, the "else" keyword will always extend the scope of an "if" block.
			// Any other token will end the "if" block, and the closing node will be
			// moved to this place to serve a proper branching target!
			*/
			closing = DaoParser_AddCode( self, DVM_LABEL, DKEY_ELSE, 0, 0, start, 0,0 );
			opening = DaoParser_AddScope( self, DVM_BRANCH, closing );
			if( (rb = DaoParser_ParseCondition( self, start+1, 1, NULL )) <0 ) return 0;
			opening->jumpTrue = self->vmcLast;
			start = 1 + rb + DaoParser_AddScope2( self, rb+1 );
			continue;
		case DKEY_ELSE :
			opening = (DaoInode*) DArray_Back( self->scopeOpenings );
			closing = (DaoInode*) DArray_Back( self->scopeClosings );
			/* If not following "if" or "else if", abort with error: */
			if( closing == NULL || closing->a != DKEY_ELSE ){
				DaoParser_Error3( self, DAO_STATEMENT_OUT_OF_CONTEXT, start );
				return 0;
			}
			inode = DaoParser_AddCode( self, DVM_GOTO, 0, 0, 0, 0, 0, 0 );
			inode->jumpTrue = closing; /* jump out of the if block */
			inode = DaoParser_AddCode( self, DVM_NOP, 0, 0, 0, 0, 0, 0 );
			opening->jumpTrue->jumpFalse = inode; /* previous condition test jump here */
			opening->jumpTrue = inode; /* reset */

			if( tokens[start+1]->name == DKEY_IF ){
				if( (rb = DaoParser_ParseCondition( self, start+2, 1, NULL )) <0 ) return 0;
				opening->jumpTrue = self->vmcLast; /* update the condition test */
				start = 1 + rb + DaoParser_AddScope2( self, rb+1 );
			}else{
				closing->a = 0; /* the if block is done */
				start += 1 + DaoParser_AddScope2( self, start+1 );
			}
			continue;
		case DKEY_FOR :
			start = DaoParser_ParseForLoop( self, start, to );
			if( start < 0 ){
				DaoParser_Error3( self, DAO_INVALID_FOR, errorStart );
				return 0;
			}
			continue;
		case DKEY_DO :
			closing = DaoParser_AddCode( self, DVM_LABEL, DKEY_WHILE, 1, DVM_DO, start, 0,0 );
			opening = DaoParser_AddScope( self, DVM_LOOP, closing );
			opening->jumpTrue = DaoParser_AddCode( self, DVM_UNUSED, 0,0,0, start, 0,0 );
			start += 1 + DaoParser_AddScope2( self, start+1 );
			continue;
		case DKEY_SWITCH :
			rb = DaoParser_FindOpenToken( self, DTOK_LB, start, -1, 1 );
			if( rb < 0 ) return 0;
			rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
			if( rb < 0 ) return 0;
			reg1 = DaoParser_MakeArithTree( self, start+2, rb-1, & cst );
			if( reg1 < 0 ) return 0;
			self->curToken = rb + 1;
			if( DaoParser_CheckTokenType( self, DTOK_LCB, "{" ) ==0 ) return 0;
			switchMap = DMap_New(D_VALUE,0);
			closing = DaoParser_AddCode( self, DVM_LABEL, 0, 1, DVM_SWITCH, start, 0,0 );
			opening = DaoParser_AddScope( self, DVM_BRANCH, closing );
			DaoParser_AddCode( self, DVM_SWITCH, reg1, self->switchMaps->size, 0, start,0,rb );
			opening->jumpTrue = self->vmcLast;
			opening->jumpTrue->jumpFalse = closing;
			DArray_Append( self->switchMaps, switchMap );
			DMap_Delete( switchMap );
			start = 1 + rb + DaoParser_AddScope2( self, rb+1 );
			DaoParser_PushLevel( self );
			continue;
		case DKEY_CASE :
		case DKEY_DEFAULT :
			opening = closing = NULL;
			k = self->scopeOpenings->size;
			DaoParser_PopLevel( self );
			if( k >= 2 && self->scopeOpenings->items.pInode[k-1]->code == DVM_LBRA ){
				opening = self->scopeOpenings->items.pInode[k-2];
				closing = self->scopeClosings->items.pInode[k-2];
			}
			if( closing == NULL || closing->c != DVM_SWITCH ){
				DaoParser_Error3( self, DAO_STATEMENT_OUT_OF_CONTEXT, start );
				return 0;
			}
			inode = DaoParser_AddCode( self, DVM_GOTO, 0,0,0, start, 0, 0 );
			inode->jumpTrue = closing;
			if( tki == DKEY_DEFAULT ){
				if( opening->jumpFalse && opening->jumpFalse->code == DVM_DEFAULT ){
					DaoParser_Error2( self, DAO_DEFAULT_DUPLICATED, start, to, 1 );
					return 0;
				}
				self->curToken = start + 1;
				if( DaoParser_CheckTokenType( self, DTOK_COLON, ":" ) ==0 ) return 0;
				DaoParser_AddCode( self, DVM_DEFAULT, 0, 0, 0, start, 0, 0 );
				DaoParser_PushLevel( self );
				opening->jumpTrue->jumpFalse = self->vmcLast;
				start += 2;
				continue;
			}
			switchMap = self->switchMaps->items.pMap[ opening->jumpTrue->b ];
			colon = DaoParser_FindOpenToken( self, DTOK_COLON, start, -1, 1 );
			comma = DaoParser_FindOpenToken( self, DTOK_COMMA, start, colon, 0 );
			last = start + 1;
			if( colon < 0 ){
				DaoParser_Error2( self, DAO_CASE_NOT_VALID, start, to, 1 );
				return 0;
			}
			if( comma < 0 ) comma = colon;
			while( last < colon ){
				DNode *it;
				DaoEnode item = {-1,0,0,NULL,NULL,NULL,NULL};
				int dots = DaoParser_FindOpenToken( self, DTOK_DOTS, last, comma, 0 );
				int oldcount = self->regCount;
				back = self->vmcLast;
				self->curToken = last;
				if( dots <0 ){
					item = DaoParser_ParseExpression( self, DTOK_COLON );
				}else{
					DaoEnode e1, e2;
					e1 = DaoParser_ParseExpression( self, DTOK_DOTS );
					self->curToken += 1; /* skip dots */
					e2 = DaoParser_ParseExpression( self, DTOK_COLON );
					if( e1.reg >= 0 && e2.reg >=0 ) item.reg = e1.reg;
					if( e1.konst && e2.konst ){
						DaoValue *v1 = DaoParser_GetVariable( self, e1.konst );
						DaoValue *v2 = DaoParser_GetVariable( self, e2.konst );
						DaoTuple *tuple = DaoNamespace_MakePair( ns, v1, v2 );
						item.konst = DaoRoutine_AddConstant( routine, (DaoValue*)tuple );
						item.konst = LOOKUP_BIND_LC( item.konst );
					}
				}
				if( item.reg < 0 ){
					DaoParser_Error2( self, DAO_CASE_NOT_VALID, start, colon, 1 );
					return 0;
				}else if( item.konst ==0 ){
					DaoParser_Error2( self, DAO_CASE_NOT_CONSTANT, last, comma-1, 0 );
					DaoParser_Error2( self, DAO_CASE_NOT_VALID, start, colon, 1 );
					return 0;
				}else if( LOOKUP_ST( item.konst ) != DAO_LOCAL_CONSTANT ){
					value = DaoParser_GetVariable( self, item.konst );
					item.konst = DaoRoutine_AddConstant( routine, value );
				}else if( LOOKUP_UP( item.konst ) != 0 ){
					value = DaoParser_GetVariable( self, item.konst );
					item.konst = DaoRoutine_AddConstant( routine, value );
				}else{
					item.konst = LOOKUP_ID( item.konst );
				}
				/*
				// remove GETC so that CASETAG will be together,
				// which is neccessary to properly setup switch table:
				*/
				DaoParser_PopCodes( self, back );
				DaoParser_PopRegisters( self, self->regCount - oldcount );
				DaoParser_AddCode( self, DVM_NOP, item.konst, 0, 0, last, 0, colon );
				value = DaoParser_GetVariable( self, LOOKUP_BIND_LC( item.konst ) );
				for(it=DMap_First(switchMap); it; it=DMap_Next(switchMap,it)){
					DaoValue *key = it->key.pValue;
					int bl = DaoValue_Compare( value, key ) == 0;
					bl |= DaoValue_Compare( key, value ) == 0;
					if( bl == 0 && value->type == DAO_TUPLE && key->type == DAO_TUPLE ){
						DaoTuple *T1 = (DaoTuple*) value;
						DaoTuple *T2 = (DaoTuple*) key;
						if( T1->subtype == DAO_PAIR && T2->subtype == DAO_PAIR ){
							int le1 = DaoValue_Compare( T1->items[0], T2->items[1] ) <= 0;
							int le2 = DaoValue_Compare( T2->items[0], T1->items[1] ) <= 0;
							bl |= le1 && le2;
						}
					}
					if( bl ){
						DaoParser_Error2( self, DAO_CASE_DUPLICATED, start, colon, 1 );
						return 0;
					}
				}
				DMap_Insert( switchMap, value, self->vmcLast );
				if( self->curToken == colon ) break;
				if( tokens[self->curToken]->name != DTOK_COMMA ){
					DaoParser_Error2( self, DAO_CASE_NOT_VALID, start, colon, 1 );
					return 0;
				}else if( DaoParser_NextTokenName( self ) == DKEY_CASE ){
					self->curToken += 1;
				}
				last = self->curToken + 1;
			}
			DaoParser_AddCode( self, DVM_UNUSED, 0, 0, 0, start,0,0 );
			DaoParser_PushLevel( self );
			start = colon + 1;
			continue;
		case DKEY_BREAK :
		case DKEY_SKIP :
			inode = DaoParser_AddCode( self, DVM_GOTO, 0, 0, tki, start,0,0 );
			opening = DaoParser_GetBreakableScope( self );
			if( opening == NULL ){
				DaoParser_Error3( self, DAO_STATEMENT_OUT_OF_CONTEXT, start );
				return 0;
			}
			inode->jumpTrue = tki == DKEY_BREAK ? opening->jumpFalse : opening->next;
			if( opening->c == DVM_SECT ){
				inode->code = DVM_GOTOX;
				inode->a = tki == DKEY_BREAK;
				inode->jumpTrue = opening->jumpFalse;
			}
			if( inode->jumpTrue->code == DVM_SWITCH ) inode->jumpTrue = inode->jumpTrue->jumpFalse;
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			start += 1;
			continue;
		case DKEY_DEFER :
			reg = DaoParser_ExpClosure( self, start );
			if( reg < 0 ) return 0;
			start = self->curToken;
			if( DaoParser_CompleteScope( self, start ) == 0 ) return 0;
			continue;
		case DKEY_RETURN :
			start += 1;
			reg = N = end = 0;
			if( start <= to && tokens[start]->line == tokens[start-1]->line ){
				DaoType *retype = self->returnType; /* Updated for code section; */
				int tok = 0, ecount = self->errors->size;
				self->curToken = start;
				switch( retype ? retype->tid : 0 ){
				case DAO_ARRAY : tok = DTOK_LSB; break;
				case DAO_LIST  : tok = DTOK_LCB; break;
				case DAO_MAP   : tok = DTOK_LCB; break;
				case DAO_TUPLE : tok = DTOK_LB; break;
				}
				if( tokens[start]->type == tok ){
					/* routine test() => list<tuple<a:int,b:int>> { return {(1,2), (3,4)} } */
					int rb = DaoParser_FindPairToken( self, tok, tok + 1, start, to );
					DArray_PushFront( self->enumTypes, retype );
					enode = DaoParser_ParseEnumeration( self, 0, tok, start+1, rb-1 );
					DArray_PopFront( self->enumTypes );
					self->curToken += 1;
				}else{
					enode = DaoParser_ParseExpressionList( self, DTOK_COMMA, NULL, NULL );
				}
				if( tokens[start]->type != DTOK_SEMCO && tokens[start]->type != DTOK_RCB ){
					if( self->errors->size > ecount ) return 0;
				}
				if( enode.reg >= 0 ){
					reg = enode.reg;
					N = enode.count;
					end = self->curToken;
					start = end;
				}
			}
			if( self->isFunctional && N > 1 ){
				int tup = DaoParser_PushRegister( self );
				DaoParser_AddCode( self, DVM_TUPLE, reg, N, tup, start, 0, end );
				DaoParser_AddCode( self, DVM_RETURN, tup, 1, 0, start, 0, end );
			}else{
				DaoParser_AddCode( self, DVM_RETURN, reg, N, 0, start, 0, end );
			}
			if( DaoParser_CompleteScope( self, start-1 ) == 0 ) return 0;
			continue;
		}

		end = DaoParser_ParseVarExpressions( self, start, to, 0, storeType, storeType2 );
		if( end < 0 ) return 0;
		if( DaoParser_CompleteScope( self, end-1 ) == 0 ) return 0;
		DaoParser_CheckStatementSeparation( self, end-1, to );
		start = end;
	}
	if( DaoParser_CompleteScope( self, to ) == 0 ) return 0;
	DaoParser_PrintWarnings( self );
	return 1;
}

static int DaoParser_SetInitValue( DaoParser *self, DaoVariable *var, DaoValue *value, DaoType *type, int start, int end )
{
	DaoNamespace *ns = self->nameSpace;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoType *tp1 = var->dtype;
	if( tp1 == NULL && type != NULL ){
		GC_IncRC( type );
		var->dtype = tp1 = type;
	}
	if( tp1 && DaoType_MatchValue( tp1, value, 0 ) ==0 ){
		DaoType *tp2 = DaoNamespace_GetType( ns, value );
		self->curLine = tokens[start]->line;
		DaoParser_Error( self, DAO_TYPE_PRESENTED, tp2->name );
		self->curLine = tokens[start]->line;
		DaoParser_Error( self, DAO_TYPE_EXPECTED, tp1->name );
		DaoParser_Error2( self, DAO_TYPE_NOT_MATCHING, start, end, 0 );
		return 0;
	}
	DaoValue_Copy( value, & var->value );
	return 1;
}
int DaoParser_ParseVarExpressions( DaoParser *self, int start, int to, int var, int store, int explicit_store )
{
	DaoValue *value;
	DaoEnode enode;
	DaoType *abtp, *extype;
	DaoVmSpace *vms = self->vmSpace;
	DaoNamespace *ns = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DaoClass *hostClass = self->hostClass;
	DaoToken *ptok, *lastok, **tokens = self->tokens->items.pToken;
	DaoInode *back = self->vmcLast;
	int cons = (vms->options & DAO_OPTION_INTERUN) && (ns->options & DAO_NS_AUTO_GLOBAL);
	int topll = (self->levelBase + self->lexLevel) ==0;
	int nameStart, oldcount = self->regCount;
	int expression = explicit_store == 0;
	int reg, cst, temp, eq, errorStart = start;
	int k, m, end = start, remove = 1;
	int tki, tki2, errors;
#if 0
	int mm;
	for(mm=start; mm<=to; mm++) printf( "%s ", tokens[mm]->string.mbs );
	printf("\n");
#endif

	/*
	// Storage prefixes have been parsed, if there were any.
	// Check variable declaration patterns:
	// [ SPECIFIERS ] IDENTIFIER { , IDENTIFIER } [ : TYPE ] [ = VALUE ]
	*/
	k = start;
	ptok = tokens[k];
	nameStart = self->toks->size;
	while( ptok->name == DTOK_IDENTIFIER || ptok->name >= DKEY_ABS ){
		DArray_Append( self->toks, ptok );
		if( (++k) > to ) break;
		lastok = ptok;
		ptok = tokens[k];
		if( ptok->name == DTOK_SEMCO || ptok->line != lastok->line ){
			expression &= 1;
			break;
		}
		if( ptok->name == DTOK_COLON || ptok->name == DTOK_ASSN ){
			expression = 0;
			break;
		}
		if( ptok->name != DTOK_COMMA ){
			expression = 1;
			break;
		}
		if( (++k) > to ) break; /* skip comma */
		ptok = tokens[k];
	}
	lastok = ptok;
	if( explicit_store == 0 && ptok->name == DTOK_ASSN && self->toks->size == nameStart+1 ){
		DaoToken *varTok = self->toks->items.pToken[nameStart];
		expression = DaoParser_GetRegister( self, varTok ) >= 0;
	}
	if( expression ){
		DArray_Erase( self->toks, nameStart, self->toks->size - nameStart );
		if( explicit_store ){
			DaoParser_Error3( self, DAO_INVALID_STATEMENT, errorStart );
			return -1;
		}
		self->curToken = start;
		enode = DaoParser_ParseExpression2( self, 0, 0, 0 );
		if( enode.reg < 0 ) return -1;
		while( DaoParser_CurrentTokenType( self ) == DTOK_COMMA ){
			self->curToken += 1;
			enode = DaoParser_ParseExpression2( self, 0, 0, 0 );
			if( enode.reg < 0 ) return -1;
		}
		return self->curToken;
	}
	self->curToken = k;
	eq = lastok->type == DTOK_ASSN ? k : -1;

	errors = self->errors->size;
	if( explicit_store != 0 || lastok->type == DTOK_COLON ){
		for(k=nameStart; k<self->toks->size; k++){
			DString *name = & self->toks->items.pToken[k]->string;
			DNode *node = MAP_Find( DaoParser_GetCurrentDataMap( self ), name );
			if( node ) DaoParser_Error( self, DAO_SYMBOL_WAS_DEFINED, name );
		}
		if( self->errors->size > errors ) return -1;
	}
	abtp = extype = NULL;
	if( DaoParser_CurrentTokenType( self ) == DTOK_COLON ){
		extype = DaoParser_ParseType( self, self->curToken+1, to, & k, NULL );
		if( extype == NULL ){
			DaoParser_Error3( self, DAO_INVALID_STATEMENT, errorStart );
			return -1;
		}
		self->curToken = k;
	}
	abtp = extype;
	oldcount = self->regCount;
	back = self->vmcLast;
	enode.reg = -1;
	enode.konst = 0;
	if( DaoParser_CurrentTokenType( self ) == DTOK_ASSN ){
		int foldConst = self->isClassBody && (store & DAO_DECL_MEMBER);
		foldConst |= store & (DAO_DECL_CONST|DAO_DECL_STATIC);
		if( self->curToken + 1 > to ){
			DaoParser_Error3( self, DAO_INVALID_STATEMENT, errorStart );
			return -1;
		}
		if( abtp && abtp->tid >= DAO_ARRAY && abtp->tid <= DAO_TUPLE )
			DArray_PushFront( self->enumTypes, abtp );
		self->curToken += 1;
		if( foldConst ) self->needConst += 1;
		enode = DaoParser_ParseExpression( self, 0 );
		if( foldConst ) self->needConst -= 1;
		start = end = self->curToken;
		if( enode.reg < 0 ){
			DaoParser_Error2( self, DAO_INVALID_STATEMENT, start, end, 0 );
			return -1;
		}
		if( foldConst && enode.konst == 0 ){
			DaoParser_Error2( self, DAO_EXPR_NEED_CONST_EXPR, start, end, 0 );
			return -1;
		}
	}

	reg = enode.reg;
	cst = enode.konst;
	start = end = self->curToken;
	if( cst == 0 && (store & DAO_DECL_CONST) ){
		DaoParser_Error2( self, DAO_EXPR_NEED_CONST_EXPR, start + 1, end, 0 );
		return -1;
	}
	value = NULL;
	if( enode.konst ){
		value = DaoParser_GetVariable( self, enode.konst );
		if( abtp && DaoType_MatchValue( abtp, value, NULL ) == 0 ){
			DaoParser_Error3( self, DAO_TYPE_NOT_MATCHING, errorStart );
			return -1;
		}
	}
	if( abtp == NULL && value ){
		abtp = DaoNamespace_GetType( ns, value );
		if( self->byteBlock ) DaoByteBlock_EncodeTypeOf( self->byteBlock, abtp, value );
	}
	for(k=nameStart; k<self->toks->size; k++){
		DaoByteBlock *block = self->byteBlock;
		DaoToken *varTok = self->toks->items.pToken[k];
		DString *name = & varTok->string;
		int id = 0;
		/* printf( "declaring %s\n", varTok->string.mbs ); */
		DaoParser_DeclareVariable( self, varTok, store, abtp );
		if( store & DAO_DECL_CONST ){
			if( store & DAO_DECL_GLOBAL ){
				id = DaoNamespace_FindConst( ns, & varTok->string );
				if( id < 0 ){
					DaoParser_Error( self, DAO_SYMBOL_NOT_DEFINED, & varTok->string );
					return -1;
				}
				DaoNamespace_SetConst( ns, id, value );
				if( block ){
					DaoByteBlock_DeclareConst( block, name, value, LOOKUP_PM(id) );
				}
			}else if( self->isClassBody && hostClass ){
				id = DaoClass_FindConst( hostClass, & varTok->string );
				if( cst ){
					DaoClass_SetConst( hostClass, id, value );
				}else if( eq >=0 ){
					DaoParser_Error2( self, DAO_EXPR_NEED_CONST_EXPR, eq+1, end, 0 );
					return -1;
				}
				if( block ){
					DaoByteBlock_DeclareConst( block, name, value, LOOKUP_PM(id) );
				}
			}else{
				id = LOOKUP_ID( DaoParser_GetRegister( self, varTok) );
				DaoValue_Copy( value, routine->routConsts->items.items.pValue + id );
				DaoValue_MarkConst( routine->routConsts->items.items.pValue[id] );
			}
		}else{
			int regC = DaoParser_GetRegister( self, varTok );
			int st = LOOKUP_ST( regC );
			int up = LOOKUP_UP( regC );
			int id = LOOKUP_ID( regC );
			int pm = LOOKUP_PM( regC );
			int isdecl = self->isClassBody && (store & DAO_DECL_MEMBER);
			int first = varTok->index;
			int mid = eq >= 0 ? eq : 0;
			remove = 0;
			switch( st ){
			case DAO_LOCAL_VARIABLE :
				if( reg < 0 ) continue;
				if( (up = DaoParser_GetOuterLevel( self, id )) > 0 ){
					DaoParser_AddCode( self, DVM_SETVH, reg, id, up, first, mid, end );
				}else{
					DaoParser_AddCode( self, DVM_MOVE, reg, 0, id, first, mid, end );
				}
				break;
			case DAO_STATIC_VARIABLE :
				if( cst && (explicit_store & DAO_DECL_STATIC) ){
					DaoVariable *var = routine->body->svariables->items.pVar[id];
					if( DaoParser_SetInitValue( self, var, value, abtp, start, end ) == 0 ){
						return -1;
					}
					remove = 1;
					if( block ){
						DaoByteBlock_DeclareStatic( block, name, var->value, var->dtype, pm );
					}
				}else if( reg >= 0 ){
					DaoParser_AddCode( self, DVM_SETVS, reg, id, 0, first, mid, end );
				}else if( explicit_store && block ){
					DaoByteBlock_DeclareStatic( block, name, NULL, extype, pm );
				}
				break;
			case DAO_OBJECT_VARIABLE :
				if( isdecl && cst ){
					DaoVariable *var = hostClass->instvars->items.pVar[id];
					if( DaoParser_SetInitValue( self, var, value, NULL, start, end ) == 0 ){
						return -1;
					}
					remove = 1;
					DaoValue_MarkConst( var->value );
					if( block ){
						DaoByteBlock_DeclareVar( block, name, var->value, var->dtype, pm );
					}
				}else if( ! self->isClassBody ){
					if( reg < 0 ) continue;
					DaoParser_AddCode( self, DVM_SETVO, reg, id, 0, first, mid, end );
				}else if( eq >=0 ){
					DaoParser_Error2( self, DAO_EXPR_NEED_CONST_EXPR, eq+1, end, 0 );
					return -1;
				}else if( abtp && DaoParser_CheckDefault( self, abtp, errorStart ) ==0 ){
					DaoParser_Error2( self, DAO_TYPE_NO_DEFAULT, errorStart, start, 0 );
					return -1;
				}else if( block ){
					DaoByteBlock_DeclareVar( block, name, NULL, extype, pm );
				}
				break;
			case DAO_CLASS_VARIABLE :
				if( isdecl && cst ){
					DaoVariable *var = hostClass->variables->items.pVar[id];
					DaoVariable_Set( var, value, DaoNamespace_GetType( ns, value ) );
					remove = 1;
					if( block ){
						DaoByteBlock_DeclareStatic( block, name, var->value, var->dtype, pm );
					}
				}else if( ! self->isClassBody ){
					if( reg < 0 ) continue;
					DaoParser_AddCode( self, DVM_SETVK, reg, id, up, first, mid, end );
				}else if( eq >=0 ){
					DaoParser_Error2( self, DAO_EXPR_NEED_CONST_EXPR, eq+1, end, 0 );
					return -1;
				}else if( abtp && DaoParser_CheckDefault( self, abtp, errorStart ) ==0 ){
					DaoParser_Error2( self, DAO_TYPE_NO_DEFAULT, errorStart, start, 0 );
					return -1;
				}else if( block ){
					DaoByteBlock_DeclareStatic( block, name, NULL, extype, pm );
				}
				break;
			case DAO_GLOBAL_VARIABLE :
				if( reg < 0 ){
					if( explicit_store && block ){
						DaoByteBlock_DeclareGlobal( block, name, NULL, extype, pm );
					}
					continue;
				}
				if( cst && (explicit_store & DAO_DECL_GLOBAL) ){
					DaoVariable *var = ns->variables->items.pVar[id];
					if( DaoParser_SetInitValue( self, var, value, abtp, start, end ) == 0 ){
						return -1;
					}
					remove = 1;
					if( block ){
						DaoByteBlock_DeclareGlobal( block, name, var->value, var->dtype, pm );
					}
				}else{
					DaoParser_AddCode( self, DVM_SETVG, reg, id, up, first, mid, end );
					self->usingGlobal = 1;
					if( explicit_store && block ){
						DaoVariable *var = ns->variables->items.pVar[id];
						DaoByteBlock_DeclareGlobal( block, name, NULL, var->dtype, pm );
					}
				}
				break;
			default :
				DaoParser_Error( self, DAO_EXPR_MODIFY_CONSTANT, & varTok->string );
				return -1;
			}
		}
	}
	DArray_Erase( self->toks, nameStart, self->toks->size - nameStart );
	if( cst && remove ){
		DaoParser_PopCodes( self, back );
		DaoParser_PopRegisters( self, self->regCount - oldcount );
	}
	self->curToken = end;
	if( DaoParser_CurrentTokenType( self ) == DTOK_COMMA && (end+1) <= to ){
		return DaoParser_ParseVarExpressions( self, end+1, to, 1, store, explicit_store );
	}
	return end;
}
static int DaoParser_SetupBranching( DaoParser *self )
{
	DaoInode *it, *it2 = NULL;
	int id = 0, unused = 0, notide = !(self->vmSpace->options & DAO_OPTION_IDE);;
	if( self->vmcLast->code != DVM_RETURN ){
		DaoVmSpace *vms = self->vmSpace;
		DaoNamespace *ns = self->nameSpace;
		int first = self->vmcLast->first + self->vmcLast->middle + self->vmcLast->last + 1;
		int opa = 0, autoret = self->autoReturn;

		if( autoret == 0 ){
			int print = (vms->options & DAO_OPTION_INTERUN) && (ns->options & DAO_NS_AUTO_GLOBAL);
			int ismain = self->routine->attribs & DAO_ROUT_MAIN;
			autoret = ismain && (print || vms->evalCmdline);
		}
		if( autoret && self->vmcValue ) opa = self->vmcValue->c;
		autoret &= self->vmcValue != NULL;
		DaoParser_AddCode( self, DVM_RETURN, opa, autoret, 0, first,0,0 );
	}
	if( self->vmSpace->options & DAO_OPTION_IDE ){
		/* for setting break points in DaoStudio */
		for(it=self->vmcFirst; it; it=it2){
			it2 = it->next;
			if( it->code == DVM_NOP ) it->code = DVM_UNUSED;
			if( it2 && it2->code == DVM_CASE ) continue;
			if( it2 && it2->line != it->line ){
				DaoParser_InsertCode( self, it, DVM_NOP, 0,0,0, it2->first )->line = it2->line;
			}
		}
	}

	for(it=self->vmcFirst; it; it=it->next){
		switch( it->code ){
		case DVM_NOP : if( notide ) it->code = DVM_UNUSED; break;
		case DVM_GOTO : if( it->jumpTrue == it->next ) it->code = DVM_UNUSED; break;
		default : if( it->code >= DVM_NULL ) it->code = DVM_UNUSED; break;
		}
		if( it->code == DVM_UNUSED || it->code == DVM_NOP ){
			if( it->prev && it->prev->code == DVM_NOP ) it->prev->code = DVM_UNUSED;
		}
	}
	for(it=self->vmcFirst,id=0; it; it=it->next){
		it->index = id;
		id += it->code != DVM_UNUSED;
	}
	if( self->regCount > 0xefff || id > 0xefff ){
		/*
		// Though Dao VM instructions can hold operand id or jump id as big as 0xffff,
		// the type inference procedure may need to allocate additional registers, or
		// add additional instructions to handle code specialization or type casting.
		*/
		char buf[50];
		if( id > 0xefff ){
			sprintf( buf, "instructions (%i)!", id );
		}else{
			sprintf( buf, "virtual registers (%i)!", self->regCount );
		}
		DString_SetMBS( self->mbs, "too big function with too many " );
		DString_AppendMBS( self->mbs, buf );
		DaoParser_Error( self, DAO_CTW_INTERNAL, self->mbs );
		DaoParser_PrintError( self, 0, 0, NULL );
		return 0;
	}
	/* DaoParser_PrintCodes( self ); */
	DArray_Clear( self->vmCodes );
	for(it=self->vmcFirst; it; it=it->next){
		/*
		DaoInode_Print( it );
		*/
		switch( it->code ){
		case DVM_NOP : break;
		case DVM_TEST   : it->b = it->jumpFalse->index; break;
		case DVM_GOTO   : it->b = it->jumpTrue->index;  break;
		case DVM_GOTOX  : it->b = it->jumpTrue->index;  break;
		case DVM_SWITCH : it->b = it->jumpFalse->index; break;
		case DVM_CASE   : it->b = it->jumpTrue->index;  break;
		default : break;
		}
		if( it->code != DVM_UNUSED ) DArray_Append( self->vmCodes, (DaoVmCodeX*) it );
	}
	return 1;
}
int DaoParser_ParseRoutine( DaoParser *self )
{
	DaoType *tt, *ft;
	DaoNamespace *myNS = self->nameSpace;
	DaoRoutine *routine = self->routine;
	const int tokCount = self->tokens->size;
	int id, np, offset = 0, defLine = routine->defLine;

	if( self->parsed ) return 1;
	GC_ShiftRC( myNS, routine->nameSpace );
	routine->nameSpace = myNS;

	self->parsed  = 1;
	self->returnType = (DaoType*) routine->routType->aux;

	if( (routine->attribs & DAO_ROUT_DECORATOR) && routine->routType->nested->size ){
		assert( routine->parCount == self->regCount );
		ft = routine->routType->nested->items.pType[0];
		if( ft->attrib & DAO_TYPE_SELFNAMED ){
			if( routine->routType->nested->size == 1 ) return 0;
			ft = routine->routType->nested->items.pType[1];
		}
		if( ft->tid == DAO_PAR_NAMED ) ft = (DaoType*) ft->aux;
		if( ft->tid != DAO_ROUTINE ) return 0;
		np = ft->nested->size;
		//if( np && ft->nested->items.pType[np-1]->tid == DAO_PAR_VALIST ) np -= 1;
		tt = DaoNamespace_MakeType( myNS, "tuple", DAO_TUPLE, 0, ft->nested->items.pType, np );
		DaoParser_DeclareVariable( self, self->decoArgName, 0, tt );
	}
	if( self->argName ){
		DArray *partypes = routine->routType->nested;
		np = partypes->size;
		//if( np && partypes->items.pType[np-1]->tid == DAO_PAR_VALIST ) np -= 1;
		tt = DaoNamespace_MakeType( myNS, "tuple", DAO_TUPLE, 0, partypes->items.pType, np );
		id = self->regCount;
		DaoParser_DeclareVariable( self, self->argName, 0, tt );
		DaoParser_AddCode( self, DVM_TUPLE, 0, routine->parCount, id, 0,0,0 );
	}

	if( DaoParser_ParseCodes( self, offset, tokCount-1 )==0 ){
		DaoParser_PrintError( self, 0, 0, NULL );
		return 0;
	}
	if( self->scopeOpenings->size ){
		DaoParser_Error3( self, DAO_INVALID_UNCLOSED_SCOPE, self->curToken );
		DaoParser_PrintError( self, 0, 0, NULL );
		return 0;
	}
	routine->defLine = defLine;

	if( self->errors->size ) return 0;
	if( DaoParser_PostParsing( self ) == 0 ) return 0;
#ifdef DAO_WITH_DECORATOR
	DaoParser_DecorateRoutine( self, routine );
	if( self->errors->size ){
		DaoParser_PrintError( self, 0, 0, NULL );
		return 0;
	}
#endif
	return 1;
}
static DaoEnode DaoParser_NoneValue( DaoParser *self )
{
	DaoEnode enode = {0,0,0,NULL,NULL,NULL,NULL};
	int cst = 0;
	if( self->noneValue >= 0 ){
		cst = self->noneValue;
	}else{
		cst = DaoRoutine_AddConstant( self->routine, dao_none_value );
		self->noneValue = cst;
	}
	enode.reg = DaoParser_PushRegister( self );
	enode.konst = self->noneValue = LOOKUP_BIND_LC( cst );
	DaoParser_AddCode( self, DVM_DATA, DAO_NONE, 0, enode.reg, self->curToken,0,0 );
	enode.first = enode.last = enode.update = self->vmcLast;
	enode.prev = self->vmcLast->prev;
	return enode;
}
static int DaoParser_IntegerOne( DaoParser *self, int start )
{
	int reg = DaoParser_PushRegister( self );
	DaoParser_AddCode( self, DVM_DATA, DAO_INTEGER, 1, reg, self->curToken,0,0 );
	return reg;
}
void DaoParser_DeclareVariable( DaoParser *self, DaoToken *tok, int storeType, DaoType *abtp )
{
	DaoNamespace *nameSpace = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DString *name = & tok->string;
	int perm = self->permission;
	int found;

	if( tok->name >= DKEY_USE && tok->name <= DKEY_PUBLIC ){
		DaoParser_Error3( self, DAO_TOKEN_NEED_NAME, tok->index );
		return;
	}
	if( self->isInterBody ){
		DaoParser_Error3( self, DAO_VARIABLE_OUT_OF_CONTEXT, tok->index );
		return;
	}
	if( storeType != 0 && MAP_Find( DaoParser_GetCurrentDataMap( self ), name ) != NULL ){
		DaoParser_Error( self, DAO_SYMBOL_WAS_DEFINED, name );
		return;
	}

	if( storeType & DAO_DECL_LOCAL ){
		if( MAP_Find( DaoParser_GetCurrentDataMap( self ), name ) == NULL ){
			int id = self->regCount;
			if( abtp ) MAP_Insert( self->routine->body->localVarType, id, abtp );
			MAP_Insert( DaoParser_GetCurrentDataMap( self ), name, id );
			DaoParser_PushRegister( self );
		}
	}else if( storeType & DAO_DECL_MEMBER ){
		if( self->hostClass ){
			DaoClass *klass = self->hostClass;
			if( self->isClassBody ){
				DNode *it = DMap_Find( klass->lookupTable, name );
				if( it != NULL && LOOKUP_UP( it->value.pInt ) == 0 ){
					DaoParser_Error( self, DAO_CTW_WAS_DEFINED, name );
					return;
				}
				if( storeType & DAO_DECL_CONST ){
					DaoClass_AddConst( klass, name, dao_none_value, perm );
				}else if( storeType & DAO_DECL_STATIC ){
					DaoClass_AddGlobalVar( klass, name, NULL, abtp, perm );
				}else{
					DaoClass_AddObjectVar( klass, name, dao_none_value, abtp, perm );
				}
			}else{
				DaoParser_Error( self, DAO_VARIABLE_OUT_OF_CONTEXT, name );
			}
		}else{
			DaoParser_Error( self, DAO_VARIABLE_OUT_OF_CONTEXT, name );
		}
	}else if( storeType & DAO_DECL_GLOBAL ){
		DNode *it = DMap_Find( nameSpace->lookupTable, name );
		if( it != NULL && LOOKUP_UP( it->value.pInt ) == 0 ){
			DaoParser_Error( self, DAO_CTW_WAS_DEFINED, name );
			return;
		}
		if( storeType & DAO_DECL_CONST ){
			DaoNamespace_AddConst( nameSpace, name, dao_none_value, perm );
		}else{
			DaoNamespace_AddVariable( nameSpace, name, NULL, abtp, perm );
		}
	}
	found = DaoParser_GetRegister( self, tok );
	if( found >= 0 ){
		MAP_Insert( DaoParser_GetCurrentDataMap( self ), name, found );
		return;
	}
	if( self->errors->size ) return;

	if( storeType & DAO_DECL_STATIC ){
		DaoVariable *var = DaoVariable_New( abtp ? abtp->value : NULL, abtp ? abtp : NULL );
		int i = LOOKUP_BIND( DAO_STATIC_VARIABLE, 0, 0, routine->body->svariables->size );
		MAP_Insert( DaoParser_GetCurrentDataMap( self ), name, i );
		DArray_Append( routine->body->svariables, var );
	}else{
		int id = 0;
		if( storeType & DAO_DECL_CONST ){
			id = routine->routConsts->items.size;
			MAP_Insert( DaoParser_GetCurrentDataMap( self ), name, LOOKUP_BIND_LC(id) );
			DaoRoutine_AddConstant( routine, dao_none_value );
		}else{
			id = self->regCount;
			if( abtp ) MAP_Insert( self->routine->body->localVarType, id, abtp );
			MAP_Insert( DaoParser_GetCurrentDataMap( self ), name, id );
			DaoParser_PushRegister( self );
		}
		tok = DArray_Append( routine->body->defLocals, tok );
		DaoToken_Set( tok, !(storeType & DAO_DECL_CONST), self->lexLevel, id, NULL );
	}
	found = DaoParser_GetRegister( self, tok );
	MAP_Insert( DaoParser_GetCurrentDataMap( self ), name, found );
}
static int DaoParser_GetRegister2( DaoParser *self, DaoToken *nametok )
{
	DaoNamespace *ns = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DString *name = & nametok->string;
	DNode *node = NULL;
	int i;

	if( self->hostCdata ){
		/* QStyleOption( version : int = QStyleOption::Version, ... ) */
		DaoValue *it = DaoType_FindValueOnly( self->hostCdata, name );
		if( it ){
			i = routine->routConsts->items.size;
			MAP_Insert( DaoParser_GetCurrentDataMap( self ), name, LOOKUP_BIND_LC(i) );
			DaoRoutine_AddConstant( routine, it );
			return LOOKUP_BIND_LC( i );
		}
	}
	if( self->isClassBody ){ /* a=1; b=class('t'){ var a = 2 }; */
		/* Look for variable in class: */
		if( self->hostClass && (node = MAP_Find( self->hostClass->lookupTable, name )) ){
			return node->value.pInt;
		}
	}

	/* Look for local data: */
	for( i=self->lexLevel; i>=0; i-- ){
		node = MAP_Find( self->localDataMaps->items.pMap[i], name );
		if( node ) return node->value.pInt;
	}

	/* Look for variable in class: */
	if( self->hostClass && (node = MAP_Find( self->hostClass->lookupTable, name )) ){
		int st = LOOKUP_ST( node->value.pInt );
		if( st == DAO_OBJECT_VARIABLE ){
			if( routine->attribs & DAO_ROUT_STATIC ){
				DaoParser_ErrorToken( self, DAO_VARIABLE_OUT_OF_CONTEXT, nametok );
				return -1;
			}
		}
		return node->value.pInt;
	}

	if( (i = DaoNamespace_FindVariable( ns, name )) >= 0 ) return i;
	if( (node = MAP_Find( self->allConsts, name )) ) return LOOKUP_BIND_LC( node->value.pInt );
	if( (i = DaoNamespace_FindConst( ns, name )) >= 0 ) return i;
	return -1;
}
int DaoParser_GetRegister( DaoParser *self, DaoToken *nametok )
{
	int loc = DaoParser_GetRegister2( self, nametok );
	/* Search up-value if, name look up failed, or the name is not local: */
	if( self->outerParser != NULL && (loc < 0 || LOOKUP_ST(loc) > DAO_STATIC_VARIABLE) ){
		DaoRoutine *routine = self->routine;
		int i = DaoParser_GetRegister( self->outerParser, nametok );
		int st = LOOKUP_ST( i );
		/*
		// Use up-value if, name look up failed, or the up-value is local;
		// namely local data of the outer scope preceeds the global data;
		*/
		if( i >=0 && (loc < 0 || st <= DAO_STATIC_VARIABLE) ){
			if( st == DAO_LOCAL_CONSTANT ){
				int id = LOOKUP_ID( i );
				DaoValue *cst = self->outerParser->routine->routConsts->items.items.pValue[id];
				i = LOOKUP_BIND_LC( routine->routConsts->items.size );
				MAP_Insert( DaoParser_GetCurrentDataMap( self ), & nametok->string, i );
				DaoRoutine_AddConstant( routine, cst );
			}else{
				int tokpos = nametok->index;
				i = DaoParser_GetNormRegister( self->outerParser, i, tokpos, 0, tokpos );
				DArray_Append( self->uplocs, i );
				DArray_Append( self->uplocs, routine->body->svariables->size + DAO_MAX_PARAM );
				DArray_Append( self->uplocs, tokpos );
				DArray_Append( self->uplocs, tokpos );
				i = LOOKUP_BIND( DAO_STATIC_VARIABLE, 0, 0, routine->body->svariables->size );
				MAP_Insert( DaoParser_GetCurrentDataMap( self ), & nametok->string, i );
				DArray_Append( routine->body->svariables, DaoVariable_New(NULL,NULL) );
			}
			return i;
		}
	}
	return loc;
}
DaoValue* DaoParser_GetVariable( DaoParser *self, int reg )
{
	DaoInteger daoIntegerZero = {DAO_INTEGER,0,0,0,1,0};
	DaoInteger daoIntegerOne  = {DAO_INTEGER,0,0,0,1,1};
	DaoComplex daoComplexImag = {DAO_COMPLEX,0,0,0,1,{0.0,1.0}};
	DaoNamespace *ns = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DaoClass *klass = self->hostClass;
	DaoValue *val = NULL;
	int st = LOOKUP_ST( reg );
	int up = LOOKUP_UP( reg );
	int id = LOOKUP_ID( reg );

	if( st == DAO_LOCAL_VARIABLE ){
		if( reg == self->integerZero ){
			val = (DaoValue*) & daoIntegerZero;
		}else if( reg == self->integerOne ){
			val = (DaoValue*) & daoIntegerOne;
		}else if( reg == self->imaginaryOne ){
			val = (DaoValue*) & daoComplexImag;
		}
		return val;
	}
	switch( st ){
	case DAO_LOCAL_CONSTANT  : val = routine->routConsts->items.items.pValue[id]; break;
	case DAO_CLASS_CONSTANT  : val = klass->constants->items.pConst[id]->value; break;
	case DAO_GLOBAL_VARIABLE : val = ns->variables->items.pVar[id]->value; break;
	case DAO_GLOBAL_CONSTANT : val = ns->constants->items.pConst[id]->value; break;
	default : break;
	}
	return val;
}
int DaoParser_GetNormRegister( DaoParser *self, int reg, int first, int mid, int last )
{
	DaoVmCodeX vmc;
	int line = self->tokens->items.pToken[first]->line;
	int st = LOOKUP_ST( reg );
	int up = LOOKUP_UP( reg );
	int id = LOOKUP_ID( reg );
	int regc, code = DVM_NOP;

	/* printf( "reg = %x\n", reg ); */
	switch( st ){
	case DAO_LOCAL_VARIABLE :
		up = DaoParser_GetOuterLevel( self, id );
		if( up == 0 ) return id;
		code = DVM_GETVH; /* Host variable accessed from code sections: */
		break;
	case DAO_STATIC_VARIABLE : code = DVM_GETVS; break;
	case DAO_LOCAL_CONSTANT  : code = DVM_GETCL; break;
	case DAO_OBJECT_VARIABLE : code = DVM_GETVO; break;
	case DAO_CLASS_VARIABLE  : code = DVM_GETVK; break;
	case DAO_CLASS_CONSTANT  : code = DVM_GETCK; break;
	case DAO_GLOBAL_VARIABLE : code = DVM_GETVG; break;
	case DAO_GLOBAL_CONSTANT : code = DVM_GETCG; break;
	default : break;
	}
	self->usingGlobal |= code == DVM_GETVG;
	/*
	   printf( "i = %i %s %i\n", i, DaoVmCode_GetOpcodeName(get), leftval );
	 */
	regc = DaoParser_PushRegister( self );
	DaoVmCode_Set( & vmc, code, up, id, regc, self->lexLevel, line, first, mid, last );
	DaoParser_PushBackCode( self, & vmc );
	return regc;
}

int DaoParser_PostParsing( DaoParser *self )
{
	DaoRoutine *routine = self->routine;
	DaoVmCodeX **vmCodes;
	int i, j, k;

	DaoRoutine_SetSource( routine, self->tokens, self->nameSpace );
	if( DaoParser_SetupBranching( self ) == 0 ) return 0;

	routine->body->regCount = self->regCount;

	vmCodes = self->vmCodes->items.pVmc;

	if( DaoRoutine_SetVmCodes( routine, self->vmCodes ) ==0) return 0;

	if( self->byteBlock ) DaoByteCoder_FinalizeRoutineBlock( self->byteCoder, self->byteBlock );
	/*
	   DaoRoutine_PrintCode( routine, self->vmSpace->errorStream );
	 */
	return 1;
}
int DaoNamespace_CyclicParent( DaoNamespace *self, DaoNamespace *parent );
int DaoParser_ParseLoadStatement( DaoParser *self, int start, int end )
{
	DaoNamespace *mod = NULL, *nameSpace = self->nameSpace;
	DaoRoutine *mainRout = nameSpace->mainRoutine;
	DaoVmSpace *vmSpace = self->vmSpace;
	DaoToken **tokens = self->tokens->items.pToken;
	DString *modname = NULL;
	int i = start+1, j, code = 0, cyclic = 0;
	int perm = self->permission;
	unsigned char tki;

	DString_Clear( self->mbs );

	if( i > end ) goto ErrorLoad;
	tki = tokens[i]->name;
	if( tki == DTOK_MBS || tki == DTOK_WCS ){
		DString_SubString( & tokens[i]->string, self->mbs, 1, tokens[i]->string.size-2 );
		i ++;
	}else if( tki == DKEY_AS ){
		code = DAO_CTW_LOAD_INVALID;
		goto ErrorLoad;
	}else{
		while( i <= end && tokens[i]->type == DTOK_IDENTIFIER ){
			DString_Append( self->mbs, & tokens[i]->string );
			i ++;
			if( i <= end && (tokens[i]->type == DTOK_COLON2 || tokens[i]->type == DTOK_DOT) ){
				i ++;
				DString_AppendMBS( self->mbs, "/" );
			}else break;
		}
	}
	if( i <= end && tokens[i]->name == DKEY_AS ){
		if( (i+1) > end || tokens[i+1]->type != DTOK_IDENTIFIER ){
			code = DAO_CTW_LOAD_INVA_MOD_NAME;
			goto ErrorLoad;
		}
		modname = & tokens[i+1]->string;
		i += 2;
	}
	if( self->byteBlock ) DaoByteBlock_EncodeLoadStmt( self->byteBlock, self->mbs, modname );

	if( (mod = DaoNamespace_FindNamespace(nameSpace, self->mbs)) == NULL ){
		mod = DaoVmSpace_LoadModule( vmSpace, self->mbs );
		if( mod == NULL && modname == NULL ){
			mod = DaoVmSpace_FindModule( vmSpace, self->mbs );
			cyclic = mod && DaoNamespace_CyclicParent( mod, nameSpace );
			mod = NULL;
		}
	}

	nameSpace->mainRoutine = mainRout;
	DaoNamespace_SetConst( nameSpace, DVR_NSC_MAIN, (DaoValue*) mainRout );
	if( mod == NULL ){
		code = DAO_CTW_LOAD_FAILED;
		if( vmSpace->stopit ) code = DAO_CTW_LOAD_CANCELLED;
		goto ErrorLoad;
	}
	if( modname == NULL ){
		cyclic = (DaoNamespace_AddParent( nameSpace, mod ) == 0);
	}else{
		DaoNamespace_AddConst( nameSpace, modname, (DaoValue*) mod, perm );
	}
	if( cyclic ) DaoParser_Warn( self, DAO_LOAD_CYCLIC, NULL );

	/*
	   printf("ns=%p; mod=%p; myns=%p\n", ns, mod, nameSpace);
	 */
	DaoParser_CheckStatementSeparation( self, i-1, end );

	return i;
ErrorLoad:
	DaoParser_Error( self, code, NULL );
	if( code != DAO_CTW_LOAD_FAILED ) DaoParser_Error( self, DAO_CTW_LOAD_FAILED, NULL );
	return -1;
}

int DaoParser_ParseForLoop( DaoParser *self, int start, int end )
{
	DaoInode *opening, *closing, *inode;
	DaoToken *tok, **tokens = self->tokens->items.pToken;
	int semic1, semic2, reg1, reg2, fromCode, colon1, colon2;
	int pos, store = 0;
	int cst, forever = 0;
	int rb = -1;
	int in = -1;
	if( start+1 >= self->tokens->size ) return -1;
	if( tokens[start+1]->name == DTOK_LB )
		rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
	if( rb >= 0 ) in = DaoParser_FindOpenToken( self, DKEY_IN, start+2, rb, 0 );
	if( (rb < 0 || rb >= end) && in < 0 ) return -1;
	if( tokens[start+2]->name == DKEY_VAR ){
		store = DAO_DECL_LOCAL;
		start += 1;
	}

	DaoParser_AddScope( self, DVM_UNUSED, NULL );
	if( in >= 0 ){
		DArray *tuples; /* list of (list/iterable, iterator, item, first, last) */
		int k, L, elem, semic, regItemt, reg, first, firstIter;
		daoint *t;

		elem = start + 2;
		semic = DaoParser_FindOpenToken( self, DTOK_SEMCO, start+2, rb, 0 );
		if( semic < 0 && elem < rb ) semic = rb;
		first = 1;
		regItemt = 0;
		tuples = DArray_New(0);
		while( semic >=0 ){
			if( tokens[elem+1]->name != DKEY_IN ){
				DaoParser_Error( self, DAO_INVALID_FORIN, NULL );
				goto CleanUp;
			}
			tok = tokens[elem];
			reg = DaoParser_GetRegister( self, tok );
			if( reg < 0 || store ) DaoParser_DeclareVariable( self, tok, store, NULL );
			reg = DaoParser_GetRegister( self, tok );
			if( LOOKUP_ISCST( reg ) ){
				DaoParser_Error( self, DAO_EXPR_MODIFY_CONSTANT, & tok->string );
				goto CleanUp;
			}
			reg = DaoParser_GetNormRegister( self, reg, elem, 0, elem ); /* register for item */
			cst = 0;
			reg1 = DaoParser_MakeArithTree( self, elem+2, semic-1, & cst );
			if( reg1 < 0 ){
				DaoParser_Error( self, DAO_INVALID_FORIN, NULL );
				goto CleanUp;
			}
			DArray_Append( tuples, reg1 ); /* list */
			DArray_Append( tuples, reg ); /* item */
			DArray_Append( tuples, elem ); /* first token */
			DArray_Append( tuples, semic-1 ); /* last token */

			elem = semic + 1;
			semic = DaoParser_FindOpenToken( self, DTOK_SEMCO, elem, rb, 0 );
			if( semic < 0 && elem < rb ) semic = rb;
			first = 0;
		}
		L = tokens[rb]->line;
		fromCode = self->vmcCount;
		firstIter = self->regCount;
		for(k=0, t=tuples->items.pInt; k<tuples->size; k+=4, t+=4){
			daoint first = t[2], last = t[3];
			DaoParser_AddCode( self, DVM_ITER, t[0], 0, self->regCount, first, first+1,last );
			DaoParser_PushRegister( self );
		}
		/* see the comments for parsing if-else: */
		closing = DaoParser_AddCode( self, DVM_LABEL, 0, 1, 0, start, 0,0 );
		opening = DaoParser_AddScope( self, DVM_LOOP, closing );

		reg = DaoParser_PushRegister( self );
		DaoParser_AddCode( self, DVM_ITER, firstIter, tuples->size/4, reg, start, in, rb );
		DaoParser_AddCode( self, DVM_TEST, reg, fromCode, 0, start, in, rb );
		opening->jumpTrue = self->vmcLast;
		self->vmcLast->jumpFalse = closing;
		reg = DaoParser_PushRegister( self );
		DaoParser_AddCode( self, DVM_DATA, DAO_INTEGER, 0, reg, start, in, rb );
		for(k=0, t=tuples->items.pInt; k<tuples->size; k+=4, t+=4){
			daoint first = t[2], last = t[3];
			daoint iter = firstIter + (k/4);
			DaoParser_AddCode( self, DVM_GETI, t[0], iter, self->regCount, first, first+1, last );
			DaoParser_AddCode( self, DVM_MOVE, self->regCount, 0, t[1], first, 0, first );
			DaoParser_PushRegister( self );
		}

		start = rb+1;
		start = 1 + rb + DaoParser_AddScope2( self, rb+1 );
		DArray_Delete( tuples );
		return start;
CleanUp:
		DArray_Delete( tuples );
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
			DaoParser_Error( self, DAO_INVALID_FOR, self->mbs );
			return -1;
		}
		tok = tokens[start+2];
		index = DaoParser_GetRegister( self, tok );
		st = LOOKUP_ST( index );
		up = LOOKUP_UP( index );
		id = LOOKUP_ID( index );
		loc = index;
		if( index < 0 || store ){
			DaoParser_DeclareVariable( self, tok, store, NULL );
			index = DaoParser_GetRegister( self, tok );
			loc = index;
		}else if( LOOKUP_ISCST( st ) ){
			DString_SetMBS( self->mbs, "can not modify constant" );
			DaoParser_Error( self, DAO_INVALID_FOR, self->mbs );
			return -1;
		}else if( st >= DAO_LOCAL_CONSTANT ){
			loc = self->regCount;
			DaoParser_PushRegister( self );
		}
		first = DaoParser_MakeArithTree( self, eq+1, colon1-1, & cst );
		if( index < 0 || first <0 ) return -1;
		pos = tokens[eq]->line;
		if( colon1 + 1 == rb ){
			/* infinite looping */
			forever = 1;
			step = DaoParser_IntegerOne( self, colon1 );
		}else{
			colon2 = DaoParser_FindOpenToken( self, DTOK_COLON, colon1+1, rb, 0 );
			if( colon2 >= 0 ){
				step = DaoParser_MakeArithTree( self, colon1+1, colon2-1, & cst );
				last = DaoParser_MakeArithTree( self, colon2+1, rb-1, & cst );
			}else{
				step = DaoParser_IntegerOne( self, colon1 );
				last = DaoParser_MakeArithTree( self, colon1+1, rb-1, & cst );
			}
			if( step < 0 || last <0 ) return -1;
		}
		DaoParser_AddCode( self, DVM_MOVE, first, 0, loc, start+2, eq, colon1-1 );
		switch( st ){
		case DAO_LOCAL_VARIABLE  :
			if( up ){
				up = 0;
				set = DVM_SETVH;
			}else if( (up = DaoParser_GetOuterLevel( self, id )) > 0 ){
				set = DVM_SETVH;
			}
			break;
		case DAO_OBJECT_VARIABLE : set = DVM_SETVO; break;
		case DAO_CLASS_VARIABLE  : set = DVM_SETVK; break;
		case DAO_GLOBAL_VARIABLE : set = DVM_SETVG; break;
		}
		self->usingGlobal |= set == DVM_SETVG;
		if( set ) DaoParser_AddCode( self, set, loc, id, up, start+2, eq, colon1 );

		pos = tokens[colon1]->line;
		/* see the comments for parsing if-else: */
		inode = DaoParser_AddCode( self, DVM_GOTO, 0, 0, 0, start+2, eq, rb-1 );
		closing = DaoParser_AddCode( self, DVM_LABEL, 0, 1, 0, start, 0,0 );
		opening = DaoParser_AddScope( self, DVM_LOOP, closing );
		DaoParser_AddCode( self, DVM_ADD, loc, step, loc, start+2, eq, rb-1 );
		if( set ) DaoParser_AddCode( self, set, loc, id, up, start+2, eq, rb-1 );
		if( forever ){
			DaoParser_AddCode( self, DVM_NOP, 0, 0, 0, start+2, eq, rb-1 );
			inode->jumpTrue = self->vmcLast;
		}else{
			DaoParser_AddCode( self, DVM_LE, loc, last, self->regCount, start+2, eq, rb-1 );
			inode->jumpTrue = self->vmcLast;
			DaoParser_AddCode( self, DVM_TEST, self->regCount, 0, 0, start, colon1, rb );
		}
		opening->jumpTrue = self->vmcLast;
		self->vmcLast->jumpFalse = closing;
		DaoParser_PushRegister( self );
		goto AddScope;
	}
	semic1 = DaoParser_FindOpenToken( self, DTOK_SEMCO, start+2, rb, 1 );
	semic2 = DaoParser_FindOpenToken( self, DTOK_SEMCO, semic1+1, rb, 1 );
	if( rb <0 || semic1 <0 || semic2 <0 ){
		DaoParser_Error( self, DAO_INVALID_FOR, NULL );
		return -1;
	}
	/* init arith; */
	cst = 0;
	pos = DaoParser_ParseVarExpressions( self, start+2, semic1-1, 0, store, store );
	if( pos < 0 ) return -1;
	if( pos != semic1 ){
		DaoParser_Error2( self, DAO_INVALID_EXPRESSION, start+2, semic1-1, 0 );
		return -1;
	}
	/* see the comments for parsing if-else: */
	inode = DaoParser_AddCode( self, DVM_GOTO, 0, 0, 0, start+2, 0,0 );
	closing = DaoParser_AddCode( self, DVM_LABEL, 0, 1, 0, start, 0,0 );
	opening = DaoParser_AddScope( self, DVM_LOOP, closing );
	/* step arith */
	if( semic2 + 1 == rb ){
		DaoParser_AddCode( self, DVM_NOP, 0, 0, 0, semic2+1, 0, rb-1 );
	}else{
		DaoEnode enode;
		self->curToken = semic2 + 1;
		enode = DaoParser_ParseExpression( self, DTOK_COMMA );
		while( enode.reg >= 0 && self->curToken < rb ){
			self->curToken += 1;
			enode = DaoParser_ParseExpression( self, DTOK_COMMA );
		}
		if( enode.reg < 0 ){
			DaoParser_Error2( self, DAO_INVALID_EXPRESSION, semic2+1, rb-1, 0 );
			return -1;
		}
	}
	/* cond airth */
	DaoParser_AddCode( self, DVM_UNUSED, 0, 0, 0, start, 0, 0 );
	inode->jumpTrue = self->vmcLast;
	if( semic1 + 1 == semic2 ){
		DaoParser_AddCode( self, DVM_NOP, 0, 0, 0, start, semic1, rb );
	}else{
		reg1 = DaoParser_MakeArithTree( self, semic1+1, semic2-1, & cst );
		if( reg1 < 0 ) return -1;
		DaoParser_AddCode( self, DVM_TEST, reg1, 0, 0, start, semic1, rb );
	}
	opening->jumpTrue = self->vmcLast;
	self->vmcLast->jumpFalse = closing;

AddScope:
	return 1 + rb + DaoParser_AddScope2( self, rb+1 );
}
/* Parse a condition test expression: */
int DaoParser_ParseCondition( DaoParser *self, int start, int dec, DaoInode *opening )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int from = self->vmcCount;
	int pos, semico, lb = start, rb = -1;
	int reg, cst = 0, store = 0;

	if( start < self->tokens->size && tokens[start]->name == DTOK_LB ){
		rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, -1 );
	}else{
		DString_SetMBS( self->mbs, "()" );
		DaoParser_Error( self, DAO_CTW_IS_EXPECTED, self->mbs );
	}
	if( lb < 0 || rb < 0 ) return -1;
	if( opening ) DaoParser_AppendCode( self, opening ); /* move to back */

	start = lb + 1;
	semico = DaoParser_FindOpenToken( self, DTOK_SEMCO, start, rb, 0 );
	if( dec && semico >= 0 ){
		if( tokens[start]->name == DKEY_VAR ){
			store = DAO_DECL_LOCAL;
			start += 1;
		}
		pos = DaoParser_ParseVarExpressions( self, start, rb-1, 0, store, store );
		if( pos < 0 ) return -1;
		if( pos != semico ){
			DaoParser_Error4( self, DAO_TOKEN_EXPECTING, tokens[pos]->line, ";" );
			return -1;
		}
		start = semico + 1;
	}

	reg = DaoParser_MakeArithTree( self, start, rb-1, & cst );
	if( reg < 0 ) return -1;
	DaoParser_AddCode( self, DVM_TEST, reg, from, 0, start, 0, rb );
	return rb;
}

static int DaoParser_AddFieldConst( DaoParser *self, DString *field )
{
	DString_SetMBS( self->mbs, "." );
	DString_Append( self->mbs, field );
	if( MAP_Find( self->allConsts, self->mbs )==NULL ){
		DaoString str = {DAO_STRING,0,0,0,0,NULL};
		str.data = field;
		MAP_Insert( self->allConsts, self->mbs, self->routine->routConsts->items.size );
		DaoRoutine_AddConstant( self->routine, (DaoValue*) & str );
	}
	return MAP_Find( self->allConsts, self->mbs )->value.pInt;
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
		case DAO_LIST : // XXX
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
static DaoValue* DaoParseNumber( DaoParser *self, DaoToken *tok, DaoValue *value )
{
	char *str = tok->string.mbs;
	daoint pl = 0;
	if( tok->name == DTOK_SINGLE_DEC ){
		value->type = DAO_FLOAT;
		value->xFloat.value = strtod( str, 0 );
	}else if( tok->name >= DTOK_DOUBLE_DEC && tok->name <= DTOK_NUMBER_SCI ){
		value->type = DAO_DOUBLE;
		value->xDouble.value = strtod( str, 0 );
	}else if( tok->name == DTOK_NUMBER_IMG ){
		str[tok->string.size-1] = '\0';
		value->type = DAO_COMPLEX;
		value->xComplex.value.real = 0;
		value->xComplex.value.imag = strtod( str, 0 );
		str[tok->string.size-1] = 'C';
	}else{
		value->type = DAO_INTEGER;
		value->xInteger.value = (sizeof(daoint) == 4) ? strtol( str, 0, 0 ) : strtoll( str, 0, 0 );
	}
	return value;
}
static int DaoParser_ParseAtomicExpression( DaoParser *self, int start, int *cst )
{
	DaoValue buffer = {0};
	DaoToken **tokens = self->tokens->items.pToken;
	DaoNamespace *ns = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DaoValue *value = NULL;
	DString *str = & tokens[start]->string;
	DNode *node;
	int varReg;
	char *tok = tokens[start]->string.mbs;
	unsigned char tki = 0;

	/*printf("DaoParser_ParseAtomicExpression()\n"); */

	memset( & buffer, 0, sizeof(DaoValue) );

	*cst = 0;
	varReg = DaoParser_GetRegister( self, tokens[start] );
	tki = tokens[start]->name;


	/*
	   printf("name=%s; %i\n", tok, varReg);
	 */
	if( varReg >= 0 ){
		int st = LOOKUP_ST( varReg );
		if( LOOKUP_ISCST( varReg ) ){
			*cst = varReg;
			value = DaoParser_GetVariable( self, varReg );
		}
		if( self->byteBlock && (st == DAO_CLASS_CONSTANT || st == DAO_GLOBAL_CONSTANT) ){
			int opcode = (st == DAO_CLASS_CONSTANT) ? DVM_GETCK : DVM_GETCG;
			DaoByteBlock *eval, *block = self->byteBlock;
			if( value && value->type >= DAO_ENUM ){
				if( DaoByteBlock_FindObjectBlock( block, value ) == NULL ){
					DaoByteBlock *name = DaoByteBlock_EncodeString( block, str );
					eval = DaoByteBlock_AddEvalBlock( block, value, opcode, 1, NULL );
					DaoByteBlock_InsertBlockIndex( eval, eval->end, name );
				}
			}
		}
		/*
		   printf("value = %i; %i; c : %i\n", value->type, varReg, *cst );
		 */
	}else if( tki == DTOK_MBS || tki == DTOK_WCS || tki == DTOK_VERBATIM ){
		if( (node = MAP_Find( self->allConsts, str )) == NULL ){
			DaoString dummy = {DAO_STRING,0,0,0,0,NULL};
			int wcs = tok[0] == '"';
			dummy.data = self->str;
			DString_ToMBS( self->str );
			if( tki == DTOK_VERBATIM ){
				daoint pos = DString_FindChar( str, ']', 1 );
				DString_SetDataMBS( self->str, tok + pos + 1, str->size - 2*(pos + 1) );
				wcs = tok[1] == '@';
			}else{
				DString_SetDataMBS( self->str, tok + 1, str->size-2 );
			}
			if( daoConfig.wcs || (daoConfig.mbs == 0 && wcs) ) DString_ToWCS( self->str );
			node = MAP_Insert( self->allConsts, str, routine->routConsts->items.size );
			DaoRoutine_AddConstant( routine, (DaoValue*) & dummy );
		}
		varReg = LOOKUP_BIND_LC( node->value.pInt );
		*cst = varReg;
	}else if( tki >= DTOK_DIGITS_DEC && tki <= DTOK_NUMBER_IMG ){
		if( (node = MAP_Find( self->allConsts, str )) == NULL ){
			value = DaoParseNumber( self, tokens[start], & buffer );
			if( value == NULL ) return -1;
			node = MAP_Insert( self->allConsts, str, routine->routConsts->items.size );
			DaoRoutine_AddConstant( routine, value );
		}
		*cst = LOOKUP_BIND_LC( node->value.pInt );
		value = routine->routConsts->items.items.pValue[ node->value.pInt ];
		varReg = *cst;
	}else if( tki == DTOK_ID_SYMBOL ){
		DaoType *type = DaoNamespace_FindType( ns, str );
		if( type == NULL ){
			type = DaoType_New( str->mbs, DAO_ENUM, NULL, NULL );
			type->mapNames = DMap_New(D_STRING,0);
			DString_Assign( self->mbs, str );
			DString_Erase( self->mbs, 0, 1 );
			DMap_Insert( type->mapNames, self->mbs, (void*)0 );
			DaoNamespace_AddType( ns, str, type );
		}
		if( (node = MAP_Find( self->allConsts, str )) == NULL ){
			value = (DaoValue*) self->denum;
			self->denum->value = 0;
			DaoEnum_SetType( self->denum, type );
			node = MAP_Insert( self->allConsts, str, routine->routConsts->items.size );
			DaoRoutine_AddConstant( routine, (DaoValue*) self->denum );
		}
		varReg = LOOKUP_BIND_LC( node->value.pInt );
		*cst = varReg;
	}else if( tki == DTOK_COLON ){
		if( (node = MAP_Find( self->allConsts, str )) == NULL ){
			DaoTuple *tuple = DaoNamespace_MakePair( ns, dao_none_value, dao_none_value );
			tuple->trait = 0;
			node = MAP_Insert( self->allConsts, str, routine->routConsts->items.size );
			DaoRoutine_AddConstant( routine, (DaoValue*) tuple );
		}
		varReg = LOOKUP_BIND_LC( node->value.pInt );
		*cst = varReg;
	}else{
		*cst = 0;
		DaoParser_Error( self, DAO_SYMBOL_NOT_DEFINED, str );
		return -1;
	}
	if( value && value->type == DAO_INTEGER && (value->xInteger.value >> 16) == 0 ){
		varReg = DaoParser_PushRegister( self );
		DaoParser_AddCode( self, DVM_DATA, DAO_INTEGER, value->xInteger.value, varReg, start, 0, 0 );
	}else if( value && value->type == DAO_FLOAT && value->xFloat.value == 0.0 ){
		varReg = DaoParser_PushRegister( self );
		DaoParser_AddCode( self, DVM_DATA, DAO_FLOAT, 0, varReg, start, 0, 0 );
	}else if( value && value->type == DAO_DOUBLE && value->xDouble.value == 0.0 ){
		varReg = DaoParser_PushRegister( self );
		DaoParser_AddCode( self, DVM_DATA, DAO_DOUBLE, 0, varReg, start, 0, 0 );
	}
	return DaoParser_GetNormRegister( self, varReg, start, 0, start );
}

static int DaoParser_ExpClosure( DaoParser *self, int start )
{
	char name[100];
	daoint offset, regCall, opc, rb = 0;
	daoint i, k, n, end = self->tokens->size-1;
	daoint tokPos = self->tokens->items.pToken[ start ]->line;
	DString *mbs = DaoParser_GetString( self );
	DaoNamespace *myNS = self->nameSpace;
	DaoRoutine *routine = self->routine;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoRoutine *rout;
	DaoParser *parser;
	DArray *uplocs;
	DNode *it;

	parser = DaoVmSpace_AcquireParser( self->vmSpace );
	rout = DaoRoutine_New( myNS, NULL, 1 );
	parser->routine = rout;
	parser->levelBase = self->levelBase + self->lexLevel + 1;
	parser->nameSpace = self->nameSpace;
	parser->vmSpace = self->vmSpace;
	parser->outerParser = self;
	if( parser->uplocs == NULL ) parser->uplocs = DArray_New(0);
	uplocs = parser->uplocs;
	DString_Assign( parser->fileName, self->fileName );
	if( self->hostClass ){
		GC_ShiftRC( self->hostClass->objType, rout->routHost );
		rout->routHost = self->hostClass->objType;
	}
	DArray_Append( myNS->definedRoutines, rout );

	if( self->byteBlock ){
		parser->byteCoder = self->byteCoder;
		parser->byteBlock = DaoByteBlock_AddRoutineBlock( self->byteBlock, parser->routine, 0 );
	}
	if( tokens[start]->name == DKEY_DEFER ){
		int offset = start + 1;
		rout->attribs |= DAO_ROUT_DEFERRED;
		if( tokens[offset]->name == DTOK_LB ){
			offset += 1;
			if( tokens[offset]->name == DTOK_IDENTIFIER ){
				DString *name = & tokens[offset]->string;
				int i = LOOKUP_BIND( DAO_STATIC_VARIABLE, 0, 0, 0 );
				MAP_Insert( DaoParser_GetCurrentDataMap( parser ), name, i );
				DArray_Append( rout->body->svariables, DaoVariable_New(NULL,NULL) );
				rout->attribs |= DAO_ROUT_PASSRET;
				offset += 1;
				if( parser->byteBlock ){
					DaoByteBlock_DeclareStatic( parser->byteBlock, NULL, NULL, NULL, 0 );
				}
			}
			if( tokens[offset]->name != DTOK_RB ) goto ErrorParsing;
			offset += 1;
		}
		GC_ShiftRC( dao_type_routine, rout->routType );
		rout->routType = dao_type_routine;
		rb = DaoParser_ExtractRoutineBody( self, parser, offset );
		if( rb < 0 ) goto ErrorParsing;
	}else if( tokens[start+1]->name == DTOK_LB ){
		rb = DaoParser_ParseSignature( self, parser, DKEY_ROUTINE, start );
	}else if( tokens[start+1]->name == DTOK_LCB ){
		GC_ShiftRC( dao_type_routine, rout->routType );
		rout->routType = dao_type_routine;
		rb = DaoParser_ExtractRoutineBody( self, parser, start+1 );
		if( rb < 0 ) goto ErrorParsing;
	}else{
		goto ErrorParsing;
	}
	offset = rb - parser->tokens->size;

	/* Routine name may have been changed by DaoParser_ParseSignature() */
	sprintf( name, "AnonymousFunction_%p", rout );
	DString_SetMBS( rout->routName, name );
	if( rb < 0 || tokens[rb]->name != DTOK_RCB ){
		DaoParser_Error( self, DAO_CTW_INVA_SYNTAX, NULL );
		goto ErrorParsing;
	}
	if( tokens[rb-1]->name != DTOK_SEMCO ){
		DaoToken *tok = DArray_Append( parser->tokens, tokens[rb-1] );
		tok->type = tok->name = DTOK_SEMCO;
		DString_SetMBS( & tok->string, ";" );
	}
	if( ! DaoParser_ParseRoutine( parser ) ) goto ErrorParsing;

	regCall = self->regCount;
	DaoParser_PushRegister( self );
	i = DaoRoutine_AddConstant( routine, (DaoValue*)rout );
	DaoParser_AddCode( self, DVM_GETCL, 0, i, regCall, start, rb, end );

	for(i=0; i<uplocs->size; i+=4 ){
		int up = uplocs->items.pInt[i];
		int loc = uplocs->items.pInt[i+1];
		int first = uplocs->items.pInt[i+2] + offset;
		int last = uplocs->items.pInt[i+3] + offset;
		DaoParser_AddCode( self, DVM_MOVE, up, 0, regCall+1+i/2, first, 0, last );
		DaoParser_AddCode( self, DVM_DATA, DAO_INTEGER, loc, regCall+2+i/2, first, 0, last );
		if( parser->byteBlock ){
			DaoByteBlock_DeclareStatic( parser->byteBlock, NULL, NULL, NULL, 0 );
		}
	}
	DaoParser_PushRegisters( self, uplocs->size/2 );

	self->curToken = rb + 1;
	opc = DaoParser_PushRegister( self );
	/* DVM_ROUTINE rout_proto, upv1, upv2, ..., opc */
	DaoParser_AddCode( self, DVM_ROUTINE, regCall, uplocs->size/2, opc, start, rb, end );
	DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	return opc;
ErrorParsing:
	DString_SetMBS( mbs, "invalid anonymous function" );
	DaoParser_Error( self, DAO_CTW_INVA_SYNTAX, mbs );
	DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	GC_IncRC( rout );
	GC_DecRC( rout );
	return -1;
}
static int DaoParser_ClassExpression( DaoParser *self, int start )
{
	DaoParser_Error( self, DAO_CTW_OBSOLETE_SYNTAX, NULL );
	return -1;
}

int DaoParser_MakeArithTree( DaoParser *self, int start, int end, int *cst )
{
	int i, reg, stop = 0;
	DaoEnode enode;
	DaoToken **tokens = self->tokens->items.pToken;
#if 0
	printf("MakeArithTree(): start = %i; end = %i;\n", start, end );
	for( i=start;i<=end;i++) printf("%s  ", tokens[i]->string->mbs); printf("\n");
#endif
	if( (end+1) < self->tokens->size ){
		i = tokens[end+1]->name;
		if( i == DTOK_COLON || i == DTOK_DOTS ) stop = i;
	}
	self->curToken = start;
	enode = DaoParser_ParseExpression( self, stop );
	reg = enode.reg;
	*cst = enode.konst;
	if( self->curToken != end+1 ) reg = -1;
	if( reg < 0 ) DaoParser_Error2( self, DAO_INVALID_EXPRESSION, start, end, 0 );
	return reg;
}


int DaoParser_GetOperPrecedence( DaoParser *self )
{
	DOper oper;
	DaoToken **tokens = self->tokens->items.pToken;
	if( self->curToken >= self->tokens->size ) return -1;
	if( (self->curToken+1) < self->tokens->size ){
		DaoToken *t1 = tokens[self->curToken];
		DaoToken *t2 = tokens[self->curToken+1];
		if( t1->line == t2->line && (t1->cpos+1) == t2->cpos ){
			/* check for operators: <<, >>, <=, >= */
			int newtok = 0;
			switch( ((int)t1->type<<8)|t2->type ){
			case (DTOK_LT<<8)|DTOK_LT : newtok = DTOK_LSHIFT; break;
			case (DTOK_GT<<8)|DTOK_GT : newtok = DTOK_RSHIFT; break;
			case (DTOK_LT<<8)|DTOK_ASSN : newtok = DTOK_LE; break;
			case (DTOK_GT<<8)|DTOK_ASSN : newtok = DTOK_GE; break;
			}
			if( newtok ){
				DString_Insert( & t2->string, & t1->string, 0, 0, 1 );
				t1->string.mbs[0] = '\0';
				t1->string.size = 0;
				t1->type = t1->name = DTOK_BLANK;
				t2->type = t2->name = newtok;
				t2->cpos = t1->cpos;
				self->curToken += 1;
			}
		}else if( t1->name == DKEY_NOT && t2->name == DKEY_IN ){
			DString_AppendChar( & t1->string, ' ' );
			DString_Insert( & t2->string, & t1->string, 0, 0, 4 );
			t1->string.mbs[0] = '\0';
			t1->string.size = 0;
			t1->type = t1->name = DTOK_BLANK;
			t2->type = t2->name = DTOK_NOTIN;
			t2->cpos = t1->cpos;
			self->curToken += 1;
		}
	}
	if( self->curToken > 0 ){
		DaoToken *t1 = tokens[self->curToken-1];
		DaoToken *t2 = tokens[self->curToken];
		if( t1->line != t2->line ) return -1;
	}
	oper = daoArithOper[tokens[self->curToken]->name];
	if( oper.oper == 0 || oper.binary == 0 ) return -1;
	return 10*(20 - oper.binary);
}
static DaoInode* DaoParser_InsertCode( DaoParser *self, DaoInode *after, int code, int a, int b, int c, int first )
{
	DaoInode *node = DaoParser_NewNode( self );
	node->code = code;
	node->a = a;
	node->b = b;
	node->c = c;
	node->first = first;
	node->jumpTrue = node->jumpFalse = NULL;
	node->prev = after;
	node->next = after->next;
	if( after->next ) after->next->prev = node;
	after->next = node;
	if( self->vmcLast->next ) self->vmcLast = node;
	return node;
}
static DaoInode* DaoParser_AddBinaryCode( DaoParser *self, int code, DaoEnode *LHS, DaoEnode *RHS, int mid )
{
	int opa = LHS->reg;
	int opb = RHS->reg;
	int first = mid - 1;
	int last = mid + 1;
	int regc = DaoParser_PushRegister( self );
	if( LHS->first ) first = LHS->first->first;
	if( RHS->last ) last = RHS->last->first + RHS->last->last;
	if( code < 0 ){
		code = -code;
		if( code != DVM_IN ){
			int ca = LHS->konst;
			int cb = RHS->konst;
			LHS->konst = cb;
			RHS->konst = ca;
			opa = RHS->reg;
			opb = LHS->reg;
		}
	}
	DaoParser_AddCode( self, code, opa, opb, regc, first, mid, last );
	return self->vmcLast;
}
/* list, map, matrix, tuple expressions: */
/* { a=>1, b=>[] }; {1,2,10}; {1:2:10}; {1:10}; [1,2,10]; [1:2:10]; [1:10] */
DaoEnode DaoParser_ParseEnumeration( DaoParser *self, int etype, int btype, int lb, int rb )
{
	DArray *cid = DaoParser_GetArray( self );
	DaoEnode enode, result = { -1, 0, 1, NULL, NULL, NULL, NULL };
	DaoInode *back = self->vmcLast;
	DaoType *tp = self->enumTypes->size ? self->enumTypes->items.pType[0] : 0;
	int start = lb - 1 - (etype != 0);
	int mid = lb, end = rb + 1;
	int regcount = self->regCount;
	int enumcode = DVM_LIST;
	int pto = DaoParser_FindOpenToken( self, DTOK_FIELD, lb, rb, 0 );
	int appxto = DaoParser_FindOpenToken( self, DTOK_APPXTO, lb, rb, 0 );
	int arrow = DaoParser_FindOpenToken( self, DTOK_ARROW, lb, rb, 0 );
	int colon = DaoParser_FindOpenToken( self, DTOK_COLON, lb, rb, 0 );
	int semi = DaoParser_FindOpenToken( self, DTOK_SEMCO, lb, rb, 0 );
	int comma = DaoParser_FindOpenToken( self, DTOK_COMMA, lb, rb, 0 );
	int isempty = 0, step = 0;
	int regC;

	if( btype == DTOK_LSB ) enumcode = DVM_VECTOR;
	if( etype == DKEY_ARRAY ) enumcode = DVM_VECTOR;
	result.prev = self->vmcLast;
	enode = result;

#ifndef DAO_WITH_NUMARRAY
	if( enumcode == DVM_VECTOR ){
		DaoParser_Error( self, DAO_DISABLED_NUMARRAY, NULL );
		goto ParsingError;
	}
#endif

	self->curToken = lb;
	if( etype == DKEY_TUPLE || btype == DTOK_LB ){
		/* ( a, b ) */
		if( tp && tp->tid != DAO_TUPLE ) goto ParsingError;
		enode = DaoParser_ParseExpressionList2( self, DTOK_COMMA, NULL, cid, DAO_EXPRLIST_TUPLE );
		if( enode.reg < 0 || self->curToken != end ) goto ParsingError;
		regC = DaoParser_PushRegister( self );
		enumcode = DVM_TUPLE;
		DaoParser_AddCode( self, DVM_TUPLE, enode.reg, enode.count, regC, start, mid, end );
	}else if( (etype == DKEY_MAP || etype == 0) && btype == DTOK_LCB && appxto >= 0 ){
		DString w = DString_WrapMBS( "using ~> for hash map, please use -> instead." );
		DaoParser_Error( self, DAO_CTW_OBSOLETE_SYNTAX, & w );
		regC = -1;
	}else if( etype == DKEY_MAP || (etype == 0 && btype == DTOK_LCB && (pto >= 0 || arrow >= 0) ) ){
		/* { a=>1, b=>[] }; {=>}; */
		/* { a->1, b->[] }; {->}; */
		if( tp && tp->tid != DAO_MAP ) goto ParsingError;
		enumcode = pto >= 0 ? DVM_MAP : DVM_HASH;
		if( etype == DKEY_MAP && arrow < 0 ) enumcode = DVM_MAP;
		isempty = lb >= rb;
		if( lb >= rb ){
			if( self->needConst ){
				DaoMap *hm = DaoMap_New(colon>=0);
				hm->ctype = tp ? tp : dao_type_map_any;
				GC_IncRC( hm->ctype );
				regC = DaoRoutine_AddConstant( self->routine, (DaoValue*) hm );
				enode.konst = LOOKUP_BIND_LC( regC );
				enode.count = 0;
				regC = DaoParser_GetNormRegister( self, enode.konst, start, 0, end );
			}else{
				regC = DaoParser_PushRegister( self );
				DaoParser_AddCode( self, enumcode, regC, 0, regC, start, mid, end );
			}
		}else{
			int sep = pto >= 0 ? DTOK_FIELD : DTOK_ARROW;
			step = 2;
			enode = DaoParser_ParseExpressionLists( self, sep, DTOK_COMMA, & step, cid );
			if( enode.reg < 0 || self->curToken != end ) goto ParsingError;
			regC = DaoParser_PushRegister( self );
			DaoParser_AddCode( self, enumcode, enode.reg, enode.count, regC, start, mid, end );
		}
	}else if( colon > lb && comma < 0 && semi < 0 ){
		/* arithmetic progression: [ 1 : 2 : 10 ]; [ 1 : 10 ] */
		if( tp && (enumcode == DVM_LIST && tp->tid != DAO_LIST) ) goto ParsingError;
		if( tp && (enumcode == DVM_VECTOR && tp->tid != DAO_ARRAY) ) goto ParsingError;
		enode = DaoParser_ParseExpressionList2( self, DTOK_COLON, NULL, cid, enumcode == DVM_VECTOR );
		if( enode.reg < 0 || self->curToken != end ) goto ParsingError;
		isempty = lb > rb;
		if( enode.reg < 0 || enode.count < 2 || enode.count > 3 ){
			DaoParser_Error( self, DAO_CTW_ENUM_INVALID, NULL );
			goto ParsingError;
		}
		if( enumcode == DVM_LIST ) enumcode = DVM_APLIST;
		if( enumcode == DVM_VECTOR ) enumcode = DVM_APVECTOR;
		regC = DaoParser_PushRegister( self );
		DaoParser_AddCode( self, enumcode, enode.reg, enode.count, regC, start, mid, end );
	}else if( semi < 0 ){
		/* [a,b,c] */
		if( tp && (enumcode == DVM_LIST && tp->tid != DAO_LIST) ) goto ParsingError;
		if( tp && (enumcode == DVM_VECTOR && tp->tid != DAO_ARRAY) ) goto ParsingError;
		enode = DaoParser_ParseExpressionList2( self, DTOK_COMMA, NULL, cid, enumcode == DVM_VECTOR );
		if( enode.reg < 0 || self->curToken != end ) goto ParsingError;
		isempty = lb > rb;
		regC = DaoParser_PushRegister( self );
		DaoParser_AddCode( self, enumcode, enode.reg, enode.count, regC, start, mid, end );
	}else if( etype == DKEY_ARRAY || (etype == 0 && btype == DTOK_LSB) ){
		/* [1,2; 3,4] */
		int row = 0, col = 0;
		if( tp && (enumcode == DVM_VECTOR && tp->tid != DAO_ARRAY) ) goto ParsingError;
		enumcode = DVM_MATRIX;
		isempty = lb > rb;
		enode = DaoParser_ParseExpressionLists( self, DTOK_COMMA, DTOK_SEMCO, & step, cid );
		if( enode.reg < 0 || self->curToken != end ) goto ParsingError;
		col = step;
		if( enode.count && col ) row = enode.count / col;
		if( row >= 255 || col >= 255 ){
			DaoParser_Error( self, DAO_CTW_ENUM_LIMIT, NULL );
			goto ParsingError;
		}
		regC = DaoParser_PushRegister( self );
		DaoParser_AddCode( self, DVM_MATRIX, enode.reg, ((row<<8)|col), regC, start, mid, end );
	}else{
		regC = -1;
	}
	if( regC < 0 ){
		DaoParser_Error( self, DAO_CTW_ENUM_INVALID, NULL );
		goto ParsingError;
	}
	if( colon >= 0 && btype != DTOK_LCB ) enode.konst = 0;
	if( self->needConst && enode.konst == enode.count ){
		regC = DaoParser_MakeEnumConst( self, & enode, cid, regcount );
	}else if( enode.count ){
		enode.konst = 0;
	}
	if( self->enumTypes->size ){
		tp = self->enumTypes->items.pType[0];
		if( tp && tp->tid != DAO_ANY ) MAP_Insert( self->routine->body->localVarType, regC, tp );
	}
	result.reg = regC;
	result.konst = enode.konst;
	result.first = result.last = result.update = self->vmcLast;
	self->vmcValue = self->vmcLast;
	if( back->next ) result.first = back->next;
	return result;
ParsingError:
	DaoParser_Error2( self, DAO_INVALID_EXPRESSION, self->curToken, end, 0 );
	return result;
}
static DaoEnode DaoParser_ParseParenthesis( DaoParser *self )
{
	DaoEnode enode, result = { -1, 0, 1, NULL, NULL, NULL, NULL };
	DaoToken **tokens = self->tokens->items.pToken;
	DaoInode *back = self->vmcLast;
	int start = self->curToken;
	int end = self->tokens->size-1;
	int rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, end );
	int comma = DaoParser_FindOpenToken( self, DTOK_COMMA, start+1, end, 0 );
	int maybeType = tokens[start+1]->type == DTOK_IDENTIFIER;
	int regC;

	result.prev = self->vmcLast;
	if( rb > 0 && rb < end && maybeType && tokens[rb]->line == tokens[rb+1]->line ){
		int count = self->errors->size;
		self->curToken = rb + 1;
		enode = DaoParser_ParsePrimary( self, 0, 0 );
		if( enode.reg >= 0 ){
			/* type casting expression */
			int it, newpos = 0;
			DaoType *abtp = DaoParser_ParseType( self, start+1, rb-1, & newpos, NULL );
			if( abtp == NULL || newpos != rb ){
				GC_IncRC( abtp );
				GC_DecRC( abtp );
				goto ParseNoCasting;
			}
			regC = DaoParser_PushRegister( self );
			MAP_Insert( self->routine->body->localVarType, regC, abtp );
			it = DaoRoutine_AddConstant( self->routine, (DaoValue*) abtp );
			DaoParser_AddCode( self, DVM_CAST, enode.reg, it, regC, start, rb, self->curToken-1 );
			result.reg = regC;
			result.first = back->next;
			result.last = result.update = self->vmcLast;
			return result;
		}
ParseNoCasting:
		DArray_Erase( self->errors, count, -1 );
	}
	self->curToken = start + 1;
	if( rb >=0 && comma >= 0 && comma < rb ){
		/* tuple enumeration expression */
		result = DaoParser_ParseEnumeration( self, 0, DTOK_LB, start+1, rb-1 );
		self->curToken = rb + 1;
		return result;
	}
	result = DaoParser_ParseExpression( self, 0 );
	if( result.reg < 0 ) return result;
	if( DaoParser_CheckTokenType( self, DTOK_RB, ")" ) == 0 ) goto ParsingError;
	self->curToken += 1;
	return result;
ParsingError:
	self->curToken = start;
	DaoParser_Error3( self, DAO_INVALID_EXPRESSION, start );
	result.reg = -1;
	return result;
}


static DaoEnode DaoParser_ParsePrimary( DaoParser *self, int stop, int eltype )
{
	DString *name;
	DaoValue *dbase;
	DaoValue *value = NULL;
	DaoInode *last = self->vmcLast;
	DaoEnode enode, result = { -1, 0, 1, NULL, NULL, NULL, NULL };
	DaoEnode error = { -1, 0, 1, NULL, NULL, NULL, NULL };
	DaoRoutine *routine = self->routine;
	DaoToken **tokens = self->tokens->items.pToken;
	DaoString daostr = {DAO_STRING,0,0,0,1,NULL};
	DaoValue *svalue = (DaoValue*) & daostr;
	DString *mbs = self->mbs;
	unsigned char tkn, tki, tki2, tki3 = 0;
	int regcount = self->regCount;
	int size = self->tokens->size;
	int start = self->curToken;
	int end = size - 1;
	int regLast = -1;
	int reg, rb, cst = 0;

	/*
	int i; for(i=start;i<=end;i++) printf("%s  ", tokens[i]->string.mbs);printf("\n");
	 */
	result.prev = self->vmcLast;
	if( start >= size ) return result;
	tkn = tokens[start]->type;
	tki = tokens[start]->name;
	tki2 = DaoParser_NextTokenName( self );
	if( tki2 == stop ) tki2 = 0;
	if( (start + 2) <= end ) tki3 = tokens[start+2]->type;
	if( (tki == DTOK_IDENTIFIER || tki == DKEY_SELF || tki >= DKEY_RAND) && tki2 == DTOK_FIELD ){
		DaoType *type = self->enumTypes->size ? self->enumTypes->items.pType[0] : NULL;
		DaoString ds = {DAO_STRING,0,0,0,1,NULL};
		DaoValue *value = (DaoValue*) & ds;
		DString *field = & tokens[start]->string;

		if( !(eltype & DAO_EXPRLIST_TUPLE) ){
			DaoParser_Error2( self, DAO_INVALID_EXPRESSION, start, 2, 0 );
			return error;
		}

		ds.data = field;
		self->curToken += 2;
		if( type && type->tid == DAO_PAR_NAMED ) type = (DaoType*) type->aux;
		if( type ) DArray_PushFront( self->enumTypes, type );
		enode = DaoParser_ParseExpression( self, stop );
		DArray_PopFront( self->enumTypes );
		if( enode.reg < 0 ) return enode;
		if( enode.konst ){
			DaoValue *v2 = DaoParser_GetVariable( self, enode.konst );
			result.reg = DaoParser_MakeArithConst( self, DVM_NAMEVA, value, v2, & result.konst, last, regcount );
			if( result.reg < 0 ){
				DaoParser_Error( self, DAO_CTW_INV_CONST_EXPR, NULL );
				return error;
			}
		}else{
			reg = DaoRoutine_AddConstant( routine, value );
			result.reg = DaoParser_PushRegister( self );
			DaoParser_AddCode( self, DVM_NAMEVA, reg, enode.reg, result.reg, start, 1, 1 );
		}
		result.last = result.update = self->vmcLast;
		result.first = result.last;
		return result;
	}else if( tki == DKEY_TYPE && tki2 == DTOK_LB ){
		int start0 = start + 2;
		DaoType *type = DaoParser_ParseType( self, start + 2, end, & start, NULL );
		if( type == NULL || start > end || tokens[start]->type != DTOK_RB ){
			DaoParser_Error3( self, DAO_INVALID_TYPE_FORM, start0 );
			return error;
		}
		result.konst = LOOKUP_BIND_LC( DaoRoutine_AddConstant( routine, (DaoValue*)type ) );
		result.reg = DaoParser_GetNormRegister( self, result.konst, start, 0, self->curToken );
		result.first = last->next;
		result.last = result.update = self->vmcLast;
		start += 1;
	}else if( tki >= DKEY_ARRAY && tki <= DKEY_LIST && tki2 == DTOK_LCB ){
		int rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
		if( rb < 0 ) return result;
		result = DaoParser_ParseEnumeration( self, tki, DTOK_LCB, start+2, rb-1 );
		start = rb + 1;
	}else if( tki == DTOK_LCB ){
		int rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
		if( rb < 0 ) return result;
		result = DaoParser_ParseEnumeration( self, 0, tki, start+1, rb-1 );
		start = rb + 1;
	}else if( tki == DTOK_LSB ){
		int rb = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, start, end );
		if( rb < 0 ) return result;
		result = DaoParser_ParseEnumeration( self, 0, tki, start+1, rb-1 );
		start = rb + 1;
	}else if( tki == DTOK_LB ){
		result = DaoParser_ParseParenthesis( self );
		start = self->curToken;
	}else if( tki == DKEY_ROUTINE ){
		int tokname = DaoParser_NextTokenName( self );
		/* anonymous function or closure expression */
		self->curToken += 1;
		if( tokname != DTOK_LB && tokname != DTOK_LCB ){
			DaoParser_Error( self, DAO_CTW_INVA_SYNTAX, self->mbs );
			return error;
		}
		result.reg = regLast = DaoParser_ExpClosure( self, start );
		result.first = last->next;
		result.last = result.update = self->vmcLast;
		start = self->curToken;
	}else if( tki == DKEY_CLASS ){
		result.reg = regLast = DaoParser_ClassExpression( self, start );
		result.first = last->next;
		result.last = result.update = self->vmcLast;
		start = self->curToken;
	}else if( tki == DKEY_YIELD ){
		if( start+1 > end || tokens[start+1]->name != DTOK_LB ){
			DaoParser_Error( self, DAO_INVALID_EXPRESSION, NULL );
			return error;
		}
		self->curToken = start + 2;
		enode = DaoParser_ParseExpressionList( self, DTOK_COMMA, NULL, NULL );
		if( enode.reg < 0 || DaoParser_CheckTokenType( self, DTOK_RB, ")" ) == 0 ) return error;
		rb = self->curToken;
		regLast = DaoParser_PushRegister( self );
		DaoParser_AddCode( self, DVM_YIELD, enode.reg, enode.count, regLast, start, 0, rb );
		result.reg = regLast;
		result.first = last->next;
		result.last = result.update = self->vmcLast;
		start = rb + 1;
	}else if( tki2 == DTOK_LB && (tki >= DKEY_RAND && tki <= DKEY_TANH) ){
		/* built-in math functions */
		rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, end );
		if( rb < 0 || (rb == start+2 && tki != DKEY_RAND) ){
			if( rb == start+2 ) DaoParser_Error( self, DAO_PARAM_INVALID, NULL );
			return error;
		}
		if( rb == start+2 /* && tki == DKEY_RAND */ ){
			reg = DaoParser_PushRegister( self );
			DaoParser_AddCode( self, DVM_DATA, DAO_DOUBLE, 1, reg, start, 0, rb );
		}else{
			reg = DaoParser_MakeArithTree( self, start+2, rb-1, &cst );
		}
		if( reg <0 ) return error;
		if( cst && tki != DKEY_RAND ){
			DaoProcess *proc;
			DaoVmCode vmc = { DVM_MATH, 0, 1, 0 };
			DaoValue *value;

			vmc.a = tki - DKEY_RAND;
			proc = DaoNamespace_ReserveFoldingOperands( self->nameSpace, 2 );
			DaoValue_Copy( DaoParser_GetVariable( self, cst ), & proc->activeValues[1] );
			proc->activeCode = & vmc;
			value = DaoParser_EvalConst( self, proc, 2 );
			if( value == NULL ) return error;
			result.konst = LOOKUP_BIND_LC( DaoRoutine_AddConstant( self->routine, value ));
			regLast = DaoParser_GetNormRegister( self, result.konst, start, 0, rb );
		}else{
			regLast = DaoParser_PushRegister( self );
			DaoParser_AddCode( self, DVM_MATH, tki-DKEY_RAND, reg, regLast, start, 0, rb );
		}
		result.reg = regLast;
		result.first = last->next;
		result.last = result.update = self->vmcLast;
		start = rb + 1;
	}else if( tki == DTOK_ID_THTYPE && tki2 == DTOK_LB ){
		DaoToken tok = *tokens[start];
		regLast = DaoParser_GetRegister( self, & tok );
		if( regLast <0 ){
			DString_SetMBS( self->mbs, tokens[start]->string.mbs + 1 );
			tok.string = *self->mbs;
			regLast = DaoParser_GetRegister( self, & tok );
			if( regLast < 0 ){
				DaoParser_Error( self, DAO_SYMBOL_NOT_DEFINED, self->mbs );
				return error;
			}
		}
		if( LOOKUP_ISCST( regLast ) ) result.konst = regLast;
		result.reg = regLast = DaoParser_GetNormRegister( self, regLast, start, 0, start );
		result.first = last->next;
		result.last = result.update = self->vmcLast;
		start += 1;
	}else if( (tki >= DTOK_IDENTIFIER && tki <= DTOK_WCS) || tki == DTOK_COLON
			|| (tki >= DKEY_ANY && tki <= DKEY_CDATA ) || tki >= DKEY_ABS || tki == DKEY_SELF ){
		regLast = DaoParser_ParseAtomicExpression( self, start, & cst );
		if( last != self->vmcLast ) result.first = result.last = result.update = self->vmcLast;
		result.reg = regLast;
		result.konst = cst;
		start += 1;
	}
	self->curToken = start;
	if( result.reg < 0 ) return result;
	while( self->curToken < self->tokens->size ){
		DArray *cid;
		DaoInode *extra = self->vmcLast;
		DaoInode *back = self->vmcLast;
		DaoInode *getx = result.last;
		int regcount = self->regCount;
		int postart = start - 1;
		tkn = DaoParser_CurrentTokenName( self );
		if( tokens[self->curToken]->line != tokens[self->curToken-1]->line ){
			if( tkn != DTOK_DOT && tkn != DTOK_COLON2 ) return result;
		}
		if( result.first == NULL && result.last ) result.first = result.last;
		if( result.last ){
			postart = result.last->first + result.last->middle;
			if( getx->code < DVM_GETVH || getx->code > DVM_GETF ) getx = NULL;
		}
		start = self->curToken;
		if( tkn == stop ) return result;
		switch( tkn ){
		case DTOK_LB :
			{
				int rb, rb2, mode = 0, konst = 0, code = DVM_CALL;
				DaoInode *inode;
				rb = rb2 = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, end );
				if( rb < 0 ) return error;
				if( (rb+1) <= end && tokens[rb+1]->name == DTOK_ASSN ) return result;

				if( (rb+1) <= end ){
					if( tokens[rb+1]->name == DTOK_BANG2 ){
						mode |= DAO_CALL_ASYNC;
						rb += 1;
					}
				}
				inode = self->vmcLast;
				if( result.last && inode->code == DVM_LOAD2 ){ /* X.Y */
					DaoParser_PopRegister( self ); /* opc of GETF will be reallocated; */
					inode->code = DVM_LOAD;
					inode->b = 0;
					inode = inode->prev;
					code = DVM_MCALL;
					extra = back->prev;
				}else if( result.last &&  DaoVmCode_CheckPermutable( result.last->code ) ){
					inode = result.last;
					extra = back;
				}else if( result.last == NULL ){
					DaoParser_AddCode( self, DVM_LOAD, regLast,0,0/*unset*/, start,0,0 );
					inode = self->vmcLast;
					extra = self->vmcLast;
				}
				/*
				// Marking the call to the decorated function decorators,
				// so that decoration can be done properly for constructors:
				*/
				if( routine->attribs & DAO_ROUT_DECORATOR ){
					int mv = inode->code == DVM_MOVE || inode->code == DVM_LOAD;
					int fp = (routine->attribs & DAO_ROUT_PARSELF) != 0;
					if( mv && inode->a == fp ) mode |= DAO_CALL_DECSUB;
				}
				self->curToken += 1;
				cid = DaoParser_GetArray( self );
				enode = DaoParser_ParseExpressionList2( self, DTOK_COMMA, inode, cid, DAO_EXPRLIST_TUPLE );
				if( DaoParser_CurrentTokenName( self ) == DTOK_DOTS ){
					mode |= DAO_CALL_EXPAR;
					self->curToken += 1;
				}
				if( enode.reg < 0 || self->curToken != rb2 ) return error;
				if( enode.count > DAO_MAX_PARAM ){
					DaoParser_Error( self, DAO_CTW_LIMIT_PAR_NUM, NULL );
					return error;
				}
				regLast = DaoParser_PushRegister( self );
				DaoParser_AddCode( self, code, enode.reg, (enode.count-1)|mode, regLast, postart, start, rb );
				if( self->needConst && result.konst && enode.konst == (enode.count-1) ){
					DaoRoutine *rout = (DaoRoutine*) DaoParser_GetVariable( self, result.konst );
					if( rout != NULL ){
						cid->items.pInt[0] = result.konst;
						enode.prev = extra ? extra->prev : back;
						/*
						// DaoProcess_DoCall() will check for the returned type,
						// if it is NULL, a none value will be returned instead.
						// Set dao_type_udf as the returned type to avoid this.
						//
						// Don't use dao_type_any, because the evaluation may call
						// DaoProcess_GetReturnType(), which need to find the
						// specialized return type.
						//
						// static s = state<int>()
						*/
						DArray_PushFront( self->enumTypes, dao_type_udf );
						regLast = DaoParser_MakeEnumConst( self, & enode, cid, regcount );
						DArray_PopFront( self->enumTypes );
						if( regLast >=0 ){
							result.first = self->vmcLast;
							konst = enode.konst;
						}
					}
				}
				result.konst = konst;
				result.reg = regLast;
				result.last = result.update = self->vmcLast;
				self->curToken = rb + 1;
				break;
			}
		case DTOK_LCB :
			{
				DaoInode *open, *jump, *label, *sect, *call;
				DMap *varFunctional;
				int isFunctional, opa = result.reg, opb = -1;
				int lb = start, regCount;
				int rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
				if( rb < 0 ) return error;

				self->curToken += 1;
				if( result.last && back->code == DVM_LOAD2 ){ /* X.Y */
					DaoParser_PopRegister( self ); /* opc of GETF will be reallocated; */
					back->code = DVM_LOAD;
					back->b = 0;
					back->prev->c = DaoParser_PushRegister( self );
					back->c = DaoParser_PushRegister( self );
					regLast = DaoParser_PushRegister( self );
					DaoParser_AddCode( self, DVM_MCALL, back->prev->c, 1, regLast, start,0,0 );
				}else if( back->code != DVM_CALL && back->code != DVM_MCALL ){
					regLast = DaoParser_PushRegister( self );
					DaoParser_AddCode( self, DVM_CALL, opa, 0, regLast, start,0,0 );
				}
				call = self->vmcLast;
				call->b |= DAO_CALL_BLOCK;

				if( DaoParser_PushOuterRegOffset( self, start, rb ) == 0 )
					goto InvalidFunctional;

				jump = DaoParser_AddCode( self, DVM_GOTO, 0, 0, DVM_SECT, start+1, 0, 0 );
				sect = DaoParser_AddCode( self, DVM_SECT, self->regCount, 0,0, start+1, 0,0 );
				label = DaoParser_AddCode( self, DVM_LABEL, 0, 1, 0, rb, 0,0 );
				open = DaoParser_AddScope( self, DVM_LBRA, label ); /* breakable scope; */
				jump->jumpTrue = label;
				open->c = DVM_SECT;
				varFunctional = DaoParser_GetCurrentDataMap( self );
				start += 1;
				regCount = self->regCount;
				if( tokens[start]->name == DTOK_LSB ){
					int j, i = start + 1;
					int rb2 = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, start, rb );
					if( rb2 < 0 ) goto InvalidFunctional;
					while( i < rb2 ){
						if( tokens[i]->type != DTOK_IDENTIFIER ) break;
						j = DaoParser_PushRegister( self );
						MAP_Insert( varFunctional, & tokens[i]->string, j );
						if( tokens[++i]->name != DTOK_COMMA ) break;
						i += 1;
					}
					if( i < rb2 ){
						DaoParser_PopRegisters( self, self->regCount - regCount );
					}else{
						sect->c = self->regCount - regCount;
						start = rb2 + 1;
					}
				}
				assert( sect->c < 16 && self->numSections < 14 );
				sect->c |= (++self->numSections) << 4;
				if( start == lb + 1 ){
					DString X = DString_WrapMBS( "X" );
					DString Y = DString_WrapMBS( "Y" );
					MAP_Insert( varFunctional, & X, DaoParser_PushRegister( self ) );
					MAP_Insert( varFunctional, & Y, DaoParser_PushRegister( self ) );
				}
				sect->b = self->regCount - regCount;
				back = self->vmcLast;
				regCount = self->regCount;
				isFunctional = self->isFunctional;
				self->isFunctional = 1;
				self->curToken = start;
				enode = DaoParser_ParseExpression2( self, 0, 0, 0 );
				if( enode.reg >= 0 && self->curToken == rb ){
					DaoParser_AddCode( self, DVM_RETURN, enode.reg, 1, DVM_SECT, start, 0, rb-1 );
				}else{
					self->curToken = start;
					DaoParser_Restore( self, back, regCount );
					enode = DaoParser_ParseExpressionList( self, DTOK_COMMA, NULL, NULL );
					if( enode.reg >= 0 && self->curToken == rb ){
						DaoParser_AddCode( self, DVM_RETURN, enode.reg, enode.count, DVM_SECT, start, 0, rb-1 );
					}else{
						DaoType *oldret = self->returnType;
						DaoParser_Restore( self, back, regCount );
						self->returnType = NULL;
						if( DaoParser_ParseCodes( self, start, rb-1 ) == 0 ){
							self->returnType = oldret;
							goto InvalidFunctional;
						}
						self->returnType = oldret;
					}
				}
				if( self->vmcLast->code == DVM_RETURN ){
					self->vmcLast->c = DVM_SECT;
				}else{
					int first = self->vmcLast->first;
					opa = self->vmcValue ? self->vmcValue->c : 0;
					opb = self->vmcValue != NULL;
					DaoParser_AddCode( self, DVM_RETURN, opa, opb, DVM_SECT, first, 0, rb );
				}
				self->isFunctional = isFunctional;

				self->curToken = rb + 1;
				DaoParser_DelScope( self, NULL );
				DaoParser_AddCode( self, DVM_GOTO, 0, 0, DVM_SECT, rb, 0, 0 );
				DArray_PopBack( self->outers );
				self->vmcLast->jumpTrue = jump;
				DaoParser_AppendCode( self, label ); /* move to back */
				regLast = call->c;
				result.konst = 0;
				result.reg = regLast;
				result.last = self->vmcLast;
				result.update = call;
				self->curToken = rb + 1;
				break;
InvalidFunctional:
				self->curToken = rb + 1;
				enode.reg = -1;
				return enode;
			}
		case DTOK_LSB :
			{
				/*  map/list/array[ i ] : */
				int regcount = self->regCount;

				rb = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, start, end );
				if( rb < 0 ) return error;

				cst = 0;
				self->curToken += 1;
				if( DaoParser_FindOpenToken( self, DTOK_COMMA, start+1, rb, 0 ) < 0 ){
					if( (start+1) == rb ){
						enode = DaoParser_NoneValue( self );
					}else{
						enode = DaoParser_ParseExpression2( self, 0, DAO_EXPRLIST_SLICE, 1 );
					}
					if( enode.reg < 0 || self->curToken != rb ) return error;
					if( result.konst && enode.konst ){
						DaoValue *v1 = DaoParser_GetVariable( self, result.konst );
						DaoValue *v2 = DaoParser_GetVariable( self, enode.konst );
						regLast = DaoParser_MakeArithConst( self, DVM_GETI, v1, v2, & cst, back, regcount );
						result.konst = cst;
					}else{
						result.konst = 0;
					}
					if( result.konst == 0 ){
						regLast = DaoParser_PushRegister( self );
						DaoParser_AddCode( self, DVM_GETI, result.reg, enode.reg, regLast, postart, start, rb );
						if( getx ) self->vmcLast->extra = getx;
					}
				}else{
					reg = -1;
					cid = DaoParser_GetArray( self );
					if( result.last == NULL )
						DaoParser_AddCode( self, DVM_LOAD, regLast,0,0/*unset*/, start,0,0 );
					enode = DaoParser_ParseExpressionList2( self, DTOK_COMMA, self->vmcLast, cid, DAO_EXPRLIST_SLICE );
					if( enode.reg < 0 || self->curToken != rb ) return enode;
					regLast = DaoParser_PushRegister( self );
					DaoParser_AddCode( self, DVM_GETMI, enode.reg, enode.count-1, regLast, postart, start, rb );

					if( result.konst ){
						cid->items.pInt[0] = result.konst;
						enode.prev = extra ? extra->prev : back;

						reg = DaoParser_MakeEnumConst( self, & enode, cid, regcount );
					}
					if( reg >=0 ){
						result.first = self->vmcLast;
						result.konst = cst = enode.konst;
						regLast = reg;
					}else{
						result.konst = 0;
						if( getx ) self->vmcLast->extra = getx;
					}
				}
				result.reg = regLast;
				result.last = result.update = self->vmcLast;
				result.konst = cst;
				self->curToken = rb + 1;
				break;
			}
		case DTOK_COLON2 :
			{
				DaoValue *it = NULL;
				int j, opa = result.reg, opb = -1;

				self->curToken += 1;
				if( tokens[start+1]->name == DTOK_LCB ){ /* ::{}, data packing */
					/* dao_class::{ members } enumeration,
					 * or routine::{ parameters } */
					DaoInode *inode = self->vmcLast;
					int code = DVM_PACK;
					int rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
					if( rb < 0 ) return error;

					if( result.last && back->code == DVM_LOAD2 ){ /* X.Y */
						DaoParser_PopRegister( self ); /* opc of GETF will be reallocated; */
						extra = back->prev;
						back->code = DVM_LOAD;
						back->b = 0;
						code = DVM_MPACK;
					}else if( result.last &&  DaoVmCode_CheckPermutable( back->code ) ){
						extra = back;
					}else{
						DaoParser_AddCode( self, DVM_LOAD, regLast,0,0/*unset*/, start,0,0 );
						extra = self->vmcLast;
					}
					self->curToken += 1;
					cid = DaoParser_GetArray( self );
					enode = DaoParser_ParseExpressionList2( self, DTOK_COMMA, extra, cid, DAO_EXPRLIST_TUPLE );
					if( enode.reg < 0 || self->curToken != rb ) return enode;

					regLast = DaoParser_PushRegister( self );
					DaoParser_AddCode( self, code, enode.reg, enode.count-1, regLast, postart, start, rb );

					if( self->needConst && result.konst && enode.konst == (enode.count-1) ){
						value = DaoParser_GetVariable( self, result.konst );
						if( code == DVM_PACK && (value == NULL || value->type != DAO_CLASS) ){
							cid->items.pInt[0] = result.konst;
							if( extra ){
								DaoInode *inode = self->vmcLast;
								inode->last = inode->first + inode->last - extra->first;
								inode->middle = inode->first - extra->first;
								inode->first = extra->first;
							}
							enode.prev = extra ? extra->prev : back;
							regLast = DaoParser_MakeEnumConst( self, & enode, cid, regcount );
							if( regLast >=0 ) result.first = self->vmcLast;
						}
					}
					result.reg = regLast;
					result.last = result.update = self->vmcLast;
					result.konst = enode.konst;
					self->curToken = rb + 1;
					break;
				}

				if( DaoParser_CurrentTokenType( self ) != DTOK_IDENTIFIER ){
					DaoParser_Error2( self, DAO_INVALID_EXPRESSION, start, 1, 0 );
					return error;
				}
				name = & tokens[self->curToken]->string;
				/* printf( "%s  %i\n", name->mbs, result.konst ); */
				if( result.konst ){
					int count = self->errors->size;
					DaoValue *obj = DaoParser_GetVariable( self, result.konst );
					daostr.data = name;
					regLast = DaoParser_MakeArithConst( self, DVM_GETF, obj, svalue, & cst, back, regcount );
					if( cst == 0 && (eltype & DAO_EXPRLIST_SCOPE) ){
						DArray_Erase( self->errors, count, -1 );
						self->curToken --;
						return result;
					}
					result.konst = cst;
				}
				if( result.konst == 0 ){
					opb = DaoParser_AddFieldConst( self, name );
					regLast = DaoParser_PushRegister( self );
					DaoParser_AddCode( self, DVM_GETF, opa, opb, regLast, postart, start, start+1 );
					if( getx ) self->vmcLast->extra = getx;
				}
				result.reg = regLast;
				result.last = result.update = self->vmcLast;
				self->curToken += 1;
				break;
			}
		case DTOK_DOT :
			{
				int opb, opa = result.reg;
				self->curToken += 1;
				if( DaoParser_CurrentTokenType( self ) != DTOK_IDENTIFIER ){
					DaoParser_Error2( self, DAO_INVALID_EXPRESSION, start, 1, 0 );
					return error;
				}
				name = & tokens[self->curToken]->string;
				if( result.konst ){
					DaoValue *obj = DaoParser_GetVariable( self, result.konst );
					daostr.data = name;
					regLast = DaoParser_MakeArithConst( self, DVM_GETF, obj, svalue, & cst, back, regcount );
					result.konst = cst;
				}
				if( result.konst == 0 ){
					opb = DaoParser_AddFieldConst( self, name );
					regLast = DaoParser_PushRegister( self );
					DaoParser_AddCode( self, DVM_GETF, opa, opb, regLast, postart, start, start+1 );
					if( getx ) self->vmcLast->extra = getx;
				}
				result.reg = regLast;
				result.last = result.update = self->vmcLast;
				DaoParser_AddCode( self, DVM_LOAD2, opa, 0, 0, postart, start, start+1 );
				self->curToken += 1;
				break;
			}
		case DTOK_LT :
			if( result.konst == 0 ) return result;
			value = DaoParser_GetVariable( self, result.konst );
			if( value->type != DAO_CLASS && value->type != DAO_CTYPE ) return result;
			rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, end );
			if( rb < 0 ) return result;
			dbase = DaoParse_InstantiateType( self, value, start+1, rb-1 );
			if( dbase ){
				DaoInode *prev = result.first ? result.first->prev : NULL;
				DaoParser_PopBackCode( self );
				DaoParser_PopRegister( self );
				self->curToken = rb + 1;
				cst = DaoRoutine_AddConstant( self->routine, dbase );
				cst = LOOKUP_BIND_LC( cst );
				result.konst = cst;
				result.reg = DaoParser_GetNormRegister( self, cst, start, 0, rb );
				result.last = result.update = self->vmcLast;
				if( prev ) result.first = prev->next;
				break;
			}else{
				DaoParser_Error2( self, DAO_FAILED_INSTANTIATION, start-1, rb, 0 );
			}
			return result;
		default : return result;
		}
	}
	return result;
}
static void DaoParser_TryAddSetCodes( DaoParser *self )
{
	DaoInode *setx, *inode = self->vmcLast;
	unsigned short opc = inode->c;
	if( inode->code < DVM_SETVH || inode->code > DVM_SETF ) return;
	while( inode->extra && inode->c == opc ){
		inode = inode->extra;
		if( inode->code < DVM_GETVH || inode->code > DVM_GETF ) continue;
		setx = DaoParser_PushBackCode( self, (DaoVmCodeX*)inode );
		setx->code += DVM_SETVH - DVM_GETVH;
		setx->c = inode->a;
		setx->a = inode->c;
		/* DaoInode_Print( inode ); */
	}
}
static DaoEnode DaoParser_ParseUnary( DaoParser *self, int stop, int eltype )
{
	DaoEnode result;
	DaoInode *back = self->vmcLast;
	int oldcount = self->regCount;
	int tok = DaoParser_CurrentTokenName( self );
	int oper = daoArithOper[ tok ].oper;
	int code = mapAithOpcode[ oper ];
	int end, start = self->curToken;
	int opa, opb = 0, ec = 0;

	if( daoArithOper[ tok ].left == 0 ) return DaoParser_ParsePrimary( self, stop, eltype );

	/* parse left hand unary operator */
	self->curToken += 1;
	result = DaoParser_ParseUnary( self, stop, 0 );
	if( result.reg < 0 ) return result;
	if( oper == DAO_OPER_ADD ) return result;

	switch( oper ){
	case DAO_OPER_NOT : code = DVM_NOT; break;
	case DAO_OPER_INCR : code = DVM_ADD; break;
	case DAO_OPER_DECR : code = DVM_SUB; break;
	case DAO_OPER_SUB  : code = DVM_MINUS; break;
	case DAO_OPER_TILDE : code = DVM_TILDE; break;
	case DAO_OPER_MOD   : code = DVM_SIZE; break;
	default : ec = DAO_INVALID_EXPRESSION; goto ErrorParsing;
	}
	if( result.konst && (code == DVM_ADD || code == DVM_SUB) ){
		ec = DAO_EXPR_MODIFY_CONSTANT;
		goto ErrorParsing;
	}

	opa = result.reg;
	end = self->curToken - 1;
	if( result.konst && code != DVM_ADD && code != DVM_SUB ){
		DaoValue *value = DaoParser_GetVariable( self, result.konst );
		result.reg = DaoParser_MakeArithConst( self, code, value, dao_none_value, & result.konst, back, oldcount );
		if( result.reg < 0 ){
			ec = DAO_CTW_INV_CONST_EXPR;
			goto ErrorParsing;
		}
		result.prev = back;
		result.first = result.last = result.update = self->vmcLast;
		return result;
	}else if( code == DVM_ADD || code == DVM_SUB ){
		DaoInode *vmc = result.last;
		int code2 = vmc ? vmc->code : -1;
		opb = DaoParser_IntegerOne( self, start );
		DaoParser_AddCode( self, code, opa, opb, opa, start, 0, end );
		if( code2 >= DVM_GETVH && code2 <= DVM_GETF ){
			DaoParser_PushBackCode( self, (DaoVmCodeX*) vmc );
			self->vmcLast->extra = vmc->extra;
			vmc = self->vmcLast;
			opa = vmc->a; vmc->a = vmc->c; vmc->c = opa;
			vmc->code += DVM_SETVH - DVM_GETVH;
			DaoParser_TryAddSetCodes( self );
		}
		result.update = self->vmcLast;
		/* to prevent the previous instruction from beeing updated by ParseExpressionList(s) */
		DaoParser_AddCode( self, DVM_UNUSED, 0,0,0, 0,0,0 );
	}else{
		result.reg = DaoParser_PushRegister( self );
		DaoParser_AddCode( self, code, opa, opb, result.reg, start, 0, end );
	}
	result.konst = 0;
	result.prev = back;
	result.first = result.last = self->vmcLast;
	if( result.update == NULL ) result.update = self->vmcLast;
	return result;
ErrorParsing:
	DaoParser_Error( self, ec, & self->tokens->items.pToken[start]->string );
	result.reg = -1;
	return result;
}
static DaoEnode DaoParser_ParseOperator( DaoParser *self, DaoEnode LHS, int prec, int stop, int eltype, int warn )
{
	DaoEnode result = { -1, 0, 1, NULL, NULL, NULL, NULL };
	DaoEnode RHS = { -1, 0, 1, NULL, NULL, NULL, NULL };
	DaoToken **tokens = self->tokens->items.pToken;
	DaoInode *move, *test, *jump, *jump2, *inode = NULL;
	int oper, code, postart = self->curToken-1, posend;

	if( LHS.first ) postart = LHS.first->first;
	result.prev = LHS.prev;
	while(1){
		DaoInode *last2, *last1;
		int pos = self->curToken, fold = 0;
		int thisPrec, nextPrec, curtok = DaoParser_CurrentTokenName( self );
		if( curtok == stop ) return LHS;
		thisPrec = DaoParser_GetOperPrecedence( self );

		/* If this is not an operator, or is an operator with precedence
		 * less than the precedence of the previous operator: */
		if(thisPrec < prec) return LHS;

		/* Surely an operator: */
		oper = daoArithOper[ tokens[self->curToken]->name ].oper;
		self->curToken += 1; /* eat the operator */

		code = LHS.update ? LHS.update->code : DVM_NOP;
		if( oper == DAO_OPER_ASSN && code >= DVM_GETVH && code <= DVM_GETF ){
			/* GETX will be to SETX, pop unused register: */
			DaoParser_PopRegister( self ); /* opc of LHS.last */
		}

		/* Parse the primary expression after the binary operator: */
		last1 = self->vmcLast;
		RHS = DaoParser_ParseUnary( self, stop, 0 );
		if( RHS.reg < 0 ){
			if( oper != DAO_OPER_COLON || self->curToken > pos + 1 ) return RHS;
			/* e : , */
			RHS = DaoParser_NoneValue( self );
		}
		result.update = NULL;
		if( oper == DAO_OPER_IF ){ /* conditional operation:  c ? e1 : e2 */
			DaoEnode RHS1, RHS2;
			int prec2 = 10*(20 - daoArithOper[DTOK_COLON].binary);
			RHS1 = DaoParser_ParseOperator(self, RHS, prec2 + 1, DTOK_COLON, 0, 1 );
			if( DaoParser_CheckTokenType( self, DTOK_COLON, ":" ) == 0 ) RHS1.reg = -1;
			if( RHS1.reg < 0 ) return RHS1;
			if( last1 == self->vmcLast ){
				last1 = DaoParser_AddCode( self, DVM_MOVE, RHS1.reg, 888, RHS1.reg, 0,0, pos );
				RHS1.first = RHS1.update = RHS1.last = last1;
			}
			last2 = self->vmcLast;
			self->curToken += 1;
			RHS2 = DaoParser_ParseUnary( self, DTOK_COLON, 0 );
			if( RHS2.reg < 0 ) return RHS2;
			RHS2 = DaoParser_ParseOperator(self, RHS2, prec2 + 1, DTOK_COLON, 0, 1 );
			if( RHS2.reg < 0 ) return RHS2;
			if( last2 == self->vmcLast ){
				last2 = DaoParser_AddCode( self, DVM_MOVE, RHS2.reg, 999, RHS2.reg, 0,0, pos );
				RHS2.first = RHS2.update = RHS2.last = last2;
			}

			result.reg = DaoParser_PushRegister( self );
			if( LHS.last == NULL ) LHS.last = RHS1.prev;
			test = DaoParser_InsertCode( self, LHS.last, DVM_TEST, LHS.reg, 0, 0, pos );
			jump = DaoParser_InsertCode( self, RHS2.prev, DVM_GOTO, 0,0,0, pos );
			DaoParser_InsertCode( self, RHS2.prev, DVM_MOVE, RHS1.reg, 0, result.reg, pos );
			DaoParser_AddCode( self, DVM_MOVE, RHS2.reg, 0, result.reg, 0,0, pos );
			DaoParser_AddCode( self, DVM_UNUSED, 0,0,0, 0,0,0 );
			jump->jumpTrue = self->vmcLast;
			test->jumpFalse = jump->next;
			result.last = self->vmcLast;
			result.update = result.last->prev;
			result.first = LHS.first;
			if( result.first == NULL ) result.first = result.last;
			if( result.update == NULL ) result.update = result.last;
			LHS = result;
			continue;
		}

		nextPrec = DaoParser_GetOperPrecedence( self );
		/* If the pending operator has higher precedence,
		 * use RHS as the LHS of the pending operator: */
		if (thisPrec < nextPrec) {
			RHS = DaoParser_ParseOperator(self, RHS, thisPrec+1, stop, 0, 1 );
			if( RHS.reg < 0 ) return RHS;
		}

		posend = self->curToken - 1;
		if( oper == DAO_OPER_ASSN ){
			if( LHS.konst ) goto InvalidConstModificatioin;
			if( warn && curtok == DTOK_ASSN )
				DaoParser_Warn2( self, DAO_WARN_ASSIGNMENT, postart, posend );

			if( code >= DVM_GETVH && code <= DVM_GETF ){ /* change GETX to SETX */
				LHS.last->code += DVM_SETVH - DVM_GETVH;
				LHS.last->c = LHS.last->a;
				LHS.last->a = RHS.reg;
				DaoParser_AppendCode( self, LHS.last ); /* move to back */
				result.reg = RHS.reg;
				result.update = RHS.update;
				result.last = self->vmcLast;
				DaoParser_TryAddSetCodes( self );
			}else{
				DaoParser_AddCode( self, DVM_MOVE, RHS.reg, 0, LHS.reg, postart, 0, posend );
				result.reg = LHS.reg;
				result.last = self->vmcLast;
			}
		}else if( oper >= DAO_OPER_ASSN_ADD && oper <= DAO_OPER_ASSN_XOR ){
			if( LHS.konst ) goto InvalidConstModificatioin;
			result.last = DaoParser_AddBinaryCode( self, mapAithOpcode[oper], & LHS, & RHS, pos );
			result.update = result.last;
			DaoParser_PopRegister( self ); /* result.last->c */
			if( code >= DVM_GETVH && code <= DVM_GETF ){ /* add SETX */
				/* For X += Y, if X is not local, it will be compiled into:
				 *   GETX A1, B1, C1; # LHS.last;
				 *   ADD  C1, B2, C2; # result.last;
				 *   SETX C2, B1, A1; # inode;
				 * If X is a string and Y an integer, it will require that
				 *   C1 == C2;
				 * to pass static type checking: */
				result.last->c = LHS.last->c;
				result.reg = LHS.last->c;

				inode = DaoParser_PushBackCode( self, (DaoVmCodeX*) LHS.last );
				inode->extra = LHS.last->extra;
				inode->code += DVM_SETVH - DVM_GETVH;
				inode->c = inode->a;
				inode->a = result.reg;
				result.last = inode;
				DaoParser_TryAddSetCodes( self );
			}else{
				result.last->c = result.last->a;
				result.reg = result.last->a;
			}
		}else if( oper == DAO_OPER_AND || oper == DAO_OPER_OR ){
			result.last = DaoParser_AddBinaryCode( self, mapAithOpcode[oper], & LHS, & RHS, pos );
			result.reg = result.last->c;
			fold = 1;
			if( LHS.first != LHS.last || RHS.first != RHS.last ){ /* use branching */
				DaoInode *it = LHS.first;
				assert( LHS.update != NULL || RHS.update != NULL );
				if( LHS.last == NULL ) LHS.last = RHS.prev;
				if( RHS.first == NULL ) RHS.first = self->vmcLast->prev;
				result.reg = DaoParser_PushRegister( self );
				move = DaoParser_InsertCode( self, LHS.last, DVM_MOVE, LHS.reg, 0, result.reg, postart );
				test = DaoParser_InsertCode( self, move, DVM_TEST, LHS.reg,0,0,postart );
				while( it ){
					if( it->jumpTrue == test->next ) it->jumpTrue = move;
					if( it->jumpFalse == test->next ) it->jumpFalse = move;
					it = it->next;
				}
				result.last->code = mapAithOpcode[oper];
				result.last->a = result.reg;
				result.last->b = result.last->b;
				result.last->c = result.reg;
				test->jumpFalse = DaoParser_AddCode( self, DVM_NOP, 0, 0, 0, postart,0,0 );
				if( oper == DAO_OPER_OR ){
					jump = DaoParser_InsertCode( self, test, DVM_GOTO, 0,0,0, postart );
					jump->jumpTrue = self->vmcLast;
					test->jumpFalse = jump->next;
				}
				fold = 0;
			}
		}else{
			if( oper == DAO_OPER_COLON && !(eltype & DAO_EXPRLIST_SLICE) )
				goto InvalidExpression;
			result.last = DaoParser_AddBinaryCode( self, mapAithOpcode[oper], & LHS, & RHS, pos );
			result.reg = result.last->c;
			fold = 1;
		}
		if( fold && LHS.konst && RHS.konst ){
			DaoValue *v1 = DaoParser_GetVariable( self, LHS.konst );
			DaoValue *v2 = DaoParser_GetVariable( self, RHS.konst );
			code = result.last->code;
			result.reg = DaoParser_MakeArithConst( self, code, v1, v2, & result.konst, LHS.prev, LHS.reg );
			result.last = self->vmcLast;
			if( result.reg < 0 ){
				DaoParser_Error( self, DAO_CTW_INV_CONST_EXPR, NULL );
				return result;
			}
		}else{
			result.first = LHS.first;
			if( result.first == NULL ) result.first = RHS.first;
		}
		if( oper == DAO_OPER_NOTIN ){
			/* It was compiled as IN, now add NOT: */
			if( result.konst ){
				DaoValue *value = DaoParser_GetVariable( self, result.konst );
				result.reg = DaoParser_MakeArithConst( self, DVM_NOT, value, dao_none_value, & result.konst, LHS.prev, LHS.reg );
				if( result.reg < 0 ) return result;
				result.last = result.update = self->vmcLast;
			}else{
				int reg = DaoParser_PushRegister( self );
				DaoParser_AddCode( self, DVM_NOT, result.reg, 0, reg, postart, 0, posend );
				result.reg = reg;
				result.last = result.update = self->vmcLast;
			}
		}
		if( result.first == NULL ) result.first = result.last;
		if( result.update == NULL ) result.update = result.last;
		LHS = result;
	}
	return LHS;
InvalidConstModificatioin:
	DaoParser_Error3( self, DAO_EXPR_MODIFY_CONSTANT, postart );
InvalidExpression:
	DaoParser_Error3( self, DAO_INVALID_EXPRESSION, postart );
	result.reg = -1;
	return result;
}
static DaoEnode DaoParser_ParseExpression2( DaoParser *self, int stop, int eltype, int warn )
{
	int start = self->curToken;
	DaoEnode LHS = { -1, 0, 1, NULL, NULL, NULL, NULL };
	DaoToken **tokens = self->tokens->items.pToken;

#if 0
	int i, end = self->tokens->size;
	printf("DaoParser_ParseExpression(): start = %i;\n", start );
	for( i=start;i<end;i++) printf("%s  ", tokens[i]->string.mbs); printf("\n");
#endif
	if( DaoParser_CurrentTokenType( self ) == DTOK_COLON ){
		/* : e , */
		LHS = DaoParser_NoneValue( self );
		if( !(eltype & DAO_EXPRLIST_SLICE) ) LHS.reg = -1;
	}else{
		LHS = DaoParser_ParseUnary( self, stop, eltype );
	}
	if( LHS.reg >= 0 && DaoParser_GetOperPrecedence( self ) >= 0 ){
		LHS = DaoParser_ParseOperator( self, LHS, 0, stop, eltype, warn );
	}
	if( LHS.reg < 0 ){
		if( self->curToken < self->tokens->size && DaoParser_CurrentTokenType( self ) < DTOK_COMMENT ){
			DString *tok = & self->tokens->items.pToken[ self->curToken ]->string;
			DaoParser_Error( self, DAO_INVALID_TOKEN, tok );
		}
		self->curToken = start;
		DaoParser_Error3( self, DAO_INVALID_EXPRESSION, start );
	}
	self->vmcValue = LHS.update;
	return LHS;
}
static DaoEnode DaoParser_ParseExpression( DaoParser *self, int stop )
{
	return DaoParser_ParseExpression2( self, stop, 0, 1 );
}

static DaoEnode DaoParser_ParseExpressionList2( DaoParser *self, int sep, DaoInode *pre, DArray *cids, int eltype )
{
	DaoInode *inode;
	DaoType *type = self->enumTypes->size ? self->enumTypes->items.pType[0] : NULL;
	DaoEnode item, result = { -1, 0, 1, NULL, NULL, NULL, NULL };
	DMap *vartypes = self->routine->body->localVarType;
	DArray *inodes = DArray_New(0);
	int i, tok, cur, id = 0;
	while( pre != NULL ){
		DArray_Append( inodes, pre );
		pre = pre->next;
		if( cids ) DArray_Append( cids, 0 );
	}
	result.konst = 0;
	result.prev = self->vmcLast;
	self->curToken -= 1;
	do {
		self->curToken += 1;
		cur = self->curToken;
		tok = DaoParser_CurrentTokenName( self );
		if( tok == DTOK_RB || tok == DTOK_RCB || tok == DTOK_RSB ) break;
		if( tok == DTOK_DOTS || tok == DTOK_SEMCO ) break;
		if( eltype & DAO_EXPRLIST_ARRAY ){
			/* bar : array<float> = [[1.5 : 3] : [10.5 : 3] : 5] */
			DArray_PushFront( self->enumTypes, type );
		}else{
			DaoParser_PushItemType( self, type, id++, sep );
		}
		item = DaoParser_ParseExpression2( self, sep, eltype, 1 );
		DArray_PopFront( self->enumTypes );
		if( item.reg < 0 ){
			if( self->curToken == cur ) break;
			goto Finalize;
		}
		result.konst += item.konst != 0;
		/* Avoid adding an extra MOVE for intermediate result: */
		if( item.update == self->vmcLast && MAP_Find( vartypes, item.update->c ) == NULL ){
			DaoParser_PopRegister( self );
			DArray_Append( inodes, item.last );
		}else{ /* { a[1] += 2 }: item.update is ADD, but item.last is SETI */
			int p1, p2, p3;
			p1 = p2 = p3 = self->curToken - 1;
			if( item.update ){
				p1 = item.update->first;
				p2 = p1 + item.update->middle;
				p3 = p1 + item.update->last;
			}
			inode = DaoParser_AddCode( self, DVM_MOVE, item.reg,0,0, p1,p2,p3);
			DArray_Append( inodes, inode );
		}
		if( cids ) DArray_Append( cids, item.konst );
	} while( DaoParser_CurrentTokenName( self ) == sep );
	result.count = inodes->size;
	result.reg = DaoParser_PushRegisters( self, inodes->size );
	/*
	// Do not change the order of these instructions!
	// Consider: b = 2; a = { b, b += 3 };
	*/
	for(i=0; i<inodes->size; i++) inodes->items.pInode[i]->c = result.reg + i;
	result.first = result.prev->next;
	result.last = self->vmcLast;
Finalize:
	DArray_Delete( inodes );
	return result;
}
static DaoEnode DaoParser_ParseExpressionList( DaoParser *self, int sep, DaoInode *pre, DArray *cids )
{
	return DaoParser_ParseExpressionList2( self, sep, pre, cids, 0 );
}
/* sep2 should be a natural seperator such as comma and semicolon: */
static DaoEnode DaoParser_ParseExpressionLists( DaoParser *self, int sep1, int sep2, int *step, DArray *cids )
{
	DaoInode *inode;
	DaoType *type = self->enumTypes->size ? self->enumTypes->items.pType[0] : NULL;
	DaoEnode item, result = { -1, 0, 1, NULL, NULL, NULL, NULL };
	DMap *vartypes = self->routine->body->localVarType;
	DArray *inodes = DArray_New(0);
	int i, tok, id=0, count = 0;

	result.konst = 0;
	result.prev = self->vmcLast;
	self->curToken -= 1;
	do {
		self->curToken += 1;
		tok = DaoParser_CurrentTokenName( self );
		if( tok == DTOK_RB || tok == DTOK_RCB || tok == DTOK_RSB ) break;
		if( tok == DTOK_DOTS || tok == DTOK_SEMCO ) break;
		DaoParser_PushItemType( self, type, id++, sep1 );
		item = DaoParser_ParseExpression( self, sep1 );
		DArray_PopFront( self->enumTypes );
		if( item.reg < 0 ) goto Finalize;
		result.konst += item.konst != 0;
		/* Avoid adding an extra MOVE for intermediate result: */
		if( item.update == self->vmcLast && MAP_Find( vartypes, item.update->c ) == NULL ){
			DaoParser_PopRegister( self );
			DArray_Append( inodes, item.last );
		}else{ /* { a[1] += 2 }: item.update is ADD, but item.last is SETI */
			int p1, p2, p3;
			p1 = p2 = p3 = self->curToken - 1;
			if( item.update ){
				p1 = item.update->first;
				p2 = p1 + item.update->middle;
				p3 = p1 + item.update->last;
			}
			inode = DaoParser_AddCode( self, DVM_MOVE, item.reg,0,0, p1,p2,p3);
			DArray_Append( inodes, inode );
		}
		if( cids ) DArray_Append( cids, item.konst );
		tok = DaoParser_CurrentTokenName( self );
		count += 1;
		if( tok != sep1 ){
			if( step && *step && count != *step ) return result;
			if( step ) *step = count;
			count = 0;
		}
	} while( tok == sep1 || tok == sep2 );
	result.count = inodes->size;
	result.reg = DaoParser_PushRegisters( self, inodes->size );
	/* Do not change the order of these instructions! */
	for(i=0; i<inodes->size; i++) inodes->items.pInode[i]->c = result.reg + i;
	result.first = result.prev->next;
	result.last = self->vmcLast;
Finalize:
	DArray_Delete( inodes );
	return result;
}

DaoProcess* DaoNamespace_ReserveFoldingOperands( DaoNamespace *self, int N )
{
	DaoRoutine *rout;
	DaoProcess *proc;

	DaoNamespace_InitConstEvalData( self );
	proc = self->constEvalProcess;
	rout = self->constEvalRoutine;
	if( rout->body->regType->size < N ) DArray_Resize( rout->body->regType, N, NULL );
	DaoProcess_PushFrame( proc, N );
	DaoProcess_PopFrame( proc );
	proc->activeRoutine = self->constEvalRoutine;
	proc->activeValues = proc->stackValues;
	proc->activeTypes = rout->body->regType->items.pType;
	proc->activeNamespace = self;
	return proc;
}
static DaoValue* DaoParser_EvalConst( DaoParser *self, DaoProcess *proc, int nvalues )
{
	DaoVmCode *vmc = proc->activeCode;
	DaoValue **operands = proc->activeValues + 1;
	DaoByteBlock *eval, *coder = self->byteBlock;
	DaoStream *stream;
	DaoValue *value;
	int i, j, count, max = 0;

	if( self->hostType ){
		GC_ShiftRC( self->hostType, proc->activeRoutine->routHost );
		proc->activeRoutine->routHost = self->hostType;
	}
	if( self->byteBlock ){
		for(i=0; i<nvalues; ++i){
			DaoByteBlock *block = DaoByteBlock_FindObjectBlock( coder, operands[i] );
			if( operands[i] && operands[i]->type > max ){
				max = operands[i]->type;
			}else if( block != NULL && block->type == DAO_ASM_EVAL ){
				max = DAO_OBJECT;
			}
		}
		/*
		// Encode the constant as ASM_EVAL if any operand was encoded as ASM_EVAL.
		// Otherwise, do not encode it now as it can be encode as value later.
		//
		// Encode the operands before doing constant folding, because they might
		// be changed by the evaluation.
		*/
		if( max > DAO_COMPLEX ){
			DaoByteBlock_EncodeValues( coder, operands, nvalues );
			DaoByteBlock_EncodeType( coder, proc->activeTypes[0] );
		}
	}
	value = DaoProcess_MakeConst( proc );
	if( value != NULL && max > DAO_COMPLEX ){
		if( max > DAO_ENUM || DaoByteBlock_FindObjectBlock( coder, value ) == NULL ){
			DaoType* retype = proc->activeTypes[0];
			int opb = vmc->code == DVM_GETF ? 2 : vmc->b;
			eval = DaoByteBlock_AddEvalBlock( coder, value, vmc->code, opb, retype );
			DaoByteBlock_AddBlockIndexData( eval, 0, nvalues );
		}
	}
	GC_DecRC( proc->activeRoutine->routHost );
	proc->activeRoutine->routHost = NULL;

	/*
	// DaoProcess_MakeConst() may produce a NULL value without
	// raising an exceptioin. This is acceptable for constant
	// folding for field expressions such as: namespace::variable.
	*/
	if( proc->exceptions->size == 0 ) return value;

	stream = DaoStream_New();
	stream->attribs |= DAO_IO_STRING;
	DaoProcess_PrintException( proc, stream, 1 );
	DaoParser_Error( self, DAO_EVAL_EXCEPTION, stream->streamString );
	DaoParser_Error( self, DAO_CTW_INV_CONST_EXPR, NULL );
	DaoStream_Delete( stream );
	return value;
}
int DaoParser_MakeEnumConst( DaoParser *self, DaoEnode *enode, DArray *cid, int regcount )
{
	DaoProcess *proc;
	DaoType *type = self->enumTypes->size ? self->enumTypes->items.pType[0] : NULL;
	DaoVmCode vmcValue = {0,1,0,0};
	DaoValue *value;
	int p1 = self->vmcLast->first;
	int p3 = p1 + self->vmcLast->last;
	int i, N = enode->count;

	vmcValue.code = self->vmcLast->code;
	vmcValue.b = self->vmcLast->b;
	proc = DaoNamespace_ReserveFoldingOperands( self->nameSpace, N+1 );
	/* printf( "code = %s, %i\n", DaoVmCode_GetOpcodeName( code ), N ); */
	/* Prepare registers for the instruction. */
	for( i=0; i<N; i++ ){
		/* printf( "reg = %i\n", cid->items.pInt[i] ); */
		/* No need GC here: */
		DaoValue *v = DaoParser_GetVariable( self, cid->items.pInt[i] );
		DaoValue_Copy( v, & proc->activeValues[i+1] );
	}
	DaoParser_PopCodes2( self, enode->prev );
	for(i=regcount; i<self->regCount; i++) MAP_Erase( self->routine->body->localVarType, i );
	DaoParser_PopRegisters( self, self->regCount - regcount );
	/* Execute the instruction to get the const result: */
	GC_ShiftRC( type, proc->activeTypes[0] );
	proc->activeTypes[0] = type;
	proc->activeCode = & vmcValue;
	value = DaoParser_EvalConst( self, proc, N );
	if( value == NULL ) return -1;
	enode->konst = LOOKUP_BIND_LC( DaoRoutine_AddConstant( self->routine, value ));
	return DaoParser_GetNormRegister( self, enode->konst, p1, 0, p3 );
}
int DaoParser_MakeArithConst( DaoParser *self, ushort_t code, DaoValue *a, DaoValue *b, int *cst, DaoInode *back, int regcount )
{
	DaoValue *value;
	DaoProcess *proc;
	DaoVmCode vmc = { 0, 1, 2, 0 };
	int p1 = self->vmcLast->first;
	int p2 = p1 + self->vmcLast->middle;
	int p3 = p1 + self->vmcLast->last;

	DaoParser_PopCodes2( self, back );
	DaoParser_PopRegisters( self, self->regCount - regcount );

	*cst = 0;
	vmc.code = code;
	proc = DaoNamespace_ReserveFoldingOperands( self->nameSpace, 3 );
	if( code == DVM_NAMEVA ) vmc.a = DaoRoutine_AddConstant( proc->activeRoutine, a );
	if( code == DVM_GETF ) vmc.b = DaoRoutine_AddConstant( proc->activeRoutine, b );
	DaoValue_Copy( a, & proc->activeValues[1] );
	DaoValue_Copy( b, & proc->activeValues[2] );
	GC_DecRC( proc->activeTypes[0] );
	proc->activeTypes[0] = NULL;
	proc->activeCode = & vmc;
	value = DaoParser_EvalConst( self, proc, 2 );
	if( value == NULL ) return -1;
	*cst = LOOKUP_BIND_LC( DaoRoutine_AddConstant( self->routine, value ));
	return DaoParser_GetNormRegister( self, *cst, p1, p2, p3 );
}
