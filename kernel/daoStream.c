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

#include"string.h"
#include"daoStream.h"
#include"daoVmspace.h"
#include"daoRoutine.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoNumtype.h"
#include"daoNamespace.h"
#include"daoValue.h"

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
static void DaoIO_Write0( DaoStream *self, DaoProcess *proc, DaoValue *p[], int N )
{
	DMap *cycData;
	int i;
	if( (self->attribs & (DAO_IO_FILE | DAO_IO_PIPE)) && self->file == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "stream is not open!" );
		return;
	}
	cycData = DMap_New(0,0);
	for(i=0; i<N; i++) DaoValue_Print( p[i], proc, self, cycData );
	DMap_Delete( cycData );
}
static void DaoIO_Write( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	if( ( self->mode & DAO_IO_WRITE ) == 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "stream is not writable" );
		return;
	}
	DaoIO_Write0( self, proc, p+1, N-1 );
}
static void DaoIO_Write2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoIO_Write0( proc->vmSpace->stdStream, proc, p, N );
}
static void DaoIO_Writeln0( DaoStream *self, DaoProcess *proc, DaoValue *p[], int N )
{
	DMap *cycData;
	int i;
	if( (self->attribs & (DAO_IO_FILE | DAO_IO_PIPE)) && self->file == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "stream is not open!" );
		return;
	}
	cycData = DMap_New(0,0);
	for(i=0; i<N; i++){
		DaoValue_Print( p[i], proc, self, cycData );
		if( i+1<N ) DaoStream_WriteMBS( self, " ");
	}
	DMap_Delete( cycData );
	DaoStream_WriteMBS( self, "\n");
}
static void DaoIO_Writeln( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	if( ( self->mode & DAO_IO_WRITE ) == 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "stream is not writable" );
		return;
	}
	DaoIO_Writeln0( self, proc, p+1, N-1 );
}
static void DaoIO_Writeln2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoIO_Writeln0( proc->vmSpace->stdStream, proc, p, N );
}
static void DaoIO_Writef0( DaoStream *self, DaoProcess *proc, DaoValue *p[], int N )
{
	DString *mbs;
	DMap *cycData;
	const char *convs = "diouxXfeEgGaAcCsSpm";
	char *s, *fmt, *format = DString_GetMBS( p[0]->xString.data );
	char ch;
	int i, j;
	if( (self->attribs & (DAO_IO_FILE | DAO_IO_PIPE)) && self->file == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "stream is not open!" );
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
			if( *s == 'l' ) s ++;
			if( strchr( convs, *s ) ==NULL ){
				DaoProcess_RaiseException( proc, DAO_WARNING, "invalid format conversion" );
			}else{
				ch = s[1];
				s[1] = 0;
				self->format = fmt;
				if( j < N ) DaoValue_Print( p[j], proc, self, cycData );
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
static void DaoIO_Writef( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	if( ( self->mode & DAO_IO_WRITE ) == 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "stream is not writable" );
		return;
	}
	DaoIO_Writef0( self, proc, p+1, N-1 );
}
static void DaoIO_Writef2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoIO_Writef0( proc->vmSpace->stdStream, proc, p, N );
}
static void DaoIO_Flush( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	DaoStream_Flush( self );
}
static void DaoIO_Read( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = proc->vmSpace->stdStream;
	DaoVmSpace *vms = self->vmSpace;
	DString *ds = DaoProcess_PutMBString( proc, "" );
	int count = 0;
	if( N >0 ) self = & p[0]->xStream;
	if( N >1 ) count = p[1]->xInteger.value;
	if( (self->attribs & (DAO_IO_FILE | DAO_IO_PIPE)) && self->file == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "stream is not open!" );
		return;
	}
	if( ( self->mode & DAO_IO_READ ) == 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "stream is not readable" );
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
static void DaoIO_MakePath( DaoProcess *proc, DString *path )
{
	DString_ToMBS( path );
	if( path->size ==0 ) return;
	if( path->mbs[0] != ':' ) return;
	if( path->mbs[1] == ':' ){
		DString_Erase( path, 0, 2 );
		Dao_MakePath( proc->activeNamespace->path, path );
		return;
	}
	DString_Erase( path, 0, 1 );
	Dao_MakePath( proc->vmSpace->pathWorking, path );
}
static void DaoIO_ReadFile( DaoProcess *proc, DaoValue *p[], int N )
{
	char buf[IO_BUF_SIZE];
	DString *res = DaoProcess_PutMBString( proc, "" );
	dint silent = p[1]->xInteger.value;
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	if( DString_Size( p[0]->xString.data ) ==0 ){
		while(1){
			size_t count = fread( buf, 1, IO_BUF_SIZE, stdin );
			if( count ==0 ) break;
			DString_AppendDataMBS( res, buf, count );
		}
	}else{
		DString *fname = DString_Copy( p[0]->xString.data );
		FILE *fin;
		DString_ToMBS( fname );
		DaoIO_MakePath( proc, fname );
		fin = fopen( fname->mbs, "r" );
		DString_Delete( fname );
		if( fin == NULL ){
			if( silent ) return;
			snprintf( buf, IO_BUF_SIZE, "file not exist: %s", DString_GetMBS( p[0]->xString.data ) );
			DaoProcess_RaiseException( proc, DAO_ERROR, buf );
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
static void DaoIO_Open( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stream = NULL;
	char *mode;
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	stream = DaoStream_New();
	stream->file = (DFile*)dao_malloc( sizeof(DFile) );
	stream->file->rc = 1;
	stream->attribs |= DAO_IO_FILE;
	if( N==0 ){
		stream->file->fd = tmpfile();
		if( stream->file->fd <= 0 ){
			DaoProcess_RaiseException( proc, DAO_ERROR, "failed to create a temporary file" );
			return;
		}
	}else{
		char buf[100];
		DString *fname = stream->fname;
		DString_Assign( fname, p[0]->xString.data );
		DString_ToMBS( fname );
		snprintf( buf, 99, "file opening, %s", fname->mbs );
		if( DString_Size( fname ) >0 ){
			DaoIO_MakePath( proc, fname );
			mode = DString_GetMBS( p[1]->xString.data );
			stream->file->fd = fopen( DString_GetMBS( fname ), mode );
			if( stream->file->fd == NULL ){
				dao_free( stream->file );
				stream->file = NULL;
				DaoProcess_RaiseException( proc, DAO_ERROR, buf );
			}
			stream->mode = 0;
			if( strstr( mode, "+" ) )
				stream->mode = DAO_IO_WRITE | DAO_IO_READ;
			else{
				if( strstr( mode, "r" ) )
					stream->mode |= DAO_IO_READ;
				if( strstr( mode, "w" ) || strstr( mode, "a" ) )
					stream->mode |= DAO_IO_WRITE;
			}
		}else{
			DaoProcess_RaiseException( proc, DAO_ERROR, buf );
		}
	}
	DaoProcess_PutValue( proc, (DaoValue*)stream );
}
static void DaoIO_Close( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	DaoStream_Close( self );
}
static void DaoIO_Eof( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	dint *num = DaoProcess_PutInteger( proc, 0 );
	*num = 1;
	if( self->file ) *num = feof( self->file->fd );
}
static void DaoIO_Isopen( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	DaoProcess_PutInteger( proc, (self->file != NULL) );
}
static void DaoIO_Seek( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	int options[] = { SEEK_SET, SEEK_CUR, SEEK_END };
	int where = options[ p[2]->xEnum.value ];
	if( self->file == NULL ) return;
	fseek( self->file->fd, p[1]->xInteger.value, where );
}
static void DaoIO_Tell( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	dint *num = DaoProcess_PutInteger( proc, 0 );
	if( self->file == NULL ) return;
	*num = ftell( self->file->fd );
}
static void DaoIO_FileNO( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	dint *num = DaoProcess_PutInteger( proc, 0 );
	if( self->file == NULL ) return;
	*num = fileno( self->file->fd );
}
static void DaoIO_Name( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	DString *res = DaoProcess_PutMBString( proc, "" );
	DString_Assign( res, self->fname );
}
static void DaoIO_SStream( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stream = DaoStream_New();
	if( p[0]->xEnum.value == 1 ) DString_ToWCS( stream->streamString );
	stream->attribs |= DAO_IO_STRING;
	DaoProcess_PutValue( proc, (DaoValue*)stream );
}
static void DaoIO_GetString( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	DString *res = DaoProcess_PutMBString( proc, "" );
	DString_Assign( res, self->streamString );
	DString_Clear( self->streamString );
}
static void DaoIO_Popen( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stream = NULL;
	char *mode;
	DString *fname;
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	stream = DaoStream_New();
	stream->file = (DFile*)dao_malloc( sizeof(DFile) );
	stream->file->rc = 1;
	stream->attribs |= DAO_IO_PIPE;
	fname = stream->fname;
	DString_Assign( fname, p[0]->xString.data );
	if( DString_Size( fname ) >0 ){
		mode = DString_GetMBS( p[1]->xString.data );
		stream->file->fd = popen( DString_GetMBS( fname ), mode );
		if( stream->file->fd == NULL ){
			dao_free( stream->file );
			stream->file = NULL;
			DaoProcess_RaiseException( proc, DAO_ERROR, "pipe opening" );
		}
		stream->mode = 0;
		if( strstr( mode, "+" ) )
			stream->mode = DAO_IO_WRITE | DAO_IO_READ;
		else{
			if( strstr( mode, "r" ) )
				stream->mode |= DAO_IO_READ;
			if( strstr( mode, "w" ) || strstr( mode, "a" ) )
				stream->mode |= DAO_IO_WRITE;
		}
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "pipe opening" );
	}
	DaoProcess_PutValue( proc, (DaoValue*)stream );
}
static void DaoIO_Iter( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	DaoValue **tuple = p[1]->xTuple.items;
	tuple[0]->xInteger.value = 1;
	if( self->file && self->file->fd ){
		fseek( self->file->fd, 0, SEEK_SET );
		tuple[0]->xInteger.value = ! feof( self->file->fd );
	}
}
static void DaoIO_GetItem( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	DaoValue **tuple = p[1]->xTuple.items;
	DaoIO_Read( proc, p, 1 );
	tuple[0]->xInteger.value = 0;
	if( self->file && self->file->fd ) tuple[0]->xInteger.value = ! feof( self->file->fd );
}

static void DaoIO_Read2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoInteger mode = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *params[2] = { NULL, NULL };
	params[0] = p[0];
	params[1] = (DaoValue*) & mode;
	mode.value = ( p[1]->xEnum.value == 0 )? 0 : -1;
	DaoIO_Read( proc, params, N );
}

static void DaoIO_Mode( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	char buf[10] = {0};
	if( self->mode & DAO_IO_READ ) strcat( buf, "$read" );
	if( self->mode & DAO_IO_WRITE ) strcat( buf, "$write" );
	DaoProcess_PutEnum( proc, buf );
}

static void DaoIO_ReadLines( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *fname;
	DaoValue *res;
	DaoString *line;
	DaoVmCode *sect = proc->activeCode + 2;
	DaoList *list = DaoProcess_PutList( proc );
	int chop = p[1]->xInteger.value;
	char buf[IO_BUF_SIZE];
	FILE *fin;
	if( proc->vmSpace->options & DAO_EXEC_SAFE ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "not permitted" );
		return;
	}
	fname = DString_Copy( p[0]->xString.data );
	DString_ToMBS( fname );
	DaoIO_MakePath( proc, fname );
	fin = fopen( fname->mbs, "r" );
	DString_Delete( fname );
	if( fin == NULL ){
		snprintf( buf, IO_BUF_SIZE, "file not exist: %s", DString_GetMBS( p[0]->xString.data ) );
		DaoProcess_RaiseException( proc, DAO_ERROR, buf );
		return;
	}
	if( sect->code != DVM_SECT ){
		line = DaoString_New(1);
		while( DaoFile_ReadLine( fin, line->data ) ){
			if( chop ) DString_Chop( line->data );
			DaoList_Append( list, (DaoValue*) line );
		}
		DaoString_Delete( line );
	}else{
		DaoString tmp = {DAO_STRING,0,0,0,1,NULL};
		tmp.data = p[0]->xString.data;
		line = (DaoString*) DaoProcess_SetValue( proc, sect->a, (DaoValue*)(void*) &tmp );
		while( DaoFile_ReadLine( fin, line->data ) ){
			if( chop ) DString_Chop( line->data );
			DaoProcess_ExecuteSection( proc, proc->topFrame->prev->entry + 1 );
			if( proc->status == DAO_VMPROC_ABORTED ) break;
			res = proc->stackValues[0];
			if( res && res->type != DAO_NULL ) DaoList_Append( list, res );
		}
	}
	fclose( fin );
}
static void DaoIO_ReadLines2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoValue *res;
	DaoString *line;
	DaoVmCode *sect = proc->activeCode + 2;
	DaoList *list = DaoProcess_PutList( proc );
	DaoStream *self = & p[0]->xStream;
	int i = 0, count = p[1]->xInteger.value;
	int chop = p[2]->xInteger.value;
	char buf[IO_BUF_SIZE];

	if( sect->code != DVM_SECT ){
		line = DaoString_New(1);
		while( (i++) < count && DaoStream_ReadLine( self, line->data ) ){
			if( chop ) DString_Chop( line->data );
			DaoList_Append( list, (DaoValue*) line );
		}
		DaoString_Delete( line );
	}else{
		DaoString tmp = {DAO_STRING,0,0,0,1,NULL};
		DString tmp2 = DString_WrapMBS( "" );
		tmp.data = & tmp2;
		line = (DaoString*) DaoProcess_SetValue( proc, sect->a, (DaoValue*)(void*) &tmp );
		while( (i++) < count && DaoStream_ReadLine( self, line->data ) ){
			if( chop ) DString_Chop( line->data );
			DaoProcess_ExecuteSection( proc, proc->topFrame->prev->entry + 1 );
			if( proc->status == DAO_VMPROC_ABORTED ) break;
			res = proc->stackValues[0];
			if( res && res->type != DAO_NULL ) DaoList_Append( list, res );
		}
	}
}

