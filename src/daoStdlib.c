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

#include"time.h"
#include"math.h"
#include"string.h"
#include"locale.h"
#include"errno.h"
#include<sys/stat.h>

#ifdef _MSC_VER
#include<io.h> /* XXX ? */
#define chdir _chdir
#define rmdir _rmdir
#define getcwd _getcwd
#define mkdir _mkdir
#define stat _stat
#else
#include"dirent.h"
#endif

#ifdef UNIX
#include<unistd.h>
#include<sys/time.h>
#endif

#include"daoStdlib.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoClass.h"
#include"daoStream.h"
#include"daoThread.h"
#include"daoObject.h"
#include"daoRoutine.h"
#include"daoVmspace.h"
#include"daoNamespace.h"
#include"daoNumtype.h"
#include"daoParser.h"
#include"daoGC.h"
#include"daoSched.h"

static void STD_Compile( DaoContext *ctx, DValue *p[], int N )
{
	DaoVmProcess *vmp = ctx->process;
	DaoNameSpace *ns = ctx->nameSpace;
	if( DaoVmProcess_Compile( vmp, ns, p[0]->v.s, p[1]->v.i ) ==0 ){
		DaoContext_SetResult( ctx, & nil );
		return;
	}
	DaoContext_SetResult( ctx, ns->mainRoutines->items.pBase[ ns->mainRoutines->size-1 ] );
}
static void STD_Eval( DaoContext *ctx, DValue *p[], int N )
{
	DaoVmSpace *vms = ctx->vmSpace;
	DaoNameSpace *ns = ctx->nameSpace;
	DaoStream *prevStream = vms->stdStream->redirect;
	DaoStream *redirect = (DaoStream*) p[2]->v.p;
	dint *num = DaoContext_PutInteger( ctx, 0 );
	int safe = p[3]->v.i;
	int wasProt = 0;
	if( vms->options & DAO_EXEC_SAFE ) wasProt = 1;
	if( redirect != prevStream ) vms->stdStream->redirect = redirect;

	if( safe ) vms->options |= DAO_EXEC_SAFE;
	*num = DaoVmProcess_Eval( ctx->process, ns, p[0]->v.s, p[1]->v.i );
	if( ! wasProt ) vms->options &= ~DAO_EXEC_SAFE;
	if( redirect != prevStream ) vms->stdStream->redirect = prevStream;
}
static void STD_Load( DaoContext *ctx, DValue *p[], int N )
{
	DString *name = p[0]->v.s;
	int wasProt = 0;
	int import = p[1]->v.i;
	int safe = p[2]->v.i;
	int res = 0;
	DaoVmSpace *vms = ctx->vmSpace;
	DaoNameSpace *ns;
	DString_ToMBS( name );
	if( safe ) vms->options |= DAO_EXEC_SAFE;
	if( vms->options & DAO_EXEC_SAFE ) wasProt = 1;
	DArray_PushFront( vms->pathLoading, ctx->nameSpace->path );
	ns = DaoVmSpace_Load( vms, name );
	DaoContext_SetResult( ctx, (DaoBase*) ns );
	if( ! wasProt ) vms->options &= ~DAO_EXEC_SAFE;
#if 0
	if( ns ){ /* in the case that it is cancelled from console */
		DArray_PushFront( vms->pathLoading, ns->path );
		res = DaoVmProcess_Call( ctx->process, ns->mainRoutine, NULL, NULL, 0 );
		if( ctx->process->stopit | vms->stopit )
			DaoContext_RaiseException( ctx, DAO_ERROR, "loading cancelled" );
		else if( res == 0 )
			DaoContext_RaiseException( ctx, DAO_ERROR, "loading failed" );
		DArray_PopFront( vms->pathLoading );
	}
#endif
	DArray_PopFront( vms->pathLoading );
	if( import && ns ) DaoNameSpace_Import( ctx->nameSpace, ns, NULL );
}
static void Dao_AboutVar( DaoNameSpace *ns, DValue var, DString *str )
{
	DaoType *abtp = DaoNameSpace_GetTypeV( ns, var );
	void *p = var.v.p;
	char buf[50];
	if( var.t >= DAO_INTEGER && var.t <= DAO_DOUBLE ) p = & var.v;
	if( abtp ){
		DString_Append( str, abtp->name );
		sprintf( buf, "[%p]", p );
		DString_AppendMBS( str, buf );
	}else{
		DString_AppendMBS( str, "NULL" );
	}
}
static void Dao_AboutVars( DaoNameSpace *ns, DValue *par[], int N, DString *str )
{
	int i;
	DString_Clear( str );
	for( i=0; i<N; i++ ){
		Dao_AboutVar( ns, *par[i], str );
		if( i+1<N ) DString_AppendMBS( str, " " );
	}
}
static void STD_About( DaoContext *ctx, DValue *p[], int N )
{
	DString *str = DaoContext_PutMBString( ctx, "" );
	Dao_AboutVars( ctx->nameSpace, p, N, str );
}
static void STD_Callable( DaoContext *ctx, DValue *p[], int N )
{
	DValue p0 = *p[0];
	dint *res = DaoContext_PutInteger( ctx, 0 );
	if( p0.t == 0 ){
		*res = 0;
		return;
	}
	switch( p0.t ){
	case DAO_CLASS :
	case DAO_ROUTINE :
	case DAO_FUNCTION :
	case DAO_CONTEXT :
		*res = 1;
		break;
	case DAO_OBJECT :
		{
			DaoObject *object = p0.v.object;
			DString *mbs = DString_New(1);
			DString_SetMBS( mbs, "()" );
			DaoObject_GetData( object, mbs, & p0, ctx->object, NULL );
			DString_Delete( mbs );
			if( p0.t == DAO_ROUTINE ) *res = 1;
			break;
		}
	case DAO_CDATA :
		{
			DaoCData *plugin = (DaoCData*) p[0]->v.p;
			DaoTypeBase *tp = plugin->typer;
			DaoFunction *func;
			if( plugin->data == NULL && (plugin->trait & DAO_DATA_CONST) ){
				func = DaoFindFunction2( tp, tp->name );
				*res = func != NULL;
			}else{
				func = DaoFindFunction2( tp, "()" );
				if( func ) *res = 1;
			}
			break;
		}
	default : break;
	}
}
static void STD_Copy( DaoContext *ctx, DValue *p[], int N )
{
	DMap *cycData = DMap_New(0,0);
	DaoTypeBase *typer = DValue_GetTyper( *p[0] );
	DaoContext_PutValue( ctx, typer->priv->Copy( p[0], ctx, cycData ) );
	DMap_Delete( cycData );
}

extern void SplitByWhiteSpaces( DString *str, DArray *tokens );

static const char *const sep =
"-------------------------------------------------------------------\n";
static const char *const help =
"h, help:       print this help info.\n"
"q, quit:       quit debugging.\n"
"k, kill:       kill the current virtual process.\n"
"a, about reg:  print info. about the data held by the register.\n" 
"g, goto id:    goto id-th instruction.\n"
"l, list num:   list num instructions before or after the current.\n"
"p, print reg:  print the data held by the register.\n"
"t, trace dep:  trace back dep-depth in the calling stack.\n";

