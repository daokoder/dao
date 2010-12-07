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

#ifndef DAO_NUMERIC_H
#define DAO_NUMERIC_H

#include"daoType.h"

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

double abs_c( const complex16 com );
double arg_c( const complex16 com );
double norm_c( const complex16 com );
complex16 cos_c( const complex16 com );
complex16 cosh_c( const complex16 com );
complex16 exp_c( const complex16 com );
complex16 log_c( const complex16 com );
complex16 sin_c( const complex16 com );
complex16 sinh_c( const complex16 com );
complex16 sqrt_c( const complex16 com );
complex16 tan_c( const complex16 com );
complex16 tanh_c( const complex16 com );
complex16 ceil_c( const complex16 com );
complex16 floor_c( const complex16 com );

#ifdef __STRICT_ANSI__
#define LONG_BITS 7
#else
#define LONG_BITS 15
#endif

#define LONG_BASE (1<<LONG_BITS)
#define LONG_MASK ((int)(LONG_BASE-1))

struct DLong
{
	ushort_t *data;
	ushort_t *pbuf;
	short     sign;
	short     base;
	size_t    size;
	size_t    bufSize;
};
DLong* DLong_New();
void DLong_Delete( DLong *self );
void DLong_Clear( DLong *self );
void DLong_Resize( DLong *self, size_t size );
void DLong_PushBack( DLong *self, ushort_t it );
void DLong_PushFront( DLong *self, ushort_t it );
int DLong_UCompare( DLong *x, DLong *y );
int DLong_Compare( DLong *x, DLong *y );
void DLong_Move( DLong *z, DLong *x );
void DLong_Add( DLong *z, DLong *x, DLong *y );
void DLong_Sub( DLong *z, DLong *x, DLong *y );
void DLong_Mul( DLong *z, DLong *x, DLong *y );
void DLong_Div( DLong *z, DLong *x, DLong *y, DLong *r );
void DLong_Pow( DLong *z, DLong *x, dint n );
void DLong_AddInt( DLong *z, DLong *x, dint y, DLong *buf );
void DLong_MulInt( DLong *z, DLong *x, dint y );
void DLong_Flip( DLong *self );
void DLong_BitAND( DLong *z, DLong *x, DLong *y );
void DLong_BitOR( DLong *z, DLong *x, DLong *y );
void DLong_BitXOR( DLong *z, DLong *x, DLong *y );
void DLong_ShiftLeft( DLong *self, int bits );
void DLong_ShiftRight( DLong *self, int bits );
void DLong_Print( DLong *self, DString *s );
void DLong_FromInteger( DLong *self, dint x );
void DLong_FromDouble( DLong *self, double x );
char DLong_FromString( DLong *self, DString *s );
dint DLong_ToInteger( DLong *self );
double DLong_ToDouble( DLong *self );
int DLong_CompareToZero( DLong *self );
int DLong_CompareToInteger( DLong *self, dint x );
int DLong_CompareToDouble( DLong *self, double x );

ushort_t DLong_UDivDigit( DLong *z, ushort_t digit );

#define DLong_Append  DLong_PushBack

struct DaoArray
{
	DAO_DATA_COMMON;

	size_t  size;
	short   numType;
	short   owner;

	union{
		void       *p;
		int        *i;
		float      *f;
		double     *d;
		complex16  *c;
	} data;

	DArray  *dims;
	DArray  *dimAccum;

	size_t    subSize; /* size of the sub array / slice */
	DArray   *slice; /* array of indexes in each dimension */
	DaoArray *reference; /* reference array */

	DaoMap  *meta;
	DaoType *unitype;

	void  **matrix;
};
#ifdef DAO_WITH_NUMARRAY

DaoArray* DaoArray_New( int type );
DaoArray* DaoArray_Copy( DaoArray *self );
int DaoArray_CopyArray( DaoArray *self, DaoArray *other );
void DaoArray_Delete( DaoArray *self );

void DaoArray_ResizeVector( DaoArray *self, int size );
void DaoArray_ResizeArray( DaoArray *self, size_t *dims, int D );

int DaoArray_Sliced( DaoArray *self );
void DaoArray_UseData( DaoArray *self, void *data );

int    DaoArray_GetInteger( DaoArray *na, int i );
float  DaoArray_GetFloat( DaoArray *na, int i );
double DaoArray_GetDouble( DaoArray *na, int i );
complex16 DaoArray_GetComplex( DaoArray *na, int i );

#define ARRAY_GET_INT( x, i ) DaoArray_GetInteger( x, i )
#define ARRAY_GET_SF( x, i ) DaoArray_GetFloat( x, i )
#define ARRAY_GET_DF( x, i ) DaoArray_GetDouble( x, i )
#define ARRAY_GET_DC( x, i ) DaoArray_GetComplex( x, i )

#endif

#endif
