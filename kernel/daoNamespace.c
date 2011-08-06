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

#include"stdlib.h"
#include"stdio.h"
#include"string.h"
#include"ctype.h"
#include"assert.h"

#include"daoType.h"
#include"daoVmspace.h"
#include"daoNamespace.h"
#include"daoNumtype.h"
#include"daoStream.h"
#include"daoRoutine.h"
#include"daoObject.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoStdlib.h"
#include"daoClass.h"
#include"daoParser.h"
#include"daoMacro.h"
#include"daoRegex.h"

#ifdef DAO_WITH_THREAD
DMutex dao_vsetup_mutex;
DMutex dao_msetup_mutex;
#endif

extern void DaoTypeBase_Free( DaoTypeBase *typer );

DaoTypeBase cmodTyper=
{
	"cmodule", & baseCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoCModule_Delete, NULL
};

DaoCModule* DaoCModule_New()
{
	DaoCModule *self = (DaoCModule*) dao_malloc( sizeof(DaoCModule) );
	DaoBase_Init( self, DAO_CMODULE );
	self->ctypers  = DArray_New(0);
	self->cmethods = DArray_New(0);
	self->libHandle = NULL;
	return self;
}
void DaoCModule_Delete( DaoCModule *self )
{
	int i;
	for(i=0; i<self->ctypers->size; i++){
		DaoTypeBase *typer = (DaoTypeBase*)self->ctypers->items.pBase[i];
		DaoTypeBase_Free( typer );
		if( typer->priv ) dao_free( typer->priv );
		typer->priv = NULL;
	}
	DArray_Delete( self->ctypers );
	DArray_Delete( self->cmethods );
	/* XXX if( self->libHandle ) DaoCloseLibrary( self->libHandle ); */
	dao_free( self );
}

static void DNS_GetField( DValue *self0, DaoContext *ctx, DString *name )
{
	DaoNameSpace *self = self0->v.ns;
	DNode *node = NULL;
	int st, pm, up, id;
	node = MAP_Find( self->lookupTable, name );
	if( node == NULL ) goto FieldNotExist;
	st = LOOKUP_ST( node->value.pSize );
	pm = LOOKUP_PM( node->value.pSize );
	up = LOOKUP_UP( node->value.pSize );
	id = LOOKUP_ID( node->value.pSize );
	if( pm == DAO_DATA_PRIVATE && self != ctx->nameSpace ) goto FieldNoPermit;
	if( st == DAO_GLOBAL_CONSTANT ){
		if( up >= self->cstDataTable->size ) goto InvalidField;
		DaoContext_PutValue( ctx, self->cstDataTable->items.pVarray[up]->data[id] );
	}else{
		if( up >= self->varDataTable->size ) goto InvalidField;
		DaoContext_PutValue( ctx, self->varDataTable->items.pVarray[up]->data[id] );
	}
	return;
FieldNotExist:
	DaoContext_RaiseException( ctx, DAO_ERROR_FIELD_NOTEXIST, name->mbs );
	return;
FieldNoPermit:
	DaoContext_RaiseException( ctx, DAO_ERROR_FIELD_NOTPERMIT, name->mbs );
	return;
InvalidField:
	DaoContext_RaiseException( ctx, DAO_ERROR_FIELD, name->mbs );
}
static void DNS_SetField( DValue *self0, DaoContext *ctx, DString *name, DValue value )
{
	DaoNameSpace *self = self0->v.ns;
	DaoType *type;
	DValue *dest;
	DNode *node = NULL;
	int st, pm, up, id;
	node = MAP_Find( self->lookupTable, name );
	if( node == NULL ) goto FieldNotExist;
	st = LOOKUP_ST( node->value.pSize );
	pm = LOOKUP_PM( node->value.pSize );
	up = LOOKUP_UP( node->value.pSize );
	id = LOOKUP_ID( node->value.pSize );
	if( pm == DAO_DATA_PRIVATE && self != ctx->nameSpace ) goto FieldNoPermit;
	if( st == DAO_GLOBAL_CONSTANT ) goto FieldNoPermit;
	if( up >= self->varDataTable->size ) goto InvalidField;
	type = self->varDataTable->items.pArray[up]->items.pType[id];
	dest = self->varDataTable->items.pVarray[up]->data + id;
	if( DValue_Move( value, dest, type ) ==0 ) goto TypeNotMatching;
	return;
FieldNotExist:
	DaoContext_RaiseException( ctx, DAO_ERROR_FIELD_NOTEXIST, name->mbs );
	return;
FieldNoPermit:
	DaoContext_RaiseException( ctx, DAO_ERROR_FIELD_NOTPERMIT, name->mbs );
	return;
TypeNotMatching:
	DaoContext_RaiseException( ctx, DAO_ERROR_TYPE, "not matching" );
	return;
InvalidField:
	DaoContext_RaiseException( ctx, DAO_ERROR_FIELD, name->mbs );
}
static void DNS_GetItem( DValue *self0, DaoContext *ctx, DValue *ids[], int N )
{
}
static void DNS_SetItem( DValue *self0, DaoContext *ctx, DValue *ids[], int N, DValue value )
{
}
static DaoTypeCore nsCore =
{
	0, NULL, NULL, NULL, NULL,
	DNS_GetField,
	DNS_SetField,
	DNS_GetItem,
	DNS_SetItem,
	DaoBase_Print,
	DaoBase_Copy, /* do not copy namespace */
};

DaoNameSpace* DaoNameSpace_GetNameSpace( DaoNameSpace *self, const char *name )
{
	DValue value = { DAO_NAMESPACE, 0, 0, 0, {0} };
	DaoNameSpace *ns;
	DString *mbs = DString_New(1);
	DString_SetMBS( mbs, name );
	ns = DaoNameSpace_FindNameSpace( self, mbs );
	if( ns == NULL ){
		ns = DaoNameSpace_New( self->vmSpace, name );
		DArray_Append( ns->parents, self );
		value.v.ns = ns;
		DaoNameSpace_AddConst( self, mbs, value, DAO_DATA_PUBLIC );
		value.v.ns = self;
		DVarray_Append( ns->cstData, value ); /* for GC */
		DValue_MarkConst( & ns->cstData->data[ns->cstData->size-1] );
	}
	DString_Delete( mbs );
	return ns;
}
void DaoNameSpace_AddValue( DaoNameSpace *self, const char *s, DValue v, const char *t )
{
	DaoType *abtp = NULL;
	DString *name = DString_New(1);
	if( t && strlen( t ) >0 ){
		DaoParser *parser = DaoParser_New();
		parser->nameSpace = self;
		parser->vmSpace = self->vmSpace;
		abtp = DaoParser_ParseTypeName( t, self, NULL ); /* XXX warn */
		DaoParser_Delete( parser );
	}
	/*
	   printf( "%s %s\n", name, abtp->name->mbs );
	 */
	DString_SetMBS( name, s );
	DaoNameSpace_AddVariable( self, name, v, abtp, DAO_DATA_PUBLIC );
	DString_Delete( name );
}
void DaoNameSpace_AddData( DaoNameSpace *self, const char *s, DaoBase *d, const char *t )
{
	DValue v = daoNullValue;
	if( d ){
		v.t = d->type;
		v.v.p = d;
	}
	DaoNameSpace_AddValue( self, s, v, t );
}
DValue DaoNameSpace_FindData( DaoNameSpace *self, const char *name )
{
	DString *s = DString_New(1);
	DValue res;
	DString_SetMBS( s, name );
	res = DaoNameSpace_GetData( self, s );
	DString_Delete( s );
	return res;
}
void DaoNameSpace_AddConstNumbers( DaoNameSpace *self0, DaoNumItem *items )
{
	DaoNameSpace *self = (DaoNameSpace*)self0;
	DString *s = DString_New(1);
	DValue value = daoNullValue;
	int i = 0;
	while( items[i].name != NULL ){
		value.t = items[i].type;
		if( value.t < DAO_INTEGER || value.t > DAO_DOUBLE ) continue;
		switch( value.t ){
		case DAO_INTEGER : value.v.i = (int) items[i].value; break;
		case DAO_FLOAT   : value.v.f = (float) items[i].value; break;
		case DAO_DOUBLE  : value.v.d = items[i].value; break;
		default: break;
		}
		DString_SetMBS( s, items[i].name );
		DaoNameSpace_AddConst( self, s, value, DAO_DATA_PUBLIC );
		i ++;
	}
	DString_Delete( s );
}
void DaoNameSpace_AddConstValue( DaoNameSpace *self, const char *name, DValue value )
{
	DString *s = DString_New(1);
	DString_SetMBS( s, name );
	DaoNameSpace_AddConst( self, s, value, DAO_DATA_PUBLIC );
	DString_Delete( s );
}
void DaoNameSpace_AddConstData( DaoNameSpace *self, const char *name, DaoBase *data )
{
	DString *s = DString_New(1);
	DValue value = { 0, 0, 0, 0, {0} };
	value.t = data->type;
	value.v.p = data;
	DString_SetMBS( s, name );
	DaoNameSpace_AddConst( self, s, value, DAO_DATA_PUBLIC );
	DString_Delete( s );
}
static void DaoTypeBase_Parents( DaoTypeBase *typer, DArray *parents )
{
	int i, k;
	DArray_Clear( parents );
	DArray_Append( parents, typer );
	for(k=0; k<parents->size; k++){
		typer = (DaoTypeBase*) parents->items.pVoid[k];
		for(i=0; i<DAO_MAX_CDATA_SUPER; i++){
			if( typer->supers[i] == NULL ) break;
			DArray_Append( parents, typer->supers[i] );
		}
	}
}
int DaoNameSpace_SetupValues( DaoNameSpace *self, DaoTypeBase *typer )
{
	int i, j, valCount;
	DArray *parents;
	DMap *values;
	DMap *supValues;
	DNode *it;
	DValue value = daoNullValue;

	if( typer->priv == NULL ) return 0;
	if( typer->priv->values != NULL ) return 1;
	for(i=0; i<DAO_MAX_CDATA_SUPER; i++){
		if( typer->supers[i] == NULL ) break;
		DaoNameSpace_SetupValues( self, typer->supers[i] );
	}
	valCount = 0;
	if( typer->numItems != NULL ){
		while( typer->numItems[ valCount ].name != NULL ) valCount ++;
	}

#ifdef DAO_WITH_THREAD
	DMutex_Lock( & dao_vsetup_mutex );
#endif
	if( typer->priv->values == NULL ){
		DaoType *abtype = typer->priv->abtype;
		DString defname = DString_WrapMBS( "default" );
		values = DHash_New( D_STRING, D_VALUE );
		typer->priv->values = values;
		if( abtype && abtype->value.t ) DMap_Insert( values, & defname, & abtype->value );
		for(i=0; i<valCount; i++){
			DString name = DString_WrapMBS( typer->numItems[i].name );
			double dv = typer->numItems[i].value;
			value = daoNullValue;
			value.t = typer->numItems[i].type;
			switch( typer->numItems[i].type ){
			case DAO_INTEGER : value.v.i = (int) dv; break;
			case DAO_FLOAT : value.v.f = (float) dv; break;
			case DAO_DOUBLE : value.v.d = dv; break;
			default : break;
			}
			DMap_Insert( values, & name, & value );
		}
		parents = DArray_New(0);
		DaoTypeBase_Parents( typer, parents );
		for(i=1; i<parents->size; i++){
			DaoTypeBase *sup = (DaoTypeBase*) parents->items.pVoid[i];
			supValues = sup->priv->values;
			if( sup->numItems == NULL ) continue;
			for(j=0; sup->numItems[j].name!=NULL; j++){
				DString name = DString_WrapMBS( sup->numItems[j].name );
				it = DMap_Find( supValues, & name );
				if( it && DMap_Find( values, & name ) == NULL )
					DMap_Insert( values, it->key.pVoid, it->value.pVoid );
			}
		}
		DArray_Delete( parents );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & dao_vsetup_mutex );
#endif
	return 1;
}
void DaoMethods_Insert( DMap *methods, DRoutine *rout, DaoType *host )
{
	DNode *node = MAP_Find( methods, rout->routName );
	if( node == NULL ){
		GC_IncRC( rout );
		DMap_Insert( methods, rout->routName, rout );
	}else if( node->value.pBase->type == DAO_FUNCTREE ){
		DaoFunctree_Add( (DaoFunctree*) node->value.pBase, rout );
	}else{
		DRoutine *existed = (DRoutine*) node->value.pBase;
		DaoFunctree *mroutine = DaoFunctree_New( existed->nameSpace, rout->routName );
		GC_IncRC( host );
		mroutine->host = host;
		DaoFunctree_Add( mroutine, (DRoutine*) node->value.pBase );
		DaoFunctree_Add( mroutine, (DRoutine*) rout );
		GC_ShiftRC( mroutine, node->value.pBase );
		node->value.pVoid = mroutine;
	}
}
int DaoNameSpace_SetupMethods( DaoNameSpace *self, DaoTypeBase *typer )
{
	DaoParser *parser, *defparser;
	DaoFunction *cur;
	DString *name1, *name2;
	DArray *parents;
	DMap *methods;
	DMap *supMethods;
	DNode *it;
	int i, k, size;
	if( typer->priv == NULL ){
		printf( "WARNING: type \"%s\" has no core structure.\n", typer->name );
		return 0;
	}
	if( typer->funcItems == NULL ) return 0;
	if( typer->priv->methods != NULL ) return 1;
	for(i=0; i<DAO_MAX_CDATA_SUPER; i++){
		if( typer->supers[i] == NULL ) break;
		DaoNameSpace_SetupMethods( self, typer->supers[i] );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & dao_msetup_mutex );
#endif
	if( typer->priv->methods == NULL ){
		DaoType *hostype = typer->priv->abtype;
		methods = DHash_New( D_STRING, 0 );
		typer->priv->methods = methods;
		size = 0;
		name1 = DString_New(1);
		name2 = DString_New(1);
		parser = DaoParser_New();
		parser->vmSpace = self->vmSpace;
		parser->nameSpace = self;
		parser->hostCData = hostype;
		parser->defParser = defparser = DaoParser_New();
		defparser->vmSpace = self->vmSpace;
		defparser->nameSpace = self;
		defparser->hostCData = hostype;
		defparser->routine = self->routEvalConst;

		if( typer->funcItems != NULL ){
			while( typer->funcItems[ size ].proto != NULL ) size ++;
		}

		for( i=0; i<size; i++ ){
			const char *proto = typer->funcItems[i].proto;
			cur = DaoNameSpace_ParsePrototype( self, proto, parser );
			if( cur == NULL ){
				printf( "  In function: %s::%s\n", typer->name, proto );
				continue;
			}
			cur->pFunc = typer->funcItems[i].fpter;
			if( self->vmSpace->safeTag ) cur->attribs |= DAO_ROUT_EXTFUNC;

			DaoMethods_Insert( methods, (DRoutine*)cur, hostype );
		}
		parents = DArray_New(0);
		DaoTypeBase_Parents( typer, parents );
		for(i=1; i<parents->size; i++){
			DaoTypeBase *sup = (DaoTypeBase*) parents->items.pVoid[i];
			supMethods = sup->priv->methods;
			for(it=DMap_First(supMethods); it; it=DMap_Next(supMethods, it)){
				if( it->value.pBase->type == DAO_FUNCTREE ){
					DaoFunctree *meta = (DaoFunctree*) it->value.pVoid;
					/* skip constructor */
					if( STRCMP( meta->name, sup->name ) ==0 ) continue;
					for(k=0; k<meta->routines->size; k++){
						DRoutine *rout = meta->routines->items.pRout2[k];
						/* skip methods not defined in this parent type */
						if( rout->routHost != sup->priv->abtype ) continue;
						DaoMethods_Insert( methods, rout, hostype );
					}
				}else{
					DRoutine *rout = (DRoutine*) it->value.pVoid;
					/* skip constructor */
					if( STRCMP( rout->routName, sup->name ) ==0 ) continue;
					/* skip methods not defined in this parent type */
					if( rout->routHost != sup->priv->abtype ) continue;
					DaoMethods_Insert( methods, rout, hostype );
				}
			}
		}
		DArray_Delete( parents );
		DaoParser_Delete( parser );
		DaoParser_Delete( defparser );

		assert( DAO_ROUT_MAIN < (1<<DVM_MOVE) );
		for(i=DVM_MOVE; i<=DVM_BITRIT; i++){
			DString_SetMBS( name1, daoBitBoolArithOpers[i-DVM_MOVE] );
			if( DMap_Find( methods, name1 ) == NULL ) continue;
			typer->priv->attribs |= DAO_OPER_OVERLOADED | (DVM_MOVE<<(i-DVM_MOVE+1));
		}
		DString_Delete( name1 );
		DString_Delete( name2 );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & dao_msetup_mutex );
#endif
	return 1;
}
enum { DAO_DT_FAILED, DAO_DT_SCOPED, DAO_DT_UNSCOPED };

