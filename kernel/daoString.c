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

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<wchar.h>

#include"daoString.h"
#include"daoThread.h"

#ifdef DAO_WITH_THREAD
DMutex  mutex_string_sharing;
#endif


//static
int dao_string[4] = {1,0,0,0};

/**/
void DString_Init( DString *self )
{
	self->bytes = (char*)(dao_string + 1);
	self->detached = 1;
	self->sharing = 1;
	self->size = 0;
	self->bufSize = 0;
}
DString* DString_New()
{
	DString *self = (DString*)dao_calloc( 1, sizeof(DString) );
	DString_Init( self );
	return self;
}
DString* DString_NewChars( const char *mbs )
{
	DString *self = DString_New();
	DString_SetChars( self, mbs );
	return self;
}

void DString_DeleteData( DString *self )
{
	int *data = (int*)self->bytes - self->sharing;

	if( data == NULL || data == dao_string ) return;

	if( self->sharing ){
#ifdef DAO_WITH_THREAD
		DMutex_Lock( & mutex_string_sharing );
#endif
		if( self->sharing ){
			data[0] -= 1;
			if( data[0] ==0 ) dao_free( data );
		}
#ifdef DAO_WITH_THREAD
		DMutex_Unlock( & mutex_string_sharing );
#endif
	}else{
		dao_free( data );
	}

	self->sharing = 0;
	self->bytes = NULL;
}
void DString_Delete( DString *self )
{
	DString_DeleteData( self );
	dao_free( self );
}
void DString_Detach( DString *self, int bufsize )
{
	daoint size;
	int *data2, *data = (int*)self->bytes - self->sharing;
	if( self->sharing == 0 ) return;
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & mutex_string_sharing );
#endif
	if( data[0] >1 || data == dao_string ){
		if( bufsize < self->size ) bufsize = self->size;
		data[0] -= 1;
		self->bufSize = bufsize + 1;
		data2 = (int*) dao_malloc( (self->bufSize + 1)*sizeof(char) + sizeof(int) );
		data2[0] = 1;
		memcpy( data2+1, data+1, (self->size + 1)*sizeof(char) );
		self->bytes = (char*)(data2 + 1);
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & mutex_string_sharing );
#endif
}
static int* DString_Realloc( DString *self, daoint bufsize )
{
	daoint bsize = (bufsize + 1)*sizeof(char) + self->sharing*sizeof(int);
	int *data, *data2;

	data = data2 = (int*)self->bytes - self->sharing;
	if( data == dao_string ) data = NULL;

	data = (int*)dao_realloc( data, bsize );
	self->bytes = (char*)(data + self->sharing);
	if( data2 == dao_string ) self->bytes[ self->size ] = '\0';
	if( self->sharing && data2 == dao_string ) data[0] = 1;
	return data;
}
void DString_SetSharing( DString *self, int sharing )
{
	int *data = (int*)self->bytes - self->sharing;
	if( (self->sharing == 0) == (sharing == 0) ) return;

	if( sharing && data == dao_string ){
		self->sharing = 1;
		return; /* OK for sharing; */
	}

	DString_Detach( self, self->bufSize );
	data = (int*)self->bytes - self->sharing;
	self->sharing = sharing != 0;

#ifdef DAO_WITH_THREAD
	DMutex_Lock( & mutex_string_sharing );
#endif
	if( sharing ==0 ){
		memmove( data, self->bytes, self->size*sizeof(char) );
		self->bufSize += sizeof(int)/sizeof(char);
		self->bytes = (char*) data;
		self->bytes[ self->size ] = 0;
	}else{
		if( self->bufSize < self->size + (daoint)(sizeof(int)/sizeof(char)) ){
			size_t size = (self->size + 1)*sizeof(char) + sizeof(int);
			if( data == dao_string ) data = NULL;
			data = (int*) dao_realloc( data, size );
			self->bufSize = self->size;
		}
		self->bytes = (char*)(data + 1);
		memmove( self->bytes, data, self->size*sizeof(char) );
		self->bytes[ self->size ] = 0;
		data[0] = 1;
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & mutex_string_sharing );
#endif
}

