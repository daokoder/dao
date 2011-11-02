/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2011, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

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
#include"daoValue.h"

static void STD_Path( DaoProcess *proc, DaoValue *p[], int N )
{
	char *path = DString_GetMBS( p[0]->xString.data );
	switch( p[1]->xInteger.value ){
	case 0 : DaoVmSpace_SetPath( proc->vmSpace, path ); break;
	case 1 : DaoVmSpace_AddPath( proc->vmSpace, path ); break;
	case 2 : DaoVmSpace_DelPath( proc->vmSpace, path ); break;
	}
}
static void STD_Compile( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns = proc->activeNamespace;
	if( DaoProcess_Compile( proc, ns, p[0]->xString.data, p[1]->xInteger.value ) ==0 ){
		DaoProcess_PutValue( proc, dao_none_value );
		return;
	}
	DaoProcess_PutValue( proc, ns->mainRoutines->items.pValue[ ns->mainRoutines->size-1 ] );
}
static void STD_Eval( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoVmSpace *vms = proc->vmSpace;
	DaoNamespace *ns = proc->activeNamespace;
	DaoStream *prevStream = vms->stdStream->redirect;
	DaoStream *redirect = (DaoStream*) p[2];
	dint *num = DaoProcess_PutInteger( proc, 0 );
	int safe = p[3]->xInteger.value;
	int wasProt = 0;
	if( vms->options & DAO_EXEC_SAFE ) wasProt = 1;
	if( redirect != prevStream ) vms->stdStream->redirect = redirect;

	if( safe ) vms->options |= DAO_EXEC_SAFE;
	*num = DaoProcess_Eval( proc, ns, p[0]->xString.data, p[1]->xInteger.value );
	if( ! wasProt ) vms->options &= ~DAO_EXEC_SAFE;
	if( redirect != prevStream ) vms->stdStream->redirect = prevStream;
}
static void STD_Load( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *name = p[0]->xString.data;
	int import = p[1]->xInteger.value;
	int safe = p[2]->xInteger.value;
	int wasProt = 0;
	int res = 0;
	DaoVmSpace *vms = proc->vmSpace;
	DaoNamespace *ns;
	DString_ToMBS( name );
	if( safe ) vms->options |= DAO_EXEC_SAFE;
	if( vms->options & DAO_EXEC_SAFE ) wasProt = 1;
	DArray_PushFront( vms->pathLoading, proc->activeNamespace->path );
	ns = DaoVmSpace_Load( vms, name );
	DaoProcess_PutValue( proc, (DaoValue*) ns );
	if( ! wasProt ) vms->options &= ~DAO_EXEC_SAFE;
#if 0
	if( ns ){ /* in the case that it is cancelled from console */
		DArray_PushFront( vms->pathLoading, ns->path );
		res = DaoProcess_Call( proc, (DaoMethod*)ns->mainRoutine, NULL, NULL, 0 );
		if( proc->stopit | vms->stopit )
			DaoProcess_RaiseException( proc, DAO_ERROR, "loading cancelled" );
		else if( res == 0 )
			DaoProcess_RaiseException( proc, DAO_ERROR, "loading failed" );
		DArray_PopFront( vms->pathLoading );
	}
#endif
	DArray_PopFront( vms->pathLoading );
	if( import && ns ) DaoNamespace_Import( proc->activeNamespace, ns, NULL );
}
static void Dao_AboutVar( DaoNamespace *ns, DaoValue *var, DString *str )
{
	DaoType *abtp = DaoNamespace_GetType( ns, var );
	char buf[50];
	if( abtp ){
		DString_Append( str, abtp->name );
		sprintf( buf, "[%p]", var );
		DString_AppendMBS( str, buf );
	}else{
		DString_AppendMBS( str, "NULL" );
	}
}
static void Dao_AboutVars( DaoNamespace *ns, DaoValue *par[], int N, DString *str )
{
	int i;
	DString_Clear( str );
	for( i=0; i<N; i++ ){
		Dao_AboutVar( ns, par[i], str );
		if( i+1<N ) DString_AppendMBS( str, " " );
	}
}
static void STD_About( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *str = DaoProcess_PutMBString( proc, "" );
	Dao_AboutVars( proc->activeNamespace, p, N, str );
}
static void STD_Callable( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoValue *p0 = p[0];
	dint *res = DaoProcess_PutInteger( proc, 0 );
	if( p0 == NULL || p0->type == 0 ){
		*res = 0;
		return;
	}
	switch( p0->type ){
	case DAO_CLASS :
	case DAO_ROUTINE :
	case DAO_FUNCTION :
		*res = 1;
		break;
	case DAO_OBJECT :
		{
			DaoObject *object = & p0->xObject;
			DString *mbs = DString_New(1);
			DString_SetMBS( mbs, "()" );
			DaoObject_GetData( object, mbs, & p0, proc->activeObject );
			DString_Delete( mbs );
			if( p0 && p0->type == DAO_ROUTINE ) *res = 1;
			break;
		}
	case DAO_CTYPE :
		{
			DaoTypeBase *typer = p0->xCdata.typer;
			*res = DaoTypeBase_FindFunction( typer, p0->xCdata.ctype->name ) != NULL;
			break;
		}
	case DAO_CDATA :
		{
			DaoTypeBase *typer = p0->xCdata.typer;
			*res = DaoTypeBase_FindFunctionMBS( typer, "()" ) != NULL;
			break;
		}
	default : break;
	}
}
static void STD_Copy( DaoProcess *proc, DaoValue *p[], int N )
{
	DMap *cycData = DMap_New(0,0);
	DaoTypeBase *typer = DaoValue_GetTyper( p[0] );
	DaoProcess_PutValue( proc, typer->core->Copy( p[0], proc, cycData ) );
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

void DaoProcess_Trace( DaoProcess *self, int depth )
{
	DaoStream *stream = self->vmSpace->stdStream;
	DaoStackFrame *frame = self->topFrame;
	int k, i = 0;
	while( frame && (frame->routine || frame->function) ){
		DRoutine *routine = (DRoutine*) frame->routine;
		if( routine == NULL ) routine = (DRoutine*) frame->function;
		if( depth && ++i > depth ) break;

		DaoStream_WriteString( stream, routine->routName );
		DaoStream_WriteMBS( stream, "(): " );
		if( routine->routType ) DaoStream_WriteString( stream, routine->routType->name );

		if( frame->routine ){
			k = (i==1) ? (int)( self->activeCode - frame->codes ) : frame->entry;
			DaoStream_WriteMBS( stream, ", instruction " );
			DaoStream_WriteInt( stream, k );
			DaoStream_WriteMBS( stream, " at line " );
			DaoStream_WriteInt( stream, frame->routine->annotCodes->items.pVmc[k]->line );
		}

		DaoStream_WriteMBS( stream, " in " );
		DaoStream_WriteString( stream, routine->nameSpace->name );
		DaoStream_WriteMBS( stream, ";" );
		DaoStream_WriteNewLine( stream );
		frame = frame->prev;
	}
}
void DaoRoutine_FormatCode( DaoRoutine *self, int i, DString *output );
void STD_Debug( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoUserHandler *handler = proc->vmSpace->userHandler;
	DaoRoutine *routine = proc->activeRoutine;
	DaoStream *stream = proc->vmSpace->stdStream;
	DString *input;
	DArray *tokens;
	DMap   *cycData;
	char *chs, *cmd;
	int i;
	if( ! (proc->vmSpace->options & DAO_EXEC_DEBUG ) ) return;
	input = DString_New(1);
	if( N > 0 && p[0]->type == DAO_STREAM ){
		stream = (DaoStream*)p[0];
		p ++;
		N --;
	}
	if( N > 0 ){
		Dao_AboutVars( proc->activeNamespace, p, N, input );
		DaoStream_WriteString( stream, input );
		DaoStream_WriteMBS( stream, "\n" );
		DString_Delete( input );
		return;
	}
	if( handler && handler->StdlibDebug ){
		handler->StdlibDebug( handler, proc );
		return;
	}
	tokens = DArray_New(D_STRING);
	cycData = DMap_New(0,0);
	while(1){
		if( proc->vmSpace->ReadLine ){
			chs = proc->vmSpace->ReadLine( "(debug) " );
			if( chs ){
				DString_SetMBS( input, chs );
				DString_Trim( input );
				if( input->size && proc->vmSpace->AddHistory )
					proc->vmSpace->AddHistory( chs );
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
			proc->status = DAO_VMPROC_ABORTED;
			break;
		}else if( strcmp( cmd, "a" ) == 0 || strcmp( cmd, "about" ) == 0 ){
			if( tokens->size > 1 ){
				ushort_t reg = (ushort_t)strtod( tokens->items.pString[1]->mbs, 0 );
				DaoType *tp = proc->activeTypes[ reg ];
				DString_Clear( input );
				Dao_AboutVar( proc->activeNamespace, proc->activeValues[reg], input );
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
				int entry = proc->activeCode - proc->activeRoutine->vmCodes->codes;
				if( n < 0 ) n = entry - n;
				if( n >= routine->vmCodes->size ) n = routine->vmCodes->size -1;
				proc->topFrame->entry = n;
				proc->status = DAO_VMPROC_STACKED;
				return;
			}
		}else if( strcmp( cmd, "h" ) == 0 || strcmp( cmd, "help" ) == 0 ){
			DaoStream_WriteMBS( stream, help );
		}else if( strcmp( cmd, "l" ) == 0 || strcmp( cmd, "list" ) == 0 ){
			DString *mbs = DString_New(1);
			int entry = proc->activeCode - proc->activeRoutine->vmCodes->codes;
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
				DaoValue_Print( proc->activeValues[reg], proc, stream, cycData );
				DaoStream_WriteMBS( stream, "\n" );
			}
		}else if( strcmp( cmd, "t" ) == 0 || strcmp( cmd, "trace" ) == 0 ){
			int depth = 1;
			if( tokens->size >1 ) depth = atoi( tokens->items.pString[1]->mbs );
			DaoProcess_Trace( proc, depth );
		}else{
			DaoStream_WriteMBS( stream, "Unknown debugging command.\n" );
		}
	}
	DString_Delete( input );
	DArray_Delete( tokens );
}
static void STD_Error( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_RaiseException( proc, DAO_ERROR, DString_GetMBS( p[0]->xString.data ) );
}
static void STD_Log( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *info = p[0]->xString.data;
	FILE *fout = fopen( "dao.log", "a" );
	DString_ToMBS( info );
	fprintf( fout, "%s\n", info->mbs );
	fclose( fout );
}
static void STD_Gcmax( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutInteger( proc, DaoGC_Max( -1 ) );
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		if( N == 1 ) DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( N == 1 ) DaoGC_Max( (int)p[0]->xInteger.value );
}
static void STD_Gcmin( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutInteger( proc, DaoGC_Min( -1 ) );
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		if( N == 1 ) DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( N == 1 ) DaoGC_Min( (int)p[0]->xInteger.value );
}
static void STD_ListMeth( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoTypeBase *typer = DaoValue_GetTyper( p[0] );
	DaoFunction **meths;
	DArray *array;
	DMap *hash;
	DNode *it;
	int i, j, methCount;
	if( typer == NULL || typer->core == NULL ) return;
	array = DArray_New(0);
	hash = typer->core->kernel->values;
	if( hash == NULL ){
		DaoNamespace_SetupValues( typer->core->kernel->nspace, typer );
		hash = typer->core->kernel->values;
	}
	if( typer->core->kernel->methods == NULL ){
		DaoNamespace_SetupMethods( typer->core->kernel->nspace, typer );
	}
	if( typer->core->kernel->methods ) DMap_SortMethods( typer->core->kernel->methods, array );
	meths = (DaoFunction**) array->items.pVoid;
	methCount = array->size;
	DaoProcess_Print( proc, "======================================\nConsts, methods of type \"" );
	DaoProcess_Print( proc, typer->name );
	DaoProcess_Print( proc, "\":\n======================================\n" );
	if( typer->core->kernel->values ){
		for(it=DMap_First(hash); it; it=DMap_Next(hash,it)){
			DaoProcess_Print( proc, it->key.pString->mbs );
			DaoProcess_Print( proc, "\n" );
		}
	}
	if( typer->core->kernel->methods ){
		for(i=0; i<methCount; i++ ){
			DaoProcess_Print( proc, meths[i]->routName->mbs );
			DaoProcess_Print( proc, ": " );
			for(j=meths[i]->routName->size; j<20; j++) DaoProcess_Print( proc, " " );
			DaoProcess_Print( proc, meths[i]->routType->name->mbs );
			DaoProcess_Print( proc, "\n" );
		}
	}
	DArray_Delete( array );
}
extern int DaoToken_Tokenize( DArray *toks, const char *src, int r, int cmt, int nosp );
static void STD_Tokenize( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *source = p[0]->xString.data;
	DaoList *list = DaoProcess_PutList( proc );
	DArray *tokens = DArray_New(D_TOKEN);
	int i, rc = 0;
	DString_ToMBS( source );
	rc = DaoToken_Tokenize( tokens, source->mbs, 0, 1, 1 );
	if( rc ){
		DaoString *str = DaoString_New(1);
		for(i=0; i<tokens->size; i++){
			DString_Assign( str->data, tokens->items.pToken[i]->string );
			DArray_Append( & list->items, (DaoValue*) str );
		}
		DaoString_Delete( str );
	}
	DArray_Delete( tokens );
}
static void STD_SubType( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoType *tp1 = DaoNamespace_GetType( proc->activeNamespace, p[0] );
	DaoType *tp2 = DaoNamespace_GetType( proc->activeNamespace, p[1] );
	DaoProcess_PutInteger( proc, DaoType_MatchTo( tp1, tp2, NULL ) );
}
static void STD_Unpack( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoInteger zero = {DAO_INTEGER,0,0,0,0,0};
	DaoList *list = DaoProcess_PutList( proc );
	DString *str = p[0]->xString.data;
	DaoValue **data;
	int i;
	if( str->mbs ){
		DArray_Resize( & list->items, str->size, & zero );
		data = list->items.items.pValue;
		for( i=0; i<str->size; i++ ){
			data[i]->xInteger.value = (uchar_t)str->mbs[i];
		}
	}else{
		DArray_Resize( & list->items, str->size, & zero );
		data = list->items.items.pValue;
		for( i=0; i<str->size; i++ ){
			data[i]->xInteger.value = (wchar_t)str->wcs[i];
		}
	}
}
#ifdef DAO_WITH_SERIALIZATION
static void STD_Serialize( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *mbs = DaoProcess_PutMBString( proc, "" );
	DaoValue_Serialize( p[0], mbs, proc->activeNamespace, proc );
}
static void STD_Deserialize( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoValue *value = NULL;
	DaoValue_Deserialize( & value, p[0]->xString.data, proc->activeNamespace, proc );
	DaoProcess_PutValue( proc, value );
	GC_DecRC( value );
}
static void STD_Backup( DaoProcess *proc, DaoValue *p[], int N )
{
	FILE *fout = fopen( DString_GetMBS( p[0]->xString.data ), "w+" );
	if( fout == NULL ){
		DaoProcess_RaiseException( proc,DAO_ERROR_FILE, DString_GetMBS( p[0]->xString.data ) );
		return;
	}
	DaoNamespace_Backup( proc->activeNamespace, proc, fout, p[1]->xInteger.value );
	fclose( fout );
}
static void STD_Restore( DaoProcess *proc, DaoValue *p[], int N )
{
	FILE *fin = fopen( DString_GetMBS( p[0]->xString.data ), "r" );
	if( fin == NULL ){
		DaoProcess_RaiseException( proc,DAO_ERROR_FILE, DString_GetMBS( p[0]->xString.data ) );
		return;
	}
	DaoNamespace_Restore( proc->activeNamespace, proc, fin );
	fclose( fin );
}
#else
static void STD_Serialize( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_RaiseException( proc, DAO_ERROR, getCtInfo( DAO_DISABLED_SERIALIZATION ) );
}
#define STD_Deserialize STD_Serialize
#define STD_Backup      STD_Serialize
#define STD_Restore     STD_Serialize
#endif
static void STD_Warn( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_RaiseException( proc, DAO_WARNING, DString_GetMBS( p[0]->xString.data ) );
}
static void STD_Version( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutMBString( proc, DAO_VERSION );
}

static void STD_String( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DString *string = DaoProcess_PutMBString( proc, "" );
	dint i, entry, size = p[0]->xInteger.value;

	if( p[1]->xEnum.value ) DString_ToWCS( string );
	if( sect == NULL || size < 0 ) return; // TODO exception
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	for(i=0; i<size; i++){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_VMPROC_ABORTED ) break;
		DString_AppendWChar( string, proc->stackValues[0]->xInteger.value );
	}
	DaoProcess_PopFrame( proc );
}
int DaoArray_AlignShape( DaoArray *self, DArray *sidx, size_t *dims, int ndim );
static void STD_Array( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *res, *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoArray *array = DaoProcess_PutArray( proc );
	DaoArray *first = NULL;
	DaoArray *sub = NULL;
	dint i, j, k, entry, size = 1;

	/* if multi-dimensional array is disabled, DaoProcess_PutArray() will raise exception. */
#ifdef DAO_WITH_NUMARRAY
	for(i=0; i<N; i++){
		dint d = p[i]->xInteger.value;
		if( d < 0 ){
			DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, NULL );
			break;
		}
		size *= d;
	}
	if( size == 0 ) return;
	if( sect == NULL ) return; // TODO exception
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	for(i=0; i<size; i++){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_VMPROC_ABORTED ) break;
		res = proc->stackValues[0];
		if( i == 0 ){
			int D = N + (res->type == DAO_ARRAY ? res->xArray.ndim : 0);
			DaoArray_SetDimCount( array, D );
			for(j=0; j<N; j++) array->dims[j] = p[j]->xInteger.value;
			if( res->type == DAO_ARRAY ){
				first = (DaoArray*) res;
				memmove( array->dims + N, first->dims, first->ndim*sizeof(size_t) );
			}
			DaoArray_ResizeArray( array, array->dims, D );
		}
		if( res->type == DAO_ARRAY ){
			sub = (DaoArray*) res;
			if( first == NULL || DaoArray_AlignShape( sub, NULL, first->dims, first->ndim ) ==0 ){
				DaoProcess_RaiseException( proc, DAO_ERROR, "inconsistent elements or subarrays" );
				break;
			}
			k = i * sub->size;
			for(j=0; j<sub->size; j++){
				switch( array->etype ){
				case DAO_INTEGER : array->data.i[k+j] = DaoArray_GetInteger( sub, j ); break;
				case DAO_FLOAT   : array->data.f[k+j] = DaoArray_GetFloat( sub, j ); break;
				case DAO_DOUBLE  : array->data.d[k+j] = DaoArray_GetDouble( sub, j ); break;
				case DAO_COMPLEX : array->data.c[k+j] = DaoArray_GetComplex( sub, j ); break;
				}
			}
		}else{
			switch( array->etype ){
			case DAO_INTEGER : array->data.i[i] = DaoValue_GetInteger( res ); break;
			case DAO_FLOAT   : array->data.f[i] = DaoValue_GetFloat( res ); break;
			case DAO_DOUBLE  : array->data.d[i] = DaoValue_GetDouble( res ); break;
			case DAO_COMPLEX : array->data.c[i] = DaoValue_GetComplex( res ); break;
			}
		}
	}
	DaoProcess_PopFrame( proc );
