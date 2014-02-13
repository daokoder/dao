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
	if( strcmp( name->mbs, "real" ) == 0 ){
		DaoProcess_PutDouble( proc, self->xComplex.value.real );
	}else if( strcmp( name->mbs, "imag" ) == 0 ){
		DaoProcess_PutDouble( proc, self->xComplex.value.imag );
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, name->mbs );
	}
}
static void DaoComplex_SetField( DaoValue *self, DaoProcess *proc, DString *name, DaoValue *value )
{
	if( strcmp( name->mbs, "real" ) == 0 ){
		self->xComplex.value.real = DaoValue_GetDouble( value );
	}else if( strcmp( name->mbs, "imag" ) == 0 ){
		self->xComplex.value.imag = DaoValue_GetDouble( value );
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, name->mbs );
	}
}
static void DaoComplex_Print( DaoValue *self, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	complex16 p = self->xComplex.value;
	DaoStream_WriteFloat( stream, p.real );
	if( p.imag >= -0.0 ) DaoStream_WriteMBS( stream, "+" );
	DaoStream_WriteFloat( stream, p.imag );
	DaoStream_WriteMBS( stream, "$" );
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

#define PI2 6.283185307179586

#define complex16_mul(z,x,y) { complex16 tmp; \
	tmp.real=x.real*y.real-x.imag*y.imag; \
	tmp.imag=x.real*y.imag+x.imag*y.real; z = tmp; }
#define complex_init(c,r,i) { c.real=r; c.imag=i; }

void dao_fft16( complex16 data[], daoint M, int inv )
{
	daoint d, i, j, k, m, S, B, D, N = 1<<M;
	double expo = PI2 / (double) N;
	complex16 wn = { 0.0, 0.0 };
	complex16 wi, wj, ws, tmp;

	wn.real = cos( 0.5 * PI2 );  wn.imag = inv * sin( 0.5 * PI2 );
	assert( abs(inv) == 1 );
	D = N >> 1;
	for(i=0; i<N; i++){ /* even/odd permutation */
		k = 0; j = i; m = D;
		while( j ){
			if( j & 0x1 ) k += m;
			j >>= 1; m >>= 1;
		}
		if( i < k ) tmp = data[k], data[k] = data[i], data[i] = tmp;
	}
	for(m=0; m<M; m++){ /* levels */
		B = 1<<m;     /* butterfly size */
		S = 1<<(m+1); /* DFT size */
		D = N>>(m+1); /* number of DFTs */
		complex_init( ws, cos( expo * D ), inv * sin( expo * D ) );
		complex_init( wi, 1.0, 0.0 );
		for(k=0; k<B; k++){ /* for k-th butterfly */
			complex16_mul( wj, wi, wn );
			for(d=0; d<D; d++){ /* in each DFT */
				i = d * S + k;  j = i + B;
				tmp = data[i];
				complex16_mul( data[i], data[j], wi );
				complex16_mul( data[j], data[j], wj );
				data[i].real += tmp.real; data[i].imag += tmp.imag;
				data[j].real += tmp.real; data[j].imag += tmp.imag;
			}
			complex16_mul( wi, wi, ws );
		}
	}
}



#ifdef DAO_WITH_NUMARRAY

