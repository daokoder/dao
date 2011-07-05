

#ifndef __CDAO_MODULE_H__
#define __CDAO_MODULE_H__

#include <clang/Basic/FileManager.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclGroup.h>
#include <clang/AST/DeclCXX.h>
#include <clang/Frontend/CompilerInstance.h>
#include <ostream>
#include <string>
#include <vector>
#include <map>

#include "cdaoFunction.hpp"
#include "cdaoUserType.hpp"
#include "cdaoNamespace.hpp"

using namespace std;
using namespace llvm;
using namespace clang;

extern string cdao_qname_to_idname( const string & qname );

struct CDaoModuleInfo
{
	string      name;
	string      path;
	FileEntry  *entry;
};
struct CDaoHeaderInfo
{
	string      path;
	FileEntry  *entry;

	CDaoHeaderInfo( const string & p="", FileEntry *f = NULL ){
		path = p;
		entry = f;
	}
};
struct CDaoInclusionInfo
{
	FileEntry  *includer;
	FileEntry  *includee;

	CDaoInclusionInfo( FileEntry *f1 = NULL, FileEntry *f2 = NULL ){
		includer = f1;
		includee = f2;
	}
	CDaoInclusionInfo( const CDaoInclusionInfo & other ){
		includer = other.includer;
		includee = other.includee;
	}
	bool operator<( const CDaoInclusionInfo & other )const{
		if( includer != other.includer ) return includer < other.includer;
		return includee < other.includee;
	}
};

struct CDaoModule
{
	CompilerInstance  *compiler;
	CDaoModuleInfo     moduleInfo;

	map<FileEntry*,CDaoModuleInfo>  requiredModules; // directly required modules;
	map<FileEntry*,CDaoModuleInfo>  requiredModules2; // directly/indirectly required modules;

	map<FileEntry*,CDaoHeaderInfo>  headers; // header files from this module;
	map<FileEntry*,CDaoHeaderInfo>  extHeaders; // header files from the required modules;
	map<CDaoInclusionInfo,int>      inclusions;
	map<string,vector<string> >     functionHints;

	vector<CDaoNamespace*>  namespaces;
	vector<CDaoUserType*>   usertypes;
	vector<CDaoFunction>    functions;
	vector<CDaoFunction*>   callbacks;
	vector<EnumDecl*>       enums;
	vector<VarDecl*>        variables;

	map<const FunctionProtoType*,CDaoFunction*> allCallbacks;

	vector<CDaoUserType*>                     allUsertypes;
	map<const RecordDecl*,CDaoUserType*>      allUsertypes2;
	map<const NamespaceDecl*,CDaoNamespace*>  allNamespaces;

	static map<string,int>  mapExtensions;

	CDaoModule( CompilerInstance *com, const string & path );

	CDaoUserType* GetUserType( const RecordDecl *decl );
	CDaoUserType* NewUserType( RecordDecl *decl );
	CDaoNamespace* GetNamespace( const NamespaceDecl *decl );
	CDaoNamespace* NewNamespace( NamespaceDecl *decl );
	CDaoNamespace* AddNamespace( NamespaceDecl *decl );

	int Generate();

	int CheckFileExtension( const string & name );
	bool IsHeaderFile( const string & name );
	bool IsSourceFile( const string & name );
	bool IsFromModules( SourceLocation loc );
	bool IsFromMainModule( SourceLocation loc );
	bool IsFromModuleSources( SourceLocation loc );
	bool CheckHeaderDependency();

	string GetFileName( SourceLocation );

	void HandleModuleDeclaration( const MacroInfo *macro );
	void HandleHeaderInclusion( SourceLocation loc, const string & name, const FileEntry *file );
	void HandleHintDefinition( const string & name, const MacroInfo *macro );

	void HandleVariable( VarDecl *var );
	void HandleEnum( EnumDecl *decl );
	void HandleFunction( FunctionDecl *funcdec );
	void HandleUserType( RecordDecl *record );
	void HandleNamespace( NamespaceDecl *nsdecl );
	void HandleTypeDefine( TypedefDecl *decl );

	void WriteHeaderIncludes( std::ostream & stream );

	string MakeDaoFunctionPrototype( FunctionDecl *funcdec );

	string MakeHeaderCodes( vector<CDaoUserType*> & usertypes );
	string MakeSourceCodes( vector<CDaoUserType*> & usertypes, CDaoNamespace *ns = NULL );
	string MakeSource2Codes( vector<CDaoUserType*> & usertypes );
	string MakeSource3Codes( vector<CDaoUserType*> & usertypes );
	string MakeOnLoadCodes( vector<CDaoUserType*> & usertypes, CDaoNamespace *ns = NULL );

	string MakeSourceCodes( vector<CDaoFunction> & functions, CDaoNamespace *ns = NULL );
	string MakeOnLoadCodes( vector<CDaoFunction> & functions, CDaoNamespace *ns = NULL );
	string MakeConstantItems( vector<EnumDecl*> & enums, vector<VarDecl*> & vars, const string & name = "" );
	string MakeConstantStruct( vector<EnumDecl*> & enums, vector<VarDecl*> & vars, const string & name = "" );

	string ExtractSource( SourceLocation & start, SourceLocation & end, bool original = true );
	string ExtractSource( const SourceRange & range, bool original = true );

	static string GetQName( const NamedDecl *D ){ return D ? D->getQualifiedNameAsString() : ""; }
	static string GetIdName( const NamedDecl *D ){ return cdao_qname_to_idname( GetQName( D ) ); }
};

#endif
