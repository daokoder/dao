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

#include"string.h"

#include"daoConst.h"
#include"daoRoutine.h"
#include"daoContext.h"
#include"daoGC.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoStream.h"
#include"daoParser.h"
#include"daoProcess.h"
#include"daoVmspace.h"
#include"daoRegex.h"
#include"daoNumtype.h"
#include"daoNamespace.h"

static void DRoutine_Init( DRoutine *self )
{
	self->minimal = 0;
	self->attribs = 0;
	self->parCount = 0;
	self->tidHost = 0;
	self->routHost = NULL;
	self->routType = NULL;
	self->routHelp = NULL;
	self->routName   = DString_New(1);
	self->parCodes   = DString_New(1);
	self->routConsts = DVarray_New();
	self->routTable = DArray_New(0);
	self->nameSpace = NULL;
}
DRoutine* DRoutine_New()
{
	DRoutine *self = (DRoutine*) dao_calloc( 1, sizeof(DRoutine) );
	DaoBase_Init( self, DAO_ROUTINE );
	DRoutine_Init( self );
	self->minimal = 1;
	return self;
}
void DRoutine_CopyFields( DRoutine *self, DRoutine *from )
{
	int i;
	self->parCount = from->parCount;
	self->attribs = from->attribs;
	self->tidHost = from->tidHost;
	GC_ShiftRC( from->routHost, self->routHost );
	GC_ShiftRC( from->nameSpace, self->nameSpace );
	GC_ShiftRC( from->routType, self->routType );
	self->routHost = from->routHost;
	self->nameSpace = from->nameSpace;
	self->routType = from->routType;
	DVarray_Assign( self->routConsts, from->routConsts );
	DString_Assign( self->routName, from->routName );
	DString_Assign( self->parCodes, from->parCodes );
	for(i=0; i<from->routConsts->size; i++) DValue_MarkConst( self->routConsts->data + i );
}
static void DRoutine_DeleteFields( DRoutine *self )
{
	GC_DecRCs( self->routTable );
	GC_DecRC( self->routHost );
	GC_DecRC( self->routType );
	GC_DecRC( self->nameSpace );
	DString_Delete( self->routName );
	DString_Delete( self->parCodes );
	DVarray_Delete( self->routConsts );
	DArray_Delete( self->routTable );
	if( self->routHelp ) DString_Delete( self->routHelp );
}
int DRoutine_AddConst( DRoutine *self, DaoBase *dbase )
{
	DValue value = daoNullValue;
	value.v.p = dbase;
	if( dbase ) value.t = dbase->type;
	DVarray_Append( self->routConsts, daoNullValue );
	DValue_SimpleMove( value, & self->routConsts->data[self->routConsts->size-1] );
	DValue_MarkConst( & self->routConsts->data[self->routConsts->size-1] );
	return self->routConsts->size-1;
}
int  DRoutine_AddConstValue( DRoutine *self, DValue value )
{
	DVarray_Append( self->routConsts, daoNullValue );
	DValue_SimpleMove( value, & self->routConsts->data[self->routConsts->size-1] );
	DValue_MarkConst( & self->routConsts->data[self->routConsts->size-1] );
	return self->routConsts->size-1;
}
void DRoutine_AddOverLoad( DRoutine *self, DRoutine *rout )
{
	GC_IncRC( rout );
	if( self->routHost == rout->routHost ){
		DArray_PushFront( self->routTable, rout );
	}else{
		DArray_PushBack( self->routTable, rout );
	}
}
static int DaoDecorator_CheckType( DaoType *decoType, DValue *csts, DaoType *ts[], int np )
{
	DArray *nested = decoType->nested;
	DaoType **stypes, **types = nested->items.pType;
	DaoType *t, *ft, *tp, *routType = ts[0];
	DNode *it, *node;
	DMap *mapNames, *defs = DMap_New(0,0);
	int parpass[DAO_MAX_PARAM];
	float sum = 0, sum2 = 0;
	int i, j, k, match;

	if( np ==0 || nested->size ==0 || np > nested->size ) goto CheckFailed;
	if( csts[0].t == DAO_ROUTINE ) routType = csts[0].v.routine->routType;
	if( routType == NULL || routType->tid != DAO_ROUTINE ) goto CheckFailed;
	ft = types[0];
	stypes = routType->nested->items.pType;
	if( ft->tid != DAO_PAR_NAMED || ft->X.abtype->tid != DAO_ROUTINE ) goto CheckFailed;
	ft = ft->X.abtype;
	if( ft->nested->size > routType->nested->size ) goto CheckFailed;
	mapNames = ft->mapNames;
	for(it=DMap_First(mapNames); it; it=DMap_Next(mapNames,it)){
		tp = ft->nested->items.pType[it->value.pInt];
		node = DMap_Find( routType->mapNames, it->key.pVoid );
		if( node == NULL ) goto CheckFailed;
		match = DaoType_MatchTo( tp, stypes[node->value.pInt], NULL );
		if( match ==0 ) goto CheckFailed;
		sum += match;
	}
	if( mapNames->size ==0 ) sum = 1;
	for(j=0; j<nested->size; j++) parpass[j] = 0;
	k = 1;
	for(j=1; j<np; j++){
		DValue cs = daoNullValue;
		if( csts && csts[j].t ) cs = csts[j];
		if( k < nested->size && types[k]->tid == DAO_PAR_VALIST ){
			for(; k<np; k++) parpass[k] = 1;
			break;
		}
		tp = ts[j];
		if( tp == NULL ) goto CheckFailed;
		if( tp->tid == DAO_PAR_NAMED ){
			node = DMap_Find( decoType->mapNames, tp->fname );
			if( node == NULL ) goto CheckFailed;
			k = node->value.pInt;
			tp = tp->X.abtype;
		}
		if( k > nested->size || tp ==NULL )  goto CheckFailed;
		t = nested->items.pType[k];
		if( t->tid == DAO_PAR_NAMED || t->tid == DAO_PAR_DEFAULT ) t = t->X.abtype;
		if( t->tid ==DAO_ROUTINE && ( cs.t ==DAO_ROUTINE || cs.t ==DAO_FUNCTION ) ){
			DRoutine *rout = DRoutine_GetOverLoadByType( (DRoutine*)cs.v.p, t );
			tp = rout ? rout->routType : NULL;
		}
		match = DaoType_MatchTo( tp, t, defs );

		/*
		   printf( "%p %s %p %s\n", tp->X.extra, tp->name->mbs, t->X.extra, t->name->mbs );
		   printf( "%i:  %i\n", k, match );
		 */

		/* less strict */
		if( tp && match ==0 ){
			if( tp->tid == DAO_ANY && t->tid == DAO_ANY )
				match = DAO_MT_ANY;
			else if( tp->tid == DAO_ANY || tp->tid == DAO_UDF )
				match = DAO_MT_NEGLECT;
		}
		if( match == 0 ) goto CheckFailed;
		sum2 += match;
		parpass[k] = 1;
		k += 1;
	}
	for(j=1; j<nested->size; j++){
		k = types[j]->tid;
		if( k == DAO_PAR_VALIST ) break;
		if( parpass[j] ) continue;
		if( k != DAO_PAR_DEFAULT ) goto CheckFailed;
		parpass[j] = 1;
		sum2 += 1;
	}
	sum = sum / (routType->nested->size + 1) + sum2 / nested->size;
	return 1000*sum;
CheckFailed:
	return -1;
}
static int DRoutine_CheckType( DaoType *routType, DaoNameSpace *ns, DaoType *selftype,
		DValue *csts, DaoType *ts[], int np, int code, int def, int *min, int *spec )
{
	int ndef = 0;
	int i, j, match = 1;
	int ifrom, ito;
	int parpass[DAO_MAX_PARAM];
	int npar = np, size = routType->nested->size;
	int selfChecked = 0, selfMatch = 0;
	DaoType  *abtp, **partypes = routType->nested->items.pType;
	DaoType **tps = ts;
	DValue cs = daoNullValue;
	DRoutine *rout;
	DNode *node;
	DMap *defs;

	if( routType->name->mbs[0] == '@' )
		return DaoDecorator_CheckType( routType, csts, ts, np );

	defs = DMap_New(0,0);
	if( routType->nested ){
		ndef = routType->nested->size;
		if( ndef ){
			abtp = partypes[ ndef-1 ];
			if( abtp->tid == DAO_PAR_VALIST ) ndef = DAO_MAX_PARAM;
		}
	}

	/*
	   printf( "=====================================\n" );
	   for( j=0; j<npar; j++){
	   DaoType *tp = tps[j];
	   if( tp != NULL ) printf( "tp[ %i ]: %s\n", j, tp->name->mbs );
	   }
	   printf( "%s %i %i\n", routType->name->mbs, ndef, npar );
	   if( selftype ) printf( "%i\n", routType->name->mbs, ndef, npar, selftype );
	 */

	*min = DAO_MT_EQ;
	*spec = 0;
	if( (code == DVM_MCALL  || code == DVM_MCALL_TC)
			&& ! ( routType->attrib & DAO_TYPE_SELF ) ){
		npar --;
		tps ++;
	}else if( selftype && ( routType->attrib & DAO_TYPE_SELF)
			&& code != DVM_MCALL && code != DVM_MCALL_TC ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		abtp = partypes[0]->X.abtype;
		selfMatch = DaoType_MatchTo( selftype, abtp, defs );
		if( selfMatch ){
			selfChecked = 1;
			parpass[0] = selfMatch;
			if( selfMatch >= DAO_MT_ANYUDF && selfMatch <= DAO_MT_UDF ){
				*spec = 1;
			}
			if( *min > selfMatch ) *min = selfMatch;
		}
	}
	if( npar == ndef && ndef == 0 ) goto FinishOK;
	if( npar > ndef && (size == 0 || partypes[size-1]->tid != DAO_PAR_VALIST ) ){
		goto FinishError;
	}

	for( j=selfChecked; j<ndef; j++) parpass[j] = 0;
	for(ifrom=0; ifrom<npar; ifrom++){
		DaoType *tp = tps[ifrom];
		ito = ifrom + selfChecked;
		if( ito < ndef && partypes[ito]->tid == DAO_PAR_VALIST ){
			for(; ifrom<npar; ifrom++) parpass[ifrom+selfChecked] = 1;
			break;
		}
		cs = daoNullValue;
		if( csts && csts[ifrom].t ) cs = csts[ifrom];
		if( tp == NULL ) goto FinishError;
		if( tp->tid == DAO_PAR_NAMED ){
			node = DMap_Find( routType->mapNames, tp->fname );
			if( node == NULL ) goto FinishError;
			ito = node->value.pInt;
			tp = tp->X.abtype;
		}
		if( ito > ndef || tp ==NULL )  goto FinishError;
		abtp = routType->nested->items.pType[ito];
		if( abtp->tid == DAO_PAR_NAMED || abtp->tid == DAO_PAR_DEFAULT ) abtp = abtp->X.abtype;
		if( abtp->tid ==DAO_ROUTINE && ( cs.t ==DAO_ROUTINE || cs.t ==DAO_FUNCTION ) ){
			rout = DRoutine_GetOverLoadByType( (DRoutine*)cs.v.p, abtp );
			tp = rout ? rout->routType : NULL;
		}
		parpass[ito] = DaoType_MatchTo( tp, abtp, defs );

		/*
		   printf( "%p %s %p %s\n", tp->X.extra, tp->name->mbs, abtp->X.extra, abtp->name->mbs );
		   printf( "%i:  %i\n", ito, parpass[ito] );
		 */

		/* less strict */
		if( tp && parpass[ito] ==0 ){
			if( tp->tid == DAO_ANY && abtp->tid == DAO_ANY )
				parpass[ito] = DAO_MT_ANY;
			else if( tp->tid == DAO_ANY || tp->tid == DAO_UDF )
				parpass[ito] = DAO_MT_NEGLECT;
		}
		if( parpass[ito] >= DAO_MT_ANYUDF && parpass[ito] <= DAO_MT_UDF ){
			*spec = 1;
		}
		if( parpass[ito] < *min ) *min = parpass[ito];
		if( parpass[ito] == 0 ) goto FinishError;
		if( def ) tps[ifrom] = DaoType_DefineTypes( tp, ns, defs );
	}
	for(ito=0; ito<ndef; ito++){
		i = partypes[ito]->tid;
		if( i == DAO_PAR_VALIST ) break;
		if( parpass[ito] ) continue;
		if( i != DAO_PAR_DEFAULT ) goto FinishError;
		parpass[ito] = 1;
	}
	match = selfMatch;
	for( j=selfChecked; j<ndef; j++ ) match += parpass[j];
	if( npar == 0 && ( ndef==0 || partypes[0]->tid == DAO_PAR_VALIST
				|| partypes[0]->tid == DAO_PAR_DEFAULT ) ) match = 1;

	/*
	   printf( "%s %i\n", routType->name->mbs, *min );
	 */
FinishOK:
	DMap_Delete( defs );
	return match;
FinishError:
	DMap_Delete( defs );
	return -1;
}
DRoutine* DRoutine_GetOverLoadByType( DRoutine *self, DaoType *type )
{
	DRoutine *rout;
	int i, j, k=-1, max = 0;
	for(i=0; i<self->routTable->size; i++){
		rout = (DRoutine*) self->routTable->items.pBase[i];
		if( rout->routType == type ) return rout;
	}
	for(i=0; i<self->routTable->size; i++){
		rout = (DRoutine*) self->routTable->items.pBase[i];
		j = DaoType_MatchTo( rout->routType, type, NULL );
		if( j && j >= max ){
			max = j;
			k = i;
		}
	}
	if( k >=0 ) return (DRoutine*) self->routTable->items.pBase[k];
	return NULL;
}
static DRoutine* MatchByParamType( DRoutine *self, DaoType *selftype,
		DValue *csts, DaoType *ts[], int np, int code, int *min, int *spec )
{
	int i, match;
	int best = -1;
	int max2 = 0;
	int min3=0, spec3=0;
	DaoNameSpace *ns = self->nameSpace;
	DaoRoutine *routine;
	DRoutine *rout;

	*spec = 0;
	for(i=0; i<self->routTable->size; i++){
		rout = (DRoutine*) self->routTable->items.pBase[i];
		/*
		   printf( "=====================================\n" );
		   printf("ovld %i, %p %s : %s\n", i, rout, self->routName->mbs, rout->routType->name->mbs);
		 */
		match = DRoutine_CheckType( rout->routType, ns, selftype, csts, ts, np, code, 0, & min3, & spec3 );
		if( match > max2 ){
			max2 = match;
			best = i;
			*min = min3;
			*spec = spec3;
		}
	}
	/*
	   printf("%s : best = %i, spec = %i\n", self->routName->mbs, best, *spec);
	 */
	if( best < 0 ) return NULL;
	rout = (DRoutine*) self->routTable->items.pBase[best];
	if( rout->type != DAO_ROUTINE || rout->minimal ==1 ) return rout;

	routine = (DaoRoutine*) rout;
	if( routine->parser ) DaoRoutine_Compile( routine );
	if( routine->specialized == NULL ) return rout;
	best = -1;
	for(i=0; i<routine->specialized->size; i++){
		rout = (DRoutine*) routine->specialized->items.pBase[i];
		/*
		   printf( "=====================================\n" );
		   printf("ovld %i, %p %s : %s\n", i, rout, self->routName->mbs, rout->routType->name->mbs);
		 */
		match = DRoutine_CheckType( rout->routType, ns, selftype, csts, ts, np, code, 0, & min3, & spec3 );
		if( match > max2 ){
			max2 = match;
			best = i;
			*min = min3;
			*spec = spec3;
		}
	}
	if( best < 0 ) return (DRoutine*)routine;
	return (DRoutine*) routine->specialized->items.pBase[best];
}
void DRoutine_PassParamTypes( DRoutine *self, DaoType *selftype,
		DValue *csts, DaoType *ts[], int np, int code )
{
	int npar = np;
	int ndef = self->parCount;
	int j, ifrom, ito;
	int selfChecked = 0;
	DaoType **parType = self->routType->nested->items.pType;
	DaoType **tps = ts;
	DaoType  *abtp, *tp;
	DNode *node;
	DMap *defs;
	DMap *mapNames = self->routType->mapNames;
	DValue cs;

	if( npar == ndef && ndef == 0 ) return;
	defs = DMap_New(0,0);
	if( (code == DVM_MCALL  || code == DVM_MCALL_TC)
			&& ! ( self->routType->attrib & DAO_TYPE_SELF ) ){
		npar --;
		tps ++;
	}else if( selftype && ( self->routType->attrib & DAO_TYPE_SELF)
			&& code != DVM_MCALL && code != DVM_MCALL_TC ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		abtp = self->routType->nested->items.pType[0]->X.abtype;
		if( DaoType_MatchTo( selftype, abtp, defs ) ){
			selfChecked = 1;
			DaoType_RenewTypes( selftype, self->nameSpace, defs );
		}
	}
	for(ifrom=0; ifrom<npar; ifrom++){
		ito = ifrom + selfChecked;
		if( ito < ndef && parType[ito]->tid == DAO_PAR_VALIST ) break;
		tp = tps[ifrom];
		cs = daoNullValue;
		if( csts && csts[ifrom].t ==0 ) cs = csts[ifrom];
		if( tp == NULL ) break;
		if( tp->tid == DAO_PAR_NAMED ){
			node = DMap_Find( mapNames, tp->fname );
			if( node == NULL ) break;
			ito = node->value.pInt;
			tp = tp->X.abtype;
		}
		abtp = parType[ito];
		if( ito >= ndef || tp ==NULL || abtp ==NULL )  break;
		if( abtp->tid == DAO_PAR_NAMED || abtp->tid == DAO_PAR_DEFAULT ) abtp = abtp->X.abtype;
		DaoType_MatchTo( tp, abtp, defs );
	}
	abtp = DaoType_DefineTypes( self->routType, self->nameSpace, defs );
	GC_ShiftRC( abtp, self->routType );
	self->routType = abtp;
	/*
	   printf( "tps1: %p %s\n", self->routType, self->routType->name->mbs );
	   for(node=DMap_First(defs);node;node=DMap_Next(defs,node)){
	   printf( "%i  %i\n", node->key.pType->tid, node->value.pType->tid );
	   printf( "%s  %s\n", node->key.pType->name->mbs, node->value.pType->name->mbs );
	   }
	 */
	for( j=0; j<npar; j++){
		tps[j] = DaoType_DefineTypes( tps[j], self->nameSpace, defs );
	}
	if( selftype && ( self->routType->attrib & DAO_TYPE_SELF) ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		abtp = self->routType->nested->items.pType[0];
		if( DaoType_MatchTo( selftype, abtp, defs ) )
			DaoType_RenewTypes( selftype, self->nameSpace, defs );
	}
	if( self->type == DAO_ROUTINE ){
		DaoRoutine *rout = (DaoRoutine*) self;
		for(j=0; j<rout->regType->size; j++){
			abtp = rout->regType->items.pType[j];
			abtp = DaoType_DefineTypes( abtp, rout->nameSpace, defs );
			GC_ShiftRC( abtp, rout->regType->items.pType[j] );
			rout->regType->items.pType[j] = abtp;
		}
	}
	DMap_Delete( defs );
}
DaoType* DRoutine_PassParamTypes2( DRoutine *self, DaoType *selftype,
		DValue *csts, DaoType *ts[], int np, int code )
{
	int npar = np;
	int ndef = self->parCount;
	int j, ifrom, ito;
	int selfChecked = 0;
	DaoType **parType = self->routType->nested->items.pType;
	DaoType **tps = ts;
	DaoType  *abtp, *tp, *abtp2;
	DNode *node;
	DMap *defs;
	DMap *mapNames = self->routType->mapNames;
	DValue cs;
	/*
	printf( "%s %s\n", self->routName->mbs, self->routType->name->mbs );
	*/
	if( npar == ndef && ndef == 0 ) return self->routType->X.abtype;
	if( self->routType->X.abtype == NULL || self->routType->X.abtype->tid < DAO_ARRAY )
		return self->routType->X.abtype;
	defs = DMap_New(0,0);
	if( (code == DVM_MCALL  || code == DVM_MCALL_TC)
			&& ! ( self->routType->attrib & DAO_TYPE_SELF ) ){
		npar --;
		tps ++;
	}else if( selftype && ( self->routType->attrib & DAO_TYPE_SELF)
			&& code != DVM_MCALL && code != DVM_MCALL_TC ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		abtp = self->routType->nested->items.pType[0]->X.abtype;
		if( DaoType_MatchTo( selftype, abtp, defs ) ){
			selfChecked = 1;
			DaoType_RenewTypes( selftype, self->nameSpace, defs );
		}
	}
	for(ifrom=0; ifrom<npar; ifrom++){
		ito = ifrom + selfChecked;
		if( ito < ndef && parType[ito]->tid == DAO_PAR_VALIST ) break;
		tp = tps[ifrom];
		cs = daoNullValue;
		if( csts && csts[ifrom].t ==0 ) cs = csts[ifrom];
		if( tp == NULL ) break;
		if( tp->tid == DAO_PAR_NAMED ){
			node = DMap_Find( mapNames, tp->fname );
			if( node == NULL ) break;
			ito = node->value.pInt;
			tp = tp->X.abtype;
		}
		abtp = parType[ito];
		if( ito >= ndef || tp ==NULL || abtp ==NULL )  break;
		if( abtp->tid == DAO_PAR_NAMED || abtp->tid == DAO_PAR_DEFAULT ) abtp = abtp->X.abtype;
		DaoType_MatchTo( tp, abtp, defs );
	}
	/*
	for(node=DMap_First(defs);node;node=DMap_Next(defs,node))
		printf( "binding:  %s  %s\n", node->key.pType->name->mbs, node->value.pType->name->mbs );
	 */
	abtp2 = DaoType_DefineTypes( self->routType, self->nameSpace, defs );
	abtp2 = abtp2->X.abtype;
	for( j=0; j<npar; j++){
		tps[j] = DaoType_DefineTypes( tps[j], self->nameSpace, defs );
	}
	if( selftype && ( self->routType->attrib & DAO_TYPE_SELF) ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		abtp = self->routType->nested->items.pType[0];
		if( DaoType_MatchTo( selftype, abtp, defs ) )
			DaoType_RenewTypes( selftype, self->nameSpace, defs );
	}
	DMap_Delete( defs );
	return abtp2;
}
static DRoutine* DRoutine_GetOverLoadExt(
		DArray *routTable, DaoClass *filter, DValue *obj, DValue *p[], int n, int code )
{
	float match, max = 0;
	int i, j, m, sum;
	int ifrom, ito;
	int best = -1;
	int dist = 0;
	int mcall = (code == DVM_MCALL  || code == DVM_MCALL_TC);
	int parpass[DAO_MAX_PARAM];
	DaoRoutine *routine;
	DaoBase *filter2 = (DaoBase*) filter;
	DNode *node;
	DMap *defs = NULL;

#if 0
	if( self->routTable->size == 1 ){
		/* self might be a dummy routine created in deriving class methods */
		self = (DRoutine*) self->routTable->items.pBase[0];
		return self;
	}
#endif
#if 0
	if( strcmp( self->routName->mbs, "expand" ) ==0 ){
		for(i=0; i<n; i++) printf( "%i  %i\n", i, p[i]->t );
	}
#endif

	i = 0;
	while( i<routTable->size ){
		DRoutine *rout = (DRoutine*) routTable->items.pBase[i];
		DaoType **parType = rout->routType->nested->items.pType;
		DaoType  *abtp;
		DMap *mapNames = rout->routType->mapNames;
		DValue **dpar = p;
		int ndef = rout->parCount;
		int npar = n;
		int selfChecked = 0, selfMatch = 0;
		if( filter && obj && obj->t == DAO_OBJECT ){
			if( DaoClass_ChildOf( obj->v.object->myClass, filter2 ) ==0 ) goto NextRoutine;
		}
		if( defs == NULL ) defs = DMap_New(0,0);
		/* func();
		 * obj.func();
		 * obj::func();
		 */
		if( (code == DVM_MCALL  || code == DVM_MCALL_TC)
				&& ! ( rout->routType->attrib & DAO_TYPE_SELF ) ){
			npar --;
			dpar ++;
		}else if( obj && obj->t && ( rout->routType->attrib & DAO_TYPE_SELF)
				&& code != DVM_MCALL && code != DVM_MCALL_TC ){
			/* class DaoClass : CppClass{ cppmethod(); }
			 * use stdio;
			 * print(..);
			 */
			abtp = parType[0]->X.abtype;
			selfMatch = DaoType_MatchValue( abtp, *obj, defs );
			if( selfMatch ){
				parpass[0] = selfMatch;
				selfChecked = 1;
			}
		}
		/*
		   if( strcmp( rout->routName->mbs, "expand" ) ==0 )
		   printf( "%i, %p, parlist = %s; npar = %i; ndef = %i, %i\n", i, rout, rout->routType->name->mbs, npar, ndef, selfChecked );
		 */
		sum = 0;
		if( (npar | ndef) ==0 ){
			if( max <1 ){
				max = 1;
				best = i;
			}
			sum = 1;
			goto NextRoutine;
		}
		if( npar > ndef ) goto NextRoutine;
		for( j=selfChecked; j<ndef; j++) parpass[j] = 0;
		for(ifrom=0; ifrom<npar; ifrom++){
			DValue val = *dpar[ifrom];
			ito = ifrom + selfChecked;
			if( ito < ndef && parType[ito]->tid == DAO_PAR_VALIST ){
				for(; ifrom<npar; ifrom++) parpass[ifrom+selfChecked] = 1;
				break;
			}
			if( val.t == DAO_PAR_NAMED ){
				DaoPair *pair = val.v.pair;
				val = pair->second;
				node = DMap_Find( mapNames, pair->first.v.s );
				if( node == NULL ) goto NextRoutine;
				ito = node->value.pInt;
			}
			if( ito >= ndef )  goto NextRoutine;
			abtp = parType[ito]->X.abtype; /* must be named */
			parpass[ito] = DaoType_MatchValue( abtp, val, defs );
			/*
			   printf( "%i:  %s\n", parpass[ito], abtp->name->mbs );
			 */
			if( parpass[ito] ==0 ) goto NextRoutine;
		}
		for(ito=0; ito<ndef; ito++){
			m = parType[ito]->tid;
			if( m == DAO_PAR_VALIST ) break;
			if( parpass[ito] ) continue;
			if( m != DAO_PAR_DEFAULT ) goto NextRoutine;
			parpass[ito] = 1;
		}
		sum = selfMatch;
		for( j=selfChecked; j<ndef; j++ ) sum += parpass[j];
		if( npar ==0 && ( ndef==0 || parType[0]->tid == DAO_PAR_VALIST ) ) sum = 1;
		match = sum / (ndef + 1.0);
		if( match > max ){
			max = match;
			best = i;
		}
NextRoutine:
		i ++;
	}
	/*
	   if( strcmp( self->routName->mbs, "expand" ) ==0 )
	   printf("best = %i\n", best);
	 */
	if( defs ) DMap_Delete( defs );
	if( best < 0 ) return NULL;
	routine = routTable->items.pRout[best];
	if( routine->type != DAO_ROUTINE || routine->specialized == NULL )
		return (DRoutine*)routine;
	routine = (DaoRoutine*) DRoutine_GetOverLoadExt( routine->specialized, NULL, obj, p, n, code );
	if( routine ) return (DRoutine*) routine;
	return (DRoutine*) routTable->items.pBase[best];
}
static DaoRoutine* DaoRoutine_GetDecorator( DArray *routTable, DValue *p[], int n )
{
	DaoRoutine *rout, *dc, *best = NULL;
	DaoType *ft, *tp, **types, **stypes;
	DMap *mapNames, *defs = NULL;
	DArray *nested;
	DNode *it, *node;
	int parpass[DAO_MAX_PARAM];
	float sum, sum2, max = 0;
	int i, j, k, match;
	if( n ==0 || p[0]->t != DAO_ROUTINE ) return NULL;
	rout = p[0]->v.routine;
	stypes = rout->routType->nested->items.pType;
	for(i=0; i<routTable->size; i++){
		dc = routTable->items.pRout[i];
		nested = dc->routType->nested;
		types = nested->items.pType;
		if( n > nested->size ) continue;
		ft = types[0];
		if( ft->tid != DAO_PAR_NAMED || ft->X.abtype->tid != DAO_ROUTINE ) continue;
		ft = ft->X.abtype;
		if( ft->nested->size > rout->routType->nested->size ) continue;
		sum = sum2 = 0;
		mapNames = ft->mapNames;
		for(it=DMap_First(mapNames); it; it=DMap_Next(mapNames,it)){
			tp = ft->nested->items.pType[it->value.pInt];
			node = DMap_Find( rout->routType->mapNames, it->key.pVoid );
			if( node == NULL ) goto NextDecorator;
			match = DaoType_MatchTo( tp, stypes[node->value.pInt], NULL );
			if( match ==0 ) goto NextDecorator;
			sum += match;
		}
		if( defs == NULL ) defs = DMap_New(0,0);
		DMap_Clear( defs );
		if( mapNames->size ==0 ) sum = 1;
		for(j=0; j<nested->size; j++) parpass[j] = 0;
		k = 1;
		for(j=1; j<n; j++){
			DValue pv = *p[j];
			if( k < nested->size && types[k]->tid == DAO_PAR_VALIST ){
				for(; k<n; k++) parpass[k] = 1;
				break;
			}
			if( pv.t == DAO_PAR_NAMED ){
				DaoPair *pair = pv.v.pair;
				node = DMap_Find( dc->routType->mapNames, pair->first.v.s );
				if( node == NULL ) goto NextDecorator;
				pv = pair->second;
				k = node->value.pInt;
			}
			if( k ==0 || k > nested->size )  goto NextDecorator;
			match = DaoType_MatchValue( types[k]->X.abtype, pv, defs );

			/*
			   printf( "%p %s %p %s\n", tp->X.extra, tp->name->mbs, t->X.extra, t->name->mbs );
			   printf( "%i:  %i\n", ito, parpass[ito] );
			 */

			if( match == 0 ) goto NextDecorator;
			sum2 += match;
			parpass[k] = 1;
			k += 1;
		}
		for(j=1; j<nested->size; j++){
			k = types[j]->tid;
			if( k == DAO_PAR_VALIST ) break;
			if( parpass[j] ) continue;
			if( k != DAO_PAR_DEFAULT ) goto NextDecorator;
			parpass[j] = 1;
			sum2 += 1;
		}
		sum = sum / (rout->routType->nested->size + 1) + sum2 / nested->size;
		if( sum > max ){
			max = sum;
			best = dc;
		}
NextDecorator:
		continue;
	}
	if( defs ) DMap_Delete( defs );
	return best;
}
DRoutine* DRoutine_GetOverLoad( DRoutine *self, DValue *obj, DValue *p[], int n, int code )
{
	DValue value = daoNullValue;
	DaoClass *klass, *filter;
	DaoRoutine *rout;
	DRoutine *rout2;
	if( self->routType->name->mbs[0] == '@' )
		self = (DRoutine*) DaoRoutine_GetDecorator( self->routTable, p, n );
	else
		self = DRoutine_GetOverLoadExt( self->routTable, NULL, obj, p, n, code );
	if( self ==NULL ) return NULL;
	if( !(self->attribs & DAO_ROUT_VIRTUAL) ) return self;
	rout = (DaoRoutine*) self;
	if( (obj && obj->t != DAO_OBJECT) || rout->tidHost != DAO_OBJECT ) return self;

	/* when,
	 *   Base::VirtMeth();
	 * is called in a "Sub" class method, the rout->routHost->X.klass
	 * will be "Base", but the obj.v.object->myClass will be "Sub". */
	filter = rout->routHost->X.klass;
	if( filter != obj->v.object->myClass ) return self;
	/* when called with scope resolution operator ::, they are always the same.  */

	klass = obj->v.object->that->myClass;
	if( klass == filter ) return self;
	if( DaoClass_ChildOf( klass, (DaoBase*) filter ) ==0 ) return self;
	DaoClass_GetData( klass, self->routName, & value, klass, NULL );
	if( value.t == DAO_ROUTINE ){
		rout2 = (DRoutine*) value.v.routine;
		value = daoNullObject;
		value.v.object = obj->v.object->that;
		rout2 = DRoutine_GetOverLoadExt( rout2->routTable, filter, &value, p, n, code );
		if( rout2 ) return (DRoutine*) rout2;
	}
	return self;
}
int DRoutine_PassDefault( DRoutine *routine, DValue *recv[], int passed )
{
	DaoType *tp, *routype = routine->routType;
	DaoType **types = routype->nested->items.pType;
	int constParam = routine->type == DAO_ROUTINE ? ((DaoRoutine*)routine)->constParam : 0;
	int ndef = routine->parCount;
	int ito;
	for(ito=0; ito<ndef; ito++){
		DValue val;
		int m = types[ito]->tid;
		if( m == DAO_PAR_VALIST ) break;
		if( passed & (1<<ito) ) continue;
		if( m != DAO_PAR_DEFAULT ) return 0;

		val = routine->routConsts->data[ito];
		if( val.t ==0 && val.ndef ) return 0; /* no default */
		if( val.t ==0 ){ /* null value as default (e.g. in DaoMap methods) */
			DValue_Copy( recv[ito], val );
			continue;
		}
		tp = types[ito]->X.abtype;
		if( DValue_Move( val, recv[ito], tp ) ==0 ) return 0;
		if( constParam & (1<<ito) ) recv[0]->cst = 1;
	}
	return 1;
}
int DRoutine_PassParams( DRoutine *routine, DValue *obj, DValue *recv[], DValue *p[], DValue *base, int np, int code )
{
	ullong_t passed = 0;
	int constParam = routine->type == DAO_ROUTINE ? ((DaoRoutine*)routine)->constParam : 0;
	int npar = np;
	int ifrom, ito;
	int ndef = routine->parCount;
	int selfChecked = 0;
	DaoType *routype = routine->routType;
	DaoType *tp, **types = routype->nested->items.pType;
#if 0
	int i;
	printf( "%s: %i %i %i\n", routine->routName->mbs, ndef, np, obj ? obj->t : 0 );
	for(i=0; i<npar; i++){
		tp = DaoNameSpace_GetTypeV( routine->nameSpace, *p[i] );
		printf( "%i  %s\n", i, tp->name->mbs );
	}
#endif

	if( (code == DVM_MCALL || code == DVM_MCALL_TC) && !(routype->attrib & DAO_TYPE_SELF) ){
		npar --;
		p ++;
		if(base) base ++;
	}else if( obj && obj->t && (routype->attrib & DAO_TYPE_SELF) ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		tp = types[0]->X.abtype;
		if( obj->t < DAO_ARRAY ){
			if( tp == NULL || DaoType_MatchValue( tp, *obj, NULL ) == DAO_MT_EQ ){
				recv[0] = obj;
				selfChecked = 1;
				passed = 1;
			}
		}else{
			DValue o = *obj;
			if( o.t == DAO_OBJECT && (tp->tid ==DAO_OBJECT || tp->tid ==DAO_CDATA) ){
				o.v.object = o.v.object->that; /* for virtual method call */
				o.v.p = DaoObject_MapThisObject( o.v.object, tp );
				o.t = o.v.p ? o.v.p->type : 0;
			}
			if( DValue_Move( o, recv[0], tp ) ){
				selfChecked = 1;
				passed = 1;
			}
			recv[0]->cst = obj->cst;
		}
	}
	/*
	   printf( "%s, rout = %s; ndef = %i; npar = %i, %i\n", routine->routName->mbs, routine->routType->name->mbs, ndef, npar, selfChecked );
	 */
	if( npar > ndef ) return 0;
	if( (npar|ndef) ==0 ) return 1;
	/* pass from p[ifrom] to recv[ito], with type checking by types[ito] */
	for(ifrom=0; ifrom<npar; ifrom++){
		DValue *val = p[ifrom];
		DValue *loc = base ? base + ifrom : NULL;
		ito = ifrom + selfChecked;
		if( ito < ndef && types[ito]->tid == DAO_PAR_VALIST ){
			for(; ifrom<npar; ifrom++){
				ito = ifrom + selfChecked;
				DValue_Move( *p[ifrom], recv[ito], NULL );
				passed |= 1<<ito;
			}
			break;
		}
		if( val->t == DAO_PAR_NAMED ){
			DaoPair *pair = val->v.pair;
			DNode *node = DMap_Find( routype->mapNames, pair->first.v.s );
			val = & pair->second;
			if( node == NULL ) return 0;
			ito = node->value.pInt;
		}
		if( ito >= ndef ) return 0;
		passed |= 1<<ito;
		tp = types[ito]->X.abtype;
		if( loc && val != loc && !(constParam & (1<<ito)) ){ /* put by DVM_LOAD */
			if( DaoType_MatchValue( tp, *val, NULL ) == DAO_MT_EQ ){
				recv[ito] = val;
				continue;
			}
		}
		if( DValue_Move( *val, recv[ito], tp ) ==0 ) return 0;
		if( constParam & (1<<ito) ) recv[ito]->cst = 1;
	}
	return DRoutine_PassDefault( routine, recv, passed );
}
int DRoutine_FastPassParams( DRoutine *routine, DValue *obj, DValue *recv[], DValue *p[], DValue *base, int np, int code )
{
	int npar = np;
	int ndef = routine->parCount;
	int ifrom, ito;
	int selfChecked = 0;
	int constParam = routine->type == DAO_ROUTINE ? ((DaoRoutine*)routine)->constParam : 0;
	DaoType *routype = routine->routType;
	DaoType *tp, **types = routype->nested->items.pType;

	if( code == DVM_MCALL_TC && ! (routype->attrib & DAO_TYPE_SELF) ){
		npar --;
		p ++;
		if(base) base ++;
	}else if( obj && obj->t && (routype->attrib & DAO_TYPE_SELF) ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		tp = types[0]->X.abtype;
		if( obj->t < DAO_ARRAY ){
			if( tp == NULL || DaoType_MatchValue( tp, *obj, NULL ) == DAO_MT_EQ ){
				recv[0] = obj;
				selfChecked = 1;
			}
		}else{
			DValue o = *obj;
			if( o.t == DAO_OBJECT && (tp->tid ==DAO_OBJECT || tp->tid ==DAO_CDATA) ){
				o.v.object = o.v.object->that; /* for virtual method call */
				o.v.p = DaoObject_MapThisObject( o.v.object, tp );
				o.t = o.v.p ? o.v.p->type : 0;
			}
			if( DValue_Move( o, recv[0], tp ) ) selfChecked = 1;
			recv[0]->cst = obj->cst;
		}
	}
	/*
	   printf( "rout = %s; ndef = %i; npar = %i, %i\n", routine->routType->name->mbs, ndef, npar, selfChecked );
	 */
	if( (npar|ndef) ==0 ) return 1;
	for(ifrom=0; ifrom<npar; ifrom++){
		DValue *val = p[ifrom];
		DValue *loc = base ? base + ifrom : NULL;
		ito = ifrom + selfChecked;
		tp = types[ito]->X.abtype;
		if( loc && val != loc && !(constParam & (1<<ito)) ){ /* put by DVM_LOAD */
			if( DaoType_MatchValue( tp, *val, NULL ) == DAO_MT_EQ ){
				recv[ito] = val;
				continue;
			}
		}
#if 0
		printf( "ip = %i\n", ip );
		DaoType *tp = DaoNameSpace_GetTypeV( routine->nameSpace, *pv );
		printf( "%s\n", parType[ip]->X.abtype->name->mbs );
		printf( "%s\n", tp->name->mbs );
#endif
		if( ! DValue_Move( *val, recv[ito], tp ) ) return 0;
		if( constParam & (1<<ito) ) recv[ito]->cst = 1;
	}
	return 1;
}

