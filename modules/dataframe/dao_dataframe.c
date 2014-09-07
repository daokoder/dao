/*
// Dao DataFrame Module
// http://www.daovm.net
//
// Copyright (c) 2013, Limin Fu
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
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "dao_dataframe.h"
#include "daoVmspace.h"
#include "daoProcess.h"
#include "daoNumtype.h"
#include "daoValue.h"
#include "daoType.h"
#include "daoGC.h"



static int DaoType_GetDataSize( DaoType *self )
{
	switch( self->tid ){
	case DAO_INTEGER : return sizeof(dao_integer);
	case DAO_FLOAT   : return sizeof(dao_float);
	case DAO_COMPLEX : return sizeof(dao_complex);
	case DAO_STRING  : return sizeof(DString);
	default : break;
	}
	return sizeof(DaoValue*);
}
static int DaoType_GetDataType( DaoType *self )
{
	int datatype = self->tid;
	switch( self->tid ){
	case DAO_INTEGER :
	case DAO_FLOAT   :
	case DAO_COMPLEX :
	case DAO_STRING  : break;
	default : datatype = 0; break;  /* zero for DaoValue* items; */
	}
	return datatype;
}



DaoType *daox_type_datacolumn = NULL;

DaoxDataColumn* DaoxDataColumn_New( DaoType *type )
{
	DaoxDataColumn *self = (DaoxDataColumn*) dao_calloc( 1, sizeof(DaoxDataColumn) );
	DaoCstruct_Init( (DaoCstruct*) self, daox_type_datacolumn );
	if( type == NULL ) type = DaoType_GetCommonType( DAO_ANY, 0 );
	GC_IncRC( type );
	self->vatype = type;
	self->cells = DArray_New( DaoType_GetDataSize( type ) );
	self->cells->type = DaoType_GetDataType( type );
	return self;
}
void DaoxDataColumn_Delete( DaoxDataColumn *self )
{
	DaoxDataColumn_Reset( self, 0 );
	DArray_Delete( self->cells );
	GC_DecRC( self->vatype );
	DaoCstruct_Free( (DaoCstruct*) self );
	dao_free( self );
}
void DaoxDataColumn_Reset( DaoxDataColumn *self, daoint size )
{
	daoint i, datatype;
	if( self->vatype == NULL && self->cells->size == 0 && size == 0 ) return;
	assert( self->vatype != NULL );
	datatype = DaoType_GetDataType( self->vatype );
	if( size < self->cells->size ){
		for(i=size; i<self->cells->size; ++i){
			if( datatype == DAO_STRING ){
				DString_Clear( self->cells->data.strings + i );
			}else if( datatype == 0 ){
				GC_DecRC( self->cells->data.values[i] );
			}
		}
		DArray_Reset( self->cells, size );
	}else if( size > self->cells->size ){
		DArray_Reserve( self->cells, size );
		for(i=self->cells->size; i<size; ++i){
			if( datatype == DAO_STRING ){
				DString_Init( self->cells->data.strings + i );
				DString *s = & self->cells->data.strings[i];
			}else if( datatype == 0 ){
				self->cells->data.values[i] = NULL;
			}
		}
		DArray_Reset( self->cells, size );
	}
}
void DaoxDataColumn_SetType( DaoxDataColumn *self, DaoType *type )
{
	int datatype, datasize;

	DaoxDataColumn_Reset( self, 0 );
	if( type == NULL ) type = DaoType_GetCommonType( DAO_ANY, 0 );
	datatype = DaoType_GetDataType( type );
	datasize = DaoType_GetDataSize( type );

	GC_Assign( & self->vatype, type );
	self->cells->capacity = (self->cells->capacity * self->cells->stride) / datasize;
	self->cells->stride = datasize;
	self->cells->type = datatype;
}
void DaoxDataColumn_SetCell( DaoxDataColumn *self, daoint i, DaoValue *value )
{
	if( value == NULL ){
		dao_complex zero = {0.0,0.0};
		switch( self->vatype->tid ){
		default :
			GC_DecRC( self->cells->data.values[i] );
			self->cells->data.values[i] = value;
			break;
		case DAO_INTEGER : self->cells->data.daoints[i]   = 0; break;
		case DAO_FLOAT   : self->cells->data.doubles[i]   = 0.0; break;
		case DAO_COMPLEX : self->cells->data.complexes[i] = zero; break;
		case DAO_STRING  : DString_Reset( & self->cells->data.strings[i], 0 ); break;
		}
		return;
	}
	switch( self->vatype->tid ){
	default :
		GC_Assign( & self->cells->data.values[i], value );
		break;
	case DAO_INTEGER : self->cells->data.daoints[i]   = DaoValue_GetInteger( value ); break;
	case DAO_FLOAT   : self->cells->data.doubles[i]   = DaoValue_GetFloat( value ); break;
	case DAO_COMPLEX : self->cells->data.complexes[i] = DaoValue_GetComplex( value ); break;
	case DAO_STRING  : DaoValue_GetString( value, & self->cells->data.strings[i] ); break;
	}
}
DaoValue* DaoxDataColumn_GetCell( DaoxDataColumn *self, daoint i, DaoValue *value )
{
	value->xNone.type = self->vatype->tid;
	switch( self->vatype->tid ){
	case DAO_INTEGER : value->xInteger.value = self->cells->data.daoints[i]; break;
	case DAO_FLOAT   : value->xFloat.value  = self->cells->data.doubles[i]; break;
	case DAO_COMPLEX : value->xComplex.value = self->cells->data.complexes[i]; break;
	case DAO_STRING  : value->xString.value   = & self->cells->data.strings[i]; break;
	default : value = self->cells->data.values[i]; break;
	}
	return value;
}
static int DaoxDataColumn_GetPrintWidth( DaoxDataColumn *self, int max )
{
	daoint i, width = 0;
	switch( self->vatype->tid ){
	case DAO_FLOAT   : return 12;
	case DAO_COMPLEX : return 16;
	}
	for(i=0; i<self->cells->size; ++i){
		int w = 0;
		switch( self->vatype->tid ){
		case DAO_INTEGER : w = 1 + log10( 1 + abs( self->cells->data.daoints[i] ) ); break;
		case DAO_STRING  : w = self->cells->data.strings[i].size; break;
		default : w = max; break;
		}
		if( w > width ) width = w;
		if( w >= max ) break;
	}
	if( width < 2 ) width = 2;
	if( width > max ) width = max;
	return width;
}

static void DaoxDataColumn_GetGCFields( void *p, DList *values, DList *a, DList *m, int rm )
{
	DaoxDataColumn *self = (DaoxDataColumn*) p;
	if( self->vatype ) DList_Append( values, self->vatype );
	if( rm ){
		DaoxDataColumn_Reset( self, 0 );
		self->vatype = NULL;
	}
}

DaoTypeBase datacolumnTyper =
{
	"DataColumn", NULL, NULL, NULL, {0}, {0},
	(FuncPtrDel)DaoxDataColumn_Delete, DaoxDataColumn_GetGCFields
};




DaoType *daox_type_dataframe = NULL;

DaoxDataFrame* DaoxDataFrame_New()
{
	int i;
	DaoxDataFrame *self = (DaoxDataFrame*) dao_calloc( 1, sizeof(DaoxDataFrame) );
	DaoCstruct_Init( (DaoCstruct*) self, daox_type_dataframe );
	for(i=0; i<3; ++i){
		self->dims[i] = 0;
		self->groups[i] = 0;
		self->labels[i] = DList_New( DAO_DATA_MAP );
	}
	self->columns = DList_New( DAO_DATA_VALUE );
	self->caches = DList_New( DAO_DATA_VALUE );
	self->original = NULL;
	self->slices = NULL;
	return self;
}
void DaoxDataFrame_Delete( DaoxDataFrame *self )
{
	int i;
	for(i=0; i<3; ++i) DList_Delete( self->labels[i] );
	if( self->slices ) DArray_Delete( self->slices );
	DList_Delete( self->columns );
	DList_Delete( self->caches );
	GC_DecRC( self->original );
	DaoCstruct_Free( (DaoCstruct*) self );
	dao_free( self );
}

void DaoxDataFrame_Clear( DaoxDataFrame *self )
{
	daoint i;
	DaoxDataFrame_Reset( self );
	for(i=0; i<self->caches->size; ++i){
		DaoxDataColumn_Delete( (DaoxDataColumn*) self->caches->items.pVoid[i] );
	}
	DList_Clear( self->caches );
}
void DaoxDataFrame_Reset( DaoxDataFrame *self )
{
	daoint i;
	for(i=0; i<self->columns->size; ++i){
		DList_Append( self->caches, self->columns->items.pVoid[i] );
	}
	for(i=0; i<3; ++i){
		self->dims[i] = 0;
		self->groups[i] = 0;
		DList_Clear( self->labels[i] );
	}
	GC_DecRC( self->original );
	self->original = NULL;
	DList_Clear( self->columns );
}
DaoxDataColumn* DaoxDataFrame_MakeColumn( DaoxDataFrame *self, DaoType *type )
{
	if( self->caches->size ){
		DaoxDataColumn *column = (DaoxDataColumn*) DList_Back( self->caches );
		DaoxDataColumn_SetType( column, type );
		DList_PopBack( self->caches );
		return column;
	}
	return DaoxDataColumn_New( type );
}

