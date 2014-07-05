/*
// Dao Standard Modules
// http://www.daovm.net
//
// Copyright (c) 2014, Limin Fu
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
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include<math.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>
#include"daoStdtype.h"
#include"daoValue.h"
#include"daoProcess.h"
#include"daoVmspace.h"
#include"daoThread.h"

#define LONG_BITS 8
#define LONG_BASE 256
#define LONG_MASK 255

typedef signed char        schar_t;
typedef struct DaoxBigInt  DaoxBigInt;
typedef struct DaoxBigIntBuffer DaoxBigIntBuffer;

/* bit integer */
struct DaoxBigInt
{
	DAO_CSTRUCT_COMMON;

	uchar_t  *data;
	uchar_t   base;
	schar_t   sign;
	ushort_t  offset;
	daoint    size;
	daoint    bufSize;
};
DaoType *daox_type_bigint = NULL;

DAO_DLL DaoxBigInt* DaoxBigInt_New();
DAO_DLL void DaoxBigInt_Delete( DaoxBigInt *self );
DAO_DLL void DaoxBigInt_Detach( DaoxBigInt *self );
DAO_DLL void DaoxBigInt_Clear( DaoxBigInt *self );

DAO_DLL void DaoxBigInt_Copy( DaoxBigInt *z, DaoxBigInt *x );
DAO_DLL void DaoxBigInt_Resize( DaoxBigInt *self, daoint size );
DAO_DLL void DaoxBigInt_PushBack( DaoxBigInt *self, uchar_t it );
DAO_DLL void DaoxBigInt_PushFront( DaoxBigInt *self, uchar_t it );
DAO_DLL int DaoxBigInt_Compare( DaoxBigInt *x, DaoxBigInt *y );
DAO_DLL void DaoxBigInt_Move( DaoxBigInt *z, DaoxBigInt *x );
DAO_DLL void DaoxBigInt_Add( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y );
DAO_DLL void DaoxBigInt_Sub( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y );
DAO_DLL void DaoxBigInt_Mul( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y, DaoxBigIntBuffer *b );
DAO_DLL void DaoxBigInt_Div( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y, DaoxBigInt *r );
DAO_DLL void DaoxBigInt_Pow( DaoxBigInt *z, DaoxBigInt *x, daoint n, DaoxBigIntBuffer *b );
DAO_DLL void DaoxBigInt_AddInt( DaoxBigInt *z, DaoxBigInt *x, daoint y, DaoxBigInt *buf );
DAO_DLL void DaoxBigInt_MulInt( DaoxBigInt *z, DaoxBigInt *x, daoint y );
DAO_DLL void DaoxBigInt_Flip( DaoxBigInt *self );
DAO_DLL void DaoxBigInt_BitAND( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y );
DAO_DLL void DaoxBigInt_BitOR( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y );
DAO_DLL void DaoxBigInt_BitXOR( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y );
DAO_DLL void DaoxBigInt_ShiftLeft( DaoxBigInt *self, int bits );
DAO_DLL void DaoxBigInt_ShiftRight( DaoxBigInt *self, int bits );
DAO_DLL void DaoxBigInt_Print( DaoxBigInt *self, DString *s );
DAO_DLL void DaoxBigInt_FromInteger( DaoxBigInt *self, daoint x );
DAO_DLL void DaoxBigInt_FromDouble( DaoxBigInt *self, double x );
DAO_DLL char DaoxBigInt_FromString( DaoxBigInt *self, DString *s );
DAO_DLL void DaoxBigInt_FromValue( DaoxBigInt *self, DaoValue *value );
DAO_DLL daoint DaoxBigInt_ToInteger( DaoxBigInt *self );
DAO_DLL double DaoxBigInt_ToDouble( DaoxBigInt *self );
DAO_DLL int DaoxBigInt_CompareToZero( DaoxBigInt *self );
DAO_DLL int DaoxBigInt_CompareToInteger( DaoxBigInt *self, daoint x );
DAO_DLL int DaoxBigInt_CompareToDouble( DaoxBigInt *self, double x );

DAO_DLL void DaoxBigInt_UAdd( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y );
DAO_DLL void DaoxBigInt_UMul( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y, DaoxBigIntBuffer *b );
DAO_DLL int DaoxBigInt_UCompare( DaoxBigInt *x, DaoxBigInt *y );
DAO_DLL daoint DaoxBigInt_NormCount( DaoxBigInt *self );
#define DaoxBigInt_Append  DaoxBigInt_PushBack


