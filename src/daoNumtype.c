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

#include"daoStream.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoVmspace.h"
#include"daoRoutine.h"
#include"daoNumtype.h"
#include"assert.h"
#include"math.h"
#include"stdlib.h"
#include"string.h"
#include"ctype.h"

static void DaoComplex_Print( DValue *self, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
	complex16 *p = self->v.c;
	DaoStream_WriteFloat( stream, p->real );
	if( p->imag >= -0.0 ) DaoStream_WriteMBS( stream, "+" );
	DaoStream_WriteFloat( stream, p->imag );
	DaoStream_WriteMBS( stream, "$" );
}
static DValue DaoComplex_Copy( DValue *self, DaoContext *ctx, DMap *cycData )
{
	return *self;
}
static DaoTypeCore comCore =
{
	0, NULL, NULL, NULL, NULL,
	DaoBase_GetField,
	DaoBase_SetField,
	DaoBase_GetItem,
	DaoBase_SetItem,
	DaoComplex_Print,
	DaoComplex_Copy,
};
static void DaoComplex_Lib_Real( DaoContext *ctx, DValue *par[], int N )
{
	complex16 *self = par[0]->v.c;
	DaoContext_PutDouble( ctx, self->real );
	if( N == 2 ) self->real = par[1]->v.d;
}
static void DaoComplex_Lib_Imag( DaoContext *ctx, DValue *par[], int N )
{
	complex16 *self = par[0]->v.c;
	DaoContext_PutDouble( ctx, self->imag );
	if( N == 2 ) self->imag = par[1]->v.d;
}
static DaoFuncItem comMeths[] =
{
	{ DaoComplex_Lib_Real,     "real( self :complex, v=0.00 )=>double" },
	{ DaoComplex_Lib_Imag,     "imag( self :complex, v=0.00 )=>double" },

	{ NULL, NULL }
};

DaoTypeBase comTyper = 
{
	"complex", & comCore, NULL, (DaoFuncItem*) comMeths, {0}, NULL, NULL
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

#define complex8_mul(z,x,y) { complex8 tmp; \
	tmp.real=x.real*y.real-x.imag*y.imag; \
	tmp.imag=x.real*y.imag+x.imag*y.real; z = tmp; }
#define complex16_mul(z,x,y) { complex16 tmp; \
	tmp.real=x.real*y.real-x.imag*y.imag; \
	tmp.imag=x.real*y.imag+x.imag*y.real; z = tmp; }
#define complex_init(c,r,i) { c.real=r; c.imag=i; }

