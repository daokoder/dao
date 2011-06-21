

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

	int   index; // overloading index
	bool  excluded;

	string  cxxWrapName; // such as: dao_host_meth;
	string  cxxWrapper; // wrapper function definition;

	// implement method in DaoCxxVirt_$(type) to call appropriate Dao function:
	string  cxxWrapperVirt;
	// reimplement virtual function in DaoCxx_$(type)
	// by calling method from parent DaoCxxVirt_$(type):
	string  cxxWrapperVirt2;
	// reimplement virtual function in DaoCxxVirt_$(type)
	// by calling method from parent DaoCxxVirt_$(type):
	string  cxxWrapperVirt3;

	string  cxxWrapperVirtProto; // virtual method prototype;

	// Dao routine prototype for the wrapping function:
	// { dao_func_wrapping, "func( vec : array<int>, N : int )" },
	string  daoProtoCodes;

	// C prototype for the wrapping function:
	// void dao_func_wrapping( DaoContext *_ctx, DValue *_p[], int N )
	string  cxxProtoCodes;

	string  cxxCallCodes;
	string  cxxProtoParam;
	string  cxxProtoParamDecl;
	string  cxxProtoParamVirt;
	string  cxxCallParam;
	string  cxxCallParamV;

	CDaoFunction( CDaoModule *mod = NULL, FunctionDecl *decl = NULL, int idx = 1 );

	void SetDeclaration( FunctionDecl *decl );

	int Generate();
};

#endif
