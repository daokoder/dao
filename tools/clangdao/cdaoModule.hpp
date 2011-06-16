

#ifndef __CDAO_MODULE_H__
#define __CDAO_MODULE_H__

#include <clang/Basic/FileManager.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclGroup.h>
#include <clang/Frontend/CompilerInstance.h>
#include <string>
#include <vector>
#include <map>

using namespace std;
using namespace llvm;
using namespace clang;

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

	static map<string,int>  mapExtensions;

	CDaoModule( CompilerInstance *com, const string & path );

	int CheckFileExtension( const string & name );
	bool IsHeaderFile( const string & name );
	bool IsSourceFile( const string & name );
	bool CheckHeaderDependency();

	void HandleModuleDeclaration( const MacroInfo *macro );
	void HandleHeaderInclusion( SourceLocation loc, const string & name, const FileEntry *file );
	void HandleHintDefinition( const string & name, const MacroInfo *macro );

	void HandleVariable( const VarDecl & var );
	void HandleFunction( const FunctionDecl & funcdec );
};

#endif
