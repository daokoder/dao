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
#include"daoValue.h"

#ifndef FE_ALL_EXCEPT
#define FE_ALL_EXCEPT 0xffff
#endif

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

void DaoArray_number_op_array( DaoArray *C, DaoValue *A, DaoArray *B, short op, DaoContext *ctx );
void DaoArray_array_op_number( DaoArray *C, DaoArray *A, DaoValue *B, short op, DaoContext *ctx );
void DaoArray_ArrayArith( DaoArray *s, DaoArray *l, DaoArray *r, short p, DaoContext *c );

extern void DaoVmProcess_Trace( DaoVmProcess *self, int depth );
int DaoVmProcess_Resume2( DaoVmProcess *self, DaoValue *par[], int N, DaoContext *ret );
void DaoPrintException( DaoCData *except, DaoStream *stream );


DaoTypeBase ctxTyper =
{
	"context", & baseCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoContext_Delete, NULL
};

DaoContext* DaoContext_New()
{
	DaoContext *self = (DaoContext*) dao_malloc( sizeof( DaoContext ) );
	DaoValue_Init( self, DAO_CONTEXT );

	self->codes = NULL;
	self->vmc = NULL;
	self->frame = NULL;
	self->regArray = DTuple_New( 0, NULL );
	self->regValues = NULL;

	self->routine   = NULL;
	self->object    = NULL;
	self->nameSpace = NULL;
	self->vmSpace   = NULL;
	self->process    = NULL;
	self->lastRoutine = NULL;
	self->thisFunction = NULL;

	self->parCount = 0;
	self->entryCode = 0;
	self->ctxState = 0;
	return self;
}

