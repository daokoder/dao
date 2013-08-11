/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2013, Limin Fu
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

#include <string.h>
#include "daoPlatforms.h"
#include "daoStream.h"


void Dao_NormalizePathSep( DString *path )
{
#ifdef WIN32
	DString_ChangeMBS( path, "\\", "/", 0 );
#endif
}

double Dao_GetCurrentTime()
{
#ifdef WIN32
	return timeGetTime();
#else
	struct timeval tv;
	gettimeofday( & tv, NULL);
	return tv.tv_sec + (double)tv.tv_usec * 1.0E-6;
#endif
}


#ifdef DAO_WITHOUT_DLL

void Dao_GetErrorDLL(){}
void* Dao_OpenDLL( const char *name ){ return NULL; }
void* Dao_GetSymbolAddress( void *handle, const char *name ){ return NULL; }

#else

void Dao_GetErrorDLL()
{
#ifdef UNIX
	printf( "%s\n", dlerror() );
#elif WIN32
	DWORD error = GetLastError();
	LPSTR message;
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, error, LANG_NEUTRAL, (LPTSTR)&message, 0, NULL );
	if( message ){
		printf( "%s\n", message );
		LocalFree( message );
	}
#endif
}

void* Dao_OpenDLL( const char *name )
{
	void *handle = NULL;
#ifdef UNIX
	handle = dlopen( name, RTLD_NOW | RTLD_GLOBAL );
#elif WIN32
	handle = LoadLibrary( name );
#endif
	if( !handle ){
		Dao_GetErrorDLL();
		return 0;
	}
	return handle;
}
void* Dao_GetSymbolAddress( void *handle, const char *name )
{
	void *sym = NULL;
#ifdef UNIX
	sym = dlsym( handle, name );
#elif WIN32
	sym = (void*)GetProcAddress( (HMODULE)handle, name );
#endif
	return sym;
}

#endif



size_t Dao_FileChangedTime( const char *file )
{
	struct stat st;
	if( stat( file, &st ) ==0 ) return (size_t) st.st_mtime;
	return 0;
}
int Dao_IsFile( const char *file )
{
	struct stat st;
	if( stat( file, &st ) ) return 0;
#if WIN32
	return (st.st_mode & _S_IFDIR) == 0;
#else
	return S_ISDIR( st.st_mode ) == 0;
#endif
}
int Dao_IsDir( const char *file )
{
	struct stat st;
	if( stat( file, &st ) ) return 0;
#if WIN32
	return (st.st_mode & _S_IFDIR) != 0;
#else
	return S_ISDIR( st.st_mode ) != 0;
#endif
}




#ifndef DAO_WITHOUT_COLORPRINT

#ifdef WIN32

#include<windows.h>
#include<io.h>

const char* const dao_colors[8]
= { "black", "blue", "green", "cyan", "red", "magenta", "yellow", "white" };

static int SetCharColor( DaoStream *stream, int color, int RGB[3] )
{
	int res = 0;
	struct _CONSOLE_SCREEN_BUFFER_INFO info;
	FILE *file = DaoStream_GetFile( stream );
	HANDLE fd = INVALID_HANDLE_VALUE;
	WORD attr;
	if( file ) fd = (HANDLE)_get_osfhandle( _fileno( file ) );
	if( fd == INVALID_HANDLE_VALUE ) fd = GetStdHandle( STD_OUTPUT_HANDLE );
	if( !GetConsoleScreenBufferInfo( fd, &info ) ) return 255;
	attr = info.wAttributes;
	if( attr & RGB[2] ) res |= 1;
	if( attr & RGB[1] ) res |= 2;
	if( attr & RGB[0] ) res |= 4;
	attr = attr & ~RGB[2] & ~RGB[1] & ~RGB[0];
	if( color & 1 ) attr |= RGB[2];
	if( color & 2 ) attr |= RGB[1];
	if( color & 4 ) attr |= RGB[0];
	if( !SetConsoleTextAttribute( fd, attr ) ) return 255;
	return res;
}
static int SetCharForeground( DaoStream *stream, int color )
{
	int RGB[3] = { FOREGROUND_RED, FOREGROUND_GREEN, FOREGROUND_BLUE };
	return SetCharColor( stream, color, RGB );
}
static int SetCharBackground( DaoStream *stream, int color )
{
	int RGB[3] = { BACKGROUND_RED, BACKGROUND_GREEN, BACKGROUND_BLUE };
	return SetCharColor( stream, color, RGB );
}

#else

#define CSI_RESET "\033[0m"
#define CSI_FCOLOR "\033[3%im"
#define CSI_BCOLOR "\033[4%im"

const char* const dao_colors[8]
= { "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white" };

#ifdef UNIX
#include<unistd.h>
#endif
static int IsaTTY( DaoStream *stream )
{
#ifdef UNIX
	static int ist = 0;
	static int checked = 0;
	FILE *file = stdout;
	if( checked ) return ist;
	if( stream->file ) file = stream->file;
	ist = isatty( fileno( file ) );
	checked = 1;
	return ist;
#else
	return 0;
#endif
}

static int SetCharColor( DaoStream *stream, int color, const char *csi )
{
	char buf[20];
	if( IsaTTY( stream ) == 0 ) return 254;
	if( color == 254 )
		snprintf( buf, sizeof( buf ), CSI_RESET );
	else
		snprintf( buf, sizeof( buf ), csi, color );
	DaoStream_WriteMBS( stream, buf );
	return 254;
}
static int SetCharForeground( DaoStream *stream, int color )
{
	return SetCharColor( stream, color, CSI_FCOLOR );
}
static int SetCharBackground( DaoStream *stream, int color )
{
	return SetCharColor( stream, color, CSI_BCOLOR );
}
#endif

static int MapColor( const char *mbs )
{
	int i;
	for(i=0; i<8; i++) if( ! strcmp( dao_colors[i], mbs ) ) return i;
	return 254;
}

void DaoStream_SetColor( DaoStream *self, const char *fgcolor, const char *bgcolor )
{
	static int fg = -1;
	static int bg = -1;
	if( self->redirect == NULL && self->file == NULL ){
		/* reset first, because resetting one of foreground and background could reset both: */
		if( fg >= 0 && (fgcolor == NULL || fgcolor[0] == 0) ) SetCharForeground( self, fg );
		if( bg >= 0 && (bgcolor == NULL || bgcolor[0] == 0) ) SetCharBackground( self, bg );

		if( fgcolor && fgcolor[0] ) fg = SetCharForeground( self, MapColor( fgcolor ) );
		if( bgcolor && bgcolor[0] ) bg = SetCharBackground( self, MapColor( bgcolor ) );
		return;
	}
	if( self->redirect == NULL || self->redirect->SetColor == NULL ) return;
	self->redirect->SetColor( self->redirect, fgcolor, bgcolor );
}

#else

void DaoStream_SetColor( DaoStream *self, const char *fgcolor, const char *bgcolor ){}

#endif
