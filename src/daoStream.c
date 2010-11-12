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

#include"string.h"
#include"daoStream.h"
#include"daoVmspace.h"
#include"daoRoutine.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoNumtype.h"
#include"daoNamespace.h"

void DaoStream_Flush( DaoStream *self )
{
	DaoVmSpace *vms = self->vmSpace;
	if( self->file ){
		fflush( self->file->fd );
	}else if( vms && vms->userHandler && vms->userHandler->StdioFlush ){
		vms->userHandler->StdioFlush( vms->userHandler );
	}else{
		fflush( stdout );
	}
}
static void DaoIO_Print0( DaoStream *self, DaoContext *ctx, DValue *p[], int N )
{
	DMap *cycData;
	int i;
	if( (self->attribs & (DAO_IO_FILE | DAO_IO_PIPE)) && self->file == NULL ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "stream is not open!" );
		return;
	}
	cycData = DMap_New(0,0);
	for(i=0; i<N; i++) DValue_Print( *p[i], ctx, self, cycData );
	DMap_Delete( cycData );
}
static void DaoIO_Print( DaoContext *ctx, DValue *p[], int N )
{
	DaoIO_Print0( p[0]->v.stream, ctx, p+1, N-1 );
}
static void DaoIO_Print2( DaoContext *ctx, DValue *p[], int N )
{
	DaoIO_Print0( ctx->vmSpace->stdStream, ctx, p, N );
}
static void DaoIO_Println0( DaoStream *self, DaoContext *ctx, DValue *p[], int N )
{
	DMap *cycData;
	int i;
	if( (self->attribs & (DAO_IO_FILE | DAO_IO_PIPE)) && self->file == NULL ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "stream is not open!" );
		return;
	}
	cycData = DMap_New(0,0);
	for(i=0; i<N; i++){
		DValue_Print( *p[i], ctx, self, cycData );
		if( i+1<N ) DaoStream_WriteMBS( self, " ");
	}
	DMap_Delete( cycData );
	DaoStream_WriteMBS( self, "\n");
}
static void DaoIO_Println( DaoContext *ctx, DValue *p[], int N )
{
	DaoIO_Println0( p[0]->v.stream, ctx, p+1, N-1 );
}
static void DaoIO_Println2( DaoContext *ctx, DValue *p[], int N )
{
	DaoIO_Println0( ctx->vmSpace->stdStream, ctx, p, N );
}
static void DaoIO_Printf0( DaoStream *self, DaoContext *ctx, DValue *p[], int N )
{
	DString *mbs;
	DMap *cycData;
	const char *convs = "diouxXfeEgGaAcCsSpm";
	char *s, *fmt, *format = DString_GetMBS( p[0]->v.s );
	char ch;
	int i, j;
	if( (self->attribs & (DAO_IO_FILE | DAO_IO_PIPE)) && self->file == NULL ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "stream is not open!" );
		return;
	}
	mbs = DString_New(1);
	cycData = DMap_New(0,0);
	DString_SetMBS( mbs, format );
	s = mbs->mbs;
	i = 0;
	j = 1;
	while( *s ){
		if( *s =='%' ){
			fmt = s;
			if( s[1] =='%' ){
				DaoStream_WriteMBS( self, "%");
				s += 2;
				continue;
			}
			while( *s ){
				if( ( *s >= 'a' && *s <= 'z' ) || (  *s >= 'A' && *s <= 'Z' ) ) break;
				s ++;
			}
			if( *s == 'l' ) s ++;
			if( strchr( convs, *s ) ==NULL ){
				DaoContext_RaiseException( ctx, DAO_WARNING, "invalid format conversion" );
			}else{
				ch = s[1];
				s[1] = 0;
				self->format = fmt;
				if( j < N ) DValue_Print( *p[j], ctx, self, cycData );
				j ++;
				self->format = NULL;
				s[1] = ch;
			}
		}else{
			DaoStream_WriteChar( self, *s );
		}
		s ++;
	}
	DString_Delete( mbs );
	DMap_Delete( cycData );
}
static void DaoIO_Printf( DaoContext *ctx, DValue *p[], int N )
{
	DaoIO_Printf0( p[0]->v.stream, ctx, p+1, N-1 );
}
static void DaoIO_Printf2( DaoContext *ctx, DValue *p[], int N )
{
	DaoIO_Printf0( ctx->vmSpace->stdStream, ctx, p, N );
}
static void DaoIO_Flush( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *self = p[0]->v.stream;
	DaoStream_Flush( self );
}
static void DaoIO_Read( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *self = ctx->vmSpace->stdStream;
	DaoVmSpace *vms = self->vmSpace;
	DString *ds = DaoContext_PutMBString( ctx, "" );
	int ch, count = 0;
	if( N >0 ) self = p[0]->v.stream;
	if( N >1 ) count = p[1]->v.i;
	if( (self->attribs & (DAO_IO_FILE | DAO_IO_PIPE)) && self->file == NULL ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "stream is not open!" );
		return;
	}
	if( self->file == NULL && vms && vms->userHandler && vms->userHandler->StdioRead ){
		vms->userHandler->StdioRead( vms->userHandler, ds, count );
	}else if( count ){
		FILE *fd = stdin;
		char buf[IO_BUF_SIZE];
		DString_Clear( ds );
		if( self->file ) fd = self->file->fd;
		if( count >0 ){
			while( count >0 ){
				size_t rest = count;
				if( rest > IO_BUF_SIZE ) rest = IO_BUF_SIZE;
				count -= rest;
				rest = fread( buf, 1, rest, fd );
				if( rest ==0 ) break;
				DString_AppendDataMBS( ds, buf, rest );
			}
		}else{
			while( 1 ){
				size_t rest = fread( buf, 1, IO_BUF_SIZE, fd );
				if( rest ==0 ) break;
				DString_AppendDataMBS( ds, buf, rest );
			}
		}
		if( fd == stdin ) fseek( stdin, 0, SEEK_END );
	}else{
		DaoStream_ReadLine( self, ds );
	}
}

