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

#include"ctype.h"
#include"string.h"
#include"daoStream.h"
#include"daoVmspace.h"
#include"daoRoutine.h"
#include"daoProcess.h"
#include"daoNumtype.h"
#include"daoNamespace.h"
#include"daoValue.h"
#include"daoGC.h"

#define IO_BUF_SIZE  4096


int DaoStream_ReadStdin( DaoStream *self, DString *data, int count )
{
	DString_Reset( data, 0 );
	if( count >= 0 ){
		DString_Reset( data, count );
		DString_Reset( data, fread( data->chars, 1, count, stdin ) );
	}else if( count == -1 ){
		DaoFile_ReadLine( stdin, data );
	}else{
		DaoFile_ReadAll( stdin, data, 0 );
	}
	fseek( stdin, 0, SEEK_END );
	return data->size;
}
int DaoStream_WriteStdout( DaoStream *self, const void *data, int count )
{
	DString bytes = DString_WrapBytes( (char*) data, count );
	DaoFile_WriteString( stdout, & bytes );
	return count;
}
int DaoStream_WriteStderr( DaoStream *self, const void *data, int count )
{
	DString bytes = DString_WrapBytes( (char*) data, count );
	DaoFile_WriteString( stderr, & bytes );
	return count;
}
int DaoStream_AtEnd( DaoStream *self )
{
	return 0;
}
void DaoStream_FlushStdout( DaoStream *self )
{
	if( self->Write == DaoStream_WriteStdout ){
		fflush( stdout );
	}else if( self->Write == DaoStream_WriteStderr ){
		fflush( stderr );
	}
}

int DaoStdStream_ReadStdin( DaoStream *stream, DString *data, int count )
{
	DaoStdStream *self = (DaoStdStream*) stream;
	if( self->redirect && self->redirect->Read ){
		return self->redirect->Read( self->redirect, data, count );
	}
	return DaoStream_ReadStdin( stream, data, count );
}
int DaoStdStream_WriteStdout( DaoStream *stream, const void *data, int count )
{
	DaoStdStream *self = (DaoStdStream*) stream;
	if( count == 0 ) return 0;
	if( self->redirect && self->redirect->Write ){
		int k = self->redirect->Write( self->redirect, data, count );
		if( k ) return k;
	}
	return DaoStream_WriteStdout( stream, data, count );
}
int DaoStdStream_WriteStderr( DaoStream *stream, const void *data, int count )
{
	DaoStdStream *self = (DaoStdStream*) stream;
	if( count == 0 ) return 0;
	if( self->redirect && self->redirect->Write ){
		int k = self->redirect->Write( self->redirect, data, count );
		if( k ) return k;
	}
	return DaoStream_WriteStderr( stream, data, count );
}
static int DaoStdStream_AtEnd( DaoStream *stream )
{
	DaoStdStream *self = (DaoStdStream*) stream;
	if( self->redirect && self->redirect->Flush ){
		return self->redirect->AtEnd( self->redirect );
	}
	return DaoStream_AtEnd( stream );
}
static void DaoStdStream_FlushStdout( DaoStream *stream )
{
	DaoStdStream *self = (DaoStdStream*) stream;
	if( self->redirect && self->redirect->Flush ){
		self->redirect->Flush( self->redirect );
		return;
	}
	DaoStream_FlushStdout( stream );
}
static int DaoStdStream_SetColor( DaoStream *stream, const char *fgcolor, const char *bgcolor )
{
	DaoStdStream *self = (DaoStdStream*) stream;
	if( self->redirect ){
		if( self->redirect->SetColor ){
			return self->redirect->SetColor( self->redirect, fgcolor, bgcolor );
		}
		return 1;
	}
	return DaoStream_SetScreenColor( stream, fgcolor, bgcolor );
}
static int DaoStream_WriteBuffer( DaoStream *self, const void *data, int count )
{
	DString_AppendBytes( self->buffer, (char*) data, count );
	return count;
}



