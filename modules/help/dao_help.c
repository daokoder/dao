/*=========================================================================================
  This file is a part of the Dao standard modules.
  Copyright (C) 2011-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#include<stdlib.h>
#include<string.h>
#include<math.h>
#include"daoString.h"
#include"daoValue.h"
#include"daoRegex.h"
#include"daoParser.h"
#include"daoNamespace.h"
#include"daoVmspace.h"
#include"daoGC.h"

#define DAOX_TEXT_WIDTH 80
#define DAOX_TREE_WIDTH 100

enum DaoxHelpBlockType
{
	DAOX_HELP_NONE ,
	DAOX_HELP_TEXT ,
	DAOX_HELP_CODE ,
	DAOX_HELP_LIST ,
	DAOX_HELP_SECTION ,
	DAOX_HELP_SUBSECT ,
	DAOX_HELP_SUBSECT2 ,
	DAOX_HELP_COMMENT
};

enum DaoxCodeColor
{
	DAOX_BLACK,
	DAOX_WHITE,
	DAOX_RED,
	DAOX_GREEN,
	DAOX_BLUE,
	DAOX_YELLOW,
	DAOX_MAGENTA,
	DAOX_CYAN
};

const char* const dao_colors[8]
= { "black", "white", "red", "green", "blue", "yellow", "magenta", "cyan" };



typedef struct DaoxHelpBlock   DaoxHelpBlock;
typedef struct DaoxHelpEntry   DaoxHelpEntry;
typedef struct DaoxHelp        DaoxHelp;
typedef struct DaoxHelper      DaoxHelper;
typedef struct DaoxStream      DaoxStream;

struct DaoxHelpBlock
{
	DaoxHelpEntry *entry;

	unsigned type;
	DString *text;
	DString *lang;

	DaoxHelpBlock *next;
};
struct DaoxHelpEntry
{
	DaoxHelp       *help;
	DaoxHelpEntry  *parent;
	DArray         *nested2;
	DMap           *nested;

	DString        *name;
	DString        *title;
	DString        *author;
	DString        *license;
	DaoxHelpBlock  *first;
	DaoxHelpBlock  *last;
};
struct DaoxHelp
{
	DaoNamespace   *nspace;

	DMap           *entries;
	DaoxHelpEntry  *current;
};
struct DaoxHelper
{
	DaoxHelpEntry  *tree;
	DMap           *trees;
	DMap           *entries;
	DMap           *helps;
	DArray         *nslist;
	DString        *notice;
};
struct DaoxStream
{
	DaoNamespace  *nspace;
	DaoProcess    *process;
	DaoRegex      *regex;
	DaoStream     *stream;
	DMap          *mtypes;
	unsigned       section;  /* section index; */
	unsigned       subsect;  /* subsection index; */
	unsigned       subsect2; /* subsubsection index; */
	unsigned       offset;   /* offset in the current line; */
	char           last;     /* last char in the current line; */
};
DaoxHelper *daox_helper = NULL;
DaoValue *daox_cdata_helper = NULL;
DaoVmSpace *dao_vmspace = NULL;



