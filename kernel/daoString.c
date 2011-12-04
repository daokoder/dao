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
#include<stdlib.h>
#include<string.h>
#include<ctype.h>

#include"daoString.h"
#include"daoThread.h"

#ifdef DAO_WITH_THREAD
DMutex  mutex_string_sharing;
#endif

/* TODO: better string searching algorithm */
static size_t DMBString_Find( DString *self, size_t S, const char *chs, size_t M )
{
	size_t i, j;

	if( M == 0 ) return MAXSIZE;
	if( M+S > self->size ) return MAXSIZE;
	for( i=S; i<self->size-M+1; i++){
		int found = 1;
		for( j=0; j<M; j++ ){
			if( self->mbs[i+j] != chs[j] ){
				found = 0;
				break;
			}
		}
		if( found ) return i;
	}
	return MAXSIZE;
}
static size_t DMBString_RFind( DString *self, size_t S, const char* chs, size_t M )
{
	size_t i, j;

	if( M == 0 || self->size == 0 ) return MAXSIZE;
	if( S >= self->size ) S = self->size-1;
	if( M > S || M > self->size ) return MAXSIZE;
	for( i=S; i>=M-1; i--){
		int found = 1;
		for( j=0; j<M; j++ ){
			if( self->mbs[i-j] != chs[M-1-j] ){
				found = 0;
				break;
			}
		}
		if( found ) return i;
	}
	return MAXSIZE;
}
static size_t DWCString_Find( DString *self, size_t S, const wchar_t *chs, size_t M )
{
	size_t i, j;

	if( M == 0 ) return MAXSIZE;
	if( M+S > self->size ) return MAXSIZE;
	for( i=S; i<self->size-M+1; i++){
		int found = 1;
		for( j=0; j<M; j++ ){
			if( self->wcs[i+j] != chs[j] ){
				found = 0;
				break;
			}
		}
		if( found ) return i;
	}
	return MAXSIZE;
}
static size_t DWCString_RFind( DString *self, size_t S, const wchar_t* chs, size_t M )
{
	size_t i, j;

	if( M == 0 ) return MAXSIZE;
	if( self->size==0 ) return MAXSIZE;
	if( S >= self->size ) S = self->size-1;
	if( M > S || M > self->size ) return MAXSIZE;
	for( i=S; i>=M-1; i--){
		int found = 1;
		for( j=0; j<M; j++ ){
			if( self->wcs[i-j] != chs[M-1-j] ){
				found = 0;
				break;
			}
		}
		if( found ) return i;
	}
	return MAXSIZE;
}

/**/
void DString_Init( DString *self, int mbs )
{
	int bsize = mbs ? sizeof(char) : sizeof(wchar_t);
	int *data = (int*) dao_malloc( sizeof(int) + bsize );
	data[0] = 1;
	self->shared = 1;
	self->size = 0;
	self->bufSize = 0;
	if( mbs ){
		self->mbs = (char*)(data + 1);
		self->mbs[0] = 0;
	}else{
		self->wcs = (wchar_t*)(data + 1);
		self->wcs[0] = 0;
	}
}
DString* DString_New( int mbs )
{
	DString *self = (DString*)dao_calloc( 1, sizeof(DString) );
	DString_Init( self, mbs );
	return self;
}

void DString_DeleteData( DString *self )
{
	int *data = (self->mbs ? (int*)self->mbs : (int*)self->wcs) - self->shared;
	if( data == NULL ) return;
	if( self->shared ){
#ifdef DAO_WITH_THREAD
		DMutex_Lock( & mutex_string_sharing );
#endif
		if( self->shared ){
			data[0] -= 1;
			if( data[0] ==0 ) dao_free( data );
		}
#ifdef DAO_WITH_THREAD
		DMutex_Unlock( & mutex_string_sharing );
#endif
	}else{
		dao_free( data );
	}
	self->shared = 0;
	self->mbs = NULL;
	self->wcs = NULL;
}
void DString_Delete( DString *self )
{
	DString_DeleteData( self );
	dao_free( self );
}
void DString_Detach( DString *self )
{
	size_t size, chsize;
	int *data2, *data = (self->mbs ? (int*)self->mbs : (int*)self->wcs) - self->shared;
	if( self->shared ==0 ) return;
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & mutex_string_sharing );
#endif
	if( data[0] >1 ){
		data[0] -= 1;
		chsize = self->mbs ? sizeof(char) : sizeof(wchar_t);
		size = (self->size + 1) * chsize;
		self->bufSize = self->size;
		data2 = (int*) dao_malloc( size + sizeof(int) );
		data2[0] = 1;
		memcpy( data2+1, data+1, size );
		if( self->mbs ) self->mbs = (char*)(data2 + 1);
		if( self->wcs ) self->wcs = (wchar_t*)(data2 + 1);
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & mutex_string_sharing );
#endif
}
void DString_SetSharing( DString *self, int sharing )
{
	int *data;
	if( (self->shared == 0) == (sharing == 0) ) return;
	DString_Detach( self );
	data = (self->mbs ? (int*)self->mbs : (int*)self->wcs) - self->shared;
	self->shared = sharing != 0;

#ifdef DAO_WITH_THREAD
	DMutex_Lock( & mutex_string_sharing );
#endif
	if( sharing ==0 ){
		if( self->mbs ){
			memmove( data, self->mbs, self->size*sizeof(char) );
			self->bufSize += sizeof(int)/sizeof(char);
			self->mbs = (char*) data;
			self->mbs[ self->size ] = 0;
		}else{
			memmove( data, self->wcs, self->size*sizeof(wchar_t) );
			self->bufSize += sizeof(int)/sizeof(wchar_t);
			self->wcs = (wchar_t*) data;
			self->wcs[ self->size ] = 0;
		}
	}else if( self->mbs ){
		if( self->bufSize < self->size + sizeof(int)/sizeof(char) ){
			size_t size = (self->size + 1)*sizeof(char) + sizeof(int);
			data = (int*) dao_realloc( data, size );
			self->bufSize = self->size;
		}
		self->mbs = (char*)(data + 1);
		memmove( self->mbs, data, self->size*sizeof(char) );
		self->mbs[ self->size ] = 0;
		data[0] = 1;
	}else{ /* self->wcs */
		if( self->bufSize < self->size + sizeof(int)/sizeof(wchar_t) ){
			size_t size = (self->size + 1)*sizeof(wchar_t) + sizeof(int);
			data = (int*) dao_realloc( data, size );
			self->bufSize = self->size;
		}
		self->wcs = (wchar_t*)(data + 1);
		memmove( self->wcs, data, self->size*sizeof(wchar_t) );
		self->wcs[ self->size ] = 0;
		data[0] = 1;
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & mutex_string_sharing );
#endif
}

