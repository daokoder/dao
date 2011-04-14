
#include<assert.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<stack>

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
#include "llvm/PassManager.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Support/IRBuilder.h"

#include "daoJIT.h"

LLVMContext     *llvm_context = NULL;
Module          *llvm_module = NULL;
ExecutionEngine *llvm_exe_engine = NULL;

const Type *int8_type = NULL;
const Type *int16_type = NULL;
const Type *int32_type = NULL;
const Type *int64_type = NULL;
const Type *dint_type = NULL; // 32 or 64 bits
const Type *float_type = NULL;
const Type *double_type = NULL;
const Type *size_t_type = NULL;
const VectorType *int8_vector2 = NULL;

const Type *void_type = NULL; // i8*

const StructType *dao_value_type = NULL; // DValue
const StructType *dao_varray_type = NULL; // DVarray
const StructType *dao_enum_type = NULL; // DEnum

const StructType *dao_base_type = NULL; // DaoBase
const StructType *dao_routine_type = NULL; // DaoRoutine
const StructType *dao_context_type = NULL; // DaoContext

const PointerType *void_type_p = NULL; // DValue*

const PointerType *dao_value_type_p = NULL; // DValue*
const PointerType *dao_value_type_pp = NULL; // DValue**
const PointerType *dao_varray_type_p = NULL; // DVarray*
const PointerType *dao_enum_type_p = NULL; // DEnum*
const PointerType *dao_enum_type_pp = NULL; // DEnum**

const PointerType *dao_base_type_p = NULL; // DaoBase*
const PointerType *dao_routine_type_p = NULL; // DaoRoutine*
const PointerType *dao_context_type_p = NULL; // DaoContext*

const ArrayType *dao_value_array_type = NULL; // DValue[]
const ArrayType *dao_value_ptr_array_type = NULL; // DValue*[]
const PointerType *dao_value_array_type_p = NULL; // DValue[]*
const PointerType *dao_value_ptr_array_type_p = NULL; // DValue*[]*

int dao_opcode_compilable[ DVM_NULL ];

Function *dao_pow_double = NULL;
Function *dao_abs_double = NULL;
Function *dao_acos_double = NULL;
Function *dao_asin_double = NULL;
Function *dao_atan_double = NULL;
Function *dao_ceil_double = NULL;
Function *dao_cos_double = NULL;
Function *dao_cosh_double = NULL;
Function *dao_exp_double = NULL;
Function *dao_floor_double = NULL;
Function *dao_log_double = NULL;
Function *dao_rand_double = NULL;
Function *dao_sin_double = NULL;
Function *dao_sinh_double = NULL;
Function *dao_sqrt_double = NULL;
Function *dao_tan_double = NULL;
Function *dao_tanh_double = NULL;

FunctionType *dao_jit_function_type = NULL;

double dao_rand( double max ){ return max * rand() / (RAND_MAX+1.0); }

