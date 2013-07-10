/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2013, Limin Fu
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

#include<string.h>
#include<assert.h>

#include"daoConst.h"
#include"daoRoutine.h"
#include"daoGC.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoStream.h"
#include"daoParser.h"
#include"daoProcess.h"
#include"daoVmspace.h"
#include"daoRegex.h"
#include"daoNumtype.h"
#include"daoNamespace.h"
#include"daoValue.h"

DMutex mutex_routines_update;
DMutex mutex_routine_specialize;
DMutex mutex_routine_specialize2;

DaoRoutine* DaoRoutine_New( DaoNamespace *nspace, DaoType *host, int body )
{
	DaoRoutine *self = (DaoRoutine*) dao_calloc( 1, sizeof(DaoRoutine) );
	DaoValue_Init( self, DAO_ROUTINE );
	self->trait |= DAO_VALUE_DELAYGC;
	self->routName = DString_New(1);
	self->routConsts = DaoList_New();
	self->nameSpace = nspace;
	self->routHost = host;
	GC_IncRC( self->nameSpace );
	GC_IncRC( self->routHost );
	GC_IncRC( self->routConsts );
	if( body ){
		self->body = DaoRoutineBody_New();
		GC_IncRC( self->body );
	}
	return self;
}
DaoRoutine* DaoRoutines_New( DaoNamespace *nspace, DaoType *host, DaoRoutine *init )
{
	DaoRoutine *self = DaoRoutine_New( nspace, host, 0 );
	self->overloads = DRoutines_New();
	self->routType = DaoType_New( "routine", DAO_ROUTINE, (DaoValue*)self, NULL );
	self->routType->overloads = 1;
	GC_IncRC( self->routType );
	if( init == NULL ) return self;

	DString_Assign( self->routName, init->routName );
	if( self->nameSpace == NULL ){
		self->nameSpace = init->nameSpace;
		GC_IncRC( self->nameSpace );
	}
	if( init->overloads ){
		DArray *routs = init->overloads->routines;
		int i, n = routs->size;
		for(i=0; i<n; i++){
			DaoRoutine *routine = routs->items.pRoutine[i];
			if( routine->attribs & DAO_ROUT_PRIVATE ){
				if( routine->routHost && routine->routHost != host ) continue;
				if( routine->routHost == NULL && routine->nameSpace != nspace ) continue;
			}
			DRoutines_Add( self->overloads, routine );
		}
	}else{
		DRoutines_Add( self->overloads, init );
	}
	return self;
}
void DaoRoutine_CopyFields( DaoRoutine *self, DaoRoutine *from, int cst, int cbody, int stat )
{
	int i;
	self->attribs = from->attribs;
	self->parCount = from->parCount;
	self->defLine = from->defLine;
	self->pFunc = from->pFunc;
	GC_ShiftRC( from->routHost, self->routHost );
	GC_ShiftRC( from->routType, self->routType );
	GC_ShiftRC( from->nameSpace, self->nameSpace );
	self->routHost = from->routHost;
	self->routType = from->routType;
	self->nameSpace = from->nameSpace;
	DString_Assign( self->routName, from->routName );
	if( cst ){
		DaoList *list = DaoList_New();
		GC_ShiftRC( list, self->routConsts );
		self->routConsts = list;
		DArray_Assign( & self->routConsts->items, & from->routConsts->items );
	}else{
		GC_ShiftRC( from->routConsts, self->routConsts );
		self->routConsts = from->routConsts;
	}
	if( from->body ){
		DaoRoutineBody *body = from->body;
		if( cbody ) body = DaoRoutineBody_Copy( body, stat );
		GC_ShiftRC( body, self->body );
		self->body = body;
	}
}
DaoRoutine* DaoRoutine_Copy( DaoRoutine *self, int cst, int body, int stat )
{
	DaoRoutine *copy = DaoRoutine_New( self->nameSpace, self->routHost, 0 );
	DaoRoutine_CopyFields( copy, self, cst, body, stat );
	return copy;
}
void DaoRoutine_Delete( DaoRoutine *self )
{
	GC_DecRC( self->routHost );
	GC_DecRC( self->routType );
	GC_DecRC( self->routConsts );
	GC_DecRC( self->nameSpace );
	DString_Delete( self->routName );
	if( self->overloads ) DRoutines_Delete( self->overloads );
	if( self->specialized ) DRoutines_Delete( self->specialized );
	if( self->body ) GC_DecRC( self->body );
	dao_free( self );
}
int DaoRoutine_IsWrapper( DaoRoutine *self )
{
	return self->pFunc != NULL;
}
int DaoRoutine_AddConstant( DaoRoutine *self, DaoValue *value )
{
	DArray *consts = & self->routConsts->items;
	DArray_Append( consts, value );
	DaoValue_MarkConst( consts->items.pValue[consts->size-1] );
	return consts->size-1;
}
static int DaoRoutine_Check( DaoRoutine *self, DaoValue *obj, DaoValue *p[], int n, int code )
{
	DNode *node;
	DaoValue **dpar = p;
	DMap *defs = NULL;
	DMap *mapNames = self->routType->mapNames;
	DaoType *abtp, **parType = self->routType->nested->items.pType;
	int need_self = self->routType->attrib & DAO_TYPE_SELF;
	int selfChecked = 0, selfMatch = 0;
	int ndef = self->parCount;
	int npar = n;
	int j, ifrom, ito;
	int parpass[DAO_MAX_PARAM];

	/* func();
	 * obj.func();
	 * obj::func();
	 */
	if( code == DVM_MCALL && ! (self->routType->attrib & DAO_TYPE_SELF) ){
		npar --;
		dpar ++;
	}else if( obj && need_self && code != DVM_MCALL ){
		/* class DaoClass : CppClass{ cppmethod(); }
		 * use io;
		 * print(..);
		 */
		abtp = & parType[0]->aux->xType;
		selfMatch = DaoType_MatchValue2( abtp, obj, defs );
		if( selfMatch ){
			parpass[0] = selfMatch;
			selfChecked = 1;
		}
	}
	/*
	   if( strcmp( rout->routName->mbs, "expand" ) ==0 )
	   printf( "%i, %p, parlist = %s; npar = %i; ndef = %i, %i\n", i, rout, rout->routType->name->mbs, npar, ndef, selfChecked );
	 */
	if( (npar | ndef) ==0 ) return 1;
	if( npar > ndef ) return 0;
	defs = DMap_New(0,0);
	for( j=selfChecked; j<ndef; j++) parpass[j] = 0;
	for(ifrom=0; ifrom<npar; ifrom++){
		DaoValue *val = dpar[ifrom];
		ito = ifrom + selfChecked;
		if( ito < ndef && parType[ito]->tid == DAO_PAR_VALIST ){
			DaoType *vltype = (DaoType*) parType[ito]->aux;
			for(; ifrom<npar; ifrom++){
				DaoValue *val = dpar[ifrom];
				if( vltype && DaoType_MatchValue2( vltype, val, defs ) == 0 ) goto NotMatched;
				parpass[ifrom+selfChecked] = 1;
			}
			break;
		}
		if( val->type == DAO_PAR_NAMED ){
			DaoNameValue *nameva = & val->xNameValue;
			val = nameva->value;
			node = DMap_Find( mapNames, nameva->name );
			if( node == NULL ) goto NotMatched;
			ito = node->value.pInt;
		}
		if( ito >= ndef ) goto NotMatched;
		abtp = & parType[ito]->aux->xType; /* must be named */
		parpass[ito] = DaoType_MatchValue2( abtp, val, defs );
		/*
		   printf( "%i:  %i  %s\n", parpass[ito], abtp->tid, abtp->name->mbs );
		 */
		if( parpass[ito] == 0 ) goto NotMatched;
	}
	for(ito=0; ito<ndef; ito++){
		int m = parType[ito]->tid;
		if( m == DAO_PAR_VALIST ) break;
		if( parpass[ito] ) continue;
		if( m != DAO_PAR_DEFAULT ) goto NotMatched;
		parpass[ito] = 1;
	}
	DMap_Delete( defs );
	return 1;
NotMatched:
	DMap_Delete( defs );
	return 0;
}

