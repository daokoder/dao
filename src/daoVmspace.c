/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2010, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

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
#include"daoContext.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoSched.h"
#include"daoAsmbc.h"

#ifdef DAO_WITH_THREAD
#include"daoThread.h"
#endif

#ifdef DAO_WITH_NETWORK
void DaoNetwork_Init( DaoVmSpace *vms, DaoNameSpace *ns );
extern DaoTypeBase libNetTyper;
#endif

extern int ObjectProfile[100];

DAO_DLL DaoAPI __dao;

DaoConfig daoConfig =
{
	1,    /*cpu*/
	1,    /*jit*/
	0,    /*safe*/
	1,    /*typedcode*/
	1,    /*incompile*/
	0,    /*iscgi*/
	8,    /*tabspace*/
	0     /*chindent*/
};

DaoVmSpace   *mainVmSpace = NULL;
DaoVmProcess *mainVmProcess = NULL;

static int TestPath( DaoVmSpace *vms, DString *fname );

extern ullong_t FileChangedTime( const char *file );

static const char* const daoFileSuffix[] = { ".dao.o", ".dao.s", ".dao", DAO_DLL_SUFFIX };
enum{
	DAO_MODULE_NONE,
	DAO_MODULE_DAO_O,
	DAO_MODULE_DAO_S,
	DAO_MODULE_DAO,
	DAO_MODULE_DLL
};

static const char *const copy_notice =
"\n  Dao Virtual Machine (" DAO_VERSION ", " __DATE__ ")\n"
"  Copyright(C) 2006-2010, Fu Limin.\n"
"  Dao can be copied under the terms of GNU Lesser General Public License.\n"
"  Dao Language website: http://www.daovm.net\n\n";

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
"   -T, --no-typed-code:  no typed VM codes;\n"
"   -J, --no-jit:         no just-in-time compiling;\n"
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
extern DaoTypeBase  pairTyper;
extern DaoTypeBase  streamTyper;
extern DaoTypeBase  routTyper;
extern DaoTypeBase  funcTyper;
extern DaoTypeBase  interTyper;
extern DaoTypeBase  classTyper;
extern DaoTypeBase  objTyper;
extern DaoTypeBase  nsTyper;
extern DaoTypeBase  cmodTyper;
extern DaoTypeBase  tupleTyper;

extern DaoTypeBase  numarTyper;
extern DaoTypeBase  comTyper;
extern DaoTypeBase  abstypeTyper;
extern DaoTypeBase  curryTyper;
extern DaoTypeBase  rgxMatchTyper;
extern DaoTypeBase  futureTyper;

extern DaoTypeBase mutexTyper;
extern DaoTypeBase condvTyper;
extern DaoTypeBase semaTyper;
extern DaoTypeBase threadTyper;
extern DaoTypeBase thdMasterTyper;

extern DaoTypeBase macroTyper;
extern DaoTypeBase regexTyper;
extern DaoTypeBase ctxTyper;
extern DaoTypeBase vmpTyper;
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
	case DAO_PAIR     :  return & pairTyper;
	case DAO_PAR_NAMED :  return & pairTyper;
	case DAO_TUPLE     : return & tupleTyper;
#ifdef DAO_WITH_NUMARRAY
	case DAO_ARRAY  :  return & numarTyper;
#else
	case DAO_ARRAY  :  return & baseTyper;
#endif
					   /*     case DAO_REGEX    :  return & regexTyper; // XXX */
	case DAO_FUNCURRY : return & curryTyper;
	case DAO_CDATA   :  return & cdataTyper;
	case DAO_ROUTINE   :  return & routTyper;
	case DAO_FUNCTION  :  return & funcTyper;
	case DAO_INTERFACE :  return & interTyper;
	case DAO_CLASS     :  return & classTyper;
	case DAO_OBJECT    :  return & objTyper;
	case DAO_STREAM    :  return & streamTyper;
	case DAO_NAMESPACE :  return & nsTyper;
	case DAO_CMODULE   :  return & cmodTyper;
	case DAO_CONTEXT   :  return & ctxTyper;
	case DAO_VMPROCESS :  return & vmpTyper;
	case DAO_VMSPACE   :  return & vmsTyper;
	case DAO_TYPE      :  return & abstypeTyper;
#ifdef DAO_WITH_MACRO
	case DAO_MACRO     :  return & macroTyper;
#endif
#ifdef DAO_WITH_THREAD
	case DAO_MUTEX     :  return & mutexTyper;
	case DAO_CONDVAR   :  return & condvTyper;
	case DAO_SEMA      :  return & semaTyper;
	case DAO_THREAD    :  return & threadTyper;
	case DAO_THDMASTER :  return & thdMasterTyper;
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
DaoNameSpace* DaoVmSpace_GetNameSpace( DaoVmSpace *self, const char *name )
{
	DaoNameSpace *ns;
	DString str = DString_WrapMBS( name );
	DNode *node = DMap_Find( self->nsModules, & str );
	if( node ) return (DaoNameSpace*) node->value.pBase;
	ns = DaoNameSpace_New( self );
	ns->refCount ++;
	DaoVmSpace_Lock( self );
	DMap_Insert( self->nsModules, & str, ns );
	DaoVmSpace_Unlock( self );
	return ns;
}
DaoNameSpace* DaoVmSpace_MainNameSpace( DaoVmSpace *self )
{
	return self->mainNamespace;
}
DaoVmProcess* DaoVmSpace_MainVmProcess( DaoVmSpace *self )
{
	return self->mainProcess;
}
DaoVmProcess* DaoVmSpace_AcquireProcess( DaoVmSpace *self )
{
	DaoVmProcess *proc = NULL;
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexProc );
#endif
	if( self->processes->size ){
		proc = DArray_Back( self->processes );
		DArray_PopBack( self->processes );
	}else{
		proc = DaoVmProcess_New( self );
		GC_IncRC( proc );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexProc );
#endif
	return proc;
}
void DaoVmSpace_ReleaseProcess( DaoVmSpace *self, DaoVmProcess *proc )
{
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexProc );
#endif
	DArray_PushBack( self->processes, proc );
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
	DaoVmSpace_AddPath( self, DAO_PATH );
	DaoVmSpace_AddPath( self, "~/dao" );
	if( daodir ) DaoVmSpace_AddPath( self, daodir );
}

static DaoTypeBase vmsTyper=
{
	"vmspace", NULL, NULL, NULL, {0},
	(FuncPtrDel) DaoVmSpace_Delete, NULL
};


DaoVmSpace* DaoVmSpace_New()
{
	DaoVmSpace *self = (DaoVmSpace*) dao_malloc( sizeof(DaoVmSpace) );
	DaoBase_Init( self, DAO_VMSPACE );
	self->stdStream = DaoStream_New();
	self->stdStream->vmSpace = self;
	self->source = DString_New(1);
	self->options = 0;
	self->stopit = 0;
	self->safeTag = 1;
	self->evalCmdline = 0;
	self->userHandler = NULL;
	self->vfiles = DMap_New(D_STRING,D_STRING);
	self->nsModules = DMap_New(D_STRING,0);
	self->modRequire = DMap_New(D_STRING,0);
	self->allTokens = DMap_New(D_STRING,0);
	self->fileName = DString_New(1);
	self->pathWorking = DString_New(1);
	self->nameLoading = DArray_New(D_STRING);
	self->pathLoading = DArray_New(D_STRING);
	self->pathSearching = DArray_New(D_STRING);
	self->processes = DArray_New(0);

	if( daoConfig.safe ) self->options |= DAO_EXEC_SAFE;

	self->thdMaster = NULL;
#ifdef DAO_WITH_THREAD
	self->thdMaster = DaoThdMaster_New();
	self->thdMaster->refCount ++;
	DMutex_Init( & self->mutexLoad );
	DMutex_Init( & self->mutexProc );
	self->locked = 0;
#endif

	self->nsInternal = NULL; /* need to be set for DaoNameSpace_New() */
	self->nsInternal = DaoNameSpace_New( self );
	self->nsInternal->vmSpace = self;
	self->nsInternal->refCount += 2;
	DString_SetMBS( self->nsInternal->name, "dao" );
	DMap_Insert( self->nsModules, self->nsInternal->name, self->nsInternal );

	self->mainNamespace = DaoNameSpace_New( self );
	self->mainNamespace->vmSpace = self;
	self->mainNamespace->refCount ++;
	DString_SetMBS( self->mainNamespace->name, "MainNameSpace" );
	self->stdStream->refCount ++;

	self->ReadLine = NULL;
	self->AddHistory = NULL;

	self->mainProcess = DaoVmProcess_New( self );
	GC_IncRC( self->mainProcess );

	if( mainVmSpace ){
		DaoNameSpace_Import( self->nsInternal, mainVmSpace->nsInternal, 0 );
	}else{
		DaoVmSpace_InitPath( self );
	}
	DString_Clear( self->source );

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
		printf( "%i  %i  %s\n", node->value.pBase->refCount,
				((DaoNameSpace*)node->value.pBase)->cmethods->size, node->key.pString->mbs );
#endif
		GC_DecRC( node->value.pBase );
	}
	GC_DecRC( self->nsInternal );
	GC_DecRC( self->mainNamespace );
	GC_DecRC( self->stdStream );
	GC_DecRC( self->thdMaster );
	GC_DecRCs( self->processes );
	DString_Delete( self->source );
	DString_Delete( self->fileName );
	DString_Delete( self->pathWorking );
	DArray_Delete( self->nameLoading );
	DArray_Delete( self->pathLoading );
	DArray_Delete( self->pathSearching );
	DArray_Delete( self->processes );
	DMap_Delete( self->vfiles );
	DMap_Delete( self->allTokens );
	DMap_Delete( self->nsModules );
	DMap_Delete( self->modRequire );
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
	if( self->locked ) return;
	DMutex_Lock( & self->mutexLoad );
	self->locked = 1;