extern void Dao_MakePath( DString *base, DString *path );
static void DaoIO_MakePath( DaoContext *ctx, DString *path )
{
	DString_ToMBS( path );
	if( path->size ==0 ) return;
	if( path->mbs[0] != ':' ) return;
	if( path->mbs[1] == ':' ){
		DString_Erase( path, 0, 2 );
		Dao_MakePath( ctx->nameSpace->path, path );
		return;
	}
	DString_Erase( path, 0, 1 );
	Dao_MakePath( ctx->vmSpace->pathWorking, path );
}
static void DaoIO_ReadFile( DaoContext *ctx, DValue *p[], int N )
{
	char buf[IO_BUF_SIZE];
	DString *res = DaoContext_PutMBString( ctx, "" );
	dint silent = p[1]->v.i;
	if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
		return;
	}
	if( DString_Size( p[0]->v.s ) ==0 ){
		while(1){
			size_t count = fread( buf, 1, IO_BUF_SIZE, stdin );
			if( count ==0 ) break;
			DString_AppendDataMBS( res, buf, count );
		}
	}else{
		DString *fname = DString_Copy( p[0]->v.s );
		FILE *fin;
		DString_ToMBS( fname );
		DaoIO_MakePath( ctx, fname );
		fin = fopen( fname->mbs, "r" );
		DString_Delete( fname );
		if( fin == NULL ){
			if( silent ) return;
			snprintf( buf, IO_BUF_SIZE, "file not exist: %s", DString_GetMBS( p[0]->v.s ) );
			DaoContext_RaiseException( ctx, DAO_ERROR, buf );
			return;
		}
		while(1){
			size_t count = fread( buf, 1, IO_BUF_SIZE, fin );
			if( count ==0 ) break;
			DString_AppendDataMBS( res, buf, count );
		}
		fclose( fin );
	}
}
static void DaoIO_Open( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *stream = NULL;
	if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
		return;
	}
	stream = DaoStream_New();
	stream->file = (DFile*)dao_malloc( sizeof(DFile) );
	stream->file->rc = 1;
	stream->attribs |= DAO_IO_FILE;
	if( N==0 ){
		stream->file->fd = tmpfile();
	}else{
		char buf[100];
		DString *fname = stream->fname;
		DString_Assign( fname, p[0]->v.s );
		DString_ToMBS( fname );
		snprintf( buf, 99, "file opening, %s", fname->mbs );
		if( DString_Size( fname ) >0 ){
			DaoIO_MakePath( ctx, fname );
			stream->file->fd = fopen( DString_GetMBS( fname ), DString_GetMBS( p[1]->v.s ) );
			if( stream->file->fd == NULL ){
				dao_free( stream->file );
				stream->file = NULL;
				DaoContext_RaiseException( ctx, DAO_ERROR, buf );
			}
		}else{
			DaoContext_RaiseException( ctx, DAO_ERROR, buf );
		}
	}
	DaoContext_SetResult( ctx, (DaoBase*)stream );
}
static void DaoIO_Close( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *self = p[0]->v.stream;
	DaoStream_Close( self );
}
static void DaoIO_Eof( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *self = p[0]->v.stream;
	dint *num = DaoContext_PutInteger( ctx, 0 );
	*num = 1;
	if( self->file ) *num = feof( self->file->fd );
}
static void DaoIO_Isopen( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *self = p[0]->v.stream;
	DaoContext_PutInteger( ctx, (self->file != NULL) );
}
static void DaoIO_Seek( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *self = p[0]->v.stream;
	int options[] = { SEEK_SET, SEEK_CUR, SEEK_END };
	int where = options[ p[2]->v.e->value ];
	if( self->file == NULL ) return;
	fseek( self->file->fd, p[1]->v.i, where );
}
static void DaoIO_Tell( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *self = p[0]->v.stream;
	dint *num = DaoContext_PutInteger( ctx, 0 );
	if( self->file == NULL ) return;
	*num = ftell( self->file->fd );
}
static void DaoIO_FileNO( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *self = p[0]->v.stream;
	dint *num = DaoContext_PutInteger( ctx, 0 );
	if( self->file == NULL ) return;
	*num = fileno( self->file->fd );
}
static void DaoIO_Name( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *self = p[0]->v.stream;
	DString *res = DaoContext_PutMBString( ctx, "" );
	DString_Assign( res, self->fname );
}
static void DaoIO_SStream( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *stream = DaoStream_New();
	if( p[0]->v.e->value == 1 ) DString_ToWCS( stream->streamString );
	stream->attribs |= DAO_IO_STRING;
	DaoContext_SetResult( ctx, (DaoBase*)stream );
}
static void DaoIO_GetString( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *self = p[0]->v.stream;
	DString *res = DaoContext_PutMBString( ctx, "" );
	DString_Assign( res, self->streamString );
	DString_Clear( self->streamString );
}
static void DaoIO_Popen( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *stream = NULL;
	DString *fname;
	if( ctx->vmSpace->options & DAO_EXEC_SAFE ){
		DaoContext_RaiseException( ctx, DAO_ERROR, "not permitted" );
		return;
	}
	stream = DaoStream_New();
	stream->file = (DFile*)dao_malloc( sizeof(DFile) );
	stream->file->rc = 1;
	stream->attribs |= DAO_IO_PIPE;
	fname = stream->fname;
	DString_Assign( fname, p[0]->v.s );
	if( DString_Size( fname ) >0 ){
		stream->file->fd = popen( DString_GetMBS( fname ), DString_GetMBS( p[1]->v.s ) );
		if( stream->file->fd == NULL ){
			dao_free( stream->file );
			stream->file = NULL;
			DaoContext_RaiseException( ctx, DAO_ERROR, "pipe opening" );
		}
	}else{
		DaoContext_RaiseException( ctx, DAO_ERROR, "pipe opening" );
	}
	DaoContext_SetResult( ctx, (DaoBase*)stream );
}
static void DaoIO_Iter( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *self = p[0]->v.stream;
	DValue *tuple = p[1]->v.tuple->items->data;
	tuple[0].v.i = 1;
	if( self->file && self->file->fd ){
		fseek( self->file->fd, 0, SEEK_SET );
		tuple[0].v.i = ! feof( self->file->fd );
	}
}
static void DaoIO_GetItem( DaoContext *ctx, DValue *p[], int N )
{
	DaoStream *self = p[0]->v.stream;
	DValue *tuple = p[1]->v.tuple->items->data;
	DaoIO_Read( ctx, p, 1 );
	tuple[0].v.i = 0;
	if( self->file && self->file->fd ) tuple[0].v.i = ! feof( self->file->fd );
}

