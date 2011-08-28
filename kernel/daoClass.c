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

#include"assert.h"
#include"string.h"
#include"daoConst.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoRoutine.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoStream.h"
#include"daoNumtype.h"
#include"daoValue.h"
#include"daoNamespace.h"

static void DaoClass_GetField( DaoValue *self0, DaoContext *ctx, DString *name )
{
	int tid = ctx->routine->routHost ? ctx->routine->routHost->tid : 0;
	DaoType *type = ctx->routine->routHost;
	DaoClass *host = tid == DAO_OBJECT ? & type->aux->xClass : NULL;
	DaoClass *self = & self0->xClass;
	DString *mbs = DString_New(1);
	DaoValue *value = NULL;
	int rc = DaoClass_GetData( self, name, & value, host );
	if( rc ){
		DString_SetMBS( mbs, DString_GetMBS( self->className ) );
		DString_AppendMBS( mbs, "." );
		DString_Append( mbs, name );
		DaoContext_RaiseException( ctx, rc, mbs->mbs );
	}else{
		DaoContext_PutReference( ctx, value );
	}
	DString_Delete( mbs );
}
static void DaoClass_SetField( DaoValue *self0, DaoContext *ctx, DString *name, DaoValue *value )
{
	DaoClass *self = & self0->xClass;
	DNode *node = DMap_Find( self->lookupTable, name );
	if( node && LOOKUP_ST( node->value.pSize ) == DAO_CLASS_VARIABLE ){
		int up = LOOKUP_UP( node->value.pSize );
		int id = LOOKUP_ID( node->value.pSize );
		DaoValue **dt = self->glbDataTable->items.pArray[up]->items.pValue + id;
		DaoType *tp = self->glbTypeTable->items.pArray[up]->items.pType[ id ];
		if( DaoValue_Move( value, dt, tp ) ==0 )
			DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "not matched" );
	}else{
		/* XXX permission */
		DaoContext_RaiseException( ctx, DAO_ERROR_FIELD, "not exist" );
	}
}
static void DaoClass_GetItem( DaoValue *self0, DaoContext *ctx, DaoValue *ids[], int N )
{
}
static void DaoClass_SetItem( DaoValue *self0, DaoContext *ctx, DaoValue *ids[], int N, DaoValue *value )
{
}
static DaoValue* DaoClass_Copy(  DaoValue *self, DaoContext *ctx, DMap *cycData )
{
	return self;
}