DaoTypeBase routTyper=
{
	"routine", & baseCore, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoRoutine_Delete, NULL
};

DaoRoutineBody* DaoRoutineBody_New()
{
	DaoRoutineBody *self = (DaoRoutineBody*) dao_calloc( 1, sizeof( DaoRoutineBody ) );
	DaoValue_Init( self, DAO_ROUTBODY );
	self->trait |= DAO_VALUE_DELAYGC;
	self->source = NULL;
	self->vmCodes = DVector_New( sizeof(DaoVmCode) );
	self->regType = DArray_New(D_VALUE);
	self->svariables = DArray_New(D_VALUE);
	self->defLocals = DArray_New(D_TOKEN);
	self->annotCodes = DArray_New(D_VMCODE);
	self->localVarType = DMap_New(0,0);
	self->abstypes = DMap_New(D_STRING,D_VALUE);
	self->simpleVariables = DArray_New(0);
	self->codeStart = self->codeEnd = 0;
	self->jitData = NULL;
	self->specialized = 0;
	return self;
}
void DaoRoutineBody_Delete( DaoRoutineBody *self )
{
	DVector_Delete( self->vmCodes );
	DArray_Delete( self->simpleVariables );
	DArray_Delete( self->regType );
	DArray_Delete( self->svariables );
	DArray_Delete( self->defLocals );
	DArray_Delete( self->annotCodes );
	DMap_Delete( self->localVarType );
	DMap_Delete( self->abstypes );
	if( self->decoTargets ) DArray_Delete( self->decoTargets );
	if( self->revised ) GC_DecRC( self->revised );
	if( dao_jit.Free && self->jitData ){
		/* LLVMContext provides no locking guarantees: */
		DMutex_Lock( & mutex_routine_specialize );
		dao_jit.Free( self->jitData );
		DMutex_Unlock( & mutex_routine_specialize );
	}
	dao_free( self );
}
void DaoRoutineBody_CopyFields( DaoRoutineBody *self, DaoRoutineBody *other, int copy_stat )
{
	int i;
	DMap_Delete( self->localVarType );
	DArray_Delete( self->annotCodes );
	self->source = other->source;
	self->annotCodes = DArray_Copy( other->annotCodes );
	self->localVarType = DMap_Copy( other->localVarType );
	if( self->decoTargets ){
		DArray_Delete( self->decoTargets );
		self->decoTargets = NULL;
	}
	if( other->decoTargets ) self->decoTargets = DArray_Copy( other->decoTargets );
	DVector_Assign( self->vmCodes, other->vmCodes );
	DArray_Assign( self->regType, other->regType );
	DArray_Assign( self->simpleVariables, other->simpleVariables );
	self->regCount = other->regCount;
	self->codeStart = other->codeStart;
	self->codeEnd = other->codeEnd;
	DArray_Clear( self->svariables );
	for(i=0; i<other->svariables->size; ++i){
		DaoVariable *var = other->svariables->items.pVar[i];
		if( copy_stat ) var = DaoVariable_New( var->value, var->dtype );
		DArray_Append( self->svariables, var );
	}
}
DaoRoutineBody* DaoRoutineBody_Copy( DaoRoutineBody *self, int copy_stat )
{
	DaoRoutineBody *copy = DaoRoutineBody_New();
	DaoRoutineBody_CopyFields( copy, self, copy_stat );
	return copy;
}