char*  DString_GetData( DString *self )
{
	return self->bytes;
}
void DString_SetChars( DString *self, const char *chs )
{
	if( self->bytes && self->bytes == chs ) return;
	if( chs == NULL ){
		DString_Clear( self );
		return;
	}
	DString_Reset( self, strlen( chs ) );
	memcpy( self->bytes, chs, self->size*sizeof(char) );
}
void DString_Reserve( DString *self, daoint size )
{
	int *data = (int*)self->bytes - self->sharing;
	daoint bufsize = size >= self->bufSize ? (1.2*size + 4) : self->bufSize;
	if( self->sharing ) DString_Detach( self, bufsize );
	if( size <= self->bufSize ) return;
	self->bufSize = bufsize;
	DString_Realloc( self, self->bufSize );
}
void DString_ToLower( DString *self )
{
	daoint i;
	char *bytes;
	if( self->sharing ) DString_Detach( self, self->size );
	for(i=0,bytes=self->bytes; i<self->size; i++, bytes++) *bytes = tolower( *bytes );
}
void DString_ToUpper( DString *self )
{
	daoint i;
	char *bytes;
	if( self->sharing ) DString_Detach( self, self->size );
	for(i=0,bytes=self->bytes; i<self->size; i++, bytes++) *bytes = toupper( *bytes );
}
daoint DString_Size( DString *self )
{
	return self->size;
}

void DString_Reset( DString *self, daoint size )
{
	if( size <= self->bufSize ){
		if( self->sharing ) DString_Detach( self, self->bufSize );
		self->size = size;
		self->bytes[size] = '\0';
		return;
	}
	DString_Resize( self, size );
}
void DString_Resize( DString *self, daoint size )
{
	daoint i;

	if( self->sharing ) DString_Detach( self, size );
	if( size == self->size && size <= self->bufSize && 2*size >= self->bufSize ) return;

	if( size > self->bufSize || 2*size < self->bufSize ){
		self->bufSize = size;
		DString_Realloc( self, self->bufSize );
	}

	for(i=self->size; i<size; i++) self->bytes[i] = 0;
	self->bytes[ size ] = 0;
	self->size = size;
}
void DString_Clear( DString *self )
{
	int share = self->sharing;
	if( ((int*)self->bytes - self->sharing) == dao_string ) return;
	if( self->sharing ) DString_Detach( self, 0 );
	DString_DeleteData( self );
	DString_Init( self );
	DString_SetSharing( self, share );
}
void DString_Erase( DString *self, daoint start, daoint n )
{
	daoint i, rest;
	if( start >= self->size ) return;
	if( n < 0 ) n = self->size;
	if( n + start > self->size ) n = self->size - start;
	rest = self->size - start - n;
	if( rest ==0 ){
		DString_Resize( self, start );
		return;
	}

	if( self->sharing ) DString_Detach( self, self->size );

	for( i=start; i<start+rest; i++ ) self->bytes[i] = self->bytes[i+n];
	self->bytes[start+rest] = 0;
	self->size -= n;

	if( self->size < 0.5*self->bufSize && self->size+5 < self->bufSize ){
		self->bufSize = (daoint)(0.6 * self->bufSize) + 1;
		DString_Realloc( self, self->bufSize );
	}
}
void DString_InsertChars( DString *self, const char* chs, daoint at, daoint rm, daoint cp )
{
	daoint i;
	if( chs == NULL ) return;
	if( cp <= 0 ) cp = strlen( chs );
	if( at > self->size ) at = self->size;
	if( rm < 0 ) rm = self->size;
	if( rm + at > self->size ) rm = self->size - at;
	if( self->sharing ) DString_Detach( self, self->size + cp - rm );
	if( cp < rm ){
		for( i=at+cp; i<self->size+cp-rm; i++) self->bytes[i] = self->bytes[i+rm-cp];
		DString_Reserve( self, self->size + cp - rm );
	}else if( cp > rm ){
		DString_Reserve( self, self->size + cp - rm );
		for( i=self->size+cp-rm-1; i>=at+cp; i--) self->bytes[i] = self->bytes[i+rm-cp];
	}
	for( i=0; i<cp; i++ ) self->bytes[i+at] = chs[i];
	self->size += cp-rm;
	self->bytes[self->size] = 0;
}
void DString_Insert( DString *self, DString *chs, daoint at, daoint rm, daoint cp )
{
	DString_InsertChars( self, chs->bytes, at, rm, cp );
}
void DString_InsertChar( DString *self, const char ch, daoint at )
{
	char chs[2];
	chs[0] = ch;
	chs[1] = '\0';
	DString_InsertChars( self, chs, at, 0, 1 );
}
void DString_AppendBytes( DString *self, const char *chs, daoint n )
{
	daoint i;
	DString_Reserve( self, self->size + n );
	for( i=0; i<n; i++ ) self->bytes[self->size+i] = chs[i];
	self->size += n;
	self->bytes[ self->size ] = 0;
}
void DString_AppendChars( DString *self, const char *chs )
{
	DString_AppendBytes(self, chs, strlen( chs ));
}
void DString_Append( DString *self, DString *chs )
{
	DString_AppendBytes( self, chs->bytes, chs->size );
}

