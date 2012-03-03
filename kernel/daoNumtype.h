/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#ifndef DAO_NUMERIC_H
#define DAO_NUMERIC_H

#include"daoStdtype.h"

#define BBITS    (sizeof(unsigned short) * 8)
#define BSIZE(x)  (((x) / 8) + sizeof(unsigned int))
#define BMASK(x)  (1 << ((x) % BBITS))
#define BTEST(p, x)  ((p)[(x) / BBITS] & BMASK(x))
#define BSET(p, x)  ((p)[(x) / BBITS] |= BMASK(x))
#define BCLEAR(p, x)  ((p)[(x) / BBITS] &= ~BMASK(x))
#define BFLIP(p, x)  (p)[(x) / BBITS] ^= BMASK(x)


#define \
	COM_ASSN( self, com ) \
{ (self).real = (com).real; (self).imag = (com).imag; }

#define \
	COM_ASSN2( self, com ) \
{ (self)->real = (com).real; (self)->imag = (com).imag; }

#define \
	COM_IP_ADD( self, com ) \
{ (self).real += (com).real; (self).imag += (com).imag; }

#define \
	COM_IP_SUB( self, com ) \
{ (self).real -= com.real; (self).imag -= com.imag; }

#define \
	COM_IP_MUL( self, com ) \
{ (self).real *= com.real; (self).imag *= com.imag; }

#define \
	COM_IP_DIV( self, com ) \
{ (self).real /= com.real; (self).imag /= com.imag; }

#define \
	COM_ADD( self, left, right ) \
{ (self).real = left.real + right.real; (self).imag = left.imag + right.imag; }

#define \
	COM_SUB( self, left, right ) \
{ (self).real = left.real - right.real; (self).imag = left.imag - right.imag; }

#define \
	COM_MUL( self, left, right ) \
{ (self).real = left.real*right.real - left.imag*right.imag; \
	(self).imag = left.real*right.imag + left.imag*right.real; }

#define \
	COM_DIV( self, L, R ) \
{ (self).real = ( L.real*R.real + L.imag*R.imag ) / ( R.real*R.real + R.imag*R.imag ); \
	(self).imag = ( L.imag*R.real - L.real*R.imag ) / ( R.real*R.real + R.imag*R.imag ); }

#define \
	COM_UNMS( self, com ) \
{ (self).real = - com.real; (self).imag = - com.imag;  }

DAO_DLL double abs_c( const complex16 com );
DAO_DLL double arg_c( const complex16 com );
DAO_DLL double norm_c( const complex16 com );
DAO_DLL complex16 cos_c( const complex16 com );
DAO_DLL complex16 cosh_c( const complex16 com );
DAO_DLL complex16 exp_c( const complex16 com );
DAO_DLL complex16 log_c( const complex16 com );
DAO_DLL complex16 sin_c( const complex16 com );
DAO_DLL complex16 sinh_c( const complex16 com );
DAO_DLL complex16 sqrt_c( const complex16 com );
DAO_DLL complex16 tan_c( const complex16 com );
DAO_DLL complex16 tanh_c( const complex16 com );
DAO_DLL complex16 ceil_c( const complex16 com );
DAO_DLL complex16 floor_c( const complex16 com );

#define LONG_BITS 8
#define LONG_BASE 256
#define LONG_MASK 255

typedef signed char schar_t;

/* bit integer */
struct DLong
{
	uchar_t  *data;
	uchar_t   base;
	schar_t   sign;
	ushort_t  offset;
	daoint    size;
	daoint    bufSize;
};

DAO_DLL DLong* DLong_New();
DAO_DLL void DLong_Init( DLong *self );
DAO_DLL void DLong_Delete( DLong *self );
DAO_DLL void DLong_Detach( DLong *self );
DAO_DLL void DLong_Clear( DLong *self );

