
#include<assert.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>

#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetSelect.h"

#include "daoJIT.h"

using namespace llvm;

LLVMContext     *llvm_context = NULL;
Module          *llvm_module = NULL;
ExecutionEngine *llvm_exe_engine = NULL;

const Type *int8_type = NULL;
const Type *int32_type = NULL;
const Type *dint_type = NULL; // 32 or 64 bits
const Type *float_type = NULL;
const Type *double_type = NULL;
const VectorType *int8_vector2 = NULL;

const Type *void_type = NULL; // i8*

const StructType *dao_value_type = NULL; // DValue
const StructType *dao_base_type = NULL; // DaoBase
const StructType *dao_context_type = NULL; // DaoContext

const PointerType *void_type_p = NULL; // DValue*

const PointerType *dao_value_type_p = NULL; // DValue*
const PointerType *dao_value_type_pp = NULL; // DValue**

const PointerType *dao_base_type_p = NULL; // DaoBase*
const PointerType *dao_context_type_p = NULL; // DaoContext*

const ArrayType *dao_value_ptr_array_type = NULL; // DValue*[]
const PointerType *dao_value_ptr_array_type_p = NULL; // DValue*[]*

extern "C"{
DValue** DaoContext_GetLocalValues( DaoContext *daoctx )
{
	printf( "DaoContext_GetLocalValues: %p\n", daoctx );
	return daoctx->regValues;
}
}
Function *dao_context_get_local_values = NULL;

void DaoJIT_Init( DaoVmSpace *vms )
{
	printf( "DaoJIT_Init()\n" );
	InitializeNativeTarget();
	llvm_context = new LLVMContext();
	llvm_module = new Module("DaoJIT", *llvm_context);

	int8_type = Type::getInt8Ty( *llvm_context );
	int32_type = Type::getInt32Ty( *llvm_context );
	int8_vector2 = VectorType::get( int8_type, 2 );

	dint_type = int32_type;
	if( sizeof(void*) == 8 ) dint_type = Type::getInt64Ty( *llvm_context );
	float_type = Type::getFloatTy( *llvm_context );
	double_type = Type::getDoubleTy( *llvm_context );

	void_type = Type::getVoidTy( *llvm_context );
	void_type_p = PointerType::getUnqual( int8_type );

	std::vector<const Type*> dvalue_types( 4, int8_type );
	dvalue_types.push_back( void_type_p );

	// type { i8, i8, i8, i8, i8* }
	dao_value_type = StructType::get( *llvm_context, dvalue_types );
	dao_value_type_p = PointerType::getUnqual( dao_value_type );
	dao_value_type_pp = PointerType::getUnqual( dao_value_type_p );

	dao_value_ptr_array_type = ArrayType::get( dao_value_type_p, 0 );
	dao_value_ptr_array_type_p = PointerType::getUnqual( dao_value_ptr_array_type );


	std::vector<const Type*> base_types( 2, int8_type );
	base_types.push_back( int8_vector2 );
	base_types.push_back( int32_type );
	base_types.push_back( int32_type );

	// type { i8, i8, <2 x i8>, i32, i32 }
	dao_base_type = StructType::get( *llvm_context, base_types );
	dao_base_type_p = PointerType::getUnqual( dao_base_type );

	base_types.push_back( void_type_p );
	base_types.push_back( void_type_p );
	base_types.push_back( void_type_p );
	base_types.push_back( void_type_p );
	base_types.push_back( dao_value_ptr_array_type_p );

	// type { i8, i8, <2 x i8>, i32, i32, i8*, i8*, i8*, i8*, [0 x %1*]* }
	dao_context_type = StructType::get( *llvm_context, base_types );
	dao_context_type_p = PointerType::getUnqual( dao_context_type );
	//dao_context_type->setName( "DaoContext" );

	std::vector<const Type*> Double1( 1, dao_context_type_p );
	FunctionType *FT = FunctionType::get(void_type,Double1,false);
	dao_context_get_local_values = Function::Create( FT, Function::ExternalLinkage, "DaoContext_GetLocalValues", llvm_module);


	llvm_exe_engine = EngineBuilder( llvm_module ).setEngineKind(EngineKind::JIT).create();
	llvm_exe_engine->addGlobalMapping( dao_context_get_local_values, 
			(void*)(DaoContext_GetLocalValues));
}

LoadInst* DaoJIT_Dereference( Value *pvalue, BasicBlock *block )
{
	return new LoadInst( pvalue, "value", block );
}
GetElementPtrInst* DaoJIT_GetElementPointer( Value *pvalue, int id, BasicBlock *block )
{
	Value *ids[2];
	ids[0] = ConstantInt::get(Type::getInt32Ty(*llvm_context), 0);
	ids[1] = ConstantInt::get(Type::getInt32Ty(*llvm_context), id);
	return GetElementPtrInst::Create( pvalue, ids, ids+2, "pfield", block );
}
Function* DaoJIT_NewFunction( DaoRoutine *routine, int id )
{
	//char buf[100];
	char *name = routine->routName->mbs;
	Function *jitFunc = cast<Function>( llvm_module->getOrInsertFunction( 
				name, void_type, dao_context_type_p, (Type *)0));

	BasicBlock *BB = BasicBlock::Create(*llvm_context, "EntryBlock", jitFunc);
	Argument *ctx = jitFunc->arg_begin();
	ctx->setName("ctx");

	GetElementPtrInst *pppvalues = DaoJIT_GetElementPointer( ctx, 9, BB );
	DaoJIT_Dereference( pppvalues, BB );
	return jitFunc;
}
LoadInst* DaoJIT_GetLocalValue( Function *jitFunc, int id, BasicBlock *block )
{
	BasicBlock *entryBlock = jitFunc->begin();
	BasicBlock::iterator it = entryBlock->begin(); // ctx
	Instruction *ppvalues = ++it; // ctx->regValues
	// ctx->regValues + id
	GetElementPtrInst *ppvalue = DaoJIT_GetElementPointer( ppvalues, id, block );
	LoadInst *pvalue = new LoadInst( ppvalue, "pvalue", block );
	return pvalue;
}
GetElementPtrInst* DaoJIT_GetValueDataPointer( Value *pvalue, BasicBlock *block )
{
	return DaoJIT_GetElementPointer( pvalue, 4, block );
}
CastInst* DaoJIT_CastInteger( Value *pvalue, BasicBlock *block )
{
	const PointerType *type = PointerType::getUnqual( dint_type );
	return CastInst::CreatePointerCast( pvalue, type, "todint", block );
}
CastInst* DaoJIT_CastFloat( Value *pvalue, BasicBlock *block )
{
	const PointerType *type = PointerType::getUnqual( float_type );
	return CastInst::CreatePointerCast( pvalue, type, "tofloat", block );
}
CastInst* DaoJIT_CastDouble( Value *pvalue, BasicBlock *block )
{
	const PointerType *type = PointerType::getUnqual( dint_type );
	return CastInst::CreatePointerCast( pvalue, type, "todouble", block );
}



void DaoJIT_Compile( DaoRoutine *routine )
{
	DaoVmCodeX **vmcs = routine->annotCodes->items.pVmc;
	DaoVmCodeX *vmc;
	int i, N = routine->annotCodes->size;
	for(i=0; i<N; i++){
		vmc = vmcs[i];
		if( vmc->code == DVM_MOVE_II ){
		}
	}
}
void DaoJIT_Execute( DaoContext *context, DaoRoutine *routine, int jitcode )
{
}