void dao_fft8( complex8 data[], int M, int inv )
{
	int d, i, j, k, m, S, B, D, N = 1<<M;
	double expo = PI2 / (double) N;
	complex8 wn = { 0.0, 0.0 };
	complex8 wi, wj, ws, tmp;

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
			complex8_mul( wj, wi, wn );
			for(d=0; d<D; d++){ /* in each DFT */
				i = d * S + k;  j = i + B;
				tmp = data[i];
				complex8_mul( data[i], data[j], wi );
				complex8_mul( data[j], data[j], wj );
				data[i].real += tmp.real; data[i].imag += tmp.imag;
				data[j].real += tmp.real; data[j].imag += tmp.imag;
			}
			complex8_mul( wi, wi, ws );
		}
	}
}
void dao_fft16( complex16 data[], int M, int inv )
{
	int d, i, j, k, m, S, B, D, N = 1<<M;
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

/* multiple precision integer */
DLong* DLong_New()
{
	DLong *self = dao_calloc( 1, sizeof(DLong) );
	self->sign = 1;
	self->base = 10;
	return self;
}
void DLong_Delete( DLong *self )
{
	if( self->pbuf ) dao_free( self->pbuf );
	dao_free( self );
}
void DLong_Clear( DLong *self )
{
	if( self->pbuf ) dao_free( self->pbuf );
	self->data = self->pbuf = NULL;
	self->size = self->bufSize = 0;
}
void DLong_Resize( DLong *self, size_t size )
{
	size_t i;
	if( size == self->size && self->bufSize >0 ) return;
	if( self->data != self->pbuf ){
		size_t min = size > self->size ? self->size : size;
		memmove( self->pbuf, self->data, min * sizeof(short) );
		self->data = self->pbuf;
	}
	if( size >= self->bufSize || size < self->bufSize /2 ){
		self->bufSize = size + 1;
		self->pbuf = (ushort_t*)dao_realloc( self->pbuf, (self->bufSize+1)*sizeof(short) );
		self->data = self->pbuf;
	}
	for(i=self->size; i<self->bufSize; i++ ) self->data[i] = 0;
	self->data[ size ] = 0;
	self->size = size;
}
void DLong_Reserve( DLong *self, size_t size )
{
	size_t i, d = self->data - self->pbuf;
	if( self->bufSize - d >= size ) return;
	self->pbuf = (ushort_t*)dao_realloc( self->pbuf, (size+d+1)*sizeof(short) );
	for(i=self->bufSize; i<size+d; i++ ) self->pbuf[i] = 0;
	self->data = self->pbuf + d;
	self->bufSize = size + d;
}
void DLong_PushBack( DLong *self, ushort_t it )
{
	size_t d = self->data - self->pbuf;
	if( self->size + d + 1 >= self->bufSize ){
		self->bufSize += self->bufSize/5 + 5;
		self->pbuf = (ushort_t*)dao_realloc( self->pbuf, (self->bufSize+1)*sizeof(short) );
		self->data = self->pbuf + d;
	}
	self->data[ self->size ] = it;
	self->size ++;
	self->data[ self->size ] = 0;
}
void DLong_PushFront( DLong *self, ushort_t it )
{
	if( self->data == self->pbuf ){
		size_t from = self->bufSize/5 + 5;
		self->bufSize += from;
		self->pbuf = dao_realloc( self->pbuf, (self->bufSize+1)*sizeof(short) );
		memmove( self->pbuf + from, self->pbuf, self->size*sizeof(short) );
		self->data = self->pbuf + from;
	}
	self->data --;
	self->data[0] = it;
	self->size ++;
}
void DLong_Normalize( DLong *self )
{
	size_t n = self->size;
	while( n && self->data[ n-1 ] ==0 ) n --;
	DLong_Resize( self, n );
}
void DLong_Normalize2( DLong *self )
{
	size_t n = self->size;
	while( n && self->data[ n-1 ] ==0 ) n --;
	self->size = n;
}
int DLong_UCompare( DLong *x, DLong *y )
{
	int nx = x->size -1;
	int ny = y->size -1;
	ushort_t *dx = x->data;
	ushort_t *dy = y->data;
	while( nx >=0 && dx[nx] ==0 ) nx --;
	while( ny >=0 && dy[ny] ==0 ) ny --;
	if( nx > ny ) return 1; else if( nx < ny ) return -1;
	while( nx >= 0 && dx[nx] == dy[ny] ) nx --, ny --;
	if( nx <0 ) return 0;
	return ( dx[nx] > dy[ny] ) ? 1 : -1;
}
int DLong_Compare( DLong *x, DLong *y )
{
	if( x->sign != y->sign ) return x->sign - y->sign;
	return x->sign * DLong_UCompare( x, y );
}
int DLong_CompareToZero( DLong *self )
{
	size_t n = self->size;
	while( n && self->data[n-1] ==0 ) n -= 1;
	if( n == 0 ) return 0;
	return self->sign;
}
int DLong_CompareToInteger( DLong *self, dint x )
{
	int i, n = self->size - 1, m = (sizeof(dint)*8) / LONG_BITS;
	if( self->sign < 0 && x > 0 ) return -1;
	if( self->sign > 0 && x < 0 ) return  1;
	x = abs( x );
	while( m > 0 && ((x>>(m*LONG_BITS)) & LONG_MASK) ==0 ) m -= 1;
	while( n > 0 && self->data[n] == 0 ) n -= 1; /* bit array has leading 0 */
	if( n > m ) return self->sign;
	if( n < m ) return - self->sign;
	for(i=m; i>=0; i--){
		ushort_t d = (x>>(i*LONG_BITS)) & LONG_MASK;
		if( self->data[i] > d ) return self->sign;
		if( self->data[i] < d ) return - self->sign;
	}
	return 0;
}
int DLong_CompareToDouble( DLong *self, double x )
{
	double prod, frac;
	int i, expon, bit, bit2, res;

	if( self->sign > 0 && x < 0 ) return 1;
	if( self->sign < 0 && x > 0 ) return -1;

	frac = frexp( fabs( x ), & expon );
	if( expon <=0 ){ /* |x|<1 */
		res = DLong_CompareToZero( self );
		if( res ==0 ) return 0 < x ? -1 : 1;
		return self->sign;
	}

	for(i=self->size*LONG_BITS-1; i>=expon; i--){ /* check extra bits */
		bit = (self->data[i/LONG_BITS] & (1<<(i%LONG_BITS)))>>(i%LONG_BITS);
		if( bit ) return self->sign;
	}

	/* compare integer part bit by bit */
	while( expon ){
		expon -= 1;
		prod = frac * 2;
		bit = (int) prod;
		frac = prod - bit;
		bit2 = (self->data[expon/LONG_BITS] & (1<<(expon%LONG_BITS)))>>(expon%LONG_BITS);
		if( bit != bit2 ) return bit - bit2;
	}
	/* integer part is equal: */
	if( frac ) return - self->sign;
	return 0;
}
void DLong_Move( DLong *z, DLong *x )
{
	size_t nx = x->size;
	size_t nz = z->size;
	if( nx+nx < nz || nz < nx ) DLong_Resize( z, nx );
	z->sign = x->sign;
	z->base = x->base;
	memmove( z->data, x->data, nx * sizeof(ushort_t) );
	z->size = nx;
	if( x->base != 2 ) DLong_Normalize2( z );
}

static void LongAdd2( DLong *z, DLong *x, DLong *y, int base )
{
	ushort_t *dx, *dy, *dz;
	size_t nx = x->size;
	size_t ny = y->size;
	int i, sum = 0;
	if( x->size > y->size ){
		DLong *tmp = x;
		x = y;  y = tmp;
	}
	nx = x->size;
	ny = y->size;
	if( z->bufSize <= ny ) DLong_Resize( z, ny );
	dx = x->data;
	dy = y->data;
	dz = z->data;
	for(i=0; i<nx; i++){
		sum += dx[i] + dy[i];
		dz[i] = sum % base; 
		sum = sum / base;
	}
	for(i=nx; i<ny; i++){
		sum += dy[i];
		dz[i] = sum % base;
		sum = sum / base;
	}
	z->size = ny;
	while( sum ){
		dz[ z->size ++ ] = sum % base;
		sum = sum / base;
	}
}
static void DLong_UAdd( DLong *z, DLong *x, DLong *y )
{
	ushort_t *dx, *dy, *dz;
	size_t nx = x->size;
	size_t ny = y->size;
	int i, sum = 0;
	if( x->size > y->size ){
		DLong *tmp = x;
		x = y;  y = tmp;
	}
	nx = x->size;
	ny = y->size;
	if( z->bufSize <= ny ) DLong_Resize( z, ny );
	dx = x->data;
	dy = y->data;
	dz = z->data;
	for(i=0; i<nx; i++){
		sum += dx[i] + dy[i];
		dz[i] = sum & LONG_MASK; 
		sum = sum >> LONG_BITS;
	}
	for(i=nx; i<ny; i++){
		sum += dy[i];
		dz[i] = sum & LONG_MASK;
		sum = sum >> LONG_BITS;
	}
	z->size = ny;
	while( sum ){
		DLong_Append( z, sum & LONG_MASK );
		sum = sum >> LONG_BITS;
	}
}
/* x must be larger than y: */
static void LongSub3( DLong *z, DLong *x, DLong *y )
{
	ushort_t *dz = z->data;
	ushort_t *dx = x->data;
	ushort_t *dy = y->data;
	size_t nx = x->size;
	size_t ny = y->size;
	int i, sub = 1;
	assert( DLong_UCompare( x, y ) >=0 );
	if( z->bufSize < nx ) DLong_Resize( z, nx );
	dz = z->data;
	for(i=0; i<ny; i++){
		/* sub = LONG_BASE + dx[i] - dy[i] - (1-sub); */
		sub += (LONG_BASE -1) + dx[i] - dy[i];
		dz[i] = sub & LONG_MASK;
		sub = sub >> LONG_BITS;
	}
	for(i=ny; i<nx; i++){
		sub += (LONG_BASE -1) + dx[i];
		dz[i] = sub & LONG_MASK;
		sub = sub >> LONG_BITS;
	}
	while( nx && dz[ nx-1 ] ==0 ) nx --;
	z->size = nx;
}
static int LongSub2( DLong *z, DLong *x, DLong *y ) /* unsigned */
{
	z->sign = 1;
	if( DLong_UCompare( x, y ) <0 ){
		DLong *tmp = x;
		x = y;  y = tmp;
		z->sign = -1;
	}
	LongSub3( z, x, y );
	return z->sign;
}
void DLong_Add( DLong *z, DLong *x, DLong *y )
{
	if( x->sign == y->sign ){
		DLong_UAdd( z, x, y );
		z->sign = x->sign;
	}else if( x->sign >0 ){
		z->sign = LongSub2( z, x, y );
	}else{
		z->sign = LongSub2( z, y, x );
	}
}
void DLong_Sub( DLong *z, DLong *x, DLong *y )
{
	if( x->sign == y->sign ){
		z->sign = LongSub2( z, x, y ) * x->sign;
	}else if( x->sign >0 ){
		DLong_UAdd( z, x, y );
		z->sign = 1;
	}else{
		DLong_UAdd( z, x, y );
		z->sign = -1;
	}
}
static void DLong_MulAdd( DLong *z, DLong *x, ushort_t y, short m );
void DLong_UMulDigitX( DLong *z, DLong *x, ushort_t digit );
static void DLong_UMulSimple( DLong *z, DLong *x, DLong *y )
{
	int i, n = x->size + y->size;
	if( z == x || z == y ){
		DLong *z2 = DLong_New();
		DLong_UMulSimple( z2, x, y );
		DLong_Move( z, z2 );
		DLong_Delete( z2 );
		return;
	}
	if( z->bufSize < n ) DLong_Reserve( z, n );
	z->size = n;
	memset( z->data, 0, z->size * sizeof(short) );
	for(i=0; i<x->size; i++) DLong_MulAdd( z, y, x->data[i], i );
}
typedef struct DLongBuffer DLongBuffer;
struct DLongBuffer
{
	DLong *x0, *x1;
	DLong *y0, *y1;
	DLong *z0, *z1, *z2;
};
static DLongBuffer* DLongBuffer_New( int max )
{
	DLongBuffer *self = dao_malloc( sizeof(DLongBuffer) );
	self->x0 = DLong_New();
	self->x1 = DLong_New();
	self->y0 = DLong_New();
	self->y1 = DLong_New();
	self->z0 = DLong_New();
	self->z1 = DLong_New();
	self->z2 = DLong_New();
	DLong_Resize( self->x0, max );
	DLong_Resize( self->x1, max );
	DLong_Resize( self->y0, max );
	DLong_Resize( self->y1, max );
	DLong_Resize( self->z0, max );
	DLong_Resize( self->z1, max );
	DLong_Resize( self->z2, max );
	return self;
}
static void DLong_Split( DLong *x, DLong *x1, DLong *x0, size_t m )
{
	int size = x->size;
	memmove( x0->data, x->data, m * sizeof(ushort_t) );
	memmove( x1->data, x->data + m, (size-m) * sizeof(ushort_t) );
	x0->size = m;
	x1->size = size - m;
}
static void DLongBuffer_Delete( DLongBuffer *self )
{
	DLong_Delete( self->x0 );
	DLong_Delete( self->x1 );
	DLong_Delete( self->y0 );
	DLong_Delete( self->y1 );
	DLong_Delete( self->z0 );
	DLong_Delete( self->z1 );
	DLong_Delete( self->z2 );
	dao_free( self );
}
static void LongCat( DLong *z, DLong *x1, size_t m, DLong *x0 )
{
	/* z might be the same object as x1: */
	int i, n = x1->size;
	if( z->bufSize < n + m + 1 ) DLong_Reserve( z, n + m + 1 );
	memmove( z->data + m, x1->data, n * sizeof(ushort_t) );
	memset( z->data, 0, m * sizeof(ushort_t) );
	z->size = m + n;
	DLong_UAdd( z, z, x0 );
}
static void LongMulSum( DLong *z, DLong *z2, DLong *z1, DLong *z0, int m )
{
	int n = m + z1->size;
	if( z2 ) n = z2->size + m + m;
	if( z->bufSize <= n ) DLong_Reserve( z, n );
	memmove( z->data, z0->data, z0->size * sizeof(short) );
	if( z2 ) memmove( z->data + (m+m), z2->data, z2->size *sizeof(short) );
	if( z1->bufSize <= z1->size+m ) DLong_Reserve( z1, z1->size + m );
	memmove( z1->data+m, z1->data, z1->size*sizeof(short) );
	z->size = n;
	z1->size += m;
	DLong_UAdd( z, z, z1 );
}
static void LongZ1( DLong *z1, DLong *z0, DLong *z2 )
{
	int i, sub=2;
	int nz1 = z1->size;
	int nz0 = z0->size;
	int nz2 = z2->size;
	ushort_t *dz1 = z1->data;
	ushort_t *dz0 = z0->data;
	ushort_t *dz2 = z2->data;
	if( nz0 < nz2 ){
		nz0 = z2->size;
		nz2 = z0->size;
		dz0 = z2->data;
		dz2 = z0->data;
	}
	for(i=0; i<nz2; i++){
		sub += (2*LONG_BASE -2) + dz1[i] - (dz0[i] + dz2[i]);
		dz1[i] = sub & LONG_MASK;
		sub = sub >> LONG_BITS;
	}
	for(i=nz2; i<nz0; i++){
		sub += (2*LONG_BASE -2) + dz1[i] - dz0[i];
		dz1[i] = sub & LONG_MASK;
		sub = sub >> LONG_BITS;
	}
	for(i=nz0; i<nz1; i++){
		sub += (2*LONG_BASE -2) + dz1[i];
		dz1[i] = sub & LONG_MASK;
		sub = sub >> LONG_BITS;
	}
	while( nz1 && dz1[ nz1-1 ] ==0 ) nz1 --;
	z1->size = nz1;
}
/* Karatsuba's algorithm */
static void DLong_UMulK( DLong *z, DLong *x, DLong *y, DLongBuffer **bufs, int dep )
{
	DLongBuffer **inbufs = bufs;
	DLongBuffer  *buf;
	DLong *x0, *x1, *y0, *y1;
	DLong *z0, *z1, *z2;
	ushort_t *dx = x->data;
	ushort_t *dy = y->data;
	size_t nx = x->size;
	size_t ny = y->size;
	size_t m = 0;
	/*
	   printf( "nx = %i,  ny = %i,  dep = %i\n", nx, ny, dep );
	   DLong_Print( x, NULL );
	   DLong_Print( y, NULL );
	 */
	if( (nx|ny) <= 1 ){
		if( (nx&ny) == 1 ){
			int prod = dx[0] * dy[0];
			if( z->bufSize < 2 ) DLong_Reserve( z, 2 );
			z->data[0] = prod & LONG_MASK;
			z->data[1] = prod >> LONG_BITS;
			z->size = prod >> LONG_BITS ? 2 : (prod ? 1 : 0);
		}else{
			z->size = 0;
		}
		return;
	}else if( nx == 1 ){
		DLong_UMulDigitX( z, y, dx[0] );
		return;
	}else if( ny == 1 ){
		DLong_UMulDigitX( z, x, dy[0] );
		return;
	}else if( nx <= 4 ){
		DLong_UMulSimple( z, x, y );
		return;
	}else if( ny <= 4 ){
		DLong_UMulSimple( z, y, x );
		return;
	}else if( nx <= 20 && ny <= 20 ){
		DLong_UMulSimple( z, x, y );
		return;
	}
	if( nx > ny ){
		DLong *tmp = x;
		m = nx;  nx = ny;  ny = m;
		x = y;  y = tmp;
	}
	if( bufs == NULL ){
		int maxdep = 2 * (1 + log( ny ) / log(2));
		bufs = dao_calloc( maxdep, sizeof(DLongBuffer*) );
	}
	if( bufs[dep] ==NULL ) bufs[dep] = DLongBuffer_New( ny+1 );
	buf = bufs[ dep ++ ];
	x0 = buf->x0; x1 = buf->x1;
	y0 = buf->y0; y1 = buf->y1;
	z0 = buf->z0; z1 = buf->z1; z2 = buf->z2;
	m = ny>>1;
	DLong_Split( y, y1, y0, m ); /* y := y1 * B^m + y0; */
	if( nx + nx < ny ){
		DLong_UMulK( y1, x, y1, bufs, dep ); /* y1 = x * y1; */
		DLong_UMulK( y0, x, y0, bufs, dep ); /* y0 = x * y1; */
		LongCat( z, y1, m, y0 ); /* z = y1 * B^m + y0; */
	}else if( x == y ){
		DLong_UMulK( z1, y0, y1, bufs, dep ); /* z1 = y0 * y1; */
		DLong_UMulK( z2, y1, y1, bufs, dep ); /* y1 = y1 * y1; */
		DLong_UMulK( z0, y0, y0, bufs, dep ); /* y0 = y0 * y0; */
		DLong_UAdd( z1, z1, z1 ); /* z1 = 2 * z1; */
		LongMulSum( z, z2, z1, z0, m );
	}else{
		DLong_Split( x, x1, x0, m ); /* x := x1 * B^m + x0; */
		DLong_UMulK( z2, x1, y1, bufs, dep ); /* z2 = x1 * y1; */
		DLong_UMulK( z0, x0, y0, bufs, dep ); /* z0 = x0 * y0; */
		DLong_UAdd( x1, x1, x0 ); /* x1 = x1 + x0 */
		DLong_UAdd( y1, y1, y0 ); /* y1 = y1 + y0 */
		DLong_UMulK( z1, x1, y1, bufs, dep ); /* z1 = x1 * y1; */
		LongZ1( z1, z0, z2 ); /* (x1+x0)*(y1+y0)-z0-z2; */
		LongMulSum( z, z2, z1, z0, m ); /* z2 * B^2m + z1 * B^m + z0; */
	}
	if( bufs != inbufs ){
		int i, maxdep = 2*(1 + log( ny ) / log(2));
		for(i=0; i<maxdep; i++) if( bufs[i] ) DLongBuffer_Delete( bufs[i] );
		free( bufs );
	}
	z->sign = x->sign * y->sign;
}
void DLong_UMulFFT( DLong *z, DLong *x, DLong *y )
{
	complex16 *cx, *cy = NULL;
	ushort_t *dx = x->data;
	ushort_t *dy = y->data;
	size_t nx = x->size;
	size_t ny = y->size;
	size_t max = nx > ny ? nx : ny;
	size_t i, nc = 1;
	llong_t c = 0; 
	int mc = 0;
	while( (nc>>1) < max ) nc <<= 1, mc ++;
	/* printf( "nc = %i, mc = %i, max = %i\n", nc, mc, max ); */
	cx = dao_calloc( nc, sizeof(complex16) );
	for(i=0; i<nx; i++) cx[i].real = dx[i];
	dao_fft16( cx, mc, -1 );
	if( x == y ){
		cy = cx;
	}else{
		cy = dao_calloc( nc, sizeof(complex16) );
		for(i=0; i<ny; i++) cy[i].real = dy[i];
		dao_fft16( cy, mc, -1 );
	}
	for(i=0; i<nc; i++) complex16_mul( cx[i], cx[i], cy[i] );
	if( x != y ) dao_free( cy );
	dao_fft16( cx, mc, 1 );
	DLong_Resize( z, nc );
	memset( z->data, nc, sizeof(short) );
	for(i=0; i<nc; i++){
		c += (llong_t)(cx[i].real / nc + 0.5);
		z->data[i] = c & LONG_MASK;
		c = c >> LONG_BITS;
	}
	while( nc && z->data[nc-1] ==0 ) nc --;
	DLong_Resize( z, nc );
	dao_free( cx );
}
void DLong_UMul( DLong *z, DLong *x, DLong *y )
{
	ushort_t *dx = x->data;
	ushort_t *dy = y->data;
	size_t nx = x->size;
	size_t ny = y->size;
	/*
	   printf( "nx = %i,  ny = %i,  dep = %i\n", nx, ny, dep );
	   DLong_Print( x, NULL );
	   DLong_Print( y, NULL );
	 */
	if( (nx|ny) <= 1 ){
		if( (nx&ny) == 1 ){
			int prod = dx[0] * dy[0];
			if( z->bufSize < 2 ) DLong_Reserve( z, 2 );
			z->data[0] = prod & LONG_MASK;
			z->data[1] = prod >> LONG_BITS;
			z->size = prod >> LONG_BITS ? 2 : (prod ? 1 : 0);
		}else{
			z->size = 0;
		}
		return;
	}else if( nx == 1 ){
		DLong_UMulDigitX( z, y, dx[0] );
		return;
	}else if( ny == 1 ){
		DLong_UMulDigitX( z, x, dy[0] );
		return;
	}else if( nx <= 4 ){
		DLong_UMulSimple( z, x, y );
		return;
	}else if( ny <= 4 ){
		DLong_UMulSimple( z, y, x );
		return;
	}else if( nx < 16 && ny < 16 ){
		DLong_UMulSimple( z, x, y );
		return;
#if 0
	}else if( nx < 64 && nx < 64 ){
		return;
#endif
	}
#if 0
	DLong_UMulK( z, x, y, NULL, 0 );
#else
	DLong_UMulFFT( z, x, y );
#endif
}
void DLong_Mul( DLong *z, DLong *x, DLong *y )
{
	DLong_UMul( z, x, y );
	/* DLong_UMulK( z, x, y, NULL, 0 ); */
	z->sign = x->sign * y->sign;
}
static size_t DLong_NormCount( DLong *self )
{
	int i, d;
	while( self->size && self->data[ self->size-1 ] ==0 ) self->size --;
	/* if( self->size * 2 < self->bufSize ) DLong_Resize( self->size ); */
	if( self->size ==0 ) return 0;
	d = self->data[ self->size-1 ];
	for(i=1; d && (i<LONG_BITS); i++) d >>= 1;
	return self->size * LONG_BITS + i - LONG_BITS;
}
void DLong_UMulDigitX( DLong *z, DLong *x, ushort_t digit )
{
	ushort_t *dz, *dx = x->data;
	size_t nx = x->size;
	int i, carray = 0;
	if( digit == 0 ){
		z->size = 0;
		return;
	}else if( digit == 1 ){
		DLong_Move( z, x );
		return;
	}
	while(nx >0 && x->data[nx-1] ==0 ) nx--;
	if( z->bufSize < nx ) DLong_Reserve( z, nx );
	dz = z->data;
	z->size = nx;
	for(i=0; i<nx; i++){
		carray += digit * dx[i];
		dz[i] = carray & LONG_MASK;
		carray = carray >> LONG_BITS;
	}
	if( carray ) DLong_Append( z, carray );
}
ushort_t DLong_UDivDigit( DLong *z, ushort_t digit )
{
	size_t nz = z->size;
	int i, carray = 0;
	while(nz >0 && z->data[nz-1] ==0 ) nz--;
	for(i=nz; i>0; i--){
		carray = (carray<<LONG_BITS) | z->data[i-1];
		z->data[i-1] = carray / digit;
		carray = carray % digit;
	}
	while(nz >0 && z->data[nz-1] ==0 ) nz--;
	z->size = nz;
	return carray;
}
/* z = x * y + r; z, x: input, y, r: output */
void DLong_Div( DLong *z, DLong *x, DLong *y, DLong *r )
{
	DLong *mul;
	DLong *r2;
	ushort_t *pbase;
	size_t nx = x->size;
	size_t nz = z->size;
	size_t nr;
	llong_t hr;
	int cmp, hx;
	ushort_t d;

	while(nx >0 && x->data[nx-1] ==0 ) nx--;
	while(nz >0 && z->data[nz-1] ==0 ) nz--;
	DLong_Move( r, z );
	y->sign = z->sign * x->sign;
	if( nz < nx ) return;
	if( nx ==0 ){
		/* XXX error */
		y->size = 0;
		return;
	}else if( nx == 1 ){
		DLong_Move( y, z );
		r->size = 1;
		r->data[0] = DLong_UDivDigit( y, x->data[0] );
		return;
	}
	y->size = 0; /* z might be y */
	mul = DLong_New();
	r2 = DLong_New();
	pbase = r->data;
	r->data = r->data + (r->size - nx);
	r->size = nx;
	hx = (x->data[nx-1] << LONG_BITS) | x->data[nx-2];
	/* sliding x over r (originall equal to z) */
	while( r->data >= pbase ){
		if( r->size < nx || (r->size == nx && r->data[nx-1] < x->data[nx-1]) ){
			DLong_PushFront( y, 0 );
			r->data --;
			r->size ++;
			continue;
		}
		assert( (r->size-1) <= nx );
		nr = r->size;
		hr = (r->data[nx-1] << LONG_BITS) | r->data[nx-2];
		if( nr > nx ) hr |= ((llong_t)r->data[nx]) << (LONG_BITS<<1);
		/* using the first two digits of the divisor to guess the quotient */
		d = hr / hx;
		DLong_UMulDigitX( mul, x, d );
		cmp = DLong_UCompare( mul, r );
		if( cmp ==0 ){
			r->size = 0;
			r->data --;
			DLong_PushFront( y, d );
			continue;
		}else if( cmp >0 ){
			d --;
			DLong_UMulDigitX( mul, x, d );
		}
		if( y->sign <0 && r->data == pbase ){
			d ++;
			DLong_UMulDigitX( mul, x, d );
			LongSub3( r2, mul, r );
		}else{
			LongSub3( r2, r, mul );
		}
		DLong_PushFront( y, d );
		memmove( r->data, r2->data, r2->size *sizeof(short) );
		r->size = r2->size + 1;
		r->data --;
	}
	r->size --;
	r->data = pbase;
	DLong_Delete( mul );
	DLong_Delete( r2 );
	DLong_Normalize2( y );
	DLong_Normalize2( r );
}
void DLong_Pow( DLong *z, DLong *x, dint n )
{
	dint m = 1;
	if( n == 1 ){
		DLong_Move( z, x );
		return;
	}else if( n == 0 ){
		DLong_Resize( z, 1 );
		z->data[0] = 1;
		return;
	}else if( x->size == 0 ){
		DLong_Resize( z, 0 );
		return;
	}else if( x->size == 1 && x->data[0] ==1 ){
		DLong_Resize( z, 1 );
		z->data[0] = 1;
		z->sign = n%2 ? 1 : x->sign;
		return;
	}else if( n == 2 ){
		DLong_Mul( z, x, x );
		return;
	}
	DLong_Move( z, x );
	while( 2*m <= n ){
		DLong_Mul( z, z, z );
		m *= 2;
	}
	if( m < n ){
		DLong *tmp = DLong_New();
		DLong_Pow( tmp, x, n-m );
		DLong_Mul( z, z, tmp );
		DLong_Delete( tmp );
	}
}
/* z = x * x + r */
/* binary searching */
void DLong_Sqrt( DLong *z, DLong *x, DLong *r )
{
	int k, b1, b2;
	int i = 0;
	DLong *max = DLong_New();
	DLong *min = DLong_New();
	DLong_Resize( x, z->size + 1 );
	DLong_Resize( r, z->size + 2 );
	DLong_Move( max, z );
	min->size = 0;
	while(1){
		i ++;
		b1 = DLong_NormCount( min );
		b2 = DLong_NormCount( max );
		/* printf( "%i  %i\n", b1, b2 ); */
		DLong_UAdd( x, min, max );
		if( b2 > b1 + 1 ){
			DLong_ShiftRight( x, (b2-b1)>>1 );
		}else{
			DLong_ShiftRight( x, 1 );
		}
		/* DLong_Print( x, NULL ); */
		DLong_Mul( r, x, x );
		k = DLong_UCompare( r, z );
		if( k ==0 ){
			DLong_Clear(r);
			return;
		}else if( k >0 ){
			DLong_Move( max, x );
		}else{
			if( DLong_UCompare( x, min ) ==0 ) break;
			DLong_Move( min, x );
		}
	}
	/* printf( "iterations: %i\n", i ); */
	DLong_Move( x, min );
	x->sign = 1;
	DLong_UMul( max, x, x );
	DLong_Sub( r, z, max );
	DLong_Delete( min );
	DLong_Delete( max );
}
static void LongMulInt( DLong *z, DLong *x, int y, int base )
{
	ushort_t *dz = z->data;
	ushort_t *dx = x->data;
	long i, sum = 0, nx = x->size;
	for(i=0; i<nx; i++){
		sum += dx[i] * y;
		dz[i] = sum % base;
		sum = sum / base;
	}
	z->size = nx;
	while( sum ){
		dz[ z->size ++ ] = sum % base;
		sum = sum / base;
	}
}
void DLong_AddInt( DLong *z, DLong *x, dint y, DLong *buf )
{
}
void DLong_MulAdd( DLong *z, DLong *x, ushort_t y, short m )
{
	int i, carray = 0;
	if( z->bufSize < x->size + m ) DLong_Reserve( z, x->size + m );
	for(i=z->size; i<x->size+m; i++) z->data[i] = 0;
	if( z->size < x->size+m ) z->size = x->size + m;
	for(i=0; i<x->size; i++){
		carray += x->data[i] * y + z->data[i+m];
		z->data[i+m] = carray & LONG_MASK;
		carray = carray >> LONG_BITS;
	}
	for(i=x->size+m; i<z->size; i++){
		carray += z->data[i];
		z->data[i] = carray & LONG_MASK;
		carray = carray >> LONG_BITS;
	}
	while( carray ){
		DLong_Append( z, carray & LONG_MASK );
		carray = carray >> LONG_BITS;
	}
}
void DLong_MulInt( DLong *z, DLong *x, dint y )
{
	int m = 0, sign = 1;
	if( y ==0 ){
		z->size = 0;
		return;
	}else if( y < 0 ){
		sign = -1;
		y = - y;
	}
	z->sign = x->sign * sign;
	if( z->bufSize < x->size ) DLong_Resize( z, x->size );
	z->size = x->size;
	memset( z->data, 0, z->size * sizeof(short) );
	while( y ){
		DLong_MulAdd( z, x, y & LONG_MASK, m++ );
		y = y >> LONG_BITS;
	}
}
void DLong_BitAND( DLong *z, DLong *x, DLong *y )
{
	size_t i, min = x->size < y->size ? x->size : y->size;
	ushort_t *dx, *dy, *dz;
	DLong_Resize( z, min );
	dx = x->data; dy = y->data; dz = z->data;
	for(i=0; i<min; i++) dz[i] = dx[i] & dy[i];
}
void DLong_BitOR( DLong *z, DLong *x, DLong *y )
{
	size_t i, max = x->size > y->size ? x->size : y->size;
	ushort_t *dx, *dy, *dz;
	DLong_Resize( z, max );
	dx = x->data; dy = y->data; dz = z->data;
	if( max == x->size ){
		for(i=0; i<y->size; i++) dz[i] = dx[i] | dy[i];
		for(i=y->size; i<max; i++) dz[i] = dx[i];
	}else{
		for(i=0; i<x->size; i++) dz[i] = dx[i] | dy[i];
		for(i=x->size; i<max; i++) dz[i] = dy[i];
	}
}
void DLong_BitXOR( DLong *z, DLong *x, DLong *y )
{
	size_t i, max = x->size > y->size ? x->size : y->size;
	ushort_t *dx, *dy, *dz;
	DLong_Resize( z, max );
	dx = x->data; dy = y->data; dz = z->data;
	if( max == x->size ){
		for(i=0; i<y->size; i++) dz[i] = dx[i] ^ dy[i];
		for(i=y->size; i<max; i++) dz[i] = dx[i];
	}else{
		for(i=0; i<x->size; i++) dz[i] = dx[i] ^ dy[i];
		for(i=x->size; i<max; i++) dz[i] = dy[i];
	}
}
void DLong_ShiftLeft( DLong *z, int bits )
{
	ushort_t *dz = z->data;
	int i, k, nz = z->size;
	k = bits / LONG_BITS;
	if( k && z->data >= z->pbuf + k ){
		z->data -= k;
		z->size += k;
		memset( z->data, 0, k * sizeof(ushort_t) );
	}else if( k ){
		DLong_Resize( z, nz + k + 1 );
		memmove( z->data + k, z->data, nz * sizeof(ushort_t) );
		memset( z->data, 0, k * sizeof(ushort_t) );
		z->size --;
		dz = z->data;
		nz = z->size;
	}
	k = bits % LONG_BITS;
	if( k == 0 ) return;
	i = dz[nz-1] >> (LONG_BITS-k);
	if( i ) DLong_Append( z, i );
	dz = z->data;
	for(i=nz-1; i>0; i--) dz[i] = ((dz[i]<<k) | dz[i-1]>>(LONG_BITS-k)) & LONG_MASK;
	dz[0] = (dz[0] << k) & LONG_MASK;
}
void DLong_ShiftRight( DLong *z, int bits )
{
	ushort_t *dz = z->data;
	int i, k, nz = z->size;
	if( bits >= nz * LONG_BITS ){
		DLong_Clear( z );
		return;
	}
	k = bits / LONG_BITS;
	if( k && (k + z->data - z->pbuf ) < z->size/10 ){
		z->data += k;
	}else if( k ){
		memmove( z->data, z->data + k, (nz-k) * sizeof(ushort_t) );
		DLong_Resize( z, nz-k );
		dz = z->data;
		nz = z->size;
	}
	k = bits % LONG_BITS;
	if( k == 0 ) return;
	nz --;
	for(i=0; i<nz; i++) dz[i] = (( (dz[i+1]<<LONG_BITS) |dz[i] )>>k) & LONG_MASK;
	dz[nz] = dz[nz] >> k;
	if( dz[nz] ==0 ) z->size --;
}
void DLong_Flip( DLong *self )
{
	size_t i;
	for(i=0; i<self->size; i++) self->data[i] = (LONG_BASE-1) ^ self->data[i];
}
void DLong_Convert( DLong *self, int base, DLong *to, int tobase )
{
	DLong *radix, *digit;
	size_t j, i = 1 + self->size * (log(base)/ log(tobase)+1);
	radix = DLong_New();
	digit = DLong_New();
	DLong_Resize( radix, i );
	DLong_Resize( digit, i );
	DLong_Resize( to, i );
	radix->size = 1;
	radix->data[0] = 1;
	to->size = 0;
	for(j=0; j<self->size; j++){
		LongMulInt( digit, radix, self->data[j], tobase );
		LongAdd2( to, to, digit, tobase );
		LongMulInt( radix, radix, base, tobase );
	}
	i = to->size;
	while(i >0 && to->data[i-1] ==0 ) i--;
	DLong_Resize( to, i );
	DLong_Delete( radix );
	DLong_Delete( digit );
}
static void DLong_PrintBits( DLong *self, DString *s )
{
	DString *s2 = s;
	size_t i = self->size;
	int j;
	if( s == NULL ) s = DString_New(1);
	DString_SetMBS( s, "0" );
	if( i ==0 ) goto Finish;
	DString_Clear( s );

	for(i=self->size; i>0; i--){
		int digit = self->data[i-1];
		for(j=LONG_BITS-1; j>=0; j--){
			DString_AppendChar( s, '0' + ((digit & (1<<j))>>j) );
		}
	}
Finish:
	DString_AppendMBS( s, "L2" );
	if( s2 == NULL ){
		printf( "%s\n", s->mbs );
		DString_Delete(s);
	}
}
void DLong_Print( DLong *self, DString *s )
{
	const char *digits = "0123456789abcdef";
	DLong *based;
	DString *s2 = s;
	size_t i = self->size;
	if( self->base == 2 ){
		DLong_PrintBits( self, s );
		return;
	}
	while(i >0 && self->data[i-1] ==0 ) i--;
	if( s == NULL ) s = DString_New(1);
	DString_SetMBS( s, "0L" );
	if( i ==0 ) goto Finish;
	DString_Clear( s );
	if( self->sign <0 ) DString_AppendChar( s, '-' );

	based = DLong_New();
	DLong_Convert( self, LONG_BASE, based, self->base );
	i = based->size;
	while(i >0 && based->data[i-1] ==0 ) i--;
	for(; i>=1; i--) DString_AppendChar( s, digits[based->data[i-1]] );
	DString_AppendChar( s, 'L' );
	DLong_Delete( based );
Finish:
	if( self->base != 10 ){
		char buf[20];
		sprintf( buf, "%i", self->base );
		DString_AppendMBS( s, buf );
	}
	if( s2 == NULL ){
		printf( "%s\n", s->mbs );
		DString_Delete(s);
	}
}
dint DLong_ToInteger( DLong *self )
{
	size_t i, k=1, n = (sizeof(dint)*8) / LONG_BITS;
	dint res = 0;
	switch( self->size ){
	case 0 : break;
	case 1 : res = self->data[0]; break;
	case 2 : res = self->data[0] + LONG_BASE * self->data[1]; break;
	default :
			 if( self->size < n ) n = self->size;
			 for(i=0; i<n; i++, k*=LONG_BASE) res += k * self->data[i];
			 break;
	}
	return res * self->sign;
}
double DLong_ToDouble( DLong *self )
{
	size_t i, n;
	double res = 0.0, k = 1.0;
	switch( self->size ){
	case 0 : break;
	case 1 : res = self->data[0]; break;
	case 2 : res = self->data[0] + LONG_BASE * self->data[1]; break;
	default :
		for( n = 0; n < self->size && self->data[n] == 0; n++ );
		if( n == self->size )
			return res;
		for( i = n; i < self->size; i++, k *= LONG_BASE )
			res += k * self->data[i];
		if( n != 0 )
			res = ldexp( res, n * LONG_BITS - 1 );
		break;
	}
	return res * self->sign;
}
void DLong_FromInteger( DLong *self, dint x )
{
	if( x < 0 ){
		x = -x;
		self->sign = -1;
	}
	DLong_Clear( self );
	while( x ){
		DLong_Append( self, x & LONG_MASK );
		x = x >> LONG_BITS;
	}
}
void DLong_FromDouble( DLong *self, double value )
{
	double prod, frac;
	int expon, bit;

	DLong_Clear( self );
	self->sign = value > 0 ? 1 : -1;

	value = fabs( value );
	frac = frexp( value, & expon );
	if( expon <=0 ) return;

	DLong_Resize( self, expon / LONG_BITS + 1 );
	/* convert bit by bit */
	while( frac > 0 && expon ){
		expon -= 1;
		prod = frac * 2;
		bit = (int) prod;
		frac = prod - bit;
		self->data[ expon/LONG_BITS ] |= (bit<<(expon%LONG_BITS));
	}
	DLong_Normalize( self );

#if 0
	DLong_Print( self, NULL );
	DLong_Clear( self );
	value = floor( value );
	/* convert base by base: */
	while( value ){
		double y = floor( value / LONG_BASE );
		ushort_t r = (ushort_t)(value - y * LONG_BASE);
		DLong_Append( self, r );
		value = y;
	}
	DLong_Print( self, NULL );
#endif
}
char DLong_FromString( DLong *self, DString *s )
{
	DLong *tmp;
	char table[256];
	char *mbs;
	int i, j, n, pl;
	int base = 10;

	DLong_Clear( self );
	DString_ToMBS( s );
	self->base = 10;
	n = s->size;
	mbs = s->mbs;
	if( n == 0 ) return 0;
	memset( table, 0, 256 );
	for(i='0'; i<='9'; i++) table[i] = i - ('0'-1);
	for(i='a'; i<='f'; i++) table[i] = i - ('a'-1) + 10;
	for(i='A'; i<='F'; i++) table[i] = i - ('A'-1) + 10;
	pl = DString_FindChar(s, 'L', 0);
	if( pl != MAXSIZE ){
		/* if( mbs[pl+1] == '0' ) return 0; */
		if( (pl+3) < n ) return 'L';
		if( (pl+1) < n && ! isdigit( mbs[pl+1] ) ) return 'L';
		if( (pl+2) < n && ! isdigit( mbs[pl+2] ) ) return 'L';
		if( (pl+1) < n ) self->base = base = strtol( mbs+pl+1, NULL, 0 );
		if( base > 16 ) return 'L';
		n = pl;
	}
	if( n && mbs[0] == '-' ){
		self->sign = -1;
		mbs ++;
		n --;
	}
	if( n > 1 && mbs[0] == '0' && tolower( mbs[1] ) == 'x' ){
		base = 16;
		mbs += 2;
		n -= 2;
	}
	DLong_Resize( self, n );
	for(i=n-1, j=0; i>=0; i--, j++){
		short digit = table[ (short)mbs[i] ];
		if( digit ==0 ) return mbs[i];
		self->data[j] = digit - 1;
		if( self->data[j] >= base ){
			self->data[j] = 0;
			self->size = j;
			return mbs[i];
		}
	}
	tmp = DLong_New();
	DLong_Move( tmp, self );
	DLong_Convert( tmp, base, self, LONG_BASE );
	DLong_Delete( tmp );
	return 0;
}

const int base_bits[] = {0,0,1,0,2,0,0,0,3,0,0,0,0,0,0,0,4};
const int base_masks[] = {0,0,1,0,3,0,0,0,7,0,0,0,0,0,0,0,15};
static void DaoLong_GetItem( DValue *self0, DaoContext *ctx, DValue pid )
{
	DLong *self = self0->v.l;
	dint id = DValue_GetInteger( pid );
	int w = base_bits[self->base];
	int n = self->size;
	int digit = 0;
	if( self->base == 0 ){
		if( id <0 || id >= n*LONG_BITS ){
			DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "out of range" );
			return;
		}
		digit = (self->data[id/LONG_BITS] & (1<<(id%LONG_BITS)))>>(id%LONG_BITS);
		DaoContext_PutInteger( ctx, digit );
		return;
	}
	if( w == 0 ){
		DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "need power 2 radix" );
		return;
	}
	if( id <0 || (w && id*w >= n*LONG_BITS) || (w == 0 && id >= n) ){
		DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "out of range" );
		return;
	}
	if( self->base == 2 ){
		digit = (self->data[id/LONG_BITS] & (1<<(id%LONG_BITS)))>>(id%LONG_BITS);
	}else if( w ){
		int m = id*w / LONG_BITS;
		if( ((id+1)*w <= (m+1)*LONG_BITS) || m+1 >= n ){
			int digit2 = self->data[m] >> (id*w - m*LONG_BITS);
			digit = digit2 & base_masks[self->base];
			if( m+1 >= n && digit2 ==0 )
				DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "out of range" );
		}else{
			int d = (self->data[m+1]<<LONG_BITS) | self->data[m];
			digit = (d>>(id*w - m*LONG_BITS)) & base_masks[self->base];
		}
	}else{
		digit = self->data[id];
	}
	DaoContext_PutInteger( ctx, digit );
}
static void DaoLong_SetItem( DValue *self0, DaoContext *ctx, DValue pid, DValue value )
{
	DLong *self = self0->v.l;
	dint id = DValue_GetInteger( pid );
	dint digit = DValue_GetInteger( value );
	int i, n = self->size;
	int w = base_bits[self->base];
	if( self->base == 2 ){
		if( pid.t == 0 ){
			ushort_t bits = digit ? LONG_BASE-1 : 0;
			for(i=0; i<self->size; i++) self->data[i] = bits;
		}else{
			if( digit )
				self->data[id/LONG_BITS] |= (1<<(id%LONG_BITS));
			else
				self->data[id/LONG_BITS] &= ~(1<<(id%LONG_BITS));
		}
		return;
	}
	if( w == 0 ){
		DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "need power 2 radix" );
		return;
	}
	if( id <0 || (w && id*w >= n*LONG_BITS) || (w == 0 && id >= n) ){
		DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "out of range" );
		return;
	}else if( digit <0 || digit >= self->base ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "digit value out of range" );
		return;
	}
	if( pid.t == 0 ){
		ushort_t bits = digit;
		if( self->base == 2 ) bits = digit ? LONG_BASE-1 : 0;
		for(i=0; i<self->size; i++) self->data[i] = bits;
	}else{
		if( self->base == 2 ){
			if( digit )
				self->data[id/LONG_BITS] |= (1<<(id%LONG_BITS));
			else
				self->data[id/LONG_BITS] &= ~(1<<(id%LONG_BITS));
		}else if( w ){
			int m = id*w / LONG_BITS;
			int shift = id*w - m*LONG_BITS;
			if( ((id+1)*w <= (m+1)*LONG_BITS) || m+1 >= n ){
				int digit2 = self->data[m] >> shift;
				self->data[m] &= ~(base_masks[self->base]<<shift);
				self->data[m] |= digit<<shift;
				if( m+1 >= n && digit2 ==0 )
					DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "out of range" );
			}else{
				int d = (self->data[m+1]<<LONG_BITS) | self->data[m];
				d &= ~(base_masks[self->base]<<shift);
				d |= digit<<shift;
				self->data[m] = d & LONG_MASK;
				self->data[m+1] = d >> LONG_BITS;
			}
		}else{
			self->data[id] = digit;
		}
	}
}
static DaoTypeCore longCore=
{
	0, NULL, NULL, NULL, NULL,
	DaoBase_GetField,
	DaoBase_SetField,
	DaoLong_GetItem,
	DaoLong_SetItem,
	DaoBase_Print,
	DaoBase_Copy,
};
static void DaoLong_Size( DaoContext *ctx, DValue *p[], int N )
{
	DLong *self = p[0]->v.l;
	size_t size = self->size;
	if( self->base ==2 ){
		DaoContext_PutInteger( ctx, size*LONG_BITS );
		return;
	}
	while( size && self->data[size-1] ==0 ) size --;
	assert( self->size == size );
	DaoContext_PutInteger( ctx, size );
}
static void DaoLong_Sqrt( DaoContext *ctx, DValue *p[], int N )
{
	DLong *z = p[0]->v.l;
	DaoTuple *tuple = DaoTuple_New( 2 );
	DaoContext_SetResult( ctx, (DaoBase*) tuple );
	tuple->items->data[0].v.l = DLong_New();
	tuple->items->data[1].v.l = DLong_New();
	tuple->items->data[0].t = tuple->items->data[1].t = DAO_LONG;
	if( z->sign <0 ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "need positive long integer" );
		return;
	}
	DLong_Sqrt( z, tuple->items->data[0].v.l, tuple->items->data[1].v.l );
}
static DaoFuncItem longMeths[] =
{
	{ DaoLong_Size, "size( self : long ) => int" } ,
	{ DaoLong_Sqrt, "sqrt( self : long ) => tuple<long,long>" } ,
	{ NULL, NULL }
};
DaoTypeBase longTyper =
{
	"long", & longCore, NULL, (DaoFuncItem*) longMeths, {0}, NULL, NULL
};