void DaoJIT_Init( DaoVmSpace *vms )
{
	int i;
	memset( dao_opcode_compilable, 0, DVM_NULL*sizeof(int) );
	for(i=DVM_MOVE_II; i<=DVM_MOVE_DD; i++) dao_opcode_compilable[i] = 1;
	for(i=DVM_ADD_III; i<=DVM_BITRIT_DNN; i++) dao_opcode_compilable[i] = 1;
	for(i=DVM_GETF_KCI; i<=DVM_SETF_OVDD; i++) dao_opcode_compilable[i] = 1;
	dao_opcode_compilable[ DVM_NOP ] = 1;
	dao_opcode_compilable[ DVM_GOTO ] = 1;
	dao_opcode_compilable[ DVM_SWITCH ] = 1;
	dao_opcode_compilable[ DVM_CASE ] = 1;
	dao_opcode_compilable[ DVM_CRRE ] = 1;
	dao_opcode_compilable[ DVM_TEST ] = 1;
	dao_opcode_compilable[ DVM_TEST_I ] = 1;
	dao_opcode_compilable[ DVM_TEST_F ] = 1;
	dao_opcode_compilable[ DVM_TEST_D ] = 1;
	// TODO: GETCX, GETVX, swith, complex, string, array, tuple, list etc.

	printf( "DaoJIT_Init()\n" );
	InitializeNativeTarget();
	llvm_context = new LLVMContext();
	llvm_module = new Module("DaoJIT", *llvm_context);

	int8_type = Type::getInt8Ty( *llvm_context );
	int16_type = Type::getInt16Ty( *llvm_context );
	int32_type = Type::getInt32Ty( *llvm_context );
	int64_type = Type::getInt64Ty( *llvm_context );
	int8_vector2 = VectorType::get( int8_type, 2 );

	dint_type = int32_type;
	if( sizeof(void*) == 8 ) dint_type = Type::getInt64Ty( *llvm_context );
	float_type = Type::getFloatTy( *llvm_context );
	double_type = Type::getDoubleTy( *llvm_context );

	size_t_type = dint_type;

	void_type = Type::getVoidTy( *llvm_context );
	void_type_p = PointerType::getUnqual( int8_type );

	std::vector<const Type*> dvalue_types( 4, int8_type );
	dvalue_types.push_back( double_type );

	// type { i8, i8, i8, i8, double }
	dao_value_type = StructType::get( *llvm_context, dvalue_types );
	dao_value_type_p = PointerType::getUnqual( dao_value_type );
	dao_value_type_pp = PointerType::getUnqual( dao_value_type_p );

	dao_value_array_type = ArrayType::get( dao_value_type, 0 );
	dao_value_array_type_p = PointerType::getUnqual( dao_value_array_type );
	dao_value_ptr_array_type = ArrayType::get( dao_value_type_p, 0 );
	dao_value_ptr_array_type_p = PointerType::getUnqual( dao_value_ptr_array_type );

	std::vector<const Type*> varray_types( 2, dao_value_array_type_p ); // data, buf
	varray_types.push_back( size_t_type ); // size
	varray_types.push_back( size_t_type ); // bufsize
	dao_varray_type = StructType::get( *llvm_context, varray_types );
	dao_varray_type_p = PointerType::getUnqual( dao_varray_type );

	// DAO_DATA_COMMON:
	std::vector<const Type*> base_types( 2, int8_type ); // type, trait
	base_types.push_back( int8_vector2 ); // gcState[2]
	base_types.push_back( int32_type ); // refCount
	base_types.push_back( int32_type ); // cycRefCount

	// type { i8, i8, <2 x i8>, i32, i32 }
	dao_base_type = StructType::get( *llvm_context, base_types );
	dao_base_type_p = PointerType::getUnqual( dao_base_type );

	std::vector<const Type*> enum_types( 1, dao_base_type_p );
	enum_types.push_back( dint_type );
	dao_enum_type = StructType::get( *llvm_context, enum_types );
	dao_enum_type_p = PointerType::getUnqual( dao_enum_type );
	dao_enum_type_pp = PointerType::getUnqual( dao_enum_type_p );

	base_types.push_back( void_type_p ); // codes
	base_types.push_back( void_type_p ); // vmc
	base_types.push_back( void_type_p ); // frame
	base_types.push_back( void_type_p ); // regArray
	base_types.push_back( dao_value_ptr_array_type_p ); // regValues

	// type { i8, i8, <2 x i8>, i32, i32, i8*, i8*, i8*, i8*, [0 x %1*]* }
	dao_context_type = StructType::get( *llvm_context, base_types );
	dao_context_type_p = PointerType::getUnqual( dao_context_type );
	//dao_context_type->setName( "DaoContext" );

	for(i=0; i<5; i++) base_types.pop_back();
	base_types.push_back( int8_type ); // attribs
	base_types.push_back( int8_type ); // minimal
	base_types.push_back( int8_type ); // minParam
	base_types.push_back( int8_type ); // parCount
	base_types.push_back( int16_type ); // defLine
	base_types.push_back( int8_type ); // tidHost
	base_types.push_back( int8_type ); // 
	base_types.push_back( void_type_p ); // routHost
	base_types.push_back( void_type_p ); // routType
	base_types.push_back( void_type_p ); // routName
	base_types.push_back( void_type_p ); // routHelp
	base_types.push_back( void_type_p ); // parCodes
	base_types.push_back( dao_varray_type_p ); // routConsts
	dao_routine_type = StructType::get( *llvm_context, base_types );
	dao_routine_type_p = PointerType::getUnqual( dao_routine_type );

	// void (context*,routine*)
	std::vector<const Type*> jitParams( 1, dao_context_type_p );
	jitParams.push_back( dao_routine_type_p );
	dao_jit_function_type = FunctionType::get( void_type, jitParams, false );


	std::vector<const Type*> double2( 2, double_type );
	FunctionType *funtype = FunctionType::get( double_type, double2, false );
	dao_pow_double = Function::Create( funtype, Function::ExternalLinkage, "pow", llvm_module );

	std::vector<const Type*> double1( 1, double_type );
	FunctionType *mathft = FunctionType::get( double_type, double1, false );
	dao_abs_double = Function::Create( mathft, Function::ExternalLinkage, "abs", llvm_module );
	dao_acos_double = Function::Create( mathft, Function::ExternalLinkage, "acos", llvm_module );
	dao_asin_double = Function::Create( mathft, Function::ExternalLinkage, "asin", llvm_module );
	dao_atan_double = Function::Create( mathft, Function::ExternalLinkage, "atan", llvm_module );
	dao_ceil_double = Function::Create( mathft, Function::ExternalLinkage, "ceil", llvm_module );
	dao_cos_double = Function::Create( mathft, Function::ExternalLinkage, "cos", llvm_module );
	dao_cosh_double = Function::Create( mathft, Function::ExternalLinkage, "cosh", llvm_module );
	dao_exp_double = Function::Create( mathft, Function::ExternalLinkage, "exp", llvm_module );
	dao_floor_double = Function::Create( mathft, Function::ExternalLinkage, "floor", llvm_module );
	dao_log_double = Function::Create( mathft, Function::ExternalLinkage, "log", llvm_module );
	dao_rand_double = Function::Create( mathft, Function::ExternalLinkage, "rand", llvm_module );
	dao_sin_double = Function::Create( mathft, Function::ExternalLinkage, "sin", llvm_module );
	dao_sinh_double = Function::Create( mathft, Function::ExternalLinkage, "sinh", llvm_module );
	dao_sqrt_double = Function::Create( mathft, Function::ExternalLinkage, "sqrt", llvm_module );
	dao_tan_double = Function::Create( mathft, Function::ExternalLinkage, "tan", llvm_module );
	dao_tanh_double = Function::Create( mathft, Function::ExternalLinkage, "tanh", llvm_module );

	llvm_exe_engine = EngineBuilder( llvm_module ).setEngineKind(EngineKind::JIT).create();
	llvm_exe_engine->addGlobalMapping( dao_pow_double, (void*) pow );
	llvm_exe_engine->addGlobalMapping( dao_abs_double, (void*) abs );
	llvm_exe_engine->addGlobalMapping( dao_acos_double, (void*) acos );
	llvm_exe_engine->addGlobalMapping( dao_asin_double, (void*) asin );
	llvm_exe_engine->addGlobalMapping( dao_atan_double, (void*) atan );
	llvm_exe_engine->addGlobalMapping( dao_ceil_double, (void*) ceil );
	llvm_exe_engine->addGlobalMapping( dao_cos_double, (void*) cos );
	llvm_exe_engine->addGlobalMapping( dao_cosh_double, (void*) cosh );
	llvm_exe_engine->addGlobalMapping( dao_exp_double, (void*) exp );
	llvm_exe_engine->addGlobalMapping( dao_floor_double, (void*) floor );
	llvm_exe_engine->addGlobalMapping( dao_log_double, (void*) log );
	llvm_exe_engine->addGlobalMapping( dao_rand_double, (void*) dao_rand );
	llvm_exe_engine->addGlobalMapping( dao_sin_double, (void*) sin );
	llvm_exe_engine->addGlobalMapping( dao_sinh_double, (void*) sinh );
	llvm_exe_engine->addGlobalMapping( dao_sqrt_double, (void*) sqrt );
	llvm_exe_engine->addGlobalMapping( dao_tan_double, (void*) tan );
}


// Create a function with signature: void (DaoContext*,DaoRoutine*)
Function* DaoJitHandle::NewFunction( DaoRoutine *routine, int id )
{
	int i;
	char buf[100];
	std::string name = routine->routName->mbs;
	sprintf( buf, "_%p_%i", routine, id );
	name += buf;

	jitFunction = cast<Function>( llvm_module->getOrInsertFunction( name, dao_jit_function_type ) );
	entryBlock = BasicBlock::Create( *llvm_context, "EntryBlock", jitFunction );
	lastBlock = activeBlock = entryBlock;
	SetInsertPoint( entryBlock );

	Argument *ctx = jitFunction->arg_begin();
	Argument *rout = (++jitFunction->arg_begin());
	ctx->setName("context");
	rout->setName("routine");

	Value *value = CreateConstGEP2_32( ctx, 0, 9 ); // context->regValues: DValue***
	localValues = CreateLoad( value ); // context->regValues: DValue**

	value = CreateConstGEP2_32( rout, 0, 17 ); // routine->routConsts: DVarray**
	value = CreateLoad( value ); //routine->routConsts: DVarray*
	value = CreateConstGEP2_32( value, 0, 0 ); // routine->routConsts->data: DValue[]**
	localConsts = CreateLoad( value ); // routine->routConsts->data: DValue[]*

	localRefers.resize( routine->locRegCount );
	for(i=0; i<routine->locRegCount; i++) localRefers[i] = NULL;

#ifdef DEBUG
	localValues->setName( "context.regValues" );
	localConsts->setName( "routie.routConsts" );
#endif
	return jitFunction;
}
BasicBlock* DaoJitHandle::NewBlock( DaoVmCodeX *vmc )
{
	const char *name = getOpcodeName( vmc->code );
	activeBlock = BasicBlock::Create( *llvm_context, name, jitFunction );
	SetInsertPoint( activeBlock );
	lastBlock = activeBlock;
	return activeBlock;
}
void DaoJitHandle::SetActiveBlock( BasicBlock *block )
{
	activeBlock = block;
	SetInsertPoint( activeBlock );
}


