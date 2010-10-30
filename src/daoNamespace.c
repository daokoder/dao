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
	"cmodule", & baseCore, NULL, NULL, {0}, 
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
	DValue *value = NULL;
	DNode *node = NULL;
	if( ( node = MAP_Find( self->varIndex, name ) ) )
		value = self->varData->data + node->value.pInt;
	if( ( node = MAP_Find( self->cstIndex, name ) ) )
		value = self->cstData->data + node->value.pInt;

	if( value == NULL ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "invalid field" );
		return;
	}
	DaoContext_PutValue( ctx, *value );
}
static void DNS_SetField( DValue *self0, DaoContext *ctx, DString *name, DValue value )
{
	DaoNameSpace *self = self0->v.ns;
	DaoNameSpace_SetData( self, name, value );
}
static void DNS_GetItem( DValue *self0, DaoContext *ctx, DValue pid )
{
}
static void DNS_SetItem( DValue *self0, DaoContext *ctx, DValue pid, DValue value )
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

#if 0
static void NS_AddConst( DaoNameSpace *self, const char *name, DValue data )
{
	DString *s = DString_New(1);
	DaoBase *dbase = (DaoBase*)data;
	dbase->subType |= DAO_DATA_CONST;
	DString_SetMBS( s, name );
	DaoNameSpace_AddConst( self, s, dbase );
	DString_Delete( s );
}
#endif
DaoNameSpace* DaoNameSpace_GetNameSpace( DaoNameSpace *self, const char *name )
{
	DValue value = { DAO_NAMESPACE, 0, 0, 0, {0} };
	DaoNameSpace *ns;
	DString *mbs = DString_New(1);
	DString_SetMBS( mbs, name );
	ns = DaoNameSpace_FindNameSpace( self, mbs );
	if( ns == NULL ){
		ns = DaoNameSpace_New( self->vmSpace );
		DArray_Append( ns->parents, self );
		DaoNameSpace_SetName( ns, name );
		value.v.ns = ns;
		DaoNameSpace_AddConst( self, mbs, value );
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
		abtp = DaoParser_ParseTypeName( t, self, NULL, NULL ); /* XXX warn */
		DaoParser_Delete( parser );
	}
	/*
	   printf( "%s %s\n", name, abtp->name->mbs );
	 */
	DString_SetMBS( name, s );
	DaoNameSpace_AddVariable( self, name, v, abtp );
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
#if 0
static void NS_AddConstString( DaoNameSpace *self0, const char *name, const char *cst )
{
	DaoNameSpace *self = (DaoNameSpace*)self0;
	DString *s = DString_New(1);
	DaoBase *dbase = (DaoBase*)DaoString_New();
	dbase->subType |= DAO_DATA_CONST;
	DString_SetMBS( STRV( dbase ), cst );
	DString_SetMBS( s, name );
	DaoNameSpace_AddConst( self, s, dbase );
	DString_Delete( s );
}
#endif
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
		DaoNameSpace_AddConst( self, s, value );
		i ++;
	}
	DString_Delete( s );
}
void DaoNameSpace_AddConstValue( DaoNameSpace *self, const char *name, DValue value )
{
	DString *s = DString_New(1);
	DString_SetMBS( s, name );
	DaoNameSpace_AddConst( self, s, value );
	DString_Delete( s );
}
void DaoNameSpace_AddConstData( DaoNameSpace *self, const char *name, DaoBase *data )
{
	DString *s = DString_New(1);
	DValue value = { 0, 0, 0, 0, {0} };
	value.t = data->type;
	value.v.p = data;
	DString_SetMBS( s, name );
	DaoNameSpace_AddConst( self, s, value );
	DString_Delete( s );
}
static void DaoRoutine_GetSignature( DaoType *rt, DString *sig )
{
	DaoType *it;
	int i;
	DString_Clear( sig );
	DString_ToMBS( sig );
	for(i=((rt->attrib & DAO_ROUT_PARSELF)!=0); i<rt->nested->size; i++){
		it = rt->nested->items.pAbtp[i];
		if( sig->size ) DString_AppendChar( sig, ',' );
		if( it->tid == DAO_PAR_NAMED || it->tid == DAO_PAR_DEFAULT ){
			DString_Append( sig, it->X.abtype->name );
		}else{
			DString_Append( sig, it->name );
		}
	}
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
		values = DHash_New( D_STRING, D_VALUE );
		typer->priv->values = values;
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
int DaoNameSpace_SetupMethods( DaoNameSpace *self, DaoTypeBase *typer )
{
	DaoParser *parser;
	DaoFunction *cur;
	DString *name1, *name2;
	DArray *parents;
	DMap *methods;
	DMap *supMethods;
	DNode *node;
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
		methods = DHash_New( D_STRING, 0 );
		typer->priv->methods = methods;
		size = 0;
		name1 = DString_New(1);
		name2 = DString_New(1);
		parser = DaoParser_New();
		parser->vmSpace = self->vmSpace;
		parser->nameSpace = self;
		parser->hostCData = typer->priv->abtype;

		if( typer->funcItems != NULL ){
			while( typer->funcItems[ size ].proto != NULL ) size ++;
		}

		for( i=0; i<size; i++ ){
			cur = DaoNameSpace_ParsePrototype( self, typer->funcItems[i].proto, parser );
			if( cur == NULL ){
				printf( "  In function: %s::%s\n", typer->name, typer->funcItems[i].proto );
				continue;
			}
			cur->pFunc = typer->funcItems[i].fpter;
			cur->tidHost = DAO_CDATA;
			cur->routHost = typer->priv->abtype;
			GC_IncRC( cur->routHost );
			if( self->vmSpace->safeTag ) cur->attribs |= DAO_ROUT_EXTFUNC;

			node = MAP_Find( methods, cur->routName );
			if( node !=NULL ){
				DRoutine_AddOverLoad( (DRoutine*) node->value.pVoid, (DRoutine*) cur );
			}else{
				GC_IncRC( cur ); /* there is an entry in the structure */
				MAP_Insert( methods, cur->routName, cur );
			}
		}
		parents = DArray_New(0);
		DaoTypeBase_Parents( typer, parents );
		for(i=1; i<parents->size; i++){
			DaoTypeBase *sup = (DaoTypeBase*) parents->items.pVoid[i];
			supMethods = sup->priv->methods;
			for(it=DMap_First(supMethods); it; it=DMap_Next(supMethods, it)){
				DRoutine *rep = NULL;
				cur = (DaoFunction*) it->value.pVoid;
				/* skip constructor */
				if( STRCMP( cur->routName, sup->name ) ==0 ) continue;
				/* skip methods not defined in this parent type */
				if( cur->routHost != sup->priv->abtype ) continue;
				node = MAP_Find( methods, cur->routName );
				if( node == NULL ){
					rep = DRoutine_New(); /* a dummy function */
					DString_Assign( rep->routName, cur->routName );
					rep->tidHost = DAO_CDATA;
					rep->routHost = typer->priv->abtype;
					rep->routType = cur->routType;
					GC_IncRC( rep->routHost );
					GC_IncRC( rep->routType );
					GC_IncRC( rep );
					MAP_Insert( methods, rep->routName, rep );
				}else{
					rep = (DRoutine*) node->value.pVoid;
				}
				for( k=0; k<cur->routTable->size; k++){
					DRoutine *rt = (DRoutine*)cur->routTable->items.pBase[k];
					if( rt->routHost != sup->priv->abtype ) continue;
					DRoutine_AddOverLoad( rep, rt );
				}
			}
		}
		DArray_Delete( parents );
		DaoParser_Delete( parser );

		assert( DAO_ROUT_MAIN < (1<<DVM_MOVE) );
		for(i=DVM_MOVE; i<=DVM_BITRIT; i++){
			DString_SetMBS( name1, daoBitBoolArithOpers[i-DVM_MOVE] );
			if( DMap_Find( methods, name1 ) == NULL ) continue;
			typer->priv->attribs |= DAO_OPER_OVERLOADED | (1<<i);
		}
		DString_Delete( name1 );
		DString_Delete( name2 );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & dao_msetup_mutex );
#endif
	return 1;
}