#endif
}
static void STD_List( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *res = p[N==2], *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoList *list = DaoProcess_PutList( proc );
	dint i, entry, size = p[0]->xInteger.value;

	if( sect == NULL || size < 0 ) return; // TODO exception
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	for(i=0; i<size; i++){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, index );
		if( sect->b >1 && N ==2 ) DaoProcess_SetValue( proc, sect->a+1, res );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_VMPROC_ABORTED ) break;
		res = proc->stackValues[0];
		DaoList_Append( list, res );
	}
	DaoProcess_PopFrame( proc );
}
static void STD_Map( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *res, *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoMap *map = DaoProcess_PutMap( proc );
	dint i, entry, size = p[0]->xInteger.value;

	if( sect == NULL || size < 0 ) return; // TODO exception
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	for(i=0; i<size; i++){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_VMPROC_ABORTED ) break;
		res = proc->stackValues[0];
		if( res->type == DAO_TUPLE && res->xTuple.size == 2 )
			DaoMap_Insert( map, res->xTuple.items[0], res->xTuple.items[1] );
	}
	DaoProcess_PopFrame( proc );
}
static DaoFuncItem stdMeths[]=
{
	{ STD_Path,      "path( path :string, action :enum<set,add,remove>=$add )" },
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
	{ STD_Tokenize,  "tokenize( source :string )=>list<string>" },
	{ STD_SubType,   "subtype( obj1, obj2 )=>int" },
	{ STD_Unpack,    "unpack( string :string )=>list<int>" },
	{ STD_Serialize, "serialize( value : any )=>string" },
	{ STD_Deserialize, "deserialize( text : string )=>any" },
	{ STD_Backup,    "backup( tofile = 'backup.sdo', limit=0 )" },
	{ STD_Restore,   "restore( fromfile = 'backup.sdo' )" },
	{ STD_Warn,      "warn( info :string )" },
	{ STD_Version,   "version()=>string" },

	{ STD_String,   "string( size :int, type :enum<mbs,wcs>=$mbs )[index:int =>int] =>string" },
	{ STD_Array,    "array( D1 :int, D2 =0, D3 =0 )[I:int, J:int, K:int =>@V<@T<int|float|double|complex>|array<@T>>] =>array<@T>" },
	{ STD_List,     "list( size :int )[index:int =>@T] =>list<@T>" },
	{ STD_List,     "list( size :int, init :@T )[index:int, prev:@T =>@T] =>list<@T>" },
	{ STD_Map,      "map( size :int )[index:int =>tuple<@K,@V>] =>map<@K,@V>" },
	{ NULL, NULL }
};

