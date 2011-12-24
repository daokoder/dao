/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2011, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License as published by the Free Software Foundation;
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#include"string.h"
#include"ctype.h"
#include"locale.h"

#ifdef _MSC_VER
#include "direct.h"
#define getcwd _getcwd
#else
#include"unistd.h"
#endif

#include"daoNamespace.h"
#include"daoVmspace.h"
#include"daoParser.h"
#include"daoStream.h"
#include"daoRoutine.h"
#include"daoNumtype.h"
#include"daoClass.h"
#include"daoObject.h"
#include"daoRoutine.h"
#include"daoRegex.h"
#include"daoStdlib.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoSched.h"
#include"daoValue.h"

#ifdef DAO_WITH_THREAD
#include"daoThread.h"
#endif

extern int ObjectProfile[100];

DaoConfig daoConfig =
{
	1, /*cpu*/
	0, /*jit*/
	0, /*safe*/
	1, /*typedcode*/
	0, /*incompile*/
	0, /*iscgi*/
	8, /*tabspace*/
	0, /*chindent*/
	0, /*mbs*/
	0  /*wcs*/
};

DaoVmSpace *mainVmSpace = NULL;
DaoProcess *mainProcess = NULL;

extern ulong_t FileChangedTime( const char *file );

static int TestFile( DaoVmSpace *vms, DString *fname )
{
	FILE *file;
	DNode *node = MAP_Find( vms->vfiles, fname );
	/* printf( "testing: %s  %p\n", fname->mbs, node ); */
	if( node ) return 1;
	return Dao_IsFile( fname->mbs );
}
static int TestPath( DaoVmSpace *vms, DString *fname, int type )
{
	if( type == DAO_FILE_PATH ) return TestFile( vms, fname );
	return Dao_IsDir( fname->mbs );
}

static const char* const daoDllPrefix[] =
{
	"", "", "", "",
	"dao_", "libdao_", "lib"
};
static const char* const daoFileSuffix[] =
{
	".dao.o", ".dao.s", ".dao", DAO_DLL_SUFFIX,
	DAO_DLL_SUFFIX, DAO_DLL_SUFFIX, DAO_DLL_SUFFIX
	/* duplicated for automatically adding "dao/libdao_/lib" prefix; */
};
enum{
	DAO_MODULE_NONE,
	DAO_MODULE_DAO_O,
	DAO_MODULE_DAO_S,
	DAO_MODULE_DAO,
	DAO_MODULE_DLL
};

#ifndef CHANGESET_ID
#define CHANGESET_ID "Undefined"
#endif

const char *const dao_copy_notice =
"  Dao Virtual Machine " DAO_VERSION "\n"
"  Built date: " __DATE__ "\n"
"  Changeset ID: " CHANGESET_ID "\n\n"
"  Copyright(C) 2006-2011, Fu Limin\n"
"  Dao can be copied under the terms of GNU Lesser General Public License\n"
"  Dao Language website: http://www.daovm.net\n"
;

static const char *const cmd_help =
"\n Usage: dao [options] script_file\n"
" Options:\n"
"   -h, --help:           print this help information;\n"
"   -v, --version:        print version information;\n"
"   -e, --eval:           evaluate command line codes;\n"
"   -s, --safe:           run in safe mode;\n"
"   -d, --debug:          run in debug mode;\n"
"   -i, --ineractive:     run in interactive mode;\n"
"   -l, --list-bc:        print compiled bytecodes;\n"
"   -j, --jit:            enable just-in-time compiling;\n"
"   -T, --no-typed-code:  no typed VM codes;\n"
"   -n, --incr-comp:      incremental compiling;\n"
;
/*
   "   -s, --assembly:    generate assembly file;\n"
   "   -b, --bytecode:    generate bytecode file;\n"
   "   -c, --compile:     compile to bytecodes; (TODO)\n"
 */


extern DaoTypeBase  baseTyper;
extern DaoTypeBase  numberTyper;
extern DaoTypeBase  stringTyper;
extern DaoTypeBase  longTyper;
extern DaoTypeBase  enumTyper;
extern DaoTypeBase  listTyper;
extern DaoTypeBase  mapTyper;
extern DaoTypeBase  streamTyper;
extern DaoTypeBase  routTyper;
extern DaoTypeBase  funcTyper;
extern DaoTypeBase  interTyper;
extern DaoTypeBase  classTyper;
extern DaoTypeBase  objTyper;
extern DaoTypeBase  nsTyper;
extern DaoTypeBase  tupleTyper;
extern DaoTypeBase  namevaTyper;

extern DaoTypeBase  numarTyper;
extern DaoTypeBase  comTyper;
extern DaoTypeBase  abstypeTyper;
extern DaoTypeBase  curryTyper;
extern DaoTypeBase  rgxMatchTyper;
extern DaoTypeBase  futureTyper;

extern DaoTypeBase mutexTyper;
extern DaoTypeBase condvTyper;
extern DaoTypeBase semaTyper;
extern DaoTypeBase thdMasterTyper;

extern DaoTypeBase macroTyper;
extern DaoTypeBase regexTyper;
extern DaoTypeBase vmpTyper;
extern DaoTypeBase typeKernelTyper;
static DaoTypeBase vmsTyper;

DaoTypeBase* DaoVmSpace_GetTyper( short type )
{
	switch( type ){
	case DAO_INTEGER  :  return & numberTyper;
	case DAO_FLOAT    :  return & numberTyper;
	case DAO_DOUBLE   :  return & numberTyper;
	case DAO_COMPLEX  :  return & comTyper;
	case DAO_LONG     :  return & longTyper;
	case DAO_ENUM     :  return & enumTyper;
	case DAO_STRING   :  return & stringTyper;
	case DAO_LIST     :  return & listTyper;
	case DAO_MAP      :  return & mapTyper;
	case DAO_PAR_NAMED : return & namevaTyper;
	case DAO_TUPLE     : return & tupleTyper;
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY  :  return & numarTyper;
#else
	case DAO_ARRAY  :  return & baseTyper;
#endif
	case DAO_FUNCURRY : return & curryTyper;
	case DAO_CDATA   :  return & defaultCdataTyper;
	case DAO_ROUTINE   :  return & routTyper;
	case DAO_INTERFACE :  return & interTyper;
	case DAO_CLASS     :  return & classTyper;
	case DAO_OBJECT    :  return & objTyper;
	case DAO_STREAM    :  return & streamTyper;
	case DAO_NAMESPACE :  return & nsTyper;
	case DAO_PROCESS   :  return & vmpTyper;
	case DAO_VMSPACE   :  return & vmsTyper;
	case DAO_TYPE      :  return & abstypeTyper;
	case DAO_TYPEKERNEL : return & typeKernelTyper;
#ifdef DAO_WITH_MACRO
	case DAO_MACRO     :  return & macroTyper;
#endif
#ifdef DAO_WITH_CONCURRENT
	case DAO_MUTEX     :  return & mutexTyper;
	case DAO_CONDVAR   :  return & condvTyper;
	case DAO_SEMA      :  return & semaTyper;
	case DAO_FUTURE    :  return & futureTyper;
#endif
	default : break;
	}
	return & baseTyper;
}

void DaoVmSpace_SetOptions( DaoVmSpace *self, int options )
{
	self->options = options;
}
int DaoVmSpace_GetOptions( DaoVmSpace *self )
{
	return self->options;
}
DaoNamespace* DaoVmSpace_GetNamespace( DaoVmSpace *self, const char *name )
{
	DaoNamespace *ns;
	DString str = DString_WrapMBS( name );
	DNode *node = DMap_Find( self->nsModules, & str );
	if( node ) return (DaoNamespace*) node->value.pValue;
	ns = DaoNamespace_New( self, name );
	GC_IncRC( ns );
	DaoVmSpace_Lock( self );
	DMap_Insert( self->nsModules, & str, ns );
	DaoVmSpace_Unlock( self );
	return ns;
}
DaoNamespace* DaoVmSpace_MainNamespace( DaoVmSpace *self )
{
	return self->mainNamespace;
}
DaoProcess* DaoVmSpace_MainProcess( DaoVmSpace *self )
{
	return self->mainProcess;
}
DaoProcess* DaoVmSpace_AcquireProcess( DaoVmSpace *self )
{
	DaoProcess *proc = NULL;
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexProc );
#endif
	if( self->processes->size ){
		proc = DArray_Back( self->processes );
		DArray_PopBack( self->processes );
	}else{
		proc = DaoProcess_New( self );
		DMap_Insert( self->allProcesses, proc, 0 );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexProc );
#endif
	return proc;
}
void DaoVmSpace_ReleaseProcess( DaoVmSpace *self, DaoProcess *proc )
{
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexProc );
#endif
	if( DMap_Find( self->allProcesses, proc ) ){
		GC_DecRC( proc->future );
		proc->future = NULL;
#ifdef DAO_WITH_THREAD
		proc->condv = NULL;
		proc->mutex = NULL;
#endif
		DaoProcess_PopFrames( proc, proc->firstFrame );
		DArray_PushBack( self->processes, proc );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexProc );
#endif
}
void DaoVmSpace_SetUserHandler( DaoVmSpace *self, DaoUserHandler *handler )
{
	self->userHandler = handler;
}
void DaoVmSpace_ReadLine( DaoVmSpace *self, ReadLine fptr )
{
	self->ReadLine = fptr;
}
void DaoVmSpace_AddHistory( DaoVmSpace *self, AddHistory fptr )
{
	self->AddHistory = fptr;
}
void DaoVmSpace_Stop( DaoVmSpace *self, int bl )
{
	self->stopit = bl;
}

static void DaoVmSpace_InitPath( DaoVmSpace *self )
{
	char *daodir = getenv( "DAO_DIR" );
	char pwd[512];
	getcwd( pwd, 511 );
	DaoVmSpace_SetPath( self, pwd );
	DaoVmSpace_AddPath( self, pwd );
	DaoVmSpace_AddPath( self, DAO_DIR );
	DaoVmSpace_AddPath( self, "~/dao" );
	if( daodir ) DaoVmSpace_AddPath( self, daodir );
}

static DaoTypeBase vmsTyper=
{
	"vmspace", NULL, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoVmSpace_Delete, NULL
};


