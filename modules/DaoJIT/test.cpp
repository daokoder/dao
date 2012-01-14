
#include<stdio.h>
#include"daoJIT.h"
#include"daoNamespace.h"

#include "llvm/PassManager.h"
#include "llvm/CallingConv.h"
#include "llvm/Assembly/PrintModulePass.h"

#if 0
void simple_tests( DaoContext *ctx )
{
	Value *cst, *cast, *value, *fvalue, *field, *pfield, *pfloat;
	Function *func = DaoJIT_NewFunction( ctx->routine, 1000 );
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

	ReturnInst::Create( *llvm_context, block );

	verifyModule(*llvm_module, PrintMessageAction);
	PassManager PM;
	PM.add(createPrintModulePass(&outs()));
	PM.run(*llvm_module);
	std::vector<GenericValue> Args(2);
	Args[0].PointerVal = ctx;
	Args[1].PointerVal = ctx->routine;
	GenericValue GV = llvm_exe_engine->runFunction( func, Args);
	outs() << "Result: " << GV.IntVal << "\n";
}
#endif


const char* dao_source = 
"tup = ( name=>'abc', index => 123.5 )\n"
"class Test{ var a = 1.0D; } o = Test(); o.a=3.0D + rand(100.0D); a = b = o.a;\n"
"ls = { o }; c = ls[0]\n"
"s1 = 'abc'; s2 = '123'; s3 = s1 + s2; cc = s1[3]\n"
"t1 = tup.name; t2 = tup.index; tup.index += 1000; tup.name='new'\n"
"io.writeln( c, sqrt(10.0), s3, t1, t2, tup, cc )\n"
//"a=2; switch(a){case 1: a=2; case 2: a = 3}\n"
//"a:enum<AA,BB>=$AA; b = 0; switch(a){ case $AA: b=1; case $BB: c=1; default: c = 2 }\n"
#if 0
"c = 1.0\n"
"d = c < 1.0\n"
"a = sin( c )\n"
"a = cos( c )\n"
"b = rand( 100 )\n"
"b = rand( 100 )\n"
#endif
"io.writeln( std.about(a), a, b )\n"
/*
"a = 8.0\n"
"a = 12.0D * a\n"
"ii = 0\n"
"if( ii ) a = 100 else a = 200\n"
"for(i=1:5) a *= 2D\n"
"io.writeln( std.about(a), a )\n"
//"return\n"
"b = 22.3\n"
"c = 1.0 + b\n"
"if( c ){\n"
"d = c + 1.0\n"
"}\n"
"for( i = 1 : 5){ c = i; break; e = i + 1; }\n"
"io.writeln( c )\n"
*/
;

const char* dao_source3 = 
"function getSomeString(){ return 'StringFromDao' }";

//#include "llvm/LLVMContext.h"
//#include "llvm/Module.h"
//#include "llvm/Constants.h"
//#include "llvm/DerivedTypes.h"
//#include "llvm/Instructions.h"
//#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/TargetSelect.h"
//#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/raw_ostream.h"
//#include "llvm/Support/IRBuilder.h"