DaoTypeBase libStandardTyper = {
	"std", NULL, NULL, stdMeths, {0}, {0}, NULL, NULL
};

static void SYS_Ctime( DaoProcess *proc, DaoValue *p[], int N )
{
	struct tm *ctime;
	time_t t = (time_t)p[0]->xInteger.value;
	DaoTuple *tuple = DaoTuple_New( 7 );
	if( t == 0 ) t = time(NULL);
	ctime = gmtime( & t );
	tuple->items[0] = DaoValue_NewInteger( ctime->tm_year + 1900 );
	tuple->items[1] = DaoValue_NewInteger( ctime->tm_mon + 1 );
	tuple->items[2] = DaoValue_NewInteger( ctime->tm_mday );
	tuple->items[3] = DaoValue_NewInteger( ctime->tm_wday + 1 );
	tuple->items[4] = DaoValue_NewInteger( ctime->tm_hour );
	tuple->items[5] = DaoValue_NewInteger( ctime->tm_min );
	tuple->items[6] = DaoValue_NewInteger( ctime->tm_sec );
	DaoProcess_PutValue( proc, (DaoValue*) tuple );
}
static int addStringFromMap( DaoValue *self, DString *S, DaoMap *sym, const char *key, int id )
{
	DNode *node;

	if( S==NULL || sym==NULL ) return 0;
	DString_SetMBS( self->xString.data, key );
	node = DMap_Find( sym->items, & self );
	if( node ){
		DaoList *list = & node->value.pValue->xList;
		if( list->type == DAO_LIST && list->items.size > id ){
			DaoValue *p = list->items.items.pValue[ id ];
			if( p->type == DAO_STRING ){
				DString_Append( S, p->xString.data );
				return 1;
			}
		}
	}
	return 0;
}
static void SYS_Ctimef( DaoProcess *proc, DaoValue *p[], int N )
{
	int  i;
	int halfday = 0;
	const int size = p[1]->xString.data->size;
	const char *format = DString_GetMBS( p[1]->xString.data );
	char buf[100];
	char *p1 = buf+1;
	char *p2;
	DaoMap *sym = NULL;
	DaoString *ds = DaoString_New(1);;
	DaoValue *key = (DaoValue*) ds;
	DString *S;

	struct tm *ctime;
	time_t t = (time_t)p[0]->xInteger.value;
	if( t == 0 ) t = time(NULL);
	ctime = gmtime( & t );

	if( N > 1 ){
		sym = (DaoMap*)p[2];
		if( sym->items->size == 0 ) sym = NULL;
	}
	S = DaoProcess_PutMBString( proc, "" );

	for( i=0; i+1<size; i++ ){
		if( format[i] == '%' && ( format[i+1] == 'a' || format[i+1] == 'A' ) ){
			halfday = 1;
			break;
		}
	}
	buf[0] = '0'; /* for padding */

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
	DaoString_Delete( ds );
}
static void SYS_Sleep( DaoProcess *proc, DaoValue *p[], int N )
{
#ifdef DAO_WITH_THREAD
	DMutex    mutex;
	DCondVar  condv;
#endif

	double s = p[0]->xFloat.value;
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( s < 0 ){
		DaoProcess_RaiseException( proc, DAO_WARNING_VALUE, "expecting positive value" );
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
static void SYS_Exit( DaoProcess *proc, DaoValue *p[], int N )
{
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	exit( (int)p[0]->xInteger.value );
}
static void SYS_System( DaoProcess *proc, DaoValue *p[], int N )
{
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	DaoProcess_PutInteger( proc, system( DString_GetMBS( p[0]->xString.data ) ) );
}
static void SYS_Time( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutInteger( proc, time( NULL ) );
}
static void SYS_Time2( DaoProcess *proc, DaoValue *p[], int N )
{
	/* extern long timezone; */
	/* extern int daylight; // not on WIN32 */
	struct tm ctime;
	DaoValue **tup = p[0]->xTuple.items;
	memset( & ctime, 0, sizeof( struct tm ) );
	ctime.tm_year = tup[0]->xInteger.value - 1900;
	ctime.tm_mon = tup[1]->xInteger.value - 1;
	ctime.tm_mday = tup[2]->xInteger.value;
	ctime.tm_hour = tup[4]->xInteger.value;/* + daylight; */
	ctime.tm_min = tup[5]->xInteger.value;
	ctime.tm_sec = tup[6]->xInteger.value;
	ctime.tm_isdst = 0;
	DaoProcess_PutInteger( proc, (int) mktime( & ctime ) );
}
static void SYS_SetLocale( DaoProcess *proc, DaoValue *p[], int N )
{
	int category = 0;
	char* old;
	switch( p[0]->xEnum.value ){
	case 0: category = LC_ALL; break;
	case 1: category = LC_COLLATE; break;
	case 2: category = LC_CTYPE; break;
	case 3: category = LC_MONETARY; break;
	case 4: category = LC_NUMERIC; break;
	case 5: category = LC_TIME; break;
	}
	old = setlocale( category, DString_GetMBS( p[1]->xString.data ) );
	if ( old )
		DaoProcess_PutMBString( proc, old );
	else
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid locale" );
}
static void SYS_Clock( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutFloat( proc, ((float)clock())/CLOCKS_PER_SEC );
}

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
	{ SYS_SetLocale,
		"setlocale( category: enum<all,collate,ctype,monetary,numeric,time> = $all, locale = '' )=>string" },
	{ SYS_Clock,     "clock()=>float" },
	{ NULL, NULL }
};

DaoTypeBase libSystemTyper = {
	"sys", NULL, NULL, sysMeths, {0}, {0}, NULL, NULL
};

DaoProcess* DaoProcess_Create( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoProcess *vmProc;
	DaoRoutine *routine;
	DaoValue *val = par[0];
	DRoutine *rout;
	int i, passed = 0;
	if( val->type == DAO_STRING ) val = DaoNamespace_GetData( proc->activeNamespace, val->xString.data );
	if( val == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, NULL );
		return NULL;
	}
	rout = (DRoutine*)DRoutine_Resolve( val, NULL, par+1, N-1, DVM_CALL );
	if( rout ) passed = DRoutine_PassParams( rout, NULL, proc->freeValues, par+1, N-1, DVM_CALL );
	if( passed == 0 || rout == NULL || rout->type != DAO_ROUTINE ){
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "not matched" );
		return NULL;
	}
	routine = (DaoRoutine*) rout;
	vmProc = DaoProcess_New( proc->vmSpace );
	DaoProcess_PushRoutine( vmProc, routine, NULL );
	vmProc->activeValues = vmProc->stackValues + vmProc->topFrame->stackBase;
	for(i=0; i<routine->parCount; i++){
		vmProc->activeValues[i] = proc->freeValues[i];
		GC_IncRC( vmProc->activeValues[i] );
	}
	vmProc->status = DAO_VMPROC_SUSPENDED;
	return vmProc;
}

