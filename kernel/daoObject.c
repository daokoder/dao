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

#include"stdio.h"
#include"string.h"

#include"daoObject.h"
#include"daoClass.h"
#include"daoRoutine.h"
#include"daoProcess.h"
#include"daoVmspace.h"
#include"daoGC.h"
#include"daoStream.h"
#include"daoNumtype.h"
#include"daoValue.h"


DaoObject* DaoObject_Allocate( DaoClass *klass, int value_count )
{
	int extra = value_count * sizeof(DaoValue*);
	DaoObject *self = (DaoObject*) dao_calloc( 1, sizeof(DaoObject) + extra );

	DaoValue_Init( self, DAO_OBJECT );
	GC_IncRC( klass );
	self->defClass = klass;
	self->isRoot = 1;
	self->valueCount = value_count;
	self->objValues = (DaoValue**) (self + 1);
	memset( self->objValues, 0, value_count*sizeof(DaoValue*) );
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogNew( (DaoValue*) self );
#endif
	return self;
}

DaoObject* DaoObject_New( DaoClass *klass )
{
	DaoObject *self = DaoObject_Allocate( klass, klass->objDataName->size );
	GC_IncRC( self );
	self->rootObject = self;
	DaoObject_Init( self, NULL, 0 );
	return self;
}

void DaoObject_Init( DaoObject *self, DaoObject *that, int offset )
{
	DaoClass *klass = self->defClass;
	daoint i;

	self->isAsync = (klass->attribs & DAO_CLS_ASYNCHRONOUS) != 0;
	if( that ){
		GC_Assign( & self->rootObject, that );
		self->objValues = that->objValues + offset;
	}else if( self->rootObject == NULL ){
		GC_Assign( & self->rootObject, self );
		if( self->isNull ){ /* no value space is allocated for null object yet! */
			self->valueCount = klass->objDataName->size;
			self->objValues = (DaoValue**) dao_calloc( self->valueCount, sizeof(DaoValue*) );
		}
	}
	offset += self->defClass->objDefCount;
	if( klass->parent != NULL && klass->parent->type == DAO_CLASS ){
		DaoObject *sup = NULL;
		if( self->isNull ){
			sup = & klass->parent->xClass.objType->value->xObject;
		}else{
			sup = DaoObject_Allocate( (DaoClass*) klass->parent, 0 );
			sup->isRoot = 0;
			DaoObject_Init( sup, self->rootObject, offset );
		}
		GC_Assign( & self->parent, sup );
	}
	GC_Assign( & self->objValues[0], self );
	if( self->isRoot == 0 ) return;
	for(i=1; i<klass->instvars->size; i++){
		DaoVariable *var = klass->instvars->items.pVar[i];
		DaoValue **value = self->objValues + i;
		/* for data type such as list/map/array,
		 * its .ctype may need to be set properaly */
		if( var->value ){
			DaoValue_Move( var->value, value, var->dtype );
		}else if( *value == NULL && var->dtype && var->dtype->value ){
			DaoValue_Copy( var->dtype->value, value );
		}
	}
}

void DaoObject_Delete( DaoObject *self )
{
	int i;
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogDelete( (DaoValue*) self );
#endif
	GC_DecRC( self->defClass );
	GC_DecRC( self->parent );
	if( self->isRoot ){
		for(i=0; i<self->valueCount; i++) GC_DecRC( self->objValues[i] );
		if( self->objValues != (DaoValue**) (self + 1) ) dao_free( self->objValues );
	}
	dao_free( self );
}

DaoClass* DaoObject_GetClass( DaoObject *self )
{
	return self->defClass;
}

int DaoObject_ChildOf( DaoObject *self, DaoValue *obj )
{
	if( obj == (DaoValue*) self ) return 1;
#if 0
	// No way to determine inheritance relationship between wrapped C++ objects;
	if( self->type >= DAO_CSTRUCT && self->type <= DAO_CDATA ){
		if( obj->type >= DAO_CSTRUCT && obj->type <= DAO_CDATA ){
			DaoCstruct *cdata1 = (DaoCstruct*) self;
			DaoCstruct *cdata2 = (DaoCstruct*) obj;
			if( DaoType_ChildOf( cdata1->ctype, cdata2->ctype ) ) return 1;
		}
		return 0;
	}
#endif
	if( self->parent == NULL ) return 0;
	if( self->parent == obj ) return 1;
	if( self->parent->type != DAO_OBJECT ) return 0;
	return DaoObject_ChildOf( (DaoObject*) self->parent, obj );
}

