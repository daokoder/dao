/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2010, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

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

#include"daoGC.h"
#include"daoType.h"
#include"daoStream.h"
#include"daoRoutine.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoNumtype.h"

#if 1
const DValue daoNullValue = { 0, 0, 0, 0, {0}};
const DValue daoZeroInt = { DAO_INTEGER, 0, 0, 0, {0}};
const DValue daoZeroFloat = { DAO_FLOAT, 0, 0, 0, {0}};
const DValue daoZeroDouble = { DAO_DOUBLE, 0, 0, 0, {0}};
const DValue daoNullComplex = { DAO_COMPLEX, 0, 0, 0, {0}};
const DValue daoNullString = { DAO_STRING, 0, 0, 0, {0}};
const DValue daoNullEnum = { DAO_ENUM, 0, 0, 0, {0}};
const DValue daoNullArray = { DAO_ARRAY, 0, 0, 0, {0}};
const DValue daoNullList = { DAO_LIST, 0, 0, 0, {0}};
const DValue daoNullMap = { DAO_MAP, 0, 0, 0, {0}};
const DValue daoNullPair = { DAO_PAIR, 0, 0, 0, {0}};
const DValue daoNullTuple = { DAO_TUPLE, 0, 0, 0, {0}};
const DValue daoNullClass = { DAO_CLASS, 0, 0, 0, {0}};
const DValue daoNullObject = { DAO_OBJECT, 0, 0, 0, {0}};
const DValue daoNullRoutine = { DAO_ROUTINE, 0, 0, 0, {0}};
const DValue daoNullFunction = { DAO_FUNCTION, 0, 0, 0, {0}};
const DValue daoNullCData = { DAO_CDATA, 0, 0, 0, {0}};
const DValue daoNullStream = { DAO_STREAM, 0, 0, 0, {0}};
const DValue daoNullType = { DAO_TYPE, 0, 0, 0, {0}};
#endif

int DaoArray_Compare( DaoArray *x, DaoArray *y );

