/*
// Dao Virtual Machine
// http://daoscript.org
//
// Copyright (c) 2006-2017, Limin Fu
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

#include<string.h>
#include<ctype.h>
#include<stdlib.h>
#include<stdio.h>
#include<assert.h>

#include"daoGC.h"
#include"daoTasklet.h"
#include"daoLexer.h"
#include"daoValue.h"
#include"daoRoutine.h"
#include"daoNamespace.h"
#include"daoVmspace.h"
#include"daoOptimizer.h"
#include"daoInferencer.h"


extern DMutex mutex_routine_specialize;


DaoInode* DaoInode_New()
{
	DaoInode *self = (DaoInode*) dao_calloc( 1, sizeof(DaoInode) );
	return self;
}
void DaoInode_Delete( DaoInode *self )
{
	dao_free( self );
}
void DaoInode_Print( DaoInode *self, int index )
{
	const char *name = DaoVmCode_GetOpcodeName( self->code );
	static const char *fmt = "%3i: %-8s : %5i, %5i, %5i;  [%3i] [%2i] %9p %9p %9p, %s\n";
	if( index < 0 ) index = self->index;
	printf( fmt, index, name, self->a, self->b, self->c, self->line, self->level,
			self, self->jumpTrue, self->jumpFalse, "" );
}



void DaoInodes_Clear( DList *inodes )
{
	DaoInode *tmp, *inode = (DaoInode*) DList_Front( inodes );
	while( inode && inode->prev ) inode = inode->prev;
	while( inode ){
		tmp = inode;
		inode = inode->next;
		DaoInode_Delete( tmp );
	}
	DList_Clear( inodes );
}

void DaoRoutine_CodesToInodes( DaoRoutine *self, DList *inodes )
{
	DaoInode *inode, *inode2;
	DaoVmCodeX *vmc, **vmcs = self->body->annotCodes->items.pVmc;
	daoint i, N = self->body->annotCodes->size;

	for(i=0; i<N; i++){
		inode2 = (DaoInode*) DList_Back( inodes );

		inode = DaoInode_New();
		vmc = vmcs[i];
		if( vmc->code == DVM_GETMI && vmc->b == 1 ){
			vmc->code = DVM_GETI;
			vmc->b = vmc->a + 1;
		}else if( vmc->code == DVM_SETMI && vmc->b == 1 ){
			vmc->code = DVM_SETI;
			vmc->b = vmc->c + 1;
		}
		*(DaoVmCodeX*)inode = *vmc;
		inode->index = i;
		if( inode2 ){
			inode2->next = inode;
			inode->prev = inode2;
		}
		DList_PushBack( inodes, inode );
	}
	for(i=0; i<N; i++){
		vmc = vmcs[i];
		inode = inodes->items.pInode[i];
		switch( vmc->code ){
		case DVM_GOTO : case DVM_CASE : case DVM_SWITCH :
		case DVM_TEST : case DVM_TEST_B : case DVM_TEST_I : case DVM_TEST_F :
			inode->jumpFalse = inodes->items.pInode[vmc->b];
			break;
		default : break;
		}
	}
}
void DaoRoutine_CodesFromInodes( DaoRoutine *self, DList *inodes )
{
	int count = 0;
	DaoRoutineBody *body = self->body;
	DaoInode *it, *first = (DaoInode*) DList_Front( inodes );
	while( first->prev ) first = first->prev;
	for(it=first; it; it=it->next){
		if( it->jumpFalse == NULL ) continue;
		while( it->jumpFalse->prev && it->jumpFalse->prev->index == it->jumpFalse->index ){
			/*
			// Instructions with the same index are from the same single original
			// instruction, so they belong to the same branch group (basic block).
			*/
			it->jumpFalse = it->jumpFalse->prev;
		}
	}
	for(it=first; it; it=it->next){
		it->index = count;
		count += it->code != DVM_UNUSED;
	}
	DArray_Clear( body->vmCodes );
	DList_Clear( body->annotCodes );
	for(it=first,count=0; it; it=it->next){
		/* DaoInode_Print( it ); */
		switch( it->code ){
		case DVM_GOTO : case DVM_CASE : case DVM_SWITCH :
		case DVM_TEST : case DVM_TEST_B : case DVM_TEST_I : case DVM_TEST_F :
			it->b = it->jumpFalse->index;
			break;
		default : break;
		}
		if( it->code >= DVM_UNUSED ) continue;
		DArray_PushCode( body->vmCodes, *(DaoVmCode*) it );
		DList_PushBack( body->annotCodes, (DaoVmCodeX*) it );
	}
}
void DaoRoutine_SetupSimpleVars( DaoRoutine *self )
{
	DMap *refers = DMap_New(0,0);
	DaoRoutineBody *body = self->body;
	DaoVmCodeX **vmcs = body->annotCodes->items.pVmc;
	int i, n;

	self->attribs &= ~DAO_ROUT_REUSABLE;
	for(i=0,n=body->annotCodes->size; i<n; ++i){
		if( DaoVmCode_MayCreateReference( vmcs[i]->code ) ){
			DMap_Insert( refers, IntToPointer( vmcs[i]->code ), 0 );
		}
	}

	DList_Clear( body->simpleVariables );
	for(i=self->parCount,n=body->regType->size; i<n; ++i){
		DaoType *tp = body->regType->items.pType[i];
		if( tp && tp->tid <= DAO_ENUM ){
			DList_Append( body->simpleVariables, (daoint)i );
			if( DMap_Find( refers, IntToPointer(i) ) != NULL ){
				self->attribs |= DAO_ROUT_REUSABLE;
			}
		}
	}
	DMap_Delete( refers );
}



DaoInferencer* DaoInferencer_New()
{
	DaoInferencer *self = (DaoInferencer*) dao_calloc( 1, sizeof(DaoInferencer) );
	self->inodes = DList_New(0);
	self->consts = DList_New( DAO_DATA_VALUE );
	self->types = DList_New( DAO_DATA_VALUE );
	self->types2 = DList_New( DAO_DATA_VALUE );
	self->rettypes = DList_New(0);
	self->typeMaps = DList_New( DAO_DATA_MAP );
	self->errors = DList_New(0);
	self->array = DList_New(0);
	self->array2 = DList_New(0);
	self->defers = DList_New(0);
	self->routines = DList_New(0);
	self->defs = DHash_New(0,0);
	self->defs2 = DHash_New(0,0);
	self->defs3 = DHash_New(0,0);
	self->mbstring = DString_New();
	return self;
}
void DaoInferencer_Reset( DaoInferencer *self )
{
	DaoInodes_Clear( self->inodes );
	DList_Clear( self->consts );
	DList_Clear( self->types );
	DList_Clear( self->types2 );
	DList_Clear( self->typeMaps );
	DMap_Reset( self->defs );
	DMap_Reset( self->defs2 );
	DMap_Reset( self->defs3 );
	self->rettypes->size = 0;
	self->errors->size = 0;
	self->array->size = 0;
	self->array2->size = 0;
	self->defers->size = 0;
	self->routines->size = 0;
	self->error = 0;
	self->annot_first = 0;
	self->annot_last = 0;
	self->tid_target = 0;
	self->type_source = NULL;
	self->type_target = NULL;
}
void DaoInferencer_Delete( DaoInferencer *self )
{
	DaoInferencer_Reset( self );
	DList_Delete( self->inodes );
	DList_Delete( self->consts );
	DList_Delete( self->types );
	DList_Delete( self->types2 );
	DList_Delete( self->rettypes );
	DList_Delete( self->typeMaps );
	DList_Delete( self->errors );
	DList_Delete( self->array );
	DList_Delete( self->array2 );
	DList_Delete( self->defers );
	DList_Delete( self->routines );
	DString_Delete( self->mbstring );
	DMap_Delete( self->defs );
	DMap_Delete( self->defs2 );
	DMap_Delete( self->defs3 );
	dao_free( self );
}
void DaoInferencer_Init( DaoInferencer *self, DaoRoutine *routine, int silent )
{
	DNode *node;
	DMap *defs = self->defs;
	DaoType *type, **types;
	DaoNamespace *NS = routine->nameSpace;
	DaoVmSpace *VMS = NS->vmSpace;
	DList *partypes = routine->routType->args;
	daoint i, n, M = routine->body->regCount;
	ushort_t codeStart = routine->body->codeStart;
	ushort_t codeEnd = codeStart + routine->body->codeCount;

	DaoInferencer_Reset( self );
	self->silent = silent;
	self->routine = routine;
	self->tidHost = routine->routHost ? routine->routHost->tid : 0;
	self->hostClass = self->tidHost == DAO_OBJECT ? & routine->routHost->aux->xClass:NULL;

	DaoRoutine_CodesToInodes( routine, self->inodes );

	DList_Resize( self->consts, M, NULL );
	/*
	// Allocate more memory so that the "types" and "typeVH" variables in
	// DaoInferencer_DoInference() will not be invalidated by inserting instructions.
	*/
	DList_Resize( self->types, 3*M, NULL );
	self->types->size = M;
	types = self->types->items.pType;

	for(i=0,n=partypes->size; i<n; i++){
		types[i] = partypes->items.pType[i];
		if( types[i] && types[i]->tid == DAO_PAR_VALIST ){
			DaoType *vltype = (DaoType*) types[i]->aux;
			while( i < DAO_MAX_PARAM ) types[i++] = vltype;
			break;
		}
		type = types[i];
		if( type && (type->attrib & DAO_TYPE_PARNAMED) ) types[i] = & type->aux->xType;
		node = MAP_Find( routine->body->localVarType, i );
		if( node == NULL ) continue;
		if( node->value.pType == NULL || types[i] == NULL ) continue;
		DaoType_MatchTo( types[i], node->value.pType, defs );
	}
	node = DMap_First( routine->body->localVarType );
	for( ; node !=NULL; node = DMap_Next(routine->body->localVarType,node) ){
		if( node->key.pInt < (int)partypes->size ) continue;
		types[ node->key.pInt ] = DaoType_DefineTypes( node->value.pType, NS, defs );
	}
	for(i=0; i<self->types->size; i++) GC_IncRC( types[i] );
	DList_PushBack( self->typeMaps, defs );

	for(i=0; i<NS->constants->size; ++i){
		DaoRoutine *rout = DaoValue_CastRoutine( NS->constants->items.pConst[i]->value );
		if( rout != NULL && rout->body != NULL && rout != routine && rout->nameSpace == NS ){
			if( rout->attribs & DAO_ROUT_MAIN ) continue;
			if( rout->body->codeStart > codeStart && rout->body->codeStart < codeEnd ){
				DList_Append( self->routines, rout );
			}
		}
	}

	self->basicTypes[DAO_NONE]    = VMS->typeNone;
	self->basicTypes[DAO_BOOLEAN] = VMS->typeBool;
	self->basicTypes[DAO_INTEGER] = VMS->typeInt;
	self->basicTypes[DAO_FLOAT]   = VMS->typeFloat;
	self->basicTypes[DAO_COMPLEX] = VMS->typeComplex;
	self->basicTypes[DAO_ENUM]    = VMS->typeEnum;
	self->basicTypes[DAO_STRING]  = VMS->typeString;
}


static int DaoRoutine_CheckTypeX( DaoType *routType, DaoNamespace *ns, DaoType *selftype,
		DaoType *argtypes[], int argcount, int code, int def, int *parpass, int passdefault )
{
	int parcount = 0;
	int i, j, match = 1;
	int argindex, parindex;
	int size = routType->args->size;
	int selfChecked = 0, selfMatch = 0;
	DaoType **partypes = routType->args->items.pType;
	DaoType *partype;
	DMap *defs;

	/* Check for explicit self parameter: */
	if( argcount && (argtypes[0]->attrib & DAO_TYPE_SELFNAMED) ){
		selftype = NULL;
		code = DVM_MCALL;
	}

	defs = DHash_New(0,0);
	if( routType->args ){
		parcount = routType->args->size;
		if( parcount ){
			partype = partypes[ parcount-1 ];
			if( partype->tid == DAO_PAR_VALIST ) parcount = DAO_MAX_PARAM;
		}
	}

#if 0
	printf( "=====================================\n" );
	for( j=0; j<argcount; j++){
		DaoType *argtype = argtypes[j];
		if( argtype != NULL ) printf( "argtype[ %i ]: %s\n", j, argtype->name->chars );
	}
	printf( "%s %i %i\n", routType->name->chars, parcount, argcount );
	if( selftype ) printf( "%i\n", routType->name->chars, parcount, argcount, selftype );
#endif

	if( code == DVM_MCALL && ! (routType->attrib & DAO_TYPE_SELF) ){
		argcount --;
		argtypes ++;
	}else if( selftype && (routType->attrib & DAO_TYPE_SELF) && code != DVM_MCALL ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		partype = (DaoType*) partypes[0]->aux;
		/*
		// self object will be passed by reference,
		// so invariable/constant cannot be passed to variable type
		*/
		selfMatch = DaoType_MatchTo( selftype, partype, defs );
		if( selfMatch ){
			if( DaoType_CheckInvarMatch( selftype, partype, 1 ) == 0 ) goto FinishError;
			selfChecked = 1;
			parpass[0] = selfMatch;
		}
	}
	if( argcount == parcount && parcount == 0 ) goto FinishOK;
	if( (argcount+selfChecked) > parcount && (size == 0 || partypes[size-1]->tid != DAO_PAR_VALIST ) ){
		goto FinishError;
	}

	for(j=selfChecked; j<parcount; j++) parpass[j] = 0;
	for(argindex=0; argindex<argcount; argindex++){
		DaoType *argtype = argtypes[argindex];
		parindex = argindex + selfChecked;
		partype = partypes[parindex];
		if( partype->tid == DAO_PAR_VALIST ){
			DaoType *vlt = (DaoType*) partype->aux;
			for(; argindex<argcount; argindex++, parindex++){
				parpass[parindex] = 1;
				if( vlt && DaoType_MatchTo( argtype, vlt, defs ) == 0 ) goto FinishError;
			}
			break;
		}else if( (partype->attrib & DAO_TYPE_SELFNAMED) && partype->aux->xType.invar == 0 ){
			if( DaoType_CheckInvarMatch( argtype, (DaoType*) partype->aux, 1 ) == 0 ) goto FinishError;
		}
		if( argtype == NULL )  goto FinishError;
		if( argtype->attrib & DAO_TYPE_PARNAMED ) argtype = (DaoType*) argtype->aux;
		if( partype->attrib & DAO_TYPE_PARNAMED ) partype = (DaoType*) partype->aux;
		parpass[parindex] = DaoType_MatchTo( argtype, partype, defs );

#if 0
		printf( "%s %s\n", argtype->name->chars, partype->name->chars );
		printf( "%i:  %i\n", parindex, parpass[parindex] );
#endif

		if( parpass[parindex] == 0 ) goto FinishError;
		if( def ){
			DaoType *argtype = DaoType_DefineTypes( argtypes[argindex], ns, defs );
			GC_Assign( & argtypes[argindex], argtype );
		}
	}
	if( passdefault ){
		for(parindex=0; parindex<parcount; parindex++){
			i = partypes[parindex]->tid;
			if( i == DAO_PAR_VALIST ) break;
			if( parpass[parindex] ) continue;
			if( i != DAO_PAR_DEFAULT ) goto FinishError;
			parpass[parindex] = 1;
		}
	}
	match = DAO_MT_EQ;
	for(j=0; j<(argcount+selfChecked); j++) if( match > parpass[j] ) match = parpass[j];

#if 0
	printf( "%s %i %i %i\n", routType->name->chars, match, parcount, argcount );
#endif

FinishOK:
	DMap_Delete( defs );
	return match;
FinishError:
	DMap_Delete( defs );
	return 0;
}
static int DaoRoutine_CheckType( DaoType *routType, DaoNamespace *ns, DaoType *selftype,
		DaoType *argtypes[], int argcount, int codemode, int def )
{
	int parpass[DAO_MAX_PARAM];
	int code = codemode & 0xffff;
	int b1 = ((codemode>>16) & DAO_CALL_BLOCK) != 0;
	int b2 = (routType->attrib & DAO_TYPE_CODESECT) != 0;
	if( b1 != b2 ) return 0;
	return DaoRoutine_CheckTypeX( routType, ns, selftype, argtypes, argcount, code, def, parpass, 1 );
}

static void DaoRoutine_PassParamTypes( DaoRoutine *self, DaoType *selftype, DaoType *argtypes[], int argcount, int code, DMap *defs )
{
	int argindex, parindex;
	int selfChecked = 0;
	int parcount = self->parCount;
	DaoType **partypes = self->routType->args->items.pType;
	DaoType  *partype, *argtype;

	/*
	   printf( "%s %s\n", self->routName->chars, self->routType->name->chars );
	 */

	/* Check for explicit self parameter: */
	if( argcount && (argtypes[0]->attrib & DAO_TYPE_SELFNAMED) ) selftype = NULL;
	if( argcount == parcount && parcount == 0 ) return;

	/* Remove type holder bindings for the self parameter: */
	if( self->routType->attrib & DAO_TYPE_SELF ){
		partype = (DaoType*) self->routType->args->items.pType[0]->aux;
		DaoType_ResetTypeHolders( partype, defs );
	}

	if( code == DVM_MCALL && ! (self->routType->attrib & DAO_TYPE_SELF) ){
		argcount --;
		argtypes ++;
	}else if( selftype && (self->routType->attrib & DAO_TYPE_SELF) && code != DVM_MCALL ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		partype = (DaoType*) self->routType->args->items.pType[0]->aux;
		if( DaoType_MatchTo( selftype, partype, defs ) ) selfChecked = 1;
	}
	for(argindex=0; argindex<argcount; argindex++){
		parindex = argindex + selfChecked;
		if( parindex >= (int)self->routType->args->size ) break;
		if( parindex < parcount && partypes[parindex]->tid == DAO_PAR_VALIST ){
			DaoType *vlt = (DaoType*) partypes[parindex]->aux;
			while( argindex < argcount ){
				DaoType_MatchTo( argtypes[argindex++], vlt, defs );
			}
			break;
		}
		argtype = argtypes[argindex];
		if( argtype == NULL || parindex >= parcount ) break;
		partype = partypes[parindex];
		if( partype->attrib & DAO_TYPE_PARNAMED ) partype = (DaoType*) partype->aux;
		if( argtype == NULL || partype == NULL )  break;
		DaoType_MatchTo( argtype, partype, defs );
	}
	/*
	   for(node=DMap_First(defs);node;node=DMap_Next(defs,node))
	   printf( "binding:  %s  %s\n", node->key.pType->name->chars, node->value.pType->name->chars );
	 */
}


enum DaoTypingErrorCode
{
	DTE_TYPE_AMBIGIOUS_PFA = 1,
	DTE_TYPE_NOT_CONSISTENT ,
	DTE_TYPE_NOT_MATCHING ,
	DTE_TYPE_NOT_INITIALIZED,
	DTE_TYPE_WRONG_CONTAINER ,
	DTE_DATA_CANNOT_CREATE ,
	DTE_NOT_CALLABLE ,
	DTE_CALL_INVALID ,
	DTE_CALL_NON_INVAR ,
	DTE_CALL_NOT_PERMIT ,
	DTE_CALL_WITHOUT_INSTANCE ,
	DTE_CALL_INVALID_SECTPARAM ,
	DTE_CALL_INVALID_SECTION ,
	DTE_ROUT_INVALID_YIELD ,
	DTE_ROUT_INVALID_RETURN ,
	DTE_ROUT_INVALID_RETURN2 ,
	DTE_ROUT_MISSING_RETURN ,
	DTE_FIELD_NOT_PERMIT ,
	DTE_FIELD_NOT_EXIST ,
	DTE_FIELD_OF_INSTANCE ,
	DTE_INVALID_ENUMERATION ,
	DTE_ITEM_WRONG_ACCESS ,
	DTE_INDEX_NOT_VALID ,
	DTE_KEY_NOT_VALID ,
	DTE_OPERATION_NOT_VALID ,
	DTE_INVALID_INVAR_CAST ,
	DTE_INVAR_VAL_TO_VAR_VAR,
	DTE_PARAM_ERROR ,
	DTE_PARAM_WRONG_NUMBER ,
	DTE_PARAM_WRONG_TYPE ,
	DTE_PARAM_WRONG_NAME ,
	DTE_INVALID_TYPE_CASE ,
	DTE_CONST_WRONG_MODIFYING ,
	DTE_INVALID_INVAR_INITOR ,
	DTE_INVAR_INITOR_MUTABLE ,
	DTE_ROUT_NOT_IMPLEMENTED
};
static const char*const DaoTypingErrorString[] =
{
	"",
	"Ambigious partial function application on overloaded functions",
	"Inconsistent typing",
	"Types not matching",
	"Variable not initialized",
	"Wrong container type",
	"Data cannot be created",
	"Object not callable",
	"Invalid call",
	"Calling non-invar method inside invar method",
	"Calling not permitted",
	"Calling non-static method without instance",
	"Calling with invalid code section parameter",
	"Calling normal method with code section",
	"Invalid yield in ordinary routine",
	"Invalid return for the constructor or defer block",
	"Invalid return type",
	"Return is expected but not present",
	"Member not permitted",
	"Member not exist",
	"Need class instance",
	"Invalid enumeration" ,
	"Invalid index/key access",
	"Invalid index access",
	"Invalid key acess",
	"Invalid operation on the type",
	"Invalid casting from invar type",
	"Invalid assignment from invar value to var varaible",
	"Invalid parameters for the call",
	"Invalid number of parameter",
	"Invalid parameter type",
	"Invalid parameter name",
	"Invalid type case",
	"Constant or invariable cannot be modified",
	"Invalid constructor definition for invar class",
	"Invalid operation that might return external nonprimitive and mutable types",
	"Call to un-implemented function"
};

static DString* AppendError( DList *errors, DaoValue *rout, size_t type )
{
	DString *s = DString_New();
	DList_Append( errors, rout );
	DList_Append( errors, s );
	DString_AppendChars( s, DaoTypingErrorString[ type ] );
	DString_AppendChars( s, " --- \" " );
	return s;
}
static void DString_AppendTypeError( DString *self, DaoType *from, DaoType *to )
{
	DString_AppendChar( self, '\'' );
	DString_Append( self, from->name );
	DString_AppendChars( self, "\' for \'" );
	DString_Append( self, to->name );
	DString_AppendChars( self, "\' \";\n" );
}
void DaoRoutine_CheckError( DaoNamespace *ns, DaoRoutine *rout, DaoType *routType, DaoType *selftype, DaoType *argtypes[], int argcount, int codemode, DList *errors )
{
	DNode *node;
	DString *s;
	DMap *defs = DHash_New(0,0);
	DaoType *partype, **partypes = routType->args->items.pType;
	DaoValue *routobj = rout ? (DaoValue*)rout : (DaoValue*)routType;
	int size = routType->args->size;
	int i, j, parcount = 0;
	int argindex, parindex;
	int parpass[DAO_MAX_PARAM];
	int selfChecked = 0, selfMatch = 0;
	int code = codemode & 0xffff;
	int b1 = ((codemode>>16) & DAO_CALL_BLOCK) != 0;
	int b2 = (routType->attrib & DAO_TYPE_CODESECT) != 0;

	if( b1 == 0 && b2 != 0 ){
		DString *s = AppendError( errors, routobj, DTE_CALL_INVALID );
		DString_AppendChars( s, "calling code section method without code section \";\n" );
		goto FinishError;
	}else if( b1 != 0 && b2 == 0 ){
		DString *s = AppendError( errors, routobj, DTE_CALL_INVALID );
		DString_AppendChars( s, "calling normal method with code section \";\n" );
		goto FinishError;
	}

	if( routType->args ){
		parcount = routType->args->size;
		if( parcount ){
			partype = partypes[ parcount-1 ];
			if( partype->tid == DAO_PAR_VALIST ) parcount = DAO_MAX_PARAM;
		}
	}

#if 0
	printf( "=====================================\n" );
	printf( "%s\n", rout->routName->chars );
	for( j=0; j<argcount; j++){
		DaoType *argtype = argtypes[j];
		if( argtype != NULL ) printf( "argtype[ %i ]: %s\n", j, argtype->name->chars );
	}
	printf( "%s %i %i\n", routType->name->chars, parcount, argcount );
#endif

	if( code == DVM_MCALL && ! ( routType->attrib & DAO_TYPE_SELF ) ){
		argcount --;
		argtypes ++;
	}else if( selftype && ( routType->attrib & DAO_TYPE_SELF) && code != DVM_MCALL ){
		/* class DaoClass : CppClass{ cppmethod(); } */
		partype = & partypes[0]->aux->xType;
		selfMatch = DaoType_MatchTo( selftype, partype, defs );
		if( selfMatch ){
			selfChecked = 1;
			parpass[0] = selfMatch;
			if( DaoType_CheckInvarMatch( selftype, partype, 1 ) == 0 ){
				DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_TYPE );
				partype = DaoType_DefineTypes( partype, ns, defs );
				DString_AppendTypeError( s, selftype, partype );
				goto FinishError;
			}
		}
	}
	if( argcount == parcount && parcount == 0 ) goto FinishOK;
	if( (argcount+selfChecked) > parcount && (size == 0 || partypes[size-1]->tid != DAO_PAR_VALIST ) ){
		DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_NUMBER );
		DString_AppendChars( s, "too many parameters \";\n" );
		goto FinishError;
	}

	for( j=selfChecked; j<parcount; j++) parpass[j] = 0;
	for(argindex=0; argindex<argcount; argindex++){
		DaoType *partype, *argtype = argtypes[argindex];
		parindex = argindex + selfChecked;
		partype = partypes[parindex];
		if( partype->tid == DAO_PAR_VALIST ){
			DaoType *vlt = (DaoType*) partype->aux;
			for(; argindex<argcount; argindex++){
				argtype = argtypes[argindex];
				parpass[argindex+selfChecked] = vlt ? DaoType_MatchTo( argtype, vlt, defs ) : 1;
				if( parpass[argindex+selfChecked] == 0 ){
					DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_TYPE );
					partype = DaoType_DefineTypes( vlt, ns, defs );
					DString_AppendTypeError( s, argtype, partype );
					goto FinishError;
				}
			}
			break;
		}else if( (partype->attrib & DAO_TYPE_SELFNAMED) && partype->aux->xType.invar == 0 ){
			if( DaoType_CheckInvarMatch( argtype, (DaoType*) partype->aux, 1 ) == 0 ) goto WrongParamType;
		}
		if( argtype == NULL ){
			DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_TYPE );
			DString_AppendChars( s, "unknown parameter type \";\n" );
			goto FinishError;
		}
		if( argtype == NULL ){
			DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_TYPE );
			DString_AppendChars( s, "unknown parameter type \";\n" );
			goto FinishError;
		}else if( parindex >= parcount ){
			DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_NUMBER );
			DString_AppendChars( s, "too many parameters \";\n" );
			goto FinishError;
		}
		partype = (DaoType*) routType->args->items.pType[parindex]->aux;
		parpass[parindex] = DaoType_MatchTo( argtype, partype, defs );

#if 0
		printf( "%p %s %p %s\n", argtype->aux, argtype->name->chars, partype->aux, partype->name->chars );
		printf( "%i:  %i\n", parindex, parpass[parindex] );
#endif
		if( parpass[parindex] ) continue;

WrongParamType:
		s = AppendError( errors, routobj, DTE_PARAM_WRONG_TYPE );
		partype = DaoType_DefineTypes( partype, ns, defs );
		DString_AppendTypeError( s, argtype, partype );
		goto FinishError;
	}
	for(parindex=0; parindex<parcount; parindex++){
		i = partypes[parindex]->tid;
		if( i == DAO_PAR_VALIST ) break;
		if( parpass[parindex] ) continue;
		if( i != DAO_PAR_DEFAULT ){
			DString *s = AppendError( errors, routobj, DTE_PARAM_WRONG_NUMBER );
			DString_AppendChars( s, "too few parameters \";\n" );
			goto FinishError;
		}
		parpass[parindex] = 1;
	}

	/*
	   printf( "%s %i\n", routType->name->chars, *min );
	 */
