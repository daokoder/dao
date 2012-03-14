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
#include"daoParser.h"
#include"daoNamespace.h"
#include"daoVmspace.h"
#include"daoGC.h"

#ifdef WIN32

#include<windows.h>
#include<io.h>

enum DaoxCodeColor
{
	DAOX_BLACK,
	DAOX_BLUE,
	DAOX_GREEN,
	DAOX_CYAN,
	DAOX_RED,
	DAOX_MAGENTA,
	DAOX_YELLOW,
	DAOX_WHITE
};

int SetCharForeground( DaoStream *stream, int color )
{
	WORD attr;
	int res = 0;
	struct _CONSOLE_SCREEN_BUFFER_INFO info;
	HANDLE fd = (HANDLE)_get_osfhandle( _fileno( DaoStream_GetFile( stream ) ) );
	if( fd == INVALID_HANDLE_VALUE )
		fd = GetStdHandle( STD_OUTPUT_HANDLE );
	if( !GetConsoleScreenBufferInfo( fd, &info ) )
		return 255;
	attr = info.wAttributes;
	if( attr & FOREGROUND_BLUE )
		res |= 1;
	if( attr & FOREGROUND_GREEN )
		res |= 2;
	if( attr & FOREGROUND_RED )
		res |= 4;
	attr = attr & ~FOREGROUND_BLUE & ~FOREGROUND_GREEN & ~FOREGROUND_RED;
	if( color & 1 )
		attr |= FOREGROUND_BLUE;
	if( color & 2 )
		attr |= FOREGROUND_GREEN;
	if( color & 4 )
		attr |= FOREGROUND_RED;
	if( !SetConsoleTextAttribute( fd, attr ) )
		return 255;
	return res;
}

int SetCharBackground( DaoStream *stream, int color )
{
	WORD attr;
	int res = 0;
	struct _CONSOLE_SCREEN_BUFFER_INFO info;
	HANDLE fd = (HANDLE)_get_osfhandle( _fileno( DaoStream_GetFile( stream ) ) );
	if( fd == INVALID_HANDLE_VALUE )
		fd = GetStdHandle( STD_OUTPUT_HANDLE );
	if( !GetConsoleScreenBufferInfo( fd, &info ) )
		return 255;
	attr = info.wAttributes;
	if( attr & BACKGROUND_BLUE )
		res |= 1;
	if( attr & BACKGROUND_GREEN )
		res |= 2;
	if( attr & BACKGROUND_RED )
		res |= 4;
	attr = attr & ~BACKGROUND_BLUE & ~BACKGROUND_GREEN & ~BACKGROUND_RED;
	if( color & 1 )
		attr |= BACKGROUND_BLUE;
	if( color & 2 )
		attr |= BACKGROUND_GREEN;
	if( color & 4 )
		attr |= BACKGROUND_RED;
	if( !SetConsoleTextAttribute( fd, attr ) )
		return 255;
	return res;
}

#else

#define CSI_RESET "\033[0m"
#define CSI_FCOLOR "\033[3%im"
#define CSI_BCOLOR "\033[4%im"

enum DaoxCodeColor
{
	DAOX_BLACK,
	DAOX_RED,
	DAOX_GREEN,
	DAOX_YELLOW,
	DAOX_BLUE,
	DAOX_MAGENTA,
	DAOX_CYAN,
	DAOX_WHITE
};

int SetCharForeground( DaoStream *stream, int color )
{
	char buf[20];
	wchar_t wbuf[20];
	if( color == 254 )
		snprintf( buf, sizeof( buf ), CSI_RESET );
	else
		snprintf( buf, sizeof( buf ), CSI_FCOLOR, color );
	DaoStream_WriteMBS( stream, buf );
	return 254;
}

int SetCharBackground( DaoStream *stream, int color )
{
	char buf[20];
	wchar_t wbuf[20];
	if( color == 254 )
		snprintf( buf, sizeof( buf ), CSI_RESET );
	else
		snprintf( buf, sizeof( buf ), CSI_BCOLOR, color );
	DaoStream_WriteMBS( stream, buf );
	return 254;
}

