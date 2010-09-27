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

#include"stdlib.h"
#include"string.h"
#include"assert.h"

#include"daoArray.h"
#include"daoType.h"
#include"daoNumtype.h"
#include"daoContext.h"
#include"daoParser.h"

void* dao_malloc( size_t size )
{
	void *p = malloc( size );
	if( size && p == NULL ){
		printf( "memory allocation %li failed\n", size );
		exit(1);
	}
	return p;
}
void* dao_calloc( size_t nmemb, size_t size )
{
	void *p = calloc( nmemb, size );
	if( size && p == NULL ){
		printf( "memory allocation %li failed\n", size * nmemb );
		exit(1);
	}
	return p;
}
void* dao_realloc( void *ptr, size_t size )
{
	void *p = realloc( ptr, size );
	if( size && p == NULL ){
		printf( "memory allocation %li failed\n", size );
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
	self->buf.pVoid = self->items.pVoid = NULL;
	self->size = self->bufsize = 0;
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

static DaoToken* DaoToken_Copy( DaoToken *self )
{
	DaoToken* copy = NULL;
	if( self->string ){
		DaoToken2* copy2 = dao_calloc( 1, sizeof(DaoToken2) );
		copy = (DaoToken*) copy2;
		memcpy( copy, self, sizeof(DaoToken) );
		copy->string = & copy2->string;
		DString_Assign( copy->string, self->string );
	}else{
		copy = dao_malloc( sizeof(DaoToken) );
		memcpy( copy, self, sizeof(DaoToken) );
	}
	return copy;
}
static DaoVmCodeX* DaoVmCodeX_Copy( DaoVmCodeX *self )
{
	DaoVmCodeX* copy = dao_malloc( sizeof(DaoVmCodeX) );
	memcpy( copy, self, sizeof(DaoVmCodeX) );
	return copy;
}
static void DaoVmCodeX_Delete( DaoVmCodeX *self )
{
	dao_free( self );
}
static DaoJitCode* DaoJitCode_Copy( DaoJitCode *self )
{
	DaoJitCode* copy = dao_malloc( sizeof(DaoJitCode) );
	memcpy( copy, self, sizeof(DaoJitCode) );
	return copy;
}
static DVarray* DVarray_Copy( DVarray *self );
static void* DArray_CopyItem( DArray *self, void *item )
{
	switch( self->type ){
	case D_VMCODE : return DaoVmCodeX_Copy( (DaoVmCodeX*) item );
	case D_TOKEN  : return DaoToken_Copy( (DaoToken*) item );
	case D_JITCODE : return DaoJitCode_Copy( (DaoJitCode*) item );
	case D_STRING : return DString_Copy( (DString*) item );
	case D_VARRAY : return DVarray_Copy( (DVarray*) item );
	case D_ARRAY  : return DArray_Copy( (DArray*) item );
	case D_MAP    : return DMap_Copy( (DMap*) item );
	default : break;
	}
	return item;
}
static void DArray_DeleteItem( DArray *self, void *item )
{
	switch( self->type ){
	case D_VMCODE : DaoVmCodeX_Delete( (DaoVmCodeX*) item ); break;
	case D_JITCODE : dao_free( item ); break;
	case D_TOKEN : DaoToken_Delete( (DaoToken*) item ); break;
	case D_STRING : DString_Delete( (DString*) item ); break;
	case D_VARRAY : DVarray_Delete( (DVarray*) item ); break;
	case D_ARRAY  : DArray_Delete( (DArray*) item ); break;
	case D_MAP    : DMap_Delete( (DMap*) item ); break;
	default : break;
	}
}
static void DArray_DeleteItems( DArray *self, size_t M, size_t N )
{
	size_t i;
	switch( self->type ){
	case D_VMCODE : for(i=M; i<N; i++) DaoVmCodeX_Delete( self->items.pVmc[i] ); break;
	case D_TOKEN  : for(i=M; i<N; i++) DaoToken_Delete( self->items.pToken[i] ); break;
	case D_JITCODE: for(i=M; i<N; i++) dao_free( self->items.pJitc[i] ); break;
	case D_STRING : for(i=M; i<N; i++) DString_Delete( self->items.pString[i] ); break;
	case D_VARRAY : for(i=M; i<N; i++) DVarray_Delete( self->items.pVarray[i] ); break;
	case D_ARRAY  : for(i=M; i<N; i++) DArray_Delete( self->items.pArray[i] ); break;
	case D_MAP    : for(i=M; i<N; i++) DMap_Delete( self->items.pMap[i] ); break;
	default : break;
	}
}
void DArray_Resize( DArray *self, size_t size, void *val )
{
	size_t i;
	if( size == self->size && self->bufsize>0 ) return;

	DArray_DeleteItems( self, size, self->size );

	if( self->items.pVoid > self->buf.pVoid ){
		size_t min = size > self->size ? self->size : size;
		memmove( self->buf.pVoid, self->items.pVoid, min*sizeof(void*) );
		self->items.pVoid = self->buf.pVoid;
	}
	/* When resize() is called, probably this is the intended size,
	 * not to be changed frequently. */
	if( size >= self->bufsize || size < self->bufsize /2 ){
		self->bufsize = size;
		self->buf.pVoid = dao_realloc( self->buf.pVoid, self->bufsize*sizeof(void*) );
		self->items.pVoid = self->buf.pVoid;
	}

	if( self->type && val != NULL ){
		for(i=self->size; i<size; i++ ) DArray_CopyItem( self, val );
	}else{
		for(i=self->size; i<size; i++ ) self->items.pVoid[i] = val;
	}
	self->size = size;
}
void DArray_Clear( DArray *self )
{
	DArray_DeleteItems( self, 0, self->size );
	if( self->buf.pVoid ) dao_free( self->buf.pVoid );
	self->buf.pVoid = self->items.pVoid = NULL;
	self->size = self->bufsize = 0;
}

DArray* DArray_Copy( DArray *self )
{
	DArray *copy = DArray_New( self->type );
	DArray_Assign( copy, self );
	return copy;
}
void DArray_Assign( DArray *left, DArray *right )
{
	size_t i;
	assert( left->type == right->type );

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
	size_t tmpSize = left->size;
	size_t tmpBufSize = left->bufsize;
	void *tmpBuf = left->buf.pVoid;
	void *tmpItem = left->items.pVoid;
	assert( left->type == right->type );
	left->size = right->size;
	left->bufsize = right->bufsize;
	left->buf.pVoid = right->buf.pVoid;
	left->items.pVoid = right->items.pVoid;
	right->size = tmpSize;
	right->bufsize = tmpBufSize;
	right->buf.pVoid = tmpBuf;
	right->items.pVoid = tmpItem;
}
void DArray_Insert( DArray *self, void *val, size_t id )
{
	size_t from = (size_t)( self->items.pVoid - self->buf.pVoid );
	size_t i;
	if( id == 0 ){
		DArray_PushFront( self, val );
		return;
	}else if( id >= self->size ){
		DArray_PushBack( self, val );
		return;
	}
	if( from + self->size + 1 >= self->bufsize ){
		if( from > 0 ) memmove( self->buf.pVoid, self->items.pVoid, self->size*sizeof(void*) );
		self->bufsize += self->bufsize/5 + 5;
		self->buf.pVoid = dao_realloc( self->buf.pVoid, (self->bufsize+1)*sizeof(void*) );
		self->items.pVoid = self->buf.pVoid;
	}
	if( self->type && val != NULL ){
		for( i=self->size; i>id; i-- ) self->items.pVoid[i] = self->items.pVoid[i-1];
		self->items.pVoid[ id ] = DArray_CopyItem( self, val );
	}else{
		for( i=self->size; i>id; i-- ) self->items.pVoid[i] = self->items.pVoid[i-1];
		self->items.pVoid[id] = val;
	}
	self->size++;
}
void DArray_InsertArray( DArray *self, size_t at, DArray *array, size_t id, size_t n )
{
	size_t from = (size_t)( self->items.pVoid - self->buf.pVoid );
	void **objs = array->items.pVoid;
	size_t i;
	assert( self->type == array->type );
	n += id;
	if( n > array->size ) n = array->size;
	if( n ==0 || id >= array->size ) return;
	if( from + self->size + n-id >= self->bufsize ){
		if( from > 0 ) memmove( self->buf.pVoid, self->items.pVoid, self->size*sizeof(void*) );

		self->bufsize += self->bufsize/5 + 1 + ( n - id );
		self->buf.pVoid = dao_realloc( self->buf.pVoid, (self->bufsize+1)*sizeof(void*) );
		self->items.pVoid = self->buf.pVoid;
	}
	if( self->type ){
		if( at >= self->size ){
			for(i=id; i<n; i++)
				self->items.pVoid[ self->size+i-id ] = DArray_CopyItem( self, objs[i] );
		}else{
			memmove( self->items.pVoid+at+(n-id), self->items.pVoid+at, 
					(self->size-at)*sizeof(void*) );
			for(i=id; i<n; i++) self->items.pVoid[ at+i-id ] = DArray_CopyItem( self, objs[i] );
		}
	}else{
		if( at >= self->size ){
			for(i=id; i<n; i++)
				self->items.pVoid[ self->size+i-id ] = objs[i];
		}else{
			memmove( self->items.pVoid+at+(n-id), self->items.pVoid+at, 
					(self->size-at)*sizeof(void*) );
			for(i=id; i<n; i++) self->items.pVoid[ at+i-id ] = objs[i];
		}
	}
	self->size += (n-id);
}
void DArray_AppendArray( DArray *self, DArray *array )
{
	DArray_InsertArray( self, self->size, array, 0, array->size );
}
void DArray_Erase( DArray *self, size_t start, size_t n )
{
	size_t rest;
	if( start >= self->size ) return;
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
	memmove( self->items.pVoid + start, self->items.pVoid + start + n, rest * sizeof(void*) );
	self->size -= n;
	if( self->size < 0.5*self->bufsize && self->size + 10 < self->bufsize ){
		if( self->items.pVoid >self->buf.pVoid )
			memmove( self->buf.pVoid, self->items.pVoid, self->size * sizeof(void*));
		self->bufsize = (size_t)(0.6 * self->bufsize)+1;
		self->buf.pVoid = dao_realloc( self->buf.pVoid, (self->bufsize+1)*sizeof(void*) );
		self->items.pVoid = self->buf.pVoid;
	}

}
void DArray_PushFront( DArray *self, void *val )
{
	size_t from = (size_t)( self->items.pVoid - self->buf.pVoid );
	if( from > 0 ){
		self->items.pVoid --;
	}else{
		from = self->bufsize/5 + 5;
		self->bufsize += from;
		self->buf.pVoid = dao_realloc( self->buf.pVoid, (self->bufsize+1)*sizeof(void*) );
		memmove( self->buf.pVoid + from, self->buf.pVoid, self->size*sizeof(void*) );
		self->items.pVoid = self->buf.pVoid + from - 1;
	}
	if( self->type && val != NULL ){
		self->items.pVoid[0] = DArray_CopyItem( self, val );
	}else{
		self->items.pVoid[0] = val;
	}
	self->size ++;
}
void DArray_PopFront( DArray *self )
{
	size_t from;
	if( self->size == 0 ) return;
	self->size --;
	if( self->type ) DArray_DeleteItem( self, self->items.pVoid[0] );
	self->items.pVoid ++;
	from = (size_t)( self->items.pVoid - self->buf.pVoid );
	if( self->size < 0.5 * self->bufsize && self->size + 10 < self->bufsize ){
		if( from < 0.1 * self->bufsize ){ /* shrink from back */
			self->bufsize = (size_t)(0.6 * self->bufsize)+1;
		}else{ /* shrink from front */
			from = (size_t)(0.05 * self->bufsize);
			memmove( self->buf.pVoid+from, self->items.pVoid, self->size*sizeof(void*) );
		}
		self->buf.pVoid = dao_realloc( self->buf.pVoid, (self->bufsize+1)*sizeof(void*) );
		self->items.pVoid = self->buf.pVoid + from;
	}
}
void DArray_PushBack( DArray *self, void *val )
{
	size_t from = (size_t)( self->items.pVoid - self->buf.pVoid );
	if( from + self->size + 1 >= self->bufsize ){
		self->bufsize += self->bufsize/5 + 5;
		self->buf.pVoid = dao_realloc( self->buf.pVoid, (self->bufsize+1)*sizeof(void*) );
		self->items.pVoid = self->buf.pVoid + from;
	}
	if( self->type && val != NULL ){
		self->items.pVoid[ self->size ] = DArray_CopyItem( self, val );
	}else{
		self->items.pVoid[ self->size ] = val;
	}
	self->size++;
}
void DArray_PopBack( DArray *self )
{
	size_t from;
	if( self->size == 0 ) return;
	self->size --;
	if( self->type ) DArray_DeleteItem( self, self->items.pVoid[ self->size ] );
	from = (size_t)( self->items.pVoid - self->buf.pVoid );
	if( self->size < 0.5 * self->bufsize && self->size + 10 < self->bufsize ){
		if( from < 0.1 * self->bufsize ){ /* shrink from back */
			self->bufsize = (size_t)(0.6 * self->bufsize)+1;
		}else{ /* shrink from front */
			from = (size_t)(0.05 * self->bufsize);
			memmove( self->buf.pVoid+from, self->items.pVoid, self->size*sizeof(void*) );
		}
		self->buf.pVoid = dao_realloc( self->buf.pVoid, (self->bufsize+1)*sizeof(void*) );
		self->items.pVoid = self->buf.pVoid + from;
	}
}
void  DArray_SetItem( DArray *self, size_t index, void *value )
{
	if( index >= self->size ) return;
	if( self->type && value ){
		self->items.pVoid[ index ] = DArray_CopyItem( self, value );
	}else{
		self->items.pVoid[index] = value;
	}
}
void* DArray_GetItem( DArray *self, size_t index )
{
	if( index >= self->size ) return NULL;
	/*
	 * XXX
	 if( self->type && self->items.pVoid[ index ] ){
	 return DArray_CopyItem( self, self->items.pVoid[ index ] );
	 }
	 */
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



DVarray* DVarray_New()
{
#ifdef DAO_GC_PROF
	daoCountArray ++;
#endif
	DVarray *self = (DVarray*)dao_malloc( sizeof(DVarray) );
	self->buf = self->data = NULL;
	self->size = self->bufsize = 0;
	return self;
}
void DVarray_Delete( DVarray *self )
{
#ifdef DAO_GC_PROF
	daoCountArray --;
#endif
	DVarray_Clear( self );
	dao_free( self );
}

void DVarray_Resize( DVarray *self, size_t size, DValue val )
{
	DValue nil = daoNullValue;
	size_t i;
	if( size == self->size && self->bufsize>0 ) return;

	for(i=size; i<self->size; i++ ) DValue_Clear( self->data + i );

	/* When resize() is called, probably this is the intended size,
	 * not to be changed frequently. */
	if( size > self->bufsize || size < self->bufsize /2 ){
		if( self->data > self->buf ){
			size_t min = size > self->size ? self->size : size;
			memmove( self->buf, self->data, min*sizeof(DValue) );
		}
		self->bufsize = size;
		self->buf = dao_realloc( self->buf, self->bufsize*sizeof(DValue) );
		self->data = self->buf;
	}

	if( val.t <= DAO_DOUBLE ){
		for(i=self->size; i<size; i++ ) self->data[i] = val;
	}else{
		for(i=self->size; i<size; i++ ){
			self->data[i] = nil;
			DValue_Copy( self->data + i, val );
		}
	}
	self->size = size;
}
void DVarray_Clear( DVarray *self )
{
	size_t i;
	for(i=0; i<self->size; i++ ) DValue_Clear( self->data + i );
	if( self->buf ) dao_free( self->buf );
	self->buf = self->data = NULL;
	self->size = self->bufsize = 0;
}
/* for array of int, float and double only */
void DVarray_FastClear( DVarray *self )
{
	if( self->buf ) dao_free( self->buf );
	self->buf = self->data = NULL;
	self->size = self->bufsize = 0;
}
void DVarray_Assign( DVarray *left, DVarray *right )
{
	size_t i;

	if( right->size == 0 ){
		DVarray_Clear( left );
		return;
	}
	DVarray_Resize( left, right->size, daoNullValue );
	for( i=0; i<right->size; i++ ) DValue_CopyExt( left->data + i, right->data[i], 0 );
}
DVarray* DVarray_Copy( DVarray *self )
{
	DVarray *copy = DVarray_New();
	DVarray_Assign( copy, self );
	return copy;
}
void DVarray_Swap( DVarray *left, DVarray *right )
{
	size_t tmpSize = left->size;
	size_t tmpBufSize = left->bufsize;
	void *tmpBuf = left->buf;
	void *tmpItem = left->data;
	left->size = right->size;
	left->bufsize = right->bufsize;
	left->buf = right->buf;
	left->data = right->data;
	right->size = tmpSize;
	right->bufsize = tmpBufSize;
	right->buf = tmpBuf;
	right->data = tmpItem;
}

void DVarray_Insert( DVarray *self, DValue val, size_t id )
{
	size_t from = (size_t)( self->data - self->buf );
	size_t i;
	if( id == 0 ){
		DVarray_PushFront( self, val );
		return;
	}else if( id >= self->size ){
		DVarray_PushBack( self, val );
		return;
	}
	if( from + self->size + 1 >= self->bufsize ){
		if( from > 0 ) memmove( self->buf, self->data, self->size*sizeof(DValue) );

		self->bufsize += self->bufsize/5 + 5;
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DValue) );
		self->data = self->buf;
	}
	for( i=self->size; i>id; i-- ) self->data[i] = self->data[i-1];
	self->data[id] = daoNullValue;
	DValue_Copy( self->data + id, val );
	self->size++;
}
void DVarray_Erase( DVarray *self, size_t start, size_t n )
{
	size_t i, rest;
	if( start > self->size ) return;
	if( n > self->size-start ) n = self->size-start;
	if( n == 1 ){
		if( start == 0 ){
			DVarray_PopFront( self );
			return;
		}else if( start == self->size -1 ){
			DVarray_PopBack( self );
			return;
		}
	}
	for( i=start; i<start+n; i++ ) DValue_Clear( self->data + i );

	rest = self->size - start - n;
	memmove( self->data + start, self->data + start + n, rest * sizeof(DValue) );
	self->size -= n;
	if( self->size < 0.5*self->bufsize && self->size + 10 < self->bufsize ){
		if( self->data >self->buf ) memmove( self->buf, self->data, self->size * sizeof(DValue));
		self->bufsize = (size_t)(0.6 * self->bufsize)+1;
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DValue) );
		self->data = self->buf;
	}

}
void DVarray_PushFront( DVarray *self, DValue val )
{
	size_t from = (size_t)( self->data - self->buf );
	if( from > 0 ){
		self->data --;
	}else{
		from = self->bufsize/5 + 5;
		self->bufsize += from;
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DValue) );
		memmove( self->buf + from, self->buf, self->size*sizeof(DValue) );
		self->data = self->buf + from - 1;
	}
	self->data[0] = daoNullValue;
	DValue_Copy( self->data, val );
	self->size ++;
}
void DVarray_PopFront( DVarray *self )
{
	size_t from;
	if( self->size == 0 ) return;
	DValue_Clear( self->data );
	self->size --;
	self->data ++;
	from = (size_t)( self->data - self->buf );
	if( self->size < 0.5 * self->bufsize && self->size + 10 < self->bufsize ){
		if( from < 0.1 * self->bufsize ){ /* shrink from back */
			self->bufsize = (size_t)(0.6 * self->bufsize)+1;
		}else{ /* shrink from front */
			from = (size_t)(0.05 * self->bufsize);
			memmove( self->buf+from, self->data, self->size*sizeof(DValue) );
		}
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DValue) );
		self->data = self->buf + from;
	}
}
void DVarray_PushBack( DVarray *self, DValue val )
{
	size_t from = (size_t)( self->data - self->buf );
	if( from + self->size + 1 >= self->bufsize ){
		self->bufsize += self->bufsize/5 + 5;
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DValue) );
		self->data = self->buf + from;
	}
	self->data[ self->size ] = daoNullValue;
	DValue_Copy( self->data + self->size, val );
	self->size++;
}
void DVarray_PopBack( DVarray *self )
{
	size_t from;
	if( self->size == 0 ) return;
	self->size --;
	DValue_Clear( self->data + self->size );
	from = (size_t)( self->data - self->buf );
	if( self->size < 0.5 * self->bufsize && self->size + 10 < self->bufsize ){
		if( from < 0.1 * self->bufsize ){ /* shrink from back */
			self->bufsize = (size_t)(0.6 * self->bufsize)+1;
		}else{ /* shrink from front */
			from = (size_t)(0.05 * self->bufsize);
			memmove( self->buf+from, self->data, self->size*sizeof(DValue) );
		}
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DValue) );
		self->data = self->buf + from;
	}
}
DValue DVarray_Front( DVarray *self )
{
	DValue v = daoNullValue;
	if( self->size ==0 || self->data ==NULL ) return v;
	return self->data[0];
}
DValue DVarray_Back( DVarray *self )
{
	DValue v = daoNullValue;
	if( self->size ==0 || self->data ==NULL ) return v;
	return self->data[ self->size -1 ];
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
void DaoVmcArray_PushFront( DaoVmcArray *self, DaoVmCode code )
{
	size_t from = (size_t)( self->codes - self->buf );
	if( from > 0 ){
		self->codes --;
	}else{
		from = self->bufsize/5 + 5;
		self->bufsize += from;
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DaoVmCode) );
		memmove( self->buf + from, self->buf, self->size*sizeof(DaoVmCode) );
		self->codes = self->buf + from - 1;
	}
	self->codes[0] = code;
	self->size ++;
}
void DaoVmcArray_PushBack( DaoVmcArray *self, DaoVmCode code )
{
	size_t from = (size_t)( self->codes - self->buf );
	if( from + self->size + 1 >= self->bufsize ){
		self->bufsize += self->bufsize/5 + 5;
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DaoVmCode) );
		self->codes = self->buf + from;
	}
	self->codes[ self->size ] = code;
	self->size++;
}
void DaoVmcArray_PopFront( DaoVmcArray *self )
{
	size_t from;
	if( self->size == 0 ) return;
	self->size --;
	self->codes ++;
	from = (size_t)( self->codes - self->buf );
	if( self->size < 0.5 * self->bufsize && self->size + 10 < self->bufsize ){
		if( from < 0.1 * self->bufsize ){ /* shrink from back */
			self->bufsize = (size_t)(0.6 * self->bufsize)+1;
		}else{ /* shrink from front */
			from = (size_t)(0.05 * self->bufsize);
			memmove( self->buf+from, self->codes, self->size*sizeof(DaoVmCode) );
		}
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DaoVmCode) );
		self->codes = self->buf + from;
	}
}
void DaoVmcArray_PopBack( DaoVmcArray *self )
{
	size_t from;
	if( self->size == 0 ) return;
	self->size --;
	from = (size_t)( self->codes - self->buf );
	if( self->size < 0.5 * self->bufsize && self->size + 10 < self->bufsize ){
		if( from < 0.1 * self->bufsize ){ /* shrink from back */
			self->bufsize = (size_t)(0.6 * self->bufsize)+1;
		}else{ /* shrink from front */
			from = (size_t)(0.05 * self->bufsize);
			memmove( self->buf+from, self->codes, self->size*sizeof(DaoVmCode) );
		}
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DaoVmCode) );
		self->codes = self->buf + from;
	}
}
void DaoVmcArray_InsertArray( DaoVmcArray *self, size_t at, 
		DaoVmcArray *array, size_t id, size_t n )
{
	DaoVmCode *codes = array->codes;
	size_t from = (size_t)( self->codes - self->buf );
	size_t i;
	n += id;
	if( n > array->size ) n = array->size;
	if( n ==0 || id >= array->size ) return;
	if( from + self->size + n-id >= self->bufsize ){
		if( from > 0 ) memmove( self->buf, self->codes, self->size*sizeof(DaoVmCode) );

		self->bufsize += self->bufsize/5 + 1 + ( n - id );
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DaoVmCode) );
		self->codes = self->buf;
	}
	if( at >= self->size ){
		for(i=id; i<n; i++)
			self->codes[ self->size+i-id ] = codes[i];
	}else{
		memmove( self->codes+at+(n-id), self->codes+at, (self->size-at)*sizeof(DaoVmCode) );
		for(i=id; i<n; i++) self->codes[ at+i-id ] = codes[i];
	}
	self->size += (n-id);
}
void DaoVmcArray_Erase( DaoVmcArray *self, size_t start, size_t n )
{
	size_t rest;
	if( start >= self->size ) return;
	if( n > self->size - start ) n = self->size-start;
	if( n == 1 ){
		if( start == 0 ){
			DaoVmcArray_PopFront( self );
			return;
		}else if( start == self->size -1 ){
			DaoVmcArray_PopBack( self );
			return;
		}
	}
	rest = self->size - start - n;
	memmove( self->codes + start, self->codes + start + n, rest * sizeof(DaoVmCode) );
	self->size -= n;
	if( self->size < 0.5*self->bufsize && self->size + 10 < self->bufsize ){
		if( self->codes >self->buf )
			memmove( self->buf, self->codes, self->size * sizeof(DaoVmCode));
		self->bufsize = (size_t)(0.6 * self->bufsize)+1;
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DaoVmCode) );
		self->codes = self->buf;
	}

}
void DaoVmcArray_Swap( DaoVmcArray *left, DaoVmcArray *right )
{
	size_t tmpSize = left->size;
	size_t tmpBufSize = left->bufsize;
	DaoVmCode *tmpBuf = left->buf;
	DaoVmCode *tmpItem = left->codes;
	left->size = right->size;
	left->bufsize = right->bufsize;
	left->buf = right->buf;
	left->codes = right->codes;
	right->size = tmpSize;
	right->bufsize = tmpBufSize;
	right->buf = tmpBuf;
	right->codes = tmpItem;
}
void DaoVmcArray_Resize( DaoVmcArray *self, int size )
{
	if( size >= self->bufsize || size < self->bufsize /2 ){
		if( self->codes > self->buf ){
			size_t min = size > self->size ? self->size : size;
			memmove( self->buf, self->codes, min*sizeof(DaoVmCode) );
		}
		self->bufsize = size + 1;
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DaoVmCode) );
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
void DaoVmcArray_Insert( DaoVmcArray *self, DaoVmCode code, size_t pos )
{
	DaoVmCode *vmc;
	int i;
	if( self->size == self->bufsize ){
		self->bufsize += self->size / 10 + 1;
		self->buf = dao_realloc( self->buf, (self->bufsize+1)*sizeof(DaoVmCode) );
		self->codes = self->buf;
	}
	if( pos < self->size ){
		memmove( self->codes + pos + 1, self->codes + pos, 
				(self->size-pos)*sizeof(DaoVmCode) );
	}
	self->codes[ pos ] = code;
	self->size ++;
	for(i=0; i<self->size; i++){
		if( i == pos ) continue;
		vmc = self->codes + i;
		switch( vmc->code ){
		case DVM_GOTO : case DVM_TEST : case DVM_TEST_I :
		case DVM_TEST_F : case DVM_TEST_D : case DVM_SAFE_GOTO :
		case DVM_SWITCH : case DVM_CASE :
			if( vmc->b >= pos ) vmc->b ++;
			break;
		case DVM_JITC :
			if( vmc->b + i >= pos ) vmc->b ++;
			break;
		case DVM_CRRE :
			if( vmc->c && vmc->c >= pos ) vmc->c ++;
			break;
		default : break;
		}
	}
}
void DaoVmcArray_Cleanup( DaoVmcArray *self )
{
	DaoVmCode *vmc;
	DArray *dels = DArray_New(0);
	int i, j, k, M = 0, N = self->size;
	for(i=0; i<N; i++){
		vmc = self->codes + i;
		if( vmc->code >= DVM_UNUSED ) DArray_Append( dels, i );
	}
	if( dels->size ==0 ){
		DArray_Delete( dels );
		return;
	}
	for(i=0; i<N; i++){
		vmc = self->codes + i;
		switch( vmc->code ){
		case DVM_GOTO : case DVM_TEST : case DVM_TEST_I :
		case DVM_TEST_F : case DVM_TEST_D : case DVM_SAFE_GOTO :
		case DVM_SWITCH : case DVM_CASE :
		case DVM_JITC :
		case DVM_CRRE :
			j = vmc->b;
			if( vmc->code == DVM_CRRE ){
				j = vmc->c;
			}else if( vmc->code == DVM_JITC ){
				j = vmc->b + i;
			}
			for(k=0; k<dels->size; k++){
				if( dels->items.pInt[k] > j ) break; 
			}
			if( vmc->code == DVM_CRRE ){
				if( vmc->c ) vmc->c -= k;
			}else if( vmc->code == DVM_JITC ){
				vmc->b = (vmc->b + i - k) - M;
			}else{
				vmc->b -= k;
			}
			break;
		default : break;
		}
		if( vmc->code != DVM_UNUSED ){
			self->codes[M] = *vmc;
			M += 1;
		}
	}
	self->size = M;
	DArray_Delete( dels );
}
void DArray_CleanupCodes( DArray *self )
{
	DaoVmCodeX *vmc;
	DArray *dels;
	int i, j, k, M = 0, N = self->size;

	if( self->type != D_VMCODE ) return;
	dels = DArray_New(0);
	for(i=0; i<N; i++){
		vmc = self->items.pVmc[i];
		if( vmc->code >= DVM_UNUSED ) DArray_Append( dels, i );
	}
	if( dels->size ==0 ){
		DArray_Delete( dels );
		return;
	}
	for(i=0; i<N; i++){
		vmc = self->items.pVmc[i];
		switch( vmc->code ){
		case DVM_GOTO : case DVM_TEST : case DVM_TEST_I :
		case DVM_TEST_F : case DVM_TEST_D : case DVM_SAFE_GOTO :
		case DVM_SWITCH : case DVM_CASE : 
		case DVM_JITC :
		case DVM_CRRE :
			j = vmc->b;
			if( vmc->code == DVM_CRRE ){
				j = vmc->c;
			}else if( vmc->code == DVM_JITC ){
				j = vmc->b + i;
			}
			for(k=0; k<dels->size; k++){
				if( dels->items.pInt[k] > j ) break; 
			}
			if( vmc->code == DVM_CRRE ){
				if( vmc->c ) vmc->c -= k;
			}else if( vmc->code == DVM_JITC ){
				vmc->b = (vmc->b + i - k) - M;
			}else{
				vmc->b -= k;
			}
			break;
		default : break;
		}
		if( vmc->code != DVM_UNUSED ){
			self->items.pVmc[M] = vmc;
			M += 1;
		}else{
			DaoVmCodeX_Delete( vmc );
		}
	}
	self->size = M;
	DArray_Delete( dels );
}