void DaoVmProcess_Trace( DaoVmProcess *self, int depth )
{
	DaoContext *ctx2, *ctx = self->topFrame->context;
	DaoStream *stream = ctx->vmSpace->stdStream;
	DaoVmFrame *frame = self->topFrame;
	int k, i = 0;
	while( frame && frame->context ){
		if( ++i > depth ) break;
		k = (i==1) ? (int)( ctx->vmc - ctx->codes ) : frame->entry;
		ctx2 = frame->context;
		frame = frame->prev;

		DaoStream_WriteString( stream, ctx2->routine->routName );
		DaoStream_WriteMBS( stream, "(): " );
		if( ctx2->routine->routType ) DaoStream_WriteString( stream, ctx2->routine->routType->name );
		DaoStream_WriteMBS( stream, ", instruction " );
		DaoStream_WriteInt( stream, k );
		DaoStream_WriteMBS( stream, " at line " );
		DaoStream_WriteInt( stream, ctx2->routine->annotCodes->items.pVmc[k]->line );
		DaoStream_WriteMBS( stream, " in " );
		DaoStream_WriteString( stream, ctx2->nameSpace->name );
		DaoStream_WriteMBS( stream, ";" );
		DaoStream_WriteNewLine( stream );
	}
}
void DaoRoutine_FormatCode( DaoRoutine *self, int i, DString *output );
void STD_Debug( DaoContext *ctx, DValue *p[], int N )
{
	DaoUserHandler *handler = ctx->vmSpace->userHandler;
	DaoRoutine *routine = ctx->routine;
	DaoStream *stream = ctx->vmSpace->stdStream;
	DString *input;
	DArray *tokens;
	DMap   *cycData;
	char *chs, *cmd, buffer[100];
	int i, j;
	if( ! (ctx->vmSpace->options & DAO_EXEC_DEBUG ) ) return;
	input = DString_New(1);
	if( N > 0 && p[0]->t == DAO_STREAM ){
		stream = (DaoStream*)p[0]->v.p;
		p ++;
		N --;
	}
	if( N > 0 ){
		Dao_AboutVars( ctx->nameSpace, p, N, input );
		DaoStream_WriteString( stream, input );
		DaoStream_WriteMBS( stream, "\n" );
		DString_Delete( input );
		return;
	}
	if( handler && handler->StdlibDebug ){
		handler->StdlibDebug( handler, ctx );
		return;
	}
	tokens = DArray_New(D_STRING);
	cycData = DMap_New(0,0);
	while(1){
		if( ctx->vmSpace->ReadLine ){
			chs = ctx->vmSpace->ReadLine( "(debug) " );
			if( chs ){
				DString_SetMBS( input, chs );
				DString_Trim( input );
				if( input->size && ctx->vmSpace->AddHistory )
					ctx->vmSpace->AddHistory( chs );
				dao_free( chs );
			}
		}else{
			DaoStream_WriteMBS( stream, "(debug) " );
			DaoStream_ReadLine( stream, input );
		}
		if( input->size == 0 ) continue;
		SplitByWhiteSpaces( input, tokens );
		if( tokens->size == 0 ) continue;
		cmd = tokens->items.pString[0]->mbs;
		if( strcmp( cmd, "q" ) == 0 || strcmp( cmd, "quit" ) == 0 ){
			break;
		}else if( strcmp( cmd, "k" ) == 0 || strcmp( cmd, "kill" ) == 0 ){
			ctx->process->status = DAO_VMPROC_ABORTED;
			break;
		}else if( strcmp( cmd, "a" ) == 0 || strcmp( cmd, "about" ) == 0 ){
			if( tokens->size > 1 ){
				ushort_t reg = (ushort_t)strtod( tokens->items.pString[1]->mbs, 0 );
				DaoType *tp = ctx->routine->regType->items.pType[ reg ];
				DString_Clear( input );
				Dao_AboutVar( ctx->nameSpace, *ctx->regValues[reg], input );
				DaoStream_WriteMBS( stream, "type: " );
				if( tp )
					DaoStream_WriteString( stream, tp->name );
				else
					DaoStream_WriteMBS( stream, "?" );
				DaoStream_WriteMBS( stream, ", value: " );
				DaoStream_WriteString( stream, input );
				DaoStream_WriteMBS( stream, "\n" );
			}
		}else if( strcmp( cmd, "g" ) == 0 || strcmp( cmd, "goto" ) == 0 ){
			if( tokens->size > 1 ){
				int n = atoi( tokens->items.pString[1]->mbs );
				int entry = ctx->vmc - ctx->routine->vmCodes->codes;
				if( n < 0 ) n = entry - n;
				if( n >= routine->vmCodes->size ) n = routine->vmCodes->size -1;
				ctx->process->topFrame->entry = n;
				ctx->process->status = DAO_VMPROC_STACKED;
				return;
			}
		}else if( strcmp( cmd, "h" ) == 0 || strcmp( cmd, "help" ) == 0 ){
			DaoStream_WriteMBS( stream, help );
		}else if( strcmp( cmd, "l" ) == 0 || strcmp( cmd, "list" ) == 0 ){
			DaoVmCodeX **vmCodes = routine->annotCodes->items.pVmc;
			DString *mbs = DString_New(1);
			int entry = ctx->vmc - ctx->routine->vmCodes->codes;
			int start = entry - 10;
			int end = entry;
			if( tokens->size >1 ){
				int dn = atoi( tokens->items.pString[1]->mbs );
				if( dn < 0 ){
					start = entry + dn;
				}else if( dn > 0 ){
					start = entry;
					end = entry + dn;
				}
			}
			if( start < 0 ) start = 0;
			if( end >= routine->vmCodes->size ) end = routine->vmCodes->size - 1;
			DaoStream_WriteString( stream, routine->routName );
			DaoStream_WriteMBS( stream, "(): " );
			if( routine->routType ) DaoStream_WriteString( stream, routine->routType->name );
			DaoStream_WriteMBS( stream, "\n" );
			DaoStream_WriteMBS( stream, daoRoutineCodeHeader );
			DaoStream_WriteMBS( stream, sep );
			for( i=start; i<=end; i++ ){
				DaoRoutine_FormatCode( routine, i, mbs );
				DaoStream_WriteString( stream, mbs );
			}
			DString_Delete( mbs );
		}else if( strcmp( cmd, "p" ) == 0 || strcmp( cmd, "print" ) == 0 ){
			if( tokens->size > 1 ){
				ushort_t reg = (ushort_t)atoi( tokens->items.pString[1]->mbs );
				DValue_Print( *ctx->regValues[reg], ctx, stream, cycData );
				DaoStream_WriteMBS( stream, "\n" );
			}
		}else if( strcmp( cmd, "t" ) == 0 || strcmp( cmd, "trace" ) == 0 ){
			int depth = 1;
			if( tokens->size >1 ) depth = atoi( tokens->items.pString[1]->mbs );
			DaoVmProcess_Trace( ctx->process, depth );
		}else{
			DaoStream_WriteMBS( stream, "Unknown debugging command.\n" );
		}
	}
	DString_Delete( input );
	DArray_Delete( tokens );
}
static void STD_Error( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_RaiseException( ctx, DAO_ERROR, DString_GetMBS( p[0]->v.s ) );
}
static void STD_Log( DaoContext *ctx, DValue *p[], int N )
{
	DString *info = p[0]->v.s;
	FILE *fout = fopen( "dao.log", "a" );
	DString_ToMBS( info );
	fprintf( fout, "%s\n", info->mbs );
	fclose( fout );
}
static void STD_Gcmax( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutInteger( ctx, DaoGC_Max( -1 ) );
	if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
		if( N == 1 ) DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
		return;
	}
	if( N == 1 ) DaoGC_Max( (int)p[0]->v.i );
}
static void STD_Gcmin( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutInteger( ctx, DaoGC_Min( -1 ) );
	if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
		if( N == 1 ) DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
		return;
	}
	if( N == 1 ) DaoGC_Min( (int)p[0]->v.i );
}
static void STD_ListMeth( DaoContext *ctx, DValue *p[], int N )
{
	DaoTypeBase *typer = DValue_GetTyper( *p[0] );
	DaoFunction **meths;
	DArray *array;
	DMap *hash;
	DNode *it;
	int i, j, methCount;
	if( typer == NULL || typer->priv == NULL ) return;
	array = DArray_New(0);
	hash = typer->priv->values;
	if( hash == NULL ){
		DaoNameSpace_SetupValues( typer->priv->nspace, typer );
		hash = typer->priv->values;
	}
	if( typer->priv->methods == NULL ){
		DaoNameSpace_SetupMethods( typer->priv->nspace, typer );
	}
	if( typer->priv->methods ) DMap_SortMethods( typer->priv->methods, array );
	meths = (DaoFunction**) array->items.pVoid;
	methCount = array->size;
	DaoContext_Print( ctx, "======================================\nConsts, methods of type \"" );
	DaoContext_Print( ctx, typer->name );
	DaoContext_Print( ctx, "\":\n======================================\n" );
	if( typer->priv->values ){
		for(it=DMap_First(hash); it; it=DMap_Next(hash,it)){
			DaoContext_Print( ctx, it->key.pString->mbs );
			DaoContext_Print( ctx, "\n" );
		}
	}
	if( typer->priv->methods ){
		for(i=0; i<methCount; i++ ){
			DaoContext_Print( ctx, meths[i]->routName->mbs );
			DaoContext_Print( ctx, ": " );
			for(j=meths[i]->routName->size; j<20; j++) DaoContext_Print( ctx, " " );
			DaoContext_Print( ctx, meths[i]->routType->name->mbs );
			DaoContext_Print( ctx, "\n" );
		}
	}
	DArray_Delete( array );
}
static void STD_Pack1( DaoContext *ctx, DValue *p[], int N )
{
	DString *str = DaoContext_PutMBString( ctx, "" );
	wchar_t v[2] = { 0, 0 };
	v[0] = (wchar_t) p[0]->v.i;
	if( v[0] >= 0 || v[0] < 256 )
		DString_AppendChar( str, (char)v[0] );
	else
		DString_AppendWCS( str, v );
}
static void STD_Pack2( DaoContext *ctx, DValue *p[], int N )
{
	DString *str = DaoContext_PutMBString( ctx, "" );
	DaoList *list = (DaoList*)p[0]->v.p;
	DValue val;
	int wide = 0;
	int i;
	for( i=0; i<list->items->size; i++ ){
		val = list->items->data[i];
		if( val.t == DAO_INTEGER ){
			if( val.v.i < 0 || val.v.i > 255 ){
				wide = 1;
				break;
			}
		}else if( val.t == DAO_FLOAT ){
			if( val.v.f < 0 || val.v.f > 255 ){
				wide = 1;
				break;
			}
		}else if( val.t == DAO_DOUBLE ){
			if( val.v.d < 0 || val.v.d > 255 ){
				wide = 1;
				break;
			}
		}else{
			DaoContext_RaiseException( ctx, DAO_WARNING, "need list of all numbers" );
			return;
		}
	}
	if( wide ){
		DString_ToWCS( str );
		for( i=0; i<list->items->size; i++ ){
			wchar_t ch = (int)DValue_GetDouble( list->items->data[i] );
			DString_AppendWChar( str, ch );
		}
	}else{
		DString_ToMBS( str );
		for( i=0; i<list->items->size; i++ ){
			char ch = (int)DValue_GetDouble( list->items->data[i] );
			DString_AppendChar( str, ch );
		}
	}
}
extern int DaoToken_Tokenize( DArray *toks, const char *src, int r, int cmt, int nosp );
static void STD_Tokenize( DaoContext *ctx, DValue *p[], int N )
{
	DString *source = p[0]->v.s;
	DaoList *list = DaoContext_PutList( ctx );
	DArray *tokens = DArray_New(D_TOKEN);
	int rc = 0;
	DString_ToMBS( source );
	rc = DaoToken_Tokenize( tokens, source->mbs, 0, 1, 1 );
	if( rc ){
		DString *str = DString_New(1);
		DValue value = daoNullString;
		int i;
		value.v.s = str;
		for(i=0; i<tokens->size; i++){
			DString_Assign( str, tokens->items.pToken[i]->string );
			DVarray_Append( list->items, value );
		}
		DString_Delete( str );
	}
	DArray_Delete( tokens );
}
static void STD_SubType( DaoContext *ctx, DValue *p[], int N )
{
	DaoType *tp1 = DaoNameSpace_GetTypeV( ctx->nameSpace, *p[0] );
	DaoType *tp2 = DaoNameSpace_GetTypeV( ctx->nameSpace, *p[1] );
	DaoContext_PutInteger( ctx, DaoType_MatchTo( tp1, tp2, NULL ) );
}
static void STD_Unpack( DaoContext *ctx, DValue *p[], int N )
{
	DaoList *list = DaoContext_PutList( ctx );
	DString *str = p[0]->v.s;
	int i;
	DValue *data;
	if( str->mbs ){
		DVarray_Resize( list->items, str->size, daoNullValue );
		data = list->items->data;
		for( i=0; i<str->size; i++ ){
			data[i].t = DAO_INTEGER;
			data[i].v.i = (uchar_t)str->mbs[i];
		}
	}else{
		DVarray_Resize( list->items, str->size, daoNullValue );
		data = list->items->data;
		for( i=0; i<str->size; i++ ){
			data[i].t = DAO_INTEGER;
			data[i].v.i = (wchar_t)str->wcs[i];
		}
	}
}
static void STD_Warn( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_RaiseException( ctx, DAO_WARNING, DString_GetMBS( p[0]->v.s ) );
}
static void STD_Version( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutMBString( ctx, DAO_VERSION );
}

static DaoFuncItem stdMeths[]=
{
	{ STD_Compile,   "compile( source :string, replace=0 )" },
	{ STD_Eval,      "eval( source :string, replace=0, stream=io, safe=0 )" },
	{ STD_Load,      "load( file :string, import=1, safe=0 )" },
	{ STD_About,     "about( ... )=>string" },
	{ STD_Callable,  "callable( object )=>int" },
	{ STD_Copy,      "copy( object : @OBJECT ) =>@OBJECT" },
	{ STD_Debug,     "debug( ... )" },
	{ STD_Error,     "error( info :string )" },
	{ STD_Log,       "log( info='' )" },
	{ STD_Gcmax,     "gcmax( limit=0 )=>int" },/*by default, return the current value;*/
	{ STD_Gcmin,     "gcmin( limit=0 )=>int" },
	{ STD_ListMeth,  "listmeth( object )" },
	{ STD_Pack1,     "pack( i :int )=>string" },
	{ STD_Pack2,     "pack( ls :list<int> )=>string" },
	{ STD_Tokenize,  "tokenize( source :string )=>list<string>" },
	{ STD_SubType,   "subtype( obj1, obj2 )=>int" },
	{ STD_Unpack,    "unpack( string :string )=>list<int>" },
	{ STD_Warn,      "warn( info :string )" },
	{ STD_Version,   "version()=>string" },
	{ NULL, NULL }
};

DaoTypeBase libStandardTyper = {
	"std", NULL, NULL, stdMeths, {0}, NULL, NULL
};

