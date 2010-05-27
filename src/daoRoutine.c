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

static const char* const daoScriptRaise[] =
{
  "raise", "Error", "(", "\"Compiling failed.\"", ")", ";",
  NULL
};
static const unsigned char daoScriptRaise2[] =
{
  DKEY_RAISE, DTOK_IDENTIFIER, DTOK_LB, DTOK_MBS, DTOK_RB, DTOK_SEMCO
};

static void DRoutine_Init( DRoutine *self )
{
  self->attribs = 0;
  self->distance = 0;
  self->parCount = 0;
  self->tidHost = 0;
  self->routHost = NULL;
  self->routName   = DString_New(1);
  self->docString  = DString_New(1);
  self->routConsts = DVarray_New();
  self->routOverLoad = DArray_New(0);
  self->nameSpace = NULL;
  self->routType = NULL;
  self->parTokens = NULL;
  DRoutine_AddOverLoad( self, self );
}
DRoutine* DRoutine_New()
{
  DRoutine *self = (DRoutine*) dao_malloc( sizeof(DRoutine) );
  DaoBase_Init( self, DAO_ROUTINE );
  DRoutine_Init( self );
  return self;
}
void DRoutine_CopyFields( DRoutine *self, DRoutine *from )
{
  int i;
  self->parCount = from->parCount;
  self->attribs = from->attribs;
  self->distance = from->distance;
  self->tidHost = from->tidHost;
  GC_ShiftRC( from->routHost, self->routHost );
  GC_ShiftRC( from->nameSpace, self->nameSpace );
  GC_ShiftRC( from->routType, self->routType );
  self->routHost = from->routHost;
  self->nameSpace = from->nameSpace;
  self->routType = from->routType;
  DVarray_Assign( self->routConsts, from->routConsts );
  DString_Assign( self->routName, from->routName );
  for(i=0; i<from->routConsts->size; i++) DValue_MarkConst( self->routConsts->data + i );
}
static void DRoutine_DeleteFields( DRoutine *self )
{
  GC_DecRCs( self->routOverLoad );
  GC_DecRC( self->routHost );
  GC_DecRC( self->routType );
  GC_DecRC( self->nameSpace );
  DString_Delete( self->routName );
  DVarray_Delete( self->routConsts );
  DArray_Delete( self->routOverLoad );
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
  int i;
  if( self == rout ){
    GC_IncRC( rout );
    DArray_Append( self->routOverLoad, rout );
    rout->firstRoutine = self;
  }else{
    for(i=0; i<rout->routOverLoad->size; i++){
      DRoutine *rt = (DRoutine*)rout->routOverLoad->items.pBase[i];
      if( rt->firstRoutine != self ){
        GC_IncRC( rt );
        DArray_Append( self->routOverLoad, rt );
        rt->firstRoutine = self;
      }
    }
  }
}
static int DRoutine_CheckType( DaoType *routType, DaoNameSpace *ns, DaoType *selftype,
    DValue *csts, DaoType *ts[], int np, int code, int def,
    int *min, int *norm, int *spec )
{
  int ndef = 0;
  int i, j, k, m, match = 1;
  int parpass[DAO_MAX_PARAM];
  int npar = np, size = routType->nested->size;
  int selfChecked = 0, selfMatch = 0;
  DValue cs = daoNullValue;
  DRoutine *rout;
  DNode *node;
  DMap *defs = DMap_New(0,0);
  DaoType  *abtp, **partypes = routType->nested->items.pAbtp;
  DaoType **tps = ts;

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
  *norm = 1;
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
      if( selfMatch >= DAO_MT_ANYUDF && selfMatch <= DAO_MT_UDF ) *spec = 1;
      if( *min > selfMatch ) *min = selfMatch;
    }
  }
  if( npar != ndef ) *norm = 0;
  if( npar == ndef && ndef == 0 ) goto FinishOK;
  if( npar > ndef && (size == 0 || partypes[size-1]->tid != DAO_PAR_VALIST ) ){
    goto FinishError;
  }

  for( j=selfChecked; j<ndef; j++) parpass[j] = 0;
  for( i=selfChecked; i<ndef; i++){
    j = i - selfChecked;
    if( partypes[j]->tid == DAO_PAR_VALIST ){
      parpass[j] = 1;
      break;
    }
    if( j < npar ){
      DaoType *tp = tps[j];
      cs = daoNullValue;
      if( csts && csts[j].t ==0 ) cs = csts[j];
      if( tp == NULL ) goto FinishError;
      k = i;
      if( tp->tid == DAO_PAR_NAMED ){
        *norm = 0;
        node = DMap_Find( routType->mapNames, tp->fname );
        if( node == NULL ) goto FinishError;
        k = node->value.pInt & MAPF_MASK;
        tp = tp->X.abtype;
      }
      if( k > ndef || tp ==NULL )  goto FinishError;
      abtp = routType->nested->items.pAbtp[k];
      if( abtp->tid == DAO_PAR_NAMED || abtp->tid == DAO_PAR_DEFAULT ) abtp = abtp->X.abtype;
      if( abtp->tid != DAO_PAR_VALIST ){
        if( abtp->tid ==DAO_ROUTINE && ( cs.t ==DAO_ROUTINE || cs.t ==DAO_FUNCTION ) ){
          rout = DRoutine_GetOverLoadByType( (DRoutine*)cs.v.p, abtp );
          tp = rout ? rout->routType : NULL;
        }
        parpass[k] = DaoType_MatchTo( tp, abtp, defs );

        /*
        printf( "%p %s %p %s\n", tp->X.extra, tp->name->mbs, abtp->X.extra, abtp->name->mbs );
        printf( "%i\n", parpass[k] );
        */

        /* less strict */
        if( tp && parpass[k] ==0 ){
          if( tp->tid == DAO_ANY && abtp->tid == DAO_ANY )
            parpass[k] = DAO_MT_ANY;
          else if( tp->tid == DAO_ANY || tp->tid == DAO_UDF )
            parpass[k] = DAO_MT_NEGLECT;
        }
        if( parpass[k] >= DAO_MT_ANYUDF && parpass[k] <= DAO_MT_UDF ) *spec = 1;
        if( parpass[k] < *min ) *min = parpass[k];
        if( parpass[k] == 0 ) goto FinishError;
        if( def ) tps[j] = DaoType_DefineTypes( tp, ns, defs );
      }else{
        for( m=k; m<npar; m++ ) parpass[m] = 1;
        break;
      }
    }else if( partypes[j]->tid == DAO_PAR_GROUP ){
      goto FinishError;
    }else if( parpass[j] == 0 && partypes[j]->tid != DAO_PAR_DEFAULT ){
      goto FinishError;
    }else{
      parpass[j] = 1;
    }
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
  for(i=0; i<self->routOverLoad->size; i++){
    rout = (DRoutine*) self->routOverLoad->items.pBase[i];
    if( rout->routType == type ) return rout;
  }
  for(i=0; i<self->routOverLoad->size; i++){
    rout = (DRoutine*) self->routOverLoad->items.pBase[i];
    j = DaoType_MatchTo( rout->routType, type, NULL );
    if( j && j >= max ){
      max = j;
      k = i;
    }
  }
  if( k >=0 ) return (DRoutine*) self->routOverLoad->items.pBase[k];
  return NULL;
}
DRoutine* DRoutine_GetOverLoadByParamType( DRoutine *self, DaoType *selftype,
    DValue *csts, DaoType *ts[], int np, int code, int *min, int *norm,
    int *spec, int *worst )
{
  int i, match;
  int best = -1;
  int max2 = 0;
  int min2 = 1E9;
  int min3=0, norm3=0, spec3=0;
  DRoutine *rout;

  *worst = -1;
  self = self->firstRoutine;
  i = 0;
  while( i<self->routOverLoad->size ){
    rout = (DRoutine*) self->routOverLoad->items.pBase[i];
    /*
    printf( "=====================================\n" );
    printf("ovld %i, %p %s : %s\n", i, rout, self->routName->mbs, rout->routType->name->mbs);
    */
    match = DRoutine_CheckType( rout->routType, self->nameSpace, selftype, csts, ts, np, code, 0, & min3, & norm3, & spec3 );
    if( match >= max2 ){
      max2 = match;
      best = i;
      *min = min3;
      *norm = norm3;
      *spec = spec3;
    }
    if( match < min2 && match > 0 ){
      min2 = match;
      *worst = i;
    }
    i ++;
  }
  /*
  printf("%s : best = %i, spec = %i\n", self->routName->mbs, best, *spec);
  */
  if( best >=0 ){
    rout = (DRoutine*) self->routOverLoad->items.pBase[best];
    return rout;
  }
  return NULL;
}
void DRoutine_PassParamTypes( DRoutine *self, DaoType *selftype,
    DValue *csts, DaoType *ts[], int np, int code )
{
  int j, k;
  int npar = np;
  int ndef = self->parCount;
  int loop = ndef > npar ? npar : ndef;
  int selfChecked = 0;
  DaoType **parType = self->routType->nested->items.pAbtp;
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
    abtp = self->routType->nested->items.pAbtp[0]->X.abtype;
    if( DaoType_MatchTo( selftype, abtp, defs ) ){
      selfChecked = 1;
      DaoType_RenewTypes( selftype, self->nameSpace, defs );
    }
  }
  for( j=selfChecked; j<loop; j++){
    if( parType[j]->tid == DAO_PAR_VALIST ) break;
    tp = tps[j];
    cs = daoNullValue;
    if( csts && csts[j].t ==0 ) cs = csts[j];
    if( tp == NULL ) break;
    k = j;
    if( tp->tid == DAO_PAR_NAMED ){
      node = DMap_Find( mapNames, tp->fname );
      if( node == NULL ) break;
      k = node->value.pInt & MAPF_MASK;
      tp = tp->X.abtype;
    }
    if( k > ndef || tp ==NULL || parType[k] ==NULL )  break;
    abtp = parType[k];
    if( abtp->tid == DAO_PAR_NAMED || abtp->tid == DAO_PAR_DEFAULT ) abtp = abtp->X.abtype;
    DaoType_MatchTo( tp, abtp, defs );
  }
  abtp = DaoType_DefineTypes( self->routType, self->nameSpace, defs );
  GC_ShiftRC( abtp, self->routType );
  self->routType = abtp;
  /*
  printf( "tps1: %p %s\n", self->routType, self->routType->name->mbs );
  for(node=DMap_First(defs);node;node=DMap_Next(defs,node)){
    printf( "%i  %i\n", node->key.pAbtp->tid, node->value.pAbtp->tid );
    printf( "%s  %s\n", node->key.pAbtp->name->mbs, node->value.pAbtp->name->mbs );
  }
  */
  for( j=0; j<npar; j++){
    tps[j] = DaoType_DefineTypes( tps[j], self->nameSpace, defs );
  }
  if( selftype && ( self->routType->attrib & DAO_TYPE_SELF) ){
    /* class DaoClass : CppClass{ cppmethod(); } */
    abtp = self->routType->nested->items.pAbtp[0];
    if( DaoType_MatchTo( selftype, abtp, defs ) )
      DaoType_RenewTypes( selftype, self->nameSpace, defs );
  }
  DMap_Delete( defs );
}
DaoType* DRoutine_PassParamTypes2( DRoutine *self, DaoType *selftype,
    DValue *csts, DaoType *ts[], int np, int code )
{
  int j, k;
  int npar = np;
  int ndef = self->parCount;
  int loop = ndef > npar ? npar : ndef;
  int selfChecked = 0;
  DaoType **parType = self->routType->nested->items.pAbtp;
  DaoType **tps = ts;
  DaoType  *abtp, *tp, *abtp2;
  DNode *node;
  DMap *defs;
  DMap *mapNames = self->routType->mapNames;
  DValue cs;

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
    abtp = self->routType->nested->items.pAbtp[0]->X.abtype;
    if( DaoType_MatchTo( selftype, abtp, defs ) ){
      selfChecked = 1;
      DaoType_RenewTypes( selftype, self->nameSpace, defs );
    }
  }
  for( j=selfChecked; j<loop; j++){
    if( parType[j]->tid == DAO_PAR_VALIST ) break;
    tp = tps[j];
    cs = daoNullValue;
    if( csts && csts[j].t ==0 ) cs = csts[j];
    if( tp == NULL ) break;
    k = j;
    if( tp->tid == DAO_PAR_NAMED ){
      node = DMap_Find( mapNames, tp->fname );
      if( node == NULL ) break;
      k = node->value.pInt & MAPF_MASK;
      tp = tp->X.abtype;
    }
    if( k > ndef || tp ==NULL || parType[k] ==NULL )  break;
    abtp = parType[k];
    if( abtp->tid == DAO_PAR_NAMED || abtp->tid == DAO_PAR_DEFAULT ) abtp = abtp->X.abtype;
    DaoType_MatchTo( tp, abtp, defs );
  }
  abtp2 = DaoType_DefineTypes( self->routType, self->nameSpace, defs );
  abtp2 = abtp2->X.abtype;
  for( j=0; j<npar; j++){
    tps[j] = DaoType_DefineTypes( tps[j], self->nameSpace, defs );
  }
  if( selftype && ( self->routType->attrib & DAO_TYPE_SELF) ){
    /* class DaoClass : CppClass{ cppmethod(); } */
    abtp = self->routType->nested->items.pAbtp[0];
    if( DaoType_MatchTo( selftype, abtp, defs ) )
      DaoType_RenewTypes( selftype, self->nameSpace, defs );
  }
  DMap_Delete( defs );
  return abtp2;
}
static DRoutine* DRoutine_GetOverLoadExt(
    DRoutine *self, DaoVmProcess *vmp, DaoClass *filter, DValue *obj, DValue *p[], int n, int code )
{
  float match, max = 0;
  int i, j, m, ip, it, sum;
  int best = -1;
  int dist = 0;
  int mcall = (code == DVM_MCALL  || code == DVM_MCALL_TC);
  int parpass[DAO_MAX_PARAM];
  DaoBase *filter2 = (DaoBase*) filter;
  DArray *signature = vmp->signature;
  DNode *node;
  DMap *defs = NULL;
  void *pvoid;
  void *pvoid2[2];
  int nocache = 0;

  self = self->firstRoutine;
  if( self->routOverLoad->size == 1 ){
      /* self might be a dummy routine created in deriving class methods */
    self = (DRoutine*) self->routOverLoad->items.pBase[0];
    return self;
  }
#if 0
  if( strcmp( self->routName->mbs, "expand" ) ==0 ){
    for(i=0; i<n; i++) printf( "%i  %i\n", i, p[i]->t );
  }
#endif

#define TYPING_CACHE 1
#if TYPING_CACHE
  if( dao_late_deleter.safe ==0 || dao_late_deleter.version != vmp->version ){
    if( vmp->callsigs->size ) DMap_Clear( vmp->callsigs );
    if( vmp->matching->size ) DMap_Clear( vmp->matching );
  }
  if( dao_late_deleter.safe ){
    signature->size = 0;
    if( obj ){
      pvoid = DValue_GetTypeID( *obj );
      DArray_Append( signature, pvoid );
    }
    if( mcall ) DArray_Append( signature, (void*)1 );
    DArray_Append( signature, (void*)2 );
    for(i=0; i<n; i++){
      pvoid = DValue_GetTypeID( *p[i] );
      DArray_Append( signature, pvoid );
      if( pvoid == NULL ) nocache = 1;
    }
    node = DMap_Find( vmp->callsigs, signature );
    if( node == NULL ){
      DMap_Insert( vmp->callsigs, signature, NULL );
      node = DMap_Find( vmp->callsigs, signature );
    }
    pvoid2[0] = node->key.pVoid;
  }else{
    nocache = 1;
  }
#endif
  i = 0;
  while( i<self->routOverLoad->size ){
    DRoutine *rout = (DRoutine*) self->routOverLoad->items.pBase[i];
    DaoType **parType = rout->routType->nested->items.pAbtp;
    DaoType  *abtp;
    DMap *mapNames = rout->routType->mapNames;
    DValue **dpar = p;
    int ndef = rout->parCount;
    int npar = n;
    int selfChecked = 0, selfMatch = 0;
    if( filter && obj && obj->t == DAO_OBJECT ){
      if( DaoClass_ChildOf( obj->v.object->myClass, filter2 ) ==0 ) goto NextRoutine;
    }
#if TYPING_CACHE
    if( nocache ==0 ){
      pvoid2[1] = rout;
      node = DMap_Find( vmp->matching, pvoid2 );
      if( node ){
        sum = node->value.pInt;
        match = sum / (ndef + 1.0);
        if( sum && (match > max || (match == max && rout->distance <= dist)) ){
          max = match;
          best = i;
          dist = rout->distance;
        }
        goto NextRoutine;
      }
    }
#endif
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
      if( selfMatch ) selfChecked = 1;
    }
    /*
    if( strcmp( rout->routName->mbs, "expand" ) ==0 )
    printf( "%i, parlist = %s; npar = %i; ndef = %i, %i\n", i, rout->routType->name->mbs, npar, ndef, best );
    */
    sum = 0;
    if( (npar | ndef) ==0 ){
      if( max <1 || (max ==1 && rout->distance <= dist) ){
        max = 1;
        best = i;
        dist = rout->distance;
        if( nocache ==0 ) MAP_Insert( vmp->matching, pvoid2, max );
      }
      sum = 1;
      goto CacheThenNext;
    }
    if( npar > ndef ) goto CacheThenNext;
    for( j=selfChecked; j<ndef; j++) parpass[j] = 0;
    for( ip=selfChecked; ip<ndef; ip++){
      it = ip;
      if( it < npar ){
        DValue val = *dpar[it-selfChecked];
        if( val.t == DAO_PAR_NAMED ){
          DaoPair *pair = val.v.pair;
          val = pair->second;
          node = DMap_Find( mapNames, pair->first.v.s );
          if( node == NULL ) goto CacheThenNext;
          it = node->value.pInt & MAPF_MASK;
        }
        if( it > ndef )  goto CacheThenNext;
        abtp = parType[it];
        if( abtp->tid != DAO_PAR_VALIST ){
          if( abtp->tid != DAO_PAR_GROUP ) abtp = abtp->X.abtype; /* must be named */
          parpass[it] = DaoType_MatchValue( abtp, val, defs );
          if( parpass[it] ==0 ) goto CacheThenNext;
          if( abtp->tid == DAO_PAR_GROUP ) ip += abtp->nested->size -1;
        }else{
          for( m=it; m<npar; m++ ) parpass[m] = 1;
          break;
        }
      }else if( parType[it]->tid == DAO_PAR_VALIST ){
        break;
      }else if( parpass[it] == 0 && parType[it]->tid != DAO_PAR_DEFAULT ){
        goto CacheThenNext;
      }else{
        parpass[it] = 1;
      }
    }
    sum = selfMatch;
    for( j=selfChecked; j<ndef; j++ ) sum += parpass[j];
    if( npar ==0 && ( ndef==0 || parType[0]->tid == DAO_PAR_VALIST ) ) sum = 1;
    match = sum / (ndef + 1.0);
    if( match > max || (match == max && rout->distance <= dist) ){
      max = match;
      best = i;
      dist = rout->distance;
    }
#if TYPING_CACHE
CacheThenNext:
    if( nocache ==0 ) MAP_Insert( vmp->matching, pvoid2, sum );
#endif
NextRoutine:
    i ++;
  }
  /*
    if( strcmp( self->routName->mbs, "expand" ) ==0 )
  printf("%s : best = %i\n", self->routName->mbs, best);
  */
  if( defs ) DMap_Delete( defs );
  if( best >=0 ) return (DRoutine*) self->routOverLoad->items.pBase[best];
  return NULL;
}
DRoutine* DRoutine_GetOverLoad( DRoutine *self, DaoVmProcess *vmp, DValue *obj, DValue *p[], int n, int code )
{
  DValue value = daoNullValue;
  DaoClass *klass, *filter;
  DaoRoutine *rout;
  DRoutine *rout2;
  self = DRoutine_GetOverLoadExt( self, vmp, NULL, obj, p, n, code );
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
    rout2 = DRoutine_GetOverLoadExt( rout2, vmp, filter, obj, p, n, code );
    if( rout2 ) return (DRoutine*) rout2;
  }
  return self;
}
extern int DValue_Pass( DValue from, DValue *to, DaoType *tp );
static int DRoutine_PassParam( DRoutine *routine, DValue *recv[],
    DValue *val, DValue *base, int ip, int it, ullong_t *passed )
{
  DMap *mapNames = routine->routType->mapNames;
  DaoType *tp = routine->routType->nested->items.pAbtp[it]->X.abtype;
  int constParam = routine->type == DAO_ROUTINE ? ((DaoRoutine*)routine)->constParam : 0;
  int norc = 0; /* routine->type == DAO_FUNCTION ? 1 : 0; ??? */

  /* it: index of the parameter to be passed and the type to be checked;
   * ip: index of the place to hold the passed parameter; */
  if( val->t == DAO_PAR_NAMED ){
    DaoPair *pair = val->v.pair;
    DNode *node = DMap_Find( mapNames, pair->first.v.s );
    val = & pair->second;
    if( node == NULL ) return 0;
    ip = node->value.pInt >> MAPF_OFFSET;
    it = node->value.pInt & MAPF_MASK;
  }else if( base && val != base + it && val->t < DAO_ARRAY && !(constParam & (1<<ip)) ){
    if( DaoType_MatchValue( tp, *val, NULL ) == DAO_MT_EQ ){
      *passed |= 1<<it;
      recv[ip] = val;
      return 1;
    }
  }
  recv = recv + ip;
  tp = routine->routType->nested->items.pAbtp[it]->X.abtype; /* alway named */
#if 0
  //if( tp->tid == DAO_PAR_NAMED || tp->tid == DAO_PAR_DEFAULT ) tp = tp->X.abtype;
#endif

  if( ip >= routine->parCount ) return 0;
  if( norc ){
    if( DValue_Pass( *val, *recv, tp ) ==0 ) return 0;
  }else{
    if( DValue_Move( *val, *recv, tp ) ==0 ) return 0;
  }
  if( constParam & (1<<ip) ) recv[0]->cst = 1;
  *passed |= 1<<it;
  return 1;
}
static int DRoutine_PassDefault( DRoutine *routine, DValue *recv[], ullong_t passed )
{
  DValue val;
  const int ndef = routine->routType->nested->size;
  int i, ip;
  for(i=0, ip=0; i<ndef; i++, ip++){
    DaoType *tp = routine->routType->nested->items.pAbtp[i];
    if( tp->tid == DAO_PAR_VALIST ) return 1;
    if( passed & (1<<i) ){
      if( tp->tid == DAO_PAR_GROUP ) ip += tp->nested->size-1;
      continue;
    }
    tp = tp->X.abtype; /* alway named parameter, for non-grouped and omitted */
#if 0
    //if( tp->tid == DAO_PAR_NAMED || tp->tid == DAO_PAR_DEFAULT ) tp = tp->X.abtype;
#endif

    /* e.g.: spawn( pid :STRING, src :STRING, timeout=-1, ... ) XXX */
    val = routine->routConsts->data[ip];
    if( val.t ==0 && val.ndef ) return 0; /* no default */
    if( val.t ==0 ){
      DValue_Copy( recv[ip], val ); /* null value as default (e.g. in DaoMap methods) */
      passed |= 1<<i;
      continue;
    }
    if( DValue_Move( val, recv[ip], tp ) ==0 ) return 0;
    passed |= 1<<i;
  }
  return 1;
}
int DRoutine_PassParams( DRoutine *routine, DValue *obj, DValue *recv[], DValue *p[], DValue *base, int np, int code )
{
  ullong_t passed = 0;
  int j, ip, it = 0, npar = np;
  int ndef = routine->parCount;
  int selfChecked = 0;
  int norc = 0; /* routine->type == DAO_FUNCTION ? 1 : 0; ??? */
  DaoType *routype = routine->routType;
  DaoType *tp, **tps, **parType = routype->nested->items.pAbtp;
  DValue *vs;
#if 0
  int i;
  for(i=0; i<npar; i++){
    tp = DaoNameSpace_GetTypeV( routine->nameSpace, *p[i] );
    printf( "%i  %s\n", i, tp->name->mbs );
  }
#endif

  if( (code == DVM_MCALL || code == DVM_MCALL_TC)
    && ! (routype->attrib & DAO_TYPE_SELF) ){
    npar --;
    p ++;
    if(base) base ++;
  }else if( obj && obj->t && (routype->attrib & DAO_TYPE_SELF) ){
    /* class DaoClass : CppClass{ cppmethod(); } */
    tp = parType[0]->X.abtype;
    if( obj->t < DAO_ARRAY ){
      if( tp == NULL || DaoType_MatchValue( tp, *obj, NULL ) == DAO_MT_EQ ){
        recv[0] = obj;
        selfChecked = 1;
        passed = 1;
      }
    }else{
      DValue o = *obj;
      if( o.t == DAO_OBJECT && (tp->tid ==DAO_OBJECT || tp->tid ==DAO_CDATA) ){
        o.v.p = DaoObject_MapThisObject( o.v.object, tp );
        if( obj->v.p ) o.t = o.v.p->type;
      }
      if( norc ){
        if( DValue_Pass( o, recv[0], tp ) ){
          selfChecked = 1;
          passed = 1;
        }
      }else{
        if( DValue_Move( o, recv[0], tp ) ){
          selfChecked = 1;
          passed = 1;
        }
      }
      recv[0]->cst = obj->cst;
    }
  }
  /*
  printf( "%s, rout = %s; ndef = %i; npar = %i, %i\n", routine->routName->mbs, routine->routType->name->mbs, ndef, npar, selfChecked );
  */
  if( npar > ndef ) return 0;
  if( (npar|ndef) ==0 ) return 1;
  /* it: index of the parameter to be passed and the type to be checked;
   * ip: index of the place to hold the passed parameter; */
  for( ip=selfChecked; ip<ndef; ip++, it++){
    if( parType[it+selfChecked]->tid == DAO_PAR_VALIST ){
      for( j=it; j<npar; j++ ){
        DValue_Move( *p[j], recv[j], NULL );
        passed |= 1<<j;
      }
      return DRoutine_PassDefault( routine, recv, passed );
    }else if( parType[it+selfChecked]->tid == DAO_PAR_GROUP && p[it]->t == DAO_TUPLE ){
      tp = parType[it+selfChecked];
      vs = p[it]->v.tuple->items->data;
      tps = tp->nested->items.pAbtp;
      if( tp->nested->size != p[it]->v.tuple->items->size ) return 0;
      for( j=0; j<tp->nested->size; j++, ip++ ){
        if( DValue_Move( vs[j], recv[ip], tps[j]->X.abtype ) ==0) return 0;
      }
      passed |= 1<<it;
      ip --;
      continue;
    }
    if( it >= npar ) break;
    if( it < npar && ! DRoutine_PassParam( routine, recv, p[it], base, ip, it+selfChecked, &passed ) ) return 0;
  }
  return DRoutine_PassDefault( routine, recv, passed );
}
extern int DValue_Pass( DValue from, DValue *to, DaoType *tp );
int DRoutine_FastPassParams( DRoutine *routine, DValue *obj, DValue *recv[], DValue *p[], DValue *base, int np, int code )
{
  int ip, it=0, npar = np;
  int ndef = routine->parCount;
  int selfChecked = 0;
  int constParam = routine->type == DAO_ROUTINE ? ((DaoRoutine*)routine)->constParam : 0;
  int norc = 0; /* routine->type == DAO_FUNCTION ? 1 : 0; ??? */
  DaoType *routype = routine->routType;
  DaoType *tp, **parType = routype->nested->items.pAbtp;

  if( code == DVM_MCALL_TC && ! (routype->attrib & DAO_TYPE_SELF) ){
    npar --;
    p ++;
    if(base) base ++;
  }else if( obj && obj->t && (routype->attrib & DAO_TYPE_SELF) ){
    /* class DaoClass : CppClass{ cppmethod(); } */
    tp = parType[0]->X.abtype;
    if( obj->t < DAO_ARRAY ){
      if( tp == NULL || DaoType_MatchValue( tp, *obj, NULL ) == DAO_MT_EQ ){
        recv[0] = obj;
        selfChecked = 1;
      }
    }else{
      DValue o = *obj;
      if( o.t == DAO_OBJECT && (tp->tid ==DAO_OBJECT || tp->tid ==DAO_CDATA) ){
        o.v.p = DaoObject_MapThisObject( o.v.object, tp );
        if( obj->v.p ) o.t = o.v.p->type;
      }
      if( norc ){
        if( DValue_Pass( o, recv[0], tp ) ) selfChecked = 1;
      }else{
        if( DValue_Move( o, recv[0], tp ) ) selfChecked = 1;
      }
      recv[0]->cst = obj->cst;
    }
  }
  /*
  printf( "rout = %s; ndef = %i; npar = %i, %i\n", routine->routType->name->mbs, ndef, npar, selfChecked );
  */
  if( (npar|ndef) ==0 ) return 1;
  /* it: index of the parameter to be passed and the type to be checked;
   * ip: index of the place to hold the passed parameter; */
  for( ip=selfChecked; ip<ndef; ip++, it++){
    DValue *pv = p[it];
    if( base && pv != base + it && pv->t < DAO_ARRAY && !(constParam & (1<<ip)) ){
      if( DaoType_MatchValue( parType[ip]->X.abtype, *pv, NULL ) == DAO_MT_EQ ){
        recv[ip] = pv;
        continue;
      }
    }
#if 0
    printf( "ip = %i\n", ip );
    DaoType *tp = DaoNameSpace_GetTypeV( routine->nameSpace, *pv );
    printf( "%s\n", parType[ip]->X.abtype->name->mbs );
    printf( "%s\n", tp->name->mbs );
#endif
    if( norc ){
      if( ! DValue_Pass( *pv, recv[ip], parType[ip]->X.abtype ) ) return 0;
    }else{
      if( ! DValue_Move( *pv, recv[ip], parType[ip]->X.abtype ) ) return 0;
    }
    if( constParam & (1<<ip) ) recv[ip]->cst = 1;
  }
  return 1;
}

