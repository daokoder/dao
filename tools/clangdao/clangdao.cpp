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
			bool Angled, const FileEntry *File, SourceLocation End,
			StringRef SearchPath, StringRef RelativePath );
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
		StringRef Name, bool Angled, const FileEntry *File, SourceLocation End,
		StringRef SearchPath, StringRef RelativePath )
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
		if( prev == '>' && next == '>' ) continue;
		if( (isalnum( prev ) || prev == '_') && (isalnum( next ) || next == '_') ) continue;
		name.replace( pos, 1, "" );
	}
	remove_type_prefix( name, "class" );
	remove_type_prefix( name, "struct" );
	remove_type_prefix( name, "enum" );
	remove_type_prefix( name, "typename" );
	return name;
}
static bool is_invalid_dao_type_name( const string & name )
{
	int i, n = name.size();
	if( n == 0 ) return true;
	if( name[0] == '@' ){
		for(i=1; i<n; i++){
			char ch = name[i];
			if( isalnum( ch ) == 0 && ch != '_' ) return true;
		}
		return false;
	}
	for(i=0; i<n; i++){
		char ch = name[i];
		if( isalnum( ch ) == 0 && ch != '_' && ch != ':' ) return true;
	}
	return false;
}
map<string,int> type_for_quoting;
map<string,string> type_substitutions;
string cdao_make_dao_template_type_name( const string & name0, const map<string,string> & subs, const map<string,int> & type_for_quoting )
{
	map<string,string>::const_iterator it;
	string name = normalize_type_name( name0 );
	string result, part;
	int i, n;
	for(i=0, n = name.size(); i<n; i++){
		char ch = name[i];
		if( ch == '<' || ch == '>' || ch == ',' ){
			if( part != "" && part != " " ){
				string quote = is_invalid_dao_type_name( part ) ? "'" : "";
				it = subs.find( part );
				if( it != subs.end() ) part = it->second;
				if( part.find( "std::" ) == 0 ) part.replace( 0, 5, "stdcxx::" );
				if( type_for_quoting.find( part ) != type_for_quoting.end() ) quote = "'";
				if( type_for_quoting.size() == 0 ) quote = "";
				result += quote + part + quote;
			}
			if( ch == '>' && result[result.size()-1] == '>' ) result += ' ';
			result += ch;
			part = "";
		}else{
			part += ch;
		}
	}
	if( part == "" ) return result;
	string quote = is_invalid_dao_type_name( part ) ? "'" : "";
	it = subs.find( part );
	if( it != subs.end() ) part = it->second;
	if( part.find( "std::" ) == 0 ) part.replace( 0, 5, "stdcxx::" );
	if( type_for_quoting.find( part ) != type_for_quoting.end() ) quote = "'";
	return result + quote + part + quote;
}
string cdao_make_dao_template_type_name( const string & name0 )
{
	map<string,string>::const_iterator it;
	string name = normalize_type_name( name0 );
	string result, part;
	int i, n;
	for(i=0, n = name.size(); i<n; i++){
		char ch = name[i];
		if( ch == '<' || ch == '>' || ch == ',' ){
			if( part != "" && part != " " ){
				string quote = is_invalid_dao_type_name( part ) ? "'" : "";
				it = type_substitutions.find( part );
				if( it != type_substitutions.end() ) part = it->second;
				if( part.find( "std::" ) == 0 ) part.replace( 0, 5, "stdcxx::" );
				if( type_for_quoting.find( part ) != type_for_quoting.end() ) quote = "'";
				result += quote + part + quote;
			}
			//if( ch == '>' && result[result.size()-1] == '>' ) result += ' ';
			result += ch;
			part = "";
		}else{
			part += ch;
		}
	}
	if( part == "" ) return result;
	string quote = is_invalid_dao_type_name( part ) ? "'" : "";
	it = type_substitutions.find( part );
	if( it != type_substitutions.end() ) part = it->second;
	if( part.find( "std::" ) == 0 ) part.replace( 0, 5, "stdcxx::" );
	if( type_for_quoting.find( part ) != type_for_quoting.end() ) quote = "'";
	return result + quote + part + quote;
}
string cdao_substitute_typenames( const string & name0 )
{
	map<string,string>::const_iterator it;
	string name = normalize_type_name( name0 );
	string result, part;
	int i, n;
	for(i=0, n = name.size(); i<n; i++){
		char ch = name[i];
		if( ch == '<' || ch == '>' || ch == ',' ){
			if( part != "" && part != " " ){
				it = type_substitutions.find( part );
				if( it != type_substitutions.end() ) part = it->second;
				result += part;
			}
			if( ch == '>' && result[result.size()-1] == '>' ) result += ' ';
			result += ch;
			part = "";
		}else{
			part += ch;
		}
	}
	if( part == "" ) return result;
	it = type_substitutions.find( part );
	if( it != type_substitutions.end() ) part = it->second;
	return result + part;
}
string cdao_remove_type_scopes( const string & qname )
{
	size_t i, n, colon = string::npos;
	string name = qname;
	for(i=0,n=name.size(); i<n; i++){
		char ch = name[i];
		if( ch == ':' ) colon = i;
		if( isalnum( ch ) ==0 && ch != '_' && ch != ':' ) break;
	}
	if( colon != string::npos ) name.erase( 0, colon+1 );
	return name;
}
const char *const conversions[] =
{
	"::", 
	"<", ">", ",", " ", "[", "]", "(", ")", "*", ".", 
	"=", "+", "-", "*", "/", "%", "&", "|", "^", "!", "~",
	NULL
};
const char *const conversions2[] =
{
	"+=", "-=", "*=", "/=", "%=", "&=", "^=",
	"==", "!=", "<=", ">=", "<<", ">>", "[]",
	NULL
};
// qualified name to single identifier name:
string cdao_qname_to_idname( const string & qname )
{
	string idname = normalize_type_name( qname );
	int i;
	for(i=0; conversions2[i]; i++){
		size_t p = 0;
		string s = "_" + utostr( 30+i ) + "_";
		while( (p = idname.find( conversions2[i], p )) != string::npos ) idname.replace( p, 2, s );
	}
	for(i=0; conversions[i]; i++){
		size_t p = 0;
		string s = "_" + utostr( i ) + "_";
		while( (p = idname.find( conversions[i], p )) != string::npos ) idname.replace( p, 1+(i==0), s );
	}
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

static llvm::cl::opt<std::string> output_dir("o", llvm::cl::desc("output directory"));

// Note:
// The follow path is needed for Objective-C:
// /Developer/SDKs/MacOSX10.5.sdk/usr/lib/gcc/i686-apple-darwin9/4.2.1/include

const string predefines = 
"#define DAO_MODULE_NAME( name )\n"
"#define DAO_PROPERTY_HINT( hints )\n"
"#define DAO_FUNCTION_HINT( hints )\n";


int main(int argc, char *argv[] )
{
	type_for_quoting[ "void" ] = 1;
	type_for_quoting[ "bool" ] = 1;
	type_for_quoting[ "char" ] = 1;
	type_for_quoting[ "wchar_t" ] = 1;
	type_for_quoting[ "short" ] = 1;
	type_for_quoting[ "long" ] = 1;
	type_for_quoting[ "size_t" ] = 1;
	type_for_quoting[ "int8_t" ] = 1;
	type_for_quoting[ "int16_t" ] = 1;
	type_for_quoting[ "int32_t" ] = 1;
	type_for_quoting[ "int64_t" ] = 1;
	type_for_quoting[ "uint8_t" ] = 1;
	type_for_quoting[ "uint16_t" ] = 1;
	type_for_quoting[ "uint32_t" ] = 1;
	type_for_quoting[ "uint64_t" ] = 1;
	type_substitutions[ "_Bool" ] = "bool";

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
	//XXX compiler.createSema(false, NULL);
	//compiler.createSema(TU_Module, NULL);
	compiler.createSema(TU_Prefix, NULL);

	Preprocessor & pp = compiler.getPreprocessor();
	pp.setPredefines( pp.getPredefines() + "\n" + predefines );
	pp.addPPCallbacks( new CDaoPPCallbacks( & compiler, & module ) );

	compiler.InitializeSourceManager( main_input_file );
	compiler.getDiagnosticClient().BeginSourceFile( compiler.getLangOpts(), & pp );
	ParseAST( pp, &compiler.getASTConsumer(), compiler.getASTContext() );
	compiler.getDiagnosticClient().EndSourceFile();

	return module.Generate( output_dir );
}
