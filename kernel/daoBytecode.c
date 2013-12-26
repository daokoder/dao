/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2013, Limin Fu
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

#include <math.h>
#include <string.h>

#include "daoBytecode.h"
#include "daoOptimizer.h"
#include "daoNamespace.h"
#include "daoVmspace.h"
#include "daoValue.h"
#include "daoGC.h"


static const char* const dao_asm_names[] =
{
	"ASM_NONE"      ,
	"ASM_ROUTINE"   ,
	"ASM_CLASS"     ,
	"ASM_INTERFACE" ,
	"ASM_ENUM"      ,
	"ASM_TYPE"      ,
	"ASM_VALUE"     ,
	"ASM_EVAL"      ,
	"ASM_CONSTS"    ,
	"ASM_TYPES"     ,
	"ASM_CODE"      ,
	"ASM_END"       ,
	"ASM_LOAD"      ,
	"ASM_USE"       ,
	"ASM_CONST"     ,
	"ASM_STATIC"    ,
	"ASM_GLOBAL"    ,
	"ASM_VAR"       ,
	"ASM_MIXIN"     ,
	"ASM_DECOPAT"   ,
	"ASM_DATA"      ,
	"ASM_DATA2"     ,
	"ASM_SEEK"
};




void DaoByteCoder_EncodeUInt16( uchar_t *data, uint_t value )
{
	data[0] = (value >> 8) & 0xFF;
	data[1] = value & 0xFF;
}
void DaoByteCoder_EncodeUInt32( uchar_t *data, uint_t value )
{
	data[0] = (value >> 24) & 0xFF;
	data[1] = (value >> 16) & 0xFF;
	data[2] = (value >>  8) & 0xFF;
	data[3] = value & 0xFF;
}
void DaoByteCoder_EncodeDaoInt( uchar_t *data, daoint value )
{
	uchar_t i, m = sizeof(daoint);
	for(i=0; i<m; ++i) data[i] = (value >> 8*(m-1-i)) & 0xFF;
}
/*
// IEEE 754 double-precision binary floating-point format:
//   sign(1)--exponent(11)------------fraction(52)---------------------
//   S EEEEEEEEEEE FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
//   63         52                                                    0
//
//   value = (-1)^S  *  ( 1 + \sigma_0^51 (b_i * 2^{-(52-i)}) )  *  2^{E-1023}
//
// Exponents 0x000 is used to represent zero (if F=0) and subnormals (if F!=0);
// Exponents 0x7FF is used to represent inf (if F=0) and NaNs (if F!=0);
// Where F is the fraction mantissa.
*/
static void DaoByteCoder_EncodeNAN( uchar_t *data )
{
	DaoByteCoder_EncodeUInt32( data, 0x7FF << 20 );
	DaoByteCoder_EncodeUInt32( data + 4, 1 );
}
static void DaoByteCoder_EncodeInf( uchar_t *data )
{
	DaoByteCoder_EncodeUInt32( data, 0x7FF << 20 );
	DaoByteCoder_EncodeUInt32( data + 4, 0 );
}
static void DaoByteCoder_EncodeDouble( uchar_t *data, double value )
{
	uint_t i = 20, m1 = 0, m2 = 0;
	int first = 1;
	int neg = value < 0.0;
	int expon = 0;
	double frac;

	if( value == 0.0 ){
		DaoByteCoder_EncodeUInt32( data, 0 );
		DaoByteCoder_EncodeUInt32( data + 4, 0 );
		return;
	}else if( isnan( value ) ){
		DaoByteCoder_EncodeNAN( data );
		return;
	}else if( isinf( value ) ){
		DaoByteCoder_EncodeInf( data );
		return;
	}

	frac = frexp( fabs( value ), & expon );
	frac = 2.0 * frac - 1.0;
	expon -= 1;
	while(1){
		double prod = frac * 2.0;
		uint_t bit = (uint_t) prod;
		frac = prod - bit;
		i -= 1;
		if( first ){
			m1 |= bit << i;
			if( i == 0 ){
				first = 0;
				i = 32;
			}
		}else{
			m2 |= bit << i;
			if( i == 0 ) break;
		}
		if( frac <= 0.0 ) break;
	}
	m1 |= (expon+1023) << 20;
	if( neg ) m1 |= 1 << 31;
	DaoByteCoder_EncodeUInt32( data, m1 );
	DaoByteCoder_EncodeUInt32( data + 4, m2 );
}
uint_t DaoByteCoder_DecodeUInt8( uchar_t *data )
{
	return data[0];
}
uint_t DaoByteCoder_DecodeUInt16( uchar_t *data )
{
	int value = (data[0]<<8) + data[1];
	data += 2;
	return value;
}
uint_t DaoByteCoder_DecodeUInt32( uchar_t *data )
{
	uint_t value = data[0] << 24;
	value += data[1] << 16;
	value += data[2] << 8;
	value += data[3];
	data += 4;
	return value;
}
daoint DaoByteCoder_DecodeDaoInt( DaoByteCoder *self, uchar_t *data )
{
	DaoStream *stream = self->vmspace->errorStream;
	uchar_t i, m = self->intSize;
	daoint value = 0;

	if( self->intSize > sizeof(daoint) ){ /* self->intSize=8, sizeof(daoint)=4 */
		daoint B1 = data[0], B2 = data[1], B3 = data[2], B4 = data[3];
		daoint B5 = data[4], B6 = data[5], B7 = data[6], B8 = data[7];

		if( (B1 == 0x7F || B1 == 0xFF) && B2 == 0xFF && B3 == 0xFF && B4 == 0xFF ){
			if( B5 & 0x80 ) goto TooBigInteger;
			if( B1 == 0xFF ) B5 |= 0x80;
		}else if( B1 || B2 || B3 || B4 ){
			goto TooBigInteger;
		}
		return (B5<<24)|(B6<<16)|(B7<<8)|B8;
	}else if( self->intSize < sizeof(daoint) ){ /* self->intSize=4, sizeof(daoint)=8 */
		daoint B1 = data[0], B2 = data[1], B3 = data[2], B4 = data[3];

		if( B1 & 0x80 ){
			daoint leading = (0xFF<<24)|(0xFF<<16)|(0xFF<<8)|0xFF;
			daoint shift = 32; /* just to avoid a warning on 32 bit systems; */
			return (leading<<shift)|(0xFF<<24)|((B1&0x7F)<<24)|(B2<<16)|(B3<<8)|B4;
		}
		return (B1<<24)|(B2<<16)|(B3<<8)|B4;
	}

	for(i=0; i<m; ++i) value |= ((daoint)data[i]) << 8*(m-1-i);
	return value;
TooBigInteger:
	DaoStream_WriteMBS( stream, "Error: too big integer value for the platform!" );
	self->error = 1;
	return 0;
}
/*
// IEEE 754 double-precision binary floating-point format:
//   sign(1)--exponent(11)------------fraction(52)---------------------
//   S EEEEEEEEEEE FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
//   63         52                                                    0
//
//   value = (-1)^S  *  ( 1 + \sigma_0^51 (b_i * 2^{-(52-i)}) )  *  2^{E-1023}
*/
double DaoByteCoder_DecodeDouble( DaoByteCoder *self, uchar_t *data )
{
	double value = 1.0;
	uint_t first = DaoByteCoder_DecodeUInt32( data );
	uint_t second = DaoByteCoder_DecodeUInt32( data+4 );
	uint_t negative = first & (1<<31);
	int i, expon;

	if( first == 0 && second == 0 ) return 0;
	if( first == (0x7FF<<20) && second == 0 ) return INFINITY;
	if( first == (0x7FF<<20) && second == 1 ) return NAN;

	first = (first<<1)>>1;
	expon = (first>>20) - 1023;
	for(i=0; i<32; ++i){
		if( (second>>i)&0x1 ){
			int e = -(52-i);
			value += e >= 0 ? pow( 2, e ) : 1.0 / pow( 2, -e );
		}
	}
	for(i=0; i<20; ++i){
		if( (first>>i) & 0x1 ){
			int e = -(20-i);
			value += e >= 0 ? pow( 2, e ) : 1.0 / pow( 2, -e );
		}
	}
	if( expon >= 0 ){
		value *= pow( 2, expon );
	}else{
		value /= pow( 2, -expon );
	}
	if( negative ) value = -value;
	return value;
}
void DaoByteCoder_DecodeChunk2222( uchar_t *data, uint_t *A, uint_t *B, uint_t *C, uint_t *D )
{
	*A = DaoByteCoder_DecodeUInt16( data + 0 );
	*B = DaoByteCoder_DecodeUInt16( data + 2 );
	*C = DaoByteCoder_DecodeUInt16( data + 4 );
	*D = DaoByteCoder_DecodeUInt16( data + 6 );
}
void DaoByteCoder_DecodeChunk2114( uchar_t *data, uint_t *A, uint_t *B, uint_t *C, uint_t *D )
{
	*A = DaoByteCoder_DecodeUInt16( data + 0 );
	*B = DaoByteCoder_DecodeUInt8( data + 2 );
	*C = DaoByteCoder_DecodeUInt8( data + 3 );
	*D = DaoByteCoder_DecodeUInt32( data + 4 );
}
void DaoByteCoder_DecodeChunk224( uchar_t *data, uint_t *A, uint_t *B, uint_t *C )
{
	*A = DaoByteCoder_DecodeUInt16( data + 0 );
	*B = DaoByteCoder_DecodeUInt16( data + 2 );
	*C = DaoByteCoder_DecodeUInt32( data + 4 );
}
void DaoByteBlock_DecodeSubChunk222( uchar_t *data, uint_t *A, uint_t *B, uint_t *C )
{
	*A = DaoByteCoder_DecodeUInt16( data + 0 );
	*B = DaoByteCoder_DecodeUInt16( data + 2 );
	*C = DaoByteCoder_DecodeUInt16( data + 4 );
}
void DaoByteBlock_DecodeSubChunk114( uchar_t *data, uint_t *A, uint_t *B, uint_t *C )
{
	*A = DaoByteCoder_DecodeUInt8( data + 0 );
	*B = DaoByteCoder_DecodeUInt8( data + 1 );
	*C = DaoByteCoder_DecodeUInt32( data + 2 );
}
void DaoByteBlock_DecodeSubChunk24( uchar_t *data, uint_t *A, uint_t *B )
{
	*A = DaoByteCoder_DecodeUInt16( data + 0 );
	*B = DaoByteCoder_DecodeUInt32( data + 2 );
}


