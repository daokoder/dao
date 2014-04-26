/*
// Dao Standard Modules
// http://www.daovm.net
//
// Copyright (c) 2011,2012, Limin Fu
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

#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<math.h>
#include"daoString.h"
#include"daoValue.h"
#include"daoParser.h"
#include"daoNamespace.h"
#include"daoVmspace.h"
#include"daoGC.h"
#include"dao_serializer.h"


#define RADIX 32
static const char *hex_digits = "ABCDEFGHIJKLMNOP";
static const char *mydigits = "0123456789ABCDEFGHIJKLMNOPQRSTUVW";

void DaoEncodeInteger( char *p, daoint value )
{
	int m;
	if( value < 0 ){
		*(p++) = '-';
		value = - value;
	}
	*(p++) = 'X';
	*p = 0;
	if( value == 0 ){
		*(p++) = '0';
		*p = 0;
		return;
	}
	while( value ){
		m = value % RADIX;
		value /= RADIX;
		*(p++) = mydigits[m];
	}
	*p = 0;
}
daoint DaoDecodeInteger( char *p )
{
	daoint value = 0;
	daoint power = 1;
	int sign = 1;
	if( *p == '-' ){
		sign = -1;
		p ++;
	}
	if( *p == 'X' ) p ++;
	while( *p ){
		int digit = *p;
		digit -= digit >= 'A' ? 'A' - 10 : '0';
		value += digit * power;
		power *= RADIX;
		p ++;
	}
	return value * sign;
}

void DaoEncodeDouble( char *buf, double value )
{
	int expon, digit;
	double prod, frac;
	char *p = buf;
	if( value <0.0 ){
		*(p++) = '-';
		value = -value;
	}
	*(p++) = 'X';
	*p = 0;
	frac = frexp( value, & expon );
	while(1){
		prod = frac * RADIX;
		digit = (int) prod;
		frac = prod - digit;
		*(p++) = mydigits[ digit ];
		if( frac <= 0 ) break;
	}
	*(p++) = '_';
	if( expon < 0 ) *(p++) = '_';
	DaoEncodeInteger( p, abs( expon ) );
	return;
}
double DaoDecodeDouble( char *buf )
{
	double frac = 0;
	int expon, sign = 1, sign2 = 1;
	char *p = buf;
	double factor = 1.0 / RADIX;
	double accum = factor;
	if( buf[0] == '-' ){
		p ++;
		sign = -1;
	}
	if( *p == 'X' ) p ++;
	while( *p && *p != '_' ){
		int digit = *p;
		digit -= digit >= 'A' ? 'A' - 10 : '0';
		frac += accum * digit;
		accum *= factor;
		p ++;
	}
	if( p[1] == '_' ){
		sign2 = -1;
		p ++;
	}
	expon = sign2 * DaoDecodeInteger( p+1 );
	return ldexp( frac, expon ) * sign;
}

static void DaoSerializeInteger( daoint value, DString *serial )
{
	char buf[100];
	DaoEncodeInteger( buf, value );
	DString_AppendMBS( serial, buf );
}
static void DaoSerializeDouble( double value, DString *serial )
{
	char buf[100];
	DaoEncodeDouble( buf, value );
	DString_AppendMBS( serial, buf );
}
static void DaoSerializeComplex( complex16 value, DString *serial )
{
	DaoSerializeDouble( value.real, serial );
	DString_AppendChar( serial, ' ' );
	DaoSerializeDouble( value.imag, serial );
}

static int DaoValue_Serialize2( DaoValue*, DString*, DaoNamespace*, DaoProcess*, DString*, DString*, DMap* );

static void DString_Serialize( DString *self, DString *serial, DString *buf )
{
	int i;
	unsigned char *mbs;

	DString_Clear( buf );
	DString_ToMBS( buf );
	DString_Append( buf, self );
	mbs = (unsigned char*) buf->mbs;
	DString_AppendChar( serial, self->mbs ? '\'' : '\"' );
	for(i=0; i<buf->size; i++){
		DString_AppendChar( serial, hex_digits[ mbs[i] / 16 ] );
		DString_AppendChar( serial, hex_digits[ mbs[i] % 16 ] );
	}
	DString_AppendChar( serial, self->mbs ? '\'' : '\"' );
}
static void DaoArray_Serialize( DaoArray *self, DString *serial, DString *buf )
{
	DaoInteger intmp = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *value = (DaoValue*) & intmp;
	int i;
	DString_AppendChar( serial, '[' );
	for(i=0; i<self->ndim; i++){
		value->xInteger.value = self->dims[i];
		if( i ) DString_AppendChar( serial, ',' );
		DaoValue_GetString( value, buf );
		DString_Append( serial, buf );
	}
	DString_AppendChar( serial, ']' );
	for(i=0; i<self->size; i++){
		if( i ) DString_AppendChar( serial, ',' );
		switch( self->etype ){
		case DAO_INTEGER : DaoSerializeInteger( self->data.i[i], serial ); break;
		case DAO_FLOAT : DaoSerializeDouble( self->data.f[i], serial ); break;
		case DAO_DOUBLE : DaoSerializeDouble( self->data.d[i], serial ); break;
		case DAO_COMPLEX : DaoSerializeComplex( self->data.c[i], serial ); break;
		}
	}
}
static int DaoList_Serialize( DaoList *self, DString *serial, DaoNamespace *ns, DaoProcess *proc, DString *buf, DMap *omap )
{
	DaoType *type = self->ctype;
	int i, rc = 1;
	if( type->nested && type->nested->size ) type = type->nested->items.pType[0];
	if( type && type->noncyclic == 0 && (type->tid == 0 || type->tid >= DAO_ENUM)) type = NULL;
	for(i=0; i<self->items.size; i++){
		DaoType *it = NULL;
		if( type == NULL ) it = DaoNamespace_GetType( ns, self->items.items.pValue[i] );
		if( i ) DString_AppendChar( serial, ',' );
		if( it == NULL && type && type->tid >= DAO_ARRAY ) DString_AppendChar( serial, '{' );
		rc &= DaoValue_Serialize2( self->items.items.pValue[i], serial, ns, proc, it?it->name:NULL, buf, omap );
		if( it == NULL && type && type->tid >= DAO_ARRAY ) DString_AppendChar( serial, '}' );
	}
	return rc;
}
static int DaoMap_Serialize( DaoMap *self, DString *serial, DaoNamespace *ns, DaoProcess *proc, DString *buf, DMap *omap )
{
	DaoType *type = self->ctype;
	DaoType *keytype = NULL;
	DaoType *valtype = NULL;
	DNode *node;
	char *sep = self->items->hashing ? ":" : "=>";
	int i = 0, rc = 1;
	if( type->nested && type->nested->size >0 ) keytype = type->nested->items.pType[0];
	if( type->nested && type->nested->size >1 ) valtype = type->nested->items.pType[1];
	if( keytype && (keytype->tid == 0 || keytype->tid >= DAO_ENUM)) keytype = NULL;
	if( valtype && (valtype->tid == 0 || valtype->tid >= DAO_ENUM)) valtype = NULL;
	for(node=DMap_First(self->items); node; node=DMap_Next(self->items,node)){
		DaoType *kt = NULL, *vt = NULL;
		if( keytype == NULL ) kt = DaoNamespace_GetType( ns, node->key.pValue );
		if( valtype == NULL ) vt = DaoNamespace_GetType( ns, node->value.pValue );
		if( (i++) ) DString_AppendChar( serial, ',' );
		rc &= DaoValue_Serialize2( node->key.pValue, serial, ns, proc, kt?kt->name:NULL, buf, omap );
		DString_AppendMBS( serial, sep );
		rc &= DaoValue_Serialize2( node->value.pValue, serial, ns, proc, vt?vt->name:NULL, buf, omap );
	}
	return rc;
}
static int DaoTuple_Serialize( DaoTuple *self, DString *serial, DaoNamespace *ns, DaoProcess *proc, DString *buf, DMap *omap )
{
	DArray *nested = self->ctype ? self->ctype->nested : NULL;
	int i, rc = 1;
	for(i=0; i<self->size; i++){
		DaoType *type = NULL;
		DaoType *it = NULL;
		if( nested && nested->size > i ) type = nested->items.pType[i];
		if( type && type->tid == DAO_PAR_NAMED ) type = & type->aux->xType;
		if( type && (type->tid == 0 || type->tid >= DAO_ENUM)) type = NULL;
		if( type == NULL ) it = DaoNamespace_GetType( ns, self->items[i] );
		if( i ) DString_AppendChar( serial, ',' );
		rc &= DaoValue_Serialize2( self->items[i], serial, ns, proc, it?it->name:NULL, buf, omap );
	}
	return rc;
}
static int DaoObject_Serialize( DaoObject *self, DString *serial, DaoNamespace *ns, DaoProcess *proc, DString *buf, DMap *omap )
{
	DString *tname = NULL;
	DaoType *type = NULL;
	DaoValue *ret = NULL;
	DaoValue *value1 = NULL;
	DaoValue *value2 = NULL;
	DaoValue *selfpar = (DaoValue*) self;
	DString name1 = DString_WrapMBS( "typename" );
	DString name2 = DString_WrapMBS( "serialize" );
	int errcode1 = DaoObject_GetData( self, & name1, & value1, NULL );
	int errcode2 = DaoObject_GetData( self, & name2, & value2, NULL );
	char chs[64];
	if( errcode2 || value2 == NULL || value2->type != DAO_ROUTINE ) return 0;
	if( errcode1 == 0 && value1 != NULL && value1->type == DAO_ROUTINE ){
		if( DaoProcess_Call( proc, (DaoRoutine*)value1, selfpar, NULL, 0 ) ) return 0;
		ret = proc->stackValues[0];
		if( ret->type == DAO_STRING ) tname = DString_Copy( ret->xString.data );
	}
	if( DaoProcess_Call( proc, (DaoRoutine*)value2, selfpar, NULL, 0 ) ){
		if( tname ) DString_Delete( tname );
		return 0;
	}
	if( tname == NULL ) tname = self->defClass->className;
	type = DaoNamespace_GetType( ns, proc->stackValues[0] );

	DString_Append( serial, tname );
	DString_AppendChar( serial, '{' );
	DMap_Insert( omap, self, self );
	sprintf( chs, "(%p)", self );
	DString_AppendMBS( serial, chs );

	DaoValue_Serialize2( proc->stackValues[0], serial, ns, proc, type?type->name:NULL, buf, omap );
	DString_AppendChar( serial, '}' );
	if( tname != NULL && tname != self->defClass->className ) DString_Delete( tname );
	return 1;
}
static int DaoCdata_Serialize( DaoCdata *self, DString *serial, DaoNamespace *ns, DaoProcess *proc, DString *buf, DMap *omap )
{
	DString *tname = NULL;
	DaoType *type = NULL;
	DaoValue *ret = NULL;
	DaoRoutine *meth1 = DaoType_FindFunctionMBS( self->ctype, "typename" );
	DaoRoutine *meth2 = DaoType_FindFunctionMBS( self->ctype, "serialize" );
	char chs[64];
	if( meth2 == NULL ) return 0;
	if( meth1 != NULL ){
		if( DaoProcess_Call( proc, meth1, (DaoValue*)self, NULL, 0 ) ) return 0;
		ret = proc->stackValues[0];
		if( ret->type == DAO_STRING ) tname = DString_Copy( ret->xString.data );
	}
	if( DaoProcess_Call( proc, meth2, (DaoValue*)self, NULL, 0 ) ){
		if( tname ) DString_Delete( tname );
		return 0;
	}
	if( tname == NULL ) tname = self->ctype->name;
	type = DaoNamespace_GetType( ns, proc->stackValues[0] );

	DString_Append( serial, tname );
	DString_AppendChar( serial, '{' );
	DMap_Insert( omap, self, self );
	sprintf( chs, "(%p)", self );
	DString_AppendMBS( serial, chs );

	DaoValue_Serialize2( proc->stackValues[0], serial, ns, proc, type?type->name:NULL, buf, omap );
	DString_AppendChar( serial, '}' );
	if( tname != NULL && tname != self->ctype->name ) DString_Delete( tname );
	return 1;
}
int DaoValue_Serialize3( DaoValue *self, DString *serial, DaoNamespace *ns, DaoProcess *proc, DString *tname, DString *buf, DMap *omap )
{
	switch( self->type ){
	case DAO_OBJECT :
		if( proc == NULL ) break;
		return DaoObject_Serialize( & self->xObject, serial, ns, proc, buf, omap );
	case DAO_CDATA :
	case DAO_CSTRUCT :
		if( proc == NULL ) break;
		return DaoCdata_Serialize( & self->xCdata, serial, ns, proc, buf, omap );
	}
	return 0;
}
int DaoValue_Serialize2( DaoValue *self, DString *serial, DaoNamespace *ns, DaoProcess *proc, DString *tname, DString *buf, DMap *omap )
{
	int rc = 1;
	char chs[64];
	if( DMap_Find( omap, self ) ){
		sprintf( chs, "@{%p}", self );
		DString_AppendMBS( serial, chs );
		return 1;
	}
	switch( self->type ){
	case DAO_OBJECT :
	case DAO_CDATA :
	case DAO_CSTRUCT :
		return DaoValue_Serialize3( self, serial, ns, proc, tname, buf, omap );
	}
	if( tname ){
		DString_Append( serial, tname );
		DString_AppendChar( serial, '{' );
	}
	switch( self->type ){
	case DAO_NONE :
		break;
	case DAO_INTEGER :
		DaoSerializeInteger( self->xInteger.value, serial );
		break;
	case DAO_FLOAT :
		DaoSerializeDouble( self->xFloat.value, serial );
		break;
	case DAO_DOUBLE :
		DaoSerializeDouble( self->xDouble.value, serial );
		break;
	case DAO_COMPLEX :
		DaoSerializeComplex( self->xComplex.value, serial );
		break;
	case DAO_STRING :
		DString_Serialize( self->xString.data, serial, buf );
		break;
	case DAO_ENUM :
		DaoSerializeInteger( self->xEnum.value, serial );
		break;
	case DAO_ARRAY :
		DaoArray_Serialize( & self->xArray, serial, buf );
		break;
	case DAO_LIST :
	case DAO_MAP :
	case DAO_TUPLE :
		DMap_Insert( omap, self, self );
		sprintf( chs, "(%p)", self );
		DString_AppendMBS( serial, chs );
		switch( self->type ){
		case DAO_LIST :
			rc = DaoList_Serialize( & self->xList, serial, ns, proc, buf, omap );
			break;
		case DAO_MAP :
			rc = DaoMap_Serialize( & self->xMap, serial, ns, proc, buf, omap );
			break;
		case DAO_TUPLE :
			rc = DaoTuple_Serialize( & self->xTuple, serial, ns, proc, buf, omap );
			break;
		}
		break;
	default :
		DString_AppendChar( serial, '?' );
		rc = 0;
		break;
	}
	if( tname ) DString_AppendChar( serial, '}' );
	return rc;
}
int DaoValue_Serialize( DaoValue *self, DString *serial, DaoNamespace *ns, DaoProcess *proc )
{
	DaoType *type = DaoNamespace_GetType( ns, self );
	DString *buf = DString_New(1);
	DMap *omap = DMap_New(0,0);
	int rc;
	DString_Clear( serial );
	DString_ToMBS( serial );
	rc = DaoValue_Serialize2( self, serial, ns, proc, type?type->name:NULL, buf, omap );
	DString_Delete( buf );
	DMap_Delete( omap );
	return rc;
}

int DaoParser_FindPairToken( DaoParser *self,  uchar_t lw, uchar_t rw, int start, int stop );
DaoType* DaoParser_ParseType( DaoParser *self, int start, int end, int *newpos, DArray *types );

static DaoObject* DaoClass_MakeObject( DaoClass *self, DaoValue *param, DaoProcess *proc )
{
	DaoObject *object = DaoObject_New( self );
	DaoProcess_CacheValue( proc, (DaoValue*) object );
	if( DaoProcess_PushCallable( proc, self->classRoutines, (DaoValue*)object, & param, 1 ) ==0 ){
		GC_ShiftRC( object, proc->topFrame->object );
		proc->topFrame->object = object;
		proc->topFrame->returning = -1;
		if( DaoProcess_Execute( proc ) ) return object;
	}
	return NULL;
}
static DaoCdata* DaoCdata_MakeObject( DaoCdata *self, DaoValue *param, DaoProcess *proc )
{
	DaoValue *value;
	DaoRoutine *routine = DaoType_FindFunction( self->ctype, self->ctype->name );
	if( DaoProcess_PushCallable( proc, routine, NULL, & param, 1 ) ) return NULL;
	proc->topFrame->active = proc->firstFrame;
	DaoProcess_SetActiveFrame( proc, proc->firstFrame ); /* return value in stackValues[0] */
	if( DaoProcess_Execute( proc ) == 0 ) return NULL;
	value = proc->stackValues[0];
	if( value && (value->type == DAO_CDATA || value->type == DAO_CSTRUCT) ) return & value->xCdata;
	return NULL;
}

