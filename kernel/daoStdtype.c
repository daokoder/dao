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

#include"stdlib.h"
#include"stdio.h"
#include"string.h"
#include"ctype.h"
#include"math.h"

#include"daoType.h"
#include"daoVmspace.h"
#include"daoNamespace.h"
#include"daoNumtype.h"
#include"daoStream.h"
#include"daoRoutine.h"
#include"daoObject.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoStdlib.h"
#include"daoClass.h"
#include"daoParser.h"
#include"daoMacro.h"
#include"daoRegex.h"
#include"daoSched.h"
#include"daoValue.h"

int ObjectProfile[100];

void DaoValue_Init( void *value, char type )
{
	DaoNone *self = (DaoNone*) value;
	self->type = type;
	self->subtype = self->trait = self->marks = 0;
	self->refCount = 0;
	if( type >= DAO_ENUM ) ((DaoValue*)self)->xGC.cycRefCount = 0;
#ifdef DAO_GC_PROF
	if( type < 100 )  ObjectProfile[(int)type] ++;
#endif
}

DaoNone* DaoNone_New()
{
	DaoNone *self = (DaoNone*) dao_malloc( sizeof(DaoNone) );
	memset( self, 0, sizeof(DaoNone) );
	self->type = DAO_NONE;
	return self;
}
DaoInteger* DaoInteger_New( daoint value )
{
	DaoInteger *self = (DaoInteger*) dao_malloc( sizeof(DaoInteger) );
	memset( self, 0, sizeof(DaoInteger) );
	self->type = DAO_INTEGER;
	self->value = value;
	return self;
}
daoint DaoInteger_Get( DaoInteger *self )
{
	return self->value;
}
void DaoInteger_Set( DaoInteger *self, daoint value )
{
	self->value = value;
}

DaoFloat* DaoFloat_New( float value )
{
	DaoFloat *self = (DaoFloat*) dao_malloc( sizeof(DaoFloat) );
	memset( self, 0, sizeof(DaoFloat) );
	self->type = DAO_FLOAT;
	self->value = value;
	return self;
}
float DaoFloat_Get( DaoFloat *self )
{
	return self->value;
}
void DaoFloat_Set( DaoFloat *self, float value )
{
	self->value = value;
}

DaoDouble* DaoDouble_New( double value )
{
	DaoDouble *self = (DaoDouble*) dao_malloc( sizeof(DaoDouble) );
	memset( self, 0, sizeof(DaoDouble) );
	self->type = DAO_DOUBLE;
	self->value = value;
	return self;
}
double DaoDouble_Get( DaoDouble *self )
{
	return self->value;
}
void DaoDouble_Set( DaoDouble *self, double value )
{
	self->value = value;
}

DaoComplex* DaoComplex_New( complex16 value )
{
	DaoComplex *self = (DaoComplex*) dao_malloc( sizeof(DaoComplex) );
	memset( self, 0, sizeof(DaoComplex) );
	self->type = DAO_COMPLEX;
	self->value = value;
	return self;
}
complex16  DaoComplex_Get( DaoComplex *self )
{
	return self->value;
}
void DaoComplex_Set( DaoComplex *self, complex16 value )
{
	self->value = value;
}

DaoLong* DaoLong_New()
{
	DaoLong *self = (DaoLong*) dao_malloc( sizeof(DaoLong) );
	DaoValue_Init( self, DAO_LONG );
	self->value = DLong_New();
	return self;
}
DLong* DaoLong_Get( DaoLong *self )
{
	return self->value;
}
DaoLong* DaoLong_Copy( DaoLong *self )
{
	DaoLong *copy = (DaoLong*) dao_malloc( sizeof(DaoLong) );
	DaoValue_Init( copy, DAO_LONG );
	copy->value = DLong_New();
	DLong_Move( copy->value, self->value );
	return copy;
}
void DaoLong_Delete( DaoLong *self )
{
	DLong_Delete( self->value );
	dao_free( self );
}
void DaoLong_Set( DaoLong *self, DLong *value )
{
	DLong_Move( self->value, value );
}

DaoString* DaoString_New( int mbs )
{
	DaoString *self = (DaoString*) dao_malloc( sizeof(DaoString) );
	DaoValue_Init( self, DAO_STRING );
	self->data = DString_New( mbs );
	return self;
}
DaoString* DaoString_NewMBS( const char *mbs )
{
	DaoString *self = DaoString_New(1);
	DString_SetMBS( self->data, mbs );
	return self;
}
DaoString* DaoString_NewWCS( const wchar_t *wcs )
{
	DaoString *self = DaoString_New(0);
	DString_SetWCS( self->data, wcs );
	return self;
}
DaoString* DaoString_NewBytes( const char *bytes, daoint n )
{
	DaoString *self = DaoString_New(1);
	DString_SetDataMBS( self->data, bytes, n );
	return self;
}
DaoString* DaoString_Copy( DaoString *self )
{
	DaoString *copy = (DaoString*) dao_malloc( sizeof(DaoString) );
	DaoValue_Init( copy, DAO_STRING );
	copy->data = DString_Copy( self->data );
	return copy;
}
void DaoString_Delete( DaoString *self )
{
	DString_Delete( self->data );
	dao_free( self );
}
daoint  DaoString_Size( DaoString *self )
{
	return self->data->size;
}
DString* DaoString_Get( DaoString *self )
{
	return self->data;
}
const char* DaoString_GetMBS( DaoString *self )
{
	return DString_GetMBS( self->data );
}
const wchar_t* DaoString_GetWCS( DaoString *self )
{
	return DString_GetWCS( self->data );
}

void DaoString_Set( DaoString *self, DString *str )
{
	DString_Assign( self->data, str );
}
void DaoString_SetMBS( DaoString *self, const char *mbs )
{
	DString_SetMBS( self->data, mbs );
}
void DaoString_SetWCS( DaoString *self, const wchar_t *wcs )
{
	DString_SetWCS( self->data, wcs );
}
void DaoString_SetBytes( DaoString *self, const char *bytes, daoint n )
{
	DString_SetDataMBS( self->data, bytes, n );
}

enum{
	IDX_NULL,
	IDX_SINGLE,
	IDX_FROM,
	IDX_TO,
	IDX_PAIR,
	IDX_ALL,
	IDX_MULTIPLE, 
	IDX_NONUMINDEX, 
	IDX_OUTOFRANGE
};

static DaoValue* DaoValue_MakeCopy( DaoValue *self, DaoProcess *proc, DMap *cycData )
{
	DaoTypeBase *typer;
	if( self->type <= DAO_COMPLEX ) return self;
	typer = DaoValue_GetTyper( self );
	return typer->core->Copy( self, proc, cycData );
}

static DArray* MakeIndex( DaoProcess *proc, DaoValue *index, daoint N, daoint *start, daoint *end, int *idtype )
{
	daoint i, n, n1, n2;
	DaoValue **items;
	DaoValue *first, *second;
	DArray *array = NULL;

	*idtype = IDX_NULL;
	*start = 0;
	*end = N - 1;
	if( index == NULL ) return NULL;

	switch( index->type ){
	case DAO_INTEGER :
		*idtype = IDX_SINGLE;
		n1 = index->xInteger.value;
		if( n1 <0 ) n1 += N;
		if( n1 <0 || n1 >= N ) *idtype = IDX_OUTOFRANGE;
		*start = n1;
		*end = n1;
		break;
	case DAO_FLOAT :
		*idtype = IDX_SINGLE;
		n1 = (daoint)(index->xFloat.value);
		if( n1 <0 ) n1 += N;
		if( n1 <0 || n1 >= N ) *idtype = IDX_OUTOFRANGE;
		*start = n1;
		*end = n1;
		break;
	case DAO_DOUBLE :
		*idtype = IDX_SINGLE;
		n1 = (daoint)(index->xDouble.value);
		if( n1 <0 ) n1 += N;
		if( n1 <0 || n1 >= N ) *idtype = IDX_OUTOFRANGE;
		*start = n1;
		*end = n1;
		break;
	case DAO_TUPLE:
		*idtype = IDX_PAIR;
		if( index->xTuple.unitype == dao_type_for_iterator ){
			DaoValue **data = index->xTuple.items;
			if( data[0]->type == data[1]->type && data[0]->type == DAO_INTEGER ){
				*idtype = IDX_SINGLE;
				*start = data[1]->xInteger.value;
				data[1]->xInteger.value += 1;
				data[0]->xInteger.value = data[1]->xInteger.value < N;
				break;
			}
		}
		first = index->xTuple.items[0];
		second = index->xTuple.items[1];
		/* a[ : 1 ] ==> pair(nil,int) */
		if( first->type > DAO_DOUBLE || second->type > DAO_DOUBLE ){
			*idtype = IDX_NONUMINDEX;
			break;
		}
		n1 = DaoValue_GetInteger( first );
		n2 = DaoValue_GetInteger( second );
		if( n1 <0 ) n1 += N;
		if( n2 <0 ) n2 += N;
		*start = n1;
		*end = n2;
		if( first->type ==DAO_NONE && second->type ==DAO_NONE ){
			*idtype = IDX_ALL;
		}else if( first->type ==DAO_NONE ){
			*idtype = IDX_TO;
		}else if( second->type ==DAO_NONE ){
			*idtype = IDX_FROM;
		}
		/* when specify an index range, allow out of range: (eg, str[:5]=='abcde') */
		if( n1 <0 || n1 >= N ) *idtype = IDX_OUTOFRANGE;
		if( n2 <0 || n2 >= N ) *idtype = IDX_OUTOFRANGE;
		break;
	case DAO_LIST:
		*idtype = IDX_MULTIPLE;
		items = index->xList.items.items.pValue;
		array = DArray_New(0);
		DArray_Resize( array, index->xList.items.size, 0 );
		for( i=0,n=array->size; i<n; i++ ){
			if( ! DaoValue_IsNumber( items[i] ) ){
				*idtype = IDX_NONUMINDEX;
				break;
			}
			n1 = DaoValue_GetInteger( items[i] );
			if( n1 <0 ) n1 += N;
			if( n1 <0 || n1 >= N ) *idtype = IDX_OUTOFRANGE;
			array->items.pInt[i] = n1;
		}
		if( *idtype != IDX_MULTIPLE ){
			DArray_Delete( array );
			break;
		}
		return array;
	default : break;
	}
	if( *idtype == IDX_NONUMINDEX ) DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "need number" );
	if( *idtype == IDX_OUTOFRANGE ) DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, "" );
	return NULL;
}

void DaoValue_Delete( void *self ){ dao_free( self ); }

DaoValue* DaoValue_NoCopy( DaoValue *self, DaoProcess *proc, DMap *cycData )
{
	return self;
}

DaoTypeCore baseCore =
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoValue_GetItem,
	DaoValue_SetItem,
	DaoValue_Print,
	DaoValue_NoCopy,
};
DaoTypeBase baseTyper =
{
	"none", & baseCore, NULL, NULL, {0}, {0}, DaoValue_Delete, NULL
};
static DaoNone none0 = {0,0,DAO_VALUE_CONST,0,1};
static DaoNone any0 = {0,DAO_ANY,DAO_VALUE_CONST,0,1};
DaoValue *dao_none_value = (DaoValue*) (void*) & none0;
DaoValue *dao_any_value = (DaoValue*) (void*) & any0;


extern DaoTypeBase numberTyper;
extern DaoTypeBase comTyper;
extern DaoTypeBase longTyper;
extern DaoTypeBase stringTyper;

DaoEnum* DaoEnum_New( DaoType *type, int value )
{
	DaoEnum *self = (DaoEnum*) dao_malloc( sizeof(DaoEnum) );
	DaoValue_Init( self, DAO_ENUM );
	self->value = value;
	self->etype = type;
	if( type ) GC_IncRC( type );
	return self;
}
DaoEnum* DaoEnum_Copy( DaoEnum *self, DaoType *type )
{
	DaoEnum *copy = DaoEnum_New( self->etype, self->value );
	if( self->etype != type && type ){
		DaoEnum_SetType( copy, type );
		DaoEnum_SetValue( copy, self, NULL );
	}
	return copy;
}
void DaoEnum_Delete( DaoEnum *self )
{
	if( self->etype ) GC_DecRC( self->etype );
	dao_free( self );
}
void DaoEnum_MakeName( DaoEnum *self, DString *name )
{
	DMap *mapNames;
	DNode *node;
	DString_Clear( name );
	mapNames = self->etype->mapNames;
	for(node=DMap_First(mapNames);node;node=DMap_Next(mapNames,node)){
		if( self->etype->flagtype ){
			if( !(node->value.pInt & self->value) ) continue;
		}else if( node->value.pInt != self->value ){
			continue;
		}
		DString_AppendChar( name, '$' );
		DString_Append( name, node->key.pString );
	}
}
void DaoEnum_SetType( DaoEnum *self, DaoType *type )
{
	if( self->etype == type ) return;
	GC_ShiftRC( type, self->etype );
	self->etype = type;
	self->value = type->mapNames->root->value.pInt;
}
int DaoEnum_SetSymbols( DaoEnum *self, const char *symbols )
{
	DString *names;
	daoint first = 0;
	daoint value = 0;
	int notfound = 0;
	int i, n, k = 0;
	if( self->etype->name->mbs[0] == '$' ) return 0;
	names = DString_New(1);
	DString_SetMBS( names, symbols );
	for(i=0,n=names->size; i<n; i++) if( names->mbs[i] == '$' ) names->mbs[i] = 0;
	i = 0;
	if( names->mbs[0] == '\0' ) i += 1;
	do{ /* for multiple symbols */
		DString name = DString_WrapMBS( names->mbs + i );
		DNode *node = DMap_Find( self->etype->mapNames, &name );
		if( node ){
			if( ! k ) first = node->value.pInt;
			value |= node->value.pInt;
			k += 1;
		}else{
			notfound = 1;
		}
		i += name.size + 1;
	}while( i < names->size );
	DString_Delete( names );
	if( k == 0 ) return 0;
	if( self->etype->flagtype ==0 && k > 1 ){
		self->value = first;
		return 0;
	}
	self->value = value;
	return notfound == 0;
}
int DaoEnum_SetValue( DaoEnum *self, DaoEnum *other, DString *enames )
{
	DMap *selfNames = self->etype->mapNames;
	DMap *otherNames = other->etype->mapNames;
	DNode *node, *search;

	if( self->etype == other->etype ){
		self->value = other->value;
		return 1;
	}
	if( self->etype->name->mbs[0] == '$' ) return 0;

	self->value = 0;
	if( self->etype->flagtype && other->etype->flagtype ){
		for(node=DMap_First(otherNames);node;node=DMap_Next(otherNames,node)){
			if( !(node->value.pInt & other->value) ) continue;
			search = DMap_Find( selfNames, node->key.pVoid );
			if( search == NULL ) return 0;
			self->value |= search->value.pInt;
		}
	}else if( self->etype->flagtype ){
		for(node=DMap_First(otherNames);node;node=DMap_Next(otherNames,node)){
			if( node->value.pInt != other->value ) continue;
			search = DMap_Find( selfNames, node->key.pVoid );
			if( search == NULL ) return 0;
			self->value |= search->value.pInt;
		}
	}else if( other->etype->flagtype ){
		for(node=DMap_First(otherNames);node;node=DMap_Next(otherNames,node)){
			if( !(node->value.pInt & other->value) ) continue;
			search = DMap_Find( selfNames, node->key.pVoid );
			if( search == NULL ) return 0;
			self->value = search->value.pInt;
			break;
		}
		return node && (node->value.pInt == other->value);
	}else{
		for(node=DMap_First(otherNames);node;node=DMap_Next(otherNames,node)){
			if( node->value.pInt != other->value ) continue;
			search = DMap_Find( selfNames, node->key.pVoid );
			if( search == NULL ) return 0;
			self->value = search->value.pInt;
			break;
		}
	}
	return other->etype->name->mbs[0] == '$';
}
int DaoEnum_AddValue( DaoEnum *self, DaoEnum *other, DString *enames )
{
	DMap *selfNames = self->etype->mapNames;
	DMap *otherNames = other->etype->mapNames;
	DNode *node, *search;

	if( self->etype->flagtype ==0 || self->etype->name->mbs[0] == '$' ) return 0;

	if( self->etype == other->etype ){
		self->value |= other->value;
		return 1;
	}else if( other->etype->flagtype ){
		for(node=DMap_First(otherNames);node;node=DMap_Next(otherNames,node)){
			if( !(node->value.pInt & other->value) ) continue;
			search = DMap_Find( selfNames, node->key.pVoid );
			if( search == NULL ) return 0;
			self->value |= search->value.pInt;
		}
	}else{
		for(node=DMap_First(otherNames);node;node=DMap_Next(otherNames,node)){
			if( node->value.pInt != other->value ) continue;
			search = DMap_Find( selfNames, node->key.pVoid );
			if( search == NULL ) return 0;
			self->value |= search->value.pInt;
		}
	}
	return other->etype->name->mbs[0] == '$';
}
int DaoEnum_RemoveValue( DaoEnum *self, DaoEnum *other, DString *enames )
{
	DMap *selfNames = self->etype->mapNames;
	DMap *otherNames = other->etype->mapNames;
	DNode *node, *search;

	if( self->etype->flagtype ==0 || self->etype->name->mbs[0] == '$' ) return 0;

	if( self->etype == other->etype ){
		self->value &= ~ other->value;
		return 1;
	}else if( other->etype->flagtype ){
		for(node=DMap_First(otherNames);node;node=DMap_Next(otherNames,node)){
			if( !(node->value.pInt & other->value) ) continue;
			search = DMap_Find( selfNames, node->key.pVoid );
			if( search == NULL ) return 0;
			self->value &= ~search->value.pInt;
		}
	}else{
		for(node=DMap_First(otherNames);node;node=DMap_Next(otherNames,node)){
			if( node->value.pInt != other->value ) continue;
			search = DMap_Find( selfNames, node->key.pVoid );
			if( search == NULL ) return 0;
			self->value &= ~search->value.pInt;
		}
	}
	return other->etype->name->mbs[0] == '$';
}
int DaoEnum_AddSymbol( DaoEnum *self, DaoEnum *s1, DaoEnum *s2, DaoNamespace *ns )
{
	DaoType *type;
	DMap *names1 = s1->etype->mapNames;
	DMap *names2 = s2->etype->mapNames;
	DMap *mapNames;
	DNode *node;
	DString *name;
	daoint value = 0;
	if( s1->etype->name->mbs[0] != '$' && s2->etype->name->mbs[0] != '$' ) return 0;
	name = DString_New(1);
	for(node=DMap_First(names1);node;node=DMap_Next(names1,node)){
		DString_AppendChar( name, '$' );
		DString_Append( name, node->key.pString );
	}
	for(node=DMap_First(names2);node;node=DMap_Next(names2,node)){
		if( DMap_Find( names1, node->key.pVoid ) ) continue;
		DString_AppendChar( name, '$' );
		DString_Append( name, node->key.pString );
	}
	type = DaoNamespace_FindType( ns, name );
	if( type == NULL ){
		type = DaoType_New( name->mbs, DAO_ENUM, NULL, NULL );
		type->flagtype = 1;
		type->mapNames = mapNames = DMap_Copy( names1 );
		value = s1->value;
		if( mapNames->size == 1 ){
			mapNames->root->value.pInt = 1;
			value = 1;
		}
		for(node=DMap_First(names2);node;node=DMap_Next(names2,node)){
			if( DMap_Find( names1, node->key.pVoid ) ) continue;
			value |= (1<<mapNames->size);
			MAP_Insert( mapNames, node->key.pVoid, 1<<mapNames->size );
		}
		DaoNamespace_AddType( ns, name, type );
	}
	DaoEnum_SetType( self, type );
	DString_Delete( name );
	self->value = value;
	return 1;
}
int DaoEnum_SubSymbol( DaoEnum *self, DaoEnum *s1, DaoEnum *s2, DaoNamespace *ns )
{
	DaoType *type;
	DMap *names1 = s1->etype->mapNames;
	DMap *names2 = s2->etype->mapNames;
	DMap *mapNames;
	DNode *node;
	DString *name;
	daoint value = 0;
	int count = 0;
	if( s1->etype->name->mbs[0] != '$' && s2->etype->name->mbs[0] != '$' ) return 0;
	name = DString_New(1);
	for(node=DMap_First(names1);node;node=DMap_Next(names1,node)){
		if( DMap_Find( names2, node->key.pVoid ) ) continue;
		DString_AppendChar( name, '$' );
		DString_Append( name, node->key.pString );
		count += 1;
	}
	if( count ==0 ){
		DString_Delete( name );
		return 0;
	}
	type = DaoNamespace_FindType( ns, name );
	if( type == NULL ){
		type = DaoType_New( name->mbs, DAO_ENUM, NULL, NULL );
		type->flagtype = count > 1 ? 1 : 0;
		type->mapNames = mapNames = DMap_New(D_STRING,0);
		value = type->flagtype;
		for(node=DMap_First(names1);node;node=DMap_Next(names1,node)){
			if( DMap_Find( names2, node->key.pVoid ) ) continue;
			value |= (1<<mapNames->size);
			MAP_Insert( mapNames, node->key.pVoid, 1<<mapNames->size );
		}
		DaoNamespace_AddType( ns, name, type );
	}
	DaoEnum_SetType( self, type );
	DString_Delete( name );
	self->value = value;
	return 1;
}

