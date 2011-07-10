
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/raw_ostream.h>
#include <clang/Basic/IdentifierTable.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/Sema/Sema.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <iostream>
#include <fstream>
#include <string>
#include "cdaoModule.hpp"

const string cxx_get_object_method = 
"DaoMethod* Dao_Get_Object_Method( DaoCData *cd, DValue *obj, const char *name )\n\
{\n\
  DaoMethod *meth;\n\
  if( cd == NULL ) return NULL;\n\
  obj->v.object = DaoCData_GetObject( cd );\n\
  if( obj->v.object == NULL ) return NULL;\n\
  obj->t = DAO_OBJECT;\n\
  meth = DaoObject_GetMethod( obj->v.object, name );\n\
  if( meth == NULL ) return NULL;\n\
  if( meth->type != DAO_METAROUTINE && meth->type != DAO_ROUTINE ) return NULL;\n\
  return meth;\n\
}\n";

const string add_number = "  { \"$(name)\", $(type), $(namespace)$(value) },\n";
const string tpl_typedef = "  DaoNameSpace_TypeDefine( $(ns), \"$(old)\", \"$(new)\" );\n";
const string add_ccdata = "  DaoNameSpace_AddConstData( $(ns), \"$(name)\", "
"(DaoBase*)DaoCData_Wrap( dao_$(type)_Typer, ($(type)*) $(refer) $(name) ) );\n";

const string ext_typer = "extern DaoTypeBase *dao_$(type)_Typer;\n";
const string alias_typer = "DaoTypeBase *dao_$(new)_Typer = & $(old)_Typer;\n";


extern string cdao_qname_to_idname( const string & qname );


