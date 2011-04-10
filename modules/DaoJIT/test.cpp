
#include"daoJIT.h"
#include"daoNamespace.h"
#include<stdio.h>


#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/PassManager.h"
#include "llvm/CallingConv.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

extern LLVMContext     *llvm_context;
extern Module          *llvm_module;
extern ExecutionEngine *llvm_exe_engine;

extern const Type *float_type;

LoadInst* DaoJIT_Dereference( Value *pvalue, BasicBlock *block );
GetElementPtrInst* DaoJIT_GetElementPointer( Value *pvalue, int id, BasicBlock *block );
Function* DaoJIT_NewFunction( DaoRoutine *routine, int id );
LoadInst* DaoJIT_GetLocalValue( Function *jitFunc, int id, BasicBlock *block );
GetElementPtrInst* DaoJIT_GetValueDataPointer( Value *pvalue, BasicBlock *block );
CastInst* DaoJIT_CastInteger( Value *pvalue, BasicBlock *block );
CastInst* DaoJIT_CastFloat( Value *pvalue, BasicBlock *block );
CastInst* DaoJIT_CastDouble( Value *pvalue, BasicBlock *block );

extern Function *dao_context_get_local_values;

void simple_tests( DaoContext *ctx )
{
	Value *cst, *cast, *value, *fvalue, *field, *pfield, *pfloat;
	Function *func = DaoJIT_NewFunction( ctx->routine, 0 );
	BasicBlock *block = func->begin();
	// value = ctx->regValues[1]
	value = DaoJIT_GetLocalValue( func, 1, block );

	// pfield = & value->v.p;
	pfield = DaoJIT_GetValueDataPointer( value, block );
	// pfloat = & value->v.f;
	pfloat = DaoJIT_CastFloat( pfield, block );

	cst = ConstantInt::get(Type::getInt32Ty(*llvm_context), 999);
	cast = new SIToFPInst( cst, float_type, "tofloat", block );

	fvalue = DaoJIT_Dereference( pfloat, block );
	Instruction *add = BinaryOperator::CreateFAdd( fvalue, cast, "addresult", block);
	// *pfloat = 999;
	StoreInst *store = new StoreInst( add, pfloat, block );

	CallInst *daoctx = CallInst::Create(dao_context_get_local_values, func->arg_begin(), "", block);
	ReturnInst::Create( *llvm_context, block );

	verifyModule(*llvm_module, PrintMessageAction);
	PassManager PM;
	PM.add(createPrintModulePass(&outs()));
	PM.run(*llvm_module);
	std::vector<GenericValue> Args(1);
	Args[0].PointerVal = ctx;
	GenericValue GV = llvm_exe_engine->runFunction( func, Args);
	outs() << "Result: " << GV.IntVal << "\n";
}

void DaoJIT_SearchCompilable( DaoRoutine *routine, std::vector<IndexRange> & segments );


const char* dao_source = 
"a = 11.2\n"
"b = 22.3\n"
"c = a + b\n"
"if( c ){\n"
"d = c + 1.0\n"
"}\n"
"for( i = 1 : 5){ e = i; break; e = i + 1; }\n"
;

const char* dao_source3 = 
"function getSomeString(){ return 'StringFromDao' }";

int main( int argc, char *argv[] )
{
	DString *src;
	DaoVmSpace *vms;
	DaoNameSpace *ns;
	DaoNameSpace *ns2;
	DaoVmProcess *vmp;

	// Search and load the Dao library.
	// DaoInitLibrary() can take a parameter which is the path
	// to the dynamic loading file of the Dao library.
	// If the parameter is NULL, the current path is searched,
	// then the path defined by environment variable DAO_DIR,
	// then $(HOME)/dao, and then the default system path:
	// /usr/local/dao/ or C:\dao\.
	//
	// With direct APIs, the example must be linked against the Dao library.
	// So if direct APIs are used, the following call is not necessary.
#ifndef DAO_DIRECT_API
	if( DaoInitLibrary( NULL ) ==0 ) return 1;
#endif

	// Initialize Dao library, and get the default DaoVmSpace object.
	// DaoVmSpace is responsible for handling interpreter settings,
	// paths and module loading etc. It is need to create several
	// other types of objects.
	vms = DaoInit();

	// Get the main namespace of an DaoVmSpace object.
	// You can also call DaoNameSpace_New( vms ) to create one.
	ns  = DaoVmSpace_MainNameSpace( vms );
	ns2  = DaoVmSpace_GetNameSpace( vms, "dao" );

	// Get the main virtual machine process of an DaoVmSpace object.
	// You can also call DaoVmProcess_New( vms ) to create one.
	vmp = DaoVmSpace_MainVmProcess( vms );

	// Prepare the Dao source codes:
	src = DString_New(1);
	DString_SetMBS( src, dao_source );

	// Execute the Dao scripts:
	// Since the wrapped functions and types are imported into
	// namespace ns, it is need to access the wrapped functions and types
	// in the Dao scripts when it is executed:
	DaoVmProcess_Eval( vmp, ns, src, 1 );

	DaoJIT_Init( vms );
	dao_jit.Free = DaoJIT_Free;
	dao_jit.Compile = DaoJIT_Compile;
	dao_jit.Execute = DaoJIT_Execute;
	DaoRoutine_PrintCode( ns->mainRoutine, vms->stdStream );
	//DaoJIT_Compile( ns->mainRoutine );
	//DaoRoutine_PrintCode( ns->mainRoutine, vms->stdStream );

	DaoContext *ctx = DaoContext_New();
	DaoContext_Init( ctx, ns->mainRoutine );
	ctx->regValues[1]->v.f = 123.56;
	simple_tests( ctx );

	printf( "value.f = %g\n", ctx->regValues[1]->v.f );

	std::vector<IndexRange> segments;
	DaoJIT_SearchCompilable( ns->mainRoutine, segments );

	// Check if the Dao scripts have indeed modified the C++ object.

	DString_Delete( src );
	DaoQuit(); // Finalizing
	return 0;
}
