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

#ifndef DAO_SCHED_H
#define DAO_SCHED_H

#include"daoVmspace.h"

void DaoCallServer_Join();
void DaoCallServer_Stop();
void DaoCallServer_AddTask( DThreadTask func, void *param );
void DaoCallServer_AddWait( DaoProcess *wait, DaoFuture *future, double timeout, short state );
DaoFuture* DaoCallServer_AddCall( DaoProcess *call );

#endif
