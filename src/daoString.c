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

	if( M == 0 ) return MAXSIZE;
	if( self->size==0 ) return MAXSIZE;
	if( S <0 || S >= self->size ) S = self->size-1;
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
	self->data = (size_t*) dao_malloc( sizeof(size_t) + bsize );
	self->data[0] = 1;
	self->size = 0;
	self->bufSize = 0;
	if( mbs ){
		self->mbs = (char*)(self->data + 1);
		self->mbs[0] = 0;
	}else{
		self->wcs = (wchar_t*)(self->data + 1);
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
	size_t *data = self->data;
	int share = data != (size_t*)self->mbs && data != (size_t*)self->wcs;
	/* refCount ==0 for no-sharing string: */
	if( share ){
#ifdef DAO_WITH_THREAD
		DMutex_Lock( & mutex_string_sharing );
#endif
		if( share ){
			data[0] -= 1;
			if( data[0] ==0 ) dao_free( data );
		}
#ifdef DAO_WITH_THREAD
		DMutex_Unlock( & mutex_string_sharing );
#endif
	}
	if( share ==0 ) dao_free( data );
}
void DString_Delete( DString *self )
{
	DString_DeleteData( self );
	dao_free( self );
}
void DString_Detach( DString *self )
{
	size_t size, chsize;
	size_t *data = self->data;
	int share = data != (size_t*)self->mbs && data != (size_t*)self->wcs;
	if( share ==0 || data[0] ==1 ) return;
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & mutex_string_sharing );
#endif
	if( data[0] >1 ){
		data[0] -= 1;
		chsize = self->mbs ? sizeof(char) : sizeof(wchar_t);
		size = (self->size + 1) * chsize;
		self->bufSize = self->size;
		self->data = (size_t*) dao_malloc( size + sizeof(size_t) );
		self->data[0] = 1;
		memcpy( self->data+1, data+1, size );
		if( self->mbs ) self->mbs = (char*)(self->data + 1);
		if( self->wcs ) self->wcs = (wchar_t*)(self->data + 1);
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & mutex_string_sharing );
#endif
}
void DString_SetSharing( DString *self, int sharing )
{
	size_t *data = self->data;
	int old = data != (size_t*)self->mbs && data != (size_t*)self->wcs;
	if( (old !=0) == (sharing !=0) ) return;
	DString_Detach( self );

#ifdef DAO_WITH_THREAD
	DMutex_Lock( & mutex_string_sharing );
#endif
	if( sharing ==0 ){
		if( self->mbs ){
			memmove( self->data, self->mbs, self->size*sizeof(char) );
			self->bufSize += sizeof(size_t)/sizeof(char);
			self->mbs = (char*) self->data;
			self->mbs[ self->size ] = 0;
		}else{
			memmove( self->data, self->wcs, self->size*sizeof(wchar_t) );
			self->bufSize += sizeof(size_t)/sizeof(wchar_t);
			self->wcs = (wchar_t*) self->data;
			self->wcs[ self->size ] = 0;
		}
	}else if( self->mbs ){
		if( self->bufSize < self->size + sizeof(size_t)/sizeof(char) ){
			size_t size = (self->size + 1)*sizeof(char) + sizeof(size_t);
			self->data = (size_t*) dao_realloc( self->data, size );
			self->bufSize = self->size;
		}
		self->mbs = (char*)(self->data + 1);
		memmove( self->mbs, self->data, self->size*sizeof(char) );
		self->mbs[ self->size ] = 0;
		self->data[0] = 1;
	}else{ /* self->wcs */
		if( self->bufSize < self->size + sizeof(size_t)/sizeof(wchar_t) ){
			size_t size = (self->size + 1)*sizeof(wchar_t) + sizeof(size_t);
			self->data = (size_t*) dao_realloc( self->data, size );
			self->bufSize = self->size;
		}
		self->wcs = (wchar_t*)(self->data + 1);
		memmove( self->wcs, self->data, self->size*sizeof(wchar_t) );
		self->wcs[ self->size ] = 0;
		self->data[0] = 1;
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & mutex_string_sharing );
#endif
}