#endif


int SetColor( DaoStream *stream, int color )
{
	int fgcolor = (color & 0xff);
	int bgcolor = (color >> 8);
	fgcolor = SetCharForeground( stream, fgcolor );
	bgcolor = SetCharBackground( stream, bgcolor );
	return fgcolor | (bgcolor<<8);
}



typedef struct DaoxHelpBlock   DaoxHelpBlock;
typedef struct DaoxHelpEntry   DaoxHelpEntry;
typedef struct DaoxHelp        DaoxHelp;
typedef struct DaoxHelper      DaoxHelper;

struct DaoxHelpBlock
{
	DaoxHelpEntry *entry;

	DString *text;
	DString *code;
	DString *lang;

	DaoxHelpBlock *next;
};
struct DaoxHelpEntry
{
	DaoxHelp *help;

	DString *name;
	DString *title;
	DaoxHelpBlock *first;
	DaoxHelpBlock *last;
};
struct DaoxHelp
{
	DaoNamespace   *nspace;

	DMap           *entries;
	DaoxHelpEntry  *current;
};
struct DaoxHelper
{
	DMap *helps;
	DArray *nslist;
};
DaoxHelper *daox_helper = NULL;
DaoValue *daox_cdata_helper = NULL;
DaoVmSpace *dao_vmspace = NULL;



static DaoxHelpBlock* DaoxHelpBlock_New()
{
	DaoxHelpBlock *self = (DaoxHelpBlock*) dao_malloc( sizeof(DaoxHelpBlock) );
	self->text = self->code = self->lang = NULL;
	self->next = NULL;
	return self;
}
static void DaoxHelpBlock_Delete( DaoxHelpBlock *self )
{
	if( self->next ) DaoxHelpBlock_Delete( self->next );
	if( self->text ) DString_Delete( self->text );
	if( self->code ) DString_Delete( self->code );
	if( self->lang ) DString_Delete( self->lang );
	dao_free( self );
}
static void DaoxHelpBlock_PrintCode( DaoxHelpBlock *self, DaoStream *stream )
{
	DArray *tokens = DArray_New(D_TOKEN);
	daoint i, fgcolor, bgcolor;
	DaoToken_Tokenize( tokens, self->code->mbs, 0, 1, 1 );
	for(i=0; i<tokens->size; i++){
		DaoToken *tok = tokens->items.pToken[i];
		fgcolor = 0;
		switch( tok->name ){
		case DTOK_MBS : case DTOK_WCS :
		case DTOK_MBS_OPEN : case DTOK_WCS_OPEN :
		case DTOK_DIGITS_HEX : case DTOK_DIGITS_DEC :
		case DTOK_NUMBER_HEX : case DTOK_NUMBER_DEC : case DTOK_NUMBER_SCI :
			fgcolor = DAOX_RED;
			break;
		case DTOK_CMT_OPEN : case DTOK_COMMENT :
			fgcolor = DAOX_BLUE;
			break;
		case DKEY_USE : case DKEY_LOAD : case DKEY_BIND :
		case DKEY_AS : case DKEY_SYNTAX : 
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
		case DKEY_TRY : case DKEY_RETRY : case DKEY_RAISE :
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
		case DKEY_OPERATOR :
		case DKEY_SELF :
		case DTOK_ID_THTYPE :
		case DTOK_ID_SYMBOL :
			fgcolor = DAOX_GREEN;
			break;
		default: break;
		}
		fgcolor = SetCharForeground( stream, fgcolor );
		DaoStream_WriteString( stream, tokens->items.pToken[i]->string );
		fgcolor = SetCharForeground( stream, fgcolor );
	}
	DArray_Delete( tokens );
}
static void DaoxHelpBlock_Print( DaoxHelpBlock *self, DaoStream *stream, DaoProcess *proc )
{
	int bgcolor = DAOX_YELLOW;
	if( self->text ){
		DaoStream_WriteString( stream, self->text );
	}else if( self->code ){
		DaoxHelpBlock_PrintCode( self, stream );
		if( proc && (self->lang == NULL || strcmp( self->lang->mbs, "dao" ) == 0) ){
			DaoStream_WriteMBS( stream, "\n" );
			bgcolor = SetCharBackground( stream, bgcolor );
			DaoProcess_Eval( proc, self->entry->help->nspace, self->code, 1 );
			bgcolor = SetCharBackground( stream, bgcolor );
		}
	}
	DaoStream_WriteMBS( stream, "\n" );
	if( self->next ) DaoxHelpBlock_Print( self->next, stream, proc );
}



