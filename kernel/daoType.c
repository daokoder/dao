/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2011, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

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

#include"daoType.h"
#include"daoVmspace.h"
#include"daoNamespace.h"
#include"daoNumtype.h"
#include"daoStream.h"
#include"daoRoutine.h"
#include"daoObject.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoStdlib.h"
#include"daoClass.h"
#include"daoParser.h"
#include"daoMacro.h"
#include"daoRegex.h"

DMap *dao_typing_cache; /* HASH<void*[2],int> */
size_t dao_typing_version;
#ifdef DAO_WITH_THREAD
DMutex dao_typing_mutex;
#endif

void DaoType_Delete( DaoType *self )
{
	DValue_Clear( & self->aux );
	GC_DecRCs( self->nested );
	DString_Delete( self->name );
	if( self->fname ) DString_Delete( self->fname );
	if( self->nested ) DArray_Delete( self->nested );
	if( self->mapNames ) DMap_Delete( self->mapNames );
	if( self->interfaces ) DMap_Delete( self->interfaces );
	DaoLateDeleter_Push( self );
}
extern DEnum* DaoContext_GetEnum( DaoContext *self, DaoVmCode *vmc );
static void DaoType_GetField( DValue *self0, DaoContext *ctx, DString *name )
{
	DaoType *self = (DaoType*) self0->v.p;
	DEnum *denum = DaoContext_GetEnum( ctx, ctx->vmc );
	DNode *node;
	if( self->mapNames == NULL ) goto ErrorNotExist;
	node = DMap_Find( self->mapNames, name );
	if( node == NULL ) goto ErrorNotExist;
	GC_ShiftRC( self, denum->type );
	denum->type = self;
	denum->value = node->value.pInt;
	return;
ErrorNotExist:
	DaoContext_RaiseException( ctx, DAO_ERROR_FIELD_NOTEXIST, DString_GetMBS( name ) );
}
static void DaoType_GetItem( DValue *self0, DaoContext *ctx, DValue *ids[], int N )
{
	DaoType *self = (DaoType*) self0->v.p;
	DEnum *denum = DaoContext_GetEnum( ctx, ctx->vmc );
	DNode *node;
	if( self->mapNames == NULL || N != 1 || ids[0]->t != DAO_INTEGER ) goto ErrorNotExist;
	for(node=DMap_First(self->mapNames);node;node=DMap_Next(self->mapNames,node)){
		if( node->value.pInt == ids[0]->v.i ){
			GC_ShiftRC( self, denum->type );
			denum->type = self;
			denum->value = node->value.pInt;
			return;
		}
	}
ErrorNotExist:
	DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "not valid" );
}
static DaoTypeCore typeCore=
{
	0, NULL, NULL, NULL, NULL,
	DaoType_GetField,
	DaoBase_SetField,
	DaoType_GetItem,
	DaoBase_SetItem,
	DaoBase_Print,
	DaoBase_Copy
};
DaoTypeBase abstypeTyper=
{
	"type", & typeCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoType_Delete, NULL
};

void DaoType_MapNames( DaoType *self );
void DaoType_CheckAttributes( DaoType *self )
{
	if( DString_FindChar( self->name, '?', 0 ) != MAXSIZE
			|| DString_FindChar( self->name, '@', 0 ) != MAXSIZE )
		self->attrib |= DAO_TYPE_NOTDEF;
	else
		self->attrib &= ~DAO_TYPE_NOTDEF;

	if( self->tid == DAO_INTERFACE )
		self->attrib |= DAO_TYPE_INTER;
	else
		self->attrib &= ~DAO_TYPE_INTER;
}
DaoType* DaoType_New( const char *name, short tid, DaoBase *extra, DArray *nest )
{
	DaoType *self = (DaoType*) dao_calloc( 1, sizeof(DaoType) );
	DaoBase_Init( self, DAO_TYPE );
	self->tid = tid;
	self->typer = (DaoTypeBase*) DaoVmSpace_GetTyper( tid );
	self->name = DString_New(1);
	if( extra ){
		self->aux.t = extra->type;
		self->aux.v.p = extra;
		GC_IncRC( extra );
	}
	if( tid == DAO_OBJECT || tid == DAO_CTYPE ) self->interfaces = DHash_New(0,0);
	DString_SetMBS( self->name, name );
	if( (tid == DAO_PAR_NAMED || tid == DAO_PAR_DEFAULT) && extra && extra->type == DAO_TYPE ){
		self->fname = DString_New(1);
		DString_SetMBS( self->fname, name );
		DString_AppendChar( self->name, (tid == DAO_PAR_NAMED) ? ':' : '=' );
		DString_Append( self->name, self->aux.v.type->name );
	}
	if( nest ){
		self->nested = DArray_New(0);
		DArray_Assign( self->nested, nest );
		GC_IncRCs( self->nested );
	}else if( tid == DAO_ROUTINE || tid == DAO_TUPLE ){
		self->nested = DArray_New(0);
	}
	if( tid == DAO_ROUTINE || tid == DAO_TUPLE ) DaoType_MapNames( self );
	DaoType_CheckAttributes( self );
	DaoType_InitDefault( self );
#if 0
	if( strstr( self->name->mbs, "map<" ) ){
		printf( "%s  %p\n", self->name->mbs, self );
		print_trace();
	}
#endif
	return self;
}
void DaoType_InitDefault( DaoType *self )
{
	DaoType **types = self->nested ? self->nested->items.pType : NULL;
	DaoType *itype1 = types && self->nested->size > 0 ? types[0] : NULL;
	DaoType *itype2 = types && self->nested->size > 1 ? types[1] : NULL;
	int i, count = self->nested ? self->nested->size : 0;
	if( self->value.t && self->value.t != DAO_TUPLE ) return;
	if( self->value.t == DAO_TUPLE && self->value.v.tuple->items->size == count ) return;
	DValue_Clear( & self->value );
	if( self->tid <= DAO_STRING ){
		DValue_Init( & self->value, self->tid );
	}else if( self->tid == DAO_ENUM ){
		self->value = daoNullEnum;
		self->value.v.e = DEnum_New( self, 0 );
	}else if( self->tid == DAO_ARRAY ){
#ifdef DAO_WITH_NUMARRAY
		self->value = daoNullArray;
		self->value.v.array = DaoArray_New( itype1 ? itype1->tid : DAO_INTEGER );
		self->value.v.array->unitype = self;
		GC_IncRC( self );
#endif
	}else if( self->tid == DAO_LIST ){
		self->value = daoNullList;
		self->value.v.list = DaoList_New();
		self->value.v.list->unitype = self;
		GC_IncRC( self );
	}else if( self->tid == DAO_MAP ){
		self->value = daoNullMap;
		self->value.v.map = DaoMap_New(0);
		self->value.v.map->unitype = self;
		GC_IncRC( self );
	}else if( self->tid == DAO_TUPLE ){
		self->value = daoNullTuple;
		self->value.v.tuple = DaoTuple_New( count );
		self->value.v.tuple->unitype = self;
		GC_IncRC( self );
		for(i=0; i<count; i++){
			DaoType_InitDefault( types[i] );
			DValue_Copy( & self->value.v.tuple->items->data[i], types[i]->value );
		}
	}else if( self->tid == DAO_ANY ){
		DValue_Init( & self->value, 0 );
		DValue_MarkConst( & self->value );
	}else if( self->tid == DAO_VALTYPE ){
		DValue_Copy( & self->value, self->aux );
	}else if( self->tid == DAO_VARIANT ){
		for(i=0; i<count; i++) DaoType_InitDefault( types[i] );
		if( count ) DValue_Copy( & self->value, types[0]->value );
	}else if( self->tid == DAO_ROUTINE || self->tid == DAO_INTERFACE ){
		self->value.cst = 1;
	}
	if( self->value.t ) DValue_MarkConst( & self->value );
	if( self->value.t >= DAO_ARRAY ) GC_IncRC( self->value.v.p );
}
DaoType* DaoType_Copy( DaoType *other )
{
	DNode *it;
	DaoType *self = (DaoType*) dao_malloc( sizeof(DaoType) );
	memcpy( self, other, sizeof(DaoType) );
	DaoBase_Init( self, DAO_TYPE ); /* to reset gc fields */
	self->name = DString_Copy( other->name );
	self->nested = NULL;
	if( other->fname ) self->fname = DString_Copy( other->fname );
	if( other->nested ){
		self->nested = DArray_Copy( other->nested );
		GC_IncRCs( self->nested );
	}
	if( other->mapNames ) self->mapNames = DMap_Copy( other->mapNames );
	if( other->interfaces ){
		self->interfaces = DMap_Copy( other->interfaces );
		it = DMap_First( other->interfaces );
		for(; it!=NULL; it=DMap_Next(other->interfaces,it)) GC_IncRC( it->key.pBase );
	}
	self->aux = daoNullValue;
	self->value = daoNullValue;
	DValue_Copy( & self->aux, other->aux );
	DValue_Copy( & self->value, other->value );
	return self;
}
void DaoType_MapNames( DaoType *self )
{
	DaoType *tp;
	int i, j, k = 0;
	if( self->nested == NULL ) return;
	if( self->tid != DAO_TUPLE && self->tid != DAO_ROUTINE ) return;
	if( self->mapNames == NULL ) self->mapNames = DMap_New(D_STRING,0);
	for(i=0; i<self->nested->size; i++){
		tp = self->nested->items.pType[i];
		if( tp->fname ) MAP_Insert( self->mapNames, tp->fname, i );
	}
}
DaoType* DaoType_GetFromTypeStructure( DaoTypeBase *typer )
{
	if( typer->priv == NULL ) return NULL;
	return typer->priv->abtype;
}