int DValue_Compare( DValue left, DValue right )
{
	int res = 0;
	if( left.t >= DAO_INTEGER && left.t <= DAO_DOUBLE
			&& right.t >= DAO_INTEGER && right.t <= DAO_DOUBLE ){
		double L = DValue_GetDouble( left );
		double R = DValue_GetDouble( right );
		return L==R ? 0 : ( L<R ? -1 : 1 );
	}else if( left.t == DAO_ENUM && right.t == DAO_ENUM ){
		DEnum E;
		DNode *N = NULL;
		DEnum *L = left.v.e;
		DEnum *R = right.v.e;
		DMap *ML = L->type->mapNames;
		DMap *MR = R->type->mapNames;
		uchar_t FL = L->type->flagtype;
		uchar_t FR = R->type->flagtype;
		char SL = L->type->name->mbs[0];
		char SR = R->type->name->mbs[0];
		if( L->type == R->type ){
			return L->value == R->value ? 0 : ( L->value < R->value ? -1 : 1 );
		}else if( SL == '$' && SR == '$' && FL == 0 && FR == 0 ){
			return DString_Compare( L->type->name, R->type->name );
		}else if( SL == '$' && SR == '$' ){
			if( L->type->mapNames->size != R->type->mapNames->size ){
				return (int)L->type->mapNames->size - (int)R->type->mapNames->size;
			}else{
				for(N=DMap_First(ML);N;N=DMap_Next(ML,N)){
					if( DMap_Find( MR, N->key.pVoid ) ==0 ) return -1;
				}
				return 0;
			}
		}else if( SL == '$' ){
			E.type = R->type;
			E.value = R->value;
			DEnum_SetValue( & E, L, NULL );
			return E.value == R->value ? 0 : ( E.value < R->value ? -1 : 1 );
		}else if( SR == '$' ){
			E.type = L->type;
			E.value = L->value;
			DEnum_SetValue( & E, R, NULL );
			return L->value == E.value ? 0 : ( L->value < E.value ? -1 : 1 );
		}else{
			return L->value == R->value ? 0 : ( L->value < R->value ? -1 : 1 );
		}
	}else if( left.t == DAO_STRING && right.t == DAO_STRING ){
		return DString_Compare( left.v.s, right.v.s );
	}else if( left.t == DAO_LONG && right.t == DAO_LONG ){
		return DLong_Compare( left.v.l, right.v.l );
	}else if( left.t == DAO_PAIR && right.t == DAO_PAIR ){
		res = DValue_Compare( left.v.pair->first, right.v.pair->first );
		if( res == 0 ) res = DValue_Compare( left.v.pair->second, right.v.pair->second );
		return res;
	}else if( right.t == DAO_PAIR ){
		res = DValue_Compare( left, right.v.pair->first );
		if( res <= 0 ) return res;
		res = DValue_Compare( left, right.v.pair->second );
		if( res >= 0 ) return res;
	}else if( left.t == DAO_TUPLE && right.t == DAO_TUPLE ){
		DaoTuple *lt = left.v.tuple;
		DaoTuple *rt = right.v.tuple;
		if( lt->items->size == rt->items->size ){
			DValue lv, rv;
			int i, lb, rb;
			for(i=0; i<lt->items->size; i++){
				lv = lt->items->data[i];
				rv = rt->items->data[i];
				lb = lv.t;
				rb = rv.t;
				if( lb == rb && lb == DAO_TUPLE ){
					res = DValue_Compare( lv, rv );
					if( res != 0 ) return res;
				}else if( lb != rb || lb ==0 || lb >= DAO_ARRAY || lb == DAO_COMPLEX ){
					if( lv.v.p < rv.v.p ){
						return -1;
					}else if( lv.v.p > rv.v.p ){
						return 1;
					}
				}else{
					res = DValue_Compare( lv, rv );
					if( res != 0 ) return res;
				}
			}
			return 0;
		}else if( lt->items->size < rt->items->size ){
			return -1;
		}else if( lt->items->size > rt->items->size ){
			return 1;
		}
	}else if( left.t == DAO_LIST && right.t == DAO_LIST ){
		DaoList *list1 = left.v.list;
		DaoList *list2 = right.v.list;
		DValue *d1 = list1->items->data;
		DValue *d2 = list2->items->data;
		int size1 = list1->items->size;
		int size2 = list2->items->size;
		int min = size1 < size2 ? size1 : size2;
		int i = 0, cmp = 0;
		/* find the first unequal items */
		while( i < min && (cmp = DValue_Compare(*d1, *d2)) ==0 ) i++, d1++, d2++;
		if( i < min ) return cmp;
		if( size1 == size2  ) return 0;
		return size1 > size2 ? 1 : -1;
	}else if( left.t == DAO_ARRAY && right.t == DAO_ARRAY ){
		return DaoArray_Compare( left.v.array, right.v.array );
	}else if( left.t == DAO_CDATA && right.t == DAO_CDATA ){
		return (int)( (size_t)right.v.cdata->data - (size_t)left.v.cdata->data );
	}else if( left.t != right.t ){
		return right.t - left.t;
	}else if( right.v.p < left.v.p ){
		return -1;
	}else if( right.v.p > left.v.p ){
		return 1;
	}else{
		/* return (int)( (size_t)right.v.p - (size_t)left.v.p); */
	}
	return 0;
}
llong_t DValue_GetLongLong( DValue self )
{
	return (llong_t) DValue_GetDouble( self );
}
llong_t DValue_GetInteger( DValue self )
{
	DString *str;
	switch( self.t ){
	case DAO_INTEGER :
		return self.v.i;
	case DAO_FLOAT   :
		return self.v.f;
	case DAO_DOUBLE  :
		return self.v.d;
	case DAO_COMPLEX :
		return self.v.c->real;
	case DAO_LONG :
		return DLong_ToInteger( self.v.l );
	case DAO_ENUM  :
		return self.v.e->value;
	case DAO_STRING  :
		str = self.v.s;
		if( DString_IsMBS( str ) )
			return strtoll( str->mbs, NULL, 0 );
		else
			return wcstoll( str->wcs, NULL, 0 );
	default : break;
	}
	return 0;
}
float DValue_GetFloat( DValue self )
{
	return (float) DValue_GetDouble( self );
}
double DValue_GetDouble( DValue self )
{
	DString *str;
	switch( self.t ){
	case DAO_INTEGER :
		return self.v.i;
	case DAO_FLOAT   :
		return self.v.f;
	case DAO_DOUBLE  :
		return self.v.d;
	case DAO_COMPLEX :
		return self.v.c->real;
	case DAO_LONG :
		return DLong_ToDouble( self.v.l );
	case DAO_ENUM  :
		return self.v.e->value;
	case DAO_STRING  :
		str = self.v.s;
		if( DString_IsMBS( str ) )
			return strtod( str->mbs, 0 );
		else
			return wcstod( str->wcs, 0 );
	default : break;
	}
	return 0.0;
}
int DValue_IsNumber( DValue self )
{
	if( self.t >= DAO_INTEGER && self.t <= DAO_DOUBLE ) return 1;
	return 0;
}
void DValue_Print( DValue self, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
	DString *name;
	DaoTypeBase *typer;
	switch( self.t ){
	case DAO_INTEGER :
		DaoStream_WriteInt( stream, self.v.i ); break;
	case DAO_FLOAT   :
		DaoStream_WriteFloat( stream, self.v.f ); break;
	case DAO_DOUBLE  :
		DaoStream_WriteFloat( stream, self.v.d ); break;
	case DAO_COMPLEX :
		DaoStream_WriteFloat( stream, self.v.c->real );
		if( self.v.c->imag >= -0.0 ) DaoStream_WriteMBS( stream, "+" );
		DaoStream_WriteFloat( stream, self.v.c->imag );
		DaoStream_WriteMBS( stream, "$" );
		break;
	case DAO_LONG :
		DLong_Print( self.v.l, stream->streamString );
		DaoStream_WriteString( stream, stream->streamString );
		break;
	case DAO_ENUM  :
		name = DString_New(1);
		DEnum_MakeName( self.v.e, name );
		DaoStream_WriteMBS( stream, name->mbs );
		DaoStream_WriteMBS( stream, "=" );
		DaoStream_WriteInt( stream, self.v.e->value );
		DString_Delete( name );
		break;
	case DAO_STRING  :
		DaoStream_WriteString( stream, self.v.s ); break;
	default :
		typer = DaoVmSpace_GetTyper( self.t );
		typer->priv->Print( & self, ctx, stream, cycData );
		break;
	}
}
complex16 DValue_GetComplex( DValue self )
{
	complex16 com = { 0.0, 0.0 };
	switch( self.t ){
	case DAO_INTEGER : com.real = self.v.i; break;
	case DAO_FLOAT   : com.real = self.v.f; break;
	case DAO_DOUBLE  : com.real = self.v.d; break;
	case DAO_COMPLEX : com = *self.v.c; break;
	default : break;
	}
	return com;
}
DLong* DValue_GetLong( DValue val, DLong *lng )
{
	switch( val.t ){
	case DAO_INTEGER :
		DLong_FromInteger( lng, DValue_GetInteger( val ) );
		break;
	case DAO_FLOAT : case DAO_DOUBLE :
		DLong_FromDouble( lng, DValue_GetDouble( val ) );
		break;
	case DAO_COMPLEX :
		DLong_FromInteger( lng, val.v.c->real );
		break;
	case DAO_LONG :
		DLong_Move( lng, val.v.l );
		break;
	case DAO_STRING :
		DLong_FromString( lng, val.v.s );
		break;
	default : break; /* TODO list array? */
	}
	return lng;
}
DString* DValue_GetString( DValue val, DString *str )
{
	char chs[100] = {0};
	DString_Clear( str );
	switch( val.t ){
	case DAO_INTEGER : sprintf( chs, ( sizeof(dint) == 4 )? "%li" : "%lli", val.v.i ); break;
	case DAO_FLOAT   : sprintf( chs, "%g", val.v.f ); break;
	case DAO_DOUBLE  : sprintf( chs, "%g", val.v.d ); break;
	case DAO_COMPLEX : sprintf( chs, ( val.v.c->imag < 0) ? "%g%g$" : "%g+%g$", val.v.c->real, val.v.c->imag ); break;
	case DAO_LONG  : DLong_Print( val.v.l, str ); break;
	case DAO_ENUM : DEnum_MakeName( val.v.e, str ); break;
	case DAO_STRING : DString_Assign( str, val.v.s ); break;
	default : break;
	}
	if( val.t <= DAO_COMPLEX ) DString_SetMBS( str, chs );
	return str;
}
void DValue_MarkConst( DValue *self )
{
	DaoObject *obj;
	DValue oval = daoNullObject;
	DMap *map;
	DNode *it;
	int i, n;
	/* to allow class statics and namespace globals to be mutable:  */
	if( self->t != DAO_CLASS && self->t != DAO_NAMESPACE ) self->cst = 1;
	if( self->t >= DAO_ARRAY ) self->v.p->trait |= DAO_DATA_CONST;
	switch( self->t ){
	case DAO_LIST :
		for(i=0; i<self->v.list->items->size; i++)
			DValue_MarkConst( self->v.list->items->data + i );
		break;
	case DAO_TUPLE :
		for(i=0; i<self->v.tuple->items->size; i++)
			DValue_MarkConst( self->v.tuple->items->data + i );
		break;
	case DAO_MAP :
		map = self->v.map->items;
		for(it=DMap_First( map ); it != NULL; it = DMap_Next(map, it) ){
			DValue_MarkConst( it->key.pValue );
			DValue_MarkConst( it->value.pValue );
		}
		break;
	case DAO_PAIR :
		DValue_MarkConst( & self->v.pair->first );
		DValue_MarkConst( & self->v.pair->second );
		break;
	case DAO_OBJECT :
		obj = self->v.object;
		n = obj->myClass->objDataDefault->size;
		for(i=1; i<n; i++) DValue_MarkConst( obj->objValues + i );
		if( obj->superObject == NULL ) break;
		for(i=0; i<obj->superObject->size; i++){
			oval.v.object = obj->superObject->items.pObject[i];
			if( oval.v.object->type != DAO_OBJECT ) continue;
			DValue_MarkConst( & oval );
		}
		break;
	default : break;
	}
}
void DValue_Clear( DValue *self )
{
	if( self->t >= DAO_COMPLEX ){
		switch( self->t ){
		case DAO_COMPLEX : dao_free( self->v.c ); break;
		case DAO_LONG : DLong_Delete( self->v.l ); break;
		case DAO_ENUM : DEnum_Delete( self->v.e ); break;
		case DAO_STRING : DString_Delete( self->v.s ); break;
		default : GC_DecRC( self->v.p ); break;
		}
	}
	self->t = self->sub = self->cst = self->ndef = 0;
	self->v.d = 0.0;
}
void DValue_IncRCs( DValue *v, int n )
{
	int i;
	for(i=0; i<n; i++) if( v[i].t >= DAO_ARRAY ) GC_IncRC( v[i].v.p );
}