DaoTypeBase routTyper=
{
	"routine", & baseCore, NULL, NULL, {0},
	(FuncPtrDel) DaoRoutine_Delete, NULL
};

DaoRoutine* DaoRoutine_New()
{
	DaoRoutine *self = (DaoRoutine*) dao_calloc( 1, sizeof( DaoRoutine ) );
	DaoBase_Init( self, DAO_ROUTINE );
	DRoutine_Init( (DRoutine*)self );
	DRoutine_AddOverLoad( (DRoutine*)self, (DRoutine*)self );
	self->source = NULL;
	self->original = NULL;
	self->specialized = NULL;
	self->vmCodes = DaoVmcArray_New();
	self->regType = DArray_New(0);
	self->defLocals = DArray_New(D_TOKEN);
	self->annotCodes = DArray_New(D_VMCODE);
	self->localVarType = DMap_New(0,0);
	self->abstypes = DMap_New(D_STRING,0);
	self->bodyStart = self->bodyEnd = -1;
#ifdef DAO_WITH_JIT
	self->binCodes = DArray_New(D_STRING);
	self->jitFuncs = DArray_New(0);
	self->preJit = DaoVmcArray_New();
#endif
	return self;
}
void DaoRoutine_Delete( DaoRoutine *self )
{
	DNode *n;
	DRoutine_DeleteFields( (DRoutine*)self );
	if( self->minimal ==1 ){
		DaoLateDeleter_Push( self );
		return;
	}
	n = DMap_First( self->abstypes );
	for( ; n != NULL; n = DMap_Next( self->abstypes, n ) ) GC_DecRC( n->value.pBase );
	if( self->upRoutine ) GC_DecRC( self->upRoutine );
	if( self->upContext ) GC_DecRC( self->upContext );
	if( self->original ) GC_DecRC( self->original );
	if( self->specialized ){
		GC_DecRCs( self->specialized );
		DArray_Delete( self->specialized );
	}
	GC_DecRCs( self->regType );
	DaoVmcArray_Delete( self->vmCodes );
	DArray_Delete( self->regType );
	DArray_Delete( self->defLocals );
	DArray_Delete( self->annotCodes );
	DMap_Delete( self->localVarType );
	DMap_Delete( self->abstypes );
	if( self->revised ) GC_DecRC( self->revised );
	if( self->parser ) DaoParser_Delete( self->parser );
#ifdef DAO_WITH_JIT
	DArray_Delete( self->binCodes );
	DArray_Delete( self->jitFuncs );
	DaoVmcArray_Delete( self->preJit );
	if( self->jitMemory ) GC_DecRC( self->jitMemory );
#endif
	DaoLateDeleter_Push( self );
}
void DaoParser_ClearCodes( DaoParser *self );
void DaoRoutine_Compile( DaoRoutine *self )
{
	if( self->minimal ==1 ) return;
	/* XXX thread safety? */
	if( self->parser && self->parser->defined ){
		if( self->parser->parsed == 0 ){
			if( ! DaoParser_ParseRoutine( self->parser ) ){
				/* This function is used by DaoContext_Init() and DaoContext_InitWithParams(),
				 * which are used in many places, rendering it very tedious and error-prone
				 * to handle the compiling fails by returned values.
				 *
				 * By substituting the routine body of the failed ones with the following scripts:
				 *     raise Exception.Error( "Compiling failed." );
				 * it become un-neccessary to handle the compiling fails in places where
				 * DaoContext_Init() and DaoContext_InitWithParams() are used!
				 */
				DArray *tokens = DArray_New(D_TOKEN);
				DaoType *routp = self->routType;
				DaoType *retp = NULL;
				int i = 0, k = self->parser->curLine;
				DArray_Clear( self->parser->errors );
				self->parser->error = 0;
				DaoTokens_AddRaiseStatement( tokens, "Error", "'Compiling failed'", k );
				if( routp ){ /* XXX */
					/* to avoid type checking for RETURN */
					retp = routp->X.abtype;
					routp->X.abtype = NULL;
				}
				DArray_Swap( self->parser->tokens, tokens );
				DArray_Clear( self->parser->vmCodes );
				DArray_Clear( self->parser->scoping );
				DaoParser_ClearCodes( self->parser );
				self->parser->lexLevel = 0;
				self->parser->parsed = 0;
				i = DaoParser_ParseRoutine( self->parser );
				if( retp ) routp->X.abtype = retp;
				DArray_Swap( self->parser->tokens, tokens );
				DArray_Delete( tokens );
			}
		}
		/* this function may be called recursively */
		if( self->parser ) DaoParser_Delete( self->parser );
		self->parser = NULL;
	}
}
void DaoRoutine_CopyFields( DaoRoutine *self, DaoRoutine *other );
DaoRoutine* DaoRoutine_Copy( DaoRoutine *self, int overload )
{
	DaoRoutine *copy = NULL;
	if( self->minimal ){
		DRoutine *copy2 = DRoutine_New();
		DRoutine_CopyFields( (DRoutine*) copy2, (DRoutine*) self );
		if( overload ) DRoutine_AddOverLoad( (DRoutine*) self, (DRoutine*) copy2 );
		return (DaoRoutine*) copy2;
	}
	copy = DaoRoutine_New();
	DaoRoutine_Compile( self );
	DRoutine_CopyFields( (DRoutine*) copy, (DRoutine*) self );
	if( overload ) DRoutine_AddOverLoad( (DRoutine*) self, (DRoutine*) copy );
	DaoRoutine_CopyFields( copy, self );
	return copy;
}
void DaoRoutine_CopyFields( DaoRoutine *self, DaoRoutine *other )
{
	int i;
	DaoRoutine_Compile( other );
	DVarray_Assign( self->routConsts, other->routConsts );
	for(i=0; i<other->routConsts->size; i++) DValue_MarkConst( self->routConsts->data + i );
	if( other->minimal ==1 ) return;
	DMap_Delete( self->localVarType );
	DArray_Delete( self->annotCodes );
	self->source = other->source;
	self->annotCodes = DArray_Copy( other->annotCodes );
	self->localVarType = DMap_Copy( other->localVarType );
	DaoVmcArray_Assign( self->vmCodes, other->vmCodes );
	DaoGC_IncRCs( other->regType );
	DaoGC_DecRCs( self->regType );
	DArray_Assign( self->regType, other->regType );
	GC_ShiftRC( other->nameSpace, self->nameSpace );
	self->nameSpace = other->nameSpace;
	self->constParam = other->constParam;
	self->locRegCount = other->locRegCount;
	self->defLine = other->defLine;
	self->bodyStart = other->bodyStart;
	self->bodyEnd = other->bodyEnd;
#ifdef DAO_WITH_JIT
	DArray_Assign( self->binCodes, other->binCodes );
	DArray_Assign( self->jitFuncs, other->jitFuncs );
	DaoVmcArray_Assign( self->preJit, other->preJit );
	self->jitMemory = other->jitMemory;
#endif
}
static DaoRoutine* DaoRoutine_Copy2( DaoRoutine *self )
{
	DaoRoutine *copy = DaoRoutine_Copy( self, 0 );
	if( self->specialized == NULL ) self->specialized = DArray_New(0);
	GC_IncRC( copy );
	GC_IncRC( self );
	DArray_Append( self->specialized, copy );
	copy->original = self;
	return copy;
}

int DaoRoutine_InferTypes( DaoRoutine *self );
extern void DaoRoutine_JitCompile( DaoRoutine *self );

int DaoRoutine_SetVmCodes( DaoRoutine *self, DArray *vmCodes )
{
	int i;
	if( vmCodes == NULL || vmCodes->type != D_VMCODE ) return 0;
	DArray_Swap( self->annotCodes, vmCodes );
	if( self->upRoutine && self->upRoutine->regType->size ==0 ) return 1;
	vmCodes = self->annotCodes;
	DaoVmcArray_Resize( self->vmCodes, vmCodes->size );
	for(i=0; i<vmCodes->size; i++){
		self->vmCodes->codes[i] = * (DaoVmCode*) vmCodes->items.pVmc[i];
	}
	return DaoRoutine_InferTypes( self );
}
int DaoRoutine_SetVmCodes2( DaoRoutine *self, DaoVmcArray *vmCodes )
{
	DaoVmcArray_Assign( self->vmCodes, vmCodes );
	return DaoRoutine_InferTypes( self );
}
DaoFunction* DaoFunction_Copy( DaoFunction *self, int overload );

static DaoType* DaoType_DeepItemType( DaoType *self )
{
	int i, t=0, n = self->nested ? self->nested->size : 0;
	DaoType *type = self;
	switch( self->tid ){
	case DAO_ARRAY :
	case DAO_LIST :
	case DAO_TUPLE :
		for(i=0; i<n; i++){
			DaoType *tp = DaoType_DeepItemType( self->nested->items.pType[i] );
			if( tp->tid > t ){
				t = tp->tid;
				type = self;
			}
		}
		break;
	default: break;
	}
	return type;
}

enum DaoTypingErrorCode
{
	DTE_TYPE_NOT_CONSISTENT ,
	DTE_TYPE_NOT_MATCHING ,
	DTE_TYPE_NOT_INITIALIZED,
	DTE_TYPE_WRONG_CONTAINER ,
	DTE_DATA_CANNOT_CREATE ,
	DTE_FIELD_NOT_PERMIT ,
	DTE_FIELD_NOT_EXIST ,
	DTE_FIELD_OF_INSTANCE ,
	DTE_ITEM_WRONG_ACCESS ,
	DTE_INDEX_NOT_VALID ,
	DTE_INDEX_WRONG_TYPE ,
	DTE_KEY_NOT_VALID ,
	DTE_KEY_WRONG_TYPE ,
	DTE_OPERATION_NOT_VALID ,
	DTE_PARAM_ERROR ,
	DTE_PARAM_WRONG_NUMBER ,
	DTE_PARAM_WRONG_TYPE ,
	DTE_PARAM_WRONG_NAME ,
	DTE_CONST_WRONG_MODIFYING ,
	DTE_ROUT_NOT_IMPLEMENTED
};
static const char*const DaoTypingErrorString[] =
{
	"Inconsistent typing",
	"Types not matching",
	"Variable not initialized",
	"Wrong container type",
	"Data cannot be created",
	"Member not permited",
	"Member not exist",
	"Need class instance",
	"Invalid index/key access",
	"Invalid index access",
	"Invalid index type",
	"Invalid key acess",
	"Invalid key type",
	"Invalid operation on the type",
	"Invalid parameters for the call",
	"Invalid number of parameter",
	"Invalid parameter type",
	"Invalid parameter name",
	"Constant should not be modified",
	"Call to un-implemented function"
};
extern DaoClass *daoClassFutureValue;

enum OprandType
{
	OT_OOO = 0,
	OT_AOO , /* SETVX */
	OT_OOC , /* GETCX GETVX */
	OT_AOC , /* LOAD, MOVE, CAST, unary operations... */
	OT_OBC , /* NAMEVA */
	OT_ABC , /* binary operations */
	OT_AIC , /* GETF_X: access field by index */
	OT_EXP , /* LIST, ARRAY, CALL, ... */
	OT_END
};

static const char mapTyping[26] = {
	'A','B',DAO_COMPLEX, DAO_DOUBLE,'E',DAO_FLOAT,'G','H',DAO_INTEGER,
	'J','K','L','M','N','O','P','Q','R',DAO_STRING,'T','U','V','W','X','Y','Z'
};

