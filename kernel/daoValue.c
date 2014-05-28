/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2014, Limin Fu
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

#include"stdlib.h"
#include"string.h"

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
#include"daoProcess.h"
#include"daoValue.h"


DaoConstant* DaoConstant_New( DaoValue *value )
{
	DaoConstant *self = (DaoConstant*) dao_calloc( 1, sizeof(DaoConstant) );
	DaoValue_Init( self, DAO_CONSTANT );
	DaoValue_Copy( value, & self->value );
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogNew( (DaoValue*) self );
#endif
	return self;
}
DaoVariable* DaoVariable_New( DaoValue *value, DaoType *type )
{
	DaoVariable *self = (DaoVariable*) dao_calloc( 1, sizeof(DaoVariable) );
	DaoValue_Init( self, DAO_VARIABLE );
	DaoValue_Move( value, & self->value, type );
	self->dtype = type;
	GC_IncRC( type );
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogNew( (DaoValue*) self );
#endif
	return self;
}

void DaoConstant_Delete( DaoConstant *self )
{
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogDelete( (DaoValue*) self );
#endif
	GC_DecRC( self->value );
	dao_free( self );
}
void DaoVariable_Delete( DaoVariable *self )
{
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogDelete( (DaoValue*) self );
#endif
	GC_DecRC( self->value );
	GC_DecRC( self->dtype );
	dao_free( self );
}
void DaoConstant_Set( DaoConstant *self, DaoValue *value )
{
	DaoValue_Copy( value, & self->value );
}
int DaoVariable_Set( DaoVariable *self, DaoValue *value, DaoType *type )
{
	if( type ){
		GC_ShiftRC( type, self->dtype );
		self->dtype = type;
	}
	return DaoValue_Move( value, & self->value, self->dtype );
}
void DaoVariable_SetType( DaoVariable *self, DaoType *type )
{
	GC_ShiftRC( type, self->dtype );
	self->dtype = type;
	if( self->value == NULL || self->value->type != type->value->type ){
		GC_DecRC( self->value );
		self->value = DaoValue_SimpleCopy( type->value );
		GC_IncRC( self->value );
	}
}

#ifdef DAO_WITH_NUMARRAY
int DaoArray_Compare( DaoArray *x, DaoArray *y );
#endif

#define number_compare( x, y ) ((x)==(y) ? 0 : ((x)<(y) ? -1 : 1))

/* Invalid comparison returns either -100 or 100: */

int DaoComplex_Compare( DaoComplex *left, DaoComplex *right )
{
	if( left->value.real < right->value.real ) return -100;
	if( left->value.real > right->value.real ) return  100;
	if( left->value.imag < right->value.imag ) return -100;
	if( left->value.imag > right->value.imag ) return  100;
	return 0;
}

