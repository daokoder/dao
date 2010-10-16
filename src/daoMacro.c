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
extern void DaoParser_Warn( DaoParser *self, int code, DString *ext );
extern void DaoParser_Error( DaoParser *self, int code, DString *ext );
extern int DaoParser_FindPhraseEnd( DaoParser *self, int start, int end );
extern int DaoParser_FindOpenToken( DaoParser *self, uchar_t tok, int start, int end/*=-1*/, int warn/*=1*/ );
extern int DaoParser_FindPairToken( DaoParser *self,  uchar_t lw, uchar_t rw, int start, int stop/*=-1*/ );

static void DaoToken_Assign( DaoToken *self, DaoToken *other )
{
	self->type = other->type;
	self->name = other->name;
	self->line = other->line;
	self->cpos = other->cpos;
	if( self->string == NULL ) self->string = DString_Copy( other->string );
	DString_Assign( self->string, other->string );
}
static int DaoToken_EQ( DaoToken *self, DaoToken *other )
{
	if( self->name != other->name ) return 0;
	if( self->name <= DTOK_WCS ) return DString_EQ( self->string, other->string );
	return 1;
}

static int DaoParser_FindOpenToken2( DaoParser *self, DaoToken *tok, int start, int end/*=-1*/ )
{
	int i, n1, n2, n3, n4;
	DaoToken **tokens = self->tokens->items.pToken;

	if( start < 0 ) return -10000;
	if( end == -1 || end >= self->tokens->size ) end = self->tokens->size-1;
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
	self->indent = 0;
	self->stops = DArray_New(D_TOKEN);
	self->marker = DaoToken_New();
	self->marker->string = DString_New(1);
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
	self->indent = 0;
	self->units = DArray_New(0);
	self->stops = DArray_New(D_TOKEN);
	self->variables = DArray_New(D_TOKEN);
	self->parent = NULL;
	return self;
}
void DMacroGroup_Delete( DMacroGroup *self )
{
	int i;
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
	"macro", (void*) & baseCore, NULL, NULL, {0},
	(FuncPtrDel) DaoMacro_Delete, NULL
};

