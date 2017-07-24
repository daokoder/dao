/*
// Dao Virtual Machine
// http://daoscript.org
//
// Copyright (c) 2006-2017, Limin Fu
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
#include"assert.h"

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




int DaoValue_IsZero( DaoValue *self )
{
	if( self == NULL ) return 1;
	switch( self->type ){
	case DAO_NONE : return 1;
	case DAO_BOOLEAN : return self->xBoolean.value == 0;
	case DAO_INTEGER : return self->xInteger.value == 0;
	case DAO_FLOAT  : return self->xFloat.value == 0.0;
	case DAO_COMPLEX : return self->xComplex.value.real == 0.0 && self->xComplex.value.imag == 0.0;
	case DAO_ENUM : return self->xEnum.value == 0;
	}
	return 0;
}
int DaoValue_IsNumber( DaoValue *self )
{
	if( self->type == DAO_INTEGER || self->type == DAO_FLOAT ) return 1;
	return 0;
}
static dao_integer DString_ToInteger( DString *self )
{
		return strtoll( self->chars, NULL, 0 );
}
dao_float DString_ToFloat( DString *self )
{
	return strtod( self->chars, 0 );
}
dao_integer DaoValue_GetInteger( DaoValue *self )
{
	switch( self->type ){
	case DAO_NONE    : return 0;
	case DAO_BOOLEAN : return self->xBoolean.value;
	case DAO_INTEGER : return self->xInteger.value;
	case DAO_FLOAT   : return self->xFloat.value;
	case DAO_COMPLEX : return self->xComplex.value.real;
	case DAO_STRING  : return DString_ToInteger( self->xString.value );
	case DAO_ENUM    : return self->xEnum.value;
	default : break;
	}
	return 0;
}
dao_float DaoValue_GetFloat( DaoValue *self )
{
	DString *str;
	switch( self->type ){
	case DAO_NONE    : return 0;
	case DAO_BOOLEAN : return self->xBoolean.value;
	case DAO_INTEGER : return self->xInteger.value;
	case DAO_FLOAT   : return self->xFloat.value;
	case DAO_COMPLEX : return self->xComplex.value.real;
	case DAO_STRING  : return DString_ToFloat( self->xString.value );
	case DAO_ENUM    : return self->xEnum.value;
	default : break;
	}
	return 0.0;
}
dao_complex DaoValue_GetComplex( DaoValue *self )
{
	dao_complex com = { 0.0, 0.0 };
	switch( self->type ){
	case DAO_BOOLEAN : com.real = self->xBoolean.value; break;
	case DAO_INTEGER : com.real = self->xInteger.value; break;
	case DAO_FLOAT   : com.real = self->xFloat.value; break;
	case DAO_COMPLEX : com = self->xComplex.value; break;
	default : break;
	}
	return com;
}
DString* DaoValue_GetString( DaoValue *self, DString *str )
{
	dao_complex *com;
	char chs[100] = {0};
	DString_Clear( str );
	switch( self->type ){
	case DAO_COMPLEX :
		com = & self->xComplex.value;
		sprintf( chs, (com->imag < 0) ? "%g%gC" : "%g+%gC", com->real, com->imag ); break;
	case DAO_BOOLEAN : strcat( chs, self->xBoolean.value ? "true" : "false" ); break;
	case DAO_INTEGER : sprintf( chs, "%"DAO_I64, (long long) self->xInteger.value ); break;
	case DAO_FLOAT   : sprintf( chs, "%g", self->xFloat.value ); break;
	case DAO_STRING : DString_Assign( str, self->xString.value ); break;
	case DAO_ENUM : DaoEnum_MakeName( & self->xEnum, str ); break;
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
	DaoGC_Assign( self, NULL );
}


DaoValue* DaoValue_CopyContainer( DaoValue *self, DaoType *tp )
{
	switch( self->type ){
	case DAO_LIST  : return (DaoValue*) DaoList_Copy( (DaoList*) self, tp );
	case DAO_MAP   : return (DaoValue*) DaoMap_Copy( (DaoMap*) self, tp );
	case DAO_TUPLE : return (DaoValue*) DaoTuple_Copy( (DaoTuple*) self, tp );
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY : return (DaoValue*) DaoArray_Copy( (DaoArray*) self, tp );
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
	if( self->type < DAO_ENUM && (tp == NULL || tp->tid == self->type) ){
		/*
		// The following optimization is safe theoretically.
		// But it is not practically safe for DaoProcess_PutChars() etc.,
		// which often uses shallow wraps of "const char*" as the source value,
		// and expects it to be copied at the destination as a primitive value.
		*/
		/* if( cst && cst->invar ) return self; */
		switch( self->type ){
		case DAO_NONE : return self;
		case DAO_BOOLEAN : return (DaoValue*) DaoBoolean_New( self->xBoolean.value );
		case DAO_INTEGER : return (DaoValue*) DaoInteger_New( self->xInteger.value );
		case DAO_FLOAT   : return (DaoValue*) DaoFloat_New( self->xFloat.value );
		case DAO_COMPLEX : return (DaoValue*) DaoComplex_New( self->xComplex.value );
		case DAO_STRING  : return (DaoValue*) DaoString_Copy( & self->xString );
		}
		return self; /* unreachable; */
	}else if( tp && tp->tid >= DAO_BOOLEAN && tp->tid <= DAO_FLOAT ){
		DaoValue *va = NULL;
		switch( tp->tid ){
		case DAO_BOOLEAN : va = (DaoValue*) DaoBoolean_New( DaoValue_GetInteger(self) ); break;
		case DAO_INTEGER : va = (DaoValue*) DaoInteger_New( DaoValue_GetInteger(self) ); break;
		case DAO_FLOAT   : va = (DaoValue*) DaoFloat_New( DaoValue_GetFloat(self) );   break;
		}
		return va;
	}else if( self->type == DAO_ENUM ){
		switch( tp ? tp->tid : 0 ){
		case DAO_ENUM :
			if( tp->subtid == DAO_ENUM_ANY ) tp = NULL;
			return (DaoValue*) DaoEnum_Copy( & self->xEnum, tp );
		case DAO_BOOLEAN : return (DaoValue*) DaoBoolean_New( self->xEnum.value );
		case DAO_INTEGER : return (DaoValue*) DaoInteger_New( self->xEnum.value );
		case DAO_FLOAT   : return (DaoValue*) DaoFloat_New( self->xEnum.value );
		}
		return (DaoValue*) DaoEnum_Copy( & self->xEnum, NULL );
	}else if( tp && tp->tid == DAO_ENUM ){
		switch( self->type ){
		case DAO_BOOLEAN :
		case DAO_INTEGER : return (DaoValue*) DaoEnum_New( tp, self->xInteger.value );
		case DAO_FLOAT   : return (DaoValue*) DaoEnum_New( tp, self->xFloat.value );
		}
	}else if( self->type == DAO_CINVALUE ){
		return (DaoValue*) DaoCinValue_Copy( (DaoCinValue*) self );
	}
	if( tp != NULL ){
		assert( tp->tid == 0 || tp->tid > DAO_ENUM );
		assert( self->type == 0 || self->type > DAO_ENUM );
	}

