/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2013, Limin Fu
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

#include"time.h"
#include"string.h"
#include"locale.h"


#include"daoStdlib.h"
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
#include"daoTasklet.h"
#include"daoValue.h"

static void STD_Path( DaoProcess *proc, DaoValue *p[], int N )
{
	char *path = DString_GetMBS( p[0]->xString.data );
	switch( p[1]->xEnum.value ){
	case 0 : DaoVmSpace_SetPath( proc->vmSpace, path ); break;
	case 1 : DaoVmSpace_AddPath( proc->vmSpace, path ); break;
	case 2 : DaoVmSpace_DelPath( proc->vmSpace, path ); break;
	}
}
static void STD_Compile( DaoProcess *proc, DaoValue *p[], int N )
{
	char *source = DaoValue_TryGetMBString( p[0] );
	DaoNamespace *ns = DaoValue_CastNamespace( p[1] );
	DaoTuple *tuple = DaoProcess_PutTuple( proc, 0 );
	if( ns == NULL ) ns = proc->activeNamespace;
	DaoTuple_SetItem( tuple, (DaoValue*) ns, 0 );
	if( DaoProcess_Compile( proc, ns, source ) ==0 ){
		DaoTuple_SetItem( tuple, dao_none_value, 1 );
		return;
	}
	DaoTuple_SetItem( tuple, ns->mainRoutines->items.pValue[ ns->mainRoutines->size-1 ], 1 );
}
static void STD_Eval( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoVmSpace *vms = proc->vmSpace;
	DaoNamespace *ns = proc->activeNamespace;
	DaoStream *prevStream = proc->stdioStream;
	DaoStream *redirect = (DaoStream*) p[1];
	char *source = DaoValue_TryGetMBString( p[0] );
	int safe = p[2]->xInteger.value;
	int wasProt = 0;
	if( vms->options & DAO_OPTION_SAFE ) wasProt = 1;
	if( redirect != prevStream ){
		GC_ShiftRC( redirect, proc->stdioStream );
		proc->stdioStream = redirect;
	}

	if( safe ) vms->options |= DAO_OPTION_SAFE;
	DaoProcess_Eval( proc, ns, source );
	DaoProcess_PutValue( proc, proc->stackValues[0] );
	if( ! wasProt ) vms->options &= ~DAO_OPTION_SAFE;
	if( redirect != prevStream ){
		GC_ShiftRC( prevStream, proc->stdioStream );
		proc->stdioStream = prevStream;
	}
}
static void STD_Load( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *name = p[0]->xString.data;
	int import = p[1]->xInteger.value;
	int runim = p[2]->xInteger.value;
	int safe = p[3]->xInteger.value;
	int wasProt = 0;
	int res = 0;
	DaoVmSpace *vms = proc->vmSpace;
	DaoNamespace *ns;
	DString_ToMBS( name );
	if( safe ) vms->options |= DAO_OPTION_SAFE;
	if( vms->options & DAO_OPTION_SAFE ) wasProt = 1;
	DArray_PushFront( vms->pathLoading, proc->activeNamespace->path );
	ns = DaoVmSpace_LoadEx( vms, DString_GetMBS( name ), runim );
	DaoProcess_PutValue( proc, (DaoValue*) ns );
	if( ! wasProt ) vms->options &= ~DAO_OPTION_SAFE;
	if( ns == NULL ) DaoProcess_RaiseException( proc, DAO_ERROR, "loading failed" );
	DArray_PopFront( vms->pathLoading );
	if( import && ns ) DaoNamespace_AddParent( proc->activeNamespace, ns );
}
int DaoVmSpace_ReadSource( DaoVmSpace *self, DString *fname, DString *source );
static void STD_Resource( DaoProcess *proc, DaoValue *p[], int N )
{
	FILE *fin;
	DString *file = DString_Copy( p[0]->xString.data );
	if( DaoVmSpace_SearchResource( proc->vmSpace, file ) == 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "resource file not found" );
		DString_Delete( file );
		return;
	}
	DaoVmSpace_ReadSource( proc->vmSpace, file, file );
	DaoProcess_PutString( proc, file );
	DString_Delete( file );
}
static void STD_Argv( DaoProcess *proc, DaoValue *p[], int N )
{
	int i;
	DaoList *list = DaoProcess_PutList( proc );
	for(i=0; i<proc->topFrame->parCount; i++) DaoList_Append( list, proc->activeValues[i] );
}
static void Dao_AboutVar( DaoNamespace *ns, DaoValue *var, DString *str )
{
	DaoType *abtp = DaoNamespace_GetType( ns, var );
	char buf[50];
	if( abtp ){
		if( var->type == DAO_ROUTINE ){
			DString_Append( str, var->xRoutine.routName );
			DString_AppendMBS( str, "{" );
			DString_Append( str, abtp->name );
			DString_AppendMBS( str, "}" );
		}else{
			DString_Append( str, abtp->name );
		}
		sprintf( buf, "[%p]", var );
		DString_AppendMBS( str, buf );
		if( var->type == DAO_CDATA ){
			sprintf( buf, "(%p)", var->xCdata.data );
			DString_AppendMBS( str, buf );
		}
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
	daoint *res = DaoProcess_PutInteger( proc, 0 );
	if( p0 == NULL || p0->type == 0 ){
		*res = 0;
		return;
	}
	switch( p0->type ){
	case DAO_CLASS :
	case DAO_ROUTINE :
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
			DaoType *type = p0->xCdata.ctype;
			*res = DaoType_FindFunctionMBS( type, type->typer->name ) != NULL;
			break;
		}
	case DAO_CDATA :
	case DAO_CSTRUCT :
		{
			*res = DaoType_FindFunctionMBS( p0->xCdata.ctype, "()" ) != NULL;
			break;
		}
	default : break;
	}
}

