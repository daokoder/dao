/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2016, Limin Fu
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

#include"daoVmspace.h"
#include"daoNamespace.h"
#include"daoValue.h"
#include"daoInterface.h"
#include"daoGC.h"

extern int DaoType_Match( DaoType *self, DaoType *type, DMap *defs, DMap *binds, int dep );
void DaoMethods_Insert( DMap *methods, DaoRoutine *rout, DaoNamespace *ns, DaoType *host );


DaoInterface* DaoInterface_New( const char *name )
{
	DaoInterface *self = (DaoInterface*) dao_calloc( 1, sizeof(DaoInterface) );
	DaoValue_Init( self, DAO_INTERFACE );
	self->trait |= DAO_VALUE_DELAYGC;
	self->derived = 0;
	self->supers = DList_New( DAO_DATA_VALUE );
	self->methods = DHash_New( DAO_DATA_STRING, DAO_DATA_VALUE );
	self->abtype = DaoType_New( name, DAO_INTERFACE, (DaoValue*)self, NULL );
	GC_IncRC( self->abtype );
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogNew( (DaoValue*) self );
#endif
	return self;
}
void DaoInterface_Delete( DaoInterface *self )
{
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogDelete( (DaoValue*) self );
#endif
	if( self->concretes ) DMap_Delete( self->concretes );
	GC_DecRC( self->abtype );
	DList_Delete( self->supers );
	DMap_Delete( self->methods );
	dao_free( self );
}

