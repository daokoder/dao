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

#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include<assert.h>

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
	GC_DecRC( self->cbtype );
	DString_Delete( self->name );
	if( self->fname ) DString_Delete( self->fname );
	if( self->nested ) DArray_Delete( self->nested );
	if( self->bases ) DArray_Delete( self->bases );
	if( self->mapNames ) DMap_Delete( self->mapNames );
	if( self->interfaces ) DMap_Delete( self->interfaces );
	dao_free( self );
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
	DaoValue_Print
};
DaoTypeBase abstypeTyper=
{
	"type", & typeCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoType_Delete, NULL
};

void DaoType_MapNames( DaoType *self );
void DaoType_CheckAttributes( DaoType *self )
{
	daoint i, count = 0;

	self->realnum = self->tid >= DAO_INTEGER && self->tid <= DAO_DOUBLE;
	self->attrib &= ~(DAO_TYPE_SPEC|DAO_TYPE_UNDEF);
	if( DString_FindChar( self->name, '@', 0 ) != MAXSIZE ) self->attrib |= DAO_TYPE_SPEC;
	if( DString_FindChar( self->name, '?', 0 ) != MAXSIZE ) self->attrib |= DAO_TYPE_UNDEF;

	/* For potential mixin class: */
	if( self->tid == DAO_CLASS || self->tid == DAO_OBJECT ) self->attrib |= DAO_TYPE_SPEC;

	if( (self->tid == DAO_PAR_NAMED || self->tid == DAO_PAR_DEFAULT) ){
		if( self->fname != NULL && strcmp( self->fname->mbs, "self" ) == 0 )
			self->attrib |= DAO_TYPE_SELFNAMED;
	}

	if( self->tid == DAO_TUPLE ){
		self->rntcount = 0;
		for(i=0; i<self->nested->size; i++){
			DaoType *it = self->nested->items.pType[i];
			if( it->tid == DAO_PAR_NAMED ) it = & it->aux->xType;
			self->rntcount += it->tid >= DAO_INTEGER && it->tid <= DAO_DOUBLE;
		}
	}
	if( self->aux && self->aux->type == DAO_TYPE ){
		if( self->aux->xType.attrib & DAO_TYPE_SPEC ) self->attrib |= DAO_TYPE_SPEC;
	}
	if( self->nested ){
		for(i=0; i<self->nested->size; i++){
			DaoType *it = self->nested->items.pType[i];
			if( it->tid == DAO_PAR_NAMED ) it = & it->aux->xType;
			if( it->attrib & DAO_TYPE_SPEC ) self->attrib |= DAO_TYPE_SPEC;
			count += it->tid >= DAO_INTEGER && it->tid <= DAO_STRING;
		}
		self->simtype = count == self->nested->size;
		if( (self->tid == DAO_TUPLE || self->tid == DAO_ROUTINE) && self->nested->size ){
			DaoType *it = self->nested->items.pType[self->nested->size - 1];
			if( it->tid == DAO_PAR_VALIST ) self->variadic = 1;
		}
	}
}
DaoType* DaoType_New( const char *name, int tid, DaoValue *extra, DArray *nest )
{
	DaoTypeBase *typer = DaoVmSpace_GetTyper( tid );
	DaoType *self = (DaoType*) dao_calloc( 1, sizeof(DaoType) );
	DaoValue_Init( self, DAO_TYPE );
	self->trait |= DAO_VALUE_DELAYGC;
	self->tid = tid;
	self->name = DString_New(1);
	self->typer = typer;
	if( typer->core ){
		self->kernel = typer->core->kernel;
		GC_IncRC( self->kernel );
	}
	if( extra == NULL && tid == DAO_PAR_VALIST ) extra = (DaoValue*) dao_type_any;
	if( extra ){
		self->aux = extra;
		GC_IncRC( extra );
	}
	DString_SetMBS( self->name, name );
	if( (tid == DAO_PAR_NAMED || tid == DAO_PAR_DEFAULT) && extra && extra->type == DAO_TYPE ){
		self->fname = DString_New(1);
		DString_SetMBS( self->fname, name );
		DString_AppendChar( self->name, (tid == DAO_PAR_NAMED) ? ':' : '=' );
		DString_Append( self->name, self->aux->xType.name );
	}
	if( nest ){
		self->nested = DArray_New(D_VALUE);
		DArray_Assign( self->nested, nest );
	}else if( tid == DAO_ROUTINE || tid == DAO_TUPLE ){
		self->nested = DArray_New(D_VALUE);
	}
	switch( tid ){
	case DAO_ENUM : self->mapNames = DMap_New(D_STRING,0); break;
	case DAO_TUPLE :
	case DAO_ROUTINE : DaoType_MapNames( self );
	}
	DaoType_CheckAttributes( self );
	DaoType_InitDefault( self );
	return self;
}
void DaoType_InitDefault( DaoType *self )
{
	complex16 com = {0.0,0.0};
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
			DaoType *it = types[i];
			if( it->tid == DAO_PAR_NAMED || it->tid == DAO_PAR_DEFAULT ){
				it = (DaoType*) it->aux;
			}else if( it->tid == DAO_PAR_VALIST ){
				DaoValue_Copy( dao_none_value, & value->xTuple.items[i] );
				continue;
			}
			DaoType_InitDefault( it );
			DaoValue_Copy( it->value, & value->xTuple.items[i] );
		}
		break;
	case DAO_VARIANT :
		for(i=0; i<count; i++) DaoType_InitDefault( types[i] );
		if( count ) value = types[0]->value;
		break;
	case DAO_UDT :
	case DAO_ANY :
	case DAO_THT :
	case DAO_ROUTINE :
	case DAO_INTERFACE : value = dao_none_value; break;
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
	if( value ) value->xBase.trait |= DAO_VALUE_CONST;
}
DaoType* DaoType_Copy( DaoType *other )
{
	DNode *it;
	DaoType *self = (DaoType*) dao_malloc( sizeof(DaoType) );
	memcpy( self, other, sizeof(DaoType) );
	DaoValue_Init( self, DAO_TYPE ); /* to reset gc fields */
	self->trait |= DAO_VALUE_DELAYGC;
	self->nested = NULL;
	self->bases = NULL;
	self->name = DString_Copy( other->name );
	if( other->fname ) self->fname = DString_Copy( other->fname );
	if( other->bases ) self->bases = DArray_Copy( other->bases );
	if( other->nested ) self->nested = DArray_Copy( other->nested );
	if( other->mapNames ) self->mapNames = DMap_Copy( other->mapNames );
	if( other->interfaces ) self->interfaces = DMap_Copy( other->interfaces );
	self->aux = other->aux;
	self->value = other->value;
	GC_IncRC( other->aux );
	GC_IncRC( other->value );
	GC_IncRC( other->kernel );
	GC_IncRC( other->cbtype );
	return self;
}
void DaoType_MapNames( DaoType *self )
{
	DaoType *tp;
	daoint i;
	if( self->nested == NULL ) return;
	if( self->tid != DAO_TUPLE && self->tid != DAO_ROUTINE ) return;
	if( self->mapNames == NULL ) self->mapNames = DMap_New(D_STRING,0);
	for(i=0; i<self->nested->size; i++){
		tp = self->nested->items.pType[i];
		if( tp->fname ) MAP_Insert( self->mapNames, tp->fname, i );
	}
}
DaoType* DaoType_GetItemType( DaoType *self, int i )
{
	if( self->nested == NULL ) return NULL;
	if( i < 0 || i >= self->nested->size ) return NULL;
	return self->nested->items.pType[i];
}
DaoType* DaoType_GetVariantItem( DaoType *self, int tid )
{
	if( self->tid == DAO_VARIANT && self->nested ){
		DaoType **types = self->nested->items.pType;
		daoint i, n = self->nested->size;
		for(i=0; i<n; i++) if( types[i]->tid == tid ) return types[i];
	}
	return NULL;
}

