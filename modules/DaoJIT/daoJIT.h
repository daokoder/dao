
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
void DaoJIT_Compile( DaoRoutine *routine, DaoOptimizer *optimizer );
void DaoJIT_Execute( DaoProcess *process, DaoJitCallData *data, int jitcode );
}

using namespace llvm;

struct DaoJitHandle : public IRBuilder<>
{
	DaoRoutine   *routine;
	DaoOptimizer *optimizer;
	DaoCodeNode  *currentNode;
	DaoCodeNode  *firstNode;
	DaoCodeNode  *lastNode;

	int start;
	int end;

	Function   *jitFunction;
	BasicBlock *entryBlock;
	BasicBlock *activeBlock;
	BasicBlock *secondBlock; // the block after the entry;
	BasicBlock *lastBlock;

	Value *localTypes; // process->activeTypes: DaoType*[]*
	Value *localValues; // process->activeValues: DaoValue*[]*
	Value *localConsts; // routine->routConsts->data: DaoValue[]*

	// Direct values: single-definition and single-use values.
	// Such values do not explicitly use the stack, and do not
	// store to and load from memory:
	std::vector<Value*> directValues; // DaoValue**

	// Stack values: multiple-definitions or multiple-uses values.
	// They are allocated in the entry block, and may have values loaded from memory.
	// Modification to the value is stored in the stack.
	// The values may be stored to memory, if necessary, at the exit of the JIT code.
	std::vector<Value*> stackValues; // DaoValue**

	std::vector<Value*> localRefers; // DaoValue**

	DaoJitHandle( LLVMContext & ctx, DaoRoutine *rout=NULL, DaoOptimizer *opt=NULL ) 
		: IRBuilder<>( ctx ){
		routine = rout;
		optimizer = opt;
		currentNode = NULL;
		lastNode = NULL;
	}

	Function* Compile( int start, int end );

	Function* NewFunction( DaoRoutine *routine, int id );
	BasicBlock* NewBlock( int vmc );
	BasicBlock* NewBlock( DaoVmCodeX *vmc );
	void SetActiveBlock( BasicBlock *block );

	void SetValueName( Value *value, const char *name, int id );

	Value* GetLocalConstant( int id );
	Value* GetUpConstant( int id );
	Value* GetLocalReference( int reg );
	Value* GetLocalValue( int reg );
	Value* GetLocalValueDataPointer( int reg );

	Value* GetLocalNumberValue( int reg, Type *type );

	Value* GetValueTypePointer( Value *value ); // DaoValue->type
	Value* GetValueDataPointer( Value *value ); // DaoValue->value: for int, float, double;

	// DaoInteger->value, DaoFloat->value, DaoDouble->value:
	Value* GetValueNumberPointer( Value *value, Type *type );

	Value* Dereference( Value *value );
	Value* CastIntegerPointer( Value *value ); // to dint*
	Value* CastFloatPointer( Value *value ); // to float*
	Value* CastDoublePointer( Value *value ); // to double*
	Value* GetValueItem( Value *array, Value *index );

	Value* CastIntegerValuePointer( Value *value ); // to DaoInteger*
	Value* CastFloatValuePointer( Value *value ); // to DaoFloat*
	Value* CastDoubleValuePointer( Value *value ); // to DaoDouble*

	int IsDirectValue( int reg );
	void StoreNumber( Value *value, int reg );

	void ClearTempOperand( int reg );
	void ClearTempOperand( DaoVmCodeX *vmc );
	
	void AddReturnCodeChecking( Value *retcode, int vmc );
	// index: dint; size: size_t* or int*;
	Value* AddIndexChecking( Value *index, Value *size, int vmc );

	Value* GetNumberOperand( int reg ); // int
	Value* GetTupleItems( int reg ); // DaoValue*[]*
	Value* GetListItem( int reg, int index, int vmc ); // DaoValue*
	Value* GetClassConstant( int reg, int field ); // Value*
	Value* GetClassStatic( int reg, int field ); // Value*
	Value* GetObjectConstant( int reg, int field ); // Value*
	Value* GetObjectStatic( int reg, int field ); // Value*
	Value* GetObjectVariable( int reg, int field ); // Value*

	Value* MoveValue( Value *dA, Value *dC, Type *type );
};

#endif
