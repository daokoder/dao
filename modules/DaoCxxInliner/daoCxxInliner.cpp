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
		if( lb == npos || rb == npos ) continue; // TODO warning
		DString_SetDataMBS( one, marker->mbs + 1, lb - 1 );
		DString_Trim( one );
		if( strcmp( one->mbs, name ) ) continue;
		DString_SetDataMBS( one, marker->mbs + lb + 1, rb - lb - 1 );
		DString_Trim( one );
		m += 1;
		if( all == NULL ) break;
		DArray_Append( all, one );
	}
	return m;
}
static int dao_make_wrapper( DString *name, DaoType *routype, DString *cproto, DString *wrapper )
{
	return 0;
}

const char *dao_cxx_default_includes =
"#include<stdio.h>\n"
"#include<stdlib.h>\n"
"#include<math.h>\n"
"#include<dao.h>\n";

const char *dao_wrapper = "( DaoProcess *proc, DaoValue *p[], int N )";

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
	if( ! compiler.ExecuteAction( action ) ) return 1;
	Module *Module = action.takeModule();
	if( Module == NULL ) return 1;
	sprintf( name, "dao_anonymous_%p_%p", NS, VT );
	Function *Func = Module->getFunction( name );
	if( Func == NULL ) return 1;
	void *fp = engine->getPointerToFunction( Func );
	if( fp == NULL ) return 1;
	DString_SetMBS( out, name );
	DString_AppendMBS( out, "()" );
	DaoNamespace_WrapFunction( NS, (DaoFuncPtr)fp, out->mbs );
	DString_AppendChar( out, ';' );
	return 0;
}
static int dao_cxx_function( DaoNamespace *NS, DString *VT, DArray *markers, DString *source, DString *out )
{
	char file[50];
	char proto2[100];
	char *proto = proto2;
	DString *mbs = DString_New(1);

	sprintf( file, "anonymous_%p_%p.cxx", NS, VT );
	sprintf( proto2, "anonymous_%p_%p()", NS, VT );
	if( dao_markers_get( markers, "define", mbs, NULL ) ) proto = mbs->mbs;

	DaoFunction *func = DaoNamespace_WrapFunction( NS, DaoCxxInliner_Default, proto );
	if( func == NULL ) return 1; //TODO error
	
	DaoType *routype = func->routType;

	//DaoCxxInliner_AddVirtualFile( file, source->mbs );
	DString_Delete( mbs );
	return 0;
}
static int dao_cxx_header( DaoNamespace *NS, DString *VT, DArray *markers, DString *source, DString *out )
{
	DString *mbs = DString_New(1);
	char name[50];
	char *file = name;
	sprintf( name, "anonymous_%p_%p.h", NS, VT );
	if( dao_markers_get( markers, "file", mbs, NULL ) ){
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
	char name[50];
	char *file = name;
	size_t i;
	sprintf( name, "anonymous_%p_%p.cxx", NS, VT );
	if( dao_markers_get( markers, "file", mbs, NULL ) ) file = mbs->mbs;
	DaoCxxInliner_AddVirtualFile( file, source->mbs );
	DString_Delete( mbs );

	action.BeginSourceFile( compiler, file, IK_CXX );
	if( ! compiler.ExecuteAction( action ) ) return 1;
	Module *Module = action.takeModule();
	if( Module == NULL ) return 1;
	// TODO
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
	DString_Delete( source );
	DArray_Delete( markers );
	return retc;
}

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	static int argc = 2;
	static char *argv[2] = { "/Users/min/projects/dao", "dummy-main.cpp" };
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
	return 0;
}