DaoValue* DaoObject_CastToBase( DaoObject *self, DaoType *host )
{
	DaoValue *sup = self->parent;
	if( host == NULL ) return NULL;
	host = DaoType_GetBaseType( host );
	if( self->defClass->objType == host ) return (DaoValue*) self;
	if( self->parent == NULL ) return NULL;
	if( sup->type == DAO_OBJECT ){
		if( (sup = DaoObject_CastToBase( & sup->xObject, host ) ) ) return sup;
	}else if( sup->type == DAO_CSTRUCT && host->tid == DAO_CSTRUCT ){
		/*
		// It is OK to return "sup". 
		// Because it must be binary-compatiable to the target type "host";
		*/
		if( DaoType_ChildOf( sup->xCstruct.ctype, host ) ) return sup;
	}else if( sup->type == DAO_CDATA && host->tid == DAO_CDATA ){
		/*
		// It is also OK to return "sup".
		// Because if it wraps a C struct, it must be binary-compatiable
		// to the target type "host"; And if it wraps a C++ object, the
		// real casting of the wrapped pointer will happen before using
		// in wrapped functions with DaoValue_TryCastCdata();
		*/
		if( DaoType_ChildOf( sup->xCstruct.ctype, host ) ) return sup;
	}
	return NULL;
}

void DaoObject_SetParentCstruct( DaoObject *self, DaoCstruct *parent )
{
	DaoObject *child = NULL;
	DaoObject *obj = (DaoObject*) self->parent;
	DaoValue *sup = self->defClass->parent;
	if( parent == NULL ) return;
	if( sup == NULL ) return;
	if( obj && obj->type == DAO_OBJECT ){
		DaoObject_SetParentCstruct( obj, parent );
	}else if( sup->type == DAO_CTYPE ){
		DaoCtype *ctype = (DaoCtype*)sup;
		if( DaoType_ChildOf( ctype->classType, parent->ctype ) ){
			DaoValue_MoveCstruct( (DaoValue*) parent, & self->parent, 1 );
		}
	}
}

DaoCstruct* DaoObject_CastCstruct( DaoObject *self, DaoType *type )
{
	DaoValue *p = NULL;
	if( type ) p = DaoObject_CastToBase( self, type );
	if( p && (p->type == DAO_CSTRUCT || p->type == DAO_CDATA) ) return (DaoCstruct*) p;
	return NULL;
}

DaoCdata* DaoObject_CastCdata( DaoObject *self, DaoType *type )
{
	DaoCstruct *p = DaoObject_CastCstruct( self, type );
	if( p && p->type == DAO_CDATA ) return (DaoCdata*) p;
	return NULL;
}

DaoCstruct* DaoObject_CastCstructTC( DaoObject *self, DaoTypeCore *core )
{
	DaoType *type = DaoVmSpace_GetType( self->defClass->nameSpace->vmSpace, core );
	DaoValue *p = NULL;
	if( type ) p = DaoObject_CastToBase( self, type );
	if( p && (p->type == DAO_CSTRUCT || p->type == DAO_CDATA) ) return (DaoCstruct*) p;
	return NULL;
}

DaoCdata* DaoObject_CastCdataTC( DaoObject *self, DaoTypeCore *core )
{
	DaoType *type = DaoVmSpace_GetType( self->defClass->nameSpace->vmSpace, core );
	DaoCstruct *p = DaoObject_CastCstruct( self, type );
	if( p && p->type == DAO_CDATA ) return (DaoCdata*) p;
	return NULL;
}

