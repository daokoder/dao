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

#ifdef DAO_WITH_MACRO

#include"stdlib.h"
#include"stdio.h"
#include"string.h"

#include"daoConst.h"
#include"daoParser.h"
#include"daoMacro.h"
#include"daoNamespace.h"

#include"daoStream.h"
#include"daoBase.h"

/* functions defined in daoParser.c */
extern void DaoParser_Warn2( DaoParser *self, int code, int start, int end );
extern void DaoParser_Warn( DaoParser *self, int code, DString *ext );
extern void DaoParser_Error( DaoParser *self, int code, DString *ext );
static int DaoParser_ParseExpression( DaoParser *self, int stop );
extern int DaoParser_FindOpenToken( DaoParser *self, uchar_t tok, int start, int end/*=-1*/, int warn/*=1*/ );
extern int DaoParser_FindPairToken( DaoParser *self,  uchar_t lw, uchar_t rw, int start, int stop/*=-1*/ );

static void DaoToken_Assign( DaoToken *self, DaoToken *other )
{
	self->type = other->type;
	self->name = other->name;
	self->line = other->line;
	self->cpos = other->cpos;
	DString_Assign( & self->string, & other->string );
}
static int DaoToken_EQ( DaoToken *self, DaoToken *other )
{
	if( self->name != other->name ) return 0;
	if( self->name <= DTOK_WCS ) return DString_EQ( & self->string, & other->string );
	return 1;
}

static int DaoParser_FindOpenToken2( DaoParser *self, DaoToken *tok, int start, int end/*=-1*/ )
{
	int i, n1, n2, n3, n4;
	DaoToken **tokens = self->tokens->items.pToken;

	if( start < 0 ) return -10000;
	if( end == -1 || end >= (int)self->tokens->size ) end = self->tokens->size-1;
	n1 = n2 = n3 = n4 = 0;
	for( i=start; i<=end; i++){
		if( ! ( n1 | n2 | n3 | n4 ) && DaoToken_EQ( tokens[i], tok ) ){
			return i;
		}else if( n1 <0 || n2 <0 || n3 <0 || n4 <0 ){
			break;
		}else{
			switch( tokens[i]->name ){
			case DTOK_LCB : n1 ++; break;
			case DTOK_RCB : n1 --; break;
			case DTOK_LB  : n2 ++; break;
			case DTOK_RB  : n2 --; break;
			case DTOK_LSB : n3 ++; break;
			case DTOK_RSB : n3 --; break;
			}
		}
	}
	return -10000;
}

DMacroUnit* DMacroUnit_New()
{
	DMacroUnit *self = (DMacroUnit*) dao_malloc( sizeof(DMacroUnit) );
	self->type = DMACRO_TOK;
	self->stops = DArray_New(D_TOKEN);
	self->marker = DaoToken_New();
	return self;
}
void DMacroUnit_Delete( DMacroUnit *self )
{
	DArray_Delete( self->stops );
	DaoToken_Delete( self->marker );
	dao_free( self );
}

DMacroGroup* DMacroGroup_New()
{
	DMacroGroup *self = (DMacroGroup*) dao_malloc( sizeof(DMacroGroup) );
	self->type = DMACRO_GRP;
	self->repeat = DMACRO_ONE;
	self->cpos = 0;
	self->units = DArray_New(0);
	self->stops = DArray_New(D_TOKEN);
	self->variables = DArray_New(D_TOKEN);
	self->parent = NULL;
	return self;
}
void DMacroGroup_Delete( DMacroGroup *self )
{
	daoint i;
	for(i=0; i<self->units->size; i++ ){
		DMacroUnit *unit = (DMacroUnit*) self->units->items.pVoid[i];
		if( unit->type == DMACRO_GRP || unit->type == DMACRO_ALT ){
			DMacroGroup_Delete( (DMacroGroup*) unit );
		}else{
			DMacroUnit_Delete( unit );
		}
	}
	DArray_Delete( self->stops );
	DArray_Delete( self->units );
	DArray_Delete( self->variables );
	dao_free( self );
}

DaoTypeBase macroTyper=
{
	"macro", & baseCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoMacro_Delete, NULL
};

DaoMacro* DaoMacro_New()
{
	DaoMacro *self = (DaoMacro*) dao_malloc( sizeof(DaoMacro) );
	DaoValue_Init( self, DAO_MACRO );
	self->keyListApply = DArray_New(D_STRING);
	self->macroList = DArray_New(0);
	self->firstMacro = self;
	self->macroMatch = DMacroGroup_New();
	self->macroApply = DMacroGroup_New();
	DArray_Append( self->macroList, self );
	return self;
}
void DaoMacro_Delete( DaoMacro *self )
{
	daoint i;
	if( self == self->firstMacro ){
		for(i=0; i<self->macroList->size; i++){
			DaoMacro *macro = (DaoMacro*) self->macroList->items.pVoid[i];
			if( macro != self ) DaoMacro_Delete( macro );
		}
	}
	DArray_Delete( self->keyListApply );
	DArray_Delete( self->macroList );
	DMacroGroup_Delete( self->macroMatch );
	DMacroGroup_Delete( self->macroApply );
	dao_free( self );
}

static int DaoParser_FindPair( DaoParser *self,  const char *lw, const char *rw, int start, int to )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int i, k = 0;
	int found = 0;

	if( start < 0 ) goto ErrorUnPaired;
	if( to < 0 ) to = self->tokens->size - 1;
	for(i=start; i<=to; ++i){
		if( strcmp( lw, tokens[i]->string.mbs ) == 0 ){
			k++;
		}else if( strcmp( rw, tokens[i]->string.mbs ) == 0 ){
			k--;
			found = 1;
		}
		if( k == 0 && found ) return i;
	}
