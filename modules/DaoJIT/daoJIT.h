
#ifndef __DAO_JIT_H__
#define __DAO_JIT_H__

#include<vector>

#define DAO_DIRECT_API

extern "C"{
#include"daoArray.h"
#include"daoOpcode.h"
#include"daoStdtype.h"
#include"daoRoutine.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoVmspace.h"

void DaoJIT_Init( DaoVmSpace *vms );
void DaoJIT_Quit();

void DaoJIT_Free( DaoRoutine *routine );
void DaoJIT_Compile( DaoRoutine *routine );
void DaoJIT_Execute( DaoContext *context, int jitcode );
}

#endif