int DaoObject_SetData( DaoObject *self, DString *name, DaoValue *data, DaoObject *hostObject )
{
	DNode *node;
	DaoType *type;
	DaoValue **value ;
	DaoClass *klass = self->defClass;
	DaoObject *null = (DaoObject*) klass->objType->value;
	int child = hostObject && DaoObject_ChildOf( hostObject, (DaoValue*) self );
	int id, st, up, pm, access;

	if( self == null ) return DAO_ERROR;

	node = DMap_Find( self->defClass->lookupTable, name );
	if( node == NULL ) return DAO_ERROR_FIELD_ABSENT;

	pm = LOOKUP_PM( node->value.pInt );
	st = LOOKUP_ST( node->value.pInt );
	up = LOOKUP_UP( node->value.pInt );
	id = LOOKUP_ID( node->value.pInt );
	if( self == null && st == DAO_OBJECT_VARIABLE ) return DAO_ERROR_FIELD_HIDDEN;
	access = hostObject == self || pm == DAO_PERM_PUBLIC || (child && pm >= DAO_PERM_PROTECTED);
	if( access == 0 ) return DAO_ERROR_FIELD_HIDDEN;
	if( st == DAO_OBJECT_VARIABLE ){
		if( id <0 ) return DAO_ERROR_FIELD_HIDDEN;
		type = klass->instvars->items.pVar[ id ]->dtype;
		value = self->objValues + id;
		if( DaoValue_Move( data, value, type ) ==0 ) return DAO_ERROR_VALUE;
	}else if( st == DAO_CLASS_VARIABLE ){
		DaoVariable *var = klass->variables->items.pVar[id];
		if( DaoValue_Move( data, & var->value, var->dtype ) ==0 ) return DAO_ERROR_VALUE;
	}else if( st == DAO_CLASS_CONSTANT ){
		return DAO_ERROR_FIELD;
	}else{
		return DAO_ERROR_FIELD;
	}
	return 0;
}
int DaoObject_GetData( DaoObject *self, DString *name, DaoValue **data, DaoObject *hostObject )
{
	DNode *node;
	DaoValue *p = NULL;
	DaoClass *klass = self->defClass;
	DaoObject *null = (DaoObject*) klass->objType->value;
	int child = hostObject && DaoObject_ChildOf( hostObject, (DaoValue*) self );
	int id, st, up, pm, access;

	*data = NULL;
	node = DMap_Find( self->defClass->lookupTable, name );
	if( node == NULL ) return DAO_ERROR_FIELD_ABSENT;

	pm = LOOKUP_PM( node->value.pInt );
	st = LOOKUP_ST( node->value.pInt );
	up = LOOKUP_UP( node->value.pInt );
	id = LOOKUP_ID( node->value.pInt );
	if( self == null && st == DAO_OBJECT_VARIABLE ) return DAO_ERROR_FIELD_HIDDEN;
	access = hostObject == self || pm == DAO_PERM_PUBLIC || (child && pm >= DAO_PERM_PROTECTED);
	if( access == 0 ) return DAO_ERROR_FIELD_HIDDEN;
	switch( st ){
	case DAO_OBJECT_VARIABLE : p = self->objValues[id]; break;
	case DAO_CLASS_VARIABLE  : p = klass->variables->items.pVar[id]->value; break;
	case DAO_CLASS_CONSTANT  : p = klass->constants->items.pConst[id]->value; break;
	default : break;
	}
	*data = p;
	return DAO_OK;
}

DaoValue* DaoObject_GetField( DaoObject *self, const char *name )
{
	DaoValue *res = NULL;
	DString str = DString_WrapChars( name );
	DaoObject_GetData( self, & str, & res, self );
	return res;
}
DaoRoutine* DaoObject_GetMethod( DaoObject *self, const char *name )
{
	DaoValue *V;
	DString str = DString_WrapChars( name );
	int id = DaoClass_FindConst( self->defClass, & str );
	if( id < 0 ) return NULL;
	V = DaoClass_GetConst( self->defClass, id );
	if( V == NULL || V->type != DAO_ROUTINE ) return NULL;
	return (DaoRoutine*) V;
}



