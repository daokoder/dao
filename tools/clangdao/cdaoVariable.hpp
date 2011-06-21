

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

class CDaoModule;

struct CDaoVariable
{
	CDaoModule  *module;
	VarDecl     *varDecl;

	int     index;
	bool    hasNullableHint;
	bool    hasArrayHint;
	bool    unsupport;
	string  name; // name from varDecl, or from hints;
	string  cxxdefault;
	string  cxxdefault2; // with macro expansion
	string  daodefault;

	string  daotype;
	string  cxxtype;
	string  cxxtype2;
	string  cxxcall;
	string  daopar;
	string  cxxpar;
	string  cxxpar_enum_virt;
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

	CDaoVariable( CDaoModule *mod = NULL, VarDecl *decl = NULL, int id = 0 );

	void SetDeclaration( VarDecl *decl );
	void SetHints( const string & hints );
	int Generate( int offset = 0 );
	int Generate2( int offset = 0 );
	int Generate( const BuiltinType *type, int offset = 0 );
	int Generate( const PointerType *type, int offset = 0 );
	int Generate( const ArrayType *type, int offset = 0 );
};

#endif
