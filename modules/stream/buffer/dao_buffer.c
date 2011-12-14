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

#include<stdio.h>
#include<sys/stat.h>

#include"dao.h"
#include"daoStdtype.h"
#include"daoStream.h"
#include"dao_sys.h"
DAO_INIT_MODULE

#ifdef WIN32
#define fstat _fstat
#define stat _stat
#define fileno _fileno
#endif

static void DaoBufferRead( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stream = DaoValue_CastStream( p[0] );
	FILE* file = DaoStream_GetFile( stream );
	Dao_Buffer *buffer = (Dao_Buffer*)DaoValue_CastCdata( p[1] );
	size_t size = DaoValue_TryGetInteger( p[2] );
	if( !file )
		file = stdin;
	if( ( stream->mode & DAO_IO_READ ) == 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The stream is not readable" );
		return;
	}
	if( size == 0 || size > buffer->size )
		size = buffer->size;
	DaoProcess_PutInteger( proc, fread( buffer->buffer.pVoid, 1, size, file ) );
}

static void DaoBufferReadAll( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stream = DaoValue_CastStream( p[0] );
	FILE* file = DaoStream_GetFile( stream );
	Dao_Buffer *buffer;
	size_t size = DaoValue_TryGetInteger( p[1] );
	struct stat info;
	if( !file ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The stream size cannot be determined" );
		return;
	}
	if( ( stream->mode & DAO_IO_READ ) == 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The stream is not readable" );
		return;
	}
	fstat( fileno( file ), &info );
	if( size == 0 || info.st_size - ftell( file )/2 < size )
		size = info.st_size - ftell( file )/2;
	buffer = Dao_Buffer_New( size );
	size = fread( buffer->buffer.pVoid, 1, size, file );
	Dao_Buffer_Resize( buffer, size );
	DaoProcess_PutValue( proc, (DaoValue*)buffer );
}

static void DaoBufferReadAll2( DaoProcess *proc, DaoValue *p[], int N )
{
	FILE* file;
	Dao_Buffer *buffer;
	const char *fname = DaoString_GetMBS( DaoValue_CastString( p[1] ) );
	size_t size = DaoValue_TryGetInteger( p[2] );
	struct stat info;
	file = fopen( fname, "rb" );
	if( !file ){
		char errbuf[300];
		snprintf( errbuf, sizeof( errbuf ), "Error opening file: %s", fname );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	fstat( fileno( file ), &info );
	if( size == 0 || info.st_size < size )
		size = info.st_size;
	buffer = Dao_Buffer_New( size );
	size = fread( buffer->buffer.pVoid, 1, size, file );
	Dao_Buffer_Resize( buffer, size );
	fclose( file );
	DaoProcess_PutValue( proc, (DaoValue*)buffer );
}

static void DaoBufferWrite( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stream = DaoValue_CastStream( p[0] );
	FILE* file = DaoStream_GetFile( stream );
	Dao_Buffer *buffer = (Dao_Buffer*)DaoValue_CastCdata( p[1] );
	size_t size = DaoValue_TryGetInteger( p[2] );
	if( !file )
		file = stdout;
	if( ( stream->mode & DAO_IO_WRITE ) == 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The stream is not writable" );
		return;
	}
	if( size == 0 || size > buffer->size )
		size = buffer->size;
	fwrite( buffer->buffer.pVoid, 1, size, file );
}

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoVmSpace_LinkModule( vmSpace, ns, "sys" );
	DaoNamespace_WrapFunction( ns, (DaoFuncPtr)DaoBufferRead,   "fillbuf( self: stream, buf: buffer, count = 0 )=>int" );
	DaoNamespace_WrapFunction( ns, (DaoFuncPtr)DaoBufferReadAll,"readbuf( self: stream, count = 0 )=>buffer" );
	DaoNamespace_WrapFunction( ns, (DaoFuncPtr)DaoBufferReadAll2,
							   "readbuf( self: stream, file: string, count = 0 )=>buffer" );
	DaoNamespace_WrapFunction( ns, (DaoFuncPtr)DaoBufferWrite,   "writebuf( self: stream, buf: buffer, count = 0 )" );
	return 0;
}