extern void SplitByWhiteSpaces( const char *str, DArray *tokens );

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
	DaoStream *stream = self->vmSpace->stdioStream;
	DaoStackFrame *frame = self->topFrame;
	int k, i = 0;
	while( frame && frame->routine ){
		DaoRoutine *routine = frame->routine;
		if( depth && ++i > depth ) break;

		DaoStream_SetColor( stream, "white", "green" );
		DaoStream_WriteString( stream, routine->routName );
		DaoStream_WriteMBS( stream, "()" );
		DaoStream_SetColor( stream, NULL, NULL );
		DaoStream_WriteMBS( stream, ": " );
		DaoStream_SetColor( stream, "green", NULL );
		if( routine->routType ) DaoStream_WriteString( stream, routine->routType->name );
		DaoStream_SetColor( stream, NULL, NULL );

		if( frame->routine->body ){
			k = (i==1) ? (int)( self->activeCode - frame->codes ) : frame->entry;
			if( k >= 0 && k < frame->routine->body->annotCodes->size ){
				DaoStream_WriteMBS( stream, ", instruction " );
				DaoStream_WriteInt( stream, k );
				DaoStream_WriteMBS( stream, " at line " );
				DaoStream_WriteInt( stream, frame->routine->body->annotCodes->items.pVmc[k]->line );
			}
		}

		DaoStream_WriteMBS( stream, " in " );
		DaoStream_WriteString( stream, routine->nameSpace->name );
		DaoStream_WriteMBS( stream, ";" );
		DaoStream_WriteNewLine( stream );
		frame = frame->prev;
	}
}
void DaoRoutine_FormatCode( DaoRoutine *self, int i, DaoVmCodeX vmc, DString *output );
void STD_Debug( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoUserHandler *handler = proc->vmSpace->userHandler;
	DaoRoutine *routine = proc->activeRoutine;
	DaoStream *stream = proc->vmSpace->stdioStream;
	DString *input;
	DArray *tokens;
	DMap   *cycData;
	char *chs, *cmd;
	int i;
	if( ! (proc->vmSpace->options & DAO_OPTION_DEBUG ) ) return;
	input = DString_New(1);
	if( N > 0 && DaoValue_CastCstruct( p[0], dao_type_stream ) ){
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
		SplitByWhiteSpaces( input->mbs, tokens );
		if( tokens->size == 0 ) continue;
		cmd = tokens->items.pString[0]->mbs;
		if( strcmp( cmd, "q" ) == 0 || strcmp( cmd, "quit" ) == 0 ){
			break;
		}else if( strcmp( cmd, "k" ) == 0 || strcmp( cmd, "kill" ) == 0 ){
			proc->status = DAO_PROCESS_ABORTED;
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
				int entry = proc->activeCode - proc->activeRoutine->body->vmCodes->data.codes;
				if( n < 0 ) n = entry - n;
				if( n >= routine->body->vmCodes->size ) n = routine->body->vmCodes->size -1;
				proc->topFrame->entry = n;
				proc->status = DAO_PROCESS_STACKED;
				return;
			}
		}else if( strcmp( cmd, "h" ) == 0 || strcmp( cmd, "help" ) == 0 ){
			DaoStream_WriteMBS( stream, help );
		}else if( strcmp( cmd, "l" ) == 0 || strcmp( cmd, "list" ) == 0 ){
			DString *mbs = DString_New(1);
			int entry = proc->activeCode - proc->activeRoutine->body->vmCodes->data.codes;
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
			if( end >= routine->body->vmCodes->size ) end = routine->body->vmCodes->size - 1;
			DaoStream_WriteString( stream, routine->routName );
			DaoStream_WriteMBS( stream, "(): " );
			if( routine->routType ) DaoStream_WriteString( stream, routine->routType->name );
			DaoStream_WriteMBS( stream, "\n" );
			DaoStream_WriteMBS( stream, daoRoutineCodeHeader );
			DaoStream_WriteMBS( stream, sep );
			for( i=start; i<=end; i++ ){
				DaoRoutine_FormatCode( routine, i, *routine->body->annotCodes->items.pVmc[i], mbs );
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
static void STD_Gcmax( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutInteger( proc, DaoGC_Max( -1 ) );
	if( proc->vmSpace->options & DAO_OPTION_SAFE ){
		if( N == 1 ) DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( N == 1 ) DaoGC_Max( (int)p[0]->xInteger.value );
}
static void STD_Gcmin( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutInteger( proc, DaoGC_Min( -1 ) );
	if( proc->vmSpace->options & DAO_OPTION_SAFE ){
		if( N == 1 ) DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( N == 1 ) DaoGC_Min( (int)p[0]->xInteger.value );
}
static void STD_SubType( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoType *tp1 = DaoNamespace_GetType( proc->activeNamespace, p[0] );
	DaoType *tp2 = DaoNamespace_GetType( proc->activeNamespace, p[1] );
	DaoProcess_PutInteger( proc, DaoType_MatchTo( tp1, tp2, NULL ) );
}
static void STD_Warn( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_RaiseException( proc, DAO_WARNING, DString_GetMBS( p[0]->xString.data ) );
}
static void STD_Version( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutMBString( proc, DAO_VERSION );
}


static void STD_Iterate( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	daoint i, entry, times = p[0]->xInteger.value;

	if( sect == NULL || times < 0 ) return; // TODO exception
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	DaoProcess_AcquireCV( proc );
	for(i=0; i<times; i++){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_PROCESS_ABORTED ) break;
	}
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
}
static void STD_String( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DString *string = DaoProcess_PutMBString( proc, "" );
	daoint i, entry, size = p[0]->xInteger.value;

	if( p[1]->xEnum.value ) DString_ToWCS( string );
	if( sect == NULL || size < 0 ) return; // TODO exception
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	DaoProcess_AcquireCV( proc );
	for(i=0; i<size; i++){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_PROCESS_ABORTED ) break;
		DString_AppendWChar( string, proc->stackValues[0]->xInteger.value );
	}
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
}
int DaoArray_AlignShape( DaoArray *self, DArray *sidx, daoint *dims, int ndim );
static void STD_Array( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *res, *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoArray *array = DaoProcess_PutArray( proc );
	DaoArray *first = NULL;
	DaoArray *sub = NULL;
	daoint i, j, k, entry, size = 1;

	/* if multi-dimensional array is disabled, DaoProcess_PutArray() will raise exception. */
#ifdef DAO_WITH_NUMARRAY
	for(i=0; i<N; i++){
		daoint d = p[i]->xInteger.value;
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
	DaoProcess_AcquireCV( proc );
	for(i=0; i<size; i++){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_PROCESS_ABORTED ) break;
		res = proc->stackValues[0];
		if( i == 0 ){
			int D = N;
			DaoArray_SetDimCount( array, N + (res->type == DAO_ARRAY ? res->xArray.ndim : 0) );
			for(j=0; j<N; j++) array->dims[j] = p[j]->xInteger.value;
			if( res->type == DAO_ARRAY ){
				first = DaoArray_Copy( (DaoArray*) res );
				if( first->ndim == 2 && (first->dims[0] == 1 || first->dims[1] == 1) ){
					D += 1;
					array->dims[N] = first->dims[ first->dims[0] == 1 ];
				}else{
					D += first->ndim;
					memmove( array->dims + N, first->dims, first->ndim*sizeof(daoint) );
				}
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
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
	if( first ) DaoArray_Delete( first );
#endif
}
static void STD_List( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *res = p[N==2], *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoList *list = DaoProcess_PutList( proc );
	daoint i, entry, size = p[0]->xInteger.value;
	daoint fold = N == 2;

	if( fold ) DaoList_Append( list, res );
	if( sect == NULL || size < 0 ) return; // TODO exception
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	DaoProcess_AcquireCV( proc );
	for(i=fold; i<size; i++){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, index );
		if( sect->b >1 && N ==2 ) DaoProcess_SetValue( proc, sect->a+1, res );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_PROCESS_ABORTED ) break;
		res = proc->stackValues[0];
		DaoList_Append( list, res );
	}
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
}
static void STD_Map( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *res, *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoMap *map = DaoProcess_PutMap( proc, p[1]->xInteger.value );
	daoint i, entry, size = p[0]->xInteger.value;

	if( sect == NULL || size < 0 ) return; // TODO exception
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	DaoProcess_AcquireCV( proc );
	for(i=0; i<size; i++){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_PROCESS_ABORTED ) break;
		res = proc->stackValues[0];
		if( res->type == DAO_TUPLE && res->xTuple.size == 2 )
			DaoMap_Insert( map, res->xTuple.items[0], res->xTuple.items[1] );
	}
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
}

DaoFuncItem dao_std_methods[] =
{
	{ STD_Path,      "path( path :string, action :enum<set,add,remove>=$add )" },
	{ STD_Compile,   "compile( source :string, ns :any= none ) => tuple<ns:any,main:routine>" },
	{ STD_Eval,      "eval( source :string, st=io::stdio, safe=0 )=>any" },
	{ STD_Load,      "load( file :string, import=1, runim=0, safe=0 )=>any" },
	{ STD_Resource,  "resource( path :string )=>string" },
	{ STD_Argv,      "argv() => list<any>" },
	{ STD_About,     "about( ... )=>string" },
	{ STD_Callable,  "callable( object )=>int" },
	{ STD_Debug,     "debug( ... )" },
	{ STD_Warn,      "warn( info :string )" },
	{ STD_Error,     "error( info :string )" },
	{ STD_Gcmax,     "gcmax( limit=0 )=>int" },/*by default, return the current value;*/
	{ STD_Gcmin,     "gcmin( limit=0 )=>int" },
	{ STD_SubType,   "subtype( obj1, obj2 )=>int" },
	{ STD_Version,   "version()=>string" },

	{ STD_Iterate,  "iterate( times :int )[index:int]" },
	{ STD_String,   "string( size :int, type :enum<mbs,wcs>=$mbs )[index:int =>int] =>string" },
	{ STD_Array,    "array( D1 :int, D2 =0, D3 =0 )[I:int, J:int, K:int =>@V<@T<int|float|double|complex>|array<@T>>] =>array<@T>" },
	{ STD_List,     "list( size :int )[index:int =>@T] =>list<@T>" },
	{ STD_List,     "list( size :int, init :@T )[index:int, prev:@T =>@T] =>list<@T>" },
	{ STD_Map,      "map( size :int, hashing = 0 )[index:int =>tuple<@K,@V>] =>map<@K,@V>" },
	{ NULL, NULL }
};



