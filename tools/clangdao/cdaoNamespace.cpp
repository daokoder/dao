
#include <assert.h>
#include <clang/AST/DeclTemplate.h>
#include "cdaoModule.hpp"
#include "cdaoNamespace.hpp"

extern string cdao_string_fill( const string & tpl, const map<string,string> & subs );
extern string normalize_type_name( const string & name );
extern string cdao_qname_to_idname( const string & qname );
extern string cdao_remove_type_scopes( const string & qname );
extern string cdao_make_dao_template_type_name( const string & name );

CDaoNamespace::CDaoNamespace( CDaoModule *mod, const NamespaceDecl *decl )
{
	module = mod;
	nsdecl = (NamespaceDecl*) decl;
	varname = "ns";
	if( nsdecl ){
		varname = nsdecl->getQualifiedNameAsString();
		varname = cdao_qname_to_idname( varname );
	}
}
void CDaoNamespace::HandleExtension( NamespaceDecl *nsdecl )
{
	NamespaceDecl::decl_iterator it, end;
	for(it=nsdecl->decls_begin(),end=nsdecl->decls_end(); it!=end; it++){
		if (VarDecl *var = dyn_cast<VarDecl>(*it)) {
			variables.push_back( var );
		}else if (EnumDecl *e = dyn_cast<EnumDecl>(*it)) {
			enums.push_back( e );
		}else if (FunctionDecl *func = dyn_cast<FunctionDecl>(*it)) {
			AddFunction( new CDaoFunction( module, func ) );
		}else if (RecordDecl *record = dyn_cast<RecordDecl>(*it)) {
			QualType qtype( record->getTypeForDecl(), 0 );
			AddUserType( module->HandleUserType( qtype, record->getLocation() ) );
		}else if (NamespaceDecl *nsdecl = dyn_cast<NamespaceDecl>(*it)) {
			CDaoNamespace *ns = module->AddNamespace( nsdecl );
			if( ns ) namespaces.push_back( ns );
		}else if( TypedefDecl *decl = dyn_cast<TypedefDecl>(*it) ){
			module->HandleTypeDefine( decl );
		}
	}
}
int CDaoNamespace::Generate( CDaoNamespace *outer )
{
	int i, n, retcode = 0;
	for(i=0, n=usertypes.size(); i<n; i++) retcode |= usertypes[i]->Generate();
	for(i=0, n=functions.size(); i<n; i++){
		CDaoFunction *func = functions[i];
		if( func->generated || func->excluded ) continue;
		retcode |= func->Generate();
	}
#if 0
	map<string,int> check;
	for(i=0, n=usertypes.size(); i<n; i++){
		CDaoUserType *ut = usertypes[i];
		if( check.find( ut->qname ) != check.end() ) ut->isRedundant = true;
		if( ut->isRedundant || ut->IsFromRequiredModules() ) continue;
		check[ut->qname] = 1;
	}
#endif

	header = module->MakeHeaderCodes( usertypes );
	source = module->MakeSourceCodes( functions, this );
	//source += module->MakeSourceCodes( usertypes, this );
	//source2 = module->MakeSource2Codes( usertypes );
	//source3 = module->MakeSource3Codes( usertypes );

	if( nsdecl ){
		string this_name = varname;
		string outer_name = outer ? outer->varname : "ns";
		string qname = nsdecl->getQualifiedNameAsString();
		string name = nsdecl->getNameAsString();

		outer_name = cdao_qname_to_idname( outer_name );
		if( outer == NULL || outer->nsdecl == NULL ){
			if( name == "std" ) name = "stdcxx";
			onload += "\tDaoNamespace *" + this_name + " = DaoVmSpace_GetNamespace( ";
			onload += "vms, \"" + name + "\" );\n";
			onload2 += "\tDaoNamespace_AddConstValue( ns, \"" + name + "\", (DaoValue*) " + this_name + " );\n";
		}else{
			onload += "\tDaoNamespace *" + this_name + " = DaoNamespace_GetNamespace( ";
			onload += outer_name + ", \"" + name + "\" );\n";
		}
		if( enums.size() ){
			source += module->MakeConstantStruct( enums, variables, qname );
			onload2 += "\tDaoNamespace_AddConstNumbers( " + this_name;
			onload2 += ", dao_" + this_name + "_Nums );\n";
		}
	}else{
		if( enums.size() ){
			source += module->MakeConstantStruct( enums, variables );
			onload2 += "\tDaoNamespace_AddConstNumbers( ns, dao__Nums );\n";
		}
	}

	onload3 += module->MakeOnLoadCodes( functions, this );
	//onload3 += module->MakeOnLoad2Codes( usertypes );
	for(i=0, n=namespaces.size(); i<n; i++){
		retcode |= namespaces[i]->Generate( this );
		header += namespaces[i]->header;
		source += namespaces[i]->source;
		//source2 += namespaces[i]->source2;
		//source3 += namespaces[i]->source3;
		onload += namespaces[i]->onload;
		onload2 += namespaces[i]->onload2;
		onload3 += namespaces[i]->onload3;
	}
	//onload2 += module->MakeOnLoadCodes( this );

#if 0
	string code = "\tDaoNamespace_TypeDefine( " + varname + ", \"";
	for(i=0, n=usertypes.size(); i<n; i++){
		CDaoUserType *ut = usertypes[i];
		if( ut->isRedundant || ut->IsFromRequiredModules() ) continue;
		if( dyn_cast<ClassTemplateSpecializationDecl>( ut->decl ) == NULL ) continue;
		QualType qtype = QualType( ut->decl->getTypeForDecl(), 0 ).getCanonicalType();
		string qname = normalize_type_name( qtype.getAsString() );
		string name = cdao_remove_type_scopes( qname );
		string dname = cdao_make_dao_template_type_name( qname );
		string dname2 = cdao_make_dao_template_type_name( ut->name2 );
		if( name != ut->name2 ){
			outs() << ut->qname << " " << ut->name2 << " " << name << "\n";
			onload2 += code + dname + "\", \"" + dname2 + "\" );\n";
		}
	}
#endif
	return retcode;
}
void CDaoNamespace::Sort( vector<CDaoUserType*> & sorted, map<CDaoUserType*,int> & check )
{
	vector<CDaoUserType*>  sorted2;
	map<CDaoUserType*,int> check2;
	int i, n;
	for(i=0,n=usertypes.size(); i<n; i++){
		Sort( usertypes[i], sorted, check );
		Sort( usertypes[i], sorted2, check2 );
	}
	for(i=0,n=namespaces.size(); i<n; i++) namespaces[i]->Sort( sorted, check );
	usertypes.swap( sorted2 );
}
void CDaoNamespace::Sort( CDaoUserType *UT, vector<CDaoUserType*> & sorted, map<CDaoUserType*,int> & check )
{
	if( check.find( UT ) != check.end() ) return;
	RecordDecl *dd = UT->decl->getDefinition();
	if( dd && dd != UT->decl ){
		CDaoUserType *UT2 = module->GetUserType( dd );
		if( UT2 ) UT->priorUserTypes.push_back( UT2 );
	}

	int i, n = UT->priorUserTypes.size();
	//if( n ) outs() << n << "  " << UT->qname << "\n";
	for(i=0; i<n; i++) Sort( UT->priorUserTypes[i], sorted, check );
	sorted.push_back( UT );
	check[ UT ] = 1;
}
