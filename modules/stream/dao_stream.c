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

#define DAO_STREAM

#include<time.h>
#include<string.h>
#include<errno.h>
#include"dao_stream.h"
#include"daoValue.h"
#include"daoVmspace.h"

#ifdef WIN32
#include<windows.h>
#include<lmcons.h>
#define putenv _putenv

#  ifndef __GNUC__
#    define fdopen _fdopen
#    define popen  _popen
#    define pclose _pclose
#  endif

#else
#include<sys/wait.h>
#endif


extern DaoTypeCore daoFileStreamCore;
extern DaoTypeCore daoPipeStreamCore;
extern DaoTypeCore daoStringStreamCore;


DaoType* DaoFileStream_Type( DaoVmSpace *vmspace )
{
	return DaoVmSpace_GetType( vmspace, & daoFileStreamCore );
}
DaoType* DaoStringStream_Type( DaoVmSpace *vmspace )
{
	return DaoVmSpace_GetType( vmspace, & daoStringStreamCore );
}
DaoType* DaoPipeStream_Type( DaoVmSpace *vmspace )
{
	return DaoVmSpace_GetType( vmspace, & daoPipeStreamCore );
}


static int DaoFileStream_AtEnd( DaoStream *stream );

static int DaoFileStream_Read( DaoStream *stream, DString *data, int count )
{
	DaoFileStream *self = (DaoFileStream*) stream;

	DString_Reset( data, 0 );
	if( DaoFileStream_AtEnd( stream ) ) return -1;
	if( count >= 0 ){
		DString_Reset( data, count );
		DString_Reset( data, fread( data->chars, 1, count, self->file ) );
	}else if( count == -1 ){
		DaoFile_ReadLine( self->file, data );
	}else{
		DaoFile_ReadAll( self->file, data, 0 );
	}
	return data->size;
}
static int DaoFileStream_Write( DaoStream *stream, const void *data, int count )
{
	DaoFileStream *self = (DaoFileStream*) stream;
	DString bytes = DString_WrapBytes( (char*) data, count );
	DaoFile_WriteString( self->file, & bytes );
	return count;
}
static int DaoFileStream_AtEnd( DaoStream *stream )
{
	DaoFileStream *self = (DaoFileStream*) stream;
	return feof( self->file );
}
static void DaoFileStream_Flush( DaoStream *stream )
{
	DaoFileStream *self = (DaoFileStream*) stream;
	fflush( self->file );
}


static int DaoStringStream_Read( DaoStream *stream, DString *data, int count )
{
	DaoStringStream *self = (DaoStringStream*) stream;

	DString_Reset( data, 0 );
	if( self->offset >= self->base.buffer->size ) return -1;
	if( count >= 0 ){
		DString_SubString( self->base.buffer, data, self->offset, count );
		self->offset += data->size;
	}else if( count == -1 ){
		int delim = '\n';
		daoint pos = DString_FindChar( self->base.buffer, delim, self->offset );
		if( self->offset == 0 && (pos == DAO_NULLPOS || pos == self->base.buffer->size-1) ){
			DString_Append( data, self->base.buffer );
			self->offset = self->base.buffer->size;
		}else if( pos == DAO_NULLPOS ){
			DString_SubString( self->base.buffer, data, pos, -1 );
			self->offset = self->base.buffer->size;
		}else{
			DString_SubString( self->base.buffer, data, pos, pos - self->offset + 1 );
			self->offset = pos + 1;
		}
		if( self->base.mode & DAO_STREAM_AUTOCONV ) DString_ToUTF8( data );
		return self->offset < self->base.buffer->size;
	}else{
		if( self->offset == 0 ){ 
			DString_Assign( data, self->base.buffer );
		}else{
			DString_SubString( self->base.buffer, data, self->offset, -1 );
		}    
		self->offset += data->size;
	}
	return data->size;
}
static int DaoStringStream_Write( DaoStream *stream, const void *data, int count )
{
	DaoStringStream *self = (DaoStringStream*) stream;
	DString_AppendBytes( self->base.buffer, (char*) data, count );
	return count;
}
static int DaoStringStream_AtEnd( DaoStream *stream )
{
	DaoStringStream *self = (DaoStringStream*) stream;
	return self->offset >= self->base.buffer->size;
}

