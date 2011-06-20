
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/raw_ostream.h>
#include <clang/Basic/IdentifierTable.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <iostream>
#include <fstream>
#include <string>
#include "cdaoModule.hpp"

enum CDaoFileExtensionType
{
	CDAO_FILE_H ,    // .h
	CDAO_FILE_HH ,   // .hh
	CDAO_FILE_HPP ,  // .hpp
	CDAO_FILE_HXX ,  // .hxx
	CDAO_FILE_C ,    // .c
	CDAO_FILE_CC ,   // .cc
	CDAO_FILE_CPP ,  // .cpp
	CDAO_FILE_CXX ,  // .cxx
	CDAO_FILE_CXX2 , // .c++
	CDAO_FILE_M ,    // .m
	CDAO_FILE_MM ,   // .mm
	CDAO_FILE_OTHER
};
const char *const cdao_file_extensions[] = 
{
	".h" ,
	".hh" ,
	".hpp" ,
	".hxx" ,
	".c" ,
	".cc" ,
	".cpp" ,
	".cxx" ,
	".c++" ,
	".m" ,
	".mm" 
};

map<string,int> CDaoModule::mapExtensions;

CDaoModule::CDaoModule( CompilerInstance *com, const string & path )
{
	compiler = com;
	moduleInfo.path = path;
	for(int i=CDAO_FILE_H; i<=CDAO_FILE_MM; i++)
		mapExtensions[ cdao_file_extensions[i] ] = i;
}
int CDaoModule::CheckFileExtension( const string & name )
{
	string ext;
	size_t i, k;
	for(i=name.size(), k=0; i; i--, k++){
		if( name[i-1] == '.' ) break;
		if( k >= 4 ) return CDAO_FILE_OTHER;
	}
	if( i == 0 ) return CDAO_FILE_OTHER;
	for(i-=1; i<name.size(); i++) ext += tolower( name[i] );
	if( mapExtensions.find( ext ) == mapExtensions.end() ) return CDAO_FILE_OTHER;
	return mapExtensions[ext];
}
bool CDaoModule::IsHeaderFile( const string & name )
{
	return CheckFileExtension( name ) <= CDAO_FILE_HXX;
}
bool CDaoModule::IsSourceFile( const string & name )
{
	int extype = CheckFileExtension( name );
	return extype >= CDAO_FILE_C && extype <= CDAO_FILE_MM;
}
bool CDaoModule::IsFromModules( SourceLocation loc )
{
	SourceManager & sourceman = compiler->getSourceManager();
	FileID fid = sourceman.getFileID( loc );
	FileEntry *e = (FileEntry*) sourceman.getFileEntryForID( fid );
	bool is = e == moduleInfo.entry;
	is = is or requiredModules2.find( e ) != requiredModules2.end();
	is = is or headers.find( e ) != headers.end();
	is = is or extHeaders.find( e ) != extHeaders.end();
	return is;
}
bool CDaoModule::IsFromModuleSources( SourceLocation loc )
{
	SourceManager & sourceman = compiler->getSourceManager();
	FileID fid = sourceman.getFileID( loc );
	FileEntry *e = (FileEntry*) sourceman.getFileEntryForID( fid );
	return e == moduleInfo.entry or requiredModules2.find( e ) != requiredModules2.end();
}
bool CDaoModule::CheckHeaderDependency()
{
	map<CDaoInclusionInfo,int>::iterator it, end = inclusions.end();
	for(it=inclusions.begin(); it!=end; it++){
		FileEntry *includer = it->first.includer;
		FileEntry *includee = it->first.includee;
		//outs()<<includer->getName() <<" "<<includee->getName()<<"\n";
		if( includee == moduleInfo.entry ){
			errs() << "Error: main module file is included by other files!\n";
			return false;
		}
		if( headers.find( includee ) != headers.end() ){
			if( includer == moduleInfo.entry ) continue;
			if( headers.find( includer ) != headers.end() ) continue;
			errs() << "Error: wrapping header file \""<< includee->getName();
			errs() << "\" is included by external file \"" << includer->getName() << "\"!\n";
			return false;
		}
	}
	return true;
}
void CDaoModule::HandleModuleDeclaration( const MacroInfo *macro )
{
	SourceManager & sourceman = compiler->getSourceManager();
	SourceLocation loc = macro->getDefinitionLoc();
	FileID fid = sourceman.getFileID( loc );
	FileEntry *entry = (FileEntry*) sourceman.getFileEntryForID( fid );
	string name = macro->getReplacementToken( 0 ).getIdentifierInfo()->getName();
	if( sourceman.isFromMainFile( loc ) ){
		if( moduleInfo.name.size() ) return;
		moduleInfo.name = name;
		moduleInfo.entry = entry;
		outs() << "main module \"" << moduleInfo.path << "\" is named as " << name << "\n";
		return;
	}else if( requiredModules2.find( entry ) != requiredModules2.end() ){
		CDaoModuleInfo & mod = requiredModules[ entry ];
		CDaoModuleInfo & mod2 = requiredModules2[ entry ];
		mod.name = mod2.name = name;
		mod.entry = mod2.entry = entry;
		outs() << "module \"" << mod.path << "\" is named as " << name << "\n";
	}
}
void CDaoModule::HandleHeaderInclusion( SourceLocation loc, const string & name, const FileEntry *file )
{
	SourceManager & sourceman = compiler->getSourceManager();
	FileID fidInclude = sourceman.getFileID( loc );
	FileEntry *entryHeader = (FileEntry*) file;
	FileEntry *entryInclude = (FileEntry*) sourceman.getFileEntryForID( fidInclude );

	inclusions[ CDaoInclusionInfo( entryInclude, entryHeader ) ] = 1;
	if( IsSourceFile( name ) ){
		if( requiredModules2.find( entryHeader ) != requiredModules2.end() ) return;
		if( sourceman.isFromMainFile( loc ) ){
			requiredModules[ entryHeader ] = CDaoModuleInfo();
			requiredModules[ entryHeader ].path = name;
			requiredModules[ entryHeader ].entry = entryHeader;
			requiredModules2[ entryHeader ] = requiredModules[ entryHeader ];
		}else{
			requiredModules2[ entryHeader ] = CDaoModuleInfo();
			requiredModules2[ entryHeader ].path = name;
			requiredModules2[ entryHeader ].entry = entryHeader;
		}
		return;
	}
	if( not IsHeaderFile( name ) ) return;
	if( sourceman.isFromMainFile( loc ) ){
		if( headers.find( entryHeader ) != headers.end() ) return;
		headers[ entryHeader ] = CDaoHeaderInfo( name, entryHeader );
	}else if( requiredModules2.find( entryInclude ) != requiredModules2.end() ){
		if( extHeaders.find( entryHeader ) != extHeaders.end() ) return;
		extHeaders[ entryHeader ] = CDaoHeaderInfo( name, entryHeader );
	}
	outs() << name << " is included\n";
}
void CDaoModule::HandleHintDefinition( const string & name, const MacroInfo *macro )
{
	Preprocessor & pp = compiler->getPreprocessor();
	if( not IsFromModuleSources( macro->getDefinitionLoc() ) ) return;

	string proto;
	vector<string> hints;
	MacroInfo::arg_iterator argiter;
	hints.push_back( name );
	for(argiter=macro->arg_begin(); argiter!=macro->arg_end(); argiter++){
		hints.push_back( argiter[0]->getNameStart() );
	}
	bool lastiden = false;
	MacroInfo::tokens_iterator tokiter;
	for(tokiter=macro->tokens_begin(); tokiter!=macro->tokens_end(); tokiter++){
		Token tok = *tokiter;
		if( lastiden && tok.isAnyIdentifier() ) proto += " ";
		lastiden = tok.isAnyIdentifier();
		proto += pp.getSpelling( tok );
	}
	functionHints[ proto ] = hints;
	outs() << "function hint is defined for \"" << proto << "\"\n";
}
void CDaoModule::HandleVariable( VarDecl *var )
{
	outs() << var->getNameAsString() << "\n";
}
void CDaoModule::HandleFunction( FunctionDecl *funcdec )
{
	if( not IsFromModules( funcdec->getLocation() ) ) return;
	outs() << funcdec->getNameAsString() << " has "<< funcdec->param_size() << " parameters\n";
	functions.push_back( CDaoFunction( this, funcdec ) );
}
void CDaoModule::WriteHeaderIncludes( std::ostream & fout_header )
{
	string name_macro = UppercaseString( moduleInfo.name );

	fout_header << "#ifndef __DAO_" << name_macro << "_H__\n";
	fout_header << "#define __DAO_" << name_macro << "_H__\n";
	fout_header << "#include<stdlib.h>\n";
	fout_header << "#include<assert.h>\n";
	fout_header << "#include<string.h>\n";
	fout_header << "#include<dao.h>\n\n";

	map<FileEntry*,CDaoHeaderInfo>::iterator it, end = headers.end();
	for(it=headers.begin(); it != end; it++){
		fout_header << "#include\"" << it->second.path << "\"\n"; // TODO: angular
	}
	fout_header << "\n";
	map<FileEntry*,CDaoModuleInfo>::iterator it2, end2 = requiredModules.end();
	for(it2=requiredModules.begin(); it2 != end2; it2++){
		string name_macro2 = UppercaseString( it2->second.name );
		fout_header << "#ifndef DAO_" << name_macro2 << "_STATIC\n";
		fout_header << "#define DAO_DLL_" << name_macro2 << " DAO_DLL_IMPORT\n";
		fout_header << "#include\"dao_" << it2->second.name << ".h\"\n";
		fout_header << "#else\n";
		fout_header << "#define DAO_DLL_" << name_macro2 << "\n";
		fout_header << "#include\"dao_" << it2->second.name + ".h\"\n";
		fout_header << "#endif\n";
	}
	fout_header << "\n#ifndef DAO_" << name_macro << "_STATIC\n";
	fout_header << "#ifndef DAO_DLL_" << name_macro << "\n";
	fout_header << "#define DAO_DLL_" << name_macro << " DAO_DLL_EXPORT\n";
	fout_header << "#endif\n";
	fout_header << "#else\n";
	fout_header << "#define DAO_DLL_" << name_macro << "\n";
	fout_header << "#endif\n\n";
	fout_header << "extern DaoVmSpace *__daoVmSpace;\n";
}

