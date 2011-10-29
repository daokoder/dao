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
#include"daoSched.h"

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
extern void DaoProcess_DoCall( DaoProcess *self, DaoVmCode *vmc );

/* if return TRUE, there is exception, and look for the next rescue point. */
extern int DaoProcess_DoCheckExcept( DaoProcess *self, DaoVmCode *vmc );
/* if return DAO_STATUS_EXCEPTION, real exception is rose, and look for the next rescue point. */
extern void DaoProcess_DoRaiseExcept( DaoProcess *self, DaoVmCode *vmc );
/* return TRUE, if some exceptions can be rescued */
extern int DaoProcess_DoRescueExcept( DaoProcess *self, DaoVmCode *vmc );

extern void DaoProcess_MakeRoutine( DaoProcess *self, DaoVmCode *vmc );
extern void DaoProcess_MakeClass( DaoProcess *self, DaoVmCode *vmc );

static DaoVmCode* DaoProcess_DoSwitch( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoMove( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoReturn( DaoProcess *self, DaoVmCode *vmc );
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
	if( self->abtype ) GC_DecRC( self->abtype );
	if( self->future ) GC_DecRC( self->future );
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
	frame->sect = NULL;
	frame->stackBase = self->stackTop;
	frame->entry = 0;
	frame->state = 0;
	frame->returning = -1;
	if( self->topFrame->routine && self->activeCode ){
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
	self->topFrame->outer = NULL;
	if( self->topFrame->state & DVM_FRAME_SECT ){
		GC_DecRC( self->topFrame->object );
		GC_DecRC( self->topFrame->routine );
		self->topFrame->routine = NULL;
		self->topFrame->object = NULL;
		self->topFrame = self->topFrame->prev;
		return;
	}
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
void DaoProcess_MakeTuple( DaoProcess *self, DaoTuple *tuple, DaoValue *its[], int N );
int DaoProcess_Resume( DaoProcess *self, DaoValue *par[], int N, DaoProcess *ret )
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
	DaoProcess_PutValue( ret, self->stackValues[0] );
	return 1;
}

static DaoStackFrame* DaoProcess_FindSectionFrame( DaoProcess *self )
{
	DaoStackFrame *frame = self->topFrame;
	DaoType *cbtype = NULL;
	DaoVmCode *codes;
	if( frame->routine ) cbtype = frame->routine->routType->cbtype;
	if( frame->function ) cbtype = frame->function->routType->cbtype;
	if( cbtype == NULL ) return NULL;
	if( frame->sect ){
		/* yield inside code section should execute code section for the routine: */
		frame = frame->sect->prev;
	}else{
		frame = frame->active;
	}
	while( frame != self->firstFrame ){
		DaoType *cbtype2 = NULL;
		if( frame->routine ){
			cbtype2 = frame->routine->routType->cbtype;
			codes = frame->codes + frame->entry;
			if( codes[0].code == DVM_GOTO && codes[1].code == DVM_SECT ) return frame;
		}
		if( frame->function ) cbtype2 = frame->function->routType->cbtype;
		if( cbtype2 == NULL || DaoType_MatchTo( cbtype, cbtype2, NULL ) == 0 ) break;
		frame = frame->prev;
	}
	if( frame == NULL || frame->routine == NULL ) return NULL;
	codes = frame->codes + frame->entry;
	if( codes[0].code == DVM_GOTO && codes[1].code == DVM_SECT ) return frame;
	return NULL;
}
DaoStackFrame* DaoProcess_PushSectionFrame( DaoProcess *self )
{
	DaoStackFrame *next, *frame = DaoProcess_FindSectionFrame( self );
	int returning = -1;

	if( frame == NULL ) return NULL;
	if( self->topFrame->routine ){
		self->topFrame->entry = 1 + self->activeCode - self->topFrame->codes;
		returning = self->activeCode->c;
	}
	next = DaoProcess_PushFrame( self, 0 );
	next->entry = frame->entry + 2;
	next->state = DVM_FRAME_SECT | DVM_FRAME_KEEP;
	next->depth = 0;

	GC_ShiftRC( frame->object, next->object );
	GC_ShiftRC( frame->routine, next->routine );
	GC_DecRC( next->function );
	next->function = NULL;
	next->routine = frame->routine;
	next->object = frame->object;
	next->parCount = frame->parCount;
	next->stackBase = frame->stackBase;
	next->types = frame->types;
	next->codes = frame->codes;
	next->active = next;
	next->sect = frame;
	next->outer = self;
	next->returning = returning;
	DaoProcess_SetActiveFrame( self, frame );
	return frame;
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
	ret = DaoProcess_Execute( self ) == 0 ? DAO_ERROR : 0;
	DaoStream_Flush( self->vmSpace->stdStream );
	fflush( stdout );
	return ret;
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

void DaoProcess_AdjustCodes( DaoProcess *self, int options );

int DaoMoveAC( DaoProcess *self, DaoValue *A, DaoValue **C, DaoType *t );

#if defined( __GNUC__ ) && !defined( __STRICT_ANSI__ )
#define HAS_VARLABEL
#endif
#if 0
#endif

int DaoProcess_Execute( DaoProcess *self )
{
	DaoStackFrame *rollback = NULL;
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
	DaoProcess *dataVH[DAO_MAX_SECTDEPTH] = { NULL, NULL, NULL, NULL };
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
		&& LAB_GETVH , && LAB_GETVL , && LAB_GETVO , && LAB_GETVK , && LAB_GETVG ,
		&& LAB_GETI  , && LAB_GETMI , && LAB_GETF  , && LAB_GETMF ,
		&& LAB_SETVH , && LAB_SETVL , && LAB_SETVO , && LAB_SETVK , && LAB_SETVG ,
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
		&& LAB_MATH ,
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
		&& LAB_GETVH_I , && LAB_GETVH_F , && LAB_GETVH_D ,
		&& LAB_GETVL_I , && LAB_GETVL_F , && LAB_GETVL_D ,
		&& LAB_GETVO_I , && LAB_GETVO_F , && LAB_GETVO_D ,
		&& LAB_GETVK_I , && LAB_GETVK_F , && LAB_GETVK_D ,
		&& LAB_GETVG_I , && LAB_GETVG_F , && LAB_GETVG_D ,
		&& LAB_SETVH_II , && LAB_SETVH_IF , && LAB_SETVH_ID ,
		&& LAB_SETVH_FI , && LAB_SETVH_FF , && LAB_SETVH_FD ,
		&& LAB_SETVH_DI , && LAB_SETVH_DF , && LAB_SETVH_DD ,
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


	if( self->topFrame == self->firstFrame ) goto ReturnFalse;
	rollback = self->topFrame->prev;
	base = self->topFrame;
	if( self->status == DAO_VMPROC_SUSPENDED ) base = self->firstFrame->next;

CallEntry:

	/*
	   printf( "stack size = %s %i %i\n", getenv("PROC_NAME"), self->stackContext->size, base );
	 */
	if( self->topFrame == base->prev ){
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

	if( (vmSpace->options & DAO_EXEC_DEBUG) | (routine->mode & DAO_EXEC_DEBUG) )
		DaoProcess_AdjustCodes( self, vmSpace->options );

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
		if( self->pauseType == DAO_VMP_ASYNC && self->future->precondition ){
			int finished = self->future->precondition->state == DAO_CALL_FINISHED;
			if( self->future->state2 == DAO_FUTURE_VALUE ){
				DaoProcess_PutValue( self, finished ? self->future->precondition->value : null );
			}else{
				DaoProcess_PutInteger( self, finished );
			}
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
	if( topFrame->outer ){
		DaoStackFrame *frame = topFrame;
		for(i=0; (i<DAO_MAX_SECTDEPTH) && frame->outer; i++){
			dataVH[i] = frame->outer;
			frame = frame->sect;
		}
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
		}OPNEXT() OPCASE( GETVH ){
			GC_ShiftRC( dataVH[ vmc->a ]->activeValues[ vmc->b ], locVars[ vmc->c ] );
			locVars[ vmc->c ] = dataVH[ vmc->a ]->activeValues[ vmc->b ];
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
		}OPNEXT() OPCASE( SETVH ){
			abtp = locTypes[ vmc->b ];
			if( DaoMoveAC( self, locVars[vmc->a], dataVH[ vmc->c ]->activeValues + vmc->b, abtp ) ==0 )
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
			}else if( locVars[ vmc->a ] ){
				/* mt.run(3)::{ mt.critical::{} }: the inner functional will be compiled
				 * as a LOAD and RETURN, but the inner functional will not return anything,
				 * so the first operand of LOAD will be NULL! */
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
			case DAO_CTYPE :
			case DAO_CDATA :
				vmc = vA->xCdata.data ? vmc+1 : vmcBase + vmc->b;
				break;
			default :
				vmc = vmcBase + vmc->b;
				break;
			}
		}OPJUMP() OPCASE( MATH ){
			if( DaoVM_DoMath( self, vmc, locVars[ vmc->c ], locVars[vmc->b] ) )
				goto RaiseErrorInvalidOperation;
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
			if( routine->routType->cbtype ){
				DaoVmCode *vmc2;
				if( DaoProcess_PushSectionFrame( self ) == NULL ){
					printf( "No code section is found\n" ); //XXX
					goto FinishProc;
				}
				self->topFrame->state = DVM_FRAME_SECT;
				vmc2 = self->topFrame->codes + self->topFrame->entry - 1;
				locVars = self->stackValues + topFrame->stackBase;
				for(i=0; i<vmc2->b; i++){
					if( i >= vmc->b ) break;
					if( DaoProcess_SetValue( self, vmc2->a + i, locVars[vmc->a + i] ) == 0 ){
						DaoProcess_RaiseException( self, DAO_ERROR_PARAM, "invalid yield" );
					}
				}
				self->status = DAO_VMPROC_STACKED;
				goto CheckException;
			}
			if( self->abtype == NULL ){
				DaoProcess_RaiseException( self, DAO_ERROR, "Not a coroutine to yield." );
				goto CheckException;
			}
			DaoProcess_DoReturn( self, vmc );
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
		}OPNEXT() OPCASE( GETVH_I ){
			locVars[ vmc->c ]->xInteger.value = dataVH[ vmc->a ]->activeValues[ vmc->b ]->xInteger.value;
		}OPNEXT() OPCASE( GETVH_F ){
			locVars[ vmc->c ]->xFloat.value = dataVH[ vmc->a ]->activeValues[ vmc->b ]->xFloat.value;
		}OPNEXT() OPCASE( GETVH_D ){
			locVars[ vmc->c ]->xDouble.value = dataVH[ vmc->a ]->activeValues[ vmc->b ]->xDouble.value;
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
		}OPNEXT() OPCASE( SETVH_II ){
			dataVH[ vmc->c ]->activeValues[ vmc->b ]->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVH_IF ){
			dataVH[ vmc->c ]->activeValues[ vmc->b ]->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVH_ID ){
			dataVH[ vmc->c ]->activeValues[ vmc->b ]->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVH_FI ){
			dataVH[ vmc->c ]->activeValues[ vmc->b ]->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVH_FF ){
			dataVH[ vmc->c ]->activeValues[ vmc->b ]->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVH_FD ){
			dataVH[ vmc->c ]->activeValues[ vmc->b ]->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVH_DI ){
			dataVH[ vmc->c ]->activeValues[ vmc->b ]->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVH_DF ){
			dataVH[ vmc->c ]->activeValues[ vmc->b ]->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVH_DD ){
			dataVH[ vmc->c ]->activeValues[ vmc->b ]->xDouble.value = DoubleOperand( vmc->a );
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
			if( locVars[ vmc->a ] == NULL ) goto RaiseErrorNullObject;
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
			if( array->original && DaoArray_Sliced( array ) == 0 ) goto RaiseErrorSlicing; 
			if( id <0 ) id += array->size;
			if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
			IntegerOperand( vmc->c ) = array->data.i[id];
		}OPNEXT()
		OPCASE( GETI_AFI ){
			array = & locVars[ vmc->a ]->xArray;
			id = IntegerOperand( vmc->b );
			if( array->original && DaoArray_Sliced( array ) == 0 ) goto RaiseErrorSlicing; 
			if( id <0 ) id += array->size;
			if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
			FloatOperand( vmc->c ) = array->data.f[id];
		}OPNEXT()
		OPCASE( GETI_ADI ){
			array = & locVars[ vmc->a ]->xArray;
			id = IntegerOperand( vmc->b );
			if( array->original && DaoArray_Sliced( array ) == 0 ) goto RaiseErrorSlicing; 
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
				if( array->original && DaoArray_Sliced( array ) == 0 ) goto RaiseErrorSlicing; 
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
				if( array->original && DaoArray_Sliced( array ) == 0 ) goto RaiseErrorSlicing; 
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
				if( array->original && DaoArray_Sliced( array ) == 0 ) goto RaiseErrorSlicing; 
				if( id <0 ) id += array->size;
				if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
				array->data.d[id] = dnum;
			}OPNEXT()
		OPCASE( GETI_ACI ){
			array = & locVars[ vmc->a ]->xArray;
			id = IntegerOperand( vmc->b );
			if( array->original && DaoArray_Sliced( array ) == 0 ) goto RaiseErrorSlicing; 
			if( id <0 ) id += array->size;
			if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
			locVars[ vmc->c ]->xComplex.value = array->data.c[ id ];
		}OPNEXT()
		OPCASE( SETI_ACI ){
			if( locVars[ vmc->c ]->xNull.trait & DAO_DATA_CONST ) goto ModifyConstant;
			array = & locVars[ vmc->c ]->xArray;
			id = IntegerOperand( vmc->b );
			if( array->original && DaoArray_Sliced( array ) == 0 ) goto RaiseErrorSlicing; 
			if( id <0 ) id += array->size;
			if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
			array->data.c[ id ] = locVars[ vmc->a ]->xComplex.value;
		}OPNEXT()
		OPCASE( GETI_AM ){
			array = & locVars[ vmc->a ]->xArray;
			tuple = & locVars[ vmc->b ]->xTuple;
			vC = locVars[ vmc->c ];
			if( array->original && DaoArray_Sliced( array ) == 0 ) goto RaiseErrorSlicing; 
			if( array->ndim == tuple->size ){
				dims = array->dims;
				dmac = array->dims + array->ndim;
				id = 0;
				for(i=0; i<array->ndim; i++){
					j = DaoValue_GetInteger( tuple->items[i] );
					if( j <0 ) j += dims[i];
					if( j <0 || j >= dims[i] ) goto RaiseErrorIndexOutOfRange;
					id += j * dmac[i];
				}
				switch( array->etype ){
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
			if( array->original && DaoArray_Sliced( array ) == 0 ) goto RaiseErrorSlicing; 
			if( array->ndim == list->items->size ){
				dims = array->dims;
				dmac = array->dims + array->ndim;
				id = 0;
				for(i=0; i<array->ndim; i++){
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
				switch( array->etype ){
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
RaiseErrorSlicing:
			self->activeCode = vmc;
			DaoProcess_RaiseException( self, DAO_ERROR_INDEX, "slicing" );
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
RaiseErrorNullObject:
			self->activeCode = vmc;
			DaoProcess_RaiseException( self, DAO_ERROR, "operate on null object" );
			goto CheckException;
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

	if( self->topFrame->state & DVM_FRAME_KEEP ){
		self->status = DAO_VMPROC_FINISHED;
		if( self->exceptions->size > exceptCount ){
			self->status = DAO_VMPROC_ABORTED;
			goto ReturnFalse;
		}
		goto ReturnTrue;
	}
	DaoProcess_PopFrame( self );
	goto CallEntry;

FinishProc:

	if( self->exceptions->size ) DaoProcess_PrintException( self, 1 );
	DaoProcess_PopFrames( self, rollback );
	self->status = DAO_VMPROC_ABORTED;
	/*if( eventHandler ) eventHandler->mainRoutineExit(); */
ReturnFalse :
	return 0;
ReturnTrue :
	if( self->topFrame == self->firstFrame ){
		print = (vmSpace->options & DAO_EXEC_INTERUN) && (here->options & DAO_NS_AUTO_GLOBAL);
		if( (print || vmSpace->evalCmdline) && self->stackValues[0] ){
			DaoStream_WriteMBS( vmSpace->stdStream, "= " );
			DaoValue_Print( self->stackValues[0], self, vmSpace->stdStream, NULL );
			DaoStream_WriteNewLine( vmSpace->stdStream );
		}
	}
	return 1;
}
DaoVmCode* DaoProcess_DoSwitch( DaoProcess *self, DaoVmCode *vmc )
{
	DaoVmCode *mid;
	DaoValue **cst = self->activeRoutine->routConsts->items.pValue;
	DaoValue *opa = self->activeValues[ vmc->a ];
	int first, last, cmp, id;
	dint min, max;

	if( vmc->c ==0 ) return self->topFrame->codes + vmc->b;
	if( vmc[1].c == DAO_CASE_TABLE ){
		if( opa->type == DAO_INTEGER ){
			min = cst[ vmc[1].a ]->xInteger.value;
			max = cst[ vmc[vmc->c].a ]->xInteger.value;
			if( opa->xInteger.value >= min && opa->xInteger.value <= max )
				return self->topFrame->codes + vmc[ opa->xInteger.value - min + 1 ].b;
		}else if( opa->type== DAO_ENUM ){
			min = cst[ vmc[1].a ]->xEnum.value;
			max = cst[ vmc[vmc->c].a ]->xEnum.value;
			if( opa->xEnum.value >= min && opa->xEnum.value <= max )
				return self->topFrame->codes + vmc[ opa->xEnum.value - min + 1 ].b;
		}
		return self->topFrame->codes + vmc->b;
	}else if( vmc[1].c == DAO_CASE_UNORDERED ){
		for(id=1; id<=vmc->c; id++){
			mid = vmc + id;
			if( DaoValue_Compare( opa, cst[ mid->a ] ) ==0 ){
				return self->topFrame->codes + mid->b;
			}
		}
	}
	first = 1;
	last = vmc->c;
	while( first <= last ){
		id = ( first + last ) / 2;
		mid = vmc + id;
		cmp = DaoValue_Compare( opa, cst[ mid->a ] );
		if( cmp ==0 ){
			if( cst[mid->a]->type== DAO_TUPLE && cst[mid->a]->xTuple.subtype == DAO_PAIR ){
				while( id > first && DaoValue_Compare( opa, cst[ vmc[id-1].a ] ) ==0 ) id --;
				mid = vmc + id;
			}
			return self->topFrame->codes + mid->b;
		}else if( cmp <0 ){
			last = id - 1;
		}else{
			first = id + 1;
		}
	}
	return self->topFrame->codes + vmc->b;
}
int DaoMoveAC( DaoProcess *self, DaoValue *A, DaoValue **C, DaoType *t )
{
	if( ! DaoValue_Move( A, C, t ) ){
		DaoType *type;
		if( self->activeCode->code == DVM_MOVE || self->activeCode->code == DVM_MOVE_PP ){
			if( A->type == DAO_CDATA && t && t->tid == DAO_CDATA ){
				if( DaoType_MatchTo( A->xCdata.ctype, t, NULL ) ){
					DaoValue_Copy( A, C );
					return 1;
				}
			}
		}
		type = DaoNamespace_GetType( self->activeNamespace, A );
		DaoProcess_RaiseTypeError( self, type, t, "moving" );
		return 0;
	}
	return 1;
}

DaoValue* DaoProcess_SetValue( DaoProcess *self, ushort_t reg, DaoValue *value )
{
	DaoType *tp = self->activeTypes[reg];
	int res = DaoValue_Move( value, self->activeValues + reg, tp );
	if( res ) return self->activeValues[ reg ];
	return NULL;
}
DaoValue* DaoProcess_PutValue( DaoProcess *self, DaoValue *value )
{
	return DaoProcess_SetValue( self, self->activeCode->c, value );
}
int DaoProcess_PutReference( DaoProcess *self, DaoValue *refer )
{
	int tm, reg = self->activeCode->c;
	DaoValue **value = & self->activeValues[reg];
	DaoType *tp2, *tp = self->activeTypes[reg];

	if( *value == refer ) return 1;
	if( tp == NULL ){
		GC_ShiftRC( refer, *value );
		*value = refer;
		return 1;
	}
	tm = DaoType_MatchValue( tp, refer, NULL );
	if( tm == DAO_MT_EQ ){
		GC_ShiftRC( refer, *value );
		*value = refer;
		return 1;
	}
	if( DaoValue_Move( refer, value, tp ) == 0 ) goto TypeNotMatching;
	return 0;
TypeNotMatching:
	tp2 = DaoNamespace_GetType( self->activeNamespace, refer );
	DaoProcess_RaiseTypeError( self, tp2, tp, "referencing" );
	return 0;
}
void DaoProcess_DoMove( DaoProcess *self, DaoVmCode *vmc )
{
	DaoType *ct = self->activeTypes[ vmc->c ];
	DaoValue *A = self->activeValues[ vmc->a ];
	DaoValue *C = self->activeValues[ vmc->c ];
	int overload = 0;
	if( A == NULL ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "on null object" );
		return;
	}
	if( C ){
		if( A->type == C->type && C->type == DAO_OBJECT ){
			overload = DaoClass_ChildOf( A->xObject.defClass, (DaoValue*)C->xObject.defClass ) == 0;
		}else if( A->type == C->type && C->type == DAO_CDATA ){
			overload = DaoCdata_ChildOf( DaoValue_GetTyper(A), DaoValue_GetTyper(C) ) == 0;
		}else if( C->type == DAO_OBJECT || C->type == DAO_CDATA ){
			overload = 1;
		}
		if( overload ){
			DaoValue *rout = NULL;
			if( C->type == DAO_OBJECT ){
				DaoClass *scope = self->activeObject ? self->activeObject->defClass : NULL;
				rout = DaoClass_FindOperator( C->xObject.defClass, "=", scope );
			}else{
				rout = DaoTypeBase_FindFunctionMBS( C->xCdata.typer, "=" );
			}
			if( rout && DaoProcess_PushCallable( self, rout, C, & A, 1 ) == 0 ) return;
		}
	}
	DaoMoveAC( self, A, & self->activeValues[vmc->c], ct );
}
void DaoProcess_DoReturn( DaoProcess *self, DaoVmCode *vmc )
{
	DaoStackFrame *topFrame = self->topFrame;
	DaoType *type = self->abtype ? (DaoType*)self->abtype->aux : NULL;
	DaoValue **dest = self->stackValues;
	DaoValue *retValue = NULL;
	int i, returning = topFrame->returning;

	self->activeCode = vmc;
	//XXX if( DaoProcess_CheckFE( self ) ) return;

	if( topFrame->state & DVM_MAKE_OBJECT ){
		retValue = (DaoValue*)self->activeObject;
	}else if( vmc->b == 1 ){
		retValue = self->activeValues[ vmc->a ];
	}else if( vmc->b > 1 ){
		DaoTuple *tuple = DaoTuple_New( vmc->b );
		DaoValue **items = tuple->items;
		retValue = (DaoValue*) tuple;
		for(i=0; i<vmc->b; i++) DaoValue_Copy( self->activeValues[ vmc->a+i ], items + i );
	}else{
		return;
	}
	if( vmc->code == DVM_RETURN &&  returning != (ushort_t)-1 ){
		DaoStackFrame *lastframe = topFrame->prev;
		assert( lastframe && lastframe->routine );
		type = lastframe->routine->regType->items.pType[ returning ];
		dest = self->stackValues + lastframe->stackBase + returning;
	}
	if( retValue == NULL ){
		int opt1 = self->vmSpace->options & DAO_EXEC_INTERUN;
		int opt2 = self->activeNamespace->options & DAO_NS_AUTO_GLOBAL;
		int retnull = type == NULL || type->tid == DAO_UDF;
		if( retnull || self->vmSpace->evalCmdline || (opt1 && opt2) ) retValue = null;
	}
	if( DaoValue_Move( retValue, dest, type ) ==0 ){
		//printf( "retValue = %p %i %p %s\n", retValue, retValue->type, type, type->name->mbs );
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "invalid returned value" );
	}
}
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
DaoValue* DaoTypeCast( DaoProcess *proc, DaoType *ct, DaoValue *dA, DaoValue *dC, CastBuffer *b1, CastBuffer *b2 );
static void CastBuffer_Clear( CastBuffer *self )
{
	if( self->lng ) DLong_Delete( self->lng );
	if( self->str ) DString_Delete( self->str );
}
void DaoProcess_DoCast( DaoProcess *self, DaoVmCode *vmc )
{
	CastBuffer buffer1 = {NULL, NULL};
	CastBuffer buffer2 = {NULL, NULL};
	DaoType *at, *ct = self->activeTypes[ vmc->c ];
	DaoValue *va = self->activeValues[ vmc->a ];
	DaoValue *vc = self->activeValues[ vmc->c ];
	DaoValue **vc2 = self->activeValues + vmc->c;
	DaoValue value, *meth;
	DNode *node;
	int i, mt, mt2;

	self->activeCode = vmc;
	if( va == NULL ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "operate on null object" );
		return;
	}
	if( ct == NULL || ct->type == DAO_UDF || ct->type == DAO_ANY ){
		DaoValue_Copy( va, vc2 );
		return;
	}
	if( va == vc && vc->type == ct->type && ct->type < DAO_ENUM ) return;
	if( vc && vc->type == ct->type && va->type <= DAO_STRING ){
		if( va->type == ct->type ){
			DaoValue_Copy( va, vc2 );
			return;
		}
		if( va->type == DAO_STRING ){
			if( vc->type == DAO_LONG ){
				if( buffer1.lng == NULL ) buffer1.lng = DLong_New();
				vc->xLong.value = buffer1.lng;
			}
			if( ConvertStringToNumber( self, va, vc ) == 0 ) goto FailConversion;
			return;
		}
		switch( ct->type ){
		case DAO_INTEGER : vc->xInteger.value = DaoValue_GetInteger( va ); return;
		case DAO_FLOAT   : vc->xFloat.value = DaoValue_GetFloat( va ); return;
		case DAO_DOUBLE  : vc->xDouble.value = DaoValue_GetDouble( va ); return;
		case DAO_COMPLEX : vc->xComplex.value = DaoValue_GetComplex( va ); return;
		case DAO_LONG    : DaoValue_GetLong( va, vc->xLong.value ); return;
		case DAO_STRING  : DaoValue_GetString( va, vc->xString.data ); return;
		}
	}

	if( ct->tid == DAO_ENUM && (vc == NULL || vc->type != DAO_ENUM) ){
		DaoEnum *E = DaoEnum_New( NULL, 0 );
		GC_ShiftRC( E, vc );
		*vc2 = vc = (DaoValue*) E;
	}
	if( ct->tid == DAO_ENUM && va->type == DAO_ENUM ){
		DaoEnum_SetType( & vc->xEnum, ct );
		if( DaoEnum_SetValue( & vc->xEnum, & va->xEnum, NULL ) ==0 ) goto FailConversion;
		return;
	}else if( ct->tid == DAO_ENUM && va->type == DAO_INTEGER ){
		if( ct->mapNames == NULL ) goto FailConversion;
		for(node=DMap_First(ct->mapNames);node;node=DMap_Next(ct->mapNames,node)){
			if( node->value.pInt == va->xInteger.value ) break;
		}
		if( node == NULL ) goto FailConversion;
		DaoEnum_SetType( & vc->xEnum, ct );
		vc->xEnum.value = node->value.pInt;
		return;
	}else if( ct->tid == DAO_ENUM && va->type == DAO_STRING ){
		if( ct->mapNames == NULL ) goto FailConversion;
		node = DMap_Find( ct->mapNames, va->xString.data );
		if( node == NULL ) goto FailConversion;
		DaoEnum_SetType( & vc->xEnum, ct );
		vc->xEnum.value = node->value.pInt;
		return;
	}else if( ct->tid == DAO_ENUM ){
		goto FailConversion;
	}

	if( ct->tid == DAO_VARIANT ){
		at = NULL;
		mt = DAO_MT_NOT;
		for(i=0; i<ct->nested->size; i++){
			DaoType *tp = ct->nested->items.pType[i];
			mt2 = DaoType_MatchValue( tp, va, NULL );
			if( mt2 > mt ){
				mt = mt2;
				at = tp;
			}
		}
		if( at == NULL ) goto FailConversion;
		ct = at;
	}
	if( ct->tid == DAO_INTERFACE ){
		at = DaoNamespace_GetType( self->activeNamespace, va );
		/* automatic binding when casted to an interface: */
		mt = DaoInterface_BindTo( & ct->aux->xInterface, at, NULL, NULL );
	}
	mt = DaoType_MatchValue( ct, va, NULL );
	/* printf( "mt = %i, ct = %s\n", mt, ct->name->mbs ); */
	if( mt == DAO_MT_EQ || (mt && ct->tid == DAO_INTERFACE) ){
		DaoValue_Copy( va, vc2 );
		return;
	}
	if( va->type == DAO_OBJECT ){
		DaoClass *scope = self->activeObject ? self->activeObject->defClass : NULL;
		DaoValue *tpar = (DaoValue*) ct;
		meth = DaoClass_FindOperator( va->xObject.defClass, "cast", scope );
		if( meth && DaoProcess_PushCallable( self, meth, va, & tpar, 1 ) ==0 ) return;
	}else if( va->type == DAO_CDATA ){
		DaoValue *tpar = (DaoValue*) ct;
		meth = DaoTypeBase_FindFunctionMBS( va->xCdata.typer, "cast" );
		if( meth && DaoProcess_PushCallable( self, meth, va, & tpar, 1 ) ==0 ) return;
	}
NormalCasting:
	memset( & value, 0, sizeof(DaoValue) );
	va = DaoTypeCast( self, ct, va, & value, & buffer1, & buffer2 );
	if( va == NULL || va->type == 0 ) goto FailConversion;
	DaoValue_Copy( va, vc2 );
	CastBuffer_Clear( & buffer1 );
	CastBuffer_Clear( & buffer2 );
	return;
FailConversion :
	at = DaoNamespace_GetType( self->activeNamespace, self->activeValues[ vmc->a ] );
	DaoProcess_RaiseTypeError( self, at, ct, "casting" );
	CastBuffer_Clear( & buffer1 );
	CastBuffer_Clear( & buffer2 );
}
static int DaoProcess_TryAsynCall( DaoProcess *self, DaoVmCode *vmc )
{
#if( defined DAO_WITH_THREAD && defined DAO_WITH_ASYNCLASS )
	DaoStackFrame *frame = self->topFrame;
	DaoStackFrame *prev = frame->prev;
	if( vmc->code != DVM_MCALL ) return 0;
	if( frame->object && (frame->object->defClass->attribs & DAO_CLS_ASYNCHRONOUS) ){
		if( prev->object == NULL || frame->object->rootObject != prev->object->rootObject ){
			DaoNamespace *ns = self->activeNamespace;
			DaoFuture *future = DaoCallServer_AddCall( self );
			DaoType *retype = & frame->routine->routType->aux->xType;
			DaoType *type = DaoNamespace_MakeType( ns, "future", DAO_FUTURE, NULL, &retype,1 );
			GC_ShiftRC( type, future->unitype );
			future->unitype = type;
			DaoProcess_PopFrame( self );
			DaoProcess_PutValue( self, (DaoValue*) future );
			self->status = DAO_VMPROC_RUNNING;
			return 1;
		}
	}
#endif
	return 0;
}
static int DaoProcess_InitBase( DaoProcess *self, DaoVmCode *vmc, DaoValue *caller )
{
	int mode = vmc->b & 0xff00;
	if( (mode & DAO_CALL_INIT) && self->activeObject ){
		DaoClass *klass = self->activeObject->defClass;
		int init = self->activeRoutine->attribs & DAO_ROUT_INITOR;
		if( self->activeRoutine->routHost == klass->objType && init ){
			return DaoClass_FindSuper( klass, caller );
		}
	}
	return -1;
}
static void DaoProcess_PrepareCall( DaoProcess *self, DaoRoutine *rout, 
		DaoValue *selfpar, DaoValue *P[], int N, DaoVmCode *vmc )
{
	int i, M = DRoutine_PassParams( (DRoutine*)rout, selfpar, self->freeValues, P, N, vmc->code );
	if( M ==0 ){
		DaoProcess_RaiseException( self, DAO_ERROR_PARAM, "not matched (passing)" );
		return;
	}
	/* no tail call inside try{} */
	if( (vmc->b & DAO_CALL_TAIL) && self->topFrame->depth <=1 ){
		if( self->topFrame->state == 0 ){ /* No tail call in constructors etc.: */
			DaoValue **params = self->freeValues;
			DaoProcess_PopFrame( self );
			for(i=0; i<rout->parCount; i++){
				DaoValue *value = self->freeValues[i];
				self->freeValues[i] = params[i];
				params[i] = value;
			}
		}
	}
	DaoProcess_PushRoutine( self, rout, DaoValue_CastObject( selfpar ) );//, code );
	self->topFrame->parCount = M - 1;
}
static void DaoProcess_DoCxxCall( DaoProcess *self, DaoVmCode *vmc,
		DaoFunction *func, DaoValue *selfpar, DaoValue *P[], int N )
{
	int status, code = vmc->code;
	if( (self->vmSpace->options & DAO_EXEC_SAFE) && (func->attribs & DAO_ROUT_EXTFUNC) ){
		/* normally this condition will not be satisfied.
		 * it is possible only if the safe mode is set in C codes
		 * by embedding or extending. */
		DaoProcess_RaiseException( self, DAO_ERROR, "not permitted" );
		return;
	}
	if( DRoutine_PassParams( (DRoutine*)func, selfpar, self->freeValues, P, N, code ) ==0 ){
		//rout2 = (DRoutine*) rout;
		DaoProcess_ShowCallError( self, (DRoutine*)func, selfpar, P, N, code );
		return;
		//goto InvalidParameter;
	}
	/* foo: routine<x:int,s:string>
	 *   ns.foo( 1, "" );
	 * bar: routine<self:cdata,x:int>
	 *   obj.bar(1);
	 * inside: Dao class member method:
	 *   bar(1); # pass Dao class instances as self
	 */
	if( vmc->code == DVM_MCALL && ! (func->attribs & DAO_ROUT_PARSELF)) N --;
	/*
	   printf( "call: %s %i\n", func->routName->mbs, N );
	 */
	DaoProcess_PushFunction( self, func );
	DaoProcess_CallFunction( self, func, self->stackValues + self->topFrame->stackBase, N );
	status = self->status;
	DaoProcess_PopFrame( self );

	//XXX if( DaoProcess_CheckFE( self ) ) return;
	if( status == DAO_VMPROC_SUSPENDED ){
		self->topFrame->entry = (short)(vmc - self->topFrame->codes);
		self->status = status;
	}
}
static void DaoProcess_DoNewCall( DaoProcess *self, DaoVmCode *vmc,
		DaoClass *klass, DaoValue *selfpar, DaoValue *params[], int npar )
{
	DRoutine *rout;
	DaoFunctree *routines = klass->classRoutines;
	DaoObject *obj, *othis = NULL, *onew = NULL;
	int i, code = vmc->code;
	int mode = vmc->b & 0xff00;
	int codemode = code | (mode<<16);
	int initbase = DaoProcess_InitBase( self, vmc, (DaoValue*) klass );
	if( initbase >= 0 ){
		othis = self->activeObject;
	}else{
		othis = onew = DaoObject_New( klass );
	}
	rout = DRoutine_Resolve( (DaoValue*)routines, selfpar, params, npar, codemode );
	if( rout == NULL ){
		selfpar = (DaoValue*) othis;
		rout = DRoutine_Resolve( (DaoValue*)routines, selfpar, params, npar, codemode );
	}
	if( rout == NULL && (npar ==0 || (npar == 1 && code == DVM_MCALL) ) ){
		/* default contstructor */
		rout = (DRoutine*) klass->classRoutine;
	}
	if( rout == NULL ){
		//rout2 = (DRoutine*) klass->classRoutine;
		//XXX goto InvalidParameter;
		return;
	}
	if( rout->type == DAO_FUNCTION ){
		DaoFunction *func = (DaoFunction*) rout;
		npar = DRoutine_PassParams( rout, selfpar, self->freeValues, params, npar, vmc->code );
		if( npar == 0 ){
			//rout2 = (DRoutine*) rout;
			//XXX goto InvalidParameter;
			return;
		}
		DaoProcess_PushFunction( self, func );
		DaoProcess_SetActiveFrame( self, self->firstFrame ); /* return value in stackValues[0] */
		self->topFrame->active = self->firstFrame;
		DaoProcess_CallFunction( self, func, self->stackValues + self->topFrame->stackBase, npar );
		DaoProcess_PopFrame( self );

		if( self->stackValues[0] && self->stackValues[0]->type == DAO_CDATA ){
			DaoCdata *cdata = & self->stackValues[0]->xCdata;
			DaoObject_SetParentCdata( othis, cdata );
			GC_ShiftRC( othis, cdata->object );
			cdata->object = othis;
		}
		DaoProcess_PutValue( self, (DaoValue*) othis );
	}else if( rout != NULL ){
		DaoProcess_PrepareCall( self, (DaoRoutine*)rout, selfpar, params, npar, vmc );
		obj = othis;
		if( initbase >= 0 ){
			obj = (DaoObject*) DaoObject_MapThisObject( obj, rout->routHost );
		}else{
			self->topFrame->state = DVM_MAKE_OBJECT;
		}
		GC_ShiftRC( obj, self->topFrame->object );
		self->topFrame->object = obj;
	}else{
		if( onew ){ GC_IncRC( onew ); GC_DecRC( onew ); }
		DaoProcess_RaiseException( self, DAO_ERROR_PARAM, "not matched (class)" );
	}
}
void DaoProcess_DoCall2( DaoProcess *self, DaoVmCode *vmc )
{
	DRoutine *rout = NULL;
	DaoValue *selfpar = NULL;
	DaoValue *parbuf[DAO_MAX_PARAM+1];
	DaoValue **params = self->activeValues + vmc->a + 1;
	DaoValue *caller = self->activeValues[ vmc->a ];
	int mcall = vmc->code == DVM_MCALL;
	int mode = vmc->b & 0xff00;
	int npar = vmc->b & 0xff;
	int i, n = 0;

	if( npar == 0 && (mode & DAO_CALL_EXPAR) ){ /* call with caller's parameter */
		int m = (self->activeRoutine->routType->attrib & DAO_TYPE_SELF) != 0;
		npar = self->topFrame->parCount - m;
		params = self->activeValues + m;
		mode &= ~DAO_CALL_EXPAR;
	}
	if( self->activeObject && mcall == 0 ) selfpar = (DaoValue*) self->activeObject;
	if( caller->type == DAO_FUNCURRY ){
		DaoFunCurry *curry = (DaoFunCurry*) caller;
		caller = curry->callable;
		selfpar = curry->selfobj;
		for(i=0; i<curry->params->size; i++) parbuf[n++] = curry->params->items.pValue[i];
	}
	for(i=mcall; i<npar; i++) parbuf[n++] = params[i];
	if( mode & DAO_CALL_EXPAR ){
		if( npar > mcall && params[npar-1]->type == DAO_TUPLE ){
			DaoTuple *tup = & params[npar-1]->xTuple;
			n -= 1;
			for(i=0; i<tup->size; i++) parbuf[n++] = tup->items[i];
		}
	}
	params = parbuf;
	npar = n;
	if( caller->type == DAO_FUNCTREE ){
		DaoFunctree *mroutine = & caller->xFunctree;
		rout = DRoutine_Resolve( (DaoValue*)mroutine, selfpar, params, npar, DVM_CALL );
	}else if( caller->type == DAO_ROUTINE || caller->type == DAO_FUNCTION ){
		rout = (DRoutine*) caller;
	}
	if( rout == NULL ) goto InvalidParameter;
	if( rout->type == DAO_ROUTINE ){
		DaoProcess_PrepareCall( self, (DaoRoutine*)rout, selfpar, params, npar, vmc );
		if( DaoProcess_TryAsynCall( self, vmc ) ) return;
	}else if( rout->type == DAO_FUNCTION ){
		DaoFunction *func = (DaoFunction*) rout;
		DaoProcess_DoCxxCall( self, vmc, func, selfpar, params, npar );
	}else{
		DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "object not callable" );
	}
	return;
InvalidParameter:
	DaoProcess_ShowCallError( self, (DRoutine*)caller, selfpar, params, npar, DVM_CALL );
}
DaoRoutine* DaoRoutine_Decorate( DaoRoutine *self, DaoRoutine *decoFunc, DaoValue *p[], int n );
void DaoProcess_DoCall( DaoProcess *self, DaoVmCode *vmc )
{
	int sup = 0, code = vmc->code;
	int mcall = code == DVM_MCALL;
	int mode = vmc->b & 0xff00;
	int npar = vmc->b & 0xff;
	int codemode = code | (mode<<16);
	DaoValue *selfpar = NULL;
	DaoValue *caller = self->activeValues[ vmc->a ];
	DaoValue **params = self->activeValues + vmc->a + 1;
	DRoutine *rout, *rout2 = NULL;
	DaoFunctree *mroutine;
	DaoFunction *func;

	self->activeCode = vmc;
	if( caller->type ==0 ){
		DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "null object not callable" );
		return;
	}
	if( self->activeObject && mcall == 0 ) selfpar = (DaoValue*) self->activeObject;
	if( mode & DAO_CALL_COROUT ){
		DaoProcess *vmp = DaoProcess_Create( self, self->activeValues + vmc->a, npar+1 );
		if( vmp == NULL ) return;
		GC_ShiftRC( self->activeTypes[ vmc->c ], vmp->abtype );
		vmp->abtype = self->activeTypes[ vmc->c ];
		DaoProcess_PutValue( self, (DaoValue*) vmp );
	}else if( caller->type == DAO_FUNCURRY || (mode & DAO_CALL_EXPAR) ){
		DaoProcess_DoCall2( self, vmc );
	}else if( caller->type == DAO_FUNCTION ){
		DaoProcess_DoCxxCall( self, vmc, & caller->xFunction, selfpar, params, npar );
	}else if( caller->type == DAO_ROUTINE ){
		rout = DRoutine_Resolve( caller, selfpar, params, npar, codemode );
		if( rout == NULL ) goto InvalidParameter;
#ifdef DAO_WITH_DECORATOR
		if( rout->routName->mbs[0] == '@' ){
			DaoRoutine *drout = (DaoRoutine*) rout;
			drout = DaoRoutine_Decorate( & params[0]->xRoutine, drout, params+1, npar-1 );
			DaoProcess_PutValue( self, (DaoValue*) drout );
			return;
		}
#else
		DaoProcess_RaiseException( self, DAO_ERROR, getCtInfo( DAO_DISABLED_DECORATOR ) );
#endif
		DaoProcess_PrepareCall( self, (DaoRoutine*)rout, selfpar, params, npar, vmc );
		if( DaoProcess_TryAsynCall( self, vmc ) ) return;
	}else if( caller->type == DAO_FUNCTREE ){
		rout = DRoutine_Resolve( caller, selfpar, params, npar, codemode );
		if( rout == NULL ){
			rout2 = (DRoutine*) caller;
			goto InvalidParameter;
		}
		if( rout->type == DAO_ROUTINE ){
#ifdef DAO_WITH_DECORATOR
			if( rout->routName->mbs[0] == '@' ){
				DaoRoutine *drout = (DaoRoutine*) rout;
				drout = DaoRoutine_Decorate( & params[0]->xRoutine, drout, params+1, npar-1 );
				DaoProcess_PutValue( self, (DaoValue*) drout );
				return;
			}
#else
			DaoProcess_RaiseException( self, DAO_ERROR, getCtInfo( DAO_DISABLED_DECORATOR ) );
#endif
			DaoProcess_PrepareCall( self, (DaoRoutine*)rout, selfpar, params, npar, vmc );
			if( DaoProcess_TryAsynCall( self, vmc ) ) return;
		}else if( rout->type == DAO_FUNCTION ){
			func = (DaoFunction*) rout;
			DaoProcess_DoCxxCall( self, vmc, func, selfpar, params, npar );
		}else{
			DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "object not callable" );
		}
	}else if( caller->type == DAO_CLASS ){
		DaoProcess_DoNewCall( self, vmc, & caller->xClass, selfpar, params, npar );
	}else if( caller->type == DAO_OBJECT ){
		DaoClass *host = self->activeObject ? self->activeObject->defClass : NULL;
		rout = (DRoutine*) DaoClass_FindOperator( caller->xObject.defClass, "()", host );
		if( rout == NULL ){
			DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "class instance not callable" );
			return;
		}
		rout = DRoutine_Resolve( (DaoValue*)rout, caller, params, npar, codemode );
		if( rout == NULL ){
			return; //XXX
		}
		if( rout->type == DAO_ROUTINE ){
			DaoProcess_PrepareCall( self, (DaoRoutine*)rout, selfpar, params, npar, vmc );
			if( DaoProcess_TryAsynCall( self, vmc ) ) return;
		}else if( rout->type == DAO_FUNCTION ){
			func = (DaoFunction*) rout;
			DaoProcess_DoCxxCall( self, vmc, func, caller, params, npar );
		}
	}else if( caller->type == DAO_CTYPE ){
		DaoType *ctype = caller->xCdata.ctype;
		rout = (DRoutine*) DaoTypeBase_FindFunction( caller->xCdata.typer, ctype->name );
		if( rout == NULL ){
			DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "C type not callable" );
			return;
		}
		rout = DRoutine_Resolve( (DaoValue*)rout, selfpar, params, npar, codemode );
		if( rout == NULL || rout->type != DAO_FUNCTION ){
			// XXX
			return;
		}
		DaoProcess_DoCxxCall( self, vmc, (DaoFunction*) rout, selfpar, params, npar );
		// XXX handle fail
		sup = DaoProcess_InitBase( self, vmc, caller );
		if( caller->type == DAO_CTYPE && sup >= 0 ){
			DaoCdata *cdata = & self->activeValues[ vmc->c ]->xCdata;
			if( cdata && cdata->type == DAO_CDATA ){
				GC_ShiftRC( cdata, self->activeObject->parents[sup] );
				self->activeObject->parents[sup] = (DaoValue*) cdata;
				GC_ShiftRC( self->activeObject->rootObject, cdata->object );
				cdata->object = self->activeObject->rootObject;
			}
		}
	}else if( caller->type == DAO_CDATA ){
		rout = (DRoutine*)DaoTypeBase_FindFunctionMBS( caller->xCdata.typer, "()" );
		if( rout == NULL ){
			DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "C object not callable" );
			return;
		}
		rout = DRoutine_Resolve( (DaoValue*)rout, selfpar, params, npar, codemode );
		if( rout == NULL || rout->type != DAO_FUNCTION ){
			// XXX
			return;
		}
		DaoProcess_DoCxxCall( self, vmc, (DaoFunction*) rout, selfpar, params, npar );
	}else if( caller->type == DAO_PROCESS && caller->xProcess.abtype ){
		DaoProcess *vmProc = & caller->xProcess;
		if( vmProc->status == DAO_VMPROC_FINISHED ){
			DaoProcess_RaiseException( self, DAO_WARNING, "coroutine execution is finished." );
			return;
		}
		DaoProcess_Resume( vmProc, params, npar, self );
		if( vmProc->status == DAO_VMPROC_ABORTED )
			DaoProcess_RaiseException( self, DAO_ERROR, "coroutine execution is aborted." );
	}else{
		DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "object not callable" );
	}
	return;
InvalidParameter:
	DaoProcess_ShowCallError( self, rout2, selfpar, params, npar, code );
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
