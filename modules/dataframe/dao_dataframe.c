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

#include <stdlib.h>
#include <string.h>
#include "dao_dataframe.h"
#include "daoProcess.h"
#include "daoNumtype.h"
#include "daoValue.h"
#include "daoType.h"
#include "daoGC.h"

enum { SLICE_RANGE, SLICE_ENUM };


DaoType *daox_type_dataframe;

static int DaoType_GetDataSize( DaoType *self )
{
	switch( self->tid ){
	case DAO_INTEGER : return sizeof(daoint);
	case DAO_FLOAT   : return sizeof(float);
	case DAO_DOUBLE  : return sizeof(double);
	case DAO_COMPLEX : return sizeof(complex16);
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
	case DAO_DOUBLE  :
	case DAO_COMPLEX :
	case DAO_STRING  : break;
	default : datatype = 0; break;  /* zero for DaoValue* items; */
	}
	return datatype;
}


DaoxDataColumn* DaoxDataColumn_New( DaoType *type )
{
	DaoxDataColumn *self = (DaoxDataColumn*) dao_calloc( 1, sizeof(DaoxDataColumn) );
	if( type == NULL ) type = dao_type_any;
	GC_IncRC( type );
	self->type = type;
	self->cells = DVector_New( DaoType_GetDataSize( type ) );
	self->cells->type = DaoType_GetDataType( type );
	return self;
}
void DaoxDataColumn_Delete( DaoxDataColumn *self )
{
	DaoxDataColumn_Reset( self, 0 );
	DVector_Delete( self->cells );
	GC_DecRC( self->type );
	dao_free( self );
}
void DaoxDataColumn_Reset( DaoxDataColumn *self, daoint size )
{
	daoint i;
	if( size < self->cells->size ){
		for(i=size; i<self->cells->size; ++i){
			if( self->cells->type == DAO_STRING ){
				DString_Clear( self->cells->data.strings + i );
			}else if( self->cells->type == 0 ){
				GC_DecRC( self->cells->data.values[i] );
			}
		}
		DVector_Reset( self->cells, size );
	}else if( size > self->cells->size ){
		DVector_Reserve( self->cells, size );
		for(i=self->cells->size; i<size; ++i){
			if( self->cells->type == DAO_STRING ){
				DString_Init( self->cells->data.strings + i, 0 );
			}else if( self->cells->type == 0 ){
				self->cells->data.values[i]  = NULL;
			}
		}
		DVector_Reset( self->cells, size );
	}
}
void DaoxDataColumn_SetType( DaoxDataColumn *self, DaoType *type )
{
	int datatype, datasize;

	DaoxDataColumn_Reset( self, 0 );
	if( type == NULL ) type = dao_type_any;
	datatype = DaoType_GetDataType( type );
	datasize = DaoType_GetDataSize( type );

	GC_ShiftRC( type, self->type );
	self->type = type;
	self->cells->capacity = (self->cells->capacity * self->cells->stride) / datasize;
	self->cells->stride = datasize;
	self->cells->type = datatype;
}
void DaoxDataColumn_SetCell( DaoxDataColumn *self, daoint i, DaoValue *value )
{
	switch( self->type->tid ){
	default :
		GC_ShiftRC( value, self->cells->data.values[i] );
		self->cells->data.values[i] = value;
		break;
	case DAO_INTEGER : self->cells->data.daoints[i]   = DaoValue_GetInteger( value ); break;
	case DAO_FLOAT   : self->cells->data.floats[i]    = DaoValue_GetFloat( value );  break;
	case DAO_DOUBLE  : self->cells->data.doubles[i]   = DaoValue_GetDouble( value ); break;
	case DAO_COMPLEX : self->cells->data.complexes[i] = DaoValue_GetComplex( value ) ; break;
	case DAO_STRING  : DaoValue_GetString( value, & self->cells->data.strings[i] ); break;
	}
}
DaoValue* DaoxDataColumn_GetCell( DaoxDataColumn *self, daoint i, DaoValue *value )
{
	value->xNone.type = self->type->tid;
	switch( self->type->tid ){
	case DAO_INTEGER : value->xInteger.value = self->cells->data.daoints[i]; break;
	case DAO_FLOAT   : value->xFloat.value   = self->cells->data.floats[i];  break;
	case DAO_DOUBLE  : value->xDouble.value  = self->cells->data.doubles[i]; break;
	case DAO_COMPLEX : value->xComplex.value = self->cells->data.complexes[i]; break;
	case DAO_STRING  : value->xString.data   = & self->cells->data.strings[i]; break;
	default : value = self->cells->data.values[i]; break;
	}
	return value;
}