int DaoxDataFrame_FromMatrix( DaoxDataFrame *self, DaoArray *mat )
{
	DaoxDataColumn *col;
	DaoType *etype = DaoType_GetCommonType( mat->etype, 0 );
	daoint i, j, k, N, M, K, MK;

	DaoxDataFrame_Reset( self );
	if( mat->ndim == 2 ){
		self->dims[0] = mat->dims[0];
		self->dims[1] = mat->dims[1];
		self->dims[2] = 1;
	}else if( mat->ndim == 3 ){
		self->dims[0] = mat->dims[0];
		self->dims[1] = mat->dims[1];
		self->dims[2] = mat->dims[2];
	}else{
		return 0;
	}

	N = self->dims[0];
	M = self->dims[1];
	K = self->dims[2];
	MK = M * K;

	for(j=0; j<M; ++j){
		col = DaoxDataFrame_MakeColumn( self, etype );
		DList_Append( self->columns, col );
		DaoxDataColumn_Reset( col, N*K );
		for(i=0; i<N; ++i){
			for(k=0; k<K; ++k){
				daoint id2 = k * N + i;
				daoint id3 = i * MK + j * K + k;
				switch( mat->etype ){
				case DAO_INTEGER : col->cells->data.daoints[id2]   = mat->data.i[id3]; break;
				case DAO_FLOAT   : col->cells->data.doubles[id2]   = mat->data.f[id3]; break;
				case DAO_COMPLEX : col->cells->data.complexes[id2] = mat->data.c[id3]; break;
				}
			}
		}
	}
	return 1;
}
daoint DaoxDataFrame_Size( DaoxDataFrame *self )
{
	return self->dims[0] * self->dims[1] * self->dims[2];
}
void DaoxDataFrame_UseLabels( DaoxDataFrame *self, int dim, int group )
{
	if( dim >=0 && dim < 3 ){
		DList *labs = self->labels[dim];
		if( group >= labs->size ) group = labs->size - 1;
		if( group < 0 ) group = 0;
		self->groups[dim] = group;
	}
}
void DaoxDataFrame_AddLabelGroup( DaoxDataFrame *self, int dim )
{
	if( dim >=0 && dim < 3 ){
		DMap *labmap = DHash_New( DAO_DATA_STRING, 0 );
		DList *labels = self->labels[dim];
		self->groups[dim] = labels->size;
		DList_Append( labels, labmap );
		DMap_Delete( labmap );
	}
}
void DaoxDataFrame_AddLabel( DaoxDataFrame *self, int dim, const char *lab, daoint idx )
{
	DString slab = DString_WrapChars( lab );
	DMap *labmap = NULL;
	if( dim < 0 || dim >= 3 ) return;
	if( idx < 0 || idx >= self->dims[dim] ) return;
	if( self->groups[dim] >= self->labels[dim]->size ) return;
	labmap = self->labels[dim]->items.pMap[ self->groups[dim] ];
	if( labmap != NULL ) DMap_Insert( labmap, & slab, (void*)(size_t) idx );
}
daoint DaoxDataFrame_GetIndex( DaoxDataFrame *self, int dim, const char *label )
{
	DString slab = DString_WrapChars( label );
	DMap *labmap = NULL;
	DNode *it;
	if( dim < 0 || dim >= 3 ) return -1;
	if( self->groups[dim] >= self->labels[dim]->size ) return -1;
	labmap = self->labels[dim]->items.pMap[ self->groups[dim] ];
	it = labmap == NULL ? NULL : DMap_Find( labmap, & slab );
	if( it ) return it->value.pInt;
	return -1;
}
void DaoxDataFrame_AddLabels( DaoxDataFrame *self, int dim, DMap *labels )
{
	DString *lab;
	DNode *it;
	if( labels->keytype != DAO_DATA_STRING && labels->keytype != DAO_DATA_VALUE ) return;
	if( labels->valtype != 0 && labels->valtype != DAO_DATA_VALUE ) return;
	lab = DString_New(1);
	DaoxDataFrame_AddLabelGroup( self, dim );
	for(it=DMap_First(labels); it; it=DMap_Next(labels,it)){
		DString *lab2 = it->key.pString;
		daoint idx = it->value.pInt;
		if( labels->keytype == DAO_DATA_VALUE ){
			if( it->key.pValue->type != DAO_STRING ) continue;
			lab2 = it->key.pValue->xString.value;
		}
		if( labels->valtype == DAO_DATA_VALUE ){
			if( it->value.pValue->type != DAO_INTEGER ) continue;
			idx = it->value.pValue->xInteger.value;
		}
		if( idx < 0 ) continue;
		DString_Reset( lab, 0 );
		DString_Append( lab, lab2 );
		DaoxDataFrame_AddLabel( self, dim, lab->chars, idx );
	}
	DString_Delete( lab );
}
void DaoxDataFrame_GetLabel( DaoxDataFrame *self, int dim, int group, daoint i, DString *lab )
{
	DMap *labels = self->labels[dim]->items.pMap[group];
	DNode *it;
	DString_Reset( lab, 0 );
	for(it=DMap_First(labels); it; it=DMap_Next(labels,it)){
		if( it->value.pInt == i ){
			DString_Append( lab, it->key.pString );
			break;
		}
	}
}
static void DaoxDataFrame_PrepareSlices( DaoxDataFrame *self )
{
	if( self->slices != NULL ) return;
	self->slices = DArray_New( sizeof(daoint) );
}
static daoint DaoSlice_GetSize( DArray *slices, int dim )
{
	return slices->data.daoints[2*dim + 1];
}
static daoint DaoSlice_GetIndex( DArray *slices, int dim, daoint idx )
{
	return slices->data.daoints[2*dim] + idx;
}
static void DaoxDataFrame_SliceFrom( DaoxDataFrame *self, DaoxDataFrame *orig, DArray *slices )
{
	DArray *rows = DArray_New( sizeof(daoint) );
	DArray *cols = DArray_New( sizeof(daoint) );
	DArray *deps = DArray_New( sizeof(daoint) );
	daoint N = DaoSlice_GetSize( slices, 0 );
	daoint M = DaoSlice_GetSize( slices, 1 );
	daoint K = DaoSlice_GetSize( slices, 2 );
	daoint d, i, j, k;
	daoint *maps[3];
	DaoxDataFrame_Reset( self );
	DArray_Resize( rows, orig->dims[0] );
	DArray_Resize( cols, orig->dims[1] );
	DArray_Resize( deps, orig->dims[2] );
	for(i=0; i<rows->size; ++i) rows->data.daoints[i] = -1;
	for(i=0; i<cols->size; ++i) cols->data.daoints[i] = -1;
	for(i=0; i<deps->size; ++i) deps->data.daoints[i] = -1;
	maps[0] = rows->data.daoints;
	maps[1] = cols->data.daoints;
	maps[2] = deps->data.daoints;
	for(j=0; j<M; ++j){
		daoint jj = DaoSlice_GetIndex( slices, 1, j );
		DaoxDataColumn *source = (DaoxDataColumn*) orig->columns->items.pVoid[jj];
		DaoxDataColumn *target = DaoxDataFrame_MakeColumn( self, source->vatype );
		int datatype = DaoType_GetDataType( target->vatype );

		cols->data.daoints[jj] = j;
		DList_Append( self->columns, target );
		DaoxDataColumn_Reset( target, N*K );
		for(i=0; i<N; ++i){
			daoint ii = DaoSlice_GetIndex( slices, 0, i );
			rows->data.daoints[ii] = i;
			for(k=0; k<K; ++k){
				daoint kk = DaoSlice_GetIndex( slices, 2, k );
				daoint id2 = kk * orig->dims[0] + ii;
				daoint id3 = k * N + i;
				void *src = DArray_Get( source->cells, id2 );
				void *des = DArray_Get( target->cells, id3 );
				deps->data.daoints[kk] = k;
				if( datatype == 0 ){ /* DaoValue */
					DaoValue *value = source->cells->data.values[id2];
					GC_IncRC( value );
				}
				memcpy( des, src, source->cells->stride );
			}
		}
	}
	self->dims[0] = N;
	self->dims[1] = M;
	self->dims[2] = K;
	for(d=0; d<3; ++d){
		DList *labels = orig->labels[d];
		for(i=0; i<labels->size; ++i){
			DMap *labmap = labels->items.pMap[i];
			DNode *it;
			DaoxDataFrame_AddLabelGroup( self, d );
			for(it=DMap_First(labmap); it; it=DMap_Next(labmap,it)){
				daoint id = maps[d][it->value.pInt];
				if( id < 0 ) continue;
				DaoxDataFrame_AddLabel( self, d, it->key.pString->chars, id );
			}
		}
	}
	DArray_Delete( rows );
	DArray_Delete( cols );
	DArray_Delete( deps );
}
void DaoxDataFrame_Sliced( DaoxDataFrame *self )
{
	if( self->original == NULL || self->slices == NULL ) return;
	DaoxDataFrame_SliceFrom( self, self->original, self->slices );
	GC_DecRC( self->original );
	self->original = NULL;
}
daoint DaoxDataFrame_SliceSize( DaoxDataFrame *self, DaoxDataFrame *original )
{
	daoint fullsize = DaoxDataFrame_Size( self );
	daoint i, size = 1;
	if( original == NULL ) return fullsize;
	if( self->slices == NULL || self->slices->size < 6 ) return fullsize;
	return self->slices->data.daoints[6] * self->slices->data.daoints[9];
}

void DaoxDataFrame_Encode( DaoxDataFrame *self, DString *output )
{
}
void DaoxDataFrame_Decode( DaoxDataFrame *self, DString *input )
{
}




