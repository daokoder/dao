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



/* Mainly for reference numbers: */
#define DString_AppendUInt(a,b)       DString_AppendUInt32(a,b)
#define DaoByteDecoder_DecodeUInt(a)  DaoByteDecoder_DecodeUInt32(a)


#if 0
#define DEBUG_BC
#endif


enum DaoRoutineSubTypes
{
	DAO_ROUTINE_NORMAL ,
	DAO_ROUTINE_ABSTRACT ,
	DAO_ROUTINE_OVERLOADED
};


enum DaoDeclarationTypes
{
	DAO_DECL_CREATE ,
	DAO_DECL_SEARCH ,
	DAO_DECL_LOADAS ,
	DAO_DECL_TEMPLATE
};



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
	self->lines = DArray_New(0);
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
	DArray_Delete( self->lines );
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
void DString_AppendUInt8( DString *bytecodes, int value )
{
	uchar_t bytes[2];
	bytes[0] = value & 0xFF;
	DString_AppendBytes( bytecodes, bytes, 1 );
}
void DString_AppendUInt16( DString *bytecodes, int value )
{
	uchar_t bytes[2];
	bytes[0] = (value >> 8) & 0xFF;
	bytes[1] = value & 0xFF;
	DString_AppendBytes( bytecodes, bytes, 2 );
}
void DString_AppendUInt32( DString *bytecodes, uint_t value )
{
	uchar_t bytes[4];
	bytes[0] = (value >> 24) & 0xFF;
	bytes[1] = (value >> 16) & 0xFF;
	bytes[2] = (value >>  8) & 0xFF;
	bytes[3] = value & 0xFF;
	DString_AppendBytes( bytecodes, bytes, 4 );
}
void DString_AppendDaoInt( DString *bytecodes, daoint value )
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
//   value = (-1)^S  *  ( 1 + \sigma_0^51 (b_i * 2^{-(52-i)}) )  *  2^{E-1023}
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

	if( value == 0.0 ){
		DString_AppendUInt32( bytecodes, 0 );
		DString_AppendUInt32( bytecodes, 0 );
		return;
	}else if( isnan( value ) ){
		DString_AppendNaN( bytecodes );
		return;
	}else if( isinf( value ) ){
		DString_AppendInf( bytecodes );
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
void DaoByteEncoder_EncodeModules( DaoByteEncoder *self )
{
	int i;
	for(i=0; i<self->nspace->loadings->size; i+=2){
		DString *s1 = self->nspace->loadings->items.pString[i];
		DString *s2 = self->nspace->loadings->items.pString[i+1];
		int id1 = DaoByteEncoder_EncodeIdentifier( self, s1 );
		int id2 = s2->size ? DaoByteEncoder_EncodeIdentifier( self, s2 ) : 0;
		DString_AppendUInt( self->modules, id1 );
		DString_AppendUInt( self->modules, id2 );
		if( id2 ){
			int size = self->mapDeclarations->size;
			int lk = DaoNamespace_FindConst( self->nspace, s2 );
			DaoValue *value = DaoNamespace_GetConst( self->nspace, lk );
			DNode *node;
			if( value == NULL || value->type != DAO_NAMESPACE ) break;
			if( DMap_Find( self->mapDeclarations, value ) ) break;
			DMap_Insert( self->mapDeclarations, value, IntToPointer( size+1 ) );
			DString_AppendUInt8( self->declarations, DAO_NAMESPACE );
			DString_AppendUInt8( self->declarations, DAO_DECL_LOADAS );
			DString_AppendUInt8( self->declarations, 0 );
			DString_AppendUInt( self->declarations, 0 );
			DString_AppendUInt( self->declarations, (i/2)+1 );
			DString_AppendUInt( self->declarations, 0 );
		}
	}
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
	case DAO_CSTRUCT :
	case DAO_CDATA :
	case DAO_ROUTINE :
		DArray_Append( self->hosts, value );
		DMap_Insert( self->handled, value, NULL );
		DMap_Insert( self->mapLookupHost, value, host );
		DMap_Insert( self->mapLookupName, value, name );
		if( value->type == DAO_CTYPE ){
			/*
			// If the C type is declared as "TypeName<@TypeHolder=DefaultType,...>",
			// The C type object that can be looked up with name "TypeName" is
			// "TypeName<DefaultType,...>". To encode and decode other types
			// specialized from this C type, it is necessary to encode the template
			// type of this C type: "TypeName<@TypeHolder,...>".
			*/
			DaoValue *aux = value->xCtype.ctype->kernel->abtype->aux;
			if( aux != value ) DaoByteEncoder_AddLookupValue( self, aux, host, name );
		}
		break;
	}
}
void DaoByteEncoder_AddCfieldsLookup( DaoByteEncoder *self, DaoValue *host, DaoType *type )
{
	DString name = DString_WrapMBS( "" );
	DaoTypeKernel *kernel;
	DaoValue *value;
	DNode *it;
	DaoType_FindValue( type, & name );
	kernel = type->kernel;
	if( kernel == NULL ) return;
	for(it=DMap_First(kernel->values); it; it=DMap_Next(kernel->values,it)){
		value = it->value.pValue;
		DaoByteEncoder_AddLookupValue( self, value, host, it->key.pString );
	}
	for(it=DMap_First(kernel->methods); it; it=DMap_Next(kernel->methods,it)){
		value = it->value.pValue;
		DaoByteEncoder_AddLookupValue( self, value, host, it->key.pString );
	}
}
void DaoByteEncoder_SetupLookupData( DaoByteEncoder *self )
{
	DaoNamespace *ns;
	DaoInterface *inter;
	DaoClass *klass;
	DaoCtype *ctype;
	DaoType *type;
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
			DaoByteEncoder_AddCfieldsLookup( self, host, ctype->ctype );
			break;
		case DAO_INTERFACE :
			inter = (DaoInterface*) host;
			for(it=DMap_First(inter->methods); it; it=DMap_Next(inter->methods,it)){
				value = it->value.pValue;
				DaoByteEncoder_AddLookupValue( self, value, host, it->key.pString );
			}
			break;
		default :
			type = DaoNamespace_GetType( self->nspace, host );
			DaoByteEncoder_AddCfieldsLookup( self, host, type );
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
	DaoValue *aux;
	DaoClass *klass;
	DaoRoutine *routine;
	DaoType *type = NULL;
	DaoType *type2 = NULL;
	DString *name = NULL;
	DNode *node, *node2;
	int nameid, nameid2 = 0, hostid = 0;
	int dectype = DAO_DECL_SEARCH;
	int subtype = 0;

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
		type2 = klass->clsType;
		name = object->xClass.className;
		if( klass->classRoutine->nameSpace == self->nspace ){
			DArray_Append( self->objects, object );
			dectype = DAO_DECL_CREATE;
		}
		break;
	case DAO_CTYPE :
		type = object->xCtype.ctype;
		type = object->xCtype.cdtype;
		name = type->name;
		/* See comments in DaoByteEncoder_AddLookupValue(): */
		aux = object->xCtype.ctype->kernel->abtype->aux;
		if( aux != object ){
			DaoByteEncoder_EncodeDeclaration( self, aux );
		}else if( object->xCtype.ctype->kernel->sptree ){
			dectype = DAO_DECL_TEMPLATE;
		}
		break;
	case DAO_INTERFACE :
		type = object->xInterface.abtype;
		name = type->name;
		if( object->xInterface.nspace == self->nspace ){
			DArray_Append( self->objects, object );
			dectype = DAO_DECL_CREATE;
		}
		break;
	case DAO_ROUTINE :
		routine = (DaoRoutine*) object;
		name = routine->routName;
		subtype = DAO_ROUTINE_NORMAL;
		if( routine->nameSpace == self->nspace && routine->pFunc == NULL ){
			DArray_Append( self->objects, object );
			dectype = DAO_DECL_CREATE;
			if( routine->overloads ){
				subtype = DAO_ROUTINE_OVERLOADED;
			}else if( routine->body == NULL ){
				subtype = DAO_ROUTINE_ABSTRACT;
			}
		}
		break;
	case DAO_NAMESPACE :
		break;
	default : return 0;
	}

	DMap_Insert( self->mapDeclarations, object, IntToPointer( self->mapDeclarations->size+1 ) );
	nameid = DaoByteEncoder_EncodeIdentifier( self, name );
	DString_AppendUInt8( self->declarations, object->type );
	DString_AppendUInt8( self->declarations, subtype );
	DString_AppendUInt8( self->declarations, dectype );
	DString_AppendUInt( self->declarations, nameid );
	DString_AppendUInt( self->declarations, hostid );
	DString_AppendUInt( self->declarations, nameid2 );

	if( type ) DaoByteEncoder_EncodeType( self, type );
	if( type2 ) DaoByteEncoder_EncodeType( self, type2 );
	return self->mapDeclarations->size;
}
int DaoByteEncoder_FindType( DaoByteEncoder *self, DaoType *type )
{
	DNode *node = DMap_Find( self->mapTypes, type );
	if( node ) return node->value.pInt;
	return 0;
}
int DaoByteEncoder_EncodeSimpleType( DaoByteEncoder *self, DaoType *type )
{
	int nameid = DaoByteEncoder_EncodeIdentifier( self, type->name );
	DString_AppendUInt8( self->types, type->tid );
	DString_AppendUInt8( self->types, type->attrib );
	DString_AppendUInt( self->types, nameid );
	DMap_Insert( self->mapTypes, type, IntToPointer( self->mapTypes->size + 1 ) );
	return self->mapTypes->size;
}

int DaoByteEncoder_EncodeValue( DaoByteEncoder *self, DaoValue *value );
static void DaoByteEncoder_EncodeValue2( DaoByteEncoder *self, DaoValue *value );

