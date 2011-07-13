// ClangDao: Clang-based automatic binding tool for Dao
// By Limin Fu.

#include <llvm/Support/Host.h>
#include <llvm/Support/Path.h>
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
#include <iostream>
#include <string>
#include <vector>

#include "cdaoModule.hpp"

using namespace llvm;
using namespace clang;

struct CDaoPPCallbacks : public PPCallbacks
{
	CompilerInstance *compiler;
	CDaoModule *module;

	CDaoPPCallbacks( CompilerInstance *cinst, CDaoModule *mod ){
		compiler = cinst;
		module = mod;
	}

	void MacroDefined(const Token &MacroNameTok, const MacroInfo *MI);
	void InclusionDirective(SourceLocation Loc, const Token &Tok, StringRef Name, 
			bool Angled, const FileEntry *File, SourceLocation End);
};

void CDaoPPCallbacks::MacroDefined(const Token &MacroNameTok, const MacroInfo *MI)
{
	llvm::StringRef name = MacroNameTok.getIdentifierInfo()->getName();
	if( MI->getNumTokens() < 1 ) return; // number of expansion tokens;
	if( MI->isObjectLike() && name == "module_name" ){
		module->HandleModuleDeclaration( MI );
		return;
	}
	if( MI->isFunctionLike() ) module->HandleHintDefinition( name, MI );
}
void CDaoPPCallbacks::InclusionDirective(SourceLocation Loc, const Token &Tok, 
		StringRef Name, bool Angled, const FileEntry *File, SourceLocation End)
{
	module->HandleHeaderInclusion( Loc, Name.str(), File );
}



struct CDaoASTConsumer : public ASTConsumer
{
	CompilerInstance *compiler;
	CDaoModule *module;

	CDaoASTConsumer( CompilerInstance *cinst, CDaoModule *mod ){
		compiler = cinst;
		module = mod;
	}
	void HandleTopLevelDecl(DeclGroupRef group);
};

void CDaoASTConsumer::HandleTopLevelDecl(DeclGroupRef group)
{
	for (DeclGroupRef::iterator it = group.begin(); it != group.end(); ++it) {
		if (VarDecl *var = dyn_cast<VarDecl>(*it)) {
			module->HandleVariable( var );
		}else if (EnumDecl *e = dyn_cast<EnumDecl>(*it)) {
			module->HandleEnum( e );
		}else if (FunctionDecl *func = dyn_cast<FunctionDecl>(*it)) {
			module->HandleFunction( func );
		}else if (RecordDecl *record = dyn_cast<RecordDecl>(*it)) {
			module->HandleUserType( record );
		}else if (NamespaceDecl *nsdecl = dyn_cast<NamespaceDecl>(*it)) {
			module->HandleNamespace( nsdecl );
		}else if( TypedefDecl *decl = dyn_cast<TypedefDecl>(*it) ){
			module->HandleTypeDefine( decl );
		}
	}
}