/*
// Note: reference count is handled for "value2"!
// 
// Item of list/tuple etc. can be directly passed as parameter "value2",
// to avoid creating unnecessary intermediate objects.
*/
static int DaoParser_Deserialize( DaoParser *self, int start, int end, DaoValue **value2, DArray *types, DaoNamespace *ns, DaoProcess *proc, DMap *omap )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoType *it1 = NULL, *it2 = NULL, *type = NULL;
	DaoValue *value = *value2;
	DaoValue *tmp = NULL;
	DaoValue *tmp2 = NULL;
	DaoObject *object;
	DaoCdata *cdata;
	DaoArray *array;
	DaoTuple *tuple;
	DaoList *list;
	DaoMap *map;
	DArray *dims;
	DNode *node;
	void *key = NULL;
	char *str;
	int i, j, k, n;
	int minus = 0;
	int next = start + 1;
	int tok2 = start < end ? tokens[start+1]->type : 0;
	int maybetype = tok2 == DTOK_COLON2 || tok2 == DTOK_LT || tok2 == DTOK_LCB;

	if( tokens[start]->type == DTOK_AT && tok2 == DTOK_LCB ){
		int rb = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
		if( rb < 0 ) return next;
		sscanf( tokens[start+2]->string.mbs, "%p", & key );
		node = DMap_Find( omap, key );
		if( node ) DaoValue_Copy( node->value.pValue, value2 );
		return rb + 1;
	}
	if( tokens[start]->name == DTOK_ID_SYMBOL ){
		DString *mbs = DString_New(1);
		while( tokens[start]->name == DTOK_ID_SYMBOL ){
			DString_Append( mbs, & tokens[start]->string );
			start += 1;
		}
		type = DaoNamespace_MakeType( ns, mbs->mbs, DAO_ENUM, NULL, NULL, 0 );
		DString_Delete( mbs );
		if( type == NULL ) return start;
		if( tokens[start]->name != DTOK_LCB ) return start;
		end = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
		if( end < 0 ) return start;
		next = end + 1;
		start += 1;
		end -= 1;
	}else if( tokens[start]->type == DTOK_IDENTIFIER && maybetype ){
		type = DaoParser_ParseType( self, start, end, & start, NULL );
		if( type == NULL ) return next;
		if( tokens[start]->name != DTOK_LCB ) return start;
		end = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
		if( end < 0 ) return start;
		next = end + 1;
		start += 1;
		end -= 1;
	}
	if( type == NULL ){
		type = types->items.pType[0];
		if( type && type->tid >= DAO_ARRAY ){
			if( tokens[start]->name != DTOK_LCB ) return start;
			end = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
			if( end < 0 ) return start;
			next = end + 1;
			start += 1;
			end -= 1;
		}
	}
	if( type == NULL ) return next;
	DaoValue_Copy( type->value, value2 );
	if( start > end ) return next;
	if( tokens[start]->name == DTOK_SUB ){
		minus = 1;
		start += 1;
		if( start > end ) return next;
	}
	if( type->nested && type->nested->size >0 ) it1 = type->nested->items.pType[0];
	if( type->nested && type->nested->size >1 ) it2 = type->nested->items.pType[1];
	if( tokens[start]->name == DTOK_LB ){
		int rb = DaoParser_FindPairToken( self, DTOK_LB, DTOK_RB, start, end );
		if( rb < 0 ) return next;
		sscanf( tokens[start+1]->string.mbs, "%p", & key );
		DMap_Insert( omap, key, *value2 );
		start = rb + 1;
	}
	str = tokens[start]->string.mbs;
