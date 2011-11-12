// DaoCxxInliner: a Clang based module for Dao to support C/C++ code inlining.
// By Limin Fu.

#include <llvm/Module.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Target/TargetSelect.h>
#include "llvm/LLVMContext.h"
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/ExecutionEngine/Interpreter.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Lex/Token.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/SourceManager.h>
#include <iostream>
#include <string>
#include <vector>

extern "C" {
#include "dao.h"
#include "daoConst.h"
#include "daoString.h"
#include "daoArray.h"
#include "daoValue.h"
#include "daoRoutine.h"
}

DAO_INIT_MODULE

using namespace std;
using namespace llvm;
using namespace clang;

CompilerInstance compiler;
EmitLLVMOnlyAction action;
ExecutionEngine *engine;

extern "C"{
static void DaoCxxInliner_Default( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_RaiseException( proc, DAO_ERROR, "Unsuccessully wrapped function" );
}
}

static void DaoCxxInliner_AddVirtualFile( const char *name, const char *source )
{
	MemoryBuffer* Buffer = llvm::MemoryBuffer::getMemBufferCopy( source, name );
	const FileEntry* FE = compiler.getFileManager().getVirtualFile( name, 
			strlen(Buffer->getBufferStart()), time(NULL) );
	compiler.getSourceManager().overrideFileContents( FE, Buffer );
	compiler.getFrontendOpts().Inputs.push_back( pair<InputKind, std::string>( IK_C, name ) );
}

const char *source_caption_pattern = "^ @{1,2} %[ %s* %w+ %s* %( %s* (|%w+ (|%. %w+)) %s* %)";
const char *header_suffix_pattern = "%. (h | hxx | hpp)";

DaoRegex *source_caption_regex = NULL;
DaoRegex *header_suffix_regex = NULL;

static void dao_cxx_parse( DString *VT, DString *source, DArray *markers )
{
	DString *marker = NULL;
	size_t start = 0, rb = DString_FindChar( VT, ']', 0 );
	DString_SetDataMBS( source, VT->mbs + rb + 1, VT->size - 2*(rb + 1) );
	while( start < source->size && isspace( source->mbs[start] ) ) start += 1;
	while( start < source->size && source->mbs[start] == '@' ){
		size_t end = DString_FindChar( source, '\n', start );
		if( end == (size_t)-1 ) break;
		if( marker == NULL ) marker = DString_New(1);
		DString_SetDataMBS( marker, source->mbs + start, end - start );
		DArray_Append( markers, marker );
		start = end + 1;
		while( start < source->size && isspace( source->mbs[start] ) ) start += 1;
	}
	if( marker ) DString_Delete( marker );
	DString_Erase( source, 0, start );
}
static size_t dao_string_find_paired( DString *mbs, char lb, char rb )
{
	size_t i, count = 0, N = mbs->size;
	for(i=0; i<N; i++){
		if( mbs->mbs[i] == lb ){
			count -= 1;
		}else if( mbs->mbs[i] == rb ){
			count += 1;
			if( count == 0 ) return i;
		}
	}
	return (size_t)-1;
}
static int dao_markers_get( DArray *markers, const char *name, DString *one, DArray *all )
{
	size_t i, m = 0, npos = (size_t)-1;
	for(i=0; i<markers->size; i++){
		DString *marker = markers->items.pString[i];
		size_t lb = DString_FindChar( marker, '(', 0 );
		size_t rb = dao_string_find_paired( marker, '(', ')' );
		if( lb == npos || rb == npos ) continue;
		DString_SetDataMBS( one, marker->mbs + 1, lb - 1 );
		DString_Trim( one );
		if( strcmp( one->mbs, name ) ) continue;
		DString_SetDataMBS( one, marker->mbs + lb + 1, rb - lb - 1 );
		DString_Trim( one );
		DArray_Erase( markers, i, 1 );
		i -= 1;
		m += 1;
		if( all == NULL ) break;
		DArray_Append( all, one );
	}
	return m;
}
const char *dao_wrapper = "( DaoProcess *_proc, DaoValue *_p[], int _n )";
const char *dao_cxx_default_includes =
"#include<stdio.h>\n"
"#include<stdlib.h>\n"
"#include<math.h>\n"
"#include<dao.h>\n";

