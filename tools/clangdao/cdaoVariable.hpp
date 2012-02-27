

#ifndef __CDAO_VARIABLE_H__
#define __CDAO_VARIABLE_H__

#include <clang/AST/Type.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclGroup.h>
#include <string>

#include "cdaoVariable.hpp"

#define VAR_INDEX_RETURN  -1
#define VAR_INDEX_FIELD   -2

using namespace std;
using namespace llvm;
using namespace clang;

struct CDaoModule;
struct CDaoUserType;

struct CDaoVariable
{
	CDaoModule     *module;
	CDaoUserType   *hostype;
	QualType        qualtype;
	SourceLocation  location;
	const Expr     *initor;

	bool    isArithmeticType;
	bool    isPointerType;
	bool    isObjectType;
	bool    isNullable;
	bool    isCallback;
	bool    isUserData; // callback userdata
	bool    hasArrayHint;
	bool    unsupported;
	bool    useDefault;
	bool    useDaoString;
	string  name;
	string  cxxdefault;
	string  daodefault;

	string  daotype;
	string  cxxtype; // original
	string  cxxtype2; // stripped off pointer, refernce, ...
	string  cxxtyper; // typer name for user type;
	string  cxxcall;
	string  daopar;
	string  cxxpar;
	string  dao2cxx;
	string  cxx2dao;
	string  ctxput;
	string  parset;
	string  getres;
	string  getter;
	string  setter;
	string  dao_itemtype;
	string  get_item;
	string  set_item;

	string          callback;
	vector<string>  sizes;
	vector<string>  scopes;

	CDaoVariable( CDaoModule *mod = NULL, const VarDecl *decl = NULL );

	void SetQualType( QualType qtype, SourceLocation loc = SourceLocation() );
	void SetDeclaration( const VarDecl *decl );
	void SetHints( const string & hints );

	int Generate( int daopar_index = 0, int cxxpar_index = 0 );
	int Generate2( int daopar_index = 0, int cxxpar_index = 0 );
	int GenerateForBuiltin( int daopar_index = 0, int cxxpar_index = 0 );
	int GenerateForPointer( int daopar_index = 0, int cxxpar_index = 0 );
	int GenerateForReference( int daopar_index = 0, int cxxpar_index = 0 );
	int GenerateForArray( int daopar_index = 0, int cxxpar_index = 0 );
	int GenerateForArray( QualType elemtype, string size, int daopar_index = 0, int cxxpar_index = 0 );
	int GenerateForArray( QualType elemtype, string size, string size2, int dpid = 0, int cpid = 0 );
	int GenerateForArray2( QualType elemtype, string size, string size2, int dpid = 0, int cpid = 0 );

	void MakeCxxParameter( string & prefix, string & suffix );
	void MakeCxxParameter( QualType qtype, string & prefix, string & suffix );
	QualType GetStrippedType( QualType qtype );
	string GetStrippedTypeName( QualType qtype );
};

#endif