int DString_IsMBS( DString *self )
{
	return ( self->wcs == NULL );
}
char*  DString_GetMBS( DString *self )
{
	DString_ToMBS( self );
	return self->mbs;
}
wchar_t* DString_GetWCS( DString *self )
{
	DString_ToWCS( self );
	return self->wcs;
}
static void DMBString_AppendWCS( DString *self, const wchar_t *chs, size_t n );
static void DWCString_AppendMBS( DString *self, const char *chs, size_t n );
void DString_SetMBS( DString *self, const char *chs )
{
	size_t n;
	if( self->mbs && self->mbs == chs ) return;
	if( chs == NULL ){
		DString_Clear( self );
		return;
	}
	n = strlen( chs );
	if( self->mbs ){
		DString_Resize( self, n );
		memcpy( self->mbs, chs, n*sizeof(char) );
	}else{
		DString_Clear(self);
		DWCString_AppendMBS(self,chs,n);
	}
}
void DString_SetWCS( DString *self, const wchar_t *chs )
{
	size_t n;
	if( self->wcs && self->wcs == chs ) return;
	if( chs == NULL ){
		DString_Clear( self );
		return;
	}
	n = wcslen( chs );
	if( self->wcs ){
		DString_Resize( self, n );
		memcpy( self->wcs, chs, n*sizeof(wchar_t) );
	}else{
		DString_Clear(self);
		DMBString_AppendWCS(self,chs,n);
	}
}
void DString_Reserve( DString *self, size_t size )
{
	int *data;
	size_t bsize;
	DString_Detach( self );
	if( size <= self->bufSize && 2*size >= self->bufSize ) return;
	data = (self->mbs ? (int*)self->mbs : (int*)self->wcs) - self->shared;
	if( self->mbs ){
		if( size > self->bufSize || 2*size < self->bufSize ){
			self->bufSize = size * (size >= self->bufSize ? 1.2 : 1) + 1;
			bsize = (self->bufSize + 1)*sizeof(char) + self->shared*sizeof(int);
			data = (int*)dao_realloc( data, bsize );
			self->mbs = (char*)(data + self->shared);
		}
	}else{
		if( size > self->bufSize || 2*size < self->bufSize ){
			self->bufSize = size * (size >= self->bufSize ? 1.2 : 1) + 1;
			bsize = (self->bufSize + 1)*sizeof(wchar_t) + self->shared*sizeof(int);
			data = (int*)dao_realloc( data, bsize );
			self->wcs = (wchar_t*)(data + self->shared);
		}
	}
}
static void DMBString_Append( DString *self, const char *chs, size_t n )
{
	size_t i;
	DString_Reserve( self, self->size + n );
	for( i=0; i<n; i++ ) self->mbs[self->size+i] = chs[i];
	self->size += n;
	self->mbs[ self->size ] = 0;
}
static void DMBString_AppendChar( DString *self, const char ch )
{
	DString_Reserve( self, self->size + 1 );
	self->mbs[self->size] = ch;
	self->size += 1;
	self->mbs[ self->size ] = 0;
}
static void DMBString_AppendWChar( DString *self, const wchar_t ch )
{
	wchar_t chs[2] = { 0, 0 };
	chs[0] = ch;
	DMBString_AppendWCS( self, chs, 1 );
}
static void DWCString_AppendChar( DString *self, const char ch )
{
	char chs[2] = { 0, 0 };
	chs[0] = ch;
	DWCString_AppendMBS( self, chs, 1 );
}
static void DWCString_AppendWChar( DString *self, const wchar_t ch )
{
	DString_Reserve( self, self->size + 1 );
	self->wcs[self->size] = ch;
	self->size += 1;
	self->wcs[ self->size ] = 0;
}
#define MAX_CHAR_PER_WCHAR 7
static void DMBString_AppendWCS( DString *self, const wchar_t *chs, size_t n )
{
	wchar_t buffer[101];
	mbstate_t state;
	buffer[100] = 0;
	DString_Detach( self );
	while( n ){
		const wchar_t *wcs = buffer;
		size_t smin, len, m = n;
		len = n < 100 ? n : 100;
		wcsncpy( buffer, chs, len );
		buffer[len] = 0;
		len = wcslen( buffer );
		chs += len;
		n -= len;
		if( len == 0 ){
			DMBString_AppendChar( self, '\0' );
			chs ++;
			n --;
			continue;
		}
		if( self->bufSize < (self->size + len * MAX_CHAR_PER_WCHAR) ){
			/* reserve potentially enough memory to avoid frequent memory allocation: */
			if( (len * MAX_CHAR_PER_WCHAR) > m ) m = len * MAX_CHAR_PER_WCHAR;
			DString_Reserve( self, self->size + m );
		}
		/* under windows using MinGW, passing null output buffer,
		 * will NOT cause the function to perform conversion and 
		 * return the required buffer size. */
		memset( & state, 0, sizeof(state) );
		smin = wcsrtombs( self->mbs + self->size, & wcs, len * MAX_CHAR_PER_WCHAR, & state );
		if( smin == (size_t)-1 ) break;
		self->size += smin;
	}
	DString_Reserve( self, self->size );
	self->mbs[ self->size ] = 0;
}
static void DWCString_Append( DString *self, const wchar_t *chs, size_t n )
{
	size_t i;
	DString_Reserve( self, self->size + n );
	for( i=0; i<n; i++ ) self->wcs[self->size+i] = chs[i];
	self->size += n;
	self->wcs[ self->size ] = 0;
}
static void DWCString_AppendMBS( DString *self, const char *chs, size_t n )
{
	char buffer[101];
	mbstate_t state;
	buffer[100] = 0;
	DString_Detach( self );
	while( n ){
		const char *mbs = buffer;
		size_t smin, len = n < 100 ? n : 100;
		strncpy( buffer, chs, len );
		buffer[len] = 0;
		len = strlen( buffer );
		chs += len;
		n -= len;
		if( len == 0 ){
			DWCString_AppendWChar( self, L'\0' );
			chs ++;
			n --;
			continue;
		}
		if( self->bufSize < (self->size + len) ){
			/* reserve potentially enough memory to avoid frequent memory allocation: */
			DString_Reserve( self, self->size + len + n );
		}
		/* under windows using MinGW, passing null output buffer,
		 * will NOT cause the function to perform conversion and 
		 * return the required buffer size. */
		memset( & state, 0, sizeof (state) );
		smin = mbsrtowcs( self->wcs + self->size, (const char**)&mbs, len, & state );
		if( smin == (size_t)-1 ){ /* a valid encoding may have been cut in the middle: */
			size_t put = len - (mbs - buffer);
			chs -= put;
			n += put;
			mbs = buffer;
			buffer[len - put] = 0;
			memset( & state, 0, sizeof (state) );
			smin = mbsrtowcs( self->wcs + self->size, (const char**)&mbs, len, & state );
		}
		self->size += smin;
	}
	self->wcs[ self->size ] = 0;
}
void DString_ToWCS( DString *self )
{
	DString tmp = *self;
	int share = self->shared;

	if( self->wcs ) return;

	self->mbs = NULL;
	self->size = self->bufSize = 0;
	DString_Init( self, 0 );
	DString_SetSharing( self, share );
	DWCString_AppendMBS( self, tmp.mbs, tmp.size );
	DString_DeleteData( & tmp );
}
void DString_ToMBS( DString *self )
{
	DString tmp = *self;
	int share = self->shared;

	if( self->mbs ) return;

	self->wcs = NULL;
	self->size = self->bufSize = 0;
	DString_Init( self, 1 );
	DString_SetSharing( self, share );
	DMBString_AppendWCS( self, tmp.wcs, tmp.size );
	DString_DeleteData( & tmp );
}
void DString_ToLower( DString *self )
{
	size_t i;
	DString_Detach( self );
	if( self->mbs ){
		char *mbs = self->mbs;
		for( i=0; i<self->size; i++, mbs++ ) *mbs = tolower( *mbs );
	}else{
		wchar_t *wcs = self->wcs;
		for( i=0; i<self->size; i++, wcs++ ) *wcs = towlower( *wcs );
	}
}
void DString_ToUpper( DString *self )
{
	size_t i;
	DString_Detach( self );
	if( self->mbs ){
		char *mbs = self->mbs;
		for( i=0; i<self->size; i++, mbs++ ) *mbs = toupper( *mbs );
	}else{
		wchar_t *wcs = self->wcs;
		for( i=0; i<self->size; i++, wcs++ ) *wcs = towupper( *wcs );
	}
}
size_t DString_Size( DString *self )
{
	return self->size;
}

