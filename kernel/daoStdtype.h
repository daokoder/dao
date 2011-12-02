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

#ifndef DAO_STDTYPE_H
#define DAO_STDTYPE_H

#include"daoConst.h"
#include"daoBase.h"
#include"daoString.h"
#include"daoArray.h"
#include"daoMap.h"

#define DAO_DATA_CORE    uchar_t type, subtype, trait, marks; int refCount
#define DAO_DATA_COMMON  DAO_DATA_CORE; int cycRefCount

void DaoValue_Init( void *dbase, char type );

struct DaoNone
{
	DAO_DATA_CORE;
};
DAO_DLL extern DaoValue *dao_none_value;
DAO_DLL DaoNone* DaoNone_New();

struct DaoInteger
{
	DAO_DATA_CORE;

	dint value;
};
struct DaoFloat
{
	DAO_DATA_CORE;

	float value;
};
struct DaoDouble
{
	DAO_DATA_CORE;

	double value;
};
struct DaoComplex
{
	DAO_DATA_CORE;

	complex16 value;
};
struct DaoLong
{
	DAO_DATA_CORE;

	DLong  *value;
};
DAO_DLL DaoLong* DaoLong_Copy( DaoLong *self );
DAO_DLL void DaoLong_Delete( DaoLong *self );

struct DaoString
{
	DAO_DATA_CORE;

	DString  *data;
};
DAO_DLL DaoString* DaoString_Copy( DaoString *self );
DAO_DLL void DaoString_Delete( DaoString *self );

/* Structure for symbol, enum and flag:
 * Storage modes:
 * Symbol: $AA => { type<$AA>, 0 }
 * Symbols: $AA + $BB => { type<$AA$BB>, 1|2 }
 * Enum: enum MyEnum{ AA=1, BB=2 }, MyEnum.AA => { type<MyEnum>, 1 }
 * Flag: enum MyFlag{ AA=1; BB=2 }, MyFlag.AA + MyFlag.BB => { type<MyFlag>, 1|2 }
 */
struct DaoEnum
{
	DAO_DATA_COMMON;

	DaoType  *etype;  /* type information structure */
	dint      value; /* value associated with the symbol(s) or flag(s) */
};

DAO_DLL DaoEnum* DaoEnum_New( DaoType *type, dint value );
DAO_DLL DaoEnum* DaoEnum_Copy( DaoEnum *self, DaoType *type );
DAO_DLL void DaoEnum_Delete( DaoEnum *self );
DAO_DLL void DaoEnum_MakeName( DaoEnum *self, DString *name );
DAO_DLL void DaoEnum_SetType( DaoEnum *self, DaoType *type );
DAO_DLL int DaoEnum_SetSymbols( DaoEnum *self, const char *symbols );
DAO_DLL int DaoEnum_SetValue( DaoEnum *self, DaoEnum *other, DString *enames );
DAO_DLL int DaoEnum_AddValue( DaoEnum *self, DaoEnum *other, DString *enames );
DAO_DLL int DaoEnum_RemoveValue( DaoEnum *self, DaoEnum *other, DString *enames );
DAO_DLL int DaoEnum_AddSymbol( DaoEnum *self, DaoEnum *s1, DaoEnum *s2, DaoNamespace *ns );
DAO_DLL int DaoEnum_SubSymbol( DaoEnum *self, DaoEnum *s1, DaoEnum *s2, DaoNamespace *ns );

struct DaoList
{
	DAO_DATA_COMMON;

	DArray    items;
	DaoType  *unitype;
};

DAO_DLL DaoList* DaoList_New();
DAO_DLL void DaoList_Delete( DaoList *self );
DAO_DLL void DaoList_Clear( DaoList *self );

DAO_DLL void DaoList_Erase( DaoList *self, size_t id );
DAO_DLL int DaoList_SetItem( DaoList *self, DaoValue *it, size_t id );
DAO_DLL int DaoList_Append( DaoList *self, DaoValue *it );

DAO_DLL DaoList* DaoList_Copy( DaoList *self, DMap *cycdata );

struct DaoMap
{
	DAO_DATA_COMMON;

	DMap     *items;
	DaoType  *unitype;
};

