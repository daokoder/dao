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

#ifndef DAO_STRING_H
#define DAO_STRING_H

#include<wctype.h>
#include<wchar.h>

#define MAXSIZE ((size_t)(-1))

#include"daoBase.h"

struct DString
{
	size_t   size;
	size_t   bufSize;
	size_t  *data;
	char    *mbs;
	wchar_t *wcs;
	/* if data==mbs or data==wcs, no implicit sharing;
	   otherwise, data+1 must be equal to mbs or wcs, 
	   and data[0] will be the reference count. */
};

DString* DString_New( int mbs );
void DString_Delete( DString *self );
void DString_Detach( DString *self );

void DString_SetSharing( DString *self, int sharing );

int DString_IsMBS( DString *self );
int DString_IsDigits( DString *self );
int DString_IsDecimal( DString *self );

char* DString_GetMBS( DString *self );
wchar_t* DString_GetWCS( DString *self );

void DString_SetMBS( DString *self, const char *chs );
void DString_SetWCS( DString *self, const wchar_t *chs );
void DString_SetDataMBS( DString *self, const char *bytes, size_t count );
void DString_SetDataWCS( DString *self, const wchar_t *data, size_t count );

void DString_ToWCS( DString *self );
void DString_ToMBS( DString *self );
void DString_ToLower( DString *self );
void DString_ToUpper( DString *self );

size_t  DString_Size( DString *self );
void DString_Resize( DString *self, size_t size );
void DString_Reserve( DString *self, size_t size );
void DString_Clear( DString *self );

void DString_Erase( DString *self, size_t start, size_t n );
void DString_Insert( DString *self, DString *chs, size_t at, size_t rm, size_t cp );
void DString_InsertMBS( DString *self, const char *chs, size_t at, size_t rm, size_t cp );
void DString_InsertChar( DString *self, const char ch, size_t at );
void DString_InsertWCS( DString *self, const wchar_t *chs, size_t at, size_t rm, size_t cp );
void DString_Append( DString *self, DString *chs );
void DString_AppendChar( DString *self, const char ch );
void DString_AppendWChar( DString *self, const wchar_t ch );
void DString_AppendMBS( DString *self, const char *chs );
void DString_AppendWCS( DString *self, const wchar_t *chs );
void DString_AppendDataMBS( DString *self, const char *chs, size_t n );
void DString_AppendDataWCS( DString *self, const wchar_t *chs,size_t n );
void DString_SetDataMBS( DString *self, const char *bytes, size_t count );
void DString_SetDataWCS( DString *self, const wchar_t *data, size_t count );

void DString_Replace( DString *self, DString *chs, size_t start, size_t rm );
void DString_ReplaceMBS( DString *self, const char *chs, size_t start, size_t rm );
void DString_SubString( DString *self, DString *sub, size_t from, size_t n );

size_t DString_Find( DString *self, DString *chs, size_t start );
size_t DString_RFind( DString *self, DString *chs, size_t start );
size_t DString_FindMBS( DString *self, const char *ch, size_t start );
size_t DString_RFindMBS( DString *self, const char *ch, size_t start );
size_t DString_FindChar( DString *self, char ch, size_t start );
size_t DString_FindWChar( DString *self, wchar_t ch, size_t start );
size_t DString_RFindChar( DString *self, char ch, size_t start );

int DString_MatchMBS( DString *self, const char *pat, size_t *start, size_t *end );
int DString_MatchWCS( DString *self, const wchar_t *pat, size_t *start, size_t *end );

int DString_ChangeMBS( DString *self, char *pat, char *target, 
		int index, size_t *start, size_t *end );
int DString_ChangeWCS( DString *self, wchar_t *pat, wchar_t *target, 
		int index, size_t *start, size_t *end );

DString* DString_Copy( DString *self );
DString* DString_DeepCopy( DString *self );
void DString_Assign( DString *left, DString *right );
int  DString_Compare( DString *left, DString *right );
int  DString_EQ( DString *left, DString *right );

void DString_Add( DString *self, DString *left, DString *right );
void DString_Chop( DString *self );
void DString_Trim( DString *self );

int DString_Encrypt( DString *self, DString *key, int hex );
int DString_Decrypt( DString *self, DString *key, int hex );

size_t DString_BalancedChar( DString *self, uint_t ch0, uint_t lch0, uint_t rch0, 
		uint_t esc0, size_t start, size_t end, int countonly );

DString DString_WrapMBS( const char *mbs );
DString DString_WrapWCS( const wchar_t *wcs );

#endif
