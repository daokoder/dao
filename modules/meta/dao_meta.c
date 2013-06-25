/*
// Dao Standard Modules
// http://www.daovm.net
//
// Copyright (c) 2011,2012, Limin Fu
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

#include<stdlib.h>
#include<string.h>
#include"daoString.h"
#include"daoValue.h"
#include"daoParser.h"
#include"daoNamespace.h"
#include"daoGC.h"


#if 0

int DaoClass_CopyField( DaoClass *self, DaoClass *other, DMap *deftypes )
{
	DaoNamespace *ns = other->classRoutine->nameSpace;
	DaoType *tp;
	DArray *offsets = DArray_New(0);
	DArray *routines = DArray_New(0);
	DNode *it;
	int i, k, st, up, id;

	for(i=0; i<other->superClass->size; i++){
		DaoValue *sup = other->superClass->items.pValue[i];
		if( sup->type == DAO_CLASS && sup->xClass.typeHolders ){
			tp = DaoType_DefineTypes( sup->xClass.objType, ns, deftypes );
			DArray_Append( self->superClass, tp->aux );
		}else if( sup->type == DAO_CTYPE && sup->xCtype.ctype->typer->core->kernel->sptree ){
			tp = DaoType_DefineTypes( sup->xCtype.ctype, ns, deftypes );
			DArray_Append( self->superClass, tp->aux );
		}else{
			DArray_Append( self->superClass, sup );
		}
	}

	DaoRoutine_CopyFields( self->classRoutine, other->classRoutine, 1, 1 );
	for(it=DMap_First(other->lookupTable);it;it=DMap_Next(other->lookupTable,it)){
		st = LOOKUP_ST( it->value.pInt );
		up = LOOKUP_UP( it->value.pInt );
		id = LOOKUP_ID( it->value.pInt );
		if( up ==0 ){
			if( st == DAO_CLASS_CONSTANT && id <self->constants->size ) continue;
			if( st == DAO_CLASS_VARIABLE && id <self->variables->size ) continue;
			if( st == DAO_OBJECT_VARIABLE && id <self->instvars->size ) continue;
		}
		DMap_Insert( self->lookupTable, it->key.pVoid, it->value.pVoid );
	}
	for(i=self->objDataName->size; i<other->objDataName->size; i++)
		DArray_Append( self->objDataName, other->objDataName->items.pString[i] );
	for(i=self->cstDataName->size; i<other->cstDataName->size; i++)
		DArray_Append( self->cstDataName, other->cstDataName->items.pString[i] );
	for(i=self->glbDataName->size; i<other->glbDataName->size; i++)
		DArray_Append( self->glbDataName, other->glbDataName->items.pString[i] );
	for(i=self->variables->size; i<other->variables->size; i++){
		DaoVariable *var = other->variables->items.pVar[i];
		var = DaoVariable_New( var->value, DaoType_DefineTypes( var->dtype, ns, deftypes ) );
		DArray_Append( self->variables, var );
	}
	for(i=self->instvars->size; i<other->instvars->size; i++){
		DaoVariable *var = other->instvars->items.pVar[i];
		var = DaoVariable_New( var->value, DaoType_DefineTypes( var->dtype, ns, deftypes ) );
		DArray_Append( self->instvars, var );
		/* TODO fail checking */
	}
	for(i=self->constants->size; i<other->constants->size; i++){
		DaoValue *value = other->constants->items.pConst[i]->value;
		DaoRoutine *rout = & value->xRoutine;
		if( value->type != DAO_ROUTINE || rout->routHost != other->objType ){
			DArray_Append( self->constants, DaoConstant_New( value ) );
			DaoValue_Update( & self->constants->items.pConst[i]->value, ns, deftypes );
			continue;
		}
		if( rout->overloads == NULL ){
			DString *name = rout->routName;
			rout = DaoRoutine_Copy( rout, 1, 1 );
			value = (DaoValue*) rout;
			k = DaoRoutine_Finalize( rout, self->objType, deftypes );
#if 0
			printf( "%i %p:  %s  %s\n", i, rout, rout->routName->mbs, rout->routType->name->mbs );
#endif
			if( rout->attribs & DAO_ROUT_INITOR ){
				DRoutines_Add( self->classRoutines->overloads, rout );
			}else if( (it = DMap_Find( other->lookupTable, name )) ){
				st = LOOKUP_ST( it->value.pInt );
				up = LOOKUP_UP( it->value.pInt );
				id = LOOKUP_ID( it->value.pInt );
				if( st == DAO_CLASS_CONSTANT && up ==0 && id < i ){
					DaoValue *v = self->constants->items.pConst[id]->value;
					if( v->type == DAO_ROUTINE && v->xRoutine.overloads )
						DRoutines_Add( v->xRoutine.overloads, rout );
				}
			}
			DArray_Append( self->constants, DaoConstant_New( value ) );
			DArray_Append( routines, rout );
			if( k == 0 ) goto Failed;
			continue;
		}else{
			/* No need to added the overloaded routines now; */
			/* Each of them has an entry in constants, and will be handled later: */
			DaoRoutine *routs = DaoRoutines_New( ns, self->objType, NULL );
			DArray_Append( self->constants, DaoConstant_New( (DaoValue*) routs ) );
			continue;
		}
		DArray_Append( self->constants, DaoConstant_New( value ) );
		DaoValue_Update( & self->constants->items.pConst[i]->value, ns, deftypes );
	}
	for(i=0; i<routines->size; i++){
		if( DaoRoutine_DoTypeInference( routines->items.pRoutine[i], 0 ) == 0 ) goto Failed;
	}
	DArray_Delete( offsets );
	DArray_Delete( routines );
	DaoRoutine_Finalize( self->classRoutine, self->objType, deftypes );
	return DaoRoutine_DoTypeInference( self->classRoutine, 0 );