#define MIN(x,y) (x>y?y:x)

extern int DaoCData_ChildOf( DaoTypeBase *self, DaoTypeBase *super );

static unsigned char dao_type_matrix[END_EXTRA_TYPES][END_EXTRA_TYPES];

void DaoType_Init()
{
	int i, j;
	dao_typing_cache = DHash_New(D_VOID2,0);
	dao_typing_version = 0;
	memset( dao_type_matrix, DAO_MT_NOT, END_EXTRA_TYPES*END_EXTRA_TYPES );
	for(i=DAO_INTEGER; i<=DAO_DOUBLE; i++){
		dao_type_matrix[DAO_ENUM][i] = DAO_MT_SUB;
		dao_type_matrix[i][DAO_COMPLEX] = DAO_MT_SUB;
		for(j=DAO_INTEGER; j<=DAO_DOUBLE; j++)
			dao_type_matrix[i][j] = DAO_MT_SIM;
	}
	dao_type_matrix[DAO_ENUM][DAO_STRING] = DAO_MT_SUB;
	for(i=0; i<END_EXTRA_TYPES; i++) dao_type_matrix[i][i] = DAO_MT_EQ;
	for(i=0; i<END_EXTRA_TYPES; i++){
		dao_type_matrix[i][DAO_PAR_NAMED] = DAO_MT_EQ+2;
		dao_type_matrix[i][DAO_PAR_DEFAULT] = DAO_MT_EQ+2;
		dao_type_matrix[DAO_PAR_NAMED][i] = DAO_MT_EQ+2;
		dao_type_matrix[DAO_PAR_DEFAULT][i] = DAO_MT_EQ+2;

		dao_type_matrix[DAO_VALTYPE][i] = DAO_MT_EQ+1;
		dao_type_matrix[i][DAO_VALTYPE] = DAO_MT_EQ+1;
		dao_type_matrix[DAO_VARIANT][i] = DAO_MT_EQ+1;
		dao_type_matrix[i][DAO_VARIANT] = DAO_MT_EQ+1;
	}
	dao_type_matrix[DAO_VALTYPE][DAO_VALTYPE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_VARIANT][DAO_VARIANT] = DAO_MT_EQ+1;

	for(i=0; i<END_EXTRA_TYPES; i++){
		dao_type_matrix[DAO_UDF][i] = DAO_MT_UDF;
		dao_type_matrix[i][DAO_UDF] = DAO_MT_UDF;
		dao_type_matrix[i][DAO_ANY] = DAO_MT_ANY;
		dao_type_matrix[DAO_INITYPE][i] = DAO_MT_INIT;
		dao_type_matrix[i][DAO_INITYPE] = DAO_MT_INIT;
	}

	dao_type_matrix[DAO_UDF][DAO_ANY] = DAO_MT_ANYUDF;
	dao_type_matrix[DAO_ANY][DAO_UDF] = DAO_MT_ANYUDF;
	dao_type_matrix[DAO_INITYPE][DAO_ANY] = DAO_MT_ANYUDF;
	dao_type_matrix[DAO_ANY][DAO_INITYPE] = DAO_MT_ANYUDF;
	dao_type_matrix[DAO_UDF][DAO_INITYPE] = DAO_MT_UDF;
	dao_type_matrix[DAO_INITYPE][DAO_UDF] = DAO_MT_UDF;

	dao_type_matrix[DAO_LIST_EMPTY][DAO_LIST] = DAO_MT_EQ;
	dao_type_matrix[DAO_ARRAY_EMPTY][DAO_ARRAY] = DAO_MT_EQ;
	dao_type_matrix[DAO_MAP_EMPTY][DAO_MAP] = DAO_MT_EQ;

	dao_type_matrix[DAO_ENUM][DAO_ENUM] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_TYPE][DAO_TYPE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_ARRAY][DAO_ARRAY] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_LIST][DAO_LIST] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_MAP][DAO_MAP] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_TUPLE][DAO_TUPLE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_LIST][DAO_LIST_ANY] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_ARRAY][DAO_ARRAY_ANY] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_MAP][DAO_MAP_ANY] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_FUTURE][DAO_FUTURE] = DAO_MT_EQ+1;

	dao_type_matrix[DAO_CLASS][DAO_CLASS] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_CLASS][DAO_CTYPE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_CLASS][DAO_INTERFACE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_OBJECT][DAO_CDATA] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_OBJECT][DAO_OBJECT] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_OBJECT][DAO_INTERFACE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_CTYPE][DAO_CTYPE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_CTYPE][DAO_INTERFACE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_CDATA][DAO_CTYPE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_CDATA][DAO_CDATA] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_CDATA][DAO_INTERFACE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_ROUTINE][DAO_ROUTINE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_ROUTINE][DAO_FUNCTION] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_FUNCTION][DAO_ROUTINE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_FUNCTION][DAO_FUNCTION] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_VMPROCESS][DAO_ROUTINE] = DAO_MT_EQ+1;
}
static short DaoType_Match( DaoType *self, DaoType *type, DMap *defs, DMap *binds );
static int DaoInterface_TryBindTo( DaoInterface *self, DaoType *type, DMap *binds, DArray *fails );

