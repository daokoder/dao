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
#include "daoBytecode.h"
#include "daoNamespace.h"
#include "daoVmspace.h"
#include "daoValue.h"


#define IntToPointer( x ) ((void*)(size_t)x)

#define SCOPING_OBJECT_CREATE  0
#define SCOPING_OBJECT_SEARCH  1


DaoByteEncoder* DaoByteEncoder_New()
{
	DaoByteEncoder *self = (DaoByteEncoder*) dao_calloc( 1, sizeof(DaoByteEncoder) );

	self->header = DString_New(1);
	self->source = DString_New(1);
	self->modules = DString_New(1);
	self->identifiers = DString_New(1);
	self->scobjects = DString_New(1);
	self->types = DString_New(1);
	self->values = DString_New(1);
	self->constants = DString_New(1);
	self->variables = DString_New(1);
	self->interfaces = DString_New(1);
	self->classes = DString_New(1);
	self->routines = DString_New(1);

	self->valueBytes = DString_New(1);
	self->lookups = DArray_New(0);
	self->names = DArray_New(0);

	self->mapIdentifiers = DHash_New(D_STRING,0);
	self->mapScobjects = DHash_New(0,0);
	self->mapTypes = DHash_New(0,0);
	self->mapValues = DHash_New(0,0);
	self->mapValueBytes = DHash_New(D_STRING,0);
	self->mapInterfaces = DHash_New(0,0);
	self->mapClasses = DHash_New(0,0);
	self->mapRoutines = DHash_New(0,0);

	DString_AppendDataMBS( self->header, "\1Dao\1\2\r\n\0", 9 );
	DString_AppendChar( self->header, sizeof(daoint) == 4 ? '\4' : '\x8' );
	DString_AppendDataMBS( self->header, "\0\0\0\0\0\0", 6 );
	return self;
}
void DaoByteEncoder_Delete( DaoByteEncoder *self )
{
	self->valueCount = 0;
	DString_Delete( self->header );
	DString_Delete( self->source );
	DString_Delete( self->modules );
	DString_Delete( self->identifiers );
	DString_Delete( self->scobjects );
	DString_Delete( self->types );
	DString_Delete( self->values );
	DString_Delete( self->constants );
	DString_Delete( self->variables );
	DString_Delete( self->interfaces );
	DString_Delete( self->classes );
	DString_Delete( self->routines );

	DString_Delete( self->valueBytes );
	DArray_Delete( self->lookups );
	DArray_Delete( self->names );

	DMap_Delete( self->mapIdentifiers );
	DMap_Delete( self->mapScobjects );
	DMap_Delete( self->mapTypes );
	DMap_Delete( self->mapValues );
	DMap_Delete( self->mapValueBytes );
	DMap_Delete( self->mapInterfaces );
	DMap_Delete( self->mapClasses );
	DMap_Delete( self->mapRoutines );
	dao_free( self );
}
void DaoByteEncoder_Reset( DaoByteEncoder *self )
{
	DString_Reset( self->source, 0 );
	DString_Reset( self->modules, 0 );
	DString_Reset( self->identifiers, 0 );
	DString_Reset( self->scobjects, 0 );
	DString_Reset( self->types, 0 );
	DString_Reset( self->values, 0 );
	DString_Reset( self->constants, 0 );
	DString_Reset( self->variables, 0 );
	DString_Reset( self->interfaces, 0 );
	DString_Reset( self->classes, 0 );
	DString_Reset( self->routines, 0 );

	DMap_Reset( self->mapIdentifiers );
	DMap_Reset( self->mapScobjects );
	DMap_Reset( self->mapTypes );
	DMap_Reset( self->mapValues );
	DMap_Reset( self->mapValueBytes );
	DMap_Reset( self->mapInterfaces );
	DMap_Reset( self->mapClasses );
	DMap_Reset( self->mapRoutines );

	self->lookups->size = 0;
	self->names->size = 0;
}

