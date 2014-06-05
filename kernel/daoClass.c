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

#include"assert.h"
#include"string.h"
#include"daoConst.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoRoutine.h"
#include"daoProcess.h"
#include"daoOptimizer.h"
#include"daoGC.h"
#include"daoStream.h"
#include"daoNumtype.h"
#include"daoValue.h"
#include"daoNamespace.h"
#include"daoVmspace.h"

static void DaoClass_GetField( DaoValue *self0, DaoProcess *proc, DString *name )
{
	int tid = proc->activeRoutine->routHost ? proc->activeRoutine->routHost->tid : 0;
	DaoType *type = proc->activeRoutine->routHost;
	DaoClass *host = tid == DAO_OBJECT ? & type->aux->xClass : NULL;
	DaoClass *self = & self0->xClass;
	DString *mbs = DString_New();
	DaoValue *value = NULL;
	int rc = DaoClass_GetData( self, name, & value, host );
	if( rc ){
		DString_SetChars( mbs, DString_GetData( self->className ) );
		DString_AppendChars( mbs, "." );
		DString_Append( mbs, name );
		DaoProcess_RaiseException( proc, daoExceptionName[rc], mbs->chars, NULL );
	}else{
		DaoProcess_PutValue( proc, value );
	}
	DString_Delete( mbs );
}
static void DaoClass_SetField( DaoValue *self0, DaoProcess *proc, DString *name, DaoValue *value )
{
	DaoClass *self = & self0->xClass;
	DNode *node = DMap_Find( self->lookupTable, name );
	if( node && LOOKUP_ST( node->value.pInt ) == DAO_CLASS_VARIABLE ){
		int up = LOOKUP_UP( node->value.pInt );
		int id = LOOKUP_ID( node->value.pInt );
		DaoVariable *dt = self->variables->items.pVar[id];
		if( DaoValue_Move( value, & dt->value, dt->dtype ) ==0 )
			DaoProcess_RaiseError( proc, "Param", "not matched" );
	}else{
		/* XXX permission */
		DaoProcess_RaiseError( proc, "Field", "not exist" );
	}
}
static void DaoClass_GetItem( DaoValue *self0, DaoProcess *proc, DaoValue *ids[], int N )
{
}
static void DaoClass_SetItem( DaoValue *self0, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
}

static DaoTypeCore classCore=
{
	NULL,
	DaoClass_GetField,
	DaoClass_SetField,
	DaoClass_GetItem,
	DaoClass_SetItem,
	DaoValue_Print
};

DaoTypeBase classTyper =
{
	"class", & classCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoClass_Delete, NULL
};

DaoClass* DaoClass_New()
{
	DaoClass *self = (DaoClass*) dao_calloc( 1, sizeof(DaoClass) );
	DaoValue_Init( self, DAO_CLASS );
	self->trait |= DAO_VALUE_DELAYGC;
	self->className = DString_New();

	self->lookupTable = DHash_New( DAO_DATA_STRING, 0 );
	self->ovldRoutMap = DHash_New( DAO_DATA_STRING, 0 );
	self->abstypes    = DMap_New( DAO_DATA_STRING, DAO_DATA_VALUE );
	self->constants   = DArray_New( DAO_DATA_VALUE );
	self->variables   = DArray_New( DAO_DATA_VALUE );
	self->instvars    = DArray_New( DAO_DATA_VALUE );
	self->objDataName = DArray_New( DAO_DATA_STRING );
	self->cstDataName = DArray_New( DAO_DATA_STRING );
	self->glbDataName = DArray_New( DAO_DATA_STRING );
	self->parent = NULL;
	self->mixinBases = DArray_New(0);  /* refcount handled in ::allBases; */
	self->allBases   = DArray_New( DAO_DATA_VALUE );
	self->mixins  = DArray_New(0);
	self->ranges  = DVector_New(sizeof(ushort_t));
	self->offsets = DVector_New(sizeof(ushort_t));
	self->references  = DArray_New( DAO_DATA_VALUE );
	self->clsInter = NULL;
	self->objInter = NULL;

	self->cstMixinStart = self->cstMixinEnd = self->cstMixinEnd2 = 0;
	self->glbMixinStart = self->glbMixinEnd = self->glbMixinEnd2 = 0;
	self->objMixinStart = self->objMixinEnd = self->objMixinEnd2 = 0;
	self->cstParentStart = self->cstParentEnd = 0;
	self->glbParentStart = self->glbParentEnd = 0;
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogNew( (DaoValue*) self );
#endif
	return self;
}
void DaoClass_Delete( DaoClass *self )
{
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogDelete( (DaoValue*) self );
#endif
	GC_DecRC( self->clsType );
	GC_DecRC( self->castRoutines );
	DMap_Delete( self->abstypes );
	DMap_Delete( self->lookupTable );
	DMap_Delete( self->ovldRoutMap );
	DArray_Delete( self->constants );
	DArray_Delete( self->variables );
	DArray_Delete( self->instvars );
	DArray_Delete( self->objDataName );
	DArray_Delete( self->cstDataName );
	DArray_Delete( self->glbDataName );
	DArray_Delete( self->allBases );
	DArray_Delete( self->mixinBases );
	DArray_Delete( self->mixins );
	DVector_Delete( self->ranges );
	DVector_Delete( self->offsets );
	DArray_Delete( self->references );
	if( self->decoTargets ) DArray_Delete( self->decoTargets );

	DString_Delete( self->className );
	dao_free( self );
}
void DaoClass_AddReference( DaoClass *self, void *reference )
{
	if( reference == NULL ) return;
	DArray_Append( self->references, reference );
}
void DaoRoutine_MapTypes( DaoRoutine *self, DMap *deftypes );
int DaoRoutine_Finalize( DaoRoutine *self, DaoType *host, DMap *deftypes );
void DaoClass_Parents( DaoClass *self, DArray *parents, DArray *offsets );


void DaoClass_SetName( DaoClass *self, DString *name, DaoNamespace *ns )
{
	DaoRoutine *rout;
	DString *str;

	if( self->classRoutine ) return;

	self->clsInter = DaoInterface_New( name->chars );
	self->objInter = DaoInterface_New( name->chars );
	DString_SetChars( self->clsInter->abtype->name, "interface<class<" );
	DString_SetChars( self->objInter->abtype->name, "interface<" );
	DString_Append( self->clsInter->abtype->name, name );
	DString_Append( self->objInter->abtype->name, name );
	DString_AppendChars( self->clsInter->abtype->name, ">>" );
	DString_AppendChar( self->objInter->abtype->name, '>' );
	DaoClass_AddReference( self, self->clsInter );
	DaoClass_AddReference( self, self->objInter );

	self->objType = DaoType_New( name->chars, DAO_OBJECT, (DaoValue*)self, NULL );
	self->clsType = DaoType_New( name->chars, DAO_CLASS, (DaoValue*) self, NULL );
	GC_IncRC( self->clsType );
	GC_IncRC( self->clsType );
	GC_IncRC( self->objType );
	self->clsInter->model = self->clsType;
	self->objInter->model = self->objType;
	DString_InsertChars( self->clsType->name, "class<", 0, 0, 0 );
	DString_AppendChar( self->clsType->name, '>' );

	str = DString_New();
	DString_SetChars( str, "self" );
	DaoClass_AddObjectVar( self, str, NULL, self->objType, DAO_PERM_PRIVATE );
	DString_Assign( self->className, name );
	DaoClass_AddType( self, name, self->objType );

	rout = DaoRoutine_New( ns, self->objType, 1 );
	DString_Assign( rout->routName, name );
	DString_AppendChars( rout->routName, "::" );
	DString_Append( rout->routName, name );
	self->classRoutine = rout; /* XXX class<name> */
	GC_IncRC( rout );

	rout->routType = DaoType_New( "routine<=>", DAO_ROUTINE, (DaoValue*)self->objType, NULL );
	DString_Append( rout->routType->name, name );
	DString_AppendChars( rout->routType->name, ">" );
	GC_IncRC( rout->routType );
	rout->attribs |= DAO_ROUT_INITOR;

	DaoClass_AddConst( self, name, (DaoValue*) self, DAO_PERM_PUBLIC );

	self->classRoutines = DaoRoutines_New( ns, self->objType, NULL );
	DString_Assign( self->classRoutines->routName, name );

	DaoClass_AddConst( self, rout->routName, (DaoValue*)self->classRoutines, DAO_PERM_PUBLIC );

	self->objType->value = (DaoValue*) DaoObject_Allocate( self, 0 );
	self->objType->value->xObject.trait |= DAO_VALUE_CONST|DAO_VALUE_NOCOPY;
	self->objType->value->xObject.isNull = 1;
	GC_IncRC( self->objType->value );

	DString_Delete( str );
}
/* breadth-first search */
void DaoClass_Parents( DaoClass *self, DArray *parents, DArray *offsets )
{
	DaoValue *dbase;
	DaoClass *klass;
	DaoCdata *cdata;
	DaoTypeBase *typer;
	daoint i, j, offset;
	DArray_Clear( parents );
	DArray_Clear( offsets );
	DArray_Append( parents, self );
	DArray_Append( offsets, self->objDataName->size );
	for(i=0; i<parents->size; i++){
		dbase = parents->items.pValue[i];
		offset = offsets->items.pInt[i];
		if( dbase->type == DAO_CLASS ){
			klass = (DaoClass*) dbase;
			if( klass->parent ){
				DaoClass *cls = (DaoClass*) klass->parent;
				DArray_Append( parents, klass->parent );
				DArray_Append( offsets, (daoint) offset );
				offset += (cls->type == DAO_CLASS) ? cls->objDataName->size : 0;
			}
		}else if( dbase->type == DAO_CTYPE ){
			cdata = (DaoCdata*) dbase;
			typer = cdata->ctype->kernel->typer;
			for(j=0; j<DAO_MAX_CDATA_SUPER; j++){
				if( typer->supers[j] == NULL ) break;
				DArray_Append( parents, typer->supers[j]->core->kernel->abtype->aux );
				DArray_Append( offsets, (daoint) offset );
			}
		}
	}
}


