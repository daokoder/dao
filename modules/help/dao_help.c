/*
// Dao Standard Modules
// http://www.daovm.net
//
// Copyright (c) 2012,2013, Limin Fu
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
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<math.h>
#include<assert.h>
#include<locale.h>

#include"daoString.h"
#include"daoValue.h"
#include"daoRegex.h"
#include"daoParser.h"
#include"daoNamespace.h"
#include"daoVmspace.h"
#include"daoGC.h"

#ifdef UNIX
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#define DAOX_TEXT_WIDTH 79
#define DAOX_TREE_WIDTH 99

enum DaoxHelpBlockType
{
	DAOX_HELP_NONE ,
	DAOX_HELP_NODE ,
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

/*
// "dao_colors" must be declared as static, because the name "dao_colors"
// is used in daoStream.c to declare a color array with colors in a different
// order. In Linux, these two arrays will be linked as one, and the one from
// daoStream.c is used, so color ids from DaoxCodeColor will not identify
// the correct color!
*/
static char* dao_colors[8]
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
	DaoNamespace   *nspace;
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

	int  failedTests;
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
	DMap           *cxxKeywords;
};
struct DaoxStream
{
	DaoNamespace  *nspace;
	DaoProcess    *process;
	DaoRegex      *regex;
	DaoStream     *stream;
	DMap          *mtypes;
	DString       *output;
	unsigned       fmtHTML;
	unsigned       section;  /* section index; */
	unsigned       subsect;  /* subsection index; */
	unsigned       subsect2; /* subsubsection index; */
	unsigned       offset;   /* offset in the current line; */
	wchar_t        last;     /* last char in the current line; */
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
static int DString_Break( DString *self, int start, int width )
{
	wchar_t *wcs = self->wcs;
	daoint i, k = 0;
	assert( self->wcs != NULL );
	for(i=start; i<self->size; ++i){
		k += 1 + (wcs[i] >= 256);
		if( k >= width ) break;
	}
	if( i >= self->size ) return i;
	if( i == 0 || wcs[i-1] >= 256 || wcs[i] >= 256 ) return i;
	if( iswspace( wcs[i-1] ) || iswspace( wcs[i] ) ) return i;
	while( i > start && wcs[i-1] < 256 && iswspace( wcs[i-1] ) == 0 ) i -= 1;
	return i;
}

static DaoxStream* DaoxStream_New( DaoStream *stream, DaoProcess *proc )
{
	const char *pat = "@ %[ %s* (| : | %w+%s*:) %s* %w+ %s* ( %( %s* (|%w+) %s* %) | ) [^%]]* %]";
	DString spat = DString_WrapMBS( pat );
	DString snode = DString_WrapMBS( "node" );
	DString scode = DString_WrapMBS( "code" );
	DString slist = DString_WrapMBS( "list" );
	DString comment = DString_WrapMBS( "comment" );
	DString ssection = DString_WrapMBS( "section" );
	DString ssubsect = DString_WrapMBS( "subsection" );
	DString ssubsect2 = DString_WrapMBS( "subsubsection" );
	DaoxStream *self = (DaoxStream*) calloc( 1, sizeof(DaoxStream) );
	int screen = DAOX_TEXT_WIDTH;

#if defined(UNIX) && !defined(MINIX)
	struct winsize ws;
	ioctl( STDOUT_FILENO, TIOCGWINSZ, &ws );
	if( ws.ws_col <= DAOX_TEXT_WIDTH ) screen = ws.ws_col - 1;
#endif

	self->mtypes = DHash_New(D_STRING,0);
	self->regex = DaoRegex_New( & spat );
	self->process = proc;
	self->stream = stream;
	self->output = NULL;
	DMap_Insert( self->mtypes, & snode, (void*)(size_t)DAOX_HELP_NODE );
	DMap_Insert( self->mtypes, & scode, (void*)(size_t)DAOX_HELP_CODE );
	DMap_Insert( self->mtypes, & slist, (void*)(size_t)DAOX_HELP_LIST );
	DMap_Insert( self->mtypes, & comment, (void*)(size_t)DAOX_HELP_COMMENT );
	DMap_Insert( self->mtypes, & ssection, (void*)(size_t)DAOX_HELP_SECTION );
	DMap_Insert( self->mtypes, & ssubsect, (void*)(size_t)DAOX_HELP_SUBSECT );
	DMap_Insert( self->mtypes, & ssubsect2, (void*)(size_t)DAOX_HELP_SUBSECT2 );
	return self;
}
static void DaoxStream_Delete( DaoxStream *self )
{
	DaoRegex_Delete( self->regex );
	DMap_Delete( self->mtypes );
	free( self );
}
static void DaoxStream_SetColor( DaoxStream *self, const char *fg, const char *bg )
{
	DString fg2 = DString_WrapMBS( fg );
	DString bg2 = DString_WrapMBS( bg );
	DaoStream_SetColor( self->stream, fg, bg );
	if( fg == NULL && bg == NULL ){
		if( self->fmtHTML ){
			DString_AppendMBS( self->output, "</span>" );
		}
	}else{
		if( self->fmtHTML ){
			DString_AppendMBS( self->output, "<span style=\"" );
			if( fg ){
				DString_AppendMBS( self->output, "color:" );
				DString_AppendMBS( self->output, fg );
			}
			if( bg ){
				DString_AppendMBS( self->output, ";background-color:" );
				DString_AppendMBS( self->output, bg );
			}
			DString_AppendMBS( self->output, "\">" );
		}
	}
}
static void DaoxStream_WriteNewLine( DaoxStream *self, const char *text )
{
	if( self->output ){
		DString_AppendMBS( self->output, text );
		DString_AppendChar( self->output, '\n' );
	}else{
		DaoStream_WriteMBS( self->stream, text );
		DaoStream_WriteChar( self->stream, '\n' );
	}
	self->offset = 0;
	self->last = 0;
}
static void DaoxStream_WriteChar( DaoxStream *self, char ch )
{
	if( self->offset && isspace( self->last ) == 0 && isalnum( ch ) )
		DaoxStream_WriteChar( self, ' ' );

	if( self->output ){
		DString_AppendChar( self->output, ch );
	}else{
		DaoStream_WriteChar( self->stream, ch );
	}
	self->offset += 1;
	self->last = ch;
}
static void DaoxStream_WriteMBS( DaoxStream *self, const char *mbs )
{
	int len = strlen( mbs );
	if( self->offset && isalnum( self->last ) && isalnum( mbs[0] ) )
		DaoxStream_WriteChar( self, ' ' );
	if( self->output ){
		DString_AppendMBS( self->output, mbs );
	}else{
		DaoStream_WriteMBS( self->stream, mbs );
	}
	self->offset += len;
	self->last = mbs[len-1];
}
static void DaoxStream_WriteString( DaoxStream *self, DString *text )
{
	if( self->offset && isalnum( self->last ) ){
		if( text->mbs && isalnum( text->mbs[0] ) )
			DaoxStream_WriteChar( self, ' ' );
		else if( text->wcs && iswalnum( text->wcs[0] ) )
			DaoxStream_WriteChar( self, ' ' );
	}
	if( self->output ){
		DString_Append( self->output, text );
	}else{
		DaoStream_WriteString( self->stream, text );
	}
	if( text->mbs ){
		self->offset += text->size;
		if( text->size ) self->last = text->mbs[text->size-1];
	}else{
		daoint i;
		for(i=0; i<text->size; ++i) self->offset += 1 + (text->wcs[i] >= 256);
		if( text->size ) self->last = text->wcs[text->size-1];
	}
}
static void DaoxStream_WriteInteger( DaoxStream *self, int i )
{
	char buf[20];
	sprintf( buf, "%i", i );
	DaoxStream_WriteMBS( self, buf );
}
static int DaoxStream_WriteItemID( DaoxStream *self, char type, int id )
{
	char buf[20];
	if( type == '-' ){
		DaoxStream_WriteMBS( self, "  *  " );
	}else{
		sprintf( buf, "% 3i. ", id );
		DaoxStream_WriteMBS( self, buf );
	}
	return 5;
}
static void DaoxStream_WriteParagraph( DaoxStream *self, DString *text, int offset, int width )
{
	DString *line = DString_New(0);
	daoint start, next;
	int i;
	for(start=next=0; start<text->size; start=next){
		while( self->offset < offset ) DaoxStream_WriteChar( self, ' ' );
		start = DString_Break( text, start, 0 );
		next = DString_Break( text, start, width - self->offset );
		if( (next - start) <= 0 ){
			DaoxStream_WriteNewLine( self, "" );
			while( self->offset < offset ) DaoxStream_WriteChar( self, ' ' );
			next = DString_Break( text, start, width - self->offset );
		}
		DString_SubString( text, line, start, next - start );
		if( self->offset == offset ) DString_Trim( line );
		//DString_Chop( line );
		DaoxStream_WriteString( self, line );
	}
	//if( text->size && text->mbs[text->size-1] == '\n' ) DaoxStream_WriteNewLine( self, "" );
	DString_Delete( line );
}
static void DaoxStream_WriteText( DaoxStream *self, DString *text, int offset, int width )
{
	DString *paragraph = DString_New(0);
	daoint pos, last = 0;
	text = DString_Copy( text );
	DString_ToWCS( text );
	DString_ChangeWCS( text, L"[\n][ \t]+[\n]", L"\n\n", 0 );
	DString_ChangeWCS( text, L"[\n][ \t]+[\n]", L"\n\n", 0 );
	DString_ChangeWCS( text, L"(^|[^\n])[\n]([^\n]|$)", L"%1 %2", 0 );
	DString_ChangeWCS( text, L"[ \t]+", L" ", 0 );
	pos = DString_FindWChar( text, L'\n', 0 );
	while( last < text->size ){
		if( pos == MAXSIZE ) pos = text->size;
		DString_SubString( text, paragraph, last, pos - last );
		DaoxStream_WriteParagraph( self, paragraph, offset, width );
		last = pos + 1;
		while( last < text->size && text->wcs[last] == L'\n' ) last += 1;
		if( last > (pos + 1) ) DaoxStream_WriteNewLine( self, "" );
		if( last > (pos + 2) ) DaoxStream_WriteNewLine( self, "" );
		pos = DString_FindWChar( text, L'\n', last );
	}
	DString_Delete( paragraph );
	DString_Delete( text );
}
static void DaoxStream_PrintLineNumber( DaoxStream *self, int line, int offset )
{
	int i, color;
	char sline[20];
	sprintf( sline, "%4i ", line );
	for(i=0; i<offset; i++) DaoxStream_WriteChar( self, ' ' );
	if( self->fmtHTML ){
		DaoxStream_SetColor( self, "white", "black" );
	}else{
		DaoxStream_SetColor( self, "black", "yellow" );
	}
	DaoxStream_WriteMBS( self, sline );
	DaoxStream_SetColor( self, NULL, NULL );
	DaoxStream_WriteChar( self, ' ' );
}
static void DaoxStream_WriteEntryName( DaoxStream *self, DString *name )
{
	if( self->offset && isalnum( self->last ) ) DaoxStream_WriteChar( self, ' ' );
	if( self->output ){
		if( self->fmtHTML ){
			DString_AppendMBS( self->output, "<a href=\"" );
			DString_AppendMBS( self->output, name->mbs );
			DString_AppendMBS( self->output, ".html\">" );
			DString_AppendMBS( self->output, name->mbs );
			DString_AppendMBS( self->output, "</a>" );
		}else{
			DString_AppendMBS( self->output, name->mbs );
		}
		self->offset += name->size;
		self->last = name->mbs[name->size-1];
	}else{
		DaoxStream_SetColor( self, "blue", NULL );
		DaoxStream_WriteMBS( self, name->mbs );
		DaoxStream_SetColor( self, NULL, NULL );
	}
}
static void DaoxStream_WriteEntryName2( DaoxStream *self, DString *name )
{
	DString *mbs;
	daoint start = 0;
	if( self->output == NULL || self->fmtHTML == 0 ){
		DaoxStream_WriteEntryName( self, name );
		return;
	}
	DString_AppendMBS( self->output, "<a href=\"index.html\">ALL</a>" );
	DString_AppendMBS( self->output, "." );
	mbs = DString_New(1);
	while( start < name->size ){
		daoint end = DString_FindChar( name, '.', start );
		if( end == -1 ) end = name->size;
		if( start ) DString_AppendChar( self->output, '.' );
		DString_SubString( name, mbs, 0, end );
		DString_AppendMBS( self->output, "<a href=\"" );
		DString_AppendMBS( self->output, mbs->mbs );
		DString_AppendMBS( self->output, ".html\">" );
		DString_SubString( name, mbs, start, end - start );
		DString_AppendMBS( self->output, mbs->mbs );
		DString_AppendMBS( self->output, "</a>" );
		start = end + 1;
	}
	self->offset += 4 + name->size;
	self->last = name->mbs[name->size-1];
}

typedef struct DaoxLexInfo DaoxLexInfo;
struct DaoxLexInfo
{
	uchar_t      daoLineComment;
	uchar_t      daoBlockComment;
	uchar_t      singleQuotation;
	uchar_t      doubleQuotation;

