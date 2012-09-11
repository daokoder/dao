/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2012, Limin Fu
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

#include <math.h>
#include <string.h>
#include <assert.h>
#include "daoBytecode.h"
#include "daoNamespace.h"
#include "daoVmspace.h"
#include "daoValue.h"


#define IntToPointer( x ) ((void*)(size_t)x)

#define DAO_BC_SIGNATURE  "\1Dao\1\2\r\n"


#if 0
#define DEBUG_BC
#endif



DaoByteEncoder* DaoByteEncoder_New()
{
	DaoByteEncoder *self = (DaoByteEncoder*) dao_calloc( 1, sizeof(DaoByteEncoder) );

	self->header = DString_New(1);
	self->source = DString_New(1);
	self->modules = DString_New(1);
	self->identifiers = DString_New(1);
	self->declarations = DString_New(1);
	self->types = DString_New(1);
	self->values = DString_New(1);
	self->constants = DString_New(1);
	self->variables = DString_New(1);
	self->glbtypes = DString_New(1);
	self->interfaces = DString_New(1);
	self->classes = DString_New(1);
	self->routines = DString_New(1);

	self->tmpBytes = DString_New(1);
	self->valueBytes = DString_New(1);
	self->lookups = DArray_New(0);
	self->names = DArray_New(0);
	self->objects = DArray_New(0);
	self->hosts = DArray_New(0);
	self->handled = DHash_New(0,0);

	self->mapLookupHost = DHash_New(0,0);
	self->mapLookupName = DHash_New(0,D_STRING);

	self->mapIdentifiers = DHash_New(D_STRING,0);
	self->mapDeclarations = DHash_New(0,0);
	self->mapTypes = DHash_New(0,0);
	self->mapValues = DHash_New(0,0);
	self->mapValueBytes = DHash_New(D_STRING,0);
	self->mapInterfaces = DHash_New(0,0);
	self->mapClasses = DHash_New(0,0);
	self->mapRoutines = DHash_New(0,0);

	DString_AppendDataMBS( self->header, DAO_BC_SIGNATURE "\0", 9 );
	DString_AppendChar( self->header, sizeof(daoint) == 4 ? '\4' : '\x8' );
	DString_AppendDataMBS( self->header, "\0\0\0\0\0\0", 6 );
	return self;
}
void DaoByteEncoder_Delete( DaoByteEncoder *self )
{
	DString_Delete( self->header );
	DString_Delete( self->source );
	DString_Delete( self->modules );
	DString_Delete( self->identifiers );
	DString_Delete( self->declarations );
	DString_Delete( self->types );
	DString_Delete( self->values );
	DString_Delete( self->constants );
	DString_Delete( self->variables );
	DString_Delete( self->glbtypes );
	DString_Delete( self->interfaces );
	DString_Delete( self->classes );
	DString_Delete( self->routines );

	DString_Delete( self->tmpBytes );
	DString_Delete( self->valueBytes );
	DArray_Delete( self->lookups );
	DArray_Delete( self->names );
	DArray_Delete( self->objects );
	DArray_Delete( self->hosts );
	DMap_Delete( self->handled );

	DMap_Delete( self->mapLookupHost );
	DMap_Delete( self->mapLookupName );

	DMap_Delete( self->mapIdentifiers );
	DMap_Delete( self->mapDeclarations );
	DMap_Delete( self->mapTypes );
	DMap_Delete( self->mapValues );
	DMap_Delete( self->mapValueBytes );
	DMap_Delete( self->mapInterfaces );
	DMap_Delete( self->mapClasses );
	DMap_Delete( self->mapRoutines );
	dao_free( self );
}


static void DString_AppendString( DString *bytecodes, DString *string )
{
	DString_Append( bytecodes, string );
#ifdef DEBUG_BC
	DString_AppendChar( bytecodes, '\n' );
#endif
}
static void DString_AppendBytes( DString *bytecodes, const uchar_t *bytes, int n )
{
#ifdef DEBUG_BC
	const char* hexdigits = "0123456789ABCDEF";
	int i;
	for(i=0; i<n; ++i){
		DString_AppendChar( bytecodes, hexdigits[ bytes[i] >> 4 ] );
		DString_AppendChar( bytecodes, hexdigits[ bytes[i] & 0xF ] );
		DString_AppendChar( bytecodes, ' ' );
	}
	DString_AppendChar( bytecodes, '\n' );
	return;
#endif

	DString_AppendDataMBS( bytecodes, (char*) bytes, n );
}
static void DString_AppendUInt8( DString *bytecodes, int value )
{
	uchar_t bytes[2];
	bytes[0] = value & 0xFF;
	DString_AppendBytes( bytecodes, bytes, 1 );
}
static void DString_AppendUInt16( DString *bytecodes, int value )
{
	uchar_t bytes[2];
	bytes[0] = (value >> 8) & 0xFF;
	bytes[1] = value & 0xFF;
	DString_AppendBytes( bytecodes, bytes, 2 );
}
static void DString_AppendUInt32( DString *bytecodes, uint_t value )
{
	uchar_t bytes[4];
	bytes[0] = (value >> 24) & 0xFF;
	bytes[1] = (value >> 16) & 0xFF;
	bytes[2] = (value >>  8) & 0xFF;
	bytes[3] = value & 0xFF;
	DString_AppendBytes( bytecodes, bytes, 4 );
}
static void DString_AppendDaoInt( DString *bytecodes, daoint value )
{
	uchar_t i, bytes[8];
	uchar_t m = sizeof(daoint);
	for(i=0; i<m; ++i) bytes[i] = (value >> 8*(m-1-i)) & 0xFF;
	DString_AppendBytes( bytecodes, bytes, m );
}
/*
// IEEE 754 double-precision binary floating-point format:
//   sign(1)--exponent(11)------------fraction(52)---------------------
//   S EEEEEEEEEEE FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF  
//   63         52                                                    0
//
//   value = (-1)^S  *  ( 1 + \sigma_0^51 (b_i * 2^{-i-1}) )  *  2^{E-1023}
//
// Exponents 0x000 is used to represent zero (if F=0) and subnormals (if F!=0);
// Exponents 0x7FF is used to represent inf (if F=0) and NaNs (if F!=0);
// Where F is the fraction mantissa.
*/
static void DString_AppendNaN( DString *bytecodes )
{
	DString_AppendUInt32( bytecodes, 0x7FF << 20 );
	DString_AppendUInt32( bytecodes, 1 );
}
static void DString_AppendInf( DString *bytecodes )
{
	DString_AppendUInt32( bytecodes, 0x7FF << 20 );
	DString_AppendUInt32( bytecodes, 0 );
}
static void DString_AppendDouble( DString *bytecodes, double value )
{
	uint_t i = 20, m1 = 0, m2 = 0;
	int first = 1;
	int neg = value < 0.0;
	int expon = 0;
	double frac;

	if( isnan( value ) ){
		DString_AppendNaN( bytecodes );
		return;
	}else if( isinf( value ) ){
		DString_AppendInf( bytecodes );
		return;
	}

	frac = frexp( fabs( value ), & expon );
	frac = 2.0 * frac;
	expon >>= 1;
	while(1){
		double prod = frac * 2.0;
		uint_t bit = (uint_t) bit;
		frac = prod - bit;
		i -= 1;
		if( first ){
			m1 |= bit << i;
			if( i == 0 ) i = 32;
		}else{
			m2 |= bit << i;
			if( i == 0 ) break;
		}
		if( frac <= 0.0 ) break;
	}
	m1 |= expon << 20;
	if( neg ) m1 |= 1 << 31;
	DString_AppendUInt32( bytecodes, m1 );
	DString_AppendUInt32( bytecodes, m2 );
}
static void DString_AppendComplex( DString *bytecodes, complex16 value )
{
	DString_AppendDouble( bytecodes, value.real );
	DString_AppendDouble( bytecodes, value.imag );
}
int DaoByteEncoder_EncodeIdentifier( DaoByteEncoder *self, DString *name )
{
	DNode *node;
	if( name == NULL || name->mbs == NULL ) return 0;
	node = DMap_Find( self->mapIdentifiers, name );
	if( node ) return (int) node->value.pInt;
	DMap_Insert( self->mapIdentifiers, name, IntToPointer( self->mapIdentifiers->size + 1 ) );
	DString_AppendUInt16( self->identifiers, name->size );
	DString_AppendString( self->identifiers, name );
	return self->mapIdentifiers->size;
}