DVaTuple* DVaTuple_New( size_t size, DValue val )
{
	DVaTuple *self = (DVaTuple*)dao_malloc( sizeof(DVaTuple) );
	self->data = NULL;
	self->size = 0;
	if( size >0 ) DVaTuple_Resize( self, size, val );
#ifdef DAO_GC_PROF
	daoCountArray ++;
#endif
	return self;
}
void DVaTuple_Delete( DVaTuple *self )
{
	DVaTuple_Clear( self );
	dao_free( self );
#ifdef DAO_GC_PROF
	daoCountArray --;
#endif
}
void DVaTuple_Resize( DVaTuple *self, size_t size, DValue val )
{
	DValue nil = daoNullValue;
	size_t i;
	if( size == self->size ) return;

	for(i=size; i<self->size; i++ ) DValue_Clear( self->data + i );
	self->data = dao_realloc( self->data, size * sizeof(DValue) );

	if( val.t <= DAO_DOUBLE ){
		for(i=self->size; i<size; i++ ) self->data[i] = val;
	}else{
		for(i=self->size; i<size; i++ ){
			self->data[i] = nil;
			DValue_Copy( self->data + i, val );
		}
	}
	self->size = size;
}
void DVaTuple_Clear( DVaTuple *self )
{
	size_t i;
	for(i=0; i<self->size; i++ ) DValue_Clear( self->data + i );
	if( self->data ) dao_free( self->data );
	self->data = NULL;
	self->size = 0;
}

DPtrTuple* DPtrTuple_New( size_t size, void *val )
{
	DPtrTuple *self = (DPtrTuple*)dao_malloc( sizeof(DPtrTuple) );
	self->items.pVoid = NULL;
	self->size = 0;
	if( size ) DPtrTuple_Resize( self, size, val );
#ifdef DAO_GC_PROF
	daoCountArray ++;
#endif
	return self;
}
void DPtrTuple_Delete( DPtrTuple *self )
{
	DPtrTuple_Clear( self );
	dao_free( self );
#ifdef DAO_GC_PROF
	daoCountArray --;
#endif
}
void DPtrTuple_Resize( DPtrTuple *self, size_t size, void *val )
{
	size_t i;
	if( size == self->size ) return;
	self->items.pVoid = dao_realloc( self->items.pVoid, size * sizeof(void*) );
	for(i=self->size; i<size; i++ ) self->items.pVoid[i] = val;
	self->size = size;
}
void DPtrTuple_Clear( DPtrTuple *self )
{
	if( self->items.pVoid ) dao_free( self->items.pVoid );
	self->items.pVoid = NULL;
	self->size = 0;
}