typedef struct DaoMethodFields DaoMethodFields;

struct DaoMethodFields
{
	DArray  *names;
	DArray  *perms;
	DArray  *routines;
};
static DaoMethodFields* DaoMethodFields_New()
{
	DaoMethodFields *self = (DaoMethodFields*) dao_malloc( sizeof(DaoMethodFields) );
	self->names = DArray_New( DAO_DATA_STRING );
	self->perms = DArray_New(0);
	self->routines = DArray_New(0);
	return self;
}
static void DaoMethodFields_Delete( DaoMethodFields *self )
{
	DArray_Delete( self->names );
	DArray_Delete( self->perms );
	DArray_Delete( self->routines );
	dao_free( self );
}


static DaoClass* DaoClass_FindMixin( DaoClass *mixin, int st, int id, int *offset )
{
	DaoClass *last;
	ushort_t *offsets = mixin->ranges->data.ushorts;
	int size1 = mixin->mixins->size - 1;
	int i = 0, j = size1;
	int st2 = 0;

	if( offset ) *offset = 0;

	/* return this mixin if there is no component mixins: */
	if( size1 == -1 ) return mixin;

	last = mixin->mixins->items.pClass[size1];
	switch( st ){
	case DAO_CLASS_CONSTANT  : st2 = 0; break;
	case DAO_CLASS_VARIABLE  : st2 = 1; break;
	case DAO_OBJECT_VARIABLE : st2 = 2; break;
	}

	/* return this mixin if the index is for this mixin: */
	if( id < offsets[st2] ) return mixin;
	if( id >= offsets[6*size1+3+st2] ) return mixin;

	if( offset ) *offset = offsets[st2]; /* set the offset of the component mixin; */

	/* return the component mixin if there is only one: */
	if( size1 == 0 ) return last;

	while( i <= j ){ /* binary searching: */
		int k = (i + j) / 2;
		DaoClass *mid = mixin->mixins->items.pClass[k];
		if( offset ) *offset = offsets[6*k+st2];
		if( i == j ) return mid;
		if( id < offsets[6*k+st2] ){
			j = k - 1;
		}else if( id >= offsets[6*k+3+st2] ){
			i = k + 1;
		}else{
			return mid;
		}
	}
	return mixin;
}
static int DaoClass_MapIndex( DaoClass *mixin, int st, int id, DMap *mixed )
{
	int offset = 0;
	DaoClass *mx = DaoClass_FindMixin( mixin, st, id, & offset );
	DNode *it = DMap_Find( mixed, mx );
	if( it != NULL ) it = MAP_Find( it->value.pMap, LOOKUP_BIND( st, 0, 0, id-offset ) );
	if( it != NULL ) return LOOKUP_ID( it->value.pInt );
	return id;
}
static DString* DaoClass_GetDataName( DaoClass *self, int st, int id )
{
	DNode *it;
	for(it=DMap_First(self->lookupTable); it; it=DMap_Next(self->lookupTable,it)){
		if( st == LOOKUP_ST(it->value.pInt) && id == LOOKUP_ID(it->value.pInt) ){
			return it->value.pString;
		}
	}
	return NULL;
}
static int DaoRoutine_GetFieldIndex( DaoRoutine *self, DString *name )
{
	DString none = DString_WrapChars( "" );
	DaoString str = {DAO_STRING,0,0,0,0,NULL};
	DaoString *s = & str;
	daoint i;
	if( name == NULL ) name = & none;
	for(i=0; i<self->routConsts->value->size; ++i){
		DaoValue *item = DaoList_GetItem( self->routConsts, i );
		DString *field = DaoValue_TryGetString( item );
		if( field == NULL ) continue;
		if( DString_EQ( field, name ) ) return i;
	}
	str.value = name;
	return DaoRoutine_AddConstant( self, (DaoValue*) s );
}
static void DaoRoutine_OriginalHost( void *p ){}
/*
// The layout of mixins in a host class:
// 1. Each mixin occupies a continuous segment in the data arrays of the host.
//    The ranges of the segments are stored in DaoClass::ranges;
// 2. If the mixin contains other mixins, those mixins occupy segments preceding
//    the segment for this mixin;
// 3. The segments for the direct mixins of the host are arranged in the same
//    order as the mixins;
//
// For example, there are the following mixins:
//    class AA { var x = 1 }
//    class BB { var x = 1 }
//    class CC ( AA, BB ) { var x = 1 }
//    class DD ( CC, AA ) { var x = 1 }
// The mixin layout for CC:
//    CC_Header, AA_Header, AA_Data, BB_Header, BB_Data, CC_Data
// The mixin layout for DD:
//    DD_Header, AA_Header, AA_Data, BB_Header, BB_Data, CC_Header, CC_Data, DD_Data
//
// Where XX_Header are the data fields that are always placed at the header
// of the data array. For example, XX_Header for class constants contains
// two fields: one for the class itself, the other for the class constructor(s);
// XX_Header for class static variables is empty; and XX_Header for class
// instance variables contains only the field for the "self" variable.
// And XX_Data constains the mixin's own data which are not from its
// component mixins or from its paraent classes (actually only classes
// without parent classes can be used as mixins).
//
//
// To mix a mixin in the host class, the mixin (and its component mixins if any)
// are properly arranged in the host class with layouts described above.
// The non-trivial part is the update of variable types and the methods
// that are added to the host class from the mixin. To update the types,
// the type for the mixin are all replaced with the type for the host class.
//
// The update of methods involves three major steps:
// 1. Update the routine signature type, local variable types and the static
//    variable types;
// 2. Update the lookup table of the host class for the data from the mixins,
//    which is done by mapping the indices for the mixin data arrays to the
//    indices for the host data arrays;
// 3. Update the method code (VM instructions) such that operands involving
//    class or class instance data are properly mapped from the indices for
//    the mixin data arrays to the indices for the host data arrays.
*/
static int DaoClass_MixIn( DaoClass *self, DaoClass *mixin, DMap *mixed, DaoMethodFields *mf )
{
	daoint i, j, k, id, bl = 1;
	DaoNamespace *ns = self->classRoutine->nameSpace;
	DArray *routines;
	DMap *deftypes;
	DMap *routmap;
	DMap *idmap;
	DNode *it;

	if( mixin->parent != NULL ) return 0;
	if( DMap_Find( mixed, mixin ) != NULL ) return 1;

	/* Mix the component mixins first: */
	for(i=0; i<mixin->mixinBases->size; ++i){
		DaoClass *mx = mixin->mixinBases->items.pClass[i];
		bl = bl && DaoClass_MixIn( self, mx, mixed, mf );
	}
	if( bl == 0 ) return 0;

	idmap = DMap_New(0,0);
	routmap = DMap_New(0,0);
	deftypes = DMap_New(0,0);
	routines = DArray_New(0);
	DMap_Insert( mixed, mixin, idmap );
	DMap_Delete( idmap );
	idmap = DMap_Find( mixed, mixin )->value.pMap;

	/* Add this mixin to the mixin list for both direct and indirect mixins: */
	DArray_Append( self->mixins, mixin );
	/* Save the starts of the ranges for this mixin in the host class: */
	DVector_PushUshort( self->ranges, self->constants->size );
	DVector_PushUshort( self->ranges, self->variables->size );
	DVector_PushUshort( self->ranges, self->instvars->size );

	/* For updating the types for the mixin to the types for the host class: */
	DMap_Insert( deftypes, mixin->clsType, self->clsType );
	DMap_Insert( deftypes, mixin->objType, self->objType );

#if 0
	printf( "MixIn: %s %p %i\n", mixin->className->chars, mixin, mixin->cstDataName->size );
#endif

	/* Add the own constants of the mixin to the host class: */
	for(i=0; i<mixin->cstDataName->size; ++i){
		daoint src = LOOKUP_BIND( DAO_CLASS_CONSTANT, 0, 0, i );
		daoint des = LOOKUP_BIND( DAO_CLASS_CONSTANT, 0, 0, self->constants->size );
		DString *name = mixin->cstDataName->items.pString[i];
		DaoValue *value = mixin->constants->items.pConst[i]->value;
		DaoRoutine *rout = (DaoRoutine*) value;

		if( i >= mixin->cstMixinStart && i < mixin->cstMixinEnd2 ) continue;

		MAP_Insert( idmap, src, des );  /* Setup index mapping; */
		DArray_Append( self->cstDataName, (void*) name );
		if( value->type != DAO_ROUTINE || rout->routHost != mixin->objType ){
			DaoConstant *cst = DaoConstant_New( value );
			DArray_Append( self->constants, cst );
			continue;
		}
		if( rout->overloads == NULL ){
			DaoRoutine *old = rout;
			DNode *it = DMap_Find( old->body->aux, DaoRoutine_OriginalHost );
			void *original2 = it ? it->value.pVoid : old->routHost;
			rout = DaoRoutine_Copy( rout, 1, 1, 1 );
			DMap_Insert( rout->body->aux, DaoRoutine_OriginalHost, original2 );
			bl = bl && DaoRoutine_Finalize( rout, self->objType, deftypes );
#if 0
			printf( "%2i:  %s  %s\n", i, rout->routName->chars, rout->routType->name->chars );
#endif

			/*
			// Do not use DaoClass_AddConst() here, so that the original
			// method overloading structures will be mantained, without
			// interference from methods of other mixin component classes
			// or of the host class.
			*/
			it = DMap_Find( routmap, old );
			if( it ) DRoutines_Add( it->value.pRoutine->overloads, rout );
			DArray_Append( self->constants, DaoConstant_New( (DaoValue*) rout ) );
			DArray_Append( routines, rout );
			if( bl == 0 ) goto Finalize;
		}else{
			/* No need to added the overloaded routines now; */
			/* Each of them has an entry in constants, and will be handled later: */
			DaoRoutine *routs = DaoRoutines_New( ns, self->objType, NULL );
			routs->trait |= DAO_VALUE_CONST;
			DArray_Append( self->constants, DaoConstant_New( (DaoValue*) routs ) );
			for(j=0; j<rout->overloads->routines->size; ++j){
				DaoRoutine *R = rout->overloads->routines->items.pRoutine[j];
				DMap_Insert( routmap, R, routs );
			}
		}
	}
	for(i=mixin->glbMixinEnd2; i<mixin->glbDataName->size; ++i){
		daoint src = LOOKUP_BIND( DAO_CLASS_VARIABLE, 0, 0, i );
		daoint des = LOOKUP_BIND( DAO_CLASS_VARIABLE, 0, 0, self->variables->size );
		DString *name = mixin->glbDataName->items.pString[i];
		DaoValue *var = mixin->variables->items.pVar[i]->value;
		DaoType *type = mixin->variables->items.pVar[i]->dtype;

		type = DaoType_DefineTypes( type, ns, deftypes );

		MAP_Insert( idmap, src, des );
		DArray_Append( self->glbDataName, (void*) name );
		DArray_Append( self->variables, DaoVariable_New( var, type ) );
	}
	for(i=mixin->objMixinEnd2; i<mixin->objDataName->size; ++i){
		daoint src = LOOKUP_BIND( DAO_OBJECT_VARIABLE, 0, 0, i );
		daoint des = LOOKUP_BIND( DAO_OBJECT_VARIABLE, 0, 0, self->instvars->size );
		DString *name = mixin->objDataName->items.pString[i];
		DaoValue *var = mixin->instvars->items.pVar[i]->value;
		DaoType *type = mixin->instvars->items.pVar[i]->dtype;

		type = DaoType_DefineTypes( type, ns, deftypes );

		MAP_Insert( idmap, src, des );
		DArray_Append( self->objDataName, (void*) name );
		DArray_Append( self->instvars, DaoVariable_New( var, type ) );
	}

	/* Find the ends of own data of this mixin: */
	DVector_PushUshort( self->ranges, self->constants->size );
	DVector_PushUshort( self->ranges, self->variables->size );
	DVector_PushUshort( self->ranges, self->instvars->size );

	/* Update the lookup table: */
	for(it=DMap_First(mixin->lookupTable); it; it=DMap_Next(mixin->lookupTable,it)){
		int pm = LOOKUP_PM( it->value.pInt );
		int st = LOOKUP_ST( it->value.pInt );
		int up = LOOKUP_UP( it->value.pInt );
		int id = LOOKUP_ID( it->value.pInt );
		DaoValue *cst;
		/* Skip names from component mixins (because they have been handled): */
		switch( st ){
		case DAO_CLASS_CONSTANT :
			if( id >= mixin->cstMixinStart && id < mixin->cstMixinEnd2 ) continue;
			break;
		case DAO_CLASS_VARIABLE :
			if( id >= mixin->glbMixinStart && id < mixin->glbMixinEnd2 ) continue;
			break;
		case DAO_OBJECT_VARIABLE :
			if( id >= mixin->objMixinStart && id < mixin->objMixinEnd2 ) continue;
			break;
		}
		if( st != DAO_OBJECT_VARIABLE || id != 0 ){ /* not a "self": */
			DNode *it2 = MAP_Find( idmap, LOOKUP_BIND( st, 0, 0, id ) );
			if( it2 ) id = LOOKUP_ID( it2->value.pInt ); /* map index; */
		}
		MAP_Insert( self->lookupTable, it->key.pString, LOOKUP_BIND( st, pm, up+1, id ) );
		if( st != DAO_CLASS_CONSTANT ) continue;
		cst = self->constants->items.pConst[id]->value;
		if( cst->type != DAO_ROUTINE ) continue;
		DArray_Append( mf->names, it->key.pString );
		DArray_Append( mf->perms, IntToPointer( pm ) );
		DArray_Append( mf->routines, cst );
	}

	for(i=0; i<routines->size; i++){
		DaoRoutine *rout = routines->items.pRoutine[i];
		DaoType **types;
		if( rout->body == NULL ) continue;
		//DaoRoutine_PrintCode( rout, rout->nameSpace->vmSpace->stdioStream );
		types = rout->body->regType->items.pType;
		for(j=0; j<rout->body->annotCodes->size; ++j){
			DaoVmCodeX *vmc = rout->body->annotCodes->items.pVmc[j];
			DaoClass *klass;
			DString *name;
			switch( vmc->code ){
			case DVM_GETCK:
			case DVM_GETCK_I: case DVM_GETCK_F:
			case DVM_GETCK_D: case DVM_GETCK_C:
				vmc->b = DaoClass_MapIndex( mixin, DAO_CLASS_CONSTANT, vmc->b, mixed );
				break;
			case DVM_GETVK:
			case DVM_GETVK_I: case DVM_GETVK_F:
			case DVM_GETVK_D: case DVM_GETVK_C:
			case DVM_SETVK:
			case DVM_SETVK_II: case DVM_SETVK_FF:
			case DVM_SETVK_DD: case DVM_SETVK_CC:
				vmc->b = DaoClass_MapIndex( mixin, DAO_CLASS_VARIABLE, vmc->b, mixed );
				break;
			case DVM_GETVO:
			case DVM_GETVO_I: case DVM_GETVO_F:
			case DVM_GETVO_D: case DVM_GETVO_C:
			case DVM_SETVO:
			case DVM_SETVO_II: case DVM_SETVO_FF:
			case DVM_SETVO_DD: case DVM_SETVO_CC:
				vmc->b = DaoClass_MapIndex( mixin, DAO_OBJECT_VARIABLE, vmc->b, mixed );
				break;
			case DVM_GETF_KC:
			case DVM_GETF_KCI: case DVM_GETF_KCF:
			case DVM_GETF_KCD: case DVM_GETF_KCC:
			case DVM_GETF_OC:
			case DVM_GETF_OCI: case DVM_GETF_OCF:
			case DVM_GETF_OCD: case DVM_GETF_OCC:
				klass = (DaoClass*) types[ vmc->a ]->aux;
				name  = DaoClass_GetDataName( klass, DAO_CLASS_CONSTANT, vmc->b );
				vmc->b = DaoRoutine_GetFieldIndex( rout, name );
				vmc->code = DVM_GETF;
				break;
			case DVM_GETF_KG:
			case DVM_GETF_KGI: case DVM_GETF_KGF:
			case DVM_GETF_KGD: case DVM_GETF_KGC:
			case DVM_GETF_OG:
			case DVM_GETF_OGI: case DVM_GETF_OGF:
			case DVM_GETF_OGD: case DVM_GETF_OGC:
				klass = (DaoClass*) types[ vmc->a ]->aux;
				name  = DaoClass_GetDataName( klass, DAO_CLASS_VARIABLE, vmc->b );
				vmc->b = DaoRoutine_GetFieldIndex( rout, name );
				vmc->code = DVM_GETF;
				break;
			case DVM_GETF_OV:
			case DVM_GETF_OVI: case DVM_GETF_OVF:
			case DVM_GETF_OVD: case DVM_GETF_OVC:
				klass = (DaoClass*) types[ vmc->a ]->aux;
				name  = DaoClass_GetDataName( klass, DAO_OBJECT_VARIABLE, vmc->b );
				vmc->b = DaoRoutine_GetFieldIndex( rout, name );
				vmc->code = DVM_GETF;
				break;
			case DVM_SETF_KG:
			case DVM_SETF_KGII: case DVM_SETF_KGFF:
			case DVM_SETF_KGDD: case DVM_SETF_KGCC:
			case DVM_SETF_OG:
			case DVM_SETF_OGII: case DVM_SETF_OGFF:
			case DVM_SETF_OGDD: case DVM_SETF_OGCC:
				klass = (DaoClass*) types[ vmc->c ]->aux;
				name  = DaoClass_GetDataName( klass, DAO_CLASS_VARIABLE, vmc->b );
				vmc->b = DaoRoutine_GetFieldIndex( rout, name );
				vmc->code = DVM_SETF;
				break;
			case DVM_SETF_OV:
			case DVM_SETF_OVII: case DVM_SETF_OVFF:
			case DVM_SETF_OVDD: case DVM_SETF_OVCC:
				klass = (DaoClass*) types[ vmc->c ]->aux;
				name  = DaoClass_GetDataName( klass, DAO_OBJECT_VARIABLE, vmc->b );
				vmc->b = DaoRoutine_GetFieldIndex( rout, name );
				vmc->code = DVM_SETF;
				break;
			}
		}
		//DaoRoutine_PrintCode( rout, rout->nameSpace->vmSpace->stdioStream );
		bl = bl && DaoRoutine_DoTypeInference( rout, 0 );
		if( bl == 0 ) goto Finalize;
	}
Finalize:
	DArray_Delete( routines );
	DMap_Delete( routmap );
	DMap_Delete( deftypes );
	return bl;
}
static void* DaoRoutine_GetOriginal2( DaoRoutine *self )
{
	DNode *it;
	if( self->body == NULL ) return self->routHost;
	it = DMap_Find( self->body->aux, DaoRoutine_OriginalHost );
	if( it ) return it->value.pVoid;
	return self->routHost;
}
static void DaoClass_UpdateConstructor( DaoClass *self, DaoRoutine *routine, DMap *updated )
{
	DArray *values;
	int i, modified = 0;
	if( routine->overloads ){
		for(i=0; i<routine->overloads->routines->size; ++i){
			DaoRoutine *rout = routine->overloads->routines->items.pRoutine[i];
			DaoClass_UpdateConstructor( self, rout, updated );
		}
		return;
	}
	if( !(routine->attribs & DAO_ROUT_INITOR) ) return;
	if( routine->routHost != self->objType ) return;
	if( routine->body == NULL ) return;
	if( DMap_Find( updated, DaoRoutine_GetOriginal2( routine ) ) != NULL ) return;

	DMap_Insert( updated, DaoRoutine_GetOriginal2( routine ), NULL );

	values = DArray_New(0);
	DArray_Resize( values, routine->body->regCount, NULL );
	for(i=0; i<routine->body->annotCodes->size; ++i){
		DaoVmCodeX *vmc = routine->body->annotCodes->items.pVmc[i];
		if( vmc->code == DVM_GETCK ){
			values->items.pValue[vmc->c] = self->constants->items.pConst[ vmc->b ]->value;;
		}else if( vmc->code == DVM_CALL && (vmc->b & DAO_CALL_INIT) ){
			DaoRoutine *callee = values->items.pRoutine[ vmc->a ];
			if( callee->overloads ){
				if( callee->overloads->routines->size == 0 ) continue;
				callee = callee->overloads->routines->items.pRoutine[0];
			}
			if( DMap_Find( updated, DaoRoutine_GetOriginal2( callee ) ) != NULL ){
				vmc->code = DVM_UNUSED;
				modified = 1;
				continue;
			}
			DaoClass_UpdateConstructor( self, callee, updated );
		}
	}
	DArray_Delete( values );
	if( modified ) DaoRoutine_DoTypeInference( routine, 1 );
}
void DaoClass_UpdateMixinConstructors( DaoClass *self )
{
	DMap *updated;
	daoint i;
	if( self->mixins->size == 0 ) return;
	updated = DMap_New(0,0);
	for(i=0; i<self->constants->size; ++i){
		DaoValue *cst = self->constants->items.pConst[i]->value;
		if( cst == NULL || cst->type != DAO_ROUTINE ) continue;
		DaoClass_UpdateConstructor( self, (DaoRoutine*) cst, updated );
	}
	DMap_Delete( updated );
}
static void DaoClass_SetupMethodFields( DaoClass *self, DaoMethodFields *mf )
{
	DaoValue *cst;
	DMap *overloads = DMap_New( DAO_DATA_STRING, 0 );
	DNode *it, *search;
	daoint i, id, pm, pm2;

	for(i=0; i<mf->names->size; ++i){
		DString *name = mf->names->items.pString[i];
		it = DMap_Find( self->lookupTable, name );
		if( it == NULL ) continue;
		if( LOOKUP_ST( it->value.pInt ) != DAO_CLASS_CONSTANT ) continue;

		id = LOOKUP_ID( it->value.pInt );
		DMap_Insert( overloads, name, self->constants->items.pConst[id]->value );
	}

	for(i=0; i<mf->names->size; ++i){
		DString *name = mf->names->items.pString[i];
		it = DMap_Find( self->lookupTable, name );
		if( it == NULL ) continue;
		if( LOOKUP_ST( it->value.pInt ) != DAO_CLASS_CONSTANT ) continue;

		cst = mf->routines->items.pValue[i];
		search = DMap_Find( overloads, name );
		if( cst == search->value.pValue ) continue;

		pm = LOOKUP_PM( it->value.pInt );
		pm2 = mf->perms->items.pInt[i];
		if( pm2 > pm ) pm = pm2;
		/*
		// Add again the overloaded methods, so that a new overloading structure
		// will be created for the host class. This is necessary to avoid messing
		// the function calls in the methods of the mixins.
		*/
		DaoClass_AddConst( self, name, cst, pm );
	}
	DMap_Delete( overloads );
}
int DaoCass_DeriveMixinData( DaoClass *self )
{
	DMap *mixed = DMap_New(0,DAO_DATA_MAP);
	DaoMethodFields *mf = DaoMethodFields_New();
	DNode *it, *search;
	daoint i, bl = 1;

	self->cstMixinStart = self->constants->size;
	self->glbMixinStart = self->variables->size;
	self->objMixinStart = self->instvars->size;

	for(i=0; i<self->mixinBases->size; ++i){
		DaoClass *mixin = self->mixinBases->items.pClass[i];
		bl &= DaoClass_MixIn( self, mixin, mixed, mf );
	}
	self->cstMixinEnd = self->constants->size;
	self->glbMixinEnd = self->variables->size;
	self->objMixinEnd = self->instvars->size;

	DaoClass_SetupMethodFields( self, mf );

	self->cstMixinEnd2 = self->constants->size;
	self->glbMixinEnd2 = self->variables->size;
	self->objMixinEnd2 = self->instvars->size;

	DMap_Delete( mixed );
	DaoMethodFields_Delete( mf );
	return bl;
}