void DaoParser_Error( DaoParser *self, int code, DString *ext );
void DaoParser_Error2( DaoParser *self, int code, int m, int n, int single_line );
void DaoParser_PrintError( DaoParser *self, int line, int code, DString *ext );
int DaoParser_FindPairToken( DaoParser *self,  uchar_t lw, uchar_t rw, int start, int stop );
int DaoParser_ParseScopedName( DaoParser *self, DValue *scope, DValue *value, int start, int local );
DaoType* DaoParser_ParseType( DaoParser *self, int start, int end, int *newpos, DArray *types );
DaoType* DaoParser_ParseTypeItems( DaoParser *self, int start, int end, DArray *types );

int DaoNameSpace_DefineType( DaoNameSpace *self, const char *name, DaoType *type )
{
	DaoToken **tokens;
	DaoCDataCore *hostCore;
	DaoParser *parser = DaoParser_New();
	DValue scope = daoNullValue, value = daoNullValue;
	int i, k, n, ret = DAO_DT_UNSCOPED;

	parser->vmSpace = self->vmSpace;
	parser->nameSpace = self;
	parser->routine = self->routEvalConst;
	if( ! DaoToken_Tokenize( parser->tokens, name, 0, 0, 0 ) ) goto Error;
	if( parser->tokens->size == 0 ) goto Error;
	tokens = parser->tokens->items.pToken;
	n = parser->tokens->size - 1;
	if( (k = DaoParser_ParseScopedName( parser, & scope, & value, 0, 0 )) <0 ) goto Error;
	if( k == 0 && n ==0 ) goto Finalize; /* single identifier name; */
	if( k == n ){
		DaoTypeCore *core;
		DString *name = tokens[k]->string;
		DValue vtype = daoNullType;
		if( value.t ){
			DaoParser_Error2( parser, DAO_SYMBOL_WAS_DEFINED, k, k, 0 );
			goto Error;
		}
		DString_Assign( type->name, name );
		switch( scope.t ){
		case DAO_CTYPE :
			DaoNameSpace_SetupValues( self, scope.v.cdata->typer );
			core = scope.v.cdata->typer->priv;
			if( core->values == NULL ) core->values = DHash_New( D_STRING, D_VALUE );
			vtype.v.type = type;
			DMap_Insert( core->values, name, & vtype );
			break;
		case DAO_CLASS :
			DaoClass_AddType( scope.v.klass, name, type );
			break;
		case DAO_NAMESPACE :
			DaoNameSpace_AddType( scope.v.ns, name, type );
			DaoNameSpace_AddTypeConstant( scope.v.ns, name, type );
			break;
		default : DaoParser_Error2( parser, DAO_UNDEFINED_SCOPE_NAME, k-2, k-2, 0 ); goto Error;
		}
		return DAO_DT_SCOPED;
	}
	ret = k ? DAO_DT_SCOPED : DAO_DT_UNSCOPED;
	if( value.t != DAO_CTYPE || tokens[k+1]->type != DTOK_LT ) goto Error;
	if( DaoParser_FindPairToken( parser, DTOK_LT, DTOK_GT, k+1, -1 ) != n ) goto Error;
	type->nested = DArray_New(0);
	DaoParser_ParseTypeItems( parser, k+2, n-1, type->nested );
	GC_IncRCs( type->nested );
	if( parser->errors->size ) goto Error;
	hostCore = (DaoCDataCore*) value.v.cdata->typer->priv;
	if( hostCore->instanceCData == NULL ) hostCore->instanceCData = DMap_New(D_ARRAY,0);
	DMap_Insert( hostCore->instanceCData, type->nested, type->aux.v.cdata );
	DString_Clear( type->name );
	while( k < parser->tokens->size ) DString_Append( type->name, tokens[k++]->string );
Finalize:
	DaoParser_Delete( parser );
	return ret;
Error:
	DaoParser_Error2( parser, DAO_INVALID_TYPE_FORM, 0, parser->tokens->size-1, 0 );
	DaoParser_PrintError( parser, 0, 0, NULL );
	DaoParser_Delete( parser );
	return DAO_DT_FAILED;
}
DaoType* DaoNameSpace_TypeDefine( DaoNameSpace *self, const char *old, const char *type )
{
	DaoNameSpace *ns;
	DaoType *tp, *tp2;
	DString name = DString_WrapMBS( old );
	DString alias = DString_WrapMBS( type );
	DNode *node;
	int i;
	/* printf( "DaoNameSpace_TypeDefine: %s %s\n", old, type ); */
	tp = DaoNameSpace_FindType( self, & name );
	if( tp == NULL ) tp = DaoParser_ParseTypeName( old, self, NULL );
	tp2 = DaoNameSpace_FindType( self, & alias );
	if( tp2 == NULL ) tp2 = DaoParser_ParseTypeName( type, self, NULL );
	if( tp == tp2 ) return tp;
	/* printf( "ns = %p  tp = %p  name = %s\n", self, tp, type ); */
	/* tp2 = DaoNameSpace_FindType( self, & name ); */
	/* Only allow overiding types defined in parent namespaces: */
	node = MAP_Find( self->abstypes, & alias );
	if( tp == NULL || node != NULL ) return NULL;
	tp = DaoType_Copy( tp );
	DString_SetMBS( tp->name, type );
	i = DaoNameSpace_DefineType( self, type, tp );
	if( i == DAO_DT_UNSCOPED ){
		DaoNameSpace_AddType( self, tp->name, tp );
		DaoNameSpace_AddTypeConstant( self, tp->name, tp );
	}
	return i != DAO_DT_FAILED ? tp : NULL;
}
DaoCDataCore* DaoCDataCore_New();
DaoType* DaoCData_WrapType( DaoNameSpace *ns, DaoTypeBase *typer );
void DaoTypeCData_SetMethods( DaoTypeBase *self );