static void SYS_Ctime( DaoContext *ctx, DValue *p[], int N )
{
	struct tm *ctime;
	time_t t = (time_t)p[0]->v.i;
	DaoTuple *tuple = DaoTuple_New( 7 );
	DValue val = daoZeroInt;
	if( t == 0 ) t = time(NULL);
	ctime = gmtime( & t );
	val.v.i = ctime->tm_year + 1900;
	DValue_Copy( tuple->items->data, val );
	val.v.i = ctime->tm_mon + 1;
	DValue_Copy( tuple->items->data + 1, val );
	val.v.i = ctime->tm_mday;
	DValue_Copy( tuple->items->data + 2, val );
	val.v.i = ctime->tm_wday + 1;
	DValue_Copy( tuple->items->data + 3, val );
	val.v.i = ctime->tm_hour;
	DValue_Copy( tuple->items->data + 4, val );
	val.v.i = ctime->tm_min;
	DValue_Copy( tuple->items->data + 5, val );
	val.v.i = ctime->tm_sec;
	DValue_Copy( tuple->items->data + 6, val );
	DaoContext_PutResult( ctx, (DaoBase*) tuple );
}
static int addStringFromMap( DValue self, DString *S, DaoMap *sym, const char *key, int id )
{
	DNode *node;

	if( S==NULL || sym==NULL ) return 0;
	DString_SetMBS( self.v.s, key );
	node = DMap_Find( sym->items, & self );
	if( node ){
		DaoList *list = node->value.pValue->v.list;
		if( list->type == DAO_LIST && list->items->size > id ){
			DValue p = list->items->data[ id ];
			if( p.t == DAO_STRING ){
				DString_Append( S, p.v.s );
				return 1;
			}
		}
	}
	return 0;
}
static void SYS_Ctimef( DaoContext *ctx, DValue *p[], int N )
{
	int  i;
	int halfday = 0;
	const int size = DString_Size( p[1]->v.s );
	const char *format = DString_GetMBS( p[1]->v.s );
	char buf[100];
	char *p1 = buf+1;
	char *p2;
	DaoMap *sym = NULL;
	DString *ds;
	DString *S;
	DValue key = daoNullString;

	struct tm *ctime;
	time_t t = (time_t)p[0]->v.i;
	if( t == 0 ) t = time(NULL);
	ctime = gmtime( & t );

	if( N > 1 ){
		sym = (DaoMap*)p[2]->v.p;
		if( sym->items->size == 0 ) sym = NULL;
	}
	S = DaoContext_PutMBString( ctx, "" );

	for( i=0; i+1<size; i++ ){
		if( format[i] == '%' && ( format[i+1] == 'a' || format[i+1] == 'A' ) ){
			halfday = 1;
			break;
		}
	}
	buf[0] = '0'; /* for padding */

	ds = DString_New(1);
	key.v.s = ds;

	for( i=0; i+1<size; i++ ){
		p2 = p1;
		p1[0] = 0;
		if( format[i] == '%' ){
			const char ch = format[i+1];
			switch( ch ){
			case 'Y' :
				sprintf( p1, "%i", ctime->tm_year+1900 );
				break;
			case 'y' :
				sprintf( p1, "%i", ctime->tm_year+1900 );
				p2 += 2;
				break;
			case 'M' :
			case 'm' :
				if( ! addStringFromMap( key, S, sym, "month", ctime->tm_mon ) ){
					sprintf( p1, "%i", ctime->tm_mon+1 );
					if( ch=='M' && p1[1]==0 ) p2 = buf; /* padding 0; */
				}else p2 = NULL;
				break;
			case 'D' :
			case 'd' :
				if( ! addStringFromMap( key, S, sym, "date", ctime->tm_mday ) ){
					sprintf( p1, "%i", ctime->tm_mday );
					if( ch=='D' && p1[1]==0 ) p2 = buf; /* padding 0; */
				}else p2 = NULL;
				break;
			case 'W' :
			case 'w' :
				if( ! addStringFromMap( key, S, sym, "week", ctime->tm_wday ) )
					sprintf( p1, "%i", ctime->tm_wday+1 );
				else p2 = NULL;
				break;
			case 'H' :
			case 'h' :
				if( halfday )
					sprintf( p1, "%i", ctime->tm_hour %12 );
				else
					sprintf( p1, "%i", ctime->tm_hour );
				if( ch=='H' && p1[1]==0 ) p2 = buf; /* padding 0; */
				break;
			case 'I' :
			case 'i' :
				sprintf( p1, "%i", ctime->tm_min );
				if( ch=='I' && p1[1]==0 ) p2 = buf; /* padding 0; */
				break;
			case 'S' :
			case 's' :
				sprintf( p1, "%i", ctime->tm_sec );
				if( ch=='S' && p1[1]==0 ) p2 = buf; /* padding 0; */
				break;
			case 'a' :
				if( ! addStringFromMap( key, S, sym, "halfday", 0 ) ){
					if( ctime->tm_hour >= 12 ) strcpy( p1, "pm" );
					else strcpy( p1, "am" );
				}else p2 = NULL;
				break;
			case 'A' :
				if( ! addStringFromMap( key, S, sym, "halfday", 1 ) ){
					if( ctime->tm_hour >= 12 ) strcpy( p1, "PM" );
					else strcpy( p1, "AM" );
				}else p2 = NULL;
				break;
			default : break;
			}
			if( p2 ) DString_AppendMBS( S, p2 );
			i ++;
		}else{
			DString_AppendChar( S, format[i] );
		}
	}
	if( i+1 == size ) DString_AppendChar( S, format[i] );
	DString_Delete( ds );
}
static void SYS_Sleep( DaoContext *ctx, DValue *p[], int N )
{
#ifdef DAO_WITH_THREAD
	DMutex    mutex;
	DCondVar  condv;
#endif

	double s = p[0]->v.f;
	if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
		return;
	}
	if( s < 0 ){
		DaoContext_RaiseException( ctx, DAO_WARNING_VALUE, "expecting positive value" );
		return;
	}
#ifdef DAO_WITH_THREAD
	/* sleep only the current thread: */
	DMutex_Init( & mutex );
	DCondVar_Init( & condv );
	DMutex_Lock( & mutex );
	DCondVar_TimedWait( & condv, & mutex, s );
	DMutex_Unlock( & mutex );
	DMutex_Destroy( & mutex );
	DCondVar_Destroy( & condv );
#elif UNIX
	sleep( (int)s ); /* This may cause the whole process to sleep. */
#else
	Sleep( s * 1000 );
#endif
}
static void SYS_Exit( DaoContext *ctx, DValue *p[], int N )
{
	if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
		return;
	}
	exit( (int)p[0]->v.i );
}
static void SYS_System( DaoContext *ctx, DValue *p[], int N )
{
	if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
		return;
	}
	DaoContext_PutInteger( ctx, system( DString_GetMBS( p[0]->v.s ) ) );
}
static void SYS_Time( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutInteger( ctx, time( NULL ) );
}
static void SYS_Time2( DaoContext *ctx, DValue *p[], int N )
{
	extern long timezone;
	/* extern int daylight; // not on WIN32 */
	struct tm ctime;
	DValue *tup = p[0]->v.tuple->items->data;
	memset( & ctime, 0, sizeof( struct tm ) );
	ctime.tm_year = tup[0].v.i - 1900;
	ctime.tm_mon = tup[1].v.i - 1;
	ctime.tm_mday = tup[2].v.i;
	ctime.tm_hour = tup[4].v.i;/* + daylight; */
	ctime.tm_min = tup[5].v.i;
	ctime.tm_sec = tup[6].v.i;
	ctime.tm_isdst = 0;
	DaoContext_PutInteger( ctx, (int) mktime( & ctime ) );
}
static void SYS_SetLocale( DaoContext *ctx, DValue *p[], int N )
{
	int category = p[0]->v.i;
	const char *locale = DString_GetMBS(p[1]->v.s);
	char* old = setlocale(category,locale);
	if (old) DaoContext_PutMBString(ctx,old);
}
static void SYS_Clock( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutFloat( ctx, ((float)clock())/CLOCKS_PER_SEC );
}

#ifndef MAX_PATH
#define MAX_PATH 512
#endif

#define MAX_ERRMSG 80

struct DInode
{
	char *path;
	short type;
	time_t ctime;
	time_t mtime;
	short pread;
	short pwrite;
	short pexec;
};

typedef struct DInode DInode;

DInode* DInode_New()
{
	DInode *res = (DInode*)dao_malloc( sizeof(DInode) );
	res->path = NULL;
	res->type = -1;
	return res;
}

void DInode_Close( DInode *self )
{
	if( self->path ){
		dao_free( self->path );
		self->path = NULL;
		self->type = -1;
	}
}

void DInode_Delete( DInode *self )
{
	DInode_Close( self );
	dao_free( self );
}

#ifdef WIN32
#define IS_PATH_SEP( c ) ( ( c ) == '\\' || ( c ) == '/' || ( c ) == ':' )
#define STD_PATH_SEP "\\"
#else
#define IS_PATH_SEP( c ) ( ( c ) == '/' )
#define STD_PATH_SEP "/"
#endif

int DInode_Open( DInode *self, const char *path )
{
	char buf[MAX_PATH + 1] = {0};
	struct stat info;
	int len, i;
	DInode_Close( self );
	if( !path )
		return 1;
	len = strlen( path );
	if( stat( path, &info ) != 0 )
		return errno;
	for( i = 0; i < len; i++ )
		if( path[i] == '.' && ( i == 0 || IS_PATH_SEP( path[i - 1] ) )
			&& ( i == len - 1 || IS_PATH_SEP( path[i + 1] ) ||
			( path[i + 1] == '.' && ( i == len - 2 || IS_PATH_SEP( path[i + 2] ) ) ) ) )
			return -1;
#ifdef WIN32
	if( !( info.st_mode & _S_IFDIR ) && !( info.st_mode & _S_IFREG ) )
		return 1;
	self->pread = info.st_mode & _S_IREAD;
	self->pwrite = info.st_mode & _S_IWRITE;
	self->pexec = info.st_mode & _S_IEXEC;
	self->type = ( info.st_mode & _S_IFDIR )? 0 : 1;
	if( len < 2 || path[1] != ':' ){
		if( !getcwd( buf, MAX_PATH ) )
			return errno;
		strcat( buf, "\\" );
	}
#else
	if( !S_ISDIR( info.st_mode ) && !S_ISREG( info.st_mode ) )
		return 1;
	self->pread = info.st_mode & S_IRUSR;
	self->pwrite = info.st_mode & S_IWUSR;
	self->pexec = info.st_mode & S_IXUSR;
	self->type = ( S_ISDIR( info.st_mode ) )? 0 : 1;
	if( path[0] != '/' ){
		if( !getcwd( buf, MAX_PATH ) )
			return errno;
		strcat( buf, "/" );
	}
#endif
	len += strlen( buf );
	if( len > MAX_PATH )
		return ENAMETOOLONG;
	self->path = (char*)dao_malloc( len + 1 );
	strcpy( self->path, buf );
	strcat( self->path, path );
#ifndef WIN32
	if( self->path[len - 1] == '/' && len > 1 )
		self->path[len - 1] = '\0';
#endif
	self->ctime = info.st_ctime;
	self->mtime = info.st_mtime;
	return 0;
}

char* DInode_Parent( DInode *self, char *buffer )
{
	int i;
	if( !self->path )
		return NULL;
	for (i = strlen( self->path ) - 1; i >= 0; i--)
		if( IS_PATH_SEP( self->path[i] ) )
			break;
	if( self->path[i + 1] == '\0' )
		return NULL;
#ifdef WIN32
	if( self->path[i] == ':' ){
		strncpy( buffer, self->path, i + 1 );
		buffer[i + 1] = '\\';
		buffer[i + 2] = '\0';
	}
	else{
		if( i == 2 )
			i++;
		strncpy( buffer, self->path, i );
		buffer[i] = '\0';
	}
#else
	if( i == 0 )
		strcpy( buffer, "/" );
	else{
		strncpy( buffer, self->path, i );
		buffer[i] = '\0';
	}
#endif
	return buffer;
}

