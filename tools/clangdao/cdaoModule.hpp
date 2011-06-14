

#ifndef __CDAO_MODULE_H__
#define __CDAO_MODULE_H__

#include <clang/Lex/MacroInfo.h>
#include <clang/Basic/FileManager.h>
#include <string>

struct CDaoModule
{
	void HandleModuleName( const clang::MacroInfo *macro );
	void AddHeaderFile( const std::string & name, const clang::FileEntry *file );
	void AddFunctionHint( const std::string & name, const clang::MacroInfo *macro );
};

#endif
