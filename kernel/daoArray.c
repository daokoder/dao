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

#include"stdlib.h"
#include"string.h"
#include"assert.h"

#include"daoArray.h"
#include"daoType.h"
#include"daoValue.h"
#include"daoNumtype.h"
#include"daoParser.h"
#include"daoGC.h"

void* dao_malloc( size_t size )
{
	void *p = malloc( size );
	if( size && p == NULL ){
		printf( "memory allocation %lu failed\n", (unsigned long)size );
		exit(1);
	}
	return p;
}
void* dao_calloc( size_t nmemb, size_t size )
{
	void *p = calloc( nmemb, size );
	if( size && p == NULL ){
		printf( "memory allocation %lu failed\n", (unsigned long)( size * nmemb ) );
		exit(1);
	}
	return p;
}
void* dao_realloc( void *ptr, size_t size )
{
	void *p = realloc( ptr, size );
	if( size && p == NULL ){
		printf( "memory allocation %lu failed\n", (unsigned long)size );
		exit(1);
	}
	return p;
}
void  dao_free( void *p )
{
	free( p );
}

int daoCountArray = 0;

DArray* DArray_New( short type )
{
#ifdef DAO_GC_PROF
	daoCountArray ++;
#endif
	DArray *self = (DArray*)dao_malloc( sizeof(DArray) );
	self->items.pVoid = NULL;
	self->size = self->bufsize = 0;
	self->offset = 0;
	self->type = type;
	return self;
}
void DArray_Delete( DArray *self )
{
#ifdef DAO_GC_PROF
	daoCountArray --;
#endif
	DArray_Clear( self );
	dao_free( self );
}

typedef struct DaoToken2{ DaoToken token; DString string; } DaoToken2;