#if 0
	printf( "type: %s %s\n", type->name->mbs, str );
	for(i=start; i<=end; i++) printf( "%s ", tokens[i]->string.mbs ); printf( "\n" );
#endif
	value = *value2;
	switch( type->tid ){
	case DAO_NONE :
		break;
	case DAO_INTEGER :
		value->xInteger.value = DaoDecodeInteger( str );
		if( minus ) value->xInteger.value = - value->xInteger.value;
		break;
	case DAO_FLOAT :
		value->xFloat.value = DaoDecodeDouble( str );
		if( minus ) value->xFloat.value = - value->xFloat.value;
		break;
	case DAO_DOUBLE :
		value->xDouble.value = DaoDecodeDouble( str );
		if( minus ) value->xDouble.value = - value->xDouble.value;
		break;
	case DAO_COMPLEX :
		value->xComplex.value.real = DaoDecodeDouble( str );
		if( minus ) value->xComplex.value.real = - value->xComplex.value.real;
		if( start + 1 > end ) return start+1;
		minus = 0;
		if( tokens[start + 1]->name == DTOK_SUB ){
			minus = 1;
			start += 1;
			if( start + 1 > end ) return start+1;
		}
		value->xComplex.value.imag = DaoDecodeDouble( tokens[start+1]->string.mbs );
		if( minus ) value->xComplex.value.imag = - value->xComplex.value.imag;
		next = start + 2;
		break;
	case DAO_STRING :
		n = tokens[start]->string.size - 1;
		for(i=1; i<n; i++){
			char c1 = str[i];
			char c2 = str[i+1];
			if( c1 < 'A' || c1 > 'P' ) continue;
			DString_AppendChar( value->xString.data, (char)((c1-'A')*16 + (c2-'A')) );
			i += 1;
		}
		if( str[0] == '\"' ) DString_ToWCS( value->xString.data );
		break;
	case DAO_ENUM :
		value->xEnum.value = DaoDecodeInteger( str );
		break;
	case DAO_ARRAY :
#ifdef DAO_WITH_NUMARRAY
		if( tokens[start]->name != DTOK_LSB ) return next;
		k = DaoParser_FindPairToken( self, DTOK_LSB, DTOK_RSB, start, end );
		if( k < 0 ) return next;
		n = 1;
		for(i=start+1; i<k; i++){
			if( tokens[i]->name == DTOK_COMMA ) continue;
			n *= strtol( tokens[i]->string.mbs, 0, 0 );
		}
		if( n < 0 ) return next;
		if( it1 == NULL || it1->tid == 0 || it1->tid > DAO_COMPLEX ) return next;
		array = & value->xArray;
		dims = DArray_New(0);
		for(i=start+1; i<k; i++){
			if( tokens[i]->name == DTOK_COMMA ) continue;
			j = strtol( tokens[i]->string.mbs, 0, 0 );
			DArray_Append( dims, (size_t) j );
		}
		n = dims->size;
		DaoArray_ResizeArray( array, dims->items.pInt, n );
		DArray_PushFront( types, it1 );
		DArray_Delete( dims );
		n = 0;
		for(i=k+1; i<=end; i++){
			j = i + 1;
			while( j <= end && tokens[j]->name != DTOK_COMMA ) j += 1;
			DaoParser_Deserialize( self, i, j-1, & tmp, types, ns, proc, omap );
			switch( it1->tid ){
			case DAO_INTEGER : array->data.i[n] = tmp->xInteger.value; break;
			case DAO_FLOAT   : array->data.f[n] = tmp->xFloat.value; break;
			case DAO_DOUBLE  : array->data.d[n] = tmp->xDouble.value; break;
			}
			i = j;
			n += 1;
		}
		DArray_PopFront( types );
#endif
		break;
	case DAO_LIST :
		list = & value->xList;
		DArray_PushFront( types, it1 );
		n = 0;
		for(i=start; i<=end; i++){
			if( tokens[i]->name == DTOK_COMMA ) continue;
			DArray_Append( & list->items, NULL );
			k = DaoParser_Deserialize( self, i, end, list->items.items.pValue + n, types, ns, proc, omap );
			i = k - 1;
			n += 1;
		}
		DArray_PopFront( types );
		break;
	case DAO_MAP :
		map = & value->xMap;
		n = 0;
		for(i=start; i<=end; i++){
			if( tokens[i]->name == DTOK_COMMA ) continue;
			DaoValue_Clear( & tmp );
			DaoValue_Clear( & tmp2 );
			DArray_PushFront( types, it1 );
			i = DaoParser_Deserialize( self, i, end, &tmp, types, ns, proc, omap );
			DArray_PopFront( types );
			if( tokens[i]->name == DTOK_COMMA ) continue;
			if( map->items->size == 0 ){
				if( tokens[i]->name == DTOK_COLON ){
					DMap_Delete( map->items );
					map->items = DHash_New( D_VALUE, D_VALUE );
				}
			}
			if( tokens[i]->name == DTOK_COLON || tokens[i]->name == DTOK_FIELD ) i += 1;
			DArray_PushFront( types, it2 );
			i = DaoParser_Deserialize( self, i, end, &tmp2, types, ns, proc, omap );
			DArray_PopFront( types );
			node = DMap_Insert( map->items, (void*) tmp, (void*) tmp2 );
			i -= 1;
			n += 1;
		}
		break;
	case DAO_TUPLE :
		tuple = & value->xTuple;
		n = 0;
		for(i=start; i<=end; i++){
			if( tokens[i]->name == DTOK_COMMA ) continue;
			it1 = NULL;
			if( type->nested && type->nested->size > n ){
				it1 = type->nested->items.pType[n];
				if( it1 && it1->tid == DAO_PAR_NAMED ) it1 = & it1->aux->xType;
			}
			DArray_PushFront( types, it1 );
			i = DaoParser_Deserialize( self, i, end, tuple->items + n, types, ns, proc, omap );
			DArray_PopFront( types );
			i -= 1;
			n += 1;
		}
		break;
	case DAO_OBJECT :
		DArray_PushFront( types, NULL );
		DaoParser_Deserialize( self, start, end, & tmp, types, ns, proc, omap );
		DArray_PopFront( types );
		if( tmp == NULL ) break;
		object = DaoClass_MakeObject( & type->aux->xClass, tmp, proc );
		if( object ) DaoValue_Copy( (DaoValue*) object, value2 );
		break;
	case DAO_CDATA :
	case DAO_CSTRUCT :
		DArray_PushFront( types, NULL );
		DaoParser_Deserialize( self, start, end, & tmp, types, ns, proc, omap );
		DArray_PopFront( types );
		if( tmp == NULL ) break;
		cdata = DaoCdata_MakeObject( & type->aux->xCdata, tmp, proc );
		if( cdata ) DaoValue_Copy( (DaoValue*) cdata, value2 );
		break;
	}
	DaoValue_Clear( & tmp );
	DaoValue_Clear( & tmp2 );
	return next;
}
/*
// Note: reference count is not handled for "self"!
// But it is cached in the DaoProcess object, so no need to handle it by user!
*/
int DaoValue_Deserialize( DaoValue **self, DString *serial, DaoNamespace *ns, DaoProcess *proc )
{
	DaoRoutine *routine = NULL;
	DaoParser *parser = DaoVmSpace_AcquireParser( ns->vmSpace );
	DArray *types = DArray_New(0);
	DMap *omap = DMap_New(0,0);
	int rc;

	*self = NULL;
	parser->nameSpace = ns;
	parser->vmSpace = ns->vmSpace;
	DaoParser_LexCode( parser, DString_GetMBS( serial ), 0 );
	if( parser->tokens->size == 0 ) goto Failed;

	DArray_PushFront( types, NULL );
	routine = DaoRoutine_New( ns, NULL, 1 );
	GC_IncRC( routine );
	parser->routine = routine;
	rc = DaoParser_Deserialize( parser, 0, parser->tokens->size-1, self, types, ns, proc, omap );
	if( *self ) DaoProcess_CacheValue( proc, *self );
	parser->routine = NULL;
	GC_DecRC( routine );
	DaoVmSpace_ReleaseParser( ns->vmSpace, parser );
	DArray_Delete( types );
	DMap_Delete( omap );
	return rc;
Failed:
	DaoParser_Delete( parser );
	DArray_Delete( types );
	DMap_Delete( omap );
	return 0;
}