static const char vmcTyping[][7] =
{
	/*  ,  A,  B,  C,    */
	{ OT_OOO,  -1,  -1, -1, -1, -1,  -1 } , /* DVM_NOP */
	{ OT_OOC, -1, -1,  0, -1, -1,  -1 } , /* DVM_DATA */
	{ OT_OOC, -1, -1,  0, -1, -1,  -1 } , /* DVM_GETCL */
	{ OT_OOC, -1, -1,  0, -1, -1,  -1 } , /* DVM_GETCK */
	{ OT_OOC, -1, -1,  0, -1, -1,  -1 } , /* DVM_GETCG */
	{ OT_OOC, -1, -1,  0, -1, -1,  -1 } , /* DVM_GETVL */
	{ OT_OOC, -1, -1,  0, -1, -1,  -1 } , /* DVM_GETVO */
	{ OT_OOC, -1, -1,  0, -1, -1,  -1 } , /* DVM_GETVK */
	{ OT_OOC, -1, -1,  0, -1, -1,  -1 } , /* DVM_GETVG */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_GETI */
	{ OT_AIC,  0, -1,  0, -1, -1,  -1 } , /* DVM_GETF */
	{ OT_AIC,  0, -1,  0, -1, -1,  -1 } , /* DVM_GETMF */
	{ OT_AOO,  0, -1, -1, -1, 'S',  -1 } , /* DVM_SETVL */
	{ OT_AOO,  0, -1, -1, -1, 'S',  -1 } , /* DVM_SETVO */
	{ OT_AOO,  0, -1, -1, -1, 'S',  -1 } , /* DVM_SETVK */
	{ OT_AOO,  0, -1, -1, -1, 'S',  -1 } , /* DVM_SETVG */
	{ OT_ABC,  0,  0,  0, -1, 'S',  -1 } , /* DVM_SETI */
	{ OT_AIC,  0, -1,  0, -1, 'S',  -1 } , /* DVM_SETF */
	{ OT_AIC,  0, -1,  0, -1, 'S',  -1 } , /* DVM_SETMF */
	{ OT_AOC,  0, -1,  0, -1, 'V',  -1 } , /* DVM_LOAD */
	{ OT_AOC,  0, -1,  0, -1, 'V',  -1 } , /* DVM_CAST */
	{ OT_AOC,  0, -1,  0, -1, 'V',  -1 } , /* DVM_MOVE */
	{ OT_AOC,  0, -1,  0, -1, -1,  -1 } , /* DVM_NOT */
	{ OT_AOC,  0, -1,  0, -1, -1,  -1 } , /* DVM_UNMS */
	{ OT_AOC,  0, -1,  0, -1, -1,  -1 } , /* DVM_BITREV */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_ADD */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_SUB */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_MUL */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_DIV */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_MOD */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_POW */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_AND */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_OR */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_LT */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_LE */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_EQ */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_NE */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_IN */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_BITAND */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_BITOR */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_BITXOR */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_BITLFT */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_BITRIT */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_CHECK */
	{ OT_OBC, -1,  0,  0, -1, -1,  -1 } , /* DVM_CHECK */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_PAIR */
	{ OT_EXP,  0, 'X', -1, -1, -1,  -1 } , /* DVM_TUPLE */
	{ OT_EXP,  0, 'X', -1, -1, -1,  -1 } , /* DVM_LIST */
	{ OT_EXP,  0,  0 , -1, -1, -1,  -1 } , /* DVM_MAP */
	{ OT_EXP,  0,  0 , -1, -1, -1,  -1 } , /* DVM_HASH */
	{ OT_EXP,  0, 'X', -1, -1, -1,  -1 } , /* DVM_ARRAY */
	{ OT_EXP,  0,  0 , -1, -1, -1,  -1 } , /* DVM_MATRIX */
	{ OT_EXP, 'A',  0, -1, -1, -1,  -1 } , /* DVM_CURRY */
	{ OT_EXP, 'A',  0, -1, -1, -1,  -1 } , /* DVM_MCURRY */
	{ OT_EXP, 'A',  0, -1, -1, -1,  -1 } , /* DVM_ROUTINE */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_CLASS */
	{ OT_OOO, -1, -1, -1, -1, -1,  -1 } , /* DVM_GOTO */
	{ OT_OOO, -1, -1, -1, -1, -1,  -1 } , /* DVM_SWITCH */
	{ OT_OOO, -1, -1, -1, -1, -1,  -1 } , /* DVM_CASE */
	{ OT_AOC,  0, -1,  0, -1, -1,  -1 } , /* DVM_ITER */
	{ OT_OOO,  0, -1, -1, -1, -1,  -1 } , /* DVM_TEST */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_MATH */
	{ OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_FUNCT */
	{ OT_EXP, 'A',   0, -1, -1, 'M', -1 } , /* DVM_CALL */
	{ OT_EXP, 'A',   0, -1, -1, 'M', -1 } , /* DVM_MCALL */
	{ OT_EXP,   0, 'B', -1, -1, -1,  -1 } , /* DVM_CRRE */
	{ OT_OOO,  -1,  -1, -1, -1, -1,  -1 } , /* DVM_JITC */
	{ OT_EXP,   0,   0, -1, -1, -1,  -1 } , /* DVM_RETURN */
	{ OT_EXP,   0,   0, -1, -1, -1,  -1 } , /* DVM_YIELD */
	{ OT_OOO,  -1,  -1, -1, -1, -1,  -1 } , /* DVM_DEBUG */
	{ OT_OOO,  -1,  -1, -1, -1, -1,  -1 } , /* DVM_SECT */

	{ OT_AOO, 'I', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETVL_II */
	{ OT_AOO, 'F', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETVL_IF */
	{ OT_AOO, 'D', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETVL_ID */
	{ OT_AOO, 'I', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETVL_FI */
	{ OT_AOO, 'F', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETVL_FF */
	{ OT_AOO, 'D', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETVL_FD */
	{ OT_AOO, 'I', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETVL_DI */
	{ OT_AOO, 'F', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETVL_DF */
	{ OT_AOO, 'D', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETVL_DD */
	{ OT_AOO, 'I', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETVO_II */
	{ OT_AOO, 'F', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETVO_IF */
	{ OT_AOO, 'D', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETVO_ID */
	{ OT_AOO, 'I', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETVO_FI */
	{ OT_AOO, 'F', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETVO_FF */
	{ OT_AOO, 'D', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETVO_FD */
	{ OT_AOO, 'I', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETVO_DI */
	{ OT_AOO, 'F', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETVO_DF */
	{ OT_AOO, 'D', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETVO_DD */
	{ OT_AOO, 'I', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETVK_II */
	{ OT_AOO, 'F', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETVK_IF */
	{ OT_AOO, 'D', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETVK_ID */
	{ OT_AOO, 'I', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETVK_FI */
	{ OT_AOO, 'F', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETVK_FF */
	{ OT_AOO, 'D', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETVK_FD */
	{ OT_AOO, 'I', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETVK_DI */
	{ OT_AOO, 'F', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETVK_DF */
	{ OT_AOO, 'D', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETVK_DD */
	{ OT_AOO, 'I', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETVG_II */
	{ OT_AOO, 'F', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETVG_IF */
	{ OT_AOO, 'D', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETVG_ID */
	{ OT_AOO, 'I', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETVG_FI */
	{ OT_AOO, 'F', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETVG_FF */
	{ OT_AOO, 'D', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETVG_FD */
	{ OT_AOO, 'I', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETVG_DI */
	{ OT_AOO, 'F', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETVG_DF */
	{ OT_AOO, 'D', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETVG_DD */

	{ OT_AOC, 'I', -1, 'I', -1, 'V', -1 } , /* DVM_MOVE_II */
	{ OT_AOC, 'F', -1, 'I', -1, 'V', -1 } , /* DVM_MOVE_IF */
	{ OT_AOC, 'D', -1, 'I', -1, 'V', -1 } , /* DVM_MOVE_ID */
	{ OT_AOC, 'I', -1, 'F', -1, 'V', -1 } , /* DVM_MOVE_FI */
	{ OT_AOC, 'F', -1, 'F', -1, 'V', -1 } , /* DVM_MOVE_FF */
	{ OT_AOC, 'D', -1, 'F', -1, 'V', -1 } , /* DVM_MOVE_FD */
	{ OT_AOC, 'I', -1, 'D', -1, 'V', -1 } , /* DVM_MOVE_DI */
	{ OT_AOC, 'F', -1, 'D', -1, 'V', -1 } , /* DVM_MOVE_DF */
	{ OT_AOC, 'D', -1, 'D', -1, 'V', -1 } , /* DVM_MOVE_DD */
	{ OT_AOC, 'C', -1, 'C', -1, 'V', -1 } , /* DVM_MOVE_CC */
	{ OT_AOC, 'S', -1, 'S', -1, 'V', -1 } , /* DVM_MOVE_SS */
	{ OT_AOC, 'P', -1, 'P', -1, 'V', -1 } , /* DVM_MOVE_PP */

	{ OT_AOC, 'I', -1, 'I', -1, -1, -1 } , /* DVM_NOT_I */
	{ OT_AOC, 'F', -1, 'F', -1, -1, -1 } , /* DVM_NOT_F */
	{ OT_AOC, 'D', -1, 'D', -1, -1, -1 } , /* DVM_NOT_D */
	{ OT_AOC, 'I', -1, 'I', -1, -1, -1 } , /* DVM_UNMS_I */
	{ OT_AOC, 'F', -1, 'F', -1, -1, -1 } , /* DVM_UNMS_F */
	{ OT_AOC, 'D', -1, 'D', -1, -1, -1 } , /* DVM_UNMS_D */
	{ OT_AOC, 'I', -1, 'I', -1, -1, -1 } , /* DVM_BITREV_I */
	{ OT_AOC, 'F', -1, 'F', -1, -1, -1 } , /* DVM_BITREV_F */
	{ OT_AOC, 'D', -1, 'D', -1, -1, -1 } , /* DVM_BITREV_D */
	{ OT_AOC, 'C', -1, 'C', -1, -1, -1 } , /* DVM_UNMS_C */

	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_ADD_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_SUB_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_MUL_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_DIV_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_MOD_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_POW_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_AND_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_OR_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_LT_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_LE_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_EQ_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_NE_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_BITAND_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_BITOR_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_BITXOR_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_BITLFT_III */
	{ OT_ABC, 'I', 'I', 'I', -1, -1, -1 } , /* DVM_BITRIT_III */

	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_ADD_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_SUB_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_MUL_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_DIV_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_MOD_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_POW_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_AND_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_OR_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_LT_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_LE_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_EQ_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_NE_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_BITAND_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_BITOR_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_BITXOR_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_BITLFT_FFF */
	{ OT_ABC, 'F', 'F', 'F', -1, -1, -1 } , /* DVM_BITRIT_FFF */

	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_ADD_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_SUB_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_MUL_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_DIV_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_MOD_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_POW_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_AND_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_OR_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_LT_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_LE_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_EQ_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_NE_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_BITAND_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_BITOR_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_BITXOR_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_BITLFT_DDD */
	{ OT_ABC, 'D', 'D', 'D', -1, -1, -1 } , /* DVM_BITRIT_DDD */

	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_ADD_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_SUB_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_MUL_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_DIV_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_MOD_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_POW_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_AND_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_OR_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_LT_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_LE_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_EQ_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_NE_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_BITLFT_FNN */
	{ OT_ABC, 0, 0, 'F', -1, -1, -1 } , /* DVM_BITRIT_FNN */

	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_ADD_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_SUB_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_MUL_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_DIV_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_MOD_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_POW_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_AND_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_OR_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_LT_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_LE_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_EQ_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_NE_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_BITLFT_DNN */
	{ OT_ABC, 0, 0, 'D', -1, -1, -1 } , /* DVM_BITRIT_DNN */

	{ OT_ABC, 'S', 'S', 'S', -1, -1, -1 } , /* DVM_ADD_SS */
	{ OT_ABC, 'S', 'S', 'I', -1, -1, -1 } , /* DVM_LT_SS */
	{ OT_ABC, 'S', 'S', 'I', -1, -1, -1 } , /* DVM_LE_SS */
	{ OT_ABC, 'S', 'S', 'I', -1, -1, -1 } , /* DVM_EQ_SS */
	{ OT_ABC, 'S', 'S', 'I', -1, -1, -1 } , /* DVM_NE_SS */

	{ OT_ABC,   0, 'I',   0, -1,  -1, DAO_LIST } , /* DVM_GETI_LI */
	{ OT_ABC,   0, 'I',   0, -1, 'S', DAO_LIST } , /* DVM_SETI_LI */
	{ OT_ABC,   0, 'I', 'I', -1,  -1, DAO_STRING } , /* DVM_GETI_SI */
	{ OT_ABC, 'I', 'I',   0, -1, 'S', DAO_STRING } , /* DVM_SETI_SII */
	{ OT_ABC, 'I', 'I', 'I', -1,  -1, DAO_LIST } , /* DVM_GETI_LII */
	{ OT_ABC, 'F', 'I', 'F', -1,  -1, DAO_LIST } , /* DVM_GETI_LFI */
	{ OT_ABC, 'D', 'I', 'D', -1,  -1, DAO_LIST } , /* DVM_GETI_LDI */
	{ OT_ABC, 'S', 'I', 'S', -1,  -1, DAO_LIST } , /* DVM_GETI_LSI */

	{ OT_ABC, 'I', 'I', 'I', -1, 'S', DAO_LIST } , /* DVM_SETI_LIII */
	{ OT_ABC, 'F', 'I', 'I', -1, 'S', DAO_LIST } , /* DVM_SETI_LIIF */
	{ OT_ABC, 'D', 'I', 'I', -1, 'S', DAO_LIST } , /* DVM_SETI_LIID */
	{ OT_ABC, 'I', 'I', 'F', -1, 'S', DAO_LIST } , /* DVM_SETI_LFII */
	{ OT_ABC, 'F', 'I', 'F', -1, 'S', DAO_LIST } , /* DVM_SETI_LFIF */
	{ OT_ABC, 'D', 'I', 'F', -1, 'S', DAO_LIST } , /* DVM_SETI_LFID */
	{ OT_ABC, 'I', 'I', 'D', -1, 'S', DAO_LIST } , /* DVM_SETI_LDII */
	{ OT_ABC, 'F', 'I', 'D', -1, 'S', DAO_LIST } , /* DVM_SETI_LDIF */
	{ OT_ABC, 'D', 'I', 'D', -1, 'S', DAO_LIST } , /* DVM_SETI_LDID */
	{ OT_ABC, 'S', 'I', 'S', -1, 'S', DAO_LIST } , /* DVM_SETI_LSIS */

	{ OT_ABC, 'I', 'I', 'I', -1,  -1, DAO_ARRAY } , /* DVM_GETI_AII */
	{ OT_ABC, 'F', 'I', 'F', -1,  -1, DAO_ARRAY } , /* DVM_GETI_AFI */
	{ OT_ABC, 'D', 'I', 'D', -1,  -1, DAO_ARRAY } , /* DVM_GETI_ADI */

	{ OT_ABC, 'I', 'I', 'I', -1, 'S', DAO_ARRAY } , /* DVM_SETI_AIII */
	{ OT_ABC, 'F', 'I', 'I', -1, 'S', DAO_ARRAY } , /* DVM_SETI_AIIF */
	{ OT_ABC, 'D', 'I', 'I', -1, 'S', DAO_ARRAY } , /* DVM_SETI_AIID */
	{ OT_ABC, 'I', 'I', 'F', -1, 'S', DAO_ARRAY } , /* DVM_SETI_AFII */
	{ OT_ABC, 'F', 'I', 'F', -1, 'S', DAO_ARRAY } , /* DVM_SETI_AFIF */
	{ OT_ABC, 'D', 'I', 'F', -1, 'S', DAO_ARRAY } , /* DVM_SETI_AFID */
	{ OT_ABC, 'I', 'I', 'D', -1, 'S', DAO_ARRAY } , /* DVM_SETI_ADII */
	{ OT_ABC, 'F', 'I', 'D', -1, 'S', DAO_ARRAY } , /* DVM_SETI_ADIF */
	{ OT_ABC, 'D', 'I', 'D', -1, 'S', DAO_ARRAY } , /* DVM_SETI_ADID */

	{ OT_ABC,   0, 'I',   0, -1,  -1, DAO_TUPLE } , /* DVM_GETI_TI */
	{ OT_ABC,   0, 'I',   0, -1, 'S', DAO_TUPLE } , /* DVM_SETI_TI */
	{ OT_AIC,   0,  -1,   0, -1,  -1, DAO_TUPLE } , /* DVM_GETF_T */
	{ OT_AIC,   0,  -1, 'I', -1,  -1, DAO_TUPLE } , /* DVM_GETF_TI */
	{ OT_AIC,   0,  -1, 'F', -1,  -1, DAO_TUPLE } , /* DVM_GETF_TF */
	{ OT_AIC,   0,  -1, 'D', -1,  -1, DAO_TUPLE } , /* DVM_GETF_TD */
	{ OT_AIC,   0,  -1, 'S', -1,  -1, DAO_TUPLE } , /* DVM_GETF_TD */
	{ OT_AIC,   0,  -1,   0, -1, 'S', DAO_TUPLE } , /* DVM_SETF_T */
	{ OT_AIC, 'I',  -1, 'I', -1, 'S', DAO_TUPLE } , /* DVM_SETF_TII */
	{ OT_AIC, 'F',  -1, 'I', -1, 'S', DAO_TUPLE } , /* DVM_SETF_TIF */
	{ OT_AIC, 'D',  -1, 'I', -1, 'S', DAO_TUPLE } , /* DVM_SETF_TID */
	{ OT_AIC, 'I',  -1, 'F', -1, 'S', DAO_TUPLE } , /* DVM_SETF_TFI */
	{ OT_AIC, 'F',  -1, 'F', -1, 'S', DAO_TUPLE } , /* DVM_SETF_TFF */
	{ OT_AIC, 'D',  -1, 'F', -1, 'S', DAO_TUPLE } , /* DVM_SETF_TFD */
	{ OT_AIC, 'I',  -1, 'D', -1, 'S', DAO_TUPLE } , /* DVM_SETF_TDI */
	{ OT_AIC, 'F',  -1, 'D', -1, 'S', DAO_TUPLE } , /* DVM_SETF_TDF */
	{ OT_AIC, 'D',  -1, 'D', -1, 'S', DAO_TUPLE } , /* DVM_SETF_TDD */
	{ OT_AIC, 'S',  -1, 'S', -1, 'S', DAO_TUPLE } , /* DVM_SETF_TDD */

	{ OT_ABC, 'C', 'C', 'C', -1, -1,        -1 } , /* DVM_ADD_CC */
	{ OT_ABC, 'C', 'C', 'C', -1, -1,        -1 } , /* DVM_SUB_CC */
	{ OT_ABC, 'C', 'C', 'C', -1, -1,        -1 } , /* DVM_MUL_CC */
	{ OT_ABC, 'C', 'C', 'C', -1, -1,        -1 } , /* DVM_DIV_CC */

	{ OT_ABC, 'C', 'I', 'C', -1,  -1, DAO_ARRAY } , /* DVM_GETI_ACI */
	{ OT_ABC, 'C', 'I', 'C', -1, 'S', DAO_ARRAY } , /* DVM_SETI_ACI */

	{ OT_ABC,  0,  0,  0, -1,  -1,           -1 } , /* DVM_GETI_AM */
	{ OT_ABC,  0,  0,  0, -1, 'S',           -1 } , /* DVM_SETI_AM */

	{ OT_AIC,  0, -1,  0, -1, -1,  -1 } , /* DVM_GETF_M */

	{ OT_AIC,  0, -1,  0, 'C',  -1,  DAO_CLASS  } , /* DVM_GETF_KC */
	{ OT_AIC,  0, -1,  0, 'G',  -1,  DAO_CLASS  } , /* DVM_GETF_KG */
	{ OT_AIC,  0, -1,  0, 'C',  -1,  DAO_OBJECT } , /* DVM_GETF_OC */
	{ OT_AIC,  0, -1,  0, 'G',  -1,  DAO_OBJECT } , /* DVM_GETF_OG */
	{ OT_AIC,  0, -1,  0, 'V',  -1,  DAO_OBJECT } , /* DVM_GETF_OV */
	{ OT_AIC,  0, -1,  0, 'C', 'S',  DAO_CLASS  } , /* DVM_SETF_KG */
	{ OT_AIC,  0, -1,  0, 'G', 'S',  DAO_OBJECT } , /* DVM_SETF_OG */
	{ OT_AIC,  0, -1,  0, 'V', 'S',  DAO_OBJECT } , /* DVM_SETF_OV */

	{ OT_AIC,  0, -1, 'I', 'C', -1,  DAO_CLASS  } , /* DVM_GETF_KCI */
	{ OT_AIC,  0, -1, 'I', 'G', -1,  DAO_CLASS  } , /* DVM_GETF_KGI */
	{ OT_AIC,  0, -1, 'I', 'C', -1,  DAO_OBJECT } , /* DVM_GETF_OCI */
	{ OT_AIC,  0, -1, 'I', 'G', -1,  DAO_OBJECT } , /* DVM_GETF_OGI */
	{ OT_AIC,  0, -1, 'I', 'V', -1,  DAO_OBJECT } , /* DVM_GETF_OVI */
	{ OT_AIC,  0, -1, 'F', 'C', -1,  DAO_CLASS  } , /* DVM_GETF_KCF */
	{ OT_AIC,  0, -1, 'F', 'G', -1,  DAO_CLASS  } , /* DVM_GETF_KGF */
	{ OT_AIC,  0, -1, 'F', 'C', -1,  DAO_OBJECT } , /* DVM_GETF_OCF */
	{ OT_AIC,  0, -1, 'F', 'G', -1,  DAO_OBJECT } , /* DVM_GETF_OGF */
	{ OT_AIC,  0, -1, 'F', 'V', -1,  DAO_OBJECT } , /* DVM_GETF_OVF */
	{ OT_AIC,  0, -1, 'D', 'C', -1,  DAO_CLASS  } , /* DVM_GETF_KCD */
	{ OT_AIC,  0, -1, 'D', 'G', -1,  DAO_CLASS  } , /* DVM_GETF_KGD */
	{ OT_AIC,  0, -1, 'D', 'C', -1,  DAO_OBJECT } , /* DVM_GETF_OCD */
	{ OT_AIC,  0, -1, 'D', 'G', -1,  DAO_OBJECT } , /* DVM_GETF_OGD */
	{ OT_AIC,  0, -1, 'D', 'V', -1,  DAO_OBJECT } , /* DVM_GETF_OVD */

	{ OT_AIC, 'I', -1, 'I', 'G', 'S',  DAO_CLASS  } , /* DVM_SETF_KGII */
	{ OT_AIC, 'I', -1, 'I', 'G', 'S',  DAO_OBJECT } , /* DVM_SETF_OGII */
	{ OT_AIC, 'I', -1, 'I', 'V', 'S',  DAO_OBJECT } , /* DVM_SETF_OVII */
	{ OT_AIC, 'F', -1, 'I', 'G', 'S',  DAO_CLASS  } , /* DVM_SETF_KGIF */
	{ OT_AIC, 'F', -1, 'I', 'G', 'S',  DAO_OBJECT } , /* DVM_SETF_OGIF */
	{ OT_AIC, 'F', -1, 'I', 'V', 'S',  DAO_OBJECT } , /* DVM_SETF_OVIF */
	{ OT_AIC, 'D', -1, 'I', 'G', 'S',  DAO_CLASS  } , /* DVM_SETF_KGID */
	{ OT_AIC, 'D', -1, 'I', 'G', 'S',  DAO_OBJECT } , /* DVM_SETF_OGID */
	{ OT_AIC, 'D', -1, 'I', 'V', 'S',  DAO_OBJECT } , /* DVM_SETF_OVID */
	{ OT_AIC, 'I', -1, 'F', 'G', 'S',  DAO_CLASS  } , /* DVM_SETF_KGFI */
	{ OT_AIC, 'I', -1, 'F', 'G', 'S',  DAO_OBJECT } , /* DVM_SETF_OGFI */
	{ OT_AIC, 'I', -1, 'F', 'V', 'S',  DAO_OBJECT } , /* DVM_SETF_OVFI */
	{ OT_AIC, 'F', -1, 'F', 'G', 'S',  DAO_CLASS  } , /* DVM_SETF_KGFF */
	{ OT_AIC, 'F', -1, 'F', 'G', 'S',  DAO_OBJECT } , /* DVM_SETF_OGFF */
	{ OT_AIC, 'F', -1, 'F', 'V', 'S',  DAO_OBJECT } , /* DVM_SETF_OVFF */
	{ OT_AIC, 'D', -1, 'F', 'G', 'S',  DAO_CLASS  } , /* DVM_SETF_KGFD */
	{ OT_AIC, 'D', -1, 'F', 'G', 'S',  DAO_OBJECT } , /* DVM_SETF_OGFD */
	{ OT_AIC, 'D', -1, 'F', 'V', 'S',  DAO_OBJECT } , /* DVM_SETF_OVFD */
	{ OT_AIC, 'I', -1, 'D', 'G', 'S',  DAO_CLASS  } , /* DVM_SETF_KGDI */
	{ OT_AIC, 'I', -1, 'D', 'G', 'S',  DAO_OBJECT } , /* DVM_SETF_OGDI */
	{ OT_AIC, 'I', -1, 'D', 'V', 'S',  DAO_OBJECT } , /* DVM_SETF_OVDI */
	{ OT_AIC, 'F', -1, 'D', 'G', 'S',  DAO_CLASS  } , /* DVM_SETF_KGDF */
	{ OT_AIC, 'F', -1, 'D', 'G', 'S',  DAO_OBJECT } , /* DVM_SETF_OGDF */
	{ OT_AIC, 'F', -1, 'D', 'V', 'S',  DAO_OBJECT } , /* DVM_SETF_OVDF */
	{ OT_AIC, 'D', -1, 'D', 'G', 'S',  DAO_CLASS  } , /* DVM_SETF_KGDD */
	{ OT_AIC, 'D', -1, 'D', 'G', 'S',  DAO_OBJECT } , /* DVM_SETF_OGDD */
	{ OT_AIC, 'D', -1, 'D', 'V', 'S',  DAO_OBJECT } , /* DVM_SETF_OVDD */

	{ OT_AOO, 'I', -1, -1, -1, -1,  -1 } , /* DVM_TEST_I */
	{ OT_AOO, 'F', -1, -1, -1, -1,  -1 } , /* DVM_TEST_F */
	{ OT_AOO, 'D', -1, -1, -1, -1,  -1 } , /* DVM_TEST_D */

	{ OT_EXP, 'A',  0, -1, -1, 'M', -1 } , /* DVM_CALL_CF */
	{ OT_EXP, 'A',  0, -1, -1, 'M', -1 } , /* DVM_CALL_CMF */
	{ OT_EXP, 'A',  0, -1, -1, 'M', -1 } , /* DVM_CALL_TC */
	{ OT_EXP, 'A',  0, -1, -1, 'M', -1 } , /* DVM_MCALL_TC */

	{ OT_OOO, -1, -1, -1, -1, -1,  -1 } , /* DVM_SAFE_GOTO */

	{ OT_OOO, -1, -1, -1, -1, -1, -1 } /* NULL */
};

static DaoType* DaoCheckBinArith0( DaoRoutine *self, DaoVmCodeX *vmc, 
		DaoType *at, DaoType *bt, DaoType *ct, DaoClass *hostClass,
		DString *mbs, int setname )
{
	DaoNameSpace *ns = self->nameSpace;
	DaoType *inumt = DaoNameSpace_MakeType( ns, "int", DAO_INTEGER, NULL, NULL, 0 );
	DaoType *ts[3];
	DRoutine *rout = NULL;
	DRoutine *rout2 = NULL;
	DNode *node;
	int min=0, spec=0;
	int opa = vmc->a;
	int opb = vmc->b;
	int opc = vmc->c;
	int code = vmc->code;
	int boolop = code >= DVM_AND && code <= DVM_NE;
	ts[0] = ct;
	ts[1] = at;
	ts[2] = bt;
	if( setname && opa == opc && daoBitBoolArithOpers2[code-DVM_MOVE] ){
		DString_SetMBS( mbs, daoBitBoolArithOpers2[code-DVM_MOVE] );
		if( at->tid == DAO_INTERFACE ){
			node = DMap_Find( at->X.inter->methods, mbs );
			rout = (DRoutine*) node->value.pBase;
		}else if( at->tid == DAO_OBJECT ){
			rout = (DRoutine*) DaoClass_FindOperator( at->X.klass, mbs->mbs, hostClass );
		}else if( at->tid == DAO_CDATA ){
			rout = (DRoutine*) DaoFindFunction( at->typer, mbs );
		}
		if( rout ){
			rout = MatchByParamType( rout, NULL, NULL, ts+1, 2, DVM_CALL, &min, &spec );
			/* if the operation is used in the overloaded operator,
			   do operation by address */
			if( boolop && rout == (DRoutine*) self ) return inumt;
			if( rout ) return ct;
		}
	}
	if( setname ) DString_SetMBS( mbs, daoBitBoolArithOpers[code-DVM_MOVE] );
	if( at->tid == DAO_INTERFACE ){
		node = DMap_Find( at->X.inter->methods, mbs );
		rout = (DRoutine*) node->value.pBase;
	}else if( at->tid == DAO_OBJECT ){
		rout = (DRoutine*) DaoClass_FindOperator( at->X.klass, mbs->mbs, hostClass );
	}else if( at->tid == DAO_CDATA ){
		rout = (DRoutine*) DaoFindFunction( at->typer, mbs );
	}
	if( rout ){
		rout2 = rout;
		rout = MatchByParamType( rout2, NULL, NULL, ts+1, 2, DVM_CALL, &min, &spec );
		/* if the operation is used in the overloaded operator,
		   do operation by address */
		if( boolop && rout == (DRoutine*) self ) return inumt;
		if( rout ) ct = rout->routType->X.abtype;
		if( rout == NULL && ct ){
			rout = MatchByParamType( rout2, NULL, NULL, ts, 3, DVM_CALL, &min, &spec );
		}
	}else{
		if( bt->tid == DAO_INTERFACE ){
			node = DMap_Find( bt->X.inter->methods, mbs );
			rout = (DRoutine*) node->value.pBase;
		}else if( bt->tid == DAO_OBJECT ){
			rout = (DRoutine*) DaoClass_FindOperator( bt->X.klass, mbs->mbs, hostClass );
		}else if( bt->tid == DAO_CDATA ){
			rout = (DRoutine*) DaoFindFunction( bt->typer, mbs );
		}
		if( rout == NULL ) return NULL;
		rout2 = rout;
		rout = MatchByParamType( rout2, NULL, NULL, ts+1, 2, DVM_CALL, &min, &spec );
		/* if the operation is used in the overloaded operator,
		   do operation by address */
		if( boolop && rout == (DRoutine*) self ) return inumt;
		if( rout ) ct = rout->routType->X.abtype;
		if( rout == NULL && ct ){
			rout = MatchByParamType( rout2, NULL, NULL, ts, 3, DVM_CALL, &min, &spec );
		}
		if( rout == NULL ) return NULL;
	}
	return ct;
}
static DaoType* DaoCheckBinArith( DaoRoutine *self, DaoVmCodeX *vmc, 
		DaoType *at, DaoType *bt, DaoType *ct, DaoClass *hostClass, DString *mbs )
{
	DaoType *rt = DaoCheckBinArith0( self, vmc, at, bt, ct, hostClass, mbs, 1 );
	if( rt == NULL && (vmc->code == DVM_LT || vmc->code == DVM_LE) ){
		if( vmc->code == DVM_LT ){
			DString_SetMBS( mbs, ">" );
		}else{
			DString_SetMBS( mbs, ">=" );
		}
		return DaoCheckBinArith0( self, vmc, bt, at, ct, hostClass, mbs, 0 );
	}
	return rt;
}
static DString* AppendError( DArray *errors, DRoutine *rout, size_t type )
{
	DString *s = DString_New(1);
	DArray_Append( errors, rout );
	DArray_Append( errors, s );
	DString_AppendMBS( s, DaoTypingErrorString[ type ] );
	DString_AppendMBS( s, " --- \" " );
	return s;
}
static void DRoutine_CheckError( DRoutine *self, DaoNameSpace *ns, DaoType *selftype,
		DValue *csts, DaoType *ts[], int np, int code, DArray *errors )
{
	DaoType *routType = self->routType;
	DaoStream *stream = self->nameSpace->vmSpace->stdStream;
	int i, j, ndef = 0;
	int ifrom, ito;
	int parpass[DAO_MAX_PARAM];
	int npar = np, size = routType->nested->size;
	int selfChecked = 0, selfMatch = 0;
	DValue cs = daoNullValue;
	DRoutine *rout;
	DNode *node;
	DMap *defs = DMap_New(0,0);
	DaoType  *abtp, **partypes = routType->nested->items.pType;
	DaoType **tps = ts;
	DaoType *tbuf[DAO_MAX_PARAM];

	if( ts == NULL && csts ){
		tps = ts = tbuf;
		for(i=0; i<np; i++) tbuf[i] = DaoNameSpace_GetTypeV( ns, csts[i] );
	}

	if( routType->nested ){
		ndef = routType->nested->size;
		if( ndef ){
			abtp = partypes[ ndef-1 ];
			if( abtp->tid == DAO_PAR_VALIST ) ndef = DAO_MAX_PARAM;
		}
	}

	/*
	   printf( "=====================================\n" );
	   for( j=0; j<npar; j++){
	   DaoType *tp = tps[j];
	   if( tp != NULL ) printf( "tp[ %i ]: %s\n", j, tp->name->mbs );
	   }
	   printf( "%s %i %i\n", routType->name->mbs, ndef, npar );
	   if( selftype ) printf( "%i\n", routType->name->mbs, ndef, npar, selftype );
	 */

	if( (code == DVM_MCALL  || code == DVM_MCALL_TC)
			&& ! ( routType->attrib & DAO_TYPE_SELF ) ){
		npar --;
		tps ++;
	}else if( selftype && ( routType->attrib & DAO_TYPE_SELF)
			&& code != DVM_MCALL && code != DVM_MCALL_TC ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		abtp = partypes[0]->X.abtype;
		selfMatch = DaoType_MatchTo( selftype, abtp, defs );
		if( selfMatch ){
			selfChecked = 1;
			parpass[0] = selfMatch;
		}
	}
	if( npar == ndef && ndef == 0 ) goto FinishOK;
	if( npar > ndef && (size == 0 || partypes[size-1]->tid != DAO_PAR_VALIST ) ){
		DString *s = AppendError( errors, self, DTE_PARAM_WRONG_NUMBER );
		DString_AppendMBS( s, "too many parameters \";\n" );
		goto FinishError;
	}

	for( j=selfChecked; j<ndef; j++) parpass[j] = 0;
	for(ifrom=0; ifrom<npar; ifrom++){
		DaoType *tp = tps[ifrom];
		ito = ifrom + selfChecked;
		if( ito < ndef && partypes[ito]->tid == DAO_PAR_VALIST ){
			for(; ifrom<npar; ifrom++) parpass[ifrom+selfChecked] = 1;
			break;
		}
		cs = daoNullValue;
		if( csts && csts[ifrom].t ==0 ) cs = csts[ifrom];
		if( tp == NULL ){
			DString *s = AppendError( errors, self, DTE_PARAM_WRONG_TYPE );
			DString_AppendMBS( s, "unknow parameter type \";\n" );
			goto FinishError;
		}
		if( tp->tid == DAO_PAR_NAMED ){
			node = DMap_Find( routType->mapNames, tp->fname );
			if( node == NULL ){
				DString *s = AppendError( errors, self, DTE_PARAM_WRONG_NAME );
				DString_Append( s, tp->fname );
				DString_AppendMBS( s, " \";\n" );
				goto FinishError;
			}
			ito = node->value.pInt;
			tp = tp->X.abtype;
		}
		if( ito > ndef || tp ==NULL ){
			DString *s = AppendError( errors, self, DTE_PARAM_WRONG_TYPE );
			DString_AppendMBS( s, "unknow parameter type \";\n" );
			goto FinishError;
		}
		abtp = routType->nested->items.pType[ito];
		if( abtp->tid == DAO_PAR_NAMED || abtp->tid == DAO_PAR_DEFAULT ) abtp = abtp->X.abtype;
		if( abtp->tid ==DAO_ROUTINE && ( cs.t ==DAO_ROUTINE || cs.t ==DAO_FUNCTION ) ){
			rout = DRoutine_GetOverLoadByType( (DRoutine*)cs.v.p, abtp );
			tp = rout ? rout->routType : NULL;
		}
		parpass[ito] = DaoType_MatchTo( tp, abtp, defs );

		/*
		   printf( "%p %s %p %s\n", tp->X.extra, tp->name->mbs, abtp->X.extra, abtp->name->mbs );
		   printf( "%i:  %i\n", ito, parpass[ito] );
		 */

		/* less strict */
		if( tp && parpass[ito] ==0 ){
			if( tp->tid == DAO_ANY && abtp->tid == DAO_ANY )
				parpass[ito] = DAO_MT_ANY;
			else if( tp->tid == DAO_ANY || tp->tid == DAO_UDF )
				parpass[ito] = DAO_MT_NEGLECT;
		}
		if( parpass[ito] == 0 ){
			DString *s = AppendError( errors, self, DTE_PARAM_WRONG_TYPE );
			tp = tps[ifrom];
			abtp = routType->nested->items.pType[ito];
			DString_AppendChar( s, '\'' );
			DString_Append( s, tp->name );
			DString_AppendMBS( s, "\' for \'" );
			DString_Append( s, abtp->name );
			DString_AppendMBS( s, "\' \";\n" );
			goto FinishError;
		}
	}
	for(ito=0; ito<ndef; ito++){
		i = partypes[ito]->tid;
		if( i == DAO_PAR_VALIST ) break;
		if( parpass[ito] ) continue;
		if( i != DAO_PAR_DEFAULT ){
			DString *s = AppendError( errors, self, DTE_PARAM_WRONG_NUMBER );
			DString_AppendMBS( s, "too few parameters \";\n" );
			goto FinishError;
		}
		parpass[ito] = 1;
	}

	/*
	   printf( "%s %i\n", routType->name->mbs, *min );
	 */
FinishOK:
FinishError:
	DMap_Delete( defs );
}
void DRoutine_ShowError( DRoutine *self, DaoType *selftype,
		DValue *csts, DaoType *ts[], int np, int code, DArray *errors )
{
	int i;
	DRoutine *rout;

	i = 0;
	while( i<self->routTable->size ){
		rout = (DRoutine*) self->routTable->items.pBase[i];
		/*
		   printf( "=====================================\n" );
		   printf("ovld %i, %p %s : %s\n", i, rout, self->routName->mbs, rout->routType->name->mbs);
		 */
		DRoutine_CheckError( rout, self->nameSpace, selftype, csts, ts, np, code, errors );
		i ++;
	}
	/*
	   printf("%s : best = %i, spec = %i\n", self->routName->mbs, best, *spec);
	 */
}

void DaoPrintCallError( DArray *errors, DaoStream *stdio )
{
	DString *mbs = DString_New(1);
	int i, k;
	for(i=0; i<errors->size; i+=2){
		DRoutine *rout = errors->items.pRout2[i];
		DaoStream_WriteMBS( stdio, "  ** " );
		DaoStream_WriteString( stdio, errors->items.pString[i+1] );
		DaoStream_WriteMBS( stdio, "     Assuming  : " );
		if( DaoToken_IsValidName( rout->routName->mbs, rout->routName->size ) ){
			DaoStream_WriteMBS( stdio, "routine " );
		}else{
			DaoStream_WriteMBS( stdio, "operator " );
		}
		k = DString_RFindMBS( rout->routType->name, "=>", rout->routType->name->size );
		DString_Assign( mbs, rout->routName );
		DString_AppendChar( mbs, '(' );
		DString_AppendDataMBS( mbs, rout->routType->name->mbs + 8, k-9 );
		DString_AppendChar( mbs, ')' );
		if( rout->routType->name->mbs[k+1] != '?' ){
			DString_AppendMBS( mbs, "=>" );
			DString_Append( mbs, rout->routType->X.abtype->name );
		}
		DString_AppendMBS( mbs, ";\n" );
		//DaoStream_WritePointer( stdio, rout );
		DaoStream_WriteString( stdio, mbs );
		DaoStream_WriteMBS( stdio, "     Reference : " );
		if( rout->type == DAO_ROUTINE ){
			DaoStream_WriteMBS( stdio, "line " );
			DaoStream_WriteInt( stdio, ((DaoRoutine*)rout)->defLine );
			DaoStream_WriteMBS( stdio, ", " );
		}
		DaoStream_WriteMBS( stdio, "file \"" );
		DaoStream_WriteString( stdio, rout->nameSpace->name );
		DaoStream_WriteMBS( stdio, "\";\n" );
		DString_Delete( errors->items.pString[i+1] );
	}
	DString_Delete( mbs );
}
int DaoRoutine_InferTypes( DaoRoutine *self )
{
#define NoCheckingType(t) ((t->tid==DAO_UDF)|(t->tid==DAO_ANY)|(t->tid==DAO_INITYPE))

#define AssertPairNumberType( tp ) \
	{ k = (tp)->nested->items.pType[0]->tid; \
	if( k > DAO_DOUBLE && k !=DAO_ANY ) goto NotMatch; \
	k = (tp)->nested->items.pType[1]->tid; \
	if( k > DAO_DOUBLE && k !=DAO_ANY ) goto NotMatch; }

#define InsertCodeMoveToInteger( opABC, opcode ) \
	{ vmc2.a = opABC; \
	opABC = self->locRegCount + addRegType->size -1; \
	addCount[i] ++; \
	vmc2.code = opcode; \
	vmc2.c = opABC; \
	DArray_Append( addCode, & vmc2 ); \
	DArray_Append( addRegType, inumt ); }

#define GOTO_ERROR( e1, e2, t1, t2 ) { ec_general = e1; ec_specific = e2; \
	type_source = t1; type_target = t2; goto ErrorTyping; }

#define ErrorTypeNotMatching( e1, t1, t2 ) \
	{ ec_general = e1; ec_specific = DTE_TYPE_NOT_MATCHING; \
		type_source = t1; type_target = t2; goto ErrorTyping; }
	
#define AssertTypeIdMatching( source, id, gerror ) \
	if( source->tid != id ){ \
		tid_target = id; ErrorTypeNotMatching( gerror, source, NULL ); }

#define AssertTypeMatching( source, target, defs, gerror ) \
	if( source->tid && source->tid != DAO_ANY && DaoType_MatchTo( source, target, defs ) ==0 ) \
		ErrorTypeNotMatching( gerror, source, target );

#define AssertInitialized( reg, ec, first, last ) \
	if( init[reg] == 0 || type[reg] == NULL ){ \
		annot_first = first; annot_last = last; ec_general = ec; \
		ec_specific = DTE_TYPE_NOT_INITIALIZED; goto ErrorTyping; }

#define AnnotateItemExpression( vmc, k ) \
	{k = DaoTokens_FindOpenToken( self->source, DTOK_COMMA, \
			vmc->first + k, vmc->first + vmc->last ); \
	if( k < 0 ) k = vmc->last - 1; else k -= vmc->first; }

#define UpdateType( id, tp )  GC_ShiftRC( tp, type[id] ), type[id] = tp

	int typed_code = daoConfig.typedcode;
	int i, j, k, cid=0, retinf = 0;
	int N = self->vmCodes->size;
	int M = self->locRegCount;
	int min=0, spec=0, lastcomp = 0;
	int TT0, TT1, TT2, TT3, TT4, TT5, TT6;
	int ec = 0, ec_general = 0, ec_specific = 0;
	int annot_first = 0, annot_last = 0, tid_target = 0;
	ushort_t code;
	ushort_t opa, opb, opc;
	DaoNameSpace *ns = self->nameSpace;
	DaoVmSpace *vms = ns->vmSpace;
	DaoType **tp, **type;
	DaoType *type_source = NULL, *type_target = NULL;
	DaoType *container = NULL, *indexkey = NULL, *itemvalue = NULL;
	DaoType *at, *bt, *ct, *tt, *ts[DAO_ARRAY];
	DaoType *simtps[DAO_ARRAY], *arrtps[DAO_ARRAY];
	DaoType *inumt, *fnumt, *dnumt, *comt, *longt, *enumt, *strt;
	DaoType *ilst, *flst, *dlst, *slst, *iart, *fart, *dart, *cart, *any, *udf;
	DaoVmCodeX **vmcs = self->annotCodes->items.pVmc;
	DaoVmCodeX *vmc;
	DaoVmCodeX  vmc2;
	DaoStream  *stdio = NULL;
	DRoutine *rout = NULL, *rout2;
	DaoClass *hostClass = self->tidHost==DAO_OBJECT ? self->routHost->X.klass:NULL;
	DaoClass *klass;
	DMap       *tmp;
	DNode      *node;
	DString    *str, *mbs, *error = NULL;
	DMap       *defs;
	DArray     *tparray, *errors = NULL;
	char       *init, char50[50], char200[200];
	int      *addCount;
	DArray   *vmCodeNew, *addCode;
	DArray   *addRegType;
	DVarray  *regConst;
	DVarray  *routConsts = self->routConsts;
	DVarray  *dataCL[2] = { NULL, NULL };
	DArray   *typeVL[2] = { NULL, NULL };
	DArray   *typeVO[2] = { NULL, NULL };
	DArray   *dataCK = hostClass ? hostClass->cstDataTable : NULL;
	DArray   *typeVK = hostClass ? hostClass->glbTypeTable : NULL;
	DArray   *dataCG = self->nameSpace->cstDataTable;
	DArray   *typeVG = self->nameSpace->varTypeTable;
	DValue    empty = daoNullValue;
	DValue    val = daoNullValue;
	DValue   *csts;
	DValue   *pp;
	int isconst = self->attribs & DAO_ROUT_ISCONST;
	int notide = ! (vms->options & DAO_EXEC_IDE);
	/* To support Edit&Continue in DaoStudio, some of the features
	 * have to be switched off:
	 * (1) function specialization based on parameter types;
	 * (2) instruction specialization requiring
	 *     additional instructions and vm registers; */

	 dataCL[0] = self->routConsts;
	 if( hostClass ) typeVO[0] = hostClass->objDataType;
	 if( self->upRoutine ){
		 if( self->upRoutine->parser ) DaoRoutine_Compile( self->upRoutine );
		 dataCL[1] = self->upRoutine->routConsts;
		 typeVL[1] = self->upRoutine->regType;
	 }

	if( self->vmCodes->size ==0 ) return 1;
	defs = DMap_New(0,0);
	init = dao_malloc( self->locRegCount );
	memset( init, 0, self->locRegCount );
	addCount = dao_malloc( self->vmCodes->size * sizeof(int) );
	memset( addCount, 0, self->vmCodes->size * sizeof(int) );
	vmCodeNew = DArray_New( D_VMCODE );
	addCode = DArray_New( D_VMCODE );
	addRegType = DArray_New(0);
	mbs = DString_New(1);

	any = DaoNameSpace_MakeType( ns, "any", DAO_ANY, NULL, NULL, 0 );
	udf = DaoNameSpace_MakeType( ns, "?", DAO_UDF, NULL, NULL, 0 );
	inumt = DaoNameSpace_MakeType( ns, "int", DAO_INTEGER, NULL, NULL, 0 );
	fnumt = DaoNameSpace_MakeType( ns, "float", DAO_FLOAT, NULL, NULL, 0 );
	dnumt = DaoNameSpace_MakeType( ns, "double", DAO_DOUBLE, NULL, NULL, 0 );
	comt = DaoNameSpace_MakeType( ns, "complex", DAO_COMPLEX, NULL, NULL, 0 );
	longt = DaoNameSpace_MakeType( ns, "long", DAO_LONG, NULL, NULL, 0 );
	enumt = DaoNameSpace_MakeType( ns, "enum", DAO_ENUM, NULL, NULL, 0 );
	strt = DaoNameSpace_MakeType( ns, "string", DAO_STRING, NULL, NULL, 0 );
	ilst = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, &inumt, 1 );
	flst = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, &fnumt, 1 );
	dlst = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, &dnumt, 1 );
	slst = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, &strt, 1 );
	iart = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY, NULL, &inumt, 1 );
	fart = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY, NULL, &fnumt, 1 );
	dart = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY, NULL, &dnumt, 1 );
	cart = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY, NULL, &comt, 1 );

	ts[0] = simtps[0] = arrtps[0] = any;
	simtps[DAO_INTEGER] = inumt;
	simtps[DAO_FLOAT] = fnumt;
	simtps[DAO_DOUBLE] = dnumt;
	simtps[DAO_COMPLEX] = comt;
	simtps[DAO_LONG] = longt;
	simtps[DAO_ENUM] = enumt;
	simtps[DAO_STRING] = strt;
	arrtps[DAO_INTEGER] = iart;
	arrtps[DAO_FLOAT] = fart;
	arrtps[DAO_DOUBLE] = dart;
	arrtps[DAO_COMPLEX] = cart;
	ts[DAO_INTEGER] = inumt;
	ts[DAO_FLOAT] = fnumt;
	ts[DAO_DOUBLE] = dnumt;
	ts[DAO_COMPLEX] = comt;
	ts[DAO_LONG] = longt;
	ts[DAO_ENUM] = enumt;
	ts[DAO_STRING] = strt;

	GC_DecRCs( self->regType );
	regConst = DVarray_New();
	DVarray_Resize( regConst, self->locRegCount, daoNullValue );
	DArray_Resize( self->regType, self->locRegCount, 0 );
	type = self->regType->items.pType;
	csts = regConst->data;

	if( self->routName->mbs[0] == '@' && self->routType->nested->size ){
		DaoType *ftype = self->routType->nested->items.pType[0];
		if( ftype->tid == DAO_PAR_NAMED && ftype->X.abtype->tid == DAO_ROUTINE ){
			ftype = ftype->X.abtype;
			for(i=0; i<ftype->nested->size; i++) init[i+self->parCount] = 1;
		}
	}
	for(i=0; i<self->routType->nested->size; i++){
		init[i] = 1;
		type[i] = self->routType->nested->items.pType[i];
		if( type[i] && type[i]->tid == DAO_PAR_VALIST ) type[i] = NULL;
		if( self->constParam & (1<<i) ) csts[i].cst = 1;
		tt = type[i];
		if( tt ) type[i] = tt->X.abtype; /* name:type, name=type */
		node = MAP_Find( self->localVarType, i );
		if( node == NULL ) continue;
		if( node->value.pType == NULL || type[i] == NULL ) continue;
		DaoType_MatchTo( type[i], node->value.pType, defs );
	}
	node = DMap_First( self->localVarType );
	for( ; node !=NULL; node = DMap_Next(self->localVarType,node) ){
		if( node->key.pInt < self->routType->nested->size ) continue;
		type[ node->key.pInt ] = DaoType_DefineTypes( node->value.pType, ns, defs );
	}
	/* reference counts of types should be updated right in place,
	 * not at the end of this funciton), because function specialization
	 * may decrease the reference count of a type before it get increased! */
	DaoGC_IncRCs( self->regType );

	/*
	   printf( "DaoRoutine_InferTypes() %p %s %i %i\n", self, self->routName->mbs, self->parCount, self->locRegCount );
	   if( self->routType ) printf( "%p %p\n", hostClass, self->routType->X.extra );
	   DaoRoutine_PrintCode( self, self->nameSpace->vmSpace->stdStream );
	 */

	errors = DArray_New(0);
	for(i=0; i<N; i++){
		/* adding type to namespace may add constant data as well */
		cid = i;
		error = NULL;
		vmc = vmcs[i];
		vmc2 = * self->annotCodes->items.pVmc[i];
		code = vmc->code;
		opa = vmc->a;  opb = vmc->b;  opc = vmc->c;
		at = opa < M ? type[opa] : NULL;
		bt = opb < M ? type[opb] : NULL;
		ct = opc < M ? type[opc] : NULL;
		TT0 = vmcTyping[code][0]; TT1 = vmcTyping[code][1]; TT2 = vmcTyping[code][2];
		TT3 = vmcTyping[code][3]; TT4 = vmcTyping[code][4]; TT5 = vmcTyping[code][5];
		TT6 = vmcTyping[code][6];
		if( TT1 > 'A' ) TT1 = mapTyping[ TT1 - 'A' ];
		if( TT2 > 'A' ) TT2 = mapTyping[ TT2 - 'A' ];
		if( TT3 > 'A' ) TT3 = mapTyping[ TT3 - 'A' ];
		addCount[i] += i ==0 ? 0 : addCount[i-1];
		node = DMap_First( defs );
		while( node !=NULL ){
			DaoType *abtp = (DaoType*) node->key.pBase;
			if( abtp->tid == DAO_UDF ){
				DMap_Erase( defs, (void*) abtp );
				node = DMap_First( defs );
				continue;
			}
			node = DMap_Next( defs, node );
		}

#if 0
		DaoTokens_AnnotateCode( self->source, vmc2, mbs, 24 );
		printf( "%4i: ", i );DaoVmCodeX_Print( vmc2, mbs->mbs );
#endif
		switch( code ){
		case DVM_NOP :
		case DVM_DEBUG :
			break;
		case DVM_DATA :
			if( opa > DAO_STRING ) GOTO_ERROR( DTE_DATA_CANNOT_CREATE, 0, NULL, NULL );
			init[opc] = 1;
			lastcomp = opc;
			at = simtps[ opa ];
			if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ){
				UpdateType( opc, at );
			}else{
				AssertTypeMatching( at, type[opc], defs, 0);
			}
			break;
		case DVM_GETCL : case DVM_GETCK : case DVM_GETCG :
			{
				if( code == DVM_GETCL ) val = dataCL[opa]->data[opb];
				else if( code == DVM_GETCK ) val = dataCK->items.pVarray[opa]->data[opb];
				else /* code == DVM_GETCG */ val = dataCG->items.pVarray[opa]->data[opb];
				at = DaoNameSpace_GetTypeV( ns, val );

				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, at );
				/*
				   printf( "at %i %i\n", at->tid, type[opc]->tid );
				 */
				AssertTypeMatching( at, type[opc], defs, 0);
				csts[opc] = val;
				csts[opc].cst = val.cst;
				init[opc] = 1;
				lastcomp = opc;
				break;
			}
		case DVM_GETVL : case DVM_GETVO : case DVM_GETVK : case DVM_GETVG :
			{
				at = 0;
				if( code == DVM_GETVL ) at = typeVL[opa]->items.pType[opb];
				else if( code == DVM_GETVO ) at = typeVO[opa]->items.pType[opb];
				else if( code == DVM_GETVK ) at = typeVK->items.pArray[opa]->items.pType[opb];
				else /* code == DVM_GETVG */ at = typeVG->items.pArray[opa]->items.pType[opb];
				if( at == NULL ) at = any;
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, at );
				/*
				   printf( "%s\n", at->name->mbs );
				   printf( "%p %p\n", at, type[opc] );
				   printf( "%s %s\n", at->name->mbs, type[opc]->name->mbs );
				 */
				AssertTypeMatching( at, type[opc], defs, 0);
				init[opc] = 1;
				lastcomp = opc;
				if( isconst && code == DVM_GETVO ) csts[opc].cst = 1;
				break;
			}
		case DVM_SETVL : case DVM_SETVO : case DVM_SETVK : case DVM_SETVG :
			{
				if( isconst && code == DVM_SETVO ) goto ModifyConstant;
				AssertInitialized( opa, 0, vmc->middle + 1, vmc->last );
				tp = NULL;
				if( code == DVM_SETVL ) tp = typeVL[opc]->items.pType + opb;
				else if( code == DVM_SETVO ) tp = typeVO[opc]->items.pType + opb;
				else if( code == DVM_SETVK ) tp = typeVK->items.pArray[opc]->items.pType + opb;
				else /* code == DVM_SETVG */ tp = typeVG->items.pArray[opc]->items.pType + opb;
				at = type[opa];
				if( tp && ( *tp==NULL || (*tp)->tid ==DAO_UDF ) ){
					GC_ShiftRC( at, *tp );
					*tp = at;
				}
				if( tp && (*tp)->tid <= DAO_DOUBLE ){
					if( code == DVM_SETVO && opb < hostClass->objDataDefault->size ){
						hostClass->objDataDefault->data[ opb ].t = at->tid;
					}else if( code == DVM_SETVK && opc < hostClass->glbDataTable->size ){
						DVarray *array = hostClass->glbDataTable->items.pVarray[opc];
						if( opb < array->size ) array->data[opb].t = at->tid;
					}else if( code == DVM_SETVG && opc < ns->varDataTable->size ){
						DVarray *array = ns->varDataTable->items.pVarray[opc];
						if( opb < array->size ) array->data[opb].t = at->tid;
					}
				}
				/* less strict checking */
				if( type[opa]->tid == DAO_ANY || type[opa]->tid == DAO_UDF ) break;
				if( tp == 0 ) break;
				/*
				   printf( "%s %s\n", (*tp)->name->mbs, type[opa]->name->mbs );
				   printf( "ns=%p, tp=%p, tps=%p\n", ns, *tp, varTypes[opc] );
				   printf( "%i %i\n", tp[0]->tid - DAO_INTEGER, at->tid - DAO_INTEGER );
				 */
				k = DaoType_MatchTo( type[opa], *tp, defs );
				if( k ==0 ) ErrorTypeNotMatching( 0, type[opa], *tp );
				at = type[opa];
				if( tp[0]->tid >= DAO_INTEGER && tp[0]->tid <= DAO_DOUBLE
						&& at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
					if( typed_code ){
						vmc->code = 3*(tp[0]->tid - DAO_INTEGER) + (at->tid - DAO_INTEGER) + DVM_SETVL_II;
						vmc->code += 9*(code - DVM_SETVL);
					}
				}else if( k == DAO_MT_SUB && notide ){
					/* global L = { 1.5, 2.5 }; L = { 1, 2 }; L[0] = 3.5 */
					addCount[i] ++;
					vmc2.code = DVM_CAST;
					vmc2.a = opa;
					vmc2.c = self->locRegCount + addRegType->size -1;
					vmc->a = vmc2.c;
					DArray_Append( addCode, & vmc2 );
					DArray_Append( addRegType, *tp );
				}
				break;
			}
		case DVM_GETI :
			{
				csts[opc].cst = csts[opa].cst;
				lastcomp = opc;
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				at = type[opa];
				bt = type[opb];
				ct = NULL;
				container = at;
				indexkey = bt;
				if( NoCheckingType( at ) || NoCheckingType( bt ) ){
					/* allow less strict typing: */
					ct = any;
				}else if( at->tid == DAO_INTEGER ){
					ct = inumt;
					if( bt->tid > DAO_DOUBLE && bt->tid != DAO_PAIR ) goto InvIndex;
					if( bt->tid == DAO_PAIR ) AssertPairNumberType( bt );
				}else if( at->tid == DAO_STRING ){
					ct = at;
					if( bt->tid >= DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
						ct = inumt;
						if( typed_code ){
							if( bt->tid == DAO_INTEGER ){
								vmc->code = DVM_GETI_SI;
							}else if( bt->tid == DAO_FLOAT && notide ){
								ct = inumt;
								vmc->code = DVM_GETI_SI;
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_IF );
							}else if( bt->tid == DAO_DOUBLE && notide ){
								ct = inumt;
								vmc->code = DVM_GETI_SI;
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_ID );
							}
						}
					}else if( bt == dao_type_for_iterator ){
						ct = inumt;
					}else if( bt->tid ==DAO_PAIR ){
						ct = at;
						AssertPairNumberType( bt );
					}else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2 ){
						ct = at;
						AssertPairNumberType( bt );
					}else if( bt->tid ==DAO_LIST || bt->tid ==DAO_ARRAY ){
						/* passed */
						k = bt->nested->items.pType[0]->tid;
						if( k > DAO_DOUBLE && k !=DAO_ANY ) goto NotMatch;
					}
				}else if( at->tid == DAO_LONG ){
					ct = inumt; /* XXX slicing */
				}else if( at->tid == DAO_TYPE ){
					at = at->nested->items.pType[0];
					if( at->tid == DAO_ENUM && at->mapNames ){
						ct = at; /* TODO const index */
					}else{
						goto NotExist;
					}
				}else if( at->tid == DAO_LIST ){
					/*
					 */
					if( bt->tid == DAO_INTEGER || bt->tid == DAO_FLOAT
							|| bt->tid == DAO_DOUBLE ){
						ct = at->nested->items.pType[0];
						if( typed_code && notide ){
							if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE ){
								vmc->code = DVM_GETI_LII + ct->tid - DAO_INTEGER;
							}else if( ct->tid == DAO_STRING ){
								vmc->code = DVM_GETI_LSI;
							}else if( ct->tid >= DAO_ARRAY && ct->tid < DAO_ANY ){
								/* for skipping type checking */
								vmc->code = DVM_GETI_LI;
							}

							if( bt->tid == DAO_FLOAT ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_IF );
							}else if( bt->tid == DAO_DOUBLE ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_ID );
							}
						}
					}else if( bt == dao_type_for_iterator ){
						ct = at->nested->items.pType[0];
					}else if( bt->tid ==DAO_PAIR ){
						ct = at;
						AssertPairNumberType( bt );
					}else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2 ){/*XXX a[1,2] a[(1,2)]*/
						ct = at;
						AssertPairNumberType( bt );
					}else if( bt->tid ==DAO_LIST || bt->tid ==DAO_ARRAY ){
						ct = at;
						k = bt->nested->items.pType[0]->tid;
						if( k !=DAO_INTEGER && k !=DAO_FLOAT && k !=DAO_ANY && k !=DAO_UDF )
							goto NotMatch;
					}else{
						goto InvIndex;
					}
				}else if( at->tid == DAO_MAP ){
					DaoType *t0 = at->nested->items.pType[0];
					/*
					   printf( "at %s %s\n", at->name->mbs, bt->name->mbs );
					 */
					if( bt->tid == DAO_PAIR ){
						ts[0] = bt->nested->items.pType[0];
						ts[1] = bt->nested->items.pType[1];
						if( ( ts[0]->tid != DAO_UDF && ts[0]->tid != DAO_ANY
									&& DaoType_MatchTo( ts[0], t0, defs ) ==0 )
								|| ( ts[1]->tid != DAO_UDF && ts[1]->tid != DAO_ANY
									&& DaoType_MatchTo( ts[1], t0, defs ) ==0 ) ){
							goto InvKey;
						}
						ct = at;
					}else if( bt == dao_type_for_iterator ){
						ct = DaoNameSpace_MakeType( ns, "tuple", DAO_TUPLE,
								NULL, at->nested->items.pType, 2 );
					}else{
						if( DaoType_MatchTo( bt, t0, defs ) ==0) goto InvKey;
						ct = at->nested->items.pType[1];
					}
				}else if( at->tid == DAO_ARRAY ){
					if( bt->tid == DAO_INTEGER || bt->tid == DAO_FLOAT
							|| bt->tid == DAO_DOUBLE ){
						/* array[i] */
						ct = at->nested->items.pType[0];
						if( typed_code && notide ){
							if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE )
								vmc->code = DVM_GETI_AII + ct->tid - DAO_INTEGER;
							else if( ct->tid == DAO_COMPLEX )
								vmc->code = DVM_GETI_ACI;

							if( bt->tid == DAO_FLOAT ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_IF );
							}else if( bt->tid == DAO_DOUBLE ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_ID );
							}
						}
					}else if( bt->tid ==DAO_PAIR ){
						ct = at;
						AssertPairNumberType( bt );
					}else if( bt == dao_type_for_iterator ){
						ct = at->nested->items.pType[0];
					}else if( bt->tid ==DAO_TUPLE ){
						ct = at->nested->items.pType[0];
						for(j=0; j<bt->nested->size; j++){
							int tid = bt->nested->items.pType[j]->tid;
							if( tid ==0 || tid > DAO_DOUBLE ){
								ct = at;
								break;
							}
						}
					}else if( bt->tid == DAO_LIST || bt->tid == DAO_ARRAY ){
						/* array[ {1,2,3} ] or array[ [1,2,3] ] */
						ct = at;
						k = bt->nested->items.pType[0]->tid;
						if( k !=DAO_INTEGER && k !=DAO_FLOAT && k !=DAO_ANY && k !=DAO_UDF )
							goto NotMatch;
					}
				}else if( at->tid ==DAO_TUPLE ){
					ct = any;
					val = csts[opb];
					if( val.t ){
						if( val.t > DAO_DOUBLE ) goto InvIndex;
						k = DValue_GetInteger( val );
						if( k <0 || k >= at->nested->size ) goto InvIndex;
						ct = at->nested->items.pType[ k ];
						if( ct->tid == DAO_PAR_NAMED ) ct = ct->X.abtype;
						if( typed_code ){
							if( k < 0xffff ){
								if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE ){
									vmc->b = k;
									vmc->code = DVM_GETF_TI + ( ct->tid - DAO_INTEGER );
								}else if( ct->tid == DAO_STRING ){
									vmc->b = k;
									vmc->code = DVM_GETF_TS;
								}else if( ct->tid >= DAO_ARRAY && ct->tid < DAO_ROUTINE ){
									/* for skipping type checking */
									vmc->b = k;
									vmc->code = DVM_GETF_T;
								}
							}
						}
					}else if( bt->tid >= DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
						if( typed_code && bt->tid == DAO_INTEGER ){
							vmc->code = DVM_GETI_TI;
						}else if( typed_code && notide ){
							vmc->code = DVM_GETI_TI;
							addCount[i] ++;
							vmc2.code = DVM_CAST;
							vmc2.a = opb;
							vmc2.b = 0;
							vmc2.c = self->locRegCount + addRegType->size -1;
							DArray_Append( addCode, & vmc2 );
							DArray_Append( addRegType, inumt );
							vmc->b = vmc2.c;
						}
					}else if( bt->tid != DAO_UDF && bt->tid != DAO_ANY ){
						goto InvIndex;
					}
				}else if( at->tid == DAO_OBJECT && (rout = (DRoutine*) DaoClass_FindOperator( at->X.klass, "[]", hostClass )) ){
					rout2 = rout;
					rout = MatchByParamType( rout, at, NULL, & bt, 1, DVM_CALL, &min, &spec );
					if( rout == NULL ){
						DRoutine_ShowError( rout2, at, NULL, & bt, 1, DVM_CALL, errors );
						goto InvIndex;
					}
					ct = rout->routType->X.abtype;
				}else if( at->tid == DAO_CDATA ){
					DString_SetMBS( mbs, "[]" );
					rout2 = rout = (DRoutine*) DaoFindFunction( at->typer, mbs );
					if( rout == NULL ) goto WrongContainer;
					rout = MatchByParamType( rout, at, NULL, & bt, 1, DVM_CALL, &min, &spec );
					if( rout == NULL ){
						DRoutine_ShowError( rout2, at, NULL, & bt, 1, DVM_CALL, errors );
						goto InvIndex;
					}
					ct = rout->routType->X.abtype;
				}else if( at->tid == DAO_INTERFACE ){
					DString_SetMBS( mbs, "[]" );
					node = DMap_Find( at->X.inter->methods, mbs );
					if( node == NULL ) goto WrongContainer;
					rout2 = rout = (DRoutine*)node->value.pBase;
					rout = MatchByParamType( rout, at, NULL, & bt, 1, DVM_CALL, &min, &spec );
					if( rout == NULL ){
						DRoutine_ShowError( rout2, at, NULL, & bt, 1, DVM_CALL, errors );
						goto InvIndex;
					}
					ct = rout->routType->X.abtype;
				}else if( at->tid == DAO_UDF || at->tid == DAO_ANY
						|| at->tid == DAO_INITYPE ){
					ct = any;
				}else if( at->typer ){
					DString_SetMBS( mbs, "[]" );
					rout2 = rout = (DRoutine*) DaoFindFunction( at->typer, mbs );
					if( rout == NULL ) goto WrongContainer;
					rout = MatchByParamType( rout, at, NULL, & bt, 1, DVM_CALL, &min, &spec );
					if( rout == NULL ){
						DRoutine_ShowError( rout2, at, NULL, & bt, 1, DVM_CALL, errors );
						goto InvIndex;
					}
					ct = rout->routType->X.abtype;
				}else{
					goto WrongContainer;
				}
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				/*
				   DaoVmCodeX_Print( *vmc, NULL );
				   printf( "at %s %s %i\n", at->name->mbs, bt->name->mbs, bt->tid );
				   if(ct) printf( "ct %s %s\n", ct->name->mbs, type[opc]->name->mbs );
				 */
				AssertTypeMatching( ct, type[opc], defs, 0);
				break;
			}
		case DVM_GETF :
			{
				int ak = 0;
				lastcomp = opc;
				csts[opc].cst = csts[opa].cst;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ct = NULL;
				val = routConsts->data[opb];
				if( val.t != DAO_STRING ) goto NotMatch;
				str = val.v.s;
				at = type[opa];
				ak = at->tid ==DAO_CLASS;
				error = str;
				if( NoCheckingType( at ) ){
					/* allow less strict typing: */
					ct = any;
				}else if( at->tid == DAO_TYPE ){
					at = at->nested->items.pType[0];
					if( at->tid == DAO_ENUM && at->mapNames ){
						if( DMap_Find( at->mapNames, str ) == NULL ) goto NotExist;
						ct = at;
					}else{
						goto NotExist;
					}
				}else if( at->tid == DAO_INTERFACE ){
					node = DMap_Find( at->X.inter->methods, str );
					if( node ){
						ct = ((DRoutine*)node->value.pBase)->routType;
					}else{
						DString_SetMBS( mbs, "." );
						DString_Append( mbs, str );
						node = DMap_Find( at->X.inter->methods, mbs );
						if( node == NULL ) goto NotExist;
						rout2 = rout = (DRoutine*)node->value.pBase;
						rout = MatchByParamType( rout, at, NULL, & bt, 0, DVM_CALL, &min, &spec );
						if( rout == NULL ){
							DRoutine_ShowError( rout2, at, NULL, & bt, 0, DVM_CALL, errors );
							goto NotMatch;
						}
						ct = rout->routType->X.abtype;
					}
				}else if( at->tid == DAO_CLASS || at->tid == DAO_OBJECT ){
					int getter = 0;
					klass = at->X.klass;
					tp = DaoClass_GetDataType( klass, str, & j, hostClass );
					if( j ){
						DString_SetMBS( mbs, "." );
						DString_Append( mbs, str );
						tp = DaoClass_GetDataType( klass, mbs, & j, hostClass );
						DaoClass_GetData( klass, mbs, & val, hostClass, NULL );
						if( j==0 && tp == NULL ) ct = DaoNameSpace_GetTypeV( ns, val );
						if( val.t && ct && ct->tid == DAO_ROUTINE ){
							rout2 = rout = (DRoutine*) val.v.routine;
							if( rout == NULL ) goto NotExist;
							rout = MatchByParamType( rout, at, NULL, & bt, 0, DVM_CALL, &min, &spec );
							if( rout == NULL ){
								DRoutine_ShowError( rout2, at, NULL, & bt, 0, DVM_CALL, errors );
								goto NotMatch;
							}
							ct = rout->routType->X.abtype;
							getter = 1;
							if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
							AssertTypeMatching( ct, type[opc], defs, 0);
						}
					}
					DString_Assign( mbs, at->name );
					DString_AppendChar( mbs, '.' );
					DString_Append( mbs, str );
					if( j == DAO_ERROR_FIELD_NOTPERMIT ) goto NotPermit;
					if( j == DAO_ERROR_FIELD_NOTEXIST ) goto NotExist;
					j = DaoClass_GetDataIndex( klass, str, & k );
					if( k == DAO_OBJECT_VARIABLE && at->tid ==DAO_CLASS ) goto NeedInstVar;
					if( k == DAO_CLASS_VARIABLE ) csts[opc].cst = 0;
					if( getter ) break;
					if( tp ==NULL ){
						DaoClass_GetData( klass, str, & val, hostClass, NULL );
						ct = DaoNameSpace_GetTypeV( ns, val );
						csts[opc] = val;
					}else{
						ct = *tp;
					}
					if( typed_code && (klass->attribs & DAO_CLS_FINAL) ){
						/* specialize instructions for finalized class/instance: */
						vmc->b = DaoClass_GetDataIndex( klass, str, & k );
						if( ct && ct->tid >=DAO_INTEGER && ct->tid <= DAO_DOUBLE ){
							if( k == DAO_CLASS_CONSTANT )
								vmc->code = ak ? DVM_GETF_KCI : DVM_GETF_OCI;
							else if( k == DAO_CLASS_VARIABLE )
								vmc->code = ak ? DVM_GETF_KGI : DVM_GETF_OGI;
							else if( k == DAO_OBJECT_VARIABLE )
								vmc->code = DVM_GETF_OVI;
							vmc->code += 5 * ( ct->tid - DAO_INTEGER );
						}else{
							if( k == DAO_CLASS_CONSTANT )
								vmc->code = ak ? DVM_GETF_KC : DVM_GETF_OC;
							else if( k == DAO_CLASS_VARIABLE )
								vmc->code = ak ? DVM_GETF_KG : DVM_GETF_OG;
							else if( k == DAO_OBJECT_VARIABLE )
								vmc->code = DVM_GETF_OV;
						}
					}
				}else if( at->tid == DAO_TUPLE ){
					if( at->mapNames == NULL ) goto NotExist;
					node = MAP_Find( at->mapNames, str );
					if( node == NULL ) goto NotExist;
					k = node->value.pInt;
					if( k <0 || k >= at->nested->size ) goto NotExist;
					ct = at->nested->items.pType[ k ];
					if( ct->tid == DAO_PAR_NAMED ) ct = ct->X.abtype;
					if( typed_code && notide ){
						if( k < 0xffff ){
							if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE ){
								vmc->code = DVM_GETF_TI + ( ct->tid - DAO_INTEGER );
								vmc->b = k;
							}else if( ct->tid >= DAO_ARRAY && ct->tid < DAO_ROUTINE ){
								/* for skipping type checking */
								vmc->code = DVM_GETF_T;
								vmc->b = k;
							}
						}
					}
				}else if( at->tid == DAO_NAMESPACE ){
					ct = any;
					if( csts[opa].t == DAO_NAMESPACE && csts[opa].v.ns ){
						DaoNameSpace *ans = csts[opa].v.ns;
						k = DaoNameSpace_FindVariable( ans, str );
						if( k >=0 ){
							ct = DaoNameSpace_GetVariableType( ans, k );
						}else{
							k = DaoNameSpace_FindConst( ans, str );
							val = DaoNameSpace_GetConst( ans, k );
							if( val.t ) ct = DaoNameSpace_GetTypeV( ans, val );
						}
						if( k <0 ) goto NotExist;
					}
#if 0
					//}else if( at->tid == DAO_ANY || at->tid == DAO_INITYPE ){
					//  ct = any;
#endif
			}else if( at->typer ){
				DaoFunction *func;
				val = DaoFindValue( at->typer, str );
				if( val.t == DAO_FUNCTION ){ /* turn off DVM_GETF_M XXX */
					DaoFunction *func = val.v.func;
					ct = func->routType; /*XXX*/
					if( typed_code ){
#if 0
						if( 0 ){
							vmc->code = DVM_GETF_M;
							for(j=0; j<at->typer->priv->methCount; j++){
								if( at->typer->priv->methods[j] == func ){
									vmc->b = j;
									break;
								}
							}
						}
#endif
					}
					csts[opc].v.func = func;
					csts[opc].t = DAO_FUNCTION;
				}else if( val.t ){
					ct = DaoNameSpace_GetTypeV( ns, val );
					csts[opc] = val;
				}else{
					DString_SetMBS( mbs, "." );
					DString_Append( mbs, str );
					rout2 = rout = (DRoutine*) DaoFindFunction( at->typer, mbs );
					if( rout == NULL ) goto NotExist;
					rout = MatchByParamType( rout, at, NULL, & bt, 0, DVM_CALL, &min, &spec );
					if( rout == NULL ){
						DRoutine_ShowError( rout2, at, NULL, & bt, 0, DVM_CALL, errors );
						goto NotMatch;
					}
					ct = rout->routType->X.abtype;
				}
				if( ct == NULL ) ct = udf;
			}
			if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0);
			break;
			}
		case DVM_SETI :
			{
				ct = (DaoType*) type[opc];
				if( ct == NULL ) goto ErrorTyping;
				if( csts[opc].cst ) goto ModifyConstant;
				k = DaoTokens_FindLeftPair( self->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
				AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
				AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				at = type[opa];
				bt = type[opb];
				container = ct;
				indexkey = bt;
				itemvalue = at;
				if( NoCheckingType( at ) || NoCheckingType( bt ) ) break;
				switch( ct->tid ){
				case DAO_STRING :
					if( typed_code && notide ){
						if( ( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE )
								&& ( bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE ) ){
							vmc->code = DVM_SETI_SII;
							if( at->tid ==DAO_FLOAT ){
								InsertCodeMoveToInteger( vmc->a, DVM_MOVE_IF );
							}else if( at->tid ==DAO_DOUBLE ){
								InsertCodeMoveToInteger( vmc->a, DVM_MOVE_ID );
							}
							if( bt->tid ==DAO_FLOAT ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_IF );
							}else if( bt->tid ==DAO_DOUBLE ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_ID );
							}
						}
					}
					/* less strict checking */
					if( at->tid >= DAO_ARRAY && at->tid !=DAO_ANY ) goto NotMatch;

					if( bt->tid==DAO_PAIR && (at->tid==DAO_STRING || at->tid <= DAO_DOUBLE) ){
						/* passed */
						AssertPairNumberType( bt );
					}else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2
							&& (at->tid==DAO_STRING || at->tid <= DAO_DOUBLE) ){
						AssertPairNumberType( bt );
					}else if( bt->tid == DAO_LIST && at->tid <= DAO_DOUBLE ){
					}else if( bt->tid > DAO_DOUBLE && bt->tid !=DAO_ANY ){
						/* less strict checking */
						goto NotMatch;
					}
					break;
				case DAO_LONG :
					ct = inumt; /* XXX slicing */
					break;
				case DAO_LIST :
					ts[0] = ct->nested->items.pType[0];
					if( bt->tid >=DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
						ct = ct->nested->items.pType[0];
						if( typed_code && notide ){
							if( ct->tid >=DAO_INTEGER && ct->tid <= DAO_DOUBLE
									&& at->tid >=DAO_INTEGER && at->tid <= DAO_DOUBLE ){
								vmc->code = 3 * ( ct->tid - DAO_INTEGER ) + DVM_SETI_LIII
									+ at->tid - DAO_INTEGER;
							}else if( at->tid ==DAO_STRING && ct->tid ==DAO_STRING ){
								vmc->code = DVM_SETI_LSIS;
							}else if( at->tid >= DAO_ARRAY && at->tid < DAO_ANY ){
								AssertTypeMatching( at, ct, defs, 0);
								if( DString_EQ( at->name, ct->name ) )
									vmc->code = DVM_SETI_LI;
							}
							if( bt->tid ==DAO_FLOAT ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_IF );
							}else if( bt->tid ==DAO_DOUBLE ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_ID );
							}
						}
					}else if( bt->tid==DAO_PAIR ){
						AssertPairNumberType( bt );
						AssertTypeMatching( at, ts[0], defs, 0);
					}else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2 ){
						AssertPairNumberType( bt );
						AssertTypeMatching( at, ts[0], defs, 0);
					}else if( bt->tid==DAO_LIST || bt->tid==DAO_ARRAY ){
						k = bt->nested->items.pType[0]->tid;
						if( k !=DAO_INTEGER && k !=DAO_FLOAT && k !=DAO_ANY && k !=DAO_UDF )
							ErrorTypeNotMatching( 0, bt->nested->items.pType[0], inumt );
						AssertTypeMatching( at, ts[0], defs, 0);
					}else{
						ErrorTypeNotMatching( 0, bt, inumt );
					}
					break;
				case DAO_MAP :
					{
						DaoType *t0 = ct->nested->items.pType[0];
						DaoType *t1 = ct->nested->items.pType[1];
						/*
						   DaoVmCode_Print( *vmc, NULL );
						   printf( "%p %p %p\n", t0, t1, ct );
						   printf( "ct %p %s\n", ct, ct->name->mbs );
						   printf( "at %i %s %i %s\n", at->tid, at->name->mbs, t1->tid, t1->name->mbs );
						   printf( "bt %s %s\n", bt->name->mbs, t0->name->mbs );
						 */
						if( bt->tid == DAO_PAIR ){
							ts[0] = bt->nested->items.pType[0];
							ts[1] = bt->nested->items.pType[1];
							if( ( ts[0]->tid != DAO_UDF && ts[0]->tid != DAO_ANY
										&& DaoType_MatchTo( ts[0], t0, defs ) ==0 )
									|| ( ts[1]->tid != DAO_UDF && ts[1]->tid != DAO_ANY
										&& DaoType_MatchTo( ts[1], t0, defs ) ==0 ) ){
								goto InvKey;
							}
							AssertTypeMatching( at, t1, defs, 0);
						}else if( bt->tid==DAO_UDF || bt->tid==DAO_ANY ){
							/* less strict checking */
							AssertTypeMatching( at, t1, defs, 0);
						}else{
							AssertTypeMatching( bt, t0, defs, 0);
							AssertTypeMatching( at, t1, defs, 0);
						}
						break;
					}
				case DAO_ARRAY :
					if( bt->tid >=DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
						if( DaoType_MatchTo( at, ct, defs ) ) break;
						ct = ct->nested->items.pType[0];
						/* array[i] */
						if( typed_code && notide ){
							if( ct->tid >=DAO_INTEGER && ct->tid <= DAO_DOUBLE
									&& at->tid >=DAO_INTEGER && at->tid <= DAO_DOUBLE ){
								vmc->code = 3 * ( ct->tid - DAO_INTEGER ) + DVM_SETI_AIII
									+ at->tid - DAO_INTEGER;
							}else if( ct->tid == DAO_COMPLEX ){
								vmc->code = DVM_SETI_ACI;
							}else if( at->tid !=DAO_UDF && at->tid !=DAO_ANY ){
								AssertTypeMatching( at, ct, defs, 0);
							}
							if( bt->tid ==DAO_FLOAT ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_IF );
							}else if( bt->tid ==DAO_DOUBLE ){
								InsertCodeMoveToInteger( vmc->b, DVM_MOVE_ID );
							}
						}
						AssertTypeMatching( at, ct, defs, 0);
					}else if( bt->tid == DAO_LIST || bt->tid == DAO_ARRAY ){
						k = bt->nested->items.pType[0]->tid;
						if( k >=DAO_DOUBLE && k !=DAO_ANY ) goto NotMatch;
						/* imprecise checking */
						if( DaoType_MatchTo( at, ct->nested->items.pType[0], defs )==0
								&& DaoType_MatchTo( at, ct, defs )==0 )
							goto NotMatch;
					}
					break;
				case DAO_TUPLE :
					val = csts[opb];
					if( val.t ){
						if( val.t > DAO_DOUBLE ) goto InvIndex;
						k = DValue_GetInteger( val );
						if( k <0 || k >= ct->nested->size ) goto InvIndex;
						ct = ct->nested->items.pType[ k ];
						if( ct->tid == DAO_PAR_NAMED ) ct = ct->X.abtype;
						AssertTypeMatching( at, ct, defs, 0);
						if( typed_code ){
							if( k < 0xffff && DString_EQ( at->name, ct->name ) ){
								if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE
										&& at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
									vmc->code = DVM_SETF_TII + 3*( ct->tid - DAO_INTEGER )
										+ (at->tid - DAO_INTEGER);
									vmc->b = k;
								}else if( at->tid ==DAO_STRING && ct->tid ==DAO_STRING ){
									vmc->code = DVM_SETF_TSS;
									vmc->b = k;
								}else if( at->tid >= DAO_ARRAY && at->tid < DAO_ROUTINE ){
									vmc->code = DVM_SETF_T;
									vmc->b = k;
								}
							}
						}
					}else if( bt->tid >= DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
						if( typed_code && bt->tid == DAO_INTEGER ){
							vmc->code = DVM_SETI_TI;
						}else if( typed_code && notide ){
							vmc->code = DVM_SETI_TI;
							addCount[i] ++;
							vmc2.code = DVM_CAST;
							vmc2.a = opb;
							vmc2.c = self->locRegCount + addRegType->size -1;
							DArray_Append( addCode, & vmc2 );
							DArray_Append( addRegType, inumt );
							vmc->b = vmc2.c;
						}
					}else if( bt->tid != DAO_UDF && bt->tid != DAO_ANY ){
						goto InvIndex;
					}
					break;
				case DAO_OBJECT :
					if( (rout=(DRoutine*) DaoClass_FindOperator( ct->X.klass, "[]=", hostClass )) == NULL)
						goto InvIndex;
					ts[0] = bt;
					ts[1] = at;
					rout2 = rout;
					rout = MatchByParamType( rout, ct, NULL, ts, 2, DVM_CALL, &min, &spec );
					if( rout == NULL ){
						DRoutine_ShowError( rout2, ct, NULL, ts, 2, DVM_CALL, errors );
						goto InvIndex;
					}
					break;
				case DAO_CDATA :
					DString_SetMBS( mbs, "[]=" );
					rout2 = rout = (DRoutine*) DaoFindFunction( ct->typer, mbs );
					if( rout == NULL ) goto InvIndex;
					ts[0] = bt;
					ts[1] = at;
					rout = MatchByParamType( rout, ct, NULL, ts, 2, DVM_CALL, &min, &spec );
					if( rout == NULL ){
						DRoutine_ShowError( rout2, ct, NULL, ts, 2, DVM_CALL, errors );
						goto InvIndex;
					}
					break;
				default : break;
				}
				break;
			}
		case DVM_GETMF :
			{
				lastcomp = opc;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ct = any;
				val = routConsts->data[opb];
				if( val.t != DAO_STRING ) goto NotMatch;
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0);
				csts[opc].cst = csts[opa].cst;
				break;
			}
		case DVM_SETF :
			{
				int ck;
				ct = type[opc];
				at = type[opa];
				if( csts[opc].cst ) goto ModifyConstant;
				AssertInitialized( opa, 0, vmc->middle + 1, vmc->last );
				AssertInitialized( opc, 0, 0, vmc->middle - 1 );
				/*
				   printf( "a: %s\n", type[opa]->name->mbs );
				   printf( "c: %s\n", type[opc]->name->mbs );
				 */
				val = routConsts->data[opb];
				if( val.t != DAO_STRING ){
					printf( "field: %i\n", val.t );
					goto NotMatch;
				}
				str = val.v.s;
				switch( ct->tid ){
				case DAO_UDF :
				case DAO_ANY :
				case DAO_INITYPE :
					/* allow less strict typing: */
					break;
				case DAO_CLASS :
				case DAO_OBJECT :
					{
						int setter = 0;
						ck = ct->tid ==DAO_CLASS;
						klass = (DaoClass*) type[opc]->X.extra;
						tp = DaoClass_GetDataType( klass, str, & j, hostClass );
						if( STRCMP( str, "self" ) ==0 ) goto NotPermit;
						if( j ){
							DString_SetMBS( mbs, "." );
							DString_Append( mbs, str );
							DString_AppendMBS( mbs, "=" );
							tp = DaoClass_GetDataType( klass, mbs, & j, hostClass );
							DaoClass_GetData( klass, mbs, & val, hostClass, NULL );
							if( j==0 && tp == NULL ) ct = DaoNameSpace_GetTypeV( ns, val );
							if(  ct && ct->tid == DAO_ROUTINE ){
								rout2 = rout = (DRoutine*) val.v.routine;
								setter = 1;
								ts[0] = type[opc];
								ts[1] = at;
								rout = MatchByParamType( rout, ct, NULL, ts, 2, DVM_MCALL, &min, &spec );
								if( rout == NULL ){
									DRoutine_ShowError( rout2, ct, NULL, ts, 2, DVM_MCALL, errors );
									goto NotMatch;
								}
							}
						}
						if( j == DAO_ERROR_FIELD_NOTPERMIT ) goto NotPermit;
						if( j == DAO_ERROR_FIELD_NOTEXIST ) goto NotExist;
						j = DaoClass_GetDataIndex( klass, str, & k );
						if( k == DAO_OBJECT_VARIABLE && ct->tid ==DAO_CLASS ) goto NeedInstVar;
						if( setter ) break;
						if( tp ==NULL ) goto NotPermit;
						if( *tp ==NULL ) *tp = type[opa];
						AssertTypeMatching( type[opa], *tp, defs, 0);
#if 0
						if( typed_code && (klass->attribs & DAO_CLS_FINAL) ){
							vmc->b = DaoClass_GetDataIndex( klass, str, & k );
							if( k == DAO_CLASS_CONSTANT ) goto InvOper;
							if( *tp && (*tp)->tid>=DAO_INTEGER && (*tp)->tid<=DAO_DOUBLE
									&& at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE ){
								if( k == DAO_CLASS_VARIABLE )
									vmc->code = ck ? DVM_SETF_KGII : DVM_SETF_OGII;
								else if( k == DAO_OBJECT_VARIABLE )
									vmc->code = DVM_SETF_OVII;
								vmc->code += 9 * ( (*tp)->tid - DAO_INTEGER )
									+ 3 * ( at->tid - DAO_INTEGER );
							}else{
								if( k == DAO_CLASS_VARIABLE )
									vmc->code = ck ? DVM_SETF_KG : DVM_SETF_OG;
								else if( k == DAO_OBJECT_VARIABLE )
									vmc->code = DVM_SETF_OV;
							}
						}
#endif
						break;
					}
				case DAO_TUPLE :
					{
						if( ct->mapNames == NULL ) goto NotExist;
						node = MAP_Find( ct->mapNames, str );
						if( node == NULL ) goto NotExist;
						k = node->value.pInt;
						if( k <0 || k >= ct->nested->size ) goto InvIndex;
						ct = ct->nested->items.pType[ k ];
						if( ct->tid == DAO_PAR_NAMED ) ct = ct->X.abtype;
						AssertTypeMatching( at, ct, defs, 0);
						if( typed_code && k < 0xffff && DString_EQ( at->name, ct->name ) ){
							if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE
									&& at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
								vmc->code = DVM_SETF_TII + 3*( ct->tid - DAO_INTEGER )
									+ (at->tid - DAO_INTEGER);
								vmc->b = k;
							}else if( at->tid ==DAO_STRING && ct->tid ==DAO_STRING ){
								vmc->code = DVM_SETF_TSS;
								vmc->b = k;
							}else if( at->tid >= DAO_ARRAY && at->tid < DAO_ROUTINE ){
								vmc->code = DVM_SETF_T;
								vmc->b = k;
							}
						}
						break;
					}
				case DAO_NAMESPACE :
					{
						if( csts[opc].t == DAO_NAMESPACE && csts[opc].v.ns ){
							DaoNameSpace *ans = csts[opc].v.ns;
							k = DaoNameSpace_FindVariable( ans, str );
							if( k >=0 ){
								ct = DaoNameSpace_GetVariableType( ans, k );
							}else{
								k = DaoNameSpace_FindConst( ans, str );
								val = DaoNameSpace_GetConst( ans, k );
								if( val.t ) ct = DaoNameSpace_GetTypeV( ans, val );
							}
							if( k <0 ) goto NotExist;
							AssertTypeMatching( at, ct, defs, 0);
						}
						break;
					}
				case DAO_CDATA :
					{
						DString_SetMBS( mbs, "." );
						DString_Append( mbs, str );
						DString_AppendMBS( mbs, "=" );
						rout2 = rout = (DRoutine*) DaoFindFunction( ct->typer, mbs );
						if( rout == NULL ) goto NotMatch;
						ts[0] = ct;
						ts[1] = at;
						rout = MatchByParamType( rout, ct, NULL, ts, 2, DVM_MCALL, &min, &spec );
						if( rout == NULL ){
							DRoutine_ShowError( rout2, ct, NULL, ts, 2, DVM_MCALL, errors );
							goto NotMatch;
						}
						break;
					}
				default: goto InvOper;
				}
				break;
			}
		case DVM_SETMF :
			{
				ct = type[opc];
				at = type[opa];
				if( csts[opc].cst ) goto ModifyConstant;
				AssertInitialized( opa, 0, vmc->middle + 1, vmc->last );
				AssertInitialized( opc, 0, 0, vmc->middle - 1 );
				/*
				   printf( "a: %s\n", type[opa]->name->mbs );
				   printf( "c: %s\n", type[opc]->name->mbs );
				 */
				val = routConsts->data[opb];
				if( val.t != DAO_STRING ) goto NotMatch;
				break;
			}
		case DVM_CAST :
			lastcomp = opc;
			AssertInitialized( opa, 0, vmc->middle + 1, vmc->last );
			init[opc] = 1;
			if( routConsts->data[opb].t != DAO_TYPE ) goto ErrorTyping;
			if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
			at = (DaoType*) routConsts->data[opb].v.p;
			if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, at );
			AssertTypeMatching( at, type[opc], defs, 0);
			at = type[opa];
			ct = type[opc];
			if( typed_code ){
				if( at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE
						&& ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE )
					vmc->code = DVM_MOVE_II + 3 * ( ct->tid - DAO_INTEGER )
						+ at->tid - DAO_INTEGER;
				else if( at->tid == DAO_COMPLEX && ct->tid == DAO_COMPLEX )
					vmc->code = DVM_MOVE_CC;
			}
			break;
		case DVM_MOVE :
		case DVM_LOAD :
			{
				if( code == DVM_MOVE && csts[opc].cst ) goto ModifyConstant;
				lastcomp = opc;
				AssertInitialized( opa, 0, vmc->middle ? vmc->middle + 1 : 0, vmc->last );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				at = type[opa];
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, at );
				if( code == DVM_MOVE ){
					ct = type[opc];
					if( ct == dao_array_empty ) UpdateType( opc, dao_array_any );
					if( ct == dao_list_empty ) UpdateType( opc, dao_list_any );
					if( ct == dao_map_empty ) UpdateType( opc, dao_map_any );
				}
				ct = type[opc];

				k = DaoType_MatchTo( at, type[opc], defs );

				/*
				   DaoVmCodeX_Print( *vmc, NULL );
				   if( type[opa] ) printf( "a: %s\n", type[opa]->name->mbs );
				   if( type[opc] ) printf( "c: %s\n", type[opc]->name->mbs );
				   printf( "%i  %i\n", DAO_MT_SUB, k );
				 */

				if( csts[opa].t == DAO_ROUTINE && type[opc]
						&& ( type[opc]->tid ==DAO_ROUTINE || type[opc]->tid ==DAO_FUNCTION ) ){
					/* a : routine<a:number,...> = overloaded_function; */
					rout = DRoutine_GetOverLoadByType( (DRoutine*)csts[opa].v.routine, type[opc] );
					if( rout == NULL ) goto NotMatch;
				}else if( at->tid ==DAO_UDF || at->tid ==DAO_ANY ){
					/* less strict checking */
				}else if( code == DVM_MOVE && at != ct && ct->tid == DAO_OBJECT ){
					rout = rout2 = (DRoutine*)DaoClass_FindOperator( ct->X.klass, "=", hostClass );
					if( rout ){
						rout = MatchByParamType( rout, ct, NULL, & at, 1, DVM_CALL, &min, &spec );
						if( rout == NULL ){
							DRoutine_ShowError( rout2, at, NULL, & bt, 1, DVM_CALL, errors );
							goto NotMatch;
						}
					}else if( k ==0 ){
						ErrorTypeNotMatching( 0, at, type[opc] );
					}
				}else if( at->tid ==DAO_TUPLE && DaoType_MatchTo(type[opc], at, defs)){
					/* less strict checking */
				}else if( k ==0 ){
					ErrorTypeNotMatching( 0, at, type[opc] );
				}

				/* necessary, because the register may be associated with a constant.
				 * beware of control flow: */
				if( vmc->b ) csts[opc] = csts[opa];

				if( k == DAO_MT_SUB && DString_EQ( at->name, ct->name ) ==0 ){
					/* L = { 1.5, 2.5 }; L = { 1, 2 }; L[0] = 3.5 */
					vmc->code = DVM_CAST;
					break;
				}
				if( typed_code && code != DVM_LOAD ){
					if( at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE
							&& ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE ){
						vmc->code = DVM_MOVE_II + 3 * ( ct->tid - DAO_INTEGER )
							+ at->tid - DAO_INTEGER;
					}else if( at->tid == DAO_COMPLEX && ct->tid == DAO_COMPLEX ){
						vmc->code = DVM_MOVE_CC;
					}else if( at->tid == DAO_STRING && ct->tid == DAO_STRING ){
						vmc->code = DVM_MOVE_SS;
					}else if( at->tid >= DAO_ARRAY && at->tid < DAO_ANY
							&& at->tid != DAO_ROUTINE && at->tid != DAO_CLASS ){
						if( DString_EQ( at->name, ct->name ) ){
							if( DString_FindChar( at->name, '?', 0 ) == MAXSIZE ){
								vmc->code = DVM_MOVE_PP;
							}
						}
					}
				}

				if( vmc->b == 0 ){
					ct = DaoType_DefineTypes( type[opc], ns, defs );
					if( ct ) UpdateType( opc, ct );
				}
				break;
			}
		case DVM_ADD : case DVM_SUB : case DVM_MUL :
		case DVM_DIV : case DVM_MOD : case DVM_POW :
			{
				lastcomp = opc;
				at = type[opa];
				bt = type[opb];
				if( csts[opc].cst ) goto ModifyConstant;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				init[opc] = 1;
				ct = NULL;
				/*
				   if( type[opa] ) printf( "a: %s\n", type[opa]->name->mbs );
				   if( type[opb] ) printf( "b: %s\n", type[opb]->name->mbs );
				   if( type[opc] ) printf( "c: %s\n", type[opc]->name->mbs );
				 */
				if( NoCheckingType( at ) || NoCheckingType( bt ) ){
					ct = any;
				}else if( at->tid == DAO_OBJECT || bt->tid == DAO_OBJECT
						|| at->tid == DAO_CDATA || bt->tid == DAO_CDATA
						|| at->tid == DAO_INTERFACE || bt->tid == DAO_INTERFACE ){
					ct = type[opc];
					ct = DaoCheckBinArith( self, vmc, at, bt, ct, hostClass, mbs );
					if( ct == NULL ) goto InvOper;
				}else if( at->tid == bt->tid ){
					ct = at;
					switch( at->tid ){
					case DAO_INTEGER : case DAO_FLOAT : case DAO_DOUBLE :
					case DAO_LONG :
						break;
					case DAO_COMPLEX :
						if( code == DVM_MOD ) goto InvOper; break;
					case DAO_STRING :
						if( code != DVM_ADD ) goto InvOper; break;
					case DAO_ENUM :
						if( code != DVM_ADD && code != DVM_SUB ) goto InvOper;
						if( at->name->mbs[0] =='$' && bt->name->mbs[0] =='$' ){
							ct = NULL;
							if( code == DVM_ADD ){
								ct = DaoNameSpace_SymbolTypeAdd( ns, at, bt, NULL );
							}else{
								ct = DaoNameSpace_SymbolTypeSub( ns, at, bt, NULL );
							}
							if( ct == NULL ) goto InvOper;
						}else if( at->name->mbs[0] =='$' ){
							ct = bt;
						}
						break;
					case DAO_LIST :
						if( code != DVM_ADD ) goto InvOper;
						AssertTypeMatching( bt, at, defs, 0);
						break;
					case DAO_ARRAY :
						break;
					default : goto InvOper;
					}
				}else if( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE
						&& bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE ){
					ct = at->tid > bt->tid ? at : bt;
				}else if( ( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE )
						&& (bt->tid ==DAO_COMPLEX || bt->tid == DAO_LONG
							|| bt->tid ==DAO_ARRAY) ){
					ct = bt;
				}else if( (at->tid ==DAO_COMPLEX || at->tid == DAO_LONG
							|| at->tid ==DAO_ARRAY)
						&& ( bt->tid >= DAO_INTEGER && bt->tid <=DAO_DOUBLE ) ){
					ct = at;
				}else if( at->tid ==DAO_STRING && bt->tid ==DAO_INTEGER && opa==opc  ){
					ct = at;
				}else if( ( at->tid ==DAO_COMPLEX && bt->tid ==DAO_ARRAY )
						|| ( at->tid ==DAO_ARRAY && bt->tid ==DAO_COMPLEX ) ){
					ct = cart;
				}else{
					goto InvOper;
				}
				if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				/* allow less strict typing: */
				if( ct->tid ==DAO_UDF || ct->tid == DAO_ANY ) continue;
				AssertTypeMatching( ct, type[opc], defs, 0 );
				ct = type[opc];
				if( i && typed_code ){
					if( at->tid == bt->tid && at->tid == ct->tid ){
						switch( at->tid ){
						case DAO_INTEGER :
							vmc->code += DVM_ADD_III - DVM_ADD; break;
						case DAO_FLOAT :
							vmc->code += DVM_ADD_FFF - DVM_ADD; break;
						case DAO_DOUBLE :
							vmc->code += DVM_ADD_DDD - DVM_ADD; break;
						case DAO_STRING :
							if( vmc->code == DVM_ADD ) vmc->code = DVM_ADD_SS;
							break;
						case DAO_COMPLEX :
							if( vmc->code <= DVM_DIV ) vmc->code += DVM_ADD_CC - DVM_ADD;
						}
					}else if( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE
							&& bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE ){
						switch( type[opc]->tid ){
						case DAO_FLOAT : vmc->code += DVM_ADD_FNN - DVM_ADD; break;
						case DAO_DOUBLE : vmc->code += DVM_ADD_DNN - DVM_ADD; break;
						default : break;
						}
					}
				}
				break;
			}
		case DVM_AND : case DVM_OR : case DVM_LT :
		case DVM_LE :  case DVM_EQ : case DVM_NE :
			{
				lastcomp = opc;
				at = type[opa];
				bt = type[opb];
				if( csts[opc].cst ) goto ModifyConstant;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				init[opc] = 1;
				ct = inumt;
				if( NoCheckingType( at ) || NoCheckingType( bt ) ){
					ct = any;
				}else if( at->tid == DAO_OBJECT || bt->tid == DAO_OBJECT
						|| at->tid == DAO_CDATA || bt->tid == DAO_CDATA
						|| at->tid == DAO_INTERFACE || bt->tid == DAO_INTERFACE ){
					ct = type[opc];
					ct = DaoCheckBinArith( self, vmc, at, bt, ct, hostClass, mbs );
					if( ct == NULL ) ct = inumt;
				}else if( at->tid == bt->tid ){
					ct = at;
					switch( at->tid ){
					case DAO_INTEGER : case DAO_FLOAT : case DAO_DOUBLE :
						break;
					case DAO_COMPLEX :
						ct = inumt;
						if( code < DVM_EQ ) goto InvOper;
						break;
					case DAO_LONG :
					case DAO_STRING :
					case DAO_LIST :
					case DAO_ARRAY :
						if( code > DVM_OR ) ct = inumt; break;
					case DAO_ENUM :
						ct = inumt;
						break;
					case DAO_TUPLE :
						ct = inumt;
						if( code < DVM_LT ){
							ct = at;
							if( DaoType_MatchTo( at, bt, defs ) != DAO_MT_EQ ) ct = any;
						}
						break;
					default :
						ct = inumt;
						if( code != DVM_EQ && code != DVM_NE ) goto InvOper;
					}
				}else if( at->tid >= DAO_INTEGER && at->tid <= DAO_LONG
						&& bt->tid >= DAO_INTEGER && bt->tid <= DAO_LONG
						&& at->tid != DAO_COMPLEX && bt->tid != DAO_COMPLEX ){
					ct = at->tid > bt->tid ? at : bt;
				}else if( code != DVM_EQ && code != DVM_NE ){
					goto InvOper;
				}
				if( type[opc] == NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				/* allow less strict typing: */
				if( ct->tid ==DAO_UDF || ct->tid == DAO_ANY ) continue;
				AssertTypeMatching( ct, type[opc], defs, 0 );
				ct = type[opc];
				if( i && typed_code ){
					if( at->tid == bt->tid && at->tid == ct->tid ){
						switch( at->tid ){
						case DAO_INTEGER :
							vmc->code += DVM_AND_III - DVM_AND; break;
						case DAO_FLOAT :
							vmc->code += DVM_AND_FFF - DVM_AND; break;
						case DAO_DOUBLE :
							vmc->code += DVM_AND_DDD - DVM_AND; break;
						case DAO_STRING :
							if( vmc->code >= DVM_LT ) vmc->code += DVM_LT_SS - DVM_LT;
							break;
						}
					}else if( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE
							&& bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE ){
						switch( type[opc]->tid ){
						case DAO_FLOAT : vmc->code += DVM_AND_FNN - DVM_AND; break;
						case DAO_DOUBLE : vmc->code += DVM_AND_DNN - DVM_AND; break;
						default : break;
						}
					}
				}
				break;
			}
		case DVM_IN :
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
			init[opc] = 1;
			ct = inumt;
			if( at->tid != DAO_ENUM && bt->tid == DAO_ENUM ) goto InvOper;
			if( NoCheckingType( bt ) ==0 ){
				if( bt->tid < DAO_STRING || bt->tid > DAO_PAIR ) goto InvOper;
			}
			if( type[opc] == NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			break;
		case DVM_NOT : case DVM_UNMS : case DVM_BITREV :
			{
				lastcomp = opc;
				/* force the result of DVM_NOT to be a number? */
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				if( csts[opc].cst ) goto ModifyConstant;
				if( type[opc] == NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, at );
				if( NoCheckingType( at ) ) continue;
				AssertTypeMatching( type[opa], type[opc], defs, 0 );
				ct = type[opc];
				/*
				   printf( "a: %s\n", type[opa]->name->mbs );
				   printf( "c: %s\n", type[opc]->name->mbs );
				 */
				if( typed_code ){
					if( at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE
							&& at->tid == type[opc]->tid ){
						vmc->code = 3 * ( vmc->code - DVM_NOT ) + DVM_NOT_I
							+ at->tid - DAO_INTEGER;
					}
				}
				break;
			}
		case DVM_BITAND : case DVM_BITOR : case DVM_BITXOR :
		case DVM_BITLFT : case DVM_BITRIT :
			{
				lastcomp = opc;
				at = type[opa];
				bt = type[opb];
				if( csts[opc].cst ) goto ModifyConstant;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				init[opc] = 1;
				ct = NULL;
				if( at->tid == DAO_LIST ){
					if( code != DVM_BITLFT && code != DVM_BITRIT ) goto InvOper;
					ct = at;
					at = at->nested->items.pType[0];
					AssertTypeMatching( bt, at, defs, 0 );
					if( at->tid == DAO_UDF && bt->tid != DAO_UDF ){
						at = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, & bt, at!=NULL );
						UpdateType( opa, at );
					}
				}else if( NoCheckingType( at ) || NoCheckingType( bt ) ){
					ct = any;
				}else if( at->tid == DAO_OBJECT || bt->tid == DAO_OBJECT
						|| at->tid == DAO_CDATA || bt->tid == DAO_CDATA
						|| at->tid == DAO_INTERFACE || bt->tid == DAO_INTERFACE ){
					ct = DaoCheckBinArith( self, vmc, at, bt, ct, hostClass, mbs );
					if( ct == NULL ) goto InvOper;
				}else if( at->tid == bt->tid ){
					ct = at;
					if( at->tid > DAO_DOUBLE && at->tid != DAO_LONG ) goto InvOper;
				}else if( ((at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE)
							|| at->tid == DAO_LONG )
						&& ((bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE)
							|| bt->tid == DAO_LONG) ){
					ct = at->tid > bt->tid ? at : bt;
				}else{
					goto InvOper;
				}
				if( type[opc] == NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				/* allow less strict typing: */
				if( ct->tid ==DAO_UDF || ct->tid == DAO_ANY ) continue;
				AssertTypeMatching( ct, type[opc], defs, 0 );
				ct = type[opc];
				if( typed_code ){
					if( at->tid == bt->tid && at->tid == ct->tid ){
						switch( at->tid ){
						case DAO_INTEGER :
							vmc->code += DVM_BITAND_III - DVM_BITAND; break;
						case DAO_FLOAT :
							vmc->code += DVM_BITAND_FFF - DVM_BITAND; break;
						case DAO_DOUBLE :
							vmc->code += DVM_BITAND_DDD - DVM_BITAND; break;
						}
					}else if( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE
							&& bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE ){
						if( code == DVM_BITLFT || code == DVM_BITRIT ){
							switch( type[opc]->tid ){
							case DAO_FLOAT : vmc->code += DVM_BITLFT_FNN - DVM_BITLFT; break;
							case DAO_DOUBLE : vmc->code += DVM_BITLFT_DNN - DVM_BITLFT; break;
							default : break;
							}
						}
					}
				}
				break;
			}
		case DVM_CHECK :
			{
				lastcomp = opc;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, inumt );
				AssertTypeMatching( inumt, type[opc], defs, 0 );
				break;
			}
		case DVM_NAMEVA :
			{
				lastcomp = opc;
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ct = DaoNameSpace_MakeType( ns, routConsts->data[opa].v.s->mbs,
						DAO_PAR_NAMED, (DaoBase*) type[opb], 0, 0 );
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_PAIR :
			{
				lastcomp = opc;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ts[0] = type[opa];
				ts[1] = type[opb];
				ct = DaoNameSpace_MakeType( ns, "pair", DAO_PAIR, NULL, ts, 2 );
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_TUPLE :
			{
				lastcomp = opc;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ct = DaoType_New( "tuple<", DAO_TUPLE, NULL, NULL );
				k = 1;
				for(j=0; j<opb; j++){
					at = type[opa+j];
					val = csts[opa+j];
					AnnotateItemExpression( vmc, k );
					AssertInitialized( opa+j, 0, 0, k );
					if( j >0 ) DString_AppendMBS( ct->name, "," );
					if( at->tid == DAO_PAR_NAMED ){
						if( ct->mapNames == NULL ) ct->mapNames = DMap_New(D_STRING,0);
						MAP_Insert( ct->mapNames, at->fname, j );
						DString_Append( ct->name, at->name );
					}else{
						DString_Append( ct->name, at->name );
					}
					DArray_Append( ct->nested, at );
				}
				DString_AppendMBS( ct->name, ">" );
				GC_IncRCs( ct->nested );
				bt = DaoNameSpace_FindType( ns, ct->name );
				if( bt ){
					DaoType_Delete( ct );
					ct = bt;
				}else{
					DaoType_CheckAttributes( ct );
					DaoNameSpace_AddType( ns, ct->name, ct );
				}
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_LIST : case DVM_ARRAY :
			{
				init[opc] = 1;
				lastcomp = opc;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				at = NULL;
				if( opb >= 11 ){
					at = type[opa];
					for(j=1; j<opb-10; j++){
						if( DaoType_MatchTo( type[opa+j], at, defs )==0 ){
							at = 0;
							break;
						}
						if( at->tid < type[opa+j]->tid ) at = type[opa+j];
					}
					if( code == DVM_ARRAY && at )
						if( at->tid ==0 || at->tid > DAO_COMPLEX ) at = fnumt;
				}else{
					if( opb == 2 ){
						int a = type[opa]->tid;
						int b = type[opa+1]->tid;
						at = type[opa];
						/* only allow {[ number : number ]} */
						if( ( a < DAO_INTEGER && a > DAO_DOUBLE ) ||
								( b < DAO_INTEGER && b > DAO_DOUBLE ) )
							goto ErrorTyping;
					}else if( opb == 3 ){
						at = type[opa];
						if( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE ){
							if( type[opa+1]->tid < DAO_INTEGER
									|| type[opa+1]->tid > DAO_DOUBLE ) goto ErrorTyping;
						}else if( at->tid ==DAO_COMPLEX ){
							if( type[opa+1]->tid < DAO_INTEGER
									|| type[opa+1]->tid > DAO_COMPLEX ) goto ErrorTyping;
						}else if( at->tid == DAO_STRING ){
							if( type[opa+1]->tid != DAO_STRING ) goto ErrorTyping;
						}else if( at->tid == DAO_ARRAY ){
							/* XXX */
						}else{
							goto ErrorTyping;
						}
						if( type[opa+2]->tid < DAO_INTEGER
								|| type[opa+2]->tid > DAO_DOUBLE ) goto ErrorTyping;
						if( vmc->code ==DVM_ARRAY && at->tid ==DAO_STRING ) goto ErrorTyping;
					}else{
						at = udf;
					}
				}
				if( vmc->code == DVM_LIST )
					ct = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, &at, at!=NULL );
				else if( at && at->tid >=DAO_INTEGER && at->tid <= DAO_COMPLEX )
					ct = arrtps[ at->tid ];
				else if( at && at->tid == DAO_ARRAY )
					ct = at;
				else
					ct = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY,NULL, &at, at!=NULL );
				/* else goto ErrorTyping; */
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_MAP :
		case DVM_HASH :
			{
				init[opc] = 1;
				lastcomp = opc;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				ts[0] = ts[1] = NULL;
				if( opb > 0 ){
					ts[0] = type[opa];
					ts[1] = type[opa+1];
					for(j=2; j<opb; j+=2){
						if( DaoType_MatchTo( type[opa+j], ts[0], defs ) ==0 ) ts[0] = NULL;
						if( DaoType_MatchTo( type[opa+j+1], ts[1], defs ) ==0 ) ts[1] = NULL;
						if( ts[0] ==NULL && ts[1] ==NULL ) break;
					}
				}
				if( ts[0] ==NULL ) ts[0] = DaoNameSpace_GetType( ns, & nil );
				if( ts[1] ==NULL ) ts[1] = DaoNameSpace_GetType( ns, & nil );
				ct = DaoNameSpace_MakeType( ns, "map", DAO_MAP, NULL, ts, 2 );
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_MATRIX :
			{
				init[opc] = 1;
				lastcomp = opc;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				k = ( (0xff00 & vmc->b )>>8 ) * ( 0xff & vmc->b );
				at = NULL;
				if( k >0 ){
					at = type[opa];
					for( j=0; j<k; j++){
						min = type[opa+j]->tid;
						if( min == 0 || min > DAO_COMPLEX ) goto ErrorTyping;
						if( type[opa+j]->tid > at->tid ) at = type[opa+j];
					}
				}
				ct = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY,NULL,&at, at!=NULL );
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_CURRY :
		case DVM_MCURRY :
			{
				lastcomp = opc;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				at = type[opa];
				ct = NULL;
				if( at->tid == DAO_TYPE ) at = at->nested->items.pType[0];
				if( at->tid == DAO_PAIR && at->nested->size >1 && code != DVM_MCURRY )
					at = at->nested->items.pType[1];
				if( at->tid == DAO_ROUTINE ){
					ct = DaoNameSpace_MakeType( ns, "curry", DAO_FUNCURRY, NULL, NULL, 0 );
				}else if( at->tid == DAO_CLASS ){
					if( csts[opa].t ==0 ) goto NotInit;
					klass = (DaoClass*) at->X.extra;
					str = klass->className;
					ct = klass->objType;
					/* XXX: check field names */
				}else if( at->tid == DAO_TUPLE ){
					ct = at;
					if( at->nested->size != opb ) goto NotMatch;
					for(j=1; j<=opb; j++){
						bt = type[opa+j];
						val = csts[opa+j];
						if( bt == NULL ) goto ErrorTyping;
						AssertInitialized( opa+j, 0, 0, vmc->middle - 1 );
						if( bt->tid == DAO_PAR_NAMED ){
							if( at->mapNames == NULL ) goto InvField;
							node = MAP_Find( at->mapNames, bt->fname );
							if( node == NULL || node->value.pInt != j-1 ) goto InvField;
							bt = bt->X.abtype;
						}
						tt = at->nested->items.pType[j-1];
						if( tt->tid == DAO_PAR_NAMED ) tt = tt->X.abtype;
						AssertTypeMatching( bt, tt, defs, 0 );
					}
				}else{
					ct = any;
				}
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				if( at->tid == DAO_ANY || at->tid == DAO_UDF ) break;
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_SWITCH :
			AssertInitialized( opa, 0, vmc->middle + 2, vmc->last-1 );
			j = 0;
			for(k=1; k<=opc; k++){
				DValue cc = routConsts->data[ vmcs[i+k]->a ];
				j += (cc.t == DAO_ENUM && cc.v.e->type->name->mbs[0] == '$');
				bt = DaoNameSpace_GetTypeV( ns, cc );
				if( at->name->mbs[0] == '$' && bt->name->mbs[0] == '$' ) continue;
				if( DaoType_MatchValue( at, cc, defs ) ==0 ){
					cid = i + k;
					vmc = vmcs[i + k];
					type_source = DaoNameSpace_GetTypeV( ns, cc ); 
					type_target = at;
					ec_specific = DTE_TYPE_NOT_MATCHING;
					goto ErrorTyping;
				}
			}
			if( csts[opa].t ){
				DValue sv = csts[opa];
				int jump = opb;
				for(k=1; k<=opc; k++){
					DValue cc = routConsts->data[ vmcs[i+k]->a ];
					if( DValue_Compare( sv, cc ) ==0 ){
						jump = vmcs[i+k]->b;
						break;
					}
				}
				vmc->code = DVM_GOTO;
				vmc->b = jump;
			}else if( at->tid == DAO_ENUM && at->name->mbs[0] != '$' && j == opc ){
				DMap *jumps = DMap_New(D_VALUE,0);
				DValue key = daoNullEnum;
				DNode *it, *find;
				DEnum em;
				int max=0, min=0;
				em.type = at;
				key.v.e = & em;
				for(k=1; k<=opc; k++){
					DValue cc = routConsts->data[ vmcs[i+k]->a ];
					if( DEnum_SetValue( & em, cc.v.e, NULL ) ==0 ){
						cid = i + k;
						vmc = vmcs[i + k];
						type_source = cc.v.e->type; 
						type_target = at;
						ec_specific = DTE_TYPE_NOT_MATCHING;
						DMap_Delete( jumps );
						goto ErrorTyping;
					}
					if( k ==1 ){
						max = min = em.value;
					}else{
						if( em.value > max ) max = em.value;
						if( em.value < min ) min = em.value;
					}
					MAP_Insert( jumps, & key, vmcs[i+k] );
				}
				if( at->flagtype == 0 && opc > 0.75*(max-min+1) ){
					for(it=DMap_First(at->mapNames);it;it=DMap_Next(at->mapNames,it)){
						em.value = it->value.pInt;
						find = DMap_Find( jumps, & key );
						if( find == NULL ) DMap_Insert( jumps, & key, NULL );
					}
					k = 1;
					for(it=DMap_First(jumps);it;it=DMap_Next(jumps,it)){
						if( it->value.pVoid ){
							vmcs[i+k] = (DaoVmCodeX*) it->value.pVoid;
							vmcs[i+k]->a = routConsts->size;
							k += 1;
						}else{
							vmc2.code = DVM_CASE;
							vmc2.a = routConsts->size;
							vmc2.b = opb;
							vmc2.c = DAO_CASE_TABLE;
							vmc2.first = vmc2.last = 0;
							vmc2.middle = 1;
							addCount[i+k] += 1;
							DArray_Append( addCode, & vmc2 );
						}
						DRoutine_AddConstValue( (DRoutine*)self, it->key.pValue[0] );
					}
					vmc->c = jumps->size;
					vmcs[i+1]->c = DAO_CASE_TABLE;
				}else{
					k = 1;
					for(it=DMap_First(jumps);it;it=DMap_Next(jumps,it)){
						vmcs[i+k] = (DaoVmCodeX*) it->value.pVoid;
						vmcs[i+k]->a = routConsts->size;
						DRoutine_AddConstValue( (DRoutine*)self, it->key.pValue[0] );
						k += 1;
					}
				}
				DMap_Delete( jumps );
			}else if( j ){
				vmcs[i + 1]->c = DAO_CASE_UNORDERED;
			}
			break;
		case DVM_CASE :
			break;
		case DVM_ITER :
			{
				lastcomp = opc;
				AssertInitialized( opa, 0, 0, vmc->last );
				init[opc] = 1;
				DString_SetMBS( mbs, "__for_iterator__" );
				ts[0] = dao_type_for_iterator;
				rout = NULL;
				switch( at->tid ){
				case DAO_CLASS :
				case DAO_OBJECT :
					klass = at->X.klass;
					tp = DaoClass_GetDataType( klass, mbs, & j, hostClass );
					if( j == DAO_ERROR_FIELD_NOTPERMIT ) goto NotPermit;
					if( j == DAO_ERROR_FIELD_NOTEXIST ) goto NotExist;
					j = DaoClass_GetDataIndex( klass, mbs, & k );
					if( k == DAO_OBJECT_VARIABLE && at->tid ==DAO_CLASS ) goto NeedInstVar;
					DaoClass_GetData( klass, mbs, & val, hostClass, NULL );
					if( val.t != DAO_ROUTINE && val.t != DAO_FUNCTION ) goto NotMatch;
					rout = (DRoutine*)val.v.routine;
					break;
				case DAO_INTERFACE :
					node = DMap_Find( at->X.inter->methods, mbs );
					if( node == NULL ) goto NotExist;
					rout = (DRoutine*)node->value.pBase;
					break;
				default :
					if( at->typer ) rout = (DRoutine*) DaoFindFunction( at->typer, mbs );
					break;
				}
				if( rout == NULL ) goto NotMatch;
				rout2 = rout;
				rout = MatchByParamType( rout, at, NULL, ts, 1, DVM_CALL, &min, &spec );
				if( rout == NULL ){
					DRoutine_ShowError( rout2, at, NULL, ts, 1, DVM_CALL, errors );
					goto NotMatch;
				}
				ct = dao_type_for_iterator;
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				break;
			}
		case DVM_TEST :
			{
				/* if( init[opa] ==0 ) goto NotInit;  allow null value for testing! */
				if( type[opa] ==NULL ) goto NotMatch;
				at = type[opa];
				if( typed_code ){
					if( at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE )
						vmc->code = DVM_TEST_I + at->tid - DAO_INTEGER;
				}
				break;
			}
		case DVM_MATH :
			lastcomp = opc;
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last - 1 );
			init[opc] = 1;
			ct = type[opb];
			if( ct->tid > DAO_COMPLEX && ct->tid != DAO_ANY ) goto InvParam;
			if( opa == DVM_MATH_ARG || opa == DVM_MATH_IMAG || opa == DVM_MATH_NORM || opa == DVM_MATH_REAL ){
				ct = dnumt;
			}else if( opa == DVM_MATH_RAND ){
				/* ct = type[opb]; return the same type as parameter */
			}else if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_FLOAT ){ /* XXX long type */
				ct = dnumt;
			}
			if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			break;
		case DVM_FUNCT :
			lastcomp = opc;
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last - 1 );
			init[opc] = 1;
			bt = type[ opb ];
			j = bt->tid;
			k = bt->tid != DAO_UDF && bt->tid != DAO_ANY;
			if( opa == DVM_FUNCT_APPLY ){
				if( k && j != DAO_ARRAY && j != DAO_LIST ) goto ErrorTyping;
			}else if( opa >= DVM_FUNCT_REPEAT && opa <= DVM_FUNCT_LIST ){
				if( k && j != DAO_INTEGER ) goto ErrorTyping;
			}else if( opa != DVM_FUNCT_UNFOLD ){
				if( k && j != DAO_ARRAY && j != DAO_LIST && j != DAO_TUPLE ) goto ErrorTyping;
			}
			k = j;
			if( bt->tid == DAO_TUPLE && bt->nested->size ==0 ) goto ErrorTyping;
			if( bt->tid == DAO_TUPLE && opa != DVM_FUNCT_SORT && opa != DVM_FUNCT_FOLD
					&& opa != DVM_FUNCT_UNFOLD ){
				k = bt->nested->items.pType[0]->tid;
				for( j=1; j<bt->nested->size; j++ )
					if( k != bt->nested->items.pType[j]->tid ) goto ErrorTyping;
				if( k != DAO_ARRAY && k != DAO_LIST ) goto ErrorTyping;
			}
			switch( opa ){
			case DVM_FUNCT_EACH :
			case DVM_FUNCT_REPEAT :
				ct = any;
				break;
			case DVM_FUNCT_MAP :
			case DVM_FUNCT_LIST :
			case DVM_FUNCT_ARRAY :
				bt = type[ vmcs[i-2]->c ];
				if( k == DAO_ARRAY || opa == DVM_FUNCT_ARRAY ){
					bt = DaoType_DeepItemType( bt );
					ct = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY, NULL, & bt, 1 );
				}else{
					ct = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, & bt, 1 );
				}
				break;
			case DVM_FUNCT_FOLD :
				ct = type[ vmcs[i-2]->c ];
				break;
			case DVM_FUNCT_UNFOLD :
				bt = type[ vmcs[i-2]->c ];
				ct = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, & bt, 1 );
				break;
			case DVM_FUNCT_SORT :
				if( bt->tid == DAO_TUPLE ){
					if( bt->nested->size ==0 || bt->nested->size >2 ) goto ErrorTyping;
					if( bt->nested->size >1 && bt->nested->items.pType[1]->tid != DAO_INTEGER )
						goto ErrorTyping;
					bt = bt->nested->items.pType[0];
				}
				ct = bt;
				break;
			case DVM_FUNCT_SELECT :
				if( bt->tid == DAO_TUPLE ){
					if( bt->nested->size ==0 ) goto ErrorTyping;
					at = DaoType_New( "tuple<", DAO_TUPLE, NULL, NULL );
					at->nested = DArray_New(0);
					for( j=0; j<bt->nested->size; j++ ){
						ct = bt->nested->items.pType[j]->nested->items.pType[0];
						if( j ) DString_AppendChar( at->name, ',' );
						DString_Append( at->name, ct->name );
						DArray_Append( at->nested, ct );
						GC_IncRC( ct );
					}
					DString_AppendChar( at->name, '>' ); /* functional XXX gc */
					DaoType_CheckAttributes( at );
					ct = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, & at, 1 );
				}else{
					ct = bt;
				}
				break;
			case DVM_FUNCT_APPLY :
				ct = bt;
				if( bt->tid == DAO_ARRAY || bt->tid == DAO_LIST ){
					if( bt->nested->size != 1 ) goto ErrorTyping;
					at = type[ vmcs[i-2]->c ];
					bt = bt->nested->items.pType[0];
					AssertTypeMatching( at, bt, defs, 0 );
				}
				break;
			case DVM_FUNCT_INDEX : ct = ilst; break;
			case DVM_FUNCT_COUNT : ct = inumt; break;
			case DVM_FUNCT_STRING : ct = strt; break;
			default : break;
			}
			if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			break;
		case DVM_CALL : case DVM_MCALL :
			{
				int ctchecked = 0;
				lastcomp = opc;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				j = type[opa+1] ? type[opa+1]->tid : 0;
				if( code == DVM_MCALL && j >= DAO_ARRAY && j != DAO_ANY ){
					DaoVmCodeX *p = vmcs[i+1];
					if( p->code == DVM_MOVE && p->a == opa+1 ){
						p->code = DVM_NOP;
						if( i+2 < N ){
							p = vmcs[i+2];
							if( p->code >= DVM_SETVL && p->code <= DVM_SETF && p->a == opa+1 )
								p->code = DVM_NOP;
						}
					}
				}
				at = type[opa];
				bt = ct = NULL;
				if( at->tid == DAO_PAIR ){
					if( at->nested->size > 1 ){
						bt = at->nested->items.pType[0];
						at = at->nested->items.pType[1];
					}
				}else if( code == DVM_CALL && self->tidHost == DAO_OBJECT ){
					bt = hostClass->objType;
				}
				/*
				   DaoVmCodeX_Print( *vmc, NULL );
				   printf( "call: %s\n", type[opa]->name->mbs );
				   if(bt) printf( "self: %s\n", bt->name->mbs );
				 */
				ct = type[opa];
				rout = NULL;
				if( at->tid == DAO_CLASS ){
					rout = (DRoutine*) at->X.klass->classRoutine;
					ct = at->X.klass->objType;
				}else if( at->tid == DAO_CDATA ){
					val = DaoFindValue( at->typer, at->name );
					if( val.t != DAO_FUNCTION ) goto ErrorTyping;
					rout = (DRoutine*) val.v.routine;
				}else if( csts[opa].t == DAO_ROUTINE || csts[opa].t == DAO_FUNCTION ){
					rout = (DRoutine*) csts[opa].v.p;
				}else if( at->tid == DAO_INITYPE || at->tid == DAO_FUNCURRY ){
					if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, any );
					AssertTypeMatching( any, type[opc], defs, 0 );
					break;
				}else if( at->tid == DAO_UDF || at->tid == DAO_ANY ){
					if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, any );
					break;
				}else if( at->tid == DAO_OBJECT ){
					rout = (DRoutine*) DaoClass_FindOperator( at->X.klass, "()", hostClass );
					if( rout == NULL ) goto ErrorTyping;
				}else if( at->tid != DAO_ROUTINE && at->tid != DAO_FUNCTION ){
					goto ErrorTyping;
				}
				pp = csts+opa+1;
				tp = type+opa+1;
				j = vmc->b & 0xff;
				if( j == DAO_CALLER_PARAM ){
					j = self->parCount;
					pp = csts;
					tp = type;
				}
				for(k=0; k<j; k++){
					if( pp[k].t == DAO_ROUTINE )
						DaoRoutine_Compile( pp[k].v.routine );
				}
				if( rout == NULL ){
					if( DRoutine_CheckType( at, ns, bt, pp, tp, j, code, 0, &min, &spec ) ==0 ){
						goto ErrorTyping;
					}
					if( at->name->mbs[0] == '@' ){
						ct = tp[0];
						if( pp[0].t == DAO_ROUTINE ) ct = pp[0].v.routine->routType;
						if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
						AssertTypeMatching( ct, type[opc], defs, 0 );
						break;
					}
					DRoutine_CheckType( at, ns, bt, pp, tp, j, code, 1, &min, &spec );
					ct = type[opa];
				}else{
					if( rout->type != DAO_ROUTINE && rout->type != DAO_FUNCTION )
						goto ErrorTyping;
					error = rout->routName;
					rout2 = rout;
					rout = MatchByParamType( rout, bt, pp, tp, j, code, &min, &spec );
					if( rout == NULL && at->tid == DAO_CLASS && (j ==0 || (j == 1 && code == DVM_MCALL ) ) ){
						/* default contstructor */
						rout = (DRoutine*) at->X.klass->classRoutine;
					}
					if( rout ==NULL ){
						DRoutine_ShowError( rout2, bt, pp, tp, j, code, errors );
						goto InvParam;
					}
					if( rout->routName->mbs[0] == '@' ){
						ct = tp[0];
						if( pp[0].t == DAO_ROUTINE ) ct = pp[0].v.routine->routType;
						if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
						AssertTypeMatching( ct, type[opc], defs, 0 );
						break;
					}
#if 0
					//XXX if( rout != rout2 ) type[opa] = rout->routType;
#endif
					/*
					   printf( "CALL: rout = %s %s %i %i %i %i\n", rout->routName->mbs, rout->routType->name->mbs, min, DAO_MT_INIT, DAO_MT_UDF, spec );
					 */

					if( ( spec && rout->parCount != DAO_MAX_PARAM )
							|| rout->routType->X.abtype ==NULL
							/*to infer returned type*/ ){
						if( rout->type == DAO_ROUTINE && ((DaoRoutine*)rout)->parser ){
							DaoRoutine_Compile( (DaoRoutine*) rout );
						}
						if( rout->type == DAO_ROUTINE && ((DaoRoutine*)rout)->original ) rout = (DRoutine*) ((DaoRoutine*)rout)->original;
						if( rout->type == DAO_ROUTINE && rout != (DRoutine*) self
								&& ((DaoRoutine*)rout)->vmCodes->size >0 && notide ){
							/* rout may has only been declared */
							rout = (DRoutine*) DaoRoutine_Copy2( (DaoRoutine*) rout );
							DRoutine_PassParamTypes( rout, bt, pp, tp, j, code );
							if( DaoRoutine_InferTypes( (DaoRoutine*) rout ) ==0 ) goto InvParam;
						}else if( rout->type == DAO_FUNCTION ){
#if 0
							rout = (DRoutine*) DaoFunction_Copy( (DaoFunction*) rout, 1 );
							DRoutine_PassParamTypes( rout, bt, pp, tp, j, code );
#endif
							ct = DRoutine_PassParamTypes2( rout, bt, pp, tp, j, code );
							ctchecked = 1;
						}
						rout = MatchByParamType( rout, bt, pp, tp, j, code, &min, &spec );
					}else if( bt && ( DString_FindChar( bt->name, '?', 0 ) !=MAXSIZE 
								|| DString_FindChar( bt->name, '@', 0 ) !=MAXSIZE ) ){
						DRoutine_PassParamTypes( rout, bt, pp, tp, j, code );
					}else if( rout->routType->X.abtype->tid == DAO_UDF ){
						if( rout->type == DAO_ROUTINE && ((DaoRoutine*)rout)->parser )
							DaoRoutine_Compile( (DaoRoutine*) rout );
					}
#if 0
					if( rout->type == DAO_ROUTINE && ((DaoRoutine*)rout)->vmCodes->size ==0 ){
						DaoParser *parser = ((DaoRoutine*)rout)->parser;
						if( parser == NULL || parser->tokens->size ==0 ) goto FunctionNotImplemented;
					}
#endif
					if( min >= DAO_MT_SUB ){
						csts[ opa ].t = rout->type;
						csts[ opa ].v.routine = (DaoRoutine*) rout;
					}
					if( at->tid != DAO_CLASS && ! ctchecked ) ct = rout->routType;
					/*
					   printf( "ct2 = %s\n", ct ? ct->name->mbs : "" );
					 */
				}
				if( at->tid != DAO_CLASS && ! ctchecked && ! (opb & DAO_CALL_COROUT) ) ct = ct->X.abtype;

				if( code == DVM_MCALL && tp[0]->tid == DAO_OBJECT 
						&& (tp[0]->X.klass->attribs & DAO_CLS_SYNCHRONOUS) ){
					ct = DaoNameSpace_MakeType( ns, "future", DAO_FUTURE, NULL, &ct, 1 );
				}
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				if( ct == NULL ) ct = DaoNameSpace_GetType( ns, & nil );
				/*
				   printf( "ct = %s\n", type[opc]->name->mbs );
				   printf( "%p  %i\n", type[opc], type[opc] ? type[opc]->tid : 1000 );
				 */
				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );

				if( ! typed_code ) break;
				if( ct && ct->tid == DAO_FUTURE ) break;
				at = type[opa];
				if( at->name->mbs[0] == '@' ) break;
				if( at->tid != DAO_ROUTINE && at->tid != DAO_FUNCTION ) break;
				if( rout ) at = rout->routType;
				if( rout && (rout->attribs & DAO_ROUT_VIRTUAL) ) break;
				if( rout && (rout->minimal ==1) ) break;
				if( vmc->b > 0xff || vmc->b == DAO_MAX_PARAM ) break;
				k = 0;
				if( code == DVM_CALL || code == DVM_CALL_TC ){
					if( vmc->b != at->nested->size ) break;
				}else{
					if( at->attrib & DAO_TYPE_SELF ){
						if( vmc->b != at->nested->size ) break;
					}else{
						if( vmc->b != at->nested->size+1 ) break;
						k = 1;
					}
				}
				for(j=k; j<vmc->b; j++){
					DaoType *t = tp[j];
					DaoType *p = at->nested->items.pType[j-k];
					if( t == NULL || p == NULL ) break;
					if( t->tid == 0 || t->tid == DAO_ANY ) break;
					if( p->tid == DAO_PAR_VALIST ) break;
					if( t->tid == DAO_PAR_NAMED || p->tid == DAO_PAR_NAMED ) break;
				}
				if( j < vmc->b ) break;
				if( rout ){
					UpdateType( opa, rout->routType );
					csts[opa].v.routine = (DaoRoutine*) rout;
				}
				vmc->code += DVM_CALL_TC - DVM_CALL;
				break;
			}
		case DVM_ROUTINE :
			{
				lastcomp = opc;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				init[opc] = 1;
				if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
				if( type[opa]->tid != DAO_ROUTINE ) goto ErrorTyping;
				/* close on types */
				at = type[opa];
				tparray = DArray_New(0);
#if 0
				XXX typing
					DArray_Resize( tparray, at->parCount, 0 );
				for(j=0; j<vmc->b; j+=2){
					int loc = csts[vmc->a+j+2].v.i;
					if( loc >= at->parCount ) break;
					tparray->items.pType[loc] = type[opa+j+1];
				}
#endif
				at = DaoNameSpace_MakeRoutType( ns, at, NULL, tparray->items.pType, NULL );
				DArray_Delete( tparray );

				if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, at );
				AssertTypeMatching( at, type[opc], defs, 0 );
				csts[opc] = csts[opa];
				break;
			}
		case DVM_CLASS :
			lastcomp = opc;
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			init[opc] = 1;
			ct = any;
			if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			break;
		case DVM_RETURN :
		case DVM_YIELD :
			{
				if( code == DVM_RETURN ) vmc->c = lastcomp;
				if( self->routType == NULL ) continue;
				if( opc == 1 && code == DVM_RETURN ) continue;
				ct = self->routType->X.abtype;
				/*
				   printf( "%i %s %s\n", self->routType->nested->size, self->routType->name->mbs, ct?ct->name->mbs:"" );
				 */
				if( vmc->b ==0 ){
					if( ct && ( ct->tid ==DAO_UDF || ct->tid ==DAO_ANY ) ) continue;
					if( ct && ! (self->attribs & DAO_ROUT_INITOR) ) goto ErrorTyping;
				}else{
					at = type[opa];
					if( at ==NULL ) goto ErrorTyping;
					if( vmc->b >1 )
						at = DaoNameSpace_MakeType( ns, "tuple", DAO_TUPLE,
								NULL, type+opa, vmc->b);

					if( retinf && ct->tid != DAO_UDF ){
						int mac = DaoType_MatchTo( at, ct, defs );
						int mca = DaoType_MatchTo( ct, at, defs );
						if( mac==0 && mca==0 ){
							goto ErrorTyping;
						}else if( mac==0 ){
							tt = DaoNameSpace_MakeRoutType( ns, self->routType, NULL, NULL, at );
							GC_ShiftRC( tt, self->routType );
							self->routType = tt;
						}
					}else if( ct && ct->tid != DAO_UDF ){
						if( notide && DaoType_MatchTo( at, ct, defs )== DAO_MT_SUB ){
							if( ct->tid == DAO_TUPLE && DaoType_MatchTo( ct, at, defs ) ){
								/* typedef tuple<x:float,y:float> Point2D
								 * routine Test()=>Point2D{ return (1.0,2.0); } */
								addCount[i] ++;
								vmc2.code = DVM_CAST;
								vmc2.a = opa;
								vmc2.b = 0;
								vmc2.c = self->locRegCount + addRegType->size -1;
								vmc->a = vmc2.c;
								DArray_Append( addCode, & vmc2 );
								DArray_Append( addRegType, ct );
							}else{
								goto ErrorTyping;
							}
						}
					}else{
						retinf = 1;
						tt = DaoNameSpace_MakeRoutType( ns, self->routType, NULL, NULL, at );
						GC_ShiftRC( tt, self->routType );
						self->routType = tt;
					}
				}
				if( code == DVM_YIELD ){
					init[opc] = 1;
					lastcomp = opc;
					tt = self->routType;
					if( tt->nested->size ==1 ){
						ct = tt->nested->items.pType[0];
						if( ct->tid == DAO_PAR_NAMED || ct->tid == DAO_PAR_DEFAULT )
							ct = ct->X.abtype;
					}else if( tt->nested->size ){
						ct = DaoNameSpace_MakeType(ns, "tuple", DAO_TUPLE, NULL,
								tt->nested->items.pType, tt->nested->size );
					}else{
						ct = udf;
					}
					if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
					AssertTypeMatching( ct, type[opc], defs, 0 );
					AssertTypeMatching( at, tt->X.abtype, defs, 0 );
				}
				break;
			}