static int dao_make_wrapper( DString *name, DaoType *routype, DString *cproto, DString *wrapper, DString *cc )
{
	DString *pname;
	DaoType *type, **partypes = routype->nested->items.pType;
	int i, parcount = routype->nested->size;
	char sindex[10];

	DString_Clear( cc );
	DString_Clear( cproto );

	DString_Append( cc, name );
	DString_Append( cproto, name );
	DString_AppendMBS( cc, "( " );
	DString_AppendMBS( cproto, "( " );
	DString_AppendMBS( wrapper, "void dao_" );
	DString_Append( wrapper, name );
	DString_AppendMBS( wrapper, dao_wrapper );
	DString_AppendMBS( wrapper, "\n{\n" );
	for(i=0; i<parcount; i++){
		type = partypes[i];
		if( type->tid != DAO_PAR_NAMED && type->tid != DAO_PAR_DEFAULT ) return 1;
		pname = type->fname;
		type = & type->aux->xType;
		if( i ){
			DString_AppendMBS( cc, ", " );
			DString_AppendMBS( cproto, ", " );
		}
		sprintf( sindex, "%i", i );
		DString_AppendChar( wrapper, '\t' );
		switch( type->tid ){
		case DAO_INTEGER :
		case DAO_FLOAT :
		case DAO_DOUBLE :
			DString_Append( cc, pname );
			DString_Append( cproto, type->name );
			DString_AppendChar( cproto, ' ' );
			DString_Append( cproto, pname );
			DString_Append( wrapper, type->name );
			DString_AppendChar( wrapper, ' ' );
			DString_Append( wrapper, pname );
			switch( type->tid ){
			case DAO_INTEGER : DString_AppendMBS( wrapper, " = DaoValue_TryGetInteger( _p[" ); break;
			case DAO_FLOAT   : DString_AppendMBS( wrapper, " = DaoValue_TryGetFloat( _p[" ); break;
			case DAO_DOUBLE  : DString_AppendMBS( wrapper, " = DaoValue_TryGetDouble( _p[" ); break;
			}
			DString_AppendMBS( wrapper, sindex );
			DString_AppendMBS( wrapper, "] );\n" );
			break;
		case DAO_COMPLEX :
			DString_Append( cc, pname );
			DString_AppendMBS( cproto, "complex16 " );
			DString_Append( cproto, pname );
			DString_AppendMBS( wrapper, "complex16 " );
			DString_Append( wrapper, pname );
			DString_AppendMBS( wrapper, " = DaoValue_TryGetComplex( _p[" );
			DString_AppendMBS( wrapper, sindex );
			DString_AppendMBS( wrapper, "] );\n" );
			break;
		case DAO_STRING :
			DString_Append( cc, pname );
			DString_AppendMBS( cproto, "const char *" );
			DString_Append( cproto, pname );
			DString_AppendMBS( wrapper, "const char *" );
			DString_Append( wrapper, pname );
			DString_AppendMBS( wrapper, " = DaoValue_TryGetMBString( _p[" );
			DString_AppendMBS( wrapper, sindex );
			DString_AppendMBS( wrapper, "] );\n" );
			break;
		case DAO_ENUM :
			DString_Append( cc, pname );
			DString_AppendMBS( cproto, "int " );
			DString_Append( cproto, pname );
			DString_AppendMBS( wrapper, "int " );
			DString_Append( wrapper, pname );
			DString_AppendMBS( wrapper, " = DaoValue_TryGetEnum( _p[" );
			DString_AppendMBS( wrapper, sindex );
			DString_AppendMBS( wrapper, "] );\n" );
			break;
		case DAO_ARRAY :
			DString_Append( cc, pname );
			DString_AppendMBS( cproto, "DaoArray *" );
			DString_Append( cproto, pname );
			DString_AppendMBS( wrapper, "DaoArray *" );
			DString_Append( wrapper, pname );
			DString_AppendMBS( wrapper, " = DaoValue_CastArray( _p[" );
			DString_AppendMBS( wrapper, sindex );
			DString_AppendMBS( wrapper, "] );\n" );
			break;
		case DAO_LIST :
			DString_Append( cc, pname );
			DString_AppendMBS( cproto, "DaoList *" );
			DString_Append( cproto, pname );
			DString_AppendMBS( wrapper, "DaoList *" );
			DString_Append( wrapper, pname );
			DString_AppendMBS( wrapper, " = DaoValue_CastList( _p[" );
			DString_AppendMBS( wrapper, sindex );
			DString_AppendMBS( wrapper, "] );\n" );
			break;
		case DAO_TUPLE :
			DString_Append( cc, pname );
			DString_AppendMBS( cproto, "DaoTuple *" );
			DString_Append( cproto, pname );
			DString_AppendMBS( wrapper, "DaoTuple *" );
			DString_Append( wrapper, pname );
			DString_AppendMBS( wrapper, " = DaoValue_CastTuple( _p[" );
			DString_AppendMBS( wrapper, sindex );
			DString_AppendMBS( wrapper, "] );\n" );
			break;
		case DAO_MAP :
			DString_Append( cc, pname );
			DString_AppendMBS( cproto, "DaoMap *" );
			DString_Append( cproto, pname );
			DString_AppendMBS( wrapper, "DaoMap *" );
			DString_Append( wrapper, pname );
			DString_AppendMBS( wrapper, " = DaoValue_CastMap( _p[" );
			DString_AppendMBS( wrapper, sindex );
			DString_AppendMBS( wrapper, "] );\n" );
			break;
		case DAO_STREAM :
			DString_Append( cc, pname );
			DString_AppendMBS( cproto, "FILE *" );
			DString_Append( cproto, pname );
			DString_AppendMBS( wrapper, "FILE *" );
			DString_Append( wrapper, pname );
			DString_AppendMBS( wrapper, " = DaoStream_GetFile( DaoValue_CastStream( _p[" );
			DString_AppendMBS( wrapper, sindex );
			DString_AppendMBS( wrapper, "] ) );\n" );
			break;
		case DAO_CDATA :
			if( strcmp( type->name->mbs, "cdata" ) == 0 ){
				DString_Append( cc, pname );
				DString_AppendMBS( cproto, "void *" );
				DString_Append( cproto, pname );
				DString_AppendMBS( wrapper, "void *" );
				DString_Append( wrapper, pname );
				DString_AppendMBS( wrapper, " = DaoValue_TryGetCdata( _p[" );
				DString_AppendMBS( wrapper, sindex );
				DString_AppendMBS( wrapper, "] );\n" );
			}else{
				return 1;
			}
			break;
		default : return 1;
		}
	}
	DString_AppendMBS( cc, " );\n" );
	DString_AppendMBS( cproto, " )" );
	DString_AppendChar( wrapper, '\t' );

	type = & routype->aux->xType;
	if( type == NULL || type->tid == DAO_UDF ){
		DString_InsertMBS( cproto, "void ", 0, 0, 0 );
		DString_Append( wrapper, cc );
	}else{
		switch( type->tid ){
		case DAO_INTEGER :
		case DAO_FLOAT :
		case DAO_DOUBLE :
			DString_InsertMBS( cproto, " ", 0, 0, 0 );
			DString_Insert( cproto, type->name, 0, 0, 0 );
			DString_Append( wrapper, type->name );
			DString_AppendMBS( wrapper, " _res = " );
			DString_Append( wrapper, cc );
			switch( type->tid ){
			case DAO_INTEGER :
				DString_AppendMBS( wrapper, "DaoProcess_PutInteger( _proc, _res );\n" );
				break;
			case DAO_FLOAT :
				DString_AppendMBS( wrapper, "DaoProcess_PutFloat( _proc, _res );\n" );
				break;
			case DAO_DOUBLE :
				DString_AppendMBS( wrapper, "DaoProcess_PutDouble( _proc, _res );\n" );
				break;
			}
			break;
		case DAO_COMPLEX :
			DString_InsertMBS( cproto, "complex16 ", 0, 0, 0 );
			DString_AppendMBS( wrapper, "complex16 " );
			DString_AppendMBS( wrapper, " _res = " );
			DString_Append( wrapper, cc );
			DString_AppendMBS( wrapper, "DaoProcess_PutComplex( _proc, _res );\n" );
			break;
		case DAO_STRING :
			DString_InsertMBS( cproto, "const char* ", 0, 0, 0 );
			DString_AppendMBS( wrapper, "const char* " );
			DString_AppendMBS( wrapper, " _res = " );
			DString_Append( wrapper, cc );
			DString_AppendMBS( wrapper, "DaoProcess_PutMBString( _proc, _res );\n" );
		case DAO_STREAM :
			DString_InsertMBS( cproto, "FILE* ", 0, 0, 0 );
			DString_AppendMBS( wrapper, "FILE* " );
			DString_AppendMBS( wrapper, " _res = " );
			DString_Append( wrapper, cc );
			DString_AppendMBS( wrapper, "DaoProcess_PutFile( _proc, _res );\n" );
			break;
		case DAO_CDATA :
			if( strcmp( type->name->mbs, "cdata" ) == 0 ){
				DString_InsertMBS( cproto, "void* ", 0, 0, 0 );
				DString_AppendMBS( wrapper, "void* " );
				DString_AppendMBS( wrapper, " _res = " );
				DString_Append( wrapper, cc );
				DString_AppendMBS( wrapper, "DaoProcess_PutCdata( _proc, _res, NULL );\n" );
			}else{
				return 1;
			}
			break;
		default : return 1;
		}
	}
	DString_AppendMBS( wrapper, "\n}\n" );
	//printf( "\n%s\n%s\n", cproto->mbs, wrapper->mbs );
	return 0;
}