DaoStream* DaoStream_New( DaoVmSpace *vms )
{
	DaoStream *self = (DaoStream*) dao_calloc( 1, sizeof(DaoStream) );
	DaoCstruct_Init( (DaoCstruct*) self, vms->typeStream );
	self->Read = DaoStream_ReadStdin;
	self->Write = DaoStream_WriteStdout;
	self->AtEnd = DaoStream_AtEnd;
	self->Flush = DaoStream_FlushStdout;
	self->SetColor = DaoStream_SetScreenColor;
	return self;
}
DaoStream* DaoStdStream_New( DaoVmSpace *vms )
{
	DaoStdStream *self = (DaoStdStream*) dao_calloc( 1, sizeof(DaoStdStream) );
	DaoCstruct_Init( (DaoCstruct*) self, vms->typeStream );
	self->base.type = DAO_CSTRUCT;
	self->base.Read = DaoStdStream_ReadStdin;
	self->base.Write = DaoStdStream_WriteStdout;
	self->base.AtEnd = DaoStdStream_AtEnd;
	self->base.Flush = DaoStdStream_FlushStdout;
	self->base.SetColor = DaoStdStream_SetColor;
	return (DaoStream*) self;
}
void DaoStream_Delete( DaoStream *self )
{
	if( self->buffer ) DString_Delete( self->buffer );
	DaoCstruct_Free( (DaoCstruct*) self );
	dao_free( self );
}
void DaoStream_SetStringMode( DaoStream *self )
{
	if( self->buffer == NULL ) self->buffer = DString_New();
	self->Write = DaoStream_WriteBuffer;
	self->Read = NULL;
	self->Flush = NULL;
	self->SetColor = NULL;
}
void DaoStream_Flush( DaoStream *self )
{
	if( self->Flush ) self->Flush( self );
}
int DaoStream_IsOpen( DaoStream *self )
{
	return self->Read != NULL || self->Write != NULL;
}
int DaoStream_EndOfStream( DaoStream *self )
{
	if( self->AtEnd == NULL ) return 0;
	return self->AtEnd( self );
}
int DaoStream_IsReadable( DaoStream *self )
{
	return self->Read != NULL;
}
int DaoStream_IsWritable( DaoStream *self )
{
	return self->Write != NULL;
}
void DaoStream_WriteChar( DaoStream *self, char val )
{
	const char *format = "%c";
	char buffer[4];
	int count;
	if( self->Write == NULL ) return;
	count = snprintf( buffer, sizeof(buffer), "%c", val );
	self->Write( self, buffer, count );
}
void DaoStream_WriteInt( DaoStream *self, dao_integer val )
{
	const char *format = self->format;
	char buffer[48];
	int count;

	if( self->Write == NULL ) return;
	if( format == NULL ) format = "%"DAO_I64;
	count = snprintf( buffer, sizeof(buffer), format, val );
	self->Write( self, buffer, count );
}
void DaoStream_WriteFloat( DaoStream *self, double val )
{
	const char *format = self->format;
	const char *iconvs = "diouxXcC";
	char buffer[100];
	int count;
	if( self->Write == NULL ) return;
	if( format && strchr( iconvs, format[ strlen(format)-1 ] ) && val ==(dao_integer)val ){
		DaoStream_WriteInt( self, (dao_integer)val );
		return;
	}
	if( format == NULL ) format = "%f";
	count = snprintf( buffer, sizeof(buffer), format, val );
	self->Write( self, buffer, count );
}
void DaoStream_WriteChars( DaoStream *self, const char *chars )
{
	if( self->Write == NULL ) return;
	self->Write( self, chars, strlen(chars) );
}
daoint DaoStream_WriteBytes( DaoStream *self, const void *bytes, daoint count )
{
	const uchar_t *chars = (const uchar_t*)bytes;
	daoint sum = 0;
	if( self->Write == NULL ) return -1;
	while( sum < count ){
		daoint num = count - sum;
		if( num > 0x7fffffff ) num = 0x7fffffff;
		num = self->Write( self, chars + sum, num );
		if( num < 0 ) return -1;
		if( num == 0 ) break;
		sum += num;
	}
	return sum;
}
void DaoStream_WriteString( DaoStream *self, DString *val )
{
	if( self->Write == NULL ) return;
	self->Write( self, val->chars, val->size );
}
void DaoStream_WriteLocalString( DaoStream *self, DString *str )
{
	str = DString_Copy( str );
	DString_ToLocal( str );
	DaoStream_WriteString( self, str );
	DString_Delete( str );
}
void DaoStream_WritePointer( DaoStream *self, void *val )
{
	const char *format = self->format;
	char buffer[32];
	int count;
	if( format == NULL ) format = "%p";
	count = snprintf( buffer, sizeof(buffer), format, val );
	self->Write( self, buffer, count );
}
void DaoStream_WriteNewLine( DaoStream *self )
{
	DaoStream_WriteChars( self, daoConfig.iscgi ? "<br/>" : "\n" );
}
daoint DaoStream_Read( DaoStream *self, DString *output, daoint count )
{
	DString_Reset( output, 0 );
	if( self->Read == NULL ) return 0;
	if( count == -1 ){
		return self->Read( self, output, -1 );
	}else if( count <= -2 ){
		return self->Read( self, output, -2 );
	}else if( count <= 0x7fffffff ){
		return self->Read( self, output, count );
	}
	DString_Reserve( output, count );
	count = DaoStream_ReadBytes( self, output->chars, count );
	if( count > 0 ) DString_Reset( output, count );
	return count;
}