void DString_Resize( DString *self, size_t size )
{
	int *data;
	size_t i, bsize = self->bufSize;
	DString_Detach( self );
	if( size == self->size && size <= bsize && 2*size >= bsize ) return;
	data = (self->mbs ? (int*)self->mbs : (int*)self->wcs) - self->shared;
	if( self->mbs ){
		if( size > self->bufSize || 2*size < self->bufSize ){
			self->bufSize = size;
			bsize = (self->bufSize + 1)*sizeof(char) + self->shared*sizeof(int);
			data = (int*)dao_realloc( data, bsize );
			self->mbs = (char*)(data + self->shared);
		}
		for(i=self->size; i<size; i++) self->mbs[i] = 0;
		self->mbs[ size ] = 0;
	}else{
		if( size > self->bufSize || 2*size < self->bufSize ){
			self->bufSize = size;
			bsize = (self->bufSize + 1)*sizeof(wchar_t) + self->shared*sizeof(int);
			data = (int*)dao_realloc( data, bsize );
			self->wcs = (wchar_t*)(data + self->shared);
		}
		for(i=self->size; i<size; i++) self->wcs[i] = 0;
		self->wcs[ size ] = 0;
	}
	self->size = size;
}
void DString_Clear( DString *self )
{
	int share = self->shared;
	int mbs = self->mbs != NULL;
	DString_Detach( self );
	DString_DeleteData( self );
	DString_Init( self, mbs );
	DString_SetSharing( self, share );
}
void DString_Erase( DString *self, size_t start, size_t n )
{
	int *data;
	size_t i, rest, bsize;
	if( start >= self->size ) return;
	if( n == (size_t)-1 ) n = self->size;
	if( n + start > self->size ) n = self->size - start;
	rest = self->size - start - n;
	if( rest ==0 ){
		DString_Resize( self, start );
		return;
	}

	DString_Detach( self );
	data = (self->mbs ? (int*)self->mbs : (int*)self->wcs) - self->shared;
	if( self->mbs ){
		for( i=start; i<start+rest; i++ ) self->mbs[i] = self->mbs[i+n];
		self->mbs[start+rest] = 0;
		self->size -= n;
		if( self->size < 0.5*self->bufSize && self->size+5 < self->bufSize ){
			self->bufSize = (size_t)(0.6 * self->bufSize) + 1;
			bsize = (self->bufSize+1)*sizeof(char) + self->shared*sizeof(int);
			data = (int*)dao_realloc( data, bsize );
			self->mbs = (char*)(data + self->shared);
			if( self->shared ==0 ) self->bufSize += sizeof(int)/sizeof(char);
		}
	}else{
		for( i=start; i<start+rest; i++ ) self->wcs[i] = self->wcs[i+n];
		self->wcs[start+rest] = 0;
		self->size -= n;
		if( self->size < 0.5*self->bufSize && self->size + 5 < self->bufSize ){
			self->bufSize = (size_t)(0.6 * self->bufSize) + 1;
			bsize = (self->bufSize+1)*sizeof(wchar_t) + self->shared*sizeof(int);
			data = (int*)dao_realloc( data, bsize );
			self->wcs = (wchar_t*)(data + self->shared);
			if( self->shared ==0 ) self->bufSize += sizeof(int)/sizeof(wchar_t);
		}
	}
}
static void DMBString_Insert( DString *self, const char* chs, size_t at, size_t rm, size_t cp )
{
	size_t i;
	if( chs == NULL ) return;
	if( at > self->size ) at = self->size;
	if( rm + at > self->size ) rm = self->size - at;
	DString_Detach( self );
	if( cp < rm ){
		for( i=at+cp; i<self->size+cp-rm; i++) self->mbs[i] = self->mbs[i+rm-cp];
		DString_Reserve( self, self->size + cp - rm );
	}else if( cp > rm ){
		DString_Reserve( self, self->size + cp - rm );
		for( i=self->size+cp-rm-1; i>=at+cp; i--) self->mbs[i] = self->mbs[i+rm-cp];
	}
	for( i=0; i<cp; i++ ) self->mbs[i+at] = chs[i];
	self->size += cp-rm;
	self->mbs[self->size] = 0;
}
static void DWCString_Insert( DString *self, const wchar_t* chs, size_t at, size_t rm, size_t cp )
{
	size_t i;
	if( chs == NULL ) return;
	if( at > self->size ) at = self->size;
	if( rm + at > self->size ) rm = self->size - at;
	DString_Detach( self );
	if( cp < rm ){
		for( i=at+cp; i<self->size+cp-rm; i++) self->wcs[i] = self->wcs[i+rm-cp];
		DString_Reserve( self, self->size + cp - rm );
	}else if( cp > rm ){
		DString_Reserve( self, self->size + cp - rm );
		for( i=self->size+cp-rm-1; i>=at+cp; i--) self->wcs[i] = self->wcs[i+rm-cp];
	}
	for( i=0; i<cp; i++ ) self->wcs[i+at] = chs[i];
	self->size += cp-rm;
	self->wcs[self->size] = 0;
}
void DString_Insert( DString *self, DString *chs, size_t at, size_t rm, size_t cp )
{
	if( cp ==0 ) cp = chs->size;
	DString_Detach( self );
	if( self->mbs && chs->mbs ){
		DMBString_Insert( self, chs->mbs, at, rm, cp );
	}else if( self->wcs && chs->wcs ){
		DWCString_Insert( self, chs->wcs, at, rm, cp );
	}else if( self->mbs ){
		DString *str = DString_New(1);
		DMBString_AppendWCS( str, chs->wcs, cp );
		DMBString_Insert( self, str->mbs, at, rm, str->size );
		DString_Delete( str );
	}else{
		DString *str = DString_New(0);
		DWCString_AppendMBS( str, chs->mbs, cp );
		DWCString_Insert( self, str->wcs, at, rm, str->size );
		DString_Delete( str );
	}
}
void DString_InsertMBS( DString *self, const char *chs, size_t at, size_t rm, size_t n )
{
	if( n ==0 ) n = strlen( chs );
	DString_Detach( self );
	if( self->mbs ){
		DMBString_Insert( self, chs, at, rm, n );
	}else{
		DString *wcs = DString_New(0);
		DWCString_AppendMBS( wcs, chs, n );
		DWCString_Insert( self, wcs->wcs, at, rm, wcs->size );
		DString_Delete( wcs );
	}
}
void DString_InsertChar( DString *self, const char ch, size_t at )
{
	char chs[2];
	chs[0] = ch;
	chs[1] = '\0';
	DString_InsertMBS( self, chs, at, 0, 1 );
}
void DString_InsertWCS( DString *self, const wchar_t *chs, size_t at, size_t rm, size_t n )
{
	if( n ==0 ) n = wcslen( chs );
	DString_Detach( self );
	if( self->wcs ){
		DWCString_Insert( self, chs, at, rm, n );
	}else{
		DString *mbs = DString_New(1);
		DMBString_AppendWCS( mbs, chs, n );
		DMBString_Insert( self, mbs->mbs, at, rm, mbs->size );
		DString_Delete( mbs );
	}
}
void DString_Append( DString *self, DString *chs )
{
	DString_Detach( self );
	if( self->mbs && chs->mbs ){
		DMBString_Append( self, chs->mbs, chs->size );
	}else if( self->wcs && chs->wcs ){
		DWCString_Append( self, chs->wcs, chs->size );
	}else if( self->mbs ){
		DMBString_AppendWCS( self, chs->wcs, chs->size );
	}else{
		DWCString_AppendMBS( self, chs->mbs, chs->size );
	}
}
void DString_AppendChar( DString *self, const char ch )
{
	DString_Detach( self );
	if( self->mbs ){
		DMBString_AppendChar( self, ch );
	}else{
		DWCString_AppendChar( self, ch );
	}
}
void DString_AppendWChar( DString *self, const wchar_t ch )
{
	if( self->mbs && ch >=0 && ch <= 0xff ){
		DString_AppendChar( self, (char)ch );
		return;
	}
	DString_Detach( self );
	if( self->mbs ){
		DMBString_AppendWChar( self, ch );
	}else{
		DWCString_AppendWChar( self, ch );
	}
}
void DString_AppendDataMBS( DString *self, const char *chs, size_t n )
{
	DString_Detach( self );
	if( self->mbs ){
		DMBString_Append( self, chs, n );
	}else{
		DWCString_AppendMBS( self, chs, n );
	}
}