int DaoNameSpace_TypeDefine( DaoNameSpace *self, const char *old, const char *type )
{
	DaoNameSpace *ns;
	DaoType *tp, *tp2;
	DString *name = DString_New(1);
	int i;
	DString_SetMBS( name, old );
	tp = DaoNameSpace_FindType( self, name );
	if( tp == NULL && (i=DString_FindMBS( name, "::", 0 )) != MAXSIZE ){
		DString_SetDataMBS( name, old, i );
		ns = DaoNameSpace_FindNameSpace( self, name );
		if( ns ){
			DString_SetMBS( name, old + i + 2 );
			tp = DaoNameSpace_FindType( ns, name );
		}
	}
	if( tp == NULL ) tp = DaoParser_ParseTypeName( old, self, 0, 0 );
	/* printf( "ns = %p  tp = %p  name = %s\n", self, tp, type ); */
	DString_SetMBS( name, type );
	tp2 = DaoNameSpace_FindType( self, name );
	DString_Delete( name );
	if( tp == NULL || tp2 != NULL ) return 0;
	tp = DaoType_Copy( tp );
	DString_SetMBS( tp->name, type );
	DaoNameSpace_AddType( self, tp->name, tp );
	return 1;
}
DaoCDataCore* DaoCDataCore_New();
extern void DaoTypeCData_SetMethods( DaoTypeBase *self );

