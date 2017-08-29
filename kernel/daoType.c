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


static unsigned short dao_type_matrix[END_EXTRA_TYPES][END_EXTRA_TYPES];


void DaoType_MapNames( DaoType *self );
void DaoType_CheckAttributes( DaoType *self );


DaoType* DaoType_New( DaoNamespace *ns, const char *name, int tid, DaoValue *aux, DList *args )
{
	DaoType *self = (DaoType*) dao_calloc( 1, sizeof(DaoType) );
	DaoValue_Init( self, DAO_TYPE );
	self->trait |= DAO_VALUE_DELAYGC;
	self->tid = tid;
	self->name = DString_New();
	self->core = DaoType_GetCoreByID( tid );
	self->nameSpace = ns;
	GC_IncRC( ns );

	//assert( tid != DAO_PAR_VALIST || aux != NULL );
	if( aux == NULL && tid == DAO_PAR_VALIST ) aux = (DaoValue*) ns->vmSpace->typeAny;

	if( aux ){
		self->aux = aux;
		GC_IncRC( aux );
	}
	DString_SetChars( self->name, name );
	if( tid == DAO_ENUM || tid == DAO_PAR_NAMED || tid == DAO_PAR_DEFAULT ){
		self->fname = DString_New();
	}
	if( args ){
		self->args = DList_New( DAO_DATA_VALUE );
		DList_Assign( self->args, args );
	}else if( tid == DAO_ROUTINE || tid == DAO_TUPLE ){
		self->args = DList_New( DAO_DATA_VALUE );
	}
	switch( tid ){
	case DAO_ENUM : self->mapNames = DMap_New( DAO_DATA_STRING, 0 ); break;
	case DAO_TUPLE :
	case DAO_ROUTINE : DaoType_MapNames( self );
	}
	DaoType_CheckAttributes( self );
	DaoType_InitDefault( self );
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogNew( (DaoValue*) self );
#endif
	return self;
}


DaoType* DaoType_Copy( DaoType *other )
{
	DaoType *self = (DaoType*) dao_malloc( sizeof(DaoType) );
	memcpy( self, other, sizeof(DaoType) );
	DaoValue_Init( self, DAO_TYPE ); /* to reset gc fields */
	self->trait |= DAO_VALUE_DELAYGC;
	self->quadtype = NULL;
	self->args = NULL;
	self->bases = NULL;
	self->name = DString_Copy( other->name );
	if( other->fname ) self->fname = DString_Copy( other->fname );
	if( other->bases ) self->bases = DList_Copy( other->bases );
	if( other->args ) self->args = DList_Copy( other->args );
	if( other->mapNames ) self->mapNames = DMap_Copy( other->mapNames );
	if( other->interfaces ) self->interfaces = DMap_Copy( other->interfaces );
	GC_IncRC( self->nameSpace );
	GC_IncRC( self->aux );
	GC_IncRC( self->kernel );
	GC_IncRC( self->cbtype );
	/* DaoValue_Move() may fail for value type if self->value is null: */
	GC_IncRC( self->value );
	DaoValue_Move( other->value, & self->value, self ); /* needed for enum symbol types; */
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogNew( (DaoValue*) self );
#endif
	return self;
}

void DaoType_Delete( DaoType *self )
{
	//printf( "DaoType_Delete: %p\n", self );
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogDelete( (DaoValue*) self );
#endif
	GC_DecRC( self->aux );
	GC_DecRC( self->value );
	GC_DecRC( self->kernel );
	GC_DecRC( self->cbtype );
	GC_DecRC( self->quadtype );
	GC_DecRC( self->nameSpace );
	DString_Delete( self->name );
	if( self->fname ) DString_Delete( self->fname );
	if( self->args ) DList_Delete( self->args );
	if( self->bases ) DList_Delete( self->bases );
	if( self->mapNames ) DMap_Delete( self->mapNames );
	if( self->interfaces ) DMap_Delete( self->interfaces );
	dao_free( self );
}

void DaoType_SetNamespace( DaoType *self, DaoNamespace *nspace )
{
	DaoValue_Move( (DaoValue*) nspace, (DaoValue**) & self->nameSpace, NULL );
}

void DaoType_CheckAttributes( DaoType *self )
{
	daoint i, count = 0;

	self->realnum = self->tid >= DAO_BOOLEAN && self->tid <= DAO_FLOAT;
	self->attrib &= ~(DAO_TYPE_SPEC|DAO_TYPE_UNDEF);
	if( DString_FindChar( self->name, '@', 0 ) != DAO_NULLPOS ) self->attrib |= DAO_TYPE_SPEC;
	if( DString_FindChar( self->name, '?', 0 ) != DAO_NULLPOS ) self->attrib |= DAO_TYPE_UNDEF;

	if( self->tid == DAO_PAR_NAMED || self->tid == DAO_PAR_DEFAULT ){
		self->attrib |= DAO_TYPE_PARNAMED;
		if( self->fname != NULL && strcmp( self->fname->chars, "self" ) == 0 ){
			self->attrib |= DAO_TYPE_SELFNAMED;
		}
	}

	self->empty = 0;
	self->noncyclic = self->tid <= DAO_TUPLE;
	if( self->tid == DAO_THT ){
		daoint pos = DString_FindChar( self->name, '<', 0 );
		if( self->fname == NULL ) self->fname = DString_New();
		DString_Assign( self->fname, self->name );
		if( pos >= 0 ) DString_Erase( self->fname, pos, -1 );
	}
	if( self->aux && self->aux->type == DAO_TYPE ){
		if( self->aux->xType.attrib & DAO_TYPE_SPEC ) self->attrib |= DAO_TYPE_SPEC;
		self->noncyclic &= self->aux->xType.noncyclic;
	}else if( self->aux && self->aux->type < DAO_ARRAY ){
		self->valtype = 1;
	}
	if( self->args ){
		for(i=0; i<self->args->size; i++){
			DaoType *it = self->args->items.pType[i];
			if( it->tid == DAO_PAR_NAMED ) it = & it->aux->xType;
			if( it->attrib & DAO_TYPE_SPEC ) self->attrib |= DAO_TYPE_SPEC;
			if( it->tid >= DAO_ENUM ){
				self->noncyclic = 0;
				break;
			}
			self->noncyclic &= it->noncyclic;
		}
		if( self->tid == DAO_ROUTINE && self->args->size ){
			DaoType *it = self->args->items.pType[0];
			if( it->attrib & DAO_TYPE_SELFNAMED ) self->attrib |= DAO_TYPE_SELF;
		}
		if( (self->tid == DAO_TUPLE || self->tid == DAO_ROUTINE) && self->args->size ){
			DaoType *it = self->args->items.pType[self->args->size - 1];
			if( it->tid == DAO_PAR_VALIST ) self->variadic = 1;
		}
	}
}

void DaoType_InitDefault( DaoType *self )
{
	dao_complex com = {0.0,0.0};
	DaoValue *value = NULL;
	DaoType *itype, **types = self->args ? self->args->items.pType : NULL;
	int i, count = self->args ? self->args->size : 0;

	if( self->value && self->value->type != DAO_TUPLE ) return;
	if( self->value && self->value->xTuple.size == count ) return;

	switch( self->tid ){
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		itype = types && self->args->size > 0 ? types[0] : NULL;
		value = (DaoValue*) DaoArray_New( itype ? itype->tid : DAO_INTEGER );
		break;
#endif
	case DAO_LIST :
		value = (DaoValue*) DaoList_New();
		value->xList.ctype = self;
		GC_IncRC( self );
		break;
	case DAO_MAP :
		value = (DaoValue*) DaoMap_New(0);
		value->xMap.ctype = self;
		GC_IncRC( self );
		break;
	case DAO_TUPLE :
		value = (DaoValue*) DaoTuple_New( count );
		value->xTuple.ctype = self;
		GC_IncRC( self );
		for(i=0; i<count; i++){
			DaoType *it = types[i];
			if( it->tid == DAO_PAR_NAMED || it->tid == DAO_PAR_DEFAULT ){
				it = (DaoType*) it->aux;
			}else if( it->tid == DAO_PAR_VALIST ){
				DaoValue_Copy( dao_none_value, & value->xTuple.values[i] );
				continue;
			}
			DaoType_InitDefault( it );
			DaoValue_Copy( it->value, & value->xTuple.values[i] );
		}
		break;
	case DAO_VARIANT :
		for(i=0; i<count; i++) DaoType_InitDefault( types[i] );
		if( count ) value = types[0]->value;
		break;
	case DAO_ENUM :
		value = (DaoValue*) DaoEnum_New( self, 0 );
		/*
		// self->subtid may be set later, setting value->xEnum.subtype
		// to DAO_ENUM_SYM will ensure this value matchs to the type;
		*/
		if( self->subtid == 0 ) value->xEnum.subtype = DAO_ENUM_SYM;
		break;
	case DAO_UDT :
	case DAO_ANY :
	case DAO_THT :
	case DAO_ROUTINE :
	case DAO_INTERFACE : value = dao_none_value; break;
	case DAO_BOOLEAN : value = dao_false_value; break;
	case DAO_INTEGER : value = (DaoValue*) DaoInteger_New(0); break;
	case DAO_FLOAT  : value = (DaoValue*) DaoFloat_New(0.0); break;
	case DAO_COMPLEX : value = (DaoValue*) DaoComplex_New(com); break;
	case DAO_STRING : value = (DaoValue*) DaoString_New(); break;
	}
	GC_Assign( & self->value, value );
	if( value ) value->xBase.trait |= DAO_VALUE_CONST;
}

void DaoType_MapNames( DaoType *self )
{
	DaoType *tp;
	daoint i;
	if( self->args == NULL ) return;
	if( self->tid != DAO_TUPLE && self->tid != DAO_ROUTINE ) return;
	if( self->mapNames == NULL ) self->mapNames = DMap_New( DAO_DATA_STRING, 0 );
	for(i=0; i<self->args->size; i++){
		tp = self->args->items.pType[i];
		if( tp->fname ) MAP_Insert( self->mapNames, tp->fname, i );
	}
}
DaoType* DaoType_GetItemType( DaoType *self, int i )
{
	if( self->args == NULL ) return NULL;
	if( i < 0 || i >= self->args->size ) return NULL;
	return self->args->items.pType[i];
}
DaoType* DaoType_GetVariantItem( DaoType *self, int tid )
{
	if( self->tid == DAO_VARIANT && self->args ){
		DaoType **types = self->args->items.pType;
		daoint i, n = self->args->size;
		for(i=0; i<n; i++){
			DaoType *itype = types[i];
			if( itype->tid == tid ) return itype;
			if( itype->tid == DAO_VARIANT ){
				itype = DaoType_GetVariantItem( itype, tid );
				if( itype ) return itype;
			}
		}
	}
	return NULL;
}

static DaoType* DaoType_GetAutoCastType2( DaoType *self )
{
	if( self->tid != DAO_VARIANT ) return NULL;
	if( self->args->size == 1 ){
		return self->args->items.pType[0];
	}else if( self->args->size == 2 ){
		DaoType *T1 = self->args->items.pType[0];
		DaoType *T2 = self->args->items.pType[1];
		if( T1->tid == DAO_NONE ) return T2;
		if( T2->tid == DAO_NONE ) return T1;
	}
	return NULL;
}

DaoType* DaoType_GetAutoCastType( DaoType *self )
{
	int invar = self->invar;
	int konst = self->konst;
	DaoType *type = DaoType_GetAutoCastType2( self );
	if( type == NULL ) return NULL;
	if( konst ) return DaoType_GetConstType( type );
	if( invar ) return DaoType_GetInvarType( type );
	return type;
}

