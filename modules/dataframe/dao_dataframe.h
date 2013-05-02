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

#ifndef __DAO_DATAFRAME_H__
#define __DAO_DATAFRAME_H__

#include "dao.h"
#include "daoStdtype.h"

typedef struct DaoxDataColumn  DaoxDataColumn;
typedef struct DaoxDataFrame   DaoxDataFrame;



struct DaoxDataColumn
{
	DaoType  *type;   // type of the cells;
	DVector  *cells;  // DVector<daoint|float|double|complex|DString | DaoValue* >
};

DAO_DLL DaoxDataColumn* DaoxDataColumn_New( DaoType *type );
DAO_DLL void DaoxDataColumn_Delete( DaoxDataColumn *self );

DAO_DLL void DaoxDataColumn_Reset( DaoxDataColumn *self, daoint size );

DAO_DLL void DaoxDataColumn_SetType( DaoxDataColumn *self, DaoType *type );



struct DaoxDataFrame
{
	DAO_CSTRUCT_COMMON;

	daoint   rowCount;     // number of rows;
	uint_t   rowGroup;     // active row group;
	uint_t   colGroup;     // active column group;
	DArray  *rowLabels;    // DArray<DMap<DString*>*>
	DArray  *colLabels;    // DArray<DMap<DString*>*>
	DArray  *dataColumns;  // DArray<DaoxDataColumn*>
	DArray  *cacheColumns; // DArray<DaoxDataColumn*>
};

DAO_DLL DaoType *daox_type_dataframe;

DAO_DLL DaoxDataFrame* DaoxDataFrame_New();
DAO_DLL void DaoxDataFrame_Delete( DaoxDataFrame *self );

DAO_DLL void DaoxDataFrame_Clear( DaoxDataFrame *self );
DAO_DLL void DaoxDataFrame_Reset( DaoxDataFrame *self );

DAO_DLL int DaoxDataFrame_FromMatrix( DaoxDataFrame *self, DaoArray *matrix );

DAO_DLL void DaoxDataFrame_Encode( DaoxDataFrame *self, DString *output );
DAO_DLL void DaoxDataFrame_Decode( DaoxDataFrame *self, DString *input );

#endif