void DString_AppendUInt8( DString *bytecodes, int value )
{
	uchar_t bytes[2];
	bytes[0] = value && 0xFF;
	DString_AppendDataMBS( bytecodes, (char*) bytes, 1 );
}
void DString_AppendUInt16( DString *bytecodes, int value )
{
	uchar_t bytes[2];
	bytes[0] = (value >> 8) && 0xFF;
	bytes[1] = value && 0xFF;
	DString_AppendDataMBS( bytecodes, (char*) bytes, 2 );
}
void DString_AppendUInt32( DString *bytecodes, uint_t value )
{
	uchar_t bytes[4];
	bytes[0] = (value >> 24) && 0xFF;
	bytes[1] = (value >> 16) && 0xFF;
	bytes[2] = (value >>  8) && 0xFF;
	bytes[3] = value && 0xFF;
	DString_AppendDataMBS( bytecodes, (char*) bytes, 4 );
}
void DString_AppendDaoInt( DString *bytecodes, daoint value )
{
	uchar_t i, bytes[8];
	uchar_t m = sizeof(daoint);
	for(i=0; i<m; ++i) bytes[i] = (value >> 8*(m-1-i)) && 0xFF;
	DString_AppendDataMBS( bytecodes, (char*) bytes, m );
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
void DString_AppendNaN( DString *bytecodes )
{
	DString_AppendUInt32( bytecodes, 0x7FF << 20 );
	DString_AppendUInt32( bytecodes, 1 );
}
void DString_AppendInf( DString *bytecodes )
{
	DString_AppendUInt32( bytecodes, 0x7FF << 20 );
	DString_AppendUInt32( bytecodes, 0 );
}
void DString_AppendDouble( DString *bytecodes, double value )
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
void DString_AppendComplex( DString *bytecodes, complex16 value )
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
	DString_Append( self->identifiers, name );
	return self->mapIdentifiers->size;
}

int DaoByteEncoder_EncodeType( DaoByteEncoder *self, DaoType *type, DaoValue *host );

int DaoByteEncoder_EncodeScopingObject( DaoByteEncoder *self, DaoValue *object, DaoValue *host, int flag )
{
	DNode *node = DMap_Find( self->mapScobjects, object );
	DString *name = NULL;
	DaoType *type = NULL;
	int nameid, hostid;

	if( object == NULL ) return 0;
	if( node ) return node->value.pInt;

	switch( object->type ){
	case DAO_CLASS :
		type = object->xClass.objType;
		name = object->xClass.className;
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
		break;
	case DAO_NAMESPACE :
	default : return 0;
	}

	hostid = DaoByteEncoder_EncodeScopingObject( self, host, NULL, SCOPING_OBJECT_SEARCH );
	nameid = DaoByteEncoder_EncodeIdentifier( self, name );
	DMap_Insert( self->mapScobjects, object, IntToPointer( self->mapScobjects->size + 1 ) );
	DString_AppendUInt8( self->scobjects, object->type );
	DString_AppendUInt8( self->scobjects, flag );
	DString_AppendUInt16( self->scobjects, nameid );
	DString_AppendUInt16( self->scobjects, hostid );

	if( type ) DaoByteEncoder_EncodeType( self, type, host );
	return self->mapScobjects->size;
}
int DaoByteEncoder_EncodeSimpleType( DaoByteEncoder *self, DaoType *type, int scobj )
{
	DString_AppendUInt8( self->types, type->tid );
	DString_AppendUInt16( self->types, scobj );
	DMap_Insert( self->mapTypes, type, IntToPointer( self->mapTypes->size + 1 ) );
	return self->mapTypes->size;
}
int DaoByteEncoder_EncodeAliasType( DaoByteEncoder *self, DaoType *type, int scobj, int tid )
{
	int nameid = DaoByteEncoder_EncodeIdentifier( self, type->name );
	DString_AppendUInt8( self->types, 0xFF );
	DString_AppendUInt16( self->types, scobj );
	DString_AppendUInt16( self->types, nameid );
	DString_AppendUInt16( self->types, tid );
	DMap_Insert( self->mapTypes, type, IntToPointer( self->mapTypes->size + 1 ) );
	return self->mapTypes->size;
}

int DaoByteEncoder_EncodeValue( DaoByteEncoder *self, DaoValue *value );