struct IndexRange
{
	int start;
	int end;
	IndexRange( int s=0, int e=0 ){ start = s; end = e; }

	bool operator<( const IndexRange & other )const{
		return end < other.start;
	}
};


void DaoJIT_Free( DaoRoutine *routine )
{
}
/*
A compilable block is a block of virtual instructions that only branch within the block, 
or just branch to the instruction right after this block.
*/
void DaoJIT_SearchCompilable( DaoRoutine *routine, std::vector<IndexRange> & segments )
{
	std::map<IndexRange,int> ranges;
	std::map<IndexRange,int>::iterator it;
	DValue *routConsts = routine->routConsts->data;
	DaoType **types = routine->regType->items.pType;
	DaoVmCodeX *vmc, **vmcs = routine->annotCodes->items.pVmc;
	int i, j, m, jump, N = routine->annotCodes->size;
	bool compilable, last = false;
	size_t k;
	int case_mode = DAO_CASE_UNORDERED;
	for(i=0; i<N; i++){ // find the maximum blocks
		vmc = vmcs[i];
		compilable = dao_opcode_compilable[ vmc->code ];
		if( vmc->code != DVM_CASE ) case_mode = DAO_CASE_UNORDERED;
		// all branching instructions are assumed to be jit compilable for now,
		// so that they can be checked in the next stage:
		switch( vmc->code ){
		case DVM_MATH :
			j = types[vmc->b]->tid;
			m = types[vmc->c]->tid;
			compilable = j and j <= DAO_DOUBLE and m and m <= DAO_DOUBLE;
			break;
		case DVM_DATA : compilable = vmc->a <= DAO_DOUBLE; break;
		case DVM_GETCL : compilable = vmc->a == 0; break;
		default : break;
		}
		if( compilable ){
			if( last ){
				segments.back().end = i;
			}else{
				segments.push_back( IndexRange( i, i ) );
			}
		}
		last = compilable;
	}
	for(k=0; k<segments.size(); k++) ranges[segments[k]] = 1;
	for(k=0; k<segments.size(); k++){
		int code, start = segments[k].start;
		int end = segments[k].end;
		bool modified = false;
		for(j=start; j<=end; j++){
			vmc = vmcs[j];
			code = vmc->code;
			jump = vmc->b;
			if( code != DVM_CASE ) case_mode = DAO_CASE_UNORDERED;
			//DaoVmCodeX_Print( *vmc, NULL );
			switch( code ){
			case DVM_GOTO : case DVM_TEST : case DVM_CRRE : 
			case DVM_SWITCH : case DVM_CASE :
			case DVM_TEST_I : case DVM_TEST_F : case DVM_TEST_D :
				compilable = false;
				switch( code ){
				case DVM_GOTO : case DVM_TEST_I : case DVM_TEST_F : case DVM_TEST_D :
					// branchs out of the block
					compilable = vmc->b >= start and vmc->b <= (end+1);
					break;
				case DVM_SWITCH : 
					m = types[vmc->a]->tid;
					if( m == DAO_INTEGER or m == DAO_ENUM ) case_mode = vmcs[j+1]->c; // first case
					compilable = case_mode >= DAO_CASE_INTS;
					if( vmc->b < start or vmc->b > (end+1) ) compilable = false;
					break;
				case DVM_CASE : 
					compilable = case_mode >= DAO_CASE_INTS;
					m = routConsts[ vmc->a ].t;
					if( m != DAO_INTEGER and m != DAO_ENUM ) compilable = false;
					if( vmc->b < start or vmc->b > (end+1) ) compilable = false;
					break;
				case DVM_CRRE :
					jump = vmc->c ? vmc->c : -1;
					break;
				}
				if( compilable == false ){
					// break the block:
					segments.push_back( IndexRange( start, j-1 ) );
					segments.push_back( IndexRange( j+1, end ) );
					// check branching into another block:
					it = ranges.find( IndexRange( j, j ) );
					if( it != ranges.end() ) ranges.erase( it );
					ranges[ IndexRange( start, j-1 ) ] = 1;
					ranges[ IndexRange( j+1, end ) ] = 1;
					// check branching inside block:
					if( jump >= start and jump <= end ){
						it = ranges.find( IndexRange( jump, jump ) );
						if( it != ranges.end() ) ranges.erase( it );
						if( jump < j ){
							segments.push_back( IndexRange( start, jump-1 ) );
							segments.push_back( IndexRange( jump+1, j-1 ) );
							ranges[ IndexRange( start, jump-1 ) ] = 1;
							ranges[ IndexRange( jump+1, j-1 ) ] = 1;
						}else if( jump > j ){
							segments.push_back( IndexRange( j+1, jump-1 ) );
							segments.push_back( IndexRange( jump+1, end ) );
							ranges[ IndexRange( j+1, jump-1 ) ] = 1;
							ranges[ IndexRange( jump+1, end ) ] = 1;
						}
					}
					modified = true;
				}
				break;
			}
			if( modified ) break;
		}
	}
	segments.clear();
	for(it=ranges.begin(); it!=ranges.end(); it++){
		if( it->first.start < it->first.end ) segments.push_back( it->first );
	}
	for(k=0;k<segments.size();k++) printf( "%3li:%5i%5i\n", k, segments[k].start, segments[k].end );
}


