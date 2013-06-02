/*
// Dao Protobject Module
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
#include "daoValue.h"
#include "daoGC.h"


typedef struct DaoxProtobject DaoxProtobject;


struct DaoxProtobject
{
	DAO_CSTRUCT_COMMON;

	DaoxProtobject  *proto;
	DMap            *fields;
};

DaoType *daox_type_protobject = NULL;

DaoxProtobject* DaoxProtobject_New()
{
	int i;
	DaoxProtobject *self = (DaoxProtobject*) dao_calloc( 1, sizeof(DaoxProtobject) );
	DaoCstruct_Init( (DaoCstruct*) self, daox_type_protobject );
	self->fields = DHash_New(D_STRING,D_VALUE);
	self->proto = NULL;
	return self;
}
void DaoxProtobject_Delete( DaoxProtobject *self )
{
	DMap_Delete( self->fields );
	GC_DecRC( self->proto );
	dao_free( self );
}
DaoValue* DaoxProtobject_GetField( DaoxProtobject *self, DString *field )
{
	DNode *it;
	DString_ToMBS( field );
	it = DMap_Find( self->fields, field );
	if( it ) return it->value.pValue;
	if( self->proto ) return DaoxProtobject_GetField( self->proto, field );
	return NULL;
}





static void PROTOBJ_New( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxProtobject *self = DaoxProtobject_New();
	DaoProcess_PutValue( proc, (DaoValue*) self );
}
static void PROTOBJ_GetProto( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxProtobject *self = (DaoxProtobject*) p[0];
	DaoValue *value = self->proto ? (DaoValue*) self->proto : dao_none_value;
	DaoProcess_PutValue( proc, value );
}
static void PROTOBJ_SetProto( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxProtobject *self = (DaoxProtobject*) p[0];
	DaoxProtobject *pro = (DaoxProtobject*) DaoValue_CastCstruct( p[1], daox_type_protobject );
	DaoxProtobject *pp = pro;
	int cyclic = 0;
	while( pp ){
		if( pp == self ){
			cyclic = 1;
			break;
		}
		pp = pp->proto;
	}
	if( cyclic ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "cyclic prototypes" );
		return;
	}
	DaoGC_ShiftRC( (DaoValue*) pro, (DaoValue*) self->proto );
	self->proto = pro;
}
static void PROTOBJ_GetField( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxProtobject *self = (DaoxProtobject*) p[0];
	DString *field = DaoValue_TryGetString( p[1] );
	DaoValue *value = DaoxProtobject_GetField( self, field );
	DaoProcess_PutValue( proc, value );
}
static void PROTOBJ_SetField( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxProtobject *self = (DaoxProtobject*) p[0];
	DString *field = DaoValue_TryGetString( p[1] );
	DString_ToMBS( field );
	DMap_Insert( self->fields, field, p[2] );
}


static DaoFuncItem protobjectMeths[]=
{
	{ PROTOBJ_New,       "Protobject()=>Protobject" },

	{ PROTOBJ_GetProto,  ".__proto__( self :Protobject ) => Protobject|none" },
	{ PROTOBJ_SetProto,  ".__proto__=( self :Protobject, proto :Protobject|none )" },

	{ PROTOBJ_GetField,  ".( self :Protobject, field :string ) => any" },
	{ PROTOBJ_SetField,  ".=( self :Protobject, field :string, value :any )" },

	{ NULL, NULL },
};

static void DaoxProtobject_GC( void *p, DArray *values, DArray *as, DArray *maps, int remove )
{
	DaoxProtobject *self = (DaoxProtobject*) p;
	DArray_Append( values, self->proto );
	DArray_Append( maps, self->fields );
	if( remove ) self->proto = NULL;
}

DaoTypeBase protobjectTyper =
{
	"Protobject", NULL, NULL, (DaoFuncItem*) protobjectMeths, {0}, {0},
	(FuncPtrDel)DaoxProtobject_Delete, DaoxProtobject_GC
};


DAO_DLL int DaoProtobject_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	daox_type_protobject = DaoNamespace_WrapType( ns, & protobjectTyper, 0 );
	return 0;
}