DAO_DLL DaoMap* DaoMap_New( int hashing );
DAO_DLL void DaoMap_Delete( DaoMap *self );
DAO_DLL void DaoMap_Clear( DaoMap *self );
DAO_DLL void DaoMap_Reset( DaoMap *self );

DAO_DLL int DaoMap_Insert( DaoMap *self, DaoValue *key, DaoValue *value );
DAO_DLL void DaoMap_Erase( DaoMap *self, DaoValue *key );


enum{
	DAO_CDATA_FREE = 1
};

/* DaoCdata stores a pointer to a C/C++ object or to a memory buffer:
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
struct DaoCdata
{
	DAO_DATA_COMMON;

	void  *data;
	union {
		void           *pVoid;
		signed   char  *pSChar;
		unsigned char  *pUChar;
		signed   short *pSShort;
		unsigned short *pUShort;
		signed   int   *pSInt;
		unsigned int   *pUInt;
		float          *pFloat;
		double         *pDouble;
	} buffer;

	DaoObject    *object;
	DaoType      *ctype;
	DaoTypeBase  *typer;

	uchar_t   attribs;
	uchar_t   extref;
	ushort_t  memsize; /* size of single C/C++ object */

	/* in case it is a memory buffer: */
	uint_t  size;
	uint_t  bufsize;
};

DAO_DLL extern DaoTypeBase cdataTyper;
extern DaoCdata cptrCdata;

DAO_DLL void DaoCdata_DeleteData( DaoCdata *self );
DAO_DLL int DaoCdata_ChildOf( DaoTypeBase *self, DaoTypeBase *super );

/* DaoNameValue is not data type for general use, it is mainly used for 
 * passing named parameters and fields: */
struct DaoNameValue
{
	DAO_DATA_COMMON;

	DString   *name;
	DaoValue  *value;
	DaoType   *unitype;
};
DaoNameValue* DaoNameValue_New( DString *name, DaoValue *value );

#define DAO_TUPLE_ITEMS 2

struct DaoTuple
{
	DAO_DATA_COMMON;

	int         size; /* packed with the previous field in 64-bits system; */
	DaoType    *unitype;
	DaoValue   *items[DAO_TUPLE_ITEMS]; /* the actual number of items is in ::size; */
	/* 2 is used instead of 1, for two reasons:
	 * A. most often used tuples have at least two items;
	 * B. some builtin tuples have at least two items, and are accessed by
	 *    constant sub index, compilers such Clang may complain if 1 is used. */
};
DAO_DLL DaoTuple* DaoTuple_Create( DaoType *type, int init );
DAO_DLL void DaoTuple_Delete( DaoTuple *self );
DAO_DLL void DaoTuple_SetItem( DaoTuple *self, DaoValue *it, int pos );
DAO_DLL int DaoTuple_GetIndex( DaoTuple *self, DString *name );

struct DaoException
{
	int       fromLine;
	int       toLine;
	DRoutine *routine;
	DArray   *callers;
	DArray   *lines;

	DString  *name;
	DString  *info;
	DaoValue *data;
};

DaoException* DaoException_New( DaoTypeBase *typer );
DaoException* DaoException_New2( DaoTypeBase *typer, DaoValue *v );
void DaoException_Delete( DaoException *self );
void DaoException_Setup( DaoNamespace *ns );

DaoTypeBase* DaoException_GetType( int type );

extern DaoTypeBase dao_Exception_Typer;

enum{ DAO_CALL_QUEUED, DAO_CALL_RUNNING, DAO_CALL_PAUSED, DAO_CALL_FINISHED };
enum{ DAO_FUTURE_VALUE, DAO_FUTURE_WAIT };

typedef struct DaoFuture  DaoFuture;
struct DaoFuture
{
	DAO_DATA_COMMON;

	uchar_t      state;
	uchar_t      state2;
	short        parCount;
	DaoType     *unitype;
	DaoValue    *value;
	DaoValue    *params[DAO_MAX_PARAM];
	DaoObject   *object;
	DaoRoutine  *routine;
	DaoProcess  *process;
	DaoFuture   *precondition;
};
DaoFuture* DaoFuture_New();

#endif