#ifdef DAO_WITH_NUMARRAY
	if( self->type == DAO_ARRAY && self->xArray.original ){
		DaoArray_Sliced( (DaoArray*)self );
		return self;
	}else
#endif
	if( self->type == DAO_CSTRUCT || self->type == DAO_CDATA ){
		if( self->xCstruct.ctype->core->Slice ) self->xCstruct.ctype->core->Slice( self );
		return self;
	}
	if( tp == NULL ){
		switch( self->type ){
		case DAO_LIST  : if( self->xList.ctype->empty ) tp = self->xList.ctype; break;
		case DAO_MAP   : if( self->xMap.ctype->empty  ) tp = self->xMap.ctype; break;
		default : break;
		}
	}
	if( self->xBase.trait & DAO_VALUE_NOCOPY ) return self;
	if( (self->xBase.trait & DAO_VALUE_CONST) == 0 ) return self;
	if( cst != NULL && cst->invar != 0 ) return self;
	if( tp ) tp = DaoType_GetBaseType( tp );
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
void DaoValue_MoveCinValue( DaoCinValue *S, DaoValue **D )
{
	DaoValue *D2 = *D;
	if( D2 == (DaoValue*) S ) return;
	if( D2 == NULL || D2->type != DAO_CINVALUE || D2->xCinValue.refCount > 1 ){
		S = DaoCinValue_Copy( S );
		DaoGC_Assign( D, (DaoValue*) S );
		return;
	}
	if( D2->xCinValue.cintype != S->cintype ){
		GC_Assign( & D2->xCinValue.cintype, S->cintype );
	}
	DaoValue_Copy( S->value, & D2->xCinValue.value );
}

