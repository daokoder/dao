/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

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
#include"daoProcess.h"
#include"daoGC.h"
#include"daoStdlib.h"
#include"daoClass.h"
#include"daoParser.h"
#include"daoMacro.h"
#include"daoRegex.h"
#include"daoValue.h"


/* Need separated mutexes for values and methods setup.
 * Otherwise, a mutex deadlock may occur if values setup 
 * is triggered by methods setup. */
DMutex mutex_values_setup;
DMutex mutex_methods_setup;
DMutex mutex_type_map;


static void DNS_GetField( DaoValue *self0, DaoProcess *proc, DString *name )
{
	DaoNamespace *self = & self0->xNamespace;
	DNode *node = NULL;
	int st, pm, id;
	node = MAP_Find( self->lookupTable, name );
	if( node == NULL ) goto FieldNotExist;
	st = LOOKUP_ST( node->value.pInt );
	pm = LOOKUP_PM( node->value.pInt );
	id = LOOKUP_ID( node->value.pInt );
	if( pm == DAO_DATA_PRIVATE && self != proc->activeNamespace ) goto FieldNoPermit;
	if( st == DAO_GLOBAL_CONSTANT ){
		DaoProcess_PutValue( proc, self->constants->items.pConst[id]->value );
	}else{
		DaoProcess_PutValue( proc, self->variables->items.pVar[id]->value );
	}
	return;
FieldNotExist:
	DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, name->mbs );
	return;
FieldNoPermit:
	DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTPERMIT, name->mbs );
	return;
InvalidField:
	DaoProcess_RaiseException( proc, DAO_ERROR_FIELD, name->mbs );
}
static void DNS_SetField( DaoValue *self0, DaoProcess *proc, DString *name, DaoValue *value )
{
	DaoNamespace *self = & self0->xNamespace;
	DaoVariable *dest;
	DNode *node = NULL;
	int st, pm, id;
	node = MAP_Find( self->lookupTable, name );
	if( node == NULL ) goto FieldNotExist;
	st = LOOKUP_ST( node->value.pInt );
	pm = LOOKUP_PM( node->value.pInt );
	id = LOOKUP_ID( node->value.pInt );
	if( pm == DAO_DATA_PRIVATE && self != proc->activeNamespace ) goto FieldNoPermit;
	if( st == DAO_GLOBAL_CONSTANT ) goto FieldNoPermit;
	dest = self->variables->items.pVar[id];
	if( DaoValue_Move( value, & dest->value, dest->dtype ) ==0 ) goto TypeNotMatching;
	return;
FieldNotExist:
	DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTEXIST, name->mbs );
	return;
FieldNoPermit:
	DaoProcess_RaiseException( proc, DAO_ERROR_FIELD_NOTPERMIT, name->mbs );
	return;
TypeNotMatching:
	DaoProcess_RaiseException( proc, DAO_ERROR_TYPE, "not matching" );
	return;
InvalidField:
	DaoProcess_RaiseException( proc, DAO_ERROR_FIELD, name->mbs );
}
static void DNS_GetItem( DaoValue *self0, DaoProcess *proc, DaoValue *ids[], int N )
{
}
static void DNS_SetItem( DaoValue *self0, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
}
static DaoTypeCore nsCore =
{
	NULL,
	DNS_GetField,
	DNS_SetField,
	DNS_GetItem,
	DNS_SetItem,
	DaoValue_Print,
	DaoValue_NoCopy, /* do not copy namespace */
};