Value* DaoJitHandle::GetLocalConstant( int id )
{
	SetInsertPoint( activeBlock );
	return CreateConstGEP2_32( localConsts, 0, id );
}
Value* DaoJitHandle::GetLocalReference( int reg )
{
	Value *refer;
	if( localRefers[reg] ) return localRefers[reg];
	SetInsertPoint( entryBlock );
	refer = CreateConstGEP2_32( localValues, 0, reg );
	refer->setName( "local_ref" );
	localRefers[reg] = refer;
	SetInsertPoint( activeBlock );
	return refer;
}
Value* DaoJitHandle::GetLocalValue( int reg )
{
	Value *value = GetLocalReference( reg );
	SetInsertPoint( activeBlock );
	return CreateLoad( value );
}
Value* DaoJitHandle::GetLocalValueDataPointer( int reg )
{
	Value *value = GetLocalValue( reg );
	SetInsertPoint( activeBlock );
	return GetValueDataPointer( value );
}
Value* DaoJitHandle::GetValueTypePointer( Value *value )
{
	SetInsertPoint( activeBlock );
	return CreateConstGEP2_32( value, 0, 0 );
}
Value* DaoJitHandle::GetValueDataPointer( Value *value )
{
	SetInsertPoint( activeBlock );
	return CreateConstGEP2_32( value, 0, 4 );
}
Value* DaoJitHandle::Dereference( Value *value )
{
	SetInsertPoint( activeBlock );
	return CreateLoad( value );
}
Value* DaoJitHandle::CastIntegerPointer( Value *value )
{
	const PointerType *type = PointerType::getUnqual( dint_type );
	SetInsertPoint( activeBlock );
	return CreatePointerCast( value, type );
}
Value* DaoJitHandle::CastFloatPointer( Value *value )
{
	const PointerType *type = PointerType::getUnqual( float_type );
	SetInsertPoint( activeBlock );
	return CreatePointerCast( value, type );
}
Value* DaoJitHandle::CastDoublePointer( Value *value )
{
	const PointerType *type = PointerType::getUnqual( double_type );
	SetInsertPoint( activeBlock );
	return CreatePointerCast( value, type );
}
void DaoJitHandle::GetIntegerOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC )
{
	Value *A = GetLocalValueDataPointer( vmc->a );
	Value *B = GetLocalValueDataPointer( vmc->b );
	Value *C = GetLocalValueDataPointer( vmc->c );
	SetInsertPoint( activeBlock );
	A = CastIntegerPointer( A );
	B = CastIntegerPointer( B );
	C = CastIntegerPointer( C );
	A = Dereference( A );
	B = Dereference( B );
	*dA = A;
	*dB = B;
	*dC = C;
}
void DaoJitHandle::GetFloatOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC )
{
	Value *A = GetLocalValueDataPointer( vmc->a );
	Value *B = GetLocalValueDataPointer( vmc->b );
	Value *C = GetLocalValueDataPointer( vmc->c );
	SetInsertPoint( activeBlock );
	A = CastFloatPointer( A );
	B = CastFloatPointer( B );
	C = CastFloatPointer( C );
	A = Dereference( A );
	B = Dereference( B );
	*dA = A;
	*dB = B;
	*dC = C;
}
void DaoJitHandle::GetDoubleOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC )
{
	Value *A = GetLocalValueDataPointer( vmc->a );
	Value *B = GetLocalValueDataPointer( vmc->b );
	Value *C = GetLocalValueDataPointer( vmc->c );
	SetInsertPoint( activeBlock );
	//A = CastDoublePointer( A );
	//B = CastDoublePointer( B );
	//C = CastDoublePointer( C );
	A = Dereference( A );
	B = Dereference( B );
	*dA = A;
	*dB = B;
	*dC = C;
}
void DaoJitHandle::GetFNNOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC )
{
	DaoType **types = routine->regType->items.pType;
	Value *A = GetLocalValueDataPointer( vmc->a );
	Value *B = GetLocalValueDataPointer( vmc->b );
	Value *C = GetLocalValueDataPointer( vmc->c );
	SetInsertPoint( activeBlock );
	C = CastFloatPointer( C );
	switch( types[ vmc->a ]->tid ){
	case DAO_INTEGER : A = CastIntegerPointer( A ); break;
	case DAO_FLOAT :   A = CastFloatPointer( A ); break;
	}
	switch( types[ vmc->b ]->tid ){
	case DAO_INTEGER : B = CastIntegerPointer( B ); break;
	case DAO_FLOAT :   B = CastFloatPointer( B ); break;
	}
	A = Dereference( A );
	B = Dereference( B );
	*dA = A;
	*dB = B;
	*dC = C;
}
void DaoJitHandle::GetDNNOperands( DaoVmCodeX *vmc, Value **dA, Value **dB, Value **dC )
{
	DaoType **types = routine->regType->items.pType;
	Value *A = GetLocalValueDataPointer( vmc->a );
	Value *B = GetLocalValueDataPointer( vmc->b );
	Value *C = GetLocalValueDataPointer( vmc->c );
	SetInsertPoint( activeBlock );
	switch( types[ vmc->a ]->tid ){
	case DAO_INTEGER : A = CastIntegerPointer( A ); break;
	case DAO_FLOAT :   A = CastFloatPointer( A ); break;
	}
	switch( types[ vmc->b ]->tid ){
	case DAO_INTEGER : B = CastIntegerPointer( B ); break;
	case DAO_FLOAT :   B = CastFloatPointer( B ); break;
	}
	A = Dereference( A );
	B = Dereference( B );
	*dA = A;
	*dB = B;
	*dC = C;
}
Function* DaoJitHandle::Compile( DaoRoutine *routine, int start, int end )
{
	DValue *routConsts = routine->routConsts->data;
	DaoType **types = routine->regType->items.pType;
	DaoVmCodeX *vmc, **vmcs = routine->annotCodes->items.pVmc;
	Function *jitfunc = NewFunction( routine, start );
	Function *mathfunc = NULL;

	Constant *zero32 = ConstantInt::get( int32_type, 0 );
	Constant *zero8 = ConstantInt::get( int8_type, 0 );
	Constant *zero = ConstantInt::get( dint_type, 0 );
	Constant *tid8_integer = ConstantInt::get( int8_type, DAO_INTEGER );
	Constant *tid8_float = ConstantInt::get( int8_type, DAO_FLOAT );
	Constant *tid8_double = ConstantInt::get( int8_type, DAO_DOUBLE );
	Constant *type_ids[DAO_DOUBLE+1] = { zero8, tid8_integer, tid8_float, tid8_double };
	Value *type, *vdata, *dA, *dB, *dC, *value=NULL, *tmp;
	ConstantInt *caseint;
	SwitchInst *inswitch;
	int code, i, k, m;

	std::map<int,BasicBlock*> branchings;
	std::map<int,BasicBlock*> labels;
	std::map<int,BasicBlock*>::iterator iter, stop;
	for(i=start; i<=end; i++){
		vmc = vmcs[i];
		code = vmc->code;
		switch( code ){
		case DVM_GOTO : case DVM_TEST_I : case DVM_TEST_F : case DVM_TEST_D :
			branchings[i] = NULL;
			labels[i+1] = NULL;
			labels[vmc->b] = NULL;
			if( vmc->b ) branchings[vmc->b-1] = NULL;
			break;
		case DVM_SWITCH :
			branchings[i] = NULL;
			labels[i+1] = NULL;
			labels[vmc->b] = NULL;
			branchings[vmc->b-1] = NULL;
			// use DVM_CASE to add labels
			for(k=1; k<=vmc->c; k++) labels[vmcs[i+k]->b] = NULL;
			break;
		}
	}

	for(i=start; i<=end; i++){
		vmc = vmcs[i];
		code = vmc->code;
		printf( "%3i ", i ); DaoVmCodeX_Print( *vmc, NULL );
		if( labels.find( i ) != labels.end() ){
			printf( "%3i ", i ); DaoVmCodeX_Print( *vmc, NULL );
			labels[i] = NewBlock( vmc );
		}
		switch( code ){
		case DVM_NOP :
			break;
		case DVM_DATA :
			dC = GetLocalValue( vmc->c );
			type = GetValueTypePointer( dC );
			vdata = GetValueDataPointer( dC );
			if( sizeof(void*) == 8 ){
				value = getInt64( (int) vmc->b );
			}else{
				value = getInt32( (int) vmc->b );
			}
			tmp = CreateStore( type_ids[ vmc->a ], type );
			switch( vmc->a ){
			case DAO_NIL :
				break;
			case DAO_INTEGER :
				vdata = CastIntegerPointer( vdata );
				break;
			case DAO_FLOAT :
				value = CreateUIToFP( value, float_type );
				vdata = CastFloatPointer( vdata );
				break;
			case DAO_DOUBLE :
				value = CreateUIToFP( value, double_type );
				vdata = CastFloatPointer( vdata );
				break;
			default: goto Failed;
			}
			if( vmc->a ) tmp = CreateStore( value, vdata );
			break;
		case DVM_GETCL :
			if( vmc->a ) goto Failed;
			dC = GetLocalReference( vmc->c );
			dB = GetLocalConstant( vmc->b );
			tmp = CreateStore( dB, dC );
			break;
		case DVM_MATH :
			dB = GetLocalValueDataPointer( vmc->b );
			dC = GetLocalValueDataPointer( vmc->c );
			switch( types[ vmc->b ]->tid ){
			case DAO_INTEGER :
				dB = Dereference( CastIntegerPointer( dB ) );
				dB = CreateSIToFP( dB, double_type );
				break;
			case DAO_FLOAT :
				dB = Dereference( CastFloatPointer( dB ) );
				dB = CreateFPCast( dB, double_type );
				break;
			}
			switch( vmc->a ){
			case DVM_MATH_ABS  : mathfunc = dao_abs_double;  break;
			case DVM_MATH_ACOS : mathfunc = dao_acos_double; break;
			case DVM_MATH_ASIN : mathfunc = dao_asin_double; break;
			case DVM_MATH_ATAN : mathfunc = dao_atan_double; break;
			case DVM_MATH_CEIL : mathfunc = dao_ceil_double; break;
			case DVM_MATH_COS  : mathfunc = dao_cos_double;  break;
			case DVM_MATH_COSH : mathfunc = dao_cosh_double; break;
			case DVM_MATH_EXP  : mathfunc = dao_exp_double;  break;
			case DVM_MATH_FLOOR : mathfunc = dao_floor_double; break;
			case DVM_MATH_LOG  : mathfunc = dao_log_double;  break;
			case DVM_MATH_RAND : mathfunc = dao_rand_double; break;
			case DVM_MATH_SIN  : mathfunc = dao_sin_double;  break;
			case DVM_MATH_SINH : mathfunc = dao_sinh_double; break;
			case DVM_MATH_SQRT : mathfunc = dao_sqrt_double; break;
			case DVM_MATH_TAN  : mathfunc = dao_tan_double;  break;
			case DVM_MATH_TANH : mathfunc = dao_tanh_double; break;
			default : break;
			}
			dB = CreateCall( mathfunc, dB );
			switch( types[ vmc->c ]->tid ){
			case DAO_INTEGER :
				dB = CreateFPToSI( dB, dint_type );
				dC = CastIntegerPointer( dC );
				tmp = CreateStore( dB, dC );
				break;
			case DAO_FLOAT :
				dB = CreateFPCast( dB, double_type );
				dC = CastFloatPointer( dC );
				tmp = CreateStore( dB, dC );
				break;
			case DAO_DOUBLE :
				tmp = CreateStore( dB, dC );
				break;
			}
			break;
		case DVM_MOVE_II : case DVM_MOVE_IF : case DVM_MOVE_ID :
		case DVM_MOVE_FI : case DVM_MOVE_FF : case DVM_MOVE_FD :
		case DVM_MOVE_DI : case DVM_MOVE_DF : case DVM_MOVE_DD :
			dA = GetLocalValueDataPointer( vmc->a );
			dC = GetLocalValueDataPointer( vmc->c );
			switch( types[ vmc->a ]->tid ){
			case DAO_INTEGER : dA = CastIntegerPointer( dA ); break;
			case DAO_FLOAT   : dA = CastFloatPointer( dA ); break;
			case DAO_DOUBLE  : dA = CastDoublePointer( dA ); break;
			}
			switch( types[ vmc->c ]->tid ){
			case DAO_INTEGER : dC = CastIntegerPointer( dC ); break;
			case DAO_FLOAT   : dC = CastFloatPointer( dC ); break;
			case DAO_DOUBLE  : dC = CastDoublePointer( dC ); break;
			}
			dA = Dereference( dA );
			switch( code ){
			case DVM_MOVE_IF : 
			case DVM_MOVE_ID : dA = CreateFPToSI( dA, dint_type ); break;
			case DVM_MOVE_FI : dA = CreateSIToFP( dA, float_type ); break;
			case DVM_MOVE_DI : dA = CreateSIToFP( dA, double_type ); break;
			case DVM_MOVE_FD : dA = CreateFPCast( dA, float_type ); break;
			case DVM_MOVE_DF : dA = CreateFPCast( dA, double_type ); break;
			}
			tmp = CreateStore( dA, dC );
			break;
		case DVM_ADD_III :
		case DVM_SUB_III :
		case DVM_MUL_III :
		case DVM_DIV_III :
		case DVM_MOD_III :
			GetIntegerOperands( vmc, & dA, & dB, & dC );
			switch( code ){
			case DVM_ADD_III : value = CreateAdd( dA, dB ); break;
			case DVM_SUB_III : value = CreateSub( dA, dB ); break;
			case DVM_MUL_III : value = CreateMul( dA, dB ); break;
			case DVM_DIV_III : value = CreateSDiv( dA, dB ); break;
			case DVM_MOD_III : value = CreateSRem( dA, dB ); break;
			}
			tmp = CreateStore( value, dC );
			break;
		case DVM_POW_III :
			GetIntegerOperands( vmc, & dA, & dB, & dC );
			dA = CreateSIToFP( dA, double_type );
			dB = CreateSIToFP( dB, double_type );
			tmp = CreateCall2( dao_pow_double, dA, dB );
			tmp = CreateFPToSI( tmp, dint_type );
			tmp = CreateStore( tmp, dC );
			break;
		case DVM_AND_III :
		case DVM_OR_III :
			GetIntegerOperands( vmc, & dA, & dB, & dC );
			switch( code ){
			case DVM_AND_III : value = CreateICmpEQ( dA, zero ); break;
			case DVM_OR_III  : value = CreateICmpNE( dA, zero ); break;
			}
			value = CreateSelect( value, dA, dB );
			tmp = CreateStore( value, dC );
			break;
		case DVM_LT_III :
		case DVM_LE_III :
		case DVM_EQ_III :
		case DVM_NE_III :
			GetIntegerOperands( vmc, & dA, & dB, & dC );
			switch( code ){
			case DVM_LT_III : value = CreateICmpSLT( dA, dB ); break;
			case DVM_LE_III : value = CreateICmpSLE( dA, dB ); break;
			case DVM_EQ_III : value = CreateICmpEQ( dA, dB ); break;
			case DVM_NE_III : value = CreateICmpNE( dA, dB ); break;
			}
			value = CreateIntCast( value, dint_type, false );
			tmp = CreateStore( value, dC );
			break;
		case DVM_BITAND_III :
		case DVM_BITOR_III :
		case DVM_BITXOR_III :
		case DVM_BITLFT_III :
		case DVM_BITRIT_III :
			GetIntegerOperands( vmc, & dA, & dB, & dC );
			switch( code ){
			case DVM_BITAND_III : value = CreateAnd( dA, dB ); break;
			case DVM_BITOR_III : value = CreateOr( dA, dB ); break;
			case DVM_BITXOR_III : value = CreateXor( dA, dB ); break;
			case DVM_BITLFT_III : value = CreateShl( dA, dB ); break;
			case DVM_BITRIT_III : value = CreateLShr( dA, dB ); break;
			}
			tmp = CreateStore( value, dC );
			break;
		case DVM_ADD_FFF :
		case DVM_SUB_FFF :
		case DVM_MUL_FFF :
		case DVM_DIV_FFF :
		case DVM_MOD_FFF :
			GetFloatOperands( vmc, & dA, & dB, & dC );
			switch( code ){
			case DVM_ADD_FFF : value = CreateFAdd( dA, dB ); break;
			case DVM_SUB_FFF : value = CreateFSub( dA, dB ); break;
			case DVM_MUL_FFF : value = CreateFMul( dA, dB ); break;
			case DVM_DIV_FFF : value = CreateFDiv( dA, dB ); break;
			case DVM_MOD_FFF : value = CreateFRem( dA, dB ); break;
			// XXX: float mod in daovm.
			}
			tmp = CreateStore( value, dC );
			break;
		case DVM_POW_FFF :
			GetFloatOperands( vmc, & dA, & dB, & dC );
			dA = CreateFPCast( dA, double_type );
			dB = CreateFPCast( dB, double_type );
			tmp = CreateCall2( dao_pow_double, dA, dB );
			tmp = CreateFPCast( tmp, float_type );
			tmp = CreateStore( tmp, dC );
			break;
		case DVM_AND_FFF :
		case DVM_OR_FFF :
			GetFloatOperands( vmc, & dA, & dB, & dC );
			switch( code ){
			case DVM_AND_FFF : value = CreateFCmpOEQ( dA, zero ); break;
			case DVM_OR_FFF  : value = CreateFCmpONE( dA, zero ); break;
			}
			value = CreateSelect( value, dA, dB );
			tmp = CreateStore( value, dC );
			break;
		case DVM_LT_FFF :
		case DVM_LE_FFF :
		case DVM_EQ_FFF :
		case DVM_NE_FFF :
			GetFloatOperands( vmc, & dA, & dB, & dC );
			switch( code ){
			case DVM_LT_FFF : value = CreateFCmpOLT( dA, dB ); break;
			case DVM_LE_FFF : value = CreateFCmpOLE( dA, dB ); break;
			case DVM_EQ_FFF : value = CreateFCmpOEQ( dA, dB ); break;
			case DVM_NE_FFF : value = CreateFCmpONE( dA, dB ); break;
			}
			value = CreateUIToFP( value, float_type );
			tmp = CreateStore( value, dC );
			break;
		case DVM_BITAND_FFF :
		case DVM_BITOR_FFF :
		case DVM_BITXOR_FFF :
		case DVM_BITLFT_FFF :
		case DVM_BITRIT_FFF :
			GetFloatOperands( vmc, & dA, & dB, & dC );
			dA = CreateFPToUI( dA, dint_type );
			dB = CreateFPToUI( dB, dint_type );
			switch( code ){
			case DVM_BITAND_FFF : value = CreateAnd( dA, dB ); break;
			case DVM_BITOR_FFF : value = CreateOr( dA, dB ); break;
			case DVM_BITXOR_FFF : value = CreateXor( dA, dB ); break;
			case DVM_BITLFT_FFF : value = CreateShl( dA, dB ); break;
			case DVM_BITRIT_FFF : value = CreateLShr( dA, dB ); break;
			}
			value = CreateUIToFP( value, float_type );
			tmp = CreateStore( value, dC );
			break;
		case DVM_ADD_DDD :
		case DVM_SUB_DDD :
		case DVM_MUL_DDD :
		case DVM_DIV_DDD :
		case DVM_MOD_DDD :
			GetDoubleOperands( vmc, & dA, & dB, & dC );
			switch( code ){
			case DVM_ADD_DDD : value = CreateFAdd( dA, dB ); break;
			case DVM_SUB_DDD : value = CreateFSub( dA, dB ); break;
			case DVM_MUL_DDD : value = CreateFMul( dA, dB ); break;
			case DVM_DIV_DDD : value = CreateFDiv( dA, dB ); break;
			case DVM_MOD_DDD : value = CreateFRem( dA, dB ); break;
			// XXX: float mod in daovm.
			}
			tmp = CreateStore( value, dC );
			break;
		case DVM_POW_DDD :
			GetDoubleOperands( vmc, & dA, & dB, & dC );
			tmp = CreateCall2( dao_pow_double, dA, dB );
			tmp = CreateStore( tmp, dC );
			break;
		case DVM_AND_DDD :
		case DVM_OR_DDD :
			GetDoubleOperands( vmc, & dA, & dB, & dC );
			switch( code ){
			case DVM_AND_DDD : value = CreateFCmpOEQ( dA, zero ); break;
			case DVM_OR_DDD  : value = CreateFCmpONE( dA, zero ); break;
			}
			value = CreateSelect( value, dA, dB );
			tmp = CreateStore( value, dC );
			break;
		case DVM_LT_DDD :
		case DVM_LE_DDD :
		case DVM_EQ_DDD :
		case DVM_NE_DDD :
			GetDoubleOperands( vmc, & dA, & dB, & dC );
			switch( code ){
			case DVM_LT_DDD : value = CreateFCmpOLT( dA, dB ); break;
			case DVM_LE_DDD : value = CreateFCmpOLE( dA, dB ); break;
			case DVM_EQ_DDD : value = CreateFCmpOEQ( dA, dB ); break;
			case DVM_NE_DDD : value = CreateFCmpONE( dA, dB ); break;
			}
			value = CreateUIToFP( value, double_type );
			tmp = CreateStore( value, dC );
			break;
		case DVM_BITAND_DDD :
		case DVM_BITOR_DDD :
		case DVM_BITXOR_DDD :
		case DVM_BITLFT_DDD :
		case DVM_BITRIT_DDD :
			GetDoubleOperands( vmc, & dA, & dB, & dC );
			dA = CreateFPToUI( dA, int32_type );
			dB = CreateFPToUI( dB, int32_type );
			switch( code ){
			case DVM_BITAND_DDD : value = CreateAnd( dA, dB ); break;
			case DVM_BITOR_DDD : value = CreateOr( dA, dB ); break;
			case DVM_BITXOR_DDD : value = CreateXor( dA, dB ); break;
			case DVM_BITLFT_DDD : value = CreateShl( dA, dB ); break;
			case DVM_BITRIT_DDD : value = CreateLShr( dA, dB ); break;
			}
			value = CreateUIToFP( value, double_type );
			tmp = CreateStore( value, dC );
			break;
		case DVM_ADD_FNN :
		case DVM_SUB_FNN :
		case DVM_MUL_FNN :
		case DVM_DIV_FNN :
		case DVM_MOD_FNN :
			GetFNNOperands( vmc, & dA, & dB, & dC );
			switch( types[ vmc->a ]->tid ){
			case DAO_INTEGER : dA = CreateUIToFP( dA, double_type ); break;
			case DAO_FLOAT : dA = CreateFPCast( dA, double_type ); break;
			}
			switch( types[ vmc->b ]->tid ){
			case DAO_INTEGER : dB = CreateUIToFP( dB, double_type ); break;
			case DAO_FLOAT : dB = CreateFPCast( dB, double_type ); break;
			}
			switch( code ){
			case DVM_ADD_FNN : value = CreateFAdd( dA, dB ); break;
			case DVM_SUB_FNN : value = CreateFSub( dA, dB ); break;
			case DVM_MUL_FNN : value = CreateFMul( dA, dB ); break;
			case DVM_DIV_FNN : value = CreateFDiv( dA, dB ); break;
			case DVM_MOD_FNN : value = CreateFRem( dA, dB ); break;
			// XXX: float mod in daovm.
			}
			value = CreateFPCast( value, float_type );
			tmp = CreateStore( value, dC );
			break;
		case DVM_POW_FNN :
			GetFNNOperands( vmc, & dA, & dB, & dC );
			dA = CreateFPCast( dA, double_type );
			dB = CreateFPCast( dB, double_type );
			switch( types[ vmc->a ]->tid ){
			case DAO_INTEGER : dA = CreateUIToFP( dA, double_type ); break;
			case DAO_FLOAT : dA = CreateFPCast( dA, double_type ); break;
			}
			switch( types[ vmc->b ]->tid ){
			case DAO_INTEGER : dB = CreateUIToFP( dB, double_type ); break;
			case DAO_FLOAT : dB = CreateFPCast( dB, double_type ); break;
			}
			tmp = CreateCall2( dao_pow_double, dA, dB );
			tmp = CreateFPCast( tmp, float_type );
			tmp = CreateStore( tmp, dC );
			break;
		case DVM_LT_FNN :
		case DVM_LE_FNN :
		case DVM_EQ_FNN :
		case DVM_NE_FNN :
			GetFNNOperands( vmc, & dA, & dB, & dC );
			switch( types[ vmc->a ]->tid ){
			case DAO_INTEGER : dA = CreateUIToFP( dA, double_type ); break;
			case DAO_FLOAT : dA = CreateFPCast( dA, double_type ); break;
			}
			switch( types[ vmc->b ]->tid ){
			case DAO_INTEGER : dB = CreateUIToFP( dB, double_type ); break;
			case DAO_FLOAT : dB = CreateFPCast( dB, double_type ); break;
			}
			switch( code ){
			case DVM_LT_FNN : value = CreateFCmpOLT( dA, dB ); break;
			case DVM_LE_FNN : value = CreateFCmpOLE( dA, dB ); break;
			case DVM_EQ_FNN : value = CreateFCmpOEQ( dA, dB ); break;
			case DVM_NE_FNN : value = CreateFCmpONE( dA, dB ); break;
			}
			value = CreateUIToFP( value, float_type );
			tmp = CreateStore( value, dC );
			break;
		case DVM_BITLFT_FNN :
		case DVM_BITRIT_FNN :
			GetFNNOperands( vmc, & dA, & dB, & dC );
			if( types[ vmc->a ]->tid != DAO_INTEGER ) dA = CreateFPToUI( dA, dint_type );
			if( types[ vmc->b ]->tid != DAO_INTEGER ) dB = CreateFPToUI( dB, dint_type );
			switch( code ){
			case DVM_BITLFT_FNN : value = CreateShl( dA, dB ); break;
			case DVM_BITRIT_FNN : value = CreateLShr( dA, dB ); break;
			}
			value = CreateUIToFP( value, float_type );
			tmp = CreateStore( value, dC );
			break;
		case DVM_ADD_DNN :
		case DVM_SUB_DNN :
		case DVM_MUL_DNN :
		case DVM_DIV_DNN :
		case DVM_MOD_DNN :
			GetDNNOperands( vmc, & dA, & dB, & dC );
			switch( types[ vmc->a ]->tid ){
			case DAO_INTEGER : dA = CreateUIToFP( dA, double_type ); break;
			case DAO_FLOAT : dA = CreateFPCast( dA, double_type ); break;
			}
			switch( types[ vmc->b ]->tid ){
			case DAO_INTEGER : dB = CreateUIToFP( dB, double_type ); break;
			case DAO_FLOAT : dB = CreateFPCast( dB, double_type ); break;
			}
			dA->print( outs() ); printf( "\n" );
			dB->print( outs() ); printf( "\n" );
			switch( code ){
			case DVM_ADD_DNN : value = CreateFAdd( dA, dB ); break;
			case DVM_SUB_DNN : value = CreateFSub( dA, dB ); break;
			case DVM_MUL_DNN : value = CreateFMul( dA, dB ); break;
			case DVM_DIV_DNN : value = CreateFDiv( dA, dB ); break;
			case DVM_MOD_DNN : value = CreateFRem( dA, dB ); break;
			// XXX: float mod in daovm.
			}
			tmp = CreateStore( value, dC );
			break;
		case DVM_POW_DNN :
			GetDNNOperands( vmc, & dA, & dB, & dC );
			dA = CreateFPCast( dA, double_type );
			dB = CreateFPCast( dB, double_type );
			switch( types[ vmc->a ]->tid ){
			case DAO_INTEGER : dA = CreateUIToFP( dA, double_type ); break;
			case DAO_FLOAT : dA = CreateFPCast( dA, double_type ); break;
			}
			switch( types[ vmc->b ]->tid ){
			case DAO_INTEGER : dB = CreateUIToFP( dB, double_type ); break;
			case DAO_FLOAT : dB = CreateFPCast( dB, double_type ); break;
			}
			tmp = CreateCall2( dao_pow_double, dA, dB );
			tmp = CreateStore( tmp, dC );
			break;
		case DVM_LT_DNN :
		case DVM_LE_DNN :
		case DVM_EQ_DNN :
		case DVM_NE_DNN :
			GetDNNOperands( vmc, & dA, & dB, & dC );
			switch( types[ vmc->a ]->tid ){
			case DAO_INTEGER : dA = CreateUIToFP( dA, double_type ); break;
			case DAO_FLOAT : dA = CreateFPCast( dA, double_type ); break;
			}
			switch( types[ vmc->b ]->tid ){
			case DAO_INTEGER : dB = CreateUIToFP( dB, double_type ); break;
			case DAO_FLOAT : dB = CreateFPCast( dB, double_type ); break;
			}
			switch( code ){
			case DVM_LT_DNN : value = CreateFCmpOLT( dA, dB ); break;
			case DVM_LE_DNN : value = CreateFCmpOLE( dA, dB ); break;
			case DVM_EQ_DNN : value = CreateFCmpOEQ( dA, dB ); break;
			case DVM_NE_DNN : value = CreateFCmpONE( dA, dB ); break;
			}
			value = CreateUIToFP( value, double_type );
			tmp = CreateStore( value, dC );
			break;
		case DVM_BITLFT_DNN :
		case DVM_BITRIT_DNN :
			GetDNNOperands( vmc, & dA, & dB, & dC );
			if( types[ vmc->a ]->tid != DAO_INTEGER ) dA = CreateFPToUI( dA, dint_type );
			if( types[ vmc->b ]->tid != DAO_INTEGER ) dB = CreateFPToUI( dB, dint_type );
			switch( code ){
			case DVM_BITLFT_DNN : value = CreateShl( dA, dB ); break;
			case DVM_BITRIT_DNN : value = CreateLShr( dA, dB ); break;
			}
			value = CreateUIToFP( value, double_type );
			tmp = CreateStore( value, dC );
			break;
		case DVM_GOTO : case DVM_TEST_I : case DVM_TEST_F : case DVM_TEST_D : 
		case DVM_SWITCH : case DVM_CASE :
			break;
		default : goto Failed;
		}
		if( branchings.find( i ) != branchings.end() ) branchings[i] = activeBlock;
		//if( code == DVM_SWITCH ) i += vmc->c + 1; // skip cases and one goto.
	}
	for(iter=branchings.begin(), stop=branchings.end(); iter!=stop; iter++){
		vmc = vmcs[ iter->first ];
		code = vmc->code;
		k = iter->first + 1;
		switch( code ){
		case DVM_GOTO : case DVM_TEST_I : case DVM_TEST_F : case DVM_TEST_D :
		case DVM_SWITCH :
			k = vmc->b;
			break;
		}
		if( k > end ){
			labels[end+1] = NewBlock( vmcs[end+1] );
			break;
		}
	}
	for(iter=branchings.begin(), stop=branchings.end(); iter!=stop; iter++){
		vmc = vmcs[ iter->first ];
		code = vmc->code;
		if( labels[ vmc->b ] == NULL ) labels[vmc->b] = lastBlock;
		//printf( "%3i %3i %p\n", iter->first, vmc->b, labels[vmc->b] );
		printf( "%3i %9p: ", iter->first, iter->second ); DaoVmCodeX_Print( *vmc, NULL );
		SetActiveBlock( iter->second );
		switch( code ){
		case DVM_GOTO :
			CreateBr( labels[ vmc->b ] );
			break;
		case DVM_TEST_I :
			dA = GetLocalValueDataPointer( vmc->a );
			dA = Dereference( CastIntegerPointer( dA ) );
			value = CreateICmpNE( dA, zero );
			CreateCondBr( value, labels[ iter->first + 1 ], labels[ vmc->b ] );
			break;
		case DVM_TEST_F :
			dA = GetLocalValueDataPointer( vmc->a );
			dA = Dereference( CastFloatPointer( dA ) );
			value = CreateUIToFP( zero, float_type );
			value = CreateFCmpONE( dA, value );
			CreateCondBr( value, labels[ iter->first + 1 ], labels[ vmc->b ] );
			break;
		case DVM_TEST_D :
			dA = Dereference( GetLocalValueDataPointer( vmc->a ) );
			value = CreateUIToFP( zero, double_type );
			value = CreateFCmpONE( dA, value );
			CreateCondBr( value, labels[ iter->first + 1 ], labels[ vmc->b ] );
			break;
		case DVM_SWITCH :
			m = types[vmc->a]->tid; // integer or enum
			dA = GetLocalValueDataPointer( vmc->a );
			if( m == DAO_ENUM ){
				dA = CreatePointerCast( dA, dao_enum_type_pp );
				dA = Dereference( dA );
				dA = CreateConstGEP2_32( dA, 0, 1 );
			}
			dA = Dereference( CastIntegerPointer( dA ) );
			inswitch = CreateSwitch( dA, labels[ vmc->b ], vmc->c );
			// use DVM_CASE to add switch labels
			for(k=1; k<=vmc->c; k++){
				DaoVmCodeX *vmc2 = vmcs[ iter->first + k ];
				dint ic = routConsts[vmc2->a].v.i;
				if( m == DAO_ENUM ) ic = routConsts[vmc2->a].v.e->value;
				caseint = cast<ConstantInt>( ConstantInt::get( dint_type, ic ) );
				inswitch->addCase( caseint, labels[vmc2->b] );
			}
			break;
		default :
			if( iter->first < end ){
				CreateBr( labels[ iter->first + 1 ] );
			}else{
				CreateBr( labels[ end + 1 ] );
			}
			break;
		}
	}
	SetInsertPoint( lastBlock );
	CreateRetVoid();
#if 0
	if( 1 ){
		BasicBlock::iterator iter;
		for(iter=entryBlock->begin(); iter!=entryBlock->end(); iter++){
			iter->print( outs() );
			printf( "\n" );
		}
	}
#endif
	return jitfunc;
Failed:
	printf( "failed compiling: %s %4i %4i\n", routine->routName->mbs, start, end );
	jitfunc->eraseFromParent();
	return NULL;
}
void DaoJIT_Compile( DaoRoutine *routine )
{
	DaoJitHandle handle( *llvm_context, routine );
	std::vector<Function*> jitFunctions;
	std::vector<IndexRange> segments;
	DaoJIT_SearchCompilable( routine, segments );
	for(int i=0, n=segments.size(); i<n; i++){
		if( (segments[i].end - segments[i].start) < 10 ) continue;
		printf( "compiling: %5i %5i\n", segments[i].start, segments[i].end );
		Function *jitfunc = handle.Compile( routine, segments[i].start, segments[i].end );
		if( jitfunc == NULL ) continue;
		DaoVmCode *vmc = routine->vmCodes->codes + segments[i].start;
		vmc->code = DVM_JITC;
		vmc->a = jitFunctions.size();
		vmc->b = segments[i].end - segments[i].start + 1;
		jitFunctions.push_back( jitfunc );
	}
	if( jitFunctions.size() ) routine->jitData = new std::vector<Function*>( jitFunctions );
	verifyModule(*llvm_module, PrintMessageAction);
	PassManager PM;
	PM.add(createPrintModulePass(&outs()));
	PM.run(*llvm_module);
}
void DaoJIT_Execute( DaoContext *context, int jitcode )
{
	DaoRoutine *routine = context->routine;
	std::vector<Function*> & jitFuncs = *(std::vector<Function*>*) routine->jitData;
	std::vector<GenericValue> Args(2);
	Args[0].PointerVal = context;
	Args[1].PointerVal = routine;
	GenericValue GV = llvm_exe_engine->runFunction( jitFuncs[jitcode], Args);
}

