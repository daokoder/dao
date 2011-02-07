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

#include"stdio.h"
#include"string.h"

#include"daoObject.h"
#include"daoClass.h"
#include"daoRoutine.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoVmspace.h"
#include"daoGC.h"
#include"daoStream.h"
#include"daoNumtype.h"

int DaoObject_InvokeMethod( DaoObject *self, DaoObject *thisObject,
		DaoVmProcess *vmp, DString *name, DaoContext *ctx, DValue *ps[], int N, int ret )
{
	DValue value = daoNullValue;
	DValue selfpar = daoNullObject;
	int i, errcode = DaoObject_GetData( self, name, & value, thisObject, NULL );
	if( errcode ) return errcode;
	selfpar.v.object = self;
	if( value.t == DAO_ROUTINE ){
		DaoRoutine *rout = value.v.routine;
		DaoContext *ctxNew = DaoVmProcess_MakeContext( vmp, rout );
		GC_ShiftRC( self, ctxNew->object );
		ctxNew->object = self;
		DaoContext_Init( ctxNew, rout );
		DaoContext_InitWithParams( ctxNew, vmp, ps, N );
		if( DRoutine_PassParams( (DRoutine*) ctxNew->routine, &selfpar, 
					ctxNew->regValues, ps, NULL, N, DVM_CALL ) ){
			if( STRCMP( name, "_PRINT" ) ==0 ){
				DaoVmProcess_PushContext( ctx->process, ctxNew );
				DaoVmProcess_Execute( ctx->process );
			}else{
				DaoVmProcess_PushContext( ctx->process, ctxNew );
				if( ret > -10 ) ctx->process->topFrame->returning = (ushort_t) ret;
			}
			return 0;
		}
		DaoVmProcess_CacheContext( vmp, ctxNew );
		return DAO_ERROR_PARAM;
	}else if( value.t == DAO_FUNCTION ){
		DaoFunction *func = value.v.func;
		DValue *ps2[ DAO_MAX_PARAM+1 ];
		DValue self0 = daoNullObject;
		memcpy( ps2+1, ps, N*sizeof(DValue*) );
		ps2[0] = & self0;
		self0.v.object = (DaoObject*) DaoObject_MapThisObject( self, func->routHost );
		self0.t = self0.v.object ? self0.v.object->type : 0;
		func = (DaoFunction*)DRoutine_GetOverLoad( (DRoutine*) func, &selfpar, ps2, N+1, DVM_MCALL );
		DaoFunction_SimpleCall( func, ctx, ps2, N+1 );
	}
	return 0;
}
static void DaoObject_Print( DValue *self0, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
	DaoObject *self = self0->v.object;
	DValue vs = daoNullStream;
	DValue *pars = & vs;
	int ec;
	vs.v.stream = stream;
	DString_SetMBS( ctx->process->mbstring, "_PRINT" );
	ec = DaoObject_InvokeMethod( self, ctx->object, ctx->process,
			ctx->process->mbstring, ctx, & pars, 1, -1 );
	if( ec && ec != DAO_ERROR_FIELD_NOTEXIST ){
		DaoContext_RaiseException( ctx, ec, DString_GetMBS( ctx->process->mbstring ) );
	}else if( ec == DAO_ERROR_FIELD_NOTEXIST ){
		char buf[50];
		sprintf( buf, "[%p]", self );
		DaoStream_WriteString( stream, self->myClass->className );
		DaoStream_WriteMBS( stream, buf );
	}
}
static void DaoObject_Core_GetField( DValue *self0, DaoContext *ctx, DString *name )
{
	DaoObject *self = self0->v.object;
	DValue *d2 = NULL;
	DValue value = daoNullValue;
	int rc = DaoObject_GetData( self, name, & value, ctx->object, & d2 );
	if( rc ){
		DString_SetMBS( ctx->process->mbstring, "." );
		DString_Append( ctx->process->mbstring, name );
		rc = DaoObject_InvokeMethod( self, ctx->object, ctx->process,
				ctx->process->mbstring, ctx, NULL, 0, -100 );
	}else{
		DaoContext_PutReference( ctx, d2 );
	}
	if( rc ) DaoContext_RaiseException( ctx, rc, DString_GetMBS( name ) );
}
static void DaoObject_Core_SetField( DValue *self0, DaoContext *ctx, DString *name, DValue value )
{
	DaoObject *self = self0->v.object;
	DValue *par = & value;
	int ec = DaoObject_SetData( self, name, value, ctx->object );
	int ec2 = ec;
	if( ec ){
		DString_SetMBS( ctx->process->mbstring, "." );
		DString_Append( ctx->process->mbstring, name );
		DString_AppendMBS( ctx->process->mbstring, "=" );
		ec = DaoObject_InvokeMethod( self, ctx->object, ctx->process,
				ctx->process->mbstring, ctx, & par, 1, -1 );
		if( ec == DAO_ERROR_FIELD_NOTEXIST ) ec = ec2;
	}
	if( ec ) DaoContext_RaiseException( ctx, ec, DString_GetMBS( name ) );
}
static void DaoObject_GetItem( DValue *self0, DaoContext *ctx, DValue *ids[], int N )
{
	DaoObject *self = self0->v.object;
	int rc = 0;
	DString_SetMBS( ctx->process->mbstring, "[]" );
	rc = DaoObject_InvokeMethod( self, ctx->object, ctx->process,
			ctx->process->mbstring, ctx, ids, N, -100 );
	if( rc ) DaoContext_RaiseException( ctx, rc, DString_GetMBS( ctx->process->mbstring ) );
}
static void DaoObject_SetItem( DValue *self0, DaoContext *ctx, DValue *ids[], int N, DValue value )
{
	DaoObject *self = self0->v.object;
	DValue *ps[ DAO_MAX_PARAM ];
	int rc;
	memcpy( ps, ids, N*sizeof(DValue*) );
	ps[N] = & value;
	DString_SetMBS( ctx->process->mbstring, "[]=" );
	rc = DaoObject_InvokeMethod( self, ctx->object, ctx->process,
			ctx->process->mbstring, ctx, ps, N+1, -1 );
	if( rc ) DaoContext_RaiseException( ctx, rc, DString_GetMBS( ctx->process->mbstring ) );
}
extern void DaoCopyValues( DValue *copy, DValue *data, int N, DaoContext *ctx, DMap *cycData );
void DaoObject_CopyData( DaoObject *self, DaoObject *from, DaoContext *ctx, DMap *cycData )
{
	DaoObject **selfSups = NULL;
	DaoObject **fromSups = NULL;
	DValue *selfValues = self->objValues;
	DValue *fromValues = from->objValues;
	int i, selfSize = self->myClass->objDataDefault->size;
	DaoCopyValues( selfValues + 1, fromValues + 1, selfSize-1, ctx, cycData );
	/*  XXX super might be CData: */
	if( from->superObject ==NULL ) return;
	selfSups = self->superObject->items.pObject;
	fromSups = from->superObject->items.pObject;
	for( i=0; i<from->superObject->size; i++ )
		DaoObject_CopyData( (DaoObject*) selfSups[i], (DaoObject*) fromSups[i], ctx, cycData );
}
static DValue DaoObject_Copy(  DValue *value, DaoContext *ctx, DMap *cycData )
{
	DaoObject *pnew, *self = value->v.object;
	DValue res = daoNullObject;
	DNode *node = DMap_Find( cycData, self );
	if( node ){
		res.v.p = node->value.pBase;
		return res;
	}
	if( self->trait & DAO_DATA_NOCOPY ) return *value;

	pnew = DaoObject_Allocate( self->myClass );
	res.v.object = pnew;
	DMap_Insert( cycData, self, pnew );
	DaoObject_Init( pnew, NULL, 0 );
	DaoObject_CopyData( pnew, self, ctx, cycData );

	return res;
}

