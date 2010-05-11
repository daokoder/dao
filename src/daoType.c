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

#include"daoType.h"
#include"daoVmspace.h"
#include"daoNamespace.h"
#include"daoNumeric.h"
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

DMap *dao_typing_cache; /* HASH<void*[2],int> */
#ifdef DAO_WITH_THREAD
DMutex dao_typing_mutex;
#endif

DaoFunction* DaoFindFunction2( DaoTypeBase *typer, DString *name )
{
  DNode *node;
  if( typer->priv->mapMethods == NULL ) return NULL;
  node = DMap_Find( typer->priv->mapMethods, name );
  if( node ) return (DaoFunction*)node->value.pVoid;
  return NULL;
}
DValue DaoFindValue2( DaoTypeBase *typer, DString *name )
{
  DaoFunction *func = DaoFindFunction2( typer, name );
  DValue value = daoNilFunction;
  DNode *node;
  value.v.func = func;
  if( func ) return value;
  if( typer->priv->mapValues == NULL ) return daoNilValue;
  node = DMap_Find( typer->priv->mapValues, name );
  if( node ) return ((DaoNamedValue*)node->value.pVoid)->value;
  return daoNilValue;
}
DValue DaoFindValue( DaoTypeBase *typer, const char *name )
{
  DaoNamedValue **nums = typer->priv->values;
  DaoFunction *func = DaoFindFunction( typer, name );
  DValue value = daoNilFunction;
  int left = 0;
  int right = (int)typer->priv->valCount -1;
  int mid, res;
  char *pc, *pt;
  value.v.func = func;
  if( func ) return value;
  while( left <= right ){
    mid = ( left + right ) / 2;
    /* res = strcmp( quads[ mid ].name, name ); */
    pc = (char*)nums[ mid ]->name->mbs;
    pt = (char*)name;
    res = 0;
    while( *pc || *pt ){
      if( *pc < *pt ){
        res = -1;
        break;
      }else if( *pc > *pt ){
        res = 1;
        break;
      }
      pc ++;
      pt ++;
    }
    if( res == 0 ){
      return nums[ mid ]->value;
    }else if( res > 0){
      right = mid - 1;
    }else{
      left = mid + 1;
    }
  }
  return daoNilValue;
}
DaoFunction* DaoFindFunction( DaoTypeBase *typer, const char *name )
{
  DaoFunction **funcs = typer->priv->methods;
  int left = 0;
  int right = (int)typer->priv->methCount -1;
  int mid, res;
  char *pc, *pt;
  while( left <= right ){
    mid = ( left + right ) / 2;
    /* res = strcmp( quads[ mid ].name, name ); */
    pc = (char*)funcs[ mid ]->routName->mbs;
    pt = (char*)name;
    res = 0;
    while( *pc || *pt ){
      if( *pc < *pt ){
        res = -1;
        break;
      }else if( *pc > *pt ){
        res = 1;
        break;
      }
      pc ++;
      pt ++;
    }
    if( res == 0 ){
      return funcs[ mid ];
    }else if( res > 0){
      right = mid - 1;
    }else{
      left = mid + 1;
    }
  }
  return NULL;
}


enum{ IDX_NULL, IDX_SINGLE, IDX_FROM, IDX_TO, IDX_PAIR, IDX_ALL, IDX_MULTIPLE };

static DValue DValue_MakeCopy( DValue self, DaoContext *ctx, DMap *cycData )
{
  DaoTypeBase *typer;
  if( self.t <= DAO_COMPLEX ) return self;
  typer = DValue_GetTyper( self );
  return typer->priv->Copy( & self, ctx, cycData );
}

static DArray* MakeIndex( DaoContext *ctx, DValue index, size_t N, size_t *start, size_t *end, int *idtype )
{
  size_t i;
  DValue *items;
  DValue first, second;
  DArray *array;

  *idtype = IDX_NULL;
  if( index.t == 0 ) return NULL;

  switch( index.t ){
  case DAO_INTEGER :
    *idtype = IDX_SINGLE;
    *start = index.v.i;
    break;
  case DAO_FLOAT :
    *idtype = IDX_SINGLE;
    *start = (int)(index.v.f);
    break;
  case DAO_DOUBLE :
    *idtype = IDX_SINGLE;
    *start = (int)(index.v.d);
    break;
  case DAO_PAIR :
  case DAO_TUPLE:
    *idtype = IDX_PAIR;
    if( index.t == DAO_TUPLE && index.v.tuple->unitype == dao_type_for_iterator ){
      DValue *data = index.v.tuple->items->data;
      if( data[0].t == data[1].t && data[0].t == DAO_INTEGER ){
        *idtype = IDX_SINGLE;
        *start = data[1].v.i;
        data[1].v.i += 1;
        data[0].v.i = data[1].v.i < N;
        break;
      }
    }
    if( index.t == DAO_TUPLE ){
      first = index.v.tuple->items->data[0];
      second = index.v.tuple->items->data[1];
    }else{
      first = ((DaoPair*)index.v.p)->first;
      second = ((DaoPair*)index.v.p)->second;
    }
    /* a[ : 1 ] ==> pair(nil,int) */
    if( first.t > DAO_DOUBLE || second.t > DAO_DOUBLE )
      DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "need number" );
    *start = (size_t) DValue_GetInteger( first );
    *end = (size_t) DValue_GetInteger( second );
    if( first.t ==DAO_NIL && second.t ==DAO_NIL )
      *idtype = IDX_ALL;
    else if( first.t ==DAO_NIL )
      *idtype = IDX_TO;
    else if( second.t ==DAO_NIL )
      *idtype = IDX_FROM;
    break;
  case DAO_LIST:
    *idtype = IDX_MULTIPLE;
    items = ((DaoList*)index.v.p)->items->data;
    array = DArray_New(0);
    DArray_Resize( array, index.v.list->items->size, 0 );
    for( i=0; i<array->size; i++ ){
      if( ! DValue_IsNumber( items[i] ) )
        DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "need number" );
      array->items.pInt[i] = (size_t) DValue_GetDouble( items[i] );
      if( array->items.pInt[i] >= N ){
        DaoContext_RaiseException( ctx, DAO_ERROR_INDEX_OUTOFRANGE, "" );
        array->items.pInt[i] = N - 1;
      }
    }
    return array;
  default : break;
  }
  return NULL;
}

void DaoBase_Delete( void *self ){ dao_free( self ); }

void DaoBase_Print( DValue *self, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
  if( self->t <= DAO_STREAM )
    DaoStream_WriteMBS( stream, coreTypeNames[ self->t ] );
  else
    DaoStream_WriteMBS( stream, DValue_GetTyper( * self )->name );
  if( self->t == DAO_NIL ) return;
  DaoStream_WriteMBS( stream, "_" );
  DaoStream_WriteInt( stream, self->t );
  DaoStream_WriteMBS( stream, "_" );
  DaoStream_WritePointer( stream, self->v.p );
}
DValue DaoBase_Copy( DValue *self, DaoContext *ctx, DMap *cycData )
{
  return *self;
}

DaoTypeCore baseCore =
{
  0,
#ifdef DEV_HASH_LOOKUP
  NULL, NULL,
#endif
  NULL, NULL, NULL, 0, 0,
  DaoBase_GetField,
  DaoBase_SetField,
  DaoBase_GetItem,
  DaoBase_SetItem,
  DaoBase_Print,
  DaoBase_Copy,
};
DaoTypeBase baseTyper =
{
  & baseCore,
  "null",
  NULL,
  NULL,
  {0},
  NULL,
  DaoBase_Delete
};
DaoBase nil = { 0, DAO_DATA_CONST, {0,0}, 1, 0 };

int ObjectProfile[100];

void DaoBase_Init( void *dbase, char type )
{
  DaoBase *self = (DaoBase*) dbase; 
  self->type = self->subType = type;
  self->gcState[0] = self->gcState[1]  = 0;
  self->refCount = 0;
  self->cycRefCount = 0;
#ifdef DAO_GC_PROF
  if( type < 100 )  ObjectProfile[(int)type] ++;
#endif
}

DaoBase* DaoBase_Duplicate( void *dbase )
{
  DaoBase *self = (DaoBase*) dbase;
  size_t i;

  if( dbase == NULL ) return & nil;
  if( ! (self->subType & DAO_DATA_CONST) ) return self;
  switch( self->type ){
  case DAO_LIST :
    {
      DaoList *list = (DaoList*) self;
      DaoList *copy = DaoList_New();
      copy->subType = SUB_TYPE( list ); /* remove const state */
      DVarray_Resize( copy->items, list->items->size, daoNilValue );
      for(i=0; i<list->items->size; i++)
        DaoList_SetItem( copy, list->items->data[i], i );
      copy->unitype = list->unitype;
      GC_IncRC( copy->unitype );
      return (DaoBase*)copy;
    }
  case DAO_MAP :
    {
      DaoMap *map = (DaoMap*) self;
      DaoMap *copy = DaoMap_New( map->items->hashing );
      DNode *node = DMap_First( map->items );
      copy->subType = SUB_TYPE( map ); /* remove const state */
      for( ; node !=NULL; node = DMap_Next(map->items, node ))
        DMap_Insert( copy->items, node->key.pVoid, node->value.pVoid );
      copy->unitype = map->unitype;
      GC_IncRC( copy->unitype );
      return (DaoBase*)copy;
    }
  case DAO_TUPLE :
    {
      DaoTuple *tuple = (DaoTuple*) self;
      DaoTuple *copy = DaoTuple_New( tuple->items->size );
      copy->subType = SUB_TYPE( tuple ); /* remove const state */
      for(i=0; i<tuple->items->size; i++)
        DValue_Copy( copy->items->data + i, tuple->items->data[i] );
      copy->unitype = tuple->unitype;
      GC_IncRC( copy->unitype );
      return (DaoBase*) copy;
    }
#ifdef DAO_WITH_NUMARRAY
  case DAO_ARRAY :
    return (DaoBase*) DaoArray_Copy( (DaoArray*) self );
#endif
  case DAO_PAIR :
  default : break;
  }
  return self;
}

extern DaoTypeBase numberTyper;
extern DaoTypeBase comTyper;
extern DaoTypeBase stringTyper;

DaoTypeBase* DaoBase_GetTyper( DaoBase *p )
{
  if( p ==NULL ) return & baseTyper;
  if( p->type == DAO_CDATA ) return ((DaoCData*)p)->typer;
  return DaoVmSpace_GetTyper( p->type );
}
extern DaoTypeBase funcTyper;
DaoTypeBase* DValue_GetTyper( DValue self )
{
  switch( self.t ){
  case DAO_NIL : return & baseTyper;
  case DAO_INTEGER :
  case DAO_FLOAT   :
  case DAO_DOUBLE  : return & numberTyper;
  case DAO_COMPLEX : return & comTyper;
  case DAO_STRING  : return & stringTyper;
  case DAO_CDATA   : return self.v.cdata->typer;
  case DAO_FUNCTION : return & funcTyper;
  default : break;
  }
  return DaoVmSpace_GetTyper( self.t );
}

void DaoBase_GetField( DValue *self, DaoContext *ctx, DString *name )
{
  DaoTypeBase *typer = DValue_GetTyper( *self );
  DValue p = DaoFindValue2( typer, name );
  if( p.t ==0 ){
    DString *mbs = DString_New(1);
    DString_Append( mbs, name );
    DaoContext_RaiseException( ctx, DAO_ERROR_FIELD_NOTEXIST, DString_GetMBS( mbs ) );
    DString_Delete( mbs );
    return;
  }
  DaoContext_PutValue( ctx, p );
}
void DaoBase_SetField( DValue *self, DaoContext *ctx, DString *name, DValue value )
{
}
void DaoBase_SafeGetField( DValue *self, DaoContext *ctx, DString *name )
{
  if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
    DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
    return;
  }
  DaoBase_GetField( self, ctx, name );
}
void DaoBase_SafeSetField( DValue *dbase, DaoContext *ctx, DString *name, DValue value )
{
  if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
    DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
    return;
  }
  DaoBase_SetField( dbase, ctx, name, value );
}
void DaoBase_GetItem( DValue *dbase, DaoContext *ctx, DValue pid )
{
}
void DaoBase_SetItem( DValue *dbase, DaoContext *ctx, DValue pid, DValue value )
{
}

/**/
static void DaoNumber_Print( DValue *self, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
  if( self->t == DAO_INTEGER )
    DaoStream_WriteInt( stream, self->v.i );
  else if( self->t == DAO_FLOAT )
    DaoStream_WriteFloat( stream, self->v.f );
  else
    DaoStream_WriteFloat( stream, self->v.d );
}
static void DaoNumber_GetItem( DValue *self, DaoContext *ctx, DValue pid )
{
  uint_t bits = (uint_t) DValue_GetDouble( *self );
  size_t size = 8*sizeof(uint_t);
  size_t start, end;
  int idtype;
  DArray *ids = MakeIndex( ctx, pid, size, & start, & end, & idtype );
  dint *res = DaoContext_PutInteger( ctx, 0 );
  switch( idtype ){
  case IDX_NULL :
    *res = bits; break;
  case IDX_SINGLE :
    *res = ( bits >> start ) & 0x1; break;
    /*
       case IDX_PAIR :
       for(i=start; i<=end; i++) val |= bits & ( 1<<i );
       res->value = val >> start; break;
     */
  case IDX_MULTIPLE :
    DArray_Delete( ids );
  default :
    DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "not supported" );
  }
}
static void DaoNumber_SetItem( DValue *self, DaoContext *ctx, DValue pid, DValue value )
{
  uint_t bits = (uint_t) DValue_GetDouble( *self );
  uint_t val = (uint_t) DValue_GetDouble( value );
  size_t size = 8*sizeof(uint_t);
  size_t start, end;
  int idtype;
  DArray *ids = MakeIndex( ctx, pid, size, & start, & end, & idtype );
  switch( idtype ){
  case IDX_NULL :
    bits = val; break;
  case IDX_SINGLE :
    bits |= ( ( val != 0 ) & 0x1 ) << start; break;
    /*case IDX_PAIR : bits |= ( val<<( size-end+start-1 ) ) >> (size-end-1); break;*/
  case IDX_MULTIPLE :
    DArray_Delete( ids );
  default :
    DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "not supported" );
  }
  self->v.i = bits;
}

static DaoTypeCore numberCore=
{
  0,
#ifdef DEV_HASH_LOOKUP
  NULL, NULL,
#endif
  NULL, NULL, NULL, 0, 0,
  DaoBase_GetField,
  DaoBase_SetField,
  DaoNumber_GetItem,
  DaoNumber_SetItem,
  DaoNumber_Print,
  DaoBase_Copy,
};

DaoTypeBase numberTyper=
{
  & numberCore,
  "double",
  NULL, NULL, {0}, NULL, NULL
};

/**/
static void DaoString_Print( DValue *self, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
  if( stream->useQuote ) DaoStream_WriteChar( stream, '\"' );
  DaoStream_WriteString( stream, self->v.s );
  if( stream->useQuote ) DaoStream_WriteChar( stream, '\"' );
}
static void DaoString_GetItem( DValue *self0, DaoContext *ctx, DValue pid )
{
  DString *self = self0->v.s;
  size_t size = DString_Size( self );
  size_t i, start, end;
  int idtype;
  DArray *ids = MakeIndex( ctx, pid, size, & start, & end, & idtype );
  DString *res = NULL;
  if( idtype != IDX_SINGLE ) res = DaoContext_PutMBString( ctx, "" );
  switch( idtype ){
  case IDX_NULL :
    DString_Assign( res, self );
    break;
  case IDX_SINGLE :
    {
      dint *num = DaoContext_PutInteger( ctx, 0 );
      *num = self->mbs ? self->mbs[start] : self->wcs[start];
      break;
    }
  case IDX_FROM :
    DString_Substr( self, res, start, -1 );
    break;
  case IDX_TO :
    DString_Substr( self, res, 0, end+1 );
    break;
  case IDX_PAIR :
    DString_Substr( self, res, start, end-start+1 );
    break;
  case IDX_ALL :
    DString_Substr( self, res, 0, -1 );
    break;
  case IDX_MULTIPLE :
    {
      dint *ip = ids->items.pInt;
      res = DaoContext_PutMBString( ctx, "" );
      DString_Clear( res );
      if( self->mbs ){
        char *data = self->mbs;
        for( i=0; i<ids->size; i++ ) DString_AppendChar( res, data[ ip[i] ] );
      }else{
        wchar_t *data = self->wcs;
        for( i=0; i<ids->size; i++ ) DString_AppendWChar( res, data[ ip[i] ] );
      }
      DArray_Delete( ids );
    }
    break;
  default : break;
  }
}
static void DaoString_SetItem( DValue *self0, DaoContext *ctx, DValue pid, DValue value )
{
  DString *self = self0->v.s;
  size_t size = DString_Size( self );
  size_t start, end;
  int idtype;
  DArray *ids = MakeIndex( ctx, pid, size, & start, & end, & idtype );
  if( value.t >= DAO_INTEGER && value.t <= DAO_DOUBLE ){
    int id = value.v.i;
    if( self->mbs )
      self->mbs[start] = id;
    else
      self->wcs[start] = id;
  }else if( value.t == DAO_STRING ){
    DString *str = value.v.s;
    switch( idtype ){
    case IDX_NULL :
      DString_Assign( self, str );
      break;
    case IDX_SINGLE :
      {
        int ch = str->mbs ? str->mbs[0] : str->wcs[0];
        if( self->mbs )
          self->mbs[start] = ch;
        else
          self->wcs[start] = ch;
        break;
      }
    case IDX_FROM :
      DString_Replace( self, str, start, -1 );
      break;
    case IDX_TO :
      DString_Replace( self, str, 0, end+1 );
      break;
    case IDX_PAIR :
      DString_Replace( self, str, start, end-start+1 );
      break;
    case IDX_ALL :
      DString_Assign( self, str );
      break;
    case IDX_MULTIPLE :
      DArray_Delete( ids );
      DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "not supported" );
    default : break;
    }
  }
}
static DaoTypeCore stringCore=
{
  0,
#ifdef DEV_HASH_LOOKUP
  NULL, NULL,
#endif
  NULL, NULL, NULL, 0, 0,
  DaoBase_GetField,
  DaoBase_SetField,
  DaoString_GetItem,
  DaoString_SetItem,
  DaoString_Print,
  DaoBase_Copy,
};