static DaoxHelpBlock* DaoxHelpBlock_New()
{
	DaoxHelpBlock *self = (DaoxHelpBlock*) dao_malloc( sizeof(DaoxHelpBlock) );
	self->type = DAOX_HELP_TEXT;
	self->text = DString_New(1);
	self->lang = NULL;
	self->next = NULL;
	return self;
}
static void DaoxHelpBlock_Delete( DaoxHelpBlock *self )
{
	if( self->next ) DaoxHelpBlock_Delete( self->next );
	if( self->lang ) DString_Delete( self->lang );
	DString_Delete( self->text );
	dao_free( self );
}
static int DString_Break( DString *self, int at )
{
	int i = at - 15;
	if( i < 0 ) i = 0;
	if( at >= self->size ) return self->size;
	if( at == 0 || isspace( self->mbs[at-1] ) || isspace( self->mbs[at] ) ) return at;
	while( at > i && isspace( self->mbs[at-1] ) == 0 ) at -= 1;
	return at;
}
static void DaoxStream_WriteNewLine( DaoxStream *self, const char *text )
{
	DaoStream_WriteMBS( self->stream, text );
	DaoStream_WriteChar( self->stream, '\n' );
	self->offset = 0;
	self->last = 0;
}
static void DaoxStream_WriteInteger( DaoxStream *self, int i )
{
	char buf[20];
	sprintf( buf, "%i", i );
	DaoStream_WriteMBS( self->stream, buf );
	self->offset += strlen( buf );
	self->last = '0'; /* the actual value is irrelevant, but the type does; */
}
static void DaoxStream_WriteChar( DaoxStream *self, char ch )
{
	if( self->offset && isspace( self->last ) == 0 && isalnum( ch ) )
		DaoxStream_WriteChar( self, ' ' );
	DaoStream_WriteChar( self->stream, ch );
	self->offset += 1;
	self->last = ch;
}
static void DaoxStream_WriteMBS( DaoxStream *self, const char *mbs )
{
	int len = strlen( mbs );
	if( self->offset && isspace( self->last ) == 0 && isalnum( mbs[0] ) )
		DaoxStream_WriteChar( self, ' ' );
	DaoStream_WriteMBS( self->stream, mbs );
	self->offset += len;
	self->last = mbs[len-1];
}
static void DaoxStream_WriteString( DaoxStream *self, DString *text )
{
	if( self->offset && isspace( self->last ) == 0 && isalnum( text->mbs[0] ) )
		DaoxStream_WriteChar( self, ' ' );
	DaoStream_WriteString( self->stream, text );
	self->offset += text->size;
	if( text->size ) self->last = text->mbs[text->size-1];
}
static int DaoxStream_WriteItemID( DaoxStream *self, char type, int id )
{
	char buf[20];
	self->last = ' ';
	if( type == '-' ){
		DaoStream_WriteMBS( self->stream, "  *  " );
		self->offset += 5;
	}else{
		sprintf( buf, "% 3i. ", id );
		DaoStream_WriteMBS( self->stream, buf );
		self->offset += strlen( buf );
	}
	return 5;
}
static void DaoxStream_WriteParagraph( DaoxStream *self, DString *text, int offset, int width )
{
	DString *line = DString_New(1);
	daoint start, next;
	int i;
	for(start=next=0; start<text->size; start=next){
		start = DString_Break( text, start );
		next = DString_Break( text, start + width - self->offset );
		if( (next - start) <= 0 ){
			DaoxStream_WriteNewLine( self, "" );
			next = DString_Break( text, start + width - self->offset );
		}
		DString_SubString( text, line, start, next - start );
		while( self->offset < offset ) DaoxStream_WriteChar( self, ' ' );
		if( self->offset == offset ) DString_Trim( line );
		//DString_Chop( line );
		DaoxStream_WriteString( self, line );
	}
	//if( text->size && text->mbs[text->size-1] == '\n' ) DaoxStream_WriteNewLine( self, "" );
	DString_Delete( line );
}
static void DaoxStream_WriteText( DaoxStream *self, DString *text, int offset, int width )
{
	DString *paragraph = DString_New(1);
	daoint pos, last = 0;
	DString_ChangeMBS( text, "(^|[^\n])[\n]([^\n]|$)", "%1 %2", 0 );
	DString_ChangeMBS( text, "[\n]%s*[\n]", "\n\n", 0 );
	DString_ChangeMBS( text, "[ \t]+", " ", 0 );
	pos = DString_FindMBS( text, "\n\n", 0 );
	while( last < text->size ){
		DString_SubString( text, paragraph, last, pos - last + 1 );
		if( last ) DaoxStream_WriteNewLine( self, "\n" );
		DaoxStream_WriteParagraph( self, paragraph, offset, width );
		last = pos + 1;
		pos = DString_FindMBS( text, "\n\n", last );
		if( pos == MAXSIZE ) pos = text->size;
	}
	DString_Delete( paragraph );
}
static void DaoxStream_PrintLineNumber( DaoxStream *self, int line, int offset )
{
	int i, color;
	char sline[20];
	sprintf( sline, "%4i ", line );
	for(i=0; i<offset; i++) DaoxStream_WriteChar( self, ' ' );
	DaoStream_SetColor( self->stream, "white", "black" );
	DaoStream_WriteMBS( self->stream, sline );
	DaoStream_SetColor( self->stream, NULL, NULL );
	DaoStream_WriteChar( self->stream, ' ' );
}
static void DaoxStream_PrintCode( DaoxStream *self, DString *code, int offset )
{
	DString *string = DString_New(1);
	DArray *tokens = DArray_New(D_TOKEN);
	const char *bgcolor = "yellow"; 
	daoint i, j, pos, last, color, fgcolor;
	int line = 1, printedline = 0;
	int println = 1;

	if( self->offset && DString_FindChar( code, '\n', 0 ) != MAXSIZE )
		DaoxStream_WriteNewLine( self, "" );

	println = self->offset <= offset;
	if( println ) bgcolor = NULL;
	else if( isspace( self->last ) == 0 ) DaoxStream_WriteChar( self, ' ' );

	DaoToken_Tokenize( tokens, code->mbs, 0, 1, 1 );
	for(i=0; i<tokens->size; i++){
		DaoToken *tok = tokens->items.pToken[i];
		fgcolor = -1;
		if( line != printedline ){
			if( println ) DaoxStream_PrintLineNumber( self, line, offset );
			printedline = line;
		}
		switch( tok->name ){
		case DTOK_NEWLN :
			line += 1;
			break;
		case DTOK_DIGITS_HEX : case DTOK_DIGITS_DEC :
		case DTOK_NUMBER_HEX : case DTOK_NUMBER_DEC : case DTOK_NUMBER_SCI :
			fgcolor = DAOX_RED;
			break;
		case DTOK_VERBATIM :
		case DTOK_MBS : case DTOK_WCS :
		case DTOK_MBS_OPEN : case DTOK_WCS_OPEN :
		case DTOK_CMT_OPEN : case DTOK_COMMENT :
			fgcolor = DAOX_RED;
			if( tok->name == DTOK_CMT_OPEN || tok->name == DTOK_COMMENT ) fgcolor = DAOX_BLUE;
			last = 0;
			pos = DString_FindChar( tok->string, '\n', 0 );
			while( last < tok->string->size ){
				if( last ){
					printedline = line;
					if( println ) DaoxStream_PrintLineNumber( self, line, offset );
				}
				line += pos != MAXSIZE;
				if( pos == MAXSIZE ) pos = tok->string->size - 1;
				DString_SubString( tok->string, string, last, pos - last + 1 );
				DaoStream_SetColor( self->stream, dao_colors[fgcolor], bgcolor );
				DaoStream_WriteString( self->stream, string );
				DaoStream_SetColor( self->stream, NULL, NULL );
				last = pos + 1;
				pos = DString_FindChar( tok->string, '\n', pos + 1 );
			}
			fgcolor = -100;
			break;
		case DKEY_USE : case DKEY_LOAD : case DKEY_BIND :
		case DKEY_AS : 
		case DKEY_AND : case DKEY_OR : case DKEY_NOT :
		case DKEY_VIRTUAL :
			fgcolor = DAOX_CYAN;
			break;
		case DKEY_VAR : case DKEY_CONST : case DKEY_STATIC : case DKEY_GLOBAL :
		case DKEY_PRIVATE : case DKEY_PROTECTED : case DKEY_PUBLIC :
			fgcolor = DAOX_GREEN;
			break;
		case DKEY_IF : case DKEY_ELSE :
		case DKEY_WHILE : case DKEY_DO :
		case DKEY_FOR : case DKEY_IN :
		case DKEY_SKIP : case DKEY_BREAK : case DKEY_CONTINUE :
		case DKEY_TRY : case DKEY_RETRY : case DKEY_RAISE : case DKEY_CATCH :
		case DKEY_RETURN : case DKEY_YIELD :
		case DKEY_SWITCH : case DKEY_CASE : case DKEY_DEFAULT :
			fgcolor = DAOX_MAGENTA;
			break;
		case DKEY_ANY : case DKEY_NONE : case DKEY_ENUM :
		case DKEY_INT : case DKEY_LONG :
		case DKEY_FLOAT : case DKEY_DOUBLE :
		case DKEY_STRING : case DKEY_COMPLEX :
		case DKEY_LIST : case DKEY_MAP : case DKEY_TUPLE : case DKEY_ARRAY :
		case DKEY_CLASS : case DKEY_FUNCTION : case DKEY_ROUTINE : case DKEY_SUB :
		case DKEY_SYNTAX : 
		case DKEY_OPERATOR :
		case DKEY_SELF :
		case DTOK_ID_THTYPE :
		case DTOK_ID_SYMBOL :
			fgcolor = DAOX_GREEN;
			break;
		default: break;
		}
		if( fgcolor < -10 ) continue;
		if( fgcolor < 0 ){
			DaoStream_SetColor( self->stream, NULL, bgcolor );
			DaoStream_WriteString( self->stream, tok->string );
			DaoStream_SetColor( self->stream, NULL, NULL );
			continue;
		}
		DaoStream_SetColor( self->stream, dao_colors[fgcolor], bgcolor );
		DaoStream_WriteString( self->stream, tok->string );
		DaoStream_SetColor( self->stream, NULL, NULL );
	}
	if( println ) self->offset = 1000;
	else self->offset += code->size;
	DArray_Delete( tokens );
	DString_Delete( string );
}