const char *ifdef_cpp_open = "#ifdef __cplusplus\nextern \"C\"{\n#endif\n";
const char *ifdef_cpp_close = "#ifdef __cplusplus\n}\n#endif\n";

int CDaoModule::Generate()
{
	if( CheckHeaderDependency() == false ) return 1;

	// TODO: other extension, output dir
	int fetype = CheckFileExtension( moduleInfo.path );
	string festring = cdao_file_extensions[fetype];
	string fname_header = "dao_" + moduleInfo.name + ".h";
	string fname_source = "dao_" + moduleInfo.name + festring;
	string fname_source2 = "dao_" + moduleInfo.name + "2" + festring;
	string fname_source3 = "dao_" + moduleInfo.name + "3" + festring;

	ofstream fout_header( fname_header.c_str() );
	ofstream fout_source( fname_source.c_str() );
	ofstream fout_source2( fname_source2.c_str() );
	ofstream fout_source3( fname_source3.c_str() );

	WriteHeaderIncludes( fout_header );

	fout_source << "#include\"" << fname_header << "\"\n";
	fout_source2 << "#include\"" << fname_header << "\"\n";
	fout_source3 << "#include\"" << fname_header << "\"\n";

	fout_source << "DAO_INIT_MODULE;\nDaoVmSpace *__daoVmSpace = NULL;\n";

	int i, n, retcode = 0;
	for(i=0, n=functions.size(); i<n; i++) retcode |= functions[i].Generate();

	return retcode;
}
string CDaoModule::MakeDaoFunctionPrototype( FunctionDecl *funcdec )
{
	return "";
}