int main( int argc, char *argv[] )
{
	DString *src;
	DaoVmSpace *vms;
	DaoNamespace *ns;
	DaoNamespace *ns2;
	DaoProcess *vmp;

#if 0
  InitializeNativeTarget();

  LLVMContext Context;
  
  // Create some module to put our function into it.
  Module *M = new Module("test", Context);

  // Create the add1 function entry and insert this entry into module M.  The
  // function will have a return type of "int" and take an argument of "int".
  // The '0' terminates the list of argument types.
  Function *Add1F =
    cast<Function>(M->getOrInsertFunction("add1", Type::getInt32Ty(Context),
                                          Type::getInt32Ty(Context),
                                          (Type *)0));

  // Add a basic block to the function. As before, it automatically inserts
  // because of the last argument.
  BasicBlock *BB = BasicBlock::Create(Context, "EntryBlock", Add1F);

  // Create a basic block builder with default parameters.  The builder will
  // automatically append instructions to the basic block `BB'.
  IRBuilder<> builder(BB);

  // Get pointers to the constant `1'.
  Value *One = builder.getInt32(1);

  // Get pointers to the integer argument of the add1 function...
  assert(Add1F->arg_begin() != Add1F->arg_end()); // Make sure there's an arg
  Argument *ArgX = Add1F->arg_begin();  // Get the arg
  ArgX->setName("AnArg");            // Give it a nice symbolic name for fun.

  // Create the add instruction, inserting it into the end of BB.
  Value *Add = builder.CreateAdd(One, ArgX);

  // Create the return instruction and add it to the basic block
  builder.CreateRet(Add);

  // Now, function add1 is ready.


  // Now we're going to create function `foo', which returns an int and takes no
  // arguments.
  Function *FooF =
    cast<Function>(M->getOrInsertFunction("foo", Type::getInt32Ty(Context),
                                          (Type *)0));

  // Add a basic block to the FooF function.
  BB = BasicBlock::Create(Context, "EntryBlock", FooF);

  // Tell the basic block builder to attach itself to the new basic block
  builder.SetInsertPoint(BB);

  // Get pointer to the constant `10'.
  Value *Ten = builder.getInt32(10);

  // Pass Ten to the call to Add1F
  CallInst *Add1CallRes = builder.CreateCall(Add1F, Ten);
  Add1CallRes->setTailCall(true);

  // Create the return instruction and add it to the basic block.
  builder.CreateRet(Add1CallRes);

  // Now we create the JIT.
  ExecutionEngine* EE = EngineBuilder(M).create();

  outs() << "We just constructed this LLVM module:\n\n" << *M;
  outs() << "\n\nRunning foo: ";
  outs().flush();

  // Call the `foo' function with no arguments:
  std::vector<GenericValue> noargs;
  GenericValue gv = EE->runFunction(FooF, noargs);

  // Import result of execution:
  outs() << "Result: " << gv.IntVal << "\n";
#endif

	DaoJIT_Init( NULL, NULL );

	// Initialize Dao library, and get the default DaoVmSpace object.
	// DaoVmSpace is responsible for handling interpreter settings,
	// paths and module loading etc. It is need to create several
	// other types of objects.
	vms = DaoInit( argv[0] );
	DaoJIT_Init( vms, & dao_jit );

	// Get the main namespace of an DaoVmSpace object.
	// You can also call DaoNamespace_New( vms ) to create one.
	ns  = DaoVmSpace_MainNamespace( vms );
	ns2  = DaoVmSpace_GetNamespace( vms, "dao" );

	// Get the main virtual machine process of an DaoVmSpace object.
	// You can also call DaoProcess_New( vms ) to create one.
	vmp = DaoVmSpace_MainProcess( vms );

	// Prepare the Dao source codes:
	src = DString_New(1);
	DString_SetMBS( src, dao_source );

	// Execute the Dao scripts:
	// Since the wrapped functions and types are imported into
	// namespace ns, it is need to access the wrapped functions and types
	// in the Dao scripts when it is executed:
	DaoProcess_Eval( vmp, ns, src, 1 );

	DaoJIT_Init( vms, & dao_jit );
	//dao_jit.Free = DaoJIT_Free;
	//dao_jit.Compile = DaoJIT_Compile;
	//dao_jit.Execute = DaoJIT_Execute;
	DaoRoutine_PrintCode( ns->mainRoutine, vms->stdioStream );
	//DaoJIT_Compile( ns->mainRoutine );
	//DaoRoutine_PrintCode( ns->mainRoutine, vms->stdStream );

#if 0
	DaoContext *ctx = DaoContext_New();
	DaoContext_Init( ctx, ns->mainRoutine );
	ctx->regValues[1]->v.f = 123.56;
	//simple_tests( ctx );

	printf( "%p value.f = %g\n", ctx, ctx->regValues[1]->v.f );

	//std::vector<IndexRange> segments;
	//DaoJIT_SearchCompilable( ns->mainRoutine, segments );

	DaoJIT_Compile( ns->mainRoutine );
	DaoProcess_PushRoutine( vmp, ns->mainRoutine );
	ctx = vmp->topFrame->context;
	DaoProcess_Execute( vmp );
	printf( "%p value.v.f = %g\n", ctx, (float)ctx->regValues[0]->v.f );
	printf( "%p value.v.f = %g\n", ctx, (float)ctx->regValues[3]->v.f );
	printf( "%p %p %p\n", ns->mainRoutine->routConsts->data, ctx->regValues[1], ctx->regArray->data+1 );
#endif

	DString_Delete( src );
	DaoQuit(); // Finalizing
	return 0;
}