	const char  *lineComment1;
	const char  *openComment1;
	const char  *closeComment1;

	const char  *lineComment2;
	const char  *openComment2;
	const char  *closeComment2;
};

static DaoxLexInfo daox_help_cxx_lexinfo =
{
	0, 0, 1, 1,
	"//", "/*", "*/",
	NULL, NULL, NULL
};

static DIntStringPair daox_help_cxx_keywords[] =
{
	{ DKEY_CLASS,  "namespace" },
	{ DKEY_CLASS,  "class" },
	{ DKEY_CLASS,  "struct" },
	{ DKEY_CLASS,  "void" },
	{ DKEY_CLASS,  "extern" },
	{ DKEY_CLASS,  "static" },
	{ DKEY_CLASS,  "const" },
	{ DKEY_CLASS,  "unsigned" },
	{ DKEY_CLASS,  "char" },
	{ DKEY_CLASS,  "short" },
	{ DKEY_CLASS,  "int" },
	{ DKEY_CLASS,  "long" },
	{ DKEY_CLASS,  "float" },
	{ DKEY_CLASS,  "double" },
	{ DKEY_CLASS,  "daoint" },
	{ DKEY_CLASS,  "uchar_t" },
	{ DKEY_CLASS,  "wchar_t" },
	{ DKEY_CLASS,  "enum" },

	{ DKEY_NONE,   "NULL" },
	{ DKEY_NONE,   "false" },
	{ DKEY_NONE,   "true" },
	{ DKEY_NONE,   "this" },
	{ DKEY_NONE,   "self" },

	{ DKEY_VIRTUAL,    "virtual" },
	{ DKEY_PUBLIC,     "public" },
	{ DKEY_PROTECTED,  "protected" },
	{ DKEY_PRIVATE,    "private" },

	{ DKEY_FOR,    "include" },
	{ DKEY_FOR,    "define" },
	{ DKEY_FOR,    "undef" },
	{ DKEY_FOR,    "elif" },
	{ DKEY_FOR,    "endif" },

	{ DKEY_FOR,    "for" },
	{ DKEY_WHILE,  "while" },
	{ DKEY_IF,     "if" },
	{ DKEY_ELSE,   "else" },
	{ DKEY_BREAK,  "break" },
	{ DKEY_SKIP,   "continue" },
	{ DKEY_SWITCH, "switch" },
	{ DKEY_CASE,   "case" },
	{ DKEY_RETURN, "goto" },
	{ DKEY_RETURN, "return" },
	{ DKEY_RETURN, "new" },
	{ DKEY_RETURN, "delete" },

	{ DKEY_RETURN, "dynamic_cast" },
	{ DKEY_RETURN, "static_cast" },
	{ DKEY_RETURN, "const_cast" },
	{ DKEY_RETURN, "reinterpret_cast" },
	{ 0, "" }
};


int DaoxHelp_TokenizeCodes( DaoxLexInfo *info, DaoLexer *lexer, const char *source )
{
	DaoToken *one = DaoToken_New();
	DString ds = DString_WrapMBS( source );
	const char *src = source;
	int NLL1 = info->lineComment1 ? strlen( info->lineComment1 ) : 0;/* NL1, macro on Haiku */
	int NLL2 = info->lineComment2 ? strlen( info->lineComment2 ) : 0;
	int NBO1 = info->openComment1 ? strlen( info->openComment1 ) : 0;
	int NBO2 = info->openComment2 ? strlen( info->openComment2 ) : 0;
	int NBC1 = info->closeComment1 ? strlen( info->closeComment1 ) : 0;
	int NBC2 = info->closeComment2 ? strlen( info->closeComment2 ) : 0;
	int i, j, k = 0, n = ds.size;
	int cmtype = 0;

	DaoLexer_Reset( lexer );

	while( n ){
		//printf( "%5i:  %s\n\n\n\n", n, src );
		//if( n < -100 ) break;
		if( n <0 ){
			printf( "%5i:  %s\n\n\n\n", n, src );
			break;
		}
		ds.mbs = (char*)src;
		ds.size = n;
		one->type = one->name = DTOK_COMMENT;
		DString_Reset( & one->string, 0 );
		cmtype = 0;
		if( NLL1 && strncmp( src, info->lineComment1, NLL1 ) ==0 ) cmtype = 1;
		if( NLL2 && strncmp( src, info->lineComment2, NLL2 ) ==0 ) cmtype = 2;
		if( NBO1 && strncmp( src, info->openComment1, NBO1 ) ==0 ) cmtype = 3;
		if( NBO2 && strncmp( src, info->openComment2, NBO2 ) ==0 ) cmtype = 4;
		if( cmtype ==1 || cmtype ==2 ){
			while( n ){
				DString_AppendChar( & one->string, *src );
				src ++;
				n --;
				if( *src == '\n' ) break;
			}
		}else if( cmtype ==3 ){
			k = DString_FindMBS( & ds, info->closeComment1, NBO1 );
			if( k == MAXSIZE ){
				DString_Assign( & one->string, & ds );
				one->type = one->name = DTOK_CMT_OPEN;
			}else{
				k += NBC1;
				DString_SetDataMBS( & one->string, ds.mbs, k );
				src += k;
				n -= k;
			}
		}else if( cmtype ==4 ){
			k = DString_FindMBS( & ds, info->closeComment2, NBO2 );
			if( k == MAXSIZE ){
				DString_Assign( & one->string, & ds );
				one->type = one->name = DTOK_CMT_OPEN;
			}else{
				k += NBC2;
				DString_SetDataMBS( & one->string, ds.mbs, k );
				src += k;
				n -= k;
			}
		}
		if( cmtype ){
			DaoLexer_AppendToken( lexer, one );
			if( one->type == DTOK_CMT_OPEN ) return cmtype - 2;
			continue;
		}
		if( *src == '#' ){
			one->type = DTOK_NONE2;
			DString_AppendChar( & one->string, '#' );
			if( n >1 && src[1] == '{' && ! info->daoBlockComment ){
				src += 2;
				n -= 2;
				DaoLexer_AppendToken( lexer, one );
				one->type = DTOK_LCB;
				DString_SetMBS( & one->string, "{" );
				DaoLexer_AppendToken( lexer, one );
				continue;
			}else if( ! info->daoLineComment ){
				src ++;
				n --;
				DaoLexer_AppendToken( lexer, one );
				continue;
			}
			DString_Reset( & one->string, 0 );
		}
		if( *src == '\'' && ! info->singleQuotation ){
			one->type = DTOK_MBS_OPEN;
			DString_AppendChar( & one->string, *src );
			DaoLexer_AppendToken( lexer, one );
			src += 1;
			n -= 1;
			continue;
		}else if( *src == '\"' && ! info->doubleQuotation ){
			one->type = DTOK_WCS_OPEN;
			DString_AppendChar( & one->string, *src );
			DaoLexer_AppendToken( lexer, one );
			src += 1;
			n -= 1;
			continue;
		}
		one->type = one->name = DaoToken_Check( src, n, & k );
		DString_SetDataMBS( & one->string, src, k );
		DaoLexer_AppendToken( lexer, one );
		//printf( "n = %3i,  %3i,  %3i ======= %s\n", n, k, one->type, mbs->mbs );
		if( n < k ){
			printf( "n = %i,  %i,  %i=======%s\n", n, k, one->type, src );
		}
		src += k;
		n -= k;
		if( k == 0 ) break;
	}
	DaoToken_Delete( one );
	return 0;
}
static void DaoxStream_PrintCode( DaoxStream *self, DString *code, DString *lang, int offset, int width )
{
	DString *string = DString_New(1);
	DaoLexer *lexer = DaoLexer_New();
	DArray *tokens = lexer->tokens;
	const char *bgcolor = NULL; /* "yellow"; */
	const char *defaultColor = NULL;
	daoint start = 0, end = code->size-1;
	daoint i, j, pos, last, color, fgcolor;
	int line = 1, printedline = 0;
	int println = 1;

	if( self->offset && DString_FindChar( code, '\n', 0 ) != MAXSIZE ){
		DaoxStream_WriteNewLine( self, "" );
	}

	println = DString_MatchMBS( code, "^ %s* %n", & start, & end );
	if( println == 0 ) defaultColor = "blue";
	if( println ){
		bgcolor = NULL;
		for(i=0; i<offset; i++) DaoxStream_WriteChar( self, ' ' );
		if( self->fmtHTML ){
			DaoxStream_SetColor( self, "white", "black" );
		}else{
			DaoxStream_SetColor( self, "black", "yellow" );
		}
		DaoxStream_WriteMBS( self, "     " );
		DaoxStream_SetColor( self, NULL, NULL );
		DaoxStream_WriteNewLine( self, "" );
	}else if( isspace( self->last ) == 0 ){
		DaoxStream_WriteChar( self, ' ' );
	}
	DString_Trim( code );

	if( lang && strcmp( lang->mbs, "cxx" ) == 0 ){
		DaoxHelp_TokenizeCodes( & daox_help_cxx_lexinfo, lexer, code->mbs );
		for(i=0; i<tokens->size; i++){
			DaoToken *tok = tokens->items.pToken[i];
			DNode *it = DMap_Find( daox_helper->cxxKeywords, & tok->string );
			if( it ) tok->name = it->value.pInt;
		}
	}else{
		DaoLexer_Tokenize( lexer, code->mbs, DAO_LEX_COMMENT|DAO_LEX_SPACE );
		if( lang && strcmp( lang->mbs, "dao" ) == 0 ){
			for(i=0; i<tokens->size; i++){
				DaoToken *tok = tokens->items.pToken[i];
				const char *ts = tok->string.mbs;
				if( tok->type != DTOK_IDENTIFIER ) continue;
				if( strcmp( ts, "panic" ) == 0 )   tok->name = DKEY_RAND;
				if( strcmp( ts, "recover" ) == 0 ) tok->name = DKEY_RAND;
			}
		}else if( lang && strcmp( lang->mbs, "syntax" ) == 0 ){
			for(i=0; i<tokens->size; i++){
				DaoToken *tok = tokens->items.pToken[i];
				switch( tok->type ){
				case DTOK_LB : case DTOK_LCB : case DTOK_LSB :
				case DTOK_RB : case DTOK_RCB : case DTOK_RSB :
					tok->name = DKEY_SYNTAX;
					break;
				case DTOK_ADD : case DTOK_MUL : case DTOK_PIPE :
					tok->name = DKEY_WHILE;
					break;
				case DTOK_COLON2 : case DTOK_ASSN :
					tok->name = DKEY_AS;
					break;
				}
			}
		}else if( lang && strcmp( lang->mbs, "test" ) == 0 ){
			for(i=0; i<tokens->size; i++){
				DaoToken *tok = tokens->items.pToken[i];
				const char *ts = tok->string.mbs;
				if( tok->type != DTOK_IDENTIFIER ) continue;
				if( strcmp( ts, "__result__" ) == 0 ) tok->name = DTOK_COMMENT;
				if( strcmp( ts, "__answer__" ) == 0 ) tok->name = DTOK_COMMENT;
				if( strcmp( ts, "__stdout__" ) == 0 ) tok->name = DTOK_COMMENT;
				if( strcmp( ts, "__stderr__" ) == 0 ) tok->name = DTOK_COMMENT;
			}
		}
	}
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
		case DTOK_TAB :
			fgcolor = -100;
			DaoxStream_WriteMBS( self, "    " );
			break;
		case DTOK_DIGITS_HEX : case DTOK_DIGITS_DEC :
		case DTOK_NUMBER_HEX : case DTOK_NUMBER_DEC :
		case DTOK_DOUBLE_DEC : case DTOK_NUMBER_IMG :
		case DTOK_NUMBER_SCI : case DTOK_DOLLAR :
		case DKEY_NONE :
			fgcolor = DAOX_RED;
			break;
		case DTOK_VERBATIM :
		case DTOK_MBS : case DTOK_WCS :
		case DTOK_MBS_OPEN : case DTOK_WCS_OPEN :
		case DTOK_CMT_OPEN : case DTOK_COMMENT :
			fgcolor = DAOX_RED;
			if( tok->name == DTOK_CMT_OPEN || tok->name == DTOK_COMMENT ) fgcolor = DAOX_BLUE;
			last = 0;
			pos = DString_FindChar( & tok->string, '\n', 0 );
			while( last < tok->string.size ){
				if( last ){
					printedline = line;
					if( println ) DaoxStream_PrintLineNumber( self, line, offset );
				}
				line += pos != MAXSIZE;
				if( pos == MAXSIZE ) pos = tok->string.size - 1;
				DString_SubString( & tok->string, string, last, pos - last + 1 );
				if( self->output && self->fmtHTML ) DString_ChangeMBS( string, "%<", "&lt;", 0 );
				DaoxStream_SetColor( self, dao_colors[fgcolor], bgcolor );
				DaoxStream_WriteString( self, string );
				DaoxStream_SetColor( self, NULL, NULL );
				last = pos + 1;
				pos = DString_FindChar( & tok->string, '\n', pos + 1 );
			}
			fgcolor = -100;
			break;
		case DTOK_LT :
			if( self->output && self->fmtHTML ){
				DaoxStream_SetColor( self, defaultColor, bgcolor );
				DString_AppendMBS( self->output, "&lt;" );
				self->offset += 1;
				self->last = '<';
				DaoxStream_SetColor( self, NULL, NULL );
				fgcolor = -100;
			}
			break;
		case DTOK_AT2 :
		case DKEY_USE : case DKEY_LOAD :
		case DKEY_AS : 
		case DKEY_AND : case DKEY_OR : case DKEY_NOT :
		case DKEY_VIRTUAL :
			if( self->fmtHTML ){
				fgcolor = DAOX_GREEN;
			}else{
				fgcolor = DAOX_CYAN;
			}
			break;
		case DKEY_VAR : case DKEY_CONST : case DKEY_STATIC : case DKEY_GLOBAL :
		case DKEY_PRIVATE : case DKEY_PROTECTED : case DKEY_PUBLIC :
			fgcolor = DAOX_GREEN;
			break;
		case DKEY_IF : case DKEY_ELSE :
		case DKEY_WHILE : case DKEY_DO :
		case DKEY_FOR : case DKEY_IN :
		case DKEY_SKIP : case DKEY_BREAK : case DKEY_CONTINUE :
		case DKEY_RETURN : case DKEY_YIELD :
		case DKEY_SWITCH : case DKEY_CASE : case DKEY_DEFAULT :
		case DKEY_DEFER :
			fgcolor = DAOX_MAGENTA;
			break;
		case DKEY_TYPE :
		case DKEY_ANY : case DKEY_ENUM :
		case DKEY_INT : case DKEY_LONG :
		case DKEY_FLOAT : case DKEY_DOUBLE :
		case DKEY_STRING : case DKEY_COMPLEX :
		case DKEY_LIST : case DKEY_MAP : case DKEY_TUPLE : case DKEY_ARRAY :
		case DKEY_CLASS : case DKEY_INTERFACE :
		case DKEY_FUNCTION : case DKEY_ROUTINE : case DKEY_SUB :
		case DKEY_SYNTAX : 
		case DKEY_OPERATOR :
		case DKEY_SELF :
		case DTOK_ID_THTYPE :
		case DTOK_ID_SYMBOL :
			fgcolor = DAOX_GREEN;
			break;
		case DKEY_RAND : case DKEY_CEIL : case DKEY_FLOOR :
		case DKEY_ABS  : case DKEY_ARG  : case DKEY_IMAG :
		case DKEY_NORM : case DKEY_REAL : case DKEY_ACOS :
		case DKEY_ASIN : case DKEY_ATAN : case DKEY_COS :
		case DKEY_COSH : case DKEY_EXP : case DKEY_LOG :
		case DKEY_SIN : case DKEY_SINH : case DKEY_SQRT :
		case DKEY_TAN : case DKEY_TANH :
			if( self->fmtHTML ){
				fgcolor = DAOX_MAGENTA;
			}else{
				fgcolor = DAOX_YELLOW;
			}
			break;
		default: break;
		}
		if( fgcolor < -10 ) continue;
		if( println == 0 && (self->offset + tok->string.size) > width ){
			DaoxStream_WriteNewLine( self, "" );
			for(j=0; j<offset; j++) DaoxStream_WriteChar( self, ' ' );
		}
		if( fgcolor < 0 ){
			DaoxStream_SetColor( self, defaultColor, bgcolor );
			self->last = '\0'; /* no space between two string output; */
			DaoxStream_WriteString( self, & tok->string );
			DaoxStream_SetColor( self, NULL, NULL );
			continue;
		}
		DaoxStream_SetColor( self, dao_colors[fgcolor], bgcolor );
		self->last = '\0';
		DaoxStream_WriteString( self, & tok->string );
		DaoxStream_SetColor( self, NULL, NULL );
	}
	if( println ){
		DaoxStream_WriteNewLine( self, "" );
		for(i=0; i<offset; i++) DaoxStream_WriteChar( self, ' ' );
		if( self->fmtHTML ){
			DaoxStream_SetColor( self, "white", "black" );
		}else{
			DaoxStream_SetColor( self, "black", "yellow" );
		}
		DaoxStream_WriteMBS( self, "     " );
		DaoxStream_SetColor( self, NULL, NULL );
		DaoxStream_WriteNewLine( self, "" );
	}
	DaoLexer_Delete( lexer );
	DString_Delete( string );
}