DaoTypeBase routTyper=
{
  & baseCore,
  "ROUTINE",
  NULL,
  NULL, {0},
  (FuncPtrNew) DaoRoutine_New,
  (FuncPtrDel) DaoRoutine_Delete
};

DaoRoutine* DaoRoutine_New()
{
  DaoRoutine *self = (DaoRoutine*) dao_malloc( sizeof( DaoRoutine ) );
  DaoBase_Init( self, DAO_ROUTINE );
  DRoutine_Init( (DRoutine*)self );
  self->mode = 0;
  self->constParam = 0;
  self->defLine = 0;
  self->bodyStart = 0;
  self->bodyEnd = 0;
  self->revised = NULL;
  self->vmCodes = DaoVmcArray_New();
  self->regType = DArray_New(0);
  self->defLocals = DArray_New(D_TOKEN);
  self->annotCodes = DArray_New(D_VMCODE);
  self->docString = DString_New(1);
  self->regForLocVar = DMap_New(0,0);
  self->abstypes = DMap_New(D_STRING,0);
  self->locRegCount = 0;
  self->upRoutine = NULL;
  self->upContext = NULL;
  self->parser = NULL;
#ifdef DAO_WITH_JIT
  self->binCodes = DArray_New(D_STRING);
  self->jitFuncs = DArray_New(0);
  self->preJit = DaoVmcArray_New();
  self->jitMemory = NULL;
#endif
  return self;
}
void DaoRoutine_Delete( DaoRoutine *self )
{
  DRoutine_DeleteFields( (DRoutine*)self );
  DaoLateDeleter_Push( self );
  if( self->tidHost == DAO_INTERFACE ) return;
  if( self->upRoutine ) GC_DecRC( self->upRoutine );
  if( self->upContext ) GC_DecRC( self->upContext );
  GC_DecRCs( self->regType );
  DString_Delete( self->docString );
  DaoVmcArray_Delete( self->vmCodes );
  DArray_Delete( self->regType );
  DArray_Delete( self->defLocals );
  DArray_Delete( self->annotCodes );
  DMap_Delete( self->regForLocVar );
  DMap_Delete( self->abstypes );
  if( self->revised ) GC_DecRC( self->revised );
  if( self->parser ) DaoParser_Delete( self->parser );
#ifdef DAO_WITH_JIT
  DArray_Delete( self->binCodes );
  DArray_Delete( self->jitFuncs );
  DaoVmcArray_Delete( self->preJit );
  if( self->jitMemory ) GC_DecRC( self->jitMemory );
#endif
}
void DaoParser_ClearCodes( DaoParser *self );
void DaoRoutine_Compile( DaoRoutine *self )
{
  if( self->tidHost == DAO_INTERFACE ) return;
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
        while( daoScriptRaise[i] ){
          DaoTokens_Append( tokens, daoScriptRaise2[i], k, daoScriptRaise[i] );
          i ++;
        }
        if( routp ){
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
DaoRoutine* DaoRoutine_Copy( DaoRoutine *self, int overload )
{
  DaoRoutine *copy = DaoRoutine_New();
  DaoRoutine_Compile( self );
  DRoutine_CopyFields( (DRoutine*) copy, (DRoutine*) self );
  if( overload ) DRoutine_AddOverLoad( (DRoutine*) self->firstRoutine, (DRoutine*) copy );
  DMap_Delete( copy->regForLocVar );
  DArray_Delete( copy->annotCodes );
  copy->annotCodes = DArray_Copy( self->annotCodes );
  copy->regForLocVar = DMap_Copy( self->regForLocVar );
  DaoVmcArray_Assign( copy->vmCodes, self->vmCodes );
  DString_Assign( copy->routName, self->routName );
  DaoGC_IncRCs( self->regType );
  DaoGC_DecRCs( copy->regType );
  DArray_Assign( copy->regType, self->regType );
  copy->constParam = self->constParam;
  copy->locRegCount = self->locRegCount;
  copy->defLine = self->defLine;
  copy->bodyStart = self->bodyStart;
  copy->bodyEnd = self->bodyEnd;
#ifdef DAO_WITH_JIT
  DArray_Assign( copy->binCodes, self->binCodes );
  DArray_Assign( copy->jitFuncs, self->jitFuncs );
  DaoVmcArray_Assign( copy->preJit, self->preJit );
  copy->jitMemory = self->jitMemory;
#endif
  return copy;
}

static int DaoRoutine_InferTypes( DaoRoutine *self );
extern void DaoRoutine_JitCompile( DaoRoutine *self );

int DaoRoutine_SetVmCodes( DaoRoutine *self, DArray *vmCodes )
{
  int i;
  if( vmCodes == NULL || vmCodes->type != D_VMCODE ) return 0;
  DaoVmcArray_Resize( self->vmCodes, vmCodes->size );
  for(i=0; i<vmCodes->size; i++){
    self->vmCodes->codes[i] = * (DaoVmCode*) vmCodes->items.pVmc[i];
  }
  DArray_Swap( self->annotCodes, vmCodes );
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
      DaoType *tp = DaoType_DeepItemType( self->nested->items.pAbtp[i] );
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

enum DaoTypingErrorCode{
  DT_NOT_CONSIS ,
  DT_NOT_MATCH ,
  DT_NOT_INIT,
  DT_NOT_PERMIT ,
  DT_NOT_EXIST ,
  DT_NEED_INSTVAR ,
  DT_INV_INDEX ,
  DT_INV_KEY ,
  DT_INV_OPER ,
  DT_INV_PARAM ,
  DT_MODIFY_CONST ,
  DT_NOT_IMPLEMENTED
};
static const char*const DaoTypingErrorString[] =
{
  "inconsistent typing",
  "types not matching",
  "variable not initialized",
  "member not permited",
  "member not exist",
  "need class instance",
  "invalid index",
  "invalid key",
  "invalid operation on the type",
  "invalid parameters for the call",
  "constant should not be modified",
  "call to un-implemented function"
};
extern DaoClass *daoClassFutureValue;

enum OprandType
{
  OT_OOO = 0,
  OT_AOO , /* SETVX */
  OT_OOC , /* GETCX GETVX */
  OT_AOC , /* LOAD, MOVE, CAST, unary operations... */
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
  { OT_OOC, -1, -1,  0, -1, -1,  -1 } , /* DVM_GETC */
  { OT_OOC, -1, -1,  0, -1, -1,  -1 } , /* DVM_GETV */
  { OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_GETI */
  { OT_AIC,  0, -1,  0, -1, -1,  -1 } , /* DVM_GETF */
  { OT_AOO,  0, -1, -1, -1, 'S',  -1 } , /* DVM_SETV */
  { OT_ABC,  0,  0,  0, -1, 'S',  -1 } , /* DVM_SETI */
  { OT_AIC,  0, -1,  0, -1, 'S',  -1 } , /* DVM_SETF */
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
  { OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_BITAND */
  { OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_BITOR */
  { OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_BITXOR */
  { OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_BITLFT */
  { OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_BITRIT */
  { OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_CHECK */
  { OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_PAIR */
  { OT_EXP,  0, 'X', -1, -1, -1,  -1 } , /* DVM_TUPLE */
  { OT_EXP,  0, 'X', -1, -1, -1,  -1 } , /* DVM_LIST */
  { OT_EXP,  0,  0 , -1, -1, -1,  -1 } , /* DVM_MAP */
  { OT_EXP,  0,  0 , -1, -1, -1,  -1 } , /* DVM_HASH */
  { OT_EXP,  0, 'X', -1, -1, -1,  -1 } , /* DVM_ARRAY */
  { OT_EXP,  0,  0 , -1, -1, -1,  -1 } , /* DVM_MATRIX */
  { OT_EXP, 'A',  0, -1, -1, -1,  -1 } , /* DVM_CURRY */
  { OT_EXP, 'A',  0, -1, -1, -1,  -1 } , /* DVM_MCURRY */
  { OT_OOO, -1, -1, -1, -1, -1,  -1 } , /* DVM_GOTO */
  { OT_OOO, -1, -1, -1, -1, -1,  -1 } , /* DVM_SWITCH */
  { OT_OOO, -1, -1, -1, -1, -1,  -1 } , /* DVM_CASE */
  { OT_OOO, -1, -1, -1, -1, -1,  -1 } , /* DVM_ASSERT */
  { OT_AOC,  0, -1,  0, -1, -1,  -1 } , /* DVM_ITER */
  { OT_OOO,  0, -1, -1, -1, -1,  -1 } , /* DVM_TEST */
  { OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_MATH */
  { OT_ABC,  0,  0,  0, -1, -1,  -1 } , /* DVM_FUNCT */
  { OT_EXP, 'A',   0, -1, -1, 'M', -1 } , /* DVM_CALL */
  { OT_EXP, 'A',   0, -1, -1, 'M', -1 } , /* DVM_MCALL */
  { OT_EXP, 'A',   0, -1, -1, -1,  -1 } , /* DVM_CLOSE */
  { OT_EXP,   0, 'B', -1, -1, -1,  -1 } , /* DVM_CRRE */
  { OT_OOO,  -1,  -1, -1, -1, -1,  -1 } , /* DVM_JITC */
  { OT_OOO,  -1,  -1, -1, -1, -1,  -1 } , /* DVM_JOINT */
  { OT_EXP,   0,   0, -1, -1, -1,  -1 } , /* DVM_RETURN */
  { OT_EXP,   0,   0, -1, -1, -1,  -1 } , /* DVM_YIELD */
  { OT_OOO,  -1,  -1, -1, -1, -1,  -1 } , /* DVM_DEBUG */
  { OT_OOO,  -1,  -1, -1, -1, -1,  -1 } , /* DVM_SECT */

  { OT_OOC, -1, -1, 'I', 'L', -1,  -1 } , /* DVM_GETC_I */
  { OT_OOC, -1, -1, 'F', 'L', -1,  -1 } , /* DVM_GETC_F */
  { OT_OOC, -1, -1, 'D', 'L', -1,  -1 } , /* DVM_GETC_D */
  { OT_OOC, -1, -1, 'I', 'G', -1,  -1 } , /* DVM_GETV_I */
  { OT_OOC, -1, -1, 'F', 'G', -1,  -1 } , /* DVM_GETV_F */
  { OT_OOC, -1, -1, 'D', 'G', -1,  -1 } , /* DVM_GETV_D */

  { OT_AOO, 'I', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETV_II */
  { OT_AOO, 'F', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETV_IF */
  { OT_AOO, 'D', -1, 'I', 'G', 'S',  -1 } , /* DVM_SETV_ID */
  { OT_AOO, 'I', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETV_FI */
  { OT_AOO, 'F', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETV_FF */
  { OT_AOO, 'D', -1, 'F', 'G', 'S',  -1 } , /* DVM_SETV_FD */
  { OT_AOO, 'I', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETV_DI */
  { OT_AOO, 'F', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETV_DF */
  { OT_AOO, 'D', -1, 'D', 'G', 'S',  -1 } , /* DVM_SETV_DD */

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

int DaoRoutine_InferTypes( DaoRoutine *self )
{
#define CHECK_PAIR_NUMBER( tp ) \
  k = (tp)->nested->items.pAbtp[0]->tid; \
  if( k > DAO_DOUBLE && k !=DAO_ANY ) goto NotMatch; \
  k = (tp)->nested->items.pAbtp[1]->tid; \
  if( k > DAO_DOUBLE && k !=DAO_ANY ) goto NotMatch;

#define ADD_MOVE_XI( opABC, opcode ) \
  vmc2.a = opABC; \
  opABC = self->locRegCount + addRegType->size -1; \
  addCount[i] ++; \
  vmc2.code = opcode; \
  vmc2.c = opABC; \
  DArray_Append( addCode, & vmc2 ); \
  DArray_Append( addRegType, inumt );

  int typed_code = daoConfig.typedcode;
  int i, j, k, cid=0, ec = 0, retinf = 0;
  int N = self->vmCodes->size;
  int M = self->locRegCount;
  int min, norm, spec, worst;
  int TT0, TT1, TT2, TT3, TT4, TT5, TT6;
  ushort_t code;
  ushort_t opa, opb, opc;
  DaoNameSpace *ns = self->nameSpace;
  DaoVmSpace *vms = ns->vmSpace;
  DaoType **gdtypes, **cdtypes=NULL, **odtypes=NULL;
  DaoType **tp, **type;
  DaoType *at, *bt, *ct, *tt, *ts[DAO_STRING+1];
  DaoType *simtps[DAO_ARRAY], *listps[DAO_ARRAY], *arrtps[DAO_ARRAY];
  DaoType *inumt, *fnumt, *dnumt, *comt, *strt;
  DaoType *ilst, *flst, *dlst, *slst, *iart, *fart, *dart, *cart, *any, *udf;
  DaoType **varTypes[ DAO_U+1 ];
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
  DArray     *tparray;
  char       *init;
  int      *addCount;
  DArray   *vmCodeNew, *addCode;
  DArray   *addRegType;
  DVarray  *regConst;
  DValue   *cstValues[ DAO_U+1 ];
  DValue   *locConsts = self->routConsts->data;
  DValue    empty = daoNullValue;
  DValue    val;
  DValue   *csts;
  DValue   *pp;
  int isconst = self->attribs & DAO_ROUT_ISCONST;
  int notide = ! (vms->options & DAO_EXEC_IDE);
  /* To support Edit&Continue in DaoStudio, some of the features
   * have to be switched off:
   * (1) function specialization based on parameter types;
   * (2) instruction specialization requiring
   *     additional instructions and vm registers; */

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
  strt = DaoNameSpace_MakeType( ns, "string", DAO_STRING, NULL, NULL, 0 );
  ilst = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, &inumt, 1 );
  flst = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, &fnumt, 1 );
  dlst = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, &dnumt, 1 );
  slst = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, &strt, 1 );
  iart = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY, NULL, &inumt, 1 );
  fart = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY, NULL, &fnumt, 1 );
  dart = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY, NULL, &dnumt, 1 );
  cart = DaoNameSpace_MakeType( ns, "array", DAO_ARRAY, NULL, &comt, 1 );

  ts[0] = simtps[0] = listps[0] = arrtps[0] = any;
  simtps[DAO_INTEGER] = inumt;
  simtps[DAO_FLOAT] = fnumt;
  simtps[DAO_DOUBLE] = dnumt;
  simtps[DAO_COMPLEX] = comt;
  simtps[DAO_STRING] = strt;
  listps[DAO_INTEGER] = ilst;
  listps[DAO_FLOAT] = flst;
  listps[DAO_DOUBLE] = dlst;
  listps[DAO_STRING] = slst;
  arrtps[DAO_INTEGER] = iart;
  arrtps[DAO_FLOAT] = fart;
  arrtps[DAO_DOUBLE] = dart;
  arrtps[DAO_COMPLEX] = cart;
  ts[1] = inumt;
  ts[2] = fnumt;
  ts[3] = dnumt;
  ts[4] = comt;
  ts[5] = DaoNameSpace_MakeType( ns, "string", DAO_STRING, NULL, NULL, 0 );

  regConst = DVarray_New();
  DVarray_Resize( regConst, self->locRegCount, daoNullValue );
  DArray_Resize( self->regType, self->locRegCount, 0 );
  type = self->regType->items.pAbtp;
  csts = regConst->data;

  node = DMap_First( self->regForLocVar ); /* XXX remove DaoParser::regForLocVar */
  for( ; node !=NULL; node = DMap_Next(self->regForLocVar,node) ){
    type[ node->key.pInt ] = (DaoType*)node->value.pVoid;
  }
  for(i=0, k=0; i<self->routType->nested->size; i++, k++){
    init[k] = 1;
    type[k] = self->routType->nested->items.pAbtp[i];
    if( type[k] && type[k]->tid == DAO_PAR_VALIST ) type[k] = NULL;
    if( self->constParam & (1<<i) ) csts[i].cst = 1;
    tt = type[k];
    if( tt ){
      if( tt->tid == DAO_PAR_GROUP ){
        for(j=0; j<tt->nested->size; j++, k++){
          init[k] = 1;
          type[k] = tt->nested->items.pAbtp[j]->X.abtype;
        }
        k --;
      }else{
        type[k] = tt->X.abtype; /* name:type, name=type */
      }
    }
  }
  varTypes[ DAO_OV ] = NULL;
  varTypes[ DAO_K ] = NULL;
  cstValues[ DAO_K ] = NULL;
  gdtypes = self->nameSpace->varType->items.pAbtp;
  if( self->tidHost == DAO_OBJECT ){
    cdtypes = hostClass->glbDataType->items.pAbtp;
    odtypes = hostClass->objDataType->items.pAbtp;
    varTypes[ DAO_OV ] = odtypes;
    varTypes[ DAO_K ] = cdtypes;
    cstValues[ DAO_K ] = hostClass->cstData->data;
  }
  cstValues[ DAO_LC ] = self->routConsts->data;
  cstValues[ DAO_G ] = self->nameSpace->cstData->data;
  varTypes[ DAO_G ] = self->nameSpace->varType->items.pAbtp;
  if( self->upRoutine ){
    cstValues[ DAO_U ] = self->upRoutine->routConsts->data;
    varTypes[ DAO_U ] = self->upRoutine->regType->items.pAbtp;
  }

  /*
  printf( "DaoRoutine_InferTypes() %p %s %i %i\n", self, self->routName->mbs, self->parCount, self->locRegCount );
  if( self->routType ) printf( "%p %p\n", hostClass, self->routType->X.extra );
  DaoRoutine_PrintCode( self, self->nameSpace->vmSpace->stdStream );
  */

  vmc2.annot = NULL;
  for(i=0; i<N; i++){
    /* adding type to namespace may add constant data as well */
    cstValues[ DAO_G ] = self->nameSpace->cstData->data;
    cid = i;
    error = NULL;
    vmc = vmcs[i];
    vmc2 = * self->annotCodes->items.pVmc[i];
    vmc2.annot = NULL;
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
      if( STRCMP( abtp->name, "?" ) == 0 ){
        DMap_Erase( defs, (void*) abtp );
        node = DMap_First( defs );
        continue;
      }
      node = DMap_Next( defs, node );
    }

#if 0
    printf( "%4i: ", i );DaoVmCodeX_Print( vmc2, NULL );
#endif
    switch( code ){
    case DVM_NOP :
    case DVM_DEBUG :
      break;
    case DVM_DATA :
      if( opa > DAO_STRING ) goto ErrorTyping;
      init[opc] = 1;
      at = simtps[ opa ];
      if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ){
        type[opc] = at;
      }else{
        if( DaoType_MatchTo( at, type[opc], defs )==0 ) goto NotMatch;
      }
      break;
    case DVM_GETC :
      {
        val = empty;
        if( cstValues[ opa ] ) val = cstValues[ opa ][ opb ];
        at = DaoNameSpace_GetTypeV( ns, val );

        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = at;
        /*
           printf( "at %i %i\n", at->tid, type[opc]->tid );
         */
        if( DaoType_MatchTo( at, type[opc], defs )==0 ) goto NotMatch;
        csts[opc] = val;
        init[opc] = 1;
        if( typed_code ){
          if( at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
            vmc->code = DVM_GETC_I + ( at->tid - DAO_INTEGER );
          }
        }
        csts[opc].cst = 1;
        break;
      }
    case DVM_GETV :
      {
        at = 0;
        if( varTypes[ opa ] ) at = varTypes[ opa ][ opb ];
        if( self->tidHost == DAO_OBJECT && varTypes[ opa ] == NULL ){
          printf( "need host class\n" );
          goto NotMatch;/* XXX */
        }
        if( at == NULL ) at = any;
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = at;
        /*
           printf( "%s\n", at->name->mbs );
           printf( "%p %p\n", at, type[opc] );
           printf( "%s %s\n", at->name->mbs, type[opc]->name->mbs );
         */
        if( DaoType_MatchTo( at, type[opc], defs )==0 ) goto NotMatch;
        init[opc] = 1;
        if( typed_code ){
          if( at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
            vmc->code = DVM_GETV_I + ( at->tid - DAO_INTEGER );
          }
        }
        if( isconst && opa == DAO_OV ) csts[opc].cst = 1;
        break;
      }
    case DVM_SETV :
      {
        if( isconst && opc == DAO_OV ) goto ModifyConstant;
        if( type[opa] ==NULL ) goto NotInit;
        tp = NULL;
        if( varTypes[ opc ] ) tp = varTypes[ opc ] + opb;
        if( self->tidHost == DAO_OBJECT && varTypes[ opc ] == NULL ){
          printf( "need host class\n" );
          goto NotMatch;/* XXX */
        }
        at = type[opa];
        if( tp && ( *tp==NULL || (*tp)->tid ==DAO_UDF ) ) *tp = at;
        if( tp && (*tp)->tid <= DAO_DOUBLE ){
          if( opc == DAO_OV && opb < hostClass->objDataDefault->size ){
            hostClass->objDataDefault->data[ opb ].t = at->tid;
          }else if( opc == DAO_K && opb < hostClass->glbData->size ){
            hostClass->glbData->data[ opb ].t = at->tid;
          }else if( opc == DAO_G && opb < ns->varData->size ){
            ns->varData->data[ opb ].t = at->tid;
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
        if( k ==0 ) goto NotMatch;
        at = type[opa];
        if( tp[0]->tid >= DAO_INTEGER && tp[0]->tid <= DAO_DOUBLE
            && at->tid >= DAO_INTEGER && at->tid <= DAO_DOUBLE ){
          if( typed_code ){
            vmc->code = 3*( tp[0]->tid - DAO_INTEGER ) + ( at->tid - DAO_INTEGER ) + DVM_SETV_II;
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
        init[opc] = 1;
        if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
        if( type[opa] ==NULL || type[opb] == NULL ) goto NotInit;
        if( init[opa] ==0 || init[opb] == 0 ) goto NotInit;
        at = type[opa];
        bt = type[opb];
        ct = NULL;
        if( at->tid == DAO_ANY || at->tid == DAO_UDF ){
          /* allow less strict typing: */
          ct = any;
        }else if( at->tid == DAO_INTEGER ){
          ct = inumt;
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
                ADD_MOVE_XI( vmc->b, DVM_MOVE_IF );
              }else if( bt->tid == DAO_DOUBLE && notide ){
                ct = inumt;
                vmc->code = DVM_GETI_SI;
                ADD_MOVE_XI( vmc->b, DVM_MOVE_ID );
              }
            }
          }else if( bt == dao_type_for_iterator ){
            ct = inumt;
          }else if( bt->tid ==DAO_PAIR ){
            ct = at;
            CHECK_PAIR_NUMBER( bt );
          }else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2 ){
            ct = at;
            CHECK_PAIR_NUMBER( bt );
          }else if( bt->tid ==DAO_LIST || bt->tid ==DAO_ARRAY ){
            /* passed */
            k = bt->nested->items.pAbtp[0]->tid;
            if( k > DAO_DOUBLE && k !=DAO_ANY ) goto NotMatch;
          }else if( bt->tid==DAO_UDF || bt->tid==DAO_ANY ){
            /* less strict checking */
            ct = any;
          }
        }else if( at->tid == DAO_LONG ){
          ct = inumt; /* XXX slicing */
        }else if( at->tid == DAO_LIST ){
          /*
           */
          if( bt->tid == DAO_INTEGER || bt->tid == DAO_FLOAT
              || bt->tid == DAO_DOUBLE ){
            ct = at->nested->items.pAbtp[0];
            if( typed_code && notide ){
              if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE ){
                vmc->code = DVM_GETI_LII + ct->tid - DAO_INTEGER;
              }else if( ct->tid == DAO_STRING ){
                vmc->code = DVM_GETI_LSI;
              }else if( ct->tid >= DAO_ARRAY && ct->tid < DAO_ANY ){
                /* for skipping type checking */
                vmc->code = DVM_GETI_LI;
              }
              if( vmc->code != DVM_GETI && vmcs[i+1]->code == DVM_JOINT ){
                vmcs[i+1]->code = DVM_UNUSED;
                vmcs[i+3]->code = DVM_UNUSED;
              }

              if( bt->tid == DAO_FLOAT ){
                ADD_MOVE_XI( vmc->b, DVM_MOVE_IF );
              }else if( bt->tid == DAO_DOUBLE ){
                ADD_MOVE_XI( vmc->b, DVM_MOVE_ID );
              }
            }
          }else if( bt == dao_type_for_iterator ){
            ct = at->nested->items.pAbtp[0];
          }else if( bt->tid ==DAO_PAIR ){
            ct = at;
            CHECK_PAIR_NUMBER( bt );
          }else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2 ){
            ct = at;
            CHECK_PAIR_NUMBER( bt );
          }else if( bt->tid ==DAO_LIST || bt->tid ==DAO_ARRAY ){
            ct = at;
            k = bt->nested->items.pAbtp[0]->tid;
            if( k !=DAO_INTEGER && k !=DAO_FLOAT && k !=DAO_ANY && k !=DAO_UDF )
              goto NotMatch;
          }else if( bt->tid==DAO_UDF || bt->tid==DAO_ANY ){
            /* less strict checking */
            ct = any;
          }else{
            goto InvIndex;
          }
        }else if( at->tid == DAO_MAP ){
          DaoType *t0 = at->nested->items.pAbtp[0];
          /*
             printf( "at %s %s\n", at->name->mbs, bt->name->mbs );
           */
          if( bt->tid == DAO_PAIR ){
            ts[0] = bt->nested->items.pAbtp[0];
            ts[1] = bt->nested->items.pAbtp[1];
            if( ( ts[0]->tid != DAO_UDF && ts[0]->tid != DAO_ANY
                  && DaoType_MatchTo( ts[0], t0, defs ) ==0 )
                || ( ts[1]->tid != DAO_UDF && ts[1]->tid != DAO_ANY
                  && DaoType_MatchTo( ts[1], t0, defs ) ==0 ) ){
              goto InvKey;
            }
            ct = at;
          }else if( bt->tid==DAO_UDF || bt->tid==DAO_ANY ){
            /* less strict checking */
            ct = any;
          }else if( bt == dao_type_for_iterator ){
            ct = DaoNameSpace_MakeType( ns, "tuple", DAO_TUPLE,
                NULL, at->nested->items.pAbtp, 2 );
          }else{
            if( DaoType_MatchTo( bt, t0, defs ) ==0) goto InvKey;
            ct = at->nested->items.pAbtp[1];
          }
        }else if( at->tid == DAO_ARRAY ){
          if( bt->tid == DAO_INTEGER || bt->tid == DAO_FLOAT
              || bt->tid == DAO_DOUBLE ){
            /* array[i] */
            ct = at->nested->items.pAbtp[0];
            if( typed_code && notide ){
              if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_DOUBLE )
                vmc->code = DVM_GETI_AII + ct->tid - DAO_INTEGER;
              else if( ct->tid == DAO_COMPLEX )
                vmc->code = DVM_GETI_ACI;

              if( bt->tid == DAO_FLOAT ){
                ADD_MOVE_XI( vmc->b, DVM_MOVE_IF );
              }else if( bt->tid == DAO_DOUBLE ){
                ADD_MOVE_XI( vmc->b, DVM_MOVE_ID );
              }
            }
          }else if( bt->tid ==DAO_PAIR ){
            ct = at;
            CHECK_PAIR_NUMBER( bt );
          }else if( bt == dao_type_for_iterator ){
            ct = at->nested->items.pAbtp[0];
          }else if( bt->tid ==DAO_TUPLE ){
            ct = at->nested->items.pAbtp[0];
            for(j=0; j<bt->nested->size; j++){
              int tid = bt->nested->items.pAbtp[j]->tid;
              if( tid ==0 || tid > DAO_DOUBLE ){
                ct = at;
                break;
              }
            }
          }else if( bt->tid == DAO_LIST || bt->tid == DAO_ARRAY ){
            /* array[ {1,2,3} ] or array[ [1,2,3] ] */
            ct = at;
            k = bt->nested->items.pAbtp[0]->tid;
            if( k !=DAO_INTEGER && k !=DAO_FLOAT && k !=DAO_ANY && k !=DAO_UDF )
              goto NotMatch;
          }else if( bt->tid==DAO_UDF || bt->tid==DAO_ANY ){
            /* less strict checking */
            ct = any;
          }
        }else if( at->tid ==DAO_TUPLE ){
          ct = any;
          val = csts[opb];
          if( val.t ){
            if( val.t > DAO_DOUBLE ) goto InvIndex;
            k = DValue_GetInteger( val );
            if( k <0 || k >= at->nested->size ) goto InvIndex;
            ct = at->nested->items.pAbtp[ k ];
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
                if( vmc->code != DVM_GETI && vmcs[i+1]->code == DVM_JOINT ){
                  vmcs[i+1]->code = DVM_UNUSED;
                  vmcs[i+3]->code = DVM_UNUSED;
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
            if( vmc->code != DVM_GETI && vmcs[i+1]->code == DVM_JOINT ){
              vmcs[i+1]->code = DVM_UNUSED;
              vmcs[i+3]->code = DVM_UNUSED;
            }
          }else if( bt->tid != DAO_UDF && bt->tid != DAO_ANY ){
            goto InvIndex;
          }
        }else if( at->tid == DAO_OBJECT && DaoClass_FindOperator( at->X.klass, "[]", hostClass ) ){
          ct = any; /* XXX */
        }else if( at->tid == DAO_UDF || at->tid == DAO_ANY
            || at->tid == DAO_INITYPE || at->tid == DAO_CDATA /* XXX */ ){
          ct = any;
        }
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        /*
           DaoVmCodeX_Print( *vmc, NULL );
           printf( "at %s %s %i\n", at->name->mbs, bt->name->mbs, bt->tid );
           if(ct) printf( "ct %s %s\n", ct->name->mbs, type[opc]->name->mbs );
         */
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
        break;
      }
    case DVM_GETF :
      {
        int ak = 0;
        init[opc] = 1;
        if( type[opa] == NULL ) goto NotInit;
        if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
        if( init[opa] ==0 ) goto NotInit;
        ct = NULL;
        val = locConsts[opb];
        if( val.t != DAO_STRING ) goto NotMatch;
        str = val.v.s;
        at = type[opa];
        ak = at->tid ==DAO_CLASS;
        error = str;
        if( at->tid == DAO_ANY || at->tid == DAO_UDF ){
          /* allow less strict typing: */
          ct = any;
        }else if( at->tid == DAO_INTERFACE ){
          ct = any; /* TODO */
        }else if( at->tid == DAO_CLASS || at->tid == DAO_OBJECT ){
          int getter = 0;
          klass = (DaoClass*) at->X.extra;
          tp = DaoClass_GetDataType( klass, str, & j, hostClass );
          if( j ){
            DString_SetMBS( mbs, "." );
            DString_Append( mbs, str );
            tp = DaoClass_GetDataType( klass, mbs, & j, hostClass );
            DaoClass_GetData( klass, mbs, & val, hostClass, NULL );
            if( j==0 && tp == NULL ) ct = DaoNameSpace_GetTypeV( ns, val );
            if( val.t && ct && ct->tid == DAO_ROUTINE ){
              rout = (DRoutine*) val.v.routine;
              if( rout->parCount - (rout->attribs & DAO_ROUT_PARSELF) >0 )
                goto NotMatch;/* XXX */
              ct = ct->X.abtype;
              getter = 1;
              if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
              if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
            }
          }
          DString_Assign( mbs, at->name );
          DString_AppendChar( mbs, '.' );
          DString_Append( mbs, str );
          if( j == DAO_ERROR_FIELD_NOTPERMIT ) goto NotPermit;
          if( j == DAO_ERROR_FIELD_NOTEXIST ) goto NotExist;
          j = DaoClass_GetDataIndex( klass, str, & k );
          if( k == DAO_CLASS_VARIABLE && at->tid ==DAO_CLASS ) goto NeedInstVar;
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
              if( k == DAO_CLASS_CONST )
                vmc->code = ak ? DVM_GETF_KCI : DVM_GETF_OCI;
              else if( k == DAO_CLASS_GLOBAL )
                vmc->code = ak ? DVM_GETF_KGI : DVM_GETF_OGI;
              else if( k == DAO_CLASS_VARIABLE )
                vmc->code = DVM_GETF_OVI;
              vmc->code += 5 * ( ct->tid - DAO_INTEGER );
            }else{
              if( k == DAO_CLASS_CONST )
                vmc->code = ak ? DVM_GETF_KC : DVM_GETF_OC;
              else if( k == DAO_CLASS_GLOBAL )
                vmc->code = ak ? DVM_GETF_KG : DVM_GETF_OG;
              else if( k == DAO_CLASS_VARIABLE )
                vmc->code = DVM_GETF_OV;
            }
            if( vmc->code != DVM_GETF && vmcs[i+1]->code == DVM_JOINT ){
              vmcs[i+1]->code = DVM_UNUSED;
              vmcs[i+3]->code = DVM_UNUSED;
            }
          }
        }else if( at->tid == DAO_TUPLE ){
          if( at->mapNames == NULL ) goto NotExist;
          node = MAP_Find( at->mapNames, str );
          if( node == NULL ) goto NotExist;
          k = node->value.pInt;
          if( k <0 || k >= at->nested->size ) goto NotExist;
          ct = at->nested->items.pAbtp[ k ];
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
              if( vmc->code != DVM_GETF && vmcs[i+1]->code == DVM_JOINT ){
                vmcs[i+1]->code = DVM_UNUSED;
                vmcs[i+3]->code = DVM_UNUSED;
              }
            }
          }
        }else if( at->tid == DAO_NAMESPACE ){
          ct = any;
          if( csts[opa].t == DAO_NAMESPACE && csts[opa].v.ns ){
            k = DaoNameSpace_FindVariable( csts[opa].v.ns, str );
            if( k <0 ) k = DaoNameSpace_FindConst( csts[opa].v.ns, str );
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
          func = DaoFindFunction( at->typer, mbs );
          if( func ){
            if( func->parCount - (func->attribs & DAO_ROUT_PARSELF) >0 )
              goto NotMatch;/* XXX */
            ct = func->routType;
            ct = ct->X.abtype;
          }else{
            DString_Assign( mbs, at->name );
            DString_AppendChar( mbs, '.' );
            DString_Append( mbs, str );
            goto NotExist;
          }
        }
        if( ct == NULL ) ct = udf;
      }
      if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
      if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
      csts[opc].cst = csts[opa].cst;
      break;
      }
    case DVM_SETI :
      {
        ct = (DaoType*) type[opc];
        if( ct == NULL ) goto ErrorTyping;
        if( csts[opc].cst ) goto ModifyConstant;
        if( init[opa] ==0 || init[opb] ==0 || init[opc] ==0 ) goto NotInit;
        at = type[opa];
        bt = type[opb];
        if( bt->tid == DAO_UDF || bt->tid == DAO_ANY ) break;
        switch( ct->tid ){
        case DAO_UDF :
        case DAO_ANY :
          /* allow less strict typing: */
          break;
        case DAO_STRING :
          if( typed_code && notide ){
            if( ( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE )
                && ( bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE ) ){
              vmc->code = DVM_SETI_SII;
              if( at->tid ==DAO_FLOAT ){
                ADD_MOVE_XI( vmc->a, DVM_MOVE_IF );
              }else if( at->tid ==DAO_DOUBLE ){
                ADD_MOVE_XI( vmc->a, DVM_MOVE_ID );
              }
              if( bt->tid ==DAO_FLOAT ){
                ADD_MOVE_XI( vmc->b, DVM_MOVE_IF );
              }else if( bt->tid ==DAO_DOUBLE ){
                ADD_MOVE_XI( vmc->b, DVM_MOVE_ID );
              }
            }
          }
          /* less strict checking */
          if( at->tid >= DAO_ARRAY && at->tid !=DAO_ANY ) goto NotMatch;

          if( bt->tid==DAO_PAIR && at->tid==DAO_STRING ){
            /* passed */
            CHECK_PAIR_NUMBER( bt );
          }else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2
              && at->tid==DAO_STRING ){
            ct = at;
            CHECK_PAIR_NUMBER( bt );
          }else if( bt->tid > DAO_DOUBLE && bt->tid !=DAO_ANY ){
            /* less strict checking */
            goto NotMatch;
          }
          break;
        case DAO_LONG :
          ct = inumt; /* XXX slicing */
          break;
        case DAO_LIST :
          ts[0] = ct->nested->items.pAbtp[0];
          if( bt->tid >=DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
            ct = ct->nested->items.pAbtp[0];
            if( typed_code && notide ){
              if( ct->tid >=DAO_INTEGER && ct->tid <= DAO_DOUBLE
                  && at->tid >=DAO_INTEGER && at->tid <= DAO_DOUBLE ){
                vmc->code = 3 * ( ct->tid - DAO_INTEGER ) + DVM_SETI_LIII
                  + at->tid - DAO_INTEGER;
              }else if( at->tid ==DAO_STRING && ct->tid ==DAO_STRING ){
                vmc->code = DVM_SETI_LSIS;
              }else if( at->tid >= DAO_ARRAY && at->tid < DAO_ANY ){
                if( DaoType_MatchTo( at, ct, defs )==0 ) goto NotMatch;
                if( DString_EQ( at->name, ct->name ) )
                  vmc->code = DVM_SETI_LI;
              }
              if( bt->tid ==DAO_FLOAT ){
                ADD_MOVE_XI( vmc->b, DVM_MOVE_IF );
              }else if( bt->tid ==DAO_DOUBLE ){
                ADD_MOVE_XI( vmc->b, DVM_MOVE_ID );
              }
            }
          }else if( bt->tid==DAO_PAIR ){
            CHECK_PAIR_NUMBER( bt );
            if( DaoType_MatchTo( at, ts[0], defs )==0 ) goto NotMatch;
          }else if( bt->tid ==DAO_TUPLE && bt->nested->size ==2 ){
            CHECK_PAIR_NUMBER( bt );
            if( DaoType_MatchTo( at, ts[0], defs )==0 ) goto NotMatch;
          }else if( bt->tid==DAO_LIST || bt->tid==DAO_ARRAY ){
            k = bt->nested->items.pAbtp[0]->tid;
            if( k !=DAO_INTEGER && k !=DAO_FLOAT
                && k !=DAO_ANY && k !=DAO_UDF )
              goto NotMatch;
            if( DaoType_MatchTo( at, ts[0], defs )==0 ) goto NotMatch;
          }else{
            goto NotMatch;
          }
          break;
        case DAO_MAP :
          {
            DaoType *t0 = ct->nested->items.pAbtp[0];
            DaoType *t1 = ct->nested->items.pAbtp[1];
            /*
               DaoVmCode_Print( *vmc, NULL );
               printf( "%p %p %p\n", t0, t1, ct );
               printf( "ct %p %s\n", ct, ct->name->mbs );
               printf( "at %i %s %i %s\n", at->tid, at->name->mbs, t1->tid, t1->name->mbs );
               printf( "bt %s %s\n", bt->name->mbs, t0->name->mbs );
             */
            if( bt->tid == DAO_PAIR ){
              ts[0] = bt->nested->items.pAbtp[0];
              ts[1] = bt->nested->items.pAbtp[1];
              if( ( ts[0]->tid != DAO_UDF && ts[0]->tid != DAO_ANY
                    && DaoType_MatchTo( ts[0], t0, defs ) ==0 )
                  || ( ts[1]->tid != DAO_UDF && ts[1]->tid != DAO_ANY
                    && DaoType_MatchTo( ts[1], t0, defs ) ==0 ) ){
                goto InvKey;
              }
              if( DaoType_MatchTo( at, t1, defs )==0 ) goto NotMatch;
            }else if( bt->tid==DAO_UDF || bt->tid==DAO_ANY ){
              /* less strict checking */
              if( DaoType_MatchTo( at, t1, defs )==0 ) goto NotMatch;
            }else{
              if( DaoType_MatchTo( bt, t0, defs ) ==0) goto InvKey;
              if( DaoType_MatchTo( at, t1, defs )==0 ) goto NotMatch;
              /* define types for: h={=>}; h["A"]=1; */
              if( t0->tid ==DAO_UDF || t0->tid ==DAO_INITYPE
                  || t1->tid ==DAO_UDF || t1->tid ==DAO_INITYPE )
                type[opc] = DaoType_DefineTypes( ct, ns, defs );
            }
            break;
          }
        case DAO_ARRAY :
          if( bt->tid >=DAO_INTEGER && bt->tid <= DAO_DOUBLE ){
            if( DaoType_MatchTo( at, ct, defs ) ) break;
            ct = ct->nested->items.pAbtp[0];
            /* array[i] */
            if( typed_code && notide ){
              if( ct->tid >=DAO_INTEGER && ct->tid <= DAO_DOUBLE
                  && at->tid >=DAO_INTEGER && at->tid <= DAO_DOUBLE ){
                vmc->code = 3 * ( ct->tid - DAO_INTEGER ) + DVM_SETI_AIII
                  + at->tid - DAO_INTEGER;
              }else if( ct->tid == DAO_COMPLEX ){
                vmc->code = DVM_SETI_ACI;
              }else if( at->tid !=DAO_UDF && at->tid !=DAO_ANY ){
                if( DaoType_MatchTo( at, ct, defs )==0 ) goto NotMatch;
              }
              if( bt->tid ==DAO_FLOAT ){
                ADD_MOVE_XI( vmc->b, DVM_MOVE_IF );
              }else if( bt->tid ==DAO_DOUBLE ){
                ADD_MOVE_XI( vmc->b, DVM_MOVE_ID );
              }
            }
            if( DaoType_MatchTo( at, ct, defs )==0 ) goto NotMatch;
          }else if( bt->tid == DAO_LIST || bt->tid == DAO_ARRAY ){
            k = bt->nested->items.pAbtp[0]->tid;
            if( k >=DAO_DOUBLE && k !=DAO_ANY ) goto NotMatch;
            /* imprecise checking */
            if( DaoType_MatchTo( at, ct->nested->items.pAbtp[0], defs )==0
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
            ct = ct->nested->items.pAbtp[ k ];
            if( ct->tid == DAO_PAR_NAMED ) ct = ct->X.abtype;
            if( DaoType_MatchTo( at, ct, 0 ) ==0 ) goto NotMatch;
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
          if( DaoClass_FindOperator( ct->X.klass, "[]=", hostClass ) == NULL)
            goto InvIndex; /* XXX */
          break;
        default : break;
        }
        break;
      }
    case DVM_SETF :
      {
        int ck;
        ct = type[opc];
        at = type[opa];
        if( csts[opc].cst ) goto ModifyConstant;
        if( ct == NULL ) goto ErrorTyping;
        if( init[opa] ==0 || init[opc] ==0 ) goto NotInit;
        if( type[opa] ==NULL ) goto NotMatch;
        /*
           printf( "a: %s\n", type[opa]->name->mbs );
           printf( "c: %s\n", type[opc]->name->mbs );
         */
        val = locConsts[opb];
        if( val.t != DAO_STRING ){
          printf( "field: %i\n", val.t );
          goto NotMatch;
        }
        str = val.v.s;
        switch( ct->tid ){
        case DAO_UDF :
        case DAO_ANY :
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
                rout = (DRoutine*) val.v.routine;
                setter = 1;
                ts[0] = type[opc];
                ts[1] = at;
                rout = DRoutine_GetOverLoadByParamType( rout, ct,
                    NULL, ts, 2, DVM_MCALL, &min, &norm, &spec, & worst );
                if( rout == NULL ) goto NotMatch;
              }
            }
            if( j == DAO_ERROR_FIELD_NOTPERMIT ) goto NotPermit;
            if( j == DAO_ERROR_FIELD_NOTEXIST ) goto NotExist;
            j = DaoClass_GetDataIndex( klass, str, & k );
            if( k == DAO_CLASS_VARIABLE && ct->tid ==DAO_CLASS ) goto NeedInstVar;
            if( setter ) break;
            if( tp ==NULL ) goto NotPermit;
            if( *tp ==NULL ) *tp = type[opa];
            if( DaoType_MatchTo( type[opa], *tp, defs )==0 ) goto NotMatch;
            if( typed_code && (klass->attribs & DAO_CLS_FINAL) ){
              vmc->b = DaoClass_GetDataIndex( klass, str, & k );
              if( k == DAO_CLASS_CONST ) goto InvOper;
              if( *tp && (*tp)->tid>=DAO_INTEGER && (*tp)->tid<=DAO_DOUBLE
                  && at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE ){
                if( k == DAO_CLASS_GLOBAL )
                  vmc->code = ck ? DVM_SETF_KGII : DVM_SETF_OGII;
                else if( k == DAO_CLASS_VARIABLE )
                  vmc->code = DVM_SETF_OVII;
                vmc->code += 9 * ( (*tp)->tid - DAO_INTEGER )
                  + 3 * ( at->tid - DAO_INTEGER );
              }else{
                if( k == DAO_CLASS_GLOBAL )
                  vmc->code = ck ? DVM_SETF_KG : DVM_SETF_OG;
                else if( k == DAO_CLASS_VARIABLE )
                  vmc->code = DVM_SETF_OV;
              }
            }
            break;
          }
        case DAO_TUPLE :
          {
            if( ct->mapNames == NULL ) goto NotExist;
            node = MAP_Find( ct->mapNames, str );
            if( node == NULL ) goto NotExist;
            k = node->value.pInt;
            if( k <0 || k >= ct->nested->size ) goto InvIndex;
            ct = ct->nested->items.pAbtp[ k ];
            if( ct->tid == DAO_PAR_NAMED ) ct = ct->X.abtype;
            if( DaoType_MatchTo( at, ct, 0 ) ==0 ) goto NotMatch;
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
              k = DaoNameSpace_FindVariable( csts[opc].v.ns, str );
              if( k <0 ) k = DaoNameSpace_FindConst( csts[opc].v.ns, str );
              if( k <0 ) goto NotExist;
            }
            break;
          }
        case DAO_CDATA :
          {
            DString_SetMBS( mbs, "." );
            DString_Append( mbs, str );
            DString_AppendMBS( mbs, "=" );
            rout = (DRoutine*) DaoFindFunction( ct->typer, mbs );
            if( rout == NULL ) goto NotMatch; /* XXX */
            ts[0] = ct;
            ts[1] = at;
            rout = DRoutine_GetOverLoadByParamType( rout, ct, NULL,
                ts, 2, DVM_MCALL, &min, &norm, &spec, & worst );
            if( rout == NULL ) goto NotMatch;
            break;
          }
        default: goto InvOper;
        }
        break;
      }
    case DVM_CAST :
      init[opc] = 1;
      if( init[opa] ==0 ) goto NotInit;
      if( type[opa]==NULL ) goto ErrorTyping;
      if( locConsts[opb].t != DAO_TYPE ) goto ErrorTyping;
      if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
      at = (DaoType*) locConsts[opb].v.p;
      if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = at;
      if( DaoType_MatchTo( at, type[opc], defs )==0 ) goto NotMatch;
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
        init[opc] = 1;
        if( init[opa] ==0 ) goto NotInit;
        if( type[opa]==NULL ) goto ErrorTyping;
        if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
        at = type[opa];
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = at;
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
          if( DaoClass_FindOperator( ct->X.klass, "=", hostClass ) == NULL )
            goto NotMatch;
        }else if( at->tid ==DAO_TUPLE && DaoType_MatchTo(type[opc], at, defs)){
          /* less strict checking */
        }else if( k ==0 ){
          goto NotMatch;
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
          if( ct ) type[opc] = ct;
        }
        break;
      }
    case DVM_ADD : case DVM_SUB : case DVM_MUL :
    case DVM_DIV : case DVM_MOD : case DVM_POW :
      {
        init[opc] = 1;
        at = type[opa];
        bt = type[opb];
        if( csts[opc].cst ) goto ModifyConstant;
        if( at == NULL || bt == NULL ) goto ErrorTyping;
        if( init[opa] ==0 || init[opb] == 0 ) goto NotInit;
        ct = NULL;
        /*
           if( type[opa] ) printf( "a: %s\n", type[opa]->name->mbs );
           if( type[opb] ) printf( "b: %s\n", type[opb]->name->mbs );
           if( type[opc] ) printf( "c: %s\n", type[opc]->name->mbs );
         */
        if( at->tid ==DAO_UDF || bt->tid ==DAO_UDF ){
          ct = udf;
        }else if( at->tid ==DAO_ANY || bt->tid ==DAO_ANY
            || at->tid == DAO_OBJECT || bt->tid == DAO_OBJECT
            || at->tid == DAO_CDATA || bt->tid == DAO_CDATA
            /* XXX more strict checking */
            ){
          ct = any;
        }else if( at->tid == bt->tid ){
          ct = at;
          switch( at->tid ){
          case DAO_INTEGER : case DAO_FLOAT : case DAO_DOUBLE :
            break;
          case DAO_STRING :
            if( code != DVM_ADD ) goto InvOper; break;
          case DAO_COMPLEX :
            if( code == DVM_MOD ) goto InvOper; break;
          case DAO_LONG :
            if( code == DVM_POW ) goto InvOper; break;
          case DAO_LIST :
            if( code != DVM_ADD ) goto InvOper;
            if( DaoType_MatchTo( bt, at, defs )==0 ) goto NotMatch;
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
        if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        /* allow less strict typing: */
        if( ct->tid ==DAO_UDF || ct->tid == DAO_ANY ) continue;
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
        ct = type[opc];
        if( i && typed_code && vmcs[i-1]->code != DVM_JOINT ){
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
        init[opc] = 1;
        at = type[opa];
        bt = type[opb];
        if( csts[opc].cst ) goto ModifyConstant;
        if( at == NULL || bt == NULL ) goto ErrorTyping;
        if( init[opa] ==0 || init[opb] == 0 ) goto NotInit;
        ct = inumt;
        if( at->tid == DAO_ANY || bt->tid == DAO_ANY
            || at->tid == DAO_UDF || bt->tid == DAO_UDF
            || at->tid == DAO_OBJECT || bt->tid == DAO_OBJECT
            || at->tid == DAO_CDATA || bt->tid == DAO_CDATA
            /* XXX more strict checking */
          ){
          ct = any;
        }else if( at->tid == bt->tid ){
          ct = at;
          switch( at->tid ){
          case DAO_INTEGER : case DAO_FLOAT : case DAO_DOUBLE :
            break;
          case DAO_STRING :
            if( code > DVM_OR ) ct = inumt; break;
          case DAO_COMPLEX :
            ct = inumt;
            if( code < DVM_EQ ) goto InvOper;
            break;
          case DAO_ARRAY :
            if( code <= DVM_OR )
              ct = at;
            else if( code >= DVM_EQ )
              ct = inumt;
            else goto InvOper;
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
        }else if( at->tid >=DAO_INTEGER && at->tid <=DAO_DOUBLE
            && bt->tid >=DAO_INTEGER && bt->tid <=DAO_DOUBLE ){
          ct = at->tid > bt->tid ? at : bt;
        }else if( code != DVM_EQ && code != DVM_NE ){
          goto InvOper;
        }
        if( type[opc] == NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        /* allow less strict typing: */
        if( ct->tid ==DAO_UDF || ct->tid == DAO_ANY ) continue;
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
        ct = type[opc];
        if( i && typed_code && vmcs[i-1]->code != DVM_JOINT ){
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
    case DVM_NOT : case DVM_UNMS : case DVM_BITREV :
      {
        init[opc] = 1;
        /* force the result of DVM_NOT to be a number? */
        if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
        if( type[opa] == NULL ) goto ErrorTyping;
        if( csts[opc].cst ) goto ModifyConstant;
        if( init[opa] ==0 ) goto NotInit;
        if( type[opc] == NULL || type[opc]->tid ==DAO_UDF ) type[opc] = type[opa];
        if( at->tid ==DAO_UDF || at->tid == DAO_ANY ) continue;
        if( DaoType_MatchTo( type[opa], type[opc], defs )==0 ) goto NotMatch;
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
        init[opc] = 1;
        at = type[opa];
        bt = type[opb];
        if( csts[opc].cst ) goto ModifyConstant;
        if( at == NULL || bt == NULL ) goto ErrorTyping;
        if( init[opa] ==0 || init[opb] == 0 ) goto NotInit;
        ct = NULL;
        if( at->tid == DAO_LIST ){
          if( code != DVM_BITLFT && code != DVM_BITRIT ) goto InvOper;
          ct = at;
          at = at->nested->items.pAbtp[0];
          if( DaoType_MatchTo( bt, at, defs )==0 ) goto NotMatch;
          if( at->tid == DAO_UDF && bt->tid != DAO_UDF ){
            at = DaoNameSpace_MakeType( ns, "list", DAO_LIST, NULL, & bt, at!=NULL );
            type[opa] = at;
          }
        }else if( at->tid == DAO_ANY || bt->tid == DAO_ANY
            || at->tid == DAO_UDF || bt->tid == DAO_UDF
            || at->tid == DAO_OBJECT || bt->tid == DAO_OBJECT ){
          ct = any;
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
        if( type[opc] == NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        /* allow less strict typing: */
        if( ct->tid ==DAO_UDF || ct->tid == DAO_ANY ) continue;
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
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
        init[opc] = 1;
        if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
        if( type[opa] ==NULL || type[opb] ==NULL ) goto ErrorTyping;
        if( init[opa] ==0 || init[opb] ==0 ) goto NotInit;
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = inumt;
        if( DaoType_MatchTo( inumt, type[opc], defs )==0 ) goto NotMatch;
        break;
      }
    case DVM_PAIR :
      {
        init[opc] = 1;
        if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
        if( type[opa] ==NULL || type[opb] ==NULL ) goto ErrorTyping;
        if( init[opa] ==0 || init[opb] ==0 ) goto NotInit;
        ts[0] = type[opa];
        ts[1] = type[opb];
        if( csts[opa].sub == DAO_PARNAME ){
          csts[opc] = csts[opa];
          ct = DaoNameSpace_MakeType( ns, csts[opa].v.s->mbs,
              DAO_PAR_NAMED, (DaoBase*) type[opb], 0, 0 );
        }else{
          ct = DaoNameSpace_MakeType( ns, "pair", DAO_PAIR, NULL, ts, 2 );
        }
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
        break;
      }
    case DVM_TUPLE :
      {
        init[opc] = 1;
        if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
        if( type[opa] ==NULL ) goto ErrorTyping;
        if( init[opa] ==0 ) goto NotInit;
        ct = DaoType_New( "tuple<", DAO_TUPLE, NULL, NULL );
        for(j=0; j<opb; j++){
          at = type[opa+j];
          val = csts[opa+j];
          if( at == NULL ) goto ErrorTyping;
          if( init[opa+j] ==0 ) goto NotInit;
          if( j >0 ) DString_AppendMBS( ct->name, "," );
          if( at->tid == DAO_PAR_NAMED ){
            if( ct->mapNames == NULL ) ct->mapNames = DMap_New(D_STRING,0);
            MAP_Insert( ct->mapNames, at->fname, j );
            DString_Append( ct->name, at->fname );
            DString_AppendMBS( ct->name, ":" );
            at = at->X.abtype;
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
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
        break;
      }
    case DVM_LIST : case DVM_ARRAY :
      {
        init[opc] = 1;
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
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
        break;
      }
    case DVM_MAP :
    case DVM_HASH :
      {
        init[opc] = 1;
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
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
        break;
      }
    case DVM_MATRIX :
      {
        init[opc] = 1;
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
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
        break;
      }
    case DVM_CURRY :
    case DVM_MCURRY :
      {
        init[opc] = 1;
        if( init[opa] ==0 ) goto NotInit;
        if( type[opa] ==0 ) goto ErrorTyping;
        at = type[opa];
        ct = NULL;
        if( at->tid == DAO_TYPE ) at = at->nested->items.pAbtp[0];
        if( at->tid == DAO_PAIR && at->nested->size >1 && code != DVM_MCURRY )
          at = at->nested->items.pAbtp[1];
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
            if( init[opa+j] ==0 ) goto NotInit;
            if( bt->tid == DAO_PAR_NAMED ){
              if( at->mapNames == NULL ) goto InvField;
              node = MAP_Find( at->mapNames, bt->fname );
              if( node == NULL || node->value.pInt != j-1 ) goto InvField;
              bt = bt->X.abtype;
            }
            tt = at->nested->items.pAbtp[j-1];
            if( tt->tid == DAO_PAR_NAMED ) tt = tt->X.abtype;
            k = DaoType_MatchTo( bt, tt, defs );
            if( k == 0 ) goto NotMatch;
          }
        }else{
          ct = any;
        }
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        if( at->tid == DAO_ANY || at->tid == DAO_UDF ) break;
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
        break;
      }
    case DVM_ASSERT :
      init[opc] = 1;
      ct = inumt;
      if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
      if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
      break;
    case DVM_ITER :
      {
        init[opc] = 1;
        if( init[opa] ==0 ) goto NotInit;
        if( type[opa] ==0 ) goto ErrorTyping;
        ct = dao_type_for_iterator;
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
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
      init[opc] = 1;
      if( type[opb] == NULL ) goto ErrorTyping;
      if( init[opb] ==0 ) goto NotInit;
      ct = type[opb];
      if( ct->tid > DAO_COMPLEX && ct->tid != DAO_ANY ) goto InvParam;
      if( opa == DVM_MATH_ARG || opa == DVM_MATH_IMAG || opa == DVM_MATH_NORM || opa == DVM_MATH_REAL ){
        ct = dnumt;
      }else if( opa == DVM_MATH_RAND ){
        /* ct = type[opb]; return the same type as parameter */
      }else if( ct->tid >= DAO_INTEGER && ct->tid <= DAO_FLOAT ){ /* XXX long type */
        ct = dnumt;
      }
      if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
      if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
      break;
    case DVM_FUNCT :
      init[opc] = 1;
      if( type[opb] == NULL ) goto ErrorTyping;
      if( init[opb] ==0 ) goto NotInit;
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
        k = bt->nested->items.pAbtp[0]->tid;
        for( j=1; j<bt->nested->size; j++ )
          if( k != bt->nested->items.pAbtp[j]->tid ) goto ErrorTyping;
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
          if( bt->nested->size >1 && bt->nested->items.pAbtp[1]->tid != DAO_INTEGER )
            goto ErrorTyping;
          bt = bt->nested->items.pAbtp[0];
        }
        ct = bt;
        break;
      case DVM_FUNCT_SELECT :
        if( bt->tid == DAO_TUPLE ){
          if( bt->nested->size ==0 ) goto ErrorTyping;
          at = DaoType_New( "tuple<", DAO_TUPLE, NULL, NULL );
          at->nested = DArray_New(0);
          for( j=0; j<bt->nested->size; j++ ){
            ct = bt->nested->items.pAbtp[j]->nested->items.pAbtp[0];
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
        if( bt->tid == DAO_ARRAY ){
          if( bt->nested->size != 1 ) goto ErrorTyping;
          at = type[ vmcs[i-2]->c ];
          bt = bt->nested->items.pAbtp[0];
          if( DaoType_MatchTo( at, bt, defs )==0 ) goto NotMatch;
        }
        break;
      case DVM_FUNCT_INDEX : ct = ilst; break;
      case DVM_FUNCT_COUNT : ct = inumt; break;
      case DVM_FUNCT_STRING : ct = strt; break;
      default : break;
      }
      if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
      if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
      break;
    case DVM_CALL : case DVM_MCALL :
      {
        int ctchecked = 0;
        init[opc] = 1;
        if( type[opa] == NULL ) goto ErrorTyping;
        if( init[opa] ==0 ) goto NotInit;
        j = type[opa+1] ? type[opa+1]->tid : 0;
        if( code == DVM_MCALL && j >= DAO_ARRAY && j != DAO_ANY ){
          DaoVmCodeX *p = vmcs[i+1];
          if( p->code == DVM_MOVE && p->a == opa+1 ){
            p->code = DVM_NOP;
            if( i+2 < N ){
              p = vmcs[i+2];
              if( p->code >= DVM_SETV && p->code <= DVM_SETF && p->a == opa+1 )
                p->code = DVM_NOP;
            }
          }
        }
        at = type[opa];
        bt = ct = NULL;
        if( at->tid == DAO_PAIR ){
          if( at->nested->size > 1 ){
            bt = at->nested->items.pAbtp[0];
            at = at->nested->items.pAbtp[1];
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
        }else if( at->tid == DAO_ANY || at->tid == DAO_FUNCURRY ){
          if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = any;
          if( DaoType_MatchTo( any, type[opc], defs )==0 ) goto ErrorTyping;
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
        if( rout == NULL ){
          if( DRoutine_CheckType( at, ns, bt, pp, tp, j, code, 0, &min, &norm, &spec ) ==0 ){
            goto ErrorTyping;
          }
          DRoutine_CheckType( at, ns, bt, pp, tp, j, code, 1, &min, &norm, &spec );
          ct = type[opa];
        }else{
          if( rout->type != DAO_ROUTINE && rout->type != DAO_FUNCTION )
            goto ErrorTyping;
          error = rout->routName;
          rout2 = rout;
          rout = DRoutine_GetOverLoadByParamType( rout, bt,
              pp, tp, j, code, &min, &norm, &spec, &worst );
          if( rout ==NULL ) goto InvParam;
#if 0
          //XXX if( rout != rout2 ) type[opa] = rout->routType;
#endif
          /*
             printf( "CALL: rout = %s %s %i %i %i %i %i\n", rout->routName->mbs, rout->routType->name->mbs, min, DAO_MT_INIT, DAO_MT_UDF, spec, worst );
           */

          /* if the minimum match is less than DAO_MT_SUB, take the
           * worst compatible routine.
           *
           * consider the following example:
           * 1: A = { "abc" => "123" };
           * 2: I = A.has( "xyz" );
           * 3: B = {=>};
           * 4: I = B.has( "rst" );
           * 5: B[ "bar" ] = { 123 };
           *
           * 1: A = map<string,string>;
           * 2: add specialized method:
           *    routine<self:map<string,string>,string=>int>
           * 4: the best compatible method is the above one:
           *    if this one is taken, B will be forced to take type
           *    map<string,string>, which would induce an error in
           *    line 5!
           */
          if( min < DAO_MT_ANY && worst >=0 ){
            rout = (DRoutine*) rout->firstRoutine->routOverLoad->items.pBase[worst];
            DRoutine_CheckType( rout->routType, ns, bt, pp, tp, j, code,1,&min, &norm, &spec );
          }
          if( ( spec && rout->parCount != DAO_MAX_PARAM )
              || rout->routType->X.abtype ==NULL
              /*to infer returned type*/ ){
            if( rout->type == DAO_ROUTINE && ((DaoRoutine*)rout)->parser )
              DaoRoutine_Compile( (DaoRoutine*) rout );
            if( rout->type == DAO_ROUTINE && rout != (DRoutine*) self
                && ((DaoRoutine*)rout)->vmCodes->size >0 && notide ){
              /* rout may has only been declared */
              rout = (DRoutine*) DaoRoutine_Copy( (DaoRoutine*) rout, 1 );
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
            rout = DRoutine_GetOverLoadByParamType( rout, bt, pp, tp, j, code, &min, &norm, &spec, &worst );
          }else if( bt && ( DString_FindChar( bt->name, '?', 0 ) !=MAXSIZE || DString_FindChar( bt->name, '@', 0 ) !=MAXSIZE ) ){
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
          if( ! ctchecked ) ct = rout->routType;
          if( min == DAO_MT_EQ && norm && (opb & 0xff00) ==0
              && rout->parCount != DAO_MAX_PARAM
              && type[opa]->tid == DAO_ROUTINE ){
            /* XXX not for call with self parameter!!! */
#if 0
            /*
             * Breaks typing system for overloaed function! XXX
             if( rout->type == DAO_FUNCTION ){
             vmc->code = DVM_CALL_CF + code - DVM_CALL;
             DRoutine_AddConstValue( self, csts[opa] );
             addCount[i] ++;
             vmc2.code = DVM_GETCL;
             vmc2.a = self->routConsts->size - 1;
             vmc2.b = 0;
             vmc2.c = opa;
             DaoVmcArray_Append( addCode, vmc2 );
             }
             */
#endif
          }
          /*
             printf( "ct2 = %s\n", ct ? ct->name->mbs : "" );
           */
        }
        if( ! ctchecked && ! (opb & DAO_CALL_COROUT) ){
          ct = ct->X.abtype;
          if( vmc->code == DVM_CALL_CF && opb ==1
              && type[opa+1]->tid == DAO_DOUBLE
              && ct && ct->tid == DAO_DOUBLE ){
#if 0
            /*
               vmc->code = DVM_CALL_CMF;
               DRoutine_AddConstValue( self, csts[opa] );
               addCount[i] ++;
               vmc2.code = DVM_GETCL;
               vmc2.a = self->routConsts->size - 1;
               vmc2.b = 0;
               vmc2.c = opa;
               DaoVmcArray_Append( addCode, vmc2 );
             */
#endif
          }
        }
        if( opb & DAO_CALL_ASYNC ){
          ct = DaoNameSpace_MakeType( ns, "FutureValue", DAO_OBJECT, (DaoBase*) daoClassFutureValue, NULL, 0 );
        }
        if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
        if( ct == NULL ) ct = DaoNameSpace_GetType( ns, & nil );
        /*
           printf( "ct = %s\n", type[opc]->name->mbs );
           printf( "%p  %i\n", type[opc], type[opc] ? type[opc]->tid : 1000 );
         */
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto ErrorTyping;

        if( ! typed_code ) break;
        at = type[opa];
        if( at->tid != DAO_ROUTINE && at->tid != DAO_FUNCTION ) break;
        if( rout ) at = rout->routType;
        if( rout && (rout->attribs & DAO_ROUT_VIRTUAL) ) break;
        if( rout && (rout->tidHost == DAO_INTERFACE) ) break;
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
          DaoType *p = at->nested->items.pAbtp[j-k];
          if( t && (t->tid == 0 || t->tid == DAO_ANY ) ) break;
          if( p && (p->tid == DAO_PAR_GROUP || p->tid == DAO_PAR_VALIST) ) break;
        }
        if( j < vmc->b ) break;
        if( rout ){
          type[opa] = rout->routType;
          csts[opa].v.routine = (DaoRoutine*) rout;
        }
        vmc->code += DVM_CALL_TC - DVM_CALL;
        break;
      }
    case DVM_CLOSE :
      {
        init[opc] = 1;
        if( type[opa] == NULL ) goto ErrorTyping;
        if( init[opa] ==0 ) goto NotInit;
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
          tparray->items.pAbtp[loc] = type[opa+j+1];
        }
#endif
        at = DaoNameSpace_MakeRoutType( ns, at, NULL, tparray->items.pAbtp, NULL );
        DArray_Delete( tparray );

        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF )
          type[opc] = at;
        if( DaoType_MatchTo( at, type[opc], defs )==0 ) goto ErrorTyping;
        csts[opc] = csts[opa];
        break;
      }
    case DVM_RETURN :
    case DVM_YIELD :
      {
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
              /* RC increased in DaoNameSpace_MakeRoutType() */
              self->routType = DaoNameSpace_MakeRoutType( ns,
                  self->routType, NULL, NULL, at );
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
            self->routType = DaoNameSpace_MakeRoutType( ns,
                self->routType, NULL, NULL, at );
          }
        }
        if( code == DVM_YIELD ){
          init[opc] = 1;
          tt = self->routType;
          if( tt->nested->size ==1 ){
            ct = tt->nested->items.pAbtp[0];
            if( ct->tid == DAO_PAR_NAMED || ct->tid == DAO_PAR_DEFAULT )
              ct = ct->X.abtype;
          }else if( tt->nested->size ){
            ct = DaoNameSpace_MakeType(ns, "tuple", DAO_TUPLE, NULL,
                tt->nested->items.pAbtp, tt->nested->size );
          }else{
            ct = udf;
          }
          if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
          if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto ErrorTyping;
          if( DaoType_MatchTo( at, tt->X.abtype, defs )==0 ) goto ErrorTyping;
        }
        break;
      }
#define USE_TYPED_OPCODE 1
#if USE_TYPED_OPCODE
    case DVM_CALL_CF :
    case DVM_CALL_CMF :
      init[opc] = 1;
      if( type[opa] == NULL ) goto ErrorTyping;
      if( init[opa] ==0 ) goto NotInit;
      pp = csts+opa+1;
      tp = type+opa+1;
      j = vmc->b & 0xff;
      rout = (DRoutine*) csts[opa].v.routine;
      rout = DRoutine_GetOverLoadByParamType( rout, NULL,
          pp, tp, j, code, &min, &norm, &spec, &worst );
      if( rout ==NULL ) goto InvParam;
      if( min != DAO_MT_EQ || norm ==0 || (opb & 0xff00) !=0
          || rout->parCount == DAO_MAX_PARAM )
        goto ErrorTyping;
      if( rout->type != DAO_FUNCTION ) goto ErrorTyping;
      csts[opa].v.p = (DaoBase*) rout;
      csts[opa].t = rout->type;
      ct = rout->routType;
      ct = ct->X.abtype;
      if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
      if( ct == NULL ) ct = DaoNameSpace_GetType( ns, & nil );
      if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
      if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto ErrorTyping;
      break;
    case DVM_CALL_TC :
    case DVM_MCALL_TC :
      init[opc] = 1;
      if( type[opa] == NULL ) goto ErrorTyping;
      if( init[opa] ==0 ) goto NotInit;
      at = type[opa];
      pp = csts+opa+1;
      tp = type+opa+1;
      j = vmc->b;
      if( vmc->b > 0xff ) goto ErrorTyping;
      rout = (DRoutine*) csts[opa].v.routine;
      if( code == DVM_CALL_TC && self->tidHost == DAO_OBJECT ) bt = hostClass->objType;
      if( rout ){
        rout = DRoutine_GetOverLoadByParamType( rout, NULL,
            pp, tp, j, code, &min, &norm, &spec, &worst );
        if( rout ==NULL ) goto InvParam;
        csts[opa].v.routine = (DaoRoutine*) rout;
        type[opa] = rout->routType;
        ct = rout->routType->X.abtype;
      }else if( at->tid == DAO_ROUTINE ){
        j = DRoutine_CheckType( at, ns, NULL, pp, tp,
            vmc->b, code,1,&min, &norm, &spec );
        if( j <=0 ) goto ErrorTyping;
        ct = at->X.abtype;
      }else{
        goto ErrorTyping;
      }
      if( type[opc] && type[opc]->tid == DAO_ANY ) continue;
      if( ct == NULL ) ct = DaoNameSpace_GetType( ns, & nil );
      if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
      if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto ErrorTyping;
      break;
    case DVM_GETC_I : case DVM_GETC_F : case DVM_GETC_D :
      {
        val = empty;
        if( cstValues[opa] ) val = cstValues[opa][opb];
        at = DaoNameSpace_GetTypeV( ns, val );
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = at;
        if( at->tid != TT3 || type[opc]->tid != TT3 ) goto NotMatch;
        csts[opc] = val;
        init[opc] = 1;
        break;
      }
    case DVM_GETV_I : case DVM_GETV_F : case DVM_GETV_D :
      {
        at = 0;
        if( opa != DAO_G && self->tidHost != DAO_OBJECT ) goto ErrorTyping;
        if( varTypes[opa] ) at = varTypes[opa][opb];
        if( at == NULL ) at = any;
        if( type[opc]==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = at;
        /*
           printf( "%p %p\n", at, type[opc] );
           printf( "%s %s\n", at->name->mbs, type[opc]->name->mbs );
         */
        if( at->tid != TT3 || type[opc]->tid != TT3 ) goto NotMatch;
        init[opc] = 1;
        break;
      }
    case DVM_SETV_II : case DVM_SETV_IF : case DVM_SETV_ID :
    case DVM_SETV_FI : case DVM_SETV_FF : case DVM_SETV_FD :
    case DVM_SETV_DI : case DVM_SETV_DF : case DVM_SETV_DD :
      {
        tp = NULL;
        if( opc != DAO_G && self->tidHost != DAO_OBJECT ) goto ErrorTyping;
        if( varTypes[opc] ) tp = varTypes[opc] + opb;
        if( *tp==NULL || (*tp)->tid ==DAO_UDF ) *tp = type[opa];
        /* less strict checking */
        if( type[opa]->tid == DAO_ANY || type[opa]->tid == DAO_UDF ) break;
        /*
           printf( "%s %s\n", (*tp)->name->mbs, type[opa]->name->mbs );
         */
        if( DaoType_MatchTo( type[opa], *tp, defs )==0 ) goto NotMatch;
        if( tp[0]->tid != TT3 || at->tid != TT1 ) goto NotMatch;
        break;
      }
    case DVM_MOVE_II : case DVM_NOT_I : case DVM_UNMS_I : case DVM_BITREV_I :
    case DVM_MOVE_FF : case DVM_NOT_F : case DVM_UNMS_F : case DVM_BITREV_F :
    case DVM_MOVE_DD : case DVM_NOT_D : case DVM_UNMS_D : case DVM_BITREV_D :
    case DVM_MOVE_IF : case DVM_MOVE_FI :
    case DVM_MOVE_ID : case DVM_MOVE_FD :
    case DVM_MOVE_DI : case DVM_MOVE_DF :
    case DVM_MOVE_CC : case DVM_UNMS_C :
    case DVM_MOVE_SS :
      if( init[opa] ==0 ) goto NotInit;
      if( at ==NULL || at->tid != TT1 ) goto NotMatch;
      if( ct ==NULL || ct->tid ==DAO_UDF ) type[opc] = at;
      if( type[opc]->tid != TT3 ) goto NotMatch;
      init[opc] = 1;
      break;
    case DVM_MOVE_PP :
      if( init[opa] ==0 ) goto NotInit;
      if( at ==NULL || at->tid < DAO_ARRAY || at->tid >= DAO_ANY )
        goto NotMatch;
      if( ct ==NULL || ct->tid ==DAO_UDF ) type[opc] = at;
      if( type[opc]->tid != at->tid ) goto NotMatch;
      init[opc] = 1;
      if( opb ) csts[opc] = csts[opa];
      break;
    case DVM_ADD_III : case DVM_SUB_III : case DVM_MUL_III : case DVM_DIV_III :
    case DVM_MOD_III : case DVM_POW_III : case DVM_AND_III : case DVM_OR_III  :
    case DVM_LT_III  : case DVM_LE_III  : case DVM_EQ_III :
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
        if( init[opa] ==0 || init[opb] ==0 ) goto NotInit;
        if( at ==NULL || at->tid != TT1 ) goto NotMatch;
        if( bt ==NULL || bt->tid != TT2 ) goto NotMatch;
        if( ct ==NULL || ct->tid ==DAO_UDF ) type[opc] = simtps[TT3];
        if( type[opc]->tid != TT3 ) goto NotMatch;
        init[opc] = 1;
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
      if( init[opa] ==0 || init[opb] ==0 ) goto NotInit;
      if( at ==NULL || at->tid ==0 || at->tid > DAO_DOUBLE ) goto NotMatch;
      if( at ==NULL || bt->tid ==0 || bt->tid > DAO_DOUBLE ) goto NotMatch;
      if( ct ==NULL || ct->tid ==DAO_UDF ) type[opc] = simtps[TT3];
      if( type[opc]->tid != TT3 ) goto NotMatch;
      init[opc] = 1;
      break;
    case DVM_GETI_SI :
      {
        if( init[opa] ==0 || init[opb] ==0 ) goto NotInit;
        if( type[opa] ==NULL || type[opb] ==NULL ) goto NotMatch;
        if( at->tid != DAO_STRING ) goto NotMatch;
        if( code == DVM_GETI_SI && bt->tid != DAO_INTEGER ) goto NotMatch;
        if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = inumt;
        if( type[opc]->tid != DAO_INTEGER ) goto NotMatch;
        init[opc] = 1;
        break;
      }
    case DVM_SETI_SII :
      {
        if( csts[opc].cst ) goto ModifyConstant;
        if( init[opa] ==0 || init[opb] ==0 || init[opc] ==0 ) goto NotInit;
        if( type[opa] ==NULL || type[opb] ==NULL || type[opc] ==NULL ) goto NotMatch;
        if( ct->tid != DAO_STRING ) goto NotMatch;
        if( at->tid != DAO_INTEGER || bt->tid != DAO_INTEGER ) goto NotMatch;
        break;
      }
    case DVM_GETI_LI :
      {
        init[opc] = 1;
        if( init[opa] ==0 || init[opb] ==0 ) goto NotInit;
        if( type[opa] ==NULL || type[opb] ==NULL ) goto NotMatch;
        if( type[opa]->tid != DAO_LIST ) goto NotMatch;
        bt = type[opb];
        if( bt->tid != DAO_INTEGER ) goto NotMatch;
        at = type[opa]->nested->items.pAbtp[0];
        if( at->tid < DAO_ARRAY || at->tid >= DAO_ANY ) goto NotMatch;
        if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = at;
        if( DaoType_MatchTo( at, type[opc], defs )==0 ) goto NotMatch;
        break;
      }
    case DVM_GETI_LII : case DVM_GETI_LFI : case DVM_GETI_LDI :
    case DVM_GETI_AII : case DVM_GETI_AFI : case DVM_GETI_ADI :
    case DVM_GETI_LSI :
      {
        init[opc] = 1;
        if( init[opa] ==0 || init[opb] ==0 ) goto NotInit;
        if( at ==NULL || at->tid != TT6 || at->nested->size ==0 ) goto NotMatch;
        at = at->nested->items.pAbtp[0];
        if( at ==NULL || at->tid != TT1 ) goto NotMatch;
        if( bt ==NULL || bt->tid != TT2 ) goto NotMatch;
        if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = at;
        if( type[opc]->tid != TT3 ) goto NotMatch;
        break;
      }
    case DVM_SETI_LI :
      {
        if( csts[opc].cst ) goto ModifyConstant;
        if( init[opa] ==0 || init[opb] ==0 || init[opc] ==0 ) goto NotInit;
        if( type[opa] ==NULL || type[opb] ==NULL || type[opc] ==NULL ) goto NotMatch;
        bt = type[opb];
        if( bt->tid != DAO_INTEGER ) goto NotMatch;
        if( type[opc]->tid != DAO_LIST ) goto NotMatch;
        if( at->tid < DAO_ARRAY || at->tid >= DAO_ANY ) goto NotMatch;
        ct = type[opc]->nested->items.pAbtp[0];
        if( ct->tid < DAO_ARRAY || ct->tid >= DAO_ANY ) goto NotMatch;
        if( DaoType_MatchTo( type[opa], ct, defs )==0 ) goto NotMatch;
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
        if( init[opa] ==0 || init[opb] ==0 || init[opc] ==0 ) goto NotInit;
        if( at ==NULL || bt ==NULL || ct ==NULL ) goto NotMatch;
        if( ct->tid != TT6 || bt->tid != TT2 || at->tid != TT1 ) goto NotMatch;
        if( ct->nested->size !=1 || ct->nested->items.pAbtp[0]->tid != TT3 ) goto NotMatch;
        break;
      }
    case DVM_GETI_TI :
      {
        init[opc] = 1;
        if( init[opa] ==0 || init[opb] ==0 ) goto NotInit;
        if( at ==NULL || at->tid != TT6 ) goto NotMatch;
        if( bt ==NULL || bt->tid != TT2 ) goto NotMatch;
        if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = any;
        break;
      }
    case DVM_SETI_TI :
      {
        if( csts[opc].cst ) goto ModifyConstant;
        if( init[opa] ==0 || init[opb] ==0 || init[opc] ==0 ) goto NotInit;
        if( ct ==NULL || ct->tid != TT6 ) goto NotMatch;
        if( bt ==NULL || bt->tid != TT2 ) goto NotMatch;
        break;
      }
    case DVM_SETF_T :
      {
        if( csts[opc].cst ) goto ModifyConstant;
        if( init[opa] ==0 || init[opc] ==0 ) goto NotInit;
        if( at ==NULL || ct ==NULL || ct->tid != TT6 ) goto NotMatch;
        if( opb >= ct->nested->size ) goto InvIndex;
        tt = ct->nested->items.pAbtp[opb];
        if( tt->tid == DAO_PAR_NAMED ) tt = tt->X.abtype;
        if( DaoType_MatchTo( at, tt, defs ) ==0 ) goto NotMatch;
        break;
      }
    case DVM_GETF_T :
    case DVM_GETF_TI : case DVM_GETF_TF :
    case DVM_GETF_TD : case DVM_GETF_TS :
      {
        init[opc] = 1;
        if( init[opa] ==0 ) goto NotInit;
        if( at ==NULL || at->tid != TT6 ) goto NotMatch;
        if( opb >= at->nested->size ) goto InvIndex;
        ct = at->nested->items.pAbtp[opb];
        if( ct->tid == DAO_PAR_NAMED ) ct = ct->X.abtype;
        if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        if( TT3 >0 ){
          if( ct ==NULL || ct->tid != TT3 ) goto NotMatch;
          if( type[opc]->tid != TT3 ) goto NotMatch;
        }else{
          if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
        }
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
        tt = ct->nested->items.pAbtp[opb];
        if( tt->tid == DAO_PAR_NAMED ) tt = tt->X.abtype;
        if( tt->tid != TT3 ) goto NotMatch;
        break;
      }
    case DVM_GETI_ACI :
      {
        if( init[opa] ==0 || init[opb] ==0 ) goto NotInit;
        if( type[opa] ==NULL || type[opb] ==NULL ) goto NotMatch;
        if( DaoType_MatchTo( type[opa], cart, defs )==0 ) goto NotMatch;
        bt = type[opb];
        if( bt->tid != DAO_INTEGER ) goto NotMatch;
        if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = comt;
        if( type[opc]->tid != DAO_COMPLEX ) goto NotMatch;
        init[opc] = 1;
        break;
      }
    case DVM_SETI_ACI :
      {
        if( csts[opc].cst ) goto ModifyConstant;
        if( init[opa] ==0 || init[opb] ==0 || init[opc] ==0 ) goto NotInit;
        if( type[opa] ==NULL || type[opb] ==NULL || type[opc] ==NULL ) goto NotMatch;
        if( type[opa]->tid != DAO_COMPLEX ) goto NotMatch;
        if( type[opb]->tid != DAO_INTEGER ) goto NotMatch;
        if( DaoType_MatchTo( type[opc], cart, defs )==0 ) goto NotMatch;
        break;
      }
    case DVM_GETF_M :
      {
        init[opc] = 1;
        if( init[opa] ==0 || type[opa] == NULL ) goto NotInit;
        at = type[opa];
        if( at->typer ==NULL ) goto NotMatch;
#if 0
        ct = at->typer->priv->methods[opb]->routType; /*XXX*/
#endif
        /* XXX csts[opc] = (DaoBase*) at->typer->priv->methods[opb]; */
        if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        if( DaoType_MatchTo( type[opc], ct, defs )==0 ) goto NotMatch;
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
        init[opc] = 1;
        if( init[opa] ==0 || type[opa] == NULL ) goto NotInit;
        at = type[opa];
        klass = (DaoClass*) at->X.extra;
        ct = NULL;
        if( at->tid != TT6 ) goto NotMatch;
        switch( TT4 ){
        case 'C' :
          ct = DaoNameSpace_GetTypeV( ns, klass->cstData->data[ opb ] );
          break;
        case 'G' : ct = klass->glbDataType->items.pAbtp[ opb ]; break;
        case 'V' : ct = klass->objDataType->items.pAbtp[ opb ]; break;
        default : goto NotMatch;
        }

        if( type[opc] ==NULL || type[opc]->tid ==DAO_UDF ) type[opc] = ct;
        if( DaoType_MatchTo( ct, type[opc], defs )==0 ) goto NotMatch;
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
        case 'G' : bt = klass->glbDataType->items.pAbtp[ opb ]; break;
        case 'V' : bt = klass->objDataType->items.pAbtp[ opb ]; break;
        default : goto NotMatch;
        }
        if( DaoType_MatchTo( at, bt, defs )==0 ) goto NotMatch;
        if( TT1 > 0 && at->tid != TT1 ) goto NotMatch;
        if( TT3 > 0 && bt->tid != TT3 ) goto NotMatch;
      }
#endif
    default : break;
    }
  }
  GC_IncRC( self->regType->items.pBase[self->regType->size-1] );
  GC_DecRC( self->regType->items.pBase[self->regType->size-1] );
  DArray_PopBack( self->regType );
  for(i=0; i<addRegType->size; i++)
    DArray_Append( self->regType, addRegType->items.pVoid[i] );
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
        || c == DVM_ASSERT || ( c >=DVM_TEST_I && c <=DVM_TEST_D ) ){
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
  DArray_Delete( vmCodeNew );
  DArray_Delete( addCode );
  DArray_Append( self->regType, any );
  GC_IncRCs( self->regType );
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

NotMatch :     ec = DT_NOT_MATCH;      goto ErrorTyping;
NotInit :      ec = DT_NOT_INIT;       goto ErrorTyping;
NotPermit :    ec = DT_NOT_PERMIT;     goto ErrorTyping;
NotExist :     ec = DT_NOT_EXIST;      goto ErrorTyping;
NeedInstVar :  ec = DT_NEED_INSTVAR;   goto ErrorTyping;
InvIndex :     ec = DT_INV_INDEX;      goto ErrorTyping;
InvKey :       ec = DT_INV_KEY;        goto ErrorTyping;
InvField :     ec = DT_INV_KEY;        goto ErrorTyping;
InvOper :      ec = DT_INV_OPER;       goto ErrorTyping;
InvParam :     ec = DT_INV_PARAM;      goto ErrorTyping;
ModifyConstant: ec = DT_MODIFY_CONST; goto ErrorTyping;
#if 0
//FunctionNotImplemented: ec = DT_NOT_IMPLEMENTED; goto ErrorTyping;
#endif

ErrorTyping:
#if 0
  //for(i=0; i<N; i++) DaoVmCode_Print( vmcs[i], NULL );
#endif
  stdio = ns->vmSpace->stdStream;
  if( stdio ==NULL ) stdio = DaoStream_New();
  DaoStream_WriteMBS( stdio, "ERROR( " );
  DaoStream_WriteString( stdio, self->nameSpace->name );
  DaoStream_WriteMBS( stdio, " : " );
  DaoStream_WriteInt( stdio, self->annotCodes->items.pVmc[cid]->line );
  DaoStream_WriteMBS( stdio, " ): " );
  if( error ){
    DaoStream_WriteString( stdio, error );
    DaoStream_WriteMBS( stdio, ", " );
  }
  DaoStream_WriteMBS( stdio, DaoTypingErrorString[ec] );
  DaoStream_WriteMBS( stdio, ", for instruction:\n" );
  init = dao_realloc( init, 200*sizeof(char) );
  DaoVmCodeX_Print( *vmc, init );
  DaoStream_WriteMBS( stdio, init );
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


static const char *const sep1 = "==========================================\n";
static const char *const sep2 =
"-------------------------------------------------------------------------\n";

void DaoRoutine_PrintCode( DaoRoutine *self, DaoStream *stream )
{
  DaoVmCodeX **vmCodes = self->annotCodes->items.pVmc;
  char buffer[100];
  int j;

  DaoStream_WriteMBS( stream, sep1 );
  DaoStream_WriteMBS( stream, "routine " );
  DaoStream_WriteString( stream, self->routName );
  DaoStream_WriteMBS( stream, "():\n" );
  DaoStream_WriteMBS( stream, "Number of register:\n" );
  DaoStream_WriteInt( stream, (double)self->locRegCount );
  DaoStream_WriteMBS( stream, "\n" );
  DaoStream_WriteMBS( stream, sep1 );
  DaoStream_WriteMBS( stream, "Virtual Machine Code:\n\n" );
  DaoStream_WriteMBS( stream,
      "   ID  :    OPCODE    :      A ,      B ,      C ;  [ LINE ],  NOTES\n" );

  DaoStream_WriteMBS( stream, sep2 );
  for( j=0; j<self->annotCodes->size; j++){
    sprintf( buffer, "%5i  :  ", j);
    DaoStream_WriteMBS( stream, buffer );
    DaoVmCodeX_Print( *vmCodes[j], buffer );
    DaoStream_WriteMBS( stream, buffer );
  }
  DaoStream_WriteMBS( stream, sep2 );
}
void DaoFunction_Delete( DaoFunction *self )
{
  DRoutine_DeleteFields( (DRoutine*) self );
  DaoLateDeleter_Push( self );
}

DaoTypeBase funcTyper =
{
  & baseCore,
  "FUNCTION",
  NULL,
  NULL, {0},
  NULL,
  (FuncPtrDel) DaoFunction_Delete
};
DaoFunction* DaoFunction_New()
{
  DaoFunction *self = (DaoFunction*) dao_malloc( sizeof(DaoFunction) );
  DaoBase_Init( self, DAO_FUNCTION );
  DRoutine_Init( (DRoutine*)self );
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
  DaoType *tp, **types = self->routType->nested->items.pAbtp;
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
  & baseCore,
  "curry",
  NULL,
  NULL, {0},
  NULL,
  (FuncPtrDel) DaoFunCurry_Delete
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
