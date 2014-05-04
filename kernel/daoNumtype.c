/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2014, Limin Fu
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED  BY THE COPYRIGHT HOLDERS AND  CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED  WARRANTIES,  INCLUDING,  BUT NOT LIMITED TO,  THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL  THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE FOR ANY DIRECT,
// INDIRECT,  INCIDENTAL, SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL  DAMAGES (INCLUDING,
// BUT NOT LIMITED TO,  PROCUREMENT OF  SUBSTITUTE  GOODS OR  SERVICES;  LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY OF
// LIABILITY,  WHETHER IN CONTRACT,  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include"daoStream.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoVmspace.h"
#include"daoRoutine.h"
#include"daoNumtype.h"
#include"daoValue.h"
#include"assert.h"
#include"math.h"
#include"stdlib.h"
#include"string.h"
#include"ctype.h"

static void DaoComplex_GetField( DaoValue *self, DaoProcess *proc, DString *name )
{
	if( strcmp( name->chars, "real" ) == 0 ){
		DaoProcess_PutDouble( proc, self->xComplex.value.real );
	}else if( strcmp( name->chars, "imag" ) == 0 ){
		DaoProcess_PutDouble( proc, self->xComplex.value.imag );
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, name->chars );
	}
}
static void DaoComplex_SetField( DaoValue *self, DaoProcess *proc, DString *name, DaoValue *value )
{
	if( strcmp( name->chars, "real" ) == 0 ){
		self->xComplex.value.real = DaoValue_GetDouble( value );
	}else if( strcmp( name->chars, "imag" ) == 0 ){
		self->xComplex.value.imag = DaoValue_GetDouble( value );
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, name->chars );
	}
}
static void DaoComplex_Print( DaoValue *self, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	complex16 p = self->xComplex.value;
	DaoStream_WriteFloat( stream, p.real );
	if( p.imag >= -0.0 ) DaoStream_WriteChars( stream, "+" );
	DaoStream_WriteFloat( stream, p.imag );
	DaoStream_WriteChars( stream, "$" );
}
static DaoTypeCore comCore =
{
	NULL,
	DaoComplex_GetField,
	DaoComplex_SetField,
	DaoValue_GetItem,
	DaoValue_SetItem,
	DaoComplex_Print
};

DaoTypeBase comTyper =
{
	"complex", & comCore, NULL, NULL, {0}, {0}, NULL, NULL
};

double abs_c( const complex16 com )
{
	return sqrt( com.real*com.real + com.imag*com.imag );
}
double arg_c( const complex16 com )
{
	if( com.real == 0 ){
		if( com.imag >= 0 ) return asin( 1 );
		return asin( -1 );
	}
	return atan( com.imag / com.real );
}
double norm_c( const complex16 com )
{
	return com.real*com.real + com.imag*com.imag;
}

complex16 cos_c( const complex16 com )
{
	complex16 res = { 0, 0 };
	res.real = cos(com.real) * cosh(com.imag);
	res.imag = -sin(com.real) * cosh(com.imag);
	return res;
}
complex16 cosh_c( const complex16 com )
{
	complex16 res = { 0, 0 };
	res.real = cosh(com.real) * cos(com.imag);
	res.imag = sinh(com.real) * cos(com.imag);
	return res;
}

/* exp( log( p1 ) * p2 ) */
complex16 exp_c( const complex16 com )
{
	double r = exp( com.real );
	complex16 c = { 0, 0 };
	c.real = r*cos(com.imag);
	c.imag = r*sin(com.imag);
	return c;
}
complex16 log_c( const complex16 com )
{
	complex16 c = { 0, 0, };
	c.real = log( sqrt( com.real * com.real + com.imag * com.imag ) );
	c.imag = arg_c( com );
	return c;
}
complex16 sin_c( const complex16 com )
{
	complex16 res = { 0, 0, };
	res.real = sin(com.real) * cosh(com.imag);
	res.imag = cos(com.real) * sinh(com.imag);
	return res;
}
complex16 sinh_c( const complex16 com )
{
	complex16 res = { 0, 0 };
	res.real = sinh(com.real) * cos(com.imag);
	res.imag = cosh(com.real) * sin(com.imag);
	return res;
}
complex16 sqrt_c( const complex16 com )
{
	double r = sqrt( abs_c( com ) );
	double phi = arg_c( com )*0.5;
	complex16 res = { 0, 0, };
	res.real = r*cos(phi);
	res.imag = r*sin(phi);
	return res;
}
complex16 tan_c( const complex16 com )
{
	complex16 res = { 0, 0, };
	complex16 R = sin_c( com );
	complex16 L = cos_c( com );
	res.real = ( L.real*R.real + L.imag*R.imag ) / ( R.real*R.real + R.imag*R.imag );
	res.imag = ( L.imag*R.real - L.real*R.imag ) / ( R.real*R.real + R.imag*R.imag );
	return res;
}
complex16 tanh_c( const complex16 com )
{
	complex16 res = { 0, 0, };
	complex16 R = sinh_c( com );
	complex16 L = cosh_c( com );
	res.real = ( L.real*R.real + L.imag*R.imag ) / ( R.real*R.real + R.imag*R.imag );
	res.imag = ( L.imag*R.real - L.real*R.imag ) / ( R.real*R.real + R.imag*R.imag );
	return res;
}
complex16 ceil_c( const complex16 com )
{
	complex16 res = { 0, 0, };
	res.real = ceil( com.real );
	res.imag = ceil( com.imag );
	return res;
}
complex16 floor_c( const complex16 com )
{
	complex16 res = { 0, 0, };
	res.real = floor( com.real );
	res.imag = floor( com.imag );
	return res;
}



#ifdef DAO_WITH_NUMARRAY

static void Array_SetAccum( daoint *dim, daoint *accum, int N )
{
	int i;
	accum[N-1] = 1;
	for(i=N-2; i>=0; i--) accum[i] = accum[i+1] * dim[i+1];
}
static void Array_FlatIndex2Mult( daoint *dim, int N, int sd, daoint *md )
{
	int i;
	for(i=N-1; i>=0; i--){
		md[i] = sd % dim[i];
		sd = sd / dim[i];
	}
}
static void Array_MultIndex2Flat( daoint *accum, int N, daoint *sd, daoint *md )
{
	int i;
	*sd = 0;
	for(i=N-1; i>=0; i--) *sd += md[i] * accum[i];
}
/*
// in-situ index permutation: array.permute( [ 2, 0, 1 ] );
// arrange the dim-0 as the dim-2 in the permuted array...
// see also: share/matrix_transpose.dao
*/
static int DaoArray_Permute( DaoArray *self, DArray *perm )
{
	daoint i, j, k, m, N = self->size, D = perm->size;
	daoint *dm, *ac, *mdo, *mdn, *pm = perm->items.pInt;
	DArray *dim = DArray_New(0); /* new dimension vector */
	DArray *acc = DArray_New(0); /* new accumulate vector */
	DArray *mido = DArray_New(0); /* old multiple indices */
	DArray *midn = DArray_New(0); /* new multiple indices */
	daoint ival = 0;
	float fval = 0;
	double dval = 0;
	complex16  c16val = {0,0};

	if( D != self->ndim ) return 0;
	DArray_Resize( dim, D, (void*)1 );
	DArray_Resize( acc, D, (void*)1 );
	DArray_Resize( mido, D, 0 );
	DArray_Resize( midn, D, 0 );
	dm = dim->items.pInt;
	ac = acc->items.pInt;
	mdo = mido->items.pInt;
	mdn = midn->items.pInt;
	for(i=0; i<D; i++) dm[ pm[i] ] = 0;
	for(i=0; i<D; i++){
		if( dm[i] ){
			DArray_Delete( dim );
			return 0;
		}
		dm[i] = self->dims[ pm[i] ];
	}
	Array_SetAccum( dm, ac, D );
	for(i=0; i<N; i++){
		daoint min = i;
		k = i;
		while(1){
			Array_FlatIndex2Mult( dm, D, k, mdn );
			for(j=0; j<D; j++) mdo[j] = mdn[ pm[j] ];
			Array_MultIndex2Flat( self->dims + self->ndim, D, & k, mdo );
			/* printf( "i = %i; k = %i\n", i, k ); */
			if( k <= min ){
				min = k;
				break;
			}
		}
		/* printf( "i = %i; min = %i\n", i, min ); */
		if( min == i ){
			k = i;
			switch( self->etype ){
			case DAO_INTEGER : ival = self->data.i[i]; break;
			case DAO_FLOAT   : fval = self->data.f[i]; break;
			case DAO_DOUBLE  : dval = self->data.d[i]; break;
			case DAO_COMPLEX : c16val = self->data.c[i]; break;
			default : break;
			}
			while(1){
				Array_FlatIndex2Mult( dm, D, k, mdn );
				for(j=0; j<D; j++) mdo[j] = mdn[ pm[j] ];
				Array_MultIndex2Flat( self->dims + self->ndim, D, & m, mdo );
				/* printf( "i = %i; m = %i\n", i, m ); */
				switch( self->etype ){
				case DAO_INTEGER : self->data.i[k] = (m == min) ? ival : self->data.i[m]; break;
				case DAO_FLOAT : self->data.f[k] = (m == min) ? fval : self->data.f[m]; break;
				case DAO_DOUBLE : self->data.d[k] = (m == min) ? dval : self->data.d[m]; break;
				case DAO_COMPLEX : self->data.c[k] = (m == min) ? c16val : self->data.c[m]; break;
				default : break;
				}
				if( m == min ) break;
				k = m;
			}
		}
	}
	DaoArray_Reshape( self, dm, D );
	return 1;
}
void DaoArray_SetDimCount( DaoArray *self, int D )
{
	if( D == self->ndim ) return;
	self->ndim = D;
	self->dims = (daoint*) dao_realloc( self->dims, 2*D*sizeof(daoint) );
}
void DaoArray_FinalizeDimData( DaoArray *self )
{
	int i, D = self->ndim;
	daoint *prods = self->dims + D;

	prods[ D - 1 ] = 1;
	for(i=D-2; i>=0; i--) prods[i] = prods[i+1] * self->dims[i+1];
	self->size = (int)( self->dims[0] * prods[0] );
}

static int SliceRange( DVector *slices, daoint N, daoint first, daoint last )
{
	daoint *start = DVector_PushDaoInt( slices, 0 );
	daoint *count = DVector_PushDaoInt( slices, 0 );
	if( first < 0 ) first += N;
	if( last < 0 ) last += N;
	if( first < 0 || first >= N || last < 0 || last >= N ) return 0;
	*start = first;
	if( first <= last ) *count = last - first + 1;
	return 1;
}
static int SliceRange2( DVector *slices, daoint N, daoint first, daoint count )
{
	daoint *start = DVector_PushDaoInt( slices, 0 );
	daoint *count2 = DVector_PushDaoInt( slices, 0 );
	if( first < 0 ) first += N;
	if( first < 0 || first >= N ) return 0;
	*start = first;
	if( first + count > N ) return 0;
	*count2 = count;
	return 1;
}
static void MakeSlice( DaoProcess *proc, DaoValue *pid, daoint N, DVector *slices )
{
	daoint j, id, from, to, rc = 1;
	daoint size = slices->size;
	if( pid == NULL || pid->type == 0 ){
		SliceRange2( slices, N, 0, N );
		return;
	}
	switch( pid->type ){
	case DAO_INTEGER :
	case DAO_FLOAT :
	case DAO_DOUBLE :
		{
			id = DaoValue_GetInteger( pid );
			rc = SliceRange2( slices, N, id, 1 );
			break;
		}
	case DAO_TUPLE :
		{
			DaoValue **data = pid->xTuple.values;
			if( data[0]->type == DAO_INTEGER && data[1]->type == DAO_INTEGER ){
				if( pid->xTuple.ctype == dao_type_for_iterator ){
					rc = SliceRange2( slices, N, data[1]->xInteger.value, 1 );
					data[1]->xInteger.value += 1;
					data[0]->xInteger.value = data[1]->xInteger.value < N;
				}else{
					from = data[0]->xInteger.value;
					to   = data[1]->xInteger.value;
					rc = SliceRange( slices, N, from, to );
				}
			}else if( data[0]->type == DAO_NONE && data[1]->type == DAO_NONE ){
				rc = SliceRange2( slices, N, 0, N );
			}else if( data[0]->type <= DAO_DOUBLE && data[1]->type == DAO_NONE ){
				from = DaoValue_GetInteger( data[0] );
				rc = SliceRange( slices, N, from, -1 );
			}else if( data[0]->type == DAO_NONE && data[1]->type <= DAO_DOUBLE ){
				to = DaoValue_GetInteger( data[1] );
				rc = SliceRange( slices, N, 0, to );
			}else{
				DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "need number" );
			}
			break;
		}
	default: rc = 0; break;
	}
	if( slices->size == size ) SliceRange2( slices, N, 0, N );
	if( rc == 0 ) DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, "" );
}
static daoint DaoArray_IndexFromSlice( DaoArray *self, DVector *slices, daoint sid )
/* sid: plain index in the slicesd array */
{
	daoint *dimAccum = self->dims + self->ndim;
	daoint j, index = 0;
	for( j=(int)self->ndim; j>0; j-- ){
		daoint *ids = slices->data.daoints + 2*(j-1);
		daoint count = ids[1];
		daoint res = sid % count;
		index += (ids[0] + res) * dimAccum[j-1];
		sid /= count;
	}
	return index;
}