int DaoByteEncoder_EncodeType( DaoByteEncoder *self, DaoType *type )
{
	DNode *node;
	int i, k, n, tpid, dec, cb, spec;
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
				DString_AppendUInt( self->types, nameid );
				DString_AppendUInt( self->types, node->value.pInt );
			}
		}else{
			DString_AppendUInt16( self->types, 0 );
		}
		break;
	case DAO_ARRAY : case DAO_LIST : case DAO_MAP :
	case DAO_TUPLE : case DAO_TYPE : case DAO_VARIANT :
		if( type->tid == DAO_VARIANT && type->aux ){
			DaoByteEncoder_EncodeType( self, (DaoType*) type->aux );
		}
		for(i=0,n=type->nested->size; i<n; ++i){
			int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i] );
		}
		if( (typeid = DaoByteEncoder_FindType( self, type )) ) break;
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		if( type->tid == DAO_VARIANT ){
			int id = DaoByteEncoder_EncodeType( self, (DaoType*) type->aux );
			DString_AppendUInt( self->types, id );
		}
		DString_AppendUInt16( self->types, type->nested->size );
		for(i=0,n=type->nested->size; i<n; ++i){
			int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i] );
			DString_AppendUInt( self->types, id );
		}
		break;
	case DAO_VALTYPE :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		DaoByteEncoder_EncodeValue2( self, type->aux );
		DString_Append( self->types, self->valueBytes );
		break;
	case DAO_PAR_NAMED :
	case DAO_PAR_DEFAULT :
		tpid = DaoByteEncoder_EncodeType( self, (DaoType*) type->aux );
		if( (typeid = DaoByteEncoder_FindType( self, type )) ) break;
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		nameid = DaoByteEncoder_EncodeIdentifier( self, type->fname );
		DString_AppendUInt( self->types, nameid );
		DString_AppendUInt( self->types, tpid );
		break;
	case DAO_PAR_VALIST :
		tpid = type->aux ? DaoByteEncoder_EncodeType( self, (DaoType*) type->aux ) : 0;
		if( (typeid = DaoByteEncoder_FindType( self, type )) ) break;
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		DString_AppendUInt( self->types, 0 );
		DString_AppendUInt( self->types, tpid );
		break;
	case DAO_CODEBLOCK :
		tpid = DaoByteEncoder_EncodeType( self, (DaoType*) type->aux );
		for(i=0,n=type->nested->size; i<n; ++i){
			int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i] );
		}
		if( (typeid = DaoByteEncoder_FindType( self, type )) ) break;
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		DString_AppendUInt( self->types, tpid );
		DString_AppendUInt16( self->types, type->nested->size );
		for(i=0,n=type->nested->size; i<n; ++i){
			int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i] );
			DString_AppendUInt( self->types, id );
		}
		break;
	case DAO_ROUTINE :
		dec = tpid = 0;
		if( type->aux && type->aux->type == DAO_ROUTINE ){
			dec = DaoByteEncoder_EncodeDeclaration( self, type->aux );
		}else{
			tpid = DaoByteEncoder_EncodeType( self, (DaoType*) type->aux );
		}
		cb = DaoByteEncoder_EncodeType( self, type->cbtype );
		for(i=0,n=type->nested->size; i<n; ++i){
			int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i] );
		}
		if( (typeid = DaoByteEncoder_FindType( self, type )) ) break;
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		DString_AppendUInt( self->types, dec );
		DString_AppendUInt( self->types, tpid );
		DString_AppendUInt( self->types, cb );
		DString_AppendUInt16( self->types, type->nested->size );
		for(i=0,n=type->nested->size; i<n; ++i){
			int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i] );
			DString_AppendUInt( self->types, id );
		}
		break;
	case DAO_OBJECT :
	case DAO_CLASS :
	case DAO_INTERFACE :
		k = DaoByteEncoder_EncodeDeclaration( self, type->aux );
		if( (typeid = DaoByteEncoder_FindType( self, type )) ) break;
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		DString_AppendUInt( self->types, k );
		break;
	case DAO_CDATA :
	case DAO_CSTRUCT :
	case DAO_CTYPE :
		spec = type != type->kernel->abtype;
		spec &= type != type->kernel->abtype->aux->xCtype.ctype;
		if( spec && type->nested && type->nested->size ){
			k = DaoByteEncoder_EncodeDeclaration( self, type->kernel->abtype->aux );
			for(i=0,n=type->nested->size; i<n; ++i){
				int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i] );
			}
		}else{
			k = DaoByteEncoder_EncodeDeclaration( self, type->aux );
		}
		if( (typeid = DaoByteEncoder_FindType( self, type )) ) break;
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		DString_AppendUInt( self->types, k );
		if( spec && type->nested && type->nested->size ){
			DString_AppendUInt16( self->types, type->nested->size );
			for(i=0,n=type->nested->size; i<n; ++i){
				int id = DaoByteEncoder_EncodeType( self, type->nested->items.pType[i] );
				DString_AppendUInt( self->types, id );
			}
		}else{
			DString_AppendUInt16( self->types, 0 );
		}
		break;
	case DAO_ANY :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		break;
	case DAO_THT :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
		break;
	default :
		typeid = DaoByteEncoder_EncodeSimpleType( self, type );
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
	DString *string = DString_New(1);
	DString *valueBytes = self->valueBytes;
	daoint i, n, id, id2, typeid, valueid;

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
		DString_AppendDaoInt( valueBytes, value->xLong.value->size );
		for(i=0; i<value->xLong.value->size; ++i){
			DString_AppendUInt8( valueBytes, value->xLong.value->data[i] );
		}
		break;
	case DAO_STRING :
		DString_AppendUInt8( valueBytes, value->type );
		if( value->xString.data->mbs ){
			DString_AppendUInt8( valueBytes, (1<<4)|1 );
			DString_AppendDaoInt( valueBytes, value->xString.data->size );
			DString_AppendString( valueBytes, value->xString.data );
		}else{
			uint_t max = 0;
			for(i=0; i<value->xString.data->size; ++i){
				wchar_t ch = value->xString.data->wcs[i];
				if( ch > max ) max = ch;
			}
			for(i=1; i<=4; ++i){
				max = max >> 8;
				if( max == 0 ) break;
			}
			if( i == 3 ) i = 4;
			max = i;
			DString_AppendUInt8( valueBytes, (1<<4)|max );
			DString_AppendDaoInt( valueBytes, value->xString.data->size );
			for(i=0; i<value->xString.data->size; ++i){
				wchar_t ch = value->xString.data->wcs[i];
				switch( max ){
				case 1 : DString_AppendUInt8( valueBytes, ch );  break;
				case 2 : DString_AppendUInt16( valueBytes, ch ); break;
				case 4 : DString_AppendUInt32( valueBytes, ch ); break;
				}
			}
		}
		break;
	case DAO_ENUM :
		typeid = DaoByteEncoder_EncodeType( self, value->xEnum.etype );
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt( valueBytes, typeid );
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
			valueid = DaoByteEncoder_EncodeValue( self, value->xList.items.items.pValue[i] );
			DString_AppendUInt( string, valueid );
		}
		typeid = DaoByteEncoder_EncodeType( self, value->xList.unitype );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt( valueBytes, typeid );
		DString_AppendDaoInt( valueBytes, value->xList.items.size );
		DString_Append( valueBytes, string );
		break;
	case DAO_MAP :
		for(it=DMap_First(value->xMap.items); it; it=DMap_Next(value->xMap.items,it) ){
			int ik = DaoByteEncoder_EncodeValue( self, it->key.pValue );
			int iv = DaoByteEncoder_EncodeValue( self, it->value.pValue );
			DString_AppendUInt( string, ik );
			DString_AppendUInt( string, iv );
		}
		typeid = DaoByteEncoder_EncodeType( self, value->xMap.unitype );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt( valueBytes, typeid );
		DString_AppendUInt32( valueBytes, value->xMap.items->hashing );
		DString_AppendDaoInt( valueBytes, value->xMap.items->size );
		DString_Append( valueBytes, string );
		break;
	case DAO_TUPLE :
		for(i=0; i<value->xTuple.size; ++i){
			valueid = DaoByteEncoder_EncodeValue( self, value->xTuple.items[i] );
			DString_AppendUInt( string, valueid );
		}
		typeid = DaoByteEncoder_EncodeType( self, value->xTuple.unitype );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt( valueBytes, typeid );
		DString_AppendUInt32( valueBytes, value->xTuple.size );
		DString_Append( valueBytes, string );
		break;
	case DAO_OBJECT :
		object = (DaoObject*) value;
		va = (DaoValue*) object->defClass;
		id = DaoByteEncoder_EncodeDeclaration( self, va );
		if( value == object->defClass->objType->value ){
			DString_AppendUInt8( valueBytes, value->type );
			DString_AppendUInt8( valueBytes, 0x2 );
			DString_AppendUInt( valueBytes, id );
			break;
		}
		if( object->rootObject == object ){ /* isRoot??? */
			for(i=0; i<object->valueCount; ++i){
				valueid = DaoByteEncoder_EncodeValue( self, object->objValues[i] );
				DString_AppendUInt( string, valueid );
			}
		}else{
			valueid = DaoByteEncoder_EncodeValue( self, (DaoValue*) object->rootObject );
			DString_AppendUInt( string, valueid );
		}
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt8( valueBytes, object->rootObject != object );
		DString_AppendUInt( valueBytes, id );
		DString_AppendDaoInt( valueBytes, value->xTuple.size );
		DString_Append( valueBytes, string );
		break;
	case DAO_ROUTINE :
		if( value->xRoutine.overloads ){
			DArray *routines = value->xRoutine.overloads->routines;
			for(i=0; i<routines->size; ++i){
				DaoByteEncoder_EncodeValue( self, routines->items.pValue[i] );
			}
		}
		id = DaoByteEncoder_EncodeDeclaration( self, value );
		typeid = DaoByteEncoder_EncodeType( self, value->xRoutine.routType );
		id2 = DaoByteEncoder_EncodeType( self, value->xRoutine.routHost );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt( valueBytes, id );
		DString_AppendUInt( valueBytes, typeid );
		DString_AppendUInt( valueBytes, id2 );
		DString_AppendUInt16( valueBytes, value->xRoutine.attribs );
		if( value->xRoutine.overloads ){
			DArray *routines = value->xRoutine.overloads->routines;
			DString_AppendUInt16( valueBytes, routines->size );
			for(i=0; i<routines->size; ++i){
				valueid = DaoByteEncoder_EncodeValue( self, routines->items.pValue[i] );
				DString_AppendUInt( valueBytes, valueid );
			}
		}else{
			DString_AppendUInt16( valueBytes, 0 );
		}
		break;
	case DAO_CLASS :
	case DAO_CTYPE :
	case DAO_INTERFACE :
	case DAO_NAMESPACE : // XXX
		id = DaoByteEncoder_EncodeDeclaration( self, value );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt( valueBytes, id );
		break;
	case DAO_TYPE :
		typeid = DaoByteEncoder_EncodeType( self, (DaoType*) value );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt( valueBytes, typeid );
		break;
	case DAO_PAR_NAMED :
		id = DaoByteEncoder_EncodeIdentifier( self, value->xNameValue.name );
		typeid = DaoByteEncoder_EncodeType( self, (DaoType*) value->xNameValue.unitype );
		valueid = DaoByteEncoder_EncodeValue( self, value->xNameValue.value );
		self->valueBytes->size = 0;
		DString_AppendUInt8( valueBytes, value->type );
		DString_AppendUInt( valueBytes, id );
		DString_AppendUInt( valueBytes, typeid );
		DString_AppendUInt( valueBytes, valueid );
		break;
	}
	DString_Delete( string );
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
	if( self->valueBytes->size == 0 ) return 0;

	self->valueCount += 1;
	DString_Append( self->values, self->valueBytes );
	DMap_Insert( self->mapValues, value, IntToPointer( self->valueCount ) );
	DMap_Insert( self->mapValueBytes, self->valueBytes, IntToPointer( self->valueCount ) );
	self->valueBytes->size = 0;
	return self->valueCount;
}

void DaoByteEncoder_EncodeConstant( DaoByteEncoder *self, DString *name, DaoValue *value, int id, int pm )
{
	int nameid = DaoByteEncoder_EncodeIdentifier( self, name );
	int valueid = DaoByteEncoder_EncodeValue( self, value );
	self->valueBytes->size = 0;
	DString_AppendUInt( self->valueBytes, nameid );
	DString_AppendUInt16( self->valueBytes, id );
	DString_AppendUInt8( self->valueBytes, pm );
	DString_AppendUInt( self->valueBytes, valueid );
}

void DaoByteEncoder_EncodeVariable( DaoByteEncoder *self, DString *name, DaoVariable *var, int id, int pm )
{
	/* var->value maybe NULL or none value for uninitialized variable: */
	DaoValue *value = DaoType_MatchValue( var->dtype, var->value, NULL ) ? var->value : NULL;
	int nameid = DaoByteEncoder_EncodeIdentifier( self, name );
	int typeid = DaoByteEncoder_EncodeType( self, var->dtype );
	int valueid = DaoByteEncoder_EncodeValue( self, value );
	self->valueBytes->size = 0;
	DString_AppendUInt( self->valueBytes, nameid );
	DString_AppendUInt16( self->valueBytes, id );
	DString_AppendUInt8( self->valueBytes, pm );
	DString_AppendUInt( self->valueBytes, typeid );
	DString_AppendUInt( self->valueBytes, valueid );
}

