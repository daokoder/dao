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

#ifndef DAO_CONTEXT_H
#define DAO_CONTEXT_H

#include"daoOpcode.h"
#include"daoType.h"
#include"time.h"
#include"stdlib.h"

struct DaoVmcArray
{
	DaoVmCode *codes;
	DaoVmCode *buf;
	ushort_t   size;
	ushort_t   bufsize;
};

/* running time context for dao routine. */
struct DaoContext
{
	DAO_DATA_COMMON;

	DaoVmCode  *codes;
	DaoVmCode  *vmc; /* current virtual machine code */
	DaoVmFrame *frame;

	DVaTuple  *regArray; /* virtual registers for the call */
	DValue   **regValues;
	DaoType  **regTypes; /* = routine->regType->items.pType; */

	ushort_t  entryCode;
	ushort_t  idClearFE;
	ushort_t  parCount;
	uchar_t   ctxState;
	uchar_t   constCall;

	DaoRoutine   *routine; /* routine prototype */
	DaoObject    *object; /* self object */
	DaoNameSpace *nameSpace;
	DaoVmSpace   *vmSpace;

	DaoContext   *caller;
	DaoVmProcess *process;

	/* the currently called wrapped C function,
	   for exception handling and DaoCLoader module. */
	DaoFunction *thisFunction;

};

/* Create a new context, must be initialized by DaoContext_Init() */
DaoContext* DaoContext_New();
void DaoContext_Delete( DaoContext *self );

/* Intialize a context according to routine */
void DaoContext_Init( DaoContext *self, DaoRoutine *routine );
/* Intialize a context according to overloaed routine with parameter types matching to "pars" */
/* Must be called after DaoContext_Init() */
int DaoContext_InitWithParams( DaoContext *self, DaoVmProcess *vmp, DValue *pars[], int npar );
/* For internal use. */
void DaoContext_SetArrays( DaoContext *self );

/* Put data at a register */
int DaoContext_SetData( DaoContext *self, ushort_t reg, DaoBase *dbase );
DValue* DaoContext_SetValue( DaoContext *self, ushort_t reg, DValue value );

DaoBase* DaoContext_CreateResult( DaoContext *self, short type );

/* Put result at the result register for current instruction */
void DaoContext_SetResult( DaoContext *self, DaoBase *result );
int DaoContext_PutReference( DaoContext *self, DValue *refer );
DValue* DaoContext_PutValue( DaoContext *self, DValue value );

/* Put a number at the result register, return the number object */
dint*      DaoContext_PutInteger( DaoContext *self, dint value );
float*     DaoContext_PutFloat( DaoContext *self, float value );
double*    DaoContext_PutDouble( DaoContext *self, double value );
complex16* DaoContext_PutComplex( DaoContext *self, complex16 value );
DString*   DaoContext_PutMBString( DaoContext *self, const char *mbs );
DString*   DaoContext_PutWCString( DaoContext *self, const wchar_t *wcs );
DString*   DaoContext_PutString( DaoContext *self, DString *str );
DString*   DaoContext_PutBytes( DaoContext *self, const char *bytes, int N );
DEnum*     DaoContext_PutEnum( DaoContext *self, const char *symbols );
DaoArray*  DaoContext_PutArrayInteger( DaoContext *self, int *array, int N );
DaoArray*  DaoContext_PutArrayShort( DaoContext *self, short *array, int N );
DaoArray*  DaoContext_PutArrayFloat( DaoContext *self, float *array, int N );
DaoArray*  DaoContext_PutArrayDouble( DaoContext *self, double *array, int N );
DaoArray*  DaoContext_PutArrayComplex( DaoContext *self, complex16 *array, int N );
DaoTuple*  DaoContext_PutTuple( DaoContext *self );
DaoList*   DaoContext_PutList( DaoContext *self );
DaoMap*    DaoContext_PutMap( DaoContext *self );
DaoArray*  DaoContext_PutArray( DaoContext *self );
DaoCData*  DaoContext_PutCData( DaoContext *self, void *data, DaoTypeBase *plgTyper );
DaoCData*  DaoContext_PutCPointer( DaoContext *self, void *data, int size );
DaoCData*  DaoContext_CopyCData( DaoContext *self, void *data, int n, DaoTypeBase *t );

void DaoContext_RaiseException( DaoContext *self, int type, const char *value );
void DaoContext_RaiseTypeError( DaoContext *self, DaoType *from, DaoType *to, const char *op );
/**/
void DaoContext_Print( DaoContext *self, const char *chs );
void DaoContext_PrintInfo( DaoContext *self, const char *head, const char *info );
void DaoContext_PrintVmCode( DaoContext *self );


#endif