static DaoxHelpEntry* DaoxHelpEntry_New( DString *name )
{
	DaoxHelpEntry *self = (DaoxHelpEntry*) dao_malloc( sizeof(DaoxHelpEntry) );
	self->name = DString_Copy( name );
	self->title = DString_New(1);
	self->first = self->last = NULL;
	return self;
}
static void DaoxHelpEntry_Delete( DaoxHelpEntry *self )
{
	if( self->first ) DaoxHelpBlock_Delete( self->first );
	DString_Delete( self->name );
	DString_Delete( self->title );
	dao_free( self );
}
static void DaoxHelpEntry_AppendText( DaoxHelpEntry *self, DString *text )
{
	DaoxHelpBlock *block = DaoxHelpBlock_New();
	block->entry = self;
	block->text = DString_Copy( text );
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
	block->code = DString_Copy( code );
	block->lang = lang ? DString_Copy( lang ) : NULL;
	if( self->last ){
		self->last->next = block;
		self->last = block;
	}else{
		self->first = self->last = block;
	}
}
static void DaoxHelpEntry_Print( DaoxHelpEntry *self, DaoStream *stream, DaoProcess *proc )
{
	int scolor = DAOX_WHITE|(DAOX_BLUE<<8);
	int color = SetColor( stream, scolor );
	DaoStream_WriteMBS( stream, "NAME\n" );
	color = SetColor( stream, color );

	DaoStream_WriteString( stream, self->name );

	color = SetColor( stream, scolor );
	DaoStream_WriteMBS( stream, "\n\nSYNOPSIS\n" );
	color = SetColor( stream, color );

	DaoStream_WriteString( stream, self->title );

	color = SetColor( stream, scolor );
	DaoStream_WriteMBS( stream, "\n\nDESCRIPTION\n" );
	color = SetColor( stream, color );

	if( self->first ) DaoxHelpBlock_Print( self->first, stream, proc );
	DaoStream_WriteMBS( stream, "\n" );
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
	DNode *it;
	for(it=DMap_First(self->entries); it; it=DMap_Next(self->entries,it)){
		DaoxHelpEntry_Delete( (DaoxHelpEntry*) it->value.pVoid );
	}
	free( self );
}



static DaoxHelper* DaoxHelper_New()
{
	DaoxHelper *self = (DaoxHelper*) dao_malloc( sizeof(DaoxHelper) );
	self->helps = DHash_New(0,0);
	self->nslist = DArray_New(D_VALUE);
	return self;
}
static void DaoxHelper_Delete( DaoxHelper *self )
{
	DNode *it;
	for(it=DMap_First(self->helps); it; it=DMap_Next(self->helps,it)){
		DaoxHelp_Delete( (DaoxHelp*) it->value.pVoid );
	}
	DMap_Delete( self->helps );
	DArray_Delete( self->nslist );
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
		DString_ChangeMBS( name2, "%.", "_", 0 );
		NS = DaoVmSpace_Load( proc->vmSpace, name2, 0 );
		if( NS ) break;
		pos = DString_RFindChar( name, '.', -1 );
		if( pos < 0 ) break;
		DString_Erase( name, pos, -1 );
	}
	DString_Delete( name );
	DString_Delete( name2 );

	help = DaoxHelper_Get( daox_helper, NS, NULL );
	//printf( "%s %p\n", name->mbs, help );
	if( help == NULL ){
		if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
		DaoStream_WriteMBS( stdio, "No help document available for \"" );
		DaoStream_WriteString( stdio, entry_name );
		DaoStream_WriteMBS( stdio, "\"\n" );
		DString_Delete( name );
		DString_Delete( name2 );
		return NULL;
	}
	return help;
}