FinishOK:
FinishError:
	DMap_Delete( defs );
}
DaoRoutine* DaoRoutine_Check( DaoRoutine *self, DaoType *selftype, DaoType *argtypes[], int np, int codemode, DList *errors )
{
	int i, n;
	DaoRoutine *rout = DaoRoutine_ResolveX( self, NULL, selftype, NULL, argtypes, np, codemode );
	if( rout ) return rout;
	if( self->overloads == NULL ){
		DaoRoutine_CheckError( self->nameSpace, self, self->routType, selftype, argtypes, np, codemode, errors );
		return NULL;
	}
	for(i=0,n=self->overloads->routines->size; i<n; i++){
		DaoRoutine *rout = self->overloads->routines->items.pRoutine[i];
		/*
		   printf( "=====================================\n" );
		   printf("ovld %i, %p %s : %s\n", i, rout, self->routName->chars, rout->routType->name->chars);
		 */
		DaoRoutine_CheckError( rout->nameSpace, rout, rout->routType, selftype, argtypes, np, codemode, errors );
	}
	return NULL;
}

void DaoPrintCallError( DList *errors, DaoStream *stream )
{
	DString *mbs = DString_New();
	int i, j, k, n;
	for(i=0,n=errors->size; i<n; i+=2){
		DaoType *routType = errors->items.pType[i];
		DaoRoutine *rout = NULL;
		if( routType->type == DAO_ROUTINE ){
			rout = errors->items.pRoutine[i];
			routType = rout->routType;
		}
		DaoStream_WriteChars( stream, "  ** " );
		DaoStream_WriteString( stream, errors->items.pString[i+1] );
		DaoStream_WriteChars( stream, "     Assuming  : " );
		if( rout ){
			if( isalpha( rout->routName->chars[0] ) ){
				DaoStream_WriteChars( stream, "routine " );
			}else{
				DaoStream_WriteChars( stream, "operator " );
			}
			k = DString_RFindChars( routType->name, "=>", routType->name->size );
			DString_Assign( mbs, rout->routName );
			DString_AppendChar( mbs, '(' );
			for(j=0; j<routType->args->size; ++j){
				if( j ) DString_AppendChar( mbs, ',' );
				DString_Append( mbs, routType->args->items.pType[j]->name );
			}
			DString_AppendChar( mbs, ')' );
			if( routType->cbtype ) DString_Append( mbs, routType->cbtype->name );
			if( routType->aux && routType->aux->type == DAO_TYPE ){
				DString_AppendChars( mbs, "=>" );
				DString_Append( mbs, routType->aux->xType.name );
			}
		}else{
			DaoStream_WriteString( stream, routType->name );
		}
		DString_AppendChars( mbs, ";\n" );
		DaoStream_WriteString( stream, mbs );
		if( rout ){
			DaoStream_WriteChars( stream, "     Reference : " );
			if( rout->body ){
				DaoStream_WriteChars( stream, "line " );
				DaoStream_WriteInt( stream, rout->defLine );
				DaoStream_WriteChars( stream, ", " );
			}
			DaoStream_WriteChars( stream, "file \"" );
			DaoStream_WriteString( stream, rout->nameSpace->name );
			DaoStream_WriteChars( stream, "\";\n" );
		}
		DString_Delete( errors->items.pString[i+1] );
	}
	DString_Delete( mbs );
}

static DaoInode* DaoInferencer_InsertNode( DaoInferencer *self, DaoInode *inode, int code, int addreg, DaoType *type )
{
	DaoInode *next = inode;
	DaoInode *prev = inode->prev;
	int i;

	inode = DaoInode_New();
	*(DaoVmCodeX*)inode = *(DaoVmCodeX*)next;
	inode->index = next->index;  /* Same basic block (same jump/branch group); */
	inode->code = code;
	if( addreg ){
		inode->c = self->types->size;
		DList_Append( self->types, type );
		DList_Append( self->consts, NULL );
	}
	if( prev ){
		prev->next = inode;
		inode->prev = prev;
	}
	inode->next = next;
	next->prev = inode;
	for(i=0; i<self->inodes->size; ++i){
		if( next == self->inodes->items.pInode[i] ){
			DList_Insert( self->inodes, inode, i );
			break;
		}
	}
	/* For proper setting up the jumps: */
	if( next->extra == NULL ) next->extra = inode;
	return inode;
}
static DaoInode* DaoInferencer_InsertMove( DaoInferencer *self, DaoInode *inode, unsigned short *op, DaoType *at, DaoType *ct )
{
	int K = DAO_FLOAT - DAO_BOOLEAN + 1;
	int code = DVM_MOVE_BB + K*(ct->tid - DAO_BOOLEAN) + (at->tid - DAO_BOOLEAN);
	DaoInode *move = DaoInferencer_InsertNode( self, inode, code, 1, ct );
	move->a = *op;
	move->b = 0;
	*op = move->c;
	return move;
}
static DaoInode* DaoInferencer_InsertUntag( DaoInferencer *self, DaoInode *inode, unsigned short *op, DaoType *ct )
{
	DaoInode *cast = DaoInferencer_InsertNode( self, inode, DVM_UNTAG, 1, ct );
	cast->a = *op;
	*op = cast->c;
	return cast;
}
static void DaoInferencer_InsertMove2( DaoInferencer *self, DaoInode *inode, DaoType *at, DaoType *ct )
{
	unsigned short opc = inode->c;
	int K = DAO_FLOAT - DAO_BOOLEAN + 1;
	int code = DVM_MOVE_BB + K*(ct->tid - DAO_BOOLEAN) + (at->tid - DAO_BOOLEAN);
	DaoInode *move = DaoInferencer_InsertNode( self, inode->next, code, 1, at );
	move->index = inode->index;  /* Same basic block (same jump/branch group); */
	inode->c = move->c;
	move->a = move->c;
	move->c = opc;
}
static void DaoInferencer_Finalize( DaoInferencer *self )
{
	DaoRoutineBody *body = self->routine->body;

	DaoRoutine_CodesFromInodes( self->routine, self->inodes );
	DList_Assign( body->regType, self->types );

	body->regCount = body->regType->size;
	DaoRoutine_SetupSimpleVars( self->routine );
}
static int DaoInferencer_ErrorTypeNotConsistent( DaoInferencer *self, DaoType *S, DaoType *T );
static DaoType* DaoInferencer_UpdateTypeX( DaoInferencer *self, int id, DaoType *type, int c )
{
	DaoNamespace *NS = self->routine->nameSpace;
	DaoType **types = self->types->items.pType;
	DMap *defs = (DMap*)DList_Back( self->typeMaps );

	/*
	// Do NOT update types that have been inferred:
	// Because if it has been inferred, some instructions may have been
	// specialized according to this inferred type. If it is allowed to
	// be updated here, other instructions may be specialized differently.
	// So the previously specialized instruction and the currently specialized
	// instruction will assume different types of the same register!
	//
	// Note 1:
	// Operations involving undefined types or type holder types should never
	// be specialized.
	//
	// Note 2:
	// Type holder types must be updated in order to support bytecode decoding.
	// Because variables can be declared with implicit types (type holder types),
	// and only types of declared variables will be encoded for bytecodes.
	// And due to Common Subexpression Elimination, the register of a declared
	// variable could be mapped to the result register (operand) of a specialized
	// operation (such as DVM_SUB_III). So when decoding bytecode, the result type
	// of such operation might be a type holder type, and if it is not allowed to
	// update during inference, the type checking will fail for such operations.
	//
	// Note 3:
	// It is extremely hard to encode inferred types from C/C++ modules if they
	// are user-defined and never explicitly expressed in Dao source code.
	// In particular, user-defined types inferred from function calls of wrapped
	// functions cannot be reasonably encoded due to function overloading and
	// parametric polymorphism.
	*/
	if( types[id] != NULL && !(types[id]->attrib & (DAO_TYPE_SPEC|DAO_TYPE_UNDEF)) ){
		return types[id];
	}

	/* If c == 0, the de-const type should be used: */
	if( type->invar && c == 0 ) type = DaoType_GetBaseType( type );

	/*
	// Type specialization should only be done for explicit variable declaration.
	// This is handled in type inference for DVM_SETVG and DVM_MOVE.
	*/

#if 0
	if( types[id] != NULL ){
		/*
		// Specialize the declared or previously inferred (not completely specialized) type.
		// For example:  var x: @T|none = (1, 2);
		*/
		DaoType_ResetTypeHolders( types[id], defs );
		if( DaoType_MatchTo( type, types[id], defs ) == 0 ) return types[id];
		type = DaoType_DefineTypes( types[id], NS, defs );
	}else{
		if( type->attrib & DAO_TYPE_SPEC ) type = DaoType_DefineTypes( type, NS, defs );
	}
#endif

	GC_Assign( & types[id], type );
	return types[id];
}
static DaoType* DaoInferencer_UpdateType( DaoInferencer *self, int id, DaoType *type )
{
	return DaoInferencer_UpdateTypeX( self, id, type, 1 );
}
static DaoType* DaoInferencer_UpdateVarType( DaoInferencer *self, int id, DaoType *type )
{
	return DaoInferencer_UpdateTypeX( self, id, type, 0 );
}
void DaoInferencer_PrintCodeSnippet( DaoInferencer *self, DaoStream *stream, int k )
{
	DString* mbs = DString_New();
	DaoVmCodeX **codes = self->inodes->items.pVmc;
	int debug = self->routine->nameSpace->vmSpace->options & DAO_OPTION_DEBUG;
	int prev = debug ? 16 : 1;
	int next = debug ? 8 : 1;
	int j, m = self->routine->body->annotCodes->size;
	int j1 = k >= prev ? k-prev : 0;
	int j2 = (k+next) < m ? k+next : m-1;

	DaoStream_WriteChars( stream, "In code snippet:\n" );
	for(j=j1; j<=j2; ++j){
		DaoRoutine_FormatCode( self->routine, j, *codes[j], mbs );
		DaoStream_WriteChars( stream, j==k ? ">>" : "  " );
		DaoStream_WriteString( stream, mbs );
	}
	DString_Delete( mbs );
}
static void DaoInferencer_WriteErrorHeader2( DaoInferencer *self )
{
	DaoRoutine *routine = self->routine;
	DaoStream  *stream = routine->nameSpace->vmSpace->errorStream;
	int invarinit = !!(routine->attribs & DAO_ROUT_INITOR);
	char char50[50], char200[200];

	if( invarinit ){
		invarinit &= routine->routHost->tid == DAO_OBJECT;
		invarinit &= !!(routine->routHost->aux->xClass.attribs & DAO_CLS_INVAR);
	}

	DaoStream_SetColor( stream, "white", "red" );
	DaoStream_WriteChars( stream, "[[ERROR]]" );
	DaoStream_SetColor( stream, NULL, NULL );

	DaoStream_WriteChars( stream, " in file \"" );
	DaoStream_WriteString( stream, routine->nameSpace->name );
	DaoStream_WriteChars( stream, "\":\n" );
	sprintf( char50, "  At line %i : ", routine->defLine );
	DaoStream_WriteChars( stream, char50 );
	if( invarinit ){
		DaoStream_WriteChars( stream, DaoTypingErrorString[DTE_INVALID_INVAR_INITOR] );
	}else{
		DaoStream_WriteChars( stream, "Invalid function definition" );
	}
	DaoStream_WriteChars( stream, " --- \" " );
	DaoStream_WriteString( stream, routine->routName );
	DaoStream_WriteChars( stream, "() \";\n" );
}
static void DaoInferencer_WriteErrorHeader( DaoInferencer *self )
{
	DaoVmCodeX *vmc;
	DaoRoutine *routine = self->routine;
	DaoStream  *stream = routine->nameSpace->vmSpace->errorStream;
	DaoVmCodeX **codes = self->inodes->items.pVmc;
	int invarinit = !!(routine->attribs & DAO_ROUT_INITOR);
	char char50[50], char200[200];

	if( invarinit ){
		invarinit &= routine->routHost->tid == DAO_OBJECT;
		invarinit &= !!(routine->routHost->aux->xClass.attribs & DAO_CLS_INVAR);
	}

	self->error = 1;
	if( self->silent ) return;

	vmc = self->inodes->items.pVmc[self->currentIndex];
	sprintf( char200, "%s:%i,%i,%i", DaoVmCode_GetOpcodeName( vmc->code ), vmc->a, vmc->b, vmc->c );

	DaoInferencer_WriteErrorHeader2( self );

	sprintf( char50, "  At line %i : ", vmc->line );
	DaoStream_WriteChars( stream, char50 );
	DaoStream_WriteChars( stream, "Invalid virtual machine instruction --- \" " );
	DaoStream_WriteChars( stream, char200 );
	DaoStream_WriteChars( stream, " \";\n" );

	DaoInferencer_PrintCodeSnippet( self, stream, self->currentIndex );
}
static void DaoInferencer_WriteErrorGeneral( DaoInferencer *self, int error )
{
	char char50[50];
	DaoRoutine *routine = self->routine;
	DaoStream  *stream = routine->nameSpace->vmSpace->errorStream;
	DaoVmCodeX *vmc = self->inodes->items.pVmc[self->currentIndex];
	DString *mbs;

	if( error == 0 ) return;

	self->error = 1;
	if( self->silent ) return;
	sprintf( char50, "  At line %i : ", vmc->line );

	mbs = DString_New();
	DaoStream_WriteChars( stream, char50 );
	DaoStream_WriteChars( stream, DaoTypingErrorString[error] );
	DaoStream_WriteChars( stream, " --- \" " );
	DaoRoutine_AnnotateCode( routine, *vmc, mbs, 32 );
	DaoStream_WriteString( stream, mbs );
	if( error == DTE_FIELD_NOT_EXIST ){
		DaoStream_WriteChars( stream, " for " );
		DaoStream_WriteChars( stream, self->type_source->name->chars );
	}
	DaoStream_WriteChars( stream, " \";\n" );
	DString_Delete( mbs );
}
static void DaoInferencer_WriteErrorSpecific( DaoInferencer *self, int error )
{
	char char50[50];
	int annot_first = self->annot_first;
	int annot_last = self->annot_last;
	DaoRoutine *routine = self->routine;
	DaoStream  *stream = routine->nameSpace->vmSpace->errorStream;
	DaoVmCodeX *vmc = self->inodes->items.pVmc[self->currentIndex];
	DaoVmCodeX vmc2 = *vmc;
	DString *mbs;

	if( error == 0 ) return;

	self->error = 1;
	if( self->silent ) return;
	sprintf( char50, "  At line %i : ", vmc->line );

	mbs = DString_New();
	DaoStream_WriteChars( stream, char50 );
	DaoStream_WriteChars( stream, DaoTypingErrorString[error] );
	DaoStream_WriteChars( stream, " --- \" " );
	if( error == DTE_TYPE_NOT_INITIALIZED ){
		vmc2.middle = 0;
		vmc2.first = annot_first;
		vmc2.last = annot_last > annot_first ? annot_last - annot_first : 0;
		DaoRoutine_AnnotateCode( routine, vmc2, mbs, 32 );
	}else if( error == DTE_TYPE_NOT_MATCHING || error == DTE_TYPE_NOT_CONSISTENT ){
		DString_SetChars( mbs, "'" );
		DString_AppendChars( mbs, self->type_source ? self->type_source->name->chars : "none" );
		DString_AppendChars( mbs, error == DTE_TYPE_NOT_MATCHING ? "' for '" : "' with '" );
		if( self->type_target ){
			DString_AppendChars( mbs, self->type_target->name->chars );
		}else if( self->tid_target <= DAO_TUPLE ){
			DString_AppendChars( mbs, coreTypeNames[self->tid_target] );
		}
		DString_AppendChar( mbs, '\'' );
	}else{
		DaoRoutine_AnnotateCode( routine, *vmc, mbs, 32 );
	}
	DaoStream_WriteString( stream, mbs );
	DaoStream_WriteChars( stream, " \";\n" );
	DString_Delete( mbs );
}
static int DaoInferencer_Error( DaoInferencer *self, int error )
{
	DaoInferencer_WriteErrorHeader( self );
	if( self->errors->size ){
		DaoPrintCallError( self->errors, self->routine->nameSpace->vmSpace->errorStream );
		return 0;
	}
	DaoInferencer_WriteErrorGeneral( self, error );
	return 0;
}
static int DaoInferencer_ErrorModifyConst( DaoInferencer *self )
{
	return DaoInferencer_Error( self, DTE_CONST_WRONG_MODIFYING );
}
static int DaoInferencer_ErrorTypeNotMatching( DaoInferencer *self, DaoType *S, DaoType *T )
{
	if( S ) self->type_source = S;
	if( T ) self->type_target = T;
	DaoInferencer_WriteErrorHeader( self );
	DaoInferencer_WriteErrorGeneral( self, DTE_OPERATION_NOT_VALID );
	DaoInferencer_WriteErrorSpecific( self, DTE_TYPE_NOT_MATCHING );
	return 0;
}
static int DaoInferencer_ErrorTypeNotConsistent( DaoInferencer *self, DaoType *S, DaoType *T )
{
	if( S ) self->type_source = S;
	if( T ) self->type_target = T;
	DaoInferencer_WriteErrorHeader( self );
	DaoInferencer_WriteErrorGeneral( self, DTE_OPERATION_NOT_VALID );
	DaoInferencer_WriteErrorSpecific( self, DTE_TYPE_NOT_CONSISTENT );
	return 0;
}
static int DaoInferencer_ErrorTypeID( DaoInferencer *self, DaoType *S, int tid )
{
	self->type_source = S;
	self->tid_target = tid;
	DaoInferencer_WriteErrorHeader( self );
	DaoInferencer_WriteErrorGeneral( self, DTE_TYPE_NOT_MATCHING );
	DaoInferencer_WriteErrorSpecific( self, DTE_TYPE_NOT_MATCHING );
	return 0;
}
static int DaoInferencer_ErrorNotInitialized( DaoInferencer *self, int error, int first, int last )
{
	self->annot_first = first;
	self->annot_last = last;
	DaoInferencer_WriteErrorHeader( self );
	DaoInferencer_WriteErrorGeneral( self, error );
	DaoInferencer_WriteErrorSpecific( self, DTE_TYPE_NOT_INITIALIZED );
	return 0;
}
static int DaoInferencer_ErrorInvalidIndex( DaoInferencer *self )
{
	return DaoInferencer_Error( self, DTE_INDEX_NOT_VALID );
}

#define DaoType_LooseChecking(t) (t->tid & DAO_ANY)

static int DaoInferencer_AssertPairNumberType( DaoInferencer *self, DaoType *type )
{
	DaoType *itp = type->args->items.pType[0];
	if( itp->tid == DAO_PAR_NAMED ) itp = & itp->aux->xType;
	if( itp->tid > DAO_FLOAT && ! DaoType_LooseChecking(itp) ) return 0;
	itp = type->args->items.pType[1];
	if( itp->tid == DAO_PAR_NAMED ) itp = & itp->aux->xType;
	if( itp->tid > DAO_FLOAT && ! DaoType_LooseChecking(itp) ) return 0;
	return 1;
}


#define AssertTypeMatching( source, target, defs ) \
	if( !(source->tid & DAO_ANY ) && DaoType_MatchTo( source, target, defs ) ==0 ) \
		return DaoInferencer_ErrorTypeNotMatching( self, source, target );

#define AssertTypeIdMatching( source, id ) \
	if( source == NULL || source->tid != id ) \
		return DaoInferencer_ErrorTypeID( self, source, id );

#define AssertPairNumberType( tp ) \
	if( DaoInferencer_AssertPairNumberType( self, tp ) == 0 ) \
		return DaoInferencer_ErrorTypeNotMatching( self, NULL, NULL );

static DaoType* DaoInferencer_HandleSlicedType( DaoInferencer *self, DaoType *type )
{
	int i, tid = type->tid;
	const char *name = coreTypeNames[tid];
	DaoNamespace *NS = self->routine->nameSpace;
	DList *types = self->array;

	DList_Clear( types );
	for(i=0; i<type->args->size; ++i){
		DaoType *it = type->args->items.pType[i];
		if( it->tid && it->tid <= DAO_ENUM ){
			it = DaoType_GetBaseType( it );
		}else if( it->tid == DAO_PAR_NAMED || it->tid == DAO_PAR_VALIST ){
			const char *fn = it->fname->chars;
			DaoType *tp = (DaoType*) it->aux;
			if( tp->tid && tp->tid <= DAO_ENUM ){
				tp = DaoType_GetBaseType( tp );
			}else{
				tp = DaoType_GetInvarType( tp );
			}
			it = DaoNamespace_MakeType( NS, fn, it->tid, (DaoValue*) tp, NULL, 0 );
		}else{
			it = DaoType_GetInvarType( it );
		}
		DList_Append( types, it );
	}
	return DaoNamespace_MakeType( NS, name, tid, NULL, types->items.pType, types->size );
}