ErrorUnPaired:
	if( self->vmSpace ){
		DString_SetMBS( self->mbs, lw );
		if( k ==0 ){
			DaoParser_Error( self, DAO_TOKEN_NOT_FOUND, self->mbs );
		}else{
			DString_AppendChar( self->mbs, ' ' );
			DString_AppendMBS( self->mbs, rw );
			DaoParser_Error( self, DAO_TOKENS_NOT_PAIRED, self->mbs );
		}
	}
	return -100;
}
static int DaoParser_MakeMacroGroup( DaoParser *self, DMacroGroup *group, DMacroGroup *parent, int from, int to, DMap *vars, DMap *markers )
{
	unsigned char tk;
	int i, sep, rb, prev;
	DaoToken **toks = self->tokens->items.pToken;
	DMacroGroup *grp, *group2; /* mingw don't like grp2 ?! */
	DMacroUnit *unit;
	DNode *it;

	/*
	   for( i=from; i<to; i++ ) printf( "%s  ", toks[i]->mbs ); printf("\n");
	 */

	i = from;
	while( i < to ){
		DaoToken *tok = toks[i];
		char *chs = tok->string.mbs;
		int tk = tok->name;

#if 0
		printf( "%i %s\n", i, chs );
#endif
		self->curLine = tok->line;
		if( tk == DTOK_LB || tk == DTOK_LSB || tk == DTOK_LCB ){
			grp = DMacroGroup_New();
			grp->cpos = tok->cpos;
			grp->parent = parent;
			DArray_Append( group->units, (void*)grp );
			switch( tk ){
			case DTOK_LB :
				rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, i, to );
				break;
			case DTOK_LSB :
				rb = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, i, to );
				grp->repeat = DMACRO_ZERO_OR_ONE;
				break;
			case DTOK_LCB :
				rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, i, to );
				grp->repeat = DMACRO_ZERO_OR_MORE;
				break;
			default :
				rb = -1;
				DaoParser_Error( self, DAO_CTW_INV_MAC_OPEN, & tok->string );
				break;
			}
			if( rb <0 ) return 0;

			prev = i+1;
			sep = DaoParser_FindOpenToken( self, DTOK_PIPE, i+1, rb, 0 );
			if( sep >=0 ){
				while( sep >=0 ){
					group2 = DMacroGroup_New();
					group2->parent = grp;
					if( DaoParser_MakeMacroGroup( self, group2, group2, prev, sep, vars, markers ) == 0 )
						return 0;
					DArray_Append( grp->units, (void*)group2 );
					prev = sep +1;
					sep = DaoParser_FindOpenToken( self, DTOK_PIPE, prev, rb, 0 );
					if( prev < rb && sep <0 ) sep = rb;
				}
				grp->type = DMACRO_ALT;
			}else if( DaoParser_MakeMacroGroup( self, grp, grp, i+1, rb, vars, markers ) == 0 ){
				return 0;
			}
			i = rb +1;
			tok = toks[i];
			self->curLine = tok->line;
			switch( tok->name ){
			case DTOK_NOT   : grp->repeat = DMACRO_ZERO; i++; break;
			case DTOK_QUERY : grp->repeat = DMACRO_ZERO_OR_ONE; i++; break;
			case DTOK_MUL   : grp->repeat = DMACRO_ZERO_OR_MORE; i++; break;
			case DTOK_ADD   : grp->repeat = DMACRO_ONE_OR_MORE; i++; break;
			default : break;
			}
			continue;
		}

		self->curLine = tok->line;
		unit = DMacroUnit_New();
		DaoToken_Assign( unit->marker, tok );
		DArray_Append( group->units, (void*)unit );
		if( chs[0] == '$' ){
			if( DString_FindMBS( & tok->string, "EXP", 0 ) == 1 ){
				unit->type = DMACRO_EXP;
			}else if( DString_FindMBS( & tok->string, "VAR", 0 ) == 1 ){
				unit->type = DMACRO_VAR;
			}else if( DString_FindMBS( & tok->string, "ID", 0 ) == 1 ){
				unit->type = DMACRO_ID;
			}else if( DString_FindMBS( & tok->string, "OP", 0 ) == 1 ){
				unit->type = DMACRO_OP;
			}else if( DString_FindMBS( & tok->string, "BL", 0 ) == 1 ){
				unit->type = DMACRO_BL;
			}else{
				DaoParser_Error( self, DAO_CTW_INV_MAC_SPECTOK, & tok->string );
				return 0;
			}
			if( vars != NULL ){
				it = DMap_Find( vars, & unit->marker->string );
				if( it == NULL ) it = DMap_Insert( vars, & unit->marker->string, 0 );
				it->value.pInt += 1;
			}
		}else if( tk == DTOK_MBS ){
			DaoLexer_Reset( self->wlexer );
			DaoLexer_Tokenize( self->wlexer, chs + 1, 0 );
			if( self->wlexer->tokens->size == 2 ){
				DaoToken_Assign( unit->marker, self->wlexer->tokens->items.pToken[0] );
				if( markers != NULL ){
					it = DMap_Find( markers, & unit->marker->string );
					if( it == NULL ) it = DMap_Insert( markers, & unit->marker->string, 0 );
					it->value.pInt += 1;
				}
			}
			DaoLexer_Reset( self->wlexer );

			rb = -1;
			if( tok->string.size == 3 ){
				switch( chs[1] ){
				case '(': rb = DaoParser_FindPair( self, "'('", "')'", i, to ); break;
				case '[': rb = DaoParser_FindPair( self, "'['", "']'", i, to ); break;
				case '{': rb = DaoParser_FindPair( self, "'{'", "'}'", i, to ); break;
				default : break;
				}
			}
			if( rb >= 0 ){
				grp = DMacroGroup_New();
				grp->parent = group;
				grp->repeat = DMACRO_AUTO;
				DArray_Append( group->units, (void*)grp );
				if( DaoParser_MakeMacroGroup( self, grp, parent, i+1, rb, vars, markers ) == 0 ) return 0;
				i = rb;
				continue;
			}
		}
		i ++;
	}
	return 1;
}
static void DMacroGroup_AddStop( DMacroGroup *self, DArray *stops )
{
	DMacroGroup *group;
	daoint i, j;
	for(i=self->units->size-1; i>=0; i--){
		DMacroUnit *unit = (DMacroUnit*) self->units->items.pVoid[i];
		if( unit->type == DMACRO_GRP || unit->type == DMACRO_ALT ){
			group = (DMacroGroup*) unit;
			DMacroGroup_AddStop( group, stops );
			if( group->repeat >= DMACRO_ZERO_OR_MORE ) break;
		}else{
			for(j=0; j<stops->size; j++){
#if 0
				//printf( "%s\n", stops->items.pString[j]->mbs );
#endif
				DArray_Append( unit->stops, stops->items.pString[j] );
			}
			break;
		}
	}
}
/* Backward traverse the macro units and set stopping tokens for each unit.
 * A stopping token is defined as the content of a macro unit of type DMACRO_TOK.
 */
