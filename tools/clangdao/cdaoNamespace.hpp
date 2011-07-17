
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

	vector<CDaoNamespace*>  namespaces;
	vector<CDaoUserType*>   usertypes;
	vector<CDaoFunction*>   functions;
	vector<EnumDecl*>       enums;
	vector<VarDecl*>        variables;
	map<string,int>         overloads;

	string  header;
	string  source;
	string  source2;
	string  source3;
	string  onload;
	string  onload2;
	string  onload3;

	CDaoNamespace( CDaoModule *mod = NULL, const NamespaceDecl *decl = NULL );

	int Generate( CDaoNamespace *outer = NULL );
	void HandleExtension( NamespaceDecl *nsdecl );

	void AddNamespace( CDaoNamespace *one ){ namespaces.push_back( one ); }
	void AddUserType( CDaoUserType *one ){
		usertypes.push_back( one );
		one->Generate();
	}
	void AddFunction( CDaoFunction *one ){
		functions.push_back( one );
		one->index = ++overloads[ one->funcDecl->getNameAsString() ];
		one->Generate();
	}
	void AddEnumDecl( EnumDecl *one ){ enums.push_back( one ); }
	void AddVarDecl( VarDecl *one ){ variables.push_back( one ); }

	void Sort( vector<CDaoUserType*> & sorted, map<CDaoUserType*,int> & check );
	void Sort( CDaoUserType *UT, vector<CDaoUserType*> & sorted, map<CDaoUserType*,int> & check );
};

#endif