DaoToken* DaoToken_Copy( DaoToken *self )
{
	DaoToken* copy = NULL;
	if( self->string ){
		DaoToken2* copy2 = (DaoToken2*) dao_calloc( 1, sizeof(DaoToken2) );
		copy = (DaoToken*) copy2;
		memcpy( copy, self, sizeof(DaoToken) );
		copy->string = & copy2->string;
		DString_Assign( copy->string, self->string );
	}else{
		copy = (DaoToken*) dao_malloc( sizeof(DaoToken) );
		memcpy( copy, self, sizeof(DaoToken) );
	}
	return copy;
}
static DaoVmCodeX* DaoVmCodeX_Copy( DaoVmCodeX *self )
{
	DaoVmCodeX* copy = (DaoVmCodeX*) dao_malloc( sizeof(DaoVmCodeX) );
	memcpy( copy, self, sizeof(DaoVmCodeX) );
	return copy;
}
static void DaoVmCodeX_Delete( DaoVmCodeX *self )
{
	dao_free( self );
}
static void* DArray_CopyItem( DArray *self, void *item )
{
	DaoValue *v;
	if( item == NULL ) return NULL;
	switch( self->type ){
	case D_VALUE  : v = DaoValue_SimpleCopy( (DaoValue*) item ); GC_IncRC( v ); return v;
	case D_VMCODE : return DaoVmCodeX_Copy( (DaoVmCodeX*) item );
	case D_TOKEN  : return DaoToken_Copy( (DaoToken*) item );
	case D_STRING : return DString_Copy( (DString*) item );
	case D_ARRAY  : return DArray_Copy( (DArray*) item );
	case D_MAP    : return DMap_Copy( (DMap*) item );
	default : break;
	}
	return item;
}
static void DArray_DeleteItem( DArray *self, void *item )
{
	switch( self->type ){
	case D_VALUE  : GC_DecRC( item ); break;
	case D_VMCODE : DaoVmCodeX_Delete( (DaoVmCodeX*) item ); break;
	case D_TOKEN  : DaoToken_Delete( (DaoToken*) item ); break;
	case D_STRING : DString_Delete( (DString*) item ); break;
	case D_ARRAY  : DArray_Delete( (DArray*) item ); break;
	case D_MAP    : DMap_Delete( (DMap*) item ); break;
	default : break;
	}
}
static void DArray_DeleteItems( DArray *self, daoint M, daoint N )
{
	daoint i;
	switch( self->type ){
	case D_VALUE  : for(i=M; i<N; i++) GC_DecRC( self->items.pValue[i] ); break;
	case D_VMCODE : for(i=M; i<N; i++) DaoVmCodeX_Delete( self->items.pVmc[i] ); break;
	case D_TOKEN  : for(i=M; i<N; i++) DaoToken_Delete( self->items.pToken[i] ); break;
	case D_STRING : for(i=M; i<N; i++) DString_Delete( self->items.pString[i] ); break;
	case D_ARRAY  : for(i=M; i<N; i++) DArray_Delete( self->items.pArray[i] ); break;
	case D_MAP    : for(i=M; i<N; i++) DMap_Delete( self->items.pMap[i] ); break;
	default : break;
	}
}
void DArray_Resize( DArray *self, daoint size, void *val )
{
	void **buf = self->items.pVoid - self->offset;
	daoint i;

	if( size == self->size && self->bufsize>0 ) return;
	DArray_DeleteItems( self, size, self->size );

	if( self->offset ){
		daoint min = size > self->size ? self->size : size;
		int locked = self->type == D_VALUE ? DaoGC_LockData( self ) : 0;
		memmove( buf, self->items.pVoid, min*sizeof(void*) );
		self->items.pVoid = buf;
		self->offset = 0;
		if( locked ) DaoGC_UnlockData( self );
	}
	/* When resize() is called, probably this is the intended size,
	 * not to be changed frequently. */
	if( size >= self->bufsize || size < self->bufsize /2 ){
		int locked = self->type == D_VALUE ? DaoGC_LockData( self ) : 0;
		self->bufsize = size;
		self->items.pVoid = (void**) dao_realloc( buf, self->bufsize*sizeof(void*) );
		if( locked ) DaoGC_UnlockData( self );
	}

	if( self->type && val != NULL ){
		for(i=self->size; i<size; i++ ) self->items.pVoid[i] = DArray_CopyItem( self, val );
	}else{
		for(i=self->size; i<size; i++ ) self->items.pVoid[i] = val;
	}
	self->size = size;
}
void DArray_Clear( DArray *self )
{
	void **buf = self->items.pVoid - self->offset;
	DArray_DeleteItems( self, 0, self->size );
	if( buf ) dao_free( buf );
	self->items.pVoid = NULL;
	self->size = self->bufsize = 0;
	self->offset = 0;
}