static void DaoSTR_Size( DaoContext *ctx, DValue *p[], int N )
{
  DaoContext_PutInteger( ctx, DString_Size( p[0]->v.s ) );
}
static void DaoSTR_Resize( DaoContext *ctx, DValue *p[], int N )
{
  if( ( ctx->vmSpace->options & DAO_EXEC_SAFE ) && p[1]->v.i > 1E5 ){
    DaoContext_RaiseException( ctx, DAO_ERROR, 
        "not permitted to create long string in safe running mode" );
    return;
  }
  DString_Resize( p[0]->v.s, p[1]->v.i );
}
static void DaoSTR_Insert( DaoContext *ctx, DValue *p[], int N )
{
  DString *self = p[0]->v.s;
  DString *str = p[1]->v.s;
  int at = (int)p[2]->v.i;
  DString_Insert( self, str, at, 0 );
}
static void DaoSTR_Clear( DaoContext *ctx, DValue *p[], int N )
{
  DString_Clear( p[0]->v.s );
}
static void DaoSTR_Erase( DaoContext *ctx, DValue *p[], int N )
{
  DString_Erase( p[0]->v.s, p[1]->v.i, p[2]->v.i );
}
static void DaoSTR_Chop( DaoContext *ctx, DValue *p[], int N )
{
  int i, k;
  unsigned char *chs;
  DString *self = p[0]->v.s;
  DString_Detach( self );
  DString_Chop( self );

  if( p[1]->v.i && self->mbs && self->size ){
    chs = (unsigned char*) self->mbs;
    i = self->size - 1;
    k = utf8_markers[ chs[i] ];
    if( k ==1 ){
      while( utf8_markers[ chs[i] ] ==1 ) i --;
      k = utf8_markers[ chs[i] ];
      if( k == 0 ){
        chs[k+1] = 0;
        self->size = k+1;
      }else if( (self->size - i) != k ){
        if( (self->size - i) < k ){
          chs[i] = 0;
          self->size = i;
        }else{
          chs[i+k] = 0;
          self->size = i + k;
        }
      }
    }else if( k !=0 ){
      chs[i] = 0; 
      self->size --;
    }
  }
  DaoContext_PutReference( ctx, p[0] );
}
static void DaoSTR_Simplify( DaoContext *ctx, DValue *p[], int N )
{
  DString_Simplify( p[0]->v.s );
  DaoContext_PutReference( ctx, p[0] );
}
static void DaoSTR_Find( DaoContext *ctx, DValue *p[], int N )
{
  DString *self = p[0]->v.s;
  DString *str = p[1]->v.s;
  size_t from = (size_t)p[2]->v.i;
  dint pos = -1000;
  if( p[3]->v.i ){
    pos = DString_RFind( self, str, from );
    if( pos == MAXSIZE ) pos = -1000;
  }else{
    pos = DString_Find( self, str, from );
    if( pos == MAXSIZE ) pos = -1000;
  }
  DaoContext_PutInteger( ctx, pos );
}
static void DaoSTR_Replace( DaoContext *ctx, DValue *p[], int N )
{
  DString *self = p[0]->v.s;
  DString *str1 = p[1]->v.s;
  DString *str2 = p[2]->v.s;
  size_t index = (size_t)p[3]->v.i;
  size_t pos, from = 0, count = 0;
  if( self->mbs ){
    DString_ToMBS( str1 );
    DString_ToMBS( str2 );
  }else{
    DString_ToWCS( str1 );
    DString_ToWCS( str2 );
  }
  if( index == 0 ){
    pos = DString_Find( self, str1, from );
    while( pos != MAXSIZE ){
      count ++;
      DString_Insert( self, str2, pos, DString_Size( str1 ) );
      from = pos + DString_Size( str2 );
      pos = DString_Find( self, str1, from );
    }
  }else if( index > 0){
    pos = DString_Find( self, str1, from );
    while( pos != MAXSIZE ){
      count ++;
      if( count == index ){
        DString_Insert( self, str2, pos, DString_Size( str1 ) );
        break;
      }
      from = pos + DString_Size( str1 );
      pos = DString_Find( self, str1, from );
    }
    count = 1;
  }else{
    from = MAXSIZE;
    pos = DString_RFind( self, str1, from );
    while( pos != MAXSIZE ){
      count --;
      if( count == index ){
        DString_Insert( self, str2, pos-DString_Size( str1 )+1, DString_Size( str1 ) );
        break;
      }
      from = pos - DString_Size( str1 );
      pos = DString_RFind( self, str1, from );
    }
    count = 1;
  }
  DaoContext_PutInteger( ctx, count );
}
static void DaoSTR_Replace2( DaoContext *ctx, DValue *p[], int N )
{
  DString *self = p[0]->v.s;
  DString *res, *key;
  DMap *par = p[1]->v.map->items;
  DMap *words = DMap_New(D_STRING,D_STRING);
  DMap *sizemap = DMap_New(0,0);
  DNode *node = DMap_First( par );
  DArray *sizes = DArray_New(0);
  int max = p[2]->v.i;
  int i, j, k, n;
  for( ; node != NULL; node = DMap_Next(par, node) )
    DMap_Insert( words, node->key.pValue->v.s, node->value.pValue->v.s );
  if( self->mbs ){
    key = DString_New(1);
    res = DString_New(1);
    for(node=DMap_First(words); node !=NULL; node = DMap_Next(words, node) ){
      DString_ToMBS( node->key.pString );
      DString_ToMBS( node->value.pString );
      MAP_Insert( sizemap, node->key.pString->size, 0 );
    }
    for(node=DMap_First(sizemap); node !=NULL; node = DMap_Next(sizemap, node) )
      DArray_Append( sizes, node->key.pInt );
    i = 0;
    n = self->size;
    while( i < n ){
      DString *val = NULL;
      for(j=0; j<sizes->size; j++){
        k = sizes->items.pInt[j];
        if( i+k > n ) break;
        DString_Substr( self, key, i, k );
        node = DMap_FindMG( words, key );
        if( node == NULL ) break;
        if( DString_EQ( node->key.pString, key ) ){
          val = node->value.pString;
          if( max ==0 ) break;
        }
      }
      if( val ){
        DString_Append( res, val );
        i += key->size;
      }else{
        DString_AppendChar( res, self->mbs[i] );
        i ++;
      }
    }
  }else{
    key = DString_New(0);
    res = DString_New(0);
    for(node=DMap_First(words); node !=NULL; node = DMap_Next(words, node) ){
      DString_ToWCS( node->key.pString );
      DString_ToWCS( node->value.pString );
      MAP_Insert( sizemap, node->key.pString->size, 0 );
    }
    for(node=DMap_First(sizemap); node !=NULL; node = DMap_Next(sizemap, node) )
      DArray_Append( sizes, node->key.pInt );
    i = 0;
    n = self->size;
    while( i < n ){
      DString *val = NULL;
      for(j=0; j<sizes->size; j++){
        k = sizes->items.pInt[j];
        if( i+k > n ) break;
        DString_Substr( self, key, i, k );
        node = DMap_FindMG( words, key );
        if( node == NULL ) break;
        if( DString_EQ( node->key.pString, key ) ){
          val = node->value.pString;
          if( max ==0 ) break;
        }
      }
      if( val ){
        DString_Append( res, val );
        i += key->size;
      }else{
        DString_AppendWChar( res, self->wcs[i] );
        i ++;
      }
    }
  }
  DString_Assign( self, res );
  DString_Delete( key );
  DString_Delete( res );
  DArray_Delete( sizes );
  DMap_Delete( words );
  DMap_Delete( sizemap );
}
static void DaoSTR_Expand( DaoContext *ctx, DValue *p[], int N )
{
  DString *self = p[0]->v.s;
  DMap    *keys = NULL;
  DaoTuple *tup = p[1]->v.tuple;
  DString *spec = p[2]->v.s;
  DString *res = NULL, *key = NULL, *val = NULL, *sub = NULL;
  DNode *node = NULL;
  DValue vkey = daoNilString;
  int keep = p[3]->v.i;
  size_t i, pos1, pos2, prev = 0;
  wchar_t spec1;
  char spec2;
  int replace;
  int ch;
  if( DString_Size( spec ) ==0 ){
    DaoContext_PutString( ctx, self );
    return;
  }
  if(  p[1]->t == DAO_TUPLE ){
    if( tup->unitype ){
      keys = tup->unitype->mapNames;
    }else{
      DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "invalid tuple" );
      return;
    }
  }else{
    tup = NULL;
    keys = p[1]->v.map->items;
  }
  if( self->mbs && spec->wcs ) DString_ToMBS( spec );
  if( self->wcs && spec->mbs ) DString_ToWCS( spec );
  if( self->mbs ){
    res = DaoContext_PutMBString( ctx, "" );
    key = DString_New(1);
    sub = DString_New(1);
    vkey.v.s = key;
    spec2 = spec->mbs[0];
    pos1 = DString_FindChar( self, spec2, prev );
    while( pos1 != MAXSIZE ){
      pos2 = DString_FindChar( self, ')', pos1 );
      replace = 0;
      if( pos2 != MAXSIZE && self->mbs[pos1+1] == '(' ){
        replace = 1;
        for(i=pos1+2; i<pos2; i++){
          ch = self->mbs[i];
          if( ch != '_' && ! isalnum( ch ) ){
            replace = 0;
            break;
          }
        }
        if( replace ){
          DString_Substr( self, key, pos1+2, pos2-pos1-2 );
          if( tup ){
            node = DMap_Find( keys, key );
          }else{
            node = DMap_Find( keys, & vkey );
          }
          if( node ){
            if( tup ){
              i = node->value.pInt;
              DValue_GetString( tup->items->data[i], key );
              val = key;
            }else{
              val = node->value.pValue->v.s;
            }
          }else if( keep ){
            replace = 0;
          }else{
            DString_Clear( key );
            val = key;
          }
        }
      }
      DString_Substr( self, sub, prev, pos1 - prev );
      DString_Append( res, sub );
      prev = pos1 + 1;
      if( replace ){
        DString_Append( res, val );
        prev = pos2 + 1;
      }else{
        DString_AppendChar( res, spec2 );
      }
      pos1 = DString_FindChar( self, spec2, prev );
    }
  }else{
    res = DaoContext_PutWCString( ctx, L"" );
    key = DString_New(0);
    sub = DString_New(0);
    vkey.v.s = key;
    spec1 = spec->wcs[0];
    pos1 = DString_FindWChar( self, spec1, prev );
    while( pos1 != MAXSIZE ){
      pos2 = DString_FindWChar( self, L')', pos1 );
      replace = 0;
      if( pos2 != MAXSIZE && self->wcs[pos1+1] == L'(' ){
        replace = 1;
        for(i=pos1+2; i<pos2; i++){
          ch = self->wcs[i];
          if( ch != L'_' && ! isalnum( ch ) ){
            replace = 0;
            break;
          }
        }
        if( replace ){
          DString_Substr( self, key, pos1+2, pos2-pos1-2 );
          if( tup ){
            node = DMap_Find( keys, key );
          }else{
            node = DMap_Find( keys, & vkey );
          }
          if( node ){
            if( tup ){
              i = node->value.pInt;
              DValue_GetString( tup->items->data[i], key );
              val = key;
            }else{
              val = node->value.pValue->v.s;
            }
          }else if( keep ){
            replace = 0;
          }else{
            DString_Clear( key );
            val = key;
          }
        }
      }
      DString_Substr( self, sub, prev, pos1 - prev );
      DString_Append( res, sub );
      prev = pos1 + 1;
      if( replace ){
        DString_Append( res, val );
        prev = pos2 + 1;
      }else{
        DString_AppendWChar( res, spec1 );
      }
      pos1 = DString_FindWChar( self, spec1, prev );
    }
  }
  DString_Substr( self, sub, prev, DString_Size( self ) - prev );
  DString_Append( res, sub );
  DString_Delete( key );
  DString_Delete( sub );
}
static void DaoSTR_Split( DaoContext *ctx, DValue *p[], int N )
{
  DString *self = p[0]->v.s;
  DString *delm = p[1]->v.s;
  DString *quote = p[2]->v.s;
  int rm = (int)p[3]->v.i;
  DaoList *list = DaoContext_PutList( ctx );
  DString *str = DString_New(1);
  DValue value = daoNilString;
  size_t dlen = DString_Size( delm );
  size_t qlen = DString_Size( quote );
  size_t size = DString_Size( self );
  size_t last = 0;
  size_t posDelm = DString_Find( self, delm, last );
  size_t posQuote = DString_Find( self, quote, last );
  size_t posQuote2 = -1;
  value.v.s = str;
  if( N ==1 || DString_Size( delm ) ==0 ){
    size_t i = 0;
    if( self->mbs ){
      unsigned char *mbs = (unsigned char*) self->mbs;
      size_t j, k;
      while( i < size ){
        k = utf8_markers[ mbs[i] ];
        if( k ==0 || k ==7 ){
          DString_SetBytes( str, (char*)mbs + i, 1 );
          DVarray_Append( list->items, value );
          i ++;
        }else if( k ==1 ){
          k = i;
          while( i < size && utf8_markers[ mbs[i] ] ==1 ) i ++;
          DString_SetBytes( str, (char*)mbs + k, i-k );
          DVarray_Append( list->items, value );
        }else{
          for( j=1; j<k; j++ ){
            if( i + j >= size ) break;
            if( utf8_markers[ mbs[i+j] ] != 1 ) break;
          }
          DString_SetBytes( str, (char*)mbs + i, j );
          DVarray_Append( list->items, value );
          i += j;
        }
      }
    }else{
      wchar_t *wcs = self->wcs;
      DString_ToWCS( str );
      DString_Resize( str, 1 );
      for(i=0; i<size; i++){
        DString_Detach( str );
        str->wcs[0] = wcs[i];
        DVarray_Append( list->items, value );
      }
    }
    DString_Delete( str );
    return;
  }
  if( posDelm != MAXSIZE && posQuote != MAXSIZE && posQuote < posDelm ){
    posQuote2 = DString_Find( self, quote, posQuote+qlen );
    if( posQuote2 != MAXSIZE && posQuote2 > posDelm )
      posDelm = DString_Find( self, delm, posQuote2 );
  }
  while( posDelm != MAXSIZE ){
    if( rm && posQuote == last && posQuote2 == posDelm-qlen )
      DString_Substr( self, str, last+qlen, posDelm-last-2*qlen );
    else
      DString_Substr( self, str, last, posDelm-last );
    if( last !=0 || posDelm !=0 ) DVarray_Append( list->items, value );

    last = posDelm + dlen;
    posDelm = DString_Find( self, delm, last );
    posQuote = DString_Find( self, quote, last );
    posQuote2 = -1;
    if( posDelm != MAXSIZE && posQuote != MAXSIZE && posQuote < posDelm ){
      posQuote2 = DString_Find( self, quote, posQuote+qlen );
      if( posQuote2 != MAXSIZE && posQuote2 > posDelm )
        posDelm = DString_Find( self, delm, posQuote2 );
    }
  }
  if( posQuote != MAXSIZE && posQuote < size )
    posQuote2 = DString_Find( self, quote, posQuote+qlen );
  if( rm && posQuote == last && posQuote2 == size-qlen )
    DString_Substr( self, str, last+qlen, size-last-2*qlen );
  else
    DString_Substr( self, str, last, size-last );
  DVarray_Append( list->items, value );
  DString_Delete( str );
}
static void DaoSTR_Tokenize( DaoContext *ctx, DValue *p[], int N )
{
  DString *self = p[0]->v.s;
  DString *delms = p[1]->v.s;
  DString *quotes = p[2]->v.s;
  int bkslash = (int)p[3]->v.i;
  int simplify = (int)p[4]->v.i;
  DaoList *list = DaoContext_PutList( ctx );
  DString *str = DString_New(1);
  DValue value = daoNilString;
  value.v.s = str;
  if( self->mbs ){
    char *s = self->mbs;
    DString_ToMBS( str );
    DString_ToMBS( delms );
    DString_ToMBS( quotes );
    while( *s ){
      if( bkslash && *s == '\\' ){
        DString_AppendChar( str, *s );
        DString_AppendChar( str, *(s+1) );
        s += 2;
        continue;
      }
      if( ( bkslash == 0 || s == self->mbs || *(s-1) !='\\' ) 
          && DString_FindChar( quotes, *s, 0 ) != MAXSIZE ){
        DString_AppendChar( str, *s );
        s ++;
        while( *s ){
          if( bkslash && *s == '\\' ){
            DString_AppendChar( str, *s );
            DString_AppendChar( str, *(s+1) );
            s += 2;
          }
          if( ( bkslash == 0 || *(s-1) !='\\' ) 
              && DString_FindChar( quotes, *s, 0 ) != MAXSIZE )
            break;
          DString_AppendChar( str, *s );
          s ++;
        }
        DString_AppendChar( str, *s );
        s ++;
        continue;
      }
      if( DString_FindChar( delms, *s, 0 ) != MAXSIZE ){
        if( s != self->mbs && *(s-1)=='\\' ){
          DString_AppendChar( str, *s );
          s ++;
          continue;
        }else{
          if( simplify ) DString_Simplify( str );
          if( str->size > 0 ){
            DVarray_Append( list->items, value );
            DString_Clear( str );
          }
          DString_AppendChar( str, *s );
          s ++;
          if( simplify ) DString_Simplify( str );
          if( str->size > 0 ) DVarray_Append( list->items, value );
          DString_Clear( str );
          continue;
        }
      }
      DString_AppendChar( str, *s );
      s ++;
    }
    if( simplify ) DString_Simplify( str );
    if( str->size > 0 ) DVarray_Append( list->items, value );
  }else{
    wchar_t *s = self->wcs;
    DString_ToWCS( str );
    DString_ToWCS( delms );
    DString_ToWCS( quotes );
    while( *s ){
      if( ( s == self->wcs || bkslash ==0 || *(s-1)!=L'\\' ) 
        && DString_FindWChar( quotes, *s, 0 ) != MAXSIZE ){
        DString_AppendChar( str, *s );
        s ++;
        while( *s ){
          if( ( bkslash ==0 || *(s-1)!=L'\\' )
              && DString_FindWChar( quotes, *s, 0 ) != MAXSIZE ) break;
          DString_AppendChar( str, *s );
          s ++;
        }
        DString_AppendChar( str, *s );
        s ++;
        continue;
      }
      if( DString_FindWChar( delms, *s, 0 ) != MAXSIZE ){
        if( s != self->wcs && ( bkslash && *(s-1)==L'\\' ) ){
          DString_AppendChar( str, *s );
          s ++;
          continue;
        }else{
          if( simplify ) DString_Simplify( str );
          if( str->size > 0 ){
            DVarray_Append( list->items, value );
            DString_Clear( str );
          }
          DString_AppendChar( str, *s );
          s ++;
          if( simplify ) DString_Simplify( str );
          if( str->size > 0 ) DVarray_Append( list->items, value );
          DString_Clear( str );
          continue;
        }
      }
      DString_AppendChar( str, *s );
      s ++;
    }
    if( simplify ) DString_Simplify( str );
    if( str->size > 0 ) DVarray_Append( list->items, value );
  }
  DString_Delete( str );
}
int xBaseInteger( char *first, char *last, int xbase, DaoContext *ctx )
{
  register char *p = last;
  register int m = 1;
  register int k = 0;
  register int d;
  register char c;
  first --;
  while( p != first ){
    c = ( (*p) | 0x20 );
    d = ( c>='0' && c<='9' ) ? ( c -'0' ) : c - ('a' - 10); 
    if( d >= xbase || d < 0 ){
      DaoContext_RaiseException( ctx, DAO_ERROR, "invalid digit" );
      return 0;
    }
    k += d*m;
    m *= xbase;
    p --;
  }
  return k;
}
static double xBaseDecimal( char *first, char *last, int xbase, DaoContext *ctx )
{
  register char *p = first;
  register double inv = 1.0 / xbase;
  register double m = inv;
  register double k = 0;
  register int d;
  register char c;
  while( p != last ){
    c = ( (*p) | 0x20 );
    d = ( c>='0' && c<='9' ) ? ( c -'0' ) : c - ('a' - 10); 
    if( d >= xbase || d < 0 ){
      DaoContext_RaiseException( ctx, DAO_ERROR, "invalid digit" );
      return 0;
    }
    k += d*m;
    m *= inv;
    p ++;
  }
  return k;
}
static double xBaseNumber( DString *str, int xbase, DaoContext *ctx )
{
  char *chs = str->mbs;
  size_t dot = DString_FindChar( str, '.', 0 );
  double num = 0;
  int negat = 0;
  char *first = chs;

  if( str->size == 0 ) return 0;
  if( chs[0] == '-' ) negat = 1;
  if( chs[0] =='+' || chs[0] =='-' ) first ++;

  if( dot != MAXSIZE ){
    num += xBaseInteger( first, chs+(dot-1), xbase, ctx );
    if( dot+1 < str->size ) num += xBaseDecimal( chs+dot+1, chs+str->size, xbase, ctx );
  }else{
    num += xBaseInteger( first, chs+(str->size-1), xbase, ctx );
  }
  if( negat ) num = - num;
  return num;
}
static void DaoSTR_Tonumber( DaoContext *ctx, DValue *p[], int N )
{
  double *num = DaoContext_PutDouble( ctx, 0.0 );
  DString *mbs = DString_Copy( p[0]->v.s );
  DString_ToMBS( mbs );
  *num = xBaseNumber( mbs, p[1]->v.i, ctx );
  DString_Delete( mbs );
}
static void DaoSTR_Tolower( DaoContext *ctx, DValue *p[], int N )
{
  DString_ToLower( p[0]->v.s );
  DaoContext_PutReference( ctx, p[0] );
}
  const char   *ptype;