DaoNamespace* DaoNamespace_GetNamespace( DaoNamespace *self, const char *name )
{
	DaoNamespace *ns;
	DString mbs = DString_WrapMBS( name );
	ns = DaoNamespace_FindNamespace( self, & mbs );
	if( ns == NULL ){
		ns = DaoNamespace_New( self->vmSpace, name );
		DArray_Append( ns->namespaces, self );
		DaoNamespace_AddConst( self, & mbs, (DaoValue*)ns, DAO_DATA_PUBLIC );
		DArray_Append( ns->auxData, self ); /* for GC */
	}
	return ns;
}
void DaoNamespace_AddValue( DaoNamespace *self, const char *s, DaoValue *v, const char *t )
{
	DaoType *abtp = NULL;
	DString name = DString_WrapMBS( s );
	if( t && strlen( t ) >0 ){
		DaoParser *parser = DaoParser_New();
		parser->nameSpace = self;
		parser->vmSpace = self->vmSpace;
		abtp = DaoParser_ParseTypeName( t, self, NULL ); /* XXX warn */
		DaoParser_Delete( parser );
	}
	DaoNamespace_AddVariable( self, & name, v, abtp, DAO_DATA_PUBLIC );
}
DaoValue* DaoNamespace_FindData( DaoNamespace *self, const char *name )
{
	DString s = DString_WrapMBS( name );
	return DaoNamespace_GetData( self, & s );
}
void DaoNamespace_AddConstNumbers( DaoNamespace *self, DaoNumItem *items )
{
	DaoDouble buf = {0,0,0,0,0,0.0};
	DaoValue *value = (DaoValue*) & buf;
	int i = 0;
	while( items[i].name != NULL ){
		DString name = DString_WrapMBS( items[i].name );
		switch( items[i].type ){
		case DAO_INTEGER : value->xInteger.value = (int) items[i].value; break;
		case DAO_FLOAT   : value->xFloat.value = (float) items[i].value; break;
		case DAO_DOUBLE  : value->xDouble.value = items[i].value; break;
		default: continue;
		}
		value->type = items[i].type;
		DaoNamespace_AddConst( self, & name, value, DAO_DATA_PUBLIC );
		i ++;
	}
}
void DaoNamespace_AddConstValue( DaoNamespace *self, const char *name, DaoValue *value )
{
	DString s = DString_WrapMBS( name );
	DaoNamespace_AddConst( self, & s, value, DAO_DATA_PUBLIC );
}
static void DaoTypeBase_Parents( DaoTypeBase *typer, DArray *parents )
{
	daoint i, k, n;
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
int DaoNamespace_SetupValues( DaoNamespace *self, DaoTypeBase *typer )
{
	daoint i, j, valCount;
	DArray *parents;
	DMap *values;
	DMap *supValues;
	DNode *it;

	if( typer->core == NULL ) return 0;
	if( typer->core->kernel && typer->core->kernel->values != NULL ) return 1;
	for(i=0; i<DAO_MAX_CDATA_SUPER; i++){
		if( typer->supers[i] == NULL ) break;
		DaoNamespace_SetupValues( self, typer->supers[i] );
	}
	valCount = 0;
	if( typer->numItems != NULL ){
		while( typer->numItems[ valCount ].name != NULL ) valCount ++;
	}

	DMutex_Lock( & mutex_values_setup );
	if( typer->core->kernel == NULL ){
		typer->core->kernel = DaoTypeKernel_New( typer );
		DArray_Append( self->auxData, typer->core->kernel );
	}
	if( typer->core->kernel->values == NULL ){
		DString defname = DString_WrapMBS( "default" );
		DaoDouble buf = {0,0,0,0,0,0.0};
		DaoValue *value = (DaoValue*) & buf;
		DaoType *abtype = typer->core->kernel->abtype;

		values = DHash_New( D_STRING, D_VALUE );
		if( abtype && abtype->value ) DMap_Insert( values, & defname, abtype->value );
		for(i=0; i<valCount; i++){
			DString name = DString_WrapMBS( typer->numItems[i].name );
			double dv = typer->numItems[i].value;
			value->type = typer->numItems[i].type;
			switch( value->type ){
			case DAO_INTEGER : value->xInteger.value = (int) dv; break;
			case DAO_FLOAT : value->xFloat.value = (float) dv; break;
			case DAO_DOUBLE : value->xDouble.value = dv; break;
			default : continue;
			}
			DMap_Insert( values, & name, value );
		}
		parents = DArray_New(0);
		DaoTypeBase_Parents( typer, parents );
		for(i=1; i<parents->size; i++){
			DaoTypeBase *sup = (DaoTypeBase*) parents->items.pVoid[i];
			supValues = sup->core->kernel->values;
			if( sup->numItems == NULL ) continue;
			for(j=0; sup->numItems[j].name!=NULL; j++){
				DString name = DString_WrapMBS( sup->numItems[j].name );
				it = DMap_Find( supValues, & name );
				if( it && DMap_Find( values, & name ) == NULL )
					DMap_Insert( values, it->key.pVoid, it->value.pVoid );
			}
		}
		DArray_Delete( parents );
		/* Set values field after it has been setup, for read safety in multithreading: */
		typer->core->kernel->values = values;
	}
	DMutex_Unlock( & mutex_values_setup );
	return 1;
}
void DaoMethods_Insert( DMap *methods, DaoRoutine *rout, DaoNamespace *ns, DaoType *host )
{
	DNode *node = MAP_Find( methods, rout->routName );
	if( node == NULL ){
		DMap_Insert( methods, rout->routName, rout );
	}else if( node->value.pRoutine->overloads ){
		DRoutines_Add( node->value.pRoutine->overloads, rout );
	}else{
		DaoRoutine *mroutine = DaoRoutines_New( ns, host, node->value.pRoutine );
		DRoutines_Add( mroutine->overloads, rout );
		GC_ShiftRC( mroutine, node->value.pValue );
		node->value.pVoid = mroutine;
	}
}
int DaoNamespace_SetupMethods( DaoNamespace *self, DaoTypeBase *typer )
{
	DaoParser *parser, *defparser;
	DaoRoutine *cur;
	DString *name1, *name2;
	DArray *parents;
	DMap *methods;
	DMap *supMethods;
	DNode *it;
	daoint i, k, size;

	assert( typer->core != NULL );
	if( typer->funcItems == NULL ) return 0;
	if( typer->core->kernel && typer->core->kernel->methods != NULL ) return 1;
	for(i=0; i<DAO_MAX_CDATA_SUPER; i++){
		if( typer->supers[i] == NULL ) break;
		DaoNamespace_SetupMethods( self, typer->supers[i] );
	}
	DMutex_Lock( & mutex_methods_setup );
	if( typer->core->kernel == NULL ){
		typer->core->kernel = DaoTypeKernel_New( typer );
		DArray_Append( self->auxData, typer->core->kernel );
	}
	if( typer->core->kernel->methods == NULL ){
		DaoType *hostype = typer->core->kernel->abtype;
		methods = DHash_New( D_STRING, D_VALUE );
		size = 0;
		name1 = DString_New(1);
		name2 = DString_New(1);
		DaoNamespace_InitConstEvalData( self );

		parser = DaoParser_New();
		parser->vmSpace = self->vmSpace;
		parser->nameSpace = self;
		parser->hostCdata = hostype;
		parser->defParser = defparser = DaoParser_New();
		defparser->vmSpace = self->vmSpace;
		defparser->nameSpace = self;
		defparser->hostCdata = hostype;
		defparser->routine = self->constEvalRoutine;

		if( typer->funcItems != NULL ){
			while( typer->funcItems[ size ].proto != NULL ) size ++;
		}

		for( i=0; i<size; i++ ){
			const char *proto = typer->funcItems[i].proto;
			cur = DaoNamespace_ParsePrototype( self, proto, parser );
			if( cur == NULL ){
				printf( "  In function: %s::%s\n", typer->name, proto );
				continue;
			}
			cur->pFunc = typer->funcItems[i].fpter;
			if( hostype && DString_EQ( cur->routName, hostype->name ) ){
				cur->attribs |= DAO_ROUT_INITOR;
			}
			DaoMethods_Insert( methods, cur, self, hostype );
		}
		parents = DArray_New(0);
		DaoTypeBase_Parents( typer, parents );
		for(i=1; i<parents->size; i++){
			DaoTypeBase *sup = (DaoTypeBase*) parents->items.pVoid[i];
			supMethods = sup->core->kernel->methods;
			for(it=DMap_First(supMethods); it; it=DMap_Next(supMethods, it)){
				if( it->value.pRoutine->overloads ){
					DRoutines *meta = (DRoutines*) it->value.pRoutine->overloads;
					for(k=0; k<meta->routines->size; k++){
						DaoRoutine *rout = meta->routines->items.pRoutine[k];
						/* skip constructor */
						if( rout->attribs & DAO_ROUT_INITOR ) continue;
						/* skip methods not defined in this parent type */
						if( rout->routHost != sup->core->kernel->abtype ) continue;
						DaoMethods_Insert( methods, rout, self, hostype );
					}
				}else{
					DaoRoutine *rout = it->value.pRoutine;
					/* skip constructor */
					if( rout->attribs & DAO_ROUT_INITOR ) continue;
					/* skip methods not defined in this parent type */
					if( rout->routHost != sup->core->kernel->abtype ) continue;
					DaoMethods_Insert( methods, rout, self, hostype );
				}
			}
		}
		DArray_Delete( parents );
		DaoParser_Delete( parser );
		DaoParser_Delete( defparser );

		assert( DAO_ROUT_MAIN < (1<<DVM_NOT) );
		for(i=DVM_NOT; i<=DVM_BITRIT; i++){
			DString_SetMBS( name1, daoBitBoolArithOpers[i-DVM_NOT] );
			if( DMap_Find( methods, name1 ) == NULL ) continue;
			typer->core->kernel->attribs |= DAO_OPER_OVERLOADED | (DVM_NOT<<(i-DVM_NOT+1));
		}
		DString_Delete( name1 );
		DString_Delete( name2 );
		/* Set methods field after it has been setup, for read safety in multithreading: */
		typer->core->kernel->methods = methods;
	}
	DMutex_Unlock( & mutex_methods_setup );
	return 1;
}
enum { DAO_DT_FAILED, DAO_DT_SCOPED, DAO_DT_UNSCOPED };

void DaoParser_Error( DaoParser *self, int code, DString *ext );
void DaoParser_Error2( DaoParser *self, int code, int m, int n, int single_line );
void DaoParser_PrintError( DaoParser *self, int line, int code, DString *ext );
int DaoParser_FindPairToken( DaoParser *self,  uchar_t lw, uchar_t rw, int start, int stop );
int DaoParser_ParseScopedName( DaoParser *self, DaoValue **scope, DaoValue **value, int i, int loc );
int DaoParser_ParseTemplateParams( DaoParser *self, int start, int end, DArray *holders, DArray *defaults, DString *name );
DaoType* DaoParser_ParseType( DaoParser *self, int start, int end, int *newpos, DArray *types );
DaoType* DaoParser_ParseTypeItems( DaoParser *self, int start, int end, DArray *types, int *co );
DaoType* DaoCdata_WrapType( DaoNamespace *ns, DaoTypeBase *typer, int opaque );
DaoType* DaoCdata_NewType( DaoTypeBase *typer );

static int DaoNS_ParseType( DaoNamespace *self, const char *name, DaoType *type, DaoType *type2, int isnew )
{
	DArray *types = NULL;
	DArray *defts = NULL;
	DString *string = NULL;
	DaoToken **tokens;
	DaoParser *parser = DaoParser_New();
	DaoValue *scope = NULL, *value = NULL;
	DTypeSpecTree *sptree = NULL;
	daoint i, k, n, ret = DAO_DT_UNSCOPED;

	DaoNamespace_InitConstEvalData( self );
	parser->vmSpace = self->vmSpace;
	parser->nameSpace = self;
	parser->routine = self->constEvalRoutine;
	if( ! DaoToken_Tokenize( parser->tokens, name, 0, 0, 0 ) ) goto Error;
	if( parser->tokens->size == 0 ) goto Error;
	tokens = parser->tokens->items.pToken;
	n = parser->tokens->size - 1;
	DArray_Clear( parser->errors );
	if( (k = DaoParser_ParseScopedName( parser, & scope, & value, 0, 0 )) <0 ) goto Error;
	if( k == 0 && n ==0 ) goto Finalize; /* single identifier name; */
	if( k == n ){
		DaoTypeCore *core;
		DString *name = tokens[k]->string;
		if( value != NULL ){
			DaoParser_Error2( parser, DAO_SYMBOL_WAS_DEFINED, k, k, 0 );
			goto Error;
		}else if( scope == NULL ){
			DaoParser_Error2( parser, DAO_UNDEFINED_SCOPE_NAME, k-2, k-2, 0 );
			goto Error;
		}
		if( isnew ){
			DString_Assign( type->name, name );
			DString_Assign( type2->name, name );
		}
		switch( scope->type ){
		case DAO_CTYPE :
			DaoNamespace_SetupValues( self, scope->xCdata.ctype->kernel->typer );
			core = scope->xCdata.ctype->kernel->core;
			if( core->kernel->values == NULL ) core->kernel->values = DHash_New( D_STRING, D_VALUE );
			DMap_Insert( core->kernel->values, name, type->aux );
			break;
		case DAO_CLASS :
			DaoClass_AddType( & scope->xClass, name, type );
			DaoClass_AddConst( & scope->xClass, name, type->aux, DAO_DATA_PUBLIC, -1 );
			break;
		case DAO_NAMESPACE :
			if( type->typer->core && type->typer->core->kernel ){
				/* For properly parsing methods (scope of types and default values): */
				GC_ShiftRC( scope, type->typer->core->kernel->nspace );
				type->typer->core->kernel->nspace = (DaoNamespace*) scope;
			}
			DaoNamespace_AddType( & scope->xNamespace, name, type2 );
			DaoNamespace_AddTypeConstant( & scope->xNamespace, name, type );
			break;
		default : DaoParser_Error2( parser, DAO_UNDEFINED_SCOPE_NAME, k-2, k-2, 0 ); goto Error;
		}
		DaoParser_Delete( parser );
		return DAO_DT_SCOPED;
	}
	ret = k ? DAO_DT_SCOPED : DAO_DT_UNSCOPED;
	if( type->tid != DAO_CTYPE ) goto Error;
	if( (value && value->type != DAO_CTYPE) || tokens[k+1]->type != DTOK_LT ) goto Error;
	if( DaoParser_FindPairToken( parser, DTOK_LT, DTOK_GT, k+1, -1 ) != (int)n ) goto Error;

	types = DArray_New(0);
	defts = DArray_New(0);
	string = DString_New(1);
	DArray_Clear( parser->errors );
	DaoParser_ParseTemplateParams( parser, k+2, n, types, defts, NULL );
	if( parser->errors->size ) goto Error;

	if( value == NULL ){
		DaoTypeKernel *kernel = type->typer->core->kernel;
		DaoType *cdata_type = DaoCdata_NewType( type->typer );
		DaoType *ctype_type = cdata_type->aux->xCdata.ctype;
		DString_Clear( cdata_type->name );
		for(i=0; i<=k; i++) DString_Append( cdata_type->name, tokens[i]->string );
		DString_Assign( ctype_type->name, cdata_type->name );
		DaoNS_ParseType( self, cdata_type->name->mbs, ctype_type, cdata_type, 1 );
		value = cdata_type->aux;

		GC_ShiftRC( kernel, ctype_type->kernel );
		GC_ShiftRC( kernel, cdata_type->kernel );
		ctype_type->kernel = kernel;
		cdata_type->kernel = kernel;
		cdata_type->cdatatype = type2->cdatatype;
		cdata_type->nested = DArray_New(D_VALUE);
		ctype_type->nested = DArray_New(D_VALUE);
		kernel->sptree = DTypeSpecTree_New();
		for(i=0; i<types->size; i++){
			DArray_Append( kernel->sptree->holders, types->items.pType[i] );
			DArray_Append( kernel->sptree->defaults, defts->items.pType[i] );
			if( kernel->sptree->defaults->items.pType[0] ){
				DArray_Append( cdata_type->nested, defts->items.pType[i] );
				DArray_Append( ctype_type->nested, defts->items.pType[i] );
			}
		}
	}
	sptree = value->xCdata.ctype->kernel->sptree;
	if( sptree == NULL ) goto Error;
	if( sptree->holders->size && types->size )
		if( DTypeSpecTree_Test( sptree, types ) == 0 ) goto Error;
	DTypeSpecTree_Add( sptree, types, type2 );

	while( k < parser->tokens->size ) DString_Append( string, tokens[k++]->string );
	if( isnew ){
		type->nested = DArray_New(D_VALUE);
		DArray_Assign( type->nested, types );
		type2->nested = DArray_Copy( type->nested );
		DString_Assign( type->name, string );
		DString_Assign( type2->name, string );
	}

Finalize:
	if( sptree == NULL && ret == DAO_DT_UNSCOPED ){
		if( string == NULL ){
			string = DString_New(1);
			DString_SetMBS( string, name );
		}
		DaoNamespace_AddType( self, string, type2 );
		DaoNamespace_AddTypeConstant( self, string, type );
	}
	DaoParser_Delete( parser );
	if( string ) DString_Delete( string );
	if( types ) DArray_Delete( types );
	if( defts ) DArray_Delete( defts );
	return ret;
Error:
	DaoParser_Error2( parser, DAO_INVALID_TYPE_FORM, 0, parser->tokens->size-1, 0 );
	DaoParser_PrintError( parser, 0, 0, NULL );
	DaoParser_Delete( parser );
	if( string ) DString_Delete( string );
	if( types ) DArray_Delete( types );
	if( defts ) DArray_Delete( defts );
	return DAO_DT_FAILED;
}
DaoType* DaoNamespace_TypeDefine( DaoNamespace *self, const char *old, const char *type )
{
	DaoNamespace *ns;
	DaoType *tp, *tp2;
	DString name = DString_WrapMBS( old );
	DString alias = DString_WrapMBS( type );
	DNode *node;
	int i;
	/* printf( "DaoNamespace_TypeDefine: %s %s\n", old, type ); */
	tp = DaoNamespace_FindType( self, & name );
	if( tp == NULL ) tp = DaoParser_ParseTypeName( old, self, NULL );
	if( tp == NULL ){
		printf( "type aliasing failed: %s to %s, source type is not found!\n", old, type );
		return NULL;
	}
	tp2 = DaoNamespace_FindType( self, & alias );
	if( tp2 == NULL ) tp2 = DaoParser_ParseTypeName( type, self, NULL );
	if( tp == tp2 ) return tp;
	/* printf( "ns = %p  tp = %p  name = %s\n", self, tp, type ); */

	/* Only allow overiding types defined in parent namespaces: */
	node = MAP_Find( self->abstypes, & alias );
	if( node != NULL ){
		printf( "type aliasing failed: %s to %s, target type was defined!\n", old, type );
		return NULL;
	}

	/*
	// Copy primitive types, so that it can be treated differently when necessary.
	// For example, a SQL module may alias "int" to "INT", "TINYINT", "SMALLINT"
	// and "INT_PRIMARY_KEY" etc., so that they can be handled differently in handling
	// table fields.
	//
	// Do not copy other non-primitive types, in particular, do not copy cdata types.
	// Otherwise, inheritance relationship will not be handled properly for the copied
	// types. If it is a template-like type, copying the type and using new template
	// parameter types may cause problems in type matching, or function specialization
	// if the function tries to specialize based on the types of template parameters.
	// 
	// To create a template-like alias to a template-like cdata type, it is only
	// necessary to add a specialization entry in the template cdata type.
	*/
	if( tp->tid == DAO_CDATA ) tp = tp->aux->xCtype.ctype;
	tp2 = tp;
	if( tp->tid && tp->tid < DAO_ARRAY ){
		tp = DaoType_Copy( tp );
		DString_SetMBS( tp->name, type );
	}
	if( DaoNS_ParseType( self, type, tp, tp, tp != tp2 ) == DAO_DT_FAILED ){
		printf( "type aliasing failed: %s to %s\n", old, type );
		GC_IncRC( tp );
		GC_DecRC( tp );
		return NULL;
	}
	return tp;
}

static DaoType* DaoNamespace_WrapType2( DaoNamespace *self, DaoTypeBase *typer, int opaque, DaoParser *parser )
{
	DaoParser *parser2 = parser;
	DaoType *ctype_type, *cdata_type;
	DaoCdataCore *hostCore;

	if( typer->core ) return typer->core->kernel->abtype;

	ctype_type = DaoCdata_WrapType( self, typer, opaque );
	cdata_type = typer->core->kernel->abtype;
	typer->core->kernel->attribs |= DAO_TYPER_PRIV_FREE;
	if( DaoNS_ParseType( self, typer->name, ctype_type, cdata_type, 1 ) == DAO_DT_FAILED ){
		printf( "type wrapping failed: %s\n", typer->name );
		return NULL;
	}
	//printf( "type wrapping: %s\n", typer->name );
	return cdata_type;
}
DaoType* DaoNamespace_WrapType( DaoNamespace *self, DaoTypeBase *typer, int opaque )
{
	return DaoNamespace_WrapType2( self, typer, opaque, NULL );
}
DaoType* DaoNamespace_SetupType( DaoNamespace *self, DaoTypeBase *typer )
{
	if( typer->core == NULL ) return NULL;
	DMutex_Lock( & mutex_values_setup ); // XXX
	if( typer->core->kernel == NULL ){
		typer->core->kernel = DaoTypeKernel_New( typer );
		typer->core->kernel->nspace = self;
		GC_IncRC( self );
		DArray_Append( self->auxData, typer->core->kernel );
	}
	DMutex_Unlock( & mutex_values_setup );
	return typer->core->kernel->abtype;
}
int DaoNamespace_WrapTypes( DaoNamespace *self, DaoTypeBase *typers[] )
{
	DaoParser *parser = DaoParser_New();
	int i, ec = 0;
	for(i=0; typers[i]; i++ ){
		ec += DaoNamespace_WrapType2( self, typers[i], 1, parser ) == NULL;
		/* e |= ( DaoNamespace_SetupValues( self, typers[i] ) == 0 ); */
	}
	/* if( setup ) return DaoNamespace_SetupTypes( self, typers ); */
	DaoParser_Delete( parser );
	return ec;
}
int DaoNamespace_TypeDefines( DaoNamespace *self, const char *alias[] )
{
	int i = 0, ec = 0;
	if( alias == NULL ) return 0;
	while( alias[i] && alias[i+1] ){
		ec += DaoNamespace_TypeDefine( self, alias[i], alias[i+1] ) == NULL;
		i += 2;
	}
	return ec;
}
int DaoNamespace_SetupTypes( DaoNamespace *self, DaoTypeBase *typers[] )
{
	int i, ec = 0;
	for(i=0; typers[i]; i++ ){
		ec += ( DaoNamespace_SetupMethods( self, typers[i] ) == 0 );
	}
	return ec;
}
DaoRoutine* DaoNamespace_MakeFunction( DaoNamespace *self, const char *proto, DaoParser *parser )
{
	DaoParser *old = parser;
	DaoParser *defparser = NULL;
	DaoRoutine *func;
	DaoValue *value;

	if( parser == NULL ){
		DaoNamespace_InitConstEvalData( self );
		parser = DaoParser_New();
		parser->vmSpace = self->vmSpace;
		parser->nameSpace = self;
		parser->defParser = defparser = DaoParser_New();
		defparser->vmSpace = self->vmSpace;
		defparser->nameSpace = self;
		defparser->routine = self->constEvalRoutine;
	}
	func = DaoNamespace_ParsePrototype( self, proto, parser );
	if( old == NULL ){
		DaoParser_Delete( parser );
		DaoParser_Delete( defparser );
	}
	if( func == NULL ) return NULL;
	value = DaoNamespace_GetData( self, func->routName );
	if( value && value->type == DAO_ROUTINE && value->xRoutine.overloads ){
		DRoutines_Add( value->xRoutine.overloads, func );
	}else{
		DaoNamespace_AddConst( self, func->routName, (DaoValue*) func, DAO_DATA_PUBLIC );
	}
	return func;
}
DaoRoutine* DaoNamespace_WrapFunction( DaoNamespace *self, DaoCFunction fptr, const char *proto )
{
	DaoRoutine *func = DaoNamespace_MakeFunction( self, proto, NULL );
	if( func == NULL ) return NULL;
	func->pFunc = fptr;
	return func;
}

int DaoNamespace_WrapFunctions( DaoNamespace *self, DaoFuncItem *items )
{
	DaoParser *defparser, *parser = DaoParser_New();
	DaoRoutine *func;
	int i = 0;
	int ec = 0;
	DaoNamespace_InitConstEvalData( self );
	parser->vmSpace = self->vmSpace;
	parser->nameSpace = self;
	parser->defParser = defparser = DaoParser_New();
	defparser->vmSpace = self->vmSpace;
	defparser->nameSpace = self;
	defparser->routine = self->constEvalRoutine;
	while( items[i].fpter != NULL ){
		func = DaoNamespace_MakeFunction( self, items[i].proto, parser );
		if( func ) func->pFunc = (DaoCFunction)items[i].fpter;
		ec += func == NULL;
		i ++;
	}
	DaoParser_Delete( parser );
	DaoParser_Delete( defparser );
	return ec;
}
int DaoNamespace_Load( DaoNamespace *self, const char *fname )
{
	DString *src;
	DaoVmSpace *vms = self->vmSpace;
	FILE *fin = fopen( fname, "r" );
	int ret;
	if( ! fin ){
		DaoStream_WriteMBS( vms->errorStream, "ERROR: can not open file \"" );
		DaoStream_WriteMBS( vms->errorStream, fname );
		DaoStream_WriteMBS( vms->errorStream, "\".\n" );
		return 0;
	}
	src = DString_New(1);
	DaoFile_ReadAll( fin, src, 1 );
	ret = DaoProcess_Eval( self->vmSpace->mainProcess, self, src, 1 );
	DString_Delete( src );
	return ret;
}
void DaoNamespace_SetOptions( DaoNamespace *self, int options )
{
	self->options = options;
}
int DaoNamespace_GetOptions( DaoNamespace *self )
{
	return self->options;
}


DaoTypeBase nsTyper=
{
	"namespace", & nsCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoNamespace_Delete, NULL
};

DaoNamespace* DaoNamespace_New( DaoVmSpace *vms, const char *nsname )
{
	DaoValue *value;
	DString *name = DString_New(1);
	DaoNamespace *self = (DaoNamespace*) dao_malloc( sizeof(DaoNamespace) );
	DaoValue_Init( self, DAO_NAMESPACE );
	self->trait |= DAO_VALUE_DELAYGC;
	self->vmSpace = vms;
	self->cstUser = 0;
	self->options = 0;
	self->mainRoutine = NULL;
	self->constants = DArray_New(D_VALUE);
	self->variables = DArray_New(D_VALUE);
	self->auxData = DArray_New(D_VALUE);
	self->namespaces = DArray_New(0);
	self->lookupTable = DHash_New(D_STRING,0);
	self->mainRoutines  = DArray_New(D_VALUE);
	self->definedRoutines = DArray_New(0);
	self->localMacros = DHash_New(D_STRING,D_VALUE);
	self->globalMacros = DHash_New(D_STRING,D_VALUE);
	self->abstypes = DHash_New(D_STRING,D_VALUE);
	self->moduleLoaders = DHash_New(D_STRING,0);
	self->codeInliners = DHash_New(D_STRING,0);
	self->argParams = DaoList_New();
	self->time = 0;
	self->file = DString_New(1);
	self->path = DString_New(1);
	self->name = DString_New(1);
	self->lang = DString_New(1);
	self->inputs = DString_New(1);
	self->sources = DArray_New(D_ARRAY);
	self->tokens = DHash_New(D_STRING,0);
	self->constEvalProcess = NULL;
	self->constEvalRoutine = NULL;

	DArray_Append( self->auxData, self->argParams );

	DString_SetMBS( self->lang, "dao" );
	DArray_Append( self->namespaces, self );

	DaoNamespace_SetName( self, nsname );
	DaoNamespace_AddConst( self, self->name, (DaoValue*) self, DAO_DATA_PUBLIC );

	DString_SetMBS( name, "none" ); 
	DaoNamespace_AddConst( self, name, dao_none_value, DAO_DATA_PUBLIC );

	DArray_Append( self->constants, DaoConstant_New( dao_none_value ) ); /* reserved for main */

	if( vms == NULL || vms->nsInternal == NULL ){
		DString_SetMBS( name, "io" ); 
		DaoNamespace_AddConst( self, name, (DaoValue*) vms->stdioStream, DAO_DATA_PUBLIC );

		DString_SetMBS( name, "exceptions" );
		value = (DaoValue*) DaoList_New();
		DaoNamespace_AddVariable( self, name, value, NULL, DAO_DATA_PUBLIC );
	}

	if( vms && vms->nsInternal ){
		DaoNamespace *ns = vms->nsInternal;
		DaoNamespace_AddConst( self, ns->name, (DaoValue*)ns, DAO_DATA_PUBLIC );
		DArray_Append( self->namespaces, ns );
		DaoNamespace_UpdateLookupTable( self );
	}
	DString_Delete( name );
	self->cstUser = self->constants->size;
	return self;
}
void DaoNamespace_Delete( DaoNamespace *self )
{
	/* printf( "DaoNamespace_Delete  %s\n", self->name->mbs ); */
	daoint i, j;
	for(i=0; i<self->sources->size; i++){
		DArray *array = self->sources->items.pArray[i];
		for(j=0; j<array->size; j++) array->items.pToken[j]->string = NULL;
	}

	DMap_Delete( self->lookupTable );
	DArray_Delete( self->constants );
	DArray_Delete( self->variables );
	DArray_Delete( self->auxData );

	/* no need for GC, because these namespaces are indirectly
	 * referenced through functions. */
	DArray_Delete( self->namespaces );

	DArray_Delete( self->mainRoutines );
	DArray_Delete( self->definedRoutines );
	DMap_Delete( self->localMacros );
	DMap_Delete( self->globalMacros );
	DMap_Delete( self->abstypes );
	DMap_Delete( self->moduleLoaders );
	DMap_Delete( self->codeInliners );
	DString_Delete( self->file );
	DString_Delete( self->path );
	DString_Delete( self->name );
	DString_Delete( self->lang );
	DString_Delete( self->inputs );
	DArray_Delete( self->sources );
	DMap_Delete( self->tokens );
	dao_free( self );
}
void DaoNamespace_InitConstEvalData( DaoNamespace *self )
{
	if( self->constEvalProcess ) return;
	self->constEvalProcess = DaoProcess_New( self->vmSpace );
	self->constEvalRoutine = DaoRoutine_New( self, NULL, 1 );
	self->constEvalRoutine->routType = dao_routine;
	self->constEvalProcess->activeNamespace = self;
	GC_IncRC( dao_routine );
	DaoProcess_InitTopFrame( self->constEvalProcess, self->constEvalRoutine, NULL );
	DaoProcess_SetActiveFrame( self->constEvalProcess, self->constEvalProcess->topFrame );
	self->constEvalRoutine->trait |= DAO_VALUE_CONST;
	self->constEvalProcess->trait |= DAO_VALUE_CONST;
	DArray_Append( self->auxData, (DaoValue*) self->constEvalRoutine );
	DArray_Append( self->auxData, (DaoValue*) self->constEvalProcess );
}
void DaoNamespace_SetName( DaoNamespace *self, const char *name )
{
	daoint i;
	DString_SetMBS( self->name, name );
	i = DString_RFindChar( self->name, '/', -1 );
	if( i != MAXSIZE ){
		DString_SetMBS( self->file, name + i + 1 );
		DString_SetDataMBS( self->path, name, i );
		i = DString_RFindChar( self->name, '.', -1 );
		if( i != MAXSIZE ) DString_SetMBS( self->lang, self->name->mbs + i + 1 );
	}else{
		DString_Clear( self->file );
		DString_Clear( self->path );
	}
}
int DaoNamespace_FindConst( DaoNamespace *self, DString *name )
{
	DNode *node = DMap_Find( self->lookupTable, name );
	if( node == NULL ) return -1;
	if( LOOKUP_ST( node->value.pInt ) != DAO_GLOBAL_CONSTANT ) return -1;
	return node->value.pInt;
}
int DaoNamespace_AddConst( DaoNamespace *self, DString *name, DaoValue *value, int pm )
{
	DaoValue *vdest;
	DaoConstant *dest;
	DaoRoutine *mroutine;
	DNode *node = MAP_Find( self->lookupTable, name );
	int isrout2, isrout = value->type == DAO_ROUTINE;
	daoint sto, pm2, up, id = 0;

	if( node && LOOKUP_UP( node->value.pInt ) ){ /* inherited data: */
		sto = LOOKUP_ST( node->value.pInt );
		pm2 = LOOKUP_PM( node->value.pInt );
		id = LOOKUP_ID( node->value.pInt );
		if( sto != DAO_GLOBAL_CONSTANT ){ /* override inherited variable: */
			DMap_EraseNode( self->lookupTable, node );
			return DaoNamespace_AddConst( self, name, value, pm );
		}
		node->value.pInt = LOOKUP_BIND( sto, pm2, 0, id );
		dest = self->constants->items.pConst[id];
		if( dest->value->type == DAO_ROUTINE && value->type == DAO_ROUTINE ){
			/* Add the inherited routine(s) for overloading: */
			DaoRoutine *routs = DaoRoutines_New( self, NULL, (DaoRoutine*) dest->value );
			DaoConstant *cst = DaoConstant_New( (DaoValue*) routs );
			GC_ShiftRC( cst, dest );
			self->constants->items.pConst[id] = cst;
			return DaoNamespace_AddConst( self, name, value, pm );
		}else{
			/* Add the new constant: */
			DaoConstant *cst = DaoConstant_New( value );
			GC_ShiftRC( cst, dest );
			self->constants->items.pConst[id] = cst;
			return node->value.pInt;
		}
	}else if( node ){
		sto = LOOKUP_ST( node->value.pInt );
		pm2 = LOOKUP_PM( node->value.pInt );
		id = LOOKUP_ID( node->value.pInt );
		if( sto != DAO_GLOBAL_CONSTANT ) return -1;
		dest = self->constants->items.pConst[id];
		vdest = dest->value;
		if( vdest->type != DAO_ROUTINE || value->type != DAO_ROUTINE ) return -1;
		if( pm > pm2 ) node->value.pInt = LOOKUP_BIND( sto, pm, 0, id );
		if( vdest->xRoutine.overloads == NULL || vdest->xRoutine.nameSpace != self ){
			/* Add individual entry for the existing function: */
			if( vdest->xRoutine.nameSpace == self ) DArray_Append( self->constants, dest );

			mroutine = DaoRoutines_New( self, NULL, (DaoRoutine*) dest->value );
			dest = DaoConstant_New( (DaoValue*) mroutine );
			dest->value->xBase.trait |= DAO_VALUE_CONST;
			GC_ShiftRC( dest, self->constants->items.pConst[id] );
			self->constants->items.pConst[id] = dest;
		}
		if( value->xRoutine.overloads ){
			DaoRoutines_Import( (DaoRoutine*) dest->value, value->xRoutine.overloads );
		}else{
			DRoutines_Add( dest->value->xRoutine.overloads, (DaoRoutine*) value );
			/* Add individual entry for the new function: */
			DArray_Append( self->constants, DaoConstant_New( value ) );
			value->xBase.trait |= DAO_VALUE_CONST;
		}
		return node->value.pInt;
	}else{
		DaoRoutine *rout = (DaoRoutine*) value;
		if( value->type == DAO_ROUTINE && rout->overloads && rout->nameSpace != self ){
			mroutine = DaoRoutines_New( self, NULL, rout );
			value = (DaoValue*) mroutine;
		}
		id = LOOKUP_BIND( DAO_GLOBAL_CONSTANT, pm, 0, self->constants->size );
		MAP_Insert( self->lookupTable, name, id ) ;
		DArray_Append( self->constants, (dest = DaoConstant_New( value )) );
		DaoValue_MarkConst( dest->value );
	}
	return id;
}
void DaoNamespace_SetConst( DaoNamespace *self, int index, DaoValue *value )
{
	DaoConstant *dest;
	daoint id = LOOKUP_ID( index );
	if( LOOKUP_ST( index ) != DAO_GLOBAL_CONSTANT ) return;
	if( id >= self->constants->size ) return;
	dest = self->constants->items.pConst[id];
	DaoValue_Copy( value, & dest->value );
	DaoValue_MarkConst( dest->value );
}
DaoValue* DaoNamespace_GetConst( DaoNamespace *self, int index )
{
	daoint st = LOOKUP_ST( index );
	daoint id = LOOKUP_ID( index );
	if( index < 0 ) return NULL;
	if( st != DAO_GLOBAL_CONSTANT ) return NULL;
	if( id >= self->constants->size ) return NULL;
	return self->constants->items.pConst[id]->value;
}
int DaoNamespace_FindVariable( DaoNamespace *self, DString *name )
{
	DNode *node = DMap_Find( self->lookupTable, name );
	if( node == NULL ) return -1;
	if( LOOKUP_ST( node->value.pInt ) != DAO_GLOBAL_VARIABLE ) return -1;
	return node->value.pInt;
}
int DaoNamespace_AddVariable( DaoNamespace *self, DString *name, DaoValue *value, DaoType *tp, int pm )
{
	DaoType *abtp = DaoNamespace_GetType( self, value );
	DNode *node = MAP_Find( self->lookupTable, name );
	DaoVariable *dest;
	daoint id = 0;

	if( abtp == NULL ) abtp = dao_type_udf;
	if( tp && value && DaoType_MatchValue( tp, value, NULL ) ==0 ) return -1;
	if( tp == NULL ) tp = abtp;
	if( value == NULL && tp ) value = tp->value;
	if( node && LOOKUP_UP( node->value.pInt ) ){ /* overriding */
		DMap_EraseNode( self->lookupTable, node );
		DaoNamespace_AddVariable( self, name, value, tp, pm );
		node = MAP_Find( self->lookupTable, name );
		return node->value.pInt;
	}else if( node ){
		id = LOOKUP_ID( node->value.pInt );
		if( LOOKUP_ST( node->value.pInt ) != DAO_GLOBAL_VARIABLE ) return -1;
		assert( id < self->variables->size );
		dest = self->variables->items.pVar[id];
		if( tp ){
			GC_ShiftRC( tp, dest->dtype );
			dest->dtype = tp;
		}
		if( DaoValue_Move( value, & dest->value, dest->dtype ) ==0 ) return -1;
		id = node->value.pInt;
	}else{
		id = LOOKUP_BIND( DAO_GLOBAL_VARIABLE, pm, 0, self->variables->size );
		MAP_Insert( self->lookupTable, name, id ) ;
		DArray_Append( self->variables, DaoVariable_New( value, tp ) );
	}
	return id;
}
int DaoNamespace_SetVariable( DaoNamespace *self, int index, DaoValue *value )
{
	DaoVariable *dest;
	daoint id = LOOKUP_ID( index );
	if( LOOKUP_ST( index ) != DAO_GLOBAL_VARIABLE ) return 0;
	if( id >= self->variables->size ) return 0;
	dest = self->variables->items.pVar[id];
	return DaoValue_Move( value, & dest->value, dest->dtype );
}
DaoValue* DaoNamespace_GetVariable( DaoNamespace *self, int index )
{
	daoint st = LOOKUP_ST( index );
	daoint id = LOOKUP_ID( index );
	if( st != DAO_GLOBAL_VARIABLE ) return NULL;
	if( id >= self->variables->size ) return NULL;
	return self->variables->items.pVar[id]->value;
}
DaoType* DaoNamespace_GetVariableType( DaoNamespace *self, int index )
{
	daoint st = LOOKUP_ST( index );
	daoint id = LOOKUP_ID( index );
	if( st != DAO_GLOBAL_VARIABLE ) return NULL;
	if( id >= self->variables->size ) return NULL;
	return self->variables->items.pVar[id]->dtype;
}
void DaoNamespace_SetData( DaoNamespace *self, DString *name, DaoValue *value )
{
	DNode *node = MAP_Find( self->lookupTable, name );
	if( node ){
		daoint id = node->value.pInt;
		daoint st = LOOKUP_ST( id );
		if( st == DAO_GLOBAL_CONSTANT ) DaoNamespace_SetConst( self, id, value );
		if( st == DAO_GLOBAL_VARIABLE ) DaoNamespace_SetVariable( self, id, value );
		return;
	}
	DaoNamespace_AddVariable( self, name, value, NULL, DAO_DATA_PROTECTED );
}
DaoValue* DaoNamespace_GetData( DaoNamespace *self, DString *name )
{
	DNode *node = MAP_Find( self->lookupTable, name );
	if( node == NULL ) return NULL;
	return DaoNamespace_GetValue( self, node->value.pInt );
}
DaoValue* DaoNamespace_GetValue( DaoNamespace *self, daoint index )
{
	daoint st = LOOKUP_ST( index );
	if( st == DAO_GLOBAL_CONSTANT ) return DaoNamespace_GetConst( self, index );
	if( st == DAO_GLOBAL_VARIABLE ) return DaoNamespace_GetVariable( self, index );
	return NULL;
}
DaoClass* DaoNamespace_FindClass( DaoNamespace *self, DString *name )
{
	int id = DaoNamespace_FindConst( self, name );
	DaoValue *value = DaoNamespace_GetConst( self, id );
	if( value && value->type == DAO_CLASS ) return & value->xClass;
	return NULL;
}
DaoNamespace* DaoNamespace_FindNamespace( DaoNamespace *self, DString *name )
{
	int id = DaoNamespace_FindConst( self, name );
	DaoValue *value = DaoNamespace_GetConst( self, id );
	if( value && value->type == DAO_NAMESPACE ) return & value->xNamespace;
	return NULL;
}
void DaoNamespace_AddMacro( DaoNamespace *self, DString *lang, DString *name, DaoMacro *macro )
{
	DMap *macros = lang ? self->globalMacros : self->localMacros;
	DString *combo = lang ? DString_Copy( lang ) : name;
	DNode *node;
	if( lang ){
		DString_AppendMBS( combo, ":" );
		DString_Append( combo, name );
	}
	node = MAP_Find( macros, combo );
	if( node == NULL ){
		MAP_Insert( macros, combo, macro );
	}else{
		DaoMacro *m2 = (DaoMacro*) node->value.pVoid;
		DArray_Append( m2->macroList, macro );
	}
	if( lang ) DString_Delete( combo );
}
int DaoNamespace_CyclicParent( DaoNamespace *self, DaoNamespace *parent )
{
	daoint i;
	if( parent == self ) return 1;
	for(i=0; i<self->namespaces->size; i++)
		if( self->namespaces->items.pNS[i] == parent ) return 0;
	for(i=1; i<parent->namespaces->size; i++){
		if( DaoNamespace_CyclicParent( self, parent->namespaces->items.pNS[i] ) ) return 1;
	}
	return 0;
}
static void DaoNS_ImportRoutine( DaoNamespace *self, DString *name, DaoRoutine *routine, int pm )
{
	DNode *search = MAP_Find( self->lookupTable, name );
	if( search == NULL ){
		DaoNamespace_AddConst( self, name, (DaoValue*)routine, pm );
	}else if( LOOKUP_ST( search->value.pInt ) == DAO_GLOBAL_CONSTANT ){
		DaoRoutine *routine2 = (DaoRoutine*) DaoNamespace_GetConst( self, search->value.pInt );
		if( routine2->type != DAO_ROUTINE ) return;
		if( routine2->overloads ){
			DRoutines_Add( routine2->overloads, routine );
		}else{
			DaoRoutine *routs = DaoRoutines_New( self, NULL, routine );
			DRoutines_Add( routs->overloads, routine2 );
			DaoValue_MarkConst( (DaoValue*) routine2 );
			/* Add individual entry for the existing function: */
			DArray_Append( self->constants, DaoConstant_New( (DaoValue*) routine2 ) );
			DaoNamespace_SetConst( self, search->value.pInt, (DaoValue*) routs );
		}
	}
}
void DaoNamespace_UpdateLookupTable( DaoNamespace *self )
{
	DArray *namespaces = DArray_Copy( self->namespaces );
	DArray *upindexes = DArray_New(0);
	DNode *it, *search;
	daoint i, j, k, pm, st, up, id;

	for(i=0; i<namespaces->size; i++) DArray_Append( upindexes, i );
	for(i=1; i<namespaces->size; i++){
		DaoNamespace *ns = namespaces->items.pNS[i];
		DaoNamespace_UpdateLookupTable( ns );
		for(j=1; j<ns->namespaces->size; j++){
			DArray_Append( namespaces, ns->namespaces->items.pVoid[j] );
			DArray_Append( upindexes, i );
		}
		k = upindexes->items.pInt[i];
		for(it=DMap_First( ns->lookupTable ); it; it=DMap_Next(ns->lookupTable,it) ){
			DaoValue *value = DaoNamespace_GetValue( ns, it->value.pInt );
			DString *name = it->key.pString;
			up = LOOKUP_UP( it->value.pInt );
			pm = LOOKUP_PM( it->value.pInt );
			st = LOOKUP_ST( it->value.pInt );
			id = LOOKUP_ID( it->value.pInt );
			if( up || pm != DAO_DATA_PUBLIC || value == NULL ) continue;

			search = MAP_Find( self->lookupTable, name );
			if( search && value->type == DAO_ROUTINE ){
				DaoNS_ImportRoutine( self, name, (DaoRoutine*)value, pm );
				continue;
			}
			if( search ) continue;
			if( st == DAO_GLOBAL_CONSTANT ){
				DaoValue *value = ns->constants->items.pConst[id]->value;
				if( value->type == DAO_ROUTINE && value->xRoutine.overloads ){
					/* To skip the private methods: */
					DaoNamespace_AddConst( self, name, value, pm );
					continue;
				}
			}
			if( st == DAO_GLOBAL_CONSTANT ){
				MAP_Insert( self->lookupTable, name, LOOKUP_BIND( st, pm, k, self->constants->size ) );
				DArray_Append( self->constants, ns->constants->items.pConst[id] );
			}else{
				MAP_Insert( self->lookupTable, name, LOOKUP_BIND( st, pm, k, self->variables->size ) );
				DArray_Append( self->variables, ns->variables->items.pVar[id] );
			}
		}
	}
	DArray_Delete( namespaces );
	DArray_Delete( upindexes );
}
int DaoNamespace_AddParent( DaoNamespace *self, DaoNamespace *parent )
{
	daoint i;
	if( parent == self ) return 0;
	if( DaoNamespace_CyclicParent( self, parent ) ) return 0;
	for(i=0; i<self->namespaces->size; i++){
		if( self->namespaces->items.pNS[i] == parent ){
			DaoNamespace_UpdateLookupTable( self );
			return 1;
		}
	}
	parent->trait |= DAO_VALUE_CONST;
	DArray_Append( self->auxData, parent );
	DArray_Append( self->namespaces, parent );
	DaoNamespace_UpdateLookupTable( self );
	return 1;
}

static DaoMacro* DaoNamespace_FindMacro2( DaoNamespace *self, DString *lang, DString *name )
{
	daoint i, n = self->namespaces->size;
	DString *combo = DString_Copy( lang );
	DNode *node;

	DString_AppendMBS( combo, ":" );
	DString_Append( combo, name );
	node = MAP_Find( self->globalMacros, combo );
	DString_Delete( combo );
	if( node ) return (DaoMacro*) node->value.pVoid;
	for(i=1; i<n; i++){
		DaoNamespace *ns = self->namespaces->items.pNS[i];
		DaoMacro *macro = DaoNamespace_FindMacro2( ns, lang, name );
		if( macro ) return macro;
	}
	return NULL;
}
DaoMacro* DaoNamespace_FindMacro( DaoNamespace *self, DString *lang, DString *name )
{
	daoint i, n = self->namespaces->size;
	DString *combo = NULL;
	DNode *node;

	/* check local macros that are not associated with any language name: */
	if( (node = MAP_Find( self->localMacros, name )) ) return (DaoMacro*) node->value.pVoid;

	combo = DString_Copy( lang );
	DString_AppendMBS( combo, ":" );
	DString_Append( combo, name );
	if( (node = MAP_Find( self->localMacros, combo )) ) goto ReturnMacro;
	if( (node = MAP_Find( self->globalMacros, combo )) ) goto ReturnMacro;
	/* Stop searching upstream namespaces if the current is .dao file: */
	if( strcmp( self->lang->mbs, "dao" ) ==0 ) goto ReturnNull;
	for(i=1; i<n; i++){
		DaoNamespace *ns = self->namespaces->items.pNS[i];
		DaoMacro *macro = DaoNamespace_FindMacro2( ns, lang, name );
		if( macro == NULL ) continue;
		MAP_Insert( self->globalMacros, combo, macro );
		DString_Delete( combo );
		return macro;
	}
ReturnNull:
	DString_Delete( combo );
	return NULL;
ReturnMacro:
	DString_Delete( combo );
	return (DaoMacro*) node->value.pVoid;
}
void DaoNamespace_ImportMacro( DaoNamespace *self, DString *lang )
{
	DString *name2 = DString_New(1);
	DNode *it;
	daoint i, pos;
	for(i=0; i<self->namespaces->size; i++){
		DaoNamespace *ns = self->namespaces->items.pNS[i];
		for(it=DMap_First( ns->globalMacros ); it; it=DMap_Next(ns->globalMacros,it) ){
			DString *name = it->key.pString;
			pos = DString_Find( name, lang, 0 );
			if( pos != 0 || name->mbs[lang->size] != ':' ) continue;
			/* Add as local macro: */
			DString_SetDataMBS( name2, name->mbs + lang->size + 1, name->size - lang->size - 1 );
			DaoNamespace_AddMacro( self, NULL, name2, (DaoMacro*) it->value.pVoid );
		}
	}
	DString_Delete( name2 );
}
void DaoNamespace_AddModuleLoader( DaoNamespace *self, const char *name, DaoModuleLoader fp )
{
	DString mbs = DString_WrapMBS( name );
	DMap_Insert( self->moduleLoaders, & mbs, (void*)fp );
}
void DaoNamespace_AddCodeInliner( DaoNamespace *self, const char *name, DaoCodeInliner fp )
{
	DString mbs = DString_WrapMBS( name );
	DMap_Insert( self->codeInliners, & mbs, (void*)fp );
}
DaoModuleLoader DaoNamespace_FindModuleLoader( DaoNamespace *self, DString *name )
{
	int i, n = self->namespaces->size;
	DNode *node = MAP_Find( self->moduleLoaders, name );
	if( node ) return (DaoModuleLoader) node->value.pVoid;
	for(i=1; i<n; i++){
		DaoNamespace *ns = self->namespaces->items.pNS[i];
		DaoModuleLoader loader = DaoNamespace_FindModuleLoader( ns, name );
		if( loader ) return loader;
	}
	return NULL;
}
DaoCodeInliner DaoNamespace_FindCodeInliner( DaoNamespace *self, DString *name )
{
	int i, n = self->namespaces->size;
	DNode *node = MAP_Find( self->codeInliners, name );
	if( node ) return (DaoCodeInliner) node->value.pVoid;
	for(i=1; i<n; i++){
		DaoNamespace *ns = self->namespaces->items.pNS[i];
		DaoCodeInliner inliner = DaoNamespace_FindCodeInliner( ns, name );
		if( inliner ) return inliner;
	}
	return NULL;
}

DaoType* DaoNamespace_FindType( DaoNamespace *self, DString *name )
{
	DNode *node;
	DaoType *type = NULL;
	int i, n = self->namespaces->size;

	DMutex_Lock( & mutex_type_map );
	node = MAP_Find( self->abstypes, name );
	if( node ) type = node->value.pType;
	DMutex_Unlock( & mutex_type_map );
	if( type ) return type;

	for(i=1; i<n; i++){
		DaoNamespace *ns = self->namespaces->items.pNS[i];
		DaoType *type = DaoNamespace_FindType( ns, name );
		if( type == NULL ) continue;
		return type;
	}
	return NULL;
}
DaoType* DaoNamespace_AddType( DaoNamespace *self, DString *name, DaoType *type )
{
	DNode *node;
	DMutex_Lock( & mutex_type_map );
	node = MAP_Find( self->abstypes, name );
	if( node == NULL ){
		MAP_Insert( self->abstypes, name, type );
	}else{
		DArray_Append( self->auxData, type );
		type = node->value.pType;
	}
	DMutex_Unlock( & mutex_type_map );
	return type;
}
void DaoNamespace_AddTypeConstant( DaoNamespace *self, DString *name, DaoType *tp )
{
	int id = DaoNamespace_FindConst( self, name );
	if( id >=0 ) return;
	if( tp->aux && (tp->tid >= DAO_OBJECT && tp->tid <= DAO_CTYPE) ){
		DaoNamespace_AddConst( self, name, tp->aux, DAO_DATA_PUBLIC );
	}else if( tp->tid != DAO_VALTYPE && tp->tid != DAO_THT ){
		DaoNamespace_AddConst( self, name, (DaoValue*) tp, DAO_DATA_PUBLIC );
	}
}

DaoType *simpleTypes[ DAO_ARRAY ] = { 0, 0, 0, 0, 0, 0, 0, 0 };

DaoType* DaoNamespace_GetType( DaoNamespace *self, DaoValue *p )
{
	DNode *node;
	DArray *nested = NULL;
	DString *mbs;
	DaoRoutine *rout;
	DaoType *abtp = NULL;
	DaoType *itp = (DaoType*) p;
	DaoTuple *tuple = (DaoTuple*) p;
	DaoNameValue *nameva = (DaoNameValue*) p;
	DaoList *list = (DaoList*) p;
	DaoMap *map = (DaoMap*) p;
	DaoArray *array = (DaoArray*) p;
	DaoCdata *cdata = (DaoCdata*) p;
	DaoProcess *vmp = (DaoProcess*) p;
	DaoTypeBase *typer;
	int i, tid;

	if( p == NULL ) return NULL;
	if( p->type == DAO_TYPE && itp->tid == DAO_TYPE ) return itp;
	tid = p->type;

	switch( p->type ){
	case DAO_NONE :
		if( p->xBase.subtype == DAO_ANY ){
			abtp = dao_type_any;
		}else{
			abtp = DaoNamespace_MakeValueType( self, dao_none_value );
		}
		break;
	case DAO_INTEGER : case DAO_FLOAT : case DAO_DOUBLE :
	case DAO_COMPLEX : case DAO_LONG : case DAO_STRING : 
		abtp = simpleTypes[ p->type ];
		if( abtp ) break;
		abtp = DaoNamespace_MakeType( self, coreTypeNames[p->type], p->type, NULL, NULL, 0 );
		simpleTypes[ p->type ] = abtp;
		GC_IncRC( abtp );
		break;
	case DAO_ENUM : 
		abtp = p->xEnum.etype;
		if( abtp ) break;
		abtp = simpleTypes[ p->type ];
		if( abtp ) break;
		abtp = DaoNamespace_MakeType( self, coreTypeNames[p->type], p->type, NULL, NULL, 0 );
		simpleTypes[ p->type ] = abtp;
		GC_IncRC( abtp );
		break;
	case DAO_LIST :
		abtp = list->unitype; break;
	case DAO_MAP :
		abtp = map->unitype; break;
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY :
		abtp = dao_array_types[ array->etype ]; break;
#endif
	case DAO_OBJECT :
		abtp = p->xObject.defClass->objType; break;
	case DAO_CLASS :
		abtp = p->xClass.clsType; break;
	case DAO_CTYPE :
	case DAO_CDATA :
		abtp = p->xCdata.ctype; break;
	case DAO_ROUTINE :
		abtp = p->xRoutine.routType;
		break;
	case DAO_PAR_NAMED :
		abtp = nameva->unitype; break;
	case DAO_TUPLE :
		abtp = tuple->unitype; break;
	case DAO_FUTURE :
		abtp = ((DaoFuture*)p)->unitype; break;
	case DAO_PROCESS :
		abtp = vmp->abtype; break;
	case DAO_INTERFACE :
		abtp = p->xInterface.abtype; break;
	default : break;
	}
	if( abtp ){
		//abtp->typer = DaoValue_GetTyper( p );
		return abtp;
	}

	mbs = DString_New(1);
	if( p->type <= DAO_STREAM ){
		DString_SetMBS( mbs, coreTypeNames[p->type] );
		if( p->type == DAO_LIST ){
			nested = DArray_New(0);
			if( list->items.size ==0 ){
				DString_AppendMBS( mbs, "<?>" );
				DArray_Append( nested, dao_type_udf );
			}else{
				itp = dao_type_any;
				DString_AppendMBS( mbs, "<any>" );
				DArray_Append( nested, itp );
			}  
		}else if( p->type == DAO_MAP ){
			nested = DArray_New(0);
			if( map->items->size ==0 ){
				DString_AppendMBS( mbs, "<?,?>" );
				DArray_Append( nested, dao_type_udf );
				DArray_Append( nested, dao_type_udf );
			}else{
				itp = dao_type_any;
				DString_AppendMBS( mbs, "<any,any>" );
				DArray_Append( nested, itp );
				DArray_Append( nested, itp );
			}
#ifdef DAO_WITH_NUMARRAY
		}else if( p->type == DAO_ARRAY ){
			nested = DArray_New(0);
			if( array->size ==0 ){
				DString_AppendMBS( mbs, "<?>" );
				DArray_Append( nested, dao_type_udf );
			}else if( array->etype == DAO_INTEGER ){
				itp = DaoNamespace_MakeType( self, "int", DAO_INTEGER, 0,0,0 );
				DString_AppendMBS( mbs, "<int>" );
				DArray_Append( nested, itp );
			}else if( array->etype == DAO_FLOAT ){
				itp = DaoNamespace_MakeType( self, "float", DAO_FLOAT, 0,0,0 );
				DString_AppendMBS( mbs, "<float>" );
				DArray_Append( nested, itp );
			}else if( array->etype == DAO_DOUBLE ){
				itp = DaoNamespace_MakeType( self, "double", DAO_DOUBLE, 0,0,0 );
				DString_AppendMBS( mbs, "<double>" );
				DArray_Append( nested, itp );
			}else{
				itp = DaoNamespace_MakeType( self, "complex", DAO_COMPLEX, 0,0,0 );
				DString_AppendMBS( mbs, "<complex>" );
				DArray_Append( nested, itp );
			}
#endif
		}else if( p->type == DAO_TUPLE ){
			DString_SetMBS( mbs, "tuple<" );
			nested = DArray_New(0);
			for(i=0; i<tuple->size; i++){
				itp = DaoNamespace_GetType( self, tuple->items[i] );
				DArray_Append( nested, itp );
				DString_Append( mbs, itp->name );
				if( i+1 < tuple->size ) DString_AppendMBS( mbs, "," );
			}
			DString_AppendMBS( mbs, ">" );
		}
		abtp = DaoNamespace_FindType( self, mbs );
		if( abtp == NULL ){
			abtp = DaoType_New( mbs->mbs, tid, NULL, nested );
			if( p->type && p->type < DAO_ARRAY ){
				simpleTypes[ p->type ] = abtp;
				GC_IncRC( abtp );
			}
			abtp = DaoNamespace_AddType( self, abtp->name, abtp );
		}
#if 1
		switch( p->type ){
		case DAO_LIST :
			list->unitype = abtp;
			GC_IncRC( abtp );
			break;
		case DAO_MAP :
			map->unitype = abtp;
			GC_IncRC( abtp );
			break;
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
	}else if( p->type == DAO_TYPE ){
		DString_SetMBS( mbs, "type<" );
		nested = DArray_New(0);
		DArray_Append( nested, itp );
		DString_Append( mbs, itp->name );
		DString_AppendMBS( mbs, ">" );
		abtp = DaoNamespace_FindType( self, mbs );
		if( abtp == NULL ){
			abtp = DaoType_New( mbs->mbs, p->type, NULL, nested );
			abtp = DaoNamespace_AddType( self, abtp->name, abtp );
		}
	}else{
		typer = DaoValue_GetTyper( p );
		DString_SetMBS( mbs, typer->name );
		abtp = DaoNamespace_FindType( self, mbs );
		if( abtp == NULL ){
			abtp = DaoType_New( typer->name, p->type, NULL, NULL );
			abtp = DaoNamespace_AddType( self, abtp->name, abtp );
		}
	}
	/* abtp might be rout->routType, which might be NULL,
	 * in case rout is DaoNamespace.constEvalRoutine */
	//XXX if( abtp && abtp->typer ==NULL ) abtp->typer = DaoValue_GetTyper( p );
	DString_Delete( mbs );
	if( nested ) DArray_Delete( nested );
	return abtp;
}
DaoType* DaoNamespace_MakeType( DaoNamespace *self, const char *name, 
		uint_t tid, DaoValue *pb, DaoType *nest[], int N )
{
	DaoClass *klass;
	DaoType *any = NULL;
	DaoType *tp;
	DNode   *node;
	DString *mbs = DString_New(1);
	DArray  *nstd = NULL;
	int i, n = strlen( name );
	int attrib = tid >> 16;

	tid = tid & 0xffff;
	if( tid != DAO_ANY ) any = dao_type_any;

	DString_SetMBS( mbs, name );
	if( tid == DAO_CODEBLOCK ) DString_Clear( mbs );
	if( N > 0 || tid == DAO_CODEBLOCK ){
		nstd = DArray_New(0);
		if( n || tid != DAO_VARIANT ) DString_AppendChar( mbs, '<' );
		for(i=0; i<N; i++){
			DaoType *it = nest[i];
			if( tid == DAO_TUPLE && it->tid == DAO_PAR_DEFAULT )
				it = DaoNamespace_MakeType( self, it->fname->mbs, DAO_PAR_NAMED, it->aux, NULL, 0 );

			if( i ) DString_AppendChar( mbs, tid == DAO_VARIANT ? '|' : ',' );
			DString_Append( mbs, it->name );
			DArray_Append( nstd, it );
		}
		if( attrib & DAO_TYPE_VARIADIC ){
			if( N ) DString_AppendChar( mbs, ',' );
			DString_AppendMBS( mbs, "..." );
		}
		if( (tid == DAO_ROUTINE || tid == DAO_CODEBLOCK) && pb && pb->type == DAO_TYPE ){
			DString_AppendMBS( mbs, "=>" );
			if( attrib & DAO_TYPE_COROUTINE ) DString_AppendChar( mbs, '[' );
			DString_Append( mbs, ((DaoType*)pb)->name );
			if( attrib & DAO_TYPE_COROUTINE ) DString_AppendChar( mbs, ']' );
		}
		if( n || tid != DAO_VARIANT ) DString_AppendChar( mbs, '>' );
	}else if( tid == DAO_LIST || tid == DAO_ARRAY ){
		nstd = DArray_New(0);
		DString_AppendMBS( mbs, "<any>" );
		DArray_Append( nstd, any );
	}else if( tid == DAO_MAP ){
		nstd = DArray_New(0);
		DString_AppendMBS( mbs, "<any,any>" );
		DArray_Append( nstd, any );
		DArray_Append( nstd, any );
	}else if( tid == DAO_TUPLE ){
		DString_AppendMBS( mbs, "<...>" );
		attrib |= DAO_TYPE_VARIADIC;
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
	}else if( (tid == DAO_ROUTINE || tid == DAO_CODEBLOCK) && pb && pb->type == DAO_TYPE ){
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
	if( tid == DAO_CODEBLOCK ){
		mbs->mbs[0] = '[';
		mbs->mbs[mbs->size-1] = ']';
	}
	tp = DaoNamespace_FindType( self, mbs );
	if( tp == NULL ){
		if( tid == DAO_PAR_NAMED || tid == DAO_PAR_DEFAULT ) DString_SetMBS( mbs, name );
		tp = DaoType_New( mbs->mbs, tid, pb, nstd );
		tp->attrib |= attrib;
		if( attrib & DAO_TYPE_VARIADIC ) tp->variadic = 1;
		tp = DaoNamespace_AddType( self, tp->name, tp );
	}
Finalizing:
	DString_Delete( mbs );
	if( nstd ) DArray_Delete( nstd );
	return tp;
}
DaoType* DaoNamespace_MakeRoutType( DaoNamespace *self, DaoType *routype,
		DaoValue *vals[], DaoType *types[], DaoType *retp )
{
	DaoType *tp, *tp2, *abtp;
	DString *fname = NULL;
	DNode *node;
	daoint i, ch = 0;

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
			tp2 = & tp->aux->xType;
		}
		if( tp2 && tp2->tid == DAO_UDT ){
			if( vals && vals[i] ){
				tp2 = DaoNamespace_GetType( self, vals[i] );
			}else if( types && types[i] ){
				tp2 = types[i];
			}
		}
		/* XXX typing DString_AppendMBS( abtp->name, tp ? tp->name->mbs : "..." ); */
		if( tp2 != tp && tp2 != & tp->aux->xType ){
			fname = tp->fname;
			tp = DaoType_New( fname->mbs, tp->tid, (DaoValue*) tp2, NULL );
			DString_AppendChar( tp->name, ch );
			DString_Append( tp->name, tp2->name );
			if( tp->fname ) DString_Assign( tp->fname, fname );
			else tp->fname = DString_Copy( fname );
		}
		DString_Append( abtp->name, tp->name );
		DArray_Append( abtp->nested, tp );
	}
	tp = retp ? retp : & routype->aux->xType;
	if( tp ){
		DString_AppendMBS( abtp->name, "=>" );
		if( routype->attrib & DAO_TYPE_COROUTINE ) DString_AppendChar( abtp->name, '[' );
		DString_Append( abtp->name, tp->name );
		if( routype->attrib & DAO_TYPE_COROUTINE ) DString_AppendChar( abtp->name, ']' );
	}
	DString_AppendMBS( abtp->name, ">" );
	GC_ShiftRC( tp, abtp->aux );
	abtp->aux = (DaoValue*) tp;
	if( routype->cbtype ){
		DMap *defs = DHash_New(0,0);
		DaoType_MatchTo( abtp, routype, defs );
		tp = DaoType_DefineTypes( routype->cbtype, self, defs );
		GC_ShiftRC( tp, abtp->cbtype );
		abtp->cbtype = tp;
		DMap_Delete( defs );
		DString_Append( abtp->name, abtp->cbtype->name );
	}
	tp = DaoNamespace_FindType( self, abtp->name );
	if( tp ){
		DaoType_Delete( abtp );
		return tp;
	}
	DaoType_CheckAttributes( abtp );
	DaoType_InitDefault( abtp );
	DaoNamespace_AddType( self, abtp->name, abtp );
	return abtp;
}

DaoRoutine* DaoNamespace_ParsePrototype( DaoNamespace *self, const char *proto, DaoParser *parser )
{
	DaoRoutine *func = DaoRoutine_New( self, NULL, 0 );
	DaoParser *defparser;
	int key = DKEY_OPERATOR;
	int optok = 0;

	assert( parser != NULL );
	assert( parser->defParser != NULL );
	defparser = parser->defParser;

	GC_IncRC( parser->hostCdata );
	func->routHost = parser->hostCdata;
	if( ! DaoToken_Tokenize( defparser->tokens, proto, 0, 0, 0 ) ) goto Error;
	if( defparser->tokens->size < 3 ) goto Error;
	if( (optok = defparser->tokens->items.pToken[0]->name == DKEY_OPERATOR) == 0 ){
		if( defparser->tokens->items.pToken[0]->type == DTOK_IDENTIFIER 
				&& defparser->tokens->items.pToken[1]->type == DTOK_LB ) key = 0;
	}
	DArray_Clear( defparser->partoks );

	parser->routine = (DaoRoutine*) func; /* safe to parse params only */
	if( DaoParser_ParsePrototype( defparser, parser, key, optok ) < 0 ){
		DaoParser_PrintError( defparser, 0, 0, NULL );
		goto Error;
	}
	return func;
Error:
	printf( "Function wrapping failed for %s\n", proto );
	DaoRoutine_Delete( func );
	return NULL;
}
/* symbols should be comma or semicolon delimited string */
DaoType* DaoNamespace_MakeEnumType( DaoNamespace *self, const char *symbols )
{
	DaoType *type;
	DString *key, *name = DString_New(1);
	int n = strlen( symbols );
	int i, k = 0, t1 = 0, t2 = 0;

	DString_SetMBS( name, "enum<" );
	DString_AppendMBS( name, symbols );
	DString_AppendChar( name, '>' );
	type = DaoNamespace_FindType( self, name );
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
	DaoNamespace_AddType( self, name, type );
	DString_Delete( name );
	DString_Delete( key );
	return (t1&t2) ==0 ? type : NULL;
}
DaoType* DaoNamespace_SymbolTypeAdd( DaoNamespace *self, DaoType *t1, DaoType *t2, int *value )
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
	type = DaoNamespace_FindType( self, name );
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
		DaoNamespace_AddType( self, name, type );
	}
	DString_Delete( name );
	return type;
}
DaoType* DaoNamespace_SymbolTypeSub( DaoNamespace *self, DaoType *t1, DaoType *t2, int *value )
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
	type = DaoNamespace_FindType( self, name );
	if( type == NULL ){
		type = DaoType_New( name->mbs, DAO_ENUM, NULL, NULL );
		type->flagtype = count > 1;
		type->mapNames = mapNames = DMap_New(D_STRING,0);
		*value = type->flagtype;
		for(node=DMap_First(names1);node;node=DMap_Next(names1,node)){
			if( DMap_Find( names2, node->key.pVoid ) ) continue;
			*value |= (1<<mapNames->size);
			MAP_Insert( mapNames, node->key.pVoid, 1<<mapNames->size );
		}
		DaoNamespace_AddType( self, name, type );
	}
	DString_Delete( name );
	return type;
}
DaoType* DaoNamespace_MakeValueType( DaoNamespace *self, DaoValue *value )
{
	DaoType *type;
	DString *name;
	if( value->type >= DAO_ARRAY ) return NULL;
	name = DString_New(1);
	DaoValue_GetString( value, name );
	if( value->type == DAO_STRING ){
		DString_InsertChar( name, '\'', 0 );
		DString_AppendChar( name, '\'' );
	}
	if( name->size ==0 && value->type ==0 ) DString_SetMBS( name, "none" );
	type = DaoNamespace_MakeType( self->vmSpace->nsInternal, name->mbs, DAO_VALTYPE, 0,0,0 );
	DaoValue_Copy( value, & type->aux );
	DString_Delete( name );
	return type;
}
DaoType* DaoNamespace_MakePairType( DaoNamespace *self, DaoType *first, DaoType *second )
{
	DaoType *types[2] = {NULL, NULL};
	DaoType *noneType = DaoNamespace_MakeValueType( self, dao_none_value );
	if( first == NULL ) first = noneType;
	if( second == NULL ) second = noneType;
	types[0] = DaoNamespace_MakeType( self, "first", DAO_PAR_NAMED, (DaoValue*)first, 0, 0 );
	types[1] = DaoNamespace_MakeType( self, "second", DAO_PAR_NAMED, (DaoValue*)second, 0, 0 );
	return DaoNamespace_MakeType( self, "tuple", DAO_TUPLE, NULL, types, 2 );
}
DaoType* DaoNamespace_MakePairValueType( DaoNamespace *self, DaoValue *first, DaoValue *second )
{
	DaoType *tp1, *tp2;
	tp1 = DaoNamespace_MakeValueType( self, first );
	tp2 = DaoNamespace_MakeValueType( self, second );
	return DaoNamespace_MakePairType( self, tp1, tp2 );
}
DaoTuple* DaoNamespace_MakePair( DaoNamespace *self, DaoValue *first, DaoValue *second )
{
	DaoTuple *tuple = DaoTuple_New(2);
	DaoType *type1 = DaoNamespace_MakeValueType( self, first );
	DaoType *type2 = DaoNamespace_MakeValueType( self, second );
	tuple->unitype = DaoNamespace_MakePairType( self, type1, type2 );
	GC_IncRC( tuple->unitype );
	DaoValue_Copy( first, & tuple->items[0] );
	DaoValue_Copy( second, & tuple->items[1] );
	tuple->subtype = DAO_PAIR;
	return tuple;
}
DaoValue* DaoValue_FindAuxMethod( DaoValue *self, DString *name, DaoNamespace *nspace )
{
	int i;
	DaoValue *meth = DaoNamespace_GetConst( nspace, DaoNamespace_FindConst( nspace, name ) );
	if( meth == NULL || meth->type != DAO_ROUTINE ) return NULL;
	if( meth->type == DAO_ROUTINE && meth->xRoutine.overloads ){
		DRoutines *routs = meth->xRoutine.overloads;
		DParamNode *param;
		if( routs->mtree == NULL ) return NULL;
		for(param=routs->mtree->first; param; param=param->next){
			if( param->type && DaoType_MatchValue( param->type, self, NULL ) ) return meth;
		}
	}else if( meth->xRoutine.attribs & DAO_ROUT_PARSELF ){
		DaoType *type = meth->xRoutine.routType->nested->items.pType[0];
		if( DaoType_MatchValue( (DaoType*) type->aux, self, NULL ) ) return meth;
	}
	return NULL;
}
DaoValue* DaoType_FindAuxMethod( DaoType *self, DString *name, DaoNamespace *nspace )
{
	int i;
	DaoValue *meth = DaoNamespace_GetConst( nspace, DaoNamespace_FindConst( nspace, name ) );
	if( meth == NULL || meth->type != DAO_ROUTINE ) return NULL;
	if( meth->type == DAO_ROUTINE && meth->xRoutine.overloads ){
		DRoutines *routs = meth->xRoutine.overloads;
		DParamNode *param;
		if( routs->mtree == NULL ) return NULL;
		for(param=routs->mtree->first; param; param=param->next){
			if( param->type && DaoType_MatchTo( self, param->type, NULL ) ) return meth;
		}
	}else if( meth->xRoutine.attribs & DAO_ROUT_PARSELF ){
		DaoType *type = meth->xRoutine.routType->nested->items.pType[0];
		if( DaoType_MatchTo( self, (DaoType*) type->aux, NULL ) ) return meth;
	}
	return NULL;
}