/**/
static void REFL_NS( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *res = proc->activeNamespace;
	if( N == 0 ){
		res = proc->activeNamespace;
	}else if( p[0]->type == DAO_CLASS ){
		res = p[0]->xClass.classRoutine->nameSpace;
	}else if( p[0]->type == DAO_FUNCTREE ){
		res = p[0]->xFunctree.space;
	}else if( p[0]->type == DAO_ROUTINE || p[0]->type == DAO_FUNCTION ){
		res = p[0]->xRoutine.nameSpace;
	}
	DaoProcess_PutValue( proc, (DaoValue*) res );
}
static void REFL_Name( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *str = DaoProcess_PutMBString( proc, "" );
	switch( p[0]->type ){
	case DAO_ROUTINE :
	case DAO_FUNCTION :
		DString_Assign( str, p[0]->xRoutine.routName );
		break;
	case DAO_CLASS :
		DString_Assign( str, p[0]->xClass.className );
		break;
	case DAO_TYPE :
		DString_Assign( str, p[0]->xType.name );
		break;
	default : break;
	}
}
static void REFL_Base( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *ls = DaoProcess_PutList( proc );
	int i;
	if( p[0]->type == DAO_CLASS ){
		DaoClass *k = & p[0]->xClass;
		for( i=0; i<k->superClass->size; i++ ){
			DaoList_Append( ls, k->superClass->items.pValue[i] );
		}
	}else if( p[0]->type == DAO_OBJECT ){
		DaoObject *k = & p[0]->xObject;
		for( i=0; i<k->baseCount; i++ ) DaoList_Append( ls, k->parents[i] );
	}
}
static void REFL_Type( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoType *tp = DaoNamespace_GetType( proc->activeNamespace, p[0] );
	DaoProcess_PutValue( proc, (DaoValue*) tp );
}