DaoTypeBase enumTyper=
{
	"enum", & baseCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoEnum_Delete, NULL
};

extern DaoTypeBase funcTyper;
DaoTypeBase* DaoValue_GetTyper( DaoValue *self )
{
	if( self == NULL ) return & baseTyper;
	switch( self->type ){
	case DAO_NONE : return & baseTyper;
	case DAO_INTEGER :
	case DAO_FLOAT   :
	case DAO_DOUBLE  : return & numberTyper;
	case DAO_COMPLEX : return & comTyper;
	case DAO_LONG    : return & longTyper;
	case DAO_ENUM    : return & enumTyper;
	case DAO_STRING  : return & stringTyper;
	case DAO_CTYPE   :
	case DAO_CDATA   : return self->xCdata.typer;
	default : break;
	}
	return DaoVmSpace_GetTyper( self->type );
}

void DaoValue_GetField( DaoValue *self, DaoProcess *proc, DString *name )
{
	DaoType *type = DaoNamespace_GetType( proc->activeNamespace, self );
	DaoValue *p = DaoType_FindValue( type, name );
	if( p == NULL ){
		DString *mbs = DString_New(1);
		DString_Append( mbs, name );
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, DString_GetMBS( mbs ) );
		DString_Delete( mbs );
		return;
	}
	DaoProcess_PutValue( proc, p );
}
void DaoValue_SetField( DaoValue *self, DaoProcess *proc, DString *name, DaoValue *value )
{
}
void DaoValue_SafeGetField( DaoValue *self, DaoProcess *proc, DString *name )
{
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	DaoValue_GetField( self, proc, name );
}
void DaoValue_SafeSetField( DaoValue *self, DaoProcess *proc, DString *name, DaoValue *value )
{
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	DaoValue_SetField( self, proc, name, value );
}
void DaoValue_GetItem( DaoValue *self, DaoProcess *proc, DaoValue *pid[], int N )
{
	DaoType *type = DaoNamespace_GetType( proc->activeNamespace, self );
	DaoRoutine *func = DaoType_FindFunctionMBS( type, "[]" );
	if( func == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, "[]" );
		return;
	}
	DaoProcess_PushCallable( proc, func, self, pid, N );
}
void DaoValue_SetItem( DaoValue *self, DaoProcess *proc, DaoValue *pid[], int N, DaoValue *value )
{
	DaoType *type = DaoNamespace_GetType( proc->activeNamespace, self );
	DaoRoutine *func = DaoType_FindFunctionMBS( type, "[]=" );
	DaoValue *p[ DAO_MAX_PARAM ];
	memcpy( p, pid, N*sizeof(DaoValue*) );
	p[N+1] = value;
	if( func == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, "[]=" );
		return;
	}
	DaoProcess_PushCallable( proc, func, self, p, N+1 );
}

/**/
static void DaoNumber_Print( DaoValue *self, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	switch( self->type ){
	case DAO_INTEGER : DaoStream_WriteInt( stream, self->xInteger.value ); break;
	case DAO_FLOAT   : DaoStream_WriteFloat( stream, self->xFloat.value ); break;
	case DAO_DOUBLE  : DaoStream_WriteFloat( stream, self->xDouble.value ); break;
	}
}
static void DaoNumber_GetItem1( DaoValue *self, DaoProcess *proc, DaoValue *pid )
{
	size_t bits = (size_t) DaoValue_GetDouble( self );
	daoint size = 8*sizeof(daoint);
	daoint start, end;
	int idtype;
	DArray *ids = MakeIndex( proc, pid, size, & start, & end, & idtype );
	daoint *res = DaoProcess_PutInteger( proc, 0 );
	switch( idtype ){
	case IDX_NULL : *res = bits; break;
	case IDX_SINGLE : *res = ( bits >> start ) & 0x1; break;
	case IDX_MULTIPLE : DArray_Delete( ids ); /* fall through */
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}
static void DaoNumber_SetItem1( DaoValue *self, DaoProcess *proc, DaoValue *pid, DaoValue *value )
{
	size_t bits = (size_t) DaoValue_GetDouble( self );
	size_t val = (size_t) DaoValue_GetDouble( value );
	daoint size = 8*sizeof(daoint);
	daoint start, end;
	int idtype;
	DArray *ids = MakeIndex( proc, pid, size, & start, & end, & idtype );
	switch( idtype ){
	case IDX_NULL : bits = val; break;
	case IDX_SINGLE : bits |= ( ( val != 0 ) & 0x1 ) << start; break;
	case IDX_MULTIPLE : DArray_Delete( ids ); /* fall through */
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
	self->xInteger.value = bits;
}
static void DaoNumber_GetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N )
{
	switch( N ){
	case 0 : DaoNumber_GetItem1( self, proc, NULL ); break;
	case 1 : DaoNumber_GetItem1( self, proc, ids[0] ); break;
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}
static void DaoNumber_SetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	switch( N ){
	case 0 : DaoNumber_SetItem1( self, proc, NULL, value ); break;
	case 1 : DaoNumber_SetItem1( self, proc, ids[0], value ); break;
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}

static DaoTypeCore numberCore=
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoNumber_GetItem,
	DaoNumber_SetItem,
	DaoNumber_Print,
	DaoValue_NoCopy,
};

DaoTypeBase numberTyper=
{
	"double", & numberCore, NULL, NULL, {0}, {0}, DaoValue_Delete, NULL
};

/**/
static void DaoString_Print( DaoValue *self, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	if( stream->useQuote ) DaoStream_WriteChar( stream, '\"' );
	DaoStream_WriteString( stream, self->xString.data );
	if( stream->useQuote ) DaoStream_WriteChar( stream, '\"' );
}
static void DaoString_GetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid )
{
	DString *self = self0->xString.data;
	daoint size = DString_Size( self );
	daoint i, n, start, end;
	int idtype;
	DArray *ids = MakeIndex( proc, pid, size, & start, & end, & idtype );
	DString *res = NULL;
	if( idtype != IDX_SINGLE ) res = DaoProcess_PutMBString( proc, "" );
	switch( idtype ){
	case IDX_NULL :
		DString_Assign( res, self );
		break;
	case IDX_SINGLE :
		{
			daoint *num = DaoProcess_PutInteger( proc, 0 );
			*num = self->mbs ? self->mbs[start] : self->wcs[start];
			break;
		}
	case IDX_FROM :
		DString_SubString( self, res, start, -1 );
		break;
	case IDX_TO :
		DString_SubString( self, res, 0, end+1 );
		break;
	case IDX_PAIR :
		DString_SubString( self, res, start, end-start+1 );
		break;
	case IDX_ALL :
		DString_SubString( self, res, 0, -1 );
		break;
	case IDX_MULTIPLE :
		{
			daoint *ip = ids->items.pInt;
			res = DaoProcess_PutMBString( proc, "" );
			DString_Clear( res );
			if( self->mbs ){
				char *data = self->mbs;
				for(i=0,n=ids->size; i<n; i++) DString_AppendChar( res, data[ ip[i] ] );
			}else{
				wchar_t *data = self->wcs;
				for(i=0,n=ids->size; i<n; i++) DString_AppendWChar( res, data[ ip[i] ] );
			}
			DArray_Delete( ids );
		}
		break;
	default : break;
	}
}
static void DaoString_SetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid, DaoValue *value )
{
	DString *self = self0->xString.data;
	daoint size = DString_Size( self );
	daoint start, end;
	int idtype;
	DArray *ids = MakeIndex( proc, pid, size, & start, & end, & idtype );
	DString_Detach( self );
	if( value->type >= DAO_INTEGER && value->type <= DAO_DOUBLE ){
		daoint i, n, id = value->xInteger.value;
		if( idtype == IDX_MULTIPLE ){
			daoint *ip = ids->items.pInt;
			if( self->mbs ){
				for(i=0,n=ids->size; i<n; i++) self->mbs[ ip[i] ] = id;
			}else{
				for(i=0,n=ids->size; i<n; i++) self->wcs[ ip[i] ] = id;
			}
			DArray_Delete( ids );
			return;
		}
		if( self->mbs ){
			for(i=start; i<=end; i++) self->mbs[i] = id;
		}else{
			for(i=start; i<=end; i++) self->wcs[i] = id;
		}
	}else if( value->type == DAO_STRING ){
		DString *str = value->xString.data;
		switch( idtype ){
		case IDX_NULL :
			DString_Assign( self, str );
			break;
		case IDX_SINGLE :
			{
				int ch = str->mbs ? str->mbs[0] : str->wcs[0];
				if( self->mbs )
					self->mbs[start] = ch;
				else
					self->wcs[start] = ch;
				break;
			}
		case IDX_FROM :
			DString_Replace( self, str, start, -1 );
			break;
		case IDX_TO :
			DString_Replace( self, str, 0, end+1 );
			break;
		case IDX_PAIR :
			DString_Replace( self, str, start, end-start+1 );
			break;
		case IDX_ALL :
			DString_Assign( self, str );
			break;
		case IDX_MULTIPLE :
			DArray_Delete( ids );
			DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
		default : break;
		}
	}
}
static void DaoString_GetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N )
{
	switch( N ){
	case 0 : DaoString_GetItem1( self, proc, NULL ); break;
	case 1 : DaoString_GetItem1( self, proc, ids[0] ); break;
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}
static void DaoString_SetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	switch( N ){
	case 0 : DaoString_SetItem1( self, proc, NULL, value ); break;
	case 1 : DaoString_SetItem1( self, proc, ids[0], value ); break;
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}
static DaoTypeCore stringCore=
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoString_GetItem,
	DaoString_SetItem,
	DaoString_Print,
	DaoValue_NoCopy,
};

static void DaoSTR_Size( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutInteger( proc, p[0]->xString.data->size );
}
static daoint DaoSTR_CheckParam( DaoProcess *proc, daoint i )
{
	if( i < 0 ){
		char buffer[100];
		sprintf( buffer, "invalid parameter with value " DAO_INT_FORMAT "\n", i );
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, buffer );
	}
	return i;
}
static daoint DaoSTR_CheckIndex( DString *self, DaoProcess *proc, daoint index, int one_past_last )
{
	daoint id = index;
	if( id < 0 ) id = self->size + id;
	if( id < 0 || id > (self->size - 1 + one_past_last) ){
		char buffer[100];
		sprintf( buffer, "index out of range with value " DAO_INT_FORMAT "\n", index );
		DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, buffer );
		return -1;
	}
	return id;
}
static void DaoSTR_Resize( DaoProcess *proc, DaoValue *p[], int N )
{
	if( ( proc->vmSpace->options & DAO_EXEC_SAFE ) && p[1]->xInteger.value > 1E5 ){
		DaoProcess_RaiseException( proc, DAO_ERROR,
				"not permitted to create long string in safe running mode" );
		return;
	}
	if( DaoSTR_CheckParam( proc, p[1]->xInteger.value ) < 0 ) return;
	DString_Resize( p[0]->xString.data, p[1]->xInteger.value );
}