DaoVmSpace* DaoVmSpace_New()
{
	DaoVmSpace *self = (DaoVmSpace*) dao_malloc( sizeof(DaoVmSpace) );
	DaoValue_Init( self, DAO_VMSPACE );
	self->stdStream = DaoStream_New();
	self->stdStream->vmSpace = self;
	self->options = 0;
	self->stopit = 0;
	self->safeTag = 1;
	self->evalCmdline = 0;
	self->userHandler = NULL;
	self->mainSource = DString_New(1);
	self->vfiles = DMap_New(D_STRING,D_STRING);
	self->nsModules = DMap_New(D_STRING,0);
	self->allTokens = DMap_New(D_STRING,0);
	self->pathWorking = DString_New(1);
	self->nameLoading = DArray_New(D_STRING);
	self->pathLoading = DArray_New(D_STRING);
	self->pathSearching = DArray_New(D_STRING);
	self->processes = DArray_New(0);
	self->allProcesses = DMap_New(D_VALUE,0);

	if( daoConfig.safe ) self->options |= DAO_EXEC_SAFE;

#ifdef DAO_WITH_THREAD
	DMutex_Init( & self->mutexLoad );
	DMutex_Init( & self->mutexProc );
#endif

	self->nsInternal = NULL; /* need to be set for DaoNamespace_New() */
	self->nsInternal = DaoNamespace_New( self, "dao" );
	self->nsInternal->vmSpace = self;
	self->nsInternal->refCount += 2;
	DMap_Insert( self->nsModules, self->nsInternal->name, self->nsInternal );

	self->mainNamespace = DaoNamespace_New( self, "MainNamespace" );
	self->mainNamespace->vmSpace = self;
	self->mainNamespace->refCount ++;
	self->stdStream->refCount ++;

	self->ReadLine = NULL;
	self->AddHistory = NULL;

	self->mainProcess = DaoProcess_New( self );
	GC_IncRC( self->mainProcess );

	if( mainVmSpace ) DaoNamespace_AddParent( self->nsInternal, mainVmSpace->nsInternal );

	return self;
}
void DaoVmSpace_Delete( DaoVmSpace *self )
{
	DNode *node = DMap_First( self->nsModules );
#ifdef DEBUG
	ObjectProfile[ DAO_VMSPACE ] --;
#endif
	for( ; node!=NULL; node = DMap_Next( self->nsModules, node ) ){
#if 0
		printf( "%i  %i  %s\n", node->value.pValue->refCount,
				((DaoNamespace*)node->value.pValue)->cmethods->size, node->key.pString->mbs );
#endif
		GC_DecRC( node->value.pValue );
	}
	GC_DecRC( self->nsInternal );
	GC_DecRC( self->mainNamespace );
	GC_DecRC( self->stdStream );
	DString_Delete( self->mainSource );
	DString_Delete( self->pathWorking );
	DArray_Delete( self->nameLoading );
	DArray_Delete( self->pathLoading );
	DArray_Delete( self->pathSearching );
	DArray_Delete( self->processes );
	DMap_Delete( self->vfiles );
	DMap_Delete( self->allTokens );
	DMap_Delete( self->nsModules );
	DMap_Delete( self->allProcesses );
	GC_DecRC( self->mainProcess );
#ifdef DAO_WITH_THREAD
	DMutex_Destroy( & self->mutexLoad );
	DMutex_Destroy( & self->mutexProc );
#endif

	dao_free( self );
}
void DaoVmSpace_Lock( DaoVmSpace *self )
{
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexLoad );
#endif
}
void DaoVmSpace_Unlock( DaoVmSpace *self )
{
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexLoad );
#endif
}
static int DaoVmSpace_ReadSource( DaoVmSpace *self, DString *fname, DString *source )
{
	DNode *node = MAP_Find( self->vfiles, fname );
	/* printf( "reading %s\n", fname->mbs ); */
	if( node ){
		DString_Assign( source, node->value.pString );
		return 1;
	}
	if( DaoFile_ReadAll( fopen( fname->mbs, "r" ), source, 1 ) ) return 1;
	DaoStream_WriteMBS( self->stdStream, "ERROR: can not open file \"" );
	DaoStream_WriteMBS( self->stdStream, fname->mbs );
	DaoStream_WriteMBS( self->stdStream, "\".\n" );
	return 0;
}
void SplitByWhiteSpaces( DString *str, DArray *tokens )
{
	size_t i, j, k=0, size = str->size;
	const char *chs;
	DString *tok = DString_New(1);
	DArray_Clear( tokens );
	DString_ToMBS( str );
	chs = str->mbs;
	while( (j=DString_FindChar( str, '\0', k )) != MAXSIZE ){
		if( j > k ){
			DString_SubString( str, tok, k, j-k );
			DArray_Append( tokens, tok );
		}
		k = j + 1;
	}
	if( tokens->size ){
		if( k < str->size ){
			DString_SubString( str, tok, k, str->size-k );
			DArray_Append( tokens, tok );
		}
		DString_Delete( tok );
		return;
	}
	for( i=0; i<size; i++){
		if( chs[i] == '\\' && i+1 < size ){
			DString_AppendChar( tok, chs[i+1] );
			++i;
			continue;
		}else if( isspace( chs[i] ) ){
			if( tok->size > 0 ){
				DArray_Append( tokens, tok );
				DString_Clear( tok );
			}
			continue;
		}
		DString_AppendChar( tok, chs[i] );
	}
	if( tok->size > 0 ) DArray_Append( tokens, tok );
	DString_Delete( tok );
}

int DaoJIT_TryInit( DaoVmSpace *vms );
int DaoVmSpace_ParseOptions( DaoVmSpace *self, DString *options )
{
	DString *str = DString_New(1);
	DArray *array = DArray_New(D_STRING);
	size_t i, j;

	SplitByWhiteSpaces( options, array );
	for( i=0; i<array->size; i++ ){
		DString *token = array->items.pString[i];
		if( self->evalCmdline ){
			DString_Append( self->mainSource, token );
			DString_AppendChar( self->mainSource, ' ' );
			continue;
		}
		if( token->mbs[0] =='-' && token->size >1 && token->mbs[1] =='-' ){
			if( strcmp( token->mbs, "--help" ) ==0 ){
				self->options |= DAO_EXEC_HELP;
			}else if( strcmp( token->mbs, "--version" ) ==0 ){
				self->options |= DAO_EXEC_VINFO;
			}else if( strcmp( token->mbs, "--eval" ) ==0 ){
				self->evalCmdline = 1;
				DString_Clear( self->mainSource );
			}else if( strcmp( token->mbs, "--debug" ) ==0 ){
				self->options |= DAO_EXEC_DEBUG;
			}else if( strcmp( token->mbs, "--safe" ) ==0 ){
				self->options |= DAO_EXEC_SAFE;
				daoConfig.safe = 1;
			}else if( strcmp( token->mbs, "--interactive" ) ==0 ){
				self->options |= DAO_EXEC_INTERUN;
			}else if( strcmp( token->mbs, "--list-bc" ) ==0 ){
				self->options |= DAO_EXEC_LIST_BC;
			}else if( strcmp( token->mbs, "--list-bc" ) ==0 ){
				self->options |= DAO_EXEC_LIST_BC;
			}else if( strcmp( token->mbs, "--compile" ) ==0 ){
				self->options |= DAO_EXEC_COMP_BC;
			}else if( strcmp( token->mbs, "--incr-comp" ) ==0 ){
				self->options |= DAO_EXEC_INCR_COMP;
				daoConfig.incompile = 1;
			}else if( strcmp( token->mbs, "--no-typed-code" ) ==0 ){
				self->options |= DAO_EXEC_NO_TC;
				daoConfig.typedcode = 0;
			}else if( strcmp( token->mbs, "--jit" ) ==0 ){
				self->options |= DAO_EXEC_JIT;
				daoConfig.jit = 1;
			}else if( token->size ){
				DaoStream_WriteMBS( self->stdStream, "Unknown option: " );
				DaoStream_WriteMBS( self->stdStream, token->mbs );
				DaoStream_WriteMBS( self->stdStream, ";\n" );
			}
		}else if( DString_MatchMBS( token, " ^ [%C_]+=.* ", NULL, NULL ) ){
			token = DString_DeepCopy( token );
			putenv( token->mbs );
		}else{
			size_t len = token->size;
			DString_Clear( str );
			for( j=0; j<len; j++ ){
				switch( token->mbs[j] ){
				case 'h' : self->options |= DAO_EXEC_HELP;      break;
				case 'v' : self->options |= DAO_EXEC_VINFO;     break;
				case 'd' : self->options |= DAO_EXEC_DEBUG;     break;
				case 'i' : self->options |= DAO_EXEC_INTERUN;   break;
				case 'l' : self->options |= DAO_EXEC_LIST_BC;   break;
				case 'c' : self->options |= DAO_EXEC_COMP_BC;   break;
				case 's' : self->options |= DAO_EXEC_SAFE;
						   daoConfig.safe = 1;
						   break;
				case 'n' : self->options |= DAO_EXEC_INCR_COMP;
						   daoConfig.incompile = 0;
						   break;
				case 'j' : self->options |= DAO_EXEC_JIT;
						   daoConfig.jit = 1;
						   break;
				case 'T' : self->options |= DAO_EXEC_NO_TC;
						   daoConfig.typedcode = 0;
						   break;
				case 'e' : self->evalCmdline = 1;
						   DString_Clear( self->mainSource );
						   break;
				case '-' : break;
				default :
						   if( token->mbs[j] ){
							   DString_AppendChar( str, token->mbs[j] );
							   DString_AppendChar( str, ' ' );
						   }
						   break;
				}
			}
			if( str->size > 0 ){
				DaoStream_WriteMBS( self->stdStream, "Unknown option: " );
				DaoStream_WriteMBS( self->stdStream, str->mbs );
				DaoStream_WriteMBS( self->stdStream, ";\n" );
			}
		}
	}
	DString_Delete( str );
	DArray_Delete( array );
	if( daoConfig.jit && dao_jit.Compile == NULL && DaoJIT_TryInit( self ) == 0 ){
		DaoStream_WriteMBS( self->stdStream, "Failed to enable Just-In-Time compiling!\n" );
	}
	return 1;
}

static DaoValue* DaoParseNumber( const char *s, DaoValue *value )
{
	if( strchr( s, 'e' ) != NULL ){
		value->type = DAO_FLOAT;
		value->xFloat.value = strtod( s, 0 );
	}else if( strchr( s, 'E' ) != NULL ){
		value->type = DAO_DOUBLE;
		value->xDouble.value = strtod( s, 0 );
	}else if( strchr( s, '.' ) != NULL ){
		int len = strlen( s );
		if( strstr( s, "00" ) == s + (len-2) ){
			value->type = DAO_DOUBLE;
			value->xDouble.value = strtod( s, 0 );
		}else{
			value->type = DAO_FLOAT;
			value->xFloat.value = strtod( s, 0 );
		}
	}else{
		value->type = DAO_INTEGER;
		value->xInteger.value = strtod( s, 0 );
	}
	return value;
}

