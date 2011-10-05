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
#include"daoProcess.h"
#include"daoGC.h"
#include"daoClass.h"
#include"daoValue.h"

void DaoType_Delete( DaoType *self )
{
	//printf( "DaoType_Delete: %p\n", self );
	if( self->refCount ){ /* likely to be referenced by its default value */
		GC_IncRC( self );
		GC_DecRC( self );
		return;
	}
	GC_DecRC( self->aux );
	GC_DecRC( self->value );
	GC_DecRC( self->kernel );
	GC_DecRCs( self->nested );
	DString_Delete( self->name );
	if( self->fname ) DString_Delete( self->fname );
	if( self->nested ) DArray_Delete( self->nested );
	if( self->mapNames ) DMap_Delete( self->mapNames );
	if( self->interfaces ) DMap_Delete( self->interfaces );
	DaoLateDeleter_Push( self );
}
extern DaoEnum* DaoProcess_GetEnum( DaoProcess *self, DaoVmCode *vmc );
static void DaoType_GetField( DaoValue *self0, DaoProcess *proc, DString *name )
{
	DaoType *self = & self0->xType;
	DaoEnum *denum = DaoProcess_GetEnum( proc, proc->activeCode );
	DNode *node;
	if( self->mapNames == NULL ) goto ErrorNotExist;
	node = DMap_Find( self->mapNames, name );
	if( node == NULL ) goto ErrorNotExist;
	GC_ShiftRC( self, denum->etype );
	denum->etype = self;
	denum->value = node->value.pInt;
	return;
ErrorNotExist:
	DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, DString_GetMBS( name ) );
}
static void DaoType_GetItem( DaoValue *self0, DaoProcess *proc, DaoValue *ids[], int N )
{
	DaoType *self = & self0->xType;
	DaoEnum *denum = DaoProcess_GetEnum( proc, proc->activeCode );
	DNode *node;
	if( self->mapNames == NULL || N != 1 || ids[0]->type != DAO_INTEGER ) goto ErrorNotExist;
	for(node=DMap_First(self->mapNames);node;node=DMap_Next(self->mapNames,node)){
		if( node->value.pInt == ids[0]->xInteger.value ){
			GC_ShiftRC( self, denum->etype );
			denum->etype = self;
			denum->value = node->value.pInt;
			return;
		}
	}
ErrorNotExist:
	DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "not valid" );
}
static DaoTypeCore typeCore=
{
	NULL,
	DaoType_GetField,
	DaoValue_SetField,
	DaoType_GetItem,
	DaoValue_SetItem,
	DaoValue_Print,
	DaoValue_NoCopy
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

	if( self->tid == DAO_TUPLE ){
		int i;
		self->rntcount = 0;
		for(i=0; i<self->nested->size; i++){
			DaoType *it = self->nested->items.pType[i];
			if( it->tid == DAO_PAR_NAMED ) it = & it->aux->xType;
			self->rntcount += it->tid >= DAO_INTEGER && it->tid <= DAO_DOUBLE;
		}
	}
}
DaoType* DaoType_New( const char *name, short tid, DaoValue *extra, DArray *nest )
{
	DaoTypeBase *typer = DaoVmSpace_GetTyper( tid );
	DaoType *self = (DaoType*) dao_calloc( 1, sizeof(DaoType) );
	DaoValue_Init( self, DAO_TYPE );
	self->tid = tid;
	self->name = DString_New(1);
	self->typer = typer;
	if( typer->core ){
		self->kernel = typer->core->kernel;
		GC_IncRC( self->kernel );
	}
	if( extra ){
		self->aux = extra;
		GC_IncRC( extra );
	}
	if( tid == DAO_OBJECT || tid == DAO_CTYPE ) self->interfaces = DHash_New(0,0);
	DString_SetMBS( self->name, name );
	if( (tid == DAO_PAR_NAMED || tid == DAO_PAR_DEFAULT) && extra && extra->type == DAO_TYPE ){
		self->fname = DString_New(1);
		DString_SetMBS( self->fname, name );
		DString_AppendChar( self->name, (tid == DAO_PAR_NAMED) ? ':' : '=' );
		DString_Append( self->name, self->aux->xType.name );
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
	return self;
}
void DaoType_InitDefault( DaoType *self )
{
	complex16 com;
	DaoValue *value = NULL;
	DaoType *itype, **types = self->nested ? self->nested->items.pType : NULL;
	int i, count = self->nested ? self->nested->size : 0;

	if( self->value && self->value->type != DAO_TUPLE ) return;
	if( self->value && self->value->xTuple.size == count ) return;

	switch( self->tid ){
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		itype = types && self->nested->size > 0 ? types[0] : NULL;
		value = (DaoValue*) DaoArray_New( itype ? itype->tid : DAO_INTEGER );
		value->xArray.unitype = self;
		GC_IncRC( self );
		break;
#endif
	case DAO_LIST :
		value = (DaoValue*) DaoList_New();
		value->xList.unitype = self;
		GC_IncRC( self );
		break;
	case DAO_MAP :
		value = (DaoValue*) DaoMap_New(0);
		value->xMap.unitype = self;
		GC_IncRC( self );
		break;
	case DAO_TUPLE :
		value = (DaoValue*) DaoTuple_New( count );
		value->xTuple.unitype = self;
		GC_IncRC( self );
		for(i=0; i<count; i++){
			DaoType_InitDefault( types[i] );
			DaoValue_Copy( types[i]->value, & value->xTuple.items[i] );
		}
		break;
	case DAO_VARIANT :
		for(i=0; i<count; i++) DaoType_InitDefault( types[i] );
		if( count ) value = types[0]->value;
		break;
	case DAO_UDF :
	case DAO_ANY :
	case DAO_INITYPE :
	case DAO_ROUTINE :
	case DAO_INTERFACE : value = null; break;
	case DAO_VALTYPE : value = self->aux; break;
	case DAO_INTEGER : value = (DaoValue*) DaoInteger_New(0); break;
	case DAO_FLOAT  : value = (DaoValue*) DaoFloat_New(0.0); break;
	case DAO_DOUBLE : value = (DaoValue*) DaoDouble_New(0.0); break;
	case DAO_COMPLEX : value = (DaoValue*) DaoComplex_New(com); break;
	case DAO_LONG   : value = (DaoValue*) DaoLong_New(); break;
	case DAO_STRING : value = (DaoValue*) DaoString_New(1); break;
	case DAO_ENUM : value = (DaoValue*) DaoEnum_New( self, 0 ); break;
	}
	GC_ShiftRC( value, self->value );
	self->value = value;
	if( value ) value->xNull.trait |= DAO_DATA_CONST;
}
DaoType* DaoType_Copy( DaoType *other )
{
	DNode *it;
	DaoType *self = (DaoType*) dao_malloc( sizeof(DaoType) );
	memcpy( self, other, sizeof(DaoType) );
	DaoValue_Init( self, DAO_TYPE ); /* to reset gc fields */
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
		for(; it!=NULL; it=DMap_Next(other->interfaces,it)) GC_IncRC( it->key.pValue );
	}
	self->aux = other->aux;
	self->value = other->value;
	GC_IncRC( other->aux );
	GC_IncRC( other->value );
	GC_IncRC( other->kernel );
	return self;
}
void DaoType_MapNames( DaoType *self )
{
	DaoType *tp;
	int i;
	if( self->nested == NULL ) return;
	if( self->tid != DAO_TUPLE && self->tid != DAO_ROUTINE ) return;
	if( self->mapNames == NULL ) self->mapNames = DMap_New(D_STRING,0);
	for(i=0; i<self->nested->size; i++){
		tp = self->nested->items.pType[i];
		if( tp->fname ) MAP_Insert( self->mapNames, tp->fname, i );
	}
}
DaoType* DaoType_GetVariantItem( DaoType *self, int tid )
{
	if( self->tid == DAO_VARIANT && self->nested ){
		DaoType **types = self->nested->items.pType;
		int i, n = self->nested->size;
		for(i=0; i<n; i++) if( types[i]->tid == tid ) return types[i];
	}
	return NULL;
}
DaoType* DaoType_GetFromTypeStructure( DaoTypeBase *typer )
{
	if( typer->core == NULL ) return NULL;
	return typer->core->kernel->abtype;
}