daoint DaoStream_ReadBytes( DaoStream *self, void *output, daoint count )
{
	daoint sum = 0;

	if( count < 0 ) return -1;
	if( self->Read == NULL ) return -1;

	while( sum < count ){
		DString buffer = DString_WrapBytes( (char*) output, count );
		daoint num = count - sum;
		if( num > 0x7fffffff ) num = 0x7fffffff;
		num = self->Read( self, & buffer, num );
		if( num < 0 ) return -1;
		if( num == 0 ) break;
		sum += num;
	}
	return sum;
}

int DaoStream_ReadLine( DaoStream *self, DString *line )
{
	DString_Reset( line, 0 );
	if( self->Read == NULL ) return 0;
	if( self->AtEnd && self->AtEnd( self ) ) return 0;
	return self->Read( self, line, -1 ) >= 0;
}

int DaoStream_SetColor( DaoStream *self, const char *fgcolor, const char *bgcolor )
{
	if( self->SetColor ) return self->SetColor( self, fgcolor, bgcolor );
	return 0;
}

void DaoStream_TryHighlight( DaoStream *self, int tag )
{
	const char *color = NULL;

	if( ! (self->mode & DAO_STREAM_HIGHLIGHT) ) return;

	if( tag == 0 ){
		DaoStream_SetColor( self, NULL, NULL );
		return;
	}
	switch( tag ){
	case '"' : color = "red"; break;
	case '0' : color = "red"; break;
	case 'A' : color = "green"; break;
	case '.' : color = "green"; break;
	case '(' :
	case ')' : color = "blue"; break;
	case '[' :
	case ']' : color = "blue"; break;
	case '{' :
	case '}' : color = "blue"; break;
	case ',' : color = "magenta"; break;
	case ';' : color = "magenta"; break;
	case ':' : color = "cyan"; break;
	default: break;
	}
	DaoStream_SetColor( self, color, NULL );
}

void DaoStream_PrintHL( DaoStream *self, int tag, const char *text )
{
	DaoStream_TryHighlight( self, tag );
	DaoStream_WriteChars( self, text );
	DaoStream_TryHighlight( self, 0 );
}




