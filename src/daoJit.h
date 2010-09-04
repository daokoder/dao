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

#ifndef DAO_JIT_H
#define DAO_JIT_H

#ifdef DAO_WITH_JIT

#include"daoType.h"

#define PAGE_SIZE  4096
#define ROUND_PAGE( x )  (((x)+4095) & ~4095)

/* TODO
   DaoJitBlock
   {
   int refCount;
   int codeCount;
   int size;
   FuncPtr fptr[];
// codes;
}
DaoRoutine should hold 2? DaoJitBlock pointer, one is being used,
the other is reallocated when necessay.
DaoContext should store one such pointer, when the context is push
to stack, the DaoJitBlock.refCount++; and when it is pop off 
DaoJitBlock.refCount --;

The GC should scan the DaoJitMemory after each collection cycle,
to detect which DaoJitBlock has zero refCount, and remove them 
from DaoJitMemory.blocks.

Then if the blocks in a DaoJitMemory occupy only a small part of 
the DaoJitMemory, copy these blocks to a new DaoJitMemory,
and mark the old DaoJitMemory to be free and reusable.

Each time when there is space become free at the end part of a DaoJitMemory,
sort is position!
 */

struct DaoJitMemory
{
	DAO_DATA_COMMON;

	int size;
	int offset;
	unsigned char *memory;
	void *heap;
	/*
	   DArray *blocks;
	 */
};

/* read|write|executable */
DaoJitMemory* DaoJitMemory_New( int size );
void DaoJitMemory_Delete( DaoJitMemory *self );
/* read|executable */
void DaoJitMemory_Protect( DaoJitMemory *self );
/* read|write|executable */
void DaoJitMemory_Unprotect( DaoJitMemory *self );

void DaoRoutine_JitCompile( DaoRoutine *self );

#endif
#endif