static DaoxHelpEntry* HELP_GetEntry( DaoProcess *proc, DaoxHelp *help, DString *entry_name )
{
	DaoxHelpEntry *entry = NULL;
	DaoStream *stdio = proc->stdioStream;
	DString *name = DString_New(1);
	daoint pos;
	DString_Assign( name, entry_name );
	DString_ToMBS( name );
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
	if( entry == NULL ){
		DaoStream_WriteMBS( stdio, "No help entry available for \"" );
		DaoStream_WriteString( stdio, entry_name );
		DaoStream_WriteMBS( stdio, "\"\n" );
		DString_Delete( name );
		return NULL;
	}
	if( DString_EQ( entry->name, entry_name ) == 0 ){
		DaoStream_WriteMBS( stdio, "No help entry available for \"" );
		DaoStream_WriteString( stdio, entry_name );
		DaoStream_WriteMBS( stdio, "\",\n" );
		DaoStream_WriteMBS( stdio, "a more generical entry is used!\n\n" );
	}
	DString_Delete( name );
	return entry;
}
static void HELP_Help( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess *newproc = NULL;
	DaoStream *stdio = proc->stdioStream;
	DaoxHelp *help = HELP_GetHelp( proc, p[0]->xString.data );
	DaoxHelpEntry *entry = HELP_GetEntry( proc, help, p[0]->xString.data );
	if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
	if( entry ){
		if( p[1]->xInteger.value ) newproc = DaoVmSpace_AcquireProcess( proc->vmSpace );
		DaoxHelpEntry_Print( entry, stdio, newproc );
		if( newproc ) DaoVmSpace_ReleaseProcess( proc->vmSpace, newproc );
	}
	DaoProcess_PutValue( proc, daox_cdata_helper );
}
static void HELP_Help2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess *newproc = NULL;
	DaoStream *stdio = proc->stdioStream;
	DaoType *type = DaoNamespace_GetType( proc->activeNamespace, p[0] );
	DaoxHelpEntry *entry;
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
	help = HELP_GetHelp( proc, p[0]->xString.data );
	entry = HELP_GetEntry( proc, help, p[0]->xString.data );
	if( entry ){
		if( p[2]->xInteger.value ) newproc = DaoVmSpace_AcquireProcess( proc->vmSpace );
		DaoxHelpEntry_Print( entry, stdio, newproc );
		if( newproc ) DaoVmSpace_ReleaseProcess( proc->vmSpace, newproc );
	}
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

static DaoFuncItem helpMeths[]=
{
	{ HELP_Help,      "help( keyword :string, run = 0 )" },
	{ HELP_Help2,     "help( object :any, keyword :string, run = 0 )" },
	{ HELP_Search,    "search( keyword :string )" },
	{ HELP_Search2,   "search( object :any, keyword :string )" },
	{ HELP_Load,      "load( help_file :string )" },
	{ HELP_List,      "list( object :any, type :enum<values,methods,auxmeths>=$methods )" },
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
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, name );
	DNode *it;
	if( help == NULL ) return dao_string_delete( name, 1 );
	//printf( "dao_help_name: %s\n", name->mbs );
	it = DMap_Find( help->entries, name );
	if( it == NULL ){
		DaoxHelpEntry *entry = DaoxHelpEntry_New( name );
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

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoType *type;
	dao_vmspace = vmSpace;
	DaoNamespace_AddCodeInliner( ns, "name", dao_help_name );
	DaoNamespace_AddCodeInliner( ns, "title", dao_help_title );
	DaoNamespace_AddCodeInliner( ns, "text", dao_help_text );
	DaoNamespace_AddCodeInliner( ns, "code", dao_help_code );
	type = DaoNamespace_WrapType( ns, & helpTyper, 1 );
	daox_helper = DaoxHelper_New();
	daox_cdata_helper = (DaoValue*) DaoCdata_New( type, daox_helper );
	DaoNamespace_AddConstValue( ns, "__helper__", daox_cdata_helper );
	return 0;
}

