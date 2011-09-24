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

#ifndef DAO_TYPE_H
#define DAO_TYPE_H

#include"daoBase.h"
#include"daoConst.h"
#include"daoString.h"
#include"daoArray.h"
#include"daoMap.h"
#include"daoStdtype.h"

/* Dao abstract type:
 * type class for number, string, ... list<X>, ...
 * 
 * for core types: number, string, complex, list, map, array
 * eg:DaoType.name = "int", "float", "string", ...
 *    DaoType.tid = DAO_INTEGER, DAO_STRING, ...
 *
 * for Dao class and routine types:
 *    DaoType.name = "foo", "bar", ...
 *    DaoType.count = 0;
 *    DaoType.tid = DAO_CLASS, DAO_CDATA,
 *    DaoType.aux = the Dao class or C type
 *
 * for nested type: list<float>, map<string,float>, ...
 *    DaoType.name = "list<float>", "map<string,float>", ...
 *    DaoType.tid = DAO_LIST, DAO_MAP
 *    DaoType.aux = NULL;
 *    DaoType.nested[] = nested DaoType(s) : X<nested[0],nested[1],...>
 *
 * for routine type: routine(float,string):float
 *    DaoType.name = "routine<float,string=>float>"
 *    DaoType.tid = DAO_ROUTINE
 *    DaoType.aux = returned type
 *    DaoType.nested[] = parameter DaoType(s) : (<nested[0],...)
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
 *    DaoType.name = "string:type" or "string=type"
 *    DaoType.tid = DAO_PAR_NAMED or DAO_PAR_DEFAULT
 *    DaoType.aux = actual type
 */
struct DaoType
{
	DAO_DATA_COMMON;

	uchar_t   tid; /* type id */
	uchar_t   attrib;
	uchar_t   flagtype : 1; /* for enum type */
	uchar_t   simtype : 1; /* if the nested contains only simple types */
	uchar_t   ffitype : 6; /* for DaoCLoader module */
	uchar_t   rntcount; /* real number type count */
	DString  *name; /* type name */
	DString  *fname; /* field name, or parameter name */
	DArray   *nested; /* type items */
	DMap     *mapNames;
	DMap     *interfaces;

	/* Auxiliary data for the type:
	 * aux can be the returned type in a routine type;
	 * aux can be the parameter type in a named parameter type;
	 * aux can be the class object in class or object type;
	 * aux can be the DaoCdata object in wrapped C type;
	 * aux can be the constant value in a constant value type. */
	DaoValue  *aux;
	DaoValue  *value; /* default value for the type; */

	DaoType  *cbtype; /* extra type for code block; */

	DaoTypeKernel  *kernel; /* type kernel of built-in or C types; */
	DaoTypeBase    *typer;
};
extern DaoType *dao_type_udf;
extern DaoType *dao_type_any;
extern DaoType *dao_array_any;
extern DaoType *dao_array_empty;
extern DaoType *dao_list_any;
extern DaoType *dao_list_empty;
extern DaoType *dao_map_any;
extern DaoType *dao_map_empty;
extern DaoType *dao_map_meta;
extern DaoType *dao_routine;
extern DaoType *dao_class_any;
extern DaoType *dao_type_for_iterator;
extern DaoType *dao_access_enum;
extern DaoType *dao_storage_enum;

DaoType* DaoType_New( const char *name, short tid, DaoValue *pb, DArray *nest );
DaoType* DaoType_Copy( DaoType *self );
void DaoType_Delete( DaoType *self );

void DaoType_InitDefault( DaoType *self );
void DaoType_CheckAttributes( DaoType *self );

/* if "self" match to "type": */
short DaoType_MatchTo( DaoType *self, DaoType *type, DMap *defs );
short DaoType_MatchValue( DaoType *self, DaoValue *value, DMap *defs );
short DaoType_MatchValue2( DaoType *self, DaoValue *value, DMap *defs );
/* define @X */
DaoType* DaoType_DefineTypes( DaoType *self, DaoNamespace *ns, DMap *defs );
void DaoType_RenewTypes( DaoType *self, DaoNamespace *ns, DMap *defs );

/* all DAO_INITYPE: @T ... */
void DaoType_GetTypeHolders( DaoType *self, DMap *types );


#define NESTYPE(t,i) ((t)->nested->items.pType[i])

struct DaoInterface
{
	DAO_DATA_COMMON;

	short bindany;
	short derived;