enum { SLICE_RANGE, SLICE_ENUM };

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
/* in-situ index permutation: array.permute( [ 2, 0, 1 ] );
 * arrange the dim-0 as the dim-2 in the permuted array...
 * see also: share/matrix_transpose.dao
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

static int SliceRange( DVector *slice, daoint N, daoint first, daoint last )
{
	DVector_Resize( slice, 3 );
	slice->data.daoints[0] = SLICE_RANGE;
	slice->data.daoints[1] = 0;
	slice->data.daoints[2] = 0;
	if( first <0 ) first += N;
	if( last <0 ) last += N;
	if( first <0 || first >= N || last <0 || last >= N ) return 0;
	slice->data.daoints[2] = first;
	if( first <= last ) slice->data.daoints[1] = last - first + 1;
	return 1;
}
static int SliceRange2( DVector *slice, daoint N, daoint first, daoint count )
{
	DVector_Resize( slice, 3 );
	slice->data.daoints[0] = SLICE_RANGE;
	slice->data.daoints[1] = 0;
	slice->data.daoints[2] = 0;
	if( first <0 ) first += N;
	if( first <0 || first >= N ) return 0;
	slice->data.daoints[2] = first;
	if( first + count > N ) return 0;
	slice->data.daoints[1] = count;
	return 1;
}
static void MakeSlice( DaoProcess *proc, DaoValue *pid, daoint N, DVector *slice )
{
	daoint j, id, from, to, rc = 1;
	if( pid == NULL || pid->type == 0 ){
		SliceRange2( slice, N, 0, N );
		return;
	}
	switch( pid->type ){
	case DAO_INTEGER :
	case DAO_FLOAT :
	case DAO_DOUBLE :
		{
			id = DaoValue_GetInteger( pid );
			rc = SliceRange2( slice, N, id, 1 );
			break;
		}
	case DAO_TUPLE :
		{
			DaoValue **data = pid->xTuple.items;
			DVector_Clear( slice );
			if( data[0]->type == DAO_INTEGER && data[1]->type == DAO_INTEGER ){
				if( pid->xTuple.ctype == dao_type_for_iterator ){
					rc = SliceRange2( slice, N, data[1]->xInteger.value, 1 );
					data[1]->xInteger.value += 1;
					data[0]->xInteger.value = data[1]->xInteger.value < N;
				}else{
					from = data[0]->xInteger.value;
					to   = data[1]->xInteger.value;
					rc = SliceRange( slice, N, from, to );
				}
			}else if( data[0]->type == DAO_NONE && data[1]->type == DAO_NONE ){
				rc = SliceRange2( slice, N, 0, N );
			}else if( data[0]->type <= DAO_DOUBLE && data[1]->type == DAO_NONE ){
				from = DaoValue_GetInteger( data[0] );
				rc = SliceRange( slice, N, from, -1 );
			}else if( data[0]->type == DAO_NONE && data[1]->type <= DAO_DOUBLE ){
				to = DaoValue_GetInteger( data[1] );
				rc = SliceRange( slice, N, 0, to );
			}else{
				DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "need number" );
			}
			break;
		}
	case DAO_LIST :
		{
			DaoList *list = & pid->xList;
			DaoValue **v = list->items.items.pValue;
			DVector_Resize( slice, list->items.size + 2 );
			slice->data.daoints[0] = SLICE_ENUM;
			slice->data.daoints[1] = list->items.size;
			for( j=0; j<list->items.size; j++){
				if( v[j]->type < DAO_INTEGER || v[j]->type > DAO_DOUBLE )
					DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "need number" );
				id = DaoValue_GetInteger( v[j] );
				if( id <0 ) id += N;
				if( id <0 || id >= N ){
					rc = id = 0;
					break;
				}
				slice->data.daoints[j+2] = id;
			}
			break;
		}
	case DAO_ARRAY :
		{
			DaoArray *na = & pid->xArray;
			daoint *p;

			if( na->etype == DAO_COMPLEX ){
				DaoProcess_RaiseException( proc, DAO_ERROR_INDEX,
						"complex array can not be used as index" );
				break;
			}
			DVector_Resize( slice, na->size + 2 );
			slice->data.daoints[0] = SLICE_ENUM;
			slice->data.daoints[1] = na->size;
			p = slice->data.daoints + 2;
			for( j=0; j<na->size; j++){
				id = DaoArray_GetInteger( na, j );
				if( id <0 ) id += N;
				if( id <0 || id >= N ){
					rc = id = 0;
					break;
				}
				p[j] = id;
			}
			break;
		}
	default: break;
	}
	if( slice->size < 2 ) SliceRange2( slice, N, 0, N );
	if( rc == 0 ) DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, "" );
}
static int DaoArray_MakeSlice( DaoArray *self, DaoProcess *proc, DaoValue *idx[], int N, DArray *slices )
{
	DVector *tmp = DVector_New( sizeof(daoint) );
	daoint *dims = self->dims;
	daoint i, D = self->ndim;
	daoint S = D != 0;
	/* slices: DArray<DVector<int> > */
	DArray_Clear( slices );
	DVector_Resize( tmp, 3 );
	tmp->data.daoints[0] = SLICE_RANGE;
	tmp->data.daoints[2] = 0;
	for(i=0; i<D; i ++){
		tmp->data.daoints[1] = dims[i];
		DArray_Append( slices, tmp );
	}
	DVector_Delete( tmp );
	if( N == 1 ){
		if( D ==2 && ( dims[0] ==1 || dims[1] ==1 ) ){
			/* For vectors: */
			int k = dims[0] == 1;
			MakeSlice( proc, idx[0], dims[k], slices->items.pVector[k] );
		}else{
			MakeSlice( proc, idx[0], dims[0], slices->items.pVector[0] );
		}
	}else{
		const int n = N > D ? D : N;
		for( i=0; i<n; i++ ) MakeSlice( proc, idx[i], dims[i], slices->items.pVector[i] );
	}
	for(i=0; i<D; i ++) S *= slices->items.pVector[i]->data.daoints[1];
	return S;
}
int DaoArray_AlignShape( DaoArray *self, DArray *sidx, daoint *dims, int ndim )
{
	int i;
	daoint *dself = self->dims;

	if( self->ndim != ndim ) return 0;
	if( sidx ){
		for(i=0; i<ndim; i++) if( sidx->items.pVector[i]->size != dims[i] ) return 0;
	}else{
		for(i=0; i<ndim; i++) if( dself[i] != dims[i] ) return 0;
	}
	return 1;
}
static daoint DaoArray_MatchShape( DaoArray *self, DaoArray *other )
{
	DaoArray *sRef = self->original;
	DaoArray *oRef = other->original;
	daoint i, m = 0;
	if( sRef && oRef ){
		if( self->slices->size != other->slices->size ) return -1;
		m = self->slices->size != 0;
		for(i=0; i<self->slices->size; i++){
			daoint n1 = self->slices->items.pVector[i]->data.daoints[1];
			daoint n2 = other->slices->items.pVector[i]->data.daoints[1];
			if( n1 != n2 ) return -1;
			m *= n1;
		}
	}else if( sRef ){
		if( self->slices->size != other->ndim ) return -1;
		m = self->slices->size != 0;
		for(i=0; i<self->slices->size; i++){
			daoint n1 = self->slices->items.pVector[i]->data.daoints[1];
			daoint n2 = other->dims[i];
			if( n1 != n2 ) return -1;
			m *= n1;
		}
	}else if( oRef ){
		if( self->ndim != other->slices->size ) return -1;
		m = self->ndim != 0;
		for(i=0; i<self->ndim; i++){
			daoint n1 = self->dims[i];
			daoint n2 = other->slices->items.pVector[i]->data.daoints[1];
			if( n1 != n2 ) return -1;
			m *= n1;
		}
	}else{
		if( self->ndim != other->ndim ) return -1;
		m = self->ndim != 0;
		for(i=0; i<self->ndim; i++){
			daoint n1 = self->dims[i];
			daoint n2 = other->dims[i];
			if( n1 != n2 ) return -1;
			m *= n1;
		}
	}
	return m;
}
daoint DaoArray_SliceSize( DaoArray *self )
{
	DVector **vectors;
	daoint i, m, n;
	if( self->original == NULL || self->slices == NULL ) return self->size;
	vectors = self->slices->items.pVector;
	n = self->slices->size;
	for(i=0,m=(n!=0); i<n; i++) m *= vectors[i]->data.daoints[1];
	return m;
}
daoint DaoArray_GetInteger( DaoArray *na, daoint i )
{
	switch( na->etype ){
	case DAO_INTEGER: return na->data.i[i];
	case DAO_FLOAT : return na->data.f[i];
	case DAO_DOUBLE : return (float)na->data.d[i];
	case DAO_COMPLEX : return (float)na->data.c[i].real;
	default : break;
	}
	return 0;
}
float DaoArray_GetFloat( DaoArray *na, daoint i )
{
	switch( na->etype ){
	case DAO_INTEGER: return na->data.i[i];
	case DAO_FLOAT : return na->data.f[i];
	case DAO_DOUBLE : return (float)na->data.d[i];
	case DAO_COMPLEX : return (float)na->data.c[i].real;
	default : break;
	}
	return 0;
}
double DaoArray_GetDouble( DaoArray *na, daoint i )
{
	switch( na->etype ){
	case DAO_INTEGER: return na->data.i[i];
	case DAO_FLOAT : return na->data.f[i];
	case DAO_DOUBLE : return na->data.d[i];
	case DAO_COMPLEX : return na->data.c[i].real;
	default : break;
	}
	return 0;
}
complex16 DaoArray_GetComplex( DaoArray *na, daoint i )
{
	complex16 com = {0,0};
	switch( na->etype ){
	case DAO_INTEGER : com.real = na->data.i[i]; break;
	case DAO_FLOAT : com.real = na->data.f[i]; break;
	case DAO_DOUBLE : com.real = na->data.d[i]; break;
	case DAO_COMPLEX : return na->data.c[i];
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

daoint DaoArray_IndexFromSlice( DaoArray *self, DArray *slices, daoint sid );
int DaoArray_number_op_array( DaoArray *C, DaoValue *A, DaoArray *B, short op, DaoProcess *proc );
int DaoArray_array_op_number( DaoArray *C, DaoArray *A, DaoValue *B, short op, DaoProcess *proc );
int DaoArray_ArrayArith( DaoArray *s, DaoArray *l, DaoArray *r, short p, DaoProcess *c );
static void DaoArray_Print( DaoValue *value, DaoProcess *proc, DaoStream *stream, DMap *cycData );

static void DaoArray_GetItem1( DaoValue *value, DaoProcess *proc, DaoValue *pid )
{
	DaoArray *na, *self = & value->xArray;
	/* if( self->ctype ) printf( "DaoArray_GetItem: %s\n", self->ctype->name->mbs ); */

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
		DaoValue **data = pid->xTuple.items;
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
	if( na->slices == NULL ) na->slices = DArray_New(D_VECTOR);
	DaoArray_MakeSlice( self, proc, & pid, 1, na->slices );
}
int DaoArray_CopyArray( DaoArray *self, DaoArray *other )
{
	daoint i, N = self->size;
	if( DaoArray_AlignShape( self, NULL, other->dims, other->ndim ) ==0 ) return 0;
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
			DaoArray_ArrayArith( self, self, & value->xArray, DVM_MOVE, proc );
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
	if( self->slices == NULL ) self->slices = DArray_New(D_VECTOR);
	DaoArray_MakeSlice( self, proc, & pid, 1, self->slices );
	self->original = self;
	if( value->type == DAO_ARRAY ){
		DaoArray *na = & value->xArray;
		DaoArray_ArrayArith( self, self, na, DVM_MOVE, proc );
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
	if( na->slices == NULL ) na->slices = DArray_New(D_VECTOR);
	DaoArray_MakeSlice( self, proc, ids, N, na->slices );
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
	if( self->slices == NULL ) self->slices = DArray_New(D_VECTOR);
	DaoArray_MakeSlice( self, proc, ids, N, self->slices );
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
		if( self->data.c[i].imag >= -0.0 ) DaoStream_WriteMBS( stream, "+" );
		DaoStream_WriteFloat( stream, self->data.c[i].imag );
		DaoStream_WriteMBS( stream, "$" );
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
		DaoStream_WriteMBS( stream, "[ " );
		for(i=0; i<self->size; i++){
			DaoArray_PrintElement( self, stream, i );
			if( i+1 < self->size ) DaoStream_WriteMBS( stream, sep );
		}
		DaoStream_WriteMBS( stream, " ]" );
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
				DaoStream_WriteMBS( stream, "row[" );
				for(j=0; j+1<self->ndim; j++){
					DaoStream_WriteFormatedInt( stream, (int)tmp[j], "%i" );
					DaoStream_WriteMBS( stream, "," );
				}
				DaoStream_WriteMBS( stream, ":]:\t" );
			}
			DaoArray_PrintElement( self, stream, i );
			if( i+1 < self->size ) DaoStream_WriteMBS( stream, "\t" );
			if( tmp[self->ndim-1] +1 == dims[self->ndim-1] )
				DaoStream_WriteMBS( stream, "\n" );
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
			DaoArray_ResizeVector( na, self->slices->size );
			v = na->data.i;
			for(i=0; i<self->slices->size; i++ )
				v[i] = self->slices->items.pVector[i]->data.daoints[1];
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
			*num = self->slices->items.pVector[i]->data.daoints[1];
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
	DaoProcess_PutInteger( proc, DaoArray_SliceSize( self ) );
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
	DaoArray *self = & par[0]->xArray;
	daoint i, k, size, cmp=0, imax = -1;

	if( self->etype == DAO_COMPLEX ) return;/* no exception, guaranteed by the typing system */
	if( (size = DaoArray_SliceSize( self )) == 0 ) return;
	if( self->original && self->slices ){
		DArray *slices = self->slices;
		self = self->original;
		if( size == 0 ) return;
		imax = DaoArray_IndexFromSlice( self, slices, 0 );
		for(i=1; i<size; i++ ){
			k = DaoArray_IndexFromSlice( self, slices, i );
			switch( self->etype ){
			case DAO_INTEGER : cmp = self->data.i[imax] < self->data.i[k]; break;
			case DAO_FLOAT  : cmp = self->data.f[imax] < self->data.f[k]; break;
			case DAO_DOUBLE : cmp = self->data.d[imax] < self->data.d[k]; break;
			default : break;
			}
			if( cmp ) imax = k;
		}
	}else{
		imax = 0;
		for(i=1; i<self->size; i++ ){
			switch( self->etype ){
			case DAO_INTEGER : cmp = self->data.i[imax] < self->data.i[i]; break;
			case DAO_FLOAT  : cmp = self->data.f[imax] < self->data.f[i]; break;
			case DAO_DOUBLE : cmp = self->data.d[imax] < self->data.d[i]; break;
			default : break;
			}
			if( cmp ) imax = i;
		}
	}
	tuple->items[1]->xInteger.value = imax;
	switch( self->etype ){
	case DAO_INTEGER : tuple->items[0]->xInteger.value = self->data.i[imax]; break;
	case DAO_FLOAT  : tuple->items[0]->xFloat.value = self->data.f[imax]; break;
	case DAO_DOUBLE : tuple->items[0]->xDouble.value = self->data.d[imax]; break;
	default : break;
	}
}
static void DaoARRAY_min( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoTuple *tuple = DaoProcess_PutTuple( proc, 0 );
	DaoArray *self = & par[0]->xArray;
	daoint i, k, size, cmp=0, imax = -1;

	if( self->etype == DAO_COMPLEX ) return;
	if( (size = DaoArray_SliceSize( self )) == 0 ) return;
	if( self->original && self->slices ){
		DArray *slices = self->slices;
		self = self->original;
		if( size == 0 ) return;
		imax = DaoArray_IndexFromSlice( self, slices, 0 );
		for(i=1; i<size; i++ ){
			k = DaoArray_IndexFromSlice( self, slices, i );
			switch( self->etype ){
			case DAO_INTEGER : cmp = self->data.i[imax] > self->data.i[k]; break;
			case DAO_FLOAT  : cmp = self->data.f[imax] > self->data.f[k]; break;
			case DAO_DOUBLE : cmp = self->data.d[imax] > self->data.d[k]; break;
			default : break;
			}
			if( cmp ) imax = k;
		}
	}else{
		imax = 0;
		for(i=1; i<self->size; i++ ){
			switch( self->etype ){
			case DAO_INTEGER : cmp = self->data.i[imax] > self->data.i[i]; break;
			case DAO_FLOAT  : cmp = self->data.f[imax] > self->data.f[i]; break;
			case DAO_DOUBLE : cmp = self->data.d[imax] > self->data.d[i]; break;
			default : break;
			}
			if( cmp ) imax = i;
		}
	}
	tuple->items[1]->xInteger.value = imax;
	switch( self->etype ){
	case DAO_INTEGER : tuple->items[0]->xInteger.value = self->data.i[imax]; break;
	case DAO_FLOAT  : tuple->items[0]->xFloat.value = self->data.f[imax]; break;
	case DAO_DOUBLE : tuple->items[0]->xDouble.value = self->data.d[imax]; break;
	default : break;
	}
}
static void DaoARRAY_sum( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoArray *self = & par[0]->xArray;
	daoint i, size;
	if( self->original && self->slices ){
		DArray *slices = self->slices;
		size = DaoArray_SliceSize( self );
		self = self->original;
		if( size == 0 ) return;
		if( self->etype == DAO_INTEGER ){
			daoint sum = 0;
			daoint *v = self->data.i;
			for(i=0; i<size; i++ ) sum += v[ DaoArray_IndexFromSlice( self, slices, i ) ];
			DaoProcess_PutInteger( proc, sum );
		}else if( self->etype == DAO_FLOAT ){
			double sum = 0;
			float *v = self->data.f;
			for(i=0; i<size; i++ ) sum += v[ DaoArray_IndexFromSlice( self, slices, i ) ];
			DaoProcess_PutFloat( proc, sum );
		}else if( self->etype == DAO_DOUBLE ){
			double sum = 0;
			double *v = self->data.d;
			for(i=0; i<size; i++ ) sum += v[ DaoArray_IndexFromSlice( self, slices, i ) ];
			DaoProcess_PutDouble( proc, sum );
		}else{
			complex16 sum = {0,0};
			complex16 *v = self->data.c;
			for(i=0; i<size; i++ ){
				complex16 x = v[ DaoArray_IndexFromSlice( self, slices, i ) ];
				COM_IP_ADD( sum, x );
			}
			DaoProcess_PutComplex( proc, sum );
		}
	}else{
		if( self->etype == DAO_INTEGER ){
			daoint sum = 0;
			daoint *v = self->data.i;
			for(i=0; i<self->size; i++ ) sum += v[i];
			DaoProcess_PutInteger( proc, sum );
		}else if( self->etype == DAO_FLOAT ){
			double sum = 0;
			float *v = self->data.f;
			for(i=0; i<self->size; i++ ) sum += v[i];
			DaoProcess_PutDouble( proc, sum );
		}else if( self->etype == DAO_DOUBLE ){
			double sum = 0;
			double *v = self->data.d;
			for(i=0; i<self->size; i++ ) sum += v[i];
			DaoProcess_PutDouble( proc, sum );
		}else{
			complex16 sum = {0,0};
			complex16 *v = self->data.c;
			for(i=0; i<self->size; i++ ) COM_IP_ADD( sum, v[i] );
			DaoProcess_PutComplex( proc, sum );
		}
	}
}
static void DaoARRAY_varn( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoArray *self = & par[0]->xArray;
	double *num = DaoProcess_PutDouble( proc, 0.0 );
	double e = 0.0;
	double sum = 0;
	double sum2 = 0;
	double dev;
	daoint i, size;

	if( self->etype == DAO_COMPLEX ) return;
	if( (size = DaoArray_SliceSize( self )) ==0 ) return;
	if( self->original && self->slices ){
		DArray *slices = self->slices;
		self = self->original;
		if( size > 0 ){
			for(i=0; i<size; i++ ){
				daoint k = DaoArray_IndexFromSlice( self, slices, i );
				switch( self->etype ){
				case DAO_INTEGER : e = self->data.i[k]; break;
				case DAO_FLOAT   : e = self->data.f[k]; break;
				case DAO_DOUBLE  : e = self->data.d[k]; break;
				default : break;
				}
				sum += e;
				sum2 += e * e;
			}
			dev = sum2 - sum * sum / size;
			*num = dev / size;
		}
	}else{
		for(i=0; i<self->size; i++ ){
			switch( self->etype ){
			case DAO_INTEGER : e = self->data.i[i]; break;
			case DAO_FLOAT   : e = self->data.f[i]; break;
			case DAO_DOUBLE  : e = self->data.d[i]; break;
			default : break;
			}
			sum += e;
			sum2 += e * e;
		}
		dev = sum2 - sum * sum / self->size;
		*num = dev / self->size;
	}
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
	DaoArray *array = & par[0]->xArray;
	DaoArray *original = array->original;
	DArray *slices = array->slices;
	daoint part = par[2]->xInteger.value;
	daoint i, N = DaoArray_SliceSize( array );
	daoint *index;
	daoint *ids;

	if( res == NULL ) return;
	if( N == 0 ) return;
	DaoArray_SetNumType( res, DAO_INTEGER );
	DaoArray_GetSliceShape( array, & res->dims, & res->ndim );
	DaoArray_ResizeArray( res, res->dims, res->ndim );
	ids = res->data.i;
	for(i=0; i<N; i++) ids[i] = i;

	if( N < 2 ) return;
	if( part ==0 ) part = N;
	index = (daoint*) dao_malloc( N * sizeof(daoint) );
	for(i=0; i<N; i++){
		index[i] = i;
		ids[i] = original ? DaoArray_IndexFromSlice( original, slices, i ) : i;
	}
	QuickSort2( original ? original : array, ids, index, 0, N-1, part, (par[1]->xEnum.value == 0) );
	for(i=0; i<N; i++) ids[i] = index[i];
	dao_free( index );
}
static void DaoARRAY_sort( DaoProcess *proc, DaoValue *par[], int npar )
{
	DaoArray *array = & par[0]->xArray;
	DaoArray *original = array->original;
	DArray *slices = array->slices;
	daoint part = par[2]->xInteger.value;
	daoint i, N = DaoArray_SliceSize( array );
	daoint *index;

	DaoProcess_PutValue( proc, par[0] );
	if( N < 2 ) return;
	if( part ==0 ) part = N;
	index = (daoint*) dao_malloc( N * sizeof(daoint) );
	for(i=0; i<N; i++) index[i] = original ? DaoArray_IndexFromSlice( original, slices, i ) : i;
	QuickSort2( original ? original : array, index, NULL, 0, N-1, part, (par[1]->xEnum.value == 0) );
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
	{ DaoARRAY_Dim,       "dim( self :array<@T>, i : int )=>int" },
	{ DaoARRAY_Dim,       "dim( self :array<@T> )=>array<int>" },
	{ DaoARRAY_Index,     "index( self :array<@T>, i : int )=>array<int>" },
	{ DaoARRAY_Size,      "size( self :array<@T> )=>int" },
	{ DaoARRAY_Resize,    "resize( self :array<@T>, dims :array<int> )" },
	{ DaoARRAY_Reshape,   "reshape( self :array<@T>, dims :array<int> )" },

	{ DaoARRAY_Permute,   "permute( self :array<@T>, dims :array<int> )" },
	{ DaoARRAY_Transpose, "transpose( self :array )" },
	{ DaoARRAY_max,       "max( self :array<@T> )=>tuple<@T,int>" },
	{ DaoARRAY_min,       "min( self :array<@T> )=>tuple<@T,int>" },
	{ DaoARRAY_sum,       "sum( self :array<@T> )=>@T" },
	{ DaoARRAY_varn,      "varn( self :array<@T> )=>double" },
	{ DaoARRAY_Reverse,   "reverse( self :array<@T> )=>array<@T>" },
	{ DaoARRAY_rank,  "rank( self :array<@T>, order :enum<ascend,descend>=$ascend, k=0 )=>array<int>" },
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
int DaoArray_GetFlatIndex( DaoArray *self, daoint *index )
{
	int i, id = 0;
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

#ifdef DEBUG
static int array_count = 0;
#endif

DaoArray* DaoArray_New( int etype )
{
	DaoArray* self = (DaoArray*) dao_calloc( 1, sizeof( DaoArray ) );
	DaoValue_Init( self, DAO_ARRAY );
	self->etype = etype;
	self->owner = 1;
	DaoArray_ResizeVector( self, 0 );
#ifdef DEBUG
	array_count ++;
#endif
	return self;
}
void DaoArray_Delete( DaoArray *self )
{
	if( self->dims ) dao_free( self->dims );
	if( self->owner && self->data.p ) dao_free( self->data.p );
	if( self->slices ) DArray_Delete( self->slices );
	if( self->original ) GC_DecRC( self->original );
	dao_free( self );
#ifdef DEBUG
	array_count --;
#endif
}
void DaoArray_UseData( DaoArray *self, void *data )
{
	if( self->data.p ) dao_free( self->data.p );
	self->data.p = data;
	self->owner = 0;
}
daoint DaoArray_IndexFromSlice( DaoArray *self, DArray *slices, daoint sid )
/* sid: plain index in the slicesd array */
{
	daoint *dimAccum = self->dims + self->ndim;
	daoint j, index = 0;
	for( j=(int)slices->size; j>0; j-- ){
		DVector *sub = slices->items.pVector[j-1];
		daoint *ids = sub->data.daoints; /* { type, count, ... } */
		daoint count = ids[1];
		daoint res = sid % count;
		if( ids[0] == SLICE_RANGE ){
			index += (ids[2] + res) * dimAccum[j-1];
		}else{
			index += ids[ res + 2 ] * dimAccum[j-1];
		}
		sid /= count;
	}
	return index;
}
void DaoArray_GetSliceShape( DaoArray *self, daoint **dims, short *ndim )
{
	DArray *shape;
	DArray *slices = self->slices;
	daoint i, k, S = 0, D = self->ndim;
	if( self->original == NULL ){
		if( *ndim != D ) *dims = (daoint*) dao_realloc( *dims, 2*D*sizeof(daoint) );
		*ndim = self->ndim;
		memmove( *dims, self->dims, self->ndim * sizeof(daoint) );
		return;
	}
	*ndim = 0;
	if( slices->size != self->original->ndim ) return;
	for(i=0; i<slices->size; i++){
		k = slices->items.pVector[i]->data.daoints[1];
		if( k ==0 ) return; /* skip empty dimension */
		S += k > 1;
	}
	shape = DArray_New(0);
	for(i=0; i<slices->size; i++){
		k = slices->items.pVector[i]->data.daoints[1];
		/* skip size one dimension if the final slice has at least two dimensions */
		if( k == 1 && (S > 1 || shape->size > 1) ) continue;
		DArray_Append( shape, k );
	}
	*ndim = shape->size;
	*dims = (daoint*) dao_realloc( *dims, shape->size * sizeof(daoint) );
	memmove( *dims, shape->items.pInt, shape->size * sizeof(daoint) );
	DArray_Delete( shape );
}
int DaoArray_SliceFrom( DaoArray *self, DaoArray *original, DArray *slices )
{
	daoint i, D = 0, S = 0;
	daoint k;
	if( slices == NULL ){
		DaoArray_ResizeArray( self, original->dims, original->ndim );
		DaoArray_CopyArray( self, original );
		return 1;
	}
	if( slices->size != original->ndim ) return 0;
	for(i=0; i<slices->size; i++){
		k = slices->items.pVector[i]->data.daoints[1];
		S += k > 1;
		if( k ==0 ){ /* skip empty dimension */
			DaoArray_ResizeVector( self, 0 );
			return 1;
		}
	}
	DaoArray_SetDimCount( self, slices->size );
	for(i=0; i<slices->size; i++){
		k = slices->items.pVector[i]->data.daoints[1];
		/* skip size one dimension if the final slice has at least two dimensions */
		if( k == 1 && (S > 1 || D > 1) ) continue;
		self->dims[D++] = k;
	}
	DaoArray_ResizeArray( self, self->dims, D );

	for(i=0; i<self->size; i++){
		daoint j = DaoArray_IndexFromSlice( original, slices, i );
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
	DArray *slices = self->slices;

	if( slices == NULL || original == NULL ) goto ReturnFalse;
	if( self->etype != original->etype ) goto ReturnFalse;
	if( slices->size != original->ndim ) goto ReturnFalse;
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
	int i, k;
	daoint old = self->size;
	if( D == 1 ){
		DaoArray_ResizeVector( self, dims[0] );
		return;
	}
	k = 0;
	for(i=0; i<D; i++){
		if( dims[i] == 0 ){
			DaoArray_ResizeVector( self, 0 );
			return;
		}
		if( dims[i] != 1 || D ==2 ) k ++;
	}
	if( self->dims != dims || self->ndim != k ) DaoArray_SetDimCount( self, k );
	k = 0;
	for(i=0; i<D; i++){
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
	DaoArray *rB = B->original;
	DaoArray *rC = C->original;
	DaoArray *dB = rB ? rB : B;
	DaoArray *dC = rC ? rC : C;
	daoint i, b, c, N = DaoArray_UpdateShape( C, B );
	double bf, af = DaoValue_GetDouble( A );
	complex16 bc, ac = {0.0, 0.0};

	ac.real = af;
	if( A->type == DAO_COMPLEX ) ac = A->xComplex.value;
	if( N < 0 ){
		if( proc ) DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "not matched shape" );
		return 0;
	}
	if( dB->etype == DAO_INTEGER && A->type == DAO_INTEGER ){
		daoint bi, ci = 0, ai = A->xInteger.value;
		for(i=0; i<N; i++){
			c = rC ? DaoArray_IndexFromSlice( rC, C->slices, i ) : i;
			b = rB ? DaoArray_IndexFromSlice( rB, B->slices, i ) : i;
			bi = dB->data.i[b];
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
			case DAO_INTEGER : dC->data.i[c] = ci; break;
			case DAO_FLOAT   : dC->data.f[c] = ci; break;
			case DAO_DOUBLE  : dC->data.d[c] = ci; break;
			case DAO_COMPLEX : dC->data.c[c].real = ci; dC->data.c[c].imag = 0; break;
			}
		}
		return 1;
	}
	for(i=0; i<N; i++){
		c = rC ? DaoArray_IndexFromSlice( rC, C->slices, i ) : i;
		b = rB ? DaoArray_IndexFromSlice( rB, B->slices, i ) : i;
		switch( C->etype ){
		case DAO_INTEGER :
			bf = DaoArray_GetDouble( dB, b );
			switch( op ){
			case DVM_MOVE : dC->data.i[c] = bf; break;
			case DVM_ADD : dC->data.i[c] = af + bf; break;
			case DVM_SUB : dC->data.i[c] = af - bf; break;
			case DVM_MUL : dC->data.i[c] = af * bf; break;
			case DVM_DIV : dC->data.i[c] = af / bf; break;
			case DVM_MOD : dC->data.i[c] = af - bf*(daoint)(af / bf); break;
			case DVM_POW : dC->data.i[c] = powf( af, bf );break;
			default : break;
			}
			break;
		case DAO_FLOAT :
			bf = DaoArray_GetDouble( dB, b );
			switch( op ){
			case DVM_MOVE : dC->data.f[c] = bf; break;
			case DVM_ADD : dC->data.f[c] = af + bf; break;
			case DVM_SUB : dC->data.f[c] = af - bf; break;
			case DVM_MUL : dC->data.f[c] = af * bf; break;
			case DVM_DIV : dC->data.f[c] = af / bf; break;
			case DVM_MOD : dC->data.f[c] = af - bf*(daoint)(af / bf); break;
			case DVM_POW : dC->data.f[c] = powf( af, bf );break;
			default : break;
			}
			break;
		case DAO_DOUBLE :
			bf = DaoArray_GetDouble( dB, b );
			switch( op ){
			case DVM_MOVE : dC->data.d[c] = bf; break;
			case DVM_ADD : dC->data.d[c] = af + bf; break;
			case DVM_SUB : dC->data.d[c] = af - bf; break;
			case DVM_MUL : dC->data.d[c] = af * bf; break;
			case DVM_DIV : dC->data.d[c] = af / bf; break;
			case DVM_MOD : dC->data.d[c] = af - bf*(daoint)(af / bf); break;
			case DVM_POW : dC->data.d[c] = powf( af, bf );break;
			default : break;
			}
			break;
		case DAO_COMPLEX :
			bc = DaoArray_GetComplex( dB, b );
			switch( op ){
			case DVM_MOVE : dC->data.c[c] = bc; break;
			case DVM_ADD : COM_ADD( dC->data.c[c], ac, bc ); break;
			case DVM_SUB : COM_SUB( dC->data.c[c], ac, bc ); break;
			case DVM_MUL : COM_MUL( dC->data.c[c], ac, bc ); break;
			case DVM_DIV : COM_DIV( dC->data.c[c], ac, bc ); break;
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
	DaoArray *rA = A->original;
	DaoArray *rC = C->original;
	DaoArray *dA = rA ? rA : A;
	DaoArray *dC = rC ? rC : C;
	daoint i, a, c, N = DaoArray_UpdateShape( C, A );
	double af, bf = DaoValue_GetDouble( B );
	daoint ai, ci = 0, bi = DaoValue_GetInteger( B );
	complex16 ac, bc = {0.0, 0.0};

	bc.real = bf;
	if( B->type == DAO_COMPLEX ) bc = B->xComplex.value;
	if( N < 0 ){
		if( proc ) DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "not matched shape" );
		return 0;
	}
	if( dA->etype == DAO_INTEGER && B->type == DAO_INTEGER ){
		for(i=0; i<N; i++){
			c = rC ? DaoArray_IndexFromSlice( rC, C->slices, i ) : i;
			a = A == C ? c : (rA ? DaoArray_IndexFromSlice( rA, A->slices, i ) : i);
			ai = dA->data.i[a];
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
			case DAO_INTEGER : dC->data.i[c] = ci; break;
			case DAO_FLOAT   : dC->data.f[c] = ci; break;
			case DAO_DOUBLE  : dC->data.d[c] = ci; break;
			case DAO_COMPLEX : dC->data.c[c].real = ci; dC->data.c[c].imag = 0; break;
			}
		}
		return 1;
	}
	for(i=0; i<N; i++){
		c = rC ? DaoArray_IndexFromSlice( rC, C->slices, i ) : i;
		a = A == C ? c : (rA ? DaoArray_IndexFromSlice( rA, A->slices, i ) : i);
		switch( C->etype ){
		case DAO_INTEGER :
			af = DaoArray_GetDouble( dA, a );
			switch( op ){
			case DVM_MOVE : dC->data.i[c] = bf; break;
			case DVM_ADD : dC->data.i[c] = af + bf; break;
			case DVM_SUB : dC->data.i[c] = af - bf; break;
			case DVM_MUL : dC->data.i[c] = af * bf; break;
			case DVM_DIV : dC->data.i[c] = af / bf; break;
			case DVM_MOD : dC->data.i[c] = af - bf*(daoint)(af / bf); break;
			case DVM_POW : dC->data.i[c] = pow( af, bf );break;
			default : break;
			}
			break;
		case DAO_FLOAT :
			af = DaoArray_GetDouble( dA, a );
			switch( op ){
			case DVM_MOVE : dC->data.f[c] = bf; break;
			case DVM_ADD : dC->data.f[c] = af + bf; break;
			case DVM_SUB : dC->data.f[c] = af - bf; break;
			case DVM_MUL : dC->data.f[c] = af * bf; break;
			case DVM_DIV : dC->data.f[c] = af / bf; break;
			case DVM_MOD : dC->data.f[c] = af - bf*(daoint)(af / bf); break;
			case DVM_POW : dC->data.f[c] = powf( af, bf );break;
			default : break;
			}
			break;
		case DAO_DOUBLE :
			af = DaoArray_GetDouble( dA, a );
			switch( op ){
			case DVM_MOVE : dC->data.d[c] = bf; break;
			case DVM_ADD : dC->data.d[c] = af + bf; break;
			case DVM_SUB : dC->data.d[c] = af - bf; break;
			case DVM_MUL : dC->data.d[c] = af * bf; break;
			case DVM_DIV : dC->data.d[c] = af / bf; break;
			case DVM_MOD : dC->data.d[c] = af - bf*(daoint)(af / bf); break;
			case DVM_POW : dC->data.d[c] = pow( af, bf );break;
			default : break;
			}
			break;
		case DAO_COMPLEX :
			ac = DaoArray_GetComplex( dA, a );
			switch( op ){
			case DVM_MOVE : dC->data.c[c] = bc; break;
			case DVM_ADD : COM_ADD( dC->data.c[c], ac, bc ); break;
			case DVM_SUB : COM_SUB( dC->data.c[c], ac, bc ); break;
			case DVM_MUL : COM_MUL( dC->data.c[c], ac, bc ); break;
			case DVM_DIV : COM_DIV( dC->data.c[c], ac, bc ); break;
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
	DaoArray *rA = A->original;
	DaoArray *rB = B->original;
	DaoArray *rC = C->original;
	DaoArray *dA = rA ? rA : A;
	DaoArray *dB = rB ? rB : B;
	DaoArray *dC = rC ? rC : C;
	daoint N = DaoArray_MatchShape( A, B );
	daoint M = C == A ? N : DaoArray_MatchShape( C, A );
	daoint i, a, b, c;
	double va, vb;
	if( N < 0 || (C->original && M != N) ){
		if( proc ) DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "not matched shape" );
		return 0;
	}
	if( A != C && C->original == NULL && M != N ){
		DaoArray_GetSliceShape( A, & C->dims, & C->ndim );
		DaoArray_ResizeArray( C, C->dims, C->ndim );
	}
	if( C->etype == A->etype && A->etype == B->etype ){
		for(i=0; i<N; i++){
			c = rC ? DaoArray_IndexFromSlice( rC, C->slices, i ) : i;
			b = rB ? DaoArray_IndexFromSlice( rB, B->slices, i ) : i;
			a = A == C ? c : (rA ? DaoArray_IndexFromSlice( rA, A->slices, i ) : i);
			switch( C->etype ){
			case DAO_INTEGER :
				switch( op ){
				case DVM_MOVE : dC->data.i[c] = dB->data.i[b]; break;
				case DVM_ADD : dC->data.i[c] = dA->data.i[a] + dB->data.i[b]; break;
				case DVM_SUB : dC->data.i[c] = dA->data.i[a] - dB->data.i[b]; break;
				case DVM_MUL : dC->data.i[c] = dA->data.i[a] * dB->data.i[b]; break;
				case DVM_DIV : dC->data.i[c] = dA->data.i[a] / dB->data.i[b]; break;
				case DVM_MOD : dC->data.i[c] = dA->data.i[a] % dB->data.i[b]; break;
				case DVM_POW : dC->data.i[c] = powf( dA->data.i[a], dB->data.i[b] );break;
				default : break;
				}
				break;
			case DAO_FLOAT :
				vb = dB->data.f[b];
				switch( op ){
				case DVM_MOVE : dC->data.f[c] = vb; break;
				case DVM_ADD : dC->data.f[c] = dA->data.f[a] + vb; break;
				case DVM_SUB : dC->data.f[c] = dA->data.f[a] - vb; break;
				case DVM_MUL : dC->data.f[c] = dA->data.f[a] * vb; break;
				case DVM_DIV : dC->data.f[c] = dA->data.f[a] / vb; break;
				case DVM_MOD : va = dA->data.f[a]; dC->data.f[c] = va - vb*(daoint)(va/vb); break;
				case DVM_POW : dC->data.f[c] = powf( dA->data.f[a], vb );break;
				default : break;
				}
				break;
			case DAO_DOUBLE :
				vb = dB->data.d[b];
				switch( op ){
				case DVM_MOVE : dC->data.d[c] = vb; break;
				case DVM_ADD : dC->data.d[c] = dA->data.d[a] + vb; break;
				case DVM_SUB : dC->data.d[c] = dA->data.d[a] - vb; break;
				case DVM_MUL : dC->data.d[c] = dA->data.d[a] * vb; break;
				case DVM_DIV : dC->data.d[c] = dA->data.d[a] / vb; break;
				case DVM_MOD : va = dA->data.d[a]; dC->data.d[c] = va - vb*(daoint)(va/vb); break;
				case DVM_POW : dC->data.d[c] = powf( dA->data.d[a], vb );break;
				default : break;
				}
				break;
			case DAO_COMPLEX :
				switch( op ){
				case DVM_MOVE : dC->data.c[c] = dB->data.c[b]; break;
				case DVM_ADD : COM_ADD( dC->data.c[c], dA->data.c[a], dB->data.c[b] ); break;
				case DVM_SUB : COM_SUB( dC->data.c[c], dA->data.c[a], dB->data.c[b] ); break;
				case DVM_MUL : COM_MUL( dC->data.c[c], dA->data.c[a], dB->data.c[b] ); break;
				case DVM_DIV : COM_DIV( dC->data.c[c], dA->data.c[a], dB->data.c[b] ); break;
				default : break;
				}
				break;
			default : break;
			}
		}
		return 1;
	}else if( dA->etype == DAO_INTEGER && dB->etype == DAO_INTEGER ){
		daoint res = 0;
		for(i=0; i<N; i++){
			c = rC ? DaoArray_IndexFromSlice( rC, C->slices, i ) : i;
			b = rB ? DaoArray_IndexFromSlice( rB, B->slices, i ) : i;
			a = A == C ? c : (rA ? DaoArray_IndexFromSlice( rA, A->slices, i ) : i);
			switch( op ){
			case DVM_MOVE : res = dB->data.i[b]; break;
			case DVM_ADD : res = dA->data.i[a] + dB->data.i[b]; break;
			case DVM_SUB : res = dA->data.i[a] - dB->data.i[b]; break;
			case DVM_MUL : res = dA->data.i[a] * dB->data.i[b]; break;
			case DVM_DIV : res = dA->data.i[a] / dB->data.i[b]; break;
			case DVM_MOD : res = dA->data.i[a] % dB->data.i[b]; break;
			case DVM_POW : res = dao_powi( dA->data.i[a], dB->data.i[b] );break;
			default : break;
			}
			switch( C->etype ){
			case DAO_INTEGER : dC->data.i[c] = res; break;
			case DAO_FLOAT   : dC->data.f[c] = res; break;
			case DAO_DOUBLE  : dC->data.d[c] = res; break;
			case DAO_COMPLEX : dC->data.c[c].real = res; dC->data.c[c].imag = 0; break;
			}
		}
		return 1;
	}
	for(i=0; i<N; i++){
		complex16 ac, bc;
		double ad, bd;
		c = rC ? DaoArray_IndexFromSlice( rC, C->slices, i ) : i;
		b = rB ? DaoArray_IndexFromSlice( rB, B->slices, i ) : i;
		a = A == C ? c : (rA ? DaoArray_IndexFromSlice( rA, A->slices, i ) : i);
		switch( C->etype ){
		case DAO_INTEGER :
			ad = DaoArray_GetDouble( dA, a );
			bd = DaoArray_GetDouble( dB, b );
			switch( op ){
			case DVM_MOVE : dC->data.i[c] = bd; break;
			case DVM_ADD : dC->data.i[c] = ad + bd; break;
			case DVM_SUB : dC->data.i[c] = ad - bd; break;
			case DVM_MUL : dC->data.i[c] = ad * bd; break;
			case DVM_DIV : dC->data.i[c] = ad / bd; break;
			case DVM_MOD : dC->data.i[c] = ad - bd*(daoint)(ad/bd); break;
			case DVM_POW : dC->data.i[c] = powf( ad, bd );break;
			default : break;
			}
			break;
		case DAO_FLOAT :
			ad = DaoArray_GetDouble( dA, a );
			bd = DaoArray_GetDouble( dB, b );
			switch( op ){
			case DVM_MOVE : dC->data.f[c] = bd; break;
			case DVM_ADD : dC->data.f[c] = ad + bd; break;
			case DVM_SUB : dC->data.f[c] = ad - bd; break;
			case DVM_MUL : dC->data.f[c] = ad * bd; break;
			case DVM_DIV : dC->data.f[c] = ad / bd; break;
			case DVM_MOD : dC->data.f[c] = ad - bd*(daoint)(ad/bd); break;
			case DVM_POW : dC->data.f[c] = powf( ad, bd );break;
			default : break;
			}
			break;
		case DAO_DOUBLE :
			ad = DaoArray_GetDouble( dA, a );
			bd = DaoArray_GetDouble( dB, b );
			switch( op ){
			case DVM_MOVE : dC->data.d[c] = bd; break;
			case DVM_ADD : dC->data.d[c] = ad + bd; break;
			case DVM_SUB : dC->data.d[c] = ad - bd; break;
			case DVM_MUL : dC->data.d[c] = ad * bd; break;
			case DVM_DIV : dC->data.d[c] = ad / bd; break;
			case DVM_MOD : dC->data.d[c] = ad - bd*(daoint)(ad/bd); break;
			case DVM_POW : dC->data.d[c] = powf( ad, bd );break;
			default : break;
			}
			break;
		case DAO_COMPLEX :
			ac = DaoArray_GetComplex( dA, a );
			bc = DaoArray_GetComplex( dB, b );
			switch( op ){
			case DVM_MOVE : dC->data.c[c] = bc; break;
			case DVM_ADD : COM_ADD( dC->data.c[c], ac, bc ); break;
			case DVM_SUB : COM_SUB( dC->data.c[c], ac, bc ); break;
			case DVM_MUL : COM_MUL( dC->data.c[c], ac, bc ); break;
			case DVM_DIV : COM_DIV( dC->data.c[c], ac, bc ); break;
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
	DaoArray *self2 = & p[0]->xArray;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoValue **idval = proc->activeValues + sect->a + 1;
	DaoValue *elem, *res = NULL;
	DaoArray *original = self2->original;
	DaoArray *self = original ? original : self2;
	DArray *slices = self2->slices;
	daoint *dims = self->dims;
	daoint N = DaoArray_SliceSize( self2 );
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
			id = original ? DaoArray_IndexFromSlice( original, slices, 0 ) : 0;
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
	for(i=first; i<N; i++){
		idval = proc->stackValues + stackBase + sect->a + 1;
		id = id2 = (original ? DaoArray_IndexFromSlice( original, slices, i ) : i);
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
