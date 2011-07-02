

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

struct CDaoVariable
{
	CDaoModule  *module;

	QualType     qualType;
	const Expr  *initor;

	bool    isNullable;
	bool    hasArrayHint;
	bool    unsupport;
	bool    useDefault;
	string  name;
	string  cxxdefault;
	string  daodefault;

	string  daotype;
	string  cxxtype;
	string  cxxtype2;
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

	vector<string>  sizes;

	CDaoVariable( CDaoModule *mod = NULL, const VarDecl *decl = NULL );

	void SetDeclaration( const VarDecl *decl );
	void SetHints( const string & hints );
	int Generate( int daopar_index = 0, int cxxpar_index = 0 );
	int Generate2( int daopar_index = 0, int cxxpar_index = 0 );
	int Generate( const BuiltinType *type, int daopar_index = 0, int cxxpar_index = 0 );
	int Generate( const PointerType *type, int daopar_index = 0, int cxxpar_index = 0 );
	int Generate( const ReferenceType *type, int daopar_index = 0, int cxxpar_index = 0 );
	int Generate( const ArrayType *type, int daopar_index = 0, int cxxpar_index = 0 );
	int GenerateForArray( QualType elemtype, string size, int daopar_index = 0, int cxxpar_index = 0 );
	int GenerateForArray( QualType elemtype, string size, string size2, int daopar_index = 0, int cxxpar_index = 0 );
};

#endif