static DaoType* DaoObject_CheckGetField( DaoType *self, DaoString *name, DaoRoutine *ctx )
{
	DaoClass *klass = (DaoClass*) self->aux;
	DaoType *type = ctx->routHost;
	DaoVmSpace *vms = ctx->nameSpace->vmSpace;
	DaoClass *host = type != NULL && type->tid == DAO_OBJECT ? (DaoClass*) type->aux : NULL;
	DaoValue *data = DaoClass_GetData( klass, name->value, host );
	DaoRoutine *rout;
	int error = DAO_OK;

	if( data == NULL ){
		error = DAO_ERROR_FIELD_ABSENT;
	}else if( data->type == DAO_NONE ){
		error = DAO_ERROR_FIELD_HIDDEN;
	}else{
		switch( data->xBase.subtype ){
		case DAO_OBJECT_VARIABLE : return data->xVar.dtype;
		case DAO_CLASS_VARIABLE  : return data->xVar.dtype;
		case DAO_CLASS_CONSTANT  : return DaoNamespace_GetType( ctx->nameSpace, data->xConst.value );
		}
	}
	if( error ){
		DString *field = DString_NewChars( "." );
		DString_Append( field, name->value );
		rout = DaoClass_FindMethod( klass, field->chars, host );
		DString_Delete( field );
		if( rout != NULL ){
			rout = DaoRoutine_MatchByType( rout, self, NULL, 0, DVM_CALL );
		}else{
			rout = DaoClass_FindMethod( klass, ".", host );
			if( rout == NULL ) return NULL;
			rout = DaoRoutine_MatchByType( rout, self, & vms->typeString, 1, DVM_CALL );
		}
		if( rout == NULL ) return NULL;
		return (DaoType*) rout->routType->aux;
	}
	return NULL;
}

static DaoValue* DaoObject_DoGetField( DaoValue *self, DaoString *name, DaoProcess *proc )
{
	DaoObject *object = (DaoObject*) self;
	DaoObject *host = proc->activeObject;
	DaoClass *hostClass = host ? host->defClass : NULL;
	DaoValue *value = NULL;
	int rc = DaoObject_GetData( object, name->value, & value, host );
	if( rc ){
		DaoRoutine *rout;
		DString *field = proc->string;

		DString_SetChars( field, "." );
		DString_Append( field, name->value );
		rout = DaoClass_FindMethod( object->defClass, field->chars, hostClass );
		if( rout != NULL ){
			rc = DaoProcess_PushCall( proc, rout, self, NULL, 0 );
		}else{
			DaoValue *arg = (DaoValue*) name;
			rout = DaoClass_FindMethod( object->defClass, ".", hostClass );
			if( rout != NULL ) rc = DaoProcess_PushCall( proc, rout, self, & arg, 1 );
		}
		if( rout == NULL ) return NULL;
	}else{
		DaoProcess_PutValue( proc, value );
	}
	if( rc ) DaoProcess_RaiseError( proc, daoExceptionNames[rc], name->value->chars );
	return NULL;
}

static int DaoObject_CheckSetField( DaoType *self, DaoString *name, DaoType *value, DaoRoutine *ctx )
{
	DaoClass *klass = (DaoClass*) self->aux;
	DaoType *type = ctx->routHost;
	DaoClass *host = type != NULL && type->tid == DAO_OBJECT ? (DaoClass*) type->aux : NULL;
	DaoValue *data = DaoClass_GetData( klass, name->value, host );
	DaoVmSpace *vms = ctx->nameSpace->vmSpace;
	DaoRoutine *rout;
	int error = DAO_OK;

	error = 0;

	if( strcmp( name->value->chars, "self" ) ==0 ) return DAO_ERROR_FIELD_HIDDEN;
	if( data == NULL ){
		error = DAO_ERROR_FIELD_ABSENT;
	}else if( data->type == DAO_NONE ){
		error = DAO_ERROR_FIELD_HIDDEN;
	}else if( data->xBase.subtype == DAO_CLASS_CONSTANT ){
		error = DAO_ERROR_FIELD_HIDDEN; // XXX
	}else{
		/* data->xBase.subtype == DAO_CLASS_VARIABLE || DAO_OBJECT_VARIABLE */
		if( DaoType_MatchTo( value, data->xVar.dtype, NULL ) == 0 ) return DAO_ERROR_VALUE;
	}
	if( error ){
		DString *field = DString_NewChars( "." );
		DString_Append( field, name->value );
		DString_AppendChars( field, "=" );
		rout = DaoClass_FindMethod( klass, field->chars, host );
		DString_Delete( field );
		if( rout != NULL ){
			rout = DaoRoutine_MatchByType( rout, self, & value, 1, DVM_CALL );
		}else{
			DaoType *args[2];
			args[0] = vms->typeString;
			args[1] = value;
			rout = DaoClass_FindMethod( klass, ".=", host );
			if( rout == NULL ) return error;
			rout = DaoRoutine_MatchByType( rout, self, args, 2, DVM_CALL );
		}
		if( rout == NULL ) return error;
	}
	return DAO_OK;
}

