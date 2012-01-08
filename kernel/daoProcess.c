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

extern DMutex mutex_routine_specialize;
extern DMutex mutex_routine_specialize2;

struct DaoJIT dao_jit = { NULL, NULL, NULL, NULL };


static DaoArray* DaoProcess_GetArray( DaoProcess *self, DaoVmCode *vmc );
static DaoList* DaoProcess_GetList( DaoProcess *self, DaoVmCode *vmc );
static DaoMap* DaoProcess_GetMap( DaoProcess *self, DaoVmCode *vmc );

static void DaoProcess_DoMap( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoList( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoPair( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoTuple( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoArray( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoMatrix( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoCurry( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoCheck( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_BindNameValue( DaoProcess *self, DaoVmCode *vmc );

static void DaoProcess_DoGetItem( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoSetItem( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoGetField( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoSetField( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoGetMetaField( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoSetMetaField( DaoProcess *self, DaoVmCode *vmc );

static void DaoProcess_DoIter( DaoProcess *self, DaoVmCode *vmc );

static int DaoProcess_TryObjectArith( DaoProcess *self, DaoValue *A, DaoValue *B, DaoValue *C );
static int DaoProcess_TryCdataArith( DaoProcess *self, DaoValue *A, DaoValue *B, DaoValue *C );

static void DaoProcess_DoInTest( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoBinArith( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoBinBool(  DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoUnaArith( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoBitLogic( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoBitShift( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoBitFlip( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoBitFlip( DaoProcess *self, DaoVmCode *vmc );

static void DaoProcess_DoCast( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoCall( DaoProcess *self, DaoVmCode *vmc );

/* if return TRUE, there is exception, and look for the next rescue point. */
static int DaoProcess_DoCheckExcept( DaoProcess *self, DaoVmCode *vmc );
/* if return DAO_STATUS_EXCEPTION, real exception is rose, and look for the next rescue point. */
static void DaoProcess_DoRaiseExcept( DaoProcess *self, DaoVmCode *vmc );
/* return TRUE, if some exceptions can be rescued */
static int DaoProcess_DoRescueExcept( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_RaiseTypeError( DaoProcess *self, DaoType *from, DaoType *to, const char *op );
static void DaoPrintException( DaoException *except, DaoStream *stream );

static void DaoProcess_MakeRoutine( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_MakeClass( DaoProcess *self, DaoVmCode *vmc );

static DaoVmCode* DaoProcess_DoSwitch( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoMove( DaoProcess *self, DaoVmCode *vmc );
static void DaoProcess_DoReturn( DaoProcess *self, DaoVmCode *vmc );
static int DaoVM_DoMath( DaoProcess *self, DaoVmCode *vmc, DaoValue *C, DaoValue *A );

void DaoArray_number_op_array( DaoArray *C, DaoValue *A, DaoArray *B, short op, DaoProcess *ctx );
void DaoArray_array_op_number( DaoArray *C, DaoArray *A, DaoValue *B, short op, DaoProcess *ctx );
void DaoArray_ArrayArith( DaoArray *s, DaoArray *l, DaoArray *r, short p, DaoProcess *c );
void DaoProcess_ShowCallError( DaoProcess *self, DaoRoutine *rout, DaoValue *selfobj, DaoValue *ps[], int np, int code );

extern void DaoProcess_Trace( DaoProcess *self, int depth );

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
	char buf[50];
	int i;
	if( mbs && src->wcs ) DString_ToMBS( src );
	if( mbs==0 && src->mbs ) DString_ToWCS( src );
	DString_Trim( src );
	if( src->size ==0 ){
		DaoProcess_RaiseException( self, DAO_ERROR, "pattern with empty string" );
		return NULL;
	}
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
	if( self->topFrame->routine && self->topFrame->routine->body && self->activeCode ){
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
	GC_DecRC( self->topFrame->retype );
	self->topFrame->retype = NULL;
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
	DaoType **types = routine->body->regType->items.pType;
	size_t *id = routine->body->simpleVariables->items.pSize;
	size_t *end = id + routine->body->simpleVariables->size;
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
	GC_ShiftRC( routine, frame->routine );
	frame->routine = routine;
	frame->codes = routine->body->vmCodes->codes;
	frame->types = routine->body->regType->items.pType;
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
		case DAO_LONG    : value2 = (DaoValue*) DaoLong_New(); break;
		case DAO_STRING  : value2 = (DaoValue*) DaoString_New(1); break;
		case DAO_ENUM    : value2 = (DaoValue*) DaoEnum_New( types[i], 0 ); break;
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
	if( frame->routine ) self->activeNamespace = frame->routine->nameSpace;
}
void DaoProcess_PushRoutine( DaoProcess *self, DaoRoutine *routine, DaoObject *object )
{
	DaoStackFrame *frame = DaoProcess_PushFrame( self, routine->body->regCount );
	DaoProcess_InitTopFrame( self, routine, object );
	frame->active = frame;
	self->status = DAO_VMPROC_STACKED;
}
void DaoProcess_PushFunction( DaoProcess *self, DaoRoutine *routine )
{
	DaoStackFrame *frame = DaoProcess_PushFrame( self, routine->parCount );
	frame->active = frame->prev->active;
	GC_ShiftRC( routine, frame->routine );
	frame->routine = routine;
	self->status = DAO_VMPROC_STACKED;
}
static int DaoRoutine_PassDefault( DaoRoutine *routine, DaoValue *dest[], int passed, DMap *defs )
{
	DaoType *tp, *routype = routine->routType;
	DaoType **types = routype->nested->items.pType;
	DaoValue **consts = routine->routConsts->items.items.pValue;
	int i, ndef = routine->parCount;
	for(i=0; i<ndef; i++){
		int m = types[i]->tid;
		if( m == DAO_PAR_VALIST ) break;
		if( passed & (1<<i) ) continue;
		if( m != DAO_PAR_DEFAULT ) return 0;
		tp = & types[i]->aux->xType;
		if( defs && tp && (tp->attrib & DAO_TYPE_NOTDEF) ){
			tp = DaoType_DefineTypes( tp, routine->nameSpace, defs );
			//XXX printf( "tp = %s\n", tp->name->mbs );
		}
		if( DaoValue_Move2( consts[i], & dest[i], tp ) ==0 ) return 0;
	}
	return 1;
}
void DaoRoutine_MapTypes( DaoRoutine *self, DMap *deftypes );
int DaoRoutine_Finalize( DaoRoutine *self, DaoType *host, DMap *deftypes );
/* Return 0 if failed, otherwise return 1 plus number passed parameters: */
static int DaoRoutine_PassParams( DaoRoutine **routine2, DaoValue *dest[], DaoType *hostype, DaoValue *obj, DaoValue *p[], int np, int code )
{
	DMap *defs = NULL;
	DaoRoutine *routine = *routine2;
	DaoType *routype = routine->routType;
	DaoType *tp, **types = routype->nested->items.pType;
	ulong_t passed = 0;
	int mcall = code == DVM_MCALL;
	int need_self = routype->attrib & DAO_TYPE_SELF;
	int need_spec = routype->attrib & DAO_TYPE_NOTDEF;
	int ndef = routine->parCount;
	int npar = np;
	int ifrom, ito;
	int selfChecked = 0;
#if 0
	int i;
	printf( "%s: %i %i %i\n", routine->routName->mbs, ndef, np, obj ? obj->type : 0 );
	for(i=0; i<npar; i++){
		tp = DaoNamespace_GetType( routine->nameSpace, p[i] );
		printf( "%i  %s\n", i, tp->name->mbs );
	}
#endif

	if( need_spec ){
		defs = DHash_New(0,0);
		if( hostype && routine->routHost && (routine->routHost->attrib & DAO_TYPE_NOTDEF) ){
			//XXX printf( "%s %s\n", hostype->name->mbs, routine->routHost->name->mbs );
			/* Init type specialization mapping for static methods: */
			DaoType_MatchTo( hostype, routine->routHost, defs );
		}
	}

	if( mcall && ! need_self ){
		npar --;
		p ++;
	}else if( obj && need_self && ! mcall ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		tp = & types[0]->aux->xType;
		if( defs && tp && (tp->attrib & DAO_TYPE_NOTDEF) ){
			DaoType *type = DaoNamespace_GetType( routine->nameSpace, obj );
			DaoType_MatchTo( type, tp, defs ); /* Init type specialization mapping; */
			/* Specialize types: */
			if( defs->size ) tp = DaoType_DefineTypes( tp, routine->nameSpace, defs );
		}
		if( obj->type < DAO_ARRAY ){
			if( tp == NULL || DaoType_MatchValue( tp, obj, defs ) == DAO_MT_EQ ){
				GC_ShiftRC( obj, dest[0] );
				dest[0] = obj;
				selfChecked = 1;
				passed = 1;
			}
		}else{
			if( obj->type == DAO_OBJECT && (tp->tid ==DAO_OBJECT || tp->tid ==DAO_CDATA) ){
				/* for virtual method call, or calling C function on Dao object: */
				obj = DaoObject_MapThisObject( obj->xObject.rootObject, tp );
			}
			if( DaoValue_Move2( obj, & dest[0], tp ) ){
				selfChecked = 1;
				passed = 1;
			}
		}
	}
	/*
	   printf( "%s, rout = %s; ndef = %i; npar = %i, %i\n", routine->routName->mbs, routine->routType->name->mbs, ndef, npar, selfChecked );
	 */
	if( npar > ndef ) goto ReturnZero;
	if( (npar|ndef) ==0 ){
		if( defs ) DMap_Delete( defs );
		return 1;
	}
	/* pass from p[ifrom] to dest[ito], with type checking by types[ito] */
	for(ifrom=0; ifrom<npar; ifrom++){
		DaoValue *val = p[ifrom];
		ito = ifrom + selfChecked;
		if( ito < ndef && types[ito]->tid == DAO_PAR_VALIST ){
			for(; ifrom<npar; ifrom++){
				ito = ifrom + selfChecked;
				DaoValue_Move2( p[ifrom], & dest[ito], NULL );
				passed |= (ulong_t)1<<ito;
			}
			break;
		}
		if( val->type == DAO_PAR_NAMED ){
			DaoNameValue *nameva = & val->xNameValue;
			DNode *node = DMap_Find( routype->mapNames, nameva->name );
			val = nameva->value;
			if( node == NULL ) goto ReturnZero;
			ito = node->value.pInt;
		}
		if( ito >= ndef ) goto ReturnZero;
		passed |= (ulong_t)1<<ito;
		tp = & types[ito]->aux->xType;
		if( defs && tp && (tp->attrib & DAO_TYPE_NOTDEF) ){
			DaoType *type = DaoNamespace_GetType( routine->nameSpace, val );
			int mt = DaoType_MatchTo( type, tp, defs ); /* Init type specialization mapping; */
			/* Specialize types: */
			if( defs->size ) tp = DaoType_DefineTypes( tp, routine->nameSpace, defs );
			if( (routine->refParams & (1<<ito)) && mt == DAO_MT_EQ ){
				GC_ShiftRC( val, dest[ito] );
				dest[ito] = val;
				continue;
			}
		}else{
			if( routine->refParams & (1<<ito) ){
				if( DaoType_MatchValue( tp, val, defs ) == DAO_MT_EQ ){
					GC_ShiftRC( val, dest[ito] );
					dest[ito] = val;
					continue;
				}
			}
		}
		if( need_self && ifrom ==0 && val->type == DAO_OBJECT && (tp->tid ==DAO_OBJECT || tp->tid ==DAO_CDATA ) ){
			/* for virtual method call */
			val = (DaoValue*) DaoObject_MapThisObject( val->xObject.rootObject, tp );
			if( val == NULL ) goto ReturnZero;
		}
		if( DaoValue_Move2( val, & dest[ito], tp ) ==0 ) goto ReturnZero;
	}
	if( DaoRoutine_PassDefault( routine, dest, passed, defs ) == 0) goto ReturnZero;
	if( defs && defs->size ){ /* Need specialization */
		DaoRoutine *original = routine->original ? routine->original : routine;
		routine = DaoRoutine_Copy( original, 0, 0 );
		DaoRoutine_Finalize( routine, routine->routHost, defs );

		DMutex_Lock( & mutex_routine_specialize );
		if( original->specialized == NULL ) original->specialized = DRoutines_New();
		DMutex_Unlock( & mutex_routine_specialize );

		GC_ShiftRC( original, routine->original );
		DRoutines_Add( original->specialized, routine );
		routine->original = original;
	}
	if( routine->original && routine->body && routine->body == routine->original->body ){
		/* Specialize routine body (local types and VM instructions): */
		DMutex_Lock( & mutex_routine_specialize2 );
		if( routine->body == routine->original->body ){
			DaoRoutineBody *body = DaoRoutineBody_Copy( routine->body );
			DMap *defs2 = DHash_New(0,0);

			DaoType_MatchTo( routine->routType, routine->original->routType, defs2 );
			GC_ShiftRC( body, routine->body );
			routine->body = body;
			/* Only specialize explicitly declared variables: */
			DaoRoutine_MapTypes( routine, defs2 );
			DMap_Delete( defs2 );
			if( DaoRoutine_DoTypeInference( routine, 1 ) == 0 ){
				/* Specialization may fail at unreachable parts for certain parameters.
				 * Example: binary tree benchmark using list (binary_tree2.dao). */
				GC_ShiftRC( (*routine2)->body, routine->body );
				routine->body = (*routine2)->body;
			}
		}
		DMutex_Unlock( & mutex_routine_specialize2 );
	}
	*routine2 = routine;
	if( defs ) DMap_Delete( defs );
	return 1 + npar + selfChecked;
ReturnZero:
	if( defs ) DMap_Delete( defs );
	return 0;
}
/* If the callable is a constructor, and O is a derived type of the constructor's type,
 * cast O to the constructor's type and then call the constructor on the casted object: */
int DaoProcess_PushCallable( DaoProcess *self, DaoRoutine *R, DaoValue *O, DaoValue *P[], int N )
{
	int passed = 0;

	if( R == NULL ) return DAO_ERROR;
	R = DaoRoutine_ResolveX( R, O, P, N, DVM_CALL );
	if( R == NULL ) return DAO_ERROR_PARAM;
	passed = DaoRoutine_PassParams( & R, self->freeValues, NULL, O, P, N, DVM_CALL );
	if( passed == 0 ) return DAO_ERROR_PARAM;

	if( R->body ){
		DaoProcess_PushRoutine( self, R, DaoValue_CastObject( O ) );
	}else{
		DaoProcess_PushFunction( self, R );
	}
	self->topFrame->parCount = passed - 1;
	return 0;
}
void DaoProcess_InterceptReturnValue( DaoProcess *self )
{
	if( self->topFrame->routine && self->topFrame->routine->body ){
		self->topFrame->returning = -1;
	}else{
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
		DaoRoutine *rout = self->topFrame->routine;
		DaoValue **values = self->stackValues + self->topFrame->stackBase;
		if( rout ) m = DaoRoutine_PassParams( & rout, values, NULL, NULL, par, N, DVM_CALL );
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
	int nop = 0;
	if( frame->routine ) cbtype = frame->routine->routType->cbtype;
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
			if( frame->routine->body ){
				codes = frame->codes + frame->entry;
				nop = codes[1].code == DVM_NOP;
				if( codes[nop].code == DVM_GOTO && codes[nop+1].code == DVM_SECT ) return frame;
			}
		}
		if( cbtype2 == NULL || DaoType_MatchTo( cbtype, cbtype2, NULL ) == 0 ) break;
		frame = frame->prev;
	}
	if( frame == NULL || frame->routine == NULL ) return NULL;
	codes = frame->codes + frame->entry;
	nop = codes[1].code == DVM_NOP;
	if( codes[nop].code == DVM_GOTO && codes[nop+1].code == DVM_SECT ) return frame;
	return NULL;
}
DaoStackFrame* DaoProcess_PushSectionFrame( DaoProcess *self )
{
	DaoStackFrame *next, *frame = DaoProcess_FindSectionFrame( self );
	int returning = -1;

	if( frame == NULL ) return NULL;
	if( self->topFrame->routine->body ){
		self->topFrame->entry = 1 + self->activeCode - self->topFrame->codes;
		returning = self->activeCode->c;
	}
	next = DaoProcess_PushFrame( self, 0 );
	next->entry = frame->entry + 2;
	next->state = DVM_FRAME_SECT | DVM_FRAME_KEEP;
	next->depth = 0;

	GC_ShiftRC( frame->object, next->object );
	GC_ShiftRC( frame->routine, next->routine );
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
	p->vmSpace = self->vmSpace;
	p->nameSpace = ns;
	DString_Assign( p->fileName, ns->name );
	res = DaoParser_LexCode( p, src->mbs, rpl ) && DaoParser_ParseScript( p );
	p->routine->body->parser = NULL;
	DaoParser_Delete( p );
	DString_Delete( src );
	return res;
}
int DaoProcess_Eval( DaoProcess *self, DaoNamespace *ns, DString *source, int rpl )
{
	DaoRoutine *rout;
	DString_SetMBS( ns->name, "code string" );
	if( DaoProcess_Compile( self, ns, source, rpl ) ==0 ) return 0;
	rout = ns->mainRoutines->items.pRoutine[ ns->mainRoutines->size-1 ];
	if( DaoProcess_Call( self, rout, NULL, NULL, 0 ) ) return 0;
	return ns->mainRoutines->size;
}
// XXX return value changed!!
int DaoProcess_Call( DaoProcess *self, DaoRoutine *M, DaoValue *O, DaoValue *P[], int N )
{
	int ret = DaoProcess_PushCallable( self, M, O, P, N );
	if( ret ) return ret;
	/* no return value to the previous stack frame */
	DaoProcess_InterceptReturnValue( self );
	ret = DaoProcess_Execute( self ) == 0 ? DAO_ERROR : 0;
	if( self->stdioStream ) DaoStream_Flush( self->stdioStream );
	DaoStream_Flush( self->vmSpace->stdioStream );
	DaoStream_Flush( self->vmSpace->errorStream );
	fflush( stdout );
	return ret;
}
void DaoProcess_CallFunction( DaoProcess *self, DaoRoutine *func, DaoValue *p[], int n )
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

static int DaoProcess_CheckFE( DaoProcess *self );
static int DaoProcess_Move( DaoProcess *self, DaoValue *A, DaoValue **C, DaoType *t );
static void DaoProcess_AdjustCodes( DaoProcess *self, int options );

#if defined( __GNUC__ ) && !defined( __STRICT_ANSI__ )
#define HAS_VARLABEL
#endif
#if 0
#endif

int DaoProcess_Execute( DaoProcess *self )
{
	DaoJitCallData jitCallData = {NULL};
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
	DArray   *NSS;
	DArray   *CSS = NULL;
	DArray   *dataCL[2] = { NULL, NULL };
	DaoProcess *dataVH[DAO_MAX_SECTDEPTH] = { NULL, NULL, NULL, NULL };
	DaoValue  **dataVL = NULL;
	DaoValue  **dataVO = NULL;
	DArray   *typeVL = NULL;
	DArray   *typeVO = NULL;
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
	complex16 czero = {0,0};
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
		&& LAB_GETI  , && LAB_GETDI , && LAB_GETMI , && LAB_GETF  , && LAB_GETMF ,
		&& LAB_SETVH , && LAB_SETVL , && LAB_SETVO , && LAB_SETVK , && LAB_SETVG ,
		&& LAB_SETI  , && LAB_SETDI , && LAB_SETMI , && LAB_SETF  , && LAB_SETMF ,
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

		&& LAB_ADD_CC , && LAB_SUB_CC ,
		&& LAB_MUL_CC , && LAB_DIV_CC ,

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

		&& LAB_GETI_ACI , && LAB_SETI_ACI ,

		&& LAB_GETMI_A , && LAB_SETMI_A ,

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
		&& LAB_CHECK_ST ,

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
#define OPBEGIN() for(;;){ printf("%3i:", (i=vmc-vmcBase) ); DaoVmCodeX_Print( *topFrame->routine->body->annotCodes->items.pVmc[i], NULL ); switch( vmc->code )
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
	if( topFrame->routine && topFrame->routine->pFunc ){
		DaoValue **p = self->stackValues + topFrame->stackBase;
		DaoProcess_CallFunction( self, topFrame->routine, p, topFrame->parCount );
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
	printf("number of instruction: %i\n", routine->body->vmCodes->size );
	if( routine->routType ) printf("routine type = %s\n", routine->routType->name->mbs);
	printf( "vmSpace = %p; nameSpace = %p\n", self->vmSpace, topCtx->nameSpace );
	printf("routine = %p; context = %p\n", routine, topCtx );
	printf( "self object = %p\n", topCtx->object );
#endif

	if( self->stopit | vmSpace->stopit ) goto FinishProc;
	//XXX if( invokehost ) handler->InvokeHost( handler, topCtx );

	if( (vmSpace->options & DAO_EXEC_DEBUG) | (routine->body->mode & DAO_EXEC_DEBUG) )
		DaoProcess_AdjustCodes( self, vmSpace->options );

	vmcBase = topFrame->codes;
	id = self->topFrame->entry;
	if( id >= routine->body->vmCodes->size ){
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
	printf("number of register: %i\n", routine->body->regCount );
	printf("number of instruction: %i\n", routine->body->vmCodes->size );
	printf( "VM process: %p\n", self );
	printf("==================================================\n");
	DaoRoutine_PrintCode( routine, self->vmSpace->stdioStream );
#endif

	vmc = vmcBase + id;
	self->stopit = 0;
	self->activeCode = vmc;
	self->activeRoutine = routine;
	self->activeObject = topFrame->object;
	self->activeValues = self->stackValues + topFrame->stackBase;
	self->activeTypes = routine->body->regType->items.pType;
	self->activeNamespace = routine->nameSpace;

	/* range ( 0, routine->body->vmCodes->size-1 ) */
	if( id ==0 ) DaoStackFrame_PushRange( topFrame, 0, (routine->body->vmCodes->size-1) );

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
			(vmc->code == DVM_CALL || vmc->code == DVM_MCALL || vmc->code == DVM_YIELD) ){
		if( self->pauseType == DAO_VMP_ASYNC && self->future->precondition ){
			int finished = self->future->precondition->state == DAO_CALL_FINISHED;
			if( self->future->state2 == DAO_FUTURE_VALUE ){
				DaoValue *res = finished ? self->future->precondition->value : dao_none_value;
				DaoProcess_PutValue( self, res );
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
	dataCL[0] = & routine->routConsts->items;
	NSS = here->namespaces;
	if( routine->body->jitData ){
		jitCallData.localValues = locVars;
		jitCallData.localConsts = routine->routConsts->items.items.pValue;
		jitCallData.globalValues = here->varData->items.pValue;
		jitCallData.globalConsts = here->cstData->items.pValue;
		jitCallData.namespaces = NSS;
		jitCallData.processes = dataVH;
	}
	if( ROUT_HOST_TID( routine ) == DAO_OBJECT ){
		host = & routine->routHost->aux->xClass;
		CSS = host->classes;
		jitCallData.classes = CSS;
		jitCallData.classValues = host->glbData->items.pValue;
		jitCallData.classConsts = host->cstData->items.pValue;
		if( !(routine->attribs & DAO_ROUT_STATIC) ){
			dataVO = this->objValues;
			typeVO = host->objDataType;
			jitCallData.objectValues = dataVO;
		}
	}
	if( routine->body->upRoutine ){
		dataCL[1] = & routine->body->upRoutine->routConsts->items;
		dataVL = routine->body->upProcess->stackValues + 1;
		typeVL = routine->body->upRoutine->body->regType;
		jitCallData.upConsts = dataCL[1]->items.pValue;
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
			if( vmc->a == DAO_NONE ){
				GC_ShiftRC( dao_none_value, locVars[ vmc->c ] );
				locVars[ vmc->c ] = dao_none_value;
			}else{
				value = locVars[vmc->c];
				if( value == NULL || value->type != vmc->a ){
					DaoValue *tmp = (DaoValue*) DaoComplex_New(czero);
					tmp->type = vmc->a;
					GC_ShiftRC( tmp, value );
					locVars[ vmc->c ] = value = tmp;
				}
				switch( vmc->a ){
				case DAO_COMPLEX :
					value->xComplex.value.real = 0;
					value->xComplex.value.imag = vmc->b;
					break;
				case DAO_INTEGER : value->xInteger.value = vmc->b; break;
				case DAO_FLOAT  : value->xFloat.value = vmc->b; break;
				case DAO_DOUBLE : value->xDouble.value = vmc->b; break;
				default : break;
				}
			}
		}OPNEXT() OPCASE( GETCL ){
			/* All GETX instructions assume the C regisgter is an intermediate register! */
			value = dataCL[ vmc->a ]->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETCK ){
			value = CSS->items.pClass[ vmc->a ]->cstData->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETCG ){
			value = NSS->items.pNS[ vmc->a ]->cstData->items.pValue[ vmc->b ];
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
			value = CSS->items.pClass[vmc->a]->glbData->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETVG ){
			value = NSS->items.pNS[vmc->a]->varData->items.pValue[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( GETI ) OPCASE( GETDI ) OPCASE( GETMI ){
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
			if( DaoProcess_Move( self, locVars[vmc->a], dataVH[ vmc->c ]->activeValues + vmc->b, abtp ) ==0 )
				goto CheckException;
		}OPNEXT() OPCASE( SETVL ){
			abtp = typeVL->items.pType[ vmc->b ];
			if( DaoProcess_Move( self, locVars[vmc->a], dataVL + vmc->b, abtp ) ==0 )
				goto CheckException;
		}OPNEXT() OPCASE( SETVO ){
			abtp = typeVO->items.pType[ vmc->b ];
			if( DaoProcess_Move( self, locVars[vmc->a], dataVO + vmc->b, abtp ) ==0 )
				goto CheckException;
		}OPNEXT() OPCASE( SETVK ){
			abtp = CSS->items.pClass[vmc->c]->glbDataType->items.pType[ vmc->b ];
			vref = CSS->items.pClass[vmc->c]->glbData->items.pValue + vmc->b;
			if( DaoProcess_Move( self, locVars[vmc->a], vref, abtp ) ==0 ) goto CheckException;
		}OPNEXT() OPCASE( SETVG ){
			abtp = NSS->items.pNS[vmc->c]->varType->items.pType[ vmc->b ];
			vref = NSS->items.pNS[vmc->c]->varData->items.pValue + vmc->b;
			if( DaoProcess_Move( self, locVars[vmc->a], vref, abtp ) ==0 ) goto CheckException;
		}OPNEXT() OPCASE( SETI ) OPCASE( SETDI ) OPCASE( SETMI ){
			DaoProcess_DoSetItem( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( SETF ){
			DaoProcess_DoSetField( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( SETMF ){
			DaoProcess_DoSetMetaField( self, vmc );
			goto CheckException;
		}OPNEXT() OPCASE( LOAD ){
			if( (vA = locVars[ vmc->a ]) ){
				/* mt.run(3)::{ mt.critical::{} }: the inner functional will be compiled
				 * as a LOAD and RETURN, but the inner functional will not return anything,
				 * so the first operand of LOAD will be NULL! */
				if( (vA->xNone.trait & DAO_DATA_CONST) == 0 ){
					GC_ShiftRC( vA, locVars[ vmc->c ] );
					locVars[ vmc->c ] = vA;
				}else{
					DaoValue_Copy( vA, & locVars[ vmc->c ] );
				}
			}
		}OPNEXT() OPCASE( CAST ){
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
			case DAO_NONE :
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
				vmc = vA->xList.items.size ? vmc+1 : vmcBase + vmc->b;
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
			jitCallData.globalValues = here->varData->items.pValue;
			dao_jit.Execute( self, & jitCallData, vmc->a );
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
			value = CSS->items.pClass[ vmc->a ]->cstData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xInteger.value = value->xInteger.value;
		}OPNEXT() OPCASE( GETCK_F ){
			value = CSS->items.pClass[ vmc->a ]->cstData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xFloat.value = value->xFloat.value;
		}OPNEXT() OPCASE( GETCK_D ){
			value = CSS->items.pClass[ vmc->a ]->cstData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xDouble.value = value->xDouble.value;
		}OPNEXT() OPCASE( GETCG_I ){
			value = NSS->items.pNS[ vmc->a ]->cstData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xInteger.value = value->xInteger.value;
		}OPNEXT() OPCASE( GETCG_F ){
			value = NSS->items.pNS[ vmc->a ]->cstData->items.pValue[ vmc->b ];
			locVars[ vmc->c ]->xFloat.value = value->xFloat.value;
		}OPNEXT() OPCASE( GETCG_D ){
			value = NSS->items.pNS[ vmc->a ]->cstData->items.pValue[ vmc->b ];
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
			IntegerOperand( vmc->c ) = CSS->items.pClass[vmc->a]->glbData->items.pInteger[vmc->b]->value;
		}OPNEXT() OPCASE( GETVK_F ){
			FloatOperand( vmc->c ) = CSS->items.pClass[vmc->a]->glbData->items.pFloat[vmc->b]->value;
		}OPNEXT() OPCASE( GETVK_D ){
			DoubleOperand( vmc->c ) = CSS->items.pClass[vmc->a]->glbData->items.pDouble[vmc->b]->value;
		}OPNEXT() OPCASE( GETVG_I ){
			IntegerOperand( vmc->c ) = NSS->items.pNS[vmc->a]->varData->items.pInteger[vmc->b]->value;
		}OPNEXT() OPCASE( GETVG_F ){
			FloatOperand( vmc->c ) = NSS->items.pNS[vmc->a]->varData->items.pFloat[vmc->b]->value;
		}OPNEXT() OPCASE( GETVG_D ){
			DoubleOperand( vmc->c ) = NSS->items.pNS[vmc->a]->varData->items.pDouble[vmc->b]->value;
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
			CSS->items.pClass[vmc->c]->glbData->items.pInteger[vmc->b]->value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_IF ){
			CSS->items.pClass[vmc->c]->glbData->items.pInteger[vmc->b]->value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_ID ){
			CSS->items.pClass[vmc->c]->glbData->items.pInteger[vmc->b]->value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_FI ){
			CSS->items.pClass[vmc->c]->glbData->items.pFloat[vmc->b]->value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_FF ){
			CSS->items.pClass[vmc->c]->glbData->items.pFloat[vmc->b]->value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_FD ){
			CSS->items.pClass[vmc->c]->glbData->items.pFloat[vmc->b]->value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_DI ){
			CSS->items.pClass[vmc->c]->glbData->items.pDouble[vmc->b]->value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_DF ){
			CSS->items.pClass[vmc->c]->glbData->items.pDouble[vmc->b]->value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVK_DD ){
			CSS->items.pClass[vmc->c]->glbData->items.pDouble[vmc->b]->value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_II ){
			NSS->items.pNS[vmc->c]->varData->items.pInteger[vmc->b]->value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_IF ){
			NSS->items.pNS[vmc->c]->varData->items.pInteger[vmc->b]->value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_ID ){
			NSS->items.pNS[vmc->c]->varData->items.pInteger[vmc->b]->value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_FI ){
			NSS->items.pNS[vmc->c]->varData->items.pFloat[vmc->b]->value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_FF ){
			NSS->items.pNS[vmc->c]->varData->items.pFloat[vmc->b]->value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_FD ){
			NSS->items.pNS[vmc->c]->varData->items.pFloat[vmc->b]->value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_DI ){
			NSS->items.pNS[vmc->c]->varData->items.pDouble[vmc->b]->value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_DF ){
			NSS->items.pNS[vmc->c]->varData->items.pDouble[vmc->b]->value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETVG_DD ){
			NSS->items.pNS[vmc->c]->varData->items.pDouble[vmc->b]->value = DoubleOperand( vmc->a );
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
			DString_Assign( locVars[ vmc->c ]->xString.data, locVars[ vmc->a ]->xString.data );
		}OPNEXT() OPCASE( MOVE_PP ){
			if( locVars[ vmc->a ] == NULL ) goto RaiseErrorNullObject;
			value = locVars[ vmc->a ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
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
			if( id <0 ) id += list->items.size;
			if( id <0 || id >= list->items.size ) goto RaiseErrorIndexOutOfRange;
			/* All GETX instructions assume the C regisgter is an intermediate register! */
			/* So no type checking is necessary here! */
			value = list->items.items.pValue[id];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( SETI_LI ){
			list = & locVars[ vmc->c ]->xList;
			id = IntegerOperand( vmc->b );
			if( id <0 ) id += list->items.size;
			if( id <0 || id >= list->items.size ) goto RaiseErrorIndexOutOfRange;
			value = locVars[ vmc->a ];
			vC2 = list->items.items.pValue + id;
			GC_ShiftRC( value, *vC2 );
			*vC2 = value;
		}OPNEXT()
		OPCASE( GETI_LII )
			OPCASE( GETI_LFI )
			OPCASE( GETI_LDI )
			OPCASE( GETI_LSI ){
				list = & locVars[ vmc->a ]->xList;
				id = IntegerOperand( vmc->b );
				if( id <0 ) id += list->items.size;
				if( id <0 || id >= list->items.size ) goto RaiseErrorIndexOutOfRange;
				vA = list->items.items.pValue[id];
				switch( vmc->code ){
				case DVM_GETI_LSI :
					GC_ShiftRC( vA, locVars[ vmc->c ] );
					locVars[ vmc->c ] = vA;
					break;
				case DVM_GETI_LII : locVars[ vmc->c ]->xInteger.value = vA->xInteger.value; break;
				case DVM_GETI_LFI : locVars[ vmc->c ]->xFloat.value = vA->xFloat.value; break;
				case DVM_GETI_LDI : locVars[ vmc->c ]->xDouble.value = vA->xDouble.value; break;
				}
			}OPNEXT()
		OPCASE( SETI_LIII )
			OPCASE( SETI_LIIF )
			OPCASE( SETI_LIID ){
				list = & locVars[ vmc->c ]->xList;
				vA = locVars[ vmc->a ];
				id = IntegerOperand( vmc->b );
				switch( vA->type ){
				case DAO_INTEGER : inum = vA->xInteger.value; break;
				case DAO_FLOAT   : inum = (dint) vA->xFloat.value; break;
				case DAO_DOUBLE  : inum = (dint) vA->xDouble.value; break;
				default : inum = 0; break;
				}
				if( id <0 ) id += list->items.size;
				if( id <0 || id >= list->items.size ) goto RaiseErrorIndexOutOfRange;
				list->items.items.pValue[id]->xInteger.value = inum;
			}OPNEXT()
		OPCASE( SETI_LFII )
			OPCASE( SETI_LFIF )
			OPCASE( SETI_LFID ){
				list = & locVars[ vmc->c ]->xList;
				vA = locVars[ vmc->a ];
				id = IntegerOperand( vmc->b );
				switch( vA->type ){
				case DAO_INTEGER : fnum = vA->xInteger.value; break;
				case DAO_FLOAT   : fnum = vA->xFloat.value; break;
				case DAO_DOUBLE  : fnum = vA->xDouble.value; break;
				default : fnum = 0; break;
				}
				if( id <0 ) id += list->items.size;
				if( id <0 || id >= list->items.size ) goto RaiseErrorIndexOutOfRange;
				list->items.items.pValue[id]->xFloat.value = fnum;
			}OPNEXT()
		OPCASE( SETI_LDII )
			OPCASE( SETI_LDIF )
			OPCASE( SETI_LDID ){
				list = & locVars[ vmc->c ]->xList;
				vA = locVars[ vmc->a ];
				id = IntegerOperand( vmc->b );
				switch( vA->type ){
				case DAO_INTEGER : dnum = vA->xInteger.value; break;
				case DAO_FLOAT   : dnum = vA->xFloat.value; break;
				case DAO_DOUBLE  : dnum = vA->xDouble.value; break;
				default : dnum = 0; break;
				}
				if( id <0 ) id += list->items.size;
				if( id <0 || id >= list->items.size ) goto RaiseErrorIndexOutOfRange;
				list->items.items.pValue[id]->xDouble.value = dnum;
			}OPNEXT()
		OPCASE( SETI_LSIS ){
			list = & locVars[ vmc->c ]->xList;
			vA = locVars[ vmc->a ];
			id = IntegerOperand( vmc->b );
			if( id <0 ) id += list->items.size;
			if( id <0 || id >= list->items.size ) goto RaiseErrorIndexOutOfRange;
			DString_Assign( list->items.items.pValue[id]->xString.data, vA->xString.data );
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
			array = & locVars[ vmc->c ]->xArray;
			id = IntegerOperand( vmc->b );
			if( array->original && DaoArray_Sliced( array ) == 0 ) goto RaiseErrorSlicing; 
			if( id <0 ) id += array->size;
			if( id <0 || id >= array->size ) goto RaiseErrorIndexOutOfRange;
			array->data.c[ id ] = locVars[ vmc->a ]->xComplex.value;
		}OPNEXT()
		OPCASE( GETMI_A ){
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
		OPCASE( SETMI_A ){
			array = & locVars[ vmc->c ]->xArray;
			list = & locVars[ vmc->b ]->xList;
			if( array->original && DaoArray_Sliced( array ) == 0 ) goto RaiseErrorSlicing; 
			if( array->ndim == list->items.size ){
				dims = array->dims;
				dmac = array->dims + array->ndim;
				id = 0;
				for(i=0; i<array->ndim; i++){
					j = DaoValue_GetInteger( list->items.items.pValue[i] );
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
			OPCASE( GETMI_A )
			OPCASE( SETMI_A ){
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
			tuple = & locVars[ vmc->c ]->xTuple;
			id = IntegerOperand( vmc->b );
			abtp = NULL;
			if( id <0 || id >= tuple->size ) goto RaiseErrorIndexOutOfRange;
			abtp = tuple->unitype->nested->items.pType[id];
			if( abtp->tid == DAO_PAR_NAMED ) abtp = & abtp->aux->xType;
			if( DaoProcess_Move( self, locVars[vmc->a], tuple->items + id, abtp ) ==0 )
				goto CheckException;
		}OPNEXT() OPCASE( GETF_T ){
			tuple = & locVars[ vmc->a ]->xTuple;
			value = tuple->items[ vmc->b ];
			GC_ShiftRC( value, locVars[ vmc->c ] );
			locVars[ vmc->c ] = value;
		}OPNEXT() OPCASE( SETF_T ){
			tuple = & locVars[ vmc->c ]->xTuple;
			value = locVars[ vmc->a ];
			vC2 = tuple->items + vmc->b;
			GC_ShiftRC( value, *vC2 );
			*vC2 = value;
		}OPNEXT() OPCASE( GETF_TI ){
			/* Do not get reference here!
			 * Getting reference is always more expensive due to reference counting.
			 * The compiler always generates SETX, if element modification is done
			 * through index or field accessing: A[B] += C, A.B += C. */
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
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TIF ){
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TID ){
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TFI ){
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TFF ){
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TFD ){
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TDI ){
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TDF ){
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TDD ){
			tuple = & locVars[ vmc->c ]->xTuple;
			tuple->items[ vmc->b ]->xDouble.value = DoubleOperand( vmc->a );
		}OPNEXT() OPCASE( SETF_TSS ){
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
			value = locVars[vmc->a];
			GC_ShiftRC( value, *vC2 );
			*vC2 = value;
		}OPNEXT() OPCASE( SETF_OG ) OPCASE( SETF_OV ){
			object = & locVars[ vmc->c ]->xObject;
			if( vmc->code == DVM_SETF_OG ){
				klass = ((DaoObject*)klass)->defClass;
				vC2 = klass->glbData->items.pValue + vmc->b;
			}else{
				if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
				vC2 = object->objValues + vmc->b;
			}
			value = locVars[vmc->a];
			GC_ShiftRC( value, *vC2 );
			*vC2 = value;
		}OPNEXT()
		OPCASE( SETF_KGII ){
			klass = & locVars[ vmc->c ]->xClass;
			klass->glbData->items.pInteger[ vmc->b ]->value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_KGIF ){
			klass = & locVars[ vmc->c ]->xClass;
			klass->glbData->items.pInteger[ vmc->b ]->value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_KGID ){
			klass = & locVars[ vmc->c ]->xClass;
			klass->glbData->items.pInteger[ vmc->b ]->value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_KGFI ){
			klass = & locVars[ vmc->c ]->xClass;
			klass->glbData->items.pFloat[ vmc->b ]->value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_KGFF ){
			klass = & locVars[ vmc->c ]->xClass;
			klass->glbData->items.pFloat[ vmc->b ]->value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_KGFD ){
			klass = & locVars[ vmc->c ]->xClass;
			klass->glbData->items.pFloat[ vmc->b ]->value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_KGDI ){
			klass = & locVars[ vmc->c ]->xClass;
			klass->glbData->items.pDouble[ vmc->b ]->value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_KGDF ){
			klass = & locVars[ vmc->c ]->xClass;
			klass->glbData->items.pDouble[ vmc->b ]->value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_KGDD ){
			klass = & locVars[ vmc->c ]->xClass;
			klass->glbData->items.pDouble[ vmc->b ]->value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OGII ){
			klass = locVars[ vmc->c ]->xObject.defClass;
			klass->glbData->items.pInteger[ vmc->b ]->value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OGIF ){
			klass = locVars[ vmc->c ]->xObject.defClass;
			klass->glbData->items.pInteger[ vmc->b ]->value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OGID ){
			klass = locVars[ vmc->c ]->xObject.defClass;
			klass->glbData->items.pInteger[ vmc->b ]->value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OGFI ){
			klass = locVars[ vmc->c ]->xObject.defClass;
			klass->glbData->items.pFloat[ vmc->b ]->value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OGFF ){
			klass = locVars[ vmc->c ]->xObject.defClass;
			klass->glbData->items.pFloat[ vmc->b ]->value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OGFD ){
			klass = locVars[ vmc->c ]->xObject.defClass;
			klass->glbData->items.pFloat[ vmc->b ]->value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OGDI ){
			klass = locVars[ vmc->c ]->xObject.defClass;
			klass->glbData->items.pDouble[ vmc->b ]->value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OGDF ){
			klass = locVars[ vmc->c ]->xObject.defClass;
			klass->glbData->items.pDouble[ vmc->b ]->value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OGDD ){
			klass = locVars[ vmc->c ]->xObject.defClass;
			klass->glbData->items.pDouble[ vmc->b ]->value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OVII ){
			object = (DaoObject*) locVars[ vmc->c ];
			if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
			object->objValues[ vmc->b ]->xInteger.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OVIF ){
			object = (DaoObject*) locVars[ vmc->c ];
			if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
			object->objValues[ vmc->b ]->xInteger.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OVID ){
			object = (DaoObject*) locVars[ vmc->c ];
			if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
			object->objValues[ vmc->b ]->xInteger.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OVFI ){
			object = (DaoObject*) locVars[ vmc->c ];
			if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
			object->objValues[ vmc->b ]->xFloat.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OVFF ){
			object = (DaoObject*) locVars[ vmc->c ];
			if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
			object->objValues[ vmc->b ]->xFloat.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OVFD ){
			object = (DaoObject*) locVars[ vmc->c ];
			if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
			object->objValues[ vmc->b ]->xFloat.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OVDI ){
			object = (DaoObject*) locVars[ vmc->c ];
			if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
			object->objValues[ vmc->b ]->xDouble.value = IntegerOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OVDF ){
			object = (DaoObject*) locVars[ vmc->c ];
			if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
			object->objValues[ vmc->b ]->xDouble.value = FloatOperand( vmc->a );
		}OPNEXT()
		OPCASE( SETF_OVDD ){
			object = (DaoObject*) locVars[ vmc->c ];
			if( object == & object->defClass->objType->value->xObject ) goto AccessDefault;
			object->objValues[ vmc->b ]->xDouble.value = DoubleOperand( vmc->a );
		}OPNEXT()
		OPCASE( CHECK_ST ){
			vA = locVars[vmc->a];
			locVars[vmc->c]->xInteger.value = vA && vA->type == locVars[vmc->b]->xType.tid;
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
			DaoProcess_RaiseException( self, DAO_ERROR, "operate on none object" );
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
	if( self->topFrame == self->firstFrame && self->topFrame->next && self->topFrame->next->routine ){
		print = (vmSpace->options & DAO_EXEC_INTERUN) && (here->options & DAO_NS_AUTO_GLOBAL);
		if( (print || vmSpace->evalCmdline) && self->stackValues[0] ){
			DaoProcess_PushRoutine( self, self->topFrame->next->routine, NULL );
			DaoProcess_SetActiveFrame( self, self->topFrame );
			DaoStream_WriteMBS( vmSpace->stdioStream, "= " );
			DaoValue_Print( self->stackValues[0], self, vmSpace->stdioStream, NULL );
			DaoStream_WriteNewLine( vmSpace->stdioStream );
			DaoProcess_PopFrame( self );
		}
	}
	return 1;
}
DaoVmCode* DaoProcess_DoSwitch( DaoProcess *self, DaoVmCode *vmc )
{
	DaoVmCode *mid;
	DaoValue **cst = self->activeRoutine->routConsts->items.items.pValue;
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
int DaoProcess_Move( DaoProcess *self, DaoValue *A, DaoValue **C, DaoType *t )
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
	if( !(refer->xNone.trait & DAO_DATA_CONST) ){
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
	}
	if( DaoValue_Move( refer, value, tp ) == 0 ) goto TypeNotMatching;
	return 0;
TypeNotMatching:
	tp2 = DaoNamespace_GetType( self->activeNamespace, refer );
	DaoProcess_RaiseTypeError( self, tp2, tp, "referencing" );
	return 0;
}
dint* DaoProcess_PutInteger( DaoProcess *self, dint value )
{
	DaoInteger tmp = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *res = DaoProcess_SetValue( self, self->activeCode->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	res->xInteger.value = value;
	return & res->xInteger.value;
}
float* DaoProcess_PutFloat( DaoProcess *self, float value )
{
	DaoFloat tmp = {DAO_FLOAT,0,0,0,0,0.0};
	DaoValue *res = DaoProcess_SetValue( self, self->activeCode->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	res->xFloat.value = value;
	return & res->xFloat.value;
}
double* DaoProcess_PutDouble( DaoProcess *self, double value )
{
	DaoDouble tmp = {DAO_DOUBLE,0,0,0,0,0.0};
	DaoValue *res = DaoProcess_SetValue( self, self->activeCode->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	res->xDouble.value = value;
	return & res->xDouble.value;
}
complex16* DaoProcess_PutComplex( DaoProcess *self, complex16 value )
{
	DaoComplex tmp = {DAO_COMPLEX,0,0,0,0,{0.0,0.0}};
	DaoValue *res;
	tmp.value = value;
	res = DaoProcess_SetValue( self, self->activeCode->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	return & res->xComplex.value;
}
DString* DaoProcess_PutMBString( DaoProcess *self, const char *mbs )
{
	DString str = DString_WrapMBS( mbs );
	DaoString tmp = {DAO_STRING,0,0,0,0,NULL};
	DaoValue *res;
	tmp.data = & str;
	res = DaoProcess_SetValue( self, self->activeCode->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	return res->xString.data;
}
DString* DaoProcess_PutWCString( DaoProcess *self, const wchar_t *wcs )
{
	DString str = DString_WrapWCS( wcs );
	DaoString tmp = {DAO_STRING,0,0,0,0,NULL};
	DaoValue *res;
	tmp.data = & str;
	res = DaoProcess_SetValue( self, self->activeCode->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	return res->xString.data;
}
DString*   DaoProcess_PutString( DaoProcess *self, DString *str )
{
	DaoString tmp = {DAO_STRING,0,0,0,0,NULL};
	DaoValue *res;
	tmp.data = str;
	res = DaoProcess_SetValue( self, self->activeCode->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	return res->xString.data;
}
DString* DaoProcess_PutBytes( DaoProcess *self, const char *bytes, size_t N )
{
	DString str = DString_WrapBytes( bytes, N );
	DaoString tmp = {DAO_STRING,0,0,0,0,NULL};
	DaoValue *res;
	tmp.data = & str;
	res = DaoProcess_SetValue( self, self->activeCode->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	return res->xString.data;
}
#ifdef DAO_WITH_NUMARRAY
DaoArray* DaoProcess_PutArrayInteger( DaoProcess *self, dint *array, size_t N )
{
	DaoArray *res = DaoProcess_GetArray( self, self->activeCode );
	res->etype = DAO_INTEGER;
	if( N ){
		DaoArray_ResizeVector( res, N );
		if( array ) memcpy( res->data.i, array, N*sizeof(dint) );
	}else{
		DaoArray_UseData( res, array );
	}
	return res;
}
DaoArray* DaoProcess_PutArrayFloat( DaoProcess *self, float *array, size_t N )
{
	DaoArray *res = DaoProcess_GetArray( self, self->activeCode );
	res->etype = DAO_FLOAT;
	if( N ){
		DaoArray_ResizeVector( res, N );
		if( array ) memcpy( res->data.f, array, N*sizeof(float) );
	}else{
		DaoArray_UseData( res, array );
	}
	return res;
}
DaoArray* DaoProcess_PutArrayDouble( DaoProcess *self, double *array, size_t N )
{
	DaoArray *res = DaoProcess_GetArray( self, self->activeCode );
	res->etype = DAO_DOUBLE;
	if( N ){
		DaoArray_ResizeVector( res, N );
		if( array ) memcpy( res->data.d, array, N*sizeof(double) );
	}else{
		DaoArray_UseData( res, array );
	}
	return res;
}
DaoArray* DaoProcess_PutArrayComplex( DaoProcess *self, complex16 *array, size_t N )
{
	DaoArray *res = DaoProcess_GetArray( self, self->activeCode );
	res->etype = DAO_COMPLEX;
	DaoArray_ResizeVector( res, N );
	if( N >0 && array ) memcpy( res->data.c, array, N*sizeof(complex16) );
	return res;
}
#else
static DaoArray* NullArray( DaoProcess *self )
{
	DaoProcess_RaiseException( self, DAO_ERROR, getCtInfo( DAO_DISABLED_NUMARRAY ) );
	return NULL;
}
DaoArray* DaoProcess_PutArrayInteger( DaoProcess *s, dint *v, size_t n ){ return NullArray( s ); }
DaoArray* DaoProcess_PutArrayFloat( DaoProcess *s, float *v, size_t n ){ return NullArray( s ); }
DaoArray* DaoProcess_PutArrayDouble( DaoProcess *s, double *v, size_t n ){ return NullArray( s ); }
DaoArray* DaoProcess_PutArrayComplex( DaoProcess *s, complex16 *v, size_t n ){ return NullArray( s ); }
#endif
DaoList* DaoProcess_PutList( DaoProcess *self )
{
	return DaoProcess_GetList( self, self->activeCode );
}
DaoMap* DaoProcess_PutMap( DaoProcess *self )
{
	return DaoProcess_GetMap( self, self->activeCode );
}
DaoArray* DaoProcess_PutArray( DaoProcess *self )
{
	return DaoProcess_GetArray( self, self->activeCode );
}
DaoStream* DaoProcess_PutFile( DaoProcess *self, FILE *file )
{
	DaoStream *stream = DaoStream_New();
	DaoStream_SetFile( stream, file );
	if( DaoProcess_SetValue( self, self->activeCode->c, (DaoValue*) stream ) ) return stream;
	DaoStream_Delete( stream );
	return NULL;
}
void DaoCdata_Delete( DaoCdata *self );
DaoCdata* DaoProcess_PutCdata( DaoProcess *self, void *data, DaoType *type )
{
	DaoCdata *cdata = DaoCdata_New( type, data );
	if( DaoProcess_SetValue( self, self->activeCode->c, (DaoValue*)cdata ) ) return cdata;
	DaoCdata_Delete( cdata );
	return NULL;
}
DaoCdata* DaoProcess_WrapCdata( DaoProcess *self, void *data, DaoType *type )
{
	DaoCdata *cdata = DaoCdata_Wrap( type, data );
	if( DaoProcess_SetValue( self, self->activeCode->c, (DaoValue*)cdata ) ) return cdata;
	DaoCdata_Delete( cdata );
	return NULL;
}
DaoCdata*  DaoProcess_CopyCdata( DaoProcess *self, void *d, int n, DaoType *t )
{
	DaoCdata *cdt;
	void *d2 = dao_malloc( n );
	memcpy( d2, d, n );
	cdt = DaoProcess_PutCdata( self, d2, t );
	return cdt;
}
DLong* DaoProcess_GetLong( DaoProcess *self, DaoVmCode *vmc )
{
	DaoType *tp = self->activeTypes[ vmc->c ];
	DaoValue *dC = self->activeValues[ vmc->c ];
	if( dC && dC->type == DAO_LONG ) return dC->xLong.value;
	if( tp && tp->tid !=DAO_LONG && tp->tid !=DAO_UDF && tp->tid !=DAO_ANY ) return NULL;
	dC = (DaoValue*) DaoLong_New();
	GC_ShiftRC( dC, self->activeValues[ vmc->c ] );
	self->activeValues[ vmc->c ] = dC;
	return dC->xLong.value;
}
DLong* DaoProcess_PutLong( DaoProcess *self )
{
	return DaoProcess_GetLong( self, self->activeCode );
}
DaoEnum* DaoProcess_GetEnum( DaoProcess *self, DaoVmCode *vmc )
{
	DaoType *tp = self->activeTypes[ vmc->c ];
	DaoValue *dC = self->activeValues[ vmc->c ];
	if( dC && dC->type == DAO_ENUM ){
		if( tp != dC->xEnum.etype ) DaoEnum_SetType( & dC->xEnum, tp );
		return & dC->xEnum;
	}
	if( tp && tp->tid !=DAO_ENUM && tp->tid !=DAO_UDF && tp->tid !=DAO_ANY ) return NULL;
	dC = (DaoValue*) DaoEnum_New( tp, 0 );
	GC_ShiftRC( dC, self->activeValues[ vmc->c ] );
	self->activeValues[ vmc->c ] = dC;
	return & dC->xEnum;
}
DaoEnum* DaoProcess_PutEnum( DaoProcess *self, const char *symbols )
{
	DaoEnum *denum = DaoProcess_GetEnum( self, self->activeCode );
	DaoEnum_SetSymbols( denum, symbols );
	return denum;
}
/**/
DaoList* DaoProcess_GetList( DaoProcess *self, DaoVmCode *vmc )
{
	/* create a new list in any case. */
	DaoList *list = DaoList_New();
	DaoType *tp = self->activeTypes[ vmc->c ];
	if( tp == NULL || tp->tid != DAO_LIST ) tp = dao_list_any;
	list->unitype = tp;
	GC_IncRC( tp );
	DaoValue_Move( (DaoValue*) list, self->activeValues + vmc->c, tp );
	return list;
}
DaoMap* DaoProcess_GetMap( DaoProcess *self,  DaoVmCode *vmc )
{
	DaoMap *map = DaoMap_New( vmc->code == DVM_HASH );
	DaoType *tp = self->activeTypes[ vmc->c ];
	DaoValue_Move( (DaoValue*) map, self->activeValues + vmc->c, tp );
	if( tp == NULL || tp->tid != DAO_MAP ) tp = dao_map_any;
	GC_ShiftRC( tp, map->unitype );
	map->unitype = tp;
	return map;
}

DaoArray* DaoProcess_GetArray( DaoProcess *self, DaoVmCode *vmc )
{
#ifdef DAO_WITH_NUMARRAY
	DaoType *tp = self->activeTypes[ vmc->c ];
	DaoValue *dC = self->activeValues[ vmc->c ];
	int type = DAO_FLOAT;
	if( tp && tp->tid == DAO_ARRAY && tp->nested->size ){
		type = tp->nested->items.pType[0]->tid;
		if( type == 0 || type > DAO_COMPLEX ) type = DAO_FLOAT;
	}
	if( dC && dC->type == DAO_ARRAY && dC->xArray.refCount == 1 ){
		if( dC->xArray.etype < type ) DaoArray_ResizeVector( & dC->xArray, 0 );
		GC_DecRC( dC->xArray.original );
		dC->xArray.original = NULL;
		dC->xArray.etype = type;
	}else{
		dC = (DaoValue*) DaoArray_New( type );
		DaoValue_Copy( dC, & self->activeValues[ vmc->c ] );
	}
	if( tp == NULL || tp->tid != DAO_ARRAY ) tp = dao_array_any;
	dC->xArray.unitype = tp;
	GC_IncRC( tp );
	return & dC->xArray;
#else
	self->activeCode = vmc;
	DaoProcess_RaiseException( self, DAO_ERROR, getCtInfo( DAO_DISABLED_NUMARRAY ) );
	return NULL;
#endif
}
static DaoTuple* DaoProcess_GetTuple( DaoProcess *self, DaoType *type, int size, int init )
{
	DaoValue *val = self->activeValues[ self->activeCode->c ];
	DaoTuple *tup = val && val->type == DAO_TUPLE ? & val->xTuple : NULL;
	
	if( tup && tup->unitype == type ){
		DaoVmCode *vmc = self->activeCode + 1;
		if( tup->refCount == 1 ) return tup;
		if( tup->refCount == 2 && (vmc->code == DVM_MOVE || vmc->code == DVM_MOVE_PP) ){
			if( self->activeValues[vmc->c] == (DaoValue*) tup ) return tup;
		}
	}
	if( type ){
		tup = DaoTuple_Create( type, init );
	}else{
		tup = DaoTuple_New( size );
	}
	GC_ShiftRC( tup, val );
	self->activeValues[ self->activeCode->c ] = (DaoValue*) tup;
	return tup;
}
DaoTuple* DaoProcess_PutTuple( DaoProcess *self )
{
	DaoType *type = self->activeTypes[ self->activeCode->c ];
	if( type && type->tid == DAO_VARIANT ) type = DaoType_GetVariantItem( type, DAO_TUPLE );
	if( type == NULL || type->tid != DAO_TUPLE ) return NULL;
	return DaoProcess_GetTuple( self, type, type->nested->size, 1 );
}
DaoType* DaoProcess_GetReturnType( DaoProcess *self )
{
	DaoStackFrame *frame = self->topFrame;
	DaoType *type = self->activeTypes[ self->activeCode->c ]; /* could be specialized; */
	if( frame->retype ) return self->topFrame->retype;
	if( type == NULL || (type->attrib & DAO_TYPE_NOTDEF) ){
		if( frame->routine ) type = (DaoType*) frame->routine->routType->aux;
	}
	if( type == NULL ) type = self->activeTypes[ self->activeCode->c ];
	GC_ShiftRC( type, self->topFrame->retype );
	self->topFrame->retype = type;
	return type;
}

void DaoProcess_MakeTuple( DaoProcess *self, DaoTuple *tuple, DaoValue *its[], int N )
{
	DaoType *tp, *ct = tuple->unitype;
	DaoValue *val;
	DNode *node;
	int i;
	if( ct == NULL ) return;
	if( ct->nested ==NULL || ct->nested->size != N ){
		DaoProcess_RaiseException( self, DAO_ERROR, "invalid tuple enumeration" );
		return;
	}
	for(i=0; i<N; i++){
		val = its[i];
		if( val->type == DAO_PAR_NAMED ){
			DaoNameValue *nameva = & val->xNameValue;
			node = MAP_Find( ct->mapNames, nameva->name );
			if( node == NULL || node->value.pInt != i ){
				DaoProcess_RaiseException( self, DAO_ERROR, "name not matched" );
				return;
			}
			val = nameva->value;
		}
		tp = ct->nested->items.pType[i];
		if( tp->tid == DAO_PAR_NAMED || tp->tid == DAO_PAR_DEFAULT ) tp = & tp->aux->xType;
		if( DaoValue_Move( val, tuple->items + i, tp ) == 0){
			DaoProcess_RaiseException( self, DAO_ERROR, "invalid tuple enumeration" );
			return;
		}
	}
}

void DaoProcess_BindNameValue( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *dA = self->activeRoutine->routConsts->items.items.pValue[ vmc->a ];
	DaoValue *dB = self->activeValues[ vmc->b ];
	DaoNameValue *nameva = DaoNameValue_New( dA->xString.data, dB );
	nameva->unitype = self->activeTypes[ vmc->c ];
	if( nameva->unitype == NULL ){
		DaoNamespace *ns = self->activeNamespace;
		DaoType *tp = DaoNamespace_GetType( ns, nameva->value );
		nameva->unitype = DaoNamespace_MakeType( ns, dA->xString.data->mbs, DAO_PAR_NAMED, (DaoValue*)tp, NULL, 0 );
	}
	GC_IncRC( nameva->unitype );
	DaoProcess_SetValue( self, vmc->c, (DaoValue*) nameva );
}
void DaoProcess_DoPair( DaoProcess *self, DaoVmCode *vmc )
{
	DaoNamespace *ns = self->activeNamespace;
	DaoType *tp = self->activeTypes[ vmc->c ];
	DaoValue *dA = self->activeValues[ vmc->a ];
	DaoValue *dB = self->activeValues[ vmc->b ];
	DaoTuple *tuple = DaoTuple_New(2);
	if( tp == NULL ) tp = DaoNamespace_MakePairValueType( ns, dA, dB );
	tuple->unitype = tp;
	GC_IncRC( tuple->unitype );
	DaoValue_Copy( dA, & tuple->items[0] );
	DaoValue_Copy( dB, & tuple->items[1] );
	DaoProcess_SetValue( self, vmc->c, (DaoValue*) tuple );
}
void DaoProcess_DoTuple( DaoProcess *self, DaoVmCode *vmc )
{
	DaoType *tp, *ct = self->activeTypes[ vmc->c ];
	DaoNamespace *ns = self->activeNamespace;
	DaoTuple *tuple;
	DaoValue *val;
	int i;
	
	self->activeCode = vmc;
	tuple = DaoProcess_GetTuple( self, ct, vmc->b, 0 );
	if( ct == NULL ){
		ct = DaoType_New( "tuple<", DAO_TUPLE, NULL, NULL );
		for(i=0; i<vmc->b; i++){
			val = self->activeValues[ vmc->a + i ];
			tp = DaoNamespace_GetType( ns, val );
			if( tp == NULL ) tp = DaoNamespace_GetType( ns, dao_none_value );
			if( i >0 ) DString_AppendMBS( ct->name, "," );
			if( tp->tid == DAO_PAR_NAMED ){
				DaoNameValue *nameva = & val->xNameValue;
				if( ct->mapNames == NULL ) ct->mapNames = DMap_New(D_STRING,0);
				MAP_Insert( ct->mapNames, nameva->name, i );
				DString_Append( ct->name, nameva->name );
				DString_AppendMBS( ct->name, ":" );
				DString_Append( ct->name, tp->aux->xType.name );
				val = nameva->value;
			}else{
				DString_Append( ct->name, tp->name );
			}
			DArray_Append( ct->nested, tp );
			DaoTuple_SetItem( tuple, val, i );
		}
		DString_AppendMBS( ct->name, ">" );
		GC_IncRCs( ct->nested );
		tp = DaoNamespace_FindType( ns, ct->name );
		if( tp ){
			DaoType_Delete( ct );
			ct = tp;
		}else{
			DaoType_CheckAttributes( ct );
			DaoType_InitDefault( ct );
			DaoNamespace_AddType( ns, ct->name, ct );
		}
		tuple->unitype = ct;
		GC_IncRC( ct );
	}else{
		DaoProcess_MakeTuple( self, tuple, self->activeValues + vmc->a, vmc->b );
	}
}
void DaoProcess_DoCheck( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *dA = self->activeValues[ vmc->a ];
	DaoValue *dB = self->activeValues[ vmc->b ];
	DaoType *type = (DaoType*) dB;
	dint *res = 0;
	self->activeCode = vmc;
	res = DaoProcess_PutInteger( self, 0 );
	if( dA->type && dB->type == DAO_TYPE ){
		if( dA->type == DAO_OBJECT ) dA = (DaoValue*) dA->xObject.rootObject;
		if( type->tid == DAO_VARIANT ){
			int i, mt = 0, id = 0, max = 0;
			for(i=0; i<type->nested->size; i++){
				if( dA->type == DAO_TYPE ){
					mt = DaoType_MatchTo( & dA->xType, type->nested->items.pType[i], NULL );
				}else{
					mt = DaoType_MatchValue( type->nested->items.pType[i], dA, NULL );
				}
				if( mt > max ){
					max = mt;
					id = i + 1;
				}
				if( max == DAO_MT_EQ ) break;
			}
			*res = id;
			return;
		}
		if( dA->type < DAO_ARRAY ){
			*res = dA->type == type->tid;
		}else{
			*res = DaoType_MatchValue( type, dA, NULL ) != 0;
		}
	}else if( dA->type == dB->type ){
		*res = 1;
		if( dA->type == DAO_OBJECT ){
			*res = dA->xObject.rootObject->defClass == dB->xObject.rootObject->defClass;
		}else if( dA->type == DAO_CDATA ){
			*res = dA->xCdata.ctype == dB->xCdata.ctype;
		}else if( dA->type >= DAO_ARRAY && dA->type <= DAO_TUPLE ){
			DaoType *t1 = NULL;
			DaoType *t2 = NULL;
			*res = 0;
			switch( dA->type ){
				case DAO_ARRAY : t1 = dA->xArray.unitype; t2 = dB->xArray.unitype; break;
				case DAO_LIST : t1 = dA->xList.unitype; t2 = dB->xList.unitype; break;
				case DAO_MAP  : t1 = dA->xMap.unitype;  t2 = dB->xMap.unitype; break;
				case DAO_TUPLE : t1 = dA->xTuple.unitype; t2 = dB->xTuple.unitype; break;
				default : break;
			}
			*res = DaoType_MatchTo( t1, t2, NULL ) == DAO_MT_EQ;
		}
	}
}
void DaoProcess_DoGetItem( DaoProcess *self, DaoVmCode *vmc )
{
	int id;
	DaoValue *B = dao_none_value;
	DaoValue *A = self->activeValues[ vmc->a ];
	DaoTypeCore *tc = DaoValue_GetTyper( A )->core;
	DaoType *ct = self->activeTypes[ vmc->c ];
	
	self->activeCode = vmc;
	if( A == NULL || A->type == 0 ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "on none object" );
		return;
	}
	if( vmc->code == DVM_GETI ) B = self->activeValues[ vmc->b ];
	if( A->type == DAO_LIST && (B->type >= DAO_INTEGER && B->type <= DAO_DOUBLE ) ){
		DaoList *list = & A->xList;
		id = DaoValue_GetInteger( B );
		if( id < 0 ) id += list->items.size;
		if( id >=0 && id < list->items.size ){
			GC_ShiftRC( list->items.items.pValue[id], self->activeValues[ vmc->c ] );
			self->activeValues[ vmc->c ] = list->items.items.pValue[id];
		}else{
			DaoProcess_RaiseException( self, DAO_ERROR, "index out of range" );
			return;
		}
#ifdef DAO_WITH_NUMARRAY
	}else if( A->type == DAO_ARRAY && (B->type >=DAO_INTEGER && B->type <=DAO_DOUBLE )){
		DaoDouble tmp = {0,0,0,0,0,0.0};
		DaoValue *C = (DaoValue*) & tmp;
		DaoArray *na = & A->xArray;
		id = DaoValue_GetInteger( B );
		if( na->original && DaoArray_Sliced( na ) == 0 ){
			DaoProcess_RaiseException( self, DAO_ERROR_INDEX, "slicing" );
			return;
		}
		if( id < 0 ) id += na->size;
		if( id < 0 || id >= na->size ){
			DaoProcess_RaiseException( self, DAO_ERROR_INDEX_OUTOFRANGE, "" );
			return;
		}
		C->type = na->etype;
		switch( na->etype ){
			case DAO_INTEGER : C->xInteger.value = na->data.i[id]; break;
			case DAO_FLOAT   : C->xFloat.value = na->data.f[id];  break;
			case DAO_DOUBLE  : C->xDouble.value = na->data.d[id];  break;
			case DAO_COMPLEX : C->xComplex.value = na->data.c[id]; break;
			default : break;
		}
		DaoProcess_Move( self, C, & self->activeValues[ vmc->c ], ct );
#endif
	}else if( vmc->code == DVM_GETI ){
		tc->GetItem( A, self, self->activeValues + vmc->b, 1 );
	}else if( vmc->code == DVM_GETDI ){
		DaoInteger iv = {DAO_INTEGER,0,0,0,1,0};
		DaoValue *piv = (DaoValue*) (DaoInteger*) & iv;
		iv.value = vmc->b;
		tc->GetItem( A, self, & piv, 1 );
	}else if( vmc->code == DVM_GETMI ){
		tc->GetItem( A, self, self->activeValues + vmc->a + 1, vmc->b );
	}
}
void DaoProcess_DoGetField( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *C, *A = self->activeValues[ vmc->a ];
	DaoTypeCore *tc = DaoValue_GetTyper( A )->core;
	DaoNamespace *ns = self->activeNamespace;
	DString *name = self->activeRoutine->routConsts->items.items.pValue[ vmc->b ]->xString.data;
	DArray *elist = self->exceptions;
	int E = elist->size;
	
	self->activeCode = vmc;
	if( A == NULL || A->type == 0 ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "on none object" );
		return;
	}
	tc->GetField( A, self, name );
	if( elist->size != (E + 1) ) return;
	if( elist->items.pCdata[E]->ctype != DaoException_GetType( DAO_ERROR_FIELD_NOTEXIST ) ) return;
	C = DaoValue_FindAuxMethod( A, name, ns );
	if( C == NULL ) return;
	DArray_PopBack( elist );
	DaoProcess_PutValue( self, C );
}

DHash *dao_meta_tables = NULL; /* hash<DaoValue*,DaoMap*> */

DaoMap* DaoMetaTables_Get( DaoValue *object, int insert )
{
	DaoMap *table = NULL;
	DNode *node = NULL;
	GC_Lock();
	if( object->xNone.trait & DAO_DATA_WIMETA ) node = DMap_Find( dao_meta_tables, object );
	if( node ){
		table = (DaoMap*) node->value.pValue;
	}else if( insert ){
		table = DaoMap_New(1);
		object->xNone.trait |= DAO_DATA_WIMETA;
		GC_IncRC( table );
		DMap_Insert( dao_meta_tables, object, table );
	}
	GC_Unlock();
	return table;
}
static void DaoMetaTables_Set( DaoValue *object, DaoMap *table )
{
	DNode *node;
	GC_Lock();
	node = DMap_Find( dao_meta_tables, object );
	GC_IncRC( table );
	if( node ) GC_DecRC( node->value.pValue );
	object->xNone.trait |= DAO_DATA_WIMETA;
	DMap_Insert( dao_meta_tables, object, table );
	GC_Unlock();
}
DaoMap* DaoMetaTables_Remove( DaoValue *object )
{
	DaoMap *table = NULL;
	DNode *node;
	GC_Lock();
	node = DMap_Find( dao_meta_tables, object );
	object->xNone.trait &= ~DAO_DATA_WIMETA;
	if( node ){
		table = (DaoMap*) node->value.pValue;
		DMap_EraseNode( dao_meta_tables, node );
	}
	GC_Unlock();
	return table;
}


static DaoValue* DaoMap_GetMetaField( DaoMap *self, DaoValue *key )
{
	DNode *node = DMap_Find( self->items, key );
	if( node ) return node->value.pValue;
	self = DaoMetaTables_Get( (DaoValue*)self, 0 );
	if( self ) return DaoMap_GetMetaField( self, key );
	return NULL;
}
void DaoProcess_DoGetMetaField( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *value;
	DaoValue *A = self->activeValues[ vmc->a ];
	DaoMap *meta = A->type == DAO_MAP ? & A->xMap : DaoMetaTables_Get( A, 0 );
	
	self->activeCode = vmc;
	if( meta == NULL ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "object has no meta fields" );
		return;
	}
	value = DaoMap_GetMetaField( meta, self->activeRoutine->routConsts->items.items.pValue[ vmc->b] );
	if( value == NULL ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "meta field not exists" );
		return;
	}
	self->activeCode = vmc;
	DaoProcess_PutValue( self, value );
}
void DaoProcess_DoSetItem( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *A, *B = dao_none_value, *C = self->activeValues[ vmc->c ];
	DaoTypeCore *tc = DaoValue_GetTyper( C )->core;
	int id, rc = 0;
	
	self->activeCode = vmc;
	A = self->activeValues[ vmc->a ];
	if( C == NULL || C->type == 0 ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "on none object" );
		return;
	}
	
	if( vmc->code == DVM_SETI ) B = self->activeValues[ vmc->b ];
	if( C->type == DAO_LIST && B->type == DAO_INTEGER ){
		rc = DaoList_SetItem( & C->xList, A, B->xInteger.value );
	}else if( C->type == DAO_LIST && B->type == DAO_FLOAT ){
		rc = DaoList_SetItem( & C->xList, A, (int) B->xFloat.value );
	}else if( C->type == DAO_LIST && B->type == DAO_DOUBLE ){
		rc = DaoList_SetItem( & C->xList, A, (int) B->xDouble.value );
#ifdef DAO_WITH_NUMARRAY
	}else if( C->type == DAO_ARRAY && (B->type >=DAO_INTEGER && B->type <=DAO_DOUBLE)
			 && (A->type >=DAO_INTEGER && A->type <=DAO_DOUBLE) ){
		DaoArray *na = & C->xArray;
		double val = DaoValue_GetDouble( A );
		complex16 cpx = DaoValue_GetComplex( A );
		id = DaoValue_GetDouble( B );
		if( na->original && DaoArray_Sliced( na ) == 0 ){
			DaoProcess_RaiseException( self, DAO_ERROR_INDEX, "slicing" );
			return;
		}
		if( id < 0 ) id += na->size;
		if( id < 0 || id >= na->size ){
			DaoProcess_RaiseException( self, DAO_ERROR_INDEX_OUTOFRANGE, "" );
			return;
		}
		switch( na->etype ){
			case DAO_INTEGER : na->data.i[ id ] = (dint) val; break;
			case DAO_FLOAT  : na->data.f[ id ] = (float) val; break;
			case DAO_DOUBLE : na->data.d[ id ] = val; break;
			case DAO_COMPLEX : na->data.c[ id ] = cpx; break;
			default : break;
		}
#endif
	}else if( vmc->code == DVM_SETI ){
		tc->SetItem( C, self, self->activeValues + vmc->b, 1, A );
	}else if( vmc->code == DVM_GETDI ){
		DaoInteger iv = {DAO_INTEGER,0,0,0,1,0};
		DaoValue *piv = (DaoValue*) (DaoInteger*) & iv;
		iv.value = vmc->b;
		tc->SetItem( C, self, & piv, 1, A );
	}else if( vmc->code == DVM_SETMI ){
		tc->SetItem( C, self, self->activeValues + vmc->c + 1, vmc->b, A );
	}
	if( rc ) DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "value type" );
}
void DaoProcess_DoSetField( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *A, *C = self->activeValues[ vmc->c ];
	DaoValue *fname = self->activeRoutine->routConsts->items.items.pValue[ vmc->b ];
	DaoTypeCore *tc = DaoValue_GetTyper( C )->core;
	
	self->activeCode = vmc;
	A = self->activeValues[ vmc->a ];
	if( C == NULL || C->type == 0 ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "on none object" );
		return;
	}
	tc->SetField( C, self, fname->xString.data, A );
}
void DaoProcess_DoSetMetaField( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *A = self->activeValues[ vmc->a ];
	DaoValue *C = self->activeValues[ vmc->c ];
	DaoValue *fname = self->activeRoutine->routConsts->items.items.pValue[ vmc->b ];
	DaoMap *meta = DaoMetaTables_Get( C, 1 );
	int m = 1;
	
	self->activeCode = vmc;
	if( meta == NULL ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "object can not have meta fields" );
		return;
	}
	/* If C is a map, try first to insert A with field name as key into C: */
	if( C->type == DAO_MAP ) m = DaoMap_Insert( & C->xMap, fname, A );
	/* If A is failed to be inserted into C, insert into the meta table: */
	if( m ) DaoMap_Insert( meta, fname, A );
	/* If A itself is a map, and the field name is __proto__,
	 * set the meta table's meta table as A: */
	if( A->type == DAO_MAP && strcmp( fname->xString.data->mbs, "__proto__" ) ==0 ){
		DaoMetaTables_Set( (DaoValue*) meta, (DaoMap*) A );
	}
}

void DaoProcess_DoMove( DaoProcess *self, DaoVmCode *vmc )
{
	DaoType *ct = self->activeTypes[ vmc->c ];
	DaoValue *A = self->activeValues[ vmc->a ];
	DaoValue *C = self->activeValues[ vmc->c ];
	int overload = 0;
	if( A == NULL ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "on none object" );
		return;
	}
	if( C ){
		if( A->type == C->type && C->type == DAO_OBJECT ){
			overload = DaoClass_ChildOf( A->xObject.defClass, (DaoValue*)C->xObject.defClass ) == 0;
		}else if( A->type == C->type && C->type == DAO_CDATA ){
			overload = DaoType_ChildOf( A->xCdata.ctype, C->xCdata.ctype ) == 0;
		}else if( C->type == DAO_OBJECT || C->type == DAO_CDATA ){
			overload = 1;
		}
		if( overload ){
			DaoRoutine *rout = NULL;
			if( C->type == DAO_OBJECT ){
				DaoClass *scope = self->activeObject ? self->activeObject->defClass : NULL;
				rout = DaoClass_FindOperator( C->xObject.defClass, "=", scope );
			}else{
				rout = DaoType_FindFunctionMBS( C->xCdata.ctype, "=" );
			}
			if( rout && DaoProcess_PushCallable( self, rout, C, & A, 1 ) == 0 ) return;
		}
	}
	DaoProcess_Move( self, A, & self->activeValues[vmc->c], ct );
}
void DaoProcess_DoReturn( DaoProcess *self, DaoVmCode *vmc )
{
	DaoStackFrame *topFrame = self->topFrame;
	DaoType *type = self->abtype ? (DaoType*)self->abtype->aux : NULL;
	DaoValue **src = self->activeValues + vmc->a;
	DaoValue **dest = self->stackValues;
	DaoValue *retValue = NULL;
	int i, returning = topFrame->returning;

	self->activeCode = vmc;
	//XXX if( DaoProcess_CheckFE( self ) ) return;

	if( vmc->code == DVM_RETURN &&  returning != (ushort_t)-1 ){
		DaoStackFrame *lastframe = topFrame->prev;
		assert( lastframe && lastframe->routine );
		type = lastframe->routine->body->regType->items.pType[ returning ];
		dest = self->stackValues + lastframe->stackBase + returning;
	}
	if( topFrame->state & DVM_MAKE_OBJECT ){
		retValue = (DaoValue*)self->activeObject;
	}else if( vmc->b == 1 ){
		retValue = self->activeValues[ vmc->a ];
	}else if( vmc->b > 1 && dest != self->stackValues ){
		int ecount = self->exceptions->size;
		DaoTuple *tuple;
		DaoProcess_SetActiveFrame( self, topFrame->prev->active );
		tuple = DaoProcess_PutTuple( self );
		DaoProcess_SetActiveFrame( self, topFrame->active );
		if( tuple == NULL || tuple->size > vmc->b ) goto InvalidReturn;
		for(i=0; i<tuple->size; i++) DaoValue_Copy( src[i], tuple->items + i );
		GC_DecRC( tuple->unitype );
		tuple->unitype = NULL;
		if( self->exceptions->size > ecount ) goto InvalidReturn;
		return;
	}else if( vmc->b > 1 ){
		DaoTuple *tuple = DaoTuple_New( vmc->b );
		retValue = (DaoValue*) tuple;
		for(i=0; i<vmc->b; i++) DaoValue_Copy( src[i], tuple->items + i );
	}else{
		retValue = dao_none_value;
		type = NULL;
	}
	if( retValue == NULL ){
		int opt1 = self->vmSpace->options & DAO_EXEC_INTERUN;
		int opt2 = self->activeNamespace->options & DAO_NS_AUTO_GLOBAL;
		int retnull = type == NULL || type->tid == DAO_UDF;
		if( retnull || self->vmSpace->evalCmdline || (opt1 && opt2) ) retValue = dao_none_value;
	}
	if( DaoValue_Move( retValue, dest, type ) ==0 ) goto InvalidReturn;
	return;
InvalidReturn:
	/* printf( "retValue = %p %i %p %s\n", retValue, retValue->type, type, type->name->mbs ); */
	DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "invalid returned value" );
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
int ConvertStringToNumber( DaoProcess *proc, DaoValue *dA, DaoValue *dC );
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
	DaoValue value;
	DaoRoutine *meth;
	DNode *node;
	int i, mt, mt2;

	if( va == NULL ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "operate on none object" );
		return;
	}
	if( ct == NULL || ct->tid == DAO_UDF || ct->tid == DAO_ANY ) goto FastCasting;
	if( va->type == ct->tid && ct->tid <= DAO_STRING ) goto FastCasting;

	if( vc && vc->type == ct->tid && va->type <= DAO_STRING ){
		if( va->type == ct->tid ) goto FastCasting;
		if( va->type == DAO_STRING ){
			if( vc->type == DAO_LONG ){
				if( buffer1.lng == NULL ) buffer1.lng = DLong_New();
				vc->xLong.value = buffer1.lng;
			}
			if( ConvertStringToNumber( self, va, vc ) == 0 ) goto FailConversion;
			return;
		}
		switch( ct->tid ){
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
		meth = DaoType_FindFunctionMBS( va->xCdata.ctype, "cast" );
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
FastCasting:
	GC_ShiftRC( va, vc );
	*vc2 = va;
	return;
FailConversion :
	at = DaoNamespace_GetType( self->activeNamespace, self->activeValues[ vmc->a ] );
	DaoProcess_RaiseTypeError( self, at, ct, "casting" );
	CastBuffer_Clear( & buffer1 );
	CastBuffer_Clear( & buffer2 );
}
static int DaoProcess_TryAsynCall( DaoProcess *self, DaoVmCode *vmc )
{
#ifdef DAO_WITH_CONCURRENT
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
	int i, M = DaoRoutine_PassParams( & rout, self->freeValues, NULL, selfpar, P, N, vmc->code );
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
		DaoType *hostype, DaoRoutine *func, DaoValue *selfpar, DaoValue *P[], int N )
{
	DaoRoutine *rout = func;
	DaoValue *caller = self->activeValues[ vmc->a ];
	int status, code = vmc->code;
	if( (self->vmSpace->options & DAO_EXEC_SAFE) && (func->attribs & DAO_ROUT_EXTFUNC) ){
		/* normally this condition will not be satisfied.
		 * it is possible only if the safe mode is set in C codes
		 * by embedding or extending. */
		DaoProcess_RaiseException( self, DAO_ERROR, "not permitted" );
		return;
	}
	func = DaoRoutine_ResolveX( func, selfpar, P, N, code );
	if( func == NULL ){
		DaoProcess_ShowCallError( self, rout, selfpar, P, N, code );
		return;
	}
	if( DaoRoutine_PassParams( & func, self->freeValues, hostype, selfpar, P, N, code ) ==0 ){
		//rout2 = (DaoRoutine*) rout;
		DaoProcess_ShowCallError( self, func, selfpar, P, N, code );
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
#if 0
	if( caller->type == DAO_CTYPE ){
		DaoType *retype = caller->xCtype.cdtype;
		printf( ">>>>>>>>>>>>> %s %s\n", retype->name->mbs, caller->xCdata.ctype->name->mbs );
		GC_ShiftRC( retype, self->topFrame->retype );
		self->topFrame->retype = retype;
	}
#endif
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
	DaoRoutine *rout;
	DaoRoutine *routines = klass->classRoutines;
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
	rout = DaoRoutine_ResolveX( routines, selfpar, params, npar, codemode );
	if( rout == NULL ){
		selfpar = (DaoValue*) othis;
		rout = DaoRoutine_ResolveX( routines, selfpar, params, npar, codemode );
	}
	if( rout == NULL && (npar ==0 || (npar == 1 && code == DVM_MCALL) ) ){
		/* default contstructor */
		rout = klass->classRoutine;
	}
	if( rout == NULL ){
		//rout2 = (DRoutine*) klass->classRoutine;
		//XXX goto InvalidParameter;
		return;
	}
	if( rout->pFunc ){
		npar = DaoRoutine_PassParams( & rout, self->freeValues, klass->objType, selfpar, params, npar, vmc->code );
		if( npar == 0 ){
			//rout2 = (DRoutine*) rout;
			//XXX goto InvalidParameter;
			return;
		}
		DaoProcess_PushFunction( self, rout );
		DaoProcess_SetActiveFrame( self, self->firstFrame ); /* return value in stackValues[0] */
		self->topFrame->active = self->firstFrame;
		DaoProcess_CallFunction( self, rout, self->stackValues + self->topFrame->stackBase, npar );
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
	DaoRoutine *rout = NULL;
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
	if( caller->xRoutine.overloads ){
		rout = DaoRoutine_ResolveX( (DaoRoutine*)caller, selfpar, params, npar, DVM_CALL );
	}else if( caller->type == DAO_ROUTINE ){
		rout = (DaoRoutine*) caller;
	}
	if( rout == NULL ) goto InvalidParameter;
	if( rout->pFunc ){
		DaoProcess_DoCxxCall( self, vmc, NULL, rout, selfpar, params, npar );
	}else if( rout->type == DAO_ROUTINE ){
		DaoProcess_PrepareCall( self, rout, selfpar, params, npar, vmc );
		if( DaoProcess_TryAsynCall( self, vmc ) ) return;
	}else{
		DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "object not callable" );
	}
	return;
InvalidParameter:
	DaoProcess_ShowCallError( self, (DaoRoutine*)caller, selfpar, params, npar, DVM_CALL );
}
static DaoProcess* DaoProcess_Create( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoProcess *vmProc;
	DaoValue *val = par[0];
	DaoRoutine *rout;
	int i, passed = 0;
	if( val->type == DAO_STRING ) val = DaoNamespace_GetData( proc->activeNamespace, val->xString.data );
	if( val == NULL || val->type != DAO_ROUTINE ){
		DaoProcess_RaiseException( proc, DAO_ERROR_TYPE, NULL );
		return NULL;
	}
	rout = DaoRoutine_ResolveX( (DaoRoutine*)val, NULL, par+1, N-1, DVM_CALL );
	if( rout ) passed = DaoRoutine_PassParams( & rout, proc->freeValues, NULL, NULL, par+1, N-1, DVM_CALL );
	if( passed == 0 || rout == NULL || rout->body == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "not matched" );
		return NULL;
	}
	vmProc = DaoProcess_New( proc->vmSpace );
	DaoProcess_PushRoutine( vmProc, rout, NULL );
	vmProc->activeValues = vmProc->stackValues + vmProc->topFrame->stackBase;
	for(i=0; i<rout->parCount; i++){
		vmProc->activeValues[i] = proc->freeValues[i];
		GC_IncRC( vmProc->activeValues[i] );
	}
	vmProc->status = DAO_VMPROC_SUSPENDED;
	return vmProc;
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
	DaoStackFrame *topFrame = self->topFrame;
	DaoRoutine *rout, *rout2 = NULL;

	self->activeCode = vmc;
	if( caller->type ==0 ){
		DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "none object not callable" );
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
	}else if( caller->type == DAO_ROUTINE ){
		if( caller->xRoutine.pFunc ){
			DaoProcess_DoCxxCall( self, vmc, NULL, & caller->xRoutine, selfpar, params, npar );
			return;
		}
		rout = DaoRoutine_ResolveX( (DaoRoutine*) caller, selfpar, params, npar, codemode );
		if( rout == NULL ){
			rout2 = (DaoRoutine*) caller;
			goto InvalidParameter;
		}
		if( rout->pFunc ){
			DaoProcess_DoCxxCall( self, vmc, NULL, rout, selfpar, params, npar );
		}else{
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
		}
	}else if( caller->type == DAO_CLASS ){
		DaoProcess_DoNewCall( self, vmc, & caller->xClass, selfpar, params, npar );
		if( self->topFrame != topFrame ){
			GC_ShiftRC( caller->xClass.objType, self->topFrame->retype );
			self->topFrame->retype = caller->xClass.objType;
		}
	}else if( caller->type == DAO_OBJECT ){
		DaoClass *host = self->activeObject ? self->activeObject->defClass : NULL;
		rout = DaoClass_FindOperator( caller->xObject.defClass, "()", host );
		if( rout == NULL ){
			DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "class instance not callable" );
			return;
		}
		rout = DaoRoutine_ResolveX( rout, caller, params, npar, codemode );
		if( rout == NULL ){
			return; //XXX
		}
		if( rout->pFunc ){
			DaoProcess_DoCxxCall( self, vmc, NULL, rout, caller, params, npar );
		}else if( rout->type == DAO_ROUTINE ){
			DaoProcess_PrepareCall( self, rout, selfpar, params, npar, vmc );
			if( DaoProcess_TryAsynCall( self, vmc ) ) return;
		}
	}else if( caller->type == DAO_CTYPE ){
		DaoType *type = caller->xCdata.ctype;
		rout = DaoType_FindFunctionMBS( type, type->typer->name );
		if( rout == NULL ){
			DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "C type not callable" );
			return;
		}
		rout = DaoRoutine_ResolveX( rout, selfpar, params, npar, codemode );
		if( rout == NULL || rout->type != DAO_ROUTINE || rout->pFunc == NULL ){
			// XXX
			return;
		}
		DaoProcess_DoCxxCall( self, vmc, caller->xCdata.ctype, rout, selfpar, params, npar );
		// XXX handle fail
		sup = DaoProcess_InitBase( self, vmc, caller );
		//printf( "sup = %i\n", sup );
		if( caller->type == DAO_CTYPE && sup >= 0 ){
			DaoCdata *cdata = & self->activeValues[ vmc->c ]->xCdata;
			if( cdata && cdata->type == DAO_CDATA ){
				//printf( "%p %p %p\n", cdata, cdata->object, self->activeObject->rootObject );
				GC_ShiftRC( cdata, self->activeObject->parents[sup] );
				self->activeObject->parents[sup] = (DaoValue*) cdata;
				GC_ShiftRC( self->activeObject->rootObject, cdata->object );
				cdata->object = self->activeObject->rootObject;
			}
		}
	}else if( caller->type == DAO_CDATA ){
		rout = DaoType_FindFunctionMBS( caller->xCdata.ctype, "()" );
		if( rout == NULL ){
			DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "C object not callable" );
			return;
		}
		rout = DaoRoutine_ResolveX( rout, selfpar, params, npar, codemode );
		if( rout == NULL || rout->type != DAO_ROUTINE || rout->pFunc == NULL ){
			// XXX
			return;
		}
		DaoProcess_DoCxxCall( self, vmc, NULL, rout, selfpar, params, npar );
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
int DaoObject_InvokeMethod( DaoObject *self, DaoObject *othis, DaoProcess *proc, 
						   DString *name, DaoValue *P[], int N, int ignore_return, int execute );
void DaoProcess_DoIter( DaoProcess *self, DaoVmCode *vmc )
{
	DString *name = self->mbstring;
	DaoValue *va = self->activeValues[ vmc->a ];
	DaoValue *vc = self->activeValues[ vmc->c ];
	DaoType *type = DaoNamespace_GetType( self->activeNamespace, va );
	DaoTuple *iter;
	int rc = 1;
	
	if( va == NULL || va->type == 0 ) return;
	
	if( vc == NULL || vc->type != DAO_TUPLE || vc->xTuple.unitype != dao_type_for_iterator ){
		vc = (DaoValue*) DaoProcess_PutTuple( self );
	}
	
	iter = & vc->xTuple;
	iter->items[0]->xInteger.value = 0;
	DaoTuple_SetItem( iter, dao_none_value, 1 );
	
	DString_SetMBS( name, "__for_iterator__" );
	if( va->type == DAO_OBJECT ){
		rc = DaoObject_InvokeMethod( & va->xObject, NULL, self, name, & vc, 1, 1, 0 );
	}else{
		DaoRoutine *meth = DaoType_FindFunction( type, name );
		if( meth ) rc = DaoProcess_Call( self, meth, va, &vc, 1 );
	}
	if( rc ) DaoProcess_RaiseException( self, DAO_ERROR_FIELD_NOTEXIST, name->mbs );
}


void DaoProcess_DoRange(  DaoProcess *self, DaoVmCode *vmc );
void DaoProcess_DoList(  DaoProcess *self, DaoVmCode *vmc )
{
	DaoNamespace *ns = self->activeNamespace;
	DaoValue **regValues = self->activeValues;
	const ushort_t opA = vmc->a;
	int i;
	
	if( vmc->b == 0 || vmc->b >= 10 ){
		const int bval = vmc->b ? vmc->b - 10 : 0;
		DaoList *list = DaoProcess_GetList( self, vmc );
		DArray_Resize( & list->items, bval, NULL );
		if( bval >0 && self->activeTypes[ vmc->c ] ==NULL ){
			DaoType *abtp = DaoNamespace_GetType( ns, regValues[opA] );
			DaoType *t = DaoNamespace_MakeType( ns, "list", DAO_LIST, NULL, & abtp, 1 );
			GC_ShiftRC( t, list->unitype );
			list->unitype = t;
		}
		for( i=0; i<bval; i++){
			if( DaoList_SetItem( list, regValues[opA+i], i ) ){
				DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "invalid items" );
				return;
			}
		}
	}else{
		DaoProcess_DoRange( self, vmc );
	}
}
static void DaoProcess_DoNumRange( DaoProcess *self, DaoVmCode *vmc );
void DaoProcess_DoArray( DaoProcess *self, DaoVmCode *vmc )
{
#ifdef DAO_WITH_NUMARRAY
	const ushort_t opA = vmc->a;
	const ushort_t count = vmc->b - 10;
	size_t i, j, m, k = 0, ndim = 0;
	size_t *dims = NULL;
	DaoArray *array = NULL;
	
	if( vmc->b < 10 ){
		DaoProcess_DoNumRange( self, vmc );
		return;
	}
	array = DaoProcess_GetArray( self, vmc );
	if( count && (array->unitype == NULL || array->unitype == dao_array_any) ){
		DaoNamespace *ns = self->activeNamespace;
		DaoValue *p = self->activeValues[opA];
		DaoType *it = DaoNamespace_GetType( ns, p );
		DaoType *type = DaoNamespace_MakeType( ns, "array", DAO_ARRAY, NULL, & it, 1 );
		switch( p->type ){
			case DAO_INTEGER :
			case DAO_FLOAT :
			case DAO_DOUBLE :
			case DAO_COMPLEX : array->etype = p->type; break;
			case DAO_ARRAY : array->etype = p->xArray.etype; break;
			default : DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "invalid items" ); return;
		}
		GC_ShiftRC( type, array->unitype ) ;
		array->unitype = type;
	}
	for( j=0; j<count; j++){
		DaoValue *p = self->activeValues[ opA + j ];
		if( p == NULL || p->type != DAO_ARRAY ) continue;
		if( dims == NULL ){
			ndim = p->xArray.ndim;
			dims = p->xArray.dims;
		}
		if( dims == p->xArray.dims ) continue;
		if( ndim != p->xArray.ndim ){
			DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "invalid items" );
			return;
		}
		for(m=0; m<ndim; m++){
			if( dims[m] != p->xArray.dims[m] ){
				dims = NULL;
				break;
			}
		}
	}
	if( dims ){
		DaoArray_SetDimCount( array, ndim + 1 );
		array->dims[0] = count;
		memmove( array->dims + 1, dims, ndim*sizeof(size_t) );
		DaoArray_ResizeArray( array, array->dims, ndim );
	}else{
		DaoArray_ResizeVector( array, count );
	}
	k = 0;
	if( array->etype == DAO_INTEGER ){
		dint *vals = array->data.i;
		for( j=0; j<count; j++ ){
			DaoValue *p = self->activeValues[ opA + j ];
			if( p && p->type == DAO_ARRAY ){
				DaoArray *array2 = & p->xArray;
				for(i=0; i<array2->size; i++){
					vals[k] = DaoArray_GetInteger( array2, i );
					k++;
				}
			}else{
				vals[k] = DaoValue_GetInteger( p );
				k ++;
			}
		}
	}else if( array->etype == DAO_FLOAT ){
		float *vals = array->data.f;
		for( j=0; j<count; j++ ){
			DaoValue *p = self->activeValues[ opA + j ];
			if( p && p->type == DAO_ARRAY ){
				DaoArray *array2 = & p->xArray;
				for(i=0; i<array2->size; i++){
					vals[k] = DaoArray_GetFloat( array2, i );
					k++;
				}
			}else{
				vals[k] = DaoValue_GetFloat( p );
				k ++;
			}
		}
	}else if( array->etype == DAO_DOUBLE ){
		double *vals = array->data.d;
		for( j=0; j<count; j++ ){
			DaoValue *p = self->activeValues[ opA + j ];
			if( p && p->type == DAO_ARRAY ){
				DaoArray *array2 = & p->xArray;
				for(i=0; i<array2->size; i++){
					vals[k] = DaoArray_GetDouble( array2, i );
					k++;
				}
			}else{
				vals[k] = DaoValue_GetDouble( p );
				k ++;
			}
		}
	}else{
		complex16 *vals = array->data.c;
		for( j=0; j<count; j++ ){
			DaoValue *p = self->activeValues[ opA + j ];
			if( p && p->type == DAO_ARRAY ){
				DaoArray *array2 = & p->xArray;
				for(i=0; i<array2->size; i++){
					vals[k] = DaoArray_GetComplex( array2, i );
					k++;
				}
			}else{
				vals[k] = DaoValue_GetComplex( p );
				k ++;
			}
		}
	}
#else
	self->activeCode = vmc;
	DaoProcess_RaiseException( self, DAO_ERROR, getCtInfo( DAO_DISABLED_NUMARRAY ) );
#endif
}
void DaoProcess_DoRange(  DaoProcess *self, DaoVmCode *vmc )
{
	const ushort_t opA = vmc->a;
	const ushort_t bval = vmc->b;
	
	DaoList *list = DaoProcess_GetList( self, vmc );
	DaoValue **items, **regValues = self->activeValues;
	DaoValue *dn = bval==3 ? regValues[opA+2] : regValues[opA+1];
	int i, num = (int)DaoValue_GetDouble( dn );
	int ta = regValues[ opA ]->type;
	double step;
	
	self->activeCode = vmc;
	if( dn->type < DAO_INTEGER || dn->type > DAO_DOUBLE ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "need number" );
		return;
	}
	if( ta < DAO_INTEGER || ta >= DAO_ENUM ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "need a number or string as first value" );
		return;
	}
	if( ( self->vmSpace->options & DAO_EXEC_SAFE ) && num > 1000 ){
		DaoProcess_RaiseException( self, DAO_ERROR, "not permitted" );
		return;
	}
	DArray_Resize( & list->items, num, NULL );
	items = list->items.items.pValue;
	for( i=0; i<num; i++){
		complex16 com = {0.0,0.0};
		DaoValue *it = NULL;
		if( items[i] && items[i]->type == ta ) continue;
		switch( ta ){
			case DAO_INTEGER : it = (DaoValue*) DaoInteger_New(0); break;
			case DAO_FLOAT   : it = (DaoValue*) DaoFloat_New(0.0); break;
			case DAO_DOUBLE  : it = (DaoValue*) DaoDouble_New(0.0); break;
			case DAO_COMPLEX : it = (DaoValue*) DaoComplex_New(com); break;
			case DAO_LONG    : it = (DaoValue*) DaoLong_New(0); break;
			case DAO_STRING  : it = (DaoValue*) DaoString_New(1); break;
		}
		GC_ShiftRC( it, items[i] );
		items[i] = it;
	}
	
	step = bval == 2 ? 1.0 : DaoValue_GetDouble( regValues[ opA+1 ] );
	switch( ta ){
		case DAO_INTEGER :
		{
			const dint first = regValues[ vmc->a ]->xInteger.value;
			if( bval == 3 && regValues[ opA+1 ]->type == DAO_INTEGER ){
				const dint step = regValues[ opA+1 ]->xInteger.value;
				for( i=0; i<num; i++) items[i]->xInteger.value = first + i * step;
			}else{
				for( i=0; i<num; i++) items[i]->xInteger.value = first + i * step;
			}
			break;
		}
		case DAO_FLOAT :
		{
			const double first = regValues[ vmc->a ]->xFloat.value;
			for( i=0; i<num; i++) items[i]->xFloat.value = first + i * step;
			break;
		}
		case DAO_DOUBLE :
		{
			const double first = regValues[ vmc->a ]->xDouble.value;
			for( i=0; i<num; i++) items[i]->xDouble.value = first + i * step;
			break;
		}
		case DAO_COMPLEX :
		{
			complex16 first = regValues[ opA ]->xComplex.value;
			complex16 step = {1,0};
			if( bval >2 ) step = DaoValue_GetComplex( regValues[ opA+1 ] );
			for( i=0; i<num; i++){
				items[i]->xComplex.value.real = first.real + i * step.real;
				items[i]->xComplex.value.imag = first.imag + i * step.imag;
			}
			break;
		}
		case DAO_LONG :
		{
			DLong *first = regValues[ opA ]->xLong.value;
			DLong *buf, *step;
			if( num == 0 ) break;
			buf = step = DLong_New();
			if( bval > 2 ){
				if( regValues[opA+1]->type == DAO_LONG ){
					step = regValues[opA+1]->xLong.value;
				}else{
					DLong_FromInteger( buf, DaoValue_GetInteger( regValues[opA+1] ) );
				}
			}else{
				DLong_FromInteger( step, 1 );
			}
			DLong_Move( items[0]->xLong.value, first );
			for(i=1; i<num; i++) DLong_Add( items[i]->xLong.value, items[i-1]->xLong.value, step );
			DLong_Delete( buf );
			DLong_Delete( step );
			break;
		}
		case DAO_STRING :
		{
			DString *first = regValues[ opA ]->xString.data;
			DString *one, *step = NULL;
			if( bval > 2 ) step = regValues[ opA+1 ]->xString.data; /* XXX check */
			one = DString_Copy( first );
			for(i=0; i<num; i++){
				DString_Assign( items[i]->xString.data, one );
				if( step ) DString_Append( one, step );
			}
			DString_Delete( one );
			break;
		}
		case DAO_ARRAY :
			/* XXX */
			break;
		default: break;
	}
	if( self->activeTypes[ vmc->c ] == NULL ){
		DaoNamespace *ns = self->activeNamespace;
		DaoType *et = DaoNamespace_GetType( ns, regValues[opA] );
		DaoType *tp = DaoNamespace_MakeType( ns, "list", DAO_LIST, NULL, & et, et !=NULL );
		GC_ShiftRC( tp, list->unitype );
		list->unitype = tp;
	}
}
void DaoProcess_DoNumRange( DaoProcess *self, DaoVmCode *vmc )
{
#ifdef DAO_WITH_NUMARRAY
	const ushort_t opA = vmc->a;
	const ushort_t bval = vmc->b;
	DaoValue **regValues = self->activeValues;
	DaoValue *dn = bval==3 ? regValues[opA+2] : regValues[opA+1];
	const size_t num = DaoValue_GetInteger( dn );
	char steptype = bval == 3 ? regValues[opA+1]->type : 0;
	char type = regValues[ opA ]->type;
	size_t i, j, k, m, N, S, transvec = 0; /* transposed vector */
	double step;
	DaoArray *array = NULL;
	DaoArray *a0, *a1;
	
	self->activeCode = vmc;
	if( dn->type < DAO_INTEGER || dn->type > DAO_DOUBLE ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "need number" );
		return;
	}
	if( ( self->vmSpace->options & DAO_EXEC_SAFE ) && num > 1000 ){
		DaoProcess_RaiseException( self, DAO_ERROR, "not permitted" );
		return;
	}
	array = DaoProcess_GetArray( self, vmc );
	if( array->unitype == NULL || array->unitype == dao_array_any ){
		DaoNamespace *ns = self->activeNamespace;
		DaoType *it = DaoNamespace_GetType( ns, regValues[opA] );
		DaoType *type = DaoNamespace_MakeType( ns, "array", DAO_ARRAY, NULL, & it, 1 );
		GC_ShiftRC( type, array->unitype ) ;
		array->unitype = type;
		array->etype = regValues[opA]->type;
	}
	DaoArray_ResizeVector( array, num );
	
	step = bval == 2 ? 1.0 : DaoValue_GetDouble( regValues[opA+1] );
	switch( type ){
		case DAO_INTEGER :
		{
			const dint first = regValues[ vmc->a ]->xInteger.value;
			if( steptype == DAO_INTEGER ){
				const dint step = regValues[opA+1]->xInteger.value;
				for(i=0; i<num; i++) array->data.i[i] = first + i*step;
			}else{
				for(i=0; i<num; i++) array->data.i[i] = first + i*step;
			}
			break;
		}
		case DAO_FLOAT :
		{
			const double first = (const double)regValues[ vmc->a ]->xFloat.value;
			for(i=0; i<num; i++) array->data.f[i] = first + i*step;
			break;
		}
		case DAO_DOUBLE :
		{
			const double first = (const double)regValues[ vmc->a ]->xDouble.value;
			for(i=0; i<num; i++) array->data.d[i] = first + i*step;
			break;
		}
		case DAO_COMPLEX :
		{
			const complex16 dc = {1,0};
			const complex16 step = (bval==2) ? dc: DaoValue_GetComplex( regValues[ opA+1 ] );
			complex16 first = DaoValue_GetComplex( regValues[ opA ] );
			for(i=0; i<num; i++){
				array->data.c[i] = first;
				COM_IP_ADD( first, step );
			}
			break;
		}
		case DAO_ARRAY :
			a0 = & regValues[ opA ]->xArray;
			array->etype = a0->etype;
			if( a0->ndim == 2 && (a0->dims[0] == 1 || a0->dims[1] == 1) ){
				DaoArray_SetDimCount( array, 2 );
				memmove( array->dims, a0->dims, 2*sizeof(size_t) );
				array->dims[ a0->dims[1] == 1 ] = num;
				transvec = a0->dims[1] == 1;
			}else{
				DaoArray_SetDimCount( array, a0->ndim + 1 );
				array->dims[0] = num;
				memmove( array->dims + 1, a0->dims, a0->ndim*sizeof(size_t) );
			}
			DaoArray_ResizeArray( array, array->dims, array->ndim );
			S = a0->size;
			N = num * a0->size;
			if( steptype == DAO_ARRAY ){
				const char* const msg[2] = { "invalid step array", "unmatched init and step array" };
				int error = -1;
				a1 = & regValues[ opA+1 ]->xArray;
				if( a0->etype <= DAO_DOUBLE && a1->etype >= DAO_COMPLEX ){
					error = 0;
				}else if( a1->ndim != a0->ndim ){
					error = 1;
				}else{
					for(i=0; i<a0->ndim; i++){
						if( a0->dims[i] != a1->dims[i] ){
							error = 1;
							break;
						}
					}
				}
				if( error >=0 ){
					DaoProcess_RaiseException( self, DAO_ERROR_VALUE, msg[error] );
					return;
				}
				for(i=0, m = 0, j=0, k = 0; i<N; i++, m=i, j=i%S, k=i/S){
					if( transvec ) m = j * num + k;
					switch( a0->etype ){
						case DAO_INTEGER :
							if( a1->etype == DAO_INTEGER ){
								array->data.i[m] = a0->data.i[j] + k*a1->data.i[j];
							}else{
								array->data.i[m] = a0->data.i[j] + k*DaoArray_GetDouble( a1, j );
							}
							break;
						case DAO_FLOAT :
							array->data.f[m] = a0->data.f[j] + k*DaoArray_GetDouble( a1, j );
							break;
						case DAO_DOUBLE :
							array->data.d[m] = a0->data.d[j] + k*DaoArray_GetDouble( a1, j );
							break;
						case DAO_COMPLEX :
							if( a1->etype == DAO_COMPLEX ){
								array->data.c[m].real = a0->data.c[j].real + k*a1->data.c[j].real;
								array->data.c[m].imag = a0->data.c[j].imag + k*a1->data.c[j].imag;
							}else{
								array->data.c[m].real = a0->data.c[j].real + k*DaoArray_GetDouble( a1, j );
								array->data.c[m].imag = a0->data.c[j].imag;
							}
							break;
						default : break;
					}
				}
			}else{
				int istep = steptype == DAO_INTEGER;
				dint intstep = istep ? regValues[opA+1]->xInteger.value : 0;
				complex16 cstep = { 1.0, 0.0 };
				if( steptype == DAO_COMPLEX ) cstep = regValues[ opA+1 ]->xComplex.value;
				for(i=0, m = 0, j=0, k = 0; i<N; i++, m=i, j=i%S, k=i/S){
					if( transvec ) m = j * num + k;
					switch( a0->etype ){
						case DAO_INTEGER :
							array->data.i[m] = a0->data.i[j] + (istep ? k * intstep : (dint)(k * step));
							break;
						case DAO_FLOAT :
							array->data.f[m] = a0->data.f[j] + k * step;
							break;
						case DAO_DOUBLE :
							array->data.d[m] = a0->data.d[j] + k * step;
							break;
						case DAO_COMPLEX :
							array->data.c[m].real = a0->data.c[j].real + k * cstep.real;
							array->data.c[m].imag = a0->data.c[j].imag + k * cstep.imag;
							break;
					}
				}
			}
			break;
		default: break;
	}
	if( ( self->vmSpace->options & DAO_EXEC_SAFE ) && array->size > 5000 ){
		DaoProcess_RaiseException( self, DAO_ERROR, "not permitted" );
		return;
	}
#else
	DaoProcess_RaiseException( self, DAO_ERROR, getCtInfo( DAO_DISABLED_NUMARRAY ) );
#endif
}
void DaoProcess_DoMap( DaoProcess *self, DaoVmCode *vmc )
{
	int i, c;
	const ushort_t opA = vmc->a;
	const ushort_t bval = vmc->b;
	DaoNamespace *ns = self->activeNamespace;
	DaoValue **pp = self->activeValues;
	DaoMap *map = DaoProcess_GetMap( self, vmc );
	
	if( bval == 2 && pp[opA]->type ==0 && pp[opA+1]->type ==0 ) return;
	for( i=0; i<bval-1; i+=2 ){
		if( (c = DaoMap_Insert( map, pp[opA+i], pp[opA+i+1] ) ) ){
			if( c ==1 ){
				DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "key not matching" );
			}else if( c ==2 ){
				DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "value not matching" );
			}
			break;
		}
	}
	if( bval >0 && self->activeTypes[ vmc->c ] ==NULL ){
		/* for constant evaluation only */
		DaoType *any = DaoNamespace_MakeType( ns, "any", DAO_ANY, 0,0,0 );
		DaoType *t, *tp[2];
		tp[0] = DaoNamespace_GetType( ns, pp[opA] );
		tp[1] = DaoNamespace_GetType( ns, pp[opA+1] );
		for(i=2; i<bval; i+=2){
			DaoType *tk = DaoNamespace_GetType( ns, pp[opA+i] );
			DaoType *tv = DaoNamespace_GetType( ns, pp[opA+i+1] );
			if( DaoType_MatchTo( tk, tp[0], 0 )==0 ) tp[0] = any;
			if( DaoType_MatchTo( tv, tp[1], 0 )==0 ) tp[1] = any;
			if( tp[0] ==any && tp[1] ==any ) break;
		}
		t = DaoNamespace_MakeType( ns, "map", DAO_MAP, NULL, tp, 2 );
		GC_ShiftRC( t, map->unitype );
		map->unitype = t;
	}
}
void DaoProcess_DoMatrix( DaoProcess *self, DaoVmCode *vmc )
{
#ifdef DAO_WITH_NUMARRAY
	const ushort_t opA = vmc->a;
	const ushort_t bval = vmc->b;
	int i, size, numtype = DAO_INTEGER;
	DaoValue **regv = self->activeValues;
	DaoArray *array = NULL;
	
	size_t dim[2];
	dim[0] = (0xff00 & bval)>>8;
	dim[1] = 0xff & bval;
	size = dim[0] * dim[1];
	array = DaoProcess_GetArray( self, vmc );
	if( size ){
		numtype = regv[opA]->type;
		if( numtype == DAO_NONE || numtype > DAO_COMPLEX ){
			DaoProcess_RaiseException( self, DAO_ERROR, "invalid matrix enumeration" );
			return;
		}
	}
	if( array->unitype == NULL || array->unitype == dao_array_any ){
		DaoNamespace *ns = self->activeNamespace;
		DaoType *it = DaoNamespace_GetType( ns, regv[opA] );
		DaoType *type = DaoNamespace_MakeType( ns, "array", DAO_ARRAY, NULL, & it, 1 );
		GC_ShiftRC( type, array->unitype ) ;
		array->unitype = type;
		array->etype = numtype;
	}
	/* TODO: more restrict type checking on elements. */
	DaoArray_ResizeArray( array, dim, 2 );
	if( numtype == DAO_INTEGER ){
		dint *vec = array->data.i;
		for(i=0; i<size; i++) vec[i] = DaoValue_GetInteger( regv[ opA+i ] );
	}else if( numtype == DAO_FLOAT ){
		float *vec = array->data.f;
		for(i=0; i<size; i++) vec[i] = DaoValue_GetFloat( regv[ opA+i ] );
	}else if( numtype == DAO_DOUBLE ){
		double *vec = array->data.d;
		for(i=0; i<size; i++) vec[i] = DaoValue_GetDouble( regv[ opA+i ] );
	}else{
		complex16 *vec = array->data.c;
		for(i=0; i<size; i++) vec[i] = DaoValue_GetComplex( regv[ opA+i ] );
	}
#else
	self->activeCode = vmc;
	DaoProcess_RaiseException( self, DAO_ERROR, getCtInfo( DAO_DISABLED_NUMARRAY ) );
#endif
}

void DaoProcess_DoCurry( DaoProcess *self, DaoVmCode *vmc )
{
	int i, k;
	int opa = vmc->a;
	int opb = vmc->b;
	DaoObject *object;
	DaoType **mtype;
	DaoValue *selfobj = NULL;
	DaoValue *p = self->activeValues[opa];
	DNode *node;
	
	if( vmc->code == DVM_MCURRY ){
		selfobj = self->activeValues[opa+1];
		opa ++;
		opb --;
	}
	
	self->activeCode = vmc;
	switch( p->type ){
		case DAO_CLASS :
		{
			DaoClass *klass = & p->xClass;
			object = DaoObject_New( klass );
			DaoProcess_SetValue( self, vmc->c, (DaoValue*)object );
			mtype = klass->objDataType->items.pType;
			if( opb >= object->valueCount ){
				DaoProcess_RaiseException( self, DAO_ERROR, "enumerating too many members" );
				break;
			}
			for( i=0; i<opb; i++){
				k = i+1; /* skip self */
				p = self->activeValues[opa+i+1];
				if( p->type == DAO_PAR_NAMED ){
					DaoNameValue *nameva = & p->xNameValue;
					node = DMap_Find( klass->lookupTable, nameva->name );
					if( node == NULL || LOOKUP_ST( node->value.pSize ) != DAO_OBJECT_VARIABLE ){
						DaoProcess_RaiseException( self, DAO_ERROR_FIELD_NOTEXIST, "" );
						break;
					}
					k = LOOKUP_ID( node->value.pSize );
					p = nameva->value;
				}
				if( DaoValue_Move( p, object->objValues + k, mtype[k] ) ==0 ){
					DaoType *type = DaoNamespace_GetType( self->activeNamespace, p );
					DaoProcess_RaiseTypeError( self, type, mtype[k], "moving" );
					break;
				}
			}
			break;
		}
		case DAO_ROUTINE :
		{
			DaoFunCurry *curry = DaoFunCurry_New( p, selfobj );
			DaoProcess_SetValue( self, vmc->c, (DaoValue*)curry );
			for( i=0; i<opb; i++) DArray_Append( curry->params, self->activeValues[opa+i+1] );
			break;
		}
		case DAO_FUNCURRY :
		{
			DaoFunCurry *curry = (DaoFunCurry*) p;
			DaoProcess_SetValue( self, vmc->c, (DaoValue*)curry );
			for(i=0; i<opb; i++) DArray_Append( curry->params, self->activeValues[opa+i+1] );
			break;
		}
		case DAO_TYPE :
		{
			DaoType *type = (DaoType*) p;
			DaoTuple *tuple = DaoProcess_GetTuple( self, type, opb, 0 );
			if( type->tid != DAO_TUPLE ){
				DaoProcess_RaiseException( self, DAO_ERROR, "invalid enumeration" );
				break;
			}
			DaoProcess_MakeTuple( self, tuple, self->activeValues + opa + 1, opb );
			break;
		}
		default :
			DaoProcess_RaiseException( self, DAO_ERROR, "invalid enumeration" );
			break;
	}
}

/* Operator (in daoBitBoolArithOpers) validity rules,
 for operation involving DaoObject:
 
 A. when one of the operand is not DaoObject:
 1. all these operators are not valid, unless overloaded;
 
 B. when both operands are DaoObject:
 
 1. AND, OR, LT, LE, EQ, NE are valid, only if none operator
 in daoBitBoolArithOpers is overloaded; In this case,
 the operations will be based on pointers;
 
 2. AND, OR, LT, LE, EQ, NE are based on pointers, if they
 are used inside the function overloaded for the same
 operator. Example:
 
 class Test{
 operator == ( A : Test, B : Test ){
 return A == B; # this will be based on pointers!
 }
 }
 
 3. since "A>B" (or "A>=B") is compiled as "B<A" (or "B<=A"),
 when a DVM_LT or DVM_LE is executed, "operator<()"
 or "operator<=()" will be search first, if not found,
 then "operator>()" or "operator>=()" is searched,
 and applied by swapping A and B'
 
 4. "A<B" and "A>B" inside "operator<()" and "operator>()"
 or "A<=B" and "A>=B" inside "operator<=()" and "operator>=()"
 will be based on pointers.
 */
/* Examples of possible ways of operator overloading:
 All these overloading functions must be "static",
 namely, they do not require a class instance for being invoked:
 
 Unary operation:
 operator ! ( C : Number, A : Number ){... return C_or_something_else}
 operator ! ( A : Number ){... return something}
 
 Binary operation:
 operator + ( C : Number, A : Number, B : Number ){... return C_or_else}
 operator + ( A : Number, B : Number ){... return something}
 
 The first method is always tried first if C is found NOT to be null,
 and have reference count equal to one;
 
 For binary operation, if C == A, the following will be tried first:
 operator += ( C : Number, B : Number ){... return C_or_else}
 */
int DaoProcess_TryObjectArith( DaoProcess *self, DaoValue *A, DaoValue *B, DaoValue *C )
{
	DaoRoutine *rout = 0;
	DaoObject *object;
	DaoClass *klass;
	DString *name = self->mbstring;
	DaoValue **p, *par[3];
	DaoValue *value = NULL;
	int code = self->activeCode->code;
	int boolres = code >= DVM_AND && code <= DVM_NE;
	int bothobj = B ? A->type == B->type : 0;
	int recursive = 0;
	int overloaded = 0;
	int compo = 0; /* try composite operator */
	int nopac = 0; /* do not pass C as parameter */
	int npar = 3;
	int rc, n;
	
	/* C = A + B */
	par[0] = C;
	par[1] = A;
	par[2] = B;
	if( C == A && B && daoBitBoolArithOpers2[ code-DVM_MOVE ] ){
		/* C += B, or C = C + B */
		par[1] = B;
		npar = 2;
		compo = 1;
	}else if( B == NULL ){ /* C = ! A */
		par[1] = A;
		npar = 2;
	}
	nopac = C == NULL || C->xNone.refCount > 1;
	p = par;
	n = npar;
	
TryAgain:
	if( compo )
		DString_SetMBS( name, daoBitBoolArithOpers2[ code-DVM_MOVE ] );
	else
		DString_SetMBS( name, daoBitBoolArithOpers[ code-DVM_MOVE ] );
	if( DString_EQ( name, self->activeRoutine->routName ) ) recursive = 1;
	
	object = A->type == DAO_OBJECT ? & A->xObject : & B->xObject;
	klass = object->defClass;
	overloaded = klass->attribs & DAO_OPER_OVERLOADED;
	
	rc = DaoObject_GetData( object, name, & value,  self->activeObject );
	if( rc && (code == DVM_LT || code == DVM_LE) ){
		if( code == DVM_LT ){
			DString_SetMBS( name, ">" );
		}else{
			DString_SetMBS( name, ">=" );
		}
		par[1] = B;
		par[2] = A;
		rc = DaoObject_GetData( object, name, & value,  self->activeObject );
		if( DString_EQ( name, self->activeRoutine->routName ) ) recursive = 1;
	}
	if( bothobj && boolres && recursive ) return 0;
	if( rc || value->type != DAO_ROUTINE ){
		if( bothobj && boolres && overloaded ==0 ) return 0;
		goto ArithError;
	}
	if( compo ==0 ){
		p = par + nopac;
		n = npar - nopac;
	}
	rout = (DaoRoutine*) value;
	if( DaoProcess_PushCallable( self, rout, NULL, p, n ) == DAO_ERROR_PARAM ) goto ArithError;
	
	return 1;
ArithError:
	if( compo ){
		compo = 0;
		par[0] = C;
		par[1] = A;
		par[2] = B;
		npar = 3;
		goto TryAgain;
	}
	if( nopac == 0 ){
		nopac = 1;
		goto TryAgain;
	}
	DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "" );
	return 0;
}
/* Similar to DaoProcess_TryObjectArith(),
 but without consideration for recursion. */
int DaoProcess_TryCdataArith( DaoProcess *self, DaoValue *A, DaoValue *B, DaoValue *C )
{
	DaoCdata *cdata = NULL;
	DaoRoutine *func;
	DaoValue **p, *par[3];
	DString *name = self->mbstring;
	int code = self->activeCode->code;
	int boolres = code >= DVM_AND && code <= DVM_NE;
	int bothobj = B ? A->type == B->type : 0;
	int overloaded = 0;
	int compo = 0; /* try composite operator */
	int nopac = 0; /* do not pass C as parameter */
	int n, npar = 3;
	
	/* C = A + B */
	par[0] = C;
	par[1] = A;
	par[2] = B;
	if( C == A && B && daoBitBoolArithOpers2[ code-DVM_MOVE ] ){
		/* C += B, or C = C + B */
		par[1] = B;
		npar = 2;
		compo = 1;
	}else if( B == NULL ){ /* C = ! A */
		par[1] = A;
		npar = 2;
	}
	nopac = C == NULL || C->xNone.refCount > 1;
	p = par;
	n = npar;
	
TryAgain:
	if( compo )
		DString_SetMBS( name, daoBitBoolArithOpers2[ code-DVM_MOVE ] );
	else
		DString_SetMBS( name, daoBitBoolArithOpers[ code-DVM_MOVE ] );
	
	cdata = A->type == DAO_CDATA ? & A->xCdata : & B->xCdata;
	overloaded = cdata->ctype->kernel->attribs & DAO_OPER_OVERLOADED;
	func = DaoType_FindFunction( cdata->ctype, name );
	if( func ==NULL && (code == DVM_LT || code == DVM_LE) ){
		if( code == DVM_LT ){
			DString_SetMBS( name, ">" );
		}else{
			DString_SetMBS( name, ">=" );
		}
		par[1] = B;
		par[2] = A;
		func = DaoType_FindFunction( cdata->ctype, name );
	}
	if( func == NULL ){
		if( bothobj && boolres ) return 0;
		goto ArithError;
	}
	if( compo ==0 ){
		p = par + nopac;
		n = npar - nopac;
	}
	if( DaoProcess_PushCallable( self, func, NULL, p, n ) == DAO_ERROR_PARAM ) goto ArithError;
	
	return 1;
ArithError:
	if( compo ){
		compo = 0;
		par[0] = C;
		par[1] = A;
		par[2] = B;
		npar = 3;
		goto TryAgain;
	}
	if( nopac == 0 ){
		nopac = 1;
		goto TryAgain;
	}
	DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "" );
	return 0;
}
static void DaoProcess_LongDiv ( DaoProcess *self, DLong *z, DLong *x, DLong *y, DLong *r )
{
	if( x->size ==0 || (x->size ==1 && x->data[0] ==0) ){
		//XXX self->idClearFE = self->activeCode - self->topFrame->codes;
		DaoProcess_RaiseException( self, DAO_ERROR_FLOAT_DIVBYZERO, "" );
		return;
	}
	DLong_Div( z, x, y, r );
}
static int DaoProcess_CheckLong2Integer( DaoProcess *self, DLong *x )
{
	short d = 8*sizeof(dint);
	if( x->size * LONG_BITS < d ) return 1;
	if( (x->size - 1) * LONG_BITS >= d ) goto RaiseInexact;
	d -= (x->size - 1) * LONG_BITS + 1; /* one bit for sign */
	if( (x->data[ x->size - 1 ] >> d) > 0 ) goto RaiseInexact;
	return 1;
RaiseInexact:
	//XXX self->idClearFE = self->activeCode - self->topFrame->codes;
	DaoProcess_RaiseException( self, DAO_ERROR_VALUE,
							  "long integer value is too big for the operation" );
	return 0;
}
void DaoProcess_DoBinArith( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *A = self->activeValues[ vmc->a ];
	DaoValue *B = self->activeValues[ vmc->b ];
	DaoValue *C = self->activeValues[ vmc->c ];
	
	self->activeCode = vmc;
	if( A == NULL || B == NULL ){
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "on none object" );
		return;
	}
	
	if( A->type == DAO_OBJECT || B->type == DAO_OBJECT ){
		self->activeCode = vmc;
		DaoProcess_TryObjectArith( self, A, B, C );
		return;
	}else if( A->type == DAO_CDATA || B->type == DAO_CDATA ){
		self->activeCode = vmc;
		DaoProcess_TryCdataArith( self, A, B, C );
		return;
	}
	
	if( A->type >= DAO_INTEGER && A->type <= DAO_DOUBLE && B->type >= DAO_INTEGER && B->type <= DAO_DOUBLE ){
		DaoValue *val;
		DaoDouble buf = {DAO_DOUBLE,0,0,0,0,0.0};
		int type = A->type > B->type ? A->type : B->type;
		double res = 0;
		long_t ib;
		switch( vmc->code ){
			case DVM_MOD:
				ib = DaoValue_GetInteger( B );
				if( ib ==0 ){
					//XXX self->idClearFE = self->activeCode - self->topFrame->codes;
					DaoProcess_RaiseException( self, DAO_ERROR_FLOAT_DIVBYZERO, "" );
				}
				res = DaoValue_GetInteger( A ) % ib;
				break;
			case DVM_ADD: res = DaoValue_GetDouble( A ) + DaoValue_GetDouble( B ); break;
			case DVM_SUB: res = DaoValue_GetDouble( A ) - DaoValue_GetDouble( B ); break;
			case DVM_MUL: res = DaoValue_GetDouble( A ) * DaoValue_GetDouble( B ); break;
			case DVM_DIV: res = DaoValue_GetDouble( A ) / DaoValue_GetDouble( B ); break;
			case DVM_POW: res = powf( DaoValue_GetDouble( A ), DaoValue_GetDouble( B ) ); break;
			default : break;
		}
		val = (DaoValue*) & buf;
		val->type = type;
		switch( type ){
			case DAO_INTEGER: val->xInteger.value = res; break;
			case DAO_FLOAT :  val->xFloat.value = res; break;
			case DAO_DOUBLE : val->xDouble.value = res; break;
			default : val->type = 0;  break;
		}
		DaoProcess_SetValue( self, vmc->c, val );
		return;
	}else if( B->type >=DAO_INTEGER && B->type <=DAO_DOUBLE && A->type ==DAO_COMPLEX ){
		DaoComplex res = {DAO_COMPLEX,0,0,0,0,{0.0,0.0}};
		double f = DaoValue_GetDouble( B );
		res.value.real = A->xComplex.value.real;
		res.value.imag = A->xComplex.value.imag;
		switch( vmc->code ){
			case DVM_ADD: res.value.real += f; break;
			case DVM_SUB: res.value.real -= f; break;
			case DVM_MUL: res.value.real *= f; res.value.imag *= f; break;
			case DVM_DIV: res.value.real /= f; res.value.imag /= f; break;
			default: break; /* XXX: pow for complex??? */
		}
		DaoProcess_SetValue( self, vmc->c, (DaoValue*) & res );
	}else if( A->type >=DAO_INTEGER && A->type <=DAO_DOUBLE && B->type ==DAO_COMPLEX ){
		DaoComplex res = {DAO_COMPLEX,0,0,0,0,{0.0,0.0}};
		double n, f = DaoValue_GetDouble( A );
		double real = B->xComplex.value.real;
		double imag = B->xComplex.value.imag;
		switch( vmc->code ){
			case DVM_DIV:
				n = real * real + imag * imag;
				res.value.real = f * real / n;
				res.value.imag = f * imag / n;
				break;
			case DVM_ADD: res.value.real = f + real;  res.value.imag = imag; break;
			case DVM_SUB: res.value.real = f - real;  res.value.imag = - imag; break;
			case DVM_MUL: res.value.real = f * real;  res.value.imag = f * imag; break;
			default: break; /* XXX: pow for complex??? */
		}
		DaoProcess_SetValue( self, vmc->c, (DaoValue*) & res );
	}else if( A->type == DAO_COMPLEX && B->type == DAO_COMPLEX ){
		DaoComplex res = {DAO_COMPLEX,0,0,0,0,{0.0,0.0}};
		double AR = A->xComplex.value.real;
		double AI = A->xComplex.value.imag;
		double BR = B->xComplex.value.real;
		double BI = B->xComplex.value.imag;
		double N = 0;
		switch( vmc->code ){
			case DVM_ADD:
				res.value.real = AR + BR;
				res.value.imag = AI + BI;
				break;
			case DVM_SUB:
				res.value.real = AR - BR;
				res.value.imag = AI - BI;
				break;
			case DVM_MUL:
				res.value.real = AR * BR - AI * BI;
				res.value.imag = AR * BI + AI * BR;
				break;
			case DVM_DIV:
				N = BR * BR + BI * BI;
				res.value.real = (AR * BR + AI * BI) / N;
				res.value.imag = (AR * BI - AI * BR) / N;
				break;
			default: break; /* XXX: pow for complex??? */
		}
		DaoProcess_SetValue( self, vmc->c, (DaoValue*) & res );
	}else if( A->type == DAO_LONG && B->type == DAO_LONG ){
		DLong *b, *c = DaoProcess_GetLong( self, vmc );
		if( vmc->code == DVM_POW && DaoProcess_CheckLong2Integer( self, B->xLong.value ) == 0 ) return;
		b = DLong_New();
		switch( vmc->code ){
			case DVM_ADD : DLong_Add( c, A->xLong.value, B->xLong.value ); break;
			case DVM_SUB : DLong_Sub( c, A->xLong.value, B->xLong.value ); break;
			case DVM_MUL : DLong_Mul( c, A->xLong.value, B->xLong.value ); break;
			case DVM_DIV : DaoProcess_LongDiv( self, A->xLong.value, B->xLong.value, c, b ); break;
			case DVM_MOD : DaoProcess_LongDiv( self, A->xLong.value, B->xLong.value, b, c ); break;
			case DVM_POW : DLong_Pow( c, A->xLong.value, DLong_ToInteger( B->xLong.value ) ); break;
			default : break;
		}    
		DLong_Delete( b ); 
	}else if( A->type == DAO_LONG && B->type >= DAO_INTEGER && B->type <= DAO_DOUBLE ){
		DLong *c = DaoProcess_GetLong( self, vmc );
		DLong *b = DLong_New();
		DLong *b2 = DLong_New();
		dint i = DaoValue_GetInteger( B );
		DLong_FromInteger( b, i );
		switch( vmc->code ){
			case DVM_ADD : DLong_Add( c, A->xLong.value, b ); break;
			case DVM_SUB : DLong_Sub( c, A->xLong.value, b ); break;
			case DVM_MUL : DLong_Mul( c, A->xLong.value, b ); break;
			case DVM_DIV : DaoProcess_LongDiv( self, A->xLong.value, b, c, b2 ); break;
			case DVM_MOD : DaoProcess_LongDiv( self, A->xLong.value, b, b2, c ); break;
			case DVM_POW : DLong_Pow( c, A->xLong.value, i ); break;
			default: break;
		}
		DLong_Delete( b );
		DLong_Delete( b2 );
	}else if( B->type == DAO_LONG && A->type >= DAO_INTEGER && A->type <= DAO_DOUBLE ){
		DLong *a, *b2, *c = DaoProcess_GetLong( self, vmc );
		dint i = DaoValue_GetInteger( A );
		if( vmc->code == DVM_POW && DaoProcess_CheckLong2Integer( self, B->xLong.value ) == 0 ) return;
		a = DLong_New();
		b2 = DLong_New();
		DLong_FromInteger( a, i );
		switch( vmc->code ){
			case DVM_ADD : DLong_Add( c, a, B->xLong.value ); break;
			case DVM_SUB : DLong_Sub( c, a, B->xLong.value ); break;
			case DVM_MUL : DLong_MulInt( c, B->xLong.value, i ); break;
			case DVM_DIV : DaoProcess_LongDiv( self, a, B->xLong.value, c, b2 ); break;
			case DVM_MOD : DaoProcess_LongDiv( self, a, B->xLong.value, b2, c ); break;
			case DVM_POW : DLong_Pow( c, a, DLong_ToInteger( B->xLong.value ) ); break;
			default: break;
		}
		DLong_Delete( a );
		DLong_Delete( b2 );
#ifdef DAO_WITH_NUMARRAY
	}else if( B->type >=DAO_INTEGER && B->type <=DAO_COMPLEX && A->type ==DAO_ARRAY ){
		DaoArray *na = & A->xArray;
		DaoArray *nc = na;
		if( vmc->a != vmc->c ) nc = DaoProcess_GetArray( self, vmc );
		DaoArray_array_op_number( nc, na, B, vmc->code, self );
	}else if( A->type >=DAO_INTEGER && A->type <=DAO_COMPLEX && B->type ==DAO_ARRAY ){
		DaoArray *nb = & B->xArray;
		DaoArray *nc = nb;
		if( vmc->b != vmc->c ) nc = DaoProcess_GetArray( self, vmc );
		DaoArray_number_op_array( nc, A, nb, vmc->code, self );
	}else if( A->type ==DAO_ARRAY && B->type ==DAO_ARRAY ){
		DaoArray *na = & A->xArray;
		DaoArray *nb = & B->xArray;
		DaoArray *nc;
		if( vmc->a == vmc->c )
			nc = na;
		else if( vmc->b == vmc->c )
			nc = nb;
		else
			nc = DaoProcess_GetArray( self, vmc );
		DaoArray_ArrayArith( nc, na, nb, vmc->code, self );
#endif
	}else if( A->type ==DAO_STRING && B->type ==DAO_INTEGER && vmc->code ==DVM_ADD
			 && vmc->a == vmc->c ){
		DString_AppendWChar( A->xString.data, (wchar_t) B->xInteger.value );
	}else if( A->type ==DAO_STRING && B->type ==DAO_STRING && vmc->code ==DVM_ADD ){
		if( vmc->a == vmc->c ){
			DString_Append( A->xString.data, B->xString.data );
		}else if( vmc->b == vmc->c ){
			DString_Insert( B->xString.data, A->xString.data, 0, 0, 0 );
		}else{
			DaoValue *C = DaoProcess_PutValue( self, A );
			DString_Append( C->xString.data, B->xString.data );
		}
	}else if( A->type == DAO_ENUM && B->type == DAO_ENUM
			 && (vmc->code == DVM_ADD || vmc->code == DVM_SUB) ){
		DaoType *ta = A->xEnum.etype;
		DaoType *tb = B->xEnum.etype;
		DaoEnum *denum = & A->xEnum;
		int rc = 0;
		if( vmc->c != vmc->a && ta->name->mbs[0] == '$' && tb->name->mbs[0] == '$' ){
			DaoNamespace *ns = self->activeNamespace;
			DaoType *type = NULL;
			dint value = 0;
			denum = DaoProcess_GetEnum( self, vmc );
			if( vmc->code == DVM_ADD ){
				type = DaoNamespace_SymbolTypeAdd( ns, ta, tb, &value );
			}else{
				type = DaoNamespace_SymbolTypeAdd( ns, ta, tb, &value );
			}
			if( type == NULL ) DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "symbol not found in the enum" );
			DaoEnum_SetType( denum, type );
			denum->value = value;
			return;
		}
		if( vmc->c != vmc->a ){
			denum = DaoProcess_GetEnum( self, vmc );
			if( denum->etype == NULL ) DaoEnum_SetType( denum, A->xEnum.etype );
			DaoEnum_SetValue( denum, & A->xEnum, NULL );
		}
		if( vmc->code == DVM_ADD ){
			rc = DaoEnum_AddValue( denum, & B->xEnum, NULL );
		}else{
			rc = DaoEnum_RemoveValue( denum, & B->xEnum, NULL );
		}
		if( rc == 0 ){
			if( denum->etype->flagtype ==0 )
				DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "not combinable enum" );
			else
				DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "symbol not found in the enum" );
			return;
		}
	}else if( A->type == DAO_LIST && B->type == DAO_LIST && vmc->code == DVM_ADD ){
		DaoList *lA = & A->xList;
		DaoList *lB = & B->xList;
		DaoList *list;
		size_t i = 0, NA = lA->items.size, NB = lB->items.size;
		if( vmc->a == vmc->c ){
			list = lA;
			for(i=0; i<NB; i++) DaoList_Append( list, lB->items.items.pValue[i] );
		}else if( vmc->b == vmc->c ){
			list = lB;
			for(i=NA; i>0; i--) DaoList_PushFront( list, lA->items.items.pValue[i-1] );
		}else{
			list = DaoProcess_GetList( self, vmc );
			DArray_Resize( & list->items, NA + NB, NULL );
			for(i=0; i<NA; i++) DaoList_SetItem( list, lA->items.items.pValue[i], i );
			for(i=0; i<NB; i++) DaoList_SetItem( list, lB->items.items.pValue[i], i + NA );
		}
	}else if( A->type == DAO_MAP && B->type == DAO_MAP && vmc->code == DVM_ADD ){
		DaoMap *hA = & A->xMap;
		DaoMap *hB = & B->xMap;
		DaoMap *hC;
		DNode *node;
		if( vmc->a == vmc->c ){
			hC = hA;
		}else if( vmc->a == vmc->c ){
			hC = hB;
			hB = hA;
		}else{
			hC = DaoProcess_GetMap( self, vmc );
			node = DMap_First( hA->items );
			for( ; node !=NULL; node=DMap_Next( hA->items, node) )
				DMap_Insert( hC->items, node->key.pVoid, node->value.pVoid );
		}
		node = DMap_First( hB->items );
		for( ; node !=NULL; node=DMap_Next( hB->items, node) )
			DMap_Insert( hC->items, node->key.pVoid, node->value.pVoid );
	}else{
		DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "" );
	}
}
/* binary operation with boolean result. */
void DaoProcess_DoBinBool(  DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *A = self->activeValues[ vmc->a ];
	DaoValue *B = self->activeValues[ vmc->b ];
	DaoValue *C = NULL;
	int D = 0, rc = 0;
	
	self->activeCode = vmc;
	if( A == NULL ) A = dao_none_value;
	if( B == NULL ) B = dao_none_value;
	if( A->type ==0 || B->type ==0 ){
		switch( vmc->code ){
			case DVM_AND: C = A->type ? B : A; break;
			case DVM_OR:  C = A->type ? A : B; break;
			case DVM_LT:  D = A->type < B->type; break;
			case DVM_LE:  D = A->type <= B->type; break;
			case DVM_EQ:  D = A->type == B->type; break;
			case DVM_NE:  D = A->type != B->type; break;
			default: break;
		}
		if( A->type == DAO_CDATA || B->type == DAO_CDATA ){
			DaoCdata *cdata = (DaoCdata*)( A->type == DAO_CDATA ? & A->xCdata : & B->xCdata );
			if( vmc->code == DVM_EQ ){
				D = cdata->data ? 0 : 1;
			}else if( vmc->code == DVM_NE ){
				D = cdata->data ? 1 : 0;
			}
		}
		if( C ) DaoProcess_PutValue( self, C );
		else DaoProcess_PutInteger( self, D );
		return;
	}
	
	if( A->type==DAO_OBJECT || B->type==DAO_OBJECT || A->type==DAO_CDATA || B->type==DAO_CDATA ){
		if( A->type ==DAO_OBJECT || B->type ==DAO_OBJECT ){
			rc = DaoProcess_TryObjectArith( self, A, B, C );
			if( rc == 0 ){
				switch( vmc->code ){
					case DVM_AND: C = A ? B : A; break;
					case DVM_OR:  C = A ? A : B; break;
					case DVM_LT:  D = A < B; break;
					case DVM_LE:  D = A <= B; break;
					case DVM_EQ:  D = A == B; break;
					case DVM_NE:  D = A != B; break;
					default: break;
				}
				if( C ) DaoProcess_PutValue( self, C );
				else DaoProcess_PutInteger( self, D );
			}
		}else if( A->type ==DAO_CDATA || B->type ==DAO_CDATA ){
			rc = DaoProcess_TryCdataArith( self, A, B, C );
			if( rc == 0 ){
				switch( vmc->code ){
					case DVM_AND: C = A->xCdata.data ? B : A; break;
					case DVM_OR:  C = A->xCdata.data ? A : B; break;
					case DVM_LT:  D = A->xCdata.data < B->xCdata.data; break;
					case DVM_LE:  D = A->xCdata.data <= B->xCdata.data; break;
					case DVM_EQ:  D = A->xCdata.data == B->xCdata.data; break;
					case DVM_NE:  D = A->xCdata.data != B->xCdata.data; break;
					default: break;
				}
				if( C ) DaoProcess_PutValue( self, C );
				else DaoProcess_PutInteger( self, D );
			}
		}
		return;
	}
	
	if( A->type >= DAO_INTEGER && A->type <= DAO_DOUBLE
	   && B->type >= DAO_INTEGER && B->type <= DAO_DOUBLE ){
		switch( vmc->code ){
			case DVM_AND: C = DaoValue_GetDouble( A ) ? B : A; break;
			case DVM_OR:  C = DaoValue_GetDouble( A ) ? A : B; break;
			case DVM_LT:  D = DaoValue_GetDouble( A ) < DaoValue_GetDouble( B ); break;
			case DVM_LE:  D = DaoValue_GetDouble( A ) <= DaoValue_GetDouble( B ); break;
			case DVM_EQ:  D = DaoValue_GetDouble( A ) == DaoValue_GetDouble( B ); break;
			case DVM_NE:  D = DaoValue_GetDouble( A ) != DaoValue_GetDouble( B ); break;
			default: break;
		}
	}else if( A->type == DAO_COMPLEX && B->type == DAO_COMPLEX ){
		double AR = A->xComplex.value.real, AI = A->xComplex.value.imag;
		double BR = B->xComplex.value.real, BI = B->xComplex.value.imag;
		switch( vmc->code ){
			case DVM_EQ: D = (AR == BR) && (AI == BI); break;
			case DVM_NE: D = (AR != BR) || (AI != BI); break;
			default: break;
		}
	}else if( A->type == DAO_LONG && B->type == DAO_LONG ){
		switch( vmc->code ){
			case DVM_AND: C = DLong_CompareToZero( A->xLong.value ) ? B : A; break;
			case DVM_OR:  C = DLong_CompareToZero( A->xLong.value ) ? A : B; break;
			case DVM_LT:  D = DLong_Compare( A->xLong.value, B->xLong.value )< 0; break;
			case DVM_LE:  D = DLong_Compare( A->xLong.value, B->xLong.value )<=0; break;
			case DVM_EQ:  D = DLong_Compare( A->xLong.value, B->xLong.value )==0; break;
			case DVM_NE:  D = DLong_Compare( A->xLong.value, B->xLong.value )!=0; break;
			default: break;
		}
	}else if( A->type == DAO_INTEGER && B->type == DAO_LONG ){
		switch( vmc->code ){
			case DVM_AND: C = A->xInteger.value ? B : A; break;
			case DVM_OR:  C = A->xInteger.value ? A : B; break;
			case DVM_LT:  D = DLong_CompareToInteger( B->xLong.value, A->xInteger.value )> 0; break;
			case DVM_LE:  D = DLong_CompareToInteger( B->xLong.value, A->xInteger.value )>=0; break;
			case DVM_EQ:  D = DLong_CompareToInteger( B->xLong.value, A->xInteger.value )==0; break;
			case DVM_NE:  D = DLong_CompareToInteger( B->xLong.value, A->xInteger.value )!=0; break;
			default: break;
		}
	}else if( A->type == DAO_LONG && B->type == DAO_INTEGER ){
		switch( vmc->code ){
			case DVM_AND: C = DLong_CompareToZero( A->xLong.value ) ? B : A; break;
			case DVM_OR:  C = DLong_CompareToZero( A->xLong.value ) ? A : B; break;
			case DVM_LT:  D = DLong_CompareToInteger( A->xLong.value, B->xInteger.value )< 0; break;
			case DVM_LE:  D = DLong_CompareToInteger( A->xLong.value, B->xInteger.value )<=0; break;
			case DVM_EQ:  D = DLong_CompareToInteger( A->xLong.value, B->xInteger.value )==0; break;
			case DVM_NE:  D = DLong_CompareToInteger( A->xLong.value, B->xInteger.value )!=0; break;
			default: break;
		}
	}else if( (A->type == DAO_FLOAT || A->type == DAO_DOUBLE) && B->type == DAO_LONG ){
		double va = DaoValue_GetDouble( A );
		switch( vmc->code ){
			case DVM_AND: C = va ? B : A; break;
			case DVM_OR:  C = va ? A : B; break;
			case DVM_LT:  D = DLong_CompareToDouble( B->xLong.value, va )> 0; break;
			case DVM_LE:  D = DLong_CompareToDouble( B->xLong.value, va )>=0; break;
			case DVM_EQ:  D = DLong_CompareToDouble( B->xLong.value, va )==0; break;
			case DVM_NE:  D = DLong_CompareToDouble( B->xLong.value, va )!=0; break;
			default: break;
		}
	}else if( A->type == DAO_LONG && (B->type == DAO_FLOAT || B->type == DAO_DOUBLE) ){
		double vb = DaoValue_GetDouble( B );
		switch( vmc->code ){
			case DVM_AND: C = DLong_CompareToZero( A->xLong.value ) ? B : A; break;
			case DVM_OR:  C = DLong_CompareToZero( A->xLong.value ) ? A : B; break;
			case DVM_LT:  D = DLong_CompareToDouble( A->xLong.value, vb )< 0; break;
			case DVM_LE:  D = DLong_CompareToDouble( A->xLong.value, vb )<=0; break;
			case DVM_EQ:  D = DLong_CompareToDouble( A->xLong.value, vb )==0; break;
			case DVM_NE:  D = DLong_CompareToDouble( A->xLong.value, vb )!=0; break;
			default: break;
		}
	}else if( A->type == DAO_STRING && B->type == DAO_STRING ){
		switch( vmc->code ){
			case DVM_AND: C = DString_Size( A->xString.data ) ? B : A; break;
			case DVM_OR:  C = DString_Size( A->xString.data ) ? A : B; break;
			case DVM_LT:  D = DString_Compare( A->xString.data, B->xString.data )<0; break;
			case DVM_LE:  D = DString_Compare( A->xString.data, B->xString.data )<=0; break;
			case DVM_EQ:  D = DString_Compare( A->xString.data, B->xString.data )==0; break;
			case DVM_NE:  D = DString_Compare( A->xString.data, B->xString.data )!=0; break;
			default: break;
		}
	}else if( (A->type == DAO_ENUM && B->type == DAO_ENUM)
			 || (A->type == DAO_TUPLE && B->type == DAO_TUPLE) ){
		switch( vmc->code ){
			case DVM_AND: C = DaoValue_GetInteger( A ) ? B : A; break;
			case DVM_OR:  C = DaoValue_GetInteger( A ) ? A : B; break;
			case DVM_LT:  D = DaoValue_Compare( A, B ) < 0; break;
			case DVM_LE:  D = DaoValue_Compare( A, B ) <= 0; break;
			case DVM_EQ:  D = DaoValue_Compare( A, B ) == 0; break;
			case DVM_NE:  D = DaoValue_Compare( A, B ) != 0; break;
			default: break;
		}
	}else if( vmc->code == DVM_AND || vmc->code == DVM_OR ){
		DaoValue *AA = A, *BB = B;
		if( vmc->code == DVM_OR ){ AA = B; BB = A; }
		switch( A->type ){
			case DAO_INTEGER : C = A->xInteger.value ? BB : AA; break;
			case DAO_FLOAT   : C = A->xFloat.value ? BB : AA; break;
			case DAO_DOUBLE  : C = A->xDouble.value ? BB : AA; break;
			case DAO_COMPLEX : C = A->xComplex.value.real && A->xComplex.value.imag ? BB : AA; break;
			case DAO_LONG : C = DLong_CompareToZero( A->xLong.value ) ? BB : AA; break;
			case DAO_STRING : C = DString_Size( A->xString.data ) ? BB : AA; break;
			case DAO_ENUM : C = A->xEnum.value ? BB : AA; break;
			case DAO_LIST : C = A->xList.items.size ? BB : AA; break;
			case DAO_MAP  : C = A->xMap.items->size ? BB : AA; break;
			case DAO_ARRAY : C = A->xArray.size ? BB : AA; break;
			default : C = A->type ? BB : AA; break;
		}
	}else{
		switch( vmc->code ){
			case DVM_LT: D = DaoValue_Compare( A, B )< 0; break;
			case DVM_LE: D = DaoValue_Compare( A, B )<=0; break;
			case DVM_EQ: D = DaoValue_Compare( A, B )==0; break; /*XXX numarray*/
			case DVM_NE: D = DaoValue_Compare( A, B )!=0; break;
			default: break;
		}
	}
	if( C ) DaoProcess_PutValue( self, C );
	else DaoProcess_PutInteger( self, D );
}
void DaoProcess_DoUnaArith( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *A = self->activeValues[ vmc->a ];
	DaoValue *C = NULL;
	int ta = A->type;
	self->activeCode = vmc;
	if( A->type ==0 ){
		DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "on none object" );
		return;
	}
	
	if( ta == DAO_INTEGER ){
		if( self->activeTypes[ vmc->c ] ) printf( "%s\n", self->activeTypes[ vmc->c ]->name->mbs );
		C = DaoProcess_SetValue( self, vmc->c, A );
		switch( vmc->code ){
			case DVM_NOT :  C->xInteger.value = ! C->xInteger.value; break;
			case DVM_UNMS : C->xInteger.value = - C->xInteger.value; break;
			default: break;
		}
	}else if( ta == DAO_FLOAT ){
		C = DaoProcess_SetValue( self, vmc->c, A );
		switch( vmc->code ){
			case DVM_NOT :  C->xFloat.value = ! C->xFloat.value; break;
			case DVM_UNMS : C->xFloat.value = - C->xFloat.value; break;
			default: break;
		}
	}else if( ta == DAO_DOUBLE ){
		C = DaoProcess_SetValue( self, vmc->c, A );
		switch( vmc->code ){
			case DVM_NOT :  C->xDouble.value = ! C->xDouble.value; break;
			case DVM_UNMS : C->xDouble.value = - C->xDouble.value; break;
			default: break;
		}
	}else if( ta == DAO_COMPLEX ){
		if( vmc->code == DVM_UNMS ){
			C = DaoProcess_SetValue( self, vmc->c, A );
			C->xComplex.value.real = - C->xComplex.value.real;
			C->xComplex.value.imag = - C->xComplex.value.imag;
		}
	}else if( ta == DAO_LONG ){
		C = DaoProcess_SetValue( self, vmc->c, A );
		switch( vmc->code ){
			case DVM_NOT  :
				ta = DLong_CompareToZero( C->xLong.value ) == 0;
				DLong_FromInteger( C->xLong.value, ta );
				break;
			case DVM_UNMS : C->xLong.value->sign = - C->xLong.value->sign; break;
			default: break;
		}
#ifdef DAO_WITH_NUMARRAY
	}else if( ta == DAO_ARRAY ){
		DaoArray *array = & A->xArray;
		size_t i;
		C = A;
		if( array->etype == DAO_FLOAT ){
			DaoArray *res = DaoProcess_GetArray( self, vmc );
			res->etype = array->etype;
			DaoArray_ResizeArray( res, array->dims, array->ndim );
			if( array->etype == DAO_FLOAT ){
				float *va = array->data.f;
				float *vc = res->data.f;
				if( vmc->code == DVM_NOT ){
					for(i=0; i<array->size; i++ ) vc[i] = (float) ! va[i];
				}else{
					for(i=0; i<array->size; i++ ) vc[i] = - va[i];
				}
			}else{
				double *va = array->data.d;
				double *vc = res->data.d;
				if( vmc->code == DVM_NOT ){
					for(i=0; i<array->size; i++ ) vc[i] = ! va[i];
				}else{
					for(i=0; i<array->size; i++ ) vc[i] = - va[i];
				}
			}
		}else if( vmc->code == DVM_UNMS ){
			DaoArray *res = DaoProcess_GetArray( self, vmc );
			complex16 *va, *vc;
			res->etype = array->etype;
			DaoArray_ResizeArray( res, array->dims, array->ndim );
			va = array->data.c;
			vc = res->data.c;
			for(i=0; i<array->size; i++ ){
				vc[i].real = - va[i].real;
				vc[i].imag = - va[i].imag;
			}
		}
#endif
	}else if( ta == DAO_OBJECT ){
		C = self->activeValues[ vmc->c ];
		DaoProcess_TryObjectArith( self, A, NULL, C );
		return;
	}else if( ta == DAO_CDATA ){
		C = self->activeValues[ vmc->c ];
		DaoProcess_TryCdataArith( self, A, NULL, C );
		return;
	}
	if( C ==0 ) DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "" );
}
void DaoProcess_DoInTest( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *A = self->activeValues[ vmc->a ];
	DaoValue *B = self->activeValues[ vmc->b ];
	dint *C = DaoProcess_PutInteger( self, 0 );
	int i;
	if( A->type == DAO_INTEGER && B->type == DAO_STRING ){
		dint bv = A->xInteger.value;
		int size = B->xString.data->size;
		if( B->xString.data->mbs ){
			char *mbs = B->xString.data->mbs;
			for(i=0; i<size; i++){
				if( mbs[i] == bv ){
					*C = 1;
					break;
				}
			}
		}else{
			wchar_t *wcs = B->xString.data->wcs;
			for(i=0; i<size; i++){
				if( wcs[i] == bv ){
					*C = 1;
					break;
				}
			}
		}
	}else if( A->type == DAO_STRING && B->type == DAO_STRING ){
		*C = DString_Find( B->xString.data, A->xString.data, 0 ) != MAXSIZE;
	}else if( A->type == DAO_ENUM && B->type == DAO_ENUM ){
		DaoType *ta = A->xEnum.etype;
		DaoType *tb = B->xEnum.etype;
		if( ta == tb ){
			*C = A->xEnum.value == (A->xEnum.value & B->xEnum.value);
		}else{
			DMap *ma = ta->mapNames;
			DMap *mb = tb->mapNames;
			DNode *it, *node;
			*C = 1;
			for(it=DMap_First(ma); it; it=DMap_Next(ma,it) ){
				if( ta->flagtype ){
					if( !(it->value.pInt & A->xEnum.value) ) continue;
				}else if( it->value.pInt != A->xEnum.value ){
					continue;
				}
				if( (node = DMap_Find( mb, it->key.pVoid )) == NULL ){
					*C = 0;
					break;
				}
				if( !(node->value.pInt & B->xEnum.value) ){
					*C = 0;
					break;
				}
			}
		}
	}else if( B->type == DAO_LIST ){
		DArray *items = & B->xList.items;
		DaoType *ta = DaoNamespace_GetType( self->activeNamespace, A );
		if( ta && B->xList.unitype && B->xList.unitype->nested->size ){
			DaoType *tb = B->xList.unitype->nested->items.pType[0];
			if( tb && DaoType_MatchTo( ta, tb, NULL ) < DAO_MT_SUB	 ) return;
		}
		for(i=0; i<items->size; i++){
			*C = DaoValue_Compare( A, items->items.pValue[i] ) ==0;
			if( *C ) break;
		}
	}else if( B->type == DAO_MAP ){
		DaoType *ta = DaoNamespace_GetType( self->activeNamespace, A );
		if( ta && B->xMap.unitype && B->xMap.unitype->nested->size ){
			DaoType *tb = B->xMap.unitype->nested->items.pType[0];
			if( tb && DaoType_MatchTo( ta, tb, NULL ) < DAO_MT_SUB	 ) return;
		}
		*C = DMap_Find( B->xMap.items, & A ) != NULL;
	}else if( B->type == DAO_TUPLE && B->xTuple.subtype == DAO_PAIR ){
		int c1 = DaoValue_Compare( B->xTuple.items[0], A );
		int c2 = DaoValue_Compare( A, B->xTuple.items[1] );
		*C = c1 <=0 && c2 <= 0;
	}else{
		DaoProcess_RaiseException( self, DAO_ERROR_TYPE, "" );
	}
}
void DaoProcess_DoBitLogic( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *A = self->activeValues[ vmc->a ];
	DaoValue *B = self->activeValues[ vmc->b ];
	ulong_t inum = 0;
	
	self->activeCode = vmc;
	if( A->type && B->type && A->type <= DAO_DOUBLE && B->type <= DAO_DOUBLE ){
		switch( vmc->code ){
			case DVM_BITAND: inum =DaoValue_GetInteger(A) & DaoValue_GetInteger(B);break;
			case DVM_BITOR: inum =DaoValue_GetInteger(A) | DaoValue_GetInteger(B);break;
			case DVM_BITXOR: inum =DaoValue_GetInteger(A) ^ DaoValue_GetInteger(B);break;
			default : break;
		}
		if( A->type == DAO_DOUBLE || B->type == DAO_DOUBLE ){
			DaoProcess_PutDouble( self, inum );
		}else if( A->type == DAO_FLOAT || B->type == DAO_FLOAT ){
			DaoProcess_PutFloat( self, inum );
		}else{
			DaoProcess_PutInteger( self, inum );
		}
	}else if( A->type == DAO_LONG && B->type >= DAO_INTEGER && B->type <= DAO_DOUBLE ){
		DLong *bigint = DaoProcess_PutLong( self );
		DLong_FromInteger( bigint, DaoValue_GetInteger( B ) );
		switch( vmc->code ){
			case DVM_BITAND : DLong_BitAND( bigint, A->xLong.value, bigint ); break;
			case DVM_BITOR :  DLong_BitOR( bigint, A->xLong.value, bigint ); break;
			case DVM_BITXOR : DLong_BitXOR( bigint, A->xLong.value, bigint ); break;
			default : break;
		}
	}else if( B->type == DAO_LONG && A->type >= DAO_INTEGER && A->type <= DAO_DOUBLE ){
		DLong *bigint = DaoProcess_PutLong( self );
		DLong_FromInteger( bigint, DaoValue_GetInteger( A ) );
		switch( vmc->code ){
			case DVM_BITAND : DLong_BitAND( bigint, B->xLong.value, bigint ); break;
			case DVM_BITOR :  DLong_BitOR( bigint, B->xLong.value, bigint ); break;
			case DVM_BITXOR : DLong_BitXOR( bigint, B->xLong.value, bigint ); break;
			default : break;
		}
	}else if( A->type ==DAO_LONG && B->type == DAO_LONG ){
		DLong *bigint = DaoProcess_PutLong( self );
		switch( vmc->code ){
			case DVM_BITAND : DLong_BitAND( bigint, A->xLong.value, B->xLong.value ); break;
			case DVM_BITOR :  DLong_BitOR( bigint, A->xLong.value, B->xLong.value ); break;
			case DVM_BITXOR : DLong_BitXOR( bigint, A->xLong.value, B->xLong.value ); break;
			default : break;
		}
	}else{
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "invalid operands" );
	}
}
void DaoProcess_DoBitShift( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *A = self->activeValues[ vmc->a ];
	DaoValue *B = self->activeValues[ vmc->b ];
	if( A->type && B->type && A->type <= DAO_DOUBLE && B->type <= DAO_DOUBLE ){
		long_t inum = 0;
		if( vmc->code == DVM_BITLFT ){
			inum = DaoValue_GetInteger(A) << DaoValue_GetInteger(B);
		}else{
			inum = DaoValue_GetInteger(A) >> DaoValue_GetInteger(B);
		}
		if( A->type == DAO_DOUBLE || B->type == DAO_DOUBLE ){
			DaoProcess_PutDouble( self, inum );
		}else if( A->type == DAO_FLOAT || B->type == DAO_FLOAT ){
			DaoProcess_PutFloat( self, inum );
		}else{
			DaoProcess_PutInteger( self, inum );
		}
	}else if( A->type ==DAO_LONG && B->type >=DAO_INTEGER && B->type <= DAO_DOUBLE ){
		if( vmc->a == vmc->c ){
			if( vmc->code == DVM_BITLFT ){
				DLong_ShiftLeft( A->xLong.value, DaoValue_GetInteger( B ) );
			}else{
				DLong_ShiftRight( A->xLong.value, DaoValue_GetInteger( B ) );
			}
		}else{
			DLong *bigint = DaoProcess_PutLong( self );
			DLong_Move( bigint, A->xLong.value );
			if( vmc->code == DVM_BITLFT ){
				DLong_ShiftLeft( bigint, DaoValue_GetInteger( B ) );
			}else{
				DLong_ShiftRight( bigint, DaoValue_GetInteger( B ) );
			}
		}
	}else if( A->type ==DAO_LIST && (vmc->code ==DVM_BITLFT || vmc->code ==DVM_BITRIT) ){
		DaoList *list = & self->activeValues[ vmc->a ]->xList;
		self->activeCode = vmc;
		if( DaoProcess_SetValue( self, vmc->c, A ) ==0 ) return;
		if( vmc->code ==DVM_BITLFT ){
			DaoType *abtp = list->unitype;
			if( abtp && abtp->nested->size ){
				abtp = abtp->nested->items.pType[0];
				if( DaoType_MatchValue( abtp, B, NULL ) ==0 ) return; /* XXX information */
			}
			DArray_PushBack( & list->items, B );
		}else{
			if( list->items.size ==0 ) return; /* XXX information */
			B = list->items.items.pValue[list->items.size-1];
			if( DaoProcess_SetValue( self, vmc->b, B ) ==0 ) return;
			DArray_PopBack( & list->items );
		}
	}else{
		self->activeCode = vmc;
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "invalid operands" );
	}
}
void DaoProcess_DoBitFlip( DaoProcess *self, DaoVmCode *vmc )
{
	DaoValue *A = self->activeValues[ vmc->a ];
	self->activeCode = vmc;
	if( A->type >= DAO_INTEGER && A->type <= DAO_DOUBLE ){
		switch( A->type ){
			case DAO_INTEGER : DaoProcess_PutInteger( self, A->xInteger.value ); break;
			case DAO_FLOAT   : DaoProcess_PutFloat( self, ~(long_t)A->xFloat.value ); break;
			case DAO_DOUBLE  : DaoProcess_PutDouble( self, ~(long_t)A->xDouble.value ); break;
		}
	}else if( A->type == DAO_LONG ){
		DLong *bigint = DaoProcess_PutLong( self );
		DLong_Move( bigint, A->xLong.value );
		DLong_Flip( bigint );
	}else{
		DaoProcess_RaiseException( self, DAO_ERROR_VALUE, "invalid operands" );
	}
}
static int DaoValue_CheckTypeShape( DaoValue *self, int type,
								   DArray *shape, unsigned int depth, int check_size )
{
	DaoTuple *tuple;
	DaoArray *array;
	DaoList *list;
	DaoValue **data;
	size_t i, j;
	if( self->type ==0 ) return -1;
	switch( self->type ){
		case DAO_INTEGER :
		case DAO_FLOAT :
		case DAO_DOUBLE :
		case DAO_COMPLEX :
			if( type <= self->type ) type = self->type;
			break;
		case DAO_STRING :
			type = type == DAO_UDF || type == self->type ? self->type : -1;
			break;
		case DAO_ARRAY :
#ifdef DAO_WITH_NUMARRAY
			array = & self->xArray;
			if( type < array->etype ) type = array->etype;
			if( shape->size <= depth ){
				for(i=0; i<array->ndim; i++) DArray_Append( shape, array->dims[i] );
			}else if( check_size ){
				for(i=0; i<array->ndim; i++){
					if( shape->items.pSize[i+depth] != array->dims[i] ) return -1;
				}
			}
#endif
			break;
		case DAO_LIST :
			list = & self->xList;
			if( shape->size <= depth ) DArray_Append( shape, (void*)list->items.size );
			if( check_size && list->items.size != shape->items.pSize[depth] ) return -1;
			depth ++;
			if( list->unitype && list->unitype->nested->size ){
				j = list->unitype->nested->items.pType[0]->tid;
				if( j <= DAO_COMPLEX ){
					if( type <= j ) type = j;
				}else if( j == DAO_STRING ){
					type = type == DAO_UDF || type == j ? j : -1;
					break;
				}
			}
			data = list->items.items.pValue;
			for(i=0; i<list->items.size; i++ ){
				type = DaoValue_CheckTypeShape( data[i], type, shape, depth, check_size );
				if( type < 0 ) break;
			}
			break;
		case DAO_TUPLE :
			tuple = & self->xTuple;
			if( shape->size <= depth ) DArray_Append( shape, (void*)(size_t)tuple->size );
			if( check_size && tuple->size != shape->items.pSize[depth] ) return -1;
			depth ++;
			data = tuple->items;
			for(i=0; i<tuple->size; i++){
				type = DaoValue_CheckTypeShape( data[i], type, shape, depth, check_size );
				if( type < 0 ) break;
			}
			break;
		default : break;
	}
	return type;
}
#ifdef DAO_WITH_NUMARRAY
static int DaoValue_ExportValue( DaoValue *self, DaoArray *array, int k )
{
	DaoTuple *tup;
	DaoArray *sub;
	DaoList *list;
	DaoValue **items;
	size_t i;
	if( k >= array->size ) return k;
	switch( self->type ){
		case DAO_INTEGER :
		case DAO_FLOAT :
		case DAO_DOUBLE :
		case DAO_LONG :
		case DAO_STRING :
			switch( array->etype ){
				case DAO_INTEGER : array->data.i[k] = DaoValue_GetInteger( self ); break;
				case DAO_FLOAT : array->data.f[k] = DaoValue_GetFloat( self ); break;
				case DAO_DOUBLE : array->data.d[k] = DaoValue_GetDouble( self ); break;
				case DAO_COMPLEX : array->data.c[k].real = DaoValue_GetDouble( self ); break;
				default : break;
			}
			k ++;
			break;
		case DAO_COMPLEX :
			switch( array->etype ){
				case DAO_INTEGER : array->data.i[k] = self->xComplex.value.real; break;
				case DAO_FLOAT : array->data.f[k] = self->xComplex.value.real; break;
				case DAO_DOUBLE : array->data.d[k] = self->xComplex.value.real; break;
				case DAO_COMPLEX : array->data.c[k] = self->xComplex.value; break;
				default : break;
			}
			k ++;
			break;
		case DAO_ARRAY :
			sub = & self->xArray;
			for(i=0; i<sub->size; i++){
				switch( array->etype ){
					case DAO_INTEGER : array->data.i[k+i] = DaoArray_GetInteger( sub, i ); break;
					case DAO_FLOAT : array->data.f[k+i] = DaoArray_GetFloat( sub, i ); break;
					case DAO_DOUBLE : array->data.d[k+i] = DaoArray_GetDouble( sub, i ); break;
					case DAO_COMPLEX : array->data.c[k+i] = DaoArray_GetComplex( sub, i ); break;
					default : break;
				}
			}
			k += sub->size;
			break;
		case DAO_LIST :
			list = & self->xList;
			items = list->items.items.pValue;
			for(i=0; i<list->items.size; i++ ) k = DaoValue_ExportValue( items[i], array, k );
			break;
		case DAO_TUPLE :
			tup = & self->xTuple;
			items = tup->items;
			for(i=0; i<tup->size; i++) k = DaoValue_ExportValue( items[i], array, k );
			break;
		default : break;
	}
	return k;
}
static void DaoArray_ToWCString( DaoArray *self, DString *str, int offset, int size )
{
	size_t i;
	int type = 1; /*MBS*/
	DString_ToWCS( str );
	DString_Resize( str, size * ( (self->etype == DAO_COMPLEX) +1 ) );
	for(i=0; i<size; i++){
		switch( self->etype ){
			case DAO_INTEGER :
				str->wcs[i] = self->data.i[offset+i];
				if( str->wcs[i] > 255 ) type = 0;
				break;
			case DAO_FLOAT :
				str->wcs[i] = self->data.f[offset+i];
				if( str->wcs[i] > 255 ) type = 0;
				break;
			case DAO_DOUBLE :
				str->wcs[i] = self->data.d[offset+i];
				if( str->wcs[i] > 255 ) type = 0;
				break;
			case DAO_COMPLEX :
				str->wcs[2*i] = self->data.c[offset+i].real;
				str->wcs[2*i+1] = self->data.c[offset+i].imag;
				if( str->wcs[2*i] > 255 ) type = 0;
				if( str->wcs[2*i+1] > 255 ) type = 0;
				break;
			default : break;
		}
	}
	if( type ) DString_ToMBS( str );
}
static int DaoArray_ToList( DaoArray *self, DaoList *list, DaoType *abtp,
						   int dim, int offset )
{
	DaoComplex tmp = {DAO_COMPLEX,0,0,0,0,{0.0,0.0}};
	DaoValue *value = (DaoValue*) & tmp;
	DaoList *ls;
	size_t *ds = self->dims;
	size_t i, size, isvector = self->ndim==2 && (ds[0] ==1 || ds[1] ==1);
	
	if( abtp == NULL ) return 0;
	abtp = abtp->nested->items.pType[0];
	if( abtp == NULL ) return 0;
	if( isvector ) dim = ds[0] ==1 ? 1 : 0;
	if( dim >= self->ndim-1 || isvector ){
		if( abtp->tid == DAO_INTEGER ){
			value->type = DAO_INTEGER;
			switch( self->etype ){
				case DAO_INTEGER :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xInteger.value = self->data.i[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				case DAO_FLOAT :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xInteger.value = self->data.f[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				case DAO_DOUBLE :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xInteger.value = self->data.d[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				default : break;
			}
		}else if( abtp->tid == DAO_FLOAT ){
			value->type = DAO_FLOAT;
			switch( self->etype ){
				case DAO_INTEGER :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xFloat.value = self->data.i[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				case DAO_FLOAT :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xFloat.value = self->data.f[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				case DAO_DOUBLE :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xFloat.value = self->data.d[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				default : break;
			}
		}else if( abtp->tid == DAO_DOUBLE ){
			value->type = DAO_DOUBLE;
			switch( self->etype ){
				case DAO_INTEGER :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xDouble.value = self->data.i[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				case DAO_FLOAT :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xDouble.value = self->data.f[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				case DAO_DOUBLE :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xDouble.value = self->data.d[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				default : break;
			}
		}else if( abtp->tid == DAO_COMPLEX ){
			value->type = DAO_COMPLEX;
			value->xComplex.value.imag = 0.0;
			switch( self->etype ){
				case DAO_INTEGER :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xComplex.value.real = self->data.i[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				case DAO_FLOAT :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xComplex.value.real = self->data.f[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				case DAO_DOUBLE :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xComplex.value.real = self->data.d[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				case DAO_COMPLEX :
					for( i=0; i<self->dims[dim]; i++ ){
						value->xComplex.value = self->data.c[ offset+i ];
						DaoList_Append( list, value );
					}
					break;
				default : break;
			}
		}else{
			return 0;
		}
	}else{
		if( abtp->tid == DAO_STRING ){
			value->type = DAO_STRING;
			value->xString.data = DString_New(1);
			size = self->dims[self->ndim + dim]; /* products of sizes */
			for(i=0; i<self->dims[dim]; i++){
				DaoArray_ToWCString( self, value->xString.data, offset, size );
				DaoList_Append( list, value );
				offset += self->dims[self->ndim + dim];
			}
			DString_Delete( value->xString.data );
		}else if( abtp->tid == DAO_LIST ){
			for(i=0; i<self->dims[dim]; i++){
				ls = DaoList_New();
				ls->unitype = abtp;
				GC_IncRC( abtp );
				DaoList_Append( list, (DaoValue*) ls );
				DaoArray_ToList( self, ls, abtp, dim+1, offset );
				offset += self->dims[self->ndim + dim];
			}
		}
	}
	return 1;
}
int DaoArray_FromList( DaoArray *self, DaoList *list, DaoType *tp )
{
	DArray *shape = DArray_New(0);
	int type = DaoValue_CheckTypeShape( (DaoValue*) list, DAO_UDF, shape, 0, 1 );
	if( type < 0 ) goto FailConversion;
	self->etype = type;
	if( tp && tp->tid && tp->tid <= DAO_COMPLEX ) self->etype = tp->tid;
	DaoArray_ResizeArray( self, shape->items.pSize, shape->size );
	DaoValue_ExportValue( (DaoValue*) list, self, 0 );
	DArray_Delete( shape );
	return 1;
FailConversion:
	DArray_Delete( shape );
	return 0;
}
#endif
/* Set dC->type before calling to instruct this function what type number to convert: */
int ConvertStringToNumber( DaoProcess *proc, DaoValue *dA, DaoValue *dC )
{
	DString *mbs = proc->mbstring;
	double d1 = 0.0, d2 = 0.0;
	int set1 = 0, set2 = 0;
	int toktype, toklen = 0;
	int tid = dC->type;
	int imagfirst = 0;
	int ec, sign = 1;
	if( dA->type != DAO_STRING || tid ==0 || tid > DAO_LONG ) return 0;
	if( dA->xString.data->mbs ){
		DString_SetDataMBS( mbs, dA->xString.data->mbs, dA->xString.data->size );
	}else{
		DString_SetDataWCS( mbs, dA->xString.data->wcs, dA->xString.data->size );
	}
	DString_Trim( mbs );
	if( mbs->size ==0 ) return 0;
	toktype = DaoToken_Check( mbs->mbs, mbs->size, & toklen );
	if( toktype == DTOK_ADD || toktype == DTOK_SUB ){
		if( toktype == DTOK_SUB ) sign = -1;
		DString_Erase( mbs, 0, toklen );
		toktype = DaoToken_Check( mbs->mbs, mbs->size, & toklen );
	}
	if( tid != DAO_COMPLEX ){
		if( toklen != mbs->size ) return 0;
		if( toktype < DTOK_DIGITS_HEX || toktype > DTOK_NUMBER_SCI ) return 0;
		if( tid == DAO_INTEGER ){
			dC->xInteger.value = (sizeof(dint) == 4) ? strtol( mbs->mbs, 0, 0 ) : strtoll( mbs->mbs, 0, 0 );
			if( sign <0 ) dC->xInteger.value = - dC->xInteger.value;
		}else if( tid == DAO_FLOAT ){
			dC->xFloat.value = strtod( mbs->mbs, 0 );
			if( sign <0 ) dC->xFloat.value = - dC->xFloat.value;
		}else if( tid == DAO_DOUBLE ){
			dC->xDouble.value = strtod( mbs->mbs, 0 );
			if( sign <0 ) dC->xDouble.value = - dC->xDouble.value;
		}else{ /* DAO_LONG */
			ec = DLong_FromString( dC->xLong.value, mbs );
			if( ec ){
				const char *msg = ec == 'L' ? "invalid radix" : "invalid digit";
				DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, msg );
				return 0;
			}
			dC->xLong.value->sign = sign;
		}
		return 1;
	}
	dC->xComplex.value.real = dC->xComplex.value.imag = 0.0;
	if( toktype >= DTOK_DIGITS_HEX && toktype <= DTOK_NUMBER_SCI ){
		set1 = 1;
		d1 = strtod( mbs->mbs, 0 );
		DString_Erase( mbs, 0, toklen );
		toktype = DaoToken_Check( mbs->mbs, mbs->size, & toklen );
	}
	if( toktype == DTOK_DOLLAR ){
		imagfirst = 1;
		if( set1 ==0 ) d1 = 1.0;
		DString_Erase( mbs, 0, toklen );
		toktype = DaoToken_Check( mbs->mbs, mbs->size, & toklen );
	}
	if( sign <0 ) d1 = - d1;
	if( imagfirst ) dC->xComplex.value.imag = d1; else dC->xComplex.value.real = d1;
	if( mbs->size ==0 ) return 1;
	if( toktype != DTOK_ADD && toktype != DTOK_SUB ) return 0;
	if( toklen == mbs->size ) return 0;
	sign = toktype == DTOK_ADD ? 1 : -1;
	DString_Erase( mbs, 0, toklen );
	toktype = DaoToken_Check( mbs->mbs, mbs->size, & toklen );
	if( imagfirst && toktype == DTOK_DOLLAR ) return 0;
	if( toktype >= DTOK_DIGITS_HEX && toktype <= DTOK_NUMBER_SCI ){
		set2 = 1;
		d2 = strtod( mbs->mbs, 0 );
		DString_Erase( mbs, 0, toklen );
		toktype = DaoToken_Check( mbs->mbs, mbs->size, & toklen );
	}
	if( imagfirst && toktype == DTOK_DOLLAR ) return 0;
	if( imagfirst ==0 && toktype != DTOK_DOLLAR ) return 0;
	if( toktype == DTOK_DOLLAR && set2 ==0 ) d2 = 1.0;
	if( sign <0 ) d2 = - d2;
	if( imagfirst ) dC->xComplex.value.real = d2; else dC->xComplex.value.imag = d2;
	return toklen == mbs->size;
}
DaoValue* DaoTypeCast( DaoProcess *proc, DaoType *ct, DaoValue *dA, DaoValue *dC, CastBuffer *b1, CastBuffer *b2 )
{
	DaoNamespace *ns = proc->activeNamespace;
	DaoTuple *tuple = NULL;
	DaoList *list = NULL, *list2 = NULL;
	DaoMap *map = NULL, *map2 = NULL;
	DaoType *tp = NULL, *tp2 = NULL;
	DaoArray *array = NULL, *array2 = NULL;
	DaoValue **data, **data2, *K, *V, key, value;
	DString *str, *wcs = NULL;
	DArray *shape = NULL;
	DNode *node;
	int i, type, size;
	if( ct == NULL ) goto FailConversion;
	memset( & key, 0, sizeof(DaoValue) );
	memset( & value, 0, sizeof(DaoValue) );
	key.xNone.trait = value.xNone.trait = DAO_DATA_CONST;
	dC->type = ct->tid;
	if( ct->tid == DAO_ANY ) goto Rebind;
	if( dA->type == ct->tid && ct->tid >= DAO_INTEGER && ct->tid < DAO_ARRAY ) goto Rebind;
	if( dA->type == DAO_STRING && ct->tid > 0 && ct->tid <= DAO_LONG ){
		if( dC->type == DAO_LONG ){
			if( b1->lng == NULL ) b1->lng = DLong_New();
			dC->xLong.value = b1->lng;
		}
		if( ConvertStringToNumber( proc, dA, dC ) ==0 ) goto FailConversion;
		return dC;
	}
	switch( ct->tid ){
		case DAO_INTEGER :
			dC->xInteger.value = DaoValue_GetInteger( dA );
			break;
		case DAO_FLOAT :
			dC->xFloat.value = DaoValue_GetFloat( dA );
			break;
		case DAO_DOUBLE :
			dC->xDouble.value = DaoValue_GetDouble( dA );
			break;
		case DAO_COMPLEX :
			if( dA->type == DAO_COMPLEX ) goto Rebind;
			if( dA->type >= DAO_ARRAY ) goto FailConversion;
			/* do not allocate complex here,
			 * the caller should not be responsible to free it.
			 * the same for string. */
			dC->xComplex.value = DaoValue_GetComplex( dA );
			break;
		case DAO_LONG :
			if( dA->type == DAO_LONG ) goto Rebind;
			if( dA->type >= DAO_ARRAY ) goto FailConversion;
			if( b1->lng == NULL ) b1->lng = DLong_New();
			dC->xLong.value = b1->lng;
			switch( dA->type ){
				case DAO_INTEGER :
					DLong_FromInteger( dC->xLong.value, DaoValue_GetInteger( dA ) );
					break;
				case DAO_FLOAT :
				case DAO_DOUBLE :
					DLong_FromDouble( dC->xLong.value, DaoValue_GetDouble( dA ) );
					break;
				case DAO_STRING :
					DLong_FromString( dC->xLong.value, dA->xString.data );
					break;
				default : break;
			}
			dC->type = DAO_LONG;
			break;
		case DAO_STRING :
			if( dA->type == DAO_STRING ) goto Rebind;
			if( b1->str == NULL ) b1->str = DString_New(1);
			dC->xString.data = b1->str;
			str = dC->xString.data;
			wcs = DString_New(0);
			if( dA->type < DAO_ARRAY ){
				DaoValue_GetString( dA, str );
#ifdef DAO_WITH_NUMARRAY
			}else if( dA->type == DAO_ARRAY ){
				array2 = & dA->xArray;
				DaoArray_ToWCString( array2, str, 0, array2->size );
#endif
			}else if( dA->type == DAO_LIST ){
				shape = DArray_New(0);
				type = DaoValue_CheckTypeShape( dA, DAO_UDF, shape, 0, 1 );
				if( type <=0 || type >= DAO_COMPLEX ||shape->size >1 ) goto FailConversion;
				list = & dA->xList;
				DString_Resize( wcs, list->items.size );
				type = 1; /*MBS*/
				for(i=0; i<list->items.size; i++){
					wcs->wcs[i] = DaoValue_GetInteger( list->items.items.pValue[i] );
					if( wcs->wcs[i] > 255 ) type = 0;
				}
				
				if( type ){
					DString_ToMBS( str );
					DString_Resize( str, wcs->size );
					for(i=0; i<wcs->size; i++) str->mbs[i] = wcs->wcs[i];
				}else{
					DString_Assign( str, wcs );
				}
			}else{
				goto FailConversion;
			}
			break;
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			if( ct->nested->size >0 ) tp = ct->nested->items.pType[0];
			if( dA->type == DAO_STRING ){
				str = dA->xString.data;
				if( tp->tid < DAO_INTEGER || tp->tid > DAO_DOUBLE ) goto FailConversion;
				array = DaoArray_New( DAO_INTEGER + ( tp->tid - DAO_INTEGER ) );
				if( str->mbs ){
					DaoArray_ResizeVector( array, str->size );
					for(i=0; i<str->size; i++){
						switch( tp->tid ){
							case DAO_INTEGER : array->data.i[i] = str->mbs[i]; break;
							case DAO_FLOAT   : array->data.f[i]  = str->mbs[i]; break;
							case DAO_DOUBLE  : array->data.d[i]  = str->mbs[i]; break;
							default : break;
						}
					}
				}else{
					DaoArray_ResizeVector( array, str->size );
					for(i=0; i<str->size; i++){
						switch( tp->tid ){
							case DAO_INTEGER : array->data.i[i] = str->wcs[i]; break;
							case DAO_FLOAT   : array->data.f[i]  = str->wcs[i]; break;
							case DAO_DOUBLE  : array->data.d[i]  = str->wcs[i]; break;
							default : break;
						}
					}
				}
			}else if( dA->type == DAO_ARRAY ){
				if( tp == NULL ) goto Rebind;
				if( tp->tid == DAO_UDF || tp->tid == DAO_ANY || tp->tid == DAO_INITYPE ) goto Rebind;
				if( array2->etype == tp->tid ) goto Rebind;
				if( tp->tid < DAO_INTEGER || tp->tid > DAO_COMPLEX ) goto FailConversion;
				array2 = & dA->xArray;
				size = array2->size;
				array = DaoArray_New( tp->tid );
				if( array2->etype == DAO_DOUBLE && tp->tid == DAO_COMPLEX )
					array->etype = DAO_COMPLEX;
				DaoArray_ResizeArray( array, array2->dims, array2->ndim );
				if( array2->etype == DAO_INTEGER ){
					if( tp->tid == DAO_FLOAT ){
						for( i=0; i<size; i++ ) array->data.f[i] = array2->data.i[i];
					}else if( tp->tid == DAO_DOUBLE ){
						for( i=0; i<size; i++ ) array->data.d[i] = array2->data.i[i];
					}else if( tp->tid == DAO_COMPLEX ){
						for( i=0; i<size; i++ ) array->data.c[i].real = array2->data.i[i];
					}
				}else if( array2->etype == DAO_FLOAT ){
					if( tp->tid == DAO_INTEGER ){
						for( i=0; i<size; i++ ) array->data.i[i] = (int)array2->data.f[i];
					}else if( tp->tid == DAO_DOUBLE ){
						for( i=0; i<size; i++ ) array->data.d[i] = array2->data.f[i];
					}else if( tp->tid == DAO_COMPLEX ){
						for( i=0; i<size; i++ ) array->data.c[i].real = array2->data.f[i];
					}
				}else if( array2->etype ==  DAO_DOUBLE ){
					if( tp->tid == DAO_INTEGER ){
						for( i=0; i<size; i++ ) array->data.i[i] = (int)array2->data.d[i];
					}else if( tp->tid == DAO_FLOAT ){
						for( i=0; i<size; i++ ) array->data.f[i] = array2->data.d[i];
					}else if( tp->tid == DAO_COMPLEX ){
						for( i=0; i<size; i++ ) array->data.c[i].real = array2->data.d[i];
					}
				}else{
					goto FailConversion;
				}
			}else if( dA->type == DAO_LIST ){
				if( tp == NULL ) goto FailConversion;
				if( tp->tid ==0 || tp->tid > DAO_COMPLEX ) goto FailConversion;
				shape = DArray_New(0);
				type = DaoValue_CheckTypeShape( dA, DAO_UDF, shape, 0, 1 );
				if( type <0 ) goto FailConversion;
				array = DaoArray_New( DAO_INTEGER );
				array->etype = tp->tid;
				DaoArray_ResizeArray( array, shape->items.pSize, shape->size );
				DaoValue_ExportValue( dA, array, 0 );
			}else goto FailConversion;
			dC = (DaoValue*) array;
			array->unitype = ct;
			GC_IncRC( ct );
			break;
#endif
		case DAO_LIST :
			list = DaoList_New();
			list->unitype = ct;
			GC_IncRC( ct );
			dC = (DaoValue*) list;
			if( ct->nested->size >0 ) tp = ct->nested->items.pType[0];
			if( tp == NULL ) goto FailConversion;
			value.type = tp->tid;
			value.xDouble.value = 0.0;
			if( dA->type == DAO_STRING ){
				str = dA->xString.data;
				if( tp->tid < DAO_INTEGER || tp->tid > DAO_DOUBLE ) goto FailConversion;
				DArray_Resize( & list->items, DString_Size( str ), & value );
				data = list->items.items.pValue;
				if( str->mbs ){
					for(i=0; i<str->size; i++){
						switch( tp->tid ){
							case DAO_INTEGER : data[i]->xInteger.value = str->mbs[i]; break;
							case DAO_FLOAT   : data[i]->xFloat.value = str->mbs[i]; break;
							case DAO_DOUBLE  : data[i]->xDouble.value = str->mbs[i]; break;
							default : break;
						}
					}
				}else{
					for(i=0; i<str->size; i++){
						switch( tp->tid ){
							case DAO_INTEGER : data[i]->xInteger.value = str->wcs[i]; break;
							case DAO_FLOAT   : data[i]->xFloat.value = str->wcs[i]; break;
							case DAO_DOUBLE  : data[i]->xDouble.value = str->wcs[i]; break;
							default : break;
						}
					}
				}
#ifdef DAO_WITH_NUMARRAY
			}else if( dA->type == DAO_ARRAY ){
				DaoArray_ToList( & dA->xArray, list, ct, 0, 0 );
#endif
			}else if( dA->type == DAO_LIST ){
				list2 = & dA->xList;
				DArray_Resize( & list->items, list2->items.size, NULL );
				data = list->items.items.pValue;
				data2 = list2->items.items.pValue;
				for(i=0; i<list2->items.size; i++ ){
					V = DaoTypeCast( proc, tp, data2[i], & value, b1, b2 );
					if( V == NULL || V->type ==0 ) goto FailConversion;
					DaoValue_Copy( V, data + i );
				}
			}else goto FailConversion;
			break;
		case DAO_MAP :
			if( dA->type != DAO_MAP ) goto FailConversion;
			map2 = & dA->xMap;
			if( map2->unitype ){
				short m = DaoType_MatchTo( map2->unitype, ct, NULL );
				if( m == DAO_MT_EQ ) goto Rebind;
			}
			map = DaoMap_New(0);
			map->unitype = ct;
			GC_IncRC( ct );
			dC = (DaoValue*) map;
			if( ct->nested->size >0 ) tp = ct->nested->items.pType[0];
			if( ct->nested->size >1 ) tp2 = ct->nested->items.pType[1];
			if( tp == NULL || tp2 == NULL ) goto FailConversion;
			node = DMap_First( map2->items );
			for(; node!=NULL; node=DMap_Next(map2->items,node) ){
				K = DaoTypeCast( proc, tp, node->key.pValue, & key, b1, b2 );
				V = DaoTypeCast( proc, tp2, node->value.pValue, & value, b2, b1 );
				if( K ==NULL || V ==NULL || K->type ==0 || V->type ==0 ) goto FailConversion;
				DMap_Insert( map->items, K, V );
			}
			break;
		case DAO_TUPLE :
			dC->type = DAO_TUPLE;
			if( dA->type == DAO_LIST || dA->type == DAO_TUPLE ){
				DaoValue **items = NULL;
				int size = 0, tsize = ct->nested->size;
				if( dA->type == DAO_LIST ){
					size = dA->xList.items.size;
					items = dA->xList.items.items.pValue;
				}else{
					size = dA->xTuple.size;
					items = dA->xTuple.items;
				}
				if( size < tsize ) goto FailConversion;
				if( tsize ) size = tsize;
				tuple = DaoTuple_New( size );
				GC_IncRC( ct );
				tuple->unitype = ct;
				dC = (DaoValue*) tuple;
				for(i=0; i<size; i++){
					DaoValue *V = items[i];
					tp = DaoNamespace_GetType( ns, V );
					if( tsize ){
						tp2 = ct->nested->items.pType[i];
						if( tp2->tid == DAO_PAR_NAMED ) tp2 = & tp2->aux->xType;
						/* if( DaoType_MatchTo( tp, tp2, 0 ) ==0 ) goto FailConversion; */
						V = DaoTypeCast( proc, tp2, V, & value, b1, b2 );
					}
					if( V == NULL || V->type == 0 ) goto FailConversion;
					DaoValue_Copy( V, tuple->items + i );
				}
			}else if( dA->type == DAO_MAP ){
				i = 0;
				tuple = DaoTuple_New( dA->xMap.items->size );
				dC = (DaoValue*) tuple;
				tuple->unitype = ct;
				GC_IncRC( ct );
				node = DMap_First( dA->xMap.items );
				for(; node!=NULL; node=DMap_Next(dA->xMap.items,node) ){
					if( i >= ct->nested->size ){
						DaoValue_Copy( node->value.pValue, tuple->items + i );
					}else{
						tp2 = ct->nested->items.pType[i];
						if( node->key.pValue->type != DAO_STRING ) goto FailConversion;
						V = DaoTypeCast( proc, tp2, node->value.pValue, & value, b1, b2 );
						if( V == NULL || V->type ==0 ) goto FailConversion;
						DaoValue_Copy( V, tuple->items + i );
					}
					i ++;
				}
			}else{
				goto FailConversion;
			}
			break;
		case DAO_OBJECT :
			if( dA->type == DAO_CDATA ) dA = (DaoValue*) dA->xCdata.object;
			/* XXX compiling time checking */
			if( dA == NULL || dA->type != DAO_OBJECT ) goto FailConversion;
			dC = DaoObject_MapThisObject( & dA->xObject, ct );
			if( dC == NULL ) goto FailConversion;
			break;
		case DAO_CTYPE :
		case DAO_CDATA :
			if( dA->type == DAO_CDATA ){
				if( DaoType_ChildOf( dA->xCdata.ctype, ct ) ){
					dC = dA;
					/*
					 }else if( DaoCdata_ChildOf( ct->typer, dA->typer ) ){
					 // not work for C++ types, that require reinterpret_cast<>
					 dA->typer = ct->typer;
					 dC = dA;
					 */
				}else{
				}
			}else if( dA->type == DAO_OBJECT ){
				dC = (DaoValue*) DaoObject_MapThisObject( & dA->xObject, ct );
				if( dC == NULL ) goto FailConversion;
			}else{
				goto FailConversion;
			}
			break;
		case DAO_VALTYPE :
			if( DaoValue_Compare( ct->aux, dA ) != 0 ) goto FailConversion;
			dC = dA;
			break;
		case DAO_VARIANT :
			dC = dA;
			break;
		default : break;
	}
	if( wcs ) DString_Delete( wcs );
	if( shape ) DArray_Delete( shape );
	return dC;
	Rebind :
	if( wcs ) DString_Delete( wcs );
	if( shape ) DArray_Delete( shape );
#ifdef DAO_WITH_NUMARRAY
	if( array ) DaoArray_Delete( array );
#endif
	return dA;
	FailConversion :
	if( wcs ) DString_Delete( wcs );
	if( shape ) DArray_Delete( shape );
#ifdef DAO_WITH_NUMARRAY
	if( array ) DaoArray_Delete( array );
#endif
	if( map ) DaoMap_Delete( map );
	if( list ) DaoList_Delete( list );
	if( tuple ) DaoTuple_Delete( tuple );
	dC->type = 0;
	return dC;
}
int DaoProcess_CheckFE( DaoProcess *self )
{
	int res = 0;
	if( dao_fe_status() ==0 ) return 0;
	if( dao_fe_divbyzero() ){
		DaoProcess_RaiseException( self, DAO_ERROR_FLOAT_DIVBYZERO, "" );
		res = 1;
	}else if( dao_fe_underflow() ){
		DaoProcess_RaiseException( self, DAO_ERROR_FLOAT_UNDERFLOW, "" );
		res = 1;
	}else if( dao_fe_overflow() ){
		DaoProcess_RaiseException( self, DAO_ERROR_FLOAT_OVERFLOW, "" );
		res = 1;
#if 0
	}else if( dao_fe_invalid() ){
		/* disabled, because some extending modules may easily produce
		 harmless float point errors */
		DaoProcess_RaiseException( self, DAO_ERROR_FLOAT, "" );
		res = 1;
#endif
	}
	//XXX self->idClearFE = self->activeCode - self->topFrame->codes;
	dao_fe_clear();
	return res;
}
#ifdef DAO_WITH_DECORATOR
DaoRoutine* DaoRoutine_Decorate( DaoRoutine *self, DaoRoutine *decoFunc, DaoValue *p[], int n )
{
	DArray *nested = decoFunc->routType->nested;
	DaoType **decotypes = nested->items.pType;
	DaoParser *parser = DaoParser_New();
	DaoRoutine *routine = DaoRoutine_New( NULL, NULL, 1 );
	DMap *mapids = DMap_New(0,D_STRING);
	int parpass[DAO_MAX_PARAM];
	int i, j, k;
	if( decoFunc->overloads ){
		decoFunc = DaoRoutine_ResolveX( decoFunc, NULL, p, n, DVM_CALL );
		if( decoFunc == NULL || decoFunc->type != DAO_ROUTINE ) return NULL;
		nested = decoFunc->routType->nested;
		decotypes = nested->items.pType;
	}
	if( self->overloads ){
		DaoType *ftype = & decotypes[0]->aux->xType;
		DaoType **pts = ftype->nested->items.pType;
		int nn = ftype->nested->size;
		int code = DVM_CALL + (ftype->attrib & DAO_TYPE_SELF);
		self = DaoRoutine_ResolveByType( self, NULL, pts, nn, code );
	}
	parser->routine = routine;
	parser->nameSpace = routine->nameSpace = decoFunc->nameSpace;
	parser->vmSpace = routine->nameSpace->vmSpace;
	GC_IncRC( routine->nameSpace );
	routine->parCount = self->parCount;
	routine->attribs = self->attribs;
	GC_ShiftRC( self->routHost, routine->routHost );
	GC_ShiftRC( self->routType, routine->routType );
	routine->routHost = self->routHost;
	routine->routType = self->routType;
	DString_Assign( routine->routName, self->routName );
	for(i=0; i<self->routType->nested->size; i++){
		DaoType *type = self->routType->nested->items.pType[i];
		DaoRoutine_AddConstant( routine, self->routConsts->items.items.pValue[i] );
		if( type->tid == DAO_PAR_VALIST ) break;
		MAP_Insert( DArray_Top( parser->localVarMap ), type->fname, i );
		DArray_Append( routine->body->defLocals, self->body->defLocals->items.pToken[i] );
	}
	parser->regCount = self->parCount;
	i = DaoRoutine_AddConstant( routine, (DaoValue*) self );
	MAP_Insert( DArray_Top( parser->localCstMap ), decotypes[0]->fname, i );
	
	k = 1;
	for(i=0; i<nested->size; i++) parpass[i] = 0;
	for(i=0; i<n; i++){
		DaoValue *pv = p[i];
		if( pv->type == DAO_PAR_NAMED ){
			DaoNameValue *nameva = & pv->xNameValue;
			DNode *node = DMap_Find( decoFunc->routType->mapNames, nameva->name );
			if( node == NULL ) goto ErrorDecorator;
			pv = nameva->value;
			k = node->value.pInt;
		}
		j = DaoRoutine_AddConstant( routine, pv );
		MAP_Insert( DArray_Top( parser->localCstMap ), decotypes[k]->fname, j );
		parpass[k] = 1;
		k += 1;
	}
	for(i=1; i<nested->size; i++){
		k = decotypes[i]->tid;
		if( k == DAO_PAR_VALIST ) break;
		if( parpass[i] ) continue;
		if( k != DAO_PAR_DEFAULT ) continue;
		k = DaoRoutine_AddConstant( routine, decoFunc->routConsts->items.items.pValue[i] );
		MAP_Insert( DArray_Top( parser->localCstMap ), decotypes[i]->fname, k );
		parpass[i] = 1;
	}
	
	/* if( decoFunc->parser ) DaoRoutine_Compile( decoFunc ); */
	DArray_Assign( parser->tokens, decoFunc->body->source );
	if( DaoParser_ParseRoutine( parser ) ==0 ) goto ErrorDecorator;
	/* DaoRoutine_PrintCode( routine, self->activeNamespace->vmSpace->stdioStream ); */
	DaoParser_Delete( parser );
	DMap_Delete( mapids );
	return routine;
ErrorDecorator:
	DaoRoutine_Delete( routine );
	DaoParser_Delete( parser );
	DMap_Delete( mapids );
	return NULL;
}
#endif
void DaoValue_Check( DaoValue *self, DaoType *selftp, DaoType *ts[], int np, int code, DArray *es );
void DaoPrintCallError( DArray *errors, DaoStream *stdio );

void DaoProcess_ShowCallError( DaoProcess *self, DaoRoutine *rout, DaoValue *selfobj, DaoValue *ps[], int np, int code )
{
	DaoStream *ss = DaoStream_New();
	DaoNamespace *ns = self->activeNamespace;
	DaoType *selftype = selfobj ? DaoNamespace_GetType( ns, selfobj ) : NULL;
	DaoType *ts[DAO_MAX_PARAM];
	DArray *errors = DArray_New(0);
	int i;
	for(i=0; i<np; i++) ts[i] = DaoNamespace_GetType( ns, ps[i] );
	DaoValue_Check( (DaoValue*) rout, selftype, ts, np, code, errors );
	ss->attribs |= DAO_IO_STRING;
	DaoPrintCallError( errors, ss );
	DArray_Delete( errors );
	DaoProcess_RaiseException( self, DAO_ERROR_PARAM, ss->streamString->mbs );
	DaoStream_Delete( ss );
}

int DaoRoutine_SetVmCodes2( DaoRoutine *self, DaoVmcArray *vmCodes );
void DaoValue_Update( DaoValue **self, DaoNamespace *ns, DMap *deftypes );

static void DaoProcess_MapTypes( DaoProcess *self, DMap *deftypes )
{
	DaoRoutine *routine = self->activeRoutine;
	DNode *it = DMap_First(routine->body->localVarType);
	for(; it; it = DMap_Next(routine->body->localVarType,it) ){
		DaoValue *V = self->activeValues[ it->key.pInt ];
		if( V == NULL || V->type != DAO_TYPE || it->value.pType->tid != DAO_TYPE ) continue;
		MAP_Insert( deftypes, it->value.pType->nested->items.pType[0], V );
	}
}
void DaoRoutine_MapTypes( DaoRoutine *self, DMap *deftypes )
{
	DaoType *tp;
	DNode *it;
	int i;
#if 0
	printf( "DaoRoutine_MapTypes() %s\n", self->routName->mbs );
	for(it=DMap_First(deftypes); it; it=DMap_Next(deftypes,it) ){
		printf( "%16p -> %p\n", it->key.pType, it->value.pType );
		printf( "%16s -> %s\n", it->key.pType->name->mbs, it->value.pType->name->mbs );
	}
#endif
	for(it=DMap_First(self->body->localVarType); it; it=DMap_Next(self->body->localVarType,it) ){
		tp = DaoType_DefineTypes( it->value.pType, self->nameSpace, deftypes );
		it->value.pType = tp;
	}
#if 0
	for(i=0; i<self->body->regType->size; i++){
		tp = self->body->regType->items.pType[i];
		tp = DaoType_DefineTypes( tp, self->nameSpace, deftypes );
		GC_ShiftRC( tp, self->body->regType->items.pType[i] );
		self->body->regType->items.pType[i] = tp;
		if( tp ) printf( "%3i: %s\n", i, tp->name->mbs );
	}
#endif
	for(i=0; i<self->routConsts->items.size; i++){
		DaoValue_Update( & self->routConsts->items.items.pValue[i], self->nameSpace, deftypes );
	}
}
int DaoRoutine_Finalize( DaoRoutine *self, DaoType *host, DMap *deftypes )
{
	DaoType *tp = DaoType_DefineTypes( self->routType, self->nameSpace, deftypes );
	if( tp == NULL ) return 0;
	GC_ShiftRC( tp, self->routType );
	self->routType = tp;
	if( host ){
		GC_ShiftRC( host, self->routHost );
		self->routHost = host;
	}
	if( self->body == NULL ) return 1;
	DaoRoutine_MapTypes( self, deftypes );
	return 1;
	/*
	 DaoRoutine_PrintCode( self, self->nameSpace->vmSpace->stdioStream );
	 */
}

void DaoProcess_MakeRoutine( DaoProcess *self, DaoVmCode *vmc )
{
	DMap *deftypes;
	DaoValue **pp = self->activeValues + vmc->a;
	DaoValue **pp2;
	DaoType *tp;
	DaoRoutine *closure;
	DaoRoutine *proto = & pp[0]->xRoutine;
	int i;
	if( proto->body->vmCodes->size ==0 && proto->body->annotCodes->size ){
		if( DaoRoutine_SetVmCodes( proto, proto->body->annotCodes ) ==0 ){
			DaoProcess_RaiseException( self, DAO_ERROR, "invalid closure" );
			return;
		}
	}
	
	closure = DaoRoutine_Copy( proto, 1, 1 );
	if( proto->body->upRoutine ){
		DMap *map = DHash_New(0,0);
		DaoProcess *proc = DaoProcess_New( self->vmSpace );
		closure->body->upRoutine = self->activeRoutine;
		closure->body->upProcess = proc;
		GC_IncRC( self->activeRoutine );
		GC_IncRC( proc );
		DaoProcess_PushRoutine( proc, self->activeRoutine, self->activeObject );
		DaoProcess_SetActiveFrame( proc, proc->topFrame );
		for(i=0; i<self->activeRoutine->body->regCount; i++){
			DaoValue *value = self->activeValues[i];
			DNode *node = DMap_Find( map, value );
			if( node == NULL ) node = DMap_Insert( map, value, DaoValue_SimpleCopy( value ) );
			GC_IncRC( node->value.pValue );
			proc->activeValues[i] = node->value.pValue;
		}
	}
	pp2 = closure->routConsts->items.items.pValue;
	for(i=0; i<vmc->b; i+=2) DaoValue_Copy( pp[i+1], pp2 + pp[i+2]->xInteger.value );
	tp = DaoNamespace_MakeRoutType( self->activeNamespace, closure->routType, pp2, NULL, NULL );
	GC_ShiftRC( tp, closure->routType );
	closure->routType = tp;
	
	deftypes = DMap_New(0,0);
	DaoProcess_MapTypes( self, deftypes );
	tp = DaoType_DefineTypes( closure->routType, closure->nameSpace, deftypes );
	GC_ShiftRC( tp, closure->routType );
	closure->routType = tp;
	DaoRoutine_MapTypes( closure, deftypes );
	DMap_Delete( deftypes );
	
	DArray_Assign( closure->body->annotCodes, proto->body->annotCodes );
	if( DaoRoutine_SetVmCodes2( closure, proto->body->vmCodes ) ==0 ){
		DaoProcess_RaiseException( self, DAO_ERROR, "function creation failed" );
	}else if( closure->body->upProcess ){
		DaoProcess_SetValue( closure->body->upProcess, vmc->c, (DaoValue*) closure );
		closure->body->upProcess->topFrame->entry = 1 + vmc - self->topFrame->codes;
		DaoProcess_Execute( closure->body->upProcess );
		DaoProcess_PopFrame( self );
		self->status = DAO_VMPROC_STACKED;
		return;
	}
	DaoProcess_SetValue( self, vmc->c, (DaoValue*) closure );
	/*
	 DaoRoutine_PrintCode( proto, self->vmSpace->stdioStream );
	 DaoRoutine_PrintCode( closure, self->vmSpace->stdioStream );
	 printf( "%s\n", closure->routType->name->mbs );
	 */
}


#ifdef DAO_WITH_DYNCLASS

/* storage enum<const,global,var> */
static int storages[3] = { DAO_CLASS_CONSTANT, DAO_CLASS_VARIABLE, DAO_OBJECT_VARIABLE };

/* access enum<private,protected,public> */
static int permissions[3] = { DAO_DATA_PRIVATE, DAO_DATA_PROTECTED, DAO_DATA_PUBLIC };

/* a = class( name, parents, fields, methods ){ proto_class_body }
 * (1) parents: optional, list<class> or map<string,class>
 * (2) fields: optional, tuple<name:string,value:any,storage:enum<>,access:enum<>>
 * (3) methods: optional, tuple<name:string,method:routine,access:enum<>>
 * (4) default storage: $var, default access: $public.
 */
void DaoProcess_MakeClass( DaoProcess *self, DaoVmCode *vmc )
{
	DaoType *tp;
	DaoRoutine *routine = self->activeRoutine;
	DaoNamespace *ns = self->activeNamespace;
	DaoNamespace *ns2 = self->activeNamespace;
	DaoTuple *tuple = & self->activeValues[vmc->a]->xTuple;
	DaoClass *klass = DaoClass_New();
	DaoClass *proto = NULL;
	DaoList *parents = NULL;
	DaoMap *parents2 = NULL;
	DaoList *fields = NULL;
	DaoList *methods = NULL;
	DString *name = NULL;
	DaoValue **data = tuple->items;
	DMap *keys = tuple->unitype->mapNames;
	DMap *deftypes = DMap_New(0,0);
	DMap *pm_map = DMap_New(D_STRING,0);
	DMap *st_map = DMap_New(D_STRING,0);
	DMap *protoValues = NULL;
	DArray *routines = DArray_New(0);
	DNode *it, *node;
	DaoEnum pmEnum = {DAO_ENUM,0,0,0,0,0,0,NULL};
	DaoEnum stEnum = {DAO_ENUM,0,0,0,0,0,0,NULL};
	int i, st, pm, up, id, size;
	char buf[50];
	
	pmEnum.etype = dao_access_enum;
	stEnum.etype = dao_storage_enum;
	
	DaoProcess_SetValue( self, vmc->c, (DaoValue*) klass );
	//printf( "%s\n", tuple->unitype->name->mbs );
	if( vmc->b && routine->routConsts->items.items.pValue[vmc->b-1]->type == DAO_CLASS ){
		proto = & routine->routConsts->items.items.pValue[vmc->b-1]->xClass;
		protoValues = proto->protoValues;
		ns2 = proto->classRoutine->nameSpace;
	}
	
	/* extract parameters */
	if( tuple->size && data[0]->type == DAO_STRING ) name = data[0]->xString.data;
	if( parents ==NULL && parents2 == NULL && tuple->size >1 ){
		if( data[1]->type == DAO_LIST ) parents = & data[1]->xList;
		if( data[1]->type == DAO_MAP ) parents2 = & data[1]->xMap;
	}
	if( fields ==NULL && tuple->size >2 && data[2]->type == DAO_LIST ) fields = & data[2]->xList;
	if( methods ==NULL && tuple->size >3 && data[3]->type == DAO_LIST ) methods = & data[3]->xList;
	
	if( name == NULL || name->size ==0 ){
		sprintf( buf, "AnonymousClass%p", klass );
		DString_SetMBS( klass->className, buf );
		DaoClass_SetName( klass, klass->className, ns2 );
	}else{
		DaoClass_SetName( klass, name, ns2 );
	}
	for(i=0; i<routine->parCount; i++){
		DaoType *type = routine->routType->nested->items.pType[i];
		DaoValue *value = self->activeValues[i];
		/* type<@T<int|float>> kind of types may be specialized to type<float>
		 * the type holder is only available from the original routine parameters: */
		if( routine->original ) type = routine->original->routType->nested->items.pType[i];
		if( type->tid == DAO_PAR_NAMED || type->tid == DAO_PAR_DEFAULT ) type = (DaoType*) type->aux;
		if( type->tid != DAO_TYPE ) continue;
		type = type->nested->items.pType[0];
		if( type->tid == DAO_VARIANT && type->aux ) type = (DaoType*) type->aux;
		if( type->tid == DAO_INITYPE && value->type == DAO_TYPE ){
			MAP_Insert( deftypes, type, value );
		}
	}
	tp = DaoNamespace_MakeType( ns, "@class", DAO_INITYPE, NULL,NULL,0 );
	if( tp ) MAP_Insert( deftypes, tp, klass->objType );
	DaoProcess_MapTypes( self, deftypes );
	
	/* copy data from the proto class */
	if( proto ) DaoClass_CopyField( klass, proto, deftypes );
	
	/* update class members with running time data */
	for(it=DMap_First(protoValues);it;it=DMap_Next(protoValues,it)){
		DaoValue *value;
		node = DMap_Find( proto->lookupTable, it->value.pString );
		st = LOOKUP_ST( node->value.pSize );
		pm = LOOKUP_PM( node->value.pSize );
		up = LOOKUP_UP( node->value.pSize );
		id = LOOKUP_ID( node->value.pSize );
		if( up ) continue; /* should be never true */
		value = self->activeValues[it->key.pInt];
		if( st == DAO_CLASS_CONSTANT ){
			DaoRoutine *newRout = NULL;
			DaoValue **dest2 = klass->cstData->items.pValue + id;
			DaoValue *dest = *dest2;
			if( value->type == DAO_ROUTINE && value->xRoutine.routHost == proto->objType ){
				newRout = & value->xRoutine;
				if( DaoRoutine_Finalize( newRout, klass->objType, deftypes ) == 0){
					DaoProcess_RaiseException( self, DAO_ERROR, "method creation failed" );
					continue;
				}
				DArray_Append( routines, newRout );
				if( strcmp( newRout->routName->mbs, "@class" ) ==0 ){
					node = DMap_Find( proto->lookupTable, newRout->routName );
					DString_Assign( newRout->routName, klass->className );
					st = LOOKUP_ST( node->value.pSize );
					up = LOOKUP_UP( node->value.pSize );
					if( st == DAO_CLASS_CONSTANT && up ==0 ){
						id = LOOKUP_ID( node->value.pSize );
						dest2 = klass->cstData->items.pValue + id;
					}
					DRoutines_Add( klass->classRoutines->overloads, newRout );
				}
			}
			dest = *dest2;
			if( dest->type == DAO_ROUTINE ){
				DaoRoutine *rout = & dest->xRoutine;
				if( rout->routHost != klass->objType ) DaoValue_Clear( dest2 );
			}
			if( dest->type == DAO_ROUTINE && dest->xRoutine.overloads ){
				DRoutines_Add( dest->xRoutine.overloads, newRout );
			}else{
				DaoValue_Move( value, dest2, NULL );
			}
		}else if( st == DAO_CLASS_VARIABLE ){
			tp = klass->glbDataType->items.pType[id];
			DaoValue_Move( value, klass->glbData->items.pValue + id, tp );
		}else if( st == DAO_OBJECT_VARIABLE ){
			tp = klass->objDataType->items.pType[id];
			DaoValue_Move( value, klass->objDataDefault->items.pValue + id, tp );
		}
	}
	
	/* add parents from parameters */
	if( parents ){
		for(i=0; i<parents->items.size; i++){
			DaoValue *item = parents->items.items.pValue[i];
			if( item->type == DAO_CLASS ){
				DaoClass_AddSuperClass( klass, item, item->xClass.className );
			}else if( item->type == DAO_CTYPE ){
				DaoClass_AddSuperClass( klass, item, item->xCdata.ctype->name );
			}
		}
	}else if( parents2 ){
		for(it=DMap_First(parents2->items);it;it=DMap_Next(parents2->items,it)){
			int type = it->value.pValue->type;
			if( it->key.pValue->type == DAO_STRING && (type == DAO_CLASS || type == DAO_CTYPE) ){
				DaoClass_AddSuperClass( klass, it->value.pValue, it->key.pValue->xString.data );
			}//XXX error handling
		}
	}
	DaoClass_DeriveClassData( klass );
	if( fields ){ /* add fields from parameters */
		for(i=0; i<fields->items.size; i++){
			DaoValue *fieldv = fields->items.items.pValue[i];
			DaoType *type = NULL;
			DaoValue *value = NULL;
			
			if( DaoType_MatchValue( dao_dynclass_field, fieldv, NULL ) == 0) continue;//XXX
			data = fieldv->xTuple.items;
			size = fieldv->xTuple.size;
			st = DAO_OBJECT_VARIABLE;
			pm = DAO_DATA_PUBLIC;
			
			name = NULL;
			if( size && data[0]->type == DAO_STRING ) name = data[0]->xString.data;
			if( size > 1 && data[1]->type ){
				value = data[1];
				type = fieldv->xTuple.unitype->nested->items.pType[1];
			}
			if( name == NULL || value == NULL ) continue;
			if( MAP_Find( klass->lookupTable, name ) ) continue;
			
			if( size > 2 && data[2]->type == DAO_ENUM ){
				if( DaoEnum_SetValue( & stEnum, & data[2]->xEnum, NULL ) ==0) goto InvalidField;
				st = storages[ stEnum.value ];
			}
			if( size > 3 && data[3]->type == DAO_ENUM ){
				if( DaoEnum_SetValue( & pmEnum, & data[3]->xEnum, NULL ) ==0) goto InvalidField;
				pm = permissions[ pmEnum.value ];
			}
			/* printf( "%s %i %i\n", name->mbs, st, pm ); */
			switch( st ){
			case DAO_OBJECT_VARIABLE: DaoClass_AddObjectVar( klass, name, value, type, pm, 0 ); break;
			case DAO_CLASS_VARIABLE : DaoClass_AddGlobalVar( klass, name, value, type, pm, 0 ); break;
			case DAO_CLASS_CONSTANT : DaoClass_AddConst( klass, name, value, pm, 0 ); break;
			default : break;
			}
			continue;
InvalidField:
			DaoProcess_RaiseException( self, DAO_ERROR_PARAM, "" );
		}
	}
	if( methods ){ /* add methods from parameters */
		for(i=0; i<methods->items.size; i++){
			DaoValue *methodv = methods->items.items.pValue[i];
			DaoRoutine *newRout;
			DaoValue *method = NULL;
			DaoValue *dest;
			
			if( DaoType_MatchValue( dao_dynclass_method, methodv, NULL ) == 0) continue;//XXX
			data = methodv->xTuple.items;
			size = methodv->xTuple.size;
			pm = DAO_DATA_PUBLIC;
			
			name = NULL;
			if( size && data[0]->type == DAO_STRING ) name = data[0]->xString.data;
			if( size > 1 && data[1]->type == DAO_ROUTINE ) method = data[1];
			if( name == NULL || method == NULL ) continue;
			if( size > 2 && data[2]->type == DAO_ENUM ){
				if( DaoEnum_SetValue( & pmEnum, & data[2]->xEnum, NULL ) ==0) goto InvalidMethod;
				pm = permissions[ pmEnum.value ];
			}
			
			newRout = & method->xRoutine;
			if( ROUT_HOST_TID( newRout ) !=0 ) continue;
			if( DaoRoutine_Finalize( newRout, klass->objType, deftypes ) == 0){
				DaoProcess_RaiseException( self, DAO_ERROR, "method creation failed" );
				continue; // XXX
			}
			DArray_Append( routines, newRout );
			DString_Assign( newRout->routName, name );
			if( DString_EQ( newRout->routName, klass->className ) ){
				DRoutines_Add( klass->classRoutines->overloads, newRout );
			}
			
			node = DMap_Find( proto->lookupTable, name );
			if( node == NULL ){
				DaoClass_AddConst( klass, name, method, pm, 0 );
				continue;
			}
			if( LOOKUP_UP( node->value.pSize ) ) continue;
			if( LOOKUP_ST( node->value.pSize ) != DAO_CLASS_CONSTANT ) continue;
			id = LOOKUP_ID( node->value.pSize );
			dest = klass->cstData->items.pValue[id];
			if( dest->type == DAO_ROUTINE && dest->xRoutine.overloads ){
				DRoutines_Add( dest->xRoutine.overloads, newRout );
			}
			continue;
InvalidMethod:
			DaoProcess_RaiseException( self, DAO_ERROR_PARAM, "" );
		}
	}
	for(i=0; i<routines->size; i++){
		if( DaoRoutine_DoTypeInference( routines->items.pRoutine[i], 0 ) == 0 ){
			DaoProcess_RaiseException( self, DAO_ERROR, "method creation failed" );
			// XXX
		}
	}
	DaoClass_DeriveObjectData( klass );
	DaoClass_ResetAttributes( klass );
	DArray_Delete( routines );
	DMap_Delete( deftypes );
	DMap_Delete( pm_map );
	DMap_Delete( st_map );
}
#else
void DaoProcess_MakeClass( DaoProcess *self, DaoVmCode *vmc )
{
	DaoProcess_RaiseException( self, DAO_ERROR, getCtInfo( DAO_DISABLED_DYNCLASS ) );
}
#endif


int DaoProcess_DoCheckExcept( DaoProcess *self, DaoVmCode *vmc )
{
	DaoList *list = & self->activeNamespace->varData->items.pValue[DVR_NSV_EXCEPTIONS]->xList;
	DaoList_Clear( list );
	self->activeCode = vmc;
	if( DaoProcess_CheckFE( self ) ) return 1;
	return ( self->exceptions->size > 0 );
}
static void DaoInitException( DaoException *except, DaoProcess *proc, DaoVmCode *vmc, int fe, const char *value )
{
	DaoRoutine *rout = proc->activeRoutine;
	DaoType *efloat = DaoException_GetType( DAO_ERROR_FLOAT );
	DaoVmCodeX **annotCodes = rout->body->annotCodes->items.pVmc;
	DaoStackFrame *frame = proc->topFrame->prev;
	int line, line2;
	int id = (int) (vmc - proc->topFrame->active->codes);
	
	line = line2 = rout->defLine;
	if( vmc && rout->body->vmCodes->size ) line = annotCodes[id]->line;
	line2 = line;
	except->routine = rout;
	except->toLine = line;
	if( DaoType_ChildOf( except->ctype, efloat ) && fe >=0 )
		line2 = (vmc && rout->body->vmCodes->size) ? annotCodes[ fe ]->line : rout->defLine;
	except->fromLine = line2;
	if( value && value[0] != 0 ){
		DaoValue_Clear( & except->edata );
		except->edata = DaoValue_NewMBString( value, 0 );
	}
	DArray_Clear( except->callers );
	DArray_Clear( except->lines );
#if 0
	XXX
	if( proc->thisFunction ){
		DArray_Append( except->callers, rout );
		DArray_Append( except->lines, (size_t)line );
		except->routine = (DRoutine*) proc->thisFunction;
	}
#endif
	while( frame && frame->routine ){
		DaoRoutineBody *body = frame->routine->body;
		if( except->callers->size >= 5 ) break;
		line = body ? body->annotCodes->items.pVmc[ frame->entry ]->line : 0;
		DArray_Append( except->callers, frame->routine );
		DArray_Append( except->lines, (size_t) line );
		frame = frame->prev;
	}
}
extern void STD_Debug( DaoProcess *proc, DaoValue *p[], int N );
void DaoProcess_DoRaiseExcept( DaoProcess *self, DaoVmCode *vmc )
{
	DaoStream *stream = self->vmSpace->errorStream;
	DaoException *cdata = NULL;
	DaoType *except = DaoException_GetType( DAO_EXCEPTION );
	DaoType *warning = DaoException_GetType( DAO_WARNING );
	DaoList *list = & self->activeNamespace->varData->items.pValue[DVR_NSV_EXCEPTIONS]->xList;
	DaoValue **excepts = self->activeValues + vmc->a;
	DaoValue *val;
	ushort_t i, line = 0, line2 = 0;
	ushort_t N = vmc->b -1;
	line2 = line;
	if( N == 0 && list->items.size >0 ){
		N = list->items.size;
		excepts = list->items.items.pValue;
	}
	for(i=0; i<N; i++){
		val = excepts[i];
		if( val->type == DAO_OBJECT || val->type == DAO_CDATA ){
			cdata = NULL;
			if( val->type == DAO_OBJECT ){
				cdata = (DaoException*) DaoObject_MapThisObject( & val->xObject, except );
			}else{
				if( DaoType_ChildOf( val->xCdata.ctype, except ) ) cdata = (DaoException*)val;
			}
			if( cdata == NULL ) goto InvalidException;
			DaoInitException( (DaoException*)cdata, self, vmc, 0/*self->idClearFE*/, NULL );
			if( DaoType_ChildOf( cdata->ctype, warning ) ){
				DaoPrintException( cdata, stream );
			}else{
				DArray_Append( self->exceptions, val );
			}
		}else{
			goto InvalidException;
		}
		continue;
	InvalidException:
		DaoProcess_RaiseException( self, DAO_ERROR, "invalid exception object" );
		break;
	}
	DaoList_Clear( list );
	if( self->vmSpace->options & DAO_EXEC_DEBUG ){
		DaoProcess_PrintException( self, 0 );
		STD_Debug( self, NULL, 0 );
	}
}
int DaoProcess_DoRescueExcept( DaoProcess *self, DaoVmCode *vmc )
{
	DaoList *list = & self->activeNamespace->varData->items.pValue[DVR_NSV_EXCEPTIONS]->xList;
	DaoType *ext = DaoException_GetType( DAO_EXCEPTION );
	DaoType *any = DaoException_GetType( DAO_EXCEPTION_ANY );
	DaoType *none = DaoException_GetType( DAO_EXCEPTION_NONE );
	DaoValue **excepts = self->activeValues + vmc->a;
	DaoValue *val, *val2;
	DaoCdata *cdata;
	ushort_t i, j;
	ushort_t N = vmc->b -1;
	int canRescue = 0;
	int M = self->exceptions->size;
	DaoList_Clear( list );
	self->activeCode = vmc;
	if( DaoProcess_CheckFE( self ) ) M = self->exceptions->size;
	if( N ==0 && M >0 ){ /* rescue without exception list */
		DArray_Swap( self->exceptions, & list->items );
		return 1;
	}
	for(i=0; i<N; i++){
		val = excepts[i];
		if( val->type == DAO_CLASS || val->type == DAO_CTYPE ){
			cdata = & val->xCdata;
			if( val->type == DAO_CLASS ){
				cdata = (DaoCdata*) DaoClass_MapToParent( & val->xClass, ext );
			}
			if( cdata && DaoType_ChildOf( cdata->ctype, any ) ){
				DArray_Swap( self->exceptions, & list->items );
				return 1;
			}else if( cdata && DaoType_ChildOf( cdata->ctype, none ) && M ==0 ){
				return 1;
			}else if( cdata ){
				for(j=0; j<self->exceptions->size; j++){
					val2 = self->exceptions->items.pValue[j];
					if( val->type == DAO_CLASS && val2->type == DAO_OBJECT ){
						if( DaoClass_ChildOf( val2->xObject.defClass, val ) ){
							canRescue = 1;
							DArray_Append( & list->items, val2 );
							DArray_Erase( self->exceptions, j, 1 );
						}
					}else if( val->type == DAO_CTYPE ){
						DaoCdata *cdata2 = & val2->xCdata;
						if( val2->type == DAO_CLASS ){
							cdata2 = (DaoCdata*) DaoType_CastToParent( val2, ext );
						}
						if( DaoType_ChildOf( cdata2->ctype, cdata->ctype ) ){
							canRescue = 1;
							DArray_Append( & list->items, val2 );
							DArray_Erase( self->exceptions, j, 1 );
						}
					}
				}
			}else{
			}
		}
	}
	return canRescue;
}
void DaoProcess_RaiseException( DaoProcess *self, int type, const char *value )
{
	DaoType *etype;
	DaoType *warning = DaoException_GetType( DAO_WARNING );
	DaoStream *stream = self->vmSpace->errorStream;
	DaoException *except;
	if( type <= 1 ) return;
	if( type >= ENDOF_BASIC_EXCEPT ) type = DAO_ERROR;
	
	etype = DaoException_GetType( type );
	if( DaoType_ChildOf( etype, warning ) ){
		/* XXX support warning suppression */
		except = DaoException_New( etype );
		DaoInitException( except, self, self->activeCode, 0/*self->idClearFE*/, value );
		DaoPrintException( except, stream );
		etype->typer->Delete( except );
		return;
	}
	except = DaoException_New( etype );
	DaoInitException( except, self, self->activeCode, 0/*self->idClearFE*/, value );
	DArray_Append( self->exceptions, (DaoValue*) except );
	if( (self->vmSpace->options & DAO_EXEC_DEBUG) ){
		if( self->stopit ==0 && self->vmSpace->stopit ==0 ){
			DaoProcess_Trace( self, 10 );
			DaoProcess_PrintException( self, 0 );
			STD_Debug( self, NULL, 0 );
		}
	}
}
void DaoProcess_RaiseTypeError( DaoProcess *self, DaoType *from, DaoType *to, const char *op )
{
	DString *details = DString_New(1);
	DString_AppendMBS( details, op );
	DString_AppendMBS( details, " from \'" );
	DString_Append( details,  from->name );
	DString_AppendMBS( details, "\' to \'" );
	DString_Append( details,  to->name );
	DString_AppendMBS( details, "\'." );
	DaoProcess_RaiseException( self, DAO_ERROR_TYPE, details->mbs );
	DString_Delete( details );
}

void DaoPrintException( DaoException *ex, DaoStream *stream )
{
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

	if( ex->edata && ex->edata->type == DAO_STRING ){
		DaoStream_WriteString( ss, ex->edata->xString.data );
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
		DaoRoutine *rout = ex->callers->items.pRoutine[i];
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
	DaoStream *stream = self->vmSpace->errorStream;
	DaoValue **excobjs = self->exceptions->items.pValue;
	int i;
	for(i=0; i<self->exceptions->size; i++){
		DaoException *except = NULL;
		if( excobjs[i]->type == DAO_CDATA ){
			except = (DaoException*) excobjs[i];
		}else if( excobjs[i]->type == DAO_OBJECT ){
			except = (DaoException*)DaoObject_MapThisObject( & excobjs[i]->xObject, extype );
		}
		if( except == NULL ) continue;
		DaoPrintException( except, stream );
	}
	if( clear ) DArray_Clear( self->exceptions );
}

void DaoProcess_Print( DaoProcess *self, const char *chs )
{
	DaoStream_WriteMBS( self->vmSpace->stdioStream, chs );
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
	if( self->activeRoutine->body->annotCodes->size == 0 )
		DArray_Append( self->activeRoutine->body->annotCodes, & vmcx );

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

static void DaoProcess_AdjustCodes( DaoProcess *self, int options )
{
	DaoUserHandler *handler = self->vmSpace->userHandler;
	DaoRoutine *routine = self->topFrame->routine;
	DaoVmCode *c = self->topFrame->codes;
	int i, n = routine->body->vmCodes->size;
	int mode = routine->body->mode;
	if( options & DAO_EXEC_DEBUG ){
		routine->body->mode |= DAO_EXEC_DEBUG;
		if( handler && handler->BreakPoints ) handler->BreakPoints( handler, routine );
	}else if( mode & DAO_EXEC_DEBUG ){
		routine->body->mode &= ~DAO_EXEC_DEBUG;
		for(i=0; i<n; i++) if( c[i].code == DVM_DEBUG ) c[i].code = DVM_NOP;
	}
	if( (options & DAO_EXEC_SAFE) == (mode & DAO_EXEC_SAFE) ) return;
	if( options & DAO_EXEC_SAFE ){
		routine->body->mode |= DAO_EXEC_SAFE;
		for(i=0; i<n; i++) if( c[i].code == DVM_GOTO ) c[i].code = DVM_SAFE_GOTO;
	}else if( mode & DAO_EXEC_SAFE ){
		routine->body->mode &= ~DAO_EXEC_SAFE;
		for(i=0; i<n; i++) if( c[i].code == DVM_SAFE_GOTO ) c[i].code = DVM_GOTO;
	}
}

void DaoProcess_SetStdio( DaoProcess *self, DaoStream *stream )
{
	GC_ShiftRC( stream, self->stdioStream );
	self->stdioStream = stream;
}