void DaoValue_CopyX( DaoValue *src, DaoValue **dest, DaoType *cst )
{
	DaoValue *dest2 = *dest;
	if( src == dest2 ) return;
	if( dest2 && dest2->xBase.refCount >1 ){
		GC_DecRC( dest2 );
		*dest = dest2 = NULL;
	}
	if( src->type == DAO_CSTRUCT || src->type == DAO_CDATA ){
		DaoValue_MoveCstruct( src, dest, cst != NULL && cst->invar != 0 );
		return;
	}else if( src->type == DAO_CINVALUE ){
		DaoValue_MoveCinValue( (DaoCinValue*) src, dest );
		return;
	}
	if( dest2 == NULL ){
		src = DaoValue_SimpleCopyWithTypeX( src, NULL, cst );
		GC_IncRC( src );
		*dest = src;
		return;
	}
	if( src->type != dest2->type || src->type > DAO_ENUM ){
		src = DaoValue_SimpleCopyWithTypeX( src, NULL, cst );
		GC_Assign( dest, src );
		return;
	}
	switch( src->type ){
	case DAO_ENUM    :
		DaoEnum_SetType( & dest2->xEnum, src->xEnum.etype );
		DaoEnum_SetValue( & dest2->xEnum, & src->xEnum );
		break;
	case DAO_BOOLEAN : dest2->xBoolean.value = src->xBoolean.value; break;
	case DAO_INTEGER : dest2->xInteger.value = src->xInteger.value; break;
	case DAO_FLOAT   : dest2->xFloat.value = src->xFloat.value; break;
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
	DaoVmSpace *vms;
	DaoType *tp2;
	DNode *it;
	if( to->type != tp->tid && tp->tid != DAO_ANY ) return;
	if( tp->attrib & DAO_TYPE_SPEC ) return;
	switch( to->type ){
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		if( to->xArray.size ) return;
		if( tp->tid != DAO_ARRAY || tp->args == NULL || tp->args->size == 0 ) break;
		tp = tp->args->items.pType[0];
		if( tp->tid == DAO_NONE || tp->tid > DAO_COMPLEX ) break;
		DaoArray_SetNumType( (DaoArray*) to, tp->tid );
		break;
#endif
	case DAO_LIST :
		/* var x: any = {}, x->ctype should be list<any> */
		if( tp->tid == DAO_ANY ){
			vms = DaoType_GetVmSpace( tp );
			tp = vms->typeListAny;
		}
		if( to->xList.ctype && !(to->xList.ctype->attrib & DAO_TYPE_UNDEF) ) break;
		GC_Assign( & to->xList.ctype, tp );
		break;
	case DAO_MAP :
		if( tp->tid == DAO_ANY ){
			vms = DaoType_GetVmSpace( tp );
			tp = vms->typeMapAny;
		}
		if( to->xMap.ctype && !(to->xMap.ctype->attrib & DAO_TYPE_UNDEF) ) break;
		GC_Assign( & to->xMap.ctype, tp );
		break;
	case DAO_TUPLE :
		tp2 = to->xTuple.ctype;
		if( tp->tid == DAO_ANY ) break;
		if( tp->args->size ==0 ) break; /* not to the generic tuple type */
		if( tp2 == NULL || tp2->mapNames == NULL || tp2->mapNames->size ==0 ){
			GC_Assign( & to->xTuple.ctype, tp );
			break;
		}
		if( tp->mapNames == NULL || tp->mapNames->size ) break;
		for(it=DMap_First(tp2->mapNames); it!=NULL; it=DMap_Next(tp2->mapNames, it)){
			if( DMap_Find( tp->mapNames, it->key.pVoid ) == NULL ) break;
		}
		if( it ) break;
		GC_Assign( & to->xTuple.ctype, tp );
		break;
	default : break;
	}
}
static int DaoValue_TryCastTuple( DaoValue *src, DaoValue **dest, DaoType *tp )
{
	DaoTuple *tuple;
	DaoType **item_types = tp->args->items.pType;
	DaoType *totype = src->xTuple.ctype;
	DaoValue **data = src->xTuple.values;
	DMap *names = totype ? totype->mapNames : NULL;
	DNode *node, *search;
	daoint i, T = tp->args->size;
	int tm, eqs = 0;
	/*
	// Auto-cast tuple type, on the following conditions:
	// (1) the item values of "dest" must match exactly to the item types of "tp";
	// (2) "tp->mapNames" must contain "(*dest)->xTuple.ctype->mapNames";
	*/
	if( src->xTuple.ctype == NULL ){
		GC_IncRC( tp );
		src->xTuple.ctype = tp;
		return 1;
	}
	if( DaoType_MatchValue( tp, src, NULL ) < DAO_MT_SIM ) return 1; /* Redundant? */
	/*
	// Casting is not necessary if the tuple's field names are a superset of the
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
	GC_Assign( dest, tuple );
	return 1;
}
static int DaoValue_Move4( DaoValue *S, DaoValue **D, DaoType *T, DaoType *C, DMap *defs );
static int DaoValue_Move5( DaoValue *S, DaoValue **D, DaoType *T, DaoType *C, DMap *defs );
static int DaoValue_MoveVariant( DaoValue *src, DaoValue **dest, DaoType *tp, DaoType *C )
{
	DaoType *itp = NULL;
	int n = tp->args->size;
	int j, k, mt;
	for(j=0,mt=0; j<n; j++){
		DaoType *itp2 = tp->args->items.pType[j];
		if( tp->invar ) itp2 = DaoType_GetInvarType( itp2 );
		k = DaoType_MatchValue( itp2, src, NULL );
		if( k > mt ){
			itp = itp2;
			mt = k;
			if( mt >= DAO_MT_EQ ) break;
		}
	}
	if( itp == NULL ) return 0;
	return DaoValue_Move5( src, dest, itp, C, NULL );
}

void DaoValue_MoveCstruct( DaoValue *S, DaoValue **D, int nocopying )
{
	DaoTypeCore *core = S->xCstruct.ctype->core;
	DaoValue *E = *D;
	
	if( E == S ) return;

	if( core->Slice ) core->Slice( S );
	if( core->Copy == NULL || nocopying != 0 ){
		DaoGC_Assign( D, S );
	}else if( E && E->type == S->type && E->xCstruct.ctype == S->xCstruct.ctype
			&& E->xCstruct.refCount == 1 ){
		core->Copy( S, E );
		if( S->xBase.refCount == 0 ){
			DAO_DEBUG_WARN( "Unreferenced value is not assigned to the new location!" );
		}
	}else if( S->xBase.refCount == 0 ){
		DaoGC_Assign( D, S );
	}else{
		E = (DaoValue*) core->Copy( S, NULL );
		DaoGC_Assign( D, E );
	}
}

static int DaoType_IsNullable( DaoType *self )
{
	if( self->tid == DAO_NONE || self->tid == DAO_ANY ){
		return 1;
	}else if( self->tid == DAO_VARIANT ){
		if( DaoType_GetVariantItem( self, DAO_NONE ) ) return 1;
		return DaoType_GetVariantItem( self, DAO_ANY ) != NULL;
	}
	return 0;
}

int DaoValue_Move4( DaoValue *S, DaoValue **D, DaoType *T, DaoType *C, DMap *defs )
{
	DaoCinType *cintype;
	int tm = 1;
	switch( (T->tid << 8) | S->type ){
	case (DAO_BOOLEAN << 8) | DAO_BOOLEAN :
	case (DAO_BOOLEAN << 8) | DAO_INTEGER :
	case (DAO_BOOLEAN << 8) | DAO_FLOAT   :
	case (DAO_INTEGER << 8) | DAO_BOOLEAN :
	case (DAO_INTEGER << 8) | DAO_INTEGER :
	case (DAO_INTEGER << 8) | DAO_FLOAT   :
	case (DAO_FLOAT   << 8) | DAO_BOOLEAN :
	case (DAO_FLOAT   << 8) | DAO_INTEGER :
	case (DAO_FLOAT   << 8) | DAO_FLOAT   :
	case (DAO_COMPLEX << 8) | DAO_COMPLEX :
	case (DAO_STRING  << 8) | DAO_STRING  :
	case (DAO_CINVALUE<< 8) | DAO_CINVALUE  :
		S = DaoValue_SimpleCopyWithTypeX( S, T, C );
		GC_Assign( D, S );
		return 1;
	}
	switch( S->type ){
	case DAO_ENUM : if( S->xEnum.subtype == DAO_ENUM_SYM && T->realnum ) return 0; break;
	case DAO_OBJECT : if( S->xObject.isNull ) return 0; break;
	case DAO_CDATA  : if( S->xCdata.data == NULL && ! DaoType_IsNullable(T) ) return 0; break;
	}
	if( !(S->xBase.trait & DAO_VALUE_CONST) ){
		DaoVmSpace *vms;
		DaoType *ST = NULL;
		switch( (S->type << 8) | T->tid ){
		case (DAO_ARRAY<<8)|DAO_ARRAY :
			vms = DaoType_GetVmSpace( T );
			ST = vms->typeArrays[ S->xArray.etype ]; break;
		case (DAO_TUPLE<<8)|DAO_TUPLE : ST = S->xTuple.ctype; break;
		case (DAO_LIST <<8)|DAO_LIST  : ST = S->xList.ctype; break;
		case (DAO_MAP  <<8)|DAO_MAP   : ST = S->xMap.ctype; break;
		case (DAO_CDATA<<8)|DAO_CDATA : ST = S->xCdata.ctype; break;
		case (DAO_CSTRUCT<<8)|DAO_CSTRUCT : ST = S->xCstruct.ctype; break;
		case (DAO_OBJECT<<8)|DAO_OBJECT : ST = S->xObject.defClass->objType; break;
		}
		if( ST == T ){
			if( ST->tid == DAO_CSTRUCT || ST->tid == DAO_CDATA ){
				DaoValue_MoveCstruct( S, D, C != NULL && C->invar != 0 );
			}else{
				GC_Assign( D, S );
			}
			return 1;
		}
	}
	if( (T->tid == DAO_OBJECT || T->tid == DAO_CSTRUCT || T->tid == DAO_CDATA) && S->type == DAO_OBJECT ){
		if( S->xObject.defClass != & T->aux->xClass ){
			S = DaoObject_CastToBase( S->xObject.rootObject, T );
			if( S != NULL && S->type == DAO_CDATA && S->xCdata.data == NULL ){
				if( ! DaoType_IsNullable( T ) ) return 0;
			}
			tm = (S != NULL);
		}
	}else if( (T->tid == DAO_CLASS || T->tid == DAO_CTYPE) && S->type == DAO_CLASS ){
		if( S->xClass.clsType != T && T->aux != NULL ){ /* T->aux == NULL for "class"; */
			S = DaoClass_CastToBase( (DaoClass*)S, T );
			tm = (S != NULL);
		}
	}else if( T->tid == DAO_CTYPE && S->type == DAO_CTYPE ){
		if( S->xCtype.classType != T ){
			S = DaoType_CastToParent( S, T );
			tm = (S != NULL);
		}
	}else if( T->tid == DAO_ROUTINE && T->subtid != DAO_ROUTINES && S->type == DAO_ROUTINE && S->xRoutine.overloads ){
		DList *routines = S->xRoutine.overloads->routines;
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
	// Composite types must match exactly. Example,
	// where it will not work if composite types are allowed to match loosely.
	// d : list<list<int>> = {};
	// e : list<float> = { 1.0 };
	// d.append( e );
	//
	// But if d is of type list<list<any>>,
	// the matching do not necessary to be exact.
	*/
	cintype = NULL;
	if( T->tid == DAO_CINVALUE ){
		if( DaoType_MatchValue( T->aux->xCinType.target, S, NULL ) >= DAO_MT_CIV ){
			cintype = (DaoCinType*) T->aux;
		}
	}else if( T->tid == DAO_INTERFACE && T->aux->xInterface.concretes ){
		DaoInterface *inter = (DaoInterface*) T->aux;
		DaoType *st = DaoValue_GetType( S, inter->nameSpace->vmSpace );
		DNode *it; 
		cintype = DaoInterface_GetConcrete( inter, st );
		if( cintype == NULL ){
			for(it=DMap_First(inter->concretes); it; it=DMap_Next(inter->concretes,it)){
				if( DaoType_MatchValue( it->key.pType, S, NULL ) >= DAO_MT_CIV ){
					cintype = (DaoCinType*) it->value.pVoid;
					break;
				}
			}
		}
	}
	if( cintype ){
		S = (DaoValue*) DaoCinValue_New( cintype, S );
	}else if( S->type == DAO_CSTRUCT || S->type == DAO_CDATA ){
		DaoValue_MoveCstruct( S, D, C != NULL && C->invar != 0 );
		return 1;
	}else{
		S = DaoValue_SimpleCopyWithTypeX( S, T, C );
	}
	GC_Assign( D, S );
	if( S->type == DAO_TUPLE && S->xTuple.ctype != T && tm == DAO_MT_SIM ){
		return DaoValue_TryCastTuple( S, D, T );
	}else if( T && T->tid == S->type && !(T->attrib & DAO_TYPE_SPEC) ){
		DaoValue_SetType( S, T );
	}
	return 1;
}

