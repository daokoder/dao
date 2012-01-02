
#ifndef __DAO_JIT_H__
#define __DAO_JIT_H__

#include<vector>
#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Instructions.h"
#include "llvm/Support/IRBuilder.h"


extern "C"{
#include"daoArray.h"
#include"daoVmcode.h"
#include"daoValue.h"
#include"daoRoutine.h"
#include"daoProcess.h"
#include"daoVmspace.h"
#include"daoGC.h"

void DaoJIT_Init( DaoVmSpace *vms, DaoJIT *jit );
void DaoJIT_Quit();

void DaoJIT_Free( DaoRoutine *routine );
void DaoJIT_Compile( DaoRoutine *routine );
void DaoJIT_Execute( DaoProcess *process, DaoJitCallData *data, int jitcode );
}

using namespace llvm;

struct DaoJitHandle : public IRBuilder<>
{
	DaoRoutine *routine;
	Function   *jitFunction;
	BasicBlock *entryBlock;
	BasicBlock *activeBlock;
	BasicBlock *secondBlock; // the block after the entry;
	BasicBlock *lastBlock;

	Value *localTypes; // process->activeTypes: DaoType*[]*
	Value *localValues; // process->activeValues: DaoValue*[]*
	Value *localConsts; // routine->routConsts->data: DaoValue[]*

	std::vector<Value*> localRefers; // DaoValue**
	std::vector<Value*> tempRefers; // int*, float*, double*, for intermediate operands
	std::vector<Value*> tempValues; // int, float, double, for intermediate operands

	DaoJitHandle( LLVMContext & ctx, DaoRoutine *rout=NULL ) : IRBuilder<>( ctx ){
		routine = rout;
	}

	Function* Compile( DaoRoutine *routine, int start, int end );

	Function* NewFunction( DaoRoutine *routine, int id );
	BasicBlock* NewBlock( int vmc );
	BasicBlock* NewBlock( DaoVmCodeX *vmc );
	void SetActiveBlock( BasicBlock *block );

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
	Value* GetValueItem( Value *array, Value *index );

	Value* CastIntegerValuePointer( Value *value ); // to DaoInteger*
	Value* CastFloatValuePointer( Value *value ); // to DaoFloat*
	Value* CastDoubleValuePointer( Value *value ); // to DaoDouble*

	void ClearTempOperand( int reg );
	void ClearTempOperand( DaoVmCodeX *vmc );
	void StoreTempResult( Value *value, Value *dest, int reg );
	
	void AddReturnCodeChecking( Value *retcode, int vmc );
	// index: dint; size: size_t* or int*;
	Value* AddIndexChecking( Value *index, Value *size, int vmc );

	Value* GetIntegerOperand( int reg ); // int
	Value* GetFloatOperand( int reg ); // float
	Value* GetDoubleOperand( int reg ); // double
	Value* GetIntegerLeftValue( int reg ); // int*
	Value* GetFloatLeftValue( int reg ); // float*
	Value* GetDoubleLeftValue( int reg ); // double*
	Value* GetTupleItems( int reg ); // DaoValue*[]*
	Value* GetListItem( int reg, int index, int vmc ); // DaoValue*
	Value* GetClassConstant( int reg, int field ); // Value*
	Value* GetClassStatic( int reg, int field ); // Value*
	Value* GetObjectConstant( int reg, int field ); // Value*
	Value* GetObjectStatic( int reg, int field ); // Value*
	Value* GetObjectVariable( int reg, int field ); // Value*
	void GetIntegerOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC );
	void GetFloatOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC );
	void GetDoubleOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC );
	void GetNNOperands( DaoVmCodeX *vmc, Value **dA, Value **dB );
	void GetFNNOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC );
	void GetDNNOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC );
};

#endif