DaoMacro* DaoMacro_New()
{
	DaoMacro *self = (DaoMacro*) dao_malloc( sizeof(DaoMacro) );
	DaoBase_Init( self, DAO_MACRO );
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
	int i;
	if( self == self->firstMacro ){
		for( i=0; i<self->macroList->size; i++ ){
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

static int DaoParser_MakeMacroGroup( DaoParser *self,
		DMacroGroup *group, DMacroGroup *parent, int from, int to )
{
	DaoToken **toks = self->tokens->items.pToken;
	unsigned char tk;
	int i, sep, rb, prev;
	DMacroUnit *unit;
	DMacroGroup *grp, *group2; /* mingw don't like grp2 ?! */

	/*
	   for( i=from; i<to; i++ ) printf( "%s  ", toks[i]->mbs ); printf("\n");
	 */

	i = from;
	while( i < to ){
		char *chs = toks[i]->string->mbs;
		self->curLine = toks[i]->line;
		tk = toks[i]->name;
#if 0
		//printf( "%i %s\n", i, chs );
#endif
		if( tk == DTOK_ESC_LB || tk == DTOK_ESC_LSB || tk == DTOK_ESC_LCB ){
			grp = DMacroGroup_New();
			grp->cpos = toks[i]->cpos;
			grp->parent = parent;
			DArray_Append( group->units, (void*)grp );
			switch( tk ){
			case DTOK_ESC_LB :
				rb = DaoParser_FindPairToken( self, DTOK_ESC_LB, DTOK_ESC_RB, i, to );
				break;
			case DTOK_ESC_LSB :
				rb = DaoParser_FindPairToken( self, DTOK_ESC_LSB, DTOK_ESC_RSB, i, to );
				grp->repeat = DMACRO_ZERO_OR_ONE;
				break;
			case DTOK_ESC_LCB :
				rb = DaoParser_FindPairToken( self, DTOK_ESC_LCB, DTOK_ESC_RCB, i, to );
				grp->repeat = DMACRO_ZERO_OR_MORE;
				break;
			default :
				{
					rb = -1;
					DaoParser_Error( self, DAO_CTW_INV_MAC_OPEN, toks[i]->string );
					break;
				}
			}
			if( rb <0 ) return 0;

			prev = i+1;
			sep = DaoParser_FindOpenToken( self, DTOK_ESC_PIPE, i+1, rb, 0 );
			if( sep >=0 ){
				while( sep >=0 ){
					group2 = DMacroGroup_New();
					group2->parent = grp;
					if( DaoParser_MakeMacroGroup( self, group2, group2, prev, sep ) == 0 )
						return 0;
					DArray_Append( grp->units, (void*)group2 );
					prev = sep +1;
					sep = DaoParser_FindOpenToken( self, DTOK_ESC_PIPE, prev, rb, 0 );
					if( prev < rb && sep <0 ) sep = rb;
				}
				grp->type = DMACRO_ALT;
			}else if( DaoParser_MakeMacroGroup( self, grp, grp, i+1, rb ) == 0 ){
				return 0;
			}
			i = rb +1;
			self->curLine = toks[i]->line;
			if( toks[i]->string->mbs[0] == '\\' ){
				switch( toks[i]->name ){
				case DTOK_ESC_EXCLA : grp->repeat = DMACRO_ZERO; i++; break;
				case DTOK_ESC_QUES  : grp->repeat = DMACRO_ZERO_OR_ONE; i++; break;
				case DTOK_ESC_STAR  : grp->repeat = DMACRO_ZERO_OR_MORE; i++; break;
				case DTOK_ESC_PLUS  : grp->repeat = DMACRO_ONE_OR_MORE; i++; break;
				case DTOK_ESC_SQUO : case DTOK_ESC_DQUO :
				case DTOK_ESC_LB :  case DTOK_ESC_RB :
				case DTOK_ESC_LCB : case DTOK_ESC_RCB :
				case DTOK_ESC_LSB : case DTOK_ESC_RSB : break;
				default : DaoParser_Error( self, DAO_CTW_INV_MAC_REPEAT, toks[i]->string );
						  return 0;
				}
			}
			continue;
		}

		self->curLine = toks[i]->line;
		unit  = DMacroUnit_New();
		DaoToken_Assign( unit->marker, toks[i] );
		DArray_Append( group->units, (void*)unit );
		switch( chs[0] ){
		case '$' :
			if( DString_FindMBS( toks[i]->string, "EXP", 0 ) == 1 ){
				unit->type = DMACRO_EXP;
			}else if( DString_FindMBS( toks[i]->string, "VAR", 0 ) == 1 ){
				unit->type = DMACRO_VAR;
			}else if( DString_FindMBS( toks[i]->string, "ID", 0 ) == 1 ){
				unit->type = DMACRO_ID;
			}else if( DString_FindMBS( toks[i]->string, "OP", 0 ) == 1 ){
				unit->type = DMACRO_OP;
			}else if( DString_FindMBS( toks[i]->string, "BL", 0 ) == 1 ){
				unit->type = DMACRO_BL;
			}else if( DString_FindMBS( toks[i]->string, "IBL", 0 ) == 1 ){
				unit->type = DMACRO_IBL;
			}else{
				DaoParser_Error( self, DAO_CTW_INV_MAC_VARIABLE, toks[i]->string );
				return 0;
			}
			break;
		case '(' :
		case '[' :
		case '{' :
			switch( chs[0] ){
			case '(' :
				rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, i, to );
				break;
			case '[' :
				rb = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, i, to );
				break;
			case '{' :
				rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, i, to );
				break;
			default : rb = -1;
			}
			if( rb <0 ) return 0;

			grp = DMacroGroup_New();
			grp->parent = group;
			grp->repeat = DMACRO_AUTO;
			DArray_Append( group->units, (void*)grp );
			if( DaoParser_MakeMacroGroup( self, grp, parent, i+1, rb ) == 0 ) return 0;
			i = rb;
			continue;
		case '\'' :
			if( chs[2] == '\'' ){
				unit->marker->type = 0;
				switch( chs[1] ){
				case '(' : unit->marker->type = unit->marker->name = DTOK_LB; break;
				case ')' : unit->marker->type = unit->marker->name = DTOK_RB; break;
				case '[' : unit->marker->type = unit->marker->name = DTOK_LSB; break;
				case ']' : unit->marker->type = unit->marker->name = DTOK_RSB; break;
				case '{' : unit->marker->type = unit->marker->name = DTOK_LCB; break;
				case '}' : unit->marker->type = unit->marker->name = DTOK_RCB; break;
				case '\'' : unit->marker->type= unit->marker->name = DTOK_ESC_SQUO;break;
							case '\"' : unit->marker->type= unit->marker->name = DTOK_ESC_DQUO;break;
				default : break;
				}
				if( unit->marker->type == 0 ){
					DaoParser_Error( self, DAO_CTW_INV_MAC_SPECTOK, toks[i]->string );
					return 0;
				}
				unit->type = DMACRO_BR;
				DString_SetMBS( unit->marker->string, chs+1 );
				DString_Erase( unit->marker->string, unit->marker->string->size-1, 1 );
			}
		default : break;
		}
		if( i+1 < to && toks[i+1]->string->mbs[0] == '@' ){
			char ch = toks[i+1]->string->mbs[1];
			if( ch != '@' ){
				if( toks[i+1]->string->size != 2 || ch < '1' || ch >'9' ){
					DaoParser_Error( self, DAO_CTW_INV_MAC_INDENT, toks[i+1]->string );
					return 0;
				}
				unit->indent = ch - '0';
				i ++;
			}
		}
		i ++;
	}
	return 1;
}
static void DMacroGroup_AddStop( DMacroGroup *self, DArray *stops )
{
	DMacroGroup *group;
	int i, j;
	for( i=self->units->size-1; i>=0; i-- ){
		DMacroUnit *unit = self->units->items.pVoid[i];
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
 * XXX, also define stopping token by DMACRO_BR units?
 */
static void DMacroGroup_SetStop( DMacroGroup *self, DArray *stops )
{
	DMacroGroup *group;
	int i, j;
	/*
	   printf( "stop : %i\n", stops->size );
	 */

	for( i=self->units->size-1; i>=0; i-- ){
		DMacroUnit *unit = self->units->items.pVoid[i];
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
	int i, j;

	for( i=0; i<self->units->size; i++ ){
		DMacroUnit *unit = self->units->items.pVoid[i];
		if( unit->type == DMACRO_GRP || unit->type == DMACRO_ALT ){
			group = (DMacroGroup*) unit;
			DMacroGroup_FindVariables( group );
			for(j=0; j<group->variables->size; j++)
				DArray_Append( self->variables, group->variables->items.pVoid[j] );
		}else if( unit->type >= DMACRO_VAR && unit->type <= DMACRO_IBL ){
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
	DaoMacro *macro;
	DaoToken **toks = self->tokens->items.pToken;
	DArray   *stops;
	DMap     *markers;
	int N = self->tokens->size;
	int i = start;
	int rb1, rb2;

	if( start + 5 >= N ) return -1;
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
	   for( i=start; i<rb2; i++ ) printf( "%s  ", toks[i]->string->mbs ); printf("\n");
	 */

	macro = DaoMacro_New();

	if( DaoParser_MakeMacroGroup( self, macro->macroMatch, macro->macroMatch, start+2, rb1 ) ==0 ){
		DaoMacro_Delete( macro );
		return -1;
	}
	if( macro->macroMatch->units->size >0 ){
		DMacroUnit *unit = (DMacroUnit*) macro->macroMatch->units->items.pVoid[0];
		if( unit->type != DMACRO_TOK )
			DaoParser_Error( self, DAO_CTW_INV_MAC_FIRSTOK, toks[i]->string );
	}
	if( toks[rb1+3]->line != toks[rb1+2]->line ) macro->macroApply->cpos = toks[rb1+3]->cpos;
	if( DaoParser_MakeMacroGroup( self, macro->macroApply, macro->macroApply, rb1+3, rb2 ) ==0 ){
		DaoMacro_Delete( macro );
		return -1;
	}
	markers = DMap_New(D_STRING,0);

	for(i=start+2; i<rb1; i++){
		if( toks[i]->string->mbs[0] == '$' ){
			if( MAP_Find( markers, toks[i]->string ) != NULL ){
				self->curLine = toks[i]->line;
				DaoParser_Error( self, DAO_CTW_REDEF_MAC_MARKER, toks[i]->string );
				return 0;
			}
			MAP_Insert( markers, toks[i]->string, 0 );
		}
	}
	DMap_Clear( markers );
	i = rb1+3;
	if( DString_EQ( toks[start+2]->string, toks[rb1+3]->string ) ) i ++;
	while( i < rb2 ){
		char ch = toks[i]->string->mbs[0];
		if( ch != '$' && ch != '\\' && ch != '\'' ){
			if( MAP_Find( markers, toks[i]->string ) == NULL ){
				DArray_Append( macro->keyListApply, (void*)toks[i]->string );
				MAP_Insert( markers, toks[i]->string, 0 );
			}
		}
		i ++;
	}

	stops = DArray_New(D_TOKEN);
	DMacroGroup_SetStop( macro->macroMatch, stops );
	DMacroGroup_FindVariables( macro->macroMatch );
	DArray_Clear( stops );
	DMacroGroup_SetStop( macro->macroApply, stops );
	DaoNameSpace_AddMacro( self->nameSpace, toks[start+2]->string, macro );
	DArray_Delete( stops );
	DMap_Delete( markers );
	return rb2;
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
	int i;
	printf( "{ %i lev=%i nodes=%llu: ", self->isLeaf, self->level, (unsigned long long)self->nodes->size );
	for(i=0; i<self->leaves->size; i++)
		printf( "%s, ", self->leaves->items.pToken[i]->string->mbs );
	for(i=0; i<self->nodes->size; i++){
		printf( "\nnode%i\n", i );
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
		DMacroGroup *group, DMap *tokMap, int level, DArray *all, int indent[10] )
{
	DMacroUnit **units = (DMacroUnit**) group->units->items.pVoid;
	DMacroUnit  *unit;
	DMacroGroup *grp;
	DaoToken **toks = self->tokens->items.pToken;
	DNode  *kwnode;
	DMacroNode *node, *prev;
	int N = group->units->size;
	int i, j=0, k=0, m, min, from = start;
	int idt, gid = -1;

	/*
	   printf( "MacroMatch\n" );
	   printf( "from = %i\n", from );
	 */

	if( group->repeat != DMACRO_AUTO ){
		level ++;
		for(j=0; j<group->variables->size; j++){
			kwnode = MAP_Find( tokMap, group->variables->items.pToken[j]->string );
			prev = (DMacroNode*)( kwnode ? kwnode->value.pVoid : NULL );
			node = DMacroNode_New( level==1, level );
			node->group = group;
			DArray_Append( all, node );
			MAP_Insert( tokMap, group->variables->items.pToken[j]->string, node );
			while( prev && prev->group != group->parent ) prev = prev->parent;
			if( prev ){
				node->parent = prev;
				prev->isLeaf = 0;
				DArray_Append( prev->nodes, (void*) node );
			}
		}
	}

	for( i=0; i<N; i++ ){
		unit = units[i];
		idt = unit->indent;
#if 0
		printf( "match unit %i (%i), type %i, from %i,  end %i\n", i, N, unit->type, from, end );
		if( unit->type < DMACRO_GRP ) printf( "marker: %s\n", unit->marker->string->mbs );
#endif
		if( from <0 ) return -100;
		if( from < end && idt && indent[idt] >=0 && toks[from]->cpos != indent[idt] ) return -22;
		if( from < end && idt && indent[idt] < 0 ) indent[idt] = toks[from]->cpos;
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
				j = DaoParser_MacroMatch( self, from+1, k, grp, tokMap, level, all, indent );
				if( j > k ) return -4;
				from = k;
				i ++;
				break;
			default : from ++; break;
			}
			break;
		case DMACRO_BR :
			if( from >= end ) return -100;
			if( ! DaoToken_EQ( toks[from], unit->marker ) ) return -24;
			from ++;
			break;
		case DMACRO_EXP :
		case DMACRO_ID :
		case DMACRO_OP :
		case DMACRO_BL :
		case DMACRO_IBL :
			if( from >= end ) return -100;
			switch( unit->type ){
			case DMACRO_EXP :
				j = DaoParser_FindPhraseEnd( self, from, -1 ) + 1;
				min = j + unit->stops->size;
				for(k=0; k<unit->stops->size; k++){
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
				for(k=0; k<unit->stops->size; k++){
					DaoToken *stop = unit->stops->items.pToken[k];
					m = DaoParser_FindOpenToken2( self, stop, from, end );
					/* printf( "searching: %i %s %i\n", j, stop->mbs, tokPos[j] ); */
					if( m < min && m >=0 ) min = m;
				}
				/* printf( "j = %i min = %i\n", j, min ); */
				if( min > j ) return -5;
				j = min;
				break;
			case DMACRO_IBL :
				k = toks[from]->line;
				m = toks[from]->cpos;
				j = from;
				while( j > start && toks[j-1]->line == k ) j -= 1;
				if( j < from ) m = toks[j]->cpos + 1;
				j = from + 1;
				while( j < end && toks[j]->cpos >= m ) j += 1;
				/*
				   printf( "end = %i, j = %i\n", end, j );
				 */
				break;
			}

			/* key1 { \( key2 ( \( key3 $EXPR , \) \+ ) ; \) \* }
			 * key1 { key2 ( key3 A , key3 B, ) ; key2 ( key3 C, key3 D, ) ; }
			 * { level_1: { level_2, isLeaf: { A, B } }, { level_2, isLeaf: { C, D } } }
			 */
			kwnode = MAP_Find( tokMap, unit->marker->string );
			prev = (DMacroNode*) kwnode->value.pVoid;
			while( prev && prev->group != group ) prev = prev->parent;
			node = DMacroNode_New( 1, level+1 );
			node->group = group;
			DArray_Append( all, node );
			MAP_Insert( tokMap, unit->marker->string, node );
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
			j = DaoParser_MacroMatch( self, from, end, grp, tokMap, level, all, indent );
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
					j = DaoParser_MacroMatch( self, from, end, grp, tokMap, level, all, indent );
				}
				break;
			case DMACRO_ONE_OR_MORE :
				if( from >= end ) return -100;
				if( j <0 && group->type != DMACRO_ALT ) return -8;
				while( j >=0 ){
					gid = i;
					from = j;
					j = DaoParser_MacroMatch( self, from, end, grp, tokMap, level, all, indent );
				}
				break;
			}
			break;
		default : return -9;
		}
#if 0
		printf( "end = %i, j = %i,   %i   %i\n", end, from, unit->type, DMACRO_IBL );
		//if( unit->type == DMACRO_IBL && from >= end ) break;
#endif
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
	int j;
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
	int j;
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
	DString *mbs = DString_New(1);
	DaoToken tk = {0,0,0,0,0,NULL};
	DaoToken *tt = NULL;
	DNode  *kwnode = NULL;
	DMap *check = NULL;
	DMap one = { NULL, 0, 0, 0 };
	int N = group->units->size;
	int i, j, gid = -1;
	int repeated = 0;
	int start_mbs = -1;
	int start_wcs = -1;
	int squote, dquote;

	tk.string = mbs;
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
			squote = unit->marker->type == DTOK_ESC_SQUO;
			dquote = unit->marker->type == DTOK_ESC_DQUO;
			if( (squote && start_mbs >=0) || (dquote && start_wcs >=0) ){
				int qstart = squote ? start_mbs : start_wcs;
				tt = tokens->items.pToken[ qstart ];
				for(j=qstart+1; j<tokens->size; j++){
					DaoToken *jtok = tokens->items.pToken[j];
					int t = j ? tokens->items.pToken[j-1]->type : 0;
					if( t == DTOK_IDENTIFIER && jtok->type == t )
						DString_AppendChar( tt->string, ' ' );
					DString_Append( tt->string, jtok->string );
				}
				if( squote ){
					DString_AppendChar( tt->string, '\'' );
					DArray_Erase( tokens, start_mbs+1, tokens->size );
				}else{
					DString_AppendChar( tt->string, '\"' );
					DArray_Erase( tokens, start_wcs+1, tokens->size );
				}
				start_mbs = -1;
				break;
			}else if( squote ){
				start_mbs = tokens->size;
				DArray_Append( tokens, unit->marker );
				tt = tokens->items.pToken[ start_mbs ];
				tt->type = tt->name = DTOK_MBS;
				DString_SetMBS( tt->string, "\'" );
				break;
			}else if( dquote ){
				start_wcs = tokens->size;
				DArray_Append( tokens, unit->marker );
				tt = tokens->items.pToken[ start_wcs ];
				tt->type = tt->name = DTOK_WCS;
				DString_SetMBS( tt->string, "\"" );
				break;
			}
			DArray_Append( tokens, unit->marker );
			tokens->items.pToken[ tokens->size-1 ]->cpos += adjust;
			break;
		case DMACRO_VAR :
			DaoToken_Assign( & tk, unit->marker );
			DString_Append( mbs, tag );
			DArray_Append( tokens, & tk );
			break;
		case DMACRO_EXP :
		case DMACRO_ID :
		case DMACRO_OP :
		case DMACRO_BL :
		case DMACRO_IBL :

			kwnode = MAP_Find( tokMap, unit->marker->string );
			if( kwnode ==NULL ){
				DaoParser_Error( self, DAO_CTW_UNDEF_MAC_MARKER, unit->marker->string );
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
			   printf( ">>>\n%s level %i: \n", unit->marker->string->mbs, level );
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
	DString_Delete( mbs );
	DArray_Delete( toks );
	return repeated;
Failed :
	DString_Delete( mbs );
	DArray_Delete( toks );
	return -1;
}
int DaoParser_MacroTransform( DaoParser *self, DaoMacro *macro, int start, int tag )
{
	DString *mbs = DString_New(1);
	DArray *toks = DArray_New(D_TOKEN);
	DArray *all = DArray_New(0);
	DMap *tokMap = DMap_New(D_STRING,0);
	DMap *used = DMap_New(0,D_MAP);
	DNode *it;
	int i, j, p0, lev = 0, adjust=0;
	int indent[10];
	char buf[20];

	sprintf( buf, " %p %x", self->nameSpace, tag );
	DString_SetMBS( mbs, buf );
	for(i=0; i<10; i++) indent[i] = -1;

	j = DaoParser_MacroMatch( self, start, self->tokens->size, macro->macroMatch, tokMap, lev, all, indent );
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
	   for(i=0; i<toks->size; i++) printf( "%s  ", toks->items.pToken[i]->string->mbs );
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

#endif