static int SliceRange( DArray *slices, daoint N, daoint first, daoint last )
{
	daoint *start = DArray_PushDaoInt( slices, 0 );
	daoint *count = DArray_PushDaoInt( slices, 0 );
	if( first < 0 ) first += N;
	if( last < 0 ) last += N;
	if( first < 0 || first >= N || last < 0 || last >= N ) return 0;
	*start = first;
	if( first <= last ) *count = last - first + 1;
	return 1;
}
static int SliceRange2( DArray *slices, daoint N, daoint first, daoint count )
{
	daoint *start = DArray_PushDaoInt( slices, 0 );
	daoint *count2 = DArray_PushDaoInt( slices, 0 );
	if( first < 0 ) first += N;
	if( first < 0 || first >= N ) return 0;
	*start = first;
	if( first + count > N ) return 0;
	*count2 = count;
	return 1;
}
static void MakeSlice( DaoProcess *proc, DaoValue *pid, daoint N, DArray *slice )
{
	daoint j, id, from, to, rc = 0;
	if( pid == NULL || pid->type == 0 ){
		SliceRange2( slice, N, 0, N );
		return;
	}
	switch( pid->type ){
	case DAO_INTEGER :
	case DAO_FLOAT :
		{
			id = DaoValue_GetInteger( pid );
			rc = SliceRange2( slice, N, id, 1 );
			break;
		}
	case DAO_STRING :
		{
			break;
		}
	case DAO_TUPLE :
		{
			DaoValue **data = pid->xTuple.values;
			if( data[0]->type == DAO_INTEGER && data[1]->type == DAO_INTEGER ){
				from = data[0]->xInteger.value;
				to   = data[1]->xInteger.value;
				rc = SliceRange( slice, N, from, to );
			}else if( data[0]->type == DAO_NONE && data[1]->type == DAO_NONE ){
				rc = SliceRange2( slice, N, 0, N );
			}else if( data[0]->type <= DAO_FLOAT && data[1]->type == DAO_NONE ){
				from = DaoValue_GetInteger( data[0] );
				rc = SliceRange( slice, N, from, -1 );
			}else if( data[0]->type == DAO_NONE && data[1]->type <= DAO_FLOAT ){
				to = DaoValue_GetInteger( data[1] );
				rc = SliceRange( slice, N, 0, to );
			}else if( data[0]->type == DAO_STRING && data[1]->type == DAO_STRING ){
			}else if( data[0]->type == DAO_STRING && data[1]->type == DAO_NONE ){
			}else if( data[0]->type == DAO_NONE && data[1]->type == DAO_STRING ){
			}else{
				DaoProcess_RaiseError( proc, "Index", "need number" );
			}
			break;
		}
	default: break;
	}
	if( slice->size < 2 ) SliceRange2( slice, N, 0, N );
	if( rc == 0 ) DaoProcess_RaiseError( proc, "Index::Range", "" );
}
static void DaoxDataFrame_MakeFullSlice( DaoxDataFrame *self, DArray *slices )
{
	daoint i, D = 3;

	DArray_Reserve( slices, 10 );
	DArray_Reset( slices, 0 );
	for(i=0; i<D; ++i){
		DArray_PushDaoInt( slices, 0 );
		DArray_PushDaoInt( slices, self->dims[i] );
	}
}
static void DaoArray_MakeFullSlice( DaoArray *self, DArray *slices )
{
	daoint i, D = self->ndim;

	DArray_Reserve( slices, 2*self->ndim + 4 );
	DArray_Reset( slices, 0 );
	for(i=0; i<D; ++i){
		DArray_PushDaoInt( slices, 0 );
		DArray_PushDaoInt( slices, self->dims[i] );
	}
	if( D == 2 ){
		DArray_PushDaoInt( slices, 0 );
		DArray_PushDaoInt( slices, 1 );
	}
}
static int DaoDataFrame_MakeSlice( DaoxDataFrame *self, DaoProcess *proc, DaoValue *idx[], int N, DArray *slices )
{
	daoint *dims = self->dims;
	daoint i, D = 3, S = 1;

	DaoxDataFrame_MakeFullSlice( self, slices );
	if( N > D ){
		DaoProcess_RaiseWarning( proc, NULL, "too many indices" );
		N = D;
	}
	DArray_Reset( slices, 0 );
	for(i=0; i<N; ++i) MakeSlice( proc, idx[i], dims[i], slices );
	for(i=N; i<D; ++i){
		DArray_PushDaoInt( slices, 0 );
		DArray_PushDaoInt( slices, self->dims[i] );
	}
	for(i=0; i<D; ++i) S *= DaoSlice_GetSize( slices, i );
	return S;
}

static int DaoxDF_AlignShape1( daoint dims1[], daoint ndim1, daoint dims2[], daoint ndim2 )
{
	daoint i;
	if( ndim1 != ndim2 ) return 0;
	for(i=0; i<ndim1; ++i) if( dims1[i] != dims2[i] ) return 0;
	return 1;
}
static int DaoxDF_AlignShape2( DArray *slices, daoint dims[], daoint ndim )
{
	daoint i, j;
	if( slices->size < 2*ndim ) return 0;
	for(i=0; i<ndim; ++i){
		if( dims[i] != DaoSlice_GetSize( slices, i ) ) return 0;
	}
	return 1;
}
static int DaoxDF_AlignShape3( DArray *slices1, DArray *slices2, int ndim )
{
	daoint i, j;
	if( slices1->size < 2*ndim ) return 0;
	if( slices2->size < 2*ndim ) return 0;
	for(i=0; i<ndim; ++i){
		if( DaoSlice_GetSize( slices1, i ) != DaoSlice_GetSize( slices2, i ) ) return 0;
	}
	return 1;
}
static int DaoxDataFrame_AlignArray( DaoxDataFrame *self, DaoArray *array )
{
	daoint dims1[4] = {0,0,1,0};
	daoint dims2[4] = {0,0,1,0};
	daoint ndim2 = 3;
	if( self->original && self->slices ){
		dims1[0] = DaoSlice_GetSize( self->slices, 0 );
		dims1[1] = DaoSlice_GetSize( self->slices, 1 );
		dims1[2] = DaoSlice_GetSize( self->slices, 2 );
	}else{
		dims1[0] = self->dims[0];
		dims1[1] = self->dims[1];
		dims1[2] = self->dims[2];
	}
	if( array->original && array->slices ){
		dims2[0] = DaoSlice_GetSize( array->slices, 0 );
		dims2[1] = DaoSlice_GetSize( array->slices, 1 );
		if( array->original->ndim == 3 ){
			dims2[2] = DaoSlice_GetSize( array->slices, 2 );
		}else if( array->original->ndim != 2 ){
			ndim2 = array->original->ndim;
		}
	}else{
		dims2[0] = array->dims[0];
		dims2[1] = array->dims[1];
		if( array->ndim == 3 ){
			dims2[2] = array->dims[2];
		}else if( array->ndim != 2 ){
			ndim2 = array->ndim;
		}
	}
	return DaoxDF_AlignShape1( dims1, 3, dims2, ndim2 );
}
static int DaoxDataFrame_AlignFrame( DaoxDataFrame *self, DaoxDataFrame *df )
{
	if( self->original && self->slices && df->original && df->slices ){
		return DaoxDF_AlignShape3( self->slices, df->slices, 3 );
	}else if( self->original && self->slices ){
		return DaoxDF_AlignShape2( self->slices, df->dims, 3 );
	}else if( df->original && df->slices ){
		return DaoxDF_AlignShape2( df->slices, self->dims, 3 );
	}else{
		return DaoxDF_AlignShape1( self->dims, 3, df->dims, 3 );
	}
	return 0;
}

enum
{
	DAOX_DF_WRONG_SHAP = 1 ,
	DAOX_DF_WRONG_VALUE
};

static int DaoValue_Update( DaoValue *self, DaoValue *other, int opcode )
{
	double D1, D2;
	dao_complex C1, C2;

	switch( (self->type<<8)|other->type ){
	case (DAO_INTEGER<<8)|DAO_INTEGER :
		switch( opcode ){
		case DVM_MOVE   : self->xInteger.value  = other->xInteger.value; break;
		case DVM_ADD    : self->xInteger.value += other->xInteger.value; break;
		case DVM_SUB    : self->xInteger.value -= other->xInteger.value; break;
		case DVM_MUL    : self->xInteger.value *= other->xInteger.value; break;
		case DVM_DIV    : self->xInteger.value /= other->xInteger.value; break;
		case DVM_MOD    : self->xInteger.value %= other->xInteger.value; break;
		case DVM_BITAND : self->xInteger.value &= other->xInteger.value; break;
		case DVM_BITOR  : self->xInteger.value |= other->xInteger.value; break;
		case DVM_BITXOR : self->xInteger.value ^= other->xInteger.value; break;
		default : return 1;
		}
		break;
	case (DAO_FLOAT<<8)|DAO_FLOAT :
		D1 = self->xFloat.value;
		D2 = other->xFloat.value;
		switch( opcode ){
		case DVM_MOVE : self->xFloat.value  = other->xFloat.value; break;
		case DVM_ADD  : self->xFloat.value += other->xFloat.value; break;
		case DVM_SUB  : self->xFloat.value -= other->xFloat.value; break;
		case DVM_MUL  : self->xFloat.value *= other->xFloat.value; break;
		case DVM_DIV  : self->xFloat.value /= other->xFloat.value; break;
		case DVM_MOD  : self->xFloat.value  = D1-D2*(daoint)(D1/D2); break;
		default : return 1;
		}
		break;
	case (DAO_INTEGER<<8)|DAO_FLOAT :
		D1 = self->xInteger.value;
		D2 = DaoValue_GetFloat( other );
		switch( opcode ){
		case DVM_MOVE : self->xInteger.value = (daoint) D2; break;
		case DVM_ADD  : self->xInteger.value = (daoint)(D1 + D2); break;
		case DVM_SUB  : self->xInteger.value = (daoint)(D1 - D2); break;
		case DVM_MUL  : self->xInteger.value = (daoint)(D1 * D2); break;
		case DVM_DIV  : self->xInteger.value = (daoint)(D1 / D2); break;
		case DVM_MOD  : self->xInteger.value = (daoint)(D1-D2*(daoint)(D1/D2)); break;
		default : return 1;
		}
		break;
	case (DAO_FLOAT<<8)|DAO_INTEGER :
		D1 = self->xFloat.value;
		D2 = DaoValue_GetFloat( other );
		switch( opcode ){
		case DVM_MOVE : self->xFloat.value =  D2; break;
		case DVM_ADD  : self->xFloat.value = (D1 + D2); break;
		case DVM_SUB  : self->xFloat.value = (D1 - D2); break;
		case DVM_MUL  : self->xFloat.value = (D1 * D2); break;
		case DVM_DIV  : self->xFloat.value = (D1 / D2); break;
		case DVM_MOD  : self->xFloat.value = (D1-D2*(daoint)(D1/D2)); break;
		default : return 1;
		}
		break;
	case (DAO_COMPLEX<<8)|DAO_INTEGER :
	case (DAO_COMPLEX<<8)|DAO_FLOAT :
		C1 = self->xComplex.value;
		D2 = DaoValue_GetFloat( other );
		switch( opcode ){
		case DVM_MOVE : C1.real = D2;  C1.imag = 0.0; self->xComplex.value = C1; break;
		case DVM_MUL  : C1.real *= D2; C1.imag *= D2; self->xComplex.value = C1; break;
		case DVM_DIV  : C1.real /= D2; C1.imag /= D2; self->xComplex.value = C1; break;
		case DVM_ADD  : self->xComplex.value.real += D2; break;
		case DVM_SUB  : self->xComplex.value.real -= D2; break;
		default : return 1;
		}
		break;
	case (DAO_COMPLEX<<8)|DAO_COMPLEX :
		C1 = self->xComplex.value;
		C2 = other->xComplex.value;
		switch( opcode ){
		case DVM_MOVE : self->xComplex.value = C2; break;
		case DVM_ADD  : COM_IP_ADD( C1, C2 ); self->xComplex.value = C1; break;
		case DVM_SUB  : COM_IP_SUB( C1, C2 ); self->xComplex.value = C1; break;
		case DVM_MUL  : COM_IP_MUL( C1, C2 ); self->xComplex.value = C1; break;
		case DVM_DIV  : COM_IP_DIV( C1, C2 ); self->xComplex.value = C1; break;
		default : return 1;
		}
		break;
	}
	return 0;
}