static DaoTypeCore objCore = 
{
	0, NULL, NULL, NULL, NULL,
	DaoObject_Core_GetField,
	DaoObject_Core_SetField,
	DaoObject_GetItem,
	DaoObject_SetItem,
	DaoObject_Print,
	DaoObject_Copy
};

DaoTypeBase objTyper=
{
	"object", & objCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoObject_Delete, NULL
};

DaoObject* DaoObject_Allocate( DaoClass *klass )
{
	DaoObject *self = (DaoObject*) dao_malloc( sizeof( DaoObject ) );
	DaoBase_Init( self, DAO_OBJECT );
	self->myClass = klass;
	self->objData = NULL;
	self->superObject = NULL;
	self->meta = NULL;
	GC_IncRC( klass );
	return self;
}
DaoObject* DaoObject_New( DaoClass *klass, DaoObject *that, int offset )
{
	DaoObject *self = DaoObject_Allocate( klass );
	DaoObject_Init( self, that, offset );
	return self;
}
void DaoObject_Init( DaoObject *self, DaoObject *that, int offset )
{
	DaoClass *klass = self->myClass;
	int i;

	if( that ){
		self->that = that;
		self->objValues = that->objData->data + offset;
	}else{
		self->that = self;
		self->objData = DVaTuple_New( klass->objDataName->size, daoNullValue );
		self->objValues = self->objData->data;
	}
	offset += self->myClass->objDefCount;
	if( klass->superClass->size ){
		self->superObject = DPtrTuple_New( klass->superClass->size, NULL );
		for(i=0; i<klass->superClass->size; i++){
			DaoClass *supclass = klass->superClass->items.pClass[i];
			DaoObject *sup = NULL;
			if( supclass->type == DAO_CLASS ){
				sup = DaoObject_New( supclass, self->that, offset );
				sup->refCount ++;
				offset += sup->myClass->objDataName->size;
			}
			self->superObject->items.pObject[i] = sup;
		}
	}
	self->objValues[0].t = DAO_OBJECT;
	self->objValues[0].v.object = self;
	GC_IncRC( self );
	if( self->objData == NULL ) return;
	for(i=1; i<klass->objDataDefault->size; i++){
		DaoType *type = klass->objDataType->items.pType[i];
		DValue *value = self->objValues + i;
		/* for data type such as list/map/array, 
		 * its .unitype may need to be set properaly */
		if( klass->objDataDefault->data[i].t ){
			DValue_Move( klass->objDataDefault->data[i], value, type );
			continue;
		}else if( value->t == 0 && type && type->value.t ){
			DValue_Move( type->value, value, type );
		}
	}
}
void DaoObject_Delete( DaoObject *self )
{
	if( self->myClass ) GC_DecRC( self->myClass );
	if( self->meta ) GC_DecRC( self->meta );
	if( self->objData ) DVaTuple_Delete( self->objData );
	if( self->superObject ){
		int i;
		for(i=0; i<self->superObject->size; i++)
			GC_DecRC( self->superObject->items.pBase[i] );
		DPtrTuple_Delete( self->superObject );
	}
	dao_free( self );
}

