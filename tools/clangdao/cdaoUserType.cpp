
#include<map>

#include "cdaoFunction.hpp"
#include "cdaoUserType.hpp"

CDaoUserType::CDaoUserType( CDaoModule *mod, RecordDecl *decl )
{
	module = mod;
	SetDeclaration( decl );
}
void CDaoUserType::SetDeclaration( RecordDecl *decl )
{
	this->decl = decl;
}
int CDaoUserType::Generate()
{
	RecordDecl *dd = decl->getDefinition();
	outs() << decl->getNameAsString() << ": " << (void*)decl <<" " << (void*)dd << "\n";
	if( dd and dd != decl ) return 0;
	if (CXXRecordDecl *record = dyn_cast<CXXRecordDecl>(decl)) return Generate( record );
	return 1;
}
int CDaoUserType::Generate( CXXRecordDecl *decl )
{
	map<string,int> overloads;
	vector<CDaoFunction>  methods;
	CXXRecordDecl::method_iterator methit, methend = decl->method_end();
	for(methit=decl->method_begin(); methit!=methend; methit++){
		string name = methit->getNameAsString();
		methods.push_back( CDaoFunction( module, *methit, ++overloads[name] ) );
		methods.back().Generate();
	}
	return 1;
}