static void DaoSTR_Toupper( DaoContext *ctx, DValue *p[], int N )
{
  DString_ToUpper( p[0]->v.s );
  DaoContext_PutReference( ctx, p[0] );
}
static void DaoSTR_PFind( DaoContext *ctx, DValue *p[], int N )
{
  DString *self = p[0]->v.s;
  DString *pt = p[1]->v.s;
  size_t index = p[2]->v.i;
  size_t start = (size_t)p[3]->v.i;
  size_t end = (size_t)p[4]->v.i;
  size_t i, p1=start, p2=end;
  DValue value = daoZeroInt;
  DValue vtup = daoNilTuple;
  DaoTuple *tuple = NULL; 
  DaoList *list = DaoContext_PutList( ctx );
  DaoRegex *patt = DaoVmProcess_MakeRegex( ctx, pt, self->wcs ==NULL );
  if( patt ==NULL ) return;
  if( end == 0 ) p2 = end = DString_Size( self );
  i = 0;
  while( DaoRegex_Match( patt, self, & p1, & p2 ) ){
    if( index ==0 || (++i) == index ){
      tuple = vtup.v.tuple = DaoTuple_New( 2 );
      value.v.i = p1;
      DValue_Copy( tuple->items->data, value );
      value.v.i = p2;
      DValue_Copy( tuple->items->data + 1, value );
      DVarray_Append( list->items, vtup );
      if( index ) break;
    }
    p[3]->v.i = p1;
    p[4]->v.i = p2;
    p1 = p2 + 1;
    p2 = end;
  }
}
static void DaoSTR_Match0( DaoContext *ctx, DValue *p[], int N, int subm )
{
  DString *self = p[0]->v.s;
  DString *pt = p[1]->v.s;
  size_t start = (size_t)p[2]->v.i;
  size_t end = (size_t)p[3]->v.i;
  int capt = p[4]->v.i;
  size_t p1=start, p2=end;
  int gid = p[2]->v.i;
  DValue value = daoZeroInt;
  DValue matched = daoNilString;
  DaoTuple *tuple = DaoTuple_New( 3 );
  DaoRegex *patt = DaoVmProcess_MakeRegex( ctx, pt, self->wcs ==NULL );
  DaoContext_SetResult( ctx, (DaoBase*) tuple );
  if( patt ==NULL ) return;
  if( subm ){
    end = capt;
    p1 = start = end;
  }
  if( end == 0 ) p2 = end = DString_Size( self );
  pt = DString_Copy( pt );
  matched.v.s = pt;
  DString_Clear( pt );
  if( DaoRegex_Match( patt, self, & p1, & p2 ) ){
    if( subm && DaoRegex_SubMatch( patt, gid, & p1, & p2 ) ==0 ) p1 = -1;
  }else{
    p1 = -1;
  }
  value.v.i = p1;
  DValue_Copy( tuple->items->data, value );
  value.v.i = p2;
  DValue_Copy( tuple->items->data + 1, value );
  if( p1 >=0 && ( subm || capt ) ) DString_Substr( self, pt, p1, p2-p1+1 );
  DValue_Copy( tuple->items->data + 2, matched );
  if( subm ==0 ){
    p[2]->v.i = p1;
    p[3]->v.i = p2;
  }
  DString_Delete( pt );
}
static void DaoSTR_Match( DaoContext *ctx, DValue *p[], int N )
{
  DaoSTR_Match0( ctx, p, N, 0 );
}
static void DaoSTR_SubMatch( DaoContext *ctx, DValue *p[], int N )
{
  DaoSTR_Match0( ctx, p, N, 1 );
}
static void DaoSTR_Extract( DaoContext *ctx, DValue *p[], int N )
{
  DString *self = p[0]->v.s;
  DString *pt = p[1]->v.s;
  DString *mask = p[3]->v.s;
  int i, from, to, step, type = p[2]->v.i;
  int rev = p[4]->v.i;
  size_t size = DString_Size( self );
  size_t end=size, p1=0, p2=size;
  DValue subs = daoNilString;
  DArray *masks = DArray_New(0);
  DArray *matchs = DArray_New(0);
  DaoList *list = DaoContext_PutList( ctx );
  DaoRegex *patt = DaoVmProcess_MakeRegex( ctx, pt, self->wcs ==NULL );
  DaoRegex *ptmask = NULL;
  pt = DString_Copy( pt );
  if( size == 0 ) goto DoNothing;
  if( DString_Size( mask ) ==0 ) mask = NULL;
  if( mask ){
    ptmask = DaoVmProcess_MakeRegex( ctx, mask, self->wcs ==NULL );
    if( ptmask ==NULL ) goto DoNothing;
  }
  if( patt ==NULL ) goto DoNothing;
  subs.v.s = pt;
  if( mask == NULL || rev ) DArray_Append( masks, 0 );
  if( mask ){
    while( DaoRegex_Match( ptmask, self, & p1, & p2 ) ){
      DArray_Append( masks, p1 );
      DArray_Append( masks, p2 + 1 );
      p1 = p2 + 1;  p2 = size;
    }
  }
  if( mask == NULL || rev ) DArray_Append( masks, size );
  DArray_Append( matchs, 0 );
  for(i=0; i<masks->size; i+=2){
    p1 = masks->items.pInt[i];
    p2 = end = masks->items.pInt[i+1] - 1;
    while( DaoRegex_Match( patt, self, & p1, & p2 ) ){
      DArray_Append( matchs, p1 );
      DArray_Append( matchs, p2 + 1 );
      p1 = p2 + 1;  p2 = end;
    }
  }
  DArray_Append( matchs, size );
  step = 2;
  from = 0;
  to = matchs->size -1;
  if( type > 0 ){
    from = 1;
  }else if( type < 0 ){
    to = matchs->size;
  }else{
    step = 1;
  }
  for(i=from; i<to; i+=step){
    p1 = matchs->items.pInt[i];
    p2 = matchs->items.pInt[i+1];
    /*
    printf( "p1 = %i, p2 = %i\n", p1, p2 );
    */
    if( (p1 >0 && p1 <size) || p2 > p1 ){
      DString_Substr( self, pt, p1, p2-p1 );
      DVarray_Append( list->items, subs );
    }
  }
DoNothing:
  DString_Delete( pt );
  DArray_Delete( masks );
  DArray_Delete( matchs );
}
static void DaoSTR_Capture( DaoContext *ctx, DValue *p[], int N )
{
  DString *self = p[0]->v.s;
  DString *pt = p[1]->v.s;
  size_t start = (size_t)p[2]->v.i;
  size_t end = (size_t)p[3]->v.i;
  size_t p1=start, p2=end;
  int gid;
  DValue subs = daoNilString;
  DaoList *list = DaoContext_PutList( ctx );
  DaoRegex *patt = DaoVmProcess_MakeRegex( ctx, pt, self->wcs ==NULL );
  if( patt ==NULL ) return;
  if( end == 0 ) p2 = end = DString_Size( self );
  if( DaoRegex_Match( patt, self, & p1, & p2 ) ==0 ) return;
  pt = DString_Copy( pt );
  subs.v.s = pt;
  for( gid=0; gid<=patt->group; gid++ ){
    DString_Clear( pt );
    if( DaoRegex_SubMatch( patt, gid, & p1, & p2 ) ){
      DString_Substr( self, pt, p1, p2-p1+1 );
    }
    DVarray_Append( list->items, subs );
  }
  p[2]->v.i = p1;
  p[3]->v.i = p2;
  DString_Delete( pt );
}
static void DaoSTR_Change( DaoContext *ctx, DValue *p[], int N )
{
  DString *self = p[0]->v.s;
  DString *pt = p[1]->v.s;
  DString *str = p[2]->v.s;
  size_t start = (size_t)p[4]->v.i;
  size_t end = (size_t)p[5]->v.i;
  dint n, index = p[3]->v.i;
  DaoRegex *patt = DaoVmProcess_MakeRegex( ctx, pt, self->wcs ==NULL );
  n = DaoRegex_Change( patt, self, str, index, & start, & end );
  DaoContext_PutInteger( ctx, n );
}
static const char *errmsg[2] =
{
  "invalid key",
  "invalid source"
};
static void DaoSTR_Encrypt( DaoContext *ctx, DValue *p[], int N )
{
  int rc = DString_Encrypt( p[0]->v.s, p[1]->v.s, p[2]->v.i );
  if( rc ) DaoContext_RaiseException( ctx, DAO_ERROR, errmsg[rc-1] );
  DaoContext_PutReference( ctx, p[0] );
}
static void DaoSTR_Decrypt( DaoContext *ctx, DValue *p[], int N )
{
  int rc = DString_Decrypt( p[0]->v.s, p[1]->v.s, p[2]->v.i );
  if( rc ) DaoContext_RaiseException( ctx, DAO_ERROR, errmsg[rc-1] );
  DaoContext_PutReference( ctx, p[0] );
}

static DaoFuncItem stringMeths[] =
{
  { DaoSTR_Size,    "size( self :string )const=>int" },
  { DaoSTR_Resize,  "resize( self :string, size :int )" },
  { DaoSTR_Insert,  "insert( self :string, str :string, index=0 )" },
  { DaoSTR_Clear,   "clear( self :string )" },
  { DaoSTR_Erase,   "erase( self :string, start=0, n=-1 )" },
  { DaoSTR_Chop,    "chop( self :string, utf8=0 ) =>string" },
  { DaoSTR_Simplify,"simplify( self :string ) =>string" },
  /* return -1, if not found. */
  { DaoSTR_Find,    "find( self :string, str :string, from=0, reverse=0 )const=>int" },
  /* replace index-th occurrence: =0: replace all; >0: from begin; <0: from end. */
  /* return int of occurrence replaced. */
  { DaoSTR_Replace, "replace( self :string, str1 :string, str2 :string, index=0 )" },
  { DaoSTR_Replace2, "replace( self :string, table : map<string,string>, max=0 )" },
  { DaoSTR_Expand,  "expand( self :string, keys :map<string,string>, spec='$', keep=1 )const=>string" },
  { DaoSTR_Expand,  "expand( self :string, keys : tuple, spec='$', keep=1 )const=>string" },
  { DaoSTR_Split, "split( self :string, sep='', quote='', rm=1 )const=>list<string>" },
  { DaoSTR_Tokenize,
    "tokenize( self :string, seps :string, quotes='', backslash=0, simplify=0 )"
      "const=>list<string>" },
  { DaoSTR_PFind, "pfind( self :string, pt :string, index=0, start=0, end=0 )const=>list<tuple<int,int>>" },
  { DaoSTR_Match, "match( self :string, pt :string, start=0, end=0, substring=1 )const=>tuple<start:int,end:int,substring:string>" },
  { DaoSTR_SubMatch, "submatch( self :string, pt :string, group:int, start=0, end=0 )const=>tuple<start:int,end:int,substring:string>" },
  { DaoSTR_Extract, "extract( self :string, pt :string, matched=1, mask='', rev=0 )const=>list<string>" },
  { DaoSTR_Capture, "capture( self :string, pt :string, start=0, end=0 )const=>list<string>" },
  { DaoSTR_Change,  "change( self :string, pt :string, s:string, index=0, start=0, end=0 )=>int" },
  { DaoSTR_Tonumber,  "tonumber( self :string, base=10 )const=>double" },
  { DaoSTR_Tolower,   "tolower( self :string ) =>string" },
  { DaoSTR_Toupper,   "toupper( self :string ) =>string" },
  { DaoSTR_Encrypt,   "encrypt( self :string, key :string, hex=0 ) => string" },
  { DaoSTR_Decrypt,   "decrypt( self :string, key :string, hex=0 ) => string" },
  { NULL, NULL }
};

DaoTypeBase stringTyper=
{
  (void*) &stringCore,
  "string",
  NULL,
  (DaoFuncItem*) stringMeths,
  {0}, NULL, NULL
};

void DaoAbsType_Delete( DaoAbsType *self )
{
  GC_DecRC( self->X.extra );
  GC_DecRCs( self->nested );
  DString_Delete( self->name );
  if( self->fname ) DString_Delete( self->fname );
  if( self->nested ) DArray_Delete( self->nested );
  if( self->mapNames ) DMap_Delete( self->mapNames );
  dao_free( self );
}
DaoTypeBase abstypeTyper=
{
  & baseCore,
  "ABSTYPE",
  NULL, NULL, {0}, NULL,
  (FuncPtrDel) DaoAbsType_Delete
};

void DaoAbsType_MapNames( DaoAbsType *self );
void DaoAbsType_CheckName( DaoAbsType *self )
{
  if( DString_FindChar( self->name, '?', 0 ) != MAXSIZE 
      || DString_FindChar( self->name, '@', 0 ) != MAXSIZE )
    self->attrib |= DAO_TYPE_NOTDEF;
}
DaoAbsType* DaoAbsType_New( const char *name, short tid, DaoBase *extra, DArray *nest )
{
  DaoAbsType *self = (DaoAbsType*) dao_malloc( sizeof(DaoAbsType) );
  DaoBase_Init( self, DAO_ABSTYPE );
  self->tid = tid;
  self->X.extra = extra;
  self->typer = (DaoTypeBase*) DaoVmSpace_GetTyper( tid );
  self->name = DString_New(1);
  self->fname = NULL;
  self->attrib = 0;
  self->ffitype = 0;
  self->nested = NULL;
  self->mapNames = NULL;
  DString_SetMBS( self->name, name );
  DaoAbsType_CheckName( self );
  if( nest ){
    self->nested = DArray_New(0);
    DArray_Assign( self->nested, nest );
    GC_IncRCs( self->nested );
  }else if( tid == DAO_ROUTINE || tid == DAO_TUPLE ){
    self->nested = DArray_New(0);
  }
  GC_IncRC( extra );
  if( tid == DAO_ROUTINE || tid == DAO_TUPLE ) DaoAbsType_MapNames( self );
  return self;
}
DaoAbsType* DaoAbsType_Copy( DaoAbsType *other )
{
  DaoAbsType *self = (DaoAbsType*) dao_malloc( sizeof(DaoAbsType) );
  memcpy( self, other, sizeof(DaoAbsType) );
  self->name = DString_Copy( other->name );
  self->nested = NULL;
  if( other->fname ) self->fname = DString_Copy( other->fname );
  if( other->nested ){
    self->nested = DArray_Copy( other->nested );
    GC_IncRCs( self->nested );
  }
  if( other->mapNames ) self->mapNames = DMap_Copy( other->mapNames );
  GC_IncRC( self->X.extra );
  return self;
}
void DaoAbsType_MapNames( DaoAbsType *self )
{
  DaoAbsType *tp;
  int i, j, k = 0;
  if( self->nested == NULL ) return;
  if( self->tid != DAO_TUPLE && self->tid != DAO_ROUTINE ) return;
  if( self->mapNames == NULL ) self->mapNames = DMap_New(D_STRING,0);
  for(i=0; i<self->nested->size; i++){
    tp = self->nested->items.pAbtp[i];
    if( tp->fname ){
      j = self->tid == DAO_ROUTINE ? i|(k<<MAPF_OFFSET) : i;
      MAP_Insert( self->mapNames, tp->fname, j );
    }
    k += (tp->tid == DAO_PAR_GROUP) ? tp->nested->size : 1;
  }
}

#define MIN(x,y) (x>y?y:x)

extern int DaoCData_ChildOf( DaoTypeBase *self, DaoTypeBase *super );

static unsigned char dao_type_matrix[END_EXTRA_TYPES][END_EXTRA_TYPES];

