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

#ifndef DAO_TYPE_H
#define DAO_TYPE_H

#include"daoConst.h"
#include"daolib.h"
#include"daoBase.h"
#include"daoString.h"
#include"daoArray.h"
#include"daoMap.h"

#define SUB_TYPE( x )  ( (x)->subType & (0xFF>>2) )

#define DAO_DATA_COMMON uchar_t type, subType, gcState[2]; int refCount, cycRefCount;

void DaoBase_Delete( void *obj );

DValue DaoFindValue( DaoTypeBase *typer, const char *name );
DaoFunction* DaoFindFunction( DaoTypeBase *typer, const char *name );
DValue DaoFindValue2( DaoTypeBase *typer, DString *name );
DaoFunction* DaoFindFunction2( DaoTypeBase *typer, DString *name );
DaoTypeBase* DValue_GetTyper( DValue self );

typedef struct DaoNamedValue DaoNamedValue;

struct DaoNamedValue
{
  DString  *name;
  DValue    value;
};

#define DEV_HASH_LOOKUP
struct DaoTypeCore
{
  uint_t           attribs;
  DaoCData        *host;
#ifdef DEV_HASH_LOOKUP
  DMap            *mapValues;
  DMap            *mapMethods;
#endif
  DaoNamedValue  **values;
  DaoFunction    **methods;
  unsigned short   valCount;
  unsigned short   methCount;
  
  void (*GetField)( DValue *self, DaoContext *ctx, DString *name );
  void (*SetField)( DValue *self, DaoContext *ctx, DString *name, DValue value );
  void (*GetItem) ( DValue *self, DaoContext *ctx, DValue pid );
  void (*SetItem) ( DValue *self, DaoContext *ctx, DValue pid, DValue value );
  
  void   (*Print)( DValue *self, DaoContext *ctx, DaoStream *stream, DMap *cycData );
  DValue (*Copy)(  DValue *self, DaoContext *ctx, DMap *cycData );
};

extern DaoTypeCore  baseCore;

struct DaoBase
{
  DAO_DATA_COMMON
};

extern DaoBase nil;

void DaoBase_Init( void *dbase, char type );
void DaoBase_ChangeState( void *dbase, char state, char add );

DaoTypeBase* DaoBase_GetTyper( DaoBase *p );

DaoBase* DaoBase_Duplicate( void *dbase );

void DaoBase_GetField( DValue *self, DaoContext *ctx, DString *name );
void DaoBase_SetField( DValue *self, DaoContext *ctx, DString *name, DValue value );
void DaoBase_GetItem( DValue *self, DaoContext *ctx, DValue pid );
void DaoBase_SetItem( DValue *self, DaoContext *ctx, DValue pid, DValue value );
void DaoBase_Print( DValue *self, DaoContext *ctx, DaoStream *stream, DMap *cycData );
DValue DaoBase_Copy( DValue *self, DaoContext *ctx, DMap *cycData );

void DaoBase_SafeGetField( DValue *self, DaoContext *ctx, DString *name );
void DaoBase_SafeSetField( DValue *self, DaoContext *ctx, DString *name, DValue value );

/* Dao abstract type:
 * type class for number, string, ... list<X>, ...
 * 
 * for core types: number, string, complex, list, map, array
 * eg:DaoAbsType.name = "int", "float", "string", ...
 *    DaoAbsType.count = 0; DaoAbsType.pbasic = NULL;
 *    DaoAbsType.basic = DAO_INTEGER, DAO_STRING, ...
 *
 * for Dao class and routine types:
 *    DaoAbsType.name = "foo", "bar", ...
 *    DaoAbsType.count = 0;
 *    DaoAbsType.basic = DAO_CLASS, DAO_CDATA,
 *    DaoAbsType.pbasic = the Dao class or C type
 *
 * for nested type: list<float>, map<string,float>, ...
 *    DaoAbsType.name = "list<float>", "map<string,float>", ...
 *    DaoAbsType.basic = DAO_LIST, DAO_MAP
 *    DaoAbsType.pbasic = NULL;
 *    DaoAbsType.count = 1, or 2, or more for routine and tuple etc. types
 *    DaoAbsType.nested[] = nested DaoAbsType(s) : X<nested[0],nested[1],...>
 *
 * for routine type: routine(float,string):float
 *    DaoAbsType.name = "routine<float,string=>float>"
 *    DaoAbsType.basic = DAO_ROUTINE
 *    DaoAbsType.pbasic = XXX
 *    DaoAbsType.count = parameter count + 1
 *    DaoAbsType.nested[] = parameter DaoAbsType(s) : (<nested[0],...) : returned
 *
 *    e.g.:
 *        routine<float=>?>: foo( a : float ){}
 *        routine<float=>float>: foo( a : float ) : float{}
 *        routine<a:float=>?>: foo( a : float ){}
 *        routine<a=float=>?>: foo( a = 1 ){}
 *        routine<a:float,b=string=>?>: foo( a : float, b="abc" ){}
 *        routine<a:float,b:?=>?>: foo( a : float, b ){}
 *        routine<a:float,b:?=>?>: foo( a : float, b : @b ){}
 *        routine<a:float,b:?=>?>: foo( a : float, b ) : @b{}
 *
 * for named parameter passing: name => value
 *    DaoAbsType.name = "pair<string,X>"
 *    DaoAbsType.basic = DAO_PARNAME
 *    DaoAbsType.count = 2
 *    DaoAbsType.nested[] = string, X
 *
 * for pair id: id1 : id2
 *    DaoAbsType.name = "pair<X,Y>"
 *    DaoAbsType.basic = DAO_PAIR
 *    DaoAbsType.count = 2
 *    DaoAbsType.nested[] = X, Y
 */