static int DaoVmSpace_CompleteModuleName( DaoVmSpace *self, DString *fname );
static DaoNamespace* DaoVmSpace_LoadDaoByteCode( DaoVmSpace *self, DString *path, int run );
static DaoNamespace* DaoVmSpace_LoadDaoAssembly( DaoVmSpace *self, DString *path, int run );
static DaoNamespace* DaoVmSpace_LoadDaoModuleExt( DaoVmSpace *self, DString *p, DArray *a, int run );
static DaoNamespace* DaoVmSpace_LoadDllModule( DaoVmSpace *self, DString *libpath );

static void DaoVmSpace_ParseArguments( DaoVmSpace *self, DaoNamespace *ns,
		DString *file, DArray *args, DArray *argNames, DArray *argValues )
{
	DaoInteger ival = {DAO_INTEGER,0,0,0,0,0};
	DaoString sval1 = {DAO_STRING,0,0,0,0,NULL};
	DaoString sval2 = {DAO_STRING,0,0,0,0,NULL};
	DaoValue *nkey = (DaoValue*) & ival;
	DaoValue *skey = (DaoValue*) & sval1;
	DaoValue *sval = (DaoValue*) & sval2;
	DaoType *nested[2];
	DaoList *argv = DaoList_New();
	DaoMap *cmdarg = DaoMap_New(0);
	DArray *array = args;
	DString *str = DString_New(1);
	DString *key = DString_New(1);
	DString *val = DString_New(1);
	size_t i;
	int tk, offset=0, eq=0;

	skey->xString.data = key;
	sval->xString.data = val;
	nested[0] = DaoNamespace_MakeType( ns, "any", DAO_ANY, NULL,NULL,0 );
	nested[1] = DaoNamespace_MakeType( ns, "string",DAO_STRING, NULL,NULL,0 );
	cmdarg->unitype = DaoNamespace_MakeType( ns, "map",DAO_MAP,NULL,nested,2);
	argv->unitype = DaoNamespace_MakeType( ns, "list",DAO_LIST,NULL,nested+1,1);
	GC_IncRC( cmdarg->unitype );
	GC_IncRC( argv->unitype );
	if( array == NULL && file ){
		array = DArray_New(D_STRING);
		SplitByWhiteSpaces( file, array );
		DString_Assign( ns->name, array->items.pString[0] );
	}
	DString_Assign( val, array->items.pString[0] );
	DaoList_Append( argv, sval );
	DaoMap_Insert( cmdarg, nkey, sval );
	DaoVmSpace_MakePath( self, ns->name, DAO_FILE_PATH, 1 );
	DaoNamespace_SetName( ns, ns->name->mbs ); /* to update ns->path and ns->file; */
	i = 1;
	while( i < array->size ){
		DString *s = array->items.pString[i];
		i ++;
		nkey->xInteger.value ++;
		offset = 0;
		if( s->mbs[0] == '-' ){
			offset += 1;
			if( s->mbs[1] == '-' ) offset += 1;
		}
		tk = DaoToken_Check( s->mbs+offset, s->size-offset, & eq );
		if( tk == DTOK_IDENTIFIER && s->mbs[eq+offset] == '=' ){
			DString_SubString( s, key, offset, eq );
			DString_SubString( s, val, eq+offset+1, s->size-offset-eq );
			DArray_Append( argNames, key );
			DArray_Append( argValues, val );

			DaoList_Append( argv, sval );
			DaoMap_Insert( cmdarg, skey, sval );
			DString_SubString( s, key, 0, eq );
			DaoMap_Insert( cmdarg, skey, sval );
			DaoMap_Insert( cmdarg, nkey, sval );
		}else if( tk == DTOK_IDENTIFIER && offset && i < array->size ){
			DString_SubString( s, key, offset, s->size-offset );
			DString_Assign( val, array->items.pString[i] );
			DArray_Append( argNames, key );
			DArray_Append( argValues, val );

			DaoList_Append( argv, sval );
			DaoMap_Insert( cmdarg, skey, sval );
			DString_Assign( key, s );
			DaoMap_Insert( cmdarg, skey, sval );
			DaoMap_Insert( cmdarg, nkey, sval );
			i += 1;
		}else{
			DString_Clear( key );
			DString_Assign( val, s );
			DArray_Append( argNames, key );
			DArray_Append( argValues, s );

			DaoList_Append( argv, sval );
			DaoMap_Insert( cmdarg, nkey, sval );
		}
	}
	DString_SetMBS( str, "ARGV" );
	DaoNamespace_AddConst( ns, str, (DaoValue*) argv, DAO_DATA_PUBLIC );
	if( ns == self->mainNamespace ){
		DaoVmSpace_Lock( self );
		DaoNamespace_AddConst( self->nsInternal, str, nkey, DAO_DATA_PUBLIC );
		DaoVmSpace_Unlock( self );
	}
	DString_SetMBS( str, "CMDARG" );
	DaoNamespace_AddConst( ns, str, (DaoValue*) cmdarg, DAO_DATA_PUBLIC );
	if( ns == self->mainNamespace ){
		DaoVmSpace_Lock( self );
		DaoNamespace_AddConst( self->nsInternal, str, (DaoValue*) cmdarg, DAO_DATA_PUBLIC );
		DaoVmSpace_Unlock( self );
	}
	if( args == NULL ) DArray_Delete( array );
	DString_Delete( key );
	DString_Delete( val );
	DString_Delete( str );
}
static void DaoVmSpace_ConvertArguments( DaoNamespace *ns, DArray *argNames, DArray *argValues )
{
	DaoInteger ival = {DAO_INTEGER,0,0,0,0,0};
	DaoString sval1 = {DAO_STRING,0,0,0,0,NULL};
	DaoString sval2 = {DAO_STRING,0,0,0,0,NULL};
	DaoValue *nkey = (DaoValue*) & ival;
	DaoValue *skey = (DaoValue*) & sval1;
	DaoValue *sval = (DaoValue*) & sval2;
	DaoRoutine *rout = ns->mainRoutine;
	DaoType *abtp = rout->routType;
	DString *key = DString_New(1);
	DString *val = DString_New(1);
	DString *str;
	int i;
	skey->xString.data = key;
	sval->xString.data = val;
	DaoList_Clear( ns->argParams );
	DString_SetMBS( key, "main" );
	i = ns ? DaoNamespace_FindConst( ns, key ) : -1;
	if( i >=0 ){
		nkey = DaoNamespace_GetConst( ns, i );
		/* It may has not been compiled if it is not called explicitly. */
		if( nkey->type == DAO_ROUTINE ){ // TODO: better handling
			DaoRoutine_Compile( & nkey->xRoutine );
			rout = & nkey->xRoutine;
			abtp = rout->routType;
		}
	}
	if( rout == NULL ){
		DString_Delete( key );
		DString_Delete( val );
		return;
	}
	for( i=0; i<argNames->size; i++ ){
		nkey = sval;
		/*
		   printf( "argname = %s; argval = %s\n", argNames->items.pString[i]->mbs,
		   argValues->items.pString[i]->mbs );
		 */
		DString_Assign( val, argValues->items.pString[i] );
		if( abtp->nested->size > i ){
			if( abtp->nested->items.pValue[i] ){
				int k = abtp->nested->items.pType[i]->tid;
				char *chars = argValues->items.pString[i]->mbs;
				if( k == DAO_PAR_NAMED || k == DAO_PAR_DEFAULT )
					k = abtp->nested->items.pType[i]->aux->xType.tid;
				if( chars[0] == '+' || chars[0] == '-' ) chars ++;
				str = argNames->items.pString[i];
				if( str->size && abtp->mapNames ){
					DNode *node = MAP_Find( abtp->mapNames, str );
					if( node ){
						int id = node->value.pInt;
						k = abtp->nested->items.pType[id]->tid;
						if( k == DAO_PAR_NAMED || k == DAO_PAR_DEFAULT )
							k = abtp->nested->items.pType[id]->aux->xType.tid;
					}
				}
				if( k >0 && k <= DAO_DOUBLE && DaoToken_IsNumber( chars, 0 ) ){
					DaoDouble tmp = {0,0,0,0,0,0.0};
					nkey = DaoParseNumber( chars, (DaoValue*) & tmp );
				}
			}
		}
		if( argNames->items.pString[i]->size ){
			DaoNameValue *nameva = DaoNameValue_New( argNames->items.pString[i], nkey );
			DaoList_Append( ns->argParams, (DaoValue*) nameva );
			nameva->trait |= DAO_DATA_CONST;
		}else{
			DaoList_Append( ns->argParams, nkey );
		}
	}
	DString_Delete( key );
	DString_Delete( val );
}

DaoNamespace* DaoVmSpace_Load( DaoVmSpace *self, DString *file, int run )
{
	DArray *args = DArray_New(D_STRING);
	DString *path = DString_New(1);
	DaoNamespace *ns = NULL;

	SplitByWhiteSpaces( file, args );
	DString_Assign( path, args->items.pString[0] );
	switch( DaoVmSpace_CompleteModuleName( self, path ) ){
	case DAO_MODULE_DAO_O : ns = DaoVmSpace_LoadDaoByteCode( self, path, 0 ); break;
	case DAO_MODULE_DAO_S : ns = DaoVmSpace_LoadDaoAssembly( self, path, 0 ); break;
	case DAO_MODULE_DAO : ns = DaoVmSpace_LoadDaoModuleExt( self, path, args, run ); break;
	case DAO_MODULE_DLL : ns = DaoVmSpace_LoadDllModule( self, path ); break;
	default : ns = DaoVmSpace_LoadDaoModuleExt( self, path, args, run ); break; /* any suffix */
	}
	DArray_Delete( args );
	DString_Delete( path );
	if( ns == NULL ) return 0;
	return ns;
}
/* Link "ns" to the module/namespace corresponding to "mod". */
/* If the module "mod" is not loaded yet, it will be loaded first. */
/* Return the namespace corresponding to "mod". */
DaoNamespace* DaoVmSpace_LinkModule( DaoVmSpace *self, DaoNamespace *ns, const char *mod )
{
	DString name = DString_WrapMBS( mod );
	DaoNamespace *modns = DaoVmSpace_Load( self, & name, 0 );
	if( modns == NULL ) return NULL;
	DaoNamespace_AddParent( ns, modns );
	return modns;
}