/*
// Output:
// quads[0] = base type;
// quads[1] = const type;
// quads[2] = invar type;
// quads[3] = var type;
*/
static void DaoType_GetQuadTypes( DaoType *self, DaoType *quads[4] )
{
	int i;
	DaoType *types[4] = {NULL};

	types[0] = self;
	if( types[0] ) types[1] = types[0]->quadtype;
	if( types[1] ) types[2] = types[1]->quadtype;
	if( types[2] ) types[3] = types[2]->quadtype;

	memset( quads, 0, 4*sizeof(DaoType*) );
	for(i=0; i<4; ++i){
		if( types[i] == NULL ) break;
		if( types[i]->konst ){  /* Check konst before invar: */
			quads[1] = types[i];
		}else if( types[i]->invar ){
			quads[2] = types[i];
		}else if( types[i]->var ){
			quads[3] = types[i];
		}else{
			quads[0] = types[i];
		}
	}
}
static void DaoType_RelinkQuadTypes( DaoType *self, DaoType *other )
{
	DaoType *cur, *next, *types[4], *quads[4] = {NULL};
	int i, count = 0;

	DaoType_GetQuadTypes( self, quads );
	if( other->konst )  quads[1] = other;
	if( other->invar )  quads[2] = other;
	if( other->var ) quads[3] = other;

	for(i=0; i<4; ++i){
		if( quads[i] ){
			types[count++] = quads[i];
			GC_IncRC( types[count-1] );
		}
	}
	for(i=0; i<count; ++i){
		cur = types[i];
		next = types[(i+1)%count];
		GC_Assign( & cur->quadtype, next );
	}
	for(i=0; i<count; ++i) GC_DecRC( types[i] );
}
DaoType* DaoType_GetBaseType( DaoType *self )
{
	DaoType *quads[4] = {NULL};
	DaoType_GetQuadTypes( self, quads );
	return quads[0];
}
DaoType* DaoType_GetConstType( DaoType *self )
{
	DaoType *base, *konst, *quads[4] = {NULL};

	DaoType_GetQuadTypes( self, quads );
	if( quads[1] ) return quads[1];

	base = quads[0];
	konst = DaoType_Copy( base );
	DString_SetChars( konst->name, "const<" );
	DString_Append( konst->name, base->name );
	DString_AppendChar( konst->name, '>' );

	konst->konst = 1;
	konst->invar = 1;
	DaoType_RelinkQuadTypes( base, konst );
	return konst;
}
DaoType* DaoType_GetInvarType( DaoType *self )
{
	DaoType *base, *invar, *quads[4] = {NULL};

	DaoType_GetQuadTypes( self, quads );
	if( quads[2] ) return quads[2];

	base = quads[0];
	invar = DaoType_Copy( base );
	DString_SetChars( invar->name, "invar<" );
	DString_Append( invar->name, base->name );
	DString_AppendChar( invar->name, '>' );

	invar->invar = 1;
	DaoType_RelinkQuadTypes( base, invar );
	return invar;
}
DaoType* DaoType_GetVarType( DaoType *self )
{
	DaoType *base, *var, *quads[4] = {NULL};

	DaoType_GetQuadTypes( self, quads );
	if( quads[3] ) return quads[3];
#if 0
	// XXX
	if( self->tid != DAO_THT ) return quads[0];  /* base type; */
#endif

	base = quads[0];
	var = DaoType_Copy( base );
	DString_SetChars( var->name, "var<" );
	DString_Append( var->name, base->name );
	DString_AppendChar( var->name, '>' );

	var->var = 1;
	DaoType_RelinkQuadTypes( base, var );
	return var;
}

DaoType* DaoType_GetArgument( DaoType *self, int index, int strip )
{
	DaoType *arg = NULL;

	if( self->args == NULL || self->args->size == 0 ) return NULL;

	if( index < self->args->size ){
		arg = self->args->items.pType[index];
	}else if( self->variadic ){
		arg = self->args->items.pType[self->args->size-1];
	}

	if( arg == NULL ) return NULL;

	if( arg->tid >= DAO_PAR_NAMED && arg->tid <= DAO_PAR_VALIST ){
		arg = (DaoType*) arg->aux;
	}
	return arg;
}

int DaoType_IsImmutable( DaoType *self )
{
	int i;
	switch( self->tid ){
	case DAO_ROUTINE :
	case DAO_TYPE :
		return 1;
	case DAO_OBJECT :
		return (self->aux->xClass.attribs & DAO_CLS_INVAR);
	case DAO_CSTRUCT :
	case DAO_CDATA :
		return (self->aux->xCtype.attribs & DAO_CLS_INVAR);
		break;
	case DAO_VARIANT :
		for(i=0; i<self->args->size; ++i){
			if( DaoType_IsImmutable( self->args->items.pType[i] ) == 0 ) return 0;
		}
		return 1;
	}
	return 0;
}
int DaoType_IsPrimitiveOrImmutable( DaoType *self )
{
	int i;
	if( self->tid <= DAO_ENUM ) return 1;
	if( self->tid == DAO_VARIANT ){
		for(i=0; i<self->args->size; ++i){
			if( DaoType_IsPrimitiveOrImmutable( self->args->items.pType[i] ) == 0 ) return 0;
		}
		return 1;
	}
	return DaoType_IsImmutable( self );
}

enum DaoTypeMatchMode
{
	DAO_MODE_INVAR_SRC  = 1,
	DAO_MODE_INVAR_DEST = 2
};

int DaoType_Match( DaoType *self, DaoType *type, DMap *defs, DMap *binds, int dep, int mode );

static int DaoType_MatchPar( DaoType *self, DaoType *type, DMap *defs, DMap *binds, int host, int dep, int mode )
{
	DaoType *ext1 = self;
	DaoType *ext2 = type;
	int p1 = self->tid == DAO_PAR_NAMED || self->tid == DAO_PAR_DEFAULT;
	int p2 = type->tid == DAO_PAR_NAMED || type->tid == DAO_PAR_DEFAULT;
	int m = 0;
	if( p1 && p2 && type->fname->size && ! DString_EQ( self->fname, type->fname ) ){
		return DAO_MT_NOT;
	}
	if( p1 && p2 && self->tid == DAO_PAR_NAMED && type->tid == DAO_PAR_DEFAULT ) return 0;
	if( p1 || self->tid == DAO_PAR_VALIST ) ext1 = & self->aux->xType;
	if( p2 || type->tid == DAO_PAR_VALIST ) ext2 = & type->aux->xType;
	/* To avoid matching: type to name:var<type> etc. */
	if( (ext1->tid == DAO_PAR_NAMED) != (ext2->tid == DAO_PAR_NAMED) ) return 0;

	m = DaoType_Match( ext1, ext2, defs, binds, dep, mode );
	/*
	   printf( "m = %i:  %s  %s\n", m, ext1->name->chars, ext2->name->chars );
	 */
	if( host == DAO_TUPLE && m >= DAO_MT_EQ ){
		if( self->tid != DAO_PAR_NAMED && type->tid == DAO_PAR_NAMED ) return DAO_MT_SUB;
	}else if( host == DAO_ROUTINE ){
		if( self->tid != DAO_PAR_DEFAULT && type->tid == DAO_PAR_DEFAULT ) return 0;
		return m;
	}
	return m;
}

static int DaoType_MatchTemplateParams( DaoType *self, DaoType *type, DMap *defs, int dep )
{
	DaoType *template1 = self->kernel ? self->kernel->abtype : NULL;
	DaoType *template2 = type->kernel ? type->kernel->abtype : NULL;
	daoint i, k, n, mt = DAO_MT_NOT;
	if( template1 == template2 && template1 && template1->kernel->sptree ){
		DaoType **ts1 = self->args->items.pType;
		DaoType **ts2 = type->args->items.pType;
		if( self->args->size != type->args->size ) return 0;
		mt = DAO_MT_SUB;
		for(i=0,n=self->args->size; i<n; i++){
			int tid = ts2[i]->tid ;
			k = DaoType_Match( ts1[i], ts2[i], defs, NULL, dep+1, 0 );
			/*
			// When matching template types, the template argument types
			// have to be equal, otherwise there will be a typing problem
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
			/* Matching to @T<SomeType> has to be precise: */
			if( k < DAO_MT_EQ && tid == DAO_THT && ts2[i]->aux != NULL ) return DAO_MT_NOT;
		}
	}
	return mt;
}

static int DaoType_MatchToParent( DaoType *self, DaoType *type, DMap *defs, int dep )
{
	daoint i, k, n, mt = DAO_MT_NOT;
	if( self == type ) return DAO_MT_EQ;
	if( self->tid == type->tid && (self->tid >= DAO_OBJECT && self->tid <= DAO_INTERFACE) ){
		if( self->aux == type->aux ) return DAO_MT_EQ; /* for aliased type; */
	}
	if( (mt = DaoType_MatchTemplateParams( self, type, defs, dep )) ) return mt;
	if( self->bases == NULL || self->bases->size == 0 ) return DAO_MT_NOT;
	for(i=0,n=self->bases->size; i<n; i++){
		k = DaoType_MatchToParent( self->bases->items.pType[i], type, defs, dep );
		if( k >= DAO_MT_EQ ){
			return DAO_MT_SUB;
		}else if( k > DAO_MT_SUBX && k <= DAO_MT_SUB ){
			k = k - 1;
		}
		if( k > mt ) mt = k;
	}
	return mt;
}

static int DaoType_MatchToX( DaoType *self, DaoType *type, DMap *defs, DMap *binds, int dep, int mode );

static int DaoValue_MatchToParent( DaoValue *object, DaoType *parent, DMap *defs )
{
	int mt = DAO_MT_NOT;
	if( object == NULL || parent == NULL ) return DAO_MT_NOT;
	if( object->type == DAO_OBJECT ){
		mt = DaoType_MatchToParent( object->xObject.defClass->objType, parent, defs, 0 );
	}else if( object->type == DAO_CSTRUCT || object->type == DAO_CDATA ){
		mt = DaoType_MatchToParent( object->xCstruct.ctype, parent, defs, 0 );
	}else if( object->type == DAO_CTYPE ){
		mt = DaoType_MatchToParent( object->xCtype.classType, parent, defs, 0 );
	}else if( object->type == DAO_CLASS ){
		mt = DaoType_MatchToParent( object->xClass.clsType, parent, defs, 0 );
	}else if( object->type == DAO_CINVALUE ){
		mt = DaoType_MatchToParent( object->xCinValue.cintype->vatype, parent, defs, 0 );
	}else if( object->type == DAO_CINTYPE ){
		mt = DaoType_MatchToParent( object->xCinType.citype, parent, defs, 0 );
	}
	return mt;
}

static int DaoType_MatchToTypeHolder( DaoType *self, DaoType *type, DMap *defs, DMap *binds, int dep, int mode )
{
	int mt = DAO_MT_THT;
	DNode *node = defs ? MAP_Find( defs, type ) : NULL;
	if( node ){
		type = node->value.pType;  /* type associated to the type holder; */
		if( type->tid == DAO_THT || type->tid == DAO_UDT ) return DAO_MT_LOOSE;
		return DaoType_Match( self, type, defs, binds, dep, mode );
	}
	if( type->aux != NULL ){ /* @type_holder<type> */
		mt = DaoType_Match( self, (DaoType*) type->aux, defs, NULL, dep, mode );
		/* Matching to @T<SomeType> has to be precise: */
		if( mt < DAO_MT_EQ && !(mode & DAO_MODE_INVAR_DEST) ) return DAO_MT_NOT;
	}
	if( defs ){
		if( mode & DAO_MODE_INVAR_SRC ) self = DaoType_GetInvarType( self );
		MAP_Insert( defs, type, self );
	}
	return mt;
}

static int DaoType_MatchToVariant( DaoType *self, DaoType *type, DMap *defs, DMap *binds, int dep, int mode )
{
	int i, n, mt = DAO_MT_NOT;
	if( self->tid == DAO_VARIANT ){
		mt = DAO_MT_EQ;
		for(i=0,n=self->args->size; i<n; i++){
			DaoType *it2 = self->args->items.pType[i];
			int mt2 = DaoType_MatchToVariant( it2, type, defs, binds, dep, mode );
			if( mt2 < mt ) mt = mt2;
			if( mt == DAO_MT_NOT ) break;
		}
		if( dep ){
			for(i=0,n=type->args->size; i<n; i++){
				DaoType *it2 = type->args->items.pType[i];
				int mt2 = DaoType_MatchToVariant( it2, self, defs, binds, dep, mode );
				if( mt2 < mt ) mt = mt2;
				if( mt == DAO_MT_NOT ) break;
			}
		}
		return mt;
	}
	for(i=0,n=type->args->size; i<n; i++){
		DaoType *it2 = type->args->items.pType[i];
		int mt2 = DaoType_Match( self, it2, defs, binds, dep, mode );
		if( mt2 > mt ) mt = mt2;
		if( mt >= DAO_MT_EQ ) break;
	}
	return mt;
}

int DaoType_CheckInvarMatch( DaoType *self, DaoType *type, int enforePrimitive )
{
	/*
	// Invar type cannot match to variable type due to potential modification;
	// But const type can, because constant will be copied when it is moved.
	 */
	if( self->konst == 1 || self->invar == 0 || type->invar != 0 ) return 1;
	if( enforePrimitive == 0 && DaoType_IsPrimitiveOrImmutable( self ) ) return 1;
	if( DaoType_IsImmutable( self ) ) return 1;
	return type->tid == DAO_THT;
}