#define MAPF_OFFSET  12
#define MAPF_MASK    0xfff
struct DaoAbsType
{
  DAO_DATA_COMMON

  short         tid; /* type id */
  uchar_t       attrib;
  uchar_t       ffitype; /* for DaoCLoader module */
  DString      *name; /* type name */
  DString      *fname; /* field name, or parameter name */
  DArray       *nested;
  DMap         *mapNames;
  DaoTypeBase  *typer; /* TYPER of the represented type: built-in or C types */
  union {
    DaoBase    *extra;
    DaoClass   *klass;
    DaoCData   *cdata;
    DaoAbsType *abtype;
  }X; /* DaoClass, DaoCData or DaoAbsType for returned/named... */
};
extern DaoAbsType *dao_type_udf;
extern DaoAbsType *dao_array_bit;
extern DaoAbsType *dao_array_any;
extern DaoAbsType *dao_list_any;
extern DaoAbsType *dao_map_any;
extern DaoAbsType *dao_routine;
extern DaoAbsType *dao_type_for_iterator;

DaoAbsType* DaoAbsType_New( const char *name, short tid, DaoBase *pb, DArray *nest );
DaoAbsType* DaoAbsType_Copy( DaoAbsType *self );
void DaoAbsType_Delete( DaoAbsType *self );

void DaoAbsType_CheckName( DaoAbsType *self );

/* if "self" match to "type": */
short DaoAbsType_MatchTo( DaoAbsType *self, DaoAbsType *type, DMap *defs );
short DaoAbsType_MatchValue( DaoAbsType *self, DValue value, DMap *defs );
/* define @X */
DaoAbsType* DaoAbsType_DefineTypes( DaoAbsType *self, DaoNameSpace *ns, DMap *defs );
void DaoAbsType_RenewTypes( DaoAbsType *self, DaoNameSpace *ns, DMap *defs );

#define NESTYPE(t,i) ((t)->nested->items.pAbtp[i])

struct DaoList
{
  DAO_DATA_COMMON

  DVarray  *items;
  DaoAbsType *unitype;
};

DaoList* DaoList_New();
void DaoList_Delete( DaoList *self );
void DaoList_Clear( DaoList *self );

void DaoList_Append( DaoList *self, DValue it );
void DaoList_SetValue( DaoList *self, DValue it, int id );
void DaoList_Erase( DaoList *self, int id );
DValue DaoList_GetValue( DaoList *self, int id );
void DaoList_SetItem( DaoList *self, DValue it, int id );
void DaoList_Append( DaoList *self, DValue it );

DaoList* DaoList_Copy( DaoList *self, DMap *cycdata );

void DaoList_FlatList( DaoList *self, DVarray *flat );

struct DaoMap
{
  DAO_DATA_COMMON
  
  DMap *items;

  DaoAbsType *unitype;
};

DaoMap* DaoMap_New( int hashing );
void DaoMap_Delete( DaoMap *self );
void DaoMap_Clear( DaoMap *self );

int DaoMap_Insert( DaoMap *self, DValue key, DValue value );
void DaoMap_Erase( DaoMap *self, DValue key );