static int error_compile_failed( DString *out )
{
	DString_SetMBS( out, "Compiling failed on the inlined codes" );
	return 1;
}
static int error_function_notfound( DString *out, const char *name )
{
	DString_SetMBS( out, "Function not found in the inlined codes: " );
	DString_AppendMBS( out, name );
	return 1;
}
static int error_function_notwrapped( DString *out, const char *name )
{
	DString_SetMBS( out, "Function wrapping failed: " );
	DString_AppendMBS( out, name );
	return 1;
}

static int dao_cxx_block( DaoNamespace *NS, DString *VT, DArray *markers, DString *source, DString *out )
{
	char bytes[200];
	char name[50];

	sprintf( name, "anonymous_%p_%p", NS, VT );
	sprintf( bytes, "void %s(){\n", name );
	DString_InsertMBS( source, bytes, 0, 0, 0 );
	DString_InsertMBS( source, dao_cxx_default_includes, 0, 0, 0 );

	sprintf( bytes, "\n}\nextern \"C\"{\nvoid dao_%s%s{\n\t%s();\n}\n}", name, dao_wrapper, name );
	DString_AppendMBS( source, bytes );

	sprintf( name, "anonymous_%p_%p.cxx", NS, VT );
	//printf( "%s:\n%s\n", name, source->mbs );
	DaoCxxInliner_AddVirtualFile( name, source->mbs );

	action.BeginSourceFile( compiler, name, IK_CXX );
	if( ! compiler.ExecuteAction( action ) ) return error_compile_failed( out );

	Module *Module = action.takeModule();
	if( Module == NULL ) return error_compile_failed( out );

	sprintf( name, "dao_anonymous_%p_%p", NS, VT );
	Function *Func = Module->getFunction( name );
	if( Func == NULL ) return error_function_notfound( out, name );

	void *fp = engine->getPointerToFunction( Func );
	if( fp == NULL ) return error_function_notfound( out, name );

	DString_SetMBS( out, name );
	DString_AppendMBS( out, "()" );
	DaoNamespace_WrapFunction( NS, (DaoFuncPtr)fp, out->mbs );
	DString_AppendChar( out, ';' );
	return 0;
}
static int dao_cxx_function( DaoNamespace *NS, DString *VT, DArray *markers, DString *source, DString *out )
{
	int retc;
	char file[50];
	char proto2[200];
	char *proto = proto2;
	DString *mbs = DString_New(1);

	sprintf( file, "anonymous_%p_%p.cxx", NS, VT );
	sprintf( proto2, "anonymous_%p_%p()", NS, VT );
	if( dao_markers_get( markers, "define", mbs, NULL ) ) proto = mbs->mbs;

	DaoFunction *func = DaoNamespace_WrapFunction( NS, DaoCxxInliner_Default, proto );
	if( func == NULL ){
		error_function_notwrapped( out, proto );
		DString_Delete( mbs );
		return 1;
	}
	
	DString *cproto = DString_New(1);
	DString *call = DString_New(1);

	DString_AppendMBS( source, "}\nextern \"C\"{\n" );
	retc = dao_make_wrapper( func->routName, func->routType, cproto, source, call );

	DString_AppendMBS( cproto, "{\n" );
	DString_Insert( source, cproto, 0, 0, 0 );
	DString_InsertMBS( source, dao_cxx_default_includes, 0, 0, 0 );
	DString_AppendMBS( source, "}\n" );

	DString_Delete( mbs );
	DString_Delete( cproto );
	DString_Delete( call );

	if( retc ) return error_function_notwrapped( out, func->routName->mbs );

	//printf( "\n%s\n%s\n", file, source->mbs );
	DaoCxxInliner_AddVirtualFile( file, source->mbs );

	action.BeginSourceFile( compiler, file, IK_CXX );
	if( ! compiler.ExecuteAction( action ) ) return error_compile_failed( out );

	Module *Module = action.takeModule();
	if( Module == NULL ) return error_compile_failed( out );

	sprintf( proto2, "dao_%s", func->routName->mbs ); //XXX buffer size
	Function *Func = Module->getFunction( proto2 );
	if( Func == NULL ) return error_function_notfound( out, proto2 );

	void *fp = engine->getPointerToFunction( Func );
	if( fp == NULL ) return error_function_notfound( out, proto2 );

	func->pFunc = (DaoFuncPtr)fp;
	return 0;
}
static int dao_cxx_header( DaoNamespace *NS, DString *VT, DArray *markers, DString *source, DString *out )
{
	DString *mbs = DString_New(1);
	char name[50];
	char *file = name;
	sprintf( name, "anonymous_%p_%p.h", NS, VT );
	if( dao_markers_get( markers, "file", mbs, NULL ) ){
		// TODO: better handling of suffix?
		if( isalnum( mbs->mbs[0] ) ) DString_InsertMBS( mbs, "./", 0, 0, 0 );
		file = mbs->mbs;
	}
	DaoCxxInliner_AddVirtualFile( file, source->mbs );
	DString_Delete( mbs );
	return 0;
}
static int dao_cxx_source( DaoNamespace *NS, DString *VT, DArray *markers, DString *source, DString *out )
{
	DString *mbs = DString_New(1);
	DString *call = DString_New(1);
	DString *cproto = DString_New(1);
	DArray *wraps = DArray_New(D_STRING);
	DArray *funcs = DArray_New(0);
	InputKind kind = IK_CXX;
	char name[200];
	char *file = name;
	size_t i, failed = 0;

	sprintf( name, "anonymous_%p_%p.cxx", NS, VT );
	dao_markers_get( markers, "wrap", mbs, wraps );
	if( dao_markers_get( markers, "file", mbs, NULL ) ){
		file = mbs->mbs;
		// TODO: better handling of suffix?
		if( DString_FindMBS( mbs, ".c", 0 ) == mbs->size - 2 ) kind = IK_C;
		if( DString_FindMBS( mbs, ".C", 0 ) == mbs->size - 2 ) kind = IK_C;
	}

	DString_Clear( out );
	DString_InsertMBS( source, dao_cxx_default_includes, 0, 0, 0 );
	DString_AppendMBS( source, "\nextern \"C\"{\n" );
	for(i=0; i<wraps->size; i++){
		DString *daoproto = wraps->items.pString[i];
		DaoFunction *func = DaoNamespace_WrapFunction( NS, DaoCxxInliner_Default, daoproto->mbs );
		if( func == NULL || dao_make_wrapper( func->routName, func->routType, cproto, source, call ) ){
			DString_AppendMBS( out, daoproto->mbs );
			DString_AppendMBS( out, "\n" );
			failed += 1;
			continue;
		}
		DArray_Append( funcs, func );
	}
	DString_AppendMBS( source, "}\n" );

	//printf( "\n%s\n%s\n", file, source->mbs );
	if( failed == 0 ) DaoCxxInliner_AddVirtualFile( file, source->mbs );
	DString_Delete( mbs );
	DString_Delete( call );
	DString_Delete( cproto );

	if( failed ){
		DString_InsertMBS( out, "Function wrapping failed:\n", 0, 0, 0 );
		return 1;
	}

	action.BeginSourceFile( compiler, file, kind );
	if( ! compiler.ExecuteAction( action ) ) return error_compile_failed( out );

	Module *Module = action.takeModule();
	if( Module == NULL ) return error_compile_failed( out );

	for(i=0; i<funcs->size; i++){
		DaoFunction *func = (DaoFunction*) funcs->items.pVoid[i];
		sprintf( name, "dao_%s", func->routName->mbs ); //XXX buffer size

		Function *Func = Module->getFunction( name );
		if( Func == NULL ) return error_function_notfound( out, name );

		void *fp = engine->getPointerToFunction( Func );
		if( fp == NULL ) return error_function_notfound( out, name );
		func->pFunc = (DaoFuncPtr)fp;
	}
	return 0;
}