void DaoAbsType_Init()
{
  int i, j;
  dao_typing_cache = DHash_New(D_VOID2,0);
#ifdef DAO_WITH_THREAD
  DMutex_Init( & dao_typing_mutex );
#endif
  memset( dao_type_matrix, DAO_MT_NOT, END_EXTRA_TYPES*END_EXTRA_TYPES );
  for(i=DAO_INTEGER; i<=DAO_DOUBLE; i++){
    for(j=DAO_INTEGER; j<=DAO_DOUBLE; j++)
      dao_type_matrix[i][j] = DAO_MT_SUB;
  }
  for(i=0; i<END_EXTRA_TYPES; i++){
    dao_type_matrix[i][i] = DAO_MT_EQ;
    dao_type_matrix[DAO_UDF][i] = DAO_MT_UDF;
    dao_type_matrix[i][DAO_UDF] = DAO_MT_UDF;
    dao_type_matrix[i][DAO_ANY] = DAO_MT_ANY;
    dao_type_matrix[DAO_INITYPE][i] = DAO_MT_INIT;
    dao_type_matrix[i][DAO_INITYPE] = DAO_MT_INIT;

    dao_type_matrix[i][DAO_PAR_NAMED] = DAO_MT_EQ+1;
    dao_type_matrix[i][DAO_PAR_DEFAULT] = DAO_MT_EQ+1;
    dao_type_matrix[DAO_PAR_NAMED][i] = DAO_MT_EQ+1;
    dao_type_matrix[DAO_PAR_DEFAULT][i] = DAO_MT_EQ+1;
  }
  dao_type_matrix[DAO_UDF][DAO_ANY] = DAO_MT_ANYUDF;
  dao_type_matrix[DAO_ANY][DAO_UDF] = DAO_MT_ANYUDF;
  dao_type_matrix[DAO_INITYPE][DAO_ANY] = DAO_MT_ANYUDF;
  dao_type_matrix[DAO_ANY][DAO_INITYPE] = DAO_MT_ANYUDF;
  dao_type_matrix[DAO_UDF][DAO_INITYPE] = DAO_MT_UDF;
  dao_type_matrix[DAO_INITYPE][DAO_UDF] = DAO_MT_UDF;

  dao_type_matrix[DAO_LIST_EMPTY][DAO_LIST] = DAO_MT_EQ;
  dao_type_matrix[DAO_ARRAY_EMPTY][DAO_ARRAY] = DAO_MT_EQ;
  dao_type_matrix[DAO_MAP_EMPTY][DAO_MAP] = DAO_MT_EQ;

  dao_type_matrix[DAO_ARRAY][DAO_ARRAY] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_LIST][DAO_LIST] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_MAP][DAO_MAP] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_TUPLE][DAO_TUPLE] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_ROUTINE][DAO_ROUTINE] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_CLASS][DAO_CLASS] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_LIST][DAO_LIST_ANY] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_ARRAY][DAO_ARRAY_ANY] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_MAP][DAO_MAP_ANY] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_TUPLE][DAO_PAR_GROUP] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_PAR_GROUP][DAO_PAR_GROUP] = DAO_MT_EQ+1;

  dao_type_matrix[DAO_CLASS][DAO_CLASS] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_CLASS][DAO_CDATA] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_OBJECT][DAO_CDATA] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_OBJECT][DAO_OBJECT] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_CDATA][DAO_CDATA] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_ROUTINE][DAO_ROUTINE] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_ROUTINE][DAO_FUNCTION] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_FUNCTION][DAO_ROUTINE] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_FUNCTION][DAO_FUNCTION] = DAO_MT_EQ+1;
  dao_type_matrix[DAO_VMPROCESS][DAO_ROUTINE] = DAO_MT_EQ+1;
}
short DaoAbsType_MatchTo( DaoAbsType *self, DaoAbsType *type, DMap *defs );
static short DaoAbsType_MatchPar( DaoAbsType *self, DaoAbsType *type, DMap *defs, int host )
{
  DaoAbsType *ext1 = self;
  DaoAbsType *ext2 = type;
  int p1 = self->tid == DAO_PAR_NAMED || self->tid == DAO_PAR_DEFAULT;
  int p2 = type->tid == DAO_PAR_NAMED || type->tid == DAO_PAR_DEFAULT;
  int m = 0;
  if( p1 && p2 && ! DString_EQ( self->fname, type->fname ) ) return DAO_MT_NOT;
  if( p1 ) ext1 = self->X.abtype;
  if( p2 ) ext2 = type->X.abtype;
  
  m = DaoAbsType_MatchTo( ext1, ext2, defs );
  /* when a tuple matchs to a parameter group, 
   * the field name and default does not matter,
   * in other case it matters, but now, use less strict checking,
   * to allow tuple without field names to be casted to 
   * tuple with name automatically */
  if( host != DAO_PAR_GROUP && p1 == 0 && p2 && m == DAO_MT_EQ ) m = DAO_MT_SUB;
  if( host == DAO_ROUTINE ){
    if( self->tid != DAO_PAR_DEFAULT && type->tid == DAO_PAR_DEFAULT ) return 0;
    return m;
  }
  return m;
}
static short DaoAbsType_MatchToX( DaoAbsType *self, DaoAbsType *type, DMap *defs )
{
  DaoAbsType *it1, *it2;
  DNode *node = NULL;
  short i, k, mt = DAO_MT_NOT;
  if( self ==NULL || type ==NULL ) return DAO_MT_NOT;
  if( self == type ) return DAO_MT_EQ;
  mt = dao_type_matrix[self->tid][type->tid];
  /*
     printf( "here: %i  %i  %i, %s %s\n", mt, self->tid, type->tid, self->name->mbs, type->name->mbs );
   */
  if( mt == DAO_MT_INIT ){
    if( self && self->tid == DAO_INITYPE ){
      if( defs ) node = MAP_Find( defs, self );
      if( node ) self = node->value.pAbtp;
    }
    if( type && type->tid == DAO_INITYPE ){
      if( defs ) node = MAP_Find( defs, type );
      if( node == NULL ){
        if( defs ) MAP_Insert( defs, type, self );
        if( self == NULL || self->tid==DAO_ANY || self->tid==DAO_UDF )
          return DAO_MT_ANYUDF;
        return DAO_MT_INIT; /* even if self==NULL, for typing checking for undefined @X */
      }
      type = node->value.pAbtp;
      mt = DAO_MT_INIT;
      if( type == NULL || type->tid==DAO_ANY || type->tid==DAO_UDF )
        return DAO_MT_ANYUDF;
    }
  }else if( mt == DAO_MT_UDF ){
    if( self->tid == DAO_UDF ){
      if( defs && type->tid != DAO_UDF ) MAP_Insert( defs, self, type );
      if( type->tid==DAO_ANY || type->tid==DAO_UDF ) return DAO_MT_ANYUDF;
    }else{
      if( defs && self->tid != DAO_UDF ) MAP_Insert( defs, type, self );
      if( self->tid==DAO_ANY || self->tid==DAO_UDF ) return DAO_MT_ANYUDF;
    }
    return mt;
  }
  mt = dao_type_matrix[self->tid][type->tid];
  switch( mt ){
  case DAO_MT_NOT : case DAO_MT_ANYUDF : case DAO_MT_ANY : case DAO_MT_EQ :
    return mt;
  default : break;
  }
  switch( self->tid ){
  case DAO_ARRAY : case DAO_LIST :
  case DAO_MAP : case DAO_TUPLE : case DAO_PAR_GROUP :
    /* for PAR_GROUP, nested always has size >=2 */
    /* tuple<...> to tuple */
    if( self->tid == DAO_TUPLE && type->nested->size ==0 ) return DAO_MT_SUB;
    if( self->nested->size > type->nested->size ) return DAO_MT_NOT;
    for(i=0; i<self->nested->size; i++){
      it1 = self->nested->items.pAbtp[i];
      it2 = type->nested->items.pAbtp[i];
      k = DaoAbsType_MatchPar( it1, it2, defs, type->tid );
      if( k == DAO_MT_NOT ) return k;
      if( k < mt ) mt = k;
    } 
    break;
  case DAO_ROUTINE :
    if( self->nested->size < type->nested->size ) return DAO_MT_NOT;
    if( self->X.extra == NULL && type->X.extra ) return 0;
    /* self may have extra parameters, but they must have default values: */
    for(i=type->nested->size; i<self->nested->size; i++){
      it1 = self->nested->items.pAbtp[i];
      if( it1->tid != DAO_PAR_DEFAULT ) return 0;
    }
    for(i=0; i<type->nested->size; i++){
      it1 = self->nested->items.pAbtp[i];
      it2 = type->nested->items.pAbtp[i];
      k = DaoAbsType_MatchPar( it1, it2, defs, DAO_ROUTINE );
      if( k == DAO_MT_NOT ) return k;
      if( k < mt ) mt = k;
    } 
    if( self->X.extra && type->X.extra ){
      k = DaoAbsType_MatchTo( self->X.abtype, type->X.abtype, defs );
      if( k < mt ) mt = k;
    }
    break;
  case DAO_CLASS :
  case DAO_OBJECT :
    if( self->X.extra == type->X.extra ) return DAO_MT_EQ;
    if( DaoClass_ChildOf( self->X.klass, type->X.extra ) ) return DAO_MT_SUB;
    return DAO_MT_NOT;
    break;
  case DAO_CDATA :
    if( self->typer == type->typer ){
      return DAO_MT_EQ;
    }else if( DaoCData_ChildOf( self->typer, type->typer ) ){
      return DAO_MT_SUB;
    }else{
      return DAO_MT_NOT;
    }
    break;
  case DAO_PAR_NAMED :
  case DAO_PAR_DEFAULT :
    return DaoAbsType_MatchPar( self, type, defs, 0 );
  default :
    if( type->tid == DAO_PAR_NAMED || type->tid == DAO_PAR_DEFAULT )
      return DaoAbsType_MatchPar( self, type, defs, 0 );
    break;
  }
  if( mt > DAO_MT_EQ ) mt = DAO_MT_NOT;
  return mt;
}
short DaoAbsType_MatchTo( DaoAbsType *self, DaoAbsType *type, DMap *defs )
{
  DNode *node;
  void *pvoid[2];
  size_t mt;
  return DaoAbsType_MatchToX( self, type, defs );

  if( self ==NULL || type ==NULL ) return DAO_MT_NOT;
  if( self == type ) return DAO_MT_EQ;
  pvoid[0] = self;
  pvoid[1] = type;
  node = DMap_Find( dao_typing_cache, pvoid );
  if( node ) return node->value.pInt;

  mt = DaoAbsType_MatchToX( self, type, defs );
  if( ((self->attrib|type->attrib) & DAO_TYPE_NOTDEF) ==0 ){
#ifdef DAO_WITH_THREAD
    DMutex_Lock( & dao_typing_mutex );
#endif
    MAP_Insert( dao_typing_cache, pvoid, mt );
#ifdef DAO_WITH_THREAD
    DMutex_Unlock( & dao_typing_mutex );
#endif
  }
  return mt;
}
short DaoAbsType_MatchValue( DaoAbsType *self, DValue value, DMap *defs )
{
  ullong_t flags = (1<<DAO_UDF)|(1<<DAO_ANY)|(1<<DAO_INITYPE);
  DaoAbsType *tp;
  short i, mt, mt2, it1=0, it2=0;
  if( self == NULL ) return DAO_MT_NOT;
  mt = dao_type_matrix[value.t][self->tid];
  switch( mt ){
  case DAO_MT_NOT : case DAO_MT_ANYUDF : case DAO_MT_ANY : case DAO_MT_EQ :
  case DAO_MT_UDF :
  case DAO_MT_INIT :
    /* TODO : two rounds type inferring? */
    if( defs ){
      DNode *node = NULL;
      if( defs ) node = MAP_Find( defs, self );
      if( node ) return DaoAbsType_MatchValue( node->value.pAbtp, value, defs );
    }
    return mt;
  case DAO_MT_SUB :
    if( value.t <= DAO_DOUBLE ) return mt;
  default : break;
  }
  if( self->nested ){
    if( self->nested->size ) it1 = self->nested->items.pAbtp[0]->tid;
    if( self->nested->size >1 ) it2 = self->nested->items.pAbtp[1]->tid;
  }
  switch( value.t ){
  case DAO_ARRAY : 
    if( value.v.array->size == 0 ) return DAO_MT_EQ;
    tp = value.v.array->unitype;
    if( tp == self ) return DAO_MT_EQ;
    if( tp ) return DaoAbsType_MatchTo( tp, self, defs );
    if( it1 == DAO_UDF ) return DAO_MT_UDF;
    if( it1 == DAO_ANY ) return DAO_MT_ANY;
    if( it1 == DAO_INITYPE ) return DAO_MT_INIT;
    break;
  case DAO_LIST :
    if( value.v.list->items->size == 0 ) return DAO_MT_EQ;
    tp = value.v.list->unitype;
    if( tp == self ) return DAO_MT_EQ;
    if( tp ) return DaoAbsType_MatchTo( tp, self, defs );
    if( it1 == DAO_UDF ) return DAO_MT_UDF;
    if( it1 == DAO_ANY ) return DAO_MT_ANY;
    if( it1 == DAO_INITYPE ) return DAO_MT_INIT;
    break;
  case DAO_MAP : 
    if( value.v.map->items->size == 0 ) return DAO_MT_EQ;
    tp = value.v.map->unitype;
    if( tp == self ) return DAO_MT_EQ;
    if( tp ) return DaoAbsType_MatchTo( tp, self, defs );
    if( ((1<<it1)&flags) && ((1<<it2)&flags) ){
      if( it1 == DAO_UDF || it2 == DAO_UDF ) return DAO_MT_UDF;
      if( it1 == DAO_INITYPE || it2 == DAO_INITYPE ) return DAO_MT_INIT;
      if( it1 == DAO_ANY || it2 == DAO_ANY ) return DAO_MT_ANY;
    }
    break;
  case DAO_TUPLE :
    tp = value.v.tuple->unitype;
    if( tp == self ) return DAO_MT_EQ;
    if( tp ){
      return DaoAbsType_MatchTo( tp, self, defs );
    }else if( value.v.tuple->items->size != self->nested->size ){
      return DAO_MT_NOT;
    }else{
      mt = DAO_MT_EQ;
      for(i=0; i<self->nested->size; i++){
        tp = self->nested->items.pAbtp[i];
        if( tp->tid == DAO_PAR_NAMED ) tp = tp->X.abtype;

        /* for C functions that returns a tuple:
         * the tuple may be assigned to a context value before
         * its values are set properly! */
        if( value.v.tuple->items->data[i].t == 0 ) continue;

        mt2 = DaoAbsType_MatchValue( tp, value.v.tuple->items->data[i], defs );
        if( mt2 < mt ) mt = mt2;
        if( mt == 0 ) break;
      }
      return mt;
    }
    break;
  case DAO_FUNCTION :
  case DAO_ROUTINE :
    tp = value.v.routine->routType;
    if( tp == self ) return DAO_MT_EQ;
    if( tp ) return DaoAbsType_MatchTo( tp, self, NULL );
    break;
  case DAO_VMPROCESS :
    tp = value.v.vmp->abtype;
    if( tp == self ) return DAO_MT_EQ;
    if( tp ) return DaoAbsType_MatchTo( tp, self, defs );
    break;
  case DAO_CLASS :
    if( self->X.klass == value.v.klass ) return DAO_MT_EQ;
    if( DaoClass_ChildOf( value.v.klass, self->X.extra ) ) return DAO_MT_SUB;
    break;
  case DAO_OBJECT :
    if( self->X.klass == value.v.object->myClass ) return DAO_MT_EQ;
    if( DaoClass_ChildOf( value.v.object->myClass, self->X.extra ) ) return DAO_MT_SUB;
    break;
  case DAO_CDATA :
    if( self->typer == value.v.cdata->typer ){
      return DAO_MT_EQ;
    }else if( DaoCData_ChildOf( value.v.cdata->typer, self->typer ) ){
      return DAO_MT_SUB;
    }else{
      return DAO_MT_NOT;
    }
    break;
  case DAO_ABSTYPE :
    if( (DaoAbsType*)value.v.p == self ) return DAO_MT_EQ;
    return DaoAbsType_MatchTo( (DaoAbsType*)value.v.p, self, defs );
  case DAO_PAR_NAMED :
  case DAO_PAR_DEFAULT :
    if( value.v.pair->unitype == self ) return DAO_MT_EQ;
    return DaoAbsType_MatchTo( value.v.pair->unitype, self, defs );
  default :
    break;
  }
  return DAO_MT_NOT;
}
DaoAbsType* DaoAbsType_DefineTypes( DaoAbsType *self, DaoNameSpace *ns, DMap *defs )
{
  int i;
  DaoAbsType *nest;
  DaoAbsType *copy;
  DNode *node;

  if( self == NULL ) return NULL;
  if( DString_FindChar( self->name, '?', 0 ) == MAXSIZE 
      && DString_FindChar( self->name, '@', 0 ) == MAXSIZE ) return self;

  node = MAP_Find( defs, self );
  if( node ){
    if( DString_FindChar( node->value.pAbtp->name, '@', 0 ) != MAXSIZE ){
      return self;
    }
    return node->value.pAbtp;
  }

  if( self->tid ==DAO_INITYPE ){
    node = MAP_Find( defs, self );
    if( node == NULL ) return self;
    return DaoAbsType_DefineTypes( node->value.pAbtp, ns, defs );
  }else if( self->tid ==DAO_UDF ){
    node = MAP_Find( defs, self );
    copy = node ? node->value.pAbtp : NULL;
    if( copy ==0 || copy->tid ==DAO_ANY || copy->tid == DAO_UDF ) return self;
    return DaoAbsType_DefineTypes( copy, ns, defs );
  }else if( self->tid ==DAO_ANY ){
    return self;
  }

  copy = DaoAbsType_New( "", self->tid, NULL, NULL );
  copy->typer = self->typer;
  copy->attrib = self->attrib;
  copy->ffitype = self->ffitype;
  copy->attrib &= ~ DAO_TYPE_EMPTY; /* not any more empty */
  if( self->mapNames ){
    if( copy->mapNames ) DMap_Delete( copy->mapNames );
    copy->mapNames = DMap_Copy( self->mapNames );
  }
  if( self->fname ) copy->fname = DString_Copy( self->fname );
  if( self->nested ){
    if( copy->nested == NULL ) copy->nested = DArray_New(0);
    for(i=0; i<self->name->size; i++){
      char ch = self->name->mbs[i];
      if( ch < 'a' || ch > 'z' ) break;
      DString_AppendChar( copy->name, self->name->mbs[i] );
    }
    DString_AppendChar( copy->name, self->tid == DAO_PAR_GROUP ? '(' : '<' );
    for(i=0; i<self->nested->size; i++){
      nest = DaoAbsType_DefineTypes( self->nested->items.pAbtp[i], ns, defs );
      if( nest ==NULL ) goto DefFailed;
      DArray_Append( copy->nested, nest );
      DString_Append( copy->name, nest->name );
      if( i+1 <self->nested->size ) DString_AppendMBS( copy->name, "," );
    }
    GC_IncRCs( copy->nested );
    if( self->X.abtype && self->X.abtype->type == DAO_ABSTYPE ){
      DString_AppendMBS( copy->name, "=>" );
      copy->X.abtype = DaoAbsType_DefineTypes( self->X.abtype, ns, defs );
      if( copy->X.abtype ==NULL ) goto DefFailed;
      DString_Append( copy->name, copy->X.abtype->name );
    }
    DString_AppendChar( copy->name, self->tid == DAO_PAR_GROUP ? ')' : '>' );
  }else{
    if( self->X.abtype && self->X.abtype->type == DAO_ABSTYPE ){
      copy->X.abtype = DaoAbsType_DefineTypes( self->X.abtype, ns, defs );
    }else{
      copy->X.abtype = self->X.abtype;
    }
    if( self->tid == DAO_PAR_NAMED ){
      DString_Append( copy->name, self->fname );
      DString_AppendChar( copy->name, ':' );
      DString_Append( copy->name, copy->X.abtype->name );
    }else if( self->tid == DAO_PAR_DEFAULT ){
      DString_Append( copy->name, self->fname );
      DString_AppendChar( copy->name, '=' );
      DString_Append( copy->name, copy->X.abtype->name );
    }else{
      DString_Assign( copy->name, self->name );
    }
  }
  DaoAbsType_CheckName( copy );
  GC_IncRC( copy->X.abtype );
  node = DMap_Find( ns->abstypes, copy->name );
  if( node ){
    DaoAbsType_Delete( copy );
    return node->value.pAbtp;
  }else{
    DMap_Insert( ns->abstypes, copy->name, copy );
  }
  return copy;
DefFailed:
  printf( "redefine failed\n" );
  return NULL;
}
void DaoAbsType_RenewTypes( DaoAbsType *self, DaoNameSpace *ns, DMap *defs )
{
  DaoAbsType *tp = DaoAbsType_DefineTypes( self, ns, defs );
  DaoTypeBase *t;
  DaoBase *p;
  DString *s;
  DArray *a;
  DMap *m;
  int i;
  if( tp == self || tp == NULL ) return;
  i = self->tid; self->tid = tp->tid; tp->tid = i;
  s = self->name; self->name = tp->name; tp->name = s;
  p = self->X.extra; self->X.extra = tp->X.extra; tp->X.extra = p;
  t = self->typer;  self->typer = tp->typer;   tp->typer = t;
  a = self->nested; self->nested = tp->nested; tp->nested = a;
  m = self->mapNames; self->mapNames = tp->mapNames; tp->mapNames = m;
  DaoAbsType_Delete( tp );
}