static void DMacroGroup_SetStop( DMacroGroup *self, DArray *stops )
{
	DMacroGroup *group;
	daoint i, j;
	/*
	   printf( "stop : %i\n", stops->size );
	 */

	for(i=self->units->size-1; i>=0; i--){
		DMacroUnit *unit = (DMacroUnit*) self->units->items.pVoid[i];
		if( unit->type == DMACRO_GRP || unit->type == DMACRO_ALT ){
			group = (DMacroGroup*) unit;
			/* self->stops as temporary array: */
			DArray_Assign( self->stops, stops );
			/* recursive set stopping tokens for macro groups: */
			DMacroGroup_SetStop( group, self->stops );
			/* if the group has to be presented at least once,
			 * no propagating the stopping tokens to the previous macro units. */
			if( group->repeat > DMACRO_ZERO_OR_MORE ) DArray_Clear( stops );
			/* add stopping token, why only one ? XXX */
			if( group->stops->size >0) DArray_PushFront( stops, group->stops->items.pString[0] );
		}else if( unit->type == DMACRO_TOK ){
			/*
			   printf( "%s", unit->marker->mbs );
			 */
			DArray_Clear( stops );
			/* define a stopping token */
			DArray_Append( stops, unit->marker );
			DArray_Append( unit->stops, unit->marker );
		}else{
			/*
			   printf( "%s", unit->marker->mbs );
			 */
			for(j=0; j<stops->size; j++) DArray_Append( unit->stops, stops->items.pString[j] );
		}
		/*
		   printf( " : %i;  ", unit->stops->size );
		 */
	}
	if( self->repeat == DMACRO_ZERO_OR_MORE || self->repeat == DMACRO_ONE_OR_MORE ){
		/* this is fine for DMACRO_GRP unit, what about DMACRO_ALT unit? XXX */
		if( self->units->size >1 ){
			DMacroUnit *first = (DMacroUnit*) self->units->items.pVoid[0];
			DMacroGroup_AddStop( self, first->stops );
		}
	}
	DArray_Assign( self->stops, stops );
	/*
	   printf( "group : %i\n", self->stops->size );
	 */
}
static void DMacroGroup_FindVariables( DMacroGroup *self )
{
	DMacroGroup *group;
	daoint i, j;

	for(i=0; i<self->units->size; i++){
		DMacroUnit *unit = (DMacroUnit*) self->units->items.pVoid[i];
		if( unit->type == DMACRO_GRP || unit->type == DMACRO_ALT ){
			group = (DMacroGroup*) unit;
			DMacroGroup_FindVariables( group );
			for(j=0; j<group->variables->size; j++)
				DArray_Append( self->variables, group->variables->items.pVoid[j] );
		}else if( unit->type >= DMACRO_VAR && unit->type <= DMACRO_BL ){
			DArray_Append( self->variables, (void*)unit->marker );
		}
	}
	/*
	   for(j=0; j<self->variables->size; j++)
	   printf( "%p %s\n", self, self->variables->items.pString[j]->mbs );
	 */
}
int DaoParser_ParseMacro( DaoParser *self, int start )
{
	int rb1, rb2, i = start, N = self->tokens->size;
	DaoToken **toks = self->tokens->items.pToken;
	DMacroUnit *first;
	DaoMacro *macro;
	DString *lang = NULL;
	DArray  *stops;
	DMap  *markers;
	DNode *it;

	if( start + 5 >= N ) return -1;
	if( toks[start+1]->type != DTOK_LCB ){
		lang = & toks[start+1]->string;
		if( toks[start+1]->type != DTOK_IDENTIFIER ){
			DaoParser_Error( self, DAO_TOKEN_NEED_NAME, lang );
			return -1;
		}
		if( lang->size == 3 && strcmp( lang->mbs, "dao" ) == 0 ){
			DaoParser_Error( self, DAO_TOKEN_NEED_NAME, lang );
			return -1;
		}
		start += 1;
	}
	if( toks[start+1]->name != DTOK_LCB ) return -1;

	self->curLine = toks[start]->line;
	rb1 = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, -1 );
	if( rb1 <0 || rb1 +3 >= N ) return -1;
	if( toks[rb1+1]->name != DKEY_AS || toks[rb1+2]->name != DTOK_LCB ){
		DaoParser_Error( self, DAO_CTW_INV_MAC_DEFINE, NULL );
		return -1;
	}
	rb2 = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, rb1 + 1, -1 );
	if( rb2 <0 ) return -1;

	/*
	   for( i=start; i<rb2; i++ ) printf( "%s  ", toks[i]->string.mbs ); printf("\n");
	 */

	macro = DaoMacro_New();
	markers = DMap_New(D_STRING,0);

	if( DaoParser_MakeMacroGroup( self, macro->macroMatch, macro->macroMatch, start+2, rb1, markers, NULL ) ==0 ){
		goto Error;
	}
	if( macro->macroMatch->units->size == 0 ) goto Error;
	first = (DMacroUnit*) macro->macroMatch->units->items.pVoid[0];
	if( first->type != DMACRO_TOK ){
		DaoParser_Error( self, DAO_CTW_INV_MAC_FIRSTOK, & toks[i]->string );
		goto Error;
	}
	for(it=DMap_First(markers); it; it=DMap_Next(markers,it)){
		if( it->value.pInt > 1 ){
			DaoParser_Error( self, DAO_CTW_REDEF_MAC_MARKER, it->key.pString );
			goto Error;
		}
	}

	DMap_Clear( markers );
	if( toks[rb1+3]->line != toks[rb1+2]->line ) macro->macroApply->cpos = toks[rb1+3]->cpos;
	if( DaoParser_MakeMacroGroup( self, macro->macroApply, macro->macroApply, rb1+3, rb2, NULL, markers ) ==0 ){
		goto Error;
	}
	for(it=DMap_First(markers); it; it=DMap_Next(markers,it)){
		DArray_Append( macro->keyListApply, it->key.pString );
	}

	stops = DArray_New(D_TOKEN);
	DMacroGroup_SetStop( macro->macroMatch, stops );
	DMacroGroup_FindVariables( macro->macroMatch );
	DArray_Clear( stops );
	DMacroGroup_SetStop( macro->macroApply, stops );
	DaoNamespace_AddMacro( self->nameSpace, lang, & first->marker->string, macro );
	DArray_Delete( stops );
	DMap_Delete( markers );
	return rb2;
