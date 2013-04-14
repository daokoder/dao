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

#include"assert.h"
#include"string.h"
#include"daoConst.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoRoutine.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoStream.h"
#include"daoNumtype.h"
#include"daoValue.h"
#include"daoNamespace.h"

static void DaoClass_GetField( DaoValue *self0, DaoProcess *proc, DString *name )
{
	int tid = proc->activeRoutine->routHost ? proc->activeRoutine->routHost->tid : 0;
	DaoType *type = proc->activeRoutine->routHost;
	DaoClass *host = tid == DAO_OBJECT ? & type->aux->xClass : NULL;
	DaoClass *self = & self0->xClass;
	DString *mbs = DString_New(1);
	DaoValue *value = NULL;
	int rc = DaoClass_GetData( self, name, & value, host );
	if( rc ){
		DString_SetMBS( mbs, DString_GetMBS( self->className ) );
		DString_AppendMBS( mbs, "." );
		DString_Append( mbs, name );
		DaoProcess_RaiseException( proc, rc, mbs->mbs );
	}else{
		DaoProcess_PutReference( proc, value );
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
			DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "not matched" );
	}else{
		/* XXX permission */
		DaoProcess_RaiseException( proc, DAO_ERROR_FIELD, "not exist" );
	}
}
static void DaoClass_GetItem( DaoValue *self0, DaoProcess *proc, DaoValue *ids[], int N )
{
}
static void DaoClass_SetItem( DaoValue *self0, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
}
static DaoValue* DaoClass_Copy(  DaoValue *self, DaoProcess *proc, DMap *cycData )
{
	return self;
}