static int DaoxDataFrame_UpdateByArray( DaoxDataFrame *self, DaoArray *array, int opcode )
{
	DaoValue value = {0};
	DaoValue value2 = {0};
	DArray *destSlices = self->slices;
	DArray *fromSlices = array->slices;
	DArray *fromSlices2 = array->slices;
	daoint i, j, k, N, M, K;

	if( DaoxDataFrame_AlignArray( self, array ) == 0 ) return DAOX_DF_WRONG_SHAP;

	if( self->original == NULL ){
		DaoxDataFrame_PrepareSlices( self );
		DaoxDataFrame_MakeFullSlice( self, self->slices );
		destSlices = self->slices;
	}else{
		self = self->original;
	}
	if( array->original == NULL ){
		fromSlices = DArray_New( sizeof(daoint) );
		DaoArray_MakeFullSlice( array, fromSlices );
	}else{
		array = array->original;
	}

	N = DaoSlice_GetSize( destSlices, 0 );
	M = DaoSlice_GetSize( destSlices, 1 );
	K = DaoSlice_GetSize( destSlices, 2 );

	for(j=0; j<M; ++j){
		daoint jjdes = DaoSlice_GetIndex( destSlices, 1, j );
		daoint jjsrc = DaoSlice_GetIndex( fromSlices, 1, j );
		DaoxDataColumn *column = (DaoxDataColumn*) self->columns->items.pVoid[jjdes];
		for(i=0; i<N; ++i){
			daoint iides = DaoSlice_GetIndex( destSlices, 0, i );
			daoint iisrc = DaoSlice_GetIndex( fromSlices, 0, i );
			for(k=0; k<K; ++k){
				daoint kkdes = DaoSlice_GetIndex( destSlices, 2, k );
				daoint kksrc = DaoSlice_GetIndex( fromSlices, 2, k );
				daoint idsrc = iisrc * M * K + jjsrc * K + kksrc;
				daoint iddes = kkdes * N + iides;
				DaoValue *dest = DaoxDataColumn_GetCell( column, iddes, & value2 );
				DaoArray_GetValue( array, idsrc, & value );
				if( DaoValue_Update( dest, & value, opcode ) ) return DAOX_DF_WRONG_VALUE;
				DaoxDataColumn_SetCell( column, iddes, dest );
			}
		}
	}

	if( fromSlices != fromSlices2 ) DArray_Delete( fromSlices );
	return 0;
}
static int DaoxDataFrame_UpdateByFrame( DaoxDataFrame *self, DaoxDataFrame *df, int opcode )
{
	DaoValue value = {0};
	DaoValue value2 = {0};
	DArray *destSlices = self->slices;
	DArray *fromSlices = df->slices;
	DArray *fromSlices2 = df->slices;
	daoint i, j, k, N, M, K;

	if( DaoxDataFrame_AlignFrame( self, df ) == 0 ) return DAOX_DF_WRONG_SHAP;

	if( self->original == NULL ){
		DaoxDataFrame_PrepareSlices( self );
		DaoxDataFrame_MakeFullSlice( self, self->slices );
		destSlices = self->slices;
	}else{
		self = self->original;
	}
	if( df->original == NULL ){
		fromSlices = DArray_New( sizeof(daoint) );
		DaoxDataFrame_MakeFullSlice( df, fromSlices );
	}else{
		df = df->original;
	}

	N = DaoSlice_GetSize( destSlices, 0 );
	M = DaoSlice_GetSize( destSlices, 1 );
	K = DaoSlice_GetSize( destSlices, 2 );

	for(j=0; j<M; ++j){
		daoint jjdes = DaoSlice_GetIndex( destSlices, 1, j );
		daoint jjsrc = DaoSlice_GetIndex( fromSlices, 1, j );
		DaoxDataColumn *coldes = (DaoxDataColumn*) self->columns->items.pVoid[jjdes];
		DaoxDataColumn *colsrc = (DaoxDataColumn*) df->columns->items.pVoid[jjsrc];
		if( DaoType_MatchTo( colsrc->vatype, coldes->vatype, NULL ) < DAO_MT_SUB )
			return DAOX_DF_WRONG_VALUE;
		for(i=0; i<N; ++i){
			daoint iides = DaoSlice_GetIndex( destSlices, 0, i );
			daoint iisrc = DaoSlice_GetIndex( fromSlices, 0, i );
			for(k=0; k<K; ++k){
				daoint kkdes = DaoSlice_GetIndex( destSlices, 2, k );
				daoint kksrc = DaoSlice_GetIndex( fromSlices, 2, k );
				daoint idsrc = kksrc * N + iisrc;
				daoint iddes = kkdes * N + iides;
				DaoValue *dest = DaoxDataColumn_GetCell( coldes, iddes, & value2 );
				DaoValue *val = DaoxDataColumn_GetCell( colsrc, idsrc, & value );
				if( val->type == dest->type && val->type == DAO_STRING ){
					DString_Assign( dest->xString.value, val->xString.value );
					continue;
				}else if( DaoValue_Update( dest, val, opcode ) ){
					return DAOX_DF_WRONG_VALUE;
				}
				DaoxDataColumn_SetCell( coldes, iddes, dest );
			}
		}
	}

	if( fromSlices != fromSlices2 ) DArray_Delete( fromSlices );
	return 0;
}




DaoxDataFrame* DaoProcess_MakeReturnDataFrame( DaoProcess *self )
{
	DaoVmCode *vmc = self->activeCode;
	DaoValue *dC = self->activeValues[ vmc->c ];
	DaoxDataFrame *df = (DaoxDataFrame*) DaoValue_CastCstruct( dC, daox_type_dataframe );
	if( df != NULL ){
		DaoVmCode *vmc2 = vmc + 1;
		int reuse = 0;
		if( df->refCount == 1 ) reuse = 1;
		if( df->refCount == 2 && (vmc2->code == DVM_MOVE || vmc2->code == DVM_MOVE_PP) && vmc2->a != vmc2->c ){
			if( self->activeValues[vmc2->c] == (DaoValue*) df ) reuse = 1;
		}
		if( reuse ){
			DaoxDataFrame_Reset( df );
			return df;
		}
	}
	df = DaoxDataFrame_New();
	DaoValue_Copy( (DaoValue*) df, & self->activeValues[ vmc->c ] );
	return df;
}



static void FRAME_New( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = DaoxDataFrame_New();
	DaoProcess_PutValue( proc, (DaoValue*) self );
}
static void FRAME_NewMatrix( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = DaoxDataFrame_New();
	DaoProcess_PutValue( proc, (DaoValue*) self );
	DaoxDataFrame_FromMatrix( self, (DaoArray*) p[0] );
}
static void FRAME_FromMatrix( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DaoxDataFrame_FromMatrix( self, (DaoArray*) p[1] );
}
static void FRAME_Size( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DaoProcess_PutInteger( proc, DaoxDataFrame_SliceSize( self, self->original ) );
}
static void FRAME_UseLabels( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	daoint dim = p[1]->xEnum.value;
	daoint group = p[2]->xInteger.value;
	DaoxDataFrame_UseLabels( self, dim, group );
}
static void FRAME_AddLabels( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DaoxDataFrame_AddLabels( self, p[1]->xEnum.value, p[2]->xMap.value );
}
static void FRAME_AddLabel( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DString *lab = DaoValue_TryGetString( p[2] );
	daoint dim = p[1]->xEnum.value;
	daoint idx = p[3]->xInteger.value;
	DaoxDataFrame_AddLabel( self, dim, lab->chars, idx );
}
static void FRAME_GetIndex( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DString *lab = DaoValue_TryGetString( p[2] );
	daoint idx, dim = p[1]->xEnum.value;
	idx = DaoxDataFrame_GetIndex( self, dim, lab->chars );
	DaoProcess_PutInteger( proc, idx );
}
static void FRAME_AddArrayCol( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoValue value = {0};
	DaoxDataColumn *col;
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DaoArray *array = (DaoArray*) p[1];
	DString *lab = DaoValue_TryGetString( p[2] );
	DaoType *etype = DaoType_GetCommonType( array->etype, 0 );
	daoint i, M = self->dims[0] * self->dims[2];

	DaoxDataFrame_Sliced( self );
	col = DaoxDataFrame_MakeColumn( self, etype );
	DList_Append( self->columns, col );
	DaoxDataColumn_Reset( col, M );
	self->dims[1] += 1;

	if( lab->size ){
		DaoxDataFrame_AddLabel( self, DAOX_DF_COL, lab->chars, self->dims[1]-1 );
	}

	if( M > array->size ) M = array->size;
	for(i=0; i<M; ++i){
		DaoArray_GetValue( array, i, & value );
		DaoxDataColumn_SetCell( col, i, & value );
	}
	M = self->dims[0] * self->dims[2];
	for(i=array->size; i<M; ++i) DaoxDataColumn_SetCell( col, i, NULL );
}
static void FRAME_AddListCol( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataColumn *col;
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DaoList *list = (DaoList*) p[1];
	DString *lab = DaoValue_TryGetString( p[2] );
	DaoType *etype = DaoType_GetCommonType( DAO_ANY, 0 );
	daoint i, M = self->dims[0] * self->dims[2];

	if( list->ctype && list->ctype->nested->size ){
		DaoType *tp = list->ctype->nested->items.pType[0];
		if( tp != NULL && !(tp->tid & DAO_ANY) ) etype = tp;
	}

	DaoxDataFrame_Sliced( self );
	col = DaoxDataFrame_MakeColumn( self, etype );
	DList_Append( self->columns, col );
	DaoxDataColumn_Reset( col, M );
	self->dims[1] += 1;

	if( lab->size ){
		DaoxDataFrame_AddLabel( self, DAOX_DF_COL, lab->chars, self->dims[1]-1 );
	}

	if( M > list->value->size ) M = list->value->size;
	for(i=0; i<M; ++i) DaoxDataColumn_SetCell( col, i, list->value->items.pValue[i] );
	M = self->dims[0] * self->dims[2];
	for(i=list->value->size; i<M; ++i) DaoxDataColumn_SetCell( col, i, NULL );
}