Failed:
	DArray_Delete( offsets );
	DArray_Delete( routines );
	return 0;
}
DaoClass* DaoClass_Instantiate( DaoClass *self, DArray *types )
{
	DaoClass *klass = NULL;
	DaoType *type;
	DString *name;
	DNode *node;
	DMap *deftypes;
	daoint lt = DString_FindChar( self->className, '<', 0 );
	daoint i, holders = 0;
	if( self->typeHolders == NULL || self->typeHolders->size ==0 ) return self;
	while( types->size < self->typeHolders->size ){
		type = self->typeDefaults->items.pType[ types->size ];
		if( type == NULL ) type = self->typeHolders->items.pType[ types->size ];
		DArray_Append( types, type );
	}
	name = DString_New(1);
	DString_Append( name, self->className );
	if( lt != MAXSIZE ) DString_Erase( name, lt, MAXSIZE );
	DString_AppendChar( name, '<' );
	for(i=0; i<types->size; i++){
		type = types->items.pType[i];
		holders += type->tid == DAO_THT;
		if( i ) DString_AppendChar( name, ',' );
		DString_Append( name, type->name );
	}
	DString_AppendChar( name, '>' );
	while( self->templateClass ) self = self->templateClass;
	node = DMap_Find( self->instanceClasses, name );
	if( node ){
		klass = node->value.pClass;
	}else{
		deftypes = DMap_New(0,0);
		klass = DaoClass_New();
		if( holders ) klass->templateClass = self;
		DMap_Insert( self->instanceClasses, name, klass );
		DaoClass_AddReference( self, klass ); /* No need for cleanup of klass; */
		DaoClass_SetName( klass, name, self->classRoutine->nameSpace );
		for(i=0; i<types->size; i++){
			type = types->items.pType[i];
			if( DaoType_MatchTo( type, self->typeHolders->items.pType[i], deftypes ) ==0 ){
				DString_Delete( name );
				return NULL;
			}
			MAP_Insert( deftypes, self->typeHolders->items.pVoid[i], type );
		}
		klass->objType->nested = DArray_New(D_VALUE);
		DArray_Assign( klass->objType->nested, types );
		if( DaoClass_CopyField( klass, self, deftypes ) == 0 ){
			DString_Delete( name );
			return NULL;
		}
		DaoClass_DeriveClassData( klass );
		DaoClass_DeriveObjectData( klass );
		DaoClass_ResetAttributes( klass );
		DMap_Delete( deftypes );
		if( holders ){
			klass->typeHolders = DArray_New(0);
			klass->typeDefaults = DArray_New(0);
			klass->instanceClasses = DMap_New(D_STRING,0);
			DMap_Insert( klass->instanceClasses, klass->className, klass );
			for(i=0; i<types->size; i++){
				DArray_Append( klass->typeHolders, types->items.pType[i] );
				DArray_Append( klass->typeDefaults, NULL );
			}
			for(i=0; i<klass->typeHolders->size; i++){
				DaoClass_AddReference( klass, klass->typeHolders->items.pType[i] );
				DaoClass_AddReference( klass, klass->typeDefaults->items.pType[i] );
			}
		}
	}
	DString_Delete( name );
	return klass;
}


/* storage enum<const,global,var> */
static int storages[3] = { DAO_CLASS_CONSTANT, DAO_CLASS_VARIABLE, DAO_OBJECT_VARIABLE };

/* access enum<private,protected,public> */
static int permissions[3] = { DAO_DATA_PRIVATE, DAO_DATA_PROTECTED, DAO_DATA_PUBLIC };

/* a = class( name, parents, fields, methods ){ proto_class_body }
 * (1) parents: optional, list<class> or map<string,class>
 * (2) fields: optional, tuple<name:string,value:any,storage:enum<>,access:enum<>>
 * (3) methods: optional, tuple<name:string,method:routine,access:enum<>>
 * (4) default storage: $var, default access: $public.
 */