void GetErrorMessage( int code, char *buffer, size_t size )
{
	switch ( code ){
	case EAGAIN:
	case ENOMEM:	snprintf( buffer, size, "Failed to create process (EAGAIN/ENOMEM)" ); break;
	case EMFILE:
	case ENFILE:	snprintf( buffer, size, "Too many files open (EMFILE/ENFILE)" ); break;
	case EACCES:	snprintf( buffer, size, "Access not permitted (EACCES)" ); break;
	case EEXIST:	snprintf( buffer, size, "Failed to generate uniqe file name (EEXIST)" ); break;
	case ENOSPC:	snprintf( buffer, size, "Not enough free space in the file system (ENOSPC)" ); break;
	case EROFS:		snprintf( buffer, size, "Writing to a read-only file system (EROFS)" ); break;
	case EINVAL:	snprintf( buffer, size, "Invalid mode (EINVAL)" ); break;
	case EBADF:		snprintf( buffer, size, "Invalid file descriptor (EBADF)" ); break;
	default:		snprintf( buffer, size, "Unknown system error (%x)", code );
	}
}

DaoFileStream* DaoFileStream_NewByType( DaoType *type )
{
	DaoFileStream *self = (DaoFileStream*) dao_calloc( 1, sizeof(DaoFileStream) );
	DaoCstruct_Init( (DaoCstruct*) self, type );
	self->base.Read = NULL;
	self->base.Write = NULL;
	self->base.AtEnd = NULL;
	self->base.Flush = NULL;
	self->base.SetColor = NULL;
	return self;
}

DaoFileStream* DaoFileStream_New( DaoVmSpace *vmspace )
{
	return DaoFileStream_NewByType( DaoFileStream_Type( vmspace ) );
}
void DaoFileStream_Delete( DaoFileStream *self )
{
	DaoFileStream_Close( self );
	DaoCstruct_Free( (DaoCstruct*) self );
	dao_free( self );
}
void DaoFileStream_Close( DaoFileStream *self )
{
	self->base.mode &= ~(DAO_STREAM_WRITABLE | DAO_STREAM_READABLE);
	self->base.Read = NULL;
	self->base.Write = NULL;
	self->base.AtEnd = NULL;
	self->base.Flush = NULL;
	self->base.SetColor = NULL;
	if( self->file ){
		fflush( self->file );
		fclose( self->file );
		self->file = NULL;
	}
}
void DaoFileStream_InitCallbacks( DaoFileStream *self )
{
	self->base.AtEnd = DaoFileStream_AtEnd;
	if( self->base.mode & DAO_STREAM_READABLE ) self->base.Read = DaoFileStream_Read;
	if( self->base.mode & DAO_STREAM_WRITABLE ){
		self->base.Write = DaoFileStream_Write;
		self->base.Flush = DaoFileStream_Flush;
	}
}

DaoPipeStream* DaoPipeStream_New( DaoVmSpace *vmspace )
{
	return DaoFileStream_NewByType( DaoPipeStream_Type( vmspace ) );
}
int DaoPipeStream_Close( DaoPipeStream *self );
void DaoPipeStream_Delete( DaoPipeStream *self )
{
	DaoPipeStream_Close( self );
	DaoCstruct_Free( (DaoCstruct*) self );
	dao_free( self );
}
int DaoPipeStream_Close( DaoPipeStream *self )
{
	int ret = 0;
	self->base.mode &= ~(DAO_STREAM_WRITABLE | DAO_STREAM_READABLE);
	self->base.Read = NULL;
	self->base.Write = NULL;
	self->base.AtEnd = NULL;
	self->base.Flush = NULL;
	self->base.SetColor = NULL;
	if( self->file ){
		fflush( self->file );
		ret = pclose( self->file );
#ifdef UNIX
		if ( ret != -1 )
			ret = WEXITSTATUS( ret );
#endif
		self->file = NULL;
	}
	return ret;
}



DaoStringStream* DaoStringStream_New( DaoVmSpace *vmspace )
{
	DaoStringStream *self = (DaoStringStream*) dao_calloc( 1, sizeof(DaoStringStream) );
	DaoCstruct_Init( (DaoCstruct*) self, DaoStringStream_Type( vmspace ) );
	self->base.buffer = DString_New();
	self->base.Read = DaoStringStream_Read;
	self->base.Write = DaoStringStream_Write;
	self->base.AtEnd = DaoStringStream_AtEnd;
	self->base.Flush = NULL;
	self->base.SetColor = NULL;
	return self;
}
void DaoStringStream_Delete( DaoStringStream *self )
{
	DString_Delete( self->base.buffer );
	DaoCstruct_Free( (DaoCstruct*) self );
	dao_free( self );
}