static int DaoxDF_IsSingleIndex( DaoValue *value )
{
	if( value->type >= DAO_INTEGER && value->type <= DAO_FLOAT ) return 1;
	if( value->type == DAO_STRING ) return 1;
	return 0;
}
static daoint DaoxDF_MakeIndex( DaoxDataFrame *self, int dim, DaoValue *value, DaoProcess *p )
{
	daoint idx = -1;
	if( value->type >= DAO_INTEGER && value->type <= DAO_FLOAT ){
		idx = DaoValue_GetInteger( value );
	}else if( value->type == DAO_STRING ){
		idx = DaoxDataFrame_GetIndex( self, dim, value->xString.value->chars );
	}else if( value->type == DAO_NONE && self->dims[2] == 1 ){
		idx = 0;
	}
	if( idx < 0 || idx >= self->dims[dim] ){
		DaoProcess_RaiseError( p, "Index::Range", "" );
		return -1;
	}
	return idx;
}
static void FRAME_GETMI( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *df, *self = (DaoxDataFrame*) p[0];
	int singleIndex1 = DaoxDF_IsSingleIndex( p[1] );
	int singleIndex2 = DaoxDF_IsSingleIndex( p[2] );
	int singleIndex3 = DaoxDF_IsSingleIndex( p[3] );

	DaoxDataFrame_Sliced( self );
	if( singleIndex1 && singleIndex2 && (singleIndex3 || self->dims[2] == 1) ){
		daoint i = DaoxDF_MakeIndex( self, DAOX_DF_ROW, p[1], proc );
		daoint j = DaoxDF_MakeIndex( self, DAOX_DF_COL, p[2], proc );
		daoint k = DaoxDF_MakeIndex( self, DAOX_DF_DEP, p[3], proc );
		daoint ik = k * self->dims[0] + i;
		DaoValue value = {0};
		if( i < 0 || j < 0 || k < 0 ) return;
		memset( & value, 0, sizeof(DaoValue) );
		DaoxDataColumn_GetCell( (DaoxDataColumn*) self->columns->items.pVoid[j], ik, & value );
		DaoProcess_PutValue( proc, & value );
	}else{
		df = DaoProcess_MakeReturnDataFrame( proc );
		DaoxDataFrame_PrepareSlices( df );
		DaoDataFrame_MakeSlice( self, proc, p+1, N-1, df->slices );
		GC_Assign( & df->original, self );
		DaoProcess_PutValue( proc, (DaoValue*) df );
	}
}
static void FRAME_SETMI( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *df, *self = (DaoxDataFrame*) p[0];
	DaoValue *value = p[1];
	int singleIndex1 = DaoxDF_IsSingleIndex( p[2] );
	int singleIndex2 = DaoxDF_IsSingleIndex( p[3] );
	int singleIndex3 = DaoxDF_IsSingleIndex( p[4] );

	DaoxDataFrame_Sliced( self );
	if( singleIndex1 && singleIndex2 && (singleIndex3 || self->dims[2] == 1) ){
		daoint i = DaoxDF_MakeIndex( self, DAOX_DF_ROW, p[2], proc );
		daoint j = DaoxDF_MakeIndex( self, DAOX_DF_COL, p[3], proc );
		daoint k = DaoxDF_MakeIndex( self, DAOX_DF_DEP, p[4], proc );
		daoint ik = k * self->dims[0] + i;
		if( i < 0 || j < 0 || k < 0 ) return;
		DaoxDataColumn_SetCell( (DaoxDataColumn*) self->columns->items.pVoid[j], ik, value );
	}else{
		int rc = 0;
		DaoxDataFrame df2 = *self;
		df = (DaoxDataFrame*) DaoValue_CastCstruct( value, daox_type_dataframe );
		DaoxDataFrame_PrepareSlices( self );
		DaoDataFrame_MakeSlice( self, proc, p+2, N-2, self->slices );
		df2.original = self;
		if( value->type == DAO_ARRAY ){
			DaoArray *array = (DaoArray*) value;
			rc = DaoxDataFrame_UpdateByArray( & df2, array, DVM_MOVE );
		}else if( df != NULL ){
			rc = DaoxDataFrame_UpdateByFrame( & df2, df, DVM_MOVE );
		}else{
		}
		if( rc == DAOX_DF_WRONG_SHAP ){
			DaoProcess_RaiseError( proc, "Index", "Invalid shape" );
		}else if( rc == DAOX_DF_WRONG_VALUE ){
			DaoProcess_RaiseError( proc, "Value", "Invalid value" );
		}
	}
}
static void CheckPrintWidth( double value, int *max, int *min, int *dec )
{
	char *p, *p1, *p2, buf[128];
	int w1 = fabs(value) < 1E-256 ? 1 : 1+log10( fabs(value) + 1E-32 ) + (value < 0);
	int w2 = 3;
	value = fabs(value);
	value -= (daoint)value;
	sprintf( buf, "%-64.63f", value );
	p1 = strstr( buf, "0000" );
	p2 = strstr( buf, "9999" );
	p = p1 ? p1 : buf + 3;
	if( p2 && p2 < p1 ) p1 = p2;
	if( p1 && (p1 - buf) > w2 ) w2 = p1 - buf;
	w2 -= 2;

	if( w1 > *max ) *max = w1;
	if( w1 < *min ) *min = w1;
	if( w2 > *dec ) *dec = w2;
}
static void FRAME_PRINT( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DaoxDataFrame *original = self->original;
	DaoStream *stream = proc->stdioStream;
	DaoStream *sstream = DaoStream_New();
	DaoValue valueBuffer, *nulls[3] = {NULL,NULL,NULL};
	DArray *rlabwidth = DArray_New( sizeof(int) );
	DArray *clabwidth = DArray_New( sizeof(int) );
	DArray *decimals = DArray_New( sizeof(int) );
	DArray *scifmts = DArray_New( sizeof(int) );
	DArray *aligns = DArray_New( sizeof(int) );
	DString *label = DString_New(1);
	daoint d, g, i, j, k, s, N, M, K, J = 1;
	int idwidth, maxwidth = 16, maxdec = 3;
	char idfmt[16];
	char fmt[16];
	char buf[512];

	sstream->mode |= DAO_STREAM_STRING;
	memset( &valueBuffer, 0, sizeof(DaoValue) );
	if( stream == NULL ) stream = proc->vmSpace->stdioStream;
	if( self->original == NULL ){
		DaoxDataFrame_PrepareSlices( self );
		DaoDataFrame_MakeSlice( self, proc, nulls, 3, self->slices );
		original = self;
	}
	N = DaoSlice_GetSize( self->slices, 0 );
	M = DaoSlice_GetSize( self->slices, 1 );
	K = DaoSlice_GetSize( self->slices, 2 );
	DString_Reset( label, 100 + 4*sizeof(void*) + log10(1+N+M+K) );
	sprintf( label->chars, "\nDataFrame[%p]", self );
	DaoStream_WriteChars( stream, label->chars );
	if( original != self ){
		sprintf( label->chars, " (Slices from DataFrame[%p])", original );
		DaoStream_WriteChars( stream, label->chars );
	}
	sprintf( label->chars, "\nDimensions: Rows=%" DAO_INT_FORMAT ";", N );
	DaoStream_WriteChars( stream, label->chars );
	sprintf( label->chars, " Cols=%" DAO_INT_FORMAT ";", M );
	DaoStream_WriteChars( stream, label->chars );
	sprintf( label->chars, " Deps=%" DAO_INT_FORMAT ";\n", K );
	DaoStream_WriteChars( stream, label->chars );

	idwidth = 1 + (int)log10(N+1);
	for(i=0; i<N; ++i){
		daoint ii = DaoSlice_GetIndex( self->slices, 0, i );
		int width = 1 + (int)log10(ii+1);
		if( width > idwidth ) idwidth = width;
	}
	sprintf( idfmt, "%%%i%s:", idwidth, DAO_INT_FORMAT );

	if( M == 1 ){
		maxwidth = 64;
		maxdec = 24;
	}else if( M == 2 ){
		maxwidth = 40;
		maxdec = 12;
	}else if( M <= 4 ){
		maxwidth = 24;
		maxdec = 6;
	}

	for(g=0; g<original->labels[DAOX_DF_ROW]->size; ++g){
		int width = 0;
		for(i=0; i<N; ++i){
			daoint ii = DaoSlice_GetIndex( self->slices, 0, i );
			DaoxDataFrame_GetLabel( original, DAOX_DF_ROW, g, ii, label );
			if( label->size > width ) width = label->size;
			if( width > maxwidth ) break;
		}
		if( width > maxwidth ) width = maxwidth;
		DArray_PushInt( rlabwidth, width );
	}
	for(j=0; j<M; ++j){
		int w, datatype, max = 0, min = 0, dec = 0;
		daoint width, jj = DaoSlice_GetIndex( self->slices, 2, j );
		DaoxDataColumn *col = (DaoxDataColumn*) original->columns->items.pVoid[jj];
		DArray *cells = col->cells;

		datatype = DaoType_GetDataType( col->vatype );
		width = DaoxDataColumn_GetPrintWidth( col, 16 );
		for(i=0; i<N && i<1000; ++i){
			daoint v, ii = DaoSlice_GetIndex( self->slices, 0, i );
			dao_complex com;
			switch( datatype ){
			case DAO_INTEGER :
				v = cells->data.daoints[ii];
				w = log10( fabs(v) + 1E-32 ) + (v < 0);
				if( w > max ) max = w;
				break;
			case DAO_FLOAT   :
				CheckPrintWidth( cells->data.doubles[ii], & max, & min, & dec );
				break;
			case DAO_COMPLEX :
				com = cells->data.complexes[ii];
				CheckPrintWidth( com.real, & max, & min, & dec );
				CheckPrintWidth( com.imag, & max, & min, & dec );
				break;
			case DAO_STRING :
				if( cells->data.strings[i].size > max ) max = cells->data.strings[i].size;
				break;
			default :
				break;
			}
		}
		if( dec > maxdec ) dec = maxdec;
		if( col->vatype->tid == DAO_COMPLEX ){
			max *= 2;
			min *= 2;
		}
		if( datatype == 0 ){
			width = maxwidth;
			DArray_PushInt( aligns, 1 );
			DArray_PushInt( scifmts, 0 );
			DArray_PushInt( decimals, 0 );
		}else if( datatype == DAO_STRING ){
			width = max;
			DArray_PushInt( aligns, 1 );
			DArray_PushInt( scifmts, 0 );
			DArray_PushInt( decimals, 0 );
		}else if( max >= maxwidth || min < -dec ){
			width = 16;
			DArray_PushInt( aligns, 0 );
			DArray_PushInt( scifmts, 1 );
			DArray_PushInt( decimals, dec );
		}else{
			width = max + dec + 1;
			if( col->vatype->tid == DAO_COMPLEX ) width += dec + 6;
			DArray_PushInt( aligns, 0 );
			DArray_PushInt( scifmts, 0 );
			DArray_PushInt( decimals, dec );
		}

		for(g=0; g<original->labels[DAOX_DF_COL]->size; ++g){
			DaoxDataFrame_GetLabel( original, DAOX_DF_COL, g, jj, label );
			if( label->size > width ) width = label->size;
			if( width > maxwidth ) break;
		}
		if( width > maxwidth ) width = maxwidth;
		DArray_PushInt( clabwidth, width );
	}

	for(k=0; k<K; ++k){
		daoint kk = DaoSlice_GetIndex( self->slices, 2, k );
		DaoStream_WriteChars( stream, "Depth: " );
		DaoStream_WriteInt( stream, kk );
		DaoStream_WriteChars( stream, ";" );
		if( original->labels[DAOX_DF_DEP]->size ) DaoStream_WriteChars( stream, "\nLabels:" );
		for(g=0; g<original->labels[DAOX_DF_DEP]->size; ++g){
			DaoxDataFrame_GetLabel( original, DAOX_DF_DEP, g, kk, label );
			DaoStream_WriteChars( stream, " " );
			DaoStream_WriteString( stream, label );
			DaoStream_WriteChars( stream, ";" );
		}
		DaoStream_WriteChars( stream, "\n" );
		for(j=0; j<M; j=J){
			int idigits, width2, width = idwidth+1;
			daoint maxidx = 0;
			for(i=0; i<rlabwidth->size; ++i) width += rlabwidth->data.ints[i] + 1;
			width += 1;

			J = j;
			width2 = width;
			for(J=j; J<M; ++J){
				daoint jj = DaoSlice_GetIndex( self->slices, 1, J );
				width2 += clabwidth->data.ints[J] + 2;
				if( width2 > 80 ){
					width2 -= clabwidth->data.ints[J] + 2;
					break;
				}
			}
			if( J == j ) J += 1;

			sprintf( buf, "from %" DAO_INT_FORMAT " to %" DAO_INT_FORMAT ":\n", j, J-1 );
			DaoStream_WriteChars( stream, j == 0 ? "| Columns " : "> Columns " );
			DaoStream_WriteChars( stream, buf );

			for(s=j; s<J; ++s){
				daoint jj = DaoSlice_GetIndex( self->slices, 1, s );
				if( jj > maxidx ) maxidx = jj;
			}
			idigits = 1 + log( maxidx ) / log(10);

			for(g=0; g<idigits; ++g){
				int R = 1;
				sprintf( fmt, "%%-%is", width );
				sprintf( buf, fmt, j == 0 ? "|" : ">" );
				DaoStream_WriteChars( stream, buf );
				for(s=1; s<(idigits-g); ++s) R *= 10;
				for(s=j; s<J; ++s){
					daoint jj = DaoSlice_GetIndex( self->slices, 1, s );
					int width = clabwidth->data.ints[s];
					int align = aligns->data.ints[s];
					int d = (jj / R) % 10;
					if( align ){
						sprintf( fmt, "%%-%ii", width );
					}else{
						sprintf( fmt, "%%%ii", width );
					}
					snprintf( buf, width+1, fmt, d );
					DaoStream_SetColor( stream, "white", (s%2) ? "yellow" : "cyan" );
					DaoStream_WriteChars( stream, "  " );
					DaoStream_WriteChars( stream, buf );
					DaoStream_SetColor( stream, NULL, NULL );
				}
				DaoStream_WriteChars( stream, "\n" );
			}

			for(g=0; g<original->labels[DAOX_DF_COL]->size; ++g){
				sprintf( fmt, "%%-%is", width );
				sprintf( buf, fmt, j == 0 ? "|" : ">" );
				DaoStream_WriteChars( stream, buf );
				for(s=j; s<J; ++s){
					daoint jj = DaoSlice_GetIndex( self->slices, 1, s );
					int width = clabwidth->data.ints[s];
					int align = aligns->data.ints[s];
					if( align ){
						sprintf( fmt, "%%-%is", width );
					}else{
						sprintf( fmt, "%%%is", width );
					}
					DaoxDataFrame_GetLabel( original, DAOX_DF_COL, g, jj, label );
					if( label->size > width ) DString_Reset( label, width );
					snprintf( buf, width+1, fmt, label->chars );
					DaoStream_SetColor( stream, "blue", (s%2) ? "yellow" : "cyan" );
					DaoStream_WriteChars( stream, "  " );
					DaoStream_WriteChars( stream, buf );
					DaoStream_SetColor( stream, NULL, NULL );
				}
				DaoStream_WriteChars( stream, "\n" );
			}
			DaoStream_WriteChars( stream, j == 0 ? "|" : ">" );
			while( --width2 ) DaoStream_WriteChars( stream, "-" );
			DaoStream_WriteChars( stream, J < M ? ">" : "|" );
			DaoStream_WriteChars( stream, "\n" );
			for(i=0; i<N; ++i){
				daoint ii = DaoSlice_GetIndex( self->slices, 0, i );
				sprintf( buf, idfmt, ii );
				DaoStream_SetColor( stream, "white", (i%2) ? "yellow" : "cyan" );
				DaoStream_WriteChars( stream, buf );
				DaoStream_SetColor( stream, NULL, NULL );
				for(g=0; g<original->labels[DAOX_DF_ROW]->size; ++g){
					int width = rlabwidth->data.ints[g];
					DaoxDataFrame_GetLabel( original, DAOX_DF_ROW, g, ii, label );
					if( label->size > width ) DString_Reset( label, width );
					DaoStream_SetColor( stream, "white", (i%2) ? "yellow" : "cyan" );
					if( g ) DaoStream_WriteChars( stream, "," );
					DaoStream_SetColor( stream, NULL, NULL );
					sprintf( fmt, "%%-%is", width );
					snprintf( buf, width+1, fmt, label->chars );
					DaoStream_SetColor( stream, "blue", (i%2) ? "yellow" : "cyan" );
					DaoStream_WriteChars( stream, buf );
					DaoStream_SetColor( stream, NULL, NULL );
				}
				DaoStream_SetColor( stream, "white", (i%2) ? "yellow" : "cyan" );
				DaoStream_WriteChars( stream, g > 0 ? ": " : " " );
				DaoStream_SetColor( stream, NULL, NULL );
				for(s=j; s<J; ++s){
					int scifmt = scifmts->data.ints[s];
					int dec = decimals->data.ints[s];
					int width = clabwidth->data.ints[s];
					daoint jj = DaoSlice_GetIndex( self->slices, 2, s );
					DaoxDataColumn *col = (DaoxDataColumn*) original->columns->items.pVoid[jj];
					DaoValue *value = DaoxDataColumn_GetCell( col, i, & valueBuffer );

					DaoStream_WriteChars( stream, "  " );
					if( value == NULL ){
						sprintf( fmt, "%%-%is", width );
						snprintf( buf, width+1, fmt, " " );
					}else if( value->type == DAO_INTEGER ){
						sprintf( fmt, "%%%i%s", width, DAO_INT_FORMAT );
						snprintf( buf, width+1, fmt, value->xInteger.value );
					}else if( value->type == DAO_FLOAT ){
						double f = DaoValue_GetFloat( value );
						if( scifmt ){
							sprintf( fmt, "%%%iE", width );
						}else{
							sprintf( fmt, "%%%i.%if", width, dec );
						}
						snprintf( buf, width+1, fmt, f );
					}else if( value->type == DAO_COMPLEX ){
						dao_complex com = value->xComplex.value;
						char s = com.imag>=0 ? '+' : '-';
						int w = width/2-2;
						int d = dec;
						if( scifmt ){
							sprintf( fmt, "(%%%i.3E,%%%i.3E)", w, w );
						}else{
							sprintf( fmt, "(%%%i.%if,%%%i.%if)", w, d, w, d );
						}
						snprintf( buf, width, fmt, com.real, com.imag );
					}else{
						DString_Reset( sstream->streamString, 0 );
						DaoValue_Print( value, proc, sstream, NULL );
						DString_Reset( label, 0 );
						DString_Append( label, sstream->streamString );
						if( label->size > width ) DString_Reset( label, width );
						DString_Change( label, "%t", "\\t", 0 );
						DString_Change( label, "%n", "\\n", 0 );
						sprintf( fmt, "%%-%is", width );
						snprintf( buf, width+1, fmt, label->chars );
					}
					DaoStream_WriteChars( stream, buf );
				}
				DaoStream_WriteChars( stream, "\n" );
			}
			DaoStream_WriteChars( stream, "\n" );
		}
	}
	DaoStream_Delete( sstream );
	DArray_Delete( aligns );
	DArray_Delete( scifmts );
	DArray_Delete( decimals );
	DArray_Delete( rlabwidth );
	DArray_Delete( clabwidth );
	DString_Delete( label );
}

