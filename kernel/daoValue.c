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
#include"string.h"
#include"assert.h"
#include"math.h"

#include"daoGC.h"
#include"daoType.h"
#include"daoStdtype.h"
#include"daoStream.h"
#include"daoRoutine.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoNumtype.h"
#include"daoNamespace.h"
#include"daoVmspace.h"
#include"daoParser.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoValue.h"

#ifdef DAO_WITH_NUMARRAY
int DaoArray_Compare( DaoArray *x, DaoArray *y );
#endif

#define float_compare( x, y ) (x==y ? 0 : (x<y ? -1 : 1))

int DaoEnum_Compare( DaoEnum *L, DaoEnum *R )
{
	DaoEnum E;
	DNode *N = NULL;
	DMap *ML = L->etype->mapNames;
	DMap *MR = R->etype->mapNames;
	uchar_t FL = L->etype->flagtype;
	uchar_t FR = R->etype->flagtype;
	char SL = L->etype->name->mbs[0];
	char SR = R->etype->name->mbs[0];
	if( L->etype == R->etype ){
		return L->value == R->value ? 0 : ( L->value < R->value ? -1 : 1 );
	}else if( SL == '$' && SR == '$' && FL == 0 && FR == 0 ){
		return DString_Compare( L->etype->name, R->etype->name );
	}else if( SL == '$' && SR == '$' ){
		if( L->etype->mapNames->size != R->etype->mapNames->size ){
			return (int)L->etype->mapNames->size - (int)R->etype->mapNames->size;
		}else{
			for(N=DMap_First(ML);N;N=DMap_Next(ML,N)){
				if( DMap_Find( MR, N->key.pVoid ) ==0 ) return -1;
			}
			return 0;
		}
	}else if( SL == '$' ){
		E.etype = R->etype;
		E.value = R->value;
		DaoEnum_SetValue( & E, L, NULL );
		return E.value == R->value ? 0 : ( E.value < R->value ? -1 : 1 );
	}else if( SR == '$' ){
		E.etype = L->etype;
		E.value = L->value;
		DaoEnum_SetValue( & E, R, NULL );
		return L->value == E.value ? 0 : ( L->value < E.value ? -1 : 1 );
	}else{
		return L->value == R->value ? 0 : ( L->value < R->value ? -1 : 1 );
	}
}
int DaoTuple_Compare( DaoTuple *lt, DaoTuple *rt )
{
	int i, lb, rb, res;
	if( lt->items->size < rt->items->size ) return -1;
	if( lt->items->size > rt->items->size ) return 1;

	for(i=0; i<lt->items->size; i++){
		DaoValue *lv = lt->items->items.pValue[i];
		DaoValue *rv = rt->items->items.pValue[i];
		int lb = lv ? lv->type : 0;
		int rb = rv ? rv->type : 0;
		if( lb == rb && lb == DAO_TUPLE ){
			res = DaoValue_Compare( lv, rv );
			if( res != 0 ) return res;
		}else if( lb != rb || lb ==0 || lb >= DAO_ARRAY || lb == DAO_COMPLEX ){
			if( lv < rv ){
				return -1;
			}else if( lv > rv ){
				return 1;
			}
		}else{
			res = DaoValue_Compare( lv, rv );
			if( res != 0 ) return res;
		}
	}
	return 0;
}
int DaoList_Compare( DaoList *list1, DaoList *list2 )
{
	DaoValue **d1 = list1->items->items.pValue;
	DaoValue **d2 = list2->items->items.pValue;
	int size1 = list1->items->size;
	int size2 = list2->items->size;
	int min = size1 < size2 ? size1 : size2;
	int i = 0, cmp = 0;
	/* find the first unequal items */
	while( i < min && (cmp = DaoValue_Compare(*d1, *d2)) ==0 ) i++, d1++, d2++;
	if( i < min ) return cmp;
	if( size1 == size2  ) return 0;
	return size1 > size2 ? 1 : -1;
}
int DaoValue_Compare( DaoValue *left, DaoValue *right )
{
	double L, R;
	int res = 0;
	if( left == right ) return 0;
	if( left == NULL || right == NULL ) return left < right ? -1 : 1;
	if( left->type != right->type ){
		res = left->type < right->type ? -1 : 1;
		if( right->type == DAO_TUPLE && right->xTuple.items->size == 2 ){
			res = DaoValue_Compare( left, right->xTuple.items->items.pValue[0] );
			if( res <= 0 ) return res;
			res = DaoValue_Compare( left, right->xTuple.items->items.pValue[1] );
			if( res >= 0 ) return res;
			return 0;
		}
		if( left->type < DAO_INTEGER || left->type > DAO_DOUBLE ) return res;
		if( right->type < DAO_INTEGER || right->type > DAO_DOUBLE ) return res;
		L = DaoValue_GetDouble( left );
		R = DaoValue_GetDouble( right );
		return L==R ? 0 : ( L<R ? -1 : 1 );
	}
	switch( left->type ){
	case DAO_NULL : return 0;
	case DAO_INTEGER : return left->xInteger.value - right->xInteger.value;
	case DAO_FLOAT   : return float_compare( left->xFloat.value, right->xFloat.value );
	case DAO_DOUBLE  : return float_compare( left->xDouble.value, right->xDouble.value );
	case DAO_LONG    : return DLong_Compare( left->xLong.value, right->xLong.value );
	case DAO_STRING  : return DString_Compare( left->xString.data, right->xString.data );
	case DAO_ENUM    : return DaoEnum_Compare( & left->xEnum, & right->xEnum );
	case DAO_TUPLE   : return DaoTuple_Compare( & left->xTuple, & right->xTuple );
	case DAO_LIST    : return DaoList_Compare( & left->xList, & right->xList );
	case DAO_CTYPE :
	case DAO_CDATA : return (int)( (size_t)left->xCdata.data - (size_t)right->xCdata.data ); 
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY   : return DaoArray_Compare( & left->xArray, & right->xArray );
#endif
	}
	return 1;
}
int DaoValue_IsZero( DaoValue *self )
{
	if( self == NULL ) return 1;
	switch( self->type ){
	case DAO_NULL : return 1;
	case DAO_INTEGER : return self->xInteger.value == 0;
	case DAO_FLOAT  : return self->xFloat.value == 0.0;
	case DAO_DOUBLE : return self->xDouble.value == 0.0;
	case DAO_COMPLEX : return self->xComplex.value.real == 0.0 && self->xComplex.value.imag == 0.0;
	case DAO_LONG : return self->xLong.value->size == 0 && self->xLong.value->data[0] ==0;
	case DAO_ENUM : return self->xEnum.value == 0;
	}
	return 0;
}
llong_t DaoValue_GetLongLong( DaoValue *self )
{
	return (llong_t) DaoValue_GetDouble( self );
}
llong_t DString_ToInteger( DString *self )
{
	if( self->mbs ) return strtoll( self->mbs, NULL, 0 );
	return wcstoll( self->wcs, NULL, 0 );
}
double DString_ToDouble( DString *self )
{
	if( self->mbs ) return strtod( self->mbs, 0 );
	return wcstod( self->wcs, 0 );
}
llong_t DaoValue_GetInteger( DaoValue *self )
{
	switch( self->type ){
	case DAO_INTEGER : return self->xInteger.value;
	case DAO_FLOAT   : return self->xFloat.value;
	case DAO_DOUBLE  : return self->xDouble.value;
	case DAO_COMPLEX : return self->xComplex.value.real;
	case DAO_LONG    : return DLong_ToInteger( self->xLong.value );
	case DAO_STRING  : return DString_ToInteger( self->xString.data );
	case DAO_ENUM    : return self->xEnum.value;
	default : break;
	}
	return 0;
}
float DaoValue_GetFloat( DaoValue *self )
{
	return (float) DaoValue_GetDouble( self );
}
double DaoValue_GetDouble( DaoValue *self )
{
	DString *str;
	switch( self->type ){
	case DAO_NULL    : return 0;
	case DAO_INTEGER : return self->xInteger.value;
	case DAO_FLOAT   : return self->xFloat.value;
	case DAO_DOUBLE  : return self->xDouble.value;
	case DAO_COMPLEX : return self->xComplex.value.real;
	case DAO_LONG    : return DLong_ToDouble( self->xLong.value );
	case DAO_STRING  : return DString_ToDouble( self->xString.data );
	case DAO_ENUM    : return self->xEnum.value;
	default : break;
	}
	return 0.0;
}
int DaoValue_IsNumber( DaoValue *self )
{
	if( self->type >= DAO_INTEGER && self->type <= DAO_DOUBLE ) return 1;
	return 0;
}
static void DaoValue_BasicPrint( DaoValue *self, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
	DaoType *type = DaoNameSpace_GetType( ctx->nameSpace, self );
	if( self->type <= DAO_STREAM )
		DaoStream_WriteMBS( stream, coreTypeNames[ self->type ] );
	else
		DaoStream_WriteMBS( stream, DaoValue_GetTyper( self )->name );
	if( self->type == DAO_NULL ) return;
	if( self->type == DAO_TYPE ){
		DaoStream_WriteMBS( stream, "<" );
		DaoStream_WriteMBS( stream, self->xType.name->mbs );
		DaoStream_WriteMBS( stream, ">" );
	}
	DaoStream_WriteMBS( stream, "_" );
	DaoStream_WriteInt( stream, self->type );
	DaoStream_WriteMBS( stream, "_" );
	DaoStream_WritePointer( stream, self );
	if( self->type < DAO_STREAM ) return;
	if( type && self == type->value ) DaoStream_WriteMBS( stream, "[default]" );
}
void DaoValue_Print( DaoValue *self, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
	DString *name;
	DaoTypeBase *typer;
	DMap *cd = cycData;
	if( self == NULL ){
		DaoStream_WriteMBS( stream, "null[0x0]" );
		return;
	}
	if( cycData == NULL ) cycData = DMap_New(0,0);
	switch( self->type ){
	case DAO_INTEGER :
		DaoStream_WriteInt( stream, self->xInteger.value ); break;
	case DAO_FLOAT   :
		DaoStream_WriteFloat( stream, self->xFloat.value ); break;
	case DAO_DOUBLE  :
		DaoStream_WriteFloat( stream, self->xDouble.value ); break;
	case DAO_COMPLEX :
		DaoStream_WriteFloat( stream, self->xComplex.value.real );
		if( self->xComplex.value.imag >= -0.0 ) DaoStream_WriteMBS( stream, "+" );
		DaoStream_WriteFloat( stream, self->xComplex.value.imag );
		DaoStream_WriteMBS( stream, "$" );
		break;
	case DAO_LONG :
		name = DString_New(1);
		DLong_Print( self->xLong.value, name );
		DaoStream_WriteString( stream, name );
		DString_Delete( name );
		break;
	case DAO_ENUM  :
		name = DString_New(1);
		DaoEnum_MakeName( & self->xEnum, name );
		DaoStream_WriteMBS( stream, name->mbs );
		DaoStream_WriteMBS( stream, "=" );
		DaoStream_WriteInt( stream, self->xEnum.value );
		DString_Delete( name );
		break;
	case DAO_STRING  :
		DaoStream_WriteString( stream, self->xString.data ); break;
	default :
		typer = DaoVmSpace_GetTyper( self->type );
		if( typer->priv->Print == DaoValue_Print ){
			DaoValue_BasicPrint( self, ctx, stream, cycData );
			break;
		}
		typer->priv->Print( self, ctx, stream, cycData );
		break;
	}
	if( cycData != cd ) DMap_Delete( cycData );
}
complex16 DaoValue_GetComplex( DaoValue *self )
{
	complex16 com = { 0.0, 0.0 };
	switch( self->type ){
	case DAO_INTEGER : com.real = self->xInteger.value; break;
	case DAO_FLOAT   : com.real = self->xFloat.value; break;
	case DAO_DOUBLE  : com.real = self->xDouble.value; break;
	case DAO_COMPLEX : com = self->xComplex.value; break;
	default : break;
	}
	return com;
}
DLong* DaoValue_GetLong( DaoValue *self, DLong *lng )
{
	switch( self->type ){
	case DAO_INTEGER : DLong_FromInteger( lng, DaoValue_GetInteger( self ) ); break;
	case DAO_FLOAT   : 
	case DAO_DOUBLE  : DLong_FromDouble( lng, DaoValue_GetDouble( self ) ); break;
	case DAO_COMPLEX : DLong_FromDouble( lng, self->xComplex.value.real ); break;
	case DAO_LONG    : DLong_Move( lng, self->xLong.value ); break;
	case DAO_STRING  : DLong_FromString( lng, self->xString.data ); break;
	default : break; /* TODO list array? */
	}
	return lng;
}
DString* DaoValue_GetString( DaoValue *self, DString *str )
{
	char chs[100] = {0};
	DString_Clear( str );
	switch( self->type ){
	case DAO_INTEGER : sprintf( chs, ( sizeof(dint) == 4 )? "%li" : "%lli", self->xInteger.value ); break;
	case DAO_FLOAT   : sprintf( chs, "%g", self->xFloat.value ); break;
	case DAO_DOUBLE  : sprintf( chs, "%g", self->xDouble.value ); break;
	case DAO_COMPLEX : sprintf( chs, ( self->xComplex.value.imag < 0) ? "%g%g$" : "%g+%g$", self->xComplex.value.real, self->xComplex.value.imag ); break;
	case DAO_LONG  : DLong_Print( self->xLong.value, str ); break;
	case DAO_ENUM : DaoEnum_MakeName( & self->xEnum, str ); break;
	case DAO_STRING : DString_Assign( str, self->xString.data ); break;
	default : break;
	}
	if( self->type <= DAO_COMPLEX ) DString_SetMBS( str, chs );
	return str;
}
void DaoValue_MarkConst( DaoValue *self )
{
	DMap *map;
	DNode *it;
	int i, n;
	if( self == NULL || self->xNull.konst ) return;
	self->xNull.konst = 1;
	switch( self->type ){
	case DAO_LIST :
		for(i=0; i<self->xList.items->size; i++)
			DaoValue_MarkConst( self->xList.items->items.pValue[i] );
		break;
	case DAO_TUPLE :
		for(i=0; i<self->xTuple.items->size; i++)
			DaoValue_MarkConst( self->xTuple.items->items.pValue[i] );
		break;
	case DAO_MAP :
		map = self->xMap.items;
		for(it=DMap_First( map ); it != NULL; it = DMap_Next(map, it) ){
			DaoValue_MarkConst( it->key.pValue );
			DaoValue_MarkConst( it->value.pValue );
		}
		break;
	case DAO_OBJECT :
		n = self->xObject.myClass->objDataDefault->size;
		for(i=1; i<n; i++) DaoValue_MarkConst( self->xObject.objValues[i] );
		if( self->xObject.superObject == NULL ) break;
		for(i=0; i<self->xObject.superObject->size; i++){
			DaoValue *obj = self->xObject.superObject->items.pValue[i];
			if( obj == NULL || obj->type != DAO_OBJECT ) continue;
			DaoValue_MarkConst( obj );
		}
		break;
	default : break;
	}
}

