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

#include"assert.h"
#include"string.h"
#include"daoConst.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoRoutine.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoStream.h"
#include"daoNumtype.h"
#include"daoNamespace.h"

static void DaoClass_Print( DValue *self, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
  DaoBase_Print( self, ctx, stream, cycData );
}

static void DaoClass_GetField( DValue *self0, DaoContext *ctx, DString *name )
{
  DaoClass *self = self0->v.klass;
  DString *mbs = DString_New(1);
  DValue *d2 = NULL;
  DValue value = daoNilValue;
  int rc = DaoClass_GetData( self, name, & value, ctx->routine->hostClass, & d2 );
  DaoContext_PutReference( ctx, d2 );
  if( rc ){
    DString_SetMBS( mbs, DString_GetMBS( self->className ) );
    DString_AppendMBS( mbs, "." );
    DString_Append( mbs, name );
    DaoContext_RaiseException( ctx, rc, mbs->mbs );
  }
  DString_Delete( mbs );
}
static void DaoClass_SetField( DValue *self0, DaoContext *ctx, DString *name, DValue value )
{
  DaoClass *self = self0->v.klass;
  DNode *node = DMap_Find( self->lookupTable, name );
  if( node && LOOKUP_ST( node->value.pInt ) == DAO_CLASS_GLOBAL ){
    int id = LOOKUP_ID( node->value.pInt );
    DaoType *tp = self->glbDataType->items.pAbtp[ id ];
    if( DValue_Move( value, & self->glbData->data[id], tp ) ==0 )
      DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "not matched" );
  }else{
    DaoContext_RaiseException( ctx, DAO_ERROR_FIELD, "not exist" );
  }
}
static void DaoClass_GetItem( DValue *self0, DaoContext *ctx, DValue pid )
{
}
static void DaoClass_SetItem( DValue *self0, DaoContext *ctx, DValue pid, DValue value )
{
}
static DValue DaoClass_Copy(  DValue *self, DaoContext *ctx, DMap *cycData )
{
  return *self;
}

static DaoTypeCore classCore=
{
  0,
#ifdef DEV_HASH_LOOKUP
  NULL, NULL,
#endif
  NULL, NULL, NULL, 0, 0,
  DaoClass_GetField,
  DaoClass_SetField,
  DaoClass_GetItem,
  DaoClass_SetItem,
  DaoClass_Print,
  DaoClass_Copy
};

DaoTypeBase classTyper =
{
  & classCore,
  "CLASS",
  NULL,
  NULL,
  {0},
  (FuncPtrNew) DaoClass_New,
  (FuncPtrDel) DaoClass_Delete
};