void DValue_CopyExt( DValue *self, DValue from, int copy )
{
	if( from.t >= DAO_ARRAY && from.t == self->t && from.v.p == self->v.p ) return;
	switch( self->t ){
	case DAO_COMPLEX : if( from.t != DAO_COMPLEX ) dao_free( self->v.c ); break;
	case DAO_LONG  : if( from.t != DAO_LONG ) DLong_Delete( self->v.l ); break;
	case DAO_ENUM  : if( from.t != DAO_ENUM ) DEnum_Delete( self->v.e ); break;
	case DAO_STRING  : if( from.t != DAO_STRING ) DString_Delete( self->v.s ); break;
	default : if( self->t >= DAO_ARRAY ) GC_DecRC( self->v.p ); break;
	}
	switch( from.t ){
	case DAO_NIL :
	case DAO_INTEGER :
	case DAO_FLOAT   :
	case DAO_DOUBLE  :
		*self = from;
		break;
	case DAO_COMPLEX :
		if( self->t != DAO_COMPLEX ) self->v.c = dao_malloc( sizeof(complex16) );
		self->v.c[0] = from.v.c[0];
		break;
	case DAO_LONG  :
		if( self->t != DAO_LONG ) self->v.l = DLong_New();
		self->t = DAO_LONG;
		DLong_Move( self->v.l, from.v.l );
		break;
	case DAO_ENUM :
		if( self->t != DAO_ENUM ){
			self->v.e = DEnum_Copy( from.v.e );
		}else{
			DEnum_SetType( self->v.e, from.v.e->type );
			DEnum_SetValue( self->v.e, from.v.e, NULL );
		}
		break;
	case DAO_STRING  :
		if( self->t == DAO_STRING ){
			DString_Assign( self->v.s, from.v.s );
		}else{
			self->v.s = DString_Copy( from.v.s );
		}
		break;
	default :
		self->v.p = NULL;
		if( from.v.p ){
			if( copy ) from.v.p = DaoBase_Duplicate( from.v.p, NULL );
			self->v.p = from.v.p;
			GC_IncRC( self->v.p );
		}
		break;
	}
	self->t = from.t;
	self->sub = from.sub;
	self->ndef = from.ndef;
	self->cst = 0;
}
void DValue_Copy( DValue *self, DValue from )
{
	DValue_CopyExt( self, from, 1 );
}
void DValue_SetType( DValue *to, DaoType *tp )
{
	DaoType *tp2;
	DNode *it;
	if( to->t != tp->tid && tp->tid != DAO_ANY ) return;
	/* XXX compatible types? list<int> list<float> */
	switch( to->t ){
	case DAO_LIST :
		/* v : any = {}, v->unitype should be list<any> */
		if( tp->tid == DAO_ANY ) tp = dao_list_any;
		tp2 = to->v.list->unitype;
		if( tp2 && !(tp2->attrib & DAO_TYPE_EMPTY) ) break;
		GC_ShiftRC( tp, to->v.list->unitype );
		to->v.list->unitype = tp;
		break;
	case DAO_MAP :
		if( tp->tid == DAO_ANY ) tp = dao_map_any;
		tp2 = to->v.map->unitype;
		if( tp2 && !(tp2->attrib & DAO_TYPE_EMPTY) ) break;
		GC_ShiftRC( tp, to->v.map->unitype );
		to->v.map->unitype = tp;
		break;
	case DAO_TUPLE :
		tp2 = to->v.tuple->unitype;
		if( tp->nested->size ==0 ) break; /* not to the generic tuple type */
		if( tp2 == NULL || tp2->mapNames == NULL || tp2->mapNames->size ==0 ){
			GC_ShiftRC( tp, to->v.tuple->unitype );
			to->v.tuple->unitype = tp;
			break;
		}
		if( tp->mapNames == NULL || tp->mapNames->size ) break;
		for(it=DMap_First(tp2->mapNames); it!=NULL; it=DMap_Next(tp2->mapNames, it)){
			if( DMap_Find( tp->mapNames, it->key.pVoid ) == NULL ) break;
		}
		if( it ) break;
		GC_ShiftRC( tp, to->v.tuple->unitype );
		to->v.tuple->unitype = tp;
		break;
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		/*XXX array<int> array<float>*/
		if( tp->tid == DAO_ANY ) tp = dao_array_any;
		tp2 = to->v.array->unitype;
		if( tp2 && !(tp2->attrib & DAO_TYPE_EMPTY) ) break;
		GC_ShiftRC( tp, to->v.array->unitype );
		to->v.array->unitype = tp;
		break;
#endif
	default : break;
	}
}
void DValue_SimpleMove( DValue from, DValue *to )
{
	DValue_CopyExt( to, from, 0 );
}
#if 0
int DValue_Move2( DValue from, DValue *to, DaoType *tp );
int DValue_Move( DValue from, DValue *to, DaoType *tp )
{
	//DaoBase *dA = NULL;
	//int i = 1;
	if( tp == NULL ){
		DValue_Copy( to, from );
		return 1;
	}
	/* binary or is OK here: */
	if( (from.t < DAO_ARRAY) & (to->t < DAO_ARRAY) & (tp->tid < DAO_ARRAY) ){
		to->sub = from.sub;
		to->cst = to->ndef = 0;
		if( from.t != to->t ){
			switch( to->t ){
			case DAO_COMPLEX : dao_free( to->v.c ); break;
			case DAO_STRING : DString_Delete( to->v.s ); break;
			case DAO_LONG : DLong_Delete( to->v.l ); break;
			default : break;
			}
		}
		switch( (from.t<<8) | tp->tid ){
		case (DAO_COMPLEX<<8)|DAO_COMPLEX :
			if( to->t != DAO_COMPLEX ) to->v.c = dao_malloc( sizeof(complex16) );
			to->v.c[0] = from.v.c[0];
			break;
		case (DAO_STRING<<8)|DAO_STRING  :
			if( to->t == DAO_STRING ){
				DString_Assign( to->v.s, from.v.s );
			}else{
				to->v.s = DString_Copy( from.v.s );
			}
			break;
		case (DAO_LONG)|DAO_LONG  :
			if( to->t != DAO_LONG ) to->v.l = DLong_New();
			DLong_Move( to->v.l, from.v.l );
			break;
		case (DAO_INTEGER<<8)|DAO_INTEGER : to->v.i = from.v.i; break;
		case (DAO_INTEGER<<8)|DAO_FLOAT   : to->v.f = from.v.i; break;
		case (DAO_INTEGER<<8)|DAO_DOUBLE  : to->v.d = from.v.i; break;
		case (DAO_FLOAT  <<8)|DAO_INTEGER : to->v.i = from.v.f; break;
		case (DAO_FLOAT  <<8)|DAO_FLOAT   : to->v.f = from.v.f; break;
		case (DAO_FLOAT  <<8)|DAO_DOUBLE  : to->v.d = from.v.f; break;
		case (DAO_DOUBLE <<8)|DAO_INTEGER : to->v.i = from.v.d; break;
		case (DAO_DOUBLE <<8)|DAO_FLOAT   : to->v.f = from.v.d; break;
		case (DAO_DOUBLE <<8)|DAO_DOUBLE  : to->v.d = from.v.d; break;
		case 0: to->t = 0; return 1;
		default : return 0;
		}
		to->t = from.t;
		return 1;
	}
	/* binary or is OK here: */
	if( (tp->tid ==DAO_NIL) | (tp->tid ==DAO_INITYPE) ){
		DValue_Copy( to, from );
		return 1;
	}else if( tp->tid == DAO_ANY ){
		DValue_Copy( to, from );
		DValue_SetType( to, tp );
		return 1;
	}
	return DValue_Move2( from, to, tp );
}
#endif
int DValue_Move( DValue from, DValue *to, DaoType *tp )
{
	DaoBase *dA = NULL;
	DNode *node = NULL;
	int i = 1;
	if( tp ==0 || tp->tid ==DAO_NIL || tp->tid ==DAO_INITYPE ){
		DValue_Copy( to, from );
		return 1;
	}else if( tp->tid == DAO_ANY ){
		DValue_Copy( to, from );
		DValue_SetType( to, tp );
		return 1;
	}
	to->sub = from.sub;
	to->cst = to->ndef = 0;
	if( from.t >= DAO_ARRAY && from.t == to->t && from.v.p == to->v.p ) return 1;
	if( from.t >= DAO_INTEGER && from.t <= DAO_DOUBLE ){
		if( tp->tid < DAO_INTEGER || tp->tid > DAO_DOUBLE ) goto MoveFailed;;
	}else if( from.t >= DAO_COMPLEX && from.t <= DAO_ENUM ){
		if( tp->tid != from.t ) goto MoveFailed;;
	}else if( from.v.p ==NULL && tp->tid == DAO_OBJECT ){
		from.t = 0;
	}else if( from.t ==0 ){
		goto MoveFailed;;
	}else{
		dA = from.v.p;
		if( tp->tid == DAO_ROUTINE && ( dA->type ==DAO_ROUTINE || dA->type ==DAO_FUNCTION ) ){
			/* XXX pair<objetp,routine<...>> */
			dA = (DaoBase*) DRoutine_GetOverLoadByType( (DRoutine*)dA, tp );
			if( dA == NULL ) goto MoveFailed;;
			/* printf( "dA = %p,  %i  %s  %s\n", dA, i, tp->name->mbs, from.v.routine->routType->name->mbs ); */
		}else if( (tp->tid == DAO_OBJECT || tp->tid == DAO_CDATA) && dA->type == DAO_OBJECT){
			if( ((DaoObject*)dA)->myClass != tp->X.klass ){
				dA = DaoObject_MapThisObject( (DaoObject*)dA, tp );
				i = (dA != NULL);
			}
		}else if( from.t == DAO_CLASS && tp->tid == DAO_CLASS && from.v.klass->typeHolders ){
			if( DMap_Find( from.v.klass->instanceClasses, tp->X.klass->className ) ){
				from.v.klass = tp->X.klass;
				dA = (DaoBase*) from.v.klass;
				i = DAO_MT_SUB;
			}
		}else{
			i = DaoType_MatchValue( tp, from, NULL );
		}
		/*
		   if( i ==0 ){
		   printf( "tp = %p; dA = %p, type = %i\n", tp, dA, from.t );
		   printf( "tp: %s %i\n", tp->name->mbs, tp->tid );
		   if( from.t == DAO_TUPLE ) printf( "%p\n", from.v.tuple->unitype );
		   }
		   printf( "dA->type = %p\n", dA );
		 */
		if( i==0 ) goto MoveFailed;;
		/* composite known types must match exactly. example,
		 * where it will not work if composite types are allowed to match loosely.
		 * d : list<list<int>> = {};
		 * e : list<float> = { 1.0 };
		 * d.append( e );
		 * 
		 * but if d is of type list<list<any>>, 
		 * the matching do not necessary to be exact.
		 */
	}

	if( to->t >= DAO_ARRAY ){
		GC_DecRC( to->v.p );
	}else if( to->t == DAO_COMPLEX ){
		if( from.t != DAO_COMPLEX ) dao_free( to->v.c );
	}else if( to->t == DAO_LONG ){
		if( from.t != DAO_LONG ) DLong_Delete( to->v.l );
	}else if( to->t == DAO_ENUM ){
		if( from.t != DAO_ENUM ) DEnum_Delete( to->v.e );
	}else if( to->t == DAO_STRING ){
		if( from.t != DAO_STRING ) DString_Delete( to->v.s );
	}
	if( from.t >= DAO_INTEGER && from.t < DAO_ARRAY ){
		switch( from.t ){
		case DAO_INTEGER :
			to->t = tp->tid;
			switch( tp->tid ){
			case DAO_INTEGER : to->v.i = from.v.i; break;
			case DAO_FLOAT   : to->v.f = from.v.i; break;
			case DAO_DOUBLE  : to->v.d = from.v.i; break;
			default : break;
			}
			break;
		case DAO_FLOAT   :
			to->t = tp->tid;
			switch( tp->tid ){
			case DAO_INTEGER : to->v.i = from.v.f; break;
			case DAO_FLOAT   : to->v.f = from.v.f; break;
			case DAO_DOUBLE  : to->v.d = from.v.f; break;
			default : break;
			}
			break;
		case DAO_DOUBLE  :
			to->t = tp->tid;
			switch( tp->tid ){
			case DAO_INTEGER : to->v.i = from.v.d; break;
			case DAO_FLOAT   : to->v.f = from.v.d; break;
			case DAO_DOUBLE  : to->v.d = from.v.d; break;
			default : break;
			}
			break;
		case DAO_COMPLEX :
			if( to->t != DAO_COMPLEX ) to->v.c = dao_malloc( sizeof(complex16) );
			to->t = from.t;
			to->v.c[0] = DValue_GetComplex( from );
			break;
		case DAO_LONG  :
			if( to->t != DAO_LONG ) to->v.l = DLong_New();
			to->t = DAO_LONG;
			DLong_Move( to->v.l, from.v.l );
			break;
		case DAO_ENUM :
			if( to->t != DAO_ENUM ){
				to->v.e = DEnum_New( NULL, 0 );
				to->t = from.t;
			}
#if 0
			printf( "%s %s %i %i\n", tp->name->mbs, from.v.e->type->name->mbs, to->v.e->value, from.v.e->value );
			printf( "%i %i\n", tp->flagtype, from.v.e->type->flagtype );
#endif
			DEnum_SetType( to->v.e, tp );
			DEnum_SetValue( to->v.e, from.v.e, NULL );
			break;
		case DAO_STRING  :
			if( to->t == DAO_STRING ){
				DString_Assign( to->v.s, from.v.s );
			}else{
				to->v.s = DString_Copy( from.v.s );
			}
			to->t = from.t;
			break;
		default : break;
		}
		return 1;
	}

	dA = DaoBase_Duplicate( dA, tp );
	GC_IncRC( dA );
	to->t = dA->type;
	to->v.p = dA;
	if( from.t == DAO_TUPLE && from.v.p != dA && to->v.tuple->unitype != tp ){
		GC_ShiftRC( tp, to->v.tuple->unitype );
		to->v.tuple->unitype = tp;
	}else if( tp && ! ( tp->attrib & DAO_TYPE_EMPTY ) && tp->tid == dA->type ){
#if 0
		//int defed = DString_FindChar( tp->name, '@', 0 ) == MAXSIZE;
		//defed &= DString_FindChar( tp->name, '?', 0 ) == MAXSIZE;
#endif
		DValue_SetType( to, tp );
	}
	return 1;
MoveFailed:
	DValue_Clear( to );
	return 0;
}