Error:
	DaoMacro_Delete( macro );
	DMap_Delete( markers );
	return -1;
}

typedef struct DMacroNode DMacroNode;

struct DMacroNode
{
	short       isLeaf;
	short       level;
	DArray     *nodes; /* <DMacroNode*> */
	DArray     *leaves; /* <DaoToken*> */
	DMacroNode *parent;
	DMacroGroup *group;
};

DMacroNode* DMacroNode_New( short leaf, short level )
{
	DMacroNode *self = (DMacroNode*) dao_malloc( sizeof( DMacroNode ) );
	self->isLeaf = leaf;
	self->level = level;
	self->nodes = DArray_New(0);
	self->leaves = DArray_New(D_TOKEN);
	self->parent = NULL;
	self->group = NULL;
	return self;
}
void DMacroNode_Delete( DMacroNode *self )
{
	DArray_Delete( self->nodes );
	DArray_Delete( self->leaves );
	dao_free( self );
}

static void DMacroNode_Print( DMacroNode *self )
{
	daoint i;
	printf( "{ %i lev=%i nodes=%llu: ", self->isLeaf, self->level, (unsigned long long)self->nodes->size );
	for(i=0; i<self->leaves->size; i++)
		printf( "%s, ", self->leaves->items.pToken[i]->string.mbs );
	for(i=0; i<self->nodes->size; i++){
		printf( "\nnode%" DAO_INT_FORMAT "\n", i );
		DMacroNode_Print( (DMacroNode*)self->nodes->items.pVoid[i] );
	}
	printf( " }" );
}
/*
 */

/* Matching a macro to source tokens.
 * For each macro variable such as $EXP and $VAR etc.
 * the matched tokens are stored in a tree, in which the leaves
 * are corresponding to the matched tokens and the branches
 * corresponding to the repetition of the matchings.
 *
 * For example,
 * key1 { \( key2 ( \( key3 $EXPR , \) \+ ) ; \) \* }
 * key1 { key2 ( key3 A+1 , key3 B-2, ); key2 ( key3 C*3, key3 D/4, key3 E+5, ); }
 *
 * the matching will generate such tree:
 *
 * key1:                                |
 *                                      |
 * \( key2 ... \) :           __________|______________
 *                           |                         |
 *                           |                         |
 * \( key3 ... \) :      ____|____            _________|_________
 *                      |         |          |         |         |
 *                      |         |          |         |         |
 *                      |         |          |         |         |
 * $EXPR:            {A,+,1}   {B,-,2}    {C,*,3}   {D,/,4}   {E,+,5}
 */