int DaoEnum_Compare( DaoEnum *L, DaoEnum *R )
{
	DaoEnum E;
	DNode *N = NULL;
	DMap *ML = L->etype->mapNames;
	DMap *MR = R->etype->mapNames;
	char SL = L->etype->name->chars[0];
	char SR = R->etype->name->chars[0];
	if( L->etype == R->etype ){
		return L->value == R->value ? 0 : (L->value < R->value ? -1 : 1);
	}else if( L->subtype == DAO_ENUM_SYM && R->subtype == DAO_ENUM_SYM ){
		return DString_CompareUTF8( L->etype->name, R->etype->name );
	}else if( L->subtype == DAO_ENUM_SYM ){
		E = *R;
		if( DaoEnum_SetSymbols( & E, L->etype->name->chars ) == 0 ) goto CompareAddress;
		return E.value == R->value ? 0 : (E.value < R->value ? -1 : 1);
	}else if( R->subtype == DAO_ENUM_SYM ){
		E = *L;
		if( DaoEnum_SetSymbols( & E, R->etype->name->chars ) == 0 ) goto CompareAddress;
		return L->value == E.value ? 0 : (L->value < E.value ? -1 : 1);
	}
CompareAddress:
	return L->etype < R->etype ? -100 : 100;
}
int DaoTuple_Compare( DaoTuple *lt, DaoTuple *rt )
{
	int i, lb, rb, res;
	if( lt->size < rt->size ) return -100;
	if( lt->size > rt->size ) return 100;

	for(i=0; i<lt->size; i++){
		DaoValue *lv = lt->values[i];
		DaoValue *rv = rt->values[i];
		int lb = lv ? lv->type : 0;
		int rb = rv ? rv->type : 0;
		if( lb == rb && lb == DAO_TUPLE ){
			res = DaoTuple_Compare( (DaoTuple*) lv, (DaoTuple*) rv );
			if( res != 0 ) return res;
		}else if( lb != rb || lb ==0 || lb > DAO_ARRAY || lb == DAO_COMPLEX ){
			if( lv < rv ){
				return -100;
			}else if( lv > rv ){
				return 100;
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
	DaoValue **d1 = list1->value->items.pValue;
	DaoValue **d2 = list2->value->items.pValue;
	int size1 = list1->value->size;
	int size2 = list2->value->size;
	int min = size1 < size2 ? size1 : size2;
	int res = size1 == size2 ? 1 : 100;
	int i = 0, cmp = 0;
	/* find the first unequal items */
	while( i < min && (cmp = DaoValue_Compare(*d1, *d2)) ==0 ) i++, d1++, d2++;
	if( i < min ){
		if( abs( cmp > 1 ) ) return cmp;
		return cmp * res;
	}
	if( size1 == size2  ) return 0;
	return size1 < size2 ? -100 : 100;
}
int DaoCstruct_Compare( DaoValue *left, DaoValue *right )
{
	if( left == right ) return 0;
	if( left->xCstruct.ctype != right->xCstruct.ctype ){
		return number_compare( (size_t)left->xCstruct.ctype, (size_t)right->xCstruct.ctype );
	}
	if( left->type == DAO_CDATA ){
		return number_compare( (size_t)left->xCdata.data, (size_t)right->xCdata.data );
	}
	return number_compare( (size_t)left, (size_t)right );
}
int DaoValue_Compare( DaoValue *left, DaoValue *right )
{
	double L, R;
	int res = 0;
	if( left == right ) return 0;
	if( left == NULL || right == NULL ) return left < right ? -100 : 100;
	if( left->type != right->type ){
		res = left->type < right->type ? -100 : 100;
		if( right->type == DAO_TUPLE && right->xTuple.subtype == DAO_PAIR ){
			if( (res = DaoValue_Compare( left, right->xTuple.values[0] )) <= 0 ) return res;
			if( (res = DaoValue_Compare( left, right->xTuple.values[1] )) >= 0 ) return res;
			return 0;
		}else if( left->type == DAO_TUPLE && left->xTuple.subtype == DAO_PAIR ){
			if( (res = DaoValue_Compare( left->xTuple.values[0], right )) >= 0 ) return res;
			if( (res = DaoValue_Compare( left->xTuple.values[1], right )) <= 0 ) return res;
			return 0;
		}
		if( left->type < DAO_INTEGER || left->type > DAO_DOUBLE ) return res;
		if( right->type < DAO_INTEGER || right->type > DAO_DOUBLE ) return res;
		L = DaoValue_GetDouble( left );
		R = DaoValue_GetDouble( right );
		return L == R ? 0 : (L < R ? -1 : 1);
	}
	switch( left->type ){
	case DAO_NONE : return 0;
	case DAO_INTEGER : return number_compare( left->xInteger.value, right->xInteger.value );
	case DAO_FLOAT   : return number_compare( left->xFloat.value, right->xFloat.value );
	case DAO_DOUBLE  : return number_compare( left->xDouble.value, right->xDouble.value );
	case DAO_COMPLEX : return DaoComplex_Compare( & left->xComplex, & right->xComplex );
	case DAO_STRING  : return DString_CompareUTF8( left->xString.value, right->xString.value );
	case DAO_ENUM    : return DaoEnum_Compare( & left->xEnum, & right->xEnum );
	case DAO_TUPLE   : return DaoTuple_Compare( & left->xTuple, & right->xTuple );
	case DAO_LIST    : return DaoList_Compare( & left->xList, & right->xList );
	case DAO_CDATA :
	case DAO_CSTRUCT :
	case DAO_CTYPE : return DaoCstruct_Compare( left, right );
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY   : return DaoArray_Compare( & left->xArray, & right->xArray );
#endif
	}
	return left < right ? -100 : 100; /* needed for map */
}
int DaoValue_IsZero( DaoValue *self )
{
	if( self == NULL ) return 1;
	switch( self->type ){
	case DAO_NONE : return 1;
	case DAO_INTEGER : return self->xInteger.value == 0;
	case DAO_FLOAT  : return self->xFloat.value == 0.0;
	case DAO_DOUBLE : return self->xDouble.value == 0.0;
	case DAO_COMPLEX : return self->xComplex.value.real == 0.0 && self->xComplex.value.imag == 0.0;
	case DAO_ENUM : return self->xEnum.value == 0;
	}
	return 0;
}
static daoint DString_ToInteger( DString *self )
{
		return strtoll( self->chars, NULL, 0 );
}
double DString_ToDouble( DString *self )
{
	return strtod( self->chars, 0 );
}
daoint DaoValue_GetInteger( DaoValue *self )
{
	switch( self->type ){
	case DAO_INTEGER : return self->xInteger.value;
	case DAO_FLOAT   : return self->xFloat.value;
	case DAO_DOUBLE  : return self->xDouble.value;
	case DAO_COMPLEX : return self->xComplex.value.real;
	case DAO_STRING  : return DString_ToInteger( self->xString.value );
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
	case DAO_NONE    : return 0;
	case DAO_INTEGER : return self->xInteger.value;
	case DAO_FLOAT   : return self->xFloat.value;
	case DAO_DOUBLE  : return self->xDouble.value;
	case DAO_COMPLEX : return self->xComplex.value.real;
	case DAO_STRING  : return DString_ToDouble( self->xString.value );
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
static void DaoValue_BasicPrint( DaoValue *self, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	DaoType *type = DaoNamespace_GetType( proc->activeNamespace, self );
	if( self->type <= DAO_TUPLE )
		DaoStream_WriteChars( stream, coreTypeNames[ self->type ] );
	else
		DaoStream_WriteChars( stream, DaoValue_GetTyper( self )->name );
	if( self->type == DAO_NONE ) return;
	if( self->type == DAO_TYPE ){
		DaoStream_WriteChars( stream, "<" );
		DaoStream_WriteChars( stream, self->xType.name->chars );
		DaoStream_WriteChars( stream, ">" );
	}
	DaoStream_WriteChars( stream, "_" );
	DaoStream_WriteInt( stream, self->type );
	DaoStream_WriteChars( stream, "_" );
	DaoStream_WritePointer( stream, self );
	if( self->type <= DAO_TUPLE ) return;
	if( type && self == type->value ) DaoStream_WriteChars( stream, "[default]" );
}
void DaoValue_Print( DaoValue *self, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	DString *name;
	DaoTypeBase *typer;
	DMap *cd = cycData;
	if( self == NULL ){
		DaoStream_WriteChars( stream, "none[0x0]" );
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
		if( self->xComplex.value.imag >= -0.0 ) DaoStream_WriteChars( stream, "+" );
		DaoStream_WriteFloat( stream, self->xComplex.value.imag );
		DaoStream_WriteChars( stream, "C" );
		break;
	case DAO_ENUM  :
		name = DString_New();
		DaoEnum_MakeName( & self->xEnum, name );
		DaoStream_WriteChars( stream, name->chars );
		DaoStream_WriteChars( stream, "(" );
		DaoStream_WriteInt( stream, self->xEnum.value );
		DaoStream_WriteChars( stream, ")" );
		DString_Delete( name );
		break;
	case DAO_STRING  :
		DaoStream_WriteString( stream, self->xString.value );
		break;
	default :
		typer = DaoVmSpace_GetTyper( self->type );
		if( typer->core->Print == DaoValue_Print ){
			DaoValue_BasicPrint( self, proc, stream, cycData );
			break;
		}
		typer->core->Print( self, proc, stream, cycData );
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
DString* DaoValue_GetString( DaoValue *self, DString *str )
{
	char chs[100] = {0};
	DString_Clear( str );
	switch( self->type ){
	case DAO_INTEGER : sprintf( chs, "%" DAO_INT_FORMAT, self->xInteger.value ); break;
	case DAO_FLOAT   : sprintf( chs, "%g", self->xFloat.value ); break;
	case DAO_DOUBLE  : sprintf( chs, "%g", self->xDouble.value ); break;
	case DAO_COMPLEX : sprintf( chs, (self->xComplex.value.imag < 0) ? "%g%gC" : "%g+%gC", self->xComplex.value.real, self->xComplex.value.imag ); break;
	case DAO_ENUM : DaoEnum_MakeName( & self->xEnum, str ); break;
	case DAO_STRING : DString_Assign( str, self->xString.value ); break;
	default : break;
	}
	if( self->type <= DAO_COMPLEX ) DString_SetChars( str, chs );
	return str;
}
void DaoValue_MarkConst( DaoValue *self )
{
	DMap *map;
	DNode *it;
	daoint i, n;
	if( self == NULL || (self->xBase.trait & DAO_VALUE_CONST) ) return;
	self->xBase.trait |= DAO_VALUE_CONST;
	switch( self->type ){
	case DAO_LIST :
		for(i=0,n=self->xList.value->size; i<n; i++)
			DaoValue_MarkConst( self->xList.value->items.pValue[i] );
		break;
	case DAO_TUPLE :
		for(i=0,n=self->xTuple.size; i<n; i++)
			DaoValue_MarkConst( self->xTuple.values[i] );
		break;
	case DAO_MAP :
		map = self->xMap.value;
		for(it=DMap_First( map ); it != NULL; it = DMap_Next(map, it) ){
			DaoValue_MarkConst( it->key.pValue );
			DaoValue_MarkConst( it->value.pValue );
		}
		break;
	case DAO_OBJECT :
		n = self->xObject.defClass->instvars->size;
		for(i=1; i<n; i++) DaoValue_MarkConst( self->xObject.objValues[i] );
		if( self->xObject.parent ) DaoValue_MarkConst( self->xObject.parent );
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


DaoValue* DaoValue_CopyContainer( DaoValue *self, DaoType *tp )
{
	switch( self->type ){
	case DAO_LIST  : return (DaoValue*) DaoList_Copy( (DaoList*) self, tp );
	case DAO_MAP   : return (DaoValue*) DaoMap_Copy( (DaoMap*) self, tp );
	case DAO_TUPLE : return (DaoValue*) DaoTuple_Copy( (DaoTuple*) self, tp );
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY : return (DaoValue*) DaoArray_CopyX( (DaoArray*) self, tp );
#endif
	default : break;
	}
	return self;
}

/*
// Assumming the value "self" is compatible to the type "tp", if it is not null.
*/
DaoValue* DaoValue_SimpleCopyWithTypeX( DaoValue *self, DaoType *tp, DaoType *cst )
{
	if( self == NULL ) return dao_none_value;
	if( (tp == NULL || tp->tid == self->type) && self->type < DAO_ENUM ){
		if( cst && cst->invar ) return self;
		switch( self->type ){
		case DAO_NONE : return self;
		case DAO_INTEGER : return (DaoValue*) DaoInteger_New( self->xInteger.value );
		case DAO_FLOAT   : return (DaoValue*) DaoFloat_New( self->xFloat.value );
		case DAO_DOUBLE  : return (DaoValue*) DaoDouble_New( self->xDouble.value );
		case DAO_COMPLEX : return (DaoValue*) DaoComplex_New( self->xComplex.value );
		case DAO_STRING  : return (DaoValue*) DaoString_Copy( & self->xString );
		}
		return self; /* unreachable; */
	}else if( self->type == DAO_ENUM ){
		switch( tp ? tp->tid : 0 ){
		case DAO_INTEGER : return (DaoValue*) DaoInteger_New( self->xEnum.value );
		case DAO_FLOAT   : return (DaoValue*) DaoFloat_New( self->xEnum.value );
		case DAO_DOUBLE  : return (DaoValue*) DaoDouble_New( self->xEnum.value );
		case DAO_ENUM : return (DaoValue*) DaoEnum_Copy( & self->xEnum, tp );
		}
		return (DaoValue*) DaoEnum_Copy( & self->xEnum, NULL );
	}else if( tp && tp->tid >= DAO_INTEGER && tp->tid <= DAO_DOUBLE ){
		DaoValue *value = NULL;
		switch( tp->tid ){
		case DAO_INTEGER : value = (DaoValue*) DaoInteger_New(0); break;
		case DAO_FLOAT   : value = (DaoValue*) DaoFloat_New(0);   break;
		case DAO_DOUBLE  : value = (DaoValue*) DaoDouble_New(0);  break;
		}
		switch( tp->tid ){
		case DAO_INTEGER : value->xInteger.value = DaoValue_GetInteger( self ); break;
		case DAO_FLOAT   : value->xFloat.value   = DaoValue_GetFloat( self );   break;
		case DAO_DOUBLE  : value->xDouble.value  = DaoValue_GetDouble( self );  break;
		}
		return value;
	}
#ifdef DAO_WITH_NUMARRAY
	if( self->type == DAO_ARRAY && self->xArray.original ){
		DaoArray_Sliced( (DaoArray*)self );
		return self;
	}else
#endif
	if( self->type == DAO_CSTRUCT || self->type == DAO_CDATA ){
		FuncPtrSliced sliced = self->xCstruct.ctype->kernel->Sliced;
		if( sliced ) (*sliced)( self );
		return self;
	}
	if( tp == NULL ){
		switch( self->type ){
		case DAO_LIST  :
			if( self->xList.ctype == dao_type_list_empty ) tp = dao_type_list_empty;
			break;
		case DAO_MAP   :
			if( self->xMap.ctype == dao_type_map_empty ) tp = dao_type_map_empty;
			break;
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			if( self->xMap.ctype == dao_type_array_empty ) tp = dao_type_array_empty;
			break;
#endif
		default : break;
		}
		if( tp ) tp = DaoType_GetBaseType( tp );
	}
	if( self->xBase.trait & DAO_VALUE_NOCOPY ) return self;
	if( (self->xBase.trait & DAO_VALUE_CONST) == 0 ) return self;
	if( cst != NULL && cst->invar != 0 ) return self;
	return DaoValue_CopyContainer( self, tp );
}
DaoValue* DaoValue_SimpleCopyWithType( DaoValue *self, DaoType *tp )
{
	return DaoValue_SimpleCopyWithTypeX( self, tp, NULL );
}
DaoValue* DaoValue_SimpleCopy( DaoValue *self )
{
	return DaoValue_SimpleCopyWithTypeX( self, NULL, NULL );
}

void DaoValue_CopyX( DaoValue *src, DaoValue **dest, DaoType *cst )
{
	DaoValue *dest2 = *dest;
	if( src == dest2 ) return;
	if( dest2 && dest2->xBase.refCount >1 ){
		GC_DecRC( dest2 );
		*dest = dest2 = NULL;
	}
	if( dest2 == NULL ){
		src = DaoValue_SimpleCopyWithTypeX( src, NULL, cst );
		GC_IncRC( src );
		*dest = src;
		return;
	}
	if( src->type != dest2->type || src->type > DAO_ENUM ){
		src = DaoValue_SimpleCopyWithTypeX( src, NULL, cst );
		GC_ShiftRC( src, dest2 );
		*dest = src;
		return;
	}
	switch( src->type ){
	case DAO_ENUM    :
		DaoEnum_SetType( & dest2->xEnum, src->xEnum.etype );
		DaoEnum_SetValue( & dest2->xEnum, & src->xEnum );
		break;
	case DAO_INTEGER : dest2->xInteger.value = src->xInteger.value; break;
	case DAO_FLOAT   : dest2->xFloat.value = src->xFloat.value; break;
	case DAO_DOUBLE  : dest2->xDouble.value = src->xDouble.value; break;
	case DAO_COMPLEX : dest2->xComplex.value = src->xComplex.value; break;
	case DAO_STRING  : DString_Assign( dest2->xString.value, src->xString.value ); break;
	}
}
void DaoValue_Copy( DaoValue *src, DaoValue **dest )
{
	DaoValue_CopyX( src, dest, NULL );
}
void DaoValue_SetType( DaoValue *to, DaoType *tp )
{
	DaoType *tp2;
	DNode *it;
	if( to->type != tp->tid && tp->tid != DAO_ANY ) return;
	if( tp->attrib & DAO_TYPE_SPEC ) return;
	switch( to->type ){
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		if( to->xArray.size ) return;
		if( tp->tid != DAO_ARRAY || tp->nested == NULL || tp->nested->size == 0 ) break;
		tp = tp->nested->items.pType[0];
		if( tp->tid == DAO_NONE || tp->tid > DAO_COMPLEX ) break;
		DaoArray_SetNumType( (DaoArray*) to, tp->tid );
		break;
#endif
	case DAO_LIST :
		/* v : any = {}, v->ctype should be list<any> */
		if( tp->tid == DAO_ANY ) tp = dao_type_list_any;
		if( to->xList.ctype && !(to->xList.ctype->attrib & DAO_TYPE_UNDEF) ) break;
		GC_ShiftRC( tp, to->xList.ctype );
		to->xList.ctype = tp;
		break;
	case DAO_MAP :
		if( tp->tid == DAO_ANY ) tp = dao_type_map_any;
		if( to->xMap.ctype && !(to->xMap.ctype->attrib & DAO_TYPE_UNDEF) ) break;
		GC_ShiftRC( tp, to->xMap.ctype );
		to->xMap.ctype = tp;
		break;
	case DAO_TUPLE :
		tp2 = to->xTuple.ctype;
		if( tp->tid == DAO_ANY ) break;
		if( tp->nested->size ==0 ) break; /* not to the generic tuple type */
		if( tp2 == NULL || tp2->mapNames == NULL || tp2->mapNames->size ==0 ){
			GC_ShiftRC( tp, to->xTuple.ctype );
			to->xTuple.ctype = tp;
			break;
		}
		if( tp->mapNames == NULL || tp->mapNames->size ) break;
		for(it=DMap_First(tp2->mapNames); it!=NULL; it=DMap_Next(tp2->mapNames, it)){
			if( DMap_Find( tp->mapNames, it->key.pVoid ) == NULL ) break;
		}
		if( it ) break;
		GC_ShiftRC( tp, to->xTuple.ctype );
		to->xTuple.ctype = tp;
		break;
	default : break;
	}
}
static int DaoValue_TryCastTuple( DaoValue *src, DaoValue **dest, DaoType *tp )
{
	DaoTuple *tuple;
	DaoType **item_types = tp->nested->items.pType;
	DaoType *totype = src->xTuple.ctype;
	DaoValue **data = src->xTuple.values;
	DMap *names = totype ? totype->mapNames : NULL;
	DNode *node, *search;
	daoint i, T = tp->nested->size;
	int tm, eqs = 0;
	/*
	// auto-cast tuple type, on the following conditions:
	// (1) the item values of "dest" must match exactly to the item types of "tp";
	// (2) "tp->mapNames" must contain "(*dest)->xTuple.ctype->mapNames";
	*/
	if( src->xTuple.ctype == NULL ){
		GC_IncRC( tp );
		src->xTuple.ctype = tp;
		return 1;
	}
	if( DaoType_MatchValue( tp, src, NULL ) < DAO_MT_SUB ) return 1;
	/*
	// casting is not necessary if the tuple's field names are a superset of the
	// field names of the target type:
	*/
	if( tp->mapNames == NULL || tp->mapNames->size ==0 ) goto Finalize;
	if( names ){
		daoint count = 0;
		for(node=DMap_First(names); node; node=DMap_Next(names,node)){
			search = DMap_Find( tp->mapNames, node->key.pVoid );
			if( search && node->value.pInt != search->value.pInt ) return 0;
			count += search != NULL;
		}
		/* be superset of the field names of the target type: */
		if( count == tp->mapNames->size ) goto Finalize;
	}
Finalize:
	tuple = DaoTuple_New( T );
	for(i=0; i<T; i++){
		DaoType *it = item_types[i];
		if( it->tid == DAO_PAR_NAMED ) it = & it->aux->xType;
		DaoValue_Move( data[i], tuple->values+i, it );
	}
	GC_IncRC( tp );
	tuple->ctype = tp;
	GC_ShiftRC( tuple, *dest );
	*dest = (DaoValue*) tuple;
	return 1;
}
static int DaoValue_Move4( DaoValue *S, DaoValue **D, DaoType *T, DaoType *C, DMap *defs );
static int DaoValue_Move5( DaoValue *S, DaoValue **D, DaoType *T, DaoType *C, DMap *defs );
static int DaoValue_MoveVariant( DaoValue *src, DaoValue **dest, DaoType *tp, DaoType *C )
{
	DaoType *itp = NULL;
	int n = tp->nested->size;
	int j, k, mt;
	for(j=0,mt=0; j<n; j++){
		DaoType *itp2 = tp->nested->items.pType[j];
		k = DaoType_MatchValue( itp2, src, NULL );
		if( k > mt ){
			itp = itp2;
			mt = k;
			if( mt == DAO_MT_EQ ) break;
		}
	}
	if( itp == NULL ) return 0;
	return DaoValue_Move5( src, dest, itp, C, NULL );
}
int DaoValue_Move4( DaoValue *S, DaoValue **D, DaoType *T, DaoType *C, DMap *defs )
{
	int tm = 1;
	switch( (T->tid << 8) | S->type ){
	case (DAO_INTEGER << 8) | DAO_INTEGER :
	case (DAO_INTEGER << 8) | DAO_FLOAT   :
	case (DAO_INTEGER << 8) | DAO_DOUBLE  :
	case (DAO_FLOAT   << 8) | DAO_INTEGER :
	case (DAO_FLOAT   << 8) | DAO_FLOAT   :
	case (DAO_FLOAT   << 8) | DAO_DOUBLE  :
	case (DAO_DOUBLE  << 8) | DAO_INTEGER :
	case (DAO_DOUBLE  << 8) | DAO_FLOAT   :
	case (DAO_DOUBLE  << 8) | DAO_DOUBLE  :
	case (DAO_COMPLEX << 8) | DAO_COMPLEX :
	case (DAO_STRING  << 8) | DAO_STRING  :
		S = DaoValue_SimpleCopyWithTypeX( S, T, C );
		GC_ShiftRC( S, *D );
		*D = S;
		return 1;
	}
	switch( S->type ){
	case DAO_OBJECT : if( S->xObject.isNull ) return 0; break;
	case DAO_CDATA  : if( S->xCdata.data == NULL ) return 0; break;
	}
	if( !(S->xBase.trait & DAO_VALUE_CONST) ){
		DaoType *ST = NULL;
		switch( (S->type << 8) | T->tid ){
		case (DAO_TUPLE<<8)|DAO_TUPLE : ST = S->xTuple.ctype; break;
		case (DAO_ARRAY<<8)|DAO_ARRAY : ST = dao_array_types[ S->xArray.etype ]; break;
		case (DAO_LIST <<8)|DAO_LIST  : ST = S->xList.ctype; break;
		case (DAO_MAP  <<8)|DAO_MAP   : ST = S->xMap.ctype; break;
		case (DAO_CDATA<<8)|DAO_CDATA : ST = S->xCdata.ctype; break;
		case (DAO_CSTRUCT<<8)|DAO_CSTRUCT : ST = S->xCstruct.ctype; break;
		case (DAO_OBJECT<<8)|DAO_OBJECT : ST = S->xObject.defClass->objType; break;
		}
		if( ST == T ){
			DaoValue *D2 = *D;
			GC_ShiftRC( S, D2 );
			*D = S;
			return 1;
		}
	}
	if( (T->tid == DAO_OBJECT || T->tid == DAO_CDATA || T->tid == DAO_CSTRUCT) && S->type == DAO_OBJECT ){
		if( S->xObject.defClass != & T->aux->xClass ){
			S = DaoObject_CastToBase( S->xObject.rootObject, T );
			tm = (S != NULL);
		}
	}else if( (T->tid == DAO_CLASS || T->tid == DAO_CTYPE) && S->type == DAO_CLASS ){
		if( S->xClass.clsType != T && T->aux != NULL ){ /* T->aux == NULL for "class"; */
			S = DaoClass_CastToBase( (DaoClass*)S, T );
			tm = (S != NULL);
		}
	}else if( T->tid == DAO_CTYPE && S->type == DAO_CTYPE ){
		if( S->xCtype.ctype != T ){
			S = DaoType_CastToParent( S, T );
			tm = (S != NULL);
		}
	}else if( T->tid == DAO_ROUTINE && T->subtid != DAO_ROUTINES && S->type == DAO_ROUTINE && S->xRoutine.overloads ){
		DArray *routines = S->xRoutine.overloads->routines;
		int i, k, n;
		/*
		// Do not use DaoRoutine_ResolveByType( S, ... )
		// "S" should match to "T", not the other way around!
		*/
		tm = 0;
		for(i=0,n=routines->size; i<n; i++){
			DaoRoutine *rout = routines->items.pRoutine[i];
			k = rout->routType == T ? DAO_MT_EQ : DaoType_MatchTo( rout->routType, T, defs );
			if( k > tm ) tm = k;
			if( rout->routType == T ){
				S = (DaoValue*) rout;
				break;
			}
		}
	}else{
		tm = DaoType_MatchValue( T, S, defs );
	}
#if 0
	if( tm ==0 ){
		printf( "T = %p; S = %p, type = %i %i\n", T, S, S->type, DAO_ROUTINE );
		printf( "T: %s %i %i\n", T->name->chars, T->tid, tm );
		if( S->type == DAO_LIST ) printf( "%s\n", S->xList.ctype->name->chars );
		if( S->type == DAO_TUPLE ) printf( "%p\n", S->xTuple.ctype );
	}
	printf( "S->type = %p %s %i\n", S, T->name->chars, tm );
#endif
	if( tm == 0 ) return 0;
	/*
	// composite known types must match exactly. example,
	// where it will not work if composite types are allowed to match loosely.
	// d : list<list<int>> = {};
	// e : list<float> = { 1.0 };
	// d.append( e );
	//
	// but if d is of type list<list<any>>,
	// the matching do not necessary to be exact.
	*/
	S = DaoValue_SimpleCopyWithTypeX( S, T, C );
	GC_ShiftRC( S, *D );
	*D = S;
	if( S->type == DAO_TUPLE && S->xTuple.ctype != T && tm == DAO_MT_SIM ){
		return DaoValue_TryCastTuple( S, D, T );
	}else if( T && T->tid == S->type && !(T->attrib & DAO_TYPE_SPEC) ){
		DaoValue_SetType( S, T );
	}
	return 1;
}
int DaoValue_Move5( DaoValue *S, DaoValue **D, DaoType *T, DaoType *C, DMap *defs )
{
	DaoValue *D2 = *D;
	if( S == D2 ) return 1;
	if( S == NULL ){
		GC_DecRC( *D );
		*D = NULL;
		return 0;
	}
	if( T == NULL ){
		DaoValue_CopyX( S, D, C );
		return 1;
	}
	if( T->valtype ){
		if( DaoValue_Compare( S, T->aux ) !=0 ) return 0;
		DaoValue_CopyX( S, D, C );
		return 1;
	}
	switch( T->tid ){
	case DAO_UDT :
		DaoValue_CopyX( S, D, C );
		return 1;
	case DAO_THT :
		if( T->aux ) return DaoValue_Move5( S, D, (DaoType*) T->aux, C, defs );
		DaoValue_CopyX( S, D, C );
		return 1;
	case DAO_ANY :
		DaoValue_CopyX( S, D, C );
		DaoValue_SetType( *D, T );
		return 1;
	case DAO_VARIANT :
		return DaoValue_MoveVariant( S, D, T, C );
	default : break;
	}
	if( S->type >= DAO_OBJECT || !(S->xBase.trait & DAO_VALUE_CONST) || T->invar ){
		int fastmove = 0;
		switch( T->tid ){
		case DAO_LIST :
		case DAO_MAP :
		case DAO_CSTRUCT :
		case DAO_CDATA : 
		case DAO_CTYPE : fastmove = S->xCstruct.ctype == T; break;
		case DAO_TUPLE : fastmove = S->xTuple.ctype == T; break;
		case DAO_ROUTINE : fastmove = S->xRoutine.routType == T; break;
		case DAO_CLASS  : fastmove = S->xClass.clsType == T; break;
		case DAO_OBJECT : fastmove = S->xObject.defClass->objType == T; break;
		default : break;
		}
		if( fastmove ){
			GC_ShiftRC( S, D2 );
			*D = S;
			return 1;
		}
	}
	if( D2 && D2->xBase.refCount > 1 ){
		GC_DecRC( D2 );
		*D = D2 = NULL;
	}
	if( D2 && S->type == D2->type && S->type == T->tid && S->type <= DAO_ENUM ){
		switch( S->type ){
		case DAO_ENUM    :
			DaoEnum_SetType( & D2->xEnum, T );
			return DaoEnum_SetValue( & D2->xEnum, & S->xEnum );
		case DAO_INTEGER : D2->xInteger.value = S->xInteger.value; break;
		case DAO_FLOAT   : D2->xFloat.value = S->xFloat.value; break;
		case DAO_DOUBLE  : D2->xDouble.value = S->xDouble.value; break;
		case DAO_COMPLEX : D2->xComplex.value = S->xComplex.value; break;
		case DAO_STRING  : DString_Assign( D2->xString.value, S->xString.value ); break;
		}
		return 1;
	}
	if( D2 == NULL || D2->type != T->tid ) return DaoValue_Move4( S, D, T, C, defs );

	switch( (S->type << 8) | T->tid ){
	case (DAO_STRING<<8)|DAO_STRING :
		DString_Assign( D2->xString.value, S->xString.value );
		break;
	case (DAO_ENUM<<8)|DAO_ENUM :
		DaoEnum_SetType( & D2->xEnum, T );
		DaoEnum_SetValue( & D2->xEnum, & S->xEnum );
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
	default : return DaoValue_Move4( S, D, T, C, defs );
	}
	return 1;
}
int DaoValue_Move( DaoValue *S, DaoValue **D, DaoType *T )
{
	return DaoValue_Move5( S, D, T, T, NULL );
}
int DaoValue_Move2( DaoValue *S, DaoValue **D, DaoType *T, DMap *defs )
{
	int rc = DaoValue_Move5( S, D, T, T, defs );
	if( rc == 0 || T == NULL ) return rc;
	if( S->type <= DAO_TUPLE || S->type != T->tid ) return rc;
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
	if( self == NULL || self->type != DAO_CSTRUCT ) return NULL;
	return (DaoStream*) DaoValue_CastCstruct( self, dao_type_stream );
}
DaoObject* DaoValue_CastObject( DaoValue *self )
{
	if( self == NULL || self->type != DAO_OBJECT ) return NULL;
	return (DaoObject*) self;
}
DaoCstruct* DaoValue_CastCstruct( DaoValue *self, DaoType *type )
{
	if( self == NULL || type == NULL ) return (DaoCstruct*) self;
	if( self->type != DAO_CSTRUCT && self->type != DAO_CDATA ) return NULL;
	if( DaoType_ChildOf( self->xCstruct.ctype, type ) ) return (DaoCstruct*) self;
	return NULL;
}
DaoCdata* DaoValue_CastCdata( DaoValue *self, DaoType *type )
{
	if( self == NULL || type == NULL ) return (DaoCdata*) self;
	if( self->type != DAO_CDATA ) return NULL;
	if( DaoType_ChildOf( self->xCdata.ctype, type ) ) return (DaoCdata*) self;
	return NULL;
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
DaoRoutine* DaoValue_CastRoutine( DaoValue *self )
{
	if( self == NULL || self->type != DAO_ROUTINE ) return NULL;
	return (DaoRoutine*) self;
}
DaoProcess* DaoValue_CastProcess( DaoValue *self )
{
	if( self == NULL || self->type != DAO_PROCESS ) return NULL;
	return (DaoProcess*) self;
}
DaoNamespace* DaoValue_CastNamespace( DaoValue *self )
{
	if( self == NULL || self->type != DAO_NAMESPACE ) return NULL;
	return (DaoNamespace*) self;
}
DaoType* DaoValue_CastType( DaoValue *self )
{
	if( self == NULL || self->type != DAO_TYPE ) return NULL;
	return (DaoType*) self;
}

DaoValue* DaoValue_MakeNone()
{
	return dao_none_value;
}

daoint DaoValue_TryGetInteger( DaoValue *self )
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
int DaoValue_TryGetEnum(DaoValue *self)
{
	if( self->type != DAO_ENUM ) return 0;
	return self->xEnum.value;
}
complex16 DaoValue_TryGetComplex( DaoValue *self )
{
	complex16 com = {0.0,0.0};
	if( self->type != DAO_COMPLEX ) return com;
	return self->xComplex.value;
}
char* DaoValue_TryGetChars( DaoValue *self )
{
	if( self->type != DAO_STRING ) return NULL;
	return DString_GetData( self->xString.value );
}
DString* DaoValue_TryGetString( DaoValue *self )
{
	if( self->type != DAO_STRING ) return NULL;
	return self->xString.value;
}
void* DaoValue_TryGetArray( DaoValue *self )
{
	if( self->type != DAO_ARRAY ) return NULL;
	return self->xArray.data.p;
}
void* DaoValue_TryCastCdata( DaoValue *self, DaoType *type )
{
	if( self->type != DAO_CDATA ) return NULL;
	return DaoCdata_CastData( & self->xCdata, type );
}
void* DaoValue_TryGetCdata( DaoValue *self )
{
	if( self->type != DAO_CDATA ) return NULL;
	return self->xCdata.data;
}
void** DaoValue_TryGetCdata2( DaoValue *self )
{
	if( self->type != DAO_CDATA ) return NULL;
	return & self->xCdata.data;
}
void DaoValue_ClearAll( DaoValue *v[], int n )
{
	int i;
	for(i=0; i<n; i++) DaoValue_Clear( v + i );
}

void DaoProcess_CacheValue( DaoProcess *self, DaoValue *value )
{
	DArray_Append( self->factory, NULL );
	GC_IncRC( value );
	self->factory->items.pValue[ self->factory->size - 1 ] = value;
}
void DaoProcess_PopValues( DaoProcess *self, int N )
{
	if( N < 0 ) return;
	if( N >= (int)self->factory->size ){
		DArray_Clear( self->factory );
	}else{
		DArray_Erase( self->factory, self->factory->size - N, N );
	}
}
DaoValue** DaoProcess_GetLastValues( DaoProcess *self, int N )
{
	if( N > (int)self->factory->size ) return self->factory->items.pValue;
	return self->factory->items.pValue + (self->factory->size - N);
}

DaoNone* DaoProcess_NewNone( DaoProcess *self )
{
	DaoNone *res = DaoNone_New();
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoInteger* DaoProcess_NewInteger( DaoProcess *self, daoint v )
{
	DaoInteger *res = DaoInteger_New( v );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoFloat* DaoProcess_NewFloat( DaoProcess *self, float v )
{
	DaoFloat *res = DaoFloat_New( v );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoDouble* DaoProcess_NewDouble( DaoProcess *self, double v )
{
	DaoDouble *res = DaoDouble_New( v );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoComplex* DaoProcess_NewComplex( DaoProcess *self, complex16 v )
{
	DaoComplex *res = DaoComplex_New( v );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoString* DaoProcess_NewString( DaoProcess *self, const char *s, daoint n )
{
	DaoString *res = DaoString_New();
	if( s ) DString_SetBytes( res->value, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoEnum* DaoProcess_NewEnum( DaoProcess *self, DaoType *type, int value )
{
	DaoEnum *res = DaoEnum_New( type, value );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoTuple* DaoProcess_NewTuple( DaoProcess *self, int count )
{
	int i, N = abs( count );
	int M = self->factory->size;
	DaoValue **values = self->factory->items.pValue;
	DaoTuple *res = NULL;
	if( count < 0 ){
		if( M < N ) return NULL;
		res = DaoTuple_New( N );
		for(i=0; i<N; i++) DaoTuple_SetItem( res, values[M-N+i], i );
	}
	if( res == NULL ) res = DaoTuple_New( N );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoList* DaoProcess_NewList( DaoProcess *self )
{
	DaoList *res = DaoList_New();
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoMap* DaoProcess_NewMap( DaoProcess *self, unsigned int hashing )
{
	DaoMap *res = DaoMap_New( hashing );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
#ifdef DAO_WITH_NUMARRAY
DaoArray* DaoProcess_NewArray( DaoProcess *self, int type )
{
	DaoArray *res = DaoArray_New( type );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorSB( DaoProcess *self, signed char *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorSB( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorUB( DaoProcess *self, unsigned char *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorUB( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorSS( DaoProcess *self, signed short *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorSS( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorUS( DaoProcess *self, unsigned short *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorUS( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorSI( DaoProcess *self, signed int *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorSI( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorUI( DaoProcess *self, unsigned int *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorUI( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorI( DaoProcess *self, daoint *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorI( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorF( DaoProcess *self, float *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_FLOAT );
	if( s ) DaoArray_SetVectorF( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorD( DaoProcess *self, double *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_DOUBLE );
	if( s ) DaoArray_SetVectorD( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixSB( DaoProcess *self, signed char **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixSB( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixUB( DaoProcess *self, unsigned char **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixUB( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixSS( DaoProcess *self, signed short **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixSS( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixUS( DaoProcess *self, unsigned short **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixUS( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixSI( DaoProcess *self, signed int **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixSI( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixUI( DaoProcess *self, unsigned int **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixUI( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixI( DaoProcess *self, daoint **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixI( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixF( DaoProcess *self, float **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_FLOAT );
	if( s ) DaoArray_SetMatrixF( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixD( DaoProcess *self, double **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_DOUBLE );
	if( s ) DaoArray_SetMatrixD( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewBuffer( DaoProcess *self, void *p, daoint n )
{
	DaoArray *res = DaoArray_New(0);
	DaoArray_SetBuffer( res, p, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
#else
static DaoArray* DaoValue_NewArray()
{
	printf( "Error: numeric array is disabled!\n" );
	return NULL;
}
DaoArray* DaoProcess_NewVectorB( DaoProcess *self, char *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorUB( DaoProcess *self, unsigned char *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorS( DaoProcess *self, short *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorUS( DaoProcess *self, unsigned short *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorI( DaoProcess *self, daoint *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorUI( DaoProcess *self, unsigned int *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorF( DaoProcess *self, float *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorD( DaoProcess *self, double *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixB( DaoProcess *self, signed char **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixUB( DaoProcess *self, unsigned char **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixS( DaoProcess *self, short **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixUS( DaoProcess *self, unsigned short **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixI( DaoProcess *self, daoint **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixUI( DaoProcess *self, unsigned int **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixF( DaoProcess *self, float **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixD( DaoProcess *self, double **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewBuffer( DaoProcess *self, void *s, daoint n )
{
	return DaoValue_NewArray();
}
#endif
DaoStream* DaoProcess_NewStream( DaoProcess *self, FILE *f )
{
	DaoStream *res = DaoStream_New();
	DaoStream_SetFile( res, f );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoCdata* DaoProcess_NewCdata( DaoProcess *self, DaoType *type, void *data, int owned )
{
	DaoCdata *res = owned ? DaoCdata_New( type, data ) : DaoCdata_Wrap( type, data );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