static void NS_Backup( DaoNamespace *self, DaoProcess *proc, FILE *fout, int limit, int store )
{
	DNode *node = DMap_First( self->lookupTable );
	DString *prefix = DString_New(1);
	DString *serial = DString_New(1);
	DaoValue *value = NULL;
	size_t max = limit * 1000; /* limit per object in KB */
	int id, pm, up, st;

	for( ; node !=NULL; node = DMap_Next( self->lookupTable, node ) ){
		DString *name = node->key.pString;
		id = node->value.pInt;
		up = LOOKUP_UP( id );
		st = LOOKUP_ST( id );
		pm = LOOKUP_PM( id );
		if( up ) continue;
		if( st != store ) continue;
		if( st == DAO_GLOBAL_CONSTANT ) value = DaoNamespace_GetConst( self, id );
		if( st == DAO_GLOBAL_VARIABLE ) value = DaoNamespace_GetVariable( self, id );
		if( value == NULL ) continue;
		if( DaoValue_Serialize( value, serial, self, proc ) ==0 ) continue;
		prefix->size = 0;
		switch( pm ){
		case DAO_DATA_PRIVATE   : DString_AppendMBS( prefix, "private " ); break;
		case DAO_DATA_PROTECTED : DString_AppendMBS( prefix, "protected " ); break;
		case DAO_DATA_PUBLIC    : DString_AppendMBS( prefix, "public " ); break;
		}
		switch( st ){
		case DAO_GLOBAL_CONSTANT : DString_AppendMBS( prefix, "const " ); break;
		case DAO_GLOBAL_VARIABLE : DString_AppendMBS( prefix, "var " ); break;
		}
		if( max && prefix->size + name->size + serial->size + 4 > max ) continue;
		fprintf( fout, "%s%s = %s\n", prefix->mbs, name->mbs, serial->mbs );
	}
	DString_Delete( prefix );
	DString_Delete( serial );
}
void DaoNamespace_Backup( DaoNamespace *self, DaoProcess *proc, FILE *fout, int limit )
{
	int i;
	NS_Backup( self, proc, fout, limit, DAO_GLOBAL_CONSTANT );
	if( self->inputs->size ){ /* essential statements and definitions */
		static const char *digits = "ABCDEFGHIJKLMNOP";
		unsigned char *mbs = (unsigned char*) self->inputs->mbs;
		fprintf( fout, "inputs { " );
		for(i=0; i<self->inputs->size; i++){
			fprintf( fout, "%c%c", digits[ mbs[i] / 16 ], digits[ mbs[i] % 16 ] );
		}
		fprintf( fout, " }\n" );
	}
	NS_Backup( self, proc, fout, limit, DAO_GLOBAL_VARIABLE );
}
void DaoNamespace_Restore( DaoNamespace *self, DaoProcess *proc, FILE *fin )
{
	DaoParser *parser = DaoVmSpace_AcquireParser( self->vmSpace );
	DString *line = DString_New(1);
	DArray *types = DArray_New(0);
	DArray *tokens = parser->tokens;
	DMap *omap = DMap_New(0,0);
	DString *name;
	DNode *node;

	parser->nameSpace = self;
	parser->vmSpace = self->vmSpace;
	while( DaoFile_ReadLine( fin, line ) ){
		DaoValue *value = NULL;
		int st = DAO_GLOBAL_VARIABLE;
		int pm = DAO_DATA_PRIVATE;
		int i, n, start = 0;
		char *mbs;

		DaoParser_LexCode( parser, line->mbs, 0 );
		if( tokens->size == 0 ) continue;
		name = & tokens->items.pToken[start]->string;
		if( name->size == 6 && strcmp( name->mbs, "inputs" ) == 0 ){
			if( tokens->size < 3 ) continue;
			DString_Clear( line );
			n = tokens->items.pToken[start+2]->string.size;
			mbs = tokens->items.pToken[start+2]->string.mbs;
			for(i=0; i<n; i++){
				char c1 = mbs[i];
				char c2 = mbs[i+1];
				if( c1 < 'A' || c1 > 'P' ) continue;
				DString_AppendChar( line, (char)((c1-'A')*16 + (c2-'A')) );
				i += 1;
			}
			/* printf( "%s\n", line->mbs ); */
			DaoProcess_Eval( proc, self, line->mbs );
			continue;
		}
		switch( tokens->items.pToken[start]->name ){
		case DKEY_PRIVATE   : pm = DAO_DATA_PRIVATE;   start += 1; break;
		case DKEY_PROTECTED : pm = DAO_DATA_PROTECTED; start += 1; break;
		case DKEY_PUBLIC    : pm = DAO_DATA_PUBLIC;    start += 1; break;
		}
		if( start >= tokens->size ) continue;
		switch( tokens->items.pToken[start]->name ){
		case DKEY_CONST : st = DAO_GLOBAL_CONSTANT; start += 1; break;
		case DKEY_VAR   : st = DAO_GLOBAL_VARIABLE; start += 1; break;
		}
		if( tokens->items.pToken[start]->name != DTOK_IDENTIFIER ) continue;
		name = & tokens->items.pToken[start]->string;
		start += 1;
		if( start + 3 >= tokens->size ) continue;
		if( tokens->items.pToken[start]->name != DTOK_ASSN ) continue;
		start += 1;

		DArray_Clear( parser->errors );
		DArray_Clear( types );
		DArray_PushFront( types, NULL );
		DaoParser_Deserialize( parser, start, tokens->size-1, &value, types, self, proc, omap );
		if( value == NULL ) continue;
		node = DMap_Find( self->lookupTable, name );
		if( node ) continue;
		if( st == DAO_GLOBAL_CONSTANT ){
			DaoNamespace_AddConst( self, name, value, pm );
		}else{
			DaoNamespace_AddVariable( self, name, value, NULL, pm );
		}
	}
	DMap_Delete( omap );
	DString_Delete( line );
	DArray_Delete( types );
	DaoVmSpace_ReleaseParser( self->vmSpace, parser );
}