static int DaoxStream_WriteBlock( DaoxStream *self, DString *text, int offset, int width, int islist, int listdep );

static int DaoxStream_WriteList( DaoxStream *self, DString *text, int offset, int width, int islist, int listdep )
{
	const char *pat = "^[\n]+[ \t]* (%-%-|==)";
	daoint pos, lb, last = 0, start = 0, end = text->size-1;
	int i, itemid = 1;
	int fails = 0;

	if( DString_MatchMBS( text, pat, & start, & end ) ){
		DString *delim = DString_New(1);
		DString *item = DString_New(1);
		DString_SubString( text, delim, start, end - start + 1 );
		DString_ChangeMBS( delim, "^[\n][\n]+", "\n", 1 );
		while( last < text->size ){
			DString_SubString( text, item, last, start - last );
			DString_ChangeMBS( item, "^%s+", "", 0 );
			if( item->size ){
				DString_Erase( item, 0, 2 );
				for(i=0; i<offset; i++) DaoxStream_WriteChar( self, ' ' );
				i = DaoxStream_WriteItemID( self, delim->mbs[delim->size-1], itemid ++ );
				fails += DaoxStream_WriteBlock( self, item, offset+i, width, 0, listdep );
			}
			/* print newline for items except the last one: */
			if( start < text->size ) DaoxStream_WriteNewLine( self, "" );
			last = start;
			start = DString_Find( text, delim, last + delim->size );
			if( start == MAXSIZE ) start = text->size;
		}
		DString_Delete( delim );
		DString_Delete( item );
	}else{
		fails += DaoxStream_WriteBlock( self, text, offset, width, 0, listdep );
	}
	DaoxStream_WriteNewLine( self, "" );
	return fails;
}
typedef struct DaoxTestStream DaoxTestStream;
struct DaoxTestStream
{
	DaoStream *stream;
	void (*StdioRead)( DaoxTestStream *self, DString *input, int count );
	void (*StdioWrite)( DaoxTestStream *self, DString *output );
	void (*StdioFlush)( DaoxTestStream *self );
	void (*SetColor)( DaoxTestStream *self, const char *fg, const char *bg );