static int CheckCodeCompletion( DString *source, DArray *tokens )
{
	int i, bcount, cbcount, sbcount, tki = 0, completed = 1;
	DaoToken_Tokenize( tokens, source->mbs, 0, 1, 1 );
	if( tokens->size ) tki = tokens->items.pToken[tokens->size-1]->type;
	switch( tki ){
	case DTOK_LB :
	case DTOK_LCB :
	case DTOK_LSB :
	case DTOK_VBT_OPEN :
	case DTOK_CMT_OPEN :
	case DTOK_MBS_OPEN :
	case DTOK_WCS_OPEN :
		completed = 0;
		break;
	}
	if( tokens->size && completed ){
		bcount = sbcount = cbcount = 0;
		for(i=0; i<tokens->size; i++){
			DaoToken *tk = tokens->items.pToken[i];
			switch( tk->type ){
			case DTOK_LB : bcount --; break;
			case DTOK_RB : bcount ++; break;
			case DTOK_LCB : cbcount --; break;
			case DTOK_RCB : cbcount ++; break;
			case DTOK_LSB : sbcount --; break;
			case DTOK_RSB : sbcount ++; break;
			default : break;
			}
		}
		if( bcount <0 || sbcount <0 || cbcount <0 ) completed = 0;
	}
	return completed;
}
static void DaoVmSpace_Interun( DaoVmSpace *self, CallbackOnString callback )
{
	DArray *tokens = DArray_New( D_TOKEN );
	DString *input = DString_New(1);
	const char *varRegex = "^ %s* = %s* %S+";
	const char *srcRegex = "^ %s* %w+ %. dao .* $";
	const char *sysRegex = "^ %\\ %s* %w+ %s* .* $";
	char *chs;
	int ch, newline = 0;
	DString_SetMBS( self->mainNamespace->name, "interactive codes" );
	self->mainNamespace->options |= DAO_NS_AUTO_GLOBAL;
	while(1){
		DString_Clear( input );
		DaoValue_Clear( self->mainProcess->stackValues );
		if( self->ReadLine ){
			chs = self->ReadLine( "(dao) " );
			while( chs ){
				DString_AppendMBS( input, chs );
				DString_AppendChar( input, '\n' );
				dao_free( chs );
				if( CheckCodeCompletion( input, tokens ) ){
					DString_Trim( input );
					if( input->size && self->AddHistory ) self->AddHistory( input->mbs );
					break;
				}
				chs = self->ReadLine( "..... " );
			}
		}else{
			printf( "(dao) " );
			fflush( stdout );
			ch = getchar();
			if( ch == EOF ) break;
			while( ch != EOF ){
				if( ch == '\n' ){
					if( CheckCodeCompletion( input, tokens ) ) break;
					printf("..... ");
					fflush( stdout );
				}
				DString_AppendChar( input, (char)ch );
				ch = getchar();
			}
			if( ch == EOF ) clearerr( stdin );
			DString_Trim( input );
		}
		if( input->size == 0 ) continue;
		self->stopit = 0;
		if( strcmp( input->mbs, "q" ) == 0 ){
			break;
		}else if( DString_MatchMBS( input, sysRegex, NULL, NULL ) ){
			if( system( input->mbs+1 ) ==-1) printf( "shell command failed\n" );
		}else if( DString_MatchMBS( input, srcRegex, NULL, NULL ) ){
			DString_InsertMBS( input, "std.load(", 0, 0, 0 );
			DString_AppendMBS( input, ", 0, 1 )" );
			if( callback ){
				(*callback)( input->mbs );
				continue;
			}
			DaoProcess_Eval( self->mainProcess, self->mainNamespace, input, 1 );
		}else if( DString_MatchMBS( input, varRegex, NULL, NULL ) ){
			DString_ChangeMBS( input, "^ %s* = %s*", "", 0 );
			DString_InsertMBS( input, "return ", 0, 0, 0 );
			if( callback ){
				(*callback)( input->mbs );
				continue;
			}
			DaoProcess_Eval( self->mainProcess, self->mainNamespace, input, 1 );
		}else{
			if( callback ){
				(*callback)( input->mbs );
				continue;
			}
			DaoProcess_Eval( self->mainProcess, self->mainNamespace, input, 1 );
		}
#ifdef DAO_WITH_CONCURRENT
		DaoCallServer_Join();
#endif
		/*
		   printf( "%s\n", input->mbs );
		 */
	}
	self->mainNamespace->options &= ~DAO_NS_AUTO_GLOBAL;
	DString_Delete( input );
	DArray_Delete( tokens );
}

static void DaoVmSpace_ExeCmdArgs( DaoVmSpace *self )
{
	DaoNamespace *ns = self->mainNamespace;
	size_t i;
	if( self->options & DAO_EXEC_VINFO ){
		DaoStream_WriteNewLine( self->stdStream );
		DaoStream_WriteMBS( self->stdStream, dao_copy_notice );
		DaoStream_WriteNewLine( self->stdStream );
	}
	if( self->options & DAO_EXEC_HELP )  DaoStream_WriteMBS( self->stdStream, cmd_help );
	DaoStream_Flush( self->stdStream );

	if( self->options & DAO_EXEC_LIST_BC ){
		for( i=ns->cstUser; i<ns->cstData->size; i++){
			DaoValue *p = ns->cstData->items.pValue[i];
			if( p->type == DAO_ROUTINE && & p->xRoutine != ns->mainRoutine ){
				DaoRoutine_Compile( & p->xRoutine );
				DaoRoutine_PrintCode( & p->xRoutine, self->stdStream );
			}else if( p->type == DAO_CLASS ){
				DaoClass_PrintCode( & p->xClass, self->stdStream );
			}
		}
		DaoStream_Flush( self->stdStream );
		if( ns->mainRoutine )
			DaoRoutine_PrintCode( ns->mainRoutine, self->stdStream );
		if( ( self->options & DAO_EXEC_INTERUN ) && self->userHandler == NULL )
			DaoVmSpace_Interun( self, NULL );
	}
}
int DaoVmSpace_RunMain( DaoVmSpace *self, DString *file )
{
	DaoNamespace *ns = self->mainNamespace;
	DaoProcess *vmp = self->mainProcess;
	DaoRoutine *mainRoutine;
	DRoutine *unirout = NULL;
	DaoMethod *meth = NULL;
	DaoValue **ps;
	DString *name;
	DArray *argNames;
	DArray *argValues;
	ulong_t tm = 0;
	size_t N;
	int i, j, res;

	if( file == NULL || file->size ==0 || self->evalCmdline ){
		DArray_PushFront( self->nameLoading, self->pathWorking );
		DArray_PushFront( self->pathLoading, self->pathWorking );
		if( self->evalCmdline ){
			DaoRoutine *rout;
			DString_SetMBS( self->mainNamespace->name, "command line codes" );
			if( DaoProcess_Compile( vmp, ns, self->mainSource, 1 ) ==0 ) return 0;
			DaoVmSpace_ExeCmdArgs( self );
			rout = ns->mainRoutines->items.pRout[ ns->mainRoutines->size-1 ];
			if( DaoProcess_Call( vmp, (DaoMethod*) rout, NULL, NULL, 0 ) ==0 ) return 0;
		}else{
			DaoVmSpace_ExeCmdArgs( self );
		}
		if( (self->options & DAO_EXEC_INTERUN) && self->userHandler == NULL )
			DaoVmSpace_Interun( self, NULL );
		return 1;
	}
	argNames = DArray_New(D_STRING);
	argValues = DArray_New(D_STRING);
	DaoVmSpace_ParseArguments( self, ns, file, NULL, argNames, argValues );
	DaoVmSpace_AddPath( self, ns->path->mbs );
	DArray_PushFront( self->nameLoading, ns->name );
	DArray_PushFront( self->pathLoading, ns->path );
	if( DMap_Find( self->nsModules, ns->name ) == NULL ){
		MAP_Insert( self->nsModules, ns->name, ns );
		GC_IncRC( ns );
	}
	tm = FileChangedTime( ns->name->mbs );
	ns->time = tm;

	/* self->fileName may has been changed */
	res = DaoVmSpace_ReadSource( self, ns->name, self->mainSource );
	res = res && DaoProcess_Compile( vmp, ns, self->mainSource, 1 );
	if( res ) DaoVmSpace_ConvertArguments( ns, argNames, argValues );
	DArray_Delete( argNames );
	DArray_Delete( argValues );

	if( res == 0 ) return 0;

	name = DString_New(1);
	mainRoutine = ns->mainRoutine;
	DString_SetMBS( name, "main" );
	i = DaoNamespace_FindConst( ns, name );
	DString_Delete( name );

	ps = ns->argParams->items.items.pValue;
	N = ns->argParams->items.size;
	if( i >=0 ){
		DaoValue *value = DaoNamespace_GetConst( ns, i );
		if( value->type == DAO_FUNCTREE || value->type == DAO_ROUTINE ){
			meth = (DaoMethod*) DRoutine_Resolve( value, NULL, ps, N, DVM_CALL );
			unirout = (DRoutine*) meth;
		}
		if( meth == NULL ){
			DaoStream_WriteMBS( self->stdStream, "ERROR: invalid command line arguments.\n" );
			if( unirout && unirout->routHelp )
				DaoStream_WriteString( self->stdStream, unirout->routHelp );
			return 0;
		}
	}
	DaoVmSpace_ExeCmdArgs( self );
	/* always execute default ::main() routine first for initialization: */
	if( mainRoutine ){
		DaoProcess_PushRoutine( vmp, mainRoutine, NULL );
		DaoProcess_Execute( vmp );
	}
	/* check and execute explicitly defined main() routine  */
	if( meth != NULL ){
		if( DaoProcess_Call( vmp, meth, NULL, ps, N ) ){
			DaoStream_WriteMBS( self->stdStream, "ERROR: invalid command line arguments.\n" );
			if( unirout->routHelp ) DaoStream_WriteString( self->stdStream, unirout->routHelp );
			return 0;
		}
		DaoProcess_Execute( vmp );
	}
	if( ( self->options & DAO_EXEC_INTERUN ) && self->userHandler == NULL )
		DaoVmSpace_Interun( self, NULL );

	return 1;
}
static int DaoVmSpace_CompleteModuleName( DaoVmSpace *self, DString *fname )
{
	int slen = strlen( DAO_DLL_SUFFIX );
	int i, modtype = DAO_MODULE_NONE;
	size_t size;
	DString_ToMBS( fname );
	size = fname->size;
	if( size >6 && DString_FindMBS( fname, ".dao.o", 0 ) == size-6 ){
		DaoVmSpace_MakePath( self, fname, DAO_FILE_PATH, 1 );
		if( TestFile( self, fname ) ) modtype = DAO_MODULE_DAO_O;
	}else if( size >6 && DString_FindMBS( fname, ".dao.s", 0 ) == size-6 ){
		DaoVmSpace_MakePath( self, fname, DAO_FILE_PATH, 1 );
		if( TestFile( self, fname ) ) modtype = DAO_MODULE_DAO_S;
	}else if( size >4 && ( DString_FindMBS( fname, ".dao", 0 ) == size-4
				|| DString_FindMBS( fname, ".cgi", 0 ) == size-4 ) ){
		DaoVmSpace_MakePath( self, fname, DAO_FILE_PATH, 1 );
		if( TestFile( self, fname ) ) modtype = DAO_MODULE_DAO;
	}else if( size > slen && DString_FindMBS( fname, DAO_DLL_SUFFIX, 0 ) == size - slen ){
		modtype = DAO_MODULE_DLL;
#ifdef UNIX
		if( DString_FindMBS( fname, ".dll", 0 ) == size-4 )
			DString_Erase( fname, size-4, 4 );
#ifdef MAC_OSX
		if( DString_FindMBS( fname, ".dylib", 0 ) != size-6 )
			DString_AppendMBS( fname, ".dylib" );
#else
		if( DString_FindMBS( fname, ".so", 0 ) != size-3 )
			DString_AppendMBS( fname, ".so" );
#endif
#elif WIN32
		if( DString_FindMBS( fname, ".so", 0 ) == size-3 )
			DString_Erase( fname, size-3, 3 );
		if( DString_FindMBS( fname, ".dll", 0 ) != size-4 )
			DString_AppendMBS( fname, ".dll" );
#endif
		DaoVmSpace_MakePath( self, fname, DAO_FILE_PATH, 1 );
		if( TestFile( self, fname ) ) modtype = DAO_MODULE_DLL;
	}else{
		DString *fn = DString_New(1);
		DString *path = DString_New(1);
		DString *file = DString_New(1);
		size_t pos = fname->size;
		while( pos && (fname->mbs[pos-1] == '_' || isalnum( fname->mbs[pos-1] )) ) pos -= 1;
		if( pos && (fname->mbs[pos-1] == '/' || fname->mbs[pos-1] == '\\') ){
			DString_SubString( fname, path, 0, pos );
			DString_SubString( fname, file, pos, fname->size - pos );
		}else{
			DString_Assign( file, fname );
		}
		for(i=0; i<7; i++){
			if( i < DAO_MODULE_DLL ){
				DString_Assign( fn, fname );
			}else{
				if( strncmp( fname->mbs, "lib", 3 ) == 0 ) break;
				DString_Assign( fn, path );
				DString_AppendMBS( fn, daoDllPrefix[i] );
				DString_Append( fn, file );
			}
			DString_AppendMBS( fn, daoFileSuffix[i] );
			DaoVmSpace_MakePath( self, fn, DAO_FILE_PATH, 1 );
#if 0
			printf( "%s %s\n", fn->mbs, self->nameLoading->items.pString[0]->mbs );
#endif
			/* skip the current file: reason, example, in gsl_vector.dao:
			   load gsl_vector require gsl_complex, gsl_block;
			   which will allow searching for gsl_vector.so, gsl_vector.dylib or gsl_vector.dll. */
			if( self->nameLoading->size && DString_EQ( fn, self->nameLoading->items.pString[0] ) ) continue;
			if( TestFile( self, fn ) ){
				modtype = i+1;
				if( modtype > DAO_MODULE_DLL ) modtype = DAO_MODULE_DLL;
				DString_Assign( fname, fn );
				break;
			}
		}
		DString_Delete( fn );
		DString_Delete( path );
		DString_Delete( file );
	}
	return modtype;
}
static DaoNamespace* DaoVmSpace_LoadDaoByteCode( DaoVmSpace *self, DString *fname, int run )
{
	DaoStream_WriteMBS( self->stdStream, "ERROR: bytecode loader is not implemented.\n" );
	return NULL;
}
static DaoNamespace* DaoVmSpace_LoadDaoAssembly( DaoVmSpace *self, DString *fname, int run )
{
	DaoStream_WriteMBS( self->stdStream, "ERROR: assembly loader is not implemented.\n" );
	return NULL;
}
/* Loading module in Dao source file.
 * The first time the module is loaded:
 * (1) its implicit main (codes outside of any class and function) is executed;
 * (2) then, its explicit main that matches with "args" will be executed.
 * The next time the module is loaded:
 * (1) its implicit main is executed, IF run != 0; (mainly for IDE)
 * (2) its explicit main that matches with "args" will be executed. */
