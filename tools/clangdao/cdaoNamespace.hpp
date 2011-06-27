
#ifndef __CDAO_NAMESPACE_H__
#define __CDAO_NAMESPACE_H__

#include <clang/AST/Decl.h>

#include "cdaoFunction.hpp"
#include "cdaoUserType.hpp"

using namespace std;
using namespace llvm;
using namespace clang;

struct CDaoModule;

struct CDaoNamespace
{
	CDaoModule     *module;
	NamespaceDecl  *nsdecl;

	vector<CDaoNamespace>  namespaces;
	vector<CDaoUserType>   usertypes;
	vector<CDaoFunction>   functions;

	string  header;
	string  source;
	string  source2;
	string  source3;
	string  onload;

	CDaoNamespace( CDaoModule *mod = NULL, NamespaceDecl *decl = NULL );

	int Generate();
	void Extract( NamespaceDecl *nsdecl );
};

#endif
