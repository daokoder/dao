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

#ifndef DAO_GC_H
#define DAO_GC_H

#include"daoType.h"

extern int DaoGC_Min( int n /*=-1*/ );
extern int DaoGC_Max( int n /*=-1*/ );

extern void DaoStartGC();

void DaoFinishGC();

extern void DaoGC_IncRC( DaoBase *dbase );
extern void DaoGC_DecRC( DaoBase *dbase );
extern void DaoGC_ShiftRC( DaoBase *up, DaoBase *down );

extern void DaoGC_IncRCs( DArray *dbases );
extern void DaoGC_DecRCs( DArray *dbases );

#define GC_IncRC( p )        DaoGC_IncRC( (DaoBase*)(p) )
#define GC_DecRC( p )        DaoGC_DecRC( (DaoBase*)(p) )
#define GC_ShiftRC(up,down) \
	if( (DaoBase*)(up) != (DaoBase*)(down) )\
DaoGC_ShiftRC( (DaoBase*)(up), (DaoBase*)(down) )

#define GC_IncRCs( p )  DaoGC_IncRCs( p )
#define GC_DecRCs( p )  DaoGC_DecRCs( p )

/* DaoLateDeleter holds types that are deleted but not really freed.
   This is to provide safety for caching of type matching results.

   When a DaoType object is no longer used, this object is deleted
   from Dao running time system. The data held by this object is
   cleared, but the piece of memory pointed by this object is NOT
   freed immediately. Because if it is freed, it might be allocated
   as another DaoType object with the same address, which is still
   referred to by type matching caches. The reuse of the same memory
   address for two different DaoType objects will clearly invalidate
   the previous caching results! (The same for DaoRoutine/DaoFunction)

   So to be safe, the memory address of a deleted DaoType objects
   will be reserved for some time. The reserved addresses with un-freed
   memory are stored in DaoLateDeleter.

   In each garbage collection cycle, the collector will check how many 
   dead DaoType objects are produced. When there are too many, it will
   change DaoLateDeleter::safe to false, so that when type matching caches
   detect DaoLateDeleter::safe to be false, they will clear their own 
   caches, perform no caching until DaoLateDeleter::safe is set to true. 

   To avoid using mutex lock for type matching caches, the following
   protection algorithm will be used:
   1. GC Cycle I, DaoLateDeleter::types more than N:
   DaoLateDeleter::safe = false;
   DaoLateDeleter::lock = false;
   DaoLateDeleter::version += 1;

   2. GC Cycle I+1, ::safe == false, ::lock = false:
   DaoLateDeleter::lock = true;
   Clear DaoLateDeleter::types;

   3. GC Cycle I+2, ::safe == false, ::lock = true:
   DaoLateDeleter::safe = true;
   DaoLateDeleter::lock = false;

   Since these operations are done in different GC cyles, type matching
   caches are safe to use DaoLateDeleter::safe only to properly take action.
 */
typedef struct DaoLateDeleter DaoLateDeleter;
struct DaoLateDeleter
{
	short   lock;
	short   safe;
	size_t  version;
	DArray *buffer;
};
extern DaoLateDeleter dao_late_deleter;
void DaoLateDeleter_Push( void *p );

#endif