DaoByteBlock* DaoByteBlock_New( DaoByteCoder *coder )
{
	DaoByteBlock *self = (DaoByteBlock*) dao_calloc(1,sizeof(DaoByteBlock));
	self->coder = coder;
	return self;
}
void DaoByteBlock_Delete( DaoByteBlock *self )
{
	if( self->blocks ) DMap_Delete( self->blocks );
	dao_free( self );
}

DaoByteCoder* DaoByteCoder_New( DaoVmSpace *vms )
{
	DaoByteCoder *self = (DaoByteCoder*) dao_calloc(1,sizeof(DaoByteCoder));
	self->blocks = DHash_New(D_VALUE,0);
	self->caches = DArray_New(0);
	self->stack = DArray_New(0);
	self->lines = DArray_New(0);
	self->path = DString_New(1);
	self->intSize = sizeof(daoint);
	self->vmspace = vms;
	return self;
}
void DaoByteCoder_Remove( DaoByteCoder *self, DaoByteBlock *block, DaoByteBlock *parent )
{
	DaoByteBlock *it = block->first;
	while( it ){
		DaoByteBlock *b = it;
		it = it->next;
		DaoByteCoder_Remove( self, b, block );
	}
	if( block->prev ) block->prev->next = block->next;
	if( block->next ) block->next->prev = block->prev;
	if( parent && parent->first == block ) parent->first = block->next;
	if( parent && parent->last == block ) parent->last = block->prev;
	block->first = block->last = NULL;
	block->prev = block->next = NULL;
	DArray_Append( self->caches, block );
}
void DaoByteCoder_Reset( DaoByteCoder *self )
{
	if( self->top ) DaoByteCoder_Remove( self, self->top, NULL );
	self->top = NULL;
	DArray_Clear( self->stack );
	DMap_Reset( self->blocks );
}
void DaoByteCoder_Delete( DaoByteCoder *self )
{
	int i, n;
	DaoByteCoder_Reset( self );
	for(i=0,n=self->caches->size; i<n; ++i){
		DaoByteBlock *block = (DaoByteBlock*) self->caches->items.pVoid[i];
		DaoByteBlock_Delete( block );
	}
	DArray_Delete( self->caches );
	DArray_Delete( self->stack );
	DArray_Delete( self->lines );
	DString_Delete( self->path );
	DMap_Delete( self->blocks );
	dao_free( self );
}