void DString_AppendChar( DString *self, const char ch )
{
	DString_Reserve( self, self->size + 1 );
	self->bytes[self->size] = ch;
	self->size += 1;
	self->bytes[ self->size ] = 0;
}


void DString_SetBytes( DString *self, const char *bytes, daoint count )
{
	if( count < 0 ){
		DString_SetChars( self, bytes );
	}else{
		DString_Clear( self );
		DString_AppendBytes( self, bytes, count );
	}
}
void DString_Replace( DString *self, DString *chs, daoint start, daoint rm )
{
	DString_Insert( self, chs, start, rm, chs->size );
}
void DString_ReplaceChars( DString *self, const char *chs, daoint start, daoint rm )
{
	DString_InsertChars( self, chs, start, rm, strlen( chs ) );
}
void DString_SubString( DString *self, DString *sub, daoint from, daoint n )
{
	daoint i, size = self->size;
	if( from >= size ){
		DString_Clear( sub );
		return;
	}
	if( n < 0 || n > size ) n = size;
	if( from+n > size ) n = size-from;
	DString_Resize( sub, n );
	for( i=0; i<n; i++) sub->bytes[i] = self->bytes[i+from];
}
/* TODO: better string searching algorithm */
static daoint DMBString_Find( DString *self, daoint S, const char *chs, daoint M )
{
	daoint i, j;

	if( M == 0 ) return DAO_NULLPOS;
	if( M+S > self->size ) return DAO_NULLPOS;
	for( i=S; i<self->size-M+1; i++){
		int found = 1;
		for( j=0; j<M; j++ ){
			if( self->bytes[i+j] != chs[j] ){
				found = 0;
				break;
			}
		}
		if( found ) return i;
	}
	return DAO_NULLPOS;
}
static daoint DMBString_RFind( DString *self, daoint S, const char* chs, daoint M )
{
	daoint i, j;

	if( S < 0 ) S += self->size;
	if( M == 0 || self->size == 0 ) return DAO_NULLPOS;
	if( S >= self->size ) S = self->size-1;
	if( (S+1) < M || M > self->size ) return DAO_NULLPOS;
	for( i=S; i>=M-1; i--){
		int found = 1;
		for( j=0; j<M; j++ ){
			if( self->bytes[i-j] != chs[M-1-j] ){
				found = 0;
				break;
			}
		}
		if( found ) return i;
	}
	return DAO_NULLPOS;
}
daoint DString_FindChars( DString *self, const char *chs, daoint start )
{
	return DMBString_Find( self, start, chs, strlen( chs ) );
}
daoint DString_RFindChars( DString *self, const char *chs, daoint start )
{
	return DMBString_RFind( self, start, chs, strlen( chs ) );
}
daoint DString_Find( DString *self, DString *chs, daoint start )
{
	return DMBString_Find( self, start, chs->bytes, chs->size );
}
daoint DString_RFind( DString *self, DString *chs, daoint start )
{
	return DMBString_RFind( self, start, chs->bytes, chs->size );
}
daoint DString_FindChar( DString *self, char ch, daoint start )
{
	daoint i;
	for(i=start; i<self->size; i++ ) if( self->bytes[i] == ch ) return i;
	return DAO_NULLPOS;
}
daoint DString_RFindChar( DString *self, char ch, daoint start )
{
	daoint i;
	if( self->size ==0 ) return DAO_NULLPOS;
	if( start < 0 || start >= self->size ) start = self->size - 1;
	for(i=start; i >=0; i-- ) if( self->bytes[i] == ch ) return i;
	return DAO_NULLPOS;
}