int DaoValue_FastMatchTo( DaoValue *self, DaoType *type )
{
	int matched = 0;
	switch( self->type ){
	case DAO_LIST :
	case DAO_MAP :
	case DAO_CSTRUCT :
	case DAO_CDATA : matched = self->xCstruct.ctype == type; break;
	case DAO_CTYPE : matched = self->xCtype.classType == type; break;
	case DAO_TUPLE : matched = self->xTuple.ctype == type; break;
	case DAO_ROUTINE : matched = self->xRoutine.routType == type; break;
	case DAO_CLASS  : matched = self->xClass.clsType == type; break;
	case DAO_OBJECT : matched = self->xObject.defClass->objType == type; break;
	case DAO_CINVALUE : matched = self->xCinValue.cintype->vatype == type; break;
	default : break;
	}
	return matched;
}

int DaoValue_Move5( DaoValue *S, DaoValue **D, DaoType *T, DaoType *C, DMap *defs )
{
	DaoValue *D2 = *D;
	if( S == D2 && (S == NULL || DaoValue_FastMatchTo( S, T )) ) return 1;
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
		if( DaoValue_Compare( S, T->value ) !=0 ) return 0;
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
		if( DaoValue_FastMatchTo( S, T ) ){
			if( S->type == DAO_CDATA && S->xCdata.data == NULL ){
				if( ! DaoType_IsNullable( T ) ) return 0;
			}
			if( S->type == DAO_CSTRUCT || S->type == DAO_CDATA ){
				DaoValue_MoveCstruct( S, D, C != NULL && C->invar != 0 );
			}else if( S->type == DAO_CINVALUE ){
				DaoValue_MoveCinValue( (DaoCinValue*) S, D );
			}else{
				GC_Assign( D, S );
			}
			return 1;
		}
	}
	if( S->type == DAO_CINVALUE ){
		if( S->xCinValue.cintype->target == T ){
			S = S->xCinValue.value;
		}else if( DaoType_MatchTo( S->xCinValue.cintype->target, T, NULL ) >= DAO_MT_EQ ){
			S = S->xCinValue.value;
		}
	}
	if( D2 && D2->xBase.refCount > 1 ){
		GC_DecRC( D2 );
		*D = D2 = NULL;
	}
#if 0
	if( D2 && S->type == D2->type && S->type == T->tid && S->type <= DAO_ENUM ){
		switch( S->type ){
		case DAO_ENUM    :
			DaoEnum_SetType( & D2->xEnum, T->subtid == DAO_ENUM_ANY ? S->xEnum.etype : T );
			return DaoEnum_SetValue( & D2->xEnum, & S->xEnum );
		case DAO_BOOLEAN :
		case DAO_INTEGER : D2->xInteger.value = S->xInteger.value; break;
		case DAO_FLOAT   : D2->xFloat.value = S->xFloat.value; break;
		case DAO_COMPLEX : D2->xComplex.value = S->xComplex.value; break;
		case DAO_STRING  : DString_Assign( D2->xString.value, S->xString.value ); break;
		}
		return 1;
	}
