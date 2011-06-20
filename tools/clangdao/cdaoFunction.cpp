
#include "cdaoFunction.hpp"
#include "cdaoModule.hpp"

CDaoFunction::CDaoFunction( CDaoModule *mod, FunctionDecl *decl )
{
	module = mod;
	funcDecl = NULL;
	if( decl ) SetDeclaration( decl );
}
void CDaoFunction::SetDeclaration( FunctionDecl *decl )
{
	int i, n;
	funcDecl = decl;
	parlist.clear();
	if( decl == NULL ) return;
	for(i=0, n=decl->param_size(); i<n; i++){
		ParmVarDecl *pardecl = decl->getParamDecl( i );
		parlist.push_back( CDaoVariable( module, pardecl, i ) );
	}
}
int CDaoFunction::Generate()
{
	int retcode = 0;
	int i, n = parlist.size();
	for(i=0; i<n; i++) retcode |= parlist[i].Generate( 0 );
	return retcode;
}