DString* DString_Copy( DString *self )
{
	DString *str = DString_New();
	DString_Assign( str, self );
	return str;
}
/* Real copying, no implicit sharing here. For thread safty. */
DString* DString_DeepCopy( DString *self )
{
	int share = self->sharing;
	DString *copy = DString_New();
	DString_SetSharing( copy, share );
	DString_Resize( copy, self->size );
	memcpy( copy->bytes, self->bytes, self->size *sizeof(char) );
	return copy;
}
void DString_Assign( DString *self, DString *chs )
{
	int *data1 = (int*)self->bytes - self->sharing;
	int *data2 = (int*)chs->bytes - chs->sharing;
	int assigned = 0;
	if( self == chs ) return;
	if( data1 == data2 ) return;
	//XXX

	if( data2 != dao_string ){
#ifdef DAO_WITH_THREAD
		DMutex_Lock( & mutex_string_sharing );
#endif
		if( self->sharing && chs->sharing ){
			if( data1 != dao_string ){
				data1[0] -= 1;
				if( data1[0] ==0 ) dao_free( data1 );
			}
			*self = *chs;
			data2[0] += 1;
			assigned = 1;
		}else if( data1 == NULL && chs->sharing ){
			*self = *chs;
			data2[0] += 1;
			assigned = 1;
		}
#ifdef DAO_WITH_THREAD
		DMutex_Unlock( & mutex_string_sharing );
#endif

		if( assigned ) return;
	}

	if( self->bytes == NULL ){
		self->size = self->bufSize = chs->size;
		self->bytes = (char*) dao_malloc( (chs->size + 1)*sizeof(char) );
		memcpy( self->bytes, chs->bytes, chs->size*sizeof(char) );
		self->bytes[ self->size ] = 0;
	}else{
		DString_Resize( self, chs->size );
		memcpy( self->bytes, chs->bytes, chs->size*sizeof(char) );
	}
}
int DString_Compare( DString *self, DString *chs )
{
	daoint min = self->size > chs->size ? chs->size : self->size;
	char *p1 = self->bytes;
	char *p2 = chs->bytes;
	char *stop = p1 + min;
	if( p1 == p2 ) return 0;
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
	if( self->size == 0 ) return;
	if( self->sharing ) DString_Detach( self, self->size );
	if( self->bytes ){
		if( self->bytes[ self->size-1 ] == EOF  ) self->bytes[ --self->size ] = 0;
		if( self->bytes[ self->size-1 ] == '\n' ) self->bytes[ --self->size ] = 0;
		if( self->bytes[ self->size-1 ] == '\r' ) self->bytes[ --self->size ] = 0;
	}
}
void DString_Trim( DString *self )
{
	int i, ch;
	if( self->sharing ) DString_Detach( self, self->size );
	if( self->bytes ){
		while( self->size > 0 ){
			ch = self->bytes[ self->size-1 ];
			if( ch == EOF || isspace( ch ) ){
				self->size --;
				self->bytes[ self->size ] = 0;
			}else{
				break;
			}
		}
		for( i=0; i < self->size; i++ ){
			ch = self->bytes[i];
			if( ch != EOF && ! isspace( ch ) ) break;
		}
		DString_Erase( self, 0, i );
	}
}
daoint DString_FindReplace( DString *self, DString *str1, DString *str2, daoint index )
{
	daoint pos, prev = -1, from = 0, count = 0;
	int chsize = sizeof(char);

	if( index == 0 && str1->size == str2->size ){
		void *str2data = (void*) str2->bytes;
		DString_Detach( self, self->size );
		pos = DString_Find( self, str1, from );
		while( pos != DAO_NULLPOS ){
			void *dest = (void*)(self->bytes + pos);
			memcpy( dest, str2data, str2->size * chsize );
			count ++;
			from = pos + str2->size;
			pos = DString_Find( self, str1, from );
		}
	}else if( index == 0 && str1->size > str2->size ){
		void *str2data = (void*) str2->bytes;
		DString_Detach( self, self->size );
		pos = DString_Find( self, str1, from );
		if( pos == DAO_NULLPOS ) pos = self->size;
		while( from < self->size ){
			if( prev >= 0 ){
				void *dest = (void*)(self->bytes + prev);
				void *src  = (void*)(self->bytes + from);
				memmove( dest, src, (pos - from) * chsize );
				prev += pos - from;
			}else{
				prev = pos;
			}
			if( pos < self->size ){
				void *dest = (void*)(self->bytes + prev);
				memcpy( dest, str2data, str2->size * chsize );
				count ++;
			}
			prev += str2->size;
			from = pos + str1->size;
			pos = DString_Find( self, str1, from );
			if( pos == DAO_NULLPOS ) pos = self->size;
		}
		self->size -= count * (str1->size - str2->size);
		self->bytes[self->size] = 0;
	}else if( index == 0 ){
		DString *aux = DString_New();
		DString_Reserve( aux, self->size + (str2->size - str1->size) );
		pos = DString_Find( self, str1, from );
		if( pos == DAO_NULLPOS ) pos = self->size;
		while( from < self->size ){
			count += pos < self->size;
			DString_AppendBytes( aux, self->bytes + from, pos - from );
			if( pos < self->size ) DString_Append( aux, str2 );
			from = pos + str1->size;
			pos = DString_Find( self, str1, from );
			if( pos == DAO_NULLPOS ) pos = self->size;
		}
		DString_Assign( self, aux );
		DString_Delete( aux );
	}else if( index > 0){
		pos = DString_Find( self, str1, from );
		while( pos != DAO_NULLPOS ){
			count ++;
			if( count == index ){
				DString_Insert( self, str2, pos, DString_Size( str1 ), 0 );
				break;
			}
			from = pos + DString_Size( str1 );
			pos = DString_Find( self, str1, from );
		}
		count = 1;
	}else{
		from = DAO_NULLPOS;
		pos = DString_RFind( self, str1, from );
		while( pos != DAO_NULLPOS ){
			count --;
			if( count == index ){
				DString_Insert( self, str2, pos-DString_Size( str1 )+1, DString_Size( str1 ), 0 );
				break;
			}
			from = pos - DString_Size( str1 );
			pos = DString_RFind( self, str1, from );
		}
		count = 1;
	}
	return count;
}
daoint DString_BalancedChar( DString *self, uint_t ch0, uint_t lch0, uint_t rch0,
		uint_t esc0, daoint start, daoint end, int countonly )
{
	daoint size = self->size;
	daoint i, count = 0;
	if( self->bytes ){
		char *src = self->bytes;
		char chr = (char) ch0;
		char lch = (char) lch0;
		char rch = (char) rch0;
		char esc = (char) esc0;
		char c;
		int bc = 0;
		if( ch0 >= 128 || start >= size ) return DAO_NULLPOS;
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
				if( bc <0 ) return DAO_NULLPOS;
			}
		}
	}else{
	}
	if( countonly ) return count;
	return DAO_NULLPOS;
}



