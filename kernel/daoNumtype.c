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

#include"daoStream.h"
#include"daoContext.h"
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

static void DaoComplex_Print( DaoValue *self, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	complex16 p = self->xComplex.value;
	DaoStream_WriteFloat( stream, p.real );
	if( p.imag >= -0.0 ) DaoStream_WriteMBS( stream, "+" );
	DaoStream_WriteFloat( stream, p.imag );
	DaoStream_WriteMBS( stream, "$" );
}
static DaoValue* DaoComplex_Copy( DaoValue *self, DaoProcess *proc, DMap *cycData )
{
	return self;
}
static DaoTypeCore comCore =
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoValue_GetItem,
	DaoValue_SetItem,
	DaoComplex_Print,
	DaoComplex_Copy,
};
static void DaoComplex_Lib_Real( DaoProcess *proc, DaoValue *par[], int N )
{
	complex16 *self = & par[0]->xComplex.value;
	DaoProcess_PutDouble( proc, self->real );
	if( N == 2 ) self->real = par[1]->xDouble.value;
}
static void DaoComplex_Lib_Imag( DaoProcess *proc, DaoValue *par[], int N )
{
	complex16 *self = & par[0]->xComplex.value;
	DaoProcess_PutDouble( proc, self->imag );
	if( N == 2 ) self->imag = par[1]->xDouble.value;
}
static DaoFuncItem comMeths[] =
{
	{ DaoComplex_Lib_Real,     "real( self :complex, v=0.00 )=>double" },
	{ DaoComplex_Lib_Imag,     "imag( self :complex, v=0.00 )=>double" },

	{ NULL, NULL }
};

DaoTypeBase comTyper = 
{
	"complex", & comCore, NULL, (DaoFuncItem*) comMeths, {0}, {0}, NULL, NULL
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



#ifdef DAO_WITH_THREAD
DMutex  mutex_long_sharing;
#endif

DLong* DLong_New()
{
	DLong *self = dao_malloc( sizeof(DLong) );
	DLong_Init( self );
	return self;
}
void DLong_Init( DLong *self )
{
	memset( self, 0, sizeof(DLong) );
	self->sign = 1;
	self->base = 10;
}
void DLong_Delete( DLong *self )
{
	uint_t *pbuf;
	DLong_Detach( self );
	pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
	if( pbuf ) dao_free( pbuf );
	dao_free( self );
}
void DLong_Clear( DLong *self )
{
	uint_t *pbuf;
	DLong_Detach( self );
	pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
	if( pbuf ) dao_free( pbuf );
	self->data = NULL;
	self->offset = 0;
	self->size = self->bufSize = 0;
}
void DLong_Detach( DLong *self /* , int extrasize  TODO */ )
{
	size_t size;
	uint_t *pbuf2, *pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
	if( pbuf == NULL ){
		pbuf = (uint_t*) dao_malloc( sizeof(uint_t) );
		pbuf[0] = 1;
		self->data = (uchar_t*)(pbuf + 1);
	}
	if( pbuf[0] == 1 ) return;
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & mutex_long_sharing );
#endif
	if( pbuf[0] >1 ){
		pbuf[0] -= 1;
		size = self->size * sizeof(uchar_t);
		self->bufSize = self->size;
		pbuf2 = (uint_t*) dao_malloc( size + sizeof(uint_t) );
		pbuf2[0] = 1;
		memcpy( pbuf2+1, pbuf+1, size );
		self->data = (uchar_t*)(pbuf2 + 1);
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & mutex_long_sharing );
#endif
}
void DLong_Resize( DLong *self, size_t size )
{
	size_t i;
	uint_t *pbuf;
	DLong_Detach( self );
	pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
	if( size == self->size && self->bufSize >0 ) return;
	if( self->offset ){
		size_t min = size > self->size ? self->size : size;
		memmove( pbuf + 1, self->data, min * sizeof(uchar_t) );
		self->data = (uchar_t*)(pbuf + 1);
		self->offset = 0;
	}
	if( size > self->bufSize || size < self->bufSize /2 ){
		self->bufSize = size;
		pbuf = (uint_t*)dao_realloc( pbuf, self->bufSize*sizeof(uchar_t) + sizeof(int) );
		self->data = (uchar_t*)(pbuf + 1);
	}
	self->size = size;
}
void DLong_Reserve( DLong *self, size_t size )
{
	size_t i;
	uint_t *pbuf;
	DLong_Detach( self );
	if( size + self->offset <= self->bufSize ) return;
	pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
	pbuf = (uint_t*)dao_realloc( pbuf, (size + self->offset)*sizeof(uchar_t) + sizeof(int) );
	self->data = (uchar_t*)(pbuf + 1) + self->offset;
	self->bufSize = size + self->offset;
}
void DLong_PushBack( DLong *self, uchar_t it )
{
	uint_t *pbuf;
	DLong_Detach( self );
	pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
	if( self->size + self->offset + 1 > self->bufSize ){
		self->bufSize += self->bufSize/5 + 1;
		pbuf = (uint_t*)dao_realloc( pbuf, self->bufSize*sizeof(uchar_t) + sizeof(int) );
		self->data = (uchar_t*)(pbuf + 1) + self->offset;
	}
	self->data[ self->size ] = it;
	self->size ++;
}
void DLong_PushFront( DLong *self, uchar_t it )
{
	DLong_Detach( self );
	if( self->offset == 0 ){
		uint_t *pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
		uint_t offset = self->bufSize/5 + 1;
		self->offset = offset < 0xffff ? offset : 0xffff;
		self->bufSize += self->offset;
		pbuf = dao_realloc( pbuf, self->bufSize*sizeof(uchar_t) + sizeof(uint_t) );
		self->data = (uchar_t*)(pbuf + 1) + self->offset;
		memmove( self->data, pbuf + 1, self->size*sizeof(uchar_t) );
	}
	self->offset --;
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
	uchar_t *dx = x->data;
	uchar_t *dy = y->data;
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
		uchar_t d = (x>>(i*LONG_BITS)) & LONG_MASK;
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
	uint_t *zbuf = (uint_t*)(z->data ? z->data - z->offset - sizeof(uint_t) : NULL);
	uint_t *xbuf = (uint_t*)(x->data ? x->data - x->offset - sizeof(uint_t) : NULL);
	if( z == x || zbuf == xbuf ) return;
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & mutex_long_sharing );
#endif
	if( zbuf ){
		zbuf[0] -= 1;
		if( zbuf[0] ==0 ) dao_free( zbuf );
	}
	*z = *x;
	if( xbuf ) xbuf[0] += 1;
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & mutex_long_sharing );
#endif
}
static void DLong_Copy( DLong *z, DLong *x )
{
	size_t nx = x->size;
	size_t nz = z->size;
	DLong_Detach( z );
	if( nx+nx < nz || nz < nx ) DLong_Resize( z, nx );
	z->sign = x->sign;
	z->base = x->base;
	memmove( z->data, x->data, nx * sizeof(uchar_t) );
	z->size = nx;
	if( x->base != 2 ) DLong_Normalize2( z );
}