DaoClass* DaoObject_MyClass( DaoObject *self )
{
	return self->myClass;
}
int DaoObject_ChildOf( DaoObject *self, DaoObject *obj )
{
	int i;
	if( obj == self ) return 1;
	if( self->type == DAO_CDATA ){
		if( obj->type == DAO_CDATA ){
			DaoCData *cdata1 = (DaoCData*) self;
			DaoCData *cdata2 = (DaoCData*) obj;
			if( DaoCData_ChildOf( cdata1->typer, cdata2->typer ) ) return 1;
		}
		return 0;
	}
	if( self->superObject ==NULL ) return 0;
	for( i=0; i<self->superObject->size; i++ ){
		if( obj == self->superObject->items.pObject[i] ) return 1;
		if( DaoObject_ChildOf( self->superObject->items.pObject[i], obj ) ) return 1;
	}
	return 0;
}
extern int DaoCData_ChildOf( DaoTypeBase *self, DaoTypeBase *super );

DaoBase* DaoObject_MapThisObject( DaoObject *self, DaoType *host )
{
	int i;
	if( host == NULL ) return NULL;
	if( self->myClass->objType == host ) return (DaoBase*) self;
	if( self->superObject ==NULL ) return NULL;
	for( i=0; i<self->superObject->size; i++ ){
		DaoBase *sup = self->superObject->items.pBase[i];
		if( sup == NULL ) return NULL;
		if( sup->type == DAO_OBJECT ){
			if( (sup = DaoObject_MapThisObject( (DaoObject*)sup, host ) ) ) return sup;
		}else if( sup->type == DAO_CDATA && host->tid == DAO_CDATA ){
			if( DaoCData_ChildOf( ((DaoCData*)sup)->typer, host->typer ) ) return sup;
		}
	}
	return NULL;
}
DaoBase* DaoObject_MapChildObject( DaoObject *self, DaoType *parent )
{
	int i;
	if( parent == NULL ) return NULL;
	if( self->myClass->objType == parent ) return NULL;
	if( self->superObject ==NULL ) return NULL;
	for( i=0; i<self->superObject->size; i++ ){
		DaoBase *sup = self->myClass->superClass->items.pBase[i];
		if( sup == NULL ) continue;
		if( sup->type == DAO_CLASS ){
			if( ((DaoClass*)sup)->objType == parent ) return (DaoBase*) self;
		}else if( sup->type == DAO_CDATA ){
			DaoCData *cdata = (DaoCData*)sup;
			if( DaoCData_ChildOf( cdata->typer, parent->typer ) ) return (DaoBase*) self;
		}
		sup = self->superObject->items.pBase[i];
		if( sup == NULL ) continue;
		if( sup->type == DAO_OBJECT ){
			DaoObject *obj = (DaoObject*)sup;
			if( obj->myClass->objType == parent ) return (DaoBase*) self;
			return DaoObject_MapChildObject( obj, parent );
		}
	}
	return NULL;
}
DaoCData* DaoObject_MapCData( DaoObject *self, DaoTypeBase *typer )
{
	DaoBase *p = NULL;
	if( typer && typer->priv && typer->priv->abtype )
		p = DaoObject_MapThisObject( self, typer->priv->abtype );
	if( p && p->type == DAO_CDATA ) return (DaoCData*) p;
	return NULL;
}