static void DaoSTR_Insert( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DString *str = p[1]->xString.data;
	daoint at = DaoSTR_CheckIndex( self, proc, p[2]->xInteger.value, 1 /* allow appending */ );
	daoint rm = DaoSTR_CheckParam( proc, p[3]->xInteger.value );
	daoint cp = DaoSTR_CheckParam( proc, p[4]->xInteger.value );
	if( (at < 0) | (rm < 0) | (cp < 0) ) return;
	DString_Insert( self, str, at, rm, cp );
}
static void DaoSTR_Clear( DaoProcess *proc, DaoValue *p[], int N )
{
	DString_Clear( p[0]->xString.data );
}
static void DaoSTR_Erase( DaoProcess *proc, DaoValue *p[], int N )
{
	daoint at = DaoSTR_CheckIndex( p[0]->xString.data, proc, p[1]->xInteger.value, 0 );
	daoint rm = DaoSTR_CheckParam( proc, p[2]->xInteger.value );
	if( (at < 0) | (rm < 0) ) return;
	DString_Erase( p[0]->xString.data, at, rm );
}
static void DaoSTR_Chop( DaoProcess *proc, DaoValue *p[], int N )
{
	daoint i, k;
	unsigned char *chs;
	DString *self = p[0]->xString.data;
	DString_Detach( self );
	DString_Chop( self );

	if( DString_CheckUTF8( self ) && self->mbs && self->size ){
		chs = (unsigned char*) self->mbs;
		i = self->size - 1;
		k = utf8_markers[ chs[i] ];
		if( k ==1 ){
			while( i && utf8_markers[ chs[i] ] ==1 ) i --;
			k = utf8_markers[ chs[i] ];
			if( k == 0 ){
				chs[k+1] = 0;
				self->size = k+1;
			}else if( (self->size - i) != k ){
				if( (self->size - i) < k ){
					chs[i] = 0;
					self->size = i;
				}else{
					chs[i+k] = 0;
					self->size = i + k;
				}
			}
		}else if( k !=0 ){
			chs[i] = 0;
			self->size --;
		}
	}
	DaoProcess_PutReference( proc, p[0] );
}
static void DaoSTR_Trim( DaoProcess *proc, DaoValue *p[], int N )
{
	DString_Trim( p[0]->xString.data );
	DaoProcess_PutReference( proc, p[0] );
}
static void DaoSTR_Find( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DString *str = p[1]->xString.data;
	daoint from = p[2]->xInteger.value;
	daoint pos = MAXSIZE;
	if( p[3]->xInteger.value ){
		pos = DString_RFind( self, str, from );
	}else{
		pos = DString_Find( self, str, from );
	}
	DaoProcess_PutInteger( proc, pos );
}
static void DaoSTR_Replace( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DString *str1 = p[1]->xString.data;
	DString *str2 = p[2]->xString.data;
	daoint index = p[3]->xInteger.value;
	daoint pos, from = 0, count = 0;
	if( self->mbs ){
		DString_ToMBS( str1 );
		DString_ToMBS( str2 );
	}else{
		DString_ToWCS( str1 );
		DString_ToWCS( str2 );
	}
	if( index == 0 ){
		pos = DString_Find( self, str1, from );
		while( pos != MAXSIZE ){
			count ++;
			DString_Insert( self, str2, pos, DString_Size( str1 ), 0 );
			from = pos + DString_Size( str2 );
			pos = DString_Find( self, str1, from );
		}
	}else if( index > 0){
		pos = DString_Find( self, str1, from );
		while( pos != MAXSIZE ){
			count ++;
			if( count == index ){
				DString_Insert( self, str2, pos, DString_Size( str1 ), 0 );
				break;
			}
			from = pos + DString_Size( str1 );
			pos = DString_Find( self, str1, from );
		}
		count = 1;
	}else{
		from = MAXSIZE;
		pos = DString_RFind( self, str1, from );
		while( pos != MAXSIZE ){
			count --;
			if( count == index ){
				DString_Insert( self, str2, pos-DString_Size( str1 )+1, DString_Size( str1 ), 0 );
				break;
			}
			from = pos - DString_Size( str1 );
			pos = DString_RFind( self, str1, from );
		}
		count = 1;
	}
	DaoProcess_PutInteger( proc, count );
}
static void DaoSTR_Replace2( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *res, *key;
	DString *self = p[0]->xString.data;
	DMap *par = p[1]->xMap.items;
	DMap *words = DMap_New(D_STRING,D_STRING);
	DMap *sizemap = DMap_New(0,0);
	DNode *node = DMap_First( par );
	DArray *sizes = DArray_New(0);
	daoint max = p[2]->xInteger.value;
	daoint i, j, k, n = self->size;
	for( ; node != NULL; node = DMap_Next(par, node) )
		DMap_Insert( words, node->key.pValue->xString.data, node->value.pValue->xString.data );
	if( self->mbs ){
		key = DString_New(1);
		res = DString_New(1);
		for(node=DMap_First(words); node !=NULL; node = DMap_Next(words, node) ){
			DString_ToMBS( node->key.pString );
			DString_ToMBS( node->value.pString );
			MAP_Insert( sizemap, node->key.pString->size, 0 );
		}
		for(node=DMap_First(sizemap); node !=NULL; node = DMap_Next(sizemap, node) )
			DArray_Append( sizes, node->key.pInt );
		i = 0;
		while( i < n ){
			DString *val = NULL;
			for(j=0; j<sizes->size; j++){
				k = sizes->items.pInt[j];
				if( i+k > n ) break;
				DString_SubString( self, key, i, k );
				node = DMap_FindGE( words, key );
				if( node == NULL ) break;
				if( DString_EQ( node->key.pString, key ) ){
					val = node->value.pString;
					if( max ==0 ) break;
				}
			}
			if( val ){
				DString_Append( res, val );
				i += key->size;
			}else{
				DString_AppendChar( res, self->mbs[i] );
				i ++;
			}
		}
	}else{
		key = DString_New(0);
		res = DString_New(0);
		for(node=DMap_First(words); node !=NULL; node = DMap_Next(words, node) ){
			DString_ToWCS( node->key.pString );
			DString_ToWCS( node->value.pString );
			MAP_Insert( sizemap, node->key.pString->size, 0 );
		}
		for(node=DMap_First(sizemap); node !=NULL; node = DMap_Next(sizemap, node) )
			DArray_Append( sizes, node->key.pInt );
		i = 0;
		while( i < n ){
			DString *val = NULL;
			for(j=0; j<sizes->size; j++){
				k = sizes->items.pInt[j];
				if( i+k > n ) break;
				DString_SubString( self, key, i, k );
				node = DMap_FindGE( words, key );
				if( node == NULL ) break;
				if( DString_EQ( node->key.pString, key ) ){
					val = node->value.pString;
					if( max ==0 ) break;
				}
			}
			if( val ){
				DString_Append( res, val );
				i += key->size;
			}else{
				DString_AppendWChar( res, self->wcs[i] );
				i ++;
			}
		}
	}
	DString_Assign( self, res );
	DString_Delete( key );
	DString_Delete( res );
	DArray_Delete( sizes );
	DMap_Delete( words );
	DMap_Delete( sizemap );
}
static void DaoSTR_Expand( DaoProcess *proc, DaoValue *p[], int N )
{
	DMap    *keys = NULL;
	DaoString *key = NULL;
	DaoTuple *tup = & p[1]->xTuple;
	DString *self = p[0]->xString.data;
	DString *spec = p[2]->xString.data;
	DString *res = NULL, *val = NULL, *sub = NULL;
	DNode *node = NULL;
	daoint keep = p[3]->xInteger.value;
	daoint i, pos1, pos2, prev = 0;
	wchar_t spec1;
	char spec2;
	int replace;
	int ch;
	if( DString_Size( spec ) ==0 ){
		DaoProcess_PutString( proc, self );
		return;
	}
	if(  p[1]->type == DAO_TUPLE ){
		if( tup->unitype ){
			keys = tup->unitype->mapNames;
		}else{
			DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "invalid tuple" );
			return;
		}
	}else{
		tup = NULL;
		keys = p[1]->xMap.items;
	}
	if( self->mbs && spec->wcs ) DString_ToMBS( spec );
	if( self->wcs && spec->mbs ) DString_ToWCS( spec );
	if( self->mbs ){
		res = DaoProcess_PutMBString( proc, "" );
		key = DaoString_New(1);
		sub = DString_New(1);
		spec2 = spec->mbs[0];
		pos1 = DString_FindChar( self, spec2, prev );
		while( pos1 != MAXSIZE ){
			pos2 = DString_FindChar( self, ')', pos1 );
			replace = 0;
			if( pos2 != MAXSIZE && self->mbs[pos1+1] == '(' ){
				replace = 1;
				for(i=pos1+2; i<pos2; i++){
					ch = self->mbs[i];
					if( ch != '_' && ! isalnum( ch ) ){
						replace = 0;
						break;
					}
				}
				if( replace ){
					DString_SubString( self, key->data, pos1+2, pos2-pos1-2 );
					if( tup ){
						node = DMap_Find( keys, key->data );
					}else{
						node = DMap_Find( keys, key );
					}
					if( node ){
						if( tup ){
							i = node->value.pInt;
							DaoValue_GetString( tup->items[i], key->data );
							val = key->data;
						}else{
							val = node->value.pValue->xString.data;
						}
					}else if( keep ){
						replace = 0;
					}else{
						DString_Clear( key->data );
						val = key->data;
					}
				}
			}
			DString_SubString( self, sub, prev, pos1 - prev );
			DString_Append( res, sub );
			prev = pos1 + 1;
			if( replace ){
				DString_Append( res, val );
				prev = pos2 + 1;
			}else{
				DString_AppendChar( res, spec2 );
			}
			pos1 = DString_FindChar( self, spec2, prev );
		}
	}else{
		res = DaoProcess_PutWCString( proc, L"" );
		key = DaoString_New(0);
		sub = DString_New(0);
		spec1 = spec->wcs[0];
		pos1 = DString_FindWChar( self, spec1, prev );
		while( pos1 != MAXSIZE ){
			pos2 = DString_FindWChar( self, L')', pos1 );
			replace = 0;
			if( pos2 != MAXSIZE && self->wcs[pos1+1] == L'(' ){
				replace = 1;
				for(i=pos1+2; i<pos2; i++){
					ch = self->wcs[i];
					if( ch != L'_' && ! isalnum( ch ) ){
						replace = 0;
						break;
					}
				}
				if( replace ){
					DString_SubString( self, key->data, pos1+2, pos2-pos1-2 );
					if( tup ){
						node = DMap_Find( keys, key->data );
					}else{
						node = DMap_Find( keys, key );
					}
					if( node ){
						if( tup ){
							i = node->value.pInt;
							DaoValue_GetString( tup->items[i], key->data );
							val = key->data;
						}else{
							val = node->value.pValue->xString.data;
						}
					}else if( keep ){
						replace = 0;
					}else{
						DString_Clear( key->data );
						val = key->data;
					}
				}
			}
			DString_SubString( self, sub, prev, pos1 - prev );
			DString_Append( res, sub );
			prev = pos1 + 1;
			if( replace ){
				DString_Append( res, val );
				prev = pos2 + 1;
			}else{
				DString_AppendWChar( res, spec1 );
			}
			pos1 = DString_FindWChar( self, spec1, prev );
		}
	}
	DString_SubString( self, sub, prev, DString_Size( self ) - prev );
	DString_Append( res, sub );
	DString_Delete( sub );
	DaoString_Delete( key );
}
static void DaoSTR_Split( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DString *delm = p[1]->xString.data;
	DString *quote = p[2]->xString.data;
	int rm = (int)p[3]->xInteger.value;
	DaoList *list = DaoProcess_PutList( proc );
	DaoValue *value = (DaoValue*) DaoString_New(1);
	DString *str = value->xString.data;
	daoint dlen = DString_Size( delm );
	daoint qlen = DString_Size( quote );
	daoint size = DString_Size( self );
	daoint last = 0;
	daoint posDelm = DString_Find( self, delm, last );
	daoint posQuote = DString_Find( self, quote, last );
	daoint posQuote2 = -1;
	if( N ==1 || DString_Size( delm ) ==0 ){
		daoint i = 0;
		if( self->mbs ){
			unsigned char *mbs = (unsigned char*) self->mbs;
			daoint j, k;
			while( i < size ){
				k = utf8_markers[ mbs[i] ];
				if( k ==0 || k ==7 ){
					DString_SetDataMBS( str, (char*)mbs + i, 1 );
					DArray_Append( & list->items, value );
					i ++;
				}else if( k ==1 ){
					k = i;
					while( i < size && utf8_markers[ mbs[i] ] ==1 ) i ++;
					DString_SetDataMBS( str, (char*)mbs + k, i-k );
					DArray_Append( & list->items, value );
				}else{
					for( j=1; j<k; j++ ){
						if( i + j >= size ) break;
						if( utf8_markers[ mbs[i+j] ] != 1 ) break;
					}
					DString_SetDataMBS( str, (char*)mbs + i, j );
					DArray_Append( & list->items, value );
					i += j;
				}
			}
		}else{
			wchar_t *wcs = self->wcs;
			DString_ToWCS( str );
			DString_Resize( str, 1 );
			for(i=0; i<size; i++){
				DString_Detach( str );
				str->wcs[0] = wcs[i];
				DArray_Append( & list->items, value );
			}
		}
		DaoString_Delete( (DaoString*) value );
		return;
	}
	if( posDelm != MAXSIZE && posQuote != MAXSIZE && posQuote < posDelm ){
		posQuote2 = DString_Find( self, quote, posQuote+qlen );
		if( posQuote2 != MAXSIZE && posQuote2 > posDelm )
			posDelm = DString_Find( self, delm, posQuote2 );
	}
	while( posDelm != MAXSIZE ){
		if( rm && posQuote == last && posQuote2 == posDelm-qlen )
			DString_SubString( self, str, last+qlen, posDelm-last-2*qlen );
		else
			DString_SubString( self, str, last, posDelm-last );
		/* if( last !=0 || posDelm !=0 ) */
		DArray_Append( & list->items, value );

		last = posDelm + dlen;
		posDelm = DString_Find( self, delm, last );
		posQuote = DString_Find( self, quote, last );
		posQuote2 = -1;
		if( posDelm != MAXSIZE && posQuote != MAXSIZE && posQuote < posDelm ){
			posQuote2 = DString_Find( self, quote, posQuote+qlen );
			if( posQuote2 != MAXSIZE && posQuote2 > posDelm )
				posDelm = DString_Find( self, delm, posQuote2 );
		}
	}
	if( posQuote != MAXSIZE && posQuote < size )
		posQuote2 = DString_Find( self, quote, posQuote+qlen );
	if( rm && posQuote == last && posQuote2 == size-qlen )
		DString_SubString( self, str, last+qlen, size-last-2*qlen );
	else
		DString_SubString( self, str, last, size-last );
	DArray_Append( & list->items, value );
	DaoString_Delete( (DaoString*) value );
}
static void DaoSTR_Tolower( DaoProcess *proc, DaoValue *p[], int N )
{
	DString_ToLower( p[0]->xString.data );
	DaoProcess_PutReference( proc, p[0] );
}
static void DaoSTR_Toupper( DaoProcess *proc, DaoValue *p[], int N )
{
	DString_ToUpper( p[0]->xString.data );
	DaoProcess_PutReference( proc, p[0] );
}
static void DaoSTR_PFind( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DString *pt = p[1]->xString.data;
	daoint start = p[3]->xInteger.value;
	daoint end = p[4]->xInteger.value;
	daoint index = p[2]->xInteger.value;
	daoint i, p1=start, p2=end;
	DaoTuple *tuple = NULL;
	DaoList *list = DaoProcess_PutList( proc );
	DaoType *itp = list->unitype->nested->items.pType[0];
	DaoRegex *patt = DaoProcess_MakeRegex( proc, pt, self->wcs ==NULL );
	/* no need to raise index out of range exception (consider empty self string): */
	if( start <0 ) start += self->size;
	if( end <0 ) end += self->size;
	p1 = start;
	p2 = end;
	if( (patt == NULL) | (start < 0) | (end < 0) ) return;
	if( end == 0 ) p2 = end = DString_Size( self );
	i = 0;
	while( DaoRegex_Match( patt, self, & p1, & p2 ) ){
		if( index ==0 || (++i) == index ){
			tuple = DaoTuple_New( 2 );
			GC_IncRC( itp );
			tuple->unitype = itp;
			tuple->items[0] = (DaoValue*) DaoInteger_New( p1 );
			tuple->items[1] = (DaoValue*) DaoInteger_New( p2 );
			GC_IncRC( tuple->items[0] );
			GC_IncRC( tuple->items[1] );
			DArray_Append( & list->items, tuple );
			if( index ) break;
		}
		p1 = p2 + 1;
		p2 = end;
	}
}
static void DaoSTR_Match0( DaoProcess *proc, DaoValue *p[], int N, int subm )
{
	DString *self = p[0]->xString.data;
	DString *pt = p[1]->xString.data;
	daoint start = p[2+subm]->xInteger.value;
	daoint end = p[3+subm]->xInteger.value;
	daoint p1=start, p2=end;
	int capt = p[4]->xInteger.value;
	int gid = p[2]->xInteger.value;
	DaoTuple *tuple = DaoProcess_PutTuple( proc );
	DaoRegex *patt = DaoProcess_MakeRegex( proc, pt, self->wcs ==NULL );
	DaoValue **items = tuple->items;
	if( start <0 ) start += self->size;
	if( end <0 ) end += self->size;
	p1 = start;
	p2 = end;
	if( (patt == NULL) | (start < 0) | (end < 0) ) return;
	if( end == 0 ) p2 = end = DString_Size( self );
	pt = DString_Copy( pt );
	DString_Clear( pt );
	if( DaoRegex_Match( patt, self, & p1, & p2 ) ){
		if( subm && DaoRegex_SubMatch( patt, gid, & p1, & p2 ) ==0 ) p1 = -1;
	}else{
		p1 = -1;
	}
	items[0]->xInteger.value = p1;
	items[1]->xInteger.value = p2;
	if( p1 != -1 && (subm || capt) ) DString_SubString( self, pt, p1, p2-p1+1 );
	DString_Assign( items[2]->xString.data, pt );
	DString_Delete( pt );
}
static void DaoSTR_Match( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoSTR_Match0( proc, p, N, 0 );
}
static void DaoSTR_SubMatch( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoSTR_Match0( proc, p, N, 1 );
}
static void DaoSTR_Extract( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DString *pt = p[1]->xString.data;
	DString *mask = p[3]->xTuple.items[0]->xString.data;
	int rev = p[3]->xTuple.items[1]->xInteger.value;
	int type = p[2]->xEnum.value;
	daoint i, from, to, step;
	daoint size = DString_Size( self );
	daoint end=size, p1=0, p2=size;
	DaoString *subs = DaoString_New( pt->mbs != NULL );
	DArray *masks = DArray_New(0);
	DArray *matchs = DArray_New(0);
	DaoList *list = DaoProcess_PutList( proc );
	DaoRegex *patt = DaoProcess_MakeRegex( proc, pt, self->wcs ==NULL );
	DaoRegex *ptmask = NULL;
	if( size == 0 ) goto DoNothing;
	if( DString_Size( mask ) ==0 ) mask = NULL;
	if( mask ){
		ptmask = DaoProcess_MakeRegex( proc, mask, self->wcs ==NULL );
		if( ptmask ==NULL ) goto DoNothing;
	}
	if( patt ==NULL ) goto DoNothing;
	if( mask == NULL || rev ) DArray_Append( masks, 0 );
	if( mask ){
		while( DaoRegex_Match( ptmask, self, & p1, & p2 ) ){
			DArray_Append( masks, p1 );
			DArray_Append( masks, p2 + 1 );
			p1 = p2 + 1;  p2 = size;
		}
	}
	if( mask == NULL || rev ) DArray_Append( masks, size );
	DArray_Append( matchs, 0 );
	for(i=0; i<masks->size; i+=2){
		p1 = masks->items.pInt[i];
		p2 = end = masks->items.pInt[i+1] - 1;
		while( DaoRegex_Match( patt, self, & p1, & p2 ) ){
			DArray_Append( matchs, p1 );
			DArray_Append( matchs, p2 + 1 );
			p1 = p2 + 1;  p2 = end;
		}
	}
	DArray_Append( matchs, size );
	step = 2;
	from = 0;
	to = matchs->size -1;
	switch( type ){
	case 0 : step = 1; break;
	case 1 : from = 1; break;
	case 2 : to = matchs->size; break;
	}
	for(i=from; i<to; i+=step){
		p1 = matchs->items.pInt[i];
		p2 = matchs->items.pInt[i+1];
		/*
		   printf( "p1 = %i, p2 = %i\n", p1, p2 );
		 */
		if( (p1 >0 && p1 <size) || p2 > p1 ){
			DString_SubString( self, subs->data, p1, p2-p1 );
			DArray_Append( & list->items, (DaoValue*) subs );
		}
	}
DoNothing:
	DaoString_Delete( subs );
	DArray_Delete( masks );
	DArray_Delete( matchs );
}
static void DaoSTR_Capture( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DString *pt = p[1]->xString.data;
	daoint start = p[2]->xInteger.value;
	daoint end = p[3]->xInteger.value;
	daoint p1=start, p2=end;
	int gid;
	DaoString *subs;
	DaoList *list = DaoProcess_PutList( proc );
	DaoRegex *patt = DaoProcess_MakeRegex( proc, pt, self->wcs ==NULL );
	if( start <0 ) start += self->size;
	if( end <0 ) end += self->size;
	p1 = start;
	p2 = end;
	if( (patt == NULL) | (start < 0) | (end < 0) ) return;
	if( end == 0 ) p2 = end = DString_Size( self );
	if( DaoRegex_Match( patt, self, & p1, & p2 ) ==0 ) return;
	subs = DaoString_New( pt->mbs != NULL );
	for( gid=0; gid<=patt->group; gid++ ){
		DString_Clear( subs->data );
		if( DaoRegex_SubMatch( patt, gid, & p1, & p2 ) ){
			DString_SubString( self, subs->data, p1, p2-p1+1 );
		}
		DArray_Append( & list->items, (DaoValue*) subs );
	}
	DaoString_Delete( subs );
}
static void DaoSTR_Change( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DString *pt = p[1]->xString.data;
	DString *str = p[2]->xString.data;
	DaoRegex *patt = DaoProcess_MakeRegex( proc, pt, self->wcs ==NULL );
	daoint start = p[4]->xInteger.value;
	daoint end = p[5]->xInteger.value;
	daoint index = p[3]->xInteger.value;
	daoint n, size = self->size;
	if( start <0 ) start += self->size;
	if( end <0 ) end += self->size;
	if( (patt == NULL) | (start < 0) | (end < 0) ) return;
	n = DaoRegex_ChangeExt( patt, self, str, index, & start, & end );
	DaoProcess_PutInteger( proc, n );
}
static void DaoSTR_Mpack( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DString *pt = p[1]->xString.data;
	DString *str = p[2]->xString.data;
	daoint index = p[3]->xInteger.value;
	DaoRegex *patt = DaoProcess_MakeRegex( proc, pt, self->wcs ==NULL );
	if( N == 5 ){
		DaoList *res = DaoProcess_PutList( proc );
		daoint count = p[4]->xInteger.value;
		DaoRegex_MatchAndPack( patt, self, str, index, count, & res->items );
	}else{
		DArray *packs = DArray_New(D_VALUE);
		DaoRegex_MatchAndPack( patt, self, str, index, 1, packs );
		if( packs->size ){
			DaoProcess_PutValue( proc, packs->items.pValue[0] );
		}else{
			DaoProcess_PutMBString( proc, "" );
		}
		DArray_Delete( packs );
	}
}
static void DaoSTR_Iter( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DaoTuple *tuple = & p[1]->xTuple;
	DaoInteger *iter = DaoInteger_New( 0 );
	DaoValue **data = tuple->items;
	data[0]->xInteger.value = self->size >0;
	DaoValue_Copy( (DaoValue*) iter, & data[1] );
	dao_free( iter );
}

static void DaoSTR_Type( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutEnum( proc, ( p[0]->xString.data->mbs != NULL )? "mbs" : "wcs" );
}

static void DaoSTR_Convert( DaoProcess *proc, DaoValue *p[], int N )
{
	if( p[1]->xEnum.value == 0 )
		DString_ToMBS( p[0]->xString.data );
	else
		DString_ToWCS( p[0]->xString.data );
	DaoProcess_PutReference( proc, p[0] );
}
static void DaoSTR_Reverse( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DString_Reverse( self );
	DaoProcess_PutReference( proc, p[0] );
}
static void DaoSTR_Functional( DaoProcess *proc, DaoValue *p[], int np, int funct )
{
	daoint *count = NULL;
	DString *string = NULL;
	DaoList *list = NULL;
	DaoString *self = & p[0]->xString;
	DaoInteger chint = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *res, *index = (DaoValue*)(void*)&idint;
	DaoValue *chr = (DaoValue*)(void*)&chint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DString *data = self->data;
	daoint entry, i, n, N = data->size;
	wchar_t k;
	switch( funct ){
	case DVM_FUNCT_APPLY : 
		DString_Detach( self->data );
		DaoProcess_PutReference( proc, p[0] );
		break;
	case DVM_FUNCT_MAP : string = DaoProcess_PutValue( proc, p[0] )->xString.data; break;
	case DVM_FUNCT_SELECT : string = DaoProcess_PutMBString( proc, "" ); break;
	case DVM_FUNCT_INDEX : list = DaoProcess_PutList( proc ); break;
	case DVM_FUNCT_COUNT : count = DaoProcess_PutInteger( proc, 0 ); break;
	}
	if( sect == NULL ) return;
	if( DString_CheckUTF8( self->data ) ){
		data = DString_Copy( self->data );
		DString_ToWCS( data );
	}
	if( string ) DString_ToWCS( string );
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	DaoProcess_AcquireCV( proc );
	for(i=0; i<N; i++){
		idint.value = i;
		chint.value = data->mbs ? data->mbs[i] : data->wcs[i];
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, chr );
		if( sect->b >1 ) DaoProcess_SetValue( proc, sect->a+1, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_VMPROC_ABORTED ) break;
		res = proc->stackValues[0];
		switch( funct ){
		case DVM_FUNCT_MAP :
			string->wcs[i] = DaoValue_GetInteger( res );
			break;
		case DVM_FUNCT_SELECT :
			if( ! DaoValue_IsZero( res ) ){
				if( data->mbs ){
					DString_AppendChar( string, data->mbs[i] );
				}else{
					DString_AppendWChar( string, data->wcs[i] );
				}
			}
			break;
		case DVM_FUNCT_INDEX :
			if( ! DaoValue_IsZero( res ) ) DaoList_Append( list, index );
			break;
		case DVM_FUNCT_COUNT :
			*count += ! DaoValue_IsZero( res );
			break;
		case DVM_FUNCT_APPLY :
			k = DaoValue_GetInteger( res );
			if( data->mbs ){
				data->mbs[i] = k;
			}else{
				data->wcs[i] = k;
			}
			break;
		}
	}
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
	if( data->wcs && self->data->mbs ){
		DString *tmp = self->data;
		for(i=0,k=0,n=data->size; i<n; i++) if( data->wcs[i] > k ) k = data->wcs[i];
		if( k < 128 ) DString_ToMBS( data );
		self->data = data;
		data = tmp;
	}
	if( data != self->data ) DString_Delete( data );
	if( string == NULL ) return;
	for(i=0,k=0,n=string->size; i<n; i++) if( string->wcs[i] > k ) k = string->wcs[i];
	if( k < 128 ) DString_ToMBS( string );
}
static void DaoSTR_Iterate( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoSTR_Functional( proc, p, N, DVM_FUNCT_ITERATE );
}
static void DaoSTR_Count( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoSTR_Functional( proc, p, N, DVM_FUNCT_COUNT );
}
static void DaoSTR_Map( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoSTR_Functional( proc, p, N, DVM_FUNCT_MAP );
}
static void DaoSTR_Select( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoSTR_Functional( proc, p, N, DVM_FUNCT_SELECT );
}
static void DaoSTR_Index( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoSTR_Functional( proc, p, N, DVM_FUNCT_INDEX );
}
static void DaoSTR_Apply( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoSTR_Functional( proc, p, N, DVM_FUNCT_APPLY );
}

static DaoFuncItem stringMeths[] =
{
	{ DaoSTR_Size,    "size( self :string )=>int" },
	{ DaoSTR_Resize,  "resize( self :string, size :int )" },
	{ DaoSTR_Type,    "type( self :string )=>enum<mbs, wcs>" },
	{ DaoSTR_Convert, "convert( self :string, to :enum<mbs, wcs> ) =>string" },
	{ DaoSTR_Insert,  "insert( self :string, str :string, index=0, remove=0, copy=0 )" },
	{ DaoSTR_Clear,   "clear( self :string )" },
	{ DaoSTR_Erase,   "erase( self :string, start=0, n=-1 )" },
	{ DaoSTR_Chop,    "chop( self :string ) =>string" },
	{ DaoSTR_Trim,    "trim( self :string ) =>string" },
	/* return -1, if not found. */
	{ DaoSTR_Find,    "find( self :string, str :string, from=0, reverse=0 )=>int" },
	/* replace index-th occurrence: =0: replace all; >0: from begin; <0: from end. */
	/* return int of occurrence replaced. */
	{ DaoSTR_Replace, "replace( self :string, str1 :string, str2 :string, index=0 )=>int" },
	{ DaoSTR_Replace2, "replace( self :string, table : map<string,string>, max=0 )" },
	{ DaoSTR_Expand,  "expand( self :string, keys :map<string,string>, spec='$', keep=1 )=>string" },
	{ DaoSTR_Expand,  "expand( self :string, keys : tuple, spec='$', keep=1 )=>string" },
	{ DaoSTR_Split, "split( self :string, sep='', quote='', rm=1 )=>list<string>" },
	{ DaoSTR_PFind, "pfind( self :string, pt :string, index=0, start=0, end=0 )=>list<tuple<start:int,end:int>>" },
	{ DaoSTR_Match, "match( self :string, pt :string, start=0, end=0, substring=1 )=>tuple<start:int,end:int,substring:string>" },
	{ DaoSTR_SubMatch, "submatch( self :string, pt :string, group:int, start=0, end=0 )=>tuple<start:int,end:int,substring:string>" },
	{ DaoSTR_Extract, "extract( self :string, pt :string, mtype: enum<both,matched,unmatched>=$matched, mask :tuple<pattern:string,reversed:enum<false,true>> = ('', $false) )=>list<string>" },
	{ DaoSTR_Capture, "capture( self :string, pt :string, start=0, end=0 )=>list<string>" },
	{ DaoSTR_Change,  "change( self :string, pt :string, s:string, index=0, start=0, end=0 )=>int" },
	{ DaoSTR_Mpack,  "mpack( self :string, pt :string, s:string, index=0 )=>string" },
	{ DaoSTR_Mpack,  "mpack( self :string, pt :string, s:string, index : int, count : int )=>list<string>" },
	{ DaoSTR_Tolower, "tolower( self :string ) =>string" },
	{ DaoSTR_Toupper, "toupper( self :string ) =>string" },
	{ DaoSTR_Reverse, "reverse( self :string ) =>string" },
	{ DaoSTR_Iter, "__for_iterator__( self :string, iter : for_iterator )" },

	{ DaoSTR_Iterate,   "iterate( self :string )[char :int, index :int]" },
	{ DaoSTR_Count,  "count( self :string )[char :int, index :int =>int]=>int" },
	{ DaoSTR_Map,    "map( self :string )[char :int, index :int =>int]=>string" },
	{ DaoSTR_Select, "select( self :string )[char :int, index :int =>int]=>string" },
	{ DaoSTR_Index,  "index( self :string )[char :int, index :int =>int]=>list<int>" },
	{ DaoSTR_Apply,  "apply( self :string )[char :int, index :int =>int]=>string" },
	{ NULL, NULL }
};

