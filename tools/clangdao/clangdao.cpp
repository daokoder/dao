// ClangDao: Clang-based automatic binding tool for Dao
// By Limin Fu.

#include <llvm/Support/Host.h>
#include <llvm/Support/Path.h>
#include <clang/Lex/Token.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclGroup.h>
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
	if( MI->isFunctionLike() ) module->HandleHintDefinition( MI );
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
private:
	void ConsumeVariable(const VarDecl & var) const;
	void ConsumeFunction(const FunctionDecl & func) const;
};

void CDaoASTConsumer::HandleTopLevelDecl(DeclGroupRef group)
{
	for (DeclGroupRef::iterator it = group.begin(); it != group.end(); ++it) {
		if (const VarDecl *var = dyn_cast<VarDecl>(*it)) {
			ConsumeVariable(*var);
		}else if (const FunctionDecl *func = dyn_cast<FunctionDecl>(*it)) {
			ConsumeFunction(*func);
		}
	}
}
void CDaoASTConsumer::ConsumeVariable(const VarDecl & var) const
{
	outs() << var.getNameAsString() << "\n";
}
void CDaoASTConsumer::ConsumeFunction(const FunctionDecl & func) const
{
	outs() << func.getNameAsString() << " has "<< func.param_size() << " parameters\n";
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

// TODO:
// Add -S for specifying file suffixes of module definition files.


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

	// The follow path is needed for Objective-C:
	// /Developer/SDKs/MacOSX10.5.sdk/usr/lib/gcc/i686-apple-darwin9/4.2.1/include

#if 0
	// No need to handle -Iargs here, it was handled by CreateFromArgs().
	HeaderSearchOptions & headers = compiler.getHeaderSearchOpts();
	for (i = 0; i < include_paths.size(); ++i) {
		outs() << "adding " << include_paths[i] << "\n";
		headers.AddPath( include_paths[i], frontend::System, false, false, true );
	}
#endif

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

	if( module.CheckHeaderDependency() == false ) return 1;
	return 0;
}
