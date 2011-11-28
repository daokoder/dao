/*=========================================================================================
  This file is a part of the Dao standard modules.
  Copyright (C) 2011, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#include<stdlib.h>
#include<string.h>
#include"dao_xxtea.h"
#include"daoString.h"
#include"daoValue.h"

DAO_INIT_MODULE

/* Corrected Block Tiny Encryption Algorithm (Corrected Block TEA, or XXTEA)
 * by David Wheeler and Roger Needham
 */
#define MX  ( (((z>>5)^(y<<2))+((y>>3)^(z<<4)))^((sum^y)+(k[(p&3)^e]^z)) )

int btea(int* v, int n, int *k)
{
	unsigned int z, y=v[0], sum=0, e, DELTA=0x9e3779b9;
	int p, q ;
	if (n > 1) { /* Coding Part */
		z=v[n-1];           
		q = 6+52/n ;
		while (q-- > 0) {
			sum += DELTA;
			e = (sum>>2) & 3 ;
			for (p=0; p<n-1; p++) y = v[p+1], z = v[p] += MX;
			y = v[0];
			z = v[n-1] += MX;
		}
		return 0 ; 
	} else if (n < -1) {  /* Decoding Part */
		n = -n ;
		q = 6+52/n ;
		sum = q*DELTA ;
		while (sum != 0) {
			e = (sum>>2) & 3;
			for (p=n-1; p>0; p--) z = v[p-1], y = v[p] -= MX;
			z = v[n-1];
			y = v[0] -= MX;
			sum -= DELTA;
		}
		return 0;
	}
	return 1;
}

const char *dec2hex = "0123456789abcdef";

static int HexDigit( char d )
{
	d = d | 0x20;
	if( ( (size_t)(d-'0') ) < 10 ) return d - '0';
	else if( ( (size_t)(d-'a') ) < 26 ) return d + 10 - 'a';
	return -1;
}
static int STR_Cipher( DString *self, DString *key, int hex, int flag )
{
	unsigned char ks[16];
	unsigned char *data = NULL;
	size_t size = 0;
	int i;
	DString_Detach( self );
	DString_ToMBS( self );
	if( self->size == 0 ) return 0;
	DString_ToMBS( key );
	if( key->size >= 32 ){
		for(i=0; i<16; i++){
			signed char c1 = HexDigit( key->mbs[2*i] );
			signed char c2 = HexDigit( key->mbs[2*i+1] );
			if( c1 <0 || c2 <0 ) return 1;
			ks[i] = 16 * c1 + c2;
		}
	}else if( key->size >= 16 ){
		memcpy( ks, key->mbs, 16 );
	}else{
		return 1;
	}
	if( flag == 1 ){
		size = self->size;
		i = size % 4;
		if( i ) i = 4 - i;
		DString_Resize( self, size + 4 + i );
		memmove( self->mbs + 4, self->mbs, size );
		*(int*) self->mbs = size;
		data = (unsigned char*) self->mbs;
		btea( (int*)self->mbs, self->size / 4, (int*) ks );
		if( hex ){
			size = self->size;
			DString_Resize( self, 2 * size );
			data = (unsigned char*) self->mbs;
			for(i=size-1; i>=0; i--){
				self->mbs[2*i+1] = dec2hex[ data[i] % 16 ];
				self->mbs[2*i] = dec2hex[ data[i] / 16 ];
			}
		}
	}else{
		if( hex ){
			if( self->size % 2 ) return 1;
			data = (unsigned char*) self->mbs;
			size = self->size / 2;
			for(i=0; i<size; i++){
				char c1 = HexDigit( data[2*i] );
				char c2 = HexDigit( data[2*i+1] );
				if( c1 <0 || c2 <0 ) return 1;
				data[i] = 16 * c1 + c2;
			}
			DString_Resize( self, size );
		}
		btea( (int*)self->mbs, - (int)(self->size / 4), (int*) ks );
		size = *(int*) self->mbs;
		DString_Erase( self, 0, 4 );
		self->size = size;
	}
	return 0;
}
int DString_Encrypt( DString *self, DString *key, int hex )
{
	return STR_Cipher( self, key, hex, 1 );
}
int DString_Decrypt( DString *self, DString *key, int hex )
{
	return STR_Cipher( self, key, hex, 0 );
}

static const char *errmsg[2] =
{
	"invalid key",
	"invalid source"
};
static void DaoSTR_Encrypt( DaoProcess *proc, DaoValue *p[], int N )
{
	int rc = DString_Encrypt( p[0]->xString.data, p[1]->xString.data, p[2]->xEnum.value );
	if( rc ) DaoProcess_RaiseException( proc, DAO_ERROR, errmsg[rc-1] );
	DaoProcess_PutReference( proc, p[0] );
}
static void DaoSTR_Decrypt( DaoProcess *proc, DaoValue *p[], int N )
{
	int rc = DString_Decrypt( p[0]->xString.data, p[1]->xString.data, p[2]->xEnum.value );
	if( rc ) DaoProcess_RaiseException( proc, DAO_ERROR, errmsg[rc-1] );
	DaoProcess_PutReference( proc, p[0] );
}

const char *enc = "encrypt( self :string, key :string, format :enum<regular,hex> = $regular )=>string";
const char *dec = "decrypt( self :string, key :string, format :enum<regular,hex> = $regular )=>string";

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace_WrapFunction( ns, DaoSTR_Encrypt, enc );
	DaoNamespace_WrapFunction( ns, DaoSTR_Decrypt, dec );
	return 0;
}