DaoNamespace* DaoVmSpace_LoadDaoModuleExt( DaoVmSpace *self, DString *libpath, DArray *args, int run )
{
	DString *source = NULL;
	DArray *argNames = NULL, *argValues = NULL;
	DaoNamespace *ns = NULL, *ns2 = NULL;
	DaoParser *parser = NULL;
	DaoProcess *process;
	DString name;
	DNode *node;
	ulong_t tm = 0;
	size_t i = DString_FindMBS( libpath, "/addpath.dao", 0 );
	size_t j = DString_FindMBS( libpath, "/delpath.dao", 0 );
	int bl, m;
	int cfgpath = i != MAXSIZE && i == libpath->size - 12;
	cfgpath = cfgpath || (j != MAXSIZE && j == libpath->size - 12);
	/*  XXX if cfgpath == true, only parsing? */

	if( args ){
		argNames = DArray_New(D_STRING);
		argValues = DArray_New(D_STRING);
	}

	DaoVmSpace_Lock( self );
	node = MAP_Find( self->nsModules, libpath );
	if( node ) ns = ns2 = (DaoNamespace*)node->value.pValue;
	DaoVmSpace_Unlock( self );

	tm = FileChangedTime( libpath->mbs );
	/* printf( "time = %lli,  %s  %p\n", tm, libpath->mbs, node ); */
	if( ns && ns->time >= tm ){
		if( args ) DaoVmSpace_ParseArguments( self, ns, NULL, args, argNames, argValues );
		if( run ) goto ExecuteImplicitMain;
		goto ExecuteExplicitMain;
	}

	source = DString_New(1);
	if( ! DaoVmSpace_ReadSource( self, libpath, source ) ) goto LaodingFailed;

	/*
	   printf("%p : loading %s\n", self, libpath->mbs );
	 */
	parser = DaoParser_New();
	DString_Assign( parser->fileName, libpath );
	parser->vmSpace = self;
	if( ! DaoParser_LexCode( parser, DString_GetMBS( source ), 1 ) ) goto LaodingFailed;

	ns = DaoNamespace_New( self, libpath->mbs );
	ns->time = tm;
	if( args ) DaoVmSpace_ParseArguments( self, ns, NULL, args, argNames, argValues );

	GC_ShiftRC( ns, ns2 );

	DaoVmSpace_Lock( self );
	node = MAP_Find( self->nsModules, libpath );
	MAP_Insert( self->nsModules, libpath, ns );
	DaoVmSpace_Unlock( self );

#if 0
	tok = parser->tokStr->items.pString;
	for( i=0; i<parser->tokStr->size; i++){
		node = MAP_Find( self->allTokens, tok[i] );
		if( node ){
			DArray_Append( ns->tokStr, node->key.pString );
		}else{
			MAP_Insert( self->allTokens, tok[i], 1 );
			DArray_Append( ns->tokStr, tok[i] );
		}
	}
#endif

	/*
	   printf("%p : parsing %s\n", self, libpath->mbs );
	 */
	DaoVmSpace_Lock( self );
	DArray_PushFront( self->nameLoading, ns->name );
	if( ns->path->size ) DArray_PushFront( self->pathLoading, ns->path );
	DaoVmSpace_Unlock( self );

	parser->nameSpace = ns;
	bl = DaoParser_ParseScript( parser );

	DaoVmSpace_Lock( self );
	if( ns->path->size ) DArray_PopFront( self->pathLoading );
	DArray_PopFront( self->nameLoading );
	DaoVmSpace_Unlock( self );

	if( ! bl ) goto LaodingFailed;
	if( ns->mainRoutine == NULL ) goto LaodingFailed;
	DString_SetMBS( ns->mainRoutine->routName, "::main" );
	if( args ){
		DaoVmSpace_ConvertArguments( ns, argNames, argValues );
		DArray_Delete( argNames );
		DArray_Delete( argValues );
		argNames = argValues = NULL;
	}

	DaoParser_Delete( parser );

ExecuteImplicitMain :
	if( ns->mainRoutine->vmCodes->size > 1 ){
		process = DaoVmSpace_AcquireProcess( self );
		DaoVmSpace_Lock( self );
		DArray_PushFront( self->nameLoading, ns->path );
		DArray_PushFront( self->pathLoading, ns->path );
		DaoVmSpace_Unlock( self );
		DaoProcess_PushRoutine( process, ns->mainRoutine, NULL );
		if( ! DaoProcess_Execute( process ) ){
			DaoVmSpace_ReleaseProcess( self, process );
			DaoVmSpace_Lock( self );
			DArray_PopFront( self->nameLoading );
			DArray_PopFront( self->pathLoading );
			DaoVmSpace_Unlock( self );
			goto LaodingFailed;
		}
		DaoVmSpace_ReleaseProcess( self, process );
		DaoVmSpace_Lock( self );
		DArray_PopFront( self->nameLoading );
		DArray_PopFront( self->pathLoading );
		DaoVmSpace_Unlock( self );
	}

ExecuteExplicitMain :
	name = DString_WrapMBS( "main" );
	m = DaoNamespace_FindConst( ns, & name );
	if( m >=0 ){
		DaoValue *value = DaoNamespace_GetConst( ns, m );
		if( argNames && argValues ){
			DaoVmSpace_ConvertArguments( ns, argNames, argValues );
			DArray_Delete( argNames );
			DArray_Delete( argValues );
			argNames = argValues = NULL;
		}
		if( value && value->type == DAO_ROUTINE ){
			int ret, N = ns->argParams->items.size;
			DaoValue **ps = ns->argParams->items.items.pValue;
			DaoRoutine *rout = & value->xRoutine;
			process = DaoVmSpace_AcquireProcess( self );
			ret = DaoProcess_Call( process, (DaoMethod*)rout, NULL, ps, N );
			if( ret == DAO_ERROR_PARAM ){
				DaoStream_WriteMBS( self->stdStream, "ERROR: invalid command line arguments.\n" );
				if( rout->routHelp ) DaoStream_WriteString( self->stdStream, rout->routHelp );
			}
			DaoVmSpace_ReleaseProcess( self, process );
			if( ret ) goto LaodingFailed;
		}
	}
	if( source ) DString_Delete( source );
	if( argNames ) DArray_Delete( argNames );
	if( argValues ) DArray_Delete( argValues );
	return ns;
LaodingFailed :
	if( source ) DString_Delete( source );
	if( argNames ) DArray_Delete( argNames );
	if( argValues ) DArray_Delete( argValues );
	if( parser ) DaoParser_Delete( parser );
	return 0;
}
DaoNamespace* DaoVmSpace_LoadDaoModule( DaoVmSpace *self, DString *libpath )
{
	return DaoVmSpace_LoadDaoModuleExt( self, libpath, NULL, 0 );
}

static void* DaoOpenDLL( const char *name );
static void* DaoGetSymbolAddress( void *handle, const char *name );

typedef int (*FuncType)( DaoVmSpace *, DaoNamespace * );

