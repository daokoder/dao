

#ifndef __CDAO_FUNCTION_H__
#define __CDAO_FUNCTION_H__

#include <clang/AST/Decl.h>
#include <clang/AST/DeclGroup.h>

#include "cdaoVariable.hpp"

using namespace std;
using namespace llvm;
using namespace clang;

class CDaoModule;

struct CDaoFunction
{
	CDaoModule           *module;
	FunctionDecl         *funcDecl;
	CDaoVariable          retype;
	vector<CDaoVariable>  parlist;

	CDaoFunction( CDaoModule *mod = NULL, FunctionDecl *decl = NULL );

	void SetDeclaration( FunctionDecl *decl );

	int Generate();
};

#endif
