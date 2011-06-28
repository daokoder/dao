
#include <assert.h>
#include "cdaoModule.hpp"
#include "cdaoNamespace.hpp"

extern string cdao_string_fill( const string & tpl, const map<string,string> & subs );
extern string cdao_qname_to_idname( const string & qname );

CDaoNamespace::CDaoNamespace( CDaoModule *mod, NamespaceDecl *decl )
{
	module = mod;
	nsdecl = decl;
	index = 0;
}
void CDaoNamespace::HandleExtension( NamespaceDecl *nsdecl )
{
	CXXRecordDecl::decl_iterator it, end;
	if( not module->IsFromModules( nsdecl->getLocation() ) ) return;
	for(it=nsdecl->decls_begin(),end=nsdecl->decls_end(); it!=end; it++){
		if( not module->IsFromModules( (*it)->getLocation() ) ) return;
		if (VarDecl *var = dyn_cast<VarDecl>(*it)) {
		}else if (FunctionDecl *func = dyn_cast<FunctionDecl>(*it)) {
			functions.push_back( CDaoFunction( module, func ) );
		}else if (CXXRecordDecl *record = dyn_cast<CXXRecordDecl>(*it)) {
			usertypes.push_back( module->NewUserType( record ) );
		}else if (RecordDecl *record = dyn_cast<RecordDecl>(*it)) {
		}else if (NamespaceDecl *nsdecl = dyn_cast<NamespaceDecl>(*it)) {
			CDaoNamespace *ns = module->AddNamespace( nsdecl );
			if( ns ) namespaces.push_back( ns );
		}
	}
}
int CDaoNamespace::Generate( CDaoNamespace *outer )
{
	outs() << "namespace: " << nsdecl->getQualifiedNameAsString() << "\n";

	map<string,int> overloads;
	int i, n, retcode = 0;
	for(i=0, n=functions.size(); i<n; i++){
		string name = functions[i].funcDecl->getNameAsString();
		functions[i].index = ++overloads[name];
		retcode |= functions[i].Generate();
	}
	header = module->MakeHeaderCodes( usertypes );
	source = module->MakeSourceCodes( functions, this );
	source += module->MakeSourceCodes( usertypes, this );
	source2 = module->MakeSource2Codes( usertypes );
	source3 = module->MakeSource3Codes( usertypes );

	string outer_name = outer ? outer->nsdecl->getQualifiedNameAsString() : "ns";
	string this_name = nsdecl->getQualifiedNameAsString();

	outer_name = cdao_qname_to_idname( outer_name );
	this_name = cdao_qname_to_idname( this_name );

	onload += "\tDaoNameSpace *" + this_name + " = DaoNameSpace_GetNameSpace( ";
	onload += outer_name + ", \"" + nsdecl->getNameAsString() + "\" );\n";
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