void DString_AppendMBS( DString *self, const char *chs )
{
	DString_AppendDataMBS(self, chs, strlen( chs ));
}

void DString_AppendDataWCS( DString *self, const wchar_t *chs,size_t n )
{
	DString_Detach( self );
	if( self->wcs ){
		DWCString_Append( self, chs, n );
	}else{
		DMBString_AppendWCS( self, chs, n );
	}
}

void DString_AppendWCS( DString *self, const wchar_t *chs )
{
	DString_AppendDataWCS(self,chs,wcslen( chs ));
}

void DString_SetDataMBS( DString *self, const char *bytes, size_t count )
{
	DString_Clear( self );
	DString_AppendDataMBS( self, bytes, count );
}
void DString_SetDataWCS( DString *self, const wchar_t *data, size_t count )
{
	DString_Clear( self );
	DString_AppendDataWCS( self, data, count );
}
void DString_Replace( DString *self, DString *chs, size_t start, size_t rm )
{
	DString_Insert( self, chs, start, rm, chs->size );
}
void DString_ReplaceMBS( DString *self, const char *chs, size_t start, size_t rm )
{
	/* Use by DaoParser only, guarantee to be MBS. */
	if( self->mbs ){
		DString_Detach( self );
		DMBString_Insert( self, chs, start, rm, strlen( chs ) );
	}
}
void DString_SubString( DString *self, DString *sub, size_t from, size_t n )
{
	size_t i, size = self->size;
	if( self->wcs ) DString_ToWCS( sub );
	if( self->mbs ) DString_ToMBS( sub );
	if( from >= size ){
		DString_Clear( sub );
		return;
	}
	if( n > size ) n = size;
	if( from+n > size ) n = size-from;
	DString_Resize( sub, n );

	if( self->mbs ){
		for( i=0; i<n; i++) sub->mbs[i] = self->mbs[i+from];
	}else{
		for( i=0; i<n; i++) sub->wcs[i] = self->wcs[i+from];
	}
}
size_t DString_Find( DString *self, DString *chs, size_t start )
{
	size_t res = MAXSIZE;
	if( self->mbs && chs->mbs ){
		res = DMBString_Find( self, start, chs->mbs, chs->size );
	}else if( self->wcs && chs->wcs ){
		res = DWCString_Find( self, start, chs->wcs, chs->size );
	}else if( self->mbs ){
		DString *mbs = DString_New(1);
		DMBString_AppendWCS( mbs, chs->wcs, chs->size );
		res = DMBString_Find( self, start, mbs->mbs, mbs->size );
		DString_Delete( mbs );
	}else{
		DString *wcs = DString_New(0);
		DWCString_AppendMBS( wcs, chs->mbs, chs->size );
		res = DWCString_Find( self, start, wcs->wcs, wcs->size );
		DString_Delete( wcs );
	}
	return res;
}
size_t DString_RFind( DString *self, DString *chs, size_t start )
{
	size_t res = MAXSIZE;
	if( self->mbs && chs->mbs ){
		res = DMBString_RFind( self, start, chs->mbs, chs->size );
	}else if( self->wcs && chs->wcs ){
		res = DWCString_RFind( self, start, chs->wcs, chs->size );
	}else if( self->mbs ){
		DString *mbs = DString_New(1);
		DMBString_AppendWCS( mbs, chs->wcs, chs->size );
		res = DMBString_RFind( self, start, mbs->mbs, mbs->size );
		DString_Delete( mbs );
	}else{
		DString *wcs = DString_New(0);
		DWCString_AppendMBS( wcs, chs->mbs, chs->size );
		res = DWCString_RFind( self, start, wcs->wcs, wcs->size );
		DString_Delete( wcs );
	}
	return res;
}
size_t DString_FindMBS( DString *self, const char *ch, size_t start )
{
	size_t res = MAXSIZE;
	size_t M = strlen( ch );
	if( self->mbs ){
		res = DMBString_Find( self, start, ch, M );
	}else{
		DString *wcs = DString_New(0);
		DWCString_AppendMBS( wcs, ch, M );
		res = DWCString_Find( self, start, wcs->wcs, wcs->size );
		DString_Delete( wcs );
	}
	return res;
}
size_t DString_RFindMBS( DString *self, const char *ch, size_t start )
{
	size_t res = MAXSIZE;
	size_t M = strlen( ch );
	if( self->mbs ){
		res = DMBString_RFind( self, start, ch, M );
	}else{
		DString *wcs = DString_New(0);
		DWCString_AppendMBS( wcs, ch, M );
		res = DWCString_RFind( self, start, wcs->wcs, wcs->size );
		DString_Delete( wcs );
	}
	return res;
}
size_t DString_FindChar( DString *self, char ch, size_t start )
{
	size_t i;
	if( self->mbs ){
		for(i=start; i<self->size; i++ ) if( self->mbs[i] == ch ) return i;
	}else{
		wchar_t wch = ch;
		for(i=start; i<self->size; i++ ) if( self->wcs[i] == wch ) return i;
	}
	return MAXSIZE;
}
size_t DString_RFindChar( DString *self, char ch, size_t start )
{
	int i;
	if( self->size ==0 ) return MAXSIZE;
	if( start >= self->size ) start = self->size - 1;
	if( self->mbs ){
		for(i=start; i >=0; i-- ) if( self->mbs[i] == ch ) return i;
	}else{
		wchar_t wch = ch;
		for(i=start; i >=0; i-- ) if( self->wcs[i] == wch ) return i;
	}
	return MAXSIZE;
}
size_t DString_FindWChar( DString *self, wchar_t ch, size_t start )
{
	size_t i;
	if( self->wcs ){
		for(i=start; i<self->size; i++ ) if( self->wcs[i] == ch ) return i;
	}else{
		DString *s = DString_New(1);
		DMBString_AppendWChar( s, ch );
		i = DString_Find( self, s, start );
		DString_Delete( s );
		return i;
	}
	return MAXSIZE;
}