int DaoByteEncoder_EncodeType( DaoByteEncoder *self, DaoType *type, DaoValue *host )
{
	DNode *node;
	int i, k, n, tpid, hostid;
	int nameid = 0;
	int typeid = 0;
	int valueid = 0;

	if( type == NULL ) return 0;
	node = DMap_Find( self->mapTypes, type );
	if( node ) return node->value.pInt;

	hostid = DaoByteEncoder_EncodeScopingObject( self, host, NULL, SCOPING_OBJECT_SEARCH );
	switch( type->tid ){
	case DAO_NONE :
		return DaoByteEncoder_EncodeSimpleType( self, dao_type_none, 0 );
	case DAO_INTEGER :
		typeid = DaoByteEncoder_EncodeSimpleType( self, dao_type_int, 0 );
		if( type == dao_type_int ) return typeid;
		return DaoByteEncoder_EncodeAliasType( self, type, hostid, typeid );
	case DAO_FLOAT :
		typeid = DaoByteEncoder_EncodeSimpleType( self, dao_type_float, 0 );
		if( type == dao_type_float ) return typeid;
		return DaoByteEncoder_EncodeAliasType( self, type, hostid, typeid );
	case DAO_DOUBLE :
		typeid = DaoByteEncoder_EncodeSimpleType( self, dao_type_double, 0 );
		if( type == dao_type_double ) return typeid;
		return DaoByteEncoder_EncodeAliasType( self, type, hostid, typeid );
	case DAO_COMPLEX :
		typeid = DaoByteEncoder_EncodeSimpleType( self, dao_type_complex, 0 );
		if( type == dao_type_complex ) return typeid;
		return DaoByteEncoder_EncodeAliasType( self, type, hostid, typeid );
	case DAO_LONG :
		typeid = DaoByteEncoder_EncodeSimpleType( self, dao_type_long, 0 );
		if( type == dao_type_long ) return typeid;
		return DaoByteEncoder_EncodeAliasType( self, type, hostid, typeid );
	case DAO_STRING :
		typeid = DaoByteEncoder_EncodeSimpleType( self, dao_type_string, 0 );
		if( type == dao_type_string ) return typeid;
		return DaoByteEncoder_EncodeAliasType( self, type, hostid, typeid );
	case DAO_ENUM :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type, 0 );
		DString_AppendUInt8( self->types, type->flagtype );
		DString_AppendUInt16( self->types, type->mapNames->size );
		for(node=DMap_First(type->mapNames); node; node=DMap_Next(type->mapNames,node)){
			int nameid = DaoByteEncoder_EncodeIdentifier( self, node->key.pString );
			DString_AppendUInt16( self->types, nameid );
			DString_AppendUInt32( self->types, node->value.pInt );
		}
		break;
	case DAO_ARRAY : case DAO_LIST : case DAO_MAP :
	case DAO_TUPLE : case DAO_VARIANT :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type, hostid );
		DString_AppendUInt16( self->types, type->nested->size );
		for(i=0,n=type->nested->size; i<n; ++i){
			int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i], 0 );
			DString_AppendUInt16( self->types, id );
		}
		break;
	case DAO_VALTYPE :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type, hostid );
		valueid = DaoByteEncoder_EncodeValue( self, type->value );
		break;
	case DAO_PAR_NAMED :
	case DAO_PAR_DEFAULT :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type, hostid );
		nameid = DaoByteEncoder_EncodeIdentifier( self, type->fname );
		tpid = DaoByteEncoder_EncodeType( self, (DaoType*) type->aux, 0 );
		DString_AppendUInt16( self->types, nameid );
		DString_AppendUInt16( self->types, tpid );
		break;
	case DAO_THT :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type, hostid );
		nameid = DaoByteEncoder_EncodeIdentifier( self, type->name );
		DString_AppendUInt16( self->types, nameid );
		break;
	case DAO_ROUTINE :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type, hostid );
		tpid = DaoByteEncoder_EncodeType( self, (DaoType*) type->aux, 0 );
		DString_AppendUInt16( self->types, tpid );
		DString_AppendUInt16( self->types, type->nested->size );
		for(i=0,n=type->nested->size; i<n; ++i){
			int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i], 0 );
			DString_AppendUInt16( self->types, id );
		}
		break;
	case DAO_OBJECT :
	case DAO_CDATA :
	case DAO_CLASS :
	case DAO_CTYPE :
	case DAO_INTERFACE :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type, hostid );
		k = DaoByteEncoder_EncodeScopingObject( self, type->aux, host, SCOPING_OBJECT_SEARCH );
		DString_AppendUInt16( self->types, k );
		break;
	case DAO_TYPE :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type, hostid );
		tpid = DaoByteEncoder_EncodeType( self, (DaoType*) type->aux, 0 );
		DString_AppendUInt16( self->types, tpid );
		break;
	case DAO_FUTURE :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type, hostid );
		tpid = DaoByteEncoder_EncodeType( self, type->nested->items.pType[0], 0 );
		DString_AppendUInt16( self->types, tpid );
		break;
	}
	return typeid;
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
			DString_Append( valueBytes, value->xString.data );
		}else{
			for(i=0; i<value->xString.data->size; ++i){
				DString_AppendUInt32( valueBytes, value->xString.data->wcs[i] );
			}
		}
		break;
	case DAO_ENUM :
		DString_AppendUInt8( valueBytes, value->type );
		typeid = DaoByteEncoder_EncodeType( self, value->xEnum.etype, NULL );
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
		typeid = DaoByteEncoder_EncodeType( self, value->xList.unitype, NULL );
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
		typeid = DaoByteEncoder_EncodeType( self, value->xMap.unitype, NULL );
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
		typeid = DaoByteEncoder_EncodeType( self, value->xTuple.unitype, NULL );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt16( valueBytes, typeid );
		DString_AppendDaoInt( valueBytes, value->xTuple.size );
		for(i=0; i<value->xTuple.size; ++i){
			valueid = DaoByteEncoder_EncodeValue( self, value->xTuple.items[i] );
			DString_AppendUInt16( valueBytes, valueid );
		}
		break;
	case DAO_OBJECT :
		object = (DaoObject*) value;
		va = (DaoValue*) object->defClass;
		DaoByteEncoder_EncodeScopingObject( self, va, NULL, SCOPING_OBJECT_SEARCH );
		if( object->rootObject == object ){ /* isRoot??? */
			for(i=0; i<object->valueCount; ++i){
				DaoByteEncoder_EncodeValue( self, object->objValues[i] );
			}
		}else{
			DaoByteEncoder_EncodeValue( self, (DaoValue*) object->rootObject );
		}
		self->valueBytes->size = 0;
		typeid = DaoByteEncoder_EncodeType( self, object->defClass->objType, NULL );
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt8( valueBytes, object->rootObject != object );
		DString_AppendUInt16( valueBytes, typeid );
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
		id = DaoByteEncoder_EncodeScopingObject( self, value, NULL, SCOPING_OBJECT_SEARCH );
		typeid = DaoByteEncoder_EncodeType( self, value->xRoutine.routType, NULL );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt16( valueBytes, id );
		DString_AppendUInt16( valueBytes, typeid );
		break;
	case DAO_CLASS :
	case DAO_CTYPE :
	case DAO_INTERFACE :
	case DAO_NAMESPACE : // XXX
		id = DaoByteEncoder_EncodeScopingObject( self, value, NULL, SCOPING_OBJECT_SEARCH );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt16( valueBytes, id );
		break;
	case DAO_TYPE :
		typeid = DaoByteEncoder_EncodeType( self, (DaoType*) value->xType.aux, NULL );
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
void DaoByteEncoder_EncodeNamespace( DaoByteEncoder *self, DaoNamespace *nspace )
{
	daoint i, n, st, pm, up, id;
	DNode *it;

	self->lookups->size = 0;
	self->names->size = 0;
	DArray_Resize( self->lookups, nspace->constants->size, IntToPointer(-1) );
	DArray_Resize( self->names, nspace->constants->size, NULL );
	for(it=DMap_First(nspace->lookupTable); it; it=DMap_Next(nspace->lookupTable,it)){
		if( LOOKUP_ST( it->value.pInt ) != DAO_GLOBAL_CONSTANT ) continue;
		self->lookups->items.pInt[id] = it->value.pInt;
		self->lookups->items.pString[id] = it->value.pString;
	}
	for(i=0, i<nspace->constants->size; i<n; ++i){
		DaoValue *value = nspace->constants->items.pConst[i]->value;
		int up = LOOKUP_UP( self->lookups->items.pInt[i] );
		int flag = up ? SCOPING_OBJECT_SEARCH : SCOPING_OBJECT_CREATE;
		DaoByteEncoder_EncodeScopingObject( self, value, NULL, flag );
	}
}
void DaoByteEncoder_Encode( DaoByteEncoder *self, DaoNamespace *nspace, DString *output )
{
	DaoByteEncoder_EncodeNamespace( self, nspace );

	/* Header: */
	DString_Append( output, self->header );

	/* Source: */
	DString_AppendUInt16( output, nspace->name->size );
	DString_Append( output, nspace->name );

	/* Modules: */
	DString_AppendUInt16( output, 0 );

	/* Identifiers: */
}