static char empty_bytes[] = "";

DString DString_WrapBytes( const char *bytes, int n )
{
	DString str = { NULL, 0, 0, 0, 0 };
	str.bytes = empty_bytes;
	if( bytes == NULL ) return str;
	str.bytes = (char*) bytes;
	str.size = str.bufSize = n;
	return str;
}
DString DString_WrapChars( const char *bytes )
{
	return DString_WrapBytes( bytes, bytes ? strlen( bytes ) : 0 );
}

void DString_AppendPathSep( DString *self )
{
	char last = self->size ? self->bytes[self->size-1] : 0;
	if( last != '/' && last != '\\' ) DString_AppendChar( self, '/' );
}



/*
// UTF-8 Encoding Information:
// High 4 Bits: Encoding Schemes (0: ASCII; 1: Trailing; 2-7: Leading);
// Low  4 Bits: Encoding Lengths (Char Size: Byte number for a code point);
*/
const char utf8_markers[128] =
{
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, /* 00>>1 - 0F>>1 */
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, /* 10>>1 - 1F>>1 */
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, /* 20>>1 - 2F>>1 */
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, /* 30>>1 - 3F>>1 */
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, /* 40>>1 - 4F>>1 */
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, /* 50>>1 - 5F>>1 */
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, /* 60>>1 - 6F>>1 */
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, /* 70>>1 - 7F>>1 */
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, /* 80>>1 - 8F>>1 */
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, /* 90>>1 - 9F>>1 */
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, /* A0>>1 - AF>>1 */
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, /* B0>>1 - BF>>1 */
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, /* C0>>1 - CF>>1 */
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, /* D0>>1 - DF>>1 */
	0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, /* E0>>1 - EF>>1 */
	0x44, 0x44, 0x44, 0x44, 0x55, 0x55, 0x66, 0x77  /* F0>>1 - FF>>1 */
};

#define IsU8Trail( ch )           (((ch) >> 6) == 0x2)
#define IsU8Trail2( c1, c2 )      (((((c1)>>6)<<2)|((c2)>>6)) == 0xA)
#define IsU8Trail3( c1, c2, c3 )  (((((c1)>>6)<<4)|(((c2)>>6)<<2)|((c3)>>6)) == 0x2A)
#define GetU8Trail( ch, shift )   (((uint_t)(ch) & 0x3F) << 6*shift)
#define FormU8Trail( ch, shift )  ((((ch) >> 6*(shift)) & 0x3F) | (0x1 << 7))

#define U8CodeType( ch )          (utf8_markers[(ch)>>1]>>4)
#define U8CharSize( ch )          (utf8_markers[(ch)>>1]&0xF)

int DString_UTF8CharSize( uchar_t ch )
{
	return U8CharSize( ch );
}