extern void DaoRoutine_JitCompile( DaoRoutine *self );

int DaoRoutine_SetVmCodes( DaoRoutine *self, DArray *vmCodes )
{
	int i, n;
	DaoRoutineBody *body = self->body;
	if( body == NULL ) return 0;
	if( vmCodes == NULL || vmCodes->type != D_VMCODE ) return 0;
	DArray_Swap( body->annotCodes, vmCodes );
	vmCodes = body->annotCodes;
	DVector_Resize( body->vmCodes, vmCodes->size );
	for(i=0,n=vmCodes->size; i<n; i++){
		body->vmCodes->data.codes[i] = *(DaoVmCode*) vmCodes->items.pVmc[i];
	}
	return DaoRoutine_DoTypeInference( self, 0 );
}
int DaoRoutine_SetVmCodes2( DaoRoutine *self, DVector *vmCodes )
{
	DVector_Assign( self->body->vmCodes, vmCodes );
	return DaoRoutine_DoTypeInference( self, 0 );
}

void DaoRoutine_SetSource( DaoRoutine *self, DArray *tokens, DaoNamespace *ns )
{
	DArray_Append( ns->sources, tokens );
	self->body->source = (DArray*) DArray_Back( ns->sources );
}

static const char *const sep1 = "==========================================\n";
static const char *const sep2 =
"-------------------------------------------------------------------------\n";