int DaoFile_ReadLine( FILE *fin, DString *line )
{
	int ch;

	DString_Reset( line, 0 );
	if( feof( fin ) ) return 0;

	while( (ch = fgetc(fin)) != EOF ){
		if( line->size == line->bufSize ) DString_Reserve( line, (daoint)(5 + 1.2*line->size) );
		line->chars[ line->size ++ ] = ch;
		line->chars[ line->size ] = '\0';
		if( ch == '\n' ) break;
	}
	return 1;
}
int DaoFile_ReadAll( FILE *fin, DString *output, int close )
{
	char buf[IO_BUF_SIZE];
	DString_Reset( output, 0 );
	if( fin == NULL ) return 0;
	while(1){
		// TODO: read directly into output;
		size_t count = fread( buf, 1, IO_BUF_SIZE, fin );
		if( count ==0 ) break;
		DString_AppendBytes( output, buf, count );
	}
	if( close ) fclose( fin );
	return 1;
}
int DaoFile_ReadPart( FILE *fin, DString *output, daoint offset, daoint count )
{
	char buf[IO_BUF_SIZE];
	daoint k, m, size = output->size;

	if( fin == NULL ) return 0;
	fseek( fin, offset, SEEK_SET );
	while( count > 0 ){
		m = count < IO_BUF_SIZE ? count : IO_BUF_SIZE;
		k = fread( buf, 1, m, fin );
		if( k == 0 ) break;
		DString_AppendBytes( output, buf, k );
		count -= k;
	}
	return output->size - size;
}
int DaoFile_WriteString( FILE* file, DString *str )
{
	char buffer[1024];
	int nullterm = str->chars[ str->size ] == '\0';
	daoint pos = 0;
	while( pos < str->size ){
		daoint count = 0;
		if( nullterm ){
			count = fprintf( file, "%s", str->chars + pos );
		}else{
			count = str->size - pos;
			if( count > sizeof(buffer) ) count = sizeof(buffer);
			count = snprintf( buffer, count, "%s", str->chars + pos );
			if( count >= 0 ) count = fprintf( file, "%s", buffer );
		}
		if( count < 0 ) return 0;

		pos += count;
		if( pos >= str->size ) return 1;
		if( str->chars[pos] != '\0' ) return 0;

		fprintf( file, "%c", 0 );
		pos += 1;
	}
	return 1;
}