FILE* DaoStream_GetFile( DaoStream *self )
{
	DaoVmSpace *vmspace = DaoType_GetVmSpace( self->ctype );
	if( DaoType_ChildOf( self->ctype, DaoFileStream_Type( vmspace ) ) ){
		DaoFileStream *stream = (DaoFileStream*) self;
		return stream->file;
	}else if( self->Write == DaoStdStream_WriteStdout || self->Write == DaoStdStream_WriteStderr ){
		DaoStdStream *stream = (DaoStdStream*) self;
		if( stream->redirect ){
			return DaoStream_GetFile( stream->redirect );
		}else if( self->Write == DaoStdStream_WriteStdout ){
			return stdout;
		}else if( self->Write == DaoStdStream_WriteStderr ){
			return stderr;
		}
	}else if( self->Write == DaoStream_WriteStdout ){
		return stdout;
	}else if( self->Write == DaoStream_WriteStderr ){
		return stderr;
	}
	return NULL;
}
DaoStream* DaoProcess_PutFile( DaoProcess *self, FILE *file )
{
	DaoFileStream *stream = DaoFileStream_New( self->vmSpace );
	stream->file = file;
	stream->base.mode |= DAO_STREAM_WRITABLE | DAO_STREAM_READABLE;
	DaoFileStream_InitCallbacks( stream );
	DaoProcess_PutValue( self, (DaoValue*) stream );
	return (DaoStream*) stream;
}
DaoStream* DaoProcess_NewStream( DaoProcess *self, FILE *file )
{
	DaoFileStream *stream = DaoFileStream_New( self->vmSpace );
	stream->file = file;
	stream->base.mode |= DAO_STREAM_WRITABLE | DAO_STREAM_READABLE;
	DaoFileStream_InitCallbacks( stream );
	DaoProcess_CacheValue( self, (DaoValue*) stream );
	return (DaoStream*) stream;
}


/*
// Special relative paths:
// 1. ::path, path relative to the current source code file;
// 2. :path, path relative to the current working directory;
*/
static void DaoIO_MakePath( DaoProcess *proc, DString *path )
{
	if( path->size ==0 ) return;
	if( path->chars[0] != ':' ) return;
	if( path->chars[1] == ':' ){
		DString_Erase( path, 0, 2 );
		DString_MakePath( proc->activeNamespace->path, path );
		return;
	}
	DString_Erase( path, 0, 1 );
	DString_MakePath( proc->vmSpace->pathWorking, path );
}
static FILE* DaoIO_OpenFile( DaoProcess *proc, DString *name, const char *mode, int silent )
{
	DString *fname = DString_Copy( name );
	char buf[200];
	FILE *fin;

	DaoIO_MakePath( proc, fname );
	fin = Dao_OpenFile( fname->chars, mode );
	DString_Delete( fname );
	if( fin == NULL && silent == 0 ){
		snprintf( buf, sizeof(buf), "error opening file: %s", DString_GetData( name ) );
		DaoProcess_RaiseError( proc, "Stream", buf );
		return NULL;
	}
	return fin;
}
static void DaoIO_Open( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoFileStream *self = NULL;
	char *mode;
	self = DaoFileStream_New( proc->vmSpace );
	DaoProcess_PutValue( proc, (DaoValue*)self );
	if( N == 0 ){
		do
			self->file = tmpfile();
		while ( !self->file && errno == EINTR );
		self->base.Read = DaoFileStream_Read;
		self->base.Write = DaoFileStream_Write;
		self->base.AtEnd = DaoFileStream_AtEnd;
		self->base.Flush = DaoFileStream_Flush;
		self->base.mode |= DAO_STREAM_WRITABLE | DAO_STREAM_READABLE;
		if( !self->file ){
			char errbuf[512];
			GetErrorMessage( errno, errbuf, sizeof(errbuf) );
			DaoProcess_RaiseError( proc, "Stream", errbuf );
			return;
		}
	}else{
		mode = DString_GetData( p[1]->xString.value );
		if( p[0]->type == DAO_INTEGER ){
			self->file = fdopen( p[0]->xInteger.value, mode );
			if( self->file == NULL ){
				char errbuf[512];
				GetErrorMessage( errno, errbuf, sizeof(errbuf) );
				DaoProcess_RaiseError( proc, "Stream", errbuf );
				return;
			}
		}else{
			self->file = DaoIO_OpenFile( proc, p[0]->xString.value, mode, 0 );
		}
		if( strstr( mode, "+" ) ){
			self->base.mode |= DAO_STREAM_WRITABLE | DAO_STREAM_READABLE;
		}else{
			if( strstr( mode, "r" ) ){
				self->base.mode |= DAO_STREAM_READABLE;
			}
			if( strstr( mode, "w" ) || strstr( mode, "a" ) ){
				self->base.mode |= DAO_STREAM_WRITABLE;
			}
		}
		DaoFileStream_InitCallbacks( self );
	}
}
static void DaoIO_ReadFile( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *res = DaoProcess_PutChars( proc, "" );
	daoint silent = p[1]->xBoolean.value;
	if( DString_Size( p[0]->xString.value ) ==0 ){
		char buf[4096];
		while(1){
			size_t count = fread( buf, 1, sizeof( buf ), stdin );
			if( count ==0 ) break;
			DString_AppendBytes( res, buf, count );
		}
	}else{
		FILE *fin = DaoIO_OpenFile( proc, p[0]->xString.value, "r", silent );
		struct stat info;
		if( fin == NULL ) return;
		fstat( fileno( fin ), &info );
		DString_Reserve( res, info.st_size );
		DString_Reset( res, fread( res->chars, 1, info.st_size, fin ) );
		fclose( fin );
	}
}

