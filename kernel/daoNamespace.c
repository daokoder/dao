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
#include"daoRegex.h"
#include"daoValue.h"


/*
// Need separated mutexes for values and methods setup.
// Otherwise, a mutex deadlock may occur if values setup
// is triggered by methods setup.
*/
DMutex mutex_values_setup;
DMutex mutex_methods_setup;
DMutex mutex_type_map;

static DaoRoutine* DaoNamespace_ParseSignature( DaoNamespace *self, const char *proto, DaoParser *parser, DaoParser *defparser );


DaoNamespace* DaoNamespace_New( DaoVmSpace *vms, const char *nsname )
{
	DaoNamespace *self = (DaoNamespace*) dao_calloc( 1, sizeof(DaoNamespace) );
	DString *name = DString_New();
	DaoTuple *tuple;
	DaoValue *value;
	DaoType *type;
	DNode *it;

	DaoValue_Init( self, DAO_NAMESPACE );
	//printf( "DaoNamespace_New: %p %s\n", self, nsname );
	GC_IncRC( vms );
	self->vmSpace = vms;
	self->trait |= DAO_VALUE_DELAYGC;
	self->constants = DList_New( DAO_DATA_VALUE );
	self->variables = DList_New( DAO_DATA_VALUE );
	self->auxData = DList_New( DAO_DATA_VALUE );
	self->namespaces = DList_New(0);
	self->lookupTable = DHash_New( DAO_DATA_STRING, 0 );
	self->abstypes = DHash_New( DAO_DATA_STRING, DAO_DATA_VALUE );
	self->file = DString_New();
	self->path = DString_New();
	self->name = DString_New();
	self->lang = DString_New();
	self->inputs = DString_New();
	self->sources = DList_New( DAO_DATA_LIST );

	DString_SetChars( self->lang, "dao" );
	DList_Append( self->namespaces, self );

	DaoNamespace_SetName( self, nsname );
	DaoNamespace_AddConst( self, self->name, (DaoValue*) self, DAO_PERM_PUBLIC );

	DaoNamespace_AddConstValue( self, "__main__", dao_none_value ); /* DAO_STD_CONST_MAIN */
	DaoNamespace_AddConstValue( self, "none",  dao_none_value );  /* DAO_STD_CONST_NONE; */
	DaoNamespace_AddConstValue( self, "false", dao_false_value ); /* DAO_STD_CONST_FALSE; */
	DaoNamespace_AddConstValue( self, "true",  dao_true_value );  /* DAO_STD_CONST_TRUE; */

	/* Mainly for simplifying constant folding: */
	type = DaoNamespace_MakeRangeType( self, vms->typeNone, vms->typeNone );
	tuple = DaoTuple_Create( type, 2, 1 );
	tuple->subtype = DAO_RANGE;  /* DAO_STD_CONST_RANGE; */
	DList_Append( self->constants, DaoConstant_New( (DaoValue*) tuple, DAO_GLOBAL_CONSTANT ) );

	self->nstype = DaoNamespace_MakeType( self, "namespace", DAO_NAMESPACE, (DaoValue*)self, 0, 0 );

	if( vms->daoNamespace && !(vms->options & DAO_OPTION_SANDBOX) ){
		DaoNamespace *ns = vms->daoNamespace;
		DaoNamespace_AddConst( self, ns->name, (DaoValue*)ns, DAO_PERM_PUBLIC );
		DList_Append( self->namespaces, ns );
		DaoNamespace_UpdateLookupTable( self );
	}
	for(it=DMap_First(vms->nsPlugins); it!=NULL; it=DMap_Next(vms->nsPlugins,it)){
		DList_Append( self->namespaces, (DaoNamespace*) it->value.pValue );
	}
	DaoNamespace_UpdateLookupTable( self );

	DString_Delete( name );
	self->cstUser = self->constants->size;
#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogNew( (DaoValue*) self );
#endif
	return self;
}

void DaoNamespace_Delete( DaoNamespace *self )
{
	/* printf( "DaoNamespace_Delete  %s\n", self->name->chars ); */

	//printf( "DaoNamespace_Delete: %p %s\n", self, self->name->chars );

#ifdef DAO_USE_GC_LOGGER
	DaoObjectLogger_LogDelete( (DaoValue*) self );
#endif
	DMap_Delete( self->lookupTable );
	DList_Delete( self->constants );
	DList_Delete( self->variables );
	DList_Delete( self->auxData );

	/* no need for GC, because these namespaces are indirectly
	 * referenced through functions. */
	DList_Delete( self->namespaces );

	DMap_Delete( self->abstypes );
	DString_Delete( self->file );
	DString_Delete( self->path );
	DString_Delete( self->name );
	DString_Delete( self->lang );
	DString_Delete( self->inputs );
	DList_Delete( self->sources );
	GC_DecRC( self->vmSpace );
	dao_free( self );
}

void DaoNamespace_SetName( DaoNamespace *self, const char *name )
{
	daoint i;
	DString_SetChars( self->name, name );
	i = DString_RFindChar( self->name, '/', -1 );
	if( i != DAO_NULLPOS ){
		DString_SetChars( self->file, name + i + 1 );
		DString_SetBytes( self->path, name, i + 1 );
		i = DString_RFindChar( self->name, '.', -1 );
		if( i != DAO_NULLPOS ) DString_SetChars( self->lang, self->name->chars + i + 1 );
	}else{
		DString_Clear( self->file );
		DString_Clear( self->path );
	}
}

DaoVmSpace* DaoNamespace_GetVmSpace( DaoNamespace *self )
{
	return self->vmSpace;
}

DaoNamespace* DaoNamespace_GetNamespace( DaoNamespace *self, const char *name )
{
	DaoNamespace *ns;
	DString mbs = DString_WrapChars( name );
	ns = DaoNamespace_FindNamespace( self, & mbs );
	if( ns == NULL ){
		ns = DaoNamespace_New( self->vmSpace, name );
		DString_InsertChars( ns->name, "::", 0, 0, -1 );
		DString_Insert( ns->name, self->name, 0, 0, -1 );
		DList_Append( ns->namespaces, self );
		DaoNamespace_AddConst( self, & mbs, (DaoValue*)ns, DAO_PERM_PUBLIC );
		DList_Append( ns->auxData, self ); /* for GC */
	}
	return ns;
}

void DaoNamespace_AddValue( DaoNamespace *self, const char *s, DaoValue *v, const char *t )
{
	DaoType *type = NULL;
	DString name = DString_WrapChars( s );
	if( t && strlen( t ) >0 ){
		type = DaoParser_ParseTypeName( t, self, NULL ); /* XXX warn */
	}
	DaoNamespace_AddVariable( self, & name, v, type, DAO_PERM_PUBLIC );
}

DaoValue* DaoNamespace_FindData( DaoNamespace *self, const char *name )
{
	DString s = DString_WrapChars( name );
	return DaoNamespace_GetData( self, & s );
}

void DaoNamespace_AddConstNumbers( DaoNamespace *self, DaoNumberEntry *items )
{
	DaoValue buf = {0};
	DaoValue *value = (DaoValue*) & buf;
	int i = 0;
	memset( value, 0, sizeof(DaoValue) );
	while( items[i].name != NULL ){
		DString name = DString_WrapChars( items[i].name );
		switch( items[i].type ){
		case DAO_BOOLEAN : value->xBoolean.value = (dao_boolean) items[i].value; break;
		case DAO_INTEGER : value->xInteger.value = (dao_integer) items[i].value; break;
		case DAO_FLOAT   : value->xFloat.value = items[i].value; break;
		default: continue;
		}
		value->type = items[i].type;
		DaoNamespace_AddConst( self, & name, value, DAO_PERM_PUBLIC );
		i ++;
	}
}

void DaoNamespace_AddConstValue( DaoNamespace *self, const char *name, DaoValue *value )
{
	DString s = DString_WrapChars( name );
	DaoNamespace_AddConst( self, & s, value, DAO_PERM_PUBLIC );
}

enum { DAO_DT_FAILED, DAO_DT_SCOPED, DAO_DT_UNSCOPED };

void DaoParser_Error( DaoParser *self, int code, DString *ext );
void DaoParser_Error2( DaoParser *self, int code, int m, int n, int single_line );
void DaoParser_PrintError( DaoParser *self );
int DaoParser_FindPairToken( DaoParser *self,  uchar_t lw, uchar_t rw, int start, int stop );
int DaoParser_ParseTemplateParams( DaoParser *self, int start, int end, DList *holders, DList *defaults, DString *name );
DaoType* DaoParser_ParseTypeItems( DaoParser *self, int start, int end, DList *types, int *co );

int DaoParser_ParseMaybeScopeConst( DaoParser *self, DaoValue **scope, DaoValue **value, int start, int stop, int type );

static void DaoValue_AddType( DaoValue *self, DString *name, DaoType *type )
{
	DaoType *type2 = type;
	DaoValue *cst = (DaoValue*) type;
	DaoTypeKernel *kernel;

	if( type->tid == DAO_CTYPE ) type2 = type->aux->xCtype.valueType;
	if( type->tid >= DAO_OBJECT && type->tid <= DAO_INTERFACE ) cst = type->aux;
	switch( self->type ){
	case DAO_CTYPE :
		kernel = self->xCtype.valueType->kernel;
		DaoNamespace_SetupValues( kernel->nspace, self->xCtype.valueType->core );
		if( kernel->values == NULL ){
			kernel->values = DHash_New( DAO_DATA_STRING, DAO_DATA_VALUE );
		}
		DMap_Insert( kernel->values, name, cst );
		break;
	case DAO_CLASS :
		DaoClass_AddConst( & self->xClass, name, cst, DAO_PERM_PUBLIC );
		break;
	case DAO_NAMESPACE :
		if( type->kernel ){
			/* For properly parsing methods (self of types and default values): */
			GC_Assign( & type->kernel->nspace, self );
		}
		DaoNamespace_AddType( & self->xNamespace, name, type2 );
		DaoNamespace_AddTypeConstant( & self->xNamespace, name, type );
		break;
	}
}