static DaoNamespace* DaoVmSpace_LoadDllModule( DaoVmSpace *self, DString *libpath )
{
	DaoNamespace *ns = NULL;
	DNode *node;
	FuncType funpter;
	void *handle;
	long *dhv;
	size_t i, retc;

	if( self->options & DAO_EXEC_SAFE ){
		DaoStream_WriteMBS( self->stdStream,
				"ERROR: not permitted to open shared library in safe running mode.\n" );
		return NULL;
	}
	DaoVmSpace_Lock( self );
	node = MAP_Find( self->nsModules, libpath );
	if( node ) ns = (DaoNamespace*) node->value.pValue;
	DaoVmSpace_Unlock( self );

	if( ns ) return ns;

	handle = DaoOpenDLL( libpath->mbs );
	if( ! handle ){
		DaoStream_WriteMBS( self->stdStream, "ERROR: unable to open the library file \"" );
		DaoStream_WriteMBS( self->stdStream, libpath->mbs );
		DaoStream_WriteMBS( self->stdStream, "\".\n");
		return 0;
	}
	dhv = (long*) DaoGetSymbolAddress( handle, "DaoH_Version" );
	funpter = (FuncType) DaoGetSymbolAddress( handle, "DaoOnLoad" );
	if( dhv == NULL && funpter ){
		DaoStream_WriteMBS( self->stdStream, "unable to find version number in the library.\n");
		return NULL;
	}else if( dhv && funpter == NULL ){
		DaoStream_WriteMBS( self->stdStream, "unable to find symbol DaoOnLoad in the library.\n");
		return NULL;
	}else if( dhv && *dhv != DAO_H_VERSION ){
		char buf[200];
		sprintf( buf, "ERROR: DaoH_Version not matching, require \"%i\", "
				"but find \"%li\" in the library (%s).\n", DAO_H_VERSION, *dhv, libpath->mbs );
		DaoStream_WriteMBS( self->stdStream, buf );
		return NULL;
	}

	ns = DaoNamespace_New( self, libpath->mbs );
	ns->libHandle = handle;
	GC_IncRC( ns );
	DaoVmSpace_Lock( self );
	MAP_Insert( self->nsModules, libpath, ns );
	DaoVmSpace_Unlock( self );

	i = DString_RFindChar( libpath, '/', -1 );
	if( i != MAXSIZE ) DString_Erase( libpath, 0, i+1 );
	i = DString_RFindChar( libpath, '\\', -1 );
	if( i != MAXSIZE ) DString_Erase( libpath, 0, i+1 );
	i = DString_FindChar( libpath, '.', 0 );
	if( i != MAXSIZE ) DString_Erase( libpath, i, -1 );
	/* printf( "%s\n", libpath->mbs ); */

	/* no warning or error for loading a C/C++ dynamic linking library
	   for solving symbols in Dao modules. */
	if( dhv == NULL && funpter == NULL ) return ns;

	DaoVmSpace_Lock( self );
	DArray_PushFront( self->nameLoading, ns->name );
	if( ns->path->size ) DArray_PushFront( self->pathLoading, ns->path );
	DaoVmSpace_Unlock( self );

	retc = (*funpter)( self, ns );

	DaoVmSpace_Lock( self );
	if( ns->path->size ) DArray_PopFront( self->pathLoading );
	DArray_PopFront( self->nameLoading );
	DaoVmSpace_Unlock( self );
	if( retc ){
		MAP_Erase( self->nsModules, ns->name );
		GC_DecRC( ns );
		return NULL;
	}
	return ns;
}
void DaoVmSpace_AddVirtualFile( DaoVmSpace *self, const char *file, const char *data )
{
	DNode *node;
	DString *fname = DString_New(1);
	DString *source = DString_New(1);
	DString_ToMBS( fname );
	DString_SetMBS( fname, "/@/" );
	DString_AppendMBS( fname, file );
	node = DMap_Find( self->vfiles, fname );
	if( node ){
		DString_AppendMBS( node->value.pString, data );
	}else{
		DString_ToMBS( source );
		DString_SetMBS( source, data );
		MAP_Insert( self->vfiles, fname, source );
	}
	DString_Delete( fname );
	DString_Delete( source );
}

/* base is assumed to be absolute, and path is assumed to be relative: */
void Dao_MakePath( DString *base, DString *path )
{
#ifdef WIN32
	DString_ChangeMBS( base, "\\", "/", 0 );
	DString_ChangeMBS( path, "\\", "/", 0 );
#endif
	while( DString_MatchMBS( path, " ^ %.%. / ", NULL, NULL ) ){
		if( DString_MatchMBS( base, " [^/] + ( / | ) $ ", NULL, NULL ) ){
			DString_ChangeMBS( path, " ^ %.%. / ", "", 1 );
			DString_ChangeMBS( base, " [^/] + ( / |) $ ", "", 0 );
		}else return;
	}
	if( base->size && path->size ){
		if( base->mbs[ base->size-1 ] != '/' && path->mbs[0] != '/' )
			DString_InsertChar( path, '/', 0 );
		DString_Insert( path, base, 0, 0, 0 );
	}
	DString_ChangeMBS( path, "/ %. /", "/", 0 );
}
void DaoVmSpace_MakePath( DaoVmSpace *self, DString *fname, int type, int check )
{
	size_t i;
	char *p;
	DString *path;

	DString_ToMBS( fname );
	DString_ChangeMBS( fname, "/ %s* %. %s* /", "/", 0 );
	DString_ChangeMBS( fname, "[^%./] + / %. %. /", "", 0 );
	/* erase the last '/' */
	if( fname->size && fname->mbs[ fname->size-1 ] =='/' ){
		fname->size --;
		fname->mbs[ fname->size ] = 0;
	}

	/* C:\dir\source.dao; /home/...  */
	if( fname->size >1 && ( fname->mbs[0]=='/' || fname->mbs[1]==':' ) ) return;

	while( ( p = strchr( fname->mbs, '\\') ) !=NULL ) *p = '/';

	path = DString_Copy( self->pathWorking );

	/* ./source.dao; ../../source.dao */
	if( strstr( fname->mbs, "./" ) !=NULL || strstr( fname->mbs, "../" ) !=NULL ){

		if( self->pathLoading->size ){
			DString_Assign( path, self->pathLoading->items.pString[0] );
			if( path->size ==0 ) goto FreeString;
		}else if( self->pathWorking->size==0 ) goto FreeString;

		Dao_MakePath( path, fname );
		goto FreeString;
	}

	for( i=0; i<self->pathLoading->size; i++){
		DString_Assign( path, self->pathLoading->items.pString[i] );
		if( path->size > 0 && path->mbs[ path->size-1 ] != '/' ) DString_AppendMBS( path, "/" );
		DString_Append( path, fname );
		/*
		   printf( "%s %s\n", self->pathLoading->items.pString[i]->mbs, path->mbs );
		 */
		if( TestPath( self, path, type ) ){
			DString_Assign( fname, path );
			goto FreeString;
		}
	}
	if( path->size > 0 && path->mbs[ path->size -1 ] != '/' ) DString_AppendMBS( path, "/" );
	DString_Append( path, fname );
	/* printf( "%s %s\n", path->mbs, path->mbs ); */
	if( ! check || TestPath( self, path, type ) ){
		DString_Assign( fname, path );
		goto FreeString;
	}
	for( i=0; i<self->pathSearching->size; i++){
		DString_Assign( path, self->pathSearching->items.pString[i] );
		DString_AppendMBS( path, "/" );
		DString_Append( path, fname );
		/*
		   printf( "%s %s\n", self->pathSearching->items.pString[i]->mbs, path->mbs );
		 */
		if( TestPath( self, path, type ) ){
			DString_Assign( fname, path );
			goto FreeString;
		}
	}
FreeString:
	DString_Delete( path );
}
void DaoVmSpace_SetPath( DaoVmSpace *self, const char *path )
{
	char *p;
	DString_SetMBS( self->pathWorking, path );
	while( ( p = strchr( self->pathWorking->mbs, '\\') ) !=NULL ) *p = '/';
}
void DaoVmSpace_AddPath( DaoVmSpace *self, const char *path )
{
	DString *tmp, *pstr = DString_New(1);
	char *p;

	DString_SetMBS( pstr, path );
	while( ( p = strchr( pstr->mbs, '\\') ) !=NULL ) *p = '/';
	DaoVmSpace_MakePath( self, pstr, DAO_DIR_PATH, 1 );

	if( pstr->mbs[pstr->size-1] == '/' ) DString_Erase( pstr, pstr->size-1, 1 );

	if( Dao_IsDir( pstr->mbs ) ){
		tmp = self->pathWorking;
		self->pathWorking = pstr;
		DArray_PushFront( self->pathSearching, pstr );
		DString_AppendMBS( pstr, "/addpath.dao" );
		if( TestFile( self, pstr ) ) DaoVmSpace_LoadDaoModuleExt( self, pstr, NULL, 0 );
		self->pathWorking = tmp;
	}
	DString_Delete( pstr );
	/*
	   for(i=0; i<self->pathSearching->size; i++ )
	   printf( "%s\n", self->pathSearching->items.pString[i]->mbs );
	 */
}
void DaoVmSpace_DelPath( DaoVmSpace *self, const char *path )
{
	DString *pstr;
	char *p;
	int i, id = -1;

	pstr = DString_New(1);
	DString_SetMBS( pstr, path );
	while( ( p = strchr( pstr->mbs, '\\') ) !=NULL ) *p = '/';
	DaoVmSpace_MakePath( self, pstr, DAO_DIR_PATH, 1 );

	if( pstr->mbs[pstr->size-1] == '/' ) DString_Erase( pstr, pstr->size-1, 1 );

	for(i=0; i<self->pathSearching->size; i++ ){
		if( DString_Compare( pstr, self->pathSearching->items.pString[i] ) == 0 ){
			id = i;
			break;
		}
	}
	if( id >= 0 ){
		DString *pathDao = DString_Copy( pstr );
		DString *tmp = self->pathWorking;
		self->pathWorking = pstr;
		DString_AppendMBS( pathDao, "/delpath.dao" );
		if( TestFile( self, pathDao ) ){
			DaoVmSpace_LoadDaoModuleExt( self, pathDao, NULL, 0 );
			/* id may become invalid after loadDaoModule(): */
			id = -1;
			for(i=0; i<self->pathSearching->size; i++ ){
				if( DString_Compare( pstr, self->pathSearching->items.pString[i] ) == 0 ){
					id = i;
					break;
				}
			}
		}
		DArray_Erase( self->pathSearching, id, 1 );
		DString_Delete( pathDao );
		self->pathWorking = tmp;
	}
	DString_Delete( pstr );
}
const char* DaoVmSpace_CurrentWorkingPath( DaoVmSpace *self )
{
	return self->pathWorking->mbs;
}
const char* DaoVmSpace_CurrentLoadingPath( DaoVmSpace *self )
{
	if( self->pathLoading->size ==0 ) return NULL;
	return self->pathLoading->items.pString[0]->mbs;
}