int DInode_Rename( DInode *self, const char *path )
{
	char buf[MAX_PATH + 1] = {0};
	int i, len;
	if( !self->path )
		return 1;
	len = strlen( path );
	for( i = 0; i < len; i++ )
		if( path[i] == '.' && ( i == 0 || IS_PATH_SEP( path[i - 1] ) )
			&& ( i == len - 1 || IS_PATH_SEP( path[i + 1] ) ||
			( path[i + 1] == '.' && ( i == len - 2 || IS_PATH_SEP( path[i + 2] ) ) ) ) )
			return -1;
	if( !DInode_Parent( self, buf ) )
		return 1;
#ifdef WIN32
	if( len < 2 || path[1] != ':' ){
#else
	if( path[0] != '/' ){
#endif
		strcat( buf, STD_PATH_SEP );
		len += strlen( buf );
		if( len > MAX_PATH )
			return ENAMETOOLONG;
		strcat( buf, path );
	}else{
		if( len > MAX_PATH )
			return ENAMETOOLONG;
		strcpy( buf, path );
	}
	if( rename( self->path, buf ) != 0 )
		return errno;
	self->path = (char*)dao_realloc( self->path, len + 1 );
	strcpy( self->path, buf );
	return 0;
}

int DInode_Remove( DInode *self )
{
	if( !self->path )
		return 1;
	if( self->type == 0 ){
		if( rmdir( self->path ) != 0 )
			return errno;
	}else{
		if( unlink( self->path ) != 0 )
			return errno;
	}
	DInode_Close( self );
	return 0;
}

int DInode_Append( DInode *self, const char *path, int dir, DInode *dest )
{
	int i, len;
	char buf[MAX_PATH + 1];
	FILE *handle;
	struct stat info;
	if( !self->path || self->type != 0 || !dest )
		return 1;
	len = strlen( path );
	for( i = 0; i < len; i++ )
		if( path[i] == '.' && ( i == 0 || IS_PATH_SEP( path[i - 1] ) )
			&& ( i == len - 1 || IS_PATH_SEP( path[i + 1] ) ||
			( path[i + 1] == '.' && ( i == len - 2 || IS_PATH_SEP( path[i + 2] ) ) ) ) )
			return -1;
	if( DInode_Parent( self, buf ) ){
		strcpy( buf, self->path );
		strcat( buf, STD_PATH_SEP );
	}
	else
		strcpy( buf, self->path );
	if( strlen( buf ) + len > MAX_PATH )
		return ENAMETOOLONG;
	strcat( buf, path );
#ifdef WIN32
	if( stat( buf, &info ) == 0 && ( ( dir && ( info.st_mode & _S_IFDIR ) )
		|| ( !dir && ( info.st_mode & _S_IFREG ) ) ) )
		return DInode_Open( dest, buf );
#else
	if( stat( buf, &info ) == 0 && ( ( dir && S_ISDIR( info.st_mode ) )
		|| ( !dir && S_ISREG( info.st_mode ) ) ) )
		return DInode_Open( dest, buf );
#endif
	if( !dir ){
		if( !( handle = fopen( buf, "w" ) ) )
			return ( errno == EINVAL ) ? 1 : errno;
		fclose( handle );
	}else{
#ifdef WIN32
	if( mkdir( buf ) != 0 )
		return ( errno == EINVAL ) ? 1 : errno;
#else
	if( mkdir( buf, S_IRWXU|S_IRGRP|S_IXGRP|S_IXOTH ) != 0 )
		return ( errno == EINVAL ) ? 1 : errno;
#endif
	}
	return DInode_Open( dest, buf );
}

extern DaoTypeBase inodeTyper;

int DInode_Children( DInode *self, int type, DVarray *dest, DaoRegex *pattern )
{
	char buffer[MAX_PATH + 1];
	int len, res;
#ifdef WIN32
	intptr_t handle;
	struct _finddata_t finfo;
#else
	DIR *handle;
	struct dirent *finfo;
#endif
	if( !self->path || self->type != 0 )
		return 1;
    strcpy( buffer, self->path );
	len = strlen( buffer );
#ifdef WIN32
	/* Using _findfirst/_findnext for Windows */
	if( IS_PATH_SEP( buffer[len - 1] ) )
    	strcpy( buffer + len, "*" );
    else
		strcpy( buffer + len++, "\\*" );
	handle = _findfirst( buffer, &finfo );
	if (handle != -1){
		DString *str = DString_New( 1 );
		DValue value = daoNullCData;
		DInode *inode;
		do
			if( strcmp( finfo.name, "." ) && strcmp( finfo.name, ".." ) ){
				DString_SetDataMBS( str, finfo.name, strlen(finfo.name) );
				strcpy( buffer + len, finfo.name );
				inode = DInode_New();
				if( ( res = DInode_Open( inode, buffer ) ) != 0 ){
					DInode_Delete( inode );
					return res;
				}
				if( ( inode->type == type || type == 2 ) && DaoRegex_Match( pattern, str, NULL, NULL ) ){
					value.v.cdata = DaoCData_New( &inodeTyper, inode );
					DVarray_Append( dest, value );
				}
				else
					DInode_Delete( inode );
			}
		while( !_findnext( handle, &finfo ) );
		DString_Delete( str );
		_findclose( handle );
	}
#else
	/* Using POSIX opendir/readdir otherwise */
	handle = opendir( buffer );
	if( !IS_PATH_SEP( buffer[len - 1] ) )
		strcpy( buffer + len++,  "/");
	if( handle ){
		DString *str = DString_New( 1 );
		DValue value = daoNullCData;
		DInode *inode;
		while( ( finfo = readdir( handle ) ) )
			if( strcmp( finfo->d_name, "." ) && strcmp( finfo->d_name, ".." ) ){
				DString_SetDataMBS( str, finfo->d_name, strlen(finfo->d_name) );
				strcpy( buffer + len, finfo->d_name );
				inode = DInode_New();
				if( ( res = DInode_Open( inode, buffer ) ) != 0 ){
					DInode_Delete( inode );
					return res;
				}
				if( ( inode->type == type || type == 2 ) && DaoRegex_Match( pattern, str, NULL, NULL ) ){
					value.v.cdata = DaoCData_New( &inodeTyper, inode );
					DVarray_Append( dest, value );
				}
				else
					DInode_Delete( inode );
			}
		DString_Delete( str );
		closedir( handle );
	}
#endif
	else
		return errno;
	return 0;
}

static void GetErrorMessage( char *buffer, int code, int special )
{
	switch ( code ){
	case EACCES:
	case EBADF:
		strcpy( buffer, "Access permission required (EACCES/EBADF)");
		break;
	case EBUSY:
		strcpy (buffer, "The inode's path is being used (EBUSY)" );
		break;
	case ENOTEMPTY:
	case EEXIST:
		strcpy( buffer, special? "The directory is not empty (ENOTEMPTY/EEXIST)" : "The inode already exists (EEXIST/ENOTEMPTY)" );
		break;
	case EPERM:
	case ENOTDIR:
	case EISDIR:
		strcat( buffer, "Inconsistent type of the file object(s) (EPERM/ENOTDIR/EISDIR)" );
		break;
	case EINVAL:
		strcpy( buffer, special? "The inode's path doesn't exist (EINVAL)" : "Trying to make the directory it's own subdirectory (EINVAL)" );
		break;
	case EMLINK:
		strcat( buffer, "Trying to create too many entries in the parent directory (EMLINK)" );
		break;
	case ENOENT:
		strcpy( buffer, "The inode's path doesn't exist (ENOENT)" );
		break;
	case ENOSPC:
		strcpy( buffer, "No space for a new entry in the file system (ENOSPC)" );
		break;
	case EROFS:
		strcpy( buffer, "Trying to write to a read-only file system (EROFS)" );
		break;
	case EXDEV:
		strcpy( buffer, "Trying to relocate the inode to a different file system (EXDEV)" );
		break;
	case ENAMETOOLONG:
		strcpy( buffer, "The inode's path is too long (ENAMETOOLONG)" );
		break;
	case EMFILE:
	case ENFILE:
		strcpy( buffer, "Too many inodes open (EMFILE/ENFILE)" );
		break;
	case ENOMEM:
		strcpy( buffer, "Not enough memory (ENOMEM)" );
		break;
	default:
		sprintf( buffer, "Unknown system error (%x)", code );
	}
}

static void DaoInode_Open( DaoContext *ctx, DValue *p[], int N )
{
	char errbuf[MAX_ERRMSG];
	DInode *self = (DInode*)p[0]->v.cdata->data;
	int res;
	if( ( res = DInode_Open( self, DString_GetMBS( p[1]->v.s ) ) ) != 0 ){
		if( res == 1 )
			strcpy( errbuf, "Trying to open something which is not a file/directory" );
		else if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoContext_RaiseException( ctx, DAO_ERROR, errbuf );
		return;
	}
}

static void DaoInode_Isopen( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutInteger( ctx, ((DInode*)p[0]->v.cdata->data)->path ? 1 : 0 );
}

static void DaoInode_Close( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	DInode_Close( self );
}

static void DaoInode_Path( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	DaoContext_PutMBString( ctx, self->path );
}

static void DaoInode_Name( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	int i;
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	for (i = strlen( self->path ) - 1; i >= 0; i--)
		if( IS_PATH_SEP( self->path[i] ) )
			break;
	if( self->path[i + 1] == '\0' )
		DaoContext_PutMBString( ctx, self->path );
	else
		DaoContext_PutMBString( ctx, self->path + i + 1 );
}

static void DaoInode_Parent( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	DInode *par;
	char path[MAX_PATH + 1];
	int res = 0;
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	par = DInode_New();
	if( !DInode_Parent( self, path ) || ( res = DInode_Open( par, path ) ) != 0 ){
		DInode_Delete( par );
		if( res == 0 )
			strcpy( path, "The inode has no parent" );
		else
			GetErrorMessage( path, res, 0 );
		DaoContext_RaiseException( ctx, DAO_ERROR, path );
		return;
	}
	DaoContext_PutCData( ctx, (void*)par, &inodeTyper );
}

static void DaoInode_Type( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	DaoContext_PutEnum( ctx, self->type == 1 ? "file" : "dir" );
}

static void DaoInode_Rename( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	int res;
	char errbuf[MAX_ERRMSG];
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	if ( (res = DInode_Rename( self, DString_GetMBS( p[1]->v.s ) ) ) != 0 ){
		if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else if( res == 1 )
			strcpy( errbuf, "Trying to rename root inode" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoContext_RaiseException( ctx, DAO_ERROR, errbuf );
		return;
	}
}

static void DaoInode_Remove( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	int res;
	char errbuf[MAX_ERRMSG];
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	if ( (res = DInode_Remove( self ) ) != 0 ){
		GetErrorMessage( errbuf, res, self->type == 0 );
		DaoContext_RaiseException( ctx, DAO_ERROR, errbuf );
		return;
	}
}

static void DaoInode_Ctime( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	DaoContext_PutInteger( ctx, self->ctime );
}

static void DaoInode_Mtime( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	DaoContext_PutInteger( ctx, self->mtime );
}

static void DaoInode_Access( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	char res[20] = {0};
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	if( self->pread )
		strcat( res, "$read" );
	if( self->pwrite )
		strcat( res, "$write" );
	if( self->pexec )
		strcat( res, "$execute" );
	DaoContext_PutEnum( ctx, res );
}

static void DaoInode_Makefile( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	DInode *child;
	char errbuf[MAX_ERRMSG];
	int res;
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	if( self->type != 0 ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not a directory" );
		return;
	}
	child = DInode_New();
	if( ( res = DInode_Append( self, DString_GetMBS( p[1]->v.s ), 0, child ) ) != 0 ){
		DInode_Delete( child );
		if( res == 1 )
			strcpy( errbuf, "The inode's name is invalid (EINVAL)" );
		else if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoContext_RaiseException( ctx, DAO_ERROR, errbuf );
		return;
	}
	DaoContext_PutCData( ctx, (void*)child, &inodeTyper );
}

static void DaoInode_Makedir( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	DInode *child;
	char errbuf[MAX_ERRMSG];
	int res;
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	if( self->type != 0 ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not a directory" );
		return;
	}
	child = DInode_New();
	if( ( res = DInode_Append( self, DString_GetMBS( p[1]->v.s ), 1, child ) ) != 0 ){
		DInode_Delete( child );
		if( res == 1 )
			strcpy( errbuf, "The inode's name is invalid (EINVAL)" );
		else if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoContext_RaiseException( ctx, DAO_ERROR, errbuf );
		return;
	}
	DaoContext_PutCData( ctx, (void*)child, &inodeTyper );
}

static void DaoInode_Isroot( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	if( self->path == NULL ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	DaoContext_PutInteger( ctx, IS_PATH_SEP( self->path[strlen( self->path ) - 1] ) ? 1 : 0 );
}

static void DaoInode_Child( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	DInode *child;
	char path[MAX_PATH + 1], *str;
	int res;
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	if( self->type != 0 ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not a directory" );
		return;
	}
	child = DInode_New();
	strcpy( path, self->path );
	strcat( path, STD_PATH_SEP );
	str = DString_GetMBS( p[1]->v.s );
	if( strlen( path ) + strlen( str ) > MAX_PATH ){
		GetErrorMessage( path, ENAMETOOLONG, 0 );
		DaoContext_RaiseException( ctx, DAO_ERROR, path );
		return;
	}
	strcat( path, str );
	if( ( res = DInode_Open( child, path ) ) != 0 ){
		DInode_Delete( child );
		if( res == 1 )
			strcpy( path, "The inode is not a directory" );
		else if( res == -1 )
			strcpy( path, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( path, res, 0 );
		DaoContext_RaiseException( ctx, DAO_ERROR, path );
		return;                                  
	}
	DaoContext_PutCData( ctx, (void*)child, &inodeTyper );
}

static void DaoInode_Children( DaoContext *ctx, DValue *p[], int N )
{
	DInode *self = (DInode*)p[0]->v.cdata->data;
	DaoList *list = DaoContext_PutList( ctx );
	char buffer[MAX_PATH + 1], *filter;
	int res, type = p[1]->v.e->value, i, j, len;
	DString strbuf;
	DaoRegex *pattern;
	if( !self->path ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	if( self->type != 0 ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not a directory" );
		return;
	}
	filter = DString_GetMBS( p[2]->v.s );
	len = strlen( filter );
	if( len > MAX_PATH ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The filter is too large" );
		return;
	}
	if( !p[3]->v.e->value == 1 ){
		buffer[0] = '^';
		buffer[1] = '(';
		for( i = 0, j = 2; i < len && j < MAX_PATH - 1; i++, j++ )
			switch( filter[i] )
			{
			case '?':
				buffer[j] = '.';
				break;
			case '*':
				buffer[j++] = '.';
				buffer[j] = '*';
				break;
			case ';':
				buffer[j] = '|';
				break;
			case '{':
			case '}':
			case '(':
			case ')':
			case '%':
			case '.':
			case '|':
			case '$':
			case '^':
			case '[':
			case ']':
			case '+':
			case '-':
			case '<':
			case '>':
				buffer[j++] = '%';
				buffer[j] = filter[i];
				break;
			default:
				buffer[j] = filter[i];
			}
		if( j >= MAX_PATH - 1 ){
			DaoContext_RaiseException( ctx, DAO_ERROR, "The filter is too large" );
			return;
		}
		buffer[j] = ')';
		buffer[j + 1] = '$';
		buffer[j + 2] = '\0';
	}
	else
		strcpy( buffer, filter );
	strbuf = DString_WrapMBS( buffer );
    pattern = DaoVmProcess_MakeRegex( ctx, &strbuf, 1 );
    if( !pattern )
    	return;
	type = ( type == 3 ) ? 2 : ( ( type == 1 ) ? 1 : 0 );
	if( ( res = DInode_Children( self, type, list->items, pattern ) ) != 0 ){
		GetErrorMessage( buffer, res, 1 );
		DaoContext_RaiseException( ctx, DAO_ERROR, buffer );
		return;
	}
}

static void DaoInode_Files( DaoContext *ctx, DValue *p[], int N )
{
	DValue value = daoNullEnum;
	DValue *params[] = {p[0], &value, p[1], p[2]};
	value.v.e = DEnum_New( NULL, 1 );
	DaoInode_Children( ctx, params, N + 1 );
	DEnum_Delete( value.v.e );
}

static void DaoInode_Dirs( DaoContext *ctx, DValue *p[], int N )
{
	DValue value = daoNullEnum;
	DValue *params[] = {p[0], &value, p[1], p[2]};
	value.v.e = DEnum_New( NULL, 2 );
	DaoInode_Children( ctx, params, N + 1 );
	DEnum_Delete( value.v.e );
}

static DaoFuncItem inodeMeths[] =
{
	{ DaoInode_Open,     "open( self : inode, path : string )" },
	{ DaoInode_Isopen,   "isopen( self : inode )const=>int" },
	{ DaoInode_Close,    "close( self : inode )" },
	{ DaoInode_Path,     "path( self : inode )const=>string" },
	{ DaoInode_Name,     "name( self : inode )const=>string" },
	{ DaoInode_Type,     "type( self : inode )const=>enum<file, dir>" },
	{ DaoInode_Parent,   "parent( self : inode )const=>inode" },
	{ DaoInode_Ctime,    "ctime( self : inode )const=>int" },
	{ DaoInode_Mtime,    "mtime( self : inode )const=>int" },
	{ DaoInode_Access,   "access( self : inode )const=>enum<read; write; execute>" },
	{ DaoInode_Rename,   "rename( self : inode, path : string )" },
	{ DaoInode_Remove,   "remove( self : inode )" },
	{ DaoInode_Makefile, "makefile( self : inode, path : string )=>inode" },
	{ DaoInode_Makedir,  "makedir( self : inode, path : string )=>inode" },
	{ DaoInode_Isroot,   "isroot( self : inode )const=>int" },
	{ DaoInode_Children, "children( self : inode, type : enum<files; dirs>, filter='*', filtering : enum<wildcard, regex> = $wildcard )const=>list<inode>" },
	{ DaoInode_Files,    "files( self : inode, filter='*', filtering : enum<wildcard, regex> = $wildcard )const=>list<inode>" },
	{ DaoInode_Dirs,     "dirs( self : inode, filter='*', filtering : enum<wildcard, regex> = $wildcard )const=>list<inode>" },
	{ DaoInode_Child,    "[]( self : inode, path : string )const=>inode" },
	{ NULL, NULL }
};

static void SYS_Inode( DaoContext *ctx, DValue *p[], int N )
{
	char errbuf[MAX_ERRMSG];
	DInode *inode = DInode_New();
	int res;
	if( ( res = DInode_Open( inode, DString_GetMBS( p[0]->v.s ) ) ) != 0 ){
		if( res == 1 )
			strcpy( errbuf, "Trying to open something which is not a file/directory" );
		else if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoContext_RaiseException( ctx, DAO_ERROR, errbuf );
		return;
	}
	DaoContext_PutCData( ctx, (void*)inode, &inodeTyper );
}

static void SYS_GetCWD( DaoContext *ctx, DValue *p[], int N )
{
	char buf[MAX_PATH + 1];
	int res = 0;
	DInode *inode = DInode_New();
	if( !getcwd( buf, MAX_PATH ) || ( res = DInode_Open( inode, buf ) ) != 0 ){
		DInode_Delete( inode );
		GetErrorMessage( buf, ( res == 0 )? errno : res, 0 );
		DaoContext_RaiseException( ctx, DAO_ERROR, buf );
		return;
	}
	DaoContext_PutCData( ctx, (void*)inode, &inodeTyper );
}

static void SYS_SetCWD( DaoContext *ctx, DValue *p[], int N )
{
	DInode *inode = (DInode*)p[0]->v.cdata->data;
	char errbuf[MAX_PATH + 1];
	if( inode->path == NULL ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not opened" );
		return;
	}
	if( inode->type != 0 ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "The inode is not a directory" );
		return;
	}
	if( chdir( inode->path ) != 0 ){
		GetErrorMessage( errbuf, errno, 0 );
		DaoContext_RaiseException( ctx, DAO_ERROR, errbuf );
		return;
	}
}

DaoTypeBase inodeTyper = {
	"inode", NULL, NULL, inodeMeths, {NULL}, (FuncPtrDel)DInode_Delete, NULL
};

#undef IS_PATH_SEP
#undef MAX_ERRMSG

#ifdef _MSC_VER
#undef chdir
#undef rmdir
#undef getcwd
#undef mkdir
#undef stat
#endif

static DaoFuncItem sysMeths[]=
{
	{ SYS_Ctime,     "ctime( time=0 )=>tuple<year:int,month:int,day:int,wday:int,hour:int,minute:int,second:int>" },
	{ SYS_Ctimef,    "ctimef( time=0, format=\'%Y-%M-%D, %H:%I:%S\', "
		"namemap : map<string,list<string>> = {=>} )=>string" },
	{ SYS_Exit,      "exit( code=0 )" },
	{ SYS_Sleep,     "sleep( seconds :float )" },
	{ SYS_System,    "system( command :string )" },
	{ SYS_Time,      "time(  )=>int" },
	{ SYS_Time2,     "time( tm : tuple<year:int,month:int,day:int,wday:int,hour:int,minute:int,second:int> )=>int" },
	{ SYS_SetLocale, "setlocale(category:int=0,locale:string='')=>string" },
	{ SYS_Clock,     "clock()=>float" },
	{ SYS_Inode,     "inode( path : string )=>inode" },
	{ SYS_GetCWD,    "getcwd(  )=>inode" },
	{ SYS_SetCWD,    "setcwd( dir : inode )" },
	{ NULL, NULL }
};
static DaoNumItem sysConsts[] =
{
	{ "LC_ALL", DAO_INTEGER, LC_ALL } ,
	{ "LC_COLLATE", DAO_INTEGER, LC_COLLATE } ,
	{ "LC_CTYPE", DAO_INTEGER, LC_CTYPE } ,
	{ "LC_MONETARY", DAO_INTEGER, LC_MONETARY } ,
	{ "LC_NUMERIC", DAO_INTEGER, LC_NUMERIC } ,
	{ "LC_TIME", DAO_INTEGER, LC_TIME } ,
	{ NULL, 0, 0 }
};

DaoTypeBase libSystemTyper = {
	"sys", NULL, sysConsts, sysMeths, {0}, NULL, NULL
};

DaoVmProcess* DaoVmProcess_Create( DaoContext *ctx, DValue *par[], int N )
{
	DaoContext *coCtx = NULL;
	DaoVmProcess *vmProc;
	DValue val = *par[0];
	int newed = 0;
	if( val.t == DAO_STRING ) val = DaoNameSpace_GetData( ctx->nameSpace, val.v.s );

	if( val.t == DAO_ROUTINE ){
		newed = 1;
		coCtx = DaoContext_New();
		coCtx->vmSpace = ctx->vmSpace;
		DaoContext_Init( coCtx, val.v.routine );
		/* temporary, for exception handling. */
		coCtx->process = ctx->process;
	}else if( val.t == DAO_CONTEXT ){
		coCtx = val.v.context;
	}else{
		DaoContext_SetResult( ctx, NULL );
		DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "not matched" );
		return NULL;
	}
	DaoContext_InitWithParams( coCtx, ctx->process, par+1, N-1 );
	if( ctx->process->exceptions->size > 0 ){
		if( newed ) DaoContext_Delete( coCtx );
		DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "need more parameters." );
		return NULL;
	}
	if( ! DRoutine_PassParams( (DRoutine*)coCtx->routine, NULL, coCtx->regValues, par+1, NULL, N-1, DVM_CALL ) ){
		DaoContext_SetResult( ctx, NULL );
		if( newed ) DaoContext_Delete( coCtx );
		DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "not matched" );
		return NULL;
	}
	vmProc = DaoVmProcess_New( ctx->vmSpace );
	DaoVmProcess_PushContext( vmProc, coCtx );
	vmProc->status = DAO_VMPROC_SUSPENDED;
	return vmProc;
}

/**/
static void REFL_NS( DaoContext *ctx, DValue *p[], int N )
{
	DValue res = { DAO_NAMESPACE, 0, 0, 0, {0} };
	if( N == 0 ){
		res.v.ns = ctx->nameSpace;
	}else if( p[0]->t == DAO_CLASS ){
		res.v.ns = p[0]->v.klass->classRoutine->nameSpace;
	}else if( p[0]->t == DAO_ROUTINE || p[0]->t == DAO_FUNCTION ){
		res.v.ns = p[0]->v.routine->nameSpace;
	}else{
		res.v.ns = ctx->nameSpace;
	}
	GC_IncRC( res.v.ns );
	DaoContext_PutValue( ctx, res );
}
static void REFL_Name( DaoContext *ctx, DValue *p[], int N )
{
	DString *str = DaoContext_PutMBString( ctx, "" );
	switch( p[0]->t ){
	case DAO_ROUTINE :
	case DAO_FUNCTION :
		DString_Assign( str, p[0]->v.routine->routName );
		break;
	case DAO_CLASS :
		DString_Assign( str, p[0]->v.klass->className );
		break;
	case DAO_TYPE :
		DString_Assign( str, ((DaoType*)p[0]->v.p)->name );
		break;
	default : break;
	}
}
static void REFL_Base( DaoContext *ctx, DValue *p[], int N )
{
	DaoList *ls = DaoContext_PutList( ctx );
	DValue value = daoNullValue;
	int i;
	value.t = p[0]->t;
	if( p[0]->t == DAO_CLASS ){
		DaoClass *k = p[0]->v.klass;
		for( i=0; i<k->superClass->size; i++ ){
			value.v.p = k->superClass->items.pBase[i];
			DaoList_Append( ls, value );
		}
	}else if( p[0]->t == DAO_OBJECT ){
		DaoObject *k = p[0]->v.object;
		for( i=0; i<k->superObject->size; i++ ){
			value.v.p = k->superObject->items.pBase[i];
			DaoList_Append( ls, value );
		}
	}
}
static void REFL_Type( DaoContext *ctx, DValue *p[], int N )
{
	DaoType *tp = DaoNameSpace_GetTypeV( ctx->nameSpace, *p[0] );
	DaoContext_PutResult( ctx, (DaoBase*) tp );
}

static void REFL_Cst1( DaoContext *ctx, DValue *p[], int N )
{
	DaoMap *map = DaoContext_PutMap( ctx );
	DaoTuple *tuple;
	DaoClass *klass;
	DaoObject *object;
	DaoType *tp = map->unitype->nested->items.pType[1];
	DaoNameSpace *ns, *here = ctx->nameSpace;
	DMap *index = NULL, *lookup = NULL;
	DVarray *data;
	DNode *node, *node2;
	DValue value;
	DValue name = daoNullString;
	DValue vtup = daoNullTuple;
	DValue vabtp = daoNullValue;
	int restri = p[1]->v.i;
	name.v.s = DString_New(1);
	vabtp.t = DAO_TYPE;
	if( p[0]->t == DAO_CLASS || p[0]->t == DAO_OBJECT ){
		klass = p[0]->v.klass;
		if( p[0]->t == DAO_OBJECT ){
			object = p[0]->v.object;
			klass = object->myClass;
		}
		lookup = klass->lookupTable;
		index = klass->lookupTable;
		data = klass->cstData;
	}else if( p[0]->t == DAO_NAMESPACE ){
		ns = p[0]->v.ns;
		//index = ns->cstIndex; XXX
		data = ns->cstData;
	}else{
		DaoContext_RaiseException( ctx, DAO_ERROR, "invalid parameter" );
		DString_Delete( name.v.s );
		return;
	}
	if( index == NULL ) return;
	node = DMap_First( index );
	for( ; node != NULL; node = DMap_Next( index, node ) ){
		size_t id = node->value.pSize;
		if( restri && lookup && LOOKUP_PM( id ) != DAO_DATA_PUBLIC ) continue;
		if( lookup ) id = LOOKUP_ID( id );
		tuple = DaoTuple_New( 2 );
		tuple->unitype = tp;
		GC_IncRC( tp );
		value = data->data[ id ];
		vabtp.t = DAO_TYPE;
		vabtp.v.p = (DaoBase*) DaoNameSpace_GetTypeV( here, value );
		if( vabtp.v.p == NULL ) vabtp.t = 0;
		DValue_Copy( tuple->items->data, value );
		DValue_Copy( tuple->items->data + 1, vabtp );
		DString_Assign( name.v.s, node->key.pString );
		vtup.v.tuple = tuple;
		DaoMap_Insert( map, name, vtup );
	}
	DString_Delete( name.v.s );
}
static void REFL_Var1( DaoContext *ctx, DValue *p[], int N )
{
	DaoMap *map = DaoContext_PutMap( ctx );
	DaoTuple *tuple;
	DaoClass *klass = NULL;
	DaoObject *object = NULL;
	DaoType *tp = map->unitype->nested->items.pType[1];
	DaoNameSpace *ns = NULL;
	DMap *index = NULL, *lookup = NULL;
	DNode *node;
	DValue value;
	DValue name = daoNullString;
	DValue vtup = daoNullTuple;
	DValue vabtp = daoNullValue;
	int restri = p[1]->v.i;
	name.v.s = DString_New(1);
	vabtp.t = DAO_TYPE;
	if( p[0]->t == DAO_CLASS || p[0]->t == DAO_OBJECT ){
		klass = p[0]->v.klass;
		if( p[0]->t == DAO_OBJECT ){
			object = p[0]->v.object;
			klass = object->myClass;
		}
		lookup = klass->lookupTable;
		index = klass->lookupTable;
	}else if( p[0]->t == DAO_NAMESPACE ){
		ns = p[0]->v.ns;
		//index = ns->varIndex; XXX
	}else{
		DaoContext_RaiseException( ctx, DAO_ERROR, "invalid parameter" );
		DString_Delete( name.v.s );
		return;
	}
	if( index == NULL ) return;
	node = DMap_First( index );
	for( ; node != NULL; node = DMap_Next( index, node ) ){
		size_t st = 0, id = node->value.pSize;
		if( restri && lookup && LOOKUP_PM( id ) != DAO_DATA_PUBLIC ) continue;
		if( lookup ){
			st = LOOKUP_ST( id );
			id = LOOKUP_ID( id );
			if( st == DAO_CLASS_CONSTANT ) continue;
		}
		tuple = DaoTuple_New( 2 );
		tuple->unitype = tp;
		GC_IncRC( tp );
		value = daoNullValue;
		vabtp.t = DAO_TYPE;
		if( lookup ){
			if( st == DAO_OBJECT_VARIABLE && object ){
				value = object->objValues[id];
				vabtp.v.p = klass->objDataType->items.pBase[ id ];
			}else if( st == DAO_CLASS_VARIABLE ){
				value = klass->glbData->data[id];
				vabtp.v.p = klass->glbDataType->items.pBase[ id ];
			}else if( st == DAO_OBJECT_VARIABLE ){
				vabtp.v.p = klass->objDataType->items.pBase[ id ];
			}
		}else{
			value = ns->varData->data[id];
			vabtp.v.p = ns->varType->items.pBase[ id ];
		}
		if( vabtp.v.p == NULL ) vabtp.t = 0;
		DValue_Copy( tuple->items->data, value );
		DValue_Copy( tuple->items->data + 1, vabtp );
		DString_Assign( name.v.s, node->key.pString );
		vtup.v.tuple = tuple;
		DaoMap_Insert( map, name, vtup );
	}
	DString_Delete( name.v.s );
}
static void REFL_Cst2( DaoContext *ctx, DValue *p[], int N )
{
	DaoTuple *tuple = DaoTuple_New( 2 );
	DaoNameSpace *ns = ctx->nameSpace;
	DaoClass *klass = NULL;
	DNode *node;
	DString *name = p[1]->v.s;
	DValue temp = daoNullValue;
	DValue type = daoNullValue;
	DValue *value = & temp;
	if( p[0]->t == DAO_CLASS || p[0]->t == DAO_OBJECT ){
		klass = p[0]->v.klass;
		if( p[0]->t == DAO_OBJECT ) klass = p[0]->v.object->myClass;
		node = DMap_Find( klass->lookupTable, name );
		if( node && LOOKUP_ST( node->value.pSize ) == DAO_CLASS_CONSTANT ){
			value = klass->cstData->data + LOOKUP_ID( node->value.pSize );
			type.v.p = (DaoBase*) DaoNameSpace_GetTypeV( ns, *value );
		}
	}else if( p[0]->t == DAO_NAMESPACE ){
		DaoNameSpace *ns2 = p[0]->v.ns;
		return; //XXX
		//node = DMap_Find( ns2->cstIndex, name );
		if( node ){
			value = ns2->cstData->data + node->value.pInt;
			type.v.p = (DaoBase*) DaoNameSpace_GetTypeV( ns, *value );
		}
	}else{
		DaoContext_RaiseException( ctx, DAO_ERROR, "invalid parameter" );
	}
	if( type.v.p ) type.t = DAO_TYPE;
	DValue_Copy( tuple->items->data, *value );
	DValue_Copy( tuple->items->data + 1, type );
	DaoContext_PutResult( ctx, (DaoBase*) tuple );
	if( N >2 ){
		DaoType *tp = DaoNameSpace_GetTypeV( ns, *p[2] );
		if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
			DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
			return;
		}
		if( type.v.p ){
			if( DaoType_MatchTo( tp, (DaoType*) type.v.p, NULL ) ==0 ){
				DaoContext_RaiseException( ctx, DAO_ERROR, "type not matched" );
				return;
			}
		}
		DValue_Copy( value, *p[2] );
	}
}
static void REFL_Var2( DaoContext *ctx, DValue *p[], int N )
{
	DaoTuple *tuple = DaoTuple_New( 2 );
	DaoNameSpace *ns = ctx->nameSpace;
	DaoClass *klass = NULL;
	DNode *node;
	DString *name = p[1]->v.s;
	DValue temp = daoNullValue;
	DValue type = daoNullValue;
	DValue *value = & temp;
	if( p[0]->t == DAO_CLASS || p[0]->t == DAO_OBJECT ){
		klass = p[0]->v.klass;
		if( p[0]->t == DAO_OBJECT ) klass = p[0]->v.object->myClass;
		node = DMap_Find( klass->lookupTable, name );
		if( node && LOOKUP_ST( node->value.pSize ) == DAO_CLASS_VARIABLE ){
			value = klass->cstData->data + LOOKUP_ID( node->value.pSize );
			type.v.p = klass->glbDataType->items.pBase[ LOOKUP_ID( node->value.pSize ) ];
		}else if( node && LOOKUP_ST( node->value.pSize ) == DAO_OBJECT_VARIABLE ){
			value = p[0]->v.object->objData->data + LOOKUP_ID( node->value.pSize );
			type.v.p = klass->objDataType->items.pBase[ LOOKUP_ID( node->value.pSize ) ];
		}
	}else if( p[0]->t == DAO_NAMESPACE ){
		DaoNameSpace *ns2 = p[0]->v.ns;
		return; //XXX
		//node = DMap_Find( ns2->varIndex, name );
		if( node ){
			value = ns2->varData->data + node->value.pInt;
			type.v.p = ns2->varType->items.pBase[ node->value.pInt ];
		}
	}else{
		DaoContext_RaiseException( ctx, DAO_ERROR, "invalid parameter" );
	}
	if( type.v.p ) type.t = DAO_TYPE;
	DValue_Copy( tuple->items->data, *value );
	DValue_Copy( tuple->items->data + 1, type );
	DaoContext_PutResult( ctx, (DaoBase*) tuple );
	if( N >2 ){
		DaoType *tp = DaoNameSpace_GetTypeV( ns, *p[2] );
		if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
			DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
			return;
		}
		if( type.v.p ){
			if( DaoType_MatchTo( tp, (DaoType*) type.v.p, NULL ) ==0 ){
				DaoContext_RaiseException( ctx, DAO_ERROR, "type not matched" );
				return;
			}
		}
		DValue_Copy( value, *p[2] );
	}
}
static void REFL_Routine( DaoContext *ctx, DValue *p[], int N )
{
	DaoList *list;
	DValue item = daoNullValue;
	int i;
	if( N ==1 ){
		DRoutine *rout = (DRoutine*) p[0]->v.p;
		list = DaoContext_PutList( ctx );
		if( p[0]->t != DAO_ROUTINE && p[0]->t != DAO_FUNCTION ){
			DaoContext_RaiseException( ctx, DAO_ERROR, "invalid parameter" );
			return;
		}
		for(i=0; i<rout->routTable->size; i++){
			item.v.p = rout->routTable->items.pBase[i];
			item.t = item.v.p->type;
			DaoList_Append( list, item );
		}
	}else{
		DaoContext_PutResult( ctx, (DaoBase*) ctx->routine );
	}
}
static void REFL_Class( DaoContext *ctx, DValue *p[], int N )
{
	if( p[0]->t == DAO_ROUTINE && p[0]->v.routine->tidHost == DAO_OBJECT ){
		DaoContext_SetResult( ctx, (DaoBase*) p[0]->v.routine->routHost->X.klass );
	}else if( p[0]->t == DAO_OBJECT ){
		DaoContext_SetResult( ctx, (DaoBase*) p[0]->v.object->myClass );
	}
	DaoContext_PutValue( ctx, daoNullValue );
}
static void REFL_Isa( DaoContext *ctx, DValue *p[], int N )
{
	DaoNameSpace *ns = ctx->nameSpace;
	dint *res = DaoContext_PutInteger( ctx, 0 );
	if( p[1]->t == DAO_TYPE ){
		DaoType *tp = (DaoType*) p[1]->v.p;
		if( tp && DaoType_MatchValue( tp, *p[0], NULL ) ) *res = 1;
	}else if( p[1]->t == DAO_CLASS ){
		if( p[0]->t != DAO_OBJECT ) return;
		*res = DaoClass_ChildOf( p[0]->v.object->that->myClass, p[1]->v.p );
	}else if( p[1]->t == DAO_CDATA ){
		if( p[0]->t == DAO_OBJECT )
			*res = DaoClass_ChildOf( p[0]->v.object->that->myClass, p[1]->v.p );
		else if( p[0]->t == DAO_CDATA )
			*res = DaoCData_ChildOf( p[0]->v.cdata->typer, p[1]->v.cdata->typer );
	}else if( p[1]->t == DAO_STRING ){
		DString *tname = p[1]->v.s;
		DString_ToMBS( tname );
		if( strcmp( tname->mbs, "class" ) ==0 ){
			if( p[0]->t == DAO_CLASS && p[0]->v.p ) *res = 1;
		}else if( strcmp( tname->mbs, "object" ) ==0 ){
			if( p[0]->t == DAO_OBJECT && p[0]->v.p ) *res = 1;
		}else if( strcmp( tname->mbs, "routine" ) ==0 ){
			if( p[0]->t == DAO_ROUTINE && p[0]->v.p ) *res = 1;
		}else if( strcmp( tname->mbs, "function" ) ==0 ){
			if( p[0]->t == DAO_FUNCTION && p[0]->v.p ) *res = 1;
		}else if( strcmp( tname->mbs, "namespace" ) ==0 ){
			if( p[0]->t == DAO_NAMESPACE && p[0]->v.p ) *res = 1;
		}else if( strcmp( tname->mbs, "tuple" ) ==0 ){
			if( p[0]->t == DAO_TUPLE && p[0]->v.p ) *res = 1;
		}else if( strcmp( tname->mbs, "list" ) ==0 ){
			if( p[0]->t == DAO_LIST && p[0]->v.p ) *res = 1;
		}else if( strcmp( tname->mbs, "map" ) ==0 ){
			if( p[0]->t == DAO_MAP && p[0]->v.p ) *res = 1;
		}else if( strcmp( tname->mbs, "array" ) ==0 ){
			if( p[0]->t == DAO_ARRAY && p[0]->v.p ) *res = 1;
		}else{
			DaoType *tp = DaoParser_ParseTypeName( tname->mbs, ns, 0,0 );
			if( tp && DaoType_MatchValue( tp, *p[0], NULL ) ) *res = 1;
		}
	}else{
		DaoContext_RaiseException( ctx, DAO_ERROR, "invalid parameter" );
	}
}
static void REFL_Self( DaoContext *ctx, DValue *p[], int N )
{
	if( p[0]->t == DAO_OBJECT )
		DaoContext_SetResult( ctx, (DaoBase*) p[0]->v.object->that );
	else
		DaoContext_PutValue( ctx, daoNullValue );
}
static void REFL_Param( DaoContext *ctx, DValue *p[], int N )
{
	DRoutine *routine = (DRoutine*) p[0]->v.p;
	DaoList *list = DaoContext_PutList( ctx );
	DaoTuple *tuple;
	DaoType *routype = routine->routType;
	DaoType *itp = list->unitype->nested->items.pType[0];
	DaoType **nested = routype->nested->items.pType;
	DString *mbs = DString_New(1);
	DNode *node;
	DValue num = daoZeroInt;
	DValue str = daoNullString;
	DValue vtp = daoNullValue;
	DValue vtup = daoNullTuple;
	DValue cst;
	int i;
	str.v.s = mbs;
	for(i=0; i<routine->parCount; i++){
		if( i >= routype->nested->size ) break;
		tuple = DaoTuple_New( 4 );
		tuple->unitype = itp;
		GC_IncRC( itp );
		vtup.v.tuple = tuple;
		vtp.t = 0;
		num.v.i = 0;
		cst = daoNullValue;
		vtp.v.p = (DaoBase*) nested[i];
		if( nested[i] ) vtp.t = DAO_TYPE;
		cst = routine->routConsts->data[i];
		if( cst.t || cst.ndef == 0 ) num.v.i = 1;
		DValue_Copy( tuple->items->data, str );
		DValue_Copy( tuple->items->data + 1, vtp );
		DValue_Copy( tuple->items->data + 2, num );
		DValue_Copy( tuple->items->data + 3, cst );
		DaoList_Append( list, vtup );
	}
	DString_Delete( mbs );
	if( routype->mapNames ){
		node = DMap_First( routype->mapNames );
		for( ; node !=NULL; node = DMap_Next( routype->mapNames, node ) ){
			i = node->value.pInt;
			mbs = list->items->data[i].v.tuple->items->data[0].v.s;
			DString_Assign( mbs, node->key.pString );
		}
	}
}
static void REFL_Argc( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutInteger( ctx, ctx->parCount );
}
static void REFL_Argv( DaoContext *ctx, DValue *p[], int N )
{
	int i;
	if( N ==0 ){
		DaoList *list = DaoContext_PutList( ctx );
		for(i=0; i<ctx->parCount; i++) DaoList_Append( list, *ctx->regValues[i] );
	}else{
		DValue val = daoNullValue;
		if( p[0]->v.i < ctx->parCount ) val = *ctx->regValues[ p[0]->v.i ];
		DaoContext_PutValue( ctx, val );
	}
}
static void REFL_Trace( DaoContext *ctx, DValue *p[], int N )
{
	DaoVmProcess_Trace( ctx->process, 0x7fffffff );
}
static void REFL_Doc( DaoContext *ctx, DValue *p[], int N )
{
	DString *doc = NULL;
	switch( p[0]->t ){
	case DAO_CLASS : doc = p[0]->v.klass->classHelp; break;
	case DAO_OBJECT : doc = p[0]->v.object->myClass->classHelp; break;
	case DAO_ROUTINE : doc = p[0]->v.routine->routHelp; break;
	default : break;
	}
	if( doc == NULL ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "documentation not available" );
		return;
	}
	DaoContext_PutMBString( ctx, doc->mbs );
	if( N >1 ){
		DString_Clear( doc );
		DString_Append( doc, p[1]->v.s );
	}
}
/* name( class/routine/type )
 * type( any )
 * find( "name" )
 * base( class )
 * field( class/object/ns/ )
 * doc( class/routine )
 * class( object/routine )
 *  routine( class/object ) if omitted, current routine
 * param( routine ) if omitted, current params
 * self( object )
 * ns() current ns
 * trace( print=0 )
 * */
