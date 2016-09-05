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
	self->bases = DList_New( DAO_DATA_VALUE );
	self->methods = DHash_New( DAO_DATA_STRING, DAO_DATA_VALUE );
	self->abtype = DaoType_New( name, DAO_INTERFACE, (DaoValue*)self, NULL );
	self->abtype->kernel = DaoTypeKernel_New( NULL );
	self->abtype->kernel->abtype = self->abtype;
	GC_IncRC( self->abtype->kernel );
	GC_IncRC( self->abtype );
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
	DList_Delete( self->bases );
	DMap_Delete( self->methods );
	dao_free( self );
}

void DaoInterface_DeriveMethods( DaoInterface *self )
{
	daoint i, k, m, N = self->bases->size;
	DaoInterface *base;
	DNode *it;
	for(i=0; i<N; i++){
		base = (DaoInterface*) self->bases->items.pValue[i];
		if( self->abtype->bases == NULL ) self->abtype->bases = DList_New( DAO_DATA_VALUE );
		DList_Append( self->abtype->bases, base->abtype );
		for(it=DMap_First(base->methods); it; it=DMap_Next( base->methods, it )){
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
	daoint i, N = self->bases->size;
	void *pvoid[2];
	pvoid[0] = type;
	pvoid[1] = self->abtype;
	if( DMap_Find( binds, pvoid ) ) return;
	DMap_Insert( binds, pvoid, NULL );
	for(i=0; i<N; i++){
		DaoInterface *base = (DaoInterface*) self->bases->items.pValue[i];
		DaoInterface_TempBind( base, type, binds );
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
	for(i=0,n=self->bases->size; i<n; i++){
		DaoInterface *base = (DaoInterface*) self->bases->items.pValue[i];
		if( DMap_Find( type->interfaces, base ) ) continue;
		DMap_Insert( type->interfaces, base, base );
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
			if( DaoType_MatchTo( type, it->key.pType, NULL ) >= DAO_MT_CIV ){
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
		kernel->SetupMethods( kernel->nspace, inter->abtype->core );
	}
	if( inters == NULL ) return DAO_MT_SUB * DaoInterface_BindTo( inter, self, binds );
	if( (it = DMap_Find( inters, inter )) ) return it->value.pVoid ? DAO_MT_SUB : DAO_MT_NOT;
	return DAO_MT_SUB * DaoInterface_BindTo( inter, self, binds );
}



static DaoType* DaoInterface_CheckGetField( DaoType *self, DaoString *name, DaoRoutine *ctx )
{
	return DaoType_CheckGetField( self, name );
}

static int DaoInterface_CheckSetField( DaoType *self, DaoString *name, DaoType *value, DaoRoutine *ctx )
{
	return DaoType_CheckSetField( self, name, value );
}

static DaoType* DaoInterface_CheckGetItem( DaoType *self, DaoType *index[], int N, DaoRoutine *ctx )
{
	DaoRoutine *rout = DaoType_FindFunctionChars( self, "[]" );
	if( rout != NULL ) rout = DaoRoutine_MatchByType( rout, self, index, N, DVM_CALL );
	if( rout == NULL ) return NULL;
	return (DaoType*) rout->routType->aux;
}

static int DaoInterface_CheckSetItem( DaoType *self, DaoType *index[], int N, DaoType *value, DaoRoutine *ctx )
{
	DaoRoutine *rout = DaoType_FindFunctionChars( self, "[]=" );
	DaoType *args[ DAO_MAX_PARAM + 1 ];

	args[0] = value;
	memcpy( args + 1, index, N*sizeof(DaoType*) );

	if( rout != NULL ) rout = DaoRoutine_MatchByType( rout, self, args, N+1, DVM_CALL );
	if( rout == NULL ) return DAO_ERROR_INDEX;
	return DAO_OK;
}

DaoType* DaoInterface_CheckUnary( DaoType *self, DaoVmCode *op, DaoRoutine *ctx )
{
	DaoRoutine *rout = NULL;

	switch( op->code ){
	case DVM_NOT   :
	case DVM_MINUS :
	case DVM_TILDE :
	case DVM_SIZE  : break;
	default: return NULL;
	}
	rout = DaoType_FindFunctionChars( self, DaoVmCode_GetOperator( op->code ) );
	if( rout == NULL ) return NULL;
	if( op->c == op->a ){
		rout = DaoRoutine_MatchByType( rout, self, & self, 1, DVM_CALL );
	}else{
		rout = DaoRoutine_MatchByType( rout, NULL, & self, 1, DVM_CALL );
	}
	if( rout == NULL ) return NULL;
	return (DaoType*) rout->routType->aux;
}

DaoType* DaoInterface_CheckBinary( DaoType *self, DaoVmCode *op, DaoType *args[2], DaoRoutine *ctx )
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

	if( op->c == op->a ){
		rout = DaoType_FindFunctionChars( self, DaoVmCode_GetCompoundOperator( op->code ) );
		if( rout != NULL ){
			rout = DaoRoutine_MatchByType( rout, self, args+1, 1, DVM_CALL );
			if( rout == NULL ) return NULL;
			return (DaoType*) rout->routType->aux;
		}
	}

	rout = DaoType_FindFunctionChars( self, DaoVmCode_GetOperator( op->code ) );
	if( rout == NULL ) return NULL;

	if( op->c == op->a && self == args[0] ) selftype = self;
	if( op->c == op->b && self == args[1] ) selftype = self;
	rout = DaoRoutine_MatchByType( rout, selftype, args, 2, DVM_CALL );
	if( rout == NULL ) return NULL;
	return (DaoType*) rout->routType->aux;
}

DaoType* DaoInterface_CheckConversion( DaoType *self, DaoType *type, DaoRoutine *ctx )
{
	DaoRoutine *rout;
	DString *buffer = DString_NewChars( "(" );

	DString_Append( buffer, type->name );
	DString_AppendChars( buffer, ")" );
	rout = DaoType_FindFunction( self, buffer );
	DString_Delete( buffer );
	if( rout != NULL ){
		DaoType *ttype = DaoNamespace_GetType( ctx->nameSpace, (DaoValue*) type );
		rout = DaoRoutine_MatchByType( rout, self, & ttype, 1, DVM_CALL );
		if( rout ) return type;
	}
	return NULL;
}

DaoType* DaoInterface_CheckForEach( DaoType *self, DaoRoutine *ctx )
{
	DaoRoutine *rout = DaoType_FindFunctionChars( self, "for" );
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

void DaoInterface_CoreDelete( DaoValue *self )
{
	DaoInterface_Delete( (DaoInterface*) self );
}

DaoTypeCore daoInterfaceCore =
{
	"interface",                          /* name */
	sizeof(DaoInterface),                 /* size */
	{ NULL },                             /* bases */
	NULL,                                 /* numbers */
	NULL,                                 /* methods */
	DaoInterface_CheckGetField,    NULL,  /* GetField */
	DaoInterface_CheckSetField,    NULL,  /* SetField */
	DaoInterface_CheckGetItem,     NULL,  /* GetItem */
	DaoInterface_CheckSetItem,     NULL,  /* SetItem */
	DaoInterface_CheckUnary,       NULL,  /* Unary */
	DaoInterface_CheckBinary,      NULL,  /* Binary */
	DaoInterface_CheckConversion,  NULL,  /* Conversion */
	DaoInterface_CheckForEach,     NULL,  /* ForEach */
	NULL,                                 /* Print */
	NULL,                                 /* Slice */
	NULL,                                 /* Compare */
	NULL,                                 /* Hash */
	NULL,                                 /* Create */
	NULL,                                 /* Copy */
	DaoInterface_CoreDelete,              /* Delete */
	NULL                                  /* HandleGC */
};






extern DaoTypeCore daoCinTypeCore;
extern DaoTypeCore daoCinValueCore;

DaoCinType* DaoCinType_New( DaoInterface *inter, DaoType *target )
{
	DaoCinType *self = (DaoCinType*) dao_calloc( 1, sizeof(DaoCinType) );
	DaoValue_Init( self, DAO_CINTYPE );
	self->trait |= DAO_VALUE_DELAYGC;
	self->derived = 0;
	self->bases = DList_New( DAO_DATA_VALUE );
	self->methods = DHash_New( DAO_DATA_STRING, DAO_DATA_VALUE );
	self->citype = DaoType_New( "interface<", DAO_CINTYPE, (DaoValue*)self, NULL );
	self->vatype = DaoType_New( inter->abtype->name->chars, DAO_CINVALUE, (DaoValue*)self, NULL );
	self->abstract = inter;
	self->target = target;
	self->citype->core = & daoCinTypeCore;
	self->vatype->core = & daoCinValueCore;
	GC_IncRC( self->citype );
	GC_IncRC( self->vatype );
	GC_IncRC( self->abstract );
	GC_IncRC( self->target );

	self->citype->args = DList_New( DAO_DATA_VALUE );
	self->vatype->args = DList_New( DAO_DATA_VALUE );
	DList_Append( self->citype->args, target );
	DList_Append( self->vatype->args, target );

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
	DList_Delete( self->bases );
	DMap_Delete( self->methods );
	dao_free( self );
}

void DaoCinType_DeriveMethods( DaoCinType *self )
{
	daoint i, k, m, N = self->bases->size;
	DaoCinType *base;
	DNode *it;
	for(i=0; i<N; i++){
		base = (DaoCinType*) self->bases->items.pValue[i];
		self->citype->bases = DList_New( DAO_DATA_VALUE );
		self->vatype->bases = DList_New( DAO_DATA_VALUE );
		DList_Append( self->citype->bases, base->citype );
		DList_Append( self->vatype->bases, base->vatype );
		for(it=DMap_First(base->methods); it; it=DMap_Next( base->methods, it )){
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



static DaoType* DaoCinType_CheckGetField( DaoType *self, DaoString *name, DaoRoutine *ctx )
{
	DaoRoutine *rout = DaoType_FindFunction( self, name->value );

	if( rout != NULL ) return rout->routType;
	return NULL;
}

static DaoValue* DaoCinType_DoGetField( DaoValue *self, DaoString *name, DaoProcess *proc )
{
	DaoType *type = self->xCinType.vatype;
	return DaoType_FindValue( type, name->value );
}

void DaoCinType_CoreDelete( DaoValue *self )
{
	DaoCinType_Delete( (DaoCinType*) self );
}

DaoTypeCore daoCinTypeCore =
{
	"CinType",                                         /* name */
	sizeof(DaoCinType),                                /* size */
	{ NULL },                                          /* bases */
	NULL,                                              /* numbers */
	NULL,                                              /* methods */
	DaoCinType_CheckGetField,  DaoCinType_DoGetField,  /* GetField */
	NULL,                      NULL,                   /* SetField */
	NULL,                      NULL,                   /* GetItem */
	NULL,                      NULL,                   /* SetItem */
	NULL,                      NULL,                   /* Unary */
	NULL,                      NULL,                   /* Binary */
	NULL,                      NULL,                   /* Conversion */
	NULL,                      NULL,                   /* ForEach */
	NULL,                                              /* Print */
	NULL,                                              /* Slice */
	NULL,                                              /* Compare */
	NULL,                                              /* Hash */
	NULL,                                              /* Create */
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



static DaoType* DaoCinValue_CheckGetField( DaoType *self, DaoString *name, DaoRoutine *ctx )
{
	return DaoType_CheckGetField( self, name );
}

static DaoValue* DaoCinValue_DoGetField( DaoValue *self, DaoString *name, DaoProcess *proc )
{
	return DaoType_DoGetField( self->xCinValue.cintype->vatype, self, name, proc );
}

static int DaoCinValue_CheckSetField( DaoType *self, DaoString *name, DaoType *value, DaoRoutine *ctx )
{
	return DaoType_CheckSetField( self, name, value );
}

static int DaoCinValue_DoSetField( DaoValue *self, DaoString *name, DaoValue *value, DaoProcess *proc )
{
	return DaoType_DoSetField( self->xCinValue.cintype->vatype, self, name, value, proc );
}

static DaoType* DaoCinValue_CheckGetItem( DaoType *self, DaoType *index[], int N, DaoRoutine *ctx )
{
	DaoRoutine *rout = DaoType_FindFunctionChars( self, "[]" );
	if( rout != NULL ) rout = DaoRoutine_MatchByType( rout, self, index, N, DVM_CALL );
	if( rout == NULL ) return NULL;
	return (DaoType*) rout->routType->aux;
}

static DaoValue* DaoCinValue_DoGetItem( DaoValue *self, DaoValue *index[], int N, DaoProcess *proc )
{
	DaoType *type = self->xCinValue.cintype->vatype;
	DaoRoutine *rout = DaoType_FindFunctionChars( type, "[]" );
	if( rout != NULL ) DaoProcess_PushCallable( proc, rout, self, index, N );
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

static int DaoCinValue_DoSetItem( DaoValue *self, DaoValue *index[], int N, DaoValue *value, DaoProcess *proc )
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
	rout = DaoType_FindFunctionChars( self, DaoVmCode_GetOperator( op->code ) );
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
	rout = DaoType_FindFunctionChars( type, DaoVmCode_GetOperator( op->code ) );
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
	case DVM_IN :
		break;
	default: return NULL;
	}

	if( op->c == op->a ){
		rout = DaoType_FindFunctionChars( self, DaoVmCode_GetCompoundOperator( op->code ) );
		if( rout != NULL ){
			rout = DaoRoutine_MatchByType( rout, self, args+1, 1, DVM_CALL );
			if( rout == NULL ) return NULL;
			return (DaoType*) rout->routType->aux;
		}
	}

	rout = DaoType_FindFunctionChars( self, DaoVmCode_GetOperator( op->code ) );
	if( rout == NULL ) return NULL;

	if( op->c == op->a && self == args[0]  ) selftype = self;
	if( op->c == op->b && self == args[1] ) selftype = self;
	rout = DaoRoutine_MatchByType( rout, selftype, args, 2, DVM_CALL );
	if( rout == NULL ) return NULL;
	return (DaoType*) rout->routType->aux;
}

DaoValue* DaoCinValue_DoBinary( DaoValue *self, DaoVmCode *op, DaoValue *args[2], DaoProcess *proc )
{
	DaoRoutine *rout = NULL;
	DaoValue *selfvalue = NULL;
	DaoType *type;

	switch( op->code ){
	case DVM_ADD : case DVM_SUB :
	case DVM_MUL : case DVM_DIV :
	case DVM_MOD : case DVM_POW :
	case DVM_BITAND : case DVM_BITOR  :
	case DVM_AND : case DVM_OR :
	case DVM_LT  : case DVM_LE :
	case DVM_EQ  : case DVM_NE :
	case DVM_IN :
		break;
	default: return NULL;
	}
	type = self->xCinValue.cintype->vatype;

	if( op->c == op->a ){
		rout = DaoType_FindFunctionChars( type, DaoVmCode_GetCompoundOperator( op->code ) );
		if( rout != NULL ){
			DaoProcess_PushCallable( proc, rout, self, args+1, 1 );
			return NULL;
		}
	}

	rout = DaoType_FindFunctionChars( type, DaoVmCode_GetOperator( op->code ) );
	if( rout == NULL ) return NULL;

	if( op->c == op->a && self == args[0] ) selfvalue = self;
	if( op->c == op->b && self == args[1] ) selfvalue = self;
	DaoProcess_PushCallable( proc, rout, selfvalue, args, 2 );
	// TODO: retc;
	return NULL;
}

DaoType* DaoCinValue_CheckConversion( DaoType *self, DaoType *type, DaoRoutine *ctx )
{
	DaoCinType *cintype = (DaoCinType*) self->aux;
	DaoTypeCore *core;
	DaoRoutine *rout;
	DString *buffer;

	if( cintype->target == type ){
		return type;
	}else if( DaoType_MatchTo( cintype->target, type, NULL ) >= DAO_MT_EQ ){
		return type;
	}

	buffer = DString_NewChars( "(" );
	DString_Append( buffer, type->name );
	DString_AppendChars( buffer, ")" );
	rout = DaoType_FindFunction( cintype->vatype, buffer );
	DString_Delete( buffer );
	if( rout != NULL ){
		DaoType *ttype = DaoNamespace_GetType( ctx->nameSpace, (DaoValue*) type );
		rout = DaoRoutine_MatchByType( rout, self, & ttype, 1, DVM_CALL );
		if( rout ) return type;
	}
	core = cintype->target->core;
	if( core != NULL && core->CheckConversion ){
		return core->CheckConversion( cintype->target, type, ctx );
	}
	return NULL;
}

DaoValue* DaoCinValue_DoConversion( DaoValue *self, DaoType *type, int copy, DaoProcess *proc )
{
	DaoCinType *cintype = self->xCinValue.cintype;
	DaoTypeCore *core;
	DaoRoutine *rout;
	DString *buffer;

	if( cintype->target == type ){
		if( copy ) return DaoValue_Convert( self->xCinValue.value, type, copy, proc );
		return self->xCinValue.value;
	}else if( DaoType_MatchTo( cintype->target, type, NULL ) >= DAO_MT_EQ ){
		if( copy ) return DaoValue_Convert( self->xCinValue.value, type, copy, proc );
		return self->xCinValue.value;
	}

	buffer = DString_NewChars( "(" );
	DString_Append( buffer, type->name );
	DString_AppendChars( buffer, ")" );
	rout = DaoType_FindFunction( cintype->vatype, buffer );
	DString_Delete( buffer );
	if( rout != NULL ){
		int rc = DaoProcess_PushCallable( proc, rout, self, (DaoValue**) & type, 1 );
		if( rc == 0 ) return NULL;
	}
	return DaoValue_Convert( self->xCinValue.value, type, copy, proc );
}

DaoType* DaoCinValue_CheckForEach( DaoType *self, DaoRoutine *ctx )
{
	DaoCinType *cintype = (DaoCinType*) self->aux;
	DaoRoutine *rout = DaoType_FindFunctionChars( cintype->vatype, "for" );
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

void DaoCinValue_DoForEach( DaoValue *self, DaoTuple *iterator, DaoProcess *proc )
{
	DaoCinType *cintype = self->xCinValue.cintype;
	DaoRoutine *rout = DaoType_FindFunctionChars( cintype->vatype, "for" );
	if( rout != NULL ){
		DaoProcess_PushCallable( proc, rout, self, (DaoValue**) & iterator, 1 );
	}
}

static void DaoCinValue_Print( DaoValue *self, DaoStream *stream, DMap *cycmap, DaoProcess *proc )
{
	int ec = 0;
	char buf[50];
	DaoRoutine *meth;
	DaoValue *args[2];
	DaoType *type = self->xCinValue.cintype->vatype;
	DMap *inmap = cycmap;

	if( cycmap != NULL && DMap_Find( cycmap, self ) != NULL ){
		sprintf( buf, "[%p]", self );
		DaoStream_WriteString( stream, type->name );
		DaoStream_WriteChars( stream, buf );
		return;
	}

	if( cycmap == NULL ) cycmap = DHash_New(0,0);
	DMap_Insert( cycmap, self, self );

	args[0] = (DaoValue*) dao_type_string;
	args[1] = (DaoValue*) stream;
	meth = DaoType_FindFunctionChars( type, "(string)" );
	if( meth ){
		ec = DaoProcess_Call( proc, meth, self, args, 2 );
		if( ec ) ec = DaoProcess_Call( proc, meth, self, args, 1 );
	}else{
		meth = DaoType_FindFunctionChars( type, "serialize" );
		if( meth ) ec = DaoProcess_Call( proc, meth, self, NULL, 0 );
	}
	if( meth == NULL ){
		DaoValue_Print( self->xCinValue.value, stream, cycmap, proc );
	}else if( ec ){
		DaoProcess_RaiseException( proc, daoExceptionNames[ec], proc->string->chars, NULL );
	}else if( meth && proc->stackValues[0] ){
		DaoValue_Print( proc->stackValues[0], stream, cycmap, proc );
	}else{
		DaoStream_WriteString( stream, type->name );
		DaoStream_WriteChars( stream, buf );
	}
	if( inmap == NULL ) DMap_Delete( cycmap );
}

void DaoCinValue_CoreDelete( DaoValue *self )
{
	DaoCinValue_Delete( (DaoCinValue*) self );
}

DaoTypeCore daoCinValueCore =
{
	"CinValue",                                              /* name */
	sizeof(DaoCinValue),                                     /* size */
	{ NULL },                                                /* bases */
	NULL,                                                    /* numbers */
	NULL,                                                    /* methods */
	DaoCinValue_CheckGetField,    DaoCinValue_DoGetField,    /* GetField */
	DaoCinValue_CheckSetField,    DaoCinValue_DoSetField,    /* SetField */
	DaoCinValue_CheckGetItem,     DaoCinValue_DoGetItem,     /* GetItem */
	DaoCinValue_CheckSetItem,     DaoCinValue_DoSetItem,     /* SetItem */
	DaoCinValue_CheckUnary,       DaoCinValue_DoUnary,       /* Unary */
	DaoCinValue_CheckBinary,      DaoCinValue_DoBinary,      /* Binary */
	DaoCinValue_CheckConversion,  DaoCinValue_DoConversion,  /* Conversion */
	DaoCinValue_CheckForEach,     DaoCinValue_DoForEach,     /* ForEach */
	DaoCinValue_Print,                                       /* Print */
	NULL,                                                    /* Slice */
	NULL,                                                    /* Compare */
	NULL,                                                    /* Hash */
	NULL,                                                    /* Create */
	NULL,                                                    /* Copy */
	DaoCinValue_CoreDelete,                                  /* Delete */
	NULL                                                     /* HandleGC */
};
