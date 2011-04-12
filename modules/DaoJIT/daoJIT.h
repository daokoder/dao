
#ifndef __DAO_JIT_H__
#define __DAO_JIT_H__

#include<vector>
#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Instructions.h"
#include "llvm/Support/IRBuilder.h"

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

using namespace llvm;

struct DaoJitHandle : public IRBuilder<>
{
	Function   *jitFunction;
	BasicBlock *entryBlock;
	BasicBlock *activeBlock;

	Value *localValues; // context->regValues: DValue*[]*
	Value *localConsts; // routine->routConsts->data: DValue[]*

	std::vector<Value*> localRefers;

	DaoJitHandle( LLVMContext & ctx ) : IRBuilder<>( ctx ){}

	Function* Compile( DaoRoutine *routine, int start, int end );

	Function* NewFunction( DaoRoutine *routine, int id );

	Value* GetLocalConstant( int id );
	Value* GetLocalReference( int reg );
	Value* GetLocalValue( int reg );
	Value* GetLocalValueDataPointer( int reg );

	Value* GetValueTypePointer( Value *value ); // & DValue->t
	Value* GetValueDataPointer( Value *value ); // & DValue->v.d

	Value* Dereference( Value *value );
	Value* CastIntegerPointer( Value *value ); // to dint*
	Value* CastFloatPointer( Value *value ); // to float*
	Value* CastDoublePointer( Value *value ); // to double*

	void GetIntegerOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC );
	void GetFloatOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC );
	void GetDoubleOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC );
};

#endif