DaoByteBlock* DaoByteCoder_Init( DaoByteCoder *self )
{
	DaoByteCoder_Reset( self );
	self->top = DaoByteCoder_NewBlock( self, DAO_ASM_ROUTINE );
	return self->top;
}

int DaoByteCoder_UpdateIndex( DaoByteCoder *self, DaoByteBlock *block )
{
	int inserted = 0;
	DaoByteBlock *pb = block->first;
	DNode *it;

	self->index += block->type > 0 && block->type <= DAO_ASM_EVAL;
	block->index = self->index;
	if( block->blocks ){
		for(it=DMap_First(block->blocks); it; it=DMap_Next(block->blocks,it)){
			DaoByteBlock *pb = (DaoByteBlock*) it->value.pVoid;
			if( pb != NULL && block->index > (pb->index + 0x7fff) ){
				it->value.pVoid = DaoByteBlock_EncodeSeekStmt( block, pb );
				inserted += 1;
			}
		}
	}
	while( pb ){
		inserted += DaoByteCoder_UpdateIndex( self, pb );
		pb = pb->next;
	}
	return inserted;
}
void DaoByteBlock_CopyToEndFromBegin( DaoByteBlock *self, DaoByteBlock *other )
{
	int i;
	memcpy( self->end, other->begin, 8 );
	if( other->blocks == NULL ) return;
	if( other->blocks && self->blocks == NULL ) self->blocks = DMap_New(0,0);
	for(i=0; i<8; i+=2){
		DNode *it = DMap_Find( other->blocks, (other->begin + i) );
		if( it ) DMap_Insert( self->blocks, (self->end + i), it->value.pVoid );
	}
}
void DaoByteCoder_FinalizeBlock( DaoByteCoder *self, DaoByteBlock *block )
{
	DaoByteBlock *consts, *types, *code, *cur;
	DaoByteBlock *pb = block->first;
	DaoRoutine *routine;
	DMap *vartypes;
	DNode *it;
	uchar_t *data;
	int i, N;

	while( pb ){
		DaoByteCoder_FinalizeBlock( self, pb );
		pb = pb->next;
	}
	if( block->type != DAO_ASM_ROUTINE ) return;

	routine = (DaoRoutine*) block->value;

	/* local constants: */
	N = DaoByteBlock_EncodeValues2( block, & routine->routConsts->items );
	consts = DaoByteBlock_NewBlock( block, DAO_ASM_CONSTS );
	DaoByteCoder_EncodeUInt16( consts->begin, N );
	DaoByteBlock_AddBlockIndexData( consts, 3, N );

	/* explicit types: */
	i = block->coder->stack->size;
	vartypes = routine->body->localVarType;
	for(it=DMap_First(vartypes); it; it=DMap_Next(vartypes,it)){
		DaoByteBlock *pb = DaoByteBlock_EncodeType( block, it->value.pType );
		DArray_Append( block->coder->stack, pb );
	}
	types = DaoByteBlock_NewBlock( block, DAO_ASM_TYPES );
	DaoByteCoder_EncodeUInt16( types->begin, vartypes->size );
	cur = types;
	data = types->begin + 4;
	for(it=DMap_First(vartypes); it; it=DMap_Next(vartypes,it), ++i, data+=4){
		DaoByteBlock *pb = (DaoByteBlock*) block->coder->stack->items.pVoid[i];
		if( data >= cur->begin + 8 ){
			cur = DaoByteBlock_NewBlock( types, DAO_ASM_DATA );
			data = cur->begin;
		}
		DaoByteCoder_EncodeUInt16( data, it->key.pInt );
		DaoByteBlock_InsertBlockIndex( cur, data+2, pb );
	}
	if( cur != types ){
		DaoByteBlock_CopyToEndFromBegin( types, cur );
		DaoByteCoder_Remove( types->coder, cur, types );
	}

	/* code: */
	code = DaoByteBlock_NewBlock( block, DAO_ASM_CODE );
	self->lines->size = 0;
	for(i=0; i<routine->body->annotCodes->size; ++i){
		DaoVmCodeX *vmc = routine->body->annotCodes->items.pVmc[i];
		int count = self->lines->size;
		int lastline = count ? self->lines->items.pInt[count-2] : -1;
		int lastcount = count ? self->lines->items.pInt[count-1] : -1;
		if( vmc->line != lastline ){
			DArray_PushBack( self->lines, IntToPointer( vmc->line ) );
			DArray_PushBack( self->lines, IntToPointer( 1 ) );
		}else{
			self->lines->items.pInt[count-1] += 1;
		}
	}
	DaoByteCoder_EncodeUInt16( code->begin, routine->body->annotCodes->size );
	DaoByteCoder_EncodeUInt16( code->begin + 2, self->lines->size/2 );
	cur = code;
	data = cur->begin + 4;
	for(i=0; i<self->lines->size; i+=2, data+=4){
		int line = self->lines->items.pInt[i];
		int count = self->lines->items.pInt[i+1];
		if( data >= cur->begin + 8 ){
			cur = DaoByteBlock_NewBlock( code, DAO_ASM_DATA );
			data = cur->begin;
		}
		DaoByteCoder_EncodeUInt16( data, line );
		DaoByteCoder_EncodeUInt16( data+2, count );
	}

	for(i=0; i<routine->body->annotCodes->size; ++i){
		DaoVmCodeX *vmc = routine->body->annotCodes->items.pVmc[i];
		cur = DaoByteBlock_NewBlock( code, DAO_ASM_DATA );
		DaoByteCoder_EncodeUInt16( cur->begin + 0, vmc->code );
		DaoByteCoder_EncodeUInt16( cur->begin + 2, vmc->a );
		DaoByteCoder_EncodeUInt16( cur->begin + 4, vmc->b );
		DaoByteCoder_EncodeUInt16( cur->begin + 6, vmc->c );
	}
	if( cur != code ){
		DaoByteBlock_CopyToEndFromBegin( code, cur );
		DaoByteCoder_Remove( code->coder, cur, code );
	}
}
void DaoByteCoder_FinalizeEncoding( DaoByteCoder *self, DaoByteBlock *block )
{
	DaoByteBlock *pb = block->first;
	DaoRoutine *routine;
	DNode *it;
	int i, N;

	if( block->blocks ){
		for(it=DMap_First(block->blocks); it; it=DMap_Next(block->blocks,it)){
			DaoByteBlock *pb = (DaoByteBlock*) it->value.pVoid;
			int diff = 0;
			if( pb != NULL ){
				diff = block->index - pb->index;
				if( diff == 0 ) diff = 1;
			}
			DaoByteCoder_EncodeUInt16( (uchar_t*) it->key.pVoid, diff );
		}
	}
	while( pb ){
		DaoByteCoder_FinalizeEncoding( self, pb );
		pb = pb->next;
	}
}
void DaoByteCoder_EncodeHeader( DaoByteCoder *self, const char *fname, DString *output )
{
	DString *path = DString_New(1);
	char *daodir = getenv( "DAO_DIR" );
	char *home = getenv( "HOME" );
	uchar_t bytes[2];

	DString_SetMBS( path, fname );
	if( daodir && DString_Find( path, daodir, 0 ) == 0 ){
		DString_ReplaceMBS( path, "$(DAO_DIR)", 0, strlen(daodir) );
	}else if( home && DString_Find( path, home, 0 ) == 0 ){
		DString_ReplaceMBS( path, "$(HOME)", 0, strlen(home) );
	}

	DaoByteCoder_EncodeUInt16( bytes, path->size );

	DString_AppendDataMBS( output, DAO_BC_SIGNATURE "\0", 9 );
	DString_AppendChar( output, sizeof(daoint) == 4 ? '\4' : '\x8' );
	DString_AppendDataMBS( output, "\0\0\0\0\0", 5 );
	DString_AppendDataMBS( output, "\0\0\0\0\0", 5 );
	DString_AppendDataMBS( output, "\0\0\0\0\0", 5 );
	DString_AppendDataMBS( output, "\0\0\0\0\0\r\n", 7 );

	DString_AppendDataMBS( output, bytes, 2 );
	DString_AppendDataMBS( output, path->mbs, path->size );
	DString_AppendDataMBS( output, "\0\r\n", 3 );
	DString_Delete( path );
}
void DaoByteBlock_EncodeToString( DaoByteBlock *block, DString *output )
{
	DaoByteBlock *pb = block->first;

	if( block->type == DAO_ASM_NONE ) return;

	DString_AppendChar( output, block->type );
	DString_AppendDataMBS( output, block->begin, 8 );
	while( pb ){
		DaoByteBlock_EncodeToString( pb, output );
		pb = pb->next;
	}
	if( block->type == 0 || block->type >= DAO_ASM_END ) return;
	DString_AppendChar( output, DAO_ASM_END );
	DString_AppendDataMBS( output, block->end, 8 );
}
void DaoByteCoder_MergeData( DaoByteCoder *self, DaoByteBlock *block )
{
	uint_t A, B, C, D, E, F, G, H;
	DaoByteBlock *pb = block->first;
	while( pb ){
		if( pb->type == DAO_ASM_DATA ){
			DaoByteBlock *pb2 = pb->next;
			DaoByteCoder_DecodeChunk2222( pb->begin, & A, & B,  & C, & D );
			if( A <= 0xff && B <= 0xff && C <= 0xff && D <= 0xff && pb2 != NULL ){
				DaoByteCoder_DecodeChunk2222( pb2->begin, & E, & F,  & G, & H );
				if( E <= 0xff && F <= 0xff && G <= 0xff && H <= 0xff ){
					pb->begin[0] = A; pb->begin[1] = B; pb->begin[2] = C; pb->begin[3] = D;
					pb->begin[4] = E; pb->begin[5] = F; pb->begin[6] = G; pb->begin[7] = H;
					pb->type = DAO_ASM_DATA2;
					pb2->type = DAO_ASM_NONE;
					pb = pb->next;
				}
			}
		}else{
			DaoByteCoder_MergeData( self, pb );
		}
		pb = pb->next;
	}
}
void DaoByteCoder_SplitData( DaoByteCoder *self, DaoByteBlock *block )
{
	int i;
	DaoByteBlock *pb = block->first;
	while( pb ){
		if( pb->type == DAO_ASM_DATA2 ){
			DaoByteBlock *pb2 = pb->next;
			if( pb2 == NULL || pb2->type != DAO_ASM_NONE ){
				pb2 = DaoByteCoder_NewBlock( self, DAO_ASM_DATA );
				pb2->next = pb->next;
				pb2->prev = pb;
				if( pb->next ) pb->next->prev = pb2;
				pb->next = pb2;
			}
			pb->type = DAO_ASM_DATA;
			pb2->type = DAO_ASM_DATA;
			for(i=0; i<4; ++i)  DaoByteCoder_EncodeUInt16( pb2->begin + 2*i, pb->begin[4+i] );
			for(i=3; i>=0; --i) DaoByteCoder_EncodeUInt16( pb->begin + 2*i, pb->begin[i] );
			pb = pb->next;
		}else{
			DaoByteCoder_SplitData( self, pb );
		}
		pb = pb->next;
	}
}
void DaoByteCoder_Finalize( DaoByteCoder *self )
{
	if( self->top == NULL ) return;

	DaoByteCoder_FinalizeBlock( self, self->top );

	self->index = 0;
	while( DaoByteCoder_UpdateIndex( self, self->top ) ) self->index = 0;
	DaoByteCoder_FinalizeEncoding( self, self->top );
}
void DaoByteCoder_EncodeToString( DaoByteCoder *self, DString *output )
{
	if( self->top == NULL ) return;
	DaoByteCoder_Finalize( self );
	DaoByteCoder_MergeData( self, self->top );
	DaoByteBlock_EncodeToString( self->top, output );
	printf( "output: %i\n", output->size );
}