/* assumed to be called before parsing class body */
int DaoClass_DeriveClassData( DaoClass *self )
{
	DaoType *type;
	DaoValue *value;
	DString *mbs;
	DNode *it, *search;
	DaoMethodFields *mf;
	daoint i, j, k, id;

	if( DaoCass_DeriveMixinData( self ) == 0 ) return 0;

	mbs = DString_New();
	mf = DaoMethodFields_New();

	if( self->clsType->bases == NULL ) self->clsType->bases = DArray_New( DAO_DATA_VALUE );
	if( self->objType->bases == NULL ) self->objType->bases = DArray_New( DAO_DATA_VALUE );
	DArray_Clear( self->clsType->bases );
	DArray_Clear( self->objType->bases );

	self->cstParentStart = self->constants->size;
	self->glbParentStart = self->variables->size;

	DVector_PushUshort( self->offsets, self->constants->size );
	DVector_PushUshort( self->offsets, self->variables->size );
	if( self->parent && self->parent->type == DAO_CLASS ){
		DaoClass *klass = (DaoClass*) self->parent;
		DArray_Append( self->clsType->bases, klass->clsType );
		DArray_Append( self->objType->bases, klass->objType );
		DArray_AppendArray( self->cstDataName, klass->cstDataName );
		DArray_AppendArray( self->glbDataName, klass->glbDataName );
		for(j=0; j<klass->constants->size; ++j){
			DaoValue *cst = klass->constants->items.pConst[j]->value;
			DArray_Append( self->constants, klass->constants->items.pVoid[j] );
		}
		for(j=0; j<klass->variables->size; ++j){
			DArray_Append( self->variables, klass->variables->items.pVoid[j] );
		}
		for(it=DMap_First(klass->lookupTable); it; it=DMap_Next(klass->lookupTable,it)){
			daoint pm = LOOKUP_PM( it->value.pInt );
			daoint st = LOOKUP_ST( it->value.pInt );
			daoint up = LOOKUP_UP( it->value.pInt );
			daoint id = LOOKUP_ID( it->value.pInt );
			DaoValue *cst;
			if( st == DAO_CLASS_CONSTANT ){
				id = LOOKUP_ID( it->value.pInt );
				cst = klass->constants->items.pConst[id]->value;
				if( cst->type == DAO_ROUTINE ){
					DArray_Append( mf->names, it->key.pString );
					DArray_Append( mf->perms, IntToPointer( pm ) );
					DArray_Append( mf->routines, cst );
				}
			}
			if( st == DAO_OBJECT_VARIABLE ) continue;
			if( pm == DAO_PERM_PRIVATE ) continue;
			if( DMap_Find( self->lookupTable, it->key.pString ) ) continue;
			switch( st ){
			case DAO_CLASS_CONSTANT : id += self->cstParentStart; break;
			case DAO_CLASS_VARIABLE : id += self->glbParentStart; break;
			case DAO_OBJECT_VARIABLE : continue;
			}
			id = LOOKUP_BIND( st, pm, up+1, id );
			DMap_Insert( self->lookupTable, it->key.pString, (void*)id );
		}
	}else if( self->parent && self->parent->type == DAO_CTYPE ){
		DaoCtype *cdata = (DaoCtype*) self->parent;
		DaoTypeKernel *kernel = cdata->ctype->kernel;
		DaoTypeBase *typer = kernel->typer;
		DMap *methods = kernel->methods;
		DMap *values = kernel->values;

		DArray_Append( self->clsType->bases, cdata->ctype );
		DArray_Append( self->objType->bases, cdata->cdtype );
		DaoClass_AddConst( self, cdata->ctype->name, (DaoValue*)cdata, DAO_PERM_PUBLIC );

		if( kernel->SetupValues ) kernel->SetupValues( kernel->nspace, kernel->typer );
		if( kernel->SetupMethods ) kernel->SetupMethods( kernel->nspace, kernel->typer );

		DaoType_SpecializeMethods( cdata->ctype );
		kernel = cdata->ctype->kernel;
		values = kernel->values;
		methods = kernel->methods;

		for(it=DMap_First(values); it; it=DMap_Next(values, it)){
			if( DMap_Find( self->lookupTable, it->key.pString ) ) continue;
			id = self->constants->size;
			id = LOOKUP_BIND( DAO_CLASS_CONSTANT, DAO_PERM_PUBLIC, 1, id );
			DMap_Insert( self->lookupTable, it->key.pString, IntToPointer( id ) );
			DArray_Append( self->cstDataName, it->key.pString );
			DArray_Append( self->constants, DaoConstant_New( it->value.pValue ) );
		}
		for(it=DMap_First( methods ); it; it=DMap_Next( methods, it )){
			if( DMap_Find( self->lookupTable, it->key.pString ) ) continue;
			id = self->constants->size;
			id = LOOKUP_BIND( DAO_CLASS_CONSTANT, DAO_PERM_PUBLIC, 1, id );
			DMap_Insert( self->lookupTable, it->key.pString, IntToPointer( id ) );
			DArray_Append( self->cstDataName, it->key.pString );
			DArray_Append( self->constants, DaoConstant_New( it->value.pValue ) );

			DArray_Append( mf->names, it->key.pString );
			DArray_Append( mf->perms, IntToPointer( DAO_PERM_PUBLIC ) );
			DArray_Append( mf->routines, it->value.pValue );
		}
	}
	DaoClass_SetupMethodFields( self, mf );
	DaoMethodFields_Delete( mf );

	self->cstParentEnd = self->constants->size;
	self->glbParentEnd = self->variables->size;

#if 0
	for(j=0; j<self->constants->size; ++j){
		DaoValue *value = self->constants->items.pConst[j]->value;
		DaoRoutine *routine = (DaoRoutine*) value;
		printf( "%3i: %3i %s\n", j, value->type, self->cstDataName->items.pString[j]->chars );
		if( value->type != DAO_ROUTINE ) continue;
		printf( "%3i: %3i %s\n", j, value->type, routine->routName->chars );
		if( routine->overloads ){
			DArray *routs = routine->overloads->routines;
			for(k=0; k<routs->size; ++k){
				DaoRoutine *rout = routs->items.pRoutine[k];
			}
		}else{
			if( routine->attribs & DAO_ROUT_PRIVATE ) continue;
		}
	}
	for(it=DMap_First(self->lookupTable); it; it=DMap_Next(self->lookupTable,it)){
		printf( "%s %i\n", it->key.pString->chars, it->value.pInt );
		if( LOOKUP_ST( it->value.pInt ) != DAO_CLASS_CONSTANT ) continue;
		DaoValue *value = DaoClass_GetConst( self, it->value.pInt );
		printf( "%i\n", value->type );
	}
#endif

	DString_Delete( mbs );
	return 1;
}
/* assumed to be called after parsing class body */
void DaoClass_DeriveObjectData( DaoClass *self )
{
	DaoType *type;
	DaoValue *value;
	DArray *parents, *offsets;
	DString *mbs;
	DNode *search;
	daoint i, id, perm, index, offset = 0;

	self->objDefCount = self->objDataName->size;
	offset = self->objDataName->size;
	mbs = DString_New();

	parents = DArray_New(0);
	offsets = DArray_New(0);
	DaoClass_Parents( self, parents, offsets );
	if( self->parent && self->parent->type == DAO_CLASS ){
		DaoClass *klass = (DaoClass*) self->parent;
		/* for properly arrangement object data: */
		for( id=0; id<klass->objDataName->size; id ++ ){
			DString *name = klass->objDataName->items.pString[id];
			DaoVariable *var = klass->instvars->items.pVar[id];
			var = DaoVariable_New( var->value, var->dtype );
			DArray_Append( self->objDataName, name );
			DArray_Append( self->instvars, var );
			DaoValue_MarkConst( (DaoValue*) var->value );
		}
	}
	for(i=1; i<parents->size; i++){
		DaoClass *klass = parents->items.pClass[i];
		offset = offsets->items.pInt[i]; /* plus self */
		if( klass->type == DAO_CLASS ){
			/* For object data: */
			for( id=0; id<klass->objDataName->size; id ++ ){
				DString *name = klass->objDataName->items.pString[id];
				DNode *search2, *search = MAP_Find( klass->lookupTable, name );
				int perm = LOOKUP_PM( search->value.pInt );
				int idx = LOOKUP_ID( search->value.pInt );
				/* NO deriving private member: */
				if( perm <= DAO_PERM_PRIVATE ) continue;
				search2 = MAP_Find( self->lookupTable, name );
				if( search2 == NULL ){ /* To not overide data and routine: */
					index = LOOKUP_BIND( DAO_OBJECT_VARIABLE, perm, i, (offset+idx) );
					MAP_Insert( self->lookupTable, name, index );
				}
			}
		}
	}

	self->derived = 1;
	DString_Delete( mbs );
	DArray_Delete( parents );
	DArray_Delete( offsets );
}
int DArray_MatchAffix( DArray *self, DString *name )
{
	daoint i, pos;
	if( self == NULL ) return 0;
	for(i=0; i<self->size; ++i){
		DString tmp, *pat = self->items.pString[i];
		daoint pos = DString_FindChar( pat, '~', 0 );
		if( pos < 0 ){
			if( DString_EQ( pat, name ) ) return 1;
			continue;
		}
		if( pos ){
			tmp = *pat;
			tmp.size = pos;
			if( DString_Find( name, & tmp, 0 ) != 0 ) continue;
		}
		if( pos < pat->size-1 ){
			tmp = DString_WrapChars( pat->chars + pos + 1 );
			if( DString_RFind( name, & tmp, -1 ) != (name->size - 1) ) continue;
		}
		return 1;
	}
	return 0;
}
int DaoClass_UseMixinDecorators( DaoClass *self )
{
	int bl = 1;
#ifdef DAO_WITH_DECORATOR
	daoint i, j, k;
	DaoObject object = *(DaoObject*) self->objType->value;
	DaoObject *obj = & object;

	/*
	// Apply the decorators from mixins only to the methods defined in this class.
	// Two reasons for doing this:
	// 1. Mixins are only presented once in the current class, so when the mixins
	//    are composed of mixins, they are flatten in the current class.
	//    The order in which they are arranged in the current class is not obvious,
	//    if the decorators are allowed to decorate the methods from mixins, the
	//    result may be quite confusing;
	// 2. If the methods from mixins are allowed to be decorated, such decoration
	//    will not be written to bytecode file. Because when a class is written
	//    to a bytecode file, only its own data are encoded and saved (this is
	//    necessary to properly handle module loading). As a result, when a class
	//    is loaded from a bytecode file, it will obtain an un-decorated version
	//    of the methods from the mixins.
	*/
	for(j=self->cstMixinEnd-1; j>=self->cstMixinStart; --j){
		DaoRoutine *deco = (DaoRoutine*) self->constants->items.pConst[j]->value;
		DString *decoName = deco->routName;

		if( deco->type != DAO_ROUTINE || deco->body == NULL ) continue;
		if( !(deco->attribs & DAO_ROUT_DECORATOR) ) continue; /* Not a decorator; */
		if( deco->body->decoTargets == NULL || deco->body->decoTargets->size == 0 ) continue;

		for(k=self->cstParentEnd; k<self->constants->size; ++k){
			DaoValue *cst = self->constants->items.pConst[k]->value;
			DaoRoutine *rout = (DaoRoutine*) cst;
			DaoRoutine *deco2;

			if( rout->type != DAO_ROUTINE || rout->body == NULL ) continue;
			if( rout->attribs & (DAO_ROUT_CODESECT|DAO_ROUT_DECORATOR) ) continue;
			if( rout->routHost != self->objType ) continue;

			deco2 = DaoRoutine_ResolveX( deco, (DaoValue*) obj, NULL, & cst, NULL, 1, 0 );
			if( deco2 == NULL ) continue;
			if( DArray_MatchAffix( deco2->body->decoTargets, rout->routName ) == 0 ) continue;
			bl = bl && DaoRoutine_Decorate( rout, deco2, & cst, 1, 1 ) != NULL;
			if( bl == 0 ) break;
		}
		if( bl == 0 ) break;
	}
#endif
	return bl;
}
void DaoClass_MakeInterface( DaoClass *self )
{
	daoint i, j;
	DaoType *tp;
	DaoRoutine *meth;
	DaoInterface *clsInter = self->clsInter;
	DaoInterface *objInter = self->objInter;
	DMap *deftypes = DHash_New(0,0);

	DArray_Clear( self->objInter->supers );
	DMap_Clear( self->objInter->methods );

	if( self->parent ){
		if( clsInter->abtype->bases == NULL ) clsInter->abtype->bases = DArray_New( DAO_DATA_VALUE );
		if( objInter->abtype->bases == NULL ) objInter->abtype->bases = DArray_New( DAO_DATA_VALUE );
		if( self->parent && self->parent->type == DAO_CLASS ){
			DArray_Append( clsInter->supers, self->parent->xClass.clsInter );
			DArray_Append( objInter->supers, self->parent->xClass.objInter );
			DArray_Append( clsInter->abtype->bases, self->parent->xClass.clsInter->abtype );
			DArray_Append( objInter->abtype->bases, self->parent->xClass.objInter->abtype );
		}else if( self->parent && self->parent->type == DAO_CTYPE ){
			DArray_Append( clsInter->supers, self->parent->xCtype.clsInter );
			DArray_Append( objInter->supers, self->parent->xCtype.objInter );
			DArray_Append( clsInter->abtype->bases, self->parent->xCtype.clsInter->abtype );
			DArray_Append( objInter->abtype->bases, self->parent->xCtype.objInter->abtype );
		}
	}

	for(i=0; i<self->cstDataName->size; ++i){
		DString *name = self->cstDataName->items.pString[i];
		DaoValue *value = self->constants->items.pConst[i]->value;
		DaoRoutine *rout = (DaoRoutine*) value;
		DNode *it;

		if( value->type != DAO_ROUTINE ) continue;
		if( value->xRoutine.attribs & DAO_ROUT_DECORATOR ) continue;

		it = MAP_Find( self->lookupTable, rout->routName );
		if( it == NULL || LOOKUP_PM( it->value.pInt ) != DAO_PERM_PUBLIC ) continue;

		DaoInterface_CopyMethod( self->clsInter, rout, deftypes ); /* TODO: handle error; */
		DaoInterface_CopyMethod( self->objInter, rout, deftypes ); /* TODO: handle error; */
	}
	DMap_Delete( deftypes );
}
void DaoClass_ResetAttributes( DaoClass *self )
{
	DNode *node;
	DString *mbs = DString_New();
	int i, k, id, autoinitor = self->parent == NULL;

	DaoObject_Init( & self->objType->value->xObject, NULL, 0 );
	self->objType->value->xObject.trait &= ~DAO_VALUE_CONST;
	DaoValue_MarkConst( self->objType->value );
	DaoValue_MarkConst( self->constants->items.pConst[1]->value ); /* ::default */

	for(i=0; autoinitor && (i<self->classRoutines->overloads->routines->size); i++){
		DaoRoutine *rout = self->classRoutines->overloads->routines->items.pRoutine[i];
		if( rout == self->classRoutine ) continue;
		if( !(rout->attribs & DAO_ROUT_INITOR) ) continue;
		if( rout->routHost != self->objType ) continue;
		autoinitor = 0;
	}
	if( autoinitor ) self->attribs |= DAO_CLS_AUTO_INITOR;
#if 0
	printf( "%s %i\n", self->className->chars, autoinitor );
#endif
	for(i=DVM_NOT; i<=DVM_BITRIT; i++){
		DString_SetChars( mbs, daoBitBoolArithOpers[i-DVM_NOT] );
		node = DMap_Find( self->lookupTable, mbs );
		if( node == NULL ) continue;
		if( LOOKUP_ST( node->value.pInt ) != DAO_CLASS_CONSTANT ) continue;
		id = LOOKUP_ID( node->value.pInt );
		k = self->constants->items.pConst[id]->value->type;
		if( k != DAO_ROUTINE ) continue;
		self->attribs |= DAO_OPER_OVERLOADED;
		break;
	}
	DString_Delete( mbs );
}

