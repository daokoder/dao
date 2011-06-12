// ClangDao: Clang-based automatic binding tool for Dao
// By Limin Fu.

#include <llvm/Support/Host.h>
#include <llvm/Support/Path.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclGroup.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Parse/ParseAST.h>

struct CDaoASTConsumer : clang::ASTConsumer
{
	void HandleTopLevelDecl(clang::DeclGroupRef group);
private:
	void ConsumeVariable(const clang::VarDecl & var) const;
	void ConsumeFunction(const clang::FunctionDecl & func) const;
};

void CDaoASTConsumer::HandleTopLevelDecl(clang::DeclGroupRef group)
{
	for (clang::DeclGroupRef::iterator it = group.begin(); it != group.end(); ++it) {
		if (const clang::VarDecl *var = llvm::dyn_cast<clang::VarDecl>(*it)) {
			ConsumeVariable(*var);
		}else if (const clang::FunctionDecl *func = llvm::dyn_cast<clang::FunctionDecl>(*it)) {
			ConsumeFunction(*func);
		}
	}
}
void CDaoASTConsumer::ConsumeVariable(const clang::VarDecl & var) const
{
	llvm::outs() << var.getNameAsString() << "\n";
}
void CDaoASTConsumer::ConsumeFunction(const clang::FunctionDecl & func) const
{
	llvm::outs() << func.getNameAsString() << " has "<< func.param_size() << " parameters\n";
}

int main(int argc, char *argv[] )
{
    clang::CompilerInstance compiler;
	
    compiler.createDiagnostics(argc, argv);
    //compiler.getInvocation().setLangDefaults(clang::IK_CXX);
    //compiler.getInvocation().setLangDefaults(clang::IK_ObjC);
    clang::CompilerInvocation::CreateFromArgs( compiler.getInvocation(),
			argv + 1, argv + argc, compiler.getDiagnostics() );

    const size_t n = compiler.getFrontendOpts().Inputs.size();
	if( argc == 1 or n != 1 ){
		llvm::errs() << "Need exactly one C/C++/ObjC source file as input.\n";
		return 1;
	}
	
    compiler.setTarget( clang::TargetInfo::CreateTargetInfo(
				compiler.getDiagnostics(), compiler.getTargetOpts() ) );

	clang::HeaderSearchOptions & headers = compiler.getHeaderSearchOpts();
	headers.AddPath( "/Developer/SDKs/MacOSX10.5.sdk/usr/lib/gcc/i686-apple-darwin9/4.2.1/include", clang::frontend::System, false, false, true );

    compiler.createFileManager();
    compiler.createSourceManager(compiler.getFileManager());
    compiler.createPreprocessor();
    compiler.createASTContext();
    compiler.setASTConsumer(new CDaoASTConsumer);
    compiler.createSema(false, NULL);
	
	compiler.InitializeSourceManager( compiler.getFrontendOpts().Inputs[0].second);
	clang::ParseAST( compiler.getPreprocessor(), &compiler.getASTConsumer(),
			compiler.getASTContext() );
	return 0;
}