#ifdef DAO_WITH_NUMARRAY


enum { SLICE_RANGE, SLICE_ENUM };


static void Array_SetAccum( size_t *dim, size_t *accum, int N )
{
	int i;
	accum[N-1] = 1;
	for(i=N-2; i>=0; i--) accum[i] = accum[i+1] * dim[i+1];
}
static void Array_FlatIndex2Mult( size_t *dim, int N, int sd, size_t *md )
{
	int i;
	for(i=N-1; i>=0; i--){
		md[i] = sd % dim[i];
		sd = sd / dim[i];
	}
}
static void Array_MultIndex2Flat( size_t *accum, int N, int *sd, size_t *md )
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
	int i, j, k, k2, N = self->size, D = perm->size;
	size_t *dm, *ac, *mdo, *mdn, *pm = perm->items.pSize;
	DArray *dim = DArray_New(0); /* new dimension vector */
	DArray *acc = DArray_New(0); /* new accumulate vector */
	DArray *mido = DArray_New(0); /* old multiple indices */
	DArray *midn = DArray_New(0); /* new multiple indices */
	int ival = 0;
	float fval = 0;
	double dval = 0;
	complex16  c16val = {0,0};

	if( D != self->dims->size ) return 0;
	DArray_Resize( dim, D, (void*)1 );
	DArray_Resize( acc, D, (void*)1 );
	DArray_Resize( mido, D, 0 );
	DArray_Resize( midn, D, 0 );
	dm = dim->items.pSize;
	ac = acc->items.pSize;
	mdo = mido->items.pSize;
	mdn = midn->items.pSize;
	for(i=0; i<D; i++) dm[ pm[i] ] = 0;
	for(i=0; i<D; i++){
		if( dm[i] ){
			DArray_Delete( dim );
			return 0;
		}
		dm[i] = self->dims->items.pSize[ pm[i] ];
	}
	Array_SetAccum( dm, ac, D );
	for(i=0; i<N; i++){
		int min = i;
		k = i;
		while(1){
			Array_FlatIndex2Mult( dm, D, k, mdn );
			for(j=0; j<D; j++) mdo[j] = mdn[ pm[j] ];
			Array_MultIndex2Flat( self->dimAccum->items.pSize, D, & k, mdo );
			/* printf( "i = %i; k = %i\n", i, k ); */
			if( k <= min ){
				min = k;
				break;
			}
		}
		/* printf( "i = %i; min = %i\n", i, min ); */
		if( min == i ){
			k = i;
			switch( self->numType ){
			case DAO_INTEGER :  ival = self->data.i[i]; break;
			case DAO_FLOAT :   fval = self->data.f[i]; break;
			case DAO_DOUBLE :   dval = self->data.d[i]; break;
			case DAO_COMPLEX : c16val = self->data.c[i]; break;
			default : break;
			}
			while(1){
				Array_FlatIndex2Mult( dm, D, k, mdn );
				for(j=0; j<D; j++) mdo[j] = mdn[ pm[j] ];
				Array_MultIndex2Flat( self->dimAccum->items.pSize, D, & k2, mdo );
				/* printf( "i = %i; k2 = %i\n", i, k2 ); */
				switch( self->numType ){
				case DAO_INTEGER :
					self->data.i[k] = ( k2 == min ) ? ival : self->data.i[k2];
					break;
				case DAO_FLOAT :
					self->data.f[k] = ( k2 == min ) ? fval : self->data.f[k2];
					break;
				case DAO_DOUBLE :
					self->data.d[k] = ( k2 == min ) ? dval : self->data.d[k2];
					break;
				case DAO_COMPLEX :
					self->data.c[k] = ( k2 == min ) ? c16val : self->data.c[k2];
					break;
				default : break;
				}
				if( k2 == min ) break;
				k = k2;
			}
		}
	}
	DaoArray_Reshape( self, dm, D );
	return 1;
}
static void DaoArray_UpdateDimAccum( DaoArray *self )
{
	int i;
	size_t *dims, *dimAccum;

	DArray_Resize( self->dimAccum, self->dims->size, (void*)1 );
	dims = self->dims->items.pSize;
	dimAccum = self->dimAccum->items.pSize;
	dimAccum[ self->dims->size-1 ] = 1;
	for( i=self->dims->size-2; i >=0; i-- ) dimAccum[i] = dimAccum[i+1] * dims[i+1];
	self->size = (int)( dims[0] * dimAccum[0] );
}