static DaoType* DaoNameSpace_WrapType2( DaoNameSpace *self, DaoTypeBase *typer, DaoParser *parser )
{
	DaoParser *parser2 = parser;
	DaoType *ctype_type, *cdata_type;
	DaoCDataCore *hostCore;
	int ret;

	if( typer->priv ) return typer->priv->abtype;

	ctype_type = DaoCData_WrapType( self, typer );
	cdata_type = typer->priv->abtype;
	typer->priv->attribs |= DAO_TYPER_PRIV_FREE;
	ret = DaoNameSpace_DefineType( self, typer->name, ctype_type );
	DString_Assign( cdata_type->name, ctype_type->name );
	if( ret == DAO_DT_FAILED ){
		printf( "type wrapping failed: %s\n", typer->name );
		return NULL;
	}
	if( ret == DAO_DT_UNSCOPED ){
		DaoNameSpace_AddConst( self, ctype_type->name, ctype_type->aux, DAO_DATA_PUBLIC );
		DaoNameSpace_AddType( self, cdata_type->name, cdata_type );
	}
	//printf( "type wrapping: %s\n", typer->name );
	return cdata_type;
}
DaoType* DaoNameSpace_WrapType( DaoNameSpace *self, DaoTypeBase *typer )
{
	if( typer->priv == NULL ) DArray_Append( self->cmodule->ctypers, typer );
	return DaoNameSpace_WrapType2( self, typer, NULL );
}
DaoType* DaoNameSpace_SetupType( DaoNameSpace *self, DaoTypeBase *typer )
{
	DMap *methods;
	DNode *it;
	if( typer->priv && typer->priv->abtype ) return typer->priv->abtype;
	if( DaoNameSpace_SetupValues( self, typer ) == 0 ) return NULL;
	if( DaoNameSpace_SetupMethods( self, typer ) == 0 ) return NULL;
	methods = typer->priv->methods;
	for(it=DMap_First(methods); it; it=DMap_Next(methods, it)){
		GC_IncRC( it->value.pBase );
		DArray_Append( self->cmodule->cmethods, it->value.pVoid );
	}
	return typer->priv->abtype;
}
int DaoNameSpace_WrapTypes( DaoNameSpace *self, DaoTypeBase *typers[] )
{
	DaoParser *parser = DaoParser_New();
	int i, ec = 0;
	for(i=0; typers[i]; i++ ){
		if( typers[i]->priv == NULL ) DArray_Append( self->cmodule->ctypers, typers[i] );
		ec += DaoNameSpace_WrapType2( self, typers[i], parser ) == NULL;
		/* e |= ( DaoNameSpace_SetupValues( self, typers[i] ) == 0 ); */
	}
	/* if( setup ) return DaoNameSpace_SetupTypes( self, typers ); */
	DaoParser_Delete( parser );
	return ec;
}
int DaoNameSpace_TypeDefines( DaoNameSpace *self, const char *alias[] )
{
	int i = 0, ec = 0;
	if( alias == NULL ) return 0;
	while( alias[i] && alias[i+1] ){
		ec += DaoNameSpace_TypeDefine( self, alias[i], alias[i+1] ) == NULL;
		i += 2;
	}
	return ec;
}
int DaoNameSpace_SetupTypes( DaoNameSpace *self, DaoTypeBase *typers[] )
{
	DMap *methods;
	DNode *it;
	int i, ec = 0;
	for(i=0; typers[i]; i++ ){
		ec += ( DaoNameSpace_SetupMethods( self, typers[i] ) == 0 );
		methods = typers[i]->priv->methods;
		for(it=DMap_First(methods); it; it=DMap_Next(methods, it)){
			GC_IncRC( it->value.pBase );
			DArray_Append( self->cmodule->cmethods, it->value.pVoid );
		}
	}
	return ec;
}
DaoFunction* DaoNameSpace_MakeFunction( DaoNameSpace *self, 
		const char *proto, DaoParser *parser )
{
	DaoParser *old = parser;
	DaoParser *defparser = NULL;
	DaoFunction *func;
	DValue val = daoNullValue;

	if( parser == NULL ){
		parser = DaoParser_New();
		parser->vmSpace = self->vmSpace;
		parser->nameSpace = self;
		parser->defParser = defparser = DaoParser_New();
		defparser->vmSpace = self->vmSpace;
		defparser->nameSpace = self;
		defparser->routine = self->routEvalConst;
	}
	func = DaoNameSpace_ParsePrototype( self, proto, parser );
	if( old == NULL ){
		DaoParser_Delete( parser );
		DaoParser_Delete( defparser );
	}
	if( func == NULL ) return NULL;
	val = DaoNameSpace_GetData( self, func->routName );
	if( val.t == DAO_FUNCTREE ){
		DaoFunctree_Add( val.v.mroutine, (DRoutine*)func );
	}else{
		val = daoNullFunction;
		val.v.func = func;
		DaoNameSpace_AddConst( self, func->routName, val, DAO_DATA_PUBLIC );
	}
	return func;
}
DaoFunction* DaoNameSpace_WrapFunction( DaoNameSpace *self, DaoFuncPtr fptr, const char *proto )
{
	DaoFunction *func = DaoNameSpace_MakeFunction( self, proto, NULL );
	if( func == NULL ) return NULL;
	func->pFunc = fptr;
	return func;
}

int DaoNameSpace_WrapFunctions( DaoNameSpace *self, DaoFuncItem *items )
{
	DaoParser *defparser, *parser = DaoParser_New();
	DaoFunction *func;
	int i = 0;
	int ec = 0;
	parser->vmSpace = self->vmSpace;
	parser->nameSpace = self;
	parser->defParser = defparser = DaoParser_New();
	defparser->vmSpace = self->vmSpace;
	defparser->nameSpace = self;
	defparser->routine = self->routEvalConst;
	while( items[i].fpter != NULL ){
		func = DaoNameSpace_MakeFunction( self, items[i].proto, parser );
		if( func ) func->pFunc = (DaoFuncPtr)items[i].fpter;
		ec += func == NULL;
		i ++;
	}
	DaoParser_Delete( parser );
	DaoParser_Delete( defparser );
	return ec;
}
int DaoNameSpace_Load( DaoNameSpace *self, const char *fname )
{
	DaoVmSpace *vms = self->vmSpace;
	DString *src;
	FILE *fin = fopen( fname, "r" );
	char buf[IO_BUF_SIZE];
	int ch;
	if( ! fin ){
		DaoStream_WriteMBS( vms->stdStream, "ERROR: can not open file \"" );
		DaoStream_WriteMBS( vms->stdStream, fname );
		DaoStream_WriteMBS( vms->stdStream, "\".\n" );
		return 0;
	}
	src = DString_New(1);
	while(1){
		size_t count = fread( buf, 1, IO_BUF_SIZE, fin );
		if( count ==0 ) break;
		DString_AppendDataMBS( src, buf, count );
	}
	fclose( fin );
	ch = DaoVmProcess_Eval( self->vmSpace->mainProcess, self, src, 1 );
	DString_Delete( src );
	return ch;
}
void DaoNameSpace_SetOptions( DaoNameSpace *self, int options )
{
	self->options = options;
}
int DaoNameSpace_GetOptions( DaoNameSpace *self )
{
	return self->options;
}

DaoTypeBase nsTyper=
{
	"namespace", & nsCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoNameSpace_Delete, NULL
};

