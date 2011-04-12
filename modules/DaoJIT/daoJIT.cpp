
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
const StructType *dao_base_type = NULL; // DaoBase
const StructType *dao_routine_type = NULL; // DaoRoutine
const StructType *dao_context_type = NULL; // DaoContext

const PointerType *void_type_p = NULL; // DValue*

const PointerType *dao_value_type_p = NULL; // DValue*
const PointerType *dao_value_type_pp = NULL; // DValue**

const PointerType *dao_varray_type_p = NULL; // DVarray*
const PointerType *dao_base_type_p = NULL; // DaoBase*
const PointerType *dao_routine_type_p = NULL; // DaoRoutine*
const PointerType *dao_context_type_p = NULL; // DaoContext*

const ArrayType *dao_value_array_type = NULL; // DValue[]
const ArrayType *dao_value_ptr_array_type = NULL; // DValue*[]
const PointerType *dao_value_array_type_p = NULL; // DValue[]*
const PointerType *dao_value_ptr_array_type_p = NULL; // DValue*[]*

int dao_opcode_compilable[ DVM_NULL ];

extern "C"{
DValue** DaoContext_GetLocalValues( DaoContext *daoctx )
{
	printf( "DaoContext_GetLocalValues: %p\n", daoctx );
	return daoctx->regValues;
}
}
Function *dao_context_get_local_values = NULL;
Function *dao_pow_double = NULL;

FunctionType *dao_jit_function_type = NULL;