/* also used for printing tuples */
static void DaoListCore_Print( DValue *self0, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
  DaoList *self = self0->v.list;
  DNode *node = NULL;
  DValue *data = NULL;
  size_t i, size = 0;
  char *lb = "{ ";
  char *rb = " }";
  if( self->type == DAO_TUPLE ){
    data = self0->v.tuple->items->data;
    size = self0->v.tuple->items->size;
    lb = "( ";
    rb = " )";
  }else{
    data = self->items->data;
    size = self->items->size;
  }

  if( cycData ) node = MAP_Find( cycData, self );
  if( node ){
    DaoStream_WriteMBS( stream, lb );
    DaoStream_WriteMBS( stream, "..." );
    DaoStream_WriteMBS( stream, rb );
    return;
  }
  if( cycData ) MAP_Insert( cycData, self, self );
  DaoStream_WriteMBS( stream, lb );

  for( i=0; i<size; i++ ){
    stream->useQuote = 1;
    DValue_Print( data[i], ctx, stream, cycData );
    stream->useQuote = 0;
    if( i != size-1 ) DaoStream_WriteMBS( stream, ", " );
  }
  DaoStream_WriteMBS( stream, rb );
  if( cycData ) MAP_Erase( cycData, self );
}
static void DaoListCore_GetItem( DValue *self0, DaoContext *ctx, DValue pid )
{
  DaoList *res, *self = self0->v.list;
  const size_t size = self->items->size;
  size_t i, start, end;
  int idtype;
  DArray *ids = MakeIndex( ctx, pid, size, & start, & end, & idtype );

  switch( idtype ){
  case IDX_NULL :
    res = DaoList_Copy( self, NULL );
    DaoContext_SetResult( ctx, (DaoBase*) res );
    break;
  case IDX_SINGLE :
    /* scalar duplicated in DaoMoveAC() */
    if( start >=0 && start <size )
      DaoContext_PutReference( ctx, self->items->data + start );
    else DaoContext_RaiseException( ctx, DAO_ERROR_INDEX_OUTOFRANGE, "" );
    break;
  case IDX_FROM :
    res = DaoContext_PutList( ctx );
    if( start >= self->items->size ) break;
    DVarray_Resize( res->items, self->items->size - start, daoNilValue );
    for(i=start; i<self->items->size; i++)
      DaoList_SetItem( res, self->items->data[i], i-start );
    break;
  case IDX_TO :
    res = DaoContext_PutList( ctx );
    if( end + 1 <0 ) break;
    if( end + 1 >= self->items->size ) end = self->items->size-1;
    DVarray_Resize( res->items, end +1, daoNilValue );
    for(i=0; i<=end; i++) DaoList_SetItem( res, self->items->data[i], i );
    break;
  case IDX_PAIR :
    res = DaoContext_PutList( ctx );
    if( end < start ) break;
    if( end + 1 >= self->items->size ) end = self->items->size-1;
    DVarray_Resize( res->items, end - start + 1, daoNilValue );
    for(i=start; i<=end; i++)
      DaoList_SetItem( res, self->items->data[i], i-start );
    break;
  case IDX_ALL :
    res = DaoList_Copy( self, NULL );
    DaoContext_SetResult( ctx, (DaoBase*) res );
    break;
  case IDX_MULTIPLE :
    res = DaoContext_PutList( ctx );
    DVarray_Resize( res->items, ids->size, daoNilValue );
    for(i=0; i<ids->size; i++ )
      DaoList_SetItem( res, self->items->data[ ids->items.pInt[i] ], i );
    DArray_Delete( ids );
    break;
  default : break;
  }
}
static void DaoListCore_SetItem( DValue *self0, DaoContext *ctx, DValue pid, DValue value )
{
  DaoList *self = self0->v.list;
  const size_t size = self->items->size;
  size_t i, start, end;
  int idtype;
  DArray *ids = MakeIndex( ctx, pid, size, & start, & end, & idtype );
  if( self->unitype == NULL ){
    /* a : tuple<string,list<int>> = ('',{});
       duplicating the constant to assign to a may not set the unitype properly */
    self->unitype = ctx->regTypes[ ctx->vmc->c ];
    GC_IncRC( self->unitype );
  }
  switch( idtype ){
  case IDX_NULL :
    for( i=0; i<size; i++ ) DaoList_SetItem( self, value, i );
    break;
  case IDX_SINGLE :
    if( start >=0 && start <size ) DaoList_SetItem( self, value, start );
    else DaoContext_RaiseException( ctx, DAO_ERROR_INDEX_OUTOFRANGE, "" );
    break;
  case IDX_FROM :
    for( i=start; i<self->items->size; i++ ) DaoList_SetItem( self, value, i );
    break;
  case IDX_TO :
    for( i=0; i<=end; i++ ) DaoList_SetItem( self, value, i );
    break;
  case IDX_PAIR :
    for( i=start; i<=end; i++ ) DaoList_SetItem( self, value, i );
    break;
  case IDX_ALL :
    for( i=0; i<self->items->size; i++ ) DaoList_SetItem( self, value, i );
    break;
  case IDX_MULTIPLE :
    DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "not supported" );
    DArray_Delete( ids );
    break;
  default : break;
  }
}
void DaoCopyValues( DValue *copy, DValue *data, int N, DaoContext *ctx, DMap *cycData )
{
  char t;
  int i;
  memcpy( copy, data, N*sizeof(DValue) );
  for(i=0; i<N; i++){
    t = data[i].t;
    if( t == DAO_COMPLEX ){
      copy[i].v.c = dao_malloc( sizeof(complex16) );
      copy[i].v.c->real = data[i].v.c->real;
      copy[i].v.c->imag = data[i].v.c->imag;
    }else if( t == DAO_STRING ){
      copy[i].v.s = DString_Copy( data[i].v.s );
    }else if( t == DAO_LONG ){
      copy[i].v.l = DLong_New();
      DLong_Move( copy[i].v.l, data[i].v.l );
    }else if( t >= DAO_ARRAY) {
      if( cycData ){
        /* deep copy */
        DaoTypeBase *typer = DValue_GetTyper( data[i] );
        copy[i] = typer->priv->Copy( data+i, ctx, cycData );
      }
      GC_IncRC( copy[i].v.p );
    }
  }
}
static DValue DaoListCore_Copy( DValue *dbase, DaoContext *ctx, DMap *cycData )
{
  DaoList *copy, *self = dbase->v.list;
  DValue *data = self->items->data;
  DValue res = daoNilList;

  if( cycData ){
    DNode *node = MAP_Find( cycData, self );
    if( node ){
      res.v.p = node->value.pBase;
      return res;
    }
  }

  copy = DaoList_New();
  res.v.list = copy;
  copy->subType = SUB_TYPE( self ); /* remove const state */
  copy->unitype = self->unitype;
  GC_IncRC( copy->unitype );
  if( cycData ) MAP_Insert( cycData, self, copy );

  DVarray_Resize( copy->items, self->items->size, daoNilValue );
  DaoCopyValues( copy->items->data, data, self->items->size, ctx, cycData );
  return res;
}
DaoList* DaoList_Copy( DaoList *self, DMap *cycData )
{
  DValue val = daoNilList;
  val.v.list = self;
  val = DaoListCore_Copy( & val, NULL, cycData );
  return val.v.list;
}
static DaoTypeCore listCore=
{
  0,
#ifdef DEV_HASH_LOOKUP
  NULL, NULL,
#endif
  NULL, NULL, NULL, 0, 0,
  DaoBase_GetField,
  DaoBase_SetField,
  DaoListCore_GetItem,
  DaoListCore_SetItem,
  DaoListCore_Print,
  DaoListCore_Copy,
};

static void DaoLIST_Insert( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  int size = self->items->size;
  DaoList_Insert( self, *p[1], p[2]->v.i );
  if( size == self->items->size )
    DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "value type" );
}
static void DaoLIST_Erase( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  size_t start = (size_t)p[1]->v.i;
  size_t n = (size_t)p[2]->v.i;
  size_t i;
  for(i=0; i<n; i++){
    if( self->items->size == start ) break;
    DaoList_Erase( self, start );
  }
}
static void DaoLIST_Clear( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  DaoList_Clear( self );
}
static void DaoLIST_Size( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  DaoContext_PutInteger( ctx, self->items->size );
}
static void DaoLIST_Resize( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  size_t size = (size_t)p[1]->v.i;
  size_t oldSize = self->items->size;
  size_t i;
  if( ( ctx->vmSpace->options & DAO_EXEC_SAFE ) && size > 1000 ){
    DaoContext_RaiseException( ctx, DAO_ERROR, 
        "not permitted to create large list in safe running mode" );
    return;
  }
  for(i=size; i<oldSize; i++ ) DaoList_Erase( self, size );
  DVarray_Resize( self->items, size, daoNilValue );
}
static int DaoList_CheckType( DaoList *self, DaoContext *ctx )
{
  size_t i, type;
  DValue *data = self->items->data;
  if( self->items->size == 0 ) return 0;
  type = data[0].t;
  for(i=1; i<self->items->size; i++){
    if( type != data[i].t ){
      DaoContext_RaiseException( ctx, DAO_WARNING, "need list of same type of elements" );
      return 0;
    }
  }
  if( type < DAO_INTEGER || type >= DAO_ARRAY ){
    DaoContext_RaiseException( ctx, DAO_WARNING, "need list of primitive data" );
    return 0;
  }
  return type;
}
static void DaoList_PutDefault( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  complex16 com = { 0.0, 0.0 };
  if( self->unitype && self->unitype->nested->size ){
    switch( self->unitype->nested->items.pAbtp[0]->tid ){
    case DAO_INTEGER : DaoContext_PutInteger( ctx, 0 ); break;
    case DAO_FLOAT   : DaoContext_PutFloat( ctx, 0.0 ); break;
    case DAO_DOUBLE  : DaoContext_PutDouble( ctx, 0.0 ); break;
    case DAO_STRING  : DaoContext_PutMBString( ctx, "" ); break;
    case DAO_COMPLEX : DaoContext_PutComplex( ctx, com ); break;
    default : DaoContext_SetResult( ctx, NULL ); break;
    }
  }else{
    DaoContext_SetResult( ctx, NULL );
  }
}
static void DaoLIST_Max( DaoContext *ctx, DValue *p[], int N )
{
  DaoTuple *tuple = DaoContext_PutTuple( ctx );
  DaoList *self = p[0]->v.list;
  DValue res, *data = self->items->data;
  size_t i, imax, type, size = self->items->size;

  tuple->items->data[1].t = DAO_INTEGER;
  tuple->items->data[1].v.i = -1;
  type = DaoList_CheckType( self, ctx );
  if( type == 0 ){
    /* DaoList_PutDefault( ctx, p, N ); */
    return;
  }
  imax = 0;
  res = data[0];
  for(i=1; i<size; i++){
    if( DValue_Compare( res, data[i] ) <0 ){
      imax = i;
      res = data[i];
    }
  }
  tuple->items->data[1].v.i = imax;
  DaoTuple_SetItem( tuple, res, 0 );
}
static void DaoLIST_Min( DaoContext *ctx, DValue *p[], int N )
{
  DaoTuple *tuple = DaoContext_PutTuple( ctx );
  DaoList *self = p[0]->v.list;
  DValue res, *data = self->items->data;
  size_t i, imin, type, size = self->items->size;

  tuple->items->data[1].t = DAO_INTEGER;
  tuple->items->data[1].v.i = -1;
  type = DaoList_CheckType( self, ctx );
  if( type == 0 ){
    /* DaoList_PutDefault( ctx, p, N ); */
    return;
  }
  imin = 0;
  res = data[0];
  for(i=1; i<size; i++){
    if( DValue_Compare( res, data[i] ) >0 ){
      imin = i;
      res = data[i];
    }
  }
  tuple->items->data[1].v.i = imin;
  DaoTuple_SetItem( tuple, res, 0 );
}
static void DaoLIST_Sum( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  size_t i, type, size = self->items->size;
  DValue *data = self->items->data;
  type = DaoList_CheckType( self, ctx );
  if( type == 0 ){
    DaoList_PutDefault( ctx, p, N );
    return;
  }
  switch( type ){
  case DAO_INTEGER :
    {
      dint res = 0;
      for(i=0; i<size; i++) res += data[i].v.i;
      DaoContext_PutInteger( ctx, res );
      break;
    }
  case DAO_FLOAT :
    {
      float res = 0.0;
      for(i=0; i<size; i++) res += data[i].v.f;
      DaoContext_PutFloat( ctx, res );
      break;
    }
  case DAO_DOUBLE :
    {
      double res = 0.0;
      for(i=0; i<size; i++) res += data[i].v.d;
      DaoContext_PutDouble( ctx, res );
      break;
    }
  case DAO_COMPLEX :
    {
      complex16 res = { 0.0, 0.0 };
      for(i=0; i<self->items->size; i++) COM_IP_ADD( res, data[i].v.c[0] );
      DaoContext_PutComplex( ctx, res );
      break;
    }
  case DAO_STRING :
    {
      DValue value = daoNilString;
      DString *m = DString_Copy( data[0].v.s );
      value.v.s = m;
      for(i=1; i<size; i++) DString_Append( m, data[i].v.s );
      DaoContext_PutValue( ctx, value );
      DString_Delete( m );
      break;
    }
  default : break;
  }
}
static void DaoLIST_PushFront( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  int size = self->items->size;
  DaoList_PushFront( self, *p[1] );
  if( size == self->items->size )
    DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "value type" );
}
static void DaoLIST_PopFront( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  if( self->items->size == 0 ){
    DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "list is empty" );
    return;
  }
  DaoList_Erase( self, 0 );
}
static void DaoLIST_PushBack( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  int size = self->items->size;
  DaoList_Append( self, *p[1] );
  if( size == self->items->size )
    DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "value type" );
}
static void DaoLIST_PopBack( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  if( self->items->size == 0 ){
    DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "list is empty" );
    return;
  }
  DaoList_Erase( self, self->items->size -1 );
}
static void DaoLIST_Front( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  if( self->items->size == 0 ){
    DaoContext_SetResult( ctx, & nil );
    DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "list is empty" );
    return;
  }
  DaoContext_PutReference( ctx, self->items->data );
}
static void DaoLIST_Top( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  if( self->items->size == 0 ){
    DaoContext_SetResult( ctx, & nil );
    DaoContext_RaiseException( ctx, DAO_ERROR_VALUE, "list is empty" );
    return;
  }
  DaoContext_PutReference( ctx, self->items->data + (self->items->size -1) );
}
/* Quick Sort.
 * Adam Drozdek: Data Structures and Algorithms in C++, 2nd Edition.
 */
