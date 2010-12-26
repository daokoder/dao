/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2010, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

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
#include"daoSched.h"

#ifndef FE_ALL_EXCEPT
#define FE_ALL_EXCEPT 0xffff
#endif

#define SEMA_PER_VMPROC  1000

DaoList*  DaoContext_GetList( DaoContext *self, DaoVmCode *vmc );
static DaoMap*   DaoContext_GetMap( DaoContext *self,  DaoVmCode *vmc );
static DaoArray* DaoContext_GetArray( DaoContext *self, DaoVmCode *vmc );

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
extern void DaoContext_DoFastCall( DaoContext *self, DaoVmCode *vmc );

void DaoArray_number_op_array( DaoArray *C, DValue A, DaoArray *B, short op, DaoContext *ctx );
void DaoArray_array_op_number( DaoArray *C, DaoArray *A, DValue B, short op, DaoContext *ctx );
void DaoArray_ArrayArith( DaoArray *s, DaoArray *l, DaoArray *r, short p, DaoContext *c );

extern void DaoVmProcess_Trace( DaoVmProcess *self, int depth );
int DaoVmProcess_Resume2( DaoVmProcess *self, DValue *par[], int N, DaoContext *ret );
void DaoPrintException( DaoCData *except, DaoStream *stream );

void DaoVmCode_Print( DaoVmCode self, char *buffer )
{
	const char *name = getOpcodeName( self.code );
	static const char *fmt = "%-11s : %6i , %6i , %6i ;\n";
	if( buffer == NULL )
		printf( fmt, name, self.a, self.b, self.c );
	else
		sprintf( buffer, fmt, name, self.a, self.b, self.c );
}
void DaoVmCodeX_Print( DaoVmCodeX self, char *annot )
{
	const char *name = getOpcodeName( self.code );
	static const char *fmt = "%-11s : %6i , %6i , %6i ;  %4i,  %s\n";
	printf( fmt, name, self.a, self.b, self.c, self.line, annot ? annot : "" );
}

/**/

DaoTypeBase ctxTyper =
{
	"context", & baseCore, NULL, NULL, {0},
	(FuncPtrDel) DaoContext_Delete, NULL
};

DaoContext* DaoContext_New()
{
	DaoContext *self = (DaoContext*) dao_malloc( sizeof( DaoContext ) );
	DaoBase_Init( self, DAO_CONTEXT );

	self->codes = NULL;
	self->vmc = NULL;
	self->frame = NULL;
	self->regArray = DVaTuple_New( 0, daoNullValue );
	self->regValues = NULL;

	self->routine   = NULL;
	self->object    = NULL;
	self->nameSpace = NULL;
	self->vmSpace   = NULL;
	self->process    = NULL;

	self->thisFunction = NULL;

	self->parCount = 0;
	self->entryCode = 0;
	self->ctxState = 0;
	self->constCall = 0;
	return self;
}

void DaoContext_Delete( DaoContext *self )
{
	if( self->object ) GC_DecRC( self->object );
	if( self->routine ) GC_DecRC( self->routine );
	if( self->regValues ) dao_free( self->regValues );
	DVaTuple_Delete( self->regArray );
	dao_free( self );
}
static void DaoContext_InitValues( DaoContext *self )
{
	DValue *values;
	int i, t, N = self->routine->locRegCount;
	self->entryCode = 0;
	self->ctxState = 0;
	self->constCall = 0;
	self->vmc = NULL;
	self->codes = self->routine->vmCodes->codes;
	self->regTypes = self->routine->regType->items.pType;
	if( self->regArray->size < N ){
		DVaTuple_Resize( self->regArray, N, daoNullValue );
		self->regValues = dao_realloc( self->regValues, N * sizeof(DValue*) );
	}
	if( self->routine->regType->size ==0 ) return; /* DaoTaskThread_New() */
	values = self->regArray->data;
	for(i=0; i<N; i++){
		t = 0;
		self->regValues[i] = values + i;
		if( self->regTypes[i] ) t = self->regTypes[i]->tid;
		if( t > DAO_DOUBLE && t < DAO_ARRAY ) DValue_Clear( & values[i] );
		if( t <= DAO_DOUBLE ) values[i].t = t;
	}
}
void DaoContext_Init( DaoContext *self, DaoRoutine *routine )
{
	/* dummy function, assume DaoContext_InitWithParams() will be called next */
	if( routine->type == DAO_ROUTINE && routine->minimal ==0 ){
		while( routine->revised ) routine = routine->revised;
		DaoRoutine_Compile( routine );
	}
	GC_ShiftRC( routine, self->routine );
	self->routine   = routine;
	self->nameSpace = routine->nameSpace;

	/* routine could be DaoMethod pushed in at DaoParser_ParseParams() */
	if( routine->type != DAO_ROUTINE || routine->minimal == 1 ) return;
	DaoContext_InitValues( self );
}
int DaoContext_InitWithParams( DaoContext *self, DaoVmProcess *vmp, DValue *pars[], int npar )
{ 
	DValue *selfpar = NULL;
	DValue value = daoNullObject;
	DaoObject *othis = self->object;
	DaoRoutine *rout = self->routine;
	value.v.object = self->object;
	if( self->object ) selfpar = & value;
	if( ! self->routine ) return 0;
	rout = (DaoRoutine*)DRoutine_GetOverLoad( (DRoutine*)rout, selfpar, pars, npar, 0 );
	if( rout==NULL || rout->type != DAO_ROUTINE || rout->minimal ==1 ){
		DaoContext_RaiseException( self, DAO_ERROR_PARAM, "" );
		return 0;
	}
	if( rout->type == DAO_ROUTINE ){
		while( rout->revised ) rout = rout->revised;
		DaoRoutine_Compile( rout );
	}
	GC_ShiftRC( rout, self->routine );
	self->routine = rout;
	self->nameSpace = rout->nameSpace;
	if( othis && othis->myClass->objType != rout->routHost ){
		othis = othis->that; /* for virtual method call */
		othis = (DaoObject*) DaoObject_MapThisObject( othis, rout->routHost );
		GC_ShiftRC( othis, self->object );
		self->object = othis;
	}

	DaoContext_InitValues( self );
	return 1;
}
void DaoContext_AdjustCodes( DaoContext *self, int options )
{
	DaoUserHandler *handler = self->vmSpace->userHandler;
	DaoRoutine *routine = self->routine;
	DaoVmCode *c = self->codes;
	int i, n = routine->vmCodes->size;
	int mode = routine->mode;
	if( options & DAO_EXEC_DEBUG ){
		routine->mode |= DAO_EXEC_DEBUG;
		if( handler && handler->BreakPoints ) handler->BreakPoints( handler, routine );
	}else if( mode & DAO_EXEC_DEBUG ){
		routine->mode &= ~DAO_EXEC_DEBUG;
		for(i=0; i<n; i++) if( c[i].code == DVM_DEBUG ) c[i].code = DVM_NOP;
	}
	if( (options & DAO_EXEC_SAFE) == (mode & DAO_EXEC_SAFE) ) return;
	if( options & DAO_EXEC_SAFE ){
		routine->mode |= DAO_EXEC_SAFE;
		for(i=0; i<n; i++) if( c[i].code == DVM_GOTO ) c[i].code = DVM_SAFE_GOTO;
	}else if( mode & DAO_EXEC_SAFE ){
		routine->mode &= ~DAO_EXEC_SAFE;
		for(i=0; i<n; i++) if( c[i].code == DVM_SAFE_GOTO ) c[i].code = DVM_GOTO;
	}
}

int DaoContext_SetData( DaoContext *self, ushort_t reg, DaoBase *dbase );

void DaoContext_SetResult( DaoContext *self, DaoBase *result )
{
	DaoContext_SetData( self, self->vmc->c, result );
}
int DaoMoveAC( DaoContext *self, DValue A, DValue *C, DaoType *t )
{
	if( ! DValue_Move( A, C, t ) ){
		DaoType *type = DaoNameSpace_GetTypeV( self->nameSpace, A );
		DaoContext_RaiseTypeError( self, type, t, "moving" );
		return 0;
	}
	return 1;
}
static int DaoAssign( DaoContext *self, DaoBase *A, DValue *C, DaoType *t )
{
	DValue val = daoNullValue;
	val.v.p = A;
	if( A ) val.t = A->type;
	return DValue_Move( val, C, t );
}

int DaoContext_SetData( DaoContext *self, ushort_t reg, DaoBase *dbase )
{
	DaoType *abtp = self->regTypes[ reg ];
	int bl;
	/*
	   if( dbase && dbase->type == DAO_ARRAY )
	   printf("type=%i\treg=%i\tdbase=%p\n", dbase ? dbase->type : 0, reg, dbase);

	   DaoArray *array = NULL;
	   if( self->regValues[reg].t == DAO_ARRAY ){
	   printf( "set: %i %p\n", self->regValues[reg].v.array->refCount, self->regValues[reg].v.array );
	   array = self->regValues[reg].v.array;
	   }
	 */
	bl = DaoAssign( self, dbase, self->regValues[ reg ], abtp );
	if( bl ==0 ){
		DaoType *type = DaoNameSpace_GetType( self->nameSpace, dbase );
		DaoContext_RaiseTypeError( self, type, abtp, "moving" );
		return 0;
	}
	return bl;
}
DValue* DaoContext_SetValue( DaoContext *self, ushort_t reg, DValue value )
{
	DaoType *tp = self->regTypes[reg];
	int res = DValue_Move( value, self->regValues[ reg ], tp );
	if( res ) return self->regValues[ reg ];
	return NULL;
}
DValue* DaoContext_PutValue( DaoContext *self, DValue value )
{
	return DaoContext_SetValue( self, self->vmc->c, value );
}
int DaoContext_PutReference( DaoContext *self, DValue *refer )
{
	int tm, reg = self->vmc->c;
	DaoType *tp = self->regTypes[reg];
	if( self->regValues[reg] == refer ) return 1;
	if( tp == NULL ){
		self->regValues[reg] = refer;
		return 1;
	}
	tm = DaoType_MatchValue( tp, *refer, NULL );
	if( tm == DAO_MT_EQ ){
		self->regValues[reg] = refer;
		return 1;
	}
	tm = DValue_Move( *refer, self->regValues[ reg ], tp );
	if( refer->t == DAO_CLASS || refer->t == DAO_OBJECT || refer->t == DAO_CDATA 
			|| refer->t == DAO_ROUTINE || refer->t == DAO_FUNCTION ){
		return tm;
	}
	return 0;
}

void DaoContext_Print( DaoContext *self, const char *chs )
{
	DaoStream_WriteMBS( self->vmSpace->stdStream, chs );
}
void DaoContext_PrintInfo( DaoContext *self, const char *head, const char *info )
{
	int line = self->routine->defLine;
	int id = (int) (self->vmc - self->codes);
	if( self->vmc ) line = self->routine->annotCodes->items.pVmc[id]->line;
	DaoStream_PrintInfo( self->vmSpace->stdStream, head, self->nameSpace->name, line, info, NULL );
	if( strcmp( head, "ERROR" )==0 ) self->process->stopit = 1;
}
void DaoContext_PrintVmCode( DaoContext *self )
{
	char buffer[100];
	int k = (int)( self->vmc - self->codes );
	int i = k > 5 ? k-5 : 0;
	for(; i<=k; i++){
		DaoVmCode_Print( self->codes[i], buffer );
		DaoStream_WriteMBS( self->vmSpace->stdStream, buffer );
	}
}

/**/


void DValue_SimpleMove2( DValue from, DValue *to )
{
	if( from.ndef != DAO_NOCOPYING ){ /* XXX */
		from.v.p = DaoBase_Duplicate( from.v.p, NULL );
	}else{
		from.t = 0;
	}
	GC_ShiftRC( from.v.p, to->v.p );
	*to = from;
}

#ifdef DAO_WITH_JIT

void DaoJIT_DoMove( DaoContext *self, int id )
{
	DaoVmCode *vmc = self->codes + id;
	DaoType *ct = self->regTypes[ vmc->c ];
	if( ! DValue_Move( *self->regValues[vmc->a], self->regValues[ vmc->c ], ct ) )
		DaoContext_RaiseException( self, DAO_ERROR_TYPE, "types not matching1" );
}
void DaoJIT_MovePP( DaoContext *self, int id )
{
	DaoVmCode *vmc = self->codes + id;
	DValue_SimpleMove2( *self->regValues[ vmc->a ], self->regValues[ vmc->c ] );
	/*
	   DValue *dA = self->regValues + vmc->a;
	   DValue *dC = self->regValues + vmc->c;
	   GC_ShiftRC( dA->v.p, dC->v.p );
	 *dC = *dA;
	 */
}
void DaoJIT_DoBinArith( DaoContext *self, int id )
{
	self->vmc = self->codes + id;
	DaoContext_DoBinArith( self, self->vmc );
}
void DaoJIT_MoveString( DaoContext *self, int id )
{
	DaoVmCode *vmc = self->codes + id;
	DValue *dA = self->regValues[ vmc->a ];
	DValue *dC = self->regValues[ vmc->c ];
	if( dC->t ){
		DString_Assign( dC->v.s, dA->v.s );
	}else{
		dC->t = DAO_STRING;
		dC->v.s = DString_Copy( dA->v.s );
	}
}
void DaoJIT_AddString( DaoContext *self, int id )
{
	DaoVmCode *vmc = self->codes + id;
	DValue *dA = self->regValues[ vmc->a ];
	DValue *dB = self->regValues[ vmc->b ];
	DValue *dC = self->regValues[ vmc->c ];
	dC->t = DAO_STRING;
	self->vmc = vmc;
	if( vmc->a == vmc->c ){
		DString_Append( dA->v.s, dB->v.s );
	}else if( vmc->b == vmc->c ){
		DString_Insert( dB->v.s, dA->v.s, 0, 0, 0 );
	}else{
		if( dC->v.s == NULL ) dC->v.s = DString_Copy( dA->v.s );
		DString_Assign( dC->v.s, dA->v.s );
		DString_Append( dC->v.s, dB->v.s );
	}
}
void DaoJIT_GETI_LI( DaoContext *self, int id )
{
	DaoVmCode *vmc = self->codes + id;
	DValue **locVars = self->regValues;
	DaoList *list = locVars[ vmc->a ]->v.list;
	id = locVars[ vmc->b ]->v.i;
	DValue_SimpleMove2( list->items->data[id], locVars[ vmc->c ] );
	/*
	   DaoType *abtp = self->routine->regType->items.pType[ vmc->c ];
	   DaoMoveAC( self, list->items->data[id], locVars + vmc->c, abtp );
	   GC_ShiftRC( dA->v.p, dC->v.p );
	 *dC = *dA;
	 */
}
void DaoJIT_SETI_LI( DaoContext *self, int id )
{
	DaoVmCode *vmc = self->codes + id;
	DValue **locVars = self->regValues;
	DaoList *list = locVars[ vmc->c ]->v.list;
	id = locVars[ vmc->b ]->v.i;
	DValue_SimpleMove2( locVars[ vmc->a ][0], list->items->data + id );
	/*
	   DaoType *abtp = self->routine->regType->items.pType[ vmc->c ];
	   DaoMoveAC( self, list->items->data[id], locVars + vmc->c, abtp );
	   GC_ShiftRC( dA->v.p, dC->v.p );
	 *dC = *dA;
	 */
}
void DaoJIT_GETI_LSI( DaoContext *self, int id )
{
	DaoVmCode *vmc = self->codes + id;
	DValue **locVars = self->regValues;
	DaoList *list = locVars[ vmc->a ]->v.list;
	DValue *dA, *dC = self->regValues[ vmc->c ];
	id = locVars[ vmc->b ]->v.i;
	dA = list->items->data + id;
	if( id <0 || id >= list->items->size )
		DaoContext_RaiseException( self, DAO_ERROR_INDEX_OUTOFRANGE, "" );
	if( dC->t ==0 ){
		dC->t = DAO_STRING;
		dC->v.s = DString_Copy( dA->v.s );
	}else{
		DString_Assign( dC->v.s, dA->v.s );
	}
}
void DaoJIT_SETI_LSIS( DaoContext *self, int id )
{
	DaoVmCode *vmc = self->codes + id;
	DValue **locVars = self->regValues;
	DaoList *list = locVars[ vmc->c ]->v.list;
	DValue *dC, *dA = self->regValues[ vmc->a ];
	id = locVars[ vmc->b ]->v.i;
	dC = list->items->data + id;
	if( id <0 || id >= list->items->size )
		DaoContext_RaiseException( self, DAO_ERROR_INDEX_OUTOFRANGE, "" );
	DString_Assign( dC->v.s, dA->v.s );
}
#endif

dint* DaoContext_PutInteger( DaoContext *self, dint value )
{
	DValue *res = DaoContext_SetValue( self, self->vmc->c, daoZeroInt );
	if( res ==NULL ) return NULL;
	res->v.i = value;
	return & res->v.i;
}
float* DaoContext_PutFloat( DaoContext *self, float value )
{
	DValue *res = DaoContext_SetValue( self, self->vmc->c, daoZeroFloat );
	if( res ==NULL ) return NULL;
	res->v.f = value;
	return & res->v.f;
}
double* DaoContext_PutDouble( DaoContext *self, double value )
{
	DValue *res = DaoContext_SetValue( self, self->vmc->c, daoZeroDouble );
	if( res ==NULL ) return NULL;
	res->v.d = value;
	return & res->v.d;
}
complex16* DaoContext_PutComplex( DaoContext *self, complex16 value )
{
	complex16 temp;
	DValue val = daoNullComplex;
	DValue *res;
	val.v.c = & temp;
	res = DaoContext_SetValue( self, self->vmc->c, val );
	if( res ==NULL ) return NULL;
	res->v.c->real = value.real;
	res->v.c->imag = value.imag;
	return res->v.c;
}
DString* DaoContext_PutMBString( DaoContext *self, const char *mbs )
{
	DString *str = DString_New(1);
	DValue val = daoNullString;
	DValue *res;
	val.v.s = str;
	res = DaoContext_SetValue( self, self->vmc->c, val );
	DString_Delete( str );
	if( res ==NULL ) return NULL;
	DString_SetMBS( res->v.s, mbs );
	return res->v.s;
}
DString* DaoContext_PutWCString( DaoContext *self, const wchar_t *wcs )
{
	DString *str = DString_New(0);
	DValue val = daoNullString;
	DValue *res;
	val.v.s = str;
	res = DaoContext_SetValue( self, self->vmc->c, val );
	DString_Delete( str );
	if( res ==NULL ) return NULL;
	DString_SetWCS( res->v.s, wcs );
	return res->v.s;
}
DString*   DaoContext_PutString( DaoContext *self, DString *str )
{
	DValue val = daoNullString;
	DValue *res;
	val.v.s = str;
	res = DaoContext_SetValue( self, self->vmc->c, val );
	if( res ==NULL ) return NULL;
	return res->v.s;
}
DString* DaoContext_PutBytes( DaoContext *self, const char *bytes, int N )
{
	DString *str = DString_New(1);
	DValue val = daoNullString;
	DValue *res;
	val.v.s = str;
	res = DaoContext_SetValue( self, self->vmc->c, val );
	DString_Delete( str );
	if( res ==NULL ) return NULL;
	DString_SetDataMBS( res->v.s, bytes, N );
	return res->v.s;
}
#ifdef DAO_WITH_NUMARRAY
DaoArray* DaoContext_PutArrayInteger( DaoContext *self, int *array, int N )
{
	DaoArray *res = DaoContext_GetArray( self, self->vmc );
	res->numType = DAO_INTEGER;
	if( N ){
		DaoArray_ResizeVector( res, N );
		if( array ) memcpy( res->data.i, array, N*sizeof(int) );
	}else{
		DaoArray_UseData( res, array );
	}
	return res;
}
DaoArray* DaoContext_PutArrayShort( DaoContext *self, short *array, int N )
{
	DaoArray *res = DaoContext_GetArray( self, self->vmc );
	res->numType = DAO_INTEGER;
	DaoArray_ResizeVector( res, N );
	if( array ) for( N--; N>=0; N-- ) res->data.i[N] = array[N];
	return res;
}
DaoArray* DaoContext_PutArrayFloat( DaoContext *self, float *array, int N )
{
	DaoArray *res = DaoContext_GetArray( self, self->vmc );
	res->numType = DAO_FLOAT;
	if( N ){
		DaoArray_ResizeVector( res, N );
		if( array ) memcpy( res->data.f, array, N*sizeof(float) );
	}else{
		DaoArray_UseData( res, array );
	}
	return res;
}
DaoArray* DaoContext_PutArrayDouble( DaoContext *self, double *array, int N )
{
	DaoArray *res = DaoContext_GetArray( self, self->vmc );
	res->numType = DAO_DOUBLE;
	if( N ){
		DaoArray_ResizeVector( res, N );
		if( array ) memcpy( res->data.d, array, N*sizeof(double) );
	}else{
		DaoArray_UseData( res, array );
	}
	return res;
}
DaoArray* DaoContext_PutArrayComplex( DaoContext *self, complex16 *array, int N )
{
	DaoArray *res = DaoContext_GetArray( self, self->vmc );
	res->numType = DAO_COMPLEX;
	DaoArray_ResizeVector( res, N );
	if( N >0 && array ) memcpy( res->data.c, array, N*sizeof(complex16) );
	return res;
}
#else
static void DaoContext_RaiseNoArray( DaoContext *self )
{
	DaoContext_RaiseException( self, DAO_ERROR, "numeric array is disabled" );
}
DaoArray* DaoContext_PutArrayInteger( DaoContext *self, int *array, int N )
{ DaoContext_RaiseNoArray( self ); return 0; }
DaoArray* DaoContext_PutArrayShort( DaoContext *self, short *array, int N )
{ DaoContext_RaiseNoArray( self ); return 0; }
DaoArray* DaoContext_PutArrayFloat( DaoContext *self, float *array, int N )
{ DaoContext_RaiseNoArray( self ); return 0; }
DaoArray* DaoContext_PutArrayDouble( DaoContext *self, double *array, int N )
{ DaoContext_RaiseNoArray( self ); return 0; }
DaoArray* DaoContext_PutArrayComplex( DaoContext *self, complex16 *array, int N )
{ DaoContext_RaiseNoArray( self ); return 0; }
#endif
DaoList* DaoContext_PutList( DaoContext *self )
{
	return DaoContext_GetList( self, self->vmc );
}
DaoMap* DaoContext_PutMap( DaoContext *self )
{
	return DaoContext_GetMap( self, self->vmc );
}
DaoArray* DaoContext_PutArray( DaoContext *self )
{
	return DaoContext_GetArray( self, self->vmc );
}
DaoStream* DaoContext_PutFile( DaoContext *self, FILE *file )
{
	DaoStream *stream = DaoStream_New();
	DaoStream_SetFile( stream, file );
	DaoContext_SetData( self, self->vmc->c, (DaoBase*) stream );
	return stream;
}
DaoCData* DaoContext_PutCData( DaoContext *self, void *data, DaoTypeBase *plgTyper )
{
	DaoCData *cdata = DaoCData_New( plgTyper, data );
	DaoContext_SetData( self, self->vmc->c, (DaoBase*)cdata );
	return cdata;
}
DaoCData* DaoContext_WrapCData( DaoContext *self, void *data, DaoTypeBase *plgTyper )
{
	DaoCData *cdata = DaoCData_Wrap( plgTyper, data );
	DaoContext_SetData( self, self->vmc->c, (DaoBase*)cdata );
	return cdata;
}
DaoCData* DaoContext_PutCPointer( DaoContext *self, void *data, int size )
{
	DaoCData *cptr = DaoCData_New( NULL, NULL );
	cptr->data = data;
	cptr->size = cptr->bufsize = size;
	DaoContext_SetResult( self, (DaoBase*)cptr );
	return cptr;
}
DaoCData*  DaoContext_CopyCData( DaoContext *self, void *d, int n, DaoTypeBase *t )
{
	DaoCData *cdt;
	void *d2 = dao_malloc( n );
	memcpy( d2, d, n );
	cdt = DaoContext_PutCData( self, d2, t );
	cdt->attribs |= DAO_CDATA_FREE;
	return cdt;
}
DaoBase* DaoContext_PutResult( DaoContext *self, DaoBase *data )
{
	DaoContext_SetData( self, self->vmc->c, data );
	return data; /* XXX */
}

/**/
#define STR_LT( x, y ) ( DString_Compare( x, y ) < 0 )
#define STR_LE( x, y ) ( DString_Compare( x, y ) <= 0 )
#define STR_EQ( x, y ) ( DString_Compare( x, y ) == 0 )
#define STR_NE( x, y ) ( DString_Compare( x, y ) != 0 )

DaoVmCode* DaoContext_DoSwitch( DaoContext *self, DaoVmCode *vmc )
{
	DaoVmCode *mid;
	DValue *cst = self->routine->routConsts->data;
	DValue opa = *self->regValues[ vmc->a ];
	int first, last, cmp, id;
	dint min, max;

	if( vmc->c ==0 ) return self->codes + vmc->b;
	if( vmc[1].c == DAO_CASE_TABLE ){
		if( opa.t == DAO_INTEGER ){
			min = cst[ vmc[1].a ].v.i;
			max = cst[ vmc[vmc->c].a ].v.i;
			if( opa.v.i >= min && opa.v.i <= max )
				return self->codes + vmc[ opa.v.i - min + 1 ].b;
		}else if( opa.t == DAO_ENUM ){
			min = cst[ vmc[1].a ].v.e->value;
			max = cst[ vmc[vmc->c].a ].v.e->value;
			if( opa.v.e->value >= min && opa.v.e->value <= max )
				return self->codes + vmc[ opa.v.e->value - min + 1 ].b;
		}
		return self->codes + vmc->b;
	}else if( vmc[1].c == DAO_CASE_UNORDERED ){
		for(id=1; id<=vmc->c; id++){
			mid = vmc + id;
			if( DValue_Compare( opa, cst[ mid->a ] ) ==0 ){
				return self->codes + mid->b;
			}
		}
	}
	first = 1;
	last = vmc->c;
	while( first <= last ){
		id = ( first + last ) / 2;
		mid = vmc + id;
		cmp = DValue_Compare( opa, cst[ mid->a ] );
		if( cmp ==0 ){
			if( cst[mid->a].t == DAO_PAIR ){
				while( id > first && DValue_Compare( opa, cst[ vmc[id-1].a ] ) ==0 ) id --;
				mid = vmc + id;
			}
			return self->codes + mid->b;
		}else if( cmp <0 ){
			last = id - 1;
		}else{
			first = id + 1;
		}
	}
	return self->codes + vmc->b;
}
int DaoObject_InvokeMethod( DaoObject *self, DaoObject *thisObject,
		DaoVmProcess *vmp, DString *name, DaoContext *ctx, DValue par[], int N, int ret );
