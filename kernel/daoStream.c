/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2014, Limin Fu
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

#include"ctype.h"
#include"string.h"
#include"daoStream.h"
#include"daoVmspace.h"
#include"daoRoutine.h"
#include"daoProcess.h"
#include"daoNumtype.h"
#include"daoNamespace.h"
#include"daoValue.h"


void DaoStream_Flush( DaoStream *self )
{
	if( self->redirect && self->redirect->StdioFlush ){
		self->redirect->StdioFlush( self->redirect );
	}else if( self->file ){
		fflush( self->file );
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
	DaoStream *stream = proc->stdioStream;
	if( stream == NULL ) stream = proc->vmSpace->stdioStream;
	DaoIO_Write0( stream, proc, p, N );
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
	DaoStream *stream = proc->stdioStream;
	if( stream == NULL ) stream = proc->vmSpace->stdioStream;
	DaoIO_Writeln0( stream, proc, p, N );
}
/*
// C printf format: %[parameter][flags][width][.precision][length]type
//
// Dao writef format: %[parameter][flags][width][.precision]type[color]
//
// Where 'parameter', 'flags', 'width' and 'precision' will conform to the
// C format, but 'type' can only be:
//   c, d, i, o, u, x/X : for integer;
//   e/E, f/F, g/G : for float and double;
//   s : for string;
//   p : for any type, write address;
//   a : automatic, for any type, write in the default format;
// Namely the standard ones exception 'n', and plus 'a'.
//
// Optional 'color' format will be in form of: [foreground:background], [foreground]
// or [:background]. The supported color name format will depend on the color printing
// handle. Mininum requirement is the support of the following 8 color names:
// black, white, red, green, blue, yellow, magenta, cyan.
*/
static void DaoIO_Writef0( DaoStream *self, DaoProcess *proc, DaoValue *p[], int N )
{
	DaoValue *value;
	DMap *cycData;
	DString *format, *fmt2;
	DString *fgcolor, *bgcolor;
	const char *convs = "aspcdiouxXfFeEgG";
	char F, *s, *end, *fg, *bg, *fmt, message[100];
	int k, id = 1;
	if( (self->attribs & (DAO_IO_FILE | DAO_IO_PIPE)) && self->file == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "stream is not open!" );
		return;
	}
	cycData = DMap_New(0,0);
	fmt2 = DString_New(1);
	fgcolor = DString_New(1);
	bgcolor = DString_New(1);
	format = DString_Copy( p[0]->xString.data );
	DString_ToMBS( format );
	s = format->mbs;
	end = s + format->size;
	for(; s<end; s++){
		k = 0;
		if( *s =='%' ){
			fmt = s;
			s += 1;
			if( *s =='%' || *s == '[' ){
				DaoStream_WriteChar( self, *s );
				continue;
			}
			if( isdigit( *s ) && (*s > '0') ){
				while( isdigit( *s ) ) s += 1;
				if( *s == '$' ){ /* parameter: number$ */
					*s = '\0';
					k = strtol( fmt + 1, NULL, 10 );
					if( k == 0 || k >= N ){
						DaoProcess_RaiseException( proc, DAO_WARNING, "invalid parameter number" );
					}
					*s = '%';
					fmt = s ++;
				}
			}
			/* flags: */
			while( *s == '+' || *s == '-' || *s == '#' || *s == '0' || *s == ' ' ) s += 1;
			while( isdigit( *s ) ) s += 1; /* width; */
			if( *s == '.' ){ /* precision: */
				s += 1;
				while( isdigit( *s ) ) s += 1;
			}
			DString_SetDataMBS( fmt2, fmt, s - fmt + 1 );
			if( strchr( convs, *s ) == NULL ){
				DaoProcess_RaiseException( proc, DAO_WARNING, "invalid format conversion" );
				continue;
			}
			F = *s;
			s += 1;
			fg = bg = NULL;
			if( *s == '[' ){
				s += 1;
				fmt = s;
				while( isalnum( *s ) ) s += 1;
				DString_SetDataMBS( fgcolor, fmt, s - fmt );
				if( fgcolor->size ) fg = fgcolor->mbs;
				if( *s == ':' ){
					s += 1;
					fmt = s;
					while( isalnum( *s ) ) s += 1;
					DString_SetDataMBS( bgcolor, fmt, s - fmt );
					if( bgcolor->size ) bg = bgcolor->mbs;
				}
				if( *s != ']' ) DaoProcess_RaiseException( proc, DAO_WARNING, "invalid color format" );
			}else{
				s -= 1;
			}
			if( k == 0 ) k = id;
			value = p[k];
			id += 1;
			if( fg || bg ) DaoStream_SetColor( self, fg, bg );
			self->format = fmt2->mbs;
			if( value == NULL ){
				if( F == 'p' ){
					DaoStream_WriteMBS( self, "0x0" );
				}else{
					DaoProcess_RaiseException( proc, DAO_WARNING, "null parameter" );
				}
			}
			self->format = fmt2->mbs;
			if( F == 'c' || F == 'd' || F == 'i' || F == 'o' || F == 'x' || F == 'X' ){
				if( sizeof(daoint) != 4 ) DString_InsertChar( fmt2, DAO_INT_FORMAT[0], fmt2->size-1 );
				self->format = fmt2->mbs;
				if( value->type == DAO_INTEGER ){
					DaoStream_WriteInt( self, value->xInteger.value );
				}else{
					goto WrongParameter;
				}
			}else if( toupper( F ) == 'E' || toupper( F ) == 'F' || toupper( F ) == 'G' ){
				if( value->type == DAO_FLOAT ){
					DaoStream_WriteFloat( self, value->xFloat.value );
				}else if( value->type == DAO_DOUBLE ){
					DaoStream_WriteFloat( self, value->xDouble.value );
				}else{
					goto WrongParameter;
				}
			}else if( F == 's' && value->type == DAO_STRING ){
				DaoStream_WriteString( self, value->xString.data );
			}else if( F == 'p' ){
				DaoStream_WritePointer( self, value );
			}else if( F == 'a' ){
				self->format = NULL;
				DaoValue_Print( value, proc, self, cycData );
			}else{
				goto WrongParameter;
			}
			self->format = NULL;
			if( fg || bg ) DaoStream_SetColor( self, NULL, NULL );
			continue;
WrongParameter:
			self->format = NULL;
			if( fg || bg ) DaoStream_SetColor( self, NULL, NULL );
			sprintf( message, "%i-th parameter has wrong type for format \"%s\"!", k, fmt2->mbs );
			DaoProcess_RaiseException( proc, DAO_WARNING, message );
		}else{
			DaoStream_WriteChar( self, *s );
		}
	}
	DString_Delete( fgcolor );
	DString_Delete( bgcolor );
	DString_Delete( format );
	DString_Delete( fmt2 );
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
	DaoStream *stream = proc->stdioStream;
	if( stream == NULL ) stream = proc->vmSpace->stdioStream;
	DaoIO_Writef0( stream, proc, p, N );
}
static void DaoIO_Flush( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	DaoStream_Flush( self );
}
static void DaoIO_Read( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = proc->stdioStream;
	DString *ds = DaoProcess_PutMBString( proc, "" );
	int count = 0;
	if( self == NULL ) self = proc->vmSpace->stdioStream;
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
	if( self->file == NULL && self->redirect && self->redirect->StdioRead ){
		self->redirect->StdioRead( self->redirect, ds, count );
	}else if( count ){
		FILE *fd = stdin;
		DString_Clear( ds );
		if( self->file ) fd = self->file;
		if( count >0 ){
			DString_Resize( ds, count );
			DString_Resize( ds, fread( ds->mbs, 1, count, fd ) );
		}else{
			struct stat info;
			fstat( fileno( fd ), &info );
			DString_Resize( ds, info.st_size - ftell( fd )/2 );
			DString_Resize( ds, fread( ds->mbs, 1, ds->size, fd ) );
		}
		if( fd == stdin ) fseek( stdin, 0, SEEK_END );
	}else{
		DaoStream_ReadLine( self, ds );
	}
}

