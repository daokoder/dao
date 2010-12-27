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
#include"daoNamespace.h"

static void DaoClass_Print( DValue *self, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
	DaoBase_Print( self, ctx, stream, cycData );
}

static void DaoClass_GetField( DValue *self0, DaoContext *ctx, DString *name )
{
	int tid = ctx->routine->tidHost;
	DaoType *type = ctx->routine->routHost;
	DaoClass *host = tid == DAO_OBJECT ? type->X.klass : NULL;
	DaoClass *self = self0->v.klass;
	DString *mbs = DString_New(1);
	DValue *d2 = NULL;
	DValue value = daoNullValue;
	int rc = DaoClass_GetData( self, name, & value, host, & d2 );
	if( rc ){
		DString_SetMBS( mbs, DString_GetMBS( self->className ) );
		DString_AppendMBS( mbs, "." );
		DString_Append( mbs, name );
		DaoContext_RaiseException( ctx, rc, mbs->mbs );
	}else{
		DaoContext_PutReference( ctx, d2 );
	}
	DString_Delete( mbs );
}
static void DaoClass_SetField( DValue *self0, DaoContext *ctx, DString *name, DValue value )
{
	DaoClass *self = self0->v.klass;
	DNode *node = DMap_Find( self->lookupTable, name );
	if( node && LOOKUP_ST( node->value.pSize ) == DAO_CLASS_VARIABLE ){
		int up = LOOKUP_UP( node->value.pSize );
		int id = LOOKUP_ID( node->value.pSize );
		DValue *dt = self->glbDataTable->items.pVarray[up]->data + id;
		DaoType *tp = self->glbTypeTable->items.pArray[up]->items.pType[ id ];
		if( DValue_Move( value, dt, tp ) ==0 )
			DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "not matched" );
	}else{
		/* XXX permission */
		DaoContext_RaiseException( ctx, DAO_ERROR_FIELD, "not exist" );
	}
}
static void DaoClass_GetItem( DValue *self0, DaoContext *ctx, DValue pid )
{
}
static void DaoClass_SetItem( DValue *self0, DaoContext *ctx, DValue pid, DValue value )
{
}
static DValue DaoClass_Copy(  DValue *self, DaoContext *ctx, DMap *cycData )
{
	return *self;
}

static DaoTypeCore classCore=
{
	0, NULL, NULL, NULL, NULL,
	DaoClass_GetField,
	DaoClass_SetField,
	DaoClass_GetItem,
	DaoClass_SetItem,
	DaoClass_Print,
	DaoClass_Copy
};

DaoTypeBase classTyper =
{
	"class", & classCore, NULL, NULL, {0},
	(FuncPtrDel) DaoClass_Delete, NULL
};