static DaoFuncItem reflMeths[]=
{
	{ REFL_NS,    "namespace() => any" },
	{ REFL_NS,    "namespace( object ) => any" },
	{ REFL_Name,  "name( object ) => string" },
	{ REFL_Type,  "type( object ) => any" },
	{ REFL_Base,  "base( object ) => list<any>" },
	{ REFL_Doc,   "doc( object, newdoc='' ) => string" },
	{ REFL_Cst1,  "constant( object, restrict=0 )=>map<string,tuple<value:any,type:any>>" },
	{ REFL_Var1,  "variable( object, restrict=0 )=>map<string,tuple<value:any,type:any>>" },
	{ REFL_Cst2,  "constant( object, name:string )=>tuple<value:any,type:any>" },
	{ REFL_Var2,  "variable( object, name:string )=>tuple<value:any,type:any>" },
	{ REFL_Cst2,  "constant( object, name:string, value )=>tuple<value:any,type:any>" },
	{ REFL_Var2,  "variable( object, name:string, value )=>tuple<value:any,type:any>" },
	{ REFL_Class,   "class( object ) => any" },
	{ REFL_Routine, "routine() => any" },
	{ REFL_Routine, "routine( rout : any ) => list<any>" },
	{ REFL_Param,   "param( rout )=>list<tuple<name:string,type:any,deft:int,value:any>>" },
	{ REFL_Isa,     "isa( object, name : string ) => int" },
	{ REFL_Isa,     "isa( object, type : any ) => int" },
	{ REFL_Self,    "self( object ) => any" },
	{ REFL_Argc,    "argc() => int" },
	{ REFL_Argv,    "argv() => list<any>" },
	{ REFL_Argv,    "argv( i : int ) => any" },
	{ REFL_Trace,   "trace(  )" },
	{ NULL, NULL }
};