#define DAO_SLICE_COUNT   0
#define DAO_SLICE_STEP    1
#define DAO_SLICE_START   2
#define DAO_SLICE_LENGTH  3

static int DaoArray_MakeSlice( DaoArray *self, DaoProcess *proc, DaoValue *idx[], int N, DaoArray *result )
{
	int exceptions = proc->exceptions->size;
	daoint *dims = self->dims;
	daoint count, start, step, length; /* for slices; */
	daoint i, D = self->ndim;
	daoint S = D != 0;

	if( result->slices == NULL ) result->slices = DVector_New( sizeof(daoint) );
	DVector_Reserve( result->slices, 2*self->ndim+4 );
	DVector_Reset( result->slices, 0 );
	if( N == 1 ){
		if( D == 2 && dims[0] == 1 ){ /* Row vector: */
			SliceRange2( result->slices, 1, 0, 1 );
			MakeSlice( proc, idx[0], dims[1], result->slices );
		}else{
			MakeSlice( proc, idx[0], dims[0], result->slices );
			SliceRange2( result->slices, dims[1], 0, dims[1] );
		}
		for(i=2; i<D; ++i) SliceRange2( result->slices, dims[i], 0, dims[i] );
	}else{
		const int n = N > D ? D : N;
		for(i=0; i<n; ++i) MakeSlice( proc, idx[i], dims[i], result->slices );
		for(i=n; i<D; ++i) SliceRange2( result->slices, dims[i], 0, dims[i] );
	}
	if( proc->exceptions->size > exceptions ) return 0;
	for(i=0; i<D; ++i) S *= result->slices->data.daoints[2*i+1];
	length = result->slices->data.daoints[2*self->ndim-1];
	count = S / length;
	start = DaoArray_IndexFromSlice( self, result->slices, 0 );
	step = count > 1 ? DaoArray_IndexFromSlice( self, result->slices, length ) - start : 0;
	DVector_PushDaoInt( result->slices, count );
	DVector_PushDaoInt( result->slices, step );
	DVector_PushDaoInt( result->slices, start );
	DVector_PushDaoInt( result->slices, length );
	return S;
}
daoint DaoArray_MatchShape( DaoArray *self, DaoArray *other )
{
	daoint i = 0, j = 0, m = 1;
	daoint ndim1 = self->original ? self->original->ndim : self->ndim;
	daoint ndim2 = other->original ? other->original->ndim : other->ndim;
	while( i < ndim1 && j < ndim2 ){
		daoint D1 = self->original ? self->slices->data.daoints[2*i+1] : self->dims[i];
		daoint D2 = other->original ? other->slices->data.daoints[2*j+1] : other->dims[j];
		if( D1 == 1 || D2 == 1 ){
			i += D1 == 1; /* skip size-one dimension; */
			j += D2 == 1; /* skip size-one dimension; */
			continue;
		}
		if( D1 != D2 ) return -1;
		m *= D1;
		i += 1;
		j += 1;
	}
	if( i < ndim1 || j < ndim2 ) return -1; /* not matching; */
	return m;
}
DaoArray* DaoArray_GetWorkArray( DaoArray *self )
{
	if( self->original == NULL || self->slices == NULL ) return self;
	return self->original;
}
daoint DaoArray_GetWorkSize( DaoArray *self )
{
	daoint *quad;
	if( self->original == NULL || self->slices == NULL ) return self->size;
	quad = self->slices->data.daoints + 2*self->original->ndim;
	return quad[DAO_SLICE_COUNT] * quad[DAO_SLICE_LENGTH];
}
daoint DaoArray_GetWorkStep( DaoArray *self )
{
	if( self->original == NULL || self->slices == NULL ) return 1;
	return self->slices->data.daoints[ 2*self->original->ndim + DAO_SLICE_STEP ];
}
daoint DaoArray_GetWorkStart( DaoArray *self )
{
	if( self->original == NULL || self->slices == NULL ) return 0;
	return self->slices->data.daoints[ 2*self->original->ndim + DAO_SLICE_START ];
}
daoint DaoArray_GetWorkIntervalSize( DaoArray *self )
{
	if( self->original == NULL || self->slices == NULL ) return self->size;
	return self->slices->data.daoints[ 2*self->original->ndim + DAO_SLICE_LENGTH ];
}

daoint DaoArray_GetInteger( DaoArray *self, daoint i )
{
	switch( self->etype ){
	case DAO_INTEGER: return self->data.i[i];
	case DAO_FLOAT : return self->data.f[i];
	case DAO_DOUBLE : return (float)self->data.d[i];
	case DAO_COMPLEX : return (float)self->data.c[i].real;
	default : break;
	}
	return 0;
}
float DaoArray_GetFloat( DaoArray *self, daoint i )
{
	switch( self->etype ){
	case DAO_INTEGER: return self->data.i[i];
	case DAO_FLOAT : return self->data.f[i];
	case DAO_DOUBLE : return (float)self->data.d[i];
	case DAO_COMPLEX : return (float)self->data.c[i].real;
	default : break;
	}
	return 0;
}
double DaoArray_GetDouble( DaoArray *self, daoint i )
{
	switch( self->etype ){
	case DAO_INTEGER: return self->data.i[i];
	case DAO_FLOAT : return self->data.f[i];
	case DAO_DOUBLE : return self->data.d[i];
	case DAO_COMPLEX : return self->data.c[i].real;
	default : break;
	}
	return 0;
}
complex16 DaoArray_GetComplex( DaoArray *self, daoint i )
{
	complex16 com = {0,0};
	switch( self->etype ){
	case DAO_INTEGER : com.real = self->data.i[i]; break;
	case DAO_FLOAT : com.real = self->data.f[i]; break;
	case DAO_DOUBLE : com.real = self->data.d[i]; break;
	case DAO_COMPLEX : return self->data.c[i];
	default : break;
	}
	return com;
}
DaoValue* DaoArray_GetValue( DaoArray *self, daoint i, DaoValue *res )
{
	res->type = self->etype;
	switch( self->etype ){
	case DAO_INTEGER : res->xInteger.value = self->data.i[i]; break;
	case DAO_FLOAT   : res->xFloat.value = self->data.f[i]; break;
	case DAO_DOUBLE  : res->xDouble.value = self->data.d[i]; break;
	case DAO_COMPLEX : res->xComplex.value = self->data.c[i]; break;
	default : break;
	}
	return res;
}
void DaoArray_SetValue( DaoArray *self, daoint i, DaoValue *value )
{
	switch( self->etype ){
	case DAO_INTEGER : self->data.i[i] = DaoValue_GetInteger( value ); break;
	case DAO_FLOAT   : self->data.f[i] = DaoValue_GetFloat( value ); break;
	case DAO_DOUBLE  : self->data.d[i] = DaoValue_GetDouble( value ); break;
	case DAO_COMPLEX : self->data.c[i] = DaoValue_GetComplex( value ); break;
	default : break;
	}
}

int DaoArray_number_op_array( DaoArray *C, DaoValue *A, DaoArray *B, short op, DaoProcess *proc );
int DaoArray_array_op_number( DaoArray *C, DaoArray *A, DaoValue *B, short op, DaoProcess *proc );
int DaoArray_ArrayArith( DaoArray *s, DaoArray *l, DaoArray *r, short p, DaoProcess *c );
static void DaoArray_Print( DaoValue *value, DaoProcess *proc, DaoStream *stream, DMap *cycData );