static void DaoIO_Seek( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoFileStream *self = (DaoFileStream*) p[0];
	daoint pos = p[1]->xInteger.value;
	int options[] = { SEEK_SET, SEEK_CUR, SEEK_END };
	int where = options[ p[2]->xEnum.value ];
	if( self->file == NULL ) return;
	fseek( self->file, pos, where );
}
static void DaoIO_Tell( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoFileStream *self = (DaoFileStream*) p[0];
	dao_integer *num = DaoProcess_PutInteger( proc, 0 );
	if( self->file == NULL ) return;
	*num = ftell( self->file );
}
static void DaoIO_FileNO( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoFileStream *self = (DaoFileStream*) p[0];
	dao_integer *num = DaoProcess_PutInteger( proc, 0 );
	if( self->file == NULL ) return;
	*num = fileno( self->file );
}
static void DaoIO_Close( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoFileStream *self = (DaoFileStream*) p[0];
	DaoFileStream_Close( self );
}

static void PIPE_New( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoPipeStream *stream = NULL;
	DString *fname = p[0]->xString.value;
	char *mode;

	stream = DaoPipeStream_New( proc->vmSpace );
	DaoProcess_PutValue( proc, (DaoValue*)stream );
	if( DString_Size( fname ) == 0 ){
		DaoProcess_RaiseError( proc, "Param", "Empty command" );
		return;
	}
	mode = DString_GetData( p[1]->xString.value );
	stream->file = popen( DString_GetData( fname ), mode );
	if( stream->file == NULL ){
		char errbuf[512];
		GetErrorMessage( errno, errbuf, sizeof(errbuf) );
		DaoProcess_RaiseError( proc, "Stream", errbuf );
		return;
	}
	if( strstr( mode, "r" ) ) stream->base.mode |= DAO_STREAM_READABLE;
	if( strstr( mode, "w" ) ) stream->base.mode |= DAO_STREAM_WRITABLE;
	DaoFileStream_InitCallbacks( stream );
}
static void PIPE_Close( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoPipeStream *stream = (DaoPipeStream*) p[0];
	if ( stream->file ){
		DaoProcess_PutInteger( proc, DaoPipeStream_Close( stream ) );
	} else {
		DaoProcess_RaiseError( proc, "Param", "Stream not open" );
	}
}
static void PIPE_FileNO( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoFileStream *self = (DaoPipeStream*) p[0];
	dao_integer *num = DaoProcess_PutInteger( proc, 0 );
	if( self->file == NULL ) return;
	*num = fileno( self->file );
}


