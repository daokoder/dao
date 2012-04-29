/*
// This file is part of the virtual machine for the Dao programming language.
// Copyright (C) 2006-2012, Limin Fu. Email: daokoder@gmail.com
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this 
// software and associated documentation files (the "Software"), to deal in the Software 
// without restriction, including without limitation the rights to use, copy, modify, merge, 
// publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons 
// to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or 
// substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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

#define GC_IncRCs( p )  DaoGC_IncRCs( p )
#define GC_DecRCs( p )  DaoGC_DecRCs( p )

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

#endif