static DaoTypeCore classCore=
{
	NULL,
	DaoClass_GetField,
	DaoClass_SetField,
	DaoClass_GetItem,
	DaoClass_SetItem,
	DaoValue_Print,
	DaoClass_Copy
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
	self->className = DString_New(1);

	self->lookupTable = DHash_New(D_STRING,0);
	self->ovldRoutMap = DHash_New(D_STRING,0);
	self->abstypes    = DMap_New(D_STRING,D_VALUE);
	self->constants   = DArray_New(D_VALUE);
	self->variables   = DArray_New(D_VALUE);
	self->instvars    = DArray_New(D_VALUE);
	self->objDataName = DArray_New(D_STRING);
	self->cstDataName = DArray_New(D_STRING);
	self->glbDataName = DArray_New(D_STRING);
	self->superClass  = DArray_New(D_VALUE);
	self->references  = DArray_New(D_VALUE);
	return self;
}
void DaoClass_Delete( DaoClass *self )
{
	GC_DecRC( self->clsType );
	DMap_Delete( self->abstypes );
	DMap_Delete( self->lookupTable );
	DMap_Delete( self->ovldRoutMap );
	DArray_Delete( self->constants );
	DArray_Delete( self->variables );
	DArray_Delete( self->instvars );
	DArray_Delete( self->objDataName );
	DArray_Delete( self->cstDataName );
	DArray_Delete( self->glbDataName );
	DArray_Delete( self->superClass );
	DArray_Delete( self->references );
	if( self->vtable ) DMap_Delete( self->vtable );
	if( self->protoValues ) DMap_Delete( self->protoValues );
#ifdef DAO_WITH_DYNCLASS
	if( self->typeHolders ){
		DArray_Delete( self->typeHolders );
		DArray_Delete( self->typeDefaults );
		DMap_Delete( self->instanceClasses );
	}
#endif

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
void DaoValue_Update( DaoValue **self, DaoNamespace *ns, DMap *deftypes )
{
	DaoValue *value = *self;
	DaoType *tp, *tp2;

	if( value == NULL || value->type < DAO_ENUM ) return;
	tp = DaoNamespace_GetType( ns, value );
	/* DaoType_DefineTypes() will make proper specialization of template-like type: */
	tp2 = DaoType_DefineTypes( tp, ns, deftypes );
	if( tp == tp2 ) return;
	if( tp2->tid == DAO_OBJECT && value->type == DAO_OBJECT && value->xObject.isDefault ){
		/* "self" is supposed to be a constant, so it has to be a default instance: */
		GC_ShiftRC( tp2->value, value ); /* default instance of specialized Dao class; */
		*self = tp2->value;
		return;
	}else if( tp2->tid == DAO_CLASS && value->type == DAO_CLASS ){
		GC_ShiftRC( tp2->aux, value ); /* specialized Dao class; */
		*self = tp2->aux;
		return;
	}else if( tp2->tid == DAO_CTYPE && value->type == DAO_CTYPE ){
		GC_ShiftRC( tp2->aux, value ); /* specialized C type; */
		*self = tp2->aux;
		return;
	}else if( tp2->tid == DAO_CDATA && value->type == DAO_CDATA ){
		GC_ShiftRC( tp2->value, value ); /* default instance of specialized C type; */
		*self = tp2->value;
		return;
	}else if( tp2->tid == DAO_CSTRUCT && value->type == DAO_CSTRUCT ){
		GC_ShiftRC( tp2->value, value ); /* default instance of specialized C type; */
		*self = tp2->value;
		return;
	}
	DaoValue_Move( value, self, tp2 );
}

#ifdef DAO_WITH_DYNCLASS
int DaoClass_CopyField( DaoClass *self, DaoClass *other, DMap *deftypes )
{
	DaoNamespace *ns = other->classRoutine->nameSpace;
	DaoType *tp;
	DArray *offsets = DArray_New(0);
	DArray *routines = DArray_New(0);
	DNode *it;
	int i, k, st, up, id;

	for(i=0; i<other->superClass->size; i++){
		DaoValue *sup = other->superClass->items.pValue[i];
		if( sup->type == DAO_CLASS && sup->xClass.typeHolders ){
			tp = DaoType_DefineTypes( sup->xClass.objType, ns, deftypes );
			DArray_Append( self->superClass, tp->aux );
		}else if( sup->type == DAO_CTYPE && sup->xCtype.ctype->typer->core->kernel->sptree ){
			tp = DaoType_DefineTypes( sup->xCtype.ctype, ns, deftypes );
			DArray_Append( self->superClass, tp->aux );
		}else{
			DArray_Append( self->superClass, sup );
		}
	}

	DaoRoutine_CopyFields( self->classRoutine, other->classRoutine, 1, 1 );
	for(it=DMap_First(other->lookupTable);it;it=DMap_Next(other->lookupTable,it)){
		st = LOOKUP_ST( it->value.pInt );
		up = LOOKUP_UP( it->value.pInt );
		id = LOOKUP_ID( it->value.pInt );
		if( up ==0 ){
			if( st == DAO_CLASS_CONSTANT && id <self->constants->size ) continue;
			if( st == DAO_CLASS_VARIABLE && id <self->variables->size ) continue;
			if( st == DAO_OBJECT_VARIABLE && id <self->instvars->size ) continue;
		}
		DMap_Insert( self->lookupTable, it->key.pVoid, it->value.pVoid );
	}
	for(i=self->objDataName->size; i<other->objDataName->size; i++)
		DArray_Append( self->objDataName, other->objDataName->items.pString[i] );
	for(i=self->cstDataName->size; i<other->cstDataName->size; i++)
		DArray_Append( self->cstDataName, other->cstDataName->items.pString[i] );
	for(i=self->glbDataName->size; i<other->glbDataName->size; i++)
		DArray_Append( self->glbDataName, other->glbDataName->items.pString[i] );
	for(i=self->variables->size; i<other->variables->size; i++){
		DaoVariable *var = other->variables->items.pVar[i];
		var = DaoVariable_New( var->value, DaoType_DefineTypes( var->dtype, ns, deftypes ) );
		DArray_Append( self->variables, var );
	}
	for(i=self->instvars->size; i<other->instvars->size; i++){
		DaoVariable *var = other->instvars->items.pVar[i];
		var = DaoVariable_New( var->value, DaoType_DefineTypes( var->dtype, ns, deftypes ) );
		DArray_Append( self->instvars, var );
		/* TODO fail checking */
	}
	for(i=self->constants->size; i<other->constants->size; i++){
		DaoValue *value = other->constants->items.pConst[i]->value;
		DaoRoutine *rout = & value->xRoutine;
		if( value->type == DAO_ROUTINE && rout->overloads == NULL && rout->routHost == other->objType ){
			DString *name = rout->routName;
			rout = DaoRoutine_Copy( rout, 1, 1 );
			value = (DaoValue*) rout;
			k = DaoRoutine_Finalize( rout, self->objType, deftypes );
#if 0
			printf( "%i %p:  %s  %s\n", i, rout, rout->routName->mbs, rout->routType->name->mbs );
#endif
			if( rout->attribs & DAO_ROUT_INITOR ){
				DRoutines_Add( self->classRoutines->overloads, rout );
			}else if( (it = DMap_Find( other->lookupTable, name )) ){
				st = LOOKUP_ST( it->value.pInt );
				up = LOOKUP_UP( it->value.pInt );
				id = LOOKUP_ID( it->value.pInt );
				if( st == DAO_CLASS_CONSTANT && up ==0 && id < i ){
					DaoValue *v = self->constants->items.pConst[id]->value;
					if( v->type == DAO_ROUTINE && v->xRoutine.overloads )
						DRoutines_Add( v->xRoutine.overloads, rout );
				}
			}
			DArray_Append( self->constants, DaoConstant_New( value ) );
			DArray_Append( routines, rout );
			if( k == 0 ) goto Failed;
			continue;
		}else if( value->type == DAO_ROUTINE ){
			/* No need to added the overloaded routines now; */
			/* Each of them has an entry in constants, and will be handled later: */
			DaoRoutine *routs = DaoRoutines_New( ns, self->objType, NULL );
			DArray_Append( self->constants, DaoConstant_New( (DaoValue*) routs ) );
			continue;
		}
		DArray_Append( self->constants, DaoConstant_New( value ) );
		DaoValue_Update( & self->constants->items.pConst[i]->value, ns, deftypes );
	}
	for(i=0; i<routines->size; i++){
		if( DaoRoutine_DoTypeInference( routines->items.pRoutine[i], 0 ) == 0 ) goto Failed;
	}
	DArray_Delete( offsets );
	DArray_Delete( routines );
	DaoRoutine_Finalize( self->classRoutine, self->objType, deftypes );
	return DaoRoutine_DoTypeInference( self->classRoutine, 0 );
Failed:
	DArray_Delete( offsets );
	DArray_Delete( routines );
	return 0;
}
DaoClass* DaoClass_Instantiate( DaoClass *self, DArray *types )
{
	DaoClass *klass = NULL;
	DaoType *type;
	DString *name;
	DNode *node;
	DMap *deftypes;
	daoint lt = DString_FindChar( self->className, '<', 0 );
	daoint i, holders = 0;
	if( self->typeHolders == NULL || self->typeHolders->size ==0 ) return self;
	while( types->size < self->typeHolders->size ){
		type = self->typeDefaults->items.pType[ types->size ];
		if( type == NULL ) type = self->typeHolders->items.pType[ types->size ];
		DArray_Append( types, type );
	}
	name = DString_New(1);
	DString_Append( name, self->className );
	if( lt != MAXSIZE ) DString_Erase( name, lt, MAXSIZE );
	DString_AppendChar( name, '<' );
	for(i=0; i<types->size; i++){
		type = types->items.pType[i];
		holders += type->tid == DAO_THT;
		if( i ) DString_AppendChar( name, ',' );
		DString_Append( name, type->name );
	}
	DString_AppendChar( name, '>' );
	while( self->templateClass ) self = self->templateClass;
	node = DMap_Find( self->instanceClasses, name );
	if( node ){
		klass = node->value.pClass;
	}else{
		deftypes = DMap_New(0,0);
		klass = DaoClass_New();
		if( holders ) klass->templateClass = self;
		DMap_Insert( self->instanceClasses, name, klass );
		DaoClass_AddReference( self, klass ); /* No need for cleanup of klass; */
		DaoClass_SetName( klass, name, self->classRoutine->nameSpace );
		for(i=0; i<types->size; i++){
			type = types->items.pType[i];
			if( DaoType_MatchTo( type, self->typeHolders->items.pType[i], deftypes ) ==0 ){
				DString_Delete( name );
				return NULL;
			}
			MAP_Insert( deftypes, self->typeHolders->items.pVoid[i], type );
		}
		klass->objType->nested = DArray_New(D_VALUE);
		DArray_Assign( klass->objType->nested, types );
		if( DaoClass_CopyField( klass, self, deftypes ) == 0 ){
			DString_Delete( name );
			return NULL;
		}
		DaoClass_DeriveClassData( klass );
		DaoClass_DeriveObjectData( klass );
		DaoClass_ResetAttributes( klass );
		DMap_Delete( deftypes );
		if( holders ){
			klass->typeHolders = DArray_New(0);
			klass->typeDefaults = DArray_New(0);
			klass->instanceClasses = DMap_New(D_STRING,0);
			DMap_Insert( klass->instanceClasses, klass->className, klass );
			for(i=0; i<types->size; i++){
				DArray_Append( klass->typeHolders, types->items.pType[i] );
				DArray_Append( klass->typeDefaults, NULL );
			}
			for(i=0; i<klass->typeHolders->size; i++){
				DaoClass_AddReference( klass, klass->typeHolders->items.pType[i] );
				DaoClass_AddReference( klass, klass->typeDefaults->items.pType[i] );
			}
		}
	}
	DString_Delete( name );
	return klass;
}
#endif

void DaoClass_SetName( DaoClass *self, DString *name, DaoNamespace *ns )
{
	DaoRoutine *rout;
	DString *str;

	if( self->classRoutine ) return;

	self->objType = DaoType_New( name->mbs, DAO_OBJECT, (DaoValue*)self, NULL );
	self->clsType = DaoType_New( name->mbs, DAO_CLASS, (DaoValue*) self, NULL );
	GC_IncRC( self->clsType );
	DString_InsertMBS( self->clsType->name, "class<", 0, 0, 0 );
	DString_AppendChar( self->clsType->name, '>' );

	str = DString_New(1);
	DString_SetMBS( str, "self" );
	DaoClass_AddObjectVar( self, str, NULL, self->objType, DAO_DATA_PRIVATE );
	DString_Assign( self->className, name );
	DaoClass_AddType( self, name, self->objType );

	rout = DaoRoutine_New( ns, self->objType, 1 );
	DString_Assign( rout->routName, name );
	DString_AppendMBS( rout->routName, "::" );
	DString_Append( rout->routName, name );
	self->classRoutine = rout; /* XXX class<name> */
	GC_IncRC( rout );

	rout->routType = DaoType_New( "routine<=>", DAO_ROUTINE, (DaoValue*)self->objType, NULL );
	DString_Append( rout->routType->name, name );
	DString_AppendMBS( rout->routType->name, ">" );
	GC_IncRC( rout->routType );
	rout->attribs |= DAO_ROUT_INITOR;
	DaoClass_AddConst( self, name, (DaoValue*) self, DAO_DATA_PUBLIC );

	self->objType->value = (DaoValue*) DaoObject_Allocate( self, DAO_MAX_PARENT );
	self->objType->value->xObject.trait |= DAO_VALUE_CONST|DAO_VALUE_NOCOPY;
	self->objType->value->xObject.isDefault = 1;
	GC_IncRC( self->objType->value );
	DString_SetMBS( str, "default" );
	DaoClass_AddConst( self, str, self->objType->value, DAO_DATA_PUBLIC );

	self->classRoutines = DaoRoutines_New( ns, self->objType, NULL );
	DString_Assign( self->classRoutines->routName, name );

	DaoClass_AddConst( self, rout->routName, (DaoValue*)self->classRoutines, DAO_DATA_PUBLIC );
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
			for(j=0; j<klass->superClass->size; j++){
				DaoClass *cls = klass->superClass->items.pClass[j];
				DArray_Append( parents, cls );
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
/* assumed to be called before parsing class body */
void DaoClass_DeriveClassData( DaoClass *self )
{
	DaoType *type;
	DaoValue *value;
	DArray *parents, *offsets;
	DString *mbs;
	DNode *it, *search;
	daoint i, k, id, perm, index;

	mbs = DString_New(1);

	if( self->clsType->bases == NULL ) self->clsType->bases = DArray_New(D_VALUE);
	if( self->objType->bases == NULL ) self->objType->bases = DArray_New(D_VALUE);
	DArray_Clear( self->clsType->bases );
	DArray_Clear( self->objType->bases );
	for(i=0; i<self->superClass->size; i++){
		if( self->superClass->items.pValue[i]->type == DAO_CLASS ){
			DaoValue *klass = self->superClass->items.pValue[i];
			DArray_Append( self->clsType->bases, klass->xClass.clsType );
			DArray_Append( self->objType->bases, klass->xClass.objType );
			DString_Assign( mbs, klass->xClass.className );
			DString_AppendChar( mbs, '#' );
			DString_AppendInteger( mbs, i+1 );
			DaoClass_AddConst( self, mbs, klass, DAO_DATA_PRIVATE );
		}else if( self->superClass->items.pValue[i]->type == DAO_CTYPE ){
			DaoCtype *cdata = (DaoCtype*) self->superClass->items.pValue[i];
			DaoTypeKernel *kernel = cdata->ctype->kernel;
			DMap *values = kernel->values;
			DMap *methods = kernel->methods;

			DArray_Append( self->clsType->bases, cdata->ctype );
			DArray_Append( self->objType->bases, cdata->cdtype );
			if( values == NULL ){
				DaoNamespace_SetupValues( kernel->nspace, kernel->typer );
				values = kernel->values;
			}
			if( methods == NULL ){
				DaoNamespace_SetupMethods( kernel->nspace, kernel->typer );
				methods = kernel->methods;
			}

			DString_Assign( mbs, cdata->ctype->name );
			// XXX
			DaoClass_AddConst( self, mbs, (DaoValue*)cdata, DAO_DATA_PRIVATE );
			DString_AppendChar( mbs, '#' );
			DString_AppendInteger( mbs, i+1 );
			DaoClass_AddConst( self, mbs, (DaoValue*)cdata, DAO_DATA_PRIVATE );
		}
	}
	parents = DArray_New(0);
	offsets = DArray_New(0);
	DaoClass_Parents( self, parents, offsets );
	for(i=1; i<parents->size; i++){
		DaoClass *klass = parents->items.pClass[i];
		DaoCdata *cdata = parents->items.pCdata[i];
		if( klass->type == DAO_CLASS ){
			if( klass->vtable ){
				if( self->vtable == NULL ) self->vtable = DHash_New(0,0);
				for(it=DMap_First(klass->vtable); it; it=DMap_Next(klass->vtable,it)){
					DMap_Insert( self->vtable, it->key.pVoid, it->value.pVoid );
				}
			}
			/* For class data: */
			for( id=0; id<klass->cstDataName->size; id++ ){
				DString *name = klass->cstDataName->items.pString[id];
				value = klass->constants->items.pConst[ id ]->value;
				search = MAP_Find( klass->lookupTable, name );
				if( search == NULL ) continue;
				perm = LOOKUP_PM( search->value.pInt );
				/* NO deriving private member: */
				if( perm <= DAO_DATA_PRIVATE ) continue;
				if( value->type == DAO_ROUTINE ){
					if( DString_EQ( value->xRoutine.routName, klass->className ) ) continue;
				}
				search = MAP_Find( self->lookupTable, name );
				if( search == NULL && value->type == DAO_ROUTINE && value->xRoutine.overloads ){
					/* To skip the private methods: */
					DaoClass_AddConst( self, name, (DaoValue*)value, perm );
				}else if( search == NULL ){
					index = LOOKUP_BIND( DAO_CLASS_CONSTANT, perm, i+1, self->constants->size );
					MAP_Insert( self->lookupTable, name, index );
					DArray_Append( self->constants, klass->constants->items.pConst[id] );
					DArray_Append( self->cstDataName, (void*)name );
					if( value->type == DAO_ROUTINE && (value->xRoutine.attribs & DAO_ROUT_VIRTUAL) ){
						if( self->vtable == NULL ) self->vtable = DHash_New(0,0);
						MAP_Insert( self->vtable, value, value );
					}
				}else if( value->type == DAO_ROUTINE && value->xRoutine.overloads ){
					DRoutines *routs = value->xRoutine.overloads;
					for(k=0; k<routs->routines->size; k++){
						DaoRoutine *rout = routs->routines->items.pRoutine[k];
						/* skip methods not defined in this parent type */
						if( rout->routHost != klass->objType ) continue;
						if( rout->attribs & DAO_ROUT_PRIVATE ) continue;
						DaoClass_AddConst( self, name, (DaoValue*)rout, perm );
					}
				}else if( value->type == DAO_ROUTINE ){
					/* skip methods not defined in this parent type */
					if( value->xRoutine.routHost != klass->objType ) continue;
					if( value->xRoutine.attribs & DAO_ROUT_PRIVATE ) continue;
					DaoClass_AddConst( self, name, value, perm );
				}
			}
			/* class global data */
			for( id=0; id<klass->glbDataName->size; id ++ ){
				DString *name = klass->glbDataName->items.pString[id];
				DaoVariable *var = klass->variables->items.pVar[id];
				search = MAP_Find( klass->lookupTable, name );
				perm = LOOKUP_PM( search->value.pInt );
				/* NO deriving private member: */
				if( perm <= DAO_DATA_PRIVATE ) continue;
				search = MAP_Find( self->lookupTable, name );
				/* To overide data: */
				if( search == NULL ){
					index = LOOKUP_BIND( DAO_CLASS_VARIABLE, perm, i+1, self->variables->size );
					MAP_Insert( self->lookupTable, name, index );
					DArray_Append( self->variables, var );
					DArray_Append( self->glbDataName, (void*)name );
				}
			}
		}else if( cdata->type == DAO_CTYPE ){
			DaoCtype *ctypeobj = (DaoCtype*) cdata;
			DaoTypeKernel *kernel = cdata->ctype->kernel;
			DaoTypeBase *typer = kernel->typer;
			DMap *values = kernel->values;
			DMap *methods = kernel->methods;
			DNode *it;
			int j;

			DaoCdataType_SpecializeMethods( cdata->ctype );
			kernel = cdata->ctype->kernel;
			methods = kernel->methods;

			if( typer->numItems ){
				for(j=0; typer->numItems[j].name!=NULL; j++){
					DString name = DString_WrapMBS( typer->numItems[j].name );
					it = DMap_Find( values, & name );
					if( it && DMap_Find( self->lookupTable, & name ) == NULL )
						DaoClass_AddConst( self, it->key.pString, it->value.pValue, DAO_DATA_PUBLIC );
				}
			}
			for(it=DMap_First( methods ); it; it=DMap_Next( methods, it )){
				DaoRoutine *func = it->value.pRoutine;
				DaoRoutine **funcs = & func;
				int k, count = 1;
				if( it->value.pValue->type == DAO_ROUTINE && it->value.pRoutine->overloads ){
					DRoutines *routs = it->value.pRoutine->overloads;
					funcs = routs->routines->items.pRoutine;
					count = routs->routines->size;
				}
				for(k=0; k<count; k++){
					DaoRoutine *func = funcs[k];
					if( func->routHost != ctypeobj->cdtype ) continue;
					if( func->attribs & DAO_ROUT_INITOR ) continue;
					DaoClass_AddConst( self, it->key.pString, (DaoValue*)func, DAO_DATA_PUBLIC );
				}
			}
		}
	}
	DArray_Delete( parents );
	DArray_Delete( offsets );
	DString_Delete( mbs );
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
	mbs = DString_New(1);

	parents = DArray_New(0);
	offsets = DArray_New(0);
	DaoClass_Parents( self, parents, offsets );
	for( i=0; i<self->superClass->size; i++){
		if( self->superClass->items.pValue[i]->type == DAO_CLASS ){
			DaoClass *klass = self->superClass->items.pClass[i];

			/* for properly arrangement object data: */
			for( id=0; id<klass->objDataName->size; id ++ ){
				DString *name = klass->objDataName->items.pString[id];
				DaoVariable *var = klass->instvars->items.pVar[id];
				var = DaoVariable_New( var->value, var->dtype );
				DArray_Append( self->objDataName, name );
				DArray_Append( self->instvars, var );
				DaoValue_MarkConst( (DaoValue*) var->value );
			}
			offset += klass->objDataName->size;
		}
	}
	for(i=1; i<parents->size; i++){
		DaoClass *klass = parents->items.pClass[i];
		offset = offsets->items.pInt[i]; /* plus self */
		if( klass->type == DAO_CLASS ){
			/* For object data: */
			for( id=0; id<klass->objDataName->size; id ++ ){
				DString *name = klass->objDataName->items.pString[id];
				search = MAP_Find( klass->lookupTable, name );
				perm = LOOKUP_PM( search->value.pInt );
				/* NO deriving private member: */
				if( perm <= DAO_DATA_PRIVATE ) continue;
				search = MAP_Find( self->lookupTable, name );
				if( search == NULL ){ /* To not overide data and routine: */
					index = LOOKUP_BIND( DAO_OBJECT_VARIABLE, perm, i, (offset+id) );
					MAP_Insert( self->lookupTable, name, index );
				}
			}
		}
	}
	self->derived = 1;
	DString_Delete( mbs );
	DArray_Delete( parents );
	DArray_Delete( offsets );
	DaoObject_Init( & self->objType->value->xObject, NULL, 0 );
	self->objType->value->xObject.trait &= ~DAO_VALUE_CONST;
	DaoValue_MarkConst( self->objType->value );
	DaoValue_MarkConst( self->constants->items.pConst[1]->value ); /* ::default */
}
void DaoClass_ResetAttributes( DaoClass *self )
{
	DString *mbs = DString_New(1);
	DNode *node;
	int i, k, id, autodef = 0;
	for(i=0; i<self->classRoutines->overloads->routines->size; i++){
		DaoRoutine *r2 = self->classRoutines->overloads->routines->items.pRoutine[i];
		DArray *types = r2->routType->nested;
		autodef = r2->parCount != 0;
		if( autodef ==0 ) break;
		for(k=0; k<types->size; k++){
			if( types->items.pType[k]->tid != DAO_PAR_DEFAULT ) break;
		}
		autodef = k != types->size;
		if( autodef ==0 ) break;
	}
	if( autodef ){
		for( i=0; i<self->superClass->size; i++){
			if( self->superClass->items.pValue[i]->type == DAO_CLASS ){
				DaoClass *klass = self->superClass->items.pClass[i];
				autodef = autodef && (klass->attribs & DAO_CLS_AUTO_DEFAULT);
				if( autodef ==0 ) break;
			}else{
				autodef = 0;
				break;
			}
		}
	}
	if( autodef ) self->attribs |= DAO_CLS_AUTO_DEFAULT;
	for(i=DVM_NOT; i<=DVM_BITRIT; i++){
		DString_SetMBS( mbs, daoBitBoolArithOpers[i-DVM_NOT] );
		node = DMap_Find( self->lookupTable, mbs );
		if( node == NULL ) continue;
		if( LOOKUP_ST( node->value.pInt ) != DAO_CLASS_CONSTANT ) continue;
		id = LOOKUP_ID( node->value.pInt );
		k = self->constants->items.pConst[id]->value->type;
		if( k != DAO_ROUTINE ) continue;
		self->attribs |= DAO_OPER_OVERLOADED | (DAO_OPER_OVERLOADED<<(i-DVM_NOT+1));
	}
	DString_Delete( mbs );
}
int  DaoClass_FindSuper( DaoClass *self, DaoValue *super )
{
	int i;
	for(i=0; i<self->superClass->size; i++){
		if( super == self->superClass->items.pValue[i] ) return i;
	}
	return -1;
}

int DaoCdata_ChildOf( DaoTypeBase *self, DaoTypeBase *super )
{
	int i;
	if( self == super ) return 1;
	for(i=0; i<DAO_MAX_CDATA_SUPER; i++){
		if( self->supers[i] ==NULL ) break;
		if( DaoCdata_ChildOf( self->supers[i], super ) ) return 1;
	}
	return 0;
}
int  DaoClass_ChildOf( DaoClass *self, DaoValue *klass )
{
	DaoCdata *cdata = (DaoCdata*) klass;
	int i;
	if( self == NULL ) return 0;
	if( klass == (DaoValue*) self ) return 1;
	for( i=0; i<self->superClass->size; i++ ){
		DaoClass *dsup = self->superClass->items.pClass[i];
		DaoCdata *csup = self->superClass->items.pCdata[i];
		if( dsup == NULL ) continue;
		if( klass == self->superClass->items.pValue[i] ) return 1;
		if( dsup->type == DAO_CLASS && DaoClass_ChildOf( dsup,  klass ) ){
			return 1;
		}else if( csup->type == DAO_CTYPE && klass->type == DAO_CTYPE ){
			if( DaoCdata_ChildOf( csup->ctype->kernel->typer, cdata->ctype->kernel->typer ) )
				return 1;
		}
	}
	return 0;
}
DaoValue* DaoClass_CastToBase( DaoClass *self, DaoType *parent )
{
	int i;
	if( parent == NULL ) return NULL;
	if( self->clsType == parent ) return (DaoValue*) self;
	if( self->superClass ==NULL ) return NULL;
	for( i=0; i<self->superClass->size; i++ ){
		DaoValue *sup = self->superClass->items.pValue[i];
		if( sup == NULL ) return NULL;
		if( sup->type == DAO_CLASS ){
			if( (sup = DaoClass_CastToBase( (DaoClass*)sup, parent ) ) ) return sup;
		}else if( sup->type == DAO_CTYPE && parent->tid == DAO_CTYPE ){
			if( (sup = DaoType_CastToParent( sup, parent ) ) ) return sup;
		}
	}
	return NULL;
}
void DaoClass_AddSuperClass( DaoClass *self, DaoValue *super )
{
	if( self->superClass->size >= DAO_MAX_PARENT ){
		printf( "Error: too many parents (max. %i allowed) for the class: %s\n",
				DAO_MAX_PARENT, self->className->mbs );
		return;
	}
	DArray_Append( self->superClass, super );
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
	if( self == thisClass || perm == DAO_DATA_PUBLIC || (child && perm >= DAO_DATA_PROTECTED) ){
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
	if( self == thisClass || perm == DAO_DATA_PUBLIC || (child && perm >=DAO_DATA_PROTECTED) ){
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
	if( node && LOOKUP_UP( node->value.pInt ) ) return -DAO_CTW_WAS_DEFINED;
	if( deft == NULL && t ) deft = t->value;

	id = self->objDataName->size;
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
	if( data->type == DAO_ROUTINE && data->xRoutine.routHost != self->objType ){
		if( data->xRoutine.attribs & DAO_ROUT_VIRTUAL ){ /* data->xRoutine.overloads == NULL */
			if( self->vtable == NULL ) self->vtable = DHash_New(0,0);
			MAP_Insert( self->vtable, data, data );
		}
	}
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
	int sto, pm, up, id;
	DNode *node = MAP_Find( self->lookupTable, name );
	DaoNamespace *ns = self->classRoutine->nameSpace;
	DaoConstant *dest;
	DaoValue *value;

	assert( data != NULL );
	if( node && LOOKUP_UP( node->value.pInt ) ){ /* inherited field: */
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
			DaoRoutine *routs = DaoRoutines_New( ns, self->objType, (DaoRoutine*) dest->value );
			DaoConstant *cst = DaoConstant_New( (DaoValue*) routs );
			GC_ShiftRC( cst, dest );
			self->constants->items.pConst[id] = cst;
			return DaoClass_AddConst( self, name, data, s );
		}else{
			/* Add the new constant: */
			DaoConstant *cst = DaoConstant_New( data );
			GC_ShiftRC( cst, dest );
			self->constants->items.pConst[id] = cst;
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
			if( self->vtable ) DaoRoutine_UpdateVtable( (DaoRoutine*)dest->value, rout, self->vtable );
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
		return -DAO_CTW_TYPE_NOMATCH;
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
	DNode *node = DMap_First( self->lookupTable );
	DaoStream_WriteMBS( stream, "class " );
	DaoStream_WriteString( stream, self->className );
	DaoStream_WriteMBS( stream, ":\n" );
	for( ; node != NULL; node = DMap_Next( self->lookupTable, node ) ){
		DaoValue *val;
		if( LOOKUP_ST( node->value.pInt ) != DAO_CLASS_CONSTANT ) continue;
		val = self->constants->items.pConst[ LOOKUP_ID( node->value.pInt ) ]->value;
		if( val->type == DAO_ROUTINE && val->xRoutine.body )
			DaoRoutine_PrintCode( & val->xRoutine, stream );
	}
}
DaoRoutine* DaoClass_FindOperator( DaoClass *self, const char *oper, DaoClass *scoped )
{
	DaoValue *V = NULL;
	DString name = DString_WrapMBS( oper );
	DaoClass_GetData( self, & name, & V, scoped );
	if( V == NULL || V->type != DAO_ROUTINE ) return NULL;
	return (DaoRoutine*) V;
}