static int DaoParser_MacroMatch( DaoParser *self, int start, int end,
		DMacroGroup *group, DMap *tokMap, int level, DArray *all )
{
	DMacroUnit **units = (DMacroUnit**) group->units->items.pVoid;
	DMacroUnit  *unit;
	DMacroGroup *grp;
	DaoToken **toks = self->tokens->items.pToken;
	DNode  *kwnode;
	DMacroNode *node, *prev;
	int M, N = group->units->size;
	int i, j=0, k=0, m, min, from = start;
	int idt, gid = -1;

	/*
	   printf( "MacroMatch\n" );
	   printf( "from = %i\n", from );
	 */

	if( group->repeat != DMACRO_AUTO ){
		level ++;
		for(j=0,M=group->variables->size; j<M; j++){
			kwnode = MAP_Find( tokMap, & group->variables->items.pToken[j]->string );
			prev = (DMacroNode*)( kwnode ? kwnode->value.pVoid : NULL );
			node = DMacroNode_New( level==1, level );
			node->group = group;
			DArray_Append( all, node );
			MAP_Insert( tokMap, & group->variables->items.pToken[j]->string, node );
			while( prev && prev->group != group->parent ) prev = prev->parent;
			if( prev ){
				node->parent = prev;
				prev->isLeaf = 0;
				DArray_Append( prev->nodes, (void*) node );
			}
		}
	}

	for(i=0; i<N; i++){
		unit = units[i];
#if 0
		printf( "match unit %i (%i), type %i, from %i,  end %i\n", i, N, unit->type, from, end );
		if( unit->type < DMACRO_GRP ) printf( "marker: %s\n", unit->marker->string.mbs );
#endif
		if( from <0 ) return -100;
		switch( unit->type ){
		case DMACRO_TOK :
			if( from >= end ) return -100;
			if( ! DaoToken_EQ( toks[from], unit->marker ) ) return -23;
			switch( toks[from]->name ){
			case DTOK_LB :
				k = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, from, end );
				break;
			case DTOK_LSB :
				k = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, from, end );
				break;
			case DTOK_LCB :
				k = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, from, end );
				break;
			default : break;
			}
			switch( toks[from]->name ){
			case DTOK_LB :
			case DTOK_LCB :
			case DTOK_LSB :
				if( k <0 ) return -3;
				grp = (DMacroGroup*) units[i+1];
				j = DaoParser_MacroMatch( self, from+1, k, grp, tokMap, level, all );
				if( j > k ) return -4;
				from = k;
				i ++;
				break;
			default : from ++; break;
			}
			break;
		case DMACRO_EXP :
		case DMACRO_ID :
		case DMACRO_OP :
		case DMACRO_BL :
			if( from >= end ) return -100;
			switch( unit->type ){
			case DMACRO_EXP :
				self->curToken = from;
				j = DaoParser_ParseExpression( self, 0 );
				if( j < 0 ) return -100;
				j += 1;
				min = j + unit->stops->size;
				for(k=0,M=unit->stops->size; k<M; k++){
					DaoToken *stop = unit->stops->items.pToken[k];
					m = DaoParser_FindOpenToken2( self, stop, from, end );
#if 0
					//printf( "searching: %i %s %i\n", j, stop->mbs, end );
#endif
					if( min >= m && m >=0 ) min = m;
				}
				/* if there is extra tokens between expr and the first stop marker */
				if( j < 0 || min > j+1 ) return -5;
				if( min < j ) j = min;
				break;
			case DMACRO_ID :
				if( toks[from]->type != DTOK_IDENTIFIER ) return -1;
				j = from +1;
				break;
			case DMACRO_OP :
				if( toks[from]->name < DTOK_ADD || toks[from]->name > DTOK_DECR )
					return -1;
				j = from +1;
				break;
			case DMACRO_BL :
				j = end;
				min = j + unit->stops->size;
				for(k=0,M=unit->stops->size; k<M; k++){
					DaoToken *stop = unit->stops->items.pToken[k];
					m = DaoParser_FindOpenToken2( self, stop, from, end );
					/* printf( "searching: %i %s %i\n", j, stop->mbs, tokPos[j] ); */
					if( m < min && m >=0 ) min = m;
				}
				/* printf( "j = %i min = %i\n", j, min ); */
				if( min > j ) return -5;
				j = min;
				break;
			}

			/* key1 { \( key2 ( \( key3 $EXPR , \) \+ ) ; \) \* }
			 * key1 { key2 ( key3 A , key3 B, ) ; key2 ( key3 C, key3 D, ) ; }
			 * { level_1: { level_2, isLeaf: { A, B } }, { level_2, isLeaf: { C, D } } }
			 */
			kwnode = MAP_Find( tokMap, & unit->marker->string );
			prev = (DMacroNode*) kwnode->value.pVoid;
			while( prev && prev->group != group ) prev = prev->parent;
			node = DMacroNode_New( 1, level+1 );
			node->group = group;
			DArray_Append( all, node );
			MAP_Insert( tokMap, & unit->marker->string, node );
			node->parent = prev;
			if( prev ) DArray_Append( prev->nodes, node );
			for(k=from; k<j; k++) DArray_Append( node->leaves, toks[k] );

			/*
			   DMacroNode_Print( node );
			   kwnode = DMap_Find( tokMap, (void*)unit->marker );
			   node = (DMacroNode*) kwnode->value.pVoid;
			   while( node->parent ) node = node->parent;
			   printf( "\n" );
			 */
			from = j;
			break;
		case DMACRO_GRP :
		case DMACRO_ALT :
			grp = (DMacroGroup*) unit;
			j = DaoParser_MacroMatch( self, from, end, grp, tokMap, level, all );
			switch( grp->repeat ){
			case DMACRO_AUTO :
			case DMACRO_ONE :
				if( from >= end ) return -100;
				if( j <0 && group->type != DMACRO_ALT ) return -6;
				if( j >=0 ){
					from = j;
					gid = i;
				}
				break;
			case DMACRO_ZERO :
				if( j >=0 && group->type != DMACRO_ALT ) return -7;
				if( j <0 ) gid = i;
				break;
			case DMACRO_ZERO_OR_ONE :
				if( j >=0 && group->type != DMACRO_ALT ) from = j;
				gid = i;
				break;
			case DMACRO_ZERO_OR_MORE :
				gid = i;
				while( j >=0 ){
					from = j;
					j = DaoParser_MacroMatch( self, from, end, grp, tokMap, level, all );
				}
				break;
			case DMACRO_ONE_OR_MORE :
				if( from >= end ) return -100;
				if( j <0 && group->type != DMACRO_ALT ) return -8;
				while( j >=0 ){
					gid = i;
					from = j;
					j = DaoParser_MacroMatch( self, from, end, grp, tokMap, level, all );
				}
				break;
			}
			break;
		default : return -9;
		}
		if( group->type == DMACRO_ALT && gid >=0 ) break;
	}
	if( group->repeat != DMACRO_AUTO ) level --;
	if( group->type == DMACRO_ALT && gid <0 ) return -1;
	return from;
}
/* Find a leaf at the level.
 */