#endif
	if( D2 == NULL || D2->type != T->tid ) return DaoValue_Move4( S, D, T, C, defs );

	switch( (S->type << 8) | T->tid ){
	case (DAO_STRING<<8)|DAO_STRING :
		DString_Assign( D2->xString.value, S->xString.value );
		break;
	case (DAO_ENUM<<8)|DAO_ENUM :
		DaoEnum_SetType( & D2->xEnum, T->subtid == DAO_ENUM_ANY ? S->xEnum.etype : T );
		DaoEnum_SetValue( & D2->xEnum, & S->xEnum );
		break;
	case (DAO_CINVALUE<<8)|DAO_CINVALUE :
		if( S->xCinValue.cintype->vatype != T ) return DaoValue_Move4( S, D, T, C, defs );
		DaoValue_MoveCinValue( (DaoCinValue*) S, D );
		break;
	case (DAO_BOOLEAN<<8)|DAO_BOOLEAN : D2->xBoolean.value = S->xBoolean.value; break;
	case (DAO_BOOLEAN<<8)|DAO_INTEGER : D2->xInteger.value = S->xBoolean.value; break;
	case (DAO_BOOLEAN<<8)|DAO_FLOAT   : D2->xFloat.value   = S->xBoolean.value; break;
	case (DAO_INTEGER<<8)|DAO_BOOLEAN : D2->xBoolean.value = S->xInteger.value; break;
	case (DAO_INTEGER<<8)|DAO_INTEGER : D2->xInteger.value = S->xInteger.value; break;
	case (DAO_INTEGER<<8)|DAO_FLOAT   : D2->xFloat.value   = S->xInteger.value; break;
	case (DAO_FLOAT  <<8)|DAO_BOOLEAN : D2->xBoolean.value = S->xFloat.value; break;
	case (DAO_FLOAT  <<8)|DAO_INTEGER : D2->xInteger.value = S->xFloat.value; break;
	case (DAO_FLOAT  <<8)|DAO_FLOAT   : D2->xFloat.value   = S->xFloat.value; break;
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
	return DaoValue_Move5( S, D, T, T, defs );
}