#define USE_TYPED_OPCODE 1
#if USE_TYPED_OPCODE
		case DVM_CALL_CF :
		case DVM_CALL_CMF :
			lastcomp = opc;
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			init[opc] = 1;
			pp = csts+opa+1;
			tp = type+opa+1;
			j = vmc->b & 0xff;
			if( j == DAO_CALLER_PARAM ){
				j = self->parCount;
				pp = csts;
				tp = type;
			}
			rout2 = rout = (DRoutine*) csts[opa].v.routine;
			rout = MatchByParamType( rout, NULL, pp, tp, j, code, &min, &spec );
			if( rout ==NULL ){
				DRoutine_ShowError( rout2, NULL, pp, tp, j, code, errors );
				goto InvParam;
			}
			if( min != DAO_MT_EQ || (opb&0xff00) !=0 || rout->parCount == DAO_MAX_PARAM )
				goto ErrorTyping;
			if( rout->type != DAO_FUNCTION ) goto ErrorTyping;
			csts[opa].v.p = (DaoBase*) rout;
			csts[opa].t = rout->type;
			ct = rout->routType;
			ct = ct->X.abtype;
			if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
			if( ct == NULL ) ct = DaoNameSpace_GetType( ns, & nil );
			if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			break;
		case DVM_CALL_TC :
		case DVM_MCALL_TC :
			lastcomp = opc;
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			init[opc] = 1;
			at = type[opa];
			pp = csts+opa+1;
			tp = type+opa+1;
			j = vmc->b;
			if( j == DAO_CALLER_PARAM ){
				j = self->parCount;
				pp = csts;
				tp = type;
			}
			if( vmc->b > 0xff ) goto ErrorTyping;
			rout = (DRoutine*) csts[opa].v.routine;
			if( code == DVM_CALL_TC && self->tidHost == DAO_OBJECT ) bt = hostClass->objType;
			if( rout ){
				rout2 = rout;
				rout = MatchByParamType( rout, NULL, pp, tp, j, code, &min, &spec );
				if( rout ==NULL ){
					DRoutine_ShowError( rout2, NULL, pp, tp, j, code, errors );
					goto InvParam;
				}
				csts[opa].v.routine = (DaoRoutine*) rout;
				UpdateType( opa, rout->routType );
				ct = rout->routType->X.abtype;
			}else if( at->tid == DAO_ROUTINE ){
				j = DRoutine_CheckType( at, ns, NULL, pp, tp, vmc->b, code,1,&min, &spec );
				if( j <=0 ) goto ErrorTyping;
				ct = at->X.abtype;
			}else{
				goto ErrorTyping;
			}
			if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
			if( ct == NULL ) ct = DaoNameSpace_GetType( ns, & nil );
			if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
			AssertTypeMatching( ct, type[opc], defs, 0 );
			break;
		case DVM_SETVL_II : case DVM_SETVL_IF : case DVM_SETVL_ID :
		case DVM_SETVL_FI : case DVM_SETVL_FF : case DVM_SETVL_FD :
		case DVM_SETVL_DI : case DVM_SETVL_DF : case DVM_SETVL_DD :
			tp = typeVL[opc]->items.pType + opb;
			if( *tp==NULL || (*tp)->tid ==DAO_UDF ) *tp = type[opa];
			AssertTypeMatching( type[opa], *tp, defs, 0 );
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( tp[0], TT3, 0 );
			break;
		case DVM_SETVO_II : case DVM_SETVO_IF : case DVM_SETVO_ID :
		case DVM_SETVO_FI : case DVM_SETVO_FF : case DVM_SETVO_FD :
		case DVM_SETVO_DI : case DVM_SETVO_DF : case DVM_SETVO_DD :
			if( self->tidHost != DAO_OBJECT ) goto ErrorTyping;
			tp = typeVO[opc]->items.pType + opb;
			if( *tp==NULL || (*tp)->tid ==DAO_UDF ) *tp = type[opa];
			AssertTypeMatching( type[opa], *tp, defs, 0 );
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( tp[0], TT3, 0 );
			break;
		case DVM_SETVK_II : case DVM_SETVK_IF : case DVM_SETVK_ID :
		case DVM_SETVK_FI : case DVM_SETVK_FF : case DVM_SETVK_FD :
		case DVM_SETVK_DI : case DVM_SETVK_DF : case DVM_SETVK_DD :
			tp = typeVK->items.pArray[opc]->items.pType + opb;
			if( *tp==NULL || (*tp)->tid ==DAO_UDF ) *tp = type[opa];
			AssertTypeMatching( type[opa], *tp, defs, 0 );
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( tp[0], TT3, 0 );
			break;
		case DVM_SETVG_II : case DVM_SETVG_IF : case DVM_SETVG_ID :
		case DVM_SETVG_FI : case DVM_SETVG_FF : case DVM_SETVG_FD :
		case DVM_SETVG_DI : case DVM_SETVG_DF : case DVM_SETVG_DD :
			tp = typeVG->items.pArray[opc]->items.pType + opb;
			if( *tp==NULL || (*tp)->tid ==DAO_UDF ) *tp = type[opa];
			AssertTypeMatching( type[opa], *tp, defs, 0 );
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( tp[0], TT3, 0 );
			break;
		case DVM_MOVE_II : case DVM_NOT_I : case DVM_UNMS_I : case DVM_BITREV_I :
		case DVM_MOVE_FF : case DVM_NOT_F : case DVM_UNMS_F : case DVM_BITREV_F :
		case DVM_MOVE_DD : case DVM_NOT_D : case DVM_UNMS_D : case DVM_BITREV_D :
		case DVM_MOVE_IF : case DVM_MOVE_FI :
		case DVM_MOVE_ID : case DVM_MOVE_FD :
		case DVM_MOVE_DI : case DVM_MOVE_DF :
		case DVM_MOVE_CC : case DVM_UNMS_C :
		case DVM_MOVE_SS :
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, at );
			AssertTypeIdMatching( at, TT1, 0 );
			AssertTypeIdMatching( type[opc], TT3, 0 );
			init[opc] = 1;
			lastcomp = opc;
			break;
		case DVM_MOVE_PP :
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			if( at->tid < DAO_ARRAY || at->tid >= DAO_ANY ) goto NotMatch;
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, at );
			if( type[opc]->tid != at->tid ) goto NotMatch;
			init[opc] = 1;
			lastcomp = opc;
			if( opb ) csts[opc] = csts[opa];
			break;
		case DVM_ADD_III : case DVM_SUB_III : case DVM_MUL_III : case DVM_DIV_III :
		case DVM_MOD_III : case DVM_POW_III : case DVM_AND_III : case DVM_OR_III  :
		case DVM_LT_III  : case DVM_LE_III  : case DVM_EQ_III : case DVM_NE_III :
		case DVM_BITAND_III  : case DVM_BITOR_III  : case DVM_BITXOR_III :
		case DVM_BITLFT_III  : case DVM_BITRIT_III  :
		case DVM_ADD_FFF : case DVM_SUB_FFF : case DVM_MUL_FFF : case DVM_DIV_FFF :
		case DVM_MOD_FFF : case DVM_POW_FFF : case DVM_AND_FFF : case DVM_OR_FFF  :
		case DVM_LT_FFF  : case DVM_LE_FFF  : case DVM_EQ_FFF :
		case DVM_BITAND_FFF  : case DVM_BITOR_FFF  : case DVM_BITXOR_FFF :
		case DVM_BITLFT_FFF  : case DVM_BITRIT_FFF  :
		case DVM_ADD_DDD : case DVM_SUB_DDD : case DVM_MUL_DDD : case DVM_DIV_DDD :
		case DVM_MOD_DDD : case DVM_POW_DDD : case DVM_AND_DDD : case DVM_OR_DDD  :
		case DVM_LT_DDD  : case DVM_LE_DDD  : case DVM_EQ_DDD :
		case DVM_BITAND_DDD  : case DVM_BITOR_DDD  : case DVM_BITXOR_DDD :
		case DVM_BITLFT_DDD  : case DVM_BITRIT_DDD  :
		case DVM_ADD_CC : case DVM_SUB_CC : case DVM_MUL_CC : case DVM_DIV_CC :
		case DVM_ADD_SS : case DVM_LT_SS : case DVM_LE_SS :
		case DVM_EQ_SS : case DVM_NE_SS :
			{
				if( csts[opc].cst ) goto ModifyConstant;
				AssertInitialized( opa, 0, 0, vmc->middle - 1 );
				AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
				if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, simtps[TT3] );
				AssertTypeIdMatching( at, TT1, 0 );
				AssertTypeIdMatching( bt, TT2, 0 );
				AssertTypeIdMatching( type[opc], TT3, 0 );
				init[opc] = 1;
				lastcomp = opc;
				break;
			}
		case DVM_ADD_FNN : case DVM_SUB_FNN : case DVM_MUL_FNN : case DVM_DIV_FNN :
		case DVM_MOD_FNN : case DVM_POW_FNN : case DVM_AND_FNN : case DVM_OR_FNN  :
		case DVM_LT_FNN  : case DVM_LE_FNN  : case DVM_EQ_FNN :
		case DVM_BITLFT_FNN  : case DVM_BITRIT_FNN  :
		case DVM_ADD_DNN : case DVM_SUB_DNN : case DVM_MUL_DNN : case DVM_DIV_DNN :
		case DVM_MOD_DNN : case DVM_POW_DNN : case DVM_AND_DNN : case DVM_OR_DNN  :
		case DVM_LT_DNN  : case DVM_LE_DNN  : case DVM_EQ_DNN :
		case DVM_BITLFT_DNN  : case DVM_BITRIT_DNN  :
			if( csts[opc].cst ) goto ModifyConstant;
			AssertInitialized( opa, 0, 0, vmc->middle - 1 );
			AssertInitialized( opb, 0, vmc->middle + 1, vmc->last );
			if( at->tid ==0 || at->tid > DAO_DOUBLE ) goto NotMatch;
			if( bt->tid ==0 || bt->tid > DAO_DOUBLE ) goto NotMatch;
			if( ct ==NULL || ct->tid ==DAO_UDF ) UpdateType( opc, simtps[TT3] );
			if( type[opc]->tid != TT3 ) goto NotMatch;
			AssertTypeIdMatching( type[opc], TT3, 0 );
			init[opc] = 1;
			lastcomp = opc;
			break;
		case DVM_GETI_SI :
			{
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
				AssertTypeIdMatching( at, DAO_STRING, 0 );
				if( code == DVM_GETI_SI && bt->tid != DAO_INTEGER ) goto NotMatch;
				if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, inumt );
				AssertTypeIdMatching( type[opc], DAO_INTEGER, 0 );
				init[opc] = 1;
				lastcomp = opc;
				break;
			}
		case DVM_SETI_SII :
			{
				if( csts[opc].cst ) goto ModifyConstant;
				k = DaoTokens_FindLeftPair( self->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
				AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
				AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				AssertTypeIdMatching( at, DAO_INTEGER, 0 );
				AssertTypeIdMatching( bt, DAO_INTEGER, 0 );
				AssertTypeIdMatching( ct, DAO_STRING, 0 );
				break;
			}
		case DVM_GETI_LI :
			{
				lastcomp = opc;
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
				AssertTypeIdMatching( at, DAO_LIST, 0 );
				AssertTypeIdMatching( bt, DAO_INTEGER, 0 );
				at = type[opa]->nested->items.pType[0];
				if( at->tid < DAO_ARRAY || at->tid >= DAO_ANY ) goto NotMatch;
				if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, at );
				AssertTypeMatching( at, type[opc], defs, 0 );
				init[opc] = 1;
				break;
			}
		case DVM_GETI_LII : case DVM_GETI_LFI : case DVM_GETI_LDI :
		case DVM_GETI_AII : case DVM_GETI_AFI : case DVM_GETI_ADI :
		case DVM_GETI_LSI :
			{
				lastcomp = opc;
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
				if( at->tid != TT6 || at->nested->size ==0 ) goto NotMatch;
				at = at->nested->items.pType[0];
				if( at ==NULL || at->tid != TT1 ) goto NotMatch;
				if( bt ==NULL || bt->tid != TT2 ) goto NotMatch;
				if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, at );
				AssertTypeIdMatching( type[opc], TT3, 0 );
				init[opc] = 1;
				break;
			}
		case DVM_SETI_LI :
			{
				if( csts[opc].cst ) goto ModifyConstant;
				k = DaoTokens_FindLeftPair( self->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
				AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
				AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				AssertTypeIdMatching( bt, DAO_INTEGER, 0 );
				AssertTypeIdMatching( ct, DAO_LIST, 0 );
				if( at->tid < DAO_ARRAY || at->tid >= DAO_ANY ) goto NotMatch;
				ct = type[opc]->nested->items.pType[0];
				if( ct->tid < DAO_ARRAY || ct->tid >= DAO_ANY ) goto NotMatch;
				AssertTypeMatching( type[opa], ct, defs, 0 );
				break;
			}
		case DVM_SETI_LIII : case DVM_SETI_LIIF : case DVM_SETI_LIID :
		case DVM_SETI_LFII : case DVM_SETI_LFIF : case DVM_SETI_LFID :
		case DVM_SETI_LDII : case DVM_SETI_LDIF : case DVM_SETI_LDID :
		case DVM_SETI_AIII : case DVM_SETI_AIIF : case DVM_SETI_AIID :
		case DVM_SETI_AFII : case DVM_SETI_AFIF : case DVM_SETI_AFID :
		case DVM_SETI_ADII : case DVM_SETI_ADIF : case DVM_SETI_ADID :
		case DVM_SETI_LSIS :
			{
				if( csts[opc].cst ) goto ModifyConstant;
				k = DaoTokens_FindLeftPair( self->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
				AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
				AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				if( ct->tid != TT6 || bt->tid != TT2 || at->tid != TT1 ) goto NotMatch;
				if( ct->nested->size !=1 || ct->nested->items.pType[0]->tid != TT3 ) goto NotMatch;
				break;
			}
		case DVM_GETI_TI :
			{
				lastcomp = opc;
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
				if( at->tid != TT6 || bt->tid != TT2 ) goto NotMatch;
				if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, any );
				init[opc] = 1;
				break;
			}
		case DVM_SETI_TI :
			{
				if( csts[opc].cst ) goto ModifyConstant;
				k = DaoTokens_FindLeftPair( self->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
				AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
				AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				if( ct->tid != TT6 || bt->tid != TT2 ) goto NotMatch;
				break;
			}
		case DVM_SETF_T :
			{
				if( csts[opc].cst ) goto ModifyConstant;
				if( init[opa] ==0 || init[opc] ==0 ) goto NotInit;
				if( at ==NULL || ct ==NULL || ct->tid != TT6 ) goto NotMatch;
				if( opb >= ct->nested->size ) goto InvIndex;
				tt = ct->nested->items.pType[opb];
				if( tt->tid == DAO_PAR_NAMED ) tt = tt->X.abtype;
				AssertTypeMatching( at, tt, defs, 0 );
				break;
			}
		case DVM_GETF_T :
		case DVM_GETF_TI : case DVM_GETF_TF :
		case DVM_GETF_TD : case DVM_GETF_TS :
			{
				lastcomp = opc;
				if( init[opa] ==0 ) goto NotInit;
				if( at ==NULL || at->tid != TT6 ) goto NotMatch;
				if( opb >= at->nested->size ) goto InvIndex;
				ct = at->nested->items.pType[opb];
				if( ct->tid == DAO_PAR_NAMED ) ct = ct->X.abtype;
				if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				if( TT3 >0 ){
					if( ct ==NULL || ct->tid != TT3 ) goto NotMatch;
					if( type[opc]->tid != TT3 ) goto NotMatch;
				}else{
					AssertTypeMatching( ct, type[opc], defs, 0 );
				}
				init[opc] = 1;
				break;
			}
		case DVM_SETF_TII : case DVM_SETF_TIF : case DVM_SETF_TID :
		case DVM_SETF_TFI : case DVM_SETF_TFF : case DVM_SETF_TFD :
		case DVM_SETF_TDI : case DVM_SETF_TDF : case DVM_SETF_TDD :
		case DVM_SETF_TSS :
			{
				if( csts[opc].cst ) goto ModifyConstant;
				if( init[opa] ==0 || init[opc] ==0 ) goto NotInit;
				if( at ==NULL || ct ==NULL ) goto NotMatch;
				if( ct->tid != TT6 || at->tid != TT1 ) goto NotMatch;
				if( opb >= ct->nested->size ) goto InvIndex;
				tt = ct->nested->items.pType[opb];
				if( tt->tid == DAO_PAR_NAMED ) tt = tt->X.abtype;
				if( tt->tid != TT3 ) goto NotMatch;
				break;
			}
		case DVM_GETI_ACI :
			{
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last - 1 );
				AssertTypeMatching( type[opa], cart, defs, 0 );
				bt = type[opb];
				if( bt->tid != DAO_INTEGER ) goto NotMatch;
				if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, comt );
				if( type[opc]->tid != DAO_COMPLEX ) goto NotMatch;
				init[opc] = 1;
				lastcomp = opc;
				break;
			}
		case DVM_SETI_ACI :
			{
				if( csts[opc].cst ) goto ModifyConstant;
				k = DaoTokens_FindLeftPair( self->source, DTOK_LSB, DTOK_RSB, vmc->first + vmc->middle, 0 );
				AssertInitialized( opa, DTE_ITEM_WRONG_ACCESS, vmc->middle + 1, vmc->last );
				AssertInitialized( opb, DTE_ITEM_WRONG_ACCESS, k - vmc->first + 1, vmc->middle - 2 );
				AssertInitialized( opc, DTE_ITEM_WRONG_ACCESS, 0, vmc->middle - 1 );
				if( type[opa]->tid != DAO_COMPLEX ) goto NotMatch;
				if( type[opb]->tid != DAO_INTEGER ) goto NotMatch;
				AssertTypeMatching( type[opc], cart, defs, 0 );
				break;
			}
		case DVM_GETF_M :
			{
				lastcomp = opc;
				if( init[opa] ==0 || type[opa] == NULL ) goto NotInit;
				init[opc] = 1;
				at = type[opa];
				if( at->typer ==NULL ) goto NotMatch;
#if 0
				ct = at->typer->priv->methods[opb]->routType; /*XXX*/
#endif
				/* XXX csts[opc] = (DaoBase*) at->typer->priv->methods[opb]; */
				if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				AssertTypeMatching( type[opc], ct, defs, 0 );
				break;
			}
		case DVM_GETF_KC : case DVM_GETF_KG :
		case DVM_GETF_OC : case DVM_GETF_OG : case DVM_GETF_OV :
		case DVM_GETF_KCI : case DVM_GETF_KGI :
		case DVM_GETF_OCI : case DVM_GETF_OGI : case DVM_GETF_OVI :
		case DVM_GETF_KCF : case DVM_GETF_KGF :
		case DVM_GETF_OCF : case DVM_GETF_OGF : case DVM_GETF_OVF :
		case DVM_GETF_KCD : case DVM_GETF_KGD :
		case DVM_GETF_OCD : case DVM_GETF_OGD : case DVM_GETF_OVD :
			{
				lastcomp = opc;
				if( init[opa] ==0 || type[opa] == NULL ) goto NotInit;
				init[opc] = 1;
				at = type[opa];
				klass = (DaoClass*) at->X.extra;
				ct = NULL;
				if( at->tid != TT6 ) goto NotMatch;
				switch( TT4 ){
				case 'C' :
					ct = DaoNameSpace_GetTypeV( ns, klass->cstData->data[ opb ] );
					break;
				case 'G' : ct = klass->glbDataType->items.pType[ opb ]; break;
				case 'V' : ct = klass->objDataType->items.pType[ opb ]; break;
				default : goto NotMatch;
				}

				if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) UpdateType( opc, ct );
				AssertTypeMatching( ct, type[opc], defs, 0 );
				if( TT3 >0 && ct->tid != TT3 ) goto NotMatch;
				break;
			}
		case DVM_SETF_KG : case DVM_SETF_OG : case DVM_SETF_OV :
		case DVM_SETF_KGII : case DVM_SETF_OGII : case DVM_SETF_OVII :
		case DVM_SETF_KGIF : case DVM_SETF_OGIF : case DVM_SETF_OVIF :
		case DVM_SETF_KGID : case DVM_SETF_OGID : case DVM_SETF_OVID :
		case DVM_SETF_KGFI : case DVM_SETF_OGFI : case DVM_SETF_OVFI :
		case DVM_SETF_KGFF : case DVM_SETF_OGFF : case DVM_SETF_OVFF :
		case DVM_SETF_KGFD : case DVM_SETF_OGFD : case DVM_SETF_OVFD :
		case DVM_SETF_KGDI : case DVM_SETF_OGDI : case DVM_SETF_OVDI :
		case DVM_SETF_KGDF : case DVM_SETF_OGDF : case DVM_SETF_OVDF :
		case DVM_SETF_KGDD : case DVM_SETF_OGDD : case DVM_SETF_OVDD :
			{
				ct = (DaoType*) type[opc];
				if( ct == NULL ) goto ErrorTyping;
				if( csts[opc].cst ) goto ModifyConstant;
				if( init[opa] ==0 || init[opc] ==0 ) goto NotInit;
				if( type[opa] ==NULL || type[opc] ==NULL ) goto NotMatch;

				klass = (DaoClass*) ct->X.extra;
				if( ct->tid != TT6 ) goto NotMatch;
				bt = NULL;
				switch( TT4 ){
				case 'G' : bt = klass->glbDataType->items.pType[ opb ]; break;
				case 'V' : bt = klass->objDataType->items.pType[ opb ]; break;
				default : goto NotMatch;
				}
				AssertTypeMatching( at, bt, defs, 0 );
				if( TT1 > 0 && at->tid != TT1 ) goto NotMatch;
				if( TT3 > 0 && bt->tid != TT3 ) goto NotMatch;
			}