static DaoFuncItem streamMeths[] =
{
	{ DaoIO_Write,     "write( self :stream, ... )" },
	{ DaoIO_Write2,    "write( ... )" },
	{ DaoIO_Writef,    "writef( self :stream, format : string, ... )" },
	{ DaoIO_Writef2,   "writef( format : string, ... )" },
	{ DaoIO_Writeln,   "writeln( self :stream, ... )" },
	{ DaoIO_Writeln2,  "writeln( ... )" },
	{ DaoIO_Flush,     "flush( self :stream )" },
	{ DaoIO_Read,      "read( self :stream, count=0 )=>string" },
	{ DaoIO_Read2,     "read( self :stream, quantity :enum<line, all> )=>string" },
	{ DaoIO_Read,      "read( )=>string" },
	{ DaoIO_ReadFile,  "read( file : string, silent=0 )=>string" },
	{ DaoIO_Open,      "open( )=>stream" },
	{ DaoIO_Open,      "open( file :string, mode :string )=>stream" },
	{ DaoIO_Popen,     "popen( cmd :string, mode :string )=>stream" },
	{ DaoIO_SStream,   "sstream( type :enum<mbs, wcs> = $mbs )=>stream" },
	{ DaoIO_GetString, "getstring( self :stream )=>string" },
	{ DaoIO_Close,     "close( self :stream )" },
	{ DaoIO_Eof,       "eof( self :stream )=>int" },
	{ DaoIO_Isopen,    "isopen( self :stream )=>int" },
	{ DaoIO_Seek,      "seek( self :stream, pos :int, from :enum<begin,current,end> )=>int" },
	{ DaoIO_Tell,      "tell( self :stream )=>int" },
	{ DaoIO_FileNO,    "fileno( self :stream )=>int" },
	{ DaoIO_Name,      "name( self :stream )=>string" },
	{ DaoIO_Mode,      "mode( self :stream )=>enum<read; write>" },
	{ DaoIO_Iter,      "__for_iterator__( self :stream, iter : for_iterator )" },
	{ DaoIO_GetItem,   "[]( self :stream, iter : for_iterator )=>string" },

	{ DaoIO_ReadLines,  "readlines( file :string, chop=0 )[line:string=>null|@T]=>list<@T>" },
	{ DaoIO_ReadLines2, "readlines( self :stream, numline=0, chop=0 )[line:string=>null|@T]=>list<@T>" },
	{ NULL, NULL }
};