static void REFL_Cst1( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap *map = DaoProcess_PutMap( proc );
	DaoTuple *tuple;
	DaoClass *klass;
	DaoObject *object;
	DaoType *tp = map->unitype->nested->items.pType[1];
	DaoNamespace *ns, *here = proc->activeNamespace;
	DMap *index = NULL, *lookup = NULL;
	DArray *data;
	DNode *node;
	DaoValue *value;
	DaoValue *vabtp = NULL;
	DaoString name = {DAO_STRING,0,0,0,0,NULL};
	int restri = p[1]->xInteger.value;
	name.data = DString_New(1);
	if( p[0]->type == DAO_CLASS || p[0]->type == DAO_OBJECT ){
		klass = & p[0]->xClass;
		if( p[0]->type == DAO_OBJECT ){
			object = & p[0]->xObject;
			klass = object->defClass;
		}
		lookup = klass->lookupTable;
		index = klass->lookupTable;
		data = klass->cstData;
	}else if( p[0]->type == DAO_NAMESPACE ){
		ns = & p[0]->xNamespace;
		//index = ns->cstIndex; XXX
		data = ns->cstData;
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
		DString_Delete( name.data );
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
		value = data->items.pValue[ id ];
		vabtp = (DaoValue*) DaoNamespace_GetType( here, value );
		DaoValue_Copy( value, tuple->items );
		DaoValue_Copy( vabtp, tuple->items + 1 );
		DString_Assign( name.data, node->key.pString );
		DaoMap_Insert( map, (DaoValue*) & name, (DaoValue*) & tuple );
	}
	DString_Delete( name.data );
}
static void REFL_Var1( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap *map = DaoProcess_PutMap( proc );
	DaoTuple *tuple;
	DaoClass *klass = NULL;
	DaoObject *object = NULL;
	DaoType *tp = map->unitype->nested->items.pType[1];
	DaoNamespace *ns = NULL;
	DMap *index = NULL, *lookup = NULL;
	DNode *node;
	DaoValue *value;
	DaoValue *vabtp = NULL;
	DaoString name = {DAO_STRING,0,0,0,0,NULL};
	int restri = p[1]->xInteger.value;
	name.data = DString_New(1);
	if( p[0]->type == DAO_CLASS || p[0]->type == DAO_OBJECT ){
		klass = & p[0]->xClass;
		if( p[0]->type == DAO_OBJECT ){
			object = & p[0]->xObject;
			klass = object->defClass;
		}
		lookup = klass->lookupTable;
		index = klass->lookupTable;
	}else if( p[0]->type == DAO_NAMESPACE ){
		ns = & p[0]->xNamespace;
		//index = ns->varIndex; XXX
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
		DString_Delete( name.data );
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
		value = NULL;
		if( lookup ){
			if( st == DAO_OBJECT_VARIABLE && object ){
				value = object->objValues[id];
				vabtp = klass->objDataType->items.pValue[ id ];
			}else if( st == DAO_CLASS_VARIABLE ){
				value = klass->glbData->items.pValue[id];
				vabtp = klass->glbDataType->items.pValue[ id ];
			}else if( st == DAO_OBJECT_VARIABLE ){
				vabtp = klass->objDataType->items.pValue[ id ];
			}
		}else{
			value = ns->varData->items.pValue[id];
			vabtp = ns->varType->items.pValue[ id ];
		}
		DaoValue_Copy( value, tuple->items );
		DaoValue_Copy( vabtp, tuple->items + 1 );
		DString_Assign( name.data, node->key.pString );
		DaoMap_Insert( map, (DaoValue*) & name, (DaoValue*) & tuple );
	}
	DString_Delete( name.data );
}
static void REFL_Cst2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoTuple *tuple = DaoTuple_New( 2 );
	DaoNamespace *ns = proc->activeNamespace;
	DaoClass *klass = NULL;
	DNode *node;
	DString *name = p[1]->xString.data;
	DaoValue *type = NULL;
	DaoValue **value = NULL;
	if( p[0]->type == DAO_CLASS || p[0]->type == DAO_OBJECT ){
		klass = & p[0]->xClass;
		if( p[0]->type == DAO_OBJECT ) klass = p[0]->xObject.defClass;
		node = DMap_Find( klass->lookupTable, name );
		if( node && LOOKUP_ST( node->value.pSize ) == DAO_CLASS_CONSTANT ){
			value = klass->cstData->items.pValue + LOOKUP_ID( node->value.pSize );
			type = (DaoValue*) DaoNamespace_GetType( ns, *value );
		}
	}else if( p[0]->type == DAO_NAMESPACE ){
		DaoNamespace *ns2 = & p[0]->xNamespace;
		return; //XXX
		//node = DMap_Find( ns2->cstIndex, name );
		if( node ){
			value = ns2->cstData->items.pValue + node->value.pInt;
			type = (DaoValue*) DaoNamespace_GetType( ns, *value );
		}
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
	}
	DaoValue_Copy( *value, tuple->items );
	DaoValue_Copy( type, tuple->items + 1 );
	DaoProcess_PutValue( proc, (DaoValue*) tuple );
	if( N >2 ){
		DaoType *tp = DaoNamespace_GetType( ns, p[2] );
		if( proc->vmSpace->options & DAO_EXEC_SAFE ){
			DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
			return;
		}
		if( type ){
			if( DaoType_MatchTo( tp, (DaoType*) type, NULL ) ==0 ){
				DaoProcess_RaiseException( proc, DAO_ERROR, "type not matched" );
				return;
			}
		}
		DaoValue_Copy( p[2], value );
	}
}
static void REFL_Var2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoTuple *tuple = DaoTuple_New( 2 );
	DaoNamespace *ns = proc->activeNamespace;
	DaoClass *klass = NULL;
	DNode *node;
	DString *name = p[1]->xString.data;
	DaoValue *type = NULL;
	DaoValue **value = NULL;
	if( p[0]->type == DAO_CLASS || p[0]->type == DAO_OBJECT ){
		DaoObject *object = NULL;
		klass = & p[0]->xClass;
		if( p[0]->type == DAO_OBJECT ){
			klass = p[0]->xObject.defClass;
		}
		node = DMap_Find( klass->lookupTable, name );
		if( node && LOOKUP_ST( node->value.pSize ) == DAO_CLASS_VARIABLE ){
			value = klass->cstData->items.pValue + LOOKUP_ID( node->value.pSize );
			type = klass->glbDataType->items.pValue[ LOOKUP_ID( node->value.pSize ) ];
		}else if( object && node && LOOKUP_ST( node->value.pSize ) == DAO_OBJECT_VARIABLE ){
			value = object->objValues + LOOKUP_ID( node->value.pSize );
			type = klass->objDataType->items.pValue[ LOOKUP_ID( node->value.pSize ) ];
		}else{
			DaoProcess_RaiseException( proc, DAO_ERROR, "invalid field" );
			return;
		}
	}else if( p[0]->type == DAO_NAMESPACE ){
		DaoNamespace *ns2 = & p[0]->xNamespace;
		return; //XXX
		//node = DMap_Find( ns2->varIndex, name );
		if( node ){
			value = ns2->varData->items.pValue + node->value.pInt;
			type = ns2->varType->items.pValue[ node->value.pInt ];
		}
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
		return;
	}
	DaoValue_Copy( *value, tuple->items );
	DaoValue_Copy( type, tuple->items + 1 );
	DaoProcess_PutValue( proc, (DaoValue*) tuple );
	if( N >2 ){
		DaoType *tp = DaoNamespace_GetType( ns, p[2] );
		if( proc->vmSpace->options & DAO_EXEC_SAFE ){
			DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
			return;
		}
		if( type ){
			if( DaoType_MatchTo( tp, (DaoType*) type, NULL ) ==0 ){
				DaoProcess_RaiseException( proc, DAO_ERROR, "type not matched" );
				return;
			}
		}
		DaoValue_Copy( p[2], value );
	}
}
static void REFL_Routine( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *list;
	DaoValue *item;
	int i;
	if( N ==1 ){ // XXX
		DaoFunctree *rout = & p[0]->xFunctree;
		list = DaoProcess_PutList( proc );
		if( p[0]->type != DAO_FUNCTREE ){
			DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
			return;
		}
		for(i=0; i<rout->routines->size; i++){
			item = rout->routines->items.pValue[i];
			DaoList_Append( list, item );
		}
	}else{
		DaoProcess_PutValue( proc, (DaoValue*) proc->activeRoutine );
	}
}
static void REFL_Class( DaoProcess *proc, DaoValue *p[], int N )
{
#if 0
	if( p[0]->type == DAO_ROUTINE && p[0]->v.routine->tidHost == DAO_OBJECT ){
		DaoProcess_PutValue( proc, (DaoValue*) p[0]->v.routine->routHost->aux.v.klass );
	}else if( p[0]->type == DAO_OBJECT ){
		DaoProcess_PutValue( proc, (DaoValue*) p[0]->v.object->defClass );
	}
#endif
	DaoProcess_PutValue( proc, dao_none_value );
}
static void REFL_Isa( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns = proc->activeNamespace;
	dint *res = DaoProcess_PutInteger( proc, 0 );
	if( p[1]->type == DAO_TYPE ){
		if( DaoType_MatchValue( & p[1]->xType, p[0], NULL ) ) *res = 1;
	}else if( p[1]->type == DAO_CLASS ){
		if( p[0]->type != DAO_OBJECT ) return;
		*res = DaoClass_ChildOf( p[0]->xObject.rootObject->defClass, p[1] );
	}else if( p[1]->type == DAO_CDATA ){
		if( p[0]->type == DAO_OBJECT ){
			*res = DaoClass_ChildOf( p[0]->xObject.rootObject->defClass, p[1] );
		}else if( p[0]->type == DAO_CDATA ){
			*res = DaoCdata_ChildOf( p[0]->xCdata.ctype->kernel->typer, p[1]->xCdata.ctype->kernel->typer );
		}
	}else if( p[1]->type == DAO_STRING ){
		DString *tname = p[1]->xString.data;
		DString_ToMBS( tname );
		if( strcmp( tname->mbs, "class" ) ==0 ){
			if( p[0]->type == DAO_CLASS  ) *res = 1;
		}else if( strcmp( tname->mbs, "object" ) ==0 ){
			if( p[0]->type == DAO_OBJECT  ) *res = 1;
		}else if( strcmp( tname->mbs, "routine" ) ==0 ){
			if( p[0]->type == DAO_ROUTINE  ) *res = 1;
		}else if( strcmp( tname->mbs, "function" ) ==0 ){
			if( p[0]->type == DAO_FUNCTION  ) *res = 1;
		}else if( strcmp( tname->mbs, "namespace" ) ==0 ){
			if( p[0]->type == DAO_NAMESPACE  ) *res = 1;
		}else if( strcmp( tname->mbs, "tuple" ) ==0 ){
			if( p[0]->type == DAO_TUPLE  ) *res = 1;
		}else if( strcmp( tname->mbs, "list" ) ==0 ){
			if( p[0]->type == DAO_LIST  ) *res = 1;
		}else if( strcmp( tname->mbs, "map" ) ==0 ){
			if( p[0]->type == DAO_MAP  ) *res = 1;
		}else if( strcmp( tname->mbs, "array" ) ==0 ){
			if( p[0]->type == DAO_ARRAY  ) *res = 1;
		}else{
			DaoType *tp = DaoParser_ParseTypeName( tname->mbs, ns, 0 );
			if( tp && DaoType_MatchValue( tp, p[0], NULL ) ) *res = 1;
		}
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
	}
}
static void REFL_Self( DaoProcess *proc, DaoValue *p[], int N )
{
	if( p[0]->type == DAO_OBJECT )
		DaoProcess_PutValue( proc, (DaoValue*) p[0]->xObject.rootObject );
	else
		DaoProcess_PutValue( proc, dao_none_value );
}
static void REFL_Param( DaoProcess *proc, DaoValue *p[], int N )
{
	DRoutine *routine = (DRoutine*) p[0];
	DaoList *list = DaoProcess_PutList( proc );
	DaoTuple *tuple;
	DaoType *routype = routine->routType;
	DaoType *itp = list->unitype->nested->items.pType[0];
	DaoType **nested = routype->nested->items.pType;
	DString *mbs = DString_New(1);
	DNode *node;
	DaoString str = {DAO_STRING,0,0,0,0,NULL};
	DaoInteger num = {DAO_INTEGER,0,0,0,0,1};
	int i;
	str.data = mbs;
	for(i=0; i<routine->parCount; i++){
		if( i >= routype->nested->size ) break;
		tuple = DaoTuple_New( 4 );
		tuple->unitype = itp;
		GC_IncRC( itp );
		num.value = 0;
		if( nested[i]->tid == DAO_PAR_DEFAULT ) num.value = 1;
		DaoValue_Copy( (DaoValue*) & str, & tuple->items[0] );
		DaoValue_Copy( (DaoValue*) nested[i], & tuple->items[1] );
		DaoValue_Copy( (DaoValue*) & num, & tuple->items[2] );
		DaoValue_Copy( routine->routConsts->items.pValue[i], & tuple->items[3] );
		DaoList_Append( list, (DaoValue*) tuple );
	}
	DString_Delete( mbs );
	if( routype->mapNames ){
		node = DMap_First( routype->mapNames );
		for( ; node !=NULL; node = DMap_Next( routype->mapNames, node ) ){
			i = node->value.pInt;
			mbs = list->items.items.pValue[i]->xTuple.items[0]->xString.data;
			DString_Assign( mbs, node->key.pString );
		}
	}
}
static void REFL_Argc( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutInteger( proc, proc->topFrame->parCount );
}
static void REFL_Argv( DaoProcess *proc, DaoValue *p[], int N )
{
	int i;
	if( N ==0 ){
		DaoList *list = DaoProcess_PutList( proc );
		for(i=0; i<proc->topFrame->parCount; i++) DaoList_Append( list, proc->activeValues[i] );
	}else{
		DaoValue *val = dao_none_value;
		if( p[0]->xInteger.value < proc->topFrame->parCount )
			val = proc->activeValues[ p[0]->xInteger.value ];
		DaoProcess_PutValue( proc, val );
	}
}
static void REFL_Trace( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *backtrace = DaoProcess_PutList( proc );
	DaoStackFrame *frame = proc->topFrame;
	int instr = 0, depth = 1;
	int maxDepth = 0;
	int print = 0;
	DaoTuple *entry = NULL;
	DaoValue *vRoutType;
	DaoString routName = {DAO_STRING,0,0,0,0,NULL};
	DaoString nsName = {DAO_STRING,0,0,0,0,NULL};
	DaoInteger line = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger inst = {DAO_INTEGER,0,0,0,0,0};

	if( N >=1 ) print = p[0]->xEnum.value;
	if( N ==2 ) maxDepth = p[1]->xInteger.value;

	if( print ){
		DaoProcess_Trace( proc, maxDepth );
		return;
	}

#if 0
	for( ; frame && frame->context ; frame = frame->prev, ++depth ){
		/* Check if we got deeper than requested */
		if( depth > maxDepth && maxDepth > 0 ) break;

		/* Gather some of the informations we need. */
		vRoutType = (DaoValue*) frame->context->routine->routType;
		inst.value = (depth==1) ? (int)( proc->activeCode - proc->codes ) : frame->entry;
		line.value = frame->context->routine->annotCodes->items.pVmc[inst.value]->line;
		routName.data = frame->context->routine->routName;
		nsName.data = frame->context->nameSpace->name;

		/* Put all the informations into a tuple which we append to the list. */
		/* Tuple type: tuple<rout_name:string,rout_type:any,line:int,namespace:string> */
		/* Also, namespace is most often the current file name, but not always! */
		entry = DaoTuple_New( 5 );
		entry->unitype = backtrace->unitype->nested->items.pType[0];
		GC_IncRC( entry->unitype );

		DaoTuple_SetItem( entry, (DaoValue*) & routName, 0 );
		DaoTuple_SetItem( entry, (DaoValue*) vRoutType, 1 );
		DaoTuple_SetItem( entry, (DaoValue*) & inst, 2 );
		DaoTuple_SetItem( entry, (DaoValue*) & line, 3 );
		DaoTuple_SetItem( entry, (DaoValue*) & nsName, 4 );

		DaoList_PushBack( backtrace, (DaoValue*) entry );
	}
#endif
}
static void REFL_Doc( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *doc = NULL;
	switch( p[0]->type ){
	case DAO_CLASS : doc = p[0]->xClass.classHelp; break;
	case DAO_OBJECT : doc = p[0]->xObject.defClass->classHelp; break;
	//XXX case DAO_ROUTINE : doc = p[0]->v.routine->routHelp; break;
	default : break;
	}
	if( doc == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "documentation not available" );
		return;
	}
	DaoProcess_PutMBString( proc, doc->mbs );
	if( N >1 ){
		DString_Clear( doc );
		DString_Append( doc, p[1]->xString.data );
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
	{ REFL_Trace,   "trace( action:enum<generate,print>=$generate, depth=0 ) => list<tuple<rout_name:string,rout_type:any,instr:int,line:int,namespace:string>>" },
	{ NULL, NULL }
};