DaoClass* DaoClass_New()
{
  DaoClass *self = (DaoClass*) dao_malloc( sizeof(DaoClass) );
  DaoBase_Init( self, DAO_CLASS );
  self->classRoutine = NULL;
  self->className = DString_New(1);
  self->docString = DString_New(1);

  self->derived = 0;
  self->attribs = 0;

  self->abstypes = DMap_New(D_STRING,0);
  self->lookupTable  = DHash_New(D_STRING,0);
  self->ovldRoutMap  = DHash_New(D_STRING,0);
  self->cstData      = DVarray_New();
  self->glbData      = DVarray_New();
  self->glbDataType  = DArray_New(0);
  self->objDataType  = DArray_New(0);
  self->objDataName  = DArray_New(D_STRING);
  self->cstDataName  = DArray_New(D_STRING);
  self->glbDataName  = DArray_New(D_STRING);
  self->superClass   = DArray_New(0);
  self->superAlias   = DArray_New(D_STRING);
  self->objDataDefault  = DVarray_New();
  return self;
}
void DaoClass_Delete( DaoClass *self )
{
  DNode *n = DMap_First( self->abstypes );
  for( ; n != NULL; n = DMap_Next( self->abstypes, n ) ) GC_DecRC( n->value.pBase );
  GC_DecRC( self->clsType );
  GC_DecRCs( self->superClass );
  DMap_Delete( self->abstypes );
  DMap_Delete( self->lookupTable     );
  DMap_Delete( self->ovldRoutMap  );
  DVarray_Delete( self->cstData    );
  DVarray_Delete( self->glbData    );
  DArray_Delete( self->glbDataType );
  DArray_Delete( self->objDataType );
  DArray_Delete( self->objDataName );
  DArray_Delete( self->cstDataName );
  DArray_Delete( self->glbDataName );
  DArray_Delete( self->superClass );
  DArray_Delete( self->superAlias );
  DVarray_Delete( self->objDataDefault );

  DString_Delete( self->className );
  DString_Delete( self->docString );
  dao_free( self );
}
void DaoClass_SetName( DaoClass *self, DString *name, DaoNameSpace *ns )
{
  DaoRoutine *rout = DaoRoutine_New();
  DValue value = daoNilClass;

  DString_Assign( rout->routName, name );
  DString_AppendMBS( rout->routName, "::" );
  DString_Append( rout->routName, name );
  self->classRoutine = rout; /* XXX class<name> */
  DArray_Append( ns->definedRoutines, rout );
  self->clsType = DaoType_New( name->mbs, DAO_CLASS, (DaoBase*) self, NULL );
  GC_IncRC( self->clsType );
  DString_InsertMBS( self->clsType->name, "class<", 0, 0, 0 );
  DString_AppendChar( self->clsType->name, '>' );
  self->objType = DaoType_New( name->mbs, DAO_OBJECT, (DaoBase*)self, NULL );
  DString_SetMBS( self->className, "self" );
  DaoClass_AddObjectVar( self, self->className, daoNilValue, DAO_CLS_PRIVATE, self->objType );
  DString_Assign( self->className, name );
  DaoClass_AddType( self, name, self->objType );

  rout->hostClass = self;
  rout->attribs |= DAO_ROUT_INITOR;
  value.v.klass = self;
  DaoClass_AddConst( self, name, value, DAO_CLS_PUBLIC );
  DString_Assign( rout->routName, name );
  DString_AppendMBS( rout->routName, "::" );
  DString_Append( rout->routName, name );
  value.t = DAO_ROUTINE;
  value.v.routine = rout;
  DaoClass_AddConst( self, rout->routName, value, DAO_CLS_PRIVATE );
}
void DaoClass_DeriveClassData( DaoClass *self )
{
  DaoType *type;
  DNode *search;
  DString *mbs;
  DValue value = daoNilValue;
  int i, j, k, id, perm, index;
  int offset = 0;

  if( self->derived ) return;
  offset = self->objDataName->size;
  assert( offset == 1 );
  mbs = DString_New(1);

  for( i=0; i<self->superClass->size; i++){
    if( self->superClass->items.pBase[i]->type == DAO_CLASS ){
      DaoClass *klass = self->superClass->items.pClass[i];
      DaoClass_DeriveClassData( klass );
      if( DString_EQ( klass->className, self->superAlias->items.pString[i] ) ==0 ){
        value = daoNilClass;
        value.v.klass = klass;
        DaoClass_AddConst( self, self->superAlias->items.pString[i], value, DAO_CLS_PRIVATE );
      }

      /* For class data: */
      for( id=0; id<klass->cstDataName->size; id++ ){
        DString *name = klass->cstDataName->items.pString[id];;
        search = MAP_Find( self->lookupTable, name );
        /* To overide data and routine: */
        if( search == NULL ){
          search = MAP_Find( klass->lookupTable, name );
          perm = LOOKUP_PM( search->value.pInt );
          /* To NOT derive private member: */
          if( perm > DAO_CLS_PRIVATE ){
            value = klass->cstData->data[id];
            if( value.t == DAO_ROUTINE ){
              DValue tmp = value;
              value.v.routine = DaoRoutine_New(); /* a dummy routine */
              value.v.routine->refCount --;
              value.v.routine->routOverLoad->items.pBase[0] = tmp.v.p;
              value.v.routine->routType = tmp.v.routine->routType;
              GC_IncRC( value.v.routine->routType );
              GC_IncRC( value.v.p );
            }
            DaoClass_AddConst( self, name, value, perm );
          }
        }else{
          DValue v1 = self->cstData->data[ LOOKUP_ID( search->value.pInt ) ];
          DValue v2 = klass->cstData->data[ id ];
          if( v1.t == DAO_ROUTINE && v2.t == DAO_ROUTINE ){
            DRoutine *r1 = (DRoutine*) v1.v.p;
            DRoutine *r2 = (DRoutine*) v2.v.p;
            for( k=0; k<r2->routOverLoad->size; k++)
              DRoutine_AddOverLoad( r1, (DRoutine*)r2->routOverLoad->items.pBase[k] );
          }
        }
      }
      /* class global data */
      for( id=0; id<klass->glbDataName->size; id ++ ){
        DString *name = klass->glbDataName->items.pString[id];;
        type = klass->glbDataType->items.pAbtp[id];
        search = MAP_Find( self->lookupTable, name );
        /* To overide data: */
        if( search == NULL ){
          value = klass->glbData->data[id];
          search = MAP_Find( klass->lookupTable, name );
          perm = LOOKUP_PM( search->value.pInt );
          /* To NOT derive private member: */
          if( perm > DAO_CLS_PRIVATE )
            DaoClass_AddGlobalVar( self, name, value, perm, type );
        }
      }
      /* For object data: */
      for( id=0; id<klass->objDataName->size; id ++ ){
        DString *name = klass->objDataName->items.pString[id];;
        type = klass->objDataType->items.pAbtp[id];
        value = klass->objDataDefault->data[id];
        DArray_Append( self->objDataName, name );
        DArray_Append( self->objDataType, type );
        DVarray_Append( self->objDataDefault, daoNilValue );
        DValue_SimpleMove( value, self->objDataDefault->data + self->objDataDefault->size-1 );
        DValue_MarkConst( self->objDataDefault->data + self->objDataDefault->size-1 );
        search = MAP_Find( klass->lookupTable, name );
        perm = LOOKUP_PM( search->value.pInt );
        search = MAP_Find( self->lookupTable, name );
        if( search == NULL ){
          /* To overide data and routine: */
          /* To NOT derive private member: */
          index = ( perm > DAO_CLS_PRIVATE ) ? (offset+id) : -1;
          MAP_Insert( self->lookupTable, name, LOOKUP_BIND( DAO_CLASS_VARIABLE, perm, index ) );
        }
      }
      offset += klass->objDataName->size;
      /*
      DString_SetMBS( mbs, "_" );
      DString_Append( mbs, klass->className );
      DString_AppendMBS( mbs, "_" );
      DaoClass_AddObjectVar( self, mbs, NULL, DAO_CLS_PUBLIC, klass->objType );
      */
    }else if( self->superClass->items.pBase[i]->type == DAO_CDATA ){
      DaoCData *cdata = self->superClass->items.pCData[i];
      DaoTypeCore *core = cdata->typer->priv;
      DaoNamedValue **vals = core->values;
      DaoFunction **meths = core->methods;
      DString_SetMBS( mbs, cdata->typer->name );
      value.t = DAO_CDATA;
      value.v.cdata = cdata;
      DaoClass_AddConst( self, mbs, value, DAO_CLS_PRIVATE );
      if( strcmp( cdata->typer->name, self->superAlias->items.pString[i]->mbs ) ){
        DaoClass_AddConst( self, self->superAlias->items.pString[i], value, DAO_CLS_PRIVATE );
      }
      for(j=0; j<core->valCount; j++){
        search = MAP_Find( self->lookupTable, vals[j]->name );
        if( search ==NULL )
          DaoClass_AddConst( self, vals[j]->name, vals[j]->value, DAO_CLS_PUBLIC );
      }
      value.t = DAO_FUNCTION;
      for(j=0; j<core->methCount; j++){
        search = MAP_Find( self->lookupTable, meths[j]->routName );
        value.v.func = meths[j];
        if( search ==NULL )
          DaoClass_AddConst( self, meths[j]->routName, value, DAO_CLS_PUBLIC );
      }
      /* XXX
      DString_SetMBS( mbs, "_" );
      DString_AppendMBS( mbs, cdata->typer->name );
      DString_AppendMBS( mbs, "_" );
      DaoClass_AddObjectVar( self, mbs, NULL, DAO_CLS_PUBLIC, abtp );
      */
    }
  }
  self->derived = 1;
  DString_Delete( mbs );
}
void DaoClass_ResetAttributes( DaoClass *self )
{
  DString *mbs = DString_New(1);
  DNode *node;
  int i, k, id;
  for(i=DVM_MOVE; i<=DVM_BITRIT; i++){
    DString_SetMBS( mbs, daoBitBoolArithOpers[i-DVM_MOVE] );
    node = DMap_Find( self->lookupTable, mbs );
    if( node == NULL ) continue;
    if( LOOKUP_ST( node->value.pInt ) != DAO_CLASS_CONST ) continue;
    id = LOOKUP_ID( node->value.pInt );
    k = self->cstData->data[id].t;
    if( k != DAO_ROUTINE && k != DAO_FUNCTION ) continue;
    self->attribs |= DAO_OPER_OVERLOADED | (1<<i);
  }
  DString_Delete( mbs );
}
int  DaoClass_FindSuper( DaoClass *self, DaoBase *super )
{
  int i;
  for(i=0; i<self->superClass->size; i++)
    if( super == self->superClass->items.pBase[i] ) return i;
  return -1;
}