DaoTypeBase stringTyper=
{
	"string", & stringCore, NULL, (DaoFuncItem*) stringMeths, {0}, {0}, 
	(FuncPtrDel) DaoString_Delete, NULL
};

/* also used for printing tuples */
static void DaoListCore_Print( DaoValue *self0, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	DaoList *self = & self0->xList;
	DaoValue **data = NULL;
	DNode *node = NULL;
	daoint i, size = 0;
	const char *lb = "{ ";
	const char *rb = " }";
	if( self->type == DAO_TUPLE ){
		data = self0->xTuple.items;
		size = self0->xTuple.size;
		lb = "( ";
		rb = " )";
	}else{
		data = self->items.items.pValue;
		size = self->items.size;
	}

	if( cycData ) node = MAP_Find( cycData, self );
	if( node ){
		DaoStream_WriteMBS( stream, lb );
		DaoStream_WriteMBS( stream, "..." );
		DaoStream_WriteMBS( stream, rb );
		return;
	}
	if( cycData ) MAP_Insert( cycData, self, self );
	DaoStream_WriteMBS( stream, lb );

	for( i=0; i<size; i++ ){
		stream->useQuote = 1;
		DaoValue_Print( data[i], proc, stream, cycData );
		stream->useQuote = 0;
		if( i != size-1 ) DaoStream_WriteMBS( stream, ", " );
	}
	DaoStream_WriteMBS( stream, rb );
	if( cycData ) MAP_Erase( cycData, self );
}
static void DaoListCore_GetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid )
{
	DaoList *res, *self = & self0->xList;
	daoint size = self->items.size;
	daoint e = proc->exceptions->size;
	daoint i, n, start, end;
	int idtype;
	DArray *ids = MakeIndex( proc, pid, size, & start, & end, & idtype );
	if( proc->exceptions->size > e ) return;

	switch( idtype ){
	case IDX_NULL :
		res = DaoList_Copy( self, NULL );
		DaoProcess_PutValue( proc, (DaoValue*) res );
		break;
	case IDX_SINGLE :
		DaoProcess_PutReference( proc, self->items.items.pValue[start] );
		break;
	case IDX_FROM :
		res = DaoProcess_PutList( proc );
		if( start >= self->items.size ) break;
		DArray_Resize( & res->items, self->items.size - start, NULL );
		for(i=start,n=self->items.size; i<n; i++)
			DaoList_SetItem( res, self->items.items.pValue[i], i-start );
		break;
	case IDX_TO :
		res = DaoProcess_PutList( proc );
		DArray_Resize( & res->items, end +1, NULL );
		for(i=0; i<=end; i++) DaoList_SetItem( res, self->items.items.pValue[i], i );
		break;
	case IDX_PAIR :
		res = DaoProcess_PutList( proc );
		DArray_Resize( & res->items, end - start + 1, NULL );
		for(i=start; i<=end; i++) DaoList_SetItem( res, self->items.items.pValue[i], i-start );
		break;
	case IDX_ALL :
		res = DaoList_Copy( self, NULL );
		DaoProcess_PutValue( proc, (DaoValue*) res );
		break;
	case IDX_MULTIPLE :
		res = DaoProcess_PutList( proc );
		DArray_Resize( & res->items, ids->size, NULL );
		for(i=0,n=ids->size; i<n; i++ )
			DaoList_SetItem( res, self->items.items.pValue[ ids->items.pInt[i] ], i );
		DArray_Delete( ids );
		break;
	default : break;
	}
}
static void DaoListCore_SetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid, DaoValue *value )
{
	DaoList *self = & self0->xList;
	daoint size = self->items.size;
	daoint i, n, start, end;
	int idtype, rc = 0;
	DArray *ids = MakeIndex( proc, pid, size, & start, & end, & idtype );
	if( self->unitype == NULL ){
		/* a : tuple<string,list<int>> = ('',{});
		   duplicating the constant to assign to a may not set the unitype properly */
		self->unitype = proc->activeTypes[ proc->activeCode->c ];
		GC_IncRC( self->unitype );
	}
	switch( idtype ){
	case IDX_NULL :
		for( i=0; i<size; i++ ) rc |= DaoList_SetItem( self, value, i );
		break;
	case IDX_SINGLE :
		DaoList_SetItem( self, value, start );
		break;
	case IDX_FROM :
		for( i=start,n=self->items.size; i<n; i++ ) rc |= DaoList_SetItem( self, value, i );
		break;
	case IDX_TO :
		for( i=0; i<=end; i++ ) rc |= DaoList_SetItem( self, value, i );
		break;
	case IDX_PAIR :
		for( i=start; i<=end; i++ ) rc |= DaoList_SetItem( self, value, i );
		break;
	case IDX_ALL :
		for( i=0,n=self->items.size; i<n; i++ ) rc |= DaoList_SetItem( self, value, i );
		break;
	case IDX_MULTIPLE :
		DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
		DArray_Delete( ids );
		break;
	default : break;
	}
	if( rc ) DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "value type" );
}
static void DaoListCore_GetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N )
{
	switch( N ){
	case 0 : DaoListCore_GetItem1( self, proc, NULL ); break;
	case 1 : DaoListCore_GetItem1( self, proc, ids[0] ); break;
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}
static void DaoListCore_SetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	switch( N ){
	case 0 : DaoListCore_SetItem1( self, proc, NULL, value ); break;
	case 1 : DaoListCore_SetItem1( self, proc, ids[0], value ); break;
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}
void DaoCopyValues( DaoValue **copy, DaoValue **data, int N, DaoProcess *proc, DMap *cycData )
{
	int i;
	for(i=0; i<N; i++){
		if( data[i]->type <= DAO_ENUM ){
			DaoValue_Move( data[i], copy+i, NULL );
		}else{
			DaoValue *src = data[i];
			if( cycData ){
				/* deep copy */
				DaoTypeBase *typer = DaoValue_GetTyper( data[i] );
				src = typer->core->Copy( data[i], proc, cycData );
			}
			GC_ShiftRC( src, copy[i] );
			copy[i] = src;
		}
	}
}
static DaoValue* DaoListCore_Copy( DaoValue *self0, DaoProcess *proc, DMap *cycData )
{
	DaoList *copy, *self = & self0->xList;
	DaoValue **data = self->items.items.pValue;

	if( cycData ){
		DNode *node = MAP_Find( cycData, self );
		if( node ) return node->value.pValue;
	}

	copy = DaoList_New();
	copy->unitype = self->unitype;
	GC_IncRC( copy->unitype );
	if( cycData ) MAP_Insert( cycData, self, copy );

	DArray_Resize( & copy->items, self->items.size, NULL );
	DaoCopyValues( copy->items.items.pValue, data, self->items.size, proc, cycData );
	return (DaoValue*) copy;
}
DaoList* DaoList_Copy( DaoList *self, DMap *cycData )
{
	return (DaoList*) DaoListCore_Copy( (DaoValue*)self, NULL, cycData );
}
static DaoTypeCore listCore=
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoListCore_GetItem,
	DaoListCore_SetItem,
	DaoListCore_Print,
	DaoListCore_Copy,
};

static daoint DaoList_MakeIndex( DaoList *self, daoint index, int one_past_last )
{
	if( index < 0 ) index += self->items.size;
	if( (index < 0) | (index > (self->items.size - 1 + one_past_last)) ) return -1;
	return index;
}
static void DaoLIST_Insert( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	daoint size = self->items.size;
	daoint pos = DaoList_MakeIndex( self, p[2]->xInteger.value, 1 );
	if( pos == -1 ){
		char buffer[100];
		sprintf( buffer, "with value " DAO_INT_FORMAT "\n", p[2]->xInteger.value );
		DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, buffer );
		return;
	}
	DaoList_Insert( self, p[1], pos );
	if( size == self->items.size )
		DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "value type" );
}
static void DaoLIST_Erase( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	daoint start = p[1]->xInteger.value;
	daoint n = p[2]->xInteger.value;
	DArray_Erase( & self->items, start, n );
}
static void DaoLIST_Clear( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	DaoList_Clear( self );
}
static void DaoLIST_Size( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	DaoProcess_PutInteger( proc, self->items.size );
}
static void DaoLIST_Resize( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	DaoValue *fill = dao_none_value;
	daoint size = p[1]->xInteger.value;
	if( ( proc->vmSpace->options & DAO_EXEC_SAFE ) && size > 1000 ){
		DaoProcess_RaiseException( proc, DAO_ERROR,
				"not permitted to create large list in safe running mode" );
		return;
	}
	if( self->unitype && self->unitype->value ) fill = self->unitype->value;
	DArray_Resize( & self->items, size, fill );
}
static int DaoList_CheckType( DaoList *self, DaoProcess *proc )
{
	daoint i, type;
	DaoValue **data = self->items.items.pValue;
	if( self->items.size == 0 ) return 0;
	type = data[0]->type;
	for(i=1; i<self->items.size; i++){
		if( type != data[i]->type ){
			DaoProcess_RaiseException( proc, DAO_WARNING, "need list of same type of elements" );
			return 0;
		}
	}
	if( type < DAO_INTEGER || type >= DAO_ARRAY ){
		DaoProcess_RaiseException( proc, DAO_WARNING, "need list of primitive data" );
		return 0;
	}
	return type;
}
static void DaoLIST_Max( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoTuple *tuple = DaoProcess_PutTuple( proc );
	DaoList *self = & p[0]->xList;
	DaoValue *res, **data = self->items.items.pValue;
	daoint i, imax, type, size = self->items.size;

	tuple->items[1]->xInteger.value = -1;
	type = DaoList_CheckType( self, proc );
	if( type == 0 ){
		DaoTuple_SetItem( tuple, self->unitype->nested->items.pType[0]->value, 0 );
		return;
	}
	imax = 0;
	res = data[0];
	for(i=1; i<size; i++){
		if( DaoValue_Compare( res, data[i] ) <0 ){
			imax = i;
			res = data[i];
		}
	}
	tuple->items[1]->xInteger.value = imax;
	DaoTuple_SetItem( tuple, res, 0 );
}
static void DaoLIST_Min( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoTuple *tuple = DaoProcess_PutTuple( proc );
	DaoList *self = & p[0]->xList;
	DaoValue *res, **data = self->items.items.pValue;
	daoint i, imin, type, size = self->items.size;

	tuple->items[1]->xInteger.value = -1;
	type = DaoList_CheckType( self, proc );
	if( type == 0 ){
		DaoTuple_SetItem( tuple, self->unitype->nested->items.pType[0]->value, 0 );
		return;
	}
	imin = 0;
	res = data[0];
	for(i=1; i<size; i++){
		if( DaoValue_Compare( res, data[i] ) >0 ){
			imin = i;
			res = data[i];
		}
	}
	tuple->items[1]->xInteger.value = imin;
	DaoTuple_SetItem( tuple, res, 0 );
}
extern DLong* DaoProcess_GetLong( DaoProcess *self, DaoVmCode *vmc );
extern DaoEnum* DaoProcess_GetEnum( DaoProcess *self, DaoVmCode *vmc );
static void DaoLIST_Sum( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	daoint i, type, size = self->items.size;
	DaoValue **data = self->items.items.pValue;
	type = DaoList_CheckType( self, proc );
	if( type == 0 ){
		DaoProcess_PutValue( proc, self->unitype->nested->items.pType[0]->value );
		return;
	}
	switch( type ){
	case DAO_INTEGER :
		{
			daoint res = 0;
			for(i=0; i<size; i++) res += data[i]->xInteger.value;
			DaoProcess_PutInteger( proc, res );
			break;
		}
	case DAO_FLOAT :
		{
			float res = 0.0;
			for(i=0; i<size; i++) res += data[i]->xFloat.value;
			DaoProcess_PutFloat( proc, res );
			break;
		}
	case DAO_DOUBLE :
		{
			double res = 0.0;
			for(i=0; i<size; i++) res += data[i]->xDouble.value;
			DaoProcess_PutDouble( proc, res );
			break;
		}
	case DAO_COMPLEX :
		{
			complex16 res = { 0.0, 0.0 };
			for(i=0; i<self->items.size; i++) COM_IP_ADD( res, data[i]->xComplex.value );
			DaoProcess_PutComplex( proc, res );
			break;
		}
	case DAO_LONG :
		{
			DLong *dlong = DaoProcess_GetLong( proc, proc->activeCode );
			for(i=0; i<self->items.size; i++) DLong_Add( dlong, dlong, data[i]->xLong.value );
			break;
		}
	case DAO_ENUM :
		{
			/* XXX */
			DaoEnum *denum = DaoProcess_GetEnum( proc, proc->activeCode );
			for(i=0; i<self->items.size; i++) denum->value += data[i]->xEnum.value;
			break;
		}
	case DAO_STRING :
		{
			DString *m = DaoProcess_PutString( proc, data[0]->xString.data );
			for(i=1; i<size; i++) DString_Append( m, data[i]->xString.data );
			break;
		}
	default : break;
	}
}
static void DaoLIST_Push( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	daoint size = self->items.size;
	if ( p[2]->xEnum.value == 0 )
		DaoList_PushFront( self, p[1] );
	else
		DaoList_Append( self, p[1] );
	if( size == self->items.size )
		DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "value type" );
}
static void DaoLIST_Pop( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	if( self->items.size == 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "list is empty" );
		return;
	}
	if ( p[1]->xEnum.value == 0 ){
		DaoProcess_PutReference( proc, self->items.items.pValue[0] );
		DaoList_Erase( self, 0 );
	}else{
		DaoProcess_PutReference( proc, self->items.items.pValue[self->items.size -1] );
		DaoList_Erase( self, self->items.size -1 );
	}
}
static void DaoLIST_PushBack( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	daoint size = self->items.size;
	DaoList_Append( self, p[1] );
	if( size == self->items.size )
		DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "value type" );
}
static void DaoLIST_Front( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	if( self->items.size == 0 ){
		DaoProcess_PutValue( proc, dao_none_value );
		DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "list is empty" );
		return;
	}
	DaoProcess_PutReference( proc, self->items.items.pValue[0] );
}
static void DaoLIST_Top( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	if( self->items.size == 0 ){
		DaoProcess_PutValue( proc, dao_none_value );
		DaoProcess_RaiseException( proc, DAO_ERROR_VALUE, "list is empty" );
		return;
	}
	DaoProcess_PutReference( proc, self->items.items.pValue[ self->items.size -1 ] );
}
/* Quick Sort.
 * Adam Drozdek: Data Structures and Algorithms in C++, 2nd Edition.
 */
