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

#include<string.h>
#include<stdio.h>

#include"dao.h"
#include"daoStream.h"
DAO_INIT_MODULE

#ifdef WIN32

#include<windows.h>
#include<io.h>

int SetCharForeground( DaoStream *stream, int color, int mbs )
{
	WORD attr;
	int res = 0;
	struct _CONSOLE_SCREEN_BUFFER_INFO info;
	HANDLE fd = (HANDLE)_get_osfhandle( _fileno( DaoStream_GetFile( stream ) ) );
	if( fd == INVALID_HANDLE_VALUE )
		fd = GetStdHandle( STD_OUTPUT_HANDLE );
	if( !GetConsoleScreenBufferInfo( fd, &info ) )
		return -1;
	attr = info.wAttributes;
	if( attr & FOREGROUND_BLUE )
		res |= 1;
	if( attr & FOREGROUND_GREEN )
		res |= 2;
	if( attr & FOREGROUND_RED )
		res |= 4;
	attr = attr & ~FOREGROUND_BLUE & ~FOREGROUND_GREEN & ~FOREGROUND_RED;
	if( color & 1 )
		attr |= FOREGROUND_BLUE;
	if( color & 2 )
		attr |= FOREGROUND_GREEN;
	if( color & 4 )
		attr |= FOREGROUND_RED;
	if( !SetConsoleTextAttribute( fd, attr ) )
		return -1;
	return res;
}

int SetCharBackground( DaoStream *stream, int color, int mbs )
{
	WORD attr;
	int res = 0;
	struct _CONSOLE_SCREEN_BUFFER_INFO info;
	HANDLE fd = (HANDLE)_get_osfhandle( _fileno( DaoStream_GetFile( stream ) ) );
	if( fd == INVALID_HANDLE_VALUE )
		fd = GetStdHandle( STD_OUTPUT_HANDLE );
	if( !GetConsoleScreenBufferInfo( fd, &info ) )
		return -1;
	attr = info.wAttributes;
	if( attr & BACKGROUND_BLUE )
		res |= 1;
	if( attr & BACKGROUND_GREEN )
		res |= 2;
	if( attr & BACKGROUND_RED )
		res |= 4;
	attr = attr & ~BACKGROUND_BLUE & ~BACKGROUND_GREEN & ~BACKGROUND_RED;
	if( color & 1 )
		attr |= BACKGROUND_BLUE;
	if( color & 2 )
		attr |= BACKGROUND_GREEN;
	if( color & 4 )
		attr |= BACKGROUND_RED;
	if( !SetConsoleTextAttribute( fd, attr ) )
		return -1;
	return res;
}

#else

#define CSI_RESET "\033[0m"
#define CSI_FCOLOR "\033[3%im"
#define CSI_BCOLOR "\033[4%im"

#define CSI_LRESET L"\033[0m"
#define CSI_LFCOLOR L"\033[3%im"
#define CSI_LBCOLOR L"\033[4%im"

int SetCharForeground( DaoStream *stream, int color, int mbs )
{
	char buf[20];
	wchar_t wbuf[20];
	if( mbs ){
		if( color == -2 )
			snprintf( buf, sizeof( buf ), CSI_RESET );
		else
			snprintf( buf, sizeof( buf ), CSI_FCOLOR, color );
		DaoStream_WriteMBS( stream, buf );
	}
	else{
		if( color == -2 )
			swprintf( wbuf, sizeof( wbuf ), CSI_LRESET );
		else
			swprintf( wbuf, sizeof( wbuf ), CSI_LFCOLOR, color );
		DaoStream_WriteWCS( stream, wbuf );
	}
	return -2;
}

int SetCharBackground( DaoStream *stream, int color, int mbs )
{
	char buf[20];
	wchar_t wbuf[20];
	if( mbs ){
		if( color == -2 )
			snprintf( buf, sizeof( buf ), CSI_RESET );
		else
			snprintf( buf, sizeof( buf ), CSI_BCOLOR, color );
		DaoStream_WriteMBS( stream, buf );
	}
	else{
		if( color == -2 )
			swprintf( wbuf, sizeof( wbuf ), CSI_LRESET );
		else
			swprintf( wbuf, sizeof( wbuf ), CSI_LBCOLOR, color );
		DaoStream_WriteWCS( stream, wbuf );
	}
	return -2;
}

#endif

int ParseColor( DaoProcess *proc, char *mbs, int n )
{
#ifdef WIN32
	const char* colors[8] = {"black", "blue", "green", "cyan", "red", "magenta", "yellow", "white"};
#else
	const char* colors[8] = {"black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"};
#endif
	int i;
	if( n )
		for( i = 0; i < 8; i++ )
			if( strlen( colors[i] ) == n && !strncmp( colors[i], mbs, n ) )
				return i;
	return -1;
}

int ParseColorW( DaoProcess *proc, wchar_t *wcs, int n )
{
#ifdef WIN32
	const wchar_t* colors[8] = {L"black", L"blue", L"green", L"cyan", L"red", L"magenta", L"yellow", L"white"};
#else
	const wchar_t* colors[8] = {L"black", L"red", L"green", L"yellow", L"blue", L"magenta", L"cyan", L"white"};
#endif
	int i;
	if( n )
		for( i = 0; i < 8; i++ )
			if( wcslen( colors[i] ) == n && !wcsncmp( colors[i], wcs, n ) )
				return i;
	return -1;
}