static void DaoArray_GetItem1( DaoValue *value, DaoProcess *proc, DaoValue *pid )
{
	DaoArray *na, *self = & value->xArray;
	/* if( self->ctype ) printf( "DaoArray_GetItem: %s\n", self->ctype->name->chars ); */

	if( pid->type >= DAO_INTEGER && pid->type <= DAO_DOUBLE ){
		daoint id = DaoValue_GetInteger( pid );
		if( id < 0 ) id += self->size;
		if( id < 0 || id >= self->size ){
			DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, "" );
			return;
		}
		switch( self->etype ){
		case DAO_INTEGER : DaoProcess_PutInteger( proc, self->data.i[id] ); break;
		case DAO_FLOAT : DaoProcess_PutFloat( proc, self->data.f[id] ); break;
		case DAO_DOUBLE : DaoProcess_PutDouble( proc, self->data.d[id] ); break;
		case DAO_COMPLEX : DaoProcess_PutComplex( proc, self->data.c[id] ); break;
		default : break;
		}
		return;
	}else if( pid->type == DAO_TUPLE && pid->xTuple.ctype == dao_type_for_iterator ){
		DaoValue **data = pid->xTuple.values;
		daoint id = data[1]->xInteger.value;
		if( data[1]->type != DAO_INTEGER || id < 0 || id >= self->size ){
			DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, "index out of range" );
			return;
		}
		switch( self->etype ){
		case DAO_INTEGER : DaoProcess_PutInteger( proc, self->data.i[id] ); break;
		case DAO_FLOAT : DaoProcess_PutFloat( proc, self->data.f[id] ); break;
		case DAO_DOUBLE : DaoProcess_PutDouble( proc, self->data.d[id] ); break;
		case DAO_COMPLEX : DaoProcess_PutComplex( proc, self->data.c[id] ); break;
		default : break;
		}
		data[1]->xInteger.value += 1;
		data[0]->xInteger.value = data[1]->xInteger.value < self->size;
		return;
	}
	na = DaoProcess_PutArray( proc );
	DaoArray_SetNumType( na, self->etype );
	GC_ShiftRC( self, na->original );
	na->original = self;
	DaoArray_MakeSlice( self, proc, & pid, 1, na );
}
int DaoArray_CopyArray( DaoArray *self, DaoArray *other )
{
	daoint i, N = self->size;
	assert( self->slices == NULL && other->slices == NULL );
	if( DaoArray_MatchShape( self, other ) == 0 ) return 0;
	switch( self->etype | (other->etype << 4) ){
	case DAO_INTEGER | (DAO_INTEGER<<4) :
		for(i=0;i<N;i++) self->data.i[i] = other->data.i[i]; break;
	case DAO_INTEGER | (DAO_FLOAT<<4) :
		for(i=0;i<N;i++) self->data.i[i] = other->data.f[i]; break;
	case DAO_INTEGER | (DAO_DOUBLE<<4) :
		for(i=0;i<N;i++) self->data.i[i] = other->data.d[i]; break;
	case DAO_INTEGER | (DAO_COMPLEX<<4) :
		for(i=0;i<N;i++) self->data.i[i] = other->data.c[i].real; break;
	case DAO_FLOAT | (DAO_INTEGER<<4) :
		for(i=0;i<N;i++) self->data.f[i] = other->data.i[i]; break;
	case DAO_FLOAT | (DAO_FLOAT<<4) :
		for(i=0;i<N;i++) self->data.f[i] = other->data.f[i]; break;
	case DAO_FLOAT | (DAO_DOUBLE<<4) :
		for(i=0;i<N;i++) self->data.f[i] = other->data.d[i]; break;
	case DAO_FLOAT | (DAO_COMPLEX<<4) :
		for(i=0;i<N;i++) self->data.f[i] = other->data.c[i].real; break;
	case DAO_DOUBLE | (DAO_INTEGER<<4) :
		for(i=0;i<N;i++) self->data.d[i] = other->data.i[i]; break;
	case DAO_DOUBLE | (DAO_FLOAT<<4) :
		for(i=0;i<N;i++) self->data.d[i] = other->data.f[i]; break;
	case DAO_DOUBLE | (DAO_DOUBLE<<4) :
		for(i=0;i<N;i++) self->data.d[i] = other->data.d[i]; break;
	case DAO_DOUBLE | (DAO_COMPLEX<<4) :
		for(i=0;i<N;i++) self->data.d[i] = other->data.c[i].real; break;
	case DAO_COMPLEX | (DAO_INTEGER<<4) :
		for(i=0;i<N;i++) self->data.c[i].real = other->data.i[i]; break;
	case DAO_COMPLEX | (DAO_FLOAT<<4) :
		for(i=0;i<N;i++) self->data.c[i].real = other->data.f[i]; break;
	case DAO_COMPLEX | (DAO_DOUBLE<<4) :
		for(i=0;i<N;i++) self->data.c[i].real = other->data.d[i]; break;
	case DAO_COMPLEX | (DAO_COMPLEX<<4) :
		for(i=0;i<N;i++) self->data.c[i] = other->data.c[i]; break;
	default : break;
	}
	return 1;
}
/* Invalid comparison returns either -100 or 100: */
int DaoArray_Compare( DaoArray *x, DaoArray *y )
{
	daoint *xi = x->data.i, *yi = y->data.i;
	float *xf = x->data.f, *yf = y->data.f;
	double *xd = x->data.d, *yd = y->data.d;
	complex16 *xc = x->data.c, *yc = y->data.c;
	daoint min = x->size < y->size ? x->size : y->size;
	daoint res = x->size == y->size ? 1 : 100;
	daoint i = 0;
	if( x->etype == DAO_INTEGER && y->etype == DAO_INTEGER ){
		while( i < min && *xi == *yi ) i++, xi++, yi++;
		if( i < min ) return *xi < *yi ? -res : res;
	}else if( x->etype == DAO_FLOAT && y->etype == DAO_FLOAT ){
		while( i < min && *xf == *yf ) i++, xf++, yf++;
		if( i < min ) return *xf < *yf ? -res : res;
	}else if( x->etype == DAO_DOUBLE && y->etype == DAO_DOUBLE ){
		while( i < min && *xd == *yd ) i++, xd++, yd++;
		if( i < min ) return *xd < *yd ? -res : res;
	}else if( x->etype == DAO_COMPLEX && y->etype == DAO_COMPLEX ){
		while( i < min && xc->real == yc->real && xc->imag == yc->imag ) i++, xc++, yc++;
		if( i < min ){
			if( xc->real == yc->real ) return xc->imag < yc->imag ? -100 : 100;
			if( xc->imag == yc->imag ) return xc->real < yc->real ? -100 : 100;
			return (daoint)x < (daoint)y ? -100 : 100;
		}
		if( x->size == y->size  ) return 0;
		return x->size < y->size ? -100 : 100;
	}else if( x->etype != DAO_COMPLEX && y->etype != DAO_COMPLEX ){
		while( i < min && DaoArray_GetDouble( x, i ) == DaoArray_GetDouble( y, i ) ) i++;
		if( i < min ){
			double xv = DaoArray_GetDouble( x, i );
			double yv = DaoArray_GetDouble( y, i );
			if( xv == yv ) return 0;
			return xv < yv ? -res : res;
		}
	}else{ /* one is a complex array, the other is not: */
		return (daoint) x < (daoint) y ? -100 : 100;
	}
	if( x->size == y->size  ) return 0;
	return x->size < y->size ? -100 : 100;
}
void DaoArray_SetItem1( DaoValue *va, DaoProcess *proc, DaoValue *pid, DaoValue *value )
{
	DaoArray *self = & va->xArray;

	DaoArray_Sliced( self );
	if( value->type ==0 ) return;
	if( pid == NULL || pid->type == 0 ){
		if( value->type >= DAO_INTEGER && value->type <= DAO_COMPLEX ){
			DaoArray_array_op_number( self, self, value, DVM_MOVE, proc );
		}else if( value->type == DAO_ARRAY ){
			DaoArray_ArrayArith( self, self, (DaoArray*) value, DVM_MOVE, proc );
		}else{
			DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "" );
		}
		return;
	}else if( pid->type >= DAO_INTEGER && pid->type <= DAO_DOUBLE && value->type <= DAO_COMPLEX ){
		daoint id = DaoValue_GetInteger( pid );
		if( id < 0 ) id += self->size;
		if( id < 0 || id >= self->size ){
			DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, "" );
			return;
		}
		DaoArray_SetValue( self, id, value );
		return;
	}
	DaoArray_MakeSlice( self, proc, & pid, 1, self );
	self->original = self;
	if( value->type == DAO_ARRAY ){
		DaoArray_ArrayArith( self, self, (DaoArray*) value, DVM_MOVE, proc );
	}else{
		DaoArray_array_op_number( self, self, value, DVM_MOVE, proc );
	}
	self->original = NULL;
}
static void DaoArray_GetItem( DaoValue *vself, DaoProcess *proc, DaoValue *ids[], int N )
{
	DaoArray *na, *self = & vself->xArray;
	daoint i;
	DaoArray_Sliced( self );
	if( N == 0 ){
		vself = (DaoValue*) DaoArray_Copy( self );
		DaoProcess_PutValue( proc, vself );
		return;
	}else if( N == 1 ){
		DaoArray_GetItem1( vself, proc, ids[0] );
		return;
	}else if( N <= self->ndim ){
		daoint *dimAccum = self->dims + self->ndim;
		daoint *dims = self->dims;
		daoint allNumbers = 1;
		daoint idFlat = 0;
		daoint k;
		for(i=0; i<N; i++){
			if( ids[i]->type < DAO_INTEGER || ids[i]->type > DAO_DOUBLE ){
				allNumbers = 0;
				break;
			}
			k = DaoValue_GetInteger( ids[i] );
			if( k <0 ) k += dims[i];
			idFlat += k * dimAccum[i];
			if( k < 0 || k >= dims[i] ){
				idFlat = self->size; /* to raise exception */
				break;
			}
		}
		if( idFlat >= self->size ){
			DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, "index out of range" );
			return;
		}
		if( allNumbers ){
			if( self->etype == DAO_INTEGER ){
				DaoProcess_PutInteger( proc, DaoArray_GetInteger( self, idFlat ) );
			}else if( self->etype == DAO_FLOAT ){
				DaoProcess_PutFloat( proc, DaoArray_GetFloat( self, idFlat ) );
			}else if( self->etype == DAO_DOUBLE ){
				DaoProcess_PutDouble( proc, DaoArray_GetDouble( self, idFlat ) );
			}else{
				DaoProcess_PutComplex( proc, DaoArray_GetComplex( self, idFlat ) );
			}
			return;
		}
	}
	na = DaoProcess_PutArray( proc );
	DaoArray_SetNumType( na, self->etype );
	GC_ShiftRC( self, na->original );
	na->original = self;
	DaoArray_MakeSlice( self, proc, ids, N, na );
}
static void DaoArray_SetItem( DaoValue *vself, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	DaoArray *self = & vself->xArray;
	DaoArray_Sliced( self );
	if( N == 0 ){
		DaoArray_SetItem1( vself, proc, dao_none_value, value );
		return;
	}else if( N == 1 ){
		DaoArray_SetItem1( vself, proc, ids[0], value );
		return;
	}else if( N <= self->ndim ){
		daoint *dims = self->dims;
		daoint *dimAccum = self->dims + self->ndim;
		daoint i, allNumbers = 1;
		daoint k, idFlat = 0;
		for(i=0; i<N; i++){
			if( ids[i]->type < DAO_INTEGER || ids[i]->type > DAO_DOUBLE ){
				allNumbers = 0;
				break;
			}
			k = DaoValue_GetInteger( ids[i] );
			idFlat += k * dimAccum[i];
			if( k >= dims[i] ){
				idFlat = self->size; /* to raise exception */
				break;
			}
		}
		if( idFlat >= self->size ){
			DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, "index out of range" );
			return;
		}
		if( allNumbers ){
			DaoArray_SetValue( self, idFlat, value );
			return;
		}
	}
	DaoArray_MakeSlice( self, proc, ids, N, self );
	self->original = self;
	if( value->type == DAO_ARRAY ){
		DaoArray *na = & value->xArray;
		DaoArray_ArrayArith( self, self, na, DVM_MOVE, proc );
	}else{
		DaoArray_array_op_number( self, self, value, DVM_MOVE, proc );
	}
	self->original = NULL;
}
static void DaoArray_PrintElement( DaoArray *self, DaoStream *stream, daoint i )
{
	switch( self->etype ){
	case DAO_INTEGER :
		DaoStream_WriteInt( stream, self->data.i[i] );
		break;
	case DAO_FLOAT :
		DaoStream_WriteFloat( stream, self->data.f[i] );
		break;
	case DAO_DOUBLE :
		DaoStream_WriteFloat( stream, self->data.d[i] );
		break;
	case DAO_COMPLEX :
		DaoStream_WriteFloat( stream, self->data.c[i].real );
		if( self->data.c[i].imag >= -0.0 ) DaoStream_WriteChars( stream, "+" );
		DaoStream_WriteFloat( stream, self->data.c[i].imag );
		DaoStream_WriteChars( stream, "$" );
		break;
	default : break;
	}
}
static void DaoArray_Print( DaoValue *value, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	DaoArray *self = & value->xArray;
	daoint i, *tmp, *dims = self->dims;
	int j;

	if( self->ndim < 2 ) return;
	if( self->ndim ==2 && ( dims[0] ==1 || dims[1] ==1 ) ){
		/* For vectors: */
		const char *sep = (dims[0] >1 && dims[1] ==1) ? "; " : ", ";
		DaoStream_WriteChars( stream, "[ " );
		for(i=0; i<self->size; i++){
			DaoArray_PrintElement( self, stream, i );
			if( i+1 < self->size ) DaoStream_WriteChars( stream, sep );
		}
		DaoStream_WriteChars( stream, " ]" );
	}else{
		DArray *tmpArray = DArray_New(0);
		DArray_Resize( tmpArray, self->ndim, 0 );
		tmp = tmpArray->items.pInt;
		for(i=0; i<self->size; i++){
			daoint mod = i;
			for(j=self->ndim-1; j>=0; j--){
				daoint res = ( mod % dims[j] );
				mod /= dims[j];
				tmp[j] = res;
			}
			if( tmp[self->ndim-1] ==0 ){
				DaoStream_WriteChars( stream, "row[" );
				for(j=0; j+1<self->ndim; j++){
					DaoStream_WriteFormatedInt( stream, (int)tmp[j], "%i" );
					DaoStream_WriteChars( stream, "," );
				}
				DaoStream_WriteChars( stream, ":]:\t" );
			}
			DaoArray_PrintElement( self, stream, i );
			if( i+1 < self->size ) DaoStream_WriteChars( stream, "\t" );
			if( tmp[self->ndim-1] +1 == dims[self->ndim-1] )
				DaoStream_WriteChars( stream, "\n" );
		}
		DArray_Delete( tmpArray );
	}
}

