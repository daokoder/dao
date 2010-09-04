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

#ifdef DAO_WITH_ASMBC
#undef DAO_WITH_ASMBC
#define DAO_WITH_ASMBC_XXX
#endif

#ifndef DAO_ASMBC_H
#define DAO_ASMBC_H

/* Assembly & bytecode parser and writer. */

#include"daoBase.h"

typedef struct DaoAsmClass    DaoAsmClass;
typedef struct DaoAsmRoutine  DaoAsmRoutine;

struct DaoAsmClass
{
	DaoClass *myClass;
	DaoBase  *memb;
	/* DArray<DArray<DString*>*>:
	 * {{ "module", "@BY", "loader", "@AS", "ns", "name1", ... }} */
	DArray *stmtLoad;
	DArray *nameConst;
	DArray *nameGlobal;
	DArray *nameVar;
};
struct DaoAsmRoutine
{
	int tmp;
};

struct DaoAsmWriter
{
	/* DArray<DArray<DString*>*>:
	 * {{ "module", "@BY", "loader", "@AS", "ns", "name1", ... }} */
	DArray *stmtLoad;
	DArray *classes; /* DArray<DString*>: { class, NULL/class/ns, ... } */
	DArray *routines; /* DArray<DString*>: { class, NULL/class/ns, ... } */
};

DaoAsmWriter* DaoAsmWriter_New();
void DaoAsmWriter_Delete( DaoAsmWriter *self );
void DaoAsmWriter_Write( DaoAsmWriter *self, DString *output );

int DaoParseAssembly( DaoVmSpace *self, DaoNameSpace *ns, DString *src, DString *bc );
int DaoParseByteCode( DaoVmSpace *self, DaoNameSpace *ns, DString *src, DString *asmc );

#endif
