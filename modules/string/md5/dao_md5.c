/*=========================================================================================
  This file is a part of the Dao standard modules.
  Copyright (C) 2011-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<math.h>
#include"daoString.h"
#include"daoValue.h"


static void MD5_Append( DString *md5, uint32_t h )
{
	const char *hex = "0123456789abcdef";
	uint32_t k;
	k = (h >> 0) & 0xff;  DString_AppendChar( md5, hex[k>>4] );  DString_AppendChar( md5, hex[k&0xf] );
	k = (h >> 8) & 0xff;  DString_AppendChar( md5, hex[k>>4] );  DString_AppendChar( md5, hex[k&0xf] );
	k = (h >>16) & 0xff;  DString_AppendChar( md5, hex[k>>4] );  DString_AppendChar( md5, hex[k&0xf] );
	k = (h >>24) & 0xff;  DString_AppendChar( md5, hex[k>>4] );  DString_AppendChar( md5, hex[k&0xf] );
}
static void MD5_Update( uint32_t H[4], uint32_t W[16], uint32_t K[64] )
{
	static uint32_t R[64] = {
		7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
		5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
		4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
		6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
	};
	uint32_t A = H[0];
	uint32_t B = H[1];
	uint32_t C = H[2];
	uint32_t D = H[3];
	uint32_t k;
	for(k=0; k<16; k++){
		uint32_t f = (B & C) | ((~B) & D);
		uint32_t g = k;
		uint32_t t = D;
		uint32_t x = A + f + K[k] + W[g];
		D = C;
		C = B;
		B = B + ((x << R[k]) | (x >> (32-R[k])));
		A = t;
	}
	for(k=16; k<32; k++){
		uint32_t f = (D & B) | ((~D) & C);
		uint32_t g = (k*5 + 1) % 16;
		uint32_t t = D;
		uint32_t x = A + f + K[k] + W[g];
		D = C;
		C = B;
		B = B + ((x << R[k]) | (x >> (32-R[k])));
		A = t;
	}
	for(k=32; k<48; k++){
		uint32_t f = B ^ C ^ D;
		uint32_t g = (k*3 + 5) % 16;
		uint32_t t = D;
		uint32_t x = A + f + K[k] + W[g];
		D = C;
		C = B;
		B = B + ((x << R[k]) | (x >> (32-R[k])));
		A = t;
	}
	for(k=48; k<64; k++){
		uint32_t f = C ^ (B | (~D));
		uint32_t g = (k*7) % 16;
		uint32_t t = D;
		uint32_t x = A + f + K[k] + W[g];
		D = C;
		C = B;
		B = B + ((x << R[k]) | (x >> (32-R[k])));
		A = t;
	}
	H[0] += A;
	H[1] += B;
	H[2] += C;
	H[3] += D;
}

void DString_MD5( DString *self, DString *md5 )
{
	DString *padding = md5;
	uint64_t i, k, m, n, twop32 = ((uint64_t)1)<<32;
	uint32_t H[4] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 };
	uint32_t K[64], W[16];
	int32_t size = self->size;
	int32_t chunks = self->size / 64;
	uint8_t *data = (uint8_t*) self->mbs;

	if( self->wcs ){
		data = (uint8_t*) self->wcs;
		size *= sizeof(wchar_t);
		chunks = size / 64;
	}

	for(i=0; i<64; i++) K[i] = (uint32_t) floor( fabs( sin(i+1) ) * twop32 );
	for(i=0; i<chunks; i++){
		for(k=0; k<16; k++){
			uint32_t b = i*64 + k*4;
			uint32_t m = data[b];
			m |= ((uint32_t)data[b+1])<<8;
			m |= ((uint32_t)data[b+2])<<16;
			m |= ((uint32_t)data[b+3])<<24;
			W[k] = m;
		}
		MD5_Update( H, W, K );
	}
	DString_ToMBS( padding );
	DString_Reserve( padding, 128 );
	padding->size = 64;
	data = (uint8_t*) padding->mbs;
	m = size - chunks*64;
	if( m ) memcpy( padding->mbs, data + chunks*64, m*sizeof(char) );
	if( m + 8 > 64 ) padding->size = 128;
	chunks = padding->size / 64;

	data[m] = 1<<7; // first bit 1 followed by bit 0s;
	for(i=m+1; i<padding->size-8; i++) data[i] = 0;
	n = size * 8;
	// last 64 bits to store the string size in little endian:
	data[i] = n & 0xff;
	data[i+1] = (n >> 8) & 0xff;
	data[i+2] = (n >> 16) & 0xff;
	data[i+3] = (n >> 24) & 0xff;
	data[i+4] = (n >> 32) & 0xff;
	data[i+5] = (n >> 40) & 0xff;
	data[i+6] = (n >> 48) & 0xff;
	data[i+7] = (n >> 56) & 0xff;
	for(i=0; i<chunks; i++){
		for(k=0; k<16; k++){
			uint32_t b = i*64 + k*4;
			uint32_t m = data[b];
			m |= ((uint32_t)data[b+1])<<8;
			m |= ((uint32_t)data[b+2])<<16;
			m |= ((uint32_t)data[b+3])<<24;
			W[k] = m;
		}
		MD5_Update( H, W, K );
	}
	md5->size = 0;
	MD5_Append( md5, H[0] );
	MD5_Append( md5, H[1] );
	MD5_Append( md5, H[2] );
	MD5_Append( md5, H[3] );
}

static void DaoSTR_MD5( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DString *md5 = DaoProcess_PutMBString( proc, "" );
	DString_MD5( self, md5 );
}
int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace_WrapFunction( ns, DaoSTR_MD5, "md5( self :string )=>string" );
	return 0;
}
