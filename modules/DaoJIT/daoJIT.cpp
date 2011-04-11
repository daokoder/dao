
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

#include "daoJIT.h"

using namespace llvm;

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
	dvalue_types.push_back( void_type_p );

	// type { i8, i8, i8, i8, i8* }
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

LoadInst* DaoJIT_Dereference( Value *pvalue, BasicBlock *block )
{
	return new LoadInst( pvalue, "value", block );
}
GetElementPtrInst* DaoJIT_GetElementPointer( Value *pvalue, int id, BasicBlock *block )
{
	Value *ids[2];
	ids[0] = ConstantInt::get( int32_type, 0 );
	ids[1] = ConstantInt::get( int32_type, id );
	return GetElementPtrInst::Create( pvalue, ids, ids+2, "pfield", block );
}
// Create a function with signature: void (DaoContext*,DaoRoutine*)
Function* DaoJIT_NewFunction( DaoRoutine *routine, int id )
{
	char buf[100];
	std::string name = routine->routName->mbs;
	sprintf( buf, "_%p_%i", routine, id );
	name += buf;
	Function *jitFunc = cast<Function>( llvm_module->getOrInsertFunction( 
				name, void_type, dao_context_type_p, dao_routine_type_p, (Type *)0));

	BasicBlock *BB = BasicBlock::Create(*llvm_context, "EntryBlock", jitFunc);
	Argument *ctx = jitFunc->arg_begin();
	Argument *rout = (++jitFunc->arg_begin());
	ctx->setName("context");
	rout->setName("routine");

	GetElementPtrInst *pppvalues = DaoJIT_GetElementPointer( ctx, 9, BB );
	DaoJIT_Dereference( pppvalues, BB )->setName( "context.regValues" );
	GetElementPtrInst *ppvarray = DaoJIT_GetElementPointer( rout, 17, BB );
	Value *pvarray = DaoJIT_Dereference( ppvarray, BB );
	Value *values = DaoJIT_GetElementPointer( pvarray, 0, BB );
	values = DaoJIT_Dereference( values, BB ); // routine->routConsts->data[]*
	pvarray->setName( "routie.routConsts" );
	return jitFunc;
}
Value* DaoJIT_GetLocalValueRef( Function *jitFunc, int id, BasicBlock *block )
{
	BasicBlock *entryBlock = & jitFunc->getEntryBlock();
	BasicBlock::iterator it = entryBlock->begin(); // context
	Instruction *ppvalues = ++it; // context->regValues
	// context->regValues + id
	GetElementPtrInst *ppvalue = DaoJIT_GetElementPointer( ppvalues, id, block );
	ppvalue->setName( "local_ref" );
	return ppvalue;
}
Value* DaoJIT_GetLocalValue( Function *jitFunc, int id, BasicBlock *block )
{
	Value *ppvalue = DaoJIT_GetLocalValueRef( jitFunc, id, block );
	LoadInst *pvalue = new LoadInst( ppvalue, "pvalue", block );
	pvalue->setName( "local_value" );
	return pvalue;
}
Value* DaoJIT_GetLocalConstant( Function *jitFunc, int id, BasicBlock *block )
{
	BasicBlock *entryBlock = & jitFunc->getEntryBlock();
	BasicBlock::iterator it = entryBlock->begin(); // routine
	Instruction *values = ++(++(++(++(++it)))); // routine->routConsts
	// routine->routConsts->data + id
	GetElementPtrInst *pvalue = DaoJIT_GetElementPointer( values, id, block );
	pvalue->setName( "local_const" );
	return pvalue;
}
GetElementPtrInst* DaoJIT_GetValueTypePointer( Value *pvalue, BasicBlock *block )
{
	return DaoJIT_GetElementPointer( pvalue, 0, block );
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
	const PointerType *type = PointerType::getUnqual( double_type );
	return CastInst::CreatePointerCast( pvalue, type, "todouble", block );
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
Value* DaoJIT_GetLocalReference( Function *func, int reg, std::vector<Value*> & refs )
{
	BasicBlock *entryBlock = & func->getEntryBlock();
	if( refs[reg] ) return refs[reg];
	Value *ref = DaoJIT_GetLocalValueRef( func, reg, entryBlock );
	refs[reg] = ref;
	return ref;
}
Value* DaoJIT_GetLocalValue( Function *func, BasicBlock *bl, int reg, std::vector<Value*> & refs )
{
	Value *ref = DaoJIT_GetLocalReference( func, reg, refs );
	Value *value = new LoadInst( ref, "pvalue", bl );
	return value;
}
Value* DaoJIT_GetValueDataPointer( Function *func, BasicBlock *bl, int reg, std::vector<Value*> & refs )
{
	Value *value = DaoJIT_GetLocalValue( func, bl, reg, refs );
	Value *vdata = DaoJIT_GetValueDataPointer( value, bl );
	return vdata;
}
Value* DaoJIT_Call( Value *call, Value *param1, Value *param2, BasicBlock *block )
{
	Value *params[2] = { param1, param2 };
	return CallInst::Create( call, params, params+2, "", block);
}
void DaoJIT_GetIntegerOperands( Function *func, BasicBlock *bl, DaoVmCodeX *vmc, 
		std::vector<Value*> & refs, Value **dA, Value **dB, Value **dC )
{
	Value *A = DaoJIT_GetValueDataPointer( func, bl, vmc->a, refs );
	Value *B = DaoJIT_GetValueDataPointer( func, bl, vmc->b, refs );
	Value *C = DaoJIT_GetValueDataPointer( func, bl, vmc->c, refs );
	A = DaoJIT_CastInteger( A, bl );
	B = DaoJIT_CastInteger( B, bl );
	C = DaoJIT_CastInteger( C, bl );
	A = DaoJIT_Dereference( A, bl );
	B = DaoJIT_Dereference( B, bl );
	*dA = A;
	*dB = B;
	*dC = C;
}
void DaoJIT_GetFloatOperands( Function *func, BasicBlock *bl, DaoVmCodeX *vmc, 
		std::vector<Value*> & refs, Value **dA, Value **dB, Value **dC )
{
	Value *A = DaoJIT_GetValueDataPointer( func, bl, vmc->a, refs );
	Value *B = DaoJIT_GetValueDataPointer( func, bl, vmc->b, refs );
	Value *C = DaoJIT_GetValueDataPointer( func, bl, vmc->c, refs );
	A = DaoJIT_CastFloat( A, bl );
	B = DaoJIT_CastFloat( B, bl );
	C = DaoJIT_CastFloat( C, bl );
	A = DaoJIT_Dereference( A, bl );
	B = DaoJIT_Dereference( B, bl );
	*dA = A;
	*dB = B;
	*dC = C;
}
void DaoJIT_GetDoubleOperands( Function *func, BasicBlock *bl, DaoVmCodeX *vmc, 
		std::vector<Value*> & refs, Value **dA, Value **dB, Value **dC )
{
	Value *A = DaoJIT_GetValueDataPointer( func, bl, vmc->a, refs );
	Value *B = DaoJIT_GetValueDataPointer( func, bl, vmc->b, refs );
	Value *C = DaoJIT_GetValueDataPointer( func, bl, vmc->c, refs );
	A = DaoJIT_CastDouble( A, bl );
	B = DaoJIT_CastDouble( B, bl );
	C = DaoJIT_CastDouble( C, bl );
	A = DaoJIT_Dereference( A, bl );
	B = DaoJIT_Dereference( B, bl );
	*dA = A;
	*dB = B;
	*dC = C;
}
Function* DaoJIT_Compile( DaoRoutine *routine, int start, int end )
{
	DaoType **regtypes = routine->regType->items.pType;
	DaoVmCodeX *vmc, **vmcs = routine->annotCodes->items.pVmc;
	Function *jitfunc = DaoJIT_NewFunction( routine, start );
	BasicBlock *block = & jitfunc->getEntryBlock();
	Constant *zero32 = ConstantInt::get( int32_type, 0 );
	Constant *zero8 = ConstantInt::get( int8_type, 0 );
	Constant *zero = ConstantInt::get( dint_type, 0 );
	Constant *tid8_integer = ConstantInt::get( int8_type, DAO_INTEGER );
	Constant *tid8_float = ConstantInt::get( int8_type, DAO_FLOAT );
	Constant *tid8_double = ConstantInt::get( int8_type, DAO_DOUBLE );
	Constant *type_ids[DAO_DOUBLE+1] = { zero8, tid8_integer, tid8_float, tid8_double };
	Value *type, *vdata, *dA, *dB, *dC, *value=NULL, *tmp;
	std::vector<Value*> refers( routine->locRegCount, NULL );
	int code, k;
	for(int i=start; i<=end; i++){
		vmc = vmcs[i];
		code = vmc->code;
		switch( code ){
		case DVM_NOP :
			break;
		case DVM_DATA :
			dC = DaoJIT_GetLocalValue( jitfunc, block, vmc->c, refers );
			type = DaoJIT_GetValueTypePointer( dC, block );
			vdata = DaoJIT_GetValueDataPointer( dC, block );
			value = ConstantInt::get( int32_type, (int) vmc->b );
			tmp = new StoreInst( type_ids[ vmc->a ], type, block );
			dC->setName( "DVM_DATA" );
			type->setName( "DVM_DATA.t" );
			vdata->setName( "DVM_DATA.v" );
			switch( vmc->a ){
			case DAO_NIL :
				break;
			case DAO_INTEGER :
				vdata = DaoJIT_CastInteger( vdata, block );
				break;
			case DAO_FLOAT :
				value = new UIToFPInst( value, float_type, "tofloat", block );
				vdata = DaoJIT_CastFloat( vdata, block );
				break;
			case DAO_DOUBLE :
				value = new UIToFPInst( value, double_type, "todouble", block );
				vdata = DaoJIT_CastFloat( vdata, block );
				break;
			default: goto Failed;
			}
			if( vmc->a ) tmp = new StoreInst( value, vdata, block );
			break;
		case DVM_GETCL :
			if( vmc->a ) goto Failed;
			dC = DaoJIT_GetLocalReference( jitfunc, vmc->c, refers );
			dB = DaoJIT_GetLocalConstant( jitfunc, vmc->b, block );
			tmp = new StoreInst( dB, dC, block );
			break;
		case DVM_MOVE_II : case DVM_MOVE_IF : case DVM_MOVE_ID :
		case DVM_MOVE_FI : case DVM_MOVE_FF : case DVM_MOVE_FD :
		case DVM_MOVE_DI : case DVM_MOVE_DF : case DVM_MOVE_DD :
			dA = DaoJIT_GetValueDataPointer( jitfunc, block, vmc->a, refers );
			dC = DaoJIT_GetValueDataPointer( jitfunc, block, vmc->c, refers );
			switch( regtypes[ vmc->a ]->tid ){
			case DAO_INTEGER : dA = DaoJIT_CastInteger( dA, block ); break;
			case DAO_FLOAT   : dA = DaoJIT_CastFloat( dA, block ); break;
			case DAO_DOUBLE  : dA = DaoJIT_CastDouble( dA, block ); break;
			}
			switch( regtypes[ vmc->c ]->tid ){
			case DAO_INTEGER : dC = DaoJIT_CastInteger( dC, block ); break;
			case DAO_FLOAT   : dC = DaoJIT_CastFloat( dC, block ); break;
			case DAO_DOUBLE  : dC = DaoJIT_CastDouble( dC, block ); break;
			}
			dA = DaoJIT_Dereference( dA, block );
			switch( code ){
			case DVM_MOVE_IF : 
			case DVM_MOVE_ID : dA = new FPToSIInst( dA, dint_type, "i", block ); break;
			case DVM_MOVE_FI : dA = new SIToFPInst( dA, float_type, "f", block ); break;
			case DVM_MOVE_DI : dA = new SIToFPInst( dA, double_type, "d", block ); break;
			case DVM_MOVE_FD : dA = CastInst::CreateFPCast( dA, float_type, "f", block ); break;
			case DVM_MOVE_DF : dA = CastInst::CreateFPCast( dA, double_type, "d", block ); break;
			}
			tmp = new StoreInst( dA, dC, block );
			dC->setName( "DVM_MOVE" );
			break;
		case DVM_ADD_III :
		case DVM_SUB_III :
		case DVM_MUL_III :
		case DVM_DIV_III :
		case DVM_MOD_III :
			DaoJIT_GetIntegerOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			switch( code ){
			case DVM_ADD_III : value = BinaryOperator::CreateAdd( dA, dB, "add", block); break;
			case DVM_SUB_III : value = BinaryOperator::CreateSub( dA, dB, "sub", block); break;
			case DVM_MUL_III : value = BinaryOperator::CreateMul( dA, dB, "mul", block); break;
			case DVM_DIV_III : value = BinaryOperator::CreateSDiv( dA, dB, "div", block); break;
			case DVM_MOD_III : value = BinaryOperator::CreateSRem( dA, dB, "mod", block); break;
			}
			tmp = new StoreInst( value, dC, block );
			break;
		case DVM_POW_III :
			DaoJIT_GetIntegerOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			dA = new SIToFPInst( dA, double_type, "a", block );
			dB = new SIToFPInst( dB, double_type, "b", block );
			tmp = DaoJIT_Call( dao_pow_double, dA, dB, block );
			tmp = new FPToSIInst( tmp, dint_type, "c", block );
			tmp = new StoreInst( tmp, dC, block );
			break;
		case DVM_AND_III :
		case DVM_OR_III :
			DaoJIT_GetIntegerOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			k = code == DVM_AND_III ? ICmpInst::ICMP_EQ : ICmpInst::ICMP_NE;
			value = new ICmpInst( *block, ICmpInst::ICMP_EQ, dA, zero );
			value = SelectInst::Create( value, dA, dB, "and", block );
			tmp = new StoreInst( value, dC, block );
			break;
		case DVM_LT_III :
		case DVM_LE_III :
		case DVM_EQ_III :
		case DVM_NE_III :
			DaoJIT_GetIntegerOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			switch( code ){
			case DVM_LT_III : value = new ICmpInst( *block, ICmpInst::ICMP_SLT, dA, dB ); break;
			case DVM_LE_III : value = new ICmpInst( *block, ICmpInst::ICMP_SLE, dA, dB ); break;
			case DVM_EQ_III : value = new ICmpInst( *block, ICmpInst::ICMP_EQ, dA, dB ); break;
			case DVM_NE_III : value = new ICmpInst( *block, ICmpInst::ICMP_NE, dA, dB ); break;
			}
			value = CastInst::CreateIntegerCast( value, dint_type, true, "i", block );
			tmp = new StoreInst( value, dC, block );
			break;
		case DVM_BITAND_III :
		case DVM_BITOR_III :
		case DVM_BITXOR_III :
		case DVM_BITLFT_III :
		case DVM_BITRIT_III :
			DaoJIT_GetIntegerOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			switch( code ){
			case DVM_BITAND_III : value = BinaryOperator::CreateAnd( dA, dB, "bitand", block); break;
			case DVM_BITOR_III : value = BinaryOperator::CreateOr( dA, dB, "bitor", block); break;
			case DVM_BITXOR_III : value = BinaryOperator::CreateXor( dA, dB, "bitxor", block); break;
			case DVM_BITLFT_III : value = BinaryOperator::CreateShl( dA, dB, "shl", block); break;
			case DVM_BITRIT_III : value = BinaryOperator::CreateLShr( dA, dB, "lshr", block); break;
			}
			tmp = new StoreInst( value, dC, block );
			break;
		case DVM_ADD_FFF :
		case DVM_SUB_FFF :
		case DVM_MUL_FFF :
		case DVM_DIV_FFF :
		case DVM_MOD_FFF :
			DaoJIT_GetFloatOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			switch( code ){
			case DVM_ADD_FFF : value = BinaryOperator::CreateFAdd( dA, dB, "add", block); break;
			case DVM_SUB_FFF : value = BinaryOperator::CreateFSub( dA, dB, "sub", block); break;
			case DVM_MUL_FFF : value = BinaryOperator::CreateFMul( dA, dB, "mul", block); break;
			case DVM_DIV_FFF : value = BinaryOperator::CreateFDiv( dA, dB, "div", block); break;
			case DVM_MOD_FFF : value = BinaryOperator::CreateFRem( dA, dB, "mod", block); break;
			// XXX: float mod in daovm.
			}
			tmp = new StoreInst( value, dC, block );
			break;
		case DVM_POW_FFF :
			DaoJIT_GetFloatOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			dA = CastInst::CreateFPCast( dA, double_type, "a", block );
			dB = CastInst::CreateFPCast( dB, double_type, "b", block );
			tmp = DaoJIT_Call( dao_pow_double, dA, dB, block );
			tmp = CastInst::CreateFPCast( tmp, float_type, "c", block );
			tmp = new StoreInst( tmp, dC, block );
			break;
		case DVM_AND_FFF :
		case DVM_OR_FFF :
			DaoJIT_GetFloatOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			k = code == DVM_AND_FFF ? FCmpInst::ICMP_EQ : FCmpInst::ICMP_NE;
			value = new FCmpInst( *block, FCmpInst::ICMP_EQ, dA, zero );
			value = SelectInst::Create( value, dA, dB, "and", block );
			tmp = new StoreInst( value, dC, block );
			break;
		case DVM_LT_FFF :
		case DVM_LE_FFF :
		case DVM_EQ_FFF :
		case DVM_NE_FFF :
			DaoJIT_GetFloatOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			switch( code ){
			case DVM_LT_FFF : value = new FCmpInst( *block, FCmpInst::ICMP_SLT, dA, dB ); break;
			case DVM_LE_FFF : value = new FCmpInst( *block, FCmpInst::ICMP_SLE, dA, dB ); break;
			case DVM_EQ_FFF : value = new FCmpInst( *block, FCmpInst::ICMP_EQ, dA, dB ); break;
			case DVM_NE_FFF : value = new FCmpInst( *block, FCmpInst::ICMP_NE, dA, dB ); break;
			}
			value = new UIToFPInst( value, float_type, "i", block );
			tmp = new StoreInst( value, dC, block );
			break;
		case DVM_BITAND_FFF :
		case DVM_BITOR_FFF :
		case DVM_BITXOR_FFF :
		case DVM_BITLFT_FFF :
		case DVM_BITRIT_FFF :
			DaoJIT_GetFloatOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			dA = new FPToUIInst( dA, dint_type, "i", block );
			dB = new FPToUIInst( dB, dint_type, "i", block );
			switch( code ){
			case DVM_BITAND_FFF : value = BinaryOperator::CreateAnd( dA, dB, "bitand", block); break;
			case DVM_BITOR_FFF : value = BinaryOperator::CreateOr( dA, dB, "bitor", block); break;
			case DVM_BITXOR_FFF : value = BinaryOperator::CreateXor( dA, dB, "bitxor", block); break;
			case DVM_BITLFT_FFF : value = BinaryOperator::CreateShl( dA, dB, "shl", block); break;
			case DVM_BITRIT_FFF : value = BinaryOperator::CreateLShr( dA, dB, "lshr", block); break;
			}
			value = new UIToFPInst( value, float_type, "f", block );
			tmp = new StoreInst( value, dC, block );
			break;
		case DVM_ADD_DDD :
		case DVM_SUB_DDD :
		case DVM_MUL_DDD :
		case DVM_DIV_DDD :
		case DVM_MOD_DDD :
			DaoJIT_GetDoubleOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			switch( code ){
			case DVM_ADD_DDD : value = BinaryOperator::CreateFAdd( dA, dB, "add", block); break;
			case DVM_SUB_DDD : value = BinaryOperator::CreateFSub( dA, dB, "sub", block); break;
			case DVM_MUL_DDD : value = BinaryOperator::CreateFMul( dA, dB, "mul", block); break;
			case DVM_DIV_DDD : value = BinaryOperator::CreateFDiv( dA, dB, "div", block); break;
			case DVM_MOD_DDD : value = BinaryOperator::CreateFRem( dA, dB, "mod", block); break;
			// XXX: float mod in daovm.
			}
			tmp = new StoreInst( value, dC, block );
			break;
		case DVM_POW_DDD :
			DaoJIT_GetDoubleOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			tmp = DaoJIT_Call( dao_pow_double, dA, dB, block );
			tmp = new StoreInst( tmp, dC, block );
			break;
		case DVM_AND_DDD :
		case DVM_OR_DDD :
			DaoJIT_GetDoubleOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			k = code == DVM_AND_DDD ? FCmpInst::ICMP_EQ : FCmpInst::ICMP_NE;
			value = new FCmpInst( *block, FCmpInst::ICMP_EQ, dA, zero );
			value = SelectInst::Create( value, dA, dB, "and", block );
			tmp = new StoreInst( value, dC, block );
			break;
		case DVM_LT_DDD :
		case DVM_LE_DDD :
		case DVM_EQ_DDD :
		case DVM_NE_DDD :
			DaoJIT_GetDoubleOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			switch( code ){
			case DVM_LT_DDD : value = new FCmpInst( *block, FCmpInst::ICMP_SLT, dA, dB ); break;
			case DVM_LE_DDD : value = new FCmpInst( *block, FCmpInst::ICMP_SLE, dA, dB ); break;
			case DVM_EQ_DDD : value = new FCmpInst( *block, FCmpInst::ICMP_EQ, dA, dB ); break;
			case DVM_NE_DDD : value = new FCmpInst( *block, FCmpInst::ICMP_NE, dA, dB ); break;
			}
			value = new UIToFPInst( value, double_type, "i", block );
			tmp = new StoreInst( value, dC, block );
			break;
		case DVM_BITAND_DDD :
		case DVM_BITOR_DDD :
		case DVM_BITXOR_DDD :
		case DVM_BITLFT_DDD :
		case DVM_BITRIT_DDD :
			DaoJIT_GetDoubleOperands( jitfunc, block, vmc, refers, & dA, & dB, & dC );
			dA = new FPToUIInst( dA, int32_type, "i", block );
			dB = new FPToUIInst( dB, int32_type, "i", block );
			switch( code ){
			case DVM_BITAND_DDD : value = BinaryOperator::CreateAnd( dA, dB, "bitand", block); break;
			case DVM_BITOR_DDD : value = BinaryOperator::CreateOr( dA, dB, "bitor", block); break;
			case DVM_BITXOR_DDD : value = BinaryOperator::CreateXor( dA, dB, "bitxor", block); break;
			case DVM_BITLFT_DDD : value = BinaryOperator::CreateShl( dA, dB, "shl", block); break;
			case DVM_BITRIT_DDD : value = BinaryOperator::CreateLShr( dA, dB, "lshr", block); break;
			}
			value = new UIToFPInst( value, double_type, "f", block );
			tmp = new StoreInst( value, dC, block );
			break;
		default : goto Failed;
		}
	}
	ReturnInst::Create( *llvm_context, block );
	return jitfunc;
Failed:
	delete jitfunc;
	return NULL;
}
void DaoJIT_Compile( DaoRoutine *routine )
{
	std::vector<Function*> jitFunctions;
	std::vector<IndexRange> segments;
	DaoJIT_SearchCompilable( routine, segments );
	for(int i=0, n=segments.size(); i<n; i++){
		//if( (segments[i].end - segments[i].start) < 10 ) continue;
		printf( "compiling: %5i %5i\n", segments[i].start, segments[i].end );
		Function *jitfunc = DaoJIT_Compile( routine, segments[i].start, segments[i].end );
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

