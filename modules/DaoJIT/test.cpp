
#include<stdio.h>
#include"daoJIT.cpp"
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

	CallInst *daoctx = CallInst::Create(dao_context_get_local_values, func->arg_begin(), "", block);
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
"a = 8.0D\n"
"a = 12.0D * a\n"
"io.writeln( std.about(a), a )\n"
"return\n"
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
	//simple_tests( ctx );

	printf( "%p value.f = %g\n", ctx, ctx->regValues[1]->v.f );

	std::vector<IndexRange> segments;
	DaoJIT_SearchCompilable( ns->mainRoutine, segments );

	DaoJIT_Compile( ns->mainRoutine );
	DaoVmProcess_PushRoutine( vmp, ns->mainRoutine );
	ctx = vmp->topFrame->context;
	DaoVmProcess_Execute( vmp );
	printf( "%p value.v.f = %g\n", ctx, (float)ctx->regValues[0]->v.f );
	printf( "%p value.v.f = %g\n", ctx, (float)ctx->regValues[3]->v.f );
	printf( "%p %p %p\n", ns->mainRoutine->routConsts->data, ctx->regValues[1], ctx->regArray->data+1 );

	DString_Delete( src );
	DaoQuit(); // Finalizing
	return 0;
}