int DaoInferencer_HandleGetItem( DaoInferencer *self, DaoInode *inode, DMap *defs )
{
	int code = inode->code;
	int opa = inode->a;
	int opb = inode->b;
	int opc = inode->c;
	DList *errors = self->errors;
	DaoInteger integer = {DAO_INTEGER,0,0,0,1,0};
	DaoType *type, **tp, **types = self->types->items.pType;
	DaoNamespace *NS = self->routine->nameSpace;
	DaoVmSpace *VMS = NS->vmSpace;
	DaoVmCodeX *vmc = (DaoVmCodeX*) inode;
	DaoType *at = types[opa];
	DaoType *bt = types[opb];
	DaoType *ct = types[opc];
	DaoValue *value = NULL;
	DNode *node;
	int k;

	if( code == DVM_GETI ){
		bt = types[opb];
		value = self->consts->items.pValue[opb];
	}else if( code == DVM_GETDI ){
		bt = VMS->typeInt;
		integer.value = opb;
		value = (DaoValue*)(DaoInteger*)&integer;
	}

	ct = NULL;
	if( DaoType_LooseChecking( at ) ){
		ct = at;
	}else if( at->tid == DAO_PAR_NAMED && code == DVM_GETDI ){
		ct = opb ? (DaoType*) at->aux : VMS->typeString; // TODO;
	}else if( at->tid >= DAO_BOOLEAN && at->tid <= DAO_COMPLEX ){
		ct = at->core->CheckGetItem( at, & bt, 1, self->routine );
		if( ct == NULL ) goto InvalidIndex;
	}else if( at->tid == DAO_STRING ){
		ct = at->core->CheckGetItem( at, & bt, 1, self->routine );
		if( ct == NULL ) goto InvalidIndex;
		if( code == DVM_GETI && bt->realnum && ct->tid == DAO_INTEGER ){
			if( bt->tid != DAO_INTEGER ){
				DaoInferencer_InsertMove( self, inode, & inode->b, bt, VMS->typeInt );
			}
			vmc->code = DVM_GETI_SI;
		}
	}else if( at->tid == DAO_TYPE ){
		at = at->args->items.pType[0];
		if( at->tid == DAO_ENUM && at->mapNames ){
			ct = at; /* TODO const index */
		}else{
			self->type_source = at;
			goto NotExist;
		}
	}else if( at->tid == DAO_LIST ){
		ct = at->core->CheckGetItem( at, & bt, 1, self->routine );
		if( ct == NULL ) goto InvalidIndex;
		if( code == DVM_GETI && bt->realnum && ct == at->args->items.pType[0] ){
			if( ct->tid >= DAO_BOOLEAN && ct->tid <= DAO_COMPLEX ){
				vmc->code = DVM_GETI_LBI + ct->tid - DAO_BOOLEAN;
			}else if( ct->tid == DAO_STRING ){
				vmc->code = DVM_GETI_LSI;
			}else if( ct->tid >= DAO_ARRAY && ct->tid < DAO_ANY ){
				vmc->code = DVM_GETI_LI; /* For skipping type checking; */
			}
			if( bt->tid != DAO_INTEGER ){
				DaoInferencer_InsertMove( self, inode, & inode->b, bt, VMS->typeInt );
			}
		}
	}else if( at->tid == DAO_MAP ){
		ct = at->core->CheckGetItem( at, & bt, 1, self->routine );
		if( ct == NULL ) goto InvalidKey;
	}else if( at->tid == DAO_ARRAY ){
		ct = at->core->CheckGetItem( at, & bt, 1, self->routine );
		if( ct == NULL ) goto InvalidIndex;
		if( code == DVM_GETI && bt->realnum && ct == at->args->items.pType[0] ){
			if( ct->realnum || ct->tid == DAO_COMPLEX ){
				vmc->code = DVM_GETI_ABI + ct->tid - DAO_BOOLEAN;
			}
			if( bt->tid != DAO_INTEGER ){
				DaoInferencer_InsertMove( self, inode, & inode->b, bt, VMS->typeInt );
			}
		}
	}else if( at->tid == DAO_TUPLE ){
		ct = at;
		/* tuple slicing with constant index range, will produce a tuple with type
		// determined from the index range. For variable range, it produces tuple<...>. */
		if( value && value->xBase.subtype == DAO_RANGE ){
			DaoValue *first = value->xTuple.values[0];
			DaoValue *second = value->xTuple.values[1];
			daoint start = DaoValue_GetInteger( first );
			daoint end = DaoValue_GetInteger( second );
			/* Note: a tuple may contain more items than what are explicitly typed. */
			if( start < 0 || end < 0 ) goto InvalidIndex; /* No support for negative index; */
			if( at->variadic == 0 && start >= at->args->size ) goto InvalidIndex;
			if( at->variadic == 0 && end >= at->args->size ) goto InvalidIndex;
			if( first->type > DAO_FLOAT || second->type > DAO_FLOAT ) goto InvalidIndex;
			if( first->type == DAO_NONE && second->type == DAO_NONE ){
				ct = at;
			}else{
				end = second->type == DAO_NONE ? at->args->size : end + 1;
				if( end >= at->args->size ) end = at->args->size;
				if( start >= at->args->size ) end = start;
				tp = at->args->items.pType + start;
				ct = DaoNamespace_MakeType( NS, "tuple", DAO_TUPLE, NULL, tp, end-start );
			}
		}else if( value && value->type ){
			if( value->type > DAO_FLOAT ) goto InvalidIndex;
			k = DaoValue_GetInteger( value );
			if( k < 0 ) goto InvalidIndex; /* No support for negative index; */
			if( at->variadic && k >= (at->args->size - 1) ){
				type = (DaoType*) at->args->items.pType[at->args->size-1]->aux;
				ct = type ? type : VMS->typeAny;
			}else{
				if( k >= at->args->size ) goto InvalidIndex;
				ct = at->args->items.pType[ k ];
				if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
				if( k <= 0xffff ){
					vmc->b = k;
					if( ct->tid >= DAO_BOOLEAN && ct->tid <= DAO_COMPLEX ){
						vmc->code = DVM_GETF_TB + ( ct->tid - DAO_BOOLEAN );
					}else{
						vmc->code = DVM_GETF_TX; /* For skipping type checking; */
					}
				}
			}
		}else{
			ct = at->core->CheckGetItem( at, & bt, 1, self->routine );
			if( ct == NULL ) goto InvalidIndex;
			if( code == DVM_GETI && bt->realnum ){
				vmc->code = DVM_GETI_TI;
				if( bt->tid != DAO_INTEGER ){
					DaoInferencer_InsertMove( self, inode, & inode->b, bt, VMS->typeInt );
				}
			}
		}
	}else if( at->tid == DAO_CLASS && at->aux == NULL ){  /* "class" */
		ct = VMS->typeAny;
	}else if( at->core ){
		if( at->core->CheckGetItem == NULL ) goto WrongContainer;
		ct = at->core->CheckGetItem( at, & bt, 1, self->routine );
		if( ct == NULL ) goto InvalidIndex;
	}else{
		goto WrongContainer;
	}
	if( ct == NULL ) goto InvalidKey;
	if( at->tid >= DAO_ARRAY ){  /* invar<X<Y>> to X<invar<Y>> */
		if( at->konst && ct->konst == 0 ) ct = DaoType_GetConstType( ct );
		if( at->invar && ct->invar == 0 ) ct = DaoType_GetInvarType( ct );
		if( bt->tid == DAO_NONE || bt->subtid == DAO_RANGE ){
			if( ct->invar && ct->konst == 0 ){
				if( at->tid == DAO_ARRAY ){
					ct = DaoType_GetBaseType( ct );
				}else if( at->tid == DAO_LIST || at->tid == DAO_MAP || at->tid == DAO_TUPLE ){
					ct = DaoInferencer_HandleSlicedType( self, ct );
				}
			}
		}
	}else{
		if( at->invar && ct->invar ) ct = DaoType_GetBaseType( ct );
	}
	DaoInferencer_UpdateType( self, opc, ct );
	AssertTypeMatching( ct, types[opc], defs );
	return 1;
NotMatch : return DaoInferencer_ErrorTypeNotMatching( self, NULL, NULL );
NotExist : return DaoInferencer_Error( self, DTE_FIELD_NOT_EXIST );
WrongContainer : return DaoInferencer_Error( self, DTE_TYPE_WRONG_CONTAINER );
InvalidIndex : return DaoInferencer_Error( self, DTE_INDEX_NOT_VALID );
InvalidKey : return DaoInferencer_Error( self, DTE_KEY_NOT_VALID );
}
int DaoInferencer_HandleGetMItem( DaoInferencer *self, DaoInode *inode, DMap *defs )
{
	int opa = inode->a;
	int opb = inode->b;
	int opc = inode->c;
	DList *errors = self->errors;
	DaoVmSpace *VMS = self->routine->nameSpace->vmSpace;
	DaoType *type, **types = self->types->items.pType;
	DaoType *at = types[opa];
	DaoType *ct = types[opc];
	DaoInode *inode2;
	DNode *node;
	int j;

	ct = at;
	if( opb == 0 ){
		ct = at;
	}else if( DaoType_LooseChecking( at ) ){
		ct = at;
	}else if( at->tid == DAO_STRING ){
		ct = at->core->CheckGetItem( at, types + opa + 1, opb, self->routine );
		if( ct == NULL ) goto InvalidIndex;
	}else if( at->tid == DAO_ARRAY ){
		int max = DAO_NONE, min = DAO_COMPLEX;
		ct = type = at->args->items.pType[0];
		for(j=1; j<=opb; ++j){
			int tid = types[j+opa]->tid;
			if( tid > max ) max = tid;
			if( tid < min ) min = tid;
		}
		ct = at->core->CheckGetItem( at, types + opa + 1, opb, self->routine );
		if( ct == NULL ) goto InvalidIndex;
		if( ct->tid && ct->tid <= DAO_COMPLEX ){
			if( min >= DAO_INTEGER && max <= DAO_FLOAT ){
				inode->code = DVM_GETMI_ABI + (ct->tid - DAO_BOOLEAN);
				if( max > DAO_INTEGER ){
					inode2 = DaoInferencer_InsertNode( self, inode, DVM_MOVE_PP, 1, at );
					inode2->a = inode->a;
					inode->a = self->types->size - 1;
					for(j=1; j<=opb; ++j){
						unsigned short op = j+opa;
						DaoInferencer_InsertMove( self, inode, & op, types[j+opa], VMS->typeInt );
					}
				}
			}
		}
	}else if( at->tid == DAO_MAP ){
		goto InvalidIndex;
	}else if( at->tid == DAO_CLASS && at->aux == NULL ){  /* "class" */
		ct = VMS->typeAny;
	}else if( at->core ){
		if( at->core->CheckGetItem == NULL ) goto WrongContainer;
		ct = at->core->CheckGetItem( at, types + opa + 1, opb, self->routine );
		if( ct == NULL ) goto InvalidIndex;
	}else{
		goto WrongContainer;
	}
	if( at->tid >= DAO_ARRAY ){
		if( at->konst && ct->konst == 0 ) ct = DaoType_GetConstType( ct );
		if( at->invar && ct->invar == 0 ) ct = DaoType_GetInvarType( ct );
	}else{
		if( at->invar && ct->invar ) ct = DaoType_GetBaseType( ct );
	}
	DaoInferencer_UpdateType( self, opc, ct );
	AssertTypeMatching( ct, types[opc], defs );
	return 1;
WrongContainer : return DaoInferencer_Error( self, DTE_TYPE_WRONG_CONTAINER );
InvalidIndex : return DaoInferencer_Error( self, DTE_INDEX_NOT_VALID );
}
int DaoInferencer_HandleGetField( DaoInferencer *self, DaoInode *inode, DMap *defs )
{
	int code = inode->code;
	int opa = inode->a;
	int opb = inode->b;
	int opc = inode->c;
	DList *errors = self->errors;
	DList  *routConsts = self->routine->routConsts->value;
	DaoType **type2, **types = self->types->items.pType;
	DaoValue *value, **consts = self->consts->items.pValue;
	DaoNamespace *NS = self->routine->nameSpace;
	DaoVmSpace *VMS = self->routine->nameSpace->vmSpace;
	DaoClass *klass, *hostClass = self->hostClass;
	DaoVmCodeX *vmc = (DaoVmCodeX*) inode;
	DaoType *at = types[opa];
	DaoType *ct = types[opc];
	DaoString *field; 
	DString *name;
	DNode *node;
	int k;
	DaoType **pars = NULL;
	int npar = 0;
	int ak = 0;

	ct = NULL;
	value = routConsts->items.pValue[opb];
	if( value == NULL || value->type != DAO_STRING ) goto NotMatch;
	field = (DaoString*) value;
	name = value->xString.value;
	ak = at->tid == DAO_CLASS;
	self->type_source = at;
	if( DaoType_LooseChecking( at ) ){
		/* allow less strict typing: */
		ct = VMS->typeUdf;
	}else if( at->tid == DAO_COMPLEX ){
		ct = at->core->CheckGetField( at, field, self->routine );
		if( ct == NULL ) goto InvalidField;
		inode->code = DVM_GETF_CX;
		inode->b = strcmp( name->chars, "imag" ) == 0;
	}else if( at->tid == DAO_TYPE ){
		self->type_source = at;
		at = at->args->items.pType[0];
		if( at->tid == DAO_ENUM && at->mapNames ){
			if( DMap_Find( at->mapNames, name ) == NULL ) goto NotExist;
			ct = at;
		}else{
			goto NotExist;
		}
	}else if( at->tid == DAO_CLASS && at->aux == NULL ){  /* "class" */
		ct = VMS->typeAny;
	}else if( at->tid == DAO_CLASS || at->tid == DAO_OBJECT ){
		DaoClass *klass = (DaoClass*) at->aux;
		DaoValue *data = DaoClass_GetData( klass, name, hostClass );
		int j, k;

		ct = at->core->CheckGetField( at, field, self->routine );
		if( ct == NULL ) goto InvalidField;

		if( data != NULL && data->type != DAO_NONE ){
			if( data->xBase.subtype == DAO_CLASS_CONSTANT ){
				GC_Assign( & consts[opc], data->xConst.value );
			}

			/* specialize instructions for finalized class/instance: */
			j = DaoClass_GetDataIndex( klass, name );
			k = LOOKUP_ST( j );
			vmc->b = LOOKUP_ID( j );
			if( ct && ct->tid >= DAO_BOOLEAN && ct->tid <= DAO_COMPLEX ){
				switch( k ){
				case DAO_CLASS_CONSTANT : code = ak ? DVM_GETF_KCB : DVM_GETF_OCB; break;
				case DAO_CLASS_VARIABLE : code = ak ? DVM_GETF_KGB : DVM_GETF_OGB; break;
				case DAO_OBJECT_VARIABLE : code = DVM_GETF_OVB; break;
				}
				code += ct->tid - DAO_BOOLEAN;
			}else{
				switch( k ){
				case DAO_CLASS_CONSTANT : code = ak ? DVM_GETF_KC : DVM_GETF_OC; break;
				case DAO_CLASS_VARIABLE : code = ak ? DVM_GETF_KG : DVM_GETF_OG; break;
				case DAO_OBJECT_VARIABLE : code = DVM_GETF_OV; break;
				}
			}
			vmc->code = code;
		}
	}else if( at->tid == DAO_TUPLE ){
		ct = at->core->CheckGetField( at, field, self->routine );
		if( ct == NULL ) goto InvalidField;

		node = MAP_Find( at->mapNames, name );
		k = node->value.pInt;
		if( k < 0xffff ){
			if( ct->tid >= DAO_BOOLEAN && ct->tid <= DAO_COMPLEX ){
				vmc->code = DVM_GETF_TB + ( ct->tid - DAO_BOOLEAN );
				vmc->b = k;
			}else{
				/* for skipping type checking */
				vmc->code = DVM_GETF_TX;
				vmc->b = k;
			}
		}
	}else if( at->tid == DAO_NAMESPACE ){
		ct = VMS->typeAny;
		if( consts[opa] && consts[opa]->type == DAO_NAMESPACE ){
			DaoNamespace *ans = & consts[opa]->xNamespace;
			k = DaoNamespace_FindVariable( ans, name );
			if( k >=0 ){
				ct = DaoNamespace_GetVariableType( ans, k );
			}else{
				k = DaoNamespace_FindConst( ans, name );
				value = DaoNamespace_GetConst( ans, k );
				if( value ) ct = DaoNamespace_GetType( ans, value );
			}
			if( k <0 ) goto NotExist;
		}
	}else if( at->core ){
		if( at->core->CheckGetField == NULL ) goto InvalidField;
		ct = at->core->CheckGetField( at, field, self->routine );
		if( ct == NULL ) goto InvalidField;

		if( at->tid != DAO_INTERFACE ){
			value = DaoType_FindValue( at, name );
			if( value ){
				GC_Assign( & consts[opc], value );
			}
		}
	}
	if( ct && ct->tid == DAO_ROUTINE && (ct->attrib & DAO_TYPE_SELF) ){
		DaoType *selftype = (DaoType*) ct->args->items.pType[0]->aux;
		/* Remove type holder bindings for the self parameter: */
		DaoType_ResetTypeHolders( selftype, defs );
		DaoType_MatchTo( at, selftype, defs );
		ct = DaoType_DefineTypes( ct, NS, defs );
	}
	if( at->tid != DAO_CLASS && at->tid != DAO_NAMESPACE ){
		if( at->konst && ct->konst == 0 ) ct = DaoType_GetConstType( ct );
		if( at->invar && ct->invar == 0 ) ct = DaoType_GetInvarType( ct );
	}
	DaoInferencer_UpdateType( self, opc, ct );
	AssertTypeMatching( ct, types[opc], defs );
	return 1;
NotMatch : return DaoInferencer_ErrorTypeNotMatching( self, NULL, NULL );
NotPermit : return DaoInferencer_Error( self, DTE_FIELD_NOT_PERMIT );
NotExist : return DaoInferencer_Error( self, DTE_FIELD_NOT_EXIST );
NeedInstVar : return DaoInferencer_Error( self, DTE_FIELD_OF_INSTANCE );
InvalidField : return DaoInferencer_Error( self, DTE_OPERATION_NOT_VALID ); // TODO;
}
int DaoInferencer_HandleSetItem( DaoInferencer *self, DaoInode *inode, DMap *defs )
{
	int code = inode->code;
	int opa = inode->a;
	int opb = inode->b;
	int opc = inode->c;
	DList *errors = self->errors;
	DaoInteger integer = {DAO_INTEGER,0,0,0,1,0};
	DaoType *tt, *type, **types = self->types->items.pType;
	DaoValue *value, **consts = self->consts->items.pValue;
	DaoClass *hostClass = self->hostClass;
	DaoVmSpace *VMS = self->routine->nameSpace->vmSpace;
	DaoVmCodeX *vmc = (DaoVmCodeX*) inode;
	DaoRoutine *meth, *rout;
	DaoType *at = types[opa];
	DaoType *bt = types[opb];
	DaoType *ct = types[opc];
	int k, K;

	if( ct == NULL ) goto ErrorTyping;
	integer.value = opb;
	value = (DaoValue*)(DaoInteger*)&integer;
	bt = VMS->typeInt;
	if( code == DVM_SETI ){
		bt = types[opb];
		value = consts[opb];
	}
	if( DaoType_LooseChecking( ct ) ) return 1;
	switch( ct->tid ){
	case DAO_STRING :
		K = ct->core->CheckSetItem( ct, & bt, 1, at, self->routine );
		if( K ) goto InvalidIndex;
		if( code == DVM_SETI ){
			if( at->realnum && bt->realnum ){
				vmc->code = DVM_SETI_SII;
				if( at->tid != DAO_INTEGER )
					DaoInferencer_InsertMove( self, inode, & inode->a, at, VMS->typeInt );
				if( bt->tid != DAO_INTEGER )
					DaoInferencer_InsertMove( self, inode, & inode->b, bt, VMS->typeInt );
			}
		}
		break;
	case DAO_LIST :
		K = ct->core->CheckSetItem( ct, & bt, 1, at, self->routine );
		if( K ) goto InvalidIndex;
		if( bt->tid >= DAO_BOOLEAN && bt->tid <= DAO_FLOAT ){
			ct = ct->args->items.pType[0];
			AssertTypeMatching( at, ct, defs );
			if( code == DVM_SETI ){
				if( ct->tid && ct->tid <= DAO_COMPLEX && at->tid && at->tid <= DAO_COMPLEX ){
					if( at->tid != ct->tid )
						DaoInferencer_InsertMove( self, inode, & inode->a, at, ct );
					vmc->code = DVM_SETI_LBIB + ct->tid - DAO_BOOLEAN;
				}else if( at->tid == DAO_STRING && ct->tid == DAO_STRING ){
					vmc->code = DVM_SETI_LSIS;
				}else{
					if( at == ct || ct->tid == DAO_ANY ) vmc->code = DVM_SETI_LI;
				}
				if( bt->tid != DAO_INTEGER )
					DaoInferencer_InsertMove( self, inode, & inode->b, bt, VMS->typeInt );
			}
		}
		break;
	case DAO_MAP :
		K = ct->core->CheckSetItem( ct, & bt, 1, at, self->routine );
		if( K ) goto InvalidIndex;
		break;
	case DAO_ARRAY :
		K = ct->core->CheckSetItem( ct, & bt, 1, at, self->routine );
		if( K ) goto InvalidIndex;
		if( bt->tid >= DAO_INTEGER && bt->tid <= DAO_FLOAT ){
			if( DaoType_MatchTo( at, ct, defs ) ) break;
			ct = ct->args->items.pType[0];
			/* array[i] */
			if( code == DVM_SETI ){
				if( ct->realnum && at->realnum ){
					if( at->tid != ct->tid )
						DaoInferencer_InsertMove( self, inode, & inode->a, at, ct );
					vmc->code = DVM_SETI_ABIB + ct->tid - DAO_BOOLEAN;
				}else if( ct->tid == DAO_COMPLEX && at->tid && at->tid <= DAO_COMPLEX ){
					if( at->tid != DAO_COMPLEX )
						DaoInferencer_InsertMove( self, inode, & inode->a, at, ct );
					vmc->code = DVM_SETI_ACIC;
				}else if( at->tid != DAO_UDT && at->tid != DAO_ANY ){
					AssertTypeMatching( at, ct, defs );
				}
				if( bt->tid != DAO_INTEGER )
					DaoInferencer_InsertMove( self, inode, & inode->b, bt, VMS->typeInt );
			}
			AssertTypeMatching( at, ct, defs );
		}
		break;
	case DAO_TUPLE :
		K = ct->core->CheckSetItem( ct, & bt, 1, at, self->routine );
		if( K ) goto InvalidIndex;
		if( value && value->type ){
			if( value->type > DAO_FLOAT ) goto InvalidIndex;
			k = DaoValue_GetInteger( value );
			if( k < 0 ) goto InvalidIndex;
			if( ct->variadic == 0 && k >= (int)ct->args->size ) goto InvalidIndex;
			if( ct->variadic && k >= (ct->args->size - 1) ){
				ct = ct->args->items.pType[ ct->args->size - 1 ];
				ct = ct->aux ? (DaoType*) ct->aux : VMS->typeAny;
			}else{
				ct = ct->args->items.pType[ k ];
			}
			if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
			AssertTypeMatching( at, ct, defs );
			if( k <= 0xffff ){
				if( ct->tid && ct->tid <= DAO_COMPLEX && at->tid && at->tid <= DAO_COMPLEX ){
					vmc->b = k;
					if( at->tid != ct->tid )
						DaoInferencer_InsertMove( self, inode, & inode->a, at, ct );
					vmc->code = DVM_SETF_TBB + ct->tid - DAO_BOOLEAN;
				}else if( at == ct || ct->tid == DAO_ANY ){
					vmc->b = k;
					if( at->tid ==DAO_STRING && ct->tid ==DAO_STRING ){
						vmc->code = DVM_SETF_TSS;
					}else if( at->tid >= DAO_ARRAY && at->tid <= DAO_TYPE && consts[opa] == NULL ){
						vmc->code = DVM_SETF_TPP;
					}else{
						vmc->code = DVM_SETF_TXX;
					}
				}
			}
		}else if( bt->realnum ){
			vmc->code = DVM_SETI_TI;
			if( bt->tid != DAO_INTEGER )
				DaoInferencer_InsertMove( self, inode, & inode->b, bt, VMS->typeInt );
		}
		break;
	default :
		if( ct->core == NULL || ct->core->CheckSetItem == NULL ) goto InvalidIndex;
		K = ct->core->CheckSetItem( ct, & bt, 1, at, self->routine );
		if( K ) goto InvalidIndex;
		break;
	}
	return 1;
NotMatch : return DaoInferencer_ErrorTypeNotMatching( self, NULL, NULL );
InvalidIndex : return DaoInferencer_Error( self, DTE_INDEX_NOT_VALID );
ErrorTyping: return DaoInferencer_Error( self, DTE_TYPE_NOT_MATCHING );
}
int DaoInferencer_HandleSetMItem( DaoInferencer *self, DaoInode *inode, DMap *defs )
{
	int opa = inode->a;
	int opb = inode->b;
	int opc = inode->c;
	DList *errors = self->errors;
	DaoType *type, **types = self->types->items.pType;
	DaoClass *hostClass = self->hostClass;
	DaoVmSpace *VMS = self->routine->nameSpace->vmSpace;
	DaoRoutine *meth, *rout;
	DaoType *at = types[opa];
	DaoType *ct = types[opc];
	DaoInode *inode2;
	DNode *node;
	int j, K, min, max;

	if( ct == NULL ) goto ErrorTyping;
	if( DaoType_LooseChecking( ct ) ) return 1;
	meth = NULL;
	switch( ct->tid ){
	case DAO_ARRAY :
		K = ct->core->CheckSetItem( ct, types + opc + 1, opb, at, self->routine );
		if( K ) goto InvalidIndex;
		max = DAO_NONE;
		min = DAO_COMPLEX;
		type = ct->args->items.pType[0];
		for(j=1; j<=opb; j++){
			int tid = types[j+opc]->tid;
			if( tid > max ) max = tid;
			if( tid < min ) min = tid;
		}
		if( type->tid <= DAO_COMPLEX && at->tid <= DAO_COMPLEX ){
			if( at->tid == DAO_COMPLEX &&  type->tid != DAO_COMPLEX ) goto ErrorTyping;
			if( min >= DAO_INTEGER && max <= DAO_FLOAT ){
				inode->code = DVM_SETMI_ABIB + (type->tid - DAO_BOOLEAN);
				if( at->tid != type->tid )
					DaoInferencer_InsertMove( self, inode, & inode->a, at, type );
				if( max > DAO_INTEGER ){
					inode2 = DaoInferencer_InsertNode( self, inode, DVM_MOVE_PP, 1, ct );
					inode2->c = inode->c;
					inode->c = self->types->size - 1;
					for(j=1; j<=opb; j++){
						unsigned short op = j+opc;
						DaoInferencer_InsertMove( self, inode, & op, types[j+opc], VMS->typeInt );
					}
				}
			}
		}
		break;
	case DAO_MAP :
		goto InvalidIndex;
	default :
		if( ct->core == NULL || ct->core->CheckSetItem == NULL ) goto WrongContainer;
		K = ct->core->CheckSetItem( ct, types + opc + 1, opb, at, self->routine );
		if( K ) goto InvalidIndex;
		break;
	}
	return 1;
WrongContainer : return DaoInferencer_Error( self, DTE_TYPE_WRONG_CONTAINER );
InvalidIndex : return DaoInferencer_Error( self, DTE_INDEX_NOT_VALID );
ErrorTyping: return DaoInferencer_Error( self, DTE_TYPE_NOT_MATCHING );
}
int DaoInferencer_HandleSetField( DaoInferencer *self, DaoInode *inode, DMap *defs )
{
	int opa = inode->a;
	int opb = inode->b;
	int opc = inode->c;
	DList *errors = self->errors;
	DList  *routConsts = self->routine->routConsts->value;
	DaoRoutine *routine = self->routine;
	DaoVmSpace *VMS = self->routine->nameSpace->vmSpace;
	DaoType *type, **types = self->types->items.pType;
	DaoValue *data, *value, **consts = self->consts->items.pValue;
	DaoClass *klass, *hostClass = self->hostClass;
	DaoVmCodeX *vmc = (DaoVmCodeX*) inode;
	DaoRoutine *meth, *rout;
	DaoType *at = types[opa];
	DaoType *ct = types[opc];
	DaoString *field;
	DString *name;
	DNode *node;
	int j = 0;
	int npar = 1;
	int ck = 0;
	int k, K;

#if 0
	printf( "a: %s\n", types[opa]->name->chars );
	printf( "c: %s\n", types[opc]->name->chars );
#endif
	value = routConsts->items.pValue[opb];
	if( value == NULL || value->type != DAO_STRING ) goto NotMatch;
	field = (DaoString*) value;
	name = value->xString.value;
	self->type_source = ct;
	switch( ct->tid ){
	case DAO_UDT :
	case DAO_ANY :
	case DAO_THT :
		/* allow less strict typing: */
		break;
	case DAO_COMPLEX :
		K = ct->core->CheckSetField( ct, field, at, self->routine );
		if( K ) goto InvalidField;
		if( at->realnum ){
			if( at->tid != DAO_FLOAT )
				DaoInferencer_InsertMove( self, inode, & inode->a, at, VMS->typeFloat );
			inode->code = DVM_SETF_CX;
			inode->b = strcmp( name->chars, "imag" ) == 0;
		}
		break;
	case DAO_CLASS :
	case DAO_OBJECT :
		K = ct->core->CheckSetField( ct, field, at, self->routine );
		if( K ) goto InvalidField;

		ck = ct->tid ==DAO_CLASS;
		klass = (DaoClass*) ct->aux;
		data = DaoClass_GetData( klass, name, hostClass );
		if( data == NULL || data->type == DAO_NONE ) break;

		if( data->xVar.dtype && data->xVar.dtype->invar ){
			if( !(routine->attribs & DAO_ROUT_INITOR) ) goto ModifyConstant;
		}
		if( data->xVar.dtype == NULL && data->xVar.dtype->tid == DAO_UDT ){
			GC_Assign( & data->xVar.dtype, at );
		}
		AssertTypeMatching( types[opa], data->xVar.dtype, defs );
		j = DaoClass_GetDataIndex( klass, name );

		k = LOOKUP_ST( j );
		type = data->xVar.dtype;
		if( data->xVar.dtype && data->xVar.dtype->realnum && at->realnum ){
			vmc->code = ck ? DVM_SETF_KGBB : DVM_SETF_OGBB;
			if( k == DAO_OBJECT_VARIABLE ) vmc->code = DVM_SETF_OVBB;
			if( at->tid != type->tid )
				DaoInferencer_InsertMove( self, inode, & inode->a, at, type );
			vmc->code += type->tid - DAO_BOOLEAN;
			vmc->b = LOOKUP_ID( j );
		}else if( type && type->tid == DAO_COMPLEX && at->tid && at->tid <= DAO_COMPLEX ){
			vmc->b = LOOKUP_ID( j );
			vmc->code = ck ? DVM_SETF_KGCC : DVM_SETF_OGCC;
			if( k == DAO_OBJECT_VARIABLE ) vmc->code = DVM_SETF_OVCC;
			if( at->tid != type->tid )
				DaoInferencer_InsertMove( self, inode, & inode->a, at, type );
		}else if( at == type || type->tid == DAO_ANY ){
			vmc->b = LOOKUP_ID( j );
			vmc->code = ck ? DVM_SETF_KG : DVM_SETF_OG;
			if( k == DAO_OBJECT_VARIABLE ) vmc->code = DVM_SETF_OV;
		}
		break;
	case DAO_TUPLE :
		K = ct->core->CheckSetField( ct, field, at, self->routine );
		if( K ) goto InvalidField;

		node = MAP_Find( ct->mapNames, name );
		k = node->value.pInt;
		if( k <0 || k >= (int)ct->args->size ) goto InvalidField;

		ct = ct->args->items.pType[ k ];
		if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
		if( ct && ct->invar ) goto ModifyConstant;

		AssertTypeMatching( at, ct, defs );
		if( k < 0xffff ){
			if( ct->tid && ct->tid <= DAO_COMPLEX && at->tid && at->tid <= DAO_COMPLEX ){
				if( at->tid != ct->tid )
					DaoInferencer_InsertMove( self, inode, & inode->a, at, ct );
				vmc->code = DVM_SETF_TBB + ct->tid - DAO_BOOLEAN;
				vmc->b = k;
			}else if( at->tid == DAO_STRING && ct->tid == DAO_STRING ){
				vmc->code = DVM_SETF_TSS;
				vmc->b = k;
			}else if( at == ct || ct->tid == DAO_ANY ){
				vmc->b = k;
				if( at->tid >= DAO_ARRAY && at->tid <= DAO_TYPE && consts[opa] == NULL ){
					vmc->code = DVM_SETF_TPP;
				}else{
					vmc->code = DVM_SETF_TXX;
				}
			}
		}
		break;
	case DAO_NAMESPACE :
		{
			if( consts[opc] && consts[opc]->type == DAO_NAMESPACE ){
				DaoNamespace *ans = & consts[opc]->xNamespace;
				k = DaoNamespace_FindVariable( ans, name );
				if( k >=0 ){
					ct = DaoNamespace_GetVariableType( ans, k );
				}else{
					k = DaoNamespace_FindConst( ans, name );
					value = DaoNamespace_GetConst( ans, k );
					if( value ) ct = DaoNamespace_GetType( ans, value );
				}
				if( k <0 ) goto NotExist;
				if( ct && ct->invar ) goto ModifyConstant;
				AssertTypeMatching( at, ct, defs );
			}
			break;
		}
	default:
		if( ct->core == NULL || ct->core->CheckSetField == NULL ) goto InvalidField;
		K = ct->core->CheckSetField( ct, field, at, self->routine );
		if( K ) goto InvalidField;
		break;
	}
	return 1;
NotMatch : return DaoInferencer_ErrorTypeNotMatching( self, NULL, NULL );
NotPermit : return DaoInferencer_Error( self, DTE_FIELD_NOT_PERMIT );
NotExist : return DaoInferencer_Error( self, DTE_FIELD_NOT_EXIST );
NeedInstVar : return DaoInferencer_Error( self, DTE_FIELD_OF_INSTANCE );
InvalidOper : return DaoInferencer_Error( self, DTE_OPERATION_NOT_VALID );
InvalidField : return DaoInferencer_Error( self, DTE_INDEX_NOT_VALID ); // TODO;
ModifyConstant: return DaoInferencer_Error( self, DTE_CONST_WRONG_MODIFYING );
}
DaoType* DaoInferencer_CheckBinaryOper( DaoInferencer *self, DaoInode *inode, DaoType *at, DaoType *bt )
{
	DString *mbs = self->mbstring;
	DaoType **types = self->types->items.pType;
	DaoNamespace *NS = self->routine->nameSpace;
	DaoVmSpace *VMS = self->routine->nameSpace->vmSpace;
	DaoClass *hostClass = self->hostClass;
	DaoVmCodeX *vmc = (DaoVmCodeX*) inode;
	DaoType *operands[2];
	DaoType *ct = NULL;
	int code = inode->code;
	int opc = inode->c;

	operands[0] = at;
	operands[1] = bt;
	if( DaoType_LooseChecking( at ) && DaoType_LooseChecking( bt ) ){
		ct = at->tid < bt->tid ? at : bt;
	}else if( DaoType_LooseChecking( at ) ){
		ct = at;
	}else if( DaoType_LooseChecking( bt ) ){
		ct = bt;
	}else if( at->tid == DAO_VARIANT || bt->tid == DAO_VARIANT ){
		ct = VMS->typeAny; // TODO;
	}else if(  ((at->tid >= DAO_OBJECT && at->tid <= DAO_CDATA) || at->tid == DAO_INTERFACE)
			&& ((bt->tid >= DAO_OBJECT && bt->tid <= DAO_CDATA) || bt->tid == DAO_INTERFACE) ){
		DaoType *operhost = at;
		if( inode->c == inode->a ){
			operhost = at;
		}else if( inode->c == inode->b ){
			operhost = bt;
		}else if( DaoType_ChildOf( bt, at ) ){
			operhost = bt;
		}
		if( operhost->core == NULL || operhost->core->CheckBinary == NULL ) return NULL;
		ct = operhost->core->CheckBinary( operhost, (DaoVmCode*)vmc, operands, self->routine );
	}else if( (at->tid >= DAO_OBJECT && at->tid <= DAO_CDATA) || at->tid == DAO_INTERFACE ){
		if( at->core == NULL || at->core->CheckBinary == NULL ) return NULL;
		ct = at->core->CheckBinary( at, (DaoVmCode*)vmc, operands, self->routine );
	}else if( (bt->tid >= DAO_OBJECT && bt->tid <= DAO_CDATA) || bt->tid == DAO_INTERFACE ){
		if( bt->core == NULL || bt->core->CheckBinary == NULL ) return NULL;
		ct = bt->core->CheckBinary( bt, (DaoVmCode*)vmc, operands, self->routine );
	}else if( at->tid >= bt->tid ){
		if( at->core == NULL || at->core->CheckBinary == NULL ) return NULL;
		ct = at->core->CheckBinary( at, (DaoVmCode*)vmc, operands, self->routine );
	}else{
		if( bt->core == NULL || bt->core->CheckBinary == NULL ) return NULL;
		ct = bt->core->CheckBinary( bt, (DaoVmCode*)vmc, operands, self->routine );
	}
	return ct;
}
int DaoInferencer_HandleBinaryArith( DaoInferencer *self, DaoInode *inode, DMap *defs )
{
	int code = inode->code;
	int opa = inode->a;
	int opb = inode->b;
	int opc = inode->c;
	DaoVmSpace *VMS = self->routine->nameSpace->vmSpace;
	DaoType **types = self->types->items.pType;
	DaoVmCodeX *vmc = (DaoVmCodeX*) inode;
	DaoType *at = types[opa];
	DaoType *bt = types[opb];
	DaoType *ct = types[opc];

#if 0
	if( types[opa] ) printf( "a: %s\n", types[opa]->name->chars );
	if( types[opb] ) printf( "b: %s\n", types[opb]->name->chars );
	if( types[opc] ) printf( "c: %s\n", types[opc]->name->chars );
#endif

	ct = DaoInferencer_CheckBinaryOper( self, inode, at, bt );
	if( ct == NULL ) goto InvalidOper;

	DaoInferencer_UpdateVarType( self, opc, ct );
	/* allow less strict typing: */
	if( ct->tid == DAO_UDT || ct->tid == DAO_ANY ) return 1;
	AssertTypeMatching( ct, types[opc], defs );
	ct = types[opc];
	if( at->realnum && bt->realnum && ct->realnum ){
		DaoType *max = ct;
		if( at->tid > max->tid ) max = at;
		if( bt->tid > max->tid ) max = bt;
		if( max->tid == DAO_BOOLEAN ) max = VMS->typeInt; /* such oper on bools produce int; */
		if( at->tid != max->tid ){
			DaoInferencer_InsertMove( self, inode, & inode->a, at, max );
			if( opa == opb ) inode->b = inode->a;
		}
		if( opa != opb && bt->tid != max->tid )
			DaoInferencer_InsertMove( self, inode, & inode->b, bt, max );

		switch( max->tid ){
		case DAO_INTEGER : vmc->code += DVM_ADD_III - DVM_ADD; break;
		case DAO_FLOAT  : vmc->code += DVM_ADD_FFF - DVM_ADD; break;
		}
		if( max->tid != ct->tid ) DaoInferencer_InsertMove2( self, inode, max, ct );
	}else if( ct->tid == DAO_COMPLEX && code <= DVM_DIV ){
		if( at->tid && at->tid <= DAO_COMPLEX && bt->tid && bt->tid <= DAO_COMPLEX ){
			if( at->tid != DAO_COMPLEX ){
				DaoInferencer_InsertMove( self, inode, & inode->a, at, VMS->typeComplex );
				if( opa == opb ) inode->b = inode->a;
			}
			if( opa != opb && bt->tid != DAO_COMPLEX )
				DaoInferencer_InsertMove( self, inode, & inode->b, bt, VMS->typeComplex );
			vmc->code += DVM_ADD_CCC - DVM_ADD;
		}
	}else if( at->tid == bt->tid && ct->tid == at->tid ){
		if( ct->tid == DAO_STRING && code == DVM_ADD ) vmc->code = DVM_ADD_SSS;
	}
	return 1;
InvalidOper :
	return DaoInferencer_ErrorTypeNotConsistent( self, at, bt );
}
int DaoInferencer_HandleBinaryBool( DaoInferencer *self, DaoInode *inode, DMap *defs )
{
	int code = inode->code;
	int opa = inode->a;
	int opb = inode->b;
	int opc = inode->c;
	DaoVmSpace *VMS = self->routine->nameSpace->vmSpace;
	DaoType **types = self->types->items.pType;
	DaoVmCodeX *vmc = (DaoVmCodeX*) inode;
	DaoType *at = types[opa];
	DaoType *bt = types[opb];
	DaoType *ct = types[opc];

#if 0
	if( types[opa] ) printf( "a: %s\n", types[opa]->name->chars );
	if( types[opb] ) printf( "b: %s\n", types[opb]->name->chars );
	if( types[opc] ) printf( "c: %s\n", types[opc]->name->chars );
#endif

	ct = DaoInferencer_CheckBinaryOper( self, inode, at, bt );
	if( ct == NULL ) goto InvalidOper;

	DaoInferencer_UpdateVarType( self, opc, ct );
	/* allow less strict typing: */
	if( ct->tid == DAO_UDT || ct->tid == DAO_ANY ) return 1;
	AssertTypeMatching( ct, types[opc], defs );
	ct = types[opc];
	if( at->realnum && bt->realnum && ct->realnum ){
		DaoType *max = at->tid == bt->tid ? at : VMS->typeBool;
		if( at->tid != max->tid ){
			DaoInferencer_InsertMove( self, inode, & inode->a, at, max );
			if( opa == opb ) inode->b = inode->a;
		}
		if( opa != opb && bt->tid != max->tid )
			DaoInferencer_InsertMove( self, inode, & inode->b, bt, max );

		switch( max->tid ){
		case DAO_BOOLEAN : vmc->code += DVM_AND_BBB - DVM_AND; break;
		case DAO_INTEGER : vmc->code += DVM_AND_BII - DVM_AND; break;
		case DAO_FLOAT   : vmc->code += DVM_AND_BFF - DVM_AND; break;
		}
		if( ct->tid != DAO_BOOLEAN ) DaoInferencer_InsertMove2( self, inode, VMS->typeBool, ct );
	}
	return 1;
InvalidOper : return DaoInferencer_Error( self, DTE_OPERATION_NOT_VALID );
}
int DaoInferencer_HandleListArrayEnum( DaoInferencer *self, DaoInode *inode, DMap *defs )
{
	int code = inode->code;
	int opa = inode->a;
	int opb = inode->b & (0xffff>>2);
	int opc = inode->c;
	int mode = inode->b >> 14;
	DaoType **types = self->types->items.pType;
	DaoNamespace *NS = self->routine->nameSpace;
	DaoVmSpace *VMS = self->routine->nameSpace->vmSpace;
	DaoVmCodeX *vmc = (DaoVmCodeX*) inode;
	DaoType *at = types[opa];
	DaoType *ct = types[opc];
	int j;

	int tid = code == DVM_LIST ? DAO_LIST : DAO_ARRAY;
	if( types[opc] && types[opc]->tid == tid ){
		if( types[opc]->args && types[opc]->args->size == 1 ){
			DaoType *it = types[opc]->args->items.pType[0];
			if( code == DVM_VECTOR && mode == DVM_ENUM_MODE1 ){
				int m1 = DaoType_MatchTo( types[opa], types[opc], defs );
				int m2 = DaoType_MatchTo( types[opa], it, defs );
				if( m1 == 0 && m2 == 0 ){
					return DaoInferencer_ErrorTypeNotMatching( self, types[opa], it );
				}
				if( opb == 3 ){
					int m1 = DaoType_MatchTo( types[opa+1], types[opc], defs );
					int m2 = DaoType_MatchTo( types[opa+1], it, defs );
					if( m1 == 0 && m2 == 0 ){
						return DaoInferencer_ErrorTypeNotMatching( self, types[opa+1], it );
					}
				}
				AssertTypeMatching( types[opa+opb-1], VMS->typeInt, defs );
			}else if( code == DVM_LIST && mode == DVM_ENUM_MODE1 ){
				AssertTypeMatching( types[opa], it, defs );
				if( opb == 3 ) AssertTypeMatching( types[opa+1], it, defs );
				AssertTypeMatching( types[opa+opb-1], VMS->typeInt, defs );
			}else if( code == DVM_VECTOR ){
				int m1 = DaoType_MatchTo( types[opa], types[opc], defs );
				int m2 = DaoType_MatchTo( types[opa], it, defs );
				if( m1 == 0 && m2 == 0 ){
					return DaoInferencer_ErrorTypeNotMatching( self, types[opa], it );
				}
				if( m1 ) it = types[opc];
				for(j=0; j<opb; ++j) AssertTypeMatching( types[opa+j], it, defs );
			}else{
				for(j=0; j<opb; ++j) AssertTypeMatching( types[opa+j], it, defs );
			}
			return 1;
		}
	}
	at = VMS->typeUdf;
	if( code == DVM_VECTOR && mode == DVM_ENUM_MODE0 && opb ){
		at = types[opa];
		for(j=1; j<opb; j++) AssertTypeMatching( types[opa+j], at, defs );
		if( at->tid == DAO_ARRAY ) at = at->args->items.pType[0];
		if( at->tid == DAO_NONE || at->tid > DAO_COMPLEX ) at = VMS->typeFloat;
	}else if( code == DVM_LIST && mode == DVM_ENUM_MODE0 && opb ){
		at = types[opa];
		for(j=1; j<opb; j++){
			if( DaoType_MatchTo( types[opa+j], at, defs )==0 ){
				at = VMS->typeAny;
				break;
			}
			if( at->tid < types[opa+j]->tid ) at = types[opa+j];
		}
	}else if( mode == DVM_ENUM_MODE1 ){
		int num = types[opa+1+(opb==3)]->tid;
		int init = types[opa]->tid;
		at = types[opa];
		if( num == 0 || (num > DAO_FLOAT && (num & DAO_ANY) == 0) ) goto ErrorTyping;
		if( opb == 3 && (init & DAO_ANY) == 0 && (types[opa+1]->tid & DAO_ANY) == 0 ){
			int step = types[opa+1]->tid;
			if( step == 0 ) goto ErrorTyping;
			if( types[opa]->realnum ){
				if( types[opa+1]->realnum == 0 ) goto ErrorTyping;
			}else if( init == DAO_COMPLEX ){
				if( step > DAO_COMPLEX ) goto ErrorTyping;
			}else if( init == DAO_STRING && code == DVM_LIST ){
				if( step != DAO_STRING ) goto ErrorTyping;
			}else{
				goto ErrorTyping;
			}
		}
	}else if( opb == 0 && types[opc] != NULL ){
		if( types[opc]->tid == DAO_LIST ){
			if( code == DVM_LIST ) return 1;
		}else if( types[opc]->tid == DAO_ARRAY ){
			if( code == DVM_VECTOR ) return 1;
		}
	}
	if( opb == 0 ){
		if( code == DVM_LIST ){
			ct = VMS->typeListEmpty;
		}else{
			ct = VMS->typeArrayEmpty;
		}
	}else if( code == DVM_LIST ){
		at = DaoType_GetBaseType( at );
		ct = DaoType_Specialize( VMS->typeList, & at, at != NULL, NS );
	}else if( at && at->tid >= DAO_BOOLEAN && at->tid <= DAO_COMPLEX ){
		at = DaoType_GetBaseType( at );
		ct = DaoType_Specialize( VMS->typeArray, & at, 1, NS );
	}else if( DaoType_LooseChecking( at ) ){
		ct = VMS->typeArrayEmpty; /* specially handled for copying; */
	}else{
		goto ErrorTyping;
	}
	if( ct == NULL ){
		return DaoInferencer_Error( self, DTE_INVALID_ENUMERATION );
	}
	DaoInferencer_UpdateType( self, opc, ct );
	AssertTypeMatching( ct, types[opc], defs );
	return 1;
ErrorTyping: return DaoInferencer_Error( self, DTE_TYPE_NOT_MATCHING );
}
int DaoInferencer_HandleSwitch( DaoInferencer *self, DaoInode *inode, int i, DMap *defs )
{
	int opa = inode->a;
	int opc = inode->c;
	DList  *routConsts = self->routine->routConsts->value;
	DaoInode *inode2, **inodes = self->inodes->items.pInode;
	DaoType *bt, *type, **types = self->types->items.pType;
	DaoValue **consts = self->consts->items.pValue;
	DaoNamespace *NS = self->routine->nameSpace;
	DaoVmCodeX *vmc = (DaoVmCodeX*) inode;
	DaoType *at = types[opa];
	int j, k;

	if( inodes[i+1]->c == DAO_CASE_TYPES ){
		for(k=1; k<=opc; k++){
			DaoType *tt = (DaoType*) routConsts->items.pValue[ inodes[i+k]->a ];
			if( tt->type != DAO_TYPE ) return DaoInferencer_Error( self, DTE_INVALID_TYPE_CASE );
		}
		return 1;
	}

	j = 0;
	for(k=1; k<=opc; k++){
		DaoValue *cc = routConsts->items.pValue[ inodes[i+k]->a ];
		j += (cc && cc->type == DAO_ENUM && cc->xEnum.subtype == DAO_ENUM_SYM);
		bt = DaoNamespace_GetType( NS, cc );
		if( at->tid == DAO_ENUM && bt->tid == DAO_ENUM ){
			if( at->subtid == DAO_ENUM_SYM && bt->subtid == DAO_ENUM_SYM ) continue;
		}
		if( DaoType_MatchValue( at, cc, defs ) ==0 ){
			self->currentIndex = i + k;
			type = DaoNamespace_GetType( NS, cc );
			return DaoInferencer_ErrorTypeNotMatching( self, type, at );
		}
	}
	if( consts[opa] && consts[opa]->type ){
		DaoValue *sv = consts[opa];
		for(k=1; k<=opc; k++){
			DaoValue *cc = routConsts->items.pValue[ inodes[i+k]->a ];
			if( DaoValue_Compare( sv, cc ) ==0 ){
				inode->code = DVM_GOTO;
				inode->jumpFalse = inodes[i+k];
				inodes[i+k]->code = DVM_GOTO;  /* Change DVM_CASE to DVM_GOTO; */
				break;
			}
		}
	}else if( at->tid == DAO_ENUM && at->subtid != DAO_ENUM_SYM && j == opc ){
		DaoInode *front = inodes[i];
		DaoInode *back = inodes[i+opc+1];
		DaoEnum denum = {DAO_ENUM,DAO_ENUM_SYM,0,0,0,0,0,NULL};
		DMap *jumps = DMap_New( DAO_DATA_VALUE, 0 );
		DNode *it, *find;
		dao_integer max = 0, min = 0;

		denum.etype = at;
		denum.subtype = at->subtid;
		for(k=1; k<=opc; k++){
			DaoValue *cc = routConsts->items.pValue[ inodes[i+k]->a ];
			if( DaoEnum_SetValue( & denum, & cc->xEnum ) == 0 ){
				self->currentIndex = i + k;
				DMap_Delete( jumps );
				return DaoInferencer_ErrorTypeNotMatching( self, cc->xEnum.etype, at );
			}
			if( k ==1 ){
				max = min = denum.value;
			}else{
				if( denum.value > max ) max = denum.value;
				if( denum.value < min ) min = denum.value;
			}
			MAP_Insert( jumps, (DaoValue*) & denum, inodes[i+k] );
		}
		if( at->subtid != DAO_ENUM_FLAG && opc > 0.75*(max-min+1) ){
			for(it=DMap_First(at->mapNames); it; it=DMap_Next(at->mapNames,it)){
				if( it->value.pInt < min || it->value.pInt > max ) continue;
				denum.value = it->value.pInt;
				find = DMap_Find( jumps, (DaoValue*) & denum );
				if( find == NULL ){
					inode2 = DaoInferencer_InsertNode( self, inodes[i+1], DVM_CASE, 0, 0 );
					inode2->jumpFalse = inode->jumpFalse;
					inode2->a = routConsts->size;
					inode2->c = DAO_CASE_TABLE;
					inodes[i+1]->extra = NULL;
					DMap_Insert( jumps, (DaoValue*) & denum, inode2 );
				}else{
					find->value.pInode->a = routConsts->size;
					find->value.pInode->c = DAO_CASE_TABLE;
				}
				DaoRoutine_AddConstant( self->routine, (DaoValue*) & denum );
			}
			vmc->c = jumps->size;
		}
		front->next = back;
		back->prev = front;
		for(it=DMap_First(jumps); it; it=DMap_Next(jumps,it)){
			inode2 = it->value.pInode;
			front->next = inode2;
			back->prev = inode2;
			inode2->prev = front;
			inode2->next = back;
			front = inode2;
		}
		DMap_Delete( jumps );
	}else if( j ){
		inodes[i + 1]->c = DAO_CASE_UNORDERED;
	}
	return 1;
}
static DaoRoutine* DaoInferencer_Specialize( DaoInferencer *self, DaoRoutine *rout, DMap *defs2, DaoInode *inode )
{
	DaoNamespace *NS = self->routine->nameSpace;
	DaoRoutine *orig = rout, *rout2 = rout;
	DMap *defs3 = self->defs3;

	if( rout->original ) rout = orig = rout->original;

	/* Do not specialize if the routine is not compiled yet! */
	if( rout->body == NULL ) return rout;
	if( rout->body->vmCodes->size == 0 ) return rout;

	/* Do not share function body. It may be thread unsafe to share: */
	rout = DaoRoutine_Copy( rout, 0, 1, 0 );
	DaoRoutine_Finalize( rout, orig, NULL, NS, defs2 );

	if( rout->routType == orig->routType || rout->routType == rout2->routType ){
		DaoGC_TryDelete( (DaoValue*) rout );
		rout = rout2;
	}else{
		DMutex_Lock( & mutex_routine_specialize );
		if( orig->specialized == NULL ) orig->specialized = DRoutines_New();
		DMutex_Unlock( & mutex_routine_specialize );

		GC_Assign( & rout->original, orig );
		/*
		// Need to add before specializing the body,
		// to avoid possible infinite recursion:
		 */
		DRoutines_Add( orig->specialized, rout );
		inode->b &= ~DAO_CALL_FAST;

		/* rout may has only been declared */
		/* forward declared routine may have an empty routine body: */
		if( rout->body && rout->body->vmCodes->size ){
			DaoRoutine *rout3 = rout;
			/* Create a new copy of the routine for specialization: */
			rout = DaoRoutine_Copy( rout, 0, 1, 0 );
			GC_Assign( & rout->original, orig );
			DMap_Reset( defs3 );
			DaoType_MatchTo( rout->routType, orig->routType, defs3 );
			DaoRoutine_MapTypes( rout, rout3, defs3 );

			/* to infer returned type */
			if( DaoRoutine_DoTypeInference( rout, self->silent ) ==0 ){
				DaoGC_TryDelete( (DaoValue*) rout );
				return NULL;
			}
			/* Replace the previous unspecialized copy with this specialized copy: */
			DRoutines_Add( orig->specialized, rout );
		}
	}
	return rout;
}
int DaoInferencer_HandleCall( DaoInferencer *self, DaoInode *inode, int i, DMap *defs )
{
	int code = inode->code;
	int opa = inode->a;
	int opc = inode->c;
	DMap *defs2 = self->defs2;
	DMap *defs3 = self->defs3;
	DList *errors = self->errors;
	DList *rettypes = self->rettypes;
	DaoInode **inodes = self->inodes->items.pInode;
	DaoType *bt, *tt, **tp, **types = self->types->items.pType;
	DaoValue **pp, **consts = self->consts->items.pValue;
	DaoNamespace *NS = self->routine->nameSpace;
	DaoVmSpace *VMS = self->routine->nameSpace->vmSpace;
	DaoClass *hostClass = self->hostClass;
	DaoVmCodeX *vmc = (DaoVmCodeX*) inode;
	DaoRoutine *routine = self->routine;
	DaoRoutine *rout, *rout2;
	DaoType *at = types[opa];
	DaoType *ct = types[opc];
	int callOper = 0;
	int N = self->inodes->size;
	int j, k, m, K;
	int checkfast = 0;
	int ctchecked = 0;
	int argc = vmc->b & 0xff;
	int codemode = code | ((int)vmc->b<<16);
	DaoType *cbtype = NULL;
	DaoInode *sect = NULL;

	if( (vmc->b & DAO_CALL_BLOCK) && inodes[i+2]->code == DVM_SECT ){
		sect = inodes[ i + 2 ];
		for(j=0, k=sect->a; j<sect->b; j++, k++){
			DaoInferencer_UpdateType( self, k, VMS->typeUdf );
		}
	}
	bt = ct = NULL;
	if( code == DVM_CALL && self->tidHost == DAO_OBJECT ) bt = hostClass->objType;

#if 0
	DaoVmCodeX_Print( *vmc, NULL, NULL );
	printf( "call: %s %i\n", types[opa]->name->chars, types[opa]->tid );
	if(bt) printf( "self: %s\n", bt->name->chars );
#endif

	pp = consts+opa+1;
	tp = types+opa+1;
	for(k=0; k<argc; k++){
		tt = DaoType_DefineTypes( tp[k], NS, defs );
		GC_Assign( & tp[k], tt );
	}
	if( ! (routine->attribs & DAO_ROUT_MAIN) ){
		m = 1; /* tail call; */
		for(k=i+1; k<N; k++){
			DaoInode *ret = inodes[k];
			if( ret->code == DVM_RETURN ){
				m &= ret->c ==0 && (ret->b ==0 || (ret->b ==1 && ret->a == vmc->c));
				break;
			}
			m = 0;
			break;
		}
		if( m ) vmc->b |= DAO_CALL_TAIL;
	}
	if( (vmc->b & DAO_CALL_EXPAR) && argc && tp[argc-1]->tid == DAO_TUPLE ){
		DList *its = tp[argc-1]->args;
		DList_Clear( self->types2 );
		for(k=0; k<(argc-1); k++) DList_Append( self->types2, tp[k] );
		for(k=0; k<its->size; k++){
			DaoType *it = its->items.pType[k];
			if( it->tid >= DAO_PAR_NAMED && it->tid <= DAO_PAR_VALIST ) it = (DaoType*)it->aux;
			DList_Append( self->types2, it );
		}
		tp = self->types2->items.pType;
		argc = self->types2->size;
	}

	ct = types[opa];
	rout = NULL;
	if( at->tid == DAO_CLASS ){
		if( at->aux == NULL ){  /* type "class": */
			ct = VMS->typeAny;
		}else{
			if( at->aux->xClass.initRoutines->overloads->routines->size ){
				rout = (DaoRoutine*) at->aux->xClass.initRoutines; /* XXX */
			}else{
				rout = at->aux->xClass.initRoutine;
			}
			ct = at->aux->xClass.objType;
		}
	}else if( at->tid == DAO_CTYPE ){
		rout = DaoType_GetInitor( at );
		if( rout == NULL ) goto NotCallable;
	}else if( consts[opa] && consts[opa]->type == DAO_ROUTINE ){
		rout = (DaoRoutine*) consts[opa];
	}else if( at->tid == DAO_THT ){
		DaoInferencer_UpdateType( self, opc, VMS->typeAny );
		AssertTypeMatching( VMS->typeAny, types[opc], defs );
		goto TryPushBlockReturnType;
	}else if( at->tid == DAO_UDT || at->tid == DAO_ANY ){
		DaoInferencer_UpdateType( self, opc, VMS->typeAny );
		goto TryPushBlockReturnType;
	}else if( at->tid == DAO_OBJECT ){
		rout = DaoClass_FindMethod( & at->aux->xClass, "()", hostClass );
		if( rout == NULL ) goto NotCallable;
		callOper = 1;
		bt = at;
	}else if( at->tid >= DAO_CSTRUCT && at->tid <= DAO_CTYPE ){
		rout = DaoType_FindFunctionChars( at, "()" );
		if( rout == NULL ) goto NotCallable;
		callOper = 1;
		bt = at;
	}else if( at->tid == DAO_INTERFACE ){
		DaoInterface *inter = (DaoInterface*) at->aux;
		DNode *it = DMap_Find( inter->methods, inter->abtype->name );
		if( it == NULL ) goto NotCallable;
		rout = it->value.pRoutine;
	}else if( at->tid == DAO_CINVALUE ){
		rout = DaoType_FindFunctionChars( at, "()" );
		if( rout == NULL ) goto NotCallable;
		callOper = 1;
		bt = at;
	}else if( at->tid == DAO_TYPE ){
		at = at->args->items.pType[0];
		rout = DaoType_FindFunction( at, at->name );
		if( rout == NULL ) goto NotCallable;
	}else if( at->tid != DAO_ROUTINE ){
		goto NotCallable;
	}
	if( at->tid == DAO_ROUTINE && at->subtid == DAO_ROUTINES ) rout = (DaoRoutine*)at->aux;
	if( rout == NULL && at->aux == NULL ){ /* "routine" type: */
		/* DAO_CALL_INIT: mandatory passing the implicit self parameter. */
		if( !(vmc->b & DAO_CALL_INIT) ) vmc->b |= DAO_CALL_NOSELF;
		cbtype = at->cbtype;
		ct = VMS->typeAny;
		ctchecked = 1;
	}else if( rout == NULL ){
		cbtype = at->cbtype;
		if( !(vmc->b & DAO_CALL_INIT) ) vmc->b |= DAO_CALL_NOSELF;
		if( DaoRoutine_CheckType( at, NS, NULL, tp, argc, codemode, 0 ) ==0 ){
			DaoRoutine_CheckError( NS, NULL, at, NULL, tp, argc, codemode, errors );
			goto InvalidParam;
		}
		DaoRoutine_CheckType( at, NS, NULL, tp, argc, codemode, 1 );
		ct = types[opa];
	}else{
		if( rout->type != DAO_ROUTINE ) goto NotCallable;
		rout2 = rout;
		/* rout can be DRoutines: */
		rout = DaoRoutine_Check( rout, bt, tp, argc, codemode, errors );
		if( rout == NULL ) goto InvalidParam;
		if( rout->attribs & DAO_ROUT_PRIVATE ){
			if( rout->routHost && rout->routHost != routine->routHost ) goto CallNotPermit;
			if( rout->routHost == NULL && rout->nameSpace != NS ) goto CallNotPermit;
		}else if( rout->attribs & DAO_ROUT_PROTECTED ){
			if( rout->routHost && routine->routHost == NULL ) goto CallNotPermit;
		}
		if( vmc->code == DVM_CALL && rout->routHost ){
			int staticCallee = rout->attribs & DAO_ROUT_STATIC;
			int invarCallee = rout->attribs & DAO_ROUT_INVAR;
			int initorCallee = rout->attribs & DAO_ROUT_INITOR;
			if( DaoType_ChildOf( routine->routHost, rout->routHost ) ){
				int invarCaller = routine->attribs & DAO_ROUT_INVAR;
				int staticCaller = routine->attribs & DAO_ROUT_STATIC;
				if( staticCaller && ! staticCallee && ! initorCallee && ! callOper ) goto CallWithoutInst;
				if( invarCaller && ! invarCallee && ! initorCallee ) goto CallNonInvar;
			}else{
				if( ! staticCallee && ! initorCallee && ! callOper ) goto CallWithoutInst;
			}
		}
		checkfast = DVM_CALL && ((vmc->b & 0xff00) & ~DAO_CALL_TAIL) == 0;
		checkfast &= at->tid == DAO_ROUTINE && argc >= rout2->parCount;
		checkfast &= rout2->routHost == NULL;
		checkfast &= rout2 == rout;
		checkfast &= rout2->body != NULL || rout2->pFunc != NULL; /* not curry; */
		checkfast &= (vmc->code == DVM_CALL) == !(rout2->routType->attrib & DAO_TYPE_SELF);
		vmc->b &= ~DAO_CALL_FAST;
		if( checkfast ){
			int fast = 1;
			for(k=0; fast && k<rout2->routType->args->size; ++k){
				DaoType *part = rout2->routType->args->items.pType[k];
				DaoType *argt = tp[k];
				if( part->tid >= DAO_PAR_NAMED && part->tid <= DAO_PAR_VALIST ){
					part = (DaoType*) part->aux;
					fast &= part->tid != DAO_PAR_NAMED;
				}
				if( part->tid == DAO_ANY ) continue;
				fast &= DaoType_MatchTo( argt, part, NULL ) >= DAO_MT_EQ;
			}
			if( fast ) vmc->b |= DAO_CALL_FAST;
		}

		if( rout2->overloads && rout2->overloads->routines->size > 1 ){
			DList *routines = rout2->overloads->routines;
			m = DaoRoutine_CheckType( rout->routType, NS, bt, tp, argc, codemode, 1 );
			if( m <= DAO_MT_ANY ){
				/* For situations like:
				//
				// routine test( x :int ){ io.writeln(1); return 1; }
				// routine test( x ){ io.writeln(2); return 'abc'; }
				// var a : any = 1;
				// b = test( a );
				//
				// The function call may be resolved at compiling time as test(x),
				// which returns a string. But at runnning time, the function call
				// will be resolved as test(x:int), which return an integer.
				// Such discrepancy need to be solved here:
				 */
				DList_Clear( self->array );
				for(k=0,K=routines->size; k<K; k++){
					DaoType *type = routines->items.pRoutine[k]->routType;
					m = DaoRoutine_CheckType( type, NS, bt, tp, argc, codemode, 1 );
					if( m == 0 ) continue;
					type = (DaoType*) type->aux;
					if( type == NULL ) type = VMS->typeNone;
					if( type && type->tid == DAO_ANY ){
						ctchecked = 1;
						ct = VMS->typeAny;
						break;
					}
					for(m=0,K=self->array->size; m<K; m++){
						if( self->array->items.pType[m] == type ) break;
					}
					if( m >= self->array->size ) DList_Append( self->array, type );
				}
				if( self->array->size > 1 ){
					ctchecked = 1;
					ct = VMS->typeAny; /* XXX return variant type? */
				}
			}
		}

		tt = rout->routType;
		cbtype = tt->cbtype;

		DMap_Reset( defs2 );
		if( at->tid == DAO_CTYPE && at->kernel->sptree ){
			/*
			// It is a call to the constructor of a specialized type,
			// do the matching to initialize the type holder mapping:
			*/
			DaoType_MatchTo( at, at->kernel->abtype->aux->xCtype.classType, defs2 );
		}

		/*
		// Specialization must be checked against the original routine,
		// in case that it has been specialized for compatible types.
		// For example:
		//   routine testing( a, b: string ){ io.writeln( std.about(a) ) }
		//   testing( 1, "a" )
		//   testing( 2.5, "b" )
		//
		// For the second call, the routine will be resolved to the routine
		// specialized for the first call with integer parameter. Checking
		// with the specialized routine cannot lead to new sepcialization.
		*/
		k = defs2->size;
		rout2 = rout->original ? rout->original : rout;
		DaoRoutine_PassParamTypes( rout2, bt, tp, argc, code, defs2 );

		if( defs2->size > k || rout->routType->aux->xType.tid == DAO_THT ){
			/*
			// When the original routine is specialized with the given parameters
			// here, the specialized signature need to be different from the signature
			// of the resolved routine, otherwise there is no need to specialize.
			*/
			DaoType *routype = DaoType_DefineTypes( rout2->routType, NS, defs2 );

			DMap_Reset( defs3 );
			if( DaoType_MatchTo( routype, rout->routType, defs3 ) < DAO_MT_EQ || defs3->size ){
				rout = DaoInferencer_Specialize( self, rout2, defs2, inode );
				if( rout == NULL ) goto InvalidParam;
			}
		}

		if( at->tid != DAO_CLASS && ! ctchecked ) ct = rout->routType;
		/*
		   printf( "ct2 = %s\n", ct ? ct->name->chars : "" );
		 */
	}
	k = routine->routType->attrib & ct->attrib;
	if( at->tid != DAO_CLASS && ! ctchecked ) ct = & ct->aux->xType;
	if( ct ) ct = DaoType_DefineTypes( ct, NS, defs2 );

	if( code == DVM_MCALL && tp[0]->tid == DAO_OBJECT
			&& (tp[0]->aux->xClass.attribs & DAO_CLS_ASYNCHRONOUS) ){
		ct = DaoType_Specialize( VMS->typeFuture, & ct, ct != NULL, NS );
	}else if( vmc->b & DAO_CALL_ASYNC ){
		ct = DaoType_Specialize( VMS->typeFuture, & ct, ct != NULL, NS );
	}
	if( types[opc] && types[opc]->tid == DAO_ANY ) goto TryPushBlockReturnType;
	if( ct == NULL ) ct = DaoNamespace_GetType( NS, dao_none_value );
	DaoInferencer_UpdateType( self, opc, ct );
	AssertTypeMatching( ct, types[opc], defs );

TryPushBlockReturnType:
	if( sect && cbtype && cbtype->args ){
		for(j=0, k=sect->a; j<sect->b; j++, k++){
			if( j < (int)cbtype->args->size ){
				tt = cbtype->args->items.pType[j];
				if( tt->tid == DAO_PAR_VALIST ){
					tt = (DaoType*) tt->aux;
					for(; j<sect->b; j++, k++){
						GC_DecRC( types[k] );
						types[k] = NULL;
						tt = DaoType_DefineTypes( tt, NS, defs2 );
						DaoInferencer_UpdateType( self, k, tt );
					}
					break;
				}
			}else{
				if( j < sect->c ) goto CallInvalidSectParam;
				break;
			}
			if( tt->attrib & DAO_TYPE_PARNAMED ) tt = (DaoType*)tt->aux;
			GC_DecRC( types[k] );
			types[k] = NULL;
			tt = DaoType_DefineTypes( tt, NS, defs2 );
			DaoInferencer_UpdateType( self, k, tt );
		}
		tt = DaoType_DefineTypes( (DaoType*)cbtype->aux, NS, defs2 );
		DList_Append( rettypes, inodes[i+1]->jumpFalse );
		DList_Append( rettypes, inode ); /* type at "opc" to be redefined; */
		DList_Append( rettypes, tt );
		DList_Append( rettypes, tt );
		DList_PushBack( self->typeMaps, defs2 );
	}else if( sect && cbtype == NULL ){
		if( DaoType_LooseChecking( types[opc] ) == 0 ) goto CallInvalidSection;
		DList_Append( rettypes, inodes[i+1]->jumpFalse );
		DList_Append( rettypes, inode );
		DList_Append( rettypes, NULL );
		DList_Append( rettypes, NULL );
		DList_PushBack( self->typeMaps, defs2 );
	}
	return 1;
NotCallable: return DaoInferencer_Error( self, DTE_NOT_CALLABLE );
InvalidParam : return DaoInferencer_Error( self, DTE_PARAM_ERROR );
CallNonInvar : return DaoInferencer_Error( self, DTE_CALL_NON_INVAR );
CallNotPermit : return DaoInferencer_Error( self, DTE_CALL_NOT_PERMIT );
CallWithoutInst : return DaoInferencer_Error( self, DTE_CALL_WITHOUT_INSTANCE );
CallInvalidSectParam : return DaoInferencer_Error( self, DTE_CALL_INVALID_SECTPARAM );
CallInvalidSection: return DaoInferencer_Error( self, DTE_CALL_INVALID_SECTION );
ErrorTyping: return DaoInferencer_Error( self, DTE_TYPE_NOT_MATCHING );
}
int DaoInferencer_HandleClosure( DaoInferencer *self, DaoInode *inode, int i, DMap *defs )
{
	int j;
	int code = inode->code;
	int opa = inode->a;
	int opc = inode->c;
	DMap *defs2 = self->defs2;
	DList *rettypes = self->rettypes;
	DaoType **types = self->types->items.pType;
	DaoValue **consts = self->consts->items.pValue;
	DaoInode *inode2, **inodes = self->inodes->items.pInode;
	DaoNamespace *NS = self->routine->nameSpace;
	DaoVmCodeX *vmc = (DaoVmCodeX*) inode;
	DaoRoutine *routine = self->routine;
	DaoRoutine *closure = (DaoRoutine*) consts[opa];
	DaoType *at = types[opa];
	DaoType *ct;

	if( types[opa]->tid != DAO_ROUTINE ) goto ErrorTyping;
	if( closure->attribs & DAO_ROUT_DEFER_RET ){
		if( self->rettypes->size == 4 ){
			DList_Append( self->defers, closure );
		}
	}

	for(j=0; j<vmc->b; j+=2){
		DaoInode *idata = inodes[i - vmc->b + j + 1];
		DaoType *uptype = types[opa+1+j];
		DaoVariable *var = closure->variables->items.pVar[ idata->b ];
		if( uptype->invar ) uptype = DaoType_GetBaseType( uptype );
		GC_Assign( & var->dtype, uptype );
	}
	if( DaoRoutine_DoTypeInference( closure, self->silent ) == 0 ) goto ErrorTyping;

	DaoInferencer_UpdateType( self, opc, at );
	AssertTypeMatching( at, types[opc], defs );
	return 1;
ErrorTyping: return DaoInferencer_Error( self, DTE_TYPE_NOT_MATCHING );
}
int DaoInferencer_HandleYieldReturn( DaoInferencer *self, DaoInode *inode, DMap *defs )
{
	int code = inode->code;
	int opa = inode->a;
	int opc = inode->c;
	DMap *defs2 = self->defs2;
	DList *rettypes = self->rettypes;
	DaoType *tt, **types = self->types->items.pType;
	DaoNamespace *NS = self->routine->nameSpace;
	DaoVmSpace *VMS = self->routine->nameSpace->vmSpace;
	DaoVmCodeX *vmc = (DaoVmCodeX*) inode;
	DaoRoutine *routine = self->routine;
	DaoType *at, *ct;
	DaoInode *redef;
	DaoType *ct2;

	ct = rettypes->items.pType[ rettypes->size - 1 ];
	ct2 = rettypes->items.pType[ rettypes->size - 2 ];
	redef = rettypes->items.pInode[ rettypes->size - 3 ];
	DMap_Reset( defs2 );
	DMap_Assign( defs2, defs );

	/*
	// DO NOT CHANGE
	// FROM: return (e1, e2, e3, ... )
	// TO:   return e1, e2, e3, ...
	//
	// Because they bear different semantic meaning.
	// For example, if "e1" is in the form of "name=>expression",
	// the name is not stored in the tuple value but in the tuple type for
	// the first. For the second, it should be part of the returned value.
	//
	// The following code should NOT be used!
	 */
#if 0
	if( i && inodes[i-1]->code == DVM_TUPLE && inodes[i-1]->c == vmc->a && vmc->b == 1 ){
		vmc->a = inodes[i-1]->a;
		vmc->b = inodes[i-1]->b;
		inodes[i-1]->code = DVM_UNUSED;
		opa = vmc->a;
		opb = vmc->b;
		opc = vmc->c;
	}
#endif


#if 0
	printf( "%p %i %s %s\n", self, routine->routType->args->size, routine->routType->name->chars, ct?ct->name->chars:"" );
#endif
	if( code == DVM_YIELD ){ /* yield in functional method: */
		DaoType **partypes, **argtypes;
		int parcount = 0, argcount = 0;
		int k, opb = vmc->b & 0xff;
		tt = NULL;
		if( routine->routType->cbtype ){
			tt = routine->routType->cbtype;
		}else{
			goto InvalidYield;
		}
		partypes = tt->args->items.pType;
		parcount = tt->args->size;
		if( vmc->b == 0 ){
			if( tt->args->size && partypes[0]->tid != DAO_PAR_VALIST ) goto ErrorTyping;
			ct = (DaoType*) tt->aux;
			if( ct == NULL ) ct = VMS->typeNone;
			DaoInferencer_UpdateType( self, opc, ct );
			return 1;
		}
		argtypes = types + opa;
		argcount = opb;
		if( (vmc->b & DAO_CALL_EXPAR) && opb && argtypes[opb-1]->tid == DAO_TUPLE ){
			DList *its = argtypes[opb-1]->args;
			DList_Clear( self->types2 );
			for(k=0; k<(opb-1); k++) DList_Append( self->types2, argtypes[k] );
			for(k=0; k<its->size; k++){
				DaoType *it = its->items.pType[k];
				if( it->tid >= DAO_PAR_NAMED && it->tid <= DAO_PAR_VALIST ){
					it = (DaoType*)it->aux;
				}
				DList_Append( self->types2, it );
			}
			argtypes = self->types2->items.pType;
			argcount = self->types2->size;
		}
		at = ct = DaoNamespace_MakeType( NS, "tuple<>", DAO_TUPLE, NULL, NULL, 0 );
		if( argcount ){
			at = DaoNamespace_MakeType2( NS, "tuple", DAO_TUPLE, NULL, argtypes, argcount );
		}
		if( parcount ){
			ct = DaoNamespace_MakeType2( NS, "tuple", DAO_TUPLE, NULL, partypes, parcount );
		}
#if 0
		printf( "%s %s\n", at->name->chars, ct->name->chars );
#endif
		AssertTypeMatching( at, ct, defs2 );
		ct = (DaoType*) tt->aux;
		if( ct == NULL ) ct = VMS->typeNone;
		if( ct ){
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs2 );
		}
		return 1;
	}
	if( vmc->b ==0 ){
		/* less strict checking for type holder as well (case mt.start()): */
		if( ct && (ct->tid == DAO_UDT || ct->tid == DAO_THT) ){
			ct = DaoNamespace_MakeValueType( NS, dao_none_value );
			rettypes->items.pType[ rettypes->size - 1 ] = ct;
			if( rettypes->size == 4 ){
				ct = DaoNamespace_MakeRoutType( NS, routine->routType, NULL, NULL, ct );
				GC_Assign( & routine->routType, ct );
			}
			return 1;
		}
		if( ct && DaoType_MatchValue( ct, dao_none_value, NULL ) ) return 1;
		if( ct && ! (routine->attribs & DAO_ROUT_INITOR) ) goto MissingReturn;
	}else{
		if( code == DVM_RETURN && (routine->attribs & DAO_ROUT_INITOR) ){
			/* goto InvalidReturn; */  /* TODO: not for decorated initor; */
		}else if( code == DVM_RETURN && (routine->attribs & DAO_ROUT_DEFER) ){
			if( !(routine->attribs & DAO_ROUT_DEFER_RET) ) goto InvalidReturn;
		}
		at = types[opa];
		if( at == NULL ) goto ErrorTyping;
		if( vmc->b >1 )
			at = DaoNamespace_MakeType2( NS, "tuple", DAO_TUPLE, NULL, types+opa, vmc->b);

		if( ct ) AssertTypeMatching( at, ct, defs2 );

		/* XXX */
		if( ct == NULL || ( ct->attrib & (DAO_TYPE_SPEC|DAO_TYPE_UNDEF)) ){
			if( rettypes->size == 4 ){
				DaoType *ct3 = DaoType_DefineTypes( ct, NS, defs2 );
				if( ct3 != NULL && ct3 != ct ){
					tt = DaoNamespace_MakeRoutType( NS, routine->routType, NULL, NULL, ct3 );
					GC_Assign( & routine->routType, tt );

					ct = (DaoType*)routine->routType->aux;
					rettypes->items.pType[ rettypes->size - 1 ] = ct;
				}
			}else{
				ct = DaoType_DefineTypes( ct, NS, defs2 );
				if( ct != NULL && redef != NULL ){
					tt = DaoType_DefineTypes( types[redef->c], NS, defs2 );
					GC_DecRC( types[redef->c] );
					types[redef->c] = NULL;
					DaoInferencer_UpdateType( self, redef->c, tt );
				}
				rettypes->items.pType[ rettypes->size - 1 ] = ct;
			}
		}
		if( ct != ct2 ){
			AssertTypeMatching( at, ct2, defs2 );
			AssertTypeMatching( ct, ct2, defs2 );
		}
	}
	return 1;