static DaoTypeCore numarrCore =
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoArray_GetItem,
	DaoArray_SetItem,
	DaoArray_Print
};
static void DaoARRAY_Dim( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoArray *self = & par[0]->xArray;
	daoint i, *v;

	if( N == 1 ){
		DaoArray *na = DaoProcess_PutArray( proc );
		DaoArray_SetNumType( na, DAO_INTEGER );
		if( self->original ){
			DaoArray_ResizeVector( na, self->original->ndim );
			v = na->data.i;
			for(i=0; i<self->original->ndim; ++i) v[i] = self->slices->data.daoints[2*i+1];
		}else{
			DaoArray_ResizeVector( na, self->ndim );
			v = na->data.i;
			for(i=0; i<self->ndim; i++) v[i] = self->dims[i];
		}
	}else if( self->original ){
		daoint *num = DaoProcess_PutInteger( proc, 0 );
		i = par[1]->xInteger.value;
		if( i <0 || i >= self->slices->size ){
			*num = -1;
			DaoProcess_RaiseException( proc, DAO_WARNING, "no such dimension" );
		}else{
			*num = self->slices->data.daoints[2*i+1];
		}
	}else{
		daoint *num = DaoProcess_PutInteger( proc, 0 );
		i = par[1]->xInteger.value;
		if( i <0 || i >= self->ndim ){
			*num = -1;
			DaoProcess_RaiseException( proc, DAO_WARNING, "no such dimension" );
		}else{
			*num = self->dims[i];
		}
	}
}
static void DaoARRAY_Size( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoArray *self = & par[0]->xArray;
	DaoProcess_PutInteger( proc, DaoArray_GetWorkSize( self ) );
}
static void DaoARRAY_Resize( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoArray *self = & par[0]->xArray;
	DaoArray *nad = & par[1]->xArray;
	DVector *ad;
	daoint *dims;
	daoint i, size = 1;

	if( self->etype == DAO_NONE && self->size == 0 ) self->etype = DAO_FLOAT;

	DaoArray_Sliced( self );
	DaoArray_Sliced( nad );

	if( nad->etype == DAO_COMPLEX ){
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "invalid dimension" );
		return;
	}
	ad = DVector_New( sizeof(daoint) );
	DVector_Resize( ad, nad->size );
	dims = ad->data.daoints;

	for(i=0; i<nad->size; i++){
		dims[i] = DaoArray_GetInteger( nad, i );
		size *= dims[i];
	}
	DaoProcess_PutValue( proc, (DaoValue*)self );
	DaoArray_ResizeArray( self, dims, ad->size );
	DVector_Delete( ad );
}
static void DaoARRAY_Reshape( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoArray *self = & par[0]->xArray;
	DaoArray *nad = & par[1]->xArray;
	DVector *ad;
	daoint *dims;
	daoint i, size;

	DaoArray_Sliced( self );
	DaoArray_Sliced( nad );

	if( nad->etype == DAO_COMPLEX ){
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "invalid dimension" );
		return;
	}
	ad = DVector_New( sizeof(daoint) );
	DVector_Resize( ad, nad->size );
	dims = ad->data.daoints;
	size = 1;
	for(i=0; i<nad->size; i++){
		dims[i] = DaoArray_GetInteger( nad, i );
		size *= (int)dims[i];
	}
	if( self->owner && self->size != size ){
		DVector_Delete( ad );
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "invalid dimension" );
		return;
	}
	DaoArray_SetDimCount( self, ad->size );
	memcpy( self->dims, dims, ad->size * sizeof(daoint) );
	DaoArray_FinalizeDimData( self );
	DaoProcess_PutValue( proc, (DaoValue*)self );
	DVector_Delete( ad );
}
static void DaoARRAY_Index( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoArray *self = & par[0]->xArray;
	DaoArray *na = DaoProcess_PutArray( proc );
	daoint *dim = self->dims;
	int i, D = self->ndim;
	daoint sd = par[1]->xInteger.value;
	daoint *v;

	DaoArray_Sliced( self );
	dim = self->dims;
	D = self->ndim;

	DaoArray_SetNumType( na, DAO_INTEGER );
	DaoArray_ResizeVector( na, self->ndim );
	v = na->data.i;
	for(i=D-1; i>=0; i--){
		v[i] = sd % dim[i];
		sd = sd / dim[i];
	}
}
static void DaoARRAY_max( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoTuple *tuple = DaoProcess_PutTuple( proc, 0 );
	DaoArray *self = (DaoArray*) par[0];
	DaoArray *array = DaoArray_GetWorkArray( self );
	daoint size = DaoArray_GetWorkSize( self );
	daoint start = DaoArray_GetWorkStart( self );
	daoint len = DaoArray_GetWorkIntervalSize( self );
	daoint step = DaoArray_GetWorkStep( self );
	daoint i, j, cmp = 0, imax = -1;

	if( self->etype == DAO_COMPLEX ) return;/* no exception, guaranteed by the typing system */
	if( size == 0 ) return;
	if( size ) imax = start;
	for(i=1; i<size; ++i){
		j = start + (i / len) * step + (i % len);
		switch( array->etype ){
		case DAO_INTEGER : cmp = array->data.i[imax] < array->data.i[j]; break;
		case DAO_FLOAT  : cmp = array->data.f[imax] < array->data.f[j]; break;
		case DAO_DOUBLE : cmp = array->data.d[imax] < array->data.d[j]; break;
		default : break;
		}
		if( cmp || imax < 0 ) imax = j;
	}
	tuple->values[1]->xInteger.value = imax;
	if( imax < 0 ) return;
	switch( array->etype ){
	case DAO_INTEGER : tuple->values[0]->xInteger.value = array->data.i[imax]; break;
	case DAO_FLOAT  : tuple->values[0]->xFloat.value = array->data.f[imax]; break;
	case DAO_DOUBLE : tuple->values[0]->xDouble.value = array->data.d[imax]; break;
	default : break;
	}
}
static void DaoARRAY_min( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoTuple *tuple = DaoProcess_PutTuple( proc, 0 );
	DaoArray *self = (DaoArray*) par[0];
	DaoArray *array = DaoArray_GetWorkArray( self );
	daoint size = DaoArray_GetWorkSize( self );
	daoint start = DaoArray_GetWorkStart( self );
	daoint len = DaoArray_GetWorkIntervalSize( self );
	daoint step = DaoArray_GetWorkStep( self );
	daoint i, j, cmp = 0, imin = -1;

	if( self->etype == DAO_COMPLEX ) return;/* no exception, guaranteed by the typing system */
	if( size == 0 ) return;
	if( size ) imin = start;
	for(i=1; i<size; ++i){
		j = start + (i / len) * step + (i % len);
		switch( array->etype ){
		case DAO_INTEGER : cmp = array->data.i[imin] > array->data.i[j]; break;
		case DAO_FLOAT  : cmp = array->data.f[imin] > array->data.f[j]; break;
		case DAO_DOUBLE : cmp = array->data.d[imin] > array->data.d[j]; break;
		default : break;
		}
		if( cmp || imin < 0 ) imin = j;
	}
	tuple->values[1]->xInteger.value = imin;
	if( imin < 0 ) return;
	switch( array->etype ){
	case DAO_INTEGER : tuple->values[0]->xInteger.value = array->data.i[imin]; break;
	case DAO_FLOAT  : tuple->values[0]->xFloat.value = array->data.f[imin]; break;
	case DAO_DOUBLE : tuple->values[0]->xDouble.value = array->data.d[imin]; break;
	default : break;
	}
}
static void DaoARRAY_sum( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoArray *self = (DaoArray*) par[0];
	DaoArray *array = DaoArray_GetWorkArray( self );
	daoint size = DaoArray_GetWorkSize( self );
	daoint start = DaoArray_GetWorkStart( self );
	daoint len = DaoArray_GetWorkIntervalSize( self );
	daoint step = DaoArray_GetWorkStep( self );
	daoint i, j, isum = 0;
	double fsum = 0;
	complex16 csum = {0,0};

	for(i=0; i<size; ++i){
		j = start + (i / len) * step + (i % len);
		switch( array->etype ){
		case DAO_INTEGER : isum += array->data.i[j]; break;
		case DAO_FLOAT   : fsum += array->data.f[j]; break;
		case DAO_DOUBLE  : fsum += array->data.d[j]; break;
		case DAO_COMPLEX : COM_IP_ADD( csum, array->data.c[j] ); break;
		default : break;
		}
	}

	switch( array->etype ){
	case DAO_INTEGER : DaoProcess_PutInteger( proc, isum ); break;
	case DAO_FLOAT   : DaoProcess_PutFloat( proc, fsum ); break;
	case DAO_DOUBLE  : DaoProcess_PutDouble( proc, fsum ); break;
	case DAO_COMPLEX : DaoProcess_PutComplex( proc, csum ); break;
	default : break;
	}
}
static void DaoARRAY_varn( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoArray *self = & par[0]->xArray;
	DaoArray *array = DaoArray_GetWorkArray( self );
	daoint size = DaoArray_GetWorkSize( self );
	daoint start = DaoArray_GetWorkStart( self );
	daoint len = DaoArray_GetWorkIntervalSize( self );
	daoint step = DaoArray_GetWorkStep( self );
	double *num = DaoProcess_PutDouble( proc, 0.0 );
	double dev, e = 0.0, sum = 0.0, sum2 = 0.0;
	daoint i, j;

	if( self->etype == DAO_COMPLEX ) return;
	if( size == 0 ) return;

	for(i=0; i<size; ++i){
		j = start + (i / len) * step + (i % len);
		switch( array->etype ){
		case DAO_INTEGER : e = array->data.i[j]; break;
		case DAO_FLOAT   : e = array->data.f[j]; break;
		case DAO_DOUBLE  : e = array->data.d[j]; break;
		default : break;
		}
		sum += e;
		sum2 += e * e;
	}
	dev = sum2 - sum * sum / size;
	*num = dev / size;
}
static int Compare( DaoArray *array, daoint *slice, daoint *index, daoint i, daoint j )
{
	if( index ){
		i = index[i];
		j = index[j];
	}
	i = slice[i];
	j = slice[j];
	switch( array->etype ){
	case DAO_INTEGER :
		{
			daoint a = array->data.i[i];
			daoint b = array->data.i[j];
			return a == b ? 0 : (a < b ? -1 : 1);
		}
	case DAO_FLOAT :
		{
			float a = array->data.f[i];
			float b = array->data.f[j];
			return a == b ? 0 : (a < b ? -1 : 1);
		}
	case DAO_DOUBLE :
		{
			double a = array->data.d[i];
			double b = array->data.d[j];
			return a == b ? 0 : (a < b ? -1 : 1);
		}
	default : break;
	}
	return 0;
}
static void Swap( DaoArray *array, daoint *slice, daoint *index, daoint i, daoint j )
{
	if( index ){
		int k = index[i];
		index[i] = index[j];
		index[j] = k;
		return;
	}
	i = slice[i];
	j = slice[j];
	switch( array->etype ){
	case DAO_INTEGER :
		{
			daoint a = array->data.i[i];
			daoint b = array->data.i[j];
			array->data.i[i] = b;
			array->data.i[j] = a;
			break;
		}
	case DAO_FLOAT :
		{
			float a = array->data.f[i];
			float b = array->data.f[j];
			array->data.f[i] = b;
			array->data.f[j] = a;
			break;
		}
	case DAO_DOUBLE :
		{
			double a = array->data.d[i];
			double b = array->data.d[j];
			array->data.d[i] = b;
			array->data.d[j] = a;
			break;
		}
	default : break;
	}
}
static void QuickSort2( DaoArray *array, daoint *slice,
		daoint *index, daoint first, daoint last, daoint part, int asc )
{
	daoint lower = first+1, upper = last;
	daoint pivot = (first + last) / 2;
	if( first >= last ) return;
	Swap( array, slice, index, first, pivot );

	pivot = first;
	while( lower <= upper ){
		if( asc ){
			while( lower < last && Compare( array, slice, index, lower, pivot ) <0 ) lower ++;
			while( upper > first && Compare( array, slice, index, pivot, upper ) <0 ) upper --;
		}else{
			while( lower < last && Compare( array, slice, index, lower, pivot ) >0 ) lower ++;
			while( upper > first && Compare( array, slice, index, pivot, upper ) >0 ) upper --;
		}
		if( lower < upper ){
			Swap( array, slice, index, lower, upper );
			upper --;
		}
		lower ++;
	}
	Swap( array, slice, index, first, upper );
	if( first+1 < upper ) QuickSort2( array, slice, index, first, upper-1, part, asc );
	if( upper >= part ) return;
	if( upper+1 < last ) QuickSort2( array, slice, index, upper+1, last, part, asc );
}
void DaoArray_GetSliceShape( DaoArray *self, daoint **dims, short *ndim );
static void DaoARRAY_rank( DaoProcess *proc, DaoValue *par[], int npar )
{
	DaoArray *res = DaoProcess_PutArray( proc );
	DaoArray *self = (DaoArray*) par[0];
	DaoArray *array = DaoArray_GetWorkArray( self );
	daoint size = DaoArray_GetWorkSize( self );
	daoint start = DaoArray_GetWorkStart( self );
	daoint len = DaoArray_GetWorkIntervalSize( self );
	daoint step = DaoArray_GetWorkStep( self );
	daoint part = par[2]->xInteger.value;
	daoint i, j, *ids, *index;

	if( res == NULL ) return;
	if( size == 0 ) return;

	DaoArray_SetNumType( res, DAO_INTEGER );
	DaoArray_GetSliceShape( self, & res->dims, & res->ndim );
	DaoArray_ResizeArray( res, res->dims, res->ndim );
	ids = res->data.i;
	for(i=0; i<size; i++) ids[i] = i;

	if( size < 2 ) return;
	if( part == 0 ) part = size;

	index = (daoint*) dao_malloc( size * sizeof(daoint) );
	for(i=0; i<size; ++i){
		index[i] = i;
		ids[i] = start + (i / len) * step + (i % len);
	}

	QuickSort2( array, ids, index, 0, size-1, part, (par[1]->xEnum.value == 0) );
	for(i=0; i<size; i++) ids[i] = index[i];
	dao_free( index );
}
static void DaoARRAY_sort( DaoProcess *proc, DaoValue *par[], int npar )
{
	DaoArray *self = (DaoArray*) par[0];
	DaoArray *array = DaoArray_GetWorkArray( self );
	daoint size = DaoArray_GetWorkSize( self );
	daoint start = DaoArray_GetWorkStart( self );
	daoint len = DaoArray_GetWorkIntervalSize( self );
	daoint step = DaoArray_GetWorkStep( self );
	daoint part = par[2]->xInteger.value;
	daoint i, j, *index;

	DaoProcess_PutValue( proc, par[0] );
	if( size < 2 ) return;
	if( part == 0 ) part = size;

	index = (daoint*) dao_malloc( size * sizeof(daoint) );
	for(i=0; i<size; ++i) index[i] = start + (i / len) * step + (i % len);

	QuickSort2( array, index, NULL, 0, size-1, part, (par[1]->xEnum.value == 0) );
	dao_free( index );
}