static int Compare( DaoProcess *proc, int entry, int reg0, int reg1, DaoValue *v0, DaoValue *v1 )
{
	DaoValue **locs = proc->activeValues;
	DaoValue_Copy( v0, locs + reg0 );
	DaoValue_Copy( v1, locs + reg1 );
	proc->topFrame->entry = entry;
	DaoProcess_Execute( proc );
	return DaoValue_GetInteger( proc->stackValues[0] );
}
static void PartialQuickSort( DaoProcess *proc, int entry, int r0, int r1,
		DaoValue **data, daoint first, daoint last, daoint part )
{
	daoint lower=first+1, upper=last;
	DaoValue *val;
	DaoValue *pivot;
	if( first >= last ) return;
	val = data[first];
	data[first] = data[ (first+last)/2 ];
	data[ (first+last)/2 ] = val;
	pivot = data[ first ];

	while( lower <= upper ){
		while( lower < last && Compare( proc, entry, r0, r1, data[lower], pivot ) ) lower ++;
		while( upper > first && Compare( proc, entry, r0, r1, pivot, data[upper] ) ) upper --;
		if( lower < upper ){
			val = data[lower];
			data[lower] = data[upper];
			data[upper] = val;
			upper --;
		}
		lower ++;
	}
	val = data[first];
	data[first] = data[upper];
	data[upper] = val;
	if( first+1 < upper ) PartialQuickSort( proc, entry, r0, r1, data, first, upper-1, part );
	if( upper >= part ) return;
	if( upper+1 < last ) PartialQuickSort( proc, entry, r0, r1, data, upper+1, last, part );
}
typedef struct IndexValue IndexValue;
struct IndexValue
{
	daoint     index;
	DaoValue  *value;
};
static void QuickSort( IndexValue *data, daoint first, daoint last, daoint part, int asc )
{
	daoint lower=first+1, upper=last;
	IndexValue val;
	DaoValue *pivot;
	if( first >= last ) return;
	val = data[first];
	data[first] = data[ (first+last)/2 ];
	data[ (first+last)/2 ] = val;
	pivot = data[ first ].value;

	while( lower <= upper ){
		if( asc ){
			while( lower < last && DaoValue_Compare( data[lower].value, pivot ) <0 ) lower ++;
			while( upper > first && DaoValue_Compare( pivot, data[upper].value ) <0 ) upper --;
		}else{
			while( lower < last && DaoValue_Compare( data[lower].value, pivot ) >0 ) lower ++;
			while( upper > first && DaoValue_Compare( pivot, data[upper].value ) >0 ) upper --;
		}
		if( lower < upper ){
			val = data[lower];
			data[lower] = data[upper];
			data[upper] = val;
			upper --;
		}
		lower ++;
	}
	val = data[first];
	data[first] = data[upper];
	data[upper] = val;
	if( first+1 < upper ) QuickSort( data, first, upper-1, part, asc );
	if( upper >= part ) return;
	if( upper+1 < last ) QuickSort( data, upper+1, last, part, asc );
}
static void DaoLIST_Rank( DaoProcess *proc, DaoValue *p[], int npar)
{
	DaoList *list = & p[0]->xList;
	DaoList *res = DaoProcess_PutList( proc );
	DaoValue **items = list->items.items.pValue;
	DaoValue **ids;
	IndexValue *data;
	daoint part = p[2]->xInteger.value;
	daoint i, N;

	N = list->items.size;
	DArray_Resize( & res->items, N, p[2] ); /* init to be integers */
	ids = res->items.items.pValue;
	for(i=0; i<N; i++) ids[i]->xInteger.value = i;
	if( N < 2 ) return;
	if( part ==0 ) part = N;
	data = (IndexValue*) dao_malloc( N * sizeof( IndexValue ) );
	for(i=0; i<N; i++){
		data[i].index = i;
		data[i].value = items[i];
	}
	QuickSort( data, 0, N-1, part, p[1]->xEnum.value == 0 );
	for(i=0; i<N; i++) ids[i]->xInteger.value = data[i].index;
	dao_free( data );
}
static void DaoLIST_Sort( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoList *list = & p[0]->xList;
	DaoValue **items = list->items.items.pValue;
	daoint part = p[1 + (p[1]->type== DAO_ENUM)]->xInteger.value;
	DaoStackFrame *frame;
	IndexValue *data;
	daoint i, N;

	DaoProcess_PutReference( proc, p[0] );
	N = list->items.size;
	if( N < 2 ) return;
	if( part ==0 ) part = N;

	frame = DaoProcess_PushSectionFrame( proc );
	if( frame && p[1]->type != DAO_ENUM ){
		int entry = proc->topFrame->entry;
		DaoVmCode *vmc = proc->topFrame->codes + entry - 1;
		if( vmc->b < 2 ){
			DaoProcess_RaiseException( proc, DAO_ERROR, "Two few code section parameters" );
			return;
		}
		PartialQuickSort( proc, entry, vmc->a, vmc->a + 1, items, 0, N-1, part );
		DaoProcess_PopFrame( proc );
		return;
	}

	data = (IndexValue*) dao_malloc( N * sizeof( IndexValue ) );
	for(i=0; i<N; i++){
		data[i].index = i;
		data[i].value = items[i];
	}
	QuickSort( data, 0, N-1, part, p[1]->xEnum.value == 0 );
	for(i=0; i<N; i++) items[i] = data[i].value;
	dao_free( data );
}
static void DaoLIST_Iter( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	DaoTuple *tuple = & p[1]->xTuple;
	DaoInteger *iter = DaoInteger_New( 0 );
	DaoValue **data = tuple->items;
	data[0]->xInteger.value = self->items.size >0;
	DaoValue_Copy( (DaoValue*) iter, & data[1] );
	dao_free( iter );
}
static void DaoLIST_Join( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *self = & p[0]->xList;
	DaoValue **data = self->items.items.pValue;
	DString *sep = p[1]->xString.data;
	DString *buf = DString_New( 1 );
	DString *res;
	daoint size = 0, i;
	int digits, mbs = 1;
	wchar_t ewcs[] = {0};
	for( i = 0; i < self->items.size; i++ ){
		switch( data[i]->type ){
		case DAO_STRING:
			if( data[i]->xString.data->mbs == NULL ) mbs = 0;
			size += data[i]->xString.data->size;
			break;
		case DAO_INTEGER:
			size += ( data[i]->xInteger.value < 0 ) ? 2 : 1;
			break;
		case DAO_FLOAT:
			size += ( data[i]->xFloat.value < 0 ) ? 2 : 1;
			break;
		case DAO_DOUBLE:
			size += ( data[i]->xDouble.value < 0 ) ? 2 : 1;
			break;
		case DAO_COMPLEX:
			size += ( data[i]->xComplex.value.real < 0 ) ? 5 : 4;
			break;
		case DAO_LONG:
			digits = self->items.items.pValue[i]->xLong.value->size;
			digits = digits > 1 ? (LONG_BITS * (digits - 1) + 1) : 1; /* bits */
			digits /= (int)(log( self->items.items.pValue[i]->xLong.value->base ) / log(2)); /* digits */
			size += digits + ((data[i]->xLong.value->sign < 0) ? 3 : 2); /* sign + suffix */
			break;
		case DAO_ENUM :
			size += 1;
			break;
		default:
			DaoProcess_RaiseException( proc, DAO_ERROR, "Incompatible list type (expected numeric or string)" );
			return;
		}
	}
	if( !mbs || ( sep->size != 0 && sep->mbs == NULL ) )
		res = DaoProcess_PutWCString( proc, ewcs );
	else
		res = DaoProcess_PutMBString( proc, "" );
	if( self->items.size != 0 ){
		DString_Reserve( res, size + ( self->items.size - 1 ) * sep->size );
		for( i = 0; i < self->items.size - 1; i++ ){
			DString_Append( res, DaoValue_GetString( self->items.items.pValue[i], buf ) );
			if( sep->size != 0 ) DString_Append( res, sep );
		}
		DString_Append( res, DaoValue_GetString( self->items.items.pValue[i], buf ) );
	}
	DString_Delete( buf );
}
static void DaoLIST_Reverse( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoList *list = & p[0]->xList;
	DaoValue **items = list->items.items.pValue;
	daoint i = 0, N = list->items.size;

	DaoProcess_PutReference( proc, p[0] );
	for(i=0; i<N/2; i++){
		DaoValue *tmp = items[N-i-1];
		items[N-i-1] = items[i];
		items[i] = tmp;
	}
}
static void DaoLIST_BasicFunctional( DaoProcess *proc, DaoValue *p[], int npar, int funct )
{
	daoint *count = NULL;
	int direction = funct == DVM_FUNCT_COUNT ? 0 : p[1]->xEnum.value;
	DaoList *list = & p[0]->xList;
	DaoList *list2 = NULL;
	DaoTuple *tuple = NULL;
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue **items = list->items.items.pValue;
	DaoValue *res, *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	daoint entry, i, j, N = list->items.size;
	switch( funct ){
	case DVM_FUNCT_MAP :
	case DVM_FUNCT_SELECT :
	case DVM_FUNCT_INDEX : list2 = DaoProcess_PutList( proc ); break;
	case DVM_FUNCT_COUNT : count = DaoProcess_PutInteger( proc, 0 ); break;
	case DVM_FUNCT_APPLY : DaoProcess_PutReference( proc, p[0] ); break;
	case DVM_FUNCT_FIND : DaoProcess_PutValue( proc, dao_none_value ); break;
	}
	if( sect == NULL ) return;
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	DaoProcess_AcquireCV( proc );
	for(j=0; j<N; j++){
		i = direction ? N-1-j : j;
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, items[i] );
		if( sect->b >1 ) DaoProcess_SetValue( proc, sect->a+1, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_VMPROC_ABORTED ) break;
		res = proc->stackValues[0];
		switch( funct ){
		case DVM_FUNCT_MAP : DaoList_Append( list2, res ); break;
		case DVM_FUNCT_SELECT : if( res->xInteger.value ) DaoList_Append( list2, items[i] ); break;
		case DVM_FUNCT_INDEX : if( res->xInteger.value ) DaoList_Append( list2, index ); break;
		case DVM_FUNCT_COUNT : *count += res->xInteger.value != 0; break;
		case DVM_FUNCT_APPLY : DaoList_SetItem( list, res, i ); break;
		}
		if( funct == DVM_FUNCT_FIND && res->xInteger.value ){
			DaoProcess_PopFrame( proc );
			DaoProcess_SetActiveFrame( proc, proc->topFrame );
			tuple = DaoProcess_PutTuple( proc );
			GC_ShiftRC( items[i], tuple->items[1] );
			tuple->items[1] = items[i];
			tuple->items[0]->xInteger.value = j;
			break;
		}
	}
	DaoProcess_ReleaseCV( proc );
	if( funct != DVM_FUNCT_FIND ) DaoProcess_PopFrame( proc );
}
static void DaoLIST_Map( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoLIST_BasicFunctional( proc, p, npar, DVM_FUNCT_MAP );
}
static void DaoLIST_Find( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoLIST_BasicFunctional( proc, p, npar, DVM_FUNCT_FIND );
}
static void DaoLIST_Select( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoLIST_BasicFunctional( proc, p, npar, DVM_FUNCT_SELECT );
}
static void DaoLIST_Index( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoLIST_BasicFunctional( proc, p, npar, DVM_FUNCT_INDEX );
}
static void DaoLIST_Count( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoLIST_BasicFunctional( proc, p, npar, DVM_FUNCT_COUNT );
}
static void DaoLIST_Iterate( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoLIST_BasicFunctional( proc, p, npar, DVM_FUNCT_ITERATE );
}
static void DaoLIST_Apply( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoLIST_BasicFunctional( proc, p, npar, DVM_FUNCT_APPLY );
}
static void DaoLIST_Reduce( DaoProcess *proc, DaoValue *p[], int npar, int which )
{
	DaoList *list = & p[0]->xList;
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue **items = list->items.items.pValue;
	DaoValue *res = NULL, *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	daoint entry, i, j, first = 0, D = 0, N = list->items.size;
	if( sect == NULL || list->items.size == 0 ) return; // TODO exception
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	if( which == 1 ){
		D = p[1]->xEnum.value;
		res = items[0];
		first = 1;
	}else{
		res= p[1];
		D = p[2]->xEnum.value;
	}
	DaoProcess_AcquireCV( proc );
	for(j=first; j<N; j++){
		i = D ? N-1-j : j;
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, items[i] );
		if( sect->b >1 ) DaoProcess_SetValue( proc, sect->a+1, res );
		if( sect->b >2 ) DaoProcess_SetValue( proc, sect->a+2, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_VMPROC_ABORTED ) break;
		res = proc->stackValues[0];
	}
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
	DaoProcess_SetActiveFrame( proc, proc->topFrame );
	DaoProcess_PutValue( proc, res );
}
static void DaoLIST_Reduce1( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoLIST_Reduce( proc, p, npar, 1 );
}
static void DaoLIST_Reduce2( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoLIST_Reduce( proc, p, npar, 2 );
}
static void DaoLIST_Erase2( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoList *list = & p[0]->xList;
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue **items = list->items.items.pValue;
	DaoValue *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	daoint *count = DaoProcess_PutInteger( proc, 0 );
	daoint entry, i, j, N = list->items.size;
	int mode = p[1]->xEnum.value;
	if( sect == NULL ) return;
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	DaoProcess_AcquireCV( proc );
	for(j=0; j<N; j++){
		i = mode == 2 ? N-1-j : j; /* mode = $last */
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, items[i] );
		if( sect->b >1 ) DaoProcess_SetValue( proc, sect->a+1, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_VMPROC_ABORTED ) break;
		if( proc->stackValues[0]->xInteger.value ){
			GC_DecRC( items[i] );
			items[i] = NULL; /* mark as deleted */
			if( mode ) break; /* mode != $all */
		}
	}
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
	for(i=0, j=0; i<list->items.size; i++){
		DaoValue *val = items[i];
		if( val ) items[j++] = val;
	}
	*count = list->items.size - j;
	list->items.size = j;
	DArray_Resize( & list->items, j, NULL ); /* to possibly reduce buffer size: */
}
static void DaoLIST_Map2( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoList *list = & p[0]->xList;
	DaoList *list2 = & p[1]->xList;
	DaoList *list3 = DaoProcess_PutList( proc );
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue **items = list->items.items.pValue;
	DaoValue **items2 = list2->items.items.pValue;
	DaoValue *index = (DaoValue*)(void*)&idint;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	daoint entry, i, j, N = list->items.size;
	int direction = p[2]->xEnum.value;
	if( sect == NULL ) return;
	if( N > list2->items.size ) N = list2->items.size;
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	DaoProcess_AcquireCV( proc );
	for(j=0; j<N; j++){
		i = direction ? N-1-j : j;
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, items[i] );
		if( sect->b >1 ) DaoProcess_SetValue( proc, sect->a+1, items2[i] );
		if( sect->b >2 ) DaoProcess_SetValue( proc, sect->a+2, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_VMPROC_ABORTED ) break;
		DaoList_Append( list3, proc->stackValues[0] );
	}
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
}
static DaoFuncItem listMeths[] =
{
	{ DaoLIST_Insert,   "insert( self :list<@T>, item : @T, pos=0 )" },
	{ DaoLIST_Clear,    "clear( self :list<any> )" },
	{ DaoLIST_Size,     "size( self :list<any> )=>int" },
	{ DaoLIST_Resize,   "resize( self :list<any>, size :int )" },
	{ DaoLIST_Max,      "max( self :list<@T<int|long|float|double|complex|string|enum>> )=>tuple<@T,int>" },
	{ DaoLIST_Min,      "min( self :list<@T<int|long|float|double|complex|string|enum>> )=>tuple<@T,int>" },
	{ DaoLIST_Sum,      "sum( self :list<@T<int|long|float|double|complex|string|enum>> )=>@T" },
	{ DaoLIST_Join,     "join( self :list<int|float|double|long|complex|string|enum>, separator='' )=>string" },
	{ DaoLIST_PushBack, "append( self :list<@T>, item :@T )" },
	{ DaoLIST_Push,     "push( self :list<@T>, item :@T, to :enum<front, back> = $back )" },
	{ DaoLIST_Pop,      "pop( self :list<@T>, from :enum<front, back> = $back ) => @T" },
	{ DaoLIST_Front,    "front( self :list<@T> )=>@T" },
	{ DaoLIST_Top,      "back( self :list<@T> )=>@T" },
	{ DaoLIST_Rank,     "rank( self :list<any>, order :enum<ascend, descend>=$ascend, k=0 )=>list<int>" },
	{ DaoLIST_Reverse,  "reverse( self :list<@T> )=>list<@T>" },
	{ DaoLIST_Iter,     "__for_iterator__( self :list<any>, iter : for_iterator )" },

	{ DaoLIST_Erase,    "erase( self :list<@T>, start=0, n=1 )" },
	{ DaoLIST_Erase2,   "erase( self :list<@T>, mode :enum<all,first,last> )[item:@T,index:int=>int]=>int" },
	{ DaoLIST_Map,      "map( self :list<@T>, direction :enum<forward,backward>=$forward )[item:@T,index:int=>@V]=>list<@V>" },
	{ DaoLIST_Map2,     "map( self :list<@T>, other :list<@S>, direction :enum<forward,backward>=$forward )[item:@T,item2:@S,index:int=>@V]=>list<@V>" },
	{ DaoLIST_Reduce1,  "reduce( self :list<@T>, direction :enum<forward,backward>=$forward )[item:@T,value:@T,index:int=>@T]=>@T" },
	{ DaoLIST_Reduce2,  "reduce( self :list<@T>, init :@V, direction :enum<forward,backward>=$forward )[item:@T,value:@V,index:int=>@V]=>@V" },
	{ DaoLIST_Select,   "select( self :list<@T>, direction :enum<forward,backward>=$forward )[item:@T,index:int=>int]=>list<@T>" },
	{ DaoLIST_Find,     "find( self :list<@T>, direction :enum<forward,backward>=$forward )[item:@T,index:int=>int]=>tuple<index:int,value:@T>|none" },
	{ DaoLIST_Index,    "index( self :list<@T>, direction :enum<forward,backward>=$forward )[item:@T,index:int=>int]=>list<int>" },
	{ DaoLIST_Count,    "count( self :list<@T> )[item:@T,index:int=>int]=>int" },
	{ DaoLIST_Iterate,  "iterate( self :list<@T>, direction :enum<forward,backward>=$forward )[item:@T,index:int]" },
	{ DaoLIST_Sort,     "sort( self :list<@T>, order :enum<ascend,descend>, k=0 )=>list<@T>" },
	{ DaoLIST_Sort,     "sort( self :list<@T>, k=0 )[X:@T,Y:@T=>int]=>list<@T>" },
	{ DaoLIST_Apply,    "apply( self :list<@T>, direction :enum<forward,backward>=$forward )[item:@T,index:int=>@T]=>list<@T>" },
	{ NULL, NULL }
};

int DaoList_Size( DaoList *self )
{
	return self->items.size;
}
DaoValue* DaoList_Front( DaoList *self )
{
	if( self->items.size == 0 ) return NULL;
	return self->items.items.pValue[0];
}
DaoValue* DaoList_Back( DaoList *self )
{
	if( self->items.size == 0 ) return NULL;
	return self->items.items.pValue[ self->items.size-1 ];
}
DaoValue* DaoList_GetItem( DaoList *self, daoint pos )
{
	if( (pos = DaoList_MakeIndex( self, pos, 0 )) == -1 ) return NULL;
	return self->items.items.pValue[pos];
}
DaoTuple* DaoList_ToTuple( DaoList *self, DaoTuple *proto )
{
	/* XXX */
	return NULL;
}
int DaoList_SetItem( DaoList *self, DaoValue *it, daoint pos )
{
	DaoValue **val;
	if( (pos = DaoList_MakeIndex( self, pos, 0 )) == -1 ) return 1;
	val = self->items.items.pValue + pos;
	if( self->unitype && self->unitype->nested->size ){
		return DaoValue_Move( it, val, self->unitype->nested->items.pType[0] ) == 0;
	}else{
		DaoValue_Copy( it, val );
	}
	return 0;
}

int DaoList_Insert( DaoList *self, DaoValue *item, daoint pos )
{
	DaoType *tp = self->unitype ? self->unitype->nested->items.pType[0] : NULL;
	DaoValue *temp = NULL;
	if( (pos = DaoList_MakeIndex( self, pos, 1 )) == -1 ) return 1;
	if( DaoValue_Move( item, & temp, tp ) ==0 ){
		GC_DecRC( temp );
		return 1;
	}
	DArray_Insert( & self->items, NULL, pos );
	self->items.items.pValue[ pos ] = temp;
	return 0;
}
int DaoList_PushFront( DaoList *self, DaoValue *item )
{
	DaoType *tp = self->unitype ? self->unitype->nested->items.pType[0] : NULL;
	DaoValue *temp = NULL;
	if( DaoValue_Move( item, & temp, tp ) ==0 ){
		GC_DecRC( temp );
		return 1;
	}
	DArray_PushFront( & self->items, NULL );
	self->items.items.pValue[ 0 ] = temp;
	return 0;
}
int DaoList_PushBack( DaoList *self, DaoValue *item )
{
	DaoType *tp = self->unitype ? self->unitype->nested->items.pType[0] : NULL;
	DaoValue *temp = NULL;
	if( DaoValue_Move( item, & temp, tp ) ==0 ){
		GC_DecRC( temp );
		return 1;
	}
	DArray_PushBack( & self->items, NULL );
	self->items.items.pValue[ self->items.size - 1 ] = temp;
	return 0;
}
void DaoList_PopFront( DaoList *self )
{
	if( self->items.size ==0 ) return;
	DArray_PopFront( & self->items );
}
void DaoList_PopBack( DaoList *self )
{
	if( self->items.size ==0 ) return;
	DArray_PopBack( & self->items );
}

DaoTypeBase listTyper=
{
	"list", & listCore, NULL, (DaoFuncItem*)listMeths, {0}, {0},
	(FuncPtrDel) DaoList_Delete, NULL
};

DaoList* DaoList_New()
{
	DaoList *self = (DaoList*) dao_calloc( 1, sizeof(DaoList) );
	DaoValue_Init( self, DAO_LIST );
	self->items.type = D_VALUE;
	//self->items = DArray_New(D_VALUE);
	self->unitype = NULL;
	return self;
}
void DaoList_Delete( DaoList *self )
{
	GC_DecRC( self->unitype );
	DaoList_Clear( self );
	dao_free( self );
}
void DaoList_Clear( DaoList *self )
{
	DArray_Clear( & self->items );
}
int DaoList_Append( DaoList *self, DaoValue *value )
{
	return DaoList_PushBack( self, value );
}
void DaoList_Erase( DaoList *self, daoint pos )
{
	if( pos >= self->items.size ) return;
	DArray_Erase( & self->items, pos, 1 );
}