ErrorTyping: return DaoInferencer_Error( self, DTE_TYPE_NOT_MATCHING );
InvalidYield: return DaoInferencer_Error( self, DTE_ROUT_INVALID_YIELD );
InvalidReturn: return DaoInferencer_Error( self, DTE_ROUT_INVALID_RETURN );
MissingReturn: return DaoInferencer_Error( self, DTE_ROUT_MISSING_RETURN );
}

static DaoType* DaoInferencer_HandleVarInvarDecl( DaoInferencer *self, DaoType *at, int opb )
{
	if( at->konst ) at = DaoType_GetBaseType( at ); /* Constant types do not propagate; */

	if( ! (opb & 0x2) ) return at; /* Not an explicit declaration; */

	/* Invar declaration: */
	if( opb & 0x4 ) return DaoType_GetInvarType( at );

	/* Var declaration: */

	/* Move from constant: get base type without const; */
	if( at->konst == 1 ) return DaoType_GetBaseType( at );
	
	/* Move from invariable: */
	if( at->invar == 1 ){
		/* Move from primitive type: get base type without const or invar; */	
		if( at->tid <= DAO_ENUM ){
			return DaoType_GetBaseType( at );
		}else if( DaoType_IsImmutable( at ) ){
			/*
			// Variables of immutable types are immutable regardless of the "var" keyword.
			// Such declaration is permited for conveniece, otherwise, every declaration
			// of variables or parameters of such immutable types (such as invar class or
			// ctype, e.g., DateTime) would require the use of the "invar" keyword, and
			// too verbose to write.
			//
			// Also the point of supporting invar class and invar ctype is exactly that
			// we will not need to write invar everywhere to guarantee its immutability!
			*/
			return at;
		}else{
			DaoInferencer_Error( self, DTE_INVAR_VAL_TO_VAR_VAR );
			return NULL;
		}
	}
	return at;
}