#endif
		default : break;
		}
	}
#if 0
	if( self->regType->size ){
		GC_DecRC( self->regType->items.pBase[self->regType->size-1] );
		DArray_PopBack( self->regType );
	}
#endif
	for(i=0; i<addRegType->size; i++){
		GC_IncRC( addRegType->items.pVoid[i] );
		DArray_Append( self->regType, addRegType->items.pVoid[i] );
	}
	self->locRegCount = self->regType->size;
	for(j=0; j<addCount[0]; j++){
		DArray_Append( vmCodeNew, addCode->items.pVmc[0] );
		DArray_PopFront( addCode );
	}
	DArray_Append( vmCodeNew, self->annotCodes->items.pVmc[0] );
	for(i=1; i<N; i++){
		int c;
		DaoVmCodeX *vmc = self->annotCodes->items.pVmc[i];
		c = vmc->code;
		k = addCount[i] - addCount[i-1];
		for( j=0; j<k; j ++ ){
			DArray_Append( vmCodeNew, addCode->items.pVmc[0] );
			DArray_PopFront( addCode );
		}
		if( c ==DVM_GOTO || c ==DVM_TEST || c ==DVM_SWITCH || c == DVM_CASE
				|| ( c >=DVM_TEST_I && c <=DVM_TEST_D ) ){
			if( vmc->b >0 ) vmc->b += addCount[vmc->b-1];
		}else if( c ==DVM_CRRE && vmc->c >0 ){
			vmc->c += addCount[vmc->c-1];
		}
		DArray_Append( vmCodeNew, self->annotCodes->items.pVmc[i] );
	}
	DArray_CleanupCodes( vmCodeNew );
	DaoVmcArray_Resize( self->vmCodes, vmCodeNew->size );
	for(i=0; i<vmCodeNew->size; i++){
		self->vmCodes->codes[i] = * (DaoVmCode*) vmCodeNew->items.pVmc[i];
	}
	DArray_Swap( self->annotCodes, vmCodeNew );
	DArray_Delete( errors );
	DArray_Delete( vmCodeNew );
	DArray_Delete( addCode );
	DArray_Append( self->regType, any );
	GC_IncRC( any );
	self->locRegCount ++;
	/*
	   DaoRoutine_PrintCode( self, self->nameSpace->vmSpace->stdStream );
	 */