DaoByteBlock* DaoByteCoder_NewBlock( DaoByteCoder *self, int type )
{
	DaoByteBlock *block = (DaoByteBlock*) DArray_PopBack( self->caches );
	if( block == NULL ) block = DaoByteBlock_New( self );
	if( block->blocks ) DMap_Reset( block->blocks );
	block->type = type;
	memset( block->begin, 0, 8 );
	memset( block->end, 0, 8 );
	return block;
}

DaoByteBlock* DaoByteBlock_NewBlock( DaoByteBlock *self, int type )
{
	DaoByteBlock *block = DaoByteCoder_NewBlock( self->coder, type );
	if( self->last == NULL ){
		self->last = self->first = block;
	}else{
		block->prev = self->last;
		self->last->next = block;
		self->last = block;
	}
	return block;
}

DaoByteBlock* DaoByteBlock_FindBlock( DaoByteBlock *self, DaoValue *value )
{
	DNode *it;
	if( value == NULL ) return NULL;
	it = DMap_Find( self->coder->blocks, value );
	if( it ) return (DaoByteBlock*) it->value.pVoid;
	return NULL;
}
DaoByteBlock* DaoByteBlock_AddBlock( DaoByteBlock *self, DaoValue *value, int type )
{
	DaoByteBlock *block = DaoByteBlock_NewBlock( self, type );
	// TODO
	DaoValue_Copy( value, & block->value );
	DMap_Insert( self->coder->blocks, value, block );
	return block;
}
DaoByteBlock* DaoByteBlock_AddRoutineBlock( DaoByteBlock *self, DaoRoutine *routine )
{
	DaoByteBlock *type = DaoByteBlock_EncodeType( self, routine->routType );
	DaoByteBlock *host = DaoByteBlock_EncodeType( self, routine->routHost );
	DaoByteBlock *name = DaoByteBlock_EncodeString( self, routine->routName );
	DaoByteBlock *rout = DaoByteBlock_AddBlock( self, (DaoValue*) routine, DAO_ASM_ROUTINE );
	DaoByteBlock_InsertBlockIndex( rout, rout->begin, name );
	DaoByteBlock_InsertBlockIndex( rout, rout->begin+2, type );
	if( host ) DaoByteBlock_InsertBlockIndex( rout, rout->begin+4, host );
	DaoByteCoder_EncodeUInt16( rout->begin+6, routine->attribs );
	return rout;
}
DaoByteBlock* DaoByteBlock_AddEvalBlock( DaoByteBlock *self, DaoValue *value, int code, int operands )
{
	DaoByteBlock *block = DaoByteBlock_AddBlock( self, value, DAO_ASM_EVAL );
	DaoByteCoder_EncodeUInt16( block->begin, code );
	DaoByteCoder_EncodeUInt16( block->begin+2, operands );
	return block;
}
void DaoByteBlock_InsertBlockIndex( DaoByteBlock *self, uchar_t *code, DaoByteBlock *block )
{
	if( self->blocks == NULL ) self->blocks = DMap_New(0,0);
	DMap_Insert( self->blocks, code, block );
}