DaoTypeBase libReflectTyper = { 
	"reflect", NULL, NULL, reflMeths, {0}, NULL, NULL
};

static void Corout_Create( DaoContext *ctx, DValue *p[], int N )
{
	DaoVmProcess *vmp = DaoVmProcess_Create( ctx, p, N );
	if( vmp == NULL ) return;
	vmp->parResume = DVarray_New();
	vmp->parYield = DVarray_New();
	DaoContext_SetResult( ctx, (DaoBase*) vmp );
}
static void Corout_Resume( DaoContext *ctx, DValue *p[], int N )
{
	DaoList *list = DaoContext_PutList( ctx );
	DaoVmProcess *self = (DaoVmProcess*) p[0]->v.p;
	if( self->type != DAO_VMPROCESS || self->parResume == NULL ){
		DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "need coroutine object" );
		return;
	}else if( self->status == DAO_VMPROC_FINISHED ){
		DaoContext_RaiseException( ctx, DAO_WARNING, "coroutine execution is finished." );
		return;
	}

	DaoVmProcess_Resume( self, p+1, N-1, list );
	if( self->status == DAO_VMPROC_ABORTED )
		DaoContext_RaiseException( ctx, DAO_ERROR, "coroutine execution is aborted." );
}
static void Corout_Status( DaoContext *ctx, DValue *p[], int N )
{
	DaoVmProcess *self = (DaoVmProcess*) p[0]->v.p;
	DString *res = DaoContext_PutMBString( ctx, "" );
	if( self->type != DAO_VMPROCESS || self->parResume == NULL ){
		DString_SetMBS( res, "not_a_coroutine" );
		DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "need coroutine object" );
		return;
	}

	switch( self->status ){
	case DAO_VMPROC_SUSPENDED : DString_SetMBS( res, "suspended" ); break;
	case DAO_VMPROC_RUNNING :   DString_SetMBS( res, "running" ); break;
	case DAO_VMPROC_ABORTED :   DString_SetMBS( res, "aborted" ); break;
	case DAO_VMPROC_FINISHED :  DString_SetMBS( res, "finished" ); break;
	default : break;
	}
}
static void Corout_Yield( DaoContext *ctx, DValue *p[], int N )
{
	DaoVmProcess_Yield( ctx->process, p, N, DaoContext_PutList( ctx ) );
	ctx->process->pauseType = DAO_VMP_YIELD;
}
static DaoFuncItem coroutMeths[]=
{
	{ Corout_Create,    "create( object, ... )" },
	{ Corout_Resume,    "resume( object, ... )" },
	{ Corout_Status,    "status( object )" },
	{ Corout_Yield,     "yield( ... )" },
	{ NULL, NULL }
};