static void DaoARRAY_Permute( DaoProcess *proc, DaoValue *par[], int npar )
{
	DaoArray *self = & par[0]->xArray;
	DaoArray *pm = & par[1]->xArray;
	DArray *perm;
	int i, D = self->ndim;
	int res;
	if( pm->dims[0] * pm->dims[1] != pm->size || pm->ndim >2 || pm->size != D ) goto RaiseException;

	perm = DArray_New(0);
	DArray_Resize( perm, D, 0 );
	for(i=0; i<D; i++) perm->items.pInt[i] = DaoArray_GetInteger( pm, i );
	res = DaoArray_Permute( self, perm );
	DArray_Delete( perm );
	if( res ==0 ) goto RaiseException;
	return;
RaiseException:
	DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "invalid parameter for permute()" );
}
static void DaoARRAY_Transpose( DaoProcess *proc, DaoValue *par[], int npar )
{
	DaoArray *self = & par[0]->xArray;
	DArray *perm = DArray_New(0);
	int i, D = self->ndim;
	DArray_Resize( perm, D, 0 );
	for(i=0; i<D; i++) perm->items.pInt[i] = D-1-i;
	DaoArray_Permute( self, perm );
	DArray_Delete( perm );
}
static void DaoARRAY_Reverse( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoArray *self = & p[0]->xArray;
	daoint i = 0, N = self->size;
	complex16 swc, *dc = self->data.c;
	double swd, *dd = self->data.d;
	float swf, *df = self->data.f;
	daoint swi, *di = self->data.i;

	DaoProcess_PutReference( proc, p[0] );
	for(i=0; i<N/2; i++){
		switch( self->etype ){
		case DAO_INTEGER : swi = di[i]; di[i] = di[N-1-i]; di[N-1-i] = swi; break;
		case DAO_FLOAT   : swf = df[i]; df[i] = df[N-1-i]; df[N-1-i] = swf; break;
		case DAO_DOUBLE  : swd = dd[i]; dd[i] = dd[N-1-i]; dd[N-1-i] = swd; break;
		case DAO_COMPLEX : swc = dc[i]; dc[i] = dc[N-1-i]; dc[N-1-i] = swc; break;
		}
	}
}
static void DaoARRAY_BasicFunctional( DaoProcess *proc, DaoValue *p[], int npar, int funct );
static void DaoARRAY_Map( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoARRAY_BasicFunctional( proc, p, npar, DVM_FUNCT_MAP );
}
static void DaoARRAY_Select( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoARRAY_BasicFunctional( proc, p, npar, DVM_FUNCT_SELECT );
}
static void DaoARRAY_Index2( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoARRAY_BasicFunctional( proc, p, npar, DVM_FUNCT_INDEX );
}
static void DaoARRAY_Count( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoARRAY_BasicFunctional( proc, p, npar, DVM_FUNCT_COUNT );
}
static void DaoARRAY_Iterate( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoARRAY_BasicFunctional( proc, p, npar, DVM_FUNCT_ITERATE );
}
static void DaoARRAY_Apply( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoARRAY_BasicFunctional( proc, p, npar, DVM_FUNCT_APPLY );
}
static void DaoARRAY_Reduce( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoARRAY_BasicFunctional( proc, p, npar, DVM_FUNCT_FOLD );
}
static DaoFuncItem numarMeths[] =
{
	{ DaoARRAY_Dim,       "dim( self ::array<@T>, i : int )=>int" },
	{ DaoARRAY_Dim,       "dim( self ::array<@T> )=>array<int>" },
	{ DaoARRAY_Index,     "index( self ::array<@T>, i : int )=>array<int>" },
	{ DaoARRAY_Size,      "size( self ::array<@T> )=>int" },
	{ DaoARRAY_Resize,    "resize( self :array<@T>, dims :array<int> )" },
	{ DaoARRAY_Reshape,   "reshape( self :array<@T>, dims :array<int> )" },

	{ DaoARRAY_Permute,   "permute( self :array<@T>, dims :array<int> )" },
	{ DaoARRAY_Transpose, "transpose( self :array )" },
	{ DaoARRAY_max,       "max( self ::array<@T<int|float|double>> )=>tuple<@T,int>" },
	{ DaoARRAY_min,       "min( self ::array<@T<int|float|double>> )=>tuple<@T,int>" },
	{ DaoARRAY_sum,       "sum( self ::array<@T> )=>@T" },
	{ DaoARRAY_varn,      "varn( self ::array<@T<int|float|double>> )=>double" },
	{ DaoARRAY_Reverse,   "reverse( self :array<@T> )=>array<@T>" },
	{ DaoARRAY_rank,  "rank( self ::array<@T>, order :enum<ascend,descend>=$ascend, k=0 )=>array<int>" },
	{ DaoARRAY_sort,  "sort( self :array<@T>, order :enum<ascend,descend>=$ascend, k=0 )=>array<@T>" },

	{ DaoARRAY_Map,    "map( self :array<@T> )[item:@T,I:int,J:int,K:int,L:int,M:int=>@T2]=>array<@T2>" },
	{ DaoARRAY_Reduce, "reduce( self :array<@T> )[item:@T,res:@T,I:int,J:int,K:int,L:int,M:int=>@T]=>@T" },
	{ DaoARRAY_Reduce, "reduce( self :array<@T>, init :@V )[item:@T,res:@V,I:int,J:int,K:int,L:int,M:int=>@V]=>@V" },
	{ DaoARRAY_Select, "select( self :array<@T> )[item:@T,I:int,J:int,K:int,L:int,M:int=>int]=>list<@T>" },
	{ DaoARRAY_Index2, "index( self :array<@T> )[item:@T,I:int,J:int,K:int,L:int,M:int=>int]=>list<array<int>>" },
	{ DaoARRAY_Count,  "count( self :array<@T> )[item:@T,I:int,J:int,K:int,L:int,M:int=>int]=>int" },
	{ DaoARRAY_Iterate,  "iterate( self :array<@T> )[item:@T,I:int,J:int,K:int,L:int,M:int]" },
	{ DaoARRAY_Apply,  "apply( self :array<@T> )[item:@T,I:int,J:int,K:int,L:int,M:int=>@T]=>array<@T>" },
	{ NULL, NULL }
};

static int DaoArray_DataTypeSize( DaoArray *self );
void DaoArray_ResizeData( DaoArray *self, daoint size, daoint oldSize );

int DaoArray_NumType( DaoArray *self )
{
	return self->etype;
}
void DaoArray_SetNumType( DaoArray *self, short numtype )
{
	int n, m = DaoArray_DataTypeSize( self );
	if( self->etype == numtype ) return;
	self->etype = numtype;
	n = self->size * m / DaoArray_DataTypeSize( self );
	DaoArray_ResizeData( self, self->size, n );
}
int DaoArray_Size( DaoArray *self )
{
	return self->size;
}
int DaoArray_DimCount( DaoArray *self )
{
	return self->ndim;
}
int DaoArray_SizeOfDim( DaoArray *self, int d )
{
	return self->dims[d];
}
void DaoArray_GetShape( DaoArray *self, daoint *dims )
{
	int i;
	for(i=0; i<self->ndim; i++) dims[i] = self->dims[0];
}
int DaoArray_HasShape( DaoArray *self, daoint *dims, int D )
{
	int i;
	if( D != self->ndim ) return 0;
	for(i=0; i<self->ndim; i++)
		if( dims[i] != self->dims[0] )
			return 0;
	return 1;
}
daoint DaoArray_GetFlatIndex( DaoArray *self, daoint *index )
{
	daoint i, id = 0;
	for( i=0; i<self->ndim; i++ ) id += index[i] * self->dims[self->ndim + i];
	return id;
}
int DaoArray_Reshape( DaoArray *self, daoint *dims, int D )
{
	int i, size = 1;
	for(i=0; i<D; i++) size *= dims[i];

	if( self->owner && self->size != size ) return 0;
	DaoArray_SetDimCount( self, D );
	memcpy( self->dims, dims, D*sizeof(daoint) );
	DaoArray_FinalizeDimData( self );
	return 1;
}
double* DaoArray_ToDouble( DaoArray *self )
{
	daoint i, tsize = sizeof(double);
	double *buf;
	DaoArray_Sliced( self );
	if( self->owner ==0 ) return self->data.d;
	if( self->etype == DAO_DOUBLE || self->etype == DAO_COMPLEX ) return self->data.d;
	self->data.p = dao_realloc( self->data.p, (self->size+1) * tsize );
	buf = self->data.d;
	if( self->etype == DAO_INTEGER ){
		for(i=self->size-1; i>=0; i--) buf[i] = self->data.i[i];
	}else if( self->etype == DAO_FLOAT ){
		for(i=self->size-1; i>=0; i--) buf[i] = self->data.f[i];
	}
	return buf;
}
void DaoArray_FromDouble( DaoArray *self )
{
	daoint i;
	double *buf = self->data.d;
	if( self->etype == DAO_DOUBLE || self->etype == DAO_COMPLEX ) return;
	if( self->etype == DAO_INTEGER ){
		for(i=0; i<self->size; i++) self->data.i[i] = buf[i];
	}else if( self->etype == DAO_FLOAT ){
		for(i=0; i<self->size; i++) self->data.f[i] = buf[i];
	}
}
float* DaoArray_ToFloat( DaoArray *self )
{
	daoint i;
	float *buf = self->data.f;
	DaoArray_Sliced( self );
	if( self->etype == DAO_FLOAT ) return self->data.f;
	if( self->etype == DAO_INTEGER ){
		for(i=0; i<self->size; i++) buf[i] = (float)self->data.i[i];
	}else if( self->etype == DAO_DOUBLE ){
		for(i=0; i<self->size; i++) buf[i] = (float)self->data.d[i];
	}else{
		for(i=0; i<self->size; i++){
			buf[2*i] = (float)self->data.c[i].real;
			buf[2*i+1] = (float)self->data.c[i].imag;
		}
	}
	return buf;
}
void DaoArray_FromFloat( DaoArray *self )
{
	daoint i;
	float *buf = self->data.f;
	if( self->etype == DAO_FLOAT ) return;
	if( self->etype == DAO_INTEGER ){
		for(i=0; i<self->size; i++) self->data.i[i] = buf[i];
	}else if( self->etype == DAO_DOUBLE ){
		for(i=self->size-1; i>=0; i--) self->data.d[i] = buf[i];
	}else{
		for(i=self->size-1; i>=0; i--){
			self->data.c[i].real = buf[2*i];
			self->data.c[i].imag = buf[2*i+1];
		}
	}
}
daoint* DaoArray_ToInteger( DaoArray *self )
{
	daoint i;
	daoint *buf = self->data.i;
	DaoArray_Sliced( self );
	if( self->etype == DAO_INTEGER ) return self->data.i;
	switch( self->etype ){
	case DAO_FLOAT  : for(i=0; i<self->size; i++) buf[i] = (daoint)self->data.f[i]; break;
	case DAO_DOUBLE : for(i=0; i<self->size; i++) buf[i] = (daoint)self->data.d[i]; break;
	case DAO_COMPLEX :
		for(i=0; i<self->size; i++){
			buf[2*i] = (daoint)self->data.c[i].real;
			buf[2*i+1] = (daoint)self->data.c[i].imag;
		}
		break;
	default : break;
	}
	return buf;
}
void DaoArray_FromInteger( DaoArray *self )
{
	daoint i;
	daoint *buf = self->data.i;
	if( self->etype == DAO_INTEGER ) return;
	switch( self->etype ){
	case DAO_FLOAT  : for(i=0; i<self->size; i++) self->data.f[i] = buf[i]; break;
	case DAO_DOUBLE : for(i=self->size-1; i>=0; i--) self->data.d[i] = buf[i]; break;
	case DAO_COMPLEX :
		for(i=self->size-1; i>=0; i--){
			self->data.c[i].real = buf[2*i];
			self->data.c[i].imag = buf[2*i+1];
		}
		break;
	default : break;
	}
}
#define DefineFunction_DaoArray_To( name, type, cast ) \
type* name( DaoArray *self ) \
{ \
	daoint i, size = self->size; \
	type *buf = (type*) self->data.p; \
	DaoArray_Sliced( self ); \
	switch( self->etype ){ \
	case DAO_INTEGER : for(i=0; i<size; i++) buf[i] = (cast)self->data.i[i]; break; \
	case DAO_FLOAT   : for(i=0; i<size; i++) buf[i] = (cast)self->data.f[i]; break; \
	case DAO_DOUBLE  : for(i=0; i<size; i++) buf[i] = (cast)self->data.d[i]; break; \
	case DAO_COMPLEX : \
		for(i=0; i<size; i++){ \
			buf[2*i] = (cast)self->data.c[i].real; \
			buf[2*i+1] = (cast)self->data.c[i].imag; \
		} \
		break; \
	default : break; \
	} \
	return buf; \
}
DefineFunction_DaoArray_To( DaoArray_ToSByte, signed char, int );
DefineFunction_DaoArray_To( DaoArray_ToSShort, signed short, int );
DefineFunction_DaoArray_To( DaoArray_ToSInt, signed int, int );
DefineFunction_DaoArray_To( DaoArray_ToUByte, unsigned char, unsigned int );
DefineFunction_DaoArray_To( DaoArray_ToUShort, unsigned short, unsigned int );
DefineFunction_DaoArray_To( DaoArray_ToUInt, unsigned int, unsigned int );

#define DefineFunction_DaoArray_From( name, type ) \
void name( DaoArray *self ) \
{ \
	daoint i, size = self->size; \
	type *buf = (type*) self->data.p; \
	switch( self->etype ){ \
	case DAO_INTEGER : for(i=size-1; i>=0; i--) self->data.i[i] = buf[i]; break; \
	case DAO_FLOAT   : for(i=size-1; i>=0; i--) self->data.f[i] = buf[i]; break; \
	case DAO_DOUBLE  : for(i=size-1; i>=0; i--) self->data.d[i] = buf[i]; break; \
	case DAO_COMPLEX : \
		for(i=size-1; i>=0; i--){ \
			self->data.c[i].real = buf[2*i]; \
			self->data.c[i].imag = buf[2*i+1]; \
		} \
		break; \
	default : break; \
	} \
}

