/*
// Dao Virtual Machine
// http://daoscript.org
//
// Copyright (c) 2006-2017, Limin Fu
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

#ifndef CHANGESET_ID
#define CHANGESET_ID "Undefined"
#endif

const char *const dao_version = "Dao " DAO_VERSION " (" CHANGESET_ID ", " __DATE__ ")";

static void DaoSTD_Version( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutChars( proc, p[0]->xBoolean.value ? dao_version : DAO_VERSION );
}

static void DaoSTD_Path( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoVmSpace *vms = proc->vmSpace;
	DaoNamespace *ns = proc->activeNamespace;
	dao_boolean full = p[1]->xBoolean.value;
	switch( p[0]->xEnum.value ){
	case 0: DaoProcess_PutString( proc, full ? vms->daoBinFile : vms->daoBinPath ); break;
	case 1: DaoProcess_PutString( proc, full ? ns->name : ns->path ); break;
	case 2: DaoProcess_PutString( proc, vms->startPath ); break;
	case 3: DaoProcess_PutString( proc, ns->path ); break;
	}
}

static void DaoSTD_Eval( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoValue *value;
	DaoVmSpace *vms = proc->vmSpace;
	DaoNamespace *ns = DaoNamespace_New( vms, "std.eval()" );
	DaoStream *prevStream = proc->stdioStream;
	DaoStream *redirect = prevStream;
	DString *source = p[0]->xString.value;

	if( p[1]->type != DAO_NONE ) redirect = (DaoStream*) p[1];
	if( redirect != prevStream ) GC_Assign( & proc->stdioStream, redirect );

	GC_IncRC( ns );
	DaoNamespace_AddParent( ns, proc->activeNamespace );

	value = DaoProcess_Eval( proc, ns, source->chars );
	GC_DecRC( ns );
	if( value == NULL ){
		DaoProcess_RaiseError( proc, NULL, "evaluation failed" );
		return;
	}
	DaoProcess_PutValue( proc, value );
	if( redirect != prevStream ) GC_Assign( & proc->stdioStream, prevStream );
}

static void DaoSTD_Eval2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoValue *value;
	DaoVmSpace *vms = proc->vmSpace;
	DaoNamespace *ns = (DaoNamespace*) p[1];
	DaoStream *prevStream = proc->stdioStream;
	DaoStream *redirect = prevStream;
	DString *source = p[0]->xString.value;

	if( p[2]->type != DAO_NONE ) redirect = (DaoStream*) p[2];
	if( redirect != prevStream ) GC_Assign( & proc->stdioStream, redirect );

	value = DaoProcess_Eval( proc, ns, source->chars );
	if( value == NULL ){
		DaoProcess_RaiseError( proc, NULL, "evaluation failed" );
		return;
	}
	DaoProcess_PutValue( proc, value );
	if( redirect != prevStream ) GC_Assign( & proc->stdioStream, prevStream );
}

static void DaoSTD_Compile( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *source = p[0]->xString.value;
	DaoNamespace *import = DaoValue_CastNamespace( p[1] );
	DaoNamespace *nspace = DaoNamespace_New( proc->vmSpace, "std.compile()" );
	DaoProcess_PutValue( proc, (DaoValue*) nspace );
	if( import != NULL ) DaoNamespace_AddParent( nspace, import );
	nspace = DaoProcess_Compile( proc, nspace, source->chars );
	if( nspace == NULL ) DaoProcess_RaiseError( proc, NULL, "compiling failed" );
}
static void DaoSTD_Load( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns;
	DaoVmSpace *vms = proc->vmSpace;
	DString *name = p[0]->xString.value;
	int import = p[1]->xBoolean.value;
	int runim = p[2]->xBoolean.value;
	int res = 0;

	runim = runim ? DAO_MODULE_MAIN_ALWAYS : DAO_MODULE_MAIN_ONCE;

	DList_PushFront( vms->pathLoading, proc->activeNamespace->path );
	ns = DaoVmSpace_LoadEx( vms, DString_GetData( name ), runim );
	if( ns != NULL ){
		DaoProcess_PutValue( proc, (DaoValue*) ns );
	}else{
		DaoProcess_RaiseError( proc, NULL, "loading failed" );
	}
	DList_PopFront( vms->pathLoading );
	if( import && ns ) DaoNamespace_AddParent( proc->activeNamespace, ns );
}
static void DaoSTD_Resource( DaoProcess *proc, DaoValue *p[], int N )
{
	FILE *fin;
	DString *file = DString_Copy( p[0]->xString.value );
	if( DaoVmSpace_SearchResource( proc->vmSpace, file, proc->activeNamespace->path ) == 0 ){
		DString_InsertChars( file, "resource file not found: ", 0, 0, -1 );
		DaoProcess_RaiseError( proc, NULL, file->chars );
		DString_Delete( file );
		return;
	}
	DaoVmSpace_ReadFile( proc->vmSpace, file, file );
	DaoProcess_PutString( proc, file );
	DString_Delete( file );
}
/* modules/debugger */
DAO_DLL void Dao_AboutVar( DaoProcess *proc, DaoType *type, DaoValue *var, DString *str )
{
	DaoType *valtype = DaoNamespace_GetType( proc->activeNamespace, var );
	char buf[50];
	if( valtype ){
		if( var->type == DAO_ROUTINE ){
			DString_Append( str, var->xRoutine.routName );
			DString_AppendChars( str, "{" );
			DString_Append( str, valtype->name );
			DString_AppendChars( str, "}" );
		}else{
			DString_Append( str, valtype->name );
		}
		sprintf( buf, "[%p]", var );
		DString_AppendChars( str, buf );
		if( var->type == DAO_CDATA ){
			sprintf( buf, "(%p)", var->xCdata.data );
			DString_AppendChars( str, buf );
		}
	}else{
		DString_AppendChars( str, "none[0x0]" );
	}
	if( type ){
		DString_AppendChars( str, ":" );
		DString_Append( str, type->name );
	}
}
static void Dao_AboutVars( DaoProcess *proc, DaoValue *par[], int N, DString *str )
{
	DaoVmCode *vmc = proc->activeCode;
	DaoType **types = proc->activeTypes + vmc->a + 1;
	int i;
	DString_Clear( str );
	if( vmc->code == DVM_MCALL ) types += 1;
	for( i=0; i<N; i++ ){
		Dao_AboutVar( proc, types[i], par[i], str );
		if( i+1<N ) DString_AppendChars( str, " " );
	}
}