static void MakeIndex( DaoContext *ctx, DValue pid, int N, DArray *ids )
{
	int j, id, from, to;
	if( pid.t == 0 ){
		DArray_Resize( ids, N, 0 );
		for(j=0; j<N; j++) ids->items.pSize[j] = j;
		return;
	}
	switch( pid.t ){
	case DAO_INTEGER :
	case DAO_FLOAT :
	case DAO_DOUBLE :
		{
			id = DValue_GetInteger( pid );
			if( id < 0 ) id += N;
			DArray_Clear( ids );
			DArray_Append( ids, id );
			break;
		}
	case DAO_PAIR :
		{
			DaoPair *pair = pid.v.pair;
			DValue first = pair->first;
			DValue second = pair->second;
			from = 0;
			to = N-1;
			if( first.t > DAO_DOUBLE || second.t > DAO_DOUBLE )
				DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "need number" );
			if( first.t ) from = DValue_GetInteger( first );
			if( from <0 ) from += N;
			if( second.t ) to = DValue_GetInteger( second );
			if( to <0 ) to += N;
			if( to -from +1 <= 0 ){
				DArray_Clear( ids );
			}else{
				DArray_Resize( ids, to-from+1, 0 );
				for(j=from; j<=to; j++) ids->items.pSize[ j-from ]=j;
			}
			break;
		}
	case DAO_TUPLE :
		{
			DValue *data = pid.v.tuple->items->data;
			DArray_Clear( ids );
			if( data[0].t == data[1].t && data[0].t == DAO_INTEGER ){
				if( pid.v.tuple->unitype == dao_type_for_iterator ){
					DArray_Append( ids, data[1].v.i );
					data[1].v.i += 1;
					data[0].v.i = data[1].v.i < N;
				}else{
					from = data[0].v.i;
					to   = data[1].v.i;
					DArray_Resize( ids, to-from+1, 0 );
					for(j=from; j<=to; j++) ids->items.pSize[ j-from ]=j;
				}
			}
			break;
		}
	case DAO_LIST :
		{
			DaoList *list = pid.v.list;
			DValue *v = list->items->data;
			DArray_Resize( ids, list->items->size, 0 );
			for( j=0; j<list->items->size; j++){
				if( v[j].t < DAO_INTEGER || v[j].t > DAO_DOUBLE )
					DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "need number" );
				ids->items.pSize[j] = DValue_GetInteger( v[j] );
			}
			break;
		}
	case DAO_ARRAY :
		{
			DaoArray *na = pid.v.array;
			size_t *p;

			if( na->numType == DAO_COMPLEX ){
				DaoContext_RaiseException( ctx, DAO_ERROR_INDEX,
						"complex array can not be used as index" );
				break;
			}
			DArray_Resize( ids, na->size, 0 );
			p = ids->items.pSize;
			for( j=0; j<na->size; j++) p[j] = (int)DaoArray_GetInteger( na, j );
			break;
		}
	default: break;

	}
}
static void DaoMapNumIndex( DaoArray *self, DArray *idMul, DArray *idMap )
{
	/* Have a look at this to see how indices are enumerated
	 * 4  3  2:
	 * 6  2  1: multi
	 *         res:  
	 * i=0:  0  0  0
	 * i=1:  0  0  1
	 * i=2:  0  1  0
	 * i=3:  0  1  1
	 * i=4:  0  2  0
	 * i=5:  0  2  1
	 * i=6:  1  0  0
	 * i=numElem-1:  ...
	 */

	int N = 1;
	int i, j;
	size_t *idMapped, *dimAccum;

	for(i=0; i<idMul->size; i++ ) N *= idMul->items.pArray[i]->size;
	DArray_Resize( idMap, N, 0 );

	idMapped = idMap->items.pSize;
	dimAccum = self->dimAccum->items.pSize;
	for( i=0; i<N; i++ ){
		int mod = i;
		idMapped[i] = 0;
		for( j=(int)idMul->size-1; j>=0; j-- ){
			DArray *sub = idMul->items.pArray[j];
			int res = mod % sub->size;
			mod /= sub->size;
			idMapped[i] += sub->items.pSize[ res ] * dimAccum[j];
		}
	}
}
static void DaoMakeNumIndex( DaoArray *self, DaoContext *ctx, DValue *idx, int N,
		DArray *idMul, DArray *idMap )
{
	int i, j;
	const int D = self->dims->size;
	size_t *dims = self->dims->items.pSize;
	DArray *tmpArray = DArray_New(0);
	/* idMul: DArray<DArray<int> > */
	DArray_Clear( idMul );
	for(i=0; i<D; i ++){
		DArray_Resize( tmpArray, (int)dims[i], 0 );
		for(j=0; j<dims[i]; j++ ) tmpArray->items.pSize[j] = j;
		DArray_Append( idMul, tmpArray );
	}
	DArray_Delete( tmpArray );
	if( N == 1 ){
		if( D ==2 && ( dims[0] ==1 || dims[1] ==1 ) ){
			/* For vectors: */
			int k = (dims[0] == 1);
			DArray_Resize( idMul->items.pArray[ ! k ], 1, 0 );
			MakeIndex( ctx, idx[0], (int)dims[k], idMul->items.pArray[k] );
		}else{
			MakeIndex( ctx, idx[0], (int)dims[0], idMul->items.pArray[0] );
		}
	}else{
		const int n = N > D ? D : N;
		for( i=0; i<n; i++ ) MakeIndex( ctx, idx[i], (int)dims[i], idMul->items.pArray[i] );
	}
	DaoMapNumIndex( self, idMul, idMap );
}
static void SliceRange( DArray *slice, int first, int count )
{
	if( count <0 ) count = 0;
	DArray_Resize( slice, 3, 0 );
	slice->items.pSize[0] = SLICE_RANGE;
	slice->items.pSize[1] = count;
	slice->items.pSize[2] = first;
}
static void MakeSlice( DaoContext *ctx, DValue pid, int N, DArray *slice )
{
	int j, id, from, to;
	if( pid.t == 0 ){
		SliceRange( slice, 0, N );
		return;
	}
	switch( pid.t ){
	case DAO_INTEGER :
	case DAO_FLOAT :
	case DAO_DOUBLE :
		{
			id = DValue_GetInteger( pid );
			if( id < 0 ) id += N;
			SliceRange( slice, id, 1 );
			break;
		}
	case DAO_PAIR :
		{
			DaoPair *pair = pid.v.pair;
			DValue first = pair->first;
			DValue second = pair->second;
			from = 0;
			to = N-1;
			if( first.t > DAO_DOUBLE || second.t > DAO_DOUBLE )
				DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "need number" );
			if( first.t ) from = DValue_GetInteger( first );
			if( from <0 ) from += N;
			if( second.t ) to = DValue_GetInteger( second );
			if( to <0 ) to += N;
			if( to -from +1 <= 0 ){
				SliceRange( slice, from, 0 );
			}else{
				SliceRange( slice, from, to-from+1 );
			}
			break;
		}
	case DAO_TUPLE :
		{
			DValue *data = pid.v.tuple->items->data;
			DArray_Clear( slice );
			if( data[0].t == data[1].t && data[0].t == DAO_INTEGER ){
				if( pid.v.tuple->unitype == dao_type_for_iterator ){
					SliceRange( slice, data[1].v.i, 1 );
					data[1].v.i += 1;
					data[0].v.i = data[1].v.i < N;
				}else{
					from = data[0].v.i;
					to   = data[1].v.i;
					SliceRange( slice, from, to-from+1 );
				}
			}
			break;
		}
	case DAO_LIST :
		{
			DaoList *list = pid.v.list;
			DValue *v = list->items->data;
			DArray_Resize( slice, list->items->size + 2, 0 );
			slice->items.pSize[0] = SLICE_ENUM;
			slice->items.pSize[1] = list->items->size;
			for( j=0; j<list->items->size; j++){
				if( v[j].t < DAO_INTEGER || v[j].t > DAO_DOUBLE )
					DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "need number" );
				slice->items.pSize[j+2] = DValue_GetInteger( v[j] );
			}
			break;
		}
	case DAO_ARRAY :
		{
			DaoArray *na = pid.v.array;
			size_t *p;

			if( na->numType == DAO_COMPLEX ){
				DaoContext_RaiseException( ctx, DAO_ERROR_INDEX,
						"complex array can not be used as index" );
				break;
			}
			DArray_Resize( slice, na->size + 2, 0 );
			slice->items.pSize[0] = SLICE_ENUM;
			slice->items.pSize[1] = na->size;
			p = slice->items.pSize + 2;
			for( j=0; j<na->size; j++) p[j] = (int)DaoArray_GetInteger( na, j );
			break;
		}
	default: break;

	}
}
static int DaoArray_MakeSlice( DaoArray *self, DaoContext *ctx, DValue *idx, int N, DArray *slice )
{
	DArray *tmp = DArray_New(0);
	size_t *dims = self->dims->items.pSize;
	int i, j, D = self->dims->size;
	int S = D != 0;
	/* slice: DArray<DArray<int> > */
	DArray_Clear( slice );
	DArray_Resize( tmp, 3, 0 );
	tmp->items.pSize[0] = SLICE_RANGE;
	tmp->items.pSize[2] = 0;
	for(i=0; i<D; i ++){
		tmp->items.pSize[1] = dims[i];
		DArray_Append( slice, tmp );
	}
	DArray_Delete( tmp );
	if( N == 1 ){
		if( D ==2 && ( dims[0] ==1 || dims[1] ==1 ) ){
			/* For vectors: */
			int k = dims[0] == 1;
			MakeSlice( ctx, idx[0], (int)dims[k], slice->items.pArray[k] );
		}else{
			MakeSlice( ctx, idx[0], (int)dims[0], slice->items.pArray[0] );
		}
	}else{
		const int n = N > D ? D : N;
		for( i=0; i<n; i++ ) MakeSlice( ctx, idx[i], (int)dims[i], slice->items.pArray[i] );
	}
	for(i=0; i<D; i ++) S *= slice->items.pArray[i]->items.pSize[1];
	return S;
}
static int DaoArray_AlignShape( DaoArray *self, DArray *sidx, DArray *dims )
{
	int i;
	size_t *dself = self->dims->items.pSize;
	size_t *ddims = dims->items.pSize;

	if( self->dims->size != dims->size ) return 0;
	if( sidx ){
		for(i=0; i<dims->size; i++) if( sidx->items.pArray[i]->size != ddims[i] ) return 0;
	}else{
		for(i=0; i<dims->size; i++) if( dself[i] != ddims[i] ) return 0;
	}
	return 1;
}
static int DaoArray_MatchShape( DaoArray *self, DaoArray *other )
{
	DaoArray *sRef = self->reference;
	DaoArray *oRef = other->reference;
	int i, m = 0;
	if( sRef && oRef ){
		if( self->slice->size != other->slice->size ) return -1;
		m = self->slice->size != 0;
		for(i=0; i<self->slice->size; i++){
			int n1 = self->slice->items.pArray[i]->items.pSize[1];
			int n2 = other->slice->items.pArray[i]->items.pSize[1];
			if( n1 != n2 ) return -1;
			m *= n1;
		}
	}else if( sRef ){
		if( self->slice->size != other->dims->size ) return -1;
		m = self->slice->size != 0;
		for(i=0; i<self->slice->size; i++){
			int n1 = self->slice->items.pArray[i]->items.pSize[1];
			int n2 = other->dims->items.pSize[i];
			if( n1 != n2 ) return -1;
			m *= n1;
		}
	}else if( oRef ){
		if( self->dims->size != other->slice->size ) return -1;
		m = self->dims->size != 0;
		for(i=0; i<self->dims->size; i++){
			int n1 = self->dims->items.pSize[i];
			int n2 = other->slice->items.pArray[i]->items.pSize[1];
			if( n1 != n2 ) return -1;
			m *= n1;
		}
	}else{
		if( self->dims->size != other->dims->size ) return -1;
		m = self->dims->size != 0;
		for(i=0; i<self->dims->size; i++){
			int n1 = self->dims->items.pSize[i];
			int n2 = other->dims->items.pSize[i];
			if( n1 != n2 ) return -1;
			m *= n1;
		}
	}
	return m;
}
static int DaoArray_SliceSize( DaoArray *self )
{
	int i, m;
	if( self->reference == NULL || self->slice == NULL ) return self->size;
	return self->subSize;
	m = self->slice->size != 0;
	for(i=0; i<self->slice->size; i++){
		m *= self->slice->items.pArray[i]->items.pSize[1];
	}
	return m;
}
int DaoArray_GetInteger( DaoArray *na, int i )
{
	switch( na->numType ){
	case DAO_INTEGER: return na->data.i[i];
	case DAO_FLOAT : return na->data.f[i];
	case DAO_DOUBLE : return (float)na->data.d[i];
	case DAO_COMPLEX : return (float)na->data.c[i].real;
	default : break;
	}
	return 0;
}
float DaoArray_GetFloat( DaoArray *na, int i )
{
	switch( na->numType ){
	case DAO_INTEGER: return na->data.i[i];
	case DAO_FLOAT : return na->data.f[i];
	case DAO_DOUBLE : return (float)na->data.d[i];
	case DAO_COMPLEX : return (float)na->data.c[i].real;
	default : break;
	}
	return 0;
}
double DaoArray_GetDouble( DaoArray *na, int i )
{
	switch( na->numType ){
	case DAO_INTEGER: return na->data.i[i];
	case DAO_FLOAT : return na->data.f[i];
	case DAO_DOUBLE : return na->data.d[i];
	case DAO_COMPLEX : return na->data.c[i].real;
	default : break;
	}
	return 0;
}
complex16 DaoArray_GetComplex( DaoArray *na, int i )
{
	complex16 com = {0,0};
	switch( na->numType ){
	case DAO_INTEGER : com.real = na->data.i[i]; break;
	case DAO_FLOAT : com.real = na->data.f[i]; break;
	case DAO_DOUBLE : com.real = na->data.d[i]; break;
	case DAO_COMPLEX : return na->data.c[i];
	default : break;
	}
	return com;
}

int DaoArray_IndexFromSlice( DaoArray *self, DArray *slice, int sid );
void DaoArray_number_op_array( DaoArray *C, DValue A, DaoArray *B, short op, DaoContext *ctx );
void DaoArray_array_op_number( DaoArray *C, DaoArray *A, DValue B, short op, DaoContext *ctx );
void DaoArray_ArrayArith( DaoArray *s, DaoArray *l, DaoArray *r, short p, DaoContext *c );
static void DaoArray_Print( DValue *dbase, DaoContext *ctx, DaoStream *stream, DMap *cycData );