static void DaoxStream_WriteBlock( DaoxStream *self, DString *text, int offset, int width, int islist, int listdep );

static void DaoxStream_WriteList( DaoxStream *self, DString *text, int offset, int width, int islist, int listdep )
{
	const char *pat = "^[\n]+[ \t]* (%-%-|==)";
	daoint pos, lb, last = 0, start = 0, end = text->size-1;
	int i, itemid = 1;

	if( DString_MatchMBS( text, pat, & start, & end ) ){
		DString *delim = DString_New(1);
		DString *item = DString_New(1);
		DString_SubString( text, delim, start, end - start + 1 );
		DString_ChangeMBS( delim, "^[\n][\n]+", "\n", 1 );
		while( last < text->size ){
			DString_SubString( text, item, last, start - last );
			DString_Trim( item );
			if( item->size ){
				DString_Erase( item, 0, 2 );
				DString_Trim( item );
				for(i=0; i<offset; i++) DaoxStream_WriteChar( self, ' ' );
				i = DaoxStream_WriteItemID( self, delim->mbs[delim->size-1], itemid ++ );
				DaoxStream_WriteBlock( self, item, offset+i, width, 0, listdep );
			}
			/* print newline for items except the last one: */
			if( start < text->size ) DaoxStream_WriteNewLine( self, "" );
			last = start;
			start = DString_Find( text, delim, last + delim->size );
			if( start == MAXSIZE ) start = text->size;
		}
	}else{
		DaoxStream_WriteBlock( self, text, offset, width, 0, listdep );
	}
	DaoxStream_WriteNewLine( self, "" );
}
static void DaoxStream_WriteBlock( DaoxStream *self, DString *text, int offset, int width, int islist, int listdep )
{
	daoint mtype, pos, lb, start, end, last = 0;
	DString *fgcolor = DString_New(1);
	DString *bgcolor = DString_New(1);
	DString *delim = DString_New(1);
	DString *cap = DString_New(1);
	DString *mode = DString_New(1);
	DString *part = DString_New(1);
	DNode *it;

	while( last < text->size ){
		start = last;
		end = text->size-1;
		if( DaoRegex_Match( self->regex, text, & start, & end ) ){
			DString_SubString( text, part, last, start - last );
			if( islist ) DString_Trim( part );
			DaoxStream_WriteText( self, part, offset, width );

			DString_SubString( text, delim, start, end - start + 1 );
			pos = DString_Find( text, delim, end );
			if( pos == MAXSIZE ) pos = text->size;
			last = pos + delim->size;
			DString_SubString( text, part, end + 1, pos - end - 1 );

			lb = DString_FindChar( text, '(', start );
			if( lb > end ) lb = MAXSIZE;

			DString_Clear( cap );
			DString_Clear( mode );
			if( lb != MAXSIZE ){
				daoint rb = DString_FindChar( text, ')', start );
				DString_SetDataMBS( cap, text->mbs + start + 2, lb - start - 2 );
				DString_SetDataMBS( mode, text->mbs + lb + 1, rb - lb - 1 );
				DString_Trim( mode );
			}else{
				daoint rb = DString_FindChar( text, ']', start );
				DString_SetDataMBS( cap, text->mbs + start + 2, rb - start - 2 );
			}
			DString_Trim( cap );
			//printf( "%s\n", cap->mbs );
			it = DMap_Find( self->mtypes, cap );
			mtype = DAOX_HELP_NONE;
			if( it ) mtype = it->value.pInt;
			if( mtype == DAOX_HELP_CODE ){
				DString_Trim( part );
				DaoxStream_PrintCode( self, part, offset );
				if( self->process && strcmp( mode->mbs, "dao" ) == 0 ){
					DaoxStream_WriteNewLine( self, "" );
					DaoStream_SetColor( self->stream, NULL, dao_colors[DAOX_YELLOW] );
					DaoProcess_Eval( self->process, self->nspace, part, 1 );
					DaoStream_SetColor( self->stream, NULL, NULL );
				}
			}else if( mtype >= DAOX_HELP_SECTION && mtype <= DAOX_HELP_SUBSECT2 ){
				switch( mtype ){
				case DAOX_HELP_SECTION :
					self->section += 1;
					self->subsect = 0;
					self->subsect2 = 0;
					break;
				case DAOX_HELP_SUBSECT :
					self->subsect += 1;
					self->subsect2 = 0;
					break;
				case DAOX_HELP_SUBSECT2 :
					self->subsect2 += 1;
					break;
				}
				if( self->offset ) DaoxStream_WriteNewLine( self, "" );
				DaoStream_SetColor( self->stream, dao_colors[DAOX_WHITE], dao_colors[DAOX_BLUE] );
				DaoxStream_WriteMBS( self, " " );
				DaoxStream_WriteInteger( self, self->section );
				if( self->subsect ){
					DaoxStream_WriteMBS( self, "." );
					DaoxStream_WriteInteger( self, self->subsect );
					if( self->subsect2 ){
						DaoxStream_WriteMBS( self, "." );
						DaoxStream_WriteInteger( self, self->subsect2 );
					}
				}
				DaoxStream_WriteMBS( self, " " );
				DaoStream_SetColor( self->stream, NULL, NULL );
				DaoStream_SetColor( self->stream, dao_colors[DAOX_WHITE], dao_colors[DAOX_GREEN] );
				DaoxStream_WriteMBS( self, " " );
				DaoxStream_WriteText( self, part, 0, width );
				DaoxStream_WriteMBS( self, " \n" );
				DaoStream_SetColor( self->stream, NULL, NULL );
			}else if( mtype == DAOX_HELP_LIST ){
				DaoxStream_WriteList( self, part, offset, width, 1, listdep+1 );
			}else if( mtype == DAOX_HELP_COMMENT ){
				/* skip comments; */
			}else{
				DString_Clear( fgcolor );
				DString_Clear( bgcolor );
				pos = DString_FindChar( cap, ':', 0 );
				if( pos == MAXSIZE ){
					DString_Assign( fgcolor, cap );
				}else{
					DString_SubString( cap, fgcolor, 0, pos );
					DString_SubString( cap, bgcolor, pos + 1, cap->size );
				}
				DString_Trim( fgcolor );
				DString_Trim( bgcolor );
				if( islist ) DString_Trim( part );
				if( self->offset && isspace( self->last ) == 0 ) DaoxStream_WriteChar( self, ' ' );
				DaoStream_SetColor( self->stream, fgcolor->mbs, bgcolor->mbs );
				DaoxStream_WriteText( self, part, offset, width );
				DaoStream_SetColor( self->stream, NULL, NULL );
			}
		}else{
			DString_SubString( text, part, last, text->size - last );
			if( islist ) DString_Trim( part );
			DaoxStream_WriteText( self, part, offset, width );
			break;
		}
	}
	DString_Delete( fgcolor );
	DString_Delete( bgcolor );
	DString_Delete( delim );
	DString_Delete( cap );
	DString_Delete( mode );
	DString_Delete( part );
}