DaoByteBlock* DaoByteBlock_EncodeString( DaoByteBlock *self, DString *string )
{
	DaoByteBlock *block;
	DaoString daostring = {DAO_STRING,0,0,0,1,NULL};
	DaoValue *value = (DaoValue*) & daostring;
	int i, j, size = string->size;

	daostring.data = string;
	block = DaoByteBlock_FindBlock( self, value );
	if( block ) return block;
	block = DaoByteBlock_AddBlock( self, value, DAO_ASM_VALUE );
	DaoByteCoder_EncodeUInt16( block->begin, DAO_STRING );
	DaoByteCoder_EncodeUInt16( block->begin+2, string->mbs == NULL );
	DaoByteCoder_EncodeUInt32( block->begin+4, string->size );
	if( string->mbs ){
		for(i=0; (i+8)<size; i+=8){
			DaoByteBlock *dataBlock = DaoByteBlock_NewBlock( block, DAO_ASM_DATA );
			memcpy( dataBlock->begin, string->mbs+i, 8*sizeof(char) );
		}
		memcpy( block->end, string->mbs+i, (size-i)*sizeof(char) );
	}else{
		for(i=0; (i+2)<size; i+=2){
			DaoByteBlock *dataBlock = DaoByteBlock_NewBlock( block, DAO_ASM_DATA );
			DaoByteCoder_EncodeUInt32( dataBlock->begin, string->wcs[i] );
			DaoByteCoder_EncodeUInt32( dataBlock->begin+4, string->wcs[i+1] );
		}
		if( i < size ) DaoByteCoder_EncodeUInt32( block->end, string->wcs[i] );
		if( (i+1) < size ) DaoByteCoder_EncodeUInt32( block->end+4, string->wcs[i] );
	}
	return block;
}
void DaoByteBlock_EncodeValues( DaoByteBlock *self, DaoValue **values, int count )
{
	int i;
	for(i=0; i<count; ++i){
		DaoByteBlock *block = DaoByteBlock_EncodeValue( self, values[i] );
		DArray_Append( self->coder->stack, block );
	}
}
int DaoByteBlock_EncodeValues2( DaoByteBlock *self, DArray *values )
{
	DaoByteBlock_EncodeValues( self, values->items.pValue, values->size );
	return values->size;
}
void DaoByteBlock_AddBlockIndexData( DaoByteBlock *self, int head, int size )
{
	DaoByteBlock *dataBlock = self;
	uchar_t *data = dataBlock->begin + 2*(4-head);
	int i, j, offset;
	if( size > self->coder->stack->size ) size = self->coder->stack->size;
	offset = self->coder->stack->size - size;
	for(i=0; i<size; ++i, data+=2){
		DaoByteBlock *block = (DaoByteBlock*) self->coder->stack->items.pVoid[i+offset];
		if( data >= dataBlock->begin + 8 ){
			dataBlock = DaoByteBlock_NewBlock( self, DAO_ASM_DATA );
			data = dataBlock->begin;
		}
		DaoByteBlock_InsertBlockIndex( dataBlock, data, block );
	}
	if( dataBlock != self ){
		DaoByteBlock_CopyToEndFromBegin( self, dataBlock );
		DaoByteCoder_Remove( self->coder, dataBlock, self );
	}
	DArray_Erase( self->coder->stack, offset, size );
}
DaoByteBlock* DaoByteBlock_EncodeType( DaoByteBlock *self, DaoType *type )
{
	DaoByteBlock *newBlock = DaoByteBlock_FindBlock( self, (DaoValue*) type );
	DaoByteBlock *nameBlock;
	int size = 0;

	if( type == NULL ) return NULL;
	if( newBlock ) return newBlock;
	if( type->nested ) size = DaoByteBlock_EncodeValues2( self, type->nested );
	nameBlock = DaoByteBlock_EncodeString( self, type->name );
	newBlock = DaoByteBlock_AddBlock( self, (DaoValue*) type, DAO_ASM_TYPE );

	DaoByteBlock_InsertBlockIndex( newBlock, newBlock->begin, nameBlock );
	DaoByteCoder_EncodeUInt16( newBlock->begin+2, type->tid );
	DaoByteBlock_AddBlockIndexData( newBlock, 0, size );
	return newBlock;
}
DaoByteBlock* DaoByteBlock_EncodeValue( DaoByteBlock *self, DaoValue *value )
{
	DaoByteBlock *newBlock;
	if( value == NULL ) return NULL;
	newBlock = DaoByteBlock_FindBlock( self, value );
	if( newBlock ) return newBlock;
	switch( value->type ){
	case DAO_INTEGER : return DaoByteBlock_EncodeInteger( self, value->xInteger.value );
	case DAO_FLOAT   : return DaoByteBlock_EncodeFloat( self, value->xFloat.value );
	case DAO_DOUBLE  : return DaoByteBlock_EncodeDouble( self, value->xDouble.value );
	case DAO_COMPLEX : return DaoByteBlock_EncodeComplex( self, (DaoComplex*) value );
	case DAO_STRING : return DaoByteBlock_EncodeString( self, value->xString.data );
	case DAO_LIST  : return DaoByteBlock_EncodeList( self, (DaoList*) value );
	//case DAO_MAP   : return DaoByteBlock_EncodeMap( self, (DaoMap*) value );
	//case DAO_TUPLE : return DaoByteBlock_EncodeTuple( self, (DaoTuple*) value );
	case DAO_TYPE  : return DaoByteBlock_EncodeType( self, (DaoType*) value );
	}
	return NULL;
}
DaoByteBlock* DaoByteBlock_EncodeLoadStmt( DaoByteBlock *self, DString *mod, DString *ns )
{
	DaoByteBlock *fileBlock = DaoByteBlock_EncodeString( self, mod );
	DaoByteBlock *nameBlock = ns ? DaoByteBlock_EncodeString( self, ns ) : NULL;
	DaoByteBlock *newBlock = DaoByteBlock_NewBlock( self, DAO_ASM_LOAD );
	DaoByteBlock_InsertBlockIndex( newBlock, newBlock->begin, fileBlock );
	if( ns ) DaoByteBlock_InsertBlockIndex( newBlock, newBlock->begin+2, nameBlock );
	return newBlock;
}
DaoByteBlock* DaoByteBlock_EncodeSeekStmt( DaoByteBlock *self, DaoByteBlock *target )
{
	DaoByteBlock *newBlock = DaoByteBlock_NewBlock( self, DAO_ASM_SEEK );
	DaoByteBlock_InsertBlockIndex( newBlock, newBlock->begin, target );
	return newBlock;
}