void DaoRoutine_FormatCode( DaoRoutine *self, int i, DaoVmCodeX vmc, DString *output )
{
	char buffer1[10];
	char buffer2[200];
	const char *fmt = daoRoutineCodeFormat;
	const char *name;

	DString_Clear( output );
	name = DaoVmCode_GetOpcodeName( vmc.code );
	sprintf( buffer1, "%5i :  ", i);
	if( self->body->source ) DaoLexer_AnnotateCode( self->body->source, vmc, output, 24 );
	sprintf( buffer2, fmt, name, vmc.a, vmc.b, vmc.c, vmc.line, output->mbs );
	DString_SetMBS( output, buffer1 );
	DString_AppendMBS( output, buffer2 );
}
void DaoRoutine_PrintCode( DaoRoutine *self, DaoStream *stream )
{
	DaoVmCodeX **vmCodes;
	DString *annot;
	int j, n;

	DaoStream_WriteMBS( stream, sep1 );
	DaoStream_WriteMBS( stream, "routine " );
	DaoStream_WriteString( stream, self->routName );
	DaoStream_WriteMBS( stream, "():\n" );
	DaoStream_WriteMBS( stream, "type: " );
	DaoStream_WriteString( stream, self->routType->name );
	if( self->body ){
		DaoStream_WriteMBS( stream, "\nNumber of register:\n" );
		DaoStream_WriteInt( stream, self->body->regCount );
	}
	DaoStream_WriteMBS( stream, "\n" );
	if( self->body == NULL ) return;

	DaoStream_WriteMBS( stream, sep1 );
	DaoStream_WriteMBS( stream, "Virtual Machine Code:\n\n" );
	DaoStream_WriteMBS( stream, daoRoutineCodeHeader );

	DaoStream_WriteMBS( stream, sep2 );
	annot = DString_New(1);
	vmCodes = self->body->annotCodes->items.pVmc;
	for(j=0,n=self->body->annotCodes->size; j<n; j++){
		DaoVmCode vmc = self->body->vmCodes->data.codes[j];
		if( vmc.code == DVM_JITC ){
			DaoVmCodeX vmcx = *vmCodes[j];
			memcpy( &vmcx, &vmc, sizeof(DaoVmCode) );
			DaoRoutine_FormatCode( self, j, vmcx, annot );
			DaoStream_WriteString( stream, annot );
		}
		DaoRoutine_FormatCode( self, j, *vmCodes[j], annot );
		DaoStream_WriteString( stream, annot );
	}
	DaoStream_WriteMBS( stream, sep2 );
	DString_Delete( annot );
}



static DParamNode* DParamNode_New()
{
	DParamNode *self = (DParamNode*) dao_calloc( 1, sizeof(DParamNode) );
	return self;
}
static void DParamNode_Delete( DParamNode *self )
{
	while( self->first ){
		DParamNode *node = self->first;
		self->first = node->next;
		DParamNode_Delete( node );
	}
	dao_free( self );
}


DRoutines* DRoutines_New()
{
	DRoutines *self = (DRoutines*) dao_calloc( 1, sizeof(DRoutines) );
	self->tree = NULL;
	self->mtree = NULL;
	self->routines = DArray_New(0);
	self->array = DArray_New(D_VALUE);
	self->array2 = DArray_New(0);
	return self;
}
void DRoutines_Delete( DRoutines *self )
{
	if( self->tree ) DParamNode_Delete( self->tree );
	if( self->mtree ) DParamNode_Delete( self->mtree );
	DArray_Delete( self->routines );
	DArray_Delete( self->array );
	DArray_Delete( self->array2 );
	dao_free( self );
}