int DaoInferencer_DoInference( DaoInferencer *self )
{
	DNode *node;
	DMap *defs = self->defs;
	DList *errors = self->errors;
	DString *str, *mbs = self->mbstring;
	DaoRoutine *closure;
	DaoVariable *var;
	DaoVmCodeX *vmc;
	DaoType *at, *bt, *ct, *tt, *catype, *ts[DAO_ARRAY+DAO_MAX_PARAM];
	DaoType *type, **tp, **type2, **types = self->types->items.pType;
	DaoValue *value, **consts = self->consts->items.pValue;
	DaoInode *inode, **inodes = self->inodes->items.pInode;
	DaoRoutine *rout, *meth, *routine = self->routine;
	DaoClass *klass, *hostClass = self->hostClass;
	DaoNamespace *NS = routine->nameSpace;
	DaoVmSpace *VMS = self->routine->nameSpace->vmSpace;
	DaoRoutineBody *body = routine->body;
	DaoType **typeVH[DAO_MAX_SECTDEPTH+1] = { NULL };
	DaoType *operands[2];
	DList  *rettypes = self->rettypes;
	DList  *routConsts = routine->routConsts->value;
	daoint i, N = routine->body->annotCodes->size;
	daoint j, k, J, K, M = routine->body->regCount;
	int invarinit = !!(routine->attribs & DAO_ROUT_INITOR);
	int invarmeth = routine->attribs & DAO_ROUT_INVAR;
	int code, opa, opb, opc, first, middle, last;
	int TT1, TT2, TT3, TT6;
	int nestedRoutIndex = 0;

	if( self->inodes->size == 0 ) return 1;

	/*
	DaoRoutine_PrintCode( routine, routine->nameSpace->vmSpace->errorStream );
	*/

	catype = VMS->typeArrays[DAO_COMPLEX];

	for(i=1; i<=DAO_MAX_SECTDEPTH; i++) typeVH[i] = types;

	if( invarinit ){
		invarinit &= routine->routHost->tid == DAO_OBJECT;
		invarinit &= !!(routine->routHost->aux->xClass.attribs & DAO_CLS_INVAR);
	}
	if( invarinit ){
		DaoStream *stream = routine->nameSpace->vmSpace->errorStream;
		DaoType *routype = routine->routType;
		DaoType *retype = (DaoType*) routype->aux;
		DaoType *partype = NULL;
		int error = 0;
		for(i=0; i<routype->args->size; ++i){
			partype = routype->args->items.pType[i];
			if( partype->tid >= DAO_PAR_NAMED && partype->tid <= DAO_PAR_VALIST ){
				partype = (DaoType*) partype->aux;
			}
			if( DaoType_IsPrimitiveOrImmutable( partype ) == 0 ){
				error = DTE_PARAM_WRONG_TYPE;
				break;
			}
		}
		if( error == 0 && DaoType_IsPrimitiveOrImmutable( retype ) == 0 ){
			error = DTE_ROUT_INVALID_RETURN2;
			partype = retype;
		}
		if( error ){
			char char50[50];
			sprintf( char50, "  At line %i : ", routine->defLine );
			DaoInferencer_WriteErrorHeader2( self );
			DaoStream_WriteChars( stream, char50 );
			DaoStream_WriteChars( stream, DaoTypingErrorString[error] );
			DaoStream_WriteChars( stream, " --- \" " );
			DaoStream_WriteChars( stream, partype->name->chars );
			DaoStream_WriteChars( stream, " \";\n" );
			DaoStream_WriteChars( stream, char50 );
			DaoStream_WriteChars( stream, "Expecting primitive or immutable types;\n" );
			return 0;
		}
	}
	if( (routine->attribs & DAO_ROUT_MAIN) && strcmp( routine->routName->chars, "main" ) == 0 ){
		DaoType *routype = routine->routType;
		DaoType *retype = (DaoType*) routype->aux;
		if( retype && retype->tid != DAO_THT && retype->tid != DAO_INTEGER ){
			return DaoInferencer_ErrorTypeNotMatching( self, retype, VMS->typeInt );
		}else if( retype && retype->tid == DAO_THT ){
			routype = DaoNamespace_MakeRoutType( NS, routype, NULL, NULL, VMS->typeInt );
			GC_Assign( & routine->routType, routype );
		}
	}

	DList_Append( rettypes, inodes[N-1] );
	DList_Append( rettypes, NULL );
	DList_Append( rettypes, routine->routType->aux );
	DList_Append( rettypes, routine->routType->aux );
	for(i=0; i<N; i++){
		inodes = self->inodes->items.pInode;
		consts = self->consts->items.pValue;
		types = self->types->items.pType;
		N = self->inodes->size;
		M = self->types->size;
		self->currentIndex = i;
		inode = inodes[i];
		vmc = (DaoVmCodeX*) inode;
		code = vmc->code;
		opa = vmc->a;  opb = vmc->b;  opc = vmc->c;
		at = opa < M ? types[opa] : NULL;
		bt = opb < M ? types[opb] : NULL;
		ct = opc < M ? types[opc] : NULL;
		first = vmc->first;
		middle = first + vmc->middle;
		last = middle + vmc->last;

		if( rettypes->size > 4 ){
			DaoInode *range = rettypes->items.pInode[rettypes->size - 4];
			if( inode == range ){
				DList_Erase( rettypes, rettypes->size - 4, -1 );
				DList_PopBack( self->typeMaps );
			}
		}
		DMap_Reset( defs );
		/*
		// Specialization should only use type mappings from the routine signature,
		// and use additional type mappings from each variable declaration.
		//
		// The type mappings from the routine signature are in the first element
		// in self->typeMaps. After extensive testing, self->typeMaps will be
		// replaced with a simpler field.
		*/
		DMap_Assign( defs, (DMap*)DList_Front( self->typeMaps ) );

#if 0
		DaoRoutine_AnnotateCode( routine, *(DaoVmCodeX*)inode, mbs, 24 );
		printf( "%4i: ", i );DaoVmCodeX_Print( *(DaoVmCodeX*)inode, mbs->chars, NULL );
#endif

		K = DaoVmCode_GetOpcodeType( (DaoVmCode*) inode );
		if( K && K < DAO_CODE_EXPLIST && K != DAO_CODE_SETG && K != DAO_CODE_SETU ){
			if( K != DAO_CODE_MOVE ){
				if( ct != NULL && (ct->tid == DAO_CLASS || ct->tid == DAO_CTYPE) ){
					/* TODO: SETF; see daoParser.c: line 6893; */
					if( K == DAO_CODE_SETI || K == DAO_CODE_SETM ) goto SkipChecking;
				}
				if( ct != NULL && ct->invar != 0 && K == DAO_CODE_SETF ){
					if( ct->tid != DAO_CLASS && ct->tid != DAO_NAMESPACE ) goto ModifyConstant;
				}else if( ct != NULL && ct->invar != 0 && K > DAO_CODE_GETG ){
					if( (code < DVM_RANGE || code > DVM_PACK) && code != DVM_TUPLE_SIM ){
						goto ModifyConstant;
					}
				}
			}
		}
		if( ct != NULL && ct->invar != 0 ){
			if( code == DVM_MOVE || (code >= DVM_MOVE_BB && code <= DVM_MOVE_XX) ){
				if( (opb & 0x1) && !(opb & 0x4) ) goto ModifyConstant;
			}
		}
		if( invarmeth ){
			if( code == DVM_SETVO || (code >= DVM_SETVO_II && code <= DVM_SETVO_CC ) ){
				goto ModifyConstant;
			}
		}

SkipChecking:
		switch( K ){
		case DAO_CODE_GETG :
		case DAO_CODE_SETG :
			routine->body->useNonLocal = 1;
			break;
		case DAO_CODE_MOVE :
			if( code == DVM_LOAD ){
				tt = DaoType_GetAutoCastType( at );
				if( tt != NULL ) DaoInferencer_InsertUntag( self, inode, & inode->a, tt );
			}
			break;
		case DAO_CODE_GETF :
		case DAO_CODE_GETI :
		case DAO_CODE_UNARY :
			tt = DaoType_GetAutoCastType( at );
			if( tt != NULL ) DaoInferencer_InsertUntag( self, inode, & inode->a, tt );
			break;
		case DAO_CODE_GETM :
		case DAO_CODE_ENUM2 :
		case DAO_CODE_ROUTINE :
		case DAO_CODE_CALL :
			tt = DaoType_GetAutoCastType( at );
			if( tt != NULL ){
				DaoInode *untag = DaoInferencer_InsertUntag( self, inode, & inode->a, tt );
				daoint count = K == DAO_CODE_CALL ? opb&0xff : opb;
				daoint j = i - 2*count - 4;

				inodes = self->inodes->items.pInode;
				types = self->types->items.pType;

				if( j < 0 ) j = 0;
				for(; j<i; ++j){
					DaoInode *jnode = inodes[j];
					DaoVmCode ops = DaoVmCode_CheckOperands( (DaoVmCode*) jnode );
					if( ops.c && jnode->c == opa ){
						DaoType *type = types[opa];

						types[opa] = types[untag->c];
						types[untag->c] = type;
						jnode->c = untag->c;
						untag->a = untag->c;
						untag->c = opa;
						inode->a = opa;
						break;
					}
				}
			}
			break;
		case DAO_CODE_UNARY2 :
			tt = DaoType_GetAutoCastType( bt );
			if( tt != NULL ) DaoInferencer_InsertUntag( self, inode, & inode->b, tt );
			break;
		case DAO_CODE_SETF :
		case DAO_CODE_SETI :
			tt = DaoType_GetAutoCastType( ct );
			if( tt != NULL ) DaoInferencer_InsertUntag( self, inode, & inode->c, tt );
			break;
		case DAO_CODE_SETM :
			tt = DaoType_GetAutoCastType( ct );
			if( tt != NULL ){
				DaoInode *untag = DaoInferencer_InsertUntag( self, inode, & inode->c, tt );
				daoint j = i - 2*opb - 4;

				inodes = self->inodes->items.pInode;
				types = self->types->items.pType;

				if( j < 0 ) j = 0;
				for(; j<i; ++j){
					DaoInode *jnode = inodes[j];
					DaoVmCode ops = DaoVmCode_CheckOperands( (DaoVmCode*) jnode );
					if( ops.c && jnode->c == opc ){
						DaoType *type = types[opc];

						types[opc] = types[untag->c];
						types[untag->c] = type;
						jnode->c = untag->c;
						untag->a = untag->c;
						untag->c = opc;
						inode->c = opc;
						break;
					}
				}
			}
			break;
		case DAO_CODE_BINARY :
			if( code == DVM_EQ || code == DVM_NE ) break;
			if( code == DVM_SAME || code == DVM_ISA ) break;
			tt = DaoType_GetAutoCastType( at );
			if( tt != NULL ) DaoInferencer_InsertUntag( self, inode, & inode->a, tt );
			tt = DaoType_GetAutoCastType( bt );
			if( tt != NULL ) DaoInferencer_InsertUntag( self, inode, & inode->b, tt );
			break;
		case DAO_CODE_BINARY2 :
			for(j=opb; j<=opb+1; ++j){
				tt = DaoType_GetAutoCastType( types[j] );
				if( tt != NULL ){
					DaoInode *untag = DaoInferencer_InsertUntag( self, inode, & inode->b, tt );

					inodes = self->inodes->items.pInode;
					types = self->types->items.pType;

					k = i - 6;
					if( k < 0 ) k = 0;
					for(; k<i; ++k){
						DaoInode *jnode = inodes[k];
						DaoVmCode ops = DaoVmCode_CheckOperands( (DaoVmCode*) jnode );
						if( ops.c && jnode->c == j ){
							DaoType *type = types[j];

							types[j] = types[untag->c];
							types[untag->c] = type;
							jnode->c = untag->c;
							untag->a = untag->c;
							untag->c = j;
							inode->b = opb;
							break;
						}
					}
				}
			}
			break;
		}
		if( self->inodes->size != N ){
			i--;
			continue;
		}

		while( nestedRoutIndex < self->routines->size ){
			DaoRoutine *rout = self->routines->items.pRoutine[nestedRoutIndex];
			if( rout->body->codeStart >= inode->first ) break;
			nestedRoutIndex += 1;
			if( DaoRoutine_DoTypeInference( rout, self->silent ) == 0 ) return 0;
		}

		switch( code ){
		case DVM_DATA :
			if( opa > DAO_STRING ) return DaoInferencer_Error( self, DTE_DATA_CANNOT_CREATE );
			at = self->basicTypes[ opa ];
			if( types[opc]== NULL || types[opc]->tid == DAO_UDT ){
				DaoInferencer_UpdateType( self, opc, at );
			}else{
				AssertTypeMatching( at, types[opc], defs );
			}
			value = NULL;
			switch( opa ){
			case DAO_NONE : value = dao_none_value; break;
			case DAO_BOOLEAN : value = (DaoValue*) DaoBoolean_New( opb ); break;
			case DAO_INTEGER : value = (DaoValue*) DaoInteger_New( opb ); break;
			case DAO_FLOAT : value = (DaoValue*) DaoFloat_New( opb ); break;
			}
			GC_Assign( & consts[opc], value );
			if( at->tid >= DAO_BOOLEAN && at->tid <= DAO_COMPLEX ){
				vmc->code = DVM_DATA_B + (at->tid - DAO_BOOLEAN);
			}
			break;
		case DVM_GETCL :
		case DVM_GETCK :
		case DVM_GETCG :
			switch( code ){
			case DVM_GETCL : value = routConsts->items.pValue[opb]; break;
			case DVM_GETCK : value = hostClass->constants->items.pConst[opb]->value; break;
			case DVM_GETCG : value = NS->constants->items.pConst[opb]->value; break;
			}
			at = DaoNamespace_GetType( NS, value );
			if( at->konst == 0 && (code != DVM_GETCL || opa != 0) ){
				/*
				// Do not produce constant type for implicit constant.
				// Consider: a = "something"; a += "else";
				// Also code such as: DVM_GETCL: 0, 2, 3;
				// could have been added by routine decoration.
				// They shouldn't be const type.
				*/
				at = DaoType_GetConstType( at );
			}
			DaoInferencer_UpdateType( self, opc, at );
			/*
			   printf( "at %i %i %p, %p\n", at->tid, types[opc]->tid, at, types[opc] );
			 */
			AssertTypeMatching( at, types[opc], defs );
			GC_Assign( & consts[opc], value );
			if( at->tid >= DAO_BOOLEAN && at->tid <= DAO_COMPLEX ){
				int K = DAO_COMPLEX - DAO_BOOLEAN + 1;
				vmc->code = DVM_GETCL_B + K*(code - DVM_GETCL) + (at->tid - DAO_BOOLEAN);
			}
			break;
		case DVM_GETVH :
		case DVM_GETVS :
		case DVM_GETVO :
		case DVM_GETVK :
		case DVM_GETVG :
			at = 0;
			switch( code ){
			case DVM_GETVH : at = typeVH[opa][opb]; break;
			case DVM_GETVS : at = routine->variables->items.pVar[opb]->dtype; break;
			case DVM_GETVO : at = hostClass->instvars->items.pVar[opb]->dtype; break;
			case DVM_GETVK : at = hostClass->variables->items.pVar[opb]->dtype; break;
			case DVM_GETVG : at = NS->variables->items.pVar[opb]->dtype; break;
			}
			if( at == NULL ) at = VMS->typeUdf;
			if( invarmeth && code == DVM_GETVO && at->konst == 0 ){
				at = DaoType_GetInvarType( at );
			}
			DaoInferencer_UpdateType( self, opc, at );

#if 0
			printf( "%s\n", at->name->chars );
			printf( "%p %p\n", at, types[opc] );
			printf( "%s %s\n", at->name->chars, types[opc]->name->chars );
#endif
			AssertTypeMatching( at, types[opc], defs );
			if( at->tid >= DAO_BOOLEAN && at->tid <= DAO_COMPLEX ){
				int K = DAO_COMPLEX - DAO_BOOLEAN + 1;
				vmc->code = DVM_GETVH_B + K*(code - DVM_GETVH) + (at->tid - DAO_BOOLEAN);
			}
			break;
		case DVM_SETVH :
		case DVM_SETVS :
		case DVM_SETVO :
		case DVM_SETVK :
		case DVM_SETVG :
			if( code == DVM_SETVO && opb == 0 ) goto InvalidOper; /* Invalid move to "self"; */
			var = NULL;
			type2 = NULL;
			switch( code ){
			case DVM_SETVH : type2 = typeVH[opc] + opb; break;
			case DVM_SETVS : var = routine->variables->items.pVar[opb]; break;
			case DVM_SETVO : var = hostClass->instvars->items.pVar[opb]; break;
			case DVM_SETVK : var = hostClass->variables->items.pVar[opb]; break;
			case DVM_SETVG : var = NS->variables->items.pVar[opb]; break;
			}
			if( var ) type2 = & var->dtype;
			at = types[opa];
			if( code == DVM_SETVG ){
				if( !(opc & 0x4) && var->dtype && var->dtype->invar ) goto ModifyConstant;
				at = DaoInferencer_HandleVarInvarDecl( self, at, opc );
				if( at == NULL ) return 0;
			}else if( code == DVM_SETVO && var->subtype == DAO_INVAR ){
				if( !(routine->attribs & DAO_ROUT_INITOR) ) goto ModifyConstant;
				at = DaoType_GetInvarType( at );
			}
			if( at->tid <= DAO_ENUM ) at = DaoType_GetBaseType( at );
			if( type2 && *type2 != NULL && at->tid > DAO_ENUM ){
				int im = DaoType_IsImmutable( at );
				if( im == 0 && type2[0]->var == 1 && (at->invar == 1 && at->konst == 0) ){
					return DaoInferencer_ErrorTypeNotMatching( self, at, *type2 );
				}
			}

			if( type2 ){
				/*
				// Initialization of automatically declared global variable
				// will not have 0x2 flag set. Such SETVG should be handled
				// like the other SETVX instructions.
				// Note: adding 0x2 flag by the parser for such SETVG is not
				// an option, because it will not work if the initialization
				// value is an invariable.
				*/
				if( code == DVM_SETVG && (opc & 0x2) ){
					if( *type2 != NULL ){
						DaoType_ResetTypeHolders( *type2, defs );
						if( DaoType_MatchTo( at, *type2, defs ) ){
							at = DaoType_DefineTypes( *type2, NS, defs );
							GC_Assign( type2, at );
						}
					}else if( at->attrib & DAO_TYPE_SPEC ){
						at = DaoType_DefineTypes( at, NS, defs );
						GC_Assign( type2, at );
					}else{
						GC_Assign( type2, at );
					}
				}else if( *type2 == NULL || (*type2)->tid == DAO_UDT || (*type2)->tid == DAO_THT ){
					GC_Assign( type2, at );
				}
			}

			/* less strict checking */
			if( types[opa]->tid & DAO_ANY ) break;
			if( type2 == NULL ) break;

			k = DaoType_MatchTo( types[opa], *type2, defs );
			if( k ==0 ) return DaoInferencer_ErrorTypeNotMatching( self, types[opa], *type2 );
			at = types[opa];
			if( type2[0]->tid && type2[0]->tid <= DAO_COMPLEX && at->tid && at->tid <= DAO_COMPLEX ){
				int K = DAO_COMPLEX - DAO_BOOLEAN + 1;
				/* Check and make a proper value object with default value: */
				if( var && (var->value == NULL || var->value->type != type2[0]->value->type) ){
					DaoValue_Copy( type2[0]->value, & var->value );
				}
				if( at->tid != type2[0]->tid ){
					DaoInferencer_InsertMove( self, inode, & inode->a, at, *type2 );
				}
				vmc->code = DVM_SETVH_BB + type2[0]->tid - DAO_BOOLEAN;
				vmc->code += K*(code - DVM_SETVH);
			}
			break;
		case DVM_GETI :
		case DVM_GETDI :
			if( DaoInferencer_HandleGetItem( self, inode, defs ) == 0 ) return 0;
			break;
		case DVM_GETMI :
			if( DaoInferencer_HandleGetMItem( self, inode, defs ) == 0 ) return 0;
			break;
		case DVM_GETF :
			if( DaoInferencer_HandleGetField( self, inode, defs ) == 0 ) return 0;
			break;
		case DVM_SETI :
		case DVM_SETDI :
			if( DaoInferencer_HandleSetItem( self, inode, defs ) == 0 ) return 0;
			break;
		case DVM_SETMI :
			if( DaoInferencer_HandleSetMItem( self, inode, defs ) == 0 ) return 0;
			break;
		case DVM_SETF :
			if( DaoInferencer_HandleSetField( self, inode, defs ) == 0 ) return 0;
			break;
		case DVM_CAST :
			if( routConsts->items.pValue[opb]->type != DAO_TYPE ) goto InvalidCasting;
			bt = (DaoType*) routConsts->items.pValue[opb];
			if( bt->tid == DAO_INTERFACE ){
				DaoCinType *cintype = DaoInterface_GetConcrete( (DaoInterface*) bt->aux, at );
				if( cintype ) bt = cintype->vatype;
			}
			ct = bt;
			if( at->tid == DAO_UDT || at->tid == DAO_ANY ){
			}else if( bt->tid == DAO_UDT || bt->tid == DAO_ANY ){
			}else if( ct->tid == DAO_UDT || ct->tid == DAO_ANY ){
			}else if( at->tid == DAO_VARIANT ){
				int mt1 = DaoType_MatchTo( at, bt, NULL );
				int mt2 = DaoType_MatchTo( bt, at, NULL );
				if( mt1 == 0 && mt2 == 0 ) goto InvalidCasting;
			}else if( bt->tid == DAO_VARIANT ){
				/*
				// Casting to a variant type should only be allowed on values
				// that are compatible to one of the item types in the variant.
				// Such that casting to a variant type will not need to do real
				// conversion of values, since it is not well defined which item
				// type should be used as the target type for conversion.
				*/
				if( DaoType_MatchTo( at, bt, NULL ) == 0 ) goto InvalidCasting;
			}else if( at->tid == DAO_INTERFACE ){
				ct = bt; /* The source value could be any type with a concrete interface type; */
			}else if( at->tid == DAO_CINVALUE && bt->tid == DAO_INTERFACE ){
				if( DaoType_MatchTo( at->aux->xCinType.target, bt, NULL ) == 0 ){
					goto InvalidCasting;
				}
				ct = bt;
			}else if( bt->tid == DAO_INTERFACE || bt->tid == DAO_CINVALUE ){
				if( DaoType_MatchTo( at, bt, NULL ) == 0 ) goto InvalidCasting;
			}else if( at->core != NULL && at->core->CheckConversion != NULL ){
				/* Variant type may have null core: */
				ct = at->core->CheckConversion( at, bt, self->routine );
				if( ct == NULL ) goto InvalidCasting;
			}else{
				goto InvalidCasting;
			}
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			at = types[opa];
			ct = types[opc];
			if( at->realnum && ct->realnum ){
				int K = DAO_FLOAT - DAO_BOOLEAN + 1;
				vmc->code = DVM_MOVE_BB + K*(ct->tid - DAO_BOOLEAN) + at->tid - DAO_BOOLEAN;
			}else if( at->tid == DAO_COMPLEX && ct->tid == DAO_COMPLEX ){
				vmc->code = DVM_MOVE_CC;
			}else if( ct->tid >= DAO_BOOLEAN && ct->tid <= DAO_STRING ){
				switch( ct->tid ){
				case DAO_BOOLEAN : vmc->code = DVM_CAST_B; break;
				case DAO_INTEGER : vmc->code = DVM_CAST_I; break;
				case DAO_FLOAT   : vmc->code = DVM_CAST_F; break;
				case DAO_COMPLEX : vmc->code = DVM_CAST_C; break;
				case DAO_STRING  : vmc->code = DVM_CAST_S; break;
				}
			}else if( at->tid == DAO_VARIANT ){
				for(j=0,k=0; j<at->args->size; ++j){
					int mt = DaoType_MatchTo( at->args->items.pType[j], ct, defs );
					k += at->args->items.pType[j]->tid == ct->tid;
					if( mt >= DAO_MT_EQ ){
						if( ct->tid == DAO_ENUM ){
							vmc->code = DVM_CAST_VE;
						}else if( ct->tid == DAO_NONE || ct->tid > DAO_ENUM ){
							vmc->code = DVM_CAST_VX;
						}
						break;
					}
				}
				if( vmc->code == DVM_CAST_VE || vmc->code == DVM_CAST_VX ){
					if( k > 1 ) vmc->code = DVM_CAST; /* not distinctive; */
				}
			}
			break;
		case DVM_CAST_B :
		case DVM_CAST_I :
		case DVM_CAST_F :
		case DVM_CAST_C :
		case DVM_CAST_S :
			if( routConsts->items.pValue[opb]->type != DAO_TYPE ) goto ErrorTyping;
			bt = (DaoType*) routConsts->items.pValue[opb];
			DaoInferencer_UpdateType( self, opc, bt );
			AssertTypeMatching( bt, types[opc], defs );
			switch( types[opc]->tid ){
			case DAO_BOOLEAN : if( vmc->code == DVM_CAST_B ) break; goto ErrorTyping;
			case DAO_INTEGER : if( vmc->code == DVM_CAST_I ) break; goto ErrorTyping;
			case DAO_FLOAT   : if( vmc->code == DVM_CAST_F ) break; goto ErrorTyping;
			case DAO_COMPLEX : if( vmc->code == DVM_CAST_C ) break; goto ErrorTyping;
			case DAO_STRING  : if( vmc->code == DVM_CAST_S ) break; goto ErrorTyping;
			default : goto ErrorTyping;
			}
			break;
		case DVM_CAST_VE :
		case DVM_CAST_VX :
			if( at->tid != DAO_VARIANT ) goto ErrorTyping;
			if( routConsts->items.pValue[opb]->type != DAO_TYPE ) goto ErrorTyping;
			bt = (DaoType*) routConsts->items.pValue[opb];
			DaoInferencer_UpdateType( self, opc, bt );
			AssertTypeMatching( bt, types[opc], defs );
			ct = types[opc];
			for(j=0,k=0; j<at->args->size; ++j){
				int mt = DaoType_MatchTo( at->args->items.pType[j], ct, defs );
				k += at->args->items.pType[j]->tid == ct->tid;
				if( mt >= DAO_MT_EQ ) break;
			}
			if( k != 1 ) goto ErrorTyping;
			switch( ct->tid ){
			case DAO_ENUM : if( vmc->code == DVM_CAST_VE ) break; goto ErrorTyping;
			default : if( vmc->code == DVM_CAST_VX ) break; goto ErrorTyping;
			}
			break;
		case DVM_LOAD :
			DaoInferencer_UpdateType( self, opc, at );
			AssertTypeMatching( at, types[opc], defs );
			if( at == types[opc] && consts[opa] == NULL ){
				if( at->tid == DAO_CSTRUCT || at->tid == DAO_CDATA ){
					if( at->core == NULL || at->core->Copy == NULL ){
						vmc->code = DVM_MOVE_PP;
					}
				}else if( at->tid >= DAO_ARRAY && at->tid <= DAO_TYPE ){
					vmc->code = DVM_MOVE_PP;
				}
			}else if( at == types[opc] ){
				GC_Assign( & consts[opc], consts[opa] );
			}
			break;
		case DVM_MOVE :
			{
				if( opc == 0 && !(routine->attribs & (DAO_ROUT_INITOR|DAO_ROUT_STATIC)) ){
					if( routine->routHost ) goto InvalidOper; /* Invalid move to "self"; */
				}
				at = DaoInferencer_HandleVarInvarDecl( self, at, opb );
				if( at == NULL ) return 0;
				if( opb & 0x2 ){
					if( types[opc] != NULL ){
						DaoType_ResetTypeHolders( types[opc], defs );
						if( DaoType_MatchTo( at, types[opc], defs ) ){
							DaoType *type = DaoType_DefineTypes( types[opc], NS, defs );
							GC_Assign( & types[opc], type );
						}
					}else if( at->attrib & DAO_TYPE_SPEC ){
						DaoType *type = DaoType_DefineTypes( at, NS, defs );
						GC_Assign( & types[opc], type );
					}else{
						GC_Assign( & types[opc], at );
					}
				}else{
					if( at->tid <= DAO_ENUM ) at = DaoType_GetBaseType( at );
					DaoInferencer_UpdateType( self, opc, at );
				}
				k = DaoType_MatchTo( at, types[opc], defs );
				ct = types[opc];

#if 0
				DaoVmCodeX_Print( *vmc, NULL, NULL );
				if( at ) printf( "a: %s %i\n", at->name->chars, at->invar );
				if( ct ) printf( "c: %s %i\n", ct->name->chars, ct->invar );
				printf( "%i  %i\n", DAO_MT_SUB, k );
#endif

				if( at->tid == DAO_UDT || at->tid == DAO_ANY ){
					/* less strict checking */
				}else if( at != ct && (ct->tid == DAO_OBJECT || (ct->tid >= DAO_CSTRUCT && ct->tid <= DAO_CDATA)) ){
					if( ct->tid == DAO_OBJECT ){ // XXX
						meth = DaoClass_FindMethod( & ct->aux->xClass, "=", hostClass );
					}else{
						meth = DaoType_FindFunctionChars( ct, "=" );
					}
					if( meth ){
						rout = DaoRoutine_Check( meth, ct, & at, 1, DVM_CALL, errors );
						if( rout == NULL ) goto NotMatch;
					}else if( k ==0 ){
						return DaoInferencer_ErrorTypeNotMatching( self, at, types[opc] );
					}
				}else if( at->tid ==DAO_TUPLE && DaoType_MatchTo(types[opc], at, defs)){
					/* less strict checking */
				}else if( k ==0 ){
					return DaoInferencer_ErrorTypeNotMatching( self, at, types[opc] );
				}

				if( k == DAO_MT_SIM && at != ct ){
					/* L = { 1.5, 2.5 }; L = { 1, 2 }; L[0] = 3.5 */
					if( at->tid && at->tid <= DAO_COMPLEX && types[opc]->tid == DAO_COMPLEX ){
						if( at->tid < DAO_FLOAT ){
							DaoInferencer_InsertMove( self, inode, & inode->a, at, VMS->typeFloat );
							at = VMS->typeFloat;
						}
						vmc->code = DVM_MOVE_CF + (at->tid - DAO_FLOAT);
					}
					break;
				}

				if( at->realnum && ct->realnum ){
					int K = DAO_FLOAT - DAO_BOOLEAN + 1;
					vmc->code = DVM_MOVE_BB + K*(ct->tid - DAO_BOOLEAN) + at->tid - DAO_BOOLEAN;
				}else if( at->tid && at->tid <= DAO_COMPLEX && ct->tid == DAO_COMPLEX ){
					if( at->tid < DAO_FLOAT ){
						DaoInferencer_InsertMove( self, inode, & inode->a, at, VMS->typeFloat );
						at = VMS->typeFloat;
					}
					vmc->code = DVM_MOVE_CF + (at->tid - DAO_FLOAT);
				}else if( at->tid == DAO_STRING && ct->tid == DAO_STRING ){
					vmc->code = DVM_MOVE_SS;
				}else if( at == ct || ct->tid == DAO_ANY ){
					if( types[opa]->konst ){
						vmc->code = DVM_MOVE_XX;
					}else if( (at->tid == DAO_CSTRUCT || at->tid == DAO_CDATA) && consts[opa] == NULL ){
						if( at->core == NULL || at->core->Copy == NULL ){
							vmc->code = DVM_MOVE_PP;
						}
					}else if( at->tid >= DAO_ARRAY && at->tid <= DAO_TYPE && consts[opa] == NULL ){
						vmc->code = DVM_MOVE_PP;
					}else{
						vmc->code = DVM_MOVE_XX;
					}
				}
				break;
			}
		case DVM_UNTAG :
			tt = DaoType_GetAutoCastType( at );
			if( tt == NULL ) goto ErrorTyping;
			DaoInferencer_UpdateType( self, opc, tt );
			AssertTypeMatching( tt, types[opc], defs );
			break;
		case DVM_ADD : case DVM_SUB : case DVM_MUL :
		case DVM_DIV : case DVM_MOD : case DVM_POW :
			if( DaoInferencer_HandleBinaryArith( self, inode, defs ) == 0 ) return 0;
			break;
		case DVM_AND : case DVM_OR :
			if( DaoInferencer_HandleBinaryBool( self, inode, defs ) == 0 ) return 0;
			break;
		case DVM_LT : case DVM_LE :
		case DVM_EQ : case DVM_NE :
			ct = DaoInferencer_CheckBinaryOper( self, inode, at, bt );
			if( ct == NULL ){
				if( vmc->code == DVM_EQ || vmc->code == DVM_NE ){
					ct = VMS->typeBool;
				}else{
					goto InvalidOper;
				}
			}

			DaoInferencer_UpdateVarType( self, opc, ct );
			/* allow less strict typing: */
			if( ct->tid == DAO_UDT || ct->tid == DAO_ANY ) break;
			AssertTypeMatching( ct, types[opc], defs );
			ct = types[opc];
			if( at->realnum && bt->realnum && ct->realnum ){
				DaoType *max = at->tid > bt->tid ? at : bt;
				if( at->tid != max->tid ){
					DaoInferencer_InsertMove( self, inode, & inode->a, at, max );
					if( opa == opb ) inode->b = inode->a;
				}
				if( opa != opb && bt->tid != max->tid )
					DaoInferencer_InsertMove( self, inode, & inode->b, bt, max );

				switch( max->tid ){
				case DAO_BOOLEAN : vmc->code += DVM_LT_BBB - DVM_LT; break;
				case DAO_INTEGER : vmc->code += DVM_LT_BII - DVM_LT; break;
				case DAO_FLOAT  : vmc->code += DVM_LT_BFF - DVM_LT; break;
				}
				if( ct->tid != DAO_BOOLEAN )
					DaoInferencer_InsertMove2( self, inode, VMS->typeBool, ct );
			}else if( ct->realnum && at->tid == bt->tid && bt->tid == DAO_COMPLEX ){
				vmc->code += DVM_EQ_BCC - DVM_EQ;
				if( ct->tid != DAO_BOOLEAN )
					DaoInferencer_InsertMove2( self, inode, VMS->typeBool, ct );
			}else if( ct->realnum && at->tid == bt->tid && bt->tid == DAO_STRING ){
				vmc->code += DVM_LT_BSS - DVM_LT;
				if( ct->tid != DAO_BOOLEAN )
					DaoInferencer_InsertMove2( self, inode, VMS->typeBool, ct );
			}
			break;
		case DVM_IN :
			if( bt->core == NULL || bt->core->CheckBinary == NULL ) goto InvalidOper;
			operands[0] = at;
			operands[1] = bt;
			ct = bt->core->CheckBinary( bt, (DaoVmCode*)vmc, operands, self->routine );
			if( ct == NULL ) goto InvalidOper;

			DaoInferencer_UpdateVarType( self, opc, ct );
			/* allow less strict typing: */
			if( ct->tid == DAO_UDT || ct->tid == DAO_ANY ) break;
			AssertTypeMatching( ct, types[opc], defs );
			break;
		case DVM_NOT :
			if( at->core == NULL || at->core->CheckUnary == NULL ) goto InvalidOper;
			ct = at->core->CheckUnary( at, (DaoVmCode*)vmc, self->routine );
			if( ct == NULL ) goto InvalidOper;
			DaoInferencer_UpdateVarType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			if( DaoType_LooseChecking( at ) ) break;
			ct = types[opc];
			if( at->realnum ){
				if( ct->realnum ) inode->code = DVM_NOT_B + (at->tid - DAO_BOOLEAN);
				if( ct->tid != DAO_BOOLEAN ) DaoInferencer_InsertMove2( self, inode, VMS->typeBool, ct );
				break;
			}
			break;
		case DVM_MINUS :
			if( at->core == NULL || at->core->CheckUnary == NULL ) goto InvalidOper;
			ct = at->core->CheckUnary( at, (DaoVmCode*)vmc, self->routine );
			if( ct == NULL ) goto InvalidOper;
			DaoInferencer_UpdateVarType( self, opc, ct );
			AssertTypeMatching( at, ct, defs );
			if( DaoType_LooseChecking( at ) ) break;
			if( at->tid >= DAO_INTEGER && at->tid <= DAO_COMPLEX ){
				if( at != ct ) DaoInferencer_InsertMove( self, inode, & inode->a, at, ct );
				inode->code = DVM_MINUS_I + (ct->tid - DAO_INTEGER);
				break;
			}
			break;
		case DVM_TILDE :
			{
				if( at->core == NULL || at->core->CheckUnary == NULL ) goto InvalidOper;
				ct = at->core->CheckUnary( at, (DaoVmCode*)vmc, self->routine );
				if( ct == NULL ) goto InvalidOper;
				DaoInferencer_UpdateVarType( self, opc, ct );
				AssertTypeMatching( at, ct, defs );
				if( at->realnum && ct->realnum ){
					if( at->tid != DAO_INTEGER )
						DaoInferencer_InsertMove( self, inode, & inode->a, at, VMS->typeInt );
					vmc->code = DVM_TILDE_I;
					if( ct->tid != DAO_INTEGER )
						DaoInferencer_InsertMove2( self, inode, VMS->typeInt, ct );
				}else if( at->tid == DAO_COMPLEX && ct->tid == DAO_COMPLEX ){
					vmc->code = DVM_TILDE_C;
				}
				break;
			}
		case DVM_SIZE :
			{
				if( at->core == NULL || at->core->CheckUnary == NULL ) goto InvalidOper;
				ct = at->core->CheckUnary( at, (DaoVmCode*)vmc, self->routine );
				if( ct == NULL ) goto InvalidOper;
				DaoInferencer_UpdateVarType( self, opc, ct );
				AssertTypeMatching( VMS->typeInt, types[opc], defs );
				if( at->tid >= DAO_NONE && at->tid <= DAO_COMPLEX ){
					vmc->code = DVM_DATA_I;
					vmc->a = DAO_INTEGER;
					switch( at->tid ){
					case DAO_NONE    : vmc->b = 0; break;
					case DAO_BOOLEAN : vmc->b = sizeof(dao_integer); break;
					case DAO_INTEGER : vmc->b = sizeof(dao_integer); break;
					case DAO_FLOAT   : vmc->b = sizeof(dao_float); break;
					case DAO_COMPLEX : vmc->b = sizeof(dao_complex); break;
					}
				}else if( at->tid <= DAO_TUPLE ){
					vmc->code = DVM_SIZE_X;
				}
				break;
			}
		case DVM_BITAND : case DVM_BITOR : case DVM_BITXOR :
		case DVM_BITLFT : case DVM_BITRIT :
			{
				ct = DaoInferencer_CheckBinaryOper( self, inode, at, bt );
				if( ct == NULL ) goto InvalidOper;

				DaoInferencer_UpdateVarType( self, opc, ct );
				/* allow less strict typing: */
				if( ct->tid == DAO_UDT || ct->tid == DAO_ANY ) break;
				AssertTypeMatching( ct, types[opc], defs );
				ct = types[opc];
				if( at->realnum && bt->realnum && ct->realnum ){
					vmc->code += DVM_BITAND_III - DVM_BITAND;
					if( at->tid != DAO_INTEGER ){
						DaoInferencer_InsertMove( self, inode, & inode->a, at, VMS->typeInt );
						if( opa == opb ) inode->b = inode->a;
					}
					if( opa != opb && bt->tid != DAO_INTEGER )
						DaoInferencer_InsertMove( self, inode, & inode->b, bt, VMS->typeInt );
					if( ct->tid != DAO_INTEGER )
						DaoInferencer_InsertMove2( self, inode, VMS->typeInt, ct );
				}
				break;
			}
		case DVM_SAME :
			{
				DaoInferencer_UpdateVarType( self, opc, VMS->typeBool );
				if( DaoType_LooseChecking( at ) || DaoType_LooseChecking( bt ) ) break;
				AssertTypeMatching( VMS->typeInt, types[opc], defs );
				if( at->tid != DAO_VARIANT && bt->tid != DAO_VARIANT ){
					if( types[opc]->tid == DAO_BOOLEAN ){
						if( at->tid != bt->tid ){
							vmc->code = DVM_DATA_B;
							vmc->b = 0;
						}else if( at->tid <= DAO_STRING ){
							vmc->code = DVM_DATA_B;
							vmc->b = 1;
						}
					}
				}
				break;
			}
		case DVM_ISA :
			{
				DaoInferencer_UpdateVarType( self, opc, VMS->typeBool );
				if( DaoType_LooseChecking( bt ) ) break;
				if( bt->tid == DAO_CTYPE || bt->tid == DAO_CLASS ){
					AssertTypeMatching( VMS->typeBool, types[opc], defs );
					break;
				}
				if( bt->tid != DAO_NONE && bt->tid != DAO_TYPE ) goto ErrorTyping;
				AssertTypeMatching( VMS->typeBool, types[opc], defs );
				ct = types[opc];
				k = bt->tid == DAO_TYPE ? bt->args->items.pType[0]->tid : DAO_UDT;
				if( k <= DAO_STRING && ct->tid == DAO_BOOLEAN ){
					if( at->tid == k ){
						vmc->code = DVM_DATA_B;
						vmc->b = 1;
					}else{
						vmc->code = DVM_ISA_ST;
					}
				}
				break;
			}
		case DVM_ISA_ST :
			{
				DaoInferencer_UpdateType( self, opc, VMS->typeBool );
				if( bt->tid != DAO_TYPE ) goto ErrorTyping;
				k = bt->tid == DAO_TYPE ? bt->args->items.pType[0]->tid : DAO_UDT;
				if( k > DAO_STRING ) goto ErrorTyping;
				break;
			}
		case DVM_NAMEVA :
			{
				ct = DaoNamespace_MakeType2( NS, routConsts->items.pValue[opa]->xString.value->chars,
						DAO_PAR_NAMED, (DaoValue*) types[opb], 0, 0 );
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_RANGE :
			{
				if( types[opc] && types[opc]->tid == DAO_ANY ) break;
				ct = DaoNamespace_MakeRangeType( NS, types[opa], types[opb] );
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_TUPLE :
			{
				int mode = opb >> 14;
				opb = opb & (0xffff>>2);
				ct = NULL;
				if( mode == DVM_ENUM_MODE1 ){
					k = routine->routType->args->size;
					tp = routine->routType->args->items.pType;
					ct = DaoNamespace_MakeType( NS, "tuple", DAO_TUPLE, NULL, tp+opa, k-opa );
					DaoInferencer_UpdateType( self, opc, ct );
				}else if( mode == DVM_ENUM_MODE2 ){
					DaoType *t, *its[DAO_MAX_PARAM];
					DaoInode *sect = inodes[i-1];
					int count = sect->c;

					memcpy( its, types + opa, sect->c * sizeof(DaoType*) );
					if( opb > sect->c ){
						t = DaoNamespace_MakeType( NS, "...", DAO_PAR_VALIST, 0, 0 ,0 );
						its[count++] = t;
					}
					ct = DaoNamespace_MakeType2( NS, "tuple", DAO_TUPLE, 0, its, count );
					DaoInferencer_UpdateType( self, opc, ct );
				}
				if( ct == NULL ){
					ct = DaoNamespace_MakeType2( NS, "tuple", DAO_TUPLE, 0, types + opa, opb );
					DaoInferencer_UpdateType( self, opc, ct );
					if( types[opc]->variadic == 0 && opb > types[opc]->args->size ){
						goto InvalidEnum;
					}
					if( mode == DVM_ENUM_MODE0 && opb == types[opc]->args->size ){
						for(j=0; j<opb; ++j){
							DaoType *t = types[opc]->args->items.pType[j];
							tt = types[opa+j];
							if( t->tid >= DAO_PAR_NAMED && t->tid <= DAO_PAR_VALIST ) break;
							if( tt->tid >= DAO_PAR_NAMED && tt->tid <= DAO_PAR_VALIST ) break;
						}
						if( j >= opb ) vmc->code = DVM_TUPLE_SIM;
					}
				}
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_TUPLE_SIM :
			ct = DaoNamespace_MakeType2( NS, "tuple", DAO_TUPLE, 0, types + opa, opb );
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			break;
		case DVM_LIST : case DVM_VECTOR :
			if( DaoInferencer_HandleListArrayEnum( self, inode, defs ) == 0 ) return 0;
			break;
		case DVM_MAP :
			{
				opb = opb & (0xffff>>2);
				if( types[opc] && types[opc]->tid == DAO_ANY ) break;
				if( types[opc] && types[opc]->tid == DAO_MAP ){
					if( types[opc]->args && types[opc]->args->size == 2 ){
						DaoType *kt = types[opc]->args->items.pType[0];
						DaoType *vt = types[opc]->args->items.pType[1];
						for(j=0; j<opb; j+=2){
							AssertTypeMatching( types[opa+j], kt, defs );
							AssertTypeMatching( types[opa+j+1], vt, defs );
						}
						break;
					}
				}
				ts[0] = ts[1] = VMS->typeUdf;
				if( opb > 0 ){
					ts[0] = types[opa];
					ts[1] = types[opa+1];
					for(j=2; j<opb; j+=2){
						if( DaoType_MatchTo( types[opa+j], ts[0], defs ) ==0 ) ts[0] = NULL;
						if( DaoType_MatchTo( types[opa+j+1], ts[1], defs ) ==0 ) ts[1] = NULL;
						if( ts[0] ==NULL && ts[1] ==NULL ) break;
					}
				}else if( opb == 0 && types[opc] != NULL && types[opc]->tid == DAO_MAP ){
					break;
				}
				if( ts[0] ==NULL ) ts[0] = opb ? VMS->typeAny : VMS->typeUdf;
				if( ts[1] ==NULL ) ts[1] = opb ? VMS->typeAny : VMS->typeUdf;
				ct = DaoNamespace_MakeType2( NS, "map", DAO_MAP, NULL, ts, 2 );
				if( opb == 0 ) ct = VMS->typeMapEmpty;
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_MATRIX :
			{
				k = (vmc->b >> 8) * (0xff & vmc->b);
				if( types[opc] && types[opc]->tid == DAO_ANY ) break;
				if( k == 0 && types[opc] != NULL ) break;
				at = k > 0 ? types[opa] : VMS->typeUdf;
				for( j=0; j<k; j++){
					if( DaoType_MatchTo( types[opa+j], at, defs )==0 ) goto ErrorTyping;
				}
				at = DaoType_GetBaseType( at );
				ct = DaoType_Specialize( VMS->typeArray, & at, at != NULL, NS );
				if( ct == NULL ){
					return DaoInferencer_Error( self, DTE_INVALID_ENUMERATION );
				}
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_PACK :
			{
				ct = NULL;
				if( at->tid == DAO_TYPE ) at = at->args->items.pType[0];
				if( at->tid == DAO_CLASS && at->aux == NULL ){  /* "class" */
					ct = VMS->typeAny;
				}else if( at->tid == DAO_CLASS ){
					klass = & at->aux->xClass;
					if( !(klass->attribs & DAO_CLS_AUTO_INITOR) ) goto InvalidOper;
					if( klass->attribs & (DAO_CLS_PRIVATE_VAR|DAO_CLS_PROTECTED_VAR) ) goto InvalidOper;
					if( opb >= klass->instvars->size ) goto InvalidEnum;
					str = klass->className;
					ct = klass->objType;
					for(j=1; j<=opb; j++){
						int id = j;
						bt = types[opa+j];
						if( bt == NULL ) goto InvalidEnum;
						if( bt->tid == DAO_PAR_NAMED ){
							id = DaoClass_GetDataIndex( klass, bt->fname );
							if( LOOKUP_ST( id ) != DAO_OBJECT_VARIABLE ) goto InvalidEnum;
							bt = & bt->aux->xType;
							id = LOOKUP_ID( id );
						}
						tt = klass->instvars->items.pVar[id]->dtype;
						AssertTypeMatching( bt, tt, defs );
					}
				}else if( at->tid == DAO_TUPLE ){
					ct = at;
					if( opb < (at->args->size - at->variadic) ) goto InvalidEnum;
					if( at->variadic == 0 && opb > at->args->size ) goto InvalidEnum;
					for(j=0; j<opb; j++){
						bt = types[opa+1+j];
						if( bt == NULL ) goto ErrorTyping;
						if( bt->tid == DAO_PAR_NAMED ){
							if( j >= (at->args->size - at->variadic) ) goto InvalidEnum;
							if( at->mapNames == NULL ) goto InvalidEnum;
							node = MAP_Find( at->mapNames, bt->fname );
							if( node == NULL || node->value.pInt != j ) goto InvalidEnum;
							bt = & bt->aux->xType;
						}
						if( j >= at->args->size ){
							tt = at->args->items.pType[at->args->size-1];
						}else{
							tt = at->args->items.pType[j];
						}
						if( tt->tid >= DAO_PAR_NAMED && tt->tid <= DAO_PAR_VALIST ) tt = & tt->aux->xType;
						AssertTypeMatching( bt, tt, defs );
					}
				}else{
					ct = VMS->typeUdf;
				}
				DaoInferencer_UpdateType( self, opc, ct );
				if( at->tid == DAO_ANY || at->tid == DAO_UDT ) break;
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_SWITCH :
			if( DaoInferencer_HandleSwitch( self, inode, i, defs ) == 0 ) return 0;
			break;
		case DVM_CASE :
			break;
		case DVM_ITER :
			{
				if( vmc->b > 0 ){
					int j;
					for(j=0; j<vmc->b; ++j){
						if( DaoType_LooseChecking( types[opa+j] ) ) continue;
						if( types[opa+j]->subtid != DAO_ITERATOR ){
							return DaoInferencer_ErrorTypeID( self, types[opa+j], DAO_ITERATOR );
						}
					}    
					DaoInferencer_UpdateType( self, opc, VMS->typeBool );
					break;
				}
				if( DaoType_LooseChecking( at ) ){
					ct = VMS->typeAny;
				}else{
					if( at->core == NULL || at->core->CheckForEach == NULL ) goto InvalidOper;
					ct = at->core->CheckForEach( at, self->routine );
					if( ct == NULL ) goto InvalidOper;
				}
				DaoInferencer_UpdateType( self, opc, ct );
				AssertTypeMatching( ct, types[opc], defs );
				break;
			}
		case DVM_GOTO :
			break;
		case DVM_TEST :
			{
				if( types[opa] == NULL ) goto NotMatch;
				if( at->tid == DAO_STRING ) goto NotMatch;
				if( at->subtid == DAO_ENUM_SYM ) goto NotMatch;
				if( at->tid >= DAO_ARRAY && at->tid < DAO_TUPLE ) goto NotMatch;
				if( at->tid == DAO_TUPLE && at->subtid != DAO_ITERATOR ) goto NotMatch;
				if( consts[opa] && consts[opa]->type <= DAO_COMPLEX ){
					vmc->code =  DaoValue_IsZero( consts[opa] ) ? (int)DVM_GOTO : (int)DVM_UNUSED;
					break;
				}
				if( at->tid >= DAO_BOOLEAN && at->tid <= DAO_FLOAT )
					vmc->code = DVM_TEST_B + at->tid - DAO_BOOLEAN;
				break;
			}
		case DVM_MATH :
			if( bt->tid == DAO_NONE ) goto InvalidParam;
			if( bt->tid > DAO_COMPLEX && !(bt->tid & DAO_ANY) ) goto InvalidParam;
			ct = bt; /* return the same type as the argument by default; */
			K = bt->realnum ? DVM_MATH_B + (bt->tid - DAO_BOOLEAN) : DVM_MATH;
			if( opa >= DVM_MATH_MIN ){
				at = bt;
				bt = opb+1 < M ? types[opb+1] : NULL;
				ct = at->tid > bt->tid ? at : bt;
				DaoInferencer_UpdateVarType( self, opc, ct );
				if( ct->tid <= DAO_COMPLEX && types[opc]->tid == ct->tid ){
					if( at->tid == ct->tid && bt->tid == ct->tid ) code = K;
				}
			}else if( opa <= DVM_MATH_FLOOR ){
				DaoInferencer_UpdateVarType( self, opc, ct );
				if( bt->tid <= DAO_COMPLEX && types[opc]->tid == bt->tid ) code = K;
			}else if( opa == DVM_MATH_ABS ){
				if( bt->tid == DAO_COMPLEX ) ct = VMS->typeFloat; /* return double; */
				DaoInferencer_UpdateVarType( self, opc, ct );
				if( bt->tid == DAO_COMPLEX && types[opc]->tid == DAO_FLOAT ) code = K;
				if( bt->realnum && types[opc]->tid == bt->tid ) code = K;
			}else if( opa <= DVM_MATH_REAL ){
				if( bt->tid != DAO_COMPLEX && !(bt->tid & DAO_ANY) ) goto InvalidParam;
				ct = VMS->typeFloat; /* return double; */
				DaoInferencer_UpdateVarType( self, opc, ct );
				if( bt->tid == DAO_COMPLEX && types[opc]->tid == DAO_FLOAT ) code = K;
			}else if( bt->tid >= DAO_BOOLEAN && bt->tid <= DAO_FLOAT ){
				ct = VMS->typeFloat; /* return float; */
				DaoInferencer_UpdateVarType( self, opc, ct );
				if( types[opc]->tid == DAO_FLOAT ) code = K;
			}else{
				DaoInferencer_UpdateVarType( self, opc, ct );
				if( bt->tid <= DAO_COMPLEX && types[opc]->tid == bt->tid ) code = K;
			}
			AssertTypeMatching( ct, types[opc], defs );
			inode->code = code;
			break;
		case DVM_CALL : case DVM_MCALL :
			if( DaoInferencer_HandleCall( self, inode, i, defs ) == 0 ) return 0;
			break;
		case DVM_ROUTINE :
			if( DaoInferencer_HandleClosure( self, inode, i, defs ) == 0 ) return 0;
			break;

		case DVM_RETURN :
		case DVM_YIELD :
			if( DaoInferencer_HandleYieldReturn( self, inode, defs ) == 0 ) return 0;
			break;

		case DVM_DATA_B : case DVM_DATA_I : case DVM_DATA_F : case DVM_DATA_C :
			TT1 = DAO_BOOLEAN + (code - DVM_DATA_B);
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETCL_B : case DVM_GETCL_I : case DVM_GETCL_F : case DVM_GETCL_C :
			value = routConsts->items.pValue[opb];
			TT1 = DAO_BOOLEAN + (code - DVM_GETCL_B);
			at = DaoNamespace_GetType( NS, value );
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETCK_B : case DVM_GETCK_I : case DVM_GETCK_F : case DVM_GETCK_C :
			value = hostClass->constants->items.pConst[opb]->value;
			TT1 = DAO_BOOLEAN + (code - DVM_GETCK_B);
			at = DaoNamespace_GetType( NS, value );
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETCG_B : case DVM_GETCG_I : case DVM_GETCG_F : case DVM_GETCG_C :
			value = NS->constants->items.pConst[opb]->value;
			TT1 = DAO_BOOLEAN + (code - DVM_GETCG_B);
			at = DaoNamespace_GetType( NS, value );
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETVH_B : case DVM_GETVH_I : case DVM_GETVH_F : case DVM_GETVH_C :
			TT1 = DAO_BOOLEAN + (code - DVM_GETVH_B);
			at = typeVH[opa][opb];
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETVS_B : case DVM_GETVS_I : case DVM_GETVS_F : case DVM_GETVS_C :
			TT1 = DAO_BOOLEAN + (code - DVM_GETVS_B);
			at = routine->variables->items.pVar[opb]->dtype;
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETVO_B : case DVM_GETVO_I : case DVM_GETVO_F : case DVM_GETVO_C :
			TT1 = DAO_BOOLEAN + (code - DVM_GETVO_B);
			at = hostClass->instvars->items.pVar[opb]->dtype;
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETVK_B : case DVM_GETVK_I : case DVM_GETVK_F : case DVM_GETVK_C :
			TT1 = DAO_BOOLEAN + (code - DVM_GETVK_B);
			at = hostClass->variables->items.pVar[opb]->dtype;
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_GETVG_B : case DVM_GETVG_I : case DVM_GETVG_F : case DVM_GETVG_C :
			TT1 = DAO_BOOLEAN + (code - DVM_GETVG_B);
			at = NS->variables->items.pVar[opb]->dtype;
			ct = DaoInferencer_UpdateType( self, opc, self->basicTypes[TT1] );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( ct, TT1 );
			break;
		case DVM_SETVH_BB : case DVM_SETVH_II : case DVM_SETVH_FF : case DVM_SETVH_CC :
			tp = typeVH[opc] + opb;
			at = DaoInferencer_HandleVarInvarDecl( self, at, 0 );
			if( at == NULL ) return 0;
			if( at->tid <= DAO_ENUM ) at = DaoType_GetBaseType( at );
			if( *tp == NULL || (*tp)->tid == DAO_UDT || (*tp)->tid == DAO_THT ){
				GC_Assign( tp, at );
			}
			TT1 = DAO_BOOLEAN + (code - DVM_SETVH_BB);
			AssertTypeMatching( at, *tp, defs );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( tp[0], TT1 );
			break;
		case DVM_SETVS_BB : case DVM_SETVS_II : case DVM_SETVS_FF : case DVM_SETVS_CC :
			var = routine->variables->items.pVar[opb];
			at = DaoInferencer_HandleVarInvarDecl( self, at, 0 );
			if( at == NULL ) return 0;
			if( at->tid <= DAO_ENUM ) at = DaoType_GetBaseType( at );
			if( var->dtype == NULL || var->dtype->tid == DAO_UDT || var->dtype->tid == DAO_THT ){
				DaoVariable_SetType( var, at );
			}
			TT1 = DAO_BOOLEAN + (code - DVM_SETVS_BB);
			AssertTypeMatching( at, var->dtype, defs );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( var->dtype, TT1 );
			break;
		case DVM_SETVO_BB : case DVM_SETVO_II : case DVM_SETVO_FF : case DVM_SETVO_CC :
			if( self->tidHost != DAO_OBJECT ) goto ErrorTyping;
			var = hostClass->instvars->items.pVar[opb];
			at = DaoInferencer_HandleVarInvarDecl( self, at, 0 );
			if( at == NULL ) return 0;
			if( var->subtype == DAO_INVAR ){
				if( !(routine->attribs & DAO_ROUT_INITOR) ) goto ModifyConstant;
				at = DaoType_GetInvarType( at );
			}
			if( var->dtype == NULL || var->dtype->tid == DAO_UDT || var->dtype->tid == DAO_THT ){
				DaoVariable_SetType( var, at );
			}
			TT1 = DAO_BOOLEAN + (code - DVM_SETVO_BB);
			AssertTypeMatching( at, var->dtype, defs );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( var->dtype, TT1 );
			break;
		case DVM_SETVK_BB : case DVM_SETVK_II : case DVM_SETVK_FF : case DVM_SETVK_CC :
			var = hostClass->variables->items.pVar[opb];
			at = DaoInferencer_HandleVarInvarDecl( self, at, 0 );
			if( at == NULL ) return 0;
			if( at->tid <= DAO_ENUM ) at = DaoType_GetBaseType( at );
			if( var->dtype == NULL || var->dtype->tid == DAO_UDT || var->dtype->tid == DAO_THT ){
				DaoVariable_SetType( var, at );
			}
			TT1 = DAO_BOOLEAN + (code - DVM_SETVK_BB);
			AssertTypeMatching( at, var->dtype, defs );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( var->dtype, TT1 );
			break;
		case DVM_SETVG_BB : case DVM_SETVG_II : case DVM_SETVG_FF : case DVM_SETVG_CC :
			var = NS->variables->items.pVar[opb];
			if( !(opc & 0x4) && var->dtype && var->dtype->invar ) goto ModifyConstant;
			at = DaoInferencer_HandleVarInvarDecl( self, at, opc );
			if( at == NULL ) return 0;
			if( at->tid <= DAO_ENUM ) at = DaoType_GetBaseType( at );
			if( var->dtype == NULL || var->dtype->tid == DAO_UDT || var->dtype->tid == DAO_THT ){
				DaoVariable_SetType( var, at );
			}
			TT1 = DAO_BOOLEAN + (code - DVM_SETVG_BB);
			AssertTypeMatching( at, var->dtype, defs );
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( var->dtype, TT1 );
			break;
		case DVM_MOVE_BB : case DVM_MOVE_BI : case DVM_MOVE_BF :
		case DVM_MOVE_IB : case DVM_MOVE_II : case DVM_MOVE_IF :
		case DVM_MOVE_FB : case DVM_MOVE_FI : case DVM_MOVE_FF :
		case DVM_MOVE_CB : case DVM_MOVE_CI : case DVM_MOVE_CF :
			k = DAO_FLOAT - DAO_BOOLEAN + 1;
			TT1 = DAO_BOOLEAN + (code - DVM_MOVE_BB) % k;
			TT3 = DAO_BOOLEAN + ((code - DVM_MOVE_BB)/k) % 4;
			at = DaoInferencer_HandleVarInvarDecl( self, at, opb );
			if( at == NULL ) return 0;
			if( opb & 0x2 ){
				if( ct == NULL || ct->tid == DAO_UDT || ct->tid == DAO_THT ){
					ct = self->basicTypes[TT3];
					if( opb & 0x4 ) ct = DaoType_GetInvarType( ct );
					GC_Assign( & types[opc], ct );
				}
			}else{
				DaoInferencer_UpdateType( self, opc, self->basicTypes[TT3] );
			}
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( types[opc], TT3 );
			break;
		case DVM_NOT_B : case DVM_NOT_I : case DVM_NOT_F :
			DaoInferencer_UpdateVarType( self, opc, VMS->typeBool );
			TT1 = DAO_BOOLEAN + code - DVM_NOT_B;
			TT3 = DAO_BOOLEAN;
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( types[opc], TT3 );
			break;
		case DVM_MINUS_I : case DVM_MINUS_F :
			DaoInferencer_UpdateVarType( self, opc, at );
			TT1 = TT3 = DAO_INTEGER + code - DVM_MINUS_I;
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( types[opc], TT3 );
			break;
		case DVM_TILDE_I :
			DaoInferencer_UpdateVarType( self, opc, at );
			AssertTypeIdMatching( at, DAO_INTEGER );
			AssertTypeIdMatching( types[opc], DAO_INTEGER );
			break;
		case DVM_TILDE_C :
			DaoInferencer_UpdateVarType( self, opc, at );
			AssertTypeIdMatching( at, DAO_COMPLEX );
			AssertTypeIdMatching( types[opc], DAO_COMPLEX );
			break;
		case DVM_SIZE_X :
			DaoInferencer_UpdateVarType( self, opc, VMS->typeInt );
			if( at->tid > DAO_TUPLE ) goto NotMatch;
			AssertTypeIdMatching( types[opc], DAO_INTEGER );
			break;
		case DVM_MINUS_C :
			DaoInferencer_UpdateVarType( self, opc, at );
			TT1 = TT3 = code == DVM_MOVE_SS ? DAO_STRING : DAO_COMPLEX;
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( types[opc], TT3 );
			break;
		case DVM_MOVE_CC :
		case DVM_MOVE_SS :
			at = DaoInferencer_HandleVarInvarDecl( self, at, opb );
			if( at == NULL ) return 0;
			if( opb & 0x2 ){
				if( ct == NULL || ct->tid == DAO_UDT || ct->tid == DAO_THT ){
					GC_Assign( & types[opc], at );
				}
			}else{
				DaoInferencer_UpdateType( self, opc, at );
			}
			TT1 = TT3 = code == DVM_MOVE_SS ? DAO_STRING : DAO_COMPLEX;
			AssertTypeIdMatching( at, TT1 );
			AssertTypeIdMatching( types[opc], TT3 );
			break;
		case DVM_MOVE_PP :
		case DVM_MOVE_XX :
			if( code == DVM_MOVE_PP ){
				if( at->tid == DAO_CSTRUCT || at->tid == DAO_CDATA ){
					if( at->core != NULL && at->core->Copy != NULL ) goto NotMatch;
				}else if( at->tid && (at->tid < DAO_ARRAY || at->tid > DAO_TYPE) ){
					goto NotMatch;
				}
			}
			at = DaoInferencer_HandleVarInvarDecl( self, at, opb );
			if( at == NULL ) return 0;
			if( opb & 0x2 ){
				if( ct == NULL || ct->tid == DAO_UDT || ct->tid == DAO_THT ){
					GC_Assign( & types[opc], at );
				}
			}else{
				DaoInferencer_UpdateType( self, opc, at );
			}
			if( types[opc]->tid != DAO_ANY ){
				if( DaoType_MatchTo( types[opc], at, NULL ) != DAO_MT_EQ ) goto NotMatch;
			}
			break;
		case DVM_AND_BBB : case DVM_OR_BBB : case DVM_LT_BBB :
		case DVM_LE_BBB  : case DVM_EQ_BBB : case DVM_NE_BBB :
			DaoInferencer_UpdateVarType( self, opc, VMS->typeBool );
			AssertTypeIdMatching( at, DAO_BOOLEAN );
			AssertTypeIdMatching( bt, DAO_BOOLEAN );
			AssertTypeIdMatching( types[opc], DAO_BOOLEAN );
			break;
		case DVM_AND_BII : case DVM_OR_BII : case DVM_LT_BII :
		case DVM_LE_BII  : case DVM_EQ_BII : case DVM_NE_BII :
			DaoInferencer_UpdateVarType( self, opc, VMS->typeBool );
			AssertTypeIdMatching( at, DAO_INTEGER );
			AssertTypeIdMatching( bt, DAO_INTEGER );
			AssertTypeIdMatching( types[opc], DAO_BOOLEAN );
			break;
		case DVM_ADD_III : case DVM_SUB_III : case DVM_MUL_III :
		case DVM_DIV_III : case DVM_MOD_III : case DVM_POW_III :
		case DVM_BITAND_III  : case DVM_BITOR_III  : case DVM_BITXOR_III :
		case DVM_BITLFT_III  : case DVM_BITRIT_III  :
			DaoInferencer_UpdateVarType( self, opc, VMS->typeInt );
			AssertTypeIdMatching( at, DAO_INTEGER );
			AssertTypeIdMatching( bt, DAO_INTEGER );
			AssertTypeIdMatching( types[opc], DAO_INTEGER );
			break;
		case DVM_ADD_FFF : case DVM_SUB_FFF : case DVM_MUL_FFF :
		case DVM_DIV_FFF : case DVM_MOD_FFF : case DVM_POW_FFF :
		case DVM_AND_BFF : case DVM_OR_BFF : case DVM_LT_BFF  :
		case DVM_LE_BFF  : case DVM_EQ_BFF : case DVM_NE_BFF :
			ct = (code < DVM_AND_BFF) ? VMS->typeFloat : VMS->typeBool;
			DaoInferencer_UpdateVarType( self, opc, ct );
			AssertTypeIdMatching( at, DAO_FLOAT );
			AssertTypeIdMatching( bt, DAO_FLOAT );
			AssertTypeIdMatching( types[opc], ct->tid );
			break;
		case DVM_ADD_CCC : case DVM_SUB_CCC : case DVM_MUL_CCC : case DVM_DIV_CCC :
		case DVM_EQ_BCC : case DVM_NE_BCC :
			ct = code < DVM_EQ_BCC ? VMS->typeComplex : VMS->typeBool;
			DaoInferencer_UpdateVarType( self, opc, ct );
			AssertTypeIdMatching( at, DAO_COMPLEX );
			AssertTypeIdMatching( bt, DAO_COMPLEX );
			AssertTypeIdMatching( types[opc], ct->tid );
			break;
		case DVM_ADD_SSS : case DVM_LT_BSS : case DVM_LE_BSS :
		case DVM_EQ_BSS : case DVM_NE_BSS :
			ct = code == DVM_ADD_SSS ? VMS->typeString : VMS->typeBool;
			DaoInferencer_UpdateVarType( self, opc, ct );
			AssertTypeIdMatching( at, DAO_STRING );
			AssertTypeIdMatching( bt, DAO_STRING );
			AssertTypeIdMatching( types[opc], ct->tid );
			break;
		case DVM_GETI_SI :
			AssertTypeIdMatching( at, DAO_STRING );
			if( code == DVM_GETI_SI && bt->tid != DAO_INTEGER ) goto NotMatch;
			DaoInferencer_UpdateType( self, opc, VMS->typeInt );
			AssertTypeIdMatching( types[opc], DAO_INTEGER );
			break;
		case DVM_SETI_SII :
			AssertTypeIdMatching( at, DAO_INTEGER );
			AssertTypeIdMatching( bt, DAO_INTEGER );
			AssertTypeIdMatching( ct, DAO_STRING );
			break;
		case DVM_GETI_LI :
			AssertTypeIdMatching( at, DAO_LIST );
			AssertTypeIdMatching( bt, DAO_INTEGER );
			at = types[opa]->args->items.pType[0];
			if( at->tid < DAO_ARRAY || at->tid >= DAO_ANY ) goto NotMatch;
			DaoInferencer_UpdateType( self, opc, at );
			AssertTypeMatching( at, types[opc], defs );
			break;
		case DVM_GETI_LBI : case DVM_GETI_LII : case DVM_GETI_LFI : case DVM_GETI_LCI :
		case DVM_GETI_ABI : case DVM_GETI_AII : case DVM_GETI_AFI : case DVM_GETI_ACI :
		case DVM_GETI_LSI :
			TT1 = TT3 = 0;
			if( code >= DVM_GETI_ABI ){
				TT3 = DAO_ARRAY;
				TT1 = DAO_BOOLEAN + (code - DVM_GETI_ABI);
			}else if( code != DVM_GETI_LSI ){
				TT3 = DAO_LIST;
				TT1 = DAO_BOOLEAN + (code - DVM_GETI_LBI);
			}else{
				TT3 = DAO_LIST;
				TT1 = DAO_STRING;
			}
			if( at->tid != TT3 || at->args->size ==0 ) goto NotMatch;
			at = at->args->items.pType[0];
			if( at == NULL || at->tid != TT1 ) goto NotMatch;
			if( bt == NULL || bt->tid != DAO_INTEGER ) goto NotMatch;
			DaoInferencer_UpdateType( self, opc, at );
			AssertTypeIdMatching( types[opc], TT1 );
			break;
		case DVM_GETMI_ABI : case DVM_GETMI_AII :
		case DVM_GETMI_AFI : case DVM_GETMI_ACI :
			for(j=0; j<opb; j++){
				bt = types[opa + j + 1];
				if( bt->tid == DAO_NONE || bt->tid > DAO_FLOAT ) goto InvalidIndex;
			}
			at = at->args->items.pType[0];
			DaoInferencer_UpdateType( self, opc, at );
			AssertTypeMatching( at, types[opc], defs );
			break;
		case DVM_SETI_LI :
			AssertTypeIdMatching( bt, DAO_INTEGER );
			AssertTypeIdMatching( ct, DAO_LIST );
			ct = types[opc]->args->items.pType[0];
			if( at != ct && ct->tid != DAO_ANY ) goto NotMatch;
			break;
		case DVM_SETI_LBIB : case DVM_SETI_LIII : case DVM_SETI_LFIF : case DVM_SETI_LCIC :
		case DVM_SETI_ABIB : case DVM_SETI_AIII : case DVM_SETI_AFIF : case DVM_SETI_ACIC :
		case DVM_SETI_LSIS :
			TT2 = DAO_INTEGER;
			TT1 = TT6 = 0;
			if( code >= DVM_SETI_ABIB ){
				TT6 = DAO_ARRAY;
				TT1 = DAO_BOOLEAN + code - DVM_SETI_ABIB;
			}else if( code != DVM_SETI_LSIS ){
				TT6 = DAO_LIST;
				TT1 = DAO_BOOLEAN + code - DVM_SETI_LBIB;
			}else{
				TT6 = DAO_LIST;
				TT1 = DAO_STRING;
			}
			if( ct->tid != TT6 || bt->tid != TT2 || at->tid != TT1 ) goto NotMatch;
			if( ct->args->size !=1 || ct->args->items.pType[0]->tid != TT1 ) goto NotMatch;
			break;
		case DVM_SETMI_ABIB : case DVM_SETMI_AIII :
		case DVM_SETMI_AFIF : case DVM_SETMI_ACIC :
			for(j=0; j<opb; j++){
				bt = types[opc + j + 1];
				if( bt->tid == DAO_NONE || bt->tid > DAO_FLOAT ) goto InvalidIndex;
			}
			if( at->tid != DAO_BOOLEAN + (code - DVM_SETMI_ABIB) ) goto NotMatch;
			if( at->tid == DAO_NONE || at->tid > DAO_COMPLEX ) goto NotMatch;
			if( ct->tid != DAO_ARRAY || ct->args->items.pType[0]->tid != at->tid ) goto NotMatch;
			break;
		case DVM_GETI_TI :
			if( at->tid != DAO_TUPLE || bt->tid != DAO_INTEGER ) goto NotMatch;
			self->array->size = 0;
			DaoType_ExportArguments( at, self->array, 1 );
			ct = DaoNamespace_MakeType( NS, "", DAO_VARIANT, NULL, self->array->items.pType, self->array->size );
			DaoInferencer_UpdateType( self, opc, ct );
			break;
		case DVM_SETI_TI :
			if( ct->tid != DAO_TUPLE || bt->tid != DAO_INTEGER ) goto NotMatch;
			break;
		case DVM_SETF_TPP :
		case DVM_SETF_TXX :
			if( at ==NULL || ct ==NULL || ct->tid != DAO_TUPLE ) goto NotMatch;
			if( opb >= ct->args->size ) goto InvalidIndex;
			tt = ct->args->items.pType[opb];
			if( tt->tid == DAO_PAR_NAMED ) tt = & tt->aux->xType;
			if( tt && tt->invar ) goto ModifyConstant;
			if( at != tt && tt->tid != DAO_ANY ) goto NotMatch;
			if( code == DVM_SETF_TPP && consts[opa] ) goto InvalidOper;
			break;
		case DVM_GETF_TB :
		case DVM_GETF_TI : case DVM_GETF_TF :
		case DVM_GETF_TC : case DVM_GETF_TX :
			if( at ==NULL || at->tid != DAO_TUPLE ) goto NotMatch;
			if( opb >= at->args->size ) goto InvalidIndex;
			ct = at->args->items.pType[opb];
			if( ct->tid == DAO_PAR_NAMED ) ct = & ct->aux->xType;
			DaoInferencer_UpdateType( self, opc, ct );
			if( code != DVM_GETF_TX ){
				TT3 = DAO_BOOLEAN + (code - DVM_GETF_TB);
				if( ct == NULL || ct->tid != TT3 ) goto NotMatch;
				if( types[opc]->tid != TT3 ) goto NotMatch;
			}else{
				AssertTypeMatching( ct, types[opc], defs );
			}
			break;
		case DVM_SETF_TBB : case DVM_SETF_TII : case DVM_SETF_TFF :
		case DVM_SETF_TCC : case DVM_SETF_TSS :
			if( at ==NULL || ct ==NULL ) goto NotMatch;
			TT1 = 0;
			if( code == DVM_SETF_TSS ){
				TT1 = DAO_STRING;
			}else{
				TT1 = DAO_BOOLEAN + (code - DVM_SETF_TBB);
			}
			if( ct->tid != DAO_TUPLE || at->tid != TT1 ) goto NotMatch;
			if( opb >= ct->args->size ) goto InvalidIndex;
			tt = ct->args->items.pType[opb];
			if( tt->tid == DAO_PAR_NAMED ) tt = & tt->aux->xType;
			if( tt && tt->invar ) goto ModifyConstant;
			if( tt->tid != TT1 ) goto NotMatch;
			break;
		case DVM_GETF_CX :
			if( at->tid != DAO_COMPLEX ) goto NotMatch;
			ct = DaoInferencer_UpdateType( self, opc, VMS->typeFloat );
			if( ct->tid != DAO_FLOAT ) goto NotMatch;
			break;
		case DVM_SETF_CX :
			if( at->tid != DAO_FLOAT ) goto NotMatch;
			if( ct->tid != DAO_COMPLEX ) goto NotMatch;
			break;
		case DVM_GETF_KCB : case DVM_GETF_KCI : case DVM_GETF_KCF :
		case DVM_GETF_KCC :
		case DVM_GETF_KC :
			if( types[opa]->tid != DAO_CLASS ) goto NotMatch;
			klass = & types[opa]->aux->xClass;
			if( opb >= klass->constants->size ) goto InvalidIndex;
			ct = DaoNamespace_GetType( NS, klass->constants->items.pConst[ opb ]->value );
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			if( code == DVM_GETF_KC ) break;
			if( ct->tid != (DAO_BOOLEAN + code - DVM_GETF_KCB) ) goto NotMatch;
			break;
		case DVM_GETF_KGB : case DVM_GETF_KGI : case DVM_GETF_KGF :
		case DVM_GETF_KGC :
		case DVM_GETF_KG :
			if( types[opa]->tid != DAO_CLASS ) goto NotMatch;
			klass = & types[opa]->aux->xClass;
			if( opb >= klass->variables->size ) goto InvalidIndex;
			ct = klass->variables->items.pVar[ opb ]->dtype;
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			if( code == DVM_GETF_KG ) break;
			if( ct->tid != (DAO_BOOLEAN + code - DVM_GETF_KGB) ) goto NotMatch;
			break;
		case DVM_GETF_OCB : case DVM_GETF_OCI : case DVM_GETF_OCF :
		case DVM_GETF_OCC :
		case DVM_GETF_OC :
			if( types[opa]->tid != DAO_OBJECT ) goto NotMatch;
			klass = & types[opa]->aux->xClass;
			if( opb >= klass->constants->size ) goto InvalidIndex;
			ct = DaoNamespace_GetType( NS, klass->constants->items.pConst[ opb ]->value );
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			if( code == DVM_GETF_OC ){
				value = klass->constants->items.pConst[opb]->value;
				GC_Assign( & consts[opc], value );
				break;
			}
			if( ct->tid != (DAO_BOOLEAN + code - DVM_GETF_OCB) ) goto NotMatch;
			break;
		case DVM_GETF_OGB : case DVM_GETF_OGI : case DVM_GETF_OGF :
		case DVM_GETF_OGC :
		case DVM_GETF_OG :
			if( types[opa]->tid != DAO_OBJECT ) goto NotMatch;
			klass = & types[opa]->aux->xClass;
			if( opb >= klass->variables->size ) goto InvalidIndex;
			ct = klass->variables->items.pVar[ opb ]->dtype;
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			if( code == DVM_GETF_OG ) break;
			if( ct->tid != (DAO_BOOLEAN + code - DVM_GETF_OGB) ) goto NotMatch;
			break;
		case DVM_GETF_OVB : case DVM_GETF_OVI : case DVM_GETF_OVF :
		case DVM_GETF_OVC :
		case DVM_GETF_OV :
			if( types[opa]->tid != DAO_OBJECT ) goto NotMatch;
			klass = & types[opa]->aux->xClass;
			if( opb >= klass->instvars->size ) goto InvalidIndex;
			ct = klass->instvars->items.pVar[ opb ]->dtype;
			DaoInferencer_UpdateType( self, opc, ct );
			AssertTypeMatching( ct, types[opc], defs );
			if( code == DVM_GETF_OV ) break;
			if( ct->tid != (DAO_BOOLEAN + code - DVM_GETF_OVB) ) goto NotMatch;
			break;
		case DVM_SETF_KGBB : case DVM_SETF_KGII : case DVM_SETF_KGFF :
		case DVM_SETF_KGCC :
		case DVM_SETF_KG :
			if( ct == NULL ) goto ErrorTyping;
			if( types[opa] ==NULL || types[opc] ==NULL ) goto NotMatch;
			if( ct->tid != DAO_CLASS ) goto NotMatch;
			if( opb >= ct->aux->xClass.variables->size ) goto InvalidIndex;
			ct = ct->aux->xClass.variables->items.pVar[ opb ]->dtype;
			if( ct && ct->invar && !(routine->attribs & DAO_ROUT_INITOR) ) goto ModifyConstant;
			if( code == DVM_SETF_KG ){
				if( at != ct && ct->tid != DAO_ANY ) goto NotMatch;
				break;
			}
			k = DAO_FLOAT - DAO_BOOLEAN + 1;
			AssertTypeMatching( at, ct, defs );
			if( at->tid != (DAO_BOOLEAN + (code - DVM_SETF_KGBB)%k) ) goto NotMatch;
			if( ct->tid != (DAO_BOOLEAN + (code - DVM_SETF_KGBB)/k) ) goto NotMatch;
			break;
		case DVM_SETF_OGBB : case DVM_SETF_OGII : case DVM_SETF_OGFF :
		case DVM_SETF_OGCC :
		case DVM_SETF_OG :
			if( ct == NULL ) goto ErrorTyping;
			if( types[opa] ==NULL || types[opc] ==NULL ) goto NotMatch;
			if( ct->tid != DAO_OBJECT ) goto NotMatch;
			if( opb >= ct->aux->xClass.variables->size ) goto InvalidIndex;
			ct = ct->aux->xClass.variables->items.pVar[ opb ]->dtype;
			if( ct && ct->invar && !(routine->attribs & DAO_ROUT_INITOR) ) goto ModifyConstant;
			if( code == DVM_SETF_OG ){
				if( at != ct && ct->tid != DAO_ANY ) goto NotMatch;
				break;
			}
			if( at->tid != ct->tid ) goto NotMatch;
			if( at->tid != (DAO_BOOLEAN + (code - DVM_SETF_OGBB)) ) goto NotMatch;
			break;
		case DVM_SETF_OVBB : case DVM_SETF_OVII : case DVM_SETF_OVFF :
		case DVM_SETF_OVCC :
		case DVM_SETF_OV :
			if( ct == NULL ) goto ErrorTyping;
			if( types[opa] ==NULL || types[opc] ==NULL ) goto NotMatch;
			if( ct->tid != DAO_OBJECT ) goto NotMatch;
			if( opb >= ct->aux->xClass.instvars->size ) goto InvalidIndex;
			ct = ct->aux->xClass.instvars->items.pVar[ opb ]->dtype;
			if( ct && ct->invar && !(routine->attribs & DAO_ROUT_INITOR) ) goto ModifyConstant;
			if( code == DVM_SETF_OV ){
				if( ct->tid == DAO_ANY ) break;
				if( DaoType_MatchTo( at, ct, NULL ) != DAO_MT_EQ ) goto NotMatch;
				/* Same type may be represented by different type objects by different namespaces; */
				/* if( at != ct && ct->tid != DAO_ANY ) goto NotMatch; */
				break;
			}
			if( at->tid != ct->tid ) goto NotMatch;
			if( at->tid != (DAO_BOOLEAN + (code - DVM_SETF_OVBB)) ) goto NotMatch;
			break;
		case DVM_MATH_B :
		case DVM_MATH_I :
		case DVM_MATH_F :
			TT1 = DAO_BOOLEAN + (code - DVM_MATH_B);
			type = self->basicTypes[TT1];
			if( opa <= DVM_MATH_ABS ){
				ct = DaoInferencer_UpdateType( self, opc, type );
				if( bt->tid != TT1 || ct->tid != TT1 ) goto NotMatch;
			}else if( bt->tid >= DAO_BOOLEAN && bt->tid <= DAO_FLOAT ){
				ct = DaoInferencer_UpdateType( self, opc, VMS->typeFloat );
				if( ct->tid != DAO_FLOAT ) goto NotMatch;
			}else{
				if( bt->tid == DAO_NONE || bt->tid > DAO_FLOAT ) goto NotMatch;
				DaoInferencer_UpdateType( self, opc, VMS->typeFloat );
				if( ct->tid != DAO_FLOAT ) goto NotMatch;
			}
			break;
		default : break;
		}
		if( self->inodes->size != N ){
			i--;
			continue;
		}
	}
	while( nestedRoutIndex < self->routines->size ){
		DaoRoutine *rout = self->routines->items.pRoutine[nestedRoutIndex];
		nestedRoutIndex += 1;
		if( DaoRoutine_DoTypeInference( rout, self->silent ) == 0 ) return 0;
	}

	for(i=0; i<self->defers->size; ++i){
		DaoRoutine *closure = self->defers->items.pRoutine[i];
		DaoType *retype = (DaoType*) routine->routType->aux;
		DaoType *type = closure->routType;
		type = DaoNamespace_MakeRoutType( NS, type, NULL, type->args->items.pType, retype );
		GC_Assign( & closure->routType, type );
		if( DaoRoutine_DoTypeInference( closure, self->silent ) == 0 ) return 0;
	}

	DaoInferencer_Finalize( self );
	return 1;
NotMatch: return DaoInferencer_ErrorTypeNotMatching( self, NULL, NULL );
NotInit: return DaoInferencer_ErrorNotInitialized( self, 0, 0, 0 );
NotPermit: return DaoInferencer_Error( self, DTE_FIELD_NOT_PERMIT );
NotExist: return DaoInferencer_Error( self, DTE_FIELD_NOT_EXIST );
NeedInstVar: return DaoInferencer_Error( self, DTE_FIELD_OF_INSTANCE );
ModifyConstant: return DaoInferencer_Error( self, DTE_CONST_WRONG_MODIFYING );
InvalidEnum: return DaoInferencer_Error( self, DTE_INVALID_ENUMERATION );
InvalidIndex: return DaoInferencer_Error( self, DTE_INDEX_NOT_VALID );
InvalidOper: return DaoInferencer_Error( self, DTE_OPERATION_NOT_VALID );
InvalidCasting: return DaoInferencer_Error( self, DTE_INVALID_INVAR_CAST );
InvalidParam: return DaoInferencer_Error( self, DTE_PARAM_ERROR );
ErrorTyping: return DaoInferencer_Error( self, DTE_TYPE_NOT_MATCHING );
}
static void DaoRoutine_ReduceLocalConsts( DaoRoutine *self )
{
	DaoList *list = DaoList_New();
	DMap *used = DMap_New(0,0);
	DNode *it;
	daoint i;
	for(i=0; i<self->routType->args->size; ++i){
		DMap_Insert( used, IntToPointer(i), IntToPointer(i) );
	}
	for(i=0; i<self->routConsts->value->size; ++i){
		/* For reserved space in the constant list: */
		if( self->routConsts->value->items.pValue[i] == NULL ){
			DMap_Insert( used, IntToPointer(i), IntToPointer(i) );
		}
	}
	for(i=0; i<self->body->annotCodes->size; ++i){
		DaoVmCodeX *vmc = self->body->annotCodes->items.pVmc[i];
		DaoVmCode *vmc2 = self->body->vmCodes->data.codes + i;
		int id = used->size;
		switch( vmc->code ){
		case DVM_GETCL :
		case DVM_GETCL_I : case DVM_GETCL_F :
		case DVM_GETCL_C :
		case DVM_GETF : case DVM_SETF :
		case DVM_CAST :
		case DVM_CAST_I : case DVM_CAST_F :
		case DVM_CAST_C : case DVM_CAST_S : case DVM_CAST_VE :
		case DVM_CAST_VX :
			it = DMap_Find( used, IntToPointer(vmc->b) );
			if( it == NULL ) it = DMap_Insert( used, IntToPointer(vmc->b), IntToPointer(id) );
			vmc->b = vmc2->b = it->value.pInt;
			break;
		case DVM_NAMEVA :
		case DVM_CASE :
			it = DMap_Find( used, IntToPointer(vmc->a) );
			if( it == NULL ) it = DMap_Insert( used, IntToPointer(vmc->a), IntToPointer(id) );
			vmc->a = vmc2->a = it->value.pInt;
			break;
		}
	}
	DList_Resize( list->value, used->size, NULL );
	for(it=DMap_First(used); it; it=DMap_Next(used,it)){
		DaoValue **src = self->routConsts->value->items.pValue + it->key.pInt;
		DaoValue **dest = list->value->items.pValue + it->value.pInt;
		DaoValue_Copy( src[0], dest );
		DaoValue_MarkConst( dest[0] );
	}
	GC_Assign( & self->routConsts, list );
	DMap_Delete( used );
}

#define AssertInitialized( reg, ec, first, last ) { \
	if( DaoCnode_FindResult( node, IntToPointer(reg) ) < 0 ) \
		return DaoInferencer_ErrorNotInitialized( self, ec, first, last ); }

static int DaoInferencer_CheckInitialization( DaoInferencer *self, DaoOptimizer *optimizer )
{
	DaoClass *klass;
	DaoRoutine *routine = self->routine;
	DaoStream  *stream = routine->nameSpace->vmSpace->errorStream;
	DList *annotCodes = routine->body->annotCodes;
	DaoVmCodeX **codes = annotCodes->items.pVmc;
	DaoCnode **nodes;
	char char50[50];
	int i, j, J, ret = 1;

	DaoOptimizer_DoVIA( optimizer, routine );

	nodes = optimizer->nodes->items.pCnode;
	for(i=0; i<annotCodes->size; i++){
		DaoCnode *node = nodes[i];
		DaoVmCodeX *vmc = codes[i];
		int first = vmc->first;
		int middle = first + vmc->middle;
		int last = middle + vmc->last;

		self->currentIndex = i;

#if 0
		DString *mbs = self->mbstring;
		DaoLexer_AnnotateCode( routine->body->source, *vmc, mbs, 24 );
		printf( "%4i: ", i );DaoVmCodeX_Print( *vmc, mbs->chars, NULL );
#endif

		switch( DaoVmCode_GetOpcodeType( (DaoVmCode*) vmc ) ){
		case DAO_CODE_GETU :
			if( vmc->a != 0 ) AssertInitialized( vmc->b, 0, middle, middle );
			break;
		case DAO_CODE_UNARY2 :
			AssertInitialized( vmc->b, 0, middle, middle );
			break;
		case DAO_CODE_GETF :
		case DAO_CODE_BRANCH :
			AssertInitialized( vmc->a, 0, first, first );
			break;
		case DAO_CODE_SETG :
		case DAO_CODE_SETU :
			AssertInitialized( vmc->a, 0, first, first );
			break;
		case DAO_CODE_MOVE :
		case DAO_CODE_UNARY :
			AssertInitialized( vmc->a, 0, first, last );
			break;
		case DAO_CODE_SETF :
			AssertInitialized( vmc->a, 0, last+1, last+1 );
			AssertInitialized( vmc->c, 0, first, first );
			break;
		case DAO_CODE_GETI :
			AssertInitialized( vmc->a, DTE_ITEM_WRONG_ACCESS, first, first );
			AssertInitialized( vmc->b, DTE_ITEM_WRONG_ACCESS, middle, middle );
			break;
		case DAO_CODE_BINARY :
			AssertInitialized( vmc->a, 0, first, first );
			AssertInitialized( vmc->b, 0, middle, middle );
			break;
		case DAO_CODE_BINARY2 :
			AssertInitialized( vmc->b, 0, first, first );
			AssertInitialized( vmc->b + 1, 0, middle, middle );
			break;
		case DAO_CODE_SETI :
			AssertInitialized( vmc->c, DTE_ITEM_WRONG_ACCESS, first, first );
			AssertInitialized( vmc->b, DTE_ITEM_WRONG_ACCESS, middle, middle );
			AssertInitialized( vmc->a, DTE_ITEM_WRONG_ACCESS, last+1, last+1 );
			break;
		case DAO_CODE_GETM :
		case DAO_CODE_ENUM2 :
		case DAO_CODE_ROUTINE :
			for(j=0; j<=vmc->b; ++j){
				AssertInitialized( vmc->a+j, 0, first, first );
			}
			break;
		case DAO_CODE_SETM :
			AssertInitialized( vmc->c, DTE_ITEM_WRONG_ACCESS, first, first );
			for(j=0; j<=vmc->b; ++j) AssertInitialized( vmc->c+j, 0, first, first );
			break;
		case DAO_CODE_MATRIX :
			J=(vmc->b>>8)*(vmc->b&0xff);
			for(j=0; j<J; ++j) AssertInitialized( vmc->a+j, 0, first, first );
			break;
		case DAO_CODE_CALL :
			for(j=0, J=vmc->b&0xff; j<=J; ++j){
				AssertInitialized( vmc->a+j, 0, middle, last );
			}
			break;
		case DAO_CODE_ENUM :
			for(j=0, J=vmc->b&(0xffff>>2); j<J; ++j){
				AssertInitialized( vmc->a+j, 0, first, first );
			}
			break;
		case DAO_CODE_EXPLIST :
			for(j=0; j<vmc->b; ++j) AssertInitialized( vmc->a+j, 0, first, first );
			break;
		case DAO_CODE_YIELD :
			for(j=0; j<(vmc->b&0xff); ++j) AssertInitialized( vmc->a+j, 0, first, first );
			break;
		}
	}

	if( !(routine->attribs & DAO_ROUT_INITOR) ) return 1;
	if( routine->attribs & DAO_ROUT_MIXIN ) return 1;  /* Alread checked; */

	klass = (DaoClass*) routine->routHost->aux;
	for(i=0; i<optimizer->nodes->size; i++){
		DaoCnode *node = nodes[i];
		DaoVmCodeX *vmc = codes[i];
		int first = vmc->first;
		int middle = first + vmc->middle;
		int last = middle + vmc->last;

		if( vmc->code != DVM_RETURN ) continue;
		for(j=klass->objParentEnd; j<klass->instvars->size; ++j){
			DaoVariable *var = klass->instvars->items.pVar[j];
			int key = (DAO_OBJECT_VARIABLE<<16) | j;
			if( var->dtype && var->dtype->tid <= DAO_STRING ) continue;
			if( var->value && DaoType_MatchValue( var->dtype, var->value, NULL ) ) continue;
			if( DaoCnode_FindResult( node, IntToPointer(key) ) < 0 ){
				sprintf( char50, "  At line %i : ", routine->defLine );
				DaoInferencer_WriteErrorHeader2( self );
				DaoStream_WriteChars( stream, char50 );
				DaoStream_WriteChars( stream, "Class instance field \"" );
				DaoStream_WriteString( stream, klass->objDataName->items.pString[j] );
				DaoStream_WriteChars( stream, "\" not initialized!\n" );
				ret = 0;
			}
		}
	}
	return ret;
}

int DaoRoutine_DoTypeInference( DaoRoutine *self, int silent )
{
	DaoInferencer *inferencer;
	DaoOptimizer *optimizer;
	DaoVmSpace *vmspace = self->nameSpace->vmSpace;
	int notide = ! (vmspace->options & DAO_OPTION_IDE);
	int retc;

	DaoRoutine_ReduceLocalConsts( self );

	if( self->body->vmCodes->size == 0 ) return 1;

	optimizer = DaoVmSpace_AcquireOptimizer( vmspace );
	DList_Resize( self->body->regType, self->body->regCount, NULL );
	DaoOptimizer_RemoveUnreachableCodes( optimizer, self );

	inferencer = DaoVmSpace_AcquireInferencer( vmspace );
	DaoInferencer_Init( inferencer, self, silent );
	retc  = DaoInferencer_CheckInitialization( inferencer, optimizer );
	retc &= DaoInferencer_DoInference( inferencer );
	DaoVmSpace_ReleaseInferencer( vmspace, inferencer );

	if( retc ) DaoOptimizer_Optimize( optimizer, self );
	/* Maybe more unreachable code after inference and optimization: */
	DaoOptimizer_RemoveUnreachableCodes( optimizer, self );

	if( retc && notide && daoConfig.jit && dao_jit.Compile ){
		/* LLVMContext provides no locking guarantees: */
		DMutex_Lock( & mutex_routine_specialize );
		dao_jit.Compile( self, optimizer );
		DMutex_Unlock( & mutex_routine_specialize );
	}

	/* DaoRoutine_PrintCode( self, self->nameSpace->vmSpace->errorStream ); */
	DaoVmSpace_ReleaseOptimizer( vmspace, optimizer );
	return retc;
}