static void FRAME_CellsCodeSection( DaoProcess *proc, DaoValue *p[], int npar, int func )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DaoxDataFrame *original = self->original;
	DaoVmCode *sect = DaoProcess_InitCodeSection( proc, 4 );
	DaoInteger integer1 = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger integer2 = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger integer3 = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger *rowidx = & integer1;
	DaoInteger *colidx = & integer2;
	DaoInteger *depidx = & integer3;
	DaoValue *nulls[3] = {NULL,NULL,NULL};
	DaoValue value;
	daoint N, M, K;
	daoint entry, i, j, k;

	value.xInteger = integer1;
	if( sect == NULL ) return;

	if( self->original == NULL ){
		DaoxDataFrame_PrepareSlices( self );
		DaoDataFrame_MakeSlice( self, proc, nulls, 3, self->slices );
		original = self;
	}

	N = DaoSlice_GetSize( self->slices, 0 );
	M = DaoSlice_GetSize( self->slices, 1 );
	K = DaoSlice_GetSize( self->slices, 2 );

	entry = proc->topFrame->entry;
	for(k=0; k<K; ++k){
		daoint kk = DaoSlice_GetIndex( self->slices, 2, k );
		depidx->value = kk;
		for(j=0; j<M; ++j){
			daoint jj = DaoSlice_GetIndex( self->slices, 1, j );
			DaoxDataColumn *column = (DaoxDataColumn*) original->columns->items.pVoid[jj];
			colidx->value = jj;
			for(i=0; i<N; ++i){
				daoint ii = DaoSlice_GetIndex( self->slices, 0, i );
				rowidx->value = ii;
				if( sect->b >0 ){
					DaoValue *cell = DaoxDataColumn_GetCell( column, kk*N+ii, & value );
					DaoProcess_SetValue( proc, sect->a, cell );
				}
				if( sect->b >1 ) DaoProcess_SetValue( proc, sect->a+1, (DaoValue*) rowidx );
				if( sect->b >2 ) DaoProcess_SetValue( proc, sect->a+2, (DaoValue*) colidx );
				if( sect->b >3 ) DaoProcess_SetValue( proc, sect->a+3, (DaoValue*) depidx );
				proc->topFrame->entry = entry;
				DaoProcess_Execute( proc );
				if( proc->status == DAO_PROCESS_ABORTED ) break;
				if( func ){
					DaoxDataColumn_SetCell( column, kk*N+ii, proc->stackValues[0] );
				}
			}
		}
	}
	DaoProcess_PopFrame( proc );
}
static void FRAME_RowsCodeSection( DaoProcess *proc, DaoValue *p[], int npar, int func )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DaoxDataFrame *original = self->original;
	DaoNamespace *ns = proc->activeNamespace;
	DaoVmCode *sect = DaoProcess_InitCodeSection( proc, 3 );
	DaoInteger integer1 = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger integer3 = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger *rowidx = & integer1;
	DaoInteger *depidx = & integer3;
	DaoValue *nulls[3] = {NULL,NULL,NULL};
	DaoValue value;
	DaoTuple *tuple;
	DaoType *type;
	DList *ts;
	daoint N, M, K;
	daoint entry, i, j, k;

	value.xInteger = integer1;
	if( sect == NULL ) return;

	if( self->original == NULL ){
		DaoxDataFrame_PrepareSlices( self );
		DaoDataFrame_MakeSlice( self, proc, nulls, 3, self->slices );
		original = self;
	}

	N = DaoSlice_GetSize( self->slices, 0 );
	M = DaoSlice_GetSize( self->slices, 1 );
	K = DaoSlice_GetSize( self->slices, 2 );

	ts = DList_New(0);
	for(j=0; j<M; ++j){
		daoint jj = DaoSlice_GetIndex( self->slices, 1, j );
		DaoxDataColumn *column = (DaoxDataColumn*) original->columns->items.pVoid[jj];
		DList_Append( ts, column->vatype );
	}
	type = DaoNamespace_MakeType( ns, "tuple", DAO_TUPLE, NULL, ts->items.pType, ts->size );
	DList_Delete( ts );

	tuple = DaoTuple_Create( type, 0, 0 );
	GC_IncRC( tuple );

	entry = proc->topFrame->entry;
	for(k=0; k<K; ++k){
		daoint kk = DaoSlice_GetIndex( self->slices, 2, k );
		depidx->value = kk;
		for(i=0; i<N; ++i){
			daoint ii = DaoSlice_GetIndex( self->slices, 0, i );
			rowidx->value = ii;
			for(j=0; j<M; ++j){
				daoint jj = DaoSlice_GetIndex( self->slices, 1, j );
				DaoxDataColumn *column = (DaoxDataColumn*) original->columns->items.pVoid[jj];
				DaoValue *cell = DaoxDataColumn_GetCell( column, kk*N+ii, & value );
				DaoTuple_SetItem( tuple, cell, j );
			}
			if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, (DaoValue*) tuple );
			if( sect->b >1 ) DaoProcess_SetValue( proc, sect->a+1, (DaoValue*) rowidx );
			if( sect->b >2 ) DaoProcess_SetValue( proc, sect->a+2, (DaoValue*) depidx );
			proc->topFrame->entry = entry;
			DaoProcess_Execute( proc );
			if( proc->status == DAO_PROCESS_ABORTED ) break;
			if( func == 0 ) continue;
			for(j=0; j<M; ++j){
				daoint jj = DaoSlice_GetIndex( self->slices, 1, j );
				DaoxDataColumn *column = (DaoxDataColumn*) original->columns->items.pVoid[jj];
				DaoxDataColumn_SetCell( column, kk*N+ii, tuple->values[j] );
			}
		}
	}
	DaoProcess_PopFrame( proc );
	GC_DecRC( tuple );
}
static void FRAME_ColsCodeSection( DaoProcess *proc, DaoValue *p[], int npar, int func )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DaoxDataFrame *original = self->original;
	DaoNamespace *ns = proc->activeNamespace;
	DaoVmCode *sect = DaoProcess_InitCodeSection( proc, 3 );
	DaoInteger integer2 = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger integer3 = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger *colidx = & integer2;
	DaoInteger *depidx = & integer3;
	DaoValue *nulls[3] = {NULL,NULL,NULL};
	DaoValue value;
	DaoList *list;
	daoint N, M, K;
	daoint entry, i, j, k;

	value.xInteger = integer2;
	if( sect == NULL ) return;

	if( self->original == NULL ){
		DaoxDataFrame_PrepareSlices( self );
		DaoDataFrame_MakeSlice( self, proc, nulls, 3, self->slices );
		original = self;
	}

	N = DaoSlice_GetSize( self->slices, 0 );
	M = DaoSlice_GetSize( self->slices, 1 );
	K = DaoSlice_GetSize( self->slices, 2 );

	list = DaoList_New();
	GC_IncRC( list );

	entry = proc->topFrame->entry;
	for(k=0; k<K; ++k){
		daoint kk = DaoSlice_GetIndex( self->slices, 2, k );
		depidx->value = kk;
		for(j=0; j<M; ++j){
			daoint jj = DaoSlice_GetIndex( self->slices, 1, j );
			DaoxDataColumn *column = (DaoxDataColumn*) original->columns->items.pVoid[jj];
			DaoType *type = DaoNamespace_MakeType( ns, "list", DAO_LIST, NULL, & column->vatype, 1 );
			colidx->value = jj;
			DaoList_Clear( list );
			GC_Assign( & list->ctype, type );
			for(i=0; i<N; ++i){
				daoint ii = DaoSlice_GetIndex( self->slices, 0, i );
				DaoValue *cell = DaoxDataColumn_GetCell( column, kk*N+ii, & value );
				DaoList_Append( list, cell );
			}
			if( sect->b >0 ) DaoProcess_SetValue( proc, sect->a, (DaoValue*) list );
			if( sect->b >1 ) DaoProcess_SetValue( proc, sect->a+1, (DaoValue*) colidx );
			if( sect->b >2 ) DaoProcess_SetValue( proc, sect->a+2, (DaoValue*) depidx );
			proc->topFrame->entry = entry;
			DaoProcess_Execute( proc );
			if( proc->status == DAO_PROCESS_ABORTED ) break;
			if( func == 0 ) continue;
			for(i=0; i<N; ++i){
				daoint ii = DaoSlice_GetIndex( self->slices, 0, i );
				DaoxDataColumn_SetCell( column, kk*N+ii, list->value->items.pValue[i] );
			}
		}
	}
	DaoProcess_PopFrame( proc );
	GC_DecRC( list );
}
static void FRAME_ScanCells( DaoProcess *proc, DaoValue *p[], int npar )
{
	FRAME_CellsCodeSection( proc, p, npar, 0 );
}
static void FRAME_UpdateCells( DaoProcess *proc, DaoValue *p[], int npar )
{
	FRAME_CellsCodeSection( proc, p, npar, 1 );
}
static void FRAME_ScanRows( DaoProcess *proc, DaoValue *p[], int npar )
{
	FRAME_RowsCodeSection( proc, p, npar, 0 );
}
static void FRAME_UpdateRows( DaoProcess *proc, DaoValue *p[], int npar )
{
	FRAME_RowsCodeSection( proc, p, npar, 1 );
}
static void FRAME_ScanCols( DaoProcess *proc, DaoValue *p[], int npar )
{
	FRAME_ColsCodeSection( proc, p, npar, 0 );
}
static void FRAME_UpdateCols( DaoProcess *proc, DaoValue *p[], int npar )
{
	FRAME_ColsCodeSection( proc, p, npar, 1 );
}

