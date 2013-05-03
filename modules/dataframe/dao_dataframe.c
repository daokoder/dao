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

#include "dao_dataframe.h"
#include "daoProcess.h"
#include "daoNumtype.h"
#include "daoValue.h"
#include "daoType.h"
#include "daoGC.h"


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
DaoValue* DaoxDataColumn_GetCell( DaoxDataColumn *self, daoint row, DaoValue *value )
{
	value->xNone.type = self->type->tid;
	switch( self->type->tid ){
	case DAO_INTEGER : value->xInteger.value = self->cells->data.daoints[row]; break;
	case DAO_FLOAT   : value->xFloat.value   = self->cells->data.floats[row];  break;
	case DAO_DOUBLE  : value->xDouble.value  = self->cells->data.doubles[row]; break;
	case DAO_COMPLEX : value->xComplex.value = self->cells->data.complexes[row]; break;
	case DAO_STRING  : value->xString.data   = & self->cells->data.strings[row]; break;
	default : value = self->cells->data.values[row]; break;
	}
	return value;
}


DaoxDataFrame* DaoxDataFrame_New()
{
	DaoxDataFrame *self = (DaoxDataFrame*) dao_calloc( 1, sizeof(DaoxDataFrame) );
	DaoCstruct_Init( (DaoCstruct*) self, daox_type_dataframe );
	self->rowLabels = DArray_New(D_MAP);
	self->colLabels = DArray_New(D_MAP);
	self->dataColumns = DArray_New(0);
	self->cacheColumns = DArray_New(0);
	return self;
}
void DaoxDataFrame_Delete( DaoxDataFrame *self )
{
	DArray_Delete( self->rowLabels );
	DArray_Delete( self->colLabels );
	DArray_Delete( self->dataColumns );
	DArray_Delete( self->cacheColumns );
	dao_free( self );
}

void DaoxDataFrame_Clear( DaoxDataFrame *self )
{
	daoint i;
	DaoxDataFrame_Reset( self );
	for(i=0; i<self->cacheColumns->size; ++i){
		DaoxDataColumn_Delete( (DaoxDataColumn*) self->cacheColumns->items.pVoid[i] );
	}
	DArray_Clear( self->cacheColumns );
}
void DaoxDataFrame_Reset( DaoxDataFrame *self )
{
	daoint i;
	self->rowCount = 0;
	for(i=0; i<self->dataColumns->size; ++i){
		DArray_Append( self->cacheColumns, self->dataColumns->items.pVoid[i] );
	}
	DArray_Clear( self->rowLabels );
	DArray_Clear( self->colLabels );
	DArray_Clear( self->dataColumns );
}
DaoxDataColumn* DaoxDataFrame_MakeColumn( DaoxDataFrame *self, DaoType *type )
{
	DaoxDataColumn *column = (DaoxDataColumn*) DArray_Back( self->cacheColumns );
	if( self->cacheColumns->size ){
		DaoxDataColumn_SetType( column, type );
		DArray_PopBack( self->cacheColumns );
		return column;
	}
	return DaoxDataColumn_New( type );
}

int DaoxDataFrame_FromMatrix( DaoxDataFrame *self, DaoArray *matrix )
{
	DaoType *etype = dao_array_types[ matrix->etype ]->nested->items.pType[0];
	DaoxDataColumn *column;
	daoint i, j, nrow, ncol;

	DaoxDataFrame_Reset( self );
	if( matrix->ndim != 2 ) return 0;

	nrow = matrix->dims[0];
	ncol = matrix->dims[1];
	self->rowCount = nrow;

	for(j=0; j<ncol; ++j){
		column = DaoxDataFrame_MakeColumn( self, etype );
		DArray_Append( self->dataColumns, column );
		DaoxDataColumn_Reset( column, nrow );
		for(i=0; i<nrow; ++i){
			daoint idx = i * ncol + j;
			switch( matrix->etype ){
			case DAO_INTEGER : column->cells->data.daoints[i]   = matrix->data.i[idx]; break;
			case DAO_FLOAT   : column->cells->data.floats[i]    = matrix->data.f[idx]; break;
			case DAO_DOUBLE  : column->cells->data.doubles[i]   = matrix->data.d[idx]; break;
			case DAO_COMPLEX : column->cells->data.complexes[i] = matrix->data.c[idx]; break;
			}
		}
	}
	return 1;
}
void DaoxDataFrame_UseLabels( DaoxDataFrame *self, int rowGroup, int colGroup )
{
	if( rowGroup >= self->rowLabels->size ) rowGroup = self->rowLabels->size - 1;
	if( colGroup >= self->colLabels->size ) colGroup = self->colLabels->size - 1;
	if( rowGroup < 0 ) rowGroup = 0;
	if( colGroup < 0 ) colGroup = 0;
	self->rowGroup = rowGroup;
	self->colGroup = colGroup;
}
void DaoxDataFrame_AddLabelGroup( DaoxDataFrame *self, int row )
{
	DMap *labmap = DHash_New(D_STRING,0);
	DArray *labels = row ? self->rowLabels : self->colLabels;
	if( row ){
		self->rowGroup = labels->size;
	}else{
		self->colGroup = labels->size;
	}
	DArray_Append( labels, labmap );
	DMap_Delete( labmap );
}
void DaoxDataFrame_AddLabel( DaoxDataFrame *self, int row, const char *lab, daoint idx )
{
	DString slab = DString_WrapMBS( lab );
	DMap *labmap = NULL;
	if( idx < 0 ) return;
	if( row && self->rowGroup < self->rowLabels->size ){
		if( idx >= self->rowCount ) return;
		labmap = self->rowLabels->items.pMap[self->rowGroup];
	}else if( self->colGroup < self->colLabels->size ){
		if( idx >= self->dataColumns->size ) return;
		labmap = self->colLabels->items.pMap[self->colGroup];
	}
	if( labmap != NULL ) DMap_Insert( labmap, & slab, (void*)(size_t) idx );
}
daoint DaoxDataFrame_GetIndex( DaoxDataFrame *self, int row, const char *label )
{
	DString slab = DString_WrapMBS( label );
	DMap *labmap = NULL;
	DNode *it;
	if( row && self->rowGroup < self->rowLabels->size ){
		labmap = self->rowLabels->items.pMap[self->rowGroup];
	}else if( self->colGroup < self->colLabels->size ){
		labmap = self->colLabels->items.pMap[self->colGroup];
	}
	it = labmap == NULL ? NULL : DMap_Find( labmap, & slab );
	if( it ) return it->value.pInt;
	return -1;
}
void DaoxDataFrame_AddLabels( DaoxDataFrame *self, int row, DMap *labels )
{
	DString *lab;
	DNode *it;
	if( labels->keytype != D_STRING && labels->keytype != D_VALUE ) return;
	if( labels->valtype != 0 && labels->valtype != D_VALUE ) return;
	lab = DString_New(1);
	DaoxDataFrame_AddLabelGroup( self, row );
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
		DaoxDataFrame_AddLabel( self, row, lab->mbs, idx );
	}
	DString_Delete( lab );
}