void DaoByteEncoder_AddLookupValue( DaoByteEncoder *self, DaoValue *value, DaoValue *host, DString *name )
{
	if( DMap_Find( self->handled, value ) ) return;
	switch( value->type ){
	case DAO_NAMESPACE :
	case DAO_INTERFACE :
	case DAO_CLASS :
	case DAO_CTYPE :
	case DAO_OBJECT :
	case DAO_CDATA :
		DArray_Append( self->hosts, value );
		DMap_Insert( self->handled, value, NULL );
		DMap_Insert( self->mapLookupHost, value, host );
		DMap_Insert( self->mapLookupName, value, name );
		break;
	}
}
void DaoByteEncoder_SetupLookupData( DaoByteEncoder *self )
{
	DaoTypeKernel *kernel;
	DaoNamespace *ns;
	DaoInterface *inter;
	DaoClass *klass;
	DaoCtype *ctype;
	DNode *it;
	daoint i;

	if( self->nspace == NULL ) return;
	DArray_Append( self->hosts, self->nspace );
	DMap_Insert( self->handled, self->nspace, NULL );
	for(i=0; i<self->hosts->size; ++i){
		DaoValue *value, *host = self->hosts->items.pValue[i];
		switch( host->type ){
		case DAO_NAMESPACE :
			ns = (DaoNamespace*) host;
			for(it=DMap_First(ns->lookupTable); it; it=DMap_Next(ns->lookupTable,it)){
				int st = LOOKUP_ST( it->value.pInt );
				int id = LOOKUP_ID( it->value.pInt );
				if( st != DAO_GLOBAL_CONSTANT ) continue;
				value = ns->constants->items.pConst[id]->value;
				DaoByteEncoder_AddLookupValue( self, value, host, it->key.pString );
			}
			break;
		case DAO_CLASS :
			klass = (DaoClass*) host;
			for(it=DMap_First(klass->lookupTable); it; it=DMap_Next(klass->lookupTable,it)){
				int st = LOOKUP_ST( it->value.pInt );
				int id = LOOKUP_ID( it->value.pInt );
				if( st != DAO_CLASS_CONSTANT ) continue;
				value = klass->constants->items.pConst[id]->value;
				DaoByteEncoder_AddLookupValue( self, value, host, it->key.pString );
			}
			break;
		case DAO_CTYPE :
			ctype = (DaoCtype*) host;
			kernel = ctype->ctype->kernel;
			for(it=DMap_First(kernel->values); it; it=DMap_Next(kernel->values,it)){
				value = it->value.pValue;
				DaoByteEncoder_AddLookupValue( self, value, host, it->key.pString );
			}
			for(it=DMap_First(kernel->methods); it; it=DMap_Next(kernel->methods,it)){
				value = it->value.pValue;
				DaoByteEncoder_AddLookupValue( self, value, host, it->key.pString );
			}
			break;
		case DAO_INTERFACE :
			inter = (DaoInterface*) host;
			for(it=DMap_First(inter->methods); it; it=DMap_Next(inter->methods,it)){
				value = it->value.pValue;
				DaoByteEncoder_AddLookupValue( self, value, host, it->key.pString );
			}
			break;
		}
	}
}

int DaoByteEncoder_EncodeType( DaoByteEncoder *self, DaoType *type );

int DaoByteEncoder_FindDeclaration( DaoByteEncoder *self, DaoValue *object )
{
	DNode *node = DMap_Find( self->mapDeclarations, object );
	if( node ) return node->value.pInt;
	return 0;
}

int DaoByteEncoder_EncodeDeclaration( DaoByteEncoder *self, DaoValue *object )
{
	DaoClass *klass;
	DaoType *type = NULL;
	DString *name = NULL;
	DNode *node, *node2;
	int nameid, nameid2 = 0, hostid = 0, external = 0x1;

	if( object == NULL || object == (DaoValue*) self->nspace ) return 0;

	node = DMap_Find( self->mapDeclarations, object );
	if( node ) return node->value.pInt;

	node = DMap_Find( self->mapLookupHost, object );
	if( node ){
		node2 = DMap_Find( self->mapLookupName, object );
		hostid = DaoByteEncoder_EncodeDeclaration( self, node->value.pValue );
		nameid2 = DaoByteEncoder_EncodeIdentifier( self, node2->value.pString );
	}

	switch( object->type ){
	case DAO_CLASS :
		klass = (DaoClass*) object;
		type = klass->objType;
		name = object->xClass.className;
		if( klass->classRoutine->nameSpace == self->nspace ){
			DArray_Append( self->objects, object );
			external = 0;
		}
		break;
	case DAO_CTYPE :
		type = object->xCtype.ctype;
		name = type->name;
		break;
	case DAO_INTERFACE :
		type = object->xInterface.abtype;
		name = type->name;
		break;
	case DAO_ROUTINE :
		name = object->xRoutine.routName;
		if( object->xRoutine.nameSpace == self->nspace ){
			DArray_Append( self->objects, object );
			external = 0;
		}
		break;
	case DAO_NAMESPACE :
	default : return 0;
	}

	DMap_Insert( self->mapDeclarations, object, IntToPointer( self->mapDeclarations->size+1 ) );
	nameid = DaoByteEncoder_EncodeIdentifier( self, name );
	DString_AppendUInt8( self->declarations, object->type );
	DString_AppendUInt8( self->declarations, external );
	DString_AppendUInt16( self->declarations, nameid );
	DString_AppendUInt16( self->declarations, hostid );
	DString_AppendUInt16( self->declarations, nameid2 );

	if( type ) DaoByteEncoder_EncodeType( self, type );
	return self->mapDeclarations->size;
}
int DaoByteEncoder_EncodeSimpleType( DaoByteEncoder *self, DaoType *type )
{
	int nameid = DaoByteEncoder_EncodeIdentifier( self, type->name );
	DString_AppendUInt8( self->types, type->tid );
	DString_AppendUInt16( self->types, nameid );
	DMap_Insert( self->mapTypes, type, IntToPointer( self->mapTypes->size + 1 ) );
	return self->mapTypes->size;
}
int DaoByteEncoder_EncodeAliasType( DaoByteEncoder *self, DaoType *type, int tid )
{
	int nameid = DaoByteEncoder_EncodeIdentifier( self, type->name );
	DString_AppendUInt8( self->types, 0xFF );
	DString_AppendUInt16( self->types, nameid );
	DString_AppendUInt16( self->types, tid );
	DMap_Insert( self->mapTypes, type, IntToPointer( self->mapTypes->size + 1 ) );
	return self->mapTypes->size;
}