static int DaoObject_DoSetField( DaoValue *self, DaoString *name, DaoValue *value, DaoProcess *proc )
{
	DaoObject *object = (DaoObject*) self;
	DaoObject *host = proc->activeObject;
	DaoClass *hostClass = host ? host->defClass : NULL;
	int ec = DaoObject_SetData( object, name->value, value, host );
	if( ec != DAO_OK ){
		DString *field = proc->string;
		DaoRoutine *rout;

		DString_SetChars( field, "." );
		DString_Append( field, name->value );
		DString_AppendChars( field, "=" );
		rout = DaoClass_FindMethod( object->defClass, field->chars, hostClass );
		if( rout != NULL ){
			ec = DaoProcess_PushCall( proc, rout, self, & value, 1 );
		}else{
			DaoValue *args[2];
			args[0] = (DaoValue*) name;
			args[1] = value;
			rout = DaoClass_FindMethod( object->defClass, ".=", hostClass );
			if( rout == NULL ) return DAO_ERROR_FIELD_ABSENT;
			ec = DaoProcess_PushCall( proc, rout, self, args, 2 );
		}
		if( rout == NULL ) return DAO_ERROR_FIELD_ABSENT;
	}
	if( ec ) DaoProcess_RaiseError( proc, daoExceptionNames[ec], name->value->chars );
	return ec;
}

static DaoType* DaoObject_CheckGetItem( DaoType *self, DaoType *index[], int N, DaoRoutine *ctx )
{
	DaoType *type = ctx->routHost;
	DaoClass *host = type != NULL && type->tid == DAO_OBJECT ? (DaoClass*) type->aux : NULL;
	DaoRoutine *rout = DaoClass_FindMethod( (DaoClass*) self->aux, "[]", host );

	if( rout != NULL ) rout = DaoRoutine_MatchByType( rout, self, index, N, DVM_CALL );
	if( rout == NULL ) return NULL;
	return (DaoType*) rout->routType->aux;
}

static DaoValue* DaoObject_DoGetItem( DaoValue *self, DaoValue *index[], int N, DaoProcess *proc )
{
	DaoObject *object = (DaoObject*) self;
	DaoClass *host = proc->activeObject ? proc->activeObject->defClass : NULL;
	DaoRoutine *rout = DaoClass_FindMethod( object->defClass, "[]", host );
	int rc = DaoProcess_PushCall( proc, rout, self, index, N );
	if( rc ) DaoProcess_RaiseError( proc, daoExceptionNames[rc], NULL );
	return NULL;
}

static int DaoObject_CheckSetItem( DaoType *self, DaoType *index[], int N, DaoType *value, DaoRoutine *ctx )
{
	DaoType *type = ctx->routHost;
	DaoClass *host = type != NULL && type->tid == DAO_OBJECT ? (DaoClass*) type->aux : NULL;
	DaoRoutine *rout = DaoClass_FindMethod( (DaoClass*) self->aux, "[]=", host );
	DaoType *args[ DAO_MAX_PARAM + 1 ];

	args[0] = value;
	memcpy( args + 1, index, N*sizeof(DaoType*) );

	if( rout != NULL ) rout = DaoRoutine_MatchByType( rout, self, args, N+1, DVM_CALL );
	if( rout == NULL ) return DAO_ERROR_INDEX;
	return DAO_OK;
}

static int DaoObject_DoSetItem( DaoValue *self, DaoValue *index[], int N, DaoValue *value, DaoProcess *proc )
{
	DaoObject *object = (DaoObject*) self;
	DaoClass *host = proc->activeObject ? proc->activeObject->defClass : NULL;
	DaoRoutine *rout = DaoClass_FindMethod( object->defClass, "[]=", host );
	DaoValue *args[DAO_MAX_PARAM+1];
	int rc;

	args[0] = value;
	memcpy( args+1, index, N*sizeof(DaoValue*) );
	return DaoProcess_PushCall( proc, rout, self, args, N+1 );
	//if( rc ) DaoProcess_RaiseError( proc, daoExceptionNames[rc], NULL );
}