static void FRAME_AddFrame( DaoProcess *proc, DaoValue *P[], int N )
{
	DaoxDataFrame_UpdateByFrame( (DaoxDataFrame*) P[0], (DaoxDataFrame*) P[1], DVM_ADD );
}
static void FRAME_SubFrame( DaoProcess *proc, DaoValue *P[], int N )
{
	DaoxDataFrame_UpdateByFrame( (DaoxDataFrame*) P[0], (DaoxDataFrame*) P[1], DVM_SUB );
}
static void FRAME_MulFrame( DaoProcess *proc, DaoValue *P[], int N )
{
	DaoxDataFrame_UpdateByFrame( (DaoxDataFrame*) P[0], (DaoxDataFrame*) P[1], DVM_MUL );
}
static void FRAME_DivFrame( DaoProcess *proc, DaoValue *P[], int N )
{
	DaoxDataFrame_UpdateByFrame( (DaoxDataFrame*) P[0], (DaoxDataFrame*) P[1], DVM_DIV );
}
static void FRAME_ModFrame( DaoProcess *proc, DaoValue *P[], int N )
{
	DaoxDataFrame_UpdateByFrame( (DaoxDataFrame*) P[0], (DaoxDataFrame*) P[1], DVM_MOD );
}
static void FRAME_BitAndFrame( DaoProcess *proc, DaoValue *P[], int N )
{
	DaoxDataFrame_UpdateByFrame( (DaoxDataFrame*) P[0], (DaoxDataFrame*) P[1], DVM_BITAND );
}
static void FRAME_BitOrFrame( DaoProcess *proc, DaoValue *P[], int N )
{
	DaoxDataFrame_UpdateByFrame( (DaoxDataFrame*) P[0], (DaoxDataFrame*) P[1], DVM_BITOR );
}
static void FRAME_BitXorFrame( DaoProcess *proc, DaoValue *P[], int N )
{
	DaoxDataFrame_UpdateByFrame( (DaoxDataFrame*) P[0], (DaoxDataFrame*) P[1], DVM_BITXOR );
}