#ifdef DAO_WITH_LONGINT
DAO_DLL void DLong_Resize( DLong *self, daoint size );
DAO_DLL void DLong_PushBack( DLong *self, uchar_t it );
DAO_DLL void DLong_PushFront( DLong *self, uchar_t it );
DAO_DLL int DLong_Compare( DLong *x, DLong *y );
DAO_DLL void DLong_Move( DLong *z, DLong *x );
DAO_DLL void DLong_Add( DLong *z, DLong *x, DLong *y );
DAO_DLL void DLong_Sub( DLong *z, DLong *x, DLong *y );
DAO_DLL void DLong_Mul( DLong *z, DLong *x, DLong *y );
DAO_DLL void DLong_Div( DLong *z, DLong *x, DLong *y, DLong *r );
DAO_DLL void DLong_Pow( DLong *z, DLong *x, daoint n );
DAO_DLL void DLong_AddInt( DLong *z, DLong *x, daoint y, DLong *buf );
DAO_DLL void DLong_MulInt( DLong *z, DLong *x, daoint y );
DAO_DLL void DLong_Flip( DLong *self );
DAO_DLL void DLong_BitAND( DLong *z, DLong *x, DLong *y );
DAO_DLL void DLong_BitOR( DLong *z, DLong *x, DLong *y );
DAO_DLL void DLong_BitXOR( DLong *z, DLong *x, DLong *y );
DAO_DLL void DLong_ShiftLeft( DLong *self, int bits );
DAO_DLL void DLong_ShiftRight( DLong *self, int bits );
DAO_DLL void DLong_Print( DLong *self, DString *s );
DAO_DLL void DLong_FromInteger( DLong *self, daoint x );
DAO_DLL void DLong_FromDouble( DLong *self, double x );
DAO_DLL char DLong_FromString( DLong *self, DString *s );
DAO_DLL daoint DLong_ToInteger( DLong *self );
DAO_DLL double DLong_ToDouble( DLong *self );
DAO_DLL int DLong_CompareToZero( DLong *self );
DAO_DLL int DLong_CompareToInteger( DLong *self, daoint x );
DAO_DLL int DLong_CompareToDouble( DLong *self, double x );
#define DLong_Append  DLong_PushBack
#endif



/* Multi-dimensional array stored in row major order: */
struct DaoArray
{
	DAO_DATA_COMMON;

	uchar_t  etype; /* element type; */
	uchar_t  owner; /* own the data; */
	short    ndim; /* number of dimensions; */
	daoint   size; /* total number of elements; */
	daoint  *dims; /* for i=0,...,ndim-1: dims[i], size of the i-th dimension; */
	/* dims[ndim+i], products of the sizes of the remaining dimensions after the i-th; */

	union{
		void       *p;
		daoint     *i;
		float      *f;
		double     *d;
		complex16  *c;
	} data;

	DaoArray *original; /* original array */
	DArray   *slices; /* list of slicing in each dimension */

};
#ifdef DAO_WITH_NUMARRAY

DAO_DLL DaoArray* DaoArray_New( int type );
DAO_DLL DaoArray* DaoArray_Copy( DaoArray *self );
DAO_DLL int DaoArray_CopyArray( DaoArray *self, DaoArray *other );
DAO_DLL void DaoArray_Delete( DaoArray *self );

DAO_DLL void DaoArray_SetDimCount( DaoArray *self, int D );
DAO_DLL void DaoArray_FinalizeDimData( DaoArray *self );

DAO_DLL void DaoArray_ResizeVector( DaoArray *self, daoint size );
DAO_DLL void DaoArray_ResizeArray( DaoArray *self, daoint *dims, int D );

DAO_DLL int DaoArray_Sliced( DaoArray *self );
DAO_DLL void DaoArray_UseData( DaoArray *self, void *data );

DAO_DLL daoint DaoArray_GetInteger( DaoArray *na, daoint i );
DAO_DLL float  DaoArray_GetFloat( DaoArray *na, daoint i );
DAO_DLL double DaoArray_GetDouble( DaoArray *na, daoint i );
DAO_DLL complex16 DaoArray_GetComplex( DaoArray *na, daoint i );

#endif

#endif
