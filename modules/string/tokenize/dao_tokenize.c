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

#include<stdlib.h>
#include<string.h>
#include"daoString.h"
#include"daoValue.h"

DAO_INIT_MODULE

static void DaoSTR_Tokenize( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *self = p[0]->xString.data;
	DString *delms = p[1]->xString.data;
	DString *quotes = p[2]->xString.data;
	int bkslash = (int)p[3]->xInteger.value;
	int simplify = (int)p[4]->xInteger.value;
	DaoList *list = DaoProcess_PutList( proc );
	DaoValue *value = (DaoValue*) DaoString_New(1);
	DString *str = value->xString.data;
	if( self->mbs ){
		char *s = self->mbs;
		DString_ToMBS( str );
		DString_ToMBS( delms );
		DString_ToMBS( quotes );
		while( *s ){
			if( bkslash && *s == '\\' ){
				DString_AppendChar( str, *s );
				DString_AppendChar( str, *(s+1) );
				s += 2;
				continue;
			}
			if( ( bkslash == 0 || s == self->mbs || *(s-1) !='\\' )
					&& DString_FindChar( quotes, *s, 0 ) != MAXSIZE ){
				DString_AppendChar( str, *s );
				s ++;
				while( *s ){
					if( bkslash && *s == '\\' ){
						DString_AppendChar( str, *s );
						DString_AppendChar( str, *(s+1) );
						s += 2;
					}
					if( ( bkslash == 0 || *(s-1) !='\\' )
							&& DString_FindChar( quotes, *s, 0 ) != MAXSIZE )
						break;
					DString_AppendChar( str, *s );
					s ++;
				}
				DString_AppendChar( str, *s );
				s ++;
				continue;
			}
			if( DString_FindChar( delms, *s, 0 ) != MAXSIZE ){
				if( s != self->mbs && *(s-1)=='\\' ){
					DString_AppendChar( str, *s );
					s ++;
					continue;
				}else{
					if( simplify ) DString_Trim( str );
					if( str->size > 0 ){
						DArray_Append( & list->items, value );
						DString_Clear( str );
					}
					DString_AppendChar( str, *s );
					s ++;
					if( simplify ) DString_Trim( str );
					if( str->size > 0 ) DArray_Append( & list->items, value );
					DString_Clear( str );
					continue;
				}
			}
			DString_AppendChar( str, *s );
			s ++;
		}
		if( simplify ) DString_Trim( str );
		if( str->size > 0 ) DArray_Append( & list->items, value );
	}else{
		wchar_t *s = self->wcs;
		DString_ToWCS( str );
		DString_ToWCS( delms );
		DString_ToWCS( quotes );
		while( *s ){
			if( ( s == self->wcs || bkslash ==0 || *(s-1)!=L'\\' )
					&& DString_FindWChar( quotes, *s, 0 ) != MAXSIZE ){
				DString_AppendChar( str, *s );
				s ++;
				while( *s ){
					if( ( bkslash ==0 || *(s-1)!=L'\\' )
							&& DString_FindWChar( quotes, *s, 0 ) != MAXSIZE ) break;
					DString_AppendChar( str, *s );
					s ++;
				}
				DString_AppendChar( str, *s );
				s ++;
				continue;
			}
			if( DString_FindWChar( delms, *s, 0 ) != MAXSIZE ){
				if( s != self->wcs && ( bkslash && *(s-1)==L'\\' ) ){
					DString_AppendChar( str, *s );
					s ++;
					continue;
				}else{
					if( simplify ) DString_Trim( str );
					if( str->size > 0 ){
						DArray_Append( & list->items, value );
						DString_Clear( str );
					}
					DString_AppendChar( str, *s );
					s ++;
					if( simplify ) DString_Trim( str );
					if( str->size > 0 ) DArray_Append( & list->items, value );
					DString_Clear( str );
					continue;
				}
			}
			DString_AppendChar( str, *s );
			s ++;
		}
		if( simplify ) DString_Trim( str );
		if( str->size > 0 ) DArray_Append( & list->items, value );
	}
	DaoString_Delete( (DaoString*) value );
}
const char *p = "tokenize( self :string, seps :string, quotes='', backslash=0, simplify=0 )=>list<string>";
int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace_WrapFunction( ns, DaoSTR_Tokenize, p );
	return 0;
}