static short DaoType_MatchPar( DaoType *self, DaoType *type, DMap *defs, DMap *binds, int host )
{
	DaoType *ext1 = self;
	DaoType *ext2 = type;
	int p1 = self->tid == DAO_PAR_NAMED || self->tid == DAO_PAR_DEFAULT;
	int p2 = type->tid == DAO_PAR_NAMED || type->tid == DAO_PAR_DEFAULT;
	int m = 0;
	if( p1 && p2 && ! DString_EQ( self->fname, type->fname ) ) return DAO_MT_NOT;
	if( p1 ) ext1 = self->aux.v.type;
	if( p2 ) ext2 = type->aux.v.type;

	m = DaoType_Match( ext1, ext2, defs, binds );
	/*
	   printf( "m = %i:  %s  %s\n", m, ext1->name->mbs, ext2->name->mbs );
	 */
	if( host == DAO_TUPLE && m == DAO_MT_EQ ){
		if( self->tid != DAO_PAR_NAMED && type->tid == DAO_PAR_NAMED ) return DAO_MT_SUB;
	}else if( host == DAO_ROUTINE ){
		if( self->tid != DAO_PAR_DEFAULT && type->tid == DAO_PAR_DEFAULT ) return 0;
		return m;
	}
	return m;
}
short DaoType_MatchToX( DaoType *self, DaoType *type, DMap *defs, DMap *binds )
{
	DMap *inters;
	DaoType *it1, *it2;
	DNode *it, *node = NULL;
	short i, k, mt2, mt = DAO_MT_NOT;
	if( self ==NULL || type ==NULL ) return DAO_MT_NOT;
	if( self == type ) return DAO_MT_EQ;
	mt = dao_type_matrix[self->tid][type->tid];
	/*
	printf( "here: %i  %i  %i, %s  %s,  %p\n", mt, self->tid, type->tid, 
			self->name->mbs, type->name->mbs, defs );
	 */
	if( mt == DAO_MT_INIT ){
		if( self && self->tid == DAO_INITYPE ){
			if( defs ) node = MAP_Find( defs, self );
			if( node ) self = node->value.pType;
		}
		if( type && type->tid == DAO_INITYPE ){
			if( defs ) node = MAP_Find( defs, type );
			if( node == NULL ){
				if( defs ) MAP_Insert( defs, type, self );
				if( self == NULL || self->tid==DAO_ANY || self->tid==DAO_UDF )
					return DAO_MT_ANYUDF;
				return DAO_MT_INIT; /* even if self==NULL, for typing checking for undefined @X */
			}
			type = node->value.pType;
			mt = DAO_MT_INIT;
			if( type == NULL || type->tid==DAO_ANY || type->tid==DAO_UDF )
				return DAO_MT_ANYUDF;
		}
	}else if( mt == DAO_MT_UDF ){
		if( self->tid == DAO_UDF ){
			if( defs && type->tid != DAO_UDF ) MAP_Insert( defs, self, type );
			if( type->tid==DAO_ANY || type->tid==DAO_UDF ) return DAO_MT_ANYUDF;
		}else{
			if( defs && self->tid != DAO_UDF ) MAP_Insert( defs, type, self );
			if( self->tid==DAO_ANY || self->tid==DAO_UDF ) return DAO_MT_ANYUDF;
		}
		return mt;
	}else if( mt == DAO_MT_ANYUDF ){
		if( self->tid == DAO_ANY && (type->tid == DAO_UDF || type->tid == DAO_INITYPE) ){
			if( defs ) MAP_Insert( defs, type, self );
		}
		return mt;
	}
	mt = dao_type_matrix[self->tid][type->tid];
	if( mt <= DAO_MT_EQ ) return mt;
	if( mt == DAO_MT_EQ+2 ) return DaoType_MatchPar( self, type, defs, binds, 0 );

	if( type->tid == DAO_VARIANT ){
		mt = DAO_MT_NOT;
		for(i=0; i<type->nested->size; i++){
			it2 = type->nested->items.pType[i];
			mt2 = DaoType_MatchTo( self, it2, defs );
			if( mt2 > mt ) mt = mt2;
		}
		if( mt && defs && type->aux.t == DAO_TYPE )
			MAP_Insert( defs, type->aux.v.type, self );
		return mt;
	}else if( type->tid == DAO_VALTYPE ){
		if( self->tid != DAO_VALTYPE ) return DaoType_MatchValue( self, type->aux, defs );
		if( DValue_Compare( self->aux, type->aux ) ==0 ) return DAO_MT_EQ + 1;
		return DAO_MT_NOT;
	}
	switch( self->tid ){
	case DAO_ENUM :
		if( DString_EQ( self->name, type->name ) ) return DAO_MT_EQ;
		if( self->flagtype && type->flagtype ==0 ) return 0;
		if( self->mapNames ==NULL && self->mapNames ==NULL ) return DAO_MT_SUB;
		if( self->mapNames ==NULL || self->mapNames ==NULL ) return DAO_MT_SUB;
		for(it=DMap_First(self->mapNames); it; it=DMap_Next(self->mapNames, it )){
			node = DMap_Find( type->mapNames, it->key.pVoid );
			if( node ==NULL ) return 0;
			/* if( node->value.pInt != it->value.pInt ) return 0; */
		}
		return DAO_MT_EQ;
	case DAO_ARRAY : case DAO_LIST : case DAO_MAP : case DAO_TUPLE : 
	case DAO_TYPE : case DAO_FUTURE :
		/* tuple<...> to tuple */
		if( self->tid == DAO_TUPLE && type->nested->size ==0 ) return DAO_MT_SUB;
		if( self->attrib & DAO_TYPE_EMPTY ) return DAO_MT_SUB;
		if( self->nested->size != type->nested->size ) return DAO_MT_NOT;
		for(i=0; i<self->nested->size; i++){
			it1 = self->nested->items.pType[i];
			it2 = type->nested->items.pType[i];
			k = DaoType_MatchPar( it1, it2, defs, binds, type->tid );
			if( k == DAO_MT_NOT ) return k;
			if( k < mt ) mt = k;
		}
		break;
	case DAO_ROUTINE :
		if( self->name->mbs[0] != type->name->mbs[0] ) return 0; /* @routine */
		if( self->nested->size < type->nested->size ) return DAO_MT_NOT;
		if( self->aux.v.p == NULL && type->aux.v.p ) return 0;
		/* self may have extra parameters, but they must have default values: */
		for(i=type->nested->size; i<self->nested->size; i++){
			it1 = self->nested->items.pType[i];
			if( it1->tid != DAO_PAR_DEFAULT ) return 0;
		}
		for(i=0; i<type->nested->size; i++){
			it1 = self->nested->items.pType[i];
			it2 = type->nested->items.pType[i];
			k = DaoType_MatchPar( it1, it2, defs, binds, DAO_ROUTINE );
			/*
			   printf( "%2i  %2i:  %s  %s\n", i, k, it1->name->mbs, it2->name->mbs );
			 */
			if( k == DAO_MT_NOT ) return k;
			if( k < mt ) mt = k;
		}
		if( self->aux.v.p && type->aux.v.p ){
			k = DaoType_Match( self->aux.v.type, type->aux.v.type, defs, binds );
			if( k < mt ) mt = k;
		}
		break;
	case DAO_CLASS :
	case DAO_OBJECT :
		/* par : class */
		if( type->aux.v.p == NULL && self->tid == DAO_CLASS ) return DAO_MT_SUB;
		if( self->aux.v.p == type->aux.v.p ) return DAO_MT_EQ;
		it1 = self->aux.v.klass->objType;
		if( type->tid == DAO_INTERFACE ){
			if( DaoType_HasInterface( it1, type->aux.v.inter ) ) return DAO_MT_SUB;
			if( DaoInterface_TryBindTo( type->aux.v.inter, it1, binds, NULL ) ) return DAO_MT_SUB;
		}
		if( DaoClass_ChildOf( self->aux.v.klass, type->aux.v.p ) ) return DAO_MT_SUB;
		return DAO_MT_NOT;
		break;
	case DAO_CTYPE :
	case DAO_CDATA :
		if( self->typer == type->typer ){
			return DAO_MT_EQ;
		}else if( DaoCData_ChildOf( self->typer, type->typer ) ){
			return DAO_MT_SUB;
		}else if( type->tid == DAO_INTERFACE ){
			if( DaoType_HasInterface( self, type->aux.v.inter ) ) return DAO_MT_SUB;
			if( DaoInterface_TryBindTo( type->aux.v.inter, self, binds, NULL ) ) return DAO_MT_SUB;
		}else{
			return DAO_MT_NOT;
		}
		break;
	case DAO_VALTYPE :
		if( type->tid != DAO_VALTYPE ) return DaoType_MatchValue( type, self->aux, defs );
		if( DValue_Compare( self->aux, type->aux ) ==0 ) return DAO_MT_EQ + 1;
		return DAO_MT_NOT;
	case DAO_VARIANT :
		mt = DAO_MT_NOT;
		for(i=0; i<self->nested->size; i++){
			it1 = self->nested->items.pType[i];
			mt2 = DaoType_MatchTo( it1, type, defs );
			if( mt2 > mt ) mt = mt2;
		}
		return mt;
		break;
	default : break;
	}
	if( mt > DAO_MT_EQ ) mt = DAO_MT_NOT;
	return mt;
}
short DaoType_Match( DaoType *self, DaoType *type, DMap *defs, DMap *binds )
{
	DNode *node;
	void *pvoid[2];
	size_t mt;

	pvoid[0] = self;
	pvoid[1] = type;
	if( dao_late_deleter.safe ==0 || dao_late_deleter.version != dao_typing_version ){
		if( dao_typing_cache->size ){
#ifdef DAO_WITH_THREAD
			DMutex_Lock( & dao_typing_mutex );
#endif
			if( dao_typing_cache->size ) DMap_Clear( dao_typing_cache );
#ifdef DAO_WITH_THREAD
			DMutex_Unlock( & dao_typing_mutex );
#endif
		}
		mt = DaoType_MatchToX( self, type, defs, binds );
		if( mt ==0 && binds && DMap_Find( binds, pvoid ) ) return DAO_MT_INIT;
		return mt;
	}

	if( self ==NULL || type ==NULL ) return DAO_MT_NOT;
	if( self == type ) return DAO_MT_EQ;
	node = DMap_Find( dao_typing_cache, pvoid );
	if( node ) return node->value.pInt;

	mt = DaoType_MatchToX( self, type, defs, binds );
	if( ((self->attrib|type->attrib) & (DAO_TYPE_NOTDEF|DAO_TYPE_INTER)) ==0 ){
#ifdef DAO_WITH_THREAD
		DMutex_Lock( & dao_typing_mutex );
#endif
		dao_typing_version = dao_late_deleter.version;
		MAP_Insert( dao_typing_cache, pvoid, mt );
#ifdef DAO_WITH_THREAD
		DMutex_Unlock( & dao_typing_mutex );
#endif
	}
#if 0
	if( mt ==0 && binds ){
		printf( "%p  %p\n", pvoid[0], pvoid[1] );
		printf( "%i %p\n", binds->size, DMap_Find( binds, pvoid ) );
	}
#endif
	if( mt ==0 && binds && DMap_Find( binds, pvoid ) ) return DAO_MT_INIT;
	return mt;
}
short DaoType_MatchTo( DaoType *self, DaoType *type, DMap *defs )
{
	return DaoType_Match( self, type, defs, NULL );
}
short DaoType_MatchValue( DaoType *self, DValue value, DMap *defs )
{
	ullong_t flags = (1<<DAO_UDF)|(1<<DAO_ANY)|(1<<DAO_INITYPE);
	DaoType *tp;
	DEnum *other;
	DNode *node;
	DMap *inters;
	DMap *names;
	short i, mt, mt2, it1=0, it2=0;
	if( self == NULL ) return DAO_MT_NOT;
	mt = dao_type_matrix[value.t][self->tid];
	if( value.t == 0 || self->tid == DAO_VALTYPE || self->tid == DAO_VARIANT ){
		if( self->tid == DAO_VALTYPE ){
			if( DValue_Compare( self->aux, value ) ==0 ) return DAO_MT_EQ + 1;
		}else if( self->tid == DAO_VARIANT ){
			mt = DAO_MT_NOT;
			for(i=0; i<self->nested->size; i++){
				tp = self->nested->items.pType[i];
				mt2 = DaoType_MatchValue( tp, value, defs );
				if( mt2 > mt ) mt = mt2;
			}
			return mt;
		}else if( self->tid == DAO_ANY || self->tid == DAO_INITYPE ){
			return DAO_MT_ANY;
		}
		return DAO_MT_NOT;
	}
	switch( mt ){
	case DAO_MT_NOT : case DAO_MT_ANYUDF : case DAO_MT_ANY : case DAO_MT_EQ :
	case DAO_MT_UDF : case DAO_MT_INIT : case DAO_MT_SIM :
		/* TODO : two rounds type inferring? */
		if( defs ){
			DNode *node = NULL;
			if( defs ) node = MAP_Find( defs, self );
			if( node ) return DaoType_MatchValue( node->value.pType, value, defs );
		}
		return mt;
	case DAO_MT_SUB :
		if( value.t < DAO_ARRAY ) return mt;
	default : break;
	}
	if( self->nested ){
		if( self->nested->size ) it1 = self->nested->items.pType[0]->tid;
		if( self->nested->size >1 ) it2 = self->nested->items.pType[1]->tid;
	}
	switch( value.t ){
	case DAO_ENUM :
		if( value.v.e->type == self ) return DAO_MT_EQ;
		other = value.v.e;
		names = other->type->mapNames;
		for(node=DMap_First(names); node; node=DMap_Next(names,node)){
			if( other->type->flagtype ){
				if( !(node->value.pInt & other->value) ) continue;
			}else if( node->value.pInt != other->value ){
				continue;
			}
			if( DMap_Find( self->mapNames, node->key.pVoid ) == NULL ) return 0;
		}
		return DAO_MT_EQ;
		break;
	case DAO_ARRAY :
		if( value.v.array->size == 0 ) return DAO_MT_EQ;
		tp = value.v.array->unitype;
		if( tp == self ) return DAO_MT_EQ;
		if( it1 == DAO_UDF ) return DAO_MT_UDF;
		if( it1 == DAO_ANY ) return DAO_MT_ANY;
		if( it1 == DAO_INITYPE ) return DAO_MT_INIT;
		if( value.v.array->numType == it1 ) return DAO_MT_EQ;
		/* return DAO_MT_EQ for exact match, or zero otherwise: */
		if( tp ) return (DaoType_MatchTo( tp, self, defs ) == DAO_MT_EQ) * DAO_MT_EQ;
		break;
	case DAO_LIST :
		if( value.v.list->items->size == 0 ) return DAO_MT_EQ;
		tp = value.v.list->unitype;
		if( tp == self ) return DAO_MT_EQ;
		if( it1 == DAO_UDF ) return DAO_MT_UDF;
		if( it1 == DAO_ANY ) return DAO_MT_ANY;
		if( it1 == DAO_INITYPE ) return DAO_MT_INIT;
		if( tp ) return (DaoType_MatchTo( tp, self, defs ) == DAO_MT_EQ) * DAO_MT_EQ;
		break;
	case DAO_MAP :
		if( value.v.map->items->size == 0 ) return DAO_MT_EQ;
		tp = value.v.map->unitype;
		if( tp == self ) return DAO_MT_EQ;
		if( ((1<<it1)&flags) && ((1<<it2)&flags) ){
			if( it1 == DAO_UDF || it2 == DAO_UDF ) return DAO_MT_UDF;
			if( it1 == DAO_INITYPE || it2 == DAO_INITYPE ) return DAO_MT_INIT;
			if( it1 == DAO_ANY || it2 == DAO_ANY ) return DAO_MT_ANY;
		}
		if( tp ) return (DaoType_MatchTo( tp, self, defs ) == DAO_MT_EQ) * DAO_MT_EQ;
		break;
	case DAO_TUPLE :
		tp = value.v.tuple->unitype;
		if( tp == self ) return DAO_MT_EQ;
		if( value.v.tuple->items->size != self->nested->size ) return DAO_MT_NOT;

		for(i=0; i<self->nested->size; i++){
			tp = self->nested->items.pType[i];
			if( tp->tid == DAO_PAR_NAMED ) tp = tp->aux.v.type;

			/* for C functions that returns a tuple:
			 * the tuple may be assigned to a context value before
			 * its values are set properly! */
			if( value.v.tuple->items->data[i].t == 0 ) continue;
			if( tp->tid == DAO_UDF || tp->tid == DAO_ANY || tp->tid == DAO_INITYPE ) continue;

			mt = DaoType_MatchValue( tp, value.v.tuple->items->data[i], defs );
			if( mt < DAO_MT_SIM ) return 0;
		}
		if( value.v.tuple->unitype == NULL ) return DAO_MT_EQ;
		names = self->mapNames;
		tp = value.v.tuple->unitype;
		for(node=DMap_First(names); node; node=DMap_Next(names,node)){
			DNode *search = DMap_Find( tp->mapNames, node->key.pVoid );
			if( search && search->value.pInt != node->value.pInt ) return 0;
		}
		return DAO_MT_EQ;
	case DAO_FUNCTION :
	case DAO_ROUTINE :
		tp = value.v.routine->routType;
		if( tp == self ) return DAO_MT_EQ;
		if( tp ) return DaoType_MatchTo( tp, self, NULL );
		break;
	case DAO_VMPROCESS :
		tp = value.v.vmp->abtype;
		if( tp == self ) return DAO_MT_EQ;
		if( tp ) return DaoType_MatchTo( tp, self, defs );
		break;
	case DAO_CLASS :
		if( self->aux.v.p == NULL ) return DAO_MT_SUB; /* par : class */
		if( self->aux.v.klass == value.v.klass ) return DAO_MT_EQ;
		if( DaoClass_ChildOf( value.v.klass, self->aux.v.p ) ) return DAO_MT_SUB;
		break;
	case DAO_OBJECT :
		if( self->aux.v.klass == value.v.object->myClass ) return DAO_MT_EQ;
		tp = value.v.object->myClass->objType;
		if( self->tid == DAO_INTERFACE ){
			if( DaoType_HasInterface( tp, self->aux.v.inter ) ) return DAO_MT_SUB;
			if( DaoInterface_TryBindTo( self->aux.v.inter, tp, NULL, NULL ) ) return DAO_MT_SUB;
		}
		if( DaoClass_ChildOf( value.v.object->myClass, self->aux.v.p ) ) return DAO_MT_SUB;
		break;
	case DAO_CTYPE :
	case DAO_CDATA :
		if( self->typer == value.v.cdata->typer ){
			return DAO_MT_EQ;
		}else if( DaoCData_ChildOf( value.v.cdata->typer, self->typer ) ){
			return DAO_MT_SUB;
		}else if( self->tid == DAO_INTERFACE ){
			tp = value.v.cdata->typer->priv->abtype;
			if( DaoType_HasInterface( tp, self->aux.v.inter ) ) return DAO_MT_SUB;
			if( DaoInterface_TryBindTo( self->aux.v.inter, tp, NULL, NULL ) ) return DAO_MT_SUB;
		}else{
			return DAO_MT_NOT;
		}
		break;
	case DAO_TYPE :
		tp = (DaoType*)value.v.p;
		if( self->tid != DAO_TYPE ) return 0;
		/* if( tp == self ) return DAO_MT_EQ; */
		return DaoType_MatchTo( tp, self->nested->items.pType[0], defs );
	case DAO_FUTURE :
		tp = ((DaoFuture*)value.v.p)->unitype;
		if( tp == self ) return DAO_MT_EQ;
		if( it1 == DAO_UDF ) return DAO_MT_UDF;
		if( it1 == DAO_ANY ) return DAO_MT_ANY;
		if( it1 == DAO_INITYPE ) return DAO_MT_INIT;
		if( tp ) return (DaoType_MatchTo( tp, self, defs ) == DAO_MT_EQ) * DAO_MT_EQ;
		break;
	case DAO_PAR_NAMED :
	case DAO_PAR_DEFAULT :
		if( value.v.nameva->unitype == self ) return DAO_MT_EQ;
		return DaoType_MatchTo( value.v.nameva->unitype, self, defs );
	default :
		break;
	}
	return DAO_MT_NOT;
}
short DaoType_MatchValue2( DaoType *self, DValue value, DMap *defs )
{
	short m = DaoType_MatchValue( self, value, defs );
	if( m == 0 || value.t <= DAO_STREAM || value.t != self->tid ) return m;
	if( value.t == DAO_CDATA ){
		if( value.v.cdata->data == NULL ) m = 0;
	}else{
		if( value.v.p == self->value.v.p ) m = 0;
	}
	return m;
}
static void DMap_Erase2( DMap *defs, void *p )
{
	DArray *keys = DArray_New(0);
	DNode *node;
	int i;
	DMap_Erase( defs, p );
	for(node=DMap_First(defs); node; node=DMap_Next(defs,node)){
		if( node->value.pVoid == p ) DArray_Append( keys, node->key.pVoid );
	}
	for(i=0; i<keys->size; i++) DMap_Erase( defs, keys->items.pVoid[i] );
	DArray_Delete( keys );
}
DaoType* DaoType_DefineTypes( DaoType *self, DaoNameSpace *ns, DMap *defs )
{
	int i;
	DaoType *nest;
	DaoType *copy;
	DNode *node;

	if( self == NULL ) return NULL;
	if( DString_FindChar( self->name, '?', 0 ) == MAXSIZE
			&& DString_FindChar( self->name, '@', 0 ) == MAXSIZE ) return self;

	node = MAP_Find( defs, self );
	if( node ){
		if( node->value.pType == self ) return self;
		return DaoType_DefineTypes( node->value.pType, ns, defs );
	}

	if( self->tid == DAO_INITYPE ){
		node = MAP_Find( defs, self );
		if( node == NULL ) return self;
		return DaoType_DefineTypes( node->value.pType, ns, defs );
	}else if( self->tid == DAO_UDF ){
		node = MAP_Find( defs, self );
		copy = node ? node->value.pType : NULL;
		if( copy ==0 || copy->tid == DAO_ANY || copy->tid == DAO_UDF ) return self;
		return DaoType_DefineTypes( copy, ns, defs );
	}else if( self->tid == DAO_ANY ){
		return self;
	}else if( self->tid == DAO_CLASS ){ /* e.g., class<Item<@T>> */
		copy = DaoType_DefineTypes( self->aux.v.klass->objType, ns, defs );
		if( copy->aux.v.klass != self->aux.v.klass ) self = copy->aux.v.klass->clsType;
		return self;
	}

	copy = DaoType_New( "", self->tid, NULL, NULL );
	copy->typer = self->typer;
	copy->attrib = self->attrib;
	copy->ffitype = self->ffitype;
	copy->attrib &= ~ DAO_TYPE_EMPTY; /* not any more empty */
	DMap_Insert( defs, self, copy );
	if( self->mapNames ){
		if( copy->mapNames ) DMap_Delete( copy->mapNames );
		copy->mapNames = DMap_Copy( self->mapNames );
	}
	if( self->fname ) copy->fname = DString_Copy( self->fname );
	if( self->nested && DString_MatchMBS( self->name, "^ %@? %w+ %< ", NULL, NULL ) ){
		char sep = self->tid == DAO_VARIANT ? '|' : ',';
		if( copy->nested == NULL ) copy->nested = DArray_New(0);
		DString_AppendChar( copy->name, self->name->mbs[0] ); /* @routine<> */
		for(i=1; i<self->name->size; i++){
			char ch = self->name->mbs[i];
			if( ch != '_' && !isalnum( ch ) ) break;
			DString_AppendChar( copy->name, self->name->mbs[i] );
		}
		DString_AppendChar( copy->name, '<' );
		for(i=0; i<self->nested->size; i++){
			nest = DaoType_DefineTypes( self->nested->items.pType[i], ns, defs );
			if( nest ==NULL ) goto DefFailed;
			DArray_Append( copy->nested, nest );
			DString_Append( copy->name, nest->name );
			if( i+1 <self->nested->size ) DString_AppendChar( copy->name, sep );
		}
		GC_IncRCs( copy->nested );
		/* NOT FOR @T<int|string> kind types, see notes below: */
		if( self->aux.t == DAO_TYPE && self->tid != DAO_VARIANT ){
			DString_AppendMBS( copy->name, "=>" );
			copy->aux.v.type = DaoType_DefineTypes( self->aux.v.type, ns, defs );
			if( copy->aux.v.type ==NULL ) goto DefFailed;
			DString_Append( copy->name, copy->aux.v.type->name );
			GC_IncRC( copy->aux.v.type );
			copy->aux.t = DAO_TYPE;
		}
		DString_AppendChar( copy->name, '>' );
	}
	if( copy->aux.t == 0 && self->aux.t != 0 ){
		/* NOT FOR @T<int|string> kind types. Consider:
		 *   routine Sum( alist : list<@T<int|string>> ) =>@T { return alist.sum(); }
		 * when type inference is performed for "Sum( { 1, 2, 3 } )",
		 * type holder "@T" will be defined to "int", then type inference is
		 * performed on "alist.sum()", and "@T" will be defined to "@T<int|string>",
		 * because of the prototype of "sum()"; So a cyclic definition is formed. */
		if( self->aux.t == DAO_TYPE && self->tid != DAO_VARIANT ){
			copy->aux.v.type = DaoType_DefineTypes( self->aux.v.type, ns, defs );
			if( copy->aux.v.type ==NULL ) goto DefFailed;
			GC_IncRC( copy->aux.v.type );
			copy->aux.t = DAO_TYPE;
		}else{
			DValue_Copy( & copy->aux, self->aux );
		}
	}
	if( self->tid == DAO_PAR_NAMED ){
		DString_Append( copy->name, self->fname );
		DString_AppendChar( copy->name, ':' );
		DString_Append( copy->name, copy->aux.v.type->name );
	}else if( self->tid == DAO_PAR_DEFAULT ){
		DString_Append( copy->name, self->fname );
		DString_AppendChar( copy->name, '=' );
		DString_Append( copy->name, copy->aux.v.type->name );
	}else if( self->nested == NULL ){
		DString_Assign( copy->name, self->name );
	}
	DaoType_CheckAttributes( copy );
	if( self->tid == DAO_OBJECT && self->aux.v.klass->instanceClasses ){
		DaoClass *klass = self->aux.v.klass;
		klass = DaoClass_Instantiate( klass, copy->nested );
		DMap_Erase2( defs, copy );
		DaoType_Delete( copy );
		return klass->objType;
	}
	node = DMap_Find( ns->abstypes, copy->name );
#if 0
	if( strstr( copy->name->mbs, "map<" ) == copy->name->mbs ){
		printf( "%s  %p  %p\n", copy->name->mbs, copy, node );
		printf( "%s  %s  %p  %p\n", self->name->mbs, copy->name->mbs, copy, node );
		print_trace();
	}
#endif
	if( node ){
		DMap_Erase2( defs, copy );
		DaoType_Delete( copy );
		return node->value.pType;
	}else{
		GC_IncRC( copy );
		DMap_Insert( ns->abstypes, copy->name, copy );
		DMap_Insert( defs, self, copy );
	}
	DValue_Clear( & copy->value );
	DaoType_InitDefault( copy );
	return copy;
DefFailed:
	printf( "redefine failed\n" );
	return NULL;
}
void DaoType_RenewTypes( DaoType *self, DaoNameSpace *ns, DMap *defs )
{
	DaoType *tp = DaoType_DefineTypes( self, ns, defs );
	DaoType tmp = *self;
	if( tp == self || tp == NULL ) return;
	*self = *tp;
	*tp = tmp;
	DaoType_Delete( tp );
}
void DaoType_GetTypes( DaoType *self, DMap *types )
{
	int i;
	if( self->tid == DAO_INITYPE ){
		DMap_Insert( types, self, 0 );
		return;
	}
	if( self->nested ){
		for(i=0; i<self->nested->size; i++){
			DaoType_GetTypes( self->nested->items.pType[i], types );
		}
	}
	if( self->tid == DAO_TYPE && self->aux.t == DAO_TYPE )
		DaoType_GetTypes( self->aux.v.type, types );
}