static int DaoNS_ParseType( DaoNamespace *self, const char *name, DaoType *type, DaoType *type2, int isnew )
{
	DList *types = NULL;
	DList *defts = NULL;
	DString *string = NULL;
	DaoToken **tokens;
	DaoParser *parser = DaoVmSpace_AcquireParser( self->vmSpace );
	DaoTypeKernel *kernel = type->kernel;
	DaoValue *scope = NULL, *value = NULL;
	DaoTypeTree *sptree = NULL;
	daoint i, k, n, tid, ret = DAO_DT_UNSCOPED;

	parser->vmSpace = self->vmSpace;
	parser->nameSpace = self;
	DaoParser_InitConstEvaluator( parser );
	parser->routine = parser->evaluator.routine;
	parser->evalMode |= DAO_CONST_EVAL_GETVALUE;

	if( ! DaoLexer_Tokenize( parser->lexer, name, 0 ) ) goto Error;
	tokens = parser->tokens->items.pToken;
	n = parser->tokens->size - 1;

	if( parser->tokens->size == 0 ) goto Error;
	DList_Clear( parser->errors );
	parser->evaluator.process->nodebug = 1;
	k = DaoParser_ParseMaybeScopeConst( parser, &scope, &value, 0, 0, DAO_EXPRLIST_SCOPE );
	parser->evaluator.process->nodebug = 0;
	if( k < 0 ) goto Error;
	if( k == 0 && n ==0 ) goto Finalize; /* single identifier name; */
	if( scope && (tid=scope->type) != DAO_CTYPE && tid != DAO_CLASS && tid != DAO_NAMESPACE ){
		goto Error;
	}
	if( k == n ){
		DString *name = & tokens[k]->string;
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
		DaoValue_AddType( scope, name, type );
		DaoVmSpace_ReleaseParser( self->vmSpace, parser );
		return DAO_DT_SCOPED;
	}
	ret = k ? DAO_DT_SCOPED : DAO_DT_UNSCOPED;
	if( type->tid != DAO_CTYPE && type->tid != DAO_ARRAY && type->tid != DAO_LIST && type->tid != DAO_MAP ) goto Error;
	if( (value && value->type != DAO_CTYPE) || tokens[k+1]->type != DTOK_LT ) goto Error;
	if( DaoParser_FindPairToken( parser, DTOK_LT, DTOK_GT, k+1, -1 ) != (int)n ) goto Error;

	types = DList_New(0);
	defts = DList_New(0);
	string = DString_New();
	DList_Clear( parser->errors );
	DaoParser_ParseTemplateParams( parser, k+2, n, types, defts, NULL );
	if( parser->errors->size ) goto Error;

	type->args = DList_New( DAO_DATA_VALUE );
	if( type2 != type ) type2->args = DList_New( DAO_DATA_VALUE );
	for(i=0; i<types->size; i++){
		DList_Append( type->args, types->items.pType[i] );
		if( type2 != type ) DList_Append( type2->args, types->items.pType[i] );
	}
	if( isnew ){
		DString_Assign( type->name, & tokens[k]->string );
		DString_AppendChar( type->name, '<' );
		for(i=0; i<types->size; i++){
			if( i ) DString_AppendChar( type->name, ',' );
			DString_Append( type->name, types->items.pType[i]->name );
		}
		DString_AppendChar( type->name, '>' );
		DString_Assign( type2->name, type->name );
	}

	/*
	// Declaration: type name declared in the core structure;
	// Current: the current types (namely the paremeter "type" and "type2");
	// Template: the type that hosts type specialziation data structure;
	// Alias: the type that can be accessed with the template name;
	//
	// CASE 1:
	// Declaration:                TypeName<>
	// Current:                    TypeName<>
	// Template:                   TypeName<>
	// Alias:        TypeName  =>  TypeName<>
	//
	// CASE 2:
	// Declaration:                TypeName<@TypeHolder=DefaultType,...>
	// Current:                    TypeName<@TypeHolder,...>
	// Template:                   TypeName<@TypeHolder,...>
	// Alias:        TypeName  =>  TypeName<DefaultType,...>
	//
	// CASE 3:
	// Declaration:                TypeName<@TypeHolder,...>
	// Current:                    TypeName<@TypeHolder,...>
	// Template:                   TypeName<@TypeHolder,...>
	// Alias:        TypeName  =>  TypeName<@TypeHolder,...>
	//
	// CASE 4:
	// Declaration:                TypeName<ConcreteType,...>
	// Current:                    TypeName<ConcreteType,...>
	// Template:                   TypeName<>, must have been declared!
	// Alias:        TypeName  =>  TypeName<>
	//
	// Note: @TypeHolder can be a variant type holder: @TypeHolder<Type1|Type2...>;
	*/
	if( types->size ){
		DString *name = & tokens[k]->string;
		DaoType *it = types->items.pType[0];
		DaoType *aux = (DaoType*) it->aux;
		if( it->tid == DAO_VARIANT && it->aux && it->aux->type == DAO_TYPE ) it = aux;
		if( it->tid != DAO_THT ){ /* CASE 4: ConcreteType; */
			int id = DaoNamespace_FindConst( self, name );
			value = DaoNamespace_GetConst( self, id );
			if( value == NULL ) goto Error;
		}else{
			kernel->attribs |= DAO_TYPEKERNEL_TEMPLATE;
		}
	}
	if( value == NULL ){
		DaoType *alias = type2;
		DString *name = & tokens[k]->string;

		if( scope == NULL ) scope = (DaoValue*) self;

		kernel->sptree = DaoTypeTree_New();
		for(i=0; i<types->size; i++){
			DList_Append( kernel->sptree->holders, types->items.pType[i] );
			DList_Append( kernel->sptree->defaults, defts->items.pType[i] );
		}

		/* CASE 2: */
		if( defts->size && defts->items.pType[0] )
			alias = DaoType_Specialize( type, defts->items.pType, defts->size, self );
		DaoValue_AddType( scope, name, alias );

	}else{
		sptree = value->xCtype.valueType->kernel->sptree;
		if( sptree == NULL ) goto Error;
		if( sptree->holders->size && types->size )
			if( DaoTypeTree_Test( sptree, types->items.pType, types->size ) == 0 ) goto Error;
		DaoTypeTree_Add( sptree, types->items.pType, types->size, type2 );
	}


Finalize:
	if( sptree == NULL && ret == DAO_DT_UNSCOPED ){
		if( string == NULL ){
			string = DString_New();
			DString_SetChars( string, name );
		}
		DaoNamespace_AddType( self, string, type2 );
		DaoNamespace_AddTypeConstant( self, string, type );
	}
	DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	if( string ) DString_Delete( string );
	if( types ) DList_Delete( types );
	if( defts ) DList_Delete( defts );
	return ret;
Error:
	DaoParser_Error2( parser, DAO_INVALID_TYPE_FORM, 0, parser->tokens->size-1, 0 );
	DaoParser_PrintError( parser );
	DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	if( string ) DString_Delete( string );
	if( types ) DList_Delete( types );
	if( defts ) DList_Delete( defts );
	return DAO_DT_FAILED;
}
void DaoParser_PushLevel( DaoParser *self );
void DaoParser_PopLevel( DaoParser *self );
DaoType* DaoNamespace_DefineType( DaoNamespace *self, const char *type, const char *alias )
{
	DaoNamespace *ns;
	DaoType *tp, *tp2, *tht = NULL;
	DaoStream *stream = self->vmSpace->errorStream;
	DString name = DString_WrapChars( type );
	DString alias2 = DString_WrapChars( alias );
	DNode *node;
	int i = 0, id, recursive = 0;
	/* printf( "DaoNamespace_TypeDefine: %s %s\n", type, alias ); */
	tp = DaoNamespace_FindType( self, & name );
	if( tp == NULL ){
		DaoParser *parser = DaoVmSpace_AcquireParser( self->vmSpace );
		if( ! DaoLexer_Tokenize( parser->lexer, type, DAO_LEX_ESCAPE ) ) goto DoneSourceType;
		parser->nameSpace = self;
		DaoParser_InitConstEvaluator( parser );
		parser->routine = parser->evaluator.routine;

		if( alias != NULL ){
			DaoParser_PushLevel( parser );
			tht = DaoType_New( self, alias, DAO_THT, NULL, NULL );
			id = LOOKUP_BIND_LC( parser->routine->routConsts->value->size );
			MAP_Insert( parser->lookupTables->items.pMap[ parser->lexLevel ], & alias2, id );
			DaoRoutine_AddConstant( parser->routine, (DaoValue*) tht ); 
			id = tht->refCount;
		}

		tp = DaoParser_ParseType( parser, 0, parser->tokens->size-1, &i, NULL );
		if( i < parser->tokens->size && tp ) tp = NULL;
		if( alias != NULL ){
			recursive = tht->refCount > id;
			DaoParser_PopLevel( parser );
		}

DoneSourceType:
		DaoVmSpace_ReleaseParser( self->vmSpace, parser );
		if( tp == NULL ){
			DaoStream_WriteChars( stream, "ERROR: in DaoNamespace_DefineType(), type \"" );
			DaoStream_WriteChars( stream, type );
			DaoStream_WriteChars( stream, "\" is not valid!\n" );
			return NULL;
		}
	}
	if( alias == NULL ) return tp;

	tp2 = DaoNamespace_FindType( self, & alias2 );
	if( tp2 == NULL ) tp2 = DaoParser_ParseTypeName( alias, self, NULL );
	if( tp == tp2 ) return tp;
	/* printf( "ns = %p  tp = %p  name = %s\n", self, tp, alias ); */

	/* Only allow overiding types defined in parent namespaces: */
	if( DaoNamespace_FindType( self, & alias2 ) != NULL ){
		DaoStream_WriteChars( stream, "ERROR: in DaoNamespace_DefineType(), type \"" );
		DaoStream_WriteChars( stream, alias );
		DaoStream_WriteChars( stream, "\" was defined!\n" );
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
	if( tp->tid >= DAO_CSTRUCT && tp->tid <= DAO_CDATA ) tp = tp->aux->xCtype.valueType;
	tp2 = tp;
	if( (tp->tid && tp->tid <= DAO_TUPLE) || tp->tid == DAO_VARIANT ){
		tp = DaoType_Copy( tp );
		DString_SetChars( tp->name, alias );
	}
	if( recursive ){
		tp->recursive = 1; 
		DaoType_SetupRecursive( tp, tht, tp );
	}    
	if( DaoNS_ParseType( self, alias, tp, tp, tp != tp2 ) == DAO_DT_FAILED ){
		DaoStream_WriteChars( stream, "ERROR: in DaoNamespace_DefineType(), type aliasing from \"" );
		DaoStream_WriteChars( stream, type );
		DaoStream_WriteChars( stream, "\" to \"" );
		DaoStream_WriteChars( stream, alias );
		DaoStream_WriteChars( stream, "\" failed!\n" );
		GC_IncRC( tp );
		GC_DecRC( tp );
		return NULL;
	}
	return tp;
}

static DaoType* DaoNamespace_MakeCdataType( DaoNamespace *self, DaoTypeCore *core, int tid )
{
	DaoTypeKernel *kernel = DaoTypeKernel_New( core );
	DaoCtype *ctype = DaoCtype_New( self, core, tid );
	DaoType *cdata_type = ctype->valueType;
	DaoType *ctype_type = ctype->classType;
	int i;

	GC_IncRC( self );
	GC_IncRC( cdata_type );
	kernel->nspace = self;
	kernel->abtype = cdata_type;
	GC_Assign( & ctype_type->kernel, kernel );
	GC_Assign( & cdata_type->kernel, kernel );

	DaoVmSpace_AddKernel( self->vmSpace, core, kernel );

	for(i=0; i<sizeof(core->bases); i++){
		DaoTypeCore *base = core->bases[i];
		DaoTypeKernel *basekern = DaoVmSpace_GetKernel( self->vmSpace, base );
		if( base == NULL ) break;
		if( basekern == NULL ){
			DaoGC_TryDelete( (DaoValue*) ctype );
			printf( "Parent type is not wrapped (successfully): %s\n", core->name );
			return NULL;
		}
		if( basekern->abtype->tid != cdata_type->tid ){
			DaoGC_TryDelete( (DaoValue*) ctype );
			printf( "Invalid parent type: %s\n", core->name );
			return NULL;
		}
		if( ctype_type->bases == NULL ) ctype_type->bases = DList_New( DAO_DATA_VALUE );
		if( cdata_type->bases == NULL ) cdata_type->bases = DList_New( DAO_DATA_VALUE );
		DList_Append( ctype_type->bases, basekern->abtype->aux->xCtype.classType );
		DList_Append( cdata_type->bases, basekern->abtype );
	}

	return ctype_type;
}

static DaoType* DaoNamespace_WrapType2( DaoNamespace *self, DaoTypeCore *core, int tid, int options )
{
	DaoType *ctype_type, *cdata_type;
	DaoTypeKernel *kernel = DaoVmSpace_GetKernel( self->vmSpace, core );

	if( kernel != NULL ) return kernel->abtype;

	ctype_type = DaoNamespace_MakeCdataType( self, core, tid );
	cdata_type = ctype_type->kernel->abtype;
	if( options & DAO_CTYPE_INVAR ) cdata_type->aux->xCtype.attribs |= DAO_CLS_INVAR;
	if( options & DAO_CTYPE_UNITHREAD ) cdata_type->aux->xCtype.attribs |= DAO_CLS_UNITHREAD;
	ctype_type->kernel->SetupValues = DaoNamespace_SetupValues;
	ctype_type->kernel->SetupMethods = DaoNamespace_SetupMethods;
	if( DaoNS_ParseType( self, core->name, ctype_type, cdata_type, 1 ) == DAO_DT_FAILED ){
		GC_IncRC( ctype_type );
		GC_DecRC( ctype_type );
		printf( "type wrapping failed: %s from %s\n", core->name, self->name->chars );
		return NULL;
	}
	DString_SetChars( cdata_type->aux->xCtype.name, cdata_type->name->chars );
	return cdata_type;
}

DaoType* DaoNamespace_WrapType( DaoNamespace *self, DaoTypeCore *core, int tid, int options )
{
	return DaoNamespace_WrapType2( self, core, tid, options );
}

DaoType* DaoNamespace_WrapGenericType( DaoNamespace *self, DaoTypeCore *core, int tid )
{
	DaoTypeKernel *kernel = DaoVmSpace_GetKernel( self->vmSpace, core );
	DaoType *type;

	if( kernel != NULL ) return kernel->abtype;

	kernel = DaoTypeKernel_New( core );
	type = DaoType_New( self, core->name, tid, NULL, NULL );

	DaoVmSpace_AddKernel( self->vmSpace, core, kernel );

	GC_IncRC( self );
	GC_IncRC( type );
	kernel->nspace = self;
	kernel->abtype = type;
	GC_Assign( & type->kernel, kernel );
	kernel->SetupValues = DaoNamespace_SetupValues;
	kernel->SetupMethods = DaoNamespace_SetupMethods;
	if( DaoNS_ParseType( self, core->name, type, type, 0 ) == DAO_DT_FAILED ){
		GC_DecRC( type );
		printf( "type wrapping failed: %s\n", core->name );
		return NULL;
	}
	//printf( "type wrapping: %s\n", core->name );
	return type;
}

DaoType* DaoNamespace_WrapInterface( DaoNamespace *self, DaoTypeCore *core )
{
	DaoInterface *inter;
	DaoTypeKernel *kernel = DaoVmSpace_GetKernel( self->vmSpace, core );
	DaoType *basetype;
	DaoType *abtype;
	int i;

	if( kernel != NULL ) return kernel->abtype;

	inter = DaoInterface_New( self, core->name );
	kernel = inter->abtype->kernel;
	abtype = inter->abtype;
	abtype->core = core;

	GC_Assign( & kernel->nspace, self );

	DaoVmSpace_AddKernel( self->vmSpace, core, kernel );

	for(i=0; i<sizeof(core->bases); i++){
		if( core->bases[i] == NULL ) break;
		basetype = DaoNamespace_WrapInterface( self, core->bases[i] );
		DList_Append( inter->bases, basetype->aux );
		if( abtype->bases == NULL ) abtype->bases = DList_New( DAO_DATA_VALUE );
		DList_Append( abtype->bases, basetype );
	}
	DaoInterface_DeriveMethods( inter );

	kernel->SetupValues = DaoNamespace_SetupValues;
	kernel->SetupMethods = DaoNamespace_SetupMethods;
	if( DaoNS_ParseType( self, core->name, abtype, abtype, 1 ) == DAO_DT_FAILED ){
		GC_IncRC( inter );
		GC_DecRC( inter );
		printf( "Abstract interface wrapping failed: %s from %s\n", core->name, self->name->chars );
		return NULL;
	}
	return abtype;
}

DaoType* DaoNamespace_WrapCinType( DaoNamespace *self, DaoTypeCore *con, DaoType *abs, DaoType *tar )
{
	DaoType *sutype;
	DaoTypeCore *stdCore = DaoType_GetCoreByID( DAO_CINVALUE );
	DaoTypeKernel *kernel = DaoVmSpace_GetKernel( self->vmSpace, con );
	DaoInterface *abstract = (DaoInterface*) abs->aux;
	DaoCinType *cintype;
	int i;

	if( kernel != NULL ) return kernel->abtype;

	cintype = DaoCinType_New( abstract, tar );
	kernel = DaoTypeKernel_New( con );

	DaoVmSpace_AddKernel( self->vmSpace, con, kernel );

	GC_Assign( & cintype->citype->kernel, kernel );
	GC_Assign( & cintype->vatype->kernel, kernel );
	GC_Assign( & kernel->abtype, cintype->vatype );
	GC_Assign( & kernel->nspace, self );

	cintype->citype->core = con;
	cintype->vatype->core = con;

	con->CheckGetField   = stdCore->CheckGetField;
	con->CheckGetItem    = stdCore->CheckGetItem;
	con->CheckUnary      = stdCore->CheckUnary;
	con->CheckBinary     = stdCore->CheckBinary;
	con->CheckConversion = stdCore->CheckConversion;
	con->CheckForEach    = stdCore->CheckForEach;

	con->DoSetField   = stdCore->DoSetField;
	con->DoSetItem    = stdCore->DoSetItem;
	con->DoUnary      = stdCore->DoUnary;
	con->DoBinary     = stdCore->DoBinary;
	con->DoConversion = stdCore->DoConversion;
	con->DoForEach    = stdCore->DoForEach;

	con->Print  = stdCore->Print;

	for(i=0; i<sizeof(con->bases); i++){
		if( con->bases[i] == NULL ) break;
		if( i >= abstract->bases->size ) goto Error;
		sutype = abstract->bases->items.pInter[i]->abtype;
		sutype = DaoNamespace_WrapCinType( self, con->bases[i], sutype, tar );
		if( sutype == NULL ) goto Error;
		DList_Append( cintype->bases, sutype->aux );
	}
	DaoCinType_DeriveMethods( cintype );

	kernel->SetupValues = DaoNamespace_SetupValues;
	kernel->SetupMethods = DaoNamespace_SetupMethods;
	if( DaoNS_ParseType( self, con->name, cintype->citype, cintype->vatype, 1 ) == DAO_DT_FAILED ){
		goto Error;
	}

	/* TODO: handle error: */
	DaoNamespace_SetupMethods( self, con );

	if( DaoType_MatchInterface( cintype->vatype, abstract, NULL ) == 0 ) goto Error;
	if( abstract->concretes == NULL ) abstract->concretes = DHash_New(0,DAO_DATA_VALUE);
	DMap_Insert( abstract->concretes, cintype->target, cintype );
	return cintype->vatype;
Error:
	DaoGC_TryDelete( (DaoValue*) cintype );
	printf( "Concrete interface wrapping failed: %s from %s\n", con->name, self->name->chars );
	return NULL;
}

void DaoNamespace_SetupType( DaoNamespace *self, DaoTypeCore *core, DaoType *type )
{
	if( type->kernel != NULL ) return;

	DMutex_Lock( & mutex_values_setup ); // XXX
	if( type->kernel == NULL ){
		type->kernel = DaoTypeKernel_New( core );
		type->kernel->abtype = type;
		type->kernel->nspace = self;
		GC_IncRC( type->kernel );
		GC_IncRC( type );
		GC_IncRC( self );

		type->kernel->SetupValues = DaoNamespace_SetupValues;
		type->kernel->SetupMethods = DaoNamespace_SetupMethods;
		DaoVmSpace_AddKernel( self->vmSpace, core, type->kernel );
		DList_Append( self->auxData, type->kernel );
	}
	DMutex_Unlock( & mutex_values_setup );
}

int DaoNamespace_WrapTypes( DaoNamespace *self, DaoTypeCore *cores[] )
{
	DaoParser *parser = DaoVmSpace_AcquireParser( self->vmSpace );
	int i, ec = 0;
	for(i=0; cores[i]; i++ ){
		ec += DaoNamespace_WrapType2( self, cores[i], DAO_CDATA, 0 ) == NULL;
		/* e |= ( DaoNamespace_SetupValues( self, cores[i] ) == 0 ); */
	}
	/* if( setup ) return DaoNamespace_SetupTypes( self, cores ); */
	DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	return ec;
}

int DaoNamespace_AliasTypes( DaoNamespace *self, const char *alias[] )
{
	int i = 0, ec = 0;
	if( alias == NULL ) return 0;
	while( alias[i] && alias[i+1] ){
		ec += DaoNamespace_DefineType( self, alias[i], alias[i+1] ) == NULL;
		i += 2;
	}
	return ec;
}

int DaoNamespace_SetupTypes( DaoNamespace *self, DaoTypeCore *cores[] )
{
	int i, ec = 0;
	for(i=0; cores[i]; i++ ){
		ec += ( DaoNamespace_SetupMethods( self, cores[i] ) == 0 );
	}
	return ec;
}

DaoRoutine* DaoNamespace_MakeFunction( DaoNamespace *self, const char *proto, DaoParser *parser, DaoParser *defparser )
{
	DaoParser *old = parser;
	DaoParser *old2 = defparser;
	DaoRoutine *func;
	DaoValue *value;

	if( parser == NULL ){
		parser = DaoVmSpace_AcquireParser( self->vmSpace );
		parser->vmSpace = self->vmSpace;
		parser->nameSpace = self;
	}
	if( defparser == NULL ){
		DaoParser_InitConstEvaluator( parser );
		defparser = DaoVmSpace_AcquireParser( self->vmSpace );
		defparser->vmSpace = self->vmSpace;
		defparser->nameSpace = self;
		defparser->routine = parser->evaluator.routine;
	}
	func = DaoNamespace_ParseSignature( self, proto, parser, defparser );
	if( old  == NULL ) DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	if( old2 == NULL ) DaoVmSpace_ReleaseParser( self->vmSpace, defparser );
	if( func == NULL ) return NULL;
	value = DaoNamespace_GetData( self, func->routName );
	if( value && value->type == DAO_ROUTINE && value->xRoutine.overloads ){
		DRoutines_Add( value->xRoutine.overloads, func );
	}else{
		DaoNamespace_AddConst( self, func->routName, (DaoValue*) func, DAO_PERM_PUBLIC );
	}
	return func;
}

DaoRoutine* DaoNamespace_WrapFunction( DaoNamespace *self, DaoCFunction fptr, const char *proto )
{
	DaoRoutine *func = DaoNamespace_MakeFunction( self, proto, NULL, NULL );
	if( func == NULL ) return NULL;
	func->pFunc = fptr;
	return func;
}

int DaoNamespace_WrapFunctions( DaoNamespace *self, DaoFunctionEntry *items )
{
	DaoParser *defparser = DaoVmSpace_AcquireParser( self->vmSpace );
	DaoParser *parser = DaoVmSpace_AcquireParser( self->vmSpace );
	DaoRoutine *func;
	int i = 0;
	int ec = 0;
	parser->vmSpace = self->vmSpace;
	parser->nameSpace = self;
	DaoParser_InitConstEvaluator( parser );
	defparser->vmSpace = self->vmSpace;
	defparser->nameSpace = self;
	defparser->routine = parser->evaluator.routine;
	while( items[i].fpter != NULL ){
		func = DaoNamespace_MakeFunction( self, items[i].proto, parser, defparser );
		if( func ) func->pFunc = (DaoCFunction)items[i].fpter;
		ec += func == NULL;
		i ++;
	}
	DaoVmSpace_ReleaseParser( self->vmSpace, parser );
	DaoVmSpace_ReleaseParser( self->vmSpace, defparser );
	return ec;
}

void DaoNamespace_SetOptions( DaoNamespace *self, int options )
{
	self->options = options;
}

int DaoNamespace_GetOptions( DaoNamespace *self )
{
	return self->options;
}



int DaoNamespace_FindConst( DaoNamespace *self, DString *name )
{
	DNode *node = DMap_Find( self->lookupTable, name );
	if( node == NULL ) return -1;
	if( LOOKUP_ST( node->value.pInt ) != DAO_GLOBAL_CONSTANT ) return -1;
	return node->value.pInt;
}
/* Generate a new lookup name for the existing constant/variable (for bytecode): */
static void DaoNamespace_RenameLookup( DaoNamespace *self, DNode *node )
{
	DString *name = DString_Copy( node->key.pString );
	char chs[32];
	sprintf( chs, "[%i]", LOOKUP_ID( node->value.pInt ) );
	DString_AppendChars( name, chs );
	MAP_Insert( self->lookupTable, name, node->value.pVoid ) ;
	DString_Delete( name );
}
int DaoNamespace_AddConst( DaoNamespace *self, DString *name, DaoValue *value, int pm )
{
	DaoValue *vdest;
	DaoConstant *cst, *dest;
	DaoRoutine *mroutine;
	DNode *node = MAP_Find( self->lookupTable, name );
	int isrout2, isrout = value->type == DAO_ROUTINE;
	daoint st, pm2, up, id = 0;

	if( node && LOOKUP_ST( node->value.pInt ) == DAO_GLOBAL_CONSTANT ){
		st = LOOKUP_ST( node->value.pInt );
		id = LOOKUP_ID( node->value.pInt );
		cst = self->constants->items.pConst[id];
		if( cst->value->type == DAO_ROUTINE && value->type == DAO_ROUTINE ){
			DaoNamespace_RenameLookup( self, node );
			/* For different overloadings at different definition points: */
			mroutine = DaoRoutines_New( self, NULL, (DaoRoutine*) cst->value );
			mroutine->trait |= DAO_VALUE_CONST;
			node->value.pInt = LOOKUP_BIND( st, pm, 0, self->constants->size );
			DaoRoutines_Add( mroutine, (DaoRoutine*) value );
			DList_Append( self->constants, DaoConstant_New( (DaoValue*) mroutine, DAO_GLOBAL_CONSTANT ) );
			/*
			// Always add an entry for a routine for convenience,
			// for example, for getting all defined routines in a file:
			*/
			DList_Append( self->constants, DaoConstant_New( value, DAO_GLOBAL_CONSTANT ) );
			return node->value.pInt;
		}
	}
	if( node && LOOKUP_UP( node->value.pInt ) ){
		DaoNamespace_RenameLookup( self, node );
		node = NULL;
	}
	if( node ) return -DAO_CTW_WAS_DEFINED;

	id = LOOKUP_BIND( DAO_GLOBAL_CONSTANT, pm, 0, self->constants->size );
	MAP_Insert( self->lookupTable, name, id );
	DList_Append( self->constants, (dest = DaoConstant_New( value, DAO_GLOBAL_CONSTANT )) );
	DaoValue_MarkConst( dest->value );
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
DaoValue* DaoNamespace_GetConstByName( DaoNamespace *self, DString *name )
{
	DNode *node = DMap_Find( self->lookupTable, name );
	if( node == NULL ) return NULL;
	if( LOOKUP_ST( node->value.pInt ) != DAO_GLOBAL_CONSTANT ) return NULL;
	return self->constants->items.pConst[ LOOKUP_ID(node->value.pInt) ]->value;
}
int DaoNamespace_FindVariable( DaoNamespace *self, DString *name )
{
	DNode *node = DMap_Find( self->lookupTable, name );
	if( node == NULL ) return -1;
	if( LOOKUP_ST( node->value.pInt ) != DAO_GLOBAL_VARIABLE ) return -1;
	return node->value.pInt;
}
int DaoNamespace_AddVariable( DaoNamespace *self, DString *name, DaoValue *value, DaoType *dectype, int pm )
{
	DNode *node = MAP_Find( self->lookupTable, name );
	DaoType *valtype = DaoNamespace_GetType( self, value );
	DaoVariable *dest;
	daoint id = 0;

	if( valtype == NULL ) valtype = self->vmSpace->typeUdf;
	if( dectype && value && DaoType_MatchValue( dectype, value, NULL ) ==0 ) return -1;
	if( dectype == NULL ) dectype = valtype;
	if( value == NULL && dectype ) value = dectype->value;

	if( node ){
		if( LOOKUP_UP( node->value.pInt ) ){ /* overriding */
			DMap_EraseNode( self->lookupTable, node );
			DaoNamespace_AddVariable( self, name, value, dectype, pm );
			node = MAP_Find( self->lookupTable, name );
			return node->value.pInt;
		}
		return -1;
	}else{
		id = LOOKUP_BIND( DAO_GLOBAL_VARIABLE, pm, 0, self->variables->size );
		MAP_Insert( self->lookupTable, name, id ) ;
		DList_Append( self->variables, DaoVariable_New( value, dectype, DAO_GLOBAL_VARIABLE ) );
	}
	return id;
}
int DaoNamespace_AddStaticConst( DaoNamespace *self, DString *name, DaoValue *value, int level )
{
	int ret;
	char suffix[32];
	sprintf( suffix, "{%i}[%i]", level, (int)self->constants->size );
	name = DString_Copy( name );
	DString_AppendChars( name, suffix );
	/* should always succeed: */
	ret = DaoNamespace_AddConst( self, name, value, DAO_PERM_NONE );
	DString_Delete( name );
	return ret;
}
int DaoNamespace_AddStaticVar( DaoNamespace *self, DString *name, DaoValue *var, DaoType *tp, int level )
{
	int ret;
	char suffix[32];
	sprintf( suffix, "{%i}[%i]", level, (int)self->variables->size );
	name = DString_Copy( name );
	DString_AppendChars( name, suffix );
	ret = DaoNamespace_AddVariable( self, name, var, tp, DAO_PERM_NONE );
	DString_Delete( name );
	return ret;
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
DaoValue* DaoNamespace_GetVariableByName( DaoNamespace *self, DString *name )
{
	DNode *node = DMap_Find( self->lookupTable, name );
	if( node == NULL ) return NULL;
	if( LOOKUP_ST( node->value.pInt ) != DAO_GLOBAL_VARIABLE ) return NULL;
	return self->variables->items.pVar[ LOOKUP_ID(node->value.pInt) ]->value;
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
	DaoNamespace_AddVariable( self, name, value, NULL, DAO_PERM_PROTECTED );
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
DaoValue* DaoNamespace_GetDefinition( DaoNamespace *self, DString *name )
{
	DNode *node = MAP_Find( self->lookupTable, name );
	daoint st, id;

	if( node == NULL ) return NULL;
	st = LOOKUP_ST( node->value.pInt );
	id = LOOKUP_ID( node->value.pInt );
	if( st == DAO_GLOBAL_CONSTANT ) return self->constants->items.pValue[id];
	if( st == DAO_GLOBAL_VARIABLE ) return self->variables->items.pValue[id];
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
		DaoRoutine **routines = & routine;
		int i, num = 1;
		if( routine2->type != DAO_ROUTINE ) return;
		if( routine == routine2 ) return;
		if( routine->overloads ){
			routines = routine->overloads->routines->items.pRoutine;
			num = routine->overloads->routines->size;
		}
		if( routine2->overloads ){
			for(i=0; i<num; ++i){
				DaoRoutine *rout = routines[i];
				DRoutines_Add( routine2->overloads, rout );
			}
		}else{
			DaoRoutine *routs = DaoRoutines_New( self, NULL, routine );
			for(i=0; i<num; ++i){
				DaoRoutine *rout = routines[i];
				DRoutines_Add( routs->overloads, routine2 );
			}
			DaoValue_MarkConst( (DaoValue*) routine2 );
			/* Add individual entry for the existing function: */
			DList_Append( self->constants, DaoConstant_New( (DaoValue*) routine2, DAO_GLOBAL_CONSTANT ) );
			DaoNamespace_SetConst( self, search->value.pInt, (DaoValue*) routs );
		}
	}
}
void DaoNamespace_UpdateLookupTable( DaoNamespace *self )
{
	DNode *it, *search;
	daoint i, j, k, pm, st, up, id;

	for(i=1; i<self->namespaces->size; i++){
		DaoNamespace *ns = self->namespaces->items.pNS[i];
		DaoNamespace_UpdateLookupTable( ns );
		for(it=DMap_First( ns->lookupTable ); it; it=DMap_Next(ns->lookupTable,it) ){
			DaoValue *value = DaoNamespace_GetValue( ns, it->value.pInt );
			DString *name = it->key.pString;
			up = LOOKUP_UP( it->value.pInt );
			pm = LOOKUP_PM( it->value.pInt );
			st = LOOKUP_ST( it->value.pInt );
			id = LOOKUP_ID( it->value.pInt );
			if( pm != DAO_PERM_PUBLIC ) continue;

			search = MAP_Find( self->lookupTable, name );
			if( search && value && value->type == DAO_ROUTINE && up == 0 ){
				DaoNS_ImportRoutine( self, name, (DaoRoutine*)value, pm );
				continue;
			}
			if( search ) continue;
			if( st == DAO_GLOBAL_CONSTANT ){
				DaoConstant *cst = ns->constants->items.pConst[id];
				if( cst->value && cst->value->type == DAO_ROUTINE ){
					DaoNamespace_AddConst( self, name, cst->value, pm );
					continue;
				}
				k = LOOKUP_BIND( st, pm, up+1, self->constants->size );
				MAP_Insert( self->lookupTable, name, k );
				DList_Append( self->constants, ns->constants->items.pConst[id] );
			}else{
				k = LOOKUP_BIND( st, pm, up+1, self->variables->size );
				MAP_Insert( self->lookupTable, name, k );
				DList_Append( self->variables, ns->variables->items.pVar[id] );
			}
		}
	}
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
	DList_Append( self->auxData, parent );
	DList_Append( self->namespaces, parent );
	DaoNamespace_UpdateLookupTable( self );
	return 1;
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
DaoType* DaoNamespace_FindTypeChars( DaoNamespace *self, const char *name )
{
	DString name2 = DString_WrapChars( name );
	return DaoNamespace_FindType( self, & name2 );
}
DaoType* DaoNamespace_ParseType( DaoNamespace *self, const char *name )
{
	return DaoParser_ParseTypeName( name, self, NULL );
}
DaoType* DaoNamespace_AddType( DaoNamespace *self, DString *name, DaoType *type )
{
	DNode *node;
	if( DString_FindChar( type->name, '@', 0 ) == DAO_NULLPOS ){
		DMutex_Lock( & mutex_type_map );
		node = MAP_Find( self->abstypes, name );
		if( node == NULL ){
			MAP_Insert( self->abstypes, name, type );
		}else{
			DList_Append( self->auxData, type );
			type = node->value.pType;
		}
		DMutex_Unlock( & mutex_type_map );
	}else{
		DList_Append( self->auxData, type );
	}
	return type;
}
void DaoNamespace_AddTypeConstant( DaoNamespace *self, DString *name, DaoType *type )
{
	int id = DaoNamespace_FindConst( self, name );
	if( id >=0 && LOOKUP_UP(id) ) return;
	if( type->aux && (type->tid >= DAO_OBJECT && type->tid <= DAO_INTERFACE) ){
		DaoNamespace_AddConst( self, name, type->aux, DAO_PERM_PUBLIC );
	}else{
		DaoNamespace_AddConst( self, name, (DaoValue*) type, DAO_PERM_PUBLIC );
	}
}


DaoType* DaoNamespace_GetType( DaoNamespace *self, DaoValue *value )
{
	DNode *node;
	DList *nested;
	DString *mbs;
	DaoType *type;
	DaoType *itype;
	DaoTypeCore *core;
	int i, tid;

	type = DaoValue_GetType( value, self->vmSpace );
	if( type ) return type;

	if( value == NULL ) return NULL;

	if( value->type == DAO_LIST ){
		if( value->xList.value->size == 0 ) return self->vmSpace->typeListEmpty;
		return DaoNamespace_MakeType( self, "list", DAO_LIST, NULL, NULL, 0 );
	}else if( value->type == DAO_MAP ){
		if( value->xMap.value->size == 0 ) return self->vmSpace->typeMapEmpty;
		return DaoNamespace_MakeType( self, "map", DAO_MAP, NULL, NULL, 0 );
	}

	tid = value->type;
	nested = NULL;
	mbs = DString_New();
	if( value->type <= DAO_TUPLE ){
		DString_SetChars( mbs, coreTypeNames[value->type] );
		if( value->type == DAO_TUPLE ){
			DaoTuple *tuple = (DaoTuple*) value;
			DString_SetChars( mbs, "tuple<" );
			nested = DList_New(0);
			for(i=0; i<tuple->size; i++){
				itype = DaoNamespace_GetType( self, tuple->values[i] );
				DList_Append( nested, itype );
				DString_Append( mbs, itype->name );
				if( i+1 < tuple->size ) DString_AppendChars( mbs, "," );
			}
			DString_AppendChars( mbs, ">" );
#ifdef DAO_WITH_NUMARRAY
		}else if( value->type == DAO_ARRAY ){
			DaoArray *array = (DaoArray*) value;
			nested = DList_New(0);
			if( array->size ==0 ){
				DString_AppendChars( mbs, "<?>" );
				DList_Append( nested, self->vmSpace->typeUdf );
			}else if( array->etype == DAO_INTEGER ){
				itype = DaoNamespace_MakeType( self, "int", DAO_INTEGER, 0,0,0 );
				DString_AppendChars( mbs, "<int>" );
				DList_Append( nested, itype );
			}else if( array->etype == DAO_FLOAT ){
				itype = DaoNamespace_MakeType( self, "float", DAO_FLOAT, 0,0,0 );
				DString_AppendChars( mbs, "<float>" );
				DList_Append( nested, itype );
			}else{
				itype = DaoNamespace_MakeType( self, "complex", DAO_COMPLEX, 0,0,0 );
				DString_AppendChars( mbs, "<complex>" );
				DList_Append( nested, itype );
			}
#endif
		}
		type = DaoNamespace_FindType( self, mbs );
		if( type == NULL ){
			type = DaoType_New( self, mbs->chars, tid, NULL, nested );
			type = DaoNamespace_AddType( self, type->name, type );
		}
	}else if( value->type == DAO_TYPE ){
		itype = (DaoType*) value;
		DString_SetChars( mbs, "type<" );
		nested = DList_New(0);
		DList_Append( nested, itype );
		DString_Append( mbs, itype->name );
		DString_AppendChars( mbs, ">" );
		type = DaoNamespace_FindType( self, mbs );
		if( type == NULL ){
			type = DaoType_New( self, mbs->chars, value->type, NULL, nested );
			type = DaoNamespace_AddType( self, type->name, type );
		}
	}else{
		core = DaoValue_GetTypeCore( value );
		DString_SetChars( mbs, core->name );
		type = DaoNamespace_FindType( self, mbs );
		if( type == NULL ){
			type = DaoType_New( self, core->name, value->type, NULL, NULL );
			type = DaoNamespace_AddType( self, type->name, type );
		}
	}
	/*
	// type might be rout->routType, which might be NULL,
	// in case rout is DaoNamespace.constEvalRoutine
	*/
	//XXX if( type && type->core ==NULL ) type->core = DaoValue_GetTypeCore( value );
	DString_Delete( mbs );
	if( nested ) DList_Delete( nested );
	return type;
}
DaoType* DaoNamespace_MakeType( DaoNamespace *self, const char *name,
		uint_t tid, DaoValue *aux, DaoType *itypes[], int N )
{
	DaoClass *klass;
	DaoType *any = NULL;
	DaoType *type = NULL;
	DString *mbs = NULL;
	DList  *tylist = NULL;
	int i, n = strlen( name );
	int attrib = tid >> 16;

	if( (tid & DAO_ANY) && self != self->vmSpace->daoNamespace ){
		return DaoNamespace_MakeType( self->vmSpace->daoNamespace, name, tid, aux, itypes, N );
	}

	tid = tid & 0xffff;
	if( tid != DAO_ANY ) any = self->vmSpace->typeAny;

	switch( tid ){
	case DAO_NONE :
		return self->vmSpace->typeNone;
	case DAO_BOOLEAN :
		return self->vmSpace->typeBool;
	case DAO_INTEGER :
		return self->vmSpace->typeInt;
	case DAO_FLOAT :
		return self->vmSpace->typeFloat;
	case DAO_COMPLEX :
		return self->vmSpace->typeComplex;
	case DAO_STRING :
		return self->vmSpace->typeString;
	case DAO_ENUM :
		if( N == 0 ) return self->vmSpace->typeEnum;
		break;
	case DAO_ARRAY :
		if( self->vmSpace->typeArray == NULL ) return NULL; /* Numeric array not enable; */
		return DaoType_Specialize( self->vmSpace->typeArray, itypes, N, self );
	case DAO_LIST :
		return DaoType_Specialize( self->vmSpace->typeList, itypes, N, self );
	case DAO_MAP :
		return DaoType_Specialize( self->vmSpace->typeMap, itypes, N, self );
	case DAO_TUPLE :
		if( N == 0 ) return self->vmSpace->typeTuple;
		break;
	case DAO_INTERFACE :
		if( aux == NULL ) break; /* Maybe the general "interface" type; */
		return aux->xInterface.abtype;
	case DAO_CLASS :
		if( aux == NULL ) break; /* Maybe the general "class" type; */
		return aux->xClass.clsType;
	case DAO_OBJECT :
		if( aux == NULL ) return NULL;
		return aux->xClass.objType;
	case DAO_CTYPE :
		if( aux == NULL ) return NULL;
		return aux->xCtype.classType;
	case DAO_CDATA :
	case DAO_CSTRUCT :
		if( aux == NULL ) return NULL;
		return aux->xCtype.valueType;
	}
	
	if( tid == DAO_VARIANT ){
		DList *types;
		int newlist = 0;
		int j, k = 0;
		if( N == 0 ){
			return self->vmSpace->typeNone;
		}else if( N == 1 ){
			return itypes[0];
		}
		types = DList_New(0);
		/* Coalesce variants: */
		for(i=0; i<N; ++i){
			if( itypes[i]->tid == DAO_VARIANT ){
				for(j=0; j<itypes[i]->args->size; ++j){
					DList_Append( types, itypes[i]->args->items.pType[j] );
				}
			}else{
				DList_Append( types, itypes[i] );
			}
		}
		newlist = types->size > N;
		/* Remove redundant variants: (and do not modify "itypes"!) */
		for(i=0; i<types->size; ++i){
			DaoType *it = types->items.pType[i];
			int unique = 1;
			for(j=0; j<k; ++j){
				DaoType *jt = types->items.pType[j];
				int e1 = it->tid == jt->tid;
				int e2 = it->aux == jt->aux;
				int e3 = DString_EQ( it->name, jt->name );
				/* Note:
				// 1. Consider aliased types as unique types;
				// 2. Different type objects may exist for the same type (such as "int");
				*/
				if( e1 && e2 && e3 ){
					newlist = 1;
					unique = 0;
					break;
				}
			}
			if( unique ) types->items.pType[k++] = it;
		}
		if( newlist ){
			type = DaoNamespace_MakeType( self, "", DAO_VARIANT, NULL, types->items.pType, k );
			DList_Delete( types );
			return type;
		}
		DList_Delete( types );
		/* Then use the original "itypes" and "N" arguments: */
	}

	mbs = DString_New();
	DString_Reserve( mbs, 128 );
	DString_SetChars( mbs, name );
	if( tid == DAO_CODEBLOCK ) DString_Clear( mbs );
	if( N > 0 || tid == DAO_CODEBLOCK ){
		tylist = DList_New(0);
		if( n || tid != DAO_VARIANT ) DString_AppendChar( mbs, '<' );
		for(i=0; i<N; i++){
			DaoType *it = itypes[i];
			if( tid == DAO_TUPLE && it->tid == DAO_PAR_DEFAULT ){
				it = DaoNamespace_MakeType( self, it->fname->chars, DAO_PAR_NAMED, it->aux, NULL, 0 );
			}

			if( i ) DString_AppendChar( mbs, tid == DAO_VARIANT ? '|' : ',' );
			DString_Append( mbs, it->name );
			DList_Append( tylist, it );
		}
		if( (tid == DAO_ROUTINE || tid == DAO_CODEBLOCK) && aux && aux->type == DAO_TYPE ){
			DString_AppendChars( mbs, "=>" );
			DString_Append( mbs, ((DaoType*)aux)->name );
		}
		if( n || tid != DAO_VARIANT ) DString_AppendChar( mbs, '>' );
	}else if( tid == DAO_LIST || tid == DAO_ARRAY ){
		tylist = DList_New(0);
		DString_AppendChars( mbs, "<any>" );
		DList_Append( tylist, any );
	}else if( tid == DAO_MAP ){
		tylist = DList_New(0);
		DString_AppendChars( mbs, "<any,any>" );
		DList_Append( tylist, any );
		DList_Append( tylist, any );
	}else if( tid == DAO_TUPLE ){
		if( DString_FindChar( mbs, '<', 0 ) == DAO_NULLPOS ){
			DString_AppendChars( mbs, "<...>" );
			attrib |= DAO_TYPE_VARIADIC;
		}
	}else if( tid == DAO_CLASS && aux ){
		/*
		// do not save the abstract type for class and object in namespace,
		// because the class may be nested in another class, and different
		// class may nest different class with the same name, eg:
		// Error::Field::NotExist and Error::Key::NotExist
		*/
		klass = (DaoClass*) aux;
		type = klass->clsType;
		goto Finalizing;
	}else if( tid == DAO_OBJECT ){
		klass = (DaoClass*) aux;
		type = klass->objType;
		goto Finalizing;
	}else if( (tid == DAO_ROUTINE || tid == DAO_CODEBLOCK) && aux && aux->type == DAO_TYPE ){
		DString_AppendChar( mbs, '<' );
		DString_AppendChars( mbs, "=>" );
		DString_Append( mbs, aux->xType.name );
		DString_AppendChar( mbs, '>' );
	}else if( tid >= DAO_PAR_NAMED && tid <= DAO_PAR_VALIST ){
		if( aux == NULL ) aux = (DaoValue*) any;
		if( aux->type != DAO_TYPE ) goto Finalizing;
		switch( tid ){
		case DAO_PAR_NAMED   : DString_AppendChar( mbs, ':' ); break;
		case DAO_PAR_DEFAULT : DString_AppendChar( mbs, '=' ); break;
		case DAO_PAR_VALIST  : DString_AppendChar( mbs, ':' ); break;
		}
		DString_Append( mbs, aux->xType.name );
	}
	if( tid == DAO_CODEBLOCK ){
		mbs->chars[0] = '[';
		mbs->chars[mbs->size-1] = ']';
	}
	type = DaoNamespace_FindType( self, mbs );
	if( type == NULL ){
		type = DaoType_New( self, mbs->chars, tid, aux, tylist );
		type->attrib |= attrib;
		if( attrib & DAO_TYPE_VARIADIC ) type->variadic = 1;
		if( tid == DAO_PAR_NAMED && N > 0 ){
			DaoType *it = itypes[0];
			DaoValue *aux = (DaoValue*) it;
			DString *fname = NULL;
			if( it->tid == DAO_PAR_NAMED && DString_FindChars( it->name, "var<", 0 ) != 0 ){
				aux = it->aux;
				fname = it->fname;
			}
			if( fname ) DString_Assign( type->fname, fname );
			GC_Assign( & type->aux, aux );
		}else if( tid == DAO_PAR_NAMED || tid == DAO_PAR_DEFAULT ){
			DString_SetChars( type->fname, name );
		}
		DaoType_CheckAttributes( type );
		type = DaoNamespace_AddType( self, type->name, type );
	}
Finalizing:
	DString_Delete( mbs );
	if( tylist ) DList_Delete( tylist );
	return type;
}
DaoType* DaoNamespace_MakeType2( DaoNamespace *self, const char *name,
		uint_t tid, DaoValue *aux, DaoType *itypes[], int N )
{
	DaoType **itypes2 = (DaoType**) dao_calloc( N, sizeof(DaoType*) );
	DaoType *type, *auxtype = DaoValue_CastType( aux );
	int i;
	if( auxtype && auxtype->invar ) auxtype = DaoType_GetBaseType( auxtype );
	for(i=0; i<N; ++i){
		DaoType *type = itypes[i];
		if( type && type->invar ) type = DaoType_GetBaseType( type );
		itypes2[i] = type;
	}
	type = DaoNamespace_MakeType( self, name, tid, (DaoValue*) auxtype, itypes2, N );
	dao_free( itypes2 );
	return type;
}
DaoType* DaoNamespace_MakeRoutType( DaoNamespace *self, DaoType *routype,
		DaoValue *vals[], DaoType *types[], DaoType *retype )
{
	DaoType *type, *partype, *newtype;
	DString *fname = NULL;
	DNode *node;
	daoint i, ch = 0;

	newtype = DaoType_New( self, "", DAO_ROUTINE, NULL, NULL );
	newtype->attrib = routype->attrib;
	if( routype->mapNames ){
		if( newtype->mapNames ) DMap_Delete( newtype->mapNames );
		newtype->mapNames = DMap_Copy( routype->mapNames );
	}

	DString_AppendChars( newtype->name, "routine<" );
	for(i=0; i<routype->args->size; i++){
		if( i >0 ) DString_AppendChars( newtype->name, "," );
		type = partype = routype->args->items.pType[i];
		if( type && (type->tid == DAO_PAR_NAMED || type->tid == DAO_PAR_DEFAULT) ){
			partype = & type->aux->xType;
		}
		if( partype && (partype->tid == DAO_UDT || partype->tid == DAO_THT) ){
			if( vals && vals[i] ){
				partype = DaoNamespace_GetType( self, vals[i] );
			}else if( types && types[i] ){
				partype = types[i];
			}
			if( partype && (partype->tid == DAO_PAR_NAMED || partype->tid == DAO_PAR_DEFAULT) ){
				partype = & partype->aux->xType;
			}
		}
		/* XXX typing DString_AppendChars( newtype->name, type ? type->name->chars : "..." ); */
		if( partype != type && partype != & type->aux->xType ){
			type = DaoType_New( self, type->fname->chars, type->tid, (DaoValue*) partype, NULL );
		}
		DString_Append( newtype->name, type->name );
		DList_Append( newtype->args, type );
	}
	type = retype ? retype : & routype->aux->xType;
	if( type ){
		DString_AppendChars( newtype->name, "=>" );
		DString_Append( newtype->name, type->name );
	}
	DString_AppendChars( newtype->name, ">" );
	GC_Assign( & newtype->aux, type );
	if( routype->cbtype ){
		DMap *defs = DHash_New(0,0);
		DaoType_MatchTo( newtype, routype, defs );
		type = DaoType_DefineTypes( routype->cbtype, self, defs );
		GC_Assign( & newtype->cbtype, type );
		DMap_Delete( defs );
		DString_Append( newtype->name, newtype->cbtype->name );
	}
	type = DaoNamespace_FindType( self, newtype->name );
	if( type ){
		DaoGC_TryDelete( (DaoValue*) newtype );
		return type;
	}
	DaoType_CheckAttributes( newtype );
	DaoType_InitDefault( newtype );
	DaoNamespace_AddType( self, newtype->name, newtype );
	return newtype;
}

DaoRoutine* DaoNamespace_ParseSignature( DaoNamespace *self, const char *proto, DaoParser *parser, DaoParser *defparser )
{
	DaoRoutine *func = DaoRoutine_New( self, NULL, 0 );
	DaoParser *oldparser = defparser;
	int optok = 0;

	assert( parser != NULL );
	if( defparser == NULL ){
		DaoParser_InitConstEvaluator( parser );
		defparser = DaoVmSpace_AcquireParser( self->vmSpace );
		defparser->vmSpace = self->vmSpace;
		defparser->nameSpace = self;
		defparser->hostType = parser->hostType;
		defparser->hostCtype = parser->hostCtype;
		defparser->routine = parser->evaluator.routine;
	}

	GC_IncRC( parser->hostType );
	func->routHost = parser->hostType;
	if( ! DaoLexer_Tokenize( defparser->lexer, proto, 0 ) ) goto Error;
	if( defparser->tokens->size < 3 ) goto Error;

	parser->routine = (DaoRoutine*) func; /* safe to parse params only */
	if( DaoParser_ParseSignature( defparser, parser, optok ) < 0 ){
		DaoParser_PrintError( defparser );
		goto Error;
	}
	if( oldparser == NULL ) DaoVmSpace_ReleaseParser( self->vmSpace, defparser );
	return func;
Error:
	printf( "Function wrapping failed for %s\n", proto );
	if( oldparser == NULL ) DaoVmSpace_ReleaseParser( self->vmSpace, defparser );
	DaoRoutine_Delete( func );
	return NULL;
}
DaoEnum* DaoNamespace_MakeSymbol( DaoNamespace *self, const char *symbol )
{
	DString *name = DString_NewChars( symbol );
	DaoType *type;

	if( symbol[0] != '$' ) DString_InsertChar( name, '$', 0 );

	type = DaoNamespace_MakeSymbolType( self, name->chars );
	DString_Delete( name );

	return (DaoEnum*) type->value;
}
DaoType* DaoNamespace_MakeSymbolType( DaoNamespace *self, const char *symbol )
{
	DString sym = DString_WrapChars( symbol + 1 );
	DString *name = DString_NewChars( "enum<" );
	DaoType *type;

	DString_Append( name, & sym );
	DString_AppendChar( name, '>' );
	type = DaoNamespace_FindType( self, name );
	if( type || symbol[0] != '$' ){
		DString_Delete( name );
		return type;
	}

	type = DaoType_New( self, name->chars, DAO_ENUM, NULL, NULL );
	type->subtid = DAO_ENUM_SYM;
	DString_Assign( type->fname, type->name );
	DMap_Insert( type->mapNames, & sym, (void*)0 );
	DaoNamespace_AddType( self, type->name, type );
	DString_Delete( name );
	return type;
}
/* symbols should be comma or semicolon delimited string */
DaoType* DaoNamespace_MakeEnumType( DaoNamespace *self, const char *symbols )
{
	DaoType *type;
	DString *key, *name = DString_New();
	int n = strlen( symbols );
	int i, k = 0, t1 = 0, t2 = 0;

	DString_SetChars( name, "enum<" );
	DString_AppendChars( name, symbols );
	DString_AppendChar( name, '>' );
	type = DaoNamespace_FindType( self, name );
	if( type ){
		DString_Delete( name );
		return type;
	}
	key = DString_New();
	type = DaoType_New( self, name->chars, DAO_ENUM, NULL, NULL );
	type->subtid = DAO_ENUM_SYM;
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
			t2 = sym;
		}else{
			DString_AppendChar( key, sym );
		}
	}
	if( t2 ){
		if( t2 == ';' ) type->subtid = DAO_ENUM_FLAG;
		MAP_Insert( type->mapNames, key, 1<<k );
	}else{
		if( t1 == ',' ) type->subtid = DAO_ENUM_STATE;
		MAP_Insert( type->mapNames, key, k );
	}
	DaoNamespace_AddType( self, name, type );
	DString_Delete( name );
	DString_Delete( key );
	return t1 != 0 && t2 != 0 ? NULL : type;
}
DaoType* DaoNamespace_MakeValueType( DaoNamespace *self, DaoValue *value )
{
	DaoType *type;
	DString *name;
	if( value == NULL || value->type >= DAO_ARRAY ) return NULL;
	name = DString_New();
	DaoValue_GetString( value, name );
	if( value->type == DAO_ENUM && value->xEnum.subtype == DAO_ENUM_SYM ){
		type = DaoNamespace_MakeSymbolType( self, name->chars );
		DString_Delete( name );
		return type;
	}
	if( value->type == DAO_STRING ){
		DString_InsertChar( name, '\'', 0 );
		DString_AppendChar( name, '\'' );
	}
	if( name->size ==0 && value->type ==0 ) DString_SetChars( name, "none" );
	type = DaoNamespace_FindType( self, name );
	if( type == NULL ){
		if( value->type == DAO_NONE ){
			type = DaoType_New( self, "none", DAO_NONE, NULL, NULL );
		}else{
			type = DaoNamespace_GetType( self, value );
			type = DaoType_Copy( type );
			DString_Assign( type->name, name );
		}
		GC_Assign( & type->value, value );
		type->valtype = 1;
		DaoNamespace_AddType( self, name, type );
	}
	DString_Delete( name );
	return type;
}

DaoType* DaoNamespace_MakeRangeType( DaoNamespace *self, DaoType *first, DaoType *second )
{
	DaoType *noneType = DaoNamespace_MakeValueType( self, dao_none_value );
	DaoType *types[2] = {NULL, NULL};
	DaoType *type, *type2;
	DString *name;

	if( first == NULL ) first = noneType;
	if( second == NULL ) second = noneType;
	if( first->invar )  first = DaoType_GetBaseType( first );
	if( second->invar ) second = DaoType_GetBaseType( second );
	types[0] = first;
	types[1] = second;
	type = DaoNamespace_MakeType( self, "tuple", DAO_TUPLE, NULL, types, 2 );
	name = DString_Copy( type->name );
	DString_AppendChars( name, "::subtype::range" ); /* Distinguish with normal tuple types; */
	type2 = DaoNamespace_FindType( self, name );
	if( type2 == NULL ){
		type = type2 = DaoType_Copy( type );
		type->subtid = DAO_RANGE;
		DaoNamespace_AddType( self, name, type );
	}
	DString_Delete( name );
	return type2;
}

DaoType* DaoNamespace_MakeRangeValueType( DaoNamespace *self, DaoValue *first, DaoValue *second )
{
	DaoType *tp1 = DaoNamespace_MakeValueType( self, first );
	DaoType *tp2 = DaoNamespace_MakeValueType( self, second );
	return DaoNamespace_MakeRangeType( self, tp1, tp2 );
}

DaoType* DaoNamespace_MakeIteratorType( DaoNamespace *self, DaoType *itype )
{
	DaoType *types[2] = {NULL, NULL};
	DaoType *type, *type2;
	DString *name;

	types[0] = self->vmSpace->typeBool;
	types[1] = itype;
	type = DaoNamespace_MakeType( self, "tuple", DAO_TUPLE, NULL, types, 2 );
	name = DString_Copy( type->name );
	DString_AppendChars( name, "::subtype::iterator" ); /* Distinguish with normal tuple types; */
	type2 = DaoNamespace_FindType( self, name );
	if( type2 == NULL ){
		type = type2 = DaoType_Copy( type );
		type->subtid = DAO_ITERATOR;
		DaoNamespace_AddType( self, name, type );
	}
	DString_Delete( name );
	return type2;
}

DaoType* DaoNamespace_MakeInvarSliceType( DaoNamespace *self, DaoType *type )
{
	int i, tid = type->tid;
	const char *name = coreTypeNames[tid];
	DList *types = DList_New(0);

	for(i=0; i<type->args->size; ++i){
		DaoType *it = type->args->items.pType[i];
		if( it->tid && it->tid <= DAO_ENUM ){
			it = DaoType_GetBaseType( it );
		}else if( it->tid == DAO_PAR_NAMED || it->tid == DAO_PAR_VALIST ){
			const char *fn = it->fname->chars;
			DaoType *tp = (DaoType*) it->aux;
			if( tp->tid && tp->tid <= DAO_ENUM ){
				tp = DaoType_GetBaseType( tp );
			}else{
				tp = DaoType_GetInvarType( tp );
			}
			it = DaoNamespace_MakeType( self, fn, it->tid, (DaoValue*) tp, NULL, 0 );
		}else{
			it = DaoType_GetInvarType( it );
		}
		DList_Append( types, it );
	}
	type = DaoNamespace_MakeType( self, name, tid, NULL, types->items.pType, types->size );
	DList_Delete( types );
	return type;
}

DaoNamespace* DaoNamespace_LoadModule( DaoNamespace *self, DString *name )
{
	DaoNamespace *mod = DaoNamespace_FindNamespace( self, name );
	if( mod ) return mod;

	name = DString_Copy( name );
	mod = DaoVmSpace_LoadModule( self->vmSpace, name, NULL );
	DString_Delete( name );
	return mod;
}



int DaoNamespace_SetupValues( DaoNamespace *self, DaoTypeCore *core )
{
	daoint i, j, valCount;
	DaoTypeKernel *kernel = DaoVmSpace_GetKernel( self->vmSpace, core );
	DMap *values;
	DNode *it;

	if( kernel == NULL ) return 0;
	if( kernel->SetupValues == NULL ) return 1;
	if( core->numbers == NULL && core->bases[0] == NULL ) return 0;

	for(i=0; i<sizeof(core->bases); i++){
		if( core->bases[i] == NULL ) break;
		DaoNamespace_SetupValues( self, core->bases[i] );
	}

	valCount = 0;
	if( core->numbers != NULL ){
		while( core->numbers[ valCount ].name != NULL ) valCount ++;
	}

	DMutex_Lock( & mutex_values_setup );
	if( kernel->values == NULL ){
		DaoValue buf = {0};
		DaoValue *value = (DaoValue*) & buf;
		DaoType *abtype = kernel->abtype;

		memset( value, 0, sizeof(DaoValue) );
		values = DHash_New( DAO_DATA_STRING, DAO_DATA_VALUE );
		kernel->values = values;
		for(i=0; i<valCount; i++){
			DString name = DString_WrapChars( core->numbers[i].name );
			double dv = core->numbers[i].value;
			value->type = core->numbers[i].type;
			switch( value->type ){
			case DAO_BOOLEAN : value->xBoolean.value = (dao_boolean) dv; break;
			case DAO_INTEGER : value->xInteger.value = (dao_integer) dv; break;
			case DAO_FLOAT   : value->xFloat.value = dv; break;
			default : continue;
			}
			DMap_Insert( values, & name, value );
		}
		if( kernel->abtype->bases != NULL ){
			for(i=0; i<kernel->abtype->bases->size; ++i){
				DaoTypeKernel *skn = kernel->abtype->bases->items.pType[i]->kernel;
				DaoTypeCore *sup = kernel->abtype->bases->items.pType[i]->core;

				if( sup->numbers == NULL ) continue;
				for(j=0; sup->numbers[j].name!=NULL; j++){
					DString name = DString_WrapChars( sup->numbers[j].name );
					it = DMap_Find( skn->values, & name );
					if( it && DMap_Find( values, & name ) == NULL ){
						DMap_Insert( values, it->key.pVoid, it->value.pVoid );
					}
				}
			}
		}
	}
	kernel->SetupValues = NULL;
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
		GC_Assign( & node->value.pValue, mroutine );
	}
}


int DaoNamespace_SetupMethods( DaoNamespace *self, DaoTypeCore *core )
{
	DaoTypeKernel *kernel = DaoVmSpace_GetKernel( self->vmSpace, core );
	DaoParser *parser, *defparser;
	DaoRoutine *cur;
	DList *parents;
	DMap *methods;
	DMap *supMethods;
	DNode *it;
	daoint i, k, size;

	if( kernel == NULL ) return 0;
	if( kernel->SetupMethods == NULL ) return 1;

	if( core->methods == NULL && core->bases[0] == NULL ) return 0;

	for(i=0; i<sizeof(core->bases); i++){
		if( core->bases[i] == NULL ) break;
		DaoNamespace_SetupMethods( self, core->bases[i] );
	}
	DMutex_Lock( & mutex_methods_setup );
	if( kernel->methods == NULL ){
		DaoType *hostype = kernel->abtype;

		methods = DHash_New( DAO_DATA_STRING, DAO_DATA_VALUE );

		kernel->methods = methods;

		parser = DaoVmSpace_AcquireParser( self->vmSpace );
		parser->vmSpace = self->vmSpace;
		parser->nameSpace = self;
		parser->hostType = hostype;
		parser->hostCtype = (DaoCtype*) hostype->aux;
		DaoParser_InitConstEvaluator( parser );

		defparser = DaoVmSpace_AcquireParser( self->vmSpace );
		defparser->vmSpace = self->vmSpace;
		defparser->nameSpace = self;
		defparser->hostType = hostype;
		defparser->hostCtype = (DaoCtype*) hostype->aux;
		defparser->routine = parser->evaluator.routine;

		size = 0;
		if( core->methods != NULL ){
			while( core->methods[ size ].proto != NULL ) size ++;
		}

		if( kernel->abtype->bases != NULL ){
			for(i=0; i<kernel->abtype->bases->size; ++i){
				DaoTypeKernel *skn = kernel->abtype->bases->items.pType[i]->kernel;
				DaoTypeCore *sup = kernel->abtype->bases->items.pType[i]->core;
				supMethods = skn->methods;
				for(it=DMap_First(supMethods); it; it=DMap_Next(supMethods, it)){
					if( it->value.pRoutine->overloads ){
						DRoutines *meta = (DRoutines*) it->value.pRoutine->overloads;
						for(k=0; k<meta->routines->size; k++){
							DaoRoutine *rout = meta->routines->items.pRoutine[k];
							/* skip constructor */
							if( rout->attribs & DAO_ROUT_INITOR ) continue;
							DaoMethods_Insert( hostype->kernel->methods, rout, self, hostype );
						}
					}else{
						DaoRoutine *rout = it->value.pRoutine;
						/* skip constructor */
						if( rout->attribs & DAO_ROUT_INITOR ) continue;
						DaoMethods_Insert( hostype->kernel->methods, rout, self, hostype );
					}
				}
			}
		}
		for( i=0; i<size; i++ ){
			const char *proto = core->methods[i].proto;
			cur = DaoNamespace_ParseSignature( self, proto, parser, defparser );
			if( cur == NULL ){
				printf( "  In function: %s::%s\n", core->name, proto );
				continue;
			}
			cur->pFunc = core->methods[i].fpter;
			if( hostype && DString_EQ( cur->routName, hostype->name ) ){
				cur->attribs |= DAO_ROUT_INITOR;
				DaoTypeKernel_InsertInitor( hostype->kernel, self, hostype, cur );
			}
			DaoMethods_Insert( methods, cur, self, hostype );
		}
		if( hostype->tid == DAO_INTERFACE ){
			DMap_Assign( hostype->aux->xInterface.methods, methods );
			hostype->aux->xInterface.derived = 1;
		}else if( hostype->tid == DAO_CINVALUE ){
			DMap_Assign( hostype->aux->xCinType.methods, methods );
			hostype->aux->xCinType.derived = 1;
		}
		DaoVmSpace_ReleaseParser( self->vmSpace, parser );
		DaoVmSpace_ReleaseParser( self->vmSpace, defparser );
	}
	kernel->SetupMethods = NULL;
	DMutex_Unlock( & mutex_methods_setup );
	return 1;
}



static DaoType* DaoNamespace_CheckGetField( DaoType *self, DaoString *name, DaoRoutine *ctx )
{
	DaoNamespace *NS = (DaoNamespace*) self->aux;
	DNode *node = DMap_Find( NS->lookupTable, name->value );
	int st, pm, id;

	if( node == NULL ) return NULL;
	st = LOOKUP_ST( node->value.pInt );
	pm = LOOKUP_PM( node->value.pInt );
	id = LOOKUP_ID( node->value.pInt );
	if( pm == DAO_PERM_PRIVATE && NS != ctx->nameSpace ) return NULL;
	if( st == DAO_GLOBAL_CONSTANT ){
		return DaoNamespace_GetType( NS, NS->constants->items.pConst[id]->value );
	}else{
		return NS->variables->items.pVar[id]->dtype;
	}
	return NULL;
}

static DaoValue* DaoNamespace_DoGetField( DaoValue *self, DaoString *name, DaoProcess *proc )
{
	DaoNamespace *NS = (DaoNamespace*) self;
	DNode *node = DMap_Find( NS->lookupTable, name->value );
	int st, pm, id;

	if( node == NULL ) goto FieldNotExist;
	st = LOOKUP_ST( node->value.pInt );
	pm = LOOKUP_PM( node->value.pInt );
	id = LOOKUP_ID( node->value.pInt );
	if( pm == DAO_PERM_PRIVATE && NS != proc->activeNamespace ) goto FieldNoPermit;
	if( st == DAO_GLOBAL_CONSTANT ){
		DaoProcess_PutValue( proc, NS->constants->items.pConst[id]->value );
	}else{
		DaoProcess_PutValue( proc, NS->variables->items.pVar[id]->value );
	}
	return NULL;
FieldNotExist:
	DaoProcess_RaiseError( proc, "Field::NotExist", name->value->chars );
	return NULL;
FieldNoPermit:
	DaoProcess_RaiseError( proc, "Field::NotPermit", name->value->chars );
	return NULL;
InvalidField:
	DaoProcess_RaiseError( proc, "Field", name->value->chars );
	return NULL;
}

static int DaoNamespace_CheckSetField( DaoType *self, DaoString *name, DaoType *value, DaoRoutine *ctx )
{
	DaoVariable *dest;
	DaoNamespace *NS = (DaoNamespace*) self->aux;
	DNode *node = DMap_Find( NS->lookupTable, name->value );
	int st, pm, id;

	if( node == NULL ) return DAO_ERROR_FIELD_ABSENT;
	st = LOOKUP_ST( node->value.pInt );
	pm = LOOKUP_PM( node->value.pInt );
	id = LOOKUP_ID( node->value.pInt );
	if( pm == DAO_PERM_PRIVATE && NS != ctx->nameSpace ) return DAO_ERROR_FIELD_HIDDEN;
	if( st == DAO_GLOBAL_CONSTANT ) return DAO_ERROR_FIELD_HIDDEN;
	dest = NS->variables->items.pVar[id];
	if( DaoType_MatchTo( value, dest->dtype, NULL ) == 0 ) return DAO_ERROR_TYPE;
	return DAO_OK;
}

static int DaoNamespace_DoSetField( DaoValue *self, DaoString *name, DaoValue *value, DaoProcess *proc )
{
	DaoVariable *dest;
	DaoNamespace *NS = (DaoNamespace*) self;
	DNode *node = DMap_Find( NS->lookupTable, name->value );
	int st, pm, id;

	if( node == NULL ) goto FieldNotExist;
	st = LOOKUP_ST( node->value.pInt );
	pm = LOOKUP_PM( node->value.pInt );
	id = LOOKUP_ID( node->value.pInt );
	if( pm == DAO_PERM_PRIVATE && NS != proc->activeNamespace ) goto FieldNoPermit;
	if( st == DAO_GLOBAL_CONSTANT ) goto FieldNoPermit;
	dest = NS->variables->items.pVar[id];
	if( DaoValue_Move( value, & dest->value, dest->dtype ) ==0 ) goto TypeNotMatching;
	return 0;
FieldNotExist:
	DaoProcess_RaiseError( proc, "Field::NotExist", name->value->chars );
	return DAO_ERROR_FIELD_ABSENT;
FieldNoPermit:
	DaoProcess_RaiseError( proc, "Field::NotPermit", name->value->chars );
	return DAO_ERROR_FIELD_HIDDEN;
TypeNotMatching:
	DaoProcess_RaiseError( proc, "Type", "not matching" );
	return DAO_ERROR_TYPE;
InvalidField:
	DaoProcess_RaiseError( proc, "Field", name->value->chars );
	return DAO_ERROR_FIELD;
}


DaoTypeCore daoNamespaceCore =
{
	"namespace",                                           /* name */
	sizeof(DaoNamespace),                                  /* size */
	{ NULL },                                              /* bases */
	{ NULL },                                              /* casts */
	NULL,                                                  /* numbers */
	NULL,                                                  /* methods */
	DaoNamespace_CheckGetField,  DaoNamespace_DoGetField,  /* GetField */
	DaoNamespace_CheckSetField,  DaoNamespace_DoSetField,  /* SetField */
	NULL,                        NULL,                     /* GetItem */
	NULL,                        NULL,                     /* SetItem */
	NULL,                        NULL,                     /* Unary */
	NULL,                        NULL,                     /* Binary */
	NULL,                        NULL,                     /* Conversion */
	NULL,                        NULL,                     /* ForEach */
	NULL,                                                  /* Print */
	NULL,                                                  /* Slice */
	NULL,                                                  /* Compare */
	NULL,                                                  /* Hash */
	NULL,                                                  /* Create */
	NULL,                                                  /* Copy */
	(DaoDeleteFunction) DaoNamespace_Delete,               /* Delete */
	NULL                                                   /* HandleGC */
};