static void DaoxHelpBlock_Print( DaoxHelpBlock *self, DaoStream *stream, DaoProcess *proc )
{
	const char *pat = "@ %[ %s* (| : | %w+%s*:) %s* %w+ %s* ( %( %s* (|%w+) %s* %) | ) [^%]]* %]";
	DString spat = DString_WrapMBS( pat );
	DString scode = DString_WrapMBS( "code" );
	DString slist = DString_WrapMBS( "list" );
	DString ssection = DString_WrapMBS( "section" );
	DString ssubsect = DString_WrapMBS( "subsection" );
	DString ssubsect2 = DString_WrapMBS( "subsubsection" );
	DaoxStream xstream;
	xstream.mtypes = DHash_New(D_STRING,0);
	xstream.regex = DaoRegex_New( & spat );
	xstream.nspace = self->entry->help->nspace;
	xstream.process = proc;
	xstream.stream = stream;
	xstream.section = 0;
	xstream.subsect = 0;
	xstream.subsect2 = 0;
	xstream.section = 0;
	xstream.offset = 0;
	xstream.last = 0;
	DMap_Insert( xstream.mtypes, & scode, (void*)(size_t)DAOX_HELP_CODE );
	DMap_Insert( xstream.mtypes, & slist, (void*)(size_t)DAOX_HELP_LIST );
	DMap_Insert( xstream.mtypes, & ssection, (void*)(size_t)DAOX_HELP_SECTION );
	DMap_Insert( xstream.mtypes, & ssubsect, (void*)(size_t)DAOX_HELP_SUBSECT );
	DMap_Insert( xstream.mtypes, & ssubsect2, (void*)(size_t)DAOX_HELP_SUBSECT2 );
	if( self->type == DAOX_HELP_TEXT ){
		DaoxStream_WriteBlock( & xstream, self->text, 0, DAOX_TEXT_WIDTH, 0, 0 );
	}else if( self->type == DAOX_HELP_CODE ){
		DaoxStream_PrintCode( & xstream, self->text, 0 );
		if( proc && (self->lang == NULL || strcmp( self->lang->mbs, "dao" ) == 0) ){
			DaoStream_WriteMBS( stream, "\n" );
			DaoStream_SetColor( stream, NULL, dao_colors[DAOX_YELLOW] );
			DaoProcess_Eval( proc, self->entry->help->nspace, self->text, 1 );
			DaoStream_SetColor( stream, NULL, NULL );
		}
	}
	DaoStream_WriteMBS( stream, "\n" );
	if( self->next ) DaoxHelpBlock_Print( self->next, stream, proc );
	DaoRegex_Delete( xstream.regex );
	DMap_Delete( xstream.mtypes );
}



