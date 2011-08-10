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


extern DaoList*  DaoContext_GetList( DaoContext *self, DaoVmCode *vmc );

extern void DaoContext_DoList(  DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoMap(   DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoArray(   DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoMatrix( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoCurry(  DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoPair( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoTuple( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoCheck( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_BindNameValue( DaoContext *self, DaoVmCode *vmc );

extern void DaoContext_DoGetItem( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoSetItem( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoGetField( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoSetField( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoGetMetaField( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoSetMetaField( DaoContext *self, DaoVmCode *vmc );

extern DaoVmCode* DaoContext_DoSwitch( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoIter( DaoContext *self, DaoVmCode *vmc );

extern int DaoContext_TryObjectArith( DaoContext *self, DaoValue *dA, DaoValue *dB );
extern int DaoContext_TryCDataArith( DaoContext *self, DaoValue *dA, DaoValue *dB );

extern void DaoContext_DoInTest( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoBinArith( DaoContext *self, DaoVmCode *vmc );
/* binary operation with boolean result. */
extern void DaoContext_DoBinBool(  DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoUnaArith( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoBitLogic( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoBitShift( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoBitFlip( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoBitFlip( DaoContext *self, DaoVmCode *vmc );

extern void DaoContext_DoCast( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoMove( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_DoCall( DaoContext *self, DaoVmCode *vmc );

static void DaoContext_DoFunctional( DaoContext *self, DaoVmCode *vmc );

/* if return TRUE, there is exception, and look for the next rescue point. */
extern int DaoContext_DoCheckExcept( DaoContext *self, DaoVmCode *vmc );
/* if return DAO_STATUS_EXCEPTION, real exception is rose, and look for the next rescue point. */
extern void DaoContext_DoRaiseExcept( DaoContext *self, DaoVmCode *vmc );
/* return TRUE, if some exceptions can be rescued */
extern int DaoContext_DoRescueExcept( DaoContext *self, DaoVmCode *vmc );

extern void DaoContext_DoReturn( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_MakeRoutine( DaoContext *self, DaoVmCode *vmc );
extern void DaoContext_MakeClass( DaoContext *self, DaoVmCode *vmc );

static int DaoVM_DoMath( DaoContext *self, DaoVmCode *vmc, DaoValue *c, DaoValue *p );

static DaoVmFrame* DaoVmFrame_New()
{
	DaoVmFrame *self = dao_calloc( 1, sizeof(DaoVmFrame) );
	return self;
}
#define DaoVmFrame_Delete( p ) dao_free( p )
static void DaoVmFrame_PushRange( DaoVmFrame *self, ushort_t from, ushort_t to )
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
	(FuncPtrDel) DaoVmProcess_Delete, NULL
};

DaoVmProcess* DaoVmProcess_New( DaoVmSpace *vms )
{
	DaoVmProcess *self = (DaoVmProcess*)dao_malloc( sizeof( DaoVmProcess ) );
	DaoValue_Init( self, DAO_VMPROCESS );
	self->vmSpace = vms;
	self->stopit = 0;
	self->status = DAO_VMPROC_SUSPENDED;
	self->firstFrame = NULL;
	self->topFrame = NULL;
	self->returned = NULL;
	self->parResume = NULL;
	self->parYield = NULL;
	self->abtype = NULL;
	self->future = NULL;
	self->exceptions = DArray_New(D_VALUE);

	self->mbstring = DString_New(1);
	self->mbsRegex = NULL;
	self->wcsRegex = NULL;
	self->pauseType = 0;
	return self;
}

static void DaoVmProcess_CleanProcess( DaoVmProcess *self );

void DaoVmProcess_Delete( DaoVmProcess *self )
{
	DaoVmFrame *frame = self->firstFrame;
	DNode *n;
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
		DaoVmFrame *p = frame;
		frame = frame->next;
		if( p->context ) GC_DecRC( p->context );
		dao_free( p );
	}

	DString_Delete( self->mbstring );
	DaoValue_Clear( & self->returned );
	DArray_Delete( self->exceptions );
	if( self->parResume ) DArray_Delete( self->parResume );
	if( self->parYield ) DArray_Delete( self->parYield );
	if( self->abtype ) GC_DecRC( self->abtype );
	dao_free( self );
}

DaoRegex* DaoVmProcess_MakeRegex( DaoVmProcess *self, DString *src, int mbs )
{
	DaoContext *ctx = NULL;
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
	if( self->topFrame == NULL || self->topFrame == self->firstFrame )
		ctx = self->topFrame->context;
	for( i=0; i<pat->count; i++ ){
		it = pat->items + i;
		if( it->type ==0 ){
			char buf[50];
			sprintf( buf, "incorrect pattern, at char %i.", it->length );
			if( ctx ) DaoContext_RaiseException( ctx, DAO_ERROR, buf );
			return NULL;
		}
	}
	return pat;
}

static DaoVmFrame* DaoVmProcess_NextFrame( DaoVmProcess *self )
{
	DaoVmFrame *frame = NULL;
	if( self->topFrame && self->topFrame->next ) return self->topFrame->next;

	frame = DaoVmFrame_New();
	if( self->topFrame ){
		self->topFrame->next = frame;
		frame->prev = self->topFrame;
	}
	if( self->firstFrame == NULL ){
		self->firstFrame = frame;
		self->topFrame = frame;
		frame->rollback = frame;
		/* the first frame will not be used for execution */
		return DaoVmProcess_NextFrame( self );
	}
	return frame;
}
DaoContext* DaoVmProcess_MakeContext( DaoVmProcess *self, DaoRoutine *routine )
{
	DaoVmFrame *frame = DaoVmProcess_NextFrame( self );
	DaoContext *ctx = frame->context;
	if( ctx && ctx->refCount >1 ){
		/* this should never happen in the current implementation.
		 * it is added just in case if something may change in the future. */
		/* this may actually happen, for asynchronous method call */
		GC_DecRC( ctx );
		ctx = frame->context = NULL;
	}
	if( ctx == NULL ){
		ctx = DaoContext_New();
		GC_IncRC( ctx );
		ctx->process = self;
		ctx->vmSpace = self->vmSpace;
		ctx->frame = frame;
		frame->context = ctx;
	}
	DaoContext_Init( ctx, routine );
	return ctx;
}
void DaoVmProcess_PushContext( DaoVmProcess *self, DaoContext *ctx )
{
	DaoContext *topCtx = NULL;
	DaoVmFrame *frame = DaoVmProcess_NextFrame( self );
	/* assert( frame->context == ctx ); */
	if( frame->context != ctx ){
		/* this should never happen in the current implementation.
		 * it is added just in case if something may change in the future. */
		DaoVmFrame *next = frame;
		self->topFrame->next = NULL; /* cause to add a new empty frame */
		frame = DaoVmProcess_NextFrame( self );
		next->prev = frame; /* append frames from "next" to the new frame */
		frame->next = next;
		frame->context = ctx;
		GC_IncRC( ctx );
		ctx->frame = frame;
		ctx->process = self;
		ctx->vmSpace = self->vmSpace;
	}
	frame->entry = ctx->entryCode;
	frame->state = ctx->ctxState;
	frame->rollback = NULL;
	frame->returning = -1;
	self->status = DAO_VMPROC_STACKED;
	if( self->topFrame != self->firstFrame ){
		topCtx = self->topFrame->context;
		if( topCtx->vmc ){ /* maybe topCtx is pushed in, but never executed */
			self->topFrame->entry = (int)( topCtx->vmc - topCtx->codes ) + 1;
			frame->returning = topCtx->vmc->c;
		}
	}
	if( ctx != topCtx ) ctx->caller = topCtx;
	self->topFrame = frame;
}
void DaoVmProcess_PopContext( DaoVmProcess *self )
{
	DaoContext *ctx = self->topFrame->context;
	DaoValue **values;
	int i, N;
	if( self->topFrame == self->firstFrame ) return;
	if( self->topFrame->context == NULL ) return;
	if( ctx->refCount == 1 ){ /* only referenced once, and by the stack */
		N = self->topFrame->context->routine->regCount;
		values = self->topFrame->context->regValues;
		for(i=0; i<N; i++){
			switch( values[i]->type ){
			case DAO_LONG  : case DAO_STRING :
			case DAO_ARRAY : case DAO_LIST : case DAO_MAP :
			case DAO_TUPLE : case DAO_OBJECT : case DAO_CDATA :
				DaoValue_Clear( & values[i] );
				break;
			}
		}
	}
	self->topFrame->depth = 0;
	self->topFrame = self->topFrame->prev;
}
void DaoVmProcess_PushRoutine( DaoVmProcess *self, DaoRoutine *routine )
{
	DaoContext *ctx = DaoVmProcess_MakeContext( self, routine );
	DaoVmProcess_PushContext( self, ctx );
}
static void DaoVmProcess_CleanProcess( DaoVmProcess *self )
{
	while( self->topFrame && self->topFrame->rollback != self->topFrame )
		DaoVmProcess_PopContext( self );
	DaoVmProcess_PopContext( self );
}
int DaoVmProcess_Resume( DaoVmProcess *self, DaoValue *par[], int N, DaoList *list )
{
	if( self->status == DAO_VMPROC_SUSPENDED && self->parResume ){
		int i;
		DArray_Clear( self->parResume );
		for( i=0; i<N; i++ ) DArray_Append( self->parResume, par[i] );

		DaoVmProcess_Execute( self );
		if( list )
			for( i=0; i<self->parYield->size; i++ )
				DaoList_Append( list, self->parYield->items.pValue[i] );
	}else if( self->status == DAO_VMPROC_SUSPENDED && self->pauseType == DAO_VMP_ASYNC ){
		DaoVmProcess_Execute( self );
	}
	return self->status;
}
void DaoVmProcess_Yield( DaoVmProcess *self, DaoValue *par[], int N, DaoList *list )
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
void DaoContext_MakeTuple( DaoContext *self, DaoTuple *tuple, DaoValue *its[], int N );
int DaoVmProcess_Resume2( DaoVmProcess *self, DaoValue *par[], int N, DaoContext *ret )
{
	DaoContext *ctx = self->topFrame->context;
	DaoType *tp;
	DaoVmCode *vmc;
	DaoTuple *tuple;
	if( self->status != DAO_VMPROC_SUSPENDED ) return 0;
	if( ctx->vmc && ctx->vmc->code == DVM_YIELD ){
		tp = ctx->regTypes[ ctx->vmc->c ];
		if( N == 1 ){
			DaoContext_PutValue( ctx, par[0] );
		}else if( N ){
			tuple = DaoTuple_New( N );
			tuple->unitype = tp;
			GC_IncRC( tuple->unitype );
			DaoContext_MakeTuple( ctx, tuple, par, N );
			DaoContext_PutValue( ctx, (DaoValue*) tuple );
		}
		self->topFrame->entry ++;
	}else if( N && ! DRoutine_PassParams( (DRoutine*)ctx->routine, NULL,
				ctx->regValues, par, N, DVM_CALL ) ){
		DaoContext_RaiseException( ret, DAO_ERROR, "invalid parameters." );
		return 0;
	}
	DaoVmProcess_Execute( self );
	if( self->topFrame && self->topFrame != self->firstFrame ){ /* yield */
		ctx = self->topFrame->context;
		vmc = ctx->vmc;
		tp = ret->regTypes[ ret->vmc->c ];
		if( vmc->b ==1 ){
			DaoContext_PutValue( ret, ctx->regValues[ vmc->a ] );
		}else if( vmc->b ){
			tuple = DaoTuple_New( vmc->b );
			tuple->unitype = tp;
			GC_IncRC( tuple->unitype );
			DaoContext_MakeTuple( ret, tuple, ctx->regValues + vmc->a, vmc->b );
			DaoContext_PutValue( ret, (DaoValue*) tuple );
		}
	}else{ /* return */
		DaoContext_PutValue( ret, self->returned );
	}
	return 1;
}

int DaoVmProcess_ExecuteSection( DaoVmProcess *self, int entry )
{
	ushort_t ranges[DVM_MAX_TRY_DEPTH][2];
	int depth = self->topFrame->depth;
	int ret = 0, old = 0;
	if( entry <0 ) return DAO_VMPROC_ABORTED;
	if( self->topFrame->context->codes[entry].code != DVM_SECT ) return DAO_VMPROC_ABORTED;
	old = self->topFrame->state;
	self->topFrame->entry = entry + 1;
	self->topFrame->state = DVM_SPEC_RUN;
	self->topFrame->depth = 0;
	memcpy( ranges, self->topFrame->ranges, 2*DVM_MAX_TRY_DEPTH*sizeof(short) );
	ret = DaoVmProcess_Execute( self );
	memcpy( self->topFrame->ranges, ranges, 2*DVM_MAX_TRY_DEPTH*sizeof(short) );
	self->topFrame->depth = depth;
	self->topFrame->state = old;
	return ret;
}
int DaoVmProcess_Compile( DaoVmProcess *self, DaoNameSpace *ns, DString *src, int rpl )
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
	DaoParser_Delete( p );
	DString_Delete( src );
	return res;
}
int DaoVmProcess_Eval( DaoVmProcess *self, DaoNameSpace *ns, DString *source, int rpl )
{
	DaoRoutine *rout;
	if( DaoVmProcess_Compile( self, ns, source, rpl ) ==0 ) return 0;
	rout = ns->mainRoutines->items.pRout[ ns->mainRoutines->size-1 ];
	if( DaoVmProcess_Call( self, (DaoMethod*) rout, NULL, NULL, 0 ) ==0 ) return 0;
	return ns->mainRoutines->size;
}
int DaoVmProcess_Call( DaoVmProcess *self, DaoMethod *f, DaoValue *o, DaoValue *p[], int n )
{
	DaoValue *r = (DaoValue*) f;
	DRoutine *rout = (DRoutine*) r;
	DaoContext *ctx;

	if( r && r->type == DAO_FUNCTREE ) rout = DRoutine_Resolve( r, o, p, n, DVM_CALL );
	if( rout == NULL ) return 0;
	if( rout->type == DAO_FUNCTION ){
		DaoVmCode vmc = { 0, 0, 0, 0 };
		uchar_t mode = 0;
		ctx = DaoVmProcess_MakeContext( self, (DaoRoutine*) rout );
		if( ctx->regArray->size ==0 ) DTuple_Resize( ctx->regArray, 1, NULL );
		ctx->regValues = ctx->regArray->items.pValue;
		ctx->regTypes = & dao_type_any;
		ctx->regModes = & mode;
		ctx->vmc = & vmc;
		return DaoFunction_Call( (DaoFunction*) rout, ctx, o, p, n ) ==0;
	}

	ctx = DaoVmProcess_MakeContext( self, (DaoRoutine*) rout );
	if( o && o->type == DAO_OBJECT ){
		GC_ShiftRC( o, ctx->object );
		ctx->object = & o->xObject;
	}
	if( DRoutine_PassParams( rout, o, ctx->regValues, p, n, DVM_CALL ) ==0 ){
		printf( "calling %s failed\n", rout->routName->mbs );
		return 0;
	}
	/*
	   if( db ) printf( "%i  %p\n", db->refCount, db );
	 */
	DaoVmProcess_PushContext( self, ctx );
	/* no return value to the previous stack top context */
	self->topFrame->returning = -1;
	return DaoVmProcess_Execute( self );
}
void DaoVmProcess_Stop( DaoVmProcess *self )
{
	self->stopit = 1;
}
DaoValue* DaoVmProcess_GetReturned( DaoVmProcess *self )
{
	return self->returned;
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

int DaoContext_CheckFE( DaoContext *self );

void DaoContext_AdjustCodes( DaoContext *self, int options );
int DaoMoveAC( DaoContext *self, DaoValue *A, DaoValue **C, DaoType *t );

#if defined( __GNUC__ ) && !defined( __STRICT_ANSI__ )
#define HAS_VARLABEL
#endif
#if 0
#endif

int DaoVmProcess_Execute( DaoVmProcess *self )
{
	DaoUserHandler *handler = self->vmSpace->userHandler;
	DaoVmSpace *vmSpace = self->vmSpace;
	DaoVmCode *vmc=NULL;
	DaoVmCode *vmcBase;
	DaoVmFrame *topFrame;
	DaoContext *topCtx;
	DaoRoutine *routine;
	DaoNameSpace *here = NULL;
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
	llong_t lnum = 0;
	uchar_t *regModes = NULL;
	ushort_t *range;
	complex16 acom, bcom;
	DaoVmFrame *base;

#ifdef HAS_VARLABEL
	const void *labels[] = {
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
#define OPBEGIN() for(;;){ printf("%3i:", (i=vmc-vmcBase) ); DaoVmCodeX_Print( *topCtx->routine->annotCodes->items.pVmc[i], NULL ); switch( vmc->code )
#endif

#endif


	if( self->topFrame == NULL ) goto ReturnFalse;
	base = self->topFrame;
	self->topFrame->rollback = base;
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
	topCtx = topFrame->context;
	routine = topCtx->routine;
#if 0
	if( (vmSpace->options & DAO_EXEC_SAFE) && self->topFrame->index >= 100 ){
		DaoContext_RaiseException( topCtx, DAO_ERROR,
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
	if( invokehost ) handler->InvokeHost( handler, topCtx );

	if( (vmSpace->options & DAO_EXEC_DEBUG)|(routine->mode & DAO_EXEC_DEBUG) )
		DaoContext_AdjustCodes( topCtx, vmSpace->options );

	topCtx->vmSpace = vmSpace;
	vmcBase = topCtx->codes;
	id = self->topFrame->entry;
	if( id >= routine->vmCodes->size ){
		if( id == 0 ){
			DString_SetMBS( self->mbstring, "Not implemented function, " );
			DString_Append( self->mbstring, routine->routName );
			DString_AppendMBS( self->mbstring, "()" );
			DaoContext_RaiseException( topCtx, DAO_ERROR, self->mbstring->mbs );
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

	self->stopit = 0;
	vmc = vmcBase + id;
	topCtx->vmc = vmc;
	if( id ==0 ){
		/* ( 0, routine->vmCodes->size-1 ) */
		DaoVmFrame_PushRange( topFrame, 0, (routine->vmCodes->size-1) );
	}

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
			DaoList *list = DaoContext_GetList( topCtx, vmc );
			for(i=0; i<self->parResume->size; i++)
				DaoList_Append( list, self->parResume->items.pValue[i] );
		}else if( self->pauseType == DAO_VMP_ASYNC && self->future->precondition ){
			DaoContext_PutValue( topCtx, self->future->precondition->value );
		}
		vmc ++;
	}
	self->status = DAO_VMPROC_RUNNING;
	self->pauseType = DAO_VMP_NOPAUSE;
	host = NULL;
	here = routine->nameSpace;
	this = topCtx->object;
	locVars = topCtx->regValues;
	locTypes = routine->regType->items.pType;
	regModes = (uchar_t*) routine->regMode->mbs;
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
		dataVL = routine->upContext->regValues;
		typeVL = routine->upRoutine->regType;
	}

	OPBEGIN(){
		OPCASE( NOP ){
			if( self->stopit | vmSpace->stopit ) goto FinishProc;
		}OPNEXT()
		OPCASE( DATA ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			switch( vmc->a ){
			case DAO_COMPLEX :
				ComplexOperand( vmc->c ).real = 0.0;
				ComplexOperand( vmc->c ).imag = vmc->b;
				break;
			case DAO_NULL : locVars[ vmc->c ] = null; break;
			case DAO_INTEGER : IntegerOperand( vmc->c ) = vmc->b; break;
			case DAO_FLOAT : FloatOperand( vmc->c ) = vmc->b; break;
			case DAO_DOUBLE : DoubleOperand( vmc->c ) = vmc->b; break;
			default : break;
			}
		}OPNEXT()
		OPCASE( GETCL ){
			/* ensure no copying here: */
			locVars[ vmc->c ] = dataCL[ vmc->a ]->items.pValue[ vmc->b ];
		}OPNEXT()
		OPCASE( GETCK ){
			/* ensure no copying here: */
			locVars[ vmc->c ] = dataCK->items.pArray[ vmc->a ]->items.pValue[ vmc->b ];
		}OPNEXT()
		OPCASE( GETCG ){
			value = dataCG->items.pArray[ vmc->a ]->items.pValue[ vmc->b ];
			if( regModes[ vmc->c ] == DAO_REG_REFER ){
				/* ensure no copying here: */
				locVars[ vmc->c ] = value;
			}else{
				DaoValue_Copy( value, & locVars[ vmc->c ] );
			}
		}OPNEXT()
		OPCASE( GETVL ){
			locVars[ vmc->c ] = dataVL[ vmc->b ];
		}OPNEXT()
		OPCASE( GETVO ){
			locVars[ vmc->c ] = dataVO[ vmc->b ];
		}OPNEXT()
		OPCASE( GETVK ){
			locVars[ vmc->c ] = dataVK->items.pArray[vmc->a]->items.pValue[ vmc->b ];
		}OPNEXT()
		OPCASE( GETVG ){
			value = dataVG->items.pArray[vmc->a]->items.pValue[ vmc->b ];
			if( regModes[ vmc->c ] == DAO_REG_REFER ){
				/* ensure no copying here: */
				locVars[ vmc->c ] = value;
			}else{
				DaoValue_Copy( value, & locVars[ vmc->c ] );
			}
		}OPNEXT()
		OPCASE( GETI )
		OPCASE( GETMI ){
			DaoContext_DoGetItem( topCtx, vmc );
			goto CheckException;
		}OPNEXT()
		OPCASE( GETF ){
			DaoContext_DoGetField( topCtx, vmc );
			goto CheckException;
		}OPNEXT()
		OPCASE( GETMF ){
			DaoContext_DoGetMetaField( topCtx, vmc );
			goto CheckException;
		}OPNEXT()
		OPCASE( SETVL ){
			abtp = typeVL->items.pType[ vmc->b ];
			if( DaoMoveAC( topCtx, locVars[vmc->a], dataVL + vmc->b, abtp ) ==0 )
				goto CheckException;
		}OPNEXT()
		OPCASE( SETVO ){
			abtp = typeVO->items.pType[ vmc->b ];
			if( DaoMoveAC( topCtx, locVars[vmc->a], dataVO + vmc->b, abtp ) ==0 )
				goto CheckException;
		}OPNEXT()
		OPCASE( SETVK ){
			abtp = typeVK->items.pArray[ vmc->c ]->items.pType[ vmc->b ];
			vref = dataVK->items.pArray[vmc->c]->items.pValue + vmc->b;
			if( DaoMoveAC( topCtx, locVars[vmc->a], vref, abtp ) ==0 ) goto CheckException;
		}OPNEXT()
		OPCASE( SETVG ){
			abtp = typeVG->items.pArray[ vmc->c ]->items.pType[ vmc->b ];
			vref = dataVG->items.pArray[vmc->c]->items.pValue + vmc->b;
			if( DaoMoveAC( topCtx, locVars[vmc->a], vref, abtp ) ==0 ) goto CheckException;
		}OPNEXT()
		OPCASE( SETI )
		OPCASE( SETMI ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			DaoContext_DoSetItem( topCtx, vmc );
			goto CheckException;
		}OPNEXT()
		OPCASE( SETF ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			DaoContext_DoSetField( topCtx, vmc );
			goto CheckException;
		}OPNEXT()
		OPCASE( SETMF ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			DaoContext_DoSetMetaField( topCtx, vmc );
			goto CheckException;
		}OPNEXT()
		OPCASE( LOAD ){
			if( locVars[ vmc->a ]->xNull.konst == 0 && regModes[ vmc->c ] == DAO_REG_REFER ){
				locVars[ vmc->c ] = locVars[ vmc->a ];
			}else{
				vref = topCtx->regArray->items.pValue + vmc->c;
				DaoValue_Copy( locVars[ vmc->a ], vref );
			}
			//XXX locVars[ vmc->c ]->mode = vmc->b;
		}OPNEXT()
		OPCASE( CAST ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			topCtx->vmc = vmc;
			DaoContext_DoCast( topCtx, vmc );
			goto CheckException;
		}OPNEXT()
		OPCASE( MOVE ){
			topCtx->vmc = vmc;
			DaoContext_DoMove( topCtx, vmc );
			vA = locVars[ vmc->a ];
			/* assigning no-duplicated constant:
			   routine Func( a : const list<int> ){ b = a; } */
			goto CheckException;
		}OPNEXT()
		OPCASE( ADD )
			OPCASE( SUB )
			OPCASE( MUL )
			OPCASE( DIV )
			OPCASE( MOD )
			OPCASE( POW ){
				topCtx->vmc = vmc;
				DaoContext_DoBinArith( topCtx, vmc );
				goto CheckException;
			}OPNEXT()
		OPCASE( AND )
			OPCASE( OR )
			OPCASE( LT )
			OPCASE( LE )
			OPCASE( EQ )
			OPCASE( NE ){
				topCtx->vmc = vmc;
				DaoContext_DoBinBool( topCtx, vmc );
				goto CheckException;
			}OPNEXT()
		OPCASE( IN ){
				topCtx->vmc = vmc;
				DaoContext_DoInTest( topCtx, vmc );
				goto CheckException;
			}OPNEXT()
		OPCASE( NOT )
			OPCASE( UNMS ){
				topCtx->vmc = vmc;
				DaoContext_DoUnaArith( topCtx, vmc );
				goto CheckException;
			}OPNEXT()
		OPCASE( BITAND )
			OPCASE( BITOR )
			OPCASE( BITXOR ){
				topCtx->vmc = vmc;
				DaoContext_DoBitLogic( topCtx, vmc );
				goto CheckException;
			}OPNEXT()
		OPCASE( BITLFT )
			OPCASE( BITRIT ){
				topCtx->vmc = vmc;
				DaoContext_DoBitShift( topCtx, vmc );
				goto CheckException;
			}OPNEXT()
		OPCASE( BITREV ){
			topCtx->vmc = vmc;
			DaoContext_DoBitFlip( topCtx, vmc );
			goto CheckException;
		}OPNEXT()
		OPCASE( CHECK ){
			DaoContext_DoCheck( topCtx, vmc );
		}OPNEXT()
		OPCASE( NAMEVA ){
			DaoContext_BindNameValue( topCtx, vmc );
		}OPNEXT()
		OPCASE( PAIR ){
			topCtx->vmc = vmc;
			DaoContext_DoPair( topCtx, vmc );
		}OPNEXT()
		OPCASE( TUPLE ){
			topCtx->vmc = vmc;
			DaoContext_DoTuple( topCtx, vmc );
		}OPNEXT()
		OPCASE( LIST ){
			topCtx->vmc = vmc;
			DaoContext_DoList( topCtx, vmc );
		}OPNEXT()
		OPCASE( MAP )
			OPCASE( HASH ){
				topCtx->vmc = vmc;
				DaoContext_DoMap( topCtx, vmc );
			}OPNEXT()
		OPCASE( ARRAY ){
			DaoContext_DoArray( topCtx, vmc );
		}OPNEXT()
		OPCASE( MATRIX ){
			DaoContext_DoMatrix( topCtx, vmc );
		}OPNEXT()
		OPCASE( CURRY )
			OPCASE( MCURRY ){
				DaoContext_DoCurry( topCtx, vmc );
			}OPNEXT()
		OPCASE( CASE )
			OPCASE( GOTO ){
				vmc = vmcBase + vmc->b;
			}OPJUMP()
		OPCASE( SWITCH ){
			vmc = DaoContext_DoSwitch( topCtx, vmc );
		}OPJUMP()
		OPCASE( ITER ){
			topCtx->vmc = vmc;
			DaoContext_DoIter( topCtx, vmc );
			goto CheckException;
		}OPNEXT()
		OPCASE( TEST ){
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
				if( vA->xTuple.unitype == dao_type_for_iterator && vA->xTuple.items->items.pValue[0]->xInteger.value ){
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
					vmc = vA->xTuple.items->size ? vmc+1 : vmcBase + vmc->b;
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
		}OPJUMP()
		OPCASE( MATH ){
			if( DaoVM_DoMath( topCtx, vmc, locVars[ vmc->c ], locVars[vmc->b] ) )
				goto RaiseErrorInvalidOperation;
		}OPNEXT()
		OPCASE( FUNCT ){
			topCtx->vmc = vmc;
			if( self->stopit | vmSpace->stopit ) goto FinishProc;
			DaoContext_DoFunctional( topCtx, vmc );
			goto CheckException;
		}OPNEXT()
		OPCASE( CALL )
			OPCASE( MCALL ){
				topCtx->vmc = vmc;
				if( self->stopit | vmSpace->stopit ) goto FinishProc;
				DaoContext_DoCall( topCtx, vmc );
				goto CheckException;
		}OPNEXT()
		OPCASE( ROUTINE ){
			topCtx->vmc = vmc;
			DaoContext_MakeRoutine( topCtx, vmc );
		}OPNEXT()
		OPCASE( CLASS ){
			topCtx->vmc = vmc;
			DaoContext_MakeClass( topCtx, vmc );
		}OPNEXT()
		OPCASE( CRRE ){
			DaoContext_CheckFE( topCtx );
			exceptCount = self->exceptions->size;
			topCtx->vmc = vmc;
			size = (size_t)(vmc - vmcBase);
			range = topFrame->ranges[ topFrame->depth-1 ];
			if( vmc->b == 0 ){ /* check exception: */
				int exceptFrom = range[0];
				/* remove a pair of exception scope, when it becomes invalid: */
				if( size <= exceptFrom && topFrame->depth >0 ) topFrame->depth --;
				if( DaoContext_DoCheckExcept( topCtx, vmc ) ){
					/* exception has happened before, jump to the proper handling point: */
					if( topFrame->depth ==0 ) goto FinishCall;
					vmc = vmcBase + range[1];
					topFrame->depth --;
					OPJUMP()
				}else if( vmc->c > 0 ){
					/* add exception scope for: try{ ... } */
					if( topFrame->depth < DVM_MAX_TRY_DEPTH )
						DaoVmFrame_PushRange( topFrame, size, vmc->c );
					else
						printf( "too many nested try{} statements\n" );
				}else if( topFrame->depth >0 && size >= range[1] ){
					/* remove a pair of exception scope, when it becomes invalid: */
					topFrame->depth --;
				}
			}else if( vmc->c == 0 ){
				topCtx->vmc = vmc;
				DaoContext_DoRaiseExcept( topCtx, vmc );
				goto CheckException;
			}else{
				retCode = DaoContext_DoRescueExcept( topCtx, vmc );
				exceptCount = self->exceptions->size;
				/* remove a pair of exception scope, when it becomes invalid: */
				if( topFrame->depth >0 && size >= range[1] ) topFrame->depth --;
				if( retCode == 0 ){
					vmc = vmcBase + vmc->c;
					OPJUMP()
				}
			}
		}OPNEXT()
		OPCASE( JITC ){
			dao_jit.Execute( topCtx, vmc->a );
			if( self->exceptions->size > exceptCount ) goto CheckException;
			vmc += vmc->b;
			OPJUMP()
				/*
				   dbase = (DaoValue*)inum;
				   printf( "jitc: %#x, %i\n", inum, dbase->type );
				 */
		}OPNEXT()
		OPCASE( RETURN ){
			topCtx->vmc = vmc;
			DaoContext_DoReturn( topCtx, vmc );
			//XXX DaoContext_CheckFE( topCtx );
			if( self->stopit | vmSpace->stopit ) goto FinishProc;
			goto FinishCall;
		}OPNEXT()
		OPCASE( YIELD ){
			topCtx->vmc = vmc;
			self->status = DAO_VMPROC_SUSPENDED;
			self->pauseType = DAO_VMP_YIELD;
			self->topFrame->entry = (short)(vmc - vmcBase);
			goto CheckException;
		}OPCASE( DEBUG ){
			if( self->stopit | vmSpace->stopit ) goto FinishProc;
			if( (vmSpace->options & DAO_EXEC_DEBUG ) ){
				topCtx->vmc = vmc;
				if( handler && handler->StdlibDebug ) handler->StdlibDebug( handler, topCtx );
				goto CheckException;
			}
		}OPNEXT()
		OPCASE( SECT ){
			goto ReturnFalse;
		}OPNEXT()
		OPCASE( SETVL_II ){
			dataVL[ vmc->b ]->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVL_IF ){
			dataVL[ vmc->b ]->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVL_ID ){
			dataVL[ vmc->b ]->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVL_FI ){
			dataVL[ vmc->b ]->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVL_FF ){
			dataVL[ vmc->b ]->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVL_FD ){
			dataVL[ vmc->b ]->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVL_DI ){
			dataVL[ vmc->b ]->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVL_DF ){
			dataVL[ vmc->b ]->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVL_DD ){
			dataVL[ vmc->b ]->xDouble.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVO_II ){
			dataVO[ vmc->b ]->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVO_IF ){
			dataVO[ vmc->b ]->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVO_ID ){
			dataVO[ vmc->b ]->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVO_FI ){
			dataVO[ vmc->b ]->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVO_FF ){
			dataVO[ vmc->b ]->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVO_FD ){
			dataVO[ vmc->b ]->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVO_DI ){
			dataVO[ vmc->b ]->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVO_DF ){
			dataVO[ vmc->b ]->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVO_DD ){
			dataVO[ vmc->b ]->xDouble.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVK_II ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVK_IF ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVK_ID ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVK_FI ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVK_FF ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVK_FD ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVK_DI ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVK_DF ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVK_DD ){
			ArrayArrayValue( dataVK, vmc->c, vmc->b )->xDouble.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVG_II ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVG_IF ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVG_ID ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVG_FI ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVG_FF ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVG_FD ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVG_DI ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVG_DF ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETVG_DD ){
			ArrayArrayValue( dataVG, vmc->c, vmc->b )->xDouble.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( MOVE_II ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( ADD_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) + IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( SUB_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) - IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( MUL_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) * IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( DIV_III ){
			inum = IntegerOperand( vmc->b );
			if( inum ==0 ) goto RaiseErrorDivByZero;
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) / inum;
		}OPNEXT()
		OPCASE( MOD_III ){
			inum = IntegerOperand( vmc->b );
			if( inum ==0 ) goto RaiseErrorDivByZero;
			IntegerOperand( vmc->c )=(dint)IntegerOperand( vmc->a ) % inum;
		}OPNEXT()
		OPCASE( POW_III ){
			IntegerOperand( vmc->c ) = pow( IntegerOperand( vmc->a ), IntegerOperand( vmc->b ) );
		}OPNEXT()
		OPCASE( AND_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a )
				? IntegerOperand( vmc->b ) : IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( OR_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a )
				? IntegerOperand( vmc->a ) : IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( NOT_I ){
			IntegerOperand( vmc->c ) = ! IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( UNMS_I ){
			IntegerOperand( vmc->c ) = - IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( LT_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) < IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( LE_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) <= IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( EQ_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) == IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( NE_III ){
			IntegerOperand( vmc->c ) = IntegerOperand( vmc->a ) != IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITAND_III ){
			IntegerOperand( vmc->c ) = (uint_t)IntegerOperand( vmc->a ) & (uint_t)IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITOR_III ){
			IntegerOperand( vmc->c ) = (uint_t)IntegerOperand( vmc->a ) | (uint_t)IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITXOR_III ){
			IntegerOperand( vmc->c ) = (uint_t)IntegerOperand( vmc->a ) ^ (uint_t)IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITLFT_III ){
			IntegerOperand( vmc->c ) = (uint_t)IntegerOperand( vmc->a ) << (uint_t)IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITRIT_III ){
			IntegerOperand( vmc->c ) = (uint_t)IntegerOperand( vmc->a ) >> (uint_t)IntegerOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITREV_I ){
			IntegerOperand( vmc->c ) = ~ (uint_t) IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( MOVE_FF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( ADD_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) + FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( SUB_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) - FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( MUL_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) * FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( DIV_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) / FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( MOD_FFF ){
			inum = (dint) FloatOperand( vmc->b );
			if( inum ==0 ) goto RaiseErrorDivByZero;
			FloatOperand( vmc->c ) = (dint)FloatOperand( vmc->a ) % inum;
		}OPNEXT()
		OPCASE( POW_FFF ){
			FloatOperand( vmc->c ) = powf( FloatOperand( vmc->a ), FloatOperand( vmc->b ) );
		}OPNEXT()
		OPCASE( AND_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) ? FloatOperand( vmc->b ) : FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( OR_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) ? FloatOperand( vmc->a ) : FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( NOT_F ){
			FloatOperand( vmc->c ) = ! FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( UNMS_F ){
			FloatOperand( vmc->c ) = - FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( LT_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) < FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( LE_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) <= FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( EQ_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) == FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( NE_FFF ){
			FloatOperand( vmc->c ) = FloatOperand( vmc->a ) != FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITAND_FFF ){
			FloatOperand( vmc->c ) = (ullong_t)FloatOperand( vmc->a ) & (ullong_t)FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITOR_FFF ){
			FloatOperand( vmc->c ) = (ullong_t)FloatOperand( vmc->a ) | (ullong_t)FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITXOR_FFF ){
			FloatOperand( vmc->c ) = (ullong_t)FloatOperand( vmc->a ) ^ (ullong_t)FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITLFT_FFF ){
			FloatOperand( vmc->c ) = (ullong_t)FloatOperand( vmc->a ) << (ullong_t)FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITRIT_FFF ){
			FloatOperand( vmc->c ) = (ullong_t)FloatOperand( vmc->a ) >> (ullong_t)FloatOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITREV_F ){
			FloatOperand( vmc->c ) = ~ (ullong_t) FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( MOVE_DD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( ADD_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) + DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( SUB_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) - DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( MUL_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) * DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( DIV_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) / DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( MOD_DDD ){
			lnum = (llong_t) DoubleOperand( vmc->b );
			if( lnum ==0 ) goto RaiseErrorDivByZero;
			DoubleOperand( vmc->c )=(llong_t)DoubleOperand( vmc->a ) % lnum;
		}OPNEXT()
		OPCASE( POW_DDD ){
			DoubleOperand( vmc->c ) = pow( DoubleOperand( vmc->a ), DoubleOperand( vmc->b ) );
		}OPNEXT()
		OPCASE( AND_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) ? DoubleOperand( vmc->b ) : DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( OR_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) ? DoubleOperand( vmc->a ) : DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( NOT_D ){
			DoubleOperand( vmc->c ) = ! DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( UNMS_D ){
			DoubleOperand( vmc->c ) = - DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( LT_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) < DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( LE_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) <= DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( EQ_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) == DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( NE_DDD ){
			DoubleOperand( vmc->c ) = DoubleOperand( vmc->a ) != DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITAND_DDD ){
			DoubleOperand( vmc->c ) = (ullong_t)DoubleOperand( vmc->a ) & (ullong_t)DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITOR_DDD ){
			DoubleOperand( vmc->c ) = (ullong_t)DoubleOperand( vmc->a ) | (ullong_t)DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITXOR_DDD ){
			DoubleOperand( vmc->c ) = ((ullong_t)DoubleOperand( vmc->a )) ^ (ullong_t)DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITLFT_DDD ){
			DoubleOperand( vmc->c ) = (ullong_t)DoubleOperand( vmc->a ) << (ullong_t)DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITRIT_DDD ){
			DoubleOperand( vmc->c ) = (ullong_t)DoubleOperand( vmc->a ) >> (ullong_t)DoubleOperand( vmc->b );
		}OPNEXT()
		OPCASE( BITREV_D ){
			DoubleOperand( vmc->c ) = ~ (ullong_t) DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( ADD_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA + BB;
		}OPNEXT()
		OPCASE( SUB_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA - BB;
		}OPNEXT()
		OPCASE( MUL_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA * BB;
		}OPNEXT()
		OPCASE( DIV_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA / BB;
		}OPNEXT()
		OPCASE( MOD_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = (llong_t)AA % (llong_t)BB;
		}OPNEXT()
		OPCASE( POW_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = powf( AA, BB );
		}OPNEXT()
		OPCASE( AND_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA ? BB : AA;
		}OPNEXT()
		OPCASE( OR_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA ? AA : BB;
		}OPNEXT()
		OPCASE( LT_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA < BB;
		}OPNEXT()
		OPCASE( LE_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA <= BB;
		}OPNEXT()
		OPCASE( EQ_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA == BB;
		}OPNEXT()
		OPCASE( NE_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = AA != BB;
		}OPNEXT()
		OPCASE( BITLFT_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = (ullong_t)AA << (ullong_t)BB;
		}OPNEXT()
		OPCASE( BITRIT_FNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			FloatOperand( vmc->c ) = (ullong_t)AA >> (ullong_t)BB;
		}OPNEXT()
		OPCASE( ADD_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA + BB;
		}OPNEXT()
		OPCASE( SUB_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA - BB;
		}OPNEXT()
		OPCASE( MUL_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA * BB;
		}OPNEXT()
		OPCASE( DIV_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA / BB;
		}OPNEXT()
		OPCASE( MOD_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = (llong_t)AA % (llong_t)BB;
		}OPNEXT()
		OPCASE( POW_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = pow( AA, BB );
		}OPNEXT()
		OPCASE( AND_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA ? BB : AA;
		}OPNEXT()
		OPCASE( OR_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA ? AA : BB;
		}OPNEXT()
		OPCASE( LT_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA < BB;
		}OPNEXT()
		OPCASE( LE_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA <= BB;
		}OPNEXT()
		OPCASE( EQ_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA == BB;
		}OPNEXT()
		OPCASE( NE_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = AA != BB;
		}OPNEXT()
		OPCASE( BITLFT_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = ((ullong_t)AA) << (ullong_t)BB;
		}OPNEXT()
		OPCASE( BITRIT_DNN ){
			NumberOperand( AA, vA, vmc->a );
			NumberOperand( BB, vB, vmc->b );
			DoubleOperand( vmc->c ) = ((ullong_t)AA) >> (ullong_t)BB;
		}OPNEXT()
		OPCASE( ADD_SS ){
			vA = locVars[ vmc->a ];  vB = locVars[ vmc->b ];
			vC = locVars[ vmc->c ];
			vC->type = DAO_STRING;
			if( vmc->a == vmc->c ){
				DString_Append( vA->xString.data, vB->xString.data );
			}else if( vmc->b == vmc->c ){
				DString_Insert( vB->xString.data, vA->xString.data, 0, 0, 0 );
			}else{
				if( vC->xString.data == NULL ) vC->xString.data = DString_Copy( vA->xString.data );
				DString_Assign( vC->xString.data, vA->xString.data );
				DString_Append( vC->xString.data, vB->xString.data );
			}
		}OPNEXT()
		OPCASE( LT_SS ){
			vA = locVars[ vmc->a ];  vB = locVars[ vmc->b ];
			IntegerOperand( vmc->c ) = DString_Compare( vA->xString.data, vB->xString.data ) <0;
		}OPNEXT()
		OPCASE( LE_SS ){
			vA = locVars[ vmc->a ];  vB = locVars[ vmc->b ];
			IntegerOperand( vmc->c ) = DString_Compare( vA->xString.data, vB->xString.data ) <=0;
		}OPNEXT()
		OPCASE( EQ_SS ){
			vA = locVars[ vmc->a ];  vB = locVars[ vmc->b ];
			IntegerOperand( vmc->c ) = DString_Compare( vA->xString.data, vB->xString.data ) ==0;
		}OPNEXT()
		OPCASE( NE_SS ){
			vA = locVars[ vmc->a ];  vB = locVars[ vmc->b ];
			IntegerOperand( vmc->c ) = DString_Compare( vA->xString.data, vB->xString.data ) !=0;
		}OPNEXT()
		OPCASE( MOVE_IF ){
			IntegerOperand( vmc->c ) = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( MOVE_ID ){
			IntegerOperand( vmc->c ) = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( MOVE_FI ){
			FloatOperand( vmc->c ) = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( MOVE_FD ){
			FloatOperand( vmc->c ) = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( MOVE_DI ){
			DoubleOperand( vmc->c ) = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( MOVE_DF ){
			DoubleOperand( vmc->c ) = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( MOVE_CC ){
			ComplexOperand( vmc->c ) = ComplexOperand( vmc->a );
		}OPNEXT()
		OPCASE( MOVE_SS ){
			vC = locVars[ vmc->c ];
			if( vC == NULL ){
				locVars[ vmc->c ] = (DaoValue*) DaoString_Copy( & locVars[ vmc->a ]->xString );
				GC_IncRC( locVars[ vmc->c ] );
			}else{
				DString_Assign( vC->xString.data, locVars[ vmc->a ]->xString.data );
			}
		}OPNEXT()
		OPCASE( MOVE_PP ){
			vA = locVars[ vmc->a ];
			topCtx->vmc = vmc;
			if( DaoMoveAC( topCtx, vA, locVars + vmc->c, locTypes[ vmc->c ] ) ==0 )
				goto CheckException;
		}OPNEXT()
		OPCASE( UNMS_C ){
			acom = ComplexOperand( vmc->a );
			vC = locVars[ vmc->c ];
			vC->xComplex.value.real = - acom.real;
			vC->xComplex.value.imag = - acom.imag;
		}OPNEXT()
		OPCASE( ADD_CC ){
			acom = ComplexOperand( vmc->a );  bcom = ComplexOperand( vmc->b );
			vC = locVars[ vmc->c ];
			vC->xComplex.value.real = acom.real + bcom.real;
			vC->xComplex.value.imag = acom.imag + bcom.imag;
		}OPNEXT()
		OPCASE( SUB_CC ){
			acom = ComplexOperand( vmc->a );  bcom = ComplexOperand( vmc->b );
			vC = locVars[ vmc->c ];
			vC->xComplex.value.real = acom.real - bcom.real;
			vC->xComplex.value.imag = acom.imag - bcom.imag;
		}OPNEXT()
		OPCASE( MUL_CC ){
			acom = ComplexOperand( vmc->a );  bcom = ComplexOperand( vmc->b );
			vC = locVars[ vmc->c ];
			vC->xComplex.value.real = acom.real * bcom.real - acom.imag * bcom.imag;
			vC->xComplex.value.imag = acom.real * bcom.imag + acom.imag * bcom.real;
		}OPNEXT()
		OPCASE( DIV_CC ){
			acom = ComplexOperand( vmc->a );  bcom = ComplexOperand( vmc->b );
			vC = locVars[ vmc->c ];
			dnum = acom.real * bcom.real + acom.imag * bcom.imag;
			vC->xComplex.value.real = (acom.real*bcom.real + acom.imag*bcom.imag) / dnum;
			vC->xComplex.value.imag = (acom.imag*bcom.real - acom.real*bcom.imag) / dnum;
		}OPNEXT()
		OPCASE( GETI_SI ){
			str = locVars[ vmc->a ]->xString.data;
			id = IntegerOperand( vmc->b );
			if( id <0 ) id += str->size;
			if( id <0 || id >= str->size ) goto RaiseErrorIndexOutOfRange;
			if( str->mbs ){
				IntegerOperand( vmc->c ) = str->mbs[id];
			}else{
				IntegerOperand( vmc->c ) = str->wcs[id];
			}
		}OPNEXT()
		OPCASE( SETI_SII ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
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
		}OPNEXT()
		OPCASE( GETI_LI ){
			list = & locVars[ vmc->a ]->xList;
			id = IntegerOperand( vmc->b );
			abtp = locTypes[ vmc->c ];
			if( id <0 ) id += list->items->size;
			if( id <0 || id >= list->items->size ) goto RaiseErrorIndexOutOfRange;
			if( abtp && DaoType_MatchValue( abtp, list->items->items.pValue[id], NULL ) ==0 ) goto CheckException;
			if( regModes[ vmc->c ] == DAO_REG_REFER ){
				locVars[ vmc->c ] = list->items->items.pValue[id];
			}else{
				DaoValue_Copy( list->items->items.pValue[id], & locVars[ vmc->c ] );
			}
		}OPNEXT()
		OPCASE( SETI_LI ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			list = & locVars[ vmc->c ]->xList;
			id = IntegerOperand( vmc->b );
			abtp = NULL;
			if( list->unitype && list->unitype->nested->size )
				abtp = list->unitype->nested->items.pType[0];
			if( id <0 ) id += list->items->size;
			if( id <0 || id >= list->items->size ) goto RaiseErrorIndexOutOfRange;
			if( DaoMoveAC( topCtx, locVars[vmc->a], list->items->items.pValue + id, abtp ) ==0 )
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
				if( regModes[ vmc->c ] == DAO_REG_REFER ){
					locVars[ vmc->c ] = list->items->items.pValue[id];
				}else{
					DaoValue_Copy( list->items->items.pValue[id], & locVars[ vmc->c ] );
				}
			}OPNEXT()
		OPCASE( SETI_LIII )
			OPCASE( SETI_LIIF )
			OPCASE( SETI_LIID ){
				if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
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
				if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
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
				if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
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
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
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
				if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
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
				if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
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
				if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
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
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
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
			if( array->dims->size == tuple->items->size ){
				dims = array->dims->items.pSize;
				dmac = array->dimAccum->items.pSize;
				id = 0;
				for(i=0; i<array->dims->size; i++){
					j = DaoValue_GetInteger( tuple->items->items.pValue[i] );
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
				DaoContext_DoGetItem( topCtx, vmc );
				goto CheckException;
			}
		}OPNEXT()
		OPCASE( SETI_AM ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
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
				DaoContext_DoSetItem( topCtx, vmc );
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
				topCtx->vmc = vmc;
				DaoContext_RaiseException( topCtx, DAO_ERROR, "numeric array is disabled" );
			}OPNEXT()
#endif
		OPCASE( GETI_TI ){
			tuple = & locVars[ vmc->a ]->xTuple;
			id = IntegerOperand( vmc->b );
			abtp = locTypes[ vmc->c ];
			if( id <0 || id >= tuple->items->size ) goto RaiseErrorIndexOutOfRange;
			if( abtp && DaoType_MatchValue( abtp, tuple->items->items.pValue[id], NULL ) ==0 ) goto CheckException;
			/* reference to tuple items should always be valid: */
			locVars[ vmc->c ] = tuple->items->items.pValue[id];
		}OPNEXT()
		OPCASE( SETI_TI ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			id = IntegerOperand( vmc->b );
			abtp = NULL;
			if( id <0 || id >= tuple->items->size ) goto RaiseErrorIndexOutOfRange;
			abtp = tuple->unitype->nested->items.pType[id];
			if( abtp->tid == DAO_PAR_NAMED ) abtp = & abtp->aux->xType;
			if( DaoMoveAC( topCtx, locVars[vmc->a], tuple->items->items.pValue + id, abtp ) ==0 )
				goto CheckException;
		}OPNEXT()
		OPCASE( GETF_T ){
			tuple = & locVars[ vmc->a ]->xTuple;
			id = vmc->b;
			abtp = locTypes[ vmc->c ];
			if( abtp && DaoType_MatchValue( abtp, tuple->items->items.pValue[id], NULL ) ==0 ) goto CheckException;
			locVars[ vmc->c ] = tuple->items->items.pValue[id];
		}OPNEXT()
		OPCASE( SETF_T ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			id = vmc->b;
			abtp = tuple->unitype->nested->items.pType[id];
			if( abtp->tid == DAO_PAR_NAMED ) abtp = & abtp->aux->xType;
			if( DaoMoveAC( topCtx, locVars[vmc->a], tuple->items->items.pValue + id, abtp ) ==0 )
				goto CheckException;
		}OPNEXT()
		OPCASE( GETF_TI )
			OPCASE( GETF_TF )
			OPCASE( GETF_TD )
			OPCASE( GETF_TS ){
				tuple = & locVars[ vmc->a ]->xTuple;
				locVars[ vmc->c ] = tuple->items->items.pValue[ vmc->b ];
			}OPNEXT()
		OPCASE( SETF_TII ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items->items.pValue[ vmc->b ]->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_TIF ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items->items.pValue[ vmc->b ]->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_TID ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items->items.pValue[ vmc->b ]->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_TFI ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items->items.pValue[ vmc->b ]->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_TFF ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items->items.pValue[ vmc->b ]->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_TFD ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items->items.pValue[ vmc->b ]->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_TDI ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items->items.pValue[ vmc->b ]->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_TDF ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items->items.pValue[ vmc->b ]->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_TDD ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items->items.pValue[ vmc->b ]->xDouble.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_TSS ){
			if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
			tuple = & locVars[ vmc->c ]->xTuple;
			vA = locVars[ vmc->a ];
			DString_Assign( tuple->items->items.pValue[ vmc->b ]->xString.data, vA->xString.data );
		}OPNEXT()
		OPCASE( GETF_KC ){
			value = locVars[ vmc->a ]->xClass.cstData->items.pValue[ vmc->b ];
			abtp = locTypes[ vmc->c ];
			if( abtp && DaoType_MatchValue( abtp, value, NULL ) ==0 ) goto CheckException;
			locVars[ vmc->c ] = value;
		}OPNEXT()
		OPCASE( GETF_KG ){
			value = locVars[ vmc->a ]->xClass.glbData->items.pValue[ vmc->b ];
			abtp = locTypes[ vmc->c ];
			if( abtp && DaoType_MatchValue( abtp, value, NULL ) ==0 ) goto CheckException;
			locVars[ vmc->c ] = value;
		}OPNEXT()
		OPCASE( GETF_OC ){
			value = locVars[ vmc->a ]->xObject.myClass->cstData->items.pValue[ vmc->b ];
			abtp = locTypes[ vmc->c ];
			if( abtp && DaoType_MatchValue( abtp, value, NULL ) ==0 ) goto CheckException;
			locVars[ vmc->c ] = value;
		}OPNEXT()
		OPCASE( GETF_OG ){
			value = locVars[ vmc->a ]->xObject.myClass->glbData->items.pValue[ vmc->b ];
			abtp = locTypes[ vmc->c ];
			if( abtp && DaoType_MatchValue( abtp, value, NULL ) ==0 ) goto CheckException;
			locVars[ vmc->c ] = value;
		}OPNEXT()
		OPCASE( GETF_OV ){
			object = & locVars[ vmc->a ]->xObject;
			if( object == & object->myClass->objType->value->xObject ) goto AccessDefault;
			value = object->objValues[ vmc->b ];
			abtp = locTypes[ vmc->c ];
			if( abtp && DaoType_MatchValue( abtp, value, NULL ) ==0 ) goto CheckException;
			locVars[ vmc->c ] = value;
		}OPNEXT()
		OPCASE( GETF_KCI )
			OPCASE( GETF_KCF )
			OPCASE( GETF_KCD ){
				value = locVars[ vmc->a ]->xClass.cstData->items.pValue[ vmc->b ];
				locVars[ vmc->c ] = value;
			}OPNEXT()
		OPCASE( GETF_KGI )
			OPCASE( GETF_KGF )
			OPCASE( GETF_KGD ){
				value = locVars[ vmc->a ]->xClass.glbData->items.pValue[ vmc->b ];
				locVars[ vmc->c ] = value;
			}OPNEXT()
		OPCASE( GETF_OCI )
			OPCASE( GETF_OCF )
			OPCASE( GETF_OCD ){
				value = locVars[ vmc->a ]->xObject.myClass->cstData->items.pValue[ vmc->b ];
				locVars[ vmc->c ] = value;
			}OPNEXT()
		OPCASE( GETF_OGI )
			OPCASE( GETF_OGF )
			OPCASE( GETF_OGD ){
				value = locVars[ vmc->a ]->xObject.myClass->glbData->items.pValue[ vmc->b ];
				locVars[ vmc->c ] = value;
			}OPNEXT()
		OPCASE( GETF_OVI )
			OPCASE( GETF_OVF )
			OPCASE( GETF_OVD ){
				object = & locVars[ vmc->a ]->xObject;
				if( object == & object->myClass->objType->value->xObject ) goto AccessDefault;
				locVars[ vmc->c ] = object->objValues[ vmc->b ];
			}OPNEXT()
		OPCASE( SETF_KG ){
			klass = & locVars[ vmc->c ]->xClass;
			vC2 = klass->glbData->items.pValue + vmc->b;
			abtp = klass->glbDataType->items.pType[ vmc->b ];
			if( DaoMoveAC( topCtx, locVars[vmc->a], vC2, abtp ) ==0 )
				goto CheckException;
		}OPNEXT()
		OPCASE( SETF_OG )
			OPCASE( SETF_OV ){
				object = & locVars[ vmc->c ]->xObject;
				if( vmc->code == DVM_SETF_OG ){
					klass = ((DaoObject*)klass)->myClass;
					vC2 = klass->glbData->items.pValue + vmc->b;
					abtp = klass->glbDataType->items.pType[ vmc->b ];
				}else{
					if( object == & object->myClass->objType->value->xObject ) goto AccessDefault;
					if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
					vC2 = object->objValues + vmc->b;
					abtp = object->myClass->objDataType->items.pType[ vmc->b ];
				}
				if( DaoMoveAC( topCtx, locVars[vmc->a], vC2, abtp ) ==0 )
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
				klass = locVars[ vmc->c ]->xObject.myClass;
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
				if( object == & object->myClass->objType->value->xObject ) goto AccessDefault;
				if( locVars[ vmc->c ]->xNull.konst ) goto ModifyConstant;
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
					topCtx->vmc = vmc;
					DaoContext_RaiseException( topCtx, DAO_ERROR,
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
			topCtx->vmc = vmc;
			DaoContext_RaiseException( topCtx, DAO_ERROR_INDEX_OUTOFRANGE, "" );
			goto CheckException;
RaiseErrorDivByZero:
			topCtx->vmc = vmc;
			topCtx->idClearFE = vmc - vmcBase;
			DaoContext_RaiseException( topCtx, DAO_ERROR_FLOAT_DIVBYZERO, "" );
			goto CheckException;
RaiseErrorInvalidOperation:
			topCtx->vmc = vmc;
			DaoContext_RaiseException( topCtx, DAO_ERROR, "invalid operation" );
			goto CheckException;
ModifyConstant:
			topCtx->vmc = vmc;
			DaoContext_RaiseException( topCtx, DAO_ERROR, "attempt to modify a constant" );
			goto CheckException;
AccessDefault:
			topCtx->vmc = vmc;
			DaoContext_RaiseException( topCtx, DAO_ERROR, "invalid field access for default object" );
			goto CheckException;
#if 0
RaiseErrorNullObject:
			topCtx->vmc = vmc;
			DaoContext_RaiseException( topCtx, DAO_ERROR, "operate on null object" );
			goto CheckException;
#endif
CheckException:

			if( self->stopit | vmSpace->stopit ) goto FinishProc;
			if( invokehost ) handler->InvokeHost( handler, topCtx );
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
			return 0;
		}
		return 1;
	}
	print = (vmSpace->options & DAO_EXEC_INTERUN) && (here->options & DAO_NS_AUTO_GLOBAL);
	if( print || vmSpace->evalCmdline ){
		if( self->returned->type ){
			DaoStream_WriteMBS( vmSpace->stdStream, "= " );
			DaoValue_Print( self->returned, topCtx, vmSpace->stdStream, NULL );
			DaoStream_WriteNewLine( vmSpace->stdStream );
		}
	}
	DaoVmProcess_PopContext( self );
	goto CallEntry;

FinishProc:

	DaoVmProcess_PrintException( self, 1 );
	if( self->topFrame && self->topFrame->state & DVM_SPEC_RUN ) goto ReturnTrue;
	DaoVmProcess_CleanProcess( self );
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
extern void DaoVmProcess_Trace( DaoVmProcess *self, int depth );
int DaoVM_DoMath( DaoContext *self, DaoVmCode *vmc, DaoValue *c, DaoValue *p )
{
	DaoNameSpace *ns = self->nameSpace;
	DaoType *type = self->regTypes[vmc->c];
	DaoComplex tmp = {DAO_COMPLEX};
	int func = vmc->a;
	self->vmc = vmc;
	if( p->type == DAO_COMPLEX ){
		complex16 par = p->xComplex.value;
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
			if( type == NULL ) self->regTypes[vmc->c] = DaoNameSpace_GetType( ns, (DaoValue*) & tmp );
			if( c->type == DAO_DOUBLE ){
				c->xDouble.value = rres;
			}else{
				return DaoContext_PutDouble( self, rres ) == NULL;
			}
		}else{
			if( type == NULL ) self->regTypes[vmc->c] = DaoNameSpace_GetType( ns, (DaoValue*) & tmp );
			if( c->type == DAO_COMPLEX ){
				c->xComplex.value = cres;
			}else{
				return DaoContext_PutComplex( self, cres ) == NULL;
			}
		}
		return 0;
	}else if( p->type && p->type <= DAO_DOUBLE ){
		double par = DaoValue_GetDouble( p );
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
			if( type == NULL ) self->regTypes[vmc->c] = DaoNameSpace_GetType( ns, p );
			switch( p->type ){
			case DAO_INTEGER : return DaoContext_PutInteger( self, res ) == NULL;
			case DAO_FLOAT  : return DaoContext_PutFloat( self, res ) == NULL;
			case DAO_DOUBLE : return DaoContext_PutDouble( self, res ) == NULL;
			default : break;
			}
		}else if( c->type == DAO_DOUBLE ){
			c->xDouble.value = res;
			return 0;
		}else{
			tmp.type = DAO_DOUBLE;
			if( type == NULL ) self->regTypes[vmc->c] = DaoNameSpace_GetType( ns, (DaoValue*) & tmp );
			return DaoContext_PutDouble( self, res ) == NULL;
		}
	}
	return 1;
}
void DaoContext_Sort( DaoContext *self, DaoVmCode *vmc, int index, int entry );
void DaoContext_Apply( DaoContext *self, DaoVmCode *vmc, int index, int dim, int entry );
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
	int m = tuple->items->size;
	int j, k = i * m;
	for(j=0; j<m; j++) DaoArray_SetValue( self, k + j, tuple->items->items.pValue[j] );
}
static void DaoContext_FailedMethod( DaoContext *self )
{
	if( self->process->status == DAO_VMPROC_ABORTED )
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "functional method failed" );
	else
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "invalid return value" );
}
static void DaoContext_Fold( DaoContext *self, DaoVmCode *vmc, int index, int entry )
{
	DaoValue *res = NULL, *param = self->regValues[ vmc->b ];
	DaoTuple *tuple = & param->xTuple;
	DaoList *list = & param->xList;
	DaoType *type = self->regTypes[ index + 2 ];
	DaoValue **item = & self->regValues[ index + 2 ];
	DaoValue *returned = self->process->returned;
	int i, noinit=1;
	if( param->type == DAO_TUPLE ){
		if( tuple->items->size ==0 ) return;
		list = & tuple->items->items.pValue[0]->xList;
		if( tuple->items->size >1 ){
			res = tuple->items->items.pValue[1];
			noinit = 0;
		}
	}
	if( list->items->size ==0 ) return;
	if( noinit ) res = list->items->items.pValue[0];
	DaoValue_Move( res, item, type );
	for(i=noinit; i<list->items->size; i++){
		self->regValues[index]->xInteger.value = i;
		DaoVmProcess_ExecuteSection( self->process, entry );
		if( self->process->status == DAO_VMPROC_ABORTED ) goto MethodFailed;
		if( DaoValue_Move( returned, item, type ) == 0 ) goto MethodFailed;
	}
	self->vmc = vmc; /* it is changed! */
	DaoContext_PutValue( self, self->process->returned );
	return;
MethodFailed: DaoContext_FailedMethod( self );
}
static void DaoContext_Fold2( DaoContext *self, DaoVmCode *vmc, int index, int entry )
{
	DaoValue tmp;
	DaoValue *returned = self->process->returned;
	DaoValue **item = & self->regValues[ index + 2 ];
	DaoValue *res = NULL, *param = self->regValues[ vmc->b ];
	DaoTuple *tuple = & param->xTuple;
	DaoArray *array = & param->xArray;
	DaoType *type = self->regTypes[ index + 2 ];
	int i, noinit=1;
	if( param->type == DAO_TUPLE ){
		if( tuple->items->size ==0 ) return;
		array = & tuple->items->items.pValue[0]->xArray;
		if( tuple->items->size >1 ){
			res = tuple->items->items.pValue[1];
			noinit = 0;
		}
	}
	if( array->size ==0 ) return;
	if( noinit ) res = DaoArray_GetValue( array, 0, & tmp );
	DaoValue_Move( res, item, type );
	for(i=noinit; i<array->size; i++){
		self->regValues[index]->xInteger.value = i;
		DaoVmProcess_ExecuteSection( self->process, entry );
		if( self->process->status == DAO_VMPROC_ABORTED ) goto MethodFailed;
		if( DaoValue_Move( returned, item, type ) == 0 ) goto MethodFailed;
	}
	self->vmc = vmc; /* it is changed! */
	DaoContext_PutValue( self, self->process->returned );
	return;
MethodFailed: DaoContext_FailedMethod( self );
}
static void DaoContext_Unfold( DaoContext *self, DaoVmCode *vmc, int index, int entry )
{
	DaoValue *param = self->regValues[ vmc->b ];
	DaoList *result = DaoContext_PutList( self );
	DaoType *type = self->regTypes[ index + 1 ];
	DaoValue **init = & self->regValues[ index + 1 ];
	DaoValue_Move( param, init, type );
	DaoValue_Clear( & self->process->returned ) ;
	DaoVmProcess_ExecuteSection( self->process, entry );
	int k = 0;
	while( self->process->returned->type && ++k < 10 ){
		if( self->process->status == DAO_VMPROC_ABORTED ) goto MethodFailed;
		if( DaoList_PushBack( result, self->process->returned ) ) goto MethodFailed;
		DaoValue_Clear( & self->process->returned ) ;
		DaoVmProcess_ExecuteSection( self->process, entry );
	}
	return;
MethodFailed: DaoContext_FailedMethod( self );
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
static int DaoContext_ListMapSIC( DaoContext *self, DaoVmCode *vmc, int index, int entry )
{
	DaoValue *res = NULL;
	DaoValue *param = self->regValues[ vmc->b ];
	DaoTuple *tuple = NULL;
	DaoList *list = & param->xList;
	DaoList *result = NULL;
	int i, j, count = 0;
	int size = 0;
	if( param->type == DAO_TUPLE ){
		tuple = & param->xTuple;
		list = & tuple->items->items.pValue[0]->xList;
		size = list->items->size;
		for( j=1; j<tuple->items->size; j++ )
			if( size != tuple->items->items.pValue[j]->xList.items->size ) return 1;
	}
	size = list->items->size;
	if( vmc->a != DVM_FUNCT_COUNT && vmc->a != DVM_FUNCT_EACH ) result = DaoContext_PutList( self );
	for(i=0; i<size; i++){
		self->regValues[index]->xInteger.value = i;
		DaoVmProcess_ExecuteSection( self->process, entry );
		if( self->process->status == DAO_VMPROC_ABORTED ) break;
		res = self->process->returned;
		switch( vmc->a ){
		case DVM_FUNCT_MAP :
			DaoList_PushBack( result, res );
			break;
		case DVM_FUNCT_SELECT :
			if( DaoValue_GetInteger( res ) ){
				if( tuple ){
					DaoTuple *tup = DaoTuple_New( tuple->items->size );
					for( j=0; j<tuple->items->size; j++ ){
						list = & tuple->items->items.pValue[j]->xList;
						DaoValue_Move( list->items->items.pValue[i], tup->items->items.pValue + j, NULL );
					}
					DaoList_PushBack( result, (DaoValue*)tup );
				}else{
					DaoList_PushBack( result, list->items->items.pValue[i] );
				}
			}
			break;
		case DVM_FUNCT_INDEX :
			if( DaoValue_GetInteger( res ) ){
				DaoInteger di = {DAO_INTEGER,0,1,0,{0,0},0,0,0};
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
	self->vmc = vmc; /* it is changed! */
	if( vmc->a == DVM_FUNCT_COUNT ) DaoContext_PutInteger( self, count );
	return 0;
}
static int DaoContext_ArrayMapSIC( DaoContext *self, DaoVmCode *vmc, int index, int entry )
{
#ifdef DAO_WITH_NUMARRAY
	DaoValue *param = self->regValues[ vmc->b ];
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
		array = & tuple->items->items.pValue[0]->xArray;
		size = array->size;
		for( j=1; j<tuple->items->size; j++ )
			if( size != tuple->items->items.pValue[j]->xArray.size ) return 1;
	}
	size = array->size;
	if( vmc->a != DVM_FUNCT_COUNT && vmc->a != DVM_FUNCT_EACH ){
		if( vmc->a == DVM_FUNCT_MAP ){
			int last = (vmc-2)->a;
			result = DaoContext_PutArray( self );
			if( DaoArray_TypeShape( result, array, self->regTypes[last] ) == 0 ){
				DaoContext_RaiseException( self, DAO_WARNING, "invalid return type" );
			}
		}else{
			list = DaoContext_PutList( self );
		}
	}
	memset( & tmp, 0, sizeof(DaoValue) );
	for(i=0; i<size; i++){
		self->regValues[index]->xInteger.value = i;
		DaoVmProcess_ExecuteSection( self->process, entry );
		if( self->process->status == DAO_VMPROC_ABORTED ) break;
		res = self->process->returned;
		switch( vmc->a ){
		case DVM_FUNCT_MAP :
			if( res->type == DAO_TUPLE && res->xTuple.items->size )
				DaoArray_SetValues( result, i, & res->xTuple );
			else
				DaoArray_SetValue( result, i, res );
			break;
		case DVM_FUNCT_SELECT :
			if( DaoValue_GetInteger( res ) ){
				if( tuple ){
					DaoTuple *tup = DaoTuple_New( tuple->items->size );
					for( j=0; j<tuple->items->size; j++ ){
						array = & tuple->items->items.pValue[j]->xArray;
						nval = DaoArray_GetValue( array, i, & tmp );
						DaoValue_Move( nval, tup->items->items.pValue + j, NULL );
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
	self->vmc = vmc; /* it is changed! */
	if( vmc->a == DVM_FUNCT_COUNT ) DaoContext_PutInteger( self, count );
#else
	DaoContext_RaiseException( self, DAO_DISABLED_NUMARRAY, NULL );
#endif
	return 0;
}
static void DaoContext_MapSIC( DaoContext *self, DaoVmCode *vmc, int index, int entry )
{
	DaoValue *param = self->regValues[ vmc->b ];
	DaoTuple *tuple = & param->xTuple;
	int j, k;
	if( param->type == DAO_LIST ){
		DaoContext_ListMapSIC( self, vmc, index, entry );
	}else if( param->type == DAO_ARRAY ){
		DaoContext_ArrayMapSIC( self, vmc, index, entry );
	}else if( param->type == DAO_TUPLE ){
		k = tuple->items->items.pValue[0]->type;
		for( j=1; j<tuple->items->size; j++ ) if( k != tuple->items->items.pValue[j]->type ) goto InvalidParam;
		if( k == DAO_ARRAY ){
			if( DaoContext_ArrayMapSIC( self, vmc, index, entry ) ) goto InvalidParam;
		}else if( k == DAO_LIST ){
			if( DaoContext_ListMapSIC( self, vmc, index, entry ) ) goto InvalidParam;
		}else goto InvalidParam;
	}else{
		goto InvalidParam;
	}
	return;
InvalidParam:
	DaoContext_RaiseException( self, DAO_ERROR, "invalid parameter" );
}
int DaoArray_FromList( DaoArray *self, DaoList *list, DaoType *tp );
static void DaoContext_DataFunctional( DaoContext *self, DaoVmCode *vmc, int index, int entry )
{
	int count = DaoValue_GetInteger( self->regValues[ vmc->b ] );
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
		array = DaoContext_PutArray( self );
		break;
	case DVM_FUNCT_STRING : string = DaoContext_PutMBString( self, "" ); break;
	case DVM_FUNCT_LIST : list = DaoContext_PutList( self ); break;
	default : break;
	}
	for(i=0; i<count; i++){
		if( isconst == 0 ){
			self->regValues[index]->xInteger.value = i;
			DaoVmProcess_ExecuteSection( self->process, entry );
			if( self->process->status == DAO_VMPROC_ABORTED ) break;
		}
		res = self->process->returned;
		switch( vmc->a ){
		case DVM_FUNCT_STRING :
			if( stype && res->type == DAO_STRING && res->xString.data->wcs ) DString_ToWCS( string );
			DString_Append( string, DaoValue_GetString( res, self->process->mbstring ) );
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
		if( DaoArray_FromList( array, list, self->regTypes[ vmc->c ] ) ==0 ){
			DaoContext_RaiseException( self, DAO_ERROR, "invalid array()" );
		}
		DaoList_Delete( list );
	}
#else
	if( vmc->a == DVM_FUNCT_ARRAY ){
		DaoContext_RaiseException( self, DAO_ERROR, "numeric array is disabled" );
	}
#endif
}
static void DaoContext_ApplyList( DaoContext *self, DaoVmCode *vmc, int index, int vdim, int entry )
{
	DaoValue *res = NULL;
	DaoValue *param = self->regValues[ vmc->b ];
	DaoList *list = & param->xList;
	int i, size = list->items->size;

	for(i=0; i<size; i++){
		/* Set the iteration variable's value in dao and execute the inline code. */
		self->regValues[index]->xInteger.value = i;
		DaoVmProcess_ExecuteSection( self->process, entry );
		if( self->process->status == DAO_VMPROC_ABORTED ) break;
		res = self->process->returned;

		/* Now we need to replace the current list's content with the one returned
		 * by the inline code, that is placed in "res".
		 */
		DaoList_SetItem( list, res, i );
	}
	self->vmc = vmc; /* it is changed! */
}
void DaoContext_DoFunctional( DaoContext *self, DaoVmCode *vmc )
{
	DaoValue *param = self->regValues[ vmc->b ];
	DaoTuple *tuple = & param->xTuple;
	DaoVmCode *vmcs = self->codes;
	int entry = (int)( self->vmc - vmcs );
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
		DaoContext_RaiseException( self, DAO_ERROR, "code block not found" );
		return;
	}
	entry = i;
	switch( vmc->a ){
	case DVM_FUNCT_APPLY :
		if( param->type == DAO_LIST ){
			DaoContext_ApplyList( self, vmc, index, idc-1, entry );
		}else if( param->type == DAO_ARRAY ){
#ifdef DAO_WITH_NUMARRAY
			DaoContext_Apply( self, vmc, index, idc-1, entry );
#else
			DaoContext_RaiseException( self, DAO_DISABLED_NUMARRAY, NULL );
#endif
		}else{
			DaoContext_RaiseException( self, DAO_ERROR, "apply currently is only supported for numeric arrays and lists" );
		}
		break;
	case DVM_FUNCT_SORT :
		DaoContext_Sort( self, vmc, index, entry );
		break;
	case DVM_FUNCT_FOLD :
		if( param->type != DAO_ARRAY && param->type != DAO_LIST && param->type != DAO_TUPLE ){
			DaoContext_RaiseException( self, DAO_ERROR, "invalid fold/reduce()" );
			break;
		}
		if( param->type == DAO_TUPLE ){
			if( tuple->items->size ==0 ) break;
			if( tuple->items->items.pValue[0]->type == DAO_LIST )
				DaoContext_Fold( self, vmc, index, entry );
			else if( tuple->items->items.pValue[0]->type == DAO_ARRAY )
				DaoContext_Fold2( self, vmc, index, entry );
		}else if( param->type == DAO_ARRAY ){
			DaoContext_Fold2( self, vmc, index, entry );
		}else{
			DaoContext_Fold( self, vmc, index, entry );
		}
		break;
	case DVM_FUNCT_UNFOLD :
		DaoContext_Unfold( self, vmc, index, entry );
		break;
	case DVM_FUNCT_MAP :
	case DVM_FUNCT_SELECT :
	case DVM_FUNCT_INDEX :
	case DVM_FUNCT_COUNT :
	case DVM_FUNCT_EACH :
		DaoContext_MapSIC( self, vmc, index, entry );
		break;
	case DVM_FUNCT_REPEAT :
	case DVM_FUNCT_STRING :
	case DVM_FUNCT_ARRAY :
	case DVM_FUNCT_LIST :
		DaoContext_DataFunctional( self, vmc, index, entry );
		break;
	default : break;
	}
	self->process->status = DAO_VMPROC_RUNNING;
	self->vmc = vmc; /* it is changed! */
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
void DaoPrintException( DaoCData *except, DaoStream *stream )
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

	if( ex->data->type == DAO_STRING ){
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
void DaoVmProcess_PrintException( DaoVmProcess *self, int clear )
{
	DaoType *extype = dao_Exception_Typer.priv->abtype;
	DaoStream *stdio = self->vmSpace->stdStream;
	DaoValue **excobjs = self->exceptions->items.pValue;
	int i;
	for(i=0; i<self->exceptions->size; i++){
		DaoCData *cdata = NULL;
		if( excobjs[i]->type == DAO_CDATA ){
			cdata = & excobjs[i]->xCdata;
		}else if( excobjs[i]->type == DAO_OBJECT ){
			cdata = (DaoCData*)DaoObject_MapThisObject( & excobjs[i]->xObject, extype );
		}
		if( cdata == NULL ) continue;
		DaoPrintException( cdata, stdio );
	}
	if( clear ) DArray_Clear( self->exceptions );
}

DaoValue* DaoVmProcess_MakeConst( DaoVmProcess *self )
{
	uchar_t  modes[] = { 0, 0, 0 };
	DaoType *types[] = { NULL, NULL, NULL };
	DaoVmCodeX vmcx = {0,0,0,0,0,0,0,0,0};
	DaoContext *ctx = self->topFrame->context;
	DaoVmCode *vmc = ctx->vmc;
	DaoValue *dC = ctx->regValues[ vmc->c ];

	dao_fe_clear();
	ctx->idClearFE = -1;
	ctx->codes = vmc;
	if( ctx->regTypes == NULL ) ctx->regTypes = types;
	if( ctx->regModes == NULL ) ctx->regModes = modes;
	if( ctx->routine->annotCodes->size == 0 ) DArray_Append( ctx->routine->annotCodes, & vmcx );

	switch( vmc->code ){
	case DVM_MOVE :
		DaoContext_DoMove( ctx, vmc ); break;
	case DVM_ADD : case DVM_SUB : case DVM_MUL :
	case DVM_DIV : case DVM_MOD : case DVM_POW :
		DaoContext_DoBinArith( ctx, vmc );
		break;
	case DVM_AND : case DVM_OR : case DVM_LT :
	case DVM_LE :  case DVM_EQ : case DVM_NE :
		DaoContext_DoBinBool( ctx, vmc );
		break;
	case DVM_IN :
		DaoContext_DoInTest( ctx, vmc );
		break;
	case DVM_NOT : case DVM_UNMS :
		DaoContext_DoUnaArith( ctx, vmc ); break;
	case DVM_BITAND : case DVM_BITOR : case DVM_BITXOR :
		DaoContext_DoBitLogic( ctx, vmc ); break;
	case DVM_BITLFT : case DVM_BITRIT :
		DaoContext_DoBitShift( ctx, vmc ); break;
	case DVM_BITREV :
		DaoContext_DoBitFlip( ctx, vmc ); break;
	case DVM_CHECK :
		DaoContext_DoCheck( ctx, vmc ); break;
	case DVM_NAMEVA :
		DaoContext_BindNameValue( ctx, vmc ); break;
	case DVM_PAIR :
		DaoContext_DoPair( ctx, vmc ); break;
	case DVM_TUPLE :
		DaoContext_DoTuple( ctx, vmc ); break;
	case DVM_GETI :
	case DVM_GETMI :
		DaoContext_DoGetItem( ctx, vmc ); break;
	case DVM_GETF :
		DaoContext_DoGetField( ctx, vmc ); break;
	case DVM_SETI :
	case DVM_SETMI :
		DaoContext_DoSetItem( ctx, vmc ); break;
	case DVM_SETF :
		DaoContext_DoSetField( ctx, vmc ); break;
	case DVM_LIST :
		DaoContext_DoList( ctx, vmc ); break;
	case DVM_MAP :
	case DVM_HASH :
		DaoContext_DoMap( ctx, vmc ); break;
	case DVM_ARRAY :
		DaoContext_DoArray( ctx, vmc ); break;
	case DVM_MATRIX :
		DaoContext_DoMatrix( ctx, vmc ); break;
	case DVM_MATH :
		DaoVM_DoMath( ctx, vmc, dC, ctx->regValues[1] );
		break;
	case DVM_CURRY :
	case DVM_MCURRY :
		DaoContext_DoCurry( ctx, vmc );
		break;
	case DVM_CALL :
	case DVM_MCALL :
		DaoContext_DoCall( ctx, vmc ); break;
	default: break;
	}
	ctx->regTypes = NULL;
	ctx->regModes = NULL;
	DaoContext_CheckFE( ctx );
	if( self->exceptions->size >0 ){
		DaoVmProcess_PrintException( self, 1 );
		DTuple_Clear( ctx->regArray );
		return NULL;
	}

	/* avoid GC */
	/* DArray_Clear( ctx->regArray ); */
	return dC;
}