int DaoCData_ChildOf( DaoTypeBase *self, DaoTypeBase *super )
{
  int i;
  if( self == super ) return 1;
  for(i=0; i<DAO_MAX_CDATA_SUPER; i++){
    if( self->supers[i] ==NULL ) break;
    if( DaoCData_ChildOf( self->supers[i], super ) ) return 1;
  }
  return 0;
}
int  DaoClass_ChildOf( DaoClass *self, DaoBase *klass )
{
  DaoCData *cdata = (DaoCData*) klass;
  int i;
  if( self == NULL ) return 0;
  if( klass == (DaoBase*) self ) return 1;
  for( i=0; i<self->superClass->size; i++ ){
    if( klass == self->superClass->items.pBase[i] ) return 1;
    if( self->superClass->items.pClass[i]->type == DAO_CLASS 
        && DaoClass_ChildOf( self->superClass->items.pClass[i],  klass ) ){
      return 1;
    }else if( self->superClass->items.pClass[i]->type == DAO_CDATA
        && klass->type == DAO_CDATA ){
      if( DaoCData_ChildOf( self->superClass->items.pCData[i]->typer, cdata->typer ) )
        return 1;
    }
  }
  return 0;
}
void DaoClass_AddSuperClass( DaoClass *self, DaoBase *super, DString *alias )
{
  /* XXX if( alias == NULL ) alias = super->className; */
  DArray_Append( self->superClass, super );
  DArray_Append( self->superAlias, alias );
  GC_IncRC( super );
}
int  DaoClass_FindConst( DaoClass *self, DString *name )
{
  DNode *node = MAP_Find( self->lookupTable, name );
  if( node == NULL || LOOKUP_ST( node->value.pInt ) != DAO_CLASS_CONST ) return -1;
  return LOOKUP_ID( node->value.pInt );
}
void DaoClass_SetConst( DaoClass *self, int id, DValue data )
{
  if( id <0 || id >= self->cstData->size ) return;
  DValue_Copy( self->cstData->data + id, data );
}
int DaoClass_GetData( DaoClass *self, DString *name, DValue *value, DaoClass *thisClass, DValue **d2 )
{
  DValue *p = NULL;
  int sto, perm, id;
  DNode *node = MAP_Find( self->lookupTable, name );
  *value = daoNilValue;
  if( ! node ) return DAO_ERROR_FIELD_NOTEXIST;
  perm = LOOKUP_PM( node->value.pInt );
  sto = LOOKUP_ST( node->value.pInt );
  id = LOOKUP_ID( node->value.pInt );
  if( self == thisClass || perm == DAO_CLS_PUBLIC 
      || ( thisClass && DaoClass_ChildOf( thisClass, (DaoBase*)self ) 
      && perm >= DAO_CLS_PROTECTED ) ){
    switch( sto ){
    case DAO_CLASS_GLOBAL : p = self->glbData->data + id; break;
    case DAO_CLASS_CONST  : p = self->cstData->data + id; break;
    default : return DAO_ERROR_FIELD;
    }
    if( p ) *value = *p;
    if( d2 ) *d2 = p;
  }else{
    return DAO_ERROR_FIELD_NOTPERMIT;
  }
  return 0;
}
DaoType** DaoClass_GetDataType( DaoClass *self, DString *name, int *res, DaoClass *thisClass )
{
  int sto, perm, id;
  DNode *node = MAP_Find( self->lookupTable, name );
  *res = DAO_ERROR_FIELD_NOTEXIST;
  if( ! node ) return NULL;

  *res = 0;
  perm = LOOKUP_PM( node->value.pInt );
  sto = LOOKUP_ST( node->value.pInt );
  id = LOOKUP_ID( node->value.pInt );
  if( self == thisClass || perm == DAO_CLS_PUBLIC
      || ( thisClass && DaoClass_ChildOf( thisClass, (DaoBase*)self ) 
      && perm >=DAO_CLS_PROTECTED ) ){
    switch( sto ){
    case DAO_CLASS_VARIABLE : return self->objDataType->items.pAbtp + id;
    case DAO_CLASS_GLOBAL   : return self->glbDataType->items.pAbtp + id;
    case DAO_CLASS_CONST    : return NULL;
    default : break;
    }
  }
  *res = DAO_ERROR_FIELD_NOTPERMIT;
  return NULL;
}
int DaoClass_GetDataIndex( DaoClass *self, DString *name, int *type )
{
  DNode *node = MAP_Find( self->lookupTable, name );
  if( ! node ) return -1;
  *type = LOOKUP_ST( node->value.pInt );
  return LOOKUP_ID( node->value.pInt );
}
int DaoClass_AddObjectVar( DaoClass *self, DString *name, DValue deft, int s, DaoType *t )
{
  int id;
  DNode *node = MAP_Find( self->lookupTable, name );
  if( node ) return DAO_CTW_WAS_DEFINED;

  id = self->objDataName->size;
  MAP_Insert( self->lookupTable, name, LOOKUP_BIND( DAO_CLASS_VARIABLE, s, id ) );
  DArray_Append( self->objDataType, (void*)t );
  DArray_Append( self->objDataName, (void*)name );
  DVarray_Append( self->objDataDefault, daoNilValue );
  DValue_SimpleMove( deft, self->objDataDefault->data + self->objDataDefault->size-1 );
  DValue_MarkConst( self->objDataDefault->data + self->objDataDefault->size-1 );
  return 0;
}
int DaoClass_AddConst( DaoClass *self, DString *name, DValue data, int s )
{
  DNode *node = MAP_Find( self->lookupTable, name );
  /* if( node ) return DAO_CTW_WAS_DEFINED; */
  /* XXX class methods may be added after DaoClass_DeriveClassData(), */
  /* more careful consideration on overloading and overriding */

  MAP_Insert( self->lookupTable, name, LOOKUP_BIND( DAO_CLASS_CONST, s, self->cstData->size ) );
  DArray_Append( self->cstDataName, (void*)name );
  DVarray_Append( self->cstData, daoNilValue );
  DValue_SimpleMove( data, self->cstData->data + self->cstData->size-1 );
  DValue_MarkConst( & self->cstData->data[self->cstData->size-1] );
  return node ? DAO_CTW_WAS_DEFINED : 0;
}
int DaoClass_AddGlobalVar( DaoClass *self, DString *name, DValue data, int s, DaoType *t )
{
  DNode *node = MAP_Find( self->lookupTable, name );
  if( node ) return DAO_CTW_WAS_DEFINED;
  GC_IncRC( t );
  MAP_Insert( self->lookupTable, name, LOOKUP_BIND( DAO_CLASS_GLOBAL, s, self->glbData->size ) );
  DVarray_Append( self->glbData, daoNilValue );
  DArray_Append( self->glbDataType, (void*)t );
  DArray_Append( self->glbDataName, (void*)name );
  if( data.t && DValue_Move( data, self->glbData->data + self->glbData->size -1, t ) ==0 )
    return DAO_CTW_TYPE_NOMATCH;
  return 0;
}
int DaoClass_AddType( DaoClass *self, DString *name, DaoType *tp )
{
  DNode *node = MAP_Find( self->abstypes, name );
  if( DString_FindChar( name, '?', 0 ) != MAXSIZE
      || DString_FindChar( name, '@', 0 ) != MAXSIZE ) return 0;
  if( node == NULL ){
    MAP_Insert( self->abstypes, name, tp );
    GC_IncRC( tp );
  }
  return 1;
}
void DaoClass_AddOvldRoutine( DaoClass *self, DString *signature, DaoRoutine *rout )
{
  MAP_Insert( self->ovldRoutMap, signature, rout );
}
DaoRoutine* DaoClass_GetOvldRoutine( DaoClass *self, DString *signature )
{
  DNode *node = MAP_Find( self->ovldRoutMap, signature );
  if( node ) return (DaoRoutine*) node->value.pBase;
  return NULL;
}
void DaoClass_PrintCode( DaoClass *self, DaoStream *stream )
{
  DNode *node = DMap_First( self->lookupTable );
  DaoStream_WriteMBS( stream, "class " );
  DaoStream_WriteString( stream, node->key.pString );
  DaoStream_WriteMBS( stream, ":\n" );
  for( ; node != NULL; node = DMap_Next( self->lookupTable, node ) ){
    DValue val;
    if( LOOKUP_ST( node->value.pInt ) != DAO_CLASS_CONST ) continue;
    val = self->cstData->data[ LOOKUP_ID( node->value.pInt ) ];
    if( val.t == DAO_ROUTINE ){
      DaoRoutine *rout = val.v.routine;
      if( rout->hostClass == self ){
        DaoRoutine_Compile( rout );
        DaoRoutine_PrintCode( rout, stream );
      }
    }
  }
}
DaoRoutine* DaoClass_FindOperator( DaoClass *self, const char *oper, DaoClass *scoped )
{
  int len = strlen( oper );
  DValue value = daoNilValue;
  DString name = { 0, 0, NULL, NULL, NULL };
  name.size = name.bufSize = len;
  name.mbs = (char*) oper;
  name.data = (size_t*) oper;
#if 0
  DString *name = DString_New(1);
  DString_SetMBS( name, oper );
  DaoClass_GetData( self, name, & value, scoped, NULL );
  DString_Delete( name );
#else
  DaoClass_GetData( self, & name, & value, scoped, NULL );
#endif
  if( value.t != DAO_ROUTINE ) return NULL;;
  return value.v.routine;
}