void DaoContext_Delete( DaoContext *self )
{
	int i;
	if( self->object ) GC_DecRC( self->object );
	if( self->routine ) GC_DecRC( self->routine );
	for(i=0; i<self->regArray->size; i++) GC_DecRC( self->regValues[i] );
	DTuple_Delete( self->regArray );
	dao_free( self );
}
static void DaoContext_InitValues( DaoContext *self )
{
	complex16 com;
	DaoType **types;
	DaoValue **values;
	int i, t, N = self->routine->regCount;
	self->entryCode = 0;
	self->ctxState = 0;
	self->vmc = NULL;
	if( self->lastRoutine == self->routine ) return;
	self->codes = self->routine->vmCodes->codes;
	self->regTypes = self->routine->regType->items.pType;
	if( self->regArray->size < N ) DTuple_Resize( self->regArray, N, NULL );
	self->regValues = self->regArray->items.pValue;
	if( self->routine->regType->size ==0 ) return; /* DaoTaskThread_New() */
	types = self->regTypes;
	values = self->regValues;
	for(i=0; i<N; i++){
		DaoValue *value = values[i];
		DaoType *type = types[i];
		if( type == NULL ) continue;
		if( value ){
			if( value->type == type->tid && value->xGC.refCount == 1 ) continue;
			GC_DecRC( value );
			value = values[i] = NULL;
		}
		switch( type->tid ){
		case DAO_INTEGER : value = (DaoValue*) DaoInteger_New(0); break;
		case DAO_FLOAT   : value = (DaoValue*) DaoFloat_New(0.0); break;
		case DAO_DOUBLE  : value = (DaoValue*) DaoDouble_New(0.0); break;
		case DAO_COMPLEX : value = (DaoValue*) DaoComplex_New(com); break;
		}
		if( value == NULL ) continue;
		value->xGC.refCount = 1;
		values[i] = value;
	}
	self->lastRoutine = self->routine;
}
void DaoContext_Init( DaoContext *self, DaoRoutine *routine )
{
	/* dummy function, assume DaoContext_InitWithParams() will be called next */
	if( routine->type == DAO_ROUTINE ){
		while( routine->revised ) routine = routine->revised;
		DaoRoutine_Compile( routine );
	}
	GC_ShiftRC( routine, self->routine );
	self->routine   = routine;
	self->nameSpace = routine->nameSpace;

	/* routine could be DaoFunction pushed in at DaoParser_ParseParams() */
	if( routine->type != DAO_ROUTINE ) return;
	DaoContext_InitValues( self );
}
int DaoContext_InitWithParams( DaoContext *self, DaoVmProcess *vmp, DaoValue *pars[], int npar )
{
	DaoObject *othis = self->object;
	DaoRoutine *rout = self->routine;
	if( ! self->routine ) return 0;
	rout = (DaoRoutine*) DRoutine_Resolve( (DaoValue*)rout, (DaoValue*)othis, pars, npar, 0 );
	if( rout == NULL || rout->type != DAO_ROUTINE ){
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

int DaoMoveAC( DaoContext *self, DaoValue *A, DaoValue **C, DaoType *t )
{
	if( ! DaoValue_Move( A, C, t ) ){
		DaoType *type;
		if( self->vmc->code == DVM_MOVE || self->vmc->code == DVM_MOVE_PP ){
			if( A->type == DAO_CDATA && t && t->tid == DAO_CDATA ){
				if( DaoType_MatchTo( A->xCdata.ctype, t, NULL ) ){
					DaoValue_Copy( A, C );
					return 1;
				}
			}
		}
		type = DaoNameSpace_GetType( self->nameSpace, A );
		DaoContext_RaiseTypeError( self, type, t, "moving" );
		return 0;
	}
	return 1;
}

DaoValue* DaoContext_SetValue( DaoContext *self, ushort_t reg, DaoValue *value )
{
	DaoType *tp = self->regTypes[reg];
	int res = DaoValue_Move( value, self->regValues + reg, tp );
	if( res ) return self->regValues[ reg ];
	return NULL;
}
DaoValue* DaoContext_PutValue( DaoContext *self, DaoValue *value )
{
	return DaoContext_SetValue( self, self->vmc->c, value );
}
int DaoContext_PutReference( DaoContext *self, DaoValue *refer )
{
	int tm, reg = self->vmc->c;
	DaoValue **value = & self->regValues[reg];
	DaoType *tp2, *tp = self->regTypes[reg];

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
	tp2 = DaoNameSpace_GetType( self->nameSpace, refer );
	DaoContext_RaiseTypeError( self, tp2, tp, "referencing" );
	return 0;
}
DaoVmProcess* DaoContext_CurrentProcess( DaoContext *self )
{
	return self->process;
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
dint* DaoContext_PutInteger( DaoContext *self, dint value )
{
	DaoInteger tmp = {DAO_INTEGER,0,1,0,{0,0},0,0,0};
	DaoValue *res = DaoContext_SetValue( self, self->vmc->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	res->xInteger.value = value;
	return & res->xInteger.value;
}
float* DaoContext_PutFloat( DaoContext *self, float value )
{
	DaoFloat tmp = {DAO_FLOAT,0,1,0,{0,0},0,0,0.0};
	DaoValue *res = DaoContext_SetValue( self, self->vmc->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	res->xFloat.value = value;
	return & res->xFloat.value;
}
double* DaoContext_PutDouble( DaoContext *self, double value )
{
	DaoDouble tmp = {DAO_DOUBLE,0,1,0,{0,0},0,0,0.0};
	DaoValue *res = DaoContext_SetValue( self, self->vmc->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	res->xDouble.value = value;
	return & res->xDouble.value;
}
complex16* DaoContext_PutComplex( DaoContext *self, complex16 value )
{
	DaoComplex tmp = {DAO_COMPLEX,0,1,0,{0,0},0,0,{0.0,0.0}};
	DaoValue *res;
	tmp.value = value;
	res = DaoContext_SetValue( self, self->vmc->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	return & res->xComplex.value;
}
DString* DaoContext_PutMBString( DaoContext *self, const char *mbs )
{
	DString str = DString_WrapMBS( mbs );
	DaoString tmp = {DAO_STRING,0,1,0,{0,0},0,0,NULL};
	DaoValue *res;
	tmp.data = & str;
	res = DaoContext_SetValue( self, self->vmc->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	return res->xString.data;
}
DString* DaoContext_PutWCString( DaoContext *self, const wchar_t *wcs )
{
	DString str = DString_WrapWCS( wcs );
	DaoString tmp = {DAO_STRING,0,1,0,{0,0},0,0,NULL};
	DaoValue *res;
	tmp.data = & str;
	res = DaoContext_SetValue( self, self->vmc->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	return res->xString.data;
}
DString*   DaoContext_PutString( DaoContext *self, DString *str )
{
	DaoString tmp = {DAO_STRING,0,1,0,{0,0},0,0,NULL};
	DaoValue *res;
	tmp.data = str;
	res = DaoContext_SetValue( self, self->vmc->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	return res->xString.data;
}
DString* DaoContext_PutBytes( DaoContext *self, const char *bytes, int N )
{
	DString str = DString_WrapBytes( bytes, N );
	DaoString tmp = {DAO_STRING,0,1,0,{0,0},0,0,NULL};
	DaoValue *res;
	tmp.data = & str;
	res = DaoContext_SetValue( self, self->vmc->c, (DaoValue*) & tmp );
	if( res ==NULL ) return NULL;
	return res->xString.data;
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
static DaoArray* NullArray( DaoContext *self )
{
	DaoContext_RaiseException( self, DAO_ERROR, "numeric array is disabled" );
	return NULL;
}
DaoArray* DaoContext_PutArrayInteger( DaoContext *s, int*, int ){ return NullArray( s ); }
DaoArray* DaoContext_PutArrayShort( DaoContext *s, int*, int ){ return NullArray( s ); }
DaoArray* DaoContext_PutArrayFloat( DaoContext *s, int*, int ){ return NullArray( s ); }
DaoArray* DaoContext_PutArrayDouble( DaoContext *s, int*, int ){ return NullArray( s ); }
DaoArray* DaoContext_PutArrayComplex( DaoContext *s, int*, int ){ return NullArray( s ); }
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
	DaoContext_SetValue( self, self->vmc->c, (DaoValue*) stream );
	return stream;
}
DaoCData* DaoContext_PutCData( DaoContext *self, void *data, DaoTypeBase *plgTyper )
{
	DaoCData *cdata = DaoCData_New( plgTyper, data );
	DaoContext_SetValue( self, self->vmc->c, (DaoValue*)cdata );
	return cdata;
}
DaoCData* DaoContext_WrapCData( DaoContext *self, void *data, DaoTypeBase *plgTyper )
{
	DaoCData *cdata = DaoCData_Wrap( plgTyper, data );
	DaoContext_SetValue( self, self->vmc->c, (DaoValue*)cdata );
	return cdata;
}
DaoCData* DaoContext_PutCPointer( DaoContext *self, void *data, int size )
{
	DaoCData *cptr = DaoCData_New( NULL, NULL );
	cptr->data = data;
	cptr->size = cptr->bufsize = size;
	DaoContext_PutValue( self, (DaoValue*)cptr );
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

/**/
DaoVmCode* DaoContext_DoSwitch( DaoContext *self, DaoVmCode *vmc )
{
	DaoVmCode *mid;
	DaoValue **cst = self->routine->routConsts->items.pValue;
	DaoValue *opa = self->regValues[ vmc->a ];
	int first, last, cmp, id;
	dint min, max;

	if( vmc->c ==0 ) return self->codes + vmc->b;
	if( vmc[1].c == DAO_CASE_TABLE ){
		if( opa->type == DAO_INTEGER ){
			min = cst[ vmc[1].a ]->xInteger.value;
			max = cst[ vmc[vmc->c].a ]->xInteger.value;
			if( opa->xInteger.value >= min && opa->xInteger.value <= max )
				return self->codes + vmc[ opa->xInteger.value - min + 1 ].b;
		}else if( opa->type== DAO_ENUM ){
			min = cst[ vmc[1].a ]->xEnum.value;
			max = cst[ vmc[vmc->c].a ]->xEnum.value;
			if( opa->xEnum.value >= min && opa->xEnum.value <= max )
				return self->codes + vmc[ opa->xEnum.value - min + 1 ].b;
		}
		return self->codes + vmc->b;
	}else if( vmc[1].c == DAO_CASE_UNORDERED ){
		for(id=1; id<=vmc->c; id++){
			mid = vmc + id;
			if( DaoValue_Compare( opa, cst[ mid->a ] ) ==0 ){
				return self->codes + mid->b;
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
			if( cst[mid->a]->type== DAO_TUPLE && cst[mid->a]->xTuple.pair ){
				while( id > first && DaoValue_Compare( opa, cst[ vmc[id-1].a ] ) ==0 ) id --;
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
int DaoObject_InvokeMethod( DaoObject *self, DaoObject *othis,
		DaoVmProcess *vmp, DString *name, DaoContext *ctx, DaoValue *par[], int N, int ret );
void DaoContext_DoIter( DaoContext *self, DaoVmCode *vmc )
{
	DString *name = self->process->mbstring;
	DaoValue *va = self->regValues[ vmc->a ];
	DaoValue *vc = self->regValues[ vmc->c ];
	DaoTypeBase *typer = DaoValue_GetTyper( va );
	DaoFunction *func = NULL;
	DaoTuple *iter;
	int rc = 0;

	if( va == NULL || va->type == 0 ) return;

	if( vc == NULL || vc->type != DAO_TUPLE || vc->xTuple.unitype != dao_type_for_iterator ){
		vc = (DaoValue*) DaoContext_PutTuple( self );
	}

	iter = & vc->xTuple;
	iter->items->items.pValue[0]->xInteger.value = 0;

	DString_SetMBS( name, "__for_iterator__" );
	if( va->type == DAO_OBJECT ){
		rc = DaoObject_InvokeMethod( & va->xObject, NULL, self->process, name, self, & vc, 1, vmc->c );
	}else{
		DaoValue *meth = DaoFindFunction( typer, name );
		if( meth ) func = (DaoFunction*)DRoutine_Resolve( meth, va, &vc, 1, DVM_CALL );
		if( func ){
			DaoFunction_Call( func, self, va, &vc, 1 );
		}else{
			rc = 1;
		}
	}
	if( rc ) DaoContext_RaiseException( self, DAO_ERROR_FIELD_NOTEXIST, name->mbs );
}

DLong* DaoContext_GetLong( DaoContext *self, DaoVmCode *vmc )
{
	DaoType *tp = self->regTypes[ vmc->c ];
	DaoValue *dC = self->regValues[ vmc->c ];
	if( dC && dC->type == DAO_LONG ) return dC->xLong.value;
	if( tp && tp->tid !=DAO_LONG && tp->tid !=DAO_UDF && tp->tid !=DAO_ANY ) return NULL;
	dC = (DaoValue*) DaoLong_New();
	GC_ShiftRC( dC, self->regValues[ vmc->c ] );
	self->regValues[ vmc->c ] = dC;
	return dC->xLong.value;
}
DLong* DaoContext_PutLong( DaoContext *self )
{
	return DaoContext_GetLong( self, self->vmc );
}
DaoEnum* DaoContext_GetEnum( DaoContext *self, DaoVmCode *vmc )
{
	DaoType *tp = self->regTypes[ vmc->c ];
	DaoValue *dC = self->regValues[ vmc->c ];
	if( dC && dC->type == DAO_ENUM ){
		if( tp != dC->xEnum.etype ) DaoEnum_SetType( & dC->xEnum, tp );
		return & dC->xEnum;
	}
	if( tp && tp->tid !=DAO_ENUM && tp->tid !=DAO_UDF && tp->tid !=DAO_ANY ) return NULL;
	dC = (DaoValue*) DaoEnum_New( tp, 0 );
	GC_ShiftRC( dC, self->regValues[ vmc->c ] );
	self->regValues[ vmc->c ] = dC;
	return & dC->xEnum;
}
DaoEnum* DaoContext_PutEnum( DaoContext *self, const char *symbols )
{
	DaoEnum *denum = DaoContext_GetEnum( self, self->vmc );
	DaoEnum_SetSymbols( denum, symbols );
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
	DaoValue_Move( (DaoValue*) list, self->regValues + vmc->c, tp );
	return list;
}
DaoMap* DaoContext_GetMap( DaoContext *self,  DaoVmCode *vmc )
{
	DaoMap *map = DaoMap_New( vmc->code == DVM_HASH );
	DaoType *tp = self->regTypes[ vmc->c ];
	DaoValue_Move( (DaoValue*) map, self->regValues + vmc->c, tp );
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
	DaoValue *dC = self->regValues[ vmc->c ];
	int type = DAO_FLOAT;
	if( tp && tp->tid == DAO_ARRAY && tp->nested->size ){
		type = tp->nested->items.pType[0]->tid;
		if( type == 0 || type > DAO_COMPLEX ) type = DAO_FLOAT;
	}
	if( dC && dC->type == DAO_ARRAY && dC->xArray.refCount == 1 ){
		if( dC->xArray.numType < type ) DaoArray_ResizeVector( & dC->xArray, 0 );
		dC->xArray.numType = type;
	}else{
		dC = (DaoValue*) DaoArray_New( type );
		DaoValue_Copy( dC, & self->regValues[ vmc->c ] );
	}
	if( tp == NULL || tp->tid !=DAO_ARRAY || NESTYPE( tp, 0 )->tid == DAO_UDF )
		tp = dao_array_any;
	dC->xArray.unitype = tp;
	GC_IncRC( tp );
	return & dC->xArray;
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
	DaoValue **regValues = self->regValues;
	const ushort_t opA = vmc->a;
	int i;

	if( vmc->b == 0 || vmc->b >= 10 ){
		const int bval = vmc->b ? vmc->b - 10 : 0;
		DaoList *list = DaoContext_GetList( self, vmc );
		DArray_Resize( list->items, bval, NULL );
		if( bval >0 && self->regTypes[ vmc->c ] ==NULL ){
			DaoType *abtp = DaoNameSpace_GetType( ns, regValues[opA] );
			DaoType *t = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, & abtp, 1 );
			GC_ShiftRC( t, list->unitype );
			list->unitype = t;
		}
		for( i=0; i<bval; i++){
			if( DaoList_SetItem( list, regValues[opA+i], i ) ){
				DaoContext_RaiseException( self, DAO_ERROR_VALUE, "invalid items" );
				return;
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
	const ushort_t count = vmc->b - 10;
	size_t i, j, m, k = 0;
	DaoArray *array = NULL;
	DArray *dim = NULL;

	if( vmc->b < 10 ){
		DaoContext_DoNumRange( self, vmc );
		return;
	}
	array = DaoContext_GetArray( self, vmc );
	if( count && (array->unitype == NULL || array->unitype == dao_array_any) ){
		DaoNameSpace *ns = self->nameSpace;
		DaoValue *p = self->regValues[opA];
		DaoType *it = DaoNameSpace_GetType( ns, p );
		DaoType *type = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY, NULL, & it, 1 );
		switch( p->type ){
		case DAO_INTEGER :
		case DAO_FLOAT :
		case DAO_DOUBLE :
		case DAO_COMPLEX : array->numType = p->type; break;
		case DAO_ARRAY : array->numType = p->xArray.numType; break;
		default : DaoContext_RaiseException( self, DAO_ERROR_VALUE, "invalid items" ); return;
		}
		GC_ShiftRC( type, array->unitype ) ;
		array->unitype = type;
	}
	for( j=0; j<count; j++){
		DaoValue *p = self->regValues[ opA + j ];
		if( p == NULL || p->type != DAO_ARRAY ) continue;
		if( dim == NULL ) dim = p->xArray.dims;
		if( dim == p->xArray.dims ) continue;
		if( dim->size != p->xArray.dims->size ){
			DaoContext_RaiseException( self, DAO_ERROR_VALUE, "invalid items" );
			return;
		}
		for(m=0; m<dim->size; m++){
			if( dim->items.pSize[m] != p->xArray.dims->items.pSize[m] ){
				DArray_Delete( dim );
				dim = NULL;
				break;
			}
		}
	}
	if( dim ){
		dim = DArray_Copy( dim );
		DArray_PushFront( dim, (void*) (size_t)count );
		DaoArray_ResizeArray( array, dim->items.pSize, dim->size );
		DArray_Delete( dim );
	}else{
		DaoArray_ResizeVector( array, count );
	}
	k = 0;
	if( array->numType == DAO_INTEGER ){
		int *vals = array->data.i;
		for( j=0; j<count; j++ ){
			DaoValue *p = self->regValues[ opA + j ];
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
	}else if( array->numType == DAO_FLOAT ){
		float *vals = array->data.f;
		for( j=0; j<count; j++ ){
			DaoValue *p = self->regValues[ opA + j ];
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
	}else if( array->numType == DAO_DOUBLE ){
		double *vals = array->data.d;
		for( j=0; j<count; j++ ){
			DaoValue *p = self->regValues[ opA + j ];
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
			DaoValue *p = self->regValues[ opA + j ];
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
	self->vmc = vmc;
	DaoContext_RaiseException( self, DAO_ERROR, "numeric array is disabled" );
#endif
}
void DaoContext_DoRange(  DaoContext *self, DaoVmCode *vmc )
{
	const ushort_t opA = vmc->a;
	const ushort_t bval = vmc->b;

	DaoList *list = DaoContext_GetList( self, vmc );
	DaoValue **items, **regValues = self->regValues;
	DaoValue *dn = bval==3 ? regValues[opA+2] : regValues[opA+1];
	int num = (int)DaoValue_GetDouble( dn );
	int ta = regValues[ opA ]->type;
	int i;

	self->vmc = vmc;
	if( dn->type < DAO_INTEGER || dn->type > DAO_DOUBLE ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "need number" );
		return;
	}
	if( ta < DAO_INTEGER || ta >= DAO_ENUM ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "need a number or string as first value" );
		return;
	}
	if( ( self->vmSpace->options & DAO_EXEC_SAFE ) && num > 1000 ){
		DaoContext_RaiseException( self, DAO_ERROR, "not permitted" );
		return;
	}
	DArray_Resize( list->items, num, NULL );
	items = list->items->items.pValue;
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

	switch( ta ){
	case DAO_INTEGER :
		{
			const int first = regValues[ vmc->a ]->xInteger.value;
			const int step = bval==2 ? 1: DaoValue_GetDouble( regValues[ opA+1 ] );
			for( i=0; i<num; i++) items[i]->xInteger.value = first + i * step;
			break;
		}
	case DAO_FLOAT :
		{
			const float first = regValues[ vmc->a ]->xFloat.value;
			const float step = bval==2 ? 1: DaoValue_GetDouble( regValues[ opA+1 ] );
			for( i=0; i<num; i++) items[i]->xFloat.value = first + i * step;
			break;
		}
	case DAO_DOUBLE :
		{
			const double first = regValues[ vmc->a ]->xFloat.value;
			const double step = bval==2 ? 1: DaoValue_GetDouble( regValues[ opA+1 ] );
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
	if( self->regTypes[ vmc->c ] == NULL ){
		DaoNameSpace *ns = self->nameSpace;
		DaoType *et = DaoNameSpace_GetType( ns, regValues[opA] );
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
	DaoValue **regValues = self->regValues;
	DaoValue *dn = bval==3 ? regValues[opA+2] : regValues[opA+1];
	const size_t num = DaoValue_GetInteger( dn );
	char type = regValues[ opA ]->type;
	size_t i, j;
	DaoArray *array = NULL;
	DaoArray *a0, *a1;
	DArray *dims;

	self->vmc = vmc;
	if( dn->type < DAO_INTEGER || dn->type > DAO_DOUBLE ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "need number" );
		return;
	}
	if( ( self->vmSpace->options & DAO_EXEC_SAFE ) && num > 1000 ){
		DaoContext_RaiseException( self, DAO_ERROR, "not permitted" );
		return;
	}
	array = DaoContext_GetArray( self, vmc );
	if( array->unitype == NULL || array->unitype == dao_array_any ){
		DaoNameSpace *ns = self->nameSpace;
		DaoType *it = DaoNameSpace_GetType( ns, regValues[opA] );
		DaoType *type = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY, NULL, & it, 1 );
		GC_ShiftRC( type, array->unitype ) ;
		array->unitype = type;
		array->numType = regValues[opA]->type;
	}
	DaoArray_ResizeVector( array, num );

	switch( type ){
	case DAO_INTEGER :
		{
			const int first = regValues[ vmc->a ]->xInteger.value;
			const int step = bval==2 ? 1: DaoValue_GetInteger( regValues[opA+1] );
			for(i=0; i<num; i++) array->data.i[i] = first + i*step;
			break;
		}
	case DAO_FLOAT :
		{
			const float first = (const float)regValues[ vmc->a ]->xFloat.value;
			const float step = bval==2 ? 1: DaoValue_GetFloat( regValues[opA+1] );
			for(i=0; i<num; i++) array->data.f[i] = first + i*step;
			break;
		}
	case DAO_DOUBLE :
		{
			const double first = (const double)regValues[ vmc->a ]->xDouble.value;
			const double step = bval==2 ? 1: DaoValue_GetDouble( regValues[opA+1] );
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
		array->numType = a0->numType;
		dims = DArray_New(0);
		DArray_Assign( dims, a0->dims );
		DArray_PushFront( dims, (DaoValue*) num );
		DaoArray_ResizeArray( array, dims->items.pSize, dims->size );
		DArray_Delete( dims );
		if( bval ==3 && regValues[opA+1]->type == DAO_ARRAY ){
			a1 = & regValues[ opA+1 ]->xArray;
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
				for(i=0; i<num; i++) for(j=0; j<a0->size; j++)
					array->data.i[ i*a0->size + j] =
						a0->data.i[j] + i*DaoArray_GetInteger( a1, j );
				break;
			case DAO_FLOAT :
				for(i=0; i<num; i++) for(j=0; j<a0->size; j++)
					array->data.f[ i*a0->size + j] =
						a0->data.f[j] + i*DaoArray_GetFloat( a1, j );
				break;
			case DAO_DOUBLE :
				for(i=0; i<num; i++) for(j=0; j<a0->size; j++)
					array->data.d[ i*a0->size + j] =
						a0->data.d[j] + i*DaoArray_GetDouble( a1, j );
				break;
			case DAO_COMPLEX :
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
			default : break;
			}
		}else{
			switch( a0->numType ){
			case DAO_INTEGER :
				{
					const int step = bval==2 ? 1: DaoValue_GetInteger( regValues[opA+1] );
					for(i=0; i<num; i++) for(j=0; j<a0->size; j++)
						array->data.i[ i*a0->size + j] = a0->data.i[j] + i * step;
					break;
				}
			case DAO_FLOAT :
				{
					float step = bval==2 ? 1.0: DaoValue_GetFloat( regValues[opA+1] );
					for(i=0; i<num; i++) for(j=0; j<a0->size; j++)
						array->data.f[ i*a0->size + j] = a0->data.f[j] + i * step;
					break;
				}
			case DAO_DOUBLE :
				{
					const double step = bval==2 ? 1: DaoValue_GetDouble( regValues[opA+1] );
					for(i=0; i<num; i++) for(j=0; j<a0->size; j++)
						array->data.d[ i*a0->size + j] = a0->data.d[j] + i * step;
					break;
				}
			case DAO_COMPLEX :
				{
					complex16 step = { 1.0, 0.0 };
					int offset = 0;
					double stepR = 0.0, stepI = 0.0;
					if( bval == 3 && regValues[ opA+1 ]->type == DAO_COMPLEX )
						step = regValues[ opA+1 ]->xComplex.value;
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
	DaoValue **pp = self->regValues;
	DaoMap *map = DaoContext_GetMap( self, vmc );

	if( bval == 2 && pp[opA]->type ==0 && pp[opA+1]->type ==0 ) return;
	for( i=0; i<bval-1; i+=2 ){
		if( (c = DaoMap_Insert( map, pp[opA+i], pp[opA+i+1] ) ) ){
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
		tp[0] = DaoNameSpace_GetType( ns, pp[opA] );
		tp[1] = DaoNameSpace_GetType( ns, pp[opA+1] );
		for(i=2; i<bval; i+=2){
			DaoType *tk = DaoNameSpace_GetType( ns, pp[opA+i] );
			DaoType *tv = DaoNameSpace_GetType( ns, pp[opA+i+1] );
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
	const ushort_t bval = vmc->b;
	int i, size, numtype = DAO_INTEGER;
	DaoValue **regv = self->regValues;
	DaoArray *array = NULL;

	size_t dim[2];
	dim[0] = (0xff00 & bval)>>8;
	dim[1] = 0xff & bval;
	size = dim[0] * dim[1];
	array = DaoContext_GetArray( self, vmc );
	if( size ){
		numtype = regv[opA]->type;
		if( numtype == DAO_NULL || numtype > DAO_COMPLEX ){
			DaoContext_RaiseException( self, DAO_ERROR, "invalid matrix enumeration" );
			return;
		}
	}
	if( array->unitype == NULL || array->unitype == dao_array_any ){
		DaoNameSpace *ns = self->nameSpace;
		DaoType *it = DaoNameSpace_GetType( ns, regv[opA] );
		DaoType *type = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY, NULL, & it, 1 );
		GC_ShiftRC( type, array->unitype ) ;
		array->unitype = type;
		array->numType = numtype;
	}
	/* TODO: more restrict type checking on elements. */
	DaoArray_ResizeArray( array, dim, 2 );
	if( numtype == DAO_INTEGER ){
		int *vec = array->data.i;
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
	self->vmc = vmc;
	DaoContext_RaiseException( self, DAO_ERROR, "numeric array is disabled" );
#endif
}

static DaoTuple* DaoContext_GetTuple( DaoContext *self, DaoType *type, int size )
{
	DaoValue *val = self->regValues[ self->vmc->c ];
	DaoTuple *tup = val && val->type == DAO_TUPLE ? & val->xTuple : NULL;
	if( tup && tup->refCount ==1 && tup->unitype == type ) return tup;

	tup = DaoTuple_New( size );
	GC_IncRC( type );
	tup->unitype = type;
	if( type && type->nested->size == size ){
		DaoType **types = type->nested->items.pType;
		int i;
		for(i=0; i<size; i++){
			DaoType *tp = types[i];
			if( tp->tid == DAO_PAR_NAMED || tp->tid == DAO_PAR_DEFAULT ) tp = & tp->aux->xType;
			if( tp->tid > DAO_ENUM && tp->tid != DAO_ANY && tp->tid != DAO_INITYPE ) continue;
			DaoValue_Move( tp->value, tup->items->items.pValue + i, tp );
		}
	}
	GC_ShiftRC( tup, val );
	self->regValues[ self->vmc->c ] = (DaoValue*) tup;
	return tup;
}
DaoTuple* DaoContext_PutTuple( DaoContext *self )
{
	DaoType *type = self->regTypes[ self->vmc->c ];
	if( type == NULL || type->tid != DAO_TUPLE ) return NULL;
	return DaoContext_GetTuple( self, type, type->nested->size );
}

void DaoContext_MakeTuple( DaoContext *self, DaoTuple *tuple, DaoValue *its[], int N )
{
	DaoType *tp, *ct = tuple->unitype;
	DaoValue *val;
	DNode *node;
	int i;
	if( ct == NULL ) return;
	if( ct->nested ==NULL || ct->nested->size != N ){
		DaoContext_RaiseException( self, DAO_ERROR, "invalid tuple enumeration" );
		return;
	}
	for(i=0; i<N; i++){
		val = its[i];
		if( val->type == DAO_PAR_NAMED ){
			DaoNameValue *nameva = & val->xNameValue;
			node = MAP_Find( ct->mapNames, nameva->name );
			if( node == NULL || node->value.pInt != i ){
				DaoContext_RaiseException( self, DAO_ERROR, "name not matched" );
				return;
			}
			val = nameva->value;
		}
		tp = ct->nested->items.pType[i];
		if( tp->tid == DAO_PAR_NAMED || tp->tid == DAO_PAR_DEFAULT ) tp = & tp->aux->xType;
		if( DaoValue_Move( val, tuple->items->items.pValue + i, tp ) == 0){
			DaoContext_RaiseException( self, DAO_ERROR, "invalid tuple enumeration" );
			return;
		}
	}
}
void DaoContext_DoCurry( DaoContext *self, DaoVmCode *vmc )
{
	int i, k;
	int opA = vmc->a;
	int bval = vmc->b;
	DaoObject *object;
	DaoType **mtype;
	DaoValue *selfobj = NULL;
	DaoValue *p = self->regValues[opA];
	DNode *node;

	if( vmc->code == DVM_MCURRY ){
		if( p->type == DAO_ROUTINE ){
			/* XXX here it is not convenient to check attribute for DAO_ROUT_NEEDSELF,
			 * because the routine may have not been compiled,
			 * worse it may have overloaded routine, some may NEEDSELF, some may not. */
			if( ROUT_HOST_TID( & p->xRoutine ) == DAO_OBJECT ) selfobj = self->regValues[opA+1];
		}else if( p->type == DAO_ROUTINE || p->type == DAO_FUNCTION ){
			DRoutine *rout = (DRoutine*) p;
			if( rout->attribs & DAO_ROUT_PARSELF ) selfobj = self->regValues[opA+1];
		}
		opA ++;
		bval --;
	}

	self->vmc = vmc;
	switch( p->type ){
	case DAO_CLASS :
		{
			DaoClass *klass = & p->xClass;
			object = DaoObject_New( klass, NULL, 0 );
			DaoContext_SetValue( self, vmc->c, (DaoValue*)object );
			mtype = klass->objDataType->items.pType;
			if( bval >= object->objData->size ){
				DaoContext_RaiseException( self, DAO_ERROR, "enumerating too many members" );
				break;
			}
			for( i=0; i<bval; i++){
				k = i+1; /* skip self */
				p = self->regValues[opA+i+1];
				if( p->type == DAO_PAR_NAMED ){
					DaoNameValue *nameva = & p->xNameValue;
					node = DMap_Find( klass->lookupTable, nameva->name );
					if( node == NULL || LOOKUP_ST( node->value.pSize ) != DAO_OBJECT_VARIABLE ){
						DaoContext_RaiseException( self, DAO_ERROR_FIELD_NOTEXIST, "" );
						break;
					}
					k = LOOKUP_ID( node->value.pSize );
					p = nameva->value;
				}
				if( DaoValue_Move( p, object->objValues + k, mtype[k] ) ==0 ){
					DaoType *type = DaoNameSpace_GetType( self->nameSpace, p );
					DaoContext_RaiseTypeError( self, type, mtype[k], "moving" );
					break;
				}
			}
			break;
		}
	case DAO_ROUTINE :
	case DAO_FUNCTION :
	case DAO_CONTEXT :
		{
			DaoFunCurry *curry = DaoFunCurry_New( p, selfobj );
			DaoContext_SetValue( self, vmc->c, (DaoValue*)curry );
			for( i=0; i<bval; i++) DArray_Append( curry->params, self->regValues[opA+i+1] );
			break;
		}
	case DAO_FUNCURRY :
		{
			DaoFunCurry *curry = (DaoFunCurry*) p;
			DaoContext_SetValue( self, vmc->c, (DaoValue*)curry );
			for(i=0; i<bval; i++) DArray_Append( curry->params, self->regValues[opA+i+1] );
			break;
		}
	case DAO_TYPE :
		{
			DaoType *type = (DaoType*) p;
			DaoTuple *tuple = DaoContext_GetTuple( self, type, vmc->b );
			if( type->tid != DAO_TUPLE ){
				DaoContext_RaiseException( self, DAO_ERROR, "invalid enumeration" );
				break;
			}
			DaoContext_SetValue( self, vmc->c, (DaoValue*) tuple );
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
	DaoValue *dA = self->routine->routConsts->items.pValue[ vmc->a ];
	DaoValue *dB = self->regValues[ vmc->b ];
	DaoNameValue *nameva = DaoNameValue_New( dA->xString.data, dB );
	nameva->unitype = self->regTypes[ vmc->c ];
	if( nameva->unitype == NULL ){
		DaoNameSpace *ns = self->nameSpace;
		DaoType *tp = DaoNameSpace_GetType( ns, nameva->value );
		nameva->unitype = DaoNameSpace_MakeType( ns, dA->xString.data->mbs, DAO_PAR_NAMED, (DaoValue*)tp, NULL, 0 );
	}
	GC_IncRC( nameva->unitype );
	DaoContext_SetValue( self, vmc->c, (DaoValue*) nameva );
}
void DaoContext_DoPair( DaoContext *self, DaoVmCode *vmc )
{
	DaoNameSpace *ns = self->nameSpace;
	DaoType *tp = self->regTypes[ vmc->c ];
	DaoValue *dA = self->regValues[ vmc->a ];
	DaoValue *dB = self->regValues[ vmc->b ];
	DaoTuple *tuple = DaoTuple_New(2);
	if( tp == NULL ) tp = DaoNameSpace_MakePairValueType( ns, dA, dB );
	tuple->unitype = tp;
	GC_IncRC( tuple->unitype );
	DaoValue_Copy( dA, & tuple->items->items.pValue[0] );
	DaoValue_Copy( dB, & tuple->items->items.pValue[1] );
	DaoContext_SetValue( self, vmc->c, (DaoValue*) tuple );
}
void DaoContext_DoTuple( DaoContext *self, DaoVmCode *vmc )
{
	DaoType *tp, *ct = self->regTypes[ vmc->c ];
	DaoNameSpace *ns = self->nameSpace;
	DaoTuple *tuple;
	DaoValue *val;
	int i;

	self->vmc = vmc;
	tuple = DaoContext_GetTuple( self, ct, vmc->b );
	if( ct == NULL ){
		ct = DaoType_New( "tuple<", DAO_TUPLE, NULL, NULL );
		for(i=0; i<vmc->b; i++){
			val = self->regValues[ vmc->a + i ];
			tp = DaoNameSpace_GetType( ns, val );
			if( tp == NULL ) tp = DaoNameSpace_GetType( ns, null );
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
		tp = DaoNameSpace_FindType( ns, ct->name );
		if( tp ){
			DaoType_Delete( ct );
			ct = tp;
		}else{
			DaoType_CheckAttributes( ct );
			DaoType_InitDefault( ct );
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
	DaoValue *dA = self->regValues[ vmc->a ];
	DaoValue *dB = self->regValues[ vmc->b ];
	DaoType *type = (DaoType*) dB;
	dint *res = 0;
	self->vmc = vmc;
	res = DaoContext_PutInteger( self, 0 );
	if( dA->type && dB->type == DAO_TYPE ){
		if( dA->type == DAO_OBJECT ) dA = (DaoValue*) dA->xObject.that;
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
			*res = dA->xObject.that->myClass == dB->xObject.that->myClass;
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
void DaoContext_DoGetItem( DaoContext *self, DaoVmCode *vmc )
{
	int id;
	DaoValue *B = null;
	DaoValue *A = self->regValues[ vmc->a ];
	DaoTypeCore *tc = DaoValue_GetTyper( A )->priv;
	DaoType *ct = self->regTypes[ vmc->c ];

	self->vmc = vmc;
	if( A == NULL || A->type == 0 ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "on null object" );
		return;
	}
	if( vmc->code == DVM_GETI ) B = self->regValues[ vmc->b ];
	if( A->type == DAO_LIST && (B->type >= DAO_INTEGER && B->type <= DAO_DOUBLE ) ){
		DaoList *list = & A->xList;
		id = DaoValue_GetInteger( B );
		if( id < 0 ) id += list->items->size;
		if( id >=0 && id < list->items->size ){
			GC_ShiftRC( list->items->items.pValue[id], self->regValues[ vmc->c ] );
			self->regValues[ vmc->c ] = list->items->items.pValue[id];
		}else{
			DaoContext_RaiseException( self, DAO_ERROR, "index out of range" );
			return;
		}
#ifdef DAO_WITH_NUMARRAY
	}else if( A->type == DAO_ARRAY && (B->type >=DAO_INTEGER && B->type <=DAO_DOUBLE )){
		DaoDouble tmp = {0,0,1,0,{0,0},0,0,0.0};
		DaoValue *C = (DaoValue*) & tmp;
		DaoArray *na = & A->xArray;
		id = DaoValue_GetInteger( B );
		if( id < 0 ) id += na->size;
		if( id < 0 || id >= na->size ){
			DaoContext_RaiseException( self, DAO_ERROR_INDEX_OUTOFRANGE, "" );
			return;
		}
		C->type = na->numType;
		switch( na->numType ){
		case DAO_INTEGER : C->xInteger.value = na->data.i[id]; break;
		case DAO_FLOAT   : C->xFloat.value = na->data.f[id];  break;
		case DAO_DOUBLE  : C->xDouble.value = na->data.d[id];  break;
		case DAO_COMPLEX : C->xComplex.value = na->data.c[id]; break;
		default : break;
		}
		DaoMoveAC( self, C, & self->regValues[ vmc->c ], ct );
#endif
	}else if( vmc->code == DVM_GETI ){
		tc->GetItem( A, self, self->regValues + vmc->b, 1 );
	}else if( vmc->code == DVM_GETMI ){
		tc->GetItem( A, self, self->regValues + vmc->a + 1, vmc->b );
	}
}
void DaoContext_DoGetField( DaoContext *self, DaoVmCode *vmc )
{
	DaoValue *A = self->regValues[ vmc->a ];
	DaoTypeCore *tc = DaoValue_GetTyper( A )->priv;

	self->vmc = vmc;
	if( A->type == 0 ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "on null object" );
		return;
	}
	tc->GetField( A, self, self->routine->routConsts->items.pValue[ vmc->b]->xString.data );
}
static DaoMap* DaoGetMetaMap( DaoValue *self, int create )
{
	DaoMap *meta = NULL;
	switch( self->type ){
	case DAO_ARRAY : meta = self->xArray.meta; break;
	case DAO_LIST  : meta = self->xList.meta; break;
	case DAO_TUPLE : meta = self->xTuple.meta; break;
	case DAO_MAP   : meta = self->xMap.meta; break;
	case DAO_CTYPE :
	case DAO_CDATA : meta = self->xCdata.meta; break;
	case DAO_OBJECT : meta = self->xObject.meta; break;
	default : break;
	}
	if( meta || create ==0 ) return meta;
	switch( self->type ){
	case DAO_ARRAY : meta = self->xArray.meta = DaoMap_New(1); break;
	case DAO_LIST  : meta = self->xList.meta = DaoMap_New(1); break;
	case DAO_TUPLE : meta = self->xTuple.meta = DaoMap_New(1); break;
	case DAO_MAP   : meta = self->xMap.meta = DaoMap_New(1); break;
	case DAO_CTYPE :
	case DAO_CDATA : meta = self->xCdata.meta = DaoMap_New(1); break;
	case DAO_OBJECT : meta = self->xObject.meta = DaoMap_New(1); break;
	default : break;
	}
	if( meta ){
		meta->unitype = dao_map_meta;
		GC_IncRC( meta );
		GC_IncRC( dao_map_meta );
	}
	return meta;
}
static DaoValue* DaoMap_GetMetaField( DaoMap *self, DaoValue *key )
{
	DNode *node = DMap_Find( self->items, key );
	if( node ) return node->value.pValue;
	if( self->meta ) return DaoMap_GetMetaField( self->meta, key );
	return NULL;
}
void DaoContext_DoGetMetaField( DaoContext *self, DaoVmCode *vmc )
{
	DaoValue *value;
	DaoValue *A = self->regValues[ vmc->a ];
	DaoMap *meta = A->type == DAO_MAP ? & A->xMap : DaoGetMetaMap( A, 0 );

	self->vmc = vmc;
	if( meta == NULL ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "object has no meta fields" );
		return;
	}
	value = DaoMap_GetMetaField( meta, self->routine->routConsts->items.pValue[ vmc->b] );
	if( value == NULL ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "meta field not exists" );
		return;
	}
	self->vmc = vmc;
	DaoContext_PutValue( self, value );
}
void DaoContext_DoSetItem( DaoContext *self, DaoVmCode *vmc )
{
	DaoValue *A, *B = null, *C = self->regValues[ vmc->c ];
	DaoTypeCore *tc = DaoValue_GetTyper( C )->priv;
	int id, rc = 0;

	self->vmc = vmc;
	A = self->regValues[ vmc->a ];
	if( C == NULL || C->type == 0 ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "on null object" );
		return;
	}

	if( vmc->code == DVM_SETI ) B = self->regValues[ vmc->b ];
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
		if( id < 0 ) id += na->size;
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
	}else if( vmc->code == DVM_SETI ){
		tc->SetItem( C, self, self->regValues + vmc->b, 1, A );
	}else if( vmc->code == DVM_SETMI ){
		tc->SetItem( C, self, self->regValues + vmc->c + 1, vmc->b, A );
	}
	if( rc ) DaoContext_RaiseException( self, DAO_ERROR_VALUE, "value type" );
}
void DaoContext_DoSetField( DaoContext *self, DaoVmCode *vmc )
{
	DaoValue *A, *C = self->regValues[ vmc->c ];
	DaoValue *fname = self->routine->routConsts->items.pValue[ vmc->b ];
	DaoTypeCore *tc = DaoValue_GetTyper( C )->priv;

	self->vmc = vmc;
	A = self->regValues[ vmc->a ];
	if( C == NULL || C->type == 0 ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "on null object" );
		return;
	}
	tc->SetField( C, self, fname->xString.data, A );
}
void DaoContext_DoSetMetaField( DaoContext *self, DaoVmCode *vmc )
{
	DaoValue *A = self->regValues[ vmc->a ];
	DaoValue *C = self->regValues[ vmc->c ];
	DaoValue *fname = self->routine->routConsts->items.pValue[ vmc->b ];
	DaoMap *meta = DaoGetMetaMap( C, 1 );
	int m = 1;

	self->vmc = vmc;
	if( meta == NULL ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "object can not have meta fields" );
		return;
	}
	if( C->type == DAO_MAP ) m = DaoMap_Insert( & C->xMap, fname, A );
	if( m ) DaoMap_Insert( meta, fname, A );
	if( A->type == DAO_MAP && strcmp( fname->xString.data->mbs, "__proto__" ) ==0 ){
		GC_ShiftRC( A, meta->meta );
		meta->meta = (DaoMap*) A;
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
int DaoContext_TryObjectArith( DaoContext *self, DaoValue *A, DaoValue *B, DaoValue *C )
{
	DaoRoutine *rout = 0;
	DaoContext *ctx;
	DaoObject *object;
	DaoClass *klass;
	DRoutine *drout;
	DString *name = self->process->mbstring;
	DaoValue **p, *par[3];
	DaoValue *value = NULL;
	int code = self->vmc->code;
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
	nopac = C == NULL || C->xNull.refCount > 1;
	p = par;
	n = npar;

TryAgain:
	if( compo )
		DString_SetMBS( name, daoBitBoolArithOpers2[ code-DVM_MOVE ] );
	else
		DString_SetMBS( name, daoBitBoolArithOpers[ code-DVM_MOVE ] );
	if( DString_EQ( name, self->routine->routName ) ) recursive = 1;

	object = A->type == DAO_OBJECT ? & A->xObject : & B->xObject;
	klass = object->myClass;
	overloaded = klass->attribs & DAO_OPER_OVERLOADED;

	rc = DaoObject_GetData( object, name, & value,  self->object );
	if( rc && (code == DVM_LT || code == DVM_LE) ){
		if( code == DVM_LT ){
			DString_SetMBS( name, ">" );
		}else{
			DString_SetMBS( name, ">=" );
		}
		par[1] = B;
		par[2] = A;
		rc = DaoObject_GetData( object, name, & value,  self->object );
		if( DString_EQ( name, self->routine->routName ) ) recursive = 1;
	}
	if( bothobj && boolres && recursive ) return 0;
	if( rc || value->type < DAO_FUNCTREE || value->type > DAO_FUNCTION ){
		if( bothobj && boolres && overloaded ==0 ) return 0;
		goto ArithError;
	}
	if( compo ==0 ){
		p = par + nopac;
		n = npar - nopac;
	}
	drout = DRoutine_Resolve( value, NULL, p, n, DVM_CALL );
	if( drout == NULL )  goto ArithError;

	if( drout->type == DAO_ROUTINE ){
		rout = (DaoRoutine*) drout;
		ctx = DaoVmProcess_MakeContext( self->process, rout );

		if( ! DRoutine_PassParams( drout, NULL, ctx->regValues, p, n, DVM_CALL ) ) goto ArithError;

		DaoVmProcess_PushContext( self->process, ctx );
		ctx->process->topFrame->returning = self->vmc->c;
	}else{
		DaoFunction_Call( (DaoFunction*)drout, self, NULL, p, n );
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
int DaoContext_TryCDataArith( DaoContext *self, DaoValue *A, DaoValue *B, DaoValue *C )
{
	DaoCData *cdata = NULL;
	DaoValue *func;
	DRoutine *drout;
	DaoValue **p, *par[3];
	DString *name = self->process->mbstring;
	int code = self->vmc->code;
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
	nopac = C == NULL || C->xNull.refCount > 1;
	p = par;
	n = npar;

TryAgain:
	if( compo )
		DString_SetMBS( name, daoBitBoolArithOpers2[ code-DVM_MOVE ] );
	else
		DString_SetMBS( name, daoBitBoolArithOpers[ code-DVM_MOVE ] );

	cdata = A->type == DAO_CDATA ? & A->xCdata : & B->xCdata;
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
	drout = DRoutine_Resolve( func, NULL, p, n, DVM_CALL );
	if( drout == NULL )  goto ArithError;

	DaoFunction_Call( (DaoFunction*)drout, self, NULL, p, n );
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
	DaoValue *A = self->regValues[ vmc->a ];
	DaoValue *B = self->regValues[ vmc->b ];
	DaoValue *C = self->regValues[ vmc->c ];
	self->vmc = vmc;
	if( A == NULL || B == NULL ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "on null object" );
		return;
	}

	if( A->type == DAO_OBJECT || B->type == DAO_OBJECT ){
		self->vmc = vmc;
		DaoContext_TryObjectArith( self, A, B, C );
		return;
	}else if( A->type == DAO_CDATA || B->type == DAO_CDATA ){
		self->vmc = vmc;
		DaoContext_TryCDataArith( self, A, B, C );
		return;
	}

	if( A->type >= DAO_INTEGER && A->type <= DAO_DOUBLE && B->type >= DAO_INTEGER && B->type <= DAO_DOUBLE ){
		DaoValue *val;
		DaoDouble buf = {DAO_DOUBLE,0,1,0,{0,0},0,0,0.0};
		int type = A->type > B->type ? A->type : B->type;
		double res = 0;
		llong_t ib;
		switch( vmc->code ){
		case DVM_MOD:
			ib = DaoValue_GetLongLong( B );
			if( ib ==0 ){
				self->idClearFE = self->vmc - self->codes;
				DaoContext_RaiseException( self, DAO_ERROR_FLOAT_DIVBYZERO, "" );
			}
			res = DaoValue_GetLongLong( A ) % ib;
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
		DaoContext_SetValue( self, vmc->c, val );
		return;
	}else if( B->type >=DAO_INTEGER && B->type <=DAO_DOUBLE && A->type ==DAO_COMPLEX ){
		DaoComplex res = {DAO_COMPLEX,0,1,0,{0,0},0,0,{0.0,0.0}};
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
		DaoContext_SetValue( self, vmc->c, (DaoValue*) & res );
	}else if( A->type >=DAO_INTEGER && A->type <=DAO_DOUBLE && B->type ==DAO_COMPLEX ){
		DaoComplex res = {DAO_COMPLEX,0,1,0,{0,0},0,0,{0.0,0.0}};
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
		DaoContext_SetValue( self, vmc->c, (DaoValue*) & res );
	}else if( A->type == DAO_COMPLEX && B->type == DAO_COMPLEX ){
		DaoComplex res = {DAO_COMPLEX,0,1,0,{0,0},0,0,{0.0,0.0}};
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
		DaoContext_SetValue( self, vmc->c, (DaoValue*) & res );
	}else if( A->type == DAO_LONG && B->type == DAO_LONG ){
		DLong *b, *c = DaoContext_GetLong( self, vmc );
		if( vmc->code == DVM_POW && DaoContext_CheckLong2Integer( self, B->xLong.value ) == 0 ) return;
		b = DLong_New();
		switch( vmc->code ){
		case DVM_ADD : DLong_Add( c, A->xLong.value, B->xLong.value ); break;
		case DVM_SUB : DLong_Sub( c, A->xLong.value, B->xLong.value ); break;
		case DVM_MUL : DLong_Mul( c, A->xLong.value, B->xLong.value ); break;
		case DVM_DIV : DaoContext_LongDiv( self, A->xLong.value, B->xLong.value, c, b ); break;
		case DVM_MOD : DaoContext_LongDiv( self, A->xLong.value, B->xLong.value, b, c ); break;
		case DVM_POW : DLong_Pow( c, A->xLong.value, DLong_ToInteger( B->xLong.value ) ); break;
		default : break;
		}
		DLong_Delete( b );
	}else if( A->type == DAO_LONG && B->type >= DAO_INTEGER && B->type <= DAO_DOUBLE ){
		DLong *c = DaoContext_GetLong( self, vmc );
		DLong *b = DLong_New();
		DLong *b2 = DLong_New();
		dint i = DaoValue_GetInteger( B );
		DLong_FromInteger( b, i );
		switch( vmc->code ){
		case DVM_ADD : DLong_Add( c, A->xLong.value, b ); break;
		case DVM_SUB : DLong_Sub( c, A->xLong.value, b ); break;
		case DVM_MUL : DLong_Mul( c, A->xLong.value, b ); break;
		case DVM_DIV : DaoContext_LongDiv( self, A->xLong.value, b, c, b2 ); break;
		case DVM_MOD : DaoContext_LongDiv( self, A->xLong.value, b, b2, c ); break;
		case DVM_POW : DLong_Pow( c, A->xLong.value, i ); break;
		default: break;
		}
		DLong_Delete( b );
		DLong_Delete( b2 );
	}else if( B->type == DAO_LONG && A->type >= DAO_INTEGER && A->type <= DAO_DOUBLE ){
		DLong *a, *b2, *c = DaoContext_GetLong( self, vmc );
		dint i = DaoValue_GetInteger( A );
		if( vmc->code == DVM_POW && DaoContext_CheckLong2Integer( self, B->xLong.value ) == 0 ) return;
		a = DLong_New();
		b2 = DLong_New();
		DLong_FromInteger( a, i );
		switch( vmc->code ){
		case DVM_ADD : DLong_Add( c, a, B->xLong.value ); break;
		case DVM_SUB : DLong_Sub( c, a, B->xLong.value ); break;
		case DVM_MUL : DLong_MulInt( c, B->xLong.value, i ); break;
		case DVM_DIV : DaoContext_LongDiv( self, a, B->xLong.value, c, b2 ); break;
		case DVM_MOD : DaoContext_LongDiv( self, a, B->xLong.value, b2, c ); break;
		case DVM_POW : DLong_Pow( c, a, DLong_ToInteger( B->xLong.value ) ); break;
		default: break;
		}
		DLong_Delete( a );
		DLong_Delete( b2 );
#ifdef DAO_WITH_NUMARRAY
	}else if( B->type >=DAO_INTEGER && B->type <=DAO_COMPLEX && A->type ==DAO_ARRAY ){
		DaoArray *na = & A->xArray;
		DaoArray *nc = na;
		if( vmc->a != vmc->c ) nc = DaoContext_GetArray( self, vmc );
		DaoArray_array_op_number( nc, na, B, vmc->code, self );
	}else if( A->type >=DAO_INTEGER && A->type <=DAO_COMPLEX && B->type ==DAO_ARRAY ){
		DaoArray *nb = & B->xArray;
		DaoArray *nc = nb;
		if( vmc->b != vmc->c ) nc = DaoContext_GetArray( self, vmc );
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
			nc = DaoContext_GetArray( self, vmc );
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
			DaoValue *C = DaoContext_PutValue( self, A );
			DString_Append( C->xString.data, B->xString.data );
		}
	}else if( A->type == DAO_ENUM && B->type == DAO_ENUM
			&& (vmc->code == DVM_ADD || vmc->code == DVM_SUB) ){
		DaoType *ta = A->xEnum.etype;
		DaoType *tb = B->xEnum.etype;
		DaoEnum *denum = & A->xEnum;
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
			DaoEnum_SetType( denum, type );
			denum->value = value;
			return;
		}
		if( vmc->c != vmc->a ){
			denum = DaoContext_GetEnum( self, vmc );
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
				DaoContext_RaiseException( self, DAO_ERROR_TYPE, "not combinable enum" );
			else
				DaoContext_RaiseException( self, DAO_ERROR_TYPE, "symbol not found in the enum" );
			return;
		}
	}else if( A->type == DAO_LIST && B->type == DAO_LIST && vmc->code == DVM_ADD ){
		DaoList *lA = & A->xList;
		DaoList *lB = & B->xList;
		DaoList *list;
		size_t i = 0, NA = lA->items->size, NB = lB->items->size;
		if( vmc->a == vmc->c ){
			list = lA;
			for(i=0; i<NB; i++) DaoList_Append( list, lB->items->items.pValue[i] );
		}else if( vmc->b == vmc->c ){
			list = lB;
			for(i=NA-1; i>=0; i--) DaoList_PushFront( list, lA->items->items.pValue[i] );
		}else{
			list = DaoContext_GetList( self, vmc );
			DArray_Resize( list->items, NA + NB, NULL );
			for(i=0; i<NA; i++) DaoList_SetItem( list, lA->items->items.pValue[i], i );
			for(i=0; i<NB; i++) DaoList_SetItem( list, lB->items->items.pValue[i], i + NA );
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
	DaoValue *A = self->regValues[ vmc->a ];
	DaoValue *B = self->regValues[ vmc->b ];
	DaoValue *C = NULL;
	int D = 0, rc = 0;

	self->vmc = vmc;
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
			DaoCData *cdata = (DaoCData*)( A->type == DAO_CDATA ? & A->xCdata : & B->xCdata );
			if( vmc->code == DVM_EQ ){
				D = cdata->data ? 0 : 1;
			}else if( vmc->code == DVM_NE ){
				D = cdata->data ? 1 : 0;
			}
		}
		if( C ) DaoContext_PutValue( self, C );
		else DaoContext_PutInteger( self, D );
		return;
	}

	if( A->type==DAO_OBJECT || B->type==DAO_OBJECT || A->type==DAO_CDATA || B->type==DAO_CDATA ){
		if( A->type ==DAO_OBJECT || B->type ==DAO_OBJECT ){
			rc = DaoContext_TryObjectArith( self, A, B, C );
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
				if( C ) DaoContext_PutValue( self, C );
				else DaoContext_PutInteger( self, D );
			}
		}else if( A->type ==DAO_CDATA || B->type ==DAO_CDATA ){
			rc = DaoContext_TryCDataArith( self, A, B, C );
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
				if( C ) DaoContext_PutValue( self, C );
				else DaoContext_PutInteger( self, D );
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
		case DAO_LIST : C = A->xList.items->size ? BB : AA; break;
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
	if( C ) DaoContext_PutValue( self, C );
	else DaoContext_PutInteger( self, D );
}
void DaoContext_DoUnaArith( DaoContext *self, DaoVmCode *vmc )
{
	DaoValue *A = self->regValues[ vmc->a ];
	DaoValue *C = NULL;
	int ta = A->type;
	self->vmc = vmc;
	if( A->type ==0 ){
		DaoContext_RaiseException( self, DAO_ERROR_TYPE, "on null object" );
		return;
	}

	if( ta == DAO_INTEGER ){
		C = DaoContext_SetValue( self, vmc->c, A );
		switch( vmc->code ){
		case DVM_NOT :  C->xInteger.value = ! C->xInteger.value; break;
		case DVM_UNMS : C->xInteger.value = - C->xInteger.value; break;
		default: break;
		}
	}else if( ta == DAO_FLOAT ){
		C = DaoContext_SetValue( self, vmc->c, A );
		switch( vmc->code ){
		case DVM_NOT :  C->xFloat.value = ! C->xFloat.value; break;
		case DVM_UNMS : C->xFloat.value = - C->xFloat.value; break;
		default: break;
		}
	}else if( ta == DAO_DOUBLE ){
		C = DaoContext_SetValue( self, vmc->c, A );
		switch( vmc->code ){
		case DVM_NOT :  C->xDouble.value = ! C->xDouble.value; break;
		case DVM_UNMS : C->xDouble.value = - C->xDouble.value; break;
		default: break;
		}
	}else if( ta == DAO_COMPLEX ){
		if( vmc->code == DVM_UNMS ){
			C = DaoContext_SetValue( self, vmc->c, A );
			C->xComplex.value.real = - C->xComplex.value.real;
			C->xComplex.value.imag = - C->xComplex.value.imag;
		}
	}else if( ta == DAO_LONG ){
		C = DaoContext_SetValue( self, vmc->c, A );
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
		if( array->numType == DAO_FLOAT ){
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
		C = self->regValues[ vmc->c ];
		DaoContext_TryObjectArith( self, A, NULL, C );
		return;
	}else if( ta == DAO_CDATA ){
		C = self->regValues[ vmc->c ];
		DaoContext_TryCDataArith( self, A, NULL, C );
		return;
	}
	if( C ==0 ) DaoContext_RaiseException( self, DAO_ERROR_TYPE, "" );
}
void DaoContext_DoInTest( DaoContext *self, DaoVmCode *vmc )
{
	DaoValue *A = self->regValues[ vmc->a ];
	DaoValue *B = self->regValues[ vmc->b ];
	dint *C = DaoContext_PutInteger( self, 0 );
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
		DArray *items = B->xList.items;
		DaoType *ta = DaoNameSpace_GetType( self->nameSpace, A );
		if( ta && B->xList.unitype && B->xList.unitype->nested->size ){
			DaoType *tb = B->xList.unitype->nested->items.pType[0];
			if( tb && DaoType_MatchTo( ta, tb, NULL ) < DAO_MT_SUB	 ) return;
		}
		for(i=0; i<items->size; i++){
			*C = DaoValue_Compare( A, items->items.pValue[i] ) ==0;
			if( *C ) break;
		}
	}else if( B->type == DAO_MAP ){
		DaoType *ta = DaoNameSpace_GetType( self->nameSpace, A );
		if( ta && B->xMap.unitype && B->xMap.unitype->nested->size ){
			DaoType *tb = B->xMap.unitype->nested->items.pType[0];
			if( tb && DaoType_MatchTo( ta, tb, NULL ) < DAO_MT_SUB	 ) return;
		}
		*C = DMap_Find( B->xMap.items, & A ) != NULL;
	}else if( B->type == DAO_TUPLE && B->xTuple.pair ){
		int c1 = DaoValue_Compare( B->xTuple.items->items.pValue[0], A );
		int c2 = DaoValue_Compare( A, B->xTuple.items->items.pValue[1] );
		*C = c1 <=0 && c2 <= 0;
	}else{
		DaoContext_RaiseException( self, DAO_ERROR_TYPE, "" );
	}
}
void DaoContext_DoBitLogic( DaoContext *self, DaoVmCode *vmc )
{
	DaoValue *A = self->regValues[ vmc->a ];
	DaoValue *B = self->regValues[ vmc->b ];
	ullong_t inum = 0;

	self->vmc = vmc;
	if( A->type && B->type && A->type <= DAO_DOUBLE && B->type <= DAO_DOUBLE ){
		switch( vmc->code ){
		case DVM_BITAND: inum =DaoValue_GetInteger(A) & DaoValue_GetInteger(B);break;
		case DVM_BITOR: inum =DaoValue_GetInteger(A) | DaoValue_GetInteger(B);break;
		case DVM_BITXOR: inum =DaoValue_GetInteger(A) ^ DaoValue_GetInteger(B);break;
		default : break;
		}
		if( A->type == DAO_DOUBLE || B->type == DAO_DOUBLE ){
			DaoContext_PutDouble( self, inum );
		}else if( A->type == DAO_FLOAT || B->type == DAO_FLOAT ){
			DaoContext_PutFloat( self, inum );
		}else{
			DaoContext_PutInteger( self, inum );
		}
	}else if( A->type == DAO_LONG && B->type >= DAO_INTEGER && B->type <= DAO_DOUBLE ){
		DLong *bigint = DaoContext_PutLong( self );
		DLong_FromInteger( bigint, DaoValue_GetInteger( B ) );
		switch( vmc->code ){
		case DVM_BITAND : DLong_BitAND( bigint, A->xLong.value, bigint ); break;
		case DVM_BITOR :  DLong_BitOR( bigint, A->xLong.value, bigint ); break;
		case DVM_BITXOR : DLong_BitXOR( bigint, A->xLong.value, bigint ); break;
		default : break;
		}
	}else if( B->type == DAO_LONG && A->type >= DAO_INTEGER && A->type <= DAO_DOUBLE ){
		DLong *bigint = DaoContext_PutLong( self );
		DLong_FromInteger( bigint, DaoValue_GetInteger( A ) );
		switch( vmc->code ){
		case DVM_BITAND : DLong_BitAND( bigint, B->xLong.value, bigint ); break;
		case DVM_BITOR :  DLong_BitOR( bigint, B->xLong.value, bigint ); break;
		case DVM_BITXOR : DLong_BitXOR( bigint, B->xLong.value, bigint ); break;
		default : break;
		}
	}else if( A->type ==DAO_LONG && B->type == DAO_LONG ){
		DLong *bigint = DaoContext_PutLong( self );
		switch( vmc->code ){
		case DVM_BITAND : DLong_BitAND( bigint, A->xLong.value, B->xLong.value ); break;
		case DVM_BITOR :  DLong_BitOR( bigint, A->xLong.value, B->xLong.value ); break;
		case DVM_BITXOR : DLong_BitXOR( bigint, A->xLong.value, B->xLong.value ); break;
		default : break;
		}
	}else{
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "invalid operands" );
	}
}
void DaoContext_DoBitShift( DaoContext *self, DaoVmCode *vmc )
{
	DaoValue *A = self->regValues[ vmc->a ];
	DaoValue *B = self->regValues[ vmc->b ];
	if( A->type && B->type && A->type <= DAO_DOUBLE && B->type <= DAO_DOUBLE ){
		llong_t inum = 0;
		if( vmc->code == DVM_BITLFT ){
			inum = DaoValue_GetInteger(A) << DaoValue_GetInteger(B);
		}else{
			inum = DaoValue_GetInteger(A) >> DaoValue_GetInteger(B);
		}
		if( A->type == DAO_DOUBLE || B->type == DAO_DOUBLE ){
			DaoContext_PutDouble( self, inum );
		}else if( A->type == DAO_FLOAT || B->type == DAO_FLOAT ){
			DaoContext_PutFloat( self, inum );
		}else{
			DaoContext_PutInteger( self, inum );
		}
	}else if( A->type ==DAO_LONG && B->type >=DAO_INTEGER && B->type <= DAO_DOUBLE ){
		if( vmc->a == vmc->c ){
			if( vmc->code == DVM_BITLFT ){
				DLong_ShiftLeft( A->xLong.value, DaoValue_GetInteger( B ) );
			}else{
				DLong_ShiftRight( A->xLong.value, DaoValue_GetInteger( B ) );
			}
		}else{
			DLong *bigint = DaoContext_PutLong( self );
			DLong_Move( bigint, A->xLong.value );
			if( vmc->code == DVM_BITLFT ){
				DLong_ShiftLeft( bigint, DaoValue_GetInteger( B ) );
			}else{
				DLong_ShiftRight( bigint, DaoValue_GetInteger( B ) );
			}
		}
	}else if( A->type ==DAO_LIST && (vmc->code ==DVM_BITLFT || vmc->code ==DVM_BITRIT) ){
		DaoList *list = & self->regValues[ vmc->a ]->xList;
		self->vmc = vmc;
		if( DaoContext_SetValue( self, vmc->c, A ) ==0 ) return;
		if( vmc->code ==DVM_BITLFT ){
			DaoType *abtp = list->unitype;
			if( abtp && abtp->nested->size ){
				abtp = abtp->nested->items.pType[0];
				if( DaoType_MatchValue( abtp, B, NULL ) ==0 ) return; /* XXX information */
			}
			DArray_PushBack( list->items, B );
		}else{
			if( list->items->size ==0 ) return; /* XXX information */
			B = list->items->items.pValue[list->items->size-1];
			if( DaoContext_SetValue( self, vmc->b, B ) ==0 ) return;
			DArray_PopBack( list->items );
		}
	}else{
		self->vmc = vmc;
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "invalid operands" );
	}
}
void DaoContext_DoBitFlip( DaoContext *self, DaoVmCode *vmc )
{
	DaoValue *A = self->regValues[ vmc->a ];
	self->vmc = vmc;
	if( A->type >= DAO_INTEGER && A->type <= DAO_DOUBLE ){
		switch( A->type ){
		case DAO_INTEGER : DaoContext_PutInteger( self, A->xInteger.value ); break;
		case DAO_FLOAT   : DaoContext_PutFloat( self, ~(llong_t)A->xFloat.value ); break;
		case DAO_DOUBLE  : DaoContext_PutDouble( self, ~(llong_t)A->xDouble.value ); break;
		}
	}else if( A->type == DAO_LONG ){
		DLong *bigint = DaoContext_PutLong( self );
		DLong_Move( bigint, A->xLong.value );
		DLong_Flip( bigint );
	}else{
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "invalid operands" );
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
		list = & self->xList;
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
		data = list->items->items.pValue;
		for(i=0; i<list->items->size; i++ ){
			type = DaoValue_CheckTypeShape( data[i], type, shape, depth, check_size );
			if( type < 0 ) break;
		}
		break;
	case DAO_TUPLE :
		tuple = & self->xTuple;
		if( shape->size <= depth ) DArray_Append( shape, (void*)(size_t)tuple->items->size );
		if( check_size && tuple->items->size != shape->items.pSize[depth] ) return -1;
		depth ++;
		data = tuple->items->items.pValue;
		for(i=0; i<tuple->items->size; i++){
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
		switch( array->numType ){
		case DAO_INTEGER : array->data.i[k] = DaoValue_GetInteger( self ); break;
		case DAO_FLOAT : array->data.f[k] = DaoValue_GetFloat( self ); break;
		case DAO_DOUBLE : array->data.d[k] = DaoValue_GetDouble( self ); break;
		case DAO_COMPLEX : array->data.c[k].real = DaoValue_GetDouble( self ); break;
		default : break;
		}
		k ++;
		break;
	case DAO_COMPLEX :
		switch( array->numType ){
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
		list = & self->xList;
		items = list->items->items.pValue;
		for(i=0; i<list->items->size; i++ ) k = DaoValue_ExportValue( items[i], array, k );
		break;
	case DAO_TUPLE :
		tup = & self->xTuple;
		items = tup->items->items.pValue;
		for(i=0; i<tup->items->size; i++) k = DaoValue_ExportValue( items[i], array, k );
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
	DaoComplex tmp = {DAO_COMPLEX,0,1,0,{0,0},0,0,{0.0,0.0}};
	DaoValue *value = (DaoValue*) & tmp;
	DaoList *ls;
	size_t *ds = self->dims->items.pSize;
	size_t i, size, isvector = self->dims->size==2 && (ds[0] ==1 || ds[1] ==1);

	if( abtp == NULL ) return 0;
	abtp = abtp->nested->items.pType[0];
	if( abtp == NULL ) return 0;
	if( isvector ) dim = ds[0] ==1 ? 1 : 0;
	if( dim >= self->dims->size-1 || isvector ){
		if( abtp->tid == DAO_INTEGER ){
			value->type = DAO_INTEGER;
			switch( self->numType ){
			case DAO_INTEGER :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					value->xInteger.value = self->data.i[ offset+i ];
					DaoList_Append( list, value );
				}
				break;
			case DAO_FLOAT :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					value->xInteger.value = self->data.f[ offset+i ];
					DaoList_Append( list, value );
				}
				break;
			case DAO_DOUBLE :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					value->xInteger.value = self->data.d[ offset+i ];
					DaoList_Append( list, value );
				}
				break;
			default : break;
			}
		}else if( abtp->tid == DAO_FLOAT ){
			value->type = DAO_FLOAT;
			switch( self->numType ){
			case DAO_INTEGER :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					value->xFloat.value = self->data.i[ offset+i ];
					DaoList_Append( list, value );
				}
				break;
			case DAO_FLOAT :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					value->xFloat.value = self->data.f[ offset+i ];
					DaoList_Append( list, value );
				}
				break;
			case DAO_DOUBLE :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					value->xFloat.value = self->data.d[ offset+i ];
					DaoList_Append( list, value );
				}
				break;
			default : break;
			}
		}else if( abtp->tid == DAO_DOUBLE ){
			value->type = DAO_DOUBLE;
			switch( self->numType ){
			case DAO_INTEGER :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					value->xDouble.value = self->data.i[ offset+i ];
					DaoList_Append( list, value );
				}
				break;
			case DAO_FLOAT :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					value->xDouble.value = self->data.f[ offset+i ];
					DaoList_Append( list, value );
				}
				break;
			case DAO_DOUBLE :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					value->xDouble.value = self->data.d[ offset+i ];
					DaoList_Append( list, value );
				}
				break;
			default : break;
			}
		}else if( abtp->tid == DAO_COMPLEX ){
			value->type = DAO_COMPLEX;
			value->xComplex.value.imag = 0.0;
			switch( self->numType ){
			case DAO_INTEGER :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					value->xComplex.value.real = self->data.i[ offset+i ];
					DaoList_Append( list, value );
				}
				break;
			case DAO_FLOAT :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					value->xComplex.value.real = self->data.f[ offset+i ];
					DaoList_Append( list, value );
				}
				break;
			case DAO_DOUBLE :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
					value->xComplex.value.real = self->data.d[ offset+i ];
					DaoList_Append( list, value );
				}
				break;
			case DAO_COMPLEX :
				for( i=0; i<self->dims->items.pSize[dim]; i++ ){
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
			size = self->dimAccum->items.pSize[dim];
			for(i=0; i<self->dims->items.pSize[dim]; i++){
				DaoArray_ToWCString( self, value->xString.data, offset, size );
				DaoList_Append( list, value );
				offset += self->dimAccum->items.pSize[dim];
			}
			DString_Delete( value->xString.data );
		}else if( abtp->tid == DAO_LIST ){
			for(i=0; i<self->dims->items.pSize[dim]; i++){
				ls = DaoList_New();
				ls->unitype = abtp;
				GC_IncRC( abtp );
				DaoList_Append( list, (DaoValue*) ls );
				DaoArray_ToList( self, ls, abtp, dim+1, offset );
				offset += self->dimAccum->items.pSize[dim];
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
	self->numType = type;
	if( tp && tp->tid && tp->tid <= DAO_COMPLEX ) self->numType = tp->tid;
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
int ConvertStringToNumber( DaoContext *ctx, DaoValue *dA, DaoValue *dC )
{
	DString *mbs = ctx->process->mbstring;
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
				DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, msg );
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
typedef struct CastBuffer CastBuffer;
struct CastBuffer
{
	DLong    *lng;
	DString  *str;
};
void CastBuffer_Clear( CastBuffer *self )
{
	if( self->lng ) DLong_Delete( self->lng );
	if( self->str ) DString_Delete( self->str );
}
static DaoValue* DaoTypeCast( DaoContext *ctx, DaoType *ct, DaoValue *dA, DaoValue *dC, CastBuffer *b1, CastBuffer *b2 )
{
	DaoNameSpace *ns = ctx->nameSpace;
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
	key.xNull.konst = value.xNull.konst = 1;
	dC->type = ct->tid;
	if( ct->tid == DAO_ANY ) goto Rebind;
	if( dA->type == ct->tid && ct->tid >= DAO_INTEGER && ct->tid < DAO_ARRAY ) goto Rebind;
	if( dA->type == DAO_STRING && ct->tid > 0 && ct->tid <= DAO_LONG ){
		if( dC->type == DAO_LONG ){
			if( b1->lng == NULL ) b1->lng = DLong_New();
			dC->xLong.value = b1->lng;
		}
		if( ConvertStringToNumber( ctx, dA, dC ) ==0 ) goto FailConversion;
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
			DString_Resize( wcs, list->items->size );
			type = 1; /*MBS*/
			for(i=0; i<list->items->size; i++){
				wcs->wcs[i] = DaoValue_GetInteger( list->items->items.pValue[i] );
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
			if( array2->numType == tp->tid ) goto Rebind;
			if( tp->tid < DAO_INTEGER || tp->tid > DAO_COMPLEX ) goto FailConversion;
			array2 = & dA->xArray;
			size = array2->size;
			array = DaoArray_New( tp->tid );
			if( array2->numType == DAO_DOUBLE && tp->tid == DAO_COMPLEX )
				array->numType = DAO_COMPLEX;
			DaoArray_ResizeArray( array, array2->dims->items.pSize, array2->dims->size );
			if( array2->numType == DAO_INTEGER ){
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
		}else if( dA->type == DAO_LIST ){
			if( tp == NULL ) goto FailConversion;
			if( tp->tid ==0 || tp->tid > DAO_COMPLEX ) goto FailConversion;
			shape = DArray_New(0);
			type = DaoValue_CheckTypeShape( dA, DAO_UDF, shape, 0, 1 );
			if( type <0 ) goto FailConversion;
			array = DaoArray_New( DAO_INTEGER );
			array->numType = tp->tid;
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
			DArray_Resize( list->items, DString_Size( str ), & value );
			data = list->items->items.pValue;
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
			DArray_Resize( list->items, list2->items->size, NULL );
			data = list->items->items.pValue;
			data2 = list2->items->items.pValue;
			for(i=0; i<list2->items->size; i++ ){
				V = DaoTypeCast( ctx, tp, data2[i], & value, b1, b2 );
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
			if( m == DAO_MT_ANY || m == DAO_MT_EQ ) goto Rebind;
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
			K = DaoTypeCast( ctx, tp, node->key.pValue, & key, b1, b2 );
			V = DaoTypeCast( ctx, tp2, node->value.pValue, & value, b2, b1 );
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
				size = dA->xList.items->size;
				items = dA->xList.items->items.pValue;
			}else{
				size = dA->xTuple.items->size;
				items = dA->xTuple.items->items.pValue;
			}
			if( size < tsize ) goto FailConversion;
			if( tsize ) size = tsize;
			tuple = DaoTuple_New( size );
			GC_IncRC( ct );
			tuple->unitype = ct;
			dC = (DaoValue*) tuple;
			for(i=0; i<size; i++){
				DaoValue *V = items[i];
				tp = DaoNameSpace_GetType( ns, V );
				if( tsize ){
					tp2 = ct->nested->items.pType[i];
					if( tp2->tid == DAO_PAR_NAMED ) tp2 = & tp2->aux->xType;
					/* if( DaoType_MatchTo( tp, tp2, 0 ) ==0 ) goto FailConversion; */
					V = DaoTypeCast( ctx, tp2, V, & value, b1, b2 );
				}
				if( V == NULL || V->type == 0 ) goto FailConversion;
				DaoValue_Copy( V, tuple->items->items.pValue + i );
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
					DaoValue_Copy( node->value.pValue, tuple->items->items.pValue + i );
				}else{
					tp2 = ct->nested->items.pType[i];
					if( node->key.pValue->type != DAO_STRING ) goto FailConversion;
					V = DaoTypeCast( ctx, tp2, node->value.pValue, & value, b1, b2 );
					if( V == NULL || V->type ==0 ) goto FailConversion;
					DaoValue_Copy( V, tuple->items->items.pValue + i );
				}
				i ++;
			}
		}else{
			goto FailConversion;
		}
		break;
	case DAO_OBJECT :
		if( dA->type == DAO_CDATA ) dA = (DaoValue*) dA->xCdata.daoObject;
		/* XXX compiling time checking */
		if( dA == NULL || dA->type != DAO_OBJECT ) goto FailConversion;
		dC = DaoObject_MapThisObject( & dA->xObject, ct );
		if( dC == NULL ) goto FailConversion;
		break;
	case DAO_CTYPE :
	case DAO_CDATA :
		if( dA->type == DAO_CDATA ){
			if( DaoCData_ChildOf( dA->xCdata.typer, ct->typer ) ){
				dC = dA;
				/*
				   }else if( DaoCData_ChildOf( ct->typer, dA->typer ) ){
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
	dC->type = 0;
	return dC;
}
void DaoContext_DoCast( DaoContext *self, DaoVmCode *vmc )
{
	CastBuffer buffer1 = {NULL, NULL};
	CastBuffer buffer2 = {NULL, NULL};
	DaoType *at, *ct = self->regTypes[ vmc->c ];
	DaoValue *va = self->regValues[ vmc->a ];
	DaoValue *vc = self->regValues[ vmc->c ];
	DaoValue **vc2 = self->regValues + vmc->c;
	DaoValue value;
	DRoutine *rout;
	DNode *node;
	int i, mt, mt2;

	self->vmc = vmc;
	if( va == NULL ){
		DaoContext_RaiseException( self, DAO_ERROR_VALUE, "operate on null object" );
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
		at = DaoNameSpace_GetType( self->nameSpace, va );
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
		DaoContext *ctx;
		DaoClass *scope = self->object ? self->object->myClass : NULL;
		DaoValue *tpar = (DaoValue*) ct;
		rout = (DRoutine*)DaoClass_FindOperator( va->xObject.myClass, "cast", scope );
		if( rout ) rout = DRoutine_Resolve( (DaoValue*) rout, va, & tpar, 1, DVM_CALL );
		if( rout == NULL ) goto NormalCasting;

		ctx = DaoVmProcess_MakeContext( self->process, (DaoRoutine*) rout );
		if( ! DRoutine_PassParams( rout, va, ctx->regValues, & tpar, 1, DVM_CALL ) ) goto NormalCasting;
		DaoVmProcess_PushContext( self->process, ctx );
		ctx->process->topFrame->returning = self->vmc->c;
		return;
	}else if( va->type == DAO_CDATA ){
		DaoCData *cdata = & va->xCdata;
		DaoValue *tpar = (DaoValue*) ct;
		rout = (DRoutine*) DaoFindFunction2( (DaoTypeBase*) cdata->typer, "cast" );
		if( rout ) rout = DRoutine_Resolve( (DaoValue*)rout, va, & tpar, 1, DVM_CALL );
		if( rout == NULL ) goto NormalCasting;
		DaoFunction_Call( (DaoFunction*) rout, self, va, & tpar, 1 );
		return;
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
	at = DaoNameSpace_GetType( self->nameSpace, self->regValues[ vmc->a ] );
	DaoContext_RaiseTypeError( self, at, ct, "casting" );
	CastBuffer_Clear( & buffer1 );
	CastBuffer_Clear( & buffer2 );
}
void DaoContext_DoMove( DaoContext *self, DaoVmCode *vmc )
{
	DaoType *ct = self->regTypes[ vmc->c ];
	DaoValue *A = self->regValues[ vmc->a ];
	DaoValue *C = self->regValues[ vmc->c ];
	int overload = 0;
	if( C ){
		if( A->type == C->type && C->type == DAO_OBJECT ){
			overload = DaoClass_ChildOf( A->xObject.myClass, (DaoValue*)C->xObject.myClass ) == 0;
		}else if( A->type == C->type && C->type == DAO_CDATA ){
			overload = DaoCData_ChildOf( A->xCdata.typer, C->xCdata.typer ) == 0;
		}else if( C->type == DAO_OBJECT || C->type == DAO_CDATA ){
			overload = 1;
		}
		if( overload ){
			DaoValue *rout = NULL;
			if( C->type == DAO_OBJECT ){
				DaoClass *scope = self->object ? self->object->myClass : NULL;
				rout = DaoClass_FindOperator( C->xObject.myClass, "=", scope );
			}else{
				rout = DaoFindFunction2( C->xCdata.typer, "=" );
			}
			if( rout ) rout = (DaoValue*) DRoutine_Resolve( rout, C, & A, 1, DVM_CALL );
			if( rout && DaoVmProcess_Call( self->process, (DaoMethod*) rout, C, & A, 1 ) ) return;
		}
	}
	DaoMoveAC( self, A, & self->regValues[vmc->c], ct );
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
DaoRoutine* DaoRoutine_Decorate( DaoRoutine *self, DaoRoutine *decoFunc, DaoValue *p[], int n )
{
	DArray *nested = decoFunc->routType->nested;
	DaoType **decotypes = nested->items.pType;
	DaoParser *parser = DaoParser_New();
	DaoRoutine *routine = DaoRoutine_New();
	DMap *mapids = DMap_New(0,D_STRING);
	int parpass[DAO_MAX_PARAM];
	int i, j, k;
	if( decoFunc->type == DAO_FUNCTREE ){
		decoFunc = (DaoRoutine*) DRoutine_Resolve( (DaoValue*)decoFunc, NULL, p, n, DVM_CALL );
		if( decoFunc == NULL || decoFunc->type != DAO_ROUTINE ) return NULL;
		nested = decoFunc->routType->nested;
		decotypes = nested->items.pType;
	}
	if( self->type == DAO_FUNCTREE ){
		DaoFunctree *meta = (DaoFunctree*)self;
		DaoType *ftype = & decotypes[0]->aux->xType;
		DaoType **pts = ftype->nested->items.pType;
		int nn = ftype->nested->size;
		int code = DVM_CALL + (ftype->attrib & DAO_TYPE_SELF);
		self = (DaoRoutine*) DRoutine_ResolveByType( (DaoValue*)meta, NULL, pts, nn, code );
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
	DString_Assign( routine->parCodes, self->parCodes );
	for(i=0; i<self->routType->nested->size; i++){
		DaoType *type = self->routType->nested->items.pType[i];
		DRoutine_AddConstant( (DRoutine*)routine, self->routConsts->items.pValue[i] );
		if( type->tid == DAO_PAR_VALIST ) break;
		MAP_Insert( DArray_Top( parser->localVarMap ), type->fname, i );
		DArray_Append( routine->defLocals, self->defLocals->items.pToken[i] );
	}
	parser->regCount = self->parCount;
	i = DRoutine_AddConstant( (DRoutine*)routine, (DaoValue*) self );
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
		j = DRoutine_AddConstant( (DRoutine*)routine, pv );
		MAP_Insert( DArray_Top( parser->localCstMap ), decotypes[k]->fname, j );
		parpass[k] = 1;
		k += 1;
	}
	for(i=1; i<nested->size; i++){
		k = decotypes[i]->tid;
		if( k == DAO_PAR_VALIST ) break;
		if( parpass[i] ) continue;
		if( k != DAO_PAR_DEFAULT ) continue;
		k = DRoutine_AddConstant( (DRoutine*)routine, decoFunc->routConsts->items.pValue[i] );
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
void DaoValue_Check( DaoValue *self, DaoType *selftp, DaoType *ts[], int np, int code, DArray *es );
void DaoPrintCallError( DArray *errors, DaoStream *stdio );

void DaoContext_ShowCallError( DaoContext *self, DRoutine *rout,
		DaoValue *selfobj, DaoValue *ps[], int np, int code )
{
	DaoStream *ss = DaoStream_New();
	DaoNameSpace *ns = self->nameSpace;
	DaoType *selftype = selfobj ? DaoNameSpace_GetType( ns, selfobj ) : NULL;
	DaoType *ts[DAO_MAX_PARAM];
	DArray *errors = DArray_New(0);
	int i;
	for(i=0; i<np; i++) ts[i] = DaoNameSpace_GetType( ns, ps[i] );
	DaoValue_Check( (DaoValue*) rout, selftype, ts, np, code, errors );
	ss->attribs |= DAO_IO_STRING;
	DaoPrintCallError( errors, ss );
	DArray_Delete( errors );
	DaoContext_RaiseException( self, DAO_ERROR_PARAM, ss->streamString->mbs );
	DaoStream_Delete( ss );
}

static void DaoContext_TryTailCall( DaoContext *self, DaoVmCode *vmc, DaoContext *ctx )
{
	if( (vmc->b & DAO_CALL_TAIL) && self->process->topFrame->depth <=1 ){
		/* no tail call in try{} */
		DaoVmFrame *frame = self->frame;
		DaoVmProcess_PopContext( self->process );
		ctx->frame = frame;
		self->frame = frame->next; // XXX
		frame->context = ctx;
		frame->next->context = self;
	}
}
static int DaoContext_TrySynCall( DaoContext *self, DaoVmCode *vmc, DaoContext *ctx )
{
#if( defined DAO_WITH_THREAD && defined DAO_WITH_SYNCLASS )
	DaoType *retype = self->regTypes[ self->vmc->c ];
	if( retype && retype->tid == DAO_FUTURE ){
		DaoNameSpace *ns = self->nameSpace;
		DaoFuture *future = DaoCallServer_Add( ctx, NULL, NULL );
		DaoType *retype = & ctx->routine->routType->aux->xType;
		DaoType *type = DaoNameSpace_MakeType( ns, "future", DAO_FUTURE, NULL, &retype,1 );
		GC_ShiftRC( type, future->unitype );
		future->unitype = type;
		DaoContext_PutValue( self, (DaoValue*) future );
		return 1;
	}
#endif
	return 0;
}
static int DaoContext_InitBase( DaoContext *self, DaoVmCode *vmc, DaoValue *caller )
{
	int mode = vmc->b & 0xff00;
	if( (mode & DAO_CALL_INIT) && self->object ){
		DaoClass *klass = self->object->myClass;
		int init = self->routine->attribs & DAO_ROUT_INITOR;
		if( self->routine->routHost == klass->objType && init ){
			return DaoClass_FindSuper( klass, caller );
		}
	}
	return -1;
}
static void DaoContext_PrepareCall( DaoContext *self, DaoVmCode *vmc,
		DaoContext *ctx, DaoValue *selfpar, DaoValue *params[], int npar )
{
	DRoutine *rout = (DRoutine*)ctx->routine;
	int code = vmc->code;
	int mcall = code == DVM_MCALL;
	int need_self = ctx->routine->routType->attrib & DAO_TYPE_SELF;
	int i = DRoutine_PassParams( rout, selfpar, ctx->regValues, params, npar, code );
	if( i ==0 ){
		DaoContext_RaiseException( self, DAO_ERROR_PARAM, "not matched (passing)" );
		return;
	}
	ctx->parCount = i - 1;
	if( need_self && ROUT_HOST_TID( rout ) == DAO_OBJECT ){
		DaoObject *obj;
		if( mcall && ctx->regValues[0]->type == DAO_OBJECT ){
			obj = ctx->regValues[0]->xObject.that;
			obj = (DaoObject*) DaoObject_MapThisObject( obj, rout->routHost );
			GC_ShiftRC( obj, ctx->object );
			ctx->object = obj;
		}else if( selfpar->type == DAO_OBJECT ){
			obj = selfpar->xObject.that;
			obj = (DaoObject*) DaoObject_MapThisObject( obj, rout->routHost );
			GC_ShiftRC( obj, ctx->object );
			ctx->object = obj;
		}else{
			/* Only class method that does not use object data
			 * will be allowed to be called without object
			 */
			DaoContext_RaiseException( self, DAO_ERROR, "need self object" );
			return;
		}
	}
}
static void DaoContext_DoCxxCall( DaoContext *self, DaoVmCode *vmc,
		DaoFunction *func, DaoValue *selfpar, DaoValue *params[], int npar )
{
	DaoValue *parbuf[DAO_MAX_PARAM+1];
	int i;
	memset( parbuf, 0, (DAO_MAX_PARAM+1)*sizeof(DaoValue*) );
	if( (self->vmSpace->options & DAO_EXEC_SAFE) && (func->attribs & DAO_ROUT_EXTFUNC) ){
		/* normally this condition will not be satisfied.
		 * it is possible only if the safe mode is set in C codes
		 * by embedding or extending. */
		DaoContext_RaiseException( self, DAO_ERROR, "not permitted" );
		return;
	}
	if( ! DRoutine_PassParams( (DRoutine*)func, selfpar, parbuf, params, npar, vmc->code ) ){
		for(i=0; i<=func->parCount; i++) GC_DecRC( parbuf[i] );
		//rout2 = (DRoutine*) rout;
		DaoContext_ShowCallError( self, (DRoutine*)func, selfpar, params, npar, vmc->code );
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
	if( vmc->code == DVM_MCALL && ! (func->attribs & DAO_ROUT_PARSELF)) npar --;
	/*
	   printf( "call: %s %i\n", rout->routName->mbs, npar );
	 */
	self->thisFunction = func;
	func->pFunc( self, parbuf, npar );
	self->thisFunction = NULL;
	/* DaoValue_ClearAll( parbuf, rout->parCount+1 ); */
	for(i=0; i<=func->parCount; i++) GC_DecRC( parbuf[i] );

	//XXX if( DaoContext_CheckFE( self ) ) return;
	if( self->process->status==DAO_VMPROC_SUSPENDED )
		self->process->topFrame->entry = (short)(vmc - self->codes);
}
static void DaoContext_DoNewCall( DaoContext *self, DaoVmCode *vmc,
		DaoClass *klass, DaoValue *selfpar, DaoValue *params[], int npar )
{
	DaoContext *ctx;
	DaoRoutine *rout;
	DaoFunctree *routines = klass->classRoutines;
	DaoObject *obj, *othis = NULL, *onew = NULL;
	int i, code = vmc->code;
	int mode = vmc->b & 0xff00;
	int codemode = code | (mode<<16);
	int initbase = DaoContext_InitBase( self, vmc, (DaoValue*) klass );
	if( initbase >= 0 ){
		othis = self->object;
	}else{
		othis = onew = DaoObject_New( klass, NULL, 0 );
	}
	rout = (DaoRoutine*)DRoutine_Resolve( (DaoValue*)routines, selfpar, params, npar, codemode );
	if( rout == NULL ){
		selfpar = (DaoValue*) othis;
		rout = (DaoRoutine*)DRoutine_Resolve( (DaoValue*)routines, selfpar, params, npar, codemode );
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
	if( rout->type == DAO_FUNCTION ){
		DaoFunction *func = (DaoFunction*) rout;
		DaoVmCode vmcode = { DVM_CALL, 0, 0, 0 };
		DaoValue *parbuf[DAO_MAX_PARAM+1];
		DaoValue *returned = NULL;

		memset( parbuf, 0, (DAO_MAX_PARAM+1)*sizeof(DaoValue*) );
		if( ! DRoutine_PassParams( (DRoutine*)rout, selfpar, parbuf, params, npar, vmc->code ) ){
			for(i=0; i<=rout->parCount; i++) GC_DecRC( parbuf[i] );
			if( onew ){ GC_IncRC( onew ); GC_DecRC( onew ); }
			//rout2 = (DRoutine*) rout;
			//XXX goto InvalidParameter;
			return;
		}
		ctx = DaoVmProcess_MakeContext( self->process, rout );
		ctx->vmc = & vmcode;
		ctx->regValues = & returned;
		ctx->regTypes = & rout->routHost;
		self->thisFunction = func;
		func->pFunc( ctx, parbuf, npar );
		self->thisFunction = NULL;
		ctx->regValues = NULL;
		/* DaoValue_ClearAll( parbuf, rout->parCount+1 ); */
		for(i=0; i<=rout->parCount; i++) GC_DecRC( parbuf[i] );

		if( returned && returned->type == DAO_CDATA ){
			DaoCData *cdata = & returned->xCdata;
			DaoObject_SetParentCData( othis, cdata );
			GC_ShiftRC( othis, cdata->daoObject );
			cdata->daoObject = othis;
		}
		DaoContext_PutValue( self, (DaoValue*) othis );
		GC_DecRC( returned );
		return;
	}else if( rout != NULL ){
		ctx = DaoVmProcess_MakeContext( self->process, rout );
		obj = othis;
		if( initbase >= 0 ){
			obj = (DaoObject*) DaoObject_MapThisObject( obj, rout->routHost );
			GC_ShiftRC( obj, ctx->object );
			ctx->object = obj;
		}else{
			GC_ShiftRC( obj, ctx->object );
			ctx->object = obj;
			ctx->ctxState = DVM_MAKE_OBJECT;
		}
		DaoContext_PrepareCall( self, vmc, ctx, selfpar, params, npar );
		DaoVmProcess_PushContext( self->process, ctx );
	}else{
		if( onew ){ GC_IncRC( onew ); GC_DecRC( onew ); }
		DaoContext_RaiseException( self, DAO_ERROR_PARAM, "not matched (class)" );
	}
}
void DaoContext_DoCall2( DaoContext *self, DaoVmCode *vmc )
{
	DRoutine *rout = NULL;
	DaoValue *selfpar = NULL;
	DaoValue *parbuf[DAO_MAX_PARAM+1];
	DaoValue **params = self->regValues + vmc->a + 1;
	DaoValue *caller = self->regValues[ vmc->a ];
	int mcall = vmc->code == DVM_MCALL;
	int mode = vmc->b & 0xff00;
	int npar = vmc->b & 0xff;
	int i, n = 0;

	if( npar == 0 && (mode & DAO_CALL_EXPAR) ){ /* call with caller's parameter */
		int m = (self->routine->routType->attrib & DAO_TYPE_SELF) != 0;
		npar = self->parCount - m;
		params = self->regValues + m;
		mode &= ~DAO_CALL_EXPAR;
	}
	if( self->object && mcall == 0 ) selfpar = (DaoValue*) self->object;
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
			for(i=0; i<tup->items->size; i++) parbuf[n++] = tup->items->items.pValue[i];
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
		DaoContext *ctx = DaoVmProcess_MakeContext( self->process, (DaoRoutine*)rout );
		DaoContext_PrepareCall( self, vmc, ctx, selfpar, params, npar );
		if( DaoContext_TrySynCall( self, vmc, ctx ) ) return;
		DaoContext_TryTailCall( self, vmc, ctx );
		DaoVmProcess_PushContext( self->process, ctx );
	}else if( rout->type == DAO_FUNCTION ){
		DaoFunction *func = (DaoFunction*) rout;
		DaoContext_DoCxxCall( self, vmc, func, selfpar, params, npar );
	}else{
		DaoContext_RaiseException( self, DAO_ERROR_TYPE, "object not callable" );
	}
	return;
InvalidParameter:
	DaoContext_ShowCallError( self, (DRoutine*)caller, selfpar, params, npar, DVM_CALL );
}
void DaoContext_DoCall( DaoContext *self, DaoVmCode *vmc )
{
	int sup = 0, code = vmc->code;
	int mcall = code == DVM_MCALL;
	int mode = vmc->b & 0xff00;
	int npar = vmc->b & 0xff;
	int codemode = code | (mode<<16);
	DaoValue *selfpar = NULL;
	DaoValue *caller = self->regValues[ vmc->a ];
	DaoValue **params = self->regValues + vmc->a + 1;
	DRoutine *rout, *rout2 = NULL;
	DaoFunctree *mroutine;
	DaoFunction *func;
	DaoContext *ctx;

	self->vmc = vmc;
	if( caller->type ==0 ){
		DaoContext_RaiseException( self, DAO_ERROR_TYPE, "null object not callable" );
		return;
	}
	if( self->object && mcall == 0 ) selfpar = (DaoValue*) self->object;
	if( mode & DAO_CALL_COROUT ){
		DaoVmProcess *vmp = DaoVmProcess_Create( self, self->regValues + vmc->a, npar+1 );
		if( vmp == NULL ) return;
		GC_ShiftRC( self->regTypes[ vmc->c ], vmp->abtype );
		vmp->abtype = self->regTypes[ vmc->c ];
		DaoContext_PutValue( self, (DaoValue*) vmp );
	}else if( caller->type == DAO_FUNCURRY || (mode & DAO_CALL_EXPAR) ){
		DaoContext_DoCall2( self, vmc );
	}else if( caller->type == DAO_FUNCTION ){
		DaoContext_DoCxxCall( self, vmc, & caller->xFunction, selfpar, params, npar );
	}else if( caller->type == DAO_ROUTINE ){
		rout = DRoutine_Resolve( caller, selfpar, params, npar, codemode );
		if( rout == NULL ) goto InvalidParameter;
		if( rout->routName->mbs[0] == '@' ){
			DaoRoutine *drout = (DaoRoutine*) rout;
			drout = DaoRoutine_Decorate( & params[0]->xRoutine, drout, params+1, npar-1 );
			DaoContext_PutValue( self, (DaoValue*) drout );
			return;
		}
		ctx = DaoVmProcess_MakeContext( self->process, (DaoRoutine*)rout );
		DaoContext_PrepareCall( self, vmc, ctx, selfpar, params, npar );
		if( DaoContext_TrySynCall( self, vmc, ctx ) ) return;
		DaoContext_TryTailCall( self, vmc, ctx );
		DaoVmProcess_PushContext( self->process, ctx );
	}else if( caller->type == DAO_FUNCTREE ){
		rout = DRoutine_Resolve( caller, selfpar, params, npar, codemode );
		if( rout == NULL ){
			rout2 = (DRoutine*) caller;
			goto InvalidParameter;
		}
		if( rout->type == DAO_ROUTINE ){
			if( rout->routName->mbs[0] == '@' ){
				DaoRoutine *drout = (DaoRoutine*) rout;
				drout = DaoRoutine_Decorate( & params[0]->xRoutine, drout, params+1, npar-1 );
				DaoContext_PutValue( self, (DaoValue*) drout );
				return;
			}
			ctx = DaoVmProcess_MakeContext( self->process, (DaoRoutine*)rout );
			DaoContext_PrepareCall( self, vmc, ctx, selfpar, params, npar );
			if( DaoContext_TrySynCall( self, vmc, ctx ) ) return;
			DaoContext_TryTailCall( self, vmc, ctx );
			DaoVmProcess_PushContext( self->process, ctx );
		}else if( rout->type == DAO_FUNCTION ){
			func = (DaoFunction*) rout;
			DaoContext_DoCxxCall( self, vmc, func, selfpar, params, npar );
		}else{
			DaoContext_RaiseException( self, DAO_ERROR_TYPE, "object not callable" );
		}
	}else if( caller->type == DAO_CLASS ){
		DaoContext_DoNewCall( self, vmc, & caller->xClass, selfpar, params, npar );
	}else if( caller->type == DAO_OBJECT ){
		DaoClass *host = self->object ? self->object->myClass : NULL;
		rout = (DRoutine*) DaoClass_FindOperator( caller->xObject.myClass, "()", host );
		if( rout == NULL ){
			DaoContext_RaiseException( self, DAO_ERROR_TYPE, "class instance not callable" );
			return;
		}
		rout = DRoutine_Resolve( (DaoValue*)rout, caller, params, npar, codemode );
		if( rout == NULL ){
			return; //XXX
		}
		if( rout->type == DAO_ROUTINE ){
			ctx = DaoVmProcess_MakeContext( self->process, (DaoRoutine*) rout );
			GC_ShiftRC( caller, ctx->object );
			ctx->object = & caller->xObject;
			DaoContext_PrepareCall( self, vmc, ctx, caller, params, npar );
			DaoVmProcess_PushContext( self->process, ctx );
		}else if( rout->type == DAO_FUNCTION ){
			func = (DaoFunction*) rout;
			DaoContext_DoCxxCall( self, vmc, func, caller, params, npar );
		}
	}else if( caller->type == DAO_CTYPE ){
		DaoTypeBase *typer = caller->xCdata.typer;
		rout = (DRoutine*) DaoFindFunction( typer, typer->priv->abtype->name );
		if( rout == NULL ){
			DaoContext_RaiseException( self, DAO_ERROR_TYPE, "C type not callable" );
			return;
		}
		rout = DRoutine_Resolve( (DaoValue*)rout, selfpar, params, npar, codemode );
		if( rout == NULL || rout->type != DAO_FUNCTION ){
			// XXX
			return;
		}
		DaoContext_DoCxxCall( self, vmc, (DaoFunction*) rout, selfpar, params, npar );
		// XXX handle fail
		sup = DaoContext_InitBase( self, vmc, caller );
		if( caller->type == DAO_CTYPE && sup >= 0 ){
			DaoCData *cdata = & self->regValues[ vmc->c ]->xCdata;
			if( cdata && cdata->type == DAO_CDATA ){
				GC_ShiftRC( cdata, self->object->superObject->items.pValue[sup] );
				self->object->superObject->items.pValue[sup] = (DaoValue*) cdata;
				GC_ShiftRC( self->object->that, cdata->daoObject );
				cdata->daoObject = self->object->that;
			}
		}
	}else if( caller->type == DAO_CDATA ){
		DaoTypeBase *typer = caller->xCdata.typer;
		rout = (DRoutine*)DaoFindFunction2( typer, "()" );
		if( rout == NULL ){
			DaoContext_RaiseException( self, DAO_ERROR_TYPE, "C object not callable" );
			return;
		}
		rout = DRoutine_Resolve( (DaoValue*)rout, selfpar, params, npar, codemode );
		if( rout == NULL || rout->type != DAO_FUNCTION ){
			// XXX
			return;
		}
		DaoContext_DoCxxCall( self, vmc, (DaoFunction*) rout, selfpar, params, npar );
	}else if( caller->type == DAO_VMPROCESS && caller->xProcess.abtype ){
		DaoVmProcess *vmProc = & caller->xProcess;
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
	DaoContext_ShowCallError( self, rout2, selfpar, params, npar, code );
}
void DaoContext_DoReturn( DaoContext *self, DaoVmCode *vmc )
{
	int i;
	DaoVmFrame *topFrame = self->process->topFrame;
	self->vmc = vmc;
	//XXX if( DaoContext_CheckFE( self ) ) return;
	if( vmc->c == 0 && self->caller && topFrame->returning != (ushort_t)-1 ){
		int regReturn = topFrame->returning;
		if( self->ctxState & DVM_MAKE_OBJECT ){
			DaoContext_SetValue( self->caller, regReturn, (DaoValue*)self->object );
		}else if( vmc->b == 1 ){
			DaoContext_SetValue( self->caller, regReturn, self->regValues[ vmc->a ] );
		}else if( vmc->b > 1 ){
			DaoTuple *tuple = DaoTuple_New( vmc->b );
			DaoValue **items = tuple->items->items.pValue;
			for(i=0; i<vmc->b; i++) DaoValue_Copy( self->regValues[ vmc->a+i ], items + i );
			DaoContext_SetValue( self->caller, regReturn, (DaoValue*) tuple );
		}else if( ! ( self->process->topFrame->state & DVM_SPEC_RUN ) ){
			/* XXX DaoContext_SetValue( self->caller, regReturn, daoNullValue ); */
		}
	}else if( self->process->parYield ){
		DaoVmProcess_Yield( self->process, self->regValues + vmc->a, vmc->b, NULL );
		/* self->process->status is set to DAO_VMPROC_SUSPENDED by DaoVmProcess_Yield() */
		self->process->status = DAO_VMPROC_FINISHED;
	}else{
		if( vmc->b == 1 ){
			DaoValue_Move( self->regValues[ vmc->a ], & self->process->returned, NULL );
		}else if( vmc->b > 1 ){
			DaoTuple *tuple = DaoTuple_New( vmc->b );
			DaoValue **items = tuple->items->items.pValue;
			DaoValue_Clear( & self->process->returned );
			GC_IncRC( tuple );
			self->process->returned = (DaoValue*) tuple;
			for(i=0; i<vmc->b; i++) DaoValue_Copy( self->regValues[ vmc->a+i ], items + i );
		}
	}
}
int DaoRoutine_SetVmCodes2( DaoRoutine *self, DaoVmcArray *vmCodes );
int DaoRoutine_InferTypes( DaoRoutine *self );
void DaoRoutine_CopyFields( DaoRoutine *self, DaoRoutine *other );
void DaoValue_Update( DaoValue **self, DaoNameSpace *ns, DMap *deftypes );

static void DaoContext_MapTypes( DaoContext *self, DMap *deftypes )
{
	DaoRoutine *routine = self->routine;
	DNode *it = DMap_First(routine->localVarType);
	for(; it; it = DMap_Next(routine->localVarType,it) ){
		DaoValue *V = self->regValues[ it->key.pInt ];
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
		DaoValue_Update( & self->routConsts->items.pValue[i], self->nameSpace, deftypes );
	}
}
int DaoRoutine_Finalize( DaoRoutine *self, DaoClass *klass, DMap *deftypes )
{
	DaoType *tp = DaoType_DefineTypes( self->routType, self->nameSpace, deftypes );
	if( tp == NULL ) return 0;
	GC_ShiftRC( tp, self->routType );
	self->routType = tp;
	if( klass ){
		GC_ShiftRC( klass->objType, self->routHost );
		self->routHost = klass->objType;
	}
	if( self->type != DAO_ROUTINE ) return 1;
	DaoRoutine_MapTypes( self, deftypes );
	return DaoRoutine_InferTypes( self );
	/*
	DaoRoutine_PrintCode( self, self->nameSpace->vmSpace->stdStream );
	*/
}

void DaoContext_MakeRoutine( DaoContext *self, DaoVmCode *vmc )
{
	DMap *deftypes;
	DaoValue **pp = self->regValues + vmc->a;
	DaoValue **pp2;
	DaoType *tp;
	DaoRoutine *closure;
	DaoRoutine *proto = & pp[0]->xRoutine;
	int i;
	if( proto->vmCodes->size ==0 && proto->annotCodes->size ){
		if( DaoRoutine_SetVmCodes( proto, proto->annotCodes ) ==0 ){
			DaoContext_RaiseException( self, DAO_ERROR, "invalid closure" );
			return;
		}
	}

	closure = DaoRoutine_Copy( proto );
	if( proto->upRoutine ){
		closure->upRoutine = self->routine;
		closure->upContext = self;
		GC_IncRC( self );
		GC_IncRC( self->routine );
	}
	pp2 = closure->routConsts->items.pValue;
	for(i=0; i<vmc->b; i+=2) DaoValue_Copy( pp[i+1], pp2 + pp[i+2]->xInteger.value );
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
	if( DaoRoutine_SetVmCodes2( closure, proto->vmCodes ) ==0 ){
		DaoContext_RaiseException( self, DAO_ERROR, "function creation failed" );
	}
	DaoContext_SetValue( self, vmc->c, (DaoValue*) closure );
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
	DaoNameSpace *ns2 = self->nameSpace;
	DaoTuple *tuple = & self->regValues[vmc->a]->xTuple;
	DaoClass *klass = DaoClass_New();
	DaoClass *proto = NULL;
	DaoList *parents = NULL;
	DaoMap *parents2 = NULL;
	DaoList *fields = NULL;
	DaoList *methods = NULL;
	DString *name = NULL;
	DTuple *items = tuple->items;
	DaoValue **data = tuple->items->items.pValue;
	DMap *keys = tuple->unitype->mapNames;
	DMap *deftypes = DMap_New(0,0);
	DMap *pm_map = DMap_New(D_STRING,0);
	DMap *st_map = DMap_New(D_STRING,0);
	DMap *protoValues = NULL;
	DNode *it, *node;
	DaoEnum pmEnum = {DAO_ENUM,0,1,0,{0,0},0,0,NULL,0};
	DaoEnum stEnum = {DAO_ENUM,0,1,0,{0,0},0,0,NULL,0};
	int i, st, pm, up, id, size;
	char buf[50];

	pmEnum.etype = dao_access_enum;
	stEnum.etype = dao_storage_enum;

	DaoContext_SetValue( self, vmc->c, (DaoValue*) klass );
	//printf( "%s\n", tuple->unitype->name->mbs );
	if( vmc->b && routine->routConsts->items.pValue[vmc->b-1]->type == DAO_CLASS ){
		proto = & routine->routConsts->items.pValue[vmc->b-1]->xClass;
		protoValues = proto->protoValues;
		ns2 = proto->classRoutine->nameSpace;
	}

	/* extract parameters */
	it = MAP_Find( keys, "name" );
	if( it && data[it->value.pInt]->type == DAO_STRING ) name = data[it->value.pInt]->xString.data;
	it = MAP_Find( keys, "parents" );
	if( it && data[it->value.pInt]->type == DAO_LIST ) parents = & data[it->value.pInt]->xList;
	if( it && data[it->value.pInt]->type == DAO_MAP ) parents2 = & data[it->value.pInt]->xMap;
	it = MAP_Find( keys, "fields" );
	if( it && data[it->value.pInt]->type == DAO_LIST ) fields = & data[it->value.pInt]->xList;
	it = MAP_Find( keys, "methods" );
	if( it && data[it->value.pInt]->type == DAO_LIST ) methods = & data[it->value.pInt]->xList;

	if( name ==NULL && items->size && data[0]->type == DAO_STRING ) name = data[0]->xString.data;
	if( parents ==NULL && parents2 == NULL && items->size >1 ){
		if( data[1]->type == DAO_LIST ) parents = & data[1]->xList;
		if( data[1]->type == DAO_MAP ) parents2 = & data[1]->xMap;
	}
	if( fields ==NULL && items->size >2 && data[2]->type == DAO_LIST ) fields = & data[2]->xList;
	if( methods ==NULL && items->size >3 && data[3]->type == DAO_LIST ) methods = & data[3]->xList;

	if( name == NULL || name->size ==0 ){
		sprintf( buf, "AnonymousClass%p", klass );
		DString_SetMBS( klass->className, buf );
		DaoClass_SetName( klass, klass->className, ns2 );
	}else{
		DaoClass_SetName( klass, name, ns2 );
	}
	tp = DaoNameSpace_MakeType( ns, "@class", DAO_INITYPE, NULL,NULL,0 );
	if( tp ) MAP_Insert( deftypes, tp, klass->objType );
	DaoContext_MapTypes( self, deftypes );

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
		value = self->regValues[it->key.pInt];
		if( st == DAO_CLASS_CONSTANT ){
			DaoRoutine *newRout = NULL;
			DaoValue **dest2 = klass->cstData->items.pValue + id;
			DaoValue *dest = *dest2;
			if( value->type == DAO_ROUTINE && value->xRoutine.routHost == proto->objType ){
				newRout = & value->xRoutine;
				if( DaoRoutine_Finalize( newRout, klass, deftypes ) == 0){
					DaoContext_RaiseException( self, DAO_ERROR, "method creation failed" );
					continue;
				}
				if( strcmp( newRout->routName->mbs, "@class" ) ==0 ){
					node = DMap_Find( proto->lookupTable, newRout->routName );
					DString_Assign( newRout->routName, klass->className );
					st = LOOKUP_ST( node->value.pSize );
					up = LOOKUP_UP( node->value.pSize );
					if( st == DAO_CLASS_CONSTANT && up ==0 ){
						id = LOOKUP_ID( node->value.pSize );
						dest2 = klass->cstData->items.pValue + id;
					}
					DaoFunctree_Add( klass->classRoutines, (DRoutine*)newRout );
				}
			}
			dest = *dest2;
			if( dest->type == DAO_ROUTINE ){
				DaoRoutine *rout = & dest->xRoutine;
				if( rout->routHost != klass->objType ) DaoValue_Clear( dest2 );
			}
			if( dest->type == DAO_FUNCTREE ){
				DaoFunctree_Add( & dest->xFunctree, (DRoutine*)newRout );
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
		for(i=0; i<parents->items->size; i++){
			DaoValue *item = parents->items->items.pValue[i];
			if( item->type == DAO_CLASS ){
				DaoClass_AddSuperClass( klass, item, item->xClass.className );
			}
		}
	}else if( parents2 ){
		for(it=DMap_First(parents2->items);it;it=DMap_Next(parents2->items,it)){
			if( it->key.pValue->type == DAO_STRING && it->value.pValue->type == DAO_CLASS ){
				DaoClass_AddSuperClass( klass, it->value.pValue, it->key.pValue->xString.data );
			}
		}
	}
	DaoClass_DeriveClassData( klass );
	if( fields ){ /* add fields from parameters */
		for(i=0; i<fields->items->size; i++){
			DaoType *type = NULL;
			DaoTuple *field = NULL;
			DMap *keys = NULL;
			DaoValue **data = NULL;
			DaoValue *name = NULL;
			DaoValue *value = NULL;
			DaoValue *storage = NULL;
			DaoValue *access = NULL;
			int flags = 0;

			if( fields->items->items.pValue[i]->type != DAO_TUPLE ) continue;
			field = & fields->items->items.pValue[i]->xTuple;
			keys = field->unitype->mapNames;
			data = field->items->items.pValue;
			size = field->items->size;
			st = DAO_OBJECT_VARIABLE;
			pm = DAO_DATA_PUBLIC;

			if( size < 2 || size > 4 ) goto InvalidField;

			id = (it = MAP_Find( keys, "name" )) ? it->value.pInt : -1;
			if( id >=0 && data[id]->type == DAO_STRING ){
				flags |= 1<<id;
				name = data[id];
			}
			id = (it = MAP_Find( keys, "value" )) ? it->value.pInt : -1;
			if( id >=0 && data[id]->type ){
				flags |= 1<<id;
				value = data[id];
				type = field->unitype->nested->items.pType[id];
			}
			id = (it = MAP_Find( keys, "storage" )) ? it->value.pInt : -1;
			if( id >=0 && data[id]->type == DAO_ENUM ){
				flags |= 1<<id;
				storage = data[id];
			}
			id = (it = MAP_Find( keys, "access" )) ? it->value.pInt : -1;
			if( id >=0 && data[id]->type == DAO_ENUM ){
				flags |= 1<<id;
				access = data[id];
			}

			if( name ==NULL && size && data[0]->type == DAO_STRING && !(flags & 1) ){
				flags |= 1;
				name = data[0];
			}
			if( value ==NULL && size >1 && data[1]->type && !(flags & 2) ){
				flags |= 2;
				value = data[1];
				type = field->unitype->nested->items.pType[1];
			}
			if( storage ==NULL && size >2 && data[2]->type == DAO_ENUM && !(flags&4) ){
				flags |= 4;
				storage = data[2];
			}
			if( access ==NULL && size >3 && data[3]->type == DAO_ENUM && !(flags&8) ){
				flags |= 8;
				access = data[3];
			}

			if( flags != ((1<<size)-1) ) goto InvalidField;
			if( name == NULL || value == NULL ) continue;
			if( MAP_Find( klass->lookupTable, name->xString.data ) ) continue;
			if( storage ){
				if( DaoEnum_SetValue( & stEnum, & storage->xEnum, NULL ) ==0) goto InvalidField;
				st = storages[ stEnum.value ];
			}
			if( access ){
				if( DaoEnum_SetValue( & pmEnum, & access->xEnum, NULL ) ==0) goto InvalidField;
				pm = permissions[ pmEnum.value ];
			}
			/* printf( "%s %i %i\n", name->xString.data->mbs, st, pm ); */
			switch( st ){
			case DAO_OBJECT_VARIABLE :
				DaoClass_AddObjectVar( klass, name->xString.data, value, type, pm, 0 );
				break;
			case DAO_CLASS_VARIABLE :
				DaoClass_AddGlobalVar( klass, name->xString.data, value, type, pm, 0 );
				break;
			case DAO_CLASS_CONSTANT :
				DaoClass_AddConst( klass, name->xString.data, value, pm, 0 );
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
			DaoValue **data = NULL;
			DaoValue *name = NULL;
			DaoValue *method = NULL;
			DaoValue *access = NULL;
			DaoValue *dest;
			int flags = 0;

			if( methods->items->items.pValue[i]->type != DAO_TUPLE ) continue;
			tuple = & methods->items->items.pValue[i]->xTuple;
			data = tuple->items->items.pValue;
			size = tuple->items->size;
			pm = DAO_DATA_PUBLIC;

			id = (it = MAP_Find( keys, "name" )) ? it->value.pInt : -1;
			if( id >=0 && data[id]->type == DAO_ENUM ){
				flags |= 1<<id;
				name = data[id];
			}
			id = (it = MAP_Find( keys, "method" )) ? it->value.pInt : -1;
			if( id >=0 && data[id]->type == DAO_ENUM ){
				flags |= 1<<id;
				method = data[id];
			}
			id = (it = MAP_Find( keys, "access" )) ? it->value.pInt : -1;
			if( id >=0 && data[id]->type == DAO_ENUM ){
				flags |= 1<<id;
				access = data[id];
			}

			if( name ==NULL && size && data[0]->type == DAO_STRING && !(flags&1) ){
				flags |= 1;
				name = data[0];
			}
			if( method ==NULL && size >1 && data[1]->type == DAO_ROUTINE && !(flags&2) ){
				flags |= 2;
				method = data[1];
			}
			if( access ==NULL && size >2 && data[2]->type == DAO_ENUM && !(flags&4) ){
				flags |= 4;
				access = data[2];
			}

			if( flags != ((1<<size)-1) ) goto InvalidMethod;
			if( name == NULL || method == NULL ) continue;
			if( access ){
				if( DaoEnum_SetValue( & pmEnum, & access->xEnum, NULL ) ==0) goto InvalidMethod;
				pm = permissions[ pmEnum.value ];
			}

			newRout = & method->xRoutine;
			if( ROUT_HOST_TID( newRout ) !=0 ) continue;
			if( DaoRoutine_Finalize( newRout, klass, deftypes ) == 0){
				DaoContext_RaiseException( self, DAO_ERROR, "method creation failed" );
				continue;
			}
			DString_Assign( newRout->routName, name->xString.data );
			if( DString_EQ( newRout->routName, klass->className ) ){
				DaoFunctree_Add( klass->classRoutines, (DRoutine*)newRout );
			}

			node = DMap_Find( proto->lookupTable, name->xString.data );
			if( node == NULL ){
				DaoClass_AddConst( klass, name->xString.data, method, pm, 0 );
				continue;
			}
			if( LOOKUP_UP( node->value.pSize ) ) continue;
			if( LOOKUP_ST( node->value.pSize ) != DAO_CLASS_CONSTANT ) continue;
			id = LOOKUP_ID( node->value.pSize );
			dest = klass->cstData->items.pValue[id];
			if( dest->type == DAO_FUNCTREE ){
				DaoFunctree_Add( & dest->xFunctree, (DRoutine*)newRout );
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
	DaoList *list = & self->nameSpace->varData->items.pValue[DVR_NSV_EXCEPTIONS]->xList;
	DaoList_Clear( list );
	self->vmc = vmc;
	if( DaoContext_CheckFE( self ) ) return 1;
	return ( self->process->exceptions->size > 0 );
}
static void DaoInitException( DaoCData *except, DaoContext *ctx, DaoVmCode *vmc, int fe, const char *value )
{
	DaoRoutine *rout = ctx->routine;
	DaoTypeBase *efloat = DaoException_GetType( DAO_ERROR_FLOAT );
	DaoException *exdat = (DaoException*) except->data;
	DaoVmCodeX **annotCodes = rout->annotCodes->items.pVmc;
	DaoVmFrame *frame = ctx->frame->prev;
	int line, line2;
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
		DaoValue_Clear( & exdat->data );
		exdat->data = DaoValue_NewMBString( value, 0 );
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
extern void STD_Debug( DaoContext *ctx, DaoValue *p[], int N );
void DaoContext_DoRaiseExcept( DaoContext *self, DaoVmCode *vmc )
{
	DaoStream *stdio = self->vmSpace->stdStream;
	DaoCData *cdata = NULL;
	DaoTypeBase *except = & dao_Exception_Typer;
	DaoTypeBase *warning = DaoException_GetType( DAO_WARNING );
	DaoList *list = & self->nameSpace->varData->items.pValue[DVR_NSV_EXCEPTIONS]->xList;
	DaoValue **excepts = self->regValues + vmc->a;
	DaoValue *val;
	ushort_t i, line = 0, line2 = 0;
	ushort_t N = vmc->b -1;
	line2 = line;
	if( N == 0 && list->items->size >0 ){
		N = list->items->size;
		excepts = list->items->items.pValue;
	}
	for(i=0; i<N; i++){
		val = excepts[i];
		if( val->type == DAO_OBJECT || val->type == DAO_CDATA ){
			cdata = NULL;
			if( val->type == DAO_OBJECT ){
				DaoType *type = except->priv->abtype;
				cdata = (DaoCData*) DaoObject_MapThisObject( & val->xObject, type );
			}else{
				if( DaoCData_ChildOf( val->xCdata.typer, except ) ) cdata = & val->xCdata;
			}
			if( cdata == NULL || cdata->data == NULL ) goto InvalidException;
			DaoInitException( cdata, self, vmc, self->idClearFE, NULL );
			if( DaoCData_ChildOf( cdata->typer, warning ) ){
				DaoPrintException( cdata, stdio );
			}else{
				DArray_Append( self->process->exceptions, val );
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
	DaoList *list = & self->nameSpace->varData->items.pValue[DVR_NSV_EXCEPTIONS]->xList;
	DaoTypeBase *ext = & dao_Exception_Typer;
	DaoTypeBase *any = DaoException_GetType( DAO_EXCEPTION_ANY );
	DaoTypeBase *none = DaoException_GetType( DAO_EXCEPTION_NONE );
	DaoValue **excepts = self->regValues + vmc->a;
	DaoValue *val, *val2;
	DaoCData *cdata;
	ushort_t i, j;
	ushort_t N = vmc->b -1;
	int canRescue = 0;
	int M = self->process->exceptions->size;
	DaoList_Clear( list );
	self->vmc = vmc;
	if( DaoContext_CheckFE( self ) ) M = self->process->exceptions->size;
	if( N ==0 && M >0 ){ /* rescue without exception list */
		DArray_Swap( self->process->exceptions, list->items );
		return 1;
	}
	for(i=0; i<N; i++){
		val = excepts[i];
		if( val->type == DAO_CLASS || val->type == DAO_CTYPE ){
			cdata = & val->xCdata;
			if( val->type == DAO_CLASS ){
				DaoType *type = ext->priv->abtype;
				cdata = (DaoCData*) DaoClass_MapToParent( & val->xClass, type );
			}
			if( cdata && DaoCData_ChildOf( cdata->typer, any ) ){
				DArray_Swap( self->process->exceptions, list->items );
				return 1;
			}else if( cdata && DaoCData_ChildOf( cdata->typer, none ) && M ==0 ){
				return 1;
			}else if( cdata ){
				for(j=0; j<self->process->exceptions->size; j++){
					val2 = self->process->exceptions->items.pValue[j];
					if( val->type == DAO_CLASS && val2->type == DAO_OBJECT ){
						if( DaoClass_ChildOf( val2->xObject.myClass, val ) ){
							canRescue = 1;
							DArray_Append( list->items, val2 );
							DArray_Erase( self->process->exceptions, j, 1 );
						}
					}else if( val->type == DAO_CTYPE ){
						DaoCData *cdata2 = & val2->xCdata;
						if( val2->type == DAO_CLASS ){
							DaoType *type = ext->priv->abtype;
							cdata2 = (DaoCData*) DaoClass_MapToParent( & val2->xClass, type );
						}
						if( DaoCData_ChildOf( cdata2->typer, cdata->typer ) ){
							canRescue = 1;
							DArray_Append( list->items, val2 );
							DArray_Erase( self->process->exceptions, j, 1 );
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
	DaoInitException( cdata, self, self->vmc, self->idClearFE, value );
	DArray_Append( self->process->exceptions, (DaoValue*) cdata );
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
