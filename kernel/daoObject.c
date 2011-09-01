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
#include"daoValue.h"

int DaoObject_InvokeMethod( DaoObject *self, DaoObject *othis, DaoProcess *proc, 
		DString *name, DaoValue *P[], int N, int ignore_return, int execute )
{
	DaoValue *V = NULL;
	DaoValue *O = (DaoValue*)self;
	int errcode = DaoObject_GetData( self, name, &V, othis );
	if( errcode ) return errcode;
	if( V == NULL || V->type < DAO_FUNCTREE || V->type > DAO_FUNCTION ) return DAO_ERROR_TYPE;
	if( DaoProcess_PushCallable( proc, V, O, P, N ) ) goto InvalidParam;
	if( ignore_return ) DaoProcess_InterceptReturnValue( proc );
	if( execute ) DaoProcess_Execute( proc );
	return 0;
InvalidParam:
	DaoProcess_ShowCallError( proc, (DRoutine*)V, O, P, N, DVM_CALL );
	return 0;
}
static void DaoObject_Print( DaoValue *self0, DaoProcess *proc, DaoStream *stream, DMap *cycData )
{
	int ec;
	DaoObject *self = & self0->xObject;
	if( self0 == self->myClass->objType->value ){
		DaoStream_WriteString( stream, self->myClass->className );
		DaoStream_WriteMBS( stream, "[default]" );
		return;
	}
	DString_SetMBS( proc->mbstring, "serialize" );
	DaoValue_Clear( & proc->stackValues[0] );
	ec = DaoObject_InvokeMethod( self, proc->activeObject, proc, proc->mbstring, NULL,0,1,1 );
	if( ec && ec != DAO_ERROR_FIELD_NOTEXIST ){
		DaoProcess_RaiseException( proc, ec, DString_GetMBS( proc->mbstring ) );
	}else if( ec == DAO_ERROR_FIELD_NOTEXIST || proc->stackValues[0] == NULL ){
		char buf[50];
		sprintf( buf, "[%p]", self );
		DaoStream_WriteString( stream, self->myClass->className );
		DaoStream_WriteMBS( stream, buf );
	}else{
		DaoValue_Print( proc->stackValues[0], proc, stream, cycData );
	}
}
static void DaoObject_Core_GetField( DaoValue *self0, DaoProcess *proc, DString *name )
{
	DaoObject *self = & self0->xObject;
	DaoValue *value = NULL;
	int rc = DaoObject_GetData( self, name, & value, proc->activeObject );
	if( rc ){
		DString_SetMBS( proc->mbstring, "." );
		DString_Append( proc->mbstring, name );
		rc = DaoObject_InvokeMethod( self, proc->activeObject, proc, proc->mbstring, NULL,0,0,0 );
	}else{
		DaoProcess_PutReference( proc, value );
	}
	if( rc ) DaoProcess_RaiseException( proc, rc, DString_GetMBS( name ) );
}
static void DaoObject_Core_SetField( DaoValue *self0, DaoProcess *proc, DString *name, DaoValue *value )
{
	DaoObject *self = & self0->xObject;
	int ec = DaoObject_SetData( self, name, value, proc->activeObject );
	int ec2 = ec;
	if( ec ){
		DString *mbs = proc->mbstring;
		DString_SetMBS( mbs, "." );
		DString_Append( mbs, name );
		DString_AppendMBS( mbs, "=" );
		ec = DaoObject_InvokeMethod( self, proc->activeObject, proc, mbs, & value, 1,1,0 );
		if( ec == DAO_ERROR_FIELD_NOTEXIST ) ec = ec2;
	}
	if( ec ) DaoProcess_RaiseException( proc, ec, DString_GetMBS( name ) );
}
static void DaoObject_GetItem( DaoValue *self0, DaoProcess *proc, DaoValue *ids[], int N )
{
	DaoObject *self = & self0->xObject;
	int rc = 0;
	DString_SetMBS( proc->mbstring, "[]" );
	rc = DaoObject_InvokeMethod( self, proc->activeObject, proc, proc->mbstring, ids, N,0,0 );
	if( rc ) DaoProcess_RaiseException( proc, rc, DString_GetMBS( proc->mbstring ) );
}
static void DaoObject_SetItem( DaoValue *self0, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	DaoObject *self = & self0->xObject;
	DaoValue *ps[ DAO_MAX_PARAM ];
	int rc;
	memcpy( ps, ids, N*sizeof(DaoValue*) );
	ps[N] = value;
	DString_SetMBS( proc->mbstring, "[]=" );
	rc = DaoObject_InvokeMethod( self, proc->activeObject, proc, proc->mbstring, ps, N+1,1,0 );
	if( rc ) DaoProcess_RaiseException( proc, rc, DString_GetMBS( proc->mbstring ) );
}
extern void DaoCopyValues( DaoValue **copy, DaoValue **data, int N, DaoProcess *proc, DMap *cycData );
void DaoObject_CopyData( DaoObject *self, DaoObject *from, DaoProcess *proc, DMap *cycData )
{
	DaoObject **selfSups = NULL;
	DaoObject **fromSups = NULL;
	DaoValue **selfValues = self->objValues;
	DaoValue **fromValues = from->objValues;
	int i, selfSize = self->myClass->objDataDefault->size;
	DaoCopyValues( selfValues + 1, fromValues + 1, selfSize-1, proc, cycData );
	/*  XXX super might be Cdata: */
	if( from->superObject ==NULL ) return;
	selfSups = self->superObject->items.pObject;
	fromSups = from->superObject->items.pObject;
	for( i=0; i<from->superObject->size; i++ )
		DaoObject_CopyData( (DaoObject*) selfSups[i], (DaoObject*) fromSups[i], proc, cycData );
}
static DaoValue* DaoObject_Copy(  DaoValue *value, DaoProcess *proc, DMap *cycData )
{
	DaoObject *pnew, *self = & value->xObject;
	DNode *node = DMap_Find( cycData, self );
	if( node ) return node->value.pValue;
	if( self->trait & DAO_DATA_NOCOPY ) return value;

	pnew = DaoObject_Allocate( self->myClass );
	DMap_Insert( cycData, self, pnew );
	DaoObject_Init( pnew, NULL, 0 );
	DaoObject_CopyData( pnew, self, proc, cycData );

	return (DaoValue*) pnew;
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
	DaoValue_Init( self, DAO_OBJECT );
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
	int i, defobj = self == & klass->objType->value->xObject;

	if( that ){
		self->that = that;
		self->objValues = that->objData->items.pValue + offset;
	}else{
		self->that = self;
		self->objData = DTuple_New( klass->objDataName->size, NULL );
		self->objValues = self->objData->items.pValue;
	}
	offset += self->myClass->objDefCount;
	if( klass->superClass->size ){
		self->superObject = DTuple_New( klass->superClass->size, NULL );
		for(i=0; i<klass->superClass->size; i++){
			DaoClass *supclass = klass->superClass->items.pClass[i];
			DaoObject *sup = NULL;
			if( supclass->type == DAO_CLASS ){
				if( defobj ){
					sup = & supclass->objType->value->xObject;
				}else{
					sup = DaoObject_New( supclass, self->that, offset );
				}
				GC_IncRC( sup );
				offset += sup->myClass->objDataName->size;
			}
			self->superObject->items.pObject[i] = sup;
		}
	}
	self->objValues[0] = (DaoValue*) self;
	GC_IncRC( self );
	if( self->objData == NULL ) return;
	for(i=1; i<klass->objDataDefault->size; i++){
		DaoType *type = klass->objDataType->items.pType[i];
		DaoValue **value = self->objValues + i;
		/* for data type such as list/map/array, 
		 * its .unitype may need to be set properaly */
		if( klass->objDataDefault->items.pValue[i] ){
			DaoValue_Move( klass->objDataDefault->items.pValue[i], value, type );
			continue;
		}else if( *value == NULL && type && type->value ){
			DaoValue_Copy( type->value, value );
		}
	}
}
void DaoObject_Delete( DaoObject *self )
{
	if( self->myClass ) GC_DecRC( self->myClass );
	if( self->meta ) GC_DecRC( self->meta );
	if( self->objData ) DTuple_Delete( self->objData );
	if( self->superObject ){
		int i;
		for(i=0; i<self->superObject->size; i++)
			GC_DecRC( self->superObject->items.pValue[i] );
		DTuple_Delete( self->superObject );
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
			DaoCdata *cdata1 = (DaoCdata*) self;
			DaoCdata *cdata2 = (DaoCdata*) obj;
			if( DaoCdata_ChildOf( cdata1->typer, cdata2->typer ) ) return 1;
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
extern int DaoCdata_ChildOf( DaoTypeBase *self, DaoTypeBase *super );

DaoValue* DaoObject_MapThisObject( DaoObject *self, DaoType *host )
{
	int i;
	if( host == NULL ) return NULL;
	if( self->myClass->objType == host ) return (DaoValue*) self;
	if( self->superObject ==NULL ) return NULL;
	for( i=0; i<self->superObject->size; i++ ){
		DaoValue *sup = self->superObject->items.pValue[i];
		if( sup == NULL ) return NULL;
		if( sup->type == DAO_OBJECT ){
			if( (sup = DaoObject_MapThisObject( & sup->xObject, host ) ) ) return sup;
		}else if( sup->type == DAO_CDATA && host->tid == DAO_CDATA ){
			if( DaoCdata_ChildOf( sup->xCdata.typer, host->typer ) ) return sup;
		}
	}
	return NULL;
}
DaoObject* DaoObject_SetParentCdata( DaoObject *self, DaoCdata *parent )
{
	int i;
	DaoObject *child = NULL;
	if( parent == NULL ) return NULL;
	if( self->superObject ==NULL ) return NULL;
	for( i=0; i<self->superObject->size; i++ ){
		DaoObject *obj = self->superObject->items.pObject[i];
		DaoValue *sup = self->myClass->superClass->items.pValue[i];
		if( sup == NULL ) continue;
		if( obj ){
			if( sup->type == DAO_CLASS ){
				DaoObject *o = DaoObject_SetParentCdata( obj, parent );
				/* TODO: map to first common child for multiple inheritance: */
				if( o ) child = o;
			}
			continue;
		}
		if( sup->type == DAO_CTYPE ){
			DaoCdata *cdata = (DaoCdata*)sup;
			if( DaoCdata_ChildOf( cdata->typer, parent->typer ) ){
				GC_IncRC( parent );
				self->superObject->items.pValue[i] = (DaoValue*)parent;
				return self;
			}
		}
	}
	return child;
}
DaoCdata* DaoObject_MapCdata( DaoObject *self, DaoTypeBase *typer )
{
	DaoValue *p = NULL;
	if( typer && typer->priv && typer->priv->abtype )
		p = DaoObject_MapThisObject( self, typer->priv->abtype );
	if( p && p->type == DAO_CDATA ) return (DaoCdata*) p;
	return NULL;
}

void DaoObject_AddData( DaoObject *self, DString *name, DaoValue *data )
{
}
int DaoObject_SetData( DaoObject *self, DString *name, DaoValue *data, DaoObject *othis )
{
	DNode *node;
	DaoType *type;
	DaoValue **value ;
	DaoClass *klass = self->myClass;
	DaoObject *dft = & klass->objType->value->xObject;
	int child = othis && DaoObject_ChildOf( othis, self );
	int id, st, up, pm, access;

	node = DMap_Find( self->myClass->lookupTable, name );
	if( node == NULL ) return DAO_ERROR_FIELD_NOTEXIST;

	pm = LOOKUP_PM( node->value.pSize );
	st = LOOKUP_ST( node->value.pSize );
	up = LOOKUP_UP( node->value.pSize );
	id = LOOKUP_ID( node->value.pSize );
	if( self == dft && st == DAO_OBJECT_VARIABLE ) return DAO_ERROR_FIELD_NOTPERMIT;
	access = othis == self || pm == DAO_DATA_PUBLIC || (child && pm >= DAO_DATA_PROTECTED);
	if( access == 0 ) return DAO_ERROR_FIELD_NOTPERMIT;
	if( st == DAO_OBJECT_VARIABLE ){
		if( id <0 ) return DAO_ERROR_FIELD_NOTPERMIT;
		type = klass->objDataType->items.pType[ id ];
		value = self->objValues + id;
		if( DaoValue_Move( data, value, type ) ==0 ) return DAO_ERROR_VALUE;
	}else if( st == DAO_CLASS_VARIABLE ){
		value = klass->glbDataTable->items.pArray[up]->items.pValue + id;
		type = klass->glbTypeTable->items.pArray[up]->items.pType[ id ];
		if( DaoValue_Move( data, value, type ) ==0 ) return DAO_ERROR_VALUE;
	}else if( st == DAO_CLASS_CONSTANT ){
		return DAO_ERROR_FIELD;
	}else{
		return DAO_ERROR_FIELD;
	}
	return 0;
}
int DaoObject_GetData( DaoObject *self, DString *name, DaoValue **data, DaoObject *othis )
{
	DNode *node;
	DaoValue *p = NULL;
	DaoClass *klass = self->myClass;
	DaoObject *dft = & klass->objType->value->xObject;
	int child = othis && DaoObject_ChildOf( othis, self );
	int id, st, up, pm, access;

	*data = NULL;
	node = DMap_Find( self->myClass->lookupTable, name );
	if( node == NULL ) return DAO_ERROR_FIELD_NOTEXIST;

	pm = LOOKUP_PM( node->value.pSize );
	st = LOOKUP_ST( node->value.pSize );
	up = LOOKUP_UP( node->value.pSize );
	id = LOOKUP_ID( node->value.pSize );
	if( self == dft && st == DAO_OBJECT_VARIABLE ) return DAO_ERROR_FIELD_NOTPERMIT;
	access = othis == self || pm == DAO_DATA_PUBLIC || (child && pm >= DAO_DATA_PROTECTED);
	if( access == 0 ) return DAO_ERROR_FIELD_NOTPERMIT;
	switch( st ){
	case DAO_OBJECT_VARIABLE : p = self->objValues[id]; break;
	case DAO_CLASS_VARIABLE  : p = klass->glbDataTable->items.pArray[up]->items.pValue[id]; break;
	case DAO_CLASS_CONSTANT  : p = klass->cstDataTable->items.pArray[up]->items.pValue[id]; break;
	default : break;
	}
	*data = p;
	return 0;
}

DaoValue* DaoObject_GetField( DaoObject *self, const char *name )
{
	DaoValue *res = NULL;
	DString str = DString_WrapMBS( name );
	DaoObject_GetData( self, & str, & res, self );
	return res;
}
DaoMethod* DaoObject_GetMethod( DaoObject *self, const char *name )
{
	DaoValue *V;
	DString str = DString_WrapMBS( name );
	int id = DaoClass_FindConst( self->myClass, & str );
	if( id < 0 ) return NULL;
	V = DaoClass_GetConst( self->myClass, id );
	if( V == NULL || V->type < DAO_FUNCTREE || V->type > DAO_FUNCTION ) return NULL;
	return (DaoMethod*) V;
}