static void DaoArray_GetItem( DValue *dbase, DaoContext *ctx, DValue pid )
{
	DaoArray *na, *self = dbase->v.array;
	DValue *vs = & pid;
	size_t *idm, *dims = self->dims->items.pSize;
	int i, N = 1;
	if( pid.t == DAO_TUPLE ){
		N = pid.v.tuple->items->size;
		vs = pid.v.tuple->items->data;
	}
	/* if( self->unitype ) printf( "DaoArray_GetItem: %s\n", self->unitype->name->mbs ); */

	if( pid.t >= DAO_INTEGER && pid.t <= DAO_DOUBLE ){
		int id = DValue_GetInteger( pid );
		if( id < 0 || id >= self->size ){
			DaoContext_RaiseException( ctx, DAO_ERROR_INDEX_OUTOFRANGE, "" );
			return;
		}
		switch( self->numType ){
		case DAO_INTEGER : DaoContext_PutInteger( ctx, self->data.i[id] ); break;
		case DAO_FLOAT : DaoContext_PutFloat( ctx, self->data.f[id] ); break;
		case DAO_DOUBLE : DaoContext_PutDouble( ctx, self->data.d[id] ); break;
		case DAO_COMPLEX : DaoContext_PutComplex( ctx, self->data.c[id] ); break;
		default : break;
		}
		return;
	}else if( pid.t == DAO_TUPLE && pid.v.tuple->unitype == dao_type_for_iterator ){
		DaoArray *array = self;
		DValue *data = pid.v.tuple->items->data;
		int size = self->size;
		int id = data[1].v.i;
		if( self->reference && self->slice ){
			array = self->reference;
			size = self->subSize;
		}
		if( data[1].t != DAO_INTEGER || id < 0 || id >= size ){
			DaoContext_RaiseException( ctx, DAO_ERROR_INDEX_OUTOFRANGE, "index out of range" );
			return;
		}
		if( self->reference && self->slice ){
			id = DaoArray_IndexFromSlice( self->reference, self->slice, id );
		}
		switch( array->numType ){
		case DAO_INTEGER : DaoContext_PutInteger( ctx, array->data.i[id] ); break;
		case DAO_FLOAT : DaoContext_PutFloat( ctx, array->data.f[id] ); break;
		case DAO_DOUBLE : DaoContext_PutDouble( ctx, array->data.d[id] ); break;
		case DAO_COMPLEX : DaoContext_PutComplex( ctx, array->data.c[id] ); break;
		default : break;
		}
		data[1].v.i += 1;
		data[0].v.i = data[1].v.i < size;
		return;
	}else if( pid.t == DAO_TUPLE && N == self->dims->size ){
		int allNumbers = 1;
		int k, idFlat = 0;
		size_t *dimAccum = self->dimAccum->items.pSize;
		for(i=0; i<N; i++){
			if( vs[i].t < DAO_INTEGER || vs[i].t > DAO_DOUBLE ){
				allNumbers = 0;
				break;
			}
			k = DValue_GetInteger( vs[i] );
			idFlat += k * dimAccum[i];
			if( k >= dims[i] ){
				idFlat = self->size; /* to raise exception */
				break;
			}
		}
		if( idFlat >= self->size ){
			DaoContext_RaiseException( ctx, DAO_ERROR_INDEX_OUTOFRANGE, "index out of range" );
			return;
		}
		if( allNumbers ){
			if( self->numType == DAO_INTEGER ){
				DaoContext_PutInteger( ctx, DaoArray_GetInteger( self, idFlat ) );
			}else if( self->numType == DAO_FLOAT ){
				DaoContext_PutFloat( ctx, DaoArray_GetFloat( self, idFlat ) );
			}else if( self->numType == DAO_DOUBLE ){
				DaoContext_PutDouble( ctx, DaoArray_GetDouble( self, idFlat ) );
			}else{
				DaoContext_PutComplex( ctx, DaoArray_GetComplex( self, idFlat ) );
			}
			return;
		}
	}
	na = DaoContext_PutArray( ctx );
	GC_ShiftRC( self->unitype, na->unitype );
	GC_ShiftRC( self, na->reference );
	na->numType = self->numType;
	na->unitype = self->unitype;
	na->reference = self;
	if( na->slice == NULL ) na->slice = DArray_New(D_ARRAY);
	na->subSize = DaoArray_MakeSlice( self, ctx, vs, N, na->slice );
}
static void DaoArray_SetOneItem( DaoArray *self, int id, DValue value, int op )
{
	int ival;
	float fval;
	double dval;
	complex16 c16;
	complex16 cval;

	switch( self->numType ){
	case DAO_INTEGER :
		ival = DValue_GetInteger( value );
		switch( op ){
		case DVM_ADD : self->data.i[ id ] += ival; break;
		case DVM_SUB : self->data.i[ id ] -= ival; break;
		case DVM_MUL : self->data.i[ id ] *= ival; break;
		case DVM_DIV : self->data.i[ id ] /= ival; break;
		case DVM_MOD : self->data.i[ id ] %= ival; break;
		case DVM_AND : self->data.i[ id ] &= ival; break;
		case DVM_OR  : self->data.i[ id ] |= ival; break;
		default : self->data.i[ id ] = ival; break;
		}
		break;
	case DAO_FLOAT :
		fval = DValue_GetFloat( value );
		switch( op ){
		case DVM_ADD : self->data.f[ id ] += fval; break;
		case DVM_SUB : self->data.f[ id ] -= fval; break;
		case DVM_MUL : self->data.f[ id ] *= fval; break;
		case DVM_DIV : self->data.f[ id ] /= fval; break;
		default : self->data.f[ id ] = fval; break;
		}
		break;
	case DAO_DOUBLE :
		dval = DValue_GetDouble( value );
		switch( op ){
		case DVM_ADD : self->data.d[ id ] += dval; break;
		case DVM_SUB : self->data.d[ id ] -= dval; break;
		case DVM_MUL : self->data.d[ id ] *= dval; break;
		case DVM_DIV : self->data.d[ id ] /= dval; break;
		default : self->data.d[ id ] = dval; break;
		}
		break;
	case DAO_COMPLEX :
		cval = DValue_GetComplex( value );
		c16 = self->data.c[ id ];
		switch( op ){
		case DVM_ADD : COM_IP_ADD( c16, cval ); break;
		case DVM_SUB : COM_IP_SUB( c16, cval ); break;
		case DVM_MUL : COM_IP_MUL( c16, cval ); break;
		case DVM_DIV : COM_IP_DIV( c16, cval ); break;
		default : COM_ASSN( c16, cval ); break;
		}
		self->data.c[ id ] = c16;
		break;
	default : break;
	}
}
static void DaoArray_SetComplex( DaoArray *self, complex16 c16 )
{
	int i, N = self->size;
	double vd = c16.real;
	int vi = (int) vd;
	float vf = (float) vd;
	switch( self->numType ){
	case DAO_INTEGER : for(i=0; i<N; i++) self->data.i[i] = vi; break;
	case DAO_FLOAT : for(i=0; i<N; i++) self->data.f[i] = vf; break;
	case DAO_DOUBLE : for(i=0; i<N; i++) self->data.d[i] = vd; break;
	case DAO_COMPLEX : for(i=0; i<N; i++) self->data.c[i] = c16; break;
	default : break;
	}
}
static void DaoArray_SetReal( DaoArray *self, double vd )
{
	complex16 c16 = { 0.0, 0.0 };
	c16.real = vd;
	DaoArray_SetComplex( self, c16 );
}
int DaoArray_CopyArray( DaoArray *self, DaoArray *other )
{
	int i, N = self->size;
	if( DaoArray_AlignShape( self, NULL, other->dims ) ==0 ) return 0;
	switch( self->numType | (other->numType << 4) ){
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
int DaoArray_Compare( DaoArray *x, DaoArray *y )
{
	int *xi = x->data.i, *yi = y->data.i;
	float *xf = x->data.f, *yf = y->data.f;
	double *xd = x->data.d, *yd = y->data.d;
	complex16 *xc = x->data.c, *yc = y->data.c;
	int min = x->size < y->size ? x->size : y->size;
	int i = 0, cmp = 0;
	if( x->numType == DAO_INTEGER && y->numType == DAO_INTEGER ){
		while( i < min && *xi == *yi ) i++, xi++, yi++;
		if( i < min ) return *xi < *yi ? -1 : 1;
	}else if( x->numType == DAO_FLOAT && y->numType == DAO_FLOAT ){
		while( i < min && *xf == *yf ) i++, xf++, yf++;
		if( i < min ) return *xf < *yf ? -1 : 1;
	}else if( x->numType == DAO_DOUBLE && y->numType == DAO_DOUBLE ){
		while( i < min && *xd == *yd ) i++, xd++, yd++;
		if( i < min ) return *xd < *yd ? -1 : 1;
	}else if( x->numType == DAO_COMPLEX && y->numType == DAO_COMPLEX ){
		while( i < min && xc->real == yc->real && xc->imag == yc->imag ) i++, xc++, yc++;
		if( i < min ){
			if( xc->real == yc->real && xc->imag == yc->imag ) return 0;
			if( xc->real == yc->real ) return xc->imag < yc->imag ? -1 : 1;
			if( xc->imag == yc->imag ) return xc->real < yc->real ? -1 : 1;
		}
	}else if( x->numType == DAO_COMPLEX ){
		while( i < min && xc->real == DaoArray_GetDouble( y, i ) && xc->imag ==0 ) i++, xc++;
		if( i < min ){
			double v = DaoArray_GetDouble( y, i );
			if( xc->real == v && xc->imag == 0 ) return 0;
			if( xc->real == v ) return xc->imag < 0 ? -1 : 1;
			if( xc->imag == 0 ) return xc->real < v ? -1 : 1;
		}
	}else if( y->numType == DAO_COMPLEX ){
		while( i < min && yc->real == DaoArray_GetDouble( x, i ) && yc->imag ==0 ) i++, yc++;
		if( i < min ){
			double v = DaoArray_GetDouble( x, i );
			if( v == yc->real && 0 == yc->imag ) return 0;
			if( v == yc->real ) return 0 < yc->imag ? -1 : 1;
			if( 0 == yc->imag ) return v < yc->real ? -1 : 1;
		}
	}else{
		while( i < min && DaoArray_GetDouble( x, i ) == DaoArray_GetDouble( y, i ) ) i++;
		if( i < min ){
			double xv = DaoArray_GetDouble( x, i );
			double yv = DaoArray_GetDouble( y, i );
			if( xv == yv ) return 0;
			return xv < yv ? -1 : 1;
		}
	}
	if( x->size == y->size  ) return 0;
	return x->size > y->size ? 1 : -1;
}
void DaoArray_SetItem( DValue *va, DaoContext *ctx, DValue pid, DValue value, int op )
{
	DaoArray *self = va->v.array;
	DValue *vs = & pid;
	size_t *dims = self->dims->items.pSize;
	int i, N = 1;
	if( pid.t == DAO_TUPLE ){
		N = pid.v.tuple->items->size;
		vs = pid.v.tuple->items->data;
	}
	DaoArray_Sliced( self );
	if( value.t ==0 ) return;
	if( pid.t == 0 ){
		if( value.t >= DAO_INTEGER && value.t <= DAO_COMPLEX ){
			DaoArray_array_op_number( self, self, value, op, ctx );
		}else if( value.t == DAO_ARRAY ){
			DaoArray_ArrayArith( self, self, value.v.array, op, ctx );
		}else{
			DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "" );
		}
		return;
	}else if( pid.t >= DAO_INTEGER && pid.t <= DAO_DOUBLE && value.t <= DAO_LONG ){
		int id = DValue_GetInteger( pid );
		if( id < 0 || id >= self->size ){
			DaoContext_RaiseException( ctx, DAO_ERROR_INDEX_OUTOFRANGE, "" );
			return;
		}
		DaoArray_SetOneItem( self, id, value, op );
		return;
	}else if( pid.t == DAO_TUPLE && N == self->dims->size ){
		size_t *dimAccum = self->dimAccum->items.pSize;
		int allNumbers = 1;
		int k, idFlat = 0;
		for(i=0; i<N; i++){
			if( vs[i].t < DAO_INTEGER || vs[i].t > DAO_DOUBLE ){
				allNumbers = 0;
				break;
			}
			k = DValue_GetInteger( vs[i] );
			idFlat += k * dimAccum[i];
			if( k >= dims[i] ){
				idFlat = self->size; /* to raise exception */
				break;
			}
		}
		if( idFlat >= self->size ){
			DaoContext_RaiseException( ctx, DAO_ERROR_INDEX_OUTOFRANGE, "index out of range" );
			return;
		}
		if( allNumbers ){
			DaoArray_SetOneItem( self, idFlat, value, op );
			return;
		}
	}
	if( self->slice == NULL ) self->slice = DArray_New(D_ARRAY);
	self->subSize = DaoArray_MakeSlice( self, ctx, vs, N, self->slice );
	self->reference = self;
	if( value.t == DAO_ARRAY ){
		DaoArray *na = value.v.array;
		DaoArray_ArrayArith( self, self, na, op, ctx );
	}else{
		DaoArray_array_op_number( self, self, value, op, ctx );
	}
EndOf_DaoArray_SetItem :
	self->reference = NULL;
}
static void DaoArray_SetItem2( DValue *dbase, DaoContext *ctx, DValue pid, DValue value )
{
	DaoArray_SetItem( dbase, ctx, pid, value, DVM_MOVE );
}
static void DaoArray_PrintElement( DaoArray *self, DaoStream *stream, int i )
{
	switch( self->numType ){
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
static void DaoArray_Print( DValue *dbase, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
	DaoArray *self = dbase->v.array;
	int i, j, n = self->size * BBITS;
	size_t *tmp, *dims = self->dims->items.pSize;

	if( self->dims->size < 2 ) return;
	if( self->dims->size==2 && ( dims[0] ==1 || dims[1] ==1 ) ){
		/* For vectors: */
		char *sep = (dims[0] >1 && dims[1] ==1) ? "; " : ", ";
		DaoStream_WriteMBS( stream, "[ " );
		for(i=0; i<self->size; i++){
			DaoArray_PrintElement( self, stream, i );
			if( i+1 < self->size ) DaoStream_WriteMBS( stream, sep );
		}
		DaoStream_WriteMBS( stream, " ]" );
	}else{
		DArray *tmpArray = DArray_New(0);
		DArray_Resize( tmpArray, self->dims->size, 0 );
		tmp = tmpArray->items.pSize;
		for(i=0; i<self->size; i++){
			int mod = i;
			for(j=self->dims->size-1; j >=0; j--){
				int res = ( mod % dims[j] );
				mod /= dims[j];
				tmp[j] = res;
			}
			if( tmp[self->dims->size-1] ==0 ){
				DaoStream_WriteMBS( stream, "row[" );
				for(j=0; j+1<self->dims->size; j++){
					DaoStream_WriteFormatedInt( stream, (int)tmp[j], "%i" );
					DaoStream_WriteMBS( stream, "," );
				}
				DaoStream_WriteMBS( stream, ":]:\t" );
			}
			DaoArray_PrintElement( self, stream, i );
			if( i+1 < self->size ) DaoStream_WriteMBS( stream, "\t" );
			if( tmp[self->dims->size-1] +1 == dims[self->dims->size-1] )
				DaoStream_WriteMBS( stream, "\n" );
		}
		DArray_Delete( tmpArray );
	}
}
static DValue DaoNA_Copy( DValue *dbase, DaoContext *ctx, DMap *cycData )
{
	DValue val = daoNullArray;
	val.v.array = DaoArray_Copy( dbase->v.array );
	return val;
}

static DaoTypeCore numarrCore =
{
	0, NULL, NULL, NULL, NULL,
	DaoBase_GetField,
	DaoBase_SetField,
	DaoArray_GetItem,
	DaoArray_SetItem2,
	DaoArray_Print,
	DaoNA_Copy,
};
static void DaoArray_Lib_Dim( DaoContext *ctx, DValue *par[], int N )
{
	DaoArray *self = par[0]->v.array;
	int *v;
	int i;

	if( N == 1 ){
		DaoArray *na = DaoContext_PutArray( ctx );
		na->numType = DAO_INTEGER;
		if( self->reference ){
			DaoArray_ResizeVector( na, self->slice->size );
			v = na->data.i;
			for(i=0; i<self->slice->size; i++ )
				v[i] = self->slice->items.pArray[i]->items.pSize[1];
		}else{
			DaoArray_ResizeVector( na, self->dims->size );
			v = na->data.i;
			for(i=0; i<self->dims->size; i++ ) v[i] = self->dims->items.pSize[i];
		}
	}else if( self->reference ){
		dint *num = DaoContext_PutInteger( ctx, 0 );
		i = par[1]->v.i;
		if( i <0 || i >= self->slice->size ){
			*num = -1;
			DaoContext_RaiseException( ctx, DAO_WARNING, "no such dimension" );
		}else{
			*num = self->slice->items.pArray[i]->items.pSize[1];
		}
	}else{
		dint *num = DaoContext_PutInteger( ctx, 0 );
		i = par[1]->v.i;
		if( i <0 || i >= self->dims->size ){
			*num = -1;
			DaoContext_RaiseException( ctx, DAO_WARNING, "no such dimension" );
		}else{
			*num = self->dims->items.pSize[i];
		}
	}
}
static void DaoArray_Lib_Size( DaoContext *ctx, DValue *par[], int N )
{
	DaoArray *self = par[0]->v.array;
	DaoContext_PutInteger( ctx, DaoArray_SliceSize( self ) );
}
static void DaoArray_Lib_Resize( DaoContext *ctx, DValue *par[], int N )
{
	DaoArray *self = par[0]->v.array;
	DaoArray *nad = par[1]->v.array;
	DArray *ad;
	size_t *dims;
	int i, size = 1;

	DaoArray_Sliced( self );
	DaoArray_Sliced( nad );

	if( nad->numType == DAO_COMPLEX ){
		DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "invalid dimension" );
		return;
	}
	ad = DArray_New(0);
	DArray_Resize( ad, nad->size, 0 );
	dims = ad->items.pSize;

	for(i=0; i<nad->size; i++){
		dims[i] = DaoArray_GetInteger( nad, i );
		size *= dims[i];
	}
	DaoContext_SetResult( ctx, (DaoBase*)self );
	if( (ctx->vmSpace->options & DAO_EXEC_SAFE) && size > 1000 ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
		DArray_Delete( ad );
		return;
	}
	DaoArray_ResizeArray( self, dims, ad->size );
	DArray_Delete( ad );
}
static void DaoArray_Lib_Reshape( DaoContext *ctx, DValue *par[], int N )
{
	DaoArray *self = par[0]->v.array;
	DaoArray *nad = par[1]->v.array;
	DArray *ad;
	int i, size;
	size_t *dims;

	DaoArray_Sliced( self );
	DaoArray_Sliced( nad );

	if( nad->numType == DAO_COMPLEX ){
		DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "invalid dimension" );
		return;
	}
	ad = DArray_New(0);
	DArray_Resize( ad, nad->size, 0 );
	dims = ad->items.pSize;
	size = 1;
	for(i=0; i<nad->size; i++){
		dims[i] = DaoArray_GetInteger( nad, i );
		size *= (int)dims[i];
	}
	if( self->owner && self->size != size ){
		DArray_Delete( ad );
		DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "invalid dimension" );
		return;
	}
	DArray_Resize( self->dims, ad->size, 0 );
	memcpy( self->dims->items.pSize, dims, ad->size * sizeof(size_t) );
	DaoArray_UpdateDimAccum( self );
	DaoContext_SetResult( ctx, (DaoBase*)self );
	DArray_Delete( ad );
}
static void DaoArray_Lib_Index( DaoContext *ctx, DValue *par[], int N )
{
	DaoArray *self = par[0]->v.array;
	DaoArray *na = DaoContext_PutArray( ctx );
	size_t *dim = self->dims->items.pSize;
	int i, D = self->dims->size;
	int sd = par[1]->v.i;
	int *v;

	DaoArray_Sliced( self );
	dim = self->dims->items.pSize;
	D = self->dims->size;

	na->numType = DAO_INTEGER;
	DaoArray_ResizeVector( na, self->dims->size );
	v = na->data.i;
	for(i=D-1; i>=0; i--){
		v[i] = sd % dim[i];
		sd = sd / dim[i];
	}
}
static void DaoArray_Lib_max( DaoContext *ctx, DValue *par[], int N )
{
	DaoTuple *tuple = DaoContext_PutTuple( ctx );
	DaoArray *self = par[0]->v.array;
	int i, k, size, cmp=0, imax = -1;
	tuple->items->data[0].t = self->numType;
	tuple->items->data[1].t = DAO_INTEGER;
	tuple->items->data[1].v.i = -1;
	if( self->numType == DAO_COMPLEX ) return;/* no exception, guaranteed by the typing system */
	if( DaoArray_SliceSize( self ) == 0 ) return;
	if( self->reference && self->slice ){
		DArray *slice = self->slice;
		size = self->subSize;
		self = self->reference;;
		if( size == 0 ) return;
		imax = DaoArray_IndexFromSlice( self, slice, 0 );
		for(i=1; i<size; i++ ){
			k = DaoArray_IndexFromSlice( self, slice, i );
			switch( self->numType ){
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
			switch( self->numType ){
			case DAO_INTEGER : cmp = self->data.i[imax] < self->data.i[i]; break;
			case DAO_FLOAT  : cmp = self->data.f[imax] < self->data.f[i]; break;
			case DAO_DOUBLE : cmp = self->data.d[imax] < self->data.d[i]; break;
			default : break;
			}
			if( cmp ) imax = i;
		}
	}
	tuple->items->data[1].v.i = imax;
	switch( self->numType ){
	case DAO_INTEGER : tuple->items->data[0].v.i = self->data.i[imax]; break;
	case DAO_FLOAT  : tuple->items->data[0].v.f = self->data.f[imax]; break;
	case DAO_DOUBLE : tuple->items->data[0].v.d = self->data.d[imax]; break;
	default : break;
	}
}
static void DaoArray_Lib_min( DaoContext *ctx, DValue *par[], int N )
{
	DaoTuple *tuple = DaoContext_PutTuple( ctx );
	DaoArray *self = par[0]->v.array;
	int i, k, size, cmp=0, imax = -1;
	tuple->items->data[0].t = self->numType;
	tuple->items->data[1].t = DAO_INTEGER;
	tuple->items->data[1].v.i = -1;
	if( self->numType == DAO_COMPLEX ) return;
	if( DaoArray_SliceSize( self ) == 0 ) return;
	if( self->reference && self->slice ){
		DArray *slice = self->slice;
		size = self->subSize;
		self = self->reference;;
		if( size == 0 ) return;
		imax = DaoArray_IndexFromSlice( self, slice, 0 );
		for(i=1; i<size; i++ ){
			k = DaoArray_IndexFromSlice( self, slice, i );
			switch( self->numType ){
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
			switch( self->numType ){
			case DAO_INTEGER : cmp = self->data.i[imax] > self->data.i[i]; break;
			case DAO_FLOAT  : cmp = self->data.f[imax] > self->data.f[i]; break;
			case DAO_DOUBLE : cmp = self->data.d[imax] > self->data.d[i]; break;
			default : break;
			}
			if( cmp ) imax = i;
		}
	}
	tuple->items->data[1].v.i = imax;
	switch( self->numType ){
	case DAO_INTEGER : tuple->items->data[0].v.i = self->data.i[imax]; break;
	case DAO_FLOAT  : tuple->items->data[0].v.f = self->data.f[imax]; break;
	case DAO_DOUBLE : tuple->items->data[0].v.d = self->data.d[imax]; break;
	default : break;
	}
}
static void DaoArray_Lib_sum( DaoContext *ctx, DValue *par[], int N )
{
	DaoArray *self = par[0]->v.array;
	int i, size;
	if( self->reference && self->slice ){
		DArray *slice = self->slice;
		size = self->subSize;
		self = self->reference;;
		if( size == 0 ) return;
		if( self->numType == DAO_INTEGER ){
			long sum = 0;
			int *v = self->data.i;
			for(i=0; i<size; i++ ) sum += v[ DaoArray_IndexFromSlice( self, slice, i ) ];
			DaoContext_PutInteger( ctx, sum );
		}else if( self->numType == DAO_FLOAT ){
			double sum = 0;
			float *v = self->data.f;
			for(i=0; i<size; i++ ) sum += v[ DaoArray_IndexFromSlice( self, slice, i ) ];
			DaoContext_PutFloat( ctx, sum );
		}else if( self->numType == DAO_DOUBLE ){
			double sum = 0;
			double *v = self->data.d;
			for(i=0; i<size; i++ ) sum += v[ DaoArray_IndexFromSlice( self, slice, i ) ];
			DaoContext_PutDouble( ctx, sum );
		}else{
			complex16 sum = {0,0};
			complex16 *v = self->data.c;
			for(i=0; i<size; i++ ){
				complex16 x = v[ DaoArray_IndexFromSlice( self, slice, i ) ];
				COM_IP_ADD( sum, x );
			}
			DaoContext_PutComplex( ctx, sum );
		}
	}else{
		if( self->numType == DAO_INTEGER ){
			long sum = 0;
			int *v = self->data.i;
			for(i=0; i<self->size; i++ ) sum += v[i];
			DaoContext_PutInteger( ctx, sum );
		}else if( self->numType == DAO_FLOAT ){
			double sum = 0;
			float *v = self->data.f;
			for(i=0; i<self->size; i++ ) sum += v[i];
			DaoContext_PutDouble( ctx, sum );
		}else if( self->numType == DAO_DOUBLE ){
			double sum = 0;
			double *v = self->data.d;
			for(i=0; i<self->size; i++ ) sum += v[i];
			DaoContext_PutDouble( ctx, sum );
		}else{
			complex16 sum = {0,0};
			complex16 *v = self->data.c;
			for(i=0; i<self->size; i++ ) COM_IP_ADD( sum, v[i] );
			DaoContext_PutComplex( ctx, sum );
		}
	}
}
static void DaoArray_Lib_varn( DaoContext *ctx, DValue *par[], int N )
{
	DaoArray *self = par[0]->v.array;
	double *num = DaoContext_PutDouble( ctx, 0.0 );
	double e = 0.0;
	double sum = 0;
	double sum2 = 0;
	double dev;
	int i, size;

	if( self->numType == DAO_COMPLEX ) return;
	if( DaoArray_SliceSize( self ) ==0 ) return;
	if( self->reference && self->slice ){
		DArray *slice = self->slice;
		size = self->subSize;
		self = self->reference;;
		if( size > 0 ){
			for(i=0; i<size; i++ ){
				int k = DaoArray_IndexFromSlice( self, slice, i );
				switch( self->numType ){
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
			switch( self->numType ){
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
static int Compare( DaoArray *array, int *slice, int *index, int i, int j )
{
	if( index ){
		i = index[i];
		j = index[j];
	}
	i = slice[i];
	j = slice[j];
	switch( array->numType ){
	case DAO_INTEGER :
		{
			int a = array->data.i[i];
			int b = array->data.i[j];
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
static void Swap( DaoArray *array, int *slice, int *index, int i, int j )
{
	if( index ){
		int k = index[i];
		index[i] = index[j];
		index[j] = k;
		return;
	}
	i = slice[i];
	j = slice[j];
	switch( array->numType ){
	case DAO_INTEGER :
		{
			int a = array->data.i[i];
			int b = array->data.i[j];
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
static void QuickSort2( DaoArray *array, int *slice, 
		int *index, int first, int last, int part, int asc )
{
	int lower = first+1, upper = last;
	int pivot = (first + last) / 2;
	if( first >= last ) return;
	Swap( array, slice, index, first, pivot );

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
	if( first < upper-1 ) QuickSort2( array, slice, index, first, upper-1, part, asc );
	if( upper >= part ) return;
	if( upper+1 < last ) QuickSort2( array, slice, index, upper+1, last, part, asc );
}
void DaoArray_GetSliceShape( DaoArray *self, DArray *shape );
static void DaoArray_Lib_rank( DaoContext *ctx, DValue *par[], int npar, int asc )
{
	DaoArray *res = DaoContext_PutArray( ctx );
	DaoArray *array = par[0]->v.array;
	DaoArray *ref = array->reference;
	DArray *slice = array->slice;
	dint part = par[1]->v.i;
	int i, N = DaoArray_SliceSize( array );
	int *index;
	int *ids;

	if( res == NULL ) return;
	if( array->numType == DAO_COMPLEX ){
		DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "unable to rank complex array" );
		return;
	}
	if( N == 0 ) return;
	res->numType = DAO_INTEGER;
	DaoArray_GetSliceShape( array, res->dimAccum );
	DaoArray_ResizeArray( res, res->dimAccum->items.pSize, res->dimAccum->size );
	ids = res->data.i;
	for(i=0; i<N; i++) ids[i] = i;

	if( N < 2 ) return;
	if( part ==0 ) part = N;
	index = dao_malloc( N * sizeof(int) );
	for(i=0; i<N; i++){
		index[i] = i;
		ids[i] = ref ? DaoArray_IndexFromSlice( ref, slice, i ) : i;
	}
	QuickSort2( ref ? ref : array, ids, index, 0, N-1, part, asc );
	for(i=0; i<N; i++) ids[i] = index[i];
	dao_free( index );
}
static void DaoArray_Lib_ranka( DaoContext *ctx, DValue *par[], int npar )
{
	DaoArray_Lib_rank( ctx, par, npar, 1 );
}
static void DaoArray_Lib_rankd( DaoContext *ctx, DValue *par[], int npar )
{
	DaoArray_Lib_rank( ctx, par, npar, 0 );
}
static void DaoArray_Lib_sort( DaoContext *ctx, DValue *par[], int npar, int asc )
{
	DaoArray *array = par[0]->v.array;
	DaoArray *ref = array->reference;
	DArray *slice = array->slice;
	dint part = par[1]->v.i;
	int i, N = DaoArray_SliceSize( array );
	int *index;

	DaoContext_PutValue( ctx, *par[0] );
	if( array->numType == DAO_COMPLEX ){
		DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "unable to sort complex array" );
		return;
	}
	if( N < 2 ) return;
	if( part ==0 ) part = N;
	index = dao_malloc( N * sizeof(int) );
	for(i=0; i<N; i++) index[i] = ref ? DaoArray_IndexFromSlice( ref, slice, i ) : i;
	QuickSort2( ref ? ref : array, index, NULL, 0, N-1, part, asc );
	dao_free( index );
}
static void DaoArray_Lib_sorta( DaoContext *ctx, DValue *par[], int npar )
{
	DaoArray_Lib_sort( ctx, par, npar, 1 );
}
static void DaoArray_Lib_sortd( DaoContext *ctx, DValue *par[], int npar )
{
	DaoArray_Lib_sort( ctx, par, npar, 0 );
}

static void DaoArray_Lib_Permute( DaoContext *ctx, DValue *par[], int npar )
{
	DaoArray *self = par[0]->v.array;
	DaoArray *pm = par[1]->v.array;
	DArray *perm;
	int i, D = self->dims->size;
	int res;
	if( pm->dims->items.pSize[0] * pm->dims->items.pSize[1] != pm->size
			|| pm->dims->size >2 || pm->size != D ) goto RaiseException;

	perm = DArray_New(0);
	DArray_Resize( perm, D, 0 );
	for(i=0; i<D; i++) perm->items.pSize[i] = DaoArray_GetInteger( pm, i );
	res = DaoArray_Permute( self, perm );
	DArray_Delete( perm );
	if( res ==0 ) goto RaiseException;
	return;
RaiseException:
	DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "invalid parameter for permute()" );
}
static void DaoArray_Lib_Transpose( DaoContext *ctx, DValue *par[], int npar )
{
	DaoArray *self = par[0]->v.array;
	DArray *perm = DArray_New(0);
	int i, D = self->dims->size;
	DArray_Resize( perm, D, 0 );
	for(i=0; i<D; i++) perm->items.pSize[i] = D-1-i;
	DaoArray_Permute( self, perm );
	DArray_Delete( perm );
}
static void DaoArray_Lib_FFT( DaoContext *ctx, DValue *par[], int npar )
{
	DaoArray *self = par[0]->v.array;
	int inv = ( par[1]->v.e->value == 0 )? -1 : 1;
	int size = self->size;
	int m = 0;
	if( self->numType != DAO_COMPLEX ) return;
	while( size >>= 1 ) m ++;
	if( m == 0 ) return;
	if( size % (1<<m) !=0 ) return;
	if( abs(inv) != 1 ) return;
	dao_fft16( (complex16*) self->data.c, m, inv );
}
static void DaoArray_Lib_Iter( DaoContext *ctx, DValue *p[], int N )
{
	DaoArray *self = p[0]->v.array;
	DaoTuple *tuple = p[1]->v.tuple;
	DValue *data = tuple->items->data;
	DValue iter = DValue_NewInteger(0);
	data[0].v.i = DaoArray_SliceSize( self ) >0;
	DValue_Copy( & data[1], iter );
}
static DaoFuncItem numarMeths[] =
{
	{ DaoArray_Lib_Dim,       "dim( self :array, i : int )=>int" },
	{ DaoArray_Lib_Dim,       "dim( self :array )=>array<int>" },
	{ DaoArray_Lib_Index,     "index( self :array, i : int )=>array<int>" },
	{ DaoArray_Lib_Size,      "size( self :array )=>int" },
	{ DaoArray_Lib_Resize,    "resize( self :array, dims :array<int> )" },
	{ DaoArray_Lib_Reshape,   "reshape( self :array, dims :array<int> )" },

	{ DaoArray_Lib_Permute,   "permute( self :array, dims :array<int> )" },
	{ DaoArray_Lib_Transpose, "transpose( self :array )" },
	{ DaoArray_Lib_max,       "max( self :array<@ITEM> )=>tuple<@ITEM,int>" },
	{ DaoArray_Lib_min,       "min( self :array<@ITEM> )=>tuple<@ITEM,int>" },
	{ DaoArray_Lib_sum,       "sum( self :array<@ITEM> )=>@ITEM" },
	{ DaoArray_Lib_varn,      "varn( self :array )=>double" },
	{ DaoArray_Lib_ranka,     "ranka( self :array, k=0 )=>array<int>" },
	{ DaoArray_Lib_rankd,     "rankd( self :array, k=0 )=>array<int>" },
	{ DaoArray_Lib_sorta,     "sorta( self :array, k=0 )" },
	{ DaoArray_Lib_sortd,     "sortd( self :array, k=0 )" },

	{ DaoArray_Lib_FFT,  "fft( self :array<complex>, direct :enum<forward, backward> )" },
	{ DaoArray_Lib_Iter, "__for_iterator__( self :array<any>, iter : for_iterator )" },
	{ NULL, NULL }
};

int DaoArray_NumType( DaoArray *self )
{
	return self->numType;
}
void DaoArray_SetNumType( DaoArray *self, short numtype )
{
	self->numType = numtype;
	DaoArray_ResizeVector( self, self->size );
}
int DaoArray_Size( DaoArray *self )
{
	return self->size;
}
int DaoArray_DimCount( DaoArray *self )
{
	return self->dims->size;
}
int DaoArray_SizeOfDim( DaoArray *self, int d )
{
	return self->dims->items.pInt[d];
}
void DaoArray_GetShape( DaoArray *self, size_t *dims )
{
	int i;
	for(i=0; i<self->dims->size; i++) dims[i] = self->dims->items.pSize[0];
}
int DaoArray_HasShape( DaoArray *self, size_t *dims, int D )
{
	int i;
	if( D != self->dims->size ) return 0;
	for(i=0; i<self->dims->size; i++)
		if( dims[i] != self->dims->items.pSize[0] ) 
			return 0;
	return 1;
}
int DaoArray_GetFlatIndex( DaoArray *self, size_t *index )
{
	int i, id = 0;
	for( i=0; i<self->dims->size; i++ ) id += index[i] * self->dimAccum->items.pInt[i];
	return id;
}
int DaoArray_Reshape( DaoArray *self, size_t *dims, int D )
{
	int i, size = 1;
	for(i=0; i<D; i++) size *= dims[i];

	if( self->owner && self->size != size ) return 0;
	DArray_Resize( self->dims, D, 0 );
	memcpy( self->dims->items.pInt, dims, D*sizeof(size_t) );
	DaoArray_UpdateDimAccum( self );
	return 1;
}
double* DaoArray_ToDouble( DaoArray *self )
{
	int i, tsize = sizeof(double);
	double *buf;
	if( self->owner ==0 ) return self->data.d;
	if( self->numType == DAO_DOUBLE || self->numType == DAO_COMPLEX ) return self->data.d;
	self->data.p = dao_realloc( self->data.p, (self->size+1) * tsize );
	buf = self->data.d;
	if( self->numType == DAO_INTEGER ){
		for(i=self->size-1; i>=0; i--) buf[i] = self->data.i[i];
	}else if( self->numType == DAO_FLOAT ){
		for(i=self->size-1; i>=0; i--) buf[i] = self->data.f[i];
	}
	return buf;
}
void DaoArray_FromDouble( DaoArray *self )
{
	int i;
	double *buf;
	if( self->numType == DAO_DOUBLE || self->numType == DAO_COMPLEX ) return;
	buf = self->data.d;
	if( self->numType == DAO_INTEGER ){
		for(i=0; i<self->size; i++) self->data.i[i] = buf[i];
	}else if( self->numType == DAO_FLOAT ){
		for(i=0; i<self->size; i++) self->data.f[i] = buf[i];
	}
}
float* DaoArray_ToFloat( DaoArray *self )
{
	int i;
	float *buf;
	if( self->numType == DAO_FLOAT ) return self->data.f;
	buf = self->data.f;
	if( self->numType == DAO_INTEGER ){
		for(i=0; i<self->size; i++) buf[i] = (float)self->data.i[i];
	}else if( self->numType == DAO_DOUBLE ){
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
	int i;
	float *buf;
	if( self->numType == DAO_FLOAT ) return;
	buf = self->data.f;
	if( self->numType == DAO_INTEGER ){
		for(i=0; i<self->size; i++) self->data.i[i] = buf[i];
	}else if( self->numType == DAO_DOUBLE ){
		for(i=self->size-1; i>=0; i--) self->data.d[i] = buf[i];
	}else{
		for(i=self->size-1; i>=0; i--){
			self->data.c[i].real = buf[2*i];
			self->data.c[i].imag = buf[2*i+1];
		}
	}
}
int* DaoArray_ToInt( DaoArray *self )
{
	int i;
	int *buf;
	if( self->numType == DAO_INTEGER ) return self->data.i;
	buf = self->data.i;
	switch( self->numType ){
	case DAO_FLOAT :
		for(i=0; i<self->size; i++) buf[i] = (int)self->data.f[i];
		break;
	case DAO_DOUBLE :
		for(i=0; i<self->size; i++) buf[i] = (int)self->data.d[i];
		break;
	case DAO_COMPLEX :
		for(i=0; i<self->size; i++){
			buf[2*i] = (int)self->data.c[i].real;
			buf[2*i+1] = (int)self->data.c[i].imag;
		}
		break;
	default : break;
	}
	return buf;
}
void DaoArray_FromInt( DaoArray *self )
{
	int i;
	int *buf;
	if( self->numType == DAO_INTEGER ) return;
	buf = self->data.i;
	switch( self->numType ){
	case DAO_FLOAT :
		for(i=0; i<self->size; i++) self->data.f[i] = buf[i];
		break;
	case DAO_DOUBLE :
		for(i=self->size-1; i>=0; i--) self->data.d[i] = buf[i];
		break;
	case DAO_COMPLEX :
		for(i=self->size-1; i>=0; i--){
			self->data.c[i].real = buf[2*i];
			self->data.c[i].imag = buf[2*i+1];
		}
		break;
	default : break;
	}
}
short* DaoArray_ToShort( DaoArray *self )
{
	int i;
	short *buf = (short*) self->data.p;
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<self->size; i++) buf[i] = (int)self->data.i[i];
		break;
	case DAO_FLOAT :
		for(i=0; i<self->size; i++) buf[i] = (int)self->data.f[i];
		break;
	case DAO_DOUBLE :
		for(i=0; i<self->size; i++) buf[i] = (int)self->data.d[i];
		break;
	case DAO_COMPLEX :
		for(i=0; i<self->size; i++){
			buf[2*i] = (int)self->data.c[i].real;
			buf[2*i+1] = (int)self->data.c[i].imag;
		}
		break;
	default : break;
	}
	return buf;
}
void DaoArray_FromShort( DaoArray *self )
{
	int i;
	short *buf = (short*) self->data.p;
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=self->size-1; i>=0; i--) self->data.i[i] = buf[i];
		break;
	case DAO_FLOAT :
		for(i=self->size-1; i>=0; i--) self->data.f[i] = buf[i];
		break;
	case DAO_DOUBLE :
		for(i=self->size-1; i>=0; i--) self->data.d[i] = buf[i];
		break;
	case DAO_COMPLEX :
		for(i=self->size-1; i>=0; i--){
			self->data.c[i].real = buf[2*i];
			self->data.c[i].imag = buf[2*i+1];
		}
		break;
	default : break;
	}
}
signed char* DaoArray_ToByte( DaoArray *self )
{
	int i;
	signed char *buf = (signed char*) self->data.p;
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<self->size; i++) buf[i] = (int)self->data.i[i];
		break;
	case DAO_FLOAT :
		for(i=0; i<self->size; i++) buf[i] = (int)self->data.f[i];
		break;
	case DAO_DOUBLE :
		for(i=0; i<self->size; i++) buf[i] = (int)self->data.d[i];
		break;
	case DAO_COMPLEX :
		for(i=0; i<self->size; i++){
			buf[2*i] = (int)self->data.c[i].real;
			buf[2*i+1] = (int)self->data.c[i].imag;
		}
		break;
	default : break;
	}
	return buf;
}
void DaoArray_FromByte( DaoArray *self )
{
	int i;
	signed char *buf = (signed char*) self->data.p;
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=self->size-1; i>=0; i--) self->data.i[i] = buf[i];
		break;
	case DAO_FLOAT :
		for(i=self->size-1; i>=0; i--) self->data.f[i] = buf[i];
		break;
	case DAO_DOUBLE :
		for(i=self->size-1; i>=0; i--) self->data.d[i] = buf[i];
		break;
	case DAO_COMPLEX :
		for(i=self->size-1; i>=0; i--){
			self->data.c[i].real = buf[2*i];
			self->data.c[i].imag = buf[2*i+1];
		}
		break;
	default : break;
	}
}
double** DaoArray_GetMatrixD( DaoArray *self, int row )
{
	int i, col;
	double *buf;
	if( row <= 0 ) row = self->dims->items.pInt[0];
	if( self->size % row != 0 ) return NULL;
	col = self->size / row;
	buf = DaoArray_ToDouble( self ); /* single to double */
	if( self->numType == DAO_COMPLEX ) col += col;
	self->matrix = (void**)dao_realloc( self->matrix, row * sizeof(void*) );
	for(i=0; i<row; i++) self->matrix[i] = buf + i * col;
	return (double**)self->matrix;
}
float** DaoArray_GetMatrixF( DaoArray *self, int row )
{
	int i, col;
	float *buf;
	if( row <= 0 ) row = self->dims->items.pInt[0];
	if( self->size % row != 0 ) return NULL;
	col = self->size / row;
	buf = DaoArray_ToFloat( self );
	self->matrix = (void**)dao_realloc( self->matrix, row * sizeof(void*) );
	for(i=0; i<row; i++) self->matrix[i] = buf + i * col;
	return (float**)self->matrix;
}
int** DaoArray_GetMatrixI( DaoArray *self, int row )
{
	int i, col;
	int *buf;
	if( row <= 0 ) row = self->dims->items.pInt[0];
	if( self->size % row != 0 ) return NULL;
	col = self->size / row;
	buf = DaoArray_ToInt( self );
	if( self->numType == DAO_COMPLEX ) col += col;
	self->matrix = (void**)dao_realloc( self->matrix, row * sizeof(void*) );
	for(i=0; i<row; i++) self->matrix[i] = buf + i * col;
	return (int**)self->matrix;
}
short** DaoArray_GetMatrixS( DaoArray *self, int row )
{
	int i, col;
	short *buf;
	if( row <= 0 ) row = self->dims->items.pInt[0];
	if( self->size % row != 0 ) return NULL;
	col = self->size / row;
	buf = DaoArray_ToShort( self );
	if( self->numType == DAO_COMPLEX ) col += col;
	self->matrix = (void**)dao_realloc( self->matrix, row * sizeof(void*) );
	for(i=0; i<row; i++) self->matrix[i] = buf + i * col;
	return (short**)self->matrix;
}
signed char** DaoArray_GetMatrixB( DaoArray *self, int row )
{
	int i, col;
	signed char *buf;
	if( row <= 0 ) row = self->dims->items.pInt[0];
	if( self->size % row != 0 ) return NULL;
	col = self->size / row;
	buf = DaoArray_ToByte( self );
	if( self->numType == DAO_COMPLEX ) col += col;
	self->matrix = (void**)dao_realloc( self->matrix, row * sizeof(void*) );
	for(i=0; i<row; i++) self->matrix[i] = buf + i * col;
	return (signed char**)self->matrix;
}
unsigned int* DaoArray_ToUInt( DaoArray *self )
{
	int i;
	unsigned int *buf = (unsigned int *) self->data.p;
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<self->size; i++) buf[i] = (unsigned int)self->data.i[i];
		break;
	case DAO_FLOAT :
		for(i=0; i<self->size; i++) buf[i] = (unsigned int)self->data.f[i];
		break;
	case DAO_DOUBLE :
		for(i=0; i<self->size; i++) buf[i] = (unsigned int)self->data.d[i];
		break;
	case DAO_COMPLEX :
		for(i=0; i<self->size; i++){
			buf[2*i] = (unsigned int)self->data.c[i].real;
			buf[2*i+1] = (unsigned int)self->data.c[i].imag;
		}
		break;
	default : break;
	}
	return buf;
}
void DaoArray_FromUInt( DaoArray *self )
{
	int i;
	unsigned int *buf = (unsigned int*) self->data.i;
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<self->size; i++) self->data.i[i] = buf[i];
		break;
	case DAO_FLOAT :
		for(i=0; i<self->size; i++) self->data.f[i] = buf[i];
		break;
	case DAO_DOUBLE :
		for(i=self->size-1; i>=0; i--) self->data.d[i] = buf[i];
		break;
	case DAO_COMPLEX :
		for(i=self->size-1; i>=0; i--){
			self->data.c[i].real = buf[2*i];
			self->data.c[i].imag = buf[2*i+1];
		}
		break;
	default : break;
	}
}
unsigned short* DaoArray_ToUShort( DaoArray *self )
{
	int i;
	unsigned short *buf = (unsigned short *) self->data.p;
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<self->size; i++) buf[i] = (unsigned short)self->data.i[i];
		break;
	case DAO_FLOAT :
		for(i=0; i<self->size; i++) buf[i] = (unsigned short)self->data.f[i];
		break;
	case DAO_DOUBLE :
		for(i=0; i<self->size; i++) buf[i] = (unsigned short)self->data.d[i];
		break;
		break;
	case DAO_COMPLEX :
		for(i=0; i<self->size; i++){
			buf[2*i] = (unsigned short)self->data.c[i].real;
			buf[2*i+1] = (unsigned short)self->data.c[i].imag;
		}
		break;
	default : break;
	}
	return buf;
}
void DaoArray_FromUShort( DaoArray *self )
{
	int i;
	unsigned short *buf = (unsigned short*) self->data.p;
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=self->size-1; i>=0; i--) self->data.i[i] = buf[i];
		break;
	case DAO_FLOAT :
		for(i=self->size-1; i>=0; i--) self->data.f[i] = buf[i];
		break;
	case DAO_DOUBLE :
		for(i=self->size-1; i>=0; i--) self->data.d[i] = buf[i];
		break;
	case DAO_COMPLEX :
		for(i=self->size-1; i>=0; i--){
			self->data.c[i].real = buf[2*i];
			self->data.c[i].imag = buf[2*i+1];
		}
		break;
	default : break;
	}
}
unsigned char* DaoArray_ToUByte( DaoArray *self )
{
	int i;
	unsigned char *buf = (unsigned char *) self->data.p;
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<self->size; i++) buf[i] = (unsigned char)self->data.i[i];
		break;
	case DAO_FLOAT :
		for(i=0; i<self->size; i++) buf[i] = (unsigned char)self->data.f[i];
		break;
	case DAO_DOUBLE :
		for(i=0; i<self->size; i++) buf[i] = (unsigned char)self->data.d[i];
		break;
	case DAO_COMPLEX :
		for(i=0; i<self->size; i++){
			buf[2*i] = (unsigned char)self->data.c[i].real;
			buf[2*i+1] = (unsigned char)self->data.c[i].imag;
		}
		break;
	default : break;
	}
	return buf;
}
void DaoArray_FromUByte( DaoArray *self )
{
	int i;
	unsigned char *buf = (unsigned char*) self->data.p;
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=self->size-1; i>=0; i--) self->data.i[i] = buf[i];
		break;
	case DAO_FLOAT :
		for(i=self->size-1; i>=0; i--) self->data.f[i] = buf[i];
		break;
	case DAO_DOUBLE :
		for(i=self->size-1; i>=0; i--) self->data.d[i] = buf[i];
		break;
	case DAO_COMPLEX :
		for(i=self->size-1; i>=0; i--){
			self->data.c[i].real = buf[2*i];
			self->data.c[i].imag = buf[2*i+1];
		}
		break;
	default : break;
	}
}