#define MIN(x,y) (x>y?y:x)


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
		dao_type_matrix[i][DAO_PAR_NAMED] = DAO_MT_EXACT+2;
		dao_type_matrix[i][DAO_PAR_DEFAULT] = DAO_MT_EXACT+2;
		dao_type_matrix[DAO_PAR_NAMED][i] = DAO_MT_EXACT+2;
		dao_type_matrix[DAO_PAR_DEFAULT][i] = DAO_MT_EXACT+2;

		dao_type_matrix[DAO_VALTYPE][i] = DAO_MT_EXACT+1;
		dao_type_matrix[i][DAO_VALTYPE] = DAO_MT_EXACT+1;
		dao_type_matrix[DAO_VARIANT][i] = DAO_MT_EXACT+1;
		dao_type_matrix[i][DAO_VARIANT] = DAO_MT_EXACT+1;
	}
	dao_type_matrix[DAO_VALTYPE][DAO_VALTYPE] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_VARIANT][DAO_VARIANT] = DAO_MT_EXACT+1;

	for(i=0; i<END_EXTRA_TYPES; i++){
		dao_type_matrix[DAO_UDT][i] = DAO_MT_UDF;
		dao_type_matrix[i][DAO_UDT] = DAO_MT_UDF;
		dao_type_matrix[i][DAO_ANY] = DAO_MT_ANY;
		dao_type_matrix[DAO_ANY][i] = DAO_MT_ANYX;
		dao_type_matrix[DAO_THT][i] = DAO_MT_INIT;
		dao_type_matrix[i][DAO_THT] = DAO_MT_INIT;
	}

	dao_type_matrix[DAO_UDT][DAO_ANY] = DAO_MT_ANYUDF;
	dao_type_matrix[DAO_ANY][DAO_UDT] = DAO_MT_ANYUDF;
	dao_type_matrix[DAO_THT][DAO_ANY] = DAO_MT_ANYUDF;
	dao_type_matrix[DAO_ANY][DAO_THT] = DAO_MT_ANYUDF;
	dao_type_matrix[DAO_UDT][DAO_THT] = DAO_MT_UDF;
	dao_type_matrix[DAO_THT][DAO_UDT] = DAO_MT_UDF;

	dao_type_matrix[DAO_ENUM][DAO_ENUM] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_TYPE][DAO_TYPE] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_ARRAY][DAO_ARRAY] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_LIST][DAO_LIST] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_MAP][DAO_MAP] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_TUPLE][DAO_TUPLE] = DAO_MT_EXACT+1;

	dao_type_matrix[DAO_CLASS][DAO_CLASS] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_CLASS][DAO_CTYPE] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_CLASS][DAO_INTERFACE] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_OBJECT][DAO_CDATA] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_OBJECT][DAO_CSTRUCT] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_OBJECT][DAO_OBJECT] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_OBJECT][DAO_INTERFACE] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_CTYPE][DAO_CTYPE] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_CTYPE][DAO_INTERFACE] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_CSTRUCT][DAO_CTYPE] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_CSTRUCT][DAO_CSTRUCT] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_CSTRUCT][DAO_INTERFACE] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_CDATA][DAO_CTYPE] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_CDATA][DAO_CDATA] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_CDATA][DAO_INTERFACE] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_ROUTINE][DAO_ROUTINE] = DAO_MT_EXACT+1;
	dao_type_matrix[DAO_PROCESS][DAO_ROUTINE] = DAO_MT_EXACT+1;
}
static int DaoType_Match( DaoType *self, DaoType *type, DMap *defs, DMap *binds );