/**/
static void DaoMap_Print( DaoValue *self0, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	DaoMap *self = & self0->xMap;
	const char *kvsym = self->items->hashing ? " : " : " => ";
	const daoint size = self->items->size;
	daoint i = 0;

	DNode *node = NULL;
	if( cycData ) node = MAP_Find( cycData, self );
	if( node ){
		DaoStream_WriteMBS( stream, "{ ... }" );
		return;
	}
	if( cycData ) MAP_Insert( cycData, self, self );
	DaoStream_WriteMBS( stream, "{ " );

	node = DMap_First( self->items );
	for( ; node!=NULL; node=DMap_Next(self->items,node) ){
		stream->useQuote = 1;
		DaoValue_Print( node->key.pValue, proc, stream, cycData );
		DaoStream_WriteMBS( stream, kvsym );
		DaoValue_Print( node->value.pValue, proc, stream, cycData );
		stream->useQuote = 0;
		if( i+1<size ) DaoStream_WriteMBS( stream, ", " );
		i++;
	}
	if( size==0 ) DaoStream_WriteMBS( stream, kvsym );
	DaoStream_WriteMBS( stream, " }" );
	if( cycData ) MAP_Erase( cycData, self );
}
static void DaoMap_GetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid )
{
	DaoMap *self = & self0->xMap;
	if( pid->type == DAO_TUPLE && pid->xTuple.unitype == dao_type_for_iterator ){
		DaoTuple *iter = & pid->xTuple;
		DaoTuple *tuple = DaoProcess_PutTuple( proc );
		DNode *node = (DNode*) iter->items[1]->xCdata.data;
		if( node == NULL || tuple->size != 2 ) return;
		DaoValue_Copy( node->key.pValue, tuple->items );
		DaoValue_Copy( node->value.pValue, tuple->items + 1 );
		node = DMap_Next( self->items, node );
		iter->items[0]->xInteger.value = node != NULL;
		iter->items[1]->xCdata.data = node;
	}else{
		DNode *node = MAP_Find( self->items, pid );
		if( node ==NULL ){
			DaoProcess_RaiseException( proc, DAO_ERROR_KEY, NULL );
			return;
		}
		DaoProcess_PutReference( proc, node->value.pValue );
	}
}
extern DaoType *dao_map_any;
static void DaoMap_SetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid, DaoValue *value )
{
	DaoMap *self = & self0->xMap;
	int c = DaoMap_Insert( self, pid, value );
	if( c ==1 ){
		DaoProcess_RaiseException( proc, DAO_ERROR_TYPE, "key not matching" );
	}else if( c ==2 ){
		DaoProcess_RaiseException( proc, DAO_ERROR_TYPE, "value not matching" );
	}
}
static void DaoMap_GetItem2( DaoValue *self0, DaoProcess *proc, DaoValue *ids[], int N )
{
	DaoMap *self = & self0->xMap;
	DaoMap *map = DaoProcess_PutMap( proc );
	DNode *node1 = DMap_First( self->items );
	DNode *node2 = NULL;
	if( ids[0]->type ) node1 = MAP_FindGE( self->items, ids[0] );
	if( ids[1]->type ) node2 = MAP_FindLE( self->items, ids[1] );
	if( node2 ) node2 = DMap_Next(self->items, node2 );
	for(; node1 != node2; node1 = DMap_Next(self->items, node1 ) )
		DaoMap_Insert( map, node1->key.pValue, node1->value.pValue );
}
static void DaoMap_GetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N )
{
	switch( N ){
	case 0 : DaoMap_GetItem1( self, proc, dao_none_value ); break;
	case 1 : DaoMap_GetItem1( self, proc, ids[0] ); break;
	case 2 : DaoMap_GetItem2( self, proc, ids, N ); break;
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}
static void DaoMap_SetItem2( DaoValue *self0, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	DaoMap *self = & self0->xMap;
	DaoType *tp = self->unitype;
	DaoType *tp2=NULL;
	DNode *node1 = DMap_First( self->items );
	DNode *node2 = NULL;
	if( tp == NULL ){
		/* a : tuple<string,map<string,int>> = ('',{=>});
		   duplicating the constant to assign to "a" may not set the unitype properly */
		tp = proc->activeTypes[ proc->activeCode->c ];
		if( tp == NULL || tp->tid == 0 ) tp = dao_map_any;
		self->unitype = tp;
		GC_IncRC( tp );
	}
	if( tp ){
		if( tp->nested->size != 2 || tp->nested->items.pType[1] == NULL ){
			DaoProcess_RaiseException( proc, DAO_ERROR_TYPE, "invalid map" );
			return;
		}
		tp2 = tp->nested->items.pType[1];
		if( DaoType_MatchValue( tp2, value, NULL ) ==0 )
			DaoProcess_RaiseException( proc, DAO_ERROR_TYPE, "value not matching" );
	}
	if( ids[0]->type ) node1 = MAP_FindGE( self->items, ids[0] );
	if( ids[1]->type ) node2 = MAP_FindLE( self->items, ids[1] );
	if( node2 ) node2 = DMap_Next(self->items, node2 );
	for(; node1 != node2; node1 = DMap_Next(self->items, node1 ) )
		DaoValue_Move( value, & node1->value.pValue, tp2 );
}
static void DaoMap_SetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	switch( N ){
	case 0 : DaoMap_SetItem1( self, proc, dao_none_value, value ); break;
	case 1 : DaoMap_SetItem1( self, proc, ids[0], value ); break;
	case 2 : DaoMap_SetItem2( self, proc, ids, N, value ); break;
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}
static DaoValue* DaoMap_Copy( DaoValue *self0, DaoProcess *proc, DMap *cycData )
{
	DaoMap *copy, *self = & self0->xMap;
	DNode *node;

	if( cycData ){
		DNode *node = MAP_Find( cycData, self );
		if( node ) return node->value.pValue;
	}

	copy = DaoMap_New( self->items->hashing );
	copy->unitype = self->unitype;
	GC_IncRC( copy->unitype );

	node = DMap_First( self->items );
	if( cycData ){
		MAP_Insert( cycData, self, copy );
		for( ; node!=NULL; node = DMap_Next(self->items, node) ){
			DaoValue *key = DaoValue_MakeCopy( node->key.pValue, proc, cycData );
			DaoValue *value = DaoValue_MakeCopy( node->value.pValue, proc, cycData );
			MAP_Insert( copy->items, & key, & value );
			GC_IncRC( key ); GC_IncRC( value );
			GC_DecRC( key ); GC_DecRC( value );
		}
	}else{
		for( ; node!=NULL; node = DMap_Next(self->items, node) ){
			MAP_Insert( copy->items, node->key.pValue, node->value.pValue );
		}
	}
	return (DaoValue*) copy;
}
static DaoTypeCore mapCore =
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoMap_GetItem,
	DaoMap_SetItem,
	DaoMap_Print,
	DaoMap_Copy,
};

static void DaoMAP_Clear( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap_Clear( & p[0]->xMap );
}
static void DaoMAP_Reset( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap_Reset( & p[0]->xMap );
}
static void DaoMAP_Erase( DaoProcess *proc, DaoValue *p[], int N )
{
	DMap *self = p[0]->xMap.items;
	DNode *ml, *mg;
	DArray *keys;
	N --;
	switch( N ){
	case 0 :
		DMap_Clear( self ); break;
	case 1 :
		MAP_Erase( self, p[1] );
		break;
	case 2 :
		mg = MAP_FindGE( self, p[1] );
		ml = MAP_FindLE( self, p[2] );
		if( mg ==NULL || ml ==NULL ) return;
		ml = DMap_Next( self, ml );
		keys = DArray_New(0);
		for(; mg != ml; mg=DMap_Next(self, mg)) DArray_Append( keys, mg->key.pVoid );
		while( keys->size ){
			MAP_Erase( self, keys->items.pVoid[0] );
			DArray_PopFront( keys );
		}
		DArray_Delete( keys );
		break;
	default : break;
	}
}
static void DaoMAP_Insert( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap *self = & p[0]->xMap;
	int c = DaoMap_Insert( self, p[1], p[2] );
	if( c ==1 ){
		DaoProcess_RaiseException( proc, DAO_ERROR_TYPE, "key not matching" );
	}else if( c ==2 ){
		DaoProcess_RaiseException( proc, DAO_ERROR_TYPE, "value not matching" );
	}
}
static void DaoMAP_Find( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap *self = & p[0]->xMap;
	DaoTuple *res = NULL;
	DNode *node;
	switch( (int)p[2]->xEnum.value ){
	case 0 :
		node = MAP_FindLE( self->items, p[1] );
		if( node == NULL ) break;
		res = DaoProcess_PutTuple( proc );
		DaoValue_Copy( node->key.pValue, res->items );
		DaoValue_Copy( node->value.pValue, res->items + 1 );
		break;
	case 1  :
		node = MAP_Find( self->items, p[1] );
		if( node == NULL ) break;
		res = DaoProcess_PutTuple( proc );
		DaoValue_Copy( node->key.pValue, res->items );
		DaoValue_Copy( node->value.pValue, res->items + 1 );
		break;
	case 2  :
		node = MAP_FindGE( self->items, p[1] );
		if( node == NULL ) break;
		res = DaoProcess_PutTuple( proc );
		DaoValue_Copy( node->key.pValue, res->items );
		DaoValue_Copy( node->value.pValue, res->items + 1 );
		break;
	default : break;
	}
	if( res == NULL ) DaoProcess_PutValue( proc, dao_none_value );
}
static void DaoMAP_Key( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap *self = & p[0]->xMap;
	DaoList *list = DaoProcess_PutList( proc );
	DNode *node, *ml=NULL, *mg=NULL;
	N --;
	switch( N ){
	case 0 :
		mg = DMap_First( self->items );
		break;
	case 1 :
		mg = MAP_FindGE( self->items, p[1] );
		break;
	case 2 :
		mg = MAP_FindGE( self->items, p[1] );
		ml = MAP_FindLE( self->items, p[2] );
		if( ml == NULL ) return;
		ml = DMap_Next( self->items, ml );
		break;
	default: break;
	}
	if( mg == NULL ) return;
	for( node=mg; node != ml; node = DMap_Next( self->items, node ) )
		DaoList_Append( list, node->key.pValue );
}
static void DaoMAP_Value( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap *self = & p[0]->xMap;
	DaoList *list = DaoProcess_PutList( proc );
	DNode *node, *ml=NULL, *mg=NULL;
	N --;
	switch( N ){
	case 0 :
		mg = DMap_First( self->items );
		break;
	case 1 :
		mg = MAP_FindGE( self->items, p[1] );
		break;
	case 2 :
		mg = MAP_FindGE( self->items, p[1] );
		ml = MAP_FindLE( self->items, p[2] );
		if( ml ==NULL ) return;
		ml = DMap_Next( self->items, ml );
		break;
	default: break;
	}
	if( mg == NULL ) return;
	for( node=mg; node != ml; node = DMap_Next( self->items, node ) ){
		DaoList_Append( list, node->value.pValue );
	}
}
static void DaoMAP_Has( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap *self = & p[0]->xMap;
	DaoProcess_PutInteger( proc, DMap_Find( self->items, p[1] ) != NULL );
}
static void DaoMAP_Size( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap *self = & p[0]->xMap;
	DaoProcess_PutInteger( proc, self->items->size );
}
static void DaoMAP_Iter( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap *self = & p[0]->xMap;
	DaoTuple *tuple = & p[1]->xTuple;
	DaoValue **data = tuple->items;
	DNode *node = DMap_First( self->items );
	data[0]->xInteger.value = self->items->size >0;
	if( data[1]->type != DAO_CDATA || data[1]->xCdata.ctype != dao_default_cdata.ctype ){
		DaoCdata *it = DaoCdata_New( dao_default_cdata.ctype, node );
		GC_ShiftRC( it, data[1] );
		data[1] = (DaoValue*) it;
	}else{
		data[1]->xCdata.data = node;
	}
}
static void DaoMAP_Functional( DaoProcess *proc, DaoValue *p[], int N, int funct )
{
	daoint *count = NULL;
	DaoMap *self = & p[0]->xMap;
	DaoMap *map = NULL;
	DaoList *list = NULL;
	DaoTuple *tuple = NULL;
	DaoType *type = self->unitype;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoValue *res;
	DNode *node;
	ushort_t entry;
	switch( funct ){
	case DVM_FUNCT_MAP :
	case DVM_FUNCT_SELECT :
	case DVM_FUNCT_KEYS :
	case DVM_FUNCT_VALUES : list = DaoProcess_PutList( proc ); break;
	case DVM_FUNCT_COUNT : count = DaoProcess_PutInteger( proc, 0 ); break;
	case DVM_FUNCT_APPLY : DaoProcess_PutReference( proc, p[0] ); break;
	case DVM_FUNCT_FIND : DaoProcess_PutValue( proc, dao_none_value ); break;
	}
	if( sect == NULL ) return;
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	type = type && type->nested->size > 1 ? type->nested->items.pType[1] : NULL;
	DaoProcess_AcquireCV( proc );
	for(node=DMap_First(self->items); node; node=DMap_Next(self->items,node)){
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, node->key.pValue );
		if( sect->b >1 ) DaoProcess_SetValue( proc, sect->a+1, node->value.pValue );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_VMPROC_ABORTED ) break;
		res = proc->stackValues[0];
		switch( funct ){
		case DVM_FUNCT_SELECT :
			if( res->xInteger.value ){
				tuple = DaoTuple_New(2);
				DaoList_Append( list, (DaoValue*) tuple );
				DaoTuple_SetItem( tuple, node->key.pValue, 0 );
				DaoTuple_SetItem( tuple, node->value.pValue, 1 );
			}
			break;
		case DVM_FUNCT_KEYS :
			if( res->xInteger.value ) DaoList_Append( list, node->key.pValue );
			break;
		case DVM_FUNCT_VALUES :
			if( res->xInteger.value ) DaoList_Append( list, node->value.pValue );
			break;
		case DVM_FUNCT_COUNT : *count += res->xInteger.value != 0; break;
		case DVM_FUNCT_APPLY : DaoValue_Move( res, & node->value.pValue, type ); break;
		case DVM_FUNCT_MAP : DaoList_Append( list, res ); break;
		}
		if( funct == DVM_FUNCT_FIND && res->xInteger.value ){
			DaoProcess_PopFrame( proc );
			DaoProcess_SetActiveFrame( proc, proc->topFrame );
			tuple = DaoProcess_PutTuple( proc );
			GC_ShiftRC( node->key.pValue, tuple->items[0] );
			GC_ShiftRC( node->value.pValue, tuple->items[1] );
			tuple->items[0] = node->key.pValue;
			tuple->items[1] = node->value.pValue;
			break;
		}
	}
	DaoProcess_ReleaseCV( proc );
	if( funct != DVM_FUNCT_FIND ) DaoProcess_PopFrame( proc );
}
static void DaoMAP_Iterate( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMAP_Functional( proc, p, N, DVM_FUNCT_ITERATE );
}
static void DaoMAP_Count( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMAP_Functional( proc, p, N, DVM_FUNCT_COUNT );
}
static void DaoMAP_Keys( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	if( sect == NULL ){
		DaoMAP_Key( proc, p, N );
		return;
	}
	DaoMAP_Functional( proc, p, N, DVM_FUNCT_KEYS );
}
static void DaoMAP_Values( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	if( sect == NULL ){
		DaoMAP_Value( proc, p, N );
		return;
	}
	DaoMAP_Functional( proc, p, N, DVM_FUNCT_VALUES );
}
static void DaoMAP_Find2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMAP_Functional( proc, p, N, DVM_FUNCT_FIND );
}
static void DaoMAP_Select( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMAP_Functional( proc, p, N, DVM_FUNCT_SELECT );
}
static void DaoMAP_Map( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMAP_Functional( proc, p, N, DVM_FUNCT_MAP );
}
static void DaoMAP_Apply( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMAP_Functional( proc, p, N, DVM_FUNCT_APPLY );
}
static DaoFuncItem mapMeths[] =
{
	{ DaoMAP_Clear,  "clear( self :map<any,any> )" },
	{ DaoMAP_Reset,  "reset( self :map<any,any> )" },
	{ DaoMAP_Erase,  "erase( self :map<any,any> )" },
	{ DaoMAP_Erase,  "erase( self :map<@K,@V>, from :@K )" },
	{ DaoMAP_Erase,  "erase( self :map<@K,@V>, from :@K, to :@K )" },
	{ DaoMAP_Insert, "insert( self :map<@K,@V>, key :@K, value :@V )" },
	{ DaoMAP_Find,   "find( self :map<@K,@V>, key :@K, type :enum<le,eq,ge>=$eq )=>tuple<key:@K,value:@V>|none" },
	{ DaoMAP_Key,    "keys( self :map<@K,any>, from :@K )=>list<@K>" },
	{ DaoMAP_Key,    "keys( self :map<@K,any>, from :@K, to :@K )=>list<@K>" },
	{ DaoMAP_Value,  "values( self :map<@K,@V>, from :@K )=>list<@V>" },
	{ DaoMAP_Value,  "values( self :map<@K,@V>, from :@K, to :@K )=>list<@V>" },
	{ DaoMAP_Has,    "has( self :map<@K,any>, key :@K )=>int" },
	{ DaoMAP_Size,   "size( self :map<any,any> )=>int" },
	{ DaoMAP_Iter,   "__for_iterator__( self :map<any,any>, iter : for_iterator )" },

	{ DaoMAP_Iterate,   "iterate( self :map<@K,@V> )[key :@K, value :@V]" },
	{ DaoMAP_Count,  "count( self :map<@K,@V> )[key :@K, value :@V =>int] =>int" },
	{ DaoMAP_Keys,   "keys( self :map<@K,@V> )[key :@K, value :@V =>int] =>list<@K>" },
	{ DaoMAP_Values, "values( self :map<@K,@V> )[key :@K, value :@V =>int] =>list<@V>" },
	{ DaoMAP_Select, "select( self :map<@K,@V> )[key :@K, value :@V =>int] =>list<tuple<key:@K,value:@V>>" },
	{ DaoMAP_Find2,  "find( self :map<@K,@V> )[key :@K, value :@V =>int] =>tuple<key:@K,value:@V>|none" },
	{ DaoMAP_Map,    "map( self :map<@K,@V> )[key :@K, value :@V =>@T] =>list<@T>" },
	{ DaoMAP_Apply,  "apply( self :map<@K,@V> )[key :@K, value :@V =>@V] =>map<@K,@V>" },
	{ NULL, NULL }
};

int DaoMap_Size( DaoMap *self )
{
	return self->items->size;
}
DaoValue* DaoMap_GetValue( DaoMap *self, DaoValue *key  )
{
	DNode *node = MAP_Find( self->items, & key );
	if( node ) return node->value.pValue;
	return NULL;
}
int DaoMap_InsertMBS( DaoMap *self, const char *key, DaoValue *value )
{
	DaoString vkey = { DAO_STRING,0,0,0,1,NULL};
	DString str = DString_WrapMBS( key );
	vkey.data = & str;
	return DaoMap_Insert( self, (DaoValue*) & vkey, value );
}
int DaoMap_InsertWCS( DaoMap *self, const wchar_t *key, DaoValue *value )
{
	DaoString vkey = { DAO_STRING,0,0,0,1,NULL};
	DString str = DString_WrapWCS( key );
	vkey.data = & str;
	return DaoMap_Insert( self, (DaoValue*) & vkey, value );
}
void DaoMap_EraseMBS ( DaoMap *self, const char *key )
{
	DaoString vkey = { DAO_STRING,0,0,0,1,NULL};
	DString str = DString_WrapMBS( key );
	vkey.data = & str;
	DaoMap_Erase( self, (DaoValue*) & vkey );
}
void DaoMap_EraseWCS ( DaoMap *self, const wchar_t *key )
{
	DaoString vkey = { DAO_STRING,0,0,0,1,NULL};
	DString str = DString_WrapWCS( key );
	vkey.data = & str;
	DaoMap_Erase( self, (DaoValue*) & vkey );
}
DaoValue* DaoMap_GetValueMBS( DaoMap *self, const char *key  )
{
	DaoString vkey = { DAO_STRING,0,0,0,1,NULL};
	DString str = DString_WrapMBS( key );
	DNode *node;
	vkey.data = & str;
	node = MAP_Find( self->items, (DaoValue*) &  vkey );
	if( node ) return node->value.pValue;
	return NULL;
}
DaoValue* DaoMap_GetValueWCS( DaoMap *self, const wchar_t *key  )
{
	DaoString vkey = { DAO_STRING,0,0,0,1,NULL};
	DString str = DString_WrapWCS( key );
	DNode *node;
	vkey.data = & str;
	node = MAP_Find( self->items, (DaoValue*) &  vkey );
	if( node ) return node->value.pValue;
	return NULL;
}

DaoTypeBase mapTyper=
{
	"map", & mapCore, NULL, (DaoFuncItem*) mapMeths, {0}, {0},
	(FuncPtrDel)DaoMap_Delete, NULL
};

DaoMap* DaoMap_New( unsigned int hashing )
{
	DaoMap *self = (DaoMap*) dao_malloc( sizeof( DaoMap ) );
	DaoValue_Init( self, DAO_MAP );
	self->items = hashing ? DHash_New( D_VALUE, D_VALUE ) : DMap_New( D_VALUE, D_VALUE );
	self->unitype = NULL;
	if( hashing > 1 ) self->items->hashing = hashing;
	return self;
}
void DaoMap_Delete( DaoMap *self )
{
	GC_DecRC( self->unitype );
	DaoMap_Clear( self );
	DMap_Delete( self->items );
	dao_free( self );
}
void DaoMap_Clear( DaoMap *self )
{
	DMap_Clear( self->items );
}
void DaoMap_Reset( DaoMap *self )
{
	DMap_Reset( self->items );
}
int DaoMap_Insert( DaoMap *self, DaoValue *key, DaoValue *value )
{
	DaoType *tp = self->unitype;
	DaoType *tp1=NULL, *tp2=NULL;
	DaoValue *key2 = NULL;
	DaoValue *value2 = NULL;
	int mt;
	if( tp ){
		if( tp->nested->size >=2 ){
			tp1 = tp->nested->items.pType[0];
			tp2 = tp->nested->items.pType[1];
		}else if( tp->nested->size >=1 ){
			tp1 = tp->nested->items.pType[0];
		}
	}
	/* type checking and setting */
	if( tp1 ){
		if( (mt = DaoType_MatchValue( tp1, key, NULL )) ==0 ) return 1;
		if( mt != DAO_MT_EQ ){
			if( DaoValue_Move( key, & key2, tp1 ) == 0 ){
				GC_DecRC( key2 );
				return 1;
			}
			key = key2;
		}
	}
	if( tp2 ){
		if( (mt = DaoType_MatchValue( tp2, value, NULL )) ==0 ) return 2;
		if( mt != DAO_MT_EQ ){
			if( DaoValue_Move( value, & value2, tp2 ) == 0 ){
				GC_DecRC( value2 );
				return 2;
			}
			value = value2;
		}
	}
	DMap_Insert( self->items, key, value );
	GC_DecRC( key2 );
	GC_DecRC( value2 );
	return 0;
}
void DaoMap_Erase( DaoMap *self, DaoValue *key )
{
	MAP_Erase( self->items, key );
}
DNode* DaoMap_First( DaoMap *self )
{
	return DMap_First(self->items);
}
DNode* DaoMap_Next( DaoMap *self, DNode *iter )
{
	return DMap_Next(self->items,iter);
}



/* ---------------------
 * Dao Tuple
 * ---------------------*/