static int DaoIO_CheckMode( DaoStream *self, DaoProcess *proc, int what )
{
	if( DaoStream_IsOpen( self ) == 0 ){
		DaoProcess_RaiseError( proc, NULL, "stream is not open!" );
		return 0;
	}
	if( what == DAO_STREAM_READABLE && DaoStream_EndOfStream( self ) == 1 ){
		DaoProcess_RaiseError( proc, NULL, "stream reached the end!" );
		return 0;
	}
	if( what == DAO_STREAM_READABLE && DaoStream_IsReadable( self ) == 0 ){
		DaoProcess_RaiseError( proc, NULL, "stream is not readable!" );
		return 0;
	}
	if( what == DAO_STREAM_WRITABLE && DaoStream_IsWritable( self ) == 0 ){
		DaoProcess_RaiseError( proc, NULL, "stream is not writable!" );
		return 0;
	}
	return 1;
}
static void DaoIO_Write0( DaoStream *self, DaoProcess *proc, DaoValue *p[], int N )
{
	DMap *cycmap = NULL;
	int i;

	for(i=0; i<N; i++){
		if( p[i]->type > DAO_ARRAY ){
			cycmap = DHash_New(0,0);
			break;
		}
	}
	for(i=0; i<N; i++){
		if( p[i]->type > DAO_ARRAY ) DMap_Reset( cycmap );
		DaoValue_Print( p[i], self, cycmap, proc );
	}
	if( cycmap ) DMap_Delete( cycmap );
}
static void DaoIO_Write( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	if( DaoIO_CheckMode( self, proc, DAO_STREAM_WRITABLE ) == 0 ) return;
	DaoIO_Write0( self, proc, p+1, N-1 );
}
static void DaoIO_Write2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stream = proc->stdioStream;
	if( stream == NULL ) stream = proc->vmSpace->stdioStream;
	if( DaoIO_CheckMode( stream, proc, DAO_STREAM_WRITABLE ) == 0 ) return;
	DaoIO_Write0( stream, proc, p, N );
}
static void DaoIO_Writeln0( DaoStream *self, DaoProcess *proc, DaoValue *p[], int N )
{
	DaoValue *params[DAO_MAX_PARAM];
	DMap *cycmap = NULL;
	int i;
	if( DaoIO_CheckMode( self, proc, DAO_STREAM_WRITABLE ) == 0 ) return;
	for(i=0; i<N; i++){
		if( p[i]->type > DAO_ARRAY ){
			cycmap = DHash_New(0,0);
			break;
		}
	}
	/*
	// DaoValue_Print() may call user defined function and change the stack
	// and invalidate the parameter array:
	*/
	memmove( params, p, N*sizeof(DaoValue*) );
	for(i=0; i<N; i++){
		if( params[i]->type > DAO_ARRAY ) DMap_Reset( cycmap );
		DaoValue_Print( params[i], self, cycmap, proc );
		if( i+1<N ) DaoStream_WriteChars( self, " ");
	}
	DaoStream_WriteChars( self, "\n");
	if( cycmap ) DMap_Delete( cycmap );
}
static void DaoIO_Writeln( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	if( DaoIO_CheckMode( self, proc, DAO_STREAM_WRITABLE ) == 0 ) return;
	DaoIO_Writeln0( self, proc, p+1, N-1 );
}
static void DaoIO_Writeln2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stream = proc->stdioStream;
	if( stream == NULL ) stream = proc->vmSpace->stdioStream;
	if( DaoIO_CheckMode( stream, proc, DAO_STREAM_WRITABLE ) == 0 ) return;
	DaoIO_Writeln0( stream, proc, p, N );
}
/*
// C printf format: %[parameter][flags][width][.precision][length]type
//
// Dao writef format: %[flags][width][.precision]type[color]
//
// Where 'flags', 'width' and 'precision' will conform to the C format,
// but 'type' can only be:
//   d, i, o, u, x/X : for integer;
//   e/E, f/F, g/G : for float and double;
//   c/C : for character, C for local encoding;
//   s/S : for string, S for local encoding;
//   p : for any type, write address;
//   a : automatic, for any type, write in the default format;
// Namely the standard ones except 'n', and plus 'a'.
//
// Optional 'color' format will be in form of: [foreground:background], [foreground]
// or [:background]. The supported color name format will depend on the color printing
// handle. Mininum requirement is the support of the following 8 color names:
// black, white, red, green, blue, yellow, magenta, cyan.
*/
static void DaoIO_Writef0( DaoStream *self, DaoProcess *proc, DaoValue *p[], int N )
{
	DaoValue *value;
	DString *fmt2;
	DString *fgcolor = NULL;
	DString *bgcolor = NULL;
	DMap *cycmap = NULL;
	const char *convs = "asSpcCdiouxXfFeEgG";
	char F, *s, *end, *fg, *bg, *fmt, message[100];
	int i, k, id = 0;

	if( DaoIO_CheckMode( self, proc, DAO_STREAM_WRITABLE ) == 0 ) return;

	fmt2 = DString_New();
	for(i=0; i<N; i++){
		if( p[i]->type > DAO_ARRAY ){
			cycmap = DHash_New(0,0);
			break;
		}
	}

	s = p[0]->xString.value->chars;
	end = s + p[0]->xString.value->size;
	for(; s<end; s++){
		if( *s != '%' ){
			DaoStream_WriteChar( self, *s );
			continue;
		}

		fmt = s;
		s += 1;
		if( *s =='%' || *s == '[' ){
			DaoStream_WriteChar( self, *s );
			continue;
		}

		if( ++id >= N || p[id] == NULL ) goto NullParameter;
		value = p[id];

		/* flags: */
		while( *s == '+' || *s == '-' || *s == '#' || *s == '0' || *s == ' ' ) s += 1;
		while( isdigit( *s ) ) s += 1; /* width; */
		if( *s == '.' ){ /* precision: */
			s += 1;
			while( isdigit( *s ) ) s += 1;
		}
		DString_SetBytes( fmt2, fmt, s - fmt + 1 );
		if( strchr( convs, *s ) == NULL ){
			DaoProcess_RaiseWarning( proc, NULL, "invalid format conversion" );
			continue;
		}
		F = *s;
		s += 1;
		fg = bg = NULL;
		if( *s == '[' ){
			s += 1;
			fmt = s;
			while( isalnum( *s ) ) s += 1;
			if( fgcolor == NULL ) fgcolor = DString_New();
			DString_SetBytes( fgcolor, fmt, s - fmt );
			if( fgcolor->size ) fg = fgcolor->chars;
			if( *s == ':' ){
				s += 1;
				fmt = s;
				while( isalnum( *s ) ) s += 1;
				if( bgcolor == NULL ) bgcolor = DString_New();
				DString_SetBytes( bgcolor, fmt, s - fmt );
				if( bgcolor->size ) bg = bgcolor->chars;
			}
			if( *s != ']' ) goto WrongColor;
		}else{
			s -= 1;
		}
		if( fg || bg ){
			if( DaoStream_SetColor( self, fg, bg ) == 0 ) goto WrongColor;
		}
		self->format = fmt2->chars;
		if( F == 'c' || F == 'C' ){
			if( value->type != DAO_INTEGER ) goto WrongParameter;
			DString_Reset( fmt2, 0 );
			DString_AppendWChar( fmt2, (size_t)value->xInteger.value );
			self->format = "%s";
			if( F == 'C' ) DString_ToLocal( fmt2 );
			DaoStream_WriteString( self, fmt2 );
		}else if( F == 'd' || F == 'i' || F == 'o' || F == 'x' || F == 'X' ){
			if( value->type == DAO_NONE || value->type > DAO_FLOAT ) goto WrongParameter;
			DString_InsertChars( fmt2, "ll", fmt2->size-1, 0, 2 );
			self->format = fmt2->chars;
			DaoStream_WriteInt( self, DaoValue_GetInteger( value ) );
		}else if( toupper( F ) == 'E' || toupper( F ) == 'F' || toupper( F ) == 'G' ){
			if( value->type == DAO_NONE || value->type > DAO_FLOAT ) goto WrongParameter;
			DaoStream_WriteFloat( self, DaoValue_GetFloat( value ) );
		}else if( F == 's' && value->type == DAO_STRING ){
			DaoStream_WriteString( self, value->xString.value );
		}else if( F == 'S' && value->type == DAO_STRING ){
			DaoStream_WriteLocalString( self, value->xString.value );
		}else if( F == 'p' ){
			DaoStream_WritePointer( self, value );
		}else if( F == 'a' ){
			self->format = NULL;
			if( value->type > DAO_ARRAY ) DMap_Reset( cycmap );
			DaoValue_Print( value, self, cycmap, proc );
		}else{
			goto WrongParameter;
		}
		self->format = NULL;
		if( fg || bg ) DaoStream_SetColor( self, NULL, NULL );
		continue;
NullParameter:
		sprintf( message, "%i-th parameter is null!", id );
		DaoProcess_RaiseWarning( proc, NULL, message );
		continue;
WrongColor:
		sprintf( message, "%i-th parameter has wrong color format!", id );
		DaoProcess_RaiseWarning( proc, NULL, message );
		continue;
WrongParameter:
		self->format = NULL;
		if( fg || bg ) DaoStream_SetColor( self, NULL, NULL );
		sprintf( message, "%i-th parameter has wrong type for format \"%s\"!", id, fmt2->chars );
		DaoProcess_RaiseWarning( proc, NULL, message );
	}
	if( cycmap ) DMap_Delete( cycmap );
	if( fgcolor ) DString_Delete( fgcolor );
	if( bgcolor ) DString_Delete( bgcolor );
	DString_Delete( fmt2 );
}
static void DaoIO_Writef( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	if( DaoIO_CheckMode( self, proc, DAO_STREAM_WRITABLE ) == 0 ) return;
	DaoIO_Writef0( self, proc, p+1, N-1 );
}
static void DaoIO_Writef2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stream = proc->stdioStream;
	if( stream == NULL ) stream = proc->vmSpace->stdioStream;
	if( DaoIO_CheckMode( stream, proc, DAO_STREAM_WRITABLE ) == 0 ) return;
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
	DString *ds = DaoProcess_PutChars( proc, "" );
	int ch, size, amount = -1; /* amount=-2: all; amount=-1: line; amount>=0: bytes; */
	char buf[IO_BUF_SIZE];

	if( self == NULL ) self = proc->vmSpace->stdioStream;
	if( N > 0 ){
		self = (DaoStream*) p[0];
		amount = -2;
	}
	if( DaoIO_CheckMode( self, proc, DAO_STREAM_READABLE ) == 0 ) return;
	if( N > 1 ){
		if( p[1]->type == DAO_INTEGER ){
			amount = (int)p[1]->xInteger.value;
			if( amount < 0 ){
				DaoProcess_RaiseError( proc, NULL, "cannot read negative amount!" );
				return;
			}
		}else{
			amount = - 1 - p[1]->xEnum.value;
		}
	}
	DString_Reset( ds, 0 );
	self->Read( self, ds, amount );
	if( self->mode & DAO_STREAM_AUTOCONV ) DString_ToUTF8( ds );
}


