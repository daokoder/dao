

#ifndef __CDAO_USERTYPE_H__
#define __CDAO_USERTYPE_H__

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

using namespace std;
using namespace llvm;
using namespace clang;

struct CDaoModule;
struct CDaoNamespace;

extern string cdao_qname_to_idname( const string & qname );

enum CDaoUserTypeWrapType
{
	CDAO_WRAP_TYPE_NONE ,   // none wrapping;
	CDAO_WRAP_TYPE_OPAQUE , // wrap as opaque type;
	CDAO_WRAP_TYPE_DIRECT , // wrap for direct member accessing;
	CDAO_WRAP_TYPE_PROXY    // wrap through a proxy struct or class;
};

struct CDaoUserTypeAlias
{
};

struct CDaoWrapName
{
	string  nspace;
	string  name;

	CDaoWrapName( const string & ns = "", const string & s = "" ){
		nspace = ns;
		name = s;
	}
};

struct CDaoUserType
{
	CDaoModule     *module;
	RecordDecl     *decl;
	SourceLocation  location;

	string  nspace;
	vector<CDaoWrapName>  names;

	short  wrapType;
	short  wrapCount;
	bool   forceOpaque;
	bool   isRedundant;
	bool   isQObject;
	bool   isQObjectBase;

	string  name;  // just name: vector, SomeClass;
	string  name2; // name, with template arguments if any: vector<int>, SomeClass;
	string  qname; // qualified name: std::vector<int>, SomeNamespace::SomeClass;
	string  idname; // identification name: std_0_vector_1_int_2_, SomeNamespace_0_SomeClass;

	string  type_decls;
	string  type_codes;
	string  meth_decls;
	string  meth_codes;
	string  dao_meths;
	string  alloc_default;
	string  cxxWrapperVirt;
	string  typer_codes;

	vector<CDaoUserType*>   priorUserTypes;
	vector<CXXMethodDecl*>  pureVirtuals;

	CDaoUserType( CDaoModule *mod = NULL, const RecordDecl *decl = NULL );

	void SetDeclaration( RecordDecl *decl );
	void SetNamespace( const CDaoNamespace *ns );

	string GetName()const{ return decl ? decl->getNameAsString() : ""; }
	string GetQName()const{ return decl ? decl->getQualifiedNameAsString() : ""; }
	string GetIdName()const{ return cdao_qname_to_idname( GetQName() ); }
	string GetInputFile()const;

	void MakeTyperCodes();

	bool IsFromMainModule();
	bool IsFromRequiredModules();
	void Clear();
	int Generate();
	int Generate( RecordDecl *decl );
	int Generate( CXXRecordDecl *decl );
	int GenerateSimpleTyper();
	void SetupDefaultMapping( map<string,string> & kvmap );
};

#endif
