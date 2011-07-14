

#ifndef __CDAO_USERTYPE_H__
#define __CDAO_USERTYPE_H__

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

using namespace std;
using namespace llvm;
using namespace clang;

struct CDaoModule;

extern string cdao_qname_to_idname( const string & qname );

enum CDaoUserTypeWrapType
{
	CDAO_WRAP_TYPE_NONE ,   // none wrapping;
	CDAO_WRAP_TYPE_OPAQUE , // wrap as opaque type;
	CDAO_WRAP_TYPE_DIRECT , // wrap for direct member accessing;
	CDAO_WRAP_TYPE_PROXY    // wrap through a proxy struct or class;
};

struct CDaoUserType
{
	CDaoModule     *module;
	RecordDecl     *decl;
	SourceLocation  location;

	short  wrapType;
	short  wrapCount;
	bool   isRedundant;
	bool   isQObject;
	bool   isQObjectBase;

	string  name;
	string  qname;
	string  idname;

	string  type_decls;
	string  type_codes;
	string  meth_decls;
	string  meth_codes;
	string  dao_meths;
	string  alloc_default;
	string  cxxWrapperVirt;
	string  typer_codes;

	vector<CXXMethodDecl*>  pureVirtuals;

	CDaoUserType( CDaoModule *mod = NULL, RecordDecl *decl = NULL );

	void SetDeclaration( RecordDecl *decl );
	void UpdateName( const string & writtenName );

	string GetName()const{ return decl ? decl->getNameAsString() : ""; }
	string GetQName()const{ return decl ? decl->getQualifiedNameAsString() : ""; }
	string GetIdName()const{ return cdao_qname_to_idname( GetQName() ); }
	string GetInputFile()const;

	void MakeTyperCodes();

	bool IsFromMainModule();
	void Clear();
	int Generate();
	int Generate( RecordDecl *decl );
	int Generate( CXXRecordDecl *decl );
	int GenerateSimpleTyper();
	void SetupDefaultMapping( map<string,string> & kvmap );
};

#endif