static void DaoSTD_About( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *str = DaoProcess_PutChars( proc, "" );
	Dao_AboutVars( proc, p, N, str );
}

static void DaoSTD_Note( DaoProcess *proc, DaoValue *p[], int N )
{
	int i;
	DaoVmSpace *vmspace = proc->vmSpace;

	if( vmspace->handler == NULL ) return;
	if( vmspace->handler->PrintNote == NULL ) return;

	for(i=0; i<N; ++i){
		vmspace->handler->PrintNote( vmspace->handler, p[i] );
	}
}

void DaoProcess_Trace( DaoProcess *self, int depth )
{
	DaoStream *stream = self->vmSpace->stdioStream;
	DaoStackFrame *frame = self->topFrame;
	int k, i = 0;
	while( frame && frame->routine ){
		DaoRoutine *routine = frame->routine;
		if( depth && ++i > depth ) break;

		DaoStream_SetColor( stream, "black", "green" );
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

void DaoSTD_Debug( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoDebugger *debugger = proc->vmSpace->debugger;
	DaoStream *stream = proc->vmSpace->stdioStream;
	DString *input;
	if( ! (proc->vmSpace->options & DAO_OPTION_DEBUG ) ) return;
	input = DString_New();
	if( N > 0 && DaoValue_CastCstruct( p[0], proc->vmSpace->typeStream ) ){
		stream = (DaoStream*)p[0];
		p ++;
		N --;
	}
	if( N > 0 ){
		Dao_AboutVars( proc, p, N, input );
		DaoStream_WriteString( stream, input );
		DaoStream_WriteChars( stream, "\n" );
		DString_Delete( input );
		return;
	}
	DString_Delete( input );
	if( debugger && debugger->Debug ) debugger->Debug( debugger, proc, stream );
}

void DaoProcess_TryDebugging( DaoProcess *self );

static void DaoSTD_Warn( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoType *type = DaoVmSpace_MakeExceptionType( proc->vmSpace, "Exception::Warning" );
	DaoException *exception = (DaoException*)DaoException_New( type );
	DaoStream *stream = proc->stdioStream ? proc->stdioStream : proc->vmSpace->stdioStream;
	DaoException_Init( exception, proc, p[0]->xString.value->chars, NULL );
	DaoException_Print( exception, stream );
	DaoException_Delete( exception );
}
static void DaoSTD_Error( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoType *etype = DaoVmSpace_MakeExceptionType( proc->vmSpace, "Exception::Error" );
	DaoException *exception = DaoException_New( etype );

	DaoException_Init( exception, proc, p[0]->xString.value->chars, NULL );
	DList_Append( proc->exceptions, exception );
	DaoProcess_TryDebugging( proc );
}
static void DaoSTD_Error2( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoException *error = (DaoException*) p[0];
	DList_Append( proc->exceptions, error->object ? (void*)error->object : (void*)error );
	DaoProcess_TryDebugging( proc );
}
static void DaoSTD_Error3( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoType *etype = p[0]->xCtype.valueType;
	DaoException *exception = DaoException_New( etype );
	DaoException_Init( exception, proc, p[1]->xString.value->chars, p[2] );
	DList_Append( proc->exceptions, exception );
	DaoProcess_TryDebugging( proc );
}
const char *dao_assertion_format = "Assertion failed at line %i in file \"%s\" with message:\n";
static void DaoSTD_Assert( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoType *etype;
	DaoException *exception;
	DaoRoutine *rout = proc->activeRoutine;
	DString *file = proc->activeNamespace->name;
	int line, id = (ushort_t) (proc->activeCode - proc->topFrame->active->codes);

	if( p[0]->xBoolean.value ) return;

	line = rout->defLine;
	if( id < rout->body->vmCodes->size ) line = rout->body->annotCodes->items.pVmc[id]->line;

	etype = DaoVmSpace_MakeExceptionType( proc->vmSpace, "Error::Assertion" );
	exception = DaoException_New( etype );
	DList_Append( proc->exceptions, exception );

	DaoException_Init( exception, proc, NULL, NULL );

	DString_Reserve( exception->info, file->size + 100 );
	id = sprintf( exception->info->chars, dao_assertion_format, line, file->chars );
	if( id ) exception->info->size = id;
	DString_Append( exception->info, p[1]->xString.value );
	DaoProcess_TryDebugging( proc );
}
static void DaoSTD_Exec( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoVmCode *sect = DaoProcess_InitCodeSection( proc, 0 );
	int ecount = proc->exceptions->size;

	if( sect == NULL ) return;
	DaoProcess_Execute( proc );
	DaoProcess_PopFrame( proc );
	if( proc->exceptions->size > ecount ){
		if( n > 0 ){
			DaoProcess_PutValue( proc, p[0] );
			DList_Erase( proc->exceptions, ecount, -1 );
		}
	}else{
		DaoProcess_PutValue( proc, proc->stackValues[0] );
	}
}
static void DaoSTD_Try( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoVmCode *sect = DaoProcess_InitCodeSection( proc, 0 );
	int i, ecount = proc->exceptions->size;

	if( sect == NULL ) return;
	DaoProcess_Execute( proc );
	DaoProcess_PopFrame( proc );
	if( proc->exceptions->size > (ecount+1) ){
		DaoList *list = DaoProcess_PutList( proc );
		for(i=ecount; i<proc->exceptions->size; ++i){
			DaoList_Append( list, proc->exceptions->items.pValue[i] );
		}
		DList_Erase( proc->exceptions, ecount, -1 );
	}else if( proc->exceptions->size > ecount ){
		DaoProcess_PutValue( proc, proc->exceptions->items.pValue[proc->exceptions->size-1] );
		DList_PopBack( proc->exceptions );
	}else{
		DaoProcess_PutValue( proc, proc->stackValues[0] );
	}
}
static void DaoSTD_ProcData( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoProcess_PutValue( proc, proc->stackValues[1] );
	if( n == 0 ) return;
	DaoValue_Move( p[0], & proc->stackValues[1], NULL );
}
static void DaoSTD_Test( DaoProcess *proc, DaoValue *p[], int n )
{
	printf( "%i\n", p[0]->type );
}

DaoFunctionEntry dao_std_methods[] =
{
	{ DaoSTD_Version,   "version( verbose = false ) => string" },
	{ DaoSTD_Path,
		"path( which: enum<program,script,working,loading> = $script, full = false ) => string"
		/*
		// Path types:
		// -- program: the interpreter path;
		// -- script : the source script path;
		// -- working: the working directory from which the program was started;
		// -- loading: the source directory from which the script was loaded;
		// For program and script paths:
		// -- full = true : include the file name;
		// -- full = false: exclude the file name;
		*/
	},

	{ DaoSTD_Eval,
		"eval( source: string, stream: io::Stream|none = none ) => any"
		/*
		// Evaluate a given source code, and optionally redirect
		// the printing output to a stream. If the source code
		// contains a single expression, the expression's value
		// will be returned as the result of the method.
		*/
	},
	{ DaoSTD_Eval2,
		"eval( source: string, nspace: namespace, stream: io::Stream|none = none ) => any"
		/*
		// Evaluate a given source code in a working namespace,
		// and optionally redirect the printing output to a stream.
		// If the source code contains a single expression,
		// the expression's value will be returned as the result
		// of the method.
		*/
	},
	{ DaoSTD_Compile,   "compile( source: string, import: namespace|none = none ) =>namespace"},
	{ DaoSTD_Load,      "load( file: string, import = true, run = false ) => namespace" },

	{ DaoSTD_Resource,  "resource( path: string ) => string" },
	{ DaoSTD_About,     "about( invar ... : any ) => string" },
	{ DaoSTD_Note,      "note( invar ... : any )" },
	{ DaoSTD_Debug,     "debug( invar ... : any )" },

	{ DaoSTD_ProcData,  "procdata( ) => any" },
	{ DaoSTD_ProcData,  "procdata( data: any ) => any" },

	{ DaoSTD_Warn,
		"warn( info: string )"
		/*
		// Raise a warning with message "info".
		*/
	},
	{ DaoSTD_Error,
		"error( info: string )"
		/*
		// Raise an error with message "info";
		// The exception for the error will be an instance of Exception::Error.
		*/
	},
	{ DaoSTD_Error2,
		"error( errorObject: Error )"
		/*
		// Raise an error with pre-created exception object.
		*/
	},
	{ DaoSTD_Error3,
		"error( errorType: class<Error>, info: string, data: any = none )"
		/*
		// Raise an error of type "eclass" with message "info", and associate "data"
		// to the error.
		*/
	},
	{ DaoSTD_Assert,
		"assert( condition: bool, message = '' )"
	},
	{ DaoSTD_Exec,
		"exec() [=>@T] => @T"
		/*
		//
		*/
	},
	{ DaoSTD_Exec,
		"exec( defaultValue: @T ) [=>@T] => @T"
		/*
		//
		*/
	},
	{ DaoSTD_Try,
		"try() [=>@T] => list<Error>|Error|@T"
		/*
		//
		*/
	},
#if DEBUG
	{ DaoSTD_Test,      "__test1__( ... )" },
	{ DaoSTD_Test,      "__test2__( ... : int|string )" },
#endif
	{ NULL, NULL }
};