void DaoContext_DoIter( DaoContext *self, DaoVmCode *vmc )
{
	DValue *va = self->regValues[ vmc->a ];
	DValue *vc = self->regValues[ vmc->c ];
	DString *name = self->process->mbstring;
	DaoTuple *iter;
	DaoTypeBase *typer = DValue_GetTyper( *va );
	DaoFunction *func = NULL;
	DValue val;
	DValue *p[2];
	int rc = 0;

	if( va->t ==0 || va->v.p ==0 ) return;

	if( vc->t != DAO_TUPLE || vc->v.tuple->unitype != dao_type_for_iterator )
		DaoContext_PutTuple( self );

	iter = vc->v.tuple;
	iter->items->data[0].t = DAO_INTEGER;
	iter->items->data[0].v.i = 0;

	p[0] = va;
	p[1] = vc;
	DString_SetMBS( name, "__for_iterator__" );
	if( va->t == DAO_OBJECT ){
		rc = DaoObject_InvokeMethod( va->v.object, NULL, self->process, name, self, vc, 1, vmc->c );
	}else{
		func = DaoFindFunction( typer, name );
		if( func )
			func = (DaoFunction*)DRoutine_GetOverLoad( (DRoutine*)func, va, p+1, 1, 0 );
		if( func ){
			DaoFunction_SimpleCall( func, self, p, 2 );
		}else{
			rc = 1;
		}
	}
	if( rc ) DaoContext_RaiseException( self, DAO_ERROR_FIELD_NOTEXIST, name->mbs );
}

DLong* DaoContext_GetLong( DaoContext *self, DaoVmCode *vmc )
{
	DaoType *tp = self->regTypes[ vmc->c ];
	DValue *dC = self->regValues[ vmc->c ];
	if( dC->t == DAO_LONG ) return dC->v.l;
	if( tp && tp->tid !=DAO_LONG && tp->tid !=DAO_UDF && tp->tid !=DAO_ANY ) return NULL;
	DValue_Clear( dC );
	dC->t = DAO_LONG;
	dC->v.l = DLong_New();
	return dC->v.l;
}
DEnum* DaoContext_GetEnum( DaoContext *self, DaoVmCode *vmc )
{
	DaoType *tp = self->regTypes[ vmc->c ];
	DValue *dC = self->regValues[ vmc->c ];
	if( dC->t == DAO_ENUM ){
		if( tp != dC->v.e->type ) DEnum_SetType( dC->v.e, tp );
		return dC->v.e;
	}
	if( tp && tp->tid !=DAO_ENUM && tp->tid !=DAO_UDF && tp->tid !=DAO_ANY ) return NULL;
	DValue_Clear( dC );
	dC->t = DAO_ENUM;
	dC->v.e = DEnum_New(NULL, 0);
	if( tp ) DEnum_SetType( dC->v.e, tp );
	return dC->v.e;
}
DEnum* DaoContext_PutEnum( DaoContext *self, const char *symbols )
{
	DEnum *denum = DaoContext_GetEnum( self, self->vmc );
	DEnum_SetSymbols( denum, symbols );
	return denum;
}
/**/
DaoList* DaoContext_GetList( DaoContext *self, DaoVmCode *vmc )
{
	/* create a new list in any case. */
	DaoList *list = DaoList_New();
	DaoType *tp = self->regTypes[ vmc->c ];
	if( tp == NULL || tp->tid !=DAO_LIST 
#if 0
		|| NESTYPE( tp, 0 )->tid == DAO_UDF 
#endif
		)
		tp = dao_list_any;
	list->unitype = tp;
	GC_IncRC( tp );
	DaoAssign( self, (DaoBase*) list, self->regValues[ vmc->c ], tp );
	return list;
}
DaoMap* DaoContext_GetMap( DaoContext *self,  DaoVmCode *vmc )
{
	DaoMap *map = DaoMap_New( vmc->code == DVM_HASH );
	DaoType *tp = self->regTypes[ vmc->c ];
	DaoAssign( self, (DaoBase*) map, self->regValues[ vmc->c ], tp );
	if( tp == NULL || tp->tid !=DAO_MAP
			|| NESTYPE( tp, 0 )->tid + NESTYPE( tp, 1 )->tid == DAO_UDF ){
		tp = dao_map_any;
	}else if( NESTYPE( tp, 0 )->tid * NESTYPE( tp, 1 )->tid == DAO_UDF ){
		DMap *map = DMap_New(0,0);
		tp = DaoType_DefineTypes( tp, self->nameSpace, map );
		DMap_Delete( map );
	}
	GC_ShiftRC( tp, map->unitype );
	map->unitype = tp;
	return map;
}

DaoArray* DaoContext_GetArray( DaoContext *self, DaoVmCode *vmc )
{
#ifdef DAO_WITH_NUMARRAY
	DaoType *tp = self->regTypes[ vmc->c ];
	DValue dC = *self->regValues[ vmc->c ];
	int type = DAO_FLOAT;
	DaoArray *array = dC.v.array;
	if( tp && tp->tid == DAO_ARRAY && tp->nested->size ){
		type = tp->nested->items.pType[0]->tid;
		if( type == 0 || type > DAO_COMPLEX ) type = DAO_FLOAT;
	}
	if( dC.t == DAO_ARRAY && array->refCount == 1 ){
		if( array->numType < type ) DaoArray_ResizeVector( array, 0 );
		array->numType = type;
	}else{
		dC.v.array = array = DaoArray_New( type );
		dC.t = DAO_ARRAY;
		DValue_Copy( self->regValues[ vmc->c ], dC );
	}
	if( tp == NULL || tp->tid !=DAO_ARRAY || NESTYPE( tp, 0 )->tid == DAO_UDF )
		tp = dao_array_any;
	array->unitype = tp;
	GC_IncRC( tp );
	return array;
#else
	self->vmc = vmc;
	DaoContext_RaiseException( self, DAO_ERROR, "numeric array is disabled" );
	return NULL;
#endif
}

void DaoContext_DoRange(  DaoContext *self, DaoVmCode *vmc );
void DaoContext_DoList(  DaoContext *self, DaoVmCode *vmc )
{
	DaoNameSpace *ns = self->nameSpace;
	DValue **regValues = self->regValues;
	const ushort_t opA = vmc->a;
	int i;

	if( vmc->b == 0 || vmc->b >= 10 ){
		const int bval = vmc->b ? vmc->b - 10 : 0;
		DaoList *list = DaoContext_GetList( self, vmc );
		DVarray_Resize( list->items, bval, daoNullValue );
		for( i=0; i<bval; i++){
			DaoList_SetItem( list, *regValues[opA+i], i );
			if( list->items->data[i].t == 0 && regValues[opA+i]->t !=0 ){
				DaoContext_RaiseException( self, DAO_ERROR_VALUE, "invalid items" );
				return;
			}
		}

		if( bval >0 && self->regTypes[ vmc->c ] ==NULL ){
			DValue *data = list->items->data;
			DaoType *abtp = DaoNameSpace_GetTypeV( ns, data[0] );
			for(i=1; i<bval; i++){
				DaoType *tp = DaoNameSpace_GetTypeV( ns, data[i] );
				if( DaoType_MatchTo( tp, abtp, 0 ) == 0 ){
					abtp = NULL;
					break;
				}
			}
			if( abtp ){
				DaoType *t = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, & abtp, 1 );
				GC_ShiftRC( t, list->unitype );
				list->unitype = t;
			}
		}
	}else{
		DaoContext_DoRange( self, vmc );
	}
}
static void DaoContext_DoNumRange( DaoContext *self, DaoVmCode *vmc );
void DaoContext_DoArray( DaoContext *self, DaoVmCode *vmc )
{
#ifdef DAO_WITH_NUMARRAY
	const ushort_t opA = vmc->a;
	const ushort_t bval = vmc->b - 10;
	int numtype = DAO_INTEGER;
	size_t i, j, k = 0;
	DaoArray *array = NULL;
	DVarray *tmpArray;
	DArray *dim;
	DValue *telem;

	if( vmc->b < 10 ){
		DaoContext_DoNumRange( self, vmc );
		return;
	}
	dim = DArray_New(0);
	tmpArray = DVarray_New();

	for( j=0; j<bval; j++){
		DValue *p = self->regValues[ opA + j ];
		if( p->t == DAO_LIST ){
			DaoList_FlatList( p->v.list, tmpArray );
		}else{
			DVarray_Append( tmpArray, *p );
		}
	}
	telem = tmpArray->data;
	for(j=0; j<tmpArray->size; j++ ){
		const short type = telem[j].t;
		DaoArray *na = telem[j].v.array;
		k +=  ( type == DAO_ARRAY ) ? na->size : 1;
		if( type == DAO_ARRAY ){
			/* [ [1, 2], [3, 4] ] */
			if( j == 0 ){
				DArray_Assign( dim, na->dims );
			}else{
				size_t m;
				if( dim->size == na->dims->size ){
					for(m=0; m<dim->size; m++){
						if( dim->items.pSize[m] != na->dims->items.pSize[m] ){
							DArray_Delete( dim );
							dim = NULL;
							break;
						}
					}
				}else{
					DArray_Delete( dim );
					dim = NULL;
				}
			}
		}else if( type == DAO_COMPLEX || (type == DAO_ARRAY && na->numType == DAO_COMPLEX) ){
			if( numtype < DAO_COMPLEX ) numtype = DAO_COMPLEX;
		}else if( type ==DAO_INTEGER ||(type ==DAO_ARRAY &&na->numType ==DAO_INTEGER)){
			if( numtype < DAO_INTEGER ) numtype = DAO_INTEGER;
		}else if( type == DAO_FLOAT ||(type == DAO_ARRAY &&na->numType == DAO_FLOAT ) ){
			if( numtype < DAO_FLOAT ) numtype = DAO_FLOAT;
		}else if( type == DAO_DOUBLE ||(type == DAO_ARRAY &&na->numType == DAO_DOUBLE ) ){
			if( numtype < DAO_DOUBLE ) numtype = DAO_DOUBLE;
		}else if( type == DAO_STRING ){
			numtype = DAO_FLOAT;
		}else{
			self->vmc = vmc;
			DaoContext_RaiseException( self, DAO_ERROR_VALUE, "invalid items" );
			DVarray_Delete( tmpArray );
			return;
		}
	}
	array = DaoContext_GetArray( self, vmc );
	array->numType = numtype;
	if( dim ){
		DArray_PushFront( dim, (void*) tmpArray->size );
		DaoArray_ResizeArray( array, dim->items.pSize, dim->size );
		DArray_Delete( dim );
	}else{
		DaoArray_ResizeVector( array, k );
	}
	if( numtype == DAO_INTEGER ){
		int *vals = array->data.i;
		k = 0;
		for( j=0; j<tmpArray->size; j++ ){
			if( telem[j].t == DAO_ARRAY ){
				DaoArray *array2 = telem[j].v.array;
				for(i=0; i<array2->size; i++){
					vals[k] = DaoArray_GetInteger( array2, i );
					k++;
				}
			}else{
				vals[k] = DValue_GetInteger( telem[j] );
				k ++;
			}
		}
	}else if( numtype == DAO_FLOAT ){
		float *vals = array->data.f;
		k = 0;
		for( j=0; j<tmpArray->size; j++ ){
			if( telem[j].t == DAO_ARRAY ){
				DaoArray *array2 = telem[j].v.array;
				for(i=0; i<array2->size; i++){
					vals[k] = DaoArray_GetFloat( array2, i );
					k++;
				}
			}else{
				vals[k] = DValue_GetFloat( telem[j] );
				k ++;
			}
		}
	}else if( numtype == DAO_DOUBLE ){
		double *vals = array->data.d;
		k = 0;
		for( j=0; j<tmpArray->size; j++ ){
			if( telem[j].t == DAO_ARRAY ){
				DaoArray *array2 = telem[j].v.array;
				for(i=0; i<array2->size; i++){
					vals[k] = DaoArray_GetDouble( array2, i );
					k++;
				}
			}else{
				vals[k] = DValue_GetDouble( telem[j] );
				k ++;
			}
		}
	}else{
		complex16 *vals = array->data.c;
		k = 0;
		for( j=0; j<tmpArray->size; j++ ){
			if( telem[j].t == DAO_ARRAY ){
				DaoArray *array2 = telem[j].v.array;
				for(i=0; i<array2->size; i++){
					vals[k] = DaoArray_GetComplex( array2, i );
					k++;
				}
			}else{
				vals[k] = DValue_GetComplex( telem[j] );
				k ++;
			}
		}
	}
	DVarray_Delete( tmpArray );
	array->unitype = self->regTypes[ vmc->c ];
	GC_IncRC( array->unitype );
	if( array->unitype == NULL ){
		array->unitype = DaoNameSpace_GetType( self->nameSpace, (DaoBase*)array );
		GC_IncRC( array->unitype );
	}
#else
	self->vmc = vmc;
	DaoContext_RaiseException( self, DAO_ERROR, "numeric array is disabled" );
#endif
}
void DaoContext_DoRange(  DaoContext *self, DaoVmCode *vmc )
{
	const ushort_t opA = vmc->a;
	const ushort_t bval = vmc->b;

	DaoList *list = DaoContext_GetList( self, vmc );
	DValue **regValues = self->regValues;
	DValue *dn = bval==3 ? regValues[opA+2] : regValues[opA+1];
	int num = (int)DValue_GetDouble( *dn );
	int ta = regValues[ opA ]->t;
	int i;

	self->vmc = vmc;
	if( dn->t < DAO_INTEGER || dn->t > DAO_DOUBLE ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "need number" );
		return;
	}
	if( ta < DAO_INTEGER || ta >= DAO_ARRAY ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, 
				"need data of a primitive type as first value" );
		return;
	}
	if( ( self->vmSpace->options & DAO_EXEC_SAFE ) && num > 1000 ){
		DaoContext_RaiseException( self, DAO_ERROR, "not permitted" );
		return;
	}
	DVarray_Resize( list->items, num, daoNullValue );
	for( i=0; i<num; i++) list->items->data[i].t = ta;

	switch( ta ){
	case DAO_INTEGER :
		{
			const int first = regValues[ vmc->a ]->v.i;
			const int step = bval==2 ? 1: DValue_GetDouble( *regValues[ opA+1 ] );
			for( i=0; i<num; i++) list->items->data[i].v.i = first + i * step;
			break;
		}
	case DAO_FLOAT :
		{
			const float first = regValues[ vmc->a ]->v.f;
			const float step = bval==2 ? 1: DValue_GetDouble( *regValues[ opA+1 ] );
			for( i=0; i<num; i++) list->items->data[i].v.f = first + i * step;
			break;
		}
	case DAO_DOUBLE :
		{
			const double first = regValues[ vmc->a ]->v.f;
			const double step = bval==2 ? 1: DValue_GetDouble( *regValues[ opA+1 ] );
			for( i=0; i<num; i++) list->items->data[i].v.d = first + i * step;
			break;
		}
	case DAO_COMPLEX :
		{
			complex16 first = * regValues[ opA ]->v.c;
			complex16 step = {1,0};
			if( bval >2 ) step = DValue_GetComplex( *regValues[ opA+1 ] );
			for( i=0; i<num; i++){ 
				list->items->data[i].v.c = dao_malloc( sizeof(complex16) );
				list->items->data[i].v.c->real = first.real + i * step.real;
				list->items->data[i].v.c->imag = first.imag + i * step.imag;
			}
			break;
		}
	case DAO_LONG :
		{
			DLong *first = regValues[ opA ]->v.l;
			DLong *buf, *step;
			if( num == 0 ) break;
			buf = step = DLong_New();
			if( bval > 2 ){
				if( regValues[opA+1]->t == DAO_LONG ){
					step = regValues[opA+1]->v.l;
				}else{
					DLong_FromInteger( buf, DValue_GetInteger( *regValues[opA+1] ) );
				}
			}else{
				DLong_FromInteger( step, 1 );
			}
			list->items->data[0].v.l = DLong_New();
			DLong_Move( list->items->data[0].v.l, first );
			for( i=1; i<num; i++){ 
				list->items->data[i].v.l = DLong_New();
				DLong_Add( list->items->data[i].v.l, list->items->data[i-1].v.l, step );
			}
			DLong_Delete( buf );
			break;
		}
	case DAO_STRING :
		{
			DString *first = regValues[ opA ]->v.s;
			DString *one, *step = NULL;
			if( bval > 2 ) step = regValues[ opA+1 ]->v.s; /* XXX check */
			one = DString_Copy( first );
			for( i=0; i<num; i++){
				list->items->data[i].v.s = DString_Copy( one );
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
	if( self->regTypes[ vmc->c ] == NULL ){
		DaoNameSpace *ns = self->nameSpace;
		DaoType *et = DaoNameSpace_GetTypeV( ns, *regValues[opA] );
		DaoType *tp = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, & et, et !=NULL );
		GC_ShiftRC( tp, list->unitype );
		list->unitype = tp;
	}
}
void DaoContext_DoNumRange( DaoContext *self, DaoVmCode *vmc )
{
#ifdef DAO_WITH_NUMARRAY
	const ushort_t opA = vmc->a;
	const ushort_t bval = vmc->b;
	DValue **regValues = self->regValues;
	DValue *dn = bval==3 ? regValues[opA+2] : regValues[opA+1];
	const size_t num = DValue_GetInteger( *dn );
	char type = regValues[ opA ]->t;
	size_t i, j;
	DaoArray *array = NULL;
	DaoArray *a0, *a1;
	DArray *dims;

	self->vmc = vmc;
	if( dn->t < DAO_INTEGER || dn->t > DAO_DOUBLE ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "need number" );
		return;
	}
	if( ( self->vmSpace->options & DAO_EXEC_SAFE ) && num > 1000 ){
		DaoContext_RaiseException( self, DAO_ERROR, "not permitted" );
		return;
	}
	if( bval==3 && type < regValues[ opA+1 ]->t ) type = regValues[ opA+1 ]->t;
	array = DaoContext_GetArray( self, vmc );
	if( type >= DAO_INTEGER && type <= DAO_COMPLEX ){
		array->numType = type;
		DaoArray_ResizeVector( array, num );
	}

	switch( type ){
	case DAO_INTEGER :
		{
			const int first = regValues[ vmc->a ]->v.i;
			const int step = bval==2 ? 1: DValue_GetInteger( *regValues[opA+1]);
			for(i=0; i<num; i++) array->data.i[i] = first + i*step;
			break;
		}
	case DAO_FLOAT :
		{
			const float first = (const float)regValues[ vmc->a ]->v.f;
			const float step = bval==2 ? 1: DValue_GetFloat( *regValues[opA+1]);
			for(i=0; i<num; i++) array->data.f[i] = first + i*step;
			break;
		}
	case DAO_DOUBLE :
		{
			const double first = (const double)regValues[ vmc->a ]->v.d;
			const double step = bval==2 ? 1: DValue_GetDouble( *regValues[opA+1]);
			for(i=0; i<num; i++) array->data.d[i] = first + i*step;
			break;
		}
	case DAO_COMPLEX :
		{
			const complex16 dc = {1,0};
			const complex16 step = (bval==2) ? dc: DValue_GetComplex( *regValues[ opA+1 ] );
			complex16 first = DValue_GetComplex( *regValues[ opA ] );
			for(i=0; i<num; i++){
				array->data.c[i] = first;
				COM_IP_ADD( first, step );
			}
			break;
		}
	case DAO_ARRAY :
		a0 = regValues[ opA ]->v.array;
		array->numType = a0->numType;
		dims = DArray_New(0);
		DArray_Assign( dims, a0->dims );
		DArray_PushFront( dims, (DaoBase*) num );
		DaoArray_ResizeArray( array, dims->items.pSize, dims->size );
		DArray_Delete( dims );
		if( bval ==3 && regValues[opA+1]->t == DAO_ARRAY ){
			a1 = regValues[ opA+1 ]->v.array;
			if( a0->numType <= DAO_DOUBLE && a1->numType >= DAO_COMPLEX ){
				DaoContext_RaiseException( self, DAO_ERROR_TYPE,
						"need real numarray as the step sizes" );
				return;
			}else if( a1->dims->size != a0->dims->size ){
				DaoContext_RaiseException( self, DAO_ERROR_TYPE,
						"need numarray of the same shape" );
				return;
			}else{
				for(i=0; i<a0->dims->size; i++){
					if( a0->dims->items.pSize[i] != a1->dims->items.pSize[i] ){
						DaoContext_RaiseException( self, DAO_ERROR_TYPE,
								"need numarray of the same shape" );
						return;
					}
				}
			}
			switch( a0->numType ){
			case DAO_INTEGER :
				{
					for(i=0; i<num; i++) for(j=0; j<a0->size; j++)
						array->data.i[ i*a0->size + j] = 
							a0->data.i[j] + i*DaoArray_GetInteger( a1, j );
					break;
				}
			case DAO_FLOAT :
				{
					for(i=0; i<num; i++) for(j=0; j<a0->size; j++)
						array->data.f[ i*a0->size + j] = 
							a0->data.f[j] + i*DaoArray_GetFloat( a1, j );
					break;
				}
			case DAO_DOUBLE :
				{
					for(i=0; i<num; i++) for(j=0; j<a0->size; j++)
						array->data.d[ i*a0->size + j] = 
							a0->data.d[j] + i*DaoArray_GetDouble( a1, j );
					break;
				}
			case DAO_COMPLEX :
				{
					if( a1->numType == DAO_COMPLEX ){
						for(i=0; i<num; i++) for(j=0; j<a0->size; j++){
							array->data.c[ i*a0->size + j ].real = 
								a0->data.c[j].real + i*a1->data.c[j].real;
							array->data.c[ i*a0->size + j ].imag = 
								a0->data.c[j].imag + i*a1->data.c[j].imag;
						}
					}else{
						for(i=0; i<num; i++) for(j=0; j<a0->size; j++){
							array->data.c[ i*a0->size + j ].real = 
								a0->data.c[j].real + i*DaoArray_GetDouble( a1, j );
						}
					}
					break;
				}
			default : break;
			}
		}else{
			switch( a0->numType ){
			case DAO_INTEGER :
				{
					const int step = bval==2 ? 1: DValue_GetInteger( *regValues[opA+1]);
					for(i=0; i<num; i++) for(j=0; j<a0->size; j++)
						array->data.i[ i*a0->size + j] = a0->data.i[j] + i * step;
					break;
				}
			case DAO_FLOAT :
				{
					float step = bval==2 ? 1.0: DValue_GetFloat( *regValues[opA+1]);
					for(i=0; i<num; i++) for(j=0; j<a0->size; j++)
						array->data.f[ i*a0->size + j] = a0->data.f[j] + i * step;
					break;
				}
			case DAO_DOUBLE :
				{
					const double step = bval==2 ? 1: DValue_GetDouble( *regValues[opA+1]);
					for(i=0; i<num; i++) for(j=0; j<a0->size; j++)
						array->data.d[ i*a0->size + j] = a0->data.d[j] + i * step;
					break;
				}
			case DAO_COMPLEX :
				{
					complex16 step = { 1.0, 0.0 };
					int offset = 0;
					double stepR = 0.0, stepI = 0.0;
					if( bval == 3 && regValues[ opA+1 ]->t == DAO_COMPLEX )
						step = * regValues[ opA+1 ]->v.c;
					for(i=0; i<num; i++){
						for(j=0; j<a0->size; j++){
							array->data.c[ offset+j ].real = a0->data.c[j].real + stepR;
							array->data.c[ offset+j ].imag = a0->data.c[j].imag + stepI;
						}
						offset += a0->size;
						stepR += step.real;
						stepI += step.imag;
					}
					break;
				}
			default : break;
			}
		}
		break;
	default: break;
	}
	if( self->regTypes[ vmc->c ] ==NULL ){
		DaoType *tp = DaoNameSpace_GetType( self->nameSpace, (DaoBase*)array );
		GC_ShiftRC( tp, array->unitype );
		array->unitype = tp;
	}
	if( ( self->vmSpace->options & DAO_EXEC_SAFE ) && array->size > 5000 ){
		DaoContext_RaiseException( self, DAO_ERROR, "not permitted" );
		return;
	}
#else
	DaoContext_RaiseException( self, DAO_ERROR, "numeric array is disabled" );
#endif
}
void DaoContext_DoMap( DaoContext *self, DaoVmCode *vmc )
{
	int i, c;
	const ushort_t opA = vmc->a;
	const ushort_t bval = vmc->b;
	DaoNameSpace *ns = self->nameSpace;
	DValue **pp = self->regValues;
	DaoMap *map = DaoContext_GetMap( self, vmc );

	if( bval == 2 && pp[opA]->t ==0 && pp[opA+1]->t ==0 ) return;
	for( i=0; i<bval-1; i+=2 ){
		if( (c = DaoMap_Insert( map, *pp[opA+i], *pp[opA+i+1] ) ) ){
			if( c ==1 ){
				DaoContext_RaiseException( self, DAO_ERROR_TYPE, "key not matching" );
			}else if( c ==2 ){
				DaoContext_RaiseException( self, DAO_ERROR_TYPE, "value not matching" );
			}
			break;
		}
	}
	if( bval >0 && self->regTypes[ vmc->c ] ==NULL ){
		/* for constant evaluation only */
		DaoType *any = DaoNameSpace_MakeType( ns, "any", DAO_ANY, 0,0,0 );
		DaoType *t, *tp[2];
		tp[0] = DaoNameSpace_GetTypeV( ns, *pp[opA] );
		tp[1] = DaoNameSpace_GetTypeV( ns, *pp[opA+1] );
		for(i=2; i<bval; i+=2){
			DaoType *tk = DaoNameSpace_GetTypeV( ns, *pp[opA+i] );
			DaoType *tv = DaoNameSpace_GetTypeV( ns, *pp[opA+i+1] );
			if( DaoType_MatchTo( tk, tp[0], 0 )==0 ) tp[0] = any;
			if( DaoType_MatchTo( tv, tp[1], 0 )==0 ) tp[1] = any;
			if( tp[0] ==any && tp[1] ==any ) break;
		}
		t = DaoNameSpace_MakeType( ns, "map", DAO_MAP, NULL, tp, 2 );
		GC_ShiftRC( t, map->unitype );
		map->unitype = t;
	}
}
void DaoContext_DoMatrix( DaoContext *self, DaoVmCode *vmc )
{
#ifdef DAO_WITH_NUMARRAY
	const ushort_t opA = vmc->a;
	const ushort_t bval = ( vmc->b & BITS_LOW12 );
	int i, size, type = DAO_NIL;
	DValue value = daoNullValue;
	DValue **regv = self->regValues;
	DaoArray *array = NULL;

	size_t dim[2];
	dim[0] = (0xff00 & bval)>>8;
	dim[1] = 0xff & bval;
	size = dim[0] * dim[1];
	for( i=0; i<size; i++) if( regv[ opA+i ]->t > type ) type = regv[ opA+i ]->t;
	if( type == DAO_NIL || type > DAO_COMPLEX ){
		DaoContext_RaiseException( self, DAO_ERROR, "invalid matrix enumeration" );
		return;
	}
	if( type == DAO_INTEGER ){
		int *vec;
		array = DaoContext_GetArray( self, vmc );
		array->numType = DAO_INTEGER;
		DaoArray_ResizeArray( array, dim, 2 );
		vec = array->data.i;
		for( i=0; i<size; i++) vec[i] = DValue_GetInteger( *regv[ opA+i ] );
	}else if( type == DAO_FLOAT ){
		float *vec;
		array = DaoContext_GetArray( self, vmc );
		array->numType = DAO_FLOAT;
		DaoArray_ResizeArray( array, dim, 2 );
		vec = array->data.f;
		for( i=0; i<size; i++) vec[i] = DValue_GetFloat( *regv[ opA+i ] );
	}else if( type == DAO_DOUBLE ){
		double *vec;
		array = DaoContext_GetArray( self, vmc );
		array->numType = DAO_DOUBLE;
		DaoArray_ResizeArray( array, dim, 2 );
		vec = array->data.d;
		for( i=0; i<size; i++) vec[i] = DValue_GetDouble( *regv[ opA+i ] );
	}else{
		complex16 *vec;
		array = DaoContext_GetArray( self, vmc );
		array->numType = DAO_COMPLEX;
		DaoArray_ResizeArray( array, dim, 2 );
		vec = array->data.c;
		for( i=0; i<size; i++) vec[i] = DValue_GetComplex( *regv[ opA+i ] );
		type = DAO_COMPLEX;
	}
	value.t = type;
	if( self->regTypes[ vmc->c ] ==NULL ){
		DaoType *tp = DaoNameSpace_GetTypeV( self->nameSpace, value );
		tp = DaoNameSpace_MakeType( self->nameSpace, "array", DAO_ARRAY, NULL, &tp, 1 );
		GC_ShiftRC( tp, array->unitype );
		array->unitype = tp;
	}
#else
	self->vmc = vmc;
	DaoContext_RaiseException( self, DAO_ERROR, "numeric array is disabled" );
#endif
}