static DMacroNode* DMacroNode_FindLeaf( DMacroNode *self, DMap *check, int level )
{
	DNode *used = DMap_Find( check, self );
	DMacroNode *node;
	daoint j;
	/* for variable not in a group */
	if( self->level == level+1 && self->leaves->size && used ==NULL ) return self;
	for(j=0; j<self->nodes->size; j++){
		node = (DMacroNode*) self->nodes->items.pVoid[j];
		if( node->level == level+1 ){ /* leaf is a node inside of a node */
			used = DMap_Find( check, node );
			if( node->leaves->size && used == NULL ) return node;
		}
		node = DMacroNode_FindLeaf( node, check, level );
		if( node ) return node;
	}
	return NULL;
}
static int DMacroNode_LeavesAreEmpty( DMacroNode *self )
{
	DMacroNode *node;
	int empty = 1;
	daoint j;
	if( self->leaves->size ) return 0;
	if( self->nodes->size ==0 && self->leaves->size ==0 ) return 1;
	for(j=0; j<self->nodes->size; j++){
		node = (DMacroNode*) self->nodes->items.pVoid[j];
		if( DMacroNode_LeavesAreEmpty( node ) ==0 ){
			empty = 0;
			break;
		}
	}
	return empty;
}
static void DMacroNode_RemoveEmptyLeftBranch( DMacroNode *self, int level )
{
	DMacroNode *node = self;
	while( node->level < level-1 && node->nodes->size )
		node = (DMacroNode*) node->nodes->items.pVoid[0];
	if( node->level == level-1 && DMacroNode_LeavesAreEmpty( node ) ){
		if( node->parent ) DArray_PopFront( node->parent->nodes );
	}
}
static int DaoParser_MacroApply( DaoParser *self, DArray *tokens,
		DMacroGroup *group, DMap *tokMap, DMap *used,
		int level, DString *tag, int pos0, int adjust )
{
	DMacroUnit **units = (DMacroUnit**) group->units->items.pVoid;
	DMacroUnit  *unit;
	DMacroGroup *grp;
	DMacroNode *node, *node2;
	DArray *toks = DArray_New(D_TOKEN);
	DaoToken *tk = DaoToken_New();
	DaoToken *tt = NULL;
	DNode  *kwnode = NULL;
	DMap *check = NULL;
	DMap one = { NULL, 0, 0, 0 };
	int M, N = group->units->size;
	int i, j, gid = -1;
	int repeated = 0;
	int start_mbs = -1;
	int start_wcs = -1;

	if( group->repeat != DMACRO_AUTO ) level ++;

	for( i=0; i<N; i++ ){
		unit = units[i];
		if( tokens->size >0 ) pos0 = tokens->items.pToken[ tokens->size -1 ]->line;
		self->curLine = pos0;
		/*
		   printf( "apply unit %i: %i\n", i, unit->type );
		 */
		switch( unit->type ){
		case DMACRO_TOK :
			DArray_Append( tokens, unit->marker );
			tokens->items.pToken[ tokens->size-1 ]->cpos += adjust;
			break;
		case DMACRO_VAR :
			DaoToken_Assign( tk, unit->marker );
			DString_Append( & tk->string, tag );
			DArray_Append( tokens, tk );
			break;
		case DMACRO_EXP :
		case DMACRO_ID :
		case DMACRO_OP :
		case DMACRO_BL :

			kwnode = MAP_Find( tokMap, & unit->marker->string );
			if( kwnode ==NULL ){
				DaoParser_Error( self, DAO_CTW_UNDEF_MAC_MARKER, & unit->marker->string );
				goto Failed;
			}
			node = (DMacroNode*) kwnode->value.pVoid;
			kwnode = MAP_Find( used, unit );
			if( kwnode == NULL ){
				DMap_Insert( used, unit, & one );
				kwnode = MAP_Find( used, unit );
			}
			check = (DMap*) kwnode->value.pVoid;
			repeated = 1;

			/*
			   printf( ">>>\n%s level %i: \n", unit->marker->string.mbs, level );
			   DMacroNode_Print( node );
			   printf( "\n" );
			 */
			/* search a leaf */
			node2 = DMacroNode_FindLeaf( node, check, level );
			if( node2 ){
				/*
				   printf( "appending tokens\n" );
				   DMacroNode_Print( node2 );
				   printf( "\n" );
				 */
				DArray_InsertArray( tokens, tokens->size, node2->leaves, 0, -1 );
				DMap_Insert( check, node2, NULL );
				/* DArray_Clear( node2->leaves ); */
			}else{
				DMacroNode_RemoveEmptyLeftBranch( node, level );
				goto Failed;
			}
			break;
		case DMACRO_GRP :
		case DMACRO_ALT :
			grp = (DMacroGroup*) unit;
			DArray_Clear( toks );
			j = DaoParser_MacroApply( self, toks, grp, tokMap, used, level, tag, pos0, adjust );
			switch( grp->repeat ){
			case DMACRO_AUTO :
			case DMACRO_ONE :
				if( j <0 && group->type != DMACRO_ALT ) goto Failed;
				repeated = (j>0);
				if( j >=0 ){
					gid = i;
					DArray_InsertArray( tokens, tokens->size, toks, 0, -1 );
				}
				break;
			case DMACRO_ZERO_OR_ONE :
				gid = i;
				repeated = (j>0);
				if( j >=0 ){
					DArray_InsertArray( tokens, tokens->size, toks, 0, -1 );
				}
				break;
			case DMACRO_ZERO_OR_MORE :
				gid = i;
				repeated = (j>0);
				if( j >=0 ){
					DArray_InsertArray( tokens, tokens->size, toks, 0, -1 );
				}
				while( j >0 ){
					DArray_Clear( toks );
					j = DaoParser_MacroApply( self, toks, grp, tokMap, used, level, tag, pos0, adjust );
					if( j >0 ){
						DArray_InsertArray( tokens, tokens->size, toks, 0, -1 );
					}
				}
				break;
			case DMACRO_ONE_OR_MORE :
				if( j <0 && group->type != DMACRO_ALT ) goto Failed;
				repeated = (j>0);
				if( j >=0 ){
					DArray_InsertArray( tokens, tokens->size, toks, 0, -1 );
				}

				while( j >0 ){
					gid = i;
					DArray_Clear( toks );
					j = DaoParser_MacroApply( self, toks, grp, tokMap, used, level, tag, pos0, adjust );
					if( j >0 ){
						DArray_InsertArray( tokens, tokens->size, toks, 0, -1 );
					}
				}
				break;
			}
			break;
		default : goto Failed;
		}
		if( group->type == DMACRO_ALT && gid >=0 ) break;
	}
	if( group->repeat != DMACRO_AUTO ) level --;
	if( group->type == DMACRO_ALT && gid <0 ) goto Failed;
	DaoToken_Delete( tk );
	DArray_Delete( toks );
	return repeated;
Failed :
	DaoToken_Delete( tk );
	DArray_Delete( toks );
	return -1;
}
int DaoParser_MacroTransform( DaoParser *self, DaoMacro *macro, int start, int tag )
{
	DNode *it;
	DString *mbs = DString_New(1);
	DArray *toks = DArray_New(D_TOKEN);
	DArray *all = DArray_New(0);
	DMap *tokMap = DMap_New(D_STRING,0);
	DMap *used = DMap_New(0,D_MAP);
	int j, p0, lev = 0, adjust=0;
	char buf[20];
	daoint i;

	sprintf( buf, " %p %x", self->nameSpace, tag );
	DString_SetMBS( mbs, buf );

	j = DaoParser_MacroMatch( self, start, self->tokens->size, macro->macroMatch, tokMap, lev, all );
	/*
	   printf( "MacroTransform %i\n", j );
	 */
	if( j <0 ) goto Failed;

	for( it = DMap_First( tokMap ); it != NULL; it = DMap_Next( tokMap, it ) ){
		DMacroNode *node = (DMacroNode*) it->value.pVoid;
		while( node->parent ) node = node->parent;
		it->value.pVoid = node;
	}

	lev = 0;
	p0 = self->tokens->items.pToken[start]->line;
	adjust = self->tokens->items.pToken[start]->cpos - macro->macroApply->cpos;
	if( DaoParser_MacroApply( self, toks, macro->macroApply, tokMap, used, lev, mbs, p0, adjust ) <0 )
		goto Failed;

	/*
	   for(i=0; i<toks->size; i++) printf( "%s  ", toks->items.pToken[i]->string.mbs );
	   printf( "\n" );
	 */
	DArray_Erase( self->tokens, start, j-start );
	DArray_InsertArray( self->tokens, start, toks, 0, -1 );
	/*
	   for(i=0; i<toks->size; i++){
	   DArray_Insert( self->tokStr, (void*)toks->items.pString[i], start+i );
	   DArray_Insert( self->tokPos, (void*)poss->items.pInt[i], start+i );
	   }
	 */
	j = toks->size;
	DString_Delete( mbs );
	for(i=0; i<all->size; i++) DMacroNode_Delete( (DMacroNode*) all->items.pVoid[i] );
	DArray_Delete( all );
	DArray_Delete( toks );
	DMap_Delete( tokMap );
	DMap_Delete( used );
	return start + j;
Failed :
	DString_Delete( mbs );
	for(i=0; i<all->size; i++) DMacroNode_Delete( (DMacroNode*) all->items.pVoid[i] );
	DArray_Delete( all );
	DArray_Delete( toks );
	DMap_Delete( tokMap );
	DMap_Delete( used );
	return -1;
}

