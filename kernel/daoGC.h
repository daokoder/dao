/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

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


#endif