	DString  *output;
	DString  *output2;
};
void DaoxTestStream_StdioWrite( DaoxTestStream *self, DString *output )
{
	DString_Append( self->output, output );
	DString_Append( self->output2, output );
}
static int DaoxStream_DoTest( DaoxStream *self, DString *code )
{
	int failed = 0;
	DaoValue *result, *answer, *output, *error;
	DaoVmSpace *vmspace = self->process->vmSpace;
	DaoNamespace *nspace = DaoNamespace_New( vmspace, "test" );
	DaoxTestStream stdoutStream = { NULL, NULL, NULL, NULL, NULL };
	DaoxTestStream stderrStream = { NULL, NULL, NULL, NULL, NULL };
	DaoUserStream *prevStdout, *prevStderr;
	DString *output2 = DString_New(1);

	stdoutStream.StdioWrite = DaoxTestStream_StdioWrite;
	stderrStream.StdioWrite = DaoxTestStream_StdioWrite;

	stdoutStream.output = DString_New(1);
	stderrStream.output = DString_New(1);
	stdoutStream.output2 = stderrStream.output2 = output2;

	prevStdout = DaoVmSpace_SetUserStdio( vmspace, (DaoUserStream*) & stdoutStream );
	prevStderr = DaoVmSpace_SetUserStdError( vmspace, (DaoUserStream*) & stderrStream  );

	DaoProcess_Eval( self->process, nspace, code->mbs );

	DaoVmSpace_SetUserStdio( vmspace, prevStdout );
	DaoVmSpace_SetUserStdError( vmspace, prevStderr  );

	DaoxStream_WriteNewLine( self, "Test output:" );
	DaoxStream_SetColor( self, NULL, dao_colors[DAOX_YELLOW] );
	DaoxStream_WriteString( self, output2 );
	DaoxStream_SetColor( self, NULL, NULL );

	result = DaoNamespace_FindData( nspace, "__result__" );
	answer = DaoNamespace_FindData( nspace, "__answer__" );
	output = DaoNamespace_FindData( nspace, "__stdout__" );
	error  = DaoNamespace_FindData( nspace, "__stderr__" );

	if( result && answer )
		failed |= DaoValue_Compare( result, answer ) != 0;

	if( output && output->type == DAO_STRING ){
		DString *s = DaoValue_TryGetString( output );
		failed |= DString_EQ( stdoutStream.output, s ) == 0;
	}

	if( error && error->type == DAO_STRING && DaoValue_TryGetString( error )->size ){
		DString *s = DaoValue_TryGetString( error );
		failed |= DString_Find( stderrStream.output, s, 0 ) == MAXSIZE;
	}else if( stderrStream.output->size ){
		failed |= 1;
	}
	DaoxStream_WriteNewLine( self, "Test status:" );
	if( failed ){
		DaoxStream_SetColor( self, "white", "red" );
		DaoxStream_WriteMBS( self, "FAILED!" );
	}else{
		DaoxStream_SetColor( self, "white", "green" );
		DaoxStream_WriteMBS( self, "PASSED!" );
	}
	DaoxStream_SetColor( self, NULL, NULL );
	DaoxStream_WriteNewLine( self, "" );

	DaoGC_TryDelete( (DaoValue*) nspace );
	DString_Delete( stdoutStream.output );
	DString_Delete( stderrStream.output );
	DString_Delete( output2 );

	return failed;
}
static int DaoxStream_WriteBlock( DaoxStream *self, DString *text, int offset, int width, int islist, int listdep )
{
	int fails = 0;
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
			//if( islist ) DString_Trim( part );
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
				DaoxStream_PrintCode( self, part, mode, offset, width );
				if( self->process && strcmp( mode->mbs, "test" ) == 0 ){
					fails += DaoxStream_DoTest( self, part );
				}
			}else if( mtype >= DAOX_HELP_SECTION && mtype <= DAOX_HELP_SUBSECT2 ){
				int bgcolorid = DAOX_BLACK;
				switch( mtype ){
				case DAOX_HELP_SECTION :
					self->section += 1;
					self->subsect = 0;
					self->subsect2 = 0;
					bgcolorid = DAOX_GREEN;
					break;
				case DAOX_HELP_SUBSECT :
					self->subsect += 1;
					self->subsect2 = 0;
					bgcolorid = DAOX_CYAN;
					break;
				case DAOX_HELP_SUBSECT2 :
					self->subsect2 += 1;
					bgcolorid = DAOX_YELLOW;
					break;
				}
				if( self->offset ) DaoxStream_WriteNewLine( self, "" );
				if( mtype == DAOX_HELP_SECTION ){
					DaoxStream_SetColor( self, dao_colors[DAOX_WHITE], dao_colors[DAOX_GREEN] );
					while( self->offset < (8 + part->size) ) DaoxStream_WriteMBS( self, " " );
					DaoxStream_SetColor( self, NULL, NULL );
					DaoxStream_WriteNewLine( self, "" );
				}
				DaoxStream_SetColor( self, dao_colors[DAOX_RED], dao_colors[bgcolorid] );
				if( self->section < 10 ) DaoxStream_WriteMBS( self, " " );
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
				DaoxStream_SetColor( self, NULL, NULL );
				DaoxStream_SetColor( self, dao_colors[DAOX_WHITE], dao_colors[DAOX_BLUE] );
				DaoxStream_WriteMBS( self, " " );
				DaoxStream_WriteText( self, part, 0, width );
				DaoxStream_WriteMBS( self, " " );
				if( mtype == DAOX_HELP_SECTION ){
					DaoxStream_SetColor( self, NULL, NULL );
					DaoxStream_SetColor( self, dao_colors[DAOX_WHITE], dao_colors[DAOX_GREEN] );
					DaoxStream_WriteMBS( self, "   " );
				}
				DaoxStream_SetColor( self, NULL, NULL );
				DaoxStream_WriteNewLine( self, "" );
				if( mtype == DAOX_HELP_SECTION ){
					DaoxStream_SetColor( self, dao_colors[DAOX_WHITE], dao_colors[DAOX_GREEN] );
					while( self->offset < (8 + part->size) ) DaoxStream_WriteMBS( self, " " );
					DaoxStream_SetColor( self, NULL, NULL );
					DaoxStream_WriteNewLine( self, "" );
				}
			}else if( mtype == DAOX_HELP_LIST ){
				DaoxStream_WriteList( self, part, offset, width, 1, listdep+1 );
			}else if( mtype == DAOX_HELP_NODE ){
				DaoxStream_WriteEntryName( self, part );
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
				//if( islist ) DString_Trim( part );
				if( self->offset && isspace( self->last ) == 0 ) DaoxStream_WriteChar( self, ' ' );
				DaoxStream_SetColor( self, fgcolor->mbs, bgcolor->mbs );
				DaoxStream_WriteText( self, part, offset, width );
				DaoxStream_SetColor( self, NULL, NULL );
			}
		}else{
			DString_SubString( text, part, last, text->size - last );
			//if( islist ) DString_Trim( part );
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
	return fails;
}