DefineFunction_DaoArray_From( DaoArray_FromSByte, signed char );
DefineFunction_DaoArray_From( DaoArray_FromSShort, signed short );
DefineFunction_DaoArray_From( DaoArray_FromSInt, signed int );
DefineFunction_DaoArray_From( DaoArray_FromUByte, unsigned char );
DefineFunction_DaoArray_From( DaoArray_FromUShort, unsigned short );
DefineFunction_DaoArray_From( DaoArray_FromUInt, unsigned int );


#define DefineFunction_DaoArray_SetVector( name, type ) \
void name( DaoArray *self, type *vec, daoint N ) \
{ \
	daoint i; \
	if( vec && N == 0 ){ \
		DaoArray_UseData( self, vec ); \
		return; \
	} \
	if( N != self->size ) DaoArray_ResizeData( self, N, self->size ); \
	switch( self->etype ){ \
	case DAO_INTEGER : for(i=0; i<N; i++) self->data.i[i] = (daoint) vec[i]; break; \
	case DAO_FLOAT   : for(i=0; i<N; i++) self->data.f[i] = vec[i]; break; \
	case DAO_DOUBLE  : for(i=0; i<N; i++) self->data.d[i] = vec[i]; break; \
	case DAO_COMPLEX : \
		for(i=0; i<N; i++){ \
			self->data.c[i].real = vec[i+i]; \
			self->data.c[i].imag = vec[i+i+1]; \
		} \
		break; \
	default : break; \
	} \
}

DefineFunction_DaoArray_SetVector( DaoArray_SetVectorSB, signed char );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorSS, signed short );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorSI, signed int );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorUB, unsigned char );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorUS, unsigned short );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorUI, unsigned int );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorI, daoint );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorF, float );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorD, double );

#define DefineFunction_DaoArray_SetMatrix( name, type ) \
void name( DaoArray *self, type **mat, daoint R, daoint C ) \
{ \
	daoint dm[2]; \
	daoint i, j, N = R * C; \
	dm[0] = R; dm[1] = C; \
	if( N != self->size ) DaoArray_ResizeData( self, N, self->size ); \
	DaoArray_Reshape( self, dm, 2 ); \
	switch( self->etype ){ \
	case DAO_INTEGER : for(i=0; i<N; i++) self->data.i[i] = (daoint)mat[i/R][i%R]; break; \
	case DAO_FLOAT   : for(i=0; i<N; i++) self->data.f[i] = mat[i/R][i%R]; break; \
	case DAO_DOUBLE  : for(i=0; i<N; i++) self->data.d[i] = mat[i/R][i%R]; break; \
	case DAO_COMPLEX : for(i=0; i<N; i++){ \
						   self->data.c[i].real = mat[i/R][2*(i%R)]; \
						   self->data.c[i].imag = mat[i/R][2*(i%R)+1]; \
					   } \
					   break; \
	default : break; \
	} \
}

DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixSB, signed char );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixSS, signed short );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixSI, signed int );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixUB, unsigned char );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixUS, unsigned short );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixUI, unsigned int );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixI, daoint );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixF, float );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixD, double );

void* DaoArray_GetBuffer( DaoArray *self )
{
	return self->data.p;
}
void DaoArray_SetBuffer( DaoArray *self, void *buffer, daoint size )
{
	DaoArray_UseData( self, buffer );
	self->size = size;
}

DaoTypeBase numarTyper =
{
	"array", & numarrCore, NULL, (DaoFuncItem*) numarMeths, {0}, {0},
	(FuncPtrDel) DaoArray_Delete, NULL
};


