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
#include"daoString.h"
#include"daoValue.h"
#include"daoParser.h"
#include"daoNamespace.h"
#include"daoGC.h"


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
		data = klass->cstData;
	}else if( p[0]->type == DAO_NAMESPACE ){
		ns = & p[0]->xNamespace;
		//index = ns->cstIndex; XXX
		data = ns->cstData;
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid parameter" );
		DString_Delete( name.data );
		return;
	}
	if( index == NULL ) return;
	node = DMap_First( index );
	for( ; node != NULL; node = DMap_Next( index, node ) ){
		size_t id = node->value.pSize;
		if( restri && lookup && LOOKUP_PM( id ) != DAO_DATA_PUBLIC ) continue;
		if( lookup ) id = LOOKUP_ID( id );
		tuple = DaoTuple_New( 2 );
		tuple->unitype = tp;
		GC_IncRC( tp );
		value = data->items.pValue[ id ];
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
		size_t st = 0, id = node->value.pSize;
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
				vabtp = klass->objDataType->items.pValue[ id ];
			}else if( st == DAO_CLASS_VARIABLE ){
				value = klass->glbData->items.pValue[id];
				vabtp = klass->glbDataType->items.pValue[ id ];
			}else if( st == DAO_OBJECT_VARIABLE ){
				vabtp = klass->objDataType->items.pValue[ id ];
			}
		}else{
			value = ns->varData->items.pValue[id];
			vabtp = ns->varType->items.pValue[ id ];
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
		if( node && LOOKUP_ST( node->value.pSize ) == DAO_CLASS_CONSTANT ){
			value = klass->cstData->items.pValue + LOOKUP_ID( node->value.pSize );
			type = (DaoValue*) DaoNamespace_GetType( ns, *value );
		}
	}else if( p[0]->type == DAO_NAMESPACE ){
		DaoNamespace *ns2 = & p[0]->xNamespace;
		return; //XXX
		//node = DMap_Find( ns2->cstIndex, name );
		if( node ){
			value = ns2->cstData->items.pValue + node->value.pInt;
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
		if( node && LOOKUP_ST( node->value.pSize ) == DAO_CLASS_VARIABLE ){
			value = klass->cstData->items.pValue + LOOKUP_ID( node->value.pSize );
			type = klass->glbDataType->items.pValue[ LOOKUP_ID( node->value.pSize ) ];
		}else if( object && node && LOOKUP_ST( node->value.pSize ) == DAO_OBJECT_VARIABLE ){
			value = object->objValues + LOOKUP_ID( node->value.pSize );
			type = klass->objDataType->items.pValue[ LOOKUP_ID( node->value.pSize ) ];
		}else{
			DaoProcess_RaiseException( proc, DAO_ERROR, "invalid field" );
			return;
		}
	}else if( p[0]->type == DAO_NAMESPACE ){
		DaoNamespace *ns2 = & p[0]->xNamespace;
		return; //XXX
		//node = DMap_Find( ns2->varIndex, name );
		if( node ){
			value = ns2->varData->items.pValue + node->value.pInt;
			type = ns2->varType->items.pValue[ node->value.pInt ];
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
	int i;
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
		DaoValue_Copy( routine->routConsts->items.items.pValue[i], & tuple->items[3] );
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
		//XXX DaoProcess_Trace( proc, maxDepth );
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
static void META_Doc( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *doc = NULL;
	switch( p[0]->type ){
	case DAO_CLASS : doc = p[0]->xClass.classHelp; break;
	case DAO_OBJECT : doc = p[0]->xObject.defClass->classHelp; break;
	//XXX case DAO_ROUTINE : doc = p[0]->v.routine->routHelp; break;
	default : break;
	}
	if( doc == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "documentation not available" );
		return;
	}
	DaoProcess_PutMBString( proc, doc->mbs );
	if( N >1 ){
		DString_Clear( doc );
		DString_Append( doc, p[1]->xString.data );
	}
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
	{ META_Doc,   "doc( object, newdoc='' ) => string" },
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

#ifdef DAO_INLINE_META
int DaoMeta_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
#else
int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
#endif
{
	DaoNamespace_WrapType( ns, & metaTyper, 1 );
	return 0;
}