void DaoValue_Clear( DaoValue **self )
{
	DaoValue *value = *self;
	*self = NULL;
	GC_DecRC( value );
}

void DaoObject_CopyData( DaoObject *self, DaoObject *from, DaoContext *ctx, DMap *cyc );

DaoValue* DaoValue_SimpleCopyWithType( DaoValue *self, DaoType *tp )
{
	size_t i;

	if( self == NULL ) return null;
#ifdef DAO_WITH_NUMARRAY
	if( self->type == DAO_ARRAY && self->xArray.reference ){
		DaoArray_Sliced( (DaoArray*)self );
		return self;
	}
#endif
	if( self->xNull.trait & DAO_DATA_NOCOPY ) return self;
	if( tp && tp->tid >= DAO_INTEGER && tp->tid <= DAO_DOUBLE ){
		double value = DaoValue_GetDouble( self );
		switch( tp->tid ){
		case DAO_INTEGER : return (DaoValue*) DaoInteger_New( (dint) value );
		case DAO_FLOAT   : return (DaoValue*) DaoFloat_New( value );
		case DAO_DOUBLE  : return (DaoValue*) DaoDouble_New( value );
		}
	}
	switch( self->type ){
	case DAO_NULL : return self;
	case DAO_INTEGER : return (DaoValue*) DaoInteger_New( self->xInteger.value );
	case DAO_FLOAT   : return (DaoValue*) DaoFloat_New( self->xFloat.value );
	case DAO_DOUBLE  : return (DaoValue*) DaoDouble_New( self->xDouble.value );
	case DAO_COMPLEX : return (DaoValue*) DaoComplex_New( self->xComplex.value );
	case DAO_LONG    : return (DaoValue*) DaoLong_Copy( & self->xLong );
	case DAO_STRING  : return (DaoValue*) DaoString_Copy( & self->xString );
	case DAO_ENUM    : return (DaoValue*) DaoEnum_Copy( & self->xEnum );
	}
	if( self->xNull.konst == 0 ) return self;
	switch( self->type ){
	case DAO_LIST :
		{
			DaoList *list = (DaoList*) self;
			DaoList *copy = DaoList_New();
			/* no detailed checking of type matching, must be ensured by caller */
			copy->unitype = (tp && tp->tid == DAO_LIST) ? tp : list->unitype;
			GC_IncRC( copy->unitype );
			DArray_Resize( copy->items, list->items->size, NULL );
			for(i=0; i<list->items->size; i++)
				DaoList_SetItem( copy, list->items->items.pValue[i], i );
			return (DaoValue*)copy;
		}
	case DAO_MAP :
		{
			DaoMap *map = (DaoMap*) self;
			DaoMap *copy = DaoMap_New( map->items->hashing );
			DNode *node = DMap_First( map->items );
			copy->unitype = (tp && tp->tid == DAO_MAP) ? tp : map->unitype;
			GC_IncRC( copy->unitype );
			for( ; node !=NULL; node = DMap_Next(map->items, node ))
				DaoMap_Insert( copy, node->key.pValue, node->value.pValue );
			return (DaoValue*)copy;
		}
	case DAO_TUPLE :
		{
			DaoTuple *tuple = (DaoTuple*) self;
			DaoTuple *copy = DaoTuple_New( tuple->items->size );
			copy->unitype = (tp && tp->tid == DAO_TUPLE) ? tp : tuple->unitype;
			GC_IncRC( copy->unitype );
			for(i=0; i<tuple->items->size; i++)
				DaoTuple_SetItem( copy, tuple->items->items.pValue[i], i );
			return (DaoValue*) copy;
		}
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		{
			DaoArray *array = (DaoArray*) self;
			DaoArray *copy = DaoArray_New( array->numType );
			copy->unitype = array->unitype;
			if( tp && tp->tid == DAO_ARRAY && tp->nested->size ){
				int nt = tp->nested->items.pType[0]->tid;
				if( nt >= DAO_INTEGER && nt <= DAO_COMPLEX ){
					copy->unitype = tp;
					copy->numType = nt;
				}
			}
			GC_IncRC( copy->unitype );
			DaoArray_ResizeArray( copy, array->dims->items.pSize, array->dims->size );
			DaoArray_CopyArray( copy, array );
			return (DaoValue*) copy;
		}
#endif
	default : break;
	}
	if( self->type == DAO_OBJECT ){
		DaoObject *s = (DaoObject*) self;
		DaoObject *t = DaoObject_New( s->myClass, NULL, 0 );
		DMap *cyc = DHash_New(0,0);
		DaoObject_CopyData( t, s, NULL, cyc );
		return (DaoValue*) t;
	}
	return self;
}
DaoValue* DaoValue_SimpleCopy( DaoValue *self )
{
	return DaoValue_SimpleCopyWithType( self, NULL );
}

