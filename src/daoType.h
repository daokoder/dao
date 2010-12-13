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
#include"daoStdtype.h"

/* Dao abstract type:
 * type class for number, string, ... list<X>, ...
 * 
 * for core types: number, string, complex, list, map, array
 * eg:DaoType.name = "int", "float", "string", ...
 *    DaoType.count = 0; DaoType.pbasic = NULL;
 *    DaoType.basic = DAO_INTEGER, DAO_STRING, ...
 *
 * for Dao class and routine types:
 *    DaoType.name = "foo", "bar", ...
 *    DaoType.count = 0;
 *    DaoType.basic = DAO_CLASS, DAO_CDATA,
 *    DaoType.pbasic = the Dao class or C type
 *
 * for nested type: list<float>, map<string,float>, ...
 *    DaoType.name = "list<float>", "map<string,float>", ...
 *    DaoType.basic = DAO_LIST, DAO_MAP
 *    DaoType.pbasic = NULL;
 *    DaoType.count = 1, or 2, or more for routine and tuple etc. types
 *    DaoType.nested[] = nested DaoType(s) : X<nested[0],nested[1],...>
 *
 * for routine type: routine(float,string):float
 *    DaoType.name = "routine<float,string=>float>"
 *    DaoType.basic = DAO_ROUTINE
 *    DaoType.pbasic = XXX
 *    DaoType.count = parameter count + 1
 *    DaoType.nested[] = parameter DaoType(s) : (<nested[0],...) : returned
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
 *    DaoType.name = "pair<string,X>"
 *    DaoType.basic = DAO_PARNAME
 *    DaoType.count = 2
 *    DaoType.nested[] = string, X
 *
 * for pair id: id1 : id2
 *    DaoType.name = "pair<X,Y>"
 *    DaoType.basic = DAO_PAIR
 *    DaoType.count = 2
 *    DaoType.nested[] = X, Y
 */
struct DaoType
{
	DAO_DATA_COMMON;

	uchar_t       tid; /* type id */
	uchar_t       attrib;
	uchar_t       flagtype; /* for enum type */
	uchar_t       ffitype; /* for DaoCLoader module */
	DString      *name; /* type name */
	DString      *fname; /* field name, or parameter name */
	DArray       *nested;
	DMap         *mapNames;
	DMap         *interfaces;
	DaoTypeBase  *typer; /* TYPER of the represented type: built-in or C types */
	union {
		DaoBase      *extra;
		DaoType      *abtype;
		DaoClass     *klass;
		DaoCData     *cdata;
		DaoInterface *inter;
	}X; /* DaoClass, DaoCData or DaoType for returned/named... */
};
extern DaoType *dao_type_udf;
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

DaoType* DaoType_New( const char *name, short tid, DaoBase *pb, DArray *nest );
DaoType* DaoType_Copy( DaoType *self );
void DaoType_Delete( DaoType *self );

void DaoType_CheckAttributes( DaoType *self );

/* if "self" match to "type": */
short DaoType_MatchTo( DaoType *self, DaoType *type, DMap *defs );
short DaoType_MatchValue( DaoType *self, DValue value, DMap *defs );
/* define @X */
DaoType* DaoType_DefineTypes( DaoType *self, DaoNameSpace *ns, DMap *defs );
void DaoType_RenewTypes( DaoType *self, DaoNameSpace *ns, DMap *defs );

/* all DAO_INITYPE: @T ... */
void DaoType_GetTypes( DaoType *self, DMap *types );

#define NESTYPE(t,i) ((t)->nested->items.pType[i])

struct DaoInterface
{
	DAO_DATA_COMMON;

	int bindany;

	DArray  *supers; /* parent interfaces */
	DMap    *methods; /* DHash<DString*,DRoutine*> */
	DaoType *abtype;
	DMap    *ovldRoutMap; /* <DString*,DaoRoutine*> */
};

DaoInterface* DaoInterface_New( const char *name );

int  DaoInterface_Bind( DArray *pairs, DArray *fails );
void DaoInterface_DeriveMethods( DaoInterface *self );

void DMap_SortMethods( DMap *hash, DArray *methods );

int DaoType_HasInterface( DaoType *self, DaoInterface *inter );

#endif