static DaoxHelpEntry* DaoxHelpEntry_New( DString *name )
{
	DaoxHelpEntry *self = (DaoxHelpEntry*) dao_malloc( sizeof(DaoxHelpEntry) );
	self->title = NULL;
	self->author = NULL;
	self->license = NULL;
	self->name = DString_Copy( name );
	self->nested = DMap_New(D_STRING,0);
	self->nested2 = DArray_New(0);
	self->first = self->last = NULL;
	self->parent = NULL;
	return self;
}
static void DaoxHelpEntry_Delete( DaoxHelpEntry *self )
{
	daoint i;
	DArray *array = self->nested2;
	for(i=0; i<array->size; i++) DaoxHelpEntry_Delete( (DaoxHelpEntry*) array->items.pVoid[i] );
	if( self->first ) DaoxHelpBlock_Delete( self->first );
	if( self->title ) DString_Delete( self->title );
	if( self->author ) DString_Delete( self->author );
	if( self->license ) DString_Delete( self->license );
	DString_Delete( self->name );
	DArray_Delete( self->nested2 );
	DMap_Delete( self->nested );
	dao_free( self );
}
static void DaoxHelpEntry_AppendText( DaoxHelpEntry *self, DString *text )
{
	DaoxHelpBlock *block = DaoxHelpBlock_New();
	block->entry = self;
	block->type = DAOX_HELP_TEXT;
	DString_Assign( block->text, text );
	if( self->last ){
		self->last->next = block;
		self->last = block;
	}else{
		self->first = self->last = block;
	}
}
static void DaoxHelpEntry_AppendCode( DaoxHelpEntry *self, DString *code, DString *lang )
{
	DaoxHelpBlock *block = DaoxHelpBlock_New();
	block->entry = self;
	block->type = DAOX_HELP_CODE;
	block->lang = lang ? DString_Copy( lang ) : NULL;
	DString_Assign( block->text, code );
	if( self->last ){
		self->last->next = block;
		self->last = block;
	}else{
		self->first = self->last = block;
	}
}
static void DaoxHelpEntry_AppendList( DaoxHelpEntry *self, DString *text )
{
	DaoxHelpBlock *block = DaoxHelpBlock_New();
	block->entry = self;
	block->type = DAOX_HELP_LIST;
	DString_Assign( block->text, text );
	if( self->last ){
		self->last->next = block;
		self->last = block;
	}else{
		self->first = self->last = block;
	}
}
static void DaoxHelpEntry_Print( DaoxHelpEntry *self, DaoStream *stream, DaoProcess *proc )
{
	DaoxStream xstream;
	xstream.nspace = self->help->nspace;
	xstream.stream = stream;

	xstream.offset = 0;
	if( self->author ){
		DString *notice = DString_Copy( daox_helper->notice );
		DString_ChangeMBS( notice, "%$%( %s* author %s* %)", self->author->mbs, 0 );
		if( self->license ){
			DString_ChangeMBS( notice, "%$%( %s* license %s* %)", self->license->mbs, 0 );
		}else{
			DString_ChangeMBS( notice, "%$%( %s* license %s* %)", "Unspecified License", 0 );
		}
		DaoStream_SetColor( stream, dao_colors[DAOX_WHITE], dao_colors[DAOX_GREEN] );
		DaoxStream_WriteText( & xstream, notice, 0, DAOX_TEXT_WIDTH );
		DaoxStream_WriteNewLine( & xstream, "" );
		DaoStream_SetColor( stream, NULL, NULL );
	}

	xstream.offset = 0;
	DaoStream_SetColor( stream, dao_colors[DAOX_WHITE], dao_colors[DAOX_BLUE] );
	DaoStream_WriteMBS( stream, "[NAME]\n" );
	DaoStream_SetColor( stream, NULL, NULL );

	DaoStream_WriteString( stream, self->name );

	DaoStream_SetColor( stream, dao_colors[DAOX_WHITE], dao_colors[DAOX_BLUE] );
	DaoStream_WriteMBS( stream, "\n\n[TITLE]\n" );
	DaoStream_SetColor( stream, NULL, NULL );

	xstream.offset = 0;
	if( self->title ) DaoxStream_WriteText( & xstream, self->title, 0, DAOX_TEXT_WIDTH );

	DaoStream_SetColor( stream, dao_colors[DAOX_WHITE], dao_colors[DAOX_BLUE] );
	DaoStream_WriteMBS( stream, "\n\n[DESCRIPTION]\n" );
	DaoStream_SetColor( stream, NULL, NULL );

	xstream.offset = 0;
	if( self->first ) DaoxHelpBlock_Print( self->first, stream, proc );
	DaoStream_WriteMBS( stream, "\n" );
}
static int DaoxHelpEntry_GetNameLength( DaoxHelpEntry *self )
{
	daoint pos = DString_RFindChar( self->name, '.', -1 );;
	if( pos == MAXSIZE ) return self->name->size;
	return self->name->size - 1 - pos;
}
static void DaoxHelpEntry_PrintTree( DaoxHelpEntry *self, DaoStream *stream, DArray *offsets, int offset, int width, int root, int last )
{
	DArray *old = offsets;
	DString *line = DString_New(1);
	daoint i, len = DaoxHelpEntry_GetNameLength( self );
	int extra = root ? 2 : 5;
	int color, count = 0;
	int screen = DAOX_TREE_WIDTH;

	if( offsets == NULL ){
		offsets = DArray_New(0);
		len = self->name->size;
	}

	DString_Resize( line, offset );
	memset( line->mbs, ' ', line->size*sizeof(char) );
	for(i=0; i<offsets->size; i++) line->mbs[ offsets->items.pInt[i] ] = '|';
	if( root == 0 ) DString_AppendMBS( line, "|--" );
	DaoStream_WriteString( stream, line );

	count = line->size;
	DString_AppendMBS( line, self->name->mbs + self->name->size - len );
	DaoStream_SetColor( stream, dao_colors[DAOX_GREEN], NULL );
	DaoStream_WriteMBS( stream, line->mbs + count );
	DaoStream_SetColor( stream, NULL, NULL );

	count = line->size;
	for(i=len; i<(width+2); i++) DString_AppendChar( line, '-' );
	DString_AppendChar( line, '|' );
	DString_AppendMBS( line, " " );
	DaoStream_WriteMBS( stream, line->mbs + count );

	count = line->size;
	if( self->title ){
		DaoStream_SetColor( stream, dao_colors[DAOX_BLUE], NULL );
		DaoStream_WriteString( stream, self->name );
		DaoStream_SetColor( stream, NULL, NULL );

		if( count < (screen - 20) ){
			DString *chunk = DString_New(1);
			int TW = screen - count;
			DaoStream_WriteMBS( stream, ": " );
			if( (self->title->size + self->name->size + 2) < TW ){
				DaoStream_WriteString( stream, self->title );
			}else{
				int next = 0;
				int start = DString_Break( self->title, TW - (self->name->size + 2) );
				DString_SubString( self->title, chunk, 0, start );
				DString_Trim( chunk );
				DaoStream_WriteString( stream, chunk );
				if( last )
					memset( line->mbs + offset, ' ', (width + extra)*sizeof(char) );
				else
					memset( line->mbs + offset + 1, ' ', (width + extra - 1)*sizeof(char) );
				for(; start < self->title->size; start=next){
					start = DString_Break( self->title, start );
					next = DString_Break( self->title, start + TW );
					DString_SubString( self->title, chunk, start, next - start );
					DString_Trim( chunk );
					DaoStream_WriteChar( stream, '\n' );
					DaoStream_WriteString( stream, line );
					DaoStream_WriteString( stream, chunk );
				}
			}
			DString_Delete( chunk );
		}
	}
	DaoStream_WriteChar( stream, '\n' );
	if( last == 0 ) DArray_Append( offsets, offset );
	len = 0;
	for(i=0; i<self->nested2->size; i++){
		DaoxHelpEntry *entry = (DaoxHelpEntry*) self->nested2->items.pVoid[i];
		int W = DaoxHelpEntry_GetNameLength( entry );
		if( W > len ) len = W;
	}
	for(i=0; i<self->nested2->size; i++){
		DaoxHelpEntry *entry = (DaoxHelpEntry*) self->nested2->items.pVoid[i];
		int last = (i + 1) == self->nested2->size;
		DaoxHelpEntry_PrintTree( entry, stream, offsets, offset + width + extra, len, 0, last );
	}
	if( last ){
		memset( line->mbs + offset, ' ', (width + extra + 1)*sizeof(char) );
		DaoStream_WriteString( stream, line );
		DaoStream_WriteChar( stream, '\n' );
	}
	if( last == 0 ) DArray_PopBack( offsets );
	DString_Delete( line );
	if( old == NULL ) DArray_Delete( offsets );
}



static DaoxHelp* DaoxHelp_New()
{
	DaoxHelp *self = (DaoxHelp*) dao_malloc( sizeof(DaoxHelp) );
	self->entries = DMap_New(D_STRING,0);
	self->current = NULL;
	return self;
}
static void DaoxHelp_Delete( DaoxHelp *self )
{
	/* Do not delete entries here, they are managed by DaoxHelper; */
	free( self );
}