static void DaoIO_Read2( DaoContext *ctx, DValue *p[], int N )
{
	DValue p0 = daoNullStream;
	DValue p1 = daoZeroInt;
	DValue *params[2] = {&p0, &p1};
	p0.v.stream = p[0]->v.stream;
	p1.v.i = ( p[1]->v.e->value == 0 )? 0 : -1;
	DaoIO_Read( ctx, params, N );
}

static DaoFuncItem streamMeths[] =
{
	{  DaoIO_Print,     "print( self :stream, ... )const" },
	{  DaoIO_Print2,    "print( ... )" },
	{  DaoIO_Printf,    "printf( self :stream, format : string, ... )const" },
	{  DaoIO_Printf2,   "printf( format : string, ... )" },
	{  DaoIO_Println,   "println( self :stream, ... )const" },
	{  DaoIO_Println2,  "println( ... )" },
	{  DaoIO_Print,     "write( self :stream, ... )const" },
	{  DaoIO_Print2,    "write( ... )" },
	{  DaoIO_Printf,    "writef( self :stream, format : string, ... )const" },
	{  DaoIO_Printf2,   "writef( format : string, ... )" },
	{  DaoIO_Println,   "writeln( self :stream, ... )const" },
	{  DaoIO_Println2,  "writeln( ... )" },
	{  DaoIO_Flush,     "flush( self :stream )const" },
	{  DaoIO_Read,      "read( self :stream, count=0 )const=>string" },
	{  DaoIO_Read2,     "read( self :stream, quantity :enum<line, all> )const=>string" },
	{  DaoIO_Read,      "read( )=>string" },
	{  DaoIO_ReadFile,  "read( file : string, silent=0 )=>string" },
	{  DaoIO_Open,      "open( )=>stream" },
	{  DaoIO_Open,      "open( file :string, mode :string )=>stream" },
	{  DaoIO_Popen,     "popen( cmd :string, mode :string )=>stream" },
	{  DaoIO_SStream,   "sstream( type :enum<mbs, wcs> = $mbs )=>stream" },
	{  DaoIO_GetString, "getstring( self :stream )=>string" },
	{  DaoIO_Close,     "close( self :stream )" },
	{  DaoIO_Eof,       "eof( self :stream )=>int" },
	{  DaoIO_Isopen,    "isopen( self :stream )=>int" },
	{  DaoIO_Seek,      "seek( self :stream, pos :int, from :enum<begin,current,end> )=>int" },
	{  DaoIO_Tell,      "tell( self :stream )=>int" },
	{  DaoIO_FileNO,    "fileno( self :stream )=>int" },
	{  DaoIO_Name,      "name( self :stream )=>string" },
	{  DaoIO_Iter,      "__for_iterator__( self :stream, iter : for_iterator )" },
	{  DaoIO_GetItem,   "[]( self :stream, iter : for_iterator )=>string" },
	{ NULL, NULL }
};