static DParamNode* DParamNode_Add( DParamNode *self, DaoRoutine *routine, int pid )
{
	DParamNode *param, *it;
	if( pid >= (int)routine->routType->nested->size ){
		/* If a routine with the same parameter signature is found, return it: */
		for(it=self->first; it; it=it->next) if( it->routine ) return it;
		param = DParamNode_New();
		param->routine = routine;
		/* Add as a leaf. */
		if( self->last ){
			self->last->next = param;
			self->last = param;
		}else{
			self->first = self->last = param;
		}
		return param;
	}
	/* Add a new internal node: */
	param = DParamNode_New();
	param->type = routine->routType->nested->items.pType[pid];
	if( param->type->tid == DAO_PAR_NAMED || param->type->tid == DAO_PAR_DEFAULT ){
		param->type2 = param->type;
		param->type = (DaoType*) param->type->aux;
	}
	it = DParamNode_Add( param, routine, pid+1 );
	/* Add the node to the tree after all its child nodes have been created, to ensure
	 * a reader will always lookup in a valid tree in multi-threaded applications: */
	if( self->last ){
		self->last->next = param;
		self->last = param;
	}else{
		self->first = self->last = param;
	}
	return it;
}
static void DParamNode_ExportRoutine( DParamNode *self, DArray *routines )
{
	DParamNode *it;
	if( self->routine ) DArray_PushFront( routines, self->routine );
	for(it=self->first; it; it=it->next) DParamNode_ExportRoutine( it, routines );
}
DaoRoutine* DRoutines_Add( DRoutines *self, DaoRoutine *routine )
{
	int i, n, bl = 0;
	DParamNode *param = NULL;
	DArray *routs;

	if( routine->routType == NULL ) return NULL;
	/* If the name is not set yet, set it: */
	self->attribs |= DString_FindChar( routine->routType->name, '@', 0 ) != MAXSIZE;
	DMutex_Lock( & mutex_routines_update );
	if( routine->routType->attrib & DAO_TYPE_SELF ){
		if( self->mtree == NULL ) self->mtree = DParamNode_New();
		param = DParamNode_Add( self->mtree, routine, 0 );
	}else{
		if( self->tree == NULL ) self->tree = DParamNode_New();
		param = DParamNode_Add( self->tree, routine, 0 );
	}
	/* Runtime routine specialization based on parameter types may create
	 * two specializations with identical parameter signature, so one of
	 * the specialized routine will not be successully added to the tree.
	 * To avoid memory leaking, the one not added to the tree should also
	 * be appended to "array", so that it can be properly garbage collected. */
	DArray_Append( self->array, routine );
	if( routine != param->routine && routine->routHost && param->routine->routHost ){
		DaoType *t1 = routine->routHost;
		DaoType *t2 = param->routine->routHost;
		if( t1->tid == DAO_CDATA && t2->tid == DAO_CDATA ){
			bl = DaoType_ChildOf( t1, t2 );
		}else if( t1->tid == DAO_CSTRUCT && t2->tid == DAO_CSTRUCT ){
			bl = DaoType_ChildOf( t1, t2 );
		}else if( t1->tid == DAO_OBJECT && (t2->tid == DAO_OBJECT || t2->tid == DAO_CDATA || t2->tid == DAO_CSTRUCT) ){
			bl = DaoClass_ChildOf( & t1->aux->xClass, t2->aux );
		}
		if( bl ) param->routine = routine;
	}
	self->array2->size = 0;
	if( self->mtree ) DParamNode_ExportRoutine( self->mtree, self->array2 );
	if( self->tree ) DParamNode_ExportRoutine( self->tree, self->array2 );
	/* to ensure safety for readers: */
	routs = self->routines;
	self->routines = self->array2;
	self->array2 = routs;
	DMutex_Unlock( & mutex_routines_update );
	return param->routine;
}
void DaoRoutines_Import( DaoRoutine *self, DRoutines *other )
{
	DaoType *host = self->routHost;
	DaoNamespace *nspace = self->nameSpace;
	int i, n = other->routines->size;
	if( self->overloads == NULL ) return;
	for(i=0; i<n; i++){
		DaoRoutine *routine = other->routines->items.pRoutine[i];
		if( routine->attribs & DAO_ROUT_PRIVATE ){
			if( routine->routHost && routine->routHost != host ) continue;
			if( routine->routHost == NULL && routine->nameSpace != nspace ) continue;
		}
		DRoutines_Add( self->overloads, routine );
	}
}
static DaoRoutine* DParamNode_GetLeaf( DParamNode *self, int *ms, int mode )
{
	int b1 = (mode & DAO_CALL_BLOCK) != 0;
	DParamNode *param;
	DaoRoutine *rout;
	DNode *it;

	*ms = 0;
	if( self->routine ){
		int b2 = (self->routine->attribs & DAO_ROUT_CODESECT) != 0;
		if( b1 == b2 ) return self->routine; /* a leaf */
		return NULL;
	}
	for(param=self->first; param; param=param->next){
		if( param->type == NULL ){
			int b2 = (param->routine->attribs & DAO_ROUT_CODESECT) != 0;
			if( b1 == b2 ) return param->routine; /* a leaf */
			return NULL;
		}
		if( param->type->tid == DAO_PAR_VALIST ){
			rout = DParamNode_GetLeaf( param, ms, mode );
			if( rout == NULL ) continue;
			*ms += 1;
			return rout;
		}
	}
	/* check for routines with default parameters: */
	for(param=self->first; param; param=param->next){
		if( param->type2 == NULL || param->type2->tid != DAO_PAR_DEFAULT ) continue;
		rout = DParamNode_GetLeaf( param, ms, mode );
		if( rout == NULL ) continue;
		*ms += 1;
		return rout;
	}
	return NULL;
}
static DaoRoutine* DParamNode_LookupByName( DParamNode *self, DaoValue *p[], int n, int mode, int strict, int *ms, DMap *defs )
{
	DParamNode *param;
	DaoRoutine *rout = NULL, *best = NULL;
	int i, k = 0, m = 0, max = 0;

	if( n ==0 ) return DParamNode_GetLeaf( self, ms, mode );
	for(i=0; i<n; i++){
		DaoValue *pval = p[i];
		DaoNameValue *nameva = & pval->xNameValue;
		if( pval->type != DAO_PAR_NAMED ) return NULL;
		if( nameva->value == NULL ) continue;
		p[i] = p[0];
		for(param=self->first; param; param=param->next){
			if( param->type2 == NULL ) continue;
			if( DString_EQ( param->type2->fname, nameva->name ) ==0 ) continue;
			m = DaoType_MatchValue( param->type, nameva->value, defs );
			if( strict && m < DAO_MT_ANY ) continue;
			if( m == 0 ) continue;
			rout = DParamNode_LookupByName( param, p+1, n-1, mode, strict, & k, defs );
			m += k;
			if( m > max ){
				best = rout;
				max = m;
			}
		}
		p[i] = pval;
	}
	*ms = max;
	return best;
}
static DaoRoutine* DParamNode_LookupByName2( DParamNode *self, DaoType *ts[], int n, int mode, int strict, int *ms, DMap *defs )
{
	DParamNode *param;
	DaoRoutine *rout = NULL, *best = NULL;
	int i, k = 0, m = 0, max = 0;

	if( n ==0 ) return DParamNode_GetLeaf( self, ms, mode );
	for(i=0; i<n; i++){
		DaoType *ptype = ts[i];
		DaoType *vtype = & ptype->aux->xType;
		if( ptype->tid != DAO_PAR_NAMED && ptype->tid != DAO_PAR_DEFAULT ) return NULL;
		ts[i] = ts[0];
		for(param=self->first; param; param=param->next){
			if( param->type2 == NULL ) continue;
			if( DString_EQ( param->type2->fname, ptype->fname ) ==0 ) continue;
			m = DaoType_MatchTo( vtype, param->type, defs );
			if( strict && m < DAO_MT_ANY ) continue;
			if( m == 0 ) continue;
			rout = DParamNode_LookupByName2( param, ts+1, n-1, mode, strict, & k, defs );
			m += k;
			if( m > max ){
				best = rout;
				max = m;
			}
		}
		ts[i] = ptype;
	}
	*ms = max;
	return best;
}
static DaoRoutine* DParamNode_Lookup( DParamNode *self, DaoValue *p[], int n, int mode, int strict, int *ms, DMap *defs, int clear )
{
	int i, m, k = 0, max = 0;
	DaoRoutine *rout = NULL;
	DaoRoutine *best = NULL;
	DaoValue *value = NULL;
	DParamNode *param;

	*ms = 1;
	if( n == 0 ) return DParamNode_GetLeaf( self, ms, mode );
	if( p[0]->type == DAO_PAR_NAMED ) return DParamNode_LookupByName( self, p, n, mode, strict, ms, defs );
	value = p[0];
	for(param=self->first; param; param=param->next){
		DaoType *type = param->type;
		if( type == NULL ) continue;
		if( defs && clear ) DMap_Reset( defs );
		if( type->tid == DAO_PAR_VALIST ){
			type = type->aux ? (DaoType*)type->aux : dao_type_any;
			m = 0;
			for(i=0; i<n; ++i){
				m = DaoType_MatchValue( type, p[i], defs );
				if( m == 0 ) continue;
			}
			if( m == 0 ) continue;
			n = 1;
		}
		m = DaoType_MatchValue( type, value, defs );
		if( m == 0 ) continue;
		if( strict && m < DAO_MT_ANY ) continue;
		rout = DParamNode_Lookup( param, p+1, n-1, mode, strict, & k, defs, 0 );
		if( rout == NULL ) continue;
		m += k;
		if( m > max ){
			best = rout;
			max = m;
		}
	}
	*ms = max;
	return best;
}
static DaoRoutine* DParamNode_LookupByType( DParamNode *self, DaoType *types[], int n, int mode, int strict, int *ms, DMap *defs, int clear )
{
	int i, m, k = 0, max = 0;
	DaoRoutine *rout = NULL;
	DaoRoutine *best = NULL;
	DaoType *partype = NULL;
	DParamNode *param;

	*ms = 1;
	if( n == 0 ) return DParamNode_GetLeaf( self, ms, mode );
	if( types[0]->tid == DAO_PAR_NAMED && types[0]->tid != DAO_PAR_DEFAULT )
		return DParamNode_LookupByName2( self, types, n, mode, strict, ms, defs );
	partype = types[0];
	for(param=self->first; param; param=param->next){
		DaoType *type = param->type;
		if( type == NULL ) continue;
		if( defs && clear ) DMap_Reset( defs );
		if( type->tid == DAO_PAR_VALIST ){
			type = type->aux ? (DaoType*)type->aux : dao_type_any;
			m = 0;
			for(i=0; i<n; ++i){
				m = DaoType_MatchTo( types[i], type, defs );
				if( m == 0 ) continue;
			}
			if( m == 0 ) continue;
			n = 1;
		}
		m = DaoType_MatchTo( partype, type, defs );
		if( m == 0 ) continue;
		if( strict && m < DAO_MT_ANY ) continue;
		rout = DParamNode_LookupByType( param, types+1, n-1, mode, strict, & k, defs, 0 );
		if( rout == NULL ) continue;
		m += k;
		if( m > max ){
			best = rout;
			max = m;
		}
	}
	*ms = max;
	return best;
}
static DaoRoutine* DRoutines_Lookup2( DRoutines *self, DaoValue *obj, DaoValue *p[], int n, int code, int mode, int strict )
{
	int i, k, m, score = 0;
	int mcall = code == DVM_MCALL;
	DParamNode *param = NULL;
	DaoRoutine *rout = NULL;
	DMap *defs = NULL;
	if( self->attribs ) defs = DHash_New(0,0);
	if( obj && obj->type && mcall ==0 ){
		if( self->mtree ){
			DaoRoutine *rout2 = NULL;
			for(param=self->mtree->first; param; param=param->next){
				if( param->type == NULL ) continue;
				if( defs ) DMap_Reset( defs );
				m = DaoType_MatchValue( param->type, obj, defs );
				if( strict && m < DAO_MT_ANY ) continue;
				if( m == 0 ) continue;
				rout2 = DParamNode_Lookup( param, p, n, mode, strict, & k, defs, 0 );
				if( rout2 == NULL ) continue;
				m += k;
				if( m > score ){
					rout = rout2;
					score = m;
				}
			}
			if( rout ) goto Finalize;
		}
	}
	if( mcall && self->mtree ){
		rout = DParamNode_Lookup( self->mtree, p, n, mode, strict, & score, defs, 1 );
		if( rout ) goto Finalize;
	}
	if( self->tree ){
		if( mcall ){
			p += 1;
			n -= 1;
		}
		rout = DParamNode_Lookup( self->tree, p, n, mode, strict, & score, defs, 1 );
	}
Finalize:
	if( defs ) DMap_Delete( defs );
	return rout;
}
static DaoRoutine* DRoutines_LookupByType2( DRoutines *self, DaoType *selftype, DaoType *types[], int n, int code, int mode, int strict )
{
	int i, k, m, score = 0;
	int mcall = code == DVM_MCALL;
	DParamNode *param = NULL;
	DaoRoutine *rout = NULL;
	DMap *defs = NULL;
	if( self->attribs ) defs = DHash_New(0,0);
	if( selftype && mcall ==0 ){
		if( self->mtree ){
			DaoRoutine *rout2 = NULL;
			for(param=self->mtree->first; param; param=param->next){
				if( param->type == NULL ) continue;
				if( defs ) DMap_Reset( defs );
				m = DaoType_MatchTo( selftype, param->type, defs );
				if( strict && m < DAO_MT_ANY ) continue;
				if( m == 0 ) continue;
				rout2 = DParamNode_LookupByType( param, types, n, mode, strict, & k, defs, 0 );
				if( rout2 == NULL ) continue;
				m += k;
				if( m > score ){
					rout = rout2;
					score = m;
				}
			}
			if( rout ) goto Finalize;
		}
	}
	if( mcall && self->mtree ){
		rout = DParamNode_LookupByType( self->mtree, types, n, mode, strict, & score, defs, 1 );
		if( rout ) goto Finalize;
	}
	if( self->tree ){
		if( mcall ){
			types += 1;
			n -= 1;
		}
		rout = DParamNode_LookupByType( self->tree, types, n, mode, strict, & score, defs, 1 );
	}
Finalize:
	if( defs ) DMap_Delete( defs );
	return rout;
}
static DaoRoutine* DRoutines_Lookup( DRoutines *self, DaoValue *obj, DaoValue *p[], int n, int code, int mode )
{
	return DRoutines_Lookup2( self, obj, p, n, code, mode, 0 );
}
static DaoRoutine* DRoutines_LookupByType( DRoutines *self, DaoType *selftype, DaoType *types[], int n, int code, int mode )
{
	return DRoutines_LookupByType2( self, selftype, types, n, code, mode, 0 );
}
DaoRoutine* DaoRoutine_ResolveX( DaoRoutine *self, DaoValue *obj, DaoValue *p[], int n, int codemode )
{
	DaoRoutine *rout;
	int code = codemode & 0xffff;
	int mode = codemode >> 16;
	int mcall = code == DVM_MCALL;
	int b1, b2;

	if( self == NULL ) return NULL;
	if( self->overloads ){
		self = DRoutines_Lookup( self->overloads, obj, p, n, code, mode );
		if( self == NULL ) return NULL;
	}
	rout = self;
	if( rout->specialized ){
		/* strict checking for specialized routines: */
		DaoRoutine *rt = DRoutines_Lookup2( rout->specialized, obj, p, n, code, mode, 1 );
		/*
		// If the routine has a body, check if it has done specialization.
		// Only used specialized routine for thread safety to avoid the
		// situation where the routine is used for execution, but its body
		// is still undergoing specialization.
		*/
		if( rt && (rt->body == NULL || rt->body->specialized) ) rout = rt;
	}
	b1 = (mode & DAO_CALL_BLOCK) != 0;
	b2 = (rout->attribs & DAO_ROUT_CODESECT) != 0;
	if( b1 != b2 ) return NULL;
	return (DaoRoutine*) rout;
}
DaoRoutine* DaoRoutine_ResolveByTypeX( DaoRoutine *self, DaoType *st, DaoType *t[], int n, int codemode )
{
	int code = codemode & 0xffff;
	int mode = codemode >> 16;
	int b1, b2;
	if( self == NULL ) return NULL;
	if( self->overloads ){
		self = DRoutines_LookupByType( self->overloads, st, t, n, code, mode );
		if( self == NULL ) return NULL;
	}
	if( self->specialized ){
		/* strict checking for specialized routines: */
		DaoRoutine *rt = DRoutines_LookupByType2( self->specialized, st, t, n, code, mode, 1 );
		/*
		// no need to check for specialization,
		// because routines returned by this are not used for execution:
		*/
		if( rt ) return rt;
	}
	b1 = (mode & DAO_CALL_BLOCK) != 0;
	b2 = (self->attribs & DAO_ROUT_CODESECT) != 0;
	if( b1 != b2 ) return NULL;
	return self;
}
DaoRoutine* DaoRoutine_Resolve( DaoRoutine *self, DaoValue *o, DaoValue *p[], int n )
{
	DaoRoutine *rout = DaoRoutine_ResolveX( self, o, p, n, DVM_CALL );
	if( rout == (DaoRoutine*)self ){ /* parameters not yet checked: */
		if( DaoRoutine_Check( rout, o, p, n, DVM_CALL ) ==0 ) return NULL;
	}
	return rout;
}


static DParamNode* DParamNode_BestNextByType( DParamNode *self, DaoType *par )
{
	DParamNode *param;
	if( par->tid == DAO_PAR_NAMED || par->tid == DAO_PAR_DEFAULT ) par = & par->aux->xType;
	for(param=self->first; param; param=param->next){
		if( param->type == par ) return param;
	}
	return NULL;
}
static DaoRoutine* DParamNode_LookupByType2( DParamNode *self, DaoType *types[], int n )
{
	DParamNode *param = NULL;
	if( n == 0 ){
		if( self->routine ) return self->routine; /* a leaf */
		for(param=self->first; param; param=param->next){
			if( param->type == NULL ) return param->routine; /* a leaf */
		}
		return NULL;
	}
	param = DParamNode_BestNextByType( self, types[0] );
	if( param == NULL ) return NULL;
	return DParamNode_LookupByType2( param, types+1, n-1 );
}
