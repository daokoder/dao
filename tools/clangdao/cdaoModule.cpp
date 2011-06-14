
#include <llvm/Support/raw_ostream.h>
#include <clang/Basic/IdentifierTable.h>
#include <iostream>
#include <string>
#include "cdaoModule.hpp"

using namespace llvm;
using namespace clang;

void CDaoModule::HandleModuleName( const clang::MacroInfo *macro )
{
	std::string name( macro->arg_begin()[0]->getName() );
	outs() << "DAO_MODULE_NAME is expanded with "<< name << "\n";
}
void CDaoModule::AddHeaderFile( const std::string & name, const FileEntry *file )
{
}
void CDaoModule::AddFunctionHint( const std::string & name, const MacroInfo *macro )
{
	std::string tokString;
	raw_string_ostream ss( tokString );
	const Token & signature = macro->getReplacementToken( 0 );
	ss.write( signature.getLiteralData(), signature.getLength() );
	outs() << name << " is defined\n";
	outs() << ss.str() << "\n";
	//outs() << tokString << "\n"; // this alone does not work.
}