static void DaoIO_Check( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	int res = 0, what = p[1]->xEnum.value;
	switch( what ){
	case 0 : res = DaoStream_IsReadable( self ); break;
	case 1 : res = DaoStream_IsWritable( self ); break;
	case 2 : res = DaoStream_IsOpen( self ); break;
	case 3 : res = DaoStream_EndOfStream( self ); break;
	}
	DaoProcess_PutBoolean( proc, res );
}
static void DaoIO_Check2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	int res = 0, what = p[1]->xEnum.value;
	switch( what ){
	case 0 : res = (self->mode & DAO_STREAM_AUTOCONV) != 0; break;
	}
	DaoProcess_PutBoolean( proc, res );
}
static void DaoIO_Enable( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *self = & p[0]->xStream;
	int what = p[1]->xEnum.value;
	if( p[2]->xBoolean.value ){
		self->mode |= DAO_STREAM_AUTOCONV;
	}else{
		self->mode &= ~DAO_STREAM_AUTOCONV;
	}
}
static void DaoStream_ReadLines( DaoStream *self, DaoList *list, DaoProcess *proc, int count, int chop )
{
	DaoValue *res;
	DaoString *line;
	DaoVmCode *sect = DaoProcess_InitCodeSection( proc, 1 );
	daoint i = 0;

	if( sect == NULL ){
		line = DaoString_New();
		while( (count == 0 || (i++) < count) && DaoStream_ReadLine( self, line->value ) ){
			if( line->value->size == 0 && self->AtEnd != NULL && self->AtEnd( self ) ) break;
			if( chop ) DString_Chop( line->value, 0 );
			DaoList_Append( list, (DaoValue*) line );
		}
		DaoString_Delete( line );
	}else{
		ushort_t entry = proc->topFrame->entry;
		if( sect->b ){
			DaoString tmp = {DAO_STRING,0,0,0,1,NULL};
			DString tmp2 = DString_WrapChars( "" );
			tmp.value = & tmp2;
			line = (DaoString*) DaoProcess_SetValue( proc, sect->a, (DaoValue*)(void*) &tmp );
		}
		while( (count == 0 || (i++) < count) && DaoStream_ReadLine( self, line->value ) ){
			if( line->value->size == 0 && self->AtEnd != NULL && self->AtEnd( self ) ) break;
			if( chop ) DString_Chop( line->value, 0 );
			proc->topFrame->entry = entry;
			DaoProcess_Execute( proc );
			if( proc->status == DAO_PROCESS_ABORTED ) break;
			res = proc->stackValues[0];
			if( res && res->type != DAO_NONE ) DaoList_Append( list, res );
		}
		DaoProcess_PopFrame( proc );
	}
}
static void DaoIO_ReadLines( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *list = DaoProcess_PutList( proc );
	int count = (int)p[1]->xInteger.value;
	int chop = (int)p[2]->xBoolean.value;
	if( DaoIO_CheckMode( (DaoStream*) p[0], proc, DAO_STREAM_READABLE ) == 0 ) return;
	DaoStream_ReadLines( (DaoStream*) p[0], list, proc, count, chop );
}