void DString_AppendWChar( DString *self, size_t ch )
{
	DString_Reserve( self, self->size + 6 );

	if( ch >= 0x2000000 ) ch = 0xFFFD; /* replacement character; */

	if( ch < 0x80 ){  /* 0xxxxxxx */
		self->bytes[self->size++] = ch;
	}else if( ch < 0x800 ){  /* 110xxxxx 10xxxxxx */
		self->bytes[self->size++] = (char)((ch >> 6) + (0x3 << 6));
		self->bytes[self->size++] = FormU8Trail( ch, 0 );
	}else if( ch < 0x10000 ){   /* 1110xxxx 10xxxxxx 10xxxxxx */
		self->bytes[self->size++] = (char)((ch >> 12) + (0x7 << 5));
		self->bytes[self->size++] = FormU8Trail( ch, 1 );
		self->bytes[self->size++] = FormU8Trail( ch, 0 );
	}else if( ch < 0x200000 ){  /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
		self->bytes[self->size++] = (char)((ch >> 18) + (0xF << 4));
		self->bytes[self->size++] = FormU8Trail( ch, 2 );
		self->bytes[self->size++] = FormU8Trail( ch, 1 );
		self->bytes[self->size++] = FormU8Trail( ch, 0 );
	}
	self->bytes[self->size] = 0;
}

static daoint DString_LocateCurrentChar( DString *self, daoint start )
{
	uchar_t *bytes = (uchar_t*) self->bytes + start;
	daoint k, next, pos = start;
	if( (*bytes >> 7) == 0 ) return start;
	while( pos >= 0 && IsU8Trail( *bytes ) ) bytes--, pos--;
	if( pos < 0 ) return DAO_NULLPOS;
	k = U8CharSize( *bytes ) - 1;
	if( k <= 0 ) return DAO_NULLPOS; /* not a leading byte; */
	next = pos + k;
	if( next < start ) return DAO_NULLPOS; /* too many continuation bytes; */
	if( next >= self->size ) return DAO_NULLPOS; /* not enough continuation bytes; */
	bytes = (uchar_t*) self->bytes + start + 1;
	for(k=start+1; k<=next; ++k, ++bytes) if( IsU8Trail( *bytes ) == 0 ) return DAO_NULLPOS;
	return pos;
}

daoint DString_LocateChar( DString *self, daoint start, daoint count )
{
	uchar_t *bytes = (uchar_t*) self->bytes;
	daoint pos = DString_LocateCurrentChar( self, start );
	while( count < 0 ){  /* backward location: */
		if( pos == DAO_NULLPOS ) return DAO_NULLPOS;
		if( pos == 0 ) return DAO_NULLPOS;
		pos = DString_LocateCurrentChar( self, pos - 1 );
		count += 1;
	}
	while( count > 0 ){  /* forward location: */
		if( pos == DAO_NULLPOS ) return DAO_NULLPOS;
		pos += U8CharSize( bytes[pos] );
		if( pos >= self->size ) return DAO_NULLPOS;
		pos = DString_LocateCurrentChar( self, pos );
		count -= 1;
	}
	return pos;
}

/*
// A string is considered valid if the number of valid UTF-8 bytes
// is greater or equal to 10 times the number of invalid bytes.
*/
int DString_CheckUTF8( DString *self )
{
	uchar_t *chs = (uchar_t*) self->bytes;
	uchar_t *end = chs + self->size;
	daoint valid = 0, invalid = 0;

	while( chs < end ){
		uchar_t ch = *chs;
		int len = U8CharSize( ch );
		if( (chs + len) > end ) goto InvalidByte;
		switch( U8CodeType( ch ) ){
		case 0 : break;
		case 1 : goto InvalidByte;
		case 2 : if( IsU8Trail( chs[1] ) ) break; goto InvalidByte;
		case 3 : if( IsU8Trail2( chs[1], chs[2] ) ) break; goto InvalidByte;
		case 4 : if( IsU8Trail3( chs[1], chs[2], chs[3] ) ) break; goto InvalidByte;
		default: goto InvalidByte;
		}
		valid += len;
		chs += len;
		continue;
InvalidByte:
		invalid += 1;
		chs += 1;
	}
	return valid >= 10*invalid;
}