DaoValue* DaoValue_Convert( DaoValue *self, DaoType *type, int copy, DaoProcess *proc )
{
	DaoTypeCore *core = DaoValue_GetTypeCore( self );
	DaoValue *value = self;
	DaoType *at;

	if( type->tid & DAO_ANY ){
		if( copy == 0 ) return value;
		at = DaoValue_GetType( value, proc->vmSpace );
		at = DaoType_GetBaseType( at );
		if( at == NULL ) return NULL;
		if( DaoType_IsImmutable( at ) ) return value;
		if( value->type >= DAO_ARRAY && value->type <= DAO_TUPLE ){
			at = DaoNamespace_MakeInvarSliceType( proc->activeNamespace, at );
			return DaoValue_CopyContainer( value, at );
		}else if( core != NULL && core->Copy != NULL ){
			return core->Copy( value, NULL );
		}
		return NULL;
	}else if( type->tid == DAO_CINVALUE ){
		DaoCinType *cintype = (DaoCinType*) type->aux;

		if( value->type == DAO_CINVALUE && value->xCinValue.cintype == cintype ) return value;
		if( value->type == DAO_CINVALUE && DaoType_MatchValue( type, value, NULL ) ) return value;

		at = DaoNamespace_GetType( proc->activeNamespace, value );
		if( cintype->target == at || DaoType_MatchTo( cintype->target, at, NULL ) >= DAO_MT_CIV ){
			proc->cinvalue.cintype = cintype;
			proc->cinvalue.value = value;
			return (DaoValue*) & proc->cinvalue;
		}
		return NULL;
	}else if( type->tid == DAO_INTERFACE ){
		DaoInterface *inter = (DaoInterface*) type->aux;
		DaoRoutine *incompatible;

		if( type->aux == NULL ){ /* type "interface": */
			if( value->type != DAO_INTERFACE ) return NULL;
			return value;
		}
		if( value->type == DAO_CINVALUE && DaoType_MatchValue( type, value, NULL ) ) return value;

		at = DaoNamespace_GetType( proc->activeNamespace, value );
		if( inter->concretes ){
			DaoCinType *cintype = DaoInterface_GetConcrete( inter, at );
			if( cintype ){
				proc->cinvalue.cintype = cintype;
				proc->cinvalue.value = value;
				return (DaoValue*) & proc->cinvalue;
			}
		}
		switch( value->type ){
		case DAO_OBJECT  :
			value = (DaoValue*) value->xObject.rootObject;
			at = value->xObject.defClass->objType;
			break;
		case DAO_CSTRUCT :
		case DAO_CDATA :
			if( value->xCstruct.object ){
				value = (DaoValue*) value->xCstruct.object->rootObject;
				at = value->xObject.defClass->objType;
			}
			break;
		}
		/* Automatic binding when casted to an interface: */
		incompatible = DaoInterface_BindTo( inter, at, NULL );
		if( incompatible != NULL ){
			DString *buffer = DString_New();
			DString_AppendChars( buffer, "Interface method " );
			DString_Append( buffer, inter->abtype->name );
			DString_AppendChars( buffer, "::" );
			DString_Append( buffer, incompatible->routName );
			DString_AppendChars( buffer, "() is not available in the source type;" );
			DaoProcess_DeferException( proc, "Error::Type", buffer->chars );
			DString_Delete( buffer );
			return NULL;
		}
		return value;
	}else if( type->tid == DAO_VARIANT ){
		DaoType *best = NULL;
		int i, n, max = DAO_MT_NOT;
		for(i=0,n=type->args->size; i<n; i++){
			DaoType *itype = type->args->items.pType[i];
			int mt = DaoType_MatchValue( itype, self, NULL );
			if( mt > max ){
				best = itype;
				max = mt;
			}
		}
		if( best == NULL ) return NULL;
		return DaoValue_Convert( self, best, copy, proc );
	}

	if( core == NULL || core->DoConversion == NULL ) return NULL;
	value = core->DoConversion( self, type, copy, proc );

	if( value == NULL || value->type <= DAO_ENUM || copy == 0 ) return value;

	if( value == self /*|| DaoValue_ChildOf( value, self ) || DaoValue_ChildOf( self, value )*/ ){
		// No way to determine inheritance relationship between wrapped C++ objects;
		if( value->type >= DAO_ARRAY && value->type <= DAO_TUPLE ){
			DaoType *type = DaoValue_GetType( value, proc->vmSpace );
			if( type == NULL ) return NULL;
			type = DaoNamespace_MakeInvarSliceType( proc->activeNamespace, type );
			return DaoValue_CopyContainer( value, type );
		}
		if( core == NULL || core->Copy == NULL ) return NULL;

		value = core->Copy( value, NULL ); /* Copy invariable value; */
		if( value == NULL ) return NULL;

		DaoProcess_CacheValue( proc, value );
	}
	return value;
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

static int DaoTuple_Compare( DaoTuple *lt, DaoTuple *rt, DMap *cycmap )
{
	int i, lb, rb, res;
	if( lt->size < rt->size ) return -100;
	if( lt->size > rt->size ) return 100;

	for(i=0; i<lt->size; i++){
		res = DaoValue_CompareExt( lt->values[i], rt->values[i], cycmap );
		if( res != 0 ) return res;
	}
	return 0;
}

static int DaoList_Compare( DaoList *list1, DaoList *list2, DMap *cycmap )
{
	DaoValue **d1 = list1->value->items.pValue;
	DaoValue **d2 = list2->value->items.pValue;
	int size1 = list1->value->size;
	int size2 = list2->value->size;
	int min = size1 < size2 ? size1 : size2;
	int res = size1 == size2 ? 1 : 100;
	int i, cmp = 0;

	/* find the first unequal items */
	for(i=0; i<min; i++, d1++, d2++){
		cmp = DaoValue_CompareExt( *d1, *d2, cycmap );
		if( cmp != 0 ) break;
	}
	if( i < min ){
		if( abs( cmp ) > 1 ) return cmp;
		return cmp * res;
	}
	if( size1 == size2  ) return 0;
	return size1 < size2 ? -100 : 100;
}

static int DaoCstruct_Compare( DaoCstruct *left, DaoCstruct *right, DMap *cycmap )
{
	if( left == right ) return 0;

	if( DaoType_ChildOf( left->ctype, right->ctype ) ){
		if( left->ctype->core->Compare ){
			return left->ctype->core->Compare( (DaoValue*) left, (DaoValue*) right, cycmap );
		}else if( right->ctype->core->Compare ){
			return - right->ctype->core->Compare( (DaoValue*) right, (DaoValue*) left, cycmap );
		}
	}else if( DaoType_ChildOf( right->ctype, left->ctype ) ){
		if( right->ctype->core->Compare ){
			return - right->ctype->core->Compare( (DaoValue*) right, (DaoValue*) left, cycmap );
		}else if( left->ctype->core->Compare ){
			return left->ctype->core->Compare( (DaoValue*) left, (DaoValue*) right, cycmap );
		}
	}


	if( left->ctype != right->ctype ){
		return number_compare( (size_t)left->ctype, (size_t)right->ctype );
	}else if( left->type == DAO_CDATA ){
		DaoCdata *l = (DaoCdata*) left;
		DaoCdata *r = (DaoCdata*) right;
		return number_compare( (size_t)l->data, (size_t)r->data );
	}
	return number_compare( (size_t)left, (size_t)right );
}

static int DaoObject_Compare( DaoObject *left, DaoObject *right, DMap *cycmap )
{
	DaoValue *base1 = left->parent;
	DaoValue *base2 = right->parent;

	if( left == right ) return 0;

	while( base1 != NULL && base1->type == DAO_OBJECT ) base1 = base1->xObject.parent;
	while( base2 != NULL && base2->type == DAO_OBJECT ) base2 = base2->xObject.parent;

	if( DaoType_ChildOf( left->defClass->objType, right->defClass->objType ) ){
		if( base1 != NULL ){
			return DaoCstruct_Compare( (DaoCstruct*) base1, (DaoCstruct*) base2, cycmap );
		}
	}else if( DaoType_ChildOf( right->defClass->objType, left->defClass->objType ) ){
		if( base1 != NULL ){
			return DaoCstruct_Compare( (DaoCstruct*) base1, (DaoCstruct*) base2, cycmap );
		}
	}

	return number_compare( (size_t)left, (size_t)right );
}

static int DaoType_Compare( DaoType *left, DaoType *right )
{
	if( DaoType_MatchTo( left, right, NULL ) >= DAO_MT_EQ ) return 0;
	return number_compare( (size_t)left, (size_t)right );
}

int DaoValue_CompareExt( DaoValue *left, DaoValue *right, DMap *cycmap )
{
	DMap *input = cycmap;
	void *pters[2];
	int res = 0;

	if( left == right ) return 0;
	if( left == NULL || right == NULL ) return left < right ? -100 : 100;
	if( left->type != right->type ){
		double L, R;
		res = left->type < right->type ? -100 : 100;
		if( left->type < DAO_BOOLEAN || left->type > DAO_FLOAT ) return res;
		if( right->type < DAO_BOOLEAN || right->type > DAO_FLOAT ) return res;
		L = DaoValue_GetFloat( left );
		R = DaoValue_GetFloat( right );
		return L == R ? 0 : (L < R ? -1 : 1);
	}
	switch( left->type ){
	case DAO_TUPLE   :
	case DAO_LIST    :
	case DAO_OBJECT  :
	case DAO_CDATA   :
	case DAO_CSTRUCT :
		pters[0] = left;
		pters[1] = right;
		if( cycmap != NULL ){
			DNode *it = DMap_Find( cycmap, pters );
			if( it != NULL ) return left < right ? -100 : 100;
		}
		if( cycmap == NULL ) cycmap = DHash_New(DAO_DATA_VOID2,0);
		DMap_Insert( cycmap, pters, NULL );
		break;
	}
	switch( left->type ){
	case DAO_NONE : break;
	case DAO_BOOLEAN : res = number_compare( left->xBoolean.value, right->xBoolean.value ); break;
	case DAO_INTEGER : res = number_compare( left->xInteger.value, right->xInteger.value ); break;
	case DAO_FLOAT   : res = number_compare( left->xFloat.value, right->xFloat.value ); break;
	case DAO_COMPLEX : res = DaoComplex_Compare( (DaoComplex*) left, (DaoComplex*) right ); break;
	case DAO_STRING  : res = DString_CompareUTF8( left->xString.value, right->xString.value ); break;
	case DAO_ENUM    : res = DaoEnum_Compare( (DaoEnum*) left, (DaoEnum*) right ); break;
	case DAO_TUPLE   : res = DaoTuple_Compare( (DaoTuple*) left, (DaoTuple*) right, cycmap ); break;
	case DAO_LIST    : res = DaoList_Compare( (DaoList*) left, (DaoList*) right, cycmap ); break;
	case DAO_OBJECT  : res = DaoObject_Compare( (DaoObject*)left, (DaoObject*)right, cycmap ); break;
	case DAO_CDATA   :
	case DAO_CSTRUCT : res = DaoCstruct_Compare( (DaoCstruct*)left, (DaoCstruct*)right, cycmap ); break;
	case DAO_TYPE    : res = DaoType_Compare( (DaoType*) left, (DaoType*) right ); break;
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY   : res = DaoArray_Compare( (DaoArray*) left, (DaoArray*) right ); break;
#endif
	default: res = left < right ? -100 : 100; break; /* Needed for map; */
	}
	if( cycmap != input ) DMap_Delete( cycmap );
	return res;
}

int DaoValue_Compare( DaoValue *left, DaoValue *right )
{
	return DaoValue_CompareExt( left, right, NULL );
}


DaoType* DaoValue_GetType( DaoValue *self, DaoVmSpace *vms )
{
	if( self == NULL ) return NULL;

	switch( self->type ){
	case DAO_NONE    : return self->xBase.subtype == DAO_ANY ? vms->typeAny : vms->typeNone;
	case DAO_BOOLEAN : return vms->typeBool;
	case DAO_INTEGER : return vms->typeInt;
	case DAO_FLOAT   : return vms->typeFloat;
	case DAO_COMPLEX : return vms->typeComplex;
	case DAO_STRING  : return vms->typeString;
	case DAO_ENUM    : return self->xEnum.etype ? self->xEnum.etype : vms->typeEnum;
	case DAO_ARRAY   : return vms->typeArrays[ self->xArray.etype ];
	case DAO_LIST   : return self->xList.ctype;
	case DAO_MAP    : return self->xMap.ctype;
	case DAO_TUPLE  : return self->xTuple.ctype;
	case DAO_OBJECT : return self->xObject.defClass->objType;
	case DAO_CLASS  : return self->xClass.clsType;
	case DAO_CTYPE  : return self->xCtype.classType;
	case DAO_CDATA  :
	case DAO_CSTRUCT : return self->xCstruct.ctype;
	case DAO_ROUTINE   : return self->xRoutine.routType;
	case DAO_PAR_NAMED : return self->xNameValue.ctype;
	case DAO_INTERFACE : return self->xInterface.abtype;
	case DAO_CINTYPE   : return self->xCinType.citype;
	case DAO_CINVALUE  : return self->xCinValue.cintype->vatype;
	case DAO_NAMESPACE : return self->xNamespace.nstype;
	default : break;
	}
	return NULL;
}
DaoTypeCore* DaoValue_GetTypeCore( DaoValue *self )
{
	if( self == NULL ) return DaoType_GetCoreByID( DAO_NONE );
	switch( self->type ){
	case DAO_CSTRUCT :
	case DAO_CDATA   : return self->xCstruct.ctype->core;
	default : break;
	}
	return DaoType_GetCoreByID( self->type );
}


DaoType* DaoValue_CheckGetValueField( DaoType *self, DaoString *field, DaoRoutine *ctx )
{
	DaoValue *value = DaoType_FindValue( self, field->value );
	if( value ) return DaoNamespace_GetType( ctx->nameSpace, value );
	return NULL;
}

DaoValue* DaoValue_DoGetValueField( DaoValue *self, DaoString *field, DaoProcess *proc )
{
	DaoType *type = DaoNamespace_GetType( proc->activeNamespace, self );
	return DaoType_FindValue( type, field->value );
}

DaoType* DaoValue_CheckGetField( DaoType *self, DaoString *name )
{
	DaoRoutine *rout = DaoType_FindFunction( self, name->value );
	DaoType *argtype;
	DString *buffer;

	if( rout != NULL ) return rout->routType;

	buffer = DString_NewChars( "." );
	DString_Append( buffer, name->value );
	rout = DaoType_FindFunction( self, buffer );
	DString_Delete( buffer );
	if( rout != NULL ){
		rout = DaoRoutine_MatchByType( rout, self, NULL, 0, DVM_CALL );
	}else{
		rout = DaoType_FindFunctionChars( self, "." );
		if( rout == NULL ) return NULL;
		argtype = rout->nameSpace->vmSpace->typeString;
		rout = DaoRoutine_MatchByType( rout, self, & argtype, 1, DVM_CALL );
	}
	if( rout == NULL ) return NULL;
	return (DaoType*) rout->routType->aux;
}

DaoValue* DaoValue_DoGetField( DaoValue *self, DaoType *type, DaoString *name, DaoProcess *proc )
{
	DaoValue *value = DaoType_FindValue( type, name->value );
	DaoRoutine *rout;

	if( value != NULL ) return value;

	DString_SetChars( proc->string, "." );
	DString_Append( proc->string, name->value );
	rout = DaoType_FindFunction( type, proc->string );
	if( rout != NULL ){
		DaoProcess_PushCall( proc, rout, self, NULL, 0 );
	}else{
		DaoValue *arg = (DaoValue*) name;
		rout = DaoType_FindFunctionChars( type, "." );
		if( rout == NULL ) return NULL;
		DaoProcess_PushCall( proc, rout, self, & arg, 1 );
	}
	return NULL;
}

int DaoValue_CheckSetField( DaoType *self, DaoString *name, DaoType *value )
{
	DString *buffer = DString_NewChars( "." );
	DaoRoutine *rout;
	DaoType *args[2];

	DString_Append( buffer, name->value );
	DString_AppendChars( buffer, "=" );
	rout = DaoType_FindFunction( self, buffer );
	DString_Delete( buffer );

	if( rout != NULL ){
		rout = DaoRoutine_MatchByType( rout, self, & value, 1, DVM_CALL );
		if( rout == NULL ) return DAO_ERROR_VALUE;
	}else{
		rout = DaoType_FindFunctionChars( self, ".=" );
		if( rout == NULL ) return DAO_ERROR_FIELD_ABSENT;

		args[0] = rout->nameSpace->vmSpace->typeString;
		args[1] = value;
		rout = DaoRoutine_MatchByType( rout, self, args, 2, DVM_CALL );
		if( rout == NULL ) return DAO_ERROR_VALUE;
	}
	return DAO_OK;
}

int DaoValue_DoSetField( DaoValue *self, DaoType *type, DaoString *name, DaoValue *value, DaoProcess *proc )
{
    DaoRoutine *rout;

    DString_SetChars( proc->string, "." );
    DString_Append( proc->string, name->value );
    DString_AppendChars( proc->string, "=" );
    rout = DaoType_FindFunction( type, proc->string );
	if( rout != NULL ){
		return DaoProcess_PushCall( proc, rout, self, & value, 1 );
	}else{
		DaoValue *args[2];
		args[0] = (DaoValue*) name;
		args[1] = value;
		rout = DaoType_FindFunctionChars( type, ".=" );
		if( rout == NULL ) return DAO_ERROR_FIELD_ABSENT;
		return DaoProcess_PushCall( proc, rout, self, args, 2 );
	}
	return DAO_ERROR_FIELD_ABSENT;
}

void DaoValue_Print( DaoValue *self, DaoStream *stream, DMap *cycmap, DaoProcess *proc )
{
	DaoTypeCore *core = DaoValue_GetTypeCore( self );

	if( self == NULL ){
		DaoStream_WriteChars( stream, "none[0x0]" );
		return;
	}
	if( core == NULL || core->Print == NULL ){
		if( self->type == DAO_CSTRUCT || self->type == DAO_CDATA ){
			core = DaoCstruct_GetDefaultCore();
			core->Print( self, stream, cycmap, proc );
			return;
		}
	}
	if( core != NULL && core->Print != NULL && core->Print != DaoValue_Print ){
		core->Print( self, stream, cycmap, proc );
		return;
	}
	DaoStream_WriteChars( stream, core != NULL ? core->name : "Unknown" );
	DaoStream_WriteChars( stream, "[" );
	DaoStream_WritePointer( stream, self );
	DaoStream_WriteChars( stream, "]" );
}



int DaoValue_Type( DaoValue *self )
{
	return self->type;
}

DaoBoolean* DaoValue_CastBoolean( DaoValue *self )
{
	if( self == NULL || self->type != DAO_BOOLEAN ) return NULL;
	return (DaoBoolean*) self;
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
	DaoVmSpace *vms;
	if( self == NULL || self->type != DAO_CSTRUCT ) return NULL;
	vms = DaoType_GetVmSpace( self->xCstruct.ctype );
	return (DaoStream*) DaoValue_CastCstruct( self, vms->typeStream );
}
DaoObject* DaoValue_CastObject( DaoValue *self )
{
	if( self == NULL || self->type != DAO_OBJECT ) return NULL;
	return (DaoObject*) self;
}

DaoCstruct* DaoValue_CastCstruct( DaoValue *self, DaoType *type )
{
	if( self == NULL || type == NULL ) return (DaoCstruct*) self;
	if( self->type == DAO_OBJECT ){
		self = (DaoValue*) DaoObject_CastCstruct( (DaoObject*) self, type );
		if( self == NULL ) return NULL;
	}
	if( self->type != DAO_CSTRUCT && self->type != DAO_CDATA ) return NULL;
	if( DaoType_ChildOf( self->xCstruct.ctype, type ) ) return (DaoCstruct*) self;
	return NULL;
}

DaoCdata* DaoValue_CastCdata( DaoValue *self, DaoType *type )
{
	DaoCstruct *cstruct = DaoValue_CastCstruct( self, type );
	if( cstruct == NULL || cstruct->type != DAO_CDATA ) return NULL;
	return (DaoCdata*) cstruct;
}

DaoCstruct* DaoValue_CastCstructTC( DaoValue *self, DaoTypeCore *core )
{
	DaoType *type = NULL;
	if( self == NULL || core == NULL ) return (DaoCstruct*) self;
	if( self->type == DAO_OBJECT ){
		type = DaoVmSpace_GetType( self->xObject.defClass->nameSpace->vmSpace, core );
		self = (DaoValue*) DaoObject_CastCstruct( (DaoObject*) self, type );
		if( self == NULL ) return NULL;
	}
	if( self->type != DAO_CSTRUCT && self->type != DAO_CDATA ) return NULL;
	type = DaoVmSpace_GetType( self->xCstruct.ctype->aux->xCtype.nameSpace->vmSpace, core );
	if( DaoType_ChildOf( self->xCstruct.ctype, type ) ) return (DaoCstruct*) self;
	return NULL;
}

DaoCdata* DaoValue_CastCdataTC( DaoValue *self, DaoTypeCore *core )
{
	DaoCstruct *cstruct = DaoValue_CastCstructTC( self, core );
	if( cstruct == NULL || cstruct->type != DAO_CDATA ) return NULL;
	return (DaoCdata*) cstruct;
}

DaoCinValue* DaoValue_CastCinValue( DaoValue *self )
{
	if( self == NULL || self->type != DAO_CINVALUE ) return NULL;
	return (DaoCinValue*) self;
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
DaoCinType* DaoValue_CastCinType( DaoValue *self )
{
	if( self == NULL || self->type != DAO_CINTYPE ) return NULL;
	return (DaoCinType*) self;
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

dao_boolean DaoValue_TryGetBoolean( DaoValue *self )
{
	if( self->type != DAO_BOOLEAN ) return 0;
	return self->xBoolean.value;
}
dao_integer DaoValue_TryGetInteger( DaoValue *self )
{
	if( self->type != DAO_INTEGER ) return 0;
	return self->xInteger.value;
}
dao_float DaoValue_TryGetFloat( DaoValue *self )
{
	if( self->type != DAO_FLOAT ) return 0.0;
	return self->xFloat.value;
}
dao_complex DaoValue_TryGetComplex( DaoValue *self )
{
	dao_complex com = {0.0,0.0};
	if( self->type != DAO_COMPLEX ) return com;
	return self->xComplex.value;
}
int DaoValue_TryGetEnum(DaoValue *self)
{
	if( self->type != DAO_ENUM ) return 0;
	return self->xEnum.value;
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
	if( self->type == DAO_OBJECT ){
		self = (DaoValue*) DaoObject_CastCdata( (DaoObject*) self, type );
	}
	if( self == NULL || self->type != DAO_CDATA ) return NULL;
	if( self->type != DAO_CDATA ) return NULL;
	return DaoCdata_CastData( & self->xCdata, type );
}

void* DaoValue_TryCastCdataTC( DaoValue *self, DaoTypeCore *core )
{
	DaoType *type = NULL;
	if( self->type == DAO_OBJECT ){
		type = DaoVmSpace_GetType( self->xObject.defClass->nameSpace->vmSpace, core );
		self = (DaoValue*) DaoObject_CastCdata( (DaoObject*) self, type );
	}
	if( self == NULL || self->type != DAO_CDATA ) return NULL;
	if( self->type != DAO_CDATA ) return NULL;
	type = DaoVmSpace_GetType( self->xCstruct.ctype->aux->xCtype.nameSpace->vmSpace, core );
	return DaoCdata_CastData( & self->xCdata, type );
}

void* DaoValue_TryGetCdata( DaoValue *self )
{
	if( self->type != DAO_CDATA ) return NULL;
	return self->xCdata.data;
}

void DaoValue_ClearAll( DaoValue *v[], int n )
{
	int i;
	for(i=0; i<n; i++) DaoValue_Clear( v + i );
}

