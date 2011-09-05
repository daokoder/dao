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


typedef struct DaoContext  DaoContext;
/* running time context for dao routine. */
struct DaoContext
{
	DAO_DATA_COMMON;

	DaoStackFrame *frame;

	DaoVmCode  *codes;
	DaoVmCode  *vmc; /* current virtual machine code */

	DTuple    *regArray; /* virtual registers for the call */
	DaoValue **regValues; /* = regArray->items.pValue */
	DaoType  **regTypes; /* = routine->regType->items.pType; */

	ushort_t  entryCode;
	ushort_t  idClearFE;
	ushort_t  parCount;
	ushort_t  ctxState;

	DaoRoutine   *routine; /* routine prototype */
	DaoObject    *object; /* self object */
	DaoNamespace *nameSpace;
	DaoVmSpace   *vmSpace;

	DaoContext   *caller;
	DaoProcess   *process;
	DaoRoutine   *lastRoutine;

	/* the currently called wrapped C function,
	   for exception handling and DaoCLoader module. */
	DaoFunction *thisFunction;

};

void DaoProcess_RaiseException( DaoProcess *self, int type, const char *value );
void DaoProcess_RaiseTypeError( DaoProcess *self, DaoType *from, DaoType *to, const char *op );
/**/
void DaoProcess_ShowCallError( DaoProcess *self, DRoutine *rout, DaoValue *selfobj, DaoValue *ps[], int np, int code );
void DaoProcess_Print( DaoProcess *self, const char *chs );
void DaoProcess_PrintInfo( DaoProcess *self, const char *head, const char *info );
void DaoProcess_PrintVmCode( DaoProcess *self );


#endif