void DaoProcess_MakeClass( DaoProcess *self, DaoVmCode *vmc )
{
	DaoType *tp;
	DaoValue **values = self->activeValues;
	DaoRoutine *routine = self->activeRoutine;
	DaoNamespace *ns = self->activeNamespace;
	DaoNamespace *ns2 = self->activeNamespace;
	DaoTuple *tuple = (DaoTuple*) values[vmc->a];
	DaoClass *klass = DaoClass_New();
	DaoClass *proto = NULL;
	DaoList *parents = NULL;
	DaoMap *parents2 = NULL;
	DaoList *fields = NULL;
	DaoList *methods = NULL;
	DString *name = NULL;
	DaoValue **data = tuple->items;
	DMap *keys = tuple->unitype->mapNames;
	DMap *deftypes = DMap_New(0,0);
	DMap *pm_map = DMap_New(D_STRING,0);
	DMap *st_map = DMap_New(D_STRING,0);
	DArray *routines = DArray_New(0);
	DNode *it, *node;
	DaoEnum pmEnum = {DAO_ENUM,0,0,0,0,0,0,NULL};
	DaoEnum stEnum = {DAO_ENUM,0,0,0,0,0,0,NULL};
	int iclass = values[vmc->a+1]->xInteger.value;
	int i, n, st, pm, up, id, size;
	char buf[50];

	pmEnum.etype = dao_access_enum;
	stEnum.etype = dao_storage_enum;

	DaoProcess_SetValue( self, vmc->c, (DaoValue*) klass );
	//printf( "%s\n", tuple->unitype->name->mbs );
	if( iclass && routine->routConsts->items.items.pValue[iclass-1]->type == DAO_CLASS ){
		proto = & routine->routConsts->items.items.pValue[iclass-1]->xClass;
		ns2 = proto->classRoutine->nameSpace;
	}

	/* extract parameters */
	if( tuple->size && data[0]->type == DAO_STRING ) name = data[0]->xString.data;
	if( parents ==NULL && parents2 == NULL && tuple->size >1 ){
		if( data[1]->type == DAO_LIST ) parents = & data[1]->xList;
		if( data[1]->type == DAO_MAP ) parents2 = & data[1]->xMap;
	}
	if( fields ==NULL && tuple->size >2 && data[2]->type == DAO_LIST ) fields = & data[2]->xList;
	if( methods ==NULL && tuple->size >3 && data[3]->type == DAO_LIST ) methods = & data[3]->xList;

	if( name == NULL || name->size ==0 ){
		sprintf( buf, "AnonymousClass%p", klass );
		DString_SetMBS( klass->className, buf );
		DaoClass_SetName( klass, klass->className, ns2 );
	}else{
		DaoClass_SetName( klass, name, ns2 );
	}
	for(i=0; i<routine->parCount; i++){
		DaoType *type = routine->routType->nested->items.pType[i];
		DaoValue *value = self->activeValues[i];
		/* type<@T<int|float>> kind of types may be specialized to type<float>
		 * the type holder is only available from the original routine parameters: */
		if( routine->original ) type = routine->original->routType->nested->items.pType[i];
		if( type->tid == DAO_PAR_NAMED || type->tid == DAO_PAR_DEFAULT ) type = (DaoType*) type->aux;
		if( type->tid != DAO_TYPE ) continue;
		type = type->nested->items.pType[0];
		if( type->tid == DAO_VARIANT && type->aux ) type = (DaoType*) type->aux;
		if( type->tid == DAO_THT && value->type == DAO_TYPE ){
			MAP_Insert( deftypes, type, value );
		}
	}
	tp = DaoNamespace_MakeType( ns, "@class", DAO_THT, NULL,NULL,0 );
	if( tp ) MAP_Insert( deftypes, tp, klass->objType );
	DaoProcess_MapTypes( self, deftypes );

	/* copy data from the proto class */
	if( proto ) DaoClass_CopyField( klass, proto, deftypes );

	/* update class members with running time data */
	for(i=2; i<=vmc->b; i+=3){
		DaoValue *value;
		st = values[vmc->a+i+1]->xInteger.value;
		id = values[vmc->a+i+2]->xInteger.value;
		value = self->activeValues[vmc->a+i];
		if( st == DAO_CLASS_CONSTANT ){
			DaoRoutine *newRout = NULL;
			DaoConstant *dest2 = klass->constants->items.pConst[id];
			DaoValue *dest = dest2->value;
			if( value->type == DAO_ROUTINE && value->xRoutine.routHost == proto->objType ){
				newRout = & value->xRoutine;
				if( DaoRoutine_Finalize( newRout, klass->objType, deftypes ) == 0){
					DaoProcess_RaiseException( self, DAO_ERROR, "method creation failed" );
					continue;
				}
				DArray_Append( routines, newRout );
				if( strcmp( newRout->routName->mbs, "@class" ) ==0 ){
					node = DMap_Find( proto->lookupTable, newRout->routName );
					DString_Assign( newRout->routName, klass->className );
					st = LOOKUP_ST( node->value.pInt );
					up = LOOKUP_UP( node->value.pInt );
					if( st == DAO_CLASS_CONSTANT && up ==0 ){
						id = LOOKUP_ID( node->value.pInt );
						dest2 = klass->constants->items.pConst[id];
					}
					DRoutines_Add( klass->classRoutines->overloads, newRout );
				}
			}
			dest = dest2->value;
			if( dest->type == DAO_ROUTINE ){
				DaoRoutine *rout = & dest->xRoutine;
				if( rout->routHost != klass->objType ) DaoValue_Clear( & dest2->value );
			}
			if( dest->type == DAO_ROUTINE && dest->xRoutine.overloads ){
				DRoutines_Add( dest->xRoutine.overloads, newRout );
			}else{
				DaoValue_Copy( value, & dest2->value );
			}
		}else if( st == DAO_CLASS_VARIABLE ){
			DaoVariable *var = klass->variables->items.pVar[id];
			DaoValue_Move( value, & var->value, var->dtype );
		}else if( st == DAO_OBJECT_VARIABLE ){
			DaoVariable *var = klass->instvars->items.pVar[id];
			DaoValue_Move( value, & var->value, var->dtype );
		}
	}

	/* add parents from parameters */
	if( parents ){
		for(i=0,n=parents->items.size; i<n; i++){
			DaoClass_AddSuperClass( klass, parents->items.items.pValue[i] );
		}
	}else if( parents2 ){
		for(it=DMap_First(parents2->items);it;it=DMap_Next(parents2->items,it)){
			int type = it->value.pValue->type;
			if( it->key.pValue->type == DAO_STRING && (type == DAO_CLASS || type == DAO_CTYPE) ){
				DaoClass_AddSuperClass( klass, it->value.pValue );
			}//XXX error handling
		}
	}
	DaoClass_DeriveClassData( klass );
	if( fields ){ /* add fields from parameters */
		for(i=0,n=fields->items.size; i<n; i++){
			DaoValue *fieldv = fields->items.items.pValue[i];
			DaoType *type = NULL;
			DaoValue *value = NULL;

			if( DaoType_MatchValue( dao_dynclass_field, fieldv, NULL ) == 0) continue;//XXX
			data = fieldv->xTuple.items;
			size = fieldv->xTuple.size;
			st = DAO_OBJECT_VARIABLE;
			pm = DAO_DATA_PUBLIC;

			name = NULL;
			if( size && data[0]->type == DAO_STRING ) name = data[0]->xString.data;
			if( size > 1 && data[1]->type ){
				value = data[1];
				type = fieldv->xTuple.unitype->nested->items.pType[1];
			}
			if( name == NULL || value == NULL ) continue;
			if( MAP_Find( klass->lookupTable, name ) ) continue;

			if( size > 2 && data[2]->type == DAO_ENUM ){
				if( DaoEnum_SetValue( & stEnum, & data[2]->xEnum, NULL ) ==0) goto InvalidField;
				st = storages[ stEnum.value ];
			}
			if( size > 3 && data[3]->type == DAO_ENUM ){
				if( DaoEnum_SetValue( & pmEnum, & data[3]->xEnum, NULL ) ==0) goto InvalidField;
				pm = permissions[ pmEnum.value ];
			}
			/* printf( "%s %i %i\n", name->mbs, st, pm ); */
			switch( st ){
			case DAO_OBJECT_VARIABLE: DaoClass_AddObjectVar( klass, name, value, type, pm ); break;
			case DAO_CLASS_VARIABLE : DaoClass_AddGlobalVar( klass, name, value, type, pm ); break;
			case DAO_CLASS_CONSTANT : DaoClass_AddConst( klass, name, value, pm ); break;
			default : break;
			}
			continue;
InvalidField:
			DaoProcess_RaiseException( self, DAO_ERROR_PARAM, "" );
		}
	}
	if( methods ){ /* add methods from parameters */
		for(i=0,n=methods->items.size; i<n; i++){
			DaoValue *methodv = methods->items.items.pValue[i];
			DaoRoutine *newRout;
			DaoValue *method = NULL;
			DaoValue *dest;

			if( DaoType_MatchValue( dao_dynclass_method, methodv, NULL ) == 0) continue;//XXX
			data = methodv->xTuple.items;
			size = methodv->xTuple.size;
			pm = DAO_DATA_PUBLIC;

			name = NULL;
			if( size && data[0]->type == DAO_STRING ) name = data[0]->xString.data;
			if( size > 1 && data[1]->type == DAO_ROUTINE ) method = data[1];
			if( name == NULL || method == NULL ) continue;
			if( size > 2 && data[2]->type == DAO_ENUM ){
				if( DaoEnum_SetValue( & pmEnum, & data[2]->xEnum, NULL ) ==0) goto InvalidMethod;
				pm = permissions[ pmEnum.value ];
			}

			newRout = & method->xRoutine;
			if( ROUT_HOST_TID( newRout ) !=0 ) continue;
			if( DaoRoutine_Finalize( newRout, klass->objType, deftypes ) == 0){
				DaoProcess_RaiseException( self, DAO_ERROR, "method creation failed" );
				continue; // XXX
			}
			DArray_Append( routines, newRout );
			DString_Assign( newRout->routName, name );
			if( DString_EQ( newRout->routName, klass->className ) ){
				DRoutines_Add( klass->classRoutines->overloads, newRout );
			}

			node = DMap_Find( proto->lookupTable, name );
			if( node == NULL ){
				DaoClass_AddConst( klass, name, method, pm );
				continue;
			}
			if( LOOKUP_UP( node->value.pInt ) ) continue;
			if( LOOKUP_ST( node->value.pInt ) != DAO_CLASS_CONSTANT ) continue;
			id = LOOKUP_ID( node->value.pInt );
			dest = klass->constants->items.pConst[id]->value;
			if( dest->type == DAO_ROUTINE && dest->xRoutine.overloads ){
				DRoutines_Add( dest->xRoutine.overloads, newRout );
			}
			continue;
InvalidMethod:
			DaoProcess_RaiseException( self, DAO_ERROR_PARAM, "" );
		}
	}
	for(i=0,n=routines->size; i<n; i++){
		if( DaoRoutine_DoTypeInference( routines->items.pRoutine[i], 0 ) == 0 ){
			DaoProcess_RaiseException( self, DAO_ERROR, "method creation failed" );
			// XXX
		}
	}
	DaoClass_DeriveObjectData( klass );
	DaoClass_ResetAttributes( klass );
	DArray_Delete( routines );
	DMap_Delete( deftypes );
	DMap_Delete( pm_map );
	DMap_Delete( st_map );
}
#endif