int DaoType_MatchToX( DaoType *self, DaoType *type, DMap *defs, DMap *binds, int dep, int mode )
{
	DaoType *it1, *it2;
	DNode *it, *node = NULL;
	int p1, p2, tid, mt2, mt3, mt = DAO_MT_NOT;
	daoint i, k, n;

	if( self == NULL || type == NULL ) return DAO_MT_NOT;
	if( self == type ) return DAO_MT_EQ;

	if( self->konst && self->tid == type->tid ){
		/* Empty array/list/map need to be handled as const types: */
		switch( self->tid ){
		case DAO_ARRAY : if( self->empty ) return DAO_MT_ANY; break;
		case DAO_LIST  : if( self->empty )  return DAO_MT_ANY; break;
		case DAO_MAP   : if( self->empty )   return DAO_MT_ANY; break;
		}
	}

	/* some types such routine type for overloaded routines rely on comparing type pointer: */
	p1 = self->tid >= DAO_PAR_NAMED && self->tid <= DAO_PAR_VALIST;
	p2 = type->tid >= DAO_PAR_NAMED && type->tid <= DAO_PAR_VALIST;
	if( p1 || p2 ){
		if( p1 == p2 ){
			return DaoType_MatchPar( self, type, defs, binds, 0, dep, mode );
		}else if( p2 ){
			return DAO_MT_NOT;
		}else if( type->tid == DAO_ANY ){
			return DAO_MT_ANY;
		}else if( type->tid == DAO_THT || type->tid == DAO_UDT ){
			return DaoType_MatchToTypeHolder( self, type, defs, binds, dep, mode );
		}else if( type->tid == DAO_VARIANT ){
			return DaoType_MatchToVariant( self, type, defs, binds, dep, mode );
		}else{
			return DAO_MT_NOT;
		}
	}else if( type->invar && type->tid == DAO_THT && type->aux != NULL ){
		/*
		// routine f(invar x: @T<list<int>>){} f({})
		// routine f(invar x: @T<list<int>>){} invar ls: list<int> = {}; f(ls)
		*/
		if( self->invar && self->konst == 0 ) self = DaoType_GetBaseType( self );
		mt = DaoType_Match( self, (DaoType*) type->aux, defs, binds, dep, DAO_MODE_INVAR_DEST );
		if( mt > 0 && defs ) MAP_Insert( defs, type, self );
		return mt;
	}else if( self->invar && type->invar ){
		self = DaoType_GetBaseType( self );
		type = DaoType_GetBaseType( type );
		return DaoType_Match( self, type, defs, binds, dep, DAO_MODE_INVAR_DEST );
	}else if( self->invar || type->invar ){
		if( DaoType_CheckInvarMatch( self, type, 0 ) == 0 ) return 0;

		if( self->invar ){
			self = DaoType_GetBaseType( self );
			mode |= DAO_MODE_INVAR_SRC;
		}else if( type->invar ){
			type = DaoType_GetBaseType( type );
			mode |= DAO_MODE_INVAR_DEST;
		}
		mt = DaoType_Match( self, type, defs, binds, dep, mode );
		return mt;
	}else if( self->tid == DAO_THT && type->tid == DAO_THT ){
		if( defs ){
			node = MAP_Find( defs, self );
			if( node ) self = node->value.pType;
			node = MAP_Find( defs, type );
			if( node ) type = node->value.pType;
		}
		if( self->tid == DAO_THT && type->tid == DAO_THT ){
			if( self->aux != NULL && type->aux != NULL ){
				self = (DaoType*) self->aux;
				type = (DaoType*) type->aux;
				if( self->tid == DAO_VARIANT ){
					mt = DAO_MT_NOT;
					for(i=0,n=self->args->size; i<n; i++){
						DaoType *it2 = self->args->items.pType[i];
						int mt2 = DaoType_MatchToVariant( it2, type, defs, binds, dep, mode );
						if( mt2 > mt ) mt = mt2;
						if( mt >= DAO_MT_EQ ) break;
					}
				}else{
					mt = DaoType_Match( self, type, defs, binds, dep, mode );
				}
				/* Precise matching to @T<dest_type> is require: */
				if( mt < DAO_MT_EQ ) return DAO_MT_NOT;
				return mt;
			}
			return DAO_MT_THTX;
		}
		return DaoType_MatchToTypeHolder( self, type, defs, binds, dep, mode );
	}


	mt = dao_type_matrix[self->tid][type->tid];
	/*
	printf( "here: %i  %i  %i, %s  %s,  %p\n", mt, self->tid, type->tid,
			self->name->chars, type->name->chars, defs );
	 */
	if( mt == DAO_MT_THT || mt == DAO_MT_THTX ){
		if( self->tid == DAO_THT || self->tid == DAO_UDT ){
			if( defs ) node = MAP_Find( defs, self );
			if( node ) self = node->value.pType;
		}
		if( type->tid == DAO_THT || type->tid == DAO_UDT ){
			return DaoType_MatchToTypeHolder( self, type, defs, binds, dep, mode );
		}
	}else if( self->tid == DAO_VARIANT ){
		/* Handle variant first in case it is matching to an interface; */
		mt = DAO_MT_EQ;
		mt3 = DAO_MT_NOT;
		for(i=0; i<self->args->size; ++i){
			it1 = self->args->items.pType[i];
			mt2 = DaoType_Match( it1, type, defs, binds, dep, mode );
			if( mt2 < mt ) mt = mt2;
			if( mt2 > mt3 ) mt3 = mt2;
		}
		if( mt == 0 ) return mt3 ? DAO_MT_ANYX : DAO_MT_NOT;
		return mt;
	}else if( type->tid == DAO_CINVALUE ){
		mt = DaoType_MatchTo( self, type->aux->xCinType.target, NULL );
		if( mt >= DAO_MT_CIV ) return DAO_MT_SIM;
		type = type->aux->xCinType.target;
	}else if( type->tid == DAO_INTERFACE ){
		/* Matching to "interface": */
		if( type->aux == NULL ) return DAO_MT_SUBX * (self->tid == DAO_INTERFACE);
		if( DaoInterface_GetConcrete( (DaoInterface*) type->aux, self ) ) return DAO_MT_SIM;
		return DaoType_MatchInterface( self, (DaoInterface*) type->aux, binds );
	}else if( type->tid == DAO_VARIANT ){
		return DaoType_MatchToVariant( self, type, defs, binds, dep, mode );
	}

	mt = dao_type_matrix[self->tid][type->tid];
	if( mt == DAO_MT_EQ ){
		if( self->valtype && type->valtype ){
			if( DaoValue_Compare( self->value, type->value ) == 0 ) return DAO_MT_EXACT;
			return DAO_MT_NOT;
		}else if( type->valtype ){
			return DaoType_MatchValue( self, type->value, defs );
		}else if( self->valtype ){
			mt = DaoType_MatchValue( type, self->value, defs );
			if( mt && type->tid == DAO_THT ){
				if( defs ) MAP_Insert( defs, type, self );
			}
			return mt;
		}
	}else if( self->subtid == DAO_ENUM_SYM && type->realnum ){
		return DAO_MT_NOT;
	}
	if( mt <= DAO_MT_EXACT ) return mt;

	/* some types such routine type for verloaded routines rely on comparing type pointer: */
	if( self->var ) self = DaoType_GetBaseType( self );
	if( type->var ) type = DaoType_GetBaseType( type );

	mt = DAO_MT_EQ;
	switch( self->tid ){
	case DAO_ENUM :
		if( self == type ) return DAO_MT_EQ;
		if( type->subtid == DAO_ENUM_ANY ) return DAO_MT_SIM;
		if( self->subtid != type->subtid && self->subtid != DAO_ENUM_SYM ) return 0;
		if( self->subtid == DAO_ENUM_SYM ){
			it = DMap_First(self->mapNames);
			node = DMap_Find( type->mapNames, it->key.pVoid );
			if( node == NULL ) return DAO_MT_NOT;
			if( type->subtid == DAO_ENUM_SYM ){ /* Alias: */
				return DAO_MT_EQ;
			}else{ /* Some enum type containing this symbol: */
				return DAO_MT_SUB;
			}
		}
		return DString_EQ( self->fname, type->fname ) ? DAO_MT_EQ : DAO_MT_NOT;
	case DAO_ARRAY : case DAO_LIST : case DAO_MAP :
	case DAO_TYPE :
		switch( self->tid ){
		case DAO_ARRAY : if( self->empty ) return DAO_MT_ANY; break;
		case DAO_LIST  : if( self->empty )  return DAO_MT_ANY; break;
		case DAO_MAP   : if( self->empty )   return DAO_MT_ANY; break;
		}
		if( type->args == NULL || type->args->size == 0 ) return DAO_MT_SUBX;
		if( self->args == NULL || self->args->size == 0 ) return DAO_MT_LOOSE;
		if( self->args->size != type->args->size ) return DAO_MT_NOT;
		for(i=0,n=self->args->size; i<n; i++){
			int ndefs = defs ? defs->size : 0;
			it1 = self->args->items.pType[i];
			it2 = type->args->items.pType[i];
			tid = it2->tid;
			k = DaoType_Match( it1, it2, defs, binds, dep+1, mode );
			/* printf( "%i %i %s %s\n", k, mode, it1->name->chars, it2->name->chars ); */
			if( defs && defs->size && defs->size == ndefs ){
				/*
				// No unassociated type holders involved in the matching,
				// so the matching has to be exact.
				*/
				if( k < mt ) mt = k;
			}else if( tid == DAO_THT && it2->aux != NULL ){
				/* Type matching to @T<Type> has to be precise: */
				if( k < DAO_MT_EQ && !(mode & DAO_MODE_INVAR_DEST) ) return DAO_MT_NOT;
				if( k < mt ) mt = k;
			}else if( tid == DAO_THT || tid == DAO_UDT ){
				/*
				// Target type is an unassociated type holder,
				// the matching is not exact, but allowed.
				*/
				continue;
			}else if( (it1->tid == DAO_THT || it1->tid == DAO_UDT) && tid == DAO_ANY ){
				/*
				// Unassociated type holders can represent any types,
				// so matching to "any" should be allowed.
				*/
				if( defs == NULL || DMap_Find( defs, it1 ) == NULL ){
					continue;
				}else if( k < mt ){
					mt = k;
				}
			}else{
				if( k < mt ) mt = k;
			}
			if( k < DAO_MT_EQ && !(mode & DAO_MODE_INVAR_DEST) ) return DAO_MT_NOT;
		}
		break;
	case DAO_TUPLE :
		if( type->args == NULL || type->args->size == 0 ) return DAO_MT_SUBX;
		if( self->args == NULL || self->args->size == 0 ) return DAO_MT_LOOSE;
		/* Source tuple type must contain at least as many item as the target tuple: */
		if( (self->args->size - self->variadic) < (type->args->size - type->variadic) ){
			return DAO_MT_NOT;
		}
		if( self->args->size > type->args->size && type->variadic == 0 ) return DAO_MT_NOT;
		/* Compare non-variadic part of the tuple: */
		for(i=0,n=type->args->size-(type->variadic!=0); i<n; i++){
			it1 = self->args->items.pType[i];
			it2 = type->args->items.pType[i];
			k = DaoType_MatchPar( it1, it2, defs, binds, type->tid, dep+1, mode );
			if( k > DAO_MT_SIM && it1->tid != it2->tid ) k = DAO_MT_SIM; /*name:type to type;*/
			/* printf( "%i %s %s\n", k, it1->name->chars, it2->name->chars ); */
			if( k == DAO_MT_NOT ) return k;
			if( k < mt ) mt = k;
		}
		/* Compare variadic part of the tuple: */
		it2 = type->args->items.pType[type->args->size-1];
		if( it2->tid == DAO_PAR_VALIST ) it2 = (DaoType*) it2->aux;
		for(i=type->args->size-(type->variadic!=0),n=self->args->size-(self->variadic!=0); i<n; ++i){
			it1 = self->args->items.pType[i];
			k = DaoType_MatchPar( it1, it2, defs, binds, type->tid, dep+1, mode );
			if( k > DAO_MT_SIM && it1->tid != it2->tid ) k = DAO_MT_SIM; /*name:type to type;*/
			/* printf( "%i %s %s\n", k, it1->name->chars, it2->name->chars ); */
			if( k == DAO_MT_NOT ) return k;
			if( k < mt ) mt = k;
		}
		break;
	case DAO_ROUTINE :
		if( self->subtid == DAO_ROUTINES ){
			if( type->subtid == DAO_ROUTINES ){
				return DAO_MT_EQ * (self == type);
			}else{
				DaoRoutine *rout;
				DaoType **tps = type->args->items.pType;
				DList *routines = self->aux->xRoutine.overloads->routines;
				int np = type->args->size;
				for(i=0,n=routines->size; i<n; i++){
					if( routines->items.pRoutine[i]->routType == type ) return DAO_MT_EQ;
				}
				rout = DaoRoutine_Resolve( (DaoRoutine*)self->aux, NULL, NULL, NULL, tps, np, DVM_CALL );
				if( rout == NULL ) return DAO_MT_NOT;
				return DaoType_Match( rout->routType, type, defs, NULL, dep+1, mode );
			}
		}
		if( type->subtid == DAO_ROUTINES ) return 0;
		if( type->aux == NULL ){
			int hascb1 = self->cbtype != NULL;
			int hascb2 = type->cbtype != NULL;
			/* match to "routine" or "routine[...]"; */
			return DAO_MT_SUBX * (hascb1 == hascb2);
		}
		if( self->args->size < type->args->size ) return DAO_MT_NOT;
		if( (self->cbtype == NULL) != (type->cbtype == NULL) ) return 0;
		if( self->aux == NULL && type->aux ) return 0;
		if( self->cbtype && DaoType_Match( self->cbtype, type->cbtype, defs, NULL, dep+1, mode ) ==0 ) return 0;
		/* self may have extra parameters, but they must have default values: */
		for(i=type->args->size,n=self->args->size; i<n; i++){
			it1 = self->args->items.pType[i];
			if( it1->tid != DAO_PAR_DEFAULT ) return 0;
		}
		for(i=0,n=type->args->size; i<n; i++){
			it1 = self->args->items.pType[i];
			it2 = type->args->items.pType[i];
			k = DaoType_MatchPar( it1, it2, defs, binds, DAO_ROUTINE, dep+1, mode );
			/*
			   printf( "%2i  %2i:  %s  %s\n", i, k, it1->name->chars, it2->name->chars );
			 */
			if( k == DAO_MT_NOT ) return k;
			if( k < mt ) mt = k;
		}
		if( self->aux && type->aux ){
			k = DaoType_Match( & self->aux->xType, & type->aux->xType, defs, binds, dep+1, mode );
			if( k < mt ) mt = k;
		}
		break;
	case DAO_CLASS :
	case DAO_OBJECT :
		/* par : class */
		if( type->aux == NULL && self->tid == DAO_CLASS ) return DAO_MT_SUBX;
		if( self->aux == type->aux ) return DAO_MT_EQ;
		return DaoType_MatchToParent( self, type, defs, dep );
	case DAO_CTYPE :
	case DAO_CDATA :
	case DAO_CSTRUCT :
		if( self->aux == type->aux ) return DAO_MT_EQ; /* for aliased type; */
		return DaoType_MatchToParent( self, type, defs, dep );
	case DAO_CINVALUE :
		if( self == type ) return DAO_MT_EQ;
		if( self->aux->xCinType.target == type ) return DAO_MT_SIM;
		if( type->tid == DAO_INTERFACE ){
			DaoInterface *inter = (DaoInterface*) type->aux;
			if( self->aux->xCinType.abstract == inter ) return DAO_MT_EQ;
			return DaoType_MatchInterface( self, inter, NULL );
		}
		if( DaoType_MatchTo( self->aux->xCinType.target, type, NULL ) >= DAO_MT_CIV ){
			return DAO_MT_SIM;
		}
		return DaoType_MatchToParent( self, type, defs, dep );
	default : break;
	}
	if( mt > DAO_MT_EXACT ) mt = DAO_MT_NOT;
	return mt;
}