string cdao_string_fill( const string & tpl, const map<string,string> & subs )
{
	string result;
	size_t i, rb, prev = 0, pos = tpl.find( '$', 0 );
	while( pos != string::npos ){
		string gap = tpl.substr( prev, pos - prev );
		result += gap;
		if( tpl[pos+1] == '(' and (rb = tpl.find( ')', pos ) ) != string::npos ){
			string key = tpl.substr( pos+2, rb - pos - 2 );
			map<string,string>::const_iterator it = subs.find( key );
			bool fill = it != subs.end();
			for(i=pos+2; fill and i<rb; i++) fill &= tpl[i] == '_' or isalnum( tpl[i] );
			if( fill ){
				result += it->second;
				prev = rb + 1;
				pos = tpl.find( '$', prev );
				continue;
			}
		}
		result += '$';
		prev = pos + 1;
		pos = tpl.find( '$', prev );
	}
	string gap = tpl.substr( prev, tpl.size() - prev );
	result += gap;
	return result;
}
void remove_type_prefix( string & name, const string & key )
{
	string key2 = key + " ";
	size_t klen = key2.size();
	size_t pos, from = 0;
	while( (pos = name.find( key2, from )) != string::npos ){
		if( pos == 0 || (isalnum( name[pos-1] ) ==0 && name[pos-1] != '_') ){
			name.replace( pos, klen, "" );
		}else{
			from = pos + klen;
		}
	}
}
string normalize_type_name( const string & name0 )
{
	string name = name0;
	size_t pos, from = 0;
	while( (pos = name.find( "\t" )) != string::npos ) name.replace( pos, 1, "" );
	while( (pos = name.find( "\n" )) != string::npos ) name.replace( pos, 1, "" );
	while( (pos = name.find( "  " )) != string::npos ) name.replace( pos, 2, " " );
	from = 0;
	while( (pos = name.find( " ", from )) != string::npos ){
		if( pos == 0 || (pos+1) == name.size() ){
			name.replace( pos, 1, "" );
			continue;
		}
		from = pos + 1;
		char prev = name[pos-1], next = name[pos+1];
		if( (isalnum( prev ) || prev == '_') && (isalnum( next ) || next == '_') ) continue;
		name.replace( pos, 1, "" );
	}
	remove_type_prefix( name, "class" );
	remove_type_prefix( name, "struct" );
	remove_type_prefix( name, "enum" );
	return name;
}
// qualified name to single identifier name:
string cdao_qname_to_idname( const string & qname )
{
	string idname = normalize_type_name( qname );
	size_t pos = 0;
	while( (pos = idname.find( "::", pos )) != string::npos ) idname.replace( pos, 2, "_0_" );
	while( (pos = idname.find( "<", pos )) != string::npos ) idname.replace( pos, 1, "_1_" );
	while( (pos = idname.find( ">", pos )) != string::npos ) idname.replace( pos, 1, "_2_" );
	while( (pos = idname.find( ",", pos )) != string::npos ) idname.replace( pos, 1, "_3_" );
	while( (pos = idname.find( "[", pos )) != string::npos ) idname.replace( pos, 1, "_4_" );
	while( (pos = idname.find( "]", pos )) != string::npos ) idname.replace( pos, 1, "_5_" );
	while( (pos = idname.find( "(", pos )) != string::npos ) idname.replace( pos, 1, "_6_" );
	while( (pos = idname.find( ")", pos )) != string::npos ) idname.replace( pos, 1, "_7_" );
	while( (pos = idname.find( "*", pos )) != string::npos ) idname.replace( pos, 1, "_8_" );
	while( (pos = idname.find( ".", pos )) != string::npos ) idname.replace( pos, 1, "_9_" );
	while( (pos = idname.find( " ", pos )) != string::npos ) idname.replace( pos, 1, "_10_" );
	while( (pos = idname.find( "=", pos )) != string::npos ) idname.replace( pos, 1, "_11_" );
	while( (pos = idname.find( "+", pos )) != string::npos ) idname.replace( pos, 1, "_12_" );
	while( (pos = idname.find( "-", pos )) != string::npos ) idname.replace( pos, 1, "_13_" );
	while( (pos = idname.find( "*", pos )) != string::npos ) idname.replace( pos, 1, "_14_" );
	while( (pos = idname.find( "/", pos )) != string::npos ) idname.replace( pos, 1, "_15_" );
	while( (pos = idname.find( "%", pos )) != string::npos ) idname.replace( pos, 1, "_16_" );
	while( (pos = idname.find( "<", pos )) != string::npos ) idname.replace( pos, 1, "_17_" );
	while( (pos = idname.find( ">", pos )) != string::npos ) idname.replace( pos, 1, "_18_" );
	while( (pos = idname.find( "+=", pos )) != string::npos ) idname.replace( pos, 2, "_19_" );
	while( (pos = idname.find( "-=", pos )) != string::npos ) idname.replace( pos, 2, "_20_" );
	while( (pos = idname.find( "*=", pos )) != string::npos ) idname.replace( pos, 2, "_21_" );
	while( (pos = idname.find( "/=", pos )) != string::npos ) idname.replace( pos, 2, "_22_" );
	while( (pos = idname.find( "%=", pos )) != string::npos ) idname.replace( pos, 2, "_23_" );
	while( (pos = idname.find( "==", pos )) != string::npos ) idname.replace( pos, 2, "_24_" );
	while( (pos = idname.find( "!=", pos )) != string::npos ) idname.replace( pos, 2, "_25_" );
	while( (pos = idname.find( "<=", pos )) != string::npos ) idname.replace( pos, 2, "_26_" );
	while( (pos = idname.find( ">=", pos )) != string::npos ) idname.replace( pos, 2, "_27_" );
	while( (pos = idname.find( "<<", pos )) != string::npos ) idname.replace( pos, 2, "_28_" );
	while( (pos = idname.find( ">>", pos )) != string::npos ) idname.replace( pos, 2, "_29_" );
	return idname;
}