extern DaoTypeBase libStandardTyper;
extern DaoTypeBase thdMasterTyper;
extern DaoTypeBase vmpTyper;

extern DaoTypeBase DaoFdSet_Typer;

extern void DaoInitLexTable();

static void DaoConfigure_FromFile( const char *name )
{
	double number;
	int i, ch, isnum, isint, integer=0, yes;
	FILE *fin = fopen( name, "r" );
	DaoToken *tk1, *tk2;
	DString *mbs;
	DArray *tokens;
	if( fin == NULL ) return;
	mbs = DString_New(1);
	tokens = DArray_New( D_TOKEN );
	while( ( ch=getc(fin) ) != EOF ) DString_AppendChar( mbs, ch );
	fclose( fin );
	DString_ToLower( mbs );
	DaoToken_Tokenize( tokens, mbs->mbs, 1, 0, 0 );
	i = 0;
	while( i < tokens->size ){
		tk1 = tokens->items.pToken[i];
		/* printf( "%s\n", tk1->string->mbs ); */
		if( tk1->type == DTOK_IDENTIFIER ){
			if( i+2 >= tokens->size ) goto InvalidConfig;
			if( tokens->items.pToken[i+1]->type != DTOK_ASSN ) goto InvalidConfig;
			tk2 = tokens->items.pToken[i+2];
			isnum = isint = 0;
			yes = -1;
			if( tk2->type >= DTOK_DIGITS_HEX && tk2->type <= DTOK_NUMBER_SCI ){
				isnum = 1;
				if( tk2->type <= DTOK_NUMBER_HEX ){
					isint = 1;
					number = integer = strtol( tk2->string->mbs, NULL, 0 );
				}else{
					number = strtod( tk2->string->mbs, NULL );
				}
			}else if( tk2->type == DTOK_IDENTIFIER ){
				if( TOKCMP( tk2, "yes" )==0 )  yes = 1;
				if( TOKCMP( tk2, "no" )==0 ) yes = 0;
			}
			if( TOKCMP( tk1, "cpu" )==0 ){
				/* printf( "%s  %i\n", tk2->string->mbs, tk2->type ); */
				if( isint == 0 ) goto InvalidConfigValue;
				daoConfig.cpu = integer;
			}else if( TOKCMP( tk1, "jit" )==0 ){
				if( yes <0 ) goto InvalidConfigValue;
				daoConfig.jit = yes;
			}else if( TOKCMP( tk1, "safe" )==0 ){
				if( yes <0 ) goto InvalidConfigValue;
				daoConfig.safe = yes;
			}else if( TOKCMP( tk1, "typedcode" )==0 ){
				if( yes <0 ) goto InvalidConfigValue;
				daoConfig.typedcode = yes;
			}else if( TOKCMP( tk1, "incompile" )==0 ){
				if( yes <0 ) goto InvalidConfigValue;
				daoConfig.incompile = yes;
			}else if( TOKCMP( tk1, "mbs" )==0 ){
				if( yes <0 ) goto InvalidConfigValue;
				daoConfig.mbs = yes;
			}else if( TOKCMP( tk1, "wcs" )==0 ){
				if( yes <0 ) goto InvalidConfigValue;
				daoConfig.wcs = yes;
			}else{
				goto InvalidConfigName;
			}
			i += 3;
			continue;
		}else if( tk1->type == DTOK_COMMA || tk1->type == DTOK_SEMCO ){
			i ++;
			continue;
		}
InvalidConfig :
		printf( "error: invalid configuration file format at line: %i!\n", tk1->line );
		break;
InvalidConfigName :
		printf( "error: invalid configuration option name: %s!\n", tk1->string->mbs );
		break;
InvalidConfigValue :
		printf( "error: invalid configuration option value: %s!\n", tk2->string->mbs );
		break;
	}
	DArray_Delete( tokens );
	DString_Delete( mbs );
}
static void DaoConfigure()
{
	char *daodir = getenv( "DAO_DIR" );
	DString *mbs = DString_New(1);

	DaoInitLexTable();
	daoConfig.iscgi = getenv( "GATEWAY_INTERFACE" ) ? 1 : 0;

	DString_SetMBS( mbs, DAO_DIR );
	DString_AppendMBS( mbs, "/dao.conf" );
	DaoConfigure_FromFile( mbs->mbs );
	if( daodir ){
		DString_SetMBS( mbs, daodir );
		if( daodir[ mbs->size-1 ] == '/' ){
			DString_AppendMBS( mbs, "dao.conf" );
		}else{
			DString_AppendMBS( mbs, "/dao.conf" );
		}
		DaoConfigure_FromFile( mbs->mbs );
	}
	DaoConfigure_FromFile( "./dao.conf" );
	DString_Delete( mbs );
}

#ifdef DEBUG
static void dao_FakeList_FakeList( DaoProcess *_proc, DaoValue *_p[], int _n );
static void dao_FakeList_Size( DaoProcess *_proc, DaoValue *_p[], int _n );
static void dao_FakeList_GetItem( DaoProcess *_proc, DaoValue *_p[], int _n );
static void dao_FakeList_SetItem( DaoProcess *_proc, DaoValue *_p[], int _n );

#define FakeListName "FakeList<@T<short|int|float>=int,@S=int>"

static DaoFuncItem dao_FakeList_Meths[] = 
{
	/* the names of allocators must be identical to the typer name: */
	{ dao_FakeList_FakeList, FakeListName "( size=0 )" },
	{ dao_FakeList_Size, "size( self :FakeList )=>int" },
	{ dao_FakeList_GetItem, "[]( self :FakeList<@T<short|int|float>>, index :int )=>int" },
	{ dao_FakeList_SetItem, "[]=( self :FakeList<@T<short|int|float>>, index :int, value :int )=>int" },
	{ NULL, NULL }
};
static void Dao_FakeList_Delete( void *self ){}
static DaoTypeBase FakeList_Typer = 
{ FakeListName, NULL, NULL, dao_FakeList_Meths, {0}, {0}, Dao_FakeList_Delete, NULL };
DaoTypeBase *dao_FakeList_Typer = & FakeList_Typer;

static void dao_FakeList_FakeList( DaoProcess *_proc, DaoValue *_p[], int _n )
{
  int size = _p[0]->xInteger.value;
  DaoType *retype = DaoProcess_GetReturnType( _proc );
  DaoCdata *cdata = DaoCdata_New( dao_FakeList_Typer, (void*)(size_t)size );
  printf( "retype = %s\n", retype->name->mbs );
  GC_ShiftRC( retype, cdata->ctype );
  cdata->ctype = retype;
  DaoProcess_PutValue( _proc, cdata );
}
static void dao_FakeList_Size( DaoProcess *_proc, DaoValue *_p[], int _n )
{
  dint size = (dint) DaoCdata_GetData( & _p[0]->xCdata );
  DaoProcess_PutInteger( _proc, size );
}
static void dao_FakeList_GetItem( DaoProcess *_proc, DaoValue *_p[], int _n )
{
  DaoProcess_PutInteger( _proc, 123 );
}
static void dao_FakeList_SetItem( DaoProcess *_proc, DaoValue *_p[], int _n )
{
}
#endif

extern void DaoType_Init();

DaoType *dao_type_udf = NULL;
DaoType *dao_type_any = NULL;
DaoType *dao_array_any = NULL;
DaoType *dao_array_empty = NULL;
DaoType *dao_list_any = NULL;
DaoType *dao_list_empty = NULL;
DaoType *dao_map_any = NULL;
DaoType *dao_map_empty = NULL;
DaoType *dao_map_meta = NULL;
DaoType *dao_routine = NULL;
DaoType *dao_class_any = NULL;
DaoType *dao_type_for_iterator = NULL;
DaoType *dao_access_enum = NULL;
DaoType *dao_storage_enum = NULL;
DaoType *dao_dynclass_field = NULL;
DaoType *dao_dynclass_method = NULL;

/* name:string,value:any,storage:enum<>,access:enum<> */
const char *field_typename = 
"tuple<string,any>|tuple<string,any,enum<const,global,var>>|"
"tuple<string,any,enum<const,global,var>,enum<private,protected,public>>>";

/* name:string,method:routine,access:enum<> */
const char *method_typename = 
"tuple<string,routine>|tuple<string,routine,enum<private,protected,public>>";


#ifdef DAO_WITH_THREAD
extern DMutex mutex_long_sharing;
extern DMutex mutex_string_sharing;
extern DMutex dao_vsetup_mutex;
extern DMutex dao_msetup_mutex;
extern DMutex dao_cdata_mutex;
#endif

#include<signal.h>
void print_trace();

extern DMap *dao_cdata_bindings;
extern DHash *dao_meta_tables;
extern DArray *dao_callback_data;

int DaoJIT_TryInit( DaoVmSpace *vms )
{
	void (*init)( DaoVmSpace*, DaoJIT* );
	char name[64];
	void *jitHandle;
	sprintf( name, "libDaoJIT%s", DAO_DLL_SUFFIX );
	jitHandle = DaoLoadLibrary( name );
	if( jitHandle == NULL ) return 0;
	init = (DaoJIT_InitFPT) DaoFindSymbol( jitHandle, "DaoJIT_Init" );
	if( init == NULL ) return 0;
	(*init)( vms, & dao_jit );
	dao_jit.Quit = (DaoJIT_QuitFPT) DaoFindSymbol( jitHandle, "DaoJIT_Quit" );
	dao_jit.Free = (DaoJIT_FreeFPT) DaoFindSymbol( jitHandle, "DaoJIT_Free" );
	dao_jit.Compile = (DaoJIT_CompileFPT) DaoFindSymbol( jitHandle, "DaoJIT_Compile" );
	dao_jit.Execute = (DaoJIT_ExecuteFPT) DaoFindSymbol( jitHandle, "DaoJIT_Execute" );
	if( dao_jit.Execute == NULL ) dao_jit.Compile = NULL;
	return dao_jit.Compile != NULL;
}

