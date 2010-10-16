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

#ifndef DAO_STREAM_H
#define DAO_STREAM_H

#include<stdio.h>
#include<wchar.h>

#include"daoType.h"

#define IO_BUF_SIZE  512

enum
{
	DAO_IO_FILE = 1 ,
	DAO_IO_PIPE = 2 ,
	DAO_IO_STRING = 4
};

typedef struct DFile
{
	FILE   *fd;
	int     rc;
} DFile;

struct DaoStream
{
	DAO_DATA_COMMON;

	DFile      *file;
	DString    *streamString;
	char        attribs;
	int         useQuote;
	char       *format;
	DaoVmSpace *vmSpace;
	DString    *fname;
	DaoStream  *redirect;
};

extern DaoStream* DaoStream_New();
extern void DaoStream_Delete( DaoStream *self );
extern void DaoStream_Close( DaoStream *self );
extern void DaoStream_Flush( DaoStream *self );

extern void DaoStream_WriteChar( DaoStream *self, char val );
extern void DaoStream_WriteInt( DaoStream *self, dint val );
extern void DaoStream_WriteFloat( DaoStream *self, double val );
extern void DaoStream_WriteString( DaoStream *self, DString *val );
extern void DaoStream_WriteMBS( DaoStream *self, const char *val );
extern void DaoStream_WriteWCS( DaoStream *self, const wchar_t *val );
extern void DaoStream_WritePointer( DaoStream *self, void *val );
extern void DaoStream_WriteFormatedInt( DaoStream *self, dint val, char *format );
extern void DaoStream_WriteNewLine( DaoStream *self );

extern void DaoStream_PrintInfo( DaoStream *self, const char *t, DString *s, int i, const char *e, DString *x );

extern void DaoStream_ReadLine( DaoStream *self, DString *buf );

extern DString* DaoStream_GetFormat( DaoStream *self, int tp );

#endif