DArray* DArray_Copy( DArray *self )
{
	DArray *copy = DArray_New( self->type );
	DArray_Assign( copy, self );
	return copy;
}
void DArray_Assign( DArray *left, DArray *right )
{
	daoint i;
	assert( left->type == right->type || (left->type == D_VALUE && right->type == 0) );

	if( left == right ) return;
	if( right->size == 0 ){
		DArray_Clear( left );
		return;
	}
	if( left->type ){
		DArray_Clear( left);
		for( i=0; i<right->size; i++ ) DArray_Append( left, right->items.pVoid[i] );
	}else{
		DArray_Resize( left, right->size, NULL );
		for( i=0; i<right->size; i++ ) left->items.pVoid[i] = right->items.pVoid[i];
	}
}
void DArray_Swap( DArray *left, DArray *right )
{
	daoint tmpSize = left->size;
	daoint tmpBufSize = left->bufsize;
	size_t tmpOffset = left->offset;
	void **tmpItem = left->items.pVoid;
	assert( left->type == right->type );
	assert( left->type != D_VALUE );
	left->size = right->size;
	left->offset = right->offset;
	left->bufsize = right->bufsize;
	left->items.pVoid = right->items.pVoid;
	right->size = tmpSize;
	right->offset = tmpOffset;
	right->bufsize = tmpBufSize;
	right->items.pVoid = tmpItem;
}
void DArray_Insert( DArray *self, void *val, daoint id )
{
	void **buf = self->items.pVoid - self->offset;
	daoint i;
	if( id == 0 ){
		DArray_PushFront( self, val );
		return;
	}else if( id >= self->size ){
		DArray_PushBack( self, val );
		return;
	}
	if( (daoint)(self->offset + self->size + 1) >= self->bufsize ){
		int locked = self->type == D_VALUE ? DaoGC_LockData( self ) : 0;
		if( self->offset > 0 ) memmove( buf, self->items.pVoid, self->size*sizeof(void*) );
		self->bufsize += self->bufsize/5 + 5;
		self->items.pVoid = (void**) dao_realloc( buf, (self->bufsize+1)*sizeof(void*) );
		self->offset = 0;
		if( locked ) DaoGC_UnlockData( self );
	}
	if( self->type && val != NULL ){
		int locked = self->type == D_VALUE ? DaoGC_LockData( self ) : 0;
		for( i=self->size; i>id; i-- ) self->items.pVoid[i] = self->items.pVoid[i-1];
		if( locked ) DaoGC_UnlockData( self );
		self->items.pVoid[ id ] = DArray_CopyItem( self, val );
	}else{
		for( i=self->size; i>id; i-- ) self->items.pVoid[i] = self->items.pVoid[i-1];
		self->items.pVoid[id] = val;
	}
	self->size++;
}
void DArray_InsertArray( DArray *self, daoint at, DArray *array, daoint id, daoint n )
{
	void **buf = self->items.pVoid - self->offset;
	void **objs = array->items.pVoid;
	daoint i;
	assert( self->type == array->type );
	assert( self->type != D_VALUE );
	if( n < 0 ) n = array->size;
	n += id;
	if( n > array->size ) n = array->size;
	if( n ==0 || id >= array->size ) return;
	if( (daoint)(self->offset + self->size + n-id) >= self->bufsize ){
		if( self->offset > 0 ) memmove( buf, self->items.pVoid, self->size*sizeof(void*) );
		self->bufsize += self->bufsize/5 + 1 + ( n - id );
		self->items.pVoid = (void**) dao_realloc( buf, (self->bufsize+1)*sizeof(void*) );
		self->offset = 0;
	}
	if( self->type ){
		if( at >= self->size ){
			for(i=id; i<n; i++) self->items.pVoid[ self->size+i-id ] = DArray_CopyItem( self, objs[i] );
		}else{
			memmove( self->items.pVoid+at+(n-id), self->items.pVoid+at, (self->size-at)*sizeof(void*) );
			for(i=id; i<n; i++) self->items.pVoid[ at+i-id ] = DArray_CopyItem( self, objs[i] );
		}
	}else{
		if( at >= self->size ){
			for(i=id; i<n; i++) self->items.pVoid[ self->size+i-id ] = objs[i];
		}else{
			memmove( self->items.pVoid+at+(n-id), self->items.pVoid+at, (self->size-at)*sizeof(void*) );
			for(i=id; i<n; i++) self->items.pVoid[ at+i-id ] = objs[i];
		}
	}
	self->size += (n-id);
}
void DArray_AppendArray( DArray *self, DArray *array )
{
	DArray_InsertArray( self, self->size, array, 0, array->size );
}
void DArray_Erase( DArray *self, daoint start, daoint n )
{
	void **buf = self->items.pVoid - self->offset;
	daoint rest, locked;
	if( start >= self->size ) return;
	if( n < 0 ) n = self->size;
	if( n > self->size - start ) n = self->size - start;
	if( n == 1 ){
		if( start == 0 ){
			DArray_PopFront( self );
			return;
		}else if( start == self->size -1 ){
			DArray_PopBack( self );
			return;
		}
	}

	DArray_DeleteItems( self, start, start+n );
	rest = self->size - start - n;
	locked = self->type == D_VALUE ? DaoGC_LockData( self ) : 0;
	memmove( self->items.pVoid + start, self->items.pVoid + start + n, rest * sizeof(void*) );
	self->size -= n;
	if( self->size < 0.5*self->bufsize && self->size + 10 < self->bufsize ){
		if( self->offset ) memmove( buf, self->items.pVoid, self->size * sizeof(void*));
		self->bufsize = 0.6 * self->bufsize + 1;
		self->items.pVoid = (void**) dao_realloc( buf, (self->bufsize+1)*sizeof(void*) );
		self->offset = 0;
	}
	if( locked ) DaoGC_UnlockData( self );
}
void DArray_PushFront( DArray *self, void *val )
{
	void **buf = self->items.pVoid - self->offset;
	if( self->offset > 0 ){
		/* make sure the concurrent gc won't access an invalid pointer: */
		self->items.pVoid[-1] = NULL;
		self->items.pVoid --;
	}else{
		size_t moffset = ((size_t)-1)>>4;
		size_t offset = self->bufsize/5 + 5;
		int locked = self->type == D_VALUE ? DaoGC_LockData( self ) : 0;
		self->offset = offset < moffset ? offset : moffset;
		self->bufsize += self->offset;
		buf = (void**) dao_realloc( buf, (self->bufsize+1)*sizeof(void*) );
		memmove( buf + self->offset, buf, self->size*sizeof(void*) );
		self->items.pVoid = buf + self->offset - 1;
		if( locked ) DaoGC_UnlockData( self );
	}
	if( self->type && val != NULL ){
		self->items.pVoid[0] = DArray_CopyItem( self, val );
	}else{
		self->items.pVoid[0] = val;
	}
	self->size ++;
	self->offset --;
}
void* DArray_PopFront( DArray *self )
{
	void *ret, **buf = self->items.pVoid - self->offset;
	size_t moffset = ((size_t)-1)>>4;
	if( self->size == 0 ) return NULL;
	self->size --;
	self->offset ++;
	ret = self->items.pVoid[0];
	if( self->type ) DArray_DeleteItem( self, self->items.pVoid[0] );
	self->items.pVoid ++;
	if( self->offset >= moffset ){
		int locked = self->type == D_VALUE ? DaoGC_LockData( self ) : 0;
		self->offset /= 2;
		memmove( buf + self->offset, self->items.pVoid, self->size*sizeof(void*) );
		self->items.pVoid = buf + self->offset;
		if( locked ) DaoGC_UnlockData( self );
	}else if( self->size < 0.5 * self->bufsize && self->size + 10 < self->bufsize ){
		int locked = self->type == D_VALUE ? DaoGC_LockData( self ) : 0;
		if( self->offset < 0.1 * self->bufsize ){ /* shrink from back */
			self->bufsize = 0.6 * self->bufsize + 1;
		}else{ /* shrink from front */
			self->offset = (size_t)(0.05 * self->bufsize);
			memmove( buf + self->offset, self->items.pVoid, self->size*sizeof(void*) );
		}
		buf = (void**) dao_realloc( buf, (self->bufsize+1)*sizeof(void*) );
		self->items.pVoid = buf + self->offset;
		if( locked ) DaoGC_UnlockData( self );
	}
	if( self->type ) return NULL;
	return ret;
}
void DArray_PushBack( DArray *self, void *val )
{
	void **buf = self->items.pVoid - self->offset;
	if( (daoint)(self->offset + self->size + 1) >= self->bufsize ){
		int locked = self->type == D_VALUE ? DaoGC_LockData( self ) : 0;
		self->bufsize += self->bufsize/5 + 5;
		buf = (void**) dao_realloc( buf, (self->bufsize+1)*sizeof(void*) );
		self->items.pVoid = buf + self->offset;
		if( locked ) DaoGC_UnlockData( self );
	}
	if( self->type && val != NULL ){
		self->items.pVoid[ self->size ] = DArray_CopyItem( self, val );
	}else{
		self->items.pVoid[ self->size ] = val;
	}
	self->size++;
}
void* DArray_PopBack( DArray *self )
{
	void *ret, **buf = self->items.pVoid - self->offset;
	if( self->size == 0 ) return NULL;
	self->size --;
	ret = self->items.pVoid[ self->size ];
	if( self->type ) DArray_DeleteItem( self, self->items.pVoid[ self->size ] );
	if( self->size < 0.5 * self->bufsize && self->size + 10 < self->bufsize ){
		int locked = self->type == D_VALUE ? DaoGC_LockData( self ) : 0;
		if( self->offset < 0.1 * self->bufsize ){ /* shrink from back */
			self->bufsize = 0.6 * self->bufsize + 1;
		}else{ /* shrink from front */
			self->offset = (size_t)(0.05 * self->bufsize);
			memmove( buf + self->offset, self->items.pVoid, self->size*sizeof(void*) );
		}
		buf = (void**) dao_realloc( buf, (self->bufsize+1)*sizeof(void*) );
		self->items.pVoid = buf + self->offset;
		if( locked ) DaoGC_UnlockData( self );
	}
	if( self->type ) return NULL;
	return ret;
}
void  DArray_SetItem( DArray *self, daoint index, void *value )
{
	if( index >= self->size ) return;
	if( self->type && value ){
		self->items.pVoid[ index ] = DArray_CopyItem( self, value );
	}else{
		self->items.pVoid[index] = value;
	}
}
void* DArray_GetItem( DArray *self, daoint index )
{
	if( index >= self->size ) return NULL;
	return self->items.pVoid[ index ];
}
void* DArray_Front( DArray *self )
{
	return DArray_GetItem( self, 0 );
}
void* DArray_Back( DArray *self )
{
	if( self->size ==0 ) return NULL;
	return DArray_GetItem( self, self->size-1 );
}
void** DArray_GetBuffer( DArray *self )
{
	return self->items.pVoid;
}