DaoByteDecoder* DaoByteDecoder_New( DaoVmSpace *vmspace )
{
	DaoByteDecoder *self = (DaoByteDecoder*) dao_calloc( 1, sizeof(DaoByteDecoder) );
	self->vmspace = vmspace;

	self->identifiers = DArray_New(D_STRING);
	self->scobjects = DArray_New(0);
	self->types = DArray_New(0);
	self->values = DArray_New(D_STRING);
	self->interfaces = DArray_New(0);
	self->classes = DArray_New(0);
	self->routines = DArray_New(0);

	return self;
}
void DaoByteDecoder_Delete( DaoByteDecoder *self )
{
	DArray_Delete( self->identifiers );
	DArray_Delete( self->scobjects );
	DArray_Delete( self->types );
	DArray_Delete( self->values );
	DArray_Delete( self->interfaces );
	DArray_Delete( self->classes );
	DArray_Delete( self->routines );
	dao_free( self );
}
void DaoByteDecoder_Reset( DaoByteDecoder *self )
{
	DArray_Clear( self->identifiers );
	self->scobjects->size = 0;
	self->types->size = 0;
	self->values->size = 0;
	self->interfaces->size = 0;
	self->classes->size = 0;
	self->routines->size = 0;
}

DaoNamespace* DaoByteDecoder_Decode( DaoByteDecoder *self, DString *input )
{
}