static DaoxHelper* DaoxHelper_New()
{
	DString name = DString_WrapMBS( "ALL" );
	DaoxHelper *self = (DaoxHelper*) dao_malloc( sizeof(DaoxHelper) );
	self->helps = DHash_New(0,0);
	self->tree = DaoxHelpEntry_New( & name );
	self->entries = DMap_New(D_STRING,0);
	self->nslist = DArray_New(D_VALUE);
	self->notice = DString_New(1);
	DString_SetMBS( self->notice, "By $(author). Released under the $(license).\n" );
	return self;
}
static void DaoxHelper_Delete( DaoxHelper *self )
{
	DNode *it;
	for(it=DMap_First(self->entries); it; it=DMap_Next(self->entries,it)){
		DaoxHelpEntry_Delete( (DaoxHelpEntry*) it->value.pVoid );
	}
	for(it=DMap_First(self->helps); it; it=DMap_Next(self->helps,it)){
		DaoxHelp_Delete( (DaoxHelp*) it->value.pVoid );
	}
	DMap_Delete( self->helps );
	DArray_Delete( self->nslist );
	DString_Delete( self->notice );
	free( self );
}
static void DaoxHelper_GetGCFields( void *p, DArray *v, DArray *arrays, DArray *m, int rm )
{
	DaoxHelper *self = (DaoxHelper*) p;
	DArray_Append( arrays, self->nslist );
}

static DaoxHelp* DaoxHelper_Get( DaoxHelper *self, DaoNamespace *NS, DString *name )
{
	DaoxHelp *help;
	DNode *node = DMap_Find( self->helps, NS );
	if( node ) return (DaoxHelp*) node->value.pVoid;
	help = DaoxHelp_New();
	help->nspace = NS;
	DMap_Insert( self->helps, NS, help );
	DArray_Append( self->nslist, NS );
	return help;
}
static DaoxHelpEntry* DaoxHelper_GetEntry( DaoxHelper *self, DString *name )
{
	DaoxHelpEntry *entry, *entry2;
	DNode *it = DMap_Find( self->entries, name );
	daoint pos = DString_RFindChar( name, '.', -1 );

	if( it ) return (DaoxHelpEntry*) it->value.pVoid;
	entry = DaoxHelpEntry_New( name );
	if( pos == MAXSIZE ){
		DArray_Append( self->tree->nested2, entry );
		DMap_Insert( self->tree->nested, name, entry );
		entry->parent = self->tree;
	}else{
		DString *name2 = DString_Copy( name );
		DString_Erase( name2, pos, -1 );
		entry2 = DaoxHelper_GetEntry( self, name2 );
		DString_SubString( name, name2, pos + 1, -1 );
		DArray_Append( entry2->nested2, entry );
		DMap_Insert( entry2->nested, name2, entry );
		entry->parent = entry2;
	}
	DMap_Insert( self->entries, name, entry );
	return entry;
}



static void PrintMethod( DaoProcess *proc, DaoRoutine *meth )
{
	int j;
	DaoProcess_Print( proc, meth->routName->mbs );
	DaoProcess_Print( proc, ": " );
	for(j=meth->routName->size; j<20; j++) DaoProcess_Print( proc, " " );
	DaoProcess_Print( proc, meth->routType->name->mbs );
	DaoProcess_Print( proc, "\n" );
}
static void DaoNS_GetAuxMethods( DaoNamespace *ns, DaoValue *p, DArray *methods )
{
	daoint i;
	for(i=0; i<ns->constants->size; i++){
		DaoValue *meth = ns->constants->items.pConst[i]->value;
		if( meth == NULL || meth->type != DAO_ROUTINE ) continue;
		if( meth->type == DAO_ROUTINE && meth->xRoutine.overloads ){
			DRoutines *routs = meth->xRoutine.overloads;
			if( routs->mtree == NULL ) continue;
			for(i=0; i<routs->routines->size; i++){
				DaoRoutine *rout = routs->routines->items.pRoutine[i];
				DaoType *type = rout->routType->nested->items.pType[0];
				if( DaoType_MatchValue( (DaoType*) type->aux, p, NULL ) ==0 ) continue;
				DArray_PushBack( methods, rout );
			}
		}else if( meth->xRoutine.attribs & DAO_ROUT_PARSELF ){
			DaoType *type = meth->xRoutine.routType->nested->items.pType[0];
			if( DaoType_MatchValue( (DaoType*) type->aux, p, NULL ) ==0 ) continue;
			DArray_PushBack( methods, meth );
		}
	}
	for(i=1; i<ns->namespaces->size; i++) DaoNS_GetAuxMethods( ns->namespaces->items.pNS[i], p, methods );
}
static void HELP_List( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns = proc->activeNamespace;
	DaoType *type = DaoNamespace_GetType( ns, p[0] );
	DaoRoutine **meths;
	DArray *array;
	DMap *hash;
	DNode *it;
	int etype = p[1]->xEnum.value;
	int i, j, methCount;

	if( etype == 2 ){
		array = DArray_New(0);
		hash = DHash_New(0,0);
		DaoProcess_Print( proc, "==============================================\n" );
		DaoProcess_Print( proc, "Auxiliar methods for type: " );
		DaoProcess_Print( proc, type->name->mbs );
		DaoProcess_Print( proc, "\n==============================================\n" );
		DaoNS_GetAuxMethods( ns, p[0], array );
		for(i=0; i<array->size; i++){
			DaoRoutine *rout = array->items.pRoutine[i];
			if( DMap_Find( hash, rout ) ) continue;
			DMap_Insert( hash, rout, NULL );
			PrintMethod( proc, rout );
		}
		DArray_Delete( array );
		DMap_Delete( hash );
		return;
	}

	if( type == NULL ) return;
	DaoType_FindValue( type, type->name ); /* To ensure it has been setup; */
	DaoType_FindFunction( type, type->name ); /* To ensure it has been specialized; */
	if( type->kernel == NULL ) return;

	if( etype == 0 ){
		DaoProcess_Print( proc, "==============================================\n" );
		DaoProcess_Print( proc, "Constant values for type: " );
		DaoProcess_Print( proc, type->name->mbs );
		DaoProcess_Print( proc, "\n==============================================\n" );
		hash = type->kernel->values;
		if( type->kernel->values ){
			for(it=DMap_First(hash); it; it=DMap_Next(hash,it)){
				DaoProcess_Print( proc, it->key.pString->mbs );
				DaoProcess_Print( proc, "\n" );
			}
		}
		return;
	}

	DaoProcess_Print( proc, "==============================================\n" );
	DaoProcess_Print( proc, "Methods for type: " );
	DaoProcess_Print( proc, type->name->mbs );
	DaoProcess_Print( proc, "\n==============================================\n" );

	array = DArray_New(0);
	if( type->kernel->methods ) DMap_SortMethods( type->kernel->methods, array );
	meths = array->items.pRoutine;
	methCount = array->size;
	if( type->kernel->methods ){
		for(i=0; i<methCount; i++ ) PrintMethod( proc, (DaoRoutine*)meths[i] );
	}
	DArray_Delete( array );
}
static void HELP_ErrorNoHelp( DaoProcess *proc, DString *entry_name )
{
	DaoStream *stdio = proc->stdioStream;
	if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
	DaoStream_WriteMBS( stdio, "No available help document for \"" );
	DaoStream_WriteString( stdio, entry_name );
	DaoStream_WriteMBS( stdio, "\"!\n" );
}
static void HELP_ErrorNoEntry( DaoProcess *proc, DString *entry_name )
{
	DaoStream *stdio = proc->stdioStream;
	if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
	DaoStream_WriteMBS( stdio, "No available help entry for \"" );
	DaoStream_WriteString( stdio, entry_name );
	DaoStream_WriteMBS( stdio, "\"!\n" );
}
static void HELP_WarnNoExactEntry( DaoProcess *proc, DString *entry_name )
{
	DaoStream *stdio = proc->stdioStream;
	if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
	DaoStream_WriteMBS( stdio, "No available help entry for \"" );
	DaoStream_WriteString( stdio, entry_name );
	DaoStream_WriteMBS( stdio, "\",\n" );
	DaoStream_WriteMBS( stdio, "a more generical entry is used!\n\n" );
}
static DaoxHelp* HELP_GetHelp( DaoProcess *proc, DString *entry_name )
{
	DaoxHelp *help;
	DaoxHelpEntry *entry = NULL;
	DaoNamespace *NS = NULL;
	DaoStream *stdio = proc->stdioStream;
	DString *name = DString_New(1);
	DString *name2 = DString_New(1);
	daoint pos;

	DString_Assign( name, entry_name );
	DString_ToMBS( name );
	while( NS == NULL ){
		DString_SetMBS( name2, "help_" );
		DString_Append( name2, name );
		DString_ChangeMBS( name2, "%W", "_", 0 );
		NS = DaoVmSpace_Load( proc->vmSpace, name2, 0 );
		if( NS ) break;
		pos = DString_RFindChar( name, '.', -1 );
		if( pos < 0 ) break;
		DString_Erase( name, pos, -1 );
	}
	DString_Delete( name );
	DString_Delete( name2 );
	return DaoxHelper_Get( daox_helper, NS, NULL );
}