static void META_NS( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *res = proc->activeNamespace;
	if( N == 0 ){
		res = proc->activeNamespace;
	}else if( p[0]->type == DAO_CLASS ){
		res = p[0]->xClass.classRoutine->nameSpace;
	}else if( p[0]->type == DAO_ROUTINE ){
		res = p[0]->xRoutine.nameSpace;
	}
	DaoProcess_PutValue( proc, (DaoValue*) res );
}
static void META_Name( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *str = DaoProcess_PutMBString( proc, "" );
	switch( p[0]->type ){
	case DAO_ROUTINE :
		DString_Assign( str, p[0]->xRoutine.routName );
		break;
	case DAO_CLASS :
		DString_Assign( str, p[0]->xClass.className );
		break;
	case DAO_TYPE :
		DString_Assign( str, p[0]->xType.name );
		break;
	default : break;
	}
}
static void META_Base( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *ls = DaoProcess_PutList( proc );
	int i;
	if( p[0]->type == DAO_CLASS ){
		DaoClass *k = & p[0]->xClass;
		for( i=0; i<k->superClass->size; i++ ){
			DaoList_Append( ls, k->superClass->items.pValue[i] );
		}
	}else if( p[0]->type == DAO_OBJECT ){
		DaoObject *k = & p[0]->xObject;
		for( i=0; i<k->baseCount; i++ ) DaoList_Append( ls, k->parents[i] );
	}
}
static void META_Type( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoType *tp = DaoNamespace_GetType( proc->activeNamespace, p[0] );
	DaoProcess_PutValue( proc, (DaoValue*) tp );
}