struct DaoxBigIntBuffer
{
	DList *coms;
	DList *ints;
};
static DaoxBigIntBuffer* DaoxBigIntBuffer_New()
{
	DaoxBigIntBuffer *self = (DaoxBigIntBuffer*) dao_malloc( sizeof(DaoxBigIntBuffer) );
	self->coms = DList_New(0);
	self->ints = DList_New(0);
	return self;
}
static void DaoxBigIntBuffer_Delete( DaoxBigIntBuffer *self )
{
	int i;
	for(i=0; i<self->coms->size; ++i){
		DArray_Delete( (DArray*) self->coms->items.pVoid[i] );
	}
	for(i=0; i<self->ints->size; ++i){
		DaoxBigInt_Delete( (DaoxBigInt*) self->ints->items.pVoid[i] );
	}
	DList_Delete( self->coms );
	DList_Delete( self->ints );
	dao_free( self );
}
static DArray* DaoxBigIntBuffer_NewVector( DaoxBigIntBuffer *self )
{
	DArray *vec;
	if( self->coms->size ) return (DArray*) DList_PopBack( self->coms );
	return DArray_New( sizeof(complex16) );
}
static DaoxBigInt* DaoxBigIntBuffer_NewBigInt( DaoxBigIntBuffer *self )
{
	DaoxBigInt *bigint;
	if( self->ints->size ) return (DaoxBigInt*) DList_PopBack( self->ints );
	return DaoxBigInt_New();
}
static void DaoxBigIntBuffer_FreeVector( DaoxBigIntBuffer *self, DArray *vec )
{
	DList_Append( self->coms, vec );
}
static void DaoxBigIntBuffer_FreeBigInt( DaoxBigIntBuffer *self, DaoxBigInt *bigint )
{
	DList_Append( self->ints, bigint );
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



#ifdef DAO_WITH_THREAD
DMutex  mutex_long_sharing;
#endif

DaoxBigInt* DaoxBigInt_New()
{
	DaoxBigInt *self = (DaoxBigInt*) dao_calloc( 1, sizeof(DaoxBigInt) );
	DaoCstruct_Init( (DaoCstruct*) self, daox_type_bigint );
	self->sign = 1;
	self->base = 10;
	return self;
}
void DaoxBigInt_Delete( DaoxBigInt *self )
{
	uint_t *pbuf;
	DaoxBigInt_Detach( self );
	pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
	if( pbuf ) dao_free( pbuf );
	DaoCstruct_Free( (DaoCstruct*) self );
	dao_free( self );
}
void DaoxBigInt_Clear( DaoxBigInt *self )
{
	uint_t *pbuf;
	DaoxBigInt_Detach( self );
	pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
	if( pbuf ) dao_free( pbuf );
	self->data = NULL;
	self->offset = 0;
	self->size = self->bufSize = 0;
}
void DaoxBigInt_Detach( DaoxBigInt *self /* , int extrasize  TODO */ )
{
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
		pbuf2 = (uint_t*) dao_malloc( sizeof(uint_t) + self->bufSize * sizeof(uchar_t) );
		pbuf2[0] = 1;
		memcpy( (uchar_t*)(pbuf2+1) + self->offset, self->data, self->size * sizeof(uchar_t) );
		self->data = (uchar_t*)(pbuf2 + 1) + self->offset;
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & mutex_long_sharing );
#endif
}
void DaoxBigInt_Resize( DaoxBigInt *self, daoint size )
{
	daoint i;
	uint_t *pbuf;
	DaoxBigInt_Detach( self );
	pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
	if( size == self->size && self->bufSize >0 ) return;
	if( self->offset ){
		daoint min = size > self->size ? self->size : size;
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
void DaoxBigInt_Reserve( DaoxBigInt *self, daoint size )
{
	daoint i;
	uint_t *pbuf;
	DaoxBigInt_Detach( self );
	if( size + self->offset <= self->bufSize ) return;
	pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
	pbuf = (uint_t*)dao_realloc( pbuf, (size + self->offset)*sizeof(uchar_t) + sizeof(int) );
	self->data = (uchar_t*)(pbuf + 1) + self->offset;
	self->bufSize = size + self->offset;
}
void DaoxBigInt_PushBack( DaoxBigInt *self, uchar_t it )
{
	uint_t *pbuf;
	DaoxBigInt_Detach( self );
	pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
	if( self->size + self->offset + 1 > self->bufSize ){
		self->bufSize += self->bufSize/5 + 1;
		pbuf = (uint_t*)dao_realloc( pbuf, self->bufSize*sizeof(uchar_t) + sizeof(int) );
		self->data = (uchar_t*)(pbuf + 1) + self->offset;
	}
	self->data[ self->size ] = it;
	self->size ++;
}
void DaoxBigInt_PushFront( DaoxBigInt *self, uchar_t it )
{
	DaoxBigInt_Detach( self );
	if( self->offset == 0 ){
		uint_t *pbuf = (uint_t*)(self->data ? self->data - self->offset - sizeof(uint_t) : NULL);
		uint_t offset = self->bufSize/5 + 1;
		self->offset = offset < 0xffff ? offset : 0xffff;
		self->bufSize += self->offset;
		pbuf = (uint_t*) dao_realloc( pbuf, self->bufSize*sizeof(uchar_t) + sizeof(uint_t) );
		self->data = (uchar_t*)(pbuf + 1) + self->offset;
		memmove( self->data, pbuf + 1, self->size*sizeof(uchar_t) );
	}
	self->offset --;
	self->data --;
	self->data[0] = it;
	self->size ++;
}
void DaoxBigInt_Normalize( DaoxBigInt *self )
{
	daoint n = self->size;
	while( n && self->data[ n-1 ] ==0 ) n --;
	DaoxBigInt_Resize( self, n );
}
void DaoxBigInt_Normalize2( DaoxBigInt *self )
{
	daoint n = self->size;
	while( n && self->data[ n-1 ] ==0 ) n --;
	self->size = n;
}
int DaoxBigInt_UCompare( DaoxBigInt *x, DaoxBigInt *y )
{
	daoint nx = x->size;
	daoint ny = y->size;
	uchar_t *dx = x->data;
	uchar_t *dy = y->data;
	while( nx >0 && dx[nx-1] ==0 ) nx --;
	while( ny >0 && dy[ny-1] ==0 ) ny --;
	if( nx > ny ) return 1; else if( nx < ny ) return -1;
	while( nx > 0 && dx[nx-1] == dy[ny-1] ) nx --, ny --;
	if( nx == 0 ) return 0;
	return ( dx[nx-1] > dy[ny-1] ) ? 1 : -1;
}
int DaoxBigInt_Compare( DaoxBigInt *x, DaoxBigInt *y )
{
	if( x->sign != y->sign ) return x->sign - y->sign;
	return x->sign * DaoxBigInt_UCompare( x, y );
}
int DaoxBigInt_CompareToZero( DaoxBigInt *self )
{
	daoint n = self->size;
	while( n && self->data[n-1] ==0 ) n -= 1;
	if( n == 0 ) return 0;
	return self->sign;
}
int DaoxBigInt_CompareToInteger( DaoxBigInt *self, daoint x )
{
	daoint i, n = self->size - 1, m = - 1 + (sizeof(daoint)*8) / LONG_BITS;
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
int DaoxBigInt_CompareToDouble( DaoxBigInt *self, double x )
{
	double prod, frac;
	int i, expon, bit, bit2, res;

	if( self->sign > 0 && x < 0 ) return 1;
	if( self->sign < 0 && x > 0 ) return -1;

	frac = frexp( fabs( x ), & expon );
	if( expon <=0 ){ /* |x|<1 */
		res = DaoxBigInt_CompareToZero( self );
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
void DaoxBigInt_Move( DaoxBigInt *z, DaoxBigInt *x )
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
	z->data = x->data;
	z->base = x->base;
	z->sign = x->sign;
	z->offset = x->offset;
	z->size = x->size;
	z->bufSize = x->bufSize;
	if( xbuf ) xbuf[0] += 1;
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & mutex_long_sharing );
#endif
}
void DaoxBigInt_Copy( DaoxBigInt *z, DaoxBigInt *x )
{
	daoint nx = x->size;
	daoint nz = z->size;
	DaoxBigInt_Detach( z );
	if( nx+nx < nz || nz < nx ) DaoxBigInt_Resize( z, nx );
	z->sign = x->sign;
	z->base = x->base;
	memmove( z->data, x->data, nx * sizeof(uchar_t) );
	z->size = nx;
	if( x->base != 2 ) DaoxBigInt_Normalize2( z );
}

void DaoxBigInt_UAdd( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y )
{
	uchar_t *dx, *dy, *dz;
	daoint nx = x->size;
	daoint ny = y->size;
	daoint i, sum = 0;
	DaoxBigInt_Detach( z );
	if( x->size > y->size ){
		DaoxBigInt *tmp = x;
		x = y;  y = tmp;
	}
	nx = x->size;
	ny = y->size;
	if( z->bufSize <= ny ) DaoxBigInt_Resize( z, ny );
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
		DaoxBigInt_Append( z, sum & LONG_MASK );
		sum = sum >> LONG_BITS;
	}
}
/* x must be larger than y: */
static void LongSub3( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y )
{
	uchar_t *dx, *dy, *dz;
	daoint i, nx, ny, sub = 1;

	DaoxBigInt_Normalize2( x );
	DaoxBigInt_Normalize2( y );
	nx = x->size;
	ny = y->size;
	assert( DaoxBigInt_UCompare( x, y ) >=0 );
	DaoxBigInt_Detach( z );
	if( z->bufSize < nx ) DaoxBigInt_Resize( z, nx );
	dx = x->data;
	dy = y->data;
	dz = z->data;
	for(i=0; i<ny; i++){
		/* sub = LONG_BASE + dx[i] - dy[i] - (1-sub); */
#if 0
		if( i >= nx ){
			printf( "error %li %li  %i\n", nx, ny, dy[i] );
			DaoxBigInt_Print( x, NULL );
			DaoxBigInt_Print( y, NULL );
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
static int LongSub2( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y ) /* unsigned */
{
	z->sign = 1;
	if( DaoxBigInt_UCompare( x, y ) <0 ){
		DaoxBigInt *tmp = x;
		x = y;  y = tmp;
		z->sign = -1;
	}
	DaoxBigInt_Detach( z );
	LongSub3( z, x, y );
	return z->sign;
}
void DaoxBigInt_Add( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y )
{
	DaoxBigInt_Detach( z );
	if( x->sign == y->sign ){
		DaoxBigInt_UAdd( z, x, y );
		z->sign = x->sign;
	}else if( x->sign >0 ){
		z->sign = LongSub2( z, x, y );
	}else{
		z->sign = LongSub2( z, y, x );
	}
}
void DaoxBigInt_Sub( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y )
{
	DaoxBigInt_Detach( z );
	if( x->sign == y->sign ){
		z->sign = LongSub2( z, x, y ) * x->sign;
	}else if( x->sign >0 ){
		DaoxBigInt_UAdd( z, x, y );
		z->sign = 1;
	}else{
		DaoxBigInt_UAdd( z, x, y );
		z->sign = -1;
	}
}
static void DaoxBigInt_MulAdd( DaoxBigInt *z, DaoxBigInt *x, uchar_t y, short m );
void DaoxBigInt_UMulDigitX( DaoxBigInt *z, DaoxBigInt *x, uchar_t digit );
static void DaoxBigInt_UMulSimple( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y, DaoxBigIntBuffer *buffer )
{
	daoint i, n = x->size + y->size;
	DaoxBigInt_Detach( z );
	if( z == x || z == y ){
		DaoxBigIntBuffer *inbuf = buffer;
		DaoxBigInt *z2;
		if( buffer == NULL ) buffer = DaoxBigIntBuffer_New();
		z2 = DaoxBigIntBuffer_NewBigInt( buffer );
		DaoxBigInt_UMulSimple( z2, x, y, buffer );
		DaoxBigInt_Move( z, z2 );
		DaoxBigIntBuffer_FreeBigInt( buffer, z2 );
		if( buffer != inbuf ) DaoxBigIntBuffer_Delete( buffer );
		return;
	}
	if( z->bufSize < n ) DaoxBigInt_Reserve( z, n );
	z->size = n;
	memset( z->data, 0, z->size * sizeof(uchar_t) );
	for(i=0; i<x->size; i++) DaoxBigInt_MulAdd( z, y, x->data[i], i );
}
static void DaoxBigInt_Split( DaoxBigInt *x, DaoxBigInt *x1, DaoxBigInt *x0, daoint m )
{
	daoint size = x->size;
	DaoxBigInt_Detach( x0 );
	DaoxBigInt_Detach( x1 );
	memmove( x0->data, x->data, m * sizeof(uchar_t) );
	memmove( x1->data, x->data + m, (size-m) * sizeof(uchar_t) );
	x0->size = m;
	x1->size = size - m;
}
static void LongCat( DaoxBigInt *z, DaoxBigInt *x1, daoint m, DaoxBigInt *x0 )
{
	/* z might be the same object as x1: */
	daoint n = x1->size;
	DaoxBigInt_Detach( z );
	if( z->bufSize < n + m + 1 ) DaoxBigInt_Reserve( z, n + m + 1 );
	memmove( z->data + m, x1->data, n * sizeof(uchar_t) );
	memset( z->data, 0, m * sizeof(uchar_t) );
	z->size = m + n;
	DaoxBigInt_UAdd( z, z, x0 );
}
static void LongMulSum( DaoxBigInt *z, DaoxBigInt *z2, DaoxBigInt *z1, DaoxBigInt *z0, int m )
{
	daoint n = m + z1->size;
	DaoxBigInt_Detach( z );
	DaoxBigInt_Detach( z1 );
	if( z2 ) n = z2->size + m + m;
	if( z->bufSize <= n ) DaoxBigInt_Reserve( z, n );
	memmove( z->data, z0->data, z0->size * sizeof(uchar_t) );
	if( z2 ) memmove( z->data + (m+m), z2->data, z2->size *sizeof(uchar_t) );
	if( z1->bufSize <= z1->size+m ) DaoxBigInt_Reserve( z1, z1->size + m );
	memmove( z1->data+m, z1->data, z1->size*sizeof(uchar_t) );
	z->size = n;
	z1->size += m;
	DaoxBigInt_UAdd( z, z, z1 );
}
static void LongZ1( DaoxBigInt *z1, DaoxBigInt *z0, DaoxBigInt *z2 )
{
	daoint i, sub=2;
	daoint nz1 = z1->size;
	daoint nz0 = z0->size;
	daoint nz2 = z2->size;
	uchar_t *dz1 = z1->data;
	uchar_t *dz0 = z0->data;
	uchar_t *dz2 = z2->data;
	DaoxBigInt_Detach( z1 );
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
static void DaoxBigInt_UMulFFT( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y, DaoxBigIntBuffer *buffer )
{
	DArray *vx = DaoxBigIntBuffer_NewVector( buffer );
	DArray *vy = DaoxBigIntBuffer_NewVector( buffer );
	complex16 *cx, *cy;
	uchar_t *dx = x->data;
	uchar_t *dy = y->data;
	daoint nx = x->size;
	daoint ny = y->size;
	daoint max = nx > ny ? nx : ny;
	daoint i, nc = 1;
	daoint c = 0;
	int mc = 0;
	while( (nc>>1) < max ) nc <<= 1, mc ++;
	/* printf( "nc = %i, mc = %i, max = %i\n", nc, mc, max ); */
	DArray_Reserve( vx, nc );
	cx = cy = vx->data.complexes;
	memset( cx, 0, nc*sizeof(complex16) );
	for(i=0; i<nx; i++) cx[i].real = dx[i];
	dao_fft16( cx, mc, -1 );
	if( x == y ){
		cy = cx;
	}else{
		DArray_Reserve( vy, nc );
		cy = vy->data.complexes;
		memset( cy, 0, nc*sizeof(complex16) );
		for(i=0; i<ny; i++) cy[i].real = dy[i];
		dao_fft16( cy, mc, -1 );
	}
	for(i=0; i<nc; i++) complex16_mul( cx[i], cx[i], cy[i] );
	dao_fft16( cx, mc, 1 );
	DaoxBigInt_Resize( z, nc );
	memset( z->data, 0, nc*sizeof(uchar_t) );
	for(i=0; i<nc; i++){
		c += (daoint)(cx[i].real / nc + 0.5);
		z->data[i] = c & LONG_MASK;
		c = c >> LONG_BITS;
	}
	while( nc && z->data[nc-1] ==0 ) nc --;
	DaoxBigInt_Resize( z, nc );
	DaoxBigIntBuffer_FreeVector( buffer, vx );
	DaoxBigIntBuffer_FreeVector( buffer, vy );
}
void DaoxBigInt_UMul( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y, DaoxBigIntBuffer *buffer )
{
	uchar_t *dx = x->data;
	uchar_t *dy = y->data;
	daoint nx = x->size;
	daoint ny = y->size;
	/*
	   printf( "nx = %i,  ny = %i,  dep = %i\n", nx, ny, dep );
	   DaoxBigInt_Print( x, NULL );
	   DaoxBigInt_Print( y, NULL );
	 */
	DaoxBigInt_Detach( z );
	if( (nx|ny) <= 1 ){
		if( (nx&ny) == 1 ){
			int prod = dx[0] * dy[0];
			if( z->bufSize < 2 ) DaoxBigInt_Reserve( z, 2 );
			z->data[0] = prod & LONG_MASK;
			z->data[1] = prod >> LONG_BITS;
			z->size = prod >> LONG_BITS ? 2 : (prod ? 1 : 0);
		}else{
			z->size = 0;
		}
		return;
	}else if( nx == 1 ){
		DaoxBigInt_UMulDigitX( z, y, dx[0] );
		return;
	}else if( ny == 1 ){
		DaoxBigInt_UMulDigitX( z, x, dy[0] );
		return;
	}else if( nx <= 4 ){
		DaoxBigInt_UMulSimple( z, x, y, buffer );
		return;
	}else if( ny <= 4 ){
		DaoxBigInt_UMulSimple( z, y, x, buffer );
		return;
	}else if( nx < 16 && ny < 16 ){
		DaoxBigInt_UMulSimple( z, x, y, buffer );
		return;
#if 0
	}else if( nx < 64 && nx < 64 ){
		return;
#endif
	}
#if 0
	DaoxBigInt_UMulK( z, x, y, NULL, 0 );
#else
	DaoxBigInt_UMulFFT( z, x, y, buffer );
#endif
}
void DaoxBigInt_Mul( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y, DaoxBigIntBuffer *buffer )
{
	DaoxBigIntBuffer *inbuf = buffer;
	if( buffer == NULL ) buffer = DaoxBigIntBuffer_New();
	DaoxBigInt_UMul( z, x, y, buffer );
	DaoxBigInt_Normalize2( z );
	/* DaoxBigInt_UMulK( z, x, y, NULL, 0 ); */
	z->sign = x->sign * y->sign;
	if( buffer != inbuf ) DaoxBigIntBuffer_Delete( buffer );
}
daoint DaoxBigInt_NormCount( DaoxBigInt *self )
{
	int i, d;
	while( self->size && self->data[ self->size-1 ] ==0 ) self->size --;
	/* if( self->size * 2 < self->bufSize ) DaoxBigInt_Resize( self->size ); */
	if( self->size ==0 ) return 0;
	d = self->data[ self->size-1 ];
	for(i=1; d && (i<LONG_BITS); i++) d >>= 1;
	return self->size * LONG_BITS + i - LONG_BITS;
}
void DaoxBigInt_UMulDigitX( DaoxBigInt *z, DaoxBigInt *x, uchar_t digit )
{
	uchar_t *dz, *dx = x->data;
	daoint i, nx = x->size;
	uint_t carray = 0;
	if( digit == 0 ){
		z->size = 0;
		return;
	}else if( digit == 1 ){
		DaoxBigInt_Move( z, x );
		return;
	}
	DaoxBigInt_Detach( z );
	while(nx >0 && x->data[nx-1] ==0 ) nx--;
	if( z->bufSize < nx ) DaoxBigInt_Reserve( z, nx );
	dz = z->data;
	z->size = nx;
	for(i=0; i<nx; i++){
		carray += digit * dx[i];
		dz[i] = carray & LONG_MASK;
		carray = carray >> LONG_BITS;
	}
	if( carray ) DaoxBigInt_Append( z, carray );
	DaoxBigInt_Normalize2( z );
}
uchar_t DaoxBigInt_UDivDigit( DaoxBigInt *z, uchar_t digit )
{
	daoint i, nz = z->size;
	uint_t carray = 0;
	DaoxBigInt_Detach( z );
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
void DaoxBigInt_Div( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y, DaoxBigInt *r )
{
	DaoxBigInt *mul;
	DaoxBigInt *r2;
	uchar_t *pbase;
	daoint nx = x->size;
	daoint nz = z->size;
	daoint nr;
	daoint hr;
	int cmp, hx;
	uchar_t d;

	while(nx >0 && x->data[nx-1] ==0 ) nx--;
	while(nz >0 && z->data[nz-1] ==0 ) nz--;
	DaoxBigInt_Detach( y );
	DaoxBigInt_Copy( r, z );
	y->sign = z->sign * x->sign;
	if( nz < nx ) return;
	if( nx ==0 ){
		/* XXX error */
		y->size = 0;
		return;
	}else if( nx == 1 ){
		DaoxBigInt_Move( y, z );
		r->size = 1;
		r->data[0] = DaoxBigInt_UDivDigit( y, x->data[0] );
		return;
	}
	y->size = 0; /* z might be y */
	mul = DaoxBigInt_New();
	r2 = DaoxBigInt_New();
	pbase = r->data;
	r->data = r->data + (r->size - nx);
	r->size = nx;
	hx = (x->data[nx-1] << LONG_BITS) | x->data[nx-2];
	/* sliding x over r (originall equal to z) */
	while( r->data >= pbase ){
		if( r->size < nx || (r->size == nx && r->data[nx-1] < x->data[nx-1]) ){
			DaoxBigInt_PushFront( y, 0 );
			r->data --;
			r->size ++;
			continue;
		}
		assert( (r->size-1) <= nx );
		nr = r->size;
		hr = (r->data[nx-1] << LONG_BITS) | r->data[nx-2];
		if( nr > nx ) hr |= ((daoint)r->data[nx]) << (LONG_BITS<<1);
		/* using the first two digits of the divisor to guess the quotient */
		d = hr / hx;
		DaoxBigInt_UMulDigitX( mul, x, d );
		cmp = DaoxBigInt_UCompare( mul, r );
		if( cmp ==0 ){
			r->size = 0;
			r->data --;
			DaoxBigInt_PushFront( y, d );
			continue;
		}else if( cmp >0 ){
			d --;
			DaoxBigInt_UMulDigitX( mul, x, d );
		}
		if( y->sign <0 && r->data == pbase ){
			d ++;
			DaoxBigInt_UMulDigitX( mul, x, d );
			LongSub3( r2, mul, r );
		}else{
			LongSub3( r2, r, mul );
		}
		DaoxBigInt_PushFront( y, d );
		memmove( r->data, r2->data, r2->size *sizeof(uchar_t) );
		r->size = r2->size + 1;
		r->data --;
	}
	r->size --;
	r->data = pbase;
	DaoxBigInt_Delete( mul );
	DaoxBigInt_Delete( r2 );
	DaoxBigInt_Normalize2( y );
	DaoxBigInt_Normalize2( r );
}
void DaoxBigInt_Pow( DaoxBigInt *z, DaoxBigInt *x, daoint n, DaoxBigIntBuffer *buffer )
{
	DaoxBigIntBuffer *inbuf = buffer;
	daoint m = 1;
	if( n == 1 ){
		DaoxBigInt_Move( z, x );
		return;
	}else if( n == 0 ){
		DaoxBigInt_Resize( z, 1 );
		z->data[0] = 1;
		return;
	}else if( x->size == 0 ){
		DaoxBigInt_Resize( z, 0 );
		return;
	}else if( x->size == 1 && x->data[0] ==1 ){
		DaoxBigInt_Resize( z, 1 );
		z->data[0] = 1;
		z->sign = n%2 ? 1 : x->sign;
		return;
	}else if( n == 2 ){
		if( buffer == NULL ) buffer = DaoxBigIntBuffer_New();
		DaoxBigInt_Mul( z, x, x, buffer );
		if( buffer != inbuf ) DaoxBigIntBuffer_Delete( buffer );
		return;
	}
	if( buffer == NULL ) buffer = DaoxBigIntBuffer_New();
	DaoxBigInt_Copy( z, x );
	while( 2*m <= n ){
		DaoxBigInt_Mul( z, z, z, buffer );
		m *= 2;
	}
	if( m < n ){
		DaoxBigInt *tmp = DaoxBigInt_New();
		DaoxBigInt_Pow( tmp, x, n-m, buffer );
		DaoxBigInt_Mul( z, z, tmp, buffer );
		DaoxBigInt_Delete( tmp );
	}
	if( buffer != inbuf ) DaoxBigIntBuffer_Delete( buffer );
}
void DaoxBigInt_AddInt( DaoxBigInt *z, DaoxBigInt *x, daoint y, DaoxBigInt *buf )
{
}
void DaoxBigInt_MulAdd( DaoxBigInt *Z, DaoxBigInt *X, uchar_t Y, short M )
{
	daoint i, NZ, NX = X->size, NXM = NX + M;
	uchar_t *DZ, *DX = X->data;
	uint_t carray = 0;

	DaoxBigInt_Detach( Z );
	if( Z->bufSize < NX + M ) DaoxBigInt_Reserve( Z, NX + M );
	DZ = Z->data;
	for(i=Z->size; i<NXM; i++) DZ[i] = 0;
	if( Z->size < NXM ) Z->size = NXM;
	NZ = Z->size;
	for(i=0; i<NX; i++){
		carray += DX[i] * Y + DZ[i+M];
		DZ[i+M] = carray & LONG_MASK;
		carray = carray >> LONG_BITS;
	}
	for(i=NX+M; i<NZ; i++){
		carray += DZ[i];
		DZ[i] = carray & LONG_MASK;
		carray = carray >> LONG_BITS;
	}
	while( carray ){
		DaoxBigInt_Append( Z, carray & LONG_MASK );
		carray = carray >> LONG_BITS;
	}
}
void DaoxBigInt_MulInt( DaoxBigInt *z, DaoxBigInt *x, daoint y )
{
	int m = 0, sign = 1;
	if( y ==0 ){
		z->size = 0;
		return;
	}else if( y < 0 ){
		sign = -1;
		y = - y;
	}
	DaoxBigInt_Detach( z );
	z->sign = x->sign * sign;
	if( z->bufSize < x->size ) DaoxBigInt_Resize( z, x->size );
	z->size = x->size;
	memset( z->data, 0, z->size * sizeof(uchar_t) );
	while( y ){
		DaoxBigInt_MulAdd( z, x, y & LONG_MASK, m++ );
		y = y >> LONG_BITS;
	}
}
void DaoxBigInt_BitAND( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y )
{
	daoint i, min = x->size < y->size ? x->size : y->size;
	uchar_t *dx, *dy, *dz;
	DaoxBigInt_Resize( z, min );
	dx = x->data; dy = y->data; dz = z->data;
	for(i=0; i<min; i++) dz[i] = dx[i] & dy[i];
}
void DaoxBigInt_BitOR( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y )
{
	daoint i, max = x->size > y->size ? x->size : y->size;
	uchar_t *dx, *dy, *dz;
	DaoxBigInt_Resize( z, max );
	dx = x->data; dy = y->data; dz = z->data;
	if( max == x->size ){
		for(i=0; i<y->size; i++) dz[i] = dx[i] | dy[i];
		for(i=y->size; i<max; i++) dz[i] = dx[i];
	}else{
		for(i=0; i<x->size; i++) dz[i] = dx[i] | dy[i];
		for(i=x->size; i<max; i++) dz[i] = dy[i];
	}
}
void DaoxBigInt_BitXOR( DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y )
{
	daoint i, max = x->size > y->size ? x->size : y->size;
	uchar_t *dx, *dy, *dz;
	DaoxBigInt_Resize( z, max );
	dx = x->data; dy = y->data; dz = z->data;
	if( max == x->size ){
		for(i=0; i<y->size; i++) dz[i] = dx[i] ^ dy[i];
		for(i=y->size; i<max; i++) dz[i] = dx[i];
	}else{
		for(i=0; i<x->size; i++) dz[i] = dx[i] ^ dy[i];
		for(i=x->size; i<max; i++) dz[i] = dy[i];
	}
}
void DaoxBigInt_ShiftLeft( DaoxBigInt *z, int bits )
{
	uchar_t *dz;
	daoint i, k, nz = z->size;
	k = bits / LONG_BITS;
	DaoxBigInt_Detach( z );
	dz = z->data;
	if( k && z->offset >= k ){
		z->offset -= k;
		z->data -= k;
		z->size += k;
		memset( z->data, 0, k * sizeof(uchar_t) );
	}else if( k ){
		DaoxBigInt_Resize( z, nz + k + 1 );
		memmove( z->data + k, z->data, nz * sizeof(uchar_t) );
		memset( z->data, 0, k * sizeof(uchar_t) );
		z->size --;
		dz = z->data;
		nz = z->size;
	}
	k = bits % LONG_BITS;
	if( k == 0 ) return;
	i = dz[nz-1] >> (LONG_BITS-k);
	if( i ) DaoxBigInt_Append( z, i );
	dz = z->data;
	for(i=nz-1; i>0; i--) dz[i] = ((dz[i]<<k) | dz[i-1]>>(LONG_BITS-k)) & LONG_MASK;
	dz[0] = (dz[0] << k) & LONG_MASK;
}
void DaoxBigInt_ShiftRight( DaoxBigInt *z, int bits )
{
	uchar_t *dz;
	daoint i, k, nz = z->size;
	if( bits >= nz * LONG_BITS ){
		DaoxBigInt_Clear( z );
		return;
	}
	DaoxBigInt_Detach( z );
	dz = z->data;
	k = bits / LONG_BITS;
	if( k && (k + z->offset) < z->size/10 ){
		z->offset += k;
		z->data += k;
	}else if( k ){
		memmove( z->data, z->data + k, (nz-k) * sizeof(uchar_t) );
		DaoxBigInt_Resize( z, nz-k );
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
void DaoxBigInt_Flip( DaoxBigInt *self )
{
	daoint i;
	DaoxBigInt_Detach( self );
	for(i=0; i<self->size; i++) self->data[i] = (LONG_BASE-1) ^ self->data[i];
}
void DaoxBigInt_Convert( DaoxBigInt *self, int base, DaoxBigInt *result, int tobase )
{
	double baselog = log(tobase)/log(2);
	daoint ispower2 = baselog == (daoint) baselog;
	daoint i, j, powers = 1, powerbase = tobase;
	daoint intbits = CHAR_BIT * sizeof(daoint);
	daoint n = 1 + self->size * (log(base) / log(tobase) + 1);
	DaoxBigInt *source;

	if( tobase == 256 ){
		DaoxBigInt_Copy( result, self );
		return;
	}
	DaoxBigInt_Reserve( result, n );
	if( ispower2 && tobase < 256 ){
		daoint tobits = (daoint) baselog;
		daoint tomask = ((uchar_t)(0xff << (8-tobits))) >> (8-tobits);
		daoint chunks = 8 / tobits;
		for(i=0; i<self->size; ++i){
			for(j=0; j<chunks; ++j){
				result->data[result->size++] = (self->data[i] >> (j*tobits)) & tomask;
			}
		}
		return;
	}

	source = DaoxBigInt_New();
	DaoxBigInt_Copy( source, self );
	while( ((powerbase * tobase) >> (intbits - LONG_BITS)) == 0 ){
		powerbase *= tobase;
		powers += 1;
	}
	while( source->size ){
		daoint remainder = 0;
		for(i=source->size-1; i>=0; --i){
			daoint div, num = source->data[i] | (remainder << LONG_BITS);
			source->data[i] = div = num / powerbase;
			remainder = num - div * powerbase;
		}
		while( source->size && source->data[ source->size - 1 ] == 0 ) source->size -= 1;
		j = powers;
		while( j-- ){
			result->data[result->size++] = remainder % tobase;
			remainder = remainder / tobase;
		}
	}
	DaoxBigInt_Delete( source );
}
static void DaoxBigInt_PrintBits( DaoxBigInt *self, DString *s )
{
	DString *s2 = s;
	daoint i = self->size;
	int j;
	if( s == NULL ) s = DString_New(1);
	DString_SetChars( s, "0" );
	if( i ==0 ) goto Finish;
	DString_Clear( s );

	for(i=self->size; i>0; i--){
		int digit = self->data[i-1];
		for(j=LONG_BITS-1; j>=0; j--){
			DString_AppendChar( s, '0' + ((digit & (1<<j))>>j) );
		}
	}
Finish:
	DString_AppendChars( s, "L2" );
	if( s2 == NULL ){
		printf( "%s\n", s->chars );
		DString_Delete(s);
	}
}
void DaoxBigInt_Print( DaoxBigInt *self, DString *s )
{
	const char *digits = "0123456789abcdef";
	DaoxBigInt *based;
	DString *s2 = s;
	daoint i = self->size;
	if( self->base == 2 ){
		DaoxBigInt_PrintBits( self, s );
		return;
	}
	while(i >0 && self->data[i-1] ==0 ) i--;
	if( s == NULL ) s = DString_New(1);
	DString_SetChars( s, "0L" );
	if( i ==0 ) goto Finish;
	DString_Clear( s );
	if( self->sign <0 ) DString_AppendChar( s, '-' );

	based = DaoxBigInt_New();
	DaoxBigInt_Convert( self, LONG_BASE, based, self->base );
	i = based->size;
	while(i >0 && based->data[i-1] ==0 ) i--;
	for(; i>=1; i--) DString_AppendChar( s, digits[based->data[i-1]] );
	DString_AppendChar( s, 'L' );
	DaoxBigInt_Delete( based );
Finish:
	if( self->base != 10 ){
		char buf[20];
		sprintf( buf, "%i", self->base );
		DString_AppendChars( s, buf );
	}
	if( s2 == NULL ){
		printf( "%s\n", s->chars );
		DString_Delete(s);
	}
}
daoint DaoxBigInt_ToInteger( DaoxBigInt *self )
{
	daoint i, k=1, n = (sizeof(daoint)*8) / LONG_BITS;
	daoint res = 0;
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
double DaoxBigInt_ToDouble( DaoxBigInt *self )
{
	daoint i, n;
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
void DaoxBigInt_FromInteger( DaoxBigInt *self, daoint x )
{
	size_t y;
	if( x < 0 ){
		x = -x;
		self->sign = -1;
	}
	DaoxBigInt_Clear( self );
	y = x;
	while( y ){
		DaoxBigInt_Append( self, y & LONG_MASK );
		y = y >> LONG_BITS;
	}
}
void DaoxBigInt_FromDouble( DaoxBigInt *self, double value )
{
	double prod, frac;
	int expon, bit;

	DaoxBigInt_Clear( self );
	self->sign = value > 0 ? 1 : -1;

	value = fabs( value );
	frac = frexp( value, & expon );
	if( expon <=0 ) return;

	DaoxBigInt_Resize( self, expon / LONG_BITS + 1 );
	/* convert bit by bit */
	while( frac > 0 && expon ){
		expon -= 1;
		prod = frac * 2;
		bit = (int) prod;
		frac = prod - bit;
		self->data[ expon/LONG_BITS ] |= (bit<<(expon%LONG_BITS));
	}
	DaoxBigInt_Normalize( self );
}
char DaoxBigInt_FromString( DaoxBigInt *self, DString *s )
{
	DaoxBigInt *tmp;
	char table[256];
	char *mbs;
	daoint i, j, n, pl;
	int base = 10;

	DaoxBigInt_Clear( self );
	self->base = 10;
	n = s->size;
	mbs = s->chars;
	if( n == 0 ) return 0;
	memset( table, 0, 256 );
	for(i='0'; i<='9'; i++) table[i] = i - ('0'-1);
	for(i='a'; i<='f'; i++) table[i] = i - ('a'-1) + 10;
	for(i='A'; i<='F'; i++) table[i] = i - ('A'-1) + 10;
	pl = DString_FindChar(s, 'L', 0);
	if( pl != DAO_NULLPOS ){
		/* if( mbs[pl+1] == '0' ) return 0; */
		if( (pl+3) < n ) return 'L';
		if( (pl+1) < n && ! isdigit( mbs[pl+1] ) ) return 'L';
		if( (pl+2) < n && ! isdigit( mbs[pl+2] ) ) return 'L';
		if( (pl+1) < n ) self->base = base = strtol( mbs+pl+1, NULL, 0 );
		if( base <= 1 || base > 16 ) return 'L';
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
	DaoxBigInt_Resize( self, n );
	for(i=n, j=0; i>0; i--, j++){
		uchar_t digit = table[ (uchar_t)mbs[i-1] ];
		if( digit ==0 ) return mbs[i-1];
		self->data[j] = digit - 1;
		if( self->data[j] >= base ){
			self->data[j] = 0;
			self->size = j;
			return mbs[i-1];
		}
	}
	tmp = DaoxBigInt_New();
	DaoxBigInt_Copy( tmp, self );
	DaoxBigInt_Convert( tmp, base, self, LONG_BASE );
	DaoxBigInt_Delete( tmp );
	return 0;
}
void DaoxBigInt_FromValue( DaoxBigInt *self, DaoValue *value )
{
	DaoxBigInt *other;
	switch( value->type ){
	case DAO_CSTRUCT :
		other = (DaoxBigInt*) DaoValue_CastCstruct( value, daox_type_bigint );
		if( other == NULL ) break;
		DaoxBigInt_Move( self, other );
		break;
	case DAO_INTEGER : DaoxBigInt_FromInteger( self, value->xInteger.value ); break;
	case DAO_FLOAT   : DaoxBigInt_FromDouble ( self, value->xFloat.value   ); break;
	case DAO_DOUBLE  : DaoxBigInt_FromDouble ( self, value->xDouble.value  ); break;
	case DAO_STRING  : DaoxBigInt_FromString ( self, value->xString.value   ); break;
	}
}

const int base_bits[] = {0,0,1,0,2,0,0,0,3,0,0,0,0,0,0,0,4};
const int base_masks[] = {0,0,1,0,3,0,0,0,7,0,0,0,0,0,0,0,15};
static void DaoLong_GetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid )
{
	DaoxBigInt *self = (DaoxBigInt*) DaoValue_CastCstruct( self0, daox_type_bigint );
	daoint id = DaoValue_GetInteger( pid );
	daoint n = self->size;
	int w = base_bits[self->base];
	int digit = 0;
	if( w == 0 ){
		DaoProcess_RaiseError( proc, "Index", "need power 2 radix" );
		return;
	}
	if( id <0 || id*w >= n*LONG_BITS ){
		DaoProcess_RaiseError( proc, "Index", "out of range" );
		return;
	}
	if( self->base == 2 ){
		digit = (self->data[id/LONG_BITS] & (1<<(id%LONG_BITS)))>>(id%LONG_BITS);
	}else{
		int m = id*w / LONG_BITS;
		if( ((id+1)*w <= (m+1)*LONG_BITS) || m+1 >= n ){
			int digit2 = self->data[m] >> (id*w - m*LONG_BITS);
			digit = digit2 & base_masks[self->base];
			if( m+1 >= n && digit2 ==0 )
				DaoProcess_RaiseError( proc, "Index", "out of range" );
		}else{
			int d = (self->data[m+1]<<LONG_BITS) | self->data[m];
			digit = (d>>(id*w - m*LONG_BITS)) & base_masks[self->base];
		}
	}
	DaoProcess_PutInteger( proc, digit );
}
static void DaoLong_SetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid, DaoValue *value )
{
	DaoxBigInt *self = (DaoxBigInt*) DaoValue_CastCstruct( self0, daox_type_bigint );
	daoint id = DaoValue_GetInteger( pid );
	daoint digit = DaoValue_GetInteger( value );
	daoint i, n = self->size;
	int w = base_bits[self->base];
	DaoxBigInt_Detach( self );
	if( w == 0 ){
		DaoProcess_RaiseError( proc, "Index", "need power 2 radix" );
		return;
	}
	if( pid->type && (id <0 || id*w >= n*LONG_BITS) ){
		DaoProcess_RaiseError( proc, "Index", "out of range" );
		return;
	}else if( digit <0 || digit >= self->base ){
		DaoProcess_RaiseError( proc, NULL, "digit value out of range" );
		return;
	}
	if( pid->type == 0 ){
		uchar_t bits = digit;
		if( self->base == 2 ) bits = digit ? LONG_BASE-1 : 0;
		for(i=0; i<self->size; i++) self->data[i] = bits;
	}else{
		if( self->base == 2 ){
			if( digit ){
				self->data[id/LONG_BITS] |= (1<<(id%LONG_BITS));
			}else{
				self->data[id/LONG_BITS] &= ~(1<<(id%LONG_BITS));
			}
		}else{
			int m = id*w / LONG_BITS;
			int shift = id*w - m*LONG_BITS;
			if( ((id+1)*w <= (m+1)*LONG_BITS) || m+1 >= n ){
				int digit2 = self->data[m] >> shift;
				self->data[m] &= ~(base_masks[self->base]<<shift);
				self->data[m] |= digit<<shift;
				if( m+1 >= n && digit2 ==0 )
					DaoProcess_RaiseError( proc, "Index", "out of range" );
			}else{
				int d = (self->data[m+1]<<LONG_BITS) | self->data[m];
				d &= ~(base_masks[self->base]<<shift);
				d |= digit<<shift;
				self->data[m] = d & LONG_MASK;
				self->data[m+1] = d >> LONG_BITS;
			}
		}
	}
}
static void DaoLong_GetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N )
{
	switch( N ){
	case 0 : DaoLong_GetItem1( self, proc, dao_none_value ); break;
	case 1 : DaoLong_GetItem1( self, proc, ids[0] ); break;
	default : DaoProcess_RaiseError( proc, "Index", "not supported" );
	}
}
static void DaoLong_SetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	switch( N ){
	case 0 : DaoLong_SetItem1( self, proc, dao_none_value, value ); break;
	case 1 : DaoLong_SetItem1( self, proc, ids[0], value ); break;
	default : DaoProcess_RaiseError( proc, "Index", "not supported" );
	}
}
static DaoTypeCore bigintCore=
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoLong_GetItem,
	DaoLong_SetItem,
	DaoValue_Print
};
static void BIGINT_New1( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxBigInt *self = DaoxBigInt_New();
	DaoProcess_PutValue( proc, (DaoValue*) self );
	DaoxBigInt_FromInteger( self, p[0]->xInteger.value );
	self->base = p[1]->xInteger.value;
}
static void BIGINT_New2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxBigInt *self = DaoxBigInt_New();
	DaoProcess_PutValue( proc, (DaoValue*) self );
	DaoxBigInt_FromString( self, p[0]->xString.value );
	self->base = p[1]->xInteger.value;
}
static void BIGINT_New3( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxBigInt *self = DaoxBigInt_New();
	DaoxBigInt *other = (DaoxBigInt*) DaoValue_CastCstruct( p[0], daox_type_bigint );
	DaoProcess_PutValue( proc, (DaoValue*) self );
	DaoxBigInt_Move( self, other );
	self->base = p[1]->xInteger.value;
}
static void BIGINT_GETI( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxBigInt *self = (DaoxBigInt*) DaoValue_CastCstruct( p[0], daox_type_bigint );
	DaoLong_GetItem1( p[0], proc, p[1] );
}
static void BIGINT_SETI( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxBigInt *self = (DaoxBigInt*) DaoValue_CastCstruct( p[0], daox_type_bigint );
	DaoLong_SetItem1( p[0], proc, p[1], p[2] );
}
static void DaoProcess_LongDiv ( DaoProcess *self, DaoxBigInt *z, DaoxBigInt *x, DaoxBigInt *y, DaoxBigInt *r )
{
	if( x->size ==0 || (x->size ==1 && x->data[0] ==0) ){
		DaoProcess_RaiseError( self, "Float::DivByZero", "" );
		return;
	}
	DaoxBigInt_Div( z, x, y, r );
}
static void BIGINT_BinaryOper1( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoxBigInt *C = DaoxBigInt_New();
	DaoxBigInt *A = (DaoxBigInt*) p[0];
	DaoxBigInt *B = DaoxBigInt_New();
	DaoxBigInt *B2 = DaoxBigInt_New();
	DaoxBigInt_FromValue( B, p[1] );
	DaoProcess_PutValue( proc, (DaoValue*) C );
	switch( oper ){
	case DVM_ADD : DaoxBigInt_Add( C, A, B ); break;
	case DVM_SUB : DaoxBigInt_Sub( C, A, B ); break;
	case DVM_MUL : DaoxBigInt_Mul( C, A, B, NULL ); break;
	case DVM_DIV : DaoProcess_LongDiv( proc, A, B, C, B2 ); break;
	case DVM_MOD : DaoProcess_LongDiv( proc, A, B, B2, C ); break;
	case DVM_POW : DaoxBigInt_Pow( C, A, DaoValue_GetInteger( p[1] ), NULL ); break;
	default: break;
	}
	C->base = A->base;
	DaoxBigInt_Delete( B );
	DaoxBigInt_Delete( B2 );
}
static void BIGINT_CompOper1( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoValue *C = NULL;
	DaoxBigInt *A = (DaoxBigInt*) p[0];
	daoint D = 0, B = DaoValue_GetInteger( p[1] );
	switch( oper ){
	case DVM_AND: C = DaoxBigInt_CompareToZero( A ) ? p[1] : p[0]; break;
	case DVM_OR:  C = DaoxBigInt_CompareToZero( A ) ? p[0] : p[1]; break;
	case DVM_LT:  D = DaoxBigInt_CompareToInteger( A, B )< 0; break;
	case DVM_LE:  D = DaoxBigInt_CompareToInteger( A, B )<=0; break;
	case DVM_EQ:  D = DaoxBigInt_CompareToInteger( A, B )==0; break;
	case DVM_NE:  D = DaoxBigInt_CompareToInteger( A, B )!=0; break;
	default: break;
	}
	if( C ) DaoProcess_PutValue( proc, C );
	else DaoProcess_PutInteger( proc, D );
}
static void BIGINT_ADD1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper1( proc, p, N, DVM_ADD );
}
static void BIGINT_SUB1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper1( proc, p, N, DVM_SUB );
}
static void BIGINT_MUL1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper1( proc, p, N, DVM_MUL );
}
static void BIGINT_DIV1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper1( proc, p, N, DVM_DIV );
}
static void BIGINT_MOD1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper1( proc, p, N, DVM_MOD );
}
static void BIGINT_POW1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper1( proc, p, N, DVM_POW );
}
static void BIGINT_AND1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_CompOper1( proc, p, N, DVM_AND );
}
static void BIGINT_OR1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_CompOper1( proc, p, N, DVM_OR );
}
static void BIGINT_LT1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_CompOper1( proc, p, N, DVM_LT );
}
static void BIGINT_LE1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_CompOper1( proc, p, N, DVM_LE );
}
static void BIGINT_EQ1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_CompOper1( proc, p, N, DVM_EQ );
}
static void BIGINT_NE1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_CompOper1( proc, p, N, DVM_NE );
}
static void BIGINT_BinaryOper2( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoxBigInt *C = DaoxBigInt_New();
	DaoxBigInt *A = (DaoxBigInt*) p[0];
	DaoxBigInt *B = (DaoxBigInt*) p[1];
	DaoxBigInt *B2 = DaoxBigInt_New();
	DaoProcess_PutValue( proc, (DaoValue*) C );
	switch( oper ){
	case DVM_ADD : DaoxBigInt_Add( C, A, B ); break;
	case DVM_SUB : DaoxBigInt_Sub( C, A, B ); break;
	case DVM_MUL : DaoxBigInt_Mul( C, A, B, NULL ); break;
	case DVM_DIV : DaoProcess_LongDiv( proc, A, B, C, B2 ); break;
	case DVM_MOD : DaoProcess_LongDiv( proc, A, B, B2, C ); break;
	case DVM_POW : DaoxBigInt_Pow( C, A, DaoxBigInt_ToInteger( B ), NULL ); break;
	default: break;
	}
	C->base = A->base;
	DaoxBigInt_Delete( B2 );
}
static void BIGINT_CompOper2( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoValue *C = NULL;
	DaoxBigInt *A = (DaoxBigInt*) p[0];
	DaoxBigInt *B = (DaoxBigInt*) p[1];
	daoint D = 0;
	switch( oper ){
	case DVM_AND: C = DaoxBigInt_CompareToZero( A ) ? p[1] : p[0]; break;
	case DVM_OR:  C = DaoxBigInt_CompareToZero( A ) ? p[0] : p[1]; break;
	case DVM_LT:  D = DaoxBigInt_Compare( A, B ) <  0; break;
	case DVM_LE:  D = DaoxBigInt_Compare( A, B ) <= 0; break;
	case DVM_EQ:  D = DaoxBigInt_Compare( A, B ) == 0; break;
	case DVM_NE:  D = DaoxBigInt_Compare( A, B ) != 0; break;
	default: break;
	}
	if( C ) DaoProcess_PutValue( proc, C );
	else DaoProcess_PutInteger( proc, D );
}
static void BIGINT_ADD2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper2( proc, p, N, DVM_ADD );
}
static void BIGINT_SUB2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper2( proc, p, N, DVM_SUB );
}
static void BIGINT_MUL2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper2( proc, p, N, DVM_MUL );
}
static void BIGINT_DIV2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper2( proc, p, N, DVM_DIV );
}
static void BIGINT_MOD2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper2( proc, p, N, DVM_MOD );
}
static void BIGINT_POW2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper2( proc, p, N, DVM_POW );
}
static void BIGINT_AND2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_CompOper2( proc, p, N, DVM_AND );
}
static void BIGINT_OR2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_CompOper2( proc, p, N, DVM_OR );
}
static void BIGINT_LT2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_CompOper2( proc, p, N, DVM_LT );
}
static void BIGINT_LE2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_CompOper2( proc, p, N, DVM_LE );
}
static void BIGINT_EQ2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_CompOper2( proc, p, N, DVM_EQ );
}
static void BIGINT_NE2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_CompOper2( proc, p, N, DVM_NE );
}
static void BIGINT_BinaryOper3( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoxBigInt *C = (DaoxBigInt*) p[0];
	DaoxBigInt *A = (DaoxBigInt*) p[1];
	DaoxBigInt *B = DaoxBigInt_New();
	DaoxBigInt *B2 = DaoxBigInt_New();
	DaoxBigInt_FromValue( B, p[2] );
	DaoProcess_PutValue( proc, (DaoValue*) C );
	switch( oper ){
	case DVM_ADD : DaoxBigInt_Add( C, A, B ); break;
	case DVM_SUB : DaoxBigInt_Sub( C, A, B ); break;
	case DVM_MUL : DaoxBigInt_Mul( C, A, B, NULL ); break;
	case DVM_DIV : DaoProcess_LongDiv( proc, A, B, C, B2 ); break;
	case DVM_MOD : DaoProcess_LongDiv( proc, A, B, B2, C ); break;
	case DVM_POW : DaoxBigInt_Pow( C, A, DaoValue_GetInteger( p[2] ), NULL ); break;
	default: break;
	}
	DaoxBigInt_Delete( B );
	DaoxBigInt_Delete( B2 );
}
static void BIGINT_ADD3( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper3( proc, p, N, DVM_ADD );
}
static void BIGINT_SUB3( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper3( proc, p, N, DVM_SUB );
}
static void BIGINT_MUL3( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper3( proc, p, N, DVM_MUL );
}
static void BIGINT_DIV3( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper3( proc, p, N, DVM_DIV );
}
static void BIGINT_MOD3( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper3( proc, p, N, DVM_MOD );
}
static void BIGINT_POW3( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper3( proc, p, N, DVM_POW );
}
static void BIGINT_BinaryOper4( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoxBigInt *C = (DaoxBigInt*) p[0];
	DaoxBigInt *A = (DaoxBigInt*) p[1];
	DaoxBigInt *B = (DaoxBigInt*) p[2];
	DaoxBigInt *B2 = DaoxBigInt_New();
	DaoProcess_PutValue( proc, (DaoValue*) C );
	switch( oper ){
	case DVM_ADD : DaoxBigInt_Add( C, A, B ); break;
	case DVM_SUB : DaoxBigInt_Sub( C, A, B ); break;
	case DVM_MUL : DaoxBigInt_Mul( C, A, B, NULL ); break;
	case DVM_DIV : DaoProcess_LongDiv( proc, A, B, C, B2 ); break;
	case DVM_MOD : DaoProcess_LongDiv( proc, A, B, B2, C ); break;
	case DVM_POW : DaoxBigInt_Pow( C, A, DaoxBigInt_ToInteger( B ), NULL ); break;
	default: break;
	}
	DaoxBigInt_Delete( B2 );
}
static void BIGINT_ADD4( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper4( proc, p, N, DVM_ADD );
}
static void BIGINT_SUB4( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper4( proc, p, N, DVM_SUB );
}
static void BIGINT_MUL4( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper4( proc, p, N, DVM_MUL );
}
static void BIGINT_DIV4( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper4( proc, p, N, DVM_DIV );
}
static void BIGINT_MOD4( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper4( proc, p, N, DVM_MOD );
}
static void BIGINT_POW4( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BinaryOper4( proc, p, N, DVM_POW );
}
static void BIGINT_UnaryOper( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	daoint ta;
	DaoxBigInt *A = (DaoxBigInt*) p[0];
	DaoxBigInt *C = DaoxBigInt_New();
	DaoProcess_PutValue( proc, (DaoValue*) C );
	switch( oper ){
	case DVM_NOT  :
		ta = DaoxBigInt_CompareToZero( A ) == 0;
		DaoxBigInt_FromInteger( C, ta );
		break;
	case DVM_MINUS :
		DaoxBigInt_Move( C, A );
		C->sign = - C->sign;
		break;
	case DVM_TILDE :
		DaoxBigInt_Move( C, A );
		DaoxBigInt_Flip( C );
		break;
	default: break;
	}
	C->base = A->base;
}
static void BIGINT_MINUS( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_UnaryOper( proc, p, N, DVM_MINUS );
}
static void BIGINT_NOT( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_UnaryOper( proc, p, N, DVM_NOT );
}
static void BIGINT_TILDE( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_UnaryOper( proc, p, N, DVM_TILDE );
}
static void BIGINT_BitOper1( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoxBigInt *A = (DaoxBigInt*) p[0];
	DaoxBigInt *C = DaoxBigInt_New();
	DaoProcess_PutValue( proc, (DaoValue*) C );
	switch( oper ){
	case DVM_BITAND :
		DaoxBigInt_FromValue( C, p[1] );
		DaoxBigInt_BitAND( C, A, C );
		break;
	case DVM_BITOR  :
		DaoxBigInt_FromValue( C, p[1] );
		DaoxBigInt_BitOR( C, A, C );
		break;
	case DVM_BITXOR :
		DaoxBigInt_FromValue( C, p[1] );
		DaoxBigInt_BitXOR( C, A, C );
		break;
	case DVM_BITLFT :
		DaoxBigInt_Move( C, A );
		DaoxBigInt_ShiftLeft( C, DaoValue_GetInteger( p[1] ) );
		break;
	case DVM_BITRIT :
		DaoxBigInt_Move( C, A );
		DaoxBigInt_ShiftRight( C, DaoValue_GetInteger( p[1] ) );
		break;
	default : break;
	}
	C->base = A->base;
}
static void BIGINT_BITAND1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper1( proc, p, N, DVM_BITAND );
}
static void BIGINT_BITOR1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper1( proc, p, N, DVM_BITOR );
}
static void BIGINT_BITXOR1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper1( proc, p, N, DVM_BITXOR );
}
static void BIGINT_BITLFT1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper1( proc, p, N, DVM_BITLFT );
}
static void BIGINT_BITRIT1( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper1( proc, p, N, DVM_BITRIT );
}
static void BIGINT_BitOper2( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoxBigInt *A = (DaoxBigInt*) p[0];
	DaoxBigInt *B = (DaoxBigInt*) p[1];
	DaoxBigInt *C = DaoxBigInt_New();
	DaoProcess_PutValue( proc, (DaoValue*) C );
	switch( oper ){
	case DVM_BITAND :
		DaoxBigInt_BitAND( C, A, B );
		break;
	case DVM_BITOR  :
		DaoxBigInt_BitOR( C, A, B );
		break;
	case DVM_BITXOR :
		DaoxBigInt_BitXOR( C, A, B );
		break;
	case DVM_BITLFT :
		DaoxBigInt_Move( C, A );
		DaoxBigInt_ShiftLeft( C, DaoxBigInt_ToInteger( B ) );
		break;
	case DVM_BITRIT :
		DaoxBigInt_Move( C, A );
		DaoxBigInt_ShiftRight( C, DaoxBigInt_ToInteger( B ) );
		break;
	default : break;
	}
	C->base = A->base;
}
static void BIGINT_BITAND2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper2( proc, p, N, DVM_BITAND );
}
static void BIGINT_BITOR2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper2( proc, p, N, DVM_BITOR );
}
static void BIGINT_BITXOR2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper2( proc, p, N, DVM_BITXOR );
}
static void BIGINT_BITLFT2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper2( proc, p, N, DVM_BITLFT );
}
static void BIGINT_BITRIT2( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper2( proc, p, N, DVM_BITRIT );
}
static void BIGINT_BitOper3( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoxBigInt *A = (DaoxBigInt*) p[1];
	DaoxBigInt *C = (DaoxBigInt*) p[0];
	DaoProcess_PutValue( proc, (DaoValue*) C );
	switch( oper ){
	case DVM_BITAND :
		DaoxBigInt_FromValue( C, p[2] );
		DaoxBigInt_BitAND( C, A, C );
		break;
	case DVM_BITOR  :
		DaoxBigInt_FromValue( C, p[2] );
		DaoxBigInt_BitOR( C, A, C );
		break;
	case DVM_BITXOR :
		DaoxBigInt_FromValue( C, p[2] );
		DaoxBigInt_BitXOR( C, A, C );
		break;
	case DVM_BITLFT :
		DaoxBigInt_Move( C, A );
		DaoxBigInt_ShiftLeft( C, DaoValue_GetInteger( p[2] ) );
		break;
	case DVM_BITRIT :
		DaoxBigInt_Move( C, A );
		DaoxBigInt_ShiftRight( C, DaoValue_GetInteger( p[2] ) );
		break;
	default : break;
	}
}
static void BIGINT_BITAND3( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper3( proc, p, N, DVM_BITAND );
}
static void BIGINT_BITOR3( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper3( proc, p, N, DVM_BITOR );
}
static void BIGINT_BITXOR3( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper3( proc, p, N, DVM_BITXOR );
}
static void BIGINT_BITLFT3( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper3( proc, p, N, DVM_BITLFT );
}
static void BIGINT_BITRIT3( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper3( proc, p, N, DVM_BITRIT );
}
static void BIGINT_BitOper4( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoxBigInt *A = (DaoxBigInt*) p[1];
	DaoxBigInt *B = (DaoxBigInt*) p[2];
	DaoxBigInt *C = (DaoxBigInt*) p[0];
	DaoProcess_PutValue( proc, (DaoValue*) C );
	switch( oper ){
	case DVM_BITAND :
		DaoxBigInt_BitAND( C, A, B );
		break;
	case DVM_BITOR  :
		DaoxBigInt_BitOR( C, A, B );
		break;
	case DVM_BITXOR :
		DaoxBigInt_BitXOR( C, A, B );
		break;
	case DVM_BITLFT :
		DaoxBigInt_Move( C, A );
		DaoxBigInt_ShiftLeft( C, DaoxBigInt_ToInteger( B ) );
		break;
	case DVM_BITRIT :
		DaoxBigInt_Move( C, A );
		DaoxBigInt_ShiftRight( C, DaoxBigInt_ToInteger( B ) );
		break;
	default : break;
	}
}
static void BIGINT_BITAND4( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper4( proc, p, N, DVM_BITAND );
}
static void BIGINT_BITOR4( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper4( proc, p, N, DVM_BITOR );
}
static void BIGINT_BITXOR4( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper4( proc, p, N, DVM_BITXOR );
}
static void BIGINT_BITLFT4( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper4( proc, p, N, DVM_BITLFT );
}
static void BIGINT_BITRIT4( DaoProcess *proc, DaoValue *p[], int N )
{
	BIGINT_BitOper4( proc, p, N, DVM_BITRIT );
}
static void BIGINT_CastToInt( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoxBigInt *self = (DaoxBigInt*) p[0];
	daoint *res = DaoProcess_PutInteger( proc, 0 );
	*res = DaoxBigInt_ToInteger( self );
}
static void BIGINT_CastToString( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoxBigInt *self = (DaoxBigInt*) p[0];
	DString *res = DaoProcess_PutChars( proc, "" );
	DaoxBigInt_Print( self, res );
}
static void BIGINT_PRINT( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoxBigInt *self = (DaoxBigInt*) p[0];
	DaoStream *stream = proc->stdioStream;
	DString *name = DString_New(1);
	if( stream == NULL ) stream = proc->vmSpace->stdioStream;
	DaoxBigInt_Print( self, name );
	DaoStream_WriteString( stream, name );
	DString_Delete( name );
}
static DaoFuncItem bigintMeths[]=
{
	{ BIGINT_New1, "BigInt( value :int, base = 10 ) => BigInt" },
	{ BIGINT_New2, "BigInt( value :string, base = 10 ) => BigInt" },
	{ BIGINT_New3, "BigInt( value :BigInt, base = 10 ) => BigInt" },

	{ BIGINT_GETI, "[]( self :BigInt, idx :none ) => BigInt" },
	{ BIGINT_GETI, "[]( self :BigInt, idx :int ) => int" },
	{ BIGINT_SETI, "[]=( self :BigInt, digit :int, idx :none )" },
	{ BIGINT_SETI, "[]=( self :BigInt, digit :int, idx :int )" },

	{ BIGINT_ADD1, "+( A :BigInt, B :int ) => BigInt" },
	{ BIGINT_SUB1, "-( A :BigInt, B :int ) => BigInt" },
	{ BIGINT_MUL1, "*( A :BigInt, B :int ) => BigInt" },
	{ BIGINT_DIV1, "/( A :BigInt, B :int ) => BigInt" },
	{ BIGINT_MOD1, "%( A :BigInt, B :int ) => BigInt" },
	{ BIGINT_POW1, "**( A :BigInt, B :int ) => BigInt" },

	{ BIGINT_AND1, "&&( A :BigInt, B :int ) => BigInt|int" },
	{ BIGINT_OR1,  "||( A :BigInt, B :int ) => BigInt|int" },
	{ BIGINT_LT1,  "< ( A :BigInt, B :int ) => int" },
	{ BIGINT_LE1,  "<=( A :BigInt, B :int ) => int" },
	{ BIGINT_EQ1,  "==( A :BigInt, B :int ) => int" },
	{ BIGINT_NE1,  "!=( A :BigInt, B :int ) => int" },

	{ BIGINT_ADD2, "+( A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_SUB2, "-( A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_MUL2, "*( A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_DIV2, "/( A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_MOD2, "%( A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_POW2, "**( A :BigInt, B :BigInt ) => BigInt" },

	{ BIGINT_AND2, "&&( A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_OR2,  "||( A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_LT2,  "< ( A :BigInt, B :BigInt ) => int" },
	{ BIGINT_LE2,  "<=( A :BigInt, B :BigInt ) => int" },
	{ BIGINT_EQ2,  "==( A :BigInt, B :BigInt ) => int" },
	{ BIGINT_NE2,  "!=( A :BigInt, B :BigInt ) => int" },

	{ BIGINT_ADD3, "+( C :BigInt, A :BigInt, B :int ) => BigInt" },
	{ BIGINT_SUB3, "-( C :BigInt, A :BigInt, B :int ) => BigInt" },
	{ BIGINT_MUL3, "*( C :BigInt, A :BigInt, B :int ) => BigInt" },
	{ BIGINT_DIV3, "/( C :BigInt, A :BigInt, B :int ) => BigInt" },
	{ BIGINT_MOD3, "%( C :BigInt, A :BigInt, B :int ) => BigInt" },
	{ BIGINT_POW3, "**( C :BigInt, A :BigInt, B :int ) => BigInt" },

	{ BIGINT_ADD4, "+( C :BigInt, A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_SUB4, "-( C :BigInt, A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_MUL4, "*( C :BigInt, A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_DIV4, "/( C :BigInt, A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_MOD4, "%( C :BigInt, A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_POW4, "**( C :BigInt, A :BigInt, B :BigInt ) => BigInt" },

	{ BIGINT_MINUS, "-( A :BigInt ) => BigInt" },
	{ BIGINT_NOT,   "!( A :BigInt ) => BigInt" },
	{ BIGINT_TILDE, "~( A :BigInt ) => BigInt" },

	{ BIGINT_BITAND1, "&( A :BigInt, B :int ) => BigInt" },
	{ BIGINT_BITOR1,  "|( A :BigInt, B :int ) => BigInt" },
	{ BIGINT_BITXOR1, "^( A :BigInt, B :int ) => BigInt" },
	{ BIGINT_BITLFT1, "<<( A :BigInt, B :int ) => BigInt" },
	{ BIGINT_BITRIT1, ">>( A :BigInt, B :int ) => BigInt" },

	{ BIGINT_BITAND2, "&( A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_BITOR2,  "|( A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_BITXOR2, "^( A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_BITLFT2, "<<( A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_BITRIT2, ">>( A :BigInt, B :BigInt ) => BigInt" },

	{ BIGINT_BITAND3, "&( C :BigInt, A :BigInt, B :int ) => BigInt" },
	{ BIGINT_BITOR3,  "|( C :BigInt, A :BigInt, B :int ) => BigInt" },
	{ BIGINT_BITXOR3, "^( C :BigInt, A :BigInt, B :int ) => BigInt" },
	{ BIGINT_BITLFT3, "<<( C :BigInt, A :BigInt, B :int ) => BigInt" },
	{ BIGINT_BITRIT3, ">>( C :BigInt, A :BigInt, B :int ) => BigInt" },

	{ BIGINT_BITAND4, "&( C :BigInt, A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_BITOR4,  "|( C :BigInt, A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_BITXOR4, "^( C :BigInt, A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_BITLFT4, "<<( C :BigInt, A :BigInt, B :BigInt ) => BigInt" },
	{ BIGINT_BITRIT4, ">>( C :BigInt, A :BigInt, B :BigInt ) => BigInt" },

	{ BIGINT_CastToInt,     "operator (int)( self :BigInt )" },
	{ BIGINT_CastToString,  "operator (string)( self :BigInt )" },

	{ BIGINT_PRINT,  "Print( self :BigInt )" },
	{ BIGINT_PRINT,  "__PRINT__( self :BigInt )" },

	{ NULL, NULL },
};
DaoTypeBase bigintTyper =
{
	"BigInt", NULL, NULL, (DaoFuncItem*) bigintMeths, {0}, {0},
	(FuncPtrDel)DaoxBigInt_Delete, NULL
};

DAO_DLL int DaoBigint_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
#ifdef DAO_WITH_THREAD
	DMutex_Init( & mutex_long_sharing ); /* TODO: destroy; */
#endif
	daox_type_bigint = DaoNamespace_WrapType( ns, & bigintTyper, 0 );
	return 0;
}