#ifdef DAO_WITH_JIT
	if( daoConfig.jit && notide ) DaoRoutine_JitCompile( self );
#endif
	regConst->size = 0; /* its values should not be deleted by DValue_Clear(). */
	DVarray_Delete( regConst );
	DMap_Delete( defs );
	DArray_Delete( addRegType );
	DString_Delete( mbs );
	dao_free( init );
	dao_free( addCount );
	return 1;

NotMatch :     ec = DTE_TYPE_NOT_MATCHING;      goto ErrorTyping;
NotInit :      ec = DTE_TYPE_NOT_INITIALIZED;       goto ErrorTyping;
NotPermit :    ec = DTE_FIELD_NOT_PERMIT;     goto ErrorTyping;
NotExist :     ec = DTE_FIELD_NOT_EXIST;      goto ErrorTyping;
NeedInstVar :  ec = DTE_FIELD_OF_INSTANCE;   goto ErrorTyping;
WrongContainer : ec = DTE_TYPE_WRONG_CONTAINER; goto ErrorTyping;
InvIndex :     ec = DTE_INDEX_NOT_VALID;      goto ErrorTyping;
InvKey :       ec = DTE_KEY_NOT_VALID;        goto ErrorTyping;
InvField :     ec = DTE_KEY_NOT_VALID;        goto ErrorTyping;
InvOper :      ec = DTE_OPERATION_NOT_VALID;       goto ErrorTyping;
InvParam :     ec = DTE_PARAM_ERROR;      goto ErrorTyping;
ModifyConstant: ec = DTE_CONST_WRONG_MODIFYING; goto ErrorTyping;
#if 0
				//FunctionNotImplemented: ec = DTE_ROUT_NOT_IMPLEMENTED; goto ErrorTyping;