enum CDaoFileExtensionType
{
	CDAO_FILE_HEADER , // c++ header without suffix
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
	"",
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

extern string cdao_string_fill( const string & tpl, const map<string,string> & subs );

CDaoModule::CDaoModule( CompilerInstance *com, const string & path ) : topLevelScope( this )
{
	finalGenerating = false;
	compiler = com;
	moduleInfo.path = path;
	for(int i=CDAO_FILE_H; i<=CDAO_FILE_MM; i++)
		mapExtensions[ cdao_file_extensions[i] ] = i;
}
CDaoUserType* CDaoModule::GetUserType( const RecordDecl *decl )
{
	map<const RecordDecl*,CDaoUserType*>::iterator it = allUsertypes.find( decl );
	if( it == allUsertypes.end() ) return NULL;
	return it->second;
}
CDaoNamespace* CDaoModule::GetNamespace( const NamespaceDecl *decl )
{
	map<const NamespaceDecl*,CDaoNamespace*>::iterator it = allNamespaces.find( decl );
	if( it == allNamespaces.end() ) return NULL;
	return it->second;
}
CDaoUserType* CDaoModule::NewUserType( RecordDecl *decl )
{
	CDaoUserType *ut = new CDaoUserType( this, decl );
	allUsertypes[ decl ] = ut;
	return ut;
}
CDaoNamespace* CDaoModule::NewNamespace( NamespaceDecl *decl )
{
	CDaoNamespace *ns = new CDaoNamespace( this, decl );
	ns->HandleExtension( decl );
	allNamespaces[ decl ] = ns;
	return ns;
}
CDaoNamespace* CDaoModule::AddNamespace( NamespaceDecl *decl )
{
	NamespaceDecl *orins = decl->getOriginalNamespace();
	if( decl != orins ){
		map<const NamespaceDecl*,CDaoNamespace*>::iterator find = allNamespaces.find( orins );
		if( find == allNamespaces.end() ) return NULL;
		find->second->HandleExtension( decl );
		return NULL;
	}
	return NewNamespace( decl );
}
CDaoUserType* CDaoModule::HandleUserType( QualType qualtype, SourceLocation loc )
{
	qualtype = qualtype.getLocalUnqualifiedType();
	QualType canotype = qualtype.getCanonicalType();
	if( const RecordType *record = dyn_cast<RecordType>( canotype.getTypePtr() ) ){
		ClassTemplateSpecializationDecl *SD, *DE;
		//outs()<<">>>>>>>> "<<qualtype.getAsString()<<" "<<canotype.getAsString()<< "\n";
		if( (SD = dyn_cast<ClassTemplateSpecializationDecl>( record->getDecl() ) ) ){
			//outs() << (void*)SD << ": " << GetFileName( SD->getLocation() ) << "\n";
			if( GetUserType( SD ) == NULL ){
				DE = cast_or_null<ClassTemplateSpecializationDecl>( SD->getDefinition());
				if( DE == NULL ){
					TemplateSpecializationKind kind = TSK_ExplicitInstantiationDefinition;
					Sema & sm = compiler->getSema();
					sm.InstantiateClassTemplateSpecialization( loc, SD, kind );
				}
				SD->setPointOfInstantiation( loc );
				//outs() << (void*)SD << " " << (void*)DE << "\n";

				CDaoUserType *UT = NewUserType( SD );
				UT->UpdateName( qualtype.getAsString() );

				if( NamespaceDecl *ND = cast_or_null<NamespaceDecl>( SD->getParent() ) ){
					CDaoNamespace *NS = GetNamespace( ND );
					if( NS == NULL ) NS = AddNamespace( ND );
					NS->AddUserType( UT );
				}else{
					topLevelScope.AddUserType( UT );
				}
			}
		}
		return GetUserType( record->getDecl() );
	}
	return NULL;
}
int CDaoModule::CheckFileExtension( const string & name )
{
	string ext;
	size_t i = name.size();
	while( i && name[i-1] != '.' ) i--;
	if( i == 0 ) return CDAO_FILE_HEADER;
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
string CDaoModule::GetFileName( SourceLocation loc )
{
	SourceManager & sourceman = compiler->getSourceManager();
	FileID fid = sourceman.getFileID( sourceman.getSpellingLoc( loc ) );
	FileEntry *e = (FileEntry*) sourceman.getFileEntryForID( fid );
	if( e == NULL ) return "";
	return e->getName();
}
bool CDaoModule::IsFromModules( SourceLocation loc )
{
	SourceManager & sourceman = compiler->getSourceManager();
	FileID fid = sourceman.getFileID( sourceman.getSpellingLoc( loc ) );
	FileEntry *e = (FileEntry*) sourceman.getFileEntryForID( fid );
	bool is = e == moduleInfo.entry;
	is = is or requiredModules2.find( e ) != requiredModules2.end();
	is = is or headers.find( e ) != headers.end();
	is = is or extHeaders.find( e ) != extHeaders.end();
	return is;
}
bool CDaoModule::IsFromMainModule( SourceLocation loc )
{
	SourceManager & sourceman = compiler->getSourceManager();
	FileID fid = sourceman.getFileID( sourceman.getSpellingLoc( loc ) );
	FileEntry *e = (FileEntry*) sourceman.getFileEntryForID( fid );
	return e == moduleInfo.entry or headers.find( e ) != headers.end();
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
	map<CDaoInclusionInfo,int>::iterator it, it2, end = inclusions.end();
	for(it=inclusions.begin(); it!=end; it++){
		FileEntry *includer = it->first.includer;
		FileEntry *includee = it->first.includee;
		//outs()<<includer->getName() <<" "<<includee->getName()<<"\n";
		if( includee == moduleInfo.entry ){
			errs() << "Error: main module file is included by other files!\n";
			return false;
		}
		if( headers.find( includer ) != headers.end() ){
			string name = includer->getName();
			string path = includer->getDir()->getName();
			name.erase( 0, path.size() );
			it2 = inclusions.find( CDaoInclusionInfo( moduleInfo.entry, includer ) );
			if( CheckFileExtension( name ) == CDAO_FILE_HEADER && it2 != end ){
				if( headers.find( includee ) == headers.end() ){
					headers[ includee ] = CDaoHeaderInfo( "", includee );
					extHeaders.erase( includee );
				}
			}
		}
		if( headers.find( includee ) != headers.end() ){
			if( includer == moduleInfo.entry ) continue;
			if( headers.find( includer ) != headers.end() ) continue;
			errs() << "Warning: wrapping header file \""<< includee->getName();
			errs() << "\" is included by external file \"" << includer->getName() << "\"!\n";
			//return false;
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

		map<FileEntry*,CDaoHeaderInfo>::iterator it = extHeaders.find( entryHeader );
		if( it != extHeaders.end() ) extHeaders.erase( it );
	}else if( requiredModules2.find( entryInclude ) != requiredModules2.end() ){
		if( headers.find( entryHeader ) != headers.end() ) return;
		if( extHeaders.find( entryHeader ) != extHeaders.end() ) return;
		extHeaders[ entryHeader ] = CDaoHeaderInfo( name, entryHeader );
	}
	//outs() << name << " is included\n";
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
		tok::TokenKind kind = tok.getKind();
		bool id = kind == tok::identifier || kind >= tok::kw_auto;
		if( lastiden && id ) proto += " ";
		lastiden = id;
		proto += pp.getSpelling( tok );
	}
	functionHints[ proto ] = hints;
	outs() << "function hint is defined for \"" << proto << "\"\n";
}
void CDaoModule::HandleVariable( VarDecl *var )
{
	//outs() << var->getNameAsString() << "\n";
	topLevelScope.AddVarDecl( var );
}
void CDaoModule::HandleEnum( EnumDecl *decl )
{
	topLevelScope.AddEnumDecl( decl );
}
void CDaoModule::HandleFunction( FunctionDecl *funcdec )
{
	//outs() << funcdec->getNameAsString() << " has "<< funcdec->param_size() << " parameters\n";
	topLevelScope.AddFunction( new CDaoFunction( this, funcdec ) );
}
void CDaoModule::HandleUserType( RecordDecl *record )
{
	//outs() << "UserType: " << record->getNameAsString() << "\n";
	//outs() << (void*)record << " " << (void*)record->getDefinition() << "\n";
	topLevelScope.AddUserType( NewUserType( record ) );
}
void CDaoModule::HandleNamespace( NamespaceDecl *nsdecl )
{
	CDaoNamespace *ns = AddNamespace( nsdecl );
	if( ns ) topLevelScope.AddNamespace( ns );
}
void CDaoModule::HandleTypeDefine( TypedefDecl *decl )
{
	QualType qtype = decl->getUnderlyingType();
	QualType qtype2 = qtype.IgnoreParens();
	if( qtype2->isPointerType() ) qtype2 = qtype2->getPointeeType();
	qtype2 = qtype2.IgnoreParens();
	if( const FunctionProtoType *ft = dyn_cast<FunctionProtoType>( qtype2.getTypePtr() ) ){
		if( allCallbacks.find( ft ) == allCallbacks.end() ){
			allCallbacks[ ft ] = new CDaoFunction( this );

			CDaoFunction *func = allCallbacks[ ft ];
			string qname = decl->getNameAsString();
			func->SetCallback( (FunctionProtoType*)ft, NULL, qname );
			func->cxxName = cdao_qname_to_idname( qname );
			if( func->retype.callback == "" ){
				errs() << "Warning: callback \"" << qname << "\" is not supported!\n";
				func->excluded = true;
			}
			//if( func->excluded == false ) func->Generate();
		}
	}
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
		if( it->second.path == "" ) continue;
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

string CDaoModule::MakeHeaderCodes( vector<CDaoUserType*> & usertypes )
{
	int i, n;
	string codes;
	map<string,string> kvmap;
	for(i=0, n=usertypes.size(); i<n; i++){
		CDaoUserType & utp = *usertypes[i];
		if( utp.isRedundant ) continue;
		kvmap[ "type" ] = utp.GetName();
		codes += cdao_string_fill( ext_typer, kvmap );
	}
	for(i=0, n=usertypes.size(); i<n; i++){
		CDaoUserType & utp = *usertypes[i];
		if( utp.isRedundant || not utp.IsFromMainModule() ) continue;
		if( utp.type_decls.size() ) codes += cdao_string_fill( utp.type_decls, kvmap );
	}
	return codes;
}
string CDaoModule::MakeSourceCodes( vector<CDaoUserType*> & usertypes, CDaoNamespace *ns )
{
	int i, n;
	string codes, idname;
	map<string,string> kvmap;
	if( ns && ns->nsdecl ) idname = cdao_qname_to_idname( ns->nsdecl->getQualifiedNameAsString() );
	//codes += ifdef_cpp_open;
	codes += "static DaoTypeBase *dao_" + idname + "_Typers[] = \n{\n";
	for(i=0, n=usertypes.size(); i<n; i++){
		CDaoUserType & utp = *usertypes[i];
		if( utp.isRedundant || not utp.IsFromMainModule() ) continue;
		codes += "\tdao_" + utp.GetIdName() + "_Typer,\n";
	}
	codes += "\tNULL\n};\n";
	//codes += ifdef_cpp_close;
	return codes;
}
string CDaoModule::MakeSource2Codes( vector<CDaoUserType*> & usertypes )
{
	int i, n;
	string codes;
	map<string,string> kvmap;
	
	codes += ifdef_cpp_open;
	for(i=0, n=usertypes.size(); i<n; i++){
		CDaoUserType & utp = *usertypes[i];
		if( utp.isRedundant || not utp.IsFromMainModule() ) continue;
		codes += "/* " + utp.GetInputFile() + " */\n";
		codes += cdao_string_fill( utp.typer_codes, kvmap );
		codes += cdao_string_fill( utp.meth_codes, kvmap );
	}
	codes += ifdef_cpp_close;
	return codes;
}
string CDaoModule::MakeSource3Codes( vector<CDaoUserType*> & usertypes )
{
	int i, n;
	string codes;
	map<string,string> kvmap;
	for(i=0, n=usertypes.size(); i<n; i++){
		CDaoUserType & utp = *usertypes[i];
		if( utp.isRedundant || not utp.IsFromMainModule() ) continue;
		codes += cdao_string_fill( utp.type_codes, kvmap );
	}
	return codes;
}
string CDaoModule::MakeOnLoadCodes( vector<CDaoUserType*> & usertypes, CDaoNamespace *ns )
{
	string codes, tname, nsname = "ns";
	if( ns && ns->nsdecl ) tname = nsname = cdao_qname_to_idname( ns->nsdecl->getQualifiedNameAsString() );
	codes += "\tDaoNameSpace_WrapTypes( " + nsname + ", dao_" + tname + "_Typers );\n";
	return codes;
}
string CDaoModule::MakeSourceCodes( vector<CDaoFunction*> & functions, CDaoNamespace *ns )
{
	int i, n;
	string func_decl;
	string rout_entry;
	string func_codes;
	string codes, idname;
	if( ns && ns->nsdecl ) idname = cdao_qname_to_idname( ns->nsdecl->getQualifiedNameAsString() );
	for(i=0, n=functions.size(); i<n; i++){
		CDaoFunction & func = *functions[i];
		if( func.excluded || not func.IsFromMainModule() ) continue;
		func_decl += func.cxxProtoCodes + ";\n";
		func_codes += func.cxxWrapper;
		rout_entry += func.daoProtoCodes;
	}
	codes += ifdef_cpp_open;
	codes += func_decl;
	codes += "static DaoFuncItem dao_" + idname + "_Funcs[] = \n{\n" + rout_entry;
	codes += "  { NULL, NULL }\n};\n";
	codes += func_codes;
	codes += ifdef_cpp_close;
	return codes;
}
string CDaoModule::MakeOnLoadCodes( vector<CDaoFunction*> & functions, CDaoNamespace *ns )
{
	string codes, tname, nsname = "ns";
	if( ns && ns->nsdecl ) tname = nsname = cdao_qname_to_idname( ns->nsdecl->getQualifiedNameAsString() );
	codes += "\tDaoNameSpace_WrapFunctions( " + nsname + ", dao_" + tname + "_Funcs );\n";
	return codes;
}
string CDaoModule::MakeConstantItems( vector<EnumDecl*> & enums, vector<VarDecl*> & vars, const string & qname )
{
	int i, n;
	string qname2 = qname.size() ? qname + "::" : "";
	string codes;
	for(i=0, n=enums.size(); i<n; i++){
		EnumDecl *decl = enums[i];
		EnumDecl *dd = (EnumDecl*) decl->getDefinition();
		// TODO: Add type define;
		if( dd == NULL && not IsFromMainModule( decl->getLocation() ) ) continue;
		if( not IsFromMainModule( dd->getLocation() ) ) continue;
		if( dd == NULL ) continue;
		EnumDecl::enumerator_iterator it, end = dd->enumerator_end();
		for(it=dd->enumerator_begin(); it!=end; it++){
			string item = it->getNameAsString();
			codes += "  { \"" + item + "\", DAO_INTEGER, " + qname2 + item + " },\n";
		}
	}
	for(i=0, n=vars.size(); i<n; i++){
		VarDecl *decl = vars[i];
		QualType qtype = decl->getType();
		string item = decl->getNameAsString();
		if( not qtype.isConstQualified() ) continue;
		qtype = qtype.getCanonicalType();
		const BuiltinType *type = dyn_cast<BuiltinType>( qtype.getTypePtr() );
		if( type == NULL ) continue;
		string tidname;
		if( type->isInteger() ){
			tidname = "DAO_INTEGER";
		}else if( type->getKind() == BuiltinType::Float ){
			tidname = "DAO_FLOAT";
		}else if( type->isFloatingPoint() ){
			tidname = "DAO_DOUBLE";
		}
		codes += "  { \"" + item + "\", " + tidname + ", " + qname2 + item + " },\n";
	}
	return codes;
}
string CDaoModule::MakeConstantStruct( vector<EnumDecl*> & enums, vector<VarDecl*> & vars, const string & qname )
{
	string idname = cdao_qname_to_idname( qname );
	string codes = "static DaoNumItem dao_" + idname + "_Nums[] = \n{\n";
	return codes + MakeConstantItems( enums, vars, qname ) + "  { NULL, 0, 0 }\n};\n";
}

int CDaoModule::Generate()
{
	finalGenerating = true;
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

	map<string,int> overloads;
	int i, n, retcode = 0;
	topLevelScope.Generate();
	for(i=0, n=callbacks.size(); i<n; i++) retcode |= callbacks[i]->Generate();

	map<string,string> kvmap;
	kvmap[ "module" ] = moduleInfo.name;

	fout_header << topLevelScope.header;
	fout_source3 << cxx_get_object_method;
	map<string,CDaoProxyFunction> & proxy_functions = CDaoProxyFunction::proxy_functions;
	map<string,CDaoProxyFunction>::iterator pit, pend = proxy_functions.end();
	for(pit=proxy_functions.begin(); pit!=pend; pit++){
		CDaoProxyFunction & proxy = pit->second;
		if( proxy.used ) fout_source3 << proxy.codes;
	}
	fout_header << ifdef_cpp_open;
	map<const FunctionProtoType*,CDaoFunction*>::iterator cbit, cbend = allCallbacks.end();
	for(cbit=allCallbacks.begin(); cbit!=cbend; cbit++){
		CDaoFunction *func = cbit->second;
		if( func->excluded ) continue;
		fout_header << func->cxxWrapperVirtProto;
		fout_source3 << func->cxxWrapperVirt;
	}
	fout_header << ifdef_cpp_close;

	fout_source << topLevelScope.source;
	fout_source2 << topLevelScope.source2;
	fout_source3 << topLevelScope.source3;
	fout_header << "#endif\n";

	fout_source << ifdef_cpp_open;
	string onload = "DaoOnLoad";
	fout_source << "int " << onload << "( DaoVmSpace *vms, DaoNameSpace *ns )\n{\n";
	fout_source << "\t__daoVmSpace = vms;\n";
	fout_source << topLevelScope.onload;
	fout_source << topLevelScope.onload2;
	fout_source << topLevelScope.onload3;

#if 0
	if( lib_name == LibType.LIB_QT ){
		fout_source.write( "   qRegisterMetaType<DaoQtMessage>(\"DaoQtMessage\");\n" );
	}
#endif
	fout_source << "\treturn 0;\n}\n";

	fout_source << ifdef_cpp_close;

	return retcode;
}
string CDaoModule::MakeDaoFunctionPrototype( FunctionDecl *funcdec )
{
	return "";
}
string CDaoModule::ExtractSource( SourceLocation & start, SourceLocation & end, bool original )
{
	Preprocessor & pp = compiler->getPreprocessor();
	SourceManager & sm = compiler->getSourceManager();
	if( original ){
		start = sm.getInstantiationLoc( start );
		end = sm.getInstantiationLoc( end );
	}else{
		start = sm.getSpellingLoc( start );
		end = sm.getSpellingLoc( end );
	}

	string source;
	const char *p = sm.getCharacterData( start );
	const char *q = sm.getCharacterData( pp.getLocForEndOfToken( end ) );
	for(; p!=q; p++) source += *p;
	return source;
}
string CDaoModule::ExtractSource( const SourceRange & range, bool original )
{
	SourceLocation start = range.getBegin();
	SourceLocation end = range.getEnd();
	return ExtractSource( start, end, original );
}