int DaoByteEncoder_EncodeValue( DaoByteEncoder *self, DaoValue *value );

int DaoByteEncoder_EncodeType( DaoByteEncoder *self, DaoType *type )
{
	DNode *node;
	int i, k, n, tpid;
	int nameid = 0;
	int typeid = 0;
	int valueid = 0;

	if( type == NULL ) return 0;
	node = DMap_Find( self->mapTypes, type );
	if( node ) return node->value.pInt;

	switch( type->tid ){
	case DAO_NONE :
	case DAO_INTEGER : case DAO_FLOAT : case DAO_DOUBLE :
	case DAO_COMPLEX : case DAO_LONG  : case DAO_STRING :
		return DaoByteEncoder_EncodeSimpleType( self, type );
	case DAO_ENUM :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		DString_AppendUInt8( self->types, type->flagtype );
		if( type->mapNames ){
			DString_AppendUInt16( self->types, type->mapNames->size );
			for(node=DMap_First(type->mapNames); node; node=DMap_Next(type->mapNames,node)){
				int nameid = DaoByteEncoder_EncodeIdentifier( self, node->key.pString );
				DString_AppendUInt16( self->types, nameid );
				DString_AppendUInt32( self->types, node->value.pInt );
			}
		}else{
			DString_AppendUInt16( self->types, 0 );
		}
		break;
	case DAO_ARRAY : case DAO_LIST : case DAO_MAP :
	case DAO_TUPLE : case DAO_VARIANT :
		for(i=0,n=type->nested->size; i<n; ++i){
			int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i] );
		}
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		DString_AppendUInt16( self->types, type->nested->size );
		for(i=0,n=type->nested->size; i<n; ++i){
			int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i] );
			DString_AppendUInt16( self->types, id );
		}
		break;
	case DAO_VALTYPE :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		valueid = DaoByteEncoder_EncodeValue( self, type->value );
		break;
	case DAO_PAR_NAMED :
	case DAO_PAR_DEFAULT :
		tpid = DaoByteEncoder_EncodeType( self, (DaoType*) type->aux );
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		nameid = DaoByteEncoder_EncodeIdentifier( self, type->fname );
		DString_AppendUInt16( self->types, nameid );
		DString_AppendUInt16( self->types, tpid );
		break;
	case DAO_THT :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		nameid = DaoByteEncoder_EncodeIdentifier( self, type->name );
		DString_AppendUInt16( self->types, nameid );
		break;
	case DAO_ROUTINE :
		tpid = DaoByteEncoder_EncodeType( self, (DaoType*) type->aux );
		for(i=0,n=type->nested->size; i<n; ++i){
			int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i] );
		}
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		DString_AppendUInt16( self->types, tpid );
		DString_AppendUInt16( self->types, type->nested->size );
		for(i=0,n=type->nested->size; i<n; ++i){
			int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i] );
			DString_AppendUInt16( self->types, id );
		}
		break;
	case DAO_OBJECT :
	case DAO_CDATA :
	case DAO_CLASS :
	case DAO_CTYPE :
	case DAO_INTERFACE :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		k = DaoByteEncoder_EncodeDeclaration( self, type->aux );
		DString_AppendUInt16( self->types, k );
		break;
	case DAO_TYPE :
		tpid = DaoByteEncoder_EncodeType( self, (DaoType*) type->aux );
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		DString_AppendUInt16( self->types, tpid );
		break;
	case DAO_FUTURE :
		tpid = DaoByteEncoder_EncodeType( self, type->nested->items.pType[0] );
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		DString_AppendUInt16( self->types, tpid );
		break;
	}
	return typeid;
}
int DaoByteEncoder_EncodeType2( DaoByteEncoder *self, DaoType *type, DString *alias )
{
	int typeid = DaoByteEncoder_EncodeType( self, type );
	if( alias == NULL || DString_EQ( type->name, alias ) ) return typeid;
	return DaoByteEncoder_EncodeAliasType( self, type, typeid );
}
void DaoByteEncoder_EncodeValue2( DaoByteEncoder *self, DaoValue *value )
{
	DNode *it;
	DaoValue *va;
	DaoArray *array;
	DaoObject *object;
	DString *valueBytes = self->valueBytes;
	daoint i, n, id, typeid, valueid;

	self->valueBytes->size = 0;
	switch( value->type ){
	case DAO_NONE :
		DString_AppendUInt8( valueBytes, value->type );
		break;
	case DAO_INTEGER :
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendDaoInt( valueBytes, value->xInteger.value );
		break;
	case DAO_FLOAT :
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendDouble( valueBytes, value->xFloat.value );
		break;
	case DAO_DOUBLE :
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendDouble( valueBytes, value->xDouble.value );
		break;
	case DAO_COMPLEX :
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendComplex( valueBytes, value->xComplex.value );
		break;
	case DAO_LONG :
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt8( valueBytes, value->xLong.value->sign < 0 );
		for(i=0; i<value->xLong.value->size; ++i){
			DString_AppendUInt8( valueBytes, value->xLong.value->data[i] );
		}
		break;
	case DAO_STRING :
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt8( valueBytes, value->xString.data->mbs != NULL );
		DString_AppendDaoInt( valueBytes, value->xString.data->size );
		if( value->xString.data->mbs ){
			DString_AppendString( valueBytes, value->xString.data );
		}else{
			for(i=0; i<value->xString.data->size; ++i){
				DString_AppendUInt32( valueBytes, value->xString.data->wcs[i] );
			}
		}
		break;
	case DAO_ENUM :
		DString_AppendUInt8( valueBytes, value->type );
		typeid = DaoByteEncoder_EncodeType( self, value->xEnum.etype );
		DString_AppendUInt16( valueBytes, typeid );
		DString_AppendUInt32( valueBytes, value->xEnum.value );
		break;
	case DAO_ARRAY :
		array = (DaoArray*) value;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt8( valueBytes, array->etype );
		DString_AppendDaoInt( valueBytes, array->size );
		DString_AppendUInt16( valueBytes, array->ndim );
		for(i=0; i<array->ndim; ++i) DString_AppendDaoInt( valueBytes, array->dims[i] );
		for(i=0; i<array->size; ++i){
			switch( array->etype ){
			case DAO_INTEGER : DString_AppendDaoInt( valueBytes, array->data.i[i] ); break;
			case DAO_FLOAT   : DString_AppendDouble( valueBytes, array->data.f[i] ); break;
			case DAO_DOUBLE  : DString_AppendDouble( valueBytes, array->data.d[i] ); break;
			case DAO_COMPLEX : DString_AppendComplex( valueBytes, array->data.c[i] ); break;
			}
		}
		break;
	case DAO_LIST :
		for(i=0; i<value->xList.items.size; ++i){
			DaoByteEncoder_EncodeValue( self, value->xList.items.items.pValue[i] );
		}
		typeid = DaoByteEncoder_EncodeType( self, value->xList.unitype );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt16( valueBytes, typeid );
		DString_AppendDaoInt( valueBytes, value->xList.items.size );
		for(i=0; i<value->xList.items.size; ++i){
			valueid = DaoByteEncoder_EncodeValue( self, value->xList.items.items.pValue[i] );
			DString_AppendUInt16( valueBytes, valueid );
		}
		break;
	case DAO_MAP :
		for(it=DMap_First(value->xMap.items); it; it=DMap_Next(value->xMap.items,it) ){
			DaoByteEncoder_EncodeValue( self, it->key.pValue );
			DaoByteEncoder_EncodeValue( self, it->value.pValue );
		}
		typeid = DaoByteEncoder_EncodeType( self, value->xMap.unitype );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt16( valueBytes, typeid );
		DString_AppendDaoInt( valueBytes, value->xMap.items->size );
		for(it=DMap_First(value->xMap.items); it; it=DMap_Next(value->xMap.items,it) ){
			int ik = DaoByteEncoder_EncodeValue( self, it->key.pValue );
			int iv = DaoByteEncoder_EncodeValue( self, it->value.pValue );
			DString_AppendUInt16( valueBytes, ik );
			DString_AppendUInt16( valueBytes, iv );
		}
		break;
	case DAO_TUPLE :
		for(i=0; i<value->xTuple.size; ++i){
			DaoByteEncoder_EncodeValue( self, value->xTuple.items[i] );
		}
		typeid = DaoByteEncoder_EncodeType( self, value->xTuple.unitype );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt16( valueBytes, typeid );
		DString_AppendUInt32( valueBytes, value->xTuple.size );
		for(i=0; i<value->xTuple.size; ++i){
			valueid = DaoByteEncoder_EncodeValue( self, value->xTuple.items[i] );
			DString_AppendUInt16( valueBytes, valueid );
		}
		break;
	case DAO_OBJECT :
		object = (DaoObject*) value;
		va = (DaoValue*) object->defClass;
		id = DaoByteEncoder_EncodeDeclaration( self, va );
		if( value == object->defClass->objType->value ){
			DString_AppendUInt8( valueBytes, value->type );
			DString_AppendUInt8( valueBytes, 0x2 );
			DString_AppendUInt16( valueBytes, id );
			break;
		}
		if( object->rootObject == object ){ /* isRoot??? */
			for(i=0; i<object->valueCount; ++i){
				DaoByteEncoder_EncodeValue( self, object->objValues[i] );
			}
		}else{
			DaoByteEncoder_EncodeValue( self, (DaoValue*) object->rootObject );
		}
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt8( valueBytes, object->rootObject != object );
		DString_AppendUInt16( valueBytes, id );
		DString_AppendDaoInt( valueBytes, value->xTuple.size );
		if( object->rootObject == object ){ /* isRoot??? */
			for(i=0; i<object->valueCount; ++i){
				valueid = DaoByteEncoder_EncodeValue( self, object->objValues[i] );
				DString_AppendUInt16( valueBytes, valueid );
			}
		}else{
			valueid = DaoByteEncoder_EncodeValue( self, (DaoValue*) object->rootObject );
			DString_AppendUInt16( valueBytes, valueid );
		}
		break;
	case DAO_ROUTINE :
		id = DaoByteEncoder_EncodeDeclaration( self, value );
		typeid = DaoByteEncoder_EncodeType( self, value->xRoutine.routType );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt16( valueBytes, id );
		DString_AppendUInt16( valueBytes, typeid );
		break;
	case DAO_CLASS :
	case DAO_CTYPE :
	case DAO_INTERFACE :
	case DAO_NAMESPACE : // XXX
		id = DaoByteEncoder_EncodeDeclaration( self, value );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt16( valueBytes, id );
		break;
	case DAO_TYPE :
		typeid = DaoByteEncoder_EncodeType( self, (DaoType*) value->xType.aux );
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt16( valueBytes, typeid );
		break;
	}
}
int DaoByteEncoder_EncodeValue( DaoByteEncoder *self, DaoValue *value )
{
	DNode *node;
	if( value == NULL ) return 0;
	node = DMap_Find( self->mapValues, value );
	if( node ) return node->value.pInt;

	/* Insert first avoid loop recursion: */
	DMap_Insert( self->mapValues, value, IntToPointer( self->valueCount ) );

	DaoByteEncoder_EncodeValue2( self, value );
	node = DMap_Find( self->mapValueBytes, self->valueBytes );
	if( node ){
		DMap_Insert( self->mapValues, value, node->value.pVoid );
		return node->value.pInt;
	}
	self->valueCount += 1;
	DString_Append( self->values, self->valueBytes );
	DMap_Insert( self->mapValues, value, IntToPointer( self->valueCount ) );
	DMap_Insert( self->mapValueBytes, self->valueBytes, IntToPointer( self->valueCount ) );
	return self->valueCount;
}