#endif

ErrorTyping:
				stdio = ns->vmSpace->stdStream;
				if( stdio ==NULL ) stdio = DaoStream_New();

				vmc = self->annotCodes->items.pVmc[cid];
				sprintf( char200, "%s:%i,%i,%i", getOpcodeName( vmc->code ), vmc->a, vmc->b, vmc->c );
				sprintf( char50, "  At line %i : ", vmc->line );

				DaoStream_WriteMBS( stdio, "[[ERROR]] in file \"" );
				DaoStream_WriteString( stdio, self->nameSpace->name );
				DaoStream_WriteMBS( stdio, "\":\n" );
				DaoStream_WriteMBS( stdio, char50 );
				DaoStream_WriteMBS( stdio, "Invalid virtual machine instruction --- \" " );
				DaoStream_WriteMBS( stdio, char200 );
				DaoStream_WriteMBS( stdio, " \";\n" );
#if 0
				if( ec == DTE_TYPE_NOT_INITIALIZED || ec == DTE_TYPE_WRONG_CONTAINER ){
					DaoStream_WriteMBS( stdio, char50 );
					if( ec == DTE_TYPE_WRONG_CONTAINER )
						DaoStream_WriteMBS( stdio, DaoTypingErrorString[DTE_ITEM_WRONG_ACCESS] );
					else
						DaoStream_WriteMBS( stdio, DaoTypingErrorString[DTE_OPERATION_NOT_VALID] );
					DaoStream_WriteMBS( stdio, " --- \" " );
					DaoTokens_AnnotateCode( self->source, *vmc, mbs, 32 );
					DaoStream_WriteString( stdio, mbs );
					DaoStream_WriteMBS( stdio, " \";\n" );
				}
				DaoStream_WriteMBS( stdio, char50 );
				DaoStream_WriteMBS( stdio, DaoTypingErrorString[ec] );
				DaoStream_WriteMBS( stdio, " --- \" " );
				if( ec == DTE_TYPE_NOT_INITIALIZED ){
					DaoVmCodeX vmc2 = *vmc;
					int t = vmcTyping[ vmc->code ][0];
					int a = 0, b = 0;
					if( t == OT_AOO || (t >= OT_AOC && t <= OT_EXP) ){
						if( init[vmc->a] ==0 || type[vmc->a] == NULL ) a = 1;
					}
					if( t == OT_ABC && (init[vmc->b] ==0 || type[vmc->b] == NULL) ) b = 1;
					if( a == 0 || b == 0 ){
						if( a ){
							vmc2.last = vmc2.middle = vmc2.middle - 1;
						}else if( b ){
							vmc2.first += vmc2.middle + 1;
							vmc2.last -= vmc2.middle + 1;
							vmc2.middle = 0;
							t = self->source->items.pToken[ vmc2.first - 1 ]->type;
							if( t == DTOK_LB || t == DTOK_LCB || t == DTOK_LSB ) vmc2.last -= 1;
						}
					}
					DaoTokens_AnnotateCode( self->source, vmc2, mbs, 32 );
				}else if( ec == DTE_TYPE_WRONG_CONTAINER ){
					DString_Assign( mbs, container->name );
				}else{
					DaoTokens_AnnotateCode( self->source, *vmc, mbs, 32 );
				}
				DaoStream_WriteString( stdio, mbs );
				DaoStream_WriteMBS( stdio, " \";\n" );