DString* DString_Copy( DString *self )
{
	DString *str = DString_New( self->mbs != 0 );
	DString_Assign( str, self );
	return str;
}
/* Real copying, no implicit sharing here. For thread safty. */
DString* DString_DeepCopy( DString *self )
{
	int share = self->shared;
	DString *copy = DString_New( DString_IsMBS( self ) );
	DString_SetSharing( copy, share );
	if( DString_IsMBS( self ) ){
		DString_Resize( copy, self->size );
		memcpy( copy->mbs, self->mbs, self->size *sizeof(char) );
	}else{
		DString_Resize( copy, self->size );
		memcpy( copy->wcs, self->wcs, self->size *sizeof(wchar_t) );
	}
	return copy;
}
void DString_Assign( DString *self, DString *chs )
{
	int *data1 = (self->mbs ? (int*)self->mbs : (int*)self->wcs) - self->shared;
	int *data2 = (chs->mbs ? (int*)chs->mbs : (int*)chs->wcs) - chs->shared;
	int assigned = 0;
	if( self == chs ) return;
	if( data1 == data2 ) return;
	//XXX

#ifdef DAO_WITH_THREAD
	DMutex_Lock( & mutex_string_sharing );
#endif
	if( self->shared && chs->shared ){
		data1[0] -= 1;
		if( data1[0] ==0 ) dao_free( data1 );
		*self = *chs;
		data2[0] += 1;
		assigned = 1;
	}else if( data1 == NULL && chs->shared ){
		*self = *chs;
		data2[0] += 1;
		assigned = 1;
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & mutex_string_sharing );
#endif

	if( assigned ) return;
	if( self->mbs == NULL && self->wcs == NULL ){
		if( chs->mbs ){
			self->wcs = NULL;
			self->size = self->bufSize = chs->size;
			self->mbs = (char*) dao_malloc( (chs->size + 1)*sizeof(char) );
			memcpy( self->mbs, chs->mbs, chs->size*sizeof(char) );
			self->mbs[ self->size ] = 0;
		}else{
			self->mbs = NULL;
			self->size = self->bufSize = chs->size;
			self->wcs = (wchar_t*) dao_malloc( (chs->size + 1)*sizeof(wchar_t) );
			memcpy( self->wcs, chs->wcs, chs->size*sizeof(wchar_t) );
			self->wcs[ self->size ] = 0;
		}
	}else if( self->mbs && chs->mbs ){
		DString_Resize( self, chs->size );
		memcpy( self->mbs, chs->mbs, chs->size*sizeof(char) );
	}else if( self->wcs && chs->wcs ){
		DString_Resize( self, chs->size );
		memcpy( self->wcs, chs->wcs, chs->size*sizeof(wchar_t) );
	}else{
		DString_Clear( self );
		DString_Append( self, chs );
	}
}
static int DMBString_Compare( DString *self, DString *chs )
{
	size_t min = self->size > chs->size ? chs->size : self->size;
	char *p1 = self->mbs;
	char *p2 = chs->mbs;
	char *stop = p1 + min;
	while( p1 != stop ){
		if( *p1 == *p2 ){
			p1 ++;
			p2 ++;
		}else if( *p1 > *p2 ){
			return 1;
		}else{
			return -1;
		}
	}
	if( self->size == chs->size )
		return 0;
	else if( self->size < chs->size )
		return -1;
	return 1;
}
static int DWCString_Compare( DString *self, DString *chs )
{
	size_t min = self->size > chs->size ? chs->size : self->size;
	size_t i;
	for( i=0; i<min; i++ ){
		if( self->wcs[i] > chs->wcs[i] )
			return 1;
		else if( self->wcs[i] < chs->wcs[i] )
			return -1;
	}
	if( self->size == chs->size )
		return 0;
	else if( self->size < chs->size )
		return -1;
	return 1;
}
int DString_Compare( DString *self, DString *chs )
{
	int res = 0;
	if( self->mbs == chs->mbs && self->wcs == chs->wcs ) return 0;
	if( self->mbs && chs->mbs ){
		res = DMBString_Compare( self, chs );
	}else if( self->wcs && chs->wcs ){
		res = DWCString_Compare( self, chs );
	}else if( self->mbs ){
		DString *wcs = DString_New(0);
		DWCString_AppendMBS( wcs, self->mbs, self->size );
		res = DWCString_Compare( wcs, chs );
		DString_Delete( wcs );
	}else{
		DString *wcs = DString_New(0);
		DWCString_AppendMBS( wcs, chs->mbs, chs->size );
		res = DWCString_Compare( self, wcs );
		DString_Delete( wcs );
	}
	return res;
}
int DString_EQ( DString *self, DString *chs )
{
	return (DString_Compare( self, chs )==0);
}
void DString_Add( DString *self, DString *left, DString *right )
{
	DString_Assign( self, left );
	DString_Append( self, right );
}
void DString_Chop( DString *self )
{
	DString_Detach( self );
	if( self->mbs ){
		if( self->size > 0 && self->mbs[ self->size-1 ] == EOF  ) self->mbs[ --self->size ] = 0;
		if( self->size > 0 && self->mbs[ self->size-1 ] == '\n' ) self->mbs[ --self->size ] = 0;
		if( self->size > 0 && self->mbs[ self->size-1 ] == '\r' ) self->mbs[ --self->size ] = 0;
	}else{
		if( self->size > 0 && self->wcs[ self->size-1 ] == EOF   ) self->wcs[ --self->size ] = 0;
		if( self->size > 0 && self->wcs[ self->size-1 ] == L'\n' ) self->wcs[ --self->size ] = 0;
		if( self->size > 0 && self->wcs[ self->size-1 ] == L'\r' ) self->wcs[ --self->size ] = 0;
	}
}
void DString_Trim( DString *self )
{
	int i, ch;
	DString_Detach( self );
	if( self->mbs ){
		while( self->size > 0 ){
			ch = self->mbs[ self->size-1 ];
			if( ch == EOF || isspace( ch ) ){
				self->size --;
				self->mbs[ self->size ] = 0;
			}else{
				break;
			}
		}
		for( i=0; i < self->size; i++ ){
			ch = self->mbs[i];
			if( ch != EOF && ! isspace( ch ) ) break;
		}
		DString_Erase( self, 0, i );
	}else{
		while( self->size > 0 ){
			ch = self->wcs[ self->size-1 ];
			if( ch == EOF || isspace( ch ) ){
				self->size --;
				self->wcs[ self->size ] = 0;
			}else{
				break;
			}
		}
		for( i=0; i < self->size; i++ ){
			ch = self->wcs[i];
			if( ch != EOF && ! isspace( ch ) ) break;
		}
		DString_Erase( self, 0, i );
	}
}
size_t DString_CheckUTF8( DString *self )
{
	size_t i = 0, m, size = self->size;
	size_t total = 0;
	size_t valid = 0;
	unsigned char *mbs;
	if( self->wcs ) return 0;
	mbs = (unsigned char*) self->mbs;
	while( i < size ){
		unsigned char ch = mbs[i++];
		m = utf8_markers[ ch ];
		total += 1;
		if( m == 1 || (ch <= 0x7f && ! isprint( ch )) ) continue; /*invalid encoding*/
		while( m > 0 && i < size && utf8_markers[ mbs[i] ] == 1 ) i += 1, m -= 1;
		valid += m <= 1;
	}
	if( valid >= 0.95 * total ) return total;
	return 0;
}
void DString_Reverse( DString *self )
{
	DString *front, *back;
	size_t m, utf8 = DString_CheckUTF8( self );
	size_t i, k, gi, gj, size = self->size;
	size_t half = size / 2;
	dint j;
	unsigned char ch, *mbs;
	if( size <= 1 ) return;
	DString_Detach( self );
	if( self->wcs ){
		for(i=0; i<half; i++){
			wchar_t c = self->wcs[i];
			self->wcs[i] = self->wcs[size-1-i];
			self->wcs[size-1-i] = c;
		}
		return;
	}
	if( utf8 == 0 ){
		for(i=0; i<half; i++){
			char c = self->mbs[i];
			self->mbs[i] = self->mbs[size-1-i];
			self->mbs[size-1-i] = c;
		}
		return;
	}
	front = DString_New(1);
	back = DString_New(1);
	mbs = (unsigned char*) self->mbs;
	i = 0;
	j = size - 1;
	gi = gj = 0;
	while( utf8 > 1 ){
		if( front->size ==0 ){ /* get valid multibytes from front */
			size_t i2 = 0;
			gi += 1;
			ch = mbs[i++];
			m = utf8_markers[ ch ];
			DString_InsertChar( front, ch, 0 );
			while( m > 1 && i < size && utf8_markers[ mbs[i] ] == 1 ){
				DString_InsertChar( front, self->mbs[i], ++i2 );
				gi += 1;
				i += 1;
				m -= 1;
			}
		}
		if( back->size ==0 ){ /* get valid multibytes from back */
			size_t j2 = j;
			k = 1;
			while( k < 7 && j >= 0 && utf8_markers[ mbs[j] ] == 1 ) j -= 1, k += 1;
			m = utf8_markers[ mbs[j] ];
			if( m && m != k ){
				DString_AppendChar( back, mbs[j2] );
				gj += 1;
				j = j2 - 1;
			}else{
				DString_AppendDataMBS( back, (char*)mbs+j, k );
				gj += k;
				j -= 1;
			}
		}
		if( back->size <= gi ){ /* enough space reserved for back mulitbytes */
			strncpy( self->mbs + i - gi, back->mbs, back->size );
			gi -= back->size;
			back->size = 0;
			utf8 -= 1;
		}
		if( front->size <= gj ){ /* enough space reserved for front mulitbytes */
			strncpy( self->mbs + j + 1 + (gj - front->size), front->mbs, front->size );
			gj -= front->size;
			front->size = 0;
			utf8 -= 1;
		}
	}
	DString_Append( back, front );
	if( back->size && gi ) strncpy( self->mbs + i - gi, back->mbs, back->size );
	DString_Delete( front );
	DString_Delete( back );
}
size_t DString_BalancedChar( DString *self, uint_t ch0, uint_t lch0, uint_t rch0, 
		uint_t esc0, size_t start, size_t end, int countonly )
{
	size_t size = self->size;
	size_t i, count = 0;
	if( self->mbs ){
		char *src = self->mbs;
		char chr = (char) ch0;
		char lch = (char) lch0;
		char rch = (char) rch0;
		char esc = (char) esc0;
		char c;
		int bc = 0;
		if( ch0 >= 128 || start >= size ) return MAXSIZE;
		if( end > size ) end = size;
		for(i=start; i<end; i++){
			c = src[i];
			if( c == esc ){
				i ++;
				continue;
			}
			if( c == chr && bc ==0 ){
				if( countonly )
					count ++;
				else return i;
			}
			if( c == lch ){
				bc ++;
			}else if( c == rch ){
				bc --;
				if( bc <0 ) return MAXSIZE;
			}
		}
	}else{
		wchar_t *src = self->wcs;
		wchar_t chr = (wchar_t) ch0;
		wchar_t lch = (wchar_t) lch0;
		wchar_t rch = (wchar_t) rch0;
		wchar_t esc = (wchar_t) esc0;
		wchar_t c;
		int bc = 0;
		if( ch0 >= 128 || start >= size ) return MAXSIZE;
		if( end > size ) end = size;
		for(i=start; i<end; i++){
			c = src[i];
			if( c == esc ){
				i ++;
				continue;
			}
			if( c == chr && bc ==0 ){
				if( countonly )
					count ++;
				else return i;
			}
			if( c == lch ){
				bc ++;
			}else if( c == rch ){
				bc --;
				if( bc <0 ) return MAXSIZE;
			}
		}
	}
	if( countonly ) return count;
	return MAXSIZE;
}

