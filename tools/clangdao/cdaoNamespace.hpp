
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

	int index;

	vector<CDaoNamespace*>  namespaces;
	vector<CDaoUserType*>   usertypes;
	vector<CDaoFunction>    functions;
	vector<EnumDecl*>       enums;

	string  header;
	string  source;
	string  source2;
	string  source3;
	string  onload;
	string  onload2;
	string  onload3;

	CDaoNamespace( CDaoModule *mod = NULL, NamespaceDecl *decl = NULL );

	int Generate( CDaoNamespace *outer = NULL );
	void HandleExtension( NamespaceDecl *nsdecl );
};

#endif