DaoFunctionEntry dao_io_methods[] =
{
	{ DaoIO_Write2,    "write( invar ... : any )" },
	{ DaoIO_Writef2,   "writef( format: string, invar ... : any )" },
	{ DaoIO_Writeln2,  "writeln( invar ... : any )" },
	{ DaoIO_Read,      "read( )=>string" },
	{ NULL, NULL }
};

static DaoFunctionEntry daoStreamMeths[] =
{
	{ DaoIO_Write,     "write( self: Stream, data: string )" },
	{ DaoIO_Write,     "write( self: Stream, invar ... : any )" },
	{ DaoIO_Writef,    "writef( self: Stream, format: string, invar ... : any )" },
	{ DaoIO_Writeln,   "writeln( self: Stream, invar ... : any )" },
	{ DaoIO_Read,      "read( self: Stream, count = -1 )=>string" },
	{ DaoIO_Read,      "read( self: Stream, amount: enum<line,all> = $all )=>string" },
	{ DaoIO_ReadLines, "readlines( self: Stream, numline=0, chop = false )[line: string=>none|@T]=>list<@T>" },

	{ DaoIO_Flush,     "flush( self: Stream )" },
	{ DaoIO_Enable,    "enable( self: Stream, what: enum<auto_conversion>, state: bool )" },
	{ DaoIO_Check,     "check( self: Stream, what: enum<readable,writable,open,eof> ) => bool" },
	{ DaoIO_Check2,    "check( self: Stream, what: enum<auto_conversion> ) => bool" },

	{ NULL, NULL }
};