DaoArray* DaoArray_New( int etype )
{
	DaoArray* self = (DaoArray*) dao_calloc( 1, sizeof( DaoArray ) );
	DaoValue_Init( self, DAO_ARRAY );
	self->etype = etype;
	self->owner = 1;
	DaoArray_ResizeVector( self, 0 );
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogNew( (DaoValue*) self );
#endif
	return self;
}
void DaoArray_Delete( DaoArray *self )
{
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogDelete( (DaoValue*) self );
#endif
	if( self->dims ) dao_free( self->dims );
	if( self->owner && self->data.p ) dao_free( self->data.p );
	if( self->slices ) DVector_Delete( self->slices );
	if( self->original ) GC_DecRC( self->original );
	dao_free( self );
}
void DaoArray_UseData( DaoArray *self, void *data )
{
	if( self->data.p ) dao_free( self->data.p );
	self->data.p = data;
	self->owner = 0;
}
void DaoArray_GetSliceShape( DaoArray *self, daoint **dims, short *ndim )
{
	DArray *shape;
	daoint i, k, S = 0, D = self->ndim;
	if( self->original == NULL ){
		if( *ndim != D ) *dims = (daoint*) dao_realloc( *dims, 2*D*sizeof(daoint) );
		*ndim = self->ndim;
		memmove( *dims, self->dims, self->ndim * sizeof(daoint) );
		return;
	}
	*ndim = 0;
	if( self->slices->size < 2*self->original->ndim ) return;
	for(i=0; i<self->original->ndim; ++i){
		k = self->slices->data.daoints[2*i+1];
		if( k == 0 ) return; /* skip empty dimension */
		S += k > 1;
	}
	shape = DArray_New(0);
	for(i=0; i<self->original->ndim; ++i){
		k = self->slices->data.daoints[2*i+1];
		/* skip size one dimension if the final slice has at least two dimensions */
		if( k == 1 && (S > 1 || shape->size > 1) ) continue;
		DArray_Append( shape, k );
	}
	*ndim = shape->size;
	*dims = (daoint*) dao_realloc( *dims, 2*shape->size * sizeof(daoint) );
	memmove( *dims, shape->items.pInt, shape->size * sizeof(daoint) );
	DArray_Delete( shape );
}
int DaoArray_SliceFrom( DaoArray *self, DaoArray *original, DVector *slices )
{
	daoint i, j, k, D = 0, S = 0;
	daoint size, step, start, len;

	if( slices == NULL ){
		DaoArray_ResizeArray( self, original->dims, original->ndim );
		DaoArray_CopyArray( self, original );
		return 1;
	}
	if( slices->size < 2*original->ndim ) return 0;
	for(i=0; i<original->ndim; ++i){
		k = slices->data.daoints[2*i+1];
		S += k > 1;
		if( k == 0 ){ /* skip empty dimension */
			DaoArray_ResizeVector( self, 0 );
			return 1;
		}
	}
	DaoArray_SetDimCount( self, S );
	for(i=0; i<original->ndim; ++i){
		k = slices->data.daoints[2*i+1];
		/* skip size one dimension if the final slice has at least two dimensions */
		if( k == 1 && (S > 1 || D > 1) ) continue;
		self->dims[D++] = k;
	}
	DaoArray_ResizeArray( self, self->dims, D );

	start = slices->data.daoints[2*original->ndim+DAO_SLICE_START];
	len   = slices->data.daoints[2*original->ndim+DAO_SLICE_LENGTH];
	size  = slices->data.daoints[2*original->ndim+DAO_SLICE_COUNT] * len;
	step  = slices->data.daoints[2*original->ndim+DAO_SLICE_STEP];
	for(i=0; i<size; ++i){
		j = start + (i / len) * step + (i % len);
		switch( self->etype | (original->etype<<3) ){
		case DAO_INTEGER|(DAO_INTEGER<<3) : self->data.i[i] = original->data.i[j]; break;
		case DAO_INTEGER|(DAO_FLOAT<<3)   : self->data.i[i] = original->data.f[j]; break;
		case DAO_INTEGER|(DAO_DOUBLE<<3)  : self->data.i[i] = original->data.d[j]; break;
		case DAO_INTEGER|(DAO_COMPLEX<<3) : self->data.i[i] = original->data.c[j].real; break;
		case DAO_FLOAT  |(DAO_INTEGER<<3) : self->data.f[i] = original->data.i[j]; break;
		case DAO_FLOAT  |(DAO_FLOAT<<3)   : self->data.f[i] = original->data.f[j]; break;
		case DAO_FLOAT  |(DAO_DOUBLE<<3)  : self->data.f[i] = original->data.d[j]; break;
		case DAO_FLOAT  |(DAO_COMPLEX<<3) : self->data.f[i] = original->data.c[j].real; break;
		case DAO_DOUBLE |(DAO_INTEGER<<3) : self->data.d[i] = original->data.i[j]; break;
		case DAO_DOUBLE |(DAO_FLOAT<<3)   : self->data.d[i] = original->data.f[j]; break;
		case DAO_DOUBLE |(DAO_DOUBLE<<3)  : self->data.d[i] = original->data.d[j]; break;
		case DAO_DOUBLE |(DAO_COMPLEX<<3) : self->data.d[i] = original->data.c[j].real; break;
		case DAO_COMPLEX|(DAO_INTEGER<<3) : self->data.c[i].real = original->data.i[j];
											self->data.c[i].imag = 0.0; break;
		case DAO_COMPLEX|(DAO_FLOAT<<3)   : self->data.c[i].real = original->data.f[j];
											self->data.c[i].imag = 0.0; break;
		case DAO_COMPLEX|(DAO_DOUBLE<<3)  : self->data.c[i].real = original->data.d[j];
											self->data.c[i].imag = 0.0; break;
		case DAO_COMPLEX|(DAO_COMPLEX<<3) : self->data.c[i] = original->data.c[j]; break;
		default : break;
		}
	}
	return 1;
}
int DaoArray_Sliced( DaoArray *self )
{
	DaoArray *original = self->original;
	DVector *slices = self->slices;

	if( slices == NULL || original == NULL ) goto ReturnFalse;
	if( self->etype != original->etype ) goto ReturnFalse;
	if( slices->size < 2*original->ndim ) goto ReturnFalse;
	if( DaoArray_SliceFrom( self, original, slices ) ==0 ) goto ReturnFalse;
	GC_DecRC( self->original );
	self->original = NULL;
	return 1;
ReturnFalse:
	GC_DecRC( self->original );
	self->original = NULL;
	return 0;
}
static int DaoArray_DataTypeSize( DaoArray *self )
{
	switch( self->etype ){
	case DAO_INTEGER : return sizeof(daoint);
	case DAO_FLOAT   : return sizeof(float);
	case DAO_DOUBLE  : return sizeof(double);
	case DAO_COMPLEX : return sizeof(complex16);
	}
	return 0;
}
DaoArray* DaoArray_Copy( DaoArray *self )
{
	DaoArray *copy = DaoArray_New( self->etype );
	DaoArray_ResizeArray( copy, self->dims, self->ndim );
	memcpy( copy->data.p, self->data.p, self->size * DaoArray_DataTypeSize( self ) );
	return copy;
}
DaoArray* DaoArray_CopyX( DaoArray *self, DaoType *tp )
{
	DaoArray *copy = DaoArray_New( self->etype );
	if( tp && tp->tid == DAO_ARRAY && tp->nested->size ){
		int nt = tp->nested->items.pType[0]->tid;
		if( nt >= DAO_INTEGER && nt <= DAO_COMPLEX ) copy->etype = nt;
	}
	DaoArray_ResizeArray( copy, self->dims, self->ndim );
	DaoArray_CopyArray( copy, self );
	return copy;
}
void DaoArray_ResizeData( DaoArray *self, daoint size, daoint old )
{
	daoint item_size = DaoArray_DataTypeSize( self );
	daoint diff = size - old;
	if( self->owner ==0 ){
		self->size = size;
		return;
	}
	self->size = size;
	self->data.p = dao_realloc( self->data.p, size * item_size );
	if( size <= old ) return;
	memset( ((char*)self->data.p) + old * item_size, 0, diff * item_size );
}
void DaoArray_ResizeVector( DaoArray *self, daoint size )
{
	daoint old = self->size;
	DaoArray_SetDimCount( self, 2 );
	self->dims[0] = 1;
	self->dims[1] = size;
	DaoArray_FinalizeDimData( self );
	if( size == old ) return;
	DaoArray_ResizeData( self, size, old );
}
void DaoArray_ResizeArray( DaoArray *self, daoint *dims, int D )
{
	daoint *dims2 = dims;
	int i, k;
	daoint old = self->size;
	if( D == 1 ){
		DaoArray_ResizeVector( self, dims[0] );
		return;
	}
	k = 0;
	for(i=0; i<D; ++i){
		if( dims[i] == 0 ){
			DaoArray_ResizeVector( self, 0 );
			return;
		}
		if( dims[i] != 1 || D ==2 ) k ++;
	}
	/* It could be: self->dims == dims; */
	dims = (daoint*) dao_malloc( D*sizeof(daoint) );
	for(i=0; i<D; ++i) dims[i] = dims2[i];
	if( self->dims != dims || self->ndim != k ) DaoArray_SetDimCount( self, k );
	k = 0;
	for(i=0; i<D; ++i){
		if( dims[i] != 1 || D ==2 ) self->dims[k++] = dims[i];
	}
	/* self->ndim will be one for dims such as [100,1,1] */
	if( self->ndim ==1 ){
		self->ndim += 1;
		self->dims = (daoint*) dao_realloc( self->dims, 2*(k+1)*sizeof(daoint) );
		if( dims[0] == 1 ){
			self->dims[1] = self->dims[0];
			self->dims[0] = 1;
		}else{
			self->dims[k] = 1;
		}
	}
	dao_free( dims );
	DaoArray_FinalizeDimData( self );
	if( self->size == old ) return;
	DaoArray_ResizeData( self, self->size, old );
}
daoint DaoArray_UpdateShape( DaoArray *C, DaoArray *A )
{
	daoint N = DaoArray_MatchShape( C, A );
	if( C->original && N < 0 ) return -1;
	if( C != A && C->original == NULL && N < 0 ){
		DaoArray_GetSliceShape( A, & C->dims, & C->ndim );
		DaoArray_ResizeArray( C, C->dims, C->ndim );
		N = C->size;
	}
	return N;
}
daoint dao_powi( daoint x, daoint n )
{
	daoint res = 1;
	if( x == 0 || n < 0 ) return 0;
	if( n == 1 ) return 1;
	while( n -- ) res *= x;
	return res;
}
int DaoArray_number_op_array( DaoArray *C, DaoValue *A, DaoArray *B, short op, DaoProcess *proc )
{
	daoint N = DaoArray_UpdateShape( C, B );
	DaoArray *array_b = DaoArray_GetWorkArray( B );
	DaoArray *array_c = DaoArray_GetWorkArray( C );
	DaoArrayData *data_b = & array_b->data;
	DaoArrayData *data_c = & array_c->data;
	daoint size_b = DaoArray_GetWorkSize( B );
	daoint size_c = DaoArray_GetWorkSize( C );
	daoint start_b = DaoArray_GetWorkStart( B );
	daoint start_c = DaoArray_GetWorkStart( C );
	daoint len_b = DaoArray_GetWorkIntervalSize( B );
	daoint len_c = DaoArray_GetWorkIntervalSize( C );
	daoint step_b = DaoArray_GetWorkStep( B );
	daoint step_c = DaoArray_GetWorkStep( C );
	double bf, af = DaoValue_GetDouble( A );
	complex16 bc, ac = {0.0, 0.0};
	daoint i, b, c;

	ac.real = af;
	if( A->type == DAO_COMPLEX ) ac = A->xComplex.value;
	if( N < 0 ){
		if( proc ) DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "not matched shape" );
		return 0;
	}
	if( array_b->etype == DAO_INTEGER && A->type == DAO_INTEGER ){
		daoint bi, ci = 0, ai = A->xInteger.value;
		for(i=0; i<N; ++i){
			b = start_b + (i / len_b) * step_b + (i % len_b);
			c = start_c + (i / len_c) * step_c + (i % len_c);
			bi = data_b->i[b];
			switch( op ){
			case DVM_MOVE : ci = bi; break;
			case DVM_ADD : ci = ai + bi; break;
			case DVM_SUB : ci = ai - bi; break;
			case DVM_MUL : ci = ai * bi; break;
			case DVM_DIV : ci = ai / bi; break;
			case DVM_MOD : ci = ai % bi; break;
			case DVM_POW : ci = dao_powi( ai, bi );break;
			default : break;
			}
			switch( C->etype ){
			case DAO_INTEGER : data_c->i[c] = ci; break;
			case DAO_FLOAT   : data_c->f[c] = ci; break;
			case DAO_DOUBLE  : data_c->d[c] = ci; break;
			case DAO_COMPLEX : data_c->c[c].real = ci; data_c->c[c].imag = 0; break;
			}
		}
		return 1;
	}
	for(i=0; i<N; ++i){
		b = start_b + (i / len_b) * step_b + (i % len_b);
		c = start_c + (i / len_c) * step_c + (i % len_c);
		switch( C->etype ){
		case DAO_INTEGER :
			bf = DaoArray_GetDouble( array_b, b );
			switch( op ){
			case DVM_MOVE : data_c->i[c] = bf; break;
			case DVM_ADD : data_c->i[c] = af + bf; break;
			case DVM_SUB : data_c->i[c] = af - bf; break;
			case DVM_MUL : data_c->i[c] = af * bf; break;
			case DVM_DIV : data_c->i[c] = af / bf; break;
			case DVM_MOD : data_c->i[c] = af - bf*(daoint)(af / bf); break;
			case DVM_POW : data_c->i[c] = powf( af, bf );break;
			default : break;
			}
			break;
		case DAO_FLOAT :
			bf = DaoArray_GetDouble( array_b, b );
			switch( op ){
			case DVM_MOVE : data_c->f[c] = bf; break;
			case DVM_ADD : data_c->f[c] = af + bf; break;
			case DVM_SUB : data_c->f[c] = af - bf; break;
			case DVM_MUL : data_c->f[c] = af * bf; break;
			case DVM_DIV : data_c->f[c] = af / bf; break;
			case DVM_MOD : data_c->f[c] = af - bf*(daoint)(af / bf); break;
			case DVM_POW : data_c->f[c] = powf( af, bf );break;
			default : break;
			}
			break;
		case DAO_DOUBLE :
			bf = DaoArray_GetDouble( array_b, b );
			switch( op ){
			case DVM_MOVE : data_c->d[c] = bf; break;
			case DVM_ADD : data_c->d[c] = af + bf; break;
			case DVM_SUB : data_c->d[c] = af - bf; break;
			case DVM_MUL : data_c->d[c] = af * bf; break;
			case DVM_DIV : data_c->d[c] = af / bf; break;
			case DVM_MOD : data_c->d[c] = af - bf*(daoint)(af / bf); break;
			case DVM_POW : data_c->d[c] = powf( af, bf );break;
			default : break;
			}
			break;
		case DAO_COMPLEX :
			bc = DaoArray_GetComplex( array_b, b );
			switch( op ){
			case DVM_MOVE : data_c->c[c] = bc; break;
			case DVM_ADD : COM_ADD( data_c->c[c], ac, bc ); break;
			case DVM_SUB : COM_SUB( data_c->c[c], ac, bc ); break;
			case DVM_MUL : COM_MUL( data_c->c[c], ac, bc ); break;
			case DVM_DIV : COM_DIV( data_c->c[c], ac, bc ); break;
			default : break;
			}
			break;
		default : break;
		}
	}
	return 1;
}
int DaoArray_array_op_number( DaoArray *C, DaoArray *A, DaoValue *B, short op, DaoProcess *proc )
{
	daoint N = DaoArray_UpdateShape( C, A );
	DaoArray *array_a = DaoArray_GetWorkArray( A );
	DaoArray *array_c = DaoArray_GetWorkArray( C );
	DaoArrayData *data_a = & array_a->data;
	DaoArrayData *data_c = & array_c->data;
	daoint size_a = DaoArray_GetWorkSize( A );
	daoint size_c = DaoArray_GetWorkSize( C );
	daoint start_a = DaoArray_GetWorkStart( A );
	daoint start_c = DaoArray_GetWorkStart( C );
	daoint len_a = DaoArray_GetWorkIntervalSize( A );
	daoint len_c = DaoArray_GetWorkIntervalSize( C );
	daoint step_a = DaoArray_GetWorkStep( A );
	daoint step_c = DaoArray_GetWorkStep( C );
	double af, bf = DaoValue_GetDouble( B );
	daoint ai, ci = 0, bi = DaoValue_GetInteger( B );
	complex16 ac, bc = {0.0, 0.0};
	daoint i, a, c;

	bc.real = bf;
	if( B->type == DAO_COMPLEX ) bc = B->xComplex.value;
	if( N < 0 ){
		if( proc ) DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "not matched shape" );
		return 0;
	}
	if( array_a->etype == DAO_INTEGER && B->type == DAO_INTEGER ){
		for(i=0; i<N; ++i){
			a = start_a + (i / len_a) * step_a + (i % len_a);
			c = start_c + (i / len_c) * step_c + (i % len_c);
			ai = data_a->i[a];
			switch( op ){
			case DVM_MOVE : ci = bi; break;
			case DVM_ADD : ci = ai + bi; break;
			case DVM_SUB : ci = ai - bi; break;
			case DVM_MUL : ci = ai * bi; break;
			case DVM_DIV : ci = ai / bi; break;
			case DVM_MOD : ci = ai % bi; break;
			case DVM_POW : ci = dao_powi( ai, bi );break;
			default : break;
			}
			switch( C->etype ){
			case DAO_INTEGER : data_c->i[c] = ci; break;
			case DAO_FLOAT   : data_c->f[c] = ci; break;
			case DAO_DOUBLE  : data_c->d[c] = ci; break;
			case DAO_COMPLEX : data_c->c[c].real = ci; data_c->c[c].imag = 0; break;
			}
		}
		return 1;
	}
	for(i=0; i<N; ++i){
		a = start_a + (i / len_a) * step_a + (i % len_a);
		c = start_c + (i / len_c) * step_c + (i % len_c);
		switch( C->etype ){
		case DAO_INTEGER :
			af = DaoArray_GetDouble( array_a, a );
			switch( op ){
			case DVM_MOVE : data_c->i[c] = bf; break;
			case DVM_ADD : data_c->i[c] = af + bf; break;
			case DVM_SUB : data_c->i[c] = af - bf; break;
			case DVM_MUL : data_c->i[c] = af * bf; break;
			case DVM_DIV : data_c->i[c] = af / bf; break;
			case DVM_MOD : data_c->i[c] = af - bf*(daoint)(af / bf); break;
			case DVM_POW : data_c->i[c] = pow( af, bf );break;
			default : break;
			}
			break;
		case DAO_FLOAT :
			af = DaoArray_GetDouble( array_a, a );
			switch( op ){
			case DVM_MOVE : data_c->f[c] = bf; break;
			case DVM_ADD : data_c->f[c] = af + bf; break;
			case DVM_SUB : data_c->f[c] = af - bf; break;
			case DVM_MUL : data_c->f[c] = af * bf; break;
			case DVM_DIV : data_c->f[c] = af / bf; break;
			case DVM_MOD : data_c->f[c] = af - bf*(daoint)(af / bf); break;
			case DVM_POW : data_c->f[c] = powf( af, bf );break;
			default : break;
			}
			break;
		case DAO_DOUBLE :
			af = DaoArray_GetDouble( array_a, a );
			switch( op ){
			case DVM_MOVE : data_c->d[c] = bf; break;
			case DVM_ADD : data_c->d[c] = af + bf; break;
			case DVM_SUB : data_c->d[c] = af - bf; break;
			case DVM_MUL : data_c->d[c] = af * bf; break;
			case DVM_DIV : data_c->d[c] = af / bf; break;
			case DVM_MOD : data_c->d[c] = af - bf*(daoint)(af / bf); break;
			case DVM_POW : data_c->d[c] = pow( af, bf );break;
			default : break;
			}
			break;
		case DAO_COMPLEX :
			ac = DaoArray_GetComplex( array_a, a );
			switch( op ){
			case DVM_MOVE : data_c->c[c] = bc; break;
			case DVM_ADD : COM_ADD( data_c->c[c], ac, bc ); break;
			case DVM_SUB : COM_SUB( data_c->c[c], ac, bc ); break;
			case DVM_MUL : COM_MUL( data_c->c[c], ac, bc ); break;
			case DVM_DIV : COM_DIV( data_c->c[c], ac, bc ); break;
			default : break;
			}
			break;
		default : break;
		}
	}
	return 1;
}
int DaoArray_ArrayArith( DaoArray *C, DaoArray *A, DaoArray *B, short op, DaoProcess *proc )
{
	DaoArray *array_a = DaoArray_GetWorkArray( A );
	DaoArray *array_b = DaoArray_GetWorkArray( B );
	DaoArray *array_c = DaoArray_GetWorkArray( C );
	DaoArrayData *data_a = & array_a->data;
	DaoArrayData *data_b = & array_b->data;
	DaoArrayData *data_c = & array_c->data;
	daoint size_a = DaoArray_GetWorkSize( A );
	daoint size_b = DaoArray_GetWorkSize( B );
	daoint size_c = DaoArray_GetWorkSize( C );
	daoint start_a = DaoArray_GetWorkStart( A );
	daoint start_b = DaoArray_GetWorkStart( B );
	daoint start_c = DaoArray_GetWorkStart( C );
	daoint len_a = DaoArray_GetWorkIntervalSize( A );
	daoint len_b = DaoArray_GetWorkIntervalSize( B );
	daoint len_c = DaoArray_GetWorkIntervalSize( C );
	daoint step_a = DaoArray_GetWorkStep( A );
	daoint step_b = DaoArray_GetWorkStep( B );
	daoint step_c = DaoArray_GetWorkStep( C );
	daoint N = DaoArray_MatchShape( A, B );
	daoint M = C == A ? N : DaoArray_MatchShape( C, A );
	daoint i, a, b, c;
	double va, vb;

	if( N < 0 || (C->original && M != N) ){
		if( proc ) DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "not matched shape" );
		return 0;
	}
	if( op == DVM_MOVE && C->original != NULL && C->original == B->original ){
		if( size_c == size_b && start_c == start_b && step_c == step_b && len_c == len_b ){
			/*
			// A[x:y] += B
			//
			// DVM_GETI : ...
			// ......
			// DVM_ADD  : ...
			// DVM_SETI : ...
			//
			// Here the DVM_SETI instruction is redundant.
			*/
			return 1;
		}
	}
	if( A != C && C->original == NULL && M != N ){
		DaoArray_GetSliceShape( A, & C->dims, & C->ndim );
		DaoArray_ResizeArray( C, C->dims, C->ndim );
	}
	size_c = DaoArray_GetWorkSize( C );
	start_c = DaoArray_GetWorkStart( C );
	len_c = DaoArray_GetWorkIntervalSize( C );
	step_c = DaoArray_GetWorkStep( C );
	if( C->etype == A->etype && A->etype == B->etype ){
		for(i=0; i<N; ++i){
			a = start_a + (i / len_a) * step_a + (i % len_a);
			b = start_b + (i / len_b) * step_b + (i % len_b);
			c = start_c + (i / len_c) * step_c + (i % len_c);
			switch( C->etype ){
			case DAO_INTEGER :
				switch( op ){
				case DVM_MOVE : data_c->i[c] = data_b->i[b]; break;
				case DVM_ADD : data_c->i[c] = data_a->i[a] + data_b->i[b]; break;
				case DVM_SUB : data_c->i[c] = data_a->i[a] - data_b->i[b]; break;
				case DVM_MUL : data_c->i[c] = data_a->i[a] * data_b->i[b]; break;
				case DVM_DIV : data_c->i[c] = data_a->i[a] / data_b->i[b]; break;
				case DVM_MOD : data_c->i[c] = data_a->i[a] % data_b->i[b]; break;
				case DVM_POW : data_c->i[c] = powf( data_a->i[a], data_b->i[b] );break;
				default : break;
				}
				break;
			case DAO_FLOAT :
				vb = data_b->f[b];
				switch( op ){
				case DVM_MOVE : data_c->f[c] = vb; break;
				case DVM_ADD : data_c->f[c] = data_a->f[a] + vb; break;
				case DVM_SUB : data_c->f[c] = data_a->f[a] - vb; break;
				case DVM_MUL : data_c->f[c] = data_a->f[a] * vb; break;
				case DVM_DIV : data_c->f[c] = data_a->f[a] / vb; break;
				case DVM_MOD : va = data_a->f[a]; data_c->f[c] = va - vb*(daoint)(va/vb); break;
				case DVM_POW : data_c->f[c] = powf( data_a->f[a], vb );break;
				default : break;
				}
				break;
			case DAO_DOUBLE :
				vb = data_b->d[b];
				switch( op ){
				case DVM_MOVE : data_c->d[c] = vb; break;
				case DVM_ADD : data_c->d[c] = data_a->d[a] + vb; break;
				case DVM_SUB : data_c->d[c] = data_a->d[a] - vb; break;
				case DVM_MUL : data_c->d[c] = data_a->d[a] * vb; break;
				case DVM_DIV : data_c->d[c] = data_a->d[a] / vb; break;
				case DVM_MOD : va = data_a->d[a]; data_c->d[c] = va - vb*(daoint)(va/vb); break;
				case DVM_POW : data_c->d[c] = powf( data_a->d[a], vb );break;
				default : break;
				}
				break;
			case DAO_COMPLEX :
				switch( op ){
				case DVM_MOVE : data_c->c[c] = data_b->c[b]; break;
				case DVM_ADD : COM_ADD( data_c->c[c], data_a->c[a], data_b->c[b] ); break;
				case DVM_SUB : COM_SUB( data_c->c[c], data_a->c[a], data_b->c[b] ); break;
				case DVM_MUL : COM_MUL( data_c->c[c], data_a->c[a], data_b->c[b] ); break;
				case DVM_DIV : COM_DIV( data_c->c[c], data_a->c[a], data_b->c[b] ); break;
				default : break;
				}
				break;
			default : break;
			}
		}
		return 1;
	}else if( array_a->etype == DAO_INTEGER && array_b->etype == DAO_INTEGER ){
		daoint res = 0;
		for(i=0; i<N; ++i){
			a = start_a + (i / len_a) * step_a + (i % len_a);
			b = start_b + (i / len_b) * step_b + (i % len_b);
			c = start_c + (i / len_c) * step_c + (i % len_c);
			switch( op ){
			case DVM_MOVE : res = data_b->i[b]; break;
			case DVM_ADD : res = data_a->i[a] + data_b->i[b]; break;
			case DVM_SUB : res = data_a->i[a] - data_b->i[b]; break;
			case DVM_MUL : res = data_a->i[a] * data_b->i[b]; break;
			case DVM_DIV : res = data_a->i[a] / data_b->i[b]; break;
			case DVM_MOD : res = data_a->i[a] % data_b->i[b]; break;
			case DVM_POW : res = dao_powi( data_a->i[a], data_b->i[b] );break;
			default : break;
			}
			switch( C->etype ){
			case DAO_INTEGER : data_c->i[c] = res; break;
			case DAO_FLOAT   : data_c->f[c] = res; break;
			case DAO_DOUBLE  : data_c->d[c] = res; break;
			case DAO_COMPLEX : data_c->c[c].real = res; data_c->c[c].imag = 0; break;
			}
		}
		return 1;
	}
	for(i=0; i<N; ++i){
		complex16 ac, bc;
		double ad, bd;
		a = start_a + (i / len_a) * step_a + (i % len_a);
		b = start_b + (i / len_b) * step_b + (i % len_b);
		c = start_c + (i / len_c) * step_c + (i % len_c);
		switch( C->etype ){
		case DAO_INTEGER :
			ad = DaoArray_GetDouble( array_a, a );
			bd = DaoArray_GetDouble( array_b, b );
			switch( op ){
			case DVM_MOVE : data_c->i[c] = bd; break;
			case DVM_ADD : data_c->i[c] = ad + bd; break;
			case DVM_SUB : data_c->i[c] = ad - bd; break;
			case DVM_MUL : data_c->i[c] = ad * bd; break;
			case DVM_DIV : data_c->i[c] = ad / bd; break;
			case DVM_MOD : data_c->i[c] = ad - bd*(daoint)(ad/bd); break;
			case DVM_POW : data_c->i[c] = powf( ad, bd );break;
			default : break;
			}
			break;
		case DAO_FLOAT :
			ad = DaoArray_GetDouble( array_a, a );
			bd = DaoArray_GetDouble( array_b, b );
			switch( op ){
			case DVM_MOVE : data_c->f[c] = bd; break;
			case DVM_ADD : data_c->f[c] = ad + bd; break;
			case DVM_SUB : data_c->f[c] = ad - bd; break;
			case DVM_MUL : data_c->f[c] = ad * bd; break;
			case DVM_DIV : data_c->f[c] = ad / bd; break;
			case DVM_MOD : data_c->f[c] = ad - bd*(daoint)(ad/bd); break;
			case DVM_POW : data_c->f[c] = powf( ad, bd );break;
			default : break;
			}
			break;
		case DAO_DOUBLE :
			ad = DaoArray_GetDouble( array_a, a );
			bd = DaoArray_GetDouble( array_b, b );
			switch( op ){
			case DVM_MOVE : data_c->d[c] = bd; break;
			case DVM_ADD : data_c->d[c] = ad + bd; break;
			case DVM_SUB : data_c->d[c] = ad - bd; break;
			case DVM_MUL : data_c->d[c] = ad * bd; break;
			case DVM_DIV : data_c->d[c] = ad / bd; break;
			case DVM_MOD : data_c->d[c] = ad - bd*(daoint)(ad/bd); break;
			case DVM_POW : data_c->d[c] = powf( ad, bd );break;
			default : break;
			}
			break;
		case DAO_COMPLEX :
			ac = DaoArray_GetComplex( array_a, a );
			bc = DaoArray_GetComplex( array_b, b );
			switch( op ){
			case DVM_MOVE : data_c->c[c] = bc; break;
			case DVM_ADD : COM_ADD( data_c->c[c], ac, bc ); break;
			case DVM_SUB : COM_SUB( data_c->c[c], ac, bc ); break;
			case DVM_MUL : COM_MUL( data_c->c[c], ac, bc ); break;
			case DVM_DIV : COM_DIV( data_c->c[c], ac, bc ); break;
			default : break;
			}
			break;
		default : break;
		}
	}
	return 1;
}

