

#ifndef __CDAO_USERTYPE_H__
#define __CDAO_USERTYPE_H__

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

using namespace std;
using namespace llvm;
using namespace clang;

struct CDaoModule;

extern string cdao_qname_to_idname( const string & qname );

struct CDaoUserType
{
	CDaoModule  *module;
	RecordDecl  *decl;

	bool noWrapping;
	bool hasVirtual; // protected or public virtual function;
	bool isQObject;
	bool isQObjectBase;
	bool isRedundant;

	string  type_decls;
	string  type_codes;
	string  meth_decls;
	string  meth_codes;
	string  dao_meths;
	string  alloc_default;
	string  cxxWrapperVirt;
	string  typer_codes;

	CDaoUserType( CDaoModule *mod = NULL, RecordDecl *decl = NULL );

	void SetDeclaration( RecordDecl *decl );

	string GetName()const{ return decl ? decl->getNameAsString() : ""; }
	string GetQName()const{ return decl ? decl->getQualifiedNameAsString() : ""; }
	string GetIdName()const{ return cdao_qname_to_idname( GetQName() ); }
	string GetInputFile()const;

	void MakeTyperCodes();

	bool IsFromMainModule();
	int Generate();
	int Generate( CXXRecordDecl *decl );
};

#endif
