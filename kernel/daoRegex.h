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