DaoxDataFrame* DaoxDataFrame_New()
{
	int i;
	DaoxDataFrame *self = (DaoxDataFrame*) dao_calloc( 1, sizeof(DaoxDataFrame) );
	DaoCstruct_Init( (DaoCstruct*) self, daox_type_dataframe );
	for(i=0; i<3; ++i){
		self->dims[i] = 0;
		self->groups[i] = 0;
		self->labels[i] = DArray_New(D_MAP);
	}
	self->columns = DArray_New(0);
	self->caches = DArray_New(0);
	self->original = NULL;
	self->slices = NULL;
	return self;
}
void DaoxDataFrame_Delete( DaoxDataFrame *self )
{
	int i;
	for(i=0; i<3; ++i) DArray_Delete( self->labels[i] );
	if( self->slices ) DArray_Delete( self->slices );
	DArray_Delete( self->columns );
	DArray_Delete( self->caches );
	GC_DecRC( self->original );
	dao_free( self );
}

void DaoxDataFrame_Clear( DaoxDataFrame *self )
{
	daoint i;
	DaoxDataFrame_Reset( self );
	for(i=0; i<self->caches->size; ++i){
		DaoxDataColumn_Delete( (DaoxDataColumn*) self->caches->items.pVoid[i] );
	}
	DArray_Clear( self->caches );
}
void DaoxDataFrame_Reset( DaoxDataFrame *self )
{
	daoint i;
	for(i=0; i<self->columns->size; ++i){
		DArray_Append( self->caches, self->columns->items.pVoid[i] );
	}
	for(i=0; i<3; ++i){
		self->dims[i] = 0;
		self->groups[i] = 0;
		DArray_Clear( self->labels[i] );
	}
	GC_DecRC( self->original );
	self->original = NULL;
	DArray_Clear( self->columns );
}
DaoxDataColumn* DaoxDataFrame_MakeColumn( DaoxDataFrame *self, DaoType *type )
{
	DaoxDataColumn *column = (DaoxDataColumn*) DArray_Back( self->caches );
	if( self->caches->size ){
		DaoxDataColumn_SetType( column, type );
		DArray_PopBack( self->caches );
		return column;
	}
	return DaoxDataColumn_New( type );
}