int DaoClass_ChildOf( DaoClass *self, DaoValue *other )
{
	if( other->type == DAO_CLASS ){
		return DaoType_ChildOf( self->clsType, other->xClass.clsType );
	}else if( other->type == DAO_CTYPE ){
		return DaoType_ChildOf( self->clsType, other->xCtype.ctype );
	}
	return 0;
}
DaoValue* DaoClass_CastToBase( DaoClass *self, DaoType *parent )
{
	DaoValue *sup;
	if( parent == NULL ) return NULL;
	if( self->clsType == parent ) return (DaoValue*) self;
	if( self->parent == NULL ) return NULL;
	if( self->parent->type == DAO_CLASS ){
		if( (sup = DaoClass_CastToBase( (DaoClass*) self->parent, parent ) ) ) return sup;
	}else if( self->parent->type == DAO_CTYPE && parent->tid == DAO_CTYPE ){
		if( (sup = DaoType_CastToParent( self->parent, parent ) ) ) return sup;
	}
	return NULL;
}
void DaoClass_AddMixinClass( DaoClass *self, DaoClass *mixin )
{
	DArray_Append( self->allBases, mixin );
	DArray_Append( self->mixinBases, mixin );
}
void DaoClass_AddSuperClass( DaoClass *self, DaoValue *super )
{
	if( self->parent ){
		printf( "Error: parent class alread set!\n" );
		return;
	}
	self->parent = super;
	DArray_Append( self->allBases, super );
}
int  DaoClass_FindConst( DaoClass *self, DString *name )
{
	DNode *node = MAP_Find( self->lookupTable, name );
	if( node == NULL || LOOKUP_ST( node->value.pInt ) != DAO_CLASS_CONSTANT ) return -1;
	return node->value.pInt;
}
DaoValue* DaoClass_GetConst( DaoClass *self, int id )
{
	id = LOOKUP_ID( id );
	if( id >= self->constants->size ) return NULL;
	return self->constants->items.pConst[id]->value;
}
void DaoClass_SetConst( DaoClass *self, int id, DaoValue *data )
{
	id = LOOKUP_ID( id );
	if( id >= self->constants->size ) return;
	DaoValue_Copy( data, & self->constants->items.pConst[id]->value );
	DaoValue_MarkConst( self->constants->items.pConst[id]->value );
}
int DaoClass_GetData( DaoClass *self, DString *name, DaoValue **value, DaoClass *thisClass )
{
	DaoValue *p = NULL;
	DNode *node = MAP_Find( self->lookupTable, name );
	int child = thisClass && DaoClass_ChildOf( thisClass, (DaoValue*)self );
	int sto, perm, up, id;

	*value = NULL;
	if( ! node ) return DAO_ERROR_FIELD_NOTEXIST;
	perm = LOOKUP_PM( node->value.pInt );
	sto = LOOKUP_ST( node->value.pInt );
	up = LOOKUP_UP( node->value.pInt );
	id = LOOKUP_ID( node->value.pInt );
	if( self == thisClass || perm == DAO_PERM_PUBLIC || (child && perm >= DAO_PERM_PROTECTED) ){
		switch( sto ){
		case DAO_CLASS_VARIABLE : p = self->variables->items.pVar[id]->value; break;
		case DAO_CLASS_CONSTANT : p = self->constants->items.pConst[id]->value; break;
		default : return DAO_ERROR_FIELD;
		}
		if( p ) *value = p;
	}else{
		return DAO_ERROR_FIELD_NOTPERMIT;
	}
	return 0;
}
DaoType** DaoClass_GetDataType( DaoClass *self, DString *name, int *res, DaoClass *thisClass )
{
	DNode *node = MAP_Find( self->lookupTable, name );
	int child = thisClass && DaoClass_ChildOf( thisClass, (DaoValue*)self );
	int sto, perm, up, id;

	*res = DAO_ERROR_FIELD_NOTEXIST;
	if( ! node ) return NULL;

	*res = 0;
	perm = LOOKUP_PM( node->value.pInt );
	sto = LOOKUP_ST( node->value.pInt );
	up = LOOKUP_UP( node->value.pInt );
	id = LOOKUP_ID( node->value.pInt );
	if( self == thisClass || perm == DAO_PERM_PUBLIC || (child && perm >=DAO_PERM_PROTECTED) ){
		switch( sto ){
		case DAO_OBJECT_VARIABLE : return & self->instvars->items.pVar[id]->dtype;
		case DAO_CLASS_VARIABLE  : return & self->variables->items.pVar[id]->dtype;
		case DAO_CLASS_CONSTANT  : return NULL;
		default : break;
		}
	}
	*res = DAO_ERROR_FIELD_NOTPERMIT;
	return NULL;
}
int DaoClass_GetDataIndex( DaoClass *self, DString *name )
{
	DNode *node = MAP_Find( self->lookupTable, name );
	if( ! node ) return -1;
	return node->value.pInt;
}
int DaoClass_AddObjectVar( DaoClass *self, DString *name, DaoValue *deft, DaoType *t, int s )
{
	int id;
	DNode *node = MAP_Find( self->lookupTable, name );
	if( node && LOOKUP_UP( node->value.pInt ) == 0 ) return -DAO_CTW_WAS_DEFINED;
	if( deft == NULL && t ) deft = t->value;

	id = self->objDataName->size;
	if( id != 0 ){ /* not self; */
		if( s == DAO_PERM_PRIVATE   ) self->attribs |= DAO_CLS_PRIVATE_VAR;
		if( s == DAO_PERM_PROTECTED ) self->attribs |= DAO_CLS_PROTECTED_VAR;
	}
	MAP_Insert( self->lookupTable, name, LOOKUP_BIND( DAO_OBJECT_VARIABLE, s, 0, id ) );
	DArray_Append( self->objDataName, (void*)name );
	DArray_Append( self->instvars, DaoVariable_New( deft, t ) );
	DaoValue_MarkConst( self->instvars->items.pVar[ id ]->value );
	return id;
}
static void DaoClass_AddConst3( DaoClass *self, DString *name, DaoValue *data )
{
	DaoConstant *cst = DaoConstant_New( data );
	DArray_Append( self->cstDataName, (void*)name );
	DArray_Append( self->constants, cst );
	DaoValue_MarkConst( cst->value );
}
static int DaoClass_AddConst2( DaoClass *self, DString *name, DaoValue *data, int s )
{
	int id = LOOKUP_BIND( DAO_CLASS_CONSTANT, s, 0, self->constants->size );
	DaoNamespace *ns = self->classRoutine->nameSpace;
	if( data->type == DAO_ROUTINE && data->xRoutine.routHost != self->objType ){
		if( data->xRoutine.overloads ){
			DaoRoutine *routs = DaoRoutines_New( ns, self->objType, (DaoRoutine*) data );
			data = (DaoValue*) routs;
		}
	}
	MAP_Insert( self->lookupTable, name, id );
	DaoClass_AddConst3( self, name, data );
	return id;
}
int DaoClass_AddConst( DaoClass *self, DString *name, DaoValue *data, int s )
{
	int fromMixin = 0;
	int fromParent = 0;
	int sto, pm, up, id;
	DNode *node = MAP_Find( self->lookupTable, name );
	DaoNamespace *ns = self->classRoutine->nameSpace;
	DaoConstant *dest;
	DaoValue *value;

	if( node ){
		id = LOOKUP_ID( node->value.pInt );
		fromParent = LOOKUP_UP( node->value.pInt ); /* From parent classes; */
		switch( LOOKUP_ST( node->value.pInt ) ){ /* Check if it is from mixins; */
		case DAO_CLASS_CONSTANT :
			fromMixin = id >= self->cstMixinStart && id < self->cstMixinEnd;
			break;
		case DAO_CLASS_VARIABLE :
			fromMixin = id >= self->glbMixinStart && id < self->glbMixinEnd;
			break;
		case DAO_OBJECT_VARIABLE :
			fromMixin = id >= self->objMixinStart && id < self->objMixinEnd;
			break;
		}
	}

	assert( data != NULL );
	if( fromParent || fromMixin ){ /* inherited field: */
		sto = LOOKUP_ST( node->value.pInt );
		pm = LOOKUP_PM( node->value.pInt );
		id = LOOKUP_ID( node->value.pInt );
		if( sto != DAO_CLASS_CONSTANT ){ /* override inherited variable: */
			DMap_EraseNode( self->lookupTable, node );
			return DaoClass_AddConst( self, name, data, s );
		}
		node->value.pInt = LOOKUP_BIND( sto, pm, 0, id );
		dest = self->constants->items.pConst[id];
		if( dest->value->type == DAO_ROUTINE && data->type == DAO_ROUTINE ){
			/* Add the inherited routine(s) for overloading: */
			DaoRoutine *routs = DaoRoutines_New( ns, self->objType, (DaoRoutine*)dest->value );
			DaoConstant *cst = DaoConstant_New( (DaoValue*) routs );
			routs->trait |= DAO_VALUE_CONST;
			node->value.pInt = LOOKUP_BIND( sto, pm, 0, self->constants->size );
			DArray_Append( self->cstDataName, (void*) name );
			DArray_Append( self->constants, cst );
			return DaoClass_AddConst( self, name, data, s );
		}else{
			/* Add the new constant: */
			DaoConstant *cst = DaoConstant_New( data );
			node->value.pInt = LOOKUP_BIND( sto, pm, 0, self->constants->size );
			DArray_Append( self->cstDataName, (void*) name );
			DArray_Append( self->constants, cst );
			return node->value.pInt;
		}
	}else if( node ){
		sto = LOOKUP_ST( node->value.pInt );
		pm = LOOKUP_PM( node->value.pInt );
		id = LOOKUP_ID( node->value.pInt );
		if( sto != DAO_CLASS_CONSTANT ) return -DAO_CTW_WAS_DEFINED;
		dest = self->constants->items.pConst[id];
		value = dest->value;
		if( value->type != DAO_ROUTINE || data->type != DAO_ROUTINE ) return -DAO_CTW_WAS_DEFINED;
		if( s > pm ) node->value.pInt = LOOKUP_BIND( sto, s, 0, id );
		if( value->xRoutine.overloads == NULL || value->xRoutine.routHost != self->objType ){
			DaoRoutine *routs = DaoRoutines_New( ns, self->objType, (DaoRoutine*) value );
			routs->trait |= DAO_VALUE_CONST;
			/* Add individual entry for the existing function: */
			if( value->xRoutine.routHost == self->objType ) DaoClass_AddConst3( self, name, value );
			GC_ShiftRC( routs, dest->value );
			dest->value = (DaoValue*) routs;
		}
		if( data->xRoutine.overloads ){
			DaoRoutines_Import( (DaoRoutine*) dest->value, data->xRoutine.overloads );
		}else{
			DaoRoutine *rout = (DaoRoutine*) data;
			DRoutines_Add( dest->value->xRoutine.overloads, rout );
			/* Add individual entry for the new function: */
			if( data->xRoutine.routHost == self->objType ) DaoClass_AddConst3( self, name, data );
		}
		return node->value.pInt;
	}

	node = MAP_Find( self->lookupTable, name );
	if( node && LOOKUP_UP( node->value.pInt ) ) return -DAO_CTW_WAS_DEFINED;
	return DaoClass_AddConst2( self, name, data, s );
}
int DaoClass_AddGlobalVar( DaoClass *self, DString *name, DaoValue *data, DaoType *t, int s )
{
	int size = self->variables->size;
	int id = LOOKUP_BIND( DAO_CLASS_VARIABLE, s, 0, size );
	DNode *node = MAP_Find( self->lookupTable, name );
	if( node && LOOKUP_UP( node->value.pInt ) ) return -DAO_CTW_WAS_DEFINED;
	if( data == NULL && t ) data = t->value;
	MAP_Insert( self->lookupTable, name, id );
	DArray_Append( self->variables, DaoVariable_New( NULL, t ) );
	DArray_Append( self->glbDataName, (void*)name );
	if( data && DaoValue_Move( data, & self->variables->items.pVar[size]->value, t ) ==0 )
		return -DAO_TYPE_NOT_MATCHING;
	return id;
}
int DaoClass_AddType( DaoClass *self, DString *name, DaoType *tp )
{
	DNode *node = MAP_Find( self->abstypes, name );
	if( node == NULL ) MAP_Insert( self->abstypes, name, tp );
	return 1;
}
void DaoClass_AddOverloadedRoutine( DaoClass *self, DString *signature, DaoRoutine *rout )
{
	MAP_Insert( self->ovldRoutMap, signature, rout );
}
DaoRoutine* DaoClass_GetOverloadedRoutine( DaoClass *self, DString *signature )
{
	DNode *node = MAP_Find( self->ovldRoutMap, signature );
	if( node ) return (DaoRoutine*) node->value.pValue;
	return NULL;
}
void DaoClass_PrintCode( DaoClass *self, DaoStream *stream )
{
	daoint i;
	DaoStream_WriteChars( stream, "class " );
	DaoStream_WriteString( stream, self->className );
	DaoStream_WriteChars( stream, ":\n" );
	for(i=0; i<self->constants->size; ++i){
		DaoValue *cst = self->constants->items.pConst[i]->value;
		if( cst->type != DAO_ROUTINE || cst->xRoutine.body == NULL ) continue;
		if( cst->xRoutine.routHost != self->objType ) continue;
		DaoRoutine_PrintCode( & cst->xRoutine, stream );
	}
}
DaoRoutine* DaoClass_FindOperator( DaoClass *self, const char *oper, DaoClass *scoped )
{
	DaoValue *V = NULL;
	DString name = DString_WrapChars( oper );
	DaoClass_GetData( self, & name, & V, scoped );
	if( V == NULL || V->type != DAO_ROUTINE ) return NULL;
	return (DaoRoutine*) V;
}