static DValue DaoTypeCast( DaoContext *ctx, DaoType *ct, DValue dA, complex16 *cb, DLong *lb, DString *sb );

static DaoTuple* DaoContext_GetTuple( DaoContext *self, DaoType *type, int size )
{
	int opc = self->vmc->c;
	DValue *val = self->regValues[ opc ];
	DaoTuple *tup = val->v.tuple;
	if( val->t == DAO_TUPLE && tup->refCount ==1 && tup->unitype == type ) return tup;
	DValue_Clear( val );
	tup = DaoTuple_New( size );
	tup->unitype = type;
	GC_IncRC( type );
	GC_IncRC( tup );
	val->v.tuple = tup;
	val->t = DAO_TUPLE;
	return tup;
}
DaoTuple* DaoContext_PutTuple( DaoContext *self )
{
	DaoType *type = self->regTypes[ self->vmc->c ];
	if( type == NULL || type->tid != DAO_TUPLE ) return NULL;
	return DaoContext_GetTuple( self, type, type->nested->size );
}

void DaoContext_MakeTuple( DaoContext *self, DaoTuple *tuple, DValue **its, int N )
{
	DaoType *tp, *ct = tuple->unitype;
	DValue val, val2;
	DNode *node;
	int i;
	if( ct == NULL ) return;
	if( ct->nested ==NULL || ct->nested->size != N ){
		DaoContext_RaiseException( self, DAO_ERROR, "invalid tuple enumeration" );
		return;
	}
	for(i=0; i<N; i++){
		val = *its[i];
		if( val.t == DAO_PAR_NAMED ){
			DaoPair *pair = (DaoPair*) val.v.p;
			val2 = pair->first;
			node = MAP_Find( ct->mapNames, val2.v.s );
			if( node == NULL || node->value.pInt != i ){
				DaoContext_RaiseException( self, DAO_ERROR, "name not matched" );
				return;
			}
			val = pair->second;
		}
		tp = ct->nested->items.pType[i];
		if( tp->tid == DAO_PAR_NAMED || tp->tid == DAO_PAR_DEFAULT ) tp = tp->X.abtype;
		if( DValue_Move( val, tuple->items->data+i, tp ) == 0){
			DaoContext_RaiseException( self, DAO_ERROR, "invalid tuple enumeration" );
			return;
		}
	}
}
void DaoContext_DoCurry( DaoContext *self, DaoVmCode *vmc )
{
	int i, k;
	int opA = vmc->a;
	int bval = ( vmc->b & BITS_LOW12 );
	DaoObject *object;
	DaoType **mtype;
	const DValue *selfobj = & daoNullValue;
	DValue *p = self->regValues[opA];
	DNode *node;

	if( vmc->code == DVM_MCURRY ){
		if( p->t == DAO_ROUTINE ){
			/* here it is not convenient to check attribute for DAO_ROUT_NEEDSELF,
			 * because the routine may have not been compiled, 
			 * worse it may have overloaded routine, some may NEEDSELF, some may not. */
			if( p->v.routine->tidHost == DAO_OBJECT ) selfobj = self->regValues[opA+1];
		}else if( p->t == DAO_ROUTINE || p->t == DAO_FUNCTION ){
			DRoutine *rout = (DRoutine*) p->v.p;
			if( rout->attribs & DAO_ROUT_PARSELF ) selfobj = self->regValues[opA+1];
		}
		opA ++;
		bval --;
	}

	self->vmc = vmc;
	switch( p->t ){
	case DAO_CLASS :
		{
			DaoClass *klass = p->v.klass;
			object = DaoObject_New( klass, NULL, 0 );
			DaoContext_SetData( self, vmc->c, (DaoBase*)object );
			mtype = klass->objDataType->items.pType;
			if( bval >= object->objData->size ){
				DaoContext_RaiseException( self, DAO_ERROR, "enumerating too many members" );
				break;
			}
			for( i=0; i<bval; i++){
				k = i+1; /* skip self */
				p = self->regValues[opA+i+1];
				if( p->t == DAO_PAR_NAMED ){
					DaoPair *pair = p->v.pair;
					node = DMap_Find( klass->lookupTable, pair->first.v.s );
					if( node == NULL || LOOKUP_ST( node->value.pSize ) != DAO_OBJECT_VARIABLE ){
						DaoContext_RaiseException( self, DAO_ERROR_FIELD_NOTEXIST, "" );
						break;
					}
					k = LOOKUP_ID( node->value.pSize );
					p = & pair->second;
				}
				if( DValue_Move( *p, object->objValues + k, mtype[k] ) ==0 ){
					DaoType *type = DaoNameSpace_GetTypeV( self->nameSpace, *p );
					DaoContext_RaiseTypeError( self, type, mtype[k], "moving" );
					break;
				}
			}
			break;
		}
	case DAO_ROUTINE :
	case DAO_FUNCTION :
	case DAO_CONTEXT :
	case DAO_PAIR :
		{
			DaoFunCurry *curry = DaoFunCurry_New( *p, *selfobj );
			DaoContext_SetData( self, vmc->c, (DaoBase*)curry );
			for( i=0; i<bval; i++) DVarray_Append( curry->params, *self->regValues[opA+i+1] );
			break;
		}
	case DAO_FUNCURRY :
		{
			DaoFunCurry *curry = (DaoFunCurry*) p->v.p;
			DaoContext_SetData( self, vmc->c, (DaoBase*)curry );
			for( i=0; i<bval; i++){
				DVarray_Append( curry->params, *self->regValues[opA+i+1] );
			}
			break;
		}
	case DAO_TYPE :
		{
			DaoType *type = (DaoType*) p->v.p;
			DaoTuple *tuple = DaoContext_GetTuple( self, type, vmc->b );
			if( type->tid != DAO_TUPLE ){
				DaoContext_RaiseException( self, DAO_ERROR, "invalid enumeration" );
				break;
			}
			DaoContext_SetData( self, vmc->c, (DaoBase*) tuple );
			DaoContext_MakeTuple( self, tuple, self->regValues + vmc->a +1, vmc->b );
			break;
		}
	default :
		DaoContext_RaiseException( self, DAO_ERROR, "invalid enumeration" );
		break;
	}
}

