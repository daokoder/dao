
#include <assert.h>
#include "cdaoModule.hpp"
#include "cdaoNamespace.hpp"

CDaoNamespace::CDaoNamespace( CDaoModule *mod, NamespaceDecl *decl )
{
	module = mod;
	nsdecl = decl;
}
void CDaoNamespace::Extract( NamespaceDecl *nsdecl )
{
	CXXRecordDecl::decl_iterator it, end;
	for(it=nsdecl->decls_begin(),end=nsdecl->decls_end(); it!=end; it++){
		if( not module->IsFromModules( (*it)->getLocation() ) ) return;
		if (VarDecl *var = dyn_cast<VarDecl>(*it)) {
		}else if (FunctionDecl *func = dyn_cast<FunctionDecl>(*it)) {
			functions.push_back( CDaoFunction( module, func ) );
		}else if (CXXRecordDecl *record = dyn_cast<CXXRecordDecl>(*it)) {
			usertypes.push_back( CDaoUserType( module, record ) );
		}else if (RecordDecl *record = dyn_cast<RecordDecl>(*it)) {
		}else if (NamespaceDecl *nsdecl = dyn_cast<NamespaceDecl>(*it)) {
			namespaces.push_back( CDaoNamespace( module, nsdecl ) );
		}
	}
}
int CDaoNamespace::Generate()
{
	outs() << "namespace: " << nsdecl->getNameAsString() << "\n";
	assert( nsdecl == nsdecl->getOriginalNamespace() );
	for(NamespaceDecl *dec=nsdecl; dec; dec=dec->getNextNamespace()) Extract( dec );

	map<string,int> overloads;
	int i, n, retcode = 0;
	for(i=0, n=functions.size(); i<n; i++){
		string name = functions[i].funcDecl->getNameAsString();
		functions[i].index = ++overloads[name];
		retcode |= functions[i].Generate();
	}
	for(i=0, n=usertypes.size(); i<n; i++) retcode |= usertypes[i].Generate();
	for(i=0, n=namespaces.size(); i<n; i++) retcode |= namespaces[i].Generate();
	return retcode;
}
