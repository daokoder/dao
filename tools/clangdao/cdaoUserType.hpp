

#ifndef __CDAO_USERTYPE_H__
#define __CDAO_USERTYPE_H__

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

using namespace std;
using namespace llvm;
using namespace clang;

class CDaoModule;

struct CDaoUserType
{
	CDaoModule  *module;
	RecordDecl  *decl;

	CDaoUserType( CDaoModule *mod = NULL, RecordDecl *decl = NULL );

	void SetDeclaration( RecordDecl *decl );

	int Generate();
	int Generate( CXXRecordDecl *decl );
};

#endif