int DaoxDataFrame_FromMatrix( DaoxDataFrame *self, DaoArray *mat )
{
	DaoxDataColumn *col;
	DaoType *etype = dao_array_types[ mat->etype ]->nested->items.pType[0];
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
		DArray_Append( self->columns, col );
		DaoxDataColumn_Reset( col, N*K );
		for(i=0; i<N; ++i){
			for(k=0; k<K; ++k){
				daoint id2 = k * N + i;
				daoint id3 = i * MK + j * K + k;
				switch( mat->etype ){
				case DAO_INTEGER : col->cells->data.daoints[id2]   = mat->data.i[id3]; break;
				case DAO_FLOAT   : col->cells->data.floats[id2]    = mat->data.f[id3]; break;
				case DAO_DOUBLE  : col->cells->data.doubles[id2]   = mat->data.d[id3]; break;
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
		DArray *labs = self->labels[dim];
		if( group >= labs->size ) group = labs->size - 1;
		if( group < 0 ) group = 0;
		self->groups[dim] = group;
	}
}
void DaoxDataFrame_AddLabelGroup( DaoxDataFrame *self, int dim )
{
	if( dim >=0 && dim < 3 ){
		DMap *labmap = DHash_New(D_STRING,0);
		DArray *labels = self->labels[dim];
		self->groups[dim] = labels->size;
		DArray_Append( labels, labmap );
		DMap_Delete( labmap );
	}
}
void DaoxDataFrame_AddLabel( DaoxDataFrame *self, int dim, const char *lab, daoint idx )
{
	DString slab = DString_WrapMBS( lab );
	DMap *labmap = NULL;
	if( dim < 0 || dim >= 3 ) return;
	if( idx < 0 || idx >= self->dims[dim] ) return;
	if( self->groups[dim] >= self->labels[dim]->size ) return;
	labmap = self->labels[dim]->items.pMap[ self->groups[dim] ];
	if( labmap != NULL ) DMap_Insert( labmap, & slab, (void*)(size_t) idx );
}
daoint DaoxDataFrame_GetIndex( DaoxDataFrame *self, int dim, const char *label )
{
	DString slab = DString_WrapMBS( label );
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
	if( labels->keytype != D_STRING && labels->keytype != D_VALUE ) return;
	if( labels->valtype != 0 && labels->valtype != D_VALUE ) return;
	lab = DString_New(1);
	DaoxDataFrame_AddLabelGroup( self, dim );
	for(it=DMap_First(labels); it; it=DMap_Next(labels,it)){
		DString *lab2 = it->key.pString;
		daoint idx = it->value.pInt;
		if( labels->keytype == D_VALUE ){
			if( it->key.pValue->type != DAO_STRING ) continue;
			lab2 = it->key.pValue->xString.data;
		}
		if( labels->valtype == D_VALUE ){
			if( it->value.pValue->type != DAO_INTEGER ) continue;
			idx = it->value.pValue->xInteger.value;
		}
		if( idx < 0 ) continue;
		DString_Reset( lab, 0 );
		DString_Append( lab, lab2 );
		DaoxDataFrame_AddLabel( self, dim, lab->mbs, idx );
	}
	DString_Delete( lab );
}
static void DaoxDataFrame_PrepareSlices( DaoxDataFrame *self )
{
	if( self->slices != NULL ) return;
	self->slices = DArray_New(D_VECTOR);
}
static void DaoxDataFrame_SliceFrom( DaoxDataFrame *self, DaoxDataFrame *orig, DArray *slices )
{
}
void DaoxDataFrame_Sliced( DaoxDataFrame *self )
{
}

void DaoxDataFrame_Encode( DaoxDataFrame *self, DString *output )
{
}
void DaoxDataFrame_Decode( DaoxDataFrame *self, DString *input )
{
}




static int SliceRange( DVector *slice, daoint N, daoint first, daoint last )
{
	DVector_Resize( slice, 3 );
	slice->data.daoints[0] = SLICE_RANGE;
	slice->data.daoints[1] = 0;
	slice->data.daoints[2] = 0;
	if( first <0 ) first += N;
	if( last <0 ) last += N;
	if( first <0 || first >= N || last <0 || last >= N ) return 0;
	slice->data.daoints[2] = first;
	if( first <= last ) slice->data.daoints[1] = last - first + 1;
	return 1;
}
static int SliceRange2( DVector *slice, daoint N, daoint first, daoint count )
{
	DVector_Resize( slice, 3 );
	slice->data.daoints[0] = SLICE_RANGE;
	slice->data.daoints[1] = 0;
	slice->data.daoints[2] = 0;
	if( first <0 ) first += N;
	if( first <0 || first >= N ) return 0;
	slice->data.daoints[2] = first;
	if( first + count > N ) return 0;
	slice->data.daoints[1] = count;
	return 1;
}
static void MakeSlice( DaoProcess *proc, DaoValue *pid, daoint N, DVector *slice )
{
	daoint j, id, from, to, rc = 1;
	if( pid == NULL || pid->type == 0 ){
		SliceRange2( slice, N, 0, N );
		return;
	}
	switch( pid->type ){
	case DAO_INTEGER :
	case DAO_FLOAT :
	case DAO_DOUBLE :
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
			DaoValue **data = pid->xTuple.items;
			DVector_Clear( slice );
			if( data[0]->type == DAO_INTEGER && data[1]->type == DAO_INTEGER ){
				from = data[0]->xInteger.value;
				to   = data[1]->xInteger.value;
				rc = SliceRange( slice, N, from, to );
			}else if( data[0]->type == DAO_NONE && data[1]->type == DAO_NONE ){
				rc = SliceRange2( slice, N, 0, N );
			}else if( data[0]->type <= DAO_DOUBLE && data[1]->type == DAO_NONE ){
				from = DaoValue_GetInteger( data[0] );
				rc = SliceRange( slice, N, from, -1 );
			}else if( data[0]->type == DAO_NONE && data[1]->type <= DAO_DOUBLE ){
				to = DaoValue_GetInteger( data[1] );
				rc = SliceRange( slice, N, 0, to );
			}else if( data[0]->type == DAO_STRING && data[1]->type == DAO_STRING ){
			}else if( data[0]->type == DAO_STRING && data[1]->type == DAO_NONE ){
			}else if( data[0]->type == DAO_NONE && data[1]->type == DAO_STRING ){
			}else{
				DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "need number" );
			}
			break;
		}
	case DAO_LIST :
		{
			DaoList *list = & pid->xList;
			DaoValue **v = list->items.items.pValue;
			DVector_Resize( slice, list->items.size + 2 );
			slice->data.daoints[0] = SLICE_ENUM;
			slice->data.daoints[1] = list->items.size;
			for( j=0; j<list->items.size; j++){
				if( v[j]->type < DAO_INTEGER || v[j]->type > DAO_DOUBLE )
					DaoProcess_RaiseException( proc, DAO_ERROR_INDEX, "need number" );
				id = DaoValue_GetInteger( v[j] );
				if( id <0 ) id += N;
				if( id <0 || id >= N ){
					rc = id = 0;
					break;
				}
				slice->data.daoints[j+2] = id;
			}
			break;
		}
	case DAO_ARRAY :
		{
			DaoArray *na = & pid->xArray;
			daoint *p;

			if( na->etype == DAO_COMPLEX ){
				DaoProcess_RaiseException( proc, DAO_ERROR_INDEX,
						"complex array can not be used as index" );
				break;
			}
			DVector_Resize( slice, na->size + 2 );
			slice->data.daoints[0] = SLICE_ENUM;
			slice->data.daoints[1] = na->size;
			p = slice->data.daoints + 2;
			for( j=0; j<na->size; j++){
				id = DaoArray_GetInteger( na, j );
				if( id <0 ) id += N;
				if( id <0 || id >= N ){
					rc = id = 0;
					break;
				}
				p[j] = id;
			}
			break;
		}
	default: break;
	}
	if( slice->size < 2 ) SliceRange2( slice, N, 0, N );
	if( rc == 0 ) DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, "" );
}
static int DaoDataFrame_MakeSlice( DaoxDataFrame *self, DaoProcess *proc, DaoValue *idx[], int N, DArray *slices )
{
	DVector *tmp = DVector_New( sizeof(daoint) );
	daoint *dims = self->dims;
	daoint i, D = 3, S = 1;

	/* slices: DArray<DVector<int> > */
	DArray_Clear( slices );
	DVector_Resize( tmp, 3 );
	tmp->data.daoints[0] = SLICE_RANGE;
	tmp->data.daoints[2] = 0;
	for(i=0; i<D; ++i){
		tmp->data.daoints[1] = dims[i];
		DArray_Append( slices, tmp );
	}
	DVector_Delete( tmp );
	if( N > D ){
		DaoProcess_RaiseException( proc, DAO_WARNING, "too many indices" );
		N = D;
	}
	for(i=0; i<N; ++i) MakeSlice( proc, idx[i], (int)dims[i], slices->items.pVector[i] );
	for(i=0; i<D; ++i) S *= slices->items.pVector[i]->data.daoints[1];
	return S;
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
	DaoProcess_PutInteger( proc, DaoxDataFrame_Size( self ) );
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
	DaoxDataFrame_AddLabels( self, p[1]->xEnum.value, p[2]->xMap.items );
}
static void FRAME_AddLabel( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DString *lab = DaoValue_TryGetString( p[2] );
	daoint dim = p[1]->xEnum.value;
	daoint idx = p[3]->xInteger.value;
	DString_ToMBS( lab );
	DaoxDataFrame_AddLabel( self, dim, lab->mbs, idx );
}
static void FRAME_GetIndex( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DString *lab = DaoValue_TryGetString( p[2] );
	daoint idx, dim = p[1]->xEnum.value;
	DString_ToMBS( lab );
	idx = DaoxDataFrame_GetIndex( self, dim, lab->mbs );
	DaoProcess_PutInteger( proc, idx );
}
static int DaoxDF_IsSingleIndex( DaoValue *value )
{
	if( value->type >= DAO_INTEGER && value->type <= DAO_DOUBLE ) return 1;
	if( value->type == DAO_STRING ) return 1;
	return 0;
}
static daoint DaoxDF_MakeIndex( DaoxDataFrame *self, int dim, DaoValue *value, DaoProcess *p )
{
	daoint idx = -1;
	if( value->type >= DAO_INTEGER && value->type <= DAO_DOUBLE ){
		idx = DaoValue_GetInteger( value );
	}else if( value->type == DAO_STRING ){
		DString_ToMBS( value->xString.data );
		idx = DaoxDataFrame_GetIndex( self, dim, value->xString.data->mbs );
	}else if( value->type == DAO_NONE && self->dims[2] == 1 ){
		idx = 0;
	}
	if( idx < 0 || idx >= self->dims[dim] ){
		DaoProcess_RaiseException( p, DAO_ERROR_INDEX_OUTOFRANGE, "" );
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

	if( singleIndex1 && singleIndex2 && (singleIndex3 || self->dims[2] == 1) ){
		daoint i = DaoxDF_MakeIndex( self, DAOX_DF_ROW, p[2], proc );
		daoint j = DaoxDF_MakeIndex( self, DAOX_DF_COL, p[3], proc );
		daoint k = DaoxDF_MakeIndex( self, DAOX_DF_DEP, p[4], proc );
		daoint ik = k * self->dims[0] + i;
		if( i < 0 || j < 0 || k < 0 ) return;
		DaoxDataColumn_SetCell( (DaoxDataColumn*) self->columns->items.pVoid[j], ik, value );
	}else{
		DaoxDataFrame_PrepareSlices( self );
		DaoDataFrame_MakeSlice( self, proc, p+1, N-1, self->slices );
	}
}

static void FRAME_ScanCells( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoInteger integer1 = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger integer2 = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger integer3 = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger *rowidx = & integer1;
	DaoInteger *colidx = & integer2;
	DaoInteger *depidx = & integer3;
	DaoValue value;
	daoint N = self->dims[0];
	daoint M = self->dims[1];
	daoint K = self->dims[2];
	daoint NK = N * K;
	daoint entry, i, j;

	value.xInteger = integer1;
	if( sect == NULL ) return;
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	DaoProcess_AcquireCV( proc );
	for(j=0; j<M; ++j){
		DaoxDataColumn *column = (DaoxDataColumn*) self->columns->items.pVoid[j];
		colidx->value = j;
		for(i=0; i<NK; ++i){
			rowidx->value = i % N;
			depidx->value = i / N;
			if( sect->b >0 ){
				DaoValue *cell = DaoxDataColumn_GetCell( column, i, & value );
				DaoProcess_SetValue( proc, sect->a, cell );
			}
			if( sect->b >1 ) DaoProcess_SetValue( proc, sect->a+1, (DaoValue*) rowidx );
			if( sect->b >2 ) DaoProcess_SetValue( proc, sect->a+2, (DaoValue*) colidx );
			if( sect->b >3 ) DaoProcess_SetValue( proc, sect->a+3, (DaoValue*) depidx );
			proc->topFrame->entry = entry;
			DaoProcess_Execute( proc );
			if( proc->status == DAO_VMPROC_ABORTED ) break;
		}
	}
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
}

#if 0

operator[](row:int, col:string)
operator[](row:string, col:string)

operator + - * / ...

frame.ScanCells( rows, cols )::{}
frame.UpdateCells( rows, cols )::{}
frame.SelectCells( rows, cols )::{}
frame.ScanRows( rows )::{}
frame.UpdateRows( rows )::{}
frame.SelectRows( rows )::{}
frame.ScanColumns( cols )::{}
frame.UpdateColumns( cols )::{}
frame.SelectColumns( cols )::{}
#endif

static DaoFuncItem dataframeMeths[]=
{
	{ FRAME_New,         "DataFrame()=>DataFrame" },
	{ FRAME_NewMatrix,   "DataFrame( mat : array )=>DataFrame" },

	{ FRAME_FromMatrix,  "FromMatrix( self :DataFrame, mat : array )" },

	{ FRAME_Size,      "Size( self :DataFrame )=>int" },
	{ FRAME_UseLabels, "UseLabels( self :DataFrame, dim :DimType, group :int )" },
	{ FRAME_AddLabels, "AddLabels( self :DataFrame, dim :DimType, labels :map<string,int> )" },
	{ FRAME_AddLabel,  "AddLabel( self :DataFrame, dim :DimType, label :string, index :int )" },
	{ FRAME_GetIndex,  "GetIndex( self :DataFrame, dim :DimType, label :string ) => int" },

	{ FRAME_GETMI,
		"[]( self :DataFrame, i :IndexType, j :IndexType =none, k :IndexType =none ) => any" },
	{ FRAME_SETMI,
		"[]=( self :DataFrame, value :any, i :IndexType, j :IndexType =none, k :IndexType =none )" },

	{ FRAME_ScanCells,  "ScanCells( self :DataFrame )[cell:@T,row:int,column:int,depth:int]" },
	{ NULL, NULL },
};

DaoTypeBase dataframeTyper =
{
	"DataFrame", NULL, NULL, (DaoFuncItem*) dataframeMeths, {0}, {0},
	(FuncPtrDel)DaoxDataFrame_Delete, NULL
};


DAO_DLL int DaoDataframe_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	daox_type_dataframe = DaoNamespace_WrapType( ns, & dataframeTyper, 0 );
	DaoNamespace_TypeDefine( ns, "enum<row,column,depth>", "DataFrame::DimType" );
	DaoNamespace_TypeDefine( ns, "none|int|string|tuple<none,none>|tuple<int,int>|tuple<string,string>|tuple<int|string,none>|tuple<none,int|string>", "DataFrame::IndexType" );
	return 0;
}