static int DaoTuple_GetIndexE( DaoTuple *self, DaoProcess *proc, DString *name )
{
	int id = DaoTuple_GetIndex( self, name );
	if( id <0 || id >= self->size ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid tuple" );
		return -1;
	}
	return id;
}
static void DaoTupleCore_GetField( DaoValue *self0, DaoProcess *proc, DString *name )
{
	DaoTuple *self = & self0->xTuple;
	int id = DaoTuple_GetIndexE( self, proc, name );
	if( id <0 ) return;
	DaoProcess_PutReference( proc, self->items[id] );
}
static void DaoTupleCore_SetField( DaoValue *self0, DaoProcess *proc, DString *name, DaoValue *value )
{
	DaoTuple *self = & self0->xTuple;
	DaoType *t, **type = self->unitype->nested->items.pType;
	int id = DaoTuple_GetIndexE( self, proc, name );
	if( id <0 ) return;
	t = type[id];
	if( t->tid == DAO_PAR_NAMED ) t = & t->aux->xType;
	if( DaoValue_Move( value, self->items + id, t ) ==0)
		DaoProcess_RaiseException( proc, DAO_ERROR, "type not matching" );
}
static void DaoTupleCore_GetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid )
{
	DaoTuple *self = & self0->xTuple;
	int ec = DAO_ERROR_INDEX;
	if( pid->type == DAO_NONE ){
		ec = 0;
		/* return a copy. TODO */
	}else if( pid->type >= DAO_INTEGER && pid->type <= DAO_DOUBLE ){
		int id = DaoValue_GetInteger( pid );
		if( id >=0 && id < self->size ){
			DaoProcess_PutReference( proc, self->items[id] );
			ec = 0;
		}else{
			ec = DAO_ERROR_INDEX_OUTOFRANGE;
		}
	}
	if( ec ) DaoProcess_RaiseException( proc, ec, "" );
}
static void DaoTupleCore_SetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid, DaoValue *value )
{
	DaoTuple *self = & self0->xTuple;
	DaoType *t, **type = self->unitype->nested->items.pType;
	int ec = 0;
	if( pid->type >= DAO_INTEGER && pid->type <= DAO_DOUBLE ){
		int id = DaoValue_GetInteger( pid );
		if( id >=0 && id < self->size ){
			t = type[id];
			if( t->tid == DAO_PAR_NAMED ) t = & t->aux->xType;
			if( DaoValue_Move( value, self->items + id, t ) ==0 ) ec = DAO_ERROR_TYPE;
		}else{
			ec = DAO_ERROR_INDEX_OUTOFRANGE;
		}
	}else{
		ec = DAO_ERROR_INDEX;
	}
	if( ec ) DaoProcess_RaiseException( proc, ec, "" );
}
static void DaoTupleCore_GetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N )
{
	switch( N ){
	case 0 : DaoTupleCore_GetItem1( self, proc, dao_none_value ); break;
	case 1 : DaoTupleCore_GetItem1( self, proc, ids[0] ); break;
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}
static void DaoTupleCore_SetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	switch( N ){
	case 0 : DaoTupleCore_SetItem1( self, proc, dao_none_value, value ); break;
	case 1 : DaoTupleCore_SetItem1( self, proc, ids[0], value ); break;
	default : DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not supported" );
	}
}
static DaoValue* DaoTupleCore_Copy( DaoValue *self0, DaoProcess *proc, DMap *cycData )
{
	DaoTuple *copy, *self = & self0->xTuple;
	DaoValue **data = self->items;

	if( cycData ){
		DNode *node = MAP_Find( cycData, self );
		if( node ) return node->value.pValue;
	}

	copy = DaoTuple_New( self->size );
	copy->unitype = self->unitype;
	GC_IncRC( copy->unitype );
	if( cycData ) MAP_Insert( cycData, self, copy );

	DaoCopyValues( copy->items, data, self->size, proc, cycData );
	return (DaoValue*) copy;
}
static DaoTypeCore tupleCore=
{
	NULL,
	DaoTupleCore_GetField,
	DaoTupleCore_SetField,
	DaoTupleCore_GetItem,
	DaoTupleCore_SetItem,
	DaoListCore_Print,
	DaoTupleCore_Copy,
};
DaoTypeBase tupleTyper=
{
	"tuple", & tupleCore, NULL, NULL, {0}, {0}, (FuncPtrDel) DaoTuple_Delete, NULL
};
DaoTuple* DaoTuple_New( int size )
{
	int extra = size > DAO_TUPLE_ITEMS ? size - DAO_TUPLE_ITEMS : 0;
	DaoTuple *self = (DaoTuple*) dao_calloc( 1, sizeof(DaoTuple) + extra*sizeof(DaoValue*) );
	self->type = DAO_TUPLE;
	self->size = size;
	self->unitype = NULL;
	return self;
}
#if 0
DaoTuple* DaoTuple_Create( DaoType *type, int init )
{
	int i, size = type->nested->size;
	int extit = size > DAO_TUPLE_ITEMS ? size - DAO_TUPLE_ITEMS : 0;
	DaoType **types = type->nested->items.pType;
	DaoTuple *self = (DaoTuple*) dao_calloc( 1, sizeof(DaoTuple) + extit*sizeof(DaoValue*) );
	self->type = DAO_TUPLE;
	if( init ){
		for(i=0; i<size; i++){
			DaoType *it = types[i];
			if( it->tid == DAO_PAR_NAMED ) it = & it->aux->xType;
			if( it->tid > DAO_ENUM && it->tid != DAO_ANY && it->tid != DAO_INITYPE ) continue;
			DaoValue_Move( it->value, self->items + i, it );
		}
	}
	GC_IncRC( type );
	self->size = size;
	self->unitype = type;
	return self;
}
#else
DaoTuple* DaoTuple_Create( DaoType *type, int init )
{
	int i, size = type->nested->size;
	int extit = size > DAO_TUPLE_ITEMS ? size - DAO_TUPLE_ITEMS : 0;
	int extra = extit*sizeof(DaoValue*) + type->rntcount*sizeof(DaoDouble);
	DaoType **types = type->nested->items.pType;
	DaoTuple *self = (DaoTuple*) dao_calloc( 1, sizeof(DaoTuple) + extra );
	DaoDouble *buffer = (DaoDouble*)(self->items + size);
	self->type = DAO_TUPLE;
	for(i=0; i<size; i++){
		DaoType *it = types[i];
		if( it->tid == DAO_PAR_NAMED ) it = & it->aux->xType;
		if( it->tid >= DAO_INTEGER && it->tid <= DAO_DOUBLE ){
			self->items[i] = (DaoValue*)buffer;
			buffer->type = it->tid;
			buffer->refCount = 2;
			buffer ++;
		}else if( init && it->tid >= DAO_COMPLEX && it->tid <= DAO_ENUM ){
			DaoValue_Move( it->value, self->items + i, it );
		}
	}
	GC_IncRC( type );
	self->size = size;
	self->unitype = type;
	return self;
}
#endif
void DaoTuple_Delete( DaoTuple *self )
{
	int i;
	for(i=0; i<self->size; i++) GC_DecRC( self->items[i] );
	GC_DecRC( self->unitype );
	dao_free( self );
}

int  DaoTuple_Size( DaoTuple *self )
{
	return self->size;
}
int DaoTuple_GetIndex( DaoTuple *self, DString *name )
{
	DaoType *abtp = self->unitype;
	DNode *node = NULL;
	if( abtp && abtp->mapNames ) node = MAP_Find( abtp->mapNames, name );
	if( node == NULL || node->value.pInt >= self->size ) return -1;
	return node->value.pInt;
}
void DaoTuple_SetItem( DaoTuple *self, DaoValue *it, int pos )
{
	DaoValue **val;
	if( pos <0 || pos >= self->size ) return;
	val = self->items + pos;
	if( self->unitype && self->unitype->nested->size ){
		DaoType *t = self->unitype->nested->items.pType[pos];
		if( t->tid == DAO_PAR_NAMED ) t = & t->aux->xType;
		DaoValue_Move( it, val, t );
	}else{
		DaoValue_Copy( it, val );
	}
}
DaoValue* DaoTuple_GetItem( DaoTuple *self, int pos )
{
	if( pos <0 || pos >= self->size ) return NULL;
	return self->items[pos];
}



void DaoNameValue_Delete( DaoNameValue *self )
{
	DString_Delete( self->name );
	DaoValue_Clear( & self->value );
	GC_DecRC( self->unitype );
	dao_free( self );
}
static DaoValue* DaoNameValue_Copy( DaoValue *self0, DaoProcess *proc, DMap *cycData )
{
	DaoNameValue *self = & self0->xNameValue;
	DaoNameValue *copy = DaoNameValue_New( self->name, self->value );
	copy->unitype = self->unitype;
	GC_IncRC( self->unitype );
	return (DaoValue*) copy;
}
static void DaoNameValue_Print( DaoValue *self0, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	DaoNameValue *self = & self0->xNameValue;
	DaoStream_WriteString( stream, self->name );
	DaoStream_WriteMBS( stream, "=>" );
	DaoValue_Print( self->value, proc, stream, cycData );
}
static DaoTypeCore namevaCore=
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoValue_GetItem,
	DaoValue_SetItem,
	DaoNameValue_Print,
	DaoNameValue_Copy
};
DaoTypeBase namevaTyper =
{
	"NameValue", & namevaCore, NULL, NULL, {0}, {0}, (FuncPtrDel) DaoNameValue_Delete, NULL
};
DaoNameValue* DaoNameValue_New( DString *name, DaoValue *value )
{
	DaoNameValue *self = (DaoNameValue*)dao_malloc( sizeof(DaoNameValue) );
	DaoValue_Init( self, DAO_PAR_NAMED );
	self->name = DString_Copy( name );
	self->unitype = NULL;
	self->value = NULL;
	DaoValue_Copy( value, & self->value );
	return self;
}



DMap *dao_cdata_bindings = NULL;
static DaoCdata* DaoCdataBindings_Find( void *data )
{
	DNode *node = DMap_Find( dao_cdata_bindings, data );
	if( node ) return (DaoCdata*) node->value.pVoid;
	return NULL;
}
#ifdef DAO_WITH_THREAD
DMutex dao_cdata_mutex;
static void DaoCdataBindings_Insert( void *data, DaoCdata *wrap )
{
	if( data == NULL ) return;
	DMutex_Lock( & dao_cdata_mutex );
	DMap_Insert( dao_cdata_bindings, data, wrap );
	DMutex_Unlock( & dao_cdata_mutex );
}
static void DaoCdataBindings_Erase( void *data )
{
	if( data == NULL ) return;
	DMutex_Lock( & dao_cdata_mutex );
	DMap_Erase( dao_cdata_bindings, data );
	DMutex_Unlock( & dao_cdata_mutex );
}
#else
static void DaoCdataBindings_Insert( void *data, DaoCdata *wrap )
{
	if( data == NULL ) return;
	DMap_Insert( dao_cdata_bindings, data, wrap );
}
static void DaoCdataBindings_Erase( void *data )
{
	if( data == NULL ) return;
	DMap_Erase( dao_cdata_bindings, data );
}
#endif

/**/
void DaoCdata_InitCommon( DaoCdata *self, DaoType *type )
{
	DaoValue_Init( self, DAO_CDATA );
	self->subtype = DAO_CDATA_PTR;
	self->typer = & defaultCdataTyper;
	self->object = NULL;
	self->ctype = type;
	if( type == NULL ) self->ctype = dao_default_cdata.ctype;
	if( self->ctype ){
		self->typer = self->ctype->typer;
		self->subtype = self->ctype->cdatatype;
		GC_IncRC( self->ctype );
	}
}
void DaoCdata_FreeCommon( DaoCdata *self )
{
	if( self->ctype ) GC_DecRC( self->ctype );
	if( self->object ) GC_DecRC( self->object );
	self->object = NULL;
	self->ctype = NULL;
}
DaoCdata* DaoCdata_New( DaoType *type, void *data )
{
	DaoCdata *self = DaoCdataBindings_Find( data );
	if( self && self->ctype == type && self->data == data ) return self;
	self = (DaoCdata*)dao_calloc( 1, sizeof(DaoCdata) );
	DaoCdata_InitCommon( self, type );
	self->data = data;
	if( data ) DaoCdataBindings_Insert( data, self );
	return self;
}
DaoCdata* DaoCdata_Wrap( DaoType *type, void *data )
{
	DaoCdata *self = DaoCdataBindings_Find( data );
	if( self && self->ctype == type && self->data == data ) return self;
	self = DaoCdata_New( type, data );
	self->subtype = DAO_CDATA_PTR;
	return self;
}
static void DaoCdata_DeleteData( DaoCdata *self );
void DaoCdata_Delete( DaoCdata *self )
{
	if( self->type == DAO_CTYPE ){
		DaoCtype_Delete( (DaoCtype*) self );
		return;
	}
	DaoCdata_DeleteData( self );
	dao_free( self );
}
void DaoCdata_DeleteData( DaoCdata *self )
{
	void (*fdel)(void*) = (void (*)(void *))DaoCdata_Delete;
	if( self->subtype != DAO_CDATA_DAO ) DaoCdataBindings_Erase( self->data );
	if( self->subtype == DAO_CDATA_CXX && self->data != NULL ){
		if( self->typer->Delete && self->typer->Delete != fdel ){
			self->typer->Delete( self->data );
		}else{
			dao_free( self->data );
		}
		self->data = NULL;
	}
	DaoCdata_FreeCommon( self );
}
int DaoCdata_IsType( DaoCdata *self, DaoType *type )
{
	return DaoType_ChildOf( self->ctype, type );
}
int DaoCdata_OwnData( DaoCdata *self )
{
	return self->subtype == DAO_CDATA_CXX;
}
void DaoCdata_SetType( DaoCdata *self, DaoType *type )
{
	if( type == NULL ) return;
	GC_ShiftRC( type, self->ctype );
	self->ctype = type;
	self->subtype = type->cdatatype;
}
void DaoCdata_SetData( DaoCdata *self, void *data )
{
	if( self->data ) DaoCdataBindings_Erase( self->data );
	self->data = data;
	if( data ) DaoCdataBindings_Insert( data, self );
}
void* DaoCdata_GetData( DaoCdata *self )
{
	return self->data;
}
void** DaoCdata_GetData2( DaoCdata *self )
{
	return & self->data;
}
DaoObject* DaoCdata_GetObject( DaoCdata *self )
{
	return (DaoObject*)self->object;
}
DaoTypeBase* DaoCdata_GetTyper(DaoCdata *self )
{
	return self->ctype->typer;
}
void* DaoCdata_CastData( DaoCdata *self, DaoType *totype )
{
	DaoValue *value = DaoType_CastToParent( (DaoValue*) self, totype );
	if( value && (value->type == DAO_CDATA || value->type == DAO_CTYPE) ) return value->xCdata.data;
	return NULL;
}


DaoCtype* DaoCtype_New( DaoType *cttype, DaoType *cdtype )
{
	DaoCtype *self = (DaoCtype*)dao_calloc( 1, sizeof(DaoCtype) );
	DaoCdata_InitCommon( (DaoCdata*)self, cttype );
	GC_IncRC( cdtype );
	self->cdtype = cdtype;
	self->type = DAO_CTYPE;
	return self;
}
void DaoCtype_Delete( DaoCtype *self )
{
	DaoCdata_FreeCommon( (DaoCdata*) self );
	GC_DecRC( self->cdtype );
	dao_free( self );
}

DaoTypeBase defaultCdataTyper =
{
	"cdata", NULL, NULL, NULL, {0}, {0},
	(FuncPtrDel)DaoCdata_Delete, NULL
};
DaoCdata dao_default_cdata = {DAO_CDATA,0,DAO_VALUE_CONST,0,1,0,NULL,NULL,NULL,NULL};



/* In analog to Dao classes, two type objects are created for each cdata type:
 * one for the cdata type type, the other for the cdata object type.
 * Additionally, two dummy cdata objects are created:
 * one with typeid DAO_CTYPE serves an auxiliary value for the two type objects;
 * the other with typeid DAO_CDATA serves as the default value for the cdata object type. */
DaoType* DaoCdata_NewType( DaoTypeBase *typer )
{
	DaoCdata *cdata = DaoCdata_New( NULL, NULL );
	DaoCtype *ctype = DaoCtype_New( NULL, NULL );
	DaoType *cdata_type;
	DaoType *ctype_type;
	int i;

	ctype->subtype = DAO_CDATA_PTR;
	cdata->subtype = DAO_CDATA_PTR;
	ctype->trait |= DAO_VALUE_NOCOPY;
	cdata->trait |= DAO_VALUE_CONST|DAO_VALUE_NOCOPY;

	ctype_type = DaoType_New( typer->name, DAO_CTYPE, (DaoValue*)ctype, NULL );
	cdata_type = DaoType_New( typer->name, DAO_CDATA, (DaoValue*)ctype, NULL );
	GC_IncRC( cdata );
	cdata_type->value = (DaoValue*) cdata;
	GC_ShiftRC( cdata_type, ctype->cdtype );
	GC_ShiftRC( ctype_type, ctype->ctype );
	GC_ShiftRC( cdata_type, cdata->ctype );
	ctype->cdtype = cdata_type;
	ctype->ctype = ctype_type;
	cdata->ctype = cdata_type;
	ctype_type->typer = typer;
	cdata_type->typer = typer;

	for(i=0; i<DAO_MAX_CDATA_SUPER; i++){
		DaoTypeBase *sup = typer->supers[i];
		if( sup == NULL ) break;
		if( sup->core == NULL || sup->core->kernel->abtype == NULL ){
			printf( "parent type is not wrapped (successfully): %s\n", typer->name );
			return NULL;
		}
		if( ctype_type->bases == NULL ) ctype_type->bases = DArray_New(D_VALUE);
		if( cdata_type->bases == NULL ) cdata_type->bases = DArray_New(D_VALUE);
		DArray_Append( ctype_type->bases, sup->core->kernel->abtype->aux->xCdata.ctype );
		DArray_Append( cdata_type->bases, sup->core->kernel->abtype );
	}
	return cdata_type;
}
DaoType* DaoCdata_WrapType( DaoNamespace *nspace, DaoTypeBase *typer, int opaque )
{
	DaoTypeKernel *kernel = DaoTypeKernel_New( typer );
	DaoType *cdata_type = DaoCdata_NewType( typer );
	DaoType *ctype_type = cdata_type->aux->xCdata.ctype;

	GC_IncRC( nspace );
	GC_IncRC( cdata_type );
	kernel->nspace = nspace;
	kernel->abtype = cdata_type;
	cdata_type->cdatatype = opaque ? DAO_CDATA_CXX : DAO_CDATA_DAO;
	GC_ShiftRC( kernel, ctype_type->kernel );
	GC_ShiftRC( kernel, cdata_type->kernel );
	ctype_type->kernel = kernel;
	cdata_type->kernel = kernel;
	typer->core = kernel->core;
	return ctype_type;
}



static void DaoException_Init( DaoException *self, DaoType *type );

DaoException* DaoException_New( DaoType *type )
{
	DaoException *self = (DaoException*) dao_malloc( sizeof(DaoException) );
	DaoCdata_InitCommon( (DaoCdata*)self, type );
	self->fromLine = 0;
	self->toLine = 0;
	self->routine = NULL;
	self->callers = DArray_New(D_VALUE);
	self->lines = DArray_New(0);
	self->name = DString_New(1);
	self->info = DString_New(1);
	self->edata = NULL;
	DaoException_Init( self, type );
	return self;
}
DaoException* DaoException_New2( DaoType *type, DaoValue *v )
{
	DaoException *self = DaoException_New( type );
	DaoValue_Move( v, & self->edata, NULL );
	return self;
}
void DaoException_Delete( DaoException *self )
{
	DaoCdata_FreeCommon( (DaoCdata*)self );
	GC_DecRC( self->edata );
	DString_Delete( self->name );
	DString_Delete( self->info );
	DArray_Delete( self->callers );
	DArray_Delete( self->lines );
	dao_free( self );
}
void DaoException_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int remove )
{
	DaoException *self = (DaoException*) p;
	if( self->edata ) DArray_Append( values, self->edata );
	if( self->callers->size ) DArray_Append( arrays, self->callers );
	if( remove ) self->edata = NULL;
}

static void Dao_Exception_Get_name( DaoProcess *proc, DaoValue *p[], int n );
static void Dao_Exception_Set_name( DaoProcess *proc, DaoValue *p[], int n );
static void Dao_Exception_Get_info( DaoProcess *proc, DaoValue *p[], int n );
static void Dao_Exception_Set_info( DaoProcess *proc, DaoValue *p[], int n );
static void Dao_Exception_Get_data( DaoProcess *proc, DaoValue *p[], int n );
static void Dao_Exception_Set_data( DaoProcess *proc, DaoValue *p[], int n );
static void Dao_Exception_New( DaoProcess *proc, DaoValue *p[], int n );
static void Dao_Exception_New22( DaoProcess *proc, DaoValue *p[], int n );

static DaoFuncItem dao_Exception_Meths[] =
{
	{ Dao_Exception_Get_name, ".name( self : Exception )=>string" },
	{ Dao_Exception_Set_name, ".name=( self : Exception, name : string)" },
	{ Dao_Exception_Get_info, ".info( self : Exception )=>string" },
	{ Dao_Exception_Set_info, ".info=( self : Exception, info : string)" },
	{ Dao_Exception_Get_data, ".data( self : Exception )=>any" },
	{ Dao_Exception_Set_data, ".data=( self : Exception, data : any)" },
	{ Dao_Exception_New,   "Exception( info = '' )=>Exception" },
	{ Dao_Exception_New22, "Exception( data : any )=>Exception" },
	/* for testing or demonstration */
	{ Dao_Exception_Get_info, "serialize( self : Exception )=>string" },
	//XXX { Dao_Exception_Get_info, "cast( self : Exception, @T<string> )=>@T" },
	{ NULL, NULL }
};