void DaoByteEncoder_GetLookupName( int size, DMap *lookupTable, DArray *lookups, DArray *names, int st )
{
	DNode *it;

	lookups->size = 0;
	names->size = 0;
	DArray_Resize( lookups, size, IntToPointer(LOOKUP_BIND(st,0,0,0)) );
	DArray_Resize( names, size, NULL );
	for(it=DMap_First(lookupTable); it; it=DMap_Next(lookupTable,it)){
		int id = LOOKUP_ID( it->value.pInt );
		if( LOOKUP_ST( it->value.pInt ) != st ) continue;
		lookups->items.pInt[id] = it->value.pInt;
		names->items.pString[id] = it->key.pString;
	}
}

void DaoByteEncoder_EncodeInterface( DaoByteEncoder *self, DaoInterface *interfase )
{
	DNode *it;
	int i, id, count = 0;;

	if( DMap_Find( self->mapInterfaces, interfase ) ) return;

	id = DaoByteEncoder_FindDeclaration( self, (DaoValue*)interfase );
	if( id == 0 ) return;

	DMap_Insert( self->mapInterfaces, interfase, IntToPointer( self->mapInterfaces->size+1 ) );
	DString_AppendUInt( self->interfaces, id );
	DString_AppendUInt16( self->interfaces, interfase->supers->size );
	for(i=0; i<interfase->supers->size; ++i){
		DaoValue *value = interfase->supers->items.pValue[i];
		id = DaoByteEncoder_EncodeDeclaration( self, value );
		DString_AppendUInt( self->interfaces, id );
	}
	for(it=DMap_First(interfase->methods); it; it=DMap_Next(interfase->methods,it)){
		DaoRoutine *routine = it->value.pRoutine;
		count += routine->overloads ? routine->overloads->routines->size : 1;
	}
	DString_AppendUInt16( self->interfaces, count );
	for(it=DMap_First(interfase->methods); it; it=DMap_Next(interfase->methods,it)){
		DaoRoutine *routine = it->value.pRoutine;
		if( routine->overloads ){
			for(i=0, count=routine->overloads->routines->size; i<count; ++i){
				DaoValue *rout = routine->overloads->routines->items.pValue[i];
				int routid = DaoByteEncoder_EncodeValue( self, rout );
				DString_AppendUInt( self->interfaces, routid );
			}
		}else{
			int routid = DaoByteEncoder_EncodeValue( self, it->value.pValue );
			DString_AppendUInt( self->interfaces, routid );
		}
	}
}

void DaoByteEncoder_EncodeClass( DaoByteEncoder *self, DaoClass *klass )
{
	DNode *it;
	DMap *abstypes;
	int i, id, id2, count;

	if( DMap_Find( self->mapClasses, klass ) ) return;

	id = DaoByteEncoder_FindDeclaration( self, (DaoValue*)klass );
	if( id == 0 ) return;

	DMap_Insert( self->mapClasses, klass, IntToPointer( self->mapClasses->size + 1 ) );
	id2 = DaoByteEncoder_EncodeDeclaration( self, (DaoValue*)klass->classRoutine );

	DaoByteEncoder_EncodeValue( self, (DaoValue*)klass->classRoutine );
	DString_AppendUInt( self->classes, id );
	DString_AppendUInt( self->classes, id2 );
	DString_AppendUInt16( self->classes, klass->attribs );
	DString_AppendUInt16( self->classes, klass->superClass->size );
	for(i=0; i<klass->superClass->size; ++i){
		DaoValue *value = klass->superClass->items.pValue[i];
		id = DaoByteEncoder_EncodeDeclaration( self, value );
		DString_AppendUInt( self->classes, id );
	}
	DString_AppendUInt16( self->classes, klass->mixinClass->size );
	for(i=0; i<klass->mixinClass->size; ++i){
		DaoValue *value = klass->mixinClass->items.pValue[i];
		id = DaoByteEncoder_EncodeDeclaration( self, value );
		DString_AppendUInt( self->classes, id );
	}

	count = 0;
	self->tmpBytes->size = 0;
	for(i=0; i<klass->cstDataName->size; ++i){
		DaoValue *value = klass->constants->items.pConst[i]->value;
		DString *name = klass->cstDataName->items.pString[i];
		DNode *node = MAP_Find( klass->lookupTable, name );
		if( i >= klass->cstMixinStart && i < klass->cstMixinEnd2 ) continue;
		if( i >= klass->cstParentStart && i < klass->cstParentEnd ) continue;
		id = LOOKUP_ID( node->value.pInt );
		DaoByteEncoder_EncodeDeclaration( self, value );
		DaoByteEncoder_EncodeConstant( self, name, value, id, LOOKUP_PM( node->value.pInt ) );
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
		if( i >= klass->glbMixinStart && i < klass->glbMixinEnd2 ) continue;
		if( i >= klass->glbParentStart && i < klass->glbParentEnd ) continue;
		id = LOOKUP_ID( node->value.pInt );
		DaoByteEncoder_EncodeDeclaration( self, var->value );
		DaoByteEncoder_EncodeVariable( self, name, var, id, pm );
		DString_Append( self->tmpBytes, self->valueBytes );
		count += 1;
	}
	DString_AppendUInt16( self->classes, count );
	DString_Append( self->classes, self->tmpBytes );

	count = 0;
	self->tmpBytes->size = 0;
	for(i=0; i<klass->objDataName->size; ++i){
		DaoVariable *var = klass->instvars->items.pVar[i];
		DString *name = klass->objDataName->items.pString[i];
		DNode *node = MAP_Find( klass->lookupTable, name );
		int pm = LOOKUP_PM( node->value.pInt );
		if( LOOKUP_UP( node->value.pInt ) ) continue;
		if( i >= klass->objMixinStart && i < klass->objMixinEnd2 ) continue;
		id = LOOKUP_ID( node->value.pInt );
		if( id < i ) break; /* self from parent class; */
		DaoByteEncoder_EncodeDeclaration( self, var->value );
		DaoByteEncoder_EncodeVariable( self, name, var, id, pm );
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
		DString_AppendUInt( self->classes, nameid );
		DString_AppendUInt( self->classes, typeid );
	}
}

