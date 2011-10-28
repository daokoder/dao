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

#ifndef DAO_CONTEXT_H
#define DAO_CONTEXT_H

#include"daoVmcode.h"
#include"daoType.h"
#include"time.h"
#include"stdlib.h"


void DaoProcess_RaiseException( DaoProcess *self, int type, const char *value );
void DaoProcess_RaiseTypeError( DaoProcess *self, DaoType *from, DaoType *to, const char *op );
/**/
void DaoProcess_ShowCallError( DaoProcess *self, DRoutine *rout, DaoValue *selfobj, DaoValue *ps[], int np, int code );
void DaoProcess_Print( DaoProcess *self, const char *chs );
void DaoProcess_PrintInfo( DaoProcess *self, const char *head, const char *info );
void DaoProcess_PrintVmCode( DaoProcess *self );


#endif