static char *empty_mbs = "";
static wchar_t *empty_wcs = L"";

DString DString_WrapBytes( const char *mbs, int n )
{
	DString str = { 0, 0, 0, 0, NULL, NULL };
	str.mbs = empty_mbs;
	if( mbs == NULL ) return str;
	str.mbs = (char*) mbs;
	str.size = str.bufSize = n;
	return str;
}
DString DString_WrapMBS( const char *mbs )
{
	return DString_WrapBytes( mbs, mbs ? strlen( mbs ) : 0 );
}
DString DString_WrapWCS( const wchar_t *wcs )
{
	DString str = { 0, 0, 0, 0, NULL, NULL };
	str.wcs = empty_wcs;
	if( wcs == NULL ) return str;
	str.wcs = (wchar_t*) wcs;
	str.size = str.bufSize = wcslen( wcs );
	return str;
}

#define IO_BUF_SIZE  512
int DString_ReadFile( DString *self, const char *fname )
{
	FILE *fin = fopen( fname, "r" );
	char buf[IO_BUF_SIZE];
	DString_Clear( self );
	if( fin == NULL ) return 0;
	while(1){
		size_t count = fread( buf, 1, IO_BUF_SIZE, fin );
		if( count ==0 ) break;
		DString_AppendDataMBS( self, buf, count );
	}
	fclose( fin );
	return 1;
}