#endif
}
void DaoVmSpace_Unlock( DaoVmSpace *self )
{
#ifdef DAO_WITH_THREAD
	if( self->locked ==0 ) return;
	self->locked = 0;
	DMutex_Unlock( & self->mutexLoad );
#endif
}
static int DaoVmSpace_ReadSource( DaoVmSpace *self, DString *fname )
{
	FILE *fin;
	char buf[IO_BUF_SIZE];
	DNode *node = MAP_Find( self->vfiles, fname );
	/* printf( "reading %s\n", fname->mbs ); */
	if( node ){
		DString_Assign( self->source, node->value.pString );
		return 1;
	}
	fin = fopen( fname->mbs, "r" );
	DString_Clear( self->source );
	if( ! fin ){
		DaoStream_WriteMBS( self->stdStream, "ERROR: can not open file \"" );
		DaoStream_WriteMBS( self->stdStream, fname->mbs );
		DaoStream_WriteMBS( self->stdStream, "\".\n" );
		return 0;
	}
	while(1){
		size_t count = fread( buf, 1, IO_BUF_SIZE, fin );
		if( count ==0 ) break;
		DString_AppendDataMBS( self->source, buf, count );
	}
	fclose( fin );
	return 1;
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
static void ParseScriptParameters( DString *str, DArray *tokens )
{
	size_t i, size = str->size;
	char quote = 0;
	const char *chs;
	DString *tok = DString_New(1);
	/* The shell may remove quotation marks, to use an arbitrary string as
	 * a parameter, the string should be enclosed insize \" \" with blackslashes.
	 * If the string contains ", it should be preceded by 3 blackslashes as,
	 * dao myscript.dao -m=\"just a message \\\"hello\\\".\"
	 */
	/*
	   printf( "2: %s\n", str->mbs );
	 */

	DArray_Clear( tokens );
	DString_ToMBS( str );
	chs = str->mbs;
	for( i=0; i<size; i++){
		if( chs[i] == '\'' || chs[i] == '"' ) quote = chs[i];
		if( quote ){
			if( tok->size > 0 ){
				DArray_Append( tokens, tok );
				DString_Clear( tok );
			}
			i ++;
			while( i<size && chs[i] != quote ){
				if( chs[i] == '\\' ){
					DString_AppendChar( tok, chs[++i] );
					i ++;
					continue;
				}
				DString_AppendChar( tok, chs[i++] );
			}
			DArray_Append( tokens, tok );
			DString_Clear( tok );
			quote = 0;
			continue;
		}else if( tokens->size && chs[i] == '\\' && i+1 < size ){
			/* Do NOT remove backslashes for script path:
			   dao C:\test\hello.dao ... */
			DString_AppendChar( tok, chs[i+1] );
			++i;
			continue;
		}else if( isspace( chs[i] ) || chs[i] == '=' ){
			if( tok->size > 0 ){
				DArray_Append( tokens, tok );
				DString_Clear( tok );
			}
			if( chs[i] == '=' ){
				DString_AppendChar( tok, chs[i] );
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

int DaoVmSpace_ParseOptions( DaoVmSpace *self, DString *options )
{
	DString *str = DString_New(1);
	DArray *array = DArray_New(D_STRING);
	size_t i, j;

	SplitByWhiteSpaces( options, array );
	for( i=0; i<array->size; i++ ){
		DString *token = array->items.pString[i];
		if( self->evalCmdline ){
			DString_Append( self->source, token );
			DString_AppendChar( self->source, ' ' );
			continue;
		}
		if( token->mbs[0] =='-' && token->size >1 && token->mbs[1] =='-' ){
			if( strcmp( token->mbs, "--help" ) ==0 ){
				self->options |= DAO_EXEC_HELP;
			}else if( strcmp( token->mbs, "--version" ) ==0 ){
				self->options |= DAO_EXEC_VINFO;
			}else if( strcmp( token->mbs, "--eval" ) ==0 ){
				self->evalCmdline = 1;
				DString_Clear( self->source );
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
			}else if( strcmp( token->mbs, "--no-typedcode" ) ==0 ){
				self->options |= DAO_EXEC_NO_JIT;
				daoConfig.jit = 0;
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
				case 'T' : self->options |= DAO_EXEC_NO_TC;
						   daoConfig.typedcode = 0;
						   break;
				case 'J' : self->options |= DAO_EXEC_NO_JIT;
						   daoConfig.jit = 0;
						   break;
				case 'e' : self->evalCmdline = 1;
						   DString_Clear( self->source );
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
	return 1;
}

static DValue DaoParseNumber( const char *s )
{
	DValue value = daoNullValue;
	if( strchr( s, 'e' ) != NULL ){
		value.t = DAO_FLOAT;
		value.v.f = strtod( s, 0 );
	}else if( strchr( s, 'E' ) != NULL ){
		value.t = DAO_DOUBLE;
		value.v.d = strtod( s, 0 );
	}else if( strchr( s, '.' ) != NULL ){
		int len = strlen( s );
		if( strstr( s, "00" ) == s + (len-2) ){
			value.t = DAO_DOUBLE;
			value.v.d = strtod( s, 0 );
		}else{
			value.t = DAO_FLOAT;
			value.v.f = strtod( s, 0 );
		}
	}else{
		value.t = DAO_INTEGER;
		value.v.i = strtod( s, 0 );
	}
	return value;
}

static int DaoVmSpace_CompleteModuleName( DaoVmSpace *self, DString *fname );

static DaoNameSpace*
DaoVmSpace_LoadDaoByteCode( DaoVmSpace *self, DString *fname, int run );

static DaoNameSpace*
DaoVmSpace_LoadDaoAssembly( DaoVmSpace *self, DString *fname, int run );

static DaoNameSpace*
DaoVmSpace_LoadDaoModuleExt( DaoVmSpace *self, DString *libpath, DArray *args );

int proxy_started = 0;

static void DaoVmSpace_ParseArguments( DaoVmSpace *self, DaoNameSpace *ns,
		DString *file, DArray *args, DArray *argNames, DArray *argValues )
{
	DaoType *nested[2];
	DaoList *argv = DaoList_New();
	DaoMap *cmdarg = DaoMap_New(0);
	DArray *array = args;
	DString *str = DString_New(1);
	DString *key = DString_New(1);
	DString *val = DString_New(1);
	DValue nkey = daoZeroInt;
	DValue skey = daoNullString;
	DValue sval = daoNullString;
	size_t i, pk;
	int tk, offset=0, eq=0;

	skey.v.s = key;
	sval.v.s = val;
	nested[0] = DaoNameSpace_MakeType( ns, "any", DAO_ANY, NULL,NULL,0 );
	nested[1] = DaoNameSpace_MakeType( ns, "string",DAO_STRING, NULL,NULL,0 );
	cmdarg->unitype = DaoNameSpace_MakeType( ns, "map",DAO_MAP,NULL,nested,2);
	argv->unitype = DaoNameSpace_MakeType( ns, "list",DAO_LIST,NULL,nested+1,1);
	GC_IncRC( cmdarg->unitype );
	GC_IncRC( argv->unitype );
	if( array == NULL && file ){
		array = DArray_New(D_STRING);
		SplitByWhiteSpaces( file, array );
		DString_Assign( self->fileName, array->items.pString[0] );
	}
	DString_Assign( val, array->items.pString[0] );
	DaoList_Append( argv, sval );
	DaoMap_Insert( cmdarg, nkey, sval );
	DaoVmSpace_MakePath( self, self->fileName, 1 );
	i = 1;
	while( i < array->size ){
		DString *s = array->items.pString[i];
		DString *name = NULL, *value = NULL;
		i ++;
		nkey.v.i ++;
		if( s->mbs[0] == '-' ){
			offset += 1;
			if( s->mbs[1] == '-' ) offset += 1;
		}
		tk = DaoToken_Check( s->mbs, s->size-offset, & eq );
		if( tk == DTOK_IDENTIFIER && s->mbs[eq+offset] == '=' ){
			DString_SubString( s, key, offset, eq-offset );
			DString_SubString( s, val, eq+1, s->size-eq );
			DArray_Append( argNames, key );
			DArray_Append( argValues, val );

			DaoList_Append( argv, sval );
			DaoMap_Insert( cmdarg, skey, sval );
			DString_SubString( s, key, 0, eq );
			DaoMap_Insert( cmdarg, skey, sval );
			DaoMap_Insert( cmdarg, nkey, sval );
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
	nkey.t = DAO_LIST;
	nkey.v.list = argv;
	DaoNameSpace_AddConst( ns, str, nkey, DAO_DATA_PUBLIC );
	if( ns == self->mainNamespace )
		DaoNameSpace_AddConst( self->nsInternal, str, nkey, DAO_DATA_PUBLIC );
	DString_SetMBS( str, "CMDARG" );
	nkey.t = DAO_MAP;
	nkey.v.map = cmdarg;
	DaoNameSpace_AddConst( ns, str, nkey, DAO_DATA_PUBLIC );
	if( ns == self->mainNamespace )
		DaoNameSpace_AddConst( self->nsInternal, str, nkey, DAO_DATA_PUBLIC );
	if( args == NULL ) DArray_Delete( array );
	DString_Delete( key );
	DString_Delete( val );
	DString_Delete( str );
}
static void DaoVmSpace_ConvertArguments( DaoNameSpace *ns, DArray *argNames, DArray *argValues )
{
	DaoRoutine *rout = ns->mainRoutine;
	DaoType *abtp = rout->routType;
	DString *key = DString_New(1);
	DString *val = DString_New(1);
	DString *str;
	DValue nkey = daoZeroInt;
	DValue skey = daoNullString;
	DValue sval = daoNullString;
	int i;
	skey.v.s = key;
	sval.v.s = val;
	DaoList_Clear( ns->argParams );
	DString_SetMBS( key, "main" );
	i = ns ? DaoNameSpace_FindConst( ns, key ) : -1;
	if( i >=0 ){
		DValue nkey = DaoNameSpace_GetConst( ns, i );
		/* It may has not been compiled if it is not called explicitly. */
		if( nkey.t == DAO_ROUTINE ){
			DaoRoutine_Compile( nkey.v.routine );
			rout = nkey.v.routine;
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
			if( abtp->nested->items.pBase[i] ){
				int k = abtp->nested->items.pType[i]->tid;
				char *chars = argValues->items.pString[i]->mbs;
				if( k == DAO_PAR_NAMED || k == DAO_PAR_DEFAULT )
					k = abtp->nested->items.pType[i]->X.abtype->tid;
				if( chars[0] == '+' || chars[0] == '-' ) chars ++;
				str = argNames->items.pString[i];
				if( str->size && abtp->mapNames ){
					DNode *node = MAP_Find( abtp->mapNames, str );
					if( node ){
						int id = node->value.pInt;
						k = abtp->nested->items.pType[id]->tid;
						if( k == DAO_PAR_NAMED || k == DAO_PAR_DEFAULT )
							k = abtp->nested->items.pType[id]->X.abtype->tid;
					}
				}
				if( k >0 && k <= DAO_DOUBLE && DaoToken_IsNumber( chars, 0 ) ){
					nkey = DaoParseNumber( chars );
				}
			}
		}
		if( argNames->items.pString[i]->size ){
			DValue vp = daoNullPair;
			DString_Assign( key, argNames->items.pString[i] );
			vp.v.pair = DaoPair_New( skey, nkey );
			vp.v.pair->trait |= DAO_DATA_CONST;
			vp.t = DAO_PAR_NAMED;
			DaoList_Append( ns->argParams, vp );
		}else{
			DaoList_Append( ns->argParams, nkey );
		}
	}
	DString_Delete( key );
	DString_Delete( val );
}

DaoNameSpace* DaoVmSpace_Load( DaoVmSpace *self, DString *file )
{
	DArray *args = DArray_New(D_STRING);
	DaoNameSpace *ns = NULL;
	int m;
	SplitByWhiteSpaces( file, args );
	DString_Assign( self->fileName, args->items.pString[0] );
	m = DaoVmSpace_CompleteModuleName( self, self->fileName );
	switch( m ){
	case DAO_MODULE_DAO_O :
		ns = DaoVmSpace_LoadDaoByteCode( self, self->fileName, 0 );
		break;
	case DAO_MODULE_DAO_S :
		ns = DaoVmSpace_LoadDaoAssembly( self, self->fileName, 0 );
		break;
	case DAO_MODULE_DAO :
		ns = DaoVmSpace_LoadDaoModuleExt( self, self->fileName, args );
		break;
	default :
		/* also allows execution of script files without suffix .dao */
		ns = DaoVmSpace_LoadDaoModuleExt( self, self->fileName, args );
		break;
	}
	DArray_Delete( args );
	if( ns == NULL ) return 0;
	return ns;
}

static void DaoVmSpace_Interun( DaoVmSpace *self, CallbackOnString callback )
{
	DString *input = DString_New(1);
	const char *varRegex = "^ %s* %w+ %s* $";
	const char *srcRegex = "^ %s* %w+ %. dao .* $";
	const char *sysRegex = "^ %\\ %s* %w+ %s* .* $";
	char *chs;
	int ch;
	DString_SetMBS( self->fileName, "interactive codes" );
	DString_SetMBS( self->mainNamespace->name, "interactive codes" );
	self->mainNamespace->options |= DAO_NS_AUTO_GLOBAL;
	while(1){
		DString_Clear( input );
		if( self->ReadLine ){
			chs = self->ReadLine( "(dao) " );
			if( chs ){
				DString_SetMBS( input, chs );
				DString_Trim( input );
				if( input->size && self->AddHistory ) self->AddHistory( chs );
				dao_free( chs );
			}
		}else{
			printf( "(dao) " );
			fflush( stdout );
			ch = getchar();
			if( ch == EOF ) break;
			while( ch != '\n' && ch != EOF ){
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
			DString_AppendMBS( input, ")" );
			if( callback ){
				(*callback)( input->mbs );
				continue;
			}
			DaoVmProcess_Eval( self->mainProcess, self->mainNamespace, input, 1 );
		}else if( DString_MatchMBS( input, varRegex, NULL, NULL ) ){
			DString_InsertMBS( input, "io.println(", 0, 0, 0 );
			DString_AppendMBS( input, ")" );
			if( callback ){
				(*callback)( input->mbs );
				continue;
			}
			DaoVmProcess_Eval( self->mainProcess, self->mainNamespace, input, 1 );
		}else{
			if( callback ){
				(*callback)( input->mbs );
				continue;
			}
			DaoVmProcess_Eval( self->mainProcess, self->mainNamespace, input, 1 );
		}
		/*
		   printf( "%s\n", input->mbs );
		 */
	}
	self->mainNamespace->options &= ~DAO_NS_AUTO_GLOBAL;
	DString_Delete( input );
}

static void DaoVmSpace_ExeCmdArgs( DaoVmSpace *self )
{
	DaoNameSpace *ns = self->mainNamespace;
	DaoRoutine *rout;
	size_t i, j, n;
	if( self->options & DAO_EXEC_VINFO ) DaoStream_WriteMBS( self->stdStream, copy_notice );
	if( self->options & DAO_EXEC_HELP )  DaoStream_WriteMBS( self->stdStream, cmd_help );
	DaoStream_Flush( self->stdStream );

	if( self->options & DAO_EXEC_LIST_BC ){
		for( i=ns->cstUser; i<ns->cstData->size; i++){
			DValue p = ns->cstData->data[i];
			if( p.t == DAO_ROUTINE && p.v.routine != ns->mainRoutine ){
				n = p.v.routine->routTable->size;
				for(j=0; j<n; j++){
					rout = (DaoRoutine*) p.v.routine->routTable->items.pBase[j];
					DaoRoutine_Compile( rout );
					DaoRoutine_PrintCode( rout, self->stdStream );
				}
			}else if( p.t == DAO_CLASS ){
				DaoClass_PrintCode( p.v.klass, self->stdStream );
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
	DaoNameSpace *ns = self->mainNamespace;
	DaoVmProcess *vmp = self->mainProcess;
	DaoContext *ctx = NULL;
	DaoRoutine *mainRoutine;
	DString *name;
	DArray *array;
	DArray *argNames;
	DArray *argValues;
	DValue value;
	DValue *ps;
	size_t N;
	int i, j, ch, res;

	if( file == NULL || file->size ==0 || self->evalCmdline ){
		DArray_PushFront( self->nameLoading, self->pathWorking );
		DArray_PushFront( self->pathLoading, self->pathWorking );
		if( self->evalCmdline ){
			DString_SetMBS( self->fileName, "command line codes" );
			DString_SetMBS( self->mainNamespace->name, "command line codes" );
			DaoVmProcess_Eval( vmp, ns, self->source, 1 );
			if( vmp->returned.t ){
				DaoContext *ctx = DaoVmProcess_MakeContext( vmp, ns->mainRoutine );
				DaoStream_WriteMBS( self->stdStream, "= " );
				DValue_Print( vmp->returned, ctx, self->stdStream, NULL );
				DaoStream_WriteNewLine( self->stdStream );
			}
		}
		DaoVmSpace_ExeCmdArgs( self );
		if( (self->options & DAO_EXEC_INTERUN) && self->userHandler == NULL )
			DaoVmSpace_Interun( self, NULL );
		return 1;
	}
	argNames = DArray_New(D_STRING);
	argValues = DArray_New(D_STRING);
	DaoVmSpace_ParseArguments( self, ns, file, NULL, argNames, argValues );
	DaoNameSpace_SetName( ns, self->fileName->mbs );
	DaoVmSpace_AddPath( self, ns->path->mbs );
	DArray_PushFront( self->nameLoading, ns->name );
	DArray_PushFront( self->pathLoading, ns->path );
	if( DMap_Find( self->nsModules, ns->name ) == NULL ){
		MAP_Insert( self->nsModules, ns->name, ns );
		GC_IncRC( ns );
	}

	/* self->fileName may has been changed */
	res = DaoVmSpace_ReadSource( self, ns->name );
	res = res && DaoVmProcess_Compile( vmp, ns, self->source, 1 );
	if( res ) DaoVmSpace_ConvertArguments( ns, argNames, argValues );
	DArray_Delete( argNames );
	DArray_Delete( argValues );

	if( res == 0 ) return 0;

	name = DString_New(1);
	mainRoutine = ns->mainRoutine;
	DString_SetMBS( name, "main" );
	i = DaoNameSpace_FindConst( ns, name );
	DString_Delete( name );

	ps = ns->argParams->items->data;
	N = ns->argParams->items->size;
	array = DArray_New(0);
	DArray_Resize( array, N, NULL );
	for(j=0; j<N; j++) array->items.pValue[j] = ps + j;
	if( i >=0 ){
		value = DaoNameSpace_GetConst( ns, i );
		if( value.t == DAO_ROUTINE ){
			mainRoutine = value.v.routine;
			ctx = DaoVmProcess_MakeContext( vmp, mainRoutine );
			ctx->vmSpace = self;
			DaoContext_Init( ctx, mainRoutine );
			if( DaoContext_InitWithParams( ctx, vmp, array->items.pValue, N ) == 0 ){
				DaoStream_WriteMBS( self->stdStream, "ERROR: invalid command line arguments.\n" );
				DaoStream_WriteString( self->stdStream, mainRoutine->routHelp );
				DArray_Delete( array );
				return 0;
			}
			DaoVmProcess_PushContext( vmp, ctx );
		}
		mainRoutine = ns->mainRoutine;
	}
	DaoVmSpace_ExeCmdArgs( self );
	/* always execute default ::main() routine first for initialization: */
	if( mainRoutine ){
		DaoVmProcess_PushRoutine( vmp, mainRoutine );
		DaoVmProcess_Execute( vmp );
	}
	/* check and execute explicitly defined main() routine  */
	if( i >=0 ){
		value = DaoNameSpace_GetConst( ns, i );
		if( value.t == DAO_ROUTINE ){
			if( ! DRoutine_PassParams( (DRoutine*)ctx->routine, NULL, ctx->regValues,
						array->items.pValue, NULL, N, 0 ) ){
				DaoStream_WriteMBS( self->stdStream, "ERROR: invalid command line arguments.\n" );
				DaoStream_WriteString( self->stdStream, ctx->routine->routHelp );
				DaoVmProcess_CacheContext( vmp, ctx );
				DArray_Delete( array );
				return 0;
			}
			DaoVmProcess_Execute( vmp );
		}
	}
	DArray_Delete( array );
	if( ( self->options & DAO_EXEC_INTERUN ) && self->userHandler == NULL )
		DaoVmSpace_Interun( self, NULL );

	return 1;
}
static int DaoVmSpace_CompleteModuleName( DaoVmSpace *self, DString *fname )
{
	int slen = strlen( DAO_DLL_SUFFIX );
	int i, modtype = DAO_MODULE_NONE;
	size_t k, k2, k3, size;
	DString_ToMBS( fname );
	size = fname->size;
	if( size >6 && DString_FindMBS( fname, ".dao.o", 0 ) == size-6 ){
		DaoVmSpace_MakePath( self, fname, 1 );
		if( TestPath( self, fname ) ) modtype = DAO_MODULE_DAO_O;
	}else if( size >6 && DString_FindMBS( fname, ".dao.s", 0 ) == size-6 ){
		DaoVmSpace_MakePath( self, fname, 1 );
		if( TestPath( self, fname ) ) modtype = DAO_MODULE_DAO_S;
	}else if( size >4 && ( DString_FindMBS( fname, ".dao", 0 ) == size-4
				|| DString_FindMBS( fname, ".cgi", 0 ) == size-4 ) ){
		DaoVmSpace_MakePath( self, fname, 1 );
		if( TestPath( self, fname ) ) modtype = DAO_MODULE_DAO;
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
		DaoVmSpace_MakePath( self, fname, 1 );
		if( TestPath( self, fname ) ) modtype = DAO_MODULE_DLL;
	}else{
		DString *fn = DString_New(1);
		for(i=0; i<4; i++){
			DString_Assign( fn, fname );
			DString_AppendMBS( fn, daoFileSuffix[i] );
			DaoVmSpace_MakePath( self, fn, 1 );
#if 0
			printf( "%s %s\n", fn->mbs, self->nameLoading->items.pString[0]->mbs );
#endif
			/* skip the current file: reason, example, in gsl_vector.dao:
			   load gsl_vector require gsl_complex, gsl_block;
			   which will allow searching for gsl_vector.so, gsl_vector.dylib or gsl_vector.dll. */
			if( self->nameLoading->size && DString_EQ( fn, self->nameLoading->items.pString[0] ) ) continue;
			if( TestPath( self, fn ) ){
				modtype = i+1;
				DString_Assign( fname, fn );
				break;
			}
		}
		DString_Delete( fn );
	}
	return modtype;
}
#ifdef DAO_WITH_ASMBC
static DaoNameSpace* DaoVmSpace_LoadDaoByteCode( DaoVmSpace *self, DString *fname, int run )
{
	DString *asmc = DString_New(1); /*XXX*/
	DaoNameSpace *ns;
	DNode *node;
	node = MAP_Find( self->nsModules, fname );
	if( node ) return (DaoNameSpace*) node->value.pBase;
	if( ! DaoVmSpace_ReadSource( self, fname ) ) return 0;
	ns = DaoNameSpace_New( self );
	if( DaoParseByteCode( self, ns, self->source, asmc ) ){
		if( run ){
			DaoVmProcess *vmProc = DaoVmProcess_New( self );
			GC_IncRC( vmProc );
			DaoVmProcess_PushRoutine( vmProc, ns->mainRoutine );
			if( ! DaoVmProcess_Execute( vmProc ) ){
				GC_DecRC( vmProc );
				return 0;
			}
			GC_DecRC( vmProc );
		}
		FILE *fout = fopen( "bytecodes.dao.s", "w" );
		int i = 0;
		for(i=0; i<asmc->size; i++) fprintf( fout, "%c", asmc->mbs[i] );
		fclose( fout );
		return ns;
	}
	DaoNameSpace_Delete( ns );
	return NULL;
}
static DaoNameSpace* DaoVmSpace_LoadDaoAssembly( DaoVmSpace *self, DString *fname, int run )
{
	DString *bc = DString_New(1);
	DaoNameSpace *ns;
	DNode *node;
	node = MAP_Find( self->nsModules, fname );
	if( node ) return (DaoNameSpace*) node->value.pBase;
	if( ! DaoVmSpace_ReadSource( self, fname ) ) return 0;
	ns = DaoNameSpace_New( self );
	if( DaoParseAssembly( self, ns, self->source, bc ) ){
		if( run ){
			DaoVmProcess *vmProc = DaoVmProcess_New( self );
			GC_IncRC( vmProc );
			DaoVmProcess_PushRoutine( vmProc, ns->mainRoutine );
			if( ! DaoVmProcess_Execute( vmProc ) ){
				GC_DecRC( vmProc );
				return 0;
			}
			GC_DecRC( vmProc );
		}
		FILE *fout = fopen( "bytecodes.dao.o", "w" );
		int i = 0;
		for(i=0; i<bc->size; i++) fprintf( fout, "%c", bc->mbs[i] );
		fclose( fout );
		return ns;
	}
	DaoNameSpace_Delete( ns );
	return NULL;
}
#else
static DaoNameSpace* DaoVmSpace_LoadDaoByteCode( DaoVmSpace *self, DString *fname, int run )
{
	DaoStream_WriteMBS( self->stdStream, "ERROR: bytecode loader is disabled.\n" );
	return NULL;
}
static DaoNameSpace* DaoVmSpace_LoadDaoAssembly( DaoVmSpace *self, DString *fname, int run )
{
	DaoStream_WriteMBS( self->stdStream, "ERROR: assembly loader is disabled.\n" );
	return NULL;
}
#endif
static DaoNameSpace* 
DaoVmSpace_LoadDaoModuleExt( DaoVmSpace *self, DString *libpath, DArray *args )
{
	DArray *argNames = NULL;
	DArray *argValues = NULL;
	DaoParser *parser;
	DaoVmProcess *vmProc;
	DaoNameSpace *ns;
	DString name;
	DNode *node;
	ullong_t tm = 0;
	size_t i = DString_FindMBS( libpath, "/addpath.dao", 0 );
	size_t j = DString_FindMBS( libpath, "/delpath.dao", 0 );
	int bl, m;
	int cfgpath = i != MAXSIZE && i == libpath->size - 12;
	cfgpath = cfgpath || (j != MAXSIZE && j == libpath->size - 12);
	/*  XXX if cfgpath == true, only parsing? */

	DString_SetMBS( self->fileName, libpath->mbs );
	if( ! DaoVmSpace_ReadSource( self, libpath ) ) return 0;

	/*
	   printf("%p : loading %s\n", self, libpath->mbs );
	 */
	parser = DaoParser_New();
	DString_Assign( parser->fileName, self->fileName );
	parser->vmSpace = self;
	if( ! DaoParser_LexCode( parser, DString_GetMBS( self->source ), 1 ) ) goto LaodingFailed;

	if( args ){
		argNames = DArray_New(D_STRING);
		argValues = DArray_New(D_STRING);
	}

	node = MAP_Find( self->nsModules, libpath );
	tm = FileChangedTime( libpath->mbs );
	/* printf( "time = %lli, %s\n", tm, libpath->mbs ); */
	if( node ){
		ns = (DaoNameSpace*)node->value.pBase;
		if( ns->time >= tm ){
			if( args ) DaoVmSpace_ParseArguments( self, ns, NULL, args, argNames, argValues );
			DaoParser_Delete( parser );
			goto ExecuteModule;
		}
	}

	ns = DaoNameSpace_New( self );
	ns->time = tm;
	/*DString_Assign( ns->source, self->source );*/
	if( args ) DaoVmSpace_ParseArguments( self, ns, NULL, args, argNames, argValues );

	GC_IncRC( ns );
	node = MAP_Find( self->nsModules, libpath );
	if( node ) GC_DecRC( node->value.pBase );
	MAP_Insert( self->nsModules, libpath, ns );

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
	DString_Assign( ns->name, libpath );
	DString_Assign( ns->file, libpath );
	DArray_PushFront( self->nameLoading, libpath );
	i = DString_RFindChar( libpath, '/', -1 );
	if( i != MAXSIZE ){
		DString_Erase( ns->file, 0, i+1 );
		DArray_PushFront( self->pathLoading, libpath );
		DString_Erase( self->pathLoading->items.pString[0], i, -1 );
		DString_Assign( ns->path, self->pathLoading->items.pString[0] );
	}
	parser->nameSpace = ns;
	bl = DaoParser_ParseScript( parser );
	if( i != MAXSIZE ){
		DArray_PopFront( self->pathLoading );
	}
	DArray_PopFront( self->nameLoading );
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
	if( ns->mainRoutine->vmCodes->size > 1 ){
		vmProc = DaoVmProcess_New( self );
		GC_IncRC( vmProc );
		DArray_PushFront( self->nameLoading, ns->path );
		DArray_PushFront( self->pathLoading, ns->path );
		DaoVmProcess_PushRoutine( vmProc, ns->mainRoutine );
		if( ! DaoVmProcess_Execute( vmProc ) ){
			GC_DecRC( vmProc );
			DArray_PopFront( self->nameLoading );
			DArray_PopFront( self->pathLoading );
			return 0;
		}
		GC_DecRC( vmProc );
		DArray_PopFront( self->nameLoading );
		DArray_PopFront( self->pathLoading );
	}

ExecuteModule :
	name = DString_WrapMBS( "main" );
	m = DaoNameSpace_FindConst( ns, & name );
	if( m >=0 ){
		DValue value = DaoNameSpace_GetConst( ns, m );
		if( argNames && argValues ){
			DaoVmSpace_ConvertArguments( ns, argNames, argValues );
			DArray_Delete( argNames );
			DArray_Delete( argValues );
			argNames = argValues = NULL;
		}
		if( value.t == DAO_ROUTINE ){
			int j, N = ns->argParams->items->size;
			DValue *ps = ns->argParams->items->data;
			DaoVmProcess *vmp = self->mainProcess;
			DaoRoutine *mainRoutine = value.v.routine;
			DaoContext *ctx = DaoVmProcess_MakeContext( vmp, mainRoutine );
			DArray *array = DArray_New(0);
			DArray_Resize( array, N, NULL );
			for(j=0; j<N; j++) array->items.pValue[j] = ps + j;
			ctx->vmSpace = self;
			DaoContext_Init( ctx, mainRoutine );
			if( DaoContext_InitWithParams( ctx, vmp, array->items.pValue, N ) == 0 ){
				DaoStream_WriteMBS( self->stdStream, "ERROR: invalid command line arguments.\n" );
				DaoStream_WriteString( self->stdStream, mainRoutine->routHelp );
				DArray_Delete( array );
				return 0;
			}
			DaoVmProcess_PushContext( vmp, ctx );
			if( ! DRoutine_PassParams( (DRoutine*)ctx->routine, NULL, ctx->regValues,
						array->items.pValue, NULL, N, 0 ) ){
				DaoStream_WriteMBS( self->stdStream, "ERROR: invalid command line arguments.\n" );
				DaoStream_WriteString( self->stdStream, ctx->routine->routHelp );
				DaoVmProcess_CacheContext( vmp, ctx );
				DArray_Delete( array );
				return 0;
			}
			DaoVmProcess_Execute( vmp );
			DArray_Delete( array );
		}
	}
	if( argNames ) DArray_Delete( argNames );
	if( argValues ) DArray_Delete( argValues );
	return ns;
LaodingFailed :
	if( argNames ) DArray_Delete( argNames );
	if( argValues ) DArray_Delete( argValues );
	DaoParser_Delete( parser );
	return 0;
}
DaoNameSpace* DaoVmSpace_LoadDaoModule( DaoVmSpace *self, DString *libpath )
{
	return DaoVmSpace_LoadDaoModuleExt( self, libpath, NULL );
}

static void* DaoOpenDLL( const char *name );
static void* DaoGetSymbolAddress( void *handle, const char *name );
DAO_DLL void DaoInitAPI( DaoAPI *api );

DaoNameSpace* DaoVmSpace_LoadDllModule( DaoVmSpace *self, DString *libpath, DArray *reqns )
{
	DaoNameSpace *ns;
	DaoAPI *api;
	DNode *node;
	typedef void (*FuncType)( DaoVmSpace *, DaoNameSpace * );
	void (*funpter)( DaoVmSpace *, DaoNameSpace * );
	void *handle;
	long *dhv;
	int i;

	if( self->options & DAO_EXEC_SAFE ){
		DaoStream_WriteMBS( self->stdStream,
				"ERROR: not permitted to open shared library in safe running mode.\n" );
		return NULL;
	}

	handle = DaoOpenDLL( libpath->mbs );
	if( ! handle ){
		DaoStream_WriteMBS( self->stdStream, "ERROR: unable to open the library file \"" );
		DaoStream_WriteMBS( self->stdStream, libpath->mbs );
		DaoStream_WriteMBS( self->stdStream, "\".\n");
		return 0;
	}

	node = MAP_Find( self->nsModules, libpath );
	if( node ){
		ns = (DaoNameSpace*) node->value.pBase;
		/* XXX dlclose(  ns->cmodule->libHandle ) */
		if( handle == ns->cmodule->libHandle ) return ns;
	}else{
		ns = DaoNameSpace_New( self );
		DString_Assign( ns->name, libpath );
		GC_IncRC( ns );
		MAP_Insert( self->nsModules, libpath, ns );
		i = DString_RFindChar( libpath, '/', -1 );
		if( i != MAXSIZE ) DString_Erase( libpath, 0, i+1 );
		i = DString_RFindChar( libpath, '\\', -1 );
		if( i != MAXSIZE ) DString_Erase( libpath, 0, i+1 );
		i = DString_FindChar( libpath, '.', 0 );
		if( i != MAXSIZE ) DString_Erase( libpath, i, -1 );
		/* printf( "%s\n", libpath->mbs ); */
		if( reqns ){
			for(i=0; i<reqns->size; i++){
				node = MAP_Find( self->modRequire, reqns->items.pString[i] );
				/* printf( "requiring:  %p  %s\n", node, reqns->items.pString[i]->mbs ); */
				/*
				if( node ) DaoNameSpace_Import( ns, (DaoNameSpace*)node->value.pBase, NULL );
				*/
				if( node ) DaoNameSpace_AddParent( ns, (DaoNameSpace*)node->value.pBase );
			}
		}
		/* MAP_Insert( self->modRequire, libpath, ns ); */
	}
	ns->cmodule->libHandle = handle;

	dhv = (long*) DaoGetSymbolAddress( handle, "DaoH_Version" );
	if( dhv == NULL ){
		/* no warning or error, for loading a C/C++ dynamic linking library
		   for solving symbols in Dao modules. */
		return ns;
	}else if( *dhv != DAO_H_VERSION ){
		char buf[200];
		sprintf( buf, "ERROR: DaoH_Version not matching, "
				"require \"%i\" but find \"%li\" in the library (%s).\n",
				DAO_H_VERSION, *dhv, libpath->mbs );
		DaoStream_WriteMBS( self->stdStream, buf );
		return 0;
	}
	api = (DaoAPI*) DaoGetSymbolAddress( handle, "__dao" );
	if( api == NULL ){
		DaoStream_WriteMBS( self->stdStream,
				"WARNING: Dao APIs are not available through wrapped interfaces.\n" );
	}else{
		DaoInitAPI( api );
	}

	funpter = (FuncType) DaoGetSymbolAddress( handle, "DaoOnLoad" );
	if( ! funpter ){
		DaoStream_WriteMBS( self->stdStream, "unable to find symbol DaoOnLoad in the library.\n");
		return 0;
	}
	(*funpter)( self, ns );
#if 0
	change to handle returned value of DaoOnLoad?
		if( bl ==0 ){
			MAP_Erase( self->nsModules, libpath );
			GC_DecRC( ns );
			return NULL;
		}
#endif
	return ns;
}
void DaoVmSpace_AddVirtualFile( DaoVmSpace *self, const char *file, const char *data )
{
	DNode *node;
	DString_ToMBS( self->fileName );
	DString_SetMBS( self->fileName, "/@/" );
	DString_AppendMBS( self->fileName, file );
	node = DMap_Find( self->vfiles, self->fileName );
	if( node ){
		DString_AppendMBS( node->value.pString, data );
	}else{
		DString_ToMBS( self->source );
		DString_SetMBS( self->source, data );
		MAP_Insert( self->vfiles, self->fileName, self->source );
	}
}

int TestPath( DaoVmSpace *vms, DString *fname )
{
	FILE *file;
	DNode *node = MAP_Find( vms->vfiles, fname );
	/* printf( "testing: %s  %p\n", fname->mbs, node ); */
	if( node ) return 1;
	file = fopen( fname->mbs, "r" );
	if( file == NULL ) return 0;
	fclose( file );
	return 1;
}
void Dao_MakePath( DString *base, DString *path )
{
	while( DString_MatchMBS( path, " ^ %.%. / ", NULL, NULL ) ){
		if( DString_MatchMBS( base, " [^/] + ( / | ) $ ", NULL, NULL ) ){
			DString_ChangeMBS( path, " ^ %.%. / ", "", 1, NULL, NULL );
			DString_ChangeMBS( base, " [^/] + ( / |) $ ", "", 0, NULL, NULL );
		}else return;
	}
	if( base->size && path->size ){
		if( base->mbs[ base->size-1 ] != '/' && path->mbs[0] != '/' )
			DString_InsertChar( path, '/', 0 );
		DString_Insert( path, base, 0, 0, 0 );
	}
	DString_ChangeMBS( path, "/ %. /", "/", 0, NULL, NULL );
}
void DaoVmSpace_MakePath( DaoVmSpace *self, DString *fname, int check )
{
	size_t i;
	char *p;
	DString *path;

	DString_ToMBS( fname );
	DString_ChangeMBS( fname, "/ %s* %. %s* /", "/", 0, NULL, NULL );
	DString_ChangeMBS( fname, "[^%./] + / %. %. /", "", 0, NULL, NULL );
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
		if( path->size > 0 && path->mbs[ path->size-1 ] != '/' )
			DString_AppendMBS( path, "/" );
		DString_Append( path, fname );
		/*
		   printf( "%s %s\n", self->pathLoading->items.pString[i]->mbs, path->mbs );
		 */
		if( TestPath( self, path ) ){
			DString_Assign( fname, path );
			goto FreeString;
		}
	}
	if( path->size > 0 && path->mbs[ path->size -1 ] != '/' )
		DString_AppendMBS( path, "/" );
	DString_Append( path, fname );
	/* printf( "%s %s\n", path->mbs, path->mbs ); */
	if( ! check || TestPath( self, path ) ){
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
		if( TestPath( self, path ) ){
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
	DString *pstr;
	int exist = 0;
	char *p;
	size_t i;

	pstr = DString_New(1);
	DString_SetMBS( pstr, path );
	while( ( p = strchr( pstr->mbs, '\\') ) !=NULL ) *p = '/';
	DaoVmSpace_MakePath( self, pstr, 1 );

	if( pstr->mbs[pstr->size-1] == '/' ) DString_Erase( pstr, pstr->size-1, 1 );

	for(i=0; i<self->pathSearching->size; i++ ){
		if( DString_Compare( pstr, self->pathSearching->items.pString[i] ) == 0 ){
			exist = 1;
			break;
		}
	}
	if( ! exist ){
		DString *tmp = self->pathWorking;
		self->pathWorking = pstr;
		DArray_PushFront( self->pathSearching, pstr );
		DString_AppendMBS( pstr, "/addpath.dao" );
		if( TestPath( self, pstr ) ){
			DaoVmSpace_LoadDaoModuleExt( self, pstr, NULL );
		}
		self->pathWorking = tmp;
	}
	/*
	   for(i=0; i<self->pathSearching->size; i++ )
	   printf( "%s\n", self->pathSearching->items.pString[i]->mbs );
	 */
	DString_Delete( pstr );
}
void DaoVmSpace_DelPath( DaoVmSpace *self, const char *path )
{
	DString *pstr;
	char *p;
	int i, id = -1;

	pstr = DString_New(1);
	DString_SetMBS( pstr, path );
	while( ( p = strchr( pstr->mbs, '\\') ) !=NULL ) *p = '/';
	DaoVmSpace_MakePath( self, pstr, 1 );

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
		if( TestPath( self, pathDao ) ){
			DaoVmSpace_LoadDaoModuleExt( self, pathDao, NULL );
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

static void DaoRoutine_GetSignature( DaoType *rt, DString *sig )
{
	DaoType *it;
	int i;
	DString_Clear( sig );
	DString_ToMBS( sig );
	for(i=((rt->attrib & DAO_ROUT_PARSELF)!=0); i<rt->nested->size; i++){
		it = rt->nested->items.pType[i];
		if( sig->size ) DString_AppendChar( sig, ',' );
		if( it->tid == DAO_PAR_NAMED || it->tid == DAO_PAR_DEFAULT ){
			DString_Append( sig, it->X.abtype->name );
		}else{
			DString_Append( sig, it->name );
		}
	}
}
void DaoTypeBase_Free( DaoTypeBase *typer )
{
	DMap *hs;
	DNode *it;
	if( typer->priv == NULL ) return;
	hs = typer->priv->methods;
	if( hs ){
		for( it=DMap_First(hs); it; it=DMap_Next(hs,it)){
			GC_DecRC( it->value.pBase );
		}
		DMap_Delete( hs );
	}
	if( typer->priv->values ) DMap_Delete( typer->priv->values );
	typer->priv->values = NULL;
	typer->priv->methods = NULL;
	if( typer->priv->attribs & DAO_TYPER_PRIV_FREE ){
		dao_free( typer->priv );
		typer->priv = NULL;
	}
}
extern DaoTypeBase libStandardTyper;
extern DaoTypeBase libSystemTyper;
extern DaoTypeBase libMathTyper;
extern DaoTypeBase libReflectTyper;
extern DaoTypeBase thdMasterTyper;
extern DaoTypeBase vmpTyper;
extern DaoTypeBase coroutTyper;
extern DaoTypeBase inodeTyper;

DaoClass *daoClassFutureValue = NULL;

extern DaoTypeBase DaoFdSet_Typer;

#ifndef DAO_WITH_THREAD
DaoMutex* DaoMutex_New( DaoVmSpace *vms ){ return NULL; }
void DaoMutex_Lock( DaoMutex *self ){}
void DaoMutex_Unlock( DaoMutex *self ){}
int DaoMutex_TryLock( DaoMutex *self ){ return 0; }
#endif

void DaoInitAPI( DaoAPI *api )
{
	if( api ==NULL ) return;
	memset( api, 0, sizeof( DaoAPI ) );

	api->DaoInit = DaoInit;
	api->DaoQuit = DaoQuit;
	api->DValue_NewInteger = DValue_NewInteger;
	api->DValue_NewFloat = DValue_NewFloat;
	api->DValue_NewDouble = DValue_NewDouble;
	api->DValue_NewMBString = DValue_NewMBString;
	api->DValue_NewWCString = DValue_NewWCString;
	api->DValue_NewVectorB = DValue_NewVectorB;
	api->DValue_NewVectorUB = DValue_NewVectorUB;
	api->DValue_NewVectorS = DValue_NewVectorS;
	api->DValue_NewVectorUS = DValue_NewVectorUS;
	api->DValue_NewVectorI = DValue_NewVectorI;
	api->DValue_NewVectorUI = DValue_NewVectorUI;
	api->DValue_NewVectorF = DValue_NewVectorF;
	api->DValue_NewVectorD = DValue_NewVectorD;
	api->DValue_NewMatrixB = DValue_NewMatrixB;
	api->DValue_NewMatrixUB = DValue_NewMatrixUB;
	api->DValue_NewMatrixS = DValue_NewMatrixS;
	api->DValue_NewMatrixUS = DValue_NewMatrixUS;
	api->DValue_NewMatrixI = DValue_NewMatrixI;
	api->DValue_NewMatrixUI = DValue_NewMatrixUI;
	api->DValue_NewMatrixF = DValue_NewMatrixF;
	api->DValue_NewMatrixD = DValue_NewMatrixD;
	api->DValue_NewBuffer = DValue_NewBuffer;
	api->DValue_NewStream = DValue_NewStream;
	api->DValue_NewCData = DValue_NewCData;
	api->DValue_WrapCData = DValue_WrapCData;
	api->DValue_Copy = DValue_Copy;
	api->DValue_Clear = DValue_Clear;
	api->DValue_ClearAll = DValue_ClearAll;

	api->DString_New = DString_New;
	api->DString_Delete = DString_Delete;

	api->DString_Size = DString_Size;
	api->DString_Clear = DString_Clear;
	api->DString_Resize = DString_Resize;

	api->DString_IsMBS = DString_IsMBS;
	api->DString_SetMBS = DString_SetMBS;
	api->DString_SetWCS = DString_SetWCS;
	api->DString_SetDataMBS = DString_SetDataMBS;
	api->DString_SetDataWCS = DString_SetDataWCS;
	api->DString_ToWCS = DString_ToWCS;
	api->DString_ToMBS = DString_ToMBS;
	api->DString_GetMBS = DString_GetMBS;
	api->DString_GetWCS = DString_GetWCS;

	api->DString_Erase = DString_Erase;
	api->DString_Insert = DString_Insert;
	api->DString_InsertMBS = DString_InsertMBS;
	api->DString_InsertChar = DString_InsertChar;
	api->DString_InsertWCS = DString_InsertWCS;
	api->DString_Append = DString_Append;
	api->DString_AppendChar = DString_AppendChar;
	api->DString_AppendWChar = DString_AppendWChar;
	api->DString_AppendMBS = DString_AppendMBS;
	api->DString_AppendWCS = DString_AppendWCS;
	api->DString_AppendDataMBS = DString_AppendDataMBS;
	api->DString_AppendDataWCS = DString_AppendDataWCS;

	api->DString_SubString = DString_SubString;

	api->DString_Find = DString_Find;
	api->DString_RFind = DString_RFind;
	api->DString_FindMBS = DString_FindMBS;
	api->DString_RFindMBS = DString_RFindMBS;
	api->DString_FindChar = DString_FindChar;
	api->DString_FindWChar = DString_FindWChar;
	api->DString_RFindChar = DString_RFindChar;

	api->DString_Copy = DString_Copy;
	api->DString_Assign = DString_Assign;
	api->DString_Compare = DString_Compare;

	api->DaoList_New = DaoList_New;
	api->DaoList_Size = DaoList_Size;
	api->DaoList_Front = DaoList_Front;
	api->DaoList_Back = DaoList_Back;
	api->DaoList_GetItem = DaoList_GetItem;

	api->DaoList_SetItem = DaoList_SetItem;
	api->DaoList_Insert = DaoList_Insert;
	api->DaoList_Erase = DaoList_Erase;
	api->DaoList_Clear = DaoList_Clear;
	api->DaoList_PushFront = DaoList_PushFront;
	api->DaoList_PushBack = DaoList_PushBack;
	api->DaoList_PopFront = DaoList_PopFront;
	api->DaoList_PopBack = DaoList_PopBack;

	api->DaoMap_New = DaoMap_New;
	api->DaoMap_Size = DaoMap_Size;
	api->DaoMap_Insert = DaoMap_Insert;
	api->DaoMap_Erase = DaoMap_Erase;
	api->DaoMap_Clear = DaoMap_Clear;
	api->DaoMap_InsertMBS = DaoMap_InsertMBS;
	api->DaoMap_InsertWCS = DaoMap_InsertWCS;
	api->DaoMap_EraseMBS = DaoMap_EraseMBS;
	api->DaoMap_EraseWCS = DaoMap_EraseWCS;
	api->DaoMap_GetValue = DaoMap_GetValue;
	api->DaoMap_GetValueMBS = DaoMap_GetValueMBS;
	api->DaoMap_GetValueWCS = DaoMap_GetValueWCS;
	api->DaoMap_First = DaoMap_First;
	api->DaoMap_Next = DaoMap_Next;
	api->DNode_Key = DNode_Key;
	api->DNode_Value = DNode_Value;

	api->DaoTuple_New = DaoTuple_New;
	api->DaoTuple_Size = DaoTuple_Size;
	api->DaoTuple_SetItem = DaoTuple_SetItem;
	api->DaoTuple_GetItem = DaoTuple_GetItem;

#ifdef DAO_WITH_NUMARRAY
	api->DaoArray_NumType = DaoArray_NumType;
	api->DaoArray_SetNumType = DaoArray_SetNumType;
	api->DaoArray_Size = DaoArray_Size;
	api->DaoArray_DimCount = DaoArray_DimCount;
	api->DaoArray_SizeOfDim = DaoArray_SizeOfDim;
	api->DaoArray_GetShape = DaoArray_GetShape;
	api->DaoArray_HasShape = DaoArray_HasShape;
	api->DaoArray_GetFlatIndex = DaoArray_GetFlatIndex;
	api->DaoArray_ResizeVector = DaoArray_ResizeVector;
	api->DaoArray_ResizeArray = DaoArray_ResizeArray;
	api->DaoArray_Reshape = DaoArray_Reshape;

	api->DaoArray_ToByte = DaoArray_ToByte;
	api->DaoArray_ToShort = DaoArray_ToShort;
	api->DaoArray_ToInt = DaoArray_ToInt;
	api->DaoArray_ToFloat = DaoArray_ToFloat;
	api->DaoArray_ToDouble = DaoArray_ToDouble;
	api->DaoArray_ToUByte = DaoArray_ToUByte;
	api->DaoArray_ToUShort = DaoArray_ToUShort;
	api->DaoArray_ToUInt = DaoArray_ToUInt;

	api->DaoArray_GetMatrixB = DaoArray_GetMatrixB;
	api->DaoArray_GetMatrixS = DaoArray_GetMatrixS;
	api->DaoArray_GetMatrixI = DaoArray_GetMatrixI;
	api->DaoArray_GetMatrixF = DaoArray_GetMatrixF;
	api->DaoArray_GetMatrixD = DaoArray_GetMatrixD;

	api->DaoArray_FromByte = DaoArray_FromByte;
	api->DaoArray_FromShort = DaoArray_FromShort;
	api->DaoArray_FromInt = DaoArray_FromInt;
	api->DaoArray_FromFloat = DaoArray_FromFloat;
	api->DaoArray_FromDouble = DaoArray_FromDouble;
	api->DaoArray_FromUByte = DaoArray_FromUByte;
	api->DaoArray_FromUShort = DaoArray_FromUShort;
	api->DaoArray_FromUInt = DaoArray_FromUInt;

	api->DaoArray_SetVectorB = DaoArray_SetVectorB;
	api->DaoArray_SetVectorS = DaoArray_SetVectorS;
	api->DaoArray_SetVectorI = DaoArray_SetVectorI;
	api->DaoArray_SetVectorF = DaoArray_SetVectorF;
	api->DaoArray_SetVectorD = DaoArray_SetVectorD;
	api->DaoArray_SetMatrixB = DaoArray_SetMatrixB;
	api->DaoArray_SetMatrixS = DaoArray_SetMatrixS;
	api->DaoArray_SetMatrixI = DaoArray_SetMatrixI;
	api->DaoArray_SetMatrixF = DaoArray_SetMatrixF;
	api->DaoArray_SetMatrixD = DaoArray_SetMatrixD;
	api->DaoArray_SetVectorUB = DaoArray_SetVectorUB;
	api->DaoArray_SetVectorUS = DaoArray_SetVectorUS;
	api->DaoArray_SetVectorUI = DaoArray_SetVectorUI;
	api->DaoArray_GetBuffer = DaoArray_GetBuffer;
	api->DaoArray_SetBuffer = DaoArray_SetBuffer;
#endif

	api->DaoObject_GetField = DaoObject_GetField;
	api->DaoObject_MapCData = DaoObject_MapCData;

	api->DaoStream_New = DaoStream_New;
	api->DaoStream_SetFile = DaoStream_SetFile;
	api->DaoStream_GetFile = DaoStream_GetFile;

	api->DaoFunction_Call = DaoFunction_Call;

	api->DaoCData_New = DaoCData_New;
	api->DaoCData_Wrap = DaoCData_Wrap;
	api->DaoCData_IsType = DaoCData_IsType;
	api->DaoCData_SetExtReference = DaoCData_SetExtReference;
	api->DaoCData_SetData = DaoCData_SetData;
	api->DaoCData_SetBuffer = DaoCData_SetBuffer;
	api->DaoCData_SetArray = DaoCData_SetArray;
	api->DaoCData_GetTyper = DaoCData_GetTyper;
	api->DaoCData_GetData = DaoCData_GetData;
	api->DaoCData_GetBuffer = DaoCData_GetBuffer;
	api->DaoCData_GetData2 = DaoCData_GetData2;
	api->DaoCData_GetObject = DaoCData_GetObject;

	api->DaoMutex_New = DaoMutex_New;
	api->DaoMutex_Lock = DaoMutex_Lock;
	api->DaoMutex_Unlock = DaoMutex_Unlock;
	api->DaoMutex_TryLock = DaoMutex_TryLock;

	api->DaoContext_PutInteger = DaoContext_PutInteger;
	api->DaoContext_PutFloat = DaoContext_PutFloat;
	api->DaoContext_PutDouble = DaoContext_PutDouble;
	api->DaoContext_PutComplex = DaoContext_PutComplex;
	api->DaoContext_PutMBString = DaoContext_PutMBString;
	api->DaoContext_PutWCString = DaoContext_PutWCString;
	api->DaoContext_PutString = DaoContext_PutString;
	api->DaoContext_PutBytes = DaoContext_PutBytes;
	api->DaoContext_PutEnum = DaoContext_PutEnum;
	api->DaoContext_PutArrayInteger = DaoContext_PutArrayInteger;
	api->DaoContext_PutArrayShort = DaoContext_PutArrayShort;
	api->DaoContext_PutArrayFloat = DaoContext_PutArrayFloat;
	api->DaoContext_PutArrayDouble = DaoContext_PutArrayDouble;
	api->DaoContext_PutArrayComplex = DaoContext_PutArrayComplex;
	api->DaoContext_PutList = DaoContext_PutList;
	api->DaoContext_PutMap = DaoContext_PutMap;
	api->DaoContext_PutArray = DaoContext_PutArray;
	api->DaoContext_PutFile = DaoContext_PutFile;
	api->DaoContext_PutCData = DaoContext_PutCData;
	api->DaoContext_PutCPointer = DaoContext_PutCPointer;
	api->DaoContext_PutResult = DaoContext_PutResult;
	api->DaoContext_WrapCData = DaoContext_WrapCData;
	api->DaoContext_CopyCData = DaoContext_CopyCData;
	api->DaoContext_PutValue = DaoContext_PutValue;
	api->DaoContext_RaiseException = DaoContext_RaiseException;

	api->DaoVmProcess_New = DaoVmProcess_New;
	api->DaoVmProcess_Compile = DaoVmProcess_Compile;
	api->DaoVmProcess_Eval = DaoVmProcess_Eval;
	api->DaoVmProcess_Call = DaoVmProcess_Call;
	api->DaoVmProcess_Stop = DaoVmProcess_Stop;
	api->DaoVmProcess_GetReturned = DaoVmProcess_GetReturned;

	api->DaoNameSpace_New = DaoNameSpace_New;
	api->DaoNameSpace_GetNameSpace = DaoNameSpace_GetNameSpace;
	api->DaoNameSpace_AddParent = DaoNameSpace_AddParent;
	api->DaoNameSpace_AddConstNumbers = DaoNameSpace_AddConstNumbers;
	api->DaoNameSpace_AddConstValue = DaoNameSpace_AddConstValue;
	api->DaoNameSpace_AddConstData = DaoNameSpace_AddConstData;
	api->DaoNameSpace_AddData = DaoNameSpace_AddData;
	api->DaoNameSpace_AddValue = DaoNameSpace_AddValue;
	api->DaoNameSpace_FindData = DaoNameSpace_FindData;

	api->DaoNameSpace_TypeDefine = DaoNameSpace_TypeDefine;
	api->DaoNameSpace_TypeDefines = DaoNameSpace_TypeDefines;
	api->DaoNameSpace_WrapType = DaoNameSpace_WrapType;
	api->DaoNameSpace_WrapTypes = DaoNameSpace_WrapTypes;
	api->DaoNameSpace_WrapFunction = DaoNameSpace_WrapFunction;
	api->DaoNameSpace_WrapFunctions = DaoNameSpace_WrapFunctions;
	api->DaoNameSpace_SetupType = DaoNameSpace_SetupType;
	api->DaoNameSpace_SetupTypes = DaoNameSpace_SetupTypes;
	api->DaoNameSpace_Load = DaoNameSpace_Load;
	api->DaoNameSpace_GetOptions = DaoNameSpace_GetOptions;
	api->DaoNameSpace_SetOptions = DaoNameSpace_SetOptions;

	api->DaoVmSpace_New = DaoVmSpace_New;
	api->DaoVmSpace_ParseOptions = DaoVmSpace_ParseOptions;
	api->DaoVmSpace_SetOptions = DaoVmSpace_SetOptions;
	api->DaoVmSpace_GetOptions = DaoVmSpace_GetOptions;

	api->DaoVmSpace_RunMain = DaoVmSpace_RunMain;
	api->DaoVmSpace_Load = DaoVmSpace_Load;
	api->DaoVmSpace_GetNameSpace = DaoVmSpace_GetNameSpace;
	api->DaoVmSpace_MainNameSpace = DaoVmSpace_MainNameSpace;
	api->DaoVmSpace_MainVmProcess = DaoVmSpace_MainVmProcess;
	api->DaoVmSpace_AcquireProcess = DaoVmSpace_AcquireProcess;
	api->DaoVmSpace_ReleaseProcess = DaoVmSpace_ReleaseProcess;

	api->DaoVmSpace_SetUserHandler = DaoVmSpace_SetUserHandler;
	api->DaoVmSpace_ReadLine = DaoVmSpace_ReadLine;
	api->DaoVmSpace_AddHistory = DaoVmSpace_AddHistory;

	api->DaoVmSpace_SetPath = DaoVmSpace_SetPath;
	api->DaoVmSpace_AddPath = DaoVmSpace_AddPath;
	api->DaoVmSpace_DelPath = DaoVmSpace_DelPath;

	api->DaoVmSpace_Stop = DaoVmSpace_Stop;

	api->DaoGC_IncRC = DaoGC_IncRC;
	api->DaoGC_DecRC = DaoGC_DecRC;
	api->DaoCallbackData_New = DaoCallbackData_New;
}
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
	daoConfig.cpu = daoConfig.jit = daoConfig.typedcode = daoConfig.incompile = 1;
	daoConfig.safe = 0;
	daoConfig.iscgi = getenv( "GATEWAY_INTERFACE" ) ? 1 : 0;

	DString_SetMBS( mbs, DAO_PATH );
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

extern void DaoJitMapper_Init();
extern void DaoType_Init();

DaoType *dao_type_udf = NULL;
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

#ifdef DAO_WITH_THREAD
extern DMutex mutex_string_sharing;
extern DMutex dao_typing_mutex;
extern DMutex dao_vsetup_mutex;
extern DMutex dao_msetup_mutex;
extern DMutex dao_cdata_mutex;
#endif

#include<signal.h>
void print_trace();

extern DMap *dao_cdata_bindings;
extern DArray *dao_callback_data;

DaoVmSpace* DaoInit()
{
	int i;
	DaoVmSpace *vms;
	DaoNameSpace *ns;
	DaoFunction *func;
	DString *mbs;

	if( mainVmSpace ) return mainVmSpace;

	dao_cdata_bindings = DHash_New(0,0);
	dao_callback_data = DArray_New(0);

	/* signal( SIGSEGV, print_trace ); */
	/* signal( SIGABRT, print_trace ); */

#ifdef DAO_WITH_THREAD
	DMutex_Init( & mutex_string_sharing );
	DMutex_Init( & dao_typing_mutex );
	DMutex_Init( & dao_vsetup_mutex );
	DMutex_Init( & dao_msetup_mutex );
	DMutex_Init( & dao_cdata_mutex );
#endif

	mbs = DString_New(1);
	setlocale( LC_CTYPE, "" );
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

#ifdef DAO_WITH_JIT
	DaoJitMapper_Init();
#endif

	DaoStartGC();

	dao_type_udf = DaoType_New( "?", DAO_UDF, NULL, NULL );
	dao_routine = DaoType_New( "routine<=>?>", DAO_ROUTINE, (DaoBase*)dao_type_udf, NULL );
	dao_class_any = DaoType_New( "class", DAO_CLASS, (DaoBase*)DaoClass_New(), NULL );

	mainVmSpace = vms = DaoVmSpace_New();
	vms->safeTag = 0;
	ns = vms->nsInternal;

	dao_type_for_iterator = DaoParser_ParseTypeName( "tuple<valid:int,iterator:any>", ns, 0,0 );
	dao_access_enum = DaoNameSpace_MakeEnumType( ns, "private,protected,public" );
	dao_storage_enum = DaoNameSpace_MakeEnumType( ns, "const,global,var"  );

	DString_SetMBS( dao_type_for_iterator->name, "for_iterator" );
	DaoNameSpace_AddType( ns, dao_type_for_iterator->name, dao_type_for_iterator );

	dao_array_any = DaoParser_ParseTypeName( "array<any>", ns, 0,0 );
	dao_list_any = DaoParser_ParseTypeName( "list<any>", ns, 0,0 );
	dao_map_any = DaoParser_ParseTypeName( "map<any,any>", ns, 0,0 );
	dao_map_meta = DaoParser_ParseTypeName( "map<string,any>", ns, 0,0 );

#if 0
	dao_array_empty = DaoParser_ParseTypeName( "array<any>", ns, 0,0 );
	dao_list_empty = DaoParser_ParseTypeName( "list<any>", ns, 0,0 );
	dao_map_empty = DaoParser_ParseTypeName( "map<any,any>", ns, 0,0 );
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
	DaoNameSpace_AddType( ns, dao_array_empty->name, dao_array_empty );
	DaoNameSpace_AddType( ns, dao_list_empty->name, dao_list_empty );
	DaoNameSpace_AddType( ns, dao_map_empty->name, dao_map_empty );

#if 0
	DString_SetMBS( vms->source, daoScripts );
	DString_SetMBS( vms->fileName, "internal scripts" );
	DaoVmProcess_Eval( vms->mainProcess, vms->nsInternal, vms->source, 0 );

	DString_SetMBS( mbs, "FutureValue" );
	daoClassFutureValue = DaoNameSpace_GetData( vms->nsInternal, mbs ).v.klass;
	GC_IncRC( daoClassFutureValue );
#endif

#ifdef DAO_WITH_NUMARRAY
	DaoNameSpace_SetupType( vms->nsInternal, & numarTyper );
#endif


	DaoNameSpace_SetupType( vms->nsInternal, & stringTyper );
	DaoNameSpace_SetupType( vms->nsInternal, & longTyper );
	DaoNameSpace_SetupType( vms->nsInternal, & comTyper );
	DaoNameSpace_SetupType( vms->nsInternal, & listTyper );
	DaoNameSpace_SetupType( vms->nsInternal, & mapTyper );

	DaoNameSpace_SetupType( vms->nsInternal, & streamTyper );
	DaoNameSpace_WrapType( vms->nsInternal, & cdataTyper );

	DaoException_Setup( vms->nsInternal );

#ifdef DAO_WITH_THREAD
	DaoNameSpace_MakeType( ns, "thread", DAO_THREAD, NULL, NULL, 0 );
	DaoNameSpace_MakeType( ns, "mtlib", DAO_THDMASTER, NULL, NULL, 0 );
	DaoNameSpace_MakeType( ns, "mutex", DAO_MUTEX, NULL, NULL, 0 );
	DaoNameSpace_MakeType( ns, "condition", DAO_CONDVAR, NULL, NULL, 0 );
	DaoNameSpace_MakeType( ns, "semaphore", DAO_SEMA, NULL, NULL, 0 );
	DaoNameSpace_SetupType( ns, & threadTyper );
	DaoNameSpace_SetupType( ns, & thdMasterTyper );
	DaoNameSpace_SetupType( ns, & mutexTyper );
	DaoNameSpace_SetupType( ns, & condvTyper );
	DaoNameSpace_SetupType( ns, & semaTyper );
	DaoNameSpace_SetupType( ns, & futureTyper );
#endif
	DaoNameSpace_SetupType( vms->nsInternal, & vmpTyper );
	DaoNameSpace_WrapType( vms->nsInternal, & coroutTyper );
	DaoNameSpace_WrapType( vms->nsInternal, & libStandardTyper );
	DaoNameSpace_WrapType( vms->nsInternal, & libSystemTyper );
	DaoNameSpace_WrapType( vms->nsInternal, & libMathTyper );
	DaoNameSpace_WrapType( vms->nsInternal, & libReflectTyper );
	DaoNameSpace_WrapType( vms->nsInternal, & inodeTyper );

#if( defined DAO_WITH_THREAD && defined DAO_WITH_SYNCLASS )
	DaoCallServer_Init( vms );
#endif

#ifdef DAO_WITH_NETWORK
	DaoNameSpace_WrapType( vms->nsInternal, & DaoFdSet_Typer );
	DaoNameSpace_WrapType( vms->nsInternal, & libNetTyper );
	DaoNetwork_Init( vms, vms->nsInternal );
#endif
	DaoNameSpace_Import( vms->mainNamespace, vms->nsInternal, NULL );

	DaoVmSpace_InitPath( vms );
	/*
	   printf( "initialized...\n" );
	 */
	DString_Delete( mbs );
	vms->safeTag = 1;
	return vms;
}
extern DaoType* DaoParser_ParseTypeName( const char *type, DaoNameSpace *ns, DaoClass *cls, DaoRoutine *rout );
extern DMap *dao_typing_cache;
void DaoQuit()
{
	int i;
	/* TypeTest(); */
#if( defined DAO_WITH_THREAD && defined DAO_WITH_SYNCLASS )
	DaoCallServer_Join( mainVmSpace );
#endif

#ifdef DAO_WITH_THREAD
	DaoStopThread( mainVmSpace->thdMaster );
#endif

	if( daoConfig.iscgi ) return;

#ifdef DAO_WITH_NUMARRAY
	DaoTypeBase_Free( & numarTyper );
#endif

	DaoTypeBase_Free( & stringTyper );
	DaoTypeBase_Free( & longTyper );
	DaoTypeBase_Free( & comTyper );
	DaoTypeBase_Free( & listTyper );
	DaoTypeBase_Free( & mapTyper );

	DaoTypeBase_Free( & streamTyper );

#ifdef DAO_WITH_THREAD
	DaoTypeBase_Free( & mutexTyper );
	DaoTypeBase_Free( & condvTyper );
	DaoTypeBase_Free( & semaTyper );
	DaoTypeBase_Free( & threadTyper );
	DaoTypeBase_Free( & thdMasterTyper );
#endif
	DaoTypeBase_Free( & vmpTyper );
	DaoTypeBase_Free( & coroutTyper );

	DaoTypeBase_Free( & libStandardTyper );
	DaoTypeBase_Free( & libMathTyper );
	DaoTypeBase_Free( & libReflectTyper );

#ifdef DAO_WITH_NETWORK
	/* DaoTypeBase_Free( & DaoFdSet_Typer ); */
	DaoTypeBase_Free( & libNetTyper );
#endif

	GC_DecRC( daoClassFutureValue );
	DaoException_CleanUp();

	/* 
	   DaoNameSpace *ns = mainVmSpace->mainNamespace;
	   printf( "%i  %p\n", ns->refCount, ns );
	   printf( "################# %i  %p\n", mainVmSpace->nsInternal->refCount, mainVmSpace->nsInternal );
	 */
	DaoVmSpace_Delete( mainVmSpace );
	DaoFinishGC();
	DMap_Delete( dao_typing_cache );
	DMap_Delete( dao_cdata_bindings );
	DArray_Delete( dao_callback_data );
	dao_typing_cache = NULL;
	dao_cdata_bindings = NULL;
	dao_callback_data = NULL;
	mainVmSpace = NULL;
	mainVmProcess = NULL; 
}
DaoNameSpace* DaoVmSpace_LoadModule( DaoVmSpace *self, DString *fname, DArray *reqns )
{
	DNode *node = MAP_Find( self->nsModules, fname );
	DaoNameSpace *ns = NULL;
	int modtype;
#ifdef DAO_WITH_THREAD
#endif
	if( node ) return (DaoNameSpace*) node->value.pBase;
	modtype = DaoVmSpace_CompleteModuleName( self, fname );
#if 0
	printf( "modtype = %i\n", modtype );
#endif
	if( modtype == DAO_MODULE_DAO_O )
		ns = DaoVmSpace_LoadDaoByteCode( self, fname, 1 );
	else if( modtype == DAO_MODULE_DAO_S )
		ns = DaoVmSpace_LoadDaoAssembly( self, fname, 1 );
	else if( modtype == DAO_MODULE_DAO )
		ns = DaoVmSpace_LoadDaoModule( self, fname );
	else if( modtype == DAO_MODULE_DLL )
		ns = DaoVmSpace_LoadDllModule( self, fname, reqns );
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