void DaoxDataFrame_Encode( DaoxDataFrame *self, DString *output )
{
}
void DaoxDataFrame_Decode( DaoxDataFrame *self, DString *input )
{
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
	DaoProcess_PutInteger( proc, self->rowCount * self->dataColumns->size );
}
static void FRAME_UseLabels( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	int rowGroup = p[1]->xInteger.value;
	int colGroup = p[2]->xInteger.value;
	DaoxDataFrame_UseLabels( self, rowGroup, colGroup );
}
static void FRAME_AddLabels( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DaoxDataFrame_AddLabels( self, p[1]->xEnum.value == 0, p[2]->xMap.items );
}
static void FRAME_AddLabel( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DString *lab = DaoValue_TryGetString( p[2] );
	daoint row = p[1]->xEnum.value == 0;
	daoint idx = p[3]->xInteger.value;
	DString_ToMBS( lab );
	DaoxDataFrame_AddLabel( self, row, lab->mbs, idx );
}
static void FRAME_GetIndex( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DString *lab = DaoValue_TryGetString( p[2] );
	daoint idx, row = p[1]->xEnum.value == 0;
	DString_ToMBS( lab );
	idx = DaoxDataFrame_GetIndex( self, row, lab->mbs );
	DaoProcess_PutInteger( proc, idx );
}

static void FRAME_ScanCells( DaoProcess *proc, DaoValue *p[], int npar )
{
	DaoxDataFrame *self = (DaoxDataFrame*) p[0];
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	DaoInteger integer1 = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger integer2 = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger *rowidx = & integer1;
	DaoInteger *colidx = & integer2;
	DaoValue value;
	daoint entry, i, j;

	value.xInteger = integer1;
	if( sect == NULL ) return;
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	entry = proc->topFrame->entry;
	DaoProcess_AcquireCV( proc );
	for(j=0; j<self->dataColumns->size; ++j){
		DaoxDataColumn *column = (DaoxDataColumn*) self->dataColumns->items.pVoid[j];
		colidx->value = j;
		for(i=0; i<self->rowCount; ++i){
			rowidx->value = i;
			if( sect->b >0 ){
				DaoValue *cell = DaoxDataColumn_GetCell( column, i, & value );
				DaoProcess_SetValue( proc, sect->a, cell );
			}
			if( sect->b >1 ) DaoProcess_SetValue( proc, sect->a+1, (DaoValue*) rowidx );
			if( sect->b >2 ) DaoProcess_SetValue( proc, sect->a+2, (DaoValue*) colidx );
			proc->topFrame->entry = entry;
			DaoProcess_Execute( proc );
			if( proc->status == DAO_VMPROC_ABORTED ) break;
		}
	}
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
}

#if 0
DataFrame.UseLabels(int,int)

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

	{ FRAME_Size,        "Size( self :DataFrame )=>int" },
	{ FRAME_UseLabels,   "UseLabels( self :DataFrame, row :int, col :int )" },
	{ FRAME_AddLabels,   "AddLabels( self :DataFrame, which :enum<row,column>, labels :map<string,int> )" },
	{ FRAME_AddLabel,    "AddLabel( self :DataFrame, which :enum<row,column>, label :string, idx :int )" },
	{ FRAME_GetIndex,    "GetIndex( self :DataFrame, which :enum<row,column>, label :string ) => int" },

	{ FRAME_ScanCells,  "ScanCells( self :DataFrame )[cell:@T,row:int,column:int]" },
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
	return 0;
}