DaoNameSpace* DaoNameSpace_New( DaoVmSpace *vms, const char *nsname )
{
	DValue value = daoNullValue;
	DString *name = DString_New(1);
	DaoNameSpace *self = (DaoNameSpace*) dao_malloc( sizeof(DaoNameSpace) );
	DaoBase_Init( self, DAO_NAMESPACE );
	self->vmSpace = vms;
	self->cstUser = 0;
	self->options = 0;
	self->mainRoutine = NULL;
	self->parents = DArray_New(0);
	self->cstData  = DVarray_New();
	self->varData  = DVarray_New();
	self->varType  = DArray_New(0);
	self->nsTable = DArray_New(0);
	self->lookupTable = DHash_New(D_STRING,0);
	self->cstDataTable = DArray_New(0);
	self->varDataTable = DArray_New(0);
	self->varTypeTable = DArray_New(0);
	self->mainRoutines  = DArray_New(0);
	self->definedRoutines  = DArray_New(0);
	self->nsLoaded  = DArray_New(0);
	self->cmodule = DaoCModule_New();
	self->localMacros   = DHash_New(D_STRING,0);
	self->globalMacros   = DHash_New(D_STRING,0);
	self->abstypes = DHash_New(D_STRING,0);
	self->argParams = DaoList_New();
	self->time = 0;
	self->file = DString_New(1);
	self->path = DString_New(1);
	self->name = DString_New(1);
	self->inputs = DString_New(1);
	self->sources = DArray_New(D_ARRAY);
	self->tokens = DHash_New(D_STRING,0);
	self->udfType1 = DaoType_New( "?", DAO_UDF, 0,0 );
	self->udfType2 = DaoType_New( "?", DAO_UDF, 0,0 );
	GC_IncRC( self->udfType1 );
	GC_IncRC( self->udfType2 );
	GC_IncRC( self->cmodule );

	DArray_Append( self->cstDataTable, self->cstData );
	DArray_Append( self->varDataTable, self->varData );
	DArray_Append( self->varTypeTable, self->varType );
	DArray_Append( self->nsTable, self );

	value.t = DAO_NAMESPACE;
	value.v.ns = self;
	DaoNameSpace_SetName( self, nsname );
	DaoNameSpace_AddConst( self, self->name, value, DAO_DATA_PUBLIC );

	value = daoNullValue;
	value.cst = 1;
	DString_SetMBS( name, "null" ); 
	DaoNameSpace_AddConst( self, name, value, DAO_DATA_PUBLIC );
	DVarray_Append( self->cstData, value ); /* reserved for main */

	value.t = DAO_STREAM;
	value.v.stream = vms->stdStream;
	DString_SetMBS( name, "io" ); 
	DaoNameSpace_AddConst( self, name, value, DAO_DATA_PUBLIC );
	if( vms->thdMaster ){
		value.t = DAO_THDMASTER;
		value.v.p = (DaoBase*) vms->thdMaster;
		DString_SetMBS( name, "mtlib" ); 
		DaoNameSpace_AddConst( self, name, value, DAO_DATA_PUBLIC );
	}

	DString_SetMBS( name, "exceptions" );
	value.t = DAO_LIST;
	value.cst = 0;
	value.v.list = DaoList_New();
	DaoNameSpace_AddVariable( self, name, value, NULL, DAO_DATA_PUBLIC );

	self->tempTypes = NULL;
	self->tempModes = DString_New(1);
	self->vmpEvalConst = DaoVmProcess_New(vms);
	self->routEvalConst = DaoRoutine_New();
	self->routEvalConst->routType = dao_routine;
	self->routEvalConst->nameSpace = self;
	GC_IncRC( dao_routine );
	GC_IncRC( self );
	DaoVmProcess_PushRoutine( self->vmpEvalConst, self->routEvalConst );
	self->vmpEvalConst->topFrame->context->trait |= DAO_DATA_CONST;
	value.cst = 1;
	value.t = DAO_ROUTINE;
	value.v.routine = self->routEvalConst;
	DVarray_Append( self->cstData, value );
	value.t = DAO_VMPROCESS;
	value.v.vmp = self->vmpEvalConst;
	DVarray_Append( self->cstData, value );
	DString_Delete( name );
	self->cstUser = self->cstData->size;

	if( vms && vms->nsInternal ){
		value.t = DAO_NAMESPACE;
		value.v.ns = vms->nsInternal;
		DaoNameSpace_AddConst( self, vms->nsInternal->name, value, DAO_DATA_PUBLIC );
		DArray_Append( self->parents, vms->nsInternal );
	}
	return self;
}
void DaoNameSpace_Delete( DaoNameSpace *self )
{
	/* printf( "DaoNameSpace_Delete  %s\n", self->name->mbs ); */
	DNode *it;
	int i, j;
	it = DMap_First( self->localMacros );
	for( ; it !=NULL; it = DMap_Next(self->localMacros, it) ) GC_DecRC( it->value.pBase );
	it = DMap_First( self->globalMacros );
	for( ; it !=NULL; it = DMap_Next(self->globalMacros, it) ) GC_DecRC( it->value.pBase );
	it = DMap_First( self->abstypes );
	for( ; it !=NULL; it = DMap_Next(self->abstypes, it) ) GC_DecRC( it->value.pBase );
	if( self->cmodule ){
		for(i=0; i<self->cmodule->ctypers->size; i++){
			DaoTypeBase *typer = (DaoTypeBase*)self->cmodule->ctypers->items.pBase[i];
			if( typer->priv ) typer->priv->nspace = NULL;
		}
		GC_DecRC( self->cmodule );
	}
	for(i=0; i<self->sources->size; i++){
		DArray *array = self->sources->items.pArray[i];
		for(j=0; j<array->size; j++) array->items.pToken[j]->string = NULL;
	}
	DaoList_Delete( self->argParams );

	if( self->tempTypes ) dao_free( self->tempTypes );
	DString_Delete( self->tempModes );
	GC_DecRC( self->udfType1 );
	GC_DecRC( self->udfType2 );
	GC_DecRCs( self->varType );
	DMap_Delete( self->lookupTable );
	DVarray_Delete( self->cstData );
	DVarray_Delete( self->varData );
	DArray_Delete( self->varType );
	DArray_Delete( self->parents );
	DArray_Delete( self->cstDataTable );
	DArray_Delete( self->varDataTable );
	DArray_Delete( self->varTypeTable );
	DArray_Delete( self->nsTable );
	/* no need for GC, because these namespaces are indirectly
	 * referenced through functions. */
	DArray_Delete( self->nsLoaded );
	DArray_Delete( self->mainRoutines );
	DArray_Delete( self->definedRoutines );
	DMap_Delete( self->localMacros );
	DMap_Delete( self->globalMacros );
	DMap_Delete( self->abstypes );
	DString_Delete( self->file );
	DString_Delete( self->path );
	DString_Delete( self->name );
	DString_Delete( self->inputs );
	DArray_Delete( self->sources );
	DMap_Delete( self->tokens );
	dao_free( self );
}
void DaoNameSpace_SetName( DaoNameSpace *self, const char *name )
{
	int i;
	DString_SetMBS( self->name, name );
	i = DString_RFindChar( self->name, '/', -1 );
	if( i != MAXSIZE ){
		DString_SetMBS( self->file, name + i + 1 );
		DString_SetDataMBS( self->path, name, i );
	}else{
		DString_Clear( self->file );
		DString_Clear( self->path );
	}
}
static int DaoNameSpace_GetUpIndex( DaoNameSpace *self, DaoNameSpace *ns );
static int DaoNameSpace_FindParentData( DaoNameSpace *self, DString *name, int storage )
{
	int i, st, pm, up, id;
	for(i=0; i<self->parents->size; i++){
		DaoNameSpace *parent = self->parents->items.pNS[i];
		DNode *node = MAP_Find( parent->lookupTable, name );
		if( node == NULL ) continue;
		pm = LOOKUP_PM( node->value.pSize );
		if( pm != DAO_DATA_PUBLIC ) continue;
		st = LOOKUP_ST( node->value.pSize );
		if( st != storage ) continue;
		up = LOOKUP_UP( node->value.pSize );
		id = LOOKUP_ID( node->value.pSize );
		up = DaoNameSpace_GetUpIndex( self, parent->nsTable->items.pNS[up] );
		node = MAP_Insert( self->lookupTable, name, LOOKUP_BIND( st, pm, up, id ) );
		return node->value.pSize;
	}
	return -1;
}
int DaoNameSpace_FindConst( DaoNameSpace *self, DString *name )
{
	DNode *node = DMap_Find( self->lookupTable, name );
	if( node == NULL ) return DaoNameSpace_FindParentData( self, name, DAO_GLOBAL_CONSTANT );
	if( LOOKUP_ST( node->value.pSize ) != DAO_GLOBAL_CONSTANT ) return -1;
	return node->value.pSize;
}
int DaoNameSpace_AddConst( DaoNameSpace *self, DString *name, DValue value, int pm )
{
	DaoFunctree *mroutine;
	DNode *node = MAP_Find( self->lookupTable, name );
	DValue *dest;
	int sto, up, id = 0;
	int isrout = value.t >= DAO_FUNCTREE && value.t <= DAO_FUNCTION;
	int isrout2;

	if( node && LOOKUP_UP( node->value.pSize ) ){ /* override */
		sto = LOOKUP_ST( node->value.pSize );
		up = LOOKUP_UP( node->value.pSize );
		id = LOOKUP_ID( node->value.pSize );
		assert( up < self->cstDataTable->size );
		assert( id < self->cstDataTable->items.pVarray[up]->size );
		dest = self->cstDataTable->items.pVarray[up]->data + id;
		isrout2 = dest->t >= DAO_FUNCTREE && dest->t <= DAO_FUNCTION;

		DMap_EraseNode( self->lookupTable, node );
		if( sto == DAO_GLOBAL_CONSTANT && isrout && isrout2 ){
			/* to allow function overloading */
			DaoNameSpace_AddConst( self, name, *dest, pm );
		}
		DaoNameSpace_AddConst( self, name, value, pm );
		node = MAP_Find( self->lookupTable, name );
		return node->value.pSize;
	}else if( node ){
		id = LOOKUP_ID( node->value.pSize );
		if( LOOKUP_ST( node->value.pSize ) != DAO_GLOBAL_CONSTANT ) return -1;
		assert( id < self->cstData->size );
		dest = self->cstData->data + id;
		isrout2 = dest->t >= DAO_FUNCTREE && dest->t <= DAO_FUNCTION;
		/* No overriding, only function overloading: */
		if( isrout == 0 || isrout2 == 0 ) return -1;
		if( dest->t == DAO_ROUTINE || dest->t == DAO_FUNCTION ){
			/* Add individual entry for the existing function: */
			DVarray_Append( self->cstData, *dest );
			DValue_MarkConst( self->cstData->data + self->cstData->size -1 );
			dest = self->cstData->data + id;

			mroutine = DaoFunctree_New( self, name );
			DaoFunctree_Add( mroutine, (DRoutine*) dest->v.p );
			GC_ShiftRC( mroutine, dest->v.p );
			dest->v.mroutine = mroutine;
			dest->t = DAO_FUNCTREE;
			DValue_MarkConst( dest );
		}
		if( value.t == DAO_FUNCTREE ){
			DaoFunctree_Import( dest->v.mroutine, value.v.mroutine );
		}else{
			DaoFunctree_Add( dest->v.mroutine, (DRoutine*) value.v.p );
			/* Add individual entry for the new function: */
			DVarray_Append( self->cstData, value );
			DValue_MarkConst( self->cstData->data + self->cstData->size -1 );
		}
		id = node->value.pSize;
	}else{
		if( value.t == DAO_FUNCTREE && value.v.mroutine->space != self ){
			mroutine = DaoFunctree_New( self, name );
			DaoFunctree_Import( mroutine, value.v.mroutine );
			value.v.mroutine = mroutine;
		}
		id = LOOKUP_BIND( DAO_GLOBAL_CONSTANT, pm, 0, self->cstData->size );
		MAP_Insert( self->lookupTable, name, id ) ;
		DVarray_Append( self->cstData, daoNullValue );
		DValue_SimpleMove( value, self->cstData->data + self->cstData->size -1 );
		DValue_MarkConst( self->cstData->data + self->cstData->size -1 );
	}
	return id;
}
void DaoNameSpace_SetConst( DaoNameSpace *self, int index, DValue value )
{
	DValue *dest;
	int up = LOOKUP_UP( index );
	int id = LOOKUP_ID( index );
	if( LOOKUP_ST( index ) != DAO_GLOBAL_CONSTANT ) return;
	if( up >= self->cstDataTable->size ) return;
	dest = self->cstDataTable->items.pVarray[up]->data + id;
	DValue_SimpleMove( value, dest );
	DValue_MarkConst( dest );
}
DValue DaoNameSpace_GetConst( DaoNameSpace *self, int index )
{
	int st = LOOKUP_ST( index );
	int up = LOOKUP_UP( index );
	int id = LOOKUP_ID( index );
	if( index < 0 ) return daoNullValue;
	if( st != DAO_GLOBAL_CONSTANT ) return daoNullValue;
	if( up >= self->cstDataTable->size ) return daoNullValue;
	if( id >= self->cstDataTable->items.pVarray[up]->size ) return daoNullValue;
	return self->cstDataTable->items.pVarray[up]->data[id];
}
int  DaoNameSpace_FindVariable( DaoNameSpace *self, DString *name )
{
	DNode *node = DMap_Find( self->lookupTable, name );
	if( node == NULL ) return DaoNameSpace_FindParentData( self, name, DAO_GLOBAL_VARIABLE );
	if( LOOKUP_ST( node->value.pSize ) != DAO_GLOBAL_VARIABLE ) return -1;
	return node->value.pSize;
}
int DaoNameSpace_AddVariable( DaoNameSpace *self, DString *name, DValue value, DaoType *tp, int pm )
{
	DaoType *abtp = DaoNameSpace_GetTypeV( self, value );
	DNode *node = MAP_Find( self->lookupTable, name );
	DValue *dest;
	int id = 0;

	if( tp && value.t && DaoType_MatchValue( tp, value, NULL ) ==0 ) return -1;
	if( tp == NULL ) tp = abtp;
	if( value.t == 0 && tp ) value = tp->value;
	if( node && LOOKUP_UP( node->value.pSize ) ){ /* overriding */
		DMap_EraseNode( self->lookupTable, node );
		DaoNameSpace_AddVariable( self, name, value, tp, pm );
		node = MAP_Find( self->lookupTable, name );
		return node->value.pSize;
	}else if( node ){
		id = LOOKUP_ID( node->value.pSize );
		if( LOOKUP_ST( node->value.pSize ) != DAO_GLOBAL_VARIABLE ) return -1;
		assert( id < self->varData->size );
		dest = self->varData->data + id;
		if( DValue_Move( value, dest, tp ) ==0 ) return -1;
		id = node->value.pSize;
	}else{
		id = LOOKUP_BIND( DAO_GLOBAL_VARIABLE, pm, 0, self->varData->size );
		MAP_Insert( self->lookupTable, name, id ) ;
		DVarray_Append( self->varData, daoNullValue );
		DValue_Move( value, self->varData->data + self->varData->size -1, tp );
		DArray_Append( self->varType, (void*)tp );
		GC_IncRC( tp );
	}
	if( abtp->attrib & DAO_TYPE_EMPTY ){
		switch( value.t ){
		case DAO_LIST :
			GC_ShiftRC( tp, value.v.list->unitype );
			value.v.list->unitype = tp; break;
		case DAO_MAP :  
			GC_ShiftRC( tp, value.v.map->unitype );
			value.v.map->unitype = tp; break;
		case DAO_ARRAY : 
			GC_ShiftRC( tp, value.v.array->unitype );
			value.v.array->unitype = tp; break;
		case DAO_TUPLE : 
			GC_ShiftRC( tp, value.v.tuple->unitype );
			value.v.tuple->unitype = tp; break;
		default : break;
		}
	}
	return id;
}
int DaoNameSpace_SetVariable( DaoNameSpace *self, int index, DValue value )
{
	DaoType *type;
	DValue *dest;
	int up = LOOKUP_UP( index );
	int id = LOOKUP_ID( index );
	if( LOOKUP_ST( index ) != DAO_GLOBAL_CONSTANT ) return 0;
	if( up >= self->cstDataTable->size ) return 0;
	type = self->varTypeTable->items.pArray[up]->items.pType[ id ];
	dest = self->cstDataTable->items.pVarray[up]->data + id;
	return DValue_Move( value, dest, type );
}
DValue DaoNameSpace_GetVariable( DaoNameSpace *self, int index )
{
	int st = LOOKUP_ST( index );
	int up = LOOKUP_UP( index );
	int id = LOOKUP_ID( index );
	if( st != DAO_GLOBAL_VARIABLE ) return daoNullValue;
	if( up >= self->varDataTable->size ) return daoNullValue;
	if( id >= self->varDataTable->items.pVarray[up]->size ) return daoNullValue;
	return self->varDataTable->items.pVarray[up]->data[id];
}
DaoType* DaoNameSpace_GetVariableType( DaoNameSpace *self, int index )
{
	int st = LOOKUP_ST( index );
	int up = LOOKUP_UP( index );
	int id = LOOKUP_ID( index );
	if( st != DAO_GLOBAL_VARIABLE ) return NULL;
	if( up >= self->varTypeTable->size ) return NULL;
	if( id >= self->varTypeTable->items.pArray[up]->size ) return NULL;
	return self->varTypeTable->items.pArray[up]->items.pType[id];
}
void DaoNameSpace_SetData( DaoNameSpace *self, DString *name, DValue value )
{
	DNode *node = MAP_Find( self->lookupTable, name );
	if( node ){
		int id = node->value.pSize;
		int st = LOOKUP_ST( id );
		if( st == DAO_GLOBAL_CONSTANT ) DaoNameSpace_SetConst( self, id, value );
		if( st == DAO_GLOBAL_VARIABLE ) DaoNameSpace_SetVariable( self, id, value );
		return;
	}
	DaoNameSpace_AddVariable( self, name, value, NULL, DAO_DATA_PROTECTED );
}
DValue DaoNameSpace_GetData( DaoNameSpace *self, DString *name )
{
	DNode *node = MAP_Find( self->lookupTable, name );
	int st, id;
	if( node ){
		id = node->value.pSize;
		st = LOOKUP_ST( id );
		if( st == DAO_GLOBAL_CONSTANT ) return DaoNameSpace_GetConst( self, id );
		if( st == DAO_GLOBAL_VARIABLE ) return DaoNameSpace_GetVariable( self, id );
	}
	id = DaoNameSpace_FindParentData( self, name, DAO_GLOBAL_VARIABLE );
	if( id >= 0 ) return DaoNameSpace_GetVariable( self, id );
	id = DaoNameSpace_FindParentData( self, name, DAO_GLOBAL_CONSTANT );
	if( id >= 0 ) return DaoNameSpace_GetConst( self, id );
	return daoNullValue;
}
DaoClass* DaoNameSpace_FindClass( DaoNameSpace *self, DString *name )
{
	int id = DaoNameSpace_FindConst( self, name );
	DValue value = DaoNameSpace_GetConst( self, id );
	if( value.t == DAO_CLASS ) return value.v.klass;
	if( id >= 0 ) return NULL;
	id = DaoNameSpace_FindParentData( self, name, DAO_GLOBAL_CONSTANT );
	value = DaoNameSpace_GetConst( self, id );
	if( value.t == DAO_CLASS ) return value.v.klass;
	return NULL;
}
DaoNameSpace* DaoNameSpace_FindNameSpace( DaoNameSpace *self, DString *name )
{
	int id = DaoNameSpace_FindConst( self, name );
	DValue value = DaoNameSpace_GetConst( self, id );
	if( value.t == DAO_NAMESPACE ) return value.v.ns;
	if( id >= 0 ) return NULL;
	id = DaoNameSpace_FindParentData( self, name, DAO_GLOBAL_CONSTANT );
	value = DaoNameSpace_GetConst( self, id );
	if( value.t == DAO_NAMESPACE ) return value.v.ns;
	return NULL;
}
void DaoNameSpace_AddMacro( DaoNameSpace *self, DString *name, DaoMacro *macro, int local )
{
	DMap *macros = local ? self->localMacros : self->globalMacros;
	DNode *node = MAP_Find( macros, name );
	if( node == NULL ){
		GC_IncRC( macro );
		MAP_Insert( macros, name, macro );
	}else{
		DaoMacro *m2 = (DaoMacro*) node->value.pVoid;
		GC_IncRC( macro );
		DArray_Append( m2->macroList, macro );
	}
}
int DaoNameSpace_CyclicParent( DaoNameSpace *self, DaoNameSpace *parent )
{
	int i;
	if( parent == self ) return 1;
	for(i=0; i<self->parents->size; i++)
		if( self->parents->items.pNS[i] == parent ) return 0;
	for(i=0; i<parent->parents->size; i++){
		if( DaoNameSpace_CyclicParent( self, parent->parents->items.pNS[i] ) ) return 1;
	}
	return 0;
}
int DaoNameSpace_AddParent( DaoNameSpace *self, DaoNameSpace *parent )
{
	int i;
	DValue value = { DAO_NAMESPACE, 0, 0, 0, {0} };
	value.v.ns = parent;
	if( parent == self ) return 0;
	if( DaoNameSpace_CyclicParent( self, parent ) ) return 0;
	for(i=0; i<self->parents->size; i++)
		if( self->parents->items.pNS[i] == parent ) return 1;
	DVarray_Append( self->cstData, value );
	DValue_MarkConst( self->cstData->data + self->cstData->size -1 );
	DArray_Append( self->parents, parent );
	for(i=0; i<parent->parents->size; i++){
		if( DaoNameSpace_AddParent( self, parent->parents->items.pNS[i] ) ==0 ) return 0;
	}
	return 1;
}
static int DaoNameSpace_GetUpIndex( DaoNameSpace *self, DaoNameSpace *ns )
{
	int i, up = 0;
	if( ns == NULL ) return 0;
	for(i=0; i<self->nsTable->size; i++){
		if( self->nsTable->items.pNS[i] == ns ){
			up = i;
			ns = NULL;
			break;
		}
	}
	if( ns ){
		up = self->nsTable->size;
		DArray_Append( self->nsTable, ns );
		DArray_Append( self->cstDataTable, ns->cstData );
		DArray_Append( self->varDataTable, ns->varData );
		DArray_Append( self->varTypeTable, ns->varType );
	}
	return up;
}
static int DaoNameSpace_ImportRoutine( DaoNameSpace *self, DString *name, DValue v1, int pm )
{
	DNode *search = MAP_Find( self->lookupTable, name );
	if( search == NULL ){
		if( v1.t >= DAO_FUNCTREE && v1.t <= DAO_FUNCTION ){
			/* To allow function overloading: */
			DaoNameSpace_AddConst( self, name, v1, pm );
			return 0;
		}
		return 1;
	}else if( LOOKUP_ST( search->value.pSize ) == DAO_GLOBAL_CONSTANT ){
		DValue v2 = DaoNameSpace_GetConst( self, search->value.pSize );
		if( v1.t < DAO_FUNCTREE || v1.t > DAO_FUNCTION ) return 0;
		if( v2.t < DAO_FUNCTREE || v2.t > DAO_FUNCTION ) return 0;
		DaoNameSpace_AddConst( self, name, v1, pm );
	}
	return 0;
}
void DaoNameSpace_Import( DaoNameSpace *self, DaoNameSpace *ns, DArray *varImport )
{
	DArray *names = DArray_New(D_STRING);
	DNode *node, *search;
	int k, st, pm, up, id;

	if( varImport && varImport->size > 0 ){
		for( k=0; k<varImport->size; k++){
			DString *name = varImport->items.pString[k];
			node = MAP_Find( ns->lookupTable, name );
			if( node == NULL ){ DArray_Append( names, name ); continue; }
			pm = LOOKUP_PM( node->value.pSize );
			if( pm == DAO_DATA_PRIVATE ) continue;
			up = LOOKUP_UP( node->value.pSize );
			assert( up < ns->nsTable->size );
			st = LOOKUP_ST( node->value.pSize );
			id = LOOKUP_ID( node->value.pSize );
			if( st == DAO_GLOBAL_CONSTANT ){
				DValue v1 = DaoNameSpace_GetConst( ns, node->value.pSize );
				if( DaoNameSpace_ImportRoutine( self, name, v1, pm ) ) continue;
			}
			up = DaoNameSpace_GetUpIndex( self, ns->nsTable->items.pNS[up] );
			MAP_Insert( self->lookupTable, name, LOOKUP_BIND( st, pm, up, id ) );
		}
	}else{
		node = DMap_First( ns->lookupTable );
		for( ; node !=NULL; node = DMap_Next(ns->lookupTable, node ) ){
			DString *name = node->key.pString;
			search = MAP_Find( self->lookupTable, name );
			pm = LOOKUP_PM( node->value.pSize );
			if( pm != DAO_DATA_PUBLIC ) continue;
			up = LOOKUP_UP( node->value.pSize );
			if( up >= ns->nsTable->size ) continue;
			st = LOOKUP_ST( node->value.pSize );
			id = LOOKUP_ID( node->value.pSize );
			if( st == DAO_GLOBAL_CONSTANT ){
				DValue v1 = DaoNameSpace_GetConst( ns, node->value.pSize );
				if( DaoNameSpace_ImportRoutine( self, name, v1, pm ) ) continue;
			}
			up = DaoNameSpace_GetUpIndex( self, ns->nsTable->items.pNS[up] );
			MAP_Insert( self->lookupTable, name, LOOKUP_BIND( st, pm, up, id ) );
		}
	}
	if( varImport ) DArray_Swap( names, varImport );
	node = DMap_First( ns->abstypes );
	for( ; node !=NULL; node = DMap_Next(ns->abstypes, node ) )
		DaoNameSpace_AddType( self, node->key.pString, node->value.pType );

	DArray_Delete( names );
}