/* parsing without code generation: */
extern DOper daoArithOper[DAO_NOKEY2];
int DaoParser_CurrentTokenType( DaoParser *self );
int DaoParser_CurrentTokenName( DaoParser *self );
int DaoParser_NextTokenName( DaoParser *self );
int DaoParser_GetOperPrecedence( DaoParser *self );

static int DaoParser_ParsePrimary( DaoParser *self, int stop );
static int DaoParser_ParseExpression( DaoParser *self, int stop );
static int DaoParser_ParseParenthesis( DaoParser *self )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int start = self->curToken;
	int end = self->tokens->size-1;
	int rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, end );
	int comma = DaoParser_FindOpenToken( self, DTOK_COMMA, start+1, end, 0 );
	int newpos = -1;

	if( rb > 0 && rb < end && tokens[start+1]->type == DTOK_IDENTIFIER ){
		self->curToken = rb + 1;
		newpos = DaoParser_ParsePrimary( self, 0 );
		if( newpos >= 0 ) return newpos;//XXX
	}
	self->curToken = start + 1;
	if( rb >=0 && comma >= 0 && comma < rb ){
		/* tuple enumeration expression */
		self->curToken = rb + 1;
		return rb;
	}
	newpos = DaoParser_ParseExpression( self, 0 );
	if( newpos < 0 ) return -1;
	if( DaoParser_CurrentTokenName( self ) != DTOK_RB ) return -1;
	self->curToken += 1;
	return newpos;
}
static int DaoParser_ParsePrimary( DaoParser *self, int stop )
{
	DaoToken **tokens = self->tokens->items.pToken;
	unsigned char tkn, tki, tki2;
	int size = self->tokens->size;
	int start = self->curToken;
	int rb, end = size - 1;

	/*
	   for(i=start;i<=end;i++) printf("%s  ", tokens[i]->string.mbs);printf("\n");
	 */
	if( start >= size ) return -1;
	tkn = tokens[start]->type;
	tki = tokens[start]->name;
	tki2 = DaoParser_NextTokenName( self );
	if( tki == DTOK_IDENTIFIER && tki2 == DTOK_FIELD ){
		DString *field = & tokens[start]->string;
		if( DaoToken_IsValidName( field->mbs, field->size ) ==0 ) return -1;
		self->curToken += 2;
		return DaoParser_ParseExpression( self, stop );
	}else if( tki == DKEY_TYPE && tki2 == DTOK_LB ){
		rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
		if( rb < 0 ) return -1;
		start = rb + 1;
	}else if( tki >= DKEY_ARRAY && tki <= DKEY_LIST && tki2 == DTOK_LCB ){
		rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
		if( rb < 0 ) return -1;
		start = rb + 1;
	}else if( tki == DTOK_LCB ){
		rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
		if( rb < 0 ) return -1;
		start = rb + 1;
	}else if( tki == DTOK_LSB ){
		rb = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, start, end );
		if( rb < 0 ) return -1;
		start = rb + 1;
	}else if( tki == DTOK_LB ){
		start = DaoParser_ParseParenthesis( self );
	}else if( tki == DTOK_AT || tki == DKEY_ROUTINE ){
		/* closure expression */
		self->curToken += 1;
		if( DaoParser_CurrentTokenName( self ) != DTOK_LB ) return -1;
		rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, self->curToken, end );
		if( rb < 0 || (rb+2) > end ) return -1;
		if( tokens[rb+1]->type != DTOK_LCB ) return -1;
		rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, rb+1, end );
		if( rb < 0 ) return -1;
		start = rb + 1;
	}else if( tki == DKEY_YIELD ){
		rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
		if( rb < 0 ) return -1;
		start = rb + 1;
	}else if( tki2 == DTOK_LB && (tki >= DKEY_ABS && tki <= DKEY_TANH) ){
		/* built-in math functions */
		rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, end );
		if( rb < 0 ) return -1;
		start = rb + 1;
	}else if( tki == DTOK_ID_THTYPE && tki2 == DTOK_LB ){
		start += 1;
	}else if( (tki >= DTOK_IDENTIFIER && tki <= DTOK_WCS) || tki == DTOK_COLON || tki >= DKEY_ABS || tki == DKEY_SELF ){
		start += 1;
	}
	if( start < 0 ) return -1;
	self->curToken = start;
	while( self->curToken < self->tokens->size ){
		start = self->curToken;
		switch( DaoParser_CurrentTokenName( self ) ){
		case DTOK_LB :
			rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, end );
			if( rb < 0 ) return -1;
			if( (rb+1) <= end && tokens[rb+1]->name == DTOK_BANG2 ) rb += 1;
			self->curToken = rb + 1;
			break;
		case DTOK_LCB :
			rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
			if( rb < 0 ) return -1;
			self->curToken = rb + 1;
			break;
		case DTOK_LSB :
			rb = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, start, end );
			if( rb < 0 ) return -1;
			self->curToken = rb + 1;
			break;
		case DTOK_DOT : case DTOK_COLON2 : case DTOK_ARROW :
			self->curToken += 1;
			if( DaoParser_CurrentTokenType( self ) == DTOK_LCB ) break;
			if( DaoParser_CurrentTokenType( self ) != DTOK_IDENTIFIER ) return -1;
			self->curToken += 1;
			break;
		default : return self->curToken - 1;
		}
	}
	return self->curToken - 1;
}
static int DaoParser_ParseUnary( DaoParser *self, int stop )
{
	int tok = DaoParser_CurrentTokenName( self );
	if( daoArithOper[ tok ].left == 0 ) return DaoParser_ParsePrimary( self, stop );
	/* parse left hand unary operator */
	self->curToken += 1;
	return DaoParser_ParseUnary( self, stop );
}
static int DaoParser_ParseOperator( DaoParser *self, int LHS, int prec, int stop )
{
	DaoToken **tokens = self->tokens->items.pToken;
	int oper, RHS, thisPrec, nextPrec;

	while(1){
		int pos = self->curToken;
		if( DaoParser_CurrentTokenName( self ) == stop ) return LHS;
		thisPrec = DaoParser_GetOperPrecedence( self );
		if(thisPrec < prec) return LHS;

		oper = daoArithOper[ tokens[self->curToken]->name ].oper;
		self->curToken += 1; /* eat the operator */

		RHS = DaoParser_ParseUnary( self, stop );
		if( RHS < 0 ){
			if( oper != DAO_OPER_COLON || self->curToken > pos + 1 ) return RHS;
			RHS = self->curToken - 1;
		}
		if( oper == DAO_OPER_IF ){ /* conditional operation:  c ? e1 : e2 */
			int RHS1, RHS2, prec2 = 10*(20 - daoArithOper[DTOK_COLON].binary);
			RHS1 = DaoParser_ParseOperator(self, RHS, prec2 + 1, DTOK_COLON );
			if( RHS1 < 0 ) return RHS1;
			self->curToken += 1;
			RHS2 = DaoParser_ParseUnary( self, DTOK_COLON );
			if( RHS2 < 0 ) return RHS2;
			RHS2 = DaoParser_ParseOperator(self, RHS2, prec2 + 1, DTOK_COLON );
			if( RHS2 < 0 ) return RHS2;
			LHS = RHS2;
			continue;
		}
		nextPrec = DaoParser_GetOperPrecedence( self );
		if (thisPrec < nextPrec) {
			RHS = DaoParser_ParseOperator(self, RHS, thisPrec+1, stop );
			if( RHS < 0 ) return RHS;
		}
		LHS = RHS;
	}
	return LHS;
}
static int DaoParser_ParseExpression( DaoParser *self, int stop )
{
	int LHS = self->curToken;
	if( DaoParser_CurrentTokenType( self ) != DTOK_COLON )
		LHS = DaoParser_ParseUnary( self, stop );
	if( LHS < 0 ) return LHS;
	return DaoParser_ParseOperator( self, LHS, 0, stop );
}

#endif
