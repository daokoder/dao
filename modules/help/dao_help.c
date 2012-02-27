/*=========================================================================================
  This file is a part of the Dao standard modules.
  Copyright (C) 2011-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#include<stdlib.h>
#include<string.h>
#include<math.h>
#include"daoString.h"
#include"daoValue.h"
#include"daoParser.h"
#include"daoNamespace.h"
#include"daoVmspace.h"
#include"daoGC.h"


typedef struct DaoxHelp   DaoxHelp;
typedef struct DaoxHelper DaoxHelper;

struct DaoxHelp
{
	int i;
};

static DaoxHelp* DaoxHelp_New()
{
	DaoxHelp *self = (DaoxHelp*) dao_malloc( sizeof(DaoxHelp) );
	return self;
}
static void DaoxHelp_Delete( DaoxHelp *self )
{
	free( self );
}

struct DaoxHelper
{
	DMap *helps;
	DArray *nslist;
};
DaoxHelper *daox_helper = NULL;
DaoValue *daox_cdata_helper = NULL;
DaoVmSpace *dao_vmspace = NULL;

static DaoxHelper* DaoxHelper_New()
{
	DaoxHelper *self = (DaoxHelper*) dao_malloc( sizeof(DaoxHelper) );
	self->helps = DHash_New(0,0);
	self->nslist = DArray_New(D_VALUE);
	return self;
}
static void DaoxHelper_Delete( DaoxHelper *self )
{
	DNode *it;
	for(it=DMap_First(self->helps); it; it=DMap_Next(self->helps,it)){
		DaoxHelp_Delete( (DaoxHelp*) it->value.pVoid );
	}
	DMap_Delete( self->helps );
	DArray_Delete( self->nslist );
	free( self );
}
static void DaoxHelper_GetGCFields( void *p, DArray *v, DArray *arrays, DArray *m, int rm )
{
	DaoxHelper *self = (DaoxHelper*) p;
	DArray_Append( arrays, self->nslist );
}

static DaoxHelp* DaoxHelper_Get( DaoxHelper *self, DaoNamespace *NS, DString *name )
{
	DaoxHelp *help;
	DNode *node = DMap_Find( self->helps, NS );
	if( node ) return (DaoxHelp*) node->value.pVoid;
	help = DaoxHelp_New();
	DMap_Insert( self->helps, NS, help );
	DArray_Append( self->nslist, NS );
	return help;
}


static void PrintMethod( DaoProcess *proc, DaoRoutine *meth )
{
	int j;
	DaoProcess_Print( proc, meth->routName->mbs );
	DaoProcess_Print( proc, ": " );
	for(j=meth->routName->size; j<20; j++) DaoProcess_Print( proc, " " );
	DaoProcess_Print( proc, meth->routType->name->mbs );
	DaoProcess_Print( proc, "\n" );
}
static void DaoNS_GetAuxMethods( DaoNamespace *ns, DaoValue *p, DArray *methods )
{
	daoint i;
	for(i=0; i<ns->cstData->size; i++){
		DaoValue *meth = ns->cstData->items.pValue[i];
		if( meth == NULL || meth->type != DAO_ROUTINE ) continue;
		if( meth->type == DAO_ROUTINE && meth->xRoutine.overloads ){
			DRoutines *routs = meth->xRoutine.overloads;
			if( routs->mtree == NULL ) continue;
			for(i=0; i<routs->routines->size; i++){
				DaoRoutine *rout = routs->routines->items.pRoutine[i];
				DaoType *type = rout->routType->nested->items.pType[0];
				if( DaoType_MatchValue( (DaoType*) type->aux, p, NULL ) ==0 ) continue;
				DArray_PushBack( methods, rout );
			}
		}else if( meth->xRoutine.attribs & DAO_ROUT_PARSELF ){
			DaoType *type = meth->xRoutine.routType->nested->items.pType[0];
			if( DaoType_MatchValue( (DaoType*) type->aux, p, NULL ) ==0 ) continue;
			DArray_PushBack( methods, meth );
		}
	}
	for(i=1; i<ns->namespaces->size; i++) DaoNS_GetAuxMethods( ns->namespaces->items.pNS[i], p, methods );
}
static void HELP_List( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns = proc->activeNamespace;
	DaoType *type = DaoNamespace_GetType( ns, p[0] );
	DaoRoutine **meths;
	DArray *array;
	DMap *hash;
	DNode *it;
	int etype = p[1]->xEnum.value;
	int i, j, methCount;

	if( etype == 2 ){
		array = DArray_New(0);
		hash = DHash_New(0,0);
		DaoProcess_Print( proc, "==============================================\n" );
		DaoProcess_Print( proc, "Auxiliar methods for type: " );
		DaoProcess_Print( proc, type->name->mbs );
		DaoProcess_Print( proc, "\n==============================================\n" );
		DaoNS_GetAuxMethods( ns, p[0], array );
		for(i=0; i<array->size; i++){
			DaoRoutine *rout = array->items.pRoutine[i];
			if( DMap_Find( hash, rout ) ) continue;
			DMap_Insert( hash, rout, NULL );
			PrintMethod( proc, rout );
		}
		DArray_Delete( array );
		DMap_Delete( hash );
		return;
	}

	if( type == NULL ) return;
	DaoType_FindValue( type, type->name ); /* To ensure it has been setup; */
	DaoType_FindFunction( type, type->name ); /* To ensure it has been specialized; */
	if( type->kernel == NULL ) return;

	if( etype == 0 ){
		DaoProcess_Print( proc, "==============================================\n" );
		DaoProcess_Print( proc, "Constant values for type: " );
		DaoProcess_Print( proc, type->name->mbs );
		DaoProcess_Print( proc, "\n==============================================\n" );
		hash = type->kernel->values;
		if( type->kernel->values ){
			for(it=DMap_First(hash); it; it=DMap_Next(hash,it)){
				DaoProcess_Print( proc, it->key.pString->mbs );
				DaoProcess_Print( proc, "\n" );
			}
		}
		return;
	}

	DaoProcess_Print( proc, "==============================================\n" );
	DaoProcess_Print( proc, "Methods for type: " );
	DaoProcess_Print( proc, type->name->mbs );
	DaoProcess_Print( proc, "\n==============================================\n" );

	array = DArray_New(0);
	if( type->kernel->methods ) DMap_SortMethods( type->kernel->methods, array );
	meths = array->items.pRoutine;
	methCount = array->size;
	if( type->kernel->methods ){
		for(i=0; i<methCount; i++ ) PrintMethod( proc, (DaoRoutine*)meths[i] );
	}
	DArray_Delete( array );
}
static void HELP_Help( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxHelp *help;
	DaoNamespace *NS = NULL;
	DaoStream *stdio = proc->stdioStream;
	DString *name = DString_New(1);
	DString *name2 = DString_New(1);
	daoint pos;

	DaoProcess_PutValue( proc, daox_cdata_helper );

	DString_Assign( name, p[0]->xString.data );
	DString_ToMBS( name );
	while( NS == NULL ){
		DString_SetMBS( name2, "help_" );
		DString_Append( name2, name );
		DString_ChangeMBS( name2, "%.", "_", 0 );
		NS = DaoVmSpace_Load( proc->vmSpace, name2, 0 );
		if( NS ) break;
		pos = DString_RFindChar( name, '.', -1 );
		if( pos < 0 ) break;
		DString_Erase( name, pos, 1 );
	}

	help = DaoxHelper_Get( daox_helper, NS, NULL );
	printf( "%s %p\n", name->mbs, help );
	DString_Delete( name );
	DString_Delete( name2 );
	if( stdio == NULL ) stdio = proc->vmSpace->stdioStream;
	if( help == NULL ){
		DaoStream_WriteMBS( stdio, "No help document available for \"" );
		DaoStream_WriteString( stdio, name );
		DaoStream_WriteMBS( stdio, "\"" );
		return;
	}
}
static void HELP_Help2( DaoProcess *proc, DaoValue *p[], int N )
{
}
static void HELP_Search( DaoProcess *proc, DaoValue *p[], int N )
{
}
static void HELP_Search2( DaoProcess *proc, DaoValue *p[], int N )
{
}