static void DaoxHelpBlock_Print( DaoxHelpBlock *self, DaoxStream *stream, DaoProcess *proc )
{
	int screen = DAOX_TEXT_WIDTH;
	stream->section = 0;
	stream->subsect = 0;
	stream->subsect2 = 0;
	stream->section = 0;
	stream->offset = 0;
	stream->last = 0;
	stream->nspace = self->entry->help->nspace;
	if( self->type == DAOX_HELP_TEXT ){
		int fails = DaoxStream_WriteBlock( stream, self->text, 0, screen, 0, 0 );
		self->entry->failedTests += fails;
	}else if( self->type == DAOX_HELP_CODE ){
		DaoxStream_PrintCode( stream, self->text, self->lang, 0, screen );
		if( proc && (self->lang == NULL || strcmp( self->lang->mbs, "dao" ) == 0) ){
			DaoxStream_WriteMBS( stream, "\n" );
			DaoxStream_SetColor( stream, NULL, dao_colors[DAOX_YELLOW] );
			DaoProcess_Eval( proc, self->entry->help->nspace, self->text->mbs );
			DaoxStream_SetColor( stream, NULL, NULL );
		}
	}
	DaoxStream_WriteMBS( stream, "\n" );
	if( self->next ) DaoxHelpBlock_Print( self->next, stream, proc );
}