DaoType* DaoObject_CheckUnary( DaoType *self, DaoVmCode *op, DaoRoutine *ctx )
{
	DaoType *type = ctx->routHost;
	DaoClass *host = type != NULL && type->tid == DAO_OBJECT ? (DaoClass*) type->aux : NULL;
	DaoRoutine *rout = NULL;

	switch( op->code ){
	case DVM_NOT   :
	case DVM_MINUS :
	case DVM_TILDE :
	case DVM_SIZE  : break;
	default: return NULL;
	}
	rout = DaoClass_FindMethod( (DaoClass*) self->aux, DaoVmCode_GetOperator( op->code ), host );
	if( rout == NULL ) return NULL;
	if( op->c == op->a ){
		rout = DaoRoutine_MatchByType( rout, self, & self, 1, DVM_CALL );
	}else{
		rout = DaoRoutine_MatchByType( rout, NULL, & self, 1, DVM_CALL );
	}
	if( rout == NULL ) return NULL;
	return (DaoType*) rout->routType->aux;
}

DaoValue* DaoObject_DoUnary( DaoValue *self, DaoVmCode *op, DaoProcess *proc )
{
	DaoClass *host = proc->activeObject ? proc->activeObject->defClass : NULL;
	DaoType *argtype = proc->activeTypes[op->a];;
	DaoRoutine *rout = NULL;
	int retc = 0;

	switch( op->code ){
	case DVM_NOT   :
	case DVM_MINUS :
	case DVM_TILDE :
	case DVM_SIZE  : break;
	default: return NULL;
	}
	rout = DaoClass_FindMethod( self->xObject.defClass, DaoVmCode_GetOperator( op->code ), host );
	if( rout == NULL ) return NULL;
	if( op->c == op->a ){
		retc = DaoProcess_PushCallWithTypes( proc, rout, self, & self, & argtype, 1 );
	}else{
		retc = DaoProcess_PushCallWithTypes( proc, rout, NULL, & self, & argtype, 1 );
	}
	// TODO: retc;
	return NULL;
}

DaoType* DaoObject_CheckBinary( DaoType *self, DaoVmCode *op, DaoType *args[2], DaoRoutine *ctx )
{
	DaoType *type = ctx->routHost;
	DaoClass *host = type != NULL && type->tid == DAO_OBJECT ? (DaoClass*) type->aux : NULL;
	DaoVmSpace *vms = ctx->nameSpace->vmSpace;
	DaoRoutine *rout = NULL;
	DaoType *selftype = NULL;

	switch( op->code ){
	case DVM_ADD : case DVM_SUB :
	case DVM_MUL : case DVM_DIV :
	case DVM_MOD : case DVM_POW :
	case DVM_BITAND : case DVM_BITOR  : case DVM_BITXOR :
	case DVM_BITLFT : case DVM_BITRIT :
	case DVM_AND : case DVM_OR :
	case DVM_LT  : case DVM_LE :
	case DVM_EQ  : case DVM_NE :
	case DVM_IN :
		break;
	default: return NULL;
	}

	if( op->c == op->a ){
		const char *name = DaoVmCode_GetCompoundOperator( op->code );
		rout = DaoClass_FindMethod( (DaoClass*) self->aux, name, host );
		if( rout != NULL ){
			rout = DaoRoutine_MatchByType( rout, self, args+1, 1, DVM_CALL );
			if( rout == NULL ) return NULL;
			return (DaoType*) rout->routType->aux;
		}
	}

	rout = DaoClass_FindMethod( (DaoClass*) self->aux, DaoVmCode_GetOperator( op->code ), host );
	if( rout == NULL ){
		if( op->code == DVM_EQ || op->code == DVM_NE ) return vms->typeBool;
		return NULL;
	}

	if( op->c == op->a && self == args[0] ) selftype = self;
	if( op->c == op->b && self == args[1] ) selftype = self;
	rout = DaoRoutine_MatchByType( rout, selftype, args, 2, DVM_CALL );
	if( rout == NULL ) return NULL;
	return (DaoType*) rout->routType->aux;
}