DaoTypeBase coroutTyper = {
	"coroutine", NULL, NULL, coroutMeths, {0}, NULL, NULL
};

/**/
static void MATH_abs( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, fabs( p[0]->v.d ) );
}
static void MATH_acos( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, acos( p[0]->v.d ) );
}
static void MATH_asin( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, asin( p[0]->v.d ) );
}
static void MATH_atan( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, atan( p[0]->v.d ) );
}
static void MATH_ceil( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, ceil( p[0]->v.d ) );
}
static void MATH_cos( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, cos( p[0]->v.d ) );
}
static void MATH_cosh( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, cosh( p[0]->v.d ) );
}
static void MATH_exp( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, exp( p[0]->v.d ) );
}
static void MATH_floor( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, floor( p[0]->v.d ) );
}
static void MATH_log( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, log( p[0]->v.d ) );
}
static void MATH_sin( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, sin( p[0]->v.d ) );
}
static void MATH_sinh( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, sinh( p[0]->v.d ) );
}
static void MATH_sqrt( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, sqrt( p[0]->v.d ) );
}
static void MATH_tan( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, tan( p[0]->v.d ) );
}
static void MATH_tanh( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, tanh( p[0]->v.d ) );
}
static void MATH_rand( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, p[0]->v.d * rand() / ( RAND_MAX + 1.0 ) );
}
static void MATH_srand( DaoContext *ctx, DValue *p[], int N )
{
	srand( (unsigned int)p[0]->v.d );
}
static void MATH_rand_gaussian( DaoContext *ctx, DValue *p[], int N )
{
	static int iset = 0;
	static double gset;
	double fac, rsq, v1, v2;
	double R = p[0]->v.d;

	if( iset ==0 ){
		do{
			v1 = 2.0 * ( rand() / (RAND_MAX+1.0) ) -1.0;
			v2 = 2.0 * ( rand() / (RAND_MAX+1.0) ) -1.0;
			rsq = v1*v1 + v2*v2 ;
		} while( rsq >= 1.0 || rsq == 0.0 );
		fac = sqrt( -2.0 * log( rsq ) / rsq );
		gset = v1*fac;
		iset = 1;
		DaoContext_PutDouble( ctx, R*v2*fac );
	} else {
		iset = 0;
		DaoContext_PutDouble( ctx, R*gset );
	}
}
static void MATH_pow( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, pow( p[0]->v.d, p[1]->v.d ) );
}

