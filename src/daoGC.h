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

#endif
