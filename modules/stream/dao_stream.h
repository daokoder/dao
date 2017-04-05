/*
// Dao Standard Modules
// http://daoscript.org
//
// Copyright (c) 2015,2016, Limin Fu
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


#include"dao.h"

#ifndef DAO_STREAM_DLL
#ifdef DAO_STREAM
#  define DAO_STREAM_DLL DAO_DLL_EXPORT
#else
#  define DAO_STREAM_DLL DAO_DLL_IMPORT
#endif
#endif


#ifndef __DAO_MOD_STREAM_H__
#define __DAO_MOD_STREAM_H__

#include<stdlib.h>
#include<math.h>
#include"daoStream.h"


typedef struct DaoFileStream    DaoFileStream;
typedef struct DaoFileStream    DaoPipeStream;
typedef struct DaoStringStream  DaoStringStream;


struct DaoFileStream
{
	DaoStream  base;
	FILE      *file;
};


struct DaoStringStream
{
	DaoStream    base;
	dao_integer  offset;
};
#endif


DAO_API( STREAM, DaoType*, DaoFileStream_Type, (DaoVmSpace *vmspace) );
DAO_API( STREAM, DaoType*, DaoStringStream_Type, (DaoVmSpace *vmspace) );
DAO_API( STREAM, DaoType*, DaoPipeStream_Type, (DaoVmSpace *vmspace) ) ;

DAO_API( STREAM, DaoFileStream*, DaoFileStream_New, (DaoVmSpace *vmspace) );
DAO_API( STREAM, void, DaoFileStream_Delete, (DaoFileStream *self) );
DAO_API( STREAM, void, DaoFileStream_Close, (DaoFileStream *self) );
DAO_API( STREAM, void, DaoFileStream_InitCallbacks, (DaoFileStream *self) );

DAO_API( STREAM, DaoStringStream*, DaoStringStream_New, (DaoVmSpace *vmspace) );
DAO_API( STREAM, void, DaoStringStream_Delete, (DaoStringStream *self) );

DAO_API( STREAM, FILE*, DaoStream_GetFile, (DaoStream *self ) );
DAO_API( STREAM, DaoStream*, DaoProcess_PutFile, (DaoProcess *self, FILE *file) );
DAO_API( STREAM, DaoStream*, DaoProcess_NewStream, (DaoProcess *self, FILE *file) );