extern void Dao_MakePath( DString *base, DString *path );

/*
// Special relative paths:
// 1. ::path, path relative to the current source code file;
// 2. :path, path relative to the current working directory;
*/
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
static FILE* DaoIO_OpenFile( DaoProcess *proc, DString *name, const char *mode, int silent )
{
	DString *fname = DString_Copy( name );
	char buf[IO_BUF_SIZE];
	FILE *fin;

	DString_ToMBS( fname );
	DaoIO_MakePath( proc, fname );
	fin = fopen( fname->mbs, mode );
	DString_Delete( fname );
	if( fin == NULL && silent == 0 ){
		snprintf( buf, IO_BUF_SIZE, "error opening file: %s", DString_GetMBS( name ) );
		DaoProcess_RaiseException( proc, DAO_ERROR, buf );
		return NULL;
	}
	return fin;
}
static void DaoIO_ReadFile( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *res = DaoProcess_PutMBString( proc, "" );
	daoint silent = p[1]->xInteger.value;
	if( DString_Size( p[0]->xString.data ) ==0 ){
		char buf[1024];
		while(1){
			size_t count = fread( buf, 1, sizeof( buf ), stdin );
			if( count ==0 ) break;
			DString_AppendDataMBS( res, buf, count );
		}
	}else{
		FILE *fin = DaoIO_OpenFile( proc, p[0]->xString.data, "r", silent );
		struct stat info;
		if( fin == NULL ) return;
		fstat( fileno( fin ), &info );
		DString_Resize( res, info.st_size );
		DString_Resize( res, fread( res->mbs, 1, res->size, fin ) );
		fclose( fin );
	}
}
static void DaoIO_Open( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stream = NULL;
	char *mode;
	stream = DaoStream_New();
	stream->attribs |= DAO_IO_FILE;
	if( N==0 ){
		stream->file = tmpfile();
		if( stream->file <= 0 ){
			DaoProcess_RaiseException( proc, DAO_ERROR, "failed to create temporary file" );
			return;
		}
	}else{
		/* XXX Error handling? */
		mode = DString_GetMBS( p[1]->xString.data );
		if( p[0]->type == DAO_INTEGER ){
			stream->file = fdopen( p[0]->xInteger.value, mode );
		}else{
			DString_Assign( stream->fname, p[0]->xString.data );
			DString_ToMBS( stream->fname );
			stream->file = DaoIO_OpenFile( proc, stream->fname, mode, 0 );
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
	daoint *num = DaoProcess_PutInteger( proc, 0 );
	*num = 1;
	if( self->file ) *num = feof( self->file );
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
	fseek( self->file, p[1]->xInteger.value, where );
}
static void DaoIO_Tell( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	daoint *num = DaoProcess_PutInteger( proc, 0 );
	if( self->file == NULL ) return;
	*num = ftell( self->file );
}
static void DaoIO_FileNO( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	daoint *num = DaoProcess_PutInteger( proc, 0 );
	if( self->file == NULL ) return;
	*num = fileno( self->file );
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
static void DaoIO_Iter( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	DaoValue **tuple = p[1]->xTuple.items;
	tuple[0]->xInteger.value = 1;
	if( self->file ){
		fseek( self->file, 0, SEEK_SET );
		tuple[0]->xInteger.value = ! feof( self->file );
	}
}
static void DaoIO_GetItem( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	DaoValue **tuple = p[1]->xTuple.items;
	DaoIO_Read( proc, p, 1 );
	tuple[0]->xInteger.value = 0;
	if( self->file ) tuple[0]->xInteger.value = ! feof( self->file );
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
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoList *list = DaoProcess_PutList( proc );
	int chop = p[1]->xInteger.value;
	char buf[IO_BUF_SIZE];
	FILE *fin;

	fin = DaoIO_OpenFile( proc, p[0]->xString.data, "r", 0 );
	if( fin == NULL ) return;
	if( sect == NULL || DaoProcess_PushSectionFrame( proc ) == NULL ){
		line = DaoString_New(1);
		while( DaoFile_ReadLine( fin, line->data ) ){
			if( chop ) DString_Chop( line->data );
			DaoList_Append( list, (DaoValue*) line );
		}
		DaoString_Delete( line );
	}else{
		ushort_t entry = proc->topFrame->entry;
		DaoString tmp = {DAO_STRING,0,0,0,1,NULL};
		tmp.data = p[0]->xString.data;
		line = (DaoString*) DaoProcess_SetValue( proc, sect->a, (DaoValue*)(void*) &tmp );
		DaoProcess_AcquireCV( proc );
		while( DaoFile_ReadLine( fin, line->data ) ){
			if( chop ) DString_Chop( line->data );
			proc->topFrame->entry = entry;
			DaoProcess_Execute( proc );
			if( proc->status == DAO_PROCESS_ABORTED ) break;
			res = proc->stackValues[0];
			if( res && res->type != DAO_NONE ) DaoList_Append( list, res );
		}
		DaoProcess_ReleaseCV( proc );
		DaoProcess_PopFrame( proc );
	}
	fclose( fin );
}
static void DaoIO_ReadLines2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoValue *res;
	DaoString *line;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoList *list = DaoProcess_PutList( proc );
	DaoStream *self = & p[0]->xStream;
	daoint i = 0, count = p[1]->xInteger.value;
	int chop = p[2]->xInteger.value;

	if( sect == NULL || DaoProcess_PushSectionFrame( proc ) == NULL ){
		line = DaoString_New(1);
		while( (i++) < count && DaoStream_ReadLine( self, line->data ) ){
			if( chop ) DString_Chop( line->data );
			DaoList_Append( list, (DaoValue*) line );
		}
		DaoString_Delete( line );
	}else{
		ushort_t entry = proc->topFrame->entry;
		DaoString tmp = {DAO_STRING,0,0,0,1,NULL};
		DString tmp2 = DString_WrapMBS( "" );
		tmp.data = & tmp2;
		line = (DaoString*) DaoProcess_SetValue( proc, sect->a, (DaoValue*)(void*) &tmp );
		DaoProcess_AcquireCV( proc );
		while( (i++) < count && DaoStream_ReadLine( self, line->data ) ){
			if( chop ) DString_Chop( line->data );
			proc->topFrame->entry = entry;
			DaoProcess_Execute( proc );
			if( proc->status == DAO_PROCESS_ABORTED ) break;
			res = proc->stackValues[0];
			if( res && res->type != DAO_NONE ) DaoList_Append( list, res );
		}
		DaoProcess_ReleaseCV( proc );
		DaoProcess_PopFrame( proc );
	}
}
static void DaoIO_WriteLines( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *string;
	DaoInteger idint = {DAO_INTEGER,0,0,0,0,0};
	DaoValue *res, *index = (DaoValue*)(void*)&idint;
	daoint i, entry, lines = p[1]->xInteger.value;
	FILE *fout = stdout;
	DaoVmCode *sect;

	if( p[0]->type == DAO_STRING ){
		fout = DaoIO_OpenFile( proc, p[0]->xString.data, "w+", 0 );
		if( fout == NULL ) return;
	}else{
		if( p[0]->xStream.file ) fout = p[0]->xStream.file;
	}
	sect = DaoProcess_InitCodeSection( proc );
	if( sect == NULL ) return;
	entry = proc->topFrame->entry;
	for(i=0; i<lines; i++){
		idint.value = i;
		if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, index );
		proc->topFrame->entry = entry;
		DaoProcess_Execute( proc );
		if( proc->status == DAO_PROCESS_ABORTED ) break;
		string = proc->stackValues[0]->xString.data;
		if( string->mbs ){
			fprintf( fout, "%s", string->mbs );
		}else{
			fprintf( fout, "%ls", string->wcs );
		}
	}
	DaoProcess_PopFrame( proc );
}

DaoFuncItem dao_io_methods[] =
{
	{ DaoIO_Write2,    "write( ... )" },
	{ DaoIO_Writef2,   "writef( format : string, ... )" },
	{ DaoIO_Writeln2,  "writeln( ... )" },
	{ DaoIO_Read,      "read( )=>string" },
	{ DaoIO_ReadFile,  "read( file : string, silent=0 )=>string" },
	{ DaoIO_Open,      "open( )=>stream" },
	{ DaoIO_Open,      "open( file :string, mode :string )=>stream" },
	{ DaoIO_Open,      "open( fileno :int, mode :string )=>stream" },
	{ DaoIO_SStream,   "sstream( type :enum<mbs,wcs> = $mbs )=>stream" },

	{ DaoIO_ReadLines,  "readlines( file :string, chop=0 )[line:string=>none|@T]=>list<@T>" },
	{ NULL, NULL }
};

static DaoFuncItem streamMeths[] =
{
	{ DaoIO_Open,      "stream( )=>stream" },
	{ DaoIO_SStream,   "stream( type :enum<mbs, wcs> )=>stream" },
	{ DaoIO_Open,      "stream( file :string, mode :string )=>stream" },
	{ DaoIO_Open,      "stream( fileno :int, mode :string )=>stream" },
	{ DaoIO_Write,     "write( self :stream, ... )" },
	{ DaoIO_Writef,    "writef( self :stream, format : string, ... )" },
	{ DaoIO_Writeln,   "writeln( self :stream, ... )" },
	{ DaoIO_Flush,     "flush( self :stream )" },
	{ DaoIO_Read,      "read( self :stream, count=0 )=>string" },
	{ DaoIO_Read2,     "read( self :stream, quantity :enum<line, all> )=>string" },
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

	{ DaoIO_ReadLines2, "readlines( self :stream, numline=0, chop=0 )[line:string=>none|@T]=>list<@T>" },
	// Not particularly useful, may be removed!
	{ DaoIO_WriteLines, "writelines( self :stream, lines :int)[line:int =>string]" },
	{ DaoIO_WriteLines, "writelines( file :string, lines :int)[line:int =>string]" },
	{ NULL, NULL }
};


void DaoStream_SetFile( DaoStream *self, FILE *fd )
{
	DaoValue *p = (DaoValue*) self;
	self->file = fd;
}
FILE* DaoStream_GetFile( DaoStream *self )
{
	if( self->file ) return self->file;
	return NULL;
}

DaoTypeBase streamTyper =
{
	"stream", NULL, NULL, (DaoFuncItem*) streamMeths, {0}, {0},
	(FuncPtrDel) DaoStream_Delete, NULL
};

DaoStream* DaoStream_New()
{
	DaoStream *self = (DaoStream*) dao_calloc( 1, sizeof(DaoStream) );
	DaoCstruct_Init( (DaoCstruct*) self, dao_type_stream );
	self->type = DAO_CSTRUCT; /* dao_type_stream may still be null in DaoVmSpace_New(); */
	self->streamString = DString_New(1);
	self->fname = DString_New(1);
	self->mode = DAO_IO_READ | DAO_IO_WRITE;
	return self;
}
void DaoStream_Close( DaoStream *self )
{
	if( self->file ){
		fflush( self->file );
		if( self->attribs & DAO_IO_PIPE )
			pclose( self->file );
		else
			fclose( self->file );
		self->file = NULL;
	}
	self->mode = DAO_IO_WRITE | DAO_IO_READ;
}
void DaoStream_Delete( DaoStream *self )
{
	DaoStream_Close( self );
	DString_Delete( self->fname );
	DString_Delete( self->streamString );
	DaoCstruct_Free( (DaoCstruct*) self );
	dao_free( self );
}
DaoUserStream* DaoStream_SetUserStream( DaoStream *self, DaoUserStream *us )
{
	DaoUserStream *stream = self->redirect;
	self->redirect = us;
	if( us ) us->stream = self;
	return stream;
}
void DaoStream_WriteChar( DaoStream *self, char val )
{
	const char *format = "%c";
	if( self->redirect && self->redirect->StdioWrite ){
		DString *mbs = DString_New(1);
		DString_AppendChar( mbs, val );
		self->redirect->StdioWrite( self->redirect, mbs );
		DString_Delete( mbs );
	}else if( self->file ){
		fprintf( self->file, format, val );
	}else if( self->attribs & DAO_IO_STRING ){
		DString_AppendChar( self->streamString, val );
	}else{
		printf( format, val );
	}
}
void DaoStream_WriteFormatedInt( DaoStream *self, daoint val, const char *format )
{
	char buffer[100];
	if( self->redirect && self->redirect->StdioWrite ){
		DString *mbs = DString_New(1);
		sprintf( buffer, format, val );
		DString_SetMBS( mbs, buffer );
		self->redirect->StdioWrite( self->redirect, mbs );
		DString_Delete( mbs );
	}else if( self->file ){
		fprintf( self->file, format, val );
	}else if( self->attribs & DAO_IO_STRING ){
		sprintf( buffer, format, val );
		DString_AppendMBS( self->streamString, buffer );
	}else{
		printf( format, val );
	}
}
void DaoStream_WriteInt( DaoStream *self, daoint val )
{
	const char *format = self->format;
	if( format == NULL ) format = "%" DAO_INT_FORMAT;
	DaoStream_WriteFormatedInt( self, val, format );
}
void DaoStream_WriteFloat( DaoStream *self, double val )
{
	const char *format = self->format;
	const char *iconvs = "diouxXcC";
	char buffer[100];
	if( format && strchr( iconvs, format[ strlen(format)-1 ] ) && val ==(long)val ){
		DaoStream_WriteInt( self, (daoint)val );
		return;
	}
	if( format == NULL ) format = "%f";
	if( self->redirect && self->redirect->StdioWrite ){
		DString *mbs = DString_New(1);
		sprintf( buffer, format, val );
		DString_SetMBS( mbs, buffer );
		self->redirect->StdioWrite( self->redirect, mbs );
		DString_Delete( mbs );
	}else if( self->file ){
		fprintf( self->file, format, val );
	}else if( self->attribs & DAO_IO_STRING ){
		sprintf( buffer, format, val );
		DString_AppendMBS( self->streamString, buffer );
	}else{
		printf( format, val );
	}
}
void DaoStream_WriteMBS( DaoStream *self, const char *val )
{
	const char *format = self->format;
	if( format == NULL ) format = "%s";
	if( self->redirect && self->redirect->StdioWrite ){
		DString *mbs = DString_New(1);
		DString_SetMBS( mbs, val );
		self->redirect->StdioWrite( self->redirect, mbs );
		DString_Delete( mbs );
	}else if( self->file ){
		fprintf( self->file, format, val );
	}else if( self->attribs & DAO_IO_STRING ){
		DString_AppendMBS( self->streamString, val );
	}else{
		printf( format, val );
	}
}
void DaoStream_WriteWCS( DaoStream *self, const wchar_t *val )
{
	const char *format = self->format;
	if( format == NULL ) format = "%ls";
	if( self->redirect && self->redirect->StdioWrite ){
		DString *mbs = DString_New(1);
		DString_SetWCS( mbs, val );
		self->redirect->StdioWrite( self->redirect, mbs );
		DString_Delete( mbs );
	}else if( self->file ){
		fprintf( self->file, format, val );
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
	int i;
	if( val->mbs ){
		const char *data = val->mbs;
		if( self->redirect && self->redirect->StdioWrite ){
			DString *mbs = DString_New(1);
			DString_SetDataMBS( mbs, data, val->size );
			self->redirect->StdioWrite( self->redirect, mbs );
			DString_Delete( mbs );
		}else if( self->file ){
			if( self->format ){
				fprintf( self->file, self->format, data );
			}else{
				DaoFile_WriteString( self->file, val );
			}
		}else if( self->attribs & DAO_IO_STRING ){
			DString_AppendDataMBS( self->streamString, data, val->size );
		}else{
			if( self->format ){
				printf( self->format, data );
			}else{
				DaoFile_WriteString( stdout, val );
			}
		}
	}else{
		const wchar_t *data = val->wcs;
		if( self->redirect && self->redirect->StdioWrite ){
			DString *mbs = DString_New(1);
			DString_SetWords( mbs, data, val->size );
			self->redirect->StdioWrite( self->redirect, mbs );
			DString_Delete( mbs );
		}else if( self->file ){
			if( self->format ){
				fprintf( self->file, self->format, data );
			}else{
				DaoFile_WriteString( self->file, val );
			}
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
				DaoFile_WriteString( stdout, val );
			}
		}
	}
}
void DaoStream_WritePointer( DaoStream *self, void *val )
{
	const char *format = self->format;
	char buffer[100];
	if( format == NULL ) format = "%p";
	if( self->redirect && self->redirect->StdioWrite ){
		DString *mbs = DString_New(1);
		sprintf( buffer, format, val );
		DString_SetMBS( mbs, buffer );
		self->redirect->StdioWrite( self->redirect, mbs );
		DString_Delete( mbs );
	}else if( self->file ){
		fprintf( self->file, format, val );
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
int DaoStream_ReadLine( DaoStream *self, DString *line )
{
	int ch, delim = '\n';
	char buf[IO_BUF_SIZE];
	char *start = buf, *end = buf + IO_BUF_SIZE;

	DString_Clear( line );
	DString_ToMBS( line );
	if( self->redirect && self->redirect->StdioRead ){
		self->redirect->StdioRead( self->redirect, line, 0 );
		return line->size >0;
	}else if( self->file ){
		return DaoFile_ReadLine( self->file, line );
	}else if( self->attribs & DAO_IO_STRING ){
		daoint pos = DString_FindWChar( self->streamString, delim, 0 );
		if( pos == MAXSIZE ){
			DString_Assign( line, self->streamString );
			DString_Clear( self->streamString );
		}else{
			DString_SubString( self->streamString, line, 0, pos+1 );
			DString_Erase( self->streamString, 0, pos+1 );
		}
		return self->streamString->size >0;
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
	char buf[IO_BUF_SIZE];
	DString_Reset( line, 0 );
	DString_ToMBS( line );
	if( feof( fin ) ) return 0;
	do{
		buf[IO_BUF_SIZE - 1] = 1;
		if( !fgets( buf, IO_BUF_SIZE, fin ) ) break;
		DString_AppendMBS( line, buf );
	} while( buf[IO_BUF_SIZE - 1] != 1 );
	return 1;
}
int DaoFile_ReadAll( FILE *fin, DString *all, int close )
{
	char buf[IO_BUF_SIZE];
	DString_Reset( all, 0 );
	DString_ToMBS( all );
	if( fin == NULL ) return 0;
	while(1){
		size_t count = fread( buf, 1, IO_BUF_SIZE, fin );
		if( count ==0 ) break;
		DString_AppendDataMBS( all, buf, count );
	}
	if( close ) fclose( fin );
	return 1;
}
void DaoFile_WriteString( FILE* file, DString *str )
{
	daoint pos = 0;
	if( str->mbs ){
		while( 1 ){
			fprintf( file, "%s", str->mbs + pos );
			pos = DString_FindChar( str, '\0', pos );
			if( pos == MAXSIZE ) break;
			fprintf( file, "%c", 0 );
			pos += 1;
		}
	}else{
		while( 1 ){
			fprintf( file, "%ls", str->wcs + pos );
			pos = DString_FindWChar( str, L'\0', pos );
			if( pos == MAXSIZE ) break;
			fprintf( file, "%lc", 0 );
			pos += 1;
		}
	}
}