static void AUX_Serialize( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *mbs = DaoProcess_PutMBString( proc, "" );
	DaoValue_Serialize( p[0], mbs, proc->activeNamespace, proc );
}
static void AUX_Deserialize( DaoProcess *proc, DaoValue *p[], int N )
{
	int top = proc->factory->size;
	DaoValue *value = NULL;
	DaoValue_Deserialize( & value, p[0]->xString.data, proc->activeNamespace, proc );
	DaoProcess_PutValue( proc, value );
	DaoProcess_PopValues( proc, proc->factory->size - top );
	GC_DecRC( value );
}
static void AUX_Backup( DaoProcess *proc, DaoValue *p[], int N )
{
	FILE *fout = fopen( DString_GetMBS( p[0]->xString.data ), "w+" );
	if( fout == NULL ){
		DaoProcess_RaiseException( proc,DAO_ERROR_FILE, DString_GetMBS( p[0]->xString.data ) );
		return;
	}
	DaoNamespace_Backup( proc->activeNamespace, proc, fout, p[1]->xInteger.value );
	fclose( fout );
}
static void AUX_Restore( DaoProcess *proc, DaoValue *p[], int N )
{
	FILE *fin = fopen( DString_GetMBS( p[0]->xString.data ), "r" );
	if( fin == NULL ){
		DaoProcess_RaiseException( proc,DAO_ERROR_FILE, DString_GetMBS( p[0]->xString.data ) );
		return;
	}
	DaoNamespace_Restore( proc->activeNamespace, proc, fin );
	fclose( fin );
}

static DaoFuncItem serializerMeths[]=
{
	{ AUX_Serialize,   "serialize( value : any )=>string" },
	{ AUX_Deserialize, "deserialize( text : string )=>any" },
	{ AUX_Backup,      "backup( tofile = 'backup.sdo', limit=0 )" },
	{ AUX_Restore,     "restore( fromfile = 'backup.sdo' )" },
	{ NULL, NULL }
};


DAO_DLL int DaoSerializer_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	ns = DaoVmSpace_GetNamespace( vmSpace, "std" );
	DaoNamespace_WrapFunctions( ns, serializerMeths );
	return 0;
}