void DaoJIT_Init( DaoVmSpace *vms )
{
	int i;
	memset( dao_opcode_compilable, 0, DVM_NULL*sizeof(int) );
	for(i=DVM_MOVE_II; i<=DVM_MOVE_DD; i++) dao_opcode_compilable[i] = 1;
	for(i=DVM_ADD_III; i<=DVM_BITRIT_DNN; i++) dao_opcode_compilable[i] = 1;
	for(i=DVM_GETF_KCI; i<=DVM_SETF_OVDD; i++) dao_opcode_compilable[i] = 1;
	dao_opcode_compilable[ DVM_NOP ] = 1;
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

	std::vector<const Type*> Double1( 1, dao_context_type_p );
	FunctionType *FT = FunctionType::get(void_type,Double1,false);
	dao_context_get_local_values = Function::Create( FT, Function::ExternalLinkage, "DaoContext_GetLocalValues", llvm_module);


	std::vector<const Type*> double2( 2, double_type );
	FunctionType *funtype = FunctionType::get( double_type, double2, false );
	dao_pow_double = Function::Create( funtype, Function::ExternalLinkage, "pow", llvm_module );


	llvm_exe_engine = EngineBuilder( llvm_module ).setEngineKind(EngineKind::JIT).create();
	llvm_exe_engine->addGlobalMapping( dao_context_get_local_values, 
			(void*)(DaoContext_GetLocalValues));
	llvm_exe_engine->addGlobalMapping( dao_pow_double, (void*) pow );
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
	activeBlock = entryBlock;
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


struct IndexRange
{
	int start;
	int end;
	IndexRange( int s=0, int e=0 ){ start = s; end = e; }

	static int Locate( const std::vector<IndexRange> & ranges, int index ){
		for(int i=0, n=ranges.size(); i<n; i++){
			const IndexRange & range = ranges[i];
			if( range.start <= index and index <= range.end ) return i;
		}
		return -1;
	}
};


void DaoJIT_Free( DaoRoutine *routine )
{
}
void DaoJIT_SearchCompilable( DaoRoutine *routine, std::vector<IndexRange> & segments )
{
	DaoVmCodeX *vmc, **vmcs = routine->annotCodes->items.pVmc;
	std::stack<IndexRange> bounds;
	int i, j, k, m, open = 0, N = routine->annotCodes->size;
	int last_uncomp = -1;
	bool compilable;
	bounds.push( IndexRange( 0, N-1 ) );
	for(i=0; i<N; i++){
		//printf( "i = %i: %i %i\n", i, (int)bounds.size(), (int)segments.size() );
		if( i > bounds.top().end and segments.size() > 1 ){ // merge segments
#if 0
			printf( "try merging...\n" );
			for(j=0, m=segments.size(); j<m; j++){
				printf( "segment %3i: %5i %5i\n", j, segments[j].start, segments[j].end );
			}
#endif
			IndexRange & seg = segments.back();
			m = segments.size();
			vmc = vmcs[seg.start];
			if( seg.start == (i-1) and vmc->code == DVM_GOTO and vmc->b < i ){
				// jump backward goto:
				bool merged = false;
				if( (k = IndexRange::Locate( segments, vmc->b )) >= 0 ){
					IndexRange & segoto = segments[k];
					if( last_uncomp < segoto.start ){ // merge
						segoto.end = seg.end;
						while( (int)segments.size() > (k+1) ) segments.pop_back();
						merged = true;
					}
				}
				if( merged == false ){
					last_uncomp = i-1;
					segments.pop_back();
				}
			}else{
				while( (m = segments.size()) > 1 ){
					if( (segments[m-2].end + 1) < segments[m-1].start ) break;
					k = segments.back().end;
					segments.pop_back();
					segments.back().end = k;
				}
			}
			bounds.pop();
		}
		vmc = vmcs[i];
		switch( vmc->code ){
		case DVM_GOTO :
			if( vmc->b <= (bounds.top().end + 1) ){
				segments.push_back( IndexRange( i, i ) );
			}else{
				last_uncomp = i;
			}
			break;
		case DVM_TEST_I : case DVM_TEST_F : case DVM_TEST_D :
			bounds.push( IndexRange( i+1, vmc->b - 1 ) );
			segments.push_back( IndexRange( i, i ) );
			break;
		default :
			compilable = dao_opcode_compilable[ vmc->code ];
			if( vmc->code == DVM_DATA ) compilable = vmc->a <= DAO_DOUBLE;
			if( vmc->code == DVM_GETCL ) compilable = vmc->a == 0;
			if( compilable ){
				if( segments.size() == 0 or segments.back().end < (i-1) ){
					segments.push_back( IndexRange( i, i ) );
				}else{
					if( i == bounds.top().start ) segments.push_back( IndexRange( i, i ) );
					segments.back().end = i;
				}
			}else{
				last_uncomp = i;
			}
			break;
		}
	}
	for(i=0,k=segments.size(); i<k; i++){
		printf( "%3i: %5i %5i\n", i, segments[i].start, segments[i].end );
	}
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
	A = CastDoublePointer( A );
	B = CastDoublePointer( B );
	C = CastDoublePointer( C );
	A = Dereference( A );
	B = Dereference( B );
	*dA = A;
	*dB = B;
	*dC = C;
}
Function* DaoJitHandle::Compile( DaoRoutine *routine, int start, int end )
{
	DaoType **regtypes = routine->regType->items.pType;
	DaoVmCodeX *vmc, **vmcs = routine->annotCodes->items.pVmc;
	Function *jitfunc = NewFunction( routine, start );

	Constant *zero32 = ConstantInt::get( int32_type, 0 );
	Constant *zero8 = ConstantInt::get( int8_type, 0 );
	Constant *zero = ConstantInt::get( dint_type, 0 );
	Constant *tid8_integer = ConstantInt::get( int8_type, DAO_INTEGER );
	Constant *tid8_float = ConstantInt::get( int8_type, DAO_FLOAT );
	Constant *tid8_double = ConstantInt::get( int8_type, DAO_DOUBLE );
	Constant *type_ids[DAO_DOUBLE+1] = { zero8, tid8_integer, tid8_float, tid8_double };
	Value *type, *vdata, *dA, *dB, *dC, *value=NULL, *tmp;
	int code, k;

	for(int i=start; i<=end; i++){
		vmc = vmcs[i];
		code = vmc->code;
		switch( code ){
		case DVM_NOP :
			break;
		case DVM_DATA :
			dC = GetLocalValue( vmc->c );
			type = GetValueTypePointer( dC );
			vdata = GetValueDataPointer( dC );
			value = getInt32( (int) vmc->b );
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
		case DVM_MOVE_II : case DVM_MOVE_IF : case DVM_MOVE_ID :
		case DVM_MOVE_FI : case DVM_MOVE_FF : case DVM_MOVE_FD :
		case DVM_MOVE_DI : case DVM_MOVE_DF : case DVM_MOVE_DD :
			dA = GetLocalValueDataPointer( vmc->a );
			dC = GetLocalValueDataPointer( vmc->c );
			switch( regtypes[ vmc->a ]->tid ){
			case DAO_INTEGER : dA = CastIntegerPointer( dA ); break;
			case DAO_FLOAT   : dA = CastFloatPointer( dA ); break;
			case DAO_DOUBLE  : dA = CastDoublePointer( dA ); break;
			}
			switch( regtypes[ vmc->c ]->tid ){
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
		default : goto Failed;
		}
	}
	CreateRetVoid();
	return jitfunc;
Failed:
	delete jitfunc;
	return NULL;
}
void DaoJIT_Compile( DaoRoutine *routine )
{
	DaoJitHandle handle( *llvm_context );
	std::vector<Function*> jitFunctions;
	std::vector<IndexRange> segments;
	DaoJIT_SearchCompilable( routine, segments );
	for(int i=0, n=segments.size(); i<n; i++){
		//if( (segments[i].end - segments[i].start) < 10 ) continue;
		printf( "compiling: %5i %5i\n", segments[i].start, segments[i].end );
		Function *jitfunc = handle.Compile( routine, segments[i].start, segments[i].end );
		if( jitfunc == NULL ) continue;
		DaoVmCode *vmc = routine->vmCodes->codes + segments[i].start;
		vmc->code = DVM_JITC;
		vmc->a = jitFunctions.size();
		vmc->b = segments[i].end - segments[i].start + 1;
		jitFunctions.push_back( jitfunc );
		break;
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