void DaoObject_AddData( DaoObject *self, DString *name, DaoBase  *data )
{
}
int DaoObject_SetData( DaoObject *self, DString *name, DValue data, DaoObject *objThis )
{
	DaoClass *klass = self->myClass;
	DaoType *type;
	DValue *value ;
	DNode *node;
	int id, sto, up, perm;

	node = DMap_Find( self->myClass->lookupTable, name );
	if( node == NULL ) return DAO_ERROR_FIELD_NOTEXIST;

	perm = LOOKUP_PM( node->value.pSize );
	sto = LOOKUP_ST( node->value.pSize );
	up = LOOKUP_UP( node->value.pSize );
	id = LOOKUP_ID( node->value.pSize );
	if( objThis == self || perm == DAO_DATA_PUBLIC
			|| (objThis && DaoObject_ChildOf( objThis, self ) && perm >= DAO_DATA_PROTECTED) ){
		if( sto == DAO_OBJECT_VARIABLE ){
			if( id <0 ) return DAO_ERROR_FIELD_NOTPERMIT;
			type = klass->objDataType->items.pType[ id ];
			value = self->objValues + id;
			DValue_Move( data, value, type );
		}else if( sto == DAO_CLASS_VARIABLE ){
			value = klass->glbDataTable->items.pVarray[up]->data + id;
			type = klass->glbTypeTable->items.pArray[up]->items.pType[ id ];
			DValue_Move( data, value, type );
		}else if( sto == DAO_CLASS_CONSTANT ){
			return DAO_ERROR_FIELD;
		}else{
			return DAO_ERROR_FIELD;
		}
	}else{
		return DAO_ERROR_FIELD_NOTPERMIT;
	}
	return 0;
}
int DaoObject_GetData( DaoObject *self, DString *name, DValue *data, DaoObject *objThis, DValue **d2 )
{
	DaoClass *klass = self->myClass;
	DValue *p = NULL;
	DNode *node;
	int id, sto, up, perm;

	*data = daoNullValue;
	node = DMap_Find( self->myClass->lookupTable, name );
	if( node == NULL ) return DAO_ERROR_FIELD_NOTEXIST;

	perm = LOOKUP_PM( node->value.pSize );
	sto = LOOKUP_ST( node->value.pSize );
	up = LOOKUP_UP( node->value.pSize );
	id = LOOKUP_ID( node->value.pSize );
	if( objThis == self || perm == DAO_DATA_PUBLIC 
			|| (objThis && DaoObject_ChildOf( objThis, self ) && perm >= DAO_DATA_PROTECTED) ){
		switch( sto ){
		case DAO_OBJECT_VARIABLE : p = self->objValues + id; break;
		case DAO_CLASS_VARIABLE  : p = klass->glbDataTable->items.pVarray[up]->data + id; break;
		case DAO_CLASS_CONSTANT  : p = klass->cstDataTable->items.pVarray[up]->data + id; break;
		default : break;
		}
		if( p ) *data = *p;
		if( d2 ) *d2 = p;
	}else{
		return DAO_ERROR_FIELD_NOTPERMIT;
	}
	return 0;
}

DValue DaoObject_GetField( DaoObject *self, const char *name )
{
	DValue res = daoNullValue;
	DString str = DString_WrapMBS( name );
	DaoObject_GetData( self, & str, & res, self, NULL );
	return res;
}