static int 
Compare( DaoContext *ctx, int entry, int reg0, int reg1, int res, DValue v0, DValue v1 )
{
  DValue **locs = ctx->regValues;

  DValue_SimpleMove( v0, locs[ reg0 ] );
  DValue_SimpleMove( v1, locs[ reg1 ] );
  DaoVmProcess_ExecuteSection( ctx->process, entry );
  return DValue_GetInteger( * ctx->regValues[ res ] );
}
static void 
PartialQuickSort( DaoContext *ctx, int entry, int r0, int r1, int rr,
    DValue *data, int first, int last, int part )
{
  int lower=first+1, upper=last;
  DValue val;
  DValue pivot;
  if( first >= last ) return;
  val = data[first];
  data[first] = data[ (first+last)/2 ];
  data[ (first+last)/2 ] = val;
  pivot = data[ first ];

  while( lower <= upper ){
    while( lower < last && Compare( ctx, entry, r0, r1, rr, data[lower], pivot ) ) lower ++;
    while( upper > first && Compare( ctx, entry, r0, r1, rr, pivot, data[upper] ) ) upper --;
    if( lower < upper ){
      val = data[lower];
      data[lower] = data[upper];
      data[upper] = val;
      upper --;
    }
    lower ++;
  }
  val = data[first];
  data[first] = data[upper];
  data[upper] = val;
  if( first < upper-1 ) PartialQuickSort( ctx, entry, r0, r1, rr, data, first, upper-1, part );
  if( upper >= part ) return;
  if( upper+1 < last ) PartialQuickSort( ctx, entry, r0, r1, rr, data, upper+1, last, part );
}
void QuickSort( IndexValue *data, int first, int last, int part, int asc )
{
  int lower=first+1, upper=last;
  IndexValue val;
  DValue pivot;
  if( first >= last ) return;
  val = data[first];
  data[first] = data[ (first+last)/2 ];
  data[ (first+last)/2 ] = val;
  pivot = data[ first ].value;

  while( lower <= upper ){
    if( asc ){
      while( lower < last && DValue_Compare( data[lower].value, pivot ) <0 ) lower ++;
      while( upper > first && DValue_Compare( pivot, data[upper].value ) <0 ) upper --;
    }else{
      while( lower < last && DValue_Compare( data[lower].value, pivot ) >0 ) lower ++;
      while( upper > first && DValue_Compare( pivot, data[upper].value ) >0 ) upper --;
    }
    if( lower < upper ){
      val = data[lower];
      data[lower] = data[upper];
      data[upper] = val;
      upper --;
    }
    lower ++;
  }
  val = data[first];
  data[first] = data[upper];
  data[upper] = val;
  if( first < upper-1 ) QuickSort( data, first, upper-1, part, asc );
  if( upper >= part ) return;
  if( upper+1 < last ) QuickSort( data, upper+1, last, part, asc );
}
static void DaoLIST_Rank( DaoContext *ctx, DValue *p[], int npar, int asc )
{
  DaoList *list = p[0]->v.list;
  DaoList *res = DaoContext_PutList( ctx );
  IndexValue *data;
  DValue *items = list->items->data;
  DValue *ids;
  dint part = p[1]->v.i;
  size_t i, N;

  N = list->items->size;
  DVarray_Resize( res->items, N, daoZeroInt );
  ids  = res->items->data;
  for(i=0; i<N; i++) ids[i].v.i = i;
  if( N < 2 ) return;
  if( part ==0 ) part = N;
  data = dao_malloc( N * sizeof( IndexValue ) );
  for(i=0; i<N; i++){
    data[i].index = i;
    data[i].value = items[i];
  }
  QuickSort( data, 0, N-1, part, asc );
  for(i=0; i<N; i++) ids[i].v.i = data[i].index;
  dao_free( data );
}
static void DaoLIST_Ranka( DaoContext *ctx, DValue *p[], int npar )
{
  DaoLIST_Rank( ctx, p, npar, 1 );
}
static void DaoLIST_Rankd( DaoContext *ctx, DValue *p[], int npar )
{
  DaoLIST_Rank( ctx, p, npar, 0 );
}
static void DaoLIST_Sort( DaoContext *ctx, DValue *p[], int npar, int asc )
{
  DaoList *list = p[0]->v.list;
  IndexValue *data;
  DValue *items = list->items->data;
  dint part = p[1]->v.i;
  size_t i, N;

  DaoContext_PutReference( ctx, p[0] );
  N = list->items->size;
  if( N < 2 ) return;
  if( part ==0 ) part = N;
  data = dao_malloc( N * sizeof( IndexValue ) );
  for(i=0; i<N; i++){
    data[i].index = i;
    data[i].value = items[i];
  }
  QuickSort( data, 0, N-1, part, asc );
  for(i=0; i<N; i++) items[i] = data[i].value;
  dao_free( data );
}
static void DaoLIST_Sorta( DaoContext *ctx, DValue *p[], int npar )
{
  DaoLIST_Sort( ctx, p, npar, 1 );
}
static void DaoLIST_Sortd( DaoContext *ctx, DValue *p[], int npar )
{
  DaoLIST_Sort( ctx, p, npar, 0 );
}
void DaoContext_Sort( DaoContext *ctx, DaoVmCode *vmc, int index, int entry, int last )
{
  DaoList *list = NULL;
  DValue *items = NULL;
  DValue param = *ctx->regValues[ vmc->b ];
  int reg0 = index + 1;
  int reg1 = index + 2;
  int nsort = 0;
  size_t N = 0;
  if( param.t == DAO_LIST ){
    list = param.v.list;
    items = list->items->data;
    nsort = N = list->items->size;
  }else if( param.t == DAO_TUPLE ){
    if( param.v.tuple->items->size != 2 ) goto ErrorParam;
    if( param.v.tuple->items->data[0].t != DAO_LIST ) goto ErrorParam;
    if( param.v.tuple->items->data[1].t != DAO_INTEGER ) goto ErrorParam;
    list = param.v.tuple->items->data[0].v.list;
    nsort = param.v.tuple->items->data[1].v.i;
    items = list->items->data;
    N = list->items->size;
  }else{
    goto ErrorParam;
  }
  DaoContext_PutResult( ctx, (DaoBase*) list );
  PartialQuickSort( ctx, entry, reg0, reg1, last, items, 0, N-1, nsort );
  return;
ErrorParam :
  DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "invalid parameter for sort()" );
}
static void DaoLIST_Iter( DaoContext *ctx, DValue *p[], int N )
{
  DaoList *self = p[0]->v.list;
  DaoTuple *tuple = p[1]->v.tuple;
  DValue *data = tuple->items->data;
  DValue iter = DValue_NewInteger(0);
  data[0].v.i = self->items->size >0;
  DValue_Copy( & data[1], iter );
}
static DaoFuncItem listMeths[] = 
{
  { DaoLIST_Insert,     "insert( self :list<@T>, item : @T, pos=0 )" },
  { DaoLIST_Erase,      "erase( self :list<any>, start=0, n=1 )" },
  { DaoLIST_Clear,      "clear( self :list<any> )" },
  { DaoLIST_Size,       "size( self :list<any> )const=>int" },
  { DaoLIST_Resize,     "resize( self :list<any>, size :int )" },
  { DaoLIST_Max,        "max( self :list<@T> )const=>tuple<@T,int>" },
  { DaoLIST_Min,        "min( self :list<@T> )const=>tuple<@T,int>" },
  { DaoLIST_Sum,        "sum( self :list<@T> )const=>@T" },
  { DaoLIST_PushFront,  "pushfront( self :list<@T>, item :@T )" },
  { DaoLIST_PopFront,   "popfront( self :list<any> )" },
  { DaoLIST_PopFront,   "dequeue( self :list<any> )" },
  { DaoLIST_PushBack,   "append( self :list<@T>, item :@T )" },
  { DaoLIST_PushBack,   "pushback( self :list<@T>, item :@T )" },
  { DaoLIST_PushBack,   "enqueue( self :list<@T>, item :@T )" },
  { DaoLIST_PushBack,   "push( self :list<@T>, item :@T )" },
  { DaoLIST_PopBack,    "popback( self :list<any> )" },
  { DaoLIST_PopBack,    "pop( self :list<any> )" },
  { DaoLIST_Front,      "front( self :list<@T> )const=>@T" },
  { DaoLIST_Top,        "top( self :list<@T> )const=>@T" },
  { DaoLIST_Top,        "back( self :list<@T> )const=>@T" },
  { DaoLIST_Ranka,      "ranka( self :list<any>, k=0 )const=>list<int>" },
  { DaoLIST_Rankd,      "rankd( self :list<any>, k=0 )const=>list<int>" },
  { DaoLIST_Sorta,      "sorta( self :list<any>, k=0 )" },
  { DaoLIST_Sortd,      "sortd( self :list<any>, k=0 )" },
  { DaoLIST_Iter,       "__for_iterator__( self :list<any>, iter : for_iterator )" },
  { NULL, NULL }
};

int DaoList_Size( DaoList *self )
{
  return self->items->size;
}
DValue DaoList_Front( DaoList *self )
{
  if( self->items->size == 0 ) return daoNilValue;
  return self->items->data[0];
}
DValue DaoList_Back( DaoList *self )
{
  if( self->items->size == 0 ) return daoNilValue;
  return self->items->data[ self->items->size-1 ];
}
DValue DaoList_GetItem( DaoList *self, int pos )
{
  if( pos <0 || pos >= self->items->size ) return daoNilValue;
  return self->items->data[pos];
}
DaoTuple* DaoList_ToTuple( DaoList *self, DaoTuple *proto )
{
  /* XXX */
  return NULL;
}
void DaoList_SetItem( DaoList *self, DValue it, int pos )
{
  DValue *val;
  if( pos <0 || pos >= self->items->size ) return;
  val = self->items->data + pos;
  if( self->unitype && self->unitype->nested->size ){
    DValue_Move( it, val, self->unitype->nested->items.pAbtp[0] );
  }else{
    DValue_Copy( val, it );
  }
}
void DValue_SetAbsType( DValue *to, DaoAbsType *tp );
static int DaoList_CheckItemType( DaoList *self, DValue it )
{
  DaoAbsType *tp = self->unitype;
  int mt;
  if( tp ) tp = self->unitype->nested->items.pAbtp[0];
  if( tp == NULL ) return 1;
  mt = DaoAbsType_MatchValue( tp, it, NULL );
  if( tp->tid >= DAO_ARRAY && tp->tid <= DAO_TUPLE && mt != DAO_MT_EQ ) return 0;
  return mt;
}
static void DaoList_SetItemType( DaoList *self, DValue it )
{
  DaoAbsType *tp = self->unitype ? self->unitype->nested->items.pAbtp[0] : NULL;
  if( tp ) DValue_SetAbsType( & it, tp );
}

void DaoList_Insert( DaoList *self, DValue item, int pos )
{
  DaoAbsType *tp = self->unitype ? self->unitype->nested->items.pAbtp[0] : NULL;
  if( DaoList_CheckItemType( self, item ) ==0 ) return;
  DVarray_Insert( self->items, daoNilValue, pos );
  DValue_Move( item, self->items->data + pos, tp );
}
void DaoList_PushFront( DaoList *self, DValue item )
{
  DaoAbsType *tp = self->unitype ? self->unitype->nested->items.pAbtp[0] : NULL;
  if( DaoList_CheckItemType( self, item ) ==0 ) return;
  DVarray_PushFront( self->items, daoNilValue );
  DValue_Move( item, self->items->data, tp );
}
void DaoList_PushBack( DaoList *self, DValue item )
{
  DaoAbsType *tp = self->unitype ? self->unitype->nested->items.pAbtp[0] : NULL;
  if( DaoList_CheckItemType( self, item ) ==0 ) return;
  DVarray_PushBack( self->items, daoNilValue );
  DValue_Move( item, self->items->data + self->items->size - 1, tp );
}
void DaoList_ClearItem( DaoList *self, int i )
{
  if( i < 0 || i >= self->items->size ) return;
  switch( self->items->data[i].t ){
  case DAO_NIL :
  case DAO_INTEGER :
  case DAO_FLOAT   :
  case DAO_DOUBLE  :
    break;
  case DAO_COMPLEX :
    dao_free( self->items->data[i].v.c );
    break;
  case DAO_STRING  :
    DString_Clear( self->items->data[i].v.s );
    break;
  default : GC_DecRC( self->items->data[i].v.p );
  }
  self->items->data[i].v.d = 0.0;
  self->items->data[i].t = 0;
}
void DaoList_PopFront( DaoList *self )
{
  if( self->items->size ==0 ) return;
  DaoList_ClearItem( self, 0 );
  DVarray_PopFront( self->items );
}
void DaoList_PopBack( DaoList *self )
{
  if( self->items->size ==0 ) return;
  DaoList_ClearItem( self, self->items->size-1 );
  DVarray_PopBack( self->items );
}

DaoTypeBase listTyper=
{
  & listCore,
  "LIST",
  NULL,
  (DaoFuncItem*)listMeths,
  {0},
  (FuncPtrNew) DaoList_New,
  (FuncPtrDel) DaoList_Delete
};

DaoList* DaoList_New()
{
  DaoList *self = (DaoList*) dao_malloc( sizeof(DaoList) );
  DaoBase_Init( self, DAO_LIST );
  self->items = DVarray_New();
  self->unitype = NULL;
  return self;
}
void DaoList_Delete( DaoList *self )
{
  GC_DecRC( self->unitype );
  DaoList_Clear( self );
  DVarray_Delete( self->items );
  dao_free( self );
}
void DaoList_Clear( DaoList *self )
{
  DVarray_Clear( self->items );
}
void DaoList_Append( DaoList *self, DValue value )
{
  DaoList_PushBack( self, value );
}
void DaoList_Erase( DaoList *self, int pos )
{
  if( pos < 0 || pos >= self->items->size ) return;
  DaoList_ClearItem( self, pos );
  DVarray_Erase( self->items, pos, 1 );
}
void DaoList_FlatList( DaoList *self, DVarray *flat )
{
  DValue *data = self->items->data;
  int i;
  for(i=0; i<self->items->size; i++){
    if( data[i].t ==0 && data[i].v.p && data[i].v.p->type == DAO_LIST ){
      DaoList_FlatList( (DaoList*) data[i].v.p, flat );
    }else{
      DVarray_Append( flat, self->items->data[i] );
    }
  }
}

/**/
static void DaoMap_Print( DValue *self0, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
  DaoMap *self = self0->v.map;
  char *kvsym = self->items->hashing ? " : " : " => ";
  const int size = self->items->size;
  int i = 0;

  DNode *node = NULL;
  if( cycData ) node = MAP_Find( cycData, self );
  if( node ){
    DaoStream_WriteMBS( stream, "{ ... }" );
    return;
  }
  if( cycData ) MAP_Insert( cycData, self, self );
  DaoStream_WriteMBS( stream, "{ " );

  node = DMap_First( self->items );
  for( ; node!=NULL; node=DMap_Next(self->items,node) ){
    stream->useQuote = 1;
    DValue_Print( node->key.pValue[0], ctx, stream, cycData );
    DaoStream_WriteMBS( stream, kvsym );
    DValue_Print( node->value.pValue[0], ctx, stream, cycData );
    stream->useQuote = 0;
    if( i+1<size ) DaoStream_WriteMBS( stream, ", " );
    i++;
  }
  if( size==0 ) DaoStream_WriteMBS( stream, kvsym );
  DaoStream_WriteMBS( stream, " }" );
  if( cycData ) MAP_Erase( cycData, self );
}
static void DaoMap_GetItem( DValue *self0, DaoContext *ctx, DValue pid )
{
  DaoMap *self = self0->v.map;
  if( pid.t == DAO_PAIR ){
    DaoPair *pair = pid.v.pair;
    DaoMap *map = DaoContext_PutMap( ctx );
    DNode *node1 = DMap_First( self->items );
    DNode *node2 = NULL;
    if( pair->first.t ) node1 = MAP_FindMG( self->items, & pair->first );
    if( pair->second.t ) node2 = MAP_FindML( self->items, & pair->second );
    if( node2 ) node2 = DMap_Next(self->items, node2 );
    for(; node1 != node2; node1 = DMap_Next(self->items, node1 ) )
      DaoMap_Insert( map, node1->key.pValue[0], node1->value.pValue[0] );
  }else if( pid.t == DAO_TUPLE && pid.v.tuple->unitype == dao_type_for_iterator ){
    DaoTuple *iter = pid.v.tuple;
    DaoTuple *tuple = DaoTuple_New( 2 );
    DNode *node = (DNode*) iter->items->data[1].v.p;
    DaoContext_SetResult( ctx, (DaoBase*) tuple );
    if( node == NULL ) return;
    DValue_Copy( tuple->items->data, node->key.pValue[0] );
    DValue_Copy( tuple->items->data + 1, node->value.pValue[0] );
    node = DMap_Next( self->items, node );
    iter->items->data[0].v.i = node != NULL;
    iter->items->data[1].v.p = (DaoBase*) node;
  }else{
    DNode *node = MAP_Find( self->items, & pid );
    if( node ==NULL ){
      DaoContext_RaiseException( ctx, DAO_ERROR_KEY, NULL );
      return;
    }
    DaoContext_PutReference( ctx, node->value.pValue );
  }
}
extern DaoAbsType *dao_map_any;
static void DaoMap_SetItem( DValue *self0, DaoContext *ctx, DValue pid, DValue value )
{
  DaoMap *self = self0->v.map;
  DaoAbsType *tp = self->unitype;
  DaoAbsType *tp1=NULL, *tp2=NULL;
  if( tp == NULL ){
    /* a : tuple<string,map<string,int>> = ('',{=>});
       duplicating the constant to assign to a may not set the unitype properly */
    tp = ctx->regTypes[ ctx->vmc->c ];
    if( tp == NULL || tp->tid == 0 ) tp = dao_map_any;
    self->unitype = tp;
    GC_IncRC( tp );
  }
  if( tp ){
    if( tp->nested->size >=2 ){
      tp1 = tp->nested->items.pAbtp[0];
      tp2 = tp->nested->items.pAbtp[1];
    }else if( tp->nested->size >=1 ){
      tp1 = tp->nested->items.pAbtp[0];
    }
  }
  if( pid.t == DAO_PAIR ){
    DaoPair *pair = pid.v.pair;
    DNode *node1 = DMap_First( self->items );
    DNode *node2 = NULL;
    if( pair->first.t ) node1 = MAP_FindMG( self->items, & pair->first );
    if( pair->second.t ) node2 = MAP_FindML( self->items, & pair->second );
    if( node2 ) node2 = DMap_Next(self->items, node2 );
    for(; node1 != node2; node1 = DMap_Next(self->items, node1 ) )
      DValue_Move( value, node1->value.pValue, tp2 );
  }else{
    int c = DaoMap_Insert( self, pid, value );
    if( c ==1 ){
      DaoContext_RaiseException( ctx, DAO_ERROR_TYPE, "key not matching" );
    }else if( c ==2 ){
      DaoContext_RaiseException( ctx, DAO_ERROR_TYPE, "value not matching" );
    }
  }
}
static DValue DaoMap_Copy( DValue *self0, DaoContext *ctx, DMap *cycData )
{
  DaoMap *copy, *self = self0->v.map;
  DValue value = daoNilMap;
  DNode *node;

  if( cycData ){
    DNode *node = MAP_Find( cycData, self );
    if( node ){
      value.v.p = node->value.pBase;
      return value;
    }
  }

  copy = DaoMap_New( self->items->hashing );
  value.v.map = copy;
  copy->subType = SUB_TYPE( self ); /* remove const state */
  copy->unitype = self->unitype;
  GC_IncRC( copy->unitype );

  node = DMap_First( self->items );
  if( cycData ){
    MAP_Insert( cycData, self, copy );
    for( ; node!=NULL; node = DMap_Next(self->items, node) ){
      DValue key = DValue_MakeCopy( node->key.pValue[0], ctx, cycData );
      DValue value = DValue_MakeCopy( node->value.pValue[0], ctx, cycData );
      MAP_Insert( copy->items, & key, & value );
    }
  }else{
    for( ; node!=NULL; node = DMap_Next(self->items, node) ){
      MAP_Insert( copy->items, node->key.pValue, node->value.pValue );
    }
  }
  return value;
}
static DaoTypeCore mapCore =
{
  0,
#ifdef DEV_HASH_LOOKUP
  NULL, NULL,
#endif
  NULL, NULL, NULL, 0, 0,
  DaoBase_GetField,
  DaoBase_SetField,
  DaoMap_GetItem,
  DaoMap_SetItem,
  DaoMap_Print,
  DaoMap_Copy,
};