void DString_ChopUTF8( DString *self )
{
	DString_Detach( self, self->size );
	DString_Chop( self );

	if( self->size == 0 || DString_CheckUTF8( self ) == 0 ) return;
	while( self->size && DString_LocateCurrentChar( self, self->size - 1 ) == DAO_NULLPOS ){
		self->size -= 1;
		self->bytes[self->size] = '\0';
	}
}
void DString_Reverse( DString *self, int utf8 )
{
	DString *aux;
	daoint size = self->size;
	daoint i, front = 0, back = size - 1;
	daoint start = 0, end = 2*size;
	uchar_t *source = (uchar_t*) self->bytes;
	uchar_t *dest;

	if( size <= 1 ) return;
	if( self->sharing ) DString_Detach( self, self->size );

	if( utf8 == 0 ){
		uchar_t *front = source;
		uchar_t *back = source + size - 1;
		while( front < back ){
			uchar_t ch = *front;
			*front = *back;
			*back = ch;
			front ++;
			back --;
		}
		return;
	}

	aux = DString_New();
	DString_Reserve( aux, 2*size );
	dest = (uchar_t*) aux->bytes;
	while( front < back ){
		daoint pos1 = DString_LocateChar( self, front, 1 ); /* next; */
		daoint pos2 = DString_LocateChar( self, back, 0 );  /* current; */
		if( pos1 == DAO_NULLPOS ){
			dest[--end] = source[front++];
		}else{
			int len = U8CharSize( source[pos1] );
			memcpy( dest + end - len, source + pos1, len*sizeof(uchar_t) );
			front += len;
			end -= len;
		}
		if( pos2 == DAO_NULLPOS ){
			dest[start++] = source[back--];
		}else{
			int len = U8CharSize( source[pos2] );
			memcpy( dest + start, source + pos2, len*sizeof(uchar_t) );
			start += len;
			back -= len;
		}
	}
	memcpy( source, dest, start*sizeof(uchar_t) );
	memcpy( source+start, dest+end, (self->size - start)*sizeof(uchar_t) );
	DString_Delete( aux );
}

int DString_DecodeUTF8( DString *self, DVector *wcs )
{
	uint_t *wch;
	wchar_t *wch1, *wch2;
	uchar_t *chs = (uchar_t*) self->bytes;
	uchar_t *end = chs + self->size;
	int ret = 1;

	if( wcs->stride != sizeof(wchar_t) && wcs->stride != 4 ) return 0;

	DVector_Reserve( wcs, self->size );
	while( chs < end ){
		uchar_t ch = *chs;
		daoint cp = ch;
		if( ch>>7 == 0 ){ /* 0xxxxxxx */
			chs += 1;
        }else if ( ch>>5 == 0x6 ){ /* 110xxxxx 10xxxxxx */
			if( (chs+1) >= end || ! IsU8Trail( chs[1] ) ) goto InvalidByte;
			cp = ((uint_t)(ch&0x1F) << 6) + GetU8Trail( chs[1], 0 );
			chs += 2;
        }else if ( ch>>4 == 0xE ){ /* 1110xxxx 10xxxxxx 10xxxxxx */
			if( (chs+2) >= end || ! IsU8Trail2( chs[1], chs[2] ) ) goto InvalidByte;
			cp = ((uint_t)(ch&0xF) << 12) + GetU8Trail( chs[1], 1 ) + GetU8Trail( chs[2], 0 );
			chs += 3;
        }else if ( ch>>3 == 0x1E ){ /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
			if( (chs+3) >= end || ! IsU8Trail3( chs[1], chs[2], chs[3] ) ) goto InvalidByte;
			cp = ((uint_t)(ch&0x7) << 18) + GetU8Trail( chs[1], 2 ) + GetU8Trail( chs[2], 1 );
			cp += GetU8Trail( chs[3], 0 );
            chs += 4;
		}else{
			goto InvalidByte;
		}
        if( wcs->stride == 4 ){ /* UTF-32 */
			wch = (uint_t*) DVector_Push( wcs );
			*wch = cp;
        }else if( cp <= 0xFFFF ){ /* UTF-16, BMP */
			wch1 = (wchar_t*) DVector_Push( wcs );
			*wch1 = cp;
		}else{ /* UTF-16, surrogates */
			cp -= 0x10000;
			wch1 = (wchar_t*) DVector_Push( wcs );
			wch2 = (wchar_t*) DVector_Push( wcs );
			*wch1 = (cp >> 10) + 0xD800;
			*wch2 = (cp & 0x3FF) + 0xDC00;
		}
		continue;
InvalidByte:
		if( wcs->stride == 4 ){ /* UTF-32 */
			wch = (uint_t*) DVector_Push( wcs );
			*wch = 0xFFFD; /* replacement character; */
		}else{
			wch1 = (wchar_t*) DVector_Push( wcs );
			*wch1 = 0xFFFD; /* replacement character; */
		}
		chs += 1;
		ret = 0;
	}
	return ret;
}