DaoVmcArray* DaoVmcArray_New()
{
	DaoVmcArray *self = (DaoVmcArray*)dao_malloc( sizeof(DaoVmcArray) );
	self->buf = self->codes = NULL;
	self->size = self->bufsize = 0;
#ifdef DAO_GC_PROF
	daoCountArray ++;
#endif
	return self;
}
void DaoVmcArray_Delete( DaoVmcArray *self )
{
	DaoVmcArray_Clear( self );
	dao_free( self );
#ifdef DAO_GC_PROF
	daoCountArray --;
#endif
}
void DaoVmcArray_Clear( DaoVmcArray *self )
{
	if( self->buf ) dao_free( self->buf );
	self->buf = self->codes = NULL;
	self->size = self->bufsize = 0;
}
void DaoVmcArray_PushBack( DaoVmcArray *self, DaoVmCode code )
{
	daoint from = (daoint)( self->codes - self->buf );
	if( from + self->size + 1 >= self->bufsize ){
		self->bufsize += self->bufsize/5 + 5;
		self->buf = (DaoVmCode*) dao_realloc( self->buf, (self->bufsize+1)*sizeof(DaoVmCode) );
		self->codes = self->buf + from;
	}
	self->codes[ self->size ] = code;
	self->size++;
}
void DaoVmcArray_Resize( DaoVmcArray *self, int size )
{
	if( size >= self->bufsize || size < self->bufsize /2 ){
		if( self->codes > self->buf ){
			daoint min = size > self->size ? self->size : size;
			memmove( self->buf, self->codes, min*sizeof(DaoVmCode) );
		}
		self->bufsize = size + 1;
		self->buf = (DaoVmCode*) dao_realloc( self->buf, (self->bufsize+1)*sizeof(DaoVmCode) );
		self->codes = self->buf;
	}
	if( size > self->size )
		memset( self->codes + self->size, 0, (size - self->size) * sizeof(DaoVmCode) );
	self->size = size;
}
void DaoVmcArray_Assign( DaoVmcArray *left, DaoVmcArray *right )
{
	DaoVmcArray_Resize( left, right->size );
	memcpy( left->codes, right->codes, right->size * sizeof(DaoVmCode) );
}