int DaoType_Match( DaoType *self, DaoType *type, DMap *defs, DMap *binds, int dep, int mode )
{
	DMap *binds2 = binds;
	void *pvoid[2];
	int mt;

	pvoid[0] = self;
	pvoid[1] = type;

	if( self == NULL || type == NULL ) return DAO_MT_NOT;
	if( self == type ) return DAO_MT_EQ;

	if( self->recursive && type->recursive && binds == NULL ){
		binds = DHash_New( DAO_DATA_VOID2, 0 );
	}
	if( self->recursive && type->recursive ){
		DNode *it = DMap_Find( binds, pvoid );
		if( it ){
			return DAO_MT_EQ;
		}else{
			DMap_Insert( binds, pvoid, NULL );
		}
	}
	mt = DaoType_MatchToX( self, type, defs, binds, dep, mode );
	if( binds2 != binds ){
		DMap_Delete( binds );
		binds = NULL;
	}
#if 0
	printf( "mt = %i %s %s\n", mt, self->name->chars, type->name->chars );
	if( mt ==0 && binds ){
		printf( "%p  %p\n", pvoid[0], pvoid[1] );
		printf( "%i %p\n", binds->size, DMap_Find( binds, pvoid ) );
	}
#endif
	if( mt ==0 && binds && DMap_Find( binds, pvoid ) ) return DAO_MT_THT;
	return mt;
}

int DaoType_MatchTo( DaoType *self, DaoType *type, DMap *defs )
{
	return DaoType_Match( self, type, defs, NULL, 0, 0 );
}

int DaoType_MatchValueX( DaoType *self, DaoValue *value, DMap *defs, int mode )
{
	DaoInterface *dinterface;
	DaoVmSpace *vms;
	DaoType *tp;
	DaoEnum *other;
	DNode *node;
	DMap *names;
	daoint i, n, mt, mt2, it1 = 0, it2 = 0;

	if( (self == NULL) | (value == NULL) ) return DAO_MT_NOT;

	if( self->valtype ){
		if( DaoValue_Compare( self->value, value ) ==0 ) return DAO_MT_EXACT;
		return DAO_MT_NOT;
	}

	switch( (self->tid << 8) | value->type ){
	case (DAO_NONE << 8)    | DAO_NONE : return DAO_MT_EXACT;
	case (DAO_BOOLEAN << 8) | DAO_BOOLEAN : return DAO_MT_EQ;
	case (DAO_INTEGER << 8) | DAO_INTEGER : return DAO_MT_EQ;
	case (DAO_FLOAT   << 8) | DAO_FLOAT   : return DAO_MT_EQ;
	case (DAO_COMPLEX << 8) | DAO_COMPLEX : return DAO_MT_EQ;
	case (DAO_STRING  << 8) | DAO_STRING  : return DAO_MT_EQ;
	case (DAO_BOOLEAN << 8) | DAO_INTEGER : return DAO_MT_SIM;
	case (DAO_BOOLEAN << 8) | DAO_FLOAT   : return DAO_MT_SIM;
	case (DAO_INTEGER << 8) | DAO_BOOLEAN : return DAO_MT_SIM;
	case (DAO_INTEGER << 8) | DAO_FLOAT   : return DAO_MT_SIM;
	case (DAO_FLOAT   << 8) | DAO_BOOLEAN : return DAO_MT_SIM;
	case (DAO_FLOAT   << 8) | DAO_INTEGER : return DAO_MT_SIM;
	}

	/* some types such routine type for verloaded routines rely on comparing type pointer: */
	if( self->var || self->invar ){
		if( self->invar ) mode = DAO_MODE_INVAR_DEST;
		self = DaoType_GetBaseType( self );
	}

	switch( self->tid ){
	case DAO_UDT :
	case DAO_THT :
		if( defs ){
			node = MAP_Find( defs, self );
			if( node ) return DaoType_MatchValueX( node->value.pType, value, defs, mode );
		}else if( self->tid == DAO_THT && self->aux != NULL ){
			mt = DaoType_MatchValueX( (DaoType*) self->aux, value, defs, mode );
			/* Type matching to @T<Type> has to be precise: */
			return mt >= DAO_MT_EQ ? mt : DAO_MT_NOT;
		}
		return DAO_MT_THT;
	case DAO_VARIANT :
		mt = DAO_MT_NOT;
		for(i=0,n=self->args->size; i<n; i++){
			tp = self->args->items.pType[i];
			mt2 = DaoType_MatchValueX( tp, value, defs, mode );
			if( mt2 > mt ) mt = mt2;
			if( mt >= DAO_MT_EQ ) break;
		}
		return mt;
	case DAO_CINVALUE :
		mt = DaoType_MatchValue( self->aux->xCinType.target, value, NULL );
		if( mt >= DAO_MT_CIV ) return DAO_MT_SIM;
		break;
	case DAO_INTERFACE :
		/* Matching to "interface": */
		if( self->aux == NULL ) return DAO_MT_SUBX * (value->type == DAO_INTERFACE);
		if( self->aux->xInterface.concretes != NULL ){
			DaoInterface *inter = (DaoInterface*) self->aux;
			DNode *it;
			for(it=DMap_First(inter->concretes); it; it=DMap_Next(inter->concretes,it)){
				if( DaoType_MatchValue( it->key.pType, value, NULL ) >= DAO_MT_CIV ){
					return DAO_MT_SIM;
				}   
			}   
		}
		break;
	case DAO_ANY : return DAO_MT_ANY;
	}
	mt = dao_type_matrix[value->type][self->tid];
	if( mt <= DAO_MT_EXACT ) return mt;

	dinterface = self->tid == DAO_INTERFACE ? (DaoInterface*) self->aux : NULL;
	switch( value->type ){
	case DAO_STRING :
		if( dinterface ){
			DaoVmSpace *vms = dinterface->nameSpace->vmSpace;
			return DaoType_MatchInterface( vms->typeString, dinterface, NULL );
		}
		break;
	case DAO_ENUM :
		other = & value->xEnum;
		if( self == value->xEnum.etype ) return DAO_MT_EQ;
		if( self->subtid == DAO_ENUM_ANY ) return DAO_MT_SUBX;
		if( self->realnum && value->xEnum.subtype == DAO_ENUM_SYM ) return DAO_MT_NOT;
		if( dinterface ) return DaoType_MatchInterface( value->xEnum.etype, dinterface, NULL );
		if( self->tid != value->type ) return DAO_MT_NOT;
		if( self->subtid != other->subtype && other->subtype != DAO_ENUM_SYM ) return 0;
		names = other->etype->mapNames;
		for(node=DMap_First(names); node; node=DMap_Next(names,node)){
			if( other->subtype == DAO_ENUM_FLAG ){
				if( !(node->value.pInt & other->value) ) continue;
			}else if( node->value.pInt != other->value ){
				continue;
			}
			if( DMap_Find( self->mapNames, node->key.pVoid ) == NULL ) return 0;
		}
		return DAO_MT_SUB;
	case DAO_ARRAY :
		if( value->xArray.size == 0 ) return DAO_MT_ANY;
		vms = DaoType_GetVmSpace( self );
		tp = vms->typeArrays[ value->xArray.etype ];
		if( tp == self ) return DAO_MT_EQ;
		if( self->args == NULL || self->args->size == 0 ) return DAO_MT_SUBX;
		if( self->tid != value->type ) return DAO_MT_NOT;
		return DaoType_Match( tp, self, defs, NULL, 0, mode );
	case DAO_LIST :
		tp = value->xList.ctype;
		if( tp == self ) return DAO_MT_EQ;
		if( self->args == NULL || self->args->size == 0 ) return DAO_MT_SUBX;
		if( dinterface ) return DaoType_MatchInterface( tp, dinterface, NULL );
		if( self->tid != value->type ) return DAO_MT_NOT;
		if( tp == NULL || tp->empty )
			return value->xList.value->size == 0 ? DAO_MT_EMPTY : DAO_MT_NOT;
		return DaoType_Match( tp, self, defs, NULL, 0, mode );
	case DAO_MAP :
		tp = value->xMap.ctype;
		if( tp == self ) return DAO_MT_EQ;
		if( self->args == NULL || self->args->size == 0 ) return DAO_MT_SUBX;
		if( dinterface ) return DaoType_MatchInterface( tp, dinterface, NULL );
		if( self->tid != value->type ) return DAO_MT_NOT;
		if( tp == NULL || tp->empty )
			return value->xMap.value->size == 0 ? DAO_MT_EMPTY : DAO_MT_NOT;
		return DaoType_Match( tp, self, defs, NULL, 0, mode );
	case DAO_TUPLE :
		tp = value->xTuple.ctype;
		if( tp == self ) return DAO_MT_EQ;
		if( self->args == NULL || self->args->size == 0 ) return DAO_MT_SUBX;
		if( dinterface ) return DaoType_MatchInterface( tp, dinterface, NULL );
		if( self->tid != value->type ) return DAO_MT_NOT;
		if( value->xTuple.size < (self->args->size - self->variadic) ) return DAO_MT_NOT;
		if( value->xTuple.size > self->args->size && self->variadic ==0 ) return DAO_MT_NOT;

		mt = DAO_MT_EQ;
		for(i=0,n=self->args->size-(self->variadic!=0); i<n; i++){
			tp = self->args->items.pType[i];
			if( tp->tid == DAO_PAR_NAMED ) tp = & tp->aux->xType;

			/*
			// for C function that returns a tuple:
			// the tuple may be assigned to a context value before
			// its values are set properly!
			*/
			if( value->xTuple.values[i] == NULL ) continue;
			if( tp->tid == DAO_UDT || tp->tid == DAO_ANY ) continue;

			mt2 = DaoType_MatchValue( tp, value->xTuple.values[i], defs );
			/* Type matching to @T<Type> has to be precise: */
			if( tp->tid == DAO_THT && tp->aux != NULL && mt2 < DAO_MT_EQ ) return DAO_MT_NOT;
			if( mt2 < DAO_MT_SIM ) return DAO_MT_NOT;
			if( mt2 < mt ) mt = mt2;
		}
		tp = self->args->items.pType[self->args->size-1];
		if( tp->tid == DAO_PAR_VALIST ) tp = (DaoType*) tp->aux;
		for(i=self->args->size-(self->variadic!=0),n=value->xTuple.size; i<n; ++i){
			if( value->xTuple.values[i] == NULL ) continue;
			if( tp->tid == DAO_UDT || tp->tid == DAO_ANY ) continue;

			mt2 = DaoType_MatchValue( tp, value->xTuple.values[i], defs );
			/* Type matching to @T<Type> has to be precise: */
			if( tp->tid == DAO_THT && tp->aux != NULL && mt2 < DAO_MT_EQ ) return DAO_MT_NOT;
			if( mt2 < DAO_MT_SIM ) return DAO_MT_NOT;
			if( mt2 < mt ) mt = mt2;
		}
		if( value->xTuple.ctype == NULL ) return DAO_MT_EQ;
		names = self->mapNames;
		tp = value->xTuple.ctype;
		for(node=DMap_First(names); node; node=DMap_Next(names,node)){
			DNode *search = DMap_Find( tp->mapNames, node->key.pVoid );
			if( search == NULL || search->value.pInt != node->value.pInt ) return DAO_MT_SIM;
		}
		return mt;
	case DAO_ROUTINE :
		if( self->tid != value->type ) return DAO_MT_NOT;
		if( value->xRoutine.overloads ){
			if( self->subtid == DAO_ROUTINES ){
				return DAO_MT_EQ * (self == value->xRoutine.routType);
			}else{
				DList *routines = value->xRoutine.overloads->routines;
				int max = 0;
				/*
				// Do not use DaoRoutine_ResolveByType( value, ... )
				// "value" should match to "self", not the other way around!
				*/
				for(i=0,n=routines->size; i<n; i++){
					DaoRoutine *rout = routines->items.pRoutine[i];
					if( rout->routType == self ) return DAO_MT_EQ;
					mt = DaoType_MatchTo( rout->routType, self, defs );
					if( mt > max ) max = mt;
				}
				return max;
			}
		}
		tp = value->xRoutine.routType;
		if( tp == self ) return DAO_MT_EQ;
		if( tp ) return DaoType_MatchTo( tp, self, NULL );
		break;
	case DAO_CLASS :
		if( self->aux == NULL ) return DAO_MT_SUBX; /* par : class */
		if( self->aux == value ) return DAO_MT_EQ;
		if( dinterface ) return DaoType_MatchInterface( value->xClass.clsType, dinterface, NULL );
		return DaoValue_MatchToParent( value, self, defs );
	case DAO_OBJECT :
		if( self->aux == (DaoValue*) value->xObject.defClass ) return DAO_MT_EQ;
		if( dinterface ) return DaoType_MatchInterface( value->xObject.defClass->objType, dinterface, NULL );
		return DaoValue_MatchToParent( value, self, defs );
	case DAO_CTYPE :
		tp = value->xCtype.classType;
		if( self == tp ) return DAO_MT_EQ;
		if( self->tid == value->type && self->aux == tp->aux ) return DAO_MT_EQ; /* alias type */
		if( dinterface ) return DaoType_MatchInterface( tp, dinterface, NULL );
		return DaoValue_MatchToParent( value, self, defs );
	case DAO_CDATA :
	case DAO_CSTRUCT :
		tp = value->xCstruct.ctype;
		if( self == tp ) return DAO_MT_EQ;
		if( self->tid == value->type && self->aux == tp->aux ) return DAO_MT_EQ; /* alias type */
		if( dinterface ) return DaoType_MatchInterface( tp, dinterface, NULL );
		return DaoValue_MatchToParent( value, self, defs );
	case DAO_CINVALUE :
		if( value->xCinValue.cintype->vatype == self ) return DAO_MT_EQ;
		if( value->xCinValue.cintype->target == self ) return DAO_MT_SIM;
		if( value->xCinValue.cintype->abstract == dinterface ) return DAO_MT_EQ;
		if( dinterface == NULL ){
			if( DaoType_MatchValue( self, value->xCinValue.value, NULL ) >= DAO_MT_EQ ){
				return DAO_MT_SIM;
			}
			return DaoValue_MatchToParent( value, self, defs );
		}
		return DaoType_MatchInterface( value->xCinValue.cintype->vatype, dinterface, NULL );
	case DAO_TYPE :
		tp = & value->xType;
		if( self->tid != DAO_TYPE ) return 0;
		/* generic "type"; */
		if( self->args == NULL || self->args->size == 0 ) return DAO_MT_SUBX;
		mt = DaoType_MatchTo( tp, self->args->items.pType[0], defs );
		if( mt >= DAO_MT_EQ ) return mt;
		return 0;
	case DAO_PAR_NAMED :
	case DAO_PAR_DEFAULT :
		return DaoType_MatchTo( value->xNameValue.ctype, self, defs );
	}
	return (self->tid == value->type) ? DAO_MT_EQ : DAO_MT_NOT;
}

