/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2011, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#include"stdio.h"
#include"string.h"
#include"math.h"

#ifndef NO_FENV
#include"fenv.h"
#endif

#include<assert.h>
#include<ctype.h>

#include"daoContext.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoStdlib.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoRoutine.h"
#include"daoVmspace.h"
#include"daoNamespace.h"
#include"daoNumtype.h"
#include"daoRegex.h"
#include"daoStream.h"
#include"daoParser.h"
#include"daoValue.h"

#ifndef FE_ALL_EXCEPT
#define FE_ALL_EXCEPT 0xffff
#endif

#define SEMA_PER_VMPROC  1000

struct DaoJIT dao_jit = { NULL, NULL, NULL, NULL };


extern DaoList* DaoProcess_GetList( DaoProcess *self, DaoVmCode *vmc );

extern void DaoProcess_DoMap( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoList( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoPair( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoTuple( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoArray( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoMatrix( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoCurry( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoCheck( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_BindNameValue( DaoProcess *self, DaoVmCode *vmc );

extern void DaoProcess_DoGetItem( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoSetItem( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoGetField( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoSetField( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoGetMetaField( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoSetMetaField( DaoProcess *self, DaoVmCode *vmc );

extern DaoVmCode* DaoProcess_DoSwitch( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoIter( DaoProcess *self, DaoVmCode *vmc );

extern int DaoProcess_TryObjectArith( DaoProcess *self, DaoValue *dA, DaoValue *dB );
extern int DaoProcess_TryCdataArith( DaoProcess *self, DaoValue *dA, DaoValue *dB );

extern void DaoProcess_DoInTest( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoBinArith( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoBinBool(  DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoUnaArith( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoBitLogic( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoBitShift( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoBitFlip( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoBitFlip( DaoProcess *self, DaoVmCode *vmc );

extern void DaoProcess_DoCast( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoMove( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_DoCall( DaoProcess *self, DaoVmCode *vmc );

extern void DaoProcess_DoFunctional( DaoProcess *self, DaoVmCode *vmc );

/* if return TRUE, there is exception, and look for the next rescue point. */
extern int DaoProcess_DoCheckExcept( DaoProcess *self, DaoVmCode *vmc );
/* if return DAO_STATUS_EXCEPTION, real exception is rose, and look for the next rescue point. */
extern void DaoProcess_DoRaiseExcept( DaoProcess *self, DaoVmCode *vmc );
/* return TRUE, if some exceptions can be rescued */
extern int DaoProcess_DoRescueExcept( DaoProcess *self, DaoVmCode *vmc );

extern void DaoProcess_DoReturn( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_MakeRoutine( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_MakeClass( DaoProcess *self, DaoVmCode *vmc );

static int DaoVM_DoMath( DaoProcess *self, DaoVmCode *vmc, DaoValue *C, DaoValue *A );

static DaoStackFrame* DaoStackFrame_New()
{
	DaoStackFrame *self = dao_calloc( 1, sizeof(DaoStackFrame) );
	return self;
}
#define DaoStackFrame_Delete( p ) dao_free( p )
static void DaoStackFrame_PushRange( DaoStackFrame *self, ushort_t from, ushort_t to )
{
	assert( self->depth < DVM_MAX_TRY_DEPTH );
	self->ranges[ self->depth ][0] = from;
	self->ranges[ self->depth ][1] = to;
	self->depth ++;
}

DaoTypeBase vmpTyper =
{
	"process",
	& baseCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoProcess_Delete, NULL
};

DaoProcess* DaoProcess_New( DaoVmSpace *vms )
{
	int i;
	DaoProcess *self = (DaoProcess*)dao_calloc( 1, sizeof( DaoProcess ) );
	DaoValue_Init( self, DAO_PROCESS );
	self->vmSpace = vms;
	self->status = DAO_VMPROC_SUSPENDED;
	self->exceptions = DArray_New(D_VALUE);

	self->firstFrame = self->topFrame = DaoStackFrame_New();
	self->firstFrame->active = self->firstFrame;
	self->firstFrame->types = & self->dummyType;
	self->firstFrame->codes = & self->dummyCode;
	self->firstFrame->entry = 1;
	self->stackValues = (DaoValue**)dao_calloc( 1+DAO_MAX_PARAM, sizeof(DaoValue*) );
	self->stackSize = 1+DAO_MAX_PARAM;
	self->stackTop = 1;
	self->freeValues = self->stackValues + 1;

	self->mbstring = DString_New(1);
	self->mbsRegex = NULL;
	self->wcsRegex = NULL;
	self->pauseType = 0;
	return self;
}

void DaoProcess_Delete( DaoProcess *self )
{
	DaoStackFrame *frame = self->firstFrame;
	DNode *n;
	size_t i;
	if( self->mbsRegex ){
		n = DMap_First( self->mbsRegex );
		for( ; n !=NULL; n = DMap_Next(self->mbsRegex, n) ) dao_free( n->value.pVoid );
		DMap_Delete( self->mbsRegex );
	}
	if( self->wcsRegex ){
		n = DMap_First( self->wcsRegex );
		for( ; n !=NULL; n = DMap_Next(self->wcsRegex, n) ) dao_free( n->value.pVoid );
		DMap_Delete( self->wcsRegex );
	}
	while( frame ){
		DaoStackFrame *p = frame;
		if( frame->object ) GC_DecRC( frame->object );
		if( frame->routine ) GC_DecRC( frame->routine );
		if( frame->function ) GC_DecRC( frame->function );
		frame = frame->next;
		dao_free( p );
	}
	for(i=0; i<self->stackSize; i++){
		if( self->stackValues[i] ) GC_DecRC( self->stackValues[i] );
	}
	if( self->stackValues ) dao_free( self->stackValues );

	DString_Delete( self->mbstring );
	DArray_Delete( self->exceptions );
	if( self->parResume ) DArray_Delete( self->parResume );
	if( self->parYield ) DArray_Delete( self->parYield );
	if( self->abtype ) GC_DecRC( self->abtype );
	dao_free( self );
}

DaoRegex* DaoProcess_MakeRegex( DaoProcess *self, DString *src, int mbs )
{
	DaoRegex *pat = NULL;
	DaoRgxItem *it;
	DNode *node;
	int i;
	if( mbs && src->wcs ) DString_ToMBS( src );
	if( mbs==0 && src->mbs ) DString_ToWCS( src );
	if( src->mbs ){
		if( self->mbsRegex == NULL ) self->mbsRegex = DHash_New(D_STRING,0);
		node = DMap_Find( self->mbsRegex, src );
		if( node ) return (DaoRegex*) node->value.pVoid;
		pat = DaoRegex_New( src );
		DMap_Insert( self->mbsRegex, src, pat );
	}else{
		if( self->wcsRegex == NULL ) self->wcsRegex = DHash_New(D_STRING,0);
		node = DMap_Find( self->wcsRegex, src );
		if( node ) return (DaoRegex*) node->value.pVoid;
		pat = DaoRegex_New( src );
		DMap_Insert( self->wcsRegex, src, pat );
	}
	for( i=0; i<pat->count; i++ ){
		it = pat->items + i;
		if( it->type ==0 ){
			char buf[50];
			sprintf( buf, "incorrect pattern, at char %i.", it->length );
			DaoProcess_RaiseException( self, DAO_ERROR, buf );
			return NULL;
		}
	}
	return pat;
}

DaoStackFrame* DaoProcess_PushFrame( DaoProcess *self, int size )
{
	size_t i, N = self->stackTop + size + DAO_MAX_PARAM;
	DaoStackFrame *frame = self->topFrame->next;
	if( N > self->stackSize ){
		size_t offset = self->activeValues - self->stackValues;
		self->stackValues = (DaoValue**)dao_realloc( self->stackValues, N*sizeof(DaoValue*) );
		memset( self->stackValues + self->stackSize, 0, (N-self->stackSize)*sizeof(DaoValue*) );
		if( self->activeValues ) self->activeValues = self->stackValues +  offset;
		self->stackSize = N;
	}
	if( frame == NULL ){
		frame = DaoStackFrame_New();
		self->topFrame->next = frame;
		frame->prev = self->topFrame;
	}
	frame->stackBase = self->stackTop;
	frame->entry = 0;
	frame->state = 0;
	frame->returning = -1;
	if( self->activeRoutine && self->activeCode ){
		self->topFrame->entry = (int)(self->activeCode - self->topFrame->codes) + 1;
		frame->returning = self->activeCode->c;
	}
	self->topFrame = frame;
	self->stackTop += size;
	self->freeValues = self->stackValues + self->stackTop;
	return frame;
}
void DaoProcess_PopFrame( DaoProcess *self )
{
	if( self->topFrame == NULL ) return;
	self->status = DAO_VMPROC_RUNNING;
	self->topFrame->depth = 0;
	self->stackTop = self->topFrame->stackBase;
	self->topFrame = self->topFrame->prev;
	self->freeValues = self->stackValues + self->stackTop;
	if( self->topFrame ) DaoProcess_SetActiveFrame( self, self->topFrame->active );
}
void DaoProcess_PopFrames( DaoProcess *self, DaoStackFrame *rollback )
{
	while( self->topFrame != rollback ) DaoProcess_PopFrame( self );
}
void DaoProcess_InitTopFrame( DaoProcess *self, DaoRoutine *routine, DaoObject *object )
{
	DaoStackFrame *frame = self->topFrame;
	DaoValue **values = self->stackValues + frame->stackBase;
	DaoType *routHost = routine->routHost;
	DaoType **types = routine->regType->items.pType;
	size_t *id = routine->definiteNumbers->items.pSize;
	size_t *end = id + routine->definiteNumbers->size;
	int j, need_self = routine->routType->attrib & DAO_TYPE_SELF;
	complex16 com = {0.0,0.0};

	if( need_self && routHost && routHost->tid == DAO_OBJECT ){
		if( object == NULL && values[0]->type == DAO_OBJECT ) object = & values[0]->xObject;
		if( object ) object = (DaoObject*) DaoObject_MapThisObject( object->rootObject, routHost );
		if( object == NULL ) DaoProcess_RaiseException( self, DAO_ERROR, "need self object" );
		GC_ShiftRC( object, frame->object );
		frame->object = object;
	}
	if( routine == frame->routine ) return;
	GC_DecRC( frame->function );
	GC_ShiftRC( routine, frame->routine );
	frame->function = NULL;
	frame->routine = routine;
	frame->codes = routine->vmCodes->codes;
	frame->types = routine->regType->items.pType;
	for(; id != end; id++){
		int i = *id, tid = types[i]->tid;
		DaoValue *value = values[i], *value2;
		if( value && value->type == tid && value->xGC.refCount == 1 && value->xGC.trait == 0 ) continue;
		value2 = NULL;
		switch( tid ){
		case DAO_INTEGER : value2 = (DaoValue*) DaoInteger_New(0); break;
		case DAO_FLOAT   : value2 = (DaoValue*) DaoFloat_New(0.0); break;
		case DAO_DOUBLE  : value2 = (DaoValue*) DaoDouble_New(0.0); break;
		case DAO_COMPLEX : value2 = (DaoValue*) DaoComplex_New(com); break;
		}
		if( value2 == NULL ) continue;
		GC_ShiftRC( value2, value );
		values[i] = value2;
	}
}
void DaoProcess_SetActiveFrame( DaoProcess *self, DaoStackFrame *frame )
{
	frame = frame->active;
	self->activeObject = frame->object;
	self->activeCode = frame->codes + frame->entry - 1;
	self->activeValues = self->stackValues + frame->stackBase;
	self->activeTypes = frame->types;
	self->activeRoutine = frame->routine;
	if( frame->routine ){
		self->activeNamespace = frame->routine->nameSpace;
	}else if( frame->function ){
		self->activeNamespace = frame->function->nameSpace;
	}
}
void DaoProcess_PushRoutine( DaoProcess *self, DaoRoutine *routine, DaoObject *object )
{
	DaoStackFrame *frame = DaoProcess_PushFrame( self, routine->regCount );
	DaoProcess_InitTopFrame( self, routine, object );
	frame->active = frame;
	self->status = DAO_VMPROC_STACKED;
}
void DaoProcess_PushFunction( DaoProcess *self, DaoFunction *function )
{
	DaoStackFrame *frame = DaoProcess_PushFrame( self, function->parCount );
	frame->active = frame->prev->active;
	GC_DecRC( frame->routine );
	GC_ShiftRC( function, frame->function );
	frame->routine = NULL;
	frame->function = function;
	self->status = DAO_VMPROC_STACKED;
}
/* If the callable is a constructor, and O is a derived type of the constructor's type,
 * cast O to the constructor's type and then call the constructor on the casted object: */
int DaoProcess_PushCallable( DaoProcess *self, DaoValue *M, DaoValue *O, DaoValue *P[], int N )
{
	DRoutine *R = (DRoutine*) M;
	int passed = 0;

	if( M == NULL ) return DAO_ERROR;
	if( M->type == DAO_FUNCTREE ) R = DRoutine_Resolve( M, O, P, N, DVM_CALL );
	if( R ) passed = DRoutine_PassParams( R, O, self->freeValues, P, N, DVM_CALL );
	if( passed == 0 ) return DAO_ERROR_PARAM;

	if( R->type == DAO_ROUTINE ){
		DaoProcess_PushRoutine( self, (DaoRoutine*)R, DaoValue_CastObject( O ) );
		self->topFrame->parCount = passed - 1;
	}else if( R->type == DAO_FUNCTION ){
		DaoProcess_PushFunction( self, (DaoFunction*)R );
		self->topFrame->parCount = passed - 1;
	}else{
		return DAO_ERROR;
	}
	return 0;
}
void DaoProcess_InterceptReturnValue( DaoProcess *self )
{
	if( self->topFrame->routine ){
		self->topFrame->returning = -1;
	}else if( self->topFrame->function ){
		self->topFrame->active = self->firstFrame;
		DaoProcess_SetActiveFrame( self, self->firstFrame );
	}
}
int DaoProcess_Resume( DaoProcess *self, DaoValue *par[], int N, DaoList *list )
{
	if( self->status == DAO_VMPROC_SUSPENDED && self->parResume ){
		int i;
		DArray_Clear( self->parResume );
		for( i=0; i<N; i++ ) DArray_Append( self->parResume, par[i] );

		DaoProcess_Execute( self );
		if( list )
			for( i=0; i<self->parYield->size; i++ )
				DaoList_Append( list, self->parYield->items.pValue[i] );
	}else if( self->status == DAO_VMPROC_SUSPENDED && self->pauseType == DAO_VMP_ASYNC ){
		DaoProcess_Execute( self );
	}
	return self->status;
}
void DaoProcess_Yield( DaoProcess *self, DaoValue *par[], int N, DaoList *list )
{
	int i;
	if( self->parYield == NULL ) return;
	DArray_Clear( self->parYield );
	for( i=0; i<N; i++ ) DArray_Append( self->parYield, par[i] );
	if( list ){
		for( i=0; i<self->parResume->size; i++ )
			DaoList_Append( list, self->parResume->items.pValue[i] );
	}
	self->status = DAO_VMPROC_SUSPENDED;
}
void DaoProcess_MakeTuple( DaoProcess *self, DaoTuple *tuple, DaoValue *its[], int N );
int DaoProcess_Resume2( DaoProcess *self, DaoValue *par[], int N, DaoProcess *ret )
{
	DaoType *tp;
	DaoVmCode *vmc;
	DaoTuple *tuple;
	if( self->status != DAO_VMPROC_SUSPENDED ) return 0;
	if( self->activeCode && self->activeCode->code == DVM_YIELD ){
		tp = self->activeTypes[ self->activeCode->c ];
		if( N == 1 ){
			DaoProcess_PutValue( self, par[0] );
		}else if( N ){
			tuple = DaoTuple_New( N );
			tuple->unitype = tp;
			GC_IncRC( tuple->unitype );
			DaoProcess_MakeTuple( self, tuple, par, N );
			DaoProcess_PutValue( self, (DaoValue*) tuple );
		}
		self->topFrame->entry ++;
	}else if( N ){
		int m = 0;
		DRoutine *rout = (DRoutine*)self->topFrame->routine;
		DaoValue **values = self->stackValues + self->topFrame->stackBase;
		if( rout ) m = DRoutine_PassParams( rout, NULL, values, par, N, DVM_CALL );
		if( m ==0 ){
			DaoProcess_RaiseException( ret, DAO_ERROR, "invalid parameters." );
			return 0;
		}
	}
	DaoProcess_Execute( self );
	if( ret->activeRoutine && ret->activeCode ){ /* yield */
		vmc = self->activeCode;
		tp = ret->activeTypes[ ret->activeCode->c ];
		if( vmc->b ==1 ){
			DaoProcess_PutValue( ret, self->activeValues[ vmc->a ] );
		}else if( vmc->b ){
			tuple = DaoTuple_New( vmc->b );
			tuple->unitype = tp;
			GC_IncRC( tuple->unitype );
			DaoProcess_MakeTuple( ret, tuple, self->activeValues + vmc->a, vmc->b );
			DaoProcess_PutValue( ret, (DaoValue*) tuple );
		}
	}else{ /* return */
		DaoProcess_PutValue( ret, self->stackValues[0] );
	}
	return 1;
}

int DaoProcess_ExecuteSection( DaoProcess *self, int entry )
{
	ushort_t ranges[DVM_MAX_TRY_DEPTH][2];
	int depth = self->topFrame->depth;
	int ret = 0, old = 0;
	if( entry <0 ) return DAO_VMPROC_ABORTED;
	if( self->topFrame->codes[entry].code != DVM_SECT ) return DAO_VMPROC_ABORTED;
	old = self->topFrame->state;
	self->topFrame->entry = entry + 1;
	self->topFrame->state = DVM_SPEC_RUN;
	self->topFrame->depth = 0;
	memcpy( ranges, self->topFrame->ranges, 2*depth*sizeof(short) );
	ret = DaoProcess_Execute( self );
	memcpy( self->topFrame->ranges, ranges, 2*depth*sizeof(short) );
	self->topFrame->depth = depth;
	self->topFrame->state = old;
	return ret;
}
int DaoProcess_Compile( DaoProcess *self, DaoNamespace *ns, DString *src, int rpl )
{
	DaoParser *p;
	int res;

	src = DString_Copy( src );
	DString_ToMBS( src );
	p = DaoParser_New();
	if( self->topFrame ) /* source name as parameter ??? */
		DString_SetMBS( p->fileName, "code string" );
	else
		DString_Assign( p->fileName, self->vmSpace->fileName );
	p->vmSpace = self->vmSpace;
	p->nameSpace = ns;
	res = DaoParser_LexCode( p, src->mbs, rpl ) && DaoParser_ParseScript( p );
	p->routine->parser = NULL;
	DaoParser_Delete( p );
	DString_Delete( src );
	return res;
}
int DaoProcess_Eval( DaoProcess *self, DaoNamespace *ns, DString *source, int rpl )
{
	DaoRoutine *rout;
	if( DaoProcess_Compile( self, ns, source, rpl ) ==0 ) return 0;
	rout = ns->mainRoutines->items.pRout[ ns->mainRoutines->size-1 ];
	if( DaoProcess_Call( self, (DaoMethod*) rout, NULL, NULL, 0 ) ) return 0;
	return ns->mainRoutines->size;
}
// XXX return value changed!!
int DaoProcess_Call( DaoProcess *self, DaoMethod *M, DaoValue *O, DaoValue *P[], int N )
{
	int ret = DaoProcess_PushCallable( self, (DaoValue*)M, O, P, N );
	if( ret ) return ret;
	/* no return value to the previous stack frame */
	DaoProcess_InterceptReturnValue( self );
	return DaoProcess_Execute( self ) == 0 ? DAO_ERROR : 0;
}
void DaoProcess_CallFunction( DaoProcess *self, DaoFunction *func, DaoValue *p[], int n )
{
	DaoValue *params[ DAO_MAX_PARAM ];
	memcpy( params, p, func->parCount*sizeof(DaoValue*) );
	func->pFunc( self, params, n );
}
void DaoProcess_Stop( DaoProcess *self )
{
	self->stopit = 1;
}
DaoValue* DaoProcess_GetReturned( DaoProcess *self )
{
	return self->stackValues[0];
}

#define IntegerOperand( i ) locVars[i]->xInteger.value
#define FloatOperand( i )   locVars[i]->xFloat.value
#define DoubleOperand( i )  locVars[i]->xDouble.value
#define ComplexOperand( i ) locVars[i]->xComplex.value

#define NumberOperand( x, v, i ) \
x = 0; \
v = locVars[i]; \
switch( v->type ){ \
case DAO_INTEGER : x = v->xInteger.value; break; \
case DAO_FLOAT   : x = v->xFloat.value; break; \
case DAO_DOUBLE  : x = v->xDouble.value; break; \
}

#define ArrayArrayValue( array, up, id ) array->items.pArray[ up ]->items.pValue[ id ]

int DaoProcess_CheckFE( DaoProcess *self );

//void DaoContext_AdjustCodes( DaoContext *self, int options );

int DaoMoveAC( DaoProcess *self, DaoValue *A, DaoValue **C, DaoType *t );

#if defined( __GNUC__ ) && !defined( __STRICT_ANSI__ )
#define HAS_VARLABEL
#endif
#if 0
#endif

int DaoProcess_Execute( DaoProcess *self )
{
	DaoStackFrame *rollback;
	DaoUserHandler *handler = self->vmSpace->userHandler;
	DaoVmSpace *vmSpace = self->vmSpace;
	DaoVmCode *vmc=NULL;
	DaoVmCode *vmcBase;
	DaoStackFrame *topFrame;
	DaoRoutine *routine;
	DaoNamespace *here = NULL;
	DaoClass *host = NULL;
	DaoClass *klass = NULL;
	DaoObject *this = NULL;
	DaoObject *object = NULL;
	DaoArray *array;
	DArray   *dataCL[2] = { NULL, NULL };
	DArray   *dataCK = NULL;
	DArray   *dataCG = NULL;
	DaoValue  **dataVL = NULL;
	DaoValue  **dataVO = NULL;
	DArray   *dataVK = NULL;
	DArray   *dataVG = NULL;
	DArray   *typeVL = NULL;
	DArray   *typeVO = NULL;
	DArray   *typeVK = NULL;
	DArray   *typeVG = NULL;
	DaoValue *value, *vA, *vB, *vC = NULL;
	DaoValue **vA2, **vB2, **vC2 = NULL;
	DaoValue **vref;
	DaoValue **locVars;
	DaoType **locTypes;
	DaoType *abtp;
	DaoTuple *tuple;
	DaoList *list;
	DString *str;
	complex16 com = {0,0};
	size_t size, *dims, *dmac;
	int invokehost = handler && handler->InvokeHost;
	int i, j, print, retCode;
	int exceptCount = 0;
	int gotoCount = 0;
	dint id;
	dint inum=0;
	float fnum=0;
	double AA, BB, dnum=0;
	long_t lnum = 0;
	ushort_t *range;
	complex16 acom, bcom;
	DaoStackFrame *base;

#ifdef HAS_VARLABEL
	static void *labels[] = {
		&& LAB_NOP ,
		&& LAB_DATA ,
		&& LAB_GETCL , && LAB_GETCK , && LAB_GETCG ,
		&& LAB_GETVL , && LAB_GETVO , && LAB_GETVK , && LAB_GETVG ,
		&& LAB_GETI  , && LAB_GETMI , && LAB_GETF  , && LAB_GETMF ,
		&& LAB_SETVL , && LAB_SETVO , && LAB_SETVK , && LAB_SETVG ,
		&& LAB_SETI  , && LAB_SETMI , && LAB_SETF , && LAB_SETMF ,
		&& LAB_LOAD  , && LAB_CAST , && LAB_MOVE ,
		&& LAB_NOT , && LAB_UNMS , && LAB_BITREV ,
		&& LAB_ADD , && LAB_SUB ,
		&& LAB_MUL , && LAB_DIV ,
		&& LAB_MOD , && LAB_POW ,
		&& LAB_AND , && LAB_OR ,
		&& LAB_LT , && LAB_LE ,
		&& LAB_EQ , && LAB_NE , && LAB_IN ,
		&& LAB_BITAND , && LAB_BITOR ,
		&& LAB_BITXOR , && LAB_BITLFT ,
		&& LAB_BITRIT , && LAB_CHECK ,
		&& LAB_NAMEVA , && LAB_PAIR ,
		&& LAB_TUPLE  , && LAB_LIST ,
		&& LAB_MAP    , && LAB_HASH ,
		&& LAB_ARRAY  , && LAB_MATRIX ,
		&& LAB_CURRY  , && LAB_MCURRY ,
		&& LAB_ROUTINE , && LAB_CLASS ,
		&& LAB_GOTO ,
		&& LAB_SWITCH , && LAB_CASE ,
		&& LAB_ITER , && LAB_TEST ,
		&& LAB_MATH , && LAB_FUNCT ,
		&& LAB_CALL , && LAB_MCALL ,
		&& LAB_CRRE ,
		&& LAB_JITC ,
		&& LAB_RETURN ,
		&& LAB_YIELD ,
		&& LAB_DEBUG ,
		&& LAB_SECT ,

		&& LAB_DATA_I , && LAB_DATA_F , && LAB_DATA_D ,
		&& LAB_GETCL_I , && LAB_GETCL_F , && LAB_GETCL_D ,
		&& LAB_GETCK_I , && LAB_GETCK_F , && LAB_GETCK_D ,
		&& LAB_GETCG_I , && LAB_GETCG_F , && LAB_GETCG_D ,
		&& LAB_GETVL_I , && LAB_GETVL_F , && LAB_GETVL_D ,
		&& LAB_GETVO_I , && LAB_GETVO_F , && LAB_GETVO_D ,
		&& LAB_GETVK_I , && LAB_GETVK_F , && LAB_GETVK_D ,
		&& LAB_GETVG_I , && LAB_GETVG_F , && LAB_GETVG_D ,
		&& LAB_SETVL_II , && LAB_SETVL_IF , && LAB_SETVL_ID ,
		&& LAB_SETVL_FI , && LAB_SETVL_FF , && LAB_SETVL_FD ,
		&& LAB_SETVL_DI , && LAB_SETVL_DF , && LAB_SETVL_DD ,
		&& LAB_SETVO_II , && LAB_SETVO_IF , && LAB_SETVO_ID ,
		&& LAB_SETVO_FI , && LAB_SETVO_FF , && LAB_SETVO_FD ,
		&& LAB_SETVO_DI , && LAB_SETVO_DF , && LAB_SETVO_DD ,
		&& LAB_SETVK_II , && LAB_SETVK_IF , && LAB_SETVK_ID ,
		&& LAB_SETVK_FI , && LAB_SETVK_FF , && LAB_SETVK_FD ,
		&& LAB_SETVK_DI , && LAB_SETVK_DF , && LAB_SETVK_DD ,
		&& LAB_SETVG_II , && LAB_SETVG_IF , && LAB_SETVG_ID ,
		&& LAB_SETVG_FI , && LAB_SETVG_FF , && LAB_SETVG_FD ,
		&& LAB_SETVG_DI , && LAB_SETVG_DF , && LAB_SETVG_DD ,

		&& LAB_MOVE_II , && LAB_MOVE_IF , && LAB_MOVE_ID ,
		&& LAB_MOVE_FI , && LAB_MOVE_FF , && LAB_MOVE_FD ,
		&& LAB_MOVE_DI , && LAB_MOVE_DF , && LAB_MOVE_DD ,
		&& LAB_MOVE_CC , && LAB_MOVE_SS , && LAB_MOVE_PP ,
		&& LAB_NOT_I , && LAB_NOT_F , && LAB_NOT_D ,
		&& LAB_UNMS_I , && LAB_UNMS_F , && LAB_UNMS_D ,
		&& LAB_BITREV_I , && LAB_BITREV_F , && LAB_BITREV_D ,
		&& LAB_UNMS_C ,

		&& LAB_ADD_III , && LAB_SUB_III ,
		&& LAB_MUL_III , && LAB_DIV_III ,
		&& LAB_MOD_III , && LAB_POW_III ,
		&& LAB_AND_III , && LAB_OR_III ,
		&& LAB_LT_III , && LAB_LE_III ,
		&& LAB_EQ_III , && LAB_NE_III ,
		&& LAB_BITAND_III , && LAB_BITOR_III ,
		&& LAB_BITXOR_III , && LAB_BITLFT_III ,
		&& LAB_BITRIT_III ,

		&& LAB_ADD_FFF , && LAB_SUB_FFF ,
		&& LAB_MUL_FFF , && LAB_DIV_FFF ,
		&& LAB_MOD_FFF , && LAB_POW_FFF ,
		&& LAB_AND_FFF , && LAB_OR_FFF ,
		&& LAB_LT_FFF , && LAB_LE_FFF ,
		&& LAB_EQ_FFF , && LAB_NE_FFF ,
		&& LAB_BITAND_FFF , && LAB_BITOR_FFF ,
		&& LAB_BITXOR_FFF , && LAB_BITLFT_FFF ,
		&& LAB_BITRIT_FFF ,

		&& LAB_ADD_DDD , && LAB_SUB_DDD ,
		&& LAB_MUL_DDD , && LAB_DIV_DDD ,
		&& LAB_MOD_DDD , && LAB_POW_DDD ,
		&& LAB_AND_DDD , && LAB_OR_DDD ,
		&& LAB_LT_DDD , && LAB_LE_DDD ,
		&& LAB_EQ_DDD , && LAB_NE_DDD ,
		&& LAB_BITAND_DDD , && LAB_BITOR_DDD ,
		&& LAB_BITXOR_DDD , && LAB_BITLFT_DDD ,
		&& LAB_BITRIT_DDD ,

		&& LAB_ADD_FNN , && LAB_SUB_FNN ,
		&& LAB_MUL_FNN , && LAB_DIV_FNN ,
		&& LAB_MOD_FNN , && LAB_POW_FNN ,
		&& LAB_AND_FNN , && LAB_OR_FNN ,
		&& LAB_LT_FNN , && LAB_LE_FNN ,
		&& LAB_EQ_FNN , && LAB_NE_FNN ,
		&& LAB_BITLFT_FNN , && LAB_BITRIT_FNN ,

		&& LAB_ADD_DNN , && LAB_SUB_DNN ,
		&& LAB_MUL_DNN , && LAB_DIV_DNN ,
		&& LAB_MOD_DNN , && LAB_POW_DNN ,
		&& LAB_AND_DNN , && LAB_OR_DNN ,
		&& LAB_LT_DNN , && LAB_LE_DNN ,
		&& LAB_EQ_DNN , && LAB_NE_DNN ,
		&& LAB_BITLFT_DNN , && LAB_BITRIT_DNN ,

		&& LAB_ADD_SS ,
		&& LAB_LT_SS , && LAB_LE_SS ,
		&& LAB_EQ_SS , && LAB_NE_SS ,

		&& LAB_GETI_LI , && LAB_SETI_LI , && LAB_GETI_SI ,
		&& LAB_SETI_SII , && LAB_GETI_LII ,
		&& LAB_GETI_LFI , && LAB_GETI_LDI ,
		&& LAB_GETI_LSI ,
		&& LAB_SETI_LIII , && LAB_SETI_LIIF , && LAB_SETI_LIID ,
		&& LAB_SETI_LFII , && LAB_SETI_LFIF , && LAB_SETI_LFID ,
		&& LAB_SETI_LDII , && LAB_SETI_LDIF , && LAB_SETI_LDID ,
		&& LAB_SETI_LSIS ,
		&& LAB_GETI_AII , && LAB_GETI_AFI , && LAB_GETI_ADI ,
		&& LAB_SETI_AIII , && LAB_SETI_AIIF , && LAB_SETI_AIID ,
		&& LAB_SETI_AFII , && LAB_SETI_AFIF , && LAB_SETI_AFID ,
		&& LAB_SETI_ADII , && LAB_SETI_ADIF , && LAB_SETI_ADID ,

		&& LAB_GETI_TI , && LAB_SETI_TI ,

		&& LAB_GETF_T ,
		&& LAB_GETF_TI , && LAB_GETF_TF ,
		&& LAB_GETF_TD , && LAB_GETF_TS ,
		&& LAB_SETF_T ,
		&& LAB_SETF_TII , && LAB_SETF_TIF , && LAB_SETF_TID ,
		&& LAB_SETF_TFI , && LAB_SETF_TFF , && LAB_SETF_TFD ,
		&& LAB_SETF_TDI , && LAB_SETF_TDF , && LAB_SETF_TDD ,
		&& LAB_SETF_TSS ,

		&& LAB_ADD_CC , && LAB_SUB_CC ,
		&& LAB_MUL_CC , && LAB_DIV_CC ,
		&& LAB_GETI_ACI , && LAB_SETI_ACI ,

		&& LAB_GETI_AM , && LAB_SETI_AM ,

		&& LAB_GETF_KC , && LAB_GETF_KG ,
		&& LAB_GETF_OC , && LAB_GETF_OG , && LAB_GETF_OV ,
		&& LAB_SETF_KG , && LAB_SETF_OG , && LAB_SETF_OV ,

		&& LAB_GETF_KCI , && LAB_GETF_KCF , && LAB_GETF_KCD , 
		&& LAB_GETF_KGI , && LAB_GETF_KGF , && LAB_GETF_KGD ,
		&& LAB_GETF_OCI , && LAB_GETF_OCF , && LAB_GETF_OCD , 
		&& LAB_GETF_OGI , && LAB_GETF_OGF , && LAB_GETF_OGD , 
		&& LAB_GETF_OVI , && LAB_GETF_OVF , && LAB_GETF_OVD ,

		&& LAB_SETF_KGII , && LAB_SETF_KGIF , && LAB_SETF_KGID , 
		&& LAB_SETF_KGFI , && LAB_SETF_KGFF , && LAB_SETF_KGFD , 
		&& LAB_SETF_KGDI , && LAB_SETF_KGDF , && LAB_SETF_KGDD , 
		&& LAB_SETF_OGII , && LAB_SETF_OGIF , && LAB_SETF_OGID , 
		&& LAB_SETF_OGFI , && LAB_SETF_OGFF , && LAB_SETF_OGFD , 
		&& LAB_SETF_OGDI , && LAB_SETF_OGDF , && LAB_SETF_OGDD , 
		&& LAB_SETF_OVII , && LAB_SETF_OVIF , && LAB_SETF_OVID ,
		&& LAB_SETF_OVFI , && LAB_SETF_OVFF , && LAB_SETF_OVFD ,
		&& LAB_SETF_OVDI , && LAB_SETF_OVDF , && LAB_SETF_OVDD ,

		&& LAB_TEST_I , && LAB_TEST_F , && LAB_TEST_D ,

		&& LAB_SAFE_GOTO
	};
#endif

#ifdef HAS_VARLABEL

#define OPBEGIN() goto *labels[ vmc->code ];
#define OPCASE( name ) LAB_##name :
#define OPNEXT() goto *labels[ (++vmc)->code ];
#define OPJUMP() goto *labels[ vmc->code ];
#define OPDEFAULT()
#define OPEND()

#else

#if defined( __GNUC__ ) && !defined( __STRICT_ANSI__ )
#warning "=========================================="
#warning "=========================================="
#warning "  NOT USING DIRECT THREADING"
#warning "=========================================="
#warning "=========================================="
#endif

#define OPBEGIN() for(;;){ switch( vmc->code )
#define OPCASE( name ) case DVM_##name :
#define OPNEXT() break;
#define OPJUMP() continue;
#define OPDEFAULT() default:
#define OPEND() vmc++; }

#if 0
#define OPBEGIN() for(;;){ printf("%3i:", (i=vmc-vmcBase) ); DaoVmCodeX_Print( *topFrame->routine->annotCodes->items.pVmc[i], NULL ); switch( vmc->code )
#endif

#endif


	if( self->topFrame == NULL || self->topFrame == self->firstFrame ) goto ReturnFalse;
	rollback = self->topFrame->prev;
	base = self->topFrame;
	if( self->status == DAO_VMPROC_SUSPENDED ) base = self->firstFrame->next;

CallEntry:

	/*
	   printf( "stack size = %s %i %i\n", getenv("PROC_NAME"), self->stackContext->size, base );
	 */
	if( self->topFrame == NULL || self->topFrame == base->prev ){
		self->status = DAO_VMPROC_FINISHED;
		if( self->exceptions->size > 0 ) goto FinishProc;
		/*if( eventHandler ) eventHandler->mainRoutineExit(); */
		goto ReturnTrue;
	}
	topFrame = self->topFrame;
	if( topFrame->function ){
		DaoValue **p = self->stackValues + topFrame->stackBase;
		DaoProcess_CallFunction( self, topFrame->function, p, topFrame->parCount );
		DaoProcess_PopFrame( self );
		goto CallEntry;
	}
	routine = topFrame->routine;
#if 0
	if( (vmSpace->options & DAO_EXEC_SAFE) && self->topFrame->index >= 100 ){
		DaoProcess_RaiseException( self, DAO_ERROR,
				"too deep recursion for safe running mode." );
		goto FinishProc;
	}
#endif

	//XXX dao_fe_clear();
	//topCtx->idClearFE = self->topFrame->entry;

#if 0
	if( ROUT_HOST_TID( routine ) == DAO_OBJECT )
		printf("class name = %s\n", routine->routHost->aux->xClass.className->mbs);
	printf("routine name = %s\n", routine->routName->mbs);
	//printf("entry code = %i\n", DArrayS4_Top( self->stackStates )[S4_ENTRY] );
	printf("number of instruction: %i\n", routine->vmCodes->size );
	if( routine->routType ) printf("routine type = %s\n", routine->routType->name->mbs);
	printf( "vmSpace = %p; nameSpace = %p\n", self->vmSpace, topCtx->nameSpace );
	printf("routine = %p; context = %p\n", routine, topCtx );
	printf( "self object = %p\n", topCtx->object );
#endif

	if( self->stopit | vmSpace->stopit ) goto FinishProc;
	//XXX if( invokehost ) handler->InvokeHost( handler, topCtx );

	//XXX if( (vmSpace->options & DAO_EXEC_DEBUG)|(routine->mode & DAO_EXEC_DEBUG) )
	//XXX	DaoContext_AdjustCodes( topCtx, vmSpace->options );

	vmcBase = topFrame->codes;
	id = self->topFrame->entry;
	if( id >= routine->vmCodes->size ){
		if( id == 0 ){
			DString_SetMBS( self->mbstring, "Not implemented function, " );
			DString_Append( self->mbstring, routine->routName );
			DString_AppendMBS( self->mbstring, "()" );
			DaoProcess_RaiseException( self, DAO_ERROR, self->mbstring->mbs );
			goto FinishProc;
		}
		goto FinishCall;
	}

#if 0
	printf("==================VM==============================\n");
	printf("entry code = %i\n", DArrayS4_Top( self->stackStates )[S4_ENTRY] );
	printf("number of register: %i\n", topCtx->regArray->size );
	printf("number of register: %i\n", routine->regCount );
	printf("number of instruction: %i\n", routine->vmCodes->size );
	printf( "VM process: %p\n", self );
	printf("==================================================\n");
	DaoRoutine_PrintCode( routine, self->vmSpace->stdStream );
#endif

	vmc = vmcBase + id;
	self->stopit = 0;
	self->activeCode = vmc;
	self->activeRoutine = routine;
	self->activeObject = topFrame->object;
	self->activeValues = self->stackValues + topFrame->stackBase;
	self->activeTypes = routine->regType->items.pType;
	self->activeNamespace = routine->nameSpace;

	/* range ( 0, routine->vmCodes->size-1 ) */
	if( id ==0 ) DaoStackFrame_PushRange( topFrame, 0, (routine->vmCodes->size-1) );

	exceptCount = self->exceptions->size;
	/* Check if an exception has been raisen by a function call: */
	if( self->exceptions->size ){ /* yes */
		if( topFrame->depth == 0 ) goto FinishCall; /* should never happen */
		/* jump to the proper CRRE instruction to handle the exception,
		 * or jump to the last RETURN instruction to defer the handling to
		 * its caller. */
		topFrame->depth --;
		vmc = vmcBase + topFrame->ranges[ topFrame->depth ][1];
	}
	if( self->status == DAO_VMPROC_SUSPENDED &&
			( vmc->code ==DVM_CALL || vmc->code ==DVM_MCALL || vmc->code ==DVM_YIELD ) ){
		if( self->parResume && self->pauseType != DAO_VMP_ASYNC ){
			DaoList *list = DaoProcess_GetList( self, vmc );
			for(i=0; i<self->parResume->size; i++)
				DaoList_Append( list, self->parResume->items.pValue[i] );
		}else if( self->pauseType == DAO_VMP_ASYNC && self->future->precondition ){
			DaoProcess_PutValue( self, self->future->precondition->value );
		}
		vmc ++;
	}
	self->status = DAO_VMPROC_RUNNING;
	self->pauseType = DAO_VMP_NOPAUSE;
	host = NULL;
	here = routine->nameSpace;
	this = topFrame->object;
	locVars = self->activeValues;
	locTypes = self->activeTypes;
	dataCL[0] = routine->routConsts;
	dataCG = here->cstDataTable;
	dataVG = here->varDataTable;
	typeVG = here->varTypeTable;
	if( ROUT_HOST_TID( routine ) == DAO_OBJECT ){
		host = & routine->routHost->aux->xClass;
		dataCK = host->cstDataTable;
		dataVK = host->glbDataTable;
		typeVK = host->glbTypeTable;
		if( !(routine->attribs & DAO_ROUT_STATIC) ){
			dataVO = this->objValues;
			typeVO = host->objDataType;
		}
	}
	if( routine->upRoutine ){
		dataCL[1] = routine->upRoutine->routConsts;
		dataVL = routine->upContext->stackValues + 1;
		typeVL = routine->upRoutine->regType;
	}

	OPBEGIN(){
		OPCASE( NOP ){
			if( self->stopit | vmSpace->stopit ) goto FinishProc;
		}OPNEXT() OPCASE( DATA ){
			//if( locVars[ vmc->c ] && locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			switch( vmc->a ){
			case DAO_COMPLEX :
				ComplexOperand( vmc->c ).real = 0.0;
				ComplexOperand( vmc->c ).imag = vmc->b;
				break;
			case DAO_NULL :
				GC_ShiftRC( null, locVars[ vmc->c ] );
				locVars[ vmc->c ] = null;
				break;
			case DAO_INTEGER : IntegerOperand( vmc->c ) = vmc->b; break;
			case DAO_FLOAT  : FloatOperand( vmc->c ) = vmc->b; break;
			case DAO_DOUBLE : DoubleOperand( vmc->c ) = vmc->b; break;
			default : break;
			}
		}OPNEXT() OPCASE( GETCL ){
			/* All GETX instructions assume the C regisgter is an intermediate register! */
			value = dataCL[ vmc->a ]->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETCK ){
			value = dataCK->items.pArray[ vmc->a ]->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETCG ){
			value = dataCG->items.pArray[ vmc->a ]->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETVL ){
			GC_ShiftRC( dataVL[ vmc->b ], locVars[ vmc->c ] );
			locVars[ vmc->c ] = dataVL[ vmc->b ];
		}OPNEXT() OPCASE( GETVO ){
			GC_ShiftRC( dataVO[ vmc->b ], locVars[ vmc->c ] );
			locVars[ vmc->c ] = dataVO[ vmc->b ];
		}OPNEXT() OPCASE( GETVK ){
			value = dataVK->items.pArray[vmc->a]->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETVG ){
			value = dataVG->items.pArray[vmc->a]->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETI ) OPCASE( GETMI ){
			DaoProcess_DoGetItem( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( GETF ){
			DaoProcess_DoGetField( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( GETMF ){
			DaoProcess_DoGetMetaField( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( SETVL ){
			abtp = typeVL->items.pType[ vmc->b ];
			if( DaoMoveAC( self, locVars[vmc->a], dataVL + vmc->b, abtp ) ==0 )
				goto CheckException;
		}OPNEXT() OPCASE( SETVO ){
			abtp = typeVO->items.pType[ vmc->b ];
			if( DaoMoveAC( self, locVars[vmc->a], dataVO + vmc->b, abtp ) ==0 )
				goto CheckException;
		}OPNEXT() OPCASE( SETVK ){
			abtp = typeVK->items.pArray[ vmc->c ]->items.pType[ vmc->b ];
			vref = dataVK->items.pArray[vmc->c]->items.pValue + vmc->b;
			if( DaoMoveAC( self, locVars[vmc->a], vref, abtp ) ==0 ) goto CheckException;
		}OPNEXT() OPCASE( SETVG ){
			abtp = typeVG->items.pArray[ vmc->c ]->items.pType[ vmc->b ];
			vref = dataVG->items.pArray[vmc->c]->items.pValue + vmc->b;
			if( DaoMoveAC( self, locVars[vmc->a], vref, abtp ) ==0 ) goto CheckException;
		}OPNEXT() OPCASE( SETI ) OPCASE( SETMI ){
			if( locVars[ vmc->c ] && (locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST) )
				goto ModifyConstant;
			DaoProcess_DoSetItem( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( SETF ){
			// class::static_member = XXX
			//if( locVars[ vmc->c ] && locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			DaoProcess_DoSetField( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( SETMF ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			DaoProcess_DoSetMetaField( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( LOAD ){
			if( locVars[ vmc->a ] && (locVars[ vmc->a ]->xNull.trait & DAO_DATA_CONST) == 0 ){
				GC_ShiftRC( locVars[ vmc->a ], locVars[ vmc->c ] );
				locVars[ vmc->c ] = locVars[ vmc->a ];
			}else{
				DaoValue_Copy( locVars[ vmc->a ], & locVars[ vmc->c ] );
			}
		}OPNEXT() OPCASE( CAST ){
			//if( locVars[ vmc->c ] && (locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST) )
			//	goto ModifyConstant;
			self->activeCode = vmc;
			DaoProcess_DoCast( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( MOVE ){
			self->activeCode = vmc;
			DaoProcess_DoMove( self, vmc );
			goto CheckException;
		}OPNEXT()
		OPCASE( ADD )
			OPCASE( SUB )
			OPCASE( MUL )
			OPCASE( DIV )
			OPCASE( MOD )
			OPCASE( POW ){
				self->activeCode = vmc;
				DaoProcess_DoBinArith( self, vmc );
				goto CheckException;
			}OPNEXT()
		OPCASE( AND )
			OPCASE( OR )
			OPCASE( LT )
			OPCASE( LE )
			OPCASE( EQ )
			OPCASE( NE ){
				self->activeCode = vmc;
				DaoProcess_DoBinBool( self, vmc );
				goto CheckException;
			}OPNEXT()
		OPCASE( IN ){
			self->activeCode = vmc;
			DaoProcess_DoInTest( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( NOT ) OPCASE( UNMS ){
			self->activeCode = vmc;
			DaoProcess_DoUnaArith( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( BITAND ) OPCASE( BITOR ) OPCASE( BITXOR ){
			self->activeCode = vmc;
			DaoProcess_DoBitLogic( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( BITLFT ) OPCASE( BITRIT ){
			self->activeCode = vmc;
			DaoProcess_DoBitShift( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( BITREV ){
			self->activeCode = vmc;
			DaoProcess_DoBitFlip( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( CHECK ){
			DaoProcess_DoCheck( self, vmc );
		}OPNEXT() OPCASE( NAMEVA ){
			DaoProcess_BindNameValue( self, vmc );
		}OPNEXT() OPCASE( PAIR ){
			self->activeCode = vmc;
			DaoProcess_DoPair( self, vmc );
		}OPNEXT() OPCASE( TUPLE ){
			self->activeCode = vmc;
			DaoProcess_DoTuple( self, vmc );
		}OPNEXT() OPCASE( LIST ){
			self->activeCode = vmc;
			DaoProcess_DoList( self, vmc );
		}OPNEXT() OPCASE( MAP ) OPCASE( HASH ){
			self->activeCode = vmc;
			DaoProcess_DoMap( self, vmc );
		}OPNEXT() OPCASE( ARRAY ){
			DaoProcess_DoArray( self, vmc );
		}OPNEXT() OPCASE( MATRIX ){
			DaoProcess_DoMatrix( self, vmc );
		}OPNEXT() OPCASE( CURRY ) OPCASE( MCURRY ){
			DaoProcess_DoCurry( self, vmc );
		}OPNEXT() OPCASE( CASE ) OPCASE( GOTO ){
			vmc = vmcBase + vmc->b;
		}OPJUMP() OPCASE( SWITCH ){
			vmc = DaoProcess_DoSwitch( self, vmc );
		}OPJUMP() OPCASE( ITER ){
			self->activeCode = vmc;
			DaoProcess_DoIter( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( TEST ){
			vA = locVars[ vmc->a ];
			switch( vA->type ){
			case DAO_NULL :
				vmc = vmcBase + vmc->b; break;
			case DAO_INTEGER :
				vmc = vA->xInteger.value ? vmc+1 : vmcBase + vmc->b; break;
			case DAO_FLOAT   :
				vmc = vA->xFloat.value ? vmc+1 : vmcBase + vmc->b; break;
			case DAO_DOUBLE  :
				vmc = vA->xDouble.value ? vmc+1 : vmcBase + vmc->b; break;
			case DAO_COMPLEX :
				vmc = (vA->xComplex.value.real || vA->xComplex.value.imag) ? vmc+1 : vmcBase + vmc->b;
				break;
			case DAO_LONG :
				j = vA->xLong.value->size >1 || (vA->xLong.value->size ==1 && vA->xLong.value->data[0]);
				vmc = j ? vmc+1 : vmcBase + vmc->b;
				break;
			case DAO_ENUM  :
				vmc = vA->xEnum.value ? vmc+1 : vmcBase + vmc->b;
				break;
			case DAO_STRING  :
				vmc = vA->xString.data->size ? vmc+1 : vmcBase + vmc->b; break;
			case DAO_TUPLE :
				if( vA->xTuple.unitype == dao_type_for_iterator && vA->xTuple.items[0]->xInteger.value ){
					vmc = vmc + 1;
				}else{
					vmc = vmcBase + vmc->b;
				}
				break;
			default :
				switch( vA->type ){
				case DAO_LIST  :
					vmc = vA->xList.items->size ? vmc+1 : vmcBase + vmc->b;
					break;
				case DAO_MAP   :
					vmc = vA->xMap.items->size ? vmc+1 : vmcBase + vmc->b;
					break;
#ifdef DAO_WITH_NUMARRAY
				case DAO_ARRAY :
					vmc = vA->xArray.size ? vmc+1 : vmcBase + vmc->b;
					break;
#endif
				case DAO_TUPLE :
					vmc = vA->xTuple.size ? vmc+1 : vmcBase + vmc->b;
					break;
				case DAO_CTYPE :
				case DAO_CDATA :
					vmc = vA->xCdata.data ? vmc+1 : vmcBase + vmc->b;
					break;
				default :
					vmc = vmcBase + vmc->b;
					break;
				}
				break;
			}
		}OPJUMP() OPCASE( MATH ){
			if( DaoVM_DoMath( self, vmc, locVars[ vmc->c ], locVars[vmc->b] ) )
				goto RaiseErrorInvalidOperation;
		}OPNEXT() OPCASE( FUNCT ){
			self->activeCode = vmc;
			if( self->stopit | vmSpace->stopit ) goto FinishProc;
			DaoProcess_DoFunctional( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( CALL ) OPCASE( MCALL ){
			self->activeCode = vmc;
			if( self->stopit | vmSpace->stopit ) goto FinishProc;
			DaoProcess_DoCall( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( ROUTINE ){
			self->activeCode = vmc;
			DaoProcess_MakeRoutine( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( CLASS ){
			self->activeCode = vmc;
			DaoProcess_MakeClass( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( CRRE ){
			DaoProcess_CheckFE( self );
			exceptCount = self->exceptions->size;
			self->activeCode = vmc;
			size = (size_t)(vmc - vmcBase);
			range = topFrame->ranges[ topFrame->depth-1 ];
			if( vmc->b == 0 ){ /* check exception: */
				int exceptFrom = range[0];
				/* remove a pair of exception scope, when it becomes invalid: */
				if( size <= exceptFrom && topFrame->depth >0 ) topFrame->depth --;
				if( DaoProcess_DoCheckExcept( self, vmc ) ){
					/* exception has happened before, jump to the proper handling point: */
					if( topFrame->depth ==0 ) goto FinishCall;
					vmc = vmcBase + range[1];
					topFrame->depth --;
					OPJUMP()
				}else if( vmc->c > 0 ){
					/* add exception scope for: try{ ... } */
					if( topFrame->depth < DVM_MAX_TRY_DEPTH )
						DaoStackFrame_PushRange( topFrame, size, vmc->c );
					else
						printf( "too many nested try{} statements\n" );
				}else if( topFrame->depth >0 && size >= range[1] ){
					/* remove a pair of exception scope, when it becomes invalid: */
					topFrame->depth --;
				}
			}else if( vmc->c == 0 ){
				self->activeCode = vmc;
				DaoProcess_DoRaiseExcept( self, vmc );
				goto CheckException;
			}else{
				retCode = DaoProcess_DoRescueExcept( self, vmc );
				exceptCount = self->exceptions->size;
				/* remove a pair of exception scope, when it becomes invalid: */
				if( topFrame->depth >0 && size >= range[1] ) topFrame->depth --;
				if( retCode == 0 ){
					vmc = vmcBase + vmc->c;
					OPJUMP()
				}
			}
		}OPNEXT() OPCASE( JITC ){
			dao_jit.Execute( self, vmc->a );
			if( self->exceptions->size > exceptCount ) goto CheckException;
			vmc += vmc->b;
			OPJUMP()
				/*
				   dbase = (DaoValue*)inum;
				   printf( "jitc: %#x, %i\n", inum, dbase->type );
				 */
		}OPNEXT() OPCASE( RETURN ){
			self->activeCode = vmc;
			DaoProcess_DoReturn( self, vmc );
			//XXX DaoProcess_CheckFE( self );
			if( self->stopit | vmSpace->stopit ) goto FinishProc;
			goto FinishCall;
		}OPNEXT() OPCASE( YIELD ){
			self->activeCode = vmc;
			self->status = DAO_VMPROC_SUSPENDED;
			self->pauseType = DAO_VMP_YIELD;
			self->topFrame->entry = (short)(vmc - vmcBase);
			goto CheckException;
		}OPCASE( DEBUG ){
			if( self->stopit | vmSpace->stopit ) goto FinishProc;
			if( (vmSpace->options & DAO_EXEC_DEBUG ) ){
				self->activeCode = vmc;
				if( handler && handler->StdlibDebug ) handler->StdlibDebug( handler, self );
				goto CheckException;
			}
		}OPNEXT() OPCASE( SECT ){
			goto ReturnFalse;
		}OPNEXT() OPCASE( DATA_I ){
			locVars[ vmc->c ]->xInteger.value = vmc->b;
		}OPNEXT() OPCASE( DATA_F ){
			locVars[ vmc->c ]->xFloat.value = vmc->b;
		}OPNEXT() OPCASE( DATA_D ){
			locVars[ vmc->c ]->xDouble.value = vmc->b;
		}OPNEXT() OPCASE( GETCL_I ){
			value = dataCL[ vmc->a ]->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xInteger.value = value->xInteger.value;
		}OPNEXT() OPCASE( GETCL_F ){
			value = dataCL[ vmc->a ]->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xFloat.value = value->xFloat.value;
		}OPNEXT() OPCASE( GETCL_D ){
			value = dataCL[ vmc->a ]->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xDouble.value = value->xDouble.value;
		}OPNEXT() OPCASE( GETCK_I ){
			value = dataCK->items.pArray[ vmc->a ]->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xInteger.value = value->xInteger.value;
		}OPNEXT() OPCASE( GETCK_F ){
			value = dataCK->items.pArray[ vmc->a ]->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xFloat.value = value->xFloat.value;
		}OPNEXT() OPCASE( GETCK_D ){
			value = dataCK->items.pArray[ vmc->a ]->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xDouble.value = value->xDouble.value;
		}OPNEXT() OPCASE( GETCG_I ){
			value = dataCG->items.pArray[ vmc->a ]->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xInteger.value = value->xInteger.value;
		}OPNEXT() OPCASE( GETCG_F ){
			value = dataCG->items.pArray[ vmc->a ]->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xFloat.value = value->xFloat.value;
		}OPNEXT() OPCASE( GETCG_D ){
			value = dataCG->items.pArray[ vmc->a ]->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xDouble.value = value->xDouble.value;
		}OPNEXT() OPCASE( GETVL_I ){
			locVars[ vmc->c ]->xInteger.value = dataVL[ vmc->b ]->xInteger.value;
		}OPNEXT() OPCASE( GETVL_F ){
			locVars[ vmc->c ]->xFloat.value = dataVL[ vmc->b ]->xFloat.value;
		}OPNEXT() OPCASE( GETVL_D ){
			locVars[ vmc->c ]->xDouble.value = dataVL[ vmc->b ]->xDouble.value;
		}OPNEXT() OPCASE( GETVO_I ){
			locVars[ vmc->c ]->xInteger.value = dataVO[ vmc->b ]->xInteger.value;
		}OPNEXT() OPCASE( GETVO_F ){
			locVars[ vmc->c ]->xFloat.value = dataVO[ vmc->b ]->xFloat.value;
		}OPNEXT() OPCASE( GETVO_D ){
			locVars[ vmc->c ]->xDouble.value = dataVO[ vmc->b ]->xDouble.value;
		}OPNEXT() OPCASE( GETVK_I ){
			IntegerOperand( vmc->c ) = ArrayArrayValue( dataVK, vmc->a, vmc->b )->xInteger.value;
		}OPNEXT() OPCASE( GETVK_F ){
			FloatOperand( vmc->c ) = ArrayArrayValue( dataVK, vmc->a, vmc->b )->xFloat.value;
		}OPNEXT() OPCASE( GETVK_D ){
			DoubleOperand( vmc->c ) = ArrayArrayValue( dataVK, vmc->a, vmc->b )->xDouble.value;
		}OPNEXT() OPCASE( GETVG_I ){
			IntegerOperand( vmc->c ) = ArrayArrayValue( dataVG, vmc->a, vmc->b )->xInteger.value;
		}OPNEXT() OPCASE( GETVG_F ){
			FloatOperand( vmc->c ) = ArrayArrayValue( dataVG, vmc->a, vmc->b )->xFloat.value;
		}OPNEXT() OPCASE( GETVG_D ){
			DoubleOperand( vmc->c ) = ArrayArrayValue( dataVG, vmc->a, vmc->b )->xDouble.value;
		}OPNEXT() OPCASE( SETVL_II ){
			dataVL[ vmc->b ]->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVL_IF ){
			dataVL[ vmc->b ]->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVL_ID ){
			dataVL[ vmc->b ]->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVL_FI ){
			dataVL[ vmc->b ]->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVL_FF ){
			dataVL[ vmc->b ]->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVL_FD ){
			dataVL[ vmc->b ]->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVL_DI ){
			dataVL[ vmc->b ]->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVL_DF ){
			dataVL[ vmc->b ]->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVL_DD ){
			dataVL[ vmc->b ]->xDouble.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVO_II ){
			dataVO[ vmc->b ]->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVO_IF ){
			dataVO[ vmc->b ]->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVO_ID ){
			dataVO[ vmc->b ]->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVO_FI ){
			dataVO[ vmc->b ]->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVO_FF ){
			dataVO[ vmc->b ]->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVO_FD ){
			dataVO[ vmc->b ]->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVO_DI ){
			dataVO[ vmc->b ]->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVO_DF ){
			dataVO[ vmc->b ]->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVO_DD ){
			dataVO[ vmc->b ]->xDouble.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_II ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_IF ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_ID ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_FI ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_FF ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_FD ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_DI ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_DF ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_DD ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xDouble.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_II ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_IF ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_ID ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_FI ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_FF ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_FD ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_DI ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_DF ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_DD ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xDouble.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( MOVE_II ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( ADD_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) + IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( SUB_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) - IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( MUL_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) * IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( DIV_III ){
			inum = IntegerOperand( vmc->b );
			if( inum ==0 ) goto RaiseErrorDivByZero;
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) / inum;
		}OPNEXT() OPCASE( MOD_III ){
			inum = IntegerOperand( vmc->b );
			if( inum ==0 ) goto RaiseErrorDivByZero;
			IntegerOperand( vmc->c )=(dint)IntegerOperand( vmc->a ) % inum;
		}OPNEXT() OPCASE( POW_III ){
			IntegerOperand( vmc->c ) = pow( IntegerOperand( vmc->a ), IntegerOperand( vmc->b ) );
		}OPNEXT() OPCASE( AND_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a )
				? IntegerOperand( vmc->b ) : IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( OR_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a )
				? IntegerOperand( vmc->a ) : IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( NOT_I ){
			IntegerOperand( vmc->c ) = ! IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( UNMS_I ){
			IntegerOperand( vmc->c ) = - IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( LT_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) < IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( LE_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) <= IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( EQ_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) == IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( NE_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) != IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( BITAND_III ){
			IntegerOperand( vmc->c ) = (ulong_t)IntegerOperand( vmc->a ) & (ulong_t)IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( BITOR_III ){
			IntegerOperand( vmc->c ) = (ulong_t)IntegerOperand( vmc->a ) | (ulong_t)IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( BITXOR_III ){
			IntegerOperand( vmc->c ) = (ulong_t)IntegerOperand( vmc->a ) ^ (ulong_t)IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( BITLFT_III ){
			IntegerOperand( vmc->c ) = (ulong_t)IntegerOperand( vmc->a ) << (ulong_t)IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( BITRIT_III ){
			IntegerOperand( vmc->c ) = (ulong_t)IntegerOperand( vmc->a ) >> (ulong_t)IntegerOperand( vmc->b );
		}OPNEXT() OPCASE( BITREV_I ){
			IntegerOperand( vmc->c ) = ~ (ulong_t) IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( MOVE_FF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( ADD_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) + FloatOperand( vmc->b );
		}OPNEXT() OPCASE( SUB_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) - FloatOperand( vmc->b );
		}OPNEXT() OPCASE( MUL_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) * FloatOperand( vmc->b );
		}OPNEXT() OPCASE( DIV_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) / FloatOperand( vmc->b );
		}OPNEXT() OPCASE( MOD_FFF ){
			inum = (dint) FloatOperand( vmc->b );
			if( inum ==0 ) goto RaiseErrorDivByZero;
			FloatOperand( vmc->c ) = (dint)FloatOperand( vmc->a ) % inum;
		}OPNEXT() OPCASE( POW_FFF ){
			FloatOperand( vmc->c ) = powf( FloatOperand( vmc->a ), FloatOperand( vmc->b ) );
		}OPNEXT() OPCASE( AND_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) ? FloatOperand( vmc->b ) : FloatOperand( vmc->a );
		}OPNEXT() OPCASE( OR_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) ? FloatOperand( vmc->a ) : FloatOperand( vmc->b );
		}OPNEXT() OPCASE( NOT_F ){
			FloatOperand( vmc->c ) = ! FloatOperand( vmc->a );
		}OPNEXT() OPCASE( UNMS_F ){
			FloatOperand( vmc->c ) = - FloatOperand( vmc->a );
		}OPNEXT() OPCASE( LT_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) < FloatOperand( vmc->b );
		}OPNEXT() OPCASE( LE_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) <= FloatOperand( vmc->b );
		}OPNEXT() OPCASE( EQ_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) == FloatOperand( vmc->b );
		}OPNEXT() OPCASE( NE_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) != FloatOperand( vmc->b );
		}OPNEXT() OPCASE( BITAND_FFF ){
			FloatOperand( vmc->c ) = (ulong_t)FloatOperand( vmc->a ) & (ulong_t)FloatOperand( vmc->b );
		}OPNEXT() OPCASE( BITOR_FFF ){
			FloatOperand( vmc->c ) = (ulong_t)FloatOperand( vmc->a ) | (ulong_t)FloatOperand( vmc->b );
		}OPNEXT() OPCASE( BITXOR_FFF ){
			FloatOperand( vmc->c ) = (ulong_t)FloatOperand( vmc->a ) ^ (ulong_t)FloatOperand( vmc->b );
		}OPNEXT() OPCASE( BITLFT_FFF ){
			FloatOperand( vmc->c ) = (ulong_t)FloatOperand( vmc->a ) << (ulong_t)FloatOperand( vmc->b );
		}OPNEXT() OPCASE( BITRIT_FFF ){
			FloatOperand( vmc->c ) = (ulong_t)FloatOperand( vmc->a ) >> (ulong_t)FloatOperand( vmc->b );
		}OPNEXT() OPCASE( BITREV_F ){
			FloatOperand( vmc->c ) = ~ (ulong_t) FloatOperand( vmc->a );
		}OPNEXT() OPCASE( MOVE_DD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( ADD_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) + DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( SUB_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) - DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( MUL_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) * DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( DIV_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) / DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( MOD_DDD ){
			lnum = (long_t) DoubleOperand( vmc->b );
			if( lnum ==0 ) goto RaiseErrorDivByZero;
			DoubleOperand( vmc->c )=(long_t)DoubleOperand( vmc->a ) % lnum;
		}OPNEXT() OPCASE( POW_DDD ){
			DoubleOperand( vmc->c ) = pow( DoubleOperand( vmc->a ), DoubleOperand( vmc->b ) );
		}OPNEXT() OPCASE( AND_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) ? DoubleOperand( vmc->b ) : DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( OR_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) ? DoubleOperand( vmc->a ) : DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( NOT_D ){
			DoubleOperand( vmc->c ) = ! DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( UNMS_D ){
			DoubleOperand( vmc->c ) = - DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( LT_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) < DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( LE_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) <= DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( EQ_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) == DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( NE_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) != DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( BITAND_DDD ){
			DoubleOperand( vmc->c ) = (ulong_t)DoubleOperand( vmc->a ) & (ulong_t)DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( BITOR_DDD ){
			DoubleOperand( vmc->c ) = (ulong_t)DoubleOperand( vmc->a ) | (ulong_t)DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( BITXOR_DDD ){
			DoubleOperand( vmc->c ) = ((ulong_t)DoubleOperand( vmc->a )) ^ (ulong_t)DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( BITLFT_DDD ){
			DoubleOperand( vmc->c ) = (ulong_t)DoubleOperand( vmc->a ) << (ulong_t)DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( BITRIT_DDD ){
			DoubleOperand( vmc->c ) = (ulong_t)DoubleOperand( vmc->a ) >> (ulong_t)DoubleOperand( vmc->b );
		}OPNEXT() OPCASE( BITREV_D ){
			DoubleOperand( vmc->c ) = ~ (ulong_t) DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( ADD_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA + BB;
		}OPNEXT() OPCASE( SUB_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA - BB;
		}OPNEXT() OPCASE( MUL_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA * BB;
		}OPNEXT() OPCASE( DIV_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA / BB;
		}OPNEXT() OPCASE( MOD_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = (long_t)AA % (long_t)BB;
		}OPNEXT() OPCASE( POW_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = powf( AA, BB );
		}OPNEXT() OPCASE( AND_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA ? BB : AA;
		}OPNEXT() OPCASE( OR_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA ? AA : BB;
		}OPNEXT() OPCASE( LT_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA < BB;
		}OPNEXT() OPCASE( LE_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA <= BB;
		}OPNEXT() OPCASE( EQ_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA == BB;
		}OPNEXT() OPCASE( NE_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA != BB;
		}OPNEXT() OPCASE( BITLFT_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = (ulong_t)AA << (ulong_t)BB;
		}OPNEXT() OPCASE( BITRIT_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = (ulong_t)AA >> (ulong_t)BB;
		}OPNEXT() OPCASE( ADD_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA + BB;
		}OPNEXT() OPCASE( SUB_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA - BB;
		}OPNEXT() OPCASE( MUL_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA * BB;
		}OPNEXT() OPCASE( DIV_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA / BB;
		}OPNEXT() OPCASE( MOD_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = (long_t)AA % (long_t)BB;
		}OPNEXT() OPCASE( POW_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = pow( AA, BB );
		}OPNEXT() OPCASE( AND_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA ? BB : AA;
		}OPNEXT() OPCASE( OR_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA ? AA : BB;
		}OPNEXT() OPCASE( LT_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA < BB;
		}OPNEXT() OPCASE( LE_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA <= BB;
		}OPNEXT() OPCASE( EQ_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA == BB;
		}OPNEXT() OPCASE( NE_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA != BB;
		}OPNEXT() OPCASE( BITLFT_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = ((ulong_t)AA) << (ulong_t)BB;
		}OPNEXT() OPCASE( BITRIT_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = ((ulong_t)AA) >> (ulong_t)BB;
		}OPNEXT() OPCASE( ADD_SS ){
			vA = locVars[ vmc->a ];  vB = locVars[ vmc->b ];
			vC = locVars[ vmc->c ];
			if( vmc->a == vmc->c ){
				DString_Append( vA->xString.data, vB->xString.data );
			}else if( vmc->b == vmc->c ){
				DString_Insert( vB->xString.data, vA->xString.data, 0, 0, 0 );
			}else if( vC == NULL ){
				vC = locVars[ vmc->c ] = (DaoValue*) DaoString_Copy( & vA->xString );
				GC_IncRC( locVars[ vmc->c ] );
				DString_Append( vC->xString.data, vB->xString.data );
			}else{
				DString_Assign( vC->xString.data, vA->xString.data );
				DString_Append( vC->xString.data, vB->xString.data );
			}
		}OPNEXT() OPCASE( LT_SS ){
			vA = locVars[ vmc->a ];  vB = locVars[ vmc->b ];
			IntegerOperand( vmc->c ) = DString_Compare( vA->xString.data, vB->xString.data ) <0;
		}OPNEXT() OPCASE( LE_SS ){
			vA = locVars[ vmc->a ];  vB = locVars[ vmc->b ];
			IntegerOperand( vmc->c ) = DString_Compare( vA->xString.data, vB->xString.data ) <=0;
		}OPNEXT() OPCASE( EQ_SS ){
			vA = locVars[ vmc->a ];  vB = locVars[ vmc->b ];
			IntegerOperand( vmc->c ) = DString_Compare( vA->xString.data, vB->xString.data ) ==0;
		}OPNEXT() OPCASE( NE_SS ){
			vA = locVars[ vmc->a ];  vB = locVars[ vmc->b ];
			IntegerOperand( vmc->c ) = DString_Compare( vA->xString.data, vB->xString.data ) !=0;
		}OPNEXT() OPCASE( MOVE_IF ){
			IntegerOperand( vmc->c ) = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( MOVE_ID ){
			IntegerOperand( vmc->c ) = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( MOVE_FI ){
			FloatOperand( vmc->c ) = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( MOVE_FD ){
			FloatOperand( vmc->c ) = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( MOVE_DI ){
			DoubleOperand( vmc->c ) = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( MOVE_DF ){
			DoubleOperand( vmc->c ) = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( MOVE_CC ){
			ComplexOperand( vmc->c ) = ComplexOperand( vmc->a );
		}OPNEXT() OPCASE( MOVE_SS ){
			vC = locVars[ vmc->c ];
			if( vC == NULL || vC->type != DAO_STRING || vC->xGC.refCount >1 ){
				value = (DaoValue*) DaoString_Copy( & locVars[ vmc->a ]->xString );
				GC_ShiftRC( value, locVars[ vmc->c ] );
				locVars[ vmc->c ] = value;
			}else{
				DString_Assign( vC->xString.data, locVars[ vmc->a ]->xString.data );
			}
		}OPNEXT() OPCASE( MOVE_PP ){
			DaoValue_Copy( locVars[ vmc->a ], & locVars[ vmc->c ] );
		}OPNEXT() OPCASE( UNMS_C ){
			acom = ComplexOperand( vmc->a );
			vC = locVars[ vmc->c ];
			vC->xComplex.value.real = - acom.real;
			vC->xComplex.value.imag = - acom.imag;
		}OPNEXT() OPCASE( ADD_CC ){
			acom = ComplexOperand( vmc->a );  bcom = ComplexOperand( vmc->b );
			vC = locVars[ vmc->c ];
			vC->xComplex.value.real = acom.real + bcom.real;
			vC->xComplex.value.imag = acom.imag + bcom.imag;
		}OPNEXT() OPCASE( SUB_CC ){
			acom = ComplexOperand( vmc->a );  bcom = ComplexOperand( vmc->b );
			vC = locVars[ vmc->c ];
			vC->xComplex.value.real = acom.real - bcom.real;
			vC->xComplex.value.imag = acom.imag - bcom.imag;
		}OPNEXT() OPCASE( MUL_CC ){
			acom = ComplexOperand( vmc->a );  bcom = ComplexOperand( vmc->b );
			vC = locVars[ vmc->c ];
			vC->xComplex.value.real = acom.real * bcom.real - acom.imag * bcom.imag;
			vC->xComplex.value.imag = acom.real * bcom.imag + acom.imag * bcom.real;
		}OPNEXT() OPCASE( DIV_CC ){
			acom = ComplexOperand( vmc->a );  bcom = ComplexOperand( vmc->b );
			vC = locVars[ vmc->c ];
			dnum = acom.real * bcom.real + acom.imag * bcom.imag;
			vC->xComplex.value.real = (acom.real*bcom.real + acom.imag*bcom.imag) / dnum;
			vC->xComplex.value.imag = (acom.imag*bcom.real - acom.real*bcom.imag) / dnum;
		}OPNEXT() OPCASE( GETI_SI ){
			str = locVars[ vmc->a ]->xString.data;
			id = IntegerOperand( vmc->b );
			if( id <0 ) id += str->size;
			if( id <0 || id >= str->size ) goto RaiseErrorIndexOutOfRange;
			if( str->mbs ){
				IntegerOperand( vmc->c ) = str->mbs[id];
			}else{
				IntegerOperand( vmc->c ) = str->wcs[id];
			}
		}OPNEXT() OPCASE( SETI_SII ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			str = locVars[ vmc->c ]->xString.data;
			id = IntegerOperand( vmc->b );
			inum = IntegerOperand( vmc->a );
			if( id <0 ) id += str->size;
			if( id <0 || id >= str->size ) goto RaiseErrorIndexOutOfRange;
			if( str->mbs ){
				str->mbs[id] = inum;
			}else{
				str->wcs[id] = inum;
			}
		}OPNEXT() OPCASE( GETI_LI ){
			list = & locVars[ vmc->a ]->xList;
			id = IntegerOperand( vmc->b );
			if( id <0 ) id += list->items->size;
			if( id <0 || id >= list->items->size ) goto RaiseErrorIndexOutOfRange;
			/* All GETX instructions assume the C regisgter is an intermediate register! */
			/* So no type checking is necessary here! */
			value = list->items->items.pValue[id];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( SETI_LI ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			list = & locVars[ vmc->c ]->xList;
			id = IntegerOperand( vmc->b );
			abtp = NULL;
			if( list->unitype && list->unitype->nested->size )
				abtp = list->unitype->nested->items.pType[0];
			if( id <0 ) id += list->items->size;
			if( id <0 || id >= list->items->size ) goto RaiseErrorIndexOutOfRange;
			if( DaoMoveAC( self, locVars[vmc->a], list->items->items.pValue + id, abtp ) ==0 )
				goto CheckException;
		}OPNEXT()
		OPCASE( GETI_LII )
			OPCASE( GETI_LFI )
			OPCASE( GETI_LDI )
			OPCASE( GETI_LSI ){
				list = & locVars[ vmc->a ]->xList;
				id = IntegerOperand( vmc->b );
				if( id <0 ) id += list->items->size;
				if( id <0 || id >= list->items->size ) goto RaiseErrorIndexOutOfRange;
				value = list->items->items.pValue[id];
				GC_ShiftRC( value, locVars[ vmc->c ] );
				locVars[ vmc->c ] = value;
			}OPNEXT()
		OPCASE( SETI_LIII )
			OPCASE( SETI_LIIF )
			OPCASE( SETI_LIID ){
				if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
				list = & locVars[ vmc->c ]->xList;
				vA = locVars[ vmc->a ];
				id = IntegerOperand( vmc->b );
				switch( vA->type ){
				case DAO_INTEGER : inum = vA->xInteger.value; break;
				case DAO_FLOAT   : inum = (dint) vA->xFloat.value; break;
				case DAO_DOUBLE  : inum = (dint) vA->xDouble.value; break;
				default : inum = 0; break;
				}
				if( id <0 ) id += list->items->size;
				if( id <0 || id >= list->items->size ) goto RaiseErrorIndexOutOfRange;
				list->items->items.pValue[id]->xInteger.value = inum;
			}OPNEXT()
		OPCASE( SETI_LFII )
			OPCASE( SETI_LFIF )
			OPCASE( SETI_LFID ){
				if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
				list = & locVars[ vmc->c ]->xList;
				vA = locVars[ vmc->a ];
				id = IntegerOperand( vmc->b );
				switch( vA->type ){
				case DAO_INTEGER : fnum = vA->xInteger.value; break;
				case DAO_FLOAT   : fnum = vA->xFloat.value; break;
				case DAO_DOUBLE  : fnum = vA->xDouble.value; break;
				default : fnum = 0; break;
				}
				if( id <0 ) id += list->items->size;
				if( id <0 || id >= list->items->size ) goto RaiseErrorIndexOutOfRange;
				list->items->items.pValue[id]->xFloat.value = fnum;
			}OPNEXT()
		OPCASE( SETI_LDII )
			OPCASE( SETI_LDIF )
			OPCASE( SETI_LDID ){
				if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
				list = & locVars[ vmc->c ]->xList;
				vA = locVars[ vmc->a ];
				id = IntegerOperand( vmc->b );
				switch( vA->type ){
				case DAO_INTEGER : dnum = vA->xInteger.value; break;
				case DAO_FLOAT   : dnum = vA->xFloat.value; break;
				case DAO_DOUBLE  : dnum = vA->xDouble.value; break;
				default : dnum = 0; break;
				}
				if( id <0 ) id += list->items->size;
				if( id <0 || id >= list->items->size ) goto RaiseErrorIndexOutOfRange;
				list->items->items.pValue[id]->xDouble.value = dnum;
			}OPNEXT()
		OPCASE( SETI_LSIS ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			list = & locVars[ vmc->c ]->xList;
			vA = locVars[ vmc->a ];
			id = IntegerOperand( vmc->b );
			if( id <0 ) id += list->items->size;
			if( id <0 || id >= list->items->size ) goto RaiseErrorIndexOutOfRange;
			DString_Assign( list->items->items.pValue[id]->xString.data, vA->xString.data );
		}OPNEXT()
#ifdef DAO_WITH_NUMARRAY
		OPCASE( GETI_AII ){
			array = & locVars[ vmc->a ]->xArray;
			id = IntegerOperand( vmc->b );
			if( id <0 ) id += array->size;
			if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
			IntegerOperand( vmc->c ) = array->data.i[id];
		}OPNEXT()
		OPCASE( GETI_AFI ){
			array = & locVars[ vmc->a ]->xArray;
			id = IntegerOperand( vmc->b );
			if( id <0 ) id += array->size;
			if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
			FloatOperand( vmc->c ) = array->data.f[id];
		}OPNEXT()
		OPCASE( GETI_ADI ){
			array = & locVars[ vmc->a ]->xArray;
			id = IntegerOperand( vmc->b );
			if( id <0 ) id += array->size;
			if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
			DoubleOperand( vmc->c ) = array->data.d[id];
		}OPNEXT()
		OPCASE( SETI_AIII )
			OPCASE( SETI_AIIF )
			OPCASE( SETI_AIID ){
				if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
				array = & locVars[ vmc->c ]->xArray;
				vA = locVars[ vmc->a ];
				id = IntegerOperand( vmc->b );
				switch( vA->type ){
				case DAO_INTEGER : inum = vA->xInteger.value; break;
				case DAO_FLOAT   : inum = (dint) vA->xFloat.value; break;
				case DAO_DOUBLE  : inum = (dint) vA->xDouble.value; break;
				default : inum = 0; break;
				}
				if( id <0 ) id += array->size;
				if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
				array->data.i[id] = inum;
			}OPNEXT()
		OPCASE( SETI_AFII )
			OPCASE( SETI_AFIF )
			OPCASE( SETI_AFID ){
				if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
				array = & locVars[ vmc->c ]->xArray;
				vA = locVars[ vmc->a ];
				id = IntegerOperand( vmc->b );
				switch( vA->type ){
				case DAO_INTEGER : fnum = vA->xInteger.value; break;
				case DAO_FLOAT   : fnum = vA->xFloat.value; break;
				case DAO_DOUBLE  : fnum = (float) vA->xDouble.value; break;
				default : fnum = 0; break;
				}
				if( id <0 ) id += array->size;
				if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
				array->data.f[id] = fnum;
			}OPNEXT()
		OPCASE( SETI_ADII )
			OPCASE( SETI_ADIF )
			OPCASE( SETI_ADID ){
				if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
				array = & locVars[ vmc->c ]->xArray;
				vA = locVars[ vmc->a ];
				id = IntegerOperand( vmc->b );
				switch( vA->type ){
				case DAO_INTEGER : dnum = vA->xInteger.value; break;
				case DAO_FLOAT   : dnum = vA->xFloat.value; break;
				case DAO_DOUBLE  : dnum = vA->xDouble.value; break;
				default : dnum = 0; break;
				}
				if( id <0 ) id += array->size;
				if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
				array->data.d[id] = dnum;
			}OPNEXT()
		OPCASE( GETI_ACI ){
			array = & locVars[ vmc->a ]->xArray;
			id = IntegerOperand( vmc->b );
			if( id <0 ) id += array->size;
			if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
			locVars[ vmc->c ]->xComplex.value = array->data.c[ id ];
		}OPNEXT()
		OPCASE( SETI_ACI ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			array = & locVars[ vmc->c ]->xArray;
			id = IntegerOperand( vmc->b );
			if( id <0 ) id += array->size;
			if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
			array->data.c[ id ] = locVars[ vmc->a ]->xComplex.value;
		}OPNEXT()
		OPCASE( GETI_AM ){
			array = & locVars[ vmc->a ]->xArray;
			tuple = & locVars[ vmc->b ]->xTuple;
			vC = locVars[ vmc->c ];
			if( array->dims->size == tuple->size ){
				dims = array->dims->items.pSize;
				dmac = array->dimAccum->items.pSize;
				id = 0;
				for(i=0; i<array->dims->size; i++){
					j = DaoValue_GetInteger( tuple->items[i] );
					if( j <0 ) j += dims[i];
					if( j <0 || j >= dims[i] ) goto RaiseErrorIndexOutOfRange;
					id += j * dmac[i];
				}
				switch( array->numType ){
				case DAO_COMPLEX : vC->xComplex.value = array->data.c[ id ]; break;
				case DAO_INTEGER : vC->xInteger.value = array->data.i[ id ]; break;
				case DAO_FLOAT : vC->xFloat.value = array->data.f[ id ]; break;
				case DAO_DOUBLE : vC->xDouble.value = array->data.d[ id ]; break;
				default : break;
				}
			}else{
				DaoProcess_DoGetItem( self, vmc );
				goto CheckException;
			}
		}OPNEXT()
		OPCASE( SETI_AM ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			array = & locVars[ vmc->c ]->xArray;
			list = & locVars[ vmc->b ]->xList;
			if( array->dims->size == list->items->size ){
				dims = array->dims->items.pSize;
				dmac = array->dimAccum->items.pSize;
				id = 0;
				for(i=0; i<array->dims->size; i++){
					j = DaoValue_GetInteger( list->items->items.pValue[i] );
					if( j <0 ) j += dims[i];
					if( j <0 || j >= dims[i] ) goto RaiseErrorIndexOutOfRange;
					id += j * dmac[i];
				}
				vA = locVars[ vmc->a ];
				com.imag = 0;
				switch( vA->type ){
				case DAO_INTEGER : dnum = vA->xInteger.value; com.real = dnum; break;
				case DAO_FLOAT   : dnum = vA->xFloat.value; com.real = dnum; break;
				case DAO_DOUBLE  : dnum = vA->xDouble.value; com.real = dnum; break;
				case DAO_COMPLEX : com = vA->xComplex.value; dnum = com.real; break;
				default : break;
				}
				inum = fnum = dnum;
				switch( array->numType ){
				case DAO_INTEGER : array->data.i[ id ] = inum; break;
				case DAO_FLOAT : array->data.f[ id ] = fnum; break;
				case DAO_DOUBLE : array->data.d[ id ] = dnum; break;
				case DAO_COMPLEX : array->data.c[ id ] = com; break;
				default : break;
				}
			}else{
				DaoProcess_DoSetItem( self, vmc );
				goto CheckException;
			}
		}OPNEXT()
#else
		OPCASE( GETI_AII )
			OPCASE( GETI_AFI )
			OPCASE( GETI_ADI )
			OPCASE( SETI_AIII )
			OPCASE( SETI_AIIF )
			OPCASE( SETI_AIID )
			OPCASE( SETI_AFII )
			OPCASE( SETI_AFIF )
			OPCASE( SETI_AFID )
			OPCASE( SETI_ADII )
			OPCASE( SETI_ADIF )
			OPCASE( SETI_ADID )
			OPCASE( GETI_ACI )
			OPCASE( SETI_ACI )
			OPCASE( GETI_AM )
			OPCASE( SETI_AM ){
				self->activeCode = vmc;
				DaoProcess_RaiseException( self, DAO_ERROR, "numeric array is disabled" );
			}OPNEXT()
#endif
		OPCASE( GETI_TI ){
			tuple = & locVars[ vmc->a ]->xTuple;
			id = IntegerOperand( vmc->b );
			if( id <0 || id >= tuple->size ) goto RaiseErrorIndexOutOfRange;
			value = tuple->items[id];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( SETI_TI ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			id = IntegerOperand( vmc->b );
			abtp = NULL;
			if( id <0 || id >= tuple->size ) goto RaiseErrorIndexOutOfRange;
			abtp = tuple->unitype->nested->items.pType[id];
			if( abtp->tid == DAO_PAR_NAMED ) abtp = & abtp->aux->xType;
			if( DaoMoveAC( self, locVars[vmc->a], tuple->items + id, abtp ) ==0 )
				goto CheckException;
		}OPNEXT() OPCASE( GETF_T ){
			tuple = & locVars[ vmc->a ]->xTuple;
			value = tuple->items[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( SETF_T ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			id = vmc->b;
			abtp = tuple->unitype->nested->items.pType[id];
			if( abtp->tid == DAO_PAR_NAMED ) abtp = & abtp->aux->xType;
			if( DaoMoveAC( self, locVars[vmc->a], tuple->items + id, abtp ) ==0 )
				goto CheckException;
		}OPNEXT() OPCASE( GETF_TI ){
			tuple = & locVars[ vmc->a ]->xTuple;
			locVars[ vmc->c ]->xInteger.value = tuple->items[ vmc->b ]->xInteger.value;
		}OPNEXT() OPCASE( GETF_TF ){
			tuple = & locVars[ vmc->a ]->xTuple;
			locVars[ vmc->c ]->xFloat.value = tuple->items[ vmc->b ]->xFloat.value;
		}OPNEXT() OPCASE( GETF_TD ){
			tuple = & locVars[ vmc->a ]->xTuple;
			locVars[ vmc->c ]->xDouble.value = tuple->items[ vmc->b ]->xDouble.value;
		}OPNEXT() OPCASE( GETF_TS ){
			tuple = & locVars[ vmc->a ]->xTuple;
			value = tuple->items[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( SETF_TII ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TIF ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TID ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TFI ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TFF ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TFD ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TDI ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TDF ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TDD ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xDouble.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TSS ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			vA = locVars[ vmc->a ];
			DString_Assign( tuple->items[ vmc->b ]->xString.data, vA->xString.data );
		}OPNEXT() OPCASE( GETF_KC ){
			value = locVars[ vmc->a ]->xClass.cstData->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETF_KG ){
			value = locVars[ vmc->a ]->xClass.glbData->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETF_OC ){
			value = locVars[ vmc->a ]->xObject.defClass->cstData->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETF_OG ){
			value = locVars[ vmc->a ]->xObject.defClass->glbData->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETF_OV ){
			object = & locVars[ vmc->a ]->xObject;
			if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
			value = object->objValues[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETF_KCI ){
			value = locVars[ vmc->a ]->xClass.cstData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xInteger.value = value->xInteger.value;
		}OPNEXT() OPCASE( GETF_KCF ){
			value = locVars[ vmc->a ]->xClass.cstData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xFloat.value = value->xFloat.value;
		}OPNEXT() OPCASE( GETF_KCD ){
			value = locVars[ vmc->a ]->xClass.cstData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xDouble.value = value->xDouble.value;
		}OPNEXT() OPCASE( GETF_KGI ){
			value = locVars[ vmc->a ]->xClass.glbData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xInteger.value = value->xInteger.value;
		}OPNEXT() OPCASE( GETF_KGF ){
			value = locVars[ vmc->a ]->xClass.glbData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xFloat.value = value->xFloat.value;
		}OPNEXT() OPCASE( GETF_KGD ){
			value = locVars[ vmc->a ]->xClass.glbData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xDouble.value = value->xDouble.value;
		}OPNEXT() OPCASE( GETF_OCI ){
			value = locVars[ vmc->a ]->xObject.defClass->cstData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xInteger.value = value->xInteger.value;
		}OPNEXT() OPCASE( GETF_OCF ){
			value = locVars[ vmc->a ]->xObject.defClass->cstData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xFloat.value = value->xFloat.value;
		}OPNEXT() OPCASE( GETF_OCD ){
			value = locVars[ vmc->a ]->xObject.defClass->cstData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xDouble.value = value->xDouble.value;
		}OPNEXT() OPCASE( GETF_OGI ){
			value = locVars[ vmc->a ]->xObject.defClass->glbData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xInteger.value = value->xInteger.value;
		}OPNEXT() OPCASE( GETF_OGF ){
			value = locVars[ vmc->a ]->xObject.defClass->glbData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xFloat.value = value->xFloat.value;
		}OPNEXT() OPCASE( GETF_OGD ){
			value = locVars[ vmc->a ]->xObject.defClass->glbData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xDouble.value = value->xDouble.value;
		}OPNEXT() OPCASE( GETF_OVI ){
			value = locVars[ vmc->a ]->xObject.objValues[ vmc->b ];
			locVars[ vmc->c ]->xInteger.value = value->xInteger.value;
		}OPNEXT() OPCASE( GETF_OVF ){
			value = locVars[ vmc->a ]->xObject.objValues[ vmc->b ];
			locVars[ vmc->c ]->xFloat.value = value->xFloat.value;
		}OPNEXT() OPCASE( GETF_OVD ){
			value = locVars[ vmc->a ]->xObject.objValues[ vmc->b ];
			locVars[ vmc->c ]->xDouble.value = value->xDouble.value;
		}OPNEXT() OPCASE( SETF_KG ){
			klass = & locVars[ vmc->c ]->xClass;
			vC2 = klass->glbData->items.pValue + vmc->b;
			abtp = klass->glbDataType->items.pType[ vmc->b ];
			if( DaoMoveAC( self, locVars[vmc->a], vC2, abtp ) ==0 ) goto CheckException;
		}OPNEXT() OPCASE( SETF_OG ) OPCASE( SETF_OV ){
			object = & locVars[ vmc->c ]->xObject;
			if( vmc->code == DVM_SETF_OG ){
				klass = ((DaoObject*)klass)->defClass;
				vC2 = klass->glbData->items.pValue + vmc->b;
				abtp = klass->glbDataType->items.pType[ vmc->b ];
			}else{
				if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
				vC2 = object->objValues + vmc->b;
				abtp = object->defClass->objDataType->items.pType[ vmc->b ];
			}
			if( DaoMoveAC( self, locVars[vmc->a], vC2, abtp ) ==0 )
				goto CheckException;
		}OPNEXT()
		OPCASE( SETF_KGII )
			OPCASE( SETF_KGIF )
			OPCASE( SETF_KGID )
			OPCASE( SETF_KGFI )
			OPCASE( SETF_KGFF )
			OPCASE( SETF_KGFD )
			OPCASE( SETF_KGDI )
			OPCASE( SETF_KGDF )
			OPCASE( SETF_KGDD ){
				klass = & locVars[ vmc->c ]->xClass;
				vC = klass->glbData->items.pValue[ vmc->b ];
				switch( vmc->code ){
				case DVM_SETF_KGII : vC->xInteger.value = IntegerOperand( vmc->a ); break;
				case DVM_SETF_KGIF : vC->xInteger.value = FloatOperand( vmc->a ); break;
				case DVM_SETF_KGID : vC->xInteger.value = DoubleOperand( vmc->a ); break;
				case DVM_SETF_KGFI : vC->xFloat.value = IntegerOperand( vmc->a ); break;
				case DVM_SETF_KGFF : vC->xFloat.value = FloatOperand( vmc->a ); break;
				case DVM_SETF_KGFD : vC->xFloat.value = DoubleOperand( vmc->a ); break;
				case DVM_SETF_KGDI : vC->xDouble.value = IntegerOperand( vmc->a ); break;
				case DVM_SETF_KGDF : vC->xDouble.value = FloatOperand( vmc->a ); break;
				case DVM_SETF_KGDD : vC->xDouble.value = DoubleOperand( vmc->a ); break;
				default : break;
				}
			}OPNEXT()
		OPCASE( SETF_OGII )
			OPCASE( SETF_OGIF )
			OPCASE( SETF_OGID )
			OPCASE( SETF_OGFI )
			OPCASE( SETF_OGFF )
			OPCASE( SETF_OGFD )
			OPCASE( SETF_OGDI )
			OPCASE( SETF_OGDF )
			OPCASE( SETF_OGDD ){
				klass = locVars[ vmc->c ]->xObject.defClass;
				vC = klass->glbData->items.pValue[ vmc->b ];
				switch( vmc->code ){
				case DVM_SETF_OGII : vC->xInteger.value = IntegerOperand( vmc->a ); break;
				case DVM_SETF_OGIF : vC->xInteger.value = FloatOperand( vmc->a ); break;
				case DVM_SETF_OGID : vC->xInteger.value = DoubleOperand( vmc->a ); break;
				case DVM_SETF_OGFI : vC->xFloat.value = IntegerOperand( vmc->a ); break;
				case DVM_SETF_OGFF : vC->xFloat.value = FloatOperand( vmc->a ); break;
				case DVM_SETF_OGFD : vC->xFloat.value = DoubleOperand( vmc->a ); break;
				case DVM_SETF_OGDI : vC->xDouble.value = IntegerOperand( vmc->a ); break;
				case DVM_SETF_OGDF : vC->xDouble.value = FloatOperand( vmc->a ); break;
				case DVM_SETF_OGDD : vC->xDouble.value = DoubleOperand( vmc->a ); break;
				default : break;
				}
			}OPNEXT()
		OPCASE( SETF_OVII )
			OPCASE( SETF_OVIF )
			OPCASE( SETF_OVID )
			OPCASE( SETF_OVFI )
			OPCASE( SETF_OVFF )
			OPCASE( SETF_OVFD )
			OPCASE( SETF_OVDI )
			OPCASE( SETF_OVDF )
			OPCASE( SETF_OVDD ){
				object = & locVars[ vmc->c ]->xObject;
				if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
				vC =  object->objValues[ vmc->b ];
				switch( vmc->code ){
				case DVM_SETF_OVII : vC->xInteger.value = IntegerOperand( vmc->a ); break;
				case DVM_SETF_OVIF : vC->xInteger.value = FloatOperand( vmc->a ); break;
				case DVM_SETF_OVID : vC->xInteger.value = DoubleOperand( vmc->a ); break;
				case DVM_SETF_OVFI : vC->xFloat.value = IntegerOperand( vmc->a ); break;
				case DVM_SETF_OVFF : vC->xFloat.value = FloatOperand( vmc->a ); break;
				case DVM_SETF_OVFD : vC->xFloat.value = DoubleOperand( vmc->a ); break;
				case DVM_SETF_OVDI : vC->xDouble.value = IntegerOperand( vmc->a ); break;
				case DVM_SETF_OVDF : vC->xDouble.value = FloatOperand( vmc->a ); break;
				case DVM_SETF_OVDD : vC->xDouble.value = DoubleOperand( vmc->a ); break;
				default : break;
				}
			}OPNEXT()
		OPCASE( SAFE_GOTO ){
			if( ( self->vmSpace->options & DAO_EXEC_SAFE ) ){
				gotoCount ++;
				if( gotoCount > 1E6 ){
					self->activeCode = vmc;
					DaoProcess_RaiseException( self, DAO_ERROR,
							"too many goto operations for safe running mode." );
					goto CheckException;
				}
			}
			vmc = vmcBase + vmc->b;
		}OPJUMP()
		OPCASE( TEST_I ){
			vmc = IntegerOperand( vmc->a ) ? vmc+1 : vmcBase+vmc->b;
		}OPJUMP()
		OPCASE( TEST_F ){
			vmc = FloatOperand( vmc->a ) ? vmc+1 : vmcBase+vmc->b;
		}OPJUMP()
		OPCASE( TEST_D ){
			vmc = DoubleOperand( vmc->a ) ? vmc+1 : vmcBase+vmc->b;
		}OPJUMP()
		OPDEFAULT()
		{
			goto CheckException;
RaiseErrorIndexOutOfRange:
			self->activeCode = vmc;
			DaoProcess_RaiseException( self, DAO_ERROR_INDEX_OUTOFRANGE, "" );
			goto CheckException;
RaiseErrorDivByZero:
			self->activeCode = vmc;
			//XXX topCtx->idClearFE = vmc - vmcBase;
			DaoProcess_RaiseException( self, DAO_ERROR_FLOAT_DIVBYZERO, "" );
			goto CheckException;
RaiseErrorInvalidOperation:
			self->activeCode = vmc;
			DaoProcess_RaiseException( self, DAO_ERROR, "invalid operation" );
			goto CheckException;
ModifyConstant:
			self->activeCode = vmc;
			DaoProcess_RaiseException( self, DAO_ERROR, "attempt to modify a constant" );
			goto CheckException;
AccessDefault:
			self->activeCode = vmc;
			DaoProcess_RaiseException( self, DAO_ERROR, "invalid field access for default object" );
			goto CheckException;
#if 0
RaiseErrorNullObject:
			self->activeCode = vmc;
			DaoProcess_RaiseException( self, DAO_ERROR, "operate on null object" );
			goto CheckException;
#endif
CheckException:

			locVars = self->activeValues;
			if( self->stopit | vmSpace->stopit ) goto FinishProc;
			//XXX if( invokehost ) handler->InvokeHost( handler, topCtx );
			if( self->exceptions->size > exceptCount ){
				size = (size_t)( vmc - vmcBase );
				if( topFrame->depth == 0 ) goto FinishCall;
				range = topFrame->ranges[ topFrame->depth-1 ];
				if( topFrame->depth >0 && size >= range[1] ) topFrame->depth --;
				topFrame->depth --;
				vmc = vmcBase + topFrame->ranges[ topFrame->depth ][1];
				exceptCount = self->exceptions->size;
				OPJUMP();
			}else if( self->status == DAO_VMPROC_STACKED ){
				goto CallEntry;
			}else if( self->status == DAO_VMPROC_SUSPENDED ){
				goto ReturnFalse;
			}else if( self->status == DAO_VMPROC_ABORTED ){
				goto FinishProc;
			}
			OPNEXT()
		}
	}OPEND()

FinishCall:

	if( self->topFrame && self->topFrame->state & DVM_SPEC_RUN ){
		if( self->exceptions->size > exceptCount ){
			self->status = DAO_VMPROC_ABORTED;
			goto ReturnFalse;
		}
		goto ReturnTrue;
	}
	print = (vmSpace->options & DAO_EXEC_INTERUN) && (here->options & DAO_NS_AUTO_GLOBAL);
	if( self->topFrame->prev == self->firstFrame && (print || vmSpace->evalCmdline) ){
		if( self->stackValues[0] ){
			DaoStream_WriteMBS( vmSpace->stdStream, "= " );
			DaoValue_Print( self->stackValues[0], self, vmSpace->stdStream, NULL );
			DaoStream_WriteNewLine( vmSpace->stdStream );
		}
	}
	DaoProcess_PopFrame( self );
	goto CallEntry;

FinishProc:

	DaoProcess_PrintException( self, 1 );
	if( self->topFrame && self->topFrame->state & DVM_SPEC_RUN ) goto ReturnTrue;
	DaoProcess_PopFrames( self, rollback );
	self->status = DAO_VMPROC_ABORTED;
	/*if( eventHandler ) eventHandler->mainRoutineExit(); */
ReturnFalse :
	DaoStream_Flush( self->vmSpace->stdStream );
	fflush( stdout );
	return 0;
ReturnTrue :
	DaoStream_Flush( self->vmSpace->stdStream );
	fflush( stdout );
	return 1;
}
extern void DaoProcess_Trace( DaoProcess *self, int depth );
int DaoVM_DoMath( DaoProcess *self, DaoVmCode *vmc, DaoValue *C, DaoValue *A )
{
	DaoNamespace *ns = self->activeRoutine->nameSpace;
	DaoType *type = self->activeTypes[vmc->c];
	DaoComplex tmp = {DAO_COMPLEX};
	int func = vmc->a;
	self->activeCode = vmc;
	if( A->type == DAO_COMPLEX ){
		complex16 par = A->xComplex.value;
		complex16 cres = {0.0,0.0};
		double rres = 0.0;
		int isreal = 0;
		switch( func ){
		case DVM_MATH_ABS  : rres = abs_c( par ); isreal = 1; break;
		case DVM_MATH_ARG  : rres = arg_c( par ); isreal = 1; break;
		case DVM_MATH_NORM  : rres = norm_c( par ); isreal = 1; break;
		case DVM_MATH_IMAG  : rres = par.imag; isreal = 1; break;
		case DVM_MATH_REAL  : rres = par.real; isreal = 1; break;
		case DVM_MATH_CEIL : cres = ceil_c( par ); break;
		case DVM_MATH_COS  : cres = cos_c( par );  break;
		case DVM_MATH_COSH : cres = cosh_c( par ); break;
		case DVM_MATH_EXP  : cres = exp_c( par );  break;
		case DVM_MATH_FLOOR : cres = floor_c( par ); break;
		case DVM_MATH_LOG  : cres = log_c( par );  break;
		case DVM_MATH_SIN  : cres = sin_c( par );  break;
		case DVM_MATH_SINH : cres = sinh_c( par ); break;
		case DVM_MATH_SQRT : cres = sqrt_c( par ); break;
		case DVM_MATH_TAN  : cres = tan_c( par );  break;
		case DVM_MATH_TANH : cres = tanh_c( par ); break;
		default : return 1;
		}
		if( isreal ){
			tmp.type = DAO_DOUBLE;
			if( type == NULL ) self->activeTypes[vmc->c] = DaoNamespace_GetType( ns, (DaoValue*) & tmp );
			if( C && C->type == DAO_DOUBLE ){
				C->xDouble.value = rres;
			}else{
				return DaoProcess_PutDouble( self, rres ) == NULL;
			}
		}else{
			if( type == NULL ) self->activeTypes[vmc->c] = DaoNamespace_GetType( ns, (DaoValue*) & tmp );
			if( C && C->type == DAO_COMPLEX ){
				C->xComplex.value = cres;
			}else{
				return DaoProcess_PutComplex( self, cres ) == NULL;
			}
		}
		return 0;
	}else if( A->type && A->type <= DAO_DOUBLE ){
		double par = DaoValue_GetDouble( A );
		double res = 0.0;
		switch( func ){
		case DVM_MATH_ABS  : res = fabs( par );  break;
		case DVM_MATH_ACOS : res = acos( par ); break;
		case DVM_MATH_ASIN : res = asin( par ); break;
		case DVM_MATH_ATAN : res = atan( par ); break;
		case DVM_MATH_CEIL : res = ceil( par ); break;
		case DVM_MATH_COS  : res = cos( par );  break;
		case DVM_MATH_COSH : res = cosh( par ); break;
		case DVM_MATH_EXP  : res = exp( par );  break;
		case DVM_MATH_FLOOR : res = floor( par ); break;
		case DVM_MATH_LOG  : res = log( par );  break;
		case DVM_MATH_RAND : res = par * rand() / (RAND_MAX+1.0); break;
		case DVM_MATH_SIN  : res = sin( par );  break;
		case DVM_MATH_SINH : res = sinh( par ); break;
		case DVM_MATH_SQRT : res = sqrt( par ); break;
		case DVM_MATH_TAN  : res = tan( par );  break;
		case DVM_MATH_TANH : res = tanh( par ); break;
		default : return 1;
		}
		if( func == DVM_MATH_RAND ){
			if( type == NULL ) self->activeTypes[vmc->c] = DaoNamespace_GetType( ns, A );
			switch( A->type ){
			case DAO_INTEGER : return DaoProcess_PutInteger( self, res ) == NULL;
			case DAO_FLOAT  : return DaoProcess_PutFloat( self, res ) == NULL;
			case DAO_DOUBLE : return DaoProcess_PutDouble( self, res ) == NULL;
			default : break;
			}
		}else if( C && C->type == DAO_DOUBLE ){
			C->xDouble.value = res;
			return 0;
		}else{
			tmp.type = DAO_DOUBLE;
			if( type == NULL ) self->activeTypes[vmc->c] = DaoNamespace_GetType( ns, (DaoValue*) & tmp );
			return DaoProcess_PutDouble( self, res ) == NULL;
		}
	}
	return 1;
}
void DaoProcess_Sort( DaoProcess *self, DaoVmCode *vmc, int index, int entry );
void DaoProcess_Apply( DaoProcess *self, DaoVmCode *vmc, int index, int dim, int entry );
static DaoValue* DaoArray_GetValue( DaoArray *self, int i, DaoValue *res )
{
	res->type = self->numType;
	switch( self->numType ){
	case DAO_INTEGER : res->xInteger.value = self->data.i[i]; break;
	case DAO_FLOAT : res->xFloat.value = self->data.f[i]; break;
	case DAO_DOUBLE : res->xDouble.value = self->data.d[i]; break;
	case DAO_COMPLEX : res->xComplex.value = self->data.c[i]; break;
	default : break;
	}
	return res;
}
static void DaoArray_SetValue( DaoArray *self, int i, DaoValue *value )
{
	switch( self->numType ){
	case DAO_INTEGER : self->data.i[i] = DaoValue_GetInteger( value ); break;
	case DAO_FLOAT : self->data.f[i] = DaoValue_GetFloat( value ); break;
	case DAO_DOUBLE : self->data.d[i] = DaoValue_GetDouble( value ); break;
	case DAO_COMPLEX : self->data.c[i] = DaoValue_GetComplex( value ); break;
	default : break;
	}
}
static void DaoArray_SetValues( DaoArray *self, int i, DaoTuple *tuple )
{
	int m = tuple->size;
	int j, k = i * m;
	for(j=0; j<m; j++) DaoArray_SetValue( self, k + j, tuple->items[j] );
}
static void DaoProcess_FailedMethod( DaoProcess *self )
{
	if( self->status == DAO_VMPROC_ABORTED )
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "functional method failed" );
	else
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "invalid return value" );
}
static void DaoProcess_Fold( DaoProcess *self, DaoVmCode *vmc, int index, int entry )
{
	DaoValue *res = NULL, *param = self->activeValues[ vmc->b ];
	DaoTuple *tuple = & param->xTuple;
	DaoList *list = & param->xList;
	DaoType *type = self->activeTypes[ index + 2 ];
	DaoValue **item = & self->activeValues[ index + 2 ];
	int i, noinit=1;
	if( param->type == DAO_TUPLE ){
		if( tuple->size ==0 ) return;
		list = & tuple->items[0]->xList;
		if( tuple->size >1 ){
			res = tuple->items[1];
			noinit = 0;
		}
	}
	if( list->items->size ==0 ) return;
	if( noinit ) res = list->items->items.pValue[0];
	DaoValue_Move( res, item, type );
	for(i=noinit; i<list->items->size; i++){
		self->activeValues[index]->xInteger.value = i;
		DaoProcess_ExecuteSection( self, entry );
		if( self->status == DAO_VMPROC_ABORTED ) goto MethodFailed;
		if( DaoValue_Move( self->stackValues[0], item, type ) == 0 ) goto MethodFailed;
	}
	self->activeCode = vmc; /* it is changed! */
	DaoProcess_PutValue( self, self->stackValues[0] );
	return;
MethodFailed: DaoProcess_FailedMethod( self );
}
static void DaoProcess_Fold2( DaoProcess *self, DaoVmCode *vmc, int index, int entry )
{
	DaoValue tmp;
	DaoValue **item = & self->activeValues[ index + 2 ];
	DaoValue *res = NULL, *param = self->activeValues[ vmc->b ];
	DaoTuple *tuple = & param->xTuple;
	DaoArray *array = & param->xArray;
	DaoType *type = self->activeTypes[ index + 2 ];
	int i, noinit=1;
	if( param->type == DAO_TUPLE ){
		if( tuple->size ==0 ) return;
		array = & tuple->items[0]->xArray;
		if( tuple->size >1 ){
			res = tuple->items[1];
			noinit = 0;
		}
	}
	if( array->size ==0 ) return;
	if( noinit ) res = DaoArray_GetValue( array, 0, & tmp );
	DaoValue_Move( res, item, type );
	for(i=noinit; i<array->size; i++){
		self->activeValues[index]->xInteger.value = i;
		DaoProcess_ExecuteSection( self, entry );
		if( self->status == DAO_VMPROC_ABORTED ) goto MethodFailed;
		if( DaoValue_Move( self->stackValues[0], item, type ) == 0 ) goto MethodFailed;
	}
	self->activeCode = vmc; /* it is changed! */
	DaoProcess_PutValue( self, self->stackValues[0] );
	return;
MethodFailed: DaoProcess_FailedMethod( self );
}
static void DaoProcess_Unfold( DaoProcess *self, DaoVmCode *vmc, int index, int entry )
{
	DaoValue *param = self->activeValues[ vmc->b ];
	DaoList *result = DaoProcess_PutList( self );
	DaoType *type = self->activeTypes[ index + 1 ];
	DaoValue **init = & self->activeValues[ index + 1 ];
	DaoValue_Move( param, init, type );
	DaoValue_Clear( & self->stackValues[0] ) ;
	DaoProcess_ExecuteSection( self, entry );
	int k = 0;
	while( self->stackValues[0]->type && ++k < 10 ){
		if( self->status == DAO_VMPROC_ABORTED ) goto MethodFailed;
		if( DaoList_PushBack( result, self->stackValues[0] ) ) goto MethodFailed;
		DaoValue_Clear( & self->stackValues[0] ) ;
		DaoProcess_ExecuteSection( self, entry );
	}
	return;
MethodFailed: DaoProcess_FailedMethod( self );
}
static int DaoArray_TypeShape( DaoArray *self, DaoArray *array, DaoType *type )
{
	DArray *dims = DArray_Copy( array->dims );
	int j, k, t, m = 1;
	int ret = 1;
	if( type->tid && type->tid <= DAO_COMPLEX ){
		self->numType = type->tid;
	}else if( type->tid == DAO_TUPLE && type->nested->size ){
		m = type->nested->size;
		t = type->nested->items.pType[0]->tid;
		for(j=1; j<m; j++){
			k = type->nested->items.pType[j]->tid;
			if( k > t ) t = k;
		}
		if( t == 0 || t > DAO_COMPLEX ) t = DAO_DOUBLE, ret = 0;
		self->numType = t;
		if( m > 1 ) DArray_Append( dims, m );
	}else{
		self->numType = DAO_DOUBLE;
		ret = 0;
	}
	DaoArray_ResizeVector( self, array->size * m );
	DaoArray_Reshape( self, dims->items.pSize, dims->size );
	DArray_Delete( dims );
	return 1;
}
static int DaoProcess_ListMapSIC( DaoProcess *self, DaoVmCode *vmc, int index, int entry )
{
	DaoValue *res = NULL;
	DaoValue *param = self->activeValues[ vmc->b ];
	DaoTuple *tuple = NULL;
	DaoList *list = & param->xList;
	DaoList *result = NULL;
	int i, j, count = 0;
	int size = 0;
	if( param->type == DAO_TUPLE ){
		tuple = & param->xTuple;
		list = & tuple->items[0]->xList;
		size = list->items->size;
		for( j=1; j<tuple->size; j++ )
			if( size != tuple->items[j]->xList.items->size ) return 1;
	}
	size = list->items->size;
	if( vmc->a != DVM_FUNCT_COUNT && vmc->a != DVM_FUNCT_EACH ) result = DaoProcess_PutList( self );
	for(i=0; i<size; i++){
		self->activeValues[index]->xInteger.value = i;
		DaoProcess_ExecuteSection( self, entry );
		if( self->status == DAO_VMPROC_ABORTED ) break;
		res = self->stackValues[0];
		switch( vmc->a ){
		case DVM_FUNCT_MAP :
			DaoList_PushBack( result, res );
			break;
		case DVM_FUNCT_SELECT :
			if( DaoValue_GetInteger( res ) ){
				if( tuple ){
					DaoTuple *tup = DaoTuple_New( tuple->size );
					for( j=0; j<tuple->size; j++ ){
						list = & tuple->items[j]->xList;
						DaoValue_Move( list->items->items.pValue[i], tup->items + j, NULL );
					}
					DaoList_PushBack( result, (DaoValue*)tup );
				}else{
					DaoList_PushBack( result, list->items->items.pValue[i] );
				}
			}
			break;
		case DVM_FUNCT_INDEX :
			if( DaoValue_GetInteger( res ) ){
				DaoInteger di = {DAO_INTEGER,0,0,0,0,0};
				di.value = i;
				DaoList_PushBack( result, (DaoValue*) & di );
			}
			break;
		case DVM_FUNCT_COUNT :
			if( DaoValue_GetInteger( res ) ) count ++;
			break;
		default : break;
		}
	}
	self->activeCode = vmc; /* it is changed! */
	if( vmc->a == DVM_FUNCT_COUNT ) DaoProcess_PutInteger( self, count );
	return 0;
}
static int DaoProcess_ArrayMapSIC( DaoProcess *self, DaoVmCode *vmc, int index, int entry )
{
#ifdef DAO_WITH_NUMARRAY
	DaoValue *param = self->activeValues[ vmc->b ];
	DaoArray *array = & param->xArray;
	DaoArray *result = NULL;
	DaoTuple *tuple = NULL;
	DaoList *list = NULL;
	DaoValue *nval = NULL;
	DaoValue *res = NULL;
	DaoValue tmp;
	int i, j, count = 0;
	int size = 0;
	if( param->type == DAO_TUPLE ){
		tuple = & param->xTuple;
		array = & tuple->items[0]->xArray;
		size = array->size;
		for( j=1; j<tuple->size; j++ )
			if( size != tuple->items[j]->xArray.size ) return 1;
	}
	size = array->size;
	if( vmc->a != DVM_FUNCT_COUNT && vmc->a != DVM_FUNCT_EACH ){
		if( vmc->a == DVM_FUNCT_MAP ){
			int last = (vmc-2)->a;
			result = DaoProcess_PutArray( self );
			if( DaoArray_TypeShape( result, array, self->activeTypes[last] ) == 0 ){
				DaoProcess_RaiseException( self, DAO_WARNING, "invalid return type" );
			}
		}else{
			list = DaoProcess_PutList( self );
		}
	}
	memset( & tmp, 0, sizeof(DaoValue) );
	for(i=0; i<size; i++){
		self->activeValues[index]->xInteger.value = i;
		DaoProcess_ExecuteSection( self, entry );
		if( self->status == DAO_VMPROC_ABORTED ) break;
		res = self->stackValues[0];
		switch( vmc->a ){
		case DVM_FUNCT_MAP :
			if( res->type == DAO_TUPLE && res->xTuple.size )
				DaoArray_SetValues( result, i, & res->xTuple );
			else
				DaoArray_SetValue( result, i, res );
			break;
		case DVM_FUNCT_SELECT :
			if( DaoValue_GetInteger( res ) ){
				if( tuple ){
					DaoTuple *tup = DaoTuple_New( tuple->size );
					for( j=0; j<tuple->size; j++ ){
						array = & tuple->items[j]->xArray;
						nval = DaoArray_GetValue( array, i, & tmp );
						DaoValue_Move( nval, tup->items + j, NULL );
					}
					DaoList_PushBack( list, (DaoValue*) tup );
				}else{
					nval = DaoArray_GetValue( array, i, & tmp );
					DaoList_PushBack( list, nval );
				}
			}
			break;
		case DVM_FUNCT_INDEX :
			if( DaoValue_GetInteger( res ) ){
				tmp.type = DAO_INTEGER;
				tmp.xInteger.value = i;
				DaoList_PushBack( list, & tmp );
			}
			break;
		case DVM_FUNCT_COUNT :
			if( DaoValue_GetInteger( res ) ) count ++;
			break;
		default : break;
		}
	}
	self->activeCode = vmc; /* it is changed! */
	if( vmc->a == DVM_FUNCT_COUNT ) DaoProcess_PutInteger( self, count );
#else
	DaoProcess_RaiseException( self, DAO_DISABLED_NUMARRAY, NULL );
#endif
	return 0;
}
static void DaoProcess_MapSIC( DaoProcess *self, DaoVmCode *vmc, int index, int entry )
{
	DaoValue *param = self->activeValues[ vmc->b ];
	DaoTuple *tuple = & param->xTuple;
	int j, k;
	if( param->type == DAO_LIST ){
		DaoProcess_ListMapSIC( self, vmc, index, entry );
	}else if( param->type == DAO_ARRAY ){
		DaoProcess_ArrayMapSIC( self, vmc, index, entry );
	}else if( param->type == DAO_TUPLE ){
		k = tuple->items[0]->type;
		for( j=1; j<tuple->size; j++ ) if( k != tuple->items[j]->type ) goto InvalidParam;
		if( k == DAO_ARRAY ){
			if( DaoProcess_ArrayMapSIC( self, vmc, index, entry ) ) goto InvalidParam;
		}else if( k == DAO_LIST ){
			if( DaoProcess_ListMapSIC( self, vmc, index, entry ) ) goto InvalidParam;
		}else goto InvalidParam;
	}else{
		goto InvalidParam;
	}
	return;
InvalidParam:
	DaoProcess_RaiseException( self, DAO_ERROR, "invalid parameter" );
}
int DaoArray_FromList( DaoArray *self, DaoList *list, DaoType *tp );
static void DaoProcess_DataFunctional( DaoProcess *self, DaoVmCode *vmc, int index, int entry )
{
	int count = DaoValue_GetInteger( self->activeValues[ vmc->b ] );
	int i, stype=1, isconst = 0;
	DaoArray *array = NULL;
	DaoList *list = NULL;
	DaoValue *res = NULL;
	DString *string = NULL;
#if 0
	if( self->codes + (entry+3) == vmc ){
		int c = self->codes[entry+1].code;
		isconst = ( c == DVM_MOVE || (c >= DVM_MOVE_II && c <= DVM_MOVE_PP ) );
		if( isconst ) last = self->codes[entry+1].a;
	} XXX
#endif
	switch( vmc->a ){
	case DVM_FUNCT_ARRAY :
		list = DaoList_New();
		array = DaoProcess_PutArray( self );
		break;
	case DVM_FUNCT_STRING : string = DaoProcess_PutMBString( self, "" ); break;
	case DVM_FUNCT_LIST : list = DaoProcess_PutList( self ); break;
	default : break;
	}
	for(i=0; i<count; i++){
		if( isconst == 0 ){
			self->activeValues[index]->xInteger.value = i;
			DaoProcess_ExecuteSection( self, entry );
			if( self->status == DAO_VMPROC_ABORTED ) break;
		}
		res = self->stackValues[0];
		switch( vmc->a ){
		case DVM_FUNCT_STRING :
			if( stype && res->type == DAO_STRING && res->xString.data->wcs ) DString_ToWCS( string );
			DString_Append( string, DaoValue_GetString( res, self->mbstring ) );
			break;
		case DVM_FUNCT_ARRAY :
		case DVM_FUNCT_LIST :
			DaoList_PushBack( list, res );
			break;
		default : break;
		}
	}
#ifdef DAO_WITH_NUMARRAY
	if( vmc->a == DVM_FUNCT_ARRAY ){
		if( DaoArray_FromList( array, list, self->activeTypes[ vmc->c ] ) ==0 ){
			DaoProcess_RaiseException( self, DAO_ERROR, "invalid array()" );
		}
		DaoList_Delete( list );
	}
#else
	if( vmc->a == DVM_FUNCT_ARRAY ){
		DaoProcess_RaiseException( self, DAO_ERROR, "numeric array is disabled" );
	}
#endif
}
static void DaoProcess_ApplyList( DaoProcess *self, DaoVmCode *vmc, int index, int vdim, int entry )
{
	DaoValue *res = NULL;
	DaoValue *param = self->activeValues[ vmc->b ];
	DaoList *list = & param->xList;
	int i, size = list->items->size;

	for(i=0; i<size; i++){
		/* Set the iteration variable's value in dao and execute the inline code. */
		self->activeValues[index]->xInteger.value = i;
		DaoProcess_ExecuteSection( self, entry );
		if( self->status == DAO_VMPROC_ABORTED ) break;
		res = self->stackValues[0];

		/* Now we need to replace the current list's content with the one returned
		 * by the inline code, that is placed in "res".
		 */
		DaoList_SetItem( list, res, i );
	}
	self->activeCode = vmc; /* it is changed! */
}
void DaoProcess_DoFunctional( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *param = self->activeValues[ vmc->b ];
	DaoTuple *tuple = & param->xTuple;
	DaoVmCode *vmcs = self->topFrame->codes;
	int entry = (int)( self->activeCode - vmcs );
	int index, idc = 0;
	int i = (vmc-1)->b;
	index = vmcs[i+1].c;
	if( i >=0 ){
		for(; i<entry; i++){
			int code = vmcs[i].code;
			if( code == DVM_DATA ) idc ++;
			if( code == DVM_SECT ) break;
		}
	}
	if( i <0 || i >= entry ){
		DaoProcess_RaiseException( self, DAO_ERROR, "code block not found" );
		return;
	}
	entry = i;
	switch( vmc->a ){
	case DVM_FUNCT_APPLY :
		if( param->type == DAO_LIST ){
			DaoProcess_ApplyList( self, vmc, index, idc-1, entry );
		}else if( param->type == DAO_ARRAY ){
#ifdef DAO_WITH_NUMARRAY
			DaoProcess_Apply( self, vmc, index, idc-1, entry );
#else
			DaoProcess_RaiseException( self, DAO_DISABLED_NUMARRAY, NULL );
#endif
		}else{
			DaoProcess_RaiseException( self, DAO_ERROR, "apply currently is only supported for numeric arrays and lists" );
		}
		break;
	case DVM_FUNCT_SORT :
		DaoProcess_Sort( self, vmc, index, entry );
		break;
	case DVM_FUNCT_FOLD :
		if( param->type != DAO_ARRAY && param->type != DAO_LIST && param->type != DAO_TUPLE ){
			DaoProcess_RaiseException( self, DAO_ERROR, "invalid fold/reduce()" );
			break;
		}
		if( param->type == DAO_TUPLE ){
			if( tuple->size ==0 ) break;
			if( tuple->items[0]->type == DAO_LIST )
				DaoProcess_Fold( self, vmc, index, entry );
			else if( tuple->items[0]->type == DAO_ARRAY )
				DaoProcess_Fold2( self, vmc, index, entry );
		}else if( param->type == DAO_ARRAY ){
			DaoProcess_Fold2( self, vmc, index, entry );
		}else{
			DaoProcess_Fold( self, vmc, index, entry );
		}
		break;
	case DVM_FUNCT_UNFOLD :
		DaoProcess_Unfold( self, vmc, index, entry );
		break;
	case DVM_FUNCT_MAP :
	case DVM_FUNCT_SELECT :
	case DVM_FUNCT_INDEX :
	case DVM_FUNCT_COUNT :
	case DVM_FUNCT_EACH :
		DaoProcess_MapSIC( self, vmc, index, entry );
		break;
	case DVM_FUNCT_REPEAT :
	case DVM_FUNCT_STRING :
	case DVM_FUNCT_ARRAY :
	case DVM_FUNCT_LIST :
		DaoProcess_DataFunctional( self, vmc, index, entry );
		break;
	default : break;
	}
	self->status = DAO_VMPROC_RUNNING;
	self->activeCode = vmc; /* it is changed! */
}
static void DaoType_WriteMainName( DaoType *self, DaoStream *stream )
{
	DString *name = self->name;
	int i, n = DString_FindChar( name, '<', 0 );
	if( n == MAXSIZE ) n = name->size;
	for(i=0; i<n; i++) DaoStream_WriteChar( stream, name->mbs[i] );
}
static void DString_Format( DString *self, int width, int head )
{
	int i, j, n, k = width - head;
	char buffer[32];
	if( head >= 30 ) head = 30;
	if( width <= head ) return;
	memset( buffer, ' ', head+1 );
	buffer[0] = '\n';
	buffer[head+1] = '\0';
	DString_ToMBS( self );
	n = self->size - head;
	if( self->size <= width ) return;
	while( n > k ){
		i = k * (n / k) + head;
		j = 0;
		while( (i+j) < self->size && isspace( self->mbs[i+j] ) ) j += 1;
		DString_InsertMBS( self, buffer, i, j, head+1 );
		n = i - head - 1;
	}
}
void DaoPrintException( DaoCdata *except, DaoStream *stream )
{
	DaoException *ex = (DaoException*) except->data;
	int i, h, w = 100, n = ex->callers->size;
	DaoStream *ss = DaoStream_New();
	DString *sstring = ss->streamString;
	ss->attribs |= DAO_IO_STRING;

	DaoStream_WriteMBS( ss, "[[" );
	DaoStream_WriteString( ss, ex->name );
	DaoStream_WriteMBS( ss, "]] --- " );
	h = sstring->size;
	if( h > 40 ) h = 40;
	DaoStream_WriteString( ss, ex->info );
	DaoStream_WriteMBS( ss, ":" );
	DaoStream_WriteNewLine( ss );
	DString_Format( sstring, w, h );
	DaoStream_WriteString( stream, sstring );
	DString_Clear( sstring );

	if( ex->data && ex->data->type == DAO_STRING ){
		DaoStream_WriteString( ss, ex->data->xString.data );
		if( DString_RFindChar( sstring, '\n', -1 ) != sstring->size-1 )
			DaoStream_WriteNewLine( ss );
		DaoStream_WriteString( stream, sstring );
		DString_Clear( sstring );
		DaoStream_WriteMBS( ss, "--\n" );
	}

	DaoStream_WriteMBS( ss, "Raised by:  " );
	if( ex->routine->attribs & DAO_ROUT_PARSELF ){
		DaoType *type = ex->routine->routType->nested->items.pType[0];
		DaoType_WriteMainName( & type->aux->xType, ss );
		DaoStream_WriteMBS( ss, "." );
	}else if( ex->routine->routHost ){
		DaoType_WriteMainName( ex->routine->routHost, ss );
		DaoStream_WriteMBS( ss, "." );
	}
	DaoStream_WriteString( ss, ex->routine->routName );
	DaoStream_WriteMBS( ss, "(), " );

	if( ex->routine->type == DAO_ROUTINE ){
		DaoStream_WriteMBS( ss, "at line " );
		DaoStream_WriteInt( ss, ex->fromLine );
		if( ex->fromLine != ex->toLine ){
			DaoStream_WriteMBS( ss, "-" );
			DaoStream_WriteInt( ss, ex->toLine );
		}
		DaoStream_WriteMBS( ss, " in file \"" );
		DaoStream_WriteString( ss, ex->routine->nameSpace->name );
		DaoStream_WriteMBS( ss, "\";\n" );
	}else{
		DaoStream_WriteMBS( ss, "from namespace \"" );
		DaoStream_WriteString( ss, ex->routine->nameSpace->name );
		DaoStream_WriteMBS( ss, "\";\n" );
	}
	DString_Format( sstring, w, 12 );
	DaoStream_WriteString( stream, sstring );
	DString_Clear( sstring );

	for(i=0; i<n; i++){
		DRoutine *rout = (DRoutine*) ex->callers->items.pRout[i];
		DaoStream_WriteMBS( ss, "Called by:  " );
		if( rout->attribs & DAO_ROUT_PARSELF ){
			DaoType *type = rout->routType->nested->items.pType[0];
			DaoType_WriteMainName( & type->aux->xType, ss );
			DaoStream_WriteMBS( ss, "." );
		}else if( rout->routHost ){
			DaoType_WriteMainName( rout->routHost, ss );
			DaoStream_WriteMBS( ss, "." );
		}
		DaoStream_WriteString( ss, rout->routName );
		DaoStream_WriteMBS( ss, "(), " );
		DaoStream_WriteMBS( ss, "at line " );
		DaoStream_WriteInt( ss, ex->lines->items.pInt[i] );
		DaoStream_WriteMBS( ss, " in file \"" );
		DaoStream_WriteString( ss, rout->nameSpace->name );
		DaoStream_WriteMBS( ss, "\";\n" );
		DString_Format( sstring, w, 12 );
		DaoStream_WriteString( stream, sstring );
		DString_Clear( sstring );
	}
	DaoStream_Delete( ss );
}
void DaoProcess_PrintException( DaoProcess *self, int clear )
{
	DaoType *extype = dao_Exception_Typer.core->kernel->abtype;
	DaoStream *stdio = self->vmSpace->stdStream;
	DaoValue **excobjs = self->exceptions->items.pValue;
	int i;
	for(i=0; i<self->exceptions->size; i++){
		DaoCdata *cdata = NULL;
		if( excobjs[i]->type == DAO_CDATA ){
			cdata = & excobjs[i]->xCdata;
		}else if( excobjs[i]->type == DAO_OBJECT ){
			cdata = (DaoCdata*)DaoObject_MapThisObject( & excobjs[i]->xObject, extype );
		}
		if( cdata == NULL ) continue;
		DaoPrintException( cdata, stdio );
	}
	if( clear ) DArray_Clear( self->exceptions );
}

DaoValue* DaoProcess_MakeConst( DaoProcess *self )
{
	DaoType *types[] = { NULL, NULL, NULL };
	DaoVmCodeX vmcx = {0,0,0,0,0,0,0,0,0};
	DaoVmCode *vmc = self->activeCode;

	dao_fe_clear();
	//self->idClearFE = -1;
	self->activeValues = self->stackValues;
	if( self->activeTypes == NULL ) self->activeTypes = types;
	if( self->activeRoutine->annotCodes->size == 0 )
		DArray_Append( self->activeRoutine->annotCodes, & vmcx );

	switch( vmc->code ){
	case DVM_MOVE :
		DaoProcess_DoMove( self, vmc ); break;
	case DVM_ADD : case DVM_SUB : case DVM_MUL :
	case DVM_DIV : case DVM_MOD : case DVM_POW :
		DaoProcess_DoBinArith( self, vmc );
		break;
	case DVM_AND : case DVM_OR : case DVM_LT :
	case DVM_LE :  case DVM_EQ : case DVM_NE :
		DaoProcess_DoBinBool( self, vmc );
		break;
	case DVM_IN :
		DaoProcess_DoInTest( self, vmc );
		break;
	case DVM_NOT : case DVM_UNMS :
		DaoProcess_DoUnaArith( self, vmc ); break;
	case DVM_BITAND : case DVM_BITOR : case DVM_BITXOR :
		DaoProcess_DoBitLogic( self, vmc ); break;
	case DVM_BITLFT : case DVM_BITRIT :
		DaoProcess_DoBitShift( self, vmc ); break;
	case DVM_BITREV :
		DaoProcess_DoBitFlip( self, vmc ); break;
	case DVM_CHECK :
		DaoProcess_DoCheck( self, vmc ); break;
	case DVM_NAMEVA :
		DaoProcess_BindNameValue( self, vmc ); break;
	case DVM_PAIR :
		DaoProcess_DoPair( self, vmc ); break;
	case DVM_TUPLE :
		DaoProcess_DoTuple( self, vmc ); break;
	case DVM_GETI :
	case DVM_GETMI :
		DaoProcess_DoGetItem( self, vmc ); break;
	case DVM_GETF :
		DaoProcess_DoGetField( self, vmc ); break;
	case DVM_SETI :
	case DVM_SETMI :
		DaoProcess_DoSetItem( self, vmc ); break;
	case DVM_SETF :
		DaoProcess_DoSetField( self, vmc ); break;
	case DVM_LIST :
		DaoProcess_DoList( self, vmc ); break;
	case DVM_MAP :
	case DVM_HASH :
		DaoProcess_DoMap( self, vmc ); break;
	case DVM_ARRAY :
		DaoProcess_DoArray( self, vmc ); break;
	case DVM_MATRIX :
		DaoProcess_DoMatrix( self, vmc ); break;
	case DVM_MATH :
		DaoVM_DoMath( self, vmc, self->activeValues[ vmc->c ], self->activeValues[1] );
		break;
	case DVM_CURRY :
	case DVM_MCURRY :
		DaoProcess_DoCurry( self, vmc );
		break;
	case DVM_CALL :
	case DVM_MCALL :
		DaoProcess_DoCall( self, vmc ); break;
	default: break;
	}
	self->activeCode = NULL;
	self->activeTypes = NULL;
	DaoProcess_CheckFE( self );
	if( self->exceptions->size >0 ){
		DaoProcess_PrintException( self, 1 );
		return NULL;
	}

	/* avoid GC */
	/* DArray_Clear( self->regArray ); */
	return self->stackValues[ vmc->c ];
}