static void DMBString_AppendWCS( DString *self, const wchar_t *chs, daoint len );
static void DWCString_AppendMBS( DVector *self, const char *chs, daoint len );

int DString_ImportUTF8( DString *self, DString *utf8 )
{
	DVector *wcs = DVector_New( sizeof(wchar_t) );
	int ret = DString_DecodeUTF8( utf8, wcs );
	DMBString_AppendWCS( self, (const wchar_t*) wcs->data.base, wcs->size );
	DVector_Delete( wcs );
	return ret;
}
int DString_ExportUTF8( DString *self, DString *utf8 )
{
	wchar_t *wchs;
	daoint i;
	DVector *wcs = DVector_New( sizeof(wchar_t) );
	DWCString_AppendMBS( wcs, self->bytes, self->size );
	DString_Reserve( utf8, utf8->size + 3*wcs->size );
	wchs = wcs->data.wchars;
	for(i=0; i<wcs->size; ++i, ++wchs) DString_AppendWChar( utf8, *wchs );
	return 1;
}
int DString_ToLocal( DString *self )
{
	DVector *wcs = DVector_New( sizeof(wchar_t) );
	int ret = DString_DecodeUTF8( self, wcs );
	DString_Reset( self, 0 );
	DMBString_AppendWCS( self, (const wchar_t*) wcs->data.base, wcs->size );
	DVector_Delete( wcs );
	return ret;
}
int DString_ToUTF8( DString *self )
{
	wchar_t *wchs;
	daoint i;
	DVector *wcs = DVector_New( sizeof(wchar_t) );
	DWCString_AppendMBS( wcs, self->bytes, self->size );
	DString_Reset( self, 0 );
	DString_Reserve( self, 3*wcs->size );
	wchs = wcs->data.wchars;
	for(i=0; i<wcs->size; ++i, ++wchs) DString_AppendWChar( self, *wchs );
	return 1;
}

#define MAX_CHAR_PER_WCHAR 7
static void DMBString_AppendWCS( DString *self, const wchar_t *chs, daoint len )
{
	daoint i, smin, more = 2;
	const wchar_t *end = chs + len;
	mbstate_t state;

	for(i=0; i<len; ++i) more += chs[i] < 128 ? 1 : MAX_CHAR_PER_WCHAR;

	DString_Reserve( self, self->size + more );
	if( len == 0 || chs == NULL ) return;
	while( chs < end ){
		const wchar_t *next = chs;
		char *dst = self->bytes + self->size;
		if( *chs == L'\0' ){
			DString_AppendChar( self, '\0' );
			chs += 1;
			continue;
		}
		while( next < end && *next != L'\0' ) next += 1;
		memset( & state, 0, sizeof(mbstate_t) );
		smin = wcsrtombs( dst, (const wchar_t**)& chs, self->bufSize - self->size, & state );
		if( smin > 0 ){
#ifdef WIN32
			/* On Windows, wcsrtombs() returns the number converted char from the source! */
			self->size += strlen( dst );
#else
			self->size += smin;
#endif
		}else if( chs != NULL ){
			chs += 1;
		}
		if( chs == NULL ) chs = next;
	}
	self->bytes[ self->size ] = 0;
	DString_Reset( self, self->size );
}
static void DWCString_AppendMBS( DVector *self, const char *chs, daoint len )
{
	const char *end = chs + len;
	mbstate_t state;
	daoint smin;

	DVector_Reset( self, 0 );
	if( self->stride < sizeof(wchar_t) ) return;
	if( len == 0 || chs == NULL ) return;

	DVector_Reserve( self, len + 2 );
	while( chs < end ){
		const char *next = chs;
		wchar_t *dst = self->data.wchars + self->size;
		if( *chs == '\0' ){
			wchar_t *wch = (wchar_t*) DVector_Push( self );
			*wch = L'\0';
			chs += 1;
			continue;
		}
		while( next < end && *next != '\0' ) next += 1;
		memset( & state, 0, sizeof(mbstate_t) );
		smin = mbsrtowcs( dst, (const char**)& chs, self->capacity - self->size, & state );
		if( smin > 0 ){
#ifdef WIN32
			/* On Windows, mbsrtowcs() returns the number converted char from the source! */
			self->size += wcslen( dst );
#else
			self->size += smin;
#endif
		}else if( chs != NULL ){
			chs += 1;
		}
		if( chs == NULL ) chs = next;
	}
	memset( self->data.wchars + self->size, 0, sizeof(wchar_t) );
}