int DaoType_MatchValue( DaoType *self, DaoValue *value, DMap *defs )
{
	return DaoType_MatchValueX( self, value, defs, 0 );
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
	return DaoType_MatchToParent( self, other, NULL, 0 );
}

DaoValue* DaoType_CastToParent( DaoValue *object, DaoType *parent )
{
	daoint i;
	DaoValue *value;
	if( object == NULL || parent == NULL ) return NULL;
	if( parent->var || parent->invar ) parent = DaoType_GetBaseType( parent );
	if( object->type == DAO_CSTRUCT || object->type == DAO_CDATA ){
		if( DaoType_MatchToParent( object->xCstruct.ctype, parent, NULL, 0 ) ) return object;
	}else if( object->type == DAO_CTYPE ){
		if( DaoType_MatchToParent( object->xCtype.classType, parent, NULL, 0 ) ) return object;
	}else if( object->type == DAO_OBJECT ){
		if( object->xObject.defClass->objType == parent ) return object;
		if( object->xObject.parent ){
			value = DaoType_CastToParent( object->xObject.parent, parent );
			if( value ) return value;
		}
	}else if( object->type == DAO_CLASS ){
		if( object->xClass.clsType == parent ) return object;
		if( object->xClass.parent ){
			value = DaoType_CastToParent( object->xClass.parent, parent );
			if( value ) return value;
		}
	}else if( object->type == DAO_INTERFACE ){
		if( object->xInterface.abtype == parent ) return object;
		for(i=0; i<object->xInterface.bases->size; ++i){
			value = DaoType_CastToParent( object->xInterface.bases->items.pValue[i], parent );
			if( value ) return value;
		}
	}else if( object->type == DAO_CINTYPE ){
		if( object->xCinType.citype == parent ) return object;
		for(i=0; i<object->xCinType.bases->size; ++i){
			value = DaoType_CastToParent( object->xCinType.bases->items.pValue[i], parent );
			if( value ) return value;
		}
	}else if( object->type == DAO_CINVALUE ){
		DaoCinType *cintype = object->xCinValue.cintype;
		if( DaoType_MatchToParent( cintype->vatype, parent, NULL, 0 ) ) return object;
		return NULL;
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
	DList *keys = DList_New(0);
	DNode *node;
	daoint i, n;
	DMap_Erase( defs, p );
	for(node=DMap_First(defs); node; node=DMap_Next(defs,node)){
		if( node->value.pVoid == p ) DList_Append( keys, node->key.pVoid );
	}
	for(i=0,n=keys->size; i<n; i++) DMap_Erase( defs, keys->items.pVoid[i] );
	DList_Delete( keys );
}

static int DaoType_CheckTypeMapping( DaoType *self, DMap *defs, DMap *chk )
{
	daoint i, n;

	if( DMap_Find( chk, self ) ) return 0;
	DMap_Insert( chk, self, NULL );

	if( DMap_Find( defs, self ) != NULL ) return 1;

	if( self->args ){
		for(i=0,n=self->args->size; i<n; i++){
			if( DaoType_CheckTypeMapping( self->args->items.pType[i], defs, chk ) ) return 1;
		}
	}
	if( self->bases ){
		for(i=0,n=self->bases->size; i<n; i++){
			if( DaoType_CheckTypeMapping( self->bases->items.pType[i], defs, chk ) ) return 1;
		}
	}
	if( self->aux && self->aux->type == DAO_TYPE ){
		if( DaoType_CheckTypeMapping( & self->aux->xType, defs, chk ) ) return 1;
	}
	return 0;
}

DaoType* DaoType_DefineTypes( DaoType *self, DaoNamespace *ns, DMap *defs )
{
	DaoType *type, *copy = NULL;
	DNode *node;
	DMap *chk;
	daoint i, n;

	if( self == NULL ) return NULL;
	/*
	// Specialization of recursive types is problematic.
	// Apparently specialization of their names is most problematic.
	// For example, for a recusive type defined as:
	// type RecursiveTuple = tuple<@T,none|RecursiveTuple>
	// What should be the name for a specialization with @T=int?
	//
	// So only allow explicit definition of recursive type using
	// the type aliasing syntax.
	*/
	if( self->recursive ) return self;

	chk = DHash_New(0,0);
	n = DaoType_CheckTypeMapping( self, defs, chk );
	DMap_Delete( chk );
	if( n == 0 && !(self->attrib & DAO_TYPE_SPEC) ) return self;

	node = MAP_Find( defs, self );
	if( node ){
		if( node->value.pType == self ) return self;
		return DaoType_DefineTypes( node->value.pType, ns, defs );
	}else if( self->invar ){
		copy = DaoType_DefineTypes( DaoType_GetBaseType( self ), ns, defs );
		copy = self->konst ? DaoType_GetConstType( copy ) : DaoType_GetInvarType( copy );;
		DMap_Insert( defs, self, copy );
		return copy;
	}else if( self->var ){
		copy = DaoType_DefineTypes( DaoType_GetBaseType( self ), ns, defs );
		copy = DaoType_GetVarType( copy );
		DMap_Insert( defs, self, copy );
		return copy;
	}else if( self->tid == DAO_TUPLE && self->subtid == DAO_RANGE ){
		DaoType *t1 = DaoType_DefineTypes( self->args->items.pType[0], ns, defs );
		DaoType *t2 = DaoType_DefineTypes( self->args->items.pType[2], ns, defs );
		return DaoNamespace_MakeRangeType( ns, t1, t2 );
	}else if( self->tid & DAO_ANY ){
		return self;
	}else if( self->tid == DAO_CLASS ){ /* e.g., class<Item<@T>> */
		return self;
	}else if( self->tid == DAO_OBJECT ){
		return self;
	}

	copy = DaoType_New( self->nameSpace, "", self->tid, NULL, NULL );
	GC_Assign( & copy->kernel, self->kernel );
	copy->core = self->core;
	copy->subtid = self->subtid;
	copy->attrib = self->attrib;
	copy->konst = self->konst;
	copy->invar = self->invar;
	copy->var = self->var;
	copy->trait |= DAO_VALUE_DELAYGC;
	DString_Reserve( copy->name, 128 );
	DList_Append( ns->auxData, copy );
	DMap_Insert( defs, self, copy );
	if( self->mapNames ){
		if( copy->mapNames ) DMap_Delete( copy->mapNames );
		copy->mapNames = DMap_Copy( self->mapNames );
	}
	if( self->fname ){
		if( copy->fname == NULL ) copy->fname = DString_New();
		DString_Assign( copy->fname, self->fname );
	}
	if( self->args ){
		int m = DString_Match( self->name, "^ %@? %w+ %< ", NULL, NULL );
		char sep = self->tid == DAO_VARIANT ? '|' : ',';
		if( copy->args == NULL ) copy->args = DList_New( DAO_DATA_VALUE );
		if( self->tid == DAO_CODEBLOCK ){
			DString_AppendChar( copy->name, '[' );
		}else if( self->tid != DAO_VARIANT && m ){
			DString_AppendChar( copy->name, self->name->chars[0] ); /* @routine<> */
			for(i=1,n=self->name->size; i<n; i++){
				char ch = self->name->chars[i];
				if( ch != '_' && !isalnum( ch ) ) break;
				DString_AppendChar( copy->name, self->name->chars[i] );
			}
			DString_AppendChar( copy->name, '<' );
		}else if( self->tid <= DAO_TUPLE && self->args->size ){
			DString_AppendChars( copy->name, coreTypeNames[self->tid] );
			DString_AppendChar( copy->name, '<' );
			m = 1;
		}
		for(i=0,n=self->args->size; i<n; i++){
			type = DaoType_DefineTypes( self->args->items.pType[i], ns, defs );
			if( type ==NULL ) goto DefFailed;
			DList_Append( copy->args, type );
			DString_Append( copy->name, type->name );
			if( i+1 < (int)self->args->size ) DString_AppendChar( copy->name, sep );
		}
		if( self->tid == DAO_ARRAY || self->tid == DAO_LIST || self->tid == DAO_MAP ){
			DList *args = copy->args;
			DaoType *gentype = NULL;
			DaoType *sptype = NULL;

			/*
			// The self type might use arbitrary type holders as its arguments.
			// Use the original generic types to ensure proper specialize the defined type.
			*/
			switch( self->tid ){
			case DAO_ARRAY : gentype = ns->vmSpace->typeArray; break;
			case DAO_LIST  : gentype = ns->vmSpace->typeList; break;
			case DAO_MAP   : gentype = ns->vmSpace->typeMap; break;
			}
			sptype = DaoType_Specialize( gentype, args->items.pType, args->size, ns );
			if( sptype ){
				DMap_Erase2( defs, copy );
				return sptype;
			}
		}else if( self->tid >= DAO_CSTRUCT && self->tid <= DAO_CTYPE ){
			DaoType *sptype = self->kernel->abtype;
			DList *args = copy->args;
			if( self->tid == DAO_CTYPE ) sptype = sptype->aux->xCtype.classType;
			if( sptype->kernel->sptree ){
				sptype = DaoType_Specialize( sptype, args->items.pType, args->size, ns );
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
				DString_AppendChars( copy->name, "=>" );
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
		copy->bases = DList_New( DAO_DATA_VALUE );
		for(i=0,n=self->bases->size; i<n; i++){
			type = DaoType_DefineTypes( self->bases->items.pType[i], ns, defs );
			DList_Append( copy->bases, type );
		}
	}
	if( copy->aux == NULL && self->aux != NULL ){
		copy->aux = self->aux;
		if( self->aux->type == DAO_TYPE ){
			copy->aux = (DaoValue*) DaoType_DefineTypes( & self->aux->xType, ns, defs );
			if( copy->aux ==NULL ) goto DefFailed;
		}
		GC_IncRC( copy->aux );
	}
	/*
	// Note: DO NOT handle ::quadtype here,
	// which should be handled by DaoType_GetInvarType() etc.;
	*/
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
	}else if( self->args == NULL ){
		DString_Assign( copy->name, self->name );
	}

	DaoType_CheckAttributes( copy );
	type = DaoNamespace_FindType( ns, copy->name );
#if 0
	if( strstr( copy->name->chars, "map<" ) == copy->name->chars ){
		printf( "%s  %p  %p\n", copy->name->chars, copy, node );
		printf( "%s  %s  %p  %p\n", self->name->chars, copy->name->chars, copy, node );
		print_trace();
	}
#endif
	if( type ){
		DMap_Erase2( defs, copy );
		return type;
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

void DaoType_GetTypeHolders( DaoType *self, DMap *types )
{
	daoint i, n;
	if( self->tid == DAO_THT ){
		if( types->keytype == DAO_DATA_STRING ){
			daoint pos = DString_FindChar( self->name, '<', 0 );
			DMap_Insert( types, self->name, self );
			if( pos != DAO_NULLPOS ){ /* @T<type1|type2> */
				DString *name = DString_New();
				DString_SubString( self->name, name, 0, pos );
				DMap_Insert( types, name, self );
				DString_Delete( name );
			}
		}else{
			DMap_Insert( types, self, 0 );
		}
		return;
	}
	if( self->args ){
		for(i=0,n=self->args->size; i<n; i++){
			DaoType_GetTypeHolders( self->args->items.pType[i], types );
		}
	}
	if( self->bases ){
		for(i=0,n=self->bases->size; i<n; i++){
			DaoType_GetTypeHolders( self->bases->items.pType[i], types );
		}
	}
	if( self->tid == DAO_TYPE || self->tid == DAO_ROUTINE ){ /* XXX */
		if( self->aux && self->aux->type == DAO_TYPE ){
			DaoType_GetTypeHolders( & self->aux->xType, types );
		}
	}
}

int DaoType_Contains( DaoType *self, DaoType *type, DMap *checking )
{
	DMap *checking2 = checking;
	daoint i, n, bl = 0;
	if( self == type ) return 1;
	if( checking && DMap_Find( checking, self ) != NULL ) return 0;
	if( checking == NULL ) checking = DHash_New(0,0);
	DMap_Insert( checking, self, NULL );
	if( self->args ){
		for(i=0,n=self->args->size; i<n; i++){
			bl |= DaoType_Contains( self->args->items.pType[i], type, checking );
		}
	}
	if( self->bases ){
		for(i=0,n=self->bases->size; i<n; i++){
			bl |= DaoType_Contains( self->bases->items.pType[i], type, checking );
		}
	}
	if( self->cbtype ) bl |= DaoType_Contains( self->cbtype, type, checking );
	if( self->aux && self->aux->type == DAO_TYPE )
		bl |= DaoType_Contains( & self->aux->xType, type, checking );

	if( checking2 == NULL ) DMap_Delete( checking );
	return bl;
}

void DaoType_ResetTypeHolders( DaoType *self, DMap *types )
{
	DNode *it;
	for(it=DMap_First(types); it; ){
		if( DaoType_Contains( it->key.pType, self, NULL ) ){
			DMap_EraseNode( types, it );
			it = DMap_First(types);
			continue;
		}
		it = DMap_Next( types, it );
	}
}

static void DaoType_SetupRecursive2( DaoType *self, DaoType *tht, DaoType *root, DMap *chk )
{
	daoint i, n;
	if( DMap_Find( chk, self ) ) return;
	DMap_Insert( chk, self, NULL );
	if( self->args ){
		for(i=0,n=self->args->size; i<n; i++){
			if( self->args->items.pType[i] == tht ){
				GC_Assign( & self->args->items.pType[i], root );
			}else{
				DaoType_SetupRecursive2( self->args->items.pType[i], tht, root, chk );
			}
		}
	}
	if( self->aux && self->aux->type == DAO_TYPE ){
		if( self->aux == (DaoValue*) tht ){
			GC_Assign( & self->aux, root );
		}else{
			DaoType_SetupRecursive2( (DaoType*) self->aux, tht, root, chk );
		}
	}
	if( self->cbtype ) DaoType_SetupRecursive2( self->cbtype, tht, root, chk );
}

void DaoType_SetupRecursive( DaoType *self, DaoType *tht, DaoType *root )
{
	DMap *chk = DHash_New(0,0);
	DaoType_SetupRecursive2( self, tht, root, chk );
	DMap_Delete( chk );
}

void DaoType_ExportArguments( DaoType *self, DList *args, int noname )
{
	int i;
	for(i=0; i<self->args->size; ++i){
		DaoType *it = self->args->items.pType[i];
		if( noname && it->tid >= DAO_PAR_NAMED && it->tid <= DAO_PAR_VALIST ){
			it = (DaoType*) it->aux;
		}
		DList_Append( args, it );
	}
}

DaoVmSpace* DaoType_GetVmSpace( DaoType *self )
{
	return self->nameSpace->vmSpace;
}



extern DMutex mutex_methods_setup;

static void DaoType_TrySetupMethods( DaoType *self )
{
	if( self->tid == DAO_CTYPE ) self = (DaoType*) self->aux->xCtype.valueType;
	if( self->kernel == NULL ) return;
	if( self->kernel->SetupMethods ){
		self->kernel->SetupMethods( self->kernel->nspace, self->core );
	}
	if( self->kernel->methods == NULL ) return;
	if( self->kernel->attribs & DAO_TYPEKERNEL_TEMPLATE ) DaoType_SpecializeMethods( self );
}

DaoRoutine* DaoType_GetInitor( DaoType *self )
{
	DaoType_TrySetupMethods( self );
	if( self->kernel == NULL ) return NULL;
	return self->kernel->initRoutines;
}

DaoRoutine* DaoType_FindFunction( DaoType *self, DString *name )
{
	DNode *node;

	if( self->tid == DAO_INTERFACE || self->tid == DAO_CINTYPE || self->tid == DAO_CINVALUE ){
		DaoValue *aux = self->aux;
		DMap *meths = self->tid == DAO_INTERFACE ? aux->xInterface.methods : aux->xCinType.methods;
		node = DMap_Find( meths, name );
		if( node ) return node->value.pRoutine;
		return NULL;
	}

	DaoType_TrySetupMethods( self );
	if( self->kernel == NULL || self->kernel->methods == NULL ) return NULL;
	node = DMap_Find( self->kernel->methods, name );
	if( node ) return node->value.pRoutine;
	return NULL;
}

DaoRoutine* DaoType_FindFunctionChars( DaoType *self, const char *name )
{
	DString mbs = DString_WrapChars( name );
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
	/*
	// Values are not specialized.
	// Get the original type kernel for template-like cdata type:
	*/
	DaoTypeKernel *kernel = self->kernel;
	DaoValue *value = NULL;
	DNode *node;

	if( kernel == NULL ) return NULL;
	/* Mainly for C data type: */
	if( kernel->abtype && kernel->abtype->aux ){
		if( DString_EQ( name, kernel->abtype->name ) ) value = kernel->abtype->aux;
	}
	if( kernel->SetupValues ) kernel->SetupValues( self->kernel->nspace, self->core );
	if( kernel->values == NULL ) return value;
	node = DMap_Find( kernel->values, name );
	if( node ) return node->value.pValue;
	return value;
}

DaoTypeCore* DaoType_GetTypeCore( DaoType *self )
{
	return self->core;
}

DaoType* DaoType_CheckGetField( DaoType *self, DaoString *name, DaoRoutine *ctx )
{
	if( self->tid == DAO_ENUM && self->mapNames ) return self;
	return NULL;
}

DaoEnum* DaoProcess_GetEnum( DaoProcess *self, DaoVmCode *vmc );

DaoValue* DaoType_DoGetField( DaoValue *self, DaoString *name, DaoProcess *proc )
{
	DaoType *type = (DaoType*) self;

	if( type->tid == DAO_ENUM && type->mapNames ){
		DNode *node = DMap_Find( type->mapNames, name->value );
		if( node ){
			DaoEnum *denum = DaoProcess_GetEnum( proc, proc->activeCode );
			denum->etype = type;
			denum->value = node->value.pInt;
			denum->subtype = type->subtid;
		}    
	}
	return NULL;
}

static void DaoType_Print( DaoValue *self, DaoStream *stream, DMap *cycmap, DaoProcess *proc )
{
	DaoStream_PrintHL( stream, 'A', "type" );
	DaoStream_PrintHL( stream, '{', "<" );
	DaoStream_PrintHL( stream, 'A', self->xType.name->chars );
	DaoStream_PrintHL( stream, '}', ">" );
}

DaoTypeCore daoTypeCore =
{
	"type",                                      /* name */
	sizeof(DaoType),                             /* size */
	{ NULL },                                    /* bases */
	{ NULL },                                    /* casts */
	NULL,                                        /* numbers */
	NULL,                                        /* methods */
	DaoType_CheckGetField,  DaoType_DoGetField,  /* GetField */
	NULL,                   NULL,                /* SetField */
	NULL,                   NULL,                /* GetItem */
	NULL,                   NULL,                /* SetItem */
	NULL,                   NULL,                /* Unary */
	NULL,                   NULL,                /* Binary */
	NULL,                   NULL,                /* Conversion */
	NULL,                   NULL,                /* ForEach */
	DaoType_Print,                               /* Print */
	NULL,                                        /* Slice */
	NULL,                                        /* Compare */
	NULL,                                        /* Hash */
	NULL,                                        /* Create */
	NULL,                                        /* Copy */
	(DaoDeleteFunction) DaoType_Delete,          /* Delete */
	NULL                                         /* HandleGC */
};






DaoTypeKernel* DaoTypeKernel_New( DaoTypeCore *core )
{
	DaoTypeKernel *self = (DaoTypeKernel*) dao_calloc( 1, sizeof(DaoTypeKernel) );
	DaoValue_Init( self, DAO_TYPEKERNEL );
	self->trait |= DAO_VALUE_DELAYGC;
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogNew( (DaoValue*) self );
#endif
	return self;
}

void DaoTypeKernel_Delete( DaoTypeKernel *self )
{
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogDelete( (DaoValue*) self );
#endif
	if( self->values ) DMap_Delete( self->values );
	if( self->methods ) DMap_Delete( self->methods );
	GC_DecRC( self->initRoutines );
	if( self->sptree ) DaoTypeTree_Delete( self->sptree );
	dao_free( self );
}


void DaoTypeKernel_InsertInitor( DaoTypeKernel *self, DaoNamespace *ns, DaoType *host, DaoRoutine *routine )
{
	if( self->initRoutines == NULL ){
		self->initRoutines = DaoRoutines_New( ns, host, NULL );
		GC_IncRC( self->initRoutines );
	}
	DRoutines_Add( self->initRoutines->overloads, routine );
}


DaoTypeCore daoTypeKernelCore =
{
	"kernel",                  /* name */
	sizeof(DaoTypeKernel),     /* size */
	{ NULL },                  /* bases */
	{ NULL },                  /* casts */
	NULL,                      /* numbers */
	NULL,                      /* methods */
	NULL,  NULL,               /* GetField */
	NULL,  NULL,               /* SetField */
	NULL,  NULL,               /* GetItem */
	NULL,  NULL,               /* SetItem */
	NULL,  NULL,               /* Unary */
	NULL,  NULL,               /* Binary */
	NULL,  NULL,               /* Conversion */
	NULL,  NULL,               /* ForEach */
	NULL,                      /* Print */
	NULL,                      /* Slice */
	NULL,                      /* Compare */
	NULL,                      /* Hash */
	NULL,                      /* Create */
	NULL,                      /* Copy */
	(DaoDeleteFunction) DaoTypeKernel_Delete,  /* Delete */
	NULL                       /* HandleGC */
};





static DaoTypeNode* DaoTypeNode_New( DaoTypeTree *tree )
{
	DaoTypeNode *self = (DaoTypeNode*) dao_calloc( 1, sizeof(DaoTypeNode) );
	self->tree = tree;
	return self;
}
static void DaoTypeNode_Delete( DaoTypeNode *self )
{
	while( self->first ){
		DaoTypeNode *node = self->first;
		self->first = node->next;
		DaoTypeNode_Delete( node );
	}
	dao_free( self );
}
static DaoTypeNode* DaoTypeNode_Add( DaoTypeNode *self, DaoType *types[], int count, int pid, DaoType *sptype )
{
	DaoTypeNode *param, *ret;
	DaoType *type;
	int i, n;
	if( pid >= count ){
		/* If a specialization with the same parameter signature is found, return it: */
		for(param=self->first; param; param=param->next) if( param->sptype ) return param;
		param = DaoTypeNode_New( self->tree );
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
		if( param->type == type ) return DaoTypeNode_Add( param, types, count, pid+1, sptype );
	}
	/* Add a new internal node: */
	param = DaoTypeNode_New( self->tree );
	param->type = type;
	ret = DaoTypeNode_Add( param, types, count, pid+1, sptype );
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
static DaoType* DaoTypeNode_GetLeaf( DaoTypeNode *self, int pid, int *ms )
{
	DList *defaults = self->tree->defaults;
	DaoTypeNode *param;
	*ms = 0;
	if( self->sptype ) return self->sptype; /* a leaf */
	/* Explicit specializable types has no default type parameters: */
	if( defaults->size && pid > (int)defaults->size ) return NULL;
	if( pid < (int)defaults->size && defaults->items.pType[pid] == NULL ) return NULL;
	for(param=self->first; param; param=param->next){
		if( param->type == NULL ) return param->sptype; /* a leaf */
		if( param->type == defaults->items.pType[pid] ){
			DaoType *type = DaoTypeNode_GetLeaf( param, pid + 1, ms );
			if( type ) return type;
		}
	}
	return NULL;
}
static DaoType* DaoTypeNode_Get2( DaoTypeNode *self, DaoType *types[], int count, int pid, int *score )
{
	DaoTypeNode *param;
	DaoType *argtype, *sptype = NULL, *best = NULL;
	int i, m, k = 0, max = 0;

	*score = 1;
	if( pid >= count ) return DaoTypeNode_GetLeaf( self, pid, score );
	argtype = types[pid];
	for(param=self->first; param; param=param->next){
		DaoType *partype = param->type;
		if( partype == NULL ) continue;
		if( argtype->tid != partype->tid ) continue;
		if( (m = DaoType_Match( argtype, partype, NULL, NULL, 1, 0 )) < DAO_MT_EQ ){
			continue;
		}
		if( argtype->tid == DAO_THT && partype->tid == DAO_THT ){
			/* @T, @S; @T<int|string>, @T<int|float|string>; ... */
			if( DString_EQ( argtype->name, partype->name ) == 0 ) continue;
		}
		if( (sptype = DaoTypeNode_Get2( param, types, count, pid+1, & k )) == NULL ) continue;
		m += k;
		if( m > max ){
			best = sptype;
			max = m;
		}
	}
	*score = max;
	return best;
}
DaoTypeTree* DaoTypeTree_New()
{
	DaoTypeTree *self = (DaoTypeTree*)dao_malloc( sizeof(DaoTypeTree) );
	self->root = DaoTypeNode_New( self );
	self->holders = DList_New( DAO_DATA_VALUE );
	self->defaults = DList_New( DAO_DATA_VALUE );
	self->sptypes = DList_New( DAO_DATA_VALUE );
	return self;
}
void DaoTypeTree_Delete( DaoTypeTree *self )
{
	DaoTypeNode_Delete( self->root );
	DList_Delete( self->holders );
	DList_Delete( self->defaults );
	DList_Delete( self->sptypes );
	dao_free( self );
}
/* Test if the type can be specialized according to the type parameters: */
int DaoTypeTree_Test( DaoTypeTree *self, DaoType *types[], int count )
{
	daoint i, n = self->holders->size;
	if( n == 0 || count > n ) return 0;
	for(i=count; i<n; i++){
		if( self->defaults->items.pType[i] == NULL ) return 0;
	}
	for(i=0; i<count; i++){
		DaoType *par = self->holders->items.pType[i];
		DaoType *arg = types[i];
		int mt = DaoType_MatchTo( arg, par, NULL );
		if( mt == 0 || (mt >= DAO_MT_SIM && mt <= DAO_MT_SUB) ) return 0;
	}
	return 1;
}
void DaoTypeTree_Add( DaoTypeTree *self, DaoType *types[], int count, DaoType *sptype )
{
	DaoTypeNode_Add( self->root, types, count, 0, sptype );
	DList_Append( self->sptypes, sptype );
}
DaoType* DaoTypeTree_Get( DaoTypeTree *self, DaoType *types[], int count )
{
	int score = 0;
	/* For explicitly specialized types, the specialization tree has no type holders: */
	if( self->holders->size && DaoTypeTree_Test( self, types, count ) == 0 ) return NULL;
	return DaoTypeNode_Get2( self->root, types, count, 0, & score );
}

static DaoType* DaoCdataType_Specialize( DaoType *self, DaoType *types[], int count, DaoNamespace *ns )
{
	DaoType *sptype, *sptype2;
	DaoTypeKernel *kernel;
	DaoTypeTree *sptree;
	DaoCtype *ctype;
	uchar_t tid = self->tid;
	daoint i, pos;

	assert( self->tid >= DAO_CSTRUCT && self->tid <= DAO_CTYPE );

	self = self->kernel->abtype;
	if( tid == DAO_CTYPE ) self = self->aux->xCtype.valueType;

	if( (kernel = self->kernel) == NULL ) return NULL;
	if( (sptree = kernel->sptree) == NULL ) return NULL;
	if( (sptype = DaoTypeTree_Get( sptree, types, count )) ){
		if( tid == DAO_CTYPE ) return sptype->aux->xCtype.classType;
		return sptype;
	}
	if( DaoTypeTree_Test( sptree, types, count ) == 0 ) return NULL;

	/*
	// Specialized cdata type will be initialized with the same kernel as the template type.
	// Upon method accessing, a new kernel will be created with specialized methods.
	*/
	ctype = DaoCtype_New( ns, self->core, self->tid );
	sptype = ctype->valueType;
	sptype2 = ctype->classType;
	GC_Assign( & sptype->kernel, self->kernel );
	GC_Assign( & sptype2->kernel, self->kernel );
	sptype->args = DList_New( DAO_DATA_VALUE );
	if( self->tid == DAO_CTYPE ){
		sptype->tid = self->aux->xCtype.valueType->tid;
	}else{
		sptype->tid = self->tid;
	}

	pos = DString_FindChar( sptype->name, '<', 0 );
	if( pos != DAO_NULLPOS ) DString_Erase( sptype->name, pos, -1 );
	DString_AppendChar( sptype->name, '<' );
	for(i=0; i<count; i++){
		if( i ) DString_AppendChar( sptype->name, ',' );
		DString_Append( sptype->name, types[i]->name );
		DList_Append( sptype->args, types[i] );
	}
	for(i=count; i<sptree->holders->size; i++){
		if( i ) DString_AppendChar( sptype->name, ',' );
		DString_Append( sptype->name, sptree->defaults->items.pType[i]->name );
		DList_Append( sptype->args, sptree->defaults->items.pType[i] );
	}
	sptype2->args = DList_Copy( sptype->args );
	DString_AppendChar( sptype->name, '>' );
	DString_Assign( sptype2->name, sptype->name );
	DaoTypeTree_Add( sptree, sptype->args->items.pType, sptype->args->size, sptype );
	if( self->bases ){
		DMap *defs = DHash_New(0,0);
		for(i=0; i<count; i++){
			DaoType_MatchTo( types[i], sptree->holders->items.pType[i], defs );
		}
		sptype->bases = DList_New( DAO_DATA_VALUE );
		sptype2->bases = DList_New( DAO_DATA_VALUE );
		for(i=0; i<self->bases->size; i++){
			DaoType *type = self->bases->items.pType[i];
			type = DaoType_DefineTypes( type, ns, defs );
			DList_Append( sptype->bases, type );
			DList_Append( sptype2->bases, type->aux->xCtype.classType );
		}
		DMap_Delete( defs );
	}
	/* May need to get rid of the attributes for type holders: */
	DaoType_CheckAttributes( sptype );
	DaoType_CheckAttributes( sptype2 );
	DString_Assign( sptype->aux->xCtype.name, sptype->name );
	if( tid == DAO_CTYPE ) return sptype2;
	return sptype;
}
static DaoType* DaoGenericType_Specialize( DaoType *self, DaoType *types[], int count, DaoNamespace *ns )
{
	DaoType *sptype;
	DaoTypeKernel *kernel;
	DaoTypeTree *sptree;
	daoint i, pos;

	assert( self->tid == DAO_ARRAY || self->tid == DAO_LIST || self->tid == DAO_MAP );

	self = self->kernel->abtype;

	if( (kernel = self->kernel) == NULL ) return NULL;
	if( (sptree = kernel->sptree) == NULL ) return NULL;
	if( (sptype = DaoTypeTree_Get( sptree, types, count )) ) return sptype;
	if( DaoTypeTree_Test( sptree, types, count ) == 0 ) return NULL;

	/*
	// Specialized type will be initialized with the same kernel as the template type.
	// Upon method accessing, a new kernel will be created with specialized methods.
	*/
	sptype = DaoType_New( ns, self->name->chars, self->tid, NULL, NULL );
	GC_Assign( & sptype->kernel, self->kernel );
	sptype->tid = self->tid;
	sptype->args = DList_New( DAO_DATA_VALUE );

	pos = DString_FindChar( sptype->name, '<', 0 );
	if( pos != DAO_NULLPOS ) DString_Erase( sptype->name, pos, -1 );
	DString_AppendChar( sptype->name, '<' );
	for(i=0; i<count; i++){
		if( i ) DString_AppendChar( sptype->name, ',' );
		DString_Append( sptype->name, types[i]->name );
		DList_Append( sptype->args, types[i] );
	}
	for(i=count; i<sptree->holders->size; i++){
		if( i ) DString_AppendChar( sptype->name, ',' );
		DString_Append( sptype->name, sptree->defaults->items.pType[i]->name );
		DList_Append( sptype->args, sptree->defaults->items.pType[i] );
	}
	DString_AppendChar( sptype->name, '>' );
	DaoTypeTree_Add( sptree, sptype->args->items.pType, sptype->args->size, sptype );

#if 0
	printf( "DaoGenericType_Specialize: %s %s %p\n", self->name->chars, sptype->name->chars, sptype );
#endif

	/* May need to get rid of the attributes for type holders: */
	DaoType_CheckAttributes( sptype );
	if( sptype->tid == DAO_ARRAY ){
		GC_DecRC( sptype->value );
		sptype->value = NULL;
		DaoType_InitDefault( sptype );
	}
	return sptype;
}
DaoType* DaoType_Specialize( DaoType *self, DaoType *types[], int count, DaoNamespace *ns )
{
	int konst = self->konst;
	int invar = self->invar;
	int var = self->var;
	DaoType *type = NULL;

	switch( self->tid ){
	case DAO_ARRAY :
	case DAO_LIST :
	case DAO_MAP :
		type = DaoGenericType_Specialize( self, types, count, ns );
		break;
	case DAO_CSTRUCT :
	case DAO_CDATA :
	case DAO_CTYPE :
		type = DaoCdataType_Specialize( self, types, count, ns );
		break;
	}
	if( konst ){
		type = DaoType_GetConstType( type );
	}else if( invar ){
		type = DaoType_GetInvarType( type );
	}else if( var ){
		type = DaoType_GetVarType( type );
	}
	return type;
}


/*
// For constructors and static methods.
// Note: types are not checked!
*/
static void DaoType_InitHostTypeDefines( DaoType *self, DaoType *type, DMap *defs )
{
	int i, N;
	DMap_Insert( defs, type, self );
	if( self->aux && type->aux && self->aux->type == DAO_TYPE && type->aux->type == DAO_TYPE ){
		DaoType_InitHostTypeDefines( (DaoType*) self->aux, (DaoType*) type->aux, defs );
	}
	if( self->args == NULL || type->args == NULL ) return;
	N = self->args->size < type->args->size ? self->args->size : type->args->size;
	for(i=0; i<N; ++i){
		DaoType *T1 = self->args->items.pType[i];
		DaoType *T2 = type->args->items.pType[i];
		DaoType_InitHostTypeDefines( T1, T2, defs );
	}
}

/*
// Init type defines for methods which may have type holders different from
// those of the host type.
*/
static void DaoType_InitTypeDefines( DaoType *self, DaoRoutine *method, DMap *defs )
{
	DaoType *type = method->routType;
	daoint i;

	DaoType_InitHostTypeDefines( self, self->kernel->abtype, defs );

	if( !(type->attrib & DAO_TYPE_SELF) ) return;
	type = (DaoType*) type->args->items.pType[0]->aux; /* self:type */

	if( type->args->size != self->args->size ) return;
	if( type->konst == self->konst && type->invar == self->invar ){
		DMap_Insert( defs, type, self );
	}
	for(i=0; i<self->args->size; i++){
		DaoType_MatchTo( self->args->items.pType[i], type->args->items.pType[i], defs );
	}
}
static void DaoType_SpecMethod( DaoType *self, DaoTypeKernel *kernel, DaoRoutine *method, DMap *defs )
{
	DaoNamespace *nspace = self->nameSpace;
	DaoRoutine *rout = DaoRoutine_Copy( method, 1, 0, 0 );

	if( method->attribs & DAO_ROUT_INITOR ) DString_Assign( rout->routName, self->name );
	DaoType_InitTypeDefines( self, rout, defs );
	DaoRoutine_Finalize( rout, method, self, nspace, defs );
	DaoMethods_Insert( kernel->methods, rout, nspace, self );
	if( method->attribs & DAO_ROUT_INITOR ){
		DaoTypeKernel_InsertInitor( kernel, nspace, self, rout );
	}
}

void DaoType_SpecializeMethods( DaoType *self )
{
	DaoTypeKernel *kernel = self->kernel;
	DaoType *original = kernel->abtype;
	DaoType *intype = self;
	DaoType *quads[4];
	DNode *it;
	daoint i, k;

#if 0
	printf( "DaoType_SpecializeMethods: %s\n", self->name->chars );
#endif

	if( self->invar ) self = DaoType_GetBaseType( self );
	if( self == original ) return;
	if( self->kernel != original->kernel ) return;
	if( original->kernel == NULL || original->kernel->methods == NULL ) return;
	assert( (self->tid >= DAO_CSTRUCT && self->tid <= DAO_CTYPE) || self->tid == DAO_ARRAY || self->tid == DAO_LIST || self->tid == DAO_MAP );
	if( self->tid == DAO_CTYPE ) self = self->aux->xCtype.valueType;
	if( self->bases ){
		for(i=0; i<self->bases->size; i++){
			DaoType *base = self->bases->items.pType[i];
			DaoType_SpecializeMethods( base );
		}
	}
	if( original->kernel->sptree == NULL ) return;
	DMutex_Lock( & mutex_methods_setup );
	if( self->kernel == original->kernel && original->kernel && original->kernel->methods ){
		DaoNamespace *nspace = self->kernel->nspace;
		DMap *orimeths = original->kernel->methods;
		DMap *methods = DHash_New( DAO_DATA_STRING, DAO_DATA_VALUE );
		DMap *defs = DHash_New(0,0);
		DList *parents = DList_New(0);
		DNode *it;

		kernel = DaoTypeKernel_New( self->core );
		kernel->attribs = original->kernel->attribs;
		kernel->nspace = original->kernel->nspace;
		kernel->abtype = original;
		kernel->methods = methods;
		GC_IncRC( kernel->nspace );
		GC_IncRC( kernel->abtype );

		/* Required for redefining routHost: */
		for(i=0; i<self->args->size; i++){
			DaoType_MatchTo( self->args->items.pType[i], original->args->items.pType[i], defs );
		}
		DList_Append( parents, self );
		for(k=0; k<parents->size; k++){
			DaoType *type = parents->items.pType[k];
			if( type->bases == NULL ) continue;
			for(i=0; i<type->bases->size; i++){
				DaoType *base = type->bases->items.pType[i];
				DList_Append( parents, base );
			}
		}
		for(it=DMap_First(orimeths); it; it=DMap_Next(orimeths, it)){
			DaoRoutine *rout, *rout2, *routine = it->value.pRoutine;
			if( routine->routHost->aux != original->aux ) continue;
			if( routine->overloads ){
				for(i=0; i<routine->overloads->routines->size; i++){
					rout = rout2 = routine->overloads->routines->items.pRoutine[i];
					if( rout->routHost->aux != original->aux ) continue;
					DaoType_SpecMethod( self, kernel, rout, defs );
				}
			}else{
				DaoType_SpecMethod( self, kernel, routine, defs );
			}
		}

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
		DList_Delete( parents );
		DMap_Delete( defs );
		/* Set new kernel after it has been setup, for read safety in multithreading: */
		if( self->tid >= DAO_CSTRUCT && self->tid <= DAO_CTYPE ){
			GC_Assign( & self->aux->xCtype.classType->kernel, kernel );
			GC_Assign( & self->aux->xCtype.valueType->kernel, kernel );
		}else{
			GC_Assign( & self->kernel, kernel );
		}
		DaoType_GetQuadTypes( self, quads );
		for(i=0; i<4; ++i){
			if( quads[i] ) GC_Assign( & quads[i]->kernel, kernel );
		}
	}
	DMutex_Unlock( & mutex_methods_setup );
}



static DaoType* DaoType_CheckGetFieldAny( DaoType *self, DaoString *name, DaoRoutine *ctx )
{
	return self;
}

static int DaoType_CheckSetFieldAny( DaoType *self, DaoString *name, DaoType *value, DaoRoutine *ctx )
{
	return DAO_OK;
}

static DaoType* DaoType_CheckGetItemAny( DaoType *self, DaoType *index[], int N, DaoRoutine *ctx )
{
	return self;
}

static int DaoType_CheckSetItemAny( DaoType *self, DaoType *index[], int N, DaoType *value, DaoRoutine *ctx )
{
	return DAO_OK;
}

static DaoType* DaoType_CheckUnaryAny( DaoType *self, DaoVmCode *op, DaoRoutine *ctx )
{
	return self;
}

static DaoType* DaoType_CheckBinaryAny( DaoType *self, DaoVmCode *op, DaoType *args[2], DaoRoutine *ctx )
{
	return self;
}

static DaoType* DaoType_CheckConversionAny( DaoType *self, DaoType *type, DaoRoutine *ctx )
{
	return type;
}

static DaoType* DaoType_CheckForEachAny( DaoType *self, DaoRoutine *ctx )
{
	return DaoNamespace_MakeIteratorType( ctx->nameSpace, self );
}


static DaoTypeCore daoAnyCore =
{
	"any",                              /* name */
	0,                                  /* size */
	{ NULL },                           /* bases */
	{ NULL },                           /* casts */
	NULL,                               /* numbers */
	NULL,                               /* methods */
	DaoType_CheckGetFieldAny,    NULL,  /* GetField */
	DaoType_CheckSetFieldAny,    NULL,  /* SetField */
	DaoType_CheckGetItemAny,     NULL,  /* GetItem */
	DaoType_CheckSetItemAny,     NULL,  /* SetItem */
	DaoType_CheckUnaryAny,       NULL,  /* Unary */
	DaoType_CheckBinaryAny,      NULL,  /* Binary */
	DaoType_CheckConversionAny,  NULL,  /* Conversion */
	DaoType_CheckForEachAny,     NULL,  /* ForEach */
	NULL,                               /* Print */
	NULL,                               /* Slice */
	NULL,                               /* Compare */
	NULL,                               /* Hash */
	NULL,                               /* Create */
	NULL,                               /* Copy */
	NULL,                               /* Delete */
	NULL                                /* HandleGC */
};


static DaoType* DaoType_CheckGetFieldVariant( DaoType *self, DaoString *name, DaoRoutine *ctx )
{
	return ctx->nameSpace->vmSpace->typeAny;
}

static int DaoType_CheckSetFieldVariant( DaoType *self, DaoString *name, DaoType *value, DaoRoutine *ctx )
{
	return DAO_OK;
}

static DaoType* DaoType_CheckGetItemVariant( DaoType *self, DaoType *index[], int N, DaoRoutine *ctx )
{
	return ctx->nameSpace->vmSpace->typeAny;
}

static int DaoType_CheckSetItemVariant( DaoType *self, DaoType *index[], int N, DaoType *value, DaoRoutine *ctx )
{
	return DAO_OK;
}

static DaoType* DaoType_CheckUnaryVariant( DaoType *self, DaoVmCode *op, DaoRoutine *ctx )
{
	return ctx->nameSpace->vmSpace->typeAny;
}

static DaoType* DaoType_CheckBinaryVariant( DaoType *self, DaoVmCode *op, DaoType *args[2], DaoRoutine *ctx )
{
	return ctx->nameSpace->vmSpace->typeAny;
}

static DaoType* DaoType_CheckConversionVariant( DaoType *self, DaoType *type, DaoRoutine *ctx )
{
	return type;
}

static DaoType* DaoType_CheckForEachVariant( DaoType *self, DaoRoutine *ctx )
{
	return DaoNamespace_MakeIteratorType( ctx->nameSpace, ctx->nameSpace->vmSpace->typeAny );
}


static DaoTypeCore daoVariantCore =
{
	"variant",                              /* name */
	0,                                      /* size */
	{ NULL },                               /* bases */
	{ NULL },                               /* casts */
	NULL,                                   /* numbers */
	NULL,                                   /* methods */
	DaoType_CheckGetFieldVariant,    NULL,  /* GetField */
	DaoType_CheckSetFieldVariant,    NULL,  /* SetField */
	DaoType_CheckGetItemVariant,     NULL,  /* GetItem */
	DaoType_CheckSetItemVariant,     NULL,  /* SetItem */
	DaoType_CheckUnaryVariant,       NULL,  /* Unary */
	DaoType_CheckBinaryVariant,      NULL,  /* Binary */
	DaoType_CheckConversionVariant,  NULL,  /* Conversion */
	DaoType_CheckForEachVariant,     NULL,  /* ForEach */
	NULL,                                   /* Print */
	NULL,                                   /* Slice */
	NULL,                                   /* Compare */
	NULL,                                   /* Hash */
	NULL,                                   /* Create */
	NULL,                                   /* Copy */
	NULL,                                   /* Delete */
	NULL                                    /* HandleGC */
};



DaoTypeCore* DaoType_GetCoreByID( short type )
{
	switch( type ){
	case DAO_NONE      :  return & daoNoneCore;
	case DAO_BOOLEAN   :  return & daoBooleanCore;
	case DAO_INTEGER   :  return & daoIntegerCore;
	case DAO_FLOAT     :  return & daoFloatCore;
	case DAO_COMPLEX   :  return & daoComplexCore;
	case DAO_ENUM      :  return & daoEnumCore;
	case DAO_STRING    :  return & daoStringCore;
	case DAO_LIST      :  return & daoListCore;
	case DAO_MAP       :  return & daoMapCore;
	case DAO_TUPLE     :  return & daoTupleCore;
	case DAO_PAR_NAMED :  return & daoNameValueCore;
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY  :  return & daoArrayCore;
#else
	case DAO_ARRAY  :  return & daoNoneCore;
#endif
	case DAO_CTYPE     :  return & daoCtypeCore;
	case DAO_CSTRUCT   :  return & daoCstructCore;
	case DAO_CDATA     :  return & daoCdataCore;
	case DAO_INTERFACE :  return & daoInterfaceCore;
	case DAO_CINTYPE   :  return & daoCinTypeCore;
	case DAO_CINVALUE  :  return & daoCinValueCore;
	case DAO_CLASS     :  return & daoClassCore;
	case DAO_OBJECT    :  return & daoObjectCore;
	case DAO_ROUTINE   :  return & daoRoutineCore;
	case DAO_NAMESPACE :  return & daoNamespaceCore;
	case DAO_PROCESS   :  return & daoProcessCore;
	case DAO_TYPE      :  return & daoTypeCore;
	case DAO_TYPEKERNEL : return & daoTypeKernelCore;
	case DAO_VARIANT    : return & daoVariantCore;
	default : if( type & DAO_ANY ) return & daoAnyCore;
	}
	return NULL;
}



void DaoType_Init()
{
	int i, j;
	memset( dao_type_matrix, DAO_MT_NOT, END_EXTRA_TYPES*END_EXTRA_TYPES );
	for(i=DAO_BOOLEAN; i<=DAO_FLOAT; i++){
		dao_type_matrix[DAO_ENUM][i] = DAO_MT_SIM;
		dao_type_matrix[i][DAO_COMPLEX] = DAO_MT_SIM;
		for(j=DAO_BOOLEAN; j<=DAO_FLOAT; j++)
			dao_type_matrix[i][j] = DAO_MT_SIM;
	}
	dao_type_matrix[DAO_NONE][DAO_NONE] = DAO_MT_EXACT;
	dao_type_matrix[DAO_BOOLEAN][DAO_ENUM] = DAO_MT_SIM;
	dao_type_matrix[DAO_INTEGER][DAO_ENUM] = DAO_MT_SIM;
	for(i=0; i<END_EXTRA_TYPES; i++) dao_type_matrix[i][i] = DAO_MT_EQ;
	for(i=0; i<END_EXTRA_TYPES; i++){
		dao_type_matrix[i][DAO_PAR_NAMED] = DAO_MT_EXACT+2;
		dao_type_matrix[i][DAO_PAR_DEFAULT] = DAO_MT_EXACT+2;
		dao_type_matrix[DAO_PAR_NAMED][i] = DAO_MT_EXACT+2;
		dao_type_matrix[DAO_PAR_DEFAULT][i] = DAO_MT_EXACT+2;

		dao_type_matrix[i][DAO_CINVALUE] = DAO_MT_EXACT+1;
		dao_type_matrix[DAO_CINVALUE][i] = DAO_MT_EXACT+1;
		dao_type_matrix[DAO_VARIANT][i] = DAO_MT_EXACT+1;
		dao_type_matrix[i][DAO_VARIANT] = DAO_MT_EXACT+1;
	}
	dao_type_matrix[DAO_VARIANT][DAO_VARIANT] = DAO_MT_EXACT+1;

	for(i=0; i<END_EXTRA_TYPES; ++i){
		dao_type_matrix[i][DAO_UDT] = DAO_MT_THT;
		dao_type_matrix[i][DAO_THT] = DAO_MT_THT;
		dao_type_matrix[i][DAO_ANY] = DAO_MT_ANY;
		dao_type_matrix[DAO_UDT][i] = DAO_MT_THTX;
		dao_type_matrix[DAO_THT][i] = DAO_MT_THTX;
		dao_type_matrix[DAO_ANY][i] = DAO_MT_ANYX;
	}
	for(i=DAO_ANY; i<=DAO_UDT; ++i){
		for(j=DAO_ANY; j<=DAO_UDT; ++j){
			dao_type_matrix[i][j] = DAO_MT_LOOSE;
		}
	}

	dao_type_matrix[DAO_ANY][DAO_ANY] = DAO_MT_EQ;
	dao_type_matrix[DAO_ANY][DAO_THT] = DAO_MT_THT;
	dao_type_matrix[DAO_THT][DAO_THT] = DAO_MT_THTX;
	dao_type_matrix[DAO_THT][DAO_ANY] = DAO_MT_THTX;

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