static void LongAdd2( DLong *z, DLong *x, DLong *y, int base )
{
	uchar_t *dx, *dy, *dz;
	size_t nx = x->size;
	size_t ny = y->size;
	int i, sum = 0;
	DLong_Detach( z );
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
	uchar_t *dx, *dy, *dz;
	size_t nx = x->size;
	size_t ny = y->size;
	int i, sum = 0;
	DLong_Detach( z );
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
	uchar_t *dx, *dy, *dz;
	int i, nx, ny, sub = 1;

	DLong_Normalize2( x );
	DLong_Normalize2( y );
	nx = x->size;
	ny = y->size;
	assert( DLong_UCompare( x, y ) >=0 );
	DLong_Detach( z );
	if( z->bufSize < nx ) DLong_Resize( z, nx );
	dx = x->data;
	dy = y->data;
	dz = z->data;
	for(i=0; i<ny; i++){
		/* sub = LONG_BASE + dx[i] - dy[i] - (1-sub); */
#if 0
		if( i >= nx ){
			printf( "error %li %li  %i\n", nx, ny, dy[i] );
			DLong_Print( x, NULL );
			DLong_Print( y, NULL );
		}
#endif
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
	DLong_Detach( z );
	LongSub3( z, x, y );
	return z->sign;
}
void DLong_Add( DLong *z, DLong *x, DLong *y )
{
	DLong_Detach( z );
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
	DLong_Detach( z );
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
static void DLong_MulAdd( DLong *z, DLong *x, uchar_t y, short m );
void DLong_UMulDigitX( DLong *z, DLong *x, uchar_t digit );
static void DLong_UMulSimple( DLong *z, DLong *x, DLong *y )
{
	int i, n = x->size + y->size;
	DLong_Detach( z );
	if( z == x || z == y ){
		DLong *z2 = DLong_New();
		DLong_UMulSimple( z2, x, y );
		DLong_Move( z, z2 );
		DLong_Delete( z2 );
		return;
	}
	if( z->bufSize < n ) DLong_Reserve( z, n );
	z->size = n;
	memset( z->data, 0, z->size * sizeof(uchar_t) );
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
	DLong_Detach( x0 );
	DLong_Detach( x1 );
	memmove( x0->data, x->data, m * sizeof(uchar_t) );
	memmove( x1->data, x->data + m, (size-m) * sizeof(uchar_t) );
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
	int n = x1->size;
	DLong_Detach( z );
	if( z->bufSize < n + m + 1 ) DLong_Reserve( z, n + m + 1 );
	memmove( z->data + m, x1->data, n * sizeof(uchar_t) );
	memset( z->data, 0, m * sizeof(uchar_t) );
	z->size = m + n;
	DLong_UAdd( z, z, x0 );
}
static void LongMulSum( DLong *z, DLong *z2, DLong *z1, DLong *z0, int m )
{
	int n = m + z1->size;
	DLong_Detach( z );
	DLong_Detach( z1 );
	if( z2 ) n = z2->size + m + m;
	if( z->bufSize <= n ) DLong_Reserve( z, n );
	memmove( z->data, z0->data, z0->size * sizeof(uchar_t) );
	if( z2 ) memmove( z->data + (m+m), z2->data, z2->size *sizeof(uchar_t) );
	if( z1->bufSize <= z1->size+m ) DLong_Reserve( z1, z1->size + m );
	memmove( z1->data+m, z1->data, z1->size*sizeof(uchar_t) );
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
	uchar_t *dz1 = z1->data;
	uchar_t *dz0 = z0->data;
	uchar_t *dz2 = z2->data;
	DLong_Detach( z1 );
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
	uchar_t *dx = x->data;
	uchar_t *dy = y->data;
	size_t nx = x->size;
	size_t ny = y->size;
	size_t m = 0;
	/*
	   printf( "nx = %i,  ny = %i,  dep = %i\n", nx, ny, dep );
	   DLong_Print( x, NULL );
	   DLong_Print( y, NULL );
	 */
	DLong_Detach( z );
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
	uchar_t *dx = x->data;
	uchar_t *dy = y->data;
	size_t nx = x->size;
	size_t ny = y->size;
	size_t max = nx > ny ? nx : ny;
	size_t i, nc = 1;
	long_t c = 0; 
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
	memset( z->data, 0, nc*sizeof(uchar_t) );
	for(i=0; i<nc; i++){
		c += (long_t)(cx[i].real / nc + 0.5);
		z->data[i] = c & LONG_MASK;
		c = c >> LONG_BITS;
	}
	while( nc && z->data[nc-1] ==0 ) nc --;
	DLong_Resize( z, nc );
	dao_free( cx );
}
void DLong_UMul( DLong *z, DLong *x, DLong *y )
{
	uchar_t *dx = x->data;
	uchar_t *dy = y->data;
	size_t nx = x->size;
	size_t ny = y->size;
	/*
	   printf( "nx = %i,  ny = %i,  dep = %i\n", nx, ny, dep );
	   DLong_Print( x, NULL );
	   DLong_Print( y, NULL );
	 */
	DLong_Detach( z );
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
void DLong_UMulDigitX( DLong *z, DLong *x, uchar_t digit )
{
	uchar_t *dz, *dx = x->data;
	size_t nx = x->size;
	int i, carray = 0;
	if( digit == 0 ){
		z->size = 0;
		return;
	}else if( digit == 1 ){
		DLong_Move( z, x );
		return;
	}
	DLong_Detach( z );
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
	DLong_Normalize2( z );
}
uchar_t DLong_UDivDigit( DLong *z, uchar_t digit )
{
	size_t nz = z->size;
	int i, carray = 0;
	DLong_Detach( z );
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
	uchar_t *pbase;
	size_t nx = x->size;
	size_t nz = z->size;
	size_t nr;
	long_t hr;
	int cmp, hx;
	uchar_t d;

	while(nx >0 && x->data[nx-1] ==0 ) nx--;
	while(nz >0 && z->data[nz-1] ==0 ) nz--;
	DLong_Detach( y );
	DLong_Copy( r, z );
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
		if( nr > nx ) hr |= ((long_t)r->data[nx]) << (LONG_BITS<<1);
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
		memmove( r->data, r2->data, r2->size *sizeof(uchar_t) );
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
	DLong_Copy( z, x );
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
	DLong_Copy( max, z );
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
	uchar_t *dz, *dx = x->data;
	long i, sum = 0, nx = x->size;
	DLong_Detach( z );
	dz = z->data;
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
void DLong_MulAdd( DLong *z, DLong *x, uchar_t y, short m )
{
	int i, carray = 0;
	DLong_Detach( z );
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
	DLong_Detach( z );
	z->sign = x->sign * sign;
	if( z->bufSize < x->size ) DLong_Resize( z, x->size );
	z->size = x->size;
	memset( z->data, 0, z->size * sizeof(uchar_t) );
	while( y ){
		DLong_MulAdd( z, x, y & LONG_MASK, m++ );
		y = y >> LONG_BITS;
	}
}
void DLong_BitAND( DLong *z, DLong *x, DLong *y )
{
	size_t i, min = x->size < y->size ? x->size : y->size;
	uchar_t *dx, *dy, *dz;
	DLong_Resize( z, min );
	dx = x->data; dy = y->data; dz = z->data;
	for(i=0; i<min; i++) dz[i] = dx[i] & dy[i];
}
void DLong_BitOR( DLong *z, DLong *x, DLong *y )
{
	size_t i, max = x->size > y->size ? x->size : y->size;
	uchar_t *dx, *dy, *dz;
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
	uchar_t *dx, *dy, *dz;
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
	uchar_t *dz;
	int i, k, nz = z->size;
	k = bits / LONG_BITS;
	DLong_Detach( z );
	dz = z->data;
	if( k && z->offset >= k ){
		z->offset -= k;
		z->data -= k;
		z->size += k;
		memset( z->data, 0, k * sizeof(uchar_t) );
	}else if( k ){
		DLong_Resize( z, nz + k + 1 );
		memmove( z->data + k, z->data, nz * sizeof(uchar_t) );
		memset( z->data, 0, k * sizeof(uchar_t) );
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
	uchar_t *dz;
	int i, k, nz = z->size;
	if( bits >= nz * LONG_BITS ){
		DLong_Clear( z );
		return;
	}
	DLong_Detach( z );
	dz = z->data;
	k = bits / LONG_BITS;
	if( k && (k + z->offset) < z->size/10 ){
		z->offset += k;
		z->data += k;
	}else if( k ){
		memmove( z->data, z->data + k, (nz-k) * sizeof(uchar_t) );
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
	DLong_Detach( self );
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
	ulong_t y;
	if( x < 0 ){
		x = -x;
		self->sign = -1;
	}
	DLong_Clear( self );
	y = x;
	while( y ){
		DLong_Append( self, y & LONG_MASK );
		y = y >> LONG_BITS;
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
		uchar_t digit = table[ (uchar_t)mbs[i] ];
		if( digit ==0 ) return mbs[i];
		self->data[j] = digit - 1;
		if( self->data[j] >= base ){
			self->data[j] = 0;
			self->size = j;
			return mbs[i];
		}
	}
	tmp = DLong_New();
	DLong_Copy( tmp, self );
	DLong_Convert( tmp, base, self, LONG_BASE );
	DLong_Delete( tmp );
	return 0;
}

const int base_bits[] = {0,0,1,0,2,0,0,0,3,0,0,0,0,0,0,0,4};
const int base_masks[] = {0,0,1,0,3,0,0,0,7,0,0,0,0,0,0,0,15};
static void DaoLong_GetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid )
{
	DLong *self = self0->xLong.value;
	dint id = DaoValue_GetInteger( pid );
	int w = base_bits[self->base];
	int n = self->size;
	int digit = 0;
	if( self->base == 0 ){
		if( id <0 || id >= n*LONG_BITS ){
			DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "out of range" );
			return;
		}
		digit = (self->data[id/LONG_BITS] & (1<<(id%LONG_BITS)))>>(id%LONG_BITS);
		DaoProcess_PutInteger( proc, digit );
		return;
	}
	if( w == 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "need power 2 radix" );
		return;
	}
	if( id <0 || (w && id*w >= n*LONG_BITS) || (w == 0 && id >= n) ){
		DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "out of range" );
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
				DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "out of range" );
		}else{
			int d = (self->data[m+1]<<LONG_BITS) | self->data[m];
			digit = (d>>(id*w - m*LONG_BITS)) & base_masks[self->base];
		}
	}else{
		digit = self->data[id];
	}
	DaoProcess_PutInteger( proc, digit );
}
static void DaoLong_SetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid, DaoValue *value )
{
	DLong *self = self0->xLong.value;
	dint id = DaoValue_GetInteger( pid );
	dint digit = DaoValue_GetInteger( value );
	int i, n = self->size;
	int w = base_bits[self->base];
	DLong_Detach( self );
	if( self->base == 2 ){
		if( pid->type == 0 ){
			uchar_t bits = digit ? LONG_BASE-1 : 0;
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
		DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "need power 2 radix" );
		return;
	}
	if( id <0 || (w && id*w >= n*LONG_BITS) || (w == 0 && id >= n) ){
		DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "out of range" );
		return;
	}else if( digit <0 || digit >= self->base ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "digit value out of range" );
		return;
	}
	if( pid->type == 0 ){
		uchar_t bits = digit;
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
					DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "out of range" );
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
static void DaoLong_GetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N )
{
	switch( N ){
	case 0 : DaoLong_GetItem1( self, proc, null ); break;
	case 1 : DaoLong_GetItem1( self, proc, ids[0] ); break;
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}
static void DaoLong_SetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	switch( N ){
	case 0 : DaoLong_SetItem1( self, proc, null, value ); break;
	case 1 : DaoLong_SetItem1( self, proc, ids[0], value ); break;
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}
static DaoTypeCore longCore=
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoLong_GetItem,
	DaoLong_SetItem,
	DaoValue_Print,
	DaoValue_NoCopy,
};
static void DaoLong_Size( DaoProcess *proc, DaoValue *p[], int N )
{
	DLong *self = p[0]->xLong.value;
	size_t size = self->size;
	if( self->base ==2 ){
		DaoProcess_PutInteger( proc, size*LONG_BITS );
		return;
	}
	while( size && self->data[size-1] ==0 ) size --;
	assert( self->size == size );
	DaoProcess_PutInteger( proc, size );
}
static void DaoLong_Sqrt( DaoProcess *proc, DaoValue *p[], int N )
{
	DLong *z = p[0]->xLong.value;
	DaoTuple *tuple = DaoProcess_PutTuple( proc );
	DaoValue **items = tuple->items;
	if( z->sign <0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "need positive long integer" );
		return;
	}
	DLong_Sqrt( z, items[0]->xLong.value, items[1]->xLong.value );
}
static DaoFuncItem longMeths[] =
{
	{ DaoLong_Size, "size( self : long ) => int" } ,
	{ DaoLong_Sqrt, "sqrt( self : long ) => tuple<long,long>" } ,
	{ NULL, NULL }
};
DaoTypeBase longTyper =
{
	"long", & longCore, NULL, (DaoFuncItem*) longMeths, {0}, {0}, NULL, NULL
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
	int i, j, k, m, N = self->size, D = perm->size;
	size_t *dm, *ac, *mdo, *mdn, *pm = perm->items.pSize;
	DArray *dim = DArray_New(0); /* new dimension vector */
	DArray *acc = DArray_New(0); /* new accumulate vector */
	DArray *mido = DArray_New(0); /* old multiple indices */
	DArray *midn = DArray_New(0); /* new multiple indices */
	dint ival = 0;
	float fval = 0;
	double dval = 0;
	complex16  c16val = {0,0};

	if( D != self->ndim ) return 0;
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
		dm[i] = self->dims[ pm[i] ];
	}
	Array_SetAccum( dm, ac, D );
	for(i=0; i<N; i++){
		int min = i;
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
	self->dims = (size_t*) dao_realloc( self->dims, 2*D*sizeof(size_t) );
}
void DaoArray_FinalizeDimData( DaoArray *self )
{
	int i, D = self->ndim;
	size_t *prods = self->dims + D;

	prods[ D - 1 ] = 1;
	for(i=D-2; i>=0; i--) prods[i] = prods[i+1] * self->dims[i+1];
	self->size = (int)( self->dims[0] * prods[0] );
}

