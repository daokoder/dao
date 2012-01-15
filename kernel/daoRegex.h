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

#ifndef DAO_REGEX_H
#define DAO_REGEX_H

#include"daoType.h"

typedef struct DaoRgxItem DaoRgxItem;

struct DaoRgxItem
{
	uchar_t  type; /* type of the pattern */
	uchar_t  config;
	short  gid;
	short  next;
	short  jump;
	short  from;
	short  min;
	short  max;
	daoint count;
	daoint pos;
	daoint offset;
	daoint posave;
	short  fromsave;

	short  length; /* length of the pattern */
	short  word;
};

struct DaoRegex
{
	void   *source;
	daoint  start;
	daoint  end;
	DaoRgxItem *items;
	/* total number of items; or free space in the buffer as input */
	short  count;
	short  config;
	short  attrib;
	short  mbs;
	short  group;
	short  indexed;
	void  *wordbuf;
	int    itemlen; /* in bytes */
	int    wordlen; /* in bytes */
	int    length;
};

DAO_DLL DaoRegex* DaoRegex_New( DString *src );
#define DaoRegex_Delete( self ) dao_free( self )
DAO_DLL void DaoRegex_Copy( DaoRegex *self, DaoRegex *src );

/* compute the number of bytes needed for storing the compiled pattern */
DAO_DLL int DaoRegex_CheckSize( DString *src );

DAO_DLL int DaoRegex_Match( DaoRegex *self, DString *src, daoint *start, daoint *end );
DAO_DLL int DaoRegex_SubMatch( DaoRegex *self, int gid, daoint *start, daoint *end );

DAO_DLL int DaoRegex_Change( DaoRegex *self, DString *src, DString *target, int index );
DAO_DLL int DaoRegex_ChangeExt( DaoRegex *self, DString *source, DString *target, 
		int index, daoint *start2, daoint *end2 );

DAO_DLL int DaoRegex_MatchAndPack( DaoRegex *self, DString *source, DString *target, 
		int index, int count, DArray *packs );

#endif
