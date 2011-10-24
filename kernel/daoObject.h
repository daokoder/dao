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

#ifndef DAO_OBJECT_H
#define DAO_OBJECT_H

#include"daoType.h"

struct DaoObject
{
	DAO_DATA_COMMON;

	uchar_t     isRoot     : 1;
	uchar_t     isDefault  : 1;
	uchar_t     baseCount  : 6;
	uchar_t     baseCount2 : 6;
	ushort_t    valueCount;

	DaoClass   *defClass; /* definition class; */
	DaoObject  *rootObject; /* root object for safe down-casting; */
	DaoValue  **objValues; /* instance variable values; */
	DaoValue   *parents[1]; /* the actual size is equal to ::baseCount; */
	//DaoMap     *meta; /* TODO */
};

DaoObject* DaoObject_Allocate( DaoClass *klass, int value_count );
DaoObject* DaoObject_New( DaoClass *klass );
void DaoObject_Init( DaoObject *self, DaoObject *that, int offset );
void DaoObject_Delete( DaoObject *self );

int DaoObject_ChildOf( DaoValue *self, DaoValue *obj );

DaoValue* DaoObject_MapThisObject( DaoObject *self, DaoType *host );
DaoObject* DaoObject_SetParentCdata( DaoObject *self, DaoCdata *parent );

void DaoObject_AddData( DaoObject *self, DString *name, DaoValue *data );

int DaoObject_SetData( DaoObject *self, DString *name, DaoValue *value, DaoObject *objThis );
int DaoObject_GetData( DaoObject *self, DString *name, DaoValue **data, DaoObject *objThis );

#endif