static DaoValue* DaoStream_Copy( DaoValue *self0, DaoProcess *proc, DMap *cycData )
{
	DaoStream *self = & self0->xStream;
	DaoStream *stream = DaoStream_New();
	stream->vmSpace = self->vmSpace;
	if( self->file ){
		stream->file = self->file;
		self->file->rc ++;
	}
	stream->attribs = self->attribs;
	return (DaoValue*) stream;
}
static DaoTypeCore streamCore =
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoValue_GetItem,
	DaoValue_SetItem,
	DaoValue_Print,
	DaoStream_Copy,
};

void DaoStream_SetFile( DaoStream *self, FILE *fd )
{
	DaoValue *p = (DaoValue*) self;
	DaoIO_Close( NULL, & p, 1 );
	self->file = (DFile*)dao_malloc( sizeof(DFile) );
	self->file->rc = 1;
	self->file->fd = fd;
}
FILE* DaoStream_GetFile( DaoStream *self )
{
	if( self->file && self->file->fd ) return self->file->fd;
	return NULL;
}

DaoTypeBase streamTyper =
{
	"stream", & streamCore, NULL, (DaoFuncItem*) streamMeths, {0}, {0},
	(FuncPtrDel) DaoStream_Delete, NULL
};

DaoStream* DaoStream_New()
{
	DaoStream *self = (DaoStream*) dao_calloc( 1, sizeof(DaoStream) );
	DaoValue_Init( self, DAO_STREAM );
	self->streamString = DString_New(1);
	self->fname = DString_New(1);
	self->mode = DAO_IO_READ | DAO_IO_WRITE;
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
	self->mode = DAO_IO_WRITE | DAO_IO_READ;
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
			int size = 0;
			DString_ToWCS( self->streamString );
			size = wcs->size;
			DString_Resize( self->streamString, wcs->size + val->size );
			memcpy( wcs->wcs + size, val->wcs, val->size * sizeof(wchar_t) );
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
int DaoStream_ReadLine( DaoStream *self, DString *line )
{
	DaoVmSpace *vms = self->vmSpace;
	int ch, delim = '\n';
	char buf[IO_BUF_SIZE];
	char *start = buf, *end = buf + IO_BUF_SIZE;

	DString_Clear( line );
	DString_ToMBS( line );
	if( self->file ){
		return DaoFile_ReadLine( self->file->fd, line );
	}else if( self->attribs & DAO_IO_STRING ){
		size_t pos = DString_FindWChar( self->streamString, delim, 0 );
		if( pos == MAXSIZE ){
			DString_Assign( line, self->streamString );
			DString_Clear( self->streamString );
		}else{
			DString_SubString( self->streamString, line, 0, pos+1 );
			DString_Erase( self->streamString, 0, pos+1 );
		}
		return self->streamString->size >0;
	}else if( vms && vms->userHandler && vms->userHandler->StdioRead ){
		vms->userHandler->StdioRead( vms->userHandler, line, 0 );
		return line->size >0;
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
		return ch != EOF;
	}
	return 0;
}
int DaoFile_ReadLine( FILE *fin, DString *line )
{
	int ch;
	char buf[IO_BUF_SIZE];
	char *start = buf, *end = buf + IO_BUF_SIZE;

	DString_Clear( line );
	DString_ToMBS( line );
	if( feof( fin ) ) return 0;

	*start = ch = getc( fin );
	start += 1;
	while( ch != '\n' && ch != '\r' && ch != EOF ){ /* LF or CR */
		*start = ch = getc( fin );
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
		if( feof( fin ) ) return 1;
		ch = getc( fin );
		if( ch == '\n' ){ /* CR + LF*/
			DString_AppendChar( line, ch );
		}else if( ch != '\r' && ch != EOF ){
			if( ch != EOF ) ungetc( ch, fin );
		}
	}
	/* the last line may be ended with newline, but followed with no line: */
	if( feof( fin ) ) return 1;
	if( line->mbs[ line->size-1 ] == '\n' ){
		ch = getc( fin );
		if( ch != EOF ) ungetc( ch, fin );
	}
	return 1;
}


#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
#include <windows.h>
#if defined(_MSC_VER) && !defined(_stat)
#define _stat stat
#endif
#endif

ulong_t FileChangedTime( const char *file )
{
	struct stat st;
	if( stat( file, &st ) ==0 ) return (ulong_t) st.st_mtime;
	return 0;
}
