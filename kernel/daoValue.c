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
#include"daoStream.h"
#include"daoRoutine.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoNumtype.h"
#include"daoNamespace.h"
#include"daoParser.h"
#include"daoContext.h"
#include"daoProcess.h"

#if 1
const DValue daoNullValue = { 0, 0, 0, 0, {0}};
const DValue daoZeroInt = { DAO_INTEGER, 0, 0, 0, {0}};
const DValue daoZeroFloat = { DAO_FLOAT, 0, 0, 0, {0}};
const DValue daoZeroDouble = { DAO_DOUBLE, 0, 0, 0, {0}};
const DValue daoNullComplex = { DAO_COMPLEX, 0, 0, 0, {0}};
const DValue daoNullLong = { DAO_LONG, 0, 0, 0, {0}};
const DValue daoNullString = { DAO_STRING, 0, 0, 0, {0}};
const DValue daoNullEnum = { DAO_ENUM, 0, 0, 0, {0}};
const DValue daoNullArray = { DAO_ARRAY, 0, 0, 0, {0}};
const DValue daoNullList = { DAO_LIST, 0, 0, 0, {0}};
const DValue daoNullMap = { DAO_MAP, 0, 0, 0, {0}};
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
	}else if( right.t == DAO_TUPLE && right.v.tuple->items->size == 2 ){
		res = DValue_Compare( left, right.v.tuple->items->data[0] );
		if( res <= 0 ) return res;
		res = DValue_Compare( left, right.v.tuple->items->data[1] );
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
	}else if( left.t == DAO_CTYPE && right.t == DAO_CTYPE ){
		return (int)( (size_t)right.v.cdata->data - (size_t)left.v.cdata->data );
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
	if( self->cst ) return;
	/* to allow class statics and namespace globals to be mutable:  */
	if( self->t != DAO_CLASS && self->t != DAO_NAMESPACE ) self->cst = 1;
	if( self->t >= DAO_ARRAY ){
		if( self->v.p->trait & DAO_DATA_CONST ) return;
		self->v.p->trait |= DAO_DATA_CONST;
	}
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
	case DAO_OBJECT :
		obj = self->v.object;
		n = obj->myClass->objDataDefault->size;
		for(i=1; i<n; i++) DValue_MarkConst( obj->objValues + i );
		if( obj->superObject == NULL ) break;
		for(i=0; i<obj->superObject->size; i++){
			oval.v.object = obj->superObject->items.pObject[i];
			if( oval.v.object == NULL || oval.v.object->type != DAO_OBJECT ) continue;
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
int DValue_Init( DValue *self, int type )
{
	if( self->t == type ) return type;
	if( self->t ) DValue_Clear( self );
	switch( type ){
	case DAO_NIL : break;
	case DAO_INTEGER : *self = daoZeroInt; break;
	case DAO_FLOAT   : *self = daoZeroFloat; break;
	case DAO_DOUBLE  : *self = daoZeroDouble; break;
	case DAO_COMPLEX : *self = daoNullComplex; self->v.c = dao_calloc(1,sizeof(complex16)); break;
	case DAO_LONG    : *self = daoNullLong; self->v.l = DLong_New(); break;
	case DAO_STRING  : *self = daoNullString; self->v.s = DString_New(1); break;
	}
	return self->t;
}

void DValue_CopyExt( DValue *self, DValue from, int copy )
{
	if( from.t >= DAO_COMPLEX && from.t == self->t && from.v.p == self->v.p ) return;
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
	}else if( tp->tid == DAO_VALTYPE ){
		if( DValue_Compare( from, tp->aux ) !=0 ) return 0;
		DValue_Copy( to, from );
		return 1;
	}else if( tp->tid == DAO_UNION ){
		DaoType *itp = NULL;
		int j, k, mt = 0;
		for(j=0; j<tp->nested->size; j++){
			k = DaoType_MatchValue( tp->nested->items.pType[j], from, NULL );
			if( k > mt ){
				itp = tp->nested->items.pType[j];
				mt = k;
			}
		}
		if( itp == NULL ) return 0;
		return DValue_Move( from, to, itp );
	}else if( from.t == 0 ){
		return 0;
	}
	to->sub = from.sub;
	to->cst = to->ndef = 0;
	if( from.t >= DAO_COMPLEX && from.t == to->t && from.v.p == to->v.p ) return 1;
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
			if( ((DaoObject*)dA)->myClass != tp->aux.v.klass ){
				dA = DaoObject_MapThisObject( (DaoObject*)dA, tp );
				i = (dA != NULL);
			}
		}else if( from.t == DAO_CLASS && tp->tid == DAO_CLASS && from.v.klass->typeHolders ){
			if( DMap_Find( from.v.klass->instanceClasses, tp->aux.v.klass->className ) ){
				from.v.klass = tp->aux.v.klass;
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
int DValue_Move2( DValue from, DValue *to, DaoType *totype )
{
	int rc = DValue_Move( from, to, totype );
	if( rc == 0 || totype == 0 ) return rc;
	if( from.t <= DAO_STREAM || from.t != totype->tid ) return rc;
	if( from.t == DAO_CDATA ){
		if( from.v.cdata->data == NULL ) rc = 0;
	}else{
		if( from.v.p == totype->value.v.p ) rc = 0;
	}
	return rc;
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
char* DValue_GetMBString( DValue *self )
{
	if( self->t != DAO_STRING ) return NULL;
	return DString_GetMBS( self->v.s );
}
wchar_t* DValue_GetWCString( DValue *self )
{
	if( self->t != DAO_STRING ) return NULL;
	return DString_GetWCS( self->v.s );
}
void* DValue_CastCData( DValue *self, DaoTypeBase *typer )
{
	if( self->t != DAO_CDATA ) return NULL;
	return DaoCData_CastData( self->v.cdata, typer );
}
void* DValue_GetCData( DValue *self )
{
	if( self->t != DAO_CDATA ) return NULL;
	return self->v.cdata->data;
}
void** DValue_GetCData2( DValue *self )
{
	if( self->t != DAO_CDATA ) return NULL;
	return & self->v.cdata->data;
}
void DValue_ClearAll( DValue *v, int n )
{
	int i;
	for(i=0; i<n; i++) DValue_Clear( v + i );
}


#define RADIX 32
static const char *mydigits = "0123456789ABCDEFGHIJKLMNOPQRSTUVW";

void DaoEncodeDouble( char *buf, double value )
{
	int expon, digit;
	double prod, frac;
	char *p = buf;
	if( value <0.0 ){
		p[0] = '-';
		p ++;
		value = -value;
	}
	frac = frexp( value, & expon );
	/* printf( "DaoEncodeDouble: frac = %f %f\n", frac, value ); */
	while(1){
		prod = frac * RADIX;
		digit = (int) prod;
		frac = prod - digit;
		sprintf( p, "%c", mydigits[ digit ] );
		p ++;
		if( frac <= 0 ) break;
	}
	sprintf( p, "_%i", expon );
	/* printf( "DaoEncodeDouble: %s, %g\n", buf, value ); */
	return;
}
double DaoDecodeDouble( char *buf )
{
	double frac = 0;
	int expon, sign = 1;
	char *p = buf;
	double factor = 1.0 / RADIX;
	double accum = factor;
	if( buf[0] == '-' ){
		p ++;
		sign = -1;
	}
	/* printf( "DaoDecodeDouble: %s\n", buf ); */
	while( *p && *p != '_' ){
		int digit = *p;
		digit -= digit >= 'A' ? 'A' - 10 : '0';
		frac += accum * digit;
		accum *= factor;
		p ++;
	}
	expon = strtol( p+1, NULL, 10 );
	/* printf( "DaoDecodeDouble: %f %f %f %s\n", frac, accum, ldexp( frac, expon ), p+1 ); */
	return ldexp( frac, expon ) * sign;
}

static void DaoSerializeDouble( double value, DString *serial )
{
	char buf[200];
	DaoEncodeDouble( buf, value );
	DString_AppendMBS( serial, buf );
}

static char *hex_digits = "ABCDEFGHIJKLMNOP";
static int DValue_Serialize2( DValue*, DString*, DaoNameSpace*, DaoVmProcess*, DaoType*, DString* );

static void DString_Serialize( DString *self, DString *serial, DString *buf )
{
	int i, c;
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
	DValue value = daoZeroInt;
	int i;
	DString_AppendChar( serial, '[' );
	for(i=0; i<self->dims->size; i++){
		value.v.i = self->dims->items.pSize[i];
		if( i ) DString_AppendChar( serial, ',' );
		DValue_GetString( value, buf );
		DString_Append( serial, buf );
	}
	DString_AppendChar( serial, ']' );
	switch( self->numType ){
	case DAO_INTEGER :
		for(i=0; i<self->size; i++){
			value.v.i = self->data.i[i];
			DValue_GetString( value, buf );
			if( i ) DString_AppendChar( serial, ',' );
			DString_Append( serial, buf );
		}
		break;
	case DAO_FLOAT :
		for(i=0; i<self->size; i++){
			if( i ) DString_AppendChar( serial, ',' );
			DaoSerializeDouble( self->data.f[i], serial );
		}
		break;
	case DAO_DOUBLE :
		for(i=0; i<self->size; i++){
			if( i ) DString_AppendChar( serial, ',' );
			DaoSerializeDouble( self->data.d[i], serial );
		}
		break;
	case DAO_COMPLEX :
		for(i=0; i<self->size; i++){
			if( i ) DString_AppendChar( serial, ',' );
			DaoSerializeDouble( self->data.c[i].real, serial );
			DString_AppendChar( serial, ' ' );
			DaoSerializeDouble( self->data.c[i].imag, serial );
		}
		break;
	}
}
static int DaoList_Serialize( DaoList *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc, DString *buf )
{
	DaoType *type = self->unitype;
	int i;
	if( type->nested && type->nested->size ) type = type->nested->items.pType[0];
	if( type && (type->tid == 0 || type->tid >= DAO_ENUM)) type = NULL;
	for(i=0; i<self->items->size; i++){
		DaoType *it = NULL;
		if( type == NULL ) it = DaoNameSpace_GetTypeV( ns, self->items->data[i] );
		if( i ) DString_AppendChar( serial, ',' );
		DValue_Serialize2( & self->items->data[i], serial, ns, proc, it, buf );
	}
	return 1;
}
static int DaoMap_Serialize( DaoMap *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc, DString *buf )
{
	DaoType *type = self->unitype;
	DaoType *keytype = NULL;
	DaoType *valtype = NULL;
	DNode *node;
	char *sep = self->items->hashing ? ":" : "=>";
	int i = 0;
	if( type->nested && type->nested->size >0 ) keytype = type->nested->items.pType[0];
	if( type->nested && type->nested->size >1 ) valtype = type->nested->items.pType[1];
	if( keytype && (keytype->tid == 0 || keytype->tid >= DAO_ENUM)) keytype = NULL;
	if( valtype && (valtype->tid == 0 || valtype->tid >= DAO_ENUM)) valtype = NULL;
	for(node=DMap_First(self->items); node; node=DMap_Next(self->items,node)){
		DaoType *kt = NULL, *vt = NULL;
		if( keytype == NULL ) kt = DaoNameSpace_GetTypeV( ns, *node->key.pValue );
		if( valtype == NULL ) vt = DaoNameSpace_GetTypeV( ns, *node->value.pValue );
		if( (i++) ) DString_AppendChar( serial, ',' );
		DValue_Serialize2( node->key.pValue, serial, ns, proc, kt, buf );
		DString_AppendMBS( serial, sep );
		DValue_Serialize2( node->value.pValue, serial, ns, proc, vt, buf );
	}
	return 1;
}
static int DaoTuple_Serialize( DaoTuple *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc, DString *buf )
{
	DArray *nested = self->unitype ? self->unitype->nested : NULL;
	int i;
	for(i=0; i<self->items->size; i++){
		DaoType *type = NULL;
		DaoType *it = NULL;
		if( nested && nested->size > i ) type = nested->items.pType[i];
		if( type && type->tid == DAO_PAR_NAMED ) type = type->aux.v.type;
		if( type && (type->tid == 0 || type->tid >= DAO_ENUM)) type = NULL;
		if( type == NULL ) it = DaoNameSpace_GetTypeV( ns, self->items->data[i] );
		if( i ) DString_AppendChar( serial, ',' );
		DValue_Serialize2( & self->items->data[i], serial, ns, proc, it, buf );
	}
	return 1;
}
static int DaoObject_Serialize( DaoObject *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc, DString *buf )
{
	DRoutine *rt;
	DaoType *type;
	DString name = DString_WrapMBS( "serialize" );
	DValue value = daoNullValue;
	DValue selfpar = daoNullObject;
	int i, errcode = DaoObject_GetData( self, & name, & value, NULL, NULL );
	if( errcode || (value.t != DAO_ROUTINE && value.t != DAO_FUNCTION) ) return 0;
	selfpar.v.object = self;
	rt = (DRoutine*) value.v.routine;
	rt = DRoutine_GetOverLoad( rt, &selfpar, NULL, 0, DVM_CALL );
	if( rt == NULL ) return 0;
	if( rt->type == DAO_ROUTINE ){
		DaoRoutine *rout = (DaoRoutine*) rt;
		DaoContext *ctx = DaoVmProcess_MakeContext( proc, rout );
		GC_ShiftRC( self, ctx->object );
		ctx->object = self;
		DaoContext_Init( ctx, rout );
		if( DRoutine_PassParams( rt, &selfpar, ctx->regValues, NULL, NULL, 0, DVM_CALL ) ){
			DaoVmProcess_PushContext( proc, ctx );
			proc->topFrame->returning = -1;
			DaoVmProcess_Execute( proc );
		}
	}else if( rt->type == DAO_FUNCTION ){
		DValue *p = & selfpar;
		DValue *res = & proc->returned;
		DaoFunction *func = (DaoFunction*) rt;
		DaoContext ctx = *proc->topFrame->context;
		DaoVmCode vmc = { 0, 0, 0, 0 };
		DaoType *types[] = { NULL, NULL, NULL };

		ctx.regValues = & res;
		ctx.regTypes = types;
		ctx.vmc = & vmc;

		DaoFunction_SimpleCall( func, & ctx, & p, 1 );
	}else{
		return 0;
	}
	type = DaoNameSpace_GetTypeV( ns, proc->returned );
	DValue_Serialize2( & proc->returned, serial, ns, proc, type, buf );
	return 1;
}
static int DaoCData_Serialize( DaoCData *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc, DString *buf )
{
	DaoType *type;
	DValue selfpar = daoNullCData;
	DValue *p = & selfpar;
	DValue *res = & proc->returned;
	DaoFunction *func = DaoFindFunction2( self->typer, "serialize" );
	DaoContext ctx = *proc->topFrame->context;
	DaoVmCode vmc = { 0, 0, 0, 0 };
	DaoType *types[] = { NULL, NULL, NULL };

	ctx.regValues = & res;
	ctx.regTypes = types;
	ctx.vmc = & vmc;

	if( func == NULL ) return 0;
	selfpar.v.cdata = self;
	func = (DaoFunction*)DRoutine_GetOverLoad( (DRoutine*)func, &selfpar, NULL, 0, DVM_CALL );
	if( func == NULL ) return 0;
	DaoFunction_SimpleCall( func, & ctx, & p, 1 );
	type = DaoNameSpace_GetTypeV( ns, proc->returned );
	DValue_Serialize2( & proc->returned, serial, ns, proc, type, buf );
	return 1;
}
int DValue_Serialize2( DValue *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc, DaoType *type, DString *buf )
{
	int rc = 1;
	if( type ){
		DString_Append( serial, type->name );
		DString_AppendChar( serial, '{' );
	}
	switch( self->t ){
	case DAO_INTEGER :
	case DAO_LONG :
	case DAO_ENUM :
		DValue_GetString( *self, buf );
		DString_Append( serial, buf );
		break;
	case DAO_FLOAT :
		DaoSerializeDouble( self->v.f, serial );
		break;
	case DAO_DOUBLE :
		DaoSerializeDouble( self->v.d, serial );
		break;
	case DAO_COMPLEX :
		DaoSerializeDouble( self->v.c->real, serial );
		DString_AppendChar( serial, ' ' );
		DaoSerializeDouble( self->v.c->imag, serial );
		break;
	case DAO_STRING :
		DString_Serialize( self->v.s, serial, buf );
		break;
	case DAO_ARRAY :
		DaoArray_Serialize( self->v.array, serial, buf );
		break;
	case DAO_LIST :
		rc = DaoList_Serialize( self->v.list, serial, ns, proc, buf );
		break;
	case DAO_MAP :
		rc = DaoMap_Serialize( self->v.map, serial, ns, proc, buf );
		break;
	case DAO_TUPLE :
		rc = DaoTuple_Serialize( self->v.tuple, serial, ns, proc, buf );
		break;
	case DAO_OBJECT :
		if( proc == NULL ) break;
		rc = DaoObject_Serialize( self->v.object, serial, ns, proc, buf );
		break;
	case DAO_CDATA :
		if( proc == NULL ) break;
		rc = DaoCData_Serialize( self->v.cdata, serial, ns, proc, buf );
		break;
	default :
		DString_AppendChar( serial, '?' );
		rc = 0;
		break;
	}
	if( type ) DString_AppendChar( serial, '}' );
	return rc;
}
int DValue_Serialize( DValue *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc )
{
	DaoType *type = DaoNameSpace_GetTypeV( ns, *self );
	DString *buf = DString_New(1);
	DString_Clear( serial );
	DString_ToMBS( serial );
	DValue_Serialize2( self, serial, ns, proc, type, buf );
	DString_Delete( buf );
	return 0;
}


int DaoParser_FindPairToken( DaoParser *self,  uchar_t lw, uchar_t rw, int start, int stop );
DaoType* DaoParser_ParseType( DaoParser *self, int start, int end, int *newpos, DArray *types );

static DaoObject* DaoClass_MakeObject( DaoClass *self, DValue param, DaoVmProcess *proc )
{
	DaoContext *ctx;
	DRoutine *rt = (DRoutine*) self->classRoutine;
	DValue *p = & param;
	rt = DRoutine_GetOverLoad( rt, NULL, & p, 1, DVM_CALL );
	if( rt == NULL || rt->type != DAO_ROUTINE ) return NULL;
	ctx = DaoVmProcess_MakeContext( proc, (DaoRoutine*) rt );
	DaoContext_Init( ctx, ctx->routine );
	if( DRoutine_PassParams( rt, NULL, ctx->regValues, & p, NULL, 1, DVM_CALL ) ){
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
static DaoCData* DaoCData_MakeObject( DaoCData *self, DValue param, DaoVmProcess *proc )
{
	DaoFunction *func = DaoFindFunction2( self->typer, self->typer->name );
	DaoContext ctx = *proc->topFrame->context;
	DaoVmCode vmc = { 0, 0, 0, 0 };
	DaoType *types[] = { self->typer->priv->abtype, NULL, NULL };
	DRoutine *rt = (DRoutine*) func;
	DValue *res = & proc->returned;
	DValue *p = & param;

	ctx.regValues = & res;
	ctx.regTypes = types;
	ctx.vmc = & vmc;
	if( func == NULL ) return NULL;
	rt = DRoutine_GetOverLoad( rt, NULL, & p, 1, DVM_CALL );
	if( func == NULL ) return NULL;
	DaoFunction_SimpleCall( func, & ctx, & p, 1 );
	if( proc->returned.t == DAO_CDATA ) return proc->returned.v.cdata;
	return NULL;
}

static int DaoParser_Deserialize2( DaoParser *self, int start, int end, DValue *value, DArray *types, DaoNameSpace *ns, DaoVmProcess *proc )
{
	DaoToken **tokens = self->tokens->items.pToken;
	DaoType *it1 = NULL, *it2 = NULL, *type = NULL;
	DaoObject *object;
	DaoCData *cdata;
	DaoArray *array;
	DaoTuple *tuple;
	DaoList *list;
	DaoMap *map;
	DNode *node;
	DValue tmp = daoNullValue;
	DValue tmp2 = daoNullValue;
	char *str;
	int i, j, k, n, rc = 1;
	int minus = 0;
	int next = start + 1;
	int tok2 = start < end ? tokens[start+1]->type : 0;
	int maybetype = tok2 == DTOK_COLON2 || tok2 == DTOK_LT || tok2 == DTOK_LCB;

	if( tokens[start]->type == DTOK_IDENTIFIER && maybetype ){
		type = DaoParser_ParseType( self, start, end, & start, NULL );
		if( type == NULL ) return start;
		if( tokens[start]->name != DTOK_LCB ) return start;
		end = DaoParser_FindPairToken( self, DTOK_LCB, DTOK_RCB, start, end );
		if( end < 0 ) return start;
		next = end + 1;
		start += 1;
		end -= 1;
	}
	if( type == NULL ) type = types->items.pType[0];
	if( type == NULL ) return next;
	DValue_Copy( value, type->value );
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
	switch( type->tid ){
	case DAO_INTEGER :
		value->v.i = (sizeof(dint) == 4) ? strtol( str, 0, 0 ) : strtoll( str, 0, 0 );
		if( minus ) value->v.i = - value->v.i;
		break;
	case DAO_FLOAT :
		value->v.f = DaoDecodeDouble( str );
		if( minus ) value->v.f = - value->v.f;
		break;
	case DAO_DOUBLE :
		value->v.d = DaoDecodeDouble( str );
		if( minus ) value->v.d = - value->v.d;
		break;
	case DAO_COMPLEX :
		value->v.c->real = DaoDecodeDouble( str );
		if( minus ) value->v.c->real = - value->v.c->real;
		if( start + 1 > end ) return start+1;
		minus = 0;
		if( tokens[start + 1]->name == DTOK_SUB ){
			minus = 1;
			start += 1;
			if( start + 1 > end ) return start+1;
		}
		value->v.c->imag = DaoDecodeDouble( tokens[start+1]->string->mbs );
		if( minus ) value->v.c->imag = - value->v.c->imag;
		next = start + 2;
		break;
	case DAO_STRING :
		n = tokens[start]->string->size - 1;
		for(i=1; i<n; i++){
			char c1 = str[i];
			char c2 = str[i+1];
			if( c1 < 'A' || c1 > 'P' ) continue;
			DString_AppendChar( value->v.s, (char)((c1-'A')*16 + (c2-'A')) );
			i += 1;
		}
		if( str[0] == '\"' ) DString_ToWCS( value->v.s );
		break;
	case DAO_ARRAY :
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
		array = value->v.array;
		DArray_Clear( array->dimAccum );
		for(i=start+1; i<k; i++){
			if( tokens[i]->name == DTOK_COMMA ) continue;
			j = strtol( tokens[i]->string->mbs, 0, 0 );
			DArray_Append( array->dimAccum, (size_t) j );
		}
		n = array->dimAccum->size;
		DaoArray_ResizeArray( array, array->dimAccum->items.pSize, n );
		if( it1->tid == DAO_COMPLEX ) tmp.t == DAO_COMPLEX;
		DArray_PushFront( types, it1 );
		n = 0;
		for(i=k+1; i<=end; i++){
			j = i + 1;
			while( j <= end && tokens[j]->name != DTOK_COMMA ) j += 1;
			if( it1->tid == DAO_COMPLEX ) tmp.v.c = array->data.c + n;
			DaoParser_Deserialize2( self, i, j-1, & tmp, types, ns, proc );
			switch( it1->tid ){
			case DAO_INTEGER : array->data.i[n] = tmp.v.i; break;
			case DAO_FLOAT   : array->data.f[n] = tmp.v.f; break;
			case DAO_DOUBLE  : array->data.d[n] = tmp.v.d; break;
			}
			i = j;
			n += 1;
		}
		DArray_PopFront( types );
		break;
	case DAO_LIST :
		list = value->v.list;
		DArray_PushFront( types, it1 );
		n = 0;
		for(i=start; i<=end; i++){
			if( tokens[i]->name == DTOK_COMMA ) continue;
			DVarray_Append( list->items, daoNullValue );
			k = DaoParser_Deserialize2( self, i, end, list->items->data + n, types, ns, proc );
			i = k - 1;
			n += 1;
		}
		DArray_PopFront( types );
		break;
	case DAO_MAP :
		map = value->v.map;
		n = 0;
		for(i=start; i<=end; i++){
			if( tokens[i]->name == DTOK_COMMA ) continue;
			DValue_Clear( & tmp );
			DValue_Clear( & tmp2 );
			DArray_PushFront( types, it1 );
			i = DaoParser_Deserialize2( self, i, end, &tmp, types, ns, proc );
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
			i = DaoParser_Deserialize2( self, i, end, &tmp2, types, ns, proc );
			DArray_PopFront( types );
			node = DMap_Insert( map->items, (void*)&tmp, (void*)&tmp2 );
			i -= 1;
			n += 1;
		}
		break;
	case DAO_TUPLE :
		tuple = value->v.tuple;
		n = 0;
		for(i=start; i<=end; i++){
			if( tokens[i]->name == DTOK_COMMA ) continue;
			it1 = NULL;
			if( type->nested && type->nested->size > n ){
				it1 = type->nested->items.pType[n];
				if( it1 && it1->tid == DAO_PAR_NAMED ) it1 = it1->aux.v.type;
			}
			DArray_PushFront( types, it1 );
			i = DaoParser_Deserialize2( self, i, end, tuple->items->data + n, types, ns, proc );
			DArray_PopFront( types );
			i -= 1;
			n += 1;
		}
		break;
	case DAO_OBJECT :
		DaoParser_Deserialize2( self, start, end, & tmp, types, ns, proc );
		if( tmp.t == 0 ) break;
		object = DaoClass_MakeObject( type->aux.v.klass, tmp, proc );
		if( object == NULL ) break;
		GC_ShiftRC( object, value->v.object );
		value->v.object = object;
		break;
	case DAO_CDATA :
		DaoParser_Deserialize2( self, start, end, & tmp, types, ns, proc );
		if( tmp.t == 0 ) break;
		cdata = DaoCData_MakeObject( type->aux.v.cdata, tmp, proc );
		if( cdata == NULL ) break;
		GC_ShiftRC( cdata, value->v.cdata );
		value->v.cdata = cdata;
		break;
	}
	DValue_Clear( & tmp );
	DValue_Clear( & tmp2 );
	return next;
}
int DValue_Deserialize( DValue *self, DString *serial, DaoNameSpace *ns, DaoVmProcess *proc )
{
	DaoParser *parser = DaoParser_New();
	DArray *types = DArray_New(0);
	int rc;

	DValue_Clear( self );
	parser->nameSpace = ns;
	parser->vmSpace = ns->vmSpace;
	DaoParser_LexCode( parser, DString_GetMBS( serial ), 0 );
	if( parser->tokens->size == 0 ) goto Failed;

	DArray_PushFront( types, NULL );
	rc = DaoParser_Deserialize2( parser, 0, parser->tokens->size-1, self, types, ns, proc );

	DaoParser_Delete( parser );
	DArray_Delete( types );
	return rc;
Failed:
	DaoParser_Delete( parser );
	DArray_Delete( types );
	return 0;
}