DaoByteBlock* DaoByteBlock_EncodeInteger( DaoByteBlock *self, daoint val )
{
	DaoByteBlock *block;
	DaoInteger tmp = {DAO_INTEGER,0,0,0,0,0.0};
	DaoValue *value = (DaoValue*) & tmp;

	value->xInteger.value = val;
	block = DaoByteBlock_FindBlock( self, value );
	if( block ) return block;
	block = DaoByteBlock_AddBlock( self, value, DAO_ASM_VALUE );
	DaoByteCoder_EncodeUInt16( block->begin, DAO_INTEGER );
	DaoByteCoder_EncodeDaoInt( block->end, val );
	return block;
}
DaoByteBlock* DaoByteBlock_EncodeFloat( DaoByteBlock *self, float val )
{
	DaoByteBlock *block;
	DaoFloat tmp = {DAO_FLOAT,0,0,0,0,0.0};
	DaoValue *value = (DaoValue*) & tmp;

	value->xFloat.value = val;
	block = DaoByteBlock_FindBlock( self, value );
	if( block ) return block;
	block = DaoByteBlock_AddBlock( self, value, DAO_ASM_VALUE );
	DaoByteCoder_EncodeUInt16( block->begin, DAO_FLOAT );
	DaoByteCoder_EncodeFloat( block->end, val );
	return block;
}
DaoByteBlock* DaoByteBlock_EncodeDouble( DaoByteBlock *self, double val )
{
	DaoByteBlock *block;
	DaoDouble tmp = {DAO_DOUBLE,0,0,0,0,0.0};
	DaoValue *value = (DaoValue*) & tmp;

	value->xDouble.value = val;
	block = DaoByteBlock_FindBlock( self, value );
	if( block ) return block;
	block = DaoByteBlock_AddBlock( self, value, DAO_ASM_VALUE );
	DaoByteCoder_EncodeUInt16( block->begin, DAO_DOUBLE );
	DaoByteCoder_EncodeDouble( block->end, val );
	return block;
}
DaoByteBlock* DaoByteBlock_EncodeComplex( DaoByteBlock *self, DaoComplex *value )
{
	DaoByteBlock *block2, *block = DaoByteBlock_FindBlock( self, (DaoValue*) value );
	if( block ) return block;
	block = DaoByteBlock_AddBlock( self, value, DAO_ASM_VALUE );
	block2 = DaoByteBlock_AddBlock( block, value, DAO_ASM_VALUE );
	DaoByteCoder_EncodeUInt16( block->begin, DAO_COMPLEX );
	DaoByteCoder_EncodeDouble( block2->begin, value->value.real );
	DaoByteCoder_EncodeDouble( block->end, value->value.imag );
	return block;
}
DaoByteBlock* DaoByteBlock_EncodeLong( DaoByteBlock *self, DLong *value )
{
}
DaoByteBlock* DaoByteBlock_EncodeEnum( DaoByteBlock *self, DaoEnum *value )
{
}