static DaoxHelpEntry* DaoxHelpEntry_New( DString *name )
{
	DaoxHelpEntry *self = (DaoxHelpEntry*) dao_malloc( sizeof(DaoxHelpEntry) );
	self->help = NULL;
	self->nspace = NULL;
	self->title = NULL;
	self->author = NULL;
	self->license = NULL;
	self->name = DString_Copy( name );
	self->nested = DMap_New(D_STRING,0);
	self->nested2 = DArray_New(0);
	self->first = self->last = NULL;
	self->parent = NULL;
	self->failedTests = 0;
	return self;
}
static void DaoxHelpEntry_Delete( DaoxHelpEntry *self )
{
	if( self->first ) DaoxHelpBlock_Delete( self->first );
	if( self->title ) DString_Delete( self->title );
	if( self->author ) DString_Delete( self->author );
	if( self->license ) DString_Delete( self->license );
	DString_Delete( self->name );
	DArray_Delete( self->nested2 );
	DMap_Delete( self->nested );
	dao_free( self );
}
static void DaoxHelpEntry_TryReset( DaoxHelpEntry *self, DaoNamespace *NS )
{
	/* No reset if the help file is not changed: */
	if( self->nspace == NS ) return;
	/*
	// No reset if the entry content is from different help files.
	// This will allow other help files to add additional content the same help entry.
	*/
	if( ! DString_EQ( self->nspace->name, NS->name ) ) return;
	if( self->first ) DaoxHelpBlock_Delete( self->first );
	self->first = self->last = NULL;
}
static void DaoxHelpEntry_AppendText( DaoxHelpEntry *self, DaoNamespace *NS, DString *text )
{
	DaoxHelpBlock *block = DaoxHelpBlock_New();
	DaoxHelpEntry_TryReset( self, NS );
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
static void DaoxHelpEntry_AppendCode( DaoxHelpEntry *self, DaoNamespace *NS, DString *code, DString *lang )
{
	DaoxHelpBlock *block = DaoxHelpBlock_New();
	DaoxHelpEntry_TryReset( self, NS );
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
static int DaoxHelpEntry_GetNameLength( DaoxHelpEntry *self )
{
	daoint pos = DString_RFindChar( self->name, '.', -1 );;
	if( pos == MAXSIZE ) return self->name->size;
	return self->name->size - 1 - pos;
}
static void DaoxHelpEntry_PrintTree( DaoxHelpEntry *self, DaoxStream *stream, DArray *offsets, int offset, int width, int root, int last, int depth, int hlerror )
{
	DArray *old = offsets;
	DString *line = DString_New(1);
	daoint i, len = DaoxHelpEntry_GetNameLength( self );
	int extra = root ? 2 : 5;
	int color, count = 0;
	int screen = DAOX_TREE_WIDTH;

#if defined(UNIX) && !defined(MINIX)
	struct winsize ws;
	ioctl( STDOUT_FILENO, TIOCGWINSZ, &ws );
	screen = ws.ws_col - 1;
#endif

	if( depth > 4 && hlerror == 0 ) return;
	if( hlerror && self->failedTests == 0 ) return;

	if( offsets == NULL ){
		offsets = DArray_New(0);
		len = self->name->size;
	}

	DString_Resize( line, offset );
	memset( line->mbs, ' ', line->size*sizeof(char) );
	for(i=0; i<offsets->size; i++) line->mbs[ offsets->items.pInt[i] ] = '|';
	if( root == 0 ) DString_AppendMBS( line, "|--" );
	DaoxStream_SetColor( stream, NULL, NULL );
	DaoxStream_WriteString( stream, line );

	count = line->size;
	DString_AppendMBS( line, self->name->mbs + self->name->size - len );
	DaoxStream_SetColor( stream, dao_colors[DAOX_GREEN], NULL );
	DaoxStream_WriteMBS( stream, line->mbs + count );
	DaoxStream_SetColor( stream, NULL, NULL );

	count = line->size;
	for(i=len; i<(width+2); i++) DString_AppendChar( line, '-' );
	DString_AppendChar( line, '|' );
	DString_AppendMBS( line, " " );
	DaoxStream_SetColor( stream, NULL, NULL );
	DaoxStream_WriteMBS( stream, line->mbs + count );

	count = line->size;
	if( self->title ){
		DaoxStream_SetColor( stream, dao_colors[DAOX_BLUE], NULL );
		DaoxStream_WriteEntryName( stream, self->name );
		DaoxStream_SetColor( stream, NULL, NULL );

		if( hlerror ){
			DaoxStream_SetColor( stream, NULL, NULL );
			DaoxStream_WriteMBS( stream, ": " );
			DaoxStream_SetColor( stream, dao_colors[DAOX_RED], NULL );
			DaoxStream_WriteMBS( stream, "failed tests: " );
			DaoxStream_WriteInteger( stream, self->failedTests );
			DaoxStream_SetColor( stream, NULL, NULL );
		}else if( count < (screen - 20) ){
			DString *title = DString_Copy( self->title );
			DString *chunk = DString_New(0);
			int TW = screen - count;
			DaoxStream_SetColor( stream, NULL, NULL );
			DaoxStream_WriteMBS( stream, ": " );
			DString_ToWCS( title );
			if( (title->size + self->name->size + 2) < TW ){
				DaoxStream_WriteString( stream, title );
			}else{
				int next = 0;
				int start = DString_Break( title, 0, TW - (self->name->size + 2) );
				if( start >= 0 ){
					DString_SubString( title, chunk, 0, start );
					DString_Trim( chunk );
					DaoxStream_WriteString( stream, chunk );
				}else{
					start = 0;
				}
				if( last )
					memset( line->mbs + offset, ' ', (width + extra)*sizeof(char) );
				else
					memset( line->mbs + offset + 1, ' ', (width + extra - 1)*sizeof(char) );
				for(; start < title->size; start=next){
					start = DString_Break( title, start, 0 );
					next = DString_Break( title, start, TW );
					DString_SubString( title, chunk, start, next - start );
					DString_Trim( chunk );
					DaoxStream_WriteChar( stream, '\n' );
					DaoxStream_WriteString( stream, line );
					DaoxStream_WriteString( stream, chunk );
				}
			}
			DaoxStream_SetColor( stream, NULL, NULL );
			DString_Delete( chunk );
			DString_Delete( title );
		}
	}
	DaoxStream_WriteChar( stream, '\n' );
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
		DaoxHelpEntry_PrintTree( entry, stream, offsets, offset+width+extra, len, 0, last, depth+1, hlerror );
	}
	if( last ){
		memset( line->mbs + offset, ' ', (width + extra + 1)*sizeof(char) );
		DaoxStream_WriteString( stream, line );
		DaoxStream_WriteChar( stream, '\n' );
	}
	if( last == 0 ) DArray_PopBack( offsets );
	DString_Delete( line );
	if( old == NULL ) DArray_Delete( offsets );
}
static void DaoxHelpEntry_Print( DaoxHelpEntry *self, DaoxStream *stream, DaoProcess *proc )
{
	int screen = DAOX_TEXT_WIDTH;

	self->failedTests = 0;
	stream->offset = 0;
	if( self->author ){
		DString *notice = DString_Copy( daox_helper->notice );
		DString_ChangeMBS( notice, "%$%( %s* author %s* %)", self->author->mbs, 0 );
		if( self->license ){
			DString_ChangeMBS( notice, "%$%( %s* license %s* %)", self->license->mbs, 0 );
		}else{
			DString_ChangeMBS( notice, "%$%( %s* license %s* %)", "Unspecified License", 0 );
		}
		DaoxStream_SetColor( stream, dao_colors[DAOX_WHITE], dao_colors[DAOX_GREEN] );
		DaoxStream_WriteText( stream, notice, 0, screen );
		DaoxStream_SetColor( stream, NULL, NULL );
		DaoxStream_WriteNewLine( stream, "" );
		DString_Delete( notice );
	}

	stream->offset = 0;
	DaoxStream_SetColor( stream, dao_colors[DAOX_WHITE], dao_colors[DAOX_BLUE] );
	DaoxStream_WriteMBS( stream, "[NAME]" );
	DaoxStream_SetColor( stream, NULL, NULL );
	DaoxStream_WriteNewLine( stream, "" );

	if( self->name ) DaoxStream_WriteEntryName2( stream, self->name );

	DaoxStream_WriteNewLine( stream, "" );
	DaoxStream_WriteNewLine( stream, "" );
	DaoxStream_SetColor( stream, dao_colors[DAOX_WHITE], dao_colors[DAOX_BLUE] );
	DaoxStream_WriteMBS( stream, "[TITLE]" );
	DaoxStream_SetColor( stream, NULL, NULL );
	DaoxStream_WriteNewLine( stream, "" );

	stream->offset = 0;
	if( self->title ) DaoxStream_WriteText( stream, self->title, 0, screen );

	DaoxStream_WriteNewLine( stream, "" );
	DaoxStream_WriteNewLine( stream, "" );
	DaoxStream_SetColor( stream, dao_colors[DAOX_WHITE], dao_colors[DAOX_BLUE] );
	DaoxStream_WriteMBS( stream, "[DESCRIPTION]" );
	DaoxStream_SetColor( stream, NULL, NULL );
	DaoxStream_WriteNewLine( stream, "" );

	stream->offset = 0;
	if( self->first ) DaoxHelpBlock_Print( self->first, stream, proc );
	DaoxStream_WriteNewLine( stream, "" );

	if( self->nested->size ){
		DaoxStream_WriteMBS( stream, "\n" );
		DaoxStream_SetColor( stream, dao_colors[DAOX_WHITE], dao_colors[DAOX_BLUE] );
		DaoxStream_WriteMBS( stream, "[STRUCTURE]" );
		DaoxStream_SetColor( stream, NULL, NULL );
		DaoxStream_WriteMBS( stream, "\n\n" );
		DaoxHelpEntry_PrintTree( self, stream, NULL, 0, self->name->size, 1, 1, 1, 0 );
	}
}



static DaoxHelp* DaoxHelp_New()
{
	DaoxHelp *self = (DaoxHelp*) dao_malloc( sizeof(DaoxHelp) );
	self->entries = DMap_New(D_STRING,0);
	self->current = NULL;
	self->nspace = NULL;
	return self;
}
static void DaoxHelp_Delete( DaoxHelp *self )
{
	/* Do not delete entries here, they are managed by DaoxHelper; */
	free( self );
}



static DaoxHelper* DaoxHelper_New()
{
	int i;
	DString name = DString_WrapMBS( "ALL" );
	DaoxHelper *self = (DaoxHelper*) dao_malloc( sizeof(DaoxHelper) );
	self->helps = DHash_New(0,0);
	self->tree = DaoxHelpEntry_New( & name );
	self->entries = DMap_New(D_STRING,0);
	self->nslist = DArray_New(D_VALUE);
	self->notice = DString_New(1);
	self->cxxKeywords = DMap_New(D_STRING,0);
	DString_SetMBS( self->notice, "By $(author). Released under the $(license).\n" );
	for(i=0; i<100; i++){
		DIntStringPair pair =  daox_help_cxx_keywords[i];
		DString name = DString_WrapMBS( pair.key );
		if( pair.value == 0 ) break;
		DMap_Insert( self->cxxKeywords, & name, (void*)(daoint) pair.value );
	}
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
	DMap_Delete( self->cxxKeywords );
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
static DaoxHelpEntry* DaoxHelper_GetEntry( DaoxHelper *self, DaoxHelp *help, DaoNamespace *NS, DString *name )
{
	DaoxHelpEntry *entry, *entry2;
	DNode *it = DMap_Find( self->entries, name );
	daoint pos = DString_RFindChar( name, '.', -1 );

	if( it ) return (DaoxHelpEntry*) it->value.pVoid;
	entry = DaoxHelpEntry_New( name );
	if( entry->nspace == NULL ) entry->nspace = NS;
	entry->help = help;
	if( pos == MAXSIZE ){
		DArray_Append( self->tree->nested2, entry );
		DMap_Insert( self->tree->nested, name, entry );
		entry->parent = self->tree;
	}else{
		DString *name2 = DString_Copy( name );
		DString_Erase( name2, pos, -1 );
		entry2 = DaoxHelper_GetEntry( self, help, NS, name2 );
		DString_SubString( name, name2, pos + 1, -1 );
		DArray_Append( entry2->nested2, entry );
		DMap_Insert( entry2->nested, name2, entry );
		entry->parent = entry2;
	}
	DMap_Insert( self->entries, name, entry );
	return entry;
}


static void DaoProcess_Print( DaoProcess *self, const char *chs )
{
	if( self->stdioStream ){
		DaoStream_WriteMBS( self->stdioStream, chs );
		return;
	}
	DaoStream_WriteMBS( self->vmSpace->stdioStream, chs );
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
		NS = DaoVmSpace_Load( proc->vmSpace, name2->mbs );
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
	DaoxStream *stream;
	DaoStream *stdio = proc->stdioStream;
	if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
	DaoProcess_PutValue( proc, daox_cdata_helper );
	stream = DaoxStream_New( stdio, NULL );
	DaoxHelpEntry_PrintTree( daox_helper->tree, stream, NULL, 0, daox_helper->tree->name->size, 1,1,1, 0 );
	DaoxStream_Delete( stream );
}
static void HELP_Help2( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *entry_name = p[0]->xString.data;
	DaoProcess *newproc = NULL;
	DaoStream *stdio = proc->stdioStream;
	DaoxStream *stream;
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

	if( p[1]->xInteger.value ) newproc = DaoVmSpace_AcquireProcess( proc->vmSpace );
	stream = DaoxStream_New( stdio, newproc );

	DaoxHelpEntry_Print( entry, stream, newproc );
	if( newproc ) DaoVmSpace_ReleaseProcess( proc->vmSpace, newproc );
	DaoxStream_Delete( stream );
}
static void HELP_Help3( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess *newproc = NULL;
	DaoStream *stdio = proc->stdioStream;
	DaoType *type = DaoNamespace_GetType( proc->activeNamespace, p[0] );
	DaoxStream *stream;
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

	if( p[2]->xInteger.value ) newproc = DaoVmSpace_AcquireProcess( proc->vmSpace );
	stream = DaoxStream_New( stdio, newproc );

	DaoxHelpEntry_Print( entry, stream, newproc );
	if( newproc ) DaoVmSpace_ReleaseProcess( proc->vmSpace, newproc );
	DaoxStream_Delete( stream );
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
	char *file = DaoValue_TryGetMBString( p[0] );
	DaoStream *stdio = proc->stdioStream;
	DaoNamespace *NS = DaoVmSpace_Load( proc->vmSpace, file );
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

static void DaoxHelpEntry_ExportHTML( DaoxHelpEntry *self, DaoxStream *stream, DString *dir )
{
	daoint i;
	FILE *fout;
	const char *title = self->title ? self->title->mbs : "";
	DString *fname = DString_Copy( dir );

	if( dir->size ) DString_AppendChar( fname, '/' );
	if( self->parent ){
		DString_Append( fname, self->name );
	}else{
		DString_AppendMBS( fname, "index" );
	}
	DString_AppendMBS( fname, ".html" );
	DString_Reset( stream->output, 0 );
	DString_AppendMBS( stream->output, "\n<pre style=\"font-weight:500\">\n" );
	if( self->parent ){
		DaoxHelpEntry_Print( self, stream, stream->process );
	}else{
		DaoxHelpEntry_PrintTree( self, stream, NULL, 0, self->name->size, 1,1,1,0 );
	}
	DString_AppendMBS( stream->output, "\n</pre>\n" );
	fout = fopen( fname->mbs, "w+" );
	if( fout == NULL ) fout = stdout;
	fprintf( fout, "<!DOCTYPE html><html><head>\n<title>Dao Help: %s</title>\n", title );
	fprintf( fout, "<meta charset=\"utf-8\"/></head><body>%s", stream->output->mbs );
	if( fout != stdout ) fclose( fout );

	for(i=0; i<self->nested2->size; i++){
		DaoxHelpEntry *entry = (DaoxHelpEntry*) self->nested2->items.pVoid[i];
		DaoxHelpEntry_ExportHTML( entry, stream, dir );
		self->failedTests += entry->failedTests;
	}
	DString_Reset( stream->output, 0 );
	if( self->failedTests ){
		DaoxHelpEntry_PrintTree( self, stream, NULL, 0, self->name->size, 1,1,1,1 );
	}
	fout = fopen( fname->mbs, "a+" );
	if( fout == NULL ) fout = stdout;
	fprintf( fout, "\n<pre style=\"font-weight:500\">\n%s</pre></body></html>", stream->output->mbs );
	if( fout != stdout ) fclose( fout );
}
static void HELP_Export( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *entry_name = p[0]->xString.data;
	DString *dir_name = p[1]->xString.data;
	DaoProcess *newproc = NULL;
	DaoStream *stdio = proc->stdioStream;
	DaoxStream *stream;
	DaoxHelpEntry *entry = NULL;
	DaoxHelp *help = NULL;

	DaoProcess_PutValue( proc, daox_cdata_helper );
	if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
	if( entry_name->size == 0 ){
		entry = daox_helper->tree;
	}else{
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
	}

	if( p[3]->xInteger.value ) newproc = DaoVmSpace_AcquireProcess( proc->vmSpace );
	stream = DaoxStream_New( stdio, newproc );
	stream->output = DString_New(1);
	stream->fmtHTML = 1;
	DaoxHelpEntry_ExportHTML( entry, stream, dir_name );
	if( newproc ) DaoVmSpace_ReleaseProcess( proc->vmSpace, newproc );
	DString_Delete( stream->output );
	DaoxStream_Delete( stream );
}


static DaoFuncItem helpMeths[]=
{
	{ HELP_Help1,     "help()" },
	{ HELP_Help2,     "help( keyword :string, run = 0 )" },
	{ HELP_Help3,     "help( object :any, keyword :string, run = 0 )" },
	{ HELP_Search,    "search( keywords :string )" },
	{ HELP_Search2,   "search( object :any, keywords :string )" },
	{ HELP_Load,      "load( help_file :string )" },
	{ HELP_List,      "list( object :any, type :enum<values,methods,auxmeths>=$methods )" },
	{ HELP_SetTempl,  "set_template( tpl :string, ttype :enum<notice> )" },
	{ HELP_Export,    "export( root = '', dir = '', format :enum<html> = $html, run = 0 )" },
	{ NULL, NULL }
};

static DaoTypeBase helpTyper =
{
	"help", NULL, NULL, helpMeths, {0}, {0},
	(FuncPtrDel) DaoxHelper_Delete, DaoxHelper_GetGCFields
};

static DString* dao_verbatim_content( DString *VT )
{
	DString *content = DString_New(1);
	daoint rb = DString_FindChar( VT, ']', 0 );
	DString_SetDataMBS( content, VT->mbs + rb + 1, VT->size - 2*(rb + 1) );
	DString_Trim( content );
	return content;
}
static DString* dao_verbatim_code( DString *VT )
{
	DString *content = DString_New(1);
	daoint rb = DString_FindChar( VT, ']', 0 );
	DString_SetDataMBS( content, VT->mbs + rb + 1, VT->size - 2*(rb + 1) );
	return content;
}
static int dao_string_delete( DString *string, int retc )
{
	if( string ) DString_Delete( string );
	return retc;
}
static int dao_help_name( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out, int line )
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
		DaoxHelpEntry *entry = DaoxHelper_GetEntry( daox_helper, help, NS, name );
		it = DMap_Insert( help->entries, name, entry );
	}
	help->current = (DaoxHelpEntry*) it->value.pVoid;
	return dao_string_delete( name, 0 );
}
static int dao_help_title( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out, int line )
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
static int dao_help_text( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out, int line )
{
	DString *text = dao_verbatim_content( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, NULL );
	//printf( "%s\n", text->mbs );
	if( help == NULL || help->current == NULL ) return dao_string_delete( text, 1 );
	//printf( "%s %p %p\n", text->mbs, help->nspace, NS );
	DaoxHelpEntry_AppendText( help->current, NS, text );
	return dao_string_delete( text, 0 );
}
static int dao_help_code( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out, int line )
{
	DString *code = dao_verbatim_code( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, NULL );
	//printf( "%s\n", code->mbs );
	if( help == NULL || help->current == NULL ) return dao_string_delete( code, 1 );
	DaoxHelpEntry_AppendCode( help->current, NS, code, NULL );
	return dao_string_delete( code, 0 );
}
static int dao_help_author( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out, int line )
{
	DString *code = dao_verbatim_content( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, NULL );
	//printf( "%s\n", code->mbs );
	if( help == NULL || help->current == NULL ) return dao_string_delete( code, 1 );
	if( help->current->author == NULL ) help->current->author = DString_New(1);
	DString_Assign( help->current->author, code );
	return dao_string_delete( code, 0 );
}
static int dao_help_license( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out, int line )
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

DAO_DLL int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	const char *evname = "DAO_HELP_LANG";
	char *lang = getenv( evname );
	int len = strlen( evname );
	DString *fname = DString_New(1);
	DaoNamespace *mod;
	DaoType *type;

	if( lang == NULL ){
		char *lang2 = getenv( "LANG" );
		if( lang2 == NULL ) lang2 = setlocale( LC_CTYPE, NULL );
		lang = (char*) malloc( len + 4 );
		lang[0] = '\0';
		strcat( lang, evname );
		strcat( lang, "=" );
		strncat( lang, lang2, 2 );
		lang[len+1] = tolower( lang[len+1] );
		lang[len+2] = tolower( lang[len+2] );
		putenv( lang );
		lang += len + 1;
	}

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

	DString_SetMBS( fname, "help_" );
	DString_AppendMBS( fname, lang );
	DString_AppendMBS( fname, "/help_dao" );
	DaoVmSpace_Load( vmSpace, fname->mbs );

	DString_SetMBS( fname, "help_" );
	DString_AppendMBS( fname, lang );
	DString_AppendMBS( fname, "/help_daovm" );
	DaoVmSpace_Load( vmSpace, fname->mbs );

	DString_SetMBS( fname, "help_" );
	DString_AppendMBS( fname, lang );
	DString_AppendMBS( fname, "/help_help" );
	mod = DaoVmSpace_Load( vmSpace, fname->mbs );
	if( mod ){
		DaoValue *value = DaoNamespace_FindData( mod, "help_message" );
		if( value ) DaoNamespace_AddValue( ns, "help_message", value, NULL );
	}

	DString_SetMBS( fname, "help_" );
	DString_AppendMBS( fname, lang );
	DString_AppendMBS( fname, "/help_tool" );
	DaoVmSpace_Load( vmSpace, fname->mbs );

	DString_SetMBS( fname, "help_" );
	DString_AppendMBS( fname, lang );
	DString_AppendMBS( fname, "/help_module" );
	DaoVmSpace_Load( vmSpace, fname->mbs );

	DString_Delete( fname );
	return 0;
}

