
#include <assert.h>
#include "cdaoModule.hpp"
#include "cdaoNamespace.hpp"

extern string cdao_string_fill( const string & tpl, const map<string,string> & subs );
extern string cdao_qname_to_idname( const string & qname );

CDaoNamespace::CDaoNamespace( CDaoModule *mod, const NamespaceDecl *decl )
{
	module = mod;
	nsdecl = (NamespaceDecl*) decl;
}
void CDaoNamespace::HandleExtension( NamespaceDecl *nsdecl )
{
	NamespaceDecl::decl_iterator it, end;
	if( not module->IsFromModules( nsdecl->getLocation() ) ) return;
	for(it=nsdecl->decls_begin(),end=nsdecl->decls_end(); it!=end; it++){
		if( not module->IsFromModules( (*it)->getLocation() ) ) return;
		if (VarDecl *var = dyn_cast<VarDecl>(*it)) {
			variables.push_back( var );
		}else if (EnumDecl *e = dyn_cast<EnumDecl>(*it)) {
			enums.push_back( e );
		}else if (FunctionDecl *func = dyn_cast<FunctionDecl>(*it)) {
			functions.push_back( new CDaoFunction( module, func ) );
			functions.back()->Generate();
		}else if (RecordDecl *record = dyn_cast<RecordDecl>(*it)) {
			usertypes.push_back( module->NewUserType( record ) );
			usertypes.back()->Generate();
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
		string name = func->funcDecl->getNameAsString();
		func->index = ++overloads[name];
		retcode |= func->Generate();
	}
	header = module->MakeHeaderCodes( usertypes );
	source = module->MakeSourceCodes( functions, this );
	source += module->MakeSourceCodes( usertypes, this );
	source2 = module->MakeSource2Codes( usertypes );
	source3 = module->MakeSource3Codes( usertypes );

	if( nsdecl ){
		string outer_name = outer && outer->nsdecl ? outer->nsdecl->getQualifiedNameAsString() : "ns";
		string this_name = nsdecl->getQualifiedNameAsString();
		string name = nsdecl->getNameAsString();
		string qname = this_name;

		outer_name = cdao_qname_to_idname( outer_name );
		this_name = cdao_qname_to_idname( this_name );
		if( (outer == NULL || outer->nsdecl == NULL) && name == "std" ) name = "stdcxx";

		onload += "\tDaoNameSpace *" + this_name + " = DaoNameSpace_GetNameSpace( ";
		onload += outer_name + ", \"" + name + "\" );\n";
		if( enums.size() ){
			source += module->MakeConstantStruct( enums, variables, qname );
			onload2 += "\tDaoNameSpace_AddConstNumbers( " + this_name;
			onload2 += ", dao_" + this_name + "_Nums );\n";
		}
	}else{
		if( enums.size() ){
			source += module->MakeConstantStruct( enums, variables );
			onload2 += "\tDaoNameSpace_AddConstNumbers( ns, dao__Nums );\n";
		}
	}

	onload3 += module->MakeOnLoadCodes( functions, this );
	for(i=0, n=namespaces.size(); i<n; i++){
		retcode |= namespaces[i]->Generate( this );
		header += namespaces[i]->header;
		source += namespaces[i]->source;
		source2 += namespaces[i]->source2;
		source3 += namespaces[i]->source3;
		onload += namespaces[i]->onload;
		onload2 += namespaces[i]->onload2;
		onload3 += namespaces[i]->onload3;
	}
	onload2 += module->MakeOnLoadCodes( usertypes, this );
	return retcode;
}