static int dao_cxx_inliner( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out )
{
	DString *source = DString_New(1);
	DArray *markers = DArray_New(D_STRING);
	int retc = 1;

	dao_cxx_parse( verbatim, source, markers );

	if( mode->size == 0 || strcmp( mode->mbs, "block" ) ==0 ){
		retc = dao_cxx_block( NS, verbatim, markers, source, out );
	}else if( strcmp( mode->mbs, "function" ) ==0 ){
		retc = dao_cxx_function( NS, verbatim, markers, source, out );
	}else if( strcmp( mode->mbs, "header" ) ==0 ){
		retc = dao_cxx_header( NS, verbatim, markers, source, out );
	}else if( strcmp( mode->mbs, "source" ) ==0 ){
		retc = dao_cxx_source( NS, verbatim, markers, source, out );
	}else{
		DString_SetMBS( out, "Invalid inline mode" );
	}
	if( markers->size ){
		DString_SetMBS( out, "Invalid specifiers for the mode: \n" );
		for(size_t i=0; i<markers->size; i++){
			DString *marker = markers->items.pString[i];
			DString_Append( out, marker );
			DString_AppendChar( out, '\n' );
		}
		retc = 1;
	}
	DString_Delete( source );
	DArray_Delete( markers );
	return retc;
}

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	static int argc = 2;
	static char *argv[2] = { "dao", "dummy-main.cpp" };
	DString *mbs = DString_New(1);

	DString_SetMBS( mbs, source_caption_pattern );
	source_caption_regex = DaoRegex_New( mbs );

	DString_SetMBS( mbs, header_suffix_pattern );
	header_suffix_regex = DaoRegex_New( mbs );

	DString_Delete( mbs );

	compiler.createDiagnostics(argc, argv);

	Diagnostic & DG = compiler.getDiagnostics();
	CompilerInvocation::CreateFromArgs( compiler.getInvocation(), argv + 1, argv + argc, DG );
	compiler.setTarget( TargetInfo::CreateTargetInfo( DG, compiler.getTargetOpts() ) );

