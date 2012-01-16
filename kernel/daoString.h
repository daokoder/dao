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

#ifndef DAO_STRING_H
#define DAO_STRING_H

#include<wctype.h>
#include<wchar.h>
#include<limits.h>

#define MAXSIZE ((daoint)(-1))

#include"daoBase.h"

struct DString
{
	daoint   size    : CHAR_BIT*sizeof(daoint)-1;
	size_t   dummy   : 1;
	daoint   bufSize : CHAR_BIT*sizeof(daoint)-1;
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
DAO_DLL daoint DString_CheckUTF8( DString *self );

DAO_DLL char* DString_GetMBS( DString *self );
DAO_DLL wchar_t* DString_GetWCS( DString *self );

DAO_DLL void DString_SetMBS( DString *self, const char *chs );
DAO_DLL void DString_SetWCS( DString *self, const wchar_t *chs );
DAO_DLL void DString_SetDataMBS( DString *self, const char *bytes, daoint count );
DAO_DLL void DString_SetDataWCS( DString *self, const wchar_t *data, daoint count );

DAO_DLL void DString_ToWCS( DString *self );
DAO_DLL void DString_ToMBS( DString *self );
DAO_DLL void DString_ToLower( DString *self );
DAO_DLL void DString_ToUpper( DString *self );
DAO_DLL void DString_Reverse( DString *self );

DAO_DLL daoint DString_Size( DString *self );
DAO_DLL void DString_Reset( DString *self, daoint size );
DAO_DLL void DString_Resize( DString *self, daoint size );
DAO_DLL void DString_Reserve( DString *self, daoint size );
DAO_DLL void DString_Clear( DString *self );

DAO_DLL void DString_Erase( DString *self, daoint start, daoint n );
DAO_DLL void DString_Insert( DString *self, DString *chs, daoint at, daoint rm, daoint cp );
DAO_DLL void DString_InsertMBS( DString *self, const char *chs, daoint at, daoint rm, daoint cp );
DAO_DLL void DString_InsertChar( DString *self, const char ch, daoint at );
DAO_DLL void DString_InsertWCS( DString *self, const wchar_t *chs, daoint at, daoint rm, daoint cp );
DAO_DLL void DString_Append( DString *self, DString *chs );
DAO_DLL void DString_AppendChar( DString *self, const char ch );
DAO_DLL void DString_AppendWChar( DString *self, const wchar_t ch );
DAO_DLL void DString_AppendMBS( DString *self, const char *chs );
DAO_DLL void DString_AppendWCS( DString *self, const wchar_t *chs );
DAO_DLL void DString_AppendDataMBS( DString *self, const char *chs, daoint n );
DAO_DLL void DString_AppendDataWCS( DString *self, const wchar_t *chs,daoint n );
DAO_DLL void DString_SetDataMBS( DString *self, const char *bytes, daoint count );
DAO_DLL void DString_SetDataWCS( DString *self, const wchar_t *data, daoint count );

DAO_DLL void DString_Replace( DString *self, DString *chs, daoint start, daoint rm );
DAO_DLL void DString_ReplaceMBS( DString *self, const char *chs, daoint start, daoint rm );
DAO_DLL void DString_SubString( DString *self, DString *sub, daoint from, daoint n );

DAO_DLL daoint DString_Find( DString *self, DString *chs, daoint start );
DAO_DLL daoint DString_RFind( DString *self, DString *chs, daoint start );
DAO_DLL daoint DString_FindMBS( DString *self, const char *ch, daoint start );
DAO_DLL daoint DString_RFindMBS( DString *self, const char *ch, daoint start );
DAO_DLL daoint DString_FindChar( DString *self, char ch, daoint start );
DAO_DLL daoint DString_FindWChar( DString *self, wchar_t ch, daoint start );
DAO_DLL daoint DString_RFindChar( DString *self, char ch, daoint start );

DAO_DLL int DString_MatchMBS( DString *self, const char *pat, daoint *start, daoint *end );
DAO_DLL int DString_MatchWCS( DString *self, const wchar_t *pat, daoint *start, daoint *end );

DAO_DLL int DString_ChangeMBS( DString *self, const char *pat, const char *target, int index );
DAO_DLL int DString_ChangeWCS( DString *self, const wchar_t *pat, const wchar_t *target, int index );

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

DAO_DLL daoint DString_BalancedChar( DString *self, uint_t ch0, uint_t lch0, uint_t rch0, 
		uint_t esc0, daoint start, daoint end, int countonly );

DAO_DLL DString DString_WrapBytes( const char *mbs, int n );
DAO_DLL DString DString_WrapMBS( const char *mbs );
DAO_DLL DString DString_WrapWCS( const wchar_t *wcs );

#endif