void DaoByteEncoder_EncodeConstant( DaoByteEncoder *self, DString *name, DaoValue *value, int pm )
{
	int nameid = DaoByteEncoder_EncodeIdentifier( self, name );
	int valueid = DaoByteEncoder_EncodeValue( self, value );
	self->valueBytes->size = 0;
	DString_AppendUInt16( self->valueBytes, nameid );
	DString_AppendUInt8( self->valueBytes, pm );
	DString_AppendUInt32( self->valueBytes, valueid );
}

void DaoByteEncoder_EncodeVariable( DaoByteEncoder *self, DString *name, DaoType *type, DaoValue *value, int pm )
{
	int nameid = DaoByteEncoder_EncodeIdentifier( self, name );
	int typeid = DaoByteEncoder_EncodeType( self, type );
	int valueid = DaoByteEncoder_EncodeValue( self, value );
	self->valueBytes->size = 0;
	DString_AppendUInt16( self->valueBytes, nameid );
	DString_AppendUInt8( self->valueBytes, pm );
	DString_AppendUInt16( self->valueBytes, typeid );
	DString_AppendUInt32( self->valueBytes, valueid );
}

void DaoByteEncoder_GetLookupName( int size, DMap *lookupTable, DArray *lookups, DArray *names, int st )
{
	DNode *it;

	lookups->size = 0;
	names->size = 0;
	DArray_Resize( lookups, size, IntToPointer(-1) );
	DArray_Resize( names, size, NULL );
	for(it=DMap_First(lookupTable); it; it=DMap_Next(lookupTable,it)){
		int id = LOOKUP_ID( it->value.pInt );
		if( LOOKUP_ST( it->value.pInt ) != st ) continue;
		lookups->items.pInt[id] = it->value.pInt;
		lookups->items.pString[id] = it->value.pString;
	}
}

void DaoByteEncoder_EncodeInterface( DaoByteEncoder *self, DaoInterface *interface )
{
	DNode *it;
	int i, id;

	if( DMap_Find( self->mapInterfaces, interface ) ) return;
	DMap_Insert( self->mapInterfaces, interface, IntToPointer( self->mapInterfaces->size+1 ) );

	id = DaoByteEncoder_FindDeclaration( self, (DaoValue*)interface );
	assert( id > 0 );

	DString_AppendUInt16( self->interfaces, id );
	DString_AppendUInt16( self->interfaces, interface->supers->size );
	for(i=0; i<interface->supers->size; ++i){
		DaoValue *value = interface->supers->items.pValue[i];
		id = DaoByteEncoder_EncodeDeclaration( self, value );
		DString_AppendUInt16( self->interfaces, id );
	}
	DString_AppendUInt16( self->interfaces, interface->methods->size );
	for(it=DMap_First(interface->methods); it; it=DMap_Next(interface->methods,it)){
		int nameid = DaoByteEncoder_EncodeIdentifier( self, it->key.pString );
		int routid = DaoByteEncoder_EncodeDeclaration( self, it->value.pValue );
		DString_AppendUInt16( self->interfaces, nameid );
		DString_AppendUInt16( self->interfaces, routid );
	}
}