static void DaoArray_ResizeData( DaoArray *self, int size, int oldSize );

void DaoArray_SetVectorB( DaoArray *self, char *vec, int N )
{
	int i;
	if( N < self->size ) DaoArray_ResizeData( self, self->size, N );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<N; i++) self->data.i[i] = vec[i]; break;
	case DAO_FLOAT :
		for(i=0; i<N; i++) self->data.f[i] = vec[i]; break;
	case DAO_DOUBLE :
		for(i=0; i<N; i++) self->data.d[i] = vec[i]; break;
	case DAO_COMPLEX :
		for(i=0; i<N; i++){
			self->data.c[i].real = vec[i+i];
			self->data.c[i].imag = vec[i+i+1];
		}
		break;
	default : break;
	}
}
void DaoArray_SetVectorS( DaoArray *self, short *vec, int N )
{
	int i;
	if( N < self->size ) DaoArray_ResizeData( self, self->size, N );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<N; i++) self->data.i[i] = vec[i]; break;
	case DAO_FLOAT :
		for(i=0; i<N; i++) self->data.f[i] = vec[i]; break;
	case DAO_DOUBLE :
		for(i=0; i<N; i++) self->data.d[i] = vec[i]; break;
	case DAO_COMPLEX :
		for(i=0; i<N; i++){
			self->data.c[i].real = vec[i+i];
			self->data.c[i].imag = vec[i+i+1];
		}
		break;
	default : break;
	}
}
void DaoArray_SetVectorI( DaoArray *self, int *vec, int N )
{
	int i;
	if( N < self->size ) DaoArray_ResizeData( self, self->size, N );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<N; i++) self->data.i[i] = vec[i]; break;
	case DAO_FLOAT :
		for(i=0; i<N; i++) self->data.f[i] = vec[i]; break;
	case DAO_DOUBLE :
		for(i=0; i<N; i++) self->data.d[i] = vec[i]; break;
	case DAO_COMPLEX :
		for(i=0; i<N; i++){
			self->data.c[i].real = vec[i+i];
			self->data.c[i].imag = vec[i+i+1];
		}
		break;
	default : break;
	}
}
void DaoArray_SetVectorF( DaoArray *self, float *vec, int N )
{
	int i;
	if( N < self->size ) DaoArray_ResizeData( self, self->size, N );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<N; i++) self->data.i[i] = (int) vec[i]; break;
	case DAO_FLOAT :
		for(i=0; i<N; i++) self->data.f[i] = vec[i]; break;
	case DAO_DOUBLE :
		for(i=0; i<N; i++) self->data.d[i] = vec[i]; break;
	case DAO_COMPLEX :
		for(i=0; i<N; i++){
			self->data.c[i].real = vec[i+i];
			self->data.c[i].imag = vec[i+i+1];
		}
		break;
	default : break;
	}
}
void DaoArray_SetVectorD( DaoArray *self, double *vec, int N )
{
	int i;
	if( N < self->size ) DaoArray_ResizeData( self, self->size, N );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<N; i++) self->data.i[i] = (int) vec[i]; break;
	case DAO_FLOAT :
		for(i=0; i<N; i++) self->data.f[i] = vec[i]; break;
	case DAO_DOUBLE :
		for(i=0; i<N; i++) self->data.d[i] = vec[i]; break;
	case DAO_COMPLEX :
		for(i=0; i<N; i++){
			self->data.c[i].real = vec[i+i];
			self->data.c[i].imag = vec[i+i+1];
		}
		break;
	default : break;
	}
}
void DaoArray_SetMatrixB( DaoArray *self, signed char **mat, int row, int col )
{
	size_t dm[2];
	int i, j;
	dm[0] = row; dm[1] = col;
	if( row * col != self->size )
		DaoArray_ResizeData( self, self->size, row * col );
	DaoArray_Reshape( self, dm, 2 );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.i[i*col+j] = mat[i][j];
		}
		break;
	case DAO_FLOAT :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.f[i*col+j] = mat[i][j];
		}
		break;
	case DAO_DOUBLE :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.d[i*col+j] = mat[i][j];
		}
		break;
	case DAO_COMPLEX :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++){
				self->data.c[i*(col+col)+j+j].real = mat[i][j+j];
				self->data.c[i*(col+col)+j+j+1].imag = mat[i][j+j+1];
			}
		}
		break;
	default : break;
	}
}
void DaoArray_SetMatrixUB( DaoArray *self, unsigned char **mat, int N, int M )
{
	printf( "not implemented\n" );
}
void DaoArray_SetMatrixUS( DaoArray *self, unsigned short **mat, int N, int M )
{
	printf( "not implemented\n" );
}
void DaoArray_SetMatrixUI( DaoArray *self, unsigned int **mat, int N, int M )
{
	printf( "not implemented\n" );
}
void DaoArray_SetMatrixS( DaoArray *self, short **mat, int row, int col )
{
	size_t dm[2];
	int i, j;
	dm[0] = row; dm[1] = col;
	if( row * col != self->size )
		DaoArray_ResizeData( self, self->size, row * col );
	DaoArray_Reshape( self, dm, 2 );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.i[i*col+j] = mat[i][j];
		}
		break;
	case DAO_FLOAT :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.f[i*col+j] = mat[i][j];
		}
		break;
	case DAO_DOUBLE :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.d[i*col+j] = mat[i][j];
		}
		break;
	case DAO_COMPLEX :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++){
				self->data.c[i*(col+col)+j+j].real = mat[i][j+j];
				self->data.c[i*(col+col)+j+j+1].imag = mat[i][j+j+1];
			}
		}
		break;
	default : break;
	}
}
void DaoArray_SetMatrixI( DaoArray *self, int **mat, int row, int col )
{
	size_t dm[2];
	int i, j;
	dm[0] = row; dm[1] = col;
	if( row * col != self->size )
		DaoArray_ResizeData( self, self->size, row * col );
	DaoArray_Reshape( self, dm, 2 );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.i[i*col+j] = mat[i][j];
		}
		break;
	case DAO_FLOAT :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.f[i*col+j] = mat[i][j];
		}
		break;
	case DAO_DOUBLE :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.d[i*col+j] = mat[i][j];
		}
		break;
	case DAO_COMPLEX :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++){
				self->data.c[i*(col+col)+j+j].real = mat[i][j+j];
				self->data.c[i*(col+col)+j+j+1].imag = mat[i][j+j+1];
			}
		}
		break;
	default : break;
	}
}
void DaoArray_SetMatrixF( DaoArray *self, float **mat, int row, int col )
{
	size_t dm[2];
	int i, j;
	dm[0] = row; dm[1] = col;
	if( row * col != self->size )
		DaoArray_ResizeData( self, self->size, row * col );
	DaoArray_Reshape( self, dm, 2 );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.i[i*col+j] = (int) mat[i][j];
		}
		break;
	case DAO_FLOAT :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.f[i*col+j] = mat[i][j];
		}
		break;
	case DAO_DOUBLE :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.d[i*col+j] = mat[i][j];
		}
		break;
	case DAO_COMPLEX :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++){
				self->data.c[i*(col+col)+j+j].real = mat[i][j+j];
				self->data.c[i*(col+col)+j+j+1].imag = mat[i][j+j+1];
			}
		}
		break;
	default : break;
	}
}
void DaoArray_SetMatrixD( DaoArray *self, double **mat, int row, int col )
{
	size_t dm[2];
	int i, j;
	dm[0] = row; dm[1] = col;
	if( row * col != self->size )
		DaoArray_ResizeData( self, self->size, row * col );
	DaoArray_Reshape( self, dm, 2 );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.i[i*col+j] = (int) mat[i][j];
		}
		break;
	case DAO_FLOAT :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.f[i*col+j] = mat[i][j];
		}
		break;
	case DAO_DOUBLE :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++) self->data.d[i*col+j] = mat[i][j];
		}
		break;
	case DAO_COMPLEX :
		for(i=0; i<row; i++){
			for(j=0; j<col; j++){
				self->data.c[i*(col+col)+j+j].real = mat[i][j+j];
				self->data.c[i*(col+col)+j+j+1].imag = mat[i][j+j+1];
			}
		}
		break;
	default : break;
	}
}
void DaoArray_SetVectorUB( DaoArray *self, unsigned char* vec, int N )
{
	int i;
	if( N < self->size ) DaoArray_ResizeData( self, self->size, N );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<N; i++) self->data.i[i] = vec[i]; break;
	case DAO_FLOAT :
		for(i=0; i<N; i++) self->data.f[i] = vec[i]; break;
	case DAO_DOUBLE :
		for(i=0; i<N; i++) self->data.d[i] = vec[i]; break;
	case DAO_COMPLEX :
		for(i=0; i<N; i++){
			self->data.c[i].real = vec[i+i];
			self->data.c[i].imag = vec[i+i+1];
		}
		break;
	default : break;
	}
}
void DaoArray_SetVectorUS( DaoArray *self, unsigned short* vec, int N )
{
	int i;
	if( N < self->size ) DaoArray_ResizeData( self, self->size, N );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<N; i++) self->data.i[i] = vec[i]; break;
	case DAO_FLOAT :
		for(i=0; i<N; i++) self->data.f[i] = vec[i]; break;
	case DAO_DOUBLE :
		for(i=0; i<N; i++) self->data.d[i] = vec[i]; break;
	case DAO_COMPLEX :
		for(i=0; i<N; i++){
			self->data.c[i].real = vec[i+i];
			self->data.c[i].imag = vec[i+i+1];
		}
		break;
	default : break;
	}
}
void DaoArray_SetVectorUI( DaoArray *self, unsigned int* vec, int N )
{
	int i;
	if( N < self->size ) DaoArray_ResizeData( self, self->size, N );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<N; i++) self->data.i[i] = vec[i]; break;
	case DAO_FLOAT :
		for(i=0; i<N; i++) self->data.f[i] = vec[i]; break;
	case DAO_DOUBLE :
		for(i=0; i<N; i++) self->data.d[i] = vec[i]; break;
	case DAO_COMPLEX :
		for(i=0; i<N; i++){
			self->data.c[i].real = vec[i+i];
			self->data.c[i].imag = vec[i+i+1];
		}
		break;
	default : break;
	}
}
void* DaoArray_GetBuffer( DaoArray *self )
{
	return self->data.p;
}
void DaoArray_SetBuffer( DaoArray *self, void *buffer, size_t size )
{
	DaoArray_UseData( self, buffer );
	self->size = size;
}

