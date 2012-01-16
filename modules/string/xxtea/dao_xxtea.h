/*=========================================================================================
  This file is a part of the Dao standard modules.
  Copyright (C) 2011-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#ifndef DAO_STRING_XXTEA
#define DAO_STRING_XXTEA

#include<dao.h>

DAO_DLL int btea(int* v, int n, int *k);
DAO_DLL int DString_Encrypt( DString *self, DString *key, int hex );
DAO_DLL int DString_Decrypt( DString *self, DString *key, int hex );

#endif