struct DaoCDataCore
{
  uint_t           attribs;
  DaoCData        *host;
#ifdef DEV_HASH_LOOKUP
  DMap            *mapValues;
  DMap            *mapMethods;
#endif
  DaoNamedValue  **values;
  DaoFunction    **methods;
  unsigned short   valCount;
  unsigned short   methCount;

  void (*GetField)( DValue *self, DaoContext *ctx, DString *name );
  void (*SetField)( DValue *self, DaoContext *ctx, DString *name, DValue value );
  void (*GetItem)(  DValue *self, DaoContext *ctx, DValue pid );
  void (*SetItem)(  DValue *self, DaoContext *ctx, DValue pid, DValue value );
  
  void   (*Print)( DValue *self, DaoContext *ctx, DaoStream *stream, DMap *cycData );
  DValue (*Copy)(  DValue *self, DaoContext *ctx, DMap *cycData );
  
  void*  (*NewData)();
  void   (*DelData)( void *data );
};
DaoCDataCore* DaoCDataCore_New();

enum{
  DAO_CDATA_FREE = 1
};

/* DaoCData stores a pointer to a C/C++ object or to a memory buffer:
 * XXX possible changes:
 * The following restriction should be imposed?
 * 1. If it is a memory buffer, the size and the memory pointer should
 *    not be mutable (in Dao scripts) after the creation of the cdata;
 *    they should be set at creation, but may be modified by C interfaces.
 *    Reason: resizing the buffer will cause the memory to be dao_reallocated
 *    which will introduce invalidated pointers, if the buffer is returned
 *    from a module written in C (e.g., a vector allocated by GSL module).
 * 2. Reading from the buffer is not limited by the "size" field, but writing
 *    to the buffer will be limited to the part smaller than "size". 
 *    If a module is generated automatically by the autobind tool, when a
 *    function need to return a memory buffer, the tool may not know the
 *    size of the buffer, and it can set it to zero, since the size of the
 *    buffer can be known from another place. Unlimited reading will allow
 *    the content of the buffer to be retrieved. Modifying a buffer outside
 *    of the "size" boundary is considered as dangerous, and should be forbidden.
 * 3. The "size" field should be respected even for reading, when the cdata
 *    is not a memory buffer, but an array of C/C++ objects. Because considering
 *    a pierce of memory beyond the "size" boundary as an C/C++ object is
 *    also dangerous, and should be forbidden.
 *
 * Other notes:
 * 1. "buffer" may store an array of C/C++ objects, and when an item is requested
 *    with the [] operator, "data" will be pointed to the requested one.
 * 2. The "attrib" field contains flags, if the DAO_CDATA_FREE flag is set in this
 *    field, this will indicate the memory pointed by "buffer"/"data" should be 
 *    freed when this cdata is reclaimed by GC. When "buffer" is not null, the 
 *    "data" field is a pointer with some offset from "buffer", so "buffer" should 
 *    be freed, not "data".
 */
struct DaoCData
{
  DAO_DATA_COMMON

  void *data;
  void *buffer;
  DaoObject *daoObject;
  DaoTypeBase *typer;

  int attribs;
  int memsize; /* size of single C/C++ object */

  /* in case ::data is a memory buffer: */
  size_t size;
  size_t bufsize;
};

extern DaoTypeBase cdataTyper;
extern DaoCData cptrCData;


/*
struct DaoException
{
  int    fromLine;
  int    toLine;

  DString  *routName;
  DString  *fileName;

  DString *name;
  DaoBase *content;
};

DaoException* DaoException_New();
DaoException* DaoException_New2( DaoBase *p );
void DaoException_Delete( DaoException *self );

extern DaoTypeCData dao_DaoException_Typer;
*/

/* DaoPair is not data type for general use, it is mainly used for index pair,
 * and object-method pair.
 *
 * class_instance.method => DaoPair
 * cdata_instance.method => DaoPair
 *
 * class.method => DRoutine
 * cdata_type.method => DRoutine
 */

struct DaoPair
{
  DAO_DATA_COMMON

  DValue first;
  DValue second;

  DaoAbsType *unitype;
};
DaoPair* DaoPair_New( DValue v1, DValue v2 );

struct DaoTuple
{
  DAO_DATA_COMMON

  DVaTuple   *items;
  DaoAbsType *unitype;
};
void DaoTuple_SetItem( DaoTuple *self, DValue it, int pos );

typedef struct IndexValue IndexValue;
struct IndexValue
{
  size_t  index;
  DValue  value;
};
void QuickSort( IndexValue *data, int first, int last, int part, int asc );

#endif