static void DaoMAP_Clear( DaoContext *ctx, DValue *p[], int N )
{
  DaoMap_Clear( p[0]->v.map );
}
static void DaoMAP_Erase( DaoContext *ctx, DValue *p[], int N )
{
  DMap *self = p[0]->v.map->items;
  DNode *node, *ml, *mg;
  DArray *keys;
  N --;
  switch( N ){
  case 0 :
    DMap_Clear( self ); break;
  case 1 :
    MAP_Erase( self, p[1] );
    break;
  case 2 :
    mg = MAP_FindMG( self, p[1] );
    ml = MAP_FindML( self, p[2] );
    if( mg ==NULL || ml ==NULL ) return;
    ml = DMap_Next( self, ml );
    keys = DArray_New(0);
    for(; mg != ml; mg=DMap_Next(self, mg)) DArray_Append( keys, mg->key.pVoid );
    while( keys->size ){
      MAP_Erase( self, keys->items.pVoid[0] );
      DArray_PopFront( keys );
    }
    DArray_Delete( keys );
    break;
  default : break;
  }
}
static void DaoMAP_Insert( DaoContext *ctx, DValue *p[], int N )
{
  DaoMap *self = p[0]->v.map;
  int c = DaoMap_Insert( self, *p[1], *p[2] );
  if( c ==1 ){
    DaoContext_RaiseException( ctx, DAO_ERROR_TYPE, "key not matching" );
  }else if( c ==2 ){
    DaoContext_RaiseException( ctx, DAO_ERROR_TYPE, "value not matching" );
  }
}
static void DaoMAP_Find( DaoContext *ctx, DValue *p[], int N )
{
  DaoMap *self = p[0]->v.map;
  DaoTuple *res = DaoTuple_New( 3 );
  DNode *node;
  DaoContext_SetResult( ctx, (DaoBase*) res );
  res->items->data[0].t = DAO_INTEGER;
  switch( (int)p[2]->v.i ){
  case -1 :
    node = MAP_FindML( self->items, p[1] );
    if( node == NULL ) return;
    res->items->data[0].v.i = 1;
    DValue_Copy( res->items->data + 1, node->key.pValue[0] );
    DValue_Copy( res->items->data + 2, node->value.pValue[0] );
    break;
  case 0  :
    node = MAP_Find( self->items, p[1] );
    if( node == NULL ) return;
    res->items->data[0].v.i = 1;
    DValue_Copy( res->items->data + 1, node->key.pValue[0] );
    DValue_Copy( res->items->data + 2, node->value.pValue[0] );
    break;
  case 1  :
    node = MAP_FindMG( self->items, p[1] );
    if( node == NULL ) return;
    res->items->data[0].v.i = 1;
    DValue_Copy( res->items->data + 1, node->key.pValue[0] );
    DValue_Copy( res->items->data + 2, node->value.pValue[0] );
    break;
  default : break;
  }
}
static void DaoMAP_Key( DaoContext *ctx, DValue *p[], int N )
{
  DaoMap *self = p[0]->v.map;
  DaoList *list = DaoContext_PutList( ctx );
  DNode *node, *ml=NULL, *mg=NULL;
  N --;
  switch( N ){
  case 0 :
    mg = DMap_First( self->items );
    break;
  case 1 :
    mg = MAP_FindMG( self->items, p[1] );
    break;
  case 2 :
    mg = MAP_FindMG( self->items, p[1] );
    ml = MAP_FindML( self->items, p[2] );
    if( ml == NULL ) return;
    ml = DMap_Next( self->items, ml );
    break;
  default: break;
  }
  if( mg == NULL ) return;
  for( node=mg; node != ml; node = DMap_Next( self->items, node ) )
    DaoList_Append( list, node->key.pValue[0] );
}
static void DaoMAP_Value( DaoContext *ctx, DValue *p[], int N )
{
  DaoMap *self = p[0]->v.map;
  DaoList *list = DaoContext_PutList( ctx );
  DNode *node, *ml=NULL, *mg=NULL;
  N --;
  switch( N ){
  case 0 :
    mg = DMap_First( self->items );
    break;
  case 1 :
    mg = MAP_FindMG( self->items, p[1] );
    break;
  case 2 :
    mg = MAP_FindMG( self->items, p[1] );
    ml = MAP_FindML( self->items, p[2] );
    if( ml ==NULL ) return;
    ml = DMap_Next( self->items, ml );
    break;
  default: break;
  }
  if( mg == NULL ) return;
  for( node=mg; node != ml; node = DMap_Next( self->items, node ) )
    DaoList_Append( list, node->value.pValue[0] );
}
static void DaoMAP_Has( DaoContext *ctx, DValue *p[], int N )
{
  DaoMap *self = p[0]->v.map;
  DaoContext_PutInteger( ctx, DMap_Find( self->items, p[1] ) != NULL );
}
static void DaoMAP_Size( DaoContext *ctx, DValue *p[], int N )
{
  DaoMap *self = p[0]->v.map;
  DaoContext_PutInteger( ctx, self->items->size );
}
static void DaoMAP_Iter( DaoContext *ctx, DValue *p[], int N )
{
  DaoMap *self = p[0]->v.map;
  DaoTuple *tuple = p[1]->v.tuple;
  DValue *data = tuple->items->data;
  data[0].v.i = self->items->size >0;
  data[1].t = 0;
  data[1].v.p = (DaoBase*) DMap_First( self->items );
}
static DaoFuncItem mapMeths[] = 
{
  { DaoMAP_Clear,  "clear( self :map<any,any> )" },
  { DaoMAP_Erase,  "erase( self :map<any,any> )" },
  { DaoMAP_Erase,  "erase( self :map<@K,@V>, from :@K )" },
  { DaoMAP_Erase,  "erase( self :map<@K,@V>, from :@K, to :@K )" },
  { DaoMAP_Insert, "insert( self :map<@K,@V>, key :@K, value :@V )" },
  /* 0:EQ; -1:MaxLess; 1:MinGreat */
  { DaoMAP_Find,   "find( self :map<@K,@V>, key :@K, type=0 )const=>tuple<int,@K,@V>" },
  { DaoMAP_Key,    "keys( self :map<@K,any> )const=>list<@K>" },
  { DaoMAP_Key,    "keys( self :map<@K,any>, from :@K )const=>list<@K>" },
  { DaoMAP_Key,    "keys( self :map<@K,any>, from :@K, to :@K )const=>list<@K>" },
  { DaoMAP_Value,  "values( self :map<any,@V> )const=>list<@V>" },
  { DaoMAP_Value,  "values( self :map<@K,@V>, from :@K )const=>list<@V>" },
  { DaoMAP_Value,  "values( self :map<@K,@V>, from :@K, to :@K )const=>list<@V>" },
  { DaoMAP_Has,    "has( self :map<@K,any>, key :@K )const=>int" },
  { DaoMAP_Size,   "size( self :map<any,any> )const=>int" },
  { DaoMAP_Iter,   "__for_iterator__( self :map<any,any>, iter : for_iterator )" },
  { NULL, NULL }
};
  
int DaoMap_Size( DaoMap *self )
{
  return self->items->size;
}
DValue DaoMap_GetValue( DaoMap *self, DValue key  )
{
  DNode *node = MAP_Find( self->items, & key );
  if( node ) return node->value.pValue[0];
  return daoNilValue;
}
void DaoMap_InsertMBS( DaoMap *self, const char *key, DValue value )
{
  DString *str = DString_New(1);
  DValue vkey = daoNilString;
  vkey.v.s = str;
  DString_SetMBS( str, key );
  DaoMap_Insert( self, vkey, value );
  DString_Delete( str );
}
void DaoMap_InsertWCS( DaoMap *self, const wchar_t *key, DValue value )
{
  DString *str = DString_New(0);
  DValue vkey = daoNilString;
  vkey.v.s = str;
  DString_SetWCS( str, key );
  DaoMap_Insert( self, vkey, value );
  DString_Delete( str );
}
void DaoMap_EraseMBS ( DaoMap *self, const char *key )
{
  DString *str = DString_New(1);
  DValue vkey = daoNilString;
  vkey.v.s = str;
  DString_SetMBS( str, key );
  DaoMap_Erase( self, vkey );
  DString_Delete( str );
}
void DaoMap_EraseWCS ( DaoMap *self, const wchar_t *key )
{
  DString *str = DString_New(0);
  DValue vkey = daoNilString;
  vkey.v.s = str;
  DString_SetWCS( str, key );
  DaoMap_Erase( self, vkey );
  DString_Delete( str );
}
DValue DaoMap_GetValueMBS( DaoMap *self, const char *key  )
{
  DNode *node;
  DString *str = DString_New(1);
  DValue vkey = daoNilString;
  vkey.v.s = str;
  DString_SetMBS( str, key );
  node = MAP_Find( self->items, & vkey );
  DString_Delete( str );
  if( node ) return node->value.pValue[0];
  return daoNilValue;
}
DValue DaoMap_GetValueWCS( DaoMap *self, const wchar_t *key  )
{
  DNode *node;
  DString *str = DString_New(0);
  DValue vkey = daoNilString;
  vkey.v.s = str;
  DString_SetWCS( str, key );
  node = MAP_Find( self->items, & vkey );
  DString_Delete( str );
  if( node ) return node->value.pValue[0];
  return daoNilValue;
}
DaoMap* DaoMap_New2(){ return DaoMap_New(0); }

DaoTypeBase mapTyper=
{
  & mapCore,
  "MAP",
  NULL,
  (DaoFuncItem*) mapMeths,
  {0},
  (FuncPtrNew)DaoMap_New2,
  (FuncPtrDel)DaoMap_Delete
};

DaoMap* DaoMap_New( int hashing )
{
  DaoMap *self = (DaoMap*) dao_malloc( sizeof( DaoMap ) );
  DaoBase_Init( self, DAO_MAP );
  self->items = hashing ? DHash_New( D_VALUE , D_VALUE ) : DMap_New( D_VALUE , D_VALUE );
  self->unitype = NULL;
  return self;
}
void DaoMap_Delete( DaoMap *self )
{
  GC_DecRC( self->unitype );
  DaoMap_Clear( self );
  DMap_Delete( self->items );
  dao_free( self );
}
void DaoMap_Clear( DaoMap *self )
{
  DMap_Clear( self->items );
}
int DaoMap_Insert( DaoMap *self, DValue key, DValue value )
{
  DaoAbsType *tp = self->unitype;
  DaoAbsType *tp1=NULL, *tp2=NULL;
  if( tp ){
    if( tp->nested->size >=2 ){
      tp1 = tp->nested->items.pAbtp[0];
      tp2 = tp->nested->items.pAbtp[1];
    }else if( tp->nested->size >=1 ){
      tp1 = tp->nested->items.pAbtp[0];
    }
  }
  /* type checking and setting */
  if( tp1 ){
    if( DaoAbsType_MatchValue( tp1, key, NULL ) ==0 ) return 1;
    DValue_SetAbsType( & key, tp1 );
  }
  if( tp2 ){
    if( DaoAbsType_MatchValue( tp2, value, NULL ) ==0 ) return 2;
    DValue_SetAbsType( & value, tp2 );
  }
  DMap_Insert( self->items, & key, & value );
  return 0;
}
void DaoMap_Erase( DaoMap *self, DValue key )
{
  MAP_Erase( self->items, & key );
}
DNode* DaoMap_First( DaoMap *self )
{
  return DMap_First(self->items);
}
DNode* DaoMap_Next( DaoMap *self, DNode *iter )
{
  return DMap_Next(self->items,iter);
}

/**/
DaoCData* DaoCData_New( DaoTypeBase *typer, void *data )
{
  DaoCData *self = (DaoCData*)dao_malloc( sizeof(DaoCData) );
  DaoBase_Init( self, DAO_CDATA );
  self->typer = typer;
  self->attribs = DAO_CDATA_FREE;
  self->data = data;
  self->buffer = NULL;
  self->memsize = 0;
  self->daoObject = NULL;
  self->size = 0;
  self->bufsize = 0;
  if( typer == NULL ){
    self->typer = & cdataTyper;
    self->buffer = data;
  }
  return self;
}
DaoCData* DaoCData_Wrap( DaoTypeBase *typer, void *data )
{
  DaoCData *self = DaoCData_New( typer, data );
  self->attribs = 0;
  return self;
}
static void DaoCData_Delete( DaoCData *self )
{
  DaoCDataCore *c = (DaoCDataCore*)self->typer->priv;
  if( self->attribs & DAO_CDATA_FREE ){
    if( self->buffer ){
      dao_free( self->buffer );
    }else if( self->data ){
      if( c->DelData )
        c->DelData( self->data );
      else if( self->bufsize > 0 )
        dao_free( self->data );
    }
  }
  dao_free( self );
}
int DaoCData_IsType( DaoCData *self, DaoTypeBase *typer )
{
  return DaoCData_ChildOf( self->typer, typer );
}

void DaoCData_SetData( DaoCData *self, void *data )
{
  self->data = data;
}
void DaoCData_SetBuffer( DaoCData *self, void *data, size_t size )
{
  self->data = data;
  self->buffer = data;
  self->size = size;
  self->bufsize = size;
}
void DaoCData_SetArray( DaoCData *self, void *data, size_t size, int memsize )
{
  self->data = data;
  self->buffer = data;
  self->memsize = memsize;
  self->size = size;
  self->bufsize = size;
  if( memsize ==0 ){
    self->data = ((void**)data)[0];
  }else{
    self->bufsize *= memsize;
  }
}
void* DaoCData_GetData( DaoCData *self )
{
  return self->data;
}
void* DaoCData_GetBuffer( DaoCData *self )
{
  return self->buffer;
}
void** DaoCData_GetData2( DaoCData *self )
{
  return & self->data;
}
DaoObject* DaoCData_GetObject( DaoCData *self )
{
  return (DaoObject*)self->daoObject;
}
DaoTypeBase* DaoCData_GetTyper(DaoCData *self )
{
  return self->typer;
}
static void DaoCData_GetField( DValue *self, DaoContext *ctx, DString *name )
{
  DaoTypeBase *typer = DValue_GetTyper( *self );
  DValue p = DaoFindValue2( typer, name );
  if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
    DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
    return;
  }
  if( p.t == 0 ){
    DaoFunction *func = NULL;
    DString_SetMBS( ctx->process->mbstring, "." );
    DString_Append( ctx->process->mbstring, name );
    p = DaoFindValue2( typer, ctx->process->mbstring );
    if( p.t == DAO_FUNCTION ) 
      func = (DaoFunction*)DRoutine_GetOverLoad( (DRoutine*)p.v.p, ctx->process, NULL, & self, 1, 0 );
    if( func == NULL ){
      DaoContext_RaiseException( ctx, DAO_ERROR_FIELD_NOTEXIST, "not exist" );
      return;
    }
    func->pFunc( ctx, & self, 1 );
  }else{
    DaoContext_PutValue( ctx, p );
  }
}
static void DaoCData_SetField( DValue *self, DaoContext *ctx, DString *name, DValue value )
{
  DaoTypeBase *typer = DValue_GetTyper( *self );
  DaoFunction *func = NULL;
  DValue val;
  DValue *p[2];
  p[0] = self;
  p[1] = & value;
  DString_SetMBS( ctx->process->mbstring, "." );
  DString_Append( ctx->process->mbstring, name );
  DString_AppendMBS( ctx->process->mbstring, "=" );
  if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
    DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
    return;
  }
  val = DaoFindValue2( typer, ctx->process->mbstring );
  if( val.t == DAO_FUNCTION )
    func = (DaoFunction*)DRoutine_GetOverLoad( (DRoutine*)val.v.p, ctx->process, self, p+1, 1, 0 );
  if( func == NULL ){
    DaoContext_RaiseException( ctx, DAO_ERROR_FIELD_NOTEXIST, name->mbs );
    return;
  }
  DaoFunction_SimpleCall( func, ctx, p, 2 );
}
static void DaoCData_GetItem( DValue *self0, DaoContext *ctx, DValue pid )
{
  DaoTypeBase *typer = DValue_GetTyper( *self0 );
  DaoCData *self = self0->v.cdata;

  if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
    DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
    return;
  }
  if( self->buffer && pid.t >=DAO_INTEGER && pid.t <=DAO_DOUBLE){
    int id = DValue_GetInteger( pid );
    self->data = self->buffer;
    if( self->size && ( id <0 || id > self->size ) ){
      DaoContext_RaiseException( ctx, DAO_ERROR_INDEX, "index out of range" );
      return;
    }
    if( self->memsize ){
      self->data = (void*)( (char*)self->buffer + id * self->memsize );
    }else{
      self->data = ((void**)self->buffer)[id];
    }
    DaoContext_PutValue( ctx, *self0 );
  }else{
    DaoFunction *func = NULL;
    DValue *p[ DAO_MAX_PARAM ];
    DValue val;
    int i, N = 1;
    p[0] = self0;
    if( pid.t == DAO_TUPLE && pid.v.tuple->unitype != dao_type_for_iterator ){
      N = pid.v.tuple->items->size;
      for(i=0; i<N; i++) p[i+1] = pid.v.tuple->items->data + i;
    }else{
      p[1] = & pid;
    }
    DString_SetMBS( ctx->process->mbstring, "[]" );
    val = DaoFindValue2( typer, ctx->process->mbstring );
    if( val.t == DAO_FUNCTION )
      func = (DaoFunction*)DRoutine_GetOverLoad( (DRoutine*)val.v.p, ctx->process, self0, p, N, 0 );
    if( func == NULL ){
      DaoContext_RaiseException( ctx, DAO_ERROR_FIELD_NOTEXIST, "" );
      return;
    }
    DaoFunction_SimpleCall( func, ctx, p, N+1 );
  }
}
static void DaoCData_SetItem( DValue *self0, DaoContext *ctx, DValue pid, DValue value )
{
  DaoTypeBase *typer = DValue_GetTyper( *self0 );
  DaoFunction *func = NULL;
  DValue *p[ DAO_MAX_PARAM+2 ];
  DValue val;
  int i, N = 1;

  DString_SetMBS( ctx->process->mbstring, "[]=" );
  if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
    DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
    return;
  }
  val = DaoFindValue2( typer, ctx->process->mbstring );
  if( val.t == DAO_FUNCTION ){
    p[0] = self0;
    p[1] = & value;
    if( pid.t == DAO_TUPLE ){
      N = pid.v.tuple->items->size;
      for(i=0; i<N; i++) p[i+2] = pid.v.tuple->items->data + i;
    }else{
      p[2] = & pid;
    }
    func = (DaoFunction*)DRoutine_GetOverLoad( (DRoutine*)val.v.p, ctx->process, self0, p, N+1, 0 );
  }
  if( func == NULL ){
    DaoContext_RaiseException( ctx, DAO_ERROR_FIELD_NOTEXIST, "" );
    return;
  }
  DaoFunction_SimpleCall( func, ctx, p, N+2 );
}

DaoCDataCore* DaoCDataCore_New()
{
  DaoCDataCore *self = (DaoCDataCore*) dao_calloc( 1, sizeof(DaoCDataCore) );
  self->GetField = DaoCData_GetField;
  self->SetField = DaoCData_SetField;
  self->GetItem = DaoCData_GetItem;
  self->SetItem = DaoCData_SetItem;
  self->Print = DaoBase_Print;
  self->Copy = DaoBase_Copy;
  return self;
}
void DaoTypeCData_SetMethods( DaoTypeBase *self )
{
  self->New = (FuncPtrNew)DaoCData_New;
  self->Delete = (FuncPtrDel)DaoCData_Delete;
}        