static DaoMacro* DaoNameSpace_FindMacro2( DaoNameSpace *self, DString *name )
{
	int i, n = self->parents->size;
	DNode *node = MAP_Find( self->globalMacros, name );
	if( node ) return (DaoMacro*) node->value.pVoid;
	for(i=0; i<n; i++){
		DaoNameSpace *ns = self->parents->items.pNS[i];
		DaoMacro *macro = DaoNameSpace_FindMacro2( ns, name );
		if( macro ) return macro;
	}
	return NULL;
}
DaoMacro* DaoNameSpace_FindMacro( DaoNameSpace *self, DString *name )
{
	int i, n = self->parents->size;
	DNode *node = MAP_Find( self->localMacros, name );
	if( node ) return (DaoMacro*) node->value.pVoid;
	node = MAP_Find( self->globalMacros, name );
	if( node ) return (DaoMacro*) node->value.pVoid;
	for(i=0; i<n; i++){
		DaoNameSpace *ns = self->parents->items.pNS[i];
		DaoMacro *macro = DaoNameSpace_FindMacro2( ns, name );
		if( macro == NULL ) continue;
		MAP_Insert( self->globalMacros, name, macro );
		GC_IncRC( macro );
		return macro;
	}
	return NULL;
}

DaoType* DaoNameSpace_FindType( DaoNameSpace *self, DString *name )
{
	DNode *node;
	int i, n = self->parents->size;
	//20110801 if( DString_FindChar( name, '?', 0 ) != MAXSIZE ) return NULL;
	node = MAP_Find( self->abstypes, name );
	if( node ) return node->value.pType;
	for(i=0; i<n; i++){
		DaoNameSpace *ns = self->parents->items.pNS[i];
		DaoType *type = DaoNameSpace_FindType( ns, name );
		if( type == NULL ) continue;
		MAP_Insert( self->abstypes, name, type );
		GC_IncRC( type );
		return type;
	}
	return NULL;
}
void DaoNameSpace_AddType( DaoNameSpace *self, DString *name, DaoType *tp )
{
	DNode *node = MAP_Find( self->abstypes, name );
#if 0
	//XXX no need? if( DString_FindChar( name, '?', 0 ) != MAXSIZE ) return 0;
#endif
	if( node == NULL ){
		MAP_Insert( self->abstypes, name, tp );
		GC_IncRC( tp );
	}
}
void DaoNameSpace_AddTypeConstant( DaoNameSpace *self, DString *name, DaoType *tp )
{
	int id = DaoNameSpace_FindConst( self, name );
	if( id >=0 ) return;
	if( tp->aux.v.p && (tp->tid >= DAO_OBJECT && tp->tid <= DAO_CTYPE) ){
		DaoNameSpace_AddConst( self, name, tp->aux, DAO_DATA_PUBLIC );
	}else if( tp->tid != DAO_VALTYPE && tp->tid != DAO_INITYPE ){
		DValue val = daoNullClass;
		val.t = DAO_TYPE;
		val.v.p = (DaoBase*) tp;
		DaoNameSpace_AddConst( self, name, val, DAO_DATA_PUBLIC );
	}
}