#else
				if( ec_general == 0 ) ec_general = ec;
				if( ec_general == 0 ) ec_general = DTE_OPERATION_NOT_VALID;
				if( ec_general ){
					DaoStream_WriteMBS( stdio, char50 );
					DaoStream_WriteMBS( stdio, DaoTypingErrorString[ec_general] );
					DaoStream_WriteMBS( stdio, " --- \" " );
					DaoTokens_AnnotateCode( self->source, *vmc, mbs, 32 );
					DaoStream_WriteString( stdio, mbs );
					DaoStream_WriteMBS( stdio, " \";\n" );
				}
				if( ec_specific ){
					DaoVmCodeX vmc2 = *vmc;
					DaoStream_WriteMBS( stdio, char50 );
					DaoStream_WriteMBS( stdio, DaoTypingErrorString[ec_specific] );
					DaoStream_WriteMBS( stdio, " --- \" " );
					if( ec_specific == DTE_TYPE_NOT_INITIALIZED ){
						vmc2.middle = 0;
						vmc2.first += annot_first;
						vmc2.last = annot_last > annot_first ? annot_last - annot_first : 0;
						DaoTokens_AnnotateCode( self->source, vmc2, mbs, 32 );
					}else if( ec_specific == DTE_TYPE_NOT_MATCHING ){
						DString_SetMBS( mbs, "'" );
						DString_AppendMBS( mbs, type_source ? type_source->name->mbs : "null" );
						DString_AppendMBS( mbs, "' for '" );
						if( type_target )
							DString_AppendMBS( mbs, type_target->name->mbs );
						else if( tid_target <= DAO_STREAM )
							DString_AppendMBS( mbs, coreTypeNames[tid_target] );
						DString_AppendChar( mbs, '\'' );
					}else{
						DaoTokens_AnnotateCode( self->source, *vmc, mbs, 32 );
					}
					DaoStream_WriteString( stdio, mbs );
					DaoStream_WriteMBS( stdio, " \";\n" );
				}
#endif
				DaoPrintCallError( errors, stdio );
				DArray_Delete( errors );
				if( stdio != ns->vmSpace->stdStream ) DaoStream_Delete( stdio );
				dao_free( init );
				dao_free( addCount );
				tmp = DMap_New(0,0);
				for( i=0; i<self->locRegCount; i++ )
					if( type[i] && type[i]->refCount ==0 ) DMap_Insert( tmp, type[i], 0 );
				node = DMap_First( tmp );
				for(; node !=NULL; node = DMap_Next(tmp, node) )
					DaoType_Delete( (DaoType*)node->key.pBase );
				DArray_Clear( self->regType );
				regConst->size = 0;
				DVarray_Delete( regConst );
				DArray_Delete( vmCodeNew );
				DArray_Delete( addCode );
				DArray_Delete( addRegType );
				DString_Delete( mbs );
				DMap_Delete( defs );
				DMap_Delete( tmp );
				return 0;
}

/* TODO register reallocation to reduce the usage of local variables for numbers */

void DaoRoutine_SetSource( DaoRoutine *self, DArray *tokens, DaoNameSpace *ns )
{
	DaoToken *tok, token = {0,0,0,0,0,NULL};
	DArray array = {{NULL},{NULL},D_TOKEN,0,0};
	DMap *nsTokens = self->nameSpace->tokens;
	DNode *node;
	DString *ts;
	int i;
	DArray_Append( self->nameSpace->sources, & array );
	self->source = (DArray*) DArray_Back( self->nameSpace->sources );
	for(i=0; i<tokens->size; i++){
		tok = tokens->items.pToken[i];
		node = DMap_Find( nsTokens, tok->string );
		if( node == NULL ) node = DMap_Insert( nsTokens, tok->string, NULL );
		token = *tok;
		token.string = NULL;
		DArray_Append( self->source, & token );
		tok = (DaoToken*) DArray_Back( self->source );
		tok->string = node->key.pString;
	}
}

static const char *const sep1 = "==========================================\n";
static const char *const sep2 =
"-------------------------------------------------------------------------\n";

void DaoRoutine_FormatCode( DaoRoutine *self, int i, DString *output )
{
	DaoVmCodeX **vmCodes = self->annotCodes->items.pVmc;
	DaoVmCodeX vmc;
	char buffer1[10];
	char buffer2[200];
	const char *fmt = daoRoutineCodeFormat;
	const char *name;

	DString_Clear( output );
	if( i < 0 || i >= self->annotCodes->size ) return;
	vmc = *vmCodes[i];
	name = getOpcodeName( vmc.code );
	sprintf( buffer1, "%5i :  ", i);
	if( self->source ) DaoTokens_AnnotateCode( self->source, vmc, output, 24 );
	sprintf( buffer2, fmt, name, vmc.a, vmc.b, vmc.c, vmc.line, output->mbs );
	DString_SetMBS( output, buffer1 );
	DString_AppendMBS( output, buffer2 );
}
void DaoRoutine_PrintCode( DaoRoutine *self, DaoStream *stream )
{
	DaoVmCodeX **vmCodes;
	DString *annot;
	const char *fmt = daoRoutineCodeFormat;
	char buffer[200];
	int j;

	if( self->type != DAO_ROUTINE || self->minimal == 1 ){
		for(j=0; j<self->routTable->size; j++)
			DaoRoutine_PrintCode( self->routTable->items.pRout[j], stream );
		return;
	}
	DaoRoutine_Compile( self );

	DaoStream_WriteMBS( stream, sep1 );
	DaoStream_WriteMBS( stream, "routine " );
	DaoStream_WriteString( stream, self->routName );
	DaoStream_WriteMBS( stream, "():\n" );
	DaoStream_WriteMBS( stream, "Number of register:\n" );
	DaoStream_WriteInt( stream, (double)self->locRegCount );
	DaoStream_WriteMBS( stream, "\n" );
	DaoStream_WriteMBS( stream, sep1 );
	DaoStream_WriteMBS( stream, "Virtual Machine Code:\n\n" );
	DaoStream_WriteMBS( stream, daoRoutineCodeHeader );

	DaoStream_WriteMBS( stream, sep2 );
	annot = DString_New(1);
	vmCodes = self->annotCodes->items.pVmc;
	for( j=0; j<self->annotCodes->size; j++){
		DaoRoutine_FormatCode( self, j, annot );
		DaoStream_WriteString( stream, annot );
	}
	DaoStream_WriteMBS( stream, sep2 );
	DString_Delete( annot );
}
void DaoFunction_Delete( DaoFunction *self )
{
	DRoutine_DeleteFields( (DRoutine*) self );
	DaoLateDeleter_Push( self );
}

DaoTypeBase funcTyper =
{
	"function", & baseCore, NULL, NULL, {0},
	(FuncPtrDel) DaoFunction_Delete, NULL
};
DaoFunction* DaoFunction_New()
{
	DaoFunction *self = (DaoFunction*) dao_malloc( sizeof(DaoFunction) );
	DaoBase_Init( self, DAO_FUNCTION );
	DRoutine_Init( (DRoutine*)self );
	DRoutine_AddOverLoad( (DRoutine*)self, (DRoutine*)self );
	self->pFunc = NULL;
	self->ffiData = NULL;
	return self;
}
DaoFunction* DaoFunction_Copy( DaoFunction *self, int overload )
{
	DaoFunction *copy = DaoFunction_New();
	DRoutine_CopyFields( (DRoutine*) copy, (DRoutine*) self );
	if( overload ) DRoutine_AddOverLoad( (DRoutine*) self, (DRoutine*) copy );
	copy->pFunc = self->pFunc;
	copy->ffiData = self->ffiData;
	return copy;
}
void DaoFunction_SimpleCall( DaoFunction *self, DaoContext *ctx, DValue *p[], int N )
{
	const int ndef = self->routType->nested->size;
	DaoType *tp, **types = self->routType->nested->items.pType;
	DValue buffer[ DAO_MAX_PARAM ];
	DValue *param[ DAO_MAX_PARAM ];
	ullong_t passed = 0;
	int i;
	memset( buffer, 0, DAO_MAX_PARAM * sizeof(DValue) );
	for(i=0; i<N; i++){
		tp = types[i];
		param[i] = & buffer[i];
		passed |= 1<<i;
		if( tp->tid == DAO_PAR_NAMED || tp->tid == DAO_PAR_DEFAULT ) tp = tp->X.abtype;
		if( i ==0 && (self->routType->attrib & DAO_TYPE_SELF) ){
			/* pass reference for self parameter */
			if( DaoType_MatchValue( tp, *p[i], NULL ) == DAO_MT_EQ ){
				param[i] = p[i];
				continue;
			}
		}
		if( DValue_Move( *p[i], param[i], tp ) ==0 ){
			printf( "%i  %s\n", p[i]->t, tp->name->mbs );
			DaoContext_RaiseException( ctx, DAO_ERROR_TYPE, "not matching" );
			return;
		}
	}
	for(i=N; i<ndef; i++) param[i] = & buffer[i];
	DRoutine_PassDefault( (DRoutine*)self, param, passed );
	ctx->thisFunction = self;
	self->pFunc( ctx, param, N );
	ctx->thisFunction = NULL;
	for(i=0; i<ndef; i++) DValue_Clear( buffer + i );
}
int DaoFunction_Call( DaoFunction *func, DaoCData *self, DValue *p[], int n )
{
	printf( "DaoFunction_Call()\n" );
	/* XXX */
	return 0;
}

void DaoFunCurry_Delete( DaoFunCurry *self )
{
	DValue_Clear( & self->callable );
	DValue_Clear( & self->selfobj );
	DVarray_Delete( self->params );
	dao_free( self );
}
DaoTypeBase curryTyper =
{
	"curry", & baseCore, NULL, NULL, {0},
	(FuncPtrDel) DaoFunCurry_Delete, NULL
};
DaoFunCurry* DaoFunCurry_New( DValue v, DValue o )
{
	DaoFunCurry *self = (DaoFunCurry*)dao_calloc( 1, sizeof(DaoFunCurry) );
	DaoBase_Init( self, DAO_FUNCURRY );
	DValue_Copy( & self->callable, v );
	DValue_Copy( & self->selfobj, o );
	self->params = DVarray_New();
	return self;
}