static DaoTypeCore classCore=
{
	0, NULL, NULL, NULL, NULL,
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
	DaoClass *self = (DaoClass*) dao_malloc( sizeof(DaoClass) );
	DaoValue_Init( self, DAO_CLASS );
	self->vtable = NULL; //XXX GC
	self->classRoutine = NULL;
	self->classRoutines = NULL;
	self->className = DString_New(1);
	self->classHelp = DString_New(1);

	self->derived = 0;
	self->attribs = 0;
	self->objDefCount = 0;

	self->cstDataTable = DArray_New(0);
	self->glbDataTable = DArray_New(0);
	self->glbTypeTable = DArray_New(0);
	self->lookupTable  = DHash_New(D_STRING,0);
	self->ovldRoutMap  = DHash_New(D_STRING,0);
	self->abstypes = DMap_New(D_STRING,0);
	self->deflines = DMap_New(D_STRING,0);
	self->cstData      = DArray_New(D_VALUE);
	self->glbData      = DArray_New(D_VALUE);
	self->glbDataType  = DArray_New(D_VALUE);
	self->objDataType  = DArray_New(D_VALUE);
	self->objDataName  = DArray_New(D_STRING);
	self->cstDataName  = DArray_New(D_STRING);
	self->glbDataName  = DArray_New(D_STRING);
	self->superAlias   = DArray_New(D_STRING);
	self->superClass   = DArray_New(D_VALUE);
	self->objDataDefault = DArray_New(D_VALUE);
	self->protoValues = NULL;
	self->typeHolders = NULL;
	self->typeDefaults = NULL;
	self->instanceClasses = NULL;
	self->templateClass = NULL;
	self->references = DArray_New(0);
	return self;
}
void DaoClass_Delete( DaoClass *self )
{
	DNode *n = DMap_First( self->abstypes );
	for( ; n != NULL; n = DMap_Next( self->abstypes, n ) ) GC_DecRC( n->value.pValue );
	GC_DecRC( self->clsType );
	GC_DecRCs( self->references );
	DMap_Delete( self->abstypes );
	DMap_Delete( self->deflines );
	DMap_Delete( self->lookupTable );
	DMap_Delete( self->ovldRoutMap );
	DArray_Delete( self->cstData );
	DArray_Delete( self->glbData );
	DArray_Delete( self->cstDataTable );
	DArray_Delete( self->glbDataTable );
	DArray_Delete( self->glbTypeTable );
	DArray_Delete( self->glbDataType );
	DArray_Delete( self->objDataType );
	DArray_Delete( self->objDataName );
	DArray_Delete( self->cstDataName );
	DArray_Delete( self->glbDataName );
	DArray_Delete( self->superClass );
	DArray_Delete( self->superAlias );
	DArray_Delete( self->references );
	DArray_Delete( self->objDataDefault );
	if( self->protoValues ) DMap_Delete( self->protoValues );
	if( self->typeHolders ){
		DArray_Delete( self->typeHolders );
		DArray_Delete( self->typeDefaults );
		DMap_Delete( self->instanceClasses );
	}

	DString_Delete( self->className );
	DString_Delete( self->classHelp );
	dao_free( self );
}
void DaoClass_AddReference( DaoClass *self, void *reference )
{
	if( reference == NULL ) return;
	GC_IncRC( reference );
	DArray_Append( self->references, reference );
}
void DaoRoutine_CopyFields( DaoRoutine *self, DaoRoutine *other );
void DaoRoutine_MapTypes( DaoRoutine *self, DMap *deftypes );
int  DaoRoutine_InferTypes( DaoRoutine *self );
int DaoRoutine_Finalize( DaoRoutine *self, DaoClass *klass, DMap *deftypes );
void DaoClass_Parents( DaoClass *self, DArray *parents, DArray *offsets );
void DaoValue_Update( DaoValue **self, DaoNamespace *ns, DMap *deftypes )
{
	DaoValue *value = *self;
	DaoObject *obj = & value->xObject;
	DaoType *tp, *tp2;

	if( value == NULL || value->type < DAO_ENUM ) return;
	tp = DaoNamespace_GetType( ns, value );
	tp2 = DaoType_DefineTypes( tp, ns, deftypes );
	if( tp == tp2 ) return;
	if( value->type == DAO_OBJECT && obj == & obj->myClass->objType->value->xObject ){
		if( tp2->tid == DAO_OBJECT ){
			GC_ShiftRC( tp2->value, obj );
			*self = tp2->value;
			return;
		}
	}
	DaoValue_Move( value, self, tp2 );
}
void DaoClass_CopyField( DaoClass *self, DaoClass *other, DMap *deftypes )
{
	DaoNamespace *ns = other->classRoutine->nameSpace;
	DaoType *tp;
	DArray *parents = DArray_New(0);
	DArray *offsets = DArray_New(0);
	DNode *it;
	int i, st, up, id;

	for(i=0; i<other->superClass->size; i++){
		DaoClass *klass = other->superClass->items.pClass[i];
		if( klass->type == DAO_CLASS && klass->typeHolders ){
			tp = DaoType_DefineTypes( klass->objType, ns, deftypes );
			if( tp ) klass = & tp->aux->xClass;
			DArray_Append( self->superClass, klass );
			DArray_Append( self->superAlias, klass->objType->name );
		}else{
			DArray_Append( self->superClass, klass );
			DArray_Append( self->superAlias, other->superAlias->items.pVoid[i] );
		}
	}
	/* temporary setup for parent data tables */
	DaoClass_Parents( self, parents, offsets );
	for(i=1; i<parents->size; i++){
		DaoClass *klass = parents->items.pClass[i];
		if( klass->type == DAO_CLASS ){
			DArray_Append( self->cstDataTable, klass->cstData );
			DArray_Append( self->glbDataTable, klass->glbData );
			DArray_Append( self->glbTypeTable, klass->glbDataType );
		}
	}

	DaoRoutine_CopyFields( self->classRoutine, other->classRoutine );
	DaoRoutine_MapTypes( self->classRoutine, deftypes );
	DaoRoutine_InferTypes( self->classRoutine );
	for(it=DMap_First(other->lookupTable);it;it=DMap_Next(other->lookupTable,it)){
		st = LOOKUP_ST( it->value.pSize );
		up = LOOKUP_UP( it->value.pSize );
		id = LOOKUP_ID( it->value.pSize );
		if( up ==0 ){
			if( st == DAO_CLASS_CONSTANT && id <self->cstData->size ) continue;
			if( st == DAO_CLASS_VARIABLE && id <self->glbData->size ) continue;
			if( st == DAO_OBJECT_VARIABLE && id <self->objDataDefault->size ) continue;
		}
		DMap_Insert( self->lookupTable, it->key.pVoid, it->value.pVoid );
	}
	for(i=self->objDataName->size; i<other->objDataName->size; i++)
		DArray_Append( self->objDataName, other->objDataName->items.pString[i] );
	for(i=self->cstDataName->size; i<other->cstDataName->size; i++)
		DArray_Append( self->cstDataName, other->cstDataName->items.pString[i] );
	for(i=self->glbDataName->size; i<other->glbDataName->size; i++)
		DArray_Append( self->glbDataName, other->glbDataName->items.pString[i] );
	for(i=self->glbData->size; i<other->glbData->size; i++)
		DArray_Append( self->glbData, other->glbData->items.pValue[i] );
	for(i=self->objDataType->size; i<other->objDataType->size; i++){
		tp = other->objDataType->items.pType[i];
		tp = DaoType_DefineTypes( tp, ns, deftypes );
		DArray_Append( self->objDataType, tp );
	}
	for(i=self->glbDataType->size; i<other->glbDataType->size; i++){
		tp = other->glbDataType->items.pType[i];
		tp = DaoType_DefineTypes( tp, ns, deftypes );
		DArray_Append( self->glbDataType, tp );
	}
	for(i=self->objDataDefault->size; i<other->objDataDefault->size; i++){
		DaoValue *v = other->objDataDefault->items.pValue[i];
		tp = self->objDataType->items.pType[i];
		DArray_Append( self->objDataDefault, NULL );
		/* TODO fail checking */
		DaoValue_Move( v, self->objDataDefault->items.pValue + i, tp );
	}
	for(i=self->cstData->size; i<other->cstData->size; i++){
		DaoValue *value = other->cstData->items.pValue[i];
		if( value->type == DAO_ROUTINE && value->xRoutine.routHost == other->objType ){
			DaoRoutine *rout = & value->xRoutine;
			DString *name = rout->routName;
			rout = DaoRoutine_Copy( rout );
			value = (DaoValue*) rout;
#if 0
			printf( "%i %p:  %s  %s\n", i, rout, rout->routName->mbs, rout->routType->name->mbs );
#endif
			if( DaoRoutine_Finalize( rout, self, deftypes ) ==0 ) continue;
			if( rout->attribs & DAO_ROUT_INITOR ){
				DaoFunctree_Add( self->classRoutines, (DRoutine*)rout );
			}else if( (it = DMap_Find( other->lookupTable, name )) ){
				st = LOOKUP_ST( it->value.pSize );
				up = LOOKUP_UP( it->value.pSize );
				id = LOOKUP_ID( it->value.pSize );
				if( st == DAO_CLASS_CONSTANT && up ==0 && id < i ){
					DaoValue *v = self->cstData->items.pValue[id];
					if( v->type == DAO_FUNCTREE ) DaoFunctree_Add( & v->xFunctree, (DRoutine*)rout );
				}
			}
			DArray_Append( self->cstData, value );
			continue;
		}else if( value->type >= DAO_FUNCTREE && value->type <= DAO_FUNCTION ){
			/* Pass NULL as name, so that its name will be properly by
			 * DaoFunctree_Add(): */
			DaoFunctree *meta = DaoFunctree_New( ns, NULL );
			GC_IncRC( self->objType );
			meta->host = self->objType;
			DArray_Append( self->cstData, meta );
			continue;
		}
		DArray_Append( self->cstData, value );
		DaoValue_Update( & self->cstData->items.pValue[i], ns, deftypes );
	}
	DArray_Erase( self->cstDataTable, 1, MAXSIZE );
	DArray_Erase( self->glbDataTable, 1, MAXSIZE );
	DArray_Erase( self->glbTypeTable, 1, MAXSIZE );
	DArray_Delete( parents );
	DArray_Delete( offsets );
}
DaoClass* DaoClass_Instantiate( DaoClass *self, DArray *types )
{
	DaoClass *klass = NULL;
	DaoType *type;
	DString *name;
	DNode *node;
	DMap *deftypes;
	size_t lt = DString_FindChar( self->className, '<', 0 );
	int i, holders = 0;
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
		holders += type->tid == DAO_INITYPE;
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
		DaoClass_AddReference( self, klass );
		DaoClass_SetName( klass, name, self->classRoutine->nameSpace );
		for(i=0; i<types->size; i++){
			type = types->items.pType[i];
			MAP_Insert( deftypes, self->typeHolders->items.pVoid[i], type );
		}
		klass->objType->nested = DArray_New(0);
		DArray_Swap( klass->objType->nested, types );
		GC_IncRCs( klass->objType->nested );
		DaoClass_CopyField( klass, self, deftypes );
		DaoClass_DeriveClassData( klass );
		DaoClass_DeriveObjectData( klass );
		DaoClass_ResetAttributes( klass );
		DMap_Delete( deftypes );
		if( holders ){
			klass->typeHolders = DArray_Copy( klass->objType->nested );
			klass->typeDefaults = DArray_New(0);
			klass->instanceClasses = DMap_New(D_STRING,0);
			DMap_Insert( klass->instanceClasses, klass->className, klass );
			while( klass->typeDefaults->size < klass->typeHolders->size )
				DArray_Append( klass->typeDefaults, NULL );
			for(i=0; i<klass->typeHolders->size; i++){
				DaoClass_AddReference( klass, klass->typeHolders->items.pType[i] );
				DaoClass_AddReference( klass, klass->typeDefaults->items.pType[i] );
			}
		}
	}
	DString_Delete( name );
	return klass;
}
void DaoClass_SetName( DaoClass *self, DString *name, DaoNamespace *ns )
{
	DaoRoutine *rout;
	DString *str;

	if( self->classRoutine ) return;

	rout = DaoRoutine_New();
	str = DString_New(1);
	DString_Assign( rout->routName, name );
	DString_AppendMBS( rout->routName, "::" );
	DString_Append( rout->routName, name );
	self->classRoutine = rout; /* XXX class<name> */
	rout->nameSpace = ns;
	GC_IncRC( ns );
	GC_IncRC( rout ); // XXX GC scan

	self->objType = DaoType_New( name->mbs, DAO_OBJECT, (DaoValue*)self, NULL );
	self->clsType = DaoType_New( name->mbs, DAO_CLASS, (DaoValue*) self, NULL );
	GC_IncRC( self->clsType );
	DString_InsertMBS( self->clsType->name, "class<", 0, 0, 0 );
	DString_AppendChar( self->clsType->name, '>' );

	DString_SetMBS( str, "self" );
	DaoClass_AddObjectVar( self, str, NULL, self->objType, DAO_DATA_PRIVATE, -1 );
	DString_Assign( self->className, name );
	DaoClass_AddType( self, name, self->objType );

	rout->routType = DaoType_New( "routine<=>", DAO_ROUTINE, (DaoValue*) self->objType, NULL );
	DString_Append( rout->routType->name, name );
	DString_AppendMBS( rout->routType->name, ">" );
	GC_IncRC( rout->routType );
	GC_IncRC( self->objType );
	rout->routHost = self->objType;
	rout->attribs |= DAO_ROUT_INITOR;
	DaoClass_AddConst( self, name, (DaoValue*) self, DAO_DATA_PUBLIC, -1 );

	self->objType->value = (DaoValue*) DaoObject_Allocate( self );
	self->objType->value->xObject.trait |= DAO_DATA_CONST|DAO_DATA_NOCOPY;
	GC_IncRC( self->objType->value );
	DString_SetMBS( str, "default" );
	DaoClass_AddConst( self, str, self->objType->value, DAO_DATA_PUBLIC, -1 );

	self->classRoutines = DaoFunctree_New( ns, name );
	self->classRoutines->host = self->objType;
	GC_IncRC( self->objType );

	DaoClass_AddConst( self, rout->routName, (DaoValue*)self->classRoutines, DAO_DATA_PUBLIC, -1 );
	DString_Delete( str );

	DArray_Append( self->cstDataTable, self->cstData );
	DArray_Append( self->glbDataTable, self->glbData );
	DArray_Append( self->glbTypeTable, self->glbDataType );
}
/* breadth-first search */
void DaoClass_Parents( DaoClass *self, DArray *parents, DArray *offsets )
{
	DaoValue *dbase;
	DaoClass *klass;
	DaoCdata *cdata;
	DaoTypeBase *typer;
	int i, j, offset;
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
				DArray_Append( offsets, (size_t) offset );
				offset += (cls->type == DAO_CLASS) ? cls->objDataName->size : 0;
			}
		}else if( dbase->type == DAO_CTYPE ){
			cdata = (DaoCdata*) dbase;
			typer = cdata->typer;
			for(j=0; j<DAO_MAX_CDATA_SUPER; j++){
				if( typer->supers[j] == NULL ) break;
				DArray_Append( parents, typer->supers[j]->priv->abtype->aux );
				DArray_Append( offsets, (size_t) offset );
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
	DNode *search;
	size_t i, id, perm, index;

	mbs = DString_New(1);

	for( i=0; i<self->superClass->size; i++){
		DString *alias = self->superAlias->items.pString[i];
		if( self->superClass->items.pValue[i]->type == DAO_CLASS ){
			DaoValue *klass = self->superClass->items.pValue[i];
			if( DString_EQ( klass->xClass.className, alias ) ==0 ){
				DaoClass_AddConst( self, alias, klass, DAO_DATA_PRIVATE, -1 );
			}
		}else if( self->superClass->items.pValue[i]->type == DAO_CTYPE ){
			DaoValue *cdata = self->superClass->items.pValue[i];
			DaoTypeBase *typer = cdata->xCdata.typer;
			DaoTypeCore *core = typer->priv;
			DMap *values = core->values;
			DMap *methods = core->methods;

			if( values == NULL ){
				DaoNamespace_SetupValues( typer->priv->nspace, typer );
				values = core->values;
			}
			if( methods == NULL ){
				DaoNamespace_SetupMethods( typer->priv->nspace, typer );
				methods = core->methods;
			}

			DString_SetMBS( mbs, typer->name );
			DaoClass_AddConst( self, mbs, cdata, DAO_DATA_PRIVATE, -1 );
			if( strcmp( typer->name, alias->mbs ) ){
				DaoClass_AddConst( self, alias, cdata, DAO_DATA_PRIVATE, -1 );
			}
		}
	}
	parents = DArray_New(0);
	offsets = DArray_New(0);
	DaoClass_Parents( self, parents, offsets );
	for(i=1; i<parents->size; i++){
		DaoClass *klass = parents->items.pClass[i];
		DaoCdata *cdata = parents->items.pCdata[i];
		if( klass->type == DAO_CLASS ){
			int up = self->cstDataTable->size;
			DArray_Append( self->cstDataTable, klass->cstData );
			DArray_Append( self->glbDataTable, klass->glbData );
			DArray_Append( self->glbTypeTable, klass->glbDataType );
			/* For class data: */
			for( id=0; id<klass->cstDataName->size; id++ ){
				DString *name = klass->cstDataName->items.pString[id];
				value = klass->cstData->items.pValue[ id ];
				search = MAP_Find( klass->lookupTable, name );
				if( search == NULL ) continue;
				perm = LOOKUP_PM( search->value.pSize );
				/* NO deriving private member: */
				if( perm <= DAO_DATA_PRIVATE ) continue;
				if( value->type == DAO_FUNCTREE ){
					if( DString_EQ( value->xFunctree.name, klass->className ) ) continue;
				}else if( value->type == DAO_ROUTINE ){
					if( DString_EQ( value->xRoutine.routName, klass->className ) ) continue;
				}
				search = MAP_Find( self->lookupTable, name );
				if( value->type == DAO_FUNCTREE ){
					DaoFunctree *meta = & value->xFunctree;
					int k;
					for(k=0; k<meta->routines->size; k++){
						DRoutine *rout = meta->routines->items.pRout2[k];
						/* skip methods not defined in this parent type */
						if( rout->routHost != klass->objType ) continue;
						DaoClass_AddConst( self, name, (DaoValue*)rout, perm, -1 );
					}
				}else if( value->type == DAO_ROUTINE ){
					/* skip methods not defined in this parent type */
					if( value->xRoutine.routHost != klass->objType ) continue;
					DaoClass_AddConst( self, name, value, perm, -1 );
				}else if( search == NULL ){
					index = LOOKUP_BIND( DAO_CLASS_CONSTANT, perm, up, id );
					MAP_Insert( self->lookupTable, name, index );
				}
			}
			/* class global data */
			for( id=0; id<klass->glbDataName->size; id ++ ){
				DString *name = klass->glbDataName->items.pString[id];
				type = klass->glbDataType->items.pType[id];
				search = MAP_Find( klass->lookupTable, name );
				perm = LOOKUP_PM( search->value.pSize );
				/* NO deriving private member: */
				if( perm <= DAO_DATA_PRIVATE ) continue;
				search = MAP_Find( self->lookupTable, name );
				/* To overide data: */
				if( search == NULL ){
					index = LOOKUP_BIND( DAO_CLASS_VARIABLE, perm, up, id );
					MAP_Insert( self->lookupTable, name, index );
				}
			}
		}else if( cdata->type == DAO_CTYPE ){
			DaoTypeBase *typer = cdata->typer;
			DaoTypeCore *core = typer->priv;
			DMap *values = core->values;
			DMap *methods = core->methods;
			DNode *it;
			int j;

			if( typer->numItems ){
				for(j=0; typer->numItems[j].name!=NULL; j++){
					DString name = DString_WrapMBS( typer->numItems[j].name );
					it = DMap_Find( values, & name );
					if( it && DMap_Find( self->lookupTable, & name ) == NULL )
						DaoClass_AddConst( self, it->key.pString, it->value.pValue, DAO_DATA_PUBLIC, -1 );
				}
			}
			for(it=DMap_First( methods ); it; it=DMap_Next( methods, it )){
				DaoFunction *func = (DaoFunction*) it->value.pValue;
				DaoFunction **funcs = & func;
				int k, count = 1;
				if( it->value.pValue->type == DAO_FUNCTREE ){
					DaoFunctree *meta = (DaoFunctree*) it->value.pValue;
					funcs = (DaoFunction**)meta->routines->items.pValue;
					count = meta->routines->size;
				}
				for(k=0; k<count; k++){
					DaoFunction *func = funcs[k];
					if( func->routHost != typer->priv->abtype ) continue;
					if( DString_EQ( func->routName, core->abtype->name ) ) continue;
					DaoClass_AddConst( self, it->key.pString, (DaoValue*)func, DAO_DATA_PUBLIC, -1 );
				}
#if 0
				if( it->value.pValue->type == DAO_FUNCTION ){
					DaoFunction *func = (DaoFunction*) it->value.pValue;
					value.v.func = func;
					value.t = func->type;
					if( func->routHost != typer->priv->abtype ) continue;
					if( DString_EQ( func->routName, core->abtype->name ) ) continue;
					DaoClass_AddConst( self, it->key.pString, value, DAO_DATA_PUBLIC, -1 );
				}else if( it->value.pValue->type == DAO_FUNCTREE ){
				}
				//if( DString_EQ( value.v.func->routName, core->abtype->name ) ) continue;
				//search = MAP_Find( self->lookupTable, it->key.pVoid );
				//if( search ==NULL ) /* TODO: overload between C and Dao functions */
				//	DaoClass_AddConst( self, it->key.pString, value, DAO_DATA_PUBLIC, -1 );
#endif
			}
		}
	}
	DString_Delete( mbs );
	DArray_Delete( parents );
	DArray_Delete( offsets );
}
/* assumed to be called after parsing class body */
void DaoClass_DeriveObjectData( DaoClass *self )
{
	DaoType *type;
	DaoValue *value;
	DArray *parents, *offsets;
	DString *mbs;
	DNode *search;
	size_t i, id, perm, index, offset = 0;

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
				type = klass->objDataType->items.pType[id];
				value = klass->objDataDefault->items.pValue[id];
				DArray_Append( self->objDataName, name );
				DArray_Append( self->objDataType, type );
				DArray_Append( self->objDataDefault, value );
				DaoValue_MarkConst( (DaoValue*) DArray_Back( self->objDataDefault ) );
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
				perm = LOOKUP_PM( search->value.pSize );
				/* NO deriving private member: */
				if( perm <= DAO_DATA_PRIVATE ) continue;
				search = MAP_Find( self->lookupTable, name );
				if( search == NULL ){ /* To not overide data and routine: */
					index = LOOKUP_BIND( DAO_OBJECT_VARIABLE, perm, 0, (offset+id) );
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
	self->objType->value->xObject.trait &= ~DAO_DATA_CONST;
	DaoValue_MarkConst( self->objType->value );
	DaoValue_MarkConst( self->cstData->items.pValue[1] ); /* ::default */
}
void DaoClass_ResetAttributes( DaoClass *self )
{
	DString *mbs = DString_New(1);
	DNode *node;
	int i, k, id, autodef = 0;
	for(i=0; i<self->classRoutines->routines->size; i++){
		DRoutine *r2 = self->classRoutines->routines->items.pRout2[i];
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
	for(i=DVM_MOVE; i<=DVM_BITRIT; i++){
		DString_SetMBS( mbs, daoBitBoolArithOpers[i-DVM_MOVE] );
		node = DMap_Find( self->lookupTable, mbs );
		if( node == NULL ) continue;
		if( LOOKUP_ST( node->value.pSize ) != DAO_CLASS_CONSTANT ) continue;
		id = LOOKUP_ID( node->value.pSize );
		k = self->cstData->items.pValue[id]->type;
		if( k != DAO_ROUTINE && k != DAO_FUNCTION ) continue;
		self->attribs |= DAO_OPER_OVERLOADED | (DAO_OPER_OVERLOADED<<(i-DVM_MOVE+1));
	}
	DString_Delete( mbs );
}
int  DaoClass_FindSuper( DaoClass *self, DaoValue *super )
{
	int i;
	for(i=0; i<self->superClass->size; i++)
		if( super == self->superClass->items.pValue[i] ) return i;
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
			if( DaoCdata_ChildOf( csup->typer, cdata->typer ) )
				return 1;
		}
	}
	return 0;
}
DaoValue* DaoClass_MapToParent( DaoClass *self, DaoType *parent )
{
	int i;
	if( parent == NULL ) return NULL;
	if( self->objType == parent ) return (DaoValue*) self;
	if( self->superClass ==NULL ) return NULL;
	for( i=0; i<self->superClass->size; i++ ){
		DaoValue *sup = self->superClass->items.pValue[i];
		if( sup == NULL ) return NULL;
		if( sup->type == DAO_CLASS ){
			if( (sup = DaoClass_MapToParent( (DaoClass*)sup, parent ) ) ) return sup;
		}else if( sup->type == DAO_CTYPE && parent->tid == DAO_CDATA ){
			/* cdata is accessible as cdata type, not ctype type. */
			if( DaoCdata_ChildOf( sup->xCdata.typer, parent->typer ) ) return sup;
		}
	}
	return NULL;
}
void DaoClass_AddSuperClass( DaoClass *self, DaoValue *super, DString *alias )
{
	/* XXX if( alias == NULL ) alias = super->className; */
	DArray_Append( self->superClass, super );
	DArray_Append( self->superAlias, alias );
}
int  DaoClass_FindConst( DaoClass *self, DString *name )
{
	DNode *node = MAP_Find( self->lookupTable, name );
	if( node == NULL || LOOKUP_ST( node->value.pSize ) != DAO_CLASS_CONSTANT ) return -1;
	return node->value.pSize;
}
DaoValue* DaoClass_GetConst( DaoClass *self, int id )
{
	int up = LOOKUP_UP( id );
	id = LOOKUP_ID( id );
	if( up >= self->cstDataTable->size ) return NULL;
	if( id >= self->cstDataTable->items.pArray[up]->size ) return NULL;
	return self->cstDataTable->items.pArray[up]->items.pValue[id];
}
void DaoClass_SetConst( DaoClass *self, int id, DaoValue *data )
{
	int up = LOOKUP_UP( id );
	id = LOOKUP_ID( id );
	if( up >= self->cstDataTable->size ) return;
	if( id >= self->cstDataTable->items.pArray[up]->size ) return;
	DaoValue_Copy( data, & self->cstDataTable->items.pArray[up]->items.pValue[id] );
}
int DaoClass_GetData( DaoClass *self, DString *name, DaoValue **value, DaoClass *thisClass )
{
	DaoValue *p = NULL;
	DNode *node = MAP_Find( self->lookupTable, name );
	int child = thisClass && DaoClass_ChildOf( thisClass, (DaoValue*)self );
	int sto, perm, up, id;

	*value = NULL;
	if( ! node ) return DAO_ERROR_FIELD_NOTEXIST;
	perm = LOOKUP_PM( node->value.pSize );
	sto = LOOKUP_ST( node->value.pSize );
	up = LOOKUP_UP( node->value.pSize );
	id = LOOKUP_ID( node->value.pSize );
	if( self == thisClass || perm == DAO_DATA_PUBLIC || (child && perm >= DAO_DATA_PROTECTED) ){
		switch( sto ){
		case DAO_CLASS_VARIABLE : p = self->glbDataTable->items.pArray[up]->items.pValue[id]; break;
		case DAO_CLASS_CONSTANT : p = self->cstDataTable->items.pArray[up]->items.pValue[id]; break;
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
	perm = LOOKUP_PM( node->value.pSize );
	sto = LOOKUP_ST( node->value.pSize );
	up = LOOKUP_UP( node->value.pSize );
	id = LOOKUP_ID( node->value.pSize );
	if( self == thisClass || perm == DAO_DATA_PUBLIC || (child && perm >=DAO_DATA_PROTECTED) ){
		switch( sto ){
		case DAO_OBJECT_VARIABLE : return self->objDataType->items.pType + id;
		case DAO_CLASS_VARIABLE  : return self->glbTypeTable->items.pArray[up]->items.pType + id;
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
	return node->value.pSize;
}
int DaoClass_AddObjectVar( DaoClass *self, DString *name, DaoValue *deft, DaoType *t, int s, int ln )
{
	int id;
	DNode *node = MAP_Find( self->deflines, name );
	if( node ) return DAO_CTW_WAS_DEFINED;
	if( ln >= 0 ) MAP_Insert( self->deflines, name, (size_t)ln );
	if( deft == NULL && t ) deft = t->value;

	id = self->objDataName->size;
	MAP_Insert( self->lookupTable, name, LOOKUP_BIND( DAO_OBJECT_VARIABLE, s, 0, id ) );
	DArray_Append( self->objDataType, (void*)t );
	DArray_Append( self->objDataName, (void*)name );
	DArray_Append( self->objDataDefault, NULL );
	DaoValue_Move( deft, self->objDataDefault->items.pValue + id, t );
	DaoValue_MarkConst( self->objDataDefault->items.pValue[ id ] );
	return 0;
}
static void DaoClass_AddConst3( DaoClass *self, DString *name, DaoValue *data )
{
	DArray_Append( self->cstDataName, (void*)name );
	DArray_Append( self->cstData, data );
	DaoValue_MarkConst( self->cstData->items.pValue[self->cstData->size-1] );
	if( data->type == DAO_ROUTINE && data->xRoutine.routHost != self->objType ){
		if( data->xRoutine.attribs & DAO_ROUT_VIRTUAL ){
			if( self->vtable == NULL ) self->vtable = DHash_New(0,0);
			MAP_Insert( self->vtable, data, data );
		}
	}
}
static int DaoClass_AddConst2( DaoClass *self, DString *name, DaoValue *data, int s, int ln )
{
	DaoNamespace *ns = self->classRoutine->nameSpace;
	if( data->type == DAO_FUNCTREE && data->xFunctree.host != self->objType ){
		DaoFunctree *mroutine = DaoFunctree_New( ns, name );
		GC_IncRC( self->objType );
		mroutine->host = self->objType;
		DaoFunctree_Import( mroutine, & data->xFunctree );
		data = (DaoValue*) mroutine;
	}
	if( ln >= 0 ) MAP_Insert( self->deflines, name, (size_t)ln );
	MAP_Insert( self->lookupTable, name, LOOKUP_BIND( DAO_CLASS_CONSTANT, s, 0, self->cstData->size ) );
	DaoClass_AddConst3( self, name, data );
	return 0;
}
int DaoClass_AddConst( DaoClass *self, DString *name, DaoValue *data, int s, int ln )
{
	int sto, up, id;
	DaoValue *dest;
	DNode *node;
	// TODO : handle NULL
	if( data->type >= DAO_FUNCTREE && data->type <= DAO_FUNCTION ){
		node = MAP_Find( self->lookupTable, name );
		/* add as new constant: */
		if( node == NULL ) return DaoClass_AddConst2( self, name, data, s, ln );
		sto = LOOKUP_ST( node->value.pSize );
		up = LOOKUP_UP( node->value.pSize );
		id = LOOKUP_ID( node->value.pSize );
		/* add as new constant: */
		if( up ) return DaoClass_AddConst2( self, name, data, s, ln );
		if( sto != DAO_CLASS_CONSTANT ) return DAO_CTW_WAS_DEFINED;
		dest = self->cstData->items.pValue[id];
		if( dest->type < DAO_FUNCTREE || dest->type > DAO_FUNCTION ) return DAO_CTW_WAS_DEFINED;
		if( dest->type == DAO_ROUTINE || dest->type == DAO_FUNCTION ){
			DaoNamespace *ns = self->classRoutine->nameSpace;
			DaoFunctree *mroutine = DaoFunctree_New( ns, name );

			if( dest->xRoutine.routHost == self->objType ){
				/* Add individual entry for the existing function: */
				DaoClass_AddConst3( self, name, dest );
			}

			DaoFunctree_Add( mroutine, (DRoutine*) dest );
			mroutine->trait |= DAO_DATA_CONST;
			mroutine->host = self->objType;
			GC_IncRC( mroutine->host );
			GC_ShiftRC( mroutine, dest );
			dest = (DaoValue*) mroutine;
			self->cstData->items.pValue[id] = dest;
		}
		if( data->type == DAO_FUNCTREE ){
			DaoFunctree_Import( & dest->xFunctree, & data->xFunctree );
		}else{
			DRoutine *rout = (DRoutine*) data;
			DaoFunctree *meta = & dest->xFunctree;
			DaoFunctree_Add( meta, rout );
			if( self->vtable ) DaoFunctree_UpdateVtable( meta, rout, self->vtable );
			if( data->xRoutine.routHost == self->objType ){
				/* Add individual entry for the new function: */
				DaoClass_AddConst3( self, name, data );
			}
		}
		return 0;
	}

	node = MAP_Find( self->deflines, name );
	if( node ) return DAO_CTW_WAS_DEFINED;
	return DaoClass_AddConst2( self, name, data, s, ln );
}
int DaoClass_AddGlobalVar( DaoClass *self, DString *name, DaoValue *data, DaoType *t, int s, int ln )
{
	DNode *node = MAP_Find( self->deflines, name );
	int size = self->glbData->size;
	if( node ) return DAO_CTW_WAS_DEFINED;
	if( ln >= 0 ) MAP_Insert( self->deflines, name, (size_t)ln );
	if( data == NULL && t ) data = t->value;
	MAP_Insert( self->lookupTable, name, LOOKUP_BIND( DAO_CLASS_VARIABLE, s, 0, size ) );
	DArray_Append( self->glbData, NULL );
	DArray_Append( self->glbDataType, (void*)t );
	DArray_Append( self->glbDataName, (void*)name );
	if( data && DaoValue_Move( data, self->glbData->items.pValue + size, t ) ==0 )
		return DAO_CTW_TYPE_NOMATCH;
	return 0;
}
int DaoClass_AddType( DaoClass *self, DString *name, DaoType *tp )
{
	DNode *node = MAP_Find( self->abstypes, name );
	/* remove this following two lines? XXX */
	if( DString_FindChar( name, '?', 0 ) != MAXSIZE
			|| DString_FindChar( name, '@', 0 ) != MAXSIZE ) return 0;
	if( node == NULL ){
		MAP_Insert( self->abstypes, name, tp );
		GC_IncRC( tp );
	}
	return 1;
}
void DaoClass_AddOvldRoutine( DaoClass *self, DString *signature, DaoRoutine *rout )
{
	MAP_Insert( self->ovldRoutMap, signature, rout );
}
DaoRoutine* DaoClass_GetOvldRoutine( DaoClass *self, DString *signature )
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
	DaoRoutine_PrintCode( self->classRoutine, stream );
	for( ; node != NULL; node = DMap_Next( self->lookupTable, node ) ){
		DaoValue *val;
		if( LOOKUP_ST( node->value.pSize ) != DAO_CLASS_CONSTANT ) continue;
		val = self->cstData->items.pValue[ LOOKUP_ID( node->value.pSize ) ];
		if( val->type == DAO_ROUTINE ) DaoRoutine_PrintCode( & val->xRoutine, stream );
	}
}
DaoValue* DaoClass_FindOperator( DaoClass *self, const char *oper, DaoClass *scoped )
{
	DaoValue *V = NULL;
	DString name = DString_WrapMBS( oper );
	DaoClass_GetData( self, & name, & V, scoped );
	if( V == NULL || V->type < DAO_FUNCTREE || V->type > DAO_FUNCTION ) return NULL;
	return V;
}