static DaoType *simpleTypes[ DAO_ARRAY ] = { 0, 0, 0, 0, 0, 0, 0, 0 };

void* DValue_GetTypeID( DValue self )
{
	void *id = NULL;
	switch( self.t ){
	case DAO_INTEGER :
	case DAO_FLOAT :
	case DAO_DOUBLE :
	case DAO_COMPLEX :
	case DAO_LONG :  
	case DAO_ENUM :  
	case DAO_STRING : id = simpleTypes[ self.t ]; break;
	case DAO_ARRAY : id = self.v.array->unitype; break;
	case DAO_LIST :  id = self.v.list->unitype; break;
	case DAO_MAP :   id = self.v.map->unitype; break;
	case DAO_PAR_NAMED : id = self.v.nameva->unitype; break;
	case DAO_TUPLE :  id = self.v.tuple->unitype; break;
	case DAO_OBJECT : id = self.v.object->myClass->objType; break;
	case DAO_CLASS :  id = self.v.klass->clsType; break;
	case DAO_CTYPE :
	case DAO_CDATA :  id = self.v.cdata->typer; break;
	case DAO_ROUTINE :
	case DAO_FUNCTION :  id = self.v.routine->routType; break;
	case DAO_VMPROCESS : id = self.v.vmp->abtype; break;
	default : id = DaoVmSpace_GetTyper( self.t ); break;
	}
	return id;
}
DaoType* DaoNameSpace_GetTypeV( DaoNameSpace *self, DValue val )
{
	DaoType *abtp = NULL;
	switch( val.t ){
	case DAO_NULL :
	case DAO_INTEGER : case DAO_FLOAT : case DAO_DOUBLE :
	case DAO_COMPLEX : case DAO_LONG : case DAO_STRING : 
		abtp = simpleTypes[ val.t ];
		if( abtp ) break;
		abtp = DaoNameSpace_MakeType( self, coreTypeNames[val.t], val.t, NULL, NULL, 0 );
		/* abtp = DaoType_New( coreTypeNames[val.t], val.t, NULL, NULL ); */
		simpleTypes[ val.t ] = abtp;
		GC_IncRC( abtp );
		break;
	case DAO_ENUM : 
		abtp = val.v.e->type;
		if( abtp ) break;
		abtp = simpleTypes[ val.t ];
		if( abtp ) break;
		abtp = DaoNameSpace_MakeType( self, coreTypeNames[val.t], val.t, NULL, NULL, 0 );
		/* abtp = DaoType_New( coreTypeNames[val.t], val.t, NULL, NULL ); */
		simpleTypes[ val.t ] = abtp;
		GC_IncRC( abtp );
		break;
	default:
		abtp = DaoNameSpace_GetType( self, val.v.p );
		break;
	}
	return abtp;
}
DaoType* DaoNameSpace_GetType( DaoNameSpace *self, DaoBase *p )
{
	DNode *node;
	DArray *nested = NULL;
	DString *mbs;
	DRoutine *rout;
	DaoType *abtp = NULL;
	DaoType *itp = (DaoType*) p;
	DaoTuple *tuple = (DaoTuple*) p;
	DaoNameValue *nameva = (DaoNameValue*) p;
	DaoList *list = (DaoList*) p;
	DaoMap *map = (DaoMap*) p;
	DaoArray *array = (DaoArray*) p;
	DaoCData *cdata = (DaoCData*) p;
	DaoVmProcess *vmp = (DaoVmProcess*) p;
	DaoTypeBase *typer;
	int i, tid, zerosize = 0;

	if( p == NULL ) return NULL;
	if( p->type == DAO_TYPE && itp->tid == DAO_TYPE ) return itp;
	tid = p->type;

	switch( p->type ){
	case DAO_INTEGER : case DAO_FLOAT : case DAO_DOUBLE :
	case DAO_COMPLEX : case DAO_LONG :
	case DAO_ENUM : case DAO_STRING : 
		abtp = simpleTypes[ p->type ]; break;
	case DAO_LIST :
		abtp = list->unitype; break;
	case DAO_MAP :
		abtp = map->unitype; break;
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		abtp = array->unitype; break;
#endif
	case DAO_OBJECT :
		abtp = ((DaoObject*)p)->myClass->objType; break;
	case DAO_CLASS :
		abtp = ((DaoClass*)p)->clsType; break;
	case DAO_CTYPE :
	case DAO_CDATA :
		abtp = ((DaoCData*)p)->ctype; break;
	case DAO_FUNCTREE :
		abtp = ((DaoFunctree*)p)->unitype; break;
	case DAO_ROUTINE :
	case DAO_FUNCTION :
		rout = (DRoutine*) p;
		abtp = rout->routType;
		break;
	case DAO_PAR_NAMED :
		abtp = nameva->unitype; break;
	case DAO_TUPLE :
		abtp = tuple->unitype; break;
	case DAO_FUTURE :
		abtp = ((DaoFuture*)p)->unitype; break;
	case DAO_VMPROCESS :
		abtp = vmp->abtype; break;
	case DAO_INTERFACE :
		abtp = ((DaoInterface*)p)->abtype; break;
	default : break;
	}
	if( abtp ){
		abtp->typer = DaoBase_GetTyper( p );
		return abtp;
	}

	mbs = DString_New(1);
	if( p->type <= DAO_STREAM ){
		DString_SetMBS( mbs, coreTypeNames[p->type] );
		if( p->type == DAO_LIST ){
			nested = DArray_New(0);
			if( list->items->size ==0 ){
				DString_AppendMBS( mbs, "<?>" );
				DArray_Append( nested, self->udfType1 );
				zerosize = 1;
			}else{
				itp = DaoNameSpace_MakeType( self, "any", DAO_ANY, 0,0,0 );
				DString_AppendMBS( mbs, "<any>" );
				DArray_Append( nested, itp );
			}  
		}else if( p->type == DAO_MAP ){
			nested = DArray_New(0);
			if( map->items->size ==0 ){
				DString_AppendMBS( mbs, "<?,?>" );
				DArray_Append( nested, self->udfType1 );
				DArray_Append( nested, self->udfType2 );
				zerosize = 1;
			}else{
				itp = DaoNameSpace_MakeType( self, "any", DAO_ANY, 0,0,0 );
				DString_AppendMBS( mbs, "<any,any>" );
				DArray_Append( nested, itp );
				DArray_Append( nested, itp );
			}
#ifdef DAO_WITH_NUMARRAY
		}else if( p->type == DAO_ARRAY ){
			nested = DArray_New(0);
			if( array->size ==0 ){
				DString_AppendMBS( mbs, "<?>" );
				DArray_Append( nested, self->udfType1 );
				zerosize = 1;
			}else if( array->numType == DAO_INTEGER ){
				itp = DaoNameSpace_MakeType( self, "int", DAO_INTEGER, 0,0,0 );
				DString_AppendMBS( mbs, "<int>" );
				DArray_Append( nested, itp );
			}else if( array->numType == DAO_FLOAT ){
				itp = DaoNameSpace_MakeType( self, "float", DAO_FLOAT, 0,0,0 );
				DString_AppendMBS( mbs, "<float>" );
				DArray_Append( nested, itp );
			}else if( array->numType == DAO_DOUBLE ){
				itp = DaoNameSpace_MakeType( self, "double", DAO_DOUBLE, 0,0,0 );
				DString_AppendMBS( mbs, "<double>" );
				DArray_Append( nested, itp );
			}else{
				itp = DaoNameSpace_MakeType( self, "complex", DAO_COMPLEX, 0,0,0 );
				DString_AppendMBS( mbs, "<complex>" );
				DArray_Append( nested, itp );
			}
#endif
		}else if( p->type == DAO_TUPLE ){
			DString_SetMBS( mbs, "tuple<" );
			nested = DArray_New(0);
			for(i=0; i<tuple->items->size; i++){
				itp = DaoNameSpace_GetTypeV( self, tuple->items->data[i] );
				DArray_Append( nested, itp );
				DString_Append( mbs, itp->name );
				if( i+1 < tuple->items->size ) DString_AppendMBS( mbs, "," );
			}
			DString_AppendMBS( mbs, ">" );
		}
		node = MAP_Find( self->abstypes, mbs );
		if( node ){
			abtp = (DaoType*) node->value.pBase;
		}else{
			abtp = DaoType_New( mbs->mbs, tid, NULL, nested );
			if( p->type < DAO_ARRAY ){
				simpleTypes[ p->type ] = abtp;
				GC_IncRC( abtp );
			}
			if( zerosize ) abtp->attrib |= DAO_TYPE_EMPTY;
			/* XXX if( DString_FindChar( abtp->name, '?', 0 ) == MAXSIZE ) */
			DaoNameSpace_AddType( self, abtp->name, abtp );
		}
#if 1
		switch( p->type ){
		case DAO_LIST :
			if( ! ( abtp->attrib & DAO_TYPE_EMPTY ) ){
				list->unitype = abtp;
				GC_IncRC( abtp );
			}
			break;
		case DAO_MAP :
			if( ! ( abtp->attrib & DAO_TYPE_EMPTY ) ){
				map->unitype = abtp;
				GC_IncRC( abtp );
			}
			break;
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			if( ! ( abtp->attrib & DAO_TYPE_EMPTY ) ){
				array->unitype = abtp;
				GC_IncRC( abtp );
			}
			break;
#endif
		case DAO_PAR_NAMED :
			GC_IncRC( abtp );
			nameva->unitype = abtp;
			break;
		case DAO_TUPLE :
			GC_IncRC( abtp );
			tuple->unitype = abtp;
			break;
		default : break;
		}
#endif
	}else if( p->type == DAO_ROUTINE || p->type == DAO_FUNCTION ){
		DRoutine *rout = (DRoutine*) p;
		abtp = rout->routType; /* might be NULL */
	}else if( p->type == DAO_CDATA ){
		DString_AppendMBS( mbs, cdata->typer->name );
		node = MAP_Find( self->abstypes, mbs );
		if( node ){
			abtp = (DaoType*) node->value.pBase;
		}else{
			abtp = DaoType_New( mbs->mbs, p->type, p, NULL );
			DaoNameSpace_AddType( self, abtp->name, abtp );
		}
		abtp->typer = cdata->typer;
	}else if( p->type == DAO_TYPE ){
		DString_SetMBS( mbs, "type<" );
		nested = DArray_New(0);
		DArray_Append( nested, itp );
		DString_Append( mbs, itp->name );
		DString_AppendMBS( mbs, ">" );
		node = MAP_Find( self->abstypes, mbs );
		if( node ){
			abtp = (DaoType*) node->value.pBase;
		}else{
			abtp = DaoType_New( mbs->mbs, p->type, NULL, nested );
			DaoNameSpace_AddType( self, abtp->name, abtp );
		}
	}else{
		typer = DaoBase_GetTyper( p );
		DString_SetMBS( mbs, typer->name );
		node = MAP_Find( self->abstypes, mbs );
		if( node ){
			abtp = (DaoType*) node->value.pBase;
		}else{
			abtp = DaoType_New( typer->name, p->type, NULL, NULL );
			abtp->typer = typer;
			DaoNameSpace_AddType( self, abtp->name, abtp );
		}
	}
	/* abtp might be rout->routType, which might be NULL,
	 * in case rout is DaoNameSpace.routEvalConst */
	if( abtp && abtp->typer ==NULL ) abtp->typer = DaoBase_GetTyper( p );
	DString_Delete( mbs );
	if( nested ) DArray_Delete( nested );
	return abtp;
}
DaoType* DaoNameSpace_MakeType( DaoNameSpace *self, const char *name, 
		uchar_t tid, DaoBase *pb, DaoType *nest[], int N )
{
	DaoClass   *klass;
	DaoType *any = NULL;
	DaoType *tp;
	DNode      *node;
	DString    *mbs = DString_New(1);
	DArray     *nstd = DArray_New(0);
	int i, n = strlen( name );

	if( tid != DAO_ANY )
		any = DaoNameSpace_MakeType( self, "any", DAO_ANY, 0,0,0 );

	DString_SetMBS( mbs, name );
	if( N > 0 ){
		if( n || tid != DAO_VARIANT ) DString_AppendChar( mbs, '<' );
		DString_Append( mbs, nest[0]->name );
		DArray_Append( nstd, nest[0] );
		for(i=1; i<N; i++){
			DString_AppendChar( mbs, tid == DAO_VARIANT ? '|' : ',' );
			DString_Append( mbs, nest[i]->name );
			DArray_Append( nstd, nest[i] );
		}
		if( tid == DAO_ROUTINE && pb && pb->type == DAO_TYPE ){
			DString_AppendMBS( mbs, "=>" );
			DString_Append( mbs, ((DaoType*)pb)->name );
		}
		if( n || tid != DAO_VARIANT ) DString_AppendChar( mbs, '>' );
	}else if( tid == DAO_LIST || tid == DAO_ARRAY ){
		DString_AppendMBS( mbs, "<any>" );
		DArray_Append( nstd, any );
	}else if( tid == DAO_MAP ){
		DString_AppendMBS( mbs, "<any,any>" );
		DArray_Append( nstd, any );
		DArray_Append( nstd, any );
	}else if( tid == DAO_CLASS && pb ){
		/* do not save the abstract type for class and object in namespace,
		 * because the class may be nested in another class, and different
		 * class may nest different class with the same name, eg:
		 * Error::Field::NotExist and Error::Key::NotExist
		 * */
		klass = (DaoClass*) pb;
		tp = klass->clsType;
		goto Finalizing;
	}else if( tid == DAO_OBJECT ){
		klass = (DaoClass*) pb;
		tp = klass->objType;
		goto Finalizing;
	}else if( tid == DAO_ROUTINE && pb && pb->type == DAO_TYPE ){
		DString_AppendChar( mbs, '<' );
		DString_AppendMBS( mbs, "=>" );
		DString_Append( mbs, ((DaoType*)pb)->name );
		DString_AppendChar( mbs, '>' );
	}else if( tid == DAO_PAR_NAMED ){
		DString_AppendMBS( mbs, ":" );
		if( pb->type == DAO_TYPE ) DString_Append( mbs, ((DaoType*)pb)->name );
	}else if( tid == DAO_PAR_DEFAULT ){
		DString_AppendMBS( mbs, "=" );
		if( pb->type == DAO_TYPE ) DString_Append( mbs, ((DaoType*)pb)->name );
	}
	node = MAP_Find( self->abstypes, mbs );
	if( node == NULL ){
		if( tid == DAO_PAR_NAMED || tid == DAO_PAR_DEFAULT ) DString_SetMBS( mbs, name );
		tp = DaoType_New( mbs->mbs, tid, pb, nstd );
		if( pb && pb->type == DAO_CDATA ) tp->typer = ((DaoCData*)pb)->typer;
		DaoNameSpace_AddType( self, tp->name, tp );
	}else{
		tp = node->value.pType;
	}
Finalizing:
	DString_Delete( mbs );
	DArray_Delete( nstd );
	return tp;
}
DaoType* DaoNameSpace_MakeRoutType( DaoNameSpace *self, DaoType *routype,
		DValue *vals, DaoType *types[], DaoType *retp )
{
	DaoType *tp, *tp2, *abtp;
	DString *fname = NULL;
	DNode *node;
	int i, ch = 0;

	abtp = DaoType_New( "", DAO_ROUTINE, NULL, NULL );
	abtp->attrib = routype->attrib;
	if( routype->mapNames ){
		if( abtp->mapNames ) DMap_Delete( abtp->mapNames );
		abtp->mapNames = DMap_Copy( routype->mapNames );
	}

	if( routype->name->mbs[0] == '@' ) DString_AppendChar( abtp->name, '@' );
	DString_AppendMBS( abtp->name, "routine<" );
	for(i=0; i<routype->nested->size; i++){
		if( i >0 ) DString_AppendMBS( abtp->name, "," );
		tp = tp2 = routype->nested->items.pType[i];
		if( tp && (tp->tid == DAO_PAR_NAMED || tp->tid == DAO_PAR_DEFAULT) ){
			ch = tp->name->mbs[tp->fname->size];
			tp2 = tp->aux.v.type;
		}
		if( tp2 && tp2->tid ==DAO_UDF ){
			if( vals ){
				tp2 = DaoNameSpace_GetTypeV( self, vals[i] );
			}else if( types && types[i] ){
				tp2 = types[i];
			}
		}
		/* XXX typing DString_AppendMBS( abtp->name, tp ? tp->name->mbs : "..." ); */
		if( tp2 != tp && tp2 != tp->aux.v.type ){
			fname = tp->fname;
			tp = DaoType_New( fname->mbs, tp->tid, (DaoBase*) tp2, NULL );
			DString_AppendChar( tp->name, ch );
			DString_Append( tp->name, tp2->name );
			if( tp->fname ) DString_Assign( tp->fname, fname );
			else tp->fname = DString_Copy( fname );
		}
		DString_Append( abtp->name, tp->name );
		DArray_Append( abtp->nested, tp );
	}
	tp = retp ? retp : routype->aux.v.type;
	if( tp ){
		DString_AppendMBS( abtp->name, "=>" );
		DString_Append( abtp->name, tp->name );
	}
	DString_AppendMBS( abtp->name, ">" );
	abtp->aux.t = DAO_TYPE;
	abtp->aux.v.type = tp;
	GC_IncRC( abtp->aux.v.type );
	GC_IncRCs( abtp->nested );
	node = MAP_Find( self->abstypes, abtp->name );
	if( node ){
		DaoType_Delete( abtp );
		return node->value.pType;
	}
	DaoType_CheckAttributes( abtp );
	DaoType_InitDefault( abtp );
	DaoNameSpace_AddType( self, abtp->name, abtp );
	return abtp;
}