static DaoFuncItem helpMeths[]=
{
	{ HELP_Help,      "help( keyword :string )" },
	{ HELP_Help2,     "help( object :any, keyword :string )" },
	{ HELP_List,      "list( object :any, type :enum<values,methods,auxmeths>=$methods )" },
	{ HELP_Search,    "search( keyword :string )" },
	{ HELP_Search2,   "search( object :any, keyword :string )" },
	{ NULL, NULL }
};

static DaoTypeBase helpTyper =
{ "help", NULL, NULL, helpMeths, {0}, {0}, (FuncPtrDel) DaoxHelper_Delete, NULL };

static DString* dao_verbatim_content( DString *VT )
{
	DString *content = DString_New(1);
	daoint rb = DString_FindChar( VT, ']', 0 );
	DString_SetDataMBS( content, VT->mbs + rb + 1, VT->size - 2*(rb + 1) );
	DString_Trim( content );
	return content;
}
static int dao_help_name( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out )
{
	DString *name = dao_verbatim_content( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, name );
	printf( "%s\n", name->mbs );
	DString_Delete( name );
	return 0;
}
static int dao_help_title( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out )
{
	DString *title = dao_verbatim_content( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, NULL );
	printf( "help = %p\n", help );
	printf( "%s\n", title->mbs );
	DString_Delete( title );
	return 0;
}
static int dao_help_text( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out )
{
	DString *text = dao_verbatim_content( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, NULL );
	printf( "help = %p\n", help );
	printf( "%s\n", text->mbs );
	DString_Delete( text );
	return 0;
}
static int dao_help_code( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out )
{
	DString *code = dao_verbatim_content( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, NULL );
	printf( "help = %p\n", help );
	printf( "%s\n", code->mbs );
	DString_Delete( code );
	return 0;
}
static int dao_help_method( DaoNamespace *NS, DString *mode, DString *verbatim, DString *out )
{
	DString *method = dao_verbatim_content( verbatim );
	DaoxHelp *help = DaoxHelper_Get( daox_helper, NS, NULL );
	printf( "help = %p\n", help );
	printf( "%s\n", method->mbs );
	DString_Delete( method );
	return 0;
}

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoType *type;
	dao_vmspace = vmSpace;
	DaoNamespace_AddCodeInliner( ns, "name", dao_help_name );
	DaoNamespace_AddCodeInliner( ns, "title", dao_help_title );
	DaoNamespace_AddCodeInliner( ns, "text", dao_help_text );
	DaoNamespace_AddCodeInliner( ns, "code", dao_help_code );
	DaoNamespace_AddCodeInliner( ns, "method", dao_help_method );
	type = DaoNamespace_WrapType( ns, & helpTyper, 1 );
	daox_helper = DaoxHelper_New();
	daox_cdata_helper = (DaoValue*) DaoCdata_New( type, daox_helper );
	DaoNamespace_AddConstValue( ns, "__helper__", daox_cdata_helper );
	return 0;
}