DaoTypeBase libReflectTyper = {
	"reflect", NULL, NULL, reflMeths, {0}, {0}, NULL, NULL
};

/**/
static void MATH_abs( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, fabs( p[0]->xDouble.value ) );
}
static void MATH_acos( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, acos( p[0]->xDouble.value ) );
}
static void MATH_asin( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, asin( p[0]->xDouble.value ) );
}
static void MATH_atan( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, atan( p[0]->xDouble.value ) );
}
static void MATH_ceil( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, ceil( p[0]->xDouble.value ) );
}
static void MATH_cos( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, cos( p[0]->xDouble.value ) );
}
static void MATH_cosh( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, cosh( p[0]->xDouble.value ) );
}
static void MATH_exp( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, exp( p[0]->xDouble.value ) );
}
static void MATH_floor( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, floor( p[0]->xDouble.value ) );
}
static void MATH_log( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, log( p[0]->xDouble.value ) );
}
static void MATH_sin( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, sin( p[0]->xDouble.value ) );
}
static void MATH_sinh( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, sinh( p[0]->xDouble.value ) );
}
static void MATH_sqrt( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, sqrt( p[0]->xDouble.value ) );
}
static void MATH_tan( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, tan( p[0]->xDouble.value ) );
}
static void MATH_tanh( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, tanh( p[0]->xDouble.value ) );
}
static void MATH_rand( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, p[0]->xDouble.value * rand() / ( RAND_MAX + 1.0 ) );
}
static void MATH_srand( DaoProcess *proc, DaoValue *p[], int N )
{
	srand( (unsigned int)p[0]->xDouble.value );
}
static void MATH_rand_gaussian( DaoProcess *proc, DaoValue *p[], int N )
{
	static int iset = 0;
	static double gset;
	double fac, rsq, v1, v2;
	double R = p[0]->xDouble.value;

	if( iset ==0 ){
		do{
			v1 = 2.0 * ( rand() / (RAND_MAX+1.0) ) -1.0;
			v2 = 2.0 * ( rand() / (RAND_MAX+1.0) ) -1.0;
			rsq = v1*v1 + v2*v2 ;
		} while( rsq >= 1.0 || rsq == 0.0 );
		fac = sqrt( -2.0 * log( rsq ) / rsq );
		gset = v1*fac;
		iset = 1;
		DaoProcess_PutDouble( proc, R*v2*fac );
	} else {
		iset = 0;
		DaoProcess_PutDouble( proc, R*gset );
	}
}
static void MATH_pow( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, pow( p[0]->xDouble.value, p[1]->xDouble.value ) );
}