/* interface implementations */
void DaoInterface_Delete( DaoInterface *self )
{
	DNode *it = DMap_First(self->methods);
	GC_DecRCs( self->supers );
	GC_DecRC( self->abtype );
	for(; it!=NULL; it=DMap_Next(self->methods,it)) GC_DecRC( it->value.pBase );
	DArray_Delete( self->supers );
	DMap_Delete( self->methods );
	dao_free( self );
}
DaoTypeBase interTyper=
{
	"interface", & baseCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoInterface_Delete, NULL
};

DaoInterface* DaoInterface_New( const char *name )
{
	DaoInterface *self = (DaoInterface*) dao_malloc( sizeof(DaoInterface) );
	DaoBase_Init( self, DAO_INTERFACE );
	self->bindany = 0;
	self->derived = 0;
	self->supers = DArray_New(0);
	self->methods = DHash_New(D_STRING,0);
	self->abtype = DaoType_New( name, DAO_INTERFACE, (DaoBase*)self, NULL );
	GC_IncRC( self->abtype );
	return self;
}
static int DRoutine_IsCompatible( DRoutine *self, DaoType *type, DMap *binds )
{
	DRoutine *rout;
	int i, j, k=-1, max = 0;
	for(i=0; i<self->routTable->size; i++){
		rout = (DRoutine*) self->routTable->items.pBase[i];
		if( rout->routType == type ) return 1;
	}
	for(i=0; i<self->routTable->size; i++){
		rout = (DRoutine*) self->routTable->items.pBase[i];
		j = DaoType_Match( rout->routType, type, NULL, binds );
		/*
		   printf( "%3i: %3i  %s  %s\n",i,j,rout->routType->name->mbs,type->name->mbs );
		 */
		if( j && j >= max ){
			max = j;
			k = i;
		}
	}
	return (k >= 0);
}
int DaoInterface_CheckBind( DArray *methods, DaoType *type, DMap *binds, DArray *fails )
{
	DValue value;
	int i, id, fcount = 0;
	if( type->tid == DAO_OBJECT ){
		DaoClass *klass = type->aux.v.klass;
		for(i=0; i<methods->size; i++){
			DRoutine *rout = methods->items.pRout2[i];
			id = DaoClass_FindConst( klass, rout->routName );
			if( id <0 ) goto RecordFailA;
			value = DaoClass_GetConst( klass, id );
			if( value.t != DAO_ROUTINE && value.t != DAO_FUNCTION ) goto RecordFailA;
			if( DRoutine_IsCompatible( (DRoutine*) value.v.routine, rout->routType, binds ) ==0 )
				goto RecordFailA;
			continue;
RecordFailA:
			if( fails ) DArray_Append( fails, rout );
			fcount += 1;
		}
	}else if( type->tid == DAO_CDATA ){
		for(i=0; i<methods->size; i++){
			DRoutine *rout = methods->items.pRout2[i];
			DaoFunction *func = DaoFindFunction( type->typer, rout->routName );
			if( func == NULL ) goto RecordFailB;
			if( DRoutine_IsCompatible( (DRoutine*) func, rout->routType, binds ) ==0 )
				goto RecordFailB;
			continue;
RecordFailB:
			if( fails ) DArray_Append( fails, rout );
			fcount += 1;
		}
	}else{
		fcount += methods->size;
		if( fails ){
			for(i=0; i<methods->size; i++){
				DRoutine *rout = methods->items.pRout2[i];
				DArray_Append( fails, rout );
			}
		}
	}
	return fcount ==0;
}
static void DaoInterface_TempBind( DaoInterface *self, DaoType *type, DMap *binds )
{
	int i, N = self->supers->size;
	void *pvoid[2];
	pvoid[0] = type;
	pvoid[1] = self->abtype;
	if( DMap_Find( binds, pvoid ) ) return;
	DMap_Insert( binds, pvoid, NULL );
	for(i=0; i<N; i++){
		DaoInterface *super = (DaoInterface*) self->supers->items.pBase[i];
		DaoInterface_TempBind( super, type, binds );
	}
}
/* INPUT:  <interface, type, first, last, 0> */
/* OUTPUT: <interface, type, first, last, fail_count> */
int DaoInterface_Bind( DArray *pairs, DArray *fails )
{
	DaoType *type;
	DaoInterface *inter;
	DArray *methods = DArray_New(0);
	DMap *binds = DHash_New(D_VOID2,0);
	int i, j, N = pairs->size;
	for(i=0; i<N; i+=5){
		inter = (DaoInterface*) pairs->items.pBase[i];
		type = (DaoType*) pairs->items.pBase[i+1];
		/*
		   printf( "%s  %s\n", inter->abtype->name->mbs, type->name->mbs );
		 */
		DaoInterface_TempBind( inter, type, binds );
	}
	for(i=0; i<N; i+=5){
		j = fails->size;
		inter = (DaoInterface*) pairs->items.pBase[i];
		type = (DaoType*) pairs->items.pBase[i+1];
		if( DMap_Find( type->interfaces, inter ) ) continue;
		methods->size = 0;
		DMap_SortMethods( inter->methods, methods );
		if( DaoInterface_CheckBind( methods, type, binds, fails ) ==0 ){
			pairs->items.pInt[i+4] = fails->size - j;
			continue;
		}

		GC_IncRC( inter );
		DMap_Insert( type->interfaces, inter, NULL );
		for(j=0; j<inter->supers->size; j++){
			DaoInterface *super = (DaoInterface*) inter->supers->items.pBase[j];
			if( DMap_Find( type->interfaces, super ) ) continue;
			GC_IncRC( super );
			DMap_Insert( type->interfaces, super, NULL );
		}
	}
	DMap_Delete( binds );
	DArray_Delete( methods );
	return fails->size ==0;
}
int DaoInterface_BindTo( DaoInterface *self, DaoType *type, DMap *binds, DArray *fails )
{
	DMap *newbinds = NULL;
	DArray *methods;
	void *pvoid[2];
	int i, bl;
	pvoid[0] = type;
	pvoid[1] = self->abtype;
	if( DMap_Find( type->interfaces, self ) ) return 1;
	if( binds && DMap_Find( binds, pvoid ) ) return 1;
	if( binds ==NULL ) newbinds = binds = DHash_New(D_VOID2,0);
	DaoInterface_TempBind( self, type, binds );
	methods = DArray_New(0);
	DMap_SortMethods( self->methods, methods );
	bl = DaoInterface_CheckBind( methods, type, binds, fails );
	DArray_Delete( methods );
	if( newbinds ) DMap_Delete( newbinds );
	if( bl ==0 ) return 0;
	GC_IncRC( self );
	DMap_Insert( type->interfaces, self, NULL );
	for(i=0; i<self->supers->size; i++){
		DaoInterface *super = (DaoInterface*) self->supers->items.pBase[i];
		if( DMap_Find( type->interfaces, super ) ) continue;
		GC_IncRC( super );
		DMap_Insert( type->interfaces, super, NULL );
	}
	return 1;
}
static int DaoInterface_TryBindTo( DaoInterface *self, DaoType *type, DMap *binds, DArray *fails )
{
	DNode *it;
	if( DMap_Find( type->interfaces, self ) ) return 1;
	for(it=DMap_First(type->interfaces); it; it=DMap_Next(type->interfaces,it)){
		DaoInterface *iter = (DaoInterface*) it->key.pVoid;
		if( DString_EQ( iter->abtype->name, self->abtype->name ) ) break;
	}
	if( self->bindany ==0 && it == NULL ) return 0;
	return DaoInterface_BindTo( self, type, binds, fails );
}
void DaoInterface_DeriveMethods( DaoInterface *self )
{
	int i, k, N = self->supers->size;
	DaoInterface *super;
	DRoutine *rout = NULL, *rout2;
	DNode *it, *node;
	for(i=0; i<N; i++){
		super = (DaoInterface*) self->supers->items.pBase[i];
		for(it=DMap_First(super->methods); it; it=DMap_Next( super->methods, it )){
			rout2 = (DRoutine*) it->value.pVoid;
			node = DMap_Find( self->methods, it->key.pVoid );
			if( node == NULL ){
				rout = DRoutine_New(); /* dummy routine */
				DString_Assign( rout->routName, it->key.pString );
				rout->routType = rout2->routType;
				rout->routHost = self->abtype;
				rout->tidHost = DAO_INTERFACE;
				rout->nameSpace = rout2->nameSpace;
				GC_IncRC( rout->routHost );
				GC_IncRC( rout->routType );
				GC_IncRC( rout->nameSpace );
				/* reference count of "rout" is already increased by DRoutine_New() */
				DMap_Insert( self->methods, it->key.pVoid, rout );
			}else{
				rout = (DRoutine*) node->value.pVoid;
			}
			for( k=0; k<rout2->routTable->size; k++)
				DRoutine_AddOverLoad( rout, rout2->routTable->items.pRout2[k] );
		}
	}
	self->derived = 1;
}