DaoValue* DaoObject_DoBinary( DaoValue *self, DaoVmCode *op, DaoValue *args[2], DaoProcess *proc )
{
	DaoClass *host = proc->activeObject ? proc->activeObject->defClass : NULL;
	DaoRoutine *rout = NULL;
	DaoValue *selfvalue = NULL;
	DaoType *argtypes[2];

	switch( op->code ){
	case DVM_ADD : case DVM_SUB :
	case DVM_MUL : case DVM_DIV :
	case DVM_MOD : case DVM_POW :
	case DVM_BITAND : case DVM_BITOR  : case DVM_BITXOR :
	case DVM_BITLFT : case DVM_BITRIT :
	case DVM_AND : case DVM_OR :
	case DVM_LT  : case DVM_LE :
	case DVM_EQ  : case DVM_NE :
	case DVM_IN :
		break;
	default: return NULL;
	}

	argtypes[0] = proc->activeTypes[ op->a ];
	argtypes[1] = proc->activeTypes[ op->b ];

	if( op->c == op->a ){
		const char *name = DaoVmCode_GetCompoundOperator( op->code );
		rout = DaoClass_FindMethod( self->xObject.defClass, name, host );
		if( rout != NULL ){
			DaoProcess_PushCallWithTypes( proc, rout, self, args+1, argtypes + 1, 1 );
			return NULL;
		}
	}

	rout = DaoClass_FindMethod( self->xObject.defClass, DaoVmCode_GetOperator( op->code ), host );
	if( rout == NULL ){
		switch( op->code ){
		case DVM_EQ : DaoProcess_PutBoolean( proc, args[0] == args[1] ); break;
		case DVM_NE : DaoProcess_PutBoolean( proc, args[0] != args[1] ); break;
		default: break;
		}
		return NULL;
	}

	if( op->c == op->a && self == args[0] ) selfvalue = self;
	if( op->c == op->b && self == args[1] ) selfvalue = self;
	DaoProcess_PushCallWithTypes( proc, rout, selfvalue, args, argtypes, 2 );
	// TODO: retc;
	return NULL;
}

static DaoType* DaoObject_CheckConversion( DaoType *self, DaoType *type, DaoRoutine *ctx )
{
	DString *buffer;
	DaoRoutine *rout;
	DaoType *hostype = ctx->routHost;
	DaoClass *host = hostype != NULL && hostype->tid == DAO_OBJECT ? (DaoClass*) hostype->aux : NULL;

	if( DaoType_ChildOf( self, type ) ) return type;
	if( DaoType_ChildOf( type, self ) ) return type;

	buffer = DString_NewChars( "(" );
	DString_Append( buffer, type->name );
	DString_AppendChars( buffer, ")" );
	rout = DaoClass_FindMethod( (DaoClass*) self->aux, buffer->chars, host );
	DString_Delete( buffer );
	if( rout != NULL ){
		DaoType *ttype = DaoNamespace_GetType( ctx->nameSpace, (DaoValue*) type );
		rout = DaoRoutine_MatchByType( rout, self, & ttype, 1, DVM_CALL );
	}
	if( rout != NULL ) return type;
	return NULL;
}

static DaoValue* DaoObject_DoConversion( DaoValue *self, DaoType *type, int copy, DaoProcess *proc )
{
	DaoObject *object = (DaoObject*) self;
	DaoClass *host = proc->activeObject ? proc->activeObject->defClass : NULL;
	DaoValue *base = DaoObject_CastToBase( object->rootObject, type );
	DaoRoutine *rout;
	DString *buffer;

	if( base ) return base;

	buffer = DString_NewChars( "(" );
	DString_Append( buffer, type->name );
	DString_AppendChars( buffer, ")" );
	rout = DaoClass_FindMethod( object->defClass, buffer->chars, host );
	DString_Delete( buffer );
	if( rout != NULL ){
		int rc = DaoProcess_PushCall( proc, rout, self, (DaoValue**) & type, 1 );
		if( rc ) return NULL;
	}
	return NULL;
}

DaoType* DaoObject_CheckForEach( DaoType *self, DaoRoutine *ctx )
{
	DaoType *hostype = ctx->routHost;
	DaoClass *host = hostype != NULL && hostype->tid == DAO_OBJECT ? (DaoClass*) hostype->aux : NULL;
	DaoRoutine *rout = DaoClass_FindMethod( (DaoClass*) self->aux, "for", host );
	if( rout != NULL ){
		DaoType *type, *itype;
		if( rout->routType->args->size != 2 ) return NULL;
		type = rout->routType->args->items.pType[1];
		if( type->tid == DAO_PAR_NAMED ) type = (DaoType*) type->aux;
		if( type->tid != DAO_TUPLE || type->args->size != 2 ) return NULL;
		itype = type->args->items.pType[0];
		if( itype->tid != DAO_BOOLEAN ) return NULL;
		return DaoNamespace_MakeIteratorType( ctx->nameSpace, type->args->items.pType[1] );
	}
	return NULL;
}