/**/
static void MATH_abs_c( DaoProcess *proc, DaoValue *p[], int N )
{
	complex16 com = p[0]->xComplex.value;
	DaoProcess_PutDouble( proc, sqrt( com.real * com.real + com.imag * com.imag ) );
}
static void MATH_arg_c( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, arg_c( p[0]->xComplex.value ) );
}
static void MATH_norm_c( DaoProcess *proc, DaoValue *p[], int N )
{
	complex16 com = p[0]->xComplex.value;
	DaoProcess_PutDouble( proc, com.real * com.real + com.imag * com.imag );
}
static void MATH_imag_c( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, p[0]->xComplex.value.imag );
}
static void MATH_real_c( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutDouble( proc, p[0]->xComplex.value.real );
}

static void MATH_cos_c( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutComplex( proc, cos_c( p[0]->xComplex.value ) );
}
static void MATH_cosh_c( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutComplex( proc, cosh_c( p[0]->xComplex.value ) );
}
static void MATH_exp_c( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutComplex( proc, exp_c( p[0]->xComplex.value ) );
}
static void MATH_log_c( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutComplex( proc, log_c( p[0]->xComplex.value ) );
}
static void MATH_sin_c( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutComplex( proc, sin_c( p[0]->xComplex.value ) );
}
static void MATH_sinh_c( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutComplex( proc, sinh_c( p[0]->xComplex.value ) );
}
static void MATH_sqrt_c( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutComplex( proc, sqrt_c( p[0]->xComplex.value ) );
}
static void MATH_tan_c( DaoProcess *proc, DaoValue *p[], int N )
{
	complex16 com = p[0]->xComplex.value;
	complex16 *res = DaoProcess_PutComplex( proc, com );
	complex16 R = sin_c( com );
	complex16 L = cos_c( com );
	res->real = ( L.real*R.real + L.imag*R.imag ) / ( R.real*R.real + R.imag*R.imag );
	res->imag = ( L.imag*R.real - L.real*R.imag ) / ( R.real*R.real + R.imag*R.imag );
}
static void MATH_tanh_c( DaoProcess *proc, DaoValue *p[], int N )
{
	complex16 com = p[0]->xComplex.value;
	complex16 *res = DaoProcess_PutComplex( proc, com );
	complex16 R = sinh_c( com );
	complex16 L = cosh_c( com );
	res->real = ( L.real*R.real + L.imag*R.imag ) / ( R.real*R.real + R.imag*R.imag );
	res->imag = ( L.imag*R.real - L.real*R.imag ) / ( R.real*R.real + R.imag*R.imag );
}
static void MATH_ceil_c( DaoProcess *proc, DaoValue *p[], int N )
{
	complex16 com = p[0]->xComplex.value;
	complex16 *res = DaoProcess_PutComplex( proc, com );
	res->real = ceil( com.real );
	res->imag = ceil( com.imag );
}
static void MATH_floor_c( DaoProcess *proc, DaoValue *p[], int N )
{
	complex16 com = p[0]->xComplex.value;
	complex16 *res = DaoProcess_PutComplex( proc, com );
	res->real = floor( com.real );
	res->imag = floor( com.imag );
}

static void MATH_pow_rc( DaoProcess *proc, DaoValue *p[], int N )
{
	complex16 com = { 0, 0 };
	complex16 *res = DaoProcess_PutComplex( proc, com );
	double lg = log( p[0]->xDouble.value );
	com.real = lg * p[1]->xComplex.value.real;
	com.imag = lg * p[1]->xComplex.value.imag;
	*res = exp_c( com );
}
static void MATH_pow_cr( DaoProcess *proc, DaoValue *p[], int N )
{
	complex16 com2 = { 0, 0 };
	complex16 *res = DaoProcess_PutComplex( proc, com2 );
	complex16 com = log_c( p[0]->xComplex.value );
	double v = p[1]->xDouble.value;
	com2.real = v * com.real;
	com2.imag = v * com.imag;
	*res = exp_c( com2 );
}
static void MATH_pow_cc( DaoProcess *proc, DaoValue *p[], int N )
{
	complex16 com2 = {0,0};
	complex16 *res = DaoProcess_PutComplex( proc, com2 );
	complex16 com = log_c( p[0]->xComplex.value );
	COM_MUL( com2, com, p[1]->xComplex.value );
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
	"math", NULL, NULL, mathMeths, {0}, {0}, NULL, NULL
};


