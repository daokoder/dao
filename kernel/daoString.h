/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2011, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#ifndef DAO_STRING_H
#define DAO_STRING_H

#include<wctype.h>
#include<wchar.h>
#include<limits.h>

#define MAXSIZE ((size_t)(-1))

#include"daoBase.h"

struct DString
{
	size_t   size    : CHAR_BIT*sizeof(size_t)-1;
	size_t   dummy   : 1;
	size_t   bufSize : CHAR_BIT*sizeof(size_t)-1;
	size_t   shared  : 1;
	char    *mbs;
	wchar_t *wcs;
};

DAO_DLL DString* DString_New( int mbs );
DAO_DLL void DString_Init( DString *self, int mbs );
DAO_DLL void DString_DeleteData( DString *self );
DAO_DLL void DString_Delete( DString *self );
DAO_DLL void DString_Detach( DString *self );

DAO_DLL void DString_SetSharing( DString *self, int sharing );

DAO_DLL int DString_IsMBS( DString *self );
DAO_DLL size_t DString_CheckUTF8( DString *self );

DAO_DLL char* DString_GetMBS( DString *self );
DAO_DLL wchar_t* DString_GetWCS( DString *self );

DAO_DLL void DString_SetMBS( DString *self, const char *chs );
DAO_DLL void DString_SetWCS( DString *self, const wchar_t *chs );
DAO_DLL void DString_SetDataMBS( DString *self, const char *bytes, size_t count );
DAO_DLL void DString_SetDataWCS( DString *self, const wchar_t *data, size_t count );

DAO_DLL void DString_ToWCS( DString *self );
DAO_DLL void DString_ToMBS( DString *self );
DAO_DLL void DString_ToLower( DString *self );
DAO_DLL void DString_ToUpper( DString *self );
DAO_DLL void DString_Reverse( DString *self );

DAO_DLL size_t DString_Size( DString *self );
DAO_DLL void DString_Resize( DString *self, size_t size );
DAO_DLL void DString_Reserve( DString *self, size_t size );
DAO_DLL void DString_Clear( DString *self );

DAO_DLL void DString_Erase( DString *self, size_t start, size_t n );
DAO_DLL void DString_Insert( DString *self, DString *chs, size_t at, size_t rm, size_t cp );
DAO_DLL void DString_InsertMBS( DString *self, const char *chs, size_t at, size_t rm, size_t cp );
DAO_DLL void DString_InsertChar( DString *self, const char ch, size_t at );
DAO_DLL void DString_InsertWCS( DString *self, const wchar_t *chs, size_t at, size_t rm, size_t cp );
DAO_DLL void DString_Append( DString *self, DString *chs );
DAO_DLL void DString_AppendChar( DString *self, const char ch );
DAO_DLL void DString_AppendWChar( DString *self, const wchar_t ch );
DAO_DLL void DString_AppendMBS( DString *self, const char *chs );
DAO_DLL void DString_AppendWCS( DString *self, const wchar_t *chs );
DAO_DLL void DString_AppendDataMBS( DString *self, const char *chs, size_t n );
DAO_DLL void DString_AppendDataWCS( DString *self, const wchar_t *chs,size_t n );
DAO_DLL void DString_SetDataMBS( DString *self, const char *bytes, size_t count );
DAO_DLL void DString_SetDataWCS( DString *self, const wchar_t *data, size_t count );

DAO_DLL void DString_Replace( DString *self, DString *chs, size_t start, size_t rm );
DAO_DLL void DString_ReplaceMBS( DString *self, const char *chs, size_t start, size_t rm );
DAO_DLL void DString_SubString( DString *self, DString *sub, size_t from, size_t n );

DAO_DLL size_t DString_Find( DString *self, DString *chs, size_t start );
DAO_DLL size_t DString_RFind( DString *self, DString *chs, size_t start );
DAO_DLL size_t DString_FindMBS( DString *self, const char *ch, size_t start );
DAO_DLL size_t DString_RFindMBS( DString *self, const char *ch, size_t start );
DAO_DLL size_t DString_FindChar( DString *self, char ch, size_t start );
DAO_DLL size_t DString_FindWChar( DString *self, wchar_t ch, size_t start );
DAO_DLL size_t DString_RFindChar( DString *self, char ch, size_t start );

DAO_DLL int DString_MatchMBS( DString *self, const char *pat, size_t *start, size_t *end );
DAO_DLL int DString_MatchWCS( DString *self, const wchar_t *pat, size_t *start, size_t *end );

DAO_DLL int DString_ChangeMBS( DString *self, char *pat, char *target, int index );
DAO_DLL int DString_ChangeWCS( DString *self, wchar_t *pat, wchar_t *target, int index );

DAO_DLL DString* DString_Copy( DString *self );
DAO_DLL DString* DString_DeepCopy( DString *self );
DAO_DLL void DString_Assign( DString *left, DString *right );
DAO_DLL int  DString_Compare( DString *left, DString *right );
DAO_DLL int  DString_EQ( DString *left, DString *right );

DAO_DLL void DString_Add( DString *self, DString *left, DString *right );
DAO_DLL void DString_Chop( DString *self );
DAO_DLL void DString_Trim( DString *self );

DAO_DLL int DString_Encrypt( DString *self, DString *key, int hex );
DAO_DLL int DString_Decrypt( DString *self, DString *key, int hex );

DAO_DLL size_t DString_BalancedChar( DString *self, uint_t ch0, uint_t lch0, uint_t rch0, 
		uint_t esc0, size_t start, size_t end, int countonly );

DAO_DLL DString DString_WrapBytes( const char *mbs, int n );
DAO_DLL DString DString_WrapMBS( const char *mbs );
DAO_DLL DString DString_WrapWCS( const wchar_t *wcs );

#endif