#ifdef MAC_OSX
	// needed to circumvent a bug which is supposingly fixed in clang 2.9-16
	clang::HeaderSearchOptions & headers = compiler.getHeaderSearchOpts();
	headers.AddPath( "/Developer/SDKs/MacOSX10.5.sdk/usr/lib/gcc/i686-apple-darwin9/4.2.1/include", clang::frontend::System, false, false, true );
#endif

	//compiler.getHeaderSearchOpts().AddPath( "", clang::frontend::Angled, false, false, true );

	compiler.createFileManager();
	compiler.createSourceManager( compiler.getFileManager() );
	DaoCxxInliner_AddVirtualFile( "dummy-main.cpp", "void dummy_main(){}" );

	InitializeNativeTarget();
	if( ! compiler.ExecuteAction( action ) ) return 1;

	std::string Error;
	engine = ExecutionEngine::createJIT( action.takeModule(), &Error );
	if( engine == NULL ){
		errs() << Error << "\n";
		return 1;
	}

	DaoNamespace_AddCodeInliner( ns, "cxx", dao_cxx_inliner );
	DaoNamespace_AddCodeInliner( ns, "cpp", dao_cxx_inliner );
	DaoNamespace_TypeDefine( ns, "int", "short" );
	DaoNamespace_TypeDefine( ns, "int", "size_t" );
	DaoNamespace_TypeDefine( ns, "int", "int8_t" );
	DaoNamespace_TypeDefine( ns, "int", "uint8_t" );
	DaoNamespace_TypeDefine( ns, "int", "int16_t" );
	DaoNamespace_TypeDefine( ns, "int", "uint16_t" );
	DaoNamespace_TypeDefine( ns, "int", "int32_t" );
	DaoNamespace_TypeDefine( ns, "int", "uint32_t" );
	DaoNamespace_TypeDefine( ns, "int", "int64_t" );
	DaoNamespace_TypeDefine( ns, "int", "uint64_t" );
	return 0;
}