static DaoxHelpEntry* HELP_GetEntry( DaoProcess *proc, DaoxHelp *help, DString *entry_name )
{
	DaoxHelpEntry *entry = NULL;
	DaoStream *stdio = proc->stdioStream;
	DString *name = DString_New(1);
	daoint pos;
	DString_Assign( name, entry_name );
	DString_ToMBS( name );
	if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
	while( entry == NULL ){
		DNode *it = DMap_Find( help->entries, name );
		if( it ){
			entry = (DaoxHelpEntry*) it->value.pVoid;
			break;
		}
		pos = DString_RFindChar( name, '.', -1 );
		if( pos < 0 ) break;
		DString_Erase( name, pos, -1 );
	}
	DString_Delete( name );
	return entry;
}
static void HELP_Help1( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stdio = proc->stdioStream;
	if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
	DaoProcess_PutValue( proc, daox_cdata_helper );
	DaoxHelpEntry_PrintTree( daox_helper->tree, stdio, NULL, 0, daox_helper->tree->name->size, 1, 1 );
}
static void HELP_Help2( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *entry_name = p[0]->xString.data;
	DaoProcess *newproc = NULL;
	DaoStream *stdio = proc->stdioStream;
	DaoxHelpEntry *entry = NULL;
	DaoxHelp *help = NULL;

	DaoProcess_PutValue( proc, daox_cdata_helper );
	if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
	help = HELP_GetHelp( proc, entry_name );
	if( help ) entry = HELP_GetEntry( proc, help, entry_name );
	if( entry == NULL ){
		DNode *node = DMap_Find( daox_helper->entries, entry_name );
		if( node ) entry = (DaoxHelpEntry*) node->value.pVoid;
	}
	if( entry == NULL ){
		if( help == NULL ){
			HELP_ErrorNoHelp( proc, entry_name );
		}else{
			HELP_ErrorNoEntry( proc, entry_name );
		}
		return;
	}
	if( DString_EQ( entry->name, entry_name ) == 0 ) HELP_WarnNoExactEntry( proc, entry_name );
	if( entry->title ){
		if( p[1]->xInteger.value ) newproc = DaoVmSpace_AcquireProcess( proc->vmSpace );
		DaoxHelpEntry_Print( entry, stdio, newproc );
		if( newproc ) DaoVmSpace_ReleaseProcess( proc->vmSpace, newproc );
	}
	if( entry->nested->size ){
		DaoStream_SetColor( stdio, dao_colors[DAOX_WHITE], dao_colors[DAOX_BLUE] );
		DaoStream_WriteMBS( stdio, "\n[STRUCTURE]\n\n" );
		DaoStream_SetColor( stdio, NULL, NULL );
		DaoxHelpEntry_PrintTree( entry, stdio, NULL, 0, entry->name->size, 1, 1 );
	}
}
static void HELP_Help3( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess *newproc = NULL;
	DaoStream *stdio = proc->stdioStream;
	DaoType *type = DaoNamespace_GetType( proc->activeNamespace, p[0] );
	DaoxHelpEntry *entry = NULL;
	DaoxHelp *help;
	DString *name;

	DaoProcess_PutValue( proc, daox_cdata_helper );
	if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
	if( type == NULL ){
		DaoStream_WriteMBS( stdio, "Cannot determine the type of the parameter object!\n" );
		return;
	}
	name = DString_Copy( type->name );
	DString_ChangeMBS( name, "%b<>", "", 0 );
	DString_ChangeMBS( name, "::", ".", 0 );
	if( N > 1 && p[1]->xString.data->size ){
		DString_AppendMBS( name, "." );
		DString_Append( name, p[1]->xString.data );
	}
	help = HELP_GetHelp( proc, name );
	if( help ) entry = HELP_GetEntry( proc, help, name );
	if( entry == NULL ){
		DNode *node = DMap_Find( daox_helper->entries, name );
		if( node ) entry = (DaoxHelpEntry*) node->value.pVoid;
	}
	if( entry == NULL ){
		if( help == NULL ){
			HELP_ErrorNoHelp( proc, name );
		}else{
			HELP_ErrorNoEntry( proc, name );
		}
		return;
	}
	if( DString_EQ( entry->name, name ) == 0 ) HELP_WarnNoExactEntry( proc, name );
	if( entry->title ){
		if( p[2]->xInteger.value ) newproc = DaoVmSpace_AcquireProcess( proc->vmSpace );
		DaoxHelpEntry_Print( entry, stdio, newproc );
		if( newproc ) DaoVmSpace_ReleaseProcess( proc->vmSpace, newproc );
	}
	if( entry->nested->size )
		DaoxHelpEntry_PrintTree( entry, stdio, NULL, 0, entry->name->size, 1, 1 );
	DString_Delete( name );
}
static void HELP_Search( DaoProcess *proc, DaoValue *p[], int N )
{
}
static void HELP_Search2( DaoProcess *proc, DaoValue *p[], int N )
{
}
static void HELP_Load( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stdio = proc->stdioStream;
	DaoNamespace *NS = DaoVmSpace_Load( proc->vmSpace, p[0]->xString.data, 0 );
	if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
	if( NS == NULL ){
		DaoStream_WriteMBS( stdio, "Failed to load help file \"" );
		DaoStream_WriteString( stdio, p[0]->xString.data );
		DaoStream_WriteMBS( stdio, "\"!\n" );
	}
}
static void HELP_SetTempl( DaoProcess *proc, DaoValue *p[], int N )
{
	DString_Assign( daox_helper->notice, p[0]->xString.data );
}


