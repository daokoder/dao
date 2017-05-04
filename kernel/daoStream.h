/*
// Dao Virtual Machine
// http://daoscript.org
//
// Copyright (c) 2006-2017, Limin Fu
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED  BY THE COPYRIGHT HOLDERS AND  CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED  WARRANTIES,  INCLUDING,  BUT NOT LIMITED TO,  THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL  THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE FOR ANY DIRECT,
// INDIRECT,  INCIDENTAL, SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL  DAMAGES (INCLUDING,
// BUT NOT LIMITED TO,  PROCUREMENT OF  SUBSTITUTE  GOODS OR  SERVICES;  LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY OF
// LIABILITY,  WHETHER IN CONTRACT,  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef DAO_STREAM_H
#define DAO_STREAM_H

#include<stdio.h>
#include<wchar.h>

#include"daoType.h"

typedef struct DaoStdStream  DaoStdStream;

enum DaoStreamModes
{
	DAO_STREAM_READABLE  = 1<<0 ,
	DAO_STREAM_WRITABLE  = 1<<1 ,
	DAO_STREAM_AUTOCONV  = 1<<2 ,
	DAO_STREAM_HIGHLIGHT = 1<<3 ,
	DAO_STREAM_DEBUGGING = 1<<4
};


struct DaoStream
{
	DAO_CSTRUCT_COMMON;

	/*
	// Read at most count bytes from the stream;
	// Return the number of readed bytes;
	// Return -1 on error;
	// Not null for a readable stream;
	//
	// If appliable:
	// -- count = -2: read all;
	// -- count = -1: read line;
	// -- count >= 0: read count bytes;
	*/
	int (*Read) ( DaoStream *self, DString *data, int count );

	/*
	// Write count number of bytes to the stream; 
	// Return the number of written bytes;
	// Return -1 on error;
	// Not null for a writable stream;
	*/
	int (*Write)( DaoStream *self, const void *data, int count );

	/*
	// End of stream, if supported;
	*/
	int (*AtEnd)( DaoStream *self );

	/*
	// Flush writes, if supported;
	*/
	void (*Flush)( DaoStream *self );

	/*
	// Set text highlighting color (terminal color on screen stdout);
	*/
	int (*SetColor)( DaoStream *self, const char *fgcolor, const char *bgcolor );

	short     mode;
	char     *format;
	DString  *buffer;
};

struct DaoStdStream
{
	DaoStream   base;
	DaoStream  *redirect;
};


DAO_DLL DaoStream* DaoStream_New( DaoVmSpace *vms );
DAO_DLL DaoStream* DaoStdStream_New( DaoVmSpace *vms );

DAO_DLL void DaoStream_Delete( DaoStream *self );
DAO_DLL void DaoStream_SetStringMode( DaoStream *self );
DAO_DLL void DaoStream_Flush( DaoStream *self );

DAO_DLL void DaoStream_WriteChar( DaoStream *self, char val );
DAO_DLL void DaoStream_WriteInt( DaoStream *self, dao_integer val );
DAO_DLL void DaoStream_WriteFloat( DaoStream *self, double val );
DAO_DLL void DaoStream_WriteString( DaoStream *self, DString *val );
DAO_DLL void DaoStream_WriteLocalString( DaoStream *self, DString *val );
DAO_DLL void DaoStream_WriteChars( DaoStream *self, const char *chars );
DAO_DLL void DaoStream_WritePointer( DaoStream *self, void *pointer );
DAO_DLL void DaoStream_WriteNewLine( DaoStream *self );

DAO_DLL daoint DaoStream_Read( DaoStream *self, DString *output, daoint count );
DAO_DLL daoint DaoStream_ReadBytes( DaoStream *self, void *output, daoint count );
DAO_DLL daoint DaoStream_WriteBytes( DaoStream *self, const void *bytes, daoint count );

DAO_DLL int DaoStream_IsOpen( DaoStream *self );
DAO_DLL int DaoStream_EndOfStream( DaoStream *self );
DAO_DLL int DaoStream_IsReadable( DaoStream *self );
DAO_DLL int DaoStream_IsWritable( DaoStream *self );

DAO_DLL int DaoStream_SetColor( DaoStream *self, const char *fgcolor, const char *bgcolor );
DAO_DLL void DaoStream_TryHighlight( DaoStream *self, int tag );
DAO_DLL void DaoStream_PrintHL( DaoStream *self, int tag, const char *text );

DAO_DLL int DaoStream_ReadLine( DaoStream *self, DString *buf );
DAO_DLL int DaoStream_ReadStdin( DaoStream *self, DString *data, int count );
DAO_DLL int DaoStream_WriteStdout( DaoStream *self, const void *output, int count );
DAO_DLL int DaoStream_WriteStderr( DaoStream *self, const void *output, int count );
DAO_DLL void DaoStream_FlushStdout( DaoStream *self );

DAO_DLL int DaoStdStream_ReadStdin( DaoStream *self, DString *data, int count );
DAO_DLL int DaoStdStream_WriteStdout( DaoStream *self, const void *output, int count );
DAO_DLL int DaoStdStream_WriteStderr( DaoStream *self, const void *output, int count );

DAO_DLL int DaoFile_ReadLine( FILE *fin, DString *line );
DAO_DLL int DaoFile_ReadAll( FILE *fin, DString *output, int close );
DAO_DLL int DaoFile_ReadPart( FILE *fin, DString *output, daoint offset, daoint count );
DAO_DLL int DaoFile_WriteString( FILE *fout, DString *str );

#endif
