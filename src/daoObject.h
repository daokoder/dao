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

#ifndef DAO_OBJECT_H
#define DAO_OBJECT_H

#include"daoType.h"

struct DaoObject
{
	DAO_DATA_COMMON;

	DValue     *objValues;
	DVaTuple   *objData;
	DaoClass   *myClass;
	DPtrTuple  *superObject; /*DPtrTuple<DaoObject/DaoCData*>*/
	DaoObject  *that;
	DaoMap     *meta;
};

DaoObject* DaoObject_New( DaoClass *klass, DaoObject *that, int offset );
void DaoObject_Delete( DaoObject *self );

int DaoObject_ChildOf( DaoObject *self, DaoObject *obj );

DaoBase* DaoObject_MapThisObject( DaoObject *self, DaoType *host );
DaoBase* DaoObject_MapChildObject( DaoObject *self, DaoType *parent );

void DaoObject_AddData( DaoObject *self, DString *name, DaoBase  *data );

int DaoObject_SetData( DaoObject *self, DString *name, DValue value, DaoObject *objThis );
int DaoObject_GetData( DaoObject *self, DString *name, DValue *data, DaoObject *objThis, DValue **d2 );

#endif