DaoVmSpace* DaoInit( const char *command )
{
	DString *mbs;
	DaoVmSpace *vms;
	DaoNamespace *ns;
	DaoType *type, *type1, *type2, *type3, *type4;
	char *daodir = getenv( "DAO_DIR" );
	int i;

	if( mainVmSpace ) return mainVmSpace;

	dao_cdata_bindings = DHash_New(0,0);
	dao_meta_tables = DHash_New(0,0);
	dao_callback_data = DArray_New(0);

	/* signal( SIGSEGV, print_trace ); */
	/* signal( SIGABRT, print_trace ); */

#ifdef DAO_WITH_THREAD
	DMutex_Init( & mutex_long_sharing );
	DMutex_Init( & mutex_string_sharing );
	DMutex_Init( & dao_vsetup_mutex );
	DMutex_Init( & dao_msetup_mutex );
	DMutex_Init( & dao_cdata_mutex );
#endif

	mbs = DString_New(1);
	setlocale( LC_CTYPE, "" );

	if( daodir == NULL && command ){
		int absolute = command[0] == '/';
		int relative = command[0] == '.';
#ifdef WIN32
		absolute = isalpha( command[0] ) && command[1] == ':';
#endif
		DString_SetMBS( mbs, command );
		if( relative ){
			DString *base = DString_New(1);
			char pwd[512];
			getcwd( pwd, 511 );
			DString_SetMBS( base, pwd );
			Dao_MakePath( base, mbs );
			DString_Delete( base );
		}
		while( (i = mbs->size) && mbs->mbs[i-1] != '/' && mbs->mbs[i-1] != '\\' ) mbs->size --;
		mbs->mbs[ mbs->size ] = 0;
		daodir = (char*) dao_malloc( mbs->size + 10 );
		strncpy( daodir, "DAO_DIR=", 9 );
		strncat( daodir, mbs->mbs, mbs->size );
		putenv( daodir );
	}

	DaoConfigure();
	DaoType_Init();
	/*
	   printf( "number of VM instructions: %i\n", DVM_NULL );
	 */

#ifdef DEBUG
	for(i=0; i<100; i++) ObjectProfile[i] = 0;
#endif

#ifdef DAO_WITH_THREAD
	DaoInitThread();
#endif

	DaoGC_Start();

	dao_type_udf = DaoType_New( "?", DAO_UDF, NULL, NULL );
	dao_type_any = DaoType_New( "any", DAO_ANY, NULL, NULL );
	dao_routine = DaoType_New( "routine<=>?>", DAO_ROUTINE, (DaoValue*)dao_type_udf, NULL );
	dao_class_any = DaoType_New( "class", DAO_CLASS, (DaoValue*)DaoClass_New(), NULL );

	mainVmSpace = vms = DaoVmSpace_New();
	vms->safeTag = 0;
	ns = vms->nsInternal;

	dao_type_for_iterator = DaoParser_ParseTypeName( "tuple<valid:int,iterator:any>", ns, NULL );
	dao_access_enum = DaoNamespace_MakeEnumType( ns, "private,protected,public" );
	dao_storage_enum = DaoNamespace_MakeEnumType( ns, "const,global,var"  );
	dao_dynclass_field = DaoParser_ParseTypeName( field_typename, ns, NULL );
	dao_dynclass_method = DaoParser_ParseTypeName( method_typename, ns, NULL );

	DString_SetMBS( dao_type_for_iterator->name, "for_iterator" );
	DaoNamespace_AddType( ns, dao_type_for_iterator->name, dao_type_for_iterator );

	dao_array_any = DaoParser_ParseTypeName( "array<any>", ns, NULL );
	dao_list_any = DaoParser_ParseTypeName( "list<any>", ns, NULL );
	dao_map_any = DaoParser_ParseTypeName( "map<any,any>", ns, NULL );
	dao_map_meta = DaoParser_ParseTypeName( "map<string,any>", ns, NULL );

#if 0
	dao_array_empty = DaoParser_ParseTypeName( "array<any>", ns, NULL );
	dao_list_empty = DaoParser_ParseTypeName( "list<any>", ns, NULL );
	dao_map_empty = DaoParser_ParseTypeName( "map<any,any>", ns, NULL );
#else
	dao_array_empty = DaoType_Copy( dao_array_any );
	dao_list_empty = DaoType_Copy( dao_list_any );
	dao_map_empty = DaoType_Copy( dao_map_any );
#endif
	DString_SetMBS( dao_array_empty->name, "array<>" );
	DString_SetMBS( dao_list_empty->name, "list<>" );
	DString_SetMBS( dao_map_empty->name, "map<>" );
	dao_array_empty->attrib |= DAO_TYPE_EMPTY;
	dao_list_empty->attrib |= DAO_TYPE_EMPTY;
	dao_map_empty->attrib |= DAO_TYPE_EMPTY;
	DaoNamespace_AddType( ns, dao_array_empty->name, dao_array_empty );
	DaoNamespace_AddType( ns, dao_list_empty->name, dao_list_empty );
	DaoNamespace_AddType( ns, dao_map_empty->name, dao_map_empty );

#ifdef DEBUG
	DaoNamespace_TypeDefine( ns, "int", "short" );
	DaoNamespace_WrapType( vms->nsInternal, dao_FakeList_Typer, 1 );
	DaoNamespace_TypeDefine( ns, "FakeList<short>", "FakeList<int>" );
#endif

#ifdef DAO_WITH_NUMARRAY
	DaoNamespace_SetupType( vms->nsInternal, & numarTyper );
#endif

	DaoNamespace_SetupType( vms->nsInternal, & stringTyper );
	DaoNamespace_SetupType( vms->nsInternal, & longTyper );
	DaoNamespace_SetupType( vms->nsInternal, & comTyper );
	DaoNamespace_SetupType( vms->nsInternal, & listTyper );
	DaoNamespace_SetupType( vms->nsInternal, & mapTyper );

	DaoNamespace_SetupType( vms->nsInternal, & streamTyper );
	type = DaoNamespace_MakeType( ns, "stream", DAO_STREAM, NULL, NULL, 0 );
	type->value = (DaoValue*) vms->stdStream;
	GC_IncRC( vms->stdStream );

	dao_default_cdata.ctype = DaoNamespace_WrapType( vms->nsInternal, & defaultCdataTyper, 0 );
	dao_default_cdata.ctype->cdatatype = DAO_CDATA_PTR;
	GC_IncRC( dao_default_cdata.ctype );

	DaoException_Setup( vms->nsInternal );

#ifdef DAO_WITH_CONCURRENT
	type2 = DaoNamespace_MakeType( ns, "mutex", DAO_MUTEX, NULL, NULL, 0 );
	type3 = DaoNamespace_MakeType( ns, "condition", DAO_CONDVAR, NULL, NULL, 0 );
	type4 = DaoNamespace_MakeType( ns, "semaphore", DAO_SEMA, NULL, NULL, 0 );
	type2->value = (DaoValue*) DaoMutex_New();
	type3->value = (DaoValue*) DaoCondVar_New();
	type4->value = (DaoValue*) DaoSema_New( 0 );
	GC_IncRC( type2->value );
	GC_IncRC( type3->value );
	GC_IncRC( type4->value );
	DaoNamespace_WrapType( ns, & thdMasterTyper, 1 );
	DaoNamespace_SetupType( ns, & mutexTyper );
	DaoNamespace_SetupType( ns, & condvTyper );
	DaoNamespace_SetupType( ns, & semaTyper );
	DaoNamespace_SetupType( ns, & futureTyper );
#endif
	DaoNamespace_SetupType( vms->nsInternal, & vmpTyper );
	DaoNamespace_WrapType( vms->nsInternal, & libStandardTyper, 1 );

	DaoNamespace_AddParent( vms->mainNamespace, vms->nsInternal );

	DaoVmSpace_InitPath( vms );
	/*
	   printf( "initialized...\n" );
	 */
	DString_Delete( mbs );
	vms->safeTag = 1;
	return vms;
}
extern DaoType* DaoParser_ParseTypeName( const char *type, DaoNamespace *ns, DaoClass *cls );
extern DaoType *simpleTypes[ DAO_ARRAY ];
void DaoQuit()
{
	int i;
	/* TypeTest(); */
#ifdef DAO_WITH_CONCURRENT
	DaoCallServer_Stop();
#endif

	if( daoConfig.iscgi ) return;

	GC_DecRC( dao_default_cdata.ctype );

	DaoVmSpace_Delete( mainVmSpace );
	for(i=0; i<DAO_ARRAY; i++){
		GC_DecRC( simpleTypes[i] );
		simpleTypes[i] = NULL;
	}
	DaoGC_Finish();
	DMap_Delete( dao_cdata_bindings );
	DMap_Delete( dao_meta_tables );
	DArray_Delete( dao_callback_data );
	dao_cdata_bindings = NULL;
	dao_meta_tables = NULL;
	dao_callback_data = NULL;
	mainVmSpace = NULL;
	mainProcess = NULL; 
	if( dao_jit.Quit ){
		dao_jit.Quit();
		dao_jit.Quit = NULL;
		dao_jit.Free = NULL;
		dao_jit.Compile = NULL;
		dao_jit.Execute = NULL;
	}
}
DaoNamespace* DaoVmSpace_FindModule( DaoVmSpace *self, DString *fname )
{
	DNode *node = MAP_Find( self->nsModules, fname );
	if( node ) return (DaoNamespace*) node->value.pValue;
	DaoVmSpace_CompleteModuleName( self, fname );
	node = MAP_Find( self->nsModules, fname );
	if( node ) return (DaoNamespace*) node->value.pValue;
	return NULL;
}
DaoNamespace* DaoVmSpace_LoadModule( DaoVmSpace *self, DString *fname )
{
	DaoNamespace *ns = NULL;
#ifdef DAO_WITH_THREAD
#endif
#if 0
	printf( "modtype = %i\n", modtype );
#endif
	switch( DaoVmSpace_CompleteModuleName( self, fname ) ){
	case DAO_MODULE_DAO_O : ns = DaoVmSpace_LoadDaoByteCode( self, fname, 1 ); break;
	case DAO_MODULE_DAO_S : ns = DaoVmSpace_LoadDaoAssembly( self, fname, 1 ); break;
	case DAO_MODULE_DAO : ns = DaoVmSpace_LoadDaoModule( self, fname ); break;
	case DAO_MODULE_DLL : ns = DaoVmSpace_LoadDllModule( self, fname ); break;
	}
#ifdef DAO_WITH_THREAD
#endif
	return ns;
}

#ifdef UNIX
#include<dlfcn.h>
#elif WIN32
#include<windows.h>
#endif

void DaoGetErrorDLL()
{
#ifdef UNIX
	printf( "%s\n", dlerror() );
#elif WIN32
	DWORD error = GetLastError();
	LPSTR message;
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, error, LANG_NEUTRAL, (LPTSTR)&message, 0, NULL );
	if( message ){
		printf( "%s\n", message );
		LocalFree( message );
	}
#endif
}

void* DaoOpenDLL( const char *name )
{
#ifdef UNIX
	void *handle = dlopen( name, RTLD_NOW | RTLD_GLOBAL );
#elif WIN32
	void *handle = LoadLibrary( name );
#endif
	if( !handle ){
		DaoGetErrorDLL();
		return 0;
	}
	return handle;
}
void* DaoGetSymbolAddress( void *handle, const char *name )
{
#ifdef UNIX
	void *sym = dlsym( handle, name );
#elif WIN32
	void *sym = (void*)GetProcAddress( (HMODULE)handle, name );
#endif
	return sym;
}