static DaoFuncItem helpMeths[]=
{
	{ HELP_Help1,     "help()" },
	{ HELP_Help2,     "help( keyword :string, run = 0 )" },
	{ HELP_Help3,     "help( object :any, keyword :string, run = 0 )" },
	{ HELP_Search,    "search( keyword :string )" },
	{ HELP_Search2,   "search( object :any, keyword :string )" },
	{ HELP_Load,      "load( help_file :string )" },
	{ HELP_List,      "list( object :any, type :enum<values,methods,auxmeths>=$methods )" },
	{ HELP_SetTempl,  "set_template( tpl :string, ttype :enum<notice> )" },
	{ NULL, NULL }
};

static DaoTypeBase helpTyper =
{ "help", NULL, NULL, helpMeths, {0}, {0}, (FuncPtrDel) DaoxHelper_Delete, NULL };

static DString* dao_verbatim_content( DString *VT )
{
	DString *content = DString_New(1);
	daoint rb = DString_FindChar( VT, ']', 0 );
	DString_SetDataMBS( content, VT->mbs + rb + 1, VT->size - 2*(rb + 1) );
	DString_Trim( content );
	return content;
}
static int dao_string_delete( DString *string, int retc )
{
	if( string ) DString_Delete( string );
	return retc;
}
static int dao_help_name( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out )
{
	DString *name = dao_verbatim_content( verbatim );
	DaoxHelp *help;
	DNode *it;
	DString_ChangeMBS( name, "%s+", " ", 0 );
	help = DaoxHelper_Get( daox_helper, NS, name );
	if( help == NULL ) return dao_string_delete( name, 1 );
	//printf( "dao_help_name: %s\n", name->mbs );
	it = DMap_Find( help->entries, name );
	if( it == NULL ){
		DaoxHelpEntry *entry = DaoxHelper_GetEntry( daox_helper, name );
		entry->help = help;
		it = DMap_Insert( help->entries, name, entry );
	}
	help->current = (DaoxHelpEntry*) it->value.pVoid;
	return dao_string_delete( name, 0 );
}
static int dao_help_title( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out )
{
	DString *title = dao_verbatim_content( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, NULL );
	//printf( "%s\n", title->mbs );
	if( help == NULL || help->current == NULL ) return dao_string_delete( title, 1 );
	DString_ChangeMBS( title, "%s+", " ", 0 );
	if( help->current->title == NULL ) help->current->title = DString_New(1);
	DString_Assign( help->current->title, title );
	return dao_string_delete( title, 0 );
}
static int dao_help_text( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out )
{
	DString *text = dao_verbatim_content( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, NULL );
	//printf( "%s\n", text->mbs );
	if( help == NULL || help->current == NULL ) return dao_string_delete( text, 1 );
	DaoxHelpEntry_AppendText( help->current, text );
	return dao_string_delete( text, 0 );
}
static int dao_help_code( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out )
{
	DString *code = dao_verbatim_content( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, NULL );
	//printf( "%s\n", code->mbs );
	if( help == NULL || help->current == NULL ) return dao_string_delete( code, 1 );
	DaoxHelpEntry_AppendCode( help->current, code, NULL );
	return dao_string_delete( code, 0 );
}
static int dao_help_author( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out )
{
	DString *code = dao_verbatim_content( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, NULL );
	//printf( "%s\n", code->mbs );
	if( help == NULL || help->current == NULL ) return dao_string_delete( code, 1 );
	if( help->current->author == NULL ) help->current->author = DString_New(1);
	DString_Assign( help->current->author, code );
	return dao_string_delete( code, 0 );
}
static int dao_help_license( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out )
{
	DString *code = dao_verbatim_content( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, NULL );
	//printf( "%s\n", code->mbs );
	if( help == NULL || help->current == NULL ) return dao_string_delete( code, 1 );
	if( help->current->license == NULL ) help->current->license = DString_New(1);
	DString_Assign( help->current->license, code );
	DString_Trim( help->current->license );
	return dao_string_delete( code, 0 );
}

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DString help_help = DString_WrapMBS( "help_help" );
	DaoType *type;

	dao_vmspace = vmSpace;
	DaoNamespace_AddCodeInliner( ns, "name", dao_help_name );
	DaoNamespace_AddCodeInliner( ns, "title", dao_help_title );
	DaoNamespace_AddCodeInliner( ns, "text", dao_help_text );
	DaoNamespace_AddCodeInliner( ns, "code", dao_help_code );
	DaoNamespace_AddCodeInliner( ns, "author", dao_help_author );
	DaoNamespace_AddCodeInliner( ns, "license", dao_help_license );
	type = DaoNamespace_WrapType( ns, & helpTyper, 1 );
	daox_helper = DaoxHelper_New();
	daox_cdata_helper = (DaoValue*) DaoCdata_New( type, daox_helper );
	DaoNamespace_AddConstValue( ns, "__helper__", daox_cdata_helper );
	DaoVmSpace_Load( vmSpace, & help_help, 0 );
	return 0;
}