#define MIN(x,y) (x>y?y:x)

extern int DaoCdata_ChildOf( DaoTypeBase *self, DaoTypeBase *super );

static unsigned char dao_type_matrix[END_EXTRA_TYPES][END_EXTRA_TYPES];

void DaoType_Init()
{
	int i, j;
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
	dao_type_matrix[DAO_ABROUTINE][DAO_ROUTINE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_ROUTINE][DAO_ROUTINE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_FUNCTION][DAO_ROUTINE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_FUNCTREE][DAO_ROUTINE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_FUNCTREE][DAO_FUNCTREE] = DAO_MT_EQ+1;
	dao_type_matrix[DAO_PROCESS][DAO_ROUTINE] = DAO_MT_EQ+1;
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
	if( p1 ) ext1 = & self->aux->xType;
	if( p2 ) ext2 = & type->aux->xType;

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
			if( mt == DAO_MT_EQ ) break;
		}
		if( mt && defs && type->aux && type->aux->type == DAO_TYPE )
			MAP_Insert( defs, type->aux, self );
		return mt;
	}else if( type->tid == DAO_VALTYPE ){
		if( self->tid != DAO_VALTYPE ) return DaoType_MatchValue( self, type->aux, defs );
		if( DaoValue_Compare( self->aux, type->aux ) ==0 ) return DAO_MT_EQ + 1;
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
	case DAO_FUNCTREE :
		if( type->tid == DAO_FUNCTREE ) return DAO_MT_EQ * (self == type);
		if( type->tid == DAO_ROUTINE ){
			DRoutine *rout;
			DaoType **tps = type->nested->items.pType;
			DArray *routines = self->aux->xFunctree.routines;
			int np = type->nested->size;
			for(i=0; i<routines->size; i++){
				if( routines->items.pRout2[i]->routType == type ) return DAO_MT_EQ;
			}
			rout = DRoutine_ResolveByType( self->aux, NULL, tps, np, DVM_CALL );
			if( rout == NULL ) return DAO_MT_NOT;
			return DaoType_MatchTo( rout->routType, type, defs );
		}
		return DAO_MT_NOT;
		break;
	case DAO_ROUTINE :
		if( self->name->mbs[0] != type->name->mbs[0] ) return 0; /* @routine */
		if( self->nested->size < type->nested->size ) return DAO_MT_NOT;
		if( (self->attrib & DAO_TYPE_COROUTINE) && !(type->attrib & DAO_TYPE_COROUTINE) ) return 0;
		if( (self->cbtype == NULL) != (type->cbtype == NULL) ) return 0;
		if( self->aux == NULL && type->aux ) return 0;
		if( self->cbtype && DaoType_MatchTo( self->cbtype, type->cbtype, defs ) ==0 ) return 0;
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
		if( self->aux && type->aux ){
			k = DaoType_Match( & self->aux->xType, & type->aux->xType, defs, binds );
			if( k < mt ) mt = k;
		}
		break;
	case DAO_CLASS :
	case DAO_OBJECT :
		/* par : class */
		if( type->aux == NULL && self->tid == DAO_CLASS ) return DAO_MT_SUB;
		if( self->aux == type->aux ) return DAO_MT_EQ;
		it1 = self->aux->xClass.objType;
		if( type->tid == DAO_INTERFACE ){
			if( DaoType_HasInterface( it1, & type->aux->xInterface ) ) return DAO_MT_SUB;
			if( DaoInterface_TryBindTo( & type->aux->xInterface, it1, binds, NULL ) ) return DAO_MT_SUB;
		}
		if( DaoClass_ChildOf( & self->aux->xClass, type->aux ) ) return DAO_MT_SUB;
		return DAO_MT_NOT;
		break;
	case DAO_CTYPE :
	case DAO_CDATA :
		if( self->typer == type->typer ){
			return DAO_MT_EQ;
		}else if( DaoCdata_ChildOf( self->typer, type->typer ) ){
			return DAO_MT_SUB;
		}else if( type->tid == DAO_INTERFACE ){
			if( DaoType_HasInterface( self, & type->aux->xInterface ) ) return DAO_MT_SUB;
			if( DaoInterface_TryBindTo( & type->aux->xInterface, self, binds, NULL ) ) return DAO_MT_SUB;
		}else{
			return DAO_MT_NOT;
		}
		break;
	case DAO_VALTYPE :
		if( type->tid != DAO_VALTYPE ) return DaoType_MatchValue( type, self->aux, defs );
		if( DaoValue_Compare( self->aux, type->aux ) ==0 ) return DAO_MT_EQ + 1;
		return DAO_MT_NOT;
	case DAO_VARIANT :
		mt = DAO_MT_NOT;
		for(i=0; i<self->nested->size; i++){
			it1 = self->nested->items.pType[i];
			mt2 = DaoType_MatchTo( it1, type, defs );
			if( mt2 > mt ) mt = mt2;
			if( mt == DAO_MT_EQ ) break;
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
	void *pvoid[2];
	size_t mt;

	pvoid[0] = self;
	pvoid[1] = type;

	if( self ==NULL || type ==NULL ) return DAO_MT_NOT;
	if( self == type ) return DAO_MT_EQ;

	mt = DaoType_MatchToX( self, type, defs, binds );
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
short DaoType_MatchValue( DaoType *self, DaoValue *value, DMap *defs )
{
	ulong_t flags;
	DaoType *tp;
	DaoEnum *other;
	DNode *node;
	DMap *names;
	short i, mt, mt2, it1, it2;
	if( self == NULL ) return DAO_MT_NOT;
	mt = dao_type_matrix[value->type][self->tid];
	if( mt == DAO_MT_SIM || mt == DAO_MT_EQ ) return mt;
	if( value->type == 0 || self->tid == DAO_VALTYPE || self->tid == DAO_VARIANT ){
		if( self->tid == DAO_VALTYPE ){
			if( DaoValue_Compare( self->aux, value ) ==0 ) return DAO_MT_EQ + 1;
		}else if( self->tid == DAO_VARIANT ){
			mt = DAO_MT_NOT;
			for(i=0; i<self->nested->size; i++){
				tp = self->nested->items.pType[i];
				mt2 = DaoType_MatchValue( tp, value, defs );
				if( mt2 > mt ) mt = mt2;
				if( mt == DAO_MT_EQ ) break;
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
		if( value->type < DAO_ARRAY ) return mt;
	default : break;
	}
	it1 = it2 = 0;
	if( self->nested ){
		if( self->nested->size ) it1 = self->nested->items.pType[0]->tid;
		if( self->nested->size >1 ) it2 = self->nested->items.pType[1]->tid;
	}
	switch( value->type ){
	case DAO_ENUM :
		if( value->xEnum.etype == self ) return DAO_MT_EQ;
		other = & value->xEnum;
		names = other->etype->mapNames;
		for(node=DMap_First(names); node; node=DMap_Next(names,node)){
			if( other->etype->flagtype ){
				if( !(node->value.pInt & other->value) ) continue;
			}else if( node->value.pInt != other->value ){
				continue;
			}
			if( DMap_Find( self->mapNames, node->key.pVoid ) == NULL ) return 0;
		}
		return DAO_MT_EQ;
		break;
	case DAO_ARRAY :
		if( value->xArray.size == 0 ) return DAO_MT_EQ;
		tp = value->xArray.unitype;
		if( tp == self ) return DAO_MT_EQ;
		if( it1 == DAO_UDF ) return DAO_MT_UDF;
		if( it1 == DAO_ANY ) return DAO_MT_ANY;
		if( it1 == DAO_INITYPE ) return DAO_MT_INIT;
		if( value->xArray.numType == it1 ) return DAO_MT_EQ;
		/* return DAO_MT_EQ for exact match, or zero otherwise: */
		if( tp ) return (DaoType_MatchTo( tp, self, defs ) == DAO_MT_EQ) * DAO_MT_EQ;
		break;
	case DAO_LIST :
		if( value->xList.items->size == 0 ) return DAO_MT_EQ;
		tp = value->xList.unitype;
		if( tp == self ) return DAO_MT_EQ;
		if( it1 == DAO_UDF ) return DAO_MT_UDF;
		if( it1 == DAO_ANY ) return DAO_MT_ANY;
		if( it1 == DAO_INITYPE ) return DAO_MT_INIT;
		if( tp ) return (DaoType_MatchTo( tp, self, defs ) == DAO_MT_EQ) * DAO_MT_EQ;
		break;
	case DAO_MAP :
		if( value->xMap.items->size == 0 ) return DAO_MT_EQ;
		tp = value->xMap.unitype;
		if( tp == self ) return DAO_MT_EQ;
		flags = (1<<DAO_UDF)|((ulong_t)1<<DAO_ANY)|((ulong_t)1<<DAO_INITYPE);
		if( (((ulong_t)1<<it1)&flags) && (((ulong_t)1<<it2)&flags) ){
			if( it1 == DAO_UDF || it2 == DAO_UDF ) return DAO_MT_UDF;
			if( it1 == DAO_INITYPE || it2 == DAO_INITYPE ) return DAO_MT_INIT;
			if( it1 == DAO_ANY || it2 == DAO_ANY ) return DAO_MT_ANY;
		}
		if( tp ) return (DaoType_MatchTo( tp, self, defs ) == DAO_MT_EQ) * DAO_MT_EQ;
		break;
	case DAO_TUPLE :
		tp = value->xTuple.unitype;
		if( tp == self ) return DAO_MT_EQ;
		if( value->xTuple.size != self->nested->size ) return DAO_MT_NOT;

		for(i=0; i<self->nested->size; i++){
			tp = self->nested->items.pType[i];
			if( tp->tid == DAO_PAR_NAMED ) tp = & tp->aux->xType;

			/* for C functions that returns a tuple:
			 * the tuple may be assigned to a context value before
			 * its values are set properly! */
			if( value->xTuple.items[i] == NULL ) continue;
			if( tp->tid == DAO_UDF || tp->tid == DAO_ANY || tp->tid == DAO_INITYPE ) continue;

			mt = DaoType_MatchValue( tp, value->xTuple.items[i], defs );
			if( mt < DAO_MT_SIM ) return 0;
		}
		if( value->xTuple.unitype == NULL ) return DAO_MT_EQ;
		names = self->mapNames;
		tp = value->xTuple.unitype;
		for(node=DMap_First(names); node; node=DMap_Next(names,node)){
			DNode *search = DMap_Find( tp->mapNames, node->key.pVoid );
			if( search && search->value.pInt != node->value.pInt ) return 0;
		}
		return DAO_MT_EQ;
	case DAO_FUNCTREE :
		if( self->tid == DAO_FUNCTREE ) return DAO_MT_EQ * (self == value->xFunctree.unitype);
		if( self->tid == DAO_ROUTINE ){
			DRoutine *rout;
			DaoType **tps = self->nested->items.pType;
			DArray *routines = value->xFunctree.routines;
			int np = self->nested->size;
			for(i=0; i<routines->size; i++){
				if( routines->items.pRout2[i]->routType == self ) return DAO_MT_EQ;
			}
			rout = DRoutine_ResolveByType( value, NULL, tps, np, DVM_CALL );
			if( rout == NULL ) return DAO_MT_NOT;
			return DaoType_MatchTo( rout->routType, self, defs );
		}
		break;
	case DAO_FUNCTION :
	case DAO_ROUTINE :
	case DAO_ABROUTINE :
		tp = value->xRoutine.routType;
		if( tp == self ) return DAO_MT_EQ;
		if( tp ) return DaoType_MatchTo( tp, self, NULL );
		break;
	case DAO_PROCESS :
		tp = value->xProcess.abtype;
		if( tp == self ) return DAO_MT_EQ;
		if( tp ) return DaoType_MatchTo( tp, self, defs );
		break;
	case DAO_CLASS :
		if( self->aux == NULL ) return DAO_MT_SUB; /* par : class */
		if( self->aux == value ) return DAO_MT_EQ;
		if( DaoClass_ChildOf( & value->xClass, self->aux ) ) return DAO_MT_SUB;
		break;
	case DAO_OBJECT :
		if( self->aux == (DaoValue*) value->xObject.defClass ) return DAO_MT_EQ;
		tp = value->xObject.defClass->objType;
		if( self->tid == DAO_INTERFACE ){
			if( DaoType_HasInterface( tp, & self->aux->xInterface ) ) return DAO_MT_SUB;
			if( DaoInterface_TryBindTo( & self->aux->xInterface, tp, NULL, NULL ) ) return DAO_MT_SUB;
		}
		if( DaoClass_ChildOf( value->xObject.defClass, self->aux ) ) return DAO_MT_SUB;
		break;
	case DAO_CTYPE :
	case DAO_CDATA :
		tp = value->xCdata.ctype;
		if( self == tp ){
			return DAO_MT_EQ;
		}else if( DaoCdata_ChildOf( value->xCdata.typer, self->typer ) ){
			return DAO_MT_SUB;
		}else if( self->tid == DAO_INTERFACE ){
			if( DaoType_HasInterface( tp, & self->aux->xInterface ) ) return DAO_MT_SUB;
			if( DaoInterface_TryBindTo( & self->aux->xInterface, tp, NULL, NULL ) ) return DAO_MT_SUB;
		}else{
			return DAO_MT_NOT;
		}
		break;
	case DAO_TYPE :
		tp = & value->xType;
		if( self->tid != DAO_TYPE ) return 0;
		/* if( tp == self ) return DAO_MT_EQ; */
		return DaoType_MatchTo( tp, self->nested->items.pType[0], defs );
	case DAO_FUTURE :
		tp = ((DaoFuture*)value)->unitype;
		if( tp == self ) return DAO_MT_EQ;
		if( it1 == DAO_UDF ) return DAO_MT_UDF;
		if( it1 == DAO_ANY ) return DAO_MT_ANY;
		if( it1 == DAO_INITYPE ) return DAO_MT_INIT;
		if( tp ) return (DaoType_MatchTo( tp, self, defs ) == DAO_MT_EQ) * DAO_MT_EQ;
		break;
	case DAO_PAR_NAMED :
	case DAO_PAR_DEFAULT :
		if( value->xNameValue.unitype == self ) return DAO_MT_EQ;
		return DaoType_MatchTo( value->xNameValue.unitype, self, defs );
	default :
		break;
	}
	return DAO_MT_NOT;
}
short DaoType_MatchValue2( DaoType *self, DaoValue *value, DMap *defs )
{
	short m = DaoType_MatchValue( self, value, defs );
	if( m == 0 || value->type <= DAO_STREAM || value->type != self->tid ) return m;
	if( value->type == DAO_CDATA ){
		if( value->xCdata.data == NULL ) m = 0;
	}else{
		if( value == self->value ) m = 0;
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
DaoType* DaoType_DefineTypes( DaoType *self, DaoNamespace *ns, DMap *defs )
{
	int i;
	DaoType *nest;
	DaoType *copy = NULL;
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
		copy = DaoType_DefineTypes( self->aux->xClass.objType, ns, defs );
		if( copy->aux != self->aux ) self = copy->aux->xClass.clsType;
		return self;
	}

	copy = DaoType_New( "", self->tid, NULL, NULL );
	GC_ShiftRC( self->kernel, copy->kernel );
	copy->kernel = self->kernel;
	copy->typer = self->typer;
	copy->attrib = self->attrib;
	copy->ffitype = self->ffitype;
	copy->attrib &= ~ DAO_TYPE_EMPTY; /* not any more empty */
	GC_IncRC( copy );
	DMap_Insert( defs, self, copy );
	if( self->mapNames ){
		if( copy->mapNames ) DMap_Delete( copy->mapNames );
		copy->mapNames = DMap_Copy( self->mapNames );
	}
	if( self->fname ) copy->fname = DString_Copy( self->fname );
	if( self->nested /*XXX && DString_MatchMBS( self->name, "^ %@? %w+ %< ", NULL, NULL )*/ ){
		int m = DString_MatchMBS( self->name, "^ %@? %w+ %< ", NULL, NULL );
		char sep = self->tid == DAO_VARIANT ? '|' : ',';
		if( copy->nested == NULL ) copy->nested = DArray_New(0);
		if( self->tid == DAO_CODEBLOCK ){
			DString_AppendChar( copy->name, '[' );
		}else if( self->tid != DAO_VARIANT && m ){
			DString_AppendChar( copy->name, self->name->mbs[0] ); /* @routine<> */
			for(i=1; i<self->name->size; i++){
				char ch = self->name->mbs[i];
				if( ch != '_' && !isalnum( ch ) ) break;
				DString_AppendChar( copy->name, self->name->mbs[i] );
			}
			DString_AppendChar( copy->name, '<' );
		}
		for(i=0; i<self->nested->size; i++){
			nest = DaoType_DefineTypes( self->nested->items.pType[i], ns, defs );
			if( nest ==NULL ) goto DefFailed;
			DArray_Append( copy->nested, nest );
			DString_Append( copy->name, nest->name );
			if( i+1 <self->nested->size ) DString_AppendChar( copy->name, sep );
		}
		GC_IncRCs( copy->nested );
		/* NOT FOR @T<int|string> kind types, see notes below: */
		if( self->aux && self->aux->type == DAO_TYPE ){
			copy->aux = (DaoValue*) DaoType_DefineTypes( & self->aux->xType, ns, defs );
			if( copy->aux ==NULL ) goto DefFailed;
			GC_IncRC( copy->aux );
			if( self->tid != DAO_VARIANT && (m || self->tid == DAO_CODEBLOCK) ){
				DString_AppendMBS( copy->name, "=>" );
				if( self->attrib & DAO_TYPE_COROUTINE ) DString_AppendChar( copy->name, '[' );
				DString_Append( copy->name, copy->aux->xType.name );
				if( self->attrib & DAO_TYPE_COROUTINE ) DString_AppendChar( copy->name, ']' );
			}
		}
		if( self->tid == DAO_CODEBLOCK ){
			DString_AppendChar( copy->name, ']' );
		}else if( self->tid != DAO_VARIANT && m ){
			DString_AppendChar( copy->name, '>' );
		}
	}
	if( copy->aux == NULL && self->aux != NULL ){
		copy->aux = self->aux;
		/* NOT FOR @T<int|string> kind types. Consider:
		 *   routine Sum( alist : list<@T<int|string>> ) =>@T { return alist.sum(); }
		 * when type inference is performed for "Sum( { 1, 2, 3 } )",
		 * type holder "@T" will be defined to "int", then type inference is
		 * performed on "alist.sum()", and "@T" will be defined to "@T<int|string>",
		 * because of the prototype of "sum()"; So a cyclic definition is formed. */
		if( self->aux->type == DAO_TYPE && self->tid != DAO_VARIANT ){
			copy->aux = (DaoValue*) DaoType_DefineTypes( & self->aux->xType, ns, defs );
			if( copy->aux ==NULL ) goto DefFailed;
		}
		GC_IncRC( copy->aux );
	}
	if( copy->cbtype == NULL && self->cbtype != NULL ){
		copy->cbtype = DaoType_DefineTypes( self->cbtype, ns, defs );
		if( copy->cbtype ==NULL ) goto DefFailed;
		GC_IncRC( copy->cbtype );
		DString_Append( copy->name, copy->cbtype->name );
	}
	if( self->tid == DAO_PAR_NAMED ){
		DString_Append( copy->name, self->fname );
		DString_AppendChar( copy->name, ':' );
		DString_Append( copy->name, copy->aux->xType.name );
	}else if( self->tid == DAO_PAR_DEFAULT ){
		DString_Append( copy->name, self->fname );
		DString_AppendChar( copy->name, '=' );
		DString_Append( copy->name, copy->aux->xType.name );
	}else if( self->nested == NULL ){
		DString_Assign( copy->name, self->name );
	}
	DaoType_CheckAttributes( copy );
	if( self->tid == DAO_OBJECT && self->aux->xClass.instanceClasses ){
		DaoClass *klass = & self->aux->xClass;
		klass = DaoClass_Instantiate( klass, copy->nested );
		DMap_Erase2( defs, copy );
		GC_DecRC( copy );
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
		GC_DecRC( copy );
		return node->value.pType;
	}else{
		//GC_IncRC( copy );
		/* reference count already increased */
		DMap_Insert( ns->abstypes, copy->name, copy );
		DMap_Insert( defs, self, copy );
	}
	//DValue_Clear( & copy->value );
	DaoType_InitDefault( copy );
	return copy;
DefFailed:
	GC_DecRC( copy );
	printf( "redefine failed\n" );
	return NULL;
}
void DaoType_RenewTypes( DaoType *self, DaoNamespace *ns, DMap *defs )
{
	DaoType *tp = DaoType_DefineTypes( self, ns, defs );
	DaoType tmp = *self;
	if( tp == self || tp == NULL ) return;
	*self = *tp;
	*tp = tmp;
	DaoType_Delete( tp );
}
void DaoType_GetTypeHolders( DaoType *self, DMap *types )
{
	int i;
	if( self->tid == DAO_INITYPE ){
		DMap_Insert( types, self, 0 );
		return;
	}
	if( self->nested ){
		for(i=0; i<self->nested->size; i++){
			DaoType_GetTypeHolders( self->nested->items.pType[i], types );
		}
	}
	if( self->tid == DAO_TYPE && self->aux && self->aux->type == DAO_TYPE )
		DaoType_GetTypeHolders( & self->aux->xType, types );
}

/* interface implementations */
void DaoInterface_Delete( DaoInterface *self )
{
	DNode *it = DMap_First(self->methods);
	GC_DecRCs( self->supers );
	GC_DecRC( self->abtype );
	for(; it!=NULL; it=DMap_Next(self->methods,it)) GC_DecRC( it->value.pValue );
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
	DaoValue_Init( self, DAO_INTERFACE );
	self->bindany = 0;
	self->derived = 0;
	self->supers = DArray_New(0);
	self->methods = DHash_New(D_STRING,0);
	self->abtype = DaoType_New( name, DAO_INTERFACE, (DaoValue*)self, NULL );
	GC_IncRC( self->abtype );
	return self;
}
static int DaoFunctree_IsCompatible( DaoFunctree *self, DaoType *type, DMap *binds )
{
	DRoutine *rout;
	int i, j, k=-1, max = 0;
	for(i=0; i<self->routines->size; i++){
		rout = (DRoutine*) self->routines->items.pValue[i];
		if( rout->routType == type ) return 1;
	}
	for(i=0; i<self->routines->size; i++){
		rout = (DRoutine*) self->routines->items.pValue[i];
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
static int DRoutine_IsCompatible( DaoValue *self, DaoType *type, DMap *binds )
{
	DRoutine *rout = (DRoutine*)self;
	if( self->type == DAO_FUNCTREE )
		return DaoFunctree_IsCompatible( & self->xFunctree, type, binds );
	return DaoType_Match( rout->routType, type, NULL, binds );
}
int DaoInterface_CheckBind( DArray *methods, DaoType *type, DMap *binds, DArray *fails )
{
	DaoValue *value;
	int i, id, fcount = 0;
	if( type->tid == DAO_OBJECT ){
		DaoClass *klass = & type->aux->xClass;
		for(i=0; i<methods->size; i++){
			DRoutine *rout = methods->items.pRout2[i];
			id = DaoClass_FindConst( klass, rout->routName );
			if( id <0 ) goto RecordFailA;
			value = DaoClass_GetConst( klass, id );
			if( DRoutine_IsCompatible( value, rout->routType, binds ) ==0 ) goto RecordFailA;
			continue;
RecordFailA:
			if( fails ) DArray_Append( fails, rout );
			fcount += 1;
		}
	}else if( type->tid == DAO_CDATA ){
		for(i=0; i<methods->size; i++){
			DRoutine *rout = methods->items.pRout2[i];
			DaoValue *func = DaoTypeBase_FindFunction( type->typer, rout->routName );
			if( func == NULL ) goto RecordFailB;
			if( DRoutine_IsCompatible( func, rout->routType, binds ) ==0 ) goto RecordFailB;
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
		DaoInterface *super = (DaoInterface*) self->supers->items.pValue[i];
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
		inter = (DaoInterface*) pairs->items.pValue[i];
		type = (DaoType*) pairs->items.pValue[i+1];
		/*
		   printf( "%s  %s\n", inter->abtype->name->mbs, type->name->mbs );
		 */
		DaoInterface_TempBind( inter, type, binds );
	}
	for(i=0; i<N; i+=5){
		j = fails->size;
		inter = (DaoInterface*) pairs->items.pValue[i];
		type = (DaoType*) pairs->items.pValue[i+1];
		if( DMap_Find( type->interfaces, inter ) ) continue;
		methods->size = 0;
		DMap_SortMethods( inter->methods, methods );
		if( DaoInterface_CheckBind( methods, type, binds, fails ) ==0 ){
			pairs->items.pInt[i+4] = fails->size - j;
			continue;
		}

		GC_IncRC( inter );
		DMap_Insert( type->interfaces, inter, inter );
		for(j=0; j<inter->supers->size; j++){
			DaoInterface *super = (DaoInterface*) inter->supers->items.pValue[j];
			if( DMap_Find( type->interfaces, super ) ) continue;
			GC_IncRC( super );
			DMap_Insert( type->interfaces, super, super );
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
	DMap_Insert( type->interfaces, self, self );
	for(i=0; i<self->supers->size; i++){
		DaoInterface *super = (DaoInterface*) self->supers->items.pValue[i];
		if( DMap_Find( type->interfaces, super ) ) continue;
		GC_IncRC( super );
		DMap_Insert( type->interfaces, super, super );
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
void DaoMethods_Insert( DMap *methods, DRoutine *rout, DaoType *host );
void DaoInterface_DeriveMethods( DaoInterface *self )
{
	int i, k, N = self->supers->size;
	DaoInterface *super;
	DNode *it;
	for(i=0; i<N; i++){
		super = (DaoInterface*) self->supers->items.pValue[i];
		for(it=DMap_First(super->methods); it; it=DMap_Next( super->methods, it )){
			if( it->value.pValue->type == DAO_FUNCTREE ){
				DaoFunctree *meta = (DaoFunctree*) it->value.pVoid;
				for(k=0; k<meta->routines->size; k++){
					DRoutine *rout = meta->routines->items.pRout2[i];
					DaoMethods_Insert( self->methods, rout, self->abtype );
				}
			}else{
				DRoutine *rout = (DRoutine*) it->value.pVoid;
				DaoMethods_Insert( self->methods, rout, self->abtype );
			}
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
		if( it->value.pValue->type == DAO_FUNCTREE ){
			DaoFunctree *one = (DaoFunctree*) it->value.pVoid;
			n = one->routines->size;
			for(i=0; i<n; i++){
				DRoutine *rout = one->routines->items.pRout2[i];
				DString_Assign( name, rout->routName );
				DString_AppendMBS( name, " " );
				DString_Append( name, rout->routType->name );
				DMap_Insert( map, name, (void*)rout );
			}
		}else{
			DRoutine *rout = (DRoutine*) it->value.pValue;
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
		DaoClass *klass = & self->aux->xClass;
		for(i=0; i<klass->superClass->size; i++){
			DaoValue *super = klass->superClass->items.pValue[i];
			DaoType *type = NULL;
			if( super->type == DAO_CLASS ){
				type = super->xClass.objType;
			}else if( super->type == DAO_CDATA ){
				type = super->xCdata.ctype;
			}
			if( DaoType_HasInterface( type, inter ) ) return DAO_MT_SUB;
		}
	}else if( self->tid == DAO_CDATA ){
		i = 0;
		while( self->typer->supers[i] ){
			DaoType *type = self->typer->supers[i]->core->kernel->abtype;
			if( DaoType_HasInterface( type, inter ) ) return DAO_MT_SUB;
			i += 1;
		}
	}
	return 0;
}



static void DaoCdata_GetField( DaoValue *self, DaoProcess *proc, DString *name )
{
	DaoTypeBase *typer = self->xCdata.typer;
	DaoValue *p = DaoTypeBase_FindValue( typer, name );
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( p == NULL ){
		DaoValue *func = NULL;
		DString_SetMBS( proc->mbstring, "." );
		DString_Append( proc->mbstring, name );
		func = DaoTypeBase_FindFunction( typer, proc->mbstring );
		func = (DaoValue*) DRoutine_Resolve( func, self, NULL, 0, DVM_CALL );
		if( func == NULL ){
			DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, "not exist" );
			return;
		}
		func->xFunction.pFunc( proc, & self, 1 );
	}else{
		DaoProcess_PutValue( proc, p );
	}
}
static void DaoCdata_SetField( DaoValue *self, DaoProcess *proc, DString *name, DaoValue *value )
{
	DaoTypeBase *typer = self->xCdata.typer;
	DaoValue *func = NULL;
	DString_SetMBS( proc->mbstring, "." );
	DString_Append( proc->mbstring, name );
	DString_AppendMBS( proc->mbstring, "=" );
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	func = DaoTypeBase_FindFunction( typer, proc->mbstring );
	if( func == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, name->mbs );
		return;
	}
	DaoProcess_PushCallable( proc, func, self, & value, 1 );
}
static void DaoCdata_GetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid )
{
	DaoCdata *self = & self0->xCdata;
	DaoTypeBase *typer = self->typer;

	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( self->buffer && pid->type >=DAO_INTEGER && pid->type <=DAO_DOUBLE){
		int id = DaoValue_GetInteger( pid );
		self->data = self->buffer;
		if( self->size && ( id <0 || id > self->size ) ){
			DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "index out of range" );
			return;
		}
		if( self->memsize ){
			self->data = (void*)( (char*)self->buffer + id * self->memsize );
		}else{
			self->data = ((void**)self->buffer)[id];
		}
		DaoProcess_PutValue( proc, self0 );
	}else{
		DaoValue *func = NULL;
		func = DaoTypeBase_FindFunctionMBS( typer, "[]" );
		if( func == NULL ){
			DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, "" );
			return;
		}
		DaoProcess_PushCallable( proc, func, self0, & pid, 1 );
	}
}
static void DaoCdata_SetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid, DaoValue *value )
{
	DaoTypeBase *typer = self0->xCdata.typer;
	DaoValue *func = NULL;
	DaoValue *p[2];

	DString_SetMBS( proc->mbstring, "[]=" );
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	func = DaoTypeBase_FindFunction( typer, proc->mbstring );
	if( func == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, "" );
		return;
	}
	p[0] = pid;
	p[1] = value;
	DaoProcess_PushCallable( proc, func, self0, p, 2 );
}
static void DaoCdata_GetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N )
{
	DaoTypeBase *typer = self->xCdata.typer;
	DaoValue *func = NULL;
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( N == 1 ){
		DaoCdata_GetItem1( self, proc, ids[0] );
		return;
	}
	func = DaoTypeBase_FindFunctionMBS( typer, "[]" );
	if( func == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, "" );
		return;
	}
	DaoProcess_PushCallable( proc, func, self, ids, N );
}
static void DaoCdata_SetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	DaoTypeBase *typer = self->xCdata.typer;
	DaoValue *func = NULL;
	DaoValue *p[ DAO_MAX_PARAM ];
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( N == 1 ){
		DaoCdata_SetItem1( self, proc, ids[0], value );
		return;
	}
	func = DaoTypeBase_FindFunctionMBS( typer, "[]=" );
	if( func == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, "" );
		return;
	}
	memcpy( p, ids, N*sizeof(DaoValue*) );
	p[N] = value;
	DaoProcess_PushCallable( proc, func, self, p, N+1 );
}
static void DaoCdata_Print( DaoValue *self0, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	int ec;
	DaoValue *meth;
	DaoCdata *self = & self0->xCdata;
	if( self0 == self->ctype->value ){
		DaoStream_WriteString( stream, self->ctype->name );
		DaoStream_WriteMBS( stream, "[default]" );
		return;
	}
	DaoValue_Clear( & proc->stackValues[0] );
	meth = DaoTypeBase_FindFunctionMBS( self->typer, "serialize" );
	if( meth && (ec = DaoProcess_Call( proc, (DaoMethod*)meth, NULL, &self0, 1 )) ){
		DaoProcess_RaiseException( proc, ec, DString_GetMBS( proc->mbstring ) );
	}else if( meth == NULL || proc->stackValues[0] == NULL ){
		char buf[50];
		sprintf( buf, "[%p]", self );
		DaoStream_WriteString( stream, self->ctype->name );
		DaoStream_WriteMBS( stream, buf );
	}else{
		DaoValue_Print( proc->stackValues[0], proc, stream, cycData );
	}
}

void DaoTypeKernel_Delete( DaoTypeKernel *self )
{
	DNode *it;
	DMap *methods = self->methods;
	self->core->kernel = NULL;
	if( self->core == (DaoTypeCore*)(self + 1) ) self->typer->core = NULL;
	for(it=DMap_First(methods); it; it=DMap_Next(methods,it)) GC_DecRC( it->value.pValue );
	if( self->values ) DMap_Delete( self->values );
	if( self->methods ) DMap_Delete( self->methods );
	if( self->instances ) DMap_Delete( self->instances );
	dao_free( self );
}

DaoTypeBase typeKernelTyper =
{
	"TypeKernel", & baseCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoTypeKernel_Delete, NULL
};

DaoTypeKernel* DaoTypeKernel_New( DaoTypeBase *typer )
{
	int extra = typer && typer->core ? 0 : sizeof(DaoCdataCore);
	DaoTypeKernel *self = (DaoTypeKernel*) dao_calloc( 1, sizeof(DaoTypeKernel) + extra );
	DaoValue_Init( self, DAO_TYPEKERNEL );
	if( typer ) self->typer = typer;
	if( typer && typer->core ) self->core = typer->core;
	if( self->core == NULL ){
		self->core = (DaoTypeCore*)(self + 1);
		self->core->GetField = DaoCdata_GetField;
		self->core->SetField = DaoCdata_SetField;
		self->core->GetItem = DaoCdata_GetItem;
		self->core->SetItem = DaoCdata_SetItem;
		self->core->Print = DaoCdata_Print;
		self->core->Copy = DaoValue_NoCopy;
	}
	self->core->kernel = self;
	return self;
}


DaoValue* DaoTypeBase_FindFunction( DaoTypeBase *self, DString *name )
{
	DNode *node;
	DaoTypeKernel *kernel = self->core->kernel;
	if( kernel == NULL ) return NULL;
	if( kernel->methods == NULL ){
		DaoNamespace_SetupMethods( kernel->nspace, self );
		if( kernel->methods == NULL ) return NULL;
	}
	node = DMap_Find( kernel->methods, name );
	if( node ) return node->value.pValue;
	return NULL;
}
DaoValue* DaoTypeBase_FindFunctionMBS( DaoTypeBase *self, const char *name )
{
	DString mbs = DString_WrapMBS( name );
	return DaoTypeBase_FindFunction( self, & mbs );
}
DaoValue* DaoTypeBase_FindValue( DaoTypeBase *self, DString *name )
{
	DaoValue *func = DaoTypeBase_FindFunction( self, name );
	DNode *node;
	if( func ) return func;
	return DaoTypeBase_FindValueOnly( self, name );
}
DaoValue* DaoTypeBase_FindValueOnly( DaoTypeBase *self, DString *name )
{
	DaoTypeKernel *kernel = self->core->kernel;
	DaoValue *value = NULL;
	DNode *node;
	if( kernel == NULL ) return NULL;
	/* mainly for C data type: */
	if( kernel->abtype && kernel->abtype->aux ){
		if( DString_EQ( name, kernel->abtype->name ) ) value = kernel->abtype->aux;
	}
	if( kernel->values == NULL ){
		DaoNamespace_SetupValues( kernel->nspace, self );
		if( kernel->values == NULL ) return value;
	}
	node = DMap_Find( kernel->values, name );
	if( node ) return node->value.pValue;
	return value;
}