void DMap_SortMethods( DMap *hash, DArray *methods )
{
	DMap *map = DMap_New(D_STRING,0);
	DString *name = DString_New(1);
	DNode *it;
	int i, n;
	for(it=DMap_First(hash); it; it=DMap_Next(hash,it)){
		DRoutine *one = (DRoutine*) it->value.pVoid;
		n = one->routTable->size;
		for(i=0; i<n; i++){
			DRoutine *rout = one->routTable->items.pRout2[i];
			DString_Assign( name, rout->routName );
			DString_AppendMBS( name, " " );
			DString_Append( name, rout->routType->name );
			DMap_Insert( map, name, (void*)rout );
		}
	}
	DArray_Clear( methods );
	for(it=DMap_First(map); it; it=DMap_Next(map,it))
		DArray_Append( methods, it->value.pVoid );
	DMap_Delete( map );
	DString_Delete( name );
}
int DaoType_HasInterface( DaoType *self, DaoInterface *inter )
{
	DMap *inters = self->interfaces;
	size_t i;
	if( self == NULL || inter == NULL ) return 0;
	if( DMap_Find( inters, inter ) ) return DAO_MT_SUB;
	if( self->tid == DAO_OBJECT ){
		DaoClass *klass = self->aux.v.klass;
		for(i=0; i<klass->superClass->size; i++){
			DaoBase *super = klass->superClass->items.pBase[i];
			DaoType *type = NULL;
			if( super->type == DAO_CLASS ){
				type = ((DaoClass*)super)->objType;
			}else if( super->type == DAO_CDATA ){
				type = ((DaoCData*)super)->typer->priv->abtype;
			}
			if( DaoType_HasInterface( type, inter ) ) return DAO_MT_SUB;
		}
	}else if( self->tid == DAO_CDATA ){
		i = 0;
		while( self->typer->supers[i] ){
			DaoType *type = self->typer->supers[i]->priv->abtype;
			if( DaoType_HasInterface( type, inter ) ) return DAO_MT_SUB;
			i += 1;
		}
	}
	return 0;
}
