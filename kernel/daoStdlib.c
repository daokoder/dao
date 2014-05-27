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
	char *path = DString_GetData( p[0]->xString.value );
	switch( p[1]->xEnum.value ){
	case 0 : DaoVmSpace_SetPath( proc->vmSpace, path ); break;
	case 1 : DaoVmSpace_AddPath( proc->vmSpace, path ); break;
	case 2 : DaoVmSpace_DelPath( proc->vmSpace, path ); break;
	}
}
static void STD_Compile( DaoProcess *proc, DaoValue *p[], int N )
{
	char *source = DaoValue_TryGetChars( p[0] );
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
	char *source = DaoValue_TryGetChars( p[0] );
	if( redirect != prevStream ){
		GC_ShiftRC( redirect, proc->stdioStream );
		proc->stdioStream = redirect;
	}

	DaoProcess_Eval( proc, ns, source );
	DaoProcess_PutValue( proc, proc->stackValues[0] );
	if( redirect != prevStream ){
		GC_ShiftRC( prevStream, proc->stdioStream );
		proc->stdioStream = prevStream;
	}
}
static void STD_Load( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns;
	DaoVmSpace *vms = proc->vmSpace;
	DString *name = p[0]->xString.value;
	int import = p[1]->xInteger.value;
	int runim = p[2]->xInteger.value;
	int res = 0;

	DArray_PushFront( vms->pathLoading, proc->activeNamespace->path );
	ns = DaoVmSpace_LoadEx( vms, DString_GetData( name ), runim );
	DaoProcess_PutValue( proc, (DaoValue*) ns );
	if( ns == NULL ) DaoProcess_RaiseError( proc, NULL, "loading failed" );
	DArray_PopFront( vms->pathLoading );
	if( import && ns ) DaoNamespace_AddParent( proc->activeNamespace, ns );
}
int DaoVmSpace_ReadSource( DaoVmSpace *self, DString *fname, DString *source );
static void STD_Resource( DaoProcess *proc, DaoValue *p[], int N )
{
	FILE *fin;
	DString *file = DString_Copy( p[0]->xString.value );
	if( DaoVmSpace_SearchResource( proc->vmSpace, file ) == 0 ){
		DaoProcess_RaiseError( proc, NULL, "resource file not found" );
		DString_Delete( file );
		return;
	}
	DaoVmSpace_ReadSource( proc->vmSpace, file, file );
	DaoProcess_PutString( proc, file );
	DString_Delete( file );
}
/* modules/debugger */
DAO_DLL void Dao_AboutVar( DaoNamespace *ns, DaoValue *var, DString *str )
{
	DaoType *abtp = DaoNamespace_GetType( ns, var );
	char buf[50];
	if( abtp ){
		if( var->type == DAO_ROUTINE ){
			DString_Append( str, var->xRoutine.routName );
			DString_AppendChars( str, "{" );
			DString_Append( str, abtp->name );
			DString_AppendChars( str, "}" );
		}else{
			DString_Append( str, abtp->name );
		}
		sprintf( buf, "[%p]", var );
		DString_AppendChars( str, buf );
		if( var->type == DAO_CDATA ){
			sprintf( buf, "(%p)", var->xCdata.data );
			DString_AppendChars( str, buf );
		}
	}else{
		DString_AppendChars( str, "NULL" );
	}
}
static void Dao_AboutVars( DaoNamespace *ns, DaoValue *par[], int N, DString *str )
{
	int i;
	DString_Clear( str );
	for( i=0; i<N; i++ ){
		Dao_AboutVar( ns, par[i], str );
		if( i+1<N ) DString_AppendChars( str, " " );
	}
}
static void STD_About( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *str = DaoProcess_PutChars( proc, "" );
	Dao_AboutVars( proc->activeNamespace, p, N, str );
}

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
		DaoStream_WriteChars( stream, "()" );
		DaoStream_SetColor( stream, NULL, NULL );
		DaoStream_WriteChars( stream, ": " );
		DaoStream_SetColor( stream, "green", NULL );
		if( routine->routType ) DaoStream_WriteString( stream, routine->routType->name );
		DaoStream_SetColor( stream, NULL, NULL );

		if( frame->routine->body ){
			k = (i==1) ? (int)( self->activeCode - frame->codes ) : frame->entry;
			if( k >= 0 && k < frame->routine->body->annotCodes->size ){
				DaoStream_WriteChars( stream, ", instruction " );
				DaoStream_WriteInt( stream, k );
				DaoStream_WriteChars( stream, " at line " );
				DaoStream_WriteInt( stream, frame->routine->body->annotCodes->items.pVmc[k]->line );
			}
		}

		DaoStream_WriteChars( stream, " in " );
		DaoStream_WriteString( stream, routine->nameSpace->name );
		DaoStream_WriteChars( stream, ";" );
		DaoStream_WriteNewLine( stream );
		frame = frame->prev;
	}
}

void STD_Debug( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoDebugger *debugger = proc->vmSpace->debugger;
	DaoRoutine *routine = proc->activeRoutine;
	DaoStream *stream = proc->vmSpace->stdioStream;
	DString *input;
	if( ! (proc->vmSpace->options & DAO_OPTION_DEBUG ) ) return;
	input = DString_New();
	if( N > 0 && DaoValue_CastCstruct( p[0], dao_type_stream ) ){
		stream = (DaoStream*)p[0];
		p ++;
		N --;
	}
	if( N > 0 ){
		Dao_AboutVars( proc->activeNamespace, p, N, input );
		DaoStream_WriteString( stream, input );
		DaoStream_WriteChars( stream, "\n" );
		DString_Delete( input );
		return;
	}
	DString_Delete( input );
	if( debugger && debugger->Debug ) debugger->Debug( debugger, proc, stream );
}

#ifndef CHANGESET_ID
#define CHANGESET_ID "Undefined"
#endif

const char *const dao_version = "Dao " DAO_VERSION " (" CHANGESET_ID ", " __DATE__ ")";

static void STD_Version( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutChars( proc, p[0]->xInteger.value ? dao_version : DAO_VERSION );
}


DaoFuncItem dao_std_methods[] =
{
	{ STD_Path,      "path( path: string, action: enum<set,add,remove> = $add )" },
	{ STD_Compile,   "compile( source: string, ns: any = none ) => tuple<ns:any,main:routine>" },
	{ STD_Eval,      "eval( source: string, st = io::stdio ) => any" },
	{ STD_Load,      "load( file: string, import = 1, runim = 0 ) => any" },
	{ STD_Resource,  "resource( path: string ) => string" },
	{ STD_About,     "about( invar ... : any ) => string" },
	{ STD_Debug,     "debug( invar ... : any )" },
	{ STD_Version,   "version( verbose = 0 ) => string" },
	{ NULL, NULL }
};