static DaoFunctionEntry daoDeviceMeths[] =
{
	{ NULL, "read( self: Device, count = -1 ) => string" },
	{ NULL, "write( self: Device, data: string )" },
	{ NULL, "check( self: Device, what: enum<readable,writable,open,eof> ) => bool" },
	{ NULL, NULL }
};


DaoTypeCore daoDeviceCore =
{
	"Device",        /* name */
	0,               /* size */
	{ NULL },        /* bases */
	{ NULL },        /* casts */
	NULL,            /* numbers */
	daoDeviceMeths,  /* methods */
	NULL,  NULL,     /* GetField */
	NULL,  NULL,     /* SetField */
	NULL,  NULL,     /* GetItem */
	NULL,  NULL,     /* SetItem */
	NULL,  NULL,     /* Unary */
	NULL,  NULL,     /* Binary */
	NULL,  NULL,     /* Conversion */
	NULL,  NULL,     /* ForEach */
	NULL,            /* Print */
	NULL,            /* Slice */
	NULL,            /* Compare */
	NULL,            /* Hash */
	NULL,            /* Create */
	NULL,            /* Copy */
	NULL,            /* Delete */
	NULL             /* HandleGC */
};


DaoTypeCore daoStreamCore =
{
	"Stream",                                          /* name */
	sizeof(DaoStream),                                 /* size */
	{ NULL },                                          /* bases */
	{ NULL },                                          /* casts */
	NULL,                                              /* numbers */
	daoStreamMeths,                                    /* methods */
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
	(DaoDeleteFunction) DaoStream_Delete,              /* Delete */
	NULL                                               /* HandleGC */
};