static void META_Cst1( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap *map = DaoProcess_PutMap( proc, 0 );
	DaoTuple *tuple;
	DaoClass *klass;
	DaoObject *object;
	DaoType *tp = map->unitype->nested->items.pType[1];
	DaoNamespace *ns, *here = proc->activeNamespace;
	DMap *index = NULL, *lookup = NULL;
	DArray *data;
	DNode *node;
	DaoValue *value;
	DaoValue *vabtp = NULL;
	DaoString name = {DAO_STRING,0,0,0,0,NULL};
	int restri = p[1]->xInteger.value;
	name.data = DString_New(1);
	if( p[0]->type == DAO_CLASS || p[0]->type == DAO_OBJECT ){
		klass = & p[0]->xClass;
		if( p[0]->type == DAO_OBJECT ){
			object = & p[0]->xObject;
			klass = object->defClass;
		}
		lookup = klass->lookupTable;
		index = klass->lookupTable;
		data = klass->constants;
	}else if( p[0]->type == DAO_NAMESPACE ){
		ns = & p[0]->xNamespace;
		//index = ns->cstIndex; XXX
		data = ns->constants;
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
		DString_Delete( name.data );
		return;
	}
	if( index == NULL ) return;
	node = DMap_First( index );
	for( ; node != NULL; node = DMap_Next( index, node ) ){
		size_t id = node->value.pInt;
		if( restri && lookup && LOOKUP_PM( id ) != DAO_DATA_PUBLIC ) continue;
		if( lookup ) id = LOOKUP_ID( id );
		tuple = DaoTuple_New( 2 );
		tuple->unitype = tp;
		GC_IncRC( tp );
		value = data->items.pConst[ id ]->value;
		vabtp = (DaoValue*) DaoNamespace_GetType( here, value );
		DaoValue_Copy( value, tuple->items );
		DaoValue_Copy( vabtp, tuple->items + 1 );
		DString_Assign( name.data, node->key.pString );
		DaoMap_Insert( map, (DaoValue*) & name, (DaoValue*) & tuple );
	}
	DString_Delete( name.data );
}
static void META_Var1( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMap *map = DaoProcess_PutMap( proc, 0 );
	DaoTuple *tuple;
	DaoClass *klass = NULL;
	DaoObject *object = NULL;
	DaoType *tp = map->unitype->nested->items.pType[1];
	DaoNamespace *ns = NULL;
	DMap *index = NULL, *lookup = NULL;
	DNode *node;
	DaoValue *value;
	DaoValue *vabtp = NULL;
	DaoString name = {DAO_STRING,0,0,0,0,NULL};
	int restri = p[1]->xInteger.value;
	name.data = DString_New(1);
	if( p[0]->type == DAO_CLASS || p[0]->type == DAO_OBJECT ){
		klass = & p[0]->xClass;
		if( p[0]->type == DAO_OBJECT ){
			object = & p[0]->xObject;
			klass = object->defClass;
		}
		lookup = klass->lookupTable;
		index = klass->lookupTable;
	}else if( p[0]->type == DAO_NAMESPACE ){
		ns = & p[0]->xNamespace;
		//index = ns->varIndex; XXX
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
		DString_Delete( name.data );
		return;
	}
	if( index == NULL ) return;
	node = DMap_First( index );
	for( ; node != NULL; node = DMap_Next( index, node ) ){
		size_t st = 0, id = node->value.pInt;
		if( restri && lookup && LOOKUP_PM( id ) != DAO_DATA_PUBLIC ) continue;
		if( lookup ){
			st = LOOKUP_ST( id );
			id = LOOKUP_ID( id );
			if( st == DAO_CLASS_CONSTANT ) continue;
		}
		tuple = DaoTuple_New( 2 );
		tuple->unitype = tp;
		GC_IncRC( tp );
		value = NULL;
		if( lookup ){
			if( st == DAO_OBJECT_VARIABLE && object ){
				value = object->objValues[id];
				vabtp = (DaoValue*) klass->instvars->items.pVar[ id ]->dtype;
			}else if( st == DAO_CLASS_VARIABLE ){
				DaoVariable *var = klass->variables->items.pVar[id];
				value = var->value;
				vabtp = (DaoValue*) var->dtype;
			}else if( st == DAO_OBJECT_VARIABLE ){
				vabtp = (DaoValue*) klass->instvars->items.pVar[ id ]->dtype;
			}
		}else{
			DaoVariable *var = ns->variables->items.pVar[id];
			value = var->value;
			vabtp = (DaoValue*) var->dtype;
		}
		DaoValue_Copy( value, tuple->items );
		DaoValue_Copy( vabtp, tuple->items + 1 );
		DString_Assign( name.data, node->key.pString );
		DaoMap_Insert( map, (DaoValue*) & name, (DaoValue*) & tuple );
	}
	DString_Delete( name.data );
}
static void META_Cst2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoTuple *tuple = DaoTuple_New( 2 );
	DaoNamespace *ns = proc->activeNamespace;
	DaoClass *klass = NULL;
	DNode *node;
	DString *name = p[1]->xString.data;
	DaoValue *type = NULL;
	DaoValue **value = NULL;
	if( p[0]->type == DAO_CLASS || p[0]->type == DAO_OBJECT ){
		klass = & p[0]->xClass;
		if( p[0]->type == DAO_OBJECT ) klass = p[0]->xObject.defClass;
		node = DMap_Find( klass->lookupTable, name );
		if( node && LOOKUP_ST( node->value.pInt ) == DAO_CLASS_CONSTANT ){
			value = klass->constants->items.pValue + LOOKUP_ID( node->value.pInt );
			type = (DaoValue*) DaoNamespace_GetType( ns, *value );
		}
	}else if( p[0]->type == DAO_NAMESPACE ){
		DaoNamespace *ns2 = & p[0]->xNamespace;
		return; //XXX
		//node = DMap_Find( ns2->cstIndex, name );
		if( node ){
			value = ns2->constants->items.pValue + node->value.pInt;
			type = (DaoValue*) DaoNamespace_GetType( ns, *value );
		}
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
	}
	DaoValue_Copy( *value, tuple->items );
	DaoValue_Copy( type, tuple->items + 1 );
	DaoProcess_PutValue( proc, (DaoValue*) tuple );
	if( N >2 ){
		DaoType *tp = DaoNamespace_GetType( ns, p[2] );
		if( type ){
			if( DaoType_MatchTo( tp, (DaoType*) type, NULL ) ==0 ){
				DaoProcess_RaiseException( proc, DAO_ERROR, "type not matched" );
				return;
			}
		}
		DaoValue_Copy( p[2], value );
	}
}
static void META_Var2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoTuple *tuple = DaoTuple_New( 2 );
	DaoNamespace *ns = proc->activeNamespace;
	DaoClass *klass = NULL;
	DNode *node;
	DString *name = p[1]->xString.data;
	DaoValue *type = NULL;
	DaoValue **value = NULL;
	if( p[0]->type == DAO_CLASS || p[0]->type == DAO_OBJECT ){
		DaoObject *object = NULL;
		klass = & p[0]->xClass;
		if( p[0]->type == DAO_OBJECT ){
			klass = p[0]->xObject.defClass;
		}
		node = DMap_Find( klass->lookupTable, name );
		if( node && LOOKUP_ST( node->value.pInt ) == DAO_CLASS_VARIABLE ){
			DaoVariable *var = klass->variables->items.pVar[LOOKUP_ID( node->value.pInt )];
			value = & var->value;
			type = (DaoValue*) var->dtype;
		}else if( object && node && LOOKUP_ST( node->value.pInt ) == DAO_OBJECT_VARIABLE ){
			value = object->objValues + LOOKUP_ID( node->value.pInt );
			type = (DaoValue*) klass->instvars->items.pVar[ LOOKUP_ID( node->value.pInt ) ]->dtype;
		}else{
			DaoProcess_RaiseException( proc, DAO_ERROR, "invalid field" );
			return;
		}
	}else if( p[0]->type == DAO_NAMESPACE ){
		DaoNamespace *ns2 = & p[0]->xNamespace;
		return; //XXX
		//node = DMap_Find( ns2->varIndex, name );
		if( node ){
			DaoVariable *var = ns2->variables->items.pVar[node->value.pInt];
			value = & var->value;
			type = (DaoValue*) var->dtype;
		}
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
		return;
	}
	DaoValue_Copy( *value, tuple->items );
	DaoValue_Copy( type, tuple->items + 1 );
	DaoProcess_PutValue( proc, (DaoValue*) tuple );
	if( N >2 ){
		DaoType *tp = DaoNamespace_GetType( ns, p[2] );
		if( type ){
			if( DaoType_MatchTo( tp, (DaoType*) type, NULL ) ==0 ){
				DaoProcess_RaiseException( proc, DAO_ERROR, "type not matched" );
				return;
			}
		}
		DaoValue_Copy( p[2], value );
	}
}
static void META_Routine( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *list;
	DaoValue *item;
	int i;
	if( N ==1 ){ // XXX
		DaoRoutine *rout = & p[0]->xRoutine;
		list = DaoProcess_PutList( proc );
		if( p[0]->type != DAO_ROUTINE ){
			DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
			return;
		}
		if( rout->overloads ){
			for(i=0; i<rout->overloads->routines->size; i++){
				item = rout->overloads->routines->items.pValue[i];
				DaoList_Append( list, item );
			}
		}else{
		}
	}else{
		DaoProcess_PutValue( proc, (DaoValue*) proc->activeRoutine );
	}
}
static void META_Class( DaoProcess *proc, DaoValue *p[], int N )
{
#if 0
	if( p[0]->type == DAO_ROUTINE && p[0]->v.routine->tidHost == DAO_OBJECT ){
		DaoProcess_PutValue( proc, (DaoValue*) p[0]->v.routine->routHost->aux.v.klass );
	}else if( p[0]->type == DAO_OBJECT ){
		DaoProcess_PutValue( proc, (DaoValue*) p[0]->v.object->defClass );
	}
#endif
	DaoProcess_PutValue( proc, dao_none_value );
}
static void META_Isa( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns = proc->activeNamespace;
	daoint *res = DaoProcess_PutInteger( proc, 0 );
	if( p[1]->type == DAO_TYPE ){
		if( DaoType_MatchValue( & p[1]->xType, p[0], NULL ) ) *res = 1;
	}else if( p[1]->type == DAO_CLASS ){
		if( p[0]->type != DAO_OBJECT ) return;
		*res = DaoClass_ChildOf( p[0]->xObject.rootObject->defClass, p[1] );
	}else if( p[1]->type == DAO_CDATA ){
		if( p[0]->type == DAO_OBJECT ){
			*res = DaoClass_ChildOf( p[0]->xObject.rootObject->defClass, p[1] );
		}else if( p[0]->type == DAO_CDATA ){
			*res = DaoType_ChildOf( p[0]->xCdata.ctype, p[1]->xCdata.ctype );
		}
	}else if( p[1]->type == DAO_STRING ){
		DString *tname = p[1]->xString.data;
		DString_ToMBS( tname );
		if( strcmp( tname->mbs, "class" ) ==0 ){
			if( p[0]->type == DAO_CLASS  ) *res = 1;
		}else if( strcmp( tname->mbs, "object" ) ==0 ){
			if( p[0]->type == DAO_OBJECT  ) *res = 1;
		}else if( strcmp( tname->mbs, "routine" ) ==0 ){
			if( p[0]->type == DAO_ROUTINE  ) *res = 1;
		}else if( strcmp( tname->mbs, "namespace" ) ==0 ){
			if( p[0]->type == DAO_NAMESPACE  ) *res = 1;
		}else if( strcmp( tname->mbs, "tuple" ) ==0 ){
			if( p[0]->type == DAO_TUPLE  ) *res = 1;
		}else if( strcmp( tname->mbs, "list" ) ==0 ){
			if( p[0]->type == DAO_LIST  ) *res = 1;
		}else if( strcmp( tname->mbs, "map" ) ==0 ){
			if( p[0]->type == DAO_MAP  ) *res = 1;
		}else if( strcmp( tname->mbs, "array" ) ==0 ){
			if( p[0]->type == DAO_ARRAY  ) *res = 1;
		}else{
			DaoType *tp = DaoParser_ParseTypeName( tname->mbs, ns, 0 );
			if( tp && DaoType_MatchValue( tp, p[0], NULL ) ) *res = 1;
		}
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
	}
}
static void META_Self( DaoProcess *proc, DaoValue *p[], int N )
{
	if( p[0]->type == DAO_OBJECT )
		DaoProcess_PutValue( proc, (DaoValue*) p[0]->xObject.rootObject );
	else
		DaoProcess_PutValue( proc, dao_none_value );
}
static void META_Param( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoRoutine *routine = (DaoRoutine*) p[0];
	DaoList *list = DaoProcess_PutList( proc );
	DaoTuple *tuple;
	DaoType *routype = routine->routType;
	DaoType *itp = list->unitype->nested->items.pType[0];
	DaoType **nested = routype->nested->items.pType;
	DString *mbs = DString_New(1);
	DNode *node;
	DaoString str = {DAO_STRING,0,0,0,0,NULL};
	DaoInteger num = {DAO_INTEGER,0,0,0,0,1};
	int i, i3 = 3;
	str.data = mbs;
	for(i=0; i<routine->parCount; i++){
		if( i >= routype->nested->size ) break;
		tuple = DaoTuple_New( 4 );
		tuple->unitype = itp;
		GC_IncRC( itp );
		num.value = 0;
		if( nested[i]->tid == DAO_PAR_DEFAULT ) num.value = 1;
		DaoValue_Copy( (DaoValue*) & str, & tuple->items[0] );
		DaoValue_Copy( (DaoValue*) nested[i], & tuple->items[1] );
		DaoValue_Copy( (DaoValue*) & num, & tuple->items[2] );
		DaoValue_Copy( routine->routConsts->items.items.pValue[i], & tuple->items[i3] );
		DaoList_Append( list, (DaoValue*) tuple );
	}
	DString_Delete( mbs );
	if( routype->mapNames ){
		node = DMap_First( routype->mapNames );
		for( ; node !=NULL; node = DMap_Next( routype->mapNames, node ) ){
			i = node->value.pInt;
			mbs = list->items.items.pValue[i]->xTuple.items[0]->xString.data;
			DString_Assign( mbs, node->key.pString );
		}
	}
}
static void META_Argc( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutInteger( proc, proc->topFrame->parCount );
}
static void META_Argv( DaoProcess *proc, DaoValue *p[], int N )
{
	int i;
	if( N ==0 ){
		DaoList *list = DaoProcess_PutList( proc );
		for(i=0; i<proc->topFrame->parCount; i++) DaoList_Append( list, proc->activeValues[i] );
	}else{
		DaoValue *val = dao_none_value;
		if( p[0]->xInteger.value < proc->topFrame->parCount )
			val = proc->activeValues[ p[0]->xInteger.value ];
		DaoProcess_PutValue( proc, val );
	}
}
static void META_Trace( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoList *backtrace = DaoProcess_PutList( proc );
	DaoStackFrame *frame = proc->topFrame;
	int instr = 0, depth = 1;
	int maxDepth = 0;
	int print = 0;
	DaoTuple *entry = NULL;
	DaoValue *vRoutType;
	DaoString routName = {DAO_STRING,0,0,0,0,NULL};
	DaoString nsName = {DAO_STRING,0,0,0,0,NULL};
	DaoInteger line = {DAO_INTEGER,0,0,0,0,0};
	DaoInteger inst = {DAO_INTEGER,0,0,0,0,0};

	if( N >=1 ) print = p[0]->xEnum.value;
	if( N ==2 ) maxDepth = p[1]->xInteger.value;

	if( print ){
		DaoProcess_Trace( proc, maxDepth );
		return;
	}

#if 0
	for( ; frame && frame->context ; frame = frame->prev, ++depth ){
		/* Check if we got deeper than requested */
		if( depth > maxDepth && maxDepth > 0 ) break;

		/* Gather some of the informations we need. */
		vRoutType = (DaoValue*) frame->context->routine->routType;
		inst.value = (depth==1) ? (int)( proc->activeCode - proc->codes ) : frame->entry;
		line.value = frame->context->routine->annotCodes->items.pVmc[inst.value]->line;
		routName.data = frame->context->routine->routName;
		nsName.data = frame->context->nameSpace->name;

		/* Put all the informations into a tuple which we append to the list. */
		/* Tuple type: tuple<rout_name:string,rout_type:any,line:int,namespace:string> */
		/* Also, namespace is most often the current file name, but not always! */
		entry = DaoTuple_New( 5 );
		entry->unitype = backtrace->unitype->nested->items.pType[0];
		GC_IncRC( entry->unitype );

		DaoTuple_SetItem( entry, (DaoValue*) & routName, 0 );
		DaoTuple_SetItem( entry, (DaoValue*) vRoutType, 1 );
		DaoTuple_SetItem( entry, (DaoValue*) & inst, 2 );
		DaoTuple_SetItem( entry, (DaoValue*) & line, 3 );
		DaoTuple_SetItem( entry, (DaoValue*) & nsName, 4 );

		DaoList_PushBack( backtrace, (DaoValue*) entry );
	}
#endif
}
/* name( class/routine/type )
 * type( any )
 * find( "name" )
 * base( class )
 * field( class/object/ns/ )
 * doc( class/routine )
 * class( object/routine )
 * routine( class/object ) if omitted, current routine
 * param( routine ) if omitted, current params
 * self( object )
 * ns() current ns
 * trace( print=0 )
 * */