DaoTypeBase numarTyper = 
{
	"array", & numarrCore, NULL, (DaoFuncItem*) numarMeths, {0},
	(FuncPtrDel) DaoArray_Delete, NULL
};

#ifdef DEBUG
static int array_count = 0;
#endif

DaoArray* DaoArray_New( int numType )
{
	DaoArray* self = (DaoArray*) dao_malloc( sizeof( DaoArray ) );
	self->data.p = NULL;
	DaoBase_Init( self, DAO_ARRAY );
	self->numType = numType;
	self->meta = NULL;
	self->unitype = NULL;
	self->size = 0;
	self->owner = 1;
	self->dims = DArray_New(0);
	self->dimAccum = DArray_New(0);
	self->data.p = NULL;
	DaoArray_ResizeVector( self, 0 );
	self->matrix = NULL;
	self->slice = NULL;
	self->reference = NULL;
#ifdef DEBUG
	array_count ++;
#endif
	return self;
}
void DaoArray_Delete( DaoArray *self )
{
	DArray_Delete( self->dims );
	DArray_Delete( self->dimAccum );
	if( self->meta ) GC_DecRC( self->meta );
	if( self->unitype ) GC_DecRC( self->unitype );
	if( self->owner && self->data.p ) dao_free( self->data.p );
	if( self->matrix ) dao_free( self->matrix );
	if( self->slice ) DArray_Delete( self->slice );
	if( self->reference ) GC_DecRC( self->reference );
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
int DaoArray_IndexFromSlice( DaoArray *self, DArray *slice, int sid )
/* sid: plain index in the sliced array */
{
	size_t *dimAccum = self->dimAccum->items.pSize;
	int j, index = 0; 
	for( j=(int)slice->size-1; j>=0; j-- ){
		DArray *sub = slice->items.pArray[j];
		size_t *ids = sub->items.pSize; /* { type, count, ... } */
		int count = ids[1];
		int res = sid % count;
		if( ids[0] == SLICE_RANGE ){
			index += (ids[2] + res) * dimAccum[j];
		}else{
			index += ids[ res + 2 ] * dimAccum[j];
		}
		sid /= count;
	}
	return index;
}
void DaoArray_GetSliceShape( DaoArray *self, DArray *shape )
{
	DArray *slice = self->slice;
	int i, k, S = 0;
	if( self->reference == NULL ){
		DArray_Assign( shape, self->dims );
		return;
	}
	DArray_Clear( shape );
	if( slice->size != self->reference->dims->size ) return;
	for(i=0; i<slice->size; i++){
		k = slice->items.pArray[i]->items.pSize[1];
		if( k ==0 ) return; /* skip empty dimension */
		S += k > 1;
	}
	for(i=0; i<slice->size; i++){
		k = slice->items.pArray[i]->items.pSize[1];
		/* skip size one dimension if the final slice has at least two dimensions */
		if( k == 1 && (S > 1 || shape->size > 1) ) continue;
		DArray_Append( shape, k );
	}
}
int DaoArray_SliceFrom( DaoArray *self, DaoArray *ref, DArray *slice )
{
	int i, S = 0;
	size_t k;
	if( slice == NULL ){
		DaoArray_ResizeArray( self, ref->dims->items.pSize, ref->dims->size );
		DaoArray_CopyArray( self, ref );
		return 1;
	}
	if( slice->size != ref->dims->size ) return 0;
	for(i=0; i<slice->size; i++){
		k = slice->items.pArray[i]->items.pSize[1];
		S += k > 1;
		if( k ==0 ){ /* skip empty dimension */
			DaoArray_ResizeVector( self, 0 );
			return 1;
		}
	}
	DArray_Clear( self->dimAccum );
	for(i=0; i<slice->size; i++){
		k = slice->items.pArray[i]->items.pSize[1];
		/* skip size one dimension if the final slice has at least two dimensions */
		if( k == 1 && (S > 1 || self->dimAccum->size > 1) ) continue;
		DArray_Append( self->dimAccum, k );
	}
	DaoArray_ResizeArray( self, self->dimAccum->items.pSize, self->dimAccum->size );

	for(i=0; i<self->size; i++){
		int j = DaoArray_IndexFromSlice( ref, slice, i );
		switch( self->numType | (ref->numType<<3) ){
		case DAO_INTEGER|(DAO_INTEGER<<3) : self->data.i[i] = ref->data.i[j]; break;
		case DAO_INTEGER|(DAO_FLOAT<<3)   : self->data.i[i] = ref->data.f[j]; break;
		case DAO_INTEGER|(DAO_DOUBLE<<3)  : self->data.i[i] = ref->data.d[j]; break;
		case DAO_INTEGER|(DAO_COMPLEX<<3) : self->data.i[i] = ref->data.c[j].real; break;
		case DAO_FLOAT|(DAO_INTEGER<<3)   : self->data.f[i] = ref->data.i[j]; break;
		case DAO_FLOAT|(DAO_FLOAT<<3)     : self->data.f[i] = ref->data.f[j]; break;
		case DAO_FLOAT|(DAO_DOUBLE<<3)    : self->data.f[i] = ref->data.d[j]; break;
		case DAO_FLOAT|(DAO_COMPLEX<<3)   : self->data.f[i] = ref->data.c[j].real; break;
		case DAO_DOUBLE|(DAO_INTEGER<<3)  : self->data.d[i] = ref->data.i[j]; break;
		case DAO_DOUBLE|(DAO_FLOAT<<3)    : self->data.d[i] = ref->data.f[j]; break;
		case DAO_DOUBLE|(DAO_DOUBLE<<3)   : self->data.d[i] = ref->data.d[j]; break;
		case DAO_DOUBLE|(DAO_COMPLEX<<3)  : self->data.d[i] = ref->data.c[j].real; break;
		case DAO_COMPLEX|(DAO_INTEGER<<3) : self->data.c[i].real = ref->data.i[j];
											self->data.c[i].imag = 0.0; break;
		case DAO_COMPLEX|(DAO_FLOAT<<3)   : self->data.c[i].real = ref->data.f[j];
											self->data.c[i].imag = 0.0; break;
		case DAO_COMPLEX|(DAO_DOUBLE<<3)  : self->data.c[i].real = ref->data.d[j];
											self->data.c[i].imag = 0.0; break;
		case DAO_COMPLEX|(DAO_COMPLEX<<3) : self->data.c[i] = ref->data.c[j]; break;
		default : break;
		}
	}
	return 1;
}
int DaoArray_Sliced( DaoArray *self )
{
	DaoArray *ref = self->reference;
	DArray *slice = self->slice;
	int i, S = 0;
	size_t k;

	if( slice == NULL || ref == NULL ) goto ReturnFalse;
	if( self->numType != ref->numType ) goto ReturnFalse;
	if( slice->size != ref->dims->size ) goto ReturnFalse;
	if( DaoArray_SliceFrom( self, ref, slice ) ==0 ) goto ReturnFalse;
ReturnTrue:
	GC_DecRC( self->reference );
	self->reference = NULL;
	return 1;
ReturnFalse:
	GC_DecRC( self->reference );
	self->reference = NULL;
	return 0;
}
DaoArray* DaoArray_Copy( DaoArray *self )
{
	DaoArray *copy = DaoArray_New( self->numType );
	copy->unitype = self->unitype;
	GC_IncRC( copy->unitype );
	DaoArray_ResizeArray( copy, self->dims->items.pSize, self->dims->size );
	switch( self->numType ){
	case DAO_INTEGER :
		memcpy( copy->data.p, self->data.p, self->size * sizeof(int) );
		break;
	case DAO_FLOAT :
		memcpy( copy->data.p, self->data.p, self->size * sizeof(float) );
		break;
	case DAO_DOUBLE :
		memcpy( copy->data.p, self->data.p, self->size * sizeof(double) );
		break;
	case DAO_COMPLEX :
		memcpy( copy->data.p, self->data.p, self->size * sizeof(complex16) );
		break;
	default : break;
	}
	return copy;
}
static void DaoArray_ResizeData( DaoArray *self, int size, int old )
{
	size_t diff = size - old;
	if( self->owner ==0 ){
		self->size = size;
		return;
	}
	switch( self->numType ){
	case DAO_INTEGER :
		self->data.i = dao_realloc( self->data.i, size*sizeof(int) );
		if( size > old ) memset( self->data.i + old, 0, diff * sizeof(int) );
		break;
	case DAO_FLOAT :
		self->data.f = dao_realloc( self->data.f, size*sizeof(float) );
		if( size > old ) memset( self->data.f + old, 0, diff * sizeof(float) );
		break;
	case DAO_DOUBLE :
		self->data.d = dao_realloc( self->data.d, size*sizeof(double) );
		if( size > old ) memset( self->data.d + old, 0, diff * sizeof(double) );
		break;
	case DAO_COMPLEX :
		self->data.c = dao_realloc( self->data.c, size*sizeof(complex16) );
		if( size > old ) memset( self->data.c + old, 0, diff * sizeof(complex16) );
		break;
	default : break;
	}
}
void DaoArray_ResizeVector( DaoArray *self, int size )
{
	int old = self->size;
	if( size < 0 ) return;
	DArray_Resize( self->dims, 2, (void*)1 );
	self->dims->items.pInt[1] = size;
	DaoArray_UpdateDimAccum( self );
	if( size == old ) return;
	DaoArray_ResizeData( self, size, old );
}
void DaoArray_ResizeArray( DaoArray *self, size_t *dims, int D )
{
	int i, k;
	int old = self->size;
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
	DArray_Resize( self->dims, k, 0 );
	k = 0;
	for(i=0; i<D; i++){
		if( dims[i] != 1 || D ==2 ){
			self->dims->items.pSize[k] = dims[i];
			k ++;
		}
	}
	/* self->dims->size will be one for dims such as [100,1,1] */
	if( self->dims->size ==1 ){
		if( dims[0] == 1 ){
			DArray_PushFront( self->dims, (void*)1 );
		}else{
			DArray_PushBack( self->dims, (void*)1 );
		}
	}
	DaoArray_UpdateDimAccum( self );
	if( self->size == old ) return;
	DaoArray_ResizeData( self, self->size, old );
}
int DaoArray_UpdateShape( DaoArray *C, DaoArray *A )
{
	int N = DaoArray_MatchShape( C, A );
	if( C->reference && N < 0 ) return -1;
	if( C != A && C->reference == NULL && N < 0 ){
		DArray *dims = C->dimAccum; /* safe to use */
		DaoArray_GetSliceShape( A, dims );
		DaoArray_ResizeArray( C, dims->items.pSize, dims->size );
		N = C->size;
	}
	return N;
}
void DaoArray_number_op_array( DaoArray *C, DValue A, DaoArray *B, short op, DaoContext *ctx )
{
	DaoArray *rB = B->reference;
	DaoArray *rC = C->reference;
	DaoArray *dB = rB ? rB : B;
	DaoArray *dC = rC ? rC : C;
	int bi, i, b, c, N = DaoArray_UpdateShape( C, B );
	double bf, af = DValue_GetDouble( A );
	complex16 bc, ac = {0.0, 0.0};

	ac.real = af;
	if( N < 0 ){
		DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "not matched shape" );
		return;
	}
	for(i=0; i<N; i++){
		c = rC ? DaoArray_IndexFromSlice( rC, C->slice, i ) : i;
		b = rB ? DaoArray_IndexFromSlice( rB, B->slice, i ) : i;
		switch( C->numType ){
		case DAO_INTEGER :
			bf = DaoArray_GetFloat( dB, b );
			switch( op ){
			case DVM_MOVE : dC->data.i[c] = bf; break;
			case DVM_ADD : dC->data.i[c] = af + bf; break;
			case DVM_SUB : dC->data.i[c] = af - bf; break;
			case DVM_MUL : dC->data.i[c] = af * bf; break;
			case DVM_DIV : dC->data.i[c] = af / bf; break;
			case DVM_MOD : dC->data.i[c] = (dint)af % (dint)bf; break;
			case DVM_POW : dC->data.i[c] = powf( af, bf );break;
			case DVM_AND : dC->data.i[c] = af && bf; break;
			case DVM_OR  : dC->data.i[c] = af || bf; break;
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
			case DVM_MOD : dC->data.f[c] = (dint)af % (dint)bf; break;
			case DVM_POW : dC->data.f[c] = powf( af, bf );break;
			case DVM_AND : dC->data.f[c] = af && bf; break;
			case DVM_OR  : dC->data.f[c] = af || bf; break;
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
			case DVM_MOD : dC->data.d[c] = (dint)af % (dint)bf; break;
			case DVM_POW : dC->data.d[c] = powf( af, bf );break;
			case DVM_AND : dC->data.d[c] = af && bf; break;
			case DVM_OR  : dC->data.d[c] = af || bf; break;
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
}
void DaoArray_array_op_number( DaoArray *C, DaoArray *A, DValue B, short op, DaoContext *ctx )
{
	DaoArray *rA = A->reference;
	DaoArray *rC = C->reference;
	DaoArray *dA = rA ? rA : A;
	DaoArray *dC = rC ? rC : C;
	int ai, i, a, c, N = DaoArray_UpdateShape( C, A );
	double af, bf = DValue_GetDouble( B );
	complex16 ac, bc = {0.0, 0.0};

	bc.real = bf;
	if( N < 0 ){
		DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "not matched shape" );
		return;
	}
	for(i=0; i<N; i++){
		c = rC ? DaoArray_IndexFromSlice( rC, C->slice, i ) : i;
		a = A == C ? c : (rA ? DaoArray_IndexFromSlice( rA, A->slice, i ) : i);
		switch( C->numType ){
		case DAO_INTEGER :
			af = DaoArray_GetFloat( dA, a );
			switch( op ){
			case DVM_MOVE : dC->data.i[c] = bf; break;
			case DVM_ADD : dC->data.i[c] = af + bf; break;
			case DVM_SUB : dC->data.i[c] = af - bf; break;
			case DVM_MUL : dC->data.i[c] = af * bf; break;
			case DVM_DIV : dC->data.i[c] = af / bf; break;
			case DVM_MOD : dC->data.i[c] = (dint)af % (dint)bf; break;
			case DVM_POW : dC->data.i[c] = powf( af, bf );break;
			case DVM_AND : dC->data.i[c] = af && bf; break;
			case DVM_OR  : dC->data.i[c] = af || bf; break;
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
			case DVM_MOD : dC->data.f[c] = (dint)af % (dint)bf; break;
			case DVM_POW : dC->data.f[c] = powf( af, bf );break;
			case DVM_AND : dC->data.f[c] = af && bf; break;
			case DVM_OR  : dC->data.f[c] = af || bf; break;
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
			case DVM_MOD : dC->data.d[c] = (dint)af % (dint)bf; break;
			case DVM_POW : dC->data.d[c] = powf( af, bf );break;
			case DVM_AND : dC->data.d[c] = af && bf; break;
			case DVM_OR  : dC->data.d[c] = af || bf; break;
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
}
void DaoArray_ArrayArith( DaoArray *C, DaoArray *A, DaoArray *B, short op, DaoContext *ctx )
{
	DaoArray *rA = A->reference;
	DaoArray *rB = B->reference;
	DaoArray *rC = C->reference;
	DaoArray *dA = rA ? rA : A;
	DaoArray *dB = rB ? rB : B;
	DaoArray *dC = rC ? rC : C;
	int N = DaoArray_MatchShape( A, B );
	int M = C == A ? N : DaoArray_MatchShape( C, A );
	int i, a, b, c;
	if( N < 0 || (C->reference && M != N) ){
		DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "not matched shape" );
		return;
	}
	if( A != C && C->reference == NULL && M != N ){
		DArray *dims = C->dimAccum; /* safe to use */
		DaoArray_GetSliceShape( A, dims );
		DaoArray_ResizeArray( C, dims->items.pSize, dims->size );
	}
	if( C->numType == A->numType && A->numType == B->numType ){
		for(i=0; i<N; i++){
			c = rC ? DaoArray_IndexFromSlice( rC, C->slice, i ) : i;
			b = rB ? DaoArray_IndexFromSlice( rB, B->slice, i ) : i;
			a = A == C ? c : (rA ? DaoArray_IndexFromSlice( rA, A->slice, i ) : i);
			switch( C->numType ){
			case DAO_INTEGER :
				switch( op ){
				case DVM_MOVE : dC->data.i[c] = dB->data.i[b]; break;
				case DVM_ADD : dC->data.i[c] = dA->data.i[a] + dB->data.i[b]; break;
				case DVM_SUB : dC->data.i[c] = dA->data.i[a] - dB->data.i[b]; break;
				case DVM_MUL : dC->data.i[c] = dA->data.i[a] * dB->data.i[b]; break;
				case DVM_DIV : dC->data.i[c] = dA->data.i[a] / dB->data.i[b]; break;
				case DVM_MOD : dC->data.i[c] = dA->data.i[a] % dB->data.i[b]; break;
				case DVM_POW : dC->data.i[c] = powf( dA->data.i[a], dB->data.i[b] );break;
				case DVM_AND : dC->data.i[c] = dA->data.i[a] && dB->data.i[b]; break;
				case DVM_OR  : dC->data.i[c] = dA->data.i[a] || dB->data.i[b]; break;
				default : break;
				}
				break;
			case DAO_FLOAT :
				switch( op ){
				case DVM_MOVE : dC->data.f[c] = dB->data.f[b]; break;
				case DVM_ADD : dC->data.f[c] = dA->data.f[a] + dB->data.f[b]; break;
				case DVM_SUB : dC->data.f[c] = dA->data.f[a] - dB->data.f[b]; break;
				case DVM_MUL : dC->data.f[c] = dA->data.f[a] * dB->data.f[b]; break;
				case DVM_DIV : dC->data.f[c] = dA->data.f[a] / dB->data.f[b]; break;
				case DVM_MOD : dC->data.f[c] = (dint)dA->data.f[a]%(dint)dB->data.f[b]; break;
				case DVM_POW : dC->data.f[c] = powf( dA->data.f[a], dB->data.f[b] );break;
				case DVM_AND : dC->data.f[c] = dA->data.f[a] && dB->data.f[b]; break;
				case DVM_OR  : dC->data.f[c] = dA->data.f[a] || dB->data.f[b]; break;
				default : break;
				}
				break;
			case DAO_DOUBLE :
				switch( op ){
				case DVM_MOVE : dC->data.d[c] = dB->data.d[b]; break;
				case DVM_ADD : dC->data.d[c] = dA->data.d[a] + dB->data.d[b]; break;
				case DVM_SUB : dC->data.d[c] = dA->data.d[a] - dB->data.d[b]; break;
				case DVM_MUL : dC->data.d[c] = dA->data.d[a] * dB->data.d[b]; break;
				case DVM_DIV : dC->data.d[c] = dA->data.d[a] / dB->data.d[b]; break;
				case DVM_MOD : dC->data.d[c] = (dint)dA->data.d[a]%(dint)dB->data.d[b]; break;
				case DVM_POW : dC->data.d[c] = powf( dA->data.d[a], dB->data.d[b] );break;
				case DVM_AND : dC->data.d[c] = dA->data.d[a] && dB->data.d[b]; break;
				case DVM_OR  : dC->data.d[c] = dA->data.d[a] || dB->data.d[b]; break;
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
		return;
	}
	for(i=0; i<N; i++){
		complex16 ac, bc;
		double ad, bd;
		float af, bf;
		int ai, bi;
		c = rC ? DaoArray_IndexFromSlice( rC, C->slice, i ) : i;
		b = rB ? DaoArray_IndexFromSlice( rB, B->slice, i ) : i;
		a = A == C ? c : (rA ? DaoArray_IndexFromSlice( rA, A->slice, i ) : i);
		switch( C->numType ){
		case DAO_INTEGER :
			af = DaoArray_GetFloat( dA, a );
			bf = DaoArray_GetFloat( dB, b );
			switch( op ){
			case DVM_MOVE : dC->data.i[c] = bf; break;
			case DVM_ADD : dC->data.i[c] = af + bf; break;
			case DVM_SUB : dC->data.i[c] = af - bf; break;
			case DVM_MUL : dC->data.i[c] = af * bf; break;
			case DVM_DIV : dC->data.i[c] = af / bf; break;
			case DVM_MOD : dC->data.i[c] = (dint)af % (dint)bf; break;
			case DVM_POW : dC->data.i[c] = powf( af, bf );break;
			case DVM_AND : dC->data.i[c] = af && bf; break;
			case DVM_OR  : dC->data.i[c] = af || bf; break;
			default : break;
			}
			break;
		case DAO_FLOAT :
			af = DaoArray_GetDouble( dA, a );
			bf = DaoArray_GetDouble( dB, b );
			switch( op ){
			case DVM_MOVE : dC->data.f[c] = bf; break;
			case DVM_ADD : dC->data.f[c] = af + bf; break;
			case DVM_SUB : dC->data.f[c] = af - bf; break;
			case DVM_MUL : dC->data.f[c] = af * bf; break;
			case DVM_DIV : dC->data.f[c] = af / bf; break;
			case DVM_MOD : dC->data.f[c] = (dint)af%(dint)bf; break;
			case DVM_POW : dC->data.f[c] = powf( af, bf );break;
			case DVM_AND : dC->data.f[c] = af && bf; break;
			case DVM_OR  : dC->data.f[c] = af || bf; break;
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
			case DVM_MOD : dC->data.d[c] = (dint)ad%(dint)bd; break;
			case DVM_POW : dC->data.d[c] = powf( ad, bd );break;
			case DVM_AND : dC->data.d[c] = ad && bd; break;
			case DVM_OR  : dC->data.d[c] = ad || bd; break;
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
}

void DaoContext_Apply( DaoContext *ctx, DaoVmCode *vmc, int index, int vdim, int entry, int last )
{
	DValue res, param = *ctx->regValues[ vmc->b ];
	DaoArray *self = param.v.array;
	DValue **idval = ctx->regValues + index + 2;
	const size_t *dims = self->dims->items.pSize;
	const int D = self->dims->size;
	int i, j, isvec = (D == 2 && (dims[0] ==1 || dims[1] == 1));
	for( i=0; i<self->size; i++ ){
		int id = i;
		if( isvec ){
			idval[0]->v.i = id;
			idval[1]->v.i = id;
		}else{
			for( j=D-1; j>=0; j--){
				int k = id % dims[j];
				id /= dims[j];
				if( j < vdim ) idval[j]->v.i = k;
			}
		}
		ctx->regValues[ index ]->v.i = i;
		DaoVmProcess_ExecuteSection( ctx->process, entry );
		res = *ctx->regValues[ last ];
		if( res.t ){
			if( res.t >= DAO_INTEGER && res.t <= DAO_DOUBLE ){
				double val2 = DValue_GetDouble( res );
				switch( self->numType ){
				case DAO_INTEGER : self->data.i[i] = val2; break;
				case DAO_FLOAT : self->data.f[i] = val2; break;
				case DAO_DOUBLE : self->data.d[i] = val2; break;
				case DAO_COMPLEX : self->data.c[i].real = val2;
								   self->data.c[i].imag = 0; break;
				default : break;
				}
			}else if( res.t == DAO_COMPLEX ){
				if( self->numType <= DAO_DOUBLE ){
					DaoContext_RaiseException( ctx, DAO_WARNING, "improper value to apply" );
					break;
				}
				self->data.c[i] = res.v.c[0]; break;
			}else{
				DaoContext_RaiseException( ctx, DAO_WARNING, "improper value to apply" );
				break;
			}
		}
	}
}
#endif