DaoByteBlock* DaoByteBlock_EncodeArray( DaoByteBlock *self, DaoArray *value )
{
}
DaoByteBlock* DaoByteBlock_EncodeList( DaoByteBlock *self, DaoList *value )
{
	DaoByteBlock *newBlock = DaoByteBlock_FindBlock( self, value );
	DaoByteBlock *typeBlock;
	int size;
	
	if( newBlock ) return newBlock;

	size = DaoByteBlock_EncodeValues2( self, & value->items );
	typeBlock = DaoByteBlock_EncodeString( self, value->ctype );
	newBlock = DaoByteBlock_AddBlock( self, value, DAO_ASM_VALUE );

	DaoByteCoder_EncodeUInt16( newBlock->begin, DAO_LIST );
	DaoByteCoder_EncodeUInt32( newBlock->begin+4, value->items.size );
	DaoByteBlock_InsertBlockIndex( newBlock, newBlock->begin+2, typeBlock );
	DaoByteBlock_AddBlockIndexData( newBlock, 0, size );
	return newBlock;
}



void DaoByteCoder_PrintBlock( DaoByteCoder *self, DaoByteBlock *block, int spaces );
void DaoByteCoder_PrintSubBlocks( DaoByteCoder *self, DaoByteBlock *block, int spaces )
{
	DaoByteBlock *pb = block->first;
	while( pb ){
		if( pb != block->first && pb->type < DAO_ASM_END ) printf( "\n" );
		DaoByteCoder_PrintBlock( self, pb, spaces );
		pb = pb->next;
	}
}
void DaoByteBlock_PrintTag( int tag, int spaces )
{
	while( spaces -- ) printf( " " );
	printf( "%s: ", dao_asm_names[tag] );
}
void DaoByteBlock_PrintChars( uchar_t *chars, int count, int wcs )
{
	int i;
	if( wcs ){
		wchar_t ch1 = DaoByteCoder_DecodeUInt32( chars );
		wchar_t ch2 = DaoByteCoder_DecodeUInt32( chars + 4 );
		printf( "\"" );
		if( count > 0 ) printf( iswprint( ch1 ) ? "%lc" : "\\%i", ch1 );
		if( count > 1 ) printf( iswprint( ch2 ) ? "%lc" : "\\%i", ch2 );
		printf( "\";\n" );
		return;
	}
	printf( "\'" );
	for(i=0; i<count; ++i){
		int ch = chars[i];
		if( isprint(ch) ){
			printf( "%c", ch );
		}else{
			printf( "\\%i", ch );
		}
	}
	printf( "\';\n" );
}
void DaoByteCoder_PrintBlock( DaoByteCoder *self, DaoByteBlock *block, int spaces )
{
	int lines;
	uint_t i, A, B, C, D;
	DaoByteBlock *pb = block->first;

	DaoByteBlock_PrintTag( block->type, spaces );
	switch( block->type ){
	case DAO_ASM_ROUTINE :
		DaoByteCoder_DecodeChunk2222( block->begin, & A, & B, & C, & D );
		printf( "%i, %i, %i, %i;\n", A, B, C, D );
		DaoByteCoder_PrintSubBlocks( self, block, spaces + 4 );
		DaoByteBlock_PrintTag( DAO_ASM_END, spaces );
		printf( " ;\n" );
		break;
	case DAO_ASM_VALUE :
		A = DaoByteCoder_DecodeUInt16( block->begin );
		switch( A ){
		case DAO_INTEGER :
			printf( "DAO_INTEGER;\n" );
			DaoByteBlock_PrintTag( DAO_ASM_END, spaces );
			printf( " %" DAO_INT_FORMAT " ;\n", DaoByteCoder_DecodeDaoInt( self, block->end ) );
			break;
		case DAO_FLOAT :
			printf( "DAO_FLOAT;\n" );
			DaoByteBlock_PrintTag( DAO_ASM_END, spaces );
			printf( " %g ;\n", DaoByteCoder_DecodeDouble( self, block->end ) );
			break;
		case DAO_DOUBLE :
			printf( "DAO_DOUBLE;\n" );
			DaoByteBlock_PrintTag( DAO_ASM_END, spaces );
			printf( " %g ;\n", DaoByteCoder_DecodeDouble( self, block->end ) );
			break;
		case DAO_STRING :
			DaoByteBlock_DecodeSubChunk24( block->begin + 2, & B, & C );
			printf( "DAO_STRING, %i, %i;\n", B, C );
			D = B ? 2 : 8;
			while( pb ){
				DaoByteBlock_PrintTag( pb->type, spaces + 4 );
				DaoByteBlock_PrintChars( pb->begin, D, B );
				pb = pb->next;
			}
			DaoByteBlock_PrintTag( DAO_ASM_END, spaces );
			DaoByteBlock_PrintChars( block->end, (C%D) ? (C%D) : D, B );
			break;
		default :
			DaoByteCoder_PrintSubBlocks( self, block, spaces + 4 );
			DaoByteBlock_PrintTag( DAO_ASM_END, spaces );
			printf( " ;\n" );
			break;
		}
		break;
	case DAO_ASM_EVAL :
		DaoByteCoder_DecodeChunk2222( block->begin, & A, & B, & C, & D );
		printf( "%s, %i, %i, %i;\n", DaoVmCode_GetOpcodeName( A ), B, C, D );
		while( pb ){
			DaoByteCoder_DecodeChunk2222( pb->begin, & A, & B, & C, & D );
			DaoByteBlock_PrintTag( pb->type, spaces + 4 );
			printf( "%i, %i, %i, %i;\n", A, B, C, D );
			pb = pb->next;
		}
		DaoByteCoder_DecodeChunk2222( block->end, & A, & B, & C, & D );
		DaoByteBlock_PrintTag( DAO_ASM_END, spaces );
		printf( "%i, %i, %i, %i;\n", A, B, C, D );
		break;
	case DAO_ASM_TYPE :
	case DAO_ASM_CONSTS :
	case DAO_ASM_TYPES :
		DaoByteCoder_DecodeChunk2222( block->begin, & A, & B, & C, & D );
		printf( "%5i, %5i, %5i, %5i;\n", A, B, C, D );
		while( pb ){
			DaoByteCoder_DecodeChunk2222( pb->begin, & A, & B, & C, & D );
			DaoByteBlock_PrintTag( pb->type, spaces + 4 );
			printf( "%5i, %5i, %5i, %5i;\n", A, B, C, D );
			pb = pb->next;
		}
		DaoByteCoder_DecodeChunk2222( block->end, & A, & B, & C, & D );
		DaoByteBlock_PrintTag( DAO_ASM_END, spaces );
		printf( "%5i, %5i, %5i, %5i;\n", A, B, C, D );
		break;
	case DAO_ASM_CODE :
		DaoByteCoder_DecodeChunk2222( block->begin, & A, & B, & C, & D );
		printf( "%5i, %5i, %5i, %5i;\n", A, B, C, D );
		lines = B - 1;
		while( pb ){
			DaoByteCoder_DecodeChunk2222( pb->begin, & A, & B, & C, & D );
			DaoByteBlock_PrintTag( pb->type, spaces + 4 );
			if( lines > 0 ){
				printf( "%5i, %5i, %5i, %5i;\n", A, B, C, D );
			}else{
				printf( "   %-11s, %5i, %5i, %5i;\n", DaoVmCode_GetOpcodeName( A ), B, C, D );
			}
			pb = pb->next;
			lines -= 2;
		}
		DaoByteCoder_DecodeChunk2222( block->end, & A, & B, & C, & D );
		DaoByteBlock_PrintTag( DAO_ASM_END, spaces );
		printf( "        %-11s, %5i, %5i, %5i;\n", DaoVmCode_GetOpcodeName( A ), B, C, D );
		break;
	case DAO_ASM_DATA :
		DaoByteCoder_DecodeChunk2222( block->begin, & A, & B, & C, & D );
		printf( "%5i, %5i, %5i, %5i;\n", A, B, C, D );
		break;
	default:
		if( block->type < DAO_ASM_END ){
			printf( ";\n" );
			DaoByteBlock_PrintTag( DAO_ASM_END, spaces );
		}
		printf( ";\n" );
		break;
	}
}
void DaoByteCoder_Disassemble( DaoByteCoder *self )
{
	if( self->top == NULL ) return;
	DaoByteCoder_SplitData( self, self->top );
	DaoByteCoder_PrintBlock( self, self->top, 0 );
}