static void DaoColorPrint( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stream = DaoValue_CastStream( p[0] );
	DString *fmt = DaoString_Get( DaoValue_CastString( p[1] ) );
	DString *str = DString_New( DString_IsMBS( fmt )? 1 : 0 );
	int pos, prevpos, colorf, colorb, pos2;
	char *mbs;
	wchar_t *wcs;
	if( DString_IsMBS( fmt ) ){
		/*MBS*/
		mbs = DString_GetMBS( fmt );
		for( pos = 0; pos < DString_Size( fmt ); pos += 2 ){
			colorf = colorb = 0;
			prevpos = pos;
			pos = DString_FindChar( fmt, '#', pos );
			if( pos == -1 ){
				DString_SetDataMBS( str, mbs + prevpos, DString_Size( fmt ) - prevpos );
				DaoStream_WriteString( stream, str );
				break;
			}
			else{
				DString_SetDataMBS( str, mbs + prevpos, pos - prevpos );
				DaoStream_WriteString( stream, str );
				if( mbs[pos + 1] == '#' ){
					DaoStream_WriteMBS( stream, "#" );
					continue;
				}
				prevpos = DString_FindChar( fmt, '(', pos + 1 );
				if( prevpos == -1 ){
					DaoProcess_RaiseException( proc, DAO_WARNING, "Colored block: opening bracket not found!" );
					break;
				}
				pos2 = DString_FindChar( fmt, ':', pos + 1 );
				if( pos2 >= 0 && pos2 < prevpos ){
					if( pos2 == pos + 1 )
						colorf = -1;
					if( pos2 == prevpos - 1 )
						colorb = -1;
				}
				else{
					pos2 = prevpos;
					colorb = -1;
				}
				if( colorf != -1 )
					colorf = ParseColor( proc, mbs + pos + 1, pos2 - pos - 1 );
				if( colorb != -1 )
					colorb = ParseColor( proc, mbs + pos2 + 1, prevpos - pos2 - 1 );
				prevpos++;
				pos = DString_FindMBS( fmt, ")#", prevpos );
				if( pos == -1 ){
					DaoProcess_RaiseException( proc, DAO_WARNING, "Colored block: bracket not closed!" );
					break;
				}
				else if( pos == prevpos )
					continue;
				if( colorf != -1 )
					colorf = SetCharForeground( stream, colorf, 1 );
				if( colorb != -1 )
					colorb = SetCharBackground( stream, colorb, 1 );
				DString_SetDataMBS( str, mbs + prevpos, pos - prevpos );
				DaoStream_WriteString( stream, str );
				if( colorb != -1 )
					colorb = SetCharBackground( stream, colorb, 1 );
				if( colorf != -1 )
					colorf = SetCharForeground( stream, colorf, 1 );
			}
		}
	}
	else{
		/*WCS*/
		wcs = DString_GetWCS( fmt );
		for( pos = 0; pos < DString_Size( fmt ); pos += 2 ){
			colorf = colorb = 0;
			prevpos = pos;
			pos = DString_FindWChar( fmt, L'#', pos );
			if( pos == -1 ){
				DString_SetDataWCS( str, wcs + prevpos, DString_Size( fmt ) - prevpos );
				DaoStream_WriteString( stream, str );
				break;
			}
			else{
				DString_SetDataWCS( str, wcs + prevpos, pos - prevpos );
				DaoStream_WriteString( stream, str );
				if( wcs[pos + 1] == L'#' ){
					DaoStream_WriteWCS( stream, L"#" );
					continue;
				}
				prevpos = DString_FindWChar( fmt, L'(', pos + 1 );
				if( prevpos == -1 ){
					DaoProcess_RaiseException( proc, DAO_WARNING, "Colored block: opening bracket not found!" );
					break;
				}
				pos2 = DString_FindWChar( fmt, L':', pos + 1 );
				if( pos2 >= 0 && pos2 < prevpos ){
					if( pos2 == pos + 1 )
						colorf = -1;
					if( pos2 == prevpos - 1 )
						colorb = -1;
				}
				else{
					pos2 = prevpos;
					colorb = -1;
				}
				if( colorf != -1 )
					colorf = ParseColorW( proc, wcs + pos + 1, pos2 - pos - 1 );
				if( colorb != -1 )
					colorb = ParseColorW( proc, wcs + pos2 + 1, prevpos - pos2 - 1 );
				prevpos++;
				pos = DString_FindMBS( fmt, ")#", prevpos );
				if( pos == -1 ){
					DaoProcess_RaiseException( proc, DAO_WARNING, "Colored block: bracket not closed!" );
					break;
				}
				else if( pos == prevpos )
					continue;
				if( colorf != -1 )
					colorf = SetCharForeground( stream, colorf, 0 );
				if( colorb != -1 )
					colorb = SetCharBackground( stream, colorb, 0 );
				DString_SetDataWCS( str, wcs + prevpos, pos - prevpos );
				DaoStream_WriteString( stream, str );
				if( colorb != -1 )
					colorb = SetCharBackground( stream, colorb, 0 );
				if( colorf != -1 )
					colorf = SetCharForeground( stream, colorf, 0 );
			}
		}
	}
	DString_Delete( str );
}

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace_WrapFunction( ns, (DaoCFunction)DaoColorPrint, "clprint( self: stream, format: string )" );
	return 0;
}
