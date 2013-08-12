/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2013, Limin Fu
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED  BY THE COPYRIGHT HOLDERS AND  CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED  WARRANTIES,  INCLUDING,  BUT NOT LIMITED TO,  THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL  THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE FOR ANY DIRECT,
// INDIRECT,  INCIDENTAL, SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL  DAMAGES (INCLUDING,
// BUT NOT LIMITED TO,  PROCUREMENT OF  SUBSTITUTE  GOODS OR  SERVICES;  LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY OF
// LIABILITY,  WHETHER IN CONTRACT,  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef DAO_GC_H
#define DAO_GC_H

#include"daoType.h"

DAO_DLL int DaoGC_Min( int n /*=-1*/ );
DAO_DLL int DaoGC_Max( int n /*=-1*/ );

DAO_DLL void DaoGC_Start();
DAO_DLL void DaoGC_Finish();
DAO_DLL void DaoGC_TryInvoke();

DAO_DLL void DaoCGC_Start();

DAO_DLL void DaoGC_IncRC( DaoValue *dbase );
DAO_DLL void DaoGC_DecRC( DaoValue *dbase );
DAO_DLL void DaoGC_ShiftRC( DaoValue *up, DaoValue *down );

DAO_DLL void DaoGC_IncRCs( DArray *dbases );
DAO_DLL void DaoGC_DecRCs( DArray *dbases );

#define GC_IncRC( p )        DaoGC_IncRC( (DaoValue*)(p) )
#define GC_DecRC( p )        DaoGC_DecRC( (DaoValue*)(p) )
#define GC_ShiftRC(up,down) \
	if( (DaoValue*)(up) != (DaoValue*)(down) )\
DaoGC_ShiftRC( (DaoValue*)(up), (DaoValue*)(down) )


DAO_DLL void GC_Lock();
DAO_DLL void GC_Unlock();

/*
// DaoGC_LockArray() and DaoGC_LockMap() are used to ensure the concurrent GC
// will be scanning a consistent array or map. A consistent array/map contains
// only valid or NULL pointers; and contain exactly one pointer per reference;
// Such protection is necessary only when the memory of an array is being reallocated,
// or its item pointers are being moved, or tree structure (in the case of map)
// is being mutated.
//
// Modification of array/map with GC_IncRC(), GC_DecRC() and GC_ShiftRC(),
// does not need such protection, because when these GC_XyzRC() methods are
// used, the concurrent GC is able to handle the concurrent modification
// of array/map properly.
//
// Both DArray and DMap have a "mutating" field, which is set to non-zero,
// when DaoGC_LockArray() or DaoGC_LockMap() is called. But they do real locking
// only if the array/map is being scanned by the concurrent GC.
// In the GC part, when it observes that the "mutating" field of an array/map
// is set to non-zero, it will block until the field is reset to zero.
// It always does real locking so that the mutator can block by locking.
*/

DAO_DLL int DaoGC_LockArray( DArray *array );
DAO_DLL void DaoGC_UnlockArray( DArray *array, int locked );

DAO_DLL int DaoGC_LockMap( DMap *map );
DAO_DLL void DaoGC_UnlockMap( DMap *map, int locked );


#define DAO_TUPLE_LIMIT 24


typedef struct DCache DCache;


struct DaoDataCache
{
	daoint  count;
	daoint  fails;
	DCache *values[DAO_TUPLE];
	DCache *tuples[DAO_TUPLE_LIMIT];
};

DAO_DLL DaoDataCache* DaoDataCache_Acquire( DaoDataCache *self, int caching );
DAO_DLL void DaoDataCache_Release( DaoDataCache *self );

DAO_DLL void DaoDataCache_Cache( DaoDataCache *self, DaoValue *value );
DAO_DLL DaoValue* DaoDataCache_MakeValue( DaoDataCache *self, int tid );
DAO_DLL DaoEnum* DaoDataCache_MakeEnum( DaoDataCache *self, DaoType *type );
DAO_DLL DaoArray* DaoDataCache_MakeArray( DaoDataCache *self, int numtype );
DAO_DLL DaoList* DaoDataCache_MakeList( DaoDataCache *self, DaoType *type );
DAO_DLL DaoMap* DaoDataCache_MakeMap( DaoDataCache *self, DaoType *type, int hashing );
DAO_DLL DaoTuple* DaoDataCache_MakeTuple( DaoDataCache *self, DaoType *type, int size, int init );

#endif