/**/
static void MATH_abs_c( DaoContext *ctx, DValue *p[], int N )
{
	complex16 com = p[0]->v.c[0];
	DaoContext_PutDouble( ctx, sqrt( com.real * com.real + com.imag * com.imag ) );
}
static void MATH_arg_c( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, arg_c( p[0]->v.c[0] ) );
}
static void MATH_norm_c( DaoContext *ctx, DValue *p[], int N )
{
	complex16 com = p[0]->v.c[0];
	DaoContext_PutDouble( ctx, com.real * com.real + com.imag * com.imag );
}
static void MATH_imag_c( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, p[0]->v.c[0].imag );
}
static void MATH_real_c( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutDouble( ctx, p[0]->v.c[0].real );
}

static void MATH_cos_c( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutComplex( ctx, cos_c( p[0]->v.c[0] ) );
}
static void MATH_cosh_c( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutComplex( ctx, cosh_c( p[0]->v.c[0] ) );
}
static void MATH_exp_c( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutComplex( ctx, exp_c( p[0]->v.c[0] ) );
}
static void MATH_log_c( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutComplex( ctx, log_c( p[0]->v.c[0] ) );
}
static void MATH_sin_c( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutComplex( ctx, sin_c( p[0]->v.c[0] ) );
}
static void MATH_sinh_c( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutComplex( ctx, sinh_c( p[0]->v.c[0] ) );
}
static void MATH_sqrt_c( DaoContext *ctx, DValue *p[], int N )
{
	DaoContext_PutComplex( ctx, sqrt_c( p[0]->v.c[0] ) );
}
static void MATH_tan_c( DaoContext *ctx, DValue *p[], int N )
{
	complex16 com = p[0]->v.c[0];
	complex16 *res = DaoContext_PutComplex( ctx, com );
	complex16 R = sin_c( com );
	complex16 L = cos_c( com );
	res->real = ( L.real*R.real + L.imag*R.imag ) / ( R.real*R.real + R.imag*R.imag );
	res->imag = ( L.imag*R.real - L.real*R.imag ) / ( R.real*R.real + R.imag*R.imag );
}
static void MATH_tanh_c( DaoContext *ctx, DValue *p[], int N )
{
	complex16 com = p[0]->v.c[0];
	complex16 *res = DaoContext_PutComplex( ctx, com );
	complex16 R = sinh_c( com );
	complex16 L = cosh_c( com );
	res->real = ( L.real*R.real + L.imag*R.imag ) / ( R.real*R.real + R.imag*R.imag );
	res->imag = ( L.imag*R.real - L.real*R.imag ) / ( R.real*R.real + R.imag*R.imag );
}
static void MATH_ceil_c( DaoContext *ctx, DValue *p[], int N )
{
	complex16 com = p[0]->v.c[0];
	complex16 *res = DaoContext_PutComplex( ctx, com );
	res->real = ceil( com.real );
	res->imag = ceil( com.imag );
}
static void MATH_floor_c( DaoContext *ctx, DValue *p[], int N )
{
	complex16 com = p[0]->v.c[0];
	complex16 *res = DaoContext_PutComplex( ctx, com );
	res->real = floor( com.real );
	res->imag = floor( com.imag );
}

static void MATH_pow_rc( DaoContext *ctx, DValue *p[], int N )
{
	complex16 com = { 0, 0 };
	complex16 *res = DaoContext_PutComplex( ctx, com );
	double lg = log( p[0]->v.d );
	com.real = lg * p[1]->v.c[0].real;
	com.imag = lg * p[1]->v.c[0].imag;
	*res = exp_c( com );
}
static void MATH_pow_cr( DaoContext *ctx, DValue *p[], int N )
{
	complex16 com2 = { 0, 0 };
	complex16 *res = DaoContext_PutComplex( ctx, com2 );
	complex16 com = log_c( p[0]->v.c[0] );
	double v = p[1]->v.d;
	com2.real = v * com.real;
	com2.imag = v * com.imag;
	*res = exp_c( com2 );
}
static void MATH_pow_cc( DaoContext *ctx, DValue *p[], int N )
{
	complex16 com2 = {0,0};
	complex16 *res = DaoContext_PutComplex( ctx, com2 );
	complex16 com = log_c( p[0]->v.c[0] );
	COM_MUL( com2, com, p[1]->v.c[0] );
	*res = exp_c( com2 );
}

static DaoFuncItem mathMeths[]=
{
	{ MATH_abs,       "abs( p :double )=>double" },
	{ MATH_acos,      "acos( p :double )=>double" },
	{ MATH_asin,      "asin( p :double )=>double" },
	{ MATH_atan,      "atan( p :double )=>double" },
	{ MATH_ceil,      "ceil( p :double )=>double" },
	{ MATH_cos,       "cos( p :double )=>double" },
	{ MATH_cosh,      "cosh( p :double )=>double" },
	{ MATH_exp,       "exp( p :double )=>double" },
	{ MATH_floor,     "floor( p :double )=>double" },
	{ MATH_log,       "log( p :double )=>double" },
	{ MATH_sin,       "sin( p :double )=>double" },
	{ MATH_sinh,      "sinh( p :double )=>double" },
	{ MATH_sqrt,      "sqrt( p :double )=>double" },
	{ MATH_tan,       "tan( p :double )=>double" },
	{ MATH_tanh,      "tanh( p :double )=>double" },
	{ MATH_srand,     "srand( p :double )=>double" },
	{ MATH_rand,      "rand( p :double=1.0D )=>double" },
	{ MATH_rand_gaussian,  "rand_gaussian( p :double=1.0D )=>double" },

	{ MATH_pow,       "pow( p1 :double, p2 :double )=>double" },

	{ MATH_abs_c,     "abs( p :complex )=>double" },
	{ MATH_arg_c,     "arg( p :complex )=>double" },
	{ MATH_imag_c,    "imag( p :complex )=>double" },
	{ MATH_norm_c,    "norm( p :complex )=>double" },
	{ MATH_real_c,    "real( p :complex )=>double" },

	{ MATH_cos_c,     "cos( p :complex )=>complex" },
	{ MATH_cosh_c,    "cosh( p :complex )=>complex" },
	{ MATH_exp_c,     "exp( p :complex )=>complex" },
	{ MATH_log_c,     "log( p :complex )=>complex" },
	{ MATH_sin_c,     "sin( p :complex )=>complex" },
	{ MATH_sinh_c,    "sinh( p :complex )=>complex" },
	{ MATH_sqrt_c,    "sqrt( p :complex )=>complex" },
	{ MATH_tan_c,     "tan( p :complex )=>complex" },
	{ MATH_tanh_c,    "tanh( p :complex )=>complex" },
	{ MATH_ceil_c,    "ceil( p :complex )=>complex" },
	{ MATH_floor_c,   "floor( p :complex )=>complex" },

	{ MATH_pow_rc,    "pow( p1 :double, p2 :complex )=>complex" },
	{ MATH_pow_cr,    "pow( p1 :complex, p2 :double )=>complex" },
	{ MATH_pow_cc,    "pow( p1 :complex, p2 :complex )=>complex" },

	{ NULL, NULL }
};

DaoTypeBase libMathTyper = {
	"math", NULL, NULL, mathMeths, {0}, NULL, NULL
};