static void DaoValue_CopyExt( DaoValue *src, DaoValue **dest, int copy )
{
	DaoValue *dest2 = *dest;
	if( *dest == NULL ){
		if( copy ) src = DaoValue_SimpleCopyWithType( src, NULL );
		GC_IncRC( src );
		*dest = src;
		return;
	}
	if( src == *dest ) return;
	if( src->type != dest2->type || src->type > DAO_ENUM ){
		if( copy ) src = DaoValue_SimpleCopyWithType( src, NULL );
		GC_ShiftRC( src, dest2 );
		*dest = src;
		return;
	}
	switch( src->type ){
	case DAO_ENUM    : 
		DaoEnum_SetType( & dest2->xEnum, src->xEnum.etype );
		DaoEnum_SetValue( & dest2->xEnum, & src->xEnum, NULL );
		break;
	case DAO_INTEGER : dest2->xInteger.value = src->xInteger.value; break;
	case DAO_FLOAT   : dest2->xFloat.value = src->xFloat.value; break;
	case DAO_DOUBLE  : dest2->xDouble.value = src->xDouble.value; break;
	case DAO_COMPLEX : dest2->xComplex.value = src->xComplex.value; break;
	case DAO_LONG    : DLong_Move( dest2->xLong.value, src->xLong.value ); break;
	case DAO_STRING  : DString_Assign( dest2->xString.data, src->xString.data ); break;
	}
}
void DaoValue_Copy( DaoValue *src, DaoValue **dest )
{
	DaoValue_CopyExt( src, dest, 1 );
}
void DaoValue_SetType( DaoValue *to, DaoType *tp )
{
	DaoType *tp2;
	DNode *it;
	if( to->type != tp->tid && tp->tid != DAO_ANY ) return;
	/* XXX compatible types? list<int> list<float> */
	switch( to->type ){
	case DAO_LIST :
		/* v : any = {}, v->unitype should be list<any> */
		if( tp->tid == DAO_ANY ) tp = dao_list_any;
		tp2 = to->xList.unitype;
		if( tp2 && !(tp2->attrib & DAO_TYPE_EMPTY) ) break;
		GC_ShiftRC( tp, to->xList.unitype );
		to->xList.unitype = tp;
		break;
	case DAO_MAP :
		if( tp->tid == DAO_ANY ) tp = dao_map_any;
		tp2 = to->xMap.unitype;
		if( tp2 && !(tp2->attrib & DAO_TYPE_EMPTY) ) break;
		GC_ShiftRC( tp, to->xMap.unitype );
		to->xMap.unitype = tp;
		break;
	case DAO_TUPLE :
		tp2 = to->xTuple.unitype;
		if( tp->nested->size ==0 ) break; /* not to the generic tuple type */
		if( tp2 == NULL || tp2->mapNames == NULL || tp2->mapNames->size ==0 ){
			GC_ShiftRC( tp, to->xTuple.unitype );
			to->xTuple.unitype = tp;
			break;
		}
		if( tp->mapNames == NULL || tp->mapNames->size ) break;
		for(it=DMap_First(tp2->mapNames); it!=NULL; it=DMap_Next(tp2->mapNames, it)){
			if( DMap_Find( tp->mapNames, it->key.pVoid ) == NULL ) break;
		}
		if( it ) break;
		GC_ShiftRC( tp, to->xTuple.unitype );
		to->xTuple.unitype = tp;
		break;
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		/*XXX array<int> array<float>*/
		if( tp->tid == DAO_ANY ) tp = dao_array_any;
		tp2 = to->xArray.unitype;
		if( tp2 && !(tp2->attrib & DAO_TYPE_EMPTY) ) break;
		GC_ShiftRC( tp, to->xArray.unitype );
		to->xArray.unitype = tp;
		break;
#endif
	default : break;
	}
}
static int DaoValue_MoveVariant( DaoValue *src, DaoValue **dest, DaoType *tp )
{
	DaoType *itp = NULL;
	int j, k, mt = 0;
	for(j=0; j<tp->nested->size; j++){
		k = DaoType_MatchValue( tp->nested->items.pType[j], src, NULL );
		if( k > mt ){
			itp = tp->nested->items.pType[j];
			mt = k;
		}
	}
	if( itp == NULL ) return 0;
	return DaoValue_Move( src, dest, itp );
}
int DaoValue_Move4( DaoValue *src, DaoValue **dest, DaoType *tp )
{
	int tm = 1;
	if( tp->tid == DAO_FUNCTREE && src->type == DAO_FUNCTREE ){
		/* XXX pair<objetp,routine<...>> */
		if( tp != src->xFunctree.unitype ) return 0;
	}else if( (tp->tid == DAO_OBJECT || tp->tid == DAO_CDATA) && src->type == DAO_OBJECT){
		if( src->xObject.myClass != & tp->aux->xClass ){
			src = DaoObject_MapThisObject( src->xObject.that, tp );
			tm = (src != NULL);
		}
	}else if( src->type == DAO_CLASS && tp->tid == DAO_CLASS && src->xClass.typeHolders ){
		if( DMap_Find( src->xClass.instanceClasses, tp->aux->xClass.className ) ){
			src = tp->aux;
			tm = DAO_MT_SUB;
		}
	}else{
		tm = DaoType_MatchValue( tp, src, NULL );
	}
#if 0
	if( tm ==0 ){
		printf( "tp = %p; src = %p, type = %i\n", tp, src, src->type );
		printf( "tp: %s %i %i\n", tp->name->mbs, tp->tid, tm );
		if( src->type == DAO_TUPLE ) printf( "%p\n", src->xTuple.unitype );
	}
	printf( "src->type = %p\n", src );
#endif
	if( tm == 0 ) return 0;
	/* composite known types must match exactly. example,
	 * where it will not work if composite types are allowed to match loosely.
	 * d : list<list<int>> = {};
	 * e : list<float> = { 1.0 };
	 * d.append( e );
	 *
	 * but if d is of type list<list<any>>,
	 * the matching do not necessary to be exact.
	 */
	src = DaoValue_SimpleCopyWithType( src, tp );
	GC_ShiftRC( src, *dest );
	*dest = src;
	if( src->type == DAO_TUPLE && src->xTuple.unitype != tp && tm >= DAO_MT_SIM ){
		DaoTuple *tuple;
		DaoType **item_types = tp->nested->items.pType;
		DaoType *totype = src->xTuple.unitype;
		DaoValue **data = src->xTuple.items->items.pValue;
		DMap *names = totype ? totype->mapNames : NULL;
		DNode *node, *search;
		int i, T = tp->nested->size;
		/* auto-cast tuple type, on the following conditions:
		 * (1) the item values of "dest" must match exactly to the item types of "tp";
		 * (2) "tp->mapNames" must contain "(*dest)->xTuple.unitype->mapNames"; */
		for(i=0; i<T; i++){
			DaoType *it = item_types[i];
			if( it->tid == DAO_PAR_NAMED ) it = & it->aux->xType;
			tm = DaoType_MatchValue( it, data[i], NULL );
			if( tm < DAO_MT_SIM ) return 1;
		}
		/* casting is not necessary if the tuple's field names are a superset of the
		 * field names of the target type: */
		if( tp->mapNames == NULL || tp->mapNames->size ==0 ) return 1;
		if( names ){
			int count = 0;
			for(node=DMap_First(names); node; node=DMap_Next(names,node)){
				search = DMap_Find( tp->mapNames, node->key.pVoid );
				if( search && node->value.pInt != search->value.pInt ) return 0;
				count += search != NULL;
			}
			/* be superset of the field names of the target type: */
			if( count == tp->mapNames->size ) return 1;
		}
		tuple = DaoTuple_New( T );
		for(i=0; i<T; i++){
			DaoType *it = item_types[i];
			if( it->tid == DAO_PAR_NAMED ) it = & it->aux->xType;
			DaoValue_Move( data[i], tuple->items->items.pValue+i, it );
		}
		GC_IncRC( tp );
		tuple->unitype = tp;
		GC_ShiftRC( tuple, *dest );
		*dest = (DaoValue*) tuple;
	}else if( tp && ! ( tp->attrib & DAO_TYPE_EMPTY ) && tp->tid == src->type ){
#if 0
		//int defed = DString_FindChar( tp->name, '@', 0 ) == MAXSIZE;
		//defed &= DString_FindChar( tp->name, '?', 0 ) == MAXSIZE;
#endif
		DaoValue_SetType( src, tp );
	}
	return 1;
}
int DaoValue_Move( DaoValue *S, DaoValue **D, DaoType *T )
{
	DaoValue *D2;
	if( S == NULL ){
		GC_DecRC( *D );
		*D = NULL;
		return 0;
	}
	if( T == NULL ){
		DaoValue_Copy( S, D );
		return 1;
	}
	switch( T->tid ){
	case DAO_NULL :
	case DAO_INITYPE :
		DaoValue_Copy( S, D );
		return 1;
	case DAO_ANY :
		DaoValue_Copy( S, D );
		DaoValue_SetType( *D, T );
		return 1;
	case DAO_VALTYPE :
		if( DaoValue_Compare( S, T->aux ) !=0 ) return 0;
		DaoValue_Copy( S, D );
		return 1;
	case DAO_VARIANT :
		return DaoValue_MoveVariant( S, D, T );
	default : break;
	}
	D2 = *D;
	if( D2 == NULL || D2->type != T->tid ) return DaoValue_Move4( S, D, T );

	switch( (S->type << 8) | T->tid ){
	case (DAO_STRING<<8)|DAO_STRING :
		DString_Assign( D2->xString.data, S->xString.data );
		break;
	case (DAO_ENUM<<8)|DAO_ENUM :
		DaoEnum_SetType( & D2->xEnum, T );
		DaoEnum_SetValue( & D2->xEnum, & S->xEnum, NULL );
		break;
	case (DAO_INTEGER<<8)|DAO_INTEGER : D2->xInteger.value = S->xInteger.value; break;
	case (DAO_INTEGER<<8)|DAO_FLOAT   : D2->xFloat.value   = S->xInteger.value; break;
	case (DAO_INTEGER<<8)|DAO_DOUBLE  : D2->xDouble.value  = S->xInteger.value; break;
	case (DAO_FLOAT  <<8)|DAO_INTEGER : D2->xInteger.value = S->xFloat.value; break;
	case (DAO_FLOAT  <<8)|DAO_FLOAT   : D2->xFloat.value   = S->xFloat.value; break;
	case (DAO_FLOAT  <<8)|DAO_DOUBLE  : D2->xDouble.value  = S->xFloat.value; break;
	case (DAO_DOUBLE <<8)|DAO_INTEGER : D2->xInteger.value = S->xDouble.value; break;
	case (DAO_DOUBLE <<8)|DAO_FLOAT   : D2->xFloat.value   = S->xDouble.value; break;
	case (DAO_DOUBLE <<8)|DAO_DOUBLE  : D2->xDouble.value  = S->xDouble.value; break;
	case (DAO_COMPLEX<<8)|DAO_COMPLEX : D2->xComplex.value = S->xComplex.value; break;
	case (DAO_LONG<<8)|DAO_LONG : DLong_Move( D2->xLong.value, S->xLong.value ); break;
	default : return DaoValue_Move4( S, D, T );
	}
	return 1;
}
int DaoValue_Move2( DaoValue *S, DaoValue **D, DaoType *T )
{
	int rc = DaoValue_Move( S, D, T );
	if( rc == 0 || T == NULL ) return rc;
	if( S->type <= DAO_STREAM || S->type != T->tid ) return rc;
	if( S->type == DAO_CDATA ){
		if( S->xCdata.data == NULL ) rc = 0;
	}else{
		if( S == T->value ) rc = 0;
	}
	return rc;
}