DaoTypeBase dao_Exception_Typer =
{
	"Exception", NULL, NULL, dao_Exception_Meths, { 0 }, { 0 }, 
	(FuncPtrDel) DaoException_Delete, DaoException_GetGCFields
};

static void Dao_Exception_Get_name( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoException* self = (DaoException*) p[0];
	DaoProcess_PutString( proc, self->name );
}
static void Dao_Exception_Set_name( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoException* self = (DaoException*) p[0];
	DString *name = p[1]->xString.data;
	DString_Assign( self->name, name );
}
static void Dao_Exception_Get_info( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoException* self = (DaoException*) p[0];
	DaoProcess_PutString( proc, self->info );
}
static void Dao_Exception_Set_info( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoException* self = (DaoException*) p[0];
	DString_Assign( self->info, p[1]->xString.data );
}
static void Dao_Exception_Get_data( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoException* self = (DaoException*) p[0];
	DaoProcess_PutValue( proc, self->edata );
}
static void Dao_Exception_Set_data( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoException* self = (DaoException*) p[0];
	DaoValue_Move( p[1], & self->edata, NULL );
}
static void Dao_Exception_New( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoType *type = proc->topFrame->routine->routHost;
	DaoException *self = (DaoException*)DaoException_New( type );
	if( n ) DString_Assign( self->info, p[0]->xString.data );
	DaoProcess_PutValue( proc, (DaoValue*)self );
}
static void Dao_Exception_New22( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoType *type = proc->topFrame->routine->routHost;
	DaoException *self = (DaoException*)DaoException_New2( type, p[0] );
	DaoProcess_PutValue( proc, (DaoValue*)self );
}

static DaoFuncItem dao_ExceptionNone_Meths[] =
{
	{ Dao_Exception_New, "None( info = '' )=>None" },
	{ Dao_Exception_New22, "None( data : any )=>None" },
	{ NULL, NULL }
};
DaoTypeBase dao_ExceptionNone_Typer =
{
	"None", NULL, NULL, dao_ExceptionNone_Meths,
	{ & dao_Exception_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_ExceptionAny_Meths[] =
{
	{ Dao_Exception_New, "Any( info = '' )=>Any" },
	{ Dao_Exception_New22, "Any( data : any )=>Any" },
	{ NULL, NULL }
};
DaoTypeBase dao_ExceptionAny_Typer =
{
	"Any", NULL, NULL, dao_ExceptionAny_Meths,
	{ & dao_Exception_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_ExceptionWarning_Meths[] =
{
	{ Dao_Exception_New, "Warning( info = '' )=>Warning" },
	{ Dao_Exception_New22, "Warning( data : any )=>Warning" },
	{ NULL, NULL }
};
DaoTypeBase dao_ExceptionWarning_Typer =
{
	"Warning", NULL, NULL, dao_ExceptionWarning_Meths,
	{ & dao_Exception_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_ExceptionError_Meths[] =
{
	{ Dao_Exception_New, "Error( info = '' )=>Error" },
	{ Dao_Exception_New22, "Error( data : any )=>Error" },
	{ NULL, NULL }
};
DaoTypeBase dao_ExceptionError_Typer =
{
	"Error", NULL, NULL, dao_ExceptionError_Meths,
	{ & dao_Exception_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_ErrorField_Meths[] =
{
	{ Dao_Exception_New, "Field( info = '' )=>Field" },
	{ Dao_Exception_New22, "Field( data : any )=>Field" },
	{ NULL, NULL }
};
DaoTypeBase dao_ErrorField_Typer =
{
	"Field", NULL, NULL, dao_ErrorField_Meths,
	{ & dao_ExceptionError_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_NotExist_Meths[] =
{
	{ Dao_Exception_New, "NotExist( info = '' )=>NotExist" },
	{ Dao_Exception_New22, "NotExist( data : any )=>NotExist" },
	{ NULL, NULL }
};
DaoTypeBase dao_FieldNotExist_Typer =
{
	"NotExist", NULL, NULL, dao_NotExist_Meths,
	{ & dao_ErrorField_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_FieldNotPermit_Meths[] =
{
	{ Dao_Exception_New, "NotPermit( info = '' )=>NotPermit" },
	{ Dao_Exception_New22, "NotPermit( data : any )=>NotPermit" },
	{ NULL, NULL }
};
DaoTypeBase dao_FieldNotPermit_Typer =
{
	"NotPermit", NULL, NULL, dao_FieldNotPermit_Meths,
	{ & dao_ErrorField_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_ErrorFloat_Meths[] =
{
	{ Dao_Exception_New, "Float( info = '' )=>Float" },
	{ Dao_Exception_New22, "Float( data : any )=>Float" },
	{ NULL, NULL }
};
DaoTypeBase dao_ErrorFloat_Typer =
{
	"Float", NULL, NULL, dao_ErrorFloat_Meths,
	{ & dao_ExceptionError_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_FloatDivByZero_Meths[] =
{
	{ Dao_Exception_New, "DivByZero( info = '' )=>DivByZero" },
	{ Dao_Exception_New22, "DivByZero( data : any )=>DivByZero" },
	{ NULL, NULL }
};
DaoTypeBase dao_FloatDivByZero_Typer =
{
	"DivByZero", NULL, NULL, dao_FloatDivByZero_Meths,
	{ & dao_ErrorFloat_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_FloatOverFlow_Meths[] =
{
	{ Dao_Exception_New, "OverFlow( info = '' )=>OverFlow" },
	{ Dao_Exception_New22, "OverFlow( data : any )=>OverFlow" },
	{ NULL, NULL }
};
DaoTypeBase dao_FloatOverFlow_Typer =
{
	"OverFlow", NULL, NULL, dao_FloatOverFlow_Meths,
	{ & dao_ErrorFloat_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_FloatUnderFlow_Meths[] =
{
	{ Dao_Exception_New, "UnderFlow( info = '' )=>UnderFlow" },
	{ Dao_Exception_New22, "UnderFlow( data : any )=>UnderFlow" },
	{ NULL, NULL }
};
DaoTypeBase dao_FloatUnderFlow_Typer =
{
	"UnderFlow", NULL, NULL, dao_FloatUnderFlow_Meths,
	{ & dao_ErrorFloat_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_ErrorIndex_Meths[] =
{
	{ Dao_Exception_New, "Index( info = '' )=>Index" },
	{ Dao_Exception_New22, "Index( data : any )=>Index" },
	{ NULL, NULL }
};
DaoTypeBase dao_ErrorIndex_Typer =
{
	"Index", NULL, NULL, dao_ErrorIndex_Meths,
	{ & dao_ExceptionError_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_IndexOutOfRange_Meths[] =
{
	{ Dao_Exception_New, "OutOfRange( info = '' )=>OutOfRange" },
	{ Dao_Exception_New22, "OutOfRange( data : any )=>OutOfRange" },
	{ NULL, NULL }
};
DaoTypeBase dao_IndexOutOfRange_Typer =
{
	"OutOfRange", NULL, NULL, dao_IndexOutOfRange_Meths,
	{ & dao_ErrorIndex_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_ErrorKey_Meths[] =
{
	{ Dao_Exception_New, "Key( info = '' )=>Key" },
	{ Dao_Exception_New22, "Key( data : any )=>Key" },
	{ NULL, NULL }
};
DaoTypeBase dao_ErrorKey_Typer =
{
	"Key", NULL, NULL, dao_ErrorKey_Meths,
	{ & dao_ExceptionError_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

DaoTypeBase dao_KeyNotExist_Typer =
{
	"NotExist", NULL, NULL, dao_NotExist_Meths,
	{ & dao_ErrorKey_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_ErrorParam_Meths[] =
{
	{ Dao_Exception_New, "Param( info = '' )=>Param" },
	{ Dao_Exception_New22, "Param( data : any )=>Param" },
	{ NULL, NULL }
};
DaoTypeBase dao_ErrorParam_Typer =
{
	"Param", NULL, NULL, dao_ErrorParam_Meths,
	{ & dao_ExceptionError_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_Syntax_Meths[] =
{
	{ Dao_Exception_New, "Syntax( info = '' )=>Syntax" },
	{ Dao_Exception_New22, "Syntax( data : any )=>Syntax" },
	{ NULL, NULL }
};
DaoTypeBase dao_WarningSyntax_Typer =
{
	"Syntax", NULL, NULL, dao_Syntax_Meths,
	{ & dao_ExceptionWarning_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};
DaoTypeBase dao_ErrorSyntax_Typer =
{
	"Syntax", NULL, NULL, dao_Syntax_Meths,
	{ & dao_ExceptionError_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_ErrorType_Meths[] =
{
	{ Dao_Exception_New, "Type( info = '' )=>Type" },
	{ Dao_Exception_New22, "Type( data : any )=>Type" },
	{ NULL, NULL }
};
DaoTypeBase dao_ErrorType_Typer =
{
	"Type", NULL, NULL, dao_ErrorType_Meths,
	{ & dao_ExceptionError_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

static DaoFuncItem dao_Value_Meths[] =
{
	{ Dao_Exception_New, "Value( info = '' )=>Value" },
	{ Dao_Exception_New22, "Value( data : any )=>Value" },
	{ NULL, NULL }
};
DaoTypeBase dao_WarningValue_Typer =
{
	"Value", NULL, NULL, dao_Value_Meths,
	{ & dao_ExceptionWarning_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};
DaoTypeBase dao_ErrorValue_Typer =
{
	"Value", NULL, NULL, dao_Value_Meths,
	{ & dao_ExceptionError_Typer, NULL }, {0},
	(FuncPtrDel) DaoException_Delete, NULL
};

void DaoException_Setup( DaoNamespace *ns )
{
	DaoType *exception = DaoCdata_WrapType( ns, & dao_Exception_Typer, 0 );
	DaoType *none = DaoCdata_WrapType( ns, & dao_ExceptionNone_Typer, 0 );
	DaoType *any = DaoCdata_WrapType( ns, & dao_ExceptionAny_Typer, 0 );
	DaoType *warning = DaoCdata_WrapType( ns, & dao_ExceptionWarning_Typer, 0 );
	DaoType *error = DaoCdata_WrapType( ns, & dao_ExceptionError_Typer, 0 );
	DaoType *field = DaoCdata_WrapType( ns, & dao_ErrorField_Typer, 0 );
	DaoType *fdnotexist = DaoCdata_WrapType( ns, & dao_FieldNotExist_Typer, 0 );
	DaoType *fdnotperm = DaoCdata_WrapType( ns, & dao_FieldNotPermit_Typer, 0 );
	DaoType *tfloat = DaoCdata_WrapType( ns, & dao_ErrorFloat_Typer, 0 );
	DaoType *fltzero = DaoCdata_WrapType( ns, & dao_FloatDivByZero_Typer, 0 );
	DaoType *fltoflow = DaoCdata_WrapType( ns, & dao_FloatOverFlow_Typer, 0 );
	DaoType *fltuflow = DaoCdata_WrapType( ns, & dao_FloatUnderFlow_Typer, 0 );
	DaoType *index = DaoCdata_WrapType( ns, & dao_ErrorIndex_Typer, 0 );
	DaoType *idorange = DaoCdata_WrapType( ns, & dao_IndexOutOfRange_Typer, 0 );
	DaoType *key = DaoCdata_WrapType( ns, & dao_ErrorKey_Typer, 0 );
	DaoType *keynotexist = DaoCdata_WrapType( ns, & dao_KeyNotExist_Typer, 0 );
	DaoType *param = DaoCdata_WrapType( ns, & dao_ErrorParam_Typer, 0 );
	DaoType *wsyntax = DaoCdata_WrapType( ns, & dao_WarningSyntax_Typer, 0 );
	DaoType *esyntax = DaoCdata_WrapType( ns, & dao_ErrorSyntax_Typer, 0 );
	DaoType *wvalue = DaoCdata_WrapType( ns, & dao_WarningValue_Typer, 0 );
	DaoType *evalue = DaoCdata_WrapType( ns, & dao_ErrorValue_Typer, 0 );
	DaoType *type = DaoCdata_WrapType( ns, & dao_ErrorType_Typer, 0 );

	DaoNamespace_AddConst( ns, exception->name, exception->aux, DAO_DATA_PUBLIC );
	DaoNamespace_AddType( ns, exception->name, exception );

	DaoNamespace_SetupValues( ns, & dao_Exception_Typer );
	DaoNamespace_SetupValues( ns, & dao_ExceptionNone_Typer );
	DaoNamespace_SetupValues( ns, & dao_ExceptionAny_Typer );
	DaoNamespace_SetupValues( ns, & dao_ExceptionWarning_Typer );
	DaoNamespace_SetupValues( ns, & dao_ExceptionError_Typer );
	DaoNamespace_SetupValues( ns, & dao_ErrorField_Typer );
	DaoNamespace_SetupValues( ns, & dao_FieldNotExist_Typer );
	DaoNamespace_SetupValues( ns, & dao_FieldNotPermit_Typer );
	DaoNamespace_SetupValues( ns, & dao_ErrorFloat_Typer );
	DaoNamespace_SetupValues( ns, & dao_FloatDivByZero_Typer );
	DaoNamespace_SetupValues( ns, & dao_FloatOverFlow_Typer );
	DaoNamespace_SetupValues( ns, & dao_FloatUnderFlow_Typer );
	DaoNamespace_SetupValues( ns, & dao_ErrorIndex_Typer );
	DaoNamespace_SetupValues( ns, & dao_IndexOutOfRange_Typer );
	DaoNamespace_SetupValues( ns, & dao_ErrorKey_Typer );
	DaoNamespace_SetupValues( ns, & dao_KeyNotExist_Typer );
	DaoNamespace_SetupValues( ns, & dao_ErrorParam_Typer );
	DaoNamespace_SetupValues( ns, & dao_ErrorSyntax_Typer );
	DaoNamespace_SetupValues( ns, & dao_ErrorValue_Typer );
	DaoNamespace_SetupValues( ns, & dao_ErrorType_Typer );
	DaoNamespace_SetupValues( ns, & dao_WarningSyntax_Typer );
	DaoNamespace_SetupValues( ns, & dao_WarningValue_Typer );

	/* setup hierarchicy of exception types: */
	DMap_Insert( exception->kernel->values, none->name, none->aux );
	DMap_Insert( exception->kernel->values, any->name, any->aux );
	DMap_Insert( exception->kernel->values, warning->name, warning->aux );
	DMap_Insert( exception->kernel->values, error->name, error->aux );
	DMap_Insert( error->kernel->values, field->name, field->aux );
	DMap_Insert( field->kernel->values, fdnotexist->name, fdnotexist->aux );
	DMap_Insert( field->kernel->values, fdnotperm->name, fdnotperm->aux );
	DMap_Insert( error->kernel->values, tfloat->name, tfloat->aux );
	DMap_Insert( tfloat->kernel->values, fltzero->name, fltzero->aux );
	DMap_Insert( tfloat->kernel->values, fltoflow->name, fltoflow->aux );
	DMap_Insert( tfloat->kernel->values, fltuflow->name, fltuflow->aux );
	DMap_Insert( error->kernel->values, index->name, index->aux );
	DMap_Insert( index->kernel->values, idorange->name, idorange->aux );
	DMap_Insert( error->kernel->values, key->name, key->aux );
	DMap_Insert( key->kernel->values, keynotexist->name, keynotexist->aux );
	DMap_Insert( error->kernel->values, param->name, param->aux );
	DMap_Insert( error->kernel->values, esyntax->name, esyntax->aux );
	DMap_Insert( error->kernel->values, evalue->name, evalue->aux );
	DMap_Insert( error->kernel->values, type->name, type->aux );
	DMap_Insert( warning->kernel->values, wsyntax->name, wsyntax->aux );
	DMap_Insert( warning->kernel->values, wvalue->name, wvalue->aux );

	DaoNamespace_SetupMethods( ns, & dao_Exception_Typer );
	DaoNamespace_SetupMethods( ns, & dao_ExceptionNone_Typer );
	DaoNamespace_SetupMethods( ns, & dao_ExceptionAny_Typer );
	DaoNamespace_SetupMethods( ns, & dao_ExceptionWarning_Typer );
	DaoNamespace_SetupMethods( ns, & dao_ExceptionError_Typer );
	DaoNamespace_SetupMethods( ns, & dao_ErrorField_Typer );
	DaoNamespace_SetupMethods( ns, & dao_FieldNotExist_Typer );
	DaoNamespace_SetupMethods( ns, & dao_FieldNotPermit_Typer );
	DaoNamespace_SetupMethods( ns, & dao_ErrorFloat_Typer );
	DaoNamespace_SetupMethods( ns, & dao_FloatDivByZero_Typer );
	DaoNamespace_SetupMethods( ns, & dao_FloatOverFlow_Typer );
	DaoNamespace_SetupMethods( ns, & dao_FloatUnderFlow_Typer );
	DaoNamespace_SetupMethods( ns, & dao_ErrorIndex_Typer );
	DaoNamespace_SetupMethods( ns, & dao_IndexOutOfRange_Typer );
	DaoNamespace_SetupMethods( ns, & dao_ErrorKey_Typer );
	DaoNamespace_SetupMethods( ns, & dao_KeyNotExist_Typer );
	DaoNamespace_SetupMethods( ns, & dao_ErrorParam_Typer );
	DaoNamespace_SetupMethods( ns, & dao_ErrorSyntax_Typer );
	DaoNamespace_SetupMethods( ns, & dao_ErrorValue_Typer );
	DaoNamespace_SetupMethods( ns, & dao_ErrorType_Typer );
	DaoNamespace_SetupMethods( ns, & dao_WarningSyntax_Typer );
	DaoNamespace_SetupMethods( ns, & dao_WarningValue_Typer );
}
DaoType* DaoException_GetType( int type )
{
	switch( type ){
	case DAO_EXCEPTION : return dao_Exception_Typer.core->kernel->abtype;
	case DAO_EXCEPTION_NONE : return dao_ExceptionNone_Typer.core->kernel->abtype;
	case DAO_EXCEPTION_ANY : return dao_ExceptionAny_Typer.core->kernel->abtype;
	case DAO_WARNING : return dao_ExceptionWarning_Typer.core->kernel->abtype;
	case DAO_ERROR : return dao_ExceptionError_Typer.core->kernel->abtype;
	case DAO_ERROR_FIELD : return dao_ErrorField_Typer.core->kernel->abtype;
	case DAO_ERROR_FIELD_NOTEXIST : return dao_FieldNotExist_Typer.core->kernel->abtype;
	case DAO_ERROR_FIELD_NOTPERMIT : return dao_FieldNotPermit_Typer.core->kernel->abtype;
	case DAO_ERROR_FLOAT : return dao_ErrorFloat_Typer.core->kernel->abtype;
	case DAO_ERROR_FLOAT_DIVBYZERO : return dao_FloatDivByZero_Typer.core->kernel->abtype;
	case DAO_ERROR_FLOAT_OVERFLOW : return dao_FloatOverFlow_Typer.core->kernel->abtype;
	case DAO_ERROR_FLOAT_UNDERFLOW : return dao_FloatUnderFlow_Typer.core->kernel->abtype;
	case DAO_ERROR_INDEX : return dao_ErrorIndex_Typer.core->kernel->abtype;
	case DAO_ERROR_INDEX_OUTOFRANGE : return dao_IndexOutOfRange_Typer.core->kernel->abtype;
	case DAO_ERROR_KEY : return dao_ErrorKey_Typer.core->kernel->abtype;
	case DAO_ERROR_KEY_NOTEXIST : return dao_KeyNotExist_Typer.core->kernel->abtype;
	case DAO_ERROR_PARAM : return dao_ErrorParam_Typer.core->kernel->abtype;
	case DAO_ERROR_SYNTAX : return dao_ErrorSyntax_Typer.core->kernel->abtype;
	case DAO_ERROR_TYPE : return dao_ErrorType_Typer.core->kernel->abtype;
	case DAO_ERROR_VALUE : return dao_ErrorValue_Typer.core->kernel->abtype;
	case DAO_WARNING_SYNTAX : return dao_WarningSyntax_Typer.core->kernel->abtype;
	case DAO_WARNING_VALUE : return dao_WarningValue_Typer.core->kernel->abtype;
	default : break;
	}
	return dao_Exception_Typer.core->kernel->abtype;
}
void DaoException_Init( DaoException *self, DaoType *type )
{
	int i;
	for(i=DAO_EXCEPTION; i<ENDOF_BASIC_EXCEPT; i++){
		if( type == DaoException_GetType( i ) ){
			DString_SetMBS( self->name, daoExceptionName[i] );
			DString_SetMBS( self->info, daoExceptionInfo[i] );
			return;
		}
	}
}

