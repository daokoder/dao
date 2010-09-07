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

#ifndef DAO_STDTYPE_H
#define DAO_STDTYPE_H

#include"daoConst.h"
#include"daolib.h"
#include"daoBase.h"
#include"daoString.h"
#include"daoArray.h"
#include"daoMap.h"

#define SUB_TYPE( x )  ( (x)->subType & (0xFF>>2) )

#define DAO_DATA_COMMON uchar_t type, subType, gcState[2]; int refCount, cycRefCount

void DaoBase_Delete( void *obj );

DValue DaoFindValue( DaoTypeBase *typer, DString *name );
DValue DaoFindValueOnly( DaoTypeBase *typer, DString *name );
DaoFunction* DaoFindFunction( DaoTypeBase *typer, DString *name );
DaoFunction* DaoFindFunction2( DaoTypeBase *typer, const char *name );

DaoTypeBase* DValue_GetTyper( DValue self );

struct DaoTypeCore
{
	uint_t         attribs;
	DMap          *values;
	DMap          *methods;
	DaoType       *abtype;
	DaoNameSpace  *nspace;

	void (*GetField)( DValue *self, DaoContext *ctx, DString *name );
	void (*SetField)( DValue *self, DaoContext *ctx, DString *name, DValue value );
	void (*GetItem) ( DValue *self, DaoContext *ctx, DValue pid );
	void (*SetItem) ( DValue *self, DaoContext *ctx, DValue pid, DValue value );

	void (*Print)( DValue *self, DaoContext *ctx, DaoStream *stream, DMap *cycData );
	DValue (*Copy)(  DValue *self, DaoContext *ctx, DMap *cycData );
};

extern DaoTypeCore  baseCore;

struct DaoBase
{
	DAO_DATA_COMMON;
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

struct DaoList
{
	DAO_DATA_COMMON;

	DVarray  *items;
	DaoMap   *meta;
	DaoType  *unitype;
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
	DAO_DATA_COMMON;

	DMap     *items;
	DaoMap   *meta;
	DaoType  *unitype;
};

DaoMap* DaoMap_New( int hashing );
void DaoMap_Delete( DaoMap *self );
void DaoMap_Clear( DaoMap *self );

int DaoMap_Insert( DaoMap *self, DValue key, DValue value );
void DaoMap_Erase( DaoMap *self, DValue key );

struct DaoCDataCore
{
	uint_t         attribs;
	DMap          *values;
	DMap          *methods;
	DaoType       *abtype;
	DaoNameSpace  *nspace;

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
	DAO_DATA_COMMON;

	void *data;
	void *buffer;

	DaoMap      *meta;
	DaoObject   *daoObject;
	DaoTypeBase *typer;

	int attribs;
	int memsize; /* size of single C/C++ object */

	/* in case ::data is a memory buffer: */
	size_t size;
	size_t bufsize;
};

extern DaoTypeBase cdataTyper;
extern DaoCData cptrCData;


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
	DAO_DATA_COMMON;

	DValue first;
	DValue second;

	DaoType *unitype;
};
DaoPair* DaoPair_New( DValue v1, DValue v2 );

struct DaoTuple
{
	DAO_DATA_COMMON;

	DVaTuple  *items;
	DaoMap    *meta;
	DaoType   *unitype;
};
void DaoTuple_SetItem( DaoTuple *self, DValue it, int pos );

typedef struct IndexValue IndexValue;
struct IndexValue
{
	size_t  index;
	DValue  value;
};
void QuickSort( IndexValue *data, int first, int last, int part, int asc );

struct DaoException
{
	int       fromLine;
	int       toLine;
	DRoutine *routine;
	DArray   *callers;
	DArray   *lines;

	DString  *name;
	DString  *info;
	DValue    data;
};

DaoException* DaoException_New( DaoTypeBase *typer );
DaoException* DaoException_New2( DaoTypeBase *typer, DValue v );
void DaoException_Delete( DaoException *self );
void DaoException_Setup( DaoNameSpace *ns );

DaoTypeBase* DaoException_GetType( int type );

extern DaoTypeBase dao_Exception_Typer;

#endif