static void DaoIOS_Open( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStringStream *self = DaoStringStream_New( proc->vmSpace );
	DaoProcess_PutValue( proc, (DaoValue*) self );
}
static void DaoIOS_Seek( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStringStream *self = (DaoStringStream*) p[0];
	daoint pos = p[1]->xInteger.value;
	int options[] = { SEEK_SET, SEEK_CUR, SEEK_END };
	int where = options[ p[2]->xEnum.value ];
	switch( where ){
	case SEEK_SET : self->offset  = pos; break;
	case SEEK_CUR : self->offset += pos; break;
	case SEEK_END : self->offset = self->base.buffer->size - pos; break;
	}
	if( self->offset < 0 ) self->offset = 0;
}
static void DaoIOS_Tell( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStringStream *self = (DaoStringStream*) p[0];
	DaoProcess_PutInteger( proc, self->offset );
}



static DaoFunctionEntry dao_io_methods[] =
{
	{ DaoIO_Open,      "tmpFile() => FileStream" },
	{ DaoIO_Open,      "open( file: string, mode: string ) => FileStream" },
	{ DaoIO_Open,      "open( fd: int, mode: string ) => FileStream" },

	/*! Spawns sub-process which executes the given shell \a command with redirected standard input or output depending on \a mode.
	 * If \a mode is 'r', returns readable stream of the process output; if \a mode is 'w', returns writable stream of the process
	 * input */
	{ PIPE_New,        "popen( command: string, mode: string ) => PipeStream" },

	{ DaoIO_ReadFile,  "read( file: string, silent = false )=>string" },
	{ NULL, NULL }
};

static DaoFunctionEntry daoFileStreamMeths[] =
{
	{ DaoIO_Open,    "FileStream() => FileStream" },
	{ DaoIO_Open,    "FileStream( file: string, mode: string ) => FileStream" },
	{ DaoIO_Open,    "FileStream( fd: int, mode: string ) => FileStream" },
	{ DaoIO_Close,   "close( self: FileStream )" },
	{ DaoIO_Seek,    "seek( self: FileStream, pos: int, from: enum<start,current,end> ) => int" },
	{ DaoIO_Tell,    "tell( self: FileStream ) => int" },
	{ DaoIO_FileNO,  ".fd( invar self: FileStream ) => int" },
	{ NULL, NULL }
};


DaoTypeCore daoFileStreamCore =
{
	"FileStream",                                      /* name */
	sizeof(DaoFileStream),                             /* size */
	{ NULL },                                          /* bases */
	{ NULL },                                          /* casts */
	NULL,                                              /* numbers */
	daoFileStreamMeths,                                /* methods */
	DaoCstruct_CheckGetField,  DaoCstruct_DoGetField,  /* GetField */
	NULL,                      NULL,                   /* SetField */
	NULL,                      NULL,                   /* GetItem */
	NULL,                      NULL,                   /* SetItem */
	NULL,                      NULL,                   /* Unary */
	NULL,                      NULL,                   /* Binary */
	NULL,                      NULL,                   /* Conversion */
	NULL,                      NULL,                   /* ForEach */
	NULL,                                              /* Print */
	NULL,                                              /* Slice */
	NULL,                                              /* Compare */
	NULL,                                              /* Hash */
	NULL,                                              /* Create */
	NULL,                                              /* Copy */
	(DaoDeleteFunction) DaoFileStream_Delete,          /* Delete */
	NULL                                               /* HandleGC */
};


static DaoFunctionEntry daoPipeStreamMeths[] =
{
	{ PIPE_New,     "PipeStream( file: string, mode: string ) => PipeStream" },
	{ PIPE_FileNO,  ".fd( invar self: PipeStream ) => int" },
	{ PIPE_Close,   "close( self: PipeStream ) => int" },
	{ NULL, NULL }
};

DaoTypeCore daoPipeStreamCore =
{
	"PipeStream",                                      /* name */
	sizeof(DaoPipeStream),                             /* size */
	{ NULL },                                          /* bases */
	{ NULL },                                          /* casts */
	NULL,                                              /* numbers */
	daoPipeStreamMeths,                                /* methods */
	DaoCstruct_CheckGetField,  DaoCstruct_DoGetField,  /* GetField */
	NULL,                      NULL,                   /* SetField */
	NULL,                      NULL,                   /* GetItem */
	NULL,                      NULL,                   /* SetItem */
	NULL,                      NULL,                   /* Unary */
	NULL,                      NULL,                   /* Binary */
	NULL,                      NULL,                   /* Conversion */
	NULL,                      NULL,                   /* ForEach */
	NULL,                                              /* Print */
	NULL,                                              /* Slice */
	NULL,                                              /* Compare */
	NULL,                                              /* Hash */
	NULL,                                              /* Create */
	NULL,                                              /* Copy */
	(DaoDeleteFunction) DaoPipeStream_Delete,          /* Delete */
	NULL                                               /* HandleGC */
};