int DaoValue_Type( DaoValue *self )
{
	return self->type;
}
DaoValue* DaoValue_NewInteger( dint v )
{
	DaoInteger *res = DaoInteger_New( v );
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewFloat( float v )
{
	DaoFloat *res = DaoFloat_New( v );
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewDouble( double v )
{
	DaoDouble *res = DaoDouble_New( v );
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewMBString( const char *s, int n )
{
	DaoString *res = DaoString_New(1);
	if( n ){
		DString_SetDataMBS( res->data, s, n );
	}else{
		DString_SetMBS( res->data, s );
	}
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewWCString( const wchar_t *s, int n )
{
	DaoString *res = DaoString_New(0);
	if( n ){
		DString_Resize( res->data, n );
		memcpy( res->data->wcs, s, n * sizeof(wchar_t) );
	}else{
		DString_SetWCS( res->data, s );
	}
	GC_IncRC( res );
	return (DaoValue*) res;
}
#ifdef DAO_WITH_NUMARRAY
DaoValue* DaoValue_NewVectorB( char *s, int n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	DaoArray_SetVectorB( res, s, n );
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewVectorUB( unsigned char *s, int n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	DaoArray_SetVectorUB( res, s, n );
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewVectorS( short *s, int n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	DaoArray_SetVectorS( res, s, n );
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewVectorUS( unsigned short *s, int n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	DaoArray_SetVectorUS( res, s, n );
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewVectorI( int *s, int n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( n == 0 ){
		DaoArray_UseData( res, s );
	}else{
		DaoArray_SetVectorI( res, s, n );
	}
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewVectorUI( unsigned int *s, int n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( n == 0 ){
		DaoArray_UseData( res, s );
	}else{
		DaoArray_SetVectorUI( res, s, n );
	}
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewVectorF( float *s, int n )
{
	DaoArray *res = DaoArray_New( DAO_FLOAT );
	if( n == 0 ){
		DaoArray_UseData( res, s );
	}else{
		DaoArray_SetVectorF( res, s, n );
	}
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewVectorD( double *s, int n )
{
	DaoArray *res = DaoArray_New( DAO_DOUBLE );
	if( n == 0 ){
		DaoArray_UseData( res, s );
	}else{
		DaoArray_SetVectorD( res, s, n );
	}
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewMatrixB( signed char **s, int n, int m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	DaoArray_SetMatrixB( res, s, n, m );
	GC_IncRC( res );
	return (DaoValue*) res;
}
extern void DaoArray_SetMatrixUB( DaoArray *self, unsigned char **mat, int N, int M );
extern void DaoArray_SetMatrixUS( DaoArray *self, unsigned short **mat, int N, int M );
extern void DaoArray_SetMatrixUI( DaoArray *self, unsigned int **mat, int N, int M );
DaoValue* DaoValue_NewMatrixUB( unsigned char **s, int n, int m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	GC_IncRC( res );
	DaoArray_SetMatrixUB( res, s, n, m );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewMatrixS( short **s, int n, int m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	DaoArray_SetMatrixS( res, s, n, m );
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewMatrixUS( unsigned short **s, int n, int m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	GC_IncRC( res );
	DaoArray_SetMatrixUS( res, s, n, m );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewMatrixI( int **s, int n, int m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	DaoArray_SetMatrixI( res, s, n, m );
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewMatrixUI( unsigned int **s, int n, int m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	GC_IncRC( res );
	DaoArray_SetMatrixUI( res, s, n, m );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewMatrixF( float **s, int n, int m )
{
	DaoArray *res = DaoArray_New( DAO_FLOAT );
	DaoArray_SetMatrixF( res, s, n, m );
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewMatrixD( double **s, int n, int m )
{
	DaoArray *res = DaoArray_New( DAO_DOUBLE );
	DaoArray_SetMatrixD( res, s, n, m );
	GC_IncRC( res );
	return (DaoValue*) res;
}
DaoValue* DaoValue_NewBuffer( void *p, int n )
{
	DaoArray *res = DaoArray_New(0);
	DaoArray_SetBuffer( res, p, n );
	GC_IncRC( res );
	return (DaoValue*) res;
}
#else
static DaoValue NumArrayDisabled()
{
	printf( "Error: numeric array is disabled!\n" );
	return daoNullValue;
}
DaoValue* DaoValue_NewVectorB( char *s, int n )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewVectorUB( unsigned char *s, int n )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewVectorS( short *s, int n )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewVectorUS( unsigned short *s, int n )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewVectorI( int *s, int n )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewVectorUI( unsigned int *s, int n )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewVectorF( float *s, int n )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewVectorD( double *s, int n )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewMatrixB( signed char **s, int n, int m )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewMatrixUB( unsigned char **s, int n, int m )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewMatrixS( short **s, int n, int m )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewMatrixUS( unsigned short **s, int n, int m )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewMatrixI( int **s, int n, int m )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewMatrixUI( unsigned int **s, int n, int m )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewMatrixF( float **s, int n, int m )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewMatrixD( double **s, int n, int m )
{
	return NumArrayDisabled();
}
DaoValue* DaoValue_NewBuffer( void *s, int n )
{
	return NumArrayDisabled();
}
#endif
DaoValue* DaoValue_NewStream( FILE *f )
{
	DaoStream *self = DaoStream_New();
	DaoStream_SetFile( self, f );
	return (DaoValue*) self;
}
DaoValue* DaoValue_NewCData( DaoTypeBase *typer, void *data )
{
	return (DaoValue*) DaoCData_New( typer, data );
}
DaoValue* DaoValue_WrapCData( DaoTypeBase *typer, void *data )
{
	return (DaoValue*) DaoCData_Wrap( typer, data );
}

DaoInteger* DaoValue_CastInteger( DaoValue *self )
{
	if( self == NULL || self->type != DAO_INTEGER ) return NULL;
	return (DaoInteger*) self;
}
DaoFloat* DaoValue_CastFloat( DaoValue *self )
{
	if( self == NULL || self->type != DAO_FLOAT ) return NULL;
	return (DaoFloat*) self;
}
DaoDouble* DaoValue_CastDouble( DaoValue *self )
{
	if( self == NULL || self->type != DAO_DOUBLE ) return NULL;
	return (DaoDouble*) self;
}
DaoComplex* DaoValue_CastComplex( DaoValue *self )
{
	if( self == NULL || self->type != DAO_COMPLEX ) return NULL;
	return (DaoComplex*) self;
}
DaoLong* DaoValue_CastLong( DaoValue *self )
{
	if( self == NULL || self->type != DAO_LONG ) return NULL;
	return (DaoLong*) self;
}
DaoString* DaoValue_CastString( DaoValue *self )
{
	if( self == NULL || self->type != DAO_STRING ) return NULL;
	return (DaoString*) self;
}
DaoEnum* DaoValue_CastEnum( DaoValue *self )
{
	if( self == NULL || self->type != DAO_ENUM ) return NULL;
	return (DaoEnum*) self;
}
DaoArray* DaoValue_CastArray( DaoValue *self )
{
	if( self == NULL || self->type != DAO_ARRAY ) return NULL;
	return (DaoArray*) self;
}
DaoList* DaoValue_CastList( DaoValue *self )
{
	if( self == NULL || self->type != DAO_LIST ) return NULL;
	return (DaoList*) self;
}
DaoMap* DaoValue_CastMap( DaoValue *self )
{
	if( self == NULL || self->type != DAO_MAP ) return NULL;
	return (DaoMap*) self;
}
DaoTuple* DaoValue_CastTuple( DaoValue *self )
{
	if( self == NULL || self->type != DAO_TUPLE ) return NULL;
	return (DaoTuple*) self;
}
DaoStream* DaoValue_CastStream( DaoValue *self )
{
	if( self == NULL || self->type != DAO_STREAM ) return NULL;
	return (DaoStream*) self;
}
DaoObject* DaoValue_CastObject( DaoValue *self )
{
	if( self == NULL || self->type != DAO_OBJECT ) return NULL;
	return (DaoObject*) self;
}
DaoCData* DaoValue_CastCData( DaoValue *self )
{
	if( self == NULL || self->type != DAO_CDATA ) return NULL;
	return (DaoCData*) self;
}
DaoClass* DaoValue_CastClass( DaoValue *self )
{
	if( self == NULL || self->type != DAO_CLASS ) return NULL;
	return (DaoClass*) self;
}
DaoInterface* DaoValue_CastInterface( DaoValue *self )
{
	if( self == NULL || self->type != DAO_INTERFACE ) return NULL;
	return (DaoInterface*) self;
}
DaoFunctree* DaoValue_CastFunctree( DaoValue *self )
{
	if( self == NULL || self->type != DAO_FUNCTREE ) return NULL;
	return (DaoFunctree*) self;
}
DaoRoutine* DaoValue_CastRoutine( DaoValue *self )
{
	if( self == NULL || self->type != DAO_ROUTINE ) return NULL;
	return (DaoRoutine*) self;
}
DaoFunction* DaoValue_CastFunction( DaoValue *self )
{
	if( self == NULL || self->type != DAO_FUNCTION ) return NULL;
	return (DaoFunction*) self;
}
DaoContext* DaoValue_CastContext( DaoValue *self )
{
	if( self == NULL || self->type != DAO_CONTEXT ) return NULL;
	return (DaoContext*) self;
}
DaoVmProcess* DaoValue_CastVmProcess( DaoValue *self )
{
	if( self == NULL || self->type != DAO_VMPROCESS ) return NULL;
	return (DaoVmProcess*) self;
}
DaoNameSpace* DaoValue_CastNameSpace( DaoValue *self )
{
	if( self == NULL || self->type != DAO_NAMESPACE ) return NULL;
	return (DaoNameSpace*) self;
}
DaoType* DaoValue_CastType( DaoValue *self )
{
	if( self == NULL || self->type != DAO_TYPE ) return NULL;
	return (DaoType*) self;
}

dint DaoValue_TryGetInteger( DaoValue *self )
{
	if( self->type != DAO_INTEGER ) return 0;
	return self->xInteger.value;
}
float DaoValue_TryGetFloat( DaoValue *self )
{
	if( self->type != DAO_FLOAT ) return 0.0;
	return self->xFloat.value;
}
double DaoValue_TryGetDouble( DaoValue *self )
{
	if( self->type != DAO_DOUBLE ) return 0.0;
	return self->xDouble.value;
}
complex16 DaoValue_TryGetComplex( DaoValue *self )
{
	complex16 com = {0.0,0.0};
	if( self->type != DAO_COMPLEX ) return com;
	return self->xComplex.value;
}
char* DaoValue_TryGetMBString( DaoValue *self )
{
	if( self->type != DAO_STRING ) return NULL;
	return DString_GetMBS( self->xString.data );
}
wchar_t* DaoValue_TryGetWCString( DaoValue *self )
{
	if( self->type != DAO_STRING ) return NULL;
	return DString_GetWCS( self->xString.data );
}
void* DaoValue_TryCastCData( DaoValue *self, DaoTypeBase *typer )
{
	if( self->type != DAO_CDATA ) return NULL;
	return DaoCData_CastData( & self->xCdata, typer );
}
void* DaoValue_TryGetCData( DaoValue *self )
{
	if( self->type != DAO_CDATA ) return NULL;
	return self->xCdata.data;
}
void** DaoValue_TryGetCData2( DaoValue *self )
{
	if( self->type != DAO_CDATA ) return NULL;
	return & self->xCdata.data;
}
void DaoValue_ClearAll( DaoValue *v[], int n )
{
	int i;
	for(i=0; i<n; i++) DaoValue_Clear( v + i );
}


#define RADIX 32
static const char *hex_digits = "ABCDEFGHIJKLMNOP";
static const char *mydigits = "0123456789ABCDEFGHIJKLMNOPQRSTUVW";

void DaoEncodeInteger( char *p, dint value )
{
	int m;
	if( value < 0 ){
		*(p++) = '-';
		value = - value;
	}
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
dint DaoDecodeInteger( char *p )
{
	dint value = 0;
	dint power = 1;
	int sign = 1;
	if( *p == '-' ){
		sign = -1;
		p ++;
	}
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

static void DaoSerializeInteger( dint value, DString *serial )
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
static void DaoSerializeLong( DLong *value, DString *serial )
{
	int i;
	DaoSerializeInteger( value->base, serial );
	DString_AppendChar( serial, value->sign > 0 ? '+' : '-' );
	for(i=0; i<value->size; i++){
		if( i ) DString_AppendChar( serial, ',' );
		DaoSerializeInteger( value->data[i], serial );
	}
}

static int DaoValue_Serialize2( DaoValue*, DString*, DaoNameSpace*, DaoVmProcess*, DaoType*, DString* );

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
	DaoInteger intmp = {DAO_INTEGER,0,0,0,{0,0},0,0,0};
	DaoValue *value = (DaoValue*) & intmp;
	int i;
	DString_AppendChar( serial, '[' );
	for(i=0; i<self->dims->size; i++){
		value->xInteger.value = self->dims->items.pSize[i];
		if( i ) DString_AppendChar( serial, ',' );
		DaoValue_GetString( value, buf );
		DString_Append( serial, buf );
	}
	DString_AppendChar( serial, ']' );
	for(i=0; i<self->size; i++){
		if( i ) DString_AppendChar( serial, ',' );
		switch( self->numType ){
		case DAO_INTEGER : DaoSerializeInteger( self->data.i[i], serial ); break;
		case DAO_FLOAT : DaoSerializeDouble( self->data.f[i], serial ); break;
		case DAO_DOUBLE : DaoSerializeDouble( self->data.d[i], serial ); break;
		case DAO_COMPLEX : DaoSerializeComplex( self->data.c[i], serial ); break;
		}
	}
}
static int DaoList_Serialize( DaoList *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc, DString *buf )
{
	DaoType *type = self->unitype;
	int i, rc = 1;
	if( type->nested && type->nested->size ) type = type->nested->items.pType[0];
	if( type && (type->tid == 0 || type->tid >= DAO_ENUM)) type = NULL;
	for(i=0; i<self->items->size; i++){
		DaoType *it = NULL;
		if( type == NULL ) it = DaoNameSpace_GetType( ns, self->items->items.pValue[i] );
		if( i ) DString_AppendChar( serial, ',' );
		rc &= DaoValue_Serialize2( self->items->items.pValue[i], serial, ns, proc, it, buf );
	}
	return rc;
}
static int DaoMap_Serialize( DaoMap *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc, DString *buf )
{
	DaoType *type = self->unitype;
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
		if( keytype == NULL ) kt = DaoNameSpace_GetType( ns, node->key.pValue );
		if( valtype == NULL ) vt = DaoNameSpace_GetType( ns, node->value.pValue );
		if( (i++) ) DString_AppendChar( serial, ',' );
		rc &= DaoValue_Serialize2( node->key.pValue, serial, ns, proc, kt, buf );
		DString_AppendMBS( serial, sep );
		rc &= DaoValue_Serialize2( node->value.pValue, serial, ns, proc, vt, buf );
	}
	return rc;
}
static int DaoTuple_Serialize( DaoTuple *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc, DString *buf )
{
	DArray *nested = self->unitype ? self->unitype->nested : NULL;
	int i, rc = 1;
	for(i=0; i<self->items->size; i++){
		DaoType *type = NULL;
		DaoType *it = NULL;
		if( nested && nested->size > i ) type = nested->items.pType[i];
		if( type && type->tid == DAO_PAR_NAMED ) type = & type->aux->xType;
		if( type && (type->tid == 0 || type->tid >= DAO_ENUM)) type = NULL;
		if( type == NULL ) it = DaoNameSpace_GetType( ns, self->items->items.pValue[i] );
		if( i ) DString_AppendChar( serial, ',' );
		rc &= DaoValue_Serialize2( self->items->items.pValue[i], serial, ns, proc, it, buf );
	}
	return rc;
}
static int DaoObject_Serialize( DaoObject *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc, DString *buf )
{
	DRoutine *rt;
	DaoType *type;
	DString name = DString_WrapMBS( "serialize" );
	DaoValue *value = NULL;
	DaoValue *selfpar = (DaoValue*) self;
	int errcode = DaoObject_GetData( self, & name, & value, NULL );
	if( errcode || value == NULL || value->type < DAO_FUNCTREE || value->type > DAO_FUNCTION ) return 0;
	rt = DRoutine_Resolve( value, selfpar, NULL, 0, DVM_CALL );
	if( rt == NULL ) return 0;
	if( rt->type == DAO_ROUTINE ){
		DaoRoutine *rout = (DaoRoutine*) rt;
		DaoContext *ctx = DaoVmProcess_MakeContext( proc, rout );
		GC_ShiftRC( self, ctx->object );
		ctx->object = self;
		DaoContext_Init( ctx, rout );
		if( DRoutine_PassParams( rt, selfpar, ctx->regValues, NULL, 0, DVM_CALL ) ){
			DaoVmProcess_PushContext( proc, ctx );
			proc->topFrame->returning = -1;
			DaoVmProcess_Execute( proc );
		}
	}else if( rt->type == DAO_FUNCTION ){
		DaoFunction *func = (DaoFunction*) rt;
		DaoContext ctx = *proc->topFrame->context;
		DaoVmCode vmc = { 0, 0, 0, 0 };
		DaoType *types[] = { NULL, NULL, NULL };

		ctx.regValues = & proc->returned;
		ctx.regTypes = types;
		ctx.vmc = & vmc;
		ctx.codes = & vmc;

		DaoFunction_Call( func, & ctx, selfpar, NULL, 0 );
	}else{
		return 0;
	}
	type = DaoNameSpace_GetType( ns, proc->returned );
	DaoValue_Serialize2( proc->returned, serial, ns, proc, type, buf );
	return 1;
}
static int DaoCData_Serialize( DaoCData *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc, DString *buf )
{
	DaoType *type;
	DaoValue *selfpar = (DaoValue*) self;
	DaoValue *meth = DaoFindFunction2( self->typer, "serialize" );
	DaoFunction *func = (DaoFunction*) meth;
	DaoContext ctx = *proc->topFrame->context;
	DaoVmCode vmc = { 0, 0, 0, 0 };
	DaoType *types[] = { NULL, NULL, NULL };

	ctx.regValues = & proc->returned;
	ctx.regTypes = types;
	ctx.vmc = & vmc;
	ctx.codes = & vmc;

	if( meth == NULL ) return 0;
	func = (DaoFunction*) DRoutine_Resolve( meth, selfpar, NULL, 0, DVM_CALL );
	if( func == NULL || func->type != DAO_FUNCTION ) return 0;
	DaoFunction_Call( func, & ctx, selfpar, NULL, 0 );
	type = DaoNameSpace_GetType( ns, proc->returned );
	DaoValue_Serialize2( proc->returned, serial, ns, proc, type, buf );
	return 1;
}
int DaoValue_Serialize2( DaoValue *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc, DaoType *type, DString *buf )
{
	int rc = 1;
	if( type ){
		DString_Append( serial, type->name );
		DString_AppendChar( serial, '{' );
	}
	switch( self->type ){
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
	case DAO_LONG :
		DaoSerializeLong( self->xLong.value, serial );
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
		rc = DaoList_Serialize( & self->xList, serial, ns, proc, buf );
		break;
	case DAO_MAP :
		rc = DaoMap_Serialize( & self->xMap, serial, ns, proc, buf );
		break;
	case DAO_TUPLE :
		rc = DaoTuple_Serialize( & self->xTuple, serial, ns, proc, buf );
		break;
	case DAO_OBJECT :
		if( proc == NULL ) break;
		rc = DaoObject_Serialize( & self->xObject, serial, ns, proc, buf );
		break;
	case DAO_CDATA :
		if( proc == NULL ) break;
		rc = DaoCData_Serialize( & self->xCdata, serial, ns, proc, buf );
		break;
	default :
		DString_AppendChar( serial, '?' );
		rc = 0;
		break;
	}
	if( type ) DString_AppendChar( serial, '}' );
	return rc;
}
int DaoValue_Serialize( DaoValue *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc )
{
	DaoType *type = DaoNameSpace_GetType( ns, self );
	DString *buf = DString_New(1);
	int rc;
	DString_Clear( serial );
	DString_ToMBS( serial );
	rc = DaoValue_Serialize2( self, serial, ns, proc, type, buf );
	DString_Delete( buf );
	return rc;
}

int DaoParser_FindPairToken( DaoParser *self,  uchar_t lw, uchar_t rw, int start, int stop );
DaoType* DaoParser_ParseType( DaoParser *self, int start, int end, int *newpos, DArray *types );

static DaoObject* DaoClass_MakeObject( DaoClass *self, DaoValue *param, DaoVmProcess *proc )
{
	DaoContext *ctx;
	DRoutine *rt = DRoutine_Resolve( (DaoValue*)self->classRoutines, NULL, & param, 1, DVM_CALL );
	if( rt == NULL || rt->type != DAO_ROUTINE ) return NULL;
	ctx = DaoVmProcess_MakeContext( proc, (DaoRoutine*) rt );
	DaoContext_Init( ctx, ctx->routine );
	if( DRoutine_PassParams( rt, NULL, ctx->regValues, & param, 1, DVM_CALL ) ){
		DaoObject *object = DaoObject_New( self, NULL, 0 );
		GC_ShiftRC( object, ctx->object );
		ctx->object = object;
		ctx->ctxState = DVM_MAKE_OBJECT;
		DaoVmProcess_PushContext( proc, ctx );
		proc->topFrame->returning = -1;
		DaoVmProcess_Execute( proc );
		return object;
	}
	return NULL;
}
static DaoCData* DaoCData_MakeObject( DaoCData *self, DaoValue *param, DaoVmProcess *proc )
{
	DaoValue *meth = DaoFindFunction2( self->typer, self->typer->name );
	DaoFunction *func = (DaoFunction*) meth;
	DaoContext ctx = *proc->topFrame->context;
	DaoVmCode vmc = { 0, 0, 0, 0 };
	DaoType *types[] = { self->typer->priv->abtype, NULL, NULL };

	ctx.regValues = & proc->returned;
	ctx.regTypes = types;
	ctx.vmc = & vmc;
	ctx.codes = & vmc;
	if( meth == NULL ) return NULL;
	func = (DaoFunction*) DRoutine_Resolve( meth, NULL, & param, 1, DVM_CALL );
	if( func == NULL || func->type != DAO_FUNCTION ) return NULL;
	DaoFunction_Call( func, & ctx, NULL, & param, 1 );
	if( proc->returned && proc->returned->type == DAO_CDATA ) return & proc->returned->xCdata;
	return NULL;
}

int DaoParser_Deserialize( DaoParser *self, int start, int end, DaoValue **value2, DArray *types, DaoNameSpace *ns, DaoVmProcess *proc )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoType *it1 = NULL, *it2 = NULL, *type = NULL;
	DaoValue *value = *value2;
	DaoValue *tmp = NULL;
	DaoValue *tmp2 = NULL;
	DaoObject *object;
	DaoCData *cdata;
	DaoArray *array;
	DaoTuple *tuple;
	DaoList *list;
	DaoMap *map;
	DNode *node;
	char *str;
	int i, j, k, n;
	int minus = 0;
	int next = start + 1;
	int tok2 = start < end ? tokens[start+1]->type : 0;
	int maybetype = tok2 == DTOK_COLON2 || tok2 == DTOK_LT || tok2 == DTOK_LCB;

	if( tokens[start]->name == DTOK_ID_SYMBOL ){
		DString *mbs = DString_New(1);
		while( tokens[start]->name == DTOK_ID_SYMBOL ){
			DString_Append( mbs, tokens[start]->string );
			start += 1;
		}
		type = DaoNameSpace_MakeType( ns, mbs->mbs, DAO_ENUM, NULL, NULL, 0 );
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
	if( type == NULL ) type = types->items.pType[0];
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
	str = tokens[start]->string->mbs;
#if 0
	printf( "type: %s %s\n", type->name->mbs, str );
	for(i=start; i<=end; i++) printf( "%s ", tokens[i]->string->mbs ); printf( "\n" );
#endif
	value = *value2;
	switch( type->tid ){
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
		value->xComplex.value.imag = DaoDecodeDouble( tokens[start+1]->string->mbs );
		if( minus ) value->xComplex.value.imag = - value->xComplex.value.imag;
		next = start + 2;
		break;
	case DAO_LONG :
		value->xLong.value->base = DaoDecodeInteger( str );
		start += 1;
		if( tokens[start]->name == DTOK_ADD ){
			value->xLong.value->sign = 1;
			start += 1;
		}else if( tokens[start]->name == DTOK_SUB ){
			value->xLong.value->sign = -1;
			start += 1;
		}
		for(i=start; i<=end; i++){
			if( tokens[i]->name == DTOK_COMMA ) continue;
			DLong_PushBack( value->xLong.value, DaoDecodeInteger( tokens[i]->string->mbs ) );
		}
		break;
	case DAO_STRING :
		n = tokens[start]->string->size - 1;
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
			n *= strtol( tokens[i]->string->mbs, 0, 0 );
		}
		if( n < 0 ) return next;
		if( it1 == NULL || it1->tid == 0 || it1->tid > DAO_COMPLEX ) return next;
		array = & value->xArray;
		DArray_Clear( array->dimAccum );
		for(i=start+1; i<k; i++){
			if( tokens[i]->name == DTOK_COMMA ) continue;
			j = strtol( tokens[i]->string->mbs, 0, 0 );
			DArray_Append( array->dimAccum, (size_t) j );
		}
		n = array->dimAccum->size;
		DaoArray_ResizeArray( array, array->dimAccum->items.pSize, n );
		DArray_PushFront( types, it1 );
		n = 0;
		for(i=k+1; i<=end; i++){
			j = i + 1;
			while( j <= end && tokens[j]->name != DTOK_COMMA ) j += 1;
			DaoParser_Deserialize( self, i, j-1, & tmp, types, ns, proc );
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
			DArray_Append( list->items, NULL );
			k = DaoParser_Deserialize( self, i, end, list->items->items.pValue + n, types, ns, proc );
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
			i = DaoParser_Deserialize( self, i, end, &tmp, types, ns, proc );
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
			i = DaoParser_Deserialize( self, i, end, &tmp2, types, ns, proc );
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
			i = DaoParser_Deserialize( self, i, end, tuple->items->items.pValue + n, types, ns, proc );
			DArray_PopFront( types );
			i -= 1;
			n += 1;
		}
		break;
	case DAO_OBJECT :
		DArray_PushFront( types, NULL );
		DaoParser_Deserialize( self, start, end, & tmp, types, ns, proc );
		DArray_PopFront( types );
		if( tmp == NULL ) break;
		object = DaoClass_MakeObject( & type->aux->xClass, tmp, proc );
		if( object == NULL ) break;
		value = (DaoValue*) object;
		DaoValue_Copy( value, value2 );
		if( *value2 != value ) GC_IncRC( value ), GC_DecRC( value );
		break;
	case DAO_CDATA :
		DArray_PushFront( types, NULL );
		DaoParser_Deserialize( self, start, end, & tmp, types, ns, proc );
		DArray_PopFront( types );
		if( tmp == NULL ) break;
		cdata = DaoCData_MakeObject( & type->aux->xCdata, tmp, proc );
		if( cdata == NULL ) break;
		value = (DaoValue*) cdata;
		DaoValue_Copy( value, value2 );
		if( *value2 != value ) GC_IncRC( value ), GC_DecRC( value );
		break;
	}
	DaoValue_Clear( & tmp );
	DaoValue_Clear( & tmp2 );
	return next;
}
int DaoValue_Deserialize( DaoValue **self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc )
{
	DaoParser *parser = DaoParser_New();
	DArray *types = DArray_New(0);
	int rc;

	DaoValue_Clear( self );
	parser->nameSpace = ns;
	parser->vmSpace = ns->vmSpace;
	DaoParser_LexCode( parser, DString_GetMBS( serial ), 0 );
	if( parser->tokens->size == 0 ) goto Failed;

	DArray_PushFront( types, NULL );
	rc = DaoParser_Deserialize( parser, 0, parser->tokens->size-1, self, types, ns, proc );

	DaoParser_Delete( parser );
	DArray_Delete( types );
	return rc;
Failed:
	DaoParser_Delete( parser );
	DArray_Delete( types );
	return 0;
}