static void DaoARRAY_BasicFunctional( DaoProcess *proc, DaoValue *p[], int npar, int funct )
{
	DaoValue com = {DAO_COMPLEX};
	DaoList *list = NULL;
	DaoArray *array = NULL;
	DaoArray *indices = NULL;
	DaoArray *self2 = (DaoArray*) p[0];
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoValue **idval = proc->activeValues + sect->a + 1;
	DaoValue *elem, *res = NULL;
	DaoArray *self = DaoArray_GetWorkArray( self2 );
	daoint size = DaoArray_GetWorkSize( self2 );
	daoint start = DaoArray_GetWorkStart( self2 );
	daoint len = DaoArray_GetWorkIntervalSize( self2 );
	daoint step = DaoArray_GetWorkStep( self2 );
	daoint *dims = self->dims;
	daoint N = DaoArray_GetWorkSize( self2 );
	daoint i, id, id2, first = 0;
	int j, D = self->ndim;
	int isvec = (D == 2 && (dims[0] ==1 || dims[1] == 1));
	int entry, vdim = sect->b - 1;
	int stackBase = proc->topFrame->active->stackBase;
	daoint *count = NULL;

	switch( funct ){
	case DVM_FUNCT_MAP :
		array = DaoProcess_PutArray( proc );
		DaoArray_GetSliceShape( self2, & array->dims, & array->ndim );
		DaoArray_ResizeArray( array, array->dims, array->ndim );
		break;
	case DVM_FUNCT_INDEX :
		list = DaoProcess_PutList( proc );
		indices = DaoArray_New( DAO_INTEGER );
		DaoArray_ResizeVector( indices, D );
		indices->trait |= DAO_VALUE_CONST;
		break;
	case DVM_FUNCT_FOLD :
		if( npar > 1 ){
			res = p[1];
		}else if( N ){
			id = start;
			res = (DaoValue*) &com;
			DaoArray_GetValue( self, id, res );
			first = 1;
		}
		DaoProcess_PutValue( proc, res );
		break;
	case DVM_FUNCT_SELECT : list = DaoProcess_PutList( proc ); break;
	case DVM_FUNCT_COUNT : count = DaoProcess_PutInteger( proc, 0 ); break;
	case DVM_FUNCT_APPLY : DaoProcess_PutReference( proc, (DaoValue*)self ); break;
	}
	if( sect == NULL ) return;
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	for(j=0; j<vdim; j++) idval[j]->xInteger.value = 0;
	DaoProcess_AcquireCV( proc );
	for(i=first; i<N; ++i){
		idval = proc->stackValues + stackBase + sect->a + 1;
		id = id2 = start + (i / len) * step + (i % len);
		if( isvec ){
			if( vdim >0 ) idval[0]->xInteger.value = id2;
			if( vdim >1 ) idval[1]->xInteger.value = id2;
		}else{
			for( j=D-1; j>=0; j--){
				daoint k = id2 % dims[j];
				id2 /= dims[j];
				if( indices ) indices->data.i[j] = k;
				if( j < vdim ) idval[j]->xInteger.value = k;
			}
		}
		if( funct == DVM_FUNCT_FOLD ) DaoProcess_SetValue( proc, sect->a+1, res );
		elem = proc->stackValues[ stackBase + sect->a ];
		if( elem == NULL || elem->type != self->etype ){
			elem = (DaoValue*)(void*) &com;
			elem->type = self->etype;
			elem = DaoProcess_SetValue( proc, sect->a, elem );
		}
		DaoArray_GetValue( self, id, elem );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		res = proc->stackValues[0];
		switch( funct ){
		case DVM_FUNCT_MAP :
			DaoArray_SetValue( array, i, res );
			break;
		case DVM_FUNCT_SELECT :
			if( ! DaoValue_IsZero( res ) ) DaoList_Append( list, elem );
			break;
		case DVM_FUNCT_INDEX :
			if( ! DaoValue_IsZero( res ) ) DaoList_Append( list, (DaoValue*)indices );
			break;
		case DVM_FUNCT_COUNT :
			*count += ! DaoValue_IsZero( res );
			break;
		case DVM_FUNCT_APPLY :
			DaoArray_SetValue( self, id, res );
			break;
		}
	}
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
	if( indices ) DaoArray_Delete( indices );
	if( funct == DVM_FUNCT_FOLD ){
		DaoProcess_SetActiveFrame( proc, proc->topFrame );
		DaoProcess_PutValue( proc, res );
	}
}
#endif /* DAO_WITH_NUMARRAY */