static DaoFunctionEntry daoStringStreamMeths[] =
{
	{ DaoIOS_Open,  "StringStream() => StringStream" },
	{ DaoIOS_Seek,  "seek( self: StringStream, pos: int, from: enum<start,current,end> ) => int" },
	{ DaoIOS_Tell,  "tell( self: StringStream ) => int" },
	{ NULL, NULL }
};


DaoTypeCore daoStringStreamCore =
{
	"StringStream",                                    /* name */
	sizeof(DaoStringStream),                           /* size */
	{ NULL },                                          /* bases */
	{ NULL },                                          /* casts */
	NULL,                                              /* numbers */
	daoStringStreamMeths,                              /* methods */
	DaoCstruct_CheckGetField,  DaoCstruct_DoGetField,  /* GetField */
	NULL,                      NULL,                   /* SetField */
	NULL,                      NULL,                   /* GetItem */
	NULL,                      NULL,                   /* SetItem */
	NULL,                      NULL,                   /* Unary */
	NULL,                      NULL,                   /* Binary */
	NULL,                      NULL,                   /* Conversion */
	NULL,                      NULL,                   /* ForEach */
	NULL,                                              /* Print */
	NULL,                                              /* Slice */
	NULL,                                              /* Compare */
	NULL,                                              /* Hash */
	NULL,                                              /* Create */
	NULL,                                              /* Copy */
	(DaoDeleteFunction) DaoStringStream_Delete,        /* Delete */
	NULL                                               /* HandleGC */
};

static DaoFunctionEntry daoSeekableDeviceMeths[] =
{
	{ NULL,	 "seek( self: SeekableDevice, pos: int, from: enum<start,current,end> ) => int" },
	{ NULL,  "tell( self: SeekableDevice ) => int" },
	{ NULL, NULL }
};

DaoTypeCore daoSeekableDeviceCore =
{
	"SeekableDevice",        /* name */
	0,                       /* size */
	{ NULL },                /* bases */
	{ NULL },                /* casts */
	NULL,                    /* numbers */
	daoSeekableDeviceMeths,  /* methods */
	NULL,  NULL,             /* GetField */
	NULL,  NULL,             /* SetField */
	NULL,  NULL,             /* GetItem */
	NULL,  NULL,             /* SetItem */
	NULL,  NULL,             /* Unary */
	NULL,  NULL,             /* Binary */
	NULL,  NULL,             /* Conversion */
	NULL,  NULL,             /* ForEach */
	NULL,                    /* Print */
	NULL,                    /* Slice */
	NULL,                    /* Compare */
	NULL,                    /* Hash */
	NULL,                    /* Create */
	NULL,                    /* Copy */
	NULL,                    /* Delete */
	NULL                     /* HandleGC */
};


#undef DAO_STREAM
#undef DAO_STREAM_DLL
#define DAO_HAS_STREAM
#include"dao_api.h"

DAO_DLL_EXPORT int DaoStream_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace *ions = DaoVmSpace_GetNamespace( vmSpace, "io" );
	DaoType *streamType = DaoNamespace_FindTypeChars( ions, "Stream" );
	DaoType *deviceType = DaoNamespace_FindTypeChars( ions, "Device" );
	daoFileStreamCore.bases[0] = DaoType_GetTypeCore( streamType );
	daoPipeStreamCore.bases[0] = DaoType_GetTypeCore( streamType );
	daoStringStreamCore.bases[0] = DaoType_GetTypeCore( streamType );
	daoSeekableDeviceCore.bases[0] = DaoType_GetTypeCore( deviceType );
	DaoNamespace_WrapType( ions, & daoFileStreamCore, DAO_CSTRUCT, 0 );
	DaoNamespace_WrapType( ions, & daoPipeStreamCore, DAO_CSTRUCT, 0 );
	DaoNamespace_WrapType( ions, & daoStringStreamCore, DAO_CSTRUCT, 0 );
	DaoNamespace_WrapInterface( ions, & daoSeekableDeviceCore );
	DaoNamespace_WrapFunctions( ions, dao_io_methods );

#define DAO_API_INIT
#include"dao_api.h"
	return 0;
}