static void SliceRange( DArray *slice, int first, int count )
{
	if( count <0 ) count = 0;
	DArray_Resize( slice, 3, 0 );
	slice->items.pSize[0] = SLICE_RANGE;
	slice->items.pSize[1] = count;
	slice->items.pSize[2] = first;
}
static void MakeSlice( DaoProcess *proc, DaoValue *pid, int N, DArray *slice )
{
	int j, id, from, to;
	if( pid == NULL || pid->type == 0 ){
		SliceRange( slice, 0, N );
		return;
	}
	switch( pid->type ){
	case DAO_INTEGER :
	case DAO_FLOAT :
	case DAO_DOUBLE :
		{
			id = DaoValue_GetInteger( pid );
			if( id < 0 ) id += N;
			SliceRange( slice, id, 1 );
			break;
		}
	case DAO_TUPLE :
		{
			DaoValue **data = pid->xTuple.items;
			DArray_Clear( slice );
			if( data[0]->type == DAO_INTEGER && data[1]->type == DAO_INTEGER ){
				if( pid->xTuple.unitype == dao_type_for_iterator ){
					SliceRange( slice, data[1]->xInteger.value, 1 );
					data[1]->xInteger.value += 1;
					data[0]->xInteger.value = data[1]->xInteger.value < N;
				}else{
					from = data[0]->xInteger.value;
					to   = data[1]->xInteger.value;
					SliceRange( slice, from, to-from+1 );
				}
			}else if( data[0]->type == DAO_NULL && data[1]->type == DAO_NULL ){
				SliceRange( slice, 0, N );
			}else if( data[0]->type <= DAO_DOUBLE && data[1]->type == DAO_NULL ){
				from = DaoValue_GetInteger( data[0] );
				if( from <0 ) from += N;
				if( from >= N ) from -= N;
				SliceRange( slice, from, N - from );
			}else if( data[0]->type == DAO_NULL && data[1]->type <= DAO_DOUBLE ){
				to = DaoValue_GetInteger( data[1] );
				if( to <0 ) to += N;
				SliceRange( slice, 0, to + 1 );
			}else{
				DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "need number" );
			}
			break;
		}
	case DAO_LIST :
		{
			DaoList *list = & pid->xList;
			DaoValue **v = list->items.items.pValue;
			DArray_Resize( slice, list->items.size + 2, 0 );
			slice->items.pSize[0] = SLICE_ENUM;
			slice->items.pSize[1] = list->items.size;
			for( j=0; j<list->items.size; j++){
				if( v[j]->type < DAO_INTEGER || v[j]->type > DAO_DOUBLE )
					DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "need number" );
				slice->items.pSize[j+2] = DaoValue_GetInteger( v[j] );
			}
			break;
		}
	case DAO_ARRAY :
		{
			DaoArray *na = & pid->xArray;
			size_t *p;

			if( na->etype == DAO_COMPLEX ){
				DaoProcess_RaiseException( proc, DAO_ERROR_INDEX,
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
	if( slice->size < 2 ) SliceRange( slice, 0, N );
}
static int DaoArray_MakeSlice( DaoArray *self, DaoProcess *proc, DaoValue *idx[], int N, DArray *slices )
{
	DArray *tmp = DArray_New(0);
	size_t *dims = self->dims;
	int i, D = self->ndim;
	int S = D != 0;
	/* slices: DArray<DArray<int> > */
	DArray_Clear( slices );
	DArray_Resize( tmp, 3, 0 );
	tmp->items.pSize[0] = SLICE_RANGE;
	tmp->items.pSize[2] = 0;
	for(i=0; i<D; i ++){
		tmp->items.pSize[1] = dims[i];
		DArray_Append( slices, tmp );
	}
	DArray_Delete( tmp );
	if( N == 1 ){
		if( D ==2 && ( dims[0] ==1 || dims[1] ==1 ) ){
			/* For vectors: */
			int k = dims[0] == 1;
			MakeSlice( proc, idx[0], (int)dims[k], slices->items.pArray[k] );
		}else{
			MakeSlice( proc, idx[0], (int)dims[0], slices->items.pArray[0] );
		}
	}else{
		const int n = N > D ? D : N;
		for( i=0; i<n; i++ ) MakeSlice( proc, idx[i], (int)dims[i], slices->items.pArray[i] );
	}
	for(i=0; i<D; i ++) S *= slices->items.pArray[i]->items.pSize[1];
	return S;
}
int DaoArray_AlignShape( DaoArray *self, DArray *sidx, size_t *dims, int ndim )
{
	int i;
	size_t *dself = self->dims;

	if( self->ndim != ndim ) return 0;
	if( sidx ){
		for(i=0; i<ndim; i++) if( sidx->items.pArray[i]->size != dims[i] ) return 0;
	}else{
		for(i=0; i<ndim; i++) if( dself[i] != dims[i] ) return 0;
	}
	return 1;
}
static int DaoArray_MatchShape( DaoArray *self, DaoArray *other )
{
	DaoArray *sRef = self->original;
	DaoArray *oRef = other->original;
	int i, m = 0;
	if( sRef && oRef ){
		if( self->slices->size != other->slices->size ) return -1;
		m = self->slices->size != 0;
		for(i=0; i<self->slices->size; i++){
			int n1 = self->slices->items.pArray[i]->items.pSize[1];
			int n2 = other->slices->items.pArray[i]->items.pSize[1];
			if( n1 != n2 ) return -1;
			m *= n1;
		}
	}else if( sRef ){
		if( self->slices->size != other->ndim ) return -1;
		m = self->slices->size != 0;
		for(i=0; i<self->slices->size; i++){
			int n1 = self->slices->items.pArray[i]->items.pSize[1];
			int n2 = other->dims[i];
			if( n1 != n2 ) return -1;
			m *= n1;
		}
	}else if( oRef ){
		if( self->ndim != other->slices->size ) return -1;
		m = self->ndim != 0;
		for(i=0; i<self->ndim; i++){
			int n1 = self->dims[i];
			int n2 = other->slices->items.pArray[i]->items.pSize[1];
			if( n1 != n2 ) return -1;
			m *= n1;
		}
	}else{
		if( self->ndim != other->ndim ) return -1;
		m = self->ndim != 0;
		for(i=0; i<self->ndim; i++){
			int n1 = self->dims[i];
			int n2 = other->dims[i];
			if( n1 != n2 ) return -1;
			m *= n1;
		}
	}
	return m;
}
int DaoArray_SliceSize( DaoArray *self )
{
	int i, m;
	if( self->original == NULL || self->slices == NULL ) return self->size;
	return self->count;
	m = self->slices->size != 0;
	for(i=0; i<self->slices->size; i++){
		m *= self->slices->items.pArray[i]->items.pSize[1];
	}
	return m;
}
dint DaoArray_GetInteger( DaoArray *na, int i )
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
float DaoArray_GetFloat( DaoArray *na, int i )
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
double DaoArray_GetDouble( DaoArray *na, int i )
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
complex16 DaoArray_GetComplex( DaoArray *na, int i )
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

int DaoArray_IndexFromSlice( DaoArray *self, DArray *slices, int sid );
void DaoArray_number_op_array( DaoArray *C, DaoValue *A, DaoArray *B, short op, DaoProcess *proc );
void DaoArray_array_op_number( DaoArray *C, DaoArray *A, DaoValue *B, short op, DaoProcess *proc );
void DaoArray_ArrayArith( DaoArray *s, DaoArray *l, DaoArray *r, short p, DaoProcess *c );
static void DaoArray_Print( DaoValue *value, DaoProcess *proc, DaoStream *stream, DMap *cycData );

static void DaoArray_GetItem1( DaoValue *value, DaoProcess *proc, DaoValue *pid )
{
	DaoArray *na, *self = & value->xArray;
	/* if( self->unitype ) printf( "DaoArray_GetItem: %s\n", self->unitype->name->mbs ); */

	if( pid->type >= DAO_INTEGER && pid->type <= DAO_DOUBLE ){
		dint id = DaoValue_GetInteger( pid );
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
	}else if( pid->type == DAO_TUPLE && pid->xTuple.unitype == dao_type_for_iterator ){
		DaoValue **data = pid->xTuple.items;
		dint id = data[1]->xInteger.value;
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
	GC_ShiftRC( self->unitype, na->unitype );
	GC_ShiftRC( self, na->original );
	na->etype = self->etype;
	na->unitype = self->unitype;
	na->original = self;
	if( na->slices == NULL ) na->slices = DArray_New(D_ARRAY);
	na->count = DaoArray_MakeSlice( self, proc, & pid, 1, na->slices );
}
static void DaoArray_SetOneItem( DaoArray *self, dint id, DaoValue *value, int op )
{
	dint ival;
	double dval;
	complex16 c16;
	complex16 cval;

	switch( self->etype ){
	case DAO_INTEGER :
		if( value->type == DAO_INTEGER ){
			ival = value->xInteger.value;
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
		}
		dval = DaoValue_GetDouble( value );
		ival = (dint) dval;
		switch( op ){
		case DVM_ADD : self->data.i[ id ] += dval; break;
		case DVM_SUB : self->data.i[ id ] -= dval; break;
		case DVM_MUL : self->data.i[ id ] *= dval; break;
		case DVM_DIV : self->data.i[ id ] /= dval; break;
		case DVM_MOD : self->data.i[ id ] %= ival; break;
		case DVM_AND : self->data.i[ id ] &= ival; break;
		case DVM_OR  : self->data.i[ id ] |= ival; break;
		default : self->data.i[ id ] = ival; break;
		}
		break;
	case DAO_FLOAT :
		dval = DaoValue_GetDouble( value );
		switch( op ){
		case DVM_ADD : self->data.f[ id ] += dval; break;
		case DVM_SUB : self->data.f[ id ] -= dval; break;
		case DVM_MUL : self->data.f[ id ] *= dval; break;
		case DVM_DIV : self->data.f[ id ] /= dval; break;
		default : self->data.f[ id ] = dval; break;
		}
		break;
	case DAO_DOUBLE :
		dval = DaoValue_GetDouble( value );
		switch( op ){
		case DVM_ADD : self->data.d[ id ] += dval; break;
		case DVM_SUB : self->data.d[ id ] -= dval; break;
		case DVM_MUL : self->data.d[ id ] *= dval; break;
		case DVM_DIV : self->data.d[ id ] /= dval; break;
		default : self->data.d[ id ] = dval; break;
		}
		break;
	case DAO_COMPLEX :
		cval = DaoValue_GetComplex( value );
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
int DaoArray_CopyArray( DaoArray *self, DaoArray *other )
{
	int i, N = self->size;
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
int DaoArray_Compare( DaoArray *x, DaoArray *y )
{
	dint *xi = x->data.i, *yi = y->data.i;
	float *xf = x->data.f, *yf = y->data.f;
	double *xd = x->data.d, *yd = y->data.d;
	complex16 *xc = x->data.c, *yc = y->data.c;
	int min = x->size < y->size ? x->size : y->size;
	int i = 0;
	if( x->etype == DAO_INTEGER && y->etype == DAO_INTEGER ){
		while( i < min && *xi == *yi ) i++, xi++, yi++;
		if( i < min ) return *xi < *yi ? -1 : 1;
	}else if( x->etype == DAO_FLOAT && y->etype == DAO_FLOAT ){
		while( i < min && *xf == *yf ) i++, xf++, yf++;
		if( i < min ) return *xf < *yf ? -1 : 1;
	}else if( x->etype == DAO_DOUBLE && y->etype == DAO_DOUBLE ){
		while( i < min && *xd == *yd ) i++, xd++, yd++;
		if( i < min ) return *xd < *yd ? -1 : 1;
	}else if( x->etype == DAO_COMPLEX && y->etype == DAO_COMPLEX ){
		while( i < min && xc->real == yc->real && xc->imag == yc->imag ) i++, xc++, yc++;
		if( i < min ){
			if( xc->real == yc->real && xc->imag == yc->imag ) return 0;
			if( xc->real == yc->real ) return xc->imag < yc->imag ? -1 : 1;
			if( xc->imag == yc->imag ) return xc->real < yc->real ? -1 : 1;
		}
	}else if( x->etype == DAO_COMPLEX ){
		while( i < min && xc->real == DaoArray_GetDouble( y, i ) && xc->imag ==0 ) i++, xc++;
		if( i < min ){
			double v = DaoArray_GetDouble( y, i );
			if( xc->real == v && xc->imag == 0 ) return 0;
			if( xc->real == v ) return xc->imag < 0 ? -1 : 1;
			if( xc->imag == 0 ) return xc->real < v ? -1 : 1;
		}
	}else if( y->etype == DAO_COMPLEX ){
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
void DaoArray_SetItem1( DaoValue *va, DaoProcess *proc, DaoValue *pid, DaoValue *value, int op )
{
	DaoArray *self = & va->xArray;

	DaoArray_Sliced( self );
	if( value->type ==0 ) return;
	if( pid == NULL || pid->type == 0 ){
		if( value->type >= DAO_INTEGER && value->type <= DAO_COMPLEX ){
			DaoArray_array_op_number( self, self, value, op, proc );
		}else if( value->type == DAO_ARRAY ){
			DaoArray_ArrayArith( self, self, & value->xArray, op, proc );
		}else{
			DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "" );
		}
		return;
	}else if( pid->type >= DAO_INTEGER && pid->type <= DAO_DOUBLE && value->type <= DAO_LONG ){
		dint id = DaoValue_GetInteger( pid );
		if( id < 0 ) id += self->size;
		if( id < 0 || id >= self->size ){
			DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, "" );
			return;
		}
		DaoArray_SetOneItem( self, id, value, op );
		return;
	}
	if( self->slices == NULL ) self->slices = DArray_New(D_ARRAY);
	self->count = DaoArray_MakeSlice( self, proc, & pid, 1, self->slices );
	self->original = self;
	if( value->type == DAO_ARRAY ){
		DaoArray *na = & value->xArray;
		DaoArray_ArrayArith( self, self, na, op, proc );
	}else{
		DaoArray_array_op_number( self, self, value, op, proc );
	}
	self->original = NULL;
}
static void DaoArray_GetItem( DaoValue *vself, DaoProcess *proc, DaoValue *ids[], int N )
{
	DaoArray *na, *self = & vself->xArray;
	int i;
	DaoArray_Sliced( self );
	if( N == 0 ){
		vself = (DaoValue*) DaoArray_Copy( self );
		DaoProcess_PutValue( proc, vself );
		return;
	}else if( N == 1 ){
		DaoArray_GetItem1( vself, proc, ids[0] );
		return;
	}else if( N == self->ndim ){
		int allNumbers = 1;
		int k, idFlat = 0;
		size_t *dims = self->dims;
		size_t *dimAccum = self->dims + self->ndim;
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
	GC_ShiftRC( self->unitype, na->unitype );
	GC_ShiftRC( self, na->original );
	na->etype = self->etype;
	na->unitype = self->unitype;
	na->original = self;
	if( na->slices == NULL ) na->slices = DArray_New(D_ARRAY);
	na->count = DaoArray_MakeSlice( self, proc, ids, N, na->slices );
}
static void DaoArray_SetItem( DaoValue *vself, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	DaoArray *self = & vself->xArray;
	DaoArray_Sliced( self );
	if( N == 0 ){
		DaoArray_SetItem1( vself, proc, null, value, DVM_MOVE );
		return;
	}else if( N == 1 ){
		DaoArray_SetItem1( vself, proc, ids[0], value, DVM_MOVE );
		return;
	}else if( N == self->ndim ){
		size_t *dims = self->dims;
		size_t *dimAccum = self->dims + self->ndim;
		int i, allNumbers = 1;
		dint k, idFlat = 0;
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
			DaoArray_SetOneItem( self, idFlat, value, DVM_MOVE );
			return;
		}
	}
	if( self->slices == NULL ) self->slices = DArray_New(D_ARRAY);
	self->count = DaoArray_MakeSlice( self, proc, ids, N, self->slices );
	self->original = self;
	if( value->type == DAO_ARRAY ){
		DaoArray *na = & value->xArray;
		DaoArray_ArrayArith( self, self, na, DVM_MOVE, proc );
	}else{
		DaoArray_array_op_number( self, self, value, DVM_MOVE, proc );
	}
	self->original = NULL;
}
static void DaoArray_PrintElement( DaoArray *self, DaoStream *stream, int i )
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
	size_t *tmp, *dims = self->dims;
	int i, j;

	if( self->ndim < 2 ) return;
	if( self->ndim ==2 && ( dims[0] ==1 || dims[1] ==1 ) ){
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
		DArray_Resize( tmpArray, self->ndim, 0 );
		tmp = tmpArray->items.pSize;
		for(i=0; i<self->size; i++){
			int mod = i;
			for(j=self->ndim-1; j >=0; j--){
				int res = ( mod % dims[j] );
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
static DaoValue* DaoNA_Copy( DaoValue *value, DaoProcess *proc, DMap *cycData )
{
	return (DaoValue*) DaoArray_Copy( & value->xArray );
}

static DaoTypeCore numarrCore =
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoArray_GetItem,
	DaoArray_SetItem,
	DaoArray_Print,
	DaoNA_Copy,
};
static void DaoARRAY_Dim( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoArray *self = & par[0]->xArray;
	dint i, *v;

	if( N == 1 ){
		DaoArray *na = DaoProcess_PutArray( proc );
		na->etype = DAO_INTEGER;
		if( self->original ){
			DaoArray_ResizeVector( na, self->slices->size );
			v = na->data.i;
			for(i=0; i<self->slices->size; i++ )
				v[i] = self->slices->items.pArray[i]->items.pSize[1];
		}else{
			DaoArray_ResizeVector( na, self->ndim );
			v = na->data.i;
			for(i=0; i<self->ndim; i++) v[i] = self->dims[i];
		}
	}else if( self->original ){
		dint *num = DaoProcess_PutInteger( proc, 0 );
		i = par[1]->xInteger.value;
		if( i <0 || i >= self->slices->size ){
			*num = -1;
			DaoProcess_RaiseException( proc, DAO_WARNING, "no such dimension" );
		}else{
			*num = self->slices->items.pArray[i]->items.pSize[1];
		}
	}else{
		dint *num = DaoProcess_PutInteger( proc, 0 );
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
	DArray *ad;
	size_t *dims;
	int i, size = 1;

	DaoArray_Sliced( self );
	DaoArray_Sliced( nad );

	if( nad->etype == DAO_COMPLEX ){
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "invalid dimension" );
		return;
	}
	ad = DArray_New(0);
	DArray_Resize( ad, nad->size, 0 );
	dims = ad->items.pSize;

	for(i=0; i<nad->size; i++){
		dims[i] = DaoArray_GetInteger( nad, i );
		size *= dims[i];
	}
	DaoProcess_PutValue( proc, (DaoValue*)self );
	if( (proc->vmSpace->options & DAO_EXEC_SAFE) && size > 1000 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		DArray_Delete( ad );
		return;
	}
	DaoArray_ResizeArray( self, dims, ad->size );
	DArray_Delete( ad );
}
static void DaoARRAY_Reshape( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoArray *self = & par[0]->xArray;
	DaoArray *nad = & par[1]->xArray;
	DArray *ad;
	int i, size;
	size_t *dims;

	DaoArray_Sliced( self );
	DaoArray_Sliced( nad );

	if( nad->etype == DAO_COMPLEX ){
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "invalid dimension" );
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
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "invalid dimension" );
		return;
	}
	DaoArray_SetDimCount( self, ad->size );
	memcpy( self->dims, dims, ad->size * sizeof(size_t) );
	DaoArray_FinalizeDimData( self );
	DaoProcess_PutValue( proc, (DaoValue*)self );
	DArray_Delete( ad );
}
static void DaoARRAY_Index( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoArray *self = & par[0]->xArray;
	DaoArray *na = DaoProcess_PutArray( proc );
	size_t *dim = self->dims;
	int i, D = self->ndim;
	dint sd = par[1]->xInteger.value;
	dint *v;

	DaoArray_Sliced( self );
	dim = self->dims;
	D = self->ndim;

	na->etype = DAO_INTEGER;
	DaoArray_ResizeVector( na, self->ndim );
	v = na->data.i;
	for(i=D-1; i>=0; i--){
		v[i] = sd % dim[i];
		sd = sd / dim[i];
	}
}
static void DaoARRAY_max( DaoProcess *proc, DaoValue *par[], int N )
{
	DaoTuple *tuple = DaoProcess_PutTuple( proc );
	DaoArray *self = & par[0]->xArray;
	int i, k, size, cmp=0, imax = -1;

	if( self->etype == DAO_COMPLEX ) return;/* no exception, guaranteed by the typing system */
	if( DaoArray_SliceSize( self ) == 0 ) return;
	if( self->original && self->slices ){
		DArray *slices = self->slices;
		size = self->count;
		self = self->original;;
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
	DaoTuple *tuple = DaoProcess_PutTuple( proc );
	DaoArray *self = & par[0]->xArray;
	int i, k, size, cmp=0, imax = -1;

	if( self->etype == DAO_COMPLEX ) return;
	if( DaoArray_SliceSize( self ) == 0 ) return;
	if( self->original && self->slices ){
		DArray *slices = self->slices;
		size = self->count;
		self = self->original;;
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
	int i, size;
	if( self->original && self->slices ){
		DArray *slices = self->slices;
		size = self->count;
		self = self->original;;
		if( size == 0 ) return;
		if( self->etype == DAO_INTEGER ){
			long sum = 0;
			dint *v = self->data.i;
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
			long sum = 0;
			dint *v = self->data.i;
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
	int i, size;

	if( self->etype == DAO_COMPLEX ) return;
	if( DaoArray_SliceSize( self ) ==0 ) return;
	if( self->original && self->slices ){
		DArray *slices = self->slices;
		size = self->count;
		self = self->original;;
		if( size > 0 ){
			for(i=0; i<size; i++ ){
				int k = DaoArray_IndexFromSlice( self, slices, i );
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
static int Compare( DaoArray *array, dint *slice, int *index, int i, int j )
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
			dint a = array->data.i[i];
			dint b = array->data.i[j];
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
static void Swap( DaoArray *array, dint *slice, int *index, int i, int j )
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
			dint a = array->data.i[i];
			dint b = array->data.i[j];
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
static void QuickSort2( DaoArray *array, dint *slice, 
		int *index, int first, int last, int part, int asc )
{
	int lower = first+1, upper = last;
	int pivot = (first + last) / 2;
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
	if( first < upper-1 ) QuickSort2( array, slice, index, first, upper-1, part, asc );
	if( upper >= part ) return;
	if( upper+1 < last ) QuickSort2( array, slice, index, upper+1, last, part, asc );
}
void DaoArray_GetSliceShape( DaoArray *self, size_t **dims, short *ndim );
static void DaoARRAY_rank( DaoProcess *proc, DaoValue *par[], int npar )
{
	DaoArray *res = DaoProcess_PutArray( proc );
	DaoArray *array = & par[0]->xArray;
	DaoArray *original = array->original;
	DArray *slices = array->slices;
	dint part = par[2]->xInteger.value;
	int i, N = DaoArray_SliceSize( array );
	int *index;
	dint *ids;

	if( res == NULL ) return;
	if( N == 0 ) return;
	res->etype = DAO_INTEGER;
	DaoArray_GetSliceShape( array, & res->dims, & res->ndim );
	DaoArray_ResizeArray( res, res->dims, res->ndim );
	ids = res->data.i;
	for(i=0; i<N; i++) ids[i] = i;

	if( N < 2 ) return;
	if( part ==0 ) part = N;
	index = dao_malloc( N * sizeof(int) );
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
	dint part = par[2]->xInteger.value;
	int i, N = DaoArray_SliceSize( array );
	dint *index;

	DaoProcess_PutValue( proc, par[0] );
	if( N < 2 ) return;
	if( part ==0 ) part = N;
	index = dao_malloc( N * sizeof(dint) );
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
	for(i=0; i<D; i++) perm->items.pSize[i] = DaoArray_GetInteger( pm, i );
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
	for(i=0; i<D; i++) perm->items.pSize[i] = D-1-i;
	DaoArray_Permute( self, perm );
	DArray_Delete( perm );
}
static void DaoARRAY_FFT( DaoProcess *proc, DaoValue *par[], int npar )
{
	DaoArray *self = & par[0]->xArray;
	int inv = ( par[1]->xEnum.value == 0 )? -1 : 1;
	int size = self->size;
	int m = 0;
	if( self->etype != DAO_COMPLEX ) return;
	while( size >>= 1 ) m ++;
	if( m == 0 ) return;
	if( size % (1<<m) !=0 ) return;
	if( abs(inv) != 1 ) return;
	dao_fft16( (complex16*) self->data.c, m, inv );
}
static void DaoARRAY_Iter( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoArray *self = & p[0]->xArray;
	DaoTuple *tuple = & p[1]->xTuple;
	DaoValue **data = tuple->items;
	DaoValue *iter = DaoValue_NewInteger(0);
	data[0]->xInteger.value = DaoArray_SliceSize( self ) >0;
	DaoValue_Copy( iter, & data[1] );
	GC_DecRC( iter );
}
static void DaoARRAY_Reverse( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoArray *self = & p[0]->xArray;
	size_t i = 0, N = self->size;
	complex16 swc, *dc = self->data.c;
	double swd, *dd = self->data.d;
	float swf, *df = self->data.f;
	dint swi, *di = self->data.i;

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
	{ DaoARRAY_Dim,       "dim( self :array, i : int )=>int" },
	{ DaoARRAY_Dim,       "dim( self :array )=>array<int>" },
	{ DaoARRAY_Index,     "index( self :array, i : int )=>array<int>" },
	{ DaoARRAY_Size,      "size( self :array )=>int" },
	{ DaoARRAY_Resize,    "resize( self :array, dims :array<int> )" },
	{ DaoARRAY_Reshape,   "reshape( self :array, dims :array<int> )" },

	{ DaoARRAY_Permute,   "permute( self :array, dims :array<int> )" },
	{ DaoARRAY_Transpose, "transpose( self :array )" },
	{ DaoARRAY_max,       "max( self :array<@ITEM> )=>tuple<@ITEM,int>" },
	{ DaoARRAY_min,       "min( self :array<@ITEM> )=>tuple<@ITEM,int>" },
	{ DaoARRAY_sum,       "sum( self :array<@ITEM> )=>@ITEM" },
	{ DaoARRAY_varn,      "varn( self :array )=>double" },
	{ DaoARRAY_Reverse,   "reverse( self :array<@ITEM> )=>array<@ITEM>" },
	{ DaoARRAY_rank,  "rank( self :array<any>, order :enum<ascend,descend>=$ascend, k=0 )=>array<int>" },
	{ DaoARRAY_sort,  "sort( self :array<@T>, order :enum<ascend,descend>=$ascend, k=0 )=>array<@T>" },

	{ DaoARRAY_FFT,    "fft( self :array<complex>, direction :enum<forward, backward> )" },
	{ DaoARRAY_Iter,   "__for_iterator__( self :array<any>, iter : for_iterator )" },

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

int DaoArray_NumType( DaoArray *self )
{
	return self->etype;
}
void DaoArray_SetNumType( DaoArray *self, short numtype )
{
	self->etype = numtype;
	DaoArray_ResizeVector( self, self->size );
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
void DaoArray_GetShape( DaoArray *self, size_t *dims )
{
	int i;
	for(i=0; i<self->ndim; i++) dims[i] = self->dims[0];
}
int DaoArray_HasShape( DaoArray *self, size_t *dims, int D )
{
	int i;
	if( D != self->ndim ) return 0;
	for(i=0; i<self->ndim; i++)
		if( dims[i] != self->dims[0] ) 
			return 0;
	return 1;
}
int DaoArray_GetFlatIndex( DaoArray *self, size_t *index )
{
	int i, id = 0;
	for( i=0; i<self->ndim; i++ ) id += index[i] * self->dims[self->ndim + i];
	return id;
}
int DaoArray_Reshape( DaoArray *self, size_t *dims, int D )
{
	int i, size = 1;
	for(i=0; i<D; i++) size *= dims[i];

	if( self->owner && self->size != size ) return 0;
	DaoArray_SetDimCount( self, D );
	memcpy( self->dims, dims, D*sizeof(size_t) );
	DaoArray_FinalizeDimData( self );
	return 1;
}
double* DaoArray_ToDouble( DaoArray *self )
{
	int i, tsize = sizeof(double);
	double *buf;
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
	int i;
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
	int i;
	float *buf = self->data.f;
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
	int i;
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
dint* DaoArray_ToInteger( DaoArray *self )
{
	int i;
	dint *buf = self->data.i;
	if( self->etype == DAO_INTEGER ) return self->data.i;
	switch( self->etype ){
	case DAO_FLOAT  : for(i=0; i<self->size; i++) buf[i] = (int)self->data.f[i]; break;
	case DAO_DOUBLE : for(i=0; i<self->size; i++) buf[i] = (int)self->data.d[i]; break;
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
void DaoArray_FromInteger( DaoArray *self )
{
	int i;
	dint *buf = self->data.i;
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
	int i, size = self->size; \
	type *buf = (type*) self->data.p; \
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
	int i, size = self->size; \
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


static void DaoArray_ResizeData( DaoArray *self, int size, int oldSize );

#define DefineFunction_DaoArray_SetVector( name, type ) \
void name( DaoArray *self, type *vec, int N ) \
{ \
	int i; \
	if( N < self->size ) DaoArray_ResizeData( self, self->size, N ); \
	switch( self->etype ){ \
	case DAO_INTEGER : for(i=0; i<N; i++) self->data.i[i] = (dint) vec[i]; break; \
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
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorI, dint );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorF, float );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorD, double );

#define DefineFunction_DaoArray_SetMatrix( name, type ) \
void name( DaoArray *self, type **mat, int R, int C ) \
{ \
	size_t dm[2]; \
	int i, j, N = R * C; \
	dm[0] = R; dm[1] = C; \
	if( N != self->size ) DaoArray_ResizeData( self, self->size, N ); \
	DaoArray_Reshape( self, dm, 2 ); \
	switch( self->etype ){ \
	case DAO_INTEGER : for(i=0; i<N; i++) self->data.i[i] = (dint)mat[i/R][i%R]; break; \
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
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixI, dint );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixF, float );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixD, double );

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
	"array", & numarrCore, NULL, (DaoFuncItem*) numarMeths, {0}, {0},
	(FuncPtrDel) DaoArray_Delete, NULL
};

#ifdef DEBUG
static int array_count = 0;
#endif

DaoArray* DaoArray_New( int etype )
{
	DaoArray* self = (DaoArray*) dao_malloc( sizeof( DaoArray ) );
	DaoValue_Init( self, DAO_ARRAY );
	self->etype = etype;
	//self->meta = NULL;
	self->unitype = NULL;
	self->size = 0;
	self->owner = 1;
	self->ndim = 0;
	self->dims = NULL;
	self->data.p = NULL;
	self->slices = NULL;
	self->original = NULL;
	DaoArray_ResizeVector( self, 0 );
#ifdef DEBUG
	array_count ++;
#endif
	return self;
}
void DaoArray_Delete( DaoArray *self )
{
	if( self->dims ) dao_free( self->dims );
	//if( self->meta ) GC_DecRC( self->meta );
	if( self->unitype ) GC_DecRC( self->unitype );
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
int DaoArray_IndexFromSlice( DaoArray *self, DArray *slices, int sid )
/* sid: plain index in the slicesd array */
{
	size_t *dimAccum = self->dims + self->ndim;
	int j, index = 0; 
	for( j=(int)slices->size-1; j>=0; j-- ){
		DArray *sub = slices->items.pArray[j];
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
void DaoArray_GetSliceShape( DaoArray *self, size_t **dims, short *ndim )
{
	DArray *shape;
	DArray *slices = self->slices;
	int i, k, S = 0, D = self->ndim;
	if( self->original == NULL ){
		if( *ndim != D ) *dims = (size_t*) dao_realloc( *dims, 2*D*sizeof(size_t) );
		*ndim = self->ndim;
		memmove( *dims, self->dims, self->ndim * sizeof(size_t) );
		return;
	}
	*ndim = 0;
	if( slices->size != self->original->ndim ) return;
	for(i=0; i<slices->size; i++){
		k = slices->items.pArray[i]->items.pSize[1];
		if( k ==0 ) return; /* skip empty dimension */
		S += k > 1;
	}
	shape = DArray_New(0);
	for(i=0; i<slices->size; i++){
		k = slices->items.pArray[i]->items.pSize[1];
		/* skip size one dimension if the final slice has at least two dimensions */
		if( k == 1 && (S > 1 || shape->size > 1) ) continue;
		DArray_Append( shape, k );
	}
	*ndim = shape->size;
	*dims = (size_t*) dao_realloc( *dims, shape->size * sizeof(size_t) );
	memmove( *dims, shape->items.pSize, shape->size * sizeof(size_t) );
	DArray_Delete( shape );
}
int DaoArray_SliceFrom( DaoArray *self, DaoArray *original, DArray *slices )
{
	int i, D = 0, S = 0;
	size_t k;
	if( slices == NULL ){
		DaoArray_ResizeArray( self, original->dims, original->ndim );
		DaoArray_CopyArray( self, original );
		return 1;
	}
	if( slices->size != original->ndim ) return 0;
	for(i=0; i<slices->size; i++){
		k = slices->items.pArray[i]->items.pSize[1];
		S += k > 1;
		if( k ==0 ){ /* skip empty dimension */
			DaoArray_ResizeVector( self, 0 );
			return 1;
		}
	}
	DaoArray_SetDimCount( self, slices->size );
	for(i=0; i<slices->size; i++){
		k = slices->items.pArray[i]->items.pSize[1];
		/* skip size one dimension if the final slice has at least two dimensions */
		if( k == 1 && (S > 1 || D > 1) ) continue;
		self->dims[D++] = k;
	}
	DaoArray_ResizeArray( self, self->dims, D );

	for(i=0; i<self->size; i++){
		int j = DaoArray_IndexFromSlice( original, slices, i );
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
	case DAO_INTEGER : return sizeof(dint);
	case DAO_FLOAT   : return sizeof(float);
	case DAO_DOUBLE  : return sizeof(double);
	case DAO_COMPLEX : return sizeof(complex16);
	}
	return 0;
}
DaoArray* DaoArray_Copy( DaoArray *self )
{
	DaoArray *copy = DaoArray_New( self->etype );
	copy->unitype = self->unitype;
	GC_IncRC( copy->unitype );
	DaoArray_ResizeArray( copy, self->dims, self->ndim );
	memcpy( copy->data.p, self->data.p, self->size * DaoArray_DataTypeSize( self ) );
	return copy;
}
static void DaoArray_ResizeData( DaoArray *self, int size, int old )
{
	int item_size = DaoArray_DataTypeSize( self );
	size_t diff = size - old;
	if( self->owner ==0 ){
		self->size = size;
		return;
	}
	self->data.p = dao_realloc( self->data.p, size * item_size );
	if( size <= old ) return;
	memset( ((char*)self->data.p) + old * item_size, 0, diff * item_size );
}
void DaoArray_ResizeVector( DaoArray *self, int size )
{
	int old = self->size;
	if( size < 0 ) return;
	DaoArray_SetDimCount( self, 2 );
	self->dims[0] = 1;
	self->dims[1] = size;
	DaoArray_FinalizeDimData( self );
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
	if( self->dims != dims ) DaoArray_SetDimCount( self, k );
	k = 0;
	for(i=0; i<D; i++){
		if( dims[i] != 1 || D ==2 ) self->dims[k++] = dims[i];
	}
	/* self->ndim will be one for dims such as [100,1,1] */
	if( self->ndim ==1 ){
		self->ndim += 1;
		self->dims = (size_t*) dao_realloc( self->dims, 2*(k+1)*sizeof(size_t) );
		if( dims[0] == 1 ){
			memmove( self->dims + 1, self->dims, k*sizeof(size_t) );
			self->dims[0] = 1;
		}else{
			self->dims[k] = 1;
		}
	}
	DaoArray_FinalizeDimData( self );
	if( self->size == old ) return;
	DaoArray_ResizeData( self, self->size, old );
}
int DaoArray_UpdateShape( DaoArray *C, DaoArray *A )
{
	int N = DaoArray_MatchShape( C, A );
	if( C->original && N < 0 ) return -1;
	if( C != A && C->original == NULL && N < 0 ){
		DaoArray_GetSliceShape( A, & C->dims, & C->ndim );
		DaoArray_ResizeArray( C, C->dims, C->ndim );
		N = C->size;
	}
	return N;
}
void DaoArray_number_op_array( DaoArray *C, DaoValue *A, DaoArray *B, short op, DaoProcess *proc )
{
	DaoArray *rB = B->original;
	DaoArray *rC = C->original;
	DaoArray *dB = rB ? rB : B;
	DaoArray *dC = rC ? rC : C;
	int i, b, c, N = DaoArray_UpdateShape( C, B );
	double bf, af = DaoValue_GetDouble( A );
	complex16 bc, ac = {0.0, 0.0};

	ac.real = af;
	if( N < 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "not matched shape" );
		return;
	}
	if( dB->etype == DAO_INTEGER && A->type == DAO_INTEGER ){
		dint bi, ci = 0, ai = A->xInteger.value;
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
			case DVM_POW : ci = powl( ai, bi );break;
			case DVM_AND : ci = ai && bi; break;
			case DVM_OR  : ci = ai || bi; break;
			default : break;
			}
			switch( C->etype ){
			case DAO_INTEGER : dC->data.i[c] = ci; break;
			case DAO_FLOAT   : dC->data.f[c] = ci; break;
			case DAO_DOUBLE  : dC->data.d[c] = ci; break;
			case DAO_COMPLEX : dC->data.c[c].real = ci; dC->data.c[c].imag = 0; break;
			}
		}
		return;
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
void DaoArray_array_op_number( DaoArray *C, DaoArray *A, DaoValue *B, short op, DaoProcess *proc )
{
	DaoArray *rA = A->original;
	DaoArray *rC = C->original;
	DaoArray *dA = rA ? rA : A;
	DaoArray *dC = rC ? rC : C;
	int i, a, c, N = DaoArray_UpdateShape( C, A );
	double af, bf = DaoValue_GetDouble( B );
	dint ai, ci = 0, bi = DaoValue_GetInteger( B );
	complex16 ac, bc = {0.0, 0.0};

	bc.real = bf;
	if( N < 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "not matched shape" );
		return;
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
			case DVM_POW : ci = powl( ai, bi );break;
			case DVM_AND : ci = ai && bi; break;
			case DVM_OR  : ci = ai || bi; break;
			default : break;
			}
			switch( C->etype ){
			case DAO_INTEGER : dC->data.i[c] = ci; break;
			case DAO_FLOAT   : dC->data.f[c] = ci; break;
			case DAO_DOUBLE  : dC->data.d[c] = ci; break;
			case DAO_COMPLEX : dC->data.c[c].real = ci; dC->data.c[c].imag = 0; break;
			}
		}
		return;
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
			case DVM_MOD : dC->data.i[c] = (dint)af % (dint)bf; break;
			case DVM_POW : dC->data.i[c] = pow( af, bf );break;
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
			case DVM_POW : dC->data.d[c] = pow( af, bf );break;
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
void DaoArray_ArrayArith( DaoArray *C, DaoArray *A, DaoArray *B, short op, DaoProcess *proc )
{
	DaoArray *rA = A->original;
	DaoArray *rB = B->original;
	DaoArray *rC = C->original;
	DaoArray *dA = rA ? rA : A;
	DaoArray *dB = rB ? rB : B;
	DaoArray *dC = rC ? rC : C;
	int N = DaoArray_MatchShape( A, B );
	int M = C == A ? N : DaoArray_MatchShape( C, A );
	int i, a, b, c;
	if( N < 0 || (C->original && M != N) ){
		DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "not matched shape" );
		return;
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
	}else if( dA->etype == DAO_INTEGER && dB->etype == DAO_INTEGER ){
		dint res = 0;
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
			case DVM_POW : res = powl( dA->data.i[a], dB->data.i[b] );break;
			case DVM_AND : res = dA->data.i[a] && dB->data.i[b]; break;
			case DVM_OR  : res = dA->data.i[a] || dB->data.i[b]; break;
			default : break;
			}
			switch( C->etype ){
			case DAO_INTEGER : dC->data.i[c] = res; break;
			case DAO_FLOAT   : dC->data.f[c] = res; break;
			case DAO_DOUBLE  : dC->data.d[c] = res; break;
			case DAO_COMPLEX : dC->data.c[c].real = res; dC->data.c[c].imag = 0; break;
			}
		}
		return;
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
			case DVM_MOD : dC->data.i[c] = (dint)ad % (dint)bd; break;
			case DVM_POW : dC->data.i[c] = powf( ad, bd );break;
			case DVM_AND : dC->data.i[c] = ad && bd; break;
			case DVM_OR  : dC->data.i[c] = ad || bd; break;
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
			case DVM_MOD : dC->data.f[c] = (dint)ad%(dint)bd; break;
			case DVM_POW : dC->data.f[c] = powf( ad, bd );break;
			case DVM_AND : dC->data.f[c] = ad && bd; break;
			case DVM_OR  : dC->data.f[c] = ad || bd; break;
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

DaoValue* DaoArray_GetValue( DaoArray *self, int i, DaoValue *res )
{
	res->type = self->etype;
	switch( self->etype ){
	case DAO_INTEGER : res->xInteger.value = self->data.i[i]; break;
	case DAO_FLOAT : res->xFloat.value = self->data.f[i]; break;
	case DAO_DOUBLE : res->xDouble.value = self->data.d[i]; break;
	case DAO_COMPLEX : res->xComplex.value = self->data.c[i]; break;
	default : break;
	}
	return res;
}
void DaoArray_SetValue( DaoArray *self, int i, DaoValue *value )
{
	switch( self->etype ){
	case DAO_INTEGER : self->data.i[i] = DaoValue_GetInteger( value ); break;
	case DAO_FLOAT : self->data.f[i] = DaoValue_GetFloat( value ); break;
	case DAO_DOUBLE : self->data.d[i] = DaoValue_GetDouble( value ); break;
	case DAO_COMPLEX : self->data.c[i] = DaoValue_GetComplex( value ); break;
	default : break;
	}
}
static void DaoARRAY_BasicFunctional( DaoProcess *proc, DaoValue *p[], int npar, int funct )
{
	DaoComplex com = {DAO_COMPLEX,0,0,0,1,{0.0,0.0}};
	DaoList *list = NULL;
	DaoArray *array = NULL;
	DaoArray *indices = NULL;
	DaoArray *self2 = & p[0]->xArray;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );;
	DaoValue **idval = proc->activeValues + sect->a + 1;
	DaoValue *elem, *res = NULL;
	DaoArray *original = self2->original;
	DaoArray *self = original ? original : self2;
	DArray *slices = self2->slices;
	size_t *dims = self->dims;
	size_t N = DaoArray_SliceSize( self2 );
	size_t i, id, id2, first = 0;
	int j, D = self->ndim;
	int isvec = (D == 2 && (dims[0] ==1 || dims[1] == 1));
	int entry, vdim = sect->b - 1;
	int stackBase = proc->topFrame->active->stackBase;
	dint *count = NULL;

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
		indices->trait |= DAO_DATA_CONST;
		break;
	case DVM_FUNCT_FOLD :
		if( npar > 1 ){
			res = p[1];
		}else if( N ){
			id = original ? DaoArray_IndexFromSlice( original, slices, 0 ) : 0;
			res = (DaoValue*)(void*) &com;
			DaoArray_GetValue( self, id, res );
			first = 1;
		}
		DaoProcess_PutValue( proc, res );
		break;
	case DVM_FUNCT_SELECT : list = DaoProcess_PutList( proc ); break;
	case DVM_FUNCT_COUNT : count = DaoProcess_PutInteger( proc, 0 ); break;
	case DVM_FUNCT_APPLY : DaoProcess_PutReference( proc, original ? original : self ); break;
	}
	if( sect == NULL ) return;
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	for(j=0; j<vdim; j++) idval[j]->xInteger.value = 0;
	for(i=first; i<N; i++){
		idval = proc->stackValues + stackBase + sect->a + 1;
		id = id2 = (original ? DaoArray_IndexFromSlice( original, slices, i ) : i);
		if( isvec ){
			if( vdim >0 ) idval[0]->xInteger.value = id2;
			if( vdim >1 ) idval[1]->xInteger.value = id2;
		}else{
			for( j=D-1; j>=0; j--){
				int k = id2 % dims[j];
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
	DaoProcess_PopFrame( proc );
	if( indices ) DaoArray_Delete( indices );
	if( funct == DVM_FUNCT_FOLD ){
		DaoProcess_SetActiveFrame( proc, proc->topFrame );
		DaoProcess_PutValue( proc, res );
	}
}
#endif