DaoClass* DaoClass_New()
{
	DaoClass *self = (DaoClass*) dao_malloc( sizeof(DaoClass) );
	DaoBase_Init( self, DAO_CLASS );
	self->classRoutine = NULL;
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
	self->cstData      = DVarray_New();
	self->glbData      = DVarray_New();
	self->glbDataType  = DArray_New(0);
	self->objDataType  = DArray_New(0);
	self->objDataName  = DArray_New(D_STRING);
	self->cstDataName  = DArray_New(D_STRING);
	self->glbDataName  = DArray_New(D_STRING);
	self->superClass   = DArray_New(0);
	self->superAlias   = DArray_New(D_STRING);
	self->objDataDefault  = DVarray_New();
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
	for( ; n != NULL; n = DMap_Next( self->abstypes, n ) ) GC_DecRC( n->value.pBase );
	GC_DecRC( self->clsType );
	GC_DecRCs( self->superClass );
	GC_DecRCs( self->glbDataType );
	GC_DecRCs( self->objDataType );
	GC_DecRCs( self->references );
	DMap_Delete( self->abstypes );
	DMap_Delete( self->deflines );
	DMap_Delete( self->lookupTable );
	DMap_Delete( self->ovldRoutMap );
	DVarray_Delete( self->cstData );
	DVarray_Delete( self->glbData );
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
	DVarray_Delete( self->objDataDefault );
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
void DaoRoutine_Finalize( DaoRoutine *self, DaoClass *klass, DMap *deftypes );
void DaoClass_Parents( DaoClass *self, DArray *parents, DArray *offsets );
void DaoClass_CopyField( DaoClass *self, DaoClass *other, DMap *deftypes )
{
	DaoNameSpace *ns = other->classRoutine->nameSpace;
	DaoType *tp, *tp2;
	DArray *parents = DArray_New(0);
	DArray *offsets = DArray_New(0);
	DNode *it;
	int i, st, up, id;

	for(i=0; i<other->superClass->size; i++){
		DaoClass *klass = other->superClass->items.pClass[i];
		if( klass->type == DAO_CLASS && klass->typeHolders ){
			tp = DaoType_DefineTypes( klass->objType, ns, deftypes );
			if( tp ) klass = tp->X.klass;
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
			int up = self->cstDataTable->size;
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
		DVarray_Append( self->glbData, other->glbData->data[i] );
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
	GC_IncRCs( self->objDataType );
	GC_IncRCs( self->glbDataType );
	for(i=self->objDataDefault->size; i<other->objDataDefault->size; i++){
		DValue v = other->objDataDefault->data[i];
		tp = self->objDataType->items.pType[i];
		DVarray_Append( self->objDataDefault, daoNullValue );
		DValue_Move( v, & self->objDataDefault->data[self->objDataDefault->size-1], tp );
	}
	for(i=self->cstData->size; i<other->cstData->size; i++){
		DValue value = other->cstData->data[i];
		if( value.t == DAO_ROUTINE && value.v.routine->routHost == other->objType ){
			DaoRoutine *rout = value.v.routine;
			DString *name = rout->routName;
			rout = value.v.routine = DaoRoutine_Copy( rout, 0 );
			DaoRoutine_Finalize( rout, self, deftypes );
#if 0
			printf( "%p:  %s  %s\n", rout, rout->routName->mbs, rout->routType->name->mbs );
#endif
			if( rout->attribs & DAO_ROUT_INITOR ){
				DRoutine_AddOverLoad( (DRoutine*)self->classRoutine, (DRoutine*)rout );
			}else{
				it = DMap_Find( other->lookupTable, name );
				if( it ){
					st = LOOKUP_ST( it->value.pSize );
					up = LOOKUP_UP( it->value.pSize );
					id = LOOKUP_ID( it->value.pSize );
					if( st == DAO_CLASS_CONSTANT && up ==0 && id < i ){
						DValue v2 = self->cstData->data[id];
						if( v2.t == DAO_ROUTINE && v2.v.routine->routHost == self->objType )
							DRoutine_AddOverLoad( (DRoutine*)v2.v.routine, (DRoutine*)rout );
					}
				}
			}
			DVarray_Append( self->cstData, value );
			continue;
		}
		DVarray_Append( self->cstData, value );
		if( value.t < DAO_ARRAY ) continue;
		tp = DaoNameSpace_GetTypeV( ns, value );
		tp2 = DaoType_DefineTypes( tp, ns, deftypes );
		if( tp == tp2 ) continue;
		value = daoNullValue;
		DValue_Move( self->cstData->data[i], & value, tp2 );
		DValue_Clear( & self->cstData->data[i] );
		self->cstData->data[i] = value;
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
	int E = types->size;
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
		DaoClass_SetName( klass, name );
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
void DaoClass_SetName( DaoClass *self, DString *name )
{
	DaoRoutine *rout = DaoRoutine_New();
	DString *str = DString_New(1);
	DValue value = daoNullClass;

	rout->refCount --;
	rout->routTable->size = 0;
	DString_Assign( rout->routName, name );
	DString_AppendMBS( rout->routName, "::" );
	DString_Append( rout->routName, name );
	self->classRoutine = rout; /* XXX class<name> */

	self->objType = DaoType_New( name->mbs, DAO_OBJECT, (DaoBase*)self, NULL );
	self->clsType = DaoType_New( name->mbs, DAO_CLASS, (DaoBase*) self, NULL );
	GC_IncRC( self->clsType );
	DString_InsertMBS( self->clsType->name, "class<", 0, 0, 0 );
	DString_AppendChar( self->clsType->name, '>' );

	DString_SetMBS( str, "self" );
	DaoClass_AddObjectVar( self, str, daoNullValue, self->objType, DAO_DATA_PRIVATE, -1 );
	DString_Assign( self->className, name );
	DaoClass_AddType( self, name, self->objType );

	rout->routType = DaoType_New( "routine<=>", DAO_ROUTINE, (DaoBase*) self->objType, NULL );
	DString_Append( rout->routType->name, name );
	DString_AppendMBS( rout->routType->name, ">" );
	GC_IncRC( rout->routType );
	GC_IncRC( self->objType );
	rout->tidHost = DAO_OBJECT;
	rout->routHost = self->objType;
	rout->attribs |= DAO_ROUT_INITOR;
	value.v.klass = self;
	DaoClass_AddConst( self, name, value, DAO_DATA_PUBLIC, -1 );
	value.t = DAO_ROUTINE;
	value.v.routine = rout;
	DaoClass_AddConst( self, rout->routName, value, DAO_DATA_PRIVATE, -1 );
	DString_Delete( str );

	DArray_Append( self->cstDataTable, self->cstData );
	DArray_Append( self->glbDataTable, self->glbData );
	DArray_Append( self->glbTypeTable, self->glbDataType );
}
/* breadth-first search */
void DaoClass_Parents( DaoClass *self, DArray *parents, DArray *offsets )
{
	DaoBase *dbase;
	DaoClass *klass;
	DaoCData *cdata;
	DaoTypeBase *typer;
	int i, j, offset;
	DArray_Clear( parents );
	DArray_Clear( offsets );
	DArray_Append( parents, self );
	DArray_Append( offsets, self->objDataName->size );
	for(i=0; i<parents->size; i++){
		dbase = parents->items.pBase[i];
		offset = offsets->items.pInt[i];
		if( dbase->type == DAO_CLASS ){
			klass = (DaoClass*) dbase;
			for(j=0; j<klass->superClass->size; j++){
				DaoClass *cls = klass->superClass->items.pClass[j];
				DArray_Append( parents, cls );
				DArray_Append( offsets, (size_t) offset );
				offset += (cls->type == DAO_CLASS) ? cls->objDataName->size : 0;
			}
		}else if( dbase->type == DAO_CDATA ){
			cdata = (DaoCData*) dbase;
			typer = cdata->typer;
			for(j=0; j<DAO_MAX_CDATA_SUPER; j++){
				if( typer->supers[j] == NULL ) break;
				DArray_Append( parents, typer->supers[j]->priv->abtype->X.extra );
				DArray_Append( offsets, (size_t) offset );
			}
		}
	}
}
/* assumed to be called before parsing class body */
void DaoClass_DeriveClassData( DaoClass *self )
{
	DArray *parents, *offsets;
	DRoutine *rep, *mem;
	DaoType *type;
	DNode *search;
	DString *mbs;
	DValue value = daoNullValue;
	size_t i, k, id, perm, index;

	mbs = DString_New(1);

	for( i=0; i<self->superClass->size; i++){
		if( self->superClass->items.pBase[i]->type == DAO_CLASS ){
			DaoClass *klass = self->superClass->items.pClass[i];
			if( DString_EQ( klass->className, self->superAlias->items.pString[i] ) ==0 ){
				value = daoNullClass;
				value.v.klass = klass;
				DaoClass_AddConst( self, self->superAlias->items.pString[i], value, DAO_DATA_PRIVATE, -1 );
			}
		}else if( self->superClass->items.pBase[i]->type == DAO_CDATA ){
			DaoCData *cdata = self->superClass->items.pCData[i];
			DaoTypeBase *typer = cdata->typer;
			DaoTypeCore *core = typer->priv;
			DMap *values = core->values;
			DMap *methods = core->methods;
			DNode *it;

			if( values == NULL ){
				DaoNameSpace_SetupValues( typer->priv->nspace, typer );
				values = core->values;
			}
			if( methods == NULL ){
				DaoNameSpace_SetupMethods( typer->priv->nspace, typer );
				methods = core->methods;
			}

			DString_SetMBS( mbs, typer->name );
			value.t = DAO_CDATA;
			value.v.cdata = cdata;
			DaoClass_AddConst( self, mbs, value, DAO_DATA_PRIVATE, -1 );
			if( strcmp( typer->name, self->superAlias->items.pString[i]->mbs ) ){
				DaoClass_AddConst( self, self->superAlias->items.pString[i], value, DAO_DATA_PRIVATE, -1 );
			}
		}
	}
	parents = DArray_New(0);
	offsets = DArray_New(0);
	DaoClass_Parents( self, parents, offsets );
	for(i=1; i<parents->size; i++){
		DaoClass *klass = parents->items.pClass[i];
		DaoCData *cdata = parents->items.pCData[i];
		DaoTypeBase *typer;
		if( klass->type == DAO_CLASS ){
			int up = self->cstDataTable->size;
			DArray_Append( self->cstDataTable, klass->cstData );
			DArray_Append( self->glbDataTable, klass->glbData );
			DArray_Append( self->glbTypeTable, klass->glbDataType );
			/* For class data: */
			for( id=0; id<klass->cstDataName->size; id++ ){
				DString *name = klass->cstDataName->items.pString[id];
				value = klass->cstData->data[ id ];
				search = MAP_Find( klass->lookupTable, name );
				if( search == NULL ) continue;
				perm = LOOKUP_PM( search->value.pSize );
				if( perm <= DAO_DATA_PRIVATE ) continue;
				rep = mem = NULL;
				if( value.t == DAO_ROUTINE || value.t == DAO_FUNCTION ){
					mem = (DRoutine*) value.v.routine;
					if( DString_EQ( mem->routName, klass->className ) ) continue;
				}
				/* NO deriving private member: */
				search = MAP_Find( self->lookupTable, name );
				/* To overide data and routine: */
				if( search == NULL ){
					if( value.t == DAO_ROUTINE ){
						rep = DRoutine_New(); /* a dummy routine */
						DString_Assign( rep->routName, name );
						rep->tidHost = DAO_OBJECT;
						rep->routHost = self->objType;
						rep->routType = mem->routType;
						rep->nameSpace = value.v.routine->nameSpace;
						GC_IncRC( rep->routHost );
						GC_IncRC( rep->routType );
						GC_IncRC( rep->nameSpace );
						value.v.p = (DaoBase*) rep;
						DaoClass_AddConst( self, name, value, perm, -1 );
					}else{
						index = LOOKUP_BIND( DAO_CLASS_CONSTANT, perm, up, id );
						MAP_Insert( self->lookupTable, name, index );
					}
				}else{
					value = self->cstData->data[ LOOKUP_ID( search->value.pSize ) ];
					if( value.t == DAO_ROUTINE || value.t == DAO_FUNCTION )
						rep = (DRoutine*) value.v.routine;
				}
				if( rep && mem && mem->minimal ==0 && mem->routHost == klass->objType ){
					/* no need to use mem->routTable, overloaded methods have 
					 * multiple entries in klass->cstDataName. */
					DRoutine_AddOverLoad( rep, mem );
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
		}else if( cdata->type == DAO_CDATA ){
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
						DaoClass_AddConst( self, it->key.pString, *it->value.pValue, DAO_DATA_PUBLIC, -1 );
				}
			}
			value.t = DAO_FUNCTION;
			for(it=DMap_First( methods ); it; it=DMap_Next( methods, it )){
				value.v.func = (DaoFunction*) it->value.pVoid;
				if( value.v.func->routHost != typer->priv->abtype ) continue;
				if( DString_EQ( value.v.func->routName, core->abtype->name ) ) continue;
				search = MAP_Find( self->lookupTable, it->key.pVoid );
				if( search ==NULL ) /* TODO: overload between C and Dao functions */
					DaoClass_AddConst( self, it->key.pString, value, DAO_DATA_PUBLIC, -1 );
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
	DArray *parents, *offsets;
	DRoutine *rep, *mem;
	DaoType *type;
	DNode *search;
	DString *mbs;
	DValue value = daoNullValue;
	size_t i, k, id, perm, index, offset = 0;

	self->objDefCount = self->objDataName->size;
	offset = self->objDataName->size;
	mbs = DString_New(1);

	parents = DArray_New(0);
	offsets = DArray_New(0);
	DaoClass_Parents( self, parents, offsets );
	for( i=0; i<self->superClass->size; i++){
		if( self->superClass->items.pBase[i]->type == DAO_CLASS ){
			DaoClass *klass = self->superClass->items.pClass[i];

			/* for properly arrangement object data: */
			for( id=0; id<klass->objDataName->size; id ++ ){
				DString *name = klass->objDataName->items.pString[id];
				type = klass->objDataType->items.pType[id];
				value = klass->objDataDefault->data[id];
				GC_IncRC( type );
				DArray_Append( self->objDataName, name );
				DArray_Append( self->objDataType, type );
				DVarray_Append( self->objDataDefault, daoNullValue );
				DValue_SimpleMove( value, self->objDataDefault->data + self->objDataDefault->size-1 );
				DValue_MarkConst( self->objDataDefault->data + self->objDataDefault->size-1 );
			}
			offset += klass->objDataName->size;
		}
	}
	for(i=1; i<parents->size; i++){
		DaoClass *klass = parents->items.pClass[i];
		DaoCData *cdata = parents->items.pCData[i];
		DaoTypeBase *typer;
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
	DString_Delete( mbs );
	DArray_Delete( parents );
	DArray_Delete( offsets );
}
void DaoClass_ResetAttributes( DaoClass *self )
{
	DString *mbs = DString_New(1);
	DNode *node;
	int i, k, id, autodef = 0;
	for(i=0; i<self->classRoutine->routTable->size; i++){
		DRoutine *r2 = self->classRoutine->routTable->items.pRout2[i];
		DArray *types = r2->routType->nested;
		if( r2 == (DRoutine*) self->classRoutine ) continue;
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
			if( self->superClass->items.pBase[i]->type == DAO_CLASS ){
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
		k = self->cstData->data[id].t;
		if( k != DAO_ROUTINE && k != DAO_FUNCTION ) continue;
		self->attribs |= DAO_OPER_OVERLOADED | (DAO_OPER_OVERLOADED<<(i-DVM_MOVE+1));
	}
	DString_Delete( mbs );
}
int  DaoClass_FindSuper( DaoClass *self, DaoBase *super )
{
	int i;
	for(i=0; i<self->superClass->size; i++)
		if( super == self->superClass->items.pBase[i] ) return i;
	return -1;
}

int DaoCData_ChildOf( DaoTypeBase *self, DaoTypeBase *super )
{
	int i;
	if( self == super ) return 1;
	for(i=0; i<DAO_MAX_CDATA_SUPER; i++){
		if( self->supers[i] ==NULL ) break;
		if( DaoCData_ChildOf( self->supers[i], super ) ) return 1;
	}
	return 0;
}
int  DaoClass_ChildOf( DaoClass *self, DaoBase *klass )
{
	DaoCData *cdata = (DaoCData*) klass;
	int i;
	if( self == NULL ) return 0;
	if( klass == (DaoBase*) self ) return 1;
	for( i=0; i<self->superClass->size; i++ ){
		if( klass == self->superClass->items.pBase[i] ) return 1;
		if( self->superClass->items.pClass[i]->type == DAO_CLASS
				&& DaoClass_ChildOf( self->superClass->items.pClass[i],  klass ) ){
			return 1;
		}else if( self->superClass->items.pClass[i]->type == DAO_CDATA
				&& klass->type == DAO_CDATA ){
			if( DaoCData_ChildOf( self->superClass->items.pCData[i]->typer, cdata->typer ) )
				return 1;
		}
	}
	return 0;
}
void DaoClass_AddSuperClass( DaoClass *self, DaoBase *super, DString *alias )
{
	/* XXX if( alias == NULL ) alias = super->className; */
	DArray_Append( self->superClass, super );
	DArray_Append( self->superAlias, alias );
	GC_IncRC( super );
}
int  DaoClass_FindConst( DaoClass *self, DString *name )
{
	DNode *node = MAP_Find( self->lookupTable, name );
	if( node == NULL || LOOKUP_ST( node->value.pSize ) != DAO_CLASS_CONSTANT ) return -1;
	return node->value.pSize;
}
DValue DaoClass_GetConst( DaoClass *self, int id )
{
	DVarray *array;
	int up = LOOKUP_UP( id );
	id = LOOKUP_ID( id );
	if( up >= self->cstDataTable->size ) return daoNullValue;
	array = self->cstDataTable->items.pVarray[up];
	if( id >= array->size ) return daoNullValue;
	return array->data[id];
}
void DaoClass_SetConst( DaoClass *self, int id, DValue data )
{
	DVarray *array;
	int up = LOOKUP_UP( id );
	id = LOOKUP_ID( id );
	if( up >= self->cstDataTable->size ) return;
	array = self->cstDataTable->items.pVarray[up];
	if( id >= array->size ) return;
	DValue_Copy( array->data + id, data );
}
int DaoClass_GetData( DaoClass *self, DString *name, DValue *value, DaoClass *thisClass, DValue **d2 )
{
	DValue *p = NULL;
	int sto, perm, up, id;
	DNode *node = MAP_Find( self->lookupTable, name );
	*value = daoNullValue;
	if( ! node ) return DAO_ERROR_FIELD_NOTEXIST;
	perm = LOOKUP_PM( node->value.pSize );
	sto = LOOKUP_ST( node->value.pSize );
	up = LOOKUP_UP( node->value.pSize );
	id = LOOKUP_ID( node->value.pSize );
	if( self == thisClass || perm == DAO_DATA_PUBLIC
			|| ( thisClass && DaoClass_ChildOf( thisClass, (DaoBase*)self )
				&& perm >= DAO_DATA_PROTECTED ) ){
		switch( sto ){
		case DAO_CLASS_VARIABLE : p = self->glbDataTable->items.pVarray[up]->data + id; break;
		case DAO_CLASS_CONSTANT : p = self->cstDataTable->items.pVarray[up]->data + id; break;
		default : return DAO_ERROR_FIELD;
		}
		if( p ) *value = *p;
		if( d2 ) *d2 = p;
	}else{
		return DAO_ERROR_FIELD_NOTPERMIT;
	}
	return 0;
}
DaoType** DaoClass_GetDataType( DaoClass *self, DString *name, int *res, DaoClass *thisClass )
{
	int sto, perm, up, id;
	DNode *node = MAP_Find( self->lookupTable, name );
	*res = DAO_ERROR_FIELD_NOTEXIST;
	if( ! node ) return NULL;

	*res = 0;
	perm = LOOKUP_PM( node->value.pSize );
	sto = LOOKUP_ST( node->value.pSize );
	up = LOOKUP_UP( node->value.pSize );
	id = LOOKUP_ID( node->value.pSize );
	if( self == thisClass || perm == DAO_DATA_PUBLIC
			|| ( thisClass && DaoClass_ChildOf( thisClass, (DaoBase*)self )
				&& perm >=DAO_DATA_PROTECTED ) ){
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
int DaoClass_GetDataIndex( DaoClass *self, DString *name, int *type )
{
	DNode *node = MAP_Find( self->lookupTable, name );
	if( ! node ) return -1;
	*type = LOOKUP_ST( node->value.pSize );
	return LOOKUP_ID( node->value.pSize );
}
int DaoClass_AddObjectVar( DaoClass *self, DString *name, DValue deft, DaoType *t, int s, int ln )
{
	int id;
	DNode *node = MAP_Find( self->deflines, name );
	if( node ) return DAO_CTW_WAS_DEFINED;
	if( ln >= 0 ) MAP_Insert( self->deflines, name, (size_t)ln );

	GC_IncRC( t );
	id = self->objDataName->size;
	MAP_Insert( self->lookupTable, name, LOOKUP_BIND( DAO_OBJECT_VARIABLE, s, 0, id ) );
	DArray_Append( self->objDataType, (void*)t );
	DArray_Append( self->objDataName, (void*)name );
	DVarray_Append( self->objDataDefault, daoNullValue );
	DValue_Move( deft, self->objDataDefault->data + self->objDataDefault->size-1, t );
	DValue_MarkConst( self->objDataDefault->data + self->objDataDefault->size-1 );
	return 0;
}
int DaoClass_AddConst( DaoClass *self, DString *name, DValue data, int s, int ln )
{
	DNode *node = MAP_Find( self->deflines, name );
	if( node ) return DAO_CTW_WAS_DEFINED;
	if( ln >= 0 ) MAP_Insert( self->deflines, name, (size_t)ln );

	MAP_Insert( self->lookupTable, name, LOOKUP_BIND( DAO_CLASS_CONSTANT, s, 0, self->cstData->size ) );
	DArray_Append( self->cstDataName, (void*)name );
	DVarray_Append( self->cstData, daoNullValue );
	DValue_SimpleMove( data, self->cstData->data + self->cstData->size-1 );
	DValue_MarkConst( & self->cstData->data[self->cstData->size-1] );
	return node ? DAO_CTW_WAS_DEFINED : 0;
}
int DaoClass_AddGlobalVar( DaoClass *self, DString *name, DValue data, DaoType *t, int s, int ln )
{
	DNode *node = MAP_Find( self->deflines, name );
	if( node ) return DAO_CTW_WAS_DEFINED;
	if( ln >= 0 ) MAP_Insert( self->deflines, name, (size_t)ln );
	GC_IncRC( t );
	MAP_Insert( self->lookupTable, name, LOOKUP_BIND( DAO_CLASS_VARIABLE, s, 0, self->glbData->size ) );
	DVarray_Append( self->glbData, daoNullValue );
	DArray_Append( self->glbDataType, (void*)t );
	DArray_Append( self->glbDataName, (void*)name );
	if( data.t && DValue_Move( data, self->glbData->data + self->glbData->size -1, t ) ==0 )
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
	if( node ) return (DaoRoutine*) node->value.pBase;
	return NULL;
}
void DaoClass_PrintCode( DaoClass *self, DaoStream *stream )
{
	int j;
	DNode *node = DMap_First( self->lookupTable );
	DaoStream_WriteMBS( stream, "class " );
	DaoStream_WriteString( stream, self->className );
	DaoStream_WriteMBS( stream, ":\n" );
	DaoRoutine_PrintCode( self->classRoutine, stream );
	for( ; node != NULL; node = DMap_Next( self->lookupTable, node ) ){
		DValue val;
		if( LOOKUP_ST( node->value.pSize ) != DAO_CLASS_CONSTANT ) continue;
		val = self->cstData->data[ LOOKUP_ID( node->value.pSize ) ];
		if( val.t == DAO_ROUTINE ){
			DaoRoutine *rout = val.v.routine;
			if( rout->minimal == 1 ){
				for(j=0; j<rout->routTable->size; j++){
					if( rout->routTable->items.pRout[j]->routHost == self->objType ){
						DaoRoutine_PrintCode( rout->routTable->items.pRout[j], stream );
					}
				}
			}else{
				DaoRoutine_PrintCode( rout, stream );
			}
		}
	}
}
DaoRoutine* DaoClass_FindOperator( DaoClass *self, const char *oper, DaoClass *scoped )
{
	DValue value = daoNullValue;
	DString name = DString_WrapMBS( oper );
	DaoClass_GetData( self, & name, & value, scoped, NULL );
	if( value.t != DAO_ROUTINE ) return NULL;
	return value.v.routine;
}