	DArray  *supers; /* parent interfaces */
	DMap    *methods; /* DHash<DString*,DRoutine*> */
	DaoType *abtype;
	DMap    *ovldRoutMap; /* <DString*,DaoRoutine*> */
};

DaoInterface* DaoInterface_New( const char *name );

int DaoInterface_Bind( DArray *pairs, DArray *fails );
int DaoInterface_BindTo( DaoInterface *self, DaoType *type, DMap *binds, DArray *fails );
void DaoInterface_DeriveMethods( DaoInterface *self );

void DMap_SortMethods( DMap *hash, DArray *methods );

int DaoType_HasInterface( DaoType *self, DaoInterface *inter );


/* Structure DaoTypeKernel will contain generated wrapping data for the type.
 * It is GC collectable, so that it will be automatically deleted once it is
 * no longer used, which make it possible to unload external modules automatically.
 * Its reference counting is handled and only handled by DaoType. */
struct DaoTypeKernel
{
	DAO_DATA_COMMON;

	uint_t         attribs;
	DMap          *values;
	DMap          *methods;
	DMap          *instances; /* for C data; */
	DaoType       *abtype;
	DaoNamespace  *nspace;
	DaoTypeCore   *core;
	DaoTypeBase   *typer;
};
DaoTypeKernel* DaoTypeKernel_New( DaoTypeBase *typer );


/* The separation of DaoTypeKernel from DaoTypeCore will make it simpler 
 * to create DaoTypeCore structures, and also make it unnecessary to change
 * the DaoTypeCore definitions when DaoTypeKernel needs to be changed. */
struct DaoTypeCore
{
	DaoTypeKernel  *kernel;

	void (*GetField)( DaoValue *self, DaoProcess *proc, DString *name );
	void (*SetField)( DaoValue *self, DaoProcess *proc, DString *name, DaoValue *value );
	void (*GetItem) ( DaoValue *self, DaoProcess *proc, DaoValue *pid[], int N );
	void (*SetItem) ( DaoValue *self, DaoProcess *proc, DaoValue *pid[], int N, DaoValue *value );
	void (*Print)( DaoValue *self, DaoProcess *proc, DaoStream *stream, DMap *cycData );
	DaoValue* (*Copy)(  DaoValue *self, DaoProcess *proc, DMap *cycData );
};
extern DaoTypeCore  baseCore;


DaoTypeBase* DaoValue_GetTyper( DaoValue *p );

DaoValue* DaoValue_Duplicate( void *dbase, DaoType *type );

void DaoValue_GetField( DaoValue *self, DaoProcess *proc, DString *name );
void DaoValue_SetField( DaoValue *self, DaoProcess *proc, DString *name, DaoValue *value );
void DaoValue_GetItem( DaoValue *self, DaoProcess *proc, DaoValue *pid[], int N );
void DaoValue_SetItem( DaoValue *self, DaoProcess *proc, DaoValue *pid[], int N, DaoValue *value );
void DaoValue_Print( DaoValue *self, DaoProcess *proc, DaoStream *stream, DMap *cycData );
DaoValue* DaoValue_NoCopy( DaoValue *self, DaoProcess *proc, DMap *cycData );

void DaoValue_SafeGetField( DaoValue *self, DaoProcess *proc, DString *name );
void DaoValue_SafeSetField( DaoValue *self, DaoProcess *proc, DString *name, DaoValue *value );

struct DaoCdataCore
{
	DaoTypeKernel  *kernel;

	void (*GetField)( DaoValue *self, DaoProcess *proc, DString *name );
	void (*SetField)( DaoValue *self, DaoProcess *proc, DString *name, DaoValue *value );
	void (*GetItem)(  DaoValue *self, DaoProcess *proc, DaoValue *pid[], int N );
	void (*SetItem)(  DaoValue *self, DaoProcess *proc, DaoValue *pid[], int N, DaoValue *value );
	void (*Print)( DaoValue *self, DaoProcess *proc, DaoStream *stream, DMap *cycData );
	DaoValue* (*Copy)(  DaoValue *self, DaoProcess *proc, DMap *cycData );

	void   (*DelData)( void *data );
	int    (*DelTest)( void *data );
};

DaoValue* DaoTypeBase_FindValue( DaoTypeBase *self, DString *name );
DaoValue* DaoTypeBase_FindValueOnly( DaoTypeBase *self, DString *name );
DaoValue* DaoTypeBase_FindFunction( DaoTypeBase *self, DString *name );
DaoValue* DaoTypeBase_FindFunctionMBS( DaoTypeBase *self, const char *name );

#endif