void DaoByteEncoder_EncodeRoutine( DaoByteEncoder *self, DaoRoutine *routine )
{
	DNode *it;
	DMap *abstypes;
	DaoType *type;
	int i, id, id2, id3;

	if( routine->body == NULL ) return;
	if( DMap_Find( self->mapRoutines, routine ) ) return;

	id = DaoByteEncoder_FindDeclaration( self, (DaoValue*)routine );
	if( id == 0 ) return;

	DMap_Insert( self->mapRoutines, routine, IntToPointer( self->mapRoutines->size + 1 ) );

#ifdef DEBUG_BC
	DString_Append( self->routines, routine->routName );
	DString_AppendMBS( self->routines, "()====\n" );
#endif

	DString_AppendUInt( self->routines, id );
	id = DaoByteEncoder_EncodeIdentifier( self, routine->routName );
	id2 = DaoByteEncoder_EncodeType( self, routine->routType );
	id3 = DaoByteEncoder_EncodeType( self, routine->routHost );
	DString_AppendUInt( self->routines, id );
	DString_AppendUInt( self->routines, id2 );
	DString_AppendUInt( self->routines, id3 );
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
		DString_AppendUInt( self->routines, id );
	}
	DString_AppendUInt16( self->routines, routine->body->svariables->size );
	for(i=0; i<routine->body->svariables->size; ++i){
		DaoVariable *var = routine->body->svariables->items.pVar[i];
		int tid = DaoByteEncoder_EncodeType( self, var->dtype );
		int vid = DaoByteEncoder_EncodeValue( self, var->value );
		DString_AppendUInt( self->routines, tid );
		DString_AppendUInt( self->routines, vid );
	}
	abstypes = routine->body->localVarType;
	DString_AppendUInt16( self->routines, routine->body->regCount );
	DString_AppendUInt16( self->routines, abstypes->size );
	for(it=DMap_First(abstypes); it; it=DMap_Next(abstypes,it)){
		id = DaoByteEncoder_EncodeType( self, it->value.pType );
		DString_AppendUInt16( self->routines, it->key.pInt );
		DString_AppendUInt( self->routines, id );
	}

	self->lines->size = 0;
	for(i=0; i<routine->body->annotCodes->size; ++i){
		DaoVmCodeX *vmc = routine->body->annotCodes->items.pVmc[i];
		int count = self->lines->size;
		int lastline = count ? self->lines->items.pInt[count-2] : -1;
		int lastcount = count ? self->lines->items.pInt[count-1] : -1;
		if( vmc->line != lastline || lastcount >= 255 ){
			DArray_PushBack( self->lines, IntToPointer( vmc->line ) );
			DArray_PushBack( self->lines, IntToPointer( 1 ) );
		}else{
			self->lines->items.pInt[count-1] += 1;
		}
	}
	DString_AppendUInt16( self->routines, self->lines->size/2 );
	for(i=0; i<self->lines->size; i+=2){
		int line = self->lines->items.pInt[i];
		int count = self->lines->items.pInt[i+1];
		DString_AppendUInt16( self->routines, line );
		DString_AppendUInt8( self->routines, count );
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

	DaoByteEncoder_SetupLookupData( self );
	DaoByteEncoder_EncodeModules( self );

	DaoByteEncoder_GetLookupName( constants->size, lookupTable, self->lookups, self->names, DAO_GLOBAL_CONSTANT );
	for(i=0, n=constants->size; i<n; ++i){
		DaoValue *value = constants->items.pConst[i]->value;
		DString *name = self->names->items.pString[i];
		int pm = LOOKUP_PM( self->lookups->items.pInt[i] );
		int up = LOOKUP_UP( self->lookups->items.pInt[i] );
		DaoByteEncoder_EncodeDeclaration( self, value );
		if( up ) continue;
		if( name == NULL && value->type == DAO_ROUTINE && value->xRoutine.nameSpace == self->nspace ){
			it = DMap_Find( lookupTable, value->xRoutine.routName );
			id = it ? LOOKUP_ID( it->value.pInt ) : i;
			/* name may be NULL for overloaded functions: */
			DaoByteEncoder_EncodeConstant( self, value->xRoutine.routName, value, id, pm );
		}else{
			DaoByteEncoder_EncodeConstant( self, name, value, i, pm );
		}
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
		DaoByteEncoder_EncodeVariable( self, name, var, i, pm );
		DString_Append( self->variables, self->valueBytes );
		self->varCount += 1;
	}

	abstypes = nspace->abstypes;
	for(it=DMap_First(abstypes); it; it=DMap_Next(abstypes,it)){
		int nameid = DaoByteEncoder_EncodeIdentifier( self, it->key.pString );
		int typeid = DaoByteEncoder_EncodeType( self, it->value.pType );
		DString_AppendUInt( self->glbtypes, nameid );
		DString_AppendUInt( self->glbtypes, typeid );
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
	DString_AppendMBS( output, "Identifier:\n" );
	DString_AppendUInt( output, self->mapIdentifiers->size );
	DString_Append( output, self->identifiers );
	DString_AppendMBS( output, "Modules:\n" );
	DString_AppendUInt( output, nspace->loadings->size/2 );
	DString_Append( output, self->modules );
	DString_AppendMBS( output, "Declarations:\n" );
	DString_AppendUInt( output, self->mapDeclarations->size );
	DString_Append( output, self->declarations );
	DString_AppendMBS( output, "Types:\n" );
	DString_AppendUInt( output, self->mapTypes->size );
	DString_Append( output, self->types );
	DString_AppendMBS( output, "Values:\n" );
	DString_AppendUInt( output, self->valueCount );
	DString_Append( output, self->values );
	DString_AppendMBS( output, "Global Constants:\n" );
	DString_AppendUInt( output, self->constCount );
	DString_Append( output, self->constants );
	DString_AppendMBS( output, "Global Variables:\n" );
	DString_AppendUInt( output, self->varCount );
	DString_Append( output, self->variables );
	DString_AppendMBS( output, "Global Types:\n" );
	DString_AppendUInt( output, nspace->abstypes->size );
	DString_Append( output, self->glbtypes );
	DString_AppendMBS( output, "Routines:\n" );
	DString_AppendUInt( output, self->mapRoutines->size );
	DString_Append( output, self->routines );
	DString_AppendMBS( output, "Interfaces:\n" );
	DString_AppendUInt( output, self->mapInterfaces->size );
	DString_Append( output, self->interfaces );
	DString_AppendMBS( output, "Classes:\n" );
	DString_AppendUInt( output, self->mapClasses->size );
	DString_Append( output, self->classes );
	return;
#endif

	/* Identifiers: */
	DString_AppendUInt( output, self->mapIdentifiers->size );
	DString_Append( output, self->identifiers );

	/* Modules: */
	DString_AppendUInt( output, nspace->loadings->size/2 );
	DString_Append( output, self->modules );

	/* Declarations: */
	DString_AppendUInt( output, self->mapDeclarations->size );
	DString_Append( output, self->declarations );

	/* Types: */
	DString_AppendUInt( output, self->mapTypes->size );
	DString_Append( output, self->types );

	/* Values: */
	DString_AppendUInt( output, self->valueCount );
	DString_Append( output, self->values );

	/* Global Constants: */
	DString_AppendUInt( output, self->constCount );
	DString_Append( output, self->constants );

	/* Global Variables: */
	DString_AppendUInt( output, self->varCount );
	DString_Append( output, self->variables );

	/* Global Types: */
	DString_AppendUInt( output, nspace->abstypes->size );
	DString_Append( output, self->glbtypes );

	/*
	// Routines:
	// Routines need to be encoded and decoded before classes,
	// in order to handle mixin classes properly.
	*/
	DString_AppendUInt( output, self->mapRoutines->size );
	DString_Append( output, self->routines );

	/* Interfaces: */
	DString_AppendUInt( output, self->mapInterfaces->size );
	DString_Append( output, self->interfaces );

	/* Classes: */
	DString_AppendUInt( output, self->mapClasses->size );
	DString_Append( output, self->classes );
}





DaoByteDecoder* DaoByteDecoder_New( DaoVmSpace *vmspace )
{
	DaoByteDecoder *self = (DaoByteDecoder*) dao_calloc( 1, sizeof(DaoByteDecoder) );
	self->vmspace = vmspace;

	self->identifiers = DArray_New(D_STRING);
	self->namespaces = DArray_New(D_VALUE);
	self->declarations = DArray_New(D_VALUE);
	self->types = DArray_New(D_VALUE);
	self->values = DArray_New(D_VALUE);
	self->interfaces = DArray_New(D_VALUE);
	self->classes = DArray_New(D_VALUE);
	self->routines = DArray_New(D_VALUE);
	self->array = DArray_New(0);
	self->string = DString_New(1);
	self->map = DMap_New(D_STRING,0);
	self->intSize = 4;
	return self;
}
void DaoByteDecoder_Delete( DaoByteDecoder *self )
{
	DArray_Delete( self->identifiers );
	DArray_Delete( self->namespaces );
	DArray_Delete( self->declarations );
	DArray_Delete( self->types );
	DArray_Delete( self->values );
	DArray_Delete( self->interfaces );
	DArray_Delete( self->classes );
	DArray_Delete( self->routines );
	DArray_Delete( self->array );
	DString_Delete( self->string );
	DMap_Delete( self->map );
	dao_free( self );
}

int DaoByteDecoder_DecodeUInt8( DaoByteDecoder *self )
{
	int value;
	if( self->codes >= self->end ) return 0;
	value = self->codes[0];
	self->codes += 1;
	return value;
}
int DaoByteDecoder_DecodeUInt16( DaoByteDecoder *self )
{
	int value;
	if( (self->codes + 2) > self->end ){
		self->codes = self->error;
		return 0;
	}
	value = (self->codes[0]<<8) + self->codes[1];
	self->codes += 2;
	return value;
}
uint_t DaoByteDecoder_DecodeUInt32( DaoByteDecoder *self )
{
	uint_t value;
	if( (self->codes + 4) > self->end ){
		self->codes = self->error;
		return 0;
	}
	value = self->codes[0] << 24;
	value += self->codes[1] << 16;
	value += self->codes[2] << 8;
	value += self->codes[3];
	self->codes += 4;
	return value;
}
daoint DaoByteDecoder_DecodeDaoInt( DaoByteDecoder *self )
{
	DaoStream *stream = self->vmspace->errorStream;
	uchar_t i, m = self->intSize;
	daoint value = 0;

	if( (self->codes + m) > self->end ) goto Error;
	if( self->intSize > sizeof(daoint) ){ /* self->intSize=8, sizeof(daoint)=4 */
		uchar_t *s = self->codes;
		daoint B1 = s[0], B2 = s[1], B3 = s[2], B4 = s[3];
		daoint B5 = s[4], B6 = s[5], B7 = s[6], B8 = s[7];

		self->codes += self->intSize;
		if( (B1 == 0x7F || B1 == 0xFF) && B2 == 0xFF && B3 == 0xFF && B4 == 0xFF ){
			if( B5 & 0x80 ) goto TooBigInteger;
			if( B1 == 0xFF ) B5 |= 0x80;
		}else if( B1 || B2 || B3 || B4 ){
			goto TooBigInteger;
		}
		return (B5<<24)|(B6<<16)|(B7<<8)|B8;
	}else if( self->intSize < sizeof(daoint) ){ /* self->intSize=4, sizeof(daoint)=8 */
		uchar_t *s = self->codes;
		daoint B1 = s[0], B2 = s[1], B3 = s[2], B4 = s[3];

		self->codes += self->intSize;
		if( B1 & 0x80 ){
			daoint leading = (0xFF<<24)|(0xFF<<16)|(0xFF<<8)|0xFF;
			daoint shift = 32; /* just to avoid a warning on 32 bit systems; */
			return (leading<<shift)|(0xFF<<24)|((B1&0x7F)<<24)|(B2<<16)|(B3<<8)|B4;
		}
		return (B1<<24)|(B2<<16)|(B3<<8)|B4;
	}

	for(i=0; i<m; ++i) value |= ((daoint)self->codes[i]) << 8*(m-1-i);
	self->codes += m;
	return value;
TooBigInteger:
	DaoStream_WriteMBS( stream, "Error: too big integer value for the platform!" );
Error:
	self->codes = self->error;
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
double DaoByteDecoder_DecodeDouble( DaoByteDecoder *self )
{
	double value = 1.0;
	uint_t first = DaoByteDecoder_DecodeUInt32( self );
	uint_t second = DaoByteDecoder_DecodeUInt32( self );
	uint_t negative = first & (1<<31);
	int i, expon;

	if( (self->codes + 8) > self->end ){
		self->codes = self->error;
		return 0;
	}
	if( first == 0 && second == 0 ) return 0;
	if( first == (0x7FF<<20) && second == 0 ) return INFINITY;
	if( first == (0x7FF<<20) && second == 1 ) return NAN;

	first = (first<<1)>>1;
	expon = (first>>20) - 1023;
	for(i=0; i<32; ++i){
		if( (second>>i)&0x1 ){
			int e = -(52-i);
			if( e >= 0 )
				value += pow( 2, e );
			else
				value += 1.0 / pow( 2, -e );
		}
	}
	for(i=0; i<20; ++i){
		if( (first>>i)&0x1 ){
			int e = -(20-i);
			if( e >= 0 )
				value += pow( 2, e );
			else
				value += 1.0 / pow( 2, -e );
		}
	}
	if( expon >= 0 )
		value *= pow( 2, expon );
	else
		value /= pow( 2, -expon );
	if( negative ) value = -value;
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
	if( self->codes >= self->end ) return;
	DString_Reset( string, 0 );
	DString_AppendDataMBS( string, (char*) self->codes, len );
	self->codes += len;
}

DaoNamespace* DaoNamespace_LoadModule( DaoNamespace *self, DString *name );

DString* DaoByteDecoder_GetIdentifier( DaoByteDecoder *self, int id )
{
	if( id == 0 ) return NULL;
	if( id < 0 || id > self->identifiers->size ){
		self->codes = self->error;
		return NULL;
	}
	return self->identifiers->items.pString[id-1];
}
DaoValue* DaoByteDecoder_GetDeclaration( DaoByteDecoder *self, int id )
{
	if( id == 0 ) return NULL;
	if( id < 0 || id > self->declarations->size ){
		self->codes = self->error;
		return NULL;
	}
	return self->declarations->items.pValue[id-1];
}
DaoType* DaoByteDecoder_GetType( DaoByteDecoder *self, int id )
{
	if( id == 0 ) return NULL;
	if( id < 0 || id > self->types->size ){
		self->codes = self->error;
		return NULL;
	}
	return self->types->items.pType[id-1];
}
DaoValue* DaoByteDecoder_GetValue( DaoByteDecoder *self, int id )
{
	if( id == 0 ) return NULL;
	if( id < 0 || id > self->values->size ){
		self->codes = self->error;
		return NULL;
	}
	return self->values->items.pValue[id-1];
}

void DaoByteDecoder_DecodeIdentifiers( DaoByteDecoder *self )
{
	int i, count = DaoByteDecoder_DecodeUInt( self );
	for(i=0; i<count; ++i){
		DaoByteDecoder_DecodeShortString( self, self->string );
		DArray_Append( self->identifiers, self->string );
	}
}
void DaoByteDecoder_DecodeModules( DaoByteDecoder *self )
{
	DaoNamespace *ns;
	int i, count = DaoByteDecoder_DecodeUInt( self );
	for(i=0; i<count; ++i){
		int id1 = DaoByteDecoder_DecodeUInt( self );
		int id2 = DaoByteDecoder_DecodeUInt( self );
		DString *s1 = DaoByteDecoder_GetIdentifier( self, id1 );
		DString *s2 = DaoByteDecoder_GetIdentifier( self, id2 );
		if( self->codes >= self->error ) break;
		ns = DaoNamespace_LoadModule( self->nspace, s1 );
		DArray_Append( self->namespaces, ns );
		if( s2 == NULL ) DaoNamespace_AddParent( self->nspace, ns );
	}
}
void DaoByteDecoder_DecodeDeclarations( DaoByteDecoder *self )
{
	int i, id, count = DaoByteDecoder_DecodeUInt( self );
	for(i=0; i<count; ++i){
		int type = DaoByteDecoder_DecodeUInt8( self );
		int subtype = DaoByteDecoder_DecodeUInt8( self );
		int dectype = DaoByteDecoder_DecodeUInt8( self );
		int nameid  = DaoByteDecoder_DecodeUInt( self );
		int hostid  = DaoByteDecoder_DecodeUInt( self );
		int fieldid = DaoByteDecoder_DecodeUInt( self );
		DString *field = DaoByteDecoder_GetIdentifier( self, fieldid );
		DString *name = NULL;
		DaoType *hostype = NULL;
		DaoValue *host = (DaoValue*) self->nspace;
		DaoValue *value = NULL;
		DaoInterface *inter;
		DaoRoutine *routine;
		DaoClass *klass;
		DNode *it;

		if( hostid ){
			host = DaoByteDecoder_GetDeclaration( self, hostid );
			if( self->codes >= self->error ) break;
			switch( host->type ){
			case DAO_CLASS : hostype = host->xClass.objType; break;
			case DAO_INTERFACE : hostype = host->xInterface.abtype; break;
			}
		}
		if( dectype == DAO_DECL_CREATE ){
			int body = hostype == NULL || hostype->tid != DAO_INTERFACE;
			name = DaoByteDecoder_GetIdentifier( self, nameid );
			if( self->codes >= self->error ) break;
			switch( type ){
			case DAO_ROUTINE :
				if( subtype == DAO_ROUTINE_OVERLOADED ){
					routine = DaoRoutines_New( self->nspace, hostype, NULL );
				}else{
					routine = DaoRoutine_New( self->nspace, hostype, body );
				}
				DString_Assign( routine->routName, name );
				value = (DaoValue*) routine;
				break;
			case DAO_CLASS :
				klass = DaoClass_New();
				value = (DaoValue*) klass;
				DaoClass_SetName( klass, name, self->nspace );
				break;
			case DAO_INTERFACE :
				inter = DaoInterface_New( self->nspace, name->mbs );
				value = (DaoValue*) inter;
				break;
			default :
				break;
			}
		}else if( dectype == DAO_DECL_LOADAS ){
			value = DaoByteDecoder_GetValue( self, nameid );
		}else{
			name = DaoByteDecoder_GetIdentifier( self, nameid );
			if( field == NULL ) field = name;
			switch( host->type ){
			case DAO_NAMESPACE :
				value = DaoNamespace_GetData( (DaoNamespace*) host, field );
				break;
			case DAO_CLASS :
				id = DaoClass_FindConst( (DaoClass*)host, field );
				if( id >= 0 ) value = DaoClass_GetConst( (DaoClass*)host, id );
				break;
			case DAO_CTYPE :
				value = DaoType_FindValue( host->xCtype.ctype, field );
				break;
			case DAO_INTERFACE :
				it = DMap_Find( host->xInterface.methods, field );
				if( it ) value = it->value.pValue;
				break;
			default :
				break;
			}
			if( dectype == DAO_DECL_TEMPLATE && value->type == DAO_CTYPE ){
				value = value->xCtype.ctype->kernel->abtype->aux;
			}
		}
		if( self->codes >= self->error ) break;
		DArray_Append( self->declarations, value );
	}
}

void DaoArray_ResizeData( DaoArray *self, daoint size, daoint old );

static DaoValue* DaoByteDecoder_DecodeValue( DaoByteDecoder *self )
{
	int tid = DaoByteDecoder_DecodeUInt8( self );
	DaoStream *stream = self->vmspace->errorStream;
	DaoCtype *ctype;
	DaoCdata *cdata;
	DaoClass *klass;
	DaoObject *object;
	DaoRoutine *routine;
	DaoInterface *inter;
	DaoNameValue *nameva;
	DaoString *stringValue;
	DaoLong *longValue;
	DaoArray *array;
	DaoList *list;
	DaoMap *map;
	DaoTuple *tuple;
	DaoType *type;
	DaoType *hostype;
	DaoValue *value = NULL;
	DString *name;
	DNode *it;
	complex16 cvalue;
	double fvalue;
	daoint k, ivalue, ivalue2;
	int i, j, flag, count;
	int id, id2, id3;
	uint_t hashing;

	switch( tid ){
	case DAO_NONE :
		value = dao_none_value;
		break;
	case DAO_INTEGER :
		ivalue = DaoByteDecoder_DecodeDaoInt( self );
		value = (DaoValue*) DaoInteger_New( ivalue );
		break;
	case DAO_FLOAT :
		fvalue = DaoByteDecoder_DecodeDouble( self );
		value = (DaoValue*) DaoFloat_New( fvalue );
		break;
	case DAO_DOUBLE :
		fvalue = DaoByteDecoder_DecodeDouble( self );
		value = (DaoValue*) DaoDouble_New( fvalue );
		break;
	case DAO_COMPLEX :
		cvalue = DaoByteDecoder_DecodeComplex( self );
		value = (DaoValue*) DaoComplex_New( cvalue );
		break;

#ifdef DAO_WITH_LONGINT
	case DAO_LONG :
		flag = DaoByteDecoder_DecodeUInt8( self );
		ivalue = DaoByteDecoder_DecodeDaoInt( self );
		longValue = DaoLong_New();
		longValue->value->sign = flag ? -1 : 1;
		value = (DaoValue*) longValue;
		for(j =0; j<ivalue; ++j){
			uchar_t d = DaoByteDecoder_DecodeUInt8( self );
			DLong_PushBack( longValue->value, d );
		}
		break;
#endif

	case DAO_STRING :
		flag = DaoByteDecoder_DecodeUInt8( self );
		ivalue = DaoByteDecoder_DecodeDaoInt( self );
		stringValue = DaoString_New( (flag>>4) != 0 );
		value = (DaoValue*) stringValue;
		if( flag >> 4 ){
			DString_AppendDataMBS( stringValue->data, (char*)self->codes, ivalue );
			self->codes += ivalue;
		}else{
			flag = flag & 0xf;
			DString_Resize( stringValue->data, ivalue );
			for(j =0; j<ivalue; ++j){
				wchar_t ch = 0;
				switch( flag ){
				case 1 : ch = DaoByteDecoder_DecodeUInt8( self ); break;
				case 2 : ch = DaoByteDecoder_DecodeUInt16( self ); break;
				case 4 : ch = DaoByteDecoder_DecodeUInt32( self ); break;
				}
				stringValue->data->wcs[j] = ch;
			}
		}
		break;
	case DAO_ENUM :
		id = DaoByteDecoder_DecodeUInt( self );
		id2 = DaoByteDecoder_DecodeUInt32( self );
		type = DaoByteDecoder_GetType( self, id );
		if( self->codes >= self->error ) break;
		value = (DaoValue*) DaoEnum_New( type, id2 );
		break;

#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		flag = DaoByteDecoder_DecodeUInt8( self );
		ivalue = DaoByteDecoder_DecodeDaoInt( self );
		count = DaoByteDecoder_DecodeUInt16( self );
		array = DaoArray_New( flag );
		value = (DaoValue*) array;
		DaoArray_SetDimCount( array, count );
		for(j=0; j<count; ++j) array->dims[j] = DaoByteDecoder_DecodeDaoInt( self );
		DaoArray_FinalizeDimData( array );
		DaoArray_ResizeData( array, array->size, 0 );
		if( array->size != ivalue ){
			self->codes = self->error;
			break;
		}
		for(k=0; k<ivalue; ++k){
			switch( flag ){
			case DAO_INTEGER: array->data.i[k] = DaoByteDecoder_DecodeDaoInt(self); break;
			case DAO_FLOAT  : array->data.f[k] = DaoByteDecoder_DecodeDouble(self); break;
			case DAO_DOUBLE : array->data.d[k] = DaoByteDecoder_DecodeDouble(self); break;
			case DAO_COMPLEX: array->data.c[k] = DaoByteDecoder_DecodeComplex(self);break;
			}
		}
		break;
#endif

	case DAO_LIST :
		id = DaoByteDecoder_DecodeUInt( self );
		ivalue = DaoByteDecoder_DecodeDaoInt( self );
		type = DaoByteDecoder_GetType( self, id );
		if( self->codes >= self->error ) break;
		list = DaoList_New();
		value = (DaoValue*) list;
		GC_ShiftRC( type, list->unitype );
		list->unitype = type;
		for(k=0; k<ivalue; ++k){
			uint_t it = DaoByteDecoder_DecodeUInt32( self );
			DaoValue *va = DaoByteDecoder_GetValue( self, it );
			if( self->codes >= self->error ) break;
			DaoList_PushBack( list, va );
		}
		break;
	case DAO_MAP :
		id = DaoByteDecoder_DecodeUInt( self );
		hashing = DaoByteDecoder_DecodeUInt32( self );
		ivalue = DaoByteDecoder_DecodeDaoInt( self );
		type = DaoByteDecoder_GetType( self, id );
		if( self->codes >= self->error ) break;

		map = DaoMap_New( hashing );
		GC_ShiftRC( type, map->unitype );
		map->unitype = type;
		value = (DaoValue*) map;
		for(k=0; k<ivalue; ++k){
			uint_t key = DaoByteDecoder_DecodeUInt32( self );
			uint_t val = DaoByteDecoder_DecodeUInt32( self );
			DaoValue *key2 = DaoByteDecoder_GetValue( self, key );
			DaoValue *val2 = DaoByteDecoder_GetValue( self, val );
			if( self->codes >= self->error ) break;
			DaoMap_Insert( map, key2, val2 );
		}
		break;
	case DAO_TUPLE :
		id = DaoByteDecoder_DecodeUInt( self );
		ivalue = DaoByteDecoder_DecodeUInt32( self );
		type = DaoByteDecoder_GetType( self, id );
		if( self->codes >= self->error ) break;

		tuple = type ? DaoTuple_Create( type, ivalue, 0 ) : DaoTuple_New( ivalue );
		value = (DaoValue*) tuple;
		for(k=0; k<ivalue; ++k){
			uint_t it = DaoByteDecoder_DecodeUInt32( self );
			DaoValue *val = DaoByteDecoder_GetValue( self, it );
			if( self->codes >= self->error ) break;
			DaoTuple_SetItem( tuple, val, k );
			if( tuple->items[k] == NULL ){
				self->codes = self->error;
				break;
			}
		}
		break;
	case DAO_ROUTINE :
		id = DaoByteDecoder_DecodeUInt( self );
		id2 = DaoByteDecoder_DecodeUInt( self );
		id3 = DaoByteDecoder_DecodeUInt( self );
		flag = DaoByteDecoder_DecodeUInt16( self );
		count = DaoByteDecoder_DecodeUInt16( self );
		routine = (DaoRoutine*) DaoByteDecoder_GetDeclaration( self, id );
		type = DaoByteDecoder_GetType( self, id2 );
		hostype = DaoByteDecoder_GetType( self, id3 );
		if( self->codes >= self->error ) break;

		routine->parCount = type->variadic ? DAO_MAX_PARAM : type->nested->size;
		routine->attribs = flag;
		GC_ShiftRC( type, routine->routType );
		routine->routType = type;
		GC_ShiftRC( hostype, routine->routHost );
		routine->routHost = hostype;
		value = (DaoValue*) routine;
		for(i=0; i<count; ++i){
			int id = DaoByteDecoder_DecodeUInt( self );
			DaoRoutine *rout = (DaoRoutine*) DaoByteDecoder_GetValue( self, id );
			if( routine->overloads ) DRoutines_Add( routine->overloads, rout );
		}
		break;
	case DAO_OBJECT :
		flag = DaoByteDecoder_DecodeUInt8( self );
		id = DaoByteDecoder_DecodeUInt( self );
		klass = (DaoClass*) DaoByteDecoder_GetDeclaration( self, id );
		if( self->codes >= self->error ) break;
		if( flag == 0x2 ){
			value = klass->objType->value;
		}else if( flag == 0x1 ){
			/* No need to handle for now, because no class instance can
			// be constant and was serialized. */
		}else{
		}
		break;
	case DAO_INTERFACE :
		id = DaoByteDecoder_DecodeUInt( self );
		value = DaoByteDecoder_GetDeclaration( self, id );
		break;
	case DAO_CLASS :
	case DAO_CTYPE :
		id = DaoByteDecoder_DecodeUInt( self );
		value = DaoByteDecoder_GetDeclaration( self, id );
		break;
	case DAO_CDATA :
	case DAO_CSTRUCT :
		id = DaoByteDecoder_DecodeUInt( self );
		break;
	case DAO_TYPE :
		id = DaoByteDecoder_DecodeUInt( self );
		value = (DaoValue*) DaoByteDecoder_GetType( self, id );
		break;
	case DAO_NAMESPACE : // XXX
		id = DaoByteDecoder_DecodeUInt( self );
		value = id ? DaoByteDecoder_GetDeclaration( self, id ) : (DaoValue*)self->nspace;
		break;
	case DAO_PAR_NAMED :
		id = DaoByteDecoder_DecodeUInt( self );
		id2 = DaoByteDecoder_DecodeUInt( self );
		id3 = DaoByteDecoder_DecodeUInt( self );
		name = DaoByteDecoder_GetIdentifier( self, id );
		value = DaoByteDecoder_GetValue( self, id3 );
		if( self->codes >= self->error ) break;

		nameva = DaoNameValue_New( name, value );
		value = (DaoValue*) nameva;
		if( id2 ){
			nameva->unitype = DaoByteDecoder_GetType( self, id2 );
			GC_IncRC( nameva->unitype );
		}
		break;
	default:
		DaoStream_WriteMBS( stream, "Error: unsupported value type id " );
		DaoStream_WriteInt( stream, tid );
		DaoStream_WriteMBS( stream, ".\n" );
		self->codes = self->error;
		break;
	}
	return value;
}

void DaoByteDecoder_DecodeTypes( DaoByteDecoder *self )
{
	DNode *it;
	DaoValue *value;
	DaoType **types;
	int i, j, flag, count, numtypes = DaoByteDecoder_DecodeUInt( self );
	int id, id2, id3;
	const char *tname;

	for(i=0; i<numtypes; ++i){
		DaoValue *aux = NULL;
		DaoType *type2, *type = NULL;
		int tid = DaoByteDecoder_DecodeUInt8( self );
		int attrib = DaoByteDecoder_DecodeUInt8( self );
		int nameid = DaoByteDecoder_DecodeUInt( self );
		DString *name = DaoByteDecoder_GetIdentifier( self, nameid );
		DString *name2;

		switch( tid ){
		case DAO_ENUM :
			flag = DaoByteDecoder_DecodeUInt8( self );
			count = DaoByteDecoder_DecodeUInt16( self );
			DMap_Reset( self->map );
			for(j=0; j<count; ++j){
				int symID = DaoByteDecoder_DecodeUInt( self );
				int value = DaoByteDecoder_DecodeUInt32( self );
				DString *sym = DaoByteDecoder_GetIdentifier( self, symID );
				if( self->codes >= self->error ) break;
				DMap_Insert( self->map, sym, IntToPointer( value ) );
			}
			if( self->codes >= self->error ) break;
			type = DaoNamespace_FindType( self->nspace, name );
			if( type == NULL ){
				type = DaoType_New( name->mbs, DAO_ENUM, NULL, NULL );
				type->mapNames = self->map;
				self->map = DMap_New(D_STRING,0);
				type->flagtype = flag != 0;
			}
			break;
		case DAO_ARRAY : case DAO_LIST : case DAO_MAP :
		case DAO_TUPLE : case DAO_TYPE : case DAO_VARIANT :
			if( tid == DAO_VARIANT ){
				id = DaoByteDecoder_DecodeUInt( self );
				aux = (DaoValue*) DaoByteDecoder_GetType( self, id );
			}
			self->array->size = 0;
			count = DaoByteDecoder_DecodeUInt16( self );
			for(j=0; j<count; ++j){
				id = DaoByteDecoder_DecodeUInt( self );
				type = DaoByteDecoder_GetType( self, id );
				DArray_Append( self->array, type );
			}
			if( self->codes >= self->error ) break;
			types = self->array->items.pType;
			switch( tid ){
			case DAO_ARRAY   : tname = "array"; break;
			case DAO_LIST    : tname = "list"; break;
			case DAO_MAP     : tname = "map"; break;
			case DAO_TUPLE   : tname = "tuple"; break;
			case DAO_TYPE    : tname = "type"; break;
			case DAO_VARIANT : tname = aux ? aux->xType.name->mbs : ""; break;
			}
			type = DaoNamespace_MakeType( self->nspace, tname, tid, aux, types, count );
			break;
		case DAO_VALTYPE :
			value = DaoByteDecoder_DecodeValue( self );
			type = DaoNamespace_MakeValueType( self->nspace, value );
			GC_IncRC( value );
			GC_DecRC( value );
			break;
		case DAO_PAR_NAMED :
		case DAO_PAR_DEFAULT :
			id = DaoByteDecoder_DecodeUInt( self );
			id2 = DaoByteDecoder_DecodeUInt( self );
			name2 = DaoByteDecoder_GetIdentifier( self, id );
			value = (DaoValue*) DaoByteDecoder_GetType( self, id2 );
			if( self->codes >= self->error ) break;
			type = DaoNamespace_MakeType( self->nspace, name2->mbs, tid, value, NULL, 0 );
			break;
		case DAO_PAR_VALIST :
			id = DaoByteDecoder_DecodeUInt( self );
			id2 = DaoByteDecoder_DecodeUInt( self );
			value = (DaoValue*) DaoByteDecoder_GetType( self, id2 );
			if( self->codes >= self->error ) break;
			type = DaoNamespace_MakeType( self->nspace, "...", DAO_PAR_VALIST, value, NULL, 0 );
			break;
		case DAO_CODEBLOCK :
			id = DaoByteDecoder_DecodeUInt( self );
			count = DaoByteDecoder_DecodeUInt16( self );
			self->array->size = 0;
			for(j=0; j<count; ++j){
				int it = DaoByteDecoder_DecodeUInt( self );
				type = DaoByteDecoder_GetType( self, it );
				DArray_Append( self->array, type );
			}
			value = (DaoValue*) DaoByteDecoder_GetType( self, id );
			if( self->codes >= self->error ) break;
			types = self->array->items.pType;
			type = DaoNamespace_MakeType( self->nspace, "", tid|(attrib<<16), value, types, count );
			break;
		case DAO_ROUTINE :
			id = DaoByteDecoder_DecodeUInt( self );
			id2 = DaoByteDecoder_DecodeUInt( self );
			id3 = DaoByteDecoder_DecodeUInt( self );
			count = DaoByteDecoder_DecodeUInt16( self );
			self->array->size = 0;
			for(j=0; j<count; ++j){
				int it = DaoByteDecoder_DecodeUInt( self );
				type = DaoByteDecoder_GetType( self, it );
				DArray_Append( self->array, type );
			}
			types = self->array->items.pType;
			if( id ){
				value = DaoByteDecoder_GetDeclaration( self, id );
				if( value == NULL || value->type != DAO_ROUTINE ) self->codes = self->error;
				if( self->codes >= self->error ) break;
				type = value->xRoutine.routType;
			}else{
				value = (DaoValue*) DaoByteDecoder_GetType( self, id2 );
				if( self->codes >= self->error ) break;
				if( value != NULL && value->type != DAO_TYPE ) self->codes = self->error;
				type = DaoNamespace_MakeType( self->nspace, "routine", tid|(attrib<<16), value, types, count );
				if( id3 ){
					DString *name = self->string;
					DaoType *cbtype = DaoByteDecoder_GetType( self, id3 );
					DaoType *type2;

					if( self->codes >= self->error ) break;
					DString_Assign( name, type->name );
					DString_Append( name, cbtype->name );
					type = DaoType_New( name->mbs, DAO_ROUTINE, type->aux, type->nested );
					type->attrib = attrib;
					type->cbtype = cbtype;
					GC_IncRC( cbtype );
					type2 = DaoNamespace_FindType( self->nspace, name );
					if( DaoType_MatchTo( type, type2, NULL ) == DAO_MT_EQ ){
						GC_IncRC( type );
						GC_DecRC( type );
						type = type2;
					}else{
						DaoNamespace_AddType( self->nspace, name, type );
					}
				}
			}
			break;
		case DAO_INTERFACE :
			id = DaoByteDecoder_DecodeUInt( self );
			value = DaoByteDecoder_GetDeclaration( self, id );
			if( self->codes >= self->error ) break;
			type = value->xInterface.abtype;
			break;
		case DAO_CLASS :
			id = DaoByteDecoder_DecodeUInt( self );
			value = DaoByteDecoder_GetDeclaration( self, id );
			if( self->codes >= self->error ) break;
			type = value->xClass.clsType;
			break;
		case DAO_OBJECT :
			id = DaoByteDecoder_DecodeUInt( self );
			value = DaoByteDecoder_GetDeclaration( self, id );
			if( self->codes >= self->error ) break;
			type = value->xClass.objType;
			break;
		case DAO_CTYPE :
		case DAO_CDATA :
		case DAO_CSTRUCT :
			id = DaoByteDecoder_DecodeUInt( self );
			count = DaoByteDecoder_DecodeUInt16( self );
			value = DaoByteDecoder_GetDeclaration( self, id );
			if( self->codes >= self->error ) break;
			type = value->xCtype.ctype;
			if( tid == DAO_CDATA || tid == DAO_CSTRUCT ) type = value->xCtype.cdtype;
			if( count ){
				self->array->size = 0;
				for(j=0; j<count; ++j){
					int it = DaoByteDecoder_DecodeUInt( self );
					DaoType *tp = DaoByteDecoder_GetType( self, it );
					DArray_Append( self->array, tp );
				}
				type = DaoCdataType_Specialize( type, self->array->items.pType, self->array->size );
			}
			break;
		case DAO_ANY :
			type = DaoNamespace_MakeType( self->nspace, "any", tid, NULL, NULL, 0 );
			break;
		case DAO_THT :
			type = DaoNamespace_MakeType( self->nspace, name->mbs, tid, NULL, NULL, 0 );
			break;
		case DAO_NAMESPACE :
			type = DaoNamespace_MakeType( self->nspace, "namespace", tid, NULL, NULL, 0 );
			break;
		default :
			type = DaoNamespace_MakeType( self->nspace, name->mbs, tid, NULL, NULL, 0 );
			break;
		}
		type->attrib = attrib;
		DaoType_CheckAttributes( type );
		if( DString_EQ( name, type->name ) == 0 ){
			type = DaoType_Copy( type );
			DString_Assign( type->name, name );
			DaoNamespace_AddType( self->nspace, name, type );
		}
		DArray_Append( self->types, type );
		if( self->codes >= self->error ) break;
	}
}
void DaoByteDecoder_DecodeValues( DaoByteDecoder *self )
{
	DNode *it;
	DaoType *type;
	DaoType *hostype;
	double fvalue;
	complex16 cvalue;
	daoint k, ivalue, ivalue2;
	int numvalues = DaoByteDecoder_DecodeUInt32( self );
	int i, j, flag, count;
	int id, id2, id3;
	uint_t hashing;

	for(i=0; i<numvalues; ++i){
		DaoValue *value = DaoByteDecoder_DecodeValue( self );
		DArray_Append( self->values, NULL );
		GC_IncRC( value );
		self->values->items.pValue[ self->values->size - 1 ] = value;
		if( self->codes >= self->error ) break;
	}
}
void DaoByteDecoder_DecodeConstants( DaoByteDecoder *self )
{
	DArray *constants = self->nspace->constants;
	int i, j, flag, count, numconsts = DaoByteDecoder_DecodeUInt( self );
	int id, id2, id3;
	for(i=0; i<numconsts; ++i){
		int nameID = DaoByteDecoder_DecodeUInt( self );
		int index = DaoByteDecoder_DecodeUInt16( self );
		int perm = DaoByteDecoder_DecodeUInt8( self );
		int valueID = DaoByteDecoder_DecodeUInt( self );
		DString *name = DaoByteDecoder_GetIdentifier( self, nameID );
		DaoValue *value = DaoByteDecoder_GetValue( self, valueID );

		if( self->codes >= self->error ) break;
		if( index > constants->size ) goto DecodingError;
		if( value == NULL ){
			self->codes = self->error;
			break;
		}

		if( index == LOOKUP_ID(DVR_NSC_MAIN) && value && value->type == DAO_ROUTINE ){
			if( index >= constants->size ) goto DecodingError;
			GC_ShiftRC( value, constants->items.pConst[index]->value );
			GC_ShiftRC( value, self->nspace->mainRoutine );
			self->nspace->mainRoutine = (DaoRoutine*) value;
			constants->items.pConst[index]->value = value;
		}else if( name && value && (value->type == DAO_ROUTINE || index == constants->size) ){
			int id = DaoNamespace_AddConst( self->nspace, name, value, perm );
			if( id < 0 ) goto DecodingError;
		}else if( index == constants->size ){
			DArray_Append( constants, DaoConstant_New( value ) );
		}else{
			DaoNamespace_SetConst( self->nspace, index, value );
		}
		if( name ){
			int lk = DaoNamespace_FindConst( self->nspace, name );
			if( lk >= 0 && index != LOOKUP_ID( lk ) ) goto DecodingError;
		}
	}
	return;
DecodingError:
	self->codes = self->error;
}
void DaoByteDecoder_DecodeVariables( DaoByteDecoder *self )
{
	DArray *variables = self->nspace->variables;
	int i, j, flag, count, numvars = DaoByteDecoder_DecodeUInt( self );
	int id, id2, id3;
	for(i=0; i<numvars; ++i){
		int nameID = DaoByteDecoder_DecodeUInt( self );
		int index = DaoByteDecoder_DecodeUInt16( self );
		int perm = DaoByteDecoder_DecodeUInt8( self );
		int typeID = DaoByteDecoder_DecodeUInt( self );
		int valueID = DaoByteDecoder_DecodeUInt( self );
		DString *name = DaoByteDecoder_GetIdentifier( self, nameID );
		DaoType *type = DaoByteDecoder_GetType( self, typeID );
		DaoValue *value = DaoByteDecoder_GetValue( self, valueID );

		if( self->codes >= self->error ) break;
		if( index > variables->size ) goto DecodingError;

		if( name && value ){
			int id = DaoNamespace_AddVariable( self->nspace, name, value, type, perm );
			if( id < 0 ) goto DecodingError;
		}else if( index == variables->size ){
			DArray_Append( variables, DaoVariable_New( value, type ) );
		}else{
			DaoVariable_Set( variables->items.pVar[index], value, type );
		}
		if( name ){
			int lk = DaoNamespace_FindVariable( self->nspace, name );
			if( lk >= 0 && index != LOOKUP_ID( lk ) ) goto DecodingError;
		}
	}
	return;
DecodingError:
	self->codes = self->error;
}
void DaoByteDecoder_DecodeGlobalTypes( DaoByteDecoder *self )
{
	int i, j, flag, count, numtypes = DaoByteDecoder_DecodeUInt( self );
	int id, id2, id3;
	for(i=0; i<numtypes; ++i){
		int nameID = DaoByteDecoder_DecodeUInt( self );
		int typeID = DaoByteDecoder_DecodeUInt( self );
		DString *name = DaoByteDecoder_GetIdentifier( self, nameID );
		DaoType *type = DaoByteDecoder_GetType( self, typeID );
		if( type == NULL ) self->codes = self->error;
		if( self->codes >= self->error ) break;
		DaoNamespace_AddType( self->nspace, name, type );
	}
}
void DaoByteDecoder_DecodeInterfaces( DaoByteDecoder *self )
{
	int i, j, flag, count, num = DaoByteDecoder_DecodeUInt( self );
	int id, id2, id3;
	for(i=0; i<num; ++i){
		int interID = DaoByteDecoder_DecodeUInt( self );
		DaoInterface *inter = (DaoInterface*) DaoByteDecoder_GetDeclaration( self, interID );
		if( self->codes >= self->error ) break;
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int supid = DaoByteDecoder_DecodeUInt( self );
			DaoValue *sup = DaoByteDecoder_GetDeclaration( self, supid );
			if( self->codes >= self->error ) break;
			DArray_Append( inter->supers, sup );
		}
		if( self->codes >= self->error ) break;
		DaoInterface_DeriveMethods( inter );
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int methid = DaoByteDecoder_DecodeUInt32( self );
			DaoRoutine *rout = (DaoRoutine*) DaoByteDecoder_GetValue( self, methid );
			if( self->codes >= self->error ) break;
			DaoMethods_Insert( inter->methods, rout, self->nspace, rout->routHost );
		}
		if( self->codes >= self->error ) break;
	}
}
void DaoByteDecoder_DecodeClasses( DaoByteDecoder *self )
{
	int i, j, flag, count, num = DaoByteDecoder_DecodeUInt( self );
	int id, id2, id3;
	for(i=0; i<num; ++i){
		int classID = DaoByteDecoder_DecodeUInt( self );
		int cstrID = DaoByteDecoder_DecodeUInt( self );
		int attribs = DaoByteDecoder_DecodeUInt16( self );
		DaoClass *klass = (DaoClass*) DaoByteDecoder_GetDeclaration( self, classID );
		DaoRoutine *rout = (DaoRoutine*) DaoByteDecoder_GetDeclaration( self, cstrID );
		DArray *constants = klass->constants;
		DArray *variables = klass->variables;
		DArray *instvars = klass->instvars;

		if( self->codes >= self->error ) break;

		GC_ShiftRC( rout, klass->classRoutine );
		klass->classRoutine = rout;
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int supid = DaoByteDecoder_DecodeUInt( self );
			DaoValue *sup = DaoByteDecoder_GetDeclaration( self, supid );
			if( self->codes >= self->error ) break;
			DaoClass_AddSuperClass( klass, sup );
		}
		if( self->codes >= self->error ) break;
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int supid = DaoByteDecoder_DecodeUInt( self );
			DaoValue *mixin = DaoByteDecoder_GetDeclaration( self, supid );
			if( self->codes >= self->error ) break;
			DaoClass_AddMixinClass( klass, (DaoClass*) mixin );
		}
		if( self->codes >= self->error ) break;
		if( DaoClass_DeriveClassData( klass ) == 0 ) self->codes = self->error;
		if( self->codes >= self->error ) break;
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int nameID = DaoByteDecoder_DecodeUInt( self );
			int index = DaoByteDecoder_DecodeUInt16( self );
			int perm = DaoByteDecoder_DecodeUInt8( self );
			int valueID = DaoByteDecoder_DecodeUInt( self );
			DString *name = DaoByteDecoder_GetIdentifier( self, nameID );
			DaoValue *value = DaoByteDecoder_GetValue( self, valueID );

			if( self->codes >= self->error ) break;
			if( index > constants->size ) goto DecodingError;

			if( name && value && (value->type == DAO_ROUTINE || index == constants->size) ){
				int id = DaoClass_AddConst( klass, name, value, perm );
				if( id < 0 ) goto DecodingError;
			}else if( index == constants->size ){
				int id = DaoClass_AddConst( klass, name, value, perm );
				if( id < 0 ) goto DecodingError;
			}else{
				DaoClass_SetConst( klass, index, value );
			}
			if( name ){
				int lk = DaoClass_FindConst( klass, name );
				if( lk >= 0 && index != LOOKUP_ID( lk ) ) goto DecodingError;
			}
		}
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int nameID = DaoByteDecoder_DecodeUInt( self );
			int index = DaoByteDecoder_DecodeUInt16( self );
			int perm = DaoByteDecoder_DecodeUInt8( self );
			int typeID = DaoByteDecoder_DecodeUInt( self );
			int valueID = DaoByteDecoder_DecodeUInt( self );
			DString *name = DaoByteDecoder_GetIdentifier( self, nameID );
			DaoType *type = DaoByteDecoder_GetType( self, typeID );
			DaoValue *value = DaoByteDecoder_GetValue( self, valueID );

			if( self->codes >= self->error ) break;
			if( index > variables->size ) goto DecodingError;

			if( index == variables->size ){
				int id = DaoClass_AddGlobalVar( klass, name, value, type, perm );
				if( id < 0 ) goto DecodingError;
			}else{
				DaoVariable_Set( variables->items.pVar[index], value, type );
			}
			if( name ){
				int lk = DaoClass_GetDataIndex( klass, name );
				if( lk >= 0 && index != LOOKUP_ID( lk ) ) goto DecodingError;
			}
		}
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int nameID = DaoByteDecoder_DecodeUInt( self );
			int index = DaoByteDecoder_DecodeUInt16( self );
			int perm = DaoByteDecoder_DecodeUInt8( self );
			int typeID = DaoByteDecoder_DecodeUInt( self );
			int valueID = DaoByteDecoder_DecodeUInt( self );
			DString *name = DaoByteDecoder_GetIdentifier( self, nameID );
			DaoType *type = DaoByteDecoder_GetType( self, typeID );
			DaoValue *value = DaoByteDecoder_GetValue( self, valueID );

			if( self->codes >= self->error ) break;
			if( index > instvars->size ) goto DecodingError;

			if( index == instvars->size ){
				int id = DaoClass_AddObjectVar( klass, name, value, type, perm );
				if( id < 0 ) goto DecodingError;
			}else{
				DaoVariable_Set( instvars->items.pVar[index], value, type );
			}
			if( name ){
				int lk = DaoClass_GetDataIndex( klass, name );
				if( lk >= 0 && index != LOOKUP_ID( lk ) ) goto DecodingError;
			}
		}
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int nameid = DaoByteDecoder_DecodeUInt( self );
			int typeid = DaoByteDecoder_DecodeUInt( self );
			DString *name = DaoByteDecoder_GetIdentifier( self, nameid );
			DaoType *type = DaoByteDecoder_GetType( self, typeid );
			if( type == NULL ) self->codes = self->error;
			if( self->codes >= self->error ) break;
			DaoClass_AddType( klass, name, type );
		}
		if( self->codes >= self->error ) break;
		DaoClass_DeriveObjectData( klass );
		klass->attribs = attribs;
	}
	return;