static FuncPtrTest DaoTypeBase_GetDeleteTest( DaoTypeBase *typer )
{
	FuncPtrTest fptr;
	int i;
	if( typer->DelTest ) return typer->DelTest;
	for(i=0; i<DAO_MAX_CDATA_SUPER; i++){
		if( typer->supers[i] == NULL ) break;
		fptr = DaoTypeBase_GetDeleteTest( typer->supers[i] );
		if( fptr ) return fptr;
	}
	return NULL;
}
static int DaoNameSpace_WrapType2( DaoNameSpace *self, DaoTypeBase *typer )
{
	DaoType *abtype;
	DaoCDataCore *plgCore;
	DString *s;
	DValue value = daoNullCData;

	if( typer->priv ) return 1;
	plgCore = DaoCDataCore_New();
	s = DString_New(1);

	DString_SetMBS( s, typer->name );
	/* Add it before preparing methods, since itself may appear in parameter lists: */
	value.v.cdata = DaoCData_New( typer, NULL );
	DaoNameSpace_AddConst( self, s, value );
	abtype = DaoNameSpace_MakeType( self, s->mbs, DAO_CDATA, value.v.p, NULL, 0 );
	DaoNameSpace_AddType( self, s, abtype );
	DString_Delete( s );

	plgCore->abtype = abtype;
	plgCore->nspace = self;
	plgCore->DelData = typer->Delete;
	plgCore->DelTest = DaoTypeBase_GetDeleteTest( typer );
	typer->priv = (DaoTypeCore*)plgCore;
	DaoTypeCData_SetMethods( typer );
	return 1;
}
int DaoNameSpace_WrapType( DaoNameSpace *self, DaoTypeBase *typer )
{
	if( typer->priv == NULL ) DArray_Append( self->cmodule->ctypers, typer );
	DaoNameSpace_WrapType2( self, typer );
	/*
	   if( DaoNameSpace_SetupValues( self, typer ) == 0 ) return 0;
	   if( setup ) return DaoNameSpace_SetupType( self, typer );
	 */
	return 1;
}
int DaoNameSpace_SetupType( DaoNameSpace *self, DaoTypeBase *typer )
{
	DMap *methods;
	DNode *it;
	if( DaoNameSpace_SetupValues( self, typer ) == 0 ) return 0;
	if( DaoNameSpace_SetupMethods( self, typer ) == 0 ) return 0;
	methods = typer->priv->methods;
	for(it=DMap_First(methods); it; it=DMap_Next(methods, it)){
		GC_IncRC( it->value.pBase );
		DArray_Append( self->cmodule->cmethods, it->value.pVoid );
	}
	return 1;
}
int DaoNameSpace_WrapTypes( DaoNameSpace *self, DaoTypeBase *typers[] )
{
	int i, e = 0;
	for(i=0; typers[i]; i++ ){
		if( typers[i]->priv == NULL ) DArray_Append( self->cmodule->ctypers, typers[i] );
		DaoNameSpace_WrapType2( self, typers[i] );
		/* e |= ( DaoNameSpace_SetupValues( self, typers[i] ) == 0 ); */
	}
	/* if( setup ) return DaoNameSpace_SetupTypes( self, typers ); */
	return e = 0;
}
int DaoNameSpace_TypeDefines( DaoNameSpace *self, const char *alias[] )
{
	int rc = 1;
	if( alias ){
		int i = 0;
		while( alias[i] && alias[i+1] ){
			if( DaoNameSpace_TypeDefine( self, alias[i], alias[i+1] ) ==0 ) rc = 0;
			i += 2;
		}
	}
	return rc;
}
int DaoNameSpace_SetupTypes( DaoNameSpace *self, DaoTypeBase *typers[] )
{
	DMap *methods;
	DNode *it;
	int i, e = 0;
	for(i=0; typers[i]; i++ ){
		e |= ( DaoNameSpace_SetupMethods( self, typers[i] ) == 0 );
		methods = typers[i]->priv->methods;
		for(it=DMap_First(methods); it; it=DMap_Next(methods, it)){
			GC_IncRC( it->value.pBase );
			DArray_Append( self->cmodule->cmethods, it->value.pVoid );
		}
	}
	return e == 0;
}
DaoFunction* DaoNameSpace_MakeFunction( DaoNameSpace *self, 
		const char *proto, DaoParser *parser )
{
	DaoFunction *func = DaoNameSpace_ParsePrototype( self, proto, parser );
	DValue val = daoNullValue;

	if( func == NULL ) return NULL;
	val = DaoNameSpace_GetData( self, func->routName );
	if( val.t == DAO_ROUTINE || val.t == DAO_FUNCTION ){
		DRoutine_AddOverLoad( (DRoutine*) val.v.p, (DRoutine*)func );
	}else{
		val = daoNullFunction;
		val.v.func = func;
		DaoNameSpace_AddConst( self, func->routName, val );
	}
	return func;
}
int DaoNameSpace_WrapFunction( DaoNameSpace *self, DaoFuncPtr fptr, const char *proto )
{
	DaoFunction *func;
	func = DaoNameSpace_MakeFunction( self, proto, NULL );
	if( func == NULL ) return 0;
	func->pFunc = fptr;
	return 1;
}