DValue DValue_NewInteger( dint v )
{
	DValue res = daoZeroInt;
	res.v.i = v;
	return res;
}
DValue DValue_NewFloat( float v )
{
	DValue res = daoZeroFloat;
	res.v.f = v;
	return res;
}
DValue DValue_NewDouble( double v )
{
	DValue res = daoZeroDouble;
	res.v.d = v;
	return res;
}
DValue DValue_NewMBString( const char *s, int n )
{
	DValue res = daoNullString;
	res.v.s = DString_New(1);
	if( n )
		DString_SetDataMBS( res.v.s, s, n );
	else
		DString_SetMBS( res.v.s, s );
	return res;
}
DValue DValue_NewWCString( const wchar_t *s, int n )
{
	DValue res = daoNullValue;
	res.v.s = DString_New(0);
	if( n ){
		DString_Resize( res.v.s, n );
		memcpy( res.v.s->wcs, s, n * sizeof(wchar_t) );
	}else{
		DString_SetWCS( res.v.s, s );
	}
	return res;
}
#ifdef DAO_WITH_NUMARRAY
DValue DValue_NewVectorB( char *s, int n )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_INTEGER );
	DaoArray_SetVectorB( res.v.array, s, n );
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_NewVectorUB( unsigned char *s, int n )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_INTEGER );
	DaoArray_SetVectorUB( res.v.array, s, n );
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_NewVectorS( short *s, int n )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_INTEGER );
	DaoArray_SetVectorS( res.v.array, s, n );
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_NewVectorUS( unsigned short *s, int n )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_INTEGER );
	DaoArray_SetVectorUS( res.v.array, s, n );
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_NewVectorI( int *s, int n )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_INTEGER );
	if( n == 0 ){
		DaoArray_UseData( res.v.array, s );
	}else{
		DaoArray_SetVectorI( res.v.array, s, n );
	}
	return res;
}
DValue DValue_NewVectorUI( unsigned int *s, int n )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_INTEGER );
	if( n == 0 ){
		DaoArray_UseData( res.v.array, s );
	}else{
		DaoArray_SetVectorUI( res.v.array, s, n );
	}
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_NewVectorF( float *s, int n )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_FLOAT );
	if( n == 0 ){
		DaoArray_UseData( res.v.array, s );
	}else{
		DaoArray_SetVectorF( res.v.array, s, n );
	}
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_NewVectorD( double *s, int n )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_DOUBLE );
	if( n == 0 ){
		DaoArray_UseData( res.v.array, s );
	}else{
		DaoArray_SetVectorD( res.v.array, s, n );
	}
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_NewMatrixB( signed char **s, int n, int m )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_INTEGER );
	DaoArray_SetMatrixB( res.v.array, s, n, m );
	GC_IncRC( res.v.p );
	return res;
}
extern void DaoArray_SetMatrixUB( DaoArray *self, unsigned char **mat, int N, int M );
extern void DaoArray_SetMatrixUS( DaoArray *self, unsigned short **mat, int N, int M );
extern void DaoArray_SetMatrixUI( DaoArray *self, unsigned int **mat, int N, int M );
DValue DValue_NewMatrixUB( unsigned char **s, int n, int m )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_INTEGER );
	GC_IncRC( res.v.p );
	DaoArray_SetMatrixUB( res.v.array, s, n, m );
	return res;
}
DValue DValue_NewMatrixS( short **s, int n, int m )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_INTEGER );
	DaoArray_SetMatrixS( res.v.array, s, n, m );
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_NewMatrixUS( unsigned short **s, int n, int m )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_INTEGER );
	GC_IncRC( res.v.p );
	DaoArray_SetMatrixUS( res.v.array, s, n, m );
	return res;
}
DValue DValue_NewMatrixI( int **s, int n, int m )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_INTEGER );
	DaoArray_SetMatrixI( res.v.array, s, n, m );
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_NewMatrixUI( unsigned int **s, int n, int m )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_INTEGER );
	GC_IncRC( res.v.p );
	DaoArray_SetMatrixUI( res.v.array, s, n, m );
	return res;
}
DValue DValue_NewMatrixF( float **s, int n, int m )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_FLOAT );
	DaoArray_SetMatrixF( res.v.array, s, n, m );
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_NewMatrixD( double **s, int n, int m )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New( DAO_DOUBLE );
	DaoArray_SetMatrixD( res.v.array, s, n, m );
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_NewBuffer( void *p, int n )
{
	DValue res = daoNullArray;
	res.v.array = DaoArray_New(0);
	DaoArray_SetBuffer( res.v.array, p, n );
	GC_IncRC( res.v.p );
	return res;
}
#else
static DValue NumArrayDisabled()
{
	printf( "Error: numeric array is disabled!\n" );
	return daoNullValue;
}
DValue DValue_NewVectorB( char *s, int n )
{
	return NumArrayDisabled();
}
DValue DValue_NewVectorUB( unsigned char *s, int n )
{
	return NumArrayDisabled();
}
DValue DValue_NewVectorS( short *s, int n )
{
	return NumArrayDisabled();
}
DValue DValue_NewVectorUS( unsigned short *s, int n )
{
	return NumArrayDisabled();
}
DValue DValue_NewVectorI( int *s, int n )
{
	return NumArrayDisabled();
}
DValue DValue_NewVectorUI( unsigned int *s, int n )
{
	return NumArrayDisabled();
}
DValue DValue_NewVectorF( float *s, int n )
{
	return NumArrayDisabled();
}
DValue DValue_NewVectorD( double *s, int n )
{
	return NumArrayDisabled();
}
DValue DValue_NewMatrixB( signed char **s, int n, int m )
{
	return NumArrayDisabled();
}
DValue DValue_NewMatrixUB( unsigned char **s, int n, int m )
{
	return NumArrayDisabled();
}
DValue DValue_NewMatrixS( short **s, int n, int m )
{
	return NumArrayDisabled();
}
DValue DValue_NewMatrixUS( unsigned short **s, int n, int m )
{
	return NumArrayDisabled();
}
DValue DValue_NewMatrixI( int **s, int n, int m )
{
	return NumArrayDisabled();
}
DValue DValue_NewMatrixUI( unsigned int **s, int n, int m )
{
	return NumArrayDisabled();
}
DValue DValue_NewMatrixF( float **s, int n, int m )
{
	return NumArrayDisabled();
}
DValue DValue_NewMatrixD( double **s, int n, int m )
{
	return NumArrayDisabled();
}
DValue DValue_NewBuffer( void *s, int n )
{
	return NumArrayDisabled();
}
#endif
DValue DValue_NewStream( FILE *f )
{
	DValue res = daoNullStream;
	res.v.stream = DaoStream_New();
	DaoStream_SetFile( res.v.stream, f );
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_NewCData( DaoTypeBase *typer, void *data )
{
	DValue res = daoNullCData;
	res.v.cdata = DaoCData_New( typer, data );
	GC_IncRC( res.v.p );
	return res;
}
DValue DValue_WrapCData( DaoTypeBase *typer, void *data )
{
	DValue res = daoNullCData;
	res.v.cdata = DaoCData_Wrap( typer, data );
	GC_IncRC( res.v.p );
	return res;
}
void DValue_ClearAll( DValue *v, int n )
{
	int i;
	for(i=0; i<n; i++) DValue_Clear( v + i );
}