DaoFunction* DaoNameSpace_ParsePrototype( DaoNameSpace *self, const char *proto, DaoParser *parser )
{
	DaoFunction *func = DaoFunction_New();
	DaoParser *defparser;
	int key = DKEY_OPERATOR;
	int optok = 0;

	assert( parser != NULL );
	assert( parser->defParser != NULL );
	defparser = parser->defParser;

	GC_IncRC( self );
	GC_IncRC( parser->hostCData );
	func->nameSpace = self;
	func->routHost = parser->hostCData;
	if( ! DaoToken_Tokenize( defparser->tokens, proto, 0, 0, 0 ) ) goto Error;
	if( defparser->tokens->size < 3 ) goto Error;
	if( (optok = defparser->tokens->items.pToken[0]->name == DKEY_OPERATOR) == 0 ){
		if( defparser->tokens->items.pToken[0]->type == DTOK_IDENTIFIER 
				&& defparser->tokens->items.pToken[1]->type == DTOK_LB ) key = 0;
	}
	DArray_Clear( defparser->partoks );

	parser->routine = (DaoRoutine*) func; /* safe to parse params only */
	if( DaoParser_ParsePrototype( defparser, parser, key, optok ) < 0 ) goto Error;
	if( DaoParser_ParseParams( parser, key ) == 0 ) goto Error;
	return func;
Error:
	DaoFunction_Delete( func );
	return NULL;
}
/* symbols should be comma or semicolon delimited string */
DaoType* DaoNameSpace_MakeEnumType( DaoNameSpace *self, const char *symbols )
{
	DaoType *type;
	DString *key, *name = DString_New(1);
	int n = strlen( symbols );
	int i, k = 0, t1 = 0, t2 = 0;

	DString_SetMBS( name, "enum<" );
	DString_AppendMBS( name, symbols );
	DString_AppendChar( name, '>' );
	type = DaoNameSpace_FindType( self, name );
	if( type ){
		DString_Delete( name );
		return type;
	}
	key = DString_New(1);
	type = DaoType_New( name->mbs, DAO_ENUM, NULL, NULL );
	type->mapNames = DMap_New(D_STRING,0);
	for(i=0; i<n; i++){
		char sym = symbols[i];
		if( sym == ',' ){
			MAP_Insert( type->mapNames, key, k );
			DString_Clear( key );
			k += 1;
			t1 = 1;
		}else if( sym == ';' ){
			MAP_Insert( type->mapNames, key, 1<<k );
			DString_Clear( key );
			k += 1;
			t2 = 1;
		}else{
			DString_AppendChar( key, sym );
		}
	}
	if( t2 ){
		MAP_Insert( type->mapNames, key, 1<<k );
	}else{
		MAP_Insert( type->mapNames, key, k );
	}
	DaoNameSpace_AddType( self, name, type );
	DString_Delete( name );
	DString_Delete( key );
	return (t1&t2) ==0 ? type : NULL;
}
DaoType* DaoNameSpace_SymbolTypeAdd( DaoNameSpace *self, DaoType *t1, DaoType *t2, dint *value )
{
	DaoType *type;
	DMap *names1 = t1->mapNames;
	DMap *names2 = t2->mapNames;
	DMap *mapNames;
	DNode *node;
	DString *name;
	int i = 0;
	*value = 0;
	if( t1->name->mbs[0] != '$' && t2->name->mbs[0] != '$' ) return NULL;
	name = DString_New(1);
	for(node=DMap_First(names1);node;node=DMap_Next(names1,node)){
		DString_AppendChar( name, '$' );
		DString_Append( name, node->key.pString );
		*value |= 1<<(i++);
	}
	for(node=DMap_First(names2);node;node=DMap_Next(names2,node)){
		if( DMap_Find( names1, node->key.pVoid ) ) continue;
		DString_AppendChar( name, '$' );
		DString_Append( name, node->key.pString );
	}
	type = DaoNameSpace_FindType( self, name );
	if( type == NULL ){
		type = DaoType_New( name->mbs, DAO_ENUM, NULL, NULL );
		type->flagtype = 1;
		type->mapNames = mapNames = DMap_Copy( names1 );
		if( mapNames->size == 1 ){
			mapNames->root->value.pInt = 1;
			*value = 1;
		}
		for(node=DMap_First(names2);node;node=DMap_Next(names2,node)){
			if( DMap_Find( names1, node->key.pVoid ) ) continue;
			*value |= (1<<mapNames->size);
			MAP_Insert( mapNames, node->key.pVoid, 1<<mapNames->size );
		}
		DaoNameSpace_AddType( self, name, type );
	}
	DString_Delete( name );
	return type;
}
DaoType* DaoNameSpace_SymbolTypeSub( DaoNameSpace *self, DaoType *t1, DaoType *t2, dint *value )
{
	DaoType *type;
	DMap *names1 = t1->mapNames;
	DMap *names2 = t2->mapNames;
	DMap *mapNames;
	DNode *node;
	DString *name;
	int count = 0;
	*value = 0;
	if( t1->name->mbs[0] != '$' && t2->name->mbs[0] != '$' ) return NULL;
	name = DString_New(1);
	for(node=DMap_First(names1);node;node=DMap_Next(names1,node)){
		if( DMap_Find( names2, node->key.pVoid ) ) continue;
		DString_AppendChar( name, '$' );
		DString_Append( name, node->key.pString );
		count += 1;
	}
	if( count ==0 ){
		DString_Delete( name );
		return 0;
	}
	type = DaoNameSpace_FindType( self, name );
	if( type == NULL ){
		type = DaoType_New( name->mbs, DAO_ENUM, NULL, NULL );
		type->flagtype = count > 1 ? 1 : 0;
		type->mapNames = mapNames = DMap_New(D_STRING,0);
		*value = type->flagtype;
		for(node=DMap_First(names1);node;node=DMap_Next(names1,node)){
			if( DMap_Find( names2, node->key.pVoid ) ) continue;
			*value |= (1<<mapNames->size);
			MAP_Insert( mapNames, node->key.pVoid, 1<<mapNames->size );
		}
		DaoNameSpace_AddType( self, name, type );
	}
	DString_Delete( name );
	return type;
}
DaoType* DaoNameSpace_MakeValueType( DaoNameSpace *self, DValue value )
{
	DaoType *type;
	DString *name;
	if( value.cst ==0 || value.t >= DAO_ARRAY ) return NULL;
	name = DString_New(1);
	DValue_GetString( value, name );
	if( value.t == DAO_STRING ){
		DString_InsertChar( name, '\'', 0 );
		DString_AppendChar( name, '\'' );
	}
	if( name->size ==0 && value.t ==0 ) DString_SetMBS( name, "null" );
	type = DaoNameSpace_MakeType( self->vmSpace->nsInternal, name->mbs, DAO_VALTYPE, 0,0,0 );
	DValue_Copy( & type->aux, value );
	DString_Delete( name );
	return type;
}
DaoType* DaoNameSpace_MakePairType( DaoNameSpace *self, DaoType *first, DaoType *second )
{
	DaoType *types[2] = {NULL, NULL};
	DaoType *nullType = DaoNameSpace_MakeType( self, "null", DAO_VALTYPE, 0, 0, 0 );
	if( first == NULL ) first = nullType;
	if( second == NULL ) second = nullType;
	types[0] = DaoNameSpace_MakeType( self, "first", DAO_PAR_NAMED, (DaoBase*)first, 0, 0 );
	types[1] = DaoNameSpace_MakeType( self, "second", DAO_PAR_NAMED, (DaoBase*)second, 0, 0 );
	return DaoNameSpace_MakeType( self, "tuple", DAO_TUPLE, NULL, types, 2 );
}
DaoType* DaoNameSpace_MakePairValueType( DaoNameSpace *self, DValue first, DValue second )
{
	DaoType *tp1, *tp2;
	first.cst = second.cst = 1;
	tp1 = DaoNameSpace_MakeValueType( self, first );
	tp2 = DaoNameSpace_MakeValueType( self, second );
	return DaoNameSpace_MakePairType( self, tp1, tp2 );
}
DaoTuple* DaoNameSpace_MakePair( DaoNameSpace *self, DValue first, DValue second )
{
	DaoTuple *tuple = DaoTuple_New(2);
	DaoType *type1 = DaoNameSpace_MakeValueType( self, first );
	DaoType *type2 = DaoNameSpace_MakeValueType( self, second );
	tuple->unitype = DaoNameSpace_MakePairType( self, type1, type2 );
	GC_IncRC( tuple->unitype );
	DValue_Copy( & tuple->items->data[0], first );
	DValue_Copy( & tuple->items->data[1], second );
	tuple->pair = 1;
	return tuple;
}