int DString_IsMBS( DString *self )
{
	return ( self->wcs == NULL );
}
int DString_IsDigits( DString *self )
{
	size_t i;
	if( self->mbs ){
		for( i=0; i<self->size; i++ )
			if( self->mbs[i] < '0' || self->mbs[i] > '9' )
				return 0;
	}else{
		for( i=0; i<self->size; i++ )
			if( self->wcs[i] < L'0' || self->wcs[i] > L'9' )
				return 0;
	}
	return 1;
}
int DString_IsDecimal( DString *self )
{
	size_t i;
	if( self->mbs ){
		for( i=0; i<self->size; i++ )
			if( (self->mbs[i] < '0' || self->mbs[i] > '9') && self->mbs[i] != '.' )
				return 0;
	}else{
		for( i=0; i<self->size; i++ )
			if( (self->wcs[i] < L'0' || self->wcs[i] > L'9') && self->wcs[i] != L'.' )
				return 0;
	}
	return 1;
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
	size_t i, bsize;
	DString_Detach( self );
	if( size <= self->bufSize && 2*size >= self->bufSize ) return;
	if( self->mbs ){
		int share = self->data != (size_t*) self->mbs;
		if( size > self->bufSize || 2*size < self->bufSize ){
			self->bufSize = size * (size >= self->bufSize ? 1.2 : 1) + 1;
			bsize = (self->bufSize + 1)*sizeof(char) + share*sizeof(size_t);
			self->data = (size_t*)dao_realloc( self->data, bsize );
			self->mbs = (char*)(self->data + share);
		}
	}else{
		int share = self->data != (size_t*) self->wcs;
		if( size > self->bufSize || 2*size < self->bufSize ){
			self->bufSize = size * (size >= self->bufSize ? 1.2 : 1) + 1;
			bsize = (self->bufSize + 1)*sizeof(wchar_t) + share*sizeof(size_t);
			self->data = (size_t*)dao_realloc( self->data, bsize );
			self->wcs = (wchar_t*)(self->data + share);
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
static void DMBString_AppendWCS( DString *self, const wchar_t *chs, size_t n )
{
	mbstate_t state;
	size_t i = 0;
	while( i < n ){
		size_t smin, len;
		const wchar_t *wcs = chs + i;
		len = wcslen( wcs );
		if( (i + len) > n ) len = n - i;
		DString_Reserve( self, self->size + 7*len );
		/* under windows using MinGW, passing null output buffer,
		 * will NOT cause the function to perform conversion and 
		 * return the required buffer size. */
		memset( & state, 0, sizeof(state) );
		smin = wcsrtombs( self->mbs + self->size, (const wchar_t**)&wcs, 7*len, & state );
		if( smin == (size_t)-1 ) break;
		self->size += smin;
		if( (i + len < n ) ) DMBString_AppendChar( self, '\0' );
		i += len + 1;
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
	mbstate_t state;
	size_t i = 0;
	while( i < n ){
		size_t smin, len;
		const char *mbs = chs + i;
		len = strlen( mbs );
		if( (i + len) > n ) len = n - i;
		DString_Reserve( self, self->size + len );
		/* under windows using MinGW, passing null output buffer,
		 * will NOT cause the function to perform conversion and 
		 * return the required buffer size. */
		memset( & state, 0, sizeof (state) );
		smin = mbsrtowcs( self->wcs + self->size, (const char**)&mbs, len, & state );
		if( smin == (size_t)-1 ) break;
		self->size += smin;
		if( (i + len) < n ) DWCString_AppendWChar( self, L'\0' );
		i += len + 1;
	}
	self->wcs[ self->size ] = 0;
}
void DString_ToWCS( DString *self )
{
	DString tmp = *self;
	size_t i = 0, size = self->size;
	int share = self->data != (size_t*)self->mbs;

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
	size_t i = 0, size = self->size;
	int share = self->data != (size_t*)self->wcs;

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
		for( i=0; i<self->size; i++ ){
			*mbs = tolower( *mbs );
			mbs ++;
		}
	}else{
		wchar_t *wcs = self->wcs;
		for( i=0; i<self->size; i++ ){
			*wcs = towlower( *wcs );
			wcs ++;
		}
	}
}
void DString_ToUpper( DString *self )
{
	size_t i;
	DString_Detach( self );
	if( self->mbs ){
		char *mbs = self->mbs;
		for( i=0; i<self->size; i++ ){
			*mbs = toupper( *mbs );
			mbs ++;
		}
	}else{
		wchar_t *wcs = self->wcs;
		for( i=0; i<self->size; i++ ){
			*wcs = towupper( *wcs );
			wcs ++;
		}
	}
}
size_t DString_Size( DString *self )
{
	return self->size;
}

void DString_Resize( DString *self, size_t size )
{
	size_t i, bsize = self->bufSize;
	DString_Detach( self );
	if( size == self->size && size <= bsize && 2*size >= bsize ) return;
	if( self->mbs ){
		int share = self->data != (size_t*) self->mbs;
		if( size > self->bufSize || 2*size < self->bufSize ){
			self->bufSize = size;
			bsize = (self->bufSize + 1)*sizeof(char) + share*sizeof(size_t);
			self->data = (size_t*)dao_realloc( self->data, bsize );
			self->mbs = (char*)(self->data + share);
		}
		for(i=self->size; i<size; i++) self->mbs[i] = 0;
		self->mbs[ size ] = 0;
	}else{
		int share = self->data != (size_t*) self->wcs;
		if( size > self->bufSize || 2*size < self->bufSize ){
			self->bufSize = size;
			bsize = (self->bufSize + 1)*sizeof(wchar_t) + share*sizeof(size_t);
			self->data = (size_t*)dao_realloc( self->data, bsize );
			self->wcs = (wchar_t*)(self->data + share);
		}
		for(i=self->size; i<size; i++) self->wcs[i] = 0;
		self->wcs[ size ] = 0;
	}
	self->size = size;
}
void DString_Clear( DString *self )
{
	size_t *data = self->data;
	int share = data != (size_t*)self->mbs && data != (size_t*)self->wcs;
	int mbs = self->mbs != NULL;
	DString_Detach( self );
	DString_DeleteData( self );
	DString_Init( self, mbs );
	DString_SetSharing( self, share );
}
void DString_Erase( DString *self, size_t start, size_t n )
{
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
	if( self->mbs ){
		int share = self->data != (size_t*) self->mbs;
		for( i=start; i<start+rest; i++ ) self->mbs[i] = self->mbs[i+n];
		self->mbs[start+rest] = 0;
		self->size -= n;
		if( self->size < 0.5*self->bufSize && self->size+5 < self->bufSize ){
			self->bufSize = (size_t)(0.6 * self->bufSize) + 1;
			bsize = (self->bufSize+1)*sizeof(char) + sizeof(size_t);
			self->data = (size_t*)dao_realloc( self->data, bsize );
			self->mbs = (char*)(self->data + share);
			if( share ==0 ) self->bufSize += sizeof(size_t)/sizeof(char);
		}
	}else{
		int share = self->data != (size_t*) self->wcs;
		for( i=start; i<start+rest; i++ ) self->wcs[i] = self->wcs[i+n];
		self->wcs[start+rest] = 0;
		self->size -= n;
		if( self->size < 0.5*self->bufSize && self->size + 5 < self->bufSize ){
			self->bufSize = (size_t)(0.6 * self->bufSize) + 1;
			bsize = (self->bufSize+1)*sizeof(wchar_t) + sizeof(size_t);
			self->data = (size_t*)dao_realloc( self->data, bsize );
			self->wcs = (wchar_t*)(self->data + share);
			if( share ==0 ) self->bufSize += sizeof(size_t)/sizeof(wchar_t);
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
	if( self == NULL ) return; /* in parsing, DaoInode.annot can be NULL */
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
	if( self == NULL ) return; /* in parsing, DaoInode.annot can be NULL */
	DString_Detach( self );
	if( self->mbs ){
		DMBString_AppendChar( self, ch );
	}else{
		DWCString_AppendChar( self, ch );
	}
}
void DString_AppendWChar( DString *self, const wchar_t ch )
{
	DString_Detach( self );
	if( self->mbs ){
		DMBString_AppendWChar( self, ch );
	}else{
		DWCString_AppendWChar( self, ch );
	}
}
void DString_AppendDataMBS( DString *self, const char *chs, size_t n )
{
	if( self == NULL ) return; /* in parsing, DaoInode.annot can be NULL */
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
	if( n < 0 || n > size ) n = size;
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
		for(i=start; i<self->size; i++ )
			if( self->mbs[i] == ch ) return i;
	}else{
		wchar_t wch = ch;
		for(i=start; i<self->size; i++ )
			if( self->wcs[i] == wch ) return i;
	}
	return MAXSIZE;
}
size_t DString_RFindChar( DString *self, char ch, size_t start )
{
	int i;
	if( self->mbs ){
		if( self->size ==0 ) return MAXSIZE;
		if( start >= self->size ) start = self->size -1;
		for(i=start; i >=0; i-- )
			if( self->mbs[i] == ch ) return i;
	}else{
		wchar_t wch = ch;
		if( self->size ==0 ) return MAXSIZE;
		if( start >= self->size ) start = self->size -1;
		for(i=start; i >=0; i-- )
			if( self->wcs[i] == wch ) return i;
	}
	return MAXSIZE;
}
size_t DString_FindWChar( DString *self, wchar_t ch, size_t start )
{
	size_t i;
	if( self->wcs ){
		for(i=start; i<self->size; i++ )
			if( self->wcs[i] == ch ) return i;
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
	size_t *data = self->data;
	int share = data != (size_t*)self->mbs && data != (size_t*)self->wcs;
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
	size_t *data1, *data2;
	int share1, share2;
	int assigned = 0;
	if( self == NULL ) return; /* in parsing, DaoInode.annot can be NULL */
	if( self == chs ) return;
	if( self->data == chs->data ) return;

#ifdef DAO_WITH_THREAD
	DMutex_Lock( & mutex_string_sharing );
#endif
	data1 = self->data;
	data2 = chs->data;
	share1 = data1 != (size_t*)self->mbs && data1 != (size_t*)self->wcs;
	share2 = data2 != (size_t*)chs->mbs && data2 != (size_t*)chs->wcs;

	if( share1 && share2 ){
		data1[0] -= 1;
		if( data1[0] ==0 ) dao_free( data1 );
		*self = *chs;
		self->data[0] += 1;
		assigned = 1;
	}else if( self->data == NULL && share2 ){
		*self = *chs;
		self->data[0] += 1;
		assigned = 1;
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & mutex_string_sharing );
#endif

	if( assigned ) return;
	if( self->data == NULL ){
		if( chs->mbs ){
			self->wcs = NULL;
			self->size = self->bufSize = chs->size;
			self->data = dao_malloc( (chs->size + 1)*sizeof(char) );
			self->mbs = (char*) self->data;
			memcpy( self->mbs, chs->mbs, chs->size*sizeof(char) );
			self->mbs[ self->size ] = 0;
		}else{
			self->mbs = NULL;
			self->size = self->bufSize = chs->size;
			self->data = dao_malloc( (chs->size + 1)*sizeof(wchar_t) );
			self->wcs = (wchar_t*) self->data;
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
	if( self->data == chs->data ) return 0; /* shared */
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
		if( self->size > 0 && self->mbs[ self->size-1 ] == EOF ){
			self->size --;
			self->mbs[ self->size ] = 0;
		}
		if( self->size > 0 && self->mbs[ self->size-1 ] == '\n' ){
			self->size --;
			self->mbs[ self->size ] = 0;
		}
		if( self->size > 0 && self->mbs[ self->size-1 ] == '\r' ){
			self->size --;
			self->mbs[ self->size ] = 0;
		}
	}else{
		if( self->size > 0 && self->wcs[ self->size-1 ] == EOF ){
			self->size --;
			self->wcs[ self->size ] = 0;
		}
		if( self->size > 0 && self->wcs[ self->size-1 ] == L'\n' ){
			self->size --;
			self->wcs[ self->size ] = 0;
		}
		if( self->size > 0 && self->wcs[ self->size-1 ] == L'\r' ){
			self->size --;
			self->wcs[ self->size ] = 0;
		}
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

/* Corrected Block Tiny Encryption Algorithm (Corrected Block TEA, or XXTEA)
 * by David Wheeler and Roger Needham
 */
#define MX  ( (((z>>5)^(y<<2))+((y>>3)^(z<<4)))^((sum^y)+(k[(p&3)^e]^z)) )

int btea(int* v, int n, int *k)
{
	unsigned int z, y=v[0], sum=0, e, DELTA=0x9e3779b9;
	int p, q ;
	if (n > 1) { /* Coding Part */
		z=v[n-1];           
		q = 6+52/n ;
		while (q-- > 0) {
			sum += DELTA;
			e = (sum>>2) & 3 ;
			for (p=0; p<n-1; p++) y = v[p+1], z = v[p] += MX;
			y = v[0];
			z = v[n-1] += MX;
		}
		return 0 ; 
	} else if (n < -1) {  /* Decoding Part */
		n = -n ;
		q = 6+52/n ;
		sum = q*DELTA ;
		while (sum != 0) {
			e = (sum>>2) & 3;
			for (p=n-1; p>0; p--) z = v[p-1], y = v[p] -= MX;
			z = v[n-1];
			y = v[0] -= MX;
			sum -= DELTA;
		}
		return 0;
	}
	return 1;
}

const char *dec2hex = "0123456789abcdef";

static int HexDigit( char d )
{
	d = d | 0x20;
	if( ( (size_t)(d-'0') ) < 10 ) return d - '0';
	else if( ( (size_t)(d-'a') ) < 26 ) return d + 10 - 'a';
	return -1;
}
static int STR_Cipher( DString *self, DString *key, int hex, int flag )
{
	unsigned char ks[16];
	unsigned char *data = NULL;
	size_t size = 0;
	int i;
	DString_Detach( self );
	DString_ToMBS( self );
	if( self->size == 0 ) return 0;
	DString_ToMBS( key );
	if( key->size >= 32 ){
		for(i=0; i<16; i++){
			signed char c1 = HexDigit( key->mbs[2*i] );
			signed char c2 = HexDigit( key->mbs[2*i+1] );
			if( c1 <0 || c2 <0 ) return 1;
			ks[i] = 16 * c1 + c2;
		}
	}else if( key->size >= 16 ){
		memcpy( ks, key->mbs, 16 );
	}else{
		return 1;
	}
	if( flag == 1 ){
		size = self->size;
		i = size % 4;
		if( i ) i = 4 - i;
		DString_Resize( self, size + 4 + i );
		memmove( self->mbs + 4, self->mbs, size );
		*(int*) self->mbs = size;
		data = (unsigned char*) self->mbs;
		btea( (int*)self->mbs, self->size / 4, (int*) ks );
		if( hex ){
			size = self->size;
			DString_Resize( self, 2 * size );
			data = (unsigned char*) self->mbs;
			for(i=size-1; i>=0; i--){
				self->mbs[2*i+1] = dec2hex[ data[i] % 16 ];
				self->mbs[2*i] = dec2hex[ data[i] / 16 ];
			}
		}
	}else{
		if( hex ){
			if( self->size % 2 ) return 1;
			data = (unsigned char*) self->mbs;
			size = self->size / 2;
			for(i=0; i<size; i++){
				char c1 = HexDigit( data[2*i] );
				char c2 = HexDigit( data[2*i+1] );
				if( c1 <0 || c2 <0 ) return 1;
				data[i] = 16 * c1 + c2;
			}
			DString_Resize( self, size );
		}
		btea( (int*)self->mbs, - (self->size / 4), (int*) ks );
		size = *(int*) self->mbs;
		DString_Erase( self, 0, 4 );
		self->size = size;
	}
	return 0;
}
int DString_Encrypt( DString *self, DString *key, int hex )
{
	return STR_Cipher( self, key, hex, 1 );
}
int DString_Decrypt( DString *self, DString *key, int hex )
{
	return STR_Cipher( self, key, hex, 0 );
}

DString DString_WrapMBS( const char *mbs )
{
	DString str = { 0, 0, NULL, NULL, NULL };
	str.mbs = (char*) mbs;
	str.data = (size_t*) mbs;
	str.size = str.bufSize = strlen( mbs );
	return str;
}
DString DString_WrapWCS( const wchar_t *wcs )
{
	DString str = { 0, 0, NULL, NULL, NULL };
	str.wcs = (wchar_t*) wcs;
	str.data = (size_t*) wcs;
	str.size = str.bufSize = wcslen( wcs );
	return str;
}
