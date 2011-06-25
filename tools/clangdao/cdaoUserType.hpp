

#ifndef __CDAO_USERTYPE_H__
#define __CDAO_USERTYPE_H__

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

using namespace std;
using namespace llvm;
using namespace clang;

class CDaoModule;

struct CDaoMethodDecl
{
	const CXXMethodDecl  *decl;

	string  prototype;
	string  wrapper;

	CDaoMethodDecl( const CXXMethodDecl *d=NULL, const string & p="", const string & w = "" ){
		decl = d;
		prototype = p;
		wrapper = w;
	}
};

struct CDaoUserType
{
	CDaoModule  *module;
	RecordDecl  *decl;

	string  type_decls;
	string  type_codes;
	string  meth_decls;
	string  meth_codes;
	string  dao_meths;
	string  alloc_default;
	string  cxxWrapperVirt;

	map<string,CDaoMethodDecl>  declmeths;
	map<string,CDaoMethodDecl>  declvirts;

	CDaoUserType( CDaoModule *mod = NULL, RecordDecl *decl = NULL );

	void SetDeclaration( RecordDecl *decl );

	int Generate();
	int Generate( CXXRecordDecl *decl );
};

#endif