DecodingError:
	self->codes = self->error;
}
static void DaoByteDecoder_VerifyRoutine( DaoByteDecoder *self, DaoRoutine *routine )
{
	DMap *outer = DHash_New(0,0);
	DArray *outers = DArray_New(D_MAP);
	DaoInferencer *inferencer;
	int regCount = routine->body->regCount;
	int i, T, M, N = routine->body->annotCodes->size;
	char buf[200];

	DArray_PushBack( outers, outer );
	DArray_PushBack( outers, outer );
	for(i=0; i<N; ++i){
		DMap *current = (DMap*) DArray_Back( outers );
		DaoVmCode *vmc2, *vmc = (DaoVmCode*) routine->body->annotCodes->items.pVmc[i];

		if( vmc->code >= DVM_NULL ) goto InvalidInstruction;

#if 0
		DaoVmCode_Print( *vmc, buf );
		printf( "%3i: %s", i, buf );
#endif

		T = DaoVmCode_GetOpcodeType( vmc );
		switch( T ){
		case DAO_CODE_GETU :
			if( vmc->c >= regCount ) goto InvalidInstruction;
			DMap_Insert( current, IntToPointer( vmc->c ), 0 );
			if( vmc->a ){
				DMap *map = outers->items.pMap[vmc->a];
				if( vmc->a >= outers->size ) goto InvalidInstruction;
				if( vmc->b >= regCount ) goto InvalidInstruction;
				if( DMap_Find( map, IntToPointer(vmc->b) ) == NULL ) goto InvalidInstruction;
			}
			break;
		case DAO_CODE_SETU :
			if( vmc->a >= regCount ) goto InvalidInstruction;
			if( vmc->c ){
				DMap *map = outers->items.pMap[vmc->c];
				if( vmc->c >= outers->size ) goto InvalidInstruction;
				if( vmc->b >= regCount ) goto InvalidInstruction;
				if( DMap_Find( map, IntToPointer(vmc->b) ) == NULL ) goto InvalidInstruction;
			}
			break;
		case DAO_CODE_GETC :
		case DAO_CODE_GETG :
			if( vmc->c >= regCount ) goto InvalidInstruction;
			switch( vmc->code ){
			case DVM_GETVS :
			case DVM_GETVS_I : case DVM_GETVS_F :
			case DVM_GETVS_D : case DVM_GETVS_C :
				if( vmc->b >= routine->body->svariables->size ) goto InvalidInstruction;
			}
			DMap_Insert( current, IntToPointer( vmc->c ), 0 );
			break;
		case DAO_CODE_SETG :
			if( vmc->a >= regCount ) goto InvalidInstruction;
			switch( vmc->code ){
			case DVM_SETVS :
			case DVM_SETVS_II : case DVM_SETVS_FF :
			case DVM_SETVS_DD : case DVM_SETVS_CC :
				if( vmc->b >= routine->body->svariables->size ) goto InvalidInstruction;
			}
			break;
		case DAO_CODE_GETF :
		case DAO_CODE_SETF :
		case DAO_CODE_MOVE :
		case DAO_CODE_UNARY :
			if( vmc->a >= regCount ) goto InvalidInstruction;
			if( vmc->c >= regCount ) goto InvalidInstruction;
			if( T != DAO_CODE_SETF ) DMap_Insert( current, IntToPointer( vmc->c ), 0 );
			break;
		case DAO_CODE_UNARY2 :
			if( vmc->b >= regCount ) goto InvalidInstruction;
			if( vmc->c >= regCount ) goto InvalidInstruction;
			DMap_Insert( current, IntToPointer( vmc->c ), 0 );
			break;
		case DAO_CODE_GETM :
		case DAO_CODE_SETM :
			if( vmc->a >= regCount ) goto InvalidInstruction;
			if( vmc->c >= regCount ) goto InvalidInstruction;
			if( (vmc->a + vmc->b) >= regCount ) goto InvalidInstruction;
			if( T == DAO_CODE_GETM ) DMap_Insert( current, IntToPointer( vmc->c ), 0 );
			break;
		case DAO_CODE_GETI :
		case DAO_CODE_SETI :
		case DAO_CODE_BINARY :
			if( vmc->a >= regCount ) goto InvalidInstruction;
			if( vmc->b >= regCount ) goto InvalidInstruction;
			if( vmc->c >= regCount ) goto InvalidInstruction;
			if( T != DAO_CODE_SETI ) DMap_Insert( current, IntToPointer( vmc->c ), 0 );
			break;
		case DAO_CODE_MATRIX :
			M = (vmc->b>>8)*(vmc->b&0xff);
			if( vmc->a >= regCount ) goto InvalidInstruction;
			if( vmc->c >= regCount ) goto InvalidInstruction;
			if( (vmc->a + M) >= regCount ) goto InvalidInstruction;
			DMap_Insert( current, IntToPointer( vmc->c ), 0 );
			break;
		case DAO_CODE_ENUM :
		case DAO_CODE_YIELD :
			if( vmc->a >= regCount ) goto InvalidInstruction;
			if( vmc->c >= regCount ) goto InvalidInstruction;
			if( (vmc->a + vmc->b - 1) >= regCount ) goto InvalidInstruction;
			DMap_Insert( current, IntToPointer( vmc->c ), 0 );
			break;
		case DAO_CODE_CALL :
			M = vmc->b & 0xff;
			if( vmc->a >= regCount ) goto InvalidInstruction;
			if( vmc->c >= regCount ) goto InvalidInstruction;
			if( (vmc->a + M) >= regCount ) goto InvalidInstruction;
			DMap_Insert( current, IntToPointer( vmc->c ), 0 );
			break;
		case DAO_CODE_ENUM2 :
		case DAO_CODE_ROUTINE :
			if( vmc->a >= regCount ) goto InvalidInstruction;
			if( vmc->c >= regCount ) goto InvalidInstruction;
			if( (vmc->a + vmc->b) >= regCount ) goto InvalidInstruction;
			DMap_Insert( current, IntToPointer( vmc->c ), 0 );
			break;
		case DAO_CODE_EXPLIST :
			if( vmc->b == 0 ) break;
			if( vmc->a >= regCount ) goto InvalidInstruction;
			if( (vmc->a + vmc->b - 1) >= regCount ) goto InvalidInstruction;
			break;
		case DAO_CODE_BRANCH :
			if( vmc->a >= regCount ) goto InvalidInstruction;
			if( vmc->b >= N ) goto InvalidInstruction;
			break;
		case DAO_CODE_JUMP :
			if( vmc->b >= N ) goto InvalidInstruction;
			break;
		default :
			break;
		}
		switch( vmc->code ){
		case DVM_SECT :
			DArray_PushBack( outers, outer );
			break;
		case DVM_GOTO :
			if( vmc->b >= i ) break;
			vmc2 = (DaoVmCode*) routine->body->annotCodes->items.pVmc[vmc->b+1];
			if( vmc2->code == DVM_SECT ) DArray_PopBack( outers );
			break;
		}
	}
	DMap_Delete( outer );
	DArray_Delete( outers );
	inferencer = DaoInferencer_New();
	DaoInferencer_Init( inferencer, routine, 0 );
	if( DaoInferencer_DoInference( inferencer ) == 0 ) self->codes = self->error;
	DaoInferencer_Delete( inferencer );
	return;
InvalidInstruction:
	DaoStream_WriteMBS( self->vmspace->errorStream, "ERROR: code verification failed for " );
	DaoStream_WriteString( self->vmspace->errorStream, routine->routName );
	DaoStream_WriteMBS( self->vmspace->errorStream, "()\n" );
	self->codes = self->error;
	DArray_Delete( outers );
	DMap_Delete( outer );
}
void DaoByteDecoder_DecodeRoutines( DaoByteDecoder *self )
{
	DArray *lines = DArray_New(0);
	int num = DaoByteDecoder_DecodeUInt( self );
	int i, j, k, m, flag, count, lineInfoCount;
	int id, id2, id3;
	for(i=0; i<num; ++i){
		int routid = DaoByteDecoder_DecodeUInt( self );
		int nameid = DaoByteDecoder_DecodeUInt( self );
		int typeid = DaoByteDecoder_DecodeUInt( self );
		int hostid = DaoByteDecoder_DecodeUInt( self );
		int attribs = DaoByteDecoder_DecodeUInt16( self );
		int line = DaoByteDecoder_DecodeUInt16( self );
		int count = DaoByteDecoder_DecodeUInt16( self );
		DaoRoutine *routine = (DaoRoutine*) DaoByteDecoder_GetDeclaration( self, routid );
		DaoType *routype = DaoByteDecoder_GetType( self, typeid );
		DaoType *hostype = DaoByteDecoder_GetType( self, hostid );

		if( routype != routine->routType ) self->codes = self->error;
		if( hostype != routine->routHost ) self->codes = self->error;
		if( attribs != routine->attribs ) self->codes = self->error;
		if( self->codes >= self->error ) break;

		routine->defLine = line;
		for(j=0; j<count; ++j){
			int valueid = DaoByteDecoder_DecodeUInt( self );
			DaoValue *value = DaoByteDecoder_GetValue( self, valueid );
			if( self->codes >= self->error ) break;
			DaoRoutine_AddConstant( routine, value );
		}
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int typeid = DaoByteDecoder_DecodeUInt( self );
			int valueid = DaoByteDecoder_DecodeUInt( self );
			DaoType *type = DaoByteDecoder_GetType( self, typeid );
			DaoValue *value = DaoByteDecoder_GetValue( self, valueid );
			if( self->codes >= self->error ) break;
			DArray_Append( routine->body->svariables, DaoVariable_New( value, type ) );
		}

		routine->body->regCount = DaoByteDecoder_DecodeUInt16( self );
		count = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<count; ++j){
			int id = DaoByteDecoder_DecodeUInt16( self );
			int id2 = DaoByteDecoder_DecodeUInt( self );
			DaoType *type = DaoByteDecoder_GetType( self, id2 );
			if( self->codes >= self->error ) break;
			DMap_Insert( routine->body->localVarType, IntToPointer(id), type );
		}
		lines->size = 0;
		lineInfoCount = DaoByteDecoder_DecodeUInt16( self );
		for(j=0; j<lineInfoCount; ++j){
			int L = DaoByteDecoder_DecodeUInt16( self );
			int C = DaoByteDecoder_DecodeUInt8( self );
			DArray_Append( lines, IntToPointer(L) );
			DArray_Append( lines, IntToPointer(C) );
		}
		count = DaoByteDecoder_DecodeUInt16( self );
		m = lines->size > 1 ? lines->items.pInt[1] : 0;
		for(j=0, k=1;  j<count;  ++j){
			DaoVmCodeX vmc = {0,0,0,0,0,0,0,0,0};
			if( j >= m && k < lineInfoCount ){
				m += lines->items.pInt[2*k+1];
				k += 1;
			}
			vmc.code = DaoByteDecoder_DecodeUInt16( self );
			vmc.a = DaoByteDecoder_DecodeUInt16( self );
			vmc.b = DaoByteDecoder_DecodeUInt16( self );
			vmc.c = DaoByteDecoder_DecodeUInt16( self );
			vmc.line = k < lineInfoCount ? lines->items.pInt[2*(k-1)] : line;
			DArray_Append( routine->body->annotCodes, & vmc );
			DVector_PushCode( routine->body->vmCodes, * (DaoVmCode*) & vmc );
		}
		if( self->codes >= self->error ) break;
		DArray_Append( self->routines, routine );
	}
	DArray_Delete( lines );
}