int DaoByteCoder_Decode( DaoByteCoder *self, DString *input )
{
	daoint i;
	DString header = *input;
	DString signature = DString_WrapBytes( DAO_BC_SIGNATURE, 8 );

	if( input->mbs == NULL ) return 0;

	if( header.size > 8 ) header.size = 8;
	if( DString_EQ( & header, & signature ) == 0 ) return 0;
	if( input->mbs[8] != 0 ) return 0; /* Not official format; */

	DaoByteCoder_Init( self );

	self->intSize = input->mbs[9];
	self->codes = (uchar_t*) input->mbs + 32;
	self->end = (uchar_t*) input->mbs + input->size;
	self->error = self->end + 1;
	if( self->intSize != 4 && self->intSize != 8 ) self->codes = self->error;

	i = DaoByteCoder_DecodeUInt16( self->codes );
	DString_Reset( self->path, i );
	memcpy( self->path->mbs, self->codes + 2, i*sizeof(char) );

	DArray_Append( self->stack, self->top );

	self->codes += i + 5;
	for(; self->codes < self->end; self->codes += 9){
		DaoByteBlock *current = (DaoByteBlock*) DArray_Back( self->stack );
		int type = self->codes[0];
		if( type == DAO_ASM_END ){
			memcpy( current->end, self->codes+1, 8*sizeof(char) );
			DArray_PopBack( self->stack );
		}else{
			DaoByteBlock *sublock = DaoByteBlock_NewBlock( current, type );
			memcpy( sublock->begin, self->codes+1, 8*sizeof(char) );
			if( type && type < DAO_ASM_END ) DArray_PushBack( self->stack, sublock );
		}
	}

	return 1;
}
int DaoByteCoder_Build( DaoByteCoder *self, DaoNamespace *nspace )
{
	if( self->top == NULL ) return 0;
	DaoByteCoder_SplitData( self, self->top );
	return 1;
}