void DaoByteEncoder_EncodeClass( DaoByteEncoder *self, DaoClass *klass )
{
	DNode *it;
	DMap *abstypes;
	int i, id, count;

	if( DMap_Find( self->mapClasses, klass ) ) return;
	DMap_Insert( self->mapClasses, klass, IntToPointer( self->mapClasses->size + 1 ) );

	id = DaoByteEncoder_FindDeclaration( self, (DaoValue*)klass );
	assert( id > 0 );

	DString_AppendUInt16( self->classes, id );
	DString_AppendUInt16( self->classes, klass->superClass->size );
	for(i=0; i<klass->superClass->size; ++i){
		DaoValue *value = klass->superClass->items.pValue[i];
		id = DaoByteEncoder_EncodeDeclaration( self, value );
		DString_AppendUInt16( self->classes, id );
	}

	count = 0;
	self->tmpBytes->size = 0;
	for(i=0; i<klass->cstDataName->size; ++i){
		DaoValue *value = klass->constants->items.pConst[i]->value;
		DString *name = klass->cstDataName->items.pString[i];
		DNode *node = MAP_Find( klass->lookupTable, name );
		if( LOOKUP_UP( node->value.pInt ) ) continue;
		DaoByteEncoder_EncodeDeclaration( self, value );
		DaoByteEncoder_EncodeConstant( self, name, value, LOOKUP_PM( node->value.pInt ) );
		DString_Append( self->tmpBytes, self->valueBytes );
		count += 1;
	}
	DString_AppendUInt16( self->classes, count );
	DString_Append( self->classes, self->tmpBytes );

	count = 0;
	self->tmpBytes->size = 0;
	for(i=0; i<klass->glbDataName->size; ++i){
		DaoVariable *var = klass->variables->items.pVar[i];
		DString *name = klass->glbDataName->items.pString[i];
		DNode *node = MAP_Find( klass->lookupTable, name );
		int pm = LOOKUP_PM( node->value.pInt );
		if( LOOKUP_UP( node->value.pInt ) ) continue;
		DaoByteEncoder_EncodeDeclaration( self, var->value );
		DaoByteEncoder_EncodeVariable( self, name, var->dtype, var->value, pm );
		DString_Append( self->tmpBytes, self->valueBytes );
		count += 1;
	}
	DString_AppendUInt16( self->classes, count );
	DString_Append( self->classes, self->tmpBytes );

	count = 0;
	self->tmpBytes->size = 0;
	for(i=0; i<klass->objDataName->size; ++i){
		DaoValue *value = klass->objDataDefault->items.pValue[i];
		DaoType *type = klass->objDataType->items.pType[i];
		DString *name = klass->objDataName->items.pString[i];
		DNode *node = MAP_Find( klass->lookupTable, name );
		int pm = LOOKUP_PM( node->value.pInt );
		if( LOOKUP_UP( node->value.pInt ) ) continue;
		DaoByteEncoder_EncodeDeclaration( self, value );
		DaoByteEncoder_EncodeVariable( self, name, type, value, pm );
		DString_Append( self->tmpBytes, self->valueBytes );
		count += 1;
	}
	DString_AppendUInt16( self->classes, count );
	DString_Append( self->classes, self->tmpBytes );

	abstypes = klass->abstypes;
	DString_AppendUInt16( self->classes, abstypes->size );
	for(it=DMap_First(abstypes); it; it=DMap_Next(abstypes,it)){
		int nameid = DaoByteEncoder_EncodeIdentifier( self, it->key.pString );
		int typeid = DaoByteEncoder_EncodeType( self, it->value.pType );
		DString_AppendUInt16( self->classes, nameid );
		DString_AppendUInt16( self->classes, typeid );
	}
}

void DaoByteEncoder_EncodeRoutine( DaoByteEncoder *self, DaoRoutine *routine )
{
	DaoType *type;
	DMap *abstypes;
	DNode *it;
	int i, id;

	if( routine->body == NULL ) return;
	if( DMap_Find( self->mapRoutines, routine ) ) return;
	DMap_Insert( self->mapRoutines, routine, IntToPointer( self->mapRoutines->size + 1 ) );

	id = DaoByteEncoder_FindDeclaration( self, (DaoValue*)routine );
	assert( id > 0 );

#ifdef DEBUG_BC
	DString_Append( self->routines, routine->routName );
	DString_AppendMBS( self->routines, "()====\n" );
#endif

	DString_AppendUInt16( self->routines, id );
	if( routine->routHost ){
		id = DaoByteEncoder_FindDeclaration( self, routine->routHost->aux );
		DString_AppendUInt16( self->routines, id );
	}else{
		DString_AppendUInt16( self->routines, 0 );
	}
	id = DaoByteEncoder_EncodeIdentifier( self, routine->routName );
	DString_AppendUInt16( self->routines, id );
	DString_AppendUInt16( self->routines, routine->attribs );
	DString_AppendUInt16( self->routines, routine->defLine );

	abstypes = routine->body->abstypes;
	for(it=DMap_First(abstypes); it; it=DMap_Next(abstypes,it)){
		DaoByteEncoder_EncodeType( self, it->value.pType );
	}
	DString_AppendUInt16( self->routines, routine->routConsts->items.size );
	for(i=0; i<routine->routConsts->items.size; ++i){
		DaoValue *value = routine->routConsts->items.items.pValue[i];
		id = DaoByteEncoder_EncodeValue( self, value );
		DString_AppendUInt16( self->routines, id );
	}
	DString_AppendUInt16( self->routines, routine->body->regType->size );
	for(i=0; i<routine->body->regType->size; ++i){
		type = routine->body->regType->items.pType[i];
		id = DaoByteEncoder_EncodeType( self, type );
		DString_AppendUInt16( self->routines, id );
	}

#ifdef DEBUG_BC
	DString_AppendMBS( self->routines, "Codes----\n" );
#endif

	DString_AppendUInt16( self->routines, routine->body->annotCodes->size );
	for(i=0; i<routine->body->annotCodes->size; ++i){
		DaoVmCodeX *vmc = routine->body->annotCodes->items.pVmc[i];
		DString_AppendUInt16( self->routines, vmc->code );
		DString_AppendUInt16( self->routines, vmc->a );
		DString_AppendUInt16( self->routines, vmc->b );
		DString_AppendUInt16( self->routines, vmc->c );
		DString_AppendUInt16( self->routines, vmc->level );
		DString_AppendUInt16( self->routines, vmc->line );
	}
}