int DaoByteDecoder_Decode( DaoByteDecoder *self, DString *input, DaoNamespace *nspace )
{
	daoint i;
	DString header = *input;
	DString signature = DString_WrapBytes( DAO_BC_SIGNATURE, 8 );

	if( header.size > 8 ) header.size = 8;
	if( DString_EQ( & header, & signature ) == 0 ) return 0;
	if( input->mbs[8] != 0 ) return 0; /* Not official format; */

	self->intSize = input->mbs[9];
	self->nspace = nspace;
	self->codes = (uchar_t*) input->mbs + 16;
	self->end = (uchar_t*) input->mbs + input->size;
	self->error = self->end + 1;
	if( self->intSize != 4 && self->intSize != 8 ) self->codes = self->error;
	DaoByteDecoder_DecodeShortString( self, self->string );

	DaoByteDecoder_DecodeIdentifiers( self );
	DaoByteDecoder_DecodeModules( self );
	DaoByteDecoder_DecodeDeclarations( self );
	DaoByteDecoder_DecodeTypes( self );
	DaoByteDecoder_DecodeValues( self );
	DaoByteDecoder_DecodeConstants( self );
	DaoByteDecoder_DecodeVariables( self );
	DaoByteDecoder_DecodeGlobalTypes( self );
	DaoByteDecoder_DecodeRoutines( self );
	DaoByteDecoder_DecodeInterfaces( self );
	DaoByteDecoder_DecodeClasses( self );
#if 0
	printf( "debug: %i\n", self->codes >= self->error );
#endif
	for(i=0; i<self->routines->size; ++i){
		DaoRoutine *routine = self->routines->items.pRoutine[i];
		DaoByteDecoder_VerifyRoutine( self, routine );
		if( self->codes >= self->error ) break;
		//DaoRoutine_PrintCode( routine, self->vmspace->stdioStream );
	}

	if( self->codes >= self->error ){
		DaoStream_WriteMBS( self->vmspace->errorStream, "ERROR: bytecode decoding failed!\n" );
	}

	return self->codes <= self->end;
}