void DaoContext_BindNameValue( DaoContext *self, DaoVmCode *vmc )
{
	DValue dA = self->routine->routConsts->data[ vmc->a ];
	DValue dB = *self->regValues[ vmc->b ];
	DaoPair *pair = DaoPair_New( dA, dB );
	pair->type = DAO_PAR_NAMED;
	pair->unitype = self->regTypes[ vmc->c ];
	if( pair->unitype == NULL ){
		DaoNameSpace *ns = self->nameSpace;
		DaoType *tp = DaoNameSpace_GetTypeV( ns, pair->second );
		pair->unitype = DaoNameSpace_MakeType( ns, dA.v.s->mbs, DAO_PAR_NAMED, (DaoBase*)tp, NULL, 0 );
	}
	GC_IncRC( pair->unitype );
	DaoContext_SetData( self, vmc->c, (DaoBase*) pair );
}
void DaoContext_DoPair( DaoContext *self, DaoVmCode *vmc )
{
	DaoType *tp[2];
	DValue dA = *self->regValues[ vmc->a ];
	DValue dB = *self->regValues[ vmc->b ];
	DaoPair *pair = DaoPair_New( dA, dB );
	pair->unitype = self->regTypes[ vmc->c ];
	if( pair->unitype == NULL ){
		tp[0] = DaoNameSpace_GetTypeV( self->nameSpace, pair->first );
		tp[1] = DaoNameSpace_GetTypeV( self->nameSpace, pair->second );
		pair->unitype = DaoNameSpace_MakeType( self->nameSpace, "pair", DAO_PAIR, NULL, tp, 2 );
	}
	GC_IncRC( pair->unitype );
	DaoContext_SetData( self, vmc->c, (DaoBase*) pair );
}
void DaoContext_DoTuple( DaoContext *self, DaoVmCode *vmc )
{
	DaoTuple *tuple;
	DaoType *tp, *ct = self->regTypes[ vmc->c ];
	DaoNameSpace *ns = self->nameSpace;
	DValue val, val2;
	int i;

	self->vmc = vmc;
	tuple = DaoContext_GetTuple( self, ct, vmc->b );
	if( ct == NULL ){
		ct = DaoType_New( "tuple<", DAO_TUPLE, NULL, NULL );
		for(i=0; i<vmc->b; i++){
			val = *self->regValues[ vmc->a + i ];
			tp = DaoNameSpace_GetTypeV( ns, val );
			if( tp == NULL ) tp = DaoNameSpace_GetType( ns, & nil );
			if( i >0 ) DString_AppendMBS( ct->name, "," );
			if( tp->tid == DAO_PAR_NAMED ){
				DaoPair *pair = (DaoPair*) val.v.p;
				val2 = pair->first;
				if( ct->mapNames == NULL ) ct->mapNames = DMap_New(D_STRING,0);
				MAP_Insert( ct->mapNames, val2.v.s, i );
				DString_Append( ct->name, val2.v.s );
				DString_AppendMBS( ct->name, ":" );
				DString_Append( ct->name, tp->X.abtype->name );
				val = pair->second;
			}else{
				DString_Append( ct->name, tp->name );
			}
			DArray_Append( ct->nested, tp );
			DaoTuple_SetItem( tuple, val, i );
		}
		DString_AppendMBS( ct->name, ">" );
		GC_IncRCs( ct->nested );
		tp = DaoNameSpace_FindType( ns, ct->name );
		if( tp ){
			DaoType_Delete( ct );
			ct = tp;
		}else{
			DaoType_CheckAttributes( ct );
			DaoNameSpace_AddType( ns, ct->name, ct );
		}
		tuple->unitype = ct;
		GC_IncRC( ct );
	}else{
		DaoContext_MakeTuple( self, tuple, self->regValues + vmc->a, vmc->b );
	}
}
void DaoContext_DoCheck( DaoContext *self, DaoVmCode *vmc )
{
	DValue dA = *self->regValues[ vmc->a ];
	DValue dB = *self->regValues[ vmc->b ];
	DaoType *type = (DaoType*) dB.v.p;
	dint *res = 0;
	self->vmc = vmc;
	res = DaoContext_PutInteger( self, 0 );
	if( dA.t && dB.t == DAO_TYPE && type->tid == DAO_TYPE ){
		type = type->nested->items.pType[0];
		if( dA.t < DAO_ARRAY ){
			*res = dA.t == type->tid;
		}else if( dA.t == DAO_OBJECT ){
			dA.v.object = dA.v.object->that;
			*res = DaoType_MatchValue( type, dA, NULL ) != 0;
		}else{
			*res = DaoType_MatchValue( type, dA, NULL ) != 0;
		}
	}else if( dA.t == dB.t ){
		*res = 1;
		if( dA.t == DAO_OBJECT ){
			*res = dA.v.object->that->myClass == dB.v.object->that->myClass;
		}else if( dA.t == DAO_CDATA ){
			*res = dA.v.cdata->typer == dB.v.cdata->typer;
		}else if( dA.t >= DAO_ARRAY && dA.t <= DAO_TUPLE ){
			DaoType *t1 = NULL;
			DaoType *t2 = NULL;
			*res = 0;
			switch( dA.t ){
			case DAO_ARRAY : t1 = dA.v.array->unitype; t2 = dB.v.array->unitype; break;
			case DAO_LIST : t1 = dA.v.list->unitype; t2 = dB.v.list->unitype; break;
			case DAO_MAP : t1 = dA.v.map->unitype; t2 = dB.v.map->unitype; break;
			case DAO_PAIR : t1 = dA.v.pair->unitype; t2 = dB.v.pair->unitype; break;
			case DAO_TUPLE : t1 = dA.v.tuple->unitype; t2 = dB.v.tuple->unitype; break;
			default : break;
			}
			*res = DaoType_MatchTo( t1, t2, NULL ) == DAO_MT_EQ;
		}
	}
}
void DaoContext_DoGetItem( DaoContext *self, DaoVmCode *vmc )
{
	int id;
	DValue q;
	DValue *p = self->regValues[ vmc->a ];
	DaoTypeCore *tc = DValue_GetTyper( * p )->priv;
	DaoType *ct = self->regTypes[ vmc->c ];

	self->vmc = vmc;
	if( p->t == 0 ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "on null object" );
		return;
	}
	q = *self->regValues[ vmc->b ];
	if( p->t == DAO_LIST && ( q.t >= DAO_INTEGER && q.t <= DAO_DOUBLE ) ){
		DaoList *list = p->v.list;
		id = DValue_GetInteger( q );
		if( id >=0 && id < list->items->size ){
			self->regValues[ vmc->c ] = list->items->data + id;
		}else{
			DaoContext_RaiseException( self, DAO_ERROR, "index out of range" );
			return;
		}
#ifdef DAO_WITH_NUMARRAY
	}else if( p->t == DAO_ARRAY && ( q.t >=DAO_INTEGER && q.t <=DAO_DOUBLE )){
		DaoArray *na = p->v.array;
		id = DValue_GetInteger( q );
		if( id < 0 || id >= na->size ){
			DaoContext_RaiseException( self, DAO_ERROR_INDEX_OUTOFRANGE, "" );
			return;
		}
		switch( na->numType ){
		case DAO_INTEGER : q.t = DAO_INTEGER; q.v.i = na->data.i[id]; break;
		case DAO_FLOAT   : q.t = DAO_FLOAT;   q.v.f = na->data.f[id];  break;
		case DAO_DOUBLE  : q.t = DAO_DOUBLE;  q.v.d = na->data.d[id];  break;
		case DAO_COMPLEX : q.t = DAO_COMPLEX; q.v.c[0] = na->data.c[id]; break;
		default : break;
		}
		DaoMoveAC( self, q, self->regValues[ vmc->c ], ct );
#endif
	}else{
		tc->GetItem( p, self, q );
	}
}
void DaoContext_DoGetField( DaoContext *self, DaoVmCode *vmc )
{
	DValue *p = self->regValues[ vmc->a ];
	DaoTypeCore *tc = DValue_GetTyper( * p )->priv;

	self->vmc = vmc;
	if( p->t == 0 ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "on null object" );
		return;
	}
	tc->GetField( p, self, self->routine->routConsts->data[ vmc->b].v.s );
}
static DaoMap* DaoGetMetaMap( DValue *self, int create )
{
	DaoMap *meta = NULL;
	switch( self->t ){
	case DAO_ARRAY : meta = self->v.array->meta; break;
	case DAO_LIST  : meta = self->v.list->meta; break;
	case DAO_TUPLE : meta = self->v.tuple->meta; break;
	case DAO_MAP   : meta = self->v.map->meta; break;
	case DAO_CDATA : meta = self->v.cdata->meta; break;
	case DAO_OBJECT : meta = self->v.object->meta; break;
	default : break;
	}
	if( meta || create ==0 ) return meta;
	switch( self->t ){
	case DAO_ARRAY : meta = self->v.array->meta = DaoMap_New(1); break;
	case DAO_LIST  : meta = self->v.list->meta = DaoMap_New(1); break;
	case DAO_TUPLE : meta = self->v.tuple->meta = DaoMap_New(1); break;
	case DAO_MAP   : meta = self->v.map->meta = DaoMap_New(1); break;
	case DAO_CDATA : meta = self->v.cdata->meta = DaoMap_New(1); break;
	case DAO_OBJECT : meta = self->v.object->meta = DaoMap_New(1); break;
	default : break;
	}
	if( meta ){
		meta->unitype = dao_map_meta;
		GC_IncRC( meta );
		GC_IncRC( dao_map_meta );
	}
	return meta;
}
static DValue* DaoMap_GetMetaField( DaoMap *self, DValue key )
{
	DNode *node = DMap_Find( self->items, & key );
	if( node ) return node->value.pValue;
	if( self->meta ) return DaoMap_GetMetaField( self->meta, key );
	return NULL;
}
void DaoContext_DoGetMetaField( DaoContext *self, DaoVmCode *vmc )
{
	DValue *p = self->regValues[ vmc->a ];
	DaoMap *meta = p->t == DAO_MAP ? p->v.map : DaoGetMetaMap( p, 0 );
	DValue *value;

	self->vmc = vmc;
	if( meta == NULL ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "object has no meta fields" );
		return;
	}
	value = DaoMap_GetMetaField( meta, self->routine->routConsts->data[ vmc->b] );
	if( value == NULL ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "meta field not exists" );
		return;
	}
	self->vmc = vmc;
	DaoContext_PutValue( self, *value );
}
void DaoContext_DoSetItem( DaoContext *self, DaoVmCode *vmc )
{
	DValue q, v;
	DValue *p = self->regValues[ vmc->c ];
	DaoTypeCore *tc = DValue_GetTyper( * p )->priv;
	int id;

	self->vmc = vmc;
	v = *self->regValues[ vmc->a ];
	if( p->t == 0 ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "on null object" );
		return;
	}

	q = *self->regValues[ vmc->b ];
	if( p->t == DAO_LIST && q.t == DAO_INTEGER ){
		DaoList_SetItem( p->v.list, v, q.v.i );
	}else if( p->t == DAO_LIST && q.t == DAO_FLOAT ){
		DaoList_SetItem( p->v.list, v, (int) q.v.f );
	}else if( p->t == DAO_LIST && q.t == DAO_DOUBLE ){
		DaoList_SetItem( p->v.list, v, (int) q.v.d );
#ifdef DAO_WITH_NUMARRAY
	}else if( p->t == DAO_ARRAY && ( q.t >=DAO_INTEGER && q.t <=DAO_DOUBLE )
			&& ( v.t >=DAO_INTEGER && v.t <=DAO_DOUBLE ) ){
		DaoArray *na = p->v.array;
		double val = DValue_GetDouble( v );
		complex16 cpx = DValue_GetComplex( v );
		id = DValue_GetDouble( q );
		if( id < 0 || id >= na->size ){
			DaoContext_RaiseException( self, DAO_ERROR_INDEX_OUTOFRANGE, "" );
			return;
		}
		switch( na->numType ){
		case DAO_INTEGER : na->data.i[ id ] = (int) val; break;
		case DAO_FLOAT  : na->data.f[ id ] = (float) val; break;
		case DAO_DOUBLE : na->data.d[ id ] = val; break;
		case DAO_COMPLEX : na->data.c[ id ] = cpx; break;
		default : break;
		}
#endif
	}else{
		tc->SetItem( p, self, q, v );
	}
}
void DaoContext_DoSetField( DaoContext *self, DaoVmCode *vmc )
{
	DValue *p = self->regValues[ vmc->c ];
	DaoTypeCore *tc = DValue_GetTyper( * p )->priv;
	DValue v;

	self->vmc = vmc;
	v = *self->regValues[ vmc->a ];
	if( p->t == 0 ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "on null object" );
		return;
	}
	tc->SetField( p, self, self->routine->routConsts->data[ vmc->b ].v.s, v );
}
void DaoContext_DoSetMetaField( DaoContext *self, DaoVmCode *vmc )
{
	DValue *p = self->regValues[ vmc->c ];
	DaoMap *meta = DaoGetMetaMap( p, 1 );
	DValue fname = self->routine->routConsts->data[ vmc->b ];
	DValue v = *self->regValues[ vmc->a ];
	DValue *value;
	int c = 1;

	self->vmc = vmc;
	if( meta == NULL ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "object can not have meta fields" );
		return;
	}
	if( p->t == DAO_MAP ) c = DaoMap_Insert( p->v.map, fname, v );
	if( c ) DaoMap_Insert( meta, fname, v );
	if( v.t == DAO_MAP && strcmp( fname.v.s->mbs, "__proto__" ) ==0 ){
		GC_ShiftRC( v.v.map, meta->meta );
		meta->meta = v.v.map;
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
int DaoContext_TryObjectArith( DaoContext *self, DValue *A, DValue *B, DValue *C )
{
	DaoRoutine *rout = 0;
	DaoContext *ctx;
	DaoObject *object;
	DaoClass *klass;
	DRoutine *drout;
	DString *name = self->process->mbstring;
	DValue **p, *par[3];
	DValue value;
	int code = self->vmc->code;
	int boolres = code >= DVM_AND && code <= DVM_NE;
	int bothobj = B ? A->t == B->t : 0;
	int recursive = 0;
	int overloaded = 0;
	int compo = 0; /* try composite operator */
	int nopac = 0; /* do not pass C as parameter */
	int npar = 2;
	int rc, n;

	if( C == A && B && daoBitBoolArithOpers2[ code-DVM_MOVE ] ){
		/* C += B, or C = C + B */
		par[0] = C;
		par[1] = B;
		compo = 1;
	}else if( C && B && A ){ /* C = A + B */
		par[0] = C;
		par[1] = A;
		par[2] = B;
		npar = 3;
	}else if( B == NULL ){ /* C = ! A */
		par[0] = C;
		par[1] = A;
	}
	nopac = C->t ==0 || (C->t >= DAO_ARRAY && C->v.p->refCount >1);
	p = par;
	n = npar;

TryAgain:
	if( compo )
		DString_SetMBS( name, daoBitBoolArithOpers2[ code-DVM_MOVE ] );
	else
		DString_SetMBS( name, daoBitBoolArithOpers[ code-DVM_MOVE ] );
	if( DString_EQ( name, self->routine->routName ) ) recursive = 1;

	object = A->t == DAO_OBJECT ? A->v.object : B->v.object;
	klass = object->myClass;
	overloaded = klass->attribs & DAO_OPER_OVERLOADED;

	rc = DaoObject_GetData( object, name, & value,  self->object, NULL );
	if( rc && (code == DVM_LT || code == DVM_LE) ){
		if( code == DVM_LT ){
			DString_SetMBS( name, ">" );
		}else{
			DString_SetMBS( name, ">=" );
		}
		par[1] = B;
		par[2] = A;
		rc = DaoObject_GetData( object, name, & value,  self->object, NULL );
		if( DString_EQ( name, self->routine->routName ) ) recursive = 1;
	}
	if( bothobj && boolres && recursive ) return 0;
	if( rc || (value.t != DAO_ROUTINE && value.t != DAO_FUNCTION) ){
		if( bothobj && boolres && overloaded ==0 ) return 0;
		goto ArithError;
	}
	if( compo ==0 ){
		p = par + nopac;
		n = npar - nopac;
	}
	drout = (DRoutine*) value.v.routine;
	drout = DRoutine_GetOverLoad( drout, NULL, p, n, DVM_CALL );
	if( drout == NULL )  goto ArithError;

	if( drout->type == DAO_ROUTINE ){
		rout = (DaoRoutine*) drout;
		ctx = DaoVmProcess_MakeContext( self->process, rout );

		if( ! DRoutine_PassParams( drout, NULL, ctx->regValues, p, NULL, n, DVM_CALL ) ) goto ArithError;

		DaoVmProcess_PushContext( self->process, ctx );
		ctx->process->topFrame->returning = self->vmc->c;
	}else{
		DaoFunction_SimpleCall( (DaoFunction*)drout, self, p, n );
	}
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
	DaoContext_RaiseException( self, DAO_ERROR_TYPE, "" );
	return 0;
}
/* Similar to DaoContext_TryObjectArith(), 
   but without consideration for recursion. */
int DaoContext_TryCDataArith( DaoContext *self, DValue *A, DValue *B, DValue *C )
{
	DaoCData *cdata = NULL;
	DaoFunction *func;
	DRoutine *drout;
	DValue **p, *par[3];
	DString *name = self->process->mbstring;
	int code = self->vmc->code;
	int boolres = code >= DVM_AND && code <= DVM_NE;
	int bothobj = B ? A->t == B->t : 0;
	int overloaded = 0;
	int compo = 0; /* try composite operator */
	int nopac = 0; /* do not pass C as parameter */
	int n, npar = 2;

	if( C == A && B && daoBitBoolArithOpers2[ code-DVM_MOVE ] ){
		/* C += B, or C = C + B */
		par[0] = C;
		par[1] = B;
		compo = 1;
	}else if( C && B && A ){ /* C = A + B */
		par[0] = C;
		par[1] = A;
		par[2] = B;
		npar = 3;
	}else if( B == NULL ){ /* C = ! A */
		par[0] = C;
		par[1] = A;
	}
	nopac = C->t ==0 || (C->t >= DAO_ARRAY && C->v.p->refCount >1);
	p = par;
	n = npar;

TryAgain:
	if( compo )
		DString_SetMBS( name, daoBitBoolArithOpers2[ code-DVM_MOVE ] );
	else
		DString_SetMBS( name, daoBitBoolArithOpers[ code-DVM_MOVE ] );

	cdata = A->t == DAO_CDATA ? A->v.cdata : B->v.cdata;
	overloaded = cdata->typer->priv->attribs & DAO_OPER_OVERLOADED;
	func = DaoFindFunction( (DaoTypeBase*) cdata->typer, name );
	if( func ==NULL && (code == DVM_LT || code == DVM_LE) ){
		if( code == DVM_LT ){
			DString_SetMBS( name, ">" );
		}else{
			DString_SetMBS( name, ">=" );
		}
		par[1] = B;
		par[2] = A;
		func = DaoFindFunction( (DaoTypeBase*) cdata->typer, name );
	}
	if( func == NULL ){
		if( bothobj && boolres ) return 0;
		goto ArithError;
	}
	if( compo ==0 ){
		p = par + nopac;
		n = npar - nopac;
	}
	drout = (DRoutine*) func;
	drout = DRoutine_GetOverLoad( drout, NULL, p, n, DVM_CALL );
	if( drout == NULL )  goto ArithError;

	DaoFunction_SimpleCall( (DaoFunction*)drout, self, p, n );
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
	DaoContext_RaiseException( self, DAO_ERROR_TYPE, "" );
	return 0;
}
static void DaoContext_LongDiv ( DaoContext *self, DLong *z, DLong *x, DLong *y, DLong *r )
{
	if( x->size ==0 || (x->size ==1 && x->data[0] ==0) ){
		self->idClearFE = self->vmc - self->codes;
		DaoContext_RaiseException( self, DAO_ERROR_FLOAT_DIVBYZERO, "" );
		return;
	}
	DLong_Div( z, x, y, r );
}
static int DaoContext_CheckLong2Integer( DaoContext *self, DLong *x )
{
	short d = 8*sizeof(dint);
	if( x->size * LONG_BITS < d ) return 1;
	if( (x->size - 1) * LONG_BITS >= d ) goto RaiseInexact;
	d -= (x->size - 1) * LONG_BITS + 1; /* one bit for sign */
	if( (x->data[ x->size - 1 ] >> d) > 0 ) goto RaiseInexact;
	return 1;
RaiseInexact:
	self->idClearFE = self->vmc - self->codes;
	DaoContext_RaiseException( self, DAO_ERROR_VALUE,
			"long integer value is too big for the operation" );
	return 0;
}
void DaoContext_DoBinArith( DaoContext *self, DaoVmCode *vmc )
{
	DValue *A = self->regValues[ vmc->a ];
	DValue *B = self->regValues[ vmc->b ];
	DValue *C = self->regValues[ vmc->c ];
	DValue dA = *A, dB = *B;
	self->vmc = vmc;
	if( (dA.t ==0 && dA.v.p ==NULL) || (dB.t ==0 && dB.v.p ==NULL) ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "on null object" );
		return;
	}

	if( dA.t == DAO_OBJECT || dB.t == DAO_OBJECT ){
		self->vmc = vmc;
		DaoContext_TryObjectArith( self, A, B, C );
		return;
	}else if( dA.t == DAO_CDATA || dB.t == DAO_CDATA ){
		self->vmc = vmc;
		DaoContext_TryCDataArith( self, A, B, C );
		return;
	}

	if( dA.t >= DAO_INTEGER && dA.t <= DAO_DOUBLE && dB.t >= DAO_INTEGER && dB.t <= DAO_DOUBLE ){
		int type = dA.t > dB.t ? dA.t : dB.t;
		DValue val = daoNullValue;
		double res = 0;
		llong_t ib;
		switch( vmc->code ){
		case DVM_MOD:
			ib = DValue_GetLongLong( dB );
			if( ib ==0 ){
				self->idClearFE = self->vmc - self->codes;
				DaoContext_RaiseException( self, DAO_ERROR_FLOAT_DIVBYZERO, "" );
			}
			res = DValue_GetLongLong( dA ) % ib;
			break;
		case DVM_ADD: res = DValue_GetDouble( dA ) + DValue_GetDouble( dB ); break;
		case DVM_SUB: res = DValue_GetDouble( dA ) - DValue_GetDouble( dB ); break;
		case DVM_MUL: res = DValue_GetDouble( dA ) * DValue_GetDouble( dB ); break;
		case DVM_DIV: res = DValue_GetDouble( dA ) / DValue_GetDouble( dB ); break;
		case DVM_POW: res = powf( DValue_GetDouble( dA ), DValue_GetDouble( dB ) ); break;
		default : break;
		}
		val.t = type;
		switch( type ){
		case DAO_INTEGER: val.v.i = res; break;
		case DAO_FLOAT :  val.v.f = res; break;
		case DAO_DOUBLE : val.v.d = res; break;
		default : val.t = 0;  break;
		}
		DaoContext_SetValue( self, vmc->c, val );
		return;
	}else if( dB.t >=DAO_INTEGER && dB.t <=DAO_DOUBLE && dA.t ==DAO_COMPLEX ){
		double f = DValue_GetDouble( dB );
		complex16 res;
		DValue val = daoNullComplex;
		val.v.c = & res;
		switch( vmc->code ){
		case DVM_ADD: res.real = dA.v.c->real + f; res.imag = dA.v.c->imag; break;
		case DVM_SUB: res.real = dA.v.c->real - f; res.imag = dA.v.c->imag; break;
		case DVM_MUL: res.real = dA.v.c->real * f; res.imag = dA.v.c->imag * f; break;
		case DVM_DIV: res.real = dA.v.c->real / f; res.imag = dA.v.c->imag / f; break;
					  /* XXX: pow for complex??? */
		default: break;
		}
		DaoContext_SetValue( self, vmc->c, val );
	}else if( dA.t >=DAO_INTEGER && dA.t <=DAO_DOUBLE && dB.t ==DAO_COMPLEX ){
		double n, f = DValue_GetDouble( dA );
		complex16 res;
		DValue val = daoNullComplex;
		val.v.c = & res;
		switch( vmc->code ){
		case DVM_ADD:
			res.real = f + dB.v.c->real;  res.imag = dB.v.c->imag;
			break;
		case DVM_SUB:
			res.real = f - dB.v.c->real;  res.imag = - dB.v.c->imag;
			break;
		case DVM_MUL:
			res.real = f * dB.v.c->real;  res.imag = f * dB.v.c->imag;
			break;
		case DVM_DIV:
			n = dB.v.c->real * dB.v.c->real + dB.v.c->imag * dB.v.c->imag;
			res.real = f * dB.v.c->real /n;
			res.imag = f * dB.v.c->imag /n;
			break;
			/* XXX: pow for complex??? */
		default: break;
		}
		DaoContext_SetValue( self, vmc->c, val );
	}else if( dA.t == DAO_COMPLEX && dB.t == DAO_COMPLEX ){
		double n = 0;
		complex16 res;
		DValue val = daoNullComplex;
		val.v.c = & res;
		switch( vmc->code ){
		case DVM_ADD:
			res.real = dA.v.c->real + dB.v.c->real;
			res.imag = dA.v.c->imag + dB.v.c->imag;
			break;
		case DVM_SUB:
			res.real = dA.v.c->real - dB.v.c->real;
			res.imag = dA.v.c->imag - dB.v.c->imag;
			break;
		case DVM_MUL:
			res.real = dA.v.c->real * dB.v.c->real - dA.v.c->imag * dB.v.c->imag;
			res.imag = dA.v.c->real * dB.v.c->imag + dA.v.c->imag * dB.v.c->real;
			break;
		case DVM_DIV:
			n = dB.v.c->real * dB.v.c->real + dB.v.c->imag * dB.v.c->imag;
			res.real = (dA.v.c->real * dB.v.c->real + dA.v.c->imag * dB.v.c->imag) /n;
			res.imag = (dA.v.c->real * dB.v.c->imag - dA.v.c->imag * dB.v.c->real) /n;
			break;
			/* XXX: pow for complex??? */
		default: break;
		}
		DaoContext_SetValue( self, vmc->c, val );
	}else if( dA.t == DAO_LONG && dB.t == DAO_LONG ){
		DLong *b, *c = DaoContext_GetLong( self, vmc );
		if( vmc->code == DVM_POW && DaoContext_CheckLong2Integer( self, dB.v.l ) == 0 ) return;
		b = DLong_New();
		switch( vmc->code ){
		case DVM_ADD : DLong_Add( c, dA.v.l, dB.v.l ); break;
		case DVM_SUB : DLong_Sub( c, dA.v.l, dB.v.l ); break;
		case DVM_MUL : DLong_Mul( c, dA.v.l, dB.v.l ); break;
		case DVM_DIV : DaoContext_LongDiv( self, dA.v.l, dB.v.l, c, b ); break;
		case DVM_MOD : DaoContext_LongDiv( self, dA.v.l, dB.v.l, b, c ); break;
		case DVM_POW : DLong_Pow( c, dA.v.l, DLong_ToInteger( dB.v.l ) ); break;
		default : break;
		}
		DLong_Delete( b );
	}else if( dA.t == DAO_LONG && dB.t >= DAO_INTEGER && dB.t <= DAO_DOUBLE ){
		DLong *c = DaoContext_GetLong( self, vmc );
		DLong *b = DLong_New();
		DLong *b2 = DLong_New();
		dint i = DValue_GetInteger( dB );
		DLong_FromInteger( b, i );
		switch( vmc->code ){
		case DVM_ADD : DLong_Add( c, dA.v.l, b ); break;
		case DVM_SUB : DLong_Sub( c, dA.v.l, b ); break;
		case DVM_MUL : DLong_Mul( c, dA.v.l, b ); break;
		case DVM_DIV : DaoContext_LongDiv( self, dA.v.l, b, c, b2 ); break;
		case DVM_MOD : DaoContext_LongDiv( self, dA.v.l, b, b2, c ); break;
		case DVM_POW : DLong_Pow( c, dA.v.l, i ); break;
		default: break;
		}
		DLong_Delete( b );
		DLong_Delete( b2 );
	}else if( dB.t == DAO_LONG && dA.t >= DAO_INTEGER && dA.t <= DAO_DOUBLE ){
		DLong *a, *b2, *c = DaoContext_GetLong( self, vmc );
		dint i = DValue_GetInteger( dA );
		if( vmc->code == DVM_POW && DaoContext_CheckLong2Integer( self, dB.v.l ) == 0 ) return;
		a = DLong_New();
		b2 = DLong_New();
		DLong_FromInteger( a, i );
		switch( vmc->code ){
		case DVM_ADD : DLong_Add( c, a, dB.v.l ); break;
		case DVM_SUB : DLong_Sub( c, a, dB.v.l ); break;
		case DVM_MUL : DLong_MulInt( c, dB.v.l, i ); break;
		case DVM_DIV : DaoContext_LongDiv( self, a, dB.v.l, c, b2 ); break;
		case DVM_MOD : DaoContext_LongDiv( self, a, dB.v.l, b2, c ); break;
		case DVM_POW : DLong_Pow( c, a, DLong_ToInteger( dB.v.l ) ); break;
		default: break;
		}
		DLong_Delete( a );
		DLong_Delete( b2 );
#ifdef DAO_WITH_NUMARRAY
	}else if( dB.t >=DAO_INTEGER && dB.t <=DAO_COMPLEX && dA.t ==DAO_ARRAY ){
		DaoArray *na = dA.v.array;
		DaoArray *nc = na;
		if( vmc->a != vmc->c ) nc = DaoContext_GetArray( self, vmc );
		DaoArray_array_op_number( nc, na, dB, vmc->code, self );
	}else if( dA.t >=DAO_INTEGER && dA.t <=DAO_COMPLEX && dB.t ==DAO_ARRAY ){
		DaoArray *nb = dB.v.array;
		DaoArray *nc = nb;
		if( vmc->b != vmc->c ) nc = DaoContext_GetArray( self, vmc );
		DaoArray_number_op_array( nc, dA, nb, vmc->code, self );
	}else if( dA.t ==DAO_ARRAY && dB.t ==DAO_ARRAY ){
		DaoArray *na = dA.v.array;
		DaoArray *nb = dB.v.array;
		DaoArray *nc;
		if( vmc->a == vmc->c )
			nc = na;
		else if( vmc->b == vmc->c )
			nc = nb;
		else
			nc = DaoContext_GetArray( self, vmc );
		DaoArray_ArrayArith( nc, na, nb, vmc->code, self );
#endif
	}else if( dA.t ==DAO_STRING && dB.t ==DAO_INTEGER && vmc->code ==DVM_ADD 
			&& vmc->a == vmc->c ){
		DString_AppendWChar( dA.v.s, (wchar_t) dB.v.i );
	}else if( dA.t ==DAO_STRING && dB.t ==DAO_STRING && vmc->code ==DVM_ADD ){
		if( vmc->a == vmc->c ){
			DString_Append( dA.v.s, dB.v.s );
		}else if( vmc->b == vmc->c ){
			DString_Insert( dB.v.s, dA.v.s, 0, 0, 0 );
		}else{
			dA.v.s = DString_Copy( dA.v.s );
			DString_Append( dA.v.s, dB.v.s );
			DaoContext_PutValue( self, dA );
			DString_Delete( dA.v.s );
		}
	}else if( dA.t == DAO_ENUM && dB.t == DAO_ENUM 
			&& (vmc->code == DVM_ADD || vmc->code == DVM_SUB) ){
		DaoType *ta = dA.v.e->type;
		DaoType *tb = dB.v.e->type;
		DEnum *denum = dA.v.e;
		int rc = 0;
		if( vmc->c != vmc->a && ta->name->mbs[0] == '$' && tb->name->mbs[0] == '$' ){
			DaoNameSpace *ns = self->nameSpace;
			DaoType *type = NULL;
			dint value = 0;
			denum = DaoContext_GetEnum( self, vmc );
			if( vmc->code == DVM_ADD ){
				type = DaoNameSpace_SymbolTypeAdd( ns, ta, tb, &value );
			}else{
				type = DaoNameSpace_SymbolTypeAdd( ns, ta, tb, &value );
			}
			if( type == NULL ) DaoContext_RaiseException( self, DAO_ERROR_TYPE, "symbol not found in the enum" );
			DEnum_SetType( denum, type );
			denum->value = value;
			return;
		}
		if( vmc->c != vmc->a ){
			denum = DaoContext_GetEnum( self, vmc );
			if( denum->type == NULL ) DEnum_SetType( denum, dA.v.e->type );
			DEnum_SetValue( denum, dA.v.e, NULL );
		}
		if( vmc->code == DVM_ADD ){
			rc = DEnum_AddValue( denum, dB.v.e, NULL );
		}else{
			rc = DEnum_RemoveValue( denum, dB.v.e, NULL );
		}
		if( rc == 0 ){
			if( denum->type->flagtype ==0 )
				DaoContext_RaiseException( self, DAO_ERROR_TYPE, "not combinable enum" );
			else
				DaoContext_RaiseException( self, DAO_ERROR_TYPE, "symbol not found in the enum" );
			return;
		}
	}else if( dA.t == DAO_LIST && dB.t == DAO_LIST && vmc->code == DVM_ADD ){
		DaoList *lA = dA.v.list;
		DaoList *lB = dB.v.list;
		DaoList *list;
		size_t i=0;
		if( vmc->a == vmc->c ){
			list = lA;
			for( i=0; i<lB->items->size; i++)
				DaoList_Append( list, lB->items->data[i] );
		}else if( vmc->b == vmc->c ){
			list = lB;
			for( i=lA->items->size-1; i>=0; i--)
				DaoList_PushFront( list, lA->items->data[i] );
		}else{
			list = DaoContext_GetList( self, vmc );
			DVarray_Resize( list->items, lA->items->size + lB->items->size, daoNullValue );
			for( i=0; i<lA->items->size; i++)
				DaoList_SetItem( list, lA->items->data[i], i );
			for( i=0; i<lB->items->size; i++)
				DaoList_SetItem( list, lB->items->data[i], i + lA->items->size );
		}
	}else if( dA.t == DAO_MAP && dB.t == DAO_MAP && vmc->code == DVM_ADD ){
		DaoMap *hA = dA.v.map;
		DaoMap *hB = dB.v.map;
		DaoMap *hC;
		DNode *node;
		if( vmc->a == vmc->c ){
			hC = hA;
		}else if( vmc->a == vmc->c ){
			hC = hB;
			hB = hA;
		}else{
			hC = DaoContext_GetMap( self, vmc );
			node = DMap_First( hA->items );
			for( ; node !=NULL; node=DMap_Next( hA->items, node) )
				DMap_Insert( hC->items, node->key.pVoid, node->value.pVoid );
		}
		node = DMap_First( hB->items );
		for( ; node !=NULL; node=DMap_Next( hB->items, node) )
			DMap_Insert( hC->items, node->key.pVoid, node->value.pVoid );
	}else{
		DaoContext_RaiseException( self, DAO_ERROR_TYPE, "" );
	}
}
/* binary operation with boolean result. */
void DaoContext_DoBinBool(  DaoContext *self, DaoVmCode *vmc )
{
	DValue *A = self->regValues[ vmc->a ];
	DValue *B = self->regValues[ vmc->b ];
	DValue *C = self->regValues[ vmc->c ];
	DValue dA = *A, dB = *B;
	DValue dC = daoZeroInt;
	int rc = 0;
	DLong *lng;

	self->vmc = vmc;

	if( dA.t ==0 || dB.t ==0 ){
		switch( vmc->code ){
		case DVM_AND: dC = dA.t ? dB : dA; break;
		case DVM_OR:  dC = dA.t ? dA : dB; break;
		case DVM_LT:  dC.v.i = dA.t < dB.t; break;
		case DVM_LE:  dC.v.i = dA.t <= dB.t; break;
		case DVM_EQ:  dC.v.i = dA.t == dB.t; break;
		case DVM_NE:  dC.v.i = dA.t != dB.t; break;
		default: break;
		}
		if( dA.t == DAO_CDATA || dB.t == DAO_CDATA ){
			DaoCData *cdata = (DaoCData*)( dA.t == DAO_CDATA ? dA.v.cdata : dB.v.cdata );
			if( vmc->code == DVM_EQ ){
				dC.v.i = cdata->data ? 0 : 1;
			}else if( vmc->code == DVM_NE ){
				dC.v.i = cdata->data ? 1 : 0;
			}
		}
		DaoContext_SetValue( self, vmc->c, dC );
		return;
	}

	if( dA.t==DAO_OBJECT || dB.t==DAO_OBJECT || dA.t==DAO_CDATA || dB.t==DAO_CDATA ){
		if( dA.t ==DAO_OBJECT || dB.t ==DAO_OBJECT ){
			rc = DaoContext_TryObjectArith( self, A, B, C );
			if( rc == 0 ){
				switch( vmc->code ){
				case DVM_AND: dC = dA.v.p ? dB : dA; break;
				case DVM_OR:  dC = dA.v.p ? dA : dB; break;
				case DVM_LT:  dC.v.i = dA.v.p < dB.v.p; break;
				case DVM_LE:  dC.v.i = dA.v.p <= dB.v.p; break;
				case DVM_EQ:  dC.v.i = dA.v.p == dB.v.p; break;
				case DVM_NE:  dC.v.i = dA.v.p != dB.v.p; break;
				default: break;
				}
				DaoContext_SetValue( self, vmc->c, dC );
			}
		}else if( dA.t ==DAO_CDATA || dB.t ==DAO_CDATA ){
			rc = DaoContext_TryCDataArith( self, A, B, C );
			if( rc == 0 ){
				switch( vmc->code ){
				case DVM_AND: dC = dA.v.cdata->data ? dB : dA; break;
				case DVM_OR:  dC = dA.v.cdata->data ? dA : dB; break;
				case DVM_LT:  dC.v.i = dA.v.cdata->data < dB.v.cdata->data; break;
				case DVM_LE:  dC.v.i = dA.v.cdata->data <= dB.v.cdata->data; break;
				case DVM_EQ:  dC.v.i = dA.v.cdata->data == dB.v.cdata->data; break;
				case DVM_NE:  dC.v.i = dA.v.cdata->data != dB.v.cdata->data; break;
				default: break;
				}
				DaoContext_SetValue( self, vmc->c, dC );
			}
		}
		return;
	}

	if( dA.t >= DAO_INTEGER && dA.t <= DAO_DOUBLE 
			&& dB.t >= DAO_INTEGER && dB.t <= DAO_DOUBLE ){
		switch( vmc->code ){
		case DVM_AND: dC = DValue_GetDouble( dA ) ? dB : dA; break;
		case DVM_OR:  dC = DValue_GetDouble( dA ) ? dA : dB; break;
		case DVM_LT:  dC.v.i = DValue_GetDouble( dA ) < DValue_GetDouble( dB ); break;
		case DVM_LE:  dC.v.i = DValue_GetDouble( dA ) <= DValue_GetDouble( dB ); break;
		case DVM_EQ:  dC.v.i = DValue_GetDouble( dA ) == DValue_GetDouble( dB ); break;
		case DVM_NE:  dC.v.i = DValue_GetDouble( dA ) != DValue_GetDouble( dB ); break;
		default: break;
		}
	}else if( dA.t == DAO_COMPLEX && dB.t == DAO_COMPLEX ){
		switch( vmc->code ){
		case DVM_EQ:  dC.v.i = (dA.v.c->real == dB.v.c->real) && (dA.v.c->imag == dB.v.c->imag); break;
		case DVM_NE:  dC.v.i = (dA.v.c->real != dB.v.c->real) || (dA.v.c->imag != dB.v.c->imag); break;
		default: break;
		}
	}else if( dA.t == DAO_LONG && dB.t == DAO_LONG ){
		switch( vmc->code ){
		case DVM_AND: dC = DLong_CompareToZero( dA.v.l ) ? dB : dA; break;
		case DVM_OR:  dC = DLong_CompareToZero( dA.v.l ) ? dA : dB; break;
		case DVM_LT:  dC.v.i = DLong_Compare( dA.v.l, dB.v.l )< 0; break;
		case DVM_LE:  dC.v.i = DLong_Compare( dA.v.l, dB.v.l )<=0; break;
		case DVM_EQ:  dC.v.i = DLong_Compare( dA.v.l, dB.v.l )==0; break;
		case DVM_NE:  dC.v.i = DLong_Compare( dA.v.l, dB.v.l )!=0; break;
		default: break;
		}
	}else if( dA.t == DAO_INTEGER && dB.t == DAO_LONG ){
		switch( vmc->code ){
		case DVM_AND: dC = dA.v.i ? dB : dA; break;
		case DVM_OR:  dC = dA.v.i ? dA : dB; break;
		case DVM_LT:  dC.v.i = DLong_CompareToInteger( dB.v.l, dA.v.i )> 0; break;
		case DVM_LE:  dC.v.i = DLong_CompareToInteger( dB.v.l, dA.v.i )>=0; break;
		case DVM_EQ:  dC.v.i = DLong_CompareToInteger( dB.v.l, dA.v.i )==0; break;
		case DVM_NE:  dC.v.i = DLong_CompareToInteger( dB.v.l, dA.v.i )!=0; break;
		default: break;
		}
	}else if( dA.t == DAO_LONG && dB.t == DAO_INTEGER ){
		switch( vmc->code ){
		case DVM_AND: dC = DLong_CompareToZero( dA.v.l ) ? dB : dA; break;
		case DVM_OR:  dC = DLong_CompareToZero( dA.v.l ) ? dA : dB; break;
		case DVM_LT:  dC.v.i = DLong_CompareToInteger( dA.v.l, dB.v.i )< 0; break;
		case DVM_LE:  dC.v.i = DLong_CompareToInteger( dA.v.l, dB.v.i )<=0; break;
		case DVM_EQ:  dC.v.i = DLong_CompareToInteger( dA.v.l, dB.v.i )==0; break;
		case DVM_NE:  dC.v.i = DLong_CompareToInteger( dA.v.l, dB.v.i )!=0; break;
		default: break;
		}
	}else if( (dA.t == DAO_FLOAT || dA.t == DAO_DOUBLE) && dB.t == DAO_LONG ){
		double va = DValue_GetDouble( dA );
		switch( vmc->code ){
		case DVM_AND: dC = va ? dB : dA; break;
		case DVM_OR:  dC = va ? dA : dB; break;
		case DVM_LT:  dC.v.i = DLong_CompareToDouble( dB.v.l, va )> 0; break;
		case DVM_LE:  dC.v.i = DLong_CompareToDouble( dB.v.l, va )>=0; break;
		case DVM_EQ:  dC.v.i = DLong_CompareToDouble( dB.v.l, va )==0; break;
		case DVM_NE:  dC.v.i = DLong_CompareToDouble( dB.v.l, va )!=0; break;
		default: break;
		}
	}else if( dA.t == DAO_LONG && (dB.t == DAO_FLOAT || dB.t == DAO_DOUBLE) ){
		double vb = DValue_GetDouble( dB );
		switch( vmc->code ){
		case DVM_AND: dC = DLong_CompareToZero( dA.v.l ) ? dB : dA; break;
		case DVM_OR:  dC = DLong_CompareToZero( dA.v.l ) ? dA : dB; break;
		case DVM_LT:  dC.v.i = DLong_CompareToDouble( dA.v.l, vb )< 0; break;
		case DVM_LE:  dC.v.i = DLong_CompareToDouble( dA.v.l, vb )<=0; break;
		case DVM_EQ:  dC.v.i = DLong_CompareToDouble( dA.v.l, vb )==0; break;
		case DVM_NE:  dC.v.i = DLong_CompareToDouble( dA.v.l, vb )!=0; break;
		default: break;
		}
	}else if( dA.t == DAO_STRING && dB.t == DAO_STRING ){
		switch( vmc->code ){
		case DVM_AND: dC = DString_Size( dA.v.s ) ? dB : dA; break;
		case DVM_OR:  dC = DString_Size( dA.v.s ) ? dA : dB; break;
		case DVM_LT:  dC.v.i = STR_LT( dA.v.s, dB.v.s ); break;
		case DVM_LE:  dC.v.i = STR_LE( dA.v.s, dB.v.s ); break;
		case DVM_EQ:  dC.v.i = STR_EQ( dA.v.s, dB.v.s ); break;
		case DVM_NE:  dC.v.i = STR_NE( dA.v.s, dB.v.s ); break;
		default: break;
		}
	}else if( (dA.t == DAO_ENUM && dB.t == DAO_ENUM)
			|| (dA.t == DAO_TUPLE && dB.t == DAO_TUPLE) ){
		switch( vmc->code ){
		case DVM_AND: dC = DValue_GetInteger( dA ) ? dB : dA; break;
		case DVM_OR:  dC = DValue_GetInteger( dA ) ? dA : dB; break;
		case DVM_LT:  dC.v.i = DValue_Compare( dA, dB ) < 0; break;
		case DVM_LE:  dC.v.i = DValue_Compare( dA, dB ) <= 0; break;
		case DVM_EQ:  dC.v.i = DValue_Compare( dA, dB ) == 0; break;
		case DVM_NE:  dC.v.i = DValue_Compare( dA, dB ) != 0; break;
		default: break;
		}
	}else if( vmc->code == DVM_AND || vmc->code == DVM_OR ){
		DValue AA = dA, BB = dB;
		if( vmc->code == DVM_OR ){ AA = dB; BB = dA; }
		switch( dA.t ){
		case DAO_INTEGER : dC = dA.v.i ? BB : AA; break;
		case DAO_FLOAT   : dC = dA.v.f ? BB : AA; break;
		case DAO_DOUBLE  : dC = dA.v.d ? BB : AA; break;
		case DAO_COMPLEX : dC = dA.v.c->real && dA.v.c->imag ? BB : AA; break;
		case DAO_LONG : dC = DLong_CompareToZero( dA.v.l ) ? BB : AA; break;
		case DAO_STRING : dC = DString_Size( dA.v.s ) ? BB : AA; break;
		case DAO_ENUM : dC = dA.v.e->value ? BB : AA; break;
		case DAO_LIST : dC = dA.v.list->items->size ? BB : AA; break;
		case DAO_MAP  : dC = dA.v.map->items->size ? BB : AA; break;
		case DAO_ARRAY : dC = dA.v.array->size ? BB : AA; break;
		default : dC = dA.t ? BB : AA; break;
		}
	}else{
		switch( vmc->code ){
		case DVM_LT: dC.v.i = DValue_Compare( dA, dB )< 0; break;
		case DVM_LE: dC.v.i = DValue_Compare( dA, dB )<=0; break;
		case DVM_EQ: dC.v.i = DValue_Compare( dA, dB )==0; break; /*XXX numarray*/
		case DVM_NE: dC.v.i = DValue_Compare( dA, dB )!=0; break;
		default: break;
		}
	}
	DaoContext_SetValue( self, vmc->c, dC );
}
void DaoContext_DoUnaArith( DaoContext *self, DaoVmCode *vmc )
{
	DValue *A = self->regValues[ vmc->a ];
	DValue *C = self->regValues[ vmc->c ];
	DValue dA = *A;
	DValue dC = dA;
	DValue *bl = NULL;
	char ta = dA.t;
	self->vmc = vmc;
	if( dA.t ==0 ){
		DaoContext_RaiseException( self, DAO_ERROR_TYPE, "on null object" );
		return;
	}

	self->vmc = vmc;
	if( ta == DAO_INTEGER ){
		switch( vmc->code ){
		case DVM_NOT :  dC.v.i = ! dC.v.i; break;
		case DVM_UNMS : dC.v.i = - dC.v.i; break;
		case DVM_INCR : dC.v.i ++; break;
		case DVM_DECR : dC.v.i --; break;
		default: break;
		}
		bl = DaoContext_SetValue( self, vmc->c, dC );
	}else if( ta == DAO_FLOAT ){
		switch( vmc->code ){
		case DVM_NOT :  dC.v.f = ! dC.v.f; break;
		case DVM_UNMS : dC.v.f = - dC.v.f; break;
		case DVM_INCR : dC.v.f ++; break;
		case DVM_DECR : dC.v.f --; break;
		default: break;
		}
		bl = DaoContext_SetValue( self, vmc->c, dC );
	}else if( ta == DAO_DOUBLE ){
		switch( vmc->code ){
		case DVM_NOT :  dC.v.d = ! dC.v.d; break;
		case DVM_UNMS : dC.v.d = - dC.v.d; break;
		case DVM_INCR : dC.v.d ++; break;
		case DVM_DECR : dC.v.d --; break;
		default: break;
		}
		bl = DaoContext_SetValue( self, vmc->c, dC );
	}else if( ta==DAO_COMPLEX ){
		if( vmc->code == DVM_UNMS ){
			bl = DaoContext_SetValue( self, vmc->c, dA );
			if( bl ){
				self->regValues[ vmc->c ]->v.c->real = - dA.v.c->real;
				self->regValues[ vmc->c ]->v.c->imag = - dA.v.c->imag;
			}
		}
	}else if( ta==DAO_LONG ){
		DLong *c = DaoContext_GetLong( self, vmc );
		DLong_Move( c, dA.v.l );
		c->sign *= -1;
		bl = & dA;
		switch( vmc->code ){
		case DVM_NOT :  dC.v.i = ! dC.v.i; break;
		case DVM_UNMS : dC.v.i = - dC.v.i; break;
		case DVM_INCR : dC.v.i ++; break;
		case DVM_DECR : dC.v.i --; break;
		default: break;
		}
		/*
		   switch( vmc->code ){
		   case DVM_NOT :  dC.v.d = ! dC.v.d; break;
		   case DVM_UNMS : dC.v.d = - dC.v.d; break;
		   case DVM_INCR : dC.v.d ++; break;
		   case DVM_DECR : dC.v.d --; break;
		   default: break;
		   }
		 */
#ifdef DAO_WITH_NUMARRAY
	}else if( ta==DAO_ARRAY ){
		DaoArray *array = dA.v.array;
		size_t i;
		if( array->numType == DAO_FLOAT ){
			if( vmc->code == DVM_NOT || vmc->code == DVM_UNMS ){
				DaoArray *res = DaoContext_GetArray( self, vmc );
				res->numType = array->numType;
				DaoArray_ResizeArray( res, array->dims->items.pSize, array->dims->size );
				if( array->numType == DAO_FLOAT ){
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
			}else{
				DaoContext_SetData( self, vmc->c, (DaoBase*)array );
				if( array->numType == DAO_FLOAT ){
					float *va = array->data.f;
					if( vmc->code == DVM_INCR ){
						for(i=0; i<array->size; i++ ) ++ va[i];
					}else{
						for(i=0; i<array->size; i++ ) -- va[i];
					}
				}else{
					double *va = array->data.d;
					if( vmc->code == DVM_INCR ){
						for(i=0; i<array->size; i++ ) ++ va[i];
					}else{
						for(i=0; i<array->size; i++ ) -- va[i];
					}
				}
			}
		}else if( vmc->code == DVM_UNMS ){
			DaoArray *res = DaoContext_GetArray( self, vmc );
			complex16 *va, *vc;
			res->numType = array->numType;
			DaoArray_ResizeArray( res, array->dims->items.pSize, array->dims->size );
			va = array->data.c;
			vc = res->data.c;
			for(i=0; i<array->size; i++ ){
				vc[i].real = - va[i].real;
				vc[i].imag = - va[i].imag;
			}
		}
#endif
	}else if( ta == DAO_OBJECT ){
		DaoContext_TryObjectArith( self, A, NULL, C );
		return;
	}else if( ta==DAO_CDATA ){
		DaoContext_TryCDataArith( self, A, NULL, C );
		return;
	}
	if( bl ==0 ) DaoContext_RaiseException( self, DAO_ERROR_TYPE, "" );
}
void DaoContext_DoInTest( DaoContext *self, DaoVmCode *vmc )
{
	DValue A = *self->regValues[ vmc->a ];
	DValue B = *self->regValues[ vmc->b ];
	dint *C = DaoContext_PutInteger( self, 0 );
	int i;
	if( A.t == DAO_INTEGER && B.t == DAO_STRING ){
		dint bv = A.v.i;
		int size = B.v.s->size;
		if( B.v.s->mbs ){
			char *mbs = B.v.s->mbs;
			for(i=0; i<size; i++){
				if( mbs[i] == bv ){
					*C = 1;
					break;
				}
			}
		}else{
			wchar_t *wcs = B.v.s->wcs;
			for(i=0; i<size; i++){
				if( wcs[i] == bv ){
					*C = 1;
					break;
				}
			}
		}
	}else if( A.t == DAO_STRING && B.t == DAO_STRING ){
		*C = DString_Find( B.v.s, A.v.s, 0 ) != MAXSIZE;
	}else if( A.t == DAO_ENUM && B.t == DAO_ENUM ){
		DaoType *ta = A.v.e->type;
		DaoType *tb = B.v.e->type;
		if( ta == tb ){
			*C = A.v.e->value == (A.v.e->value & B.v.e->value);
		}else{
			DMap *ma = ta->mapNames;
			DMap *mb = tb->mapNames;
			DNode *it, *node;
			*C = 1;
			for(it=DMap_First(ma); it; it=DMap_Next(ma,it) ){
				if( ta->flagtype ){
					if( !(it->value.pInt & A.v.e->value) ) continue;
				}else if( it->value.pInt != A.v.e->value ){
					continue;
				}
				if( (node = DMap_Find( mb, it->key.pVoid )) == NULL ){
					*C = 0;
					break;
				}
				if( !(node->value.pInt & B.v.e->value) ){
					*C = 0;
					break;
				}
			}
		}
	}else if( B.t == DAO_LIST ){
		DVarray *items = B.v.list->items;
		DaoType *ta = DaoNameSpace_GetTypeV( self->nameSpace, A );
		if( ta && B.v.list->unitype && B.v.list->unitype->nested->size ){
			DaoType *tb = B.v.list->unitype->nested->items.pType[0];
			if( tb && DaoType_MatchTo( ta, tb, NULL ) < DAO_MT_SUB	 ) return;
		}
		for(i=0; i<items->size; i++){
			if( DValue_Compare( A, items->data[i] ) ==0 ){
				*C = 1;
				break;
			}
		}
	}else if( B.t == DAO_MAP ){
		DaoType *ta = DaoNameSpace_GetTypeV( self->nameSpace, A );
		if( ta && B.v.map->unitype && B.v.map->unitype->nested->size ){
			DaoType *tb = B.v.map->unitype->nested->items.pType[0];
			if( tb && DaoType_MatchTo( ta, tb, NULL ) < DAO_MT_SUB	 ) return;
		}
		*C = DMap_Find( B.v.map->items, & A ) != NULL;
	}else if( B.t == DAO_PAIR ){
		int c1 = DValue_Compare( B.v.pair->first, A );
		int c2 = DValue_Compare( A, B.v.pair->second );
		*C = c1 <=0 && c2 <= 0;
	}else{
		DaoContext_RaiseException( self, DAO_ERROR_TYPE, "" );
	}
}
void DaoContext_DoBitLogic( DaoContext *self, DaoVmCode *vmc )
{
	DLong *bigint = DLong_New();
	DValue vA = *self->regValues[ vmc->a ];
	DValue vB = *self->regValues[ vmc->b ];
	DValue value = daoNullValue;
	ullong_t inum = 0;
	value.t = DAO_LONG;
	value.v.l = bigint;
#define NUMOP( x ) ( (x).t == DAO_INTEGER ? (x).v.i \
		: (x).t == DAO_FLOAT ? (x).v.f \
		: (x).t == DAO_DOUBLE ? (x).v.d : 0.0 )
	if( vA.t && vB.t && vA.t <= DAO_DOUBLE && vB.t <= DAO_DOUBLE ){
		switch( vmc->code ){
		case DVM_BITAND: inum =DValue_GetInteger(vA) & DValue_GetInteger(vB);break;
		case DVM_BITOR: inum =DValue_GetInteger(vA) | DValue_GetInteger(vB);break;
		case DVM_BITXOR: inum =(ullong_t)NUMOP(vA) ^ (ullong_t)NUMOP(vB);break;
		default : break;
		}
		if( vA.t == DAO_DOUBLE || vB.t == DAO_DOUBLE ){
			value.t = DAO_DOUBLE;
			value.v.d = inum;
		}else if( vA.t == DAO_FLOAT || vB.t == DAO_FLOAT ){
			value.t = DAO_FLOAT;
			value.v.f = inum;
		}else{
			value.t = DAO_INTEGER;
			value.v.i = inum;
		}
	}else if( vA.t == DAO_LONG && vB.t >= DAO_INTEGER && vB.t <= DAO_DOUBLE ){
		DLong_FromInteger( bigint, DValue_GetInteger( vB ) );
		switch( vmc->code ){
		case DVM_BITAND : DLong_BitAND( bigint, vA.v.l, bigint ); break;
		case DVM_BITOR :  DLong_BitOR( bigint, vA.v.l, bigint ); break;
		case DVM_BITXOR : DLong_BitXOR( bigint, vA.v.l, bigint ); break;
		default : break;
		}
	}else if( vB.t == DAO_LONG && vA.t >= DAO_INTEGER && vA.t <= DAO_DOUBLE ){
		DLong_FromInteger( bigint, DValue_GetInteger( vA ) );
		switch( vmc->code ){
		case DVM_BITAND : DLong_BitAND( bigint, vB.v.l, bigint ); break;
		case DVM_BITOR :  DLong_BitOR( bigint, vB.v.l, bigint ); break;
		case DVM_BITXOR : DLong_BitXOR( bigint, vB.v.l, bigint ); break;
		default : break;
		}
	}else if( vA.t ==DAO_LONG && vB.t == DAO_LONG ){
		switch( vmc->code ){
		case DVM_BITAND : DLong_BitAND( bigint, vA.v.l, vB.v.l ); break;
		case DVM_BITOR :  DLong_BitOR( bigint, vA.v.l, vB.v.l ); break;
		case DVM_BITXOR : DLong_BitXOR( bigint, vA.v.l, vB.v.l ); break;
		default : break;
		}
	}else{
		self->vmc = vmc;
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "invalid operands" );
	}
	DaoContext_SetValue( self, vmc->c, value );
	DLong_Delete( bigint );
}
void DaoContext_DoBitShift( DaoContext *self, DaoVmCode *vmc )
{
	DValue vA = *self->regValues[ vmc->a ];
	DValue vB = *self->regValues[ vmc->b ];
	DValue value = daoZeroInt;
	if( vA.t && vB.t && vA.t <= DAO_DOUBLE && vB.t <= DAO_DOUBLE ){
		llong_t inum = 0;
		if( vmc->code == DVM_BITLFT ){
			inum = DValue_GetInteger(vA) << DValue_GetInteger(vB);
		}else{
			inum = DValue_GetInteger(vA) >> DValue_GetInteger(vB);
		}
		if( vA.t == DAO_DOUBLE || vB.t == DAO_DOUBLE ){
			value.t = DAO_DOUBLE;
			value.v.d = inum;
		}else if( vA.t == DAO_FLOAT || vB.t == DAO_FLOAT ){
			value.t = DAO_FLOAT;
			value.v.f = inum;
		}else{
			value.t = DAO_INTEGER;
			value.v.i = inum;
		}
		DaoContext_SetValue( self, vmc->c, value );
	}else if( vA.t ==DAO_LONG && vB.t >=DAO_INTEGER && vB.t <= DAO_DOUBLE ){
		DLong *bigint = DLong_New();
		if( vmc->a == vmc->c ){
			if( vmc->code == DVM_BITLFT ){
				DLong_ShiftLeft( vA.v.l, DValue_GetInteger( vB ) );
			}else{
				DLong_ShiftRight( vA.v.l, DValue_GetInteger( vB ) );
			}
		}else{
			DLong_Move( bigint, vA.v.l );
			if( vmc->code == DVM_BITLFT ){
				DLong_ShiftLeft( bigint, DValue_GetInteger( vB ) );
			}else{
				DLong_ShiftRight( bigint, DValue_GetInteger( vB ) );
			}
			value.t = DAO_LONG;
			value.v.l = bigint;
			DaoContext_SetValue( self, vmc->c, value );
		}
		DLong_Delete( bigint );
	}else if( vA.t ==DAO_LIST && (vmc->code ==DVM_BITLFT || vmc->code ==DVM_BITRIT) ){
		DaoList *list = self->regValues[ vmc->a ]->v.list;
		self->vmc = vmc;
		if( DaoContext_SetValue( self, vmc->c, vA ) ==0 ) return;
		if( vmc->code ==DVM_BITLFT ){
			DaoType *abtp = list->unitype;
			if( abtp && abtp->nested->size ){
				abtp = abtp->nested->items.pType[0];
				if( DaoType_MatchValue( abtp, vB, NULL ) ==0 ) return; /* XXX information */
			}
			DVarray_PushBack( list->items, vB );
		}else{
			if( list->items->size ==0 ) return; /* XXX information */
			vB = list->items->data[list->items->size-1];
			if( DaoContext_SetValue( self, vmc->b, vB ) ==0 ) return;
			DVarray_PopBack( list->items );
		}
	}else{
		self->vmc = vmc;
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "invalid operands" );
	}
}
void DaoContext_DoBitFlip( DaoContext *self, DaoVmCode *vmc )
{
	DValue vA = *self->regValues[ vmc->a ];
	DValue value = daoZeroInt;
	if( vA.t >= DAO_INTEGER && vA.t <= DAO_DOUBLE ){
		value.v.i = ~ DValue_GetInteger( vA );
		DaoContext_SetValue( self, vmc->c, value );
	}else if( vA.t == DAO_LONG ){
		DLong *bigint = DLong_New();
		DLong_Move( bigint, vA.v.l );
		DLong_Flip( bigint );
		DaoContext_SetValue( self, vmc->c, value );
		DLong_Delete( bigint );
	}else{
		self->vmc = vmc;
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "invalid operands" );
	}
}