static cl::list<std::string> preprocessor_definitions
("D", cl::value_desc("definitions"), cl::Prefix,
 cl::desc("Preprocessor definitions from command line arguments"));

static cl::list<std::string> include_paths
("I", cl::value_desc("includes"), cl::Prefix,
 cl::desc("Include paths from command line arguments"));

static cl::opt<std::string> main_input_file
(cl::Positional, cl::desc("<input file>"), cl::Required);

static cl::list<std::string> ignored_arguments(cl::Sink);

// Note:
// The follow path is needed for Objective-C:
// /Developer/SDKs/MacOSX10.5.sdk/usr/lib/gcc/i686-apple-darwin9/4.2.1/include



int main(int argc, char *argv[] )
{
	size_t i;
	cl::ParseCommandLineOptions( argc, argv, 
			"ClangDao: Clang-based automatic binding tool for Dao." );

	if (!ignored_arguments.empty()) {
		errs() << "Ignoring the following arguments:";
		copy(ignored_arguments.begin(), ignored_arguments.end(),
				std::ostream_iterator<std::string>(std::cerr, " "));
	}

	CompilerInstance compiler;
	CDaoModule module( & compiler, main_input_file );

	compiler.createDiagnostics(argc, argv);
	//compiler.getInvocation().setLangDefaults(IK_CXX);
	//compiler.getInvocation().setLangDefaults(IK_ObjC);
	CompilerInvocation::CreateFromArgs( compiler.getInvocation(),
			argv + 1, argv + argc, compiler.getDiagnostics() );

	compiler.setTarget( TargetInfo::CreateTargetInfo(
				compiler.getDiagnostics(), compiler.getTargetOpts() ) );

	compiler.createFileManager();
	compiler.createSourceManager(compiler.getFileManager());
	compiler.createPreprocessor();
	compiler.createASTContext();
	compiler.setASTConsumer( new CDaoASTConsumer( & compiler, & module ) );
	compiler.createSema(false, NULL);

	Preprocessor & pp = compiler.getPreprocessor();
	//pp.setPredefines( "#define DAO_MODULE_NAME( name )\n" );
	pp.addPPCallbacks( new CDaoPPCallbacks( & compiler, & module ) );

	compiler.InitializeSourceManager( main_input_file );
	compiler.getDiagnosticClient().BeginSourceFile( compiler.getLangOpts(), & pp );
	ParseAST( pp, &compiler.getASTConsumer(), compiler.getASTContext() );
	compiler.getDiagnosticClient().EndSourceFile();

	map<string,string> kv;
	kv[ "aaa" ] = "AAA"; kv[ "abc" ] = "ABC";
	outs() << cdao_string_fill( "$(aaa)123$(abc)456$(def)", kv ) << "\n";

	outs() << normalize_type_name( "void (class SecondClass *, int)" ) << "\n";
	//return 0;

	return module.Generate();
}