void DaoByteEncoder_EncodeNamespace( DaoByteEncoder *self, DaoNamespace *nspace )
{
	DNode *it;
	DMap *abstypes;
	DArray *constants = nspace->constants;
	DArray *variables = nspace->variables;
	DMap *lookupTable = nspace->lookupTable;
	daoint i, n, st, pm, up, id;

	abstypes = nspace->abstypes;
	for(it=DMap_First(abstypes); it; it=DMap_Next(abstypes,it)){
		int nameid = DaoByteEncoder_EncodeIdentifier( self, it->key.pString );
		int typeid = DaoByteEncoder_EncodeType( self, it->value.pType );
		DString_AppendUInt16( self->glbtypes, nameid );
		DString_AppendUInt16( self->glbtypes, typeid );
	}

	DaoByteEncoder_GetLookupName( constants->size, lookupTable, self->lookups, self->names, DAO_GLOBAL_CONSTANT );
	for(i=0, n=constants->size; i<n; ++i){
		DaoValue *value = constants->items.pConst[i]->value;
		DString *name = self->names->items.pString[i];
		int pm = LOOKUP_PM( self->lookups->items.pInt[i] );
		int up = LOOKUP_UP( self->lookups->items.pInt[i] );
		DaoByteEncoder_EncodeDeclaration( self, value );
		if( up ) continue;
		DaoByteEncoder_EncodeConstant( self, name, value, pm );
		DString_Append( self->constants, self->valueBytes );
		self->constCount += 1;
	}
	DaoByteEncoder_GetLookupName( variables->size, lookupTable, self->lookups, self->names, DAO_GLOBAL_VARIABLE );
	for(i=0, n=variables->size; i<n; ++i){
		DaoVariable *var = variables->items.pVar[i];
		DString *name = self->names->items.pString[i];
		int pm = LOOKUP_PM( self->lookups->items.pInt[i] );
		int up = LOOKUP_UP( self->lookups->items.pInt[i] );
		DaoByteEncoder_EncodeDeclaration( self, var->value );
		if( up ) continue;
		DaoByteEncoder_EncodeVariable( self, name, var->dtype, var->value, pm );
		DString_Append( self->variables, self->valueBytes );
		self->varCount += 1;
	}
	for(i=0; i<self->objects->size; ++i){
		DaoValue *value = self->objects->items.pValue[i];
		switch( value->type ){
		case DAO_INTERFACE : DaoByteEncoder_EncodeInterface( self, (DaoInterface*) value ); break;
		case DAO_ROUTINE : DaoByteEncoder_EncodeRoutine( self, (DaoRoutine*) value ); break;
		case DAO_CLASS : DaoByteEncoder_EncodeClass( self, (DaoClass*) value ); break;
		}
	}
}
void DaoByteEncoder_Encode( DaoByteEncoder *self, DaoNamespace *nspace, DString *output )
{
	self->nspace = nspace;

	DaoByteEncoder_EncodeNamespace( self, nspace );

	/* Header: */
	DString_Append( output, self->header );

	/* Source: */
	DString_AppendUInt16( output, nspace->name->size );
	DString_AppendString( output, nspace->name );


#ifdef DEBUG_BC
	DString_AppendMBS( output, "Modules:\n" );
	DString_AppendUInt16( output, 0 );
	DString_AppendMBS( output, "Identifier:\n" );
	DString_AppendUInt16( output, self->mapIdentifiers->size );
	DString_Append( output, self->identifiers );
	DString_AppendMBS( output, "Declarations:\n" );
	DString_AppendUInt16( output, self->mapDeclarations->size );
	DString_Append( output, self->declarations );
	DString_AppendMBS( output, "Types:\n" );
	DString_AppendUInt16( output, self->mapTypes->size );
	DString_Append( output, self->types );
	DString_AppendMBS( output, "Values:\n" );
	DString_AppendUInt32( output, self->valueCount );
	DString_Append( output, self->values );
	DString_AppendMBS( output, "Global Constants:\n" );
	DString_AppendUInt16( output, self->constCount );
	DString_Append( output, self->constants );
	DString_AppendMBS( output, "Global Variables:\n" );
	DString_AppendUInt16( output, self->varCount );
	DString_Append( output, self->variables );
	DString_AppendMBS( output, "Global Types:\n" );
	DString_AppendUInt16( output, nspace->abstypes->size );
	DString_Append( output, self->glbtypes );
	DString_AppendMBS( output, "Interfaces:\n" );
	DString_AppendUInt16( output, self->mapInterfaces->size );
	DString_Append( output, self->interfaces );
	DString_AppendMBS( output, "Classes:\n" );
	DString_AppendUInt16( output, self->mapClasses->size );
	DString_Append( output, self->classes );
	DString_AppendMBS( output, "Routines:\n" );
	DString_AppendUInt16( output, self->mapRoutines->size );
	DString_Append( output, self->routines );
	return;
#endif

	/* Modules: */
	DString_AppendUInt16( output, 0 );

	/* Identifiers: */
	DString_AppendUInt16( output, self->mapIdentifiers->size );
	DString_Append( output, self->identifiers );

	/* Declarations: */
	DString_AppendUInt16( output, self->mapDeclarations->size );
	DString_Append( output, self->declarations );

	/* Types: */
	DString_AppendUInt16( output, self->mapTypes->size );
	DString_Append( output, self->types );

	/* Values: */
	DString_AppendUInt32( output, self->valueCount );
	DString_Append( output, self->values );

	/* Global Constants: */
	DString_AppendUInt16( output, self->constCount );
	DString_Append( output, self->constants );

	/* Global Variables: */
	DString_AppendUInt16( output, self->varCount );
	DString_Append( output, self->variables );

	/* Global Types: */
	DString_AppendUInt16( output, nspace->abstypes->size );
	DString_Append( output, self->glbtypes );

	/* Interfaces: */
	DString_AppendUInt16( output, self->mapInterfaces->size );
	DString_Append( output, self->interfaces );

	/* Classes: */
	DString_AppendUInt16( output, self->mapClasses->size );
	DString_Append( output, self->classes );

	/* Routines: */
	DString_AppendUInt16( output, self->mapRoutines->size );
	DString_Append( output, self->routines );
}





DaoByteDecoder* DaoByteDecoder_New( DaoVmSpace *vmspace )
{
	DaoByteDecoder *self = (DaoByteDecoder*) dao_calloc( 1, sizeof(DaoByteDecoder) );
	self->vmspace = vmspace;

	self->identifiers = DArray_New(D_STRING);
	self->declarations = DArray_New(D_VALUE);
	self->types = DArray_New(D_VALUE);
	self->values = DArray_New(D_VALUE);
	self->interfaces = DArray_New(D_VALUE);
	self->classes = DArray_New(D_VALUE);
	self->routines = DArray_New(D_VALUE);
	self->string = DString_New(1);

	return self;
}
void DaoByteDecoder_Delete( DaoByteDecoder *self )
{
	DArray_Delete( self->identifiers );
	DArray_Delete( self->declarations );
	DArray_Delete( self->types );
	DArray_Delete( self->values );
	DArray_Delete( self->interfaces );
	DArray_Delete( self->classes );
	DArray_Delete( self->routines );
	DString_Delete( self->string );
	dao_free( self );
}

int DaoByteDecoder_DecodeUInt8( DaoByteDecoder *self )
{
	int value = self->codes[0];
	self->codes += 1;
	return value;
}
int DaoByteDecoder_DecodeUInt16( DaoByteDecoder *self )
{
	int value = (self->codes[0]<<8) + self->codes[1];
	self->codes += 2;
	return value;
}
uint_t DaoByteDecoder_DecodeUInt32( DaoByteDecoder *self )
{
	uint_t value = self->codes[0] << 24;
	value += self->codes[1] << 16;
	value += self->codes[2] << 8;
	value += self->codes[3];
	self->codes += 4;
	return value;
}
daoint DaoByteDecoder_DecodeDaoInt( DaoByteDecoder *self )
{
	daoint value = 0;
	uchar_t i, m = sizeof(daoint);
	for(i=0; i<m; ++i) value += self->codes[i] << 8*(m-1-i);
	self->codes += m;
	return value;
}
double DaoByteDecoder_DecodeDouble( DaoByteDecoder *self )
{
	double value = 0.0;
	self->codes += 8;
	return value;
}
complex16 DaoByteDecoder_DecodeComplex( DaoByteDecoder *self )
{
	complex16 value = {0.0,0.0};
	value.real = DaoByteDecoder_DecodeDouble( self );
	value.imag = DaoByteDecoder_DecodeDouble( self );
	return value;
}
void DaoByteDecoder_DecodeShortString( DaoByteDecoder *self, DString *string )
{
	int len = DaoByteDecoder_DecodeUInt16( self );
	DString_Reset( string, 0 );
	DString_AppendDataMBS( string, (char*) self->codes, len );
	self->codes += len;
}