static void NS_Backup( DaoNameSpace *self, DaoVmProcess *proc, FILE *fout, int limit, int store )
{
	DNode *node = DMap_First( self->lookupTable );
	DValue value = daoNullValue;
	DString *prefix = DString_New(1);
	DString *serial = DString_New(1);
	size_t max = limit * 1000; /* limit per object in KB */
	int id, pm, up, st;

	for( ; node !=NULL; node = DMap_Next( self->lookupTable, node ) ){
		DString *name = node->key.pString;
		id = node->value.pSize;
		up = LOOKUP_UP( id );
		st = LOOKUP_ST( id );
		pm = LOOKUP_PM( id );
		if( up ) continue;
		if( st != store ) continue;
		if( st == DAO_GLOBAL_CONSTANT ) value = DaoNameSpace_GetConst( self, id );
		if( st == DAO_GLOBAL_VARIABLE ) value = DaoNameSpace_GetVariable( self, id );
		if( value.t == 0 ) continue;
		if( DValue_Serialize( & value, serial, self, proc ) ==0 ) continue;
		prefix->size = 0;
		switch( pm ){
		case DAO_DATA_PRIVATE   : DString_AppendMBS( prefix, "private " ); break;
		case DAO_DATA_PROTECTED : DString_AppendMBS( prefix, "protected " ); break;
		case DAO_DATA_PUBLIC    : DString_AppendMBS( prefix, "public " ); break;
		}
		switch( st ){
		case DAO_GLOBAL_CONSTANT : DString_AppendMBS( prefix, "const " ); break;
		case DAO_GLOBAL_VARIABLE : DString_AppendMBS( prefix, "var " ); break;
		}
		if( max && prefix->size + name->size + serial->size + 4 > max ) continue;
		fprintf( fout, "%s%s = %s\n", prefix->mbs, name->mbs, serial->mbs );
	}
	DString_Delete( prefix );
	DString_Delete( serial );
}
void DaoNameSpace_Backup( DaoNameSpace *self, DaoVmProcess *proc, FILE *fout, int limit )
{
	int i;
	NS_Backup( self, proc, fout, limit, DAO_GLOBAL_CONSTANT );
	if( self->inputs->size ){
		static const char *digits = "ABCDEFGHIJKLMNOP";
		unsigned char *mbs = (unsigned char*) self->inputs->mbs;
		fprintf( fout, "require { " );
		for(i=0; i<self->inputs->size; i++){
			fprintf( fout, "%c%c", digits[ mbs[i] / 16 ], digits[ mbs[i] % 16 ] );
		}
		fprintf( fout, " }\n" );
	}
	NS_Backup( self, proc, fout, limit, DAO_GLOBAL_VARIABLE );
}
int DaoParser_Deserialize( DaoParser*, int, int, DValue*, DArray*, DaoNameSpace*, DaoVmProcess* );
void DaoNameSpace_Restore( DaoNameSpace *self, DaoVmProcess *proc, FILE *fin )
{
	DaoParser *parser = DaoParser_New();
	DString *line = DString_New(1);
	DArray *types = DArray_New(0);
	DArray *tokens = parser->tokens;
	DString *name;
	DNode *node;

	parser->nameSpace = self;
	parser->vmSpace = self->vmSpace;
	while( DaoFile_ReadLine( fin, line ) ){
		DValue value = daoNullValue;
		int st = DAO_GLOBAL_VARIABLE;
		int pm = DAO_DATA_PRIVATE;
		int i, n, start = 0;
		char *mbs;

		DaoParser_LexCode( parser, line->mbs, 0 );
		if( tokens->size == 0 ) continue;
		if( tokens->items.pToken[start]->name == DKEY_REQUIRE ){
			if( tokens->size < 3 ) continue;
			DString_Clear( line );
			n = tokens->items.pToken[start+2]->string->size;
			mbs = tokens->items.pToken[start+2]->string->mbs;
			for(i=0; i<n; i++){
				char c1 = mbs[i];
				char c2 = mbs[i+1];
				if( c1 < 'A' || c1 > 'P' ) continue;
				DString_AppendChar( line, (char)((c1-'A')*16 + (c2-'A')) );
				i += 1;
			}
			/* printf( "%s\n", line->mbs ); */
			DaoVmProcess_Eval( proc, self, line, 0 );
			continue;
		}
		switch( tokens->items.pToken[start]->name ){
		case DKEY_PRIVATE   : pm = DAO_DATA_PRIVATE;   start += 1; break;
		case DKEY_PROTECTED : pm = DAO_DATA_PROTECTED; start += 1; break;
		case DKEY_PUBLIC    : pm = DAO_DATA_PUBLIC;    start += 1; break;
		}
		if( start >= tokens->size ) continue;
		switch( tokens->items.pToken[start]->name ){
		case DKEY_CONST : st = DAO_GLOBAL_CONSTANT; start += 1; break;
		case DKEY_VAR   : st = DAO_GLOBAL_VARIABLE; start += 1; break;
		}
		if( tokens->items.pToken[start]->name != DTOK_IDENTIFIER ) continue;
		name = tokens->items.pToken[start]->string;
		start += 1;
		if( start + 3 >= tokens->size ) continue;
		if( tokens->items.pToken[start]->name != DTOK_ASSN ) continue;
		start += 1;

		DArray_Clear( parser->errors );
		DArray_Clear( types );
		DArray_PushFront( types, NULL );
		DaoParser_Deserialize( parser, start, tokens->size-1, &value, types, self, proc );
		if( value.t == 0 ) continue;
		node = DMap_Find( self->lookupTable, name );
		if( node ) continue;
		if( st == DAO_GLOBAL_CONSTANT ){
			DaoNameSpace_AddConst( self, name, value, pm );
		}else{
			DaoNameSpace_AddVariable( self, name, value, NULL, pm );
		}
		DValue_Clear( & value );
	}
	DString_Delete( line );
	DArray_Delete( types );
	DaoParser_Delete( parser );
}
