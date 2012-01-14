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
	DAO_IO_STRING = 4 ,
	DAO_IO_READ = 1 ,
	DAO_IO_WRITE = 2
};

typedef struct DFile
{
	FILE   *fd;
	int     rc;
} DFile;

struct DaoStream
{
	DAO_DATA_COMMON;

	char        attribs;
	int         mode;
	int         useQuote;
	char       *format;
	DFile      *file;
	DString    *streamString;
	DString    *fname;

	DaoUserStream *redirect;
};

DAO_DLL DaoStream* DaoStream_New();
DAO_DLL void DaoStream_Delete( DaoStream *self );
DAO_DLL void DaoStream_Close( DaoStream *self );
DAO_DLL void DaoStream_Flush( DaoStream *self );

DAO_DLL void DaoStream_WriteChar( DaoStream *self, char val );
DAO_DLL void DaoStream_WriteInt( DaoStream *self, dint val );
DAO_DLL void DaoStream_WriteFloat( DaoStream *self, double val );
DAO_DLL void DaoStream_WriteString( DaoStream *self, DString *val );
DAO_DLL void DaoStream_WriteMBS( DaoStream *self, const char *val );
DAO_DLL void DaoStream_WriteWCS( DaoStream *self, const wchar_t *val );
DAO_DLL void DaoStream_WritePointer( DaoStream *self, void *val );
DAO_DLL void DaoStream_WriteFormatedInt( DaoStream *self, dint val, char *format );
DAO_DLL void DaoStream_WriteNewLine( DaoStream *self );

DAO_DLL int DaoStream_ReadLine( DaoStream *self, DString *buf );
DAO_DLL int DaoFile_ReadLine( FILE *fin, DString *line );
DAO_DLL int DaoFile_ReadAll( FILE *fin, DString *all, int close );

DAO_DLL int Dao_IsFile( const char *file );
DAO_DLL int Dao_IsDir( const char *file );

#endif