int DaoObject_DoForEach( DaoValue *self, DaoTuple *iterator, DaoProcess *proc )
{
	DaoObject *object = (DaoObject*) self;
	DaoClass *host = proc->activeObject ? proc->activeObject->defClass : NULL;
	DaoRoutine *rout = DaoClass_FindMethod( object->defClass, "for", host );
	if( rout != NULL ){
		return DaoProcess_PushCall( proc, rout, self, (DaoValue**) & iterator, 1 );
	}
	return DAO_ERROR;
}

void DaoObject_Print( DaoValue *self, DaoStream *stream, DMap *cycmap, DaoProcess *proc )
{
	int ec = 0;
	char buf[50];
	DMap *inmap = cycmap;
	DaoObject *object = (DaoObject*) self;
	DaoVmSpace *vms = proc->vmSpace;
	DaoValue *params[2];
	DaoRoutine *meth;

	sprintf( buf, "[%p]", object );
	if( self == object->defClass->objType->value ){
		DaoStream_WriteString( stream, object->defClass->className );
		DaoStream_WriteChars( stream, "[null]" );
		return;
	}
	if( cycmap != NULL && DMap_Find( cycmap, object ) != NULL ){
		DaoStream_WriteString( stream, object->defClass->className );
		DaoStream_WriteChars( stream, buf );
		return;
	}
	if( cycmap == NULL ) cycmap = DHash_New(0,0);
	DMap_Insert( cycmap, self, self );

	DaoValue_Clear( & proc->stackValues[0] );

	params[0] = (DaoValue*) vms->typeString;
	params[1] = (DaoValue*) stream;
	meth = DaoClass_FindMethod( object->defClass, "(string)", NULL );
	if( meth ){
		ec = DaoProcess_Call( proc, meth, self, params, 2 );
		if( ec ) ec = DaoProcess_Call( proc, meth, self, params, 1 );
	}else{
		meth = DaoClass_FindMethod( object->defClass, "serialize", NULL );
		if( meth ) ec = DaoProcess_Call( proc, meth, self, NULL, 0 );
	}
	if( ec ){
		DaoProcess_RaiseException( proc, daoExceptionNames[ec], proc->string->chars, NULL );
	}else if( meth && proc->stackValues[0] ){
		DaoValue_Print( proc->stackValues[0], stream, cycmap, proc );
	}else{
		DaoStream_WriteString( stream, object->defClass->className );
		DaoStream_WriteChars( stream, buf );
	}
	if( inmap == NULL ) DMap_Delete( cycmap );
}

void DaoObject_CoreDelete( DaoValue *self )
{
	DaoObject_Delete( (DaoObject*) self );
}

DaoTypeCore daoObjectCore =
{
	"object",                                            /* name */
	sizeof(DaoObject),                                   /* size */
	{ NULL },                                            /* bases */
	{ NULL },                                            /* casts */
	NULL,                                                /* numbers */
	NULL,                                                /* methods */
	DaoObject_CheckGetField,    DaoObject_DoGetField,    /* GetField */
	DaoObject_CheckSetField,    DaoObject_DoSetField,    /* SetField */
	DaoObject_CheckGetItem,     DaoObject_DoGetItem,     /* GetItem */
	DaoObject_CheckSetItem,     DaoObject_DoSetItem,     /* SetItem */
	DaoObject_CheckUnary,       DaoObject_DoUnary,       /* Unary */
	DaoObject_CheckBinary,      DaoObject_DoBinary,      /* Binary */
	DaoObject_CheckConversion,  DaoObject_DoConversion,  /* Conversion */
	DaoObject_CheckForEach,     DaoObject_DoForEach,     /* ForEach */
	DaoObject_Print,                                     /* Print */
	NULL,                                                /* Slice */
	NULL,                                                /* Compare */
	NULL,                                                /* Hash */
	NULL,                                                /* Create */
	NULL,                                                /* Copy */
	DaoObject_CoreDelete,                                /* Delete */
	NULL                                                 /* HandleGC */
};