void DaoBuffer_Resize( DaoCData *self, int size )
{
  self->size = size;
  if( self->size + 1 >= self->bufsize ){
    self->bufsize = self->size + self->bufsize * 0.1 + 1;
    self->buffer = dao_realloc( self->buffer, self->bufsize );
  }else if( self->size < self->bufsize * 0.75 ){
    self->bufsize = self->bufsize * 0.8 + 1;
    self->buffer = dao_realloc( self->buffer, self->bufsize );
  }
  self->data = self->buffer;
}
static void DaoBuf_New( DaoContext *ctx, DValue *p[], int N )
{
  int size = p[0]->v.i;
  DaoCData *self = DaoCData_New( NULL, NULL );
  self->attribs |= DAO_CDATA_FREE;
  DaoContext_SetResult( ctx, (DaoBase*) self );
  if( ( ctx->vmSpace->options & DAO_EXEC_SAFE ) && size > 1000 ){
    DaoContext_RaiseException( ctx, DAO_ERROR, 
        "not permitted to create large buffer object in safe running mode" );
    return;
  }
  DaoBuffer_Resize( self, size );
}
static void DaoBuf_Size( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  DaoContext_PutInteger( ctx, self->size );
}
static void DaoBuf_Resize( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  if( ( ctx->vmSpace->options & DAO_EXEC_SAFE ) && p[1]->v.i > 1000 ){
    DaoContext_RaiseException( ctx, DAO_ERROR, 
        "not permitted to create large buffer object in safe running mode" );
    return;
  }
  DaoBuffer_Resize( self, p[1]->v.i );
}
static void DaoBuf_CopyData( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  DaoCData *cdat = p[1]->v.cdata;
  if( cdat->bufsize == 0 ){
    DaoContext_RaiseException( ctx, DAO_ERROR_PARAM, "invalid value" );
    return;
  }
  if( self->bufsize < cdat->size ) DaoBuffer_Resize( self, cdat->size );
  memcpy( self->buffer, cdat->buffer, cdat->size );
  self->size = cdat->size;
}
static void DaoBuf_GetString( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  DString *str = DaoContext_PutMBString( ctx, "" );
  if( (int)p[1]->v.i ){
    DString_Resize( str, self->size );
    memcpy( str->mbs, self->buffer, self->size );
  }else{
    DString_ToWCS( str );
    DString_Resize( str, self->size / sizeof( wchar_t ) );
    memcpy( str->wcs, self->buffer, str->size * sizeof( wchar_t ) );
  }
}
static void DaoBuf_SetString( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  DString *str = p[1]->v.s;
  if( str->mbs ){
    DaoBuffer_Resize( self, str->size );
    memcpy( self->buffer, str->mbs, str->size );
  }else{
    DaoBuffer_Resize( self, str->size * sizeof(wchar_t) );
    memcpy( self->buffer, str->wcs, str->size * sizeof(wchar_t) );
  }
}
static int DaoBuf_CheckRange( DaoCData *self, int i, int m, DaoContext *ctx )
{
  if( i*m >=0 && i*m < self->size ) return 0;
  DaoContext_RaiseException( ctx, DAO_ERROR_INDEX_OUTOFRANGE, "" );
  return 1;
}
static void DaoBuf_GetByte( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  int i = p[1]->v.i;
  int it = p[2]->v.i ? ((signed char*)self->buffer)[i] : ((unsigned char*)self->buffer)[i];
  if( DaoBuf_CheckRange( self, i, sizeof(char), ctx ) ) return;
  DaoContext_PutInteger( ctx, it );
}
static void DaoBuf_GetShort( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  int i = p[1]->v.i;
  int it = p[2]->v.i ? ((signed short*)self->buffer)[i] : ((unsigned short*)self->buffer)[i];
  if( DaoBuf_CheckRange( self, i, sizeof(short), ctx ) ) return;
  DaoContext_PutInteger( ctx, it );
}
static void DaoBuf_GetInt( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  int i = p[1]->v.i;
  int it = p[2]->v.i ? ((signed int*)self->buffer)[i] : ((unsigned int*)self->buffer)[i];
  if( DaoBuf_CheckRange( self, i, sizeof(int), ctx ) ) return;
  DaoContext_PutInteger( ctx, it );
}
static void DaoBuf_GetFloat( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  if( DaoBuf_CheckRange( self, p[1]->v.i, sizeof(float), ctx ) ) return;
  DaoContext_PutFloat( ctx, ((float*)self->buffer)[ p[1]->v.i ] );
}
static void DaoBuf_GetDouble( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  if( DaoBuf_CheckRange( self, p[1]->v.i, sizeof(double), ctx ) ) return;
  DaoContext_PutDouble( ctx, ((double*)self->buffer)[ p[1]->v.i ] );
}
static void DaoBuf_SetByte( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  if( DaoBuf_CheckRange( self, p[1]->v.i, sizeof(char), ctx ) ) return;
  if( p[3]->v.i )
    ((signed char*)self->buffer)[ p[1]->v.i ] = (signed char)p[2]->v.i;
  else
    ((unsigned char*)self->buffer)[ p[1]->v.i ] = (unsigned char)p[2]->v.i;
}
static void DaoBuf_SetShort( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  if( DaoBuf_CheckRange( self, p[1]->v.i, sizeof(short), ctx ) ) return;
  if( p[3]->v.i )
    ((signed short*)self->buffer)[ p[1]->v.i ] = (signed short)p[2]->v.i;
  else
    ((unsigned short*)self->buffer)[ p[1]->v.i ] = (unsigned short)p[2]->v.i;
}
static void DaoBuf_SetInt( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  if( DaoBuf_CheckRange( self, p[1]->v.i, sizeof(int), ctx ) ) return;
  if( p[3]->v.i )
    ((signed int*)self->buffer)[ p[1]->v.i ] = (signed int)p[2]->v.i;
  else
    ((unsigned int*)self->buffer)[ p[1]->v.i ] = (unsigned int)p[2]->v.i;
}
static void DaoBuf_SetFloat( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  if( DaoBuf_CheckRange( self, p[1]->v.i, sizeof(float), ctx ) ) return;
  ((float*)self->buffer)[ p[1]->v.i ] = p[2]->v.f;
}
static void DaoBuf_SetDouble( DaoContext *ctx, DValue *p[], int N )
{
  DaoCData *self = p[0]->v.cdata;
  if( DaoBuf_CheckRange( self, p[1]->v.i, sizeof(double), ctx ) ) return;
  ((double*)self->buffer)[ p[1]->v.i ] = p[2]->v.d;
}
static DaoFuncItem cptrMeths[]=
{
  {  DaoBuf_New,         "cdata( size=0 )=>cdata" },
  {  DaoBuf_Size,        "size( self : cdata )=>int" },
  {  DaoBuf_Resize,      "resize( self : cdata, size :int )" },
  {  DaoBuf_CopyData,    "copydata( self : cdata, buf : cdata )" },
  {  DaoBuf_GetString,   "getstring( self : cdata, mbs=1 )=>string" },
  {  DaoBuf_SetString,   "setstring( self : cdata, str : string )" },
  {  DaoBuf_GetByte,     "getbyte( self : cdata, index : int, signed=1 )=>int" },
  {  DaoBuf_GetShort,    "getshort( self : cdata, index : int, signed=1 )=>int" },
  {  DaoBuf_GetInt,      "getint( self : cdata, index : int, signed=1 )=>int" },
  {  DaoBuf_GetFloat,    "getfloat( self : cdata, index : int )=>float" },
  {  DaoBuf_GetDouble,   "getdouble( self : cdata, index : int )=>double" },
  {  DaoBuf_SetByte,     "setbyte( self : cdata, index : int, value: int, signed=1)" },
  {  DaoBuf_SetShort,    "setshort( self : cdata, index : int, value: int, signed=1)"},
  {  DaoBuf_SetInt,      "setint( self : cdata, index : int, value: int, signed=1)" },
  {  DaoBuf_SetFloat,    "setfloat( self : cdata, index : int, value : float )" },
  {  DaoBuf_SetDouble,   "setdouble( self : cdata, index : int, value : double )" },
  { NULL, NULL },
};

DaoTypeBase cdataTyper = 
{
  NULL,
  "cdata",
  NULL,
  (DaoFuncItem*) cptrMeths,
  {0},
  (FuncPtrNew)DaoCData_New,
  (FuncPtrDel)DaoCData_Delete
};
DaoCData cptrCData = { DAO_CDATA, DAO_DATA_CONST, { 0, 0 }, 1, 0, NULL,NULL,NULL, & cdataTyper, 0,0,0,0 };
/*
DaoException* DaoException_New()
{
  DaoException *self = dao_malloc( sizeof(DaoException) );
  self->fromLine = 0;
  self->toLine = 0;
  self->routName = DString_New(1);
  self->fileName = DString_New(1);

  self->name = DString_New(1);
  self->content = NULL;

  return self;
}
DaoException* DaoException_New2( DaoBase *p )
{
  DaoException *self = DaoException_New();
  self->content = p;
  GC_IncRC( self->content );
  return self;
}
void DaoException_Delete( DaoException *self )
{
  GC_DecRC( self->content );
  dao_free( self->name );
  DString_Delete( self->routName );
  DString_Delete( self->fileName );
  dao_free( self );
}

static void dao_DaoException_GETF_name( Dao_Context *ctx, Dao_Base *p[], int n );
static void dao_DaoException_SETF_name( Dao_Context *ctx, Dao_Base *p[], int n );
static void dao_DaoException_GETF_content( Dao_Context *ctx, Dao_Base *p[], int n );
static void dao_DaoException_SETF_content( Dao_Context *ctx, Dao_Base *p[], int n );
static void dao_DaoException_New( Dao_Context *ctx, Dao_Base *p[], int n );
static void dao_DaoException_New22( Dao_Context *ctx, Dao_Base *p[], int n );

static Dao_FuncItem dao_DaoException_Meths[] =
{
  { dao_DaoException_GETF_name, "_GETF_name( self : DaoException )" },
  { dao_DaoException_SETF_name, "_SETF_name( self : DaoException, name : string)" },
  { dao_DaoException_GETF_content, "_GETF_content( self : DaoException )" },
  { dao_DaoException_SETF_content, "_SETF_content( self : DaoException, content : any)" },
  { dao_DaoException_New, "DaoException(  )" },
  { dao_DaoException_New22, "DaoException( content : any )" },
  { NULL, NULL }
};

DTypeCData dao_DaoException_Typer = 
{ NULL, 0, "DaoException", NULL, dao_DaoException_Meths, 
  { 0 }, NULL, DaoException_Delete, NULL };

static void dao_DaoException_GETF_name( Dao_Context *ctx, Dao_Base *p[], int n )
{
  DaoException* self = (DaoException*) (p[0]->v.cdata)->data;
  if( self->name->mbs )
    DaoContext_PutMBString( ctx, self->name->mbs );
  else
    DaoContext_PutWCString( ctx, self->name->wcs );
}
static void dao_DaoException_SETF_name( Dao_Context *ctx, Dao_Base *p[], int n )
{
  DaoException* self = (DaoException*) (p[0]->v.cdata)->data;
  DString *name = p[1]->v.s;
  DString_Assign( self->name, name );
}
static void dao_DaoException_GETF_content( Dao_Context *ctx, Dao_Base *p[], int n )
{
  DaoException* self = (DaoException*) (p[0]->v.cdata)->data;
  DaoContext_SetResult( ctx, self->content );
}
static void dao_DaoException_SETF_content( Dao_Context *ctx, Dao_Base *p[], int n )
{
  DaoException* self = (DaoException*) (p[0]->v.cdata)->data;
  GC_DecRC( self->content );
  self->content = p[1];
  GC_IncRC( self->content );
}
static void dao_DaoException_New( Dao_Context *ctx, Dao_Base *p[], int n )
{
  DaoException *self = (DaoException*)DaoException_New(  );
  DaoContext_PutCData( ctx, self, & dao_DaoException_Typer );
}
static void dao_DaoException_New22( Dao_Context *ctx, Dao_Base *p[], int n )
{
  DaoException *self = (DaoException*)DaoException_New2( p[0] );
  DaoContext_PutCData( ctx, self, & dao_DaoException_Typer );
}
*/

void DaoPair_Delete( DaoPair *self )
{
  DValue_Clear( & self->first );
  DValue_Clear( & self->second );
  GC_DecRC( self->unitype );
  dao_free( self );
}
static void DaoPair_Print( DValue *self0, DaoContext *ctx, DaoStream *stream, DMap *cycData )
{
  DaoPair *self = self0->v.pair;
  DaoStream_WriteMBS( stream, "( " );
  DValue_Print( self->first, ctx, stream, cycData );
  DaoStream_WriteMBS( stream, ", " );
  DValue_Print( self->second, ctx, stream, cycData );
  DaoStream_WriteMBS( stream, " )" );
}
static DValue DaoPair_Copy( DValue *self0, DaoContext *ctx, DMap *cycData )
{
  DaoPair *self = self0->v.pair;
  DValue copy = daoNilPair;
  copy.v.pair = DaoPair_New( daoNilValue, daoNilValue );
  DValue_Copy( & copy.v.pair->first, self->first );
  DValue_Copy( & copy.v.pair->second, self->second );
  return copy;
}
static DaoTypeCore pairCore=
{
  0,
#ifdef DEV_HASH_LOOKUP
  NULL, NULL,
#endif
  NULL, NULL, NULL, 0, 0,
  DaoBase_GetField,
  DaoBase_SetField,
  DaoBase_GetItem,
  DaoBase_SetItem,
  DaoPair_Print,
  DaoPair_Copy
};
DaoTypeBase pairTyper =
{
  & pairCore,
  "pair",
  NULL,
  NULL, {0},
  NULL,
  (FuncPtrDel) DaoPair_Delete,
};
DaoPair* DaoPair_New( DValue p1, DValue p2 )
{
  DaoPair *self = (DaoPair*)dao_malloc( sizeof(DaoPair) );
  DaoBase_Init( self, DAO_PAIR );
  self->unitype = NULL;
  self->first = self->second = daoNilValue;
  DValue_Copy( & self->first, p1 );
  DValue_Copy( & self->second, p2 );
  return self;
}

/* ---------------------
 * Dao Tuple 
 * ---------------------*/
static int DaoTuple_GetIndex( DaoTuple *self, DaoContext *ctx, DString *name )
{
  DaoAbsType *abtp = self->unitype;
  DNode *node = NULL;
  int id;
  if( abtp && abtp->mapNames ) node = MAP_Find( abtp->mapNames, name );
  if( node == NULL ){
    DaoContext_RaiseException( ctx, DAO_ERROR, "invalid field" );
    return -1;
  }
  id = node->value.pInt;
  if( id <0 || id >= self->items->size ){
    DaoContext_RaiseException( ctx, DAO_ERROR, "invalid tuple" );
    return -1;
  }
  return id;
}
static void DaoTupleCore_GetField( DValue *self0, DaoContext *ctx, DString *name )
{
  DaoTuple *self = self0->v.tuple;
  int id = DaoTuple_GetIndex( self, ctx, name );
  if( id <0 ) return;
  DaoContext_PutReference( ctx, self->items->data + id );
}
static void DaoTupleCore_SetField( DValue *self0, DaoContext *ctx, DString *name, DValue value )
{
  DaoTuple *self = self0->v.tuple;
  DaoAbsType *t, **type = self->unitype->nested->items.pAbtp;
  int id = DaoTuple_GetIndex( self, ctx, name );
  if( id <0 ) return;
  t = type[id];
  if( t->tid == DAO_PAR_NAMED ) t = t->X.abtype;
  if( DValue_Move( value, self->items->data + id, t ) ==0)
    DaoContext_RaiseException( ctx, DAO_ERROR, "type not matching" );
}
static void DaoTupleCore_GetItem( DValue *self0, DaoContext *ctx, DValue pid )
{
  DaoTuple *self = self0->v.tuple;
  int ec = DAO_ERROR_INDEX;
  if( pid.t == DAO_NIL ){
    ec = 0;
    /* return a copy. TODO */
  }else if( pid.t >= DAO_INTEGER && pid.t <= DAO_DOUBLE ){
    int id = DValue_GetInteger( pid );
    if( id >=0 && id < self->items->size ){
      DaoContext_PutReference( ctx, self->items->data + id );
      ec = 0;
    }else{
      ec = DAO_ERROR_INDEX_OUTOFRANGE;
    }
  }
  if( ec ) DaoContext_RaiseException( ctx, ec, "" );
}
static void DaoTupleCore_SetItem( DValue *self0, DaoContext *ctx, DValue pid, DValue value )
{
  DaoTuple *self = self0->v.tuple;
  DaoAbsType *t, **type = self->unitype->nested->items.pAbtp;
  int ec = 0;
  if( pid.t >= DAO_INTEGER && pid.t <= DAO_DOUBLE ){
    int id = DValue_GetInteger( pid );
    if( id >=0 && id < self->items->size ){
      t = type[id];
      if( t->tid == DAO_PAR_NAMED ) t = t->X.abtype;
      if( DValue_Move( value, self->items->data + id, t ) ==0 ) ec = DAO_ERROR_TYPE;
    }else{
      ec = DAO_ERROR_INDEX_OUTOFRANGE;
    }
  }else{
    ec = DAO_ERROR_INDEX;
  }
  if( ec ) DaoContext_RaiseException( ctx, ec, "" );
}
static DValue DaoTupleCore_Copy( DValue *self0, DaoContext *ctx, DMap *cycData )
{
  DaoTuple *copy, *self = self0->v.tuple;
  DValue *data = self->items->data;
  DValue res = daoNilTuple;

  if( cycData ){
    DNode *node = MAP_Find( cycData, self );
    if( node ){
      res.v.p = node->value.pBase;
      return res;
    }
  }

  copy = DaoTuple_New( self->items->size );
  res.v.tuple = copy;
  copy->subType = SUB_TYPE( self ); /* remove const state */
  copy->unitype = self->unitype;
  GC_IncRC( copy->unitype );
  if( cycData ) MAP_Insert( cycData, self, copy );

  DaoCopyValues( copy->items->data, data, self->items->size, ctx, cycData );
  return res;
}
static DaoTypeCore tupleCore=
{
  0,
#ifdef DEV_HASH_LOOKUP
  NULL, NULL,
#endif
  NULL, NULL, NULL, 0, 0,
  DaoTupleCore_GetField,
  DaoTupleCore_SetField,
  DaoTupleCore_GetItem,
  DaoTupleCore_SetItem,
  DaoListCore_Print,
  DaoTupleCore_Copy,
};
DaoTypeBase tupleTyper=
{
  & tupleCore,
  "tuple",
  NULL,
  NULL,
  {0},
  NULL,
  (FuncPtrDel) DaoTuple_Delete
};
DaoTuple* DaoTuple_New( int size )
{
  DaoTuple *self = (DaoTuple*) dao_malloc( sizeof(DaoTuple) );
  DaoBase_Init( self, DAO_TUPLE );
  self->items = DVaTuple_New( size, daoNilValue );
  self->unitype = NULL;
  return self;
}
void DaoTuple_Delete( DaoTuple *self )
{
  DVaTuple_Delete( self->items );
  GC_DecRC( self->unitype );
  dao_free( self );
}

int  DaoTuple_Size( DaoTuple *self )
{
  return self->items->size;
}
#if 0
DaoBase* DaoTuple_GetItem( DaoTuple *self, int pos )
{
}
DaoList* DaoTuple_ToList( DaoTuple *self )
{
}
#endif
void DaoTuple_SetItem( DaoTuple *self, DValue it, int pos )
{
  DValue *val;
  if( pos <0 || pos >= self->items->size ) return;
  val = self->items->data + pos;
  if( self->unitype && self->unitype->nested->size ){
    DaoAbsType *t = self->unitype->nested->items.pAbtp[pos];
    if( t->tid == DAO_PAR_NAMED ) t = t->X.abtype;
    DValue_Move( it, val, t );
  }else{
    DValue_Copy( val, it );
  }
}
DValue DaoTuple_GetItem( DaoTuple *self, int pos )
{
  if( pos <0 || pos >= self->items->size ) return daoNilValue;
  return self->items->data[pos];
}