void DaoInterface_DeriveMethods( DaoInterface *self )
{
	daoint i, k, m, N = self->supers->size;
	DaoInterface *super;
	DNode *it;
	for(i=0; i<N; i++){
		super = (DaoInterface*) self->supers->items.pValue[i];
		if( self->abtype->bases == NULL ) self->abtype->bases = DList_New( DAO_DATA_VALUE );
		DList_Append( self->abtype->bases, super->abtype );
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

static int DaoRoutine_IsCompatible( DaoRoutine *self, DaoType *type, DMap *binds )
{
	DaoRoutine *rout;
	daoint i, j, n, k=-1, max = 0;
	if( self->overloads == NULL ) return DaoType_Match( self->routType, type, NULL, binds, 0 );
	for(i=0,n=self->overloads->routines->size; i<n; i++){
		rout = self->overloads->routines->items.pRoutine[i];
		if( rout->routType == type ) return 1;
	}
	for(i=0,n=self->overloads->routines->size; i<n; i++){
		rout = self->overloads->routines->items.pRoutine[i];
		j = DaoType_Match( rout->routType, type, NULL, binds, 0 );
		/*
		   printf( "%3i %3i: %3i  %s  %s\n",n,i,j,rout->routType->name->chars,type->name->chars );
		 */
		if( j && j >= max ){
			max = j;
			k = i;
		}
	}
	return (k >= 0);
}
int DaoInterface_CheckBind( DList *methods, DaoType *type, DMap *binds )
{
	DNode *it;
	DaoRoutine *rout2;
	daoint i, n, id;
	if( type->tid == DAO_OBJECT || type->tid == DAO_CLASS ){
		DaoClass *klass = & type->aux->xClass;
		for(i=0,n=methods->size; i<n; i++){
			DaoRoutine *rout = methods->items.pRoutine[i];
			rout2 = klass->initRoutines;
			if( !(rout->attribs & DAO_ROUT_INITOR) ){
				id = DaoClass_FindConst( klass, rout->routName );
				if( id <0 ) return 0;
				rout2 = (DaoRoutine*) DaoClass_GetConst( klass, id );
				if( rout2->type != DAO_ROUTINE ) return 0;
			}
			/*printf( "AAA: %s %s\n", rout->routType->name->chars,rout2->routType->name->chars);*/
			if( DaoRoutine_IsCompatible( rout2, rout->routType, binds ) ==0 ) return 0;
		}
	}else if( type->tid == DAO_INTERFACE ){
		DaoInterface *inter = (DaoInterface*) type->aux;
		for(i=0,n=methods->size; i<n; i++){
			DaoRoutine *rout = methods->items.pRoutine[i];
			DString *name = rout->routName;
			if( rout->attribs & DAO_ROUT_INITOR ) name = inter->abtype->name;
			it = DMap_Find( inter->methods, name );
			if( it == NULL ) return 0;
			if( DaoRoutine_IsCompatible( it->value.pRoutine, rout->routType, binds ) ==0 ) return 0;
		}
	}else if( type->tid == DAO_CINVALUE ){
		DaoCinType *cintype = (DaoCinType*) type->aux;
		for(i=0,n=methods->size; i<n; i++){
			DaoRoutine *rout = methods->items.pRoutine[i];
			DString *name = rout->routName;
			if( rout->attribs & DAO_ROUT_INITOR ) name = cintype->vatype->name;
			it = DMap_Find( cintype->methods, name );
			if( it == NULL ) return 0;
			if( DaoRoutine_IsCompatible( it->value.pRoutine, rout->routType, binds ) ==0 ) return 0;
		}
	}else{
		for(i=0,n=methods->size; i<n; i++){
			DaoRoutine *rout = methods->items.pRoutine[i];
			DString *name = rout->routName;
			DaoRoutine *func;
			if( rout->attribs & DAO_ROUT_INITOR ) name = type->name;
			func = DaoType_FindFunction( type, name );
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
static void DMap_SortMethods( DMap *hash, DList *methods )
{
	DMap *map = DMap_New( DAO_DATA_STRING, 0 );
	DString *name = DString_New();
	DNode *it;
	daoint i, n;
	for(it=DMap_First(hash); it; it=DMap_Next(hash,it)){
		if( it->value.pRoutine->overloads ){
			DRoutines *one = it->value.pRoutine->overloads;
			for(i=0,n=one->routines->size; i<n; i++){
				DaoRoutine *rout = one->routines->items.pRoutine[i];
				DString_Assign( name, rout->routName );
				DString_AppendChars( name, " " );
				DString_Append( name, rout->routType->name );
				DMap_Insert( map, name, (void*)rout );
			}
		}else{
			DaoRoutine *rout = it->value.pRoutine;
			DString_Assign( name, rout->routName );
			DString_AppendChars( name, " " );
			DString_Append( name, rout->routType->name );
			DMap_Insert( map, name, (void*)rout );
		}
	}
	DList_Clear( methods );
	for(it=DMap_First(map); it; it=DMap_Next(map,it))
		DList_Append( methods, it->value.pVoid );
	DMap_Delete( map );
	DString_Delete( name );
}
int DaoInterface_BindTo( DaoInterface *self, DaoType *type, DMap *binds )
{
	DNode *it;
	DMap *newbinds = NULL;
	DList *methods;
	void *pvoid[2];
	daoint i, n, bl;

	/* XXX locking */
	if( type->interfaces == NULL ) type->interfaces = DHash_New( DAO_DATA_VALUE, 0 );

	pvoid[0] = type;
	pvoid[1] = self->abtype;
	if( (it = DMap_Find( type->interfaces, self )) ) return it->value.pVoid != NULL;
	if( binds && DMap_Find( binds, pvoid ) ) return 1;
	if( binds ==NULL ) newbinds = binds = DHash_New( DAO_DATA_VOID2, 0 );
	DaoInterface_TempBind( self, type, binds );
	methods = DList_New(0);
	DMap_SortMethods( self->methods, methods );
	bl = DaoInterface_CheckBind( methods, type, binds );
	DList_Delete( methods );
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
DaoCinType* DaoInterface_GetConcrete( DaoInterface *self, DaoType *type )
{
	DNode *it;
	
	if( self->concretes == NULL ) return NULL;

	it = DMap_Find( self->concretes, type );
	if( it == NULL ){
		for(it=DMap_First(self->concretes); it; it=DMap_Next(self->concretes,it)){
			if( DaoType_MatchTo( it->key.pType, type, NULL ) >= DAO_MT_EQ ){
				return (DaoCinType*) it->value.pVoid;
			}
		}
		return NULL;
	}
	return (DaoCinType*) it->value.pVoid;
}

int DaoType_MatchInterface( DaoType *self, DaoInterface *inter, DMap *binds )
{
	DMap *inters = self->interfaces;
	DNode *it;
	daoint i;
	if( inter == NULL ) return DAO_MT_NOT;
	if( inter->abtype->kernel->SetupMethods ){
		DaoTypeKernel *kernel = inter->abtype->kernel;
		kernel->SetupMethods( kernel->nspace, kernel->typer );
	}
	if( inters == NULL ) return DAO_MT_SUB * DaoInterface_BindTo( inter, self, binds );
	if( (it = DMap_Find( inters, inter )) ) return it->value.pVoid ? DAO_MT_SUB : DAO_MT_NOT;
	return DAO_MT_SUB * DaoInterface_BindTo( inter, self, binds );
}

DaoTypeCore interTyper =
{
	"interface", & baseCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoInterface_Delete, NULL
};




DaoCinType* DaoCinType_New( DaoInterface *inter, DaoType *target )
{
	DaoCinType *self = (DaoCinType*) dao_calloc( 1, sizeof(DaoCinType) );
	DaoValue_Init( self, DAO_CINTYPE );
	self->trait |= DAO_VALUE_DELAYGC;
	self->derived = 0;
	self->supers = DList_New( DAO_DATA_VALUE );
	self->methods = DHash_New( DAO_DATA_STRING, DAO_DATA_VALUE );
	self->citype = DaoType_New( "interface<", DAO_CINTYPE, (DaoValue*)self, NULL );
	self->vatype = DaoType_New( inter->abtype->name->chars, DAO_CINVALUE, (DaoValue*)self, NULL );
	self->abstract = inter;
	self->target = target;
	GC_IncRC( self->citype );
	GC_IncRC( self->vatype );
	GC_IncRC( self->abstract );
	GC_IncRC( self->target );

	self->citype->nested = DList_New( DAO_DATA_VALUE );
	self->vatype->nested = DList_New( DAO_DATA_VALUE );
	DList_Append( self->citype->nested, target );
	DList_Append( self->vatype->nested, target );

	DString_AppendChar( self->vatype->name, '<' );
	DString_Append( self->vatype->name, target->name );
	DString_AppendChar( self->vatype->name, '>' );

	DString_Append( self->citype->name, self->vatype->name );
	DString_AppendChar( self->citype->name, '>' );

#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogNew( (DaoValue*) self );
#endif
	return self;
}
void DaoCinType_Delete( DaoCinType *self )
{
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogDelete( (DaoValue*) self );
#endif
	GC_DecRC( self->abstract );
	GC_DecRC( self->target );
	GC_DecRC( self->citype );
	GC_DecRC( self->vatype );
	DList_Delete( self->supers );
	DMap_Delete( self->methods );
	dao_free( self );
}

void DaoCinType_DeriveMethods( DaoCinType *self )
{
	daoint i, k, m, N = self->supers->size;
	DaoCinType *super;
	DNode *it;
	for(i=0; i<N; i++){
		super = (DaoCinType*) self->supers->items.pValue[i];
		self->citype->bases = DList_New( DAO_DATA_VALUE );
		self->vatype->bases = DList_New( DAO_DATA_VALUE );
		DList_Append( self->citype->bases, super->citype );
		DList_Append( self->vatype->bases, super->vatype );
		for(it=DMap_First(super->methods); it; it=DMap_Next( super->methods, it )){
			if( it->value.pRoutine->overloads ){
				DRoutines *routs = it->value.pRoutine->overloads;
				for(k=0,m=routs->routines->size; k<m; k++){
					DaoRoutine *rout = routs->routines->items.pRoutine[i];
					DaoMethods_Insert( self->methods, rout, NULL, self->vatype );
				}
			}else{
				DaoMethods_Insert( self->methods, it->value.pRoutine, NULL, self->vatype );
			}
		}
	}
	self->derived = 1;
}



static DaoType* DaoCinType_CheckGetField( DaoType *self, DString *name, DaoRoutine *ctx )
{
	DaoRoutine *rout = DaoType_FindFunction( self, name );

	if( rout != NULL ) return rout->routType;
	return NULL;
}

static DaoValue* DaoCinType_DoGetField( DaoValue *selfv, DString *name, DaoProcess *proc )
{
	DaoType *type = self->xCinType.cintype->vatype;
	return DaoType_FindValue( type, name );
}

void DaoCinType_CoreDelete( DaoValue *self )
{
	DaoCinType_Delete( (DaoCinType*) self );
}

DaoTypeCore daoCinTypeCore =
{
	"CinType",                                         /* name */
	{ NULL },                                          /* bases */
	NULL,                                              /* numbers */
	NULL,                                              /* methods */
	DaoCinType_CheckGetField,  DaoCinType_DoGetField,  /* GetField */
	NULL,                      NULL,                   /* SetField */
	NULL,                      NULL,                   /* GetItem */
	NULL,                      NULL,                   /* SetItem */
	NULL,                      NULL,                   /* Unary */
	NULL,                      NULL,                   /* Binary */
	NULL,                      NULL,                   /* Comparison */
	NULL,                      NULL,                   /* Conversion */
	NULL,                      NULL,                   /* ForEach */
	DaoCinType_Print,                                  /* Print */
	NULL,                                              /* Slice */
	NULL,                                              /* Copy */
	DaoCinType_CoreDelete,                             /* Delete */
	NULL                                               /* HandleGC */
};




DaoCinValue* DaoCinValue_New( DaoCinType *cintype, DaoValue *value )
{
	DaoCinValue *self = (DaoCinValue*)dao_calloc( 1, sizeof(DaoCinValue) );
	DaoValue_Init( self, DAO_CINVALUE );
	GC_Assign( & self->cintype, cintype );
	DaoValue_Move( value, & self->value, cintype->target );
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogNew( (DaoValue*) self );
#endif
	return self;
}
DaoCinValue* DaoCinValue_Copy( DaoCinValue *self )
{
	return DaoCinValue_New( self->cintype, self->value );
}

void DaoCinValue_Delete( DaoCinValue *self )
{
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogDelete( (DaoValue*) self );
#endif
	GC_DecRC( self->cintype );
	GC_DecRC( self->value );
	dao_free( self );
}



static DaoType* DaoCinValue_CheckGetField( DaoType *self, DString *name, DaoRoutine *ctx )
{
	DaoRoutine *rout = DaoType_FindFunction( self, name );

	if( rout != NULL ) return rout->routType;

	DString_SetChars( ctx->buffer, "." );
	DString_Append( ctx->buffer, name );
	rout = DaoType_FindFunction( self, ctx->buffer );
	if( rout != NULL ) rout = DaoRoutine_MatchByType( rout, self, NULL, 0, DVM_CALL );
	if( rout == NULL ) return NULL;
	return (DaoType*) rout->routType->aux;
}

static DaoValue* DaoCinValue_DoGetField( DaoValue *selfv, DString *name, DaoProcess *proc )
{
	DaoType *type = self->xCinValue.cintype->vatype;
	DaoValue *value = DaoType_FindValue( type, name );
	DaoRoutine *func;

	if( value != NULL ) return value;

	DString_SetChars( proc->string, "." );
	DString_Append( proc->string, name );
	func = DaoType_FindFunction( type, proc->string );
	if( func == NULL ) return NULL;
	DaoProcess_PushCallable( proc, func, self, NULL, 0 );
	return NULL;
}

static int DaoCinValue_CheckSetField( DaoType *self, DString *name, DaoType *value, DaoRoutine *ctx )
{
	DaoRoutine *rout;
	DString *buffer = ctx->buffer;

	DString_SetChars( buffer, "." );
	DString_Append( buffer, name );
	DString_AppendChars( buffer, "=" );
	rout = DaoType_FindFunction( self, buffer );
	if( rout == NULL ) return DAO_ERROR_FIELD_ABSENT;
	rout = DaoRoutine_MatchByType( rout, self, & value, 1, DVM_CALL );
	if( rout == NULL ) return DAO_ERROR_VALUE;
	return DAO_OK;
}

static int DaoCinValue_DoSetField( DaoValue *selfv, DString *name, DaoValue *value, DaoProcess *proc )
{
	DaoType *type = self->xCinValue.cintype->vatype;
    DaoRoutine *func;

    DString_SetChars( proc->string, "." );
    DString_Append( proc->string, name );
    DString_AppendChars( proc->string, "=" );
    func = DaoType_FindFunction( type, proc->string );
    if( func == NULL ) return DAO_ERROR_FIELD_ABSENT;
    DaoProcess_PushCallable( proc, func, self, & value, 1 );
	return DAO_OK;
}

static DaoType* DaoCinValue_CheckGetItem( DaoType *self, DaoType *index[], int N, DaoRoutine *ctx )
{
	DaoRoutine *rout = DaoType_FindFunctionChars( self, "[]" );
	if( rout != NULL ) rout = DaoRoutine_MatchByType( rout, self, index, N, DVM_CALL );
	if( rout == NULL ) return DAO_ERROR_INDEX;
	return (DaoType*) rout->routType->aux;
}

static DaoValue* DaoCinValue_DoGetItem( DaoValue *selfv, DaoValue *index[], int N, DaoProcess *proc )
{
	DaoType *type = self->xCinValue.cintype->vatype;
	DaoRoutine *func = DaoType_FindFunctionChars( type, "[]" );
	if( func != NULL ) DaoProcess_PushCallable( proc, func, self, index, N );
	return NULL;
}

static int DaoCinValue_CheckSetItem( DaoType *self, DaoType *index[], int N, DaoType *value, DaoRoutine *ctx )
{
	DaoRoutine *rout = DaoType_FindFunctionChars( self, "[]=" );
	DaoType *args[ DAO_MAX_PARAM + 1 ];

	args[0] = value;
	memcpy( args + 1, index, N*sizeof(DaoType*) );

	if( rout != NULL ) rout = DaoRoutine_MatchByType( rout, self, args, N+1, DVM_CALL );
	if( rout == NULL ) return DAO_ERROR_INDEX;
	return DAO_OK;
}

static int DaoCinValue_DoSetItem( DaoValue *selfv, DaoValue *index[], int N, DaoValue *value, DaoProcess *proc )
{
	DaoType *type = self->xCinValue.cintype->vatype;
	DaoRoutine *rout = DaoType_FindFunctionChars( type, "[]=" );
	DaoValue *args[ DAO_MAX_PARAM ];
	if( rout == NULL ) return DAO_ERROR_INDEX;
	args[0] = value;
	memcpy( args+1, index, N*sizeof(DaoValue*) );
	DaoProcess_PushCallable( proc, rout, self, args, N+1 );
	return DAO_OK;
}

DaoType* DaoCinValue_CheckUnary( DaoType *self, DaoVmCode *op, DaoRoutine *ctx )
{
	DaoRoutine *rout = NULL;

	switch( op->code ){
	case DVM_NOT   :
	case DVM_MINUS :
	case DVM_TILDE :
	case DVM_SIZE  : break;
	default: return NULL;
	}
	rout = DaoType_FindOperator( self, op->code );
	if( rout == NULL ) return NULL;
	if( op->c == op->a ){
		rout = DaoRoutine_MatchByType( rout, self, & self, 1, DVM_CALL );
	}else{
		rout = DaoRoutine_MatchByType( rout, NULL, & self, 1, DVM_CALL );
	}
	if( rout == NULL ) return NULL;
	return (DaoType*) rout->routType->aux;
}

DaoValue* DaoCinValue_DoUnary( DaoValue *self, DaoVmCode *op, DaoProcess *proc )
{
	DaoType *type = self->xCinValue.cintype->vatype;
	DaoRoutine *rout = NULL;
	int retc = 0;

	switch( op->code ){
	case DVM_NOT   :
	case DVM_MINUS :
	case DVM_TILDE :
	case DVM_SIZE  : break;
	default: return NULL;
	}
	rout = DaoType_FindOperator( type, op->code );
	if( rout == NULL ) return NULL;
	if( op->c == op->a ){
		retc = DaoProcess_PushCallable( proc, rout, self, & self, 1 );
	}else{
		retc = DaoProcess_PushCallable( proc, rout, NULL, & self, 1 );
	}
	// TODO: retc;
	return NULL;
}

DaoType* DaoCinValue_CheckBinary( DaoType *self, DaoVmCode *op, DaoType *args[2], DaoRoutine *ctx )
{
	DaoRoutine *rout = NULL;
	DaoType *selftype = NULL;

	switch( op->code ){
	case DVM_ADD : case DVM_SUB :
	case DVM_MUL : case DVM_DIV :
	case DVM_MOD : case DVM_POW :
	case DVM_BITAND : case DVM_BITOR  :
	case DVM_AND : case DVM_OR :
	case DVM_LT  : case DVM_LE :
	case DVM_EQ  : case DVM_NE :
		break;
	default: return NULL;
	}
	rout = DaoType_FindOperator( self, op->code );
	if( rout == NULL ) return NULL;

	args[0] = left;
	args[1] = right;
	if( op->c == op->a && self == left  ) selftype = self;
	if( op->c == op->b && self == right ) selftype = self;
	rout = DaoRoutine_MatchByType( rout, selftype, args, 2, DVM_CALL );
	if( rout == NULL ) return NULL;
	return (DaoType*) rout->routType->aux;
}

DaoValue* DaoCinValue_DoBinary( DaoValue *self, DaoVmCode *op, DaoValue *args[2], DaoProcess *proc )
{
	DaoRoutine *rout = NULL;
	DaoValue *selfvalue = NULL;

	switch( op->code ){
	case DVM_ADD : case DVM_SUB :
	case DVM_MUL : case DVM_DIV :
	case DVM_MOD : case DVM_POW :
	case DVM_BITAND : case DVM_BITOR  :
	case DVM_AND : case DVM_OR :
	case DVM_LT  : case DVM_LE :
	case DVM_EQ  : case DVM_NE :
		break;
	default: return NULL;
	}
	rout = DaoType_FindOperator( self->xCinValue.cintype->vatype, op->code );
	if( rout == NULL ) return NULL;

	args[0] = left;
	args[1] = right;
	if( op->c == op->a && self == left  ) selfvalue = self;
	if( op->c == op->b && self == right ) selfvalue = self;
	retc = DaoProcess_PushCallable( proc, rout, selfvalue, args, 2 );
	// TODO: retc;
	return NULL;
}


void DaoCinValue_CoreDelete( DaoValue *self )
{
	DaoCinValue_Delete( (DaoCinValue*) self );
}

DaoTypeCore daoCinValueCore =
{
	"CinValue",                                          /* name */
	{ NULL },                                            /* bases */
	NULL,                                                /* numbers */
	NULL,                                                /* methods */
	DaoCinValue_CheckGetField,  DaoCinValue_DoGetField,  /* GetField */
	DaoCinValue_CheckSetField,  DaoCinValue_DoSetField,  /* SetField */
	DaoCinValue_CheckGetItem,   DaoCinValue_DoGetItem,   /* GetItem */
	DaoCinValue_CheckSetItem,   DaoCinValue_DoSetItem,   /* SetItem */
	DaoCinValue_CheckUnary,     DaoCinValue_DoUnary,     /* Unary */
	DaoCinValue_CheckBinary,    DaoCinValue_DoBinary,    /* Binary */
	NULL,                       NULL,                    /* Comparison */
	NULL,                       NULL,                    /* Conversion */
	NULL,                       NULL,                    /* ForEach */
	DaoCinValue_Print,                                   /* Print */
	NULL,                                                /* Slice */
	NULL,                                                /* Copy */
	DaoCinValue_CoreDelete,                              /* Delete */
	NULL                                                 /* HandleGC */
};