int DaoNameSpace_WrapFunctions( DaoNameSpace *self, DaoFuncItem *items )
{
	DaoParser *parser = DaoParser_New();
	DaoFunction *func;
	int i = 0;
	parser->vmSpace = self->vmSpace;
	parser->nameSpace = self;
	while( items[i].fpter != NULL ){
		func = DaoNameSpace_MakeFunction( self, items[i].proto, parser );
		if( func == NULL ) break;
		func->pFunc = (DaoFuncPtr)items[i].fpter;
		i ++;
	}
	DaoParser_Delete( parser );
	return (items[i].fpter == NULL);
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

DaoTypeBase nsTyper=
{
	"namespace", & nsCore, NULL, NULL, {0}, 
	(FuncPtrDel) DaoNameSpace_Delete, NULL
};

DaoNameSpace* DaoNameSpace_New( DaoVmSpace *vms )
{
	int i = 0;
	DNode *node;
	DValue value = daoNullValue;
	DString *name = DString_New(1);
	DaoNameSpace *self = (DaoNameSpace*) dao_malloc( sizeof(DaoNameSpace) );
	DaoBase_Init( self, DAO_NAMESPACE );
	self->vmSpace = vms;
	self->cstUser = 0;
	self->mainRoutine = NULL;
	self->parents = DArray_New(0);
	self->cstData  = DVarray_New();
	self->varData  = DVarray_New();
	self->varType  = DArray_New(0);
	self->mainRoutines  = DArray_New(0);
	self->definedRoutines  = DArray_New(0);
	self->nsLoaded  = DArray_New(0);
	self->cmodule = DaoCModule_New();
	self->cstIndex = DHash_New(D_STRING,0);
	self->varIndex = DHash_New(D_STRING,0);
	self->cstStatic = DHash_New(D_STRING,0);
	self->varStatic = DHash_New(D_STRING,0);
	self->macros   = DHash_New(D_STRING,0);
	self->abstypes = DHash_New(D_STRING,0);
	self->argParams = DaoList_New();
	self->time = 0;
	self->file = DString_New(1);
	self->path = DString_New(1);
	self->name = DString_New(1);
	self->sources = DArray_New(D_ARRAY);
	self->tokens = DHash_New(D_STRING,0);
	self->udfType1 = DaoType_New( "?", DAO_UDF, 0,0 );
	self->udfType2 = DaoType_New( "?", DAO_UDF, 0,0 );
	GC_IncRC( self->udfType1 );
	GC_IncRC( self->udfType2 );
	GC_IncRC( self->cmodule );

	DString_SetMBS( name, "null" ); 
	DaoNameSpace_AddConst( self, name, value );
	DVarray_Append( self->cstData, daoNullValue );

	value.t = DAO_STREAM;
	value.v.stream = vms->stdStream;
	DString_SetMBS( name, "io" ); 
	DaoNameSpace_AddConst( self, name, value );
	if( vms->thdMaster ){
		value.t = DAO_THDMASTER;
		value.v.p = (DaoBase*) vms->thdMaster;
		DString_SetMBS( name, "mtlib" ); 
		DaoNameSpace_AddConst( self, name, value );
	}

	DString_SetMBS( name, "exceptions" );
	value.t = DAO_LIST;
	value.v.list = DaoList_New();
	DaoNameSpace_AddVariable( self, name, value, NULL );

	self->vmpEvalConst = DaoVmProcess_New(vms);
	self->routEvalConst = DaoRoutine_New();
	self->routEvalConst->nameSpace = self;
	self->routEvalConst->routType = dao_routine;
	GC_IncRC( dao_routine );
	GC_IncRC( self );
	DaoVmProcess_PushRoutine( self->vmpEvalConst, self->routEvalConst );
	self->vmpEvalConst->topFrame->context->trait |= DAO_DATA_CONST;
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
		DaoNameSpace_AddConst( self, vms->nsInternal->name, value );
		DArray_Append( self->parents, vms->nsInternal );
	}
	return self;
}
void DaoNameSpace_Delete( DaoNameSpace *self )
{
	/* printf( "DaoNameSpace_Delete  %s\n", self->name->mbs ); */
	DaoTypeCore *core;
	DMap *values;
	DNode *it;
	int i, j;
	it = DMap_First( self->macros );
	for( ; it !=NULL; it = DMap_Next(self->macros, it) ) GC_DecRC( it->value.pBase );
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

	GC_DecRC( self->udfType1 );
	GC_DecRC( self->udfType2 );
	GC_DecRCs( self->varType );
	DVarray_Delete( self->cstData );
	DVarray_Delete( self->varData );
	DArray_Delete( self->varType );
	DArray_Delete( self->parents );
	/* no need for GC, because these namespaces are indirectly
	 * referenced through functions. */
	DArray_Delete( self->nsLoaded );
	DArray_Delete( self->mainRoutines );
	DArray_Delete( self->definedRoutines );
	DMap_Delete( self->cstIndex );
	DMap_Delete( self->varIndex );
	DMap_Delete( self->cstStatic );
	DMap_Delete( self->varStatic );
	DMap_Delete( self->macros );
	DMap_Delete( self->abstypes );
	DString_Delete( self->file );
	DString_Delete( self->path );
	DString_Delete( self->name );
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
int  DaoNameSpace_FindConst( DaoNameSpace *self, DString *name )
{
	DNode *node = DMap_Find( self->cstIndex, name );
	int i, n = self->parents->size;
	if( node ) return (int)node->value.pInt;
	for(i=0; i<n; i++){
		DaoNameSpace *ns = self->parents->items.pNS[i];
		int k = DaoNameSpace_FindConst( ns, name );
		if( k >=0 ) return DaoNameSpace_AddConst( self, name, ns->cstData->data[k] );
	}
	return -1;
}
int DaoNameSpace_AddConst( DaoNameSpace *self, DString *name, DValue value )
{
	DValue p;
	DNode *node;
	int id = 0;

	node = MAP_Find( self->cstIndex, name );
	if( node ){
		id = (int)node->value.pInt;
		p = self->cstData->data[id];
		if( p.t == value.t && ( p.t ==DAO_ROUTINE || p.t ==DAO_FUNCTION ) ){
			DRoutine_AddOverLoad( (DRoutine*) p.v.routine, (DRoutine*) value.v.p );
		}else{
			DValue_SimpleMove( value, self->cstData->data + id );
			DValue_MarkConst( & self->cstData->data[id] );
		}
	}else{
		id = self->cstData->size;
		MAP_Insert( self->cstIndex, name, id ) ;
		DVarray_Append( self->cstData, daoNullValue );
		DValue_SimpleMove( value, & self->cstData->data[id] );
		DValue_MarkConst( & self->cstData->data[id] );
	}
	return id;
}
void DaoNameSpace_SetConst( DaoNameSpace *self, int id, DValue value )
{
	DValue_SimpleMove( value, self->cstData->data + id );
	DValue_MarkConst( & self->cstData->data[id] );
}
DValue DaoNameSpace_GetConst( DaoNameSpace *self, int i )
{
	if( i <0 || i >= self->cstData->size ) return daoNullValue;
	return self->cstData->data[i];
}
int  DaoNameSpace_FindVariable( DaoNameSpace *self, DString *name )
{
	DNode *node = MAP_Find( self->varIndex, name );
	int i, k, n = self->parents->size;
	if( node ) return (int)node->value.pInt;
	for(i=0; i<n; i++){
		DaoNameSpace *ns = self->parents->items.pNS[i];
		DaoType *type;
		DValue value;
		if( (k = DaoNameSpace_FindVariable( ns, name )) < 0 ) continue;
		type = ns->varType->items.pAbtp[k];
		value = ns->varData->data[k];
		return DaoNameSpace_AddVariable( self, name, value, type );
	}
	return -1;
}
int DaoNameSpace_AddVariable( DaoNameSpace *self, DString *name, DValue value, DaoType *tp )
{
	DaoType *type, *abtp = DaoNameSpace_GetTypeV( self, value );
	DNode *node;
	int id = 0;

	if( tp && tp->tid <= DAO_DOUBLE ) value.t = tp->tid;
	if( abtp && tp && DaoType_MatchTo( abtp, tp, 0 ) == 0 ){
		printf( "unmatched type: %s %s\n", abtp->name->mbs, tp->name->mbs );
		return -1; /*XXX better way to handle */
	}
	if( tp == NULL ) tp = abtp;

	node = MAP_Find( self->varIndex, name );
	if( node ){
		id = (int)node->value.pInt;
		type = self->varType->items.pAbtp[ id ];
		if( type && tp && DaoType_MatchTo( tp, type, 0 ) == 0 ){
			printf( "unmatched type2\n" );
			return -1; /*XXX better way to handle */
		}
		DValue_Move( value, self->varData->data + id, type );
	}else{
		/*
		   printf("%p; %s : %p ; %i\n", self, name->mbs, dbase, dbase->type );
		 */
		id = self->varData->size;
		MAP_Insert( self->varIndex, name, id ) ;
		DVarray_Append( self->varData, value );
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
	DaoType *type = self->varType->items.pAbtp[ index ];
	return DValue_Move( value, self->varData->data + index, type );
}
DValue DaoNameSpace_GetVariable( DaoNameSpace *self, int i )
{
	if( i <0 || i >= self->varData->size ) return daoNullValue;
	return self->varData->data[i];
}
void DaoNameSpace_SetData( DaoNameSpace *self, DString *name, DValue value )
{
	DNode *node = NULL;
	if( ( node = MAP_Find( self->varIndex, name ) ) ){
		DaoType *type = self->varType->items.pAbtp[ node->value.pInt ];
		DValue_Move( value, self->varData->data + node->value.pInt, type ); /*XXX return*/
		return;
	}
	DaoNameSpace_AddVariable( self, name, value, NULL );
}
DValue DaoNameSpace_GetData( DaoNameSpace *self, DString *name )
{
	int id;
	id = DaoNameSpace_FindVariable( self, name );
	if( id >=0 ) return self->varData->data[id];
	id = DaoNameSpace_FindConst( self, name );
	if( id >=0 ) return self->cstData->data[id];
	return daoNullValue;
}
DaoClass* DaoNameSpace_FindClass( DaoNameSpace *self, DString *name )
{
	DValue val;
	DNode *node = MAP_Find( self->cstIndex, name );
	if( ! node ) return NULL;
	val = self->cstData->data[ node->value.pInt ];
	if( val.t == DAO_CLASS ) return val.v.klass;
	return NULL;
}
DaoNameSpace* DaoNameSpace_FindNameSpace( DaoNameSpace *self, DString *name )
{
	DValue val = daoNullValue;
	DNode *node = MAP_Find( self->cstIndex, name );
	int i, n = self->parents->size;
	if( node ) val = self->cstData->data[ node->value.pInt ];
	if( val.t == DAO_NAMESPACE ) return (DaoNameSpace*) val.v.p;
	for(i=0; i<n; i++){
		DaoNameSpace *ns = self->parents->items.pNS[i];
		ns = DaoNameSpace_FindNameSpace( ns, name );
		if( ns ) return ns;
	}
	return NULL;
}
void DaoNameSpace_AddMacro( DaoNameSpace *self, DString *name, DaoMacro *macro )
{
	DNode *node = MAP_Find( self->macros, name );
	if( node == NULL ){
		GC_IncRC( macro );
		MAP_Insert( self->macros, name, macro );
	}else{
		DaoMacro *m2 = (DaoMacro*) node->value.pVoid;
		GC_IncRC( macro );
		DArray_Append( m2->macroList, macro );
	}
}
void DaoNameSpace_AddParent( DaoNameSpace *self, DaoNameSpace *parent )
{
	DValue value = { DAO_NAMESPACE, 0, 0, 0, {0} };
	value.v.ns = parent;
	DArray_Append( self->parents, parent );
	DaoNameSpace_AddConst( self, parent->name, value );
}
void DaoNameSpace_Import( DaoNameSpace *self, DaoNameSpace *ns, DArray *varImport )
{
	DArray *array = NULL;
	size_t *found = NULL;
	DaoType **vtype = ns->varType->items.pAbtp;
	DArray *names = DArray_New(D_STRING);
	DNode *node, *search;
	int k;

	if( varImport && varImport->size > 0 ){
		array = DArray_New(0);
		DArray_Resize( array, varImport->size, (void*)1 );
		found = array->items.pSize;
		for( k=0; k<varImport->size; k++){
			DString *name = varImport->items.pString[k];
			int c1 = DaoNameSpace_FindConst( ns, name );
			int c2 = DaoNameSpace_FindVariable( ns, name );
			if( MAP_Find( ns->cstStatic, name ) ) continue;
			if( MAP_Find( ns->varStatic, name ) ) continue;
			node = MAP_Find( ns->macros, name );
			if( c1 >= 0 ){
				int c3 = DaoNameSpace_FindConst( self, name );
				DValue p1 = ns->cstData->data[ c1 ];
				if( c3 >= 0 ){
					DValue p3 = self->cstData->data[ c3 ];
					if( p1.t == p3.t && ( p1.t == DAO_ROUTINE || p1.t == DAO_FUNCTION ) ){
						DRoutine_AddOverLoad( (DRoutine*)p3.v.p, (DRoutine*)p1.v.p );
					}else{
						DaoNameSpace_AddConst( self, name, p1 );
					}
				}else{
					DaoNameSpace_AddConst( self, name, p1 );
				}
			}else if( c2 >= 0 ){
				DaoNameSpace_AddVariable( self, name, DaoNameSpace_GetVariable( ns, c1 ), vtype[c2] );
			}else if( node ){
				DaoNameSpace_AddMacro( self, name, (DaoMacro*) node->value.pVoid );
			}else{
				found[k] = 0;
			}
		}
	}else{
		DValue q;
		node = DMap_First( ns->cstIndex );
		for( ; node !=NULL; node = DMap_Next(ns->cstIndex, node ) ){
			DValue p = ns->cstData->data[ node->value.pInt ];
			if( MAP_Find( ns->cstStatic, node->key.pString ) ) continue;
			search = MAP_Find( self->cstIndex, node->key.pString );
			q = search == NULL ? daoNullValue : self->cstData->data[ search->value.pInt ];
			/* import only if the const do not exist, or is NULL */
			if( p.t == DAO_ROUTINE && (p.v.routine->attribs & DAO_ROUT_MAIN) ) continue;
			if( q.t == 0 ){
				DaoNameSpace_AddConst( self, node->key.pString, p );
			}else if( p.t == q.t && ( p.t == DAO_ROUTINE || p.t == DAO_FUNCTION ) ){
				DRoutine_AddOverLoad( (DRoutine*)q.v.p, (DRoutine*)p.v.p );
			}
		}
		node = DMap_First( ns->varIndex );
		for( ; node !=NULL; node = DMap_Next(ns->varIndex, node ) ){
			DValue p = ns->varData->data[ node->value.pInt ];
			if( MAP_Find( ns->cstStatic, node->key.pString ) ) continue;
			search = MAP_Find( self->varIndex, node->key.pString );
			q = search == NULL ? daoNullValue : self->varData->data[ search->value.pInt ];
			/* import only if the variable do not exist, or is NULL */
			if( q.t ==0 ){
				DaoNameSpace_AddVariable( self, node->key.pString, p, vtype[ node->value.pInt ] );
			}
		}
		node = DMap_First( ns->macros );
		for( ; node !=NULL; node = DMap_Next(ns->macros, node ) )
			DaoNameSpace_AddMacro( self, node->key.pString, (DaoMacro*) node->value.pVoid );
	}
	if( varImport ){
		DArray_Swap( names, varImport );
		for( k=0; k<names->size; k++)
			if( ! found[k] ) DArray_Append( varImport, names->items.pString[k] );
	}
	node = DMap_First( ns->abstypes );
	for( ; node !=NULL; node = DMap_Next(ns->abstypes, node ) )
		DaoNameSpace_AddType( self, node->key.pString, node->value.pAbtp );

	DArray_Delete( names );
	if( array ) DArray_Delete( array );
}

DaoType* DaoNameSpace_FindType( DaoNameSpace *self, DString *name )
{
	DNode *node;
	int i, n = self->parents->size;
	if( DString_FindChar( name, '?', 0 ) != MAXSIZE ) return NULL;
	node = MAP_Find( self->abstypes, name );
	if( node ) return node->value.pAbtp;
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
int DaoNameSpace_AddType( DaoNameSpace *self, DString *name, DaoType *tp )
{
	DNode *node = MAP_Find( self->abstypes, name );
	int id = DaoNameSpace_FindConst( self, name );
#if 0
	//XXX no need? if( DString_FindChar( name, '?', 0 ) != MAXSIZE ) return 0;
#endif
	if( node == NULL ){
		MAP_Insert( self->abstypes, name, tp );
		GC_IncRC( tp );
	}
	if( id >=0 ) return 1;
	if( tp->tid == DAO_CLASS || tp->tid == DAO_CDATA ){
		DValue val = daoNullClass;
		val.t = tp->tid;
		val.v.p = tp->X.extra;
		DaoNameSpace_AddConst( self, name, val );
	}else{
		DValue val = daoNullClass;
		val.t = DAO_TYPE;
		val.v.p = (DaoBase*) tp;
		DaoNameSpace_AddConst( self, name, val );
	}
	/*
	   node = DMap_First( self->abstypes );
	   for(; node!=NULL; node=DNode_Next(node)){
	   if( DString_Compare( node->key.pString, node->value.pAbtp->name ) != 0 )
	   printf( ">>>>>>>>>>> %s %s\n", node->key.pString->mbs, 
	   node->value.pAbtp->name->mbs );
	   }
	 */
	return 1;
}

static DaoType *simpleTypes[ DAO_ARRAY ] = { 0, 0, 0, 0, 0, 0, 0 };

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
	case DAO_PAIR :
	case DAO_PAR_NAMED : id = self.v.pair->unitype; break;
	case DAO_TUPLE :  id = self.v.tuple->unitype; break;
	case DAO_OBJECT : id = self.v.object->myClass->objType; break;
	case DAO_CLASS :  id = self.v.klass->clsType; break;
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
	case DAO_NIL :
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
	DaoPair *pair = (DaoPair*) p;
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
	case DAO_CDATA :
		abtp = ((DaoCData*)p)->typer->priv->abtype; break;
	case DAO_ROUTINE :
	case DAO_FUNCTION :
		rout = (DRoutine*) p;
		abtp = rout->routType;
		break;
	case DAO_PAIR :
	case DAO_PAR_NAMED :
		abtp = pair->unitype; break;
	case DAO_TUPLE :
		abtp = tuple->unitype; break;
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
		}else if( p->type == DAO_PAIR ){
			DString_SetMBS( mbs, "pair<" );
			nested = DArray_New(0);
			DArray_Append( nested, DaoNameSpace_GetTypeV( self, pair->first ) );
			DString_Append( mbs, nested->items.pAbtp[0]->name );
			DArray_Append( nested, DaoNameSpace_GetTypeV( self, pair->second ) );
			DString_AppendMBS( mbs, "," );
			DString_Append( mbs, nested->items.pAbtp[1]->name );
			DString_AppendMBS( mbs, ">" );
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
#if 1
			if( zerosize ) abtp->attrib |= DAO_TYPE_EMPTY;
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
			case DAO_PAIR :
				GC_IncRC( abtp );
				pair->unitype = abtp;
				break;
			case DAO_TUPLE :
				GC_IncRC( abtp );
				tuple->unitype = abtp;
				break;
			default : break;
			}
#endif
			/* XXX if( DString_FindChar( abtp->name, '?', 0 ) == MAXSIZE ) */
			DaoNameSpace_AddType( self, abtp->name, abtp );
		}
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
	int i;

	if( tid != DAO_ANY )
		any = DaoNameSpace_MakeType( self, "any", DAO_ANY, 0,0,0 );

	DString_SetMBS( mbs, name );
	if( N > 0 ){
		DString_AppendChar( mbs, '<' );
		DString_Append( mbs, nest[0]->name );
		DArray_Append( nstd, nest[0] );
		for(i=1; i<N; i++){
			DString_AppendChar( mbs, ',' );
			DString_Append( mbs, nest[i]->name );
			DArray_Append( nstd, nest[i] );
		}
		DString_AppendChar( mbs, '>' );
	}else if( tid == DAO_LIST || tid == DAO_ARRAY ){
		DString_AppendMBS( mbs, "<any>" );
		DArray_Append( nstd, any );
	}else if( tid == DAO_MAP ){
		DString_AppendMBS( mbs, "<any,any>" );
		DArray_Append( nstd, any );
		DArray_Append( nstd, any );
	}else if( tid == DAO_CLASS ){
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
	}else if( tid == DAO_PAR_NAMED ){
		DString_AppendMBS( mbs, ":" );
		if( pb->type == DAO_TYPE ) DString_Append( mbs, ((DaoType*)pb)->name );
	}
	node = MAP_Find( self->abstypes, mbs );
	if( node == NULL ){
		tp = DaoType_New( mbs->mbs, tid, pb, nstd );
		if( pb && pb->type == DAO_CDATA ) tp->typer = ((DaoCData*)pb)->typer;
		if( tid == DAO_PAR_NAMED ){
			tp->fname = DString_New(1);
			DString_SetMBS( tp->fname, name );
		}
		DaoNameSpace_AddType( self, tp->name, tp );
	}else{
		tp = node->value.pAbtp;
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

	DString_AppendMBS( abtp->name, "routine<" );
	for(i=0; i<routype->nested->size; i++){
		if( i >0 ) DString_AppendMBS( abtp->name, "," );
		tp = tp2 = routype->nested->items.pAbtp[i];
		if( tp && (tp->tid == DAO_PAR_NAMED || tp->tid == DAO_PAR_DEFAULT) ){
			ch = tp->name->mbs[tp->fname->size];
			tp2 = tp->X.abtype;
		}
		if( tp2 && tp2->tid ==DAO_UDF ){
			if( vals ){
				tp2 = DaoNameSpace_GetTypeV( self, vals[i] );
			}else if( types && types[i] ){
				tp2 = types[i];
			}
		}
		/* XXX typing DString_AppendMBS( abtp->name, tp ? tp->name->mbs : "..." ); */
		if( tp2 != tp && tp2 != tp->X.abtype ){
			fname = tp->fname;
			tp = DaoType_New( fname->mbs, tp->tid, (DaoBase*) tp2, NULL );
			DString_AppendChar( tp->name, ch );
			DString_Append( tp->name, tp2->name );
			tp->fname = DString_Copy( fname );
		}
		DString_Append( abtp->name, tp->name );
		DArray_Append( abtp->nested, tp );
	}
	tp = retp ? retp : routype->X.abtype;
	if( tp ){
		DString_AppendMBS( abtp->name, "=>" );
		DString_Append( abtp->name, tp->name );
	}
	DString_AppendMBS( abtp->name, ">" );
	abtp->X.abtype = tp;
	GC_IncRC( abtp->X.abtype );
	GC_IncRCs( abtp->nested );
	node = MAP_Find( self->abstypes, abtp->name );
	if( node ){
		DaoType_Delete( abtp );
		return node->value.pAbtp;
	}
	DaoType_CheckAttributes( abtp );
	DaoNameSpace_AddType( self, abtp->name, abtp );
	return abtp;
}

DaoFunction* DaoNameSpace_ParsePrototype( DaoNameSpace *self, const char *proto, DaoParser *parser )
{
	DaoFunction *func = DaoFunction_New();
	DaoVmSpace *vms = self->vmSpace;
	DaoParser *pp = parser;
	int key = DKEY_OPERATOR;

	GC_IncRC( self );
	func->nameSpace = self;
	if( parser == NULL ){
		parser = DaoParser_New();
		parser->vmSpace = vms;
		parser->nameSpace = self;
	}
	if( ! DaoToken_Tokenize( parser->tokens, proto, 0, 0, 0 ) ) goto Error;
	if( parser->tokens->size ==0 ) goto Error;
	if( parser->tokens->items.pToken[0]->type == DTOK_IDENTIFIER ) key = 0;
	DArray_Clear( parser->partoks );

	parser->routine = (DaoRoutine*) func; /* safe to parse params only */
	if( DaoParser_ParsePrototype( parser, parser, key, 0 ) < 0 ) goto Error;
	if( DaoParser_ParseParams( parser ) == 0 ) goto Error;
	if( parser != pp ) DaoParser_Delete( parser );
	return func;
Error:
	if( parser != pp ) DaoParser_Delete( parser );
	DArray_Clear( func->routTable ); /* routTable contains func */
	DaoFunction_Delete( func );
	return NULL;
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
			DMap_Insert( mapNames, node->key.pVoid, (void*)(1<<mapNames->size) );
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
			DMap_Insert( mapNames, node->key.pVoid, (void*)(1<<mapNames->size) );
		}
		DaoNameSpace_AddType( self, name, type );
	}
	DString_Delete( name );
	return type;
}