static DValue DaoStream_Copy( DValue *self0, DaoContext *ctx, DMap *cycData )
{
	DValue val = daoNullStream;
	DaoStream *self = self0->v.stream;
	DaoStream *stream = DaoStream_New();
	stream->vmSpace = self->vmSpace;
	if( self->file ){
		stream->file = self->file;
		self->file->rc ++;
	}
	stream->attribs = self->attribs;
	val.v.stream = stream;
	return val;
}
static DaoTypeCore streamCore =
{
	0, NULL, NULL, NULL, NULL,
	DaoBase_GetField,
	DaoBase_SetField,
	DaoBase_GetItem,
	DaoBase_SetItem,
	DaoBase_Print,
	DaoStream_Copy,
};

void DaoStream_SetFile( DaoStream *self, FILE *fd )
{
	DValue p = daoNullStream;
	DValue *p2 = & p;
	p.v.stream = self;
	DaoIO_Close( NULL, & p2, 1 );
	self->file = (DFile*)dao_malloc( sizeof(DFile) );
	self->file->rc = 1;
	self->file->fd = fd;
}
static void DaoStream_SetName( DaoStream *self, const char *name )
{
	DString_SetMBS( self->fname, name );
}
FILE* DaoStream_GetFile( DaoStream *self )
{
	if( self->file && self->file->fd ) return self->file->fd;
	return NULL;
}