void DaoByteDecoder_DecodeModules( DaoByteDecoder *self )
{
	int i, count = DaoByteDecoder_DecodeUInt16( self );
	for(i=0; i<count; ++i){
		DaoByteDecoder_DecodeShortString( self, self->string );
		// TODO: load modules;
	}
}
void DaoByteDecoder_DecodeIdentifiers( DaoByteDecoder *self )
{
	int i, count = DaoByteDecoder_DecodeUInt16( self );
	for(i=0; i<count; ++i){
		DaoByteDecoder_DecodeShortString( self, self->string );
		DArray_Append( self->identifiers, self->string );
		//printf( "%3i : %s\n", i, self->string->mbs );
	}
}
void DaoByteDecoder_DecodeDeclarations( DaoByteDecoder *self )
{
	int i, count = DaoByteDecoder_DecodeUInt16( self );
	for(i=0; i<count; ++i){
		int type = DaoByteDecoder_DecodeUInt8( self );
		int flag = DaoByteDecoder_DecodeUInt8( self );
		int name = DaoByteDecoder_DecodeUInt16( self );
		int host = DaoByteDecoder_DecodeUInt16( self );
		int field = DaoByteDecoder_DecodeUInt16( self );
		printf( "%3i : %s\n", i, self->identifiers->items.pString[name-1]->mbs );
		if( field ) printf( "%3i : %s\n", i, self->identifiers->items.pString[field-1]->mbs );
		if( flag ){
		}
		//DArray_Append( self->identifiers, self->string );
	}
}
void DaoByteDecoder_DecodeTypes( DaoByteDecoder *self )
{
	int i, j, flag, count, numtypes = DaoByteDecoder_DecodeUInt16( self );
	int id, id2, id3;
	printf( "DaoByteDecoder_DecodeTypes: %3i\n", numtypes );
	for(i=0; i<numtypes; ++i){
		int tid = DaoByteDecoder_DecodeUInt8( self );
		int name = DaoByteDecoder_DecodeUInt16( self );
		printf( "%3i: %3i -----------\n", i, tid );
		if( name ) printf( "%3i : %s\n", i, self->identifiers->items.pString[name-1]->mbs );
		switch( tid ){
		case DAO_NONE :
		case DAO_INTEGER :
		case DAO_FLOAT :
		case DAO_DOUBLE :
		case DAO_COMPLEX :
		case DAO_LONG :
		case DAO_STRING :
			break;
		case DAO_ENUM :
			flag = DaoByteDecoder_DecodeUInt8( self );
			count = DaoByteDecoder_DecodeUInt16( self );
			printf( "%3i: %3i\n", i, count );
			for(j=0; j<count; ++j){
				int sym = DaoByteDecoder_DecodeUInt16( self );
				int val = DaoByteDecoder_DecodeUInt32( self );
			}
			break;
		case DAO_ARRAY :
		case DAO_LIST :
		case DAO_MAP :
		case DAO_TUPLE :
		case DAO_VARIANT :
			count = DaoByteDecoder_DecodeUInt16( self );
			for(j=0; j<count; ++j){
				id = DaoByteDecoder_DecodeUInt16( self );
			}
			break;
		case DAO_VALTYPE :
			id = DaoByteDecoder_DecodeUInt32( self );
			break;
		case DAO_PAR_NAMED :
		case DAO_PAR_DEFAULT :
			id = DaoByteDecoder_DecodeUInt16( self );
			id2 = DaoByteDecoder_DecodeUInt16( self );
			break;
		case DAO_THT :
			break;
		case DAO_ROUTINE :
			id = DaoByteDecoder_DecodeUInt16( self );
			count = DaoByteDecoder_DecodeUInt16( self );
			printf( "count = %i\n", count );
			for(j=0; j<count; ++j){
				id = DaoByteDecoder_DecodeUInt16( self );
			}
			break;
		case DAO_INTERFACE :
		case DAO_CLASS :
		case DAO_OBJECT :
		case DAO_CTYPE :
		case DAO_CDATA :
			id = DaoByteDecoder_DecodeUInt16( self );
			break;
		case DAO_TYPE :
			id = DaoByteDecoder_DecodeUInt16( self );
			break;
		case DAO_FUTURE :
			id = DaoByteDecoder_DecodeUInt16( self );
			break;
		}
	}
}
void DaoByteDecoder_DecodeValues( DaoByteDecoder *self )
{
	double fvalue;
	complex16 cvalue;
	daoint k, ivalue, ivalue2;
	int numvalues = DaoByteDecoder_DecodeUInt32( self );
	int i, j, flag, count;
	int id, id2, id3;
	printf( "DaoByteDecoder_DecodeValues: %3i\n", numvalues );
	for(i=0; i<numvalues; ++i){
		DaoValue *value = NULL;
		int tid = DaoByteDecoder_DecodeUInt8( self );
		printf( "%3i: %3i\n", i, tid );
		switch( tid ){
		case DAO_NONE :
			value = dao_none_value;
			break;
		case DAO_INTEGER :
			ivalue = DaoByteDecoder_DecodeDaoInt( self );
			break;
		case DAO_FLOAT :
			fvalue = DaoByteDecoder_DecodeDouble( self );
			break;
		case DAO_DOUBLE :
			fvalue = DaoByteDecoder_DecodeDouble( self );
			break;
		case DAO_COMPLEX :
			cvalue = DaoByteDecoder_DecodeComplex( self );
			break;
		case DAO_LONG :
			break;
		case DAO_STRING :
			flag = DaoByteDecoder_DecodeUInt8( self );
			ivalue = DaoByteDecoder_DecodeDaoInt( self );
			if( flag ){
				self->codes += ivalue;
			}else{
				for(j =0; j<ivalue; ++j){
					wchar_t ch = DaoByteDecoder_DecodeUInt32( self );
				}
			}
			break;
		case DAO_ENUM :
			id = DaoByteDecoder_DecodeUInt16( self );
			id2 = DaoByteDecoder_DecodeUInt32( self );
			break;
		case DAO_ARRAY :
			flag = DaoByteDecoder_DecodeUInt8( self );
			ivalue = DaoByteDecoder_DecodeDaoInt( self );
			count = DaoByteDecoder_DecodeUInt16( self );
			for(j=0; j<count; ++j){
				daoint dim = DaoByteDecoder_DecodeDaoInt( self );
			}
			for(k=0; k<ivalue; ++k){
				switch( flag ){
				case DAO_INTEGER :
					ivalue2 = DaoByteDecoder_DecodeDaoInt( self );
					break;
				case DAO_FLOAT :
					fvalue = DaoByteDecoder_DecodeDouble( self );
					break;
				case DAO_DOUBLE :
					fvalue = DaoByteDecoder_DecodeDouble( self );
					break;
				case DAO_COMPLEX :
					cvalue = DaoByteDecoder_DecodeComplex( self );
					break;
				}
			}
			break;
		case DAO_LIST :
			id = DaoByteDecoder_DecodeUInt16( self );
			ivalue = DaoByteDecoder_DecodeDaoInt( self );
			for(k=0; k<ivalue; ++k){
				uint_t it = DaoByteDecoder_DecodeUInt32( self );
			}
			break;
		case DAO_MAP :
			flag = DaoByteDecoder_DecodeUInt8( self );
			id = DaoByteDecoder_DecodeUInt16( self );
			ivalue = DaoByteDecoder_DecodeDaoInt( self );
			for(k=0; k<ivalue; ++k){
				uint_t it = DaoByteDecoder_DecodeUInt32( self );
			}
			break;
		case DAO_TUPLE :
			id = DaoByteDecoder_DecodeUInt16( self );
			ivalue = DaoByteDecoder_DecodeUInt32( self );
			for(k=0; k<ivalue; ++k){
				uint_t it = DaoByteDecoder_DecodeUInt32( self );
			}
			break;
		case DAO_ROUTINE :
			id = DaoByteDecoder_DecodeUInt16( self );
			id2 = DaoByteDecoder_DecodeUInt16( self );
			break;
		case DAO_OBJECT :
			flag = DaoByteDecoder_DecodeUInt8( self );
			id = DaoByteDecoder_DecodeUInt16( self );
			if( flag == 0x2 ){
				break;
			}
			break;
		case DAO_INTERFACE :
		case DAO_CLASS :
		case DAO_CTYPE :
		case DAO_CDATA :
			id = DaoByteDecoder_DecodeUInt16( self );
			break;
		case DAO_TYPE :
			id = DaoByteDecoder_DecodeUInt16( self );
			break;
		case DAO_FUTURE :
			id = DaoByteDecoder_DecodeUInt16( self );
			break;
		case DAO_NAMESPACE : // XXX
			id = DaoByteDecoder_DecodeUInt16( self );
			break;
		}
	}
}
void DaoByteDecoder_DecodeConstants( DaoByteDecoder *self )
{
	int i, j, flag, count, numconsts = DaoByteDecoder_DecodeUInt16( self );
	int id, id2, id3;
	printf( "DaoByteDecoder_DecodeConstants: %3i\n", numconsts );
	for(i=0; i<numconsts; ++i){
		int name = DaoByteDecoder_DecodeUInt16( self );
		int perm = DaoByteDecoder_DecodeUInt8( self );
		int valueid = DaoByteDecoder_DecodeUInt32( self );
		if( name ) printf( "%3i : %s\n", i, self->identifiers->items.pString[name-1]->mbs );
	}
}
void DaoByteDecoder_DecodeVariables( DaoByteDecoder *self )
{
	int i, j, flag, count, numvars = DaoByteDecoder_DecodeUInt16( self );
	int id, id2, id3;
	printf( "DaoByteDecoder_DecodeVariables: %3i\n", numvars );
	for(i=0; i<numvars; ++i){
		int name = DaoByteDecoder_DecodeUInt16( self );
		int perm = DaoByteDecoder_DecodeUInt8( self );
		int typeid = DaoByteDecoder_DecodeUInt16( self );
		int valueid = DaoByteDecoder_DecodeUInt32( self );
		if( name ) printf( "%3i : %s\n", i, self->identifiers->items.pString[name-1]->mbs );
	}
}
void DaoByteDecoder_DecodeGlobalTypes( DaoByteDecoder *self )
{
	int i, j, flag, count, numtypes = DaoByteDecoder_DecodeUInt16( self );
	int id, id2, id3;
	printf( "DaoByteDecoder_DecodeGlobalTypes: %3i\n", numtypes );
	for(i=0; i<numtypes; ++i){
		int name = DaoByteDecoder_DecodeUInt16( self );
		int typeid = DaoByteDecoder_DecodeUInt16( self );
		if( name ) printf( "%3i : %s\n", i, self->identifiers->items.pString[name-1]->mbs );
	}
}
void DaoByteDecoder_DecodeInterfaces( DaoByteDecoder *self )
{
	int i, j, flag, count, num = DaoByteDecoder_DecodeUInt16( self );
	int id, id2, id3;
	printf( "DaoByteDecoder_DecodeInterfaces: %3i\n", num );
	for(i=0; i<num; ++i){
		int name = DaoByteDecoder_DecodeUInt16( self );
		count = DaoByteDecoder_DecodeUInt16( self );
		if( name ) printf( "%3i : %s\n", i, self->identifiers->items.pString[name-1]->mbs );
		for(j=0; j<count; ++j){
			int supid = DaoByteDecoder_DecodeUInt16( self );
		}
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int methid = DaoByteDecoder_DecodeUInt16( self );
		}
	}
}
void DaoByteDecoder_DecodeClasses( DaoByteDecoder *self )
{
	int i, j, flag, count, num = DaoByteDecoder_DecodeUInt16( self );
	int id, id2, id3;
	printf( "DaoByteDecoder_DecodeClasses: %3i\n", num );
	for(i=0; i<num; ++i){
		int name = DaoByteDecoder_DecodeUInt16( self );
		count = DaoByteDecoder_DecodeUInt16( self );
		if( name ) printf( "%3i : %s\n", i, self->identifiers->items.pString[name-1]->mbs );
		for(j=0; j<count; ++j){
			int supid = DaoByteDecoder_DecodeUInt16( self );
		}
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int name = DaoByteDecoder_DecodeUInt16( self );
			int perm = DaoByteDecoder_DecodeUInt8( self );
			int valueid = DaoByteDecoder_DecodeUInt32( self );
		}
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int name = DaoByteDecoder_DecodeUInt16( self );
			int perm = DaoByteDecoder_DecodeUInt8( self );
			int typeid = DaoByteDecoder_DecodeUInt16( self );
			int valueid = DaoByteDecoder_DecodeUInt32( self );
		}
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int name = DaoByteDecoder_DecodeUInt16( self );
			int perm = DaoByteDecoder_DecodeUInt8( self );
			int typeid = DaoByteDecoder_DecodeUInt16( self );
			int valueid = DaoByteDecoder_DecodeUInt32( self );
		}
	}
}
void DaoByteDecoder_DecodeRoutines( DaoByteDecoder *self )
{
	int i, j, flag, count, num = DaoByteDecoder_DecodeUInt16( self );
	int id, id2, id3;
	printf( "DaoByteDecoder_DecodeRoutines: %3i\n", num );
	for(i=0; i<num; ++i){
		int name = DaoByteDecoder_DecodeUInt16( self );
	}
}

int DaoByteDecoder_Decode( DaoByteDecoder *self, DString *input, DaoNamespace *nspace )
{
	if( strncmp( input->mbs, DAO_BC_SIGNATURE, 8 ) != 0 ) return 0;
	if( input->mbs[8] != 0 ) return 0; /* Not official format; */

	self->codes = (uchar_t*) input->mbs + 16;
	DaoByteDecoder_DecodeShortString( self, self->string );

	self->nspace = DaoNamespace_New( self->vmspace, self->string->mbs );

	DaoByteDecoder_DecodeModules( self );
	DaoByteDecoder_DecodeIdentifiers( self );
	DaoByteDecoder_DecodeDeclarations( self );
	DaoByteDecoder_DecodeTypes( self );
	DaoByteDecoder_DecodeValues( self );
	DaoByteDecoder_DecodeConstants( self );
	DaoByteDecoder_DecodeVariables( self );
	DaoByteDecoder_DecodeGlobalTypes( self );
	DaoByteDecoder_DecodeInterfaces( self );
	DaoByteDecoder_DecodeClasses( self );
	DaoByteDecoder_DecodeRoutines( self );

	return 1;
}