static int DaoBase_CheckTypeShape( DValue self, int type, 
		DArray *shape, unsigned int depth, int check_size )
{
	DaoTuple *tuple;
	DaoArray *array;
	DaoList *list;
	DValue  *data;
	size_t i, j;
	if( self.t ==0 ) return -1;
	switch( self.t ){
	case DAO_INTEGER :
	case DAO_FLOAT :
	case DAO_DOUBLE :
	case DAO_COMPLEX :
		if( type <= self.t ) type = self.t;
		break;
	case DAO_STRING :
		type = type == DAO_UDF || type == self.t ? self.t : -1;
		break;
	case DAO_ARRAY :
#ifdef DAO_WITH_NUMARRAY
		array = self.v.array;
		if( type < array->numType ) type = array->numType;
		if( shape->size <= depth ){
			for(i=0; i<array->dims->size; i++){
				DArray_Append( shape, array->dims->items.pVoid[i] );
			}
		}else if( check_size ){
			for(i=0; i<array->dims->size; i++){
				if( shape->items.pSize[i+depth] != array->dims->items.pSize[i] ) return -1;
			}
		}
#endif
		break;
	case DAO_LIST :
		list = self.v.list;
		if( shape->size <= depth ) DArray_Append( shape, (void*)list->items->size );
		if( check_size && list->items->size != shape->items.pSize[depth] ) return -1;
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
		data = list->items->data;
		for(i=0; i<list->items->size; i++ ){
			type = DaoBase_CheckTypeShape( data[i], type, shape, depth, check_size );
			if( type < 0 ) break;
		}
		break;
	case DAO_TUPLE :
		tuple = self.v.tuple;
		if( shape->size <= depth ) DArray_Append( shape, (void*)tuple->items->size );
		if( check_size && tuple->items->size != shape->items.pSize[depth] ) return -1;
		depth ++;
		data = tuple->items->data;
		for(i=0; i<tuple->items->size; i++){
			type = DaoBase_CheckTypeShape( data[i], type, shape, depth, check_size );
			if( type < 0 ) break;
		}
		break;
	default : break;
	}
	return type;
}
#ifdef DAO_WITH_NUMARRAY
static int DaoBase_ExportValue( DValue self, DaoArray *array, int k )
{
	DaoTuple *tup;
	DaoArray *sub;
	DaoList *list;
	size_t i;
	if( k >= array->size ) return k;
	switch( self.t ){
	case DAO_INTEGER :
	case DAO_FLOAT :
	case DAO_DOUBLE :
	case DAO_LONG :
	case DAO_STRING :
		switch( array->numType ){
		case DAO_INTEGER : array->data.i[k] = DValue_GetInteger( self ); break;
		case DAO_FLOAT : array->data.f[k] = DValue_GetFloat( self ); break;
		case DAO_DOUBLE : array->data.d[k] = DValue_GetDouble( self ); break;
		case DAO_COMPLEX : array->data.c[k].real = DValue_GetDouble( self ); break;
		default : break;
		}
		k ++;
		break;
	case DAO_COMPLEX :
		switch( array->numType ){
		case DAO_INTEGER : array->data.i[k] = self.v.c->real; break;
		case DAO_FLOAT : array->data.f[k] = self.v.c->real; break;
		case DAO_DOUBLE : array->data.d[k] = self.v.c->real; break;
		case DAO_COMPLEX : array->data.c[k] = self.v.c[0]; break;
		default : break;
		}
		k ++;
		break;
	case DAO_ARRAY :
		sub = self.v.array;
		for(i=0; i<sub->size; i++){
			switch( array->numType ){
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
		list = self.v.list;
		for(i=0; i<list->items->size; i++ ){
			k = DaoBase_ExportValue( list->items->data[i], array, k );
		}
		break;
	case DAO_TUPLE :
		tup = self.v.tuple;
		for(i=0; i<tup->items->size; i++){
			k = DaoBase_ExportValue( tup->items->data[i], array, k );
		}
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
	DString_Resize( str, size * ( (self->numType == DAO_COMPLEX) +1 ) );
	for(i=0; i<size; i++){
		switch( self->numType ){
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
	DValue num = daoZeroInt;
	DValue it;
	DaoList *ls;
	size_t *ds = self->dims->items.pSize;
	size_t i, size, isvector = self->dims->size==2 && (ds[0] ==1 || ds[1] ==1);

	if( abtp == NULL ) return 0;
	abtp = abtp->nested->items.pType[0];
	if( abtp == NULL ) return 0;
	if( isvector ) dim = ds[0] ==1 ? 1 : 0;
	if( dim >= self->dims->size-1 || isvector ){
		if( abtp->tid == DAO_INTEGER ){
			switch( self->numType ){
			case DAO_INTEGER :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.i = self->data.i[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			case DAO_FLOAT :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.i = self->data.f[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			case DAO_DOUBLE :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.i = self->data.d[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			default : break;
			}
		}else if( abtp->tid == DAO_FLOAT ){
			num.t = DAO_FLOAT;
			switch( self->numType ){
			case DAO_INTEGER :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.f = self->data.i[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			case DAO_FLOAT :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.f = self->data.f[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			case DAO_DOUBLE :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.f = self->data.d[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			default : break;
			}
		}else if( abtp->tid == DAO_DOUBLE ){
			num.t = DAO_DOUBLE;
			switch( self->numType ){
			case DAO_INTEGER :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.d = self->data.i[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			case DAO_FLOAT :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.d = self->data.f[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			case DAO_DOUBLE :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.d = self->data.d[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			default : break;
			}
		}else if( abtp->tid == DAO_COMPLEX ){
			num.t = DAO_COMPLEX;
			num.v.c = dao_malloc( sizeof(complex16) );
			switch( self->numType ){
			case DAO_INTEGER :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.c->real = self->data.i[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			case DAO_FLOAT :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.c->real = self->data.f[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			case DAO_DOUBLE :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.c->real = self->data.d[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			case DAO_COMPLEX :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					num.v.c[0] = self->data.c[ offset+i ];
					DaoList_Append( list, num );
				}
				break;
			default : break;
			}
			dao_free( num.v.c );
		}else{
			return 0;
		}
	}else{
		if( abtp->tid == DAO_STRING ){
			DValue str = daoNullString;
			str.v.s = DString_New(1);
			size = self->dimAccum->items.pSize[dim];
			for(i=0; i<self->dims->items.pSize[dim]; i++){
				DaoArray_ToWCString( self, str.v.s, offset, size );
				DaoList_Append( list, str );
				offset += self->dimAccum->items.pSize[dim];
			}
			DString_Delete( str.v.s );
		}else if( abtp->tid == DAO_LIST ){
			it = daoNullList;
			for(i=0; i<self->dims->items.pSize[dim]; i++){
				ls = DaoList_New();
				ls->unitype = abtp;
				GC_IncRC( abtp );
				it.v.list = ls;
				DaoList_Append( list, it );
				DaoArray_ToList( self, ls, abtp, dim+1, offset );
				offset += self->dimAccum->items.pSize[dim];
			}
		}
	}
	return 1;
}
int DaoArray_FromList( DaoArray *self, DaoList *list, DaoType *tp )
{
	DValue value = daoNullList;
	DArray *shape = DArray_New(0);
	int type;
	value.v.list = list;
	type = DaoBase_CheckTypeShape( value, DAO_UDF, shape, 0, 1 );
	if( type <0 ) goto FailConversion;
	self->numType = type;
	if( tp && tp->tid && tp->tid <= DAO_COMPLEX ) self->numType = tp->tid;
	DaoArray_ResizeArray( self, shape->items.pSize, shape->size );
	DaoBase_ExportValue( value, self, 0 );
	DArray_Delete( shape );
	return 1;
FailConversion:
	DArray_Delete( shape );
	return 0;
}
#endif
static int ConvertStringToNumber( DaoContext *ctx, DValue dA, DValue *dC )
{
	DString *mbs = ctx->process->mbstring;
	double d1 = 0.0, d2 = 0.0;
	int set1 = 0, set2 = 0;
	int toktype, toklen = 0;
	int tid = dC->t;
	int imagfirst = 0;
	int sign = 1;
	if( dA.t != DAO_STRING || tid ==0 || tid > DAO_LONG ) return 0;
	if( tid == DAO_STRING ){
		*dC = dA;
		return 1;
	}
	if( dA.v.s->mbs ){
		DString_SetDataMBS( mbs, dA.v.s->mbs, dA.v.s->size );
	}else{
		DString_SetDataWCS( mbs, dA.v.s->wcs, dA.v.s->size );
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
			dC->v.i = (sizeof(dint) == 4) ? strtol( mbs->mbs, 0, 0 ) : strtoll( mbs->mbs, 0, 0 );
			if( sign <0 ) dC->v.i = - dC->v.i;
		}else if( tid == DAO_FLOAT ){
			dC->v.f = strtod( mbs->mbs, 0 );
			if( sign <0 ) dC->v.f = - dC->v.f;
		}else if( tid == DAO_DOUBLE ){
			dC->v.d = strtod( mbs->mbs, 0 );
			if( sign <0 ) dC->v.d = - dC->v.d;
		}else{ /* DAO_LONG */
			char ec = DLong_FromString( dC->v.l, mbs );
			if( ec ){
				const char *msg = ec == 'L' ? "invalid radix" : "invalid digit";
				DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, msg );
				return 0;
			}
			dC->v.l->sign = sign;
		}
		return 1;
	}
	dC->v.c->real = dC->v.c->imag = 0.0;
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
	if( imagfirst ) dC->v.c->imag = d1; else dC->v.c->real = d1;
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
	if( imagfirst ) dC->v.c->real = d2; else dC->v.c->imag = d2;
	return toklen == mbs->size;
}
static DValue DaoTypeCast( DaoContext *ctx, DaoType *ct, DValue dA, 
		complex16 *cb, DLong *lb, DString *sb )
{
	DaoNameSpace *ns = ctx->nameSpace;
	DString *wcs = DString_New(0);
	DString *str;
	DArray *shape = NULL;
	DNode *node;
	DaoTuple *tuple = NULL;
	DaoList *list = NULL, *list2 = NULL;
	DaoMap *map = NULL, *map2 = NULL;
	DaoType *tp = NULL, *tp2 = NULL;
	DaoArray *array = NULL, *array2 = NULL;
	DValue dC, key, value = daoNullValue;
	DValue *data;
	int i, type, size;
	int toktype = 0, toklen = 0;
	if( ct == NULL ) goto FailConversion;
	dC = value;
	dC.t = ct->tid;
	if( ct->tid == DAO_ANY ) goto Rebind;
	if( dA.t == ct->tid && ct->tid >= DAO_INTEGER && ct->tid < DAO_ARRAY ) goto Rebind;
	if( dA.t == DAO_STRING && ct->tid > 0 && ct->tid <= DAO_LONG ){
		if( ct->tid == DAO_COMPLEX ) dC.v.c = cb;
		else if( ct->tid == DAO_LONG ) dC.v.l = lb;
		if( ConvertStringToNumber( ctx, dA, & dC ) ==0 ) goto FailConversion;
		return dC;
	}
	switch( ct->tid ){
	case DAO_INTEGER :
		dC.v.i = DValue_GetInteger( dA );
		break;
	case DAO_FLOAT :
		dC.v.f = DValue_GetFloat( dA );
		break;
	case DAO_DOUBLE :
		dC.v.d = DValue_GetDouble( dA );
		break;
	case DAO_COMPLEX :
		if( dA.t == DAO_COMPLEX ) goto Rebind;
		if( dA.t >= DAO_ARRAY ) goto FailConversion;
		/* do not allocate complex here, 
		 * the caller should not be responsible to free it.
		 * the same for string. */
		dC.v.c = cb;
		* dC.v.c = DValue_GetComplex( dA );
		break;
	case DAO_LONG :
		if( dA.t == DAO_LONG ) goto Rebind;
		if( dA.t >= DAO_ARRAY ) goto FailConversion;
		switch( dA.t ){
		case DAO_INTEGER :
			DLong_FromInteger( lb, DValue_GetInteger( dA ) );
			break;
		case DAO_FLOAT :
		case DAO_DOUBLE :
			DLong_FromDouble( lb, DValue_GetDouble( dA ) );
			break;
		case DAO_STRING :
			DLong_FromString( lb, dA.v.s );
			break;
		default : break;
		}
		dC.t = DAO_LONG;
		dC.v.l = lb;
		break;
	case DAO_STRING :
		if( dA.t == DAO_STRING ) goto Rebind;
		dC.v.s = sb;
		str = dC.v.s;
		if( dA.t < DAO_ARRAY ){
			DValue_GetString( dA, str );
#ifdef DAO_WITH_NUMARRAY
		}else if( dA.t == DAO_ARRAY ){
			array2 = dA.v.array;
			DaoArray_ToWCString( array2, str, 0, array2->size );
#endif
		}else if( dA.t == DAO_LIST ){
			shape = DArray_New(0);
			type = DaoBase_CheckTypeShape( dA, DAO_UDF, shape, 0, 1 );
			if( type <=0 || type >= DAO_COMPLEX ||shape->size >1 ) goto FailConversion;
			list = dA.v.list;
			DString_Resize( wcs, list->items->size );
			type = 1; /*MBS*/
			for(i=0; i<list->items->size; i++){
				wcs->wcs[i] = DValue_GetInteger( list->items->data[i] );
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
		if( dA.t == DAO_STRING ){
			str = dA.v.s;
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
		}else if( dA.t == DAO_ARRAY ){
			if( tp == NULL ) goto Rebind;
			if( tp->tid <DAO_INTEGER || tp->tid >DAO_COMPLEX ) goto FailConversion;
			array2 = dA.v.array;
			size = array2->size;
			if( tp->tid ==DAO_COMPLEX && array2->numType == DAO_COMPLEX ) goto Rebind;
			array = DaoArray_New( tp->tid );
			if( array2->numType == DAO_DOUBLE && tp->tid == DAO_COMPLEX )
				array->numType = DAO_COMPLEX;
			DaoArray_ResizeArray( array, array2->dims->items.pSize, array2->dims->size );
			if( tp->tid == array2->numType ){
				/* might be between int array and bit array */
				int bs = sizeof(int) * ( (tp->tid == DAO_DOUBLE) + 1 );
				memmove( array->data.p, array2->data.p, bs * size );
			}else if( array2->numType == DAO_INTEGER ){
				if( tp->tid == DAO_FLOAT ){
					for( i=0; i<size; i++ ) array->data.f[i] = array2->data.i[i];
				}else if( tp->tid == DAO_DOUBLE ){
					for( i=0; i<size; i++ ) array->data.d[i] = array2->data.i[i];
				}else if( tp->tid == DAO_COMPLEX ){
					for( i=0; i<size; i++ ) array->data.c[i].real = array2->data.i[i];
				}
			}else if( array2->numType == DAO_FLOAT ){
				if( tp->tid == DAO_INTEGER ){
					for( i=0; i<size; i++ ) array->data.i[i] = (int)array2->data.f[i];
				}else if( tp->tid == DAO_DOUBLE ){
					for( i=0; i<size; i++ ) array->data.d[i] = array2->data.f[i];
				}else if( tp->tid == DAO_COMPLEX ){
					for( i=0; i<size; i++ ) array->data.c[i].real = array2->data.f[i];
				}
			}else if( array2->numType ==  DAO_DOUBLE ){
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
		}else if( dA.t == DAO_LIST ){
			shape = DArray_New(0);
			type = DaoBase_CheckTypeShape( dA, DAO_UDF, shape, 0, 1 );
			if( type <0 ) goto FailConversion;
			array = DaoArray_New( DAO_INTEGER );
			dC.v.array = array;
			array->numType = DAO_DOUBLE;
			if( tp == NULL ) goto FailConversion;
			if( tp->tid ==0 || tp->tid > DAO_COMPLEX ) goto FailConversion;
			array->numType = tp->tid;
			DaoArray_ResizeArray( array, shape->items.pSize, shape->size );
			DaoBase_ExportValue( dA, array, 0 );
		}else goto FailConversion;
		dC.v.array = array;
		array->unitype = ct;
		GC_IncRC( ct );
		break;
#endif
	case DAO_LIST :
		list = DaoList_New();
		list->unitype = ct;
		GC_IncRC( ct );
		dC.v.list = list;
		if( ct->nested->size >0 ) tp = ct->nested->items.pType[0];
		if( tp == NULL ) goto FailConversion;
		value.t = tp->tid;
		value.v.d = 0.0;
		if( dA.t == DAO_STRING ){
			str = dA.v.s;
			if( tp->tid < DAO_INTEGER || tp->tid > DAO_DOUBLE ) goto FailConversion;
			DVarray_Resize( list->items, DString_Size( str ), value );
			data = list->items->data;
			if( str->mbs ){
				for(i=0; i<str->size; i++){
					switch( tp->tid ){
					case DAO_INTEGER : data[i].v.i = str->mbs[i]; break;
					case DAO_FLOAT   : data[i].v.f = str->mbs[i]; break;
					case DAO_DOUBLE  : data[i].v.d = str->mbs[i]; break;
					default : break;
					}
				}
			}else{
				for(i=0; i<str->size; i++){
					switch( tp->tid ){
					case DAO_INTEGER : data[i].v.i = str->wcs[i]; break;
					case DAO_FLOAT   : data[i].v.f = str->wcs[i]; break;
					case DAO_DOUBLE  : data[i].v.d = str->wcs[i]; break;
					default : break;
					}
				}
			}
#ifdef DAO_WITH_NUMARRAY
		}else if( dA.t == DAO_ARRAY ){
			DaoArray_ToList( dA.v.array, list, ct, 0, 0 );
#endif
		}else if( dA.t == DAO_LIST ){
			list2 = dA.v.list;
			DVarray_Resize( list->items, list2->items->size, daoNullValue );
			data = list->items->data;
			for(i=0; i<list2->items->size; i++ ){
				value = DaoTypeCast( ctx, tp, list2->items->data[i], cb, lb, sb );
				if( value.t ==0 ) goto FailConversion;
				DValue_Copy( data + i, value );
			}
		}else goto FailConversion;
		break;
	case DAO_MAP :
		if( dA.t != DAO_MAP ) goto FailConversion;
		map2 = dA.v.map;
		if( map2->unitype ){
			short m = DaoType_MatchTo( map2->unitype, ct, NULL );
			if( m == DAO_MT_ANY || m == DAO_MT_EQ ) goto Rebind;
		}
		map = DaoMap_New(0);
		map->unitype = ct;
		GC_IncRC( ct );
		dC.v.map = map;
		dC.t = DAO_MAP;
		if( ct->nested->size >0 ) tp = ct->nested->items.pType[0];
		if( ct->nested->size >1 ) tp2 = ct->nested->items.pType[1];
		if( tp == NULL || tp2 == NULL ) goto FailConversion;
		node = DMap_First( map2->items );
		for(; node!=NULL; node=DMap_Next(map2->items,node) ){
			key = DaoTypeCast( ctx, tp, node->key.pValue[0], cb, lb, sb );
			value = DaoTypeCast( ctx, tp2, node->value.pValue[0], cb, lb, sb );
			if( key.t ==0 || value.t ==0 ) goto FailConversion;
			DMap_Insert( map->items, & key, & value );
		}
		break;
	case DAO_TUPLE :
		dC.t = DAO_TUPLE;
		if( dA.t == DAO_LIST || dA.t == DAO_TUPLE ){
			DValue *items;
			int size = 0;
			int tsize = ct->nested->size;
			if( dA.t == DAO_LIST ){
				size = dA.v.list->items->size;
				items = dA.v.list->items->data;
			}else{
				size = dA.v.tuple->items->size;
				items = dA.v.tuple->items->data;
			}
			if( size < tsize ) goto FailConversion;
			if( tsize ) size = tsize;
			tuple = DaoTuple_New( size );
			GC_IncRC( ct );
			tuple->unitype = ct;
			dC.v.tuple = tuple;
			for(i=0; i<size; i++){
				value = items[i];
				tp = DaoNameSpace_GetTypeV( ns, value );
				if( tsize ){
					tp2 = ct->nested->items.pType[i];
					if( tp2->tid == DAO_PAR_NAMED ) tp2 = tp2->X.abtype;
					/* if( DaoType_MatchTo( tp, tp2, 0 ) ==0 ) goto FailConversion; */
					value = DaoTypeCast( ctx, tp2, value, cb, lb, sb );
				}
				if( value.t == 0 ) goto FailConversion;
				DValue_Copy( tuple->items->data + i, value );
			}
		}else if( dA.t == DAO_MAP ){
			i = 0;
			tuple = DaoTuple_New( dA.v.map->items->size );
			dC.v.tuple = tuple;
			tuple->unitype = ct;
			GC_IncRC( ct );
			node = DMap_First( dA.v.map->items );
			for(; node!=NULL; node=DMap_Next(dA.v.map->items,node) ){
				if( i >= ct->nested->size ){
					DValue_Copy( tuple->items->data + i, value );
				}else{
					tp2 = ct->nested->items.pType[i];
					if( node->key.pValue->t != DAO_STRING ) goto FailConversion;
					value = DaoTypeCast( ctx, tp2, node->value.pValue[0], cb, lb, sb );
					if( value.t ==0 ) goto FailConversion;
					DValue_Copy( tuple->items->data + i, value );
				}
				i ++;
			}
		}else{
			goto FailConversion;
		}
		break;
	case DAO_OBJECT :
		if( dA.t == DAO_CDATA && dA.v.cdata->daoObject ){
			dA.t = DAO_OBJECT;
			dA.v.object = dA.v.cdata->daoObject;
		}
		/* XXX compiling time checking */
		if( dA.t != DAO_OBJECT ) goto FailConversion;
		dC.v.p = DaoObject_MapThisObject( dA.v.object, ct );
		if( dC.v.p == NULL ) goto FailConversion;
		dC.t = dC.v.p->type;
		break;
	case DAO_CDATA :
		if( dA.t == DAO_CDATA ){
			if( DaoCData_ChildOf( dA.v.cdata->typer, ct->typer ) ){
				dC = dA;
				/*
				   }else if( DaoCData_ChildOf( ct->typer, dA->typer ) ){
				// not work for C++ types, that require reinterpret_cast<>
				dA->typer = ct->typer;
				dC = dA;
				 */
			}else{
			}
		}else if( dA.t == DAO_OBJECT ){
			dC.v.p = DaoObject_MapThisObject( dA.v.object, ct );
			if( dC.v.p == NULL ) goto FailConversion;
			dC.t = dC.v.p->type;
		}else{
			goto FailConversion;
		}
		break;
	default : break;
	}
	if( wcs ) DString_Delete( wcs );
	if( shape ) DArray_Delete( shape );
	return dC;
Rebind :
	if( wcs ) DString_Delete( wcs );
	if( shape ) DArray_Delete( shape );
#ifdef DAO_WITH_NUMARR
	if( array ) DaoArray_Delete( array );
#endif
	return dA;
FailConversion :
	if( wcs ) DString_Delete( wcs );
	if( shape ) DArray_Delete( shape );
#ifdef DAO_WITH_NUMARR
	if( array ) DaoArray_Delete( array );
#endif
	if( map ) DaoMap_Delete( map );
	if( list ) DaoList_Delete( list );
	if( tuple ) DaoTuple_Delete( tuple );
	dC.t = 0;
	return dC;
}
void DaoContext_DoCast( DaoContext *self, DaoVmCode *vmc )
{
	DaoType *at, *ct = self->regTypes[ vmc->c ];
	DValue va = *self->regValues[ vmc->a ];
	DValue *vc = self->regValues[ vmc->c ];
	DString *sb = NULL;
	DLong *lb = NULL;
	DNode *node;
	complex16 cb;

	self->vmc = vmc;
	if( va.t == vc->t && va.v.d == vc->v.d ) return;
	if( va.t ==0 && va.v.p ==NULL ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "operate on null object" );
		return;
	}
	if( ct == NULL || ct->type == DAO_UDF || ct->type == DAO_ANY ){
		DaoContext_DoMove( self, vmc );
		return;
	}
	if( ct->tid != DAO_COMPLEX  && vc->t == DAO_COMPLEX ) dao_free( vc->v.c );
	if( ct->tid != DAO_LONG  && vc->t == DAO_LONG ) DLong_Delete( vc->v.l );
	if( ct->tid != DAO_ENUM  && vc->t == DAO_ENUM ) DEnum_Delete( vc->v.e );
	if( ct->tid != DAO_STRING  && vc->t == DAO_STRING ) DString_Delete( vc->v.s );
	if( ct->tid == DAO_COMPLEX && vc->t != DAO_COMPLEX )
		vc->v.c = dao_malloc( sizeof(complex16) );
	if( ct->tid == DAO_LONG && vc->t != DAO_LONG ) vc->v.l = DLong_New();
	if( ct->tid == DAO_ENUM && vc->t != DAO_ENUM ) vc->v.e = DEnum_New(NULL,0);
	if( ct->tid == DAO_STRING && vc->t != DAO_STRING ) vc->v.s = DString_New(1);

	if( ct->tid == DAO_ENUM && va.t == DAO_ENUM ){
		int i;
		vc->t = DAO_ENUM;
		DEnum_SetType( vc->v.e, ct );
		if( DEnum_SetValue( vc->v.e, va.v.e, NULL ) ==0 ) goto FailConversion;
		return;
	}else if( ct->tid == DAO_ENUM && va.t == DAO_INTEGER ){
		vc->t = DAO_ENUM;
		if( ct->mapNames == NULL ) goto FailConversion;
		for(node=DMap_First(ct->mapNames);node;node=DMap_Next(ct->mapNames,node)){
			if( node->value.pInt == va.v.i ) break;
		}
		if( node == NULL ) goto FailConversion;
		DEnum_SetType( vc->v.e, ct );
		vc->v.e->value = node->value.pInt;
		return;
	}else if( ct->tid == DAO_ENUM && va.t == DAO_STRING ){
		vc->t = DAO_ENUM;
		if( ct->mapNames == NULL ) goto FailConversion;
		node = DMap_Find( ct->mapNames, va.v.s );
		if( node == NULL ) goto FailConversion;
		DEnum_SetType( vc->v.e, ct );
		vc->v.e->value = node->value.pInt;
		return;
	}else if( ct->tid == DAO_ENUM ){
		goto FailConversion;
	}

	if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_STRING ){
		if( va.t >= DAO_INTEGER && va.t <= DAO_STRING ){
			vc->t = ct->tid;
			if( va.t == DAO_STRING && ct->tid != DAO_STRING ){
				if( ConvertStringToNumber( self, va, vc ) ==0 ) goto FailConversion;
				return;
			}
			switch( ct->tid ){
			case DAO_INTEGER : vc->v.i = DValue_GetInteger( va ); break;
			case DAO_FLOAT   : vc->v.f = DValue_GetFloat( va );  break;
			case DAO_DOUBLE  : vc->v.d = DValue_GetDouble( va ); break;
			case DAO_COMPLEX : *vc->v.c = DValue_GetComplex( va ); break;
			case DAO_LONG    : DValue_GetLong( va, vc->v.l ); break;
			case DAO_STRING  : DValue_GetString( va, vc->v.s ); break;
			default : break;
			}
			return;
		}
	}
	sb = DString_New(1);
	lb = DLong_New();
	va = DaoTypeCast( self, ct, va, & cb, lb, sb );
	if( va.t == 0 ) goto FailConversion;
	DValue_Copy( vc, va );
	DString_Delete( sb );
	DLong_Delete( lb );
	return;
FailConversion :
	if( sb ) DString_Delete( sb );
	if( lb ) DLong_Delete( lb );
	at = DaoNameSpace_GetTypeV( self->nameSpace, *self->regValues[ vmc->a ] );
	DaoContext_RaiseTypeError( self, at, ct, "casting" );
}
void DaoContext_DoMove( DaoContext *self, DaoVmCode *vmc )
{
	DaoType *ct = self->regTypes[ vmc->c ];
	DValue *dA = self->regValues[ vmc->a ];
	DValue dC = *self->regValues[ vmc->c ];
	int overload = 0;
	if( dA->t == dC.t && dC.t == DAO_OBJECT ){
		overload = DaoClass_ChildOf( dA->v.object->myClass, (DaoBase*)dC.v.object->myClass ) == 0;
	}else if( dC.t == DAO_OBJECT ){
		overload = 1;
	}
	if( overload ){
		DaoClass *scope = self->object ? self->object->myClass : NULL;
		DRoutine *rout = (DRoutine*)DaoClass_FindOperator( dC.v.object->myClass, "=", scope );
		DValue selfpar = daoNullObject;
		selfpar.v.object = dC.v.object;
		if( rout ) rout = DRoutine_GetOverLoad( rout, & selfpar, & dA, 1, DVM_CALL );
		if( rout && DaoVmProcess_Call( self->process, (DaoRoutine*)rout, dC.v.object, & dA, 1 ) ) return;
	}
	DaoMoveAC( self, *self->regValues[vmc->a], self->regValues[vmc->c], ct );
}
int DaoContext_CheckFE( DaoContext *self )
{
	int res = 0;
	if( dao_fe_status() ==0 ) return 0;
	if( dao_fe_divbyzero() ){
		DaoContext_RaiseException( self, DAO_ERROR_FLOAT_DIVBYZERO, "" );
		res = 1;
	}else if( dao_fe_underflow() ){
		DaoContext_RaiseException( self, DAO_ERROR_FLOAT_UNDERFLOW, "" );
		res = 1;
	}else if( dao_fe_overflow() ){
		DaoContext_RaiseException( self, DAO_ERROR_FLOAT_OVERFLOW, "" );
		res = 1;
#if 0
	}else if( dao_fe_invalid() ){
		/* disabled, because some extending modules may easily produce 
		   harmless float point errors */
		DaoContext_RaiseException( self, DAO_ERROR_FLOAT, "" );
		res = 1;
#endif
	}
	self->idClearFE = self->vmc - self->codes;
	dao_fe_clear();
	return res;
}
static DaoRoutine* DaoRoutine_Decorate( DaoRoutine *self, DaoRoutine *decoFunc, DValue *p[], int n )
{
	DArray *nested = decoFunc->routType->nested;
	DaoType **decotypes = nested->items.pType;
	DaoParser *parser = DaoParser_New();
	DaoRoutine *routine = DaoRoutine_New();
	DaoRoutine tmp = *self;
	DMap *mapids = DMap_New(0,D_STRING);
	int parpass[DAO_MAX_PARAM];
	int i, j, k;
	parser->routine = routine;
	parser->nameSpace = routine->nameSpace = decoFunc->nameSpace;
	parser->vmSpace = routine->nameSpace->vmSpace;
	GC_IncRC( routine->nameSpace );
	routine->parCount = self->parCount;
	routine->attribs = self->attribs;
	routine->tidHost = self->tidHost;
	GC_ShiftRC( self->routHost, routine->routHost );
	GC_ShiftRC( self->routType, routine->routType );
	routine->routHost = self->routHost;
	routine->routType = self->routType;
	DString_Assign( routine->routName, self->routName );
	DString_Assign( routine->parCodes, self->parCodes );
	for(i=0; i<self->routType->nested->size; i++){
		DaoType *type = self->routType->nested->items.pType[i];
		DRoutine_AddConstValue( (DRoutine*)routine, self->routConsts->data[i] );
		if( type->tid == DAO_PAR_VALIST ) break;
		MAP_Insert( DArray_Top( parser->localVarMap ), type->fname, i );
		DArray_Append( routine->defLocals, self->defLocals->items.pToken[i] );
	}
	parser->locRegCount = self->parCount;
	i = DRoutine_AddConst( (DRoutine*)routine, (DaoBase*) self );
	MAP_Insert( DArray_Top( parser->localCstMap ), decotypes[0]->fname, i );

	k = 1;
	for(i=0; i<nested->size; i++) parpass[i] = 0;
	for(i=0; i<n; i++){
		DValue pv = *p[i];
		if( pv.t == DAO_PAR_NAMED ){
			DaoPair *pair = pv.v.pair;
			DNode *node = DMap_Find( decoFunc->routType->mapNames, pair->first.v.s );
			if( node == NULL ) goto ErrorDecorator;
			pv = pair->second;
			k = node->value.pInt;
		}
		j = DRoutine_AddConstValue( (DRoutine*)routine, pv );
		MAP_Insert( DArray_Top( parser->localCstMap ), decotypes[k]->fname, j );
		parpass[k] = 1;
		k += 1;
	}
	for(i=1; i<nested->size; i++){
		k = decotypes[i]->tid;
		if( k == DAO_PAR_VALIST ) break;
		if( parpass[i] ) continue;
		if( k != DAO_PAR_DEFAULT ) continue;
		k = DRoutine_AddConstValue( (DRoutine*)routine, decoFunc->routConsts->data[i] );
		MAP_Insert( DArray_Top( parser->localCstMap ), decotypes[i]->fname, k );
		parpass[i] = 1;
	}

	/* if( decoFunc->parser ) DaoRoutine_Compile( decoFunc ); */
	DArray_Assign( parser->tokens, decoFunc->source );
	if( DaoParser_ParseRoutine( parser ) ==0 ) goto ErrorDecorator;
	/* DaoRoutine_PrintCode( routine, self->nameSpace->vmSpace->stdStream ); */
	DaoParser_Delete( parser );
	return routine;
ErrorDecorator:
	DaoRoutine_Delete( routine );
	DaoParser_Delete( parser );
	DMap_Delete( mapids );
	return NULL;
}
void DRoutine_ShowError( DRoutine *self, DaoType *selftype,
		DValue *csts, DaoType *ts[], int np, int code, DArray *errors );
void DaoPrintCallError( DArray *errors, DaoStream *stdio );

void DaoContext_ShowCallError( DaoContext *self, DRoutine *rout, 
		DValue *selfobj, DValue *ps[], DaoType *ts[], int np, int code )
{
	DaoStream *ss = DaoStream_New();
	DaoNameSpace *ns = self->nameSpace;
	DaoType *selftype = selfobj ? DaoNameSpace_GetTypeV( ns, *selfobj ) : NULL;
	DValue csts[ DAO_MAX_PARAM + 1 ];
	DArray *errors = DArray_New(0);
	int i, k;
	for(i=0; i<np; i++) csts[i] = *ps[i];
	DRoutine_ShowError( rout, selftype, csts, ts, np, code, errors );
	ss->attribs |= DAO_IO_STRING;
	DaoPrintCallError( errors, ss );
	DArray_Delete( errors );
	DaoContext_RaiseException( self, DAO_ERROR_PARAM, ss->streamString->mbs );
	DaoStream_Delete( ss );
}

extern DaoClass *daoClassFutureValue;
void DaoContext_DoCall( DaoContext *self, DaoVmCode *vmc )
{
	int i, sup = 0, code = vmc->code;
	int mode = vmc->b & 0xff00;
	int npar = vmc->b & 0xff;
	DValue  *base = self->regArray->data + vmc->a + 1;
	DValue **params = self->regValues + vmc->a + 1;
	DValue parbuf[DAO_MAX_PARAM+1];
	DValue *parbuf2[DAO_MAX_PARAM+1];
	DValue selfpar0 = daoNullValue;
	DValue *selfpar = & selfpar0;
	DValue caller = *self->regValues[ vmc->a ];
	DRoutine *rout = NULL;
	DRoutine *rout2 = NULL;
	DaoFunction *func = NULL;
	DaoVmProcess *proc = self->process;
	DaoVmProcess *vmp;
	DaoContext *ctx;
	DaoTuple *tuple;
	DaoVmCode *vmc2;
	int initbase = 0;
	int async = 0;
	int tail = 0;

	//printf( "DoCall: %p %i %i\n", self->routine, self->routine->parCount, self->parCount );
	if( npar == DAO_CALLER_PARAM ){
		npar = self->parCount;
		base = self->regArray->data;
		params = self->regValues;
	}
	memset( parbuf, 0, (DAO_MAX_PARAM+1)*sizeof(DValue) );
	for(i=0; i<=DAO_MAX_PARAM; i++) parbuf2[i] = parbuf + i;
	self->vmc = vmc;
	if( DaoContext_CheckFE( self ) ) return;
	if( caller.t ==0 ){
		DaoContext_RaiseException( self, DAO_ERROR_TYPE, "null object not callable" );
		return;
	}

	if( mode & DAO_CALL_COROUT ){
		vmp = DaoVmProcess_Create( self, self->regValues + vmc->a, npar+1 );
		if( vmp == NULL ) return;
		GC_ShiftRC( self->regTypes[ vmc->c ], vmp->abtype );
		vmp->abtype = self->regTypes[ vmc->c ];
		DaoContext_SetResult( self, (DaoBase*) vmp );
		return;
	}else if( mode || caller.t == DAO_FUNCURRY ){
		if( self->process->array == NULL ) self->process->array = DArray_New(0);
		DArray_Clear( self->process->array );
		base = NULL;
		if( caller.t == DAO_FUNCURRY ){
			DaoFunCurry *curry = (DaoFunCurry*) caller.v.p;
			caller = curry->callable;
			selfpar = & curry->selfobj;
			for( i=0; i<curry->params->size; i++ ){
				DArray_Append( self->process->array, curry->params->data + i );
			}
			for( i=0; i<npar; i++ ) DArray_Append( self->process->array, params[i] );
			npar = self->process->array->size;
			params = self->process->array->items.pValue;
		}
		if( mode & DAO_CALL_EXPAR ){
			for( i=0; i<npar-1; i++ ) DArray_Append( self->process->array, params[i] );
			if( npar > 1 && params[npar-1]->t == DAO_TUPLE ){
				DValue *data;
				tuple = params[npar-1]->v.tuple;
				npar = tuple->items->size;
				data = tuple->items->data;
				for( i=0; i<npar; i++ ) DArray_Append( self->process->array, data + i );
			}else if( npar > 1 ){
				DArray_Append( self->process->array, params[npar-1] );
			}
			npar = self->process->array->size;
			params = self->process->array->items.pValue;
		}
	}
	if( caller.t == DAO_ROUTINE || caller.t == DAO_FUNCTION ){
		rout = (DRoutine*) caller.v.p;
	}

	if( self->object && (vmc->code == DVM_CALL || vmc->code == DVM_CALL_TC) ){
		selfpar->t = DAO_OBJECT;
		selfpar->v.object = self->object;
	}
	if( (mode & DAO_CALL_INIT) && self->object ){
		DaoClass *klass = self->object->myClass;
		if( DString_EQ( self->routine->routName, klass->className )
				|| self->routine == klass->classRoutine ){
			sup = DaoClass_FindSuper( klass, caller.v.p );
			if( sup >= 0 ) initbase = 1;
		}
	}

	/*
	   printf("caller = %i; %i\n", caller.t, npar );
	   printf("selfpar = %i; %i\n", selfpar->t ? selfpar->t : 0, vmc->a +1 );
	   printf( "rout = %p %s %s\n", rout, rout->routName->mbs, rout->routType->name->mbs );
	 */

	if( caller.t == DAO_FUNCTION || caller.t == DAO_CDATA ){
		rout = (DRoutine*) caller.v.p;
		if( caller.t == DAO_CDATA ){
			DaoCData *cdata = caller.v.cdata;
			DaoFunction *func = NULL;
			if( cdata->data == NULL && (cdata->trait & DAO_DATA_CONST) ){
				func = DaoFindFunction2( cdata->typer, cdata->typer->name );
				if( func == NULL ){
					DaoContext_RaiseException( self, DAO_ERROR_TYPE, "c type object not callable" );
					return;
				}
			}else{
				func = DaoFindFunction2( cdata->typer, "()" );
				if( func == NULL ){
					DaoContext_RaiseException( self, DAO_ERROR_TYPE, "object not callable" );
					return;
				}
			}
			rout = (DRoutine*)func;
			rout = DRoutine_GetOverLoad( (DRoutine*)rout, selfpar, params, npar, code );
		}else{
			rout = DRoutine_GetOverLoad( (DRoutine*)rout, selfpar, params, npar, code );
		}
		if( rout == NULL ){
			rout2 = (DRoutine*) caller.v.routine;
			goto InvalidParameter;
		}else if( rout->type == DAO_ROUTINE ){
			DaoContext_RaiseException( self, DAO_ERROR, "not supported overloading" );
			return;
		}
		if( (self->vmSpace->options &DAO_EXEC_SAFE) && (rout->attribs &DAO_ROUT_EXTFUNC) ){
			/* normally this condition will not be satisfied.
			 * it is possible only if the safe mode is set in C codes 
			 * by embedding or extending. */
			DaoContext_RaiseException( self, DAO_ERROR, "not permitted" );
			return;
		}
		/* DArray_Resize( self->parbuf, rout->parCount, 0 ); */
		if( ! DRoutine_PassParams( (DRoutine*)rout, selfpar, parbuf2, params, base, npar, vmc->code ) ){
			for(i=0; i<=rout->parCount; i++) DValue_Clear( parbuf2[i] );
			rout2 = (DRoutine*) rout;
			goto InvalidParameter;
		}
		/* foo: routine<x:int,s:string>
		 *   ns.foo( 1, "" );
		 * bar: routine<self:cdata,x:int>
		 *   obj.bar(1);
		 * inside: Dao class member method:
		 *   bar(1); # pass Dao class instances as self 
		 */
		func = (DaoFunction*)rout;
		if( vmc->code == DVM_MCALL && ! (func->attribs & DAO_ROUT_PARSELF)) npar --;
		if( !(rout->attribs & DAO_ROUT_ISCONST) ){
			for(i=0; i<npar; i++){
				if( parbuf2[i]->cst ){
					DValue_ClearAll( parbuf, rout->parCount+1 );
					DaoContext_RaiseException( self, DAO_ERROR, "calling non-const function on constant" );
					return;
				}
			}
		}
		/*
		   printf( "call: %s %i\n", rout->routName->mbs, npar );
		 */
		self->thisFunction = func;
		func->pFunc( self, parbuf2, npar );
		self->thisFunction = NULL;
		/* DValue_ClearAll( parbuf, rout->parCount+1 ); */
		for(i=0; i<=rout->parCount; i++){
			if( parbuf[i].ndef ) continue;
			DValue_Clear( parbuf+i );
		}

		if( initbase ){
			DaoCData *cdata = self->regValues[ vmc->c ]->v.cdata;
			if( cdata && cdata->type == DAO_CDATA ){
				GC_ShiftRC( cdata, self->object->superObject->items.pBase[sup] );
				self->object->superObject->items.pBase[sup] = (DaoBase*) cdata;
				GC_ShiftRC( self->object->that, cdata->daoObject );
				cdata->daoObject = self->object->that;
			}
		}
		if( DaoContext_CheckFE( self ) ) return;
		if( self->process->status==DAO_VMPROC_SUSPENDED )
			self->process->topFrame->entry = (short)(vmc - self->codes);
	}else if( caller.t==DAO_ROUTINE || caller.t==DAO_CLASS || caller.t==DAO_OBJECT ){
		DaoRoutine *rout = NULL;
		DaoObject  *obj = NULL;
		int inclass = 0;

		ctx = 0;
		if( caller.t==DAO_ROUTINE ){
			rout = caller.v.routine;
			rout = (DaoRoutine*) DRoutine_GetOverLoad( (DRoutine*)rout, selfpar, params, npar, code );
			if( rout == NULL ){
				rout2 = (DRoutine*) caller.v.routine;
				goto InvalidParameter;
			}
			if( rout->type == DAO_FUNCTION ){
				DaoContext_RaiseException( self, DAO_ERROR, "not supported overloading" );
				return;
			}
			if( rout->routName->mbs[0] == '@' ){
				rout = DaoRoutine_Decorate( params[0]->v.routine, rout, params+1, npar-1 );
				DaoContext_SetResult( self, (DaoBase*) rout );
				return;
			}
			ctx = DaoVmProcess_MakeContext( self->process, rout );
			if( rout->tidHost == DAO_OBJECT ){
				if( selfpar->t == DAO_OBJECT ){
					obj = (DaoObject*) DaoObject_MapThisObject( selfpar->v.object->that, rout->routHost );
					GC_ShiftRC( obj, ctx->object );
					ctx->object = obj;
					if( selfpar->v.object == self->object && obj ) inclass = 1;
#if 1
				}else if( params[0]->t == DAO_OBJECT ){ /* included in selfpar */
					obj = (DaoObject*) DaoObject_MapThisObject( params[0]->v.object, rout->routHost );
					GC_ShiftRC( obj, ctx->object );
					ctx->object = obj;
#endif
				}else if( rout->attribs & DAO_ROUT_NEEDSELF ){
					/* Only class method that does not use object data 
					 * will be allowed to be called without object
					 */
					DaoContext_RaiseException( self, DAO_ERROR, "need self object" );
					return;
				}
			}
		}else if( caller.t==DAO_CLASS ){
			DaoClass *klass = caller.v.klass;
			DaoObject *othis = NULL, *onew = NULL;
			DValue tmp = daoNullObject;
			if( initbase ){
				othis = self->object;
			}else{
				othis = onew = DaoObject_New( klass, NULL, 0 );
			}
			tmp.v.object = othis;
			rout = (DaoRoutine*)klass->classRoutine;
			rout = (DaoRoutine*)DRoutine_GetOverLoad( (DRoutine*)rout, selfpar, params, npar, code );
			if( rout == NULL ){
				selfpar = & tmp;
				rout = (DaoRoutine*)klass->classRoutine;
				rout = (DaoRoutine*)DRoutine_GetOverLoad( (DRoutine*)rout, selfpar, params, npar, code );
			}
			if( rout == NULL && (npar ==0 || (npar == 1 && (code == DVM_MCALL || code == DVM_MCALL_TC) ) ) ){
				/* default contstructor */
				rout = (DaoRoutine*) klass->classRoutine;
			}
			if( rout == NULL ){
				rout2 = (DRoutine*) klass->classRoutine;
				goto InvalidParameter;
			}
			if( rout->type == DAO_FUNCTION ){
				DaoFunction *func = (DaoFunction*) rout;
				DaoVmCode vmcode = { DVM_CALL, 0, 0, 0 };
				DValue returned = daoNullValue;
				DValue *returned2 = & returned;
				if( ! DRoutine_PassParams( (DRoutine*)rout, selfpar, parbuf2, params, base, npar, vmc->code ) ){
					for(i=0; i<=rout->parCount; i++) DValue_Clear( parbuf2[i] );
					if( onew ){ GC_IncRC( onew ); GC_DecRC( onew ); }
					rout2 = (DRoutine*) rout;
					goto InvalidParameter;
					return;
				}
				ctx = DaoVmProcess_MakeContext( self->process, rout );
				ctx->vmc = & vmcode;
				ctx->regValues = & returned2;
				ctx->regTypes = & rout->routHost;
				self->thisFunction = func;
				func->pFunc( ctx, parbuf2, npar );
				self->thisFunction = NULL;
				ctx->regValues = NULL;
				/* DValue_ClearAll( parbuf, rout->parCount+1 ); */
				for(i=0; i<=rout->parCount; i++){
					if( parbuf[i].ndef ) continue;
					DValue_Clear( parbuf+i );
				}
				obj = (DaoObject*) DaoObject_MapChildObject( othis, rout->routHost );
				sup = DaoClass_FindSuper( obj->myClass, rout->routHost->X.extra );
				if( returned.t == DAO_CDATA ){
					DaoCData *cdata = returned.v.cdata;
					GC_ShiftRC( cdata, obj->superObject->items.pBase[sup] );
					obj->superObject->items.pBase[sup] = (DaoBase*) cdata;
					GC_ShiftRC( obj->that, cdata->daoObject );
					cdata->daoObject = obj->that;
				}
				DaoContext_SetResult( self, (DaoBase*) othis );
				DValue_Clear( & returned );
				return;
			}else if( rout != NULL ){
				ctx = DaoVmProcess_MakeContext( self->process, rout );
				obj = othis;
				if( initbase ){
					obj = (DaoObject*) DaoObject_MapThisObject( obj, rout->routHost );
					GC_ShiftRC( obj, ctx->object );
					ctx->object = obj;
				}else{
					GC_ShiftRC( obj, ctx->object );
					ctx->object = obj;
					ctx->ctxState = DVM_MAKE_OBJECT;
				}
			}else{
				if( onew ){ GC_IncRC( onew ); GC_DecRC( onew ); }
				DaoContext_RaiseException( self, DAO_ERROR_PARAM, "not matched (class)" );
			}
		}else{ /* DaoObject */
			DString *name = DString_New(1);
			DValue p = daoNullValue;
			obj = caller.v.object;
			DString_SetMBS( name, "()" );
			DaoObject_GetData( obj, name, & p, self->object, NULL );
			DString_Delete( name );
			if( p.t != DAO_ROUTINE ){
				DaoContext_RaiseException( self, DAO_ERROR_TYPE, "class instance not callable" );
				return;
			}
			rout = (DaoRoutine*)DRoutine_GetOverLoad( (DRoutine*)p.v.p, selfpar, params, npar, code );
			ctx = DaoVmProcess_MakeContext( self->process, rout );
			GC_ShiftRC( obj, ctx->object );
			ctx->object = obj;
		}
		if( ctx == NULL ) return;
		ctx->parCount = npar;

		if( ! DRoutine_PassParams( (DRoutine*)ctx->routine, selfpar, ctx->regValues, params, base, npar, vmc->code ) ){
			DaoContext_RaiseException( self, DAO_ERROR_PARAM, "not matched (passing)" );
			return;
		}

#if( defined DAO_WITH_THREAD && defined DAO_WITH_SYNCLASS )
		async = code == DVM_MCALL && params[0]->t == DAO_OBJECT;
		async = async && (params[0]->v.object->myClass->attribs & DAO_CLS_SYNCHRONOUS);
		if( self->object ) async = async && !DaoObject_ChildOf( self->object->that, params[0]->v.object );
		if( async ){
			DaoNameSpace *ns = self->nameSpace;
			DaoFuture *future = DaoCallServer_Add( ctx, NULL, NULL );
			DaoType *retype = ctx->routine->routType->X.abtype;
			DaoType *type = DaoNameSpace_MakeType( ns, "future", DAO_FUTURE, NULL, &retype,1 );
			GC_ShiftRC( type, future->unitype );
			future->unitype = type;
			DaoContext_PutResult( self, (DaoBase*) future );
			return;
		}
#endif

		vmc2 = vmc + 1;
		if( (inclass && self->constCall) || (rout->attribs & DAO_ROUT_ISCONST) 
				|| (selfpar->t == DAO_OBJECT && selfpar->cst ) )
			ctx->constCall = 1;
		if( caller.t == DAO_ROUTINE && !(self->frame->state & DVM_MAKE_OBJECT) ){
			/* not in constructor */
			while( vmc2->code == DVM_NOP ) vmc2 ++;
			if( vmc2->code == DVM_RETURN && vmc2->c ==0 ) tail = vmc2->b ==0 || (vmc2->b ==1 && vmc2->a == vmc->c);
			if( tail ){
				DaoVmFrame *frame = self->frame;
				DaoVmProcess_PopContext( self->process );
				ctx->frame = frame;
				self->frame = frame->next;
				frame->context = ctx;
				frame->next->context = self;
				self = ctx;
			}
		}
		DaoVmProcess_PushContext( self->process, ctx );
	}else if( caller.t == DAO_VMPROCESS && caller.v.vmp->abtype ){
		DaoVmProcess *vmProc = caller.v.vmp;
		if( vmProc->status == DAO_VMPROC_FINISHED ){
			DaoContext_RaiseException( self, DAO_WARNING, "coroutine execution is finished." );
			return;
		}
		DaoVmProcess_Resume2( vmProc, params, npar, self );
		if( vmProc->status == DAO_VMPROC_ABORTED )
			DaoContext_RaiseException( self, DAO_ERROR, "coroutine execution is aborted." );
	}else{
		DaoContext_RaiseException( self, DAO_ERROR_TYPE, "object not callable" );
	}
	return;
InvalidParameter:
	DaoContext_ShowCallError( self, rout2, selfpar, params, NULL, npar, code );
}
void DaoContext_DoFastCall( DaoContext *self, DaoVmCode *vmc )
{
	int i, npar = vmc->b;
	int tail = 0;
	DaoVmCode *vmc2 = vmc + 1;
	DValue  *base = self->regArray->data + vmc->a + 1;
	DValue **params = self->regValues + vmc->a + 1;
	DValue parbuf[DAO_MAX_PARAM+1];
	DValue *parbuf2[DAO_MAX_PARAM+1];
	DValue selfpar0 = daoNullValue;
	DValue *selfpar = & selfpar0;
	DValue caller = *self->regValues[ vmc->a ];
	DRoutine *rout = NULL;
	DRoutine *rout2 = NULL;
	DaoContext *ctx;

	//printf( "DoFastCall: %p %i %i\n", self->routine, self->routine->parCount, self->parCount );
	//printf( "%i\n", npar );
	if( npar == DAO_CALLER_PARAM ){
		npar = self->parCount;
		base = self->regArray->data;
		params = self->regValues;
	}
	memset( parbuf, 0, (DAO_MAX_PARAM+1)*sizeof(DValue) );
	for(i=0; i<=DAO_MAX_PARAM; i++) parbuf2[i] = parbuf + i;
	self->vmc = vmc;
	if( DaoContext_CheckFE( self ) ) return;
	if( caller.t ==0 ){
		DaoContext_RaiseException( self, DAO_ERROR_TYPE, "null object not callable" );
		return;
	}

	if( self->object && (vmc->code == DVM_CALL || vmc->code == DVM_CALL_TC) ){
		selfpar->t = DAO_OBJECT;
		selfpar->v.object = self->object;
	}

	/*
	   printf("caller = %i; %i\n", caller.t, npar );
	   printf("selfpar = %i; %i\n", selfpar->t ? selfpar->t : 0, vmc->a +1 );
	 */

	if( caller.t == DAO_FUNCTION ){
		DaoFunction *func = (DaoFunction*) caller.v.p;
		rout = (DRoutine*) caller.v.p;
		if( (self->vmSpace->options &DAO_EXEC_SAFE) && (rout->attribs &DAO_ROUT_EXTFUNC) ){
			/* normally this condition will not be satisfied.
			 * it is possible only if the safe mode is set in C codes 
			 * by embedding or extending. */
			DaoContext_RaiseException( self, DAO_ERROR, "not permitted" );
			return;
		}
		/* DArray_Resize( self->parbuf, rout->parCount, 0 ); */
		if( ! DRoutine_FastPassParams( (DRoutine*)rout, selfpar, parbuf2, params, base, npar, vmc->code ) ){
			for(i=0; i<=rout->parCount; i++) DValue_Clear( parbuf2[i] );
			rout2 = (DRoutine*)rout;
			goto InvalidParameter;
		}
		/* foo: routine<x:int,s:string>
		 *   ns.foo( 1, "" );
		 * bar: routine<self:cdata,x:int>
		 *   obj.bar(1);
		 * inside: Dao class member method:
		 *   bar(1); # pass Dao class instances as self 
		 */
		if( vmc->code == DVM_MCALL_TC && ! (func->attribs & DAO_ROUT_PARSELF)) npar --;
		if( !(func->attribs & DAO_ROUT_ISCONST) ){
			for(i=0; i<npar; i++){
				if( parbuf2[i]->cst ){
					DValue_ClearAll( parbuf, rout->parCount+1 );
					DaoContext_RaiseException( self, DAO_ERROR, "calling non-const function on constant" );
					return;
				}
			}
		}
		/*
		   printf( "call: %s %i\n", rout->routName->mbs, npar );
		 */
		self->thisFunction = func;
		func->pFunc( self, parbuf2, npar );
		self->thisFunction = NULL;
		DValue_ClearAll( parbuf, rout->parCount+1 );
		if( DaoContext_CheckFE( self ) ) return;
		if( self->process->status==DAO_VMPROC_SUSPENDED )
			self->process->topFrame->entry = (short)(vmc - self->codes);
	}else if( caller.t==DAO_ROUTINE ){
		DaoRoutine *rout = caller.v.routine;
		DaoObject  *obj = NULL;
		int inclass = 0;
		/* rout itself could be a dummy routine */
		//printf( "rout = %p %s %s\n", rout, rout->routName->mbs, rout->routType->name->mbs );
		//rout = rout->routTable->items.pRout[0];
		//printf( "rout = %p %s %s\n", rout, rout->routName->mbs, rout->routType->name->mbs );
		ctx = DaoVmProcess_MakeContext( self->process, rout );
		if( rout->tidHost == DAO_OBJECT ){
			if( selfpar->t == DAO_OBJECT ){
				obj = (DaoObject*) DaoObject_MapThisObject( selfpar->v.object->that, rout->routHost );
				GC_ShiftRC( obj, ctx->object );
				ctx->object = obj;
				if( selfpar->v.object == self->object && obj ) inclass = 1;
#if 1
			}else if( params[0]->t == DAO_OBJECT ){ /* included in selfpar */
				obj = (DaoObject*) DaoObject_MapThisObject( params[0]->v.object, rout->routHost );
				GC_ShiftRC( obj, ctx->object );
				ctx->object = obj;
#endif
			}else if( rout->attribs & DAO_ROUT_NEEDSELF ){
				/* Only class method that does not use object data 
				 * will be allowed to be called without object
				 */
				DaoContext_RaiseException( self, DAO_ERROR, "need self object" );
				return;
			}
		}
		if( ctx == NULL ) return;
		ctx->parCount = npar;

		if( ! DRoutine_FastPassParams( (DRoutine*)ctx->routine, selfpar, ctx->regValues, params, base, npar, vmc->code ) ){
			rout2 = (DRoutine*)ctx->routine;
			goto InvalidParameter;
		}
		if( (inclass && self->constCall) || (rout->attribs & DAO_ROUT_ISCONST) 
				|| (selfpar->t == DAO_OBJECT && selfpar->cst ) )
			ctx->constCall = 1;
		while( vmc2->code == DVM_NOP ) vmc2 ++;
		if( vmc2->code == DVM_RETURN && vmc2->c ==0 ) tail = vmc2->b ==0 || (vmc2->b ==1 && vmc2->a == vmc->c);
		if( tail && !(self->frame->state & DVM_MAKE_OBJECT) ){
			DaoVmFrame *frame = self->frame;
			DaoVmProcess_PopContext( self->process );
			ctx->frame = frame;
			self->frame = frame->next;
			frame->context = ctx;
			frame->next->context = self;
			self = ctx;
		}
		DaoVmProcess_PushContext( self->process, ctx );

	}else if( caller.t == DAO_VMPROCESS && caller.v.vmp->abtype ){
		DaoVmProcess *vmProc = caller.v.vmp;
		if( vmProc->status == DAO_VMPROC_FINISHED ){
			DaoContext_RaiseException( self, DAO_WARNING, "coroutine execution is finished." );
			return;
		}
		DaoVmProcess_Resume2( vmProc, params, npar, self );
		if( vmProc->status == DAO_VMPROC_ABORTED )
			DaoContext_RaiseException( self, DAO_ERROR, "coroutine execution is aborted." );
	}else{
		DaoContext_RaiseException( self, DAO_ERROR_TYPE, "object not callable" );
	}
	return;
InvalidParameter:
	DaoContext_ShowCallError( self, rout2, selfpar, params, NULL, npar, vmc->code );
}
void DaoContext_DoReturn( DaoContext *self, DaoVmCode *vmc )
{
	short i;
	self->vmc = vmc;
	if( DaoContext_CheckFE( self ) ) return;
	if( self->caller && self->process->topFrame->returning != (ushort_t)-1 ){
		int regReturn = self->process->topFrame->returning;
		if( self->ctxState & DVM_MAKE_OBJECT ){
			DaoContext_SetData( self->caller, regReturn, (DaoBase*)self->object );
		}else if( vmc->b == 1 ){
			DaoContext_SetValue( self->caller, regReturn, *self->regValues[ vmc->a ] );
		}else if( vmc->b > 1 ){
			DaoTuple *tuple = DaoTuple_New( vmc->b );
			for( i=0; i<vmc->b; i++)
				DValue_Copy( tuple->items->data + i, *self->regValues[ vmc->a+i ] );
			DaoContext_SetData( self->caller, regReturn, (DaoBase*) tuple );
		}else if( ! ( self->process->topFrame->state & DVM_SPEC_RUN ) ){
			/* XXX DaoContext_SetValue( self->caller, regReturn, daoNullValue ); */
		}
	}else if( self->process->parYield ){
		DaoVmProcess_Yield( self->process, self->regValues + vmc->a, vmc->b, NULL );
		/* self->process->status is set to DAO_VMPROC_SUSPENDED by DaoVmProcess_Yield() */
		self->process->status = DAO_VMPROC_FINISHED;
	}else{
		if( vmc->b == 1 ){
			DValue_Move( *self->regValues[ vmc->a ], & self->process->returned, NULL );
		}else if( vmc->b > 1 ){
			DaoTuple *tuple = DaoTuple_New( vmc->b );
			DValue_Clear( & self->process->returned );
			GC_IncRC( tuple );
			self->process->returned.t = DAO_TUPLE;
			self->process->returned.v.p = (DaoBase*) tuple;
			for( i=0; i<vmc->b; i++)
				DValue_Copy( tuple->items->data + i, *self->regValues[ vmc->a+i ] );
		}
	}
}
int DaoRoutine_SetVmCodes2( DaoRoutine *self, DaoVmcArray *vmCodes );
DaoRoutine* DaoRoutine_Copy( DaoRoutine *self, int overload );
int DaoRoutine_InferTypes( DaoRoutine *self );
void DaoRoutine_CopyFields( DaoRoutine *self, DaoRoutine *other );

static void DaoContext_MapTypes( DaoContext *self, DMap *deftypes )
{
	DaoRoutine *routine = self->routine;
	DNode *it = DMap_First(routine->localVarType);
	for(; it; it = DMap_Next(routine->localVarType,it) ){
		DValue value = *self->regValues[ it->key.pInt ];
		if( value.t != DAO_TYPE || it->value.pType->tid != DAO_TYPE ) continue;
		MAP_Insert( deftypes, it->value.pType->nested->items.pType[0], value.v.p );
	}
}
void DaoRoutine_MapTypes( DaoRoutine *self, DMap *deftypes )
{
	DaoType *tp, *tp2;
	DNode *it;
	int i;
#if 0
	printf( "DaoRoutine_MapTypes() %s\n", self->routName->mbs );
	for(it=DMap_First(deftypes); it; it=DMap_Next(deftypes,it) ){
		printf( "%16p -> %p\n", it->key.pType, it->value.pType );
		printf( "%16s -> %s\n", it->key.pType->name->mbs, it->value.pType->name->mbs );
	}
#endif
	for(it=DMap_First(self->localVarType); it; it=DMap_Next(self->localVarType,it) ){
		tp = DaoType_DefineTypes( it->value.pType, self->nameSpace, deftypes );
		it->value.pType = tp;
	}
	for(i=0; i<self->regType->size; i++){
		tp = self->regType->items.pType[i];
		tp = DaoType_DefineTypes( tp, self->nameSpace, deftypes );
		GC_ShiftRC( tp, self->regType->items.pType[i] );
		self->regType->items.pType[i] = tp;
#if 0
		if( tp ) printf( "%3i: %s\n", i, tp->name->mbs );
#endif
	}
	for(i=0; i<self->routConsts->size; i++){
		DValue value = self->routConsts->data[i];
		if( value.t < DAO_ARRAY ) continue;
		tp = DaoNameSpace_GetTypeV( self->nameSpace, value );
		tp2 = DaoType_DefineTypes( tp, self->nameSpace, deftypes );
		if( tp == tp2 ) continue;
		value = daoNullValue;
		DValue_Move( self->routConsts->data[i], & value, tp2 );
		DValue_Clear( & self->routConsts->data[i] );
		self->routConsts->data[i] = value;
	}
}
void DaoRoutine_Finalize( DaoRoutine *self, DaoClass *klass, DMap *deftypes )
{
	DNode *it;
	DMap *old = deftypes;
	DaoType *tp = DaoType_DefineTypes( self->routType, self->nameSpace, deftypes );
	GC_ShiftRC( tp, self->routType );
	self->routType = tp;
	if( klass ){
		GC_ShiftRC( klass->objType, self->routHost );
		self->routHost = klass->objType;
	}
	if( self->minimal ==1 ) return;
	DaoRoutine_MapTypes( self, deftypes );
	DaoRoutine_InferTypes( self );
	/*
	DaoRoutine_PrintCode( self, self->nameSpace->vmSpace->stdStream );
	*/
}

void DaoContext_MakeRoutine( DaoContext *self, DaoVmCode *vmc )
{
	DMap *deftypes;
	DValue **pp = self->regValues + vmc->a;
	DValue *pp2;
	DaoType *tp;
	DaoRoutine *closure;
	DaoRoutine *proto = pp[0]->v.routine;
	int i;
	if( proto->vmCodes->size ==0 && proto->annotCodes->size ){
		if( DaoRoutine_SetVmCodes( proto, proto->annotCodes ) ==0 ){
			DaoContext_RaiseException( self, DAO_ERROR, "invalid closure" );
			return;
		}
	}

	closure = DaoRoutine_Copy( proto, 0 );
	if( proto->upRoutine ){
		closure->upRoutine = self->routine;
		closure->upContext = self;
		GC_IncRC( self );
		GC_IncRC( self->routine );
	}
	pp2 = closure->routConsts->data;
	for(i=0; i<vmc->b; i+=2) DValue_Copy( pp2 + pp[i+2]->v.i, *pp[i+1] );
	tp = DaoNameSpace_MakeRoutType( self->nameSpace, closure->routType, pp2, NULL, NULL );
	GC_ShiftRC( tp, closure->routType );
	closure->routType = tp;

	deftypes = DMap_New(0,0);
	DaoContext_MapTypes( self, deftypes );
	tp = DaoType_DefineTypes( closure->routType, closure->nameSpace, deftypes );
	GC_ShiftRC( tp, closure->routType );
	closure->routType = tp;
	DaoRoutine_MapTypes( closure, deftypes );
	DMap_Delete( deftypes );

	DArray_Assign( closure->annotCodes, proto->annotCodes );
	DaoRoutine_SetVmCodes2( closure, proto->vmCodes );
	DaoContext_SetData( self, vmc->c, (DaoBase*) closure );
	/*
	   DaoRoutine_PrintCode( proto, self->vmSpace->stdStream );
	   DaoRoutine_PrintCode( closure, self->vmSpace->stdStream );
	 printf( "%s\n", closure->routType->name->mbs );
	 */
}

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
void DaoContext_MakeClass( DaoContext *self, DaoVmCode *vmc )
{
	DaoType *tp;
	DaoRoutine *routine = self->routine;
	DaoNameSpace *ns = self->nameSpace;
	DaoTuple *tuple = self->regValues[vmc->a]->v.tuple;
	DaoClass *klass = DaoClass_New();
	DaoClass *proto = NULL;
	DaoList *parents = NULL;
	DaoMap *parents2 = NULL;
	DaoList *fields = NULL;
	DaoList *methods = NULL;
	DString *name = NULL;
	DVaTuple *items = tuple->items;
	DValue *data = tuple->items->data;
	DMap *keys = tuple->unitype->mapNames;
	DMap *deftypes = DMap_New(0,0);
	DMap *pm_map = DMap_New(D_STRING,0);
	DMap *st_map = DMap_New(D_STRING,0);
	DMap *protoValues = NULL;
	DValue *dest;
	DNode *it, *node;
	DEnum pmEnum = {NULL,0};
	DEnum stEnum = {NULL,0};
	int i, st, pm, up, id, size;
	char buf[50];

	pmEnum.type = dao_access_enum;
	stEnum.type = dao_storage_enum;

	DaoContext_SetData( self, vmc->c, (DaoBase*) klass );
	//printf( "%s\n", tuple->unitype->name->mbs );
	if( vmc->b && routine->routConsts->data[vmc->b-1].t == DAO_CLASS ){
		proto = routine->routConsts->data[vmc->b-1].v.klass;
		protoValues = proto->protoValues;
	}

	/* extract parameters */
	it = MAP_Find( keys, "name" );
	if( it && data[it->value.pInt].t == DAO_STRING ) name = data[it->value.pInt].v.s;
	it = MAP_Find( keys, "parents" );
	if( it && data[it->value.pInt].t == DAO_LIST ) parents = data[it->value.pInt].v.list;
	if( it && data[it->value.pInt].t == DAO_MAP ) parents2 = data[it->value.pInt].v.map;
	it = MAP_Find( keys, "fields" );
	if( it && data[it->value.pInt].t == DAO_LIST ) fields = data[it->value.pInt].v.list;
	it = MAP_Find( keys, "methods" );
	if( it && data[it->value.pInt].t == DAO_LIST ) methods = data[it->value.pInt].v.list;

	if( name ==NULL && items->size && data[0].t == DAO_STRING ) name = data[0].v.s;
	if( parents ==NULL && parents2 == NULL && items->size >1 ){
		if( data[1].t == DAO_LIST ) parents = data[1].v.list;
		if( data[1].t == DAO_MAP ) parents2 = data[1].v.map;
	}
	if( fields ==NULL && items->size >2 && data[2].t == DAO_LIST ) fields = data[2].v.list;
	if( methods ==NULL && items->size >3 && data[3].t == DAO_LIST ) methods = data[3].v.list;

	if( name == NULL || name->size ==0 ){
		sprintf( buf, "AnonymousClass%p", klass );
		DString_SetMBS( klass->className, buf );
		DaoClass_SetName( klass, klass->className );
	}else{
		DaoClass_SetName( klass, name );
	}
	tp = DaoNameSpace_MakeType( ns, "@class", DAO_INITYPE, NULL,NULL,0 );
	if( tp ) MAP_Insert( deftypes, tp, klass->objType );
	DaoContext_MapTypes( self, deftypes );

	/* copy data from the proto class */
	if( proto ) DaoClass_CopyField( klass, proto, deftypes );

	/* update class members with running time data */
	for(it=DMap_First(protoValues);it;it=DMap_Next(protoValues,it)){
		DValue value;
		node = DMap_Find( proto->lookupTable, it->value.pString );
		st = LOOKUP_ST( node->value.pSize );
		pm = LOOKUP_PM( node->value.pSize );
		up = LOOKUP_UP( node->value.pSize );
		id = LOOKUP_ID( node->value.pSize );
		if( up ) continue; /* should be never true */
		value = *self->regValues[it->key.pInt];
		if( st == DAO_CLASS_CONSTANT ){
			DaoRoutine *newRout = NULL;
			DValue *dest = klass->cstData->data + id;
			if( value.t == DAO_ROUTINE && value.v.routine->routHost == proto->objType ){
				newRout = value.v.routine;
				DaoRoutine_Finalize( newRout, klass, deftypes );
				if( strcmp( newRout->routName->mbs, "@class" ) ==0 ){
					node = DMap_Find( proto->lookupTable, newRout->routName );
					DString_Assign( newRout->routName, klass->className );
					st = LOOKUP_ST( node->value.pSize );
					up = LOOKUP_UP( node->value.pSize );
					if( st == DAO_CLASS_CONSTANT && up ==0 ){
						id = LOOKUP_ID( node->value.pSize );
						dest = klass->cstData->data + id;
					}
					DRoutine_AddOverLoad( (DRoutine*)klass->classRoutine, (DRoutine*)newRout );
				}
			}
			if( dest->t == DAO_ROUTINE ){
				DaoRoutine *rout = dest->v.routine;
				if( rout->routHost != klass->objType ) DValue_Clear( dest );
			}
			if( dest->t == DAO_ROUTINE ){
				DaoRoutine *rout = dest->v.routine;
				DRoutine_AddOverLoad( (DRoutine*)rout, (DRoutine*)newRout );
			}else{
				DValue_Move( value, klass->cstData->data+id, NULL );
			}
		}else if( st == DAO_CLASS_VARIABLE ){
			tp = klass->glbDataType->items.pType[id];
			DValue_Move( value, klass->glbData->data+id, tp );
		}else if( st == DAO_OBJECT_VARIABLE ){
			tp = klass->objDataType->items.pType[id];
			DValue_Move( value, klass->objDataDefault->data+id, tp );
		}
	}

	/* add parents from parameters */
	if( parents ){
		for(i=0; i<parents->items->size; i++){
			DValue item = parents->items->data[i];
			if( item.t == DAO_CLASS ){
				DaoClass_AddSuperClass( klass, item.v.p, item.v.klass->className );
			}
		}
	}else if( parents2 ){
		for(it=DMap_First(parents2->items);it;it=DMap_Next(parents2->items,it)){
			if( it->key.pValue->t == DAO_STRING && it->value.pValue->t == DAO_CLASS ){
				DaoClass_AddSuperClass( klass, it->value.pValue->v.p, it->key.pValue->v.s );
			}
		}
	}
	DaoClass_DeriveClassData( klass );
	if( fields ){ /* add fields from parameters */
		for(i=0; i<fields->items->size; i++){
			DaoType *type = NULL;
			DaoTuple *field = NULL;
			DMap *keys = NULL;
			DValue *data = NULL;
			DValue *name = NULL;
			DValue *value = NULL;
			DValue *storage = NULL;
			DValue *access = NULL;
			int flags = 0;

			if( fields->items->data[i].t != DAO_TUPLE ) continue;
			field = fields->items->data[i].v.tuple;
			keys = field->unitype->mapNames;
			data = field->items->data;
			size = field->items->size;
			st = DAO_OBJECT_VARIABLE;
			pm = DAO_DATA_PUBLIC;

			if( size < 2 || size > 4 ) goto InvalidField;

			id = (it = MAP_Find( keys, "name" )) ? it->value.pInt : -1;
			if( id >=0 && data[id].t == DAO_STRING ){
				flags |= 1<<id;
				name = data + id;
			}
			id = (it = MAP_Find( keys, "value" )) ? it->value.pInt : -1;
			if( id >=0 && data[id].t ){
				flags |= 1<<id;
				value = data + id;
				type = field->unitype->nested->items.pType[id];
			}
			id = (it = MAP_Find( keys, "storage" )) ? it->value.pInt : -1;
			if( id >=0 && data[id].t == DAO_ENUM ){
				flags |= 1<<id;
				storage = data + id;
			}
			id = (it = MAP_Find( keys, "access" )) ? it->value.pInt : -1;
			if( id >=0 && data[id].t == DAO_ENUM ){
				flags |= 1<<id;
				access = data + id;
			}

			if( name ==NULL && size && data[0].t == DAO_STRING && !(flags & 1) ){
				flags |= 1;
				name = data;
			}
			if( value ==NULL && size >1 && data[1].t && !(flags & 2) ){
				flags |= 2;
				value = & data[1];
				type = field->unitype->nested->items.pType[1];
			}
			if( storage ==NULL && size >2 && data[2].t == DAO_ENUM && !(flags&4) ){
				flags |= 4;
				storage = & data[2];
			}
			if( access ==NULL && size >3 && data[3].t == DAO_ENUM && !(flags&8) ){
				flags |= 8;
				access = & data[3];
			}

			if( flags != ((1<<size)-1) ) goto InvalidField;
			if( name == NULL || value == NULL ) continue;
			if( MAP_Find( klass->lookupTable, name->v.s ) ) continue;
			if( storage ){
				if( DEnum_SetValue( & stEnum, storage->v.e, NULL ) ==0) goto InvalidField;
				st = storages[ stEnum.value ];
			}
			if( access ){
				if( DEnum_SetValue( & pmEnum, access->v.e, NULL ) ==0) goto InvalidField;
				pm = permissions[ pmEnum.value ];
			}
			/* printf( "%s %i %i\n", name->v.s->mbs, st, pm ); */
			switch( st ){
			case DAO_OBJECT_VARIABLE :
				DaoClass_AddObjectVar( klass, name->v.s, *value, type, pm, 0 );
				break;
			case DAO_CLASS_VARIABLE :
				DaoClass_AddGlobalVar( klass, name->v.s, *value, type, pm, 0 );
				break;
			case DAO_CLASS_CONSTANT :
				DaoClass_AddConst( klass, name->v.s, *value, pm, 0 );
				break;
			default : break;
			}
			continue;
InvalidField:
			DaoContext_RaiseException( self, DAO_ERROR_PARAM, "" );
		}
	}
	if( methods ){ /* add methods from parameters */
		for(i=0; i<methods->items->size; i++){
			DaoTuple *tuple;
			DaoRoutine *newRout;
			DValue *data = NULL;
			DValue *name = NULL;
			DValue *method = NULL;
			DValue *access = NULL;
			DValue *dest;
			int flags = 0;

			if( methods->items->data[i].t != DAO_TUPLE ) continue;
			tuple = methods->items->data[i].v.tuple;
			data = tuple->items->data;
			size = tuple->items->size;
			pm = DAO_DATA_PUBLIC;

			id = (it = MAP_Find( keys, "name" )) ? it->value.pInt : -1;
			if( id >=0 && data[id].t == DAO_ENUM ){
				flags |= 1<<id;
				name = data + id;
			}
			id = (it = MAP_Find( keys, "method" )) ? it->value.pInt : -1;
			if( id >=0 && data[id].t == DAO_ENUM ){
				flags |= 1<<id;
				method = data + id;
			}
			id = (it = MAP_Find( keys, "access" )) ? it->value.pInt : -1;
			if( id >=0 && data[id].t == DAO_ENUM ){
				flags |= 1<<id;
				access = data + id;
			}

			if( name ==NULL && size && data[0].t == DAO_STRING && !(flags&1) ){
				flags |= 1;
				name = data;
			}
			if( method ==NULL && size >1 && data[1].t == DAO_ROUTINE && !(flags&2) ){
				flags |= 2;
				method = & data[1];
			}
			if( access ==NULL && size >2 && data[2].t == DAO_ENUM && !(flags&4) ){
				flags |= 4;
				access = & data[2];
			}

			if( flags != ((1<<size)-1) ) goto InvalidMethod;
			if( name == NULL || method == NULL ) continue;
			if( access ){
				if( DEnum_SetValue( & pmEnum, access->v.e, NULL ) ==0) goto InvalidMethod;
				pm = permissions[ pmEnum.value ];
			}

			newRout = method->v.routine;
			if( newRout->tidHost !=0 ) continue;
			DaoRoutine_Finalize( newRout, klass, deftypes );
			DString_Assign( newRout->routName, name->v.s );
			if( DString_EQ( newRout->routName, klass->className ) ){
				DRoutine_AddOverLoad( (DRoutine*)klass->classRoutine, (DRoutine*)newRout );
			}

			node = DMap_Find( proto->lookupTable, name->v.s );
			if( node == NULL ){
				DaoClass_AddConst( klass, name->v.s, *method, pm, 0 );
				continue;
			}
			if( LOOKUP_UP( node->value.pSize ) ) continue;
			if( LOOKUP_ST( node->value.pSize ) != DAO_CLASS_CONSTANT ) continue;
			id = LOOKUP_ID( node->value.pSize );
			dest = klass->cstData->data + id;
			if( dest->t == DAO_ROUTINE ){
				DaoRoutine *rout = dest->v.routine;
				DRoutine_AddOverLoad( (DRoutine*)rout, (DRoutine*)newRout );
			}
			continue;
InvalidMethod:
			DaoContext_RaiseException( self, DAO_ERROR_PARAM, "" );
		}
	}
	DaoClass_DeriveObjectData( klass );
	DaoClass_ResetAttributes( klass );
	DMap_Delete( deftypes );
	DMap_Delete( pm_map );
	DMap_Delete( st_map );
}
int DaoContext_DoCheckExcept( DaoContext *self, DaoVmCode *vmc )
{
	DaoList *list = self->nameSpace->varData->data[DVR_NSV_EXCEPTIONS].v.list;
	DaoList_Clear( list );
	self->vmc = vmc;
	if( DaoContext_CheckFE( self ) ) return 1;
	return ( self->process->exceptions->size > 0 );
}
static void DaoInitException
( DaoCData *except, DaoContext *ctx, DaoVmCode *vmc, int fe, const char *value )
{
	DaoRoutine *rout = ctx->routine;
	DaoTypeBase *efloat = DaoException_GetType( DAO_ERROR_FLOAT );
	DaoException *exdat = (DaoException*) except->data;
	DaoVmCodeX **annotCodes = rout->annotCodes->items.pVmc;
	DaoVmFrame *frame = ctx->frame->prev;
	DaoContext *ctx2;
	DValue *objData;
	int k, line, line2;
	int id = (int) (vmc - ctx->codes);

	line = line2 = rout->defLine;
	if( vmc && rout->vmCodes->size ) line = annotCodes[id]->line;
	line2 = line;
	exdat->routine = (DRoutine*) rout;
	exdat->toLine = line;
	if( DaoCData_ChildOf( except->typer, efloat ) && fe >=0 )
		line2 = (vmc && rout->vmCodes->size) ? annotCodes[ fe ]->line : rout->defLine;
	exdat->fromLine = line2;
	if( value && value[0] != 0 ){
		DValue_Clear( & exdat->data );
		exdat->data = DValue_NewMBString( value, 0 );
	}
	DArray_Clear( exdat->callers );
	DArray_Clear( exdat->lines );
	if( ctx->thisFunction ){
		DArray_Append( exdat->callers, rout );
		DArray_Append( exdat->lines, (size_t)line );
		exdat->routine = (DRoutine*) ctx->thisFunction;
	}
	while( frame && frame->context ){
		if( exdat->callers->size >= 5 ) break;
		line = frame->context->routine->annotCodes->items.pVmc[ frame->entry ]->line;
		DArray_Append( exdat->callers, frame->context->routine );
		DArray_Append( exdat->lines, (size_t) line );
		frame = frame->prev;
	}
}
extern void STD_Debug( DaoContext *ctx, DaoBase *p[], int N );
void DaoContext_DoRaiseExcept( DaoContext *self, DaoVmCode *vmc )
{
	DaoStream *stdio = self->vmSpace->stdStream;
	DaoCData *cdata = NULL;
	DaoTypeBase *except = & dao_Exception_Typer;
	DaoTypeBase *warning = DaoException_GetType( DAO_WARNING );
	DaoList *list = self->nameSpace->varData->data[DVR_NSV_EXCEPTIONS].v.list;
	DValue *excepts = self->regArray->data + vmc->a;
	DValue val;
	ushort_t i, line = 0, line2 = 0;
	ushort_t N = vmc->b -1;
	line2 = line;
	if( N == 0 && list->items->size >0 ){
		N = list->items->size;
		excepts = list->items->data;
	}
	for(i=0; i<N; i++){
		val = excepts[i];
		if( val.t == DAO_OBJECT || val.t == DAO_CDATA ){
			cdata = NULL;
			if( val.t == DAO_OBJECT ){
				DaoType *type = except->priv->abtype;
				cdata = (DaoCData*) DaoObject_MapThisObject( val.v.object, type );
			}else{
				if( DaoCData_ChildOf( val.v.cdata->typer, except ) )
					cdata = val.v.cdata;
			}
			if( cdata == NULL || cdata->data == NULL ) goto InvalidException;
			DaoInitException( cdata, self, vmc, self->idClearFE, NULL );
			if( DaoCData_ChildOf( cdata->typer, warning ) ){
				DaoPrintException( cdata, stdio );
			}else{
				DVarray_Append( self->process->exceptions, val );
			}
		}else{
			goto InvalidException;
		}
		continue;
InvalidException:
		DaoContext_RaiseException( self, DAO_ERROR, "invalid exception object" );
		break;
	}
	DaoList_Clear( list );
	if( self->vmSpace->options & DAO_EXEC_DEBUG ){
		DaoVmProcess_PrintException( self->process, 0 );
		STD_Debug( self, NULL, 0 );
	}
}
int DaoContext_DoRescueExcept( DaoContext *self, DaoVmCode *vmc )
{
	DaoList *list = self->nameSpace->varData->data[DVR_NSV_EXCEPTIONS].v.list;
	DaoTypeBase *ext = & dao_Exception_Typer;
	DaoTypeBase *any = DaoException_GetType( DAO_EXCEPTION_ANY );
	DaoTypeBase *none = DaoException_GetType( DAO_EXCEPTION_NONE );
	DaoClass *p, *q;
	DaoCData *cdata;
	DValue **excepts = self->regValues + vmc->a;
	DValue val, val2;
	ushort_t i, j;
	ushort_t N = vmc->b -1;
	int canRescue = 0;
	int M = self->process->exceptions->size;
	DaoList_Clear( list );
	self->vmc = vmc;
	if( DaoContext_CheckFE( self ) ) M = self->process->exceptions->size;
	if( N ==0 && M >0 ){ /* rescue without exception list */
		DVarray_Swap( self->process->exceptions, list->items );
		return 1;
	}
	for(i=0; i<N; i++){
		val = *excepts[i];
		if( val.t == DAO_OBJECT || val.t == DAO_CDATA ){
			cdata = val.v.cdata;
			if( val.t == DAO_OBJECT ){
				DaoType *type = ext->priv->abtype;
				cdata = (DaoCData*) DaoObject_MapThisObject( val.v.object, type );
			}
			if( cdata && DaoCData_ChildOf( cdata->typer, any ) ){
				DVarray_Swap( self->process->exceptions, list->items );
				return 1;
			}else if( cdata && DaoCData_ChildOf( cdata->typer, none ) && M ==0 ){
				return 1;
			}else if( cdata ){
				for(j=0; j<self->process->exceptions->size; j++){
					val2 = self->process->exceptions->data[j];
					if( val2.t == DAO_OBJECT || val2.t == DAO_CDATA ){
						DaoCData *cdata2 = val2.v.cdata;
						if( val2.t == DAO_OBJECT ){
							DaoType *type = ext->priv->abtype;
							cdata2 = (DaoCData*) DaoObject_MapThisObject( val2.v.object, type );
						}
						if( cdata2 == NULL ){
							/* XXX */
							continue;
						}
						if( DaoCData_ChildOf( cdata2->typer, cdata->typer ) ){
							canRescue = 1;
							DVarray_Append( list->items, val2 );
							DVarray_Erase( self->process->exceptions, j, 1 );
							continue;
						}
					}
				}
			}else{
			}
		}
	}
	return canRescue;
}
void DaoContext_RaiseException( DaoContext *self, int type, const char *value )
{
	DaoTypeBase *typer;
	DaoTypeBase *warning = DaoException_GetType( DAO_WARNING );
	DaoStream *stdio = self->vmSpace->stdStream;
	DaoCData *cdata;
	DValue val = daoNullCData;
	if( type <= 1 ) return;
	if( type >= ENDOF_BASIC_EXCEPT ) type = DAO_ERROR;

	typer = DaoException_GetType( type );
	if( DaoCData_ChildOf( typer, warning ) ){
		/* XXX support warning suppression */
		cdata = DaoCData_New( typer, DaoException_New( typer ) );
		DaoInitException( cdata, self, self->vmc, self->idClearFE, value );
		DaoPrintException( cdata, stdio ); 
		typer->Delete( cdata );
		return;
	}
	cdata = DaoCData_New( typer, DaoException_New( typer ) );
	val.v.cdata = cdata;
	DaoInitException( cdata, self, self->vmc, self->idClearFE, value );
	DVarray_Append( self->process->exceptions, val );
	if( (self->vmSpace->options & DAO_EXEC_DEBUG) ){
		if( self->process->stopit ==0 && self->vmSpace->stopit ==0 ){
			DaoVmProcess_Trace( self->process, 10 );
			DaoVmProcess_PrintException( self->process, 0 );
			STD_Debug( self, NULL, 0 );
		}
	}
}
void DaoContext_RaiseTypeError( DaoContext *self, DaoType *from, DaoType *to, const char *op )
{
	DString *details = DString_New(1);
	DString_AppendMBS( details, op );
	DString_AppendMBS( details, " from \'" );
	DString_Append( details,  from->name );
	DString_AppendMBS( details, "\' to \'" );
	DString_Append( details,  to->name );
	DString_AppendMBS( details, "\'." );
	DaoContext_RaiseException( self, DAO_ERROR_TYPE, details->mbs );
	DString_Delete( details );
}