DaoTypeBase streamTyper =
{
	"stream", & streamCore, NULL, (DaoFuncItem*) streamMeths, {0},
	(FuncPtrDel) DaoStream_Delete, NULL
};

DaoStream* DaoStream_New()
{
	DaoStream *self = (DaoStream*) dao_calloc( 1, sizeof(DaoStream) );
	DaoBase_Init( self, DAO_STREAM );
	self->streamString = DString_New(1);
	self->fname = DString_New(1);
	return self;
}
void DaoStream_Close( DaoStream *self )
{
	if( self->file ){
		self->file->rc --;
		if( self->file->rc == 0 ){
			fflush( self->file->fd );
			if( self->attribs & DAO_IO_PIPE )
				pclose( self->file->fd );
			else
				fclose( self->file->fd );
			dao_free( self->file );
		}
		self->file = NULL;
	}
}
void DaoStream_Delete( DaoStream *self )
{
	DaoStream_Close( self );
	DString_Delete( self->fname );
	DString_Delete( self->streamString );
	dao_free( self );
}
void DaoStream_WriteChar( DaoStream *self, char val )
{
	DaoVmSpace *vms = self->vmSpace;
	const char *format = "%c";
	if( self->redirect && self != self->redirect ){
		DaoStream_WriteChar( self->redirect, val );
	}else if( self->file ){
		fprintf( self->file->fd, format, val );
	}else if( vms && vms->userHandler && vms->userHandler->StdioWrite ){
		DString_Clear( self->streamString );
		DString_AppendChar( self->streamString, val );
		vms->userHandler->StdioWrite( vms->userHandler, (DString*)self->streamString );
	}else if( self->attribs & DAO_IO_STRING ){
		DString_AppendChar( self->streamString, val );
	}else{
		printf( format, val );
	}
}
void DaoStream_WriteFormatedInt( DaoStream *self, dint val, char *format )
{
	DaoVmSpace *vms = self->vmSpace;
	char buffer[100];
	if( self->redirect && self != self->redirect ){
		DaoStream_WriteInt( self->redirect, val );
	}else if( self->file ){
		fprintf( self->file->fd, format, val );
	}else if( vms && vms->userHandler && vms->userHandler->StdioWrite ){
		sprintf( buffer, format, val );
		DString_SetMBS( self->streamString, buffer );
		vms->userHandler->StdioWrite( vms->userHandler, (DString*)self->streamString );
	}else if( self->attribs & DAO_IO_STRING ){
		sprintf( buffer, format, val );
		DString_AppendMBS( self->streamString, buffer );
	}else{
		printf( format, val );
	}
}
void DaoStream_WriteInt( DaoStream *self, dint val )
{
	char *format = self->format;
	if( format == NULL ){
		format = sizeof(dint)==4 ? "%i" : "%lli";
	}
	DaoStream_WriteFormatedInt( self, val, format );
}
void DaoStream_WriteFloat( DaoStream *self, double val )
{
	DaoVmSpace *vms = self->vmSpace;
	char *format = self->format;
	const char *iconvs = "diouxXcC";
	char buffer[100];
	if( format && strchr( iconvs, format[ strlen(format)-1 ] ) && val ==(long)val ){
		DaoStream_WriteInt( self, (dint)val );
		return;
	}
	if( format == NULL ) format = "%f";
	if( self->redirect && self != self->redirect ){
		DaoStream_WriteFloat( self->redirect, val );
	}else if( self->file ){
		fprintf( self->file->fd, format, val );
	}else if( vms && vms->userHandler && vms->userHandler->StdioWrite ){
		sprintf( buffer, format, val );
		DString_SetMBS( self->streamString, buffer );
		vms->userHandler->StdioWrite( vms->userHandler, (DString*)self->streamString );
	}else if( self->attribs & DAO_IO_STRING ){
		sprintf( buffer, format, val );
		DString_AppendMBS( self->streamString, buffer );
	}else{
		printf( format, val );
	}
}
void DaoStream_WriteMBS( DaoStream *self, const char *val )
{
	DaoVmSpace *vms = self->vmSpace;
	char *format = self->format;
	if( format == NULL ) format = "%s";
	if( self->redirect && self != self->redirect ){
		DaoStream_WriteMBS( self->redirect, val );
	}else if( self->file ){
		fprintf( self->file->fd, format, val );
	}else if( vms && vms->userHandler && vms->userHandler->StdioWrite ){
		DString_SetMBS( self->streamString, val );
		vms->userHandler->StdioWrite( vms->userHandler, (DString*)self->streamString );
	}else if( self->attribs & DAO_IO_STRING ){
		DString_AppendMBS( self->streamString, val );
	}else{
		printf( format, val );
	}
}
void DaoStream_WriteWCS( DaoStream *self, const wchar_t *val )
{
	DaoVmSpace *vms = self->vmSpace;
	char *format = self->format;
	if( format == NULL ) format = "%ls";
	if( self->redirect && self != self->redirect ){
		DaoStream_WriteWCS( self->redirect, val );
	}else if( self->file ){
		fprintf( self->file->fd, format, val );
	}else if( vms && vms->userHandler && vms->userHandler->StdioWrite ){
		DString_SetWCS( self->streamString, val );
		vms->userHandler->StdioWrite( vms->userHandler, (DString*)self->streamString );
	}else if( self->attribs & DAO_IO_STRING ){
		DString_AppendWCS( self->streamString, val );
	}else{
		printf( format, val );
	}
}
void DString_SetWords( DString *self, const wchar_t *bytes, int count )
{
	int i;
	wchar_t *data;

	DString_ToWCS( self );
	DString_Resize( self, count );
	data = self->wcs;
	for( i=0; i<count; i++ ) data[i] = bytes[i];
}
void DaoStream_WriteString( DaoStream *self, DString *val )
{
	DaoVmSpace *vms = self->vmSpace;
	int i;
	if( self->redirect && self != self->redirect ){
		DaoStream_WriteString( self->redirect, val );
		return;
	}
	if( val->mbs ){
		const char *format = "%c";
		const char *data = val->mbs;
		if( self->file ){
			if( self->format ){
				fprintf( self->file->fd, self->format, data );
			}else{
				for(i=0; i<val->size; i++) fprintf( self->file->fd, format, data[i] );
			}
		}else if( vms && vms->userHandler && vms->userHandler->StdioWrite ){
			DString_SetDataMBS( self->streamString, data, val->size );
			vms->userHandler->StdioWrite( vms->userHandler, (DString*)self->streamString );
		}else if( self->attribs & DAO_IO_STRING ){
			DString_AppendDataMBS( self->streamString, data, val->size );
		}else{
			if( self->format ){
				printf( self->format, data );
			}else{
				for(i=0; i<val->size; i++) printf( format, data[i] );
			}
		}
	}else{
		const char *format = "%lc";
		const wchar_t *data = val->wcs;
		if( self->file ){
			if( self->format ){
				fprintf( self->file->fd, self->format, data );
			}else{
				for(i=0; i<val->size; i++) fprintf( self->file->fd, format, data[i] );
			}
		}else if( vms && vms->userHandler && vms->userHandler->StdioWrite ){
			DString_SetWords( self->streamString, data, val->size );
			vms->userHandler->StdioWrite( vms->userHandler, (DString*)self->streamString );
		}else if( self->attribs & DAO_IO_STRING ){
			DString *wcs = self->streamString;
			DString_ToWCS( self->streamString );
			DString_Resize( self->streamString, wcs->size + val->size );
			memcpy( wcs->wcs + wcs->size, val->wcs, val->size * sizeof(wchar_t) );
		}else{
			if( self->format ){
				printf( self->format, data );
			}else{
				for(i=0; i<val->size; i++) printf( format, data[i] );
			}
		}
	}
}
void DaoStream_WritePointer( DaoStream *self, void *val )
{
	DaoVmSpace *vms = self->vmSpace;
	const char *format = "%p";
	char buffer[100];
	if( self->redirect && self != self->redirect ){
		DaoStream_WritePointer( self->redirect, val );
	}else if( self->file ){
		fprintf( self->file->fd, format, val );
	}else if( vms && vms->userHandler && vms->userHandler->StdioWrite ){
		sprintf( buffer, format, val );
		DString_SetMBS( self->streamString, buffer );
		vms->userHandler->StdioWrite( vms->userHandler, (DString*)self->streamString );
	}else if( self->attribs & DAO_IO_STRING ){
		sprintf( buffer, format, val );
		DString_AppendMBS( self->streamString, buffer );
	}else{
		printf( format, val );
	}
}
void DaoStream_WriteNewLine( DaoStream *self )
{
	DaoStream_WriteMBS( self, daoConfig.iscgi ? "<br/>" : "\n" );
}
void DaoStream_PrintInfo( DaoStream *self,const char *t, DString *s, int i, const char *e, DString *x)
{
	DaoStream *stream = self->vmSpace->stdStream;
	DaoStream_WriteMBS( stream, t );
	DaoStream_WriteMBS( stream, "( " );
	DaoStream_WriteString( stream, s );
	DaoStream_WriteMBS( stream, ", line " );
	DaoStream_WriteInt( stream, i );
	DaoStream_WriteMBS( stream, "):\n" );
	if( x ){
		DaoStream_WriteString( stream, x );
		DaoStream_WriteMBS( stream, ", " );
	}
	DaoStream_WriteMBS( stream, e );
	DaoStream_WriteMBS( stream, ";\n\n" );
}
void DaoStream_ReadLine( DaoStream *self, DString *line )
{
	DaoVmSpace *vms = self->vmSpace;
	int ch, delim = '\n';
	char buf[IO_BUF_SIZE];
	char *start = buf, *end = buf + IO_BUF_SIZE;

	DString_Clear( line );
	DString_ToMBS( line );
	if( self->file ){
		FILE *fd = self->file->fd;
		if( feof( fd ) ) return;
		*start = ch = getc( fd );
		start += 1;
		while( ch != '\n' && ch != '\r' && ch != EOF ){ /* LF or CR */
			*start = ch = getc( fd );
			start += 1;
			if( start == end ){
				if( ch == EOF ) start -= 1;
				DString_AppendDataMBS( line, buf, start-buf );
				start = buf;
			}
		}
		if( ch == EOF && start != buf ) start -= 1;
		DString_AppendDataMBS( line, buf, start-buf );
		if( line->mbs[ line->size-1 ] == '\r' ){
			/* some programs may write consecutive 2 \r under Windows */
			if( feof( fd ) ) return;
			ch = getc( fd );
			if( ch == '\n' ){ /* CR + LF*/
				DString_AppendChar( line, ch );
			}else if( ch != '\r' && ch != EOF ){
				if( ch != EOF ) ungetc( ch, fd );
			}
		}
		/* the last line may be ended with newline, but followed with no line: */
		if( feof( fd ) ) return;
		if( line->mbs[ line->size-1 ] == '\n' ){
			ch = getc( fd );
			if( ch != EOF ) ungetc( ch, fd );
		}
	}else if( self->attribs & DAO_IO_STRING ){
		size_t pos = DString_FindWChar( self->streamString, delim, 0 );
		if( pos == MAXSIZE ){
			DString_Assign( line, self->streamString );
			DString_Clear( self->streamString );
		}else{
			DString_SubString( self->streamString, line, 0, pos+1 );
			DString_Erase( self->streamString, 0, pos+1 );
		}
	}else if( vms && vms->userHandler && vms->userHandler->StdioRead ){
		vms->userHandler->StdioRead( vms->userHandler, line, 0 );
	}else{
		*start = ch = getchar();
		start += 1;
		while( ch != delim && ch != EOF ){
			*start = ch = getchar();
			start += 1;
			if( start == end ){
				if( ch == EOF ) start -= 1;
				DString_AppendDataMBS( line, buf, start-buf );
				start = buf;
			}
		}
		if( ch == EOF && start != buf ) start -= 1;
		DString_AppendDataMBS( line, buf, start-buf );
		clearerr( stdin );
	}
}


#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
#include <windows.h>
#ifdef _MSC_VER
#define _stat stat
#endif
#endif

ullong_t FileChangedTime( const char *file )
{
	struct stat st;
	if( stat( file, &st ) ==0 ) return (ullong_t) st.st_mtime;
	return 0;
}