static void FRAME_UpdateArray( DaoProcess *proc, DaoValue *P[], int N, int oper )
{
	int rc = DaoxDataFrame_UpdateByArray( (DaoxDataFrame*) P[0], (DaoArray*) P[1], oper );
	if( rc == DAOX_DF_WRONG_SHAP ){
		DaoProcess_RaiseError( proc, "Index", "Invalid shape" );
	}else if( rc == DAOX_DF_WRONG_VALUE ){
		DaoProcess_RaiseError( proc, "Value", "Invalid value" );
	}
}
static void FRAME_AddArray( DaoProcess *proc, DaoValue *P[], int N )
{
	FRAME_UpdateArray( proc, P, N, DVM_ADD );
}
static void FRAME_SubArray( DaoProcess *proc, DaoValue *P[], int N )
{
	FRAME_UpdateArray( proc, P, N, DVM_SUB );
}
static void FRAME_MulArray( DaoProcess *proc, DaoValue *P[], int N )
{
	FRAME_UpdateArray( proc, P, N, DVM_MUL );
}
static void FRAME_DivArray( DaoProcess *proc, DaoValue *P[], int N )
{
	FRAME_UpdateArray( proc, P, N, DVM_DIV );
}
static void FRAME_ModArray( DaoProcess *proc, DaoValue *P[], int N )
{
	FRAME_UpdateArray( proc, P, N, DVM_MOD );
}
static void FRAME_BitAndArray( DaoProcess *proc, DaoValue *P[], int N )
{
	FRAME_UpdateArray( proc, P, N, DVM_BITAND );
}
static void FRAME_BitOrArray( DaoProcess *proc, DaoValue *P[], int N )
{
	FRAME_UpdateArray( proc, P, N, DVM_BITOR );
}
static void FRAME_BitXorArray( DaoProcess *proc, DaoValue *P[], int N )
{
	FRAME_UpdateArray( proc, P, N, DVM_BITXOR );
}


static void FRAME_SLICED( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoxDataFrame *self = (DaoxDataFrame*) proc;
	DaoxDataFrame_Sliced( self );
}


static DaoFuncItem dataframeMeths[]=
{
	{ FRAME_New,         "DataFrame()=>DataFrame" },
	{ FRAME_NewMatrix,   "DataFrame( mat : array<@T> )=>DataFrame" },

	{ FRAME_FromMatrix,  "FromMatrix( self :DataFrame, mat : array<@T> )" },

	{ FRAME_Size,      "Size( self :DataFrame )=>int" },
	{ FRAME_UseLabels, "UseLabels( self :DataFrame, dim :DataFrame_DimType, group :int )" },
	{ FRAME_AddLabels, "AddLabels( self :DataFrame, dim :DataFrame_DimType, labels :map<string,int> )" },
	{ FRAME_AddLabel,  "AddLabel( self :DataFrame, dim :DataFrame_DimType, label :string, index :int )" },
	{ FRAME_GetIndex,  "GetIndex( self :DataFrame, dim :DataFrame_DimType, label :string ) => int" },

	{ FRAME_AddArrayCol, "AddColumn( self :DataFrame, data :array<@T>, label :string =\"\" )" },
	{ FRAME_AddListCol,  "AddColumn( self :DataFrame, data :list<@T>, label :string =\"\" )" },

	{ FRAME_GETMI,
		"[]( self :DataFrame, i :DataFrame_IndexType, j :DataFrame_IndexType =none, k :DataFrame_IndexType =none ) => any" },
	{ FRAME_SETMI,
		"[]=( self :DataFrame, value :any, i :DataFrame_IndexType, j :DataFrame_IndexType =none, k :DataFrame_IndexType =none )" },

	{ FRAME_PRINT,  "Print( self :DataFrame )" },
	{ FRAME_PRINT,  "__PRINT__( self :DataFrame )" },

	{ FRAME_ScanCells,  "ScanCells( self :DataFrame )[cell:@T,row:int,column:int,depth:int]" },
	{ FRAME_UpdateCells, "UpdateCells( self :DataFrame )[cell:@T,row:int,column:int,depth:int=>@T]" },

	{ FRAME_ScanRows,  "ScanRows( self :DataFrame )[value:tuple,row:int,depth:int]" },
	{ FRAME_UpdateRows, "UpdateRows( self :DataFrame )[value:tuple,row:int,depth:int=>tuple]" },

	{ FRAME_ScanCols,  "ScanColumns( self :DataFrame )[value:tuple,column:int,depth:int]" },
	{ FRAME_UpdateCols, "UpdateColumns( self :DataFrame )[value:tuple,column:int,depth:int=>tuple]" },

	{ FRAME_AddFrame,     "+=( self :DataFrame, other :DataFrame )" },
	{ FRAME_SubFrame,     "-=( self :DataFrame, other :DataFrame )" },
	{ FRAME_MulFrame,     "*=( self :DataFrame, other :DataFrame )" },
	{ FRAME_DivFrame,     "/=( self :DataFrame, other :DataFrame )" },
	{ FRAME_ModFrame,     "%=( self :DataFrame, other :DataFrame )" },
	{ FRAME_BitAndFrame,  "&=( self :DataFrame, other :DataFrame )" },
	{ FRAME_BitOrFrame,   "|=( self :DataFrame, other :DataFrame )" },
	{ FRAME_BitXorFrame,  "^=( self :DataFrame, other :DataFrame )" },

	{ FRAME_AddArray,     "+=( self :DataFrame, other :array<@T> )" },
	{ FRAME_SubArray,     "-=( self :DataFrame, other :array<@T> )" },
	{ FRAME_MulArray,     "*=( self :DataFrame, other :array<@T> )" },
	{ FRAME_DivArray,     "/=( self :DataFrame, other :array<@T> )" },
	{ FRAME_ModArray,     "%=( self :DataFrame, other :array<@T> )" },
	{ FRAME_BitAndArray,  "&=( self :DataFrame, other :array<@T> )" },
	{ FRAME_BitOrArray,   "|=( self :DataFrame, other :array<@T> )" },
	{ FRAME_BitXorArray,  "^=( self :DataFrame, other :array<@T> )" },

	{ FRAME_SLICED,  "__SLICED__" },

	{ NULL, NULL },
};

static void DaoxDataFrame_GetGCFields( void *p, DList *values, DList *arrays, DList *m, int rm )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p;
	DList_Append( arrays, self->columns );
	DList_Append( arrays, self->caches );
	if( self->original ) DList_Append( values, self->original );
	if( rm ) self->original = NULL;
}

DaoTypeBase dataframeTyper =
{
	"DataFrame", NULL, NULL, (DaoFuncItem*) dataframeMeths, {0}, {0},
	(FuncPtrDel)DaoxDataFrame_Delete, DaoxDataFrame_GetGCFields
};


DAO_DLL int DaoDataframe_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	daox_type_datacolumn = DaoNamespace_WrapType( ns, & datacolumnTyper, 0 );
	daox_type_dataframe = DaoNamespace_WrapType( ns, & dataframeTyper, 0 );
	DaoNamespace_TypeDefine( ns, "enum<row,column,depth>", "DataFrame_DimType" );
	DaoNamespace_TypeDefine( ns, "none|int|string|tuple<none,none>|tuple<int,int>|tuple<string,string>|tuple<int|string,none>|tuple<none,int|string>", "DataFrame_IndexType" );
	return 0;
}