static DaoFuncItem metaMeths[]=
{
	{ META_NS,    "namespace() => any" },
	{ META_NS,    "namespace( object ) => any" },
	{ META_Name,  "name( object ) => string" },
	{ META_Type,  "type( object ) => any" },
	{ META_Base,  "base( object ) => list<any>" },
	{ META_Cst1,  "constant( object, restrict=0 )=>map<string,tuple<value:any,type:any>>" },
	{ META_Var1,  "variable( object, restrict=0 )=>map<string,tuple<value:any,type:any>>" },
	{ META_Cst2,  "constant( object, name:string )=>tuple<value:any,type:any>" },
	{ META_Var2,  "variable( object, name:string )=>tuple<value:any,type:any>" },
	{ META_Cst2,  "constant( object, name:string, value )=>tuple<value:any,type:any>" },
	{ META_Var2,  "variable( object, name:string, value )=>tuple<value:any,type:any>" },
	{ META_Class,   "class( object ) => any" },
	{ META_Routine, "routine() => any" },
	{ META_Routine, "routine( rout : any ) => list<any>" },
	{ META_Param,   "param( rout )=>list<tuple<name:string,type:any,deft:int,value:any>>" },
	{ META_Isa,     "isa( object, name : string ) => int" },
	{ META_Isa,     "isa( object, type : any ) => int" },
	{ META_Self,    "self( object ) => any" },
	{ META_Argc,    "argc() => int" },
	{ META_Argv,    "argv() => list<any>" },
	{ META_Argv,    "argv( i : int ) => any" },
	{ META_Trace,   "trace( action:enum<generate,print>=$generate, depth=0 ) => list<tuple<rout_name:string,rout_type:any,instr:int,line:int,namespace:string>>" },
	{ NULL, NULL }
};

DaoTypeBase metaTyper = {
	"meta", NULL, NULL, metaMeths, {NULL}, {0}, NULL, NULL
};

DAO_DLL int DaoMeta_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace_WrapType( ns, & metaTyper, 1 );
	return 0;
}