static int DaoType_MatchPar( DaoType *self, DaoType *type, DMap *defs, DMap *binds, int host )
{
	DaoType *ext1 = self;
	DaoType *ext2 = type;
	int p1 = self->tid == DAO_PAR_NAMED || self->tid == DAO_PAR_DEFAULT;
	int p2 = type->tid == DAO_PAR_NAMED || type->tid == DAO_PAR_DEFAULT;
	int m = 0;
	if( p1 && p2 && ! DString_EQ( self->fname, type->fname ) ) return DAO_MT_NOT;
	if( p1 || self->tid == DAO_PAR_VALIST ) ext1 = & self->aux->xType;
	if( p2 || type->tid == DAO_PAR_VALIST ) ext2 = & type->aux->xType;

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
static int DaoType_MatchTemplateParams( DaoType *self, DaoType *type, DMap *defs )
{
	DaoTypeCore *core1 = self->typer->core;
	DaoTypeCore *core2 = type->typer->core;
	DaoType *template1 = core1 && core1->kernel ? core1->kernel->abtype : NULL;
	DaoType *template2 = core2 && core2->kernel ? core2->kernel->abtype : NULL;
	daoint i, k, n, mt = DAO_MT_NOT;
	if( template1 == template2 && template1 && template1->kernel->sptree ){
		DaoType **ts1 = self->nested->items.pType;
		DaoType **ts2 = type->nested->items.pType;
		if( self->nested->size != type->nested->size ) return 0;
		mt = DAO_MT_SUB;
		for(i=0,n=self->nested->size; i<n; i++){
			int tid = ts2[i]->tid ;
			k = DaoType_MatchTo( ts1[i], ts2[i], defs );
			/*
			// When matching template types, the template argument types
			// has to be equal, otherwise there will be a typing problem
			// when calling its method.
			// For example, if mt::channel<int> is allowed to match to
			// mt::channel<any>, the following call to channel::cap()
			// will result in an error:
			//
			//    chan : mt::channel<any> = mt::channel<int>(1)
			//    chan.cap(0)
			//
			// This is because in the second line, the type of "chan.cap"
			// will be inferenced to be a method type corresponding to
			// mt::channel<any>, and can accept a wider range of parameters
			// than that for mt::channel<int>. So at runtime, "chan.cap"
			// will get an method of mt::channel<int> that cannot be assigned
			// to a (temporary) variable with type for the method of
			// mt::channel<int>.
			*/
			if( k < DAO_MT_EQ && tid != DAO_THT && tid != DAO_UDT ) return DAO_MT_NOT;
#if 0
			if( k == 0 || k == DAO_MT_SUB || k == DAO_MT_SIM ) return DAO_MT_NOT;
			if( (ts1[i]->tid & DAO_ANY) && !(ts2[i]->tid & DAO_ANY) ) return DAO_MT_NOT;
			if( k < mt ) mt = k;
#endif
		}
	}
	return mt;
}
static int DaoType_MatchToParent( DaoType *self, DaoType *type, DMap *defs )
{
	daoint i, k, n, mt = DAO_MT_NOT;
	if( self == type ) return DAO_MT_EQ;
	if( self->tid == type->tid && (self->tid >= DAO_OBJECT && self->tid <= DAO_CTYPE) )
		if( self->aux == type->aux ) return DAO_MT_EQ; /* for aliased type; */
	if( (mt = DaoType_MatchTemplateParams( self, type, defs )) ) return mt;
	if( self->bases == NULL || self->bases->size == 0 ) return DAO_MT_NOT;
	for(i=0,n=self->bases->size; i<n; i++){
		k = DaoType_MatchToParent( self->bases->items.pType[i], type, defs );
		if( k > mt ) mt = k;
		if( k == DAO_MT_EQ ) return DAO_MT_SUB;
	}
	return mt;
}
static int DaoValue_MatchToParent( DaoValue *object, DaoType *parent, DMap *defs )
{
	int mt = DAO_MT_NOT;
	if( object == NULL || parent == NULL ) return DAO_MT_NOT;
	if( object->type == DAO_OBJECT ){
		mt = DaoType_MatchToParent( object->xObject.defClass->objType, parent, defs );
	}else if( object->type == DAO_CSTRUCT || object->type == DAO_CDATA || object->type == DAO_CTYPE ){
		mt = DaoType_MatchToParent( object->xCdata.ctype, parent, defs );
	}else if( object->type == DAO_CLASS ){
		mt = DaoType_MatchToParent( object->xClass.clsType, parent, defs);
	}
	return mt;
}
int DaoType_MatchToX( DaoType *self, DaoType *type, DMap *defs, DMap *binds )
{
	DaoType *it1, *it2;
	DNode *it, *node = NULL;
	daoint i, k, n, mt2, mt3, mt = DAO_MT_NOT;
	if( self == NULL || type == NULL ) return DAO_MT_NOT;
	if( self == type ) return DAO_MT_EQ;
	mt = dao_type_matrix[self->tid][type->tid];
	/*
	printf( "here: %i  %i  %i, %s  %s,  %p\n", mt, self->tid, type->tid,
			self->name->mbs, type->name->mbs, defs );
	 */
	if( mt == DAO_MT_INIT ){
		if( self && self->tid == DAO_THT ){
			if( defs ) node = MAP_Find( defs, self );
			if( node ) self = node->value.pType;
		}
		if( type && type->tid == DAO_THT ){
			if( defs ) node = MAP_Find( defs, type );
			if( node == NULL ){
				if( defs ) MAP_Insert( defs, type, self );
				if( self == NULL || self->tid==DAO_ANY || self->tid==DAO_UDT )
					return DAO_MT_ANYUDF;
				return DAO_MT_INIT; /* even if self==NULL, for typing checking for undefined @X */
			}
			type = node->value.pType;
			mt = DAO_MT_INIT;
			if( type == NULL || type->tid==DAO_ANY || type->tid==DAO_UDT )
				return DAO_MT_ANYUDF;
		}
	}else if( mt == DAO_MT_UDF ){
		if( self->tid == DAO_UDT ){
			if( type->tid==DAO_ANY || type->tid==DAO_UDT ) return DAO_MT_ANYUDF;
		}else{
			if( self->tid==DAO_ANY || self->tid==DAO_UDT ) return DAO_MT_ANYUDF;
		}
		return mt;
	}else if( mt == DAO_MT_ANYUDF ){
		if( self->tid == DAO_ANY && type->tid == DAO_THT ){
			if( defs ) MAP_Insert( defs, type, self );
		}
		return mt;
	}else if( type->tid == DAO_INTERFACE ){
		return DaoType_MatchInterface( self, (DaoInterface*) type->aux, binds );
	}
	mt = dao_type_matrix[self->tid][type->tid];
	if( mt <= DAO_MT_EXACT ) return mt;
	if( mt == DAO_MT_EXACT+2 ) return DaoType_MatchPar( self, type, defs, binds, 0 );

	if( self->tid == DAO_VARIANT && type->tid == DAO_VARIANT ){
		mt = DAO_MT_EQ;
		for(i=0,n=self->nested->size; i<n; i++){
			it2 = self->nested->items.pType[i];
			mt2 = DaoType_MatchTo( it2, type, defs );
			if( mt2 < mt ) mt = mt2;
			if( mt == DAO_MT_NOT ) break;
		}
		if( mt && defs && type->aux && type->aux->type == DAO_TYPE )
			MAP_Insert( defs, type->aux, self->aux );
		return mt;
	}else if( type->tid == DAO_VARIANT ){
		mt = DAO_MT_NOT;
		for(i=0,n=type->nested->size; i<n; i++){
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
	mt = DAO_MT_EQ;
	switch( self->tid ){
	case DAO_ENUM :
		if( self == type ) return DAO_MT_EQ;
		if( DString_EQ( self->name, type->name ) ) return DAO_MT_SIM;
		if( self->flagtype && type->flagtype ==0 ) return 0;
		for(it=DMap_First(self->mapNames); it; it=DMap_Next(self->mapNames, it )){
			node = DMap_Find( type->mapNames, it->key.pVoid );
			if( node ==NULL ) return 0;
			/* if( node->value.pInt != it->value.pInt ) return 0; */
		}
		return DAO_MT_EQ;
	case DAO_ARRAY : case DAO_LIST : case DAO_MAP :
	case DAO_TYPE :
		if( self->nested->size != type->nested->size ) return DAO_MT_NOT;
		for(i=0,n=self->nested->size; i<n; i++){
			it1 = self->nested->items.pType[i];
			it2 = type->nested->items.pType[i];
			k = DaoType_MatchPar( it1, it2, defs, binds, type->tid );
			/* printf( "%i %s %s\n", k, it1->name->mbs, it2->name->mbs ); */
			/* Do not match between array<int>, array<float>, array<double>: */
			if( self->tid == DAO_ARRAY && k == DAO_MT_SIM ) return DAO_MT_NOT;
			if( k == DAO_MT_NOT ) return k;
			if( k < mt ) mt = k;
		}
		break;
	case DAO_TUPLE :
		/* Source tuple type must contain at least as many item as the target tuple: */
		if( self->nested->size < type->nested->size ) return DAO_MT_NOT;
		if( self->nested->size > type->nested->size && type->variadic == 0 ) return DAO_MT_NOT;
		/* Compare non-variadic part of the tuple: */
		for(i=0,n=type->nested->size-(type->variadic!=0); i<n; i++){
			it1 = self->nested->items.pType[i];
			it2 = type->nested->items.pType[i];
			k = DaoType_MatchPar( it1, it2, defs, binds, type->tid );
			/* printf( "%i %s %s\n", k, it1->name->mbs, it2->name->mbs ); */
			if( k == DAO_MT_NOT ) return k;
			if( k < mt ) mt = k;
		}
		/* Compare variadic part of the tuple: */
		it2 = type->nested->items.pType[type->nested->size-1];
		for(i=type->nested->size-(type->variadic!=0),n=self->nested->size-(self->variadic!=0); i<n; ++i){
			it1 = self->nested->items.pType[i];
			k = DaoType_MatchPar( it1, it2, defs, binds, type->tid );
			/* printf( "%i %s %s\n", k, it1->name->mbs, it2->name->mbs ); */
			if( k == DAO_MT_NOT ) return k;
			if( k < mt ) mt = k;
		}
		break;
	case DAO_ROUTINE :
		if( self->overloads ){
			if( type->overloads ){
				return DAO_MT_EQ * (self == type);
			}else{
				DaoRoutine *rout;
				DaoType **tps = type->nested->items.pType;
				DArray *routines = self->aux->xRoutine.overloads->routines;
				int np = type->nested->size;
				for(i=0,n=routines->size; i<n; i++){
					if( routines->items.pRoutine[i]->routType == type ) return DAO_MT_EQ;
				}
				rout = DaoRoutine_ResolveByType( (DaoRoutine*)self->aux, NULL, tps, np, DVM_CALL );
				if( rout == NULL ) return DAO_MT_NOT;
				return DaoType_MatchTo( rout->routType, type, defs );
			}
		}
		if( type->overloads ) return 0;
		if( self->name->mbs[0] != type->name->mbs[0] ) return 0; /* @routine */
		if( type->aux == NULL ) return DAO_MT_SIM; /* match to "routine"; */
		if( self->nested->size < type->nested->size ) return DAO_MT_NOT;
		if( (self->cbtype == NULL) != (type->cbtype == NULL) ) return 0;
		if( self->aux == NULL && type->aux ) return 0;
		if( self->cbtype && DaoType_MatchTo( self->cbtype, type->cbtype, defs ) ==0 ) return 0;
		/* self may have extra parameters, but they must have default values: */
		for(i=type->nested->size,n=self->nested->size; i<n; i++){
			it1 = self->nested->items.pType[i];
			if( it1->tid != DAO_PAR_DEFAULT ) return 0;
		}
		for(i=0,n=type->nested->size; i<n; i++){
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
		return DaoType_MatchToParent( self, type, defs );
	case DAO_CTYPE :
	case DAO_CDATA :
	case DAO_CSTRUCT :
		if( self->aux == type->aux ) return DAO_MT_EQ; /* for aliased type; */
		return DaoType_MatchToParent( self, type, defs );
	case DAO_VALTYPE :
		if( type->tid != DAO_VALTYPE ) return DaoType_MatchValue( type, self->aux, defs );
		if( DaoValue_Compare( self->aux, type->aux ) ==0 ) return DAO_MT_EXACT;
		return DAO_MT_NOT;
	case DAO_VARIANT :
		mt = DAO_MT_EQ;
		mt3 = DAO_MT_NOT;
		for(i=0,n=self->nested->size; i<n; i++){
			it1 = self->nested->items.pType[i];
			mt2 = DaoType_MatchTo( it1, type, defs );
			if( mt2 < mt ) mt = mt2;
			if( mt2 > mt3 ) mt3 = mt2;
		}
		if( mt == 0 ) return mt3 ? DAO_MT_ANYX : DAO_MT_NOT;
		return mt;
	default : break;
	}
	if( mt > DAO_MT_EXACT ) mt = DAO_MT_NOT;
	return mt;
}
int DaoType_Match( DaoType *self, DaoType *type, DMap *defs, DMap *binds )
{
	void *pvoid[2];
	int mt;

	pvoid[0] = self;
	pvoid[1] = type;

	if( self ==NULL || type ==NULL ) return DAO_MT_NOT;
	if( self == type ) return DAO_MT_EQ;

	mt = DaoType_MatchToX( self, type, defs, binds );
#if 0
	printf( "mt = %i %s %s\n", mt, self->name->mbs, type->name->mbs );
	if( mt ==0 && binds ){
		printf( "%p  %p\n", pvoid[0], pvoid[1] );
		printf( "%i %p\n", binds->size, DMap_Find( binds, pvoid ) );
	}
#endif
	if( mt ==0 && binds && DMap_Find( binds, pvoid ) ) return DAO_MT_INIT;
	return mt;
}
int DaoType_MatchTo( DaoType *self, DaoType *type, DMap *defs )
{
	return DaoType_Match( self, type, defs, NULL );
}
int DaoType_MatchValue( DaoType *self, DaoValue *value, DMap *defs )
{
	DaoInterface *dinterface;
	DaoType *tp;
	DaoEnum *other;
	DNode *node;
	DMap *names;
	daoint i, n, mt, mt2, it1 = 0, it2 = 0;

	if( (self == NULL) | (value == NULL) ) return DAO_MT_NOT;
	switch( (self->tid << 8) | value->type ){
	case (DAO_INTEGER << 8) | DAO_INTEGER : return DAO_MT_EQ;
	case (DAO_FLOAT   << 8) | DAO_FLOAT   : return DAO_MT_EQ;
	case (DAO_DOUBLE  << 8) | DAO_DOUBLE  : return DAO_MT_EQ;
	case (DAO_COMPLEX << 8) | DAO_COMPLEX : return DAO_MT_EQ;
	case (DAO_LONG    << 8) | DAO_LONG    : return DAO_MT_EQ;
	case (DAO_STRING  << 8) | DAO_STRING  : return DAO_MT_EQ;
	case (DAO_INTEGER << 8) | DAO_FLOAT   : return DAO_MT_SIM;
	case (DAO_INTEGER << 8) | DAO_DOUBLE  : return DAO_MT_SIM;
	case (DAO_FLOAT   << 8) | DAO_INTEGER : return DAO_MT_SIM;
	case (DAO_FLOAT   << 8) | DAO_DOUBLE  : return DAO_MT_SIM;
	case (DAO_DOUBLE  << 8) | DAO_INTEGER : return DAO_MT_SIM;
	case (DAO_DOUBLE  << 8) | DAO_FLOAT   : return DAO_MT_SIM;
	}
	switch( self->tid ){
	case DAO_UDT :
	case DAO_THT :
		if( defs ){
			node = MAP_Find( defs, self );
			if( node ) return DaoType_MatchValue( node->value.pType, value, defs );
		}
		return self->tid == DAO_UDT ? DAO_MT_UDF : DAO_MT_INIT;
	case DAO_VARIANT :
		mt = DAO_MT_NOT;
		for(i=0,n=self->nested->size; i<n; i++){
			tp = self->nested->items.pType[i];
			mt2 = DaoType_MatchValue( tp, value, defs );
			if( mt2 > mt ) mt = mt2;
			if( mt == DAO_MT_EQ ) break;
		}
		return mt;
	case DAO_VALTYPE :
		if( DaoValue_Compare( self->aux, value ) ==0 ) return DAO_MT_EXACT;
		return DAO_MT_NOT;
	case DAO_ANY : return DAO_MT_ANY;
	}
	dinterface = self->tid == DAO_INTERFACE ? (DaoInterface*) self->aux : NULL;
	switch( value->type ){
	case DAO_LONG :
		if( dinterface ) return DaoType_MatchInterface( dao_type_long, dinterface, NULL );
		break;
	case DAO_STRING :
		if( dinterface ) return DaoType_MatchInterface( dao_type_string, dinterface, NULL );
		break;
	case DAO_ENUM :
		if( self == value->xEnum.etype ) return DAO_MT_EQ;
		if( dinterface ) return DaoType_MatchInterface( value->xEnum.etype, dinterface, NULL );
		if( self->tid != value->type ) return DAO_MT_NOT;
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
		return DAO_MT_SIM;
		break;
	case DAO_ARRAY :
		if( value->xArray.size == 0 ) return DAO_MT_ANY;
		tp = dao_array_types[ value->xArray.etype ];
		if( tp == self ) return DAO_MT_EQ;
		// XXX
		if( self->tid != value->type ) return DAO_MT_NOT;
		if( self->nested && self->nested->size ) it1 = self->nested->items.pType[0]->tid;
		if( it1 == DAO_UDT ) return DAO_MT_UDF;
		if( it1 == DAO_ANY ) return DAO_MT_ANY;
		if( it1 == DAO_THT ) return DAO_MT_INIT;
		if( value->xArray.etype == it1 ) return DAO_MT_EQ;
		/* return DAO_MT_EQ for exact match, or zero otherwise: */
		if( tp ) return (DaoType_MatchTo( tp, self, defs ) == DAO_MT_EQ) * DAO_MT_EQ;
		break;
	case DAO_LIST :
		tp = value->xList.unitype;
		if( tp == self ) return DAO_MT_EQ;
		if( dinterface ) return DaoType_MatchInterface( tp, dinterface, NULL );
		if( self->tid != value->type ) return DAO_MT_NOT;
		if( value->xList.items.size == 0 ) return DAO_MT_ANY;
		if( self->nested && self->nested->size ) it1 = self->nested->items.pType[0]->tid;
		if( it1 == DAO_UDT ) return DAO_MT_UDF;
		if( it1 == DAO_ANY ) return DAO_MT_ANY;
		if( it1 == DAO_THT ) return DAO_MT_INIT;
		if( tp ) return (DaoType_MatchTo( tp, self, defs ) == DAO_MT_EQ) * DAO_MT_EQ;
		break;
	case DAO_MAP :
		tp = value->xMap.unitype;
		if( tp == self ) return DAO_MT_EQ;
		if( dinterface ) return DaoType_MatchInterface( tp, dinterface, NULL );
		if( self->tid != value->type ) return DAO_MT_NOT;
		if( value->xMap.items->size == 0 ) return DAO_MT_ANY;
		if( self->nested && self->nested->size ) it1 = self->nested->items.pType[0]->tid;
		if( self->nested && self->nested->size >1 ) it2 = self->nested->items.pType[1]->tid;
		if( (it1|it2) & DAO_ANY ){ /* check for bit (1<<6) */
			if( it1 == DAO_UDT || it2 == DAO_UDT ) return DAO_MT_UDF;
			if( it1 == DAO_THT || it2 == DAO_THT ) return DAO_MT_INIT;
			if( it1 == DAO_ANY || it2 == DAO_ANY ) return DAO_MT_ANY;
		}
		if( tp ) return (DaoType_MatchTo( tp, self, defs ) == DAO_MT_EQ) * DAO_MT_EQ;
		break;
	case DAO_TUPLE :
		tp = value->xTuple.unitype;
		if( tp == self ) return DAO_MT_EQ;
		if( dinterface ) return DaoType_MatchInterface( tp, dinterface, NULL );
		if( self->tid != value->type ) return DAO_MT_NOT;
		if( value->xTuple.size < self->nested->size ) return DAO_MT_NOT;
		if( value->xTuple.size > self->nested->size && self->variadic ==0 ) return DAO_MT_NOT;

		for(i=0,n=self->nested->size-(self->variadic!=0); i<n; i++){
			tp = self->nested->items.pType[i];
			if( tp->tid == DAO_PAR_NAMED ) tp = & tp->aux->xType;

			/* for C functions that returns a tuple:
			 * the tuple may be assigned to a context value before
			 * its values are set properly! */
			if( value->xTuple.items[i] == NULL ) continue;
			if( tp->tid == DAO_UDT || tp->tid == DAO_ANY || tp->tid == DAO_THT ) continue;

			mt = DaoType_MatchValue( tp, value->xTuple.items[i], defs );
			if( mt < DAO_MT_SIM ) return 0;
		}
		tp = self->nested->items.pType[self->nested->size-1];
		if( tp->tid == DAO_PAR_VALIST ) tp = (DaoType*) tp->aux;
		for(i=self->nested->size-(self->variadic!=0),n=value->xTuple.size; i<n; ++i){
			if( value->xTuple.items[i] == NULL ) continue;
			if( tp->tid == DAO_UDT || tp->tid == DAO_ANY || tp->tid == DAO_THT ) continue;

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
	case DAO_ROUTINE :
		if( self->tid != value->type ) return DAO_MT_NOT;
		if( value->xRoutine.overloads ){
			if( self->overloads ){
				return DAO_MT_EQ * (self == value->xRoutine.routType);
			}else{
				DaoRoutine *rout;
				DaoType **tps = self->nested->items.pType;
				DArray *routines = value->xRoutine.overloads->routines;
				int np = self->nested->size;
				for(i=0,n=routines->size; i<n; i++){
					if( routines->items.pRoutine[i]->routType == self ) return DAO_MT_EQ;
				}
				rout = DaoRoutine_ResolveByType( (DaoRoutine*)value, NULL, tps, np, DVM_CALL );
				if( rout == NULL ) return DAO_MT_NOT;
				return DaoType_MatchTo( rout->routType, self, defs );
			}
		}
		tp = value->xRoutine.routType;
		if( tp == self ) return DAO_MT_EQ;
		if( tp ) return DaoType_MatchTo( tp, self, NULL );
		break;
	case DAO_CLASS :
		if( self->aux == NULL ) return DAO_MT_SUB; /* par : class */
		if( self->aux == value ) return DAO_MT_EQ;
		return DaoValue_MatchToParent( value, self, defs );
	case DAO_OBJECT :
		if( self->aux == (DaoValue*) value->xObject.defClass ) return DAO_MT_EQ;
		if( dinterface ) return DaoType_MatchInterface( value->xObject.defClass->objType, dinterface, NULL );
		return DaoValue_MatchToParent( value, self, defs );
	case DAO_CTYPE :
	case DAO_CDATA :
	case DAO_CSTRUCT :
		tp = value->xCdata.ctype;
		if( self == tp ) return DAO_MT_EQ;
		if( self->tid == value->type && self->aux == tp->aux ) return DAO_MT_EQ; /* alias type */
		if( dinterface && value->type != DAO_CTYPE ) return DaoType_MatchInterface( tp, dinterface, NULL );
		return DaoValue_MatchToParent( value, self, defs );
	case DAO_TYPE :
		tp = & value->xType;
		if( self->tid != DAO_TYPE ) return 0;
		/* if( tp == self ) return DAO_MT_EQ; */
		return DaoType_MatchTo( tp, self->nested->items.pType[0], defs );
	case DAO_PAR_NAMED :
	case DAO_PAR_DEFAULT :
		if( value->xNameValue.unitype == self ) return DAO_MT_EQ;
		return DaoType_MatchTo( value->xNameValue.unitype, self, defs );
	}
	return (self->tid == value->type) ? DAO_MT_EQ : DAO_MT_NOT;
}
int DaoType_MatchValue2( DaoType *self, DaoValue *value, DMap *defs )
{
	int m = DaoType_MatchValue( self, value, defs );
	if( m == 0 || value->type <= DAO_TUPLE || value->type != self->tid ) return m;
	if( value->type == DAO_CDATA ){
		if( value->xCdata.data == NULL ) m = 0;
	}else{
		if( value == self->value ) m = 0;
	}
	return m;
}
int DaoType_ChildOf( DaoType *self, DaoType *other )
{
	if( self == NULL || other == NULL ) return 0;
	if( self == other ) return 1;
	return DaoType_MatchToParent( self, other, NULL );
}
DaoValue* DaoType_CastToParent( DaoValue *object, DaoType *parent )
{
	daoint i, n;
	DaoValue *value;
	if( object == NULL || parent == NULL ) return NULL;
	if( object->type == DAO_CSTRUCT || object->type == DAO_CDATA || object->type == DAO_CTYPE ){
		if( DaoType_MatchToParent( object->xCdata.ctype, parent, NULL ) ) return object;
	}else if( object->type == DAO_OBJECT ){
		if( object->xObject.defClass->objType == parent ) return object;
		for(i=0, n=object->xObject.baseCount; i<n; i++){
			value = DaoType_CastToParent( object->xObject.parents[i], parent );
			if( value ) return value;
		}
	}else if( object->type == DAO_CLASS ){
		if( object->xClass.clsType == parent ) return object;
		for(i=0, n=object->xClass.superClass->size; i<n; i++){
			value = DaoType_CastToParent( object->xClass.superClass->items.pValue[i], parent );
			if( value ) return value;
		}
	}
	return NULL;
}
DaoValue* DaoType_CastToDerived( DaoValue *object, DaoType *derived )
{
	/* TODO: */
	return NULL;
}
static void DMap_Erase2( DMap *defs, void *p )
{
	DArray *keys = DArray_New(0);
	DNode *node;
	daoint i, n;
	DMap_Erase( defs, p );
	for(node=DMap_First(defs); node; node=DMap_Next(defs,node)){
		if( node->value.pVoid == p ) DArray_Append( keys, node->key.pVoid );
	}
	for(i=0,n=keys->size; i<n; i++) DMap_Erase( defs, keys->items.pVoid[i] );
	DArray_Delete( keys );
}
DaoType* DaoType_DefineTypes( DaoType *self, DaoNamespace *ns, DMap *defs )
{
	DaoType *type, *copy = NULL;
	DNode *node;
	daoint i, n;

	if( self == NULL ) return NULL;
	if( !(self->attrib & DAO_TYPE_SPEC) ) return self;

	node = MAP_Find( defs, self );
	if( node ){
		if( node->value.pType == self ) return self;
		return DaoType_DefineTypes( node->value.pType, ns, defs );

	}else if( self->tid & DAO_ANY ){
		return self;
	}else if( self->tid == DAO_VARIANT && self->aux ){ /* @T<int|float> */
		node = MAP_Find( defs, self->aux );
		if( node == NULL || node->value.pType == self ) return self;
		type = DaoType_DefineTypes( node->value.pType, ns, defs );
		if( type == NULL ) return self;
		return type;
	}else if( self->tid == DAO_ANY ){
		return self;
	}else if( self->tid == DAO_CLASS ){ /* e.g., class<Item<@T>> */
		copy = DaoType_DefineTypes( self->aux->xClass.objType, ns, defs );
		if( copy->aux != self->aux ) self = copy->aux->xClass.clsType;
		return self;
	}else if( self->tid == DAO_OBJECT ){
		return self;
	}

	copy = DaoType_New( "", self->tid, NULL, NULL );
	GC_ShiftRC( self->kernel, copy->kernel );
	copy->kernel = self->kernel;
	copy->typer = self->typer;
	copy->attrib = self->attrib;
	copy->overloads = self->overloads;
	copy->trait |= DAO_VALUE_DELAYGC;
	DString_Reserve( copy->name, 128 );
	DArray_Append( ns->auxData, copy );
	DMap_Insert( defs, self, copy );
	if( self->mapNames ){
		if( copy->mapNames ) DMap_Delete( copy->mapNames );
		copy->mapNames = DMap_Copy( self->mapNames );
	}
	if( self->fname ) copy->fname = DString_Copy( self->fname );
	if( self->nested ){
		int m = DString_MatchMBS( self->name, "^ %@? %w+ %< ", NULL, NULL );
		char sep = self->tid == DAO_VARIANT ? '|' : ',';
		if( copy->nested == NULL ) copy->nested = DArray_New(D_VALUE);
		if( self->tid == DAO_CODEBLOCK ){
			DString_AppendChar( copy->name, '[' );
		}else if( self->tid != DAO_VARIANT && m ){
			DString_AppendChar( copy->name, self->name->mbs[0] ); /* @routine<> */
			for(i=1,n=self->name->size; i<n; i++){
				char ch = self->name->mbs[i];
				if( ch != '_' && !isalnum( ch ) ) break;
				DString_AppendChar( copy->name, self->name->mbs[i] );
			}
			DString_AppendChar( copy->name, '<' );
		}
		for(i=0,n=self->nested->size; i<n; i++){
			type = DaoType_DefineTypes( self->nested->items.pType[i], ns, defs );
			if( type ==NULL ) goto DefFailed;
			DArray_Append( copy->nested, type );
			DString_Append( copy->name, type->name );
			if( i+1 < (int)self->nested->size ) DString_AppendChar( copy->name, sep );
		}
		if( self->tid == DAO_CTYPE || self->tid == DAO_CDATA || self->tid == DAO_CSTRUCT ){
			if( self->typer->core->kernel->sptree ){
				DaoType *sptype = self->typer->core->kernel->abtype;
				if( self->tid == DAO_CTYPE ) sptype = sptype->aux->xCtype.ctype;
				sptype = DaoCdataType_Specialize( sptype, copy->nested->items.pType, copy->nested->size );
				if( sptype ){
					DMap_Erase2( defs, copy );
					return sptype;
				}
			}
		}
		/* NOT FOR @T<int|string> kind types, see notes below: */
		if( self->aux && self->aux->type == DAO_TYPE ){
			copy->aux = (DaoValue*) DaoType_DefineTypes( & self->aux->xType, ns, defs );
			if( copy->aux ==NULL ) goto DefFailed;
			GC_IncRC( copy->aux );
			if( self->tid != DAO_VARIANT && (m || self->tid == DAO_CODEBLOCK) ){
				DString_AppendMBS( copy->name, "=>" );
				DString_Append( copy->name, copy->aux->xType.name );
			}
		}
		if( self->tid == DAO_CODEBLOCK ){
			DString_AppendChar( copy->name, ']' );
		}else if( self->tid != DAO_VARIANT && m ){
			DString_AppendChar( copy->name, '>' );
		}
	}
	if( self->bases ){
		copy->bases = DArray_New(D_VALUE);
		for(i=0,n=self->bases->size; i<n; i++){
			type = DaoType_DefineTypes( self->bases->items.pType[i], ns, defs );
			DArray_Append( copy->bases, type );
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
		copy->attrib |= DAO_TYPE_CODESECT;
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
		return node->value.pType;
	}else{
		/* reference count already increased */
		DaoNamespace_AddType( ns, copy->name, copy );
		DMap_Insert( defs, self, copy );
	}
	//DValue_Clear( & copy->value );
	DaoType_InitDefault( copy );
	return copy;
DefFailed:
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
	daoint i, n;
	if( self->tid == DAO_THT ){
		DMap_Insert( types, self, 0 );
		return;
	}
	if( self->nested ){
		for(i=0,n=self->nested->size; i<n; i++){
			DaoType_GetTypeHolders( self->nested->items.pType[i], types );
		}
	}
	if( self->bases ){
		for(i=0,n=self->bases->size; i<n; i++){
			DaoType_GetTypeHolders( self->bases->items.pType[i], types );
		}
	}
	if( self->tid == DAO_TYPE && self->aux && self->aux->type == DAO_TYPE )
		DaoType_GetTypeHolders( & self->aux->xType, types );
}
int DaoType_CheckTypeHolder( DaoType *self, DaoType *tht )
{
	daoint i, n, bl = 0;
	if( self == tht ) return 1;
	if( self->tid == DAO_THT && tht->tid == DAO_THT ) return 0;
	if( self->tid == DAO_THT ) return DaoType_CheckTypeHolder( tht, self );
	if( self->nested ){
		for(i=0,n=self->nested->size; i<n; i++){
			bl |= DaoType_CheckTypeHolder( self->nested->items.pType[i], tht );
		}
	}
	if( self->bases ){
		for(i=0,n=self->bases->size; i<n; i++){
			bl |= DaoType_CheckTypeHolder( self->bases->items.pType[i], tht );
		}
	}
	if( self->cbtype ) bl |= DaoType_CheckTypeHolder( self->cbtype, tht );
	if( self->aux && self->aux->type == DAO_TYPE )
		bl |= DaoType_CheckTypeHolder( & self->aux->xType, tht );
	return bl;
}
void DaoType_ResetTypeHolders( DaoType *self, DMap *types )
{
	DNode *it;
	for(it=DMap_First(types); it; ){
		if( DaoType_CheckTypeHolder( it->key.pType, self ) ){
			DMap_EraseNode( types, it );
			it = DMap_First(types);
			continue;
		}
		it = DMap_Next( types, it );
	}
}

extern DMutex mutex_methods_setup;

DaoRoutine* DaoType_FindFunction( DaoType *self, DString *name )
{
	DNode *node;
	DaoTypeCore *core = self->typer->core;
	DaoTypeKernel *kernel = self->kernel;
	if( core->kernel == NULL ) return NULL;
	if( core->kernel->methods == NULL ){
		DaoNamespace_SetupMethods( core->kernel->nspace, self->typer );
		if( core->kernel->methods == NULL ) return NULL;
	}
	if( self->tid == DAO_CSTRUCT || self->tid == DAO_CDATA || self->tid == DAO_CTYPE ){
		/* For non cdata type, core->kernel->abtype could be NULL: */
		if( self->kernel == core->kernel && self->aux != core->kernel->abtype->aux ){
			/* Specialize methods for specialized cdata type: */
			DaoCdataType_SpecializeMethods( self );
		}
	}
	if( self->kernel == NULL ){
		/* The type is created before the setup of the typer structure: */
		DMutex_Lock( & mutex_methods_setup );
		if( self->kernel == NULL ){
			GC_IncRC( core->kernel );
			self->kernel = core->kernel;
		}
		DMutex_Unlock( & mutex_methods_setup );
	}
	node = DMap_Find( self->kernel->methods, name );
	if( node ) return node->value.pRoutine;
	return NULL;
}
DaoRoutine* DaoType_FindFunctionMBS( DaoType *self, const char *name )
{
	DString mbs = DString_WrapMBS( name );
	return DaoType_FindFunction( self, & mbs );
}
DaoValue* DaoType_FindValue( DaoType *self, DString *name )
{
	DaoValue *func = (DaoValue*) DaoType_FindFunction( self, name );
	DNode *node;
	if( func ) return func;
	return DaoType_FindValueOnly( self, name );
}
DaoValue* DaoType_FindValueOnly( DaoType *self, DString *name )
{
	/* Values are not specialized. */
	/* Get the original type kernel for template-like cdata type: */
	DaoTypeKernel *kernel = self->typer->core->kernel;
	DaoValue *value = NULL;
	DNode *node;
	if( kernel == NULL ) return NULL;
	/* mainly for C data type: */
	if( kernel->abtype && kernel->abtype->aux ){
		if( DString_EQ( name, kernel->abtype->name ) ) value = kernel->abtype->aux;
	}
	if( kernel->values == NULL ){
		DaoNamespace_SetupValues( kernel->nspace, self->typer );
		if( kernel->values == NULL ) return value;
	}
	node = DMap_Find( kernel->values, name );
	if( node ) return node->value.pValue;
	return value;
}



/* interface implementations */
void DaoInterface_Delete( DaoInterface *self )
{
	GC_DecRC( self->nspace );
	GC_DecRC( self->abtype );
	DArray_Delete( self->supers );
	DMap_Delete( self->methods );
	dao_free( self );
}
DaoTypeBase interTyper=
{
	"interface", & baseCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoInterface_Delete, NULL
};

DaoInterface* DaoInterface_New( DaoNamespace *nspace, const char *name )
{
	DaoInterface *self = (DaoInterface*) dao_malloc( sizeof(DaoInterface) );
	DaoValue_Init( self, DAO_INTERFACE );
	self->trait |= DAO_VALUE_DELAYGC;
	self->derived = 0;
	self->supers = DArray_New(D_VALUE);
	self->methods = DHash_New(D_STRING,D_VALUE);
	self->abtype = DaoType_New( name, DAO_INTERFACE, (DaoValue*)self, NULL );
	self->nspace = nspace;
	GC_IncRC( self->abtype );
	GC_IncRC( self->nspace );
	return self;
}
static int DaoRoutine_IsCompatible( DaoRoutine *self, DaoType *type, DMap *binds )
{
	DaoRoutine *rout;
	daoint i, j, n, k=-1, max = 0;
	if( self->overloads == NULL ) return DaoType_Match( self->routType, type, NULL, binds );
	for(i=0,n=self->overloads->routines->size; i<n; i++){
		rout = self->overloads->routines->items.pRoutine[i];
		if( rout->routType == type ) return 1;
	}
	for(i=0,n=self->overloads->routines->size; i<n; i++){
		rout = self->overloads->routines->items.pRoutine[i];
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
int DaoInterface_CheckBind( DArray *methods, DaoType *type, DMap *binds )
{
	DaoRoutine *rout2;
	daoint i, n, id;
	if( type->tid == DAO_OBJECT ){
		DaoClass *klass = & type->aux->xClass;
		for(i=0,n=methods->size; i<n; i++){
			DaoRoutine *rout = methods->items.pRoutine[i];
			id = DaoClass_FindConst( klass, rout->routName );
			if( id <0 ) return 0;
			rout2 = (DaoRoutine*) DaoClass_GetConst( klass, id );
			if( rout2->type != DAO_ROUTINE ) return 0;
			if( DaoRoutine_IsCompatible( rout2, rout->routType, binds ) ==0 ) return 0;
		}
	}else if( type->tid == DAO_INTERFACE ){
		DaoInterface *inter = (DaoInterface*) type->aux;
		for(i=0,n=methods->size; i<n; i++){
			DaoRoutine *rout = methods->items.pRoutine[i];
			DNode *it = DMap_Find( inter->methods, rout->routName );
			if( it == NULL ) return 0;
			if( DaoRoutine_IsCompatible( it->value.pRoutine, rout->routType, binds ) ==0 ) return 0;
		}
	}else{
		for(i=0,n=methods->size; i<n; i++){
			DaoRoutine *rout = methods->items.pRoutine[i];
			DaoRoutine *func = DaoType_FindFunction( type, rout->routName );
			if( func == NULL ) return 0;
			if( DaoRoutine_IsCompatible( func, rout->routType, binds ) ==0 ) return 0;
		}
	}
	return 1;
}
static void DaoInterface_TempBind( DaoInterface *self, DaoType *type, DMap *binds )
{
	daoint i, N = self->supers->size;
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
int DaoInterface_BindTo( DaoInterface *self, DaoType *type, DMap *binds )
{
	DNode *it;
	DMap *newbinds = NULL;
	DArray *methods;
	void *pvoid[2];
	daoint i, n, bl;

	/* XXX locking */
	if( type->interfaces == NULL ) type->interfaces = DHash_New(D_VALUE,0);

	pvoid[0] = type;
	pvoid[1] = self->abtype;
	if( (it = DMap_Find( type->interfaces, self )) ) return it->value.pVoid != NULL;
	if( binds && DMap_Find( binds, pvoid ) ) return 1;
	if( binds ==NULL ) newbinds = binds = DHash_New(D_VOID2,0);
	DaoInterface_TempBind( self, type, binds );
	methods = DArray_New(0);
	DMap_SortMethods( self->methods, methods );
	bl = DaoInterface_CheckBind( methods, type, binds );
	DArray_Delete( methods );
	if( newbinds ) DMap_Delete( newbinds );
	DMap_Insert( type->interfaces, self, bl ? self : NULL );
	if( bl == 0 ) return 0;
	for(i=0,n=self->supers->size; i<n; i++){
		DaoInterface *super = (DaoInterface*) self->supers->items.pValue[i];
		if( DMap_Find( type->interfaces, super ) ) continue;
		DMap_Insert( type->interfaces, super, super );
	}
	return 1;
}
void DaoMethods_Insert( DMap *methods, DaoRoutine *rout, DaoNamespace *ns, DaoType *host );
void DaoInterface_DeriveMethods( DaoInterface *self )
{
	daoint i, k, m, N = self->supers->size;
	DaoInterface *super;
	DNode *it;
	for(i=0; i<N; i++){
		super = (DaoInterface*) self->supers->items.pValue[i];
		for(it=DMap_First(super->methods); it; it=DMap_Next( super->methods, it )){
			if( it->value.pRoutine->overloads ){
				DRoutines *routs = it->value.pRoutine->overloads;
				for(k=0,m=routs->routines->size; k<m; k++){
					DaoRoutine *rout = routs->routines->items.pRoutine[i];
					DaoMethods_Insert( self->methods, rout, NULL, self->abtype );
				}
			}else{
				DaoMethods_Insert( self->methods, it->value.pRoutine, NULL, self->abtype );
			}
		}
	}
	self->derived = 1;
}

DAO_DLL void DMap_SortMethods( DMap *hash, DArray *methods )
{
	DMap *map = DMap_New(D_STRING,0);
	DString *name = DString_New(1);
	DNode *it;
	daoint i, n;
	for(it=DMap_First(hash); it; it=DMap_Next(hash,it)){
		if( it->value.pRoutine->overloads ){
			DRoutines *one = it->value.pRoutine->overloads;
			for(i=0,n=one->routines->size; i<n; i++){
				DaoRoutine *rout = one->routines->items.pRoutine[i];
				DString_Assign( name, rout->routName );
				DString_AppendMBS( name, " " );
				DString_Append( name, rout->routType->name );
				DMap_Insert( map, name, (void*)rout );
			}
		}else{
			DaoRoutine *rout = it->value.pRoutine;
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
int DaoType_MatchInterface( DaoType *self, DaoInterface *inter, DMap *binds )
{
	DMap *inters = self->interfaces;
	DNode *it;
	daoint i;
	if( inter == NULL ) return DAO_MT_NOT;
	if( inters == NULL ) return DAO_MT_SUB * DaoInterface_BindTo( inter, self, binds );
	if( (it = DMap_Find( inters, inter )) ) return it->value.pVoid ? DAO_MT_SUB : DAO_MT_NOT;
	return DAO_MT_SUB * DaoInterface_BindTo( inter, self, binds );
}



static void DaoCdata_GetField( DaoValue *self, DaoProcess *proc, DString *name )
{
	DaoType *type = self->xCdata.ctype;
	DaoValue *p = DaoType_FindValue( type, name );
	if( proc->vmSpace->options & DAO_OPTION_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( p == NULL ){
		DaoValue *pars[2];
		DaoRoutine *func = NULL;
		DaoString str = {DAO_STRING,0,0,0,1,NULL};
		daoint n = proc->factory->size;
		int npar = 1;

		str.data = name;
		pars[0] = self;

		DString_SetMBS( proc->mbstring, "." );
		DString_Append( proc->mbstring, name );
		func = DaoType_FindFunction( type, proc->mbstring );
		if( func == NULL ){
			npar = 2;
			pars[1] = (DaoValue*) & str;
			DString_SetMBS( proc->mbstring, "." );
			func = DaoType_FindFunction( type, proc->mbstring );
		}
		func = DaoRoutine_ResolveX( func, self, pars+1, npar-1, DVM_CALL );
		if( func == NULL ){
			DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, "not exist" );
			return;
		}
		func->pFunc( proc, pars, npar );
		if( proc->factory->size > n ) DArray_Erase( proc->factory, n, -1 );
	}else{
		DaoProcess_PutValue( proc, p );
	}
}
static void DaoCdata_SetField( DaoValue *self, DaoProcess *proc, DString *name, DaoValue *value )
{
	int npar = 1;
	DaoValue *pars[2];
	DaoRoutine *func = NULL;
	DaoString str = {DAO_STRING,0,0,0,1,NULL};
	DaoType *type = self->xCdata.ctype;

	str.data = name;
	pars[0] = pars[1] = value;

	DString_SetMBS( proc->mbstring, "." );
	DString_Append( proc->mbstring, name );
	DString_AppendMBS( proc->mbstring, "=" );
	if( proc->vmSpace->options & DAO_OPTION_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	func = DaoType_FindFunction( type, proc->mbstring );
	if( func == NULL ){
		pars[0] = (DaoValue*) & str;
		npar = 2;
		DString_SetMBS( proc->mbstring, ".=" );
		func = DaoType_FindFunction( type, proc->mbstring );
	}
	if( func == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, name->mbs );
		return;
	}
	DaoProcess_PushCallable( proc, func, self, pars, npar );
}
static void DaoCdata_GetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid )
{
	DaoCdata *self = & self0->xCdata;
	DaoType *type = self->ctype;
	DaoRoutine *func = NULL;

	if( proc->vmSpace->options & DAO_OPTION_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	func = DaoType_FindFunctionMBS( type, "[]" );
	if( func == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, "" );
		return;
	}
	DaoProcess_PushCallable( proc, func, self0, & pid, 1 );
}
static void DaoCdata_SetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid, DaoValue *value )
{
	DaoType *type = self0->xCdata.ctype;
	DaoRoutine *func = NULL;
	DaoValue *p[2];

	DString_SetMBS( proc->mbstring, "[]=" );
	if( proc->vmSpace->options & DAO_OPTION_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	func = DaoType_FindFunction( type, proc->mbstring );
	if( func == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, "" );
		return;
	}
	p[0] = value;
	p[1] = pid;
	DaoProcess_PushCallable( proc, func, self0, p, 2 );
}
static void DaoCdata_GetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N )
{
	DaoType *type = self->xCdata.ctype;
	DaoRoutine *func = NULL;
	if( proc->vmSpace->options & DAO_OPTION_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( N == 1 ){
		DaoCdata_GetItem1( self, proc, ids[0] );
		return;
	}
	func = DaoType_FindFunctionMBS( type, "[]" );
	if( func == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, "" );
		return;
	}
	DaoProcess_PushCallable( proc, func, self, ids, N );
}
static void DaoCdata_SetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	DaoType *type = self->xCdata.ctype;
	DaoRoutine *func = NULL;
	DaoValue *p[ DAO_MAX_PARAM ];
	if( proc->vmSpace->options & DAO_OPTION_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( N == 1 ){
		DaoCdata_SetItem1( self, proc, ids[0], value );
		return;
	}
	func = DaoType_FindFunctionMBS( type, "[]=" );
	if( func == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, "" );
		return;
	}
	memcpy( p+1, ids, N*sizeof(DaoValue*) );
	p[0] = value;
	DaoProcess_PushCallable( proc, func, self, p, N+1 );
}
static void DaoCdata_Print( DaoValue *self0, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	int ec;
	char buf[50];
	DaoRoutine *meth;
	DaoCdata *self = & self0->xCdata;

	sprintf( buf, "[%p]", self );

	if( self0 == self->ctype->value ){
		DaoStream_WriteString( stream, self->ctype->name );
		DaoStream_WriteMBS( stream, "[default]" );
		return;
	}
	if( cycData != NULL && DMap_Find( cycData, self ) != NULL ){
		DaoStream_WriteString( stream, self->ctype->name );
		DaoStream_WriteMBS( stream, buf );
		return;
	}
	if( cycData ) MAP_Insert( cycData, self, self );

	meth = DaoType_FindFunctionMBS( self->ctype, "__PRINT__" );
	if( meth && DaoProcess_Call( proc, meth, NULL, &self0, 1 ) == 0 ) return

	DaoValue_Clear( & proc->stackValues[0] );
	meth = DaoType_FindFunctionMBS( self->ctype, "serialize" );
	if( meth && DaoProcess_Call( proc, meth, NULL, &self0, 1 ) == 0 ){
		DaoValue_Print( proc->stackValues[0], proc, stream, cycData );
	}else{
		DaoStream_WriteString( stream, self->ctype->name );
		DaoStream_WriteMBS( stream, buf );
	}
}

void DaoTypeKernel_Delete( DaoTypeKernel *self )
{
	/* self->core may no longer be valid, but self->typer->core always is: */
	if( self->typer->core ) self->typer->core->kernel = NULL;
	if( self->core == (DaoTypeCore*)(self + 1) ){
		self->typer->core = NULL;
	}
	if( self->values ) DMap_Delete( self->values );
	if( self->methods ) DMap_Delete( self->methods );
	if( self->sptree ) DTypeSpecTree_Delete( self->sptree );
	dao_free( self );
}

DaoTypeBase typeKernelTyper =
{
	"TypeKernel", & baseCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoTypeKernel_Delete, NULL
};

DaoTypeKernel* DaoTypeKernel_New( DaoTypeBase *typer )
{
	int extra = typer && typer->core ? 0 : sizeof(DaoTypeCore);
	DaoTypeKernel *self = (DaoTypeKernel*) dao_calloc( 1, sizeof(DaoTypeKernel) + extra );
	DaoValue_Init( self, DAO_TYPEKERNEL );
	self->trait |= DAO_VALUE_DELAYGC;
	self->Sliced = NULL;
	if( typer ) self->typer = typer;
	if( typer && typer->core ) self->core = typer->core;
	if( self->core == NULL ){
		self->core = (DaoTypeCore*)(self + 1);
		self->core->GetField = DaoCdata_GetField;
		self->core->SetField = DaoCdata_SetField;
		self->core->GetItem = DaoCdata_GetItem;
		self->core->SetItem = DaoCdata_SetItem;
		self->core->Print = DaoCdata_Print;
	}
	if( self->core->kernel == NULL ) self->core->kernel = self;
	return self;
}




static DTypeParam* DTypeParam_New( DTypeSpecTree *tree )
{
	DTypeParam *self = (DTypeParam*) dao_calloc( 1, sizeof(DTypeParam) );
	self->tree = tree;
	return self;
}
static void DTypeParam_Delete( DTypeParam *self )
{
	while( self->first ){
		DTypeParam *node = self->first;
		self->first = node->next;
		DTypeParam_Delete( node );
	}
	dao_free( self );
}
static DTypeParam*
DTypeParam_Add( DTypeParam *self, DaoType *types[], int count, int pid, DaoType *sptype )
{
	DTypeParam *param, *ret;
	DaoType *type;
	int i, n;
	if( pid >= count ){
		/* If a specialization with the same parameter signature is found, return it: */
		for(param=self->first; param; param=param->next) if( param->sptype ) return param;
		param = DTypeParam_New( self->tree );
		param->sptype = sptype;
		/* Add a leaf. */
		if( self->last ){
			self->last->next = param;
			self->last = param;
		}else{
			self->first = self->last = param;
		}
		return param;
	}
	type = types[pid];
	for(param=self->first; param; param=param->next){
		if( param->type == type ) return DTypeParam_Add( param, types, count, pid+1, sptype );
	}
	/* Add a new internal node: */
	param = DTypeParam_New( self->tree );
	param->type = type;
	ret = DTypeParam_Add( param, types, count, pid+1, sptype );
	/* Add the node to the tree after all its child nodes have been created, to ensure
	 * a reader will always lookup in a valid tree in multi-threaded applications: */
	if( self->last ){
		self->last->next = param;
		self->last = param;
	}else{
		self->first = self->last = param;
	}
	return ret;
}
static DaoType* DTypeParam_GetLeaf( DTypeParam *self, int pid, int *ms )
{
	DArray *defaults = self->tree->defaults;
	DTypeParam *param;
	*ms = 0;
	if( self->sptype ) return self->sptype; /* a leaf */
	/* Explicit specializable types has no default type parameters: */
	if( defaults->size && pid > (int)defaults->size ) return NULL;
	if( pid < (int)defaults->size && defaults->items.pType[pid] == NULL ) return NULL;
	for(param=self->first; param; param=param->next){
		if( param->type == NULL ) return param->sptype; /* a leaf */
		if( param->type == defaults->items.pType[pid] ){
			DaoType *type = DTypeParam_GetLeaf( param, pid + 1, ms );
			if( type ) return type;
		}
	}
	return NULL;
}
static DaoType*
DTypeParam_Get2( DTypeParam *self, DaoType *types[], int count, int pid, int *score )
{
	DTypeParam *param;
	DaoType *argtype, *sptype = NULL, *best = NULL;
	int i, m, k = 0, max = 0;

	*score = 1;
	if( pid >= count ) return DTypeParam_GetLeaf( self, pid, score );
	argtype = types[pid];
	for(param=self->first; param; param=param->next){
		DaoType *partype = param->type;
		if( partype == NULL ) continue;
		if( argtype->tid != partype->tid ) continue;
		if( (m = DaoType_MatchTo( argtype, partype, NULL )) < DAO_MT_EQ ){
			continue;
		}
		if( (sptype = DTypeParam_Get2( param, types, count, pid+1, & k )) == NULL ) continue;
		m += k;
		if( m > max ){
			best = sptype;
			max = m;
		}
	}
	*score = max;
	return best;
}
DTypeSpecTree* DTypeSpecTree_New()
{
	DTypeSpecTree *self = (DTypeSpecTree*)dao_malloc( sizeof(DTypeSpecTree) );
	self->root = DTypeParam_New( self );
	self->holders = DArray_New(D_VALUE);
	self->defaults = DArray_New(D_VALUE);
	self->sptypes = DArray_New(D_VALUE);
	return self;
}
void DTypeSpecTree_Delete( DTypeSpecTree *self )
{
	DTypeParam_Delete( self->root );
	DArray_Delete( self->holders );
	DArray_Delete( self->defaults );
	DArray_Delete( self->sptypes );
	dao_free( self );
}
/* Test if the type can be specialized according to the type parameters: */
int DTypeSpecTree_Test( DTypeSpecTree *self, DaoType *types[], int count )
{
	daoint i, n = self->holders->size;
	if( n == 0 || count > n ) return 0;
	for(i=count; i<n; i++){
		if( self->defaults->items.pType[i] == NULL ) return 0;
	}
	for(i=0; i<count; i++){
		DaoType *par = self->holders->items.pType[i];
		DaoType *arg = types[i];
		if( arg->tid == DAO_UDT ) return 0;
		if( DaoType_MatchTo( arg, par, NULL ) ==0 ) return 0;
	}
	return 1;
}
void DTypeSpecTree_Add( DTypeSpecTree *self, DaoType *types[], int count, DaoType *sptype )
{
	DTypeParam_Add( self->root, types, count, 0, sptype );
	DArray_Append( self->sptypes, sptype );
}
DaoType* DTypeSpecTree_Get( DTypeSpecTree *self, DaoType *types[], int count )
{
	int score = 0;
	/* For explicitly specialized types, the specialization tree has no type holders: */
	if( self->holders->size && DTypeSpecTree_Test( self, types, count ) == 0 ) return NULL;
	return DTypeParam_Get2( self->root, types, count, 0, & score );
}
extern DaoType* DaoCdata_NewType( DaoTypeBase *typer );
DaoType* DaoCdataType_Specialize( DaoType *self, DaoType *types[], int count )
{
	DaoType *sptype, *sptype2;
	DaoTypeKernel *kernel;
	DTypeSpecTree *sptree;
	uchar_t tid = self->tid;
	daoint i, pos;

	assert( self->tid == DAO_CSTRUCT || self->tid == DAO_CDATA || self->tid == DAO_CTYPE );

	self = self->kernel->abtype;
	if( tid == DAO_CTYPE ) self = self->aux->xCtype.cdtype;

	if( (kernel = self->kernel) == NULL ) return NULL;
	if( (sptree = kernel->sptree) == NULL ) return NULL;
	if( (sptype = DTypeSpecTree_Get( sptree, types, count )) ){
		if( tid == DAO_CTYPE ) return sptype->aux->xCdata.ctype;
		return sptype;
	}
	if( DTypeSpecTree_Test( sptree, types, count ) == 0 ) return NULL;

	/* Specialized cdata type will be initialized with the same kernel as the template type.
	 * Upon method accessing, a new kernel will be created with specialized methods. */
	sptype = DaoCdata_NewType( self->typer );
	sptype2 = sptype->aux->xCtype.ctype;
	GC_ShiftRC( self->kernel, sptype->kernel );
	GC_ShiftRC( self->kernel, sptype2->kernel );
	sptype->kernel = self->kernel;
	sptype2->kernel = self->kernel;
	sptype->nested = DArray_New(D_VALUE);
	if( self->tid == DAO_CTYPE ){
		sptype->tid = self->aux->xCtype.cdtype->tid;
	}else{
		sptype->tid = self->tid;
	}

	pos = DString_FindChar( sptype->name, '<', 0 );
	if( pos != MAXSIZE ) DString_Erase( sptype->name, pos, -1 );
	DString_AppendChar( sptype->name, '<' );
	for(i=0; i<count; i++){
		if( i ) DString_AppendChar( sptype->name, ',' );
		DString_Append( sptype->name, types[i]->name );
		DArray_Append( sptype->nested, types[i] );
	}
	for(i=count; i<sptree->holders->size; i++){
		if( i ) DString_AppendChar( sptype->name, ',' );
		DString_Append( sptype->name, sptree->defaults->items.pType[i]->name );
		DArray_Append( sptype->nested, sptree->defaults->items.pType[i] );
	}
	sptype2->nested = DArray_Copy( sptype->nested );
	DString_AppendChar( sptype->name, '>' );
	DString_Assign( sptype2->name, sptype->name );
	DTypeSpecTree_Add( sptree, sptype->nested->items.pType, sptype->nested->size, sptype );
	if( self->bases ){
		DMap *defs = DHash_New(0,0);
		for(i=0; i<count; i++){
			DaoType_MatchTo( types[i], sptree->holders->items.pType[i], defs );
		}
		sptype->bases = DArray_New(D_VALUE);
		sptype2->bases = DArray_New(D_VALUE);
		for(i=0; i<self->bases->size; i++){
			DaoType *type = self->bases->items.pType[i];
			type = DaoType_DefineTypes( type, type->kernel->nspace, defs );
			DArray_Append( sptype->bases, type );
			DArray_Append( sptype2->bases, type->aux->xCdata.ctype );
		}
		DMap_Delete( defs );
	}
	/* May need to get rid of the attributes for type holders: */
	DaoType_CheckAttributes( sptype );
	DaoType_CheckAttributes( sptype2 );
	if( tid == DAO_CTYPE ) return sptype2;
	return sptype;
}

int DaoRoutine_Finalize( DaoRoutine *self, DaoType *host, DMap *deftypes );


void DaoCdataType_SpecializeMethods( DaoType *self )
{
	DaoType *original = self->typer->core->kernel->abtype;
	DaoTypeKernel *kernel;
	daoint i, k;

	if( self == original ) return;
	if( self->kernel != original->kernel ) return;
	if( original->kernel == NULL || original->kernel->methods == NULL ) return;
	assert( self->tid == DAO_CSTRUCT || self->tid == DAO_CDATA || self->tid == DAO_CTYPE );
	if( self->tid == DAO_CTYPE ) self = self->aux->xCtype.cdtype;
	if( self->bases ){
		for(i=0; i<self->bases->size; i++){
			DaoType *base = self->bases->items.pType[i];
			DaoCdataType_SpecializeMethods( base );
		}
	}
	if( original->kernel->sptree == NULL ) return;
	DMutex_Lock( & mutex_methods_setup );
	if( self->kernel == original->kernel && original->kernel && original->kernel->methods ){
		DaoNamespace *nspace = self->kernel->nspace;
		DMap *orimeths = original->kernel->methods;
		DMap *methods = DHash_New( D_STRING, D_VALUE );
		DMap *defs = DHash_New(0,0);
		DArray *parents = DArray_New(0);
		DNode *it;

		kernel = DaoTypeKernel_New( self->typer );
		kernel->attribs = original->kernel->attribs;
		kernel->nspace = original->kernel->nspace;
		kernel->abtype = original;
		GC_IncRC( kernel->nspace );
		GC_IncRC( kernel->abtype );
		GC_ShiftRC( kernel, self->aux->xCtype.ctype->kernel );
		GC_ShiftRC( kernel, self->aux->xCtype.cdtype->kernel );
		self->aux->xCtype.ctype->kernel = kernel;
		self->aux->xCtype.cdtype->kernel = kernel;

		for(i=0; i<self->nested->size; i++){
			DaoType_MatchTo( self->nested->items.pType[i], original->nested->items.pType[i], defs );
		}
		DArray_Append( parents, self );
		for(k=0; k<parents->size; k++){
			DaoType *type = parents->items.pType[k];
			if( type->bases == NULL ) continue;
			for(i=0; i<type->bases->size; i++){
				DaoType *base = type->bases->items.pType[i];
				DArray_Append( parents, base );
			}
		}
		for(it=DMap_First(orimeths); it; it=DMap_Next(orimeths, it)){
			DaoRoutine *rout, *rout2, *routine = it->value.pRoutine;
			if( routine->routHost->aux != original->aux ) continue;
			if( routine->overloads ){
				for(i=0; i<routine->overloads->routines->size; i++){
					rout = rout2 = routine->overloads->routines->items.pRoutine[i];
					if( rout->routHost->aux != original->aux ) continue;
					rout = DaoRoutine_Copy( rout, 1, 0, 0 );
					if( rout2->attribs & DAO_ROUT_INITOR )
						DString_Assign( rout->routName, self->name );
					DaoRoutine_Finalize( rout, self, defs );
					DaoMethods_Insert( methods, rout, nspace, self );
				}
			}else{
				rout = DaoRoutine_Copy( routine, 1, 0, 0 );
				if( routine->attribs & DAO_ROUT_INITOR )
					DString_Assign( rout->routName, self->name );
				DaoRoutine_Finalize( rout, self, defs );
				DaoMethods_Insert( methods, rout, nspace, self );
			}
		}
		DMap_Delete( defs );

		for(i=1; i<parents->size; i++){
			DaoType *sup = parents->items.pType[i];
			DMap *supMethods = sup->kernel->methods;
			for(it=DMap_First(supMethods); it; it=DMap_Next(supMethods, it)){
				if( it->value.pRoutine->overloads ){
					DRoutines *meta = (DRoutines*) it->value.pVoid;
					/* skip constructor */
					if( DString_EQ( it->value.pRoutine->routName, sup->name ) ) continue;
					for(k=0; k<meta->routines->size; k++){
						DaoRoutine *rout = meta->routines->items.pRoutine[k];
						/* skip methods not defined in this parent type */
						if( rout->routHost != sup->kernel->abtype ) continue;
						DaoMethods_Insert( methods, rout, nspace, self );
					}
				}else{
					DaoRoutine *rout = it->value.pRoutine;
					/* skip constructor */
					if( DString_EQ( rout->routName, sup->name ) ) continue;
					/* skip methods not defined in this parent type */
					if( rout->routHost != sup->kernel->abtype ) continue;
					DaoMethods_Insert( methods, rout, nspace, self );
				}
			}
		}
		DArray_Delete( parents );
		/* Set methods field after it has been setup, for read safety in multithreading: */
		kernel->methods = methods;
	}
	DMutex_Unlock( & mutex_methods_setup );
}
