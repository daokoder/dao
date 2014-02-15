/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2014, Limin Fu
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
#include<locale.h>
#include<assert.h>


#include"daoNamespace.h"
#include"daoVmspace.h"
#include"daoParser.h"
#include"daoBytecode.h"
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
#include"daoTasklet.h"
#include"daoValue.h"

#ifdef DAO_WITH_THREAD
#include"daoThread.h"
#endif

extern int ObjectProfile[100];

DaoConfig daoConfig =
{
	1, /*cpu*/
	0, /*jit*/
	1, /*typedcode*/
	1, /*optimize*/
	0, /*iscgi*/
	8, /*tabspace*/
	0, /*chindent*/
	0, /*mbs*/
	0  /*wcs*/
};

DaoVmSpace *mainVmSpace = NULL;
DaoProcess *mainProcess = NULL;


static int TestFile( DaoVmSpace *vms, DString *fname )
{
	DString_ToMBS( fname );
	if( MAP_Find( vms->vfiles, fname ) ) return 1;
	if( MAP_Find( vms->vmodules, fname ) ) return 1;
	return Dao_IsFile( fname->mbs );
}
static int TestPath( DaoVmSpace *vms, DString *fname, int type )
{
	DString_ToMBS( fname );
	if( type == DAO_FILE_PATH ) return TestFile( vms, fname );
	return Dao_IsDir( fname->mbs );
}
void DaoAux_Delete( DMap *aux )
{
	DNode *it;
	typedef void (*aux_delete)(void*);
	for(it=DMap_First(aux); it; it=DMap_Next(aux,it)){
		aux_delete del = (aux_delete) it->key.pVoid;
		(*del)( it->value.pVoid );
	}
	DMap_Delete( aux );
}


#define DAO_FILE_TYPE_NUM  3

static const char* const daoDllPrefix[] =
{
	"", "", DAO_DLL_PREFIX "dao_"
};
static const char* const daoFileSuffix[] =
{
	".dac", ".dao", DAO_DLL_SUFFIX
};
static int daoModuleTypes[] =
{
	DAO_MODULE_DAC, DAO_MODULE_DAO, DAO_MODULE_DLL
};

#ifndef CHANGESET_ID
#define CHANGESET_ID "Undefined"
#endif

const char *const dao_copy_notice =
"  Dao Virtual Machine " DAO_VERSION "\n"
"  Built date: " __DATE__ "\n"
"  Changeset ID: " CHANGESET_ID "\n\n"
"  Copyright(C) 2006-2014, Fu Limin\n"
"  Dao is released under the terms of the Simplified BSD License\n"
"  Dao Language website: http://www.daovm.net\n"
;

static const char *const cmd_help =
"\n Usage: dao [options] script_file\n"
" Options:\n"
"   -h, --help:           print this help information;\n"
"   -v, --version:        print version information;\n"
"   -e, --eval:           evaluate command line codes;\n"
"   -i, --interactive:    run in interactive mode;\n"
"   -d, --debug:          run in debug mode;\n"
"   -p, --profile:        run in profile mode;\n"
"   -r, --restart:        restart program on crash (unix) or nozero exit (win);\n"
"   -c, --compile:        compile to bytecodes;\n"
"   -a, --archive:        build archive file;\n"
"   -l, --list-code:      print compiled bytecodes;\n"
"   -j, --jit:            enable just-in-time compiling;\n"
"   -Ox:                  optimization level (x=0 or 1);\n"
"   --threads=number      minimum number of threads for processing tasklets;\n"
"   --path=directory      add module searching path;\n"
"   --module=module       preloading module;\n"
"   --config=config       use configure file;\n"
;


extern DaoTypeBase  baseTyper;
extern DaoTypeBase  numberTyper;
extern DaoTypeBase  stringTyper;
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
extern DaoTypeBase  rgxMatchTyper;
extern DaoTypeBase  futureTyper;
extern DaoTypeBase  channelTyper;

extern DaoTypeBase macroTyper;
extern DaoTypeBase regexTyper;
extern DaoTypeBase vmpTyper;
extern DaoTypeBase typeKernelTyper;

static DaoTypeBase vmsTyper=
{
	"vmspace", NULL, NULL, NULL, {0}, {0},
	(FuncPtrDel) DaoVmSpace_Delete, NULL
};

DaoTypeBase* DaoVmSpace_GetTyper( short type )
{
	switch( type ){
	case DAO_INTEGER  :  return & numberTyper;
	case DAO_FLOAT    :  return & numberTyper;
	case DAO_DOUBLE   :  return & numberTyper;
	case DAO_COMPLEX  :  return & comTyper;
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
	case DAO_CTYPE   :
	case DAO_CSTRUCT :
	case DAO_CDATA   :  return & defaultCdataTyper;
	case DAO_ROUTINE   :  return & routTyper;
	case DAO_INTERFACE :  return & interTyper;
	case DAO_CLASS     :  return & classTyper;
	case DAO_OBJECT    :  return & objTyper;
	case DAO_NAMESPACE :  return & nsTyper;
	case DAO_PROCESS   :  return & vmpTyper;
	case DAO_VMSPACE   :  return & vmsTyper;
	case DAO_TYPE      :  return & abstypeTyper;
	case DAO_TYPEKERNEL : return & typeKernelTyper;
#ifdef DAO_WITH_MACRO
	case DAO_MACRO     :  return & macroTyper;
#endif
	default : break;
	}
	return & baseTyper;
}
const char*const DaoVmSpace_GetCopyNotice()
{
	return dao_copy_notice;
}

void DaoVmSpace_SetOptions( DaoVmSpace *self, int options )
{
	self->options = options;
}
int DaoVmSpace_GetOptions( DaoVmSpace *self )
{
	return self->options;
}
DaoNamespace* DaoVmSpace_FindNamespace( DaoVmSpace *self, DString *name )
{
	daoint i;
	DNode *node;
	DaoNamespace *ns = NULL;
	DaoVmSpace_Lock( self );
	node = DMap_Find( self->nsModules, name );
	if( node ){
		ns = (DaoNamespace*) node->value.pValue;
		DArray_PushFront( self->loadedModules, ns );
	}
	DaoVmSpace_Unlock( self );
	return ns;
}
DaoNamespace* DaoVmSpace_GetNamespace( DaoVmSpace *self, const char *name )
{
	DString str = DString_WrapMBS( name );
	DaoNamespace *ns = DaoVmSpace_FindNamespace( self, & str );
	if( ns ) return ns;
	ns = DaoNamespace_New( self, name );
	DaoVmSpace_Lock( self );
	DMap_Insert( self->nsModules, & str, ns );
	DArray_PushFront( self->loadedModules, ns );
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
		proc = (DaoProcess*) DArray_Back( self->processes );
		proc->cache = DaoDataCache_Acquire( NULL, 0 );
		proc->active = 0;
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
		if( proc->factory ) DArray_Clear( proc->factory );
		DaoDataCache_Release( proc->cache );
		GC_DecRC( proc->future );
		proc->future = NULL;
		proc->cache = NULL;
		proc->aux = NULL;
		DaoProcess_PopFrames( proc, proc->firstFrame );
		DArray_Clear( proc->exceptions );
		DArray_PushBack( self->processes, proc );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexProc );
#endif
}

#if 0
#define SHARE_NO_PARSER
#define SHARE_NO_INFERENCER
#define SHARE_NO_OPTIMIZER
#endif

DaoParser* DaoVmSpace_AcquireParser( DaoVmSpace *self )
{
	DaoParser *parser = NULL;

#ifdef SHARE_NO_PARSER
	parser = DaoParser_New();
	parser->vmSpace = self;
	return parser;
#endif

#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexMisc );
#endif
	if( self->parsers->size ){
		parser = (DaoParser*) DArray_Back( self->parsers );
		DArray_PopBack( self->parsers );
	}else{
		parser = DaoParser_New();
		parser->vmSpace = self;
		DMap_Insert( self->allParsers, parser, 0 );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexMisc );
#endif
	return parser;
}
void DaoVmSpace_ReleaseParser( DaoVmSpace *self, DaoParser *parser )
{
#ifdef SHARE_NO_PARSER
	DaoParser_Delete( parser ); return;
#endif

	DaoParser_Reset( parser );
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexMisc );
#endif
	if( DMap_Find( self->allParsers, parser ) ){
		DArray_PushBack( self->parsers, parser );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexMisc );
#endif
}
DaoByteCoder* DaoVmSpace_AcquireByteCoder( DaoVmSpace *self )
{
	DaoByteCoder *byteCoder = NULL;

#ifdef SHARE_NO_PARSER
	byteCoder = DaoByteCoder_New( self );
	//byteCoder->vmSpace = self;
	return byteCoder;
#endif

#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexMisc );
#endif
	if( self->byteCoders->size ){
		byteCoder = (DaoByteCoder*) DArray_Back( self->byteCoders );
		DArray_PopBack( self->byteCoders );
	}else{
		byteCoder = DaoByteCoder_New( self );
		//byteCoder->vmSpace = self;
		DMap_Insert( self->allByteCoders, byteCoder, 0 );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexMisc );
#endif
	return byteCoder;
}
void DaoVmSpace_ReleaseByteCoder( DaoVmSpace *self, DaoByteCoder *byteCoder )
{
#ifdef SHARE_NO_PARSER
	DaoByteCoder_Delete( byteCoder ); return;
#endif

	DaoByteCoder_Reset( byteCoder );
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexMisc );
#endif
	if( DMap_Find( self->allByteCoders, byteCoder ) ){
		DArray_PushBack( self->byteCoders, byteCoder );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexMisc );
#endif
}
DaoInferencer* DaoVmSpace_AcquireInferencer( DaoVmSpace *self )
{
	DaoInferencer *inferencer = NULL;

#ifdef SHARE_NO_INFERENCER
	return DaoInferencer_New();
#endif

#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexMisc );
#endif
	if( self->inferencers->size ){
		inferencer = (DaoInferencer*) DArray_Back( self->inferencers );
		DArray_PopBack( self->inferencers );
	}else{
		inferencer = DaoInferencer_New();
		DMap_Insert( self->allInferencers, inferencer, 0 );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexMisc );
#endif
	return inferencer;
}
void DaoVmSpace_ReleaseInferencer( DaoVmSpace *self, DaoInferencer *inferencer )
{
#ifdef SHARE_NO_INFERENCER
	DaoInferencer_Delete( inferencer ); return;
#endif

	DaoInferencer_Reset( inferencer );
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexMisc );
#endif
	if( DMap_Find( self->allInferencers, inferencer ) ){
		DArray_PushBack( self->inferencers, inferencer );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexMisc );
#endif
}
DaoOptimizer* DaoVmSpace_AcquireOptimizer( DaoVmSpace *self )
{
	DaoOptimizer *optimizer = NULL;

#ifdef SHARE_NO_OPTIMIZER
	return DaoOptimizer_New();
#endif

#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexMisc );
#endif
	if( self->optimizers->size ){
		optimizer = (DaoOptimizer*) DArray_Back( self->optimizers );
		DArray_PopBack( self->optimizers );
	}else{
		optimizer = DaoOptimizer_New();
		DMap_Insert( self->allOptimizers, optimizer, 0 );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexMisc );
#endif
	return optimizer;
}
void DaoVmSpace_ReleaseOptimizer( DaoVmSpace *self, DaoOptimizer *optimizer )
{
#ifdef SHARE_NO_OPTIMIZER
	DaoOptimizer_Delete( optimizer ); return;
#endif

#ifdef DAO_WITH_THREAD
	DMutex_Lock( & self->mutexMisc );
#endif
	if( DMap_Find( self->allOptimizers, optimizer ) ){
		DArray_PushBack( self->optimizers, optimizer );
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & self->mutexMisc );
#endif
}
DaoStream* DaoVmSpace_StdioStream( DaoVmSpace *self )
{
	return self->stdioStream;
}
DaoStream* DaoVmSpace_ErrorStream( DaoVmSpace *self )
{
	return self->errorStream;
}
DaoUserStream* DaoVmSpace_SetUserStdio( DaoVmSpace *self, DaoUserStream *stream )
{
	return DaoStream_SetUserStream( self->stdioStream, stream );
}
DaoUserStream* DaoVmSpace_SetUserStdError( DaoVmSpace *self, DaoUserStream *stream )
{
	if( self->errorStream == self->stdioStream ){
		self->errorStream = DaoStream_New();
		GC_ShiftRC( self->errorStream, self->stdioStream );
	}
	return DaoStream_SetUserStream( self->errorStream, stream );
}
DaoDebugger* DaoVmSpace_SetUserDebugger( DaoVmSpace *self, DaoDebugger *handler )
{
	DaoDebugger *hd = self->debugger;
	self->debugger = handler;
	return hd;
}
DaoProfiler* DaoVmSpace_SetUserProfiler( DaoVmSpace *self, DaoProfiler *profiler )
{
	DaoProfiler *hd = self->profiler;
	if( profiler == NULL || profiler->EnterFrame == NULL || profiler->LeaveFrame == NULL ){
		profiler = NULL;
	}
	self->profiler = profiler;
	return hd;
}
DaoUserHandler* DaoVmSpace_SetUserHandler( DaoVmSpace *self, DaoUserHandler *handler )
{
	DaoUserHandler *hd = self->userHandler;
	self->userHandler = handler;
	return hd;
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
static int dao_default_test_inliner( DaoNamespace *NS, DString *mode, DString *vb, DString *out, int line )
{
	DString_Reset( out, 0 );
	DString_AppendChar( out, ' ' );
	return 0;
}

static void DaoVmSpace_InitPath( DaoVmSpace *self )
{
	DString *path;
	char *daodir = getenv( "DAO_DIR" );
	const char *const paths[] = { "modules", "./lib/dao/modules", "../lib/dao/modules" };
	int i;

	if( self->hasAuxlibPath && self->hasSyslibPath ) return;
	DaoVmSpace_SetPath( self, self->startPath->mbs );

	DaoVmSpace_AddPath( self, DAO_DIR );
	if( self->hasAuxlibPath && self->hasSyslibPath ) return;

	path = DString_New(1);
	for(i=0; i<3; ++i){
		DString_SetMBS( path, paths[i] );
		Dao_MakePath( mainVmSpace->daoBinPath, path );
		DaoVmSpace_AddPath( self, path->mbs );
		if( self->hasAuxlibPath && self->hasSyslibPath ) goto Done;
	}

	if( daodir ) DaoVmSpace_AddPath( self, daodir );
Done:
	DString_Delete( path );
}



DaoVmSpace* DaoVmSpace_New()
{
	DaoVmSpace *self = (DaoVmSpace*) dao_calloc( 1, sizeof(DaoVmSpace) );
	DaoValue_Init( self, DAO_VMSPACE );
	self->stdioStream = DaoStream_New();
	self->errorStream = DaoStream_New();
	self->errorStream->file = stderr;
	self->daoBinPath = DString_New(1);
	self->startPath = DString_New(1);
	self->mainSource = DString_New(1);
	self->vfiles = DHash_New(D_STRING,D_STRING);
	self->vmodules = DHash_New(D_STRING,0);
	self->nsModules = DHash_New(D_STRING,0);
	self->pathWorking = DString_New(1);
	self->nameLoading = DArray_New(D_STRING);
	self->pathLoading = DArray_New(D_STRING);
	self->pathSearching = DArray_New(D_STRING);
	self->virtualPaths = DArray_New(D_STRING);
	self->sourceArchive = DArray_New(D_STRING);
	self->processes = DArray_New(0);
	self->parsers = DArray_New(0);
	self->byteCoders = DArray_New(0);
	self->inferencers = DArray_New(0);
	self->optimizers = DArray_New(0);
	self->allProcesses = DMap_New(D_VALUE,0);
	self->allParsers = DMap_New(0,0);
	self->allByteCoders = DMap_New(0,0);
	self->allInferencers = DMap_New(0,0);
	self->allOptimizers = DMap_New(0,0);
	self->loadedModules = DArray_New(D_VALUE);

#ifdef DAO_WITH_THREAD
	DMutex_Init( & self->mutexLoad );
	DMutex_Init( & self->mutexProc );
	DMutex_Init( & self->mutexMisc );
	DCondVar_Init( & self->condvWait );
#endif

	self->nsInternal = NULL; /* need to be set for DaoNamespace_New() */
	self->nsInternal = DaoNamespace_New( self, "dao" );
	self->nsInternal->vmSpace = self;
	self->nsInternal->refCount += 1;
	DMap_Insert( self->nsModules, self->nsInternal->name, self->nsInternal );
	DArray_PushFront( self->loadedModules, self->nsInternal );

	DaoNamespace_AddCodeInliner( self->nsInternal, "test", dao_default_test_inliner );

	self->mainNamespace = DaoNamespace_New( self, "MainNamespace" );
	self->mainNamespace->vmSpace = self;
	self->mainNamespace->refCount ++;
	self->stdioStream->refCount += 1;
	self->errorStream->refCount += 1;

	self->mainProcess = DaoProcess_New( self );
	GC_IncRC( self->mainProcess );

	if( mainVmSpace ) DaoNamespace_AddParent( self->nsInternal, mainVmSpace->nsInternal );

	return self;
}
void DaoVmSpace_DeleteData( DaoVmSpace *self )
{
	DNode *it;
	for(it=DMap_First(self->allParsers); it; it=DMap_Next(self->allParsers,it)){
		DaoParser_Delete( (DaoParser*) it->key.pVoid );
	}
	for(it=DMap_First(self->allByteCoders); it; it=DMap_Next(self->allByteCoders,it)){
		DaoByteCoder_Delete( (DaoByteCoder*) it->key.pVoid );
	}
	for(it=DMap_First(self->allInferencers); it; it=DMap_Next(self->allInferencers,it)){
		DaoInferencer_Delete( (DaoInferencer*) it->key.pVoid );
	}
	for(it=DMap_First(self->allOptimizers); it; it=DMap_Next(self->allOptimizers,it)){
		DaoOptimizer_Delete( (DaoOptimizer*) it->key.pVoid );
	}
	GC_DecRC( self->nsInternal );
	GC_DecRC( self->mainNamespace );
	GC_DecRC( self->stdioStream );
	GC_DecRC( self->errorStream );
	DString_Delete( self->daoBinPath );
	DString_Delete( self->startPath );
	DString_Delete( self->mainSource );
	DString_Delete( self->pathWorking );
	DArray_Delete( self->nameLoading );
	DArray_Delete( self->pathLoading );
	DArray_Delete( self->pathSearching );
	DArray_Delete( self->virtualPaths );
	DArray_Delete( self->processes );
	DArray_Delete( self->loadedModules );
	DArray_Delete( self->sourceArchive );
	DArray_Delete( self->parsers );
	DArray_Delete( self->byteCoders );
	DArray_Delete( self->inferencers );
	DArray_Delete( self->optimizers );
	DMap_Delete( self->vfiles );
	DMap_Delete( self->vmodules );
	DMap_Delete( self->allProcesses );
	DMap_Delete( self->allParsers );
	DMap_Delete( self->allByteCoders );
	DMap_Delete( self->allInferencers );
	DMap_Delete( self->allOptimizers );
	GC_DecRC( self->mainProcess );
	self->stdioStream = NULL;
	if( self->preloadModules ) DArray_Delete( self->preloadModules );
}
void DaoVmSpace_Delete( DaoVmSpace *self )
{
	if( self->stdioStream ) DaoVmSpace_DeleteData( self );
	DMap_Delete( self->nsModules );
#ifdef DAO_WITH_THREAD
	DMutex_Destroy( & self->mutexLoad );
	DMutex_Destroy( & self->mutexProc );
	DMutex_Destroy( & self->mutexMisc );
	DCondVar_Destroy( & self->condvWait );
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
int DaoVmSpace_ReadSource( DaoVmSpace *self, DString *fname, DString *source )
{
	DNode *node = MAP_Find( self->vfiles, fname );
	/* printf( "reading %s\n", fname->mbs ); */
	if( node ){
		DString_Assign( source, node->value.pString );
		return 1;
	}
	DString_ToMBS( fname );
	if( DaoFile_ReadAll( fopen( fname->mbs, "r" ), source, 1 ) ) return 1;
	DaoStream_WriteMBS( self->errorStream, "ERROR: can not open file \"" );
	DaoStream_WriteMBS( self->errorStream, fname->mbs );
	DaoStream_WriteMBS( self->errorStream, "\".\n" );
	return 0;
}
/* modules/debugger */
DAO_DLL void SplitByWhiteSpaces( const char *chs, DArray *tokens )
{
	DString temp = DString_WrapMBS( chs );
	DString *tok = DString_New(1);
	DString *str = & temp;
	daoint i, j, k=0, size = str->size;
	DArray_Clear( tokens );
	if( (j=DString_FindChar( str, '\1', k )) != MAXSIZE ){
		while( (j=DString_FindChar( str, '\1', k )) != MAXSIZE ){
			if( j > k ){
				DString_SubString( str, tok, k, j-k );
				DArray_Append( tokens, tok );
			}
			k = j + 1;
		}
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

static void DaoConfigure_FromFile( const char *name );
int DaoVmSpace_TryInitDebugger( DaoVmSpace *self, const char *module );
int DaoVmSpace_TryInitProfiler( DaoVmSpace *self, const char *module );
int DaoVmSpace_TryInitJIT( DaoVmSpace *self, const char *module );
int DaoVmSpace_ParseOptions( DaoVmSpace *self, const char *options )
{
	DString *str = DString_New(1);
	DArray *array = DArray_New(D_STRING);
	DaoNamespace *ns;
	daoint i, j;

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
				self->options |= DAO_OPTION_HELP;
			}else if( strcmp( token->mbs, "--version" ) ==0 ){
				self->options |= DAO_OPTION_VINFO;
			}else if( strcmp( token->mbs, "--eval" ) ==0 ){
				self->evalCmdline = 1;
				DString_Clear( self->mainSource );
			}else if( strcmp( token->mbs, "--interactive" ) ==0 ){
				self->options |= DAO_OPTION_INTERUN;
			}else if( strcmp( token->mbs, "--debug" ) ==0 ){
				self->options |= DAO_OPTION_DEBUG;
			}else if( strcmp( token->mbs, "--profile" ) ==0 ){
				self->options |= DAO_OPTION_PROFILE;
			}else if( strcmp( token->mbs, "--restart" ) ==0 ){
			}else if( strcmp( token->mbs, "--list-code" ) ==0 ){
				self->options |= DAO_OPTION_LIST_BC;
			}else if( strcmp( token->mbs, "--compile" ) ==0 ){
				self->options |= DAO_OPTION_COMP_BC;
			}else if( strcmp( token->mbs, "--archive" ) ==0 ){
				self->options |= DAO_OPTION_ARCHIVE;
			}else if( strcmp( token->mbs, "--jit" ) ==0 ){
				self->options |= DAO_OPTION_JIT;
				daoConfig.jit = 1;
			}else if( strstr( token->mbs, "--threads=" ) == token->mbs ){
				daoConfig.cpu = strtol( token->mbs + 10, 0, 0 );
			}else if( strstr( token->mbs, "--path=" ) == token->mbs ){
				DaoVmSpace_AddPath( self, token->mbs + 7 );
			}else if( strstr( token->mbs, "--module=" ) == token->mbs ){
				if( self->preloadModules == NULL ) self->preloadModules = DArray_New(D_VALUE);
				if( (ns = DaoVmSpace_Load( self, token->mbs + 9 )) ){
					DArray_Append( self->preloadModules, ns );
					DArray_Append( self->mainNamespace->namespaces, ns );
					DaoNamespace_UpdateLookupTable( self->mainNamespace );
				}else{
					DaoStream_WriteMBS( self->errorStream, "Preloading failed for module: " );
					DaoStream_WriteMBS( self->errorStream, token->mbs );
					DaoStream_WriteMBS( self->errorStream, ";\n" );
				}
			}else if( strstr( token->mbs, "--config=" ) == token->mbs ){
				DaoConfigure_FromFile( token->mbs + 9 );
			}else if( token->size ){
				DaoStream_WriteMBS( self->errorStream, "Unknown option: " );
				DaoStream_WriteMBS( self->errorStream, token->mbs );
				DaoStream_WriteMBS( self->errorStream, ";\n" );
			}
		}else if( strcmp( token->mbs, "-O0" ) ==0 ){
			daoConfig.optimize = 0;
		}else if( strcmp( token->mbs, "-O1" ) ==0 ){
			daoConfig.optimize = 1;
		}else{
			daoint len = token->size;
			DString_Clear( str );
			for( j=0; j<len; j++ ){
				switch( token->mbs[j] ){
				case 'h' : self->options |= DAO_OPTION_HELP;      break;
				case 'v' : self->options |= DAO_OPTION_VINFO;     break;
				case 'i' : self->options |= DAO_OPTION_INTERUN;   break;
				case 'd' : self->options |= DAO_OPTION_DEBUG;     break;
				case 'p' : self->options |= DAO_OPTION_PROFILE;   break;
				case 'l' : self->options |= DAO_OPTION_LIST_BC;   break;
				case 'c' : self->options |= DAO_OPTION_COMP_BC;   break;
				case 'a' : self->options |= DAO_OPTION_ARCHIVE;   break;
				case 'j' : self->options |= DAO_OPTION_JIT;
						   daoConfig.jit = 1;
						   break;
				case 'T' : daoConfig.typedcode = 0; break;
				case 'e' : self->evalCmdline = 1;
						   DString_Clear( self->mainSource );
						   break;
				case 'r' : break;
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
				DaoStream_WriteMBS( self->errorStream, "Unknown option: " );
				DaoStream_WriteMBS( self->errorStream, str->mbs );
				DaoStream_WriteMBS( self->errorStream, ";\n" );
			}
		}
	}
	if( self->options & DAO_OPTION_DEBUG ) daoConfig.optimize = 0;
	DString_Delete( str );
	DArray_Delete( array );
	if( daoConfig.jit && dao_jit.Compile == NULL ) DaoVmSpace_TryInitJIT( self, NULL );
	if( self->options & DAO_OPTION_PROFILE ) DaoVmSpace_TryInitProfiler( self, NULL );
	if( self->options & DAO_OPTION_DEBUG ) DaoVmSpace_TryInitDebugger( self, NULL );
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

static void DaoVmSpace_MakePath( DaoVmSpace *self, DString *path );
static DaoNamespace* DaoVmSpace_LoadDaoModuleExt( DaoVmSpace *self, DString *p, DArray *a, int run );
static DaoNamespace* DaoVmSpace_LoadDllModule( DaoVmSpace *self, DString *libpath );

static void DaoVmSpace_ParseArguments( DaoVmSpace *self, DaoNamespace *ns,
		const char *file, DArray *args, DArray *argNames, DArray *argValues )
{
	DString *str = DString_New(1);
	DString *key = DString_New(1);
	DString *val = DString_New(1);
	DArray *array = args;
	daoint i, tk, offset = 0;
	int eq = 0;

	if( array == NULL && file ){
		array = DArray_New(D_STRING);
		SplitByWhiteSpaces( file, array );
		DString_Assign( ns->name, array->items.pString[0] );
	}
	DString_Assign( val, array->items.pString[0] );
	DaoVmSpace_MakePath( self, ns->name );
	DaoNamespace_SetName( ns, ns->name->mbs ); /* to update ns->path and ns->file; */
	i = 1;
	while( i < array->size ){
		DString *s = array->items.pString[i];
		i ++;
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
		}else if( tk == DTOK_IDENTIFIER && offset && i < array->size ){
			DString_SubString( s, key, offset, s->size-offset );
			DString_Assign( val, array->items.pString[i] );
			DArray_Append( argNames, key );
			DArray_Append( argValues, val );
			i += 1;
		}else{
			DString_Clear( key );
			DString_Assign( val, s );
			DArray_Append( argNames, key );
			DArray_Append( argValues, s );
		}
	}
	if( args == NULL ) DArray_Delete( array );
	DString_Delete( key );
	DString_Delete( val );
	DString_Delete( str );
}
static int DaoRoutine_CheckParamTypes( DaoRoutine *self, DaoType *argtypes[], int argcount )
{
	DMap *defs = DMap_New(0,0);
	DaoType *routType = self->routType;
	DaoType  *abtp, **partypes = routType->nested->items.pType;
	int parpass[DAO_MAX_PARAM];
	int ndef = self->parCount;
	int i, j, match = 1;
	int ifrom, ito;

	if( argcount == ndef && ndef == 0 ) goto FinishOK;
	match = 0;

	for(j=0; j<ndef; j++) parpass[j] = 0;
	for(ifrom=0; ifrom<argcount; ifrom++){
		DaoType *tp = argtypes[ifrom];
		ito = ifrom;
		if( ito >= ndef ) goto FinishError;
		if( partypes[ito]->tid == DAO_PAR_VALIST ){
			DaoType *vlt = (DaoType*) partypes[ito]->aux;
			for(; ifrom<argcount; ifrom++, ito++){
				parpass[ito] = 1;
				if( vlt && DaoType_MatchTo( tp, vlt, defs ) == 0 ) goto FinishError;
			}
			break;
		}
		if( tp == NULL ) goto FinishError;
		if( tp->tid == DAO_PAR_NAMED ){
			DNode *node = DMap_Find( routType->mapNames, tp->fname );
			if( node == NULL ) goto FinishError;
			ito = node->value.pInt;
			tp = & tp->aux->xType;
		}
		if( ito >= ndef || tp ==NULL )  goto FinishError;
		abtp = routType->nested->items.pType[ito];
		if( abtp->tid == DAO_PAR_NAMED || abtp->tid == DAO_PAR_DEFAULT ) abtp = & abtp->aux->xType;
		parpass[ito] = DaoType_MatchTo( tp, abtp, defs );
		if( parpass[ito] == 0 ) goto FinishError;
	}
	for(ito=0; ito<ndef; ito++){
		if( parpass[ito] || partypes[ito]->tid == DAO_PAR_VALIST ) break;
		if( partypes[ito]->tid != DAO_PAR_DEFAULT ) goto FinishError;
		parpass[ito] = 1;
	}
	match = DAO_MT_EQ;
	for(j=0; j<argcount; j++) if( match > parpass[j] ) match = parpass[j];

#if 0
	printf( "%s %i %i %i\n", routType->name->mbs, match, ndef, argcount );
#endif

FinishOK:
FinishError:
	DMap_Delete( defs );
	return match;
}
static DaoRoutine* DaoVmSpace_FindExplicitMain( DaoNamespace *ns, DArray *argNames, DArray *argValues, int *error )
{
	DArray *types;
	DString *name;
	DaoRoutine *rout = NULL;
	DaoRoutine **routs = NULL;
	int i, j, max = 0, count = 0;

	types = DArray_New(0);
	name = DString_New(1);

	*error = DAO_ERROR;
	DString_SetMBS( name, "main" );
	i = DaoNamespace_FindConst( ns, name );
	if( i >=0 ){
		DaoValue *value = DaoNamespace_GetConst( ns, i );
		if( value->type == DAO_ROUTINE ) rout = & value->xRoutine;
	}
	if( rout == NULL ){
		DArray_Delete( types );
		DString_Delete( name );
		return NULL;
	}
	for( i=0; i<argNames->size; i++ ){
		DaoType *type = dao_type_string;
		char *chars = argValues->items.pString[i]->mbs;
		if( chars[0] == '+' || chars[0] == '-' ) chars ++;
		if( DaoToken_IsNumber( chars, 0 ) ) type = dao_type_double;
		if( argNames->items.pString[i]->size ){
			DString *S = argNames->items.pString[i];
			type = DaoNamespace_MakeType( ns, S->mbs, DAO_PAR_NAMED, (DaoValue*)type, NULL, 0 );
		}
		DArray_Append( types, type );
	}
	*error = DAO_ERROR_PARAM;
	if( rout->overloads ){
		routs = rout->overloads->routines->items.pRoutine;
		count = rout->overloads->routines->size;
	}else{
		routs = & rout;
		count = 1;
	}
	for(i=0; i<count; ++i){
		int s = DaoRoutine_CheckParamTypes( routs[i], types->items.pType, types->size );
		if( s > max ){
			rout = routs[i];
			max = s;
		}
	}
	if( max == 0 ) rout = NULL;
	if( rout ) *error = 0;
	return rout;
}
static void DaoList_SetArgument( DaoList *self, int i, DaoType *type, DString *value, DaoValue *sval )
{
	DaoValue ival = {DAO_INTEGER};
	DaoValue fval = {DAO_FLOAT};
	DaoValue dval = {DAO_DOUBLE};

	switch( type->tid ){
	case DAO_INTEGER :
		ival.xInteger.value = strtoll( value->mbs, 0, 0 );
		DaoList_SetItem( self, & ival, i );
		break;
	case DAO_FLOAT :
		fval.xFloat.value = strtod( value->mbs, 0 );
		DaoList_SetItem( self, & fval, i );
		break;
	case DAO_DOUBLE :
		dval.xDouble.value = strtod( value->mbs, 0 );
		DaoList_SetItem( self, & dval, i );
		break;
	default :
		DString_Assign( sval->xString.data, value );
		DaoList_SetItem( self, sval, i );
		break;
	}
}
static int DaoVmSpace_ConvertArguments( DaoVmSpace *self, DaoRoutine *routine, DArray *argNames, DArray *argValues )
{
	DString *str, *val;
	DaoValue sval = {DAO_STRING};
	DaoNamespace *ns = routine->nameSpace;
	DaoList *argParams = ns->argParams;
	DaoType *routype = routine->routType;
	DaoType *type;
	int set[2*DAO_MAX_PARAM];
	int i, j;

	val = DString_New(1);
	sval.xString.data = val;
	DaoList_Clear( argParams );

	memset( set, 0, sizeof(int) );
	for(i=0; i<routype->nested->size; ++i){
		DaoList_Append( argParams, routine->routConsts->items.items.pValue[i] );
		set[i] = 0;
	}
	for(i=0; i<argNames->size; ++i){
		DString *name = argNames->items.pString[i];
		DString *value = argValues->items.pString[i];
		int ito = i;
		type = routype->nested->items.pType[ito];
		if( type->tid == DAO_PAR_VALIST ){
			for(j=i; j<argNames->size; ++j){
				DaoValue *argv = & sval;
				name = argNames->items.pString[j];
				value = argValues->items.pString[j];
				DString_Assign( val, value );
				if( type->aux ){
					DaoList_SetArgument( argParams, j, (DaoType*) type->aux, value, & sval );
					continue;
				}
				if( name->size ){
					DaoNameValue *nameva = DaoNameValue_New( name, argv );
					DaoValue *st = (DaoValue*) dao_type_string;
					type = DaoNamespace_MakeType( ns, name->mbs, DAO_PAR_NAMED, st, NULL, 0 );
					nameva->ctype = type;
					GC_IncRC( nameva->ctype );
					argv = (DaoValue*) nameva;
				}
				DaoList_SetItem( argParams, argv, j );
			}
			break;
		}
		if( name->size ){
			DNode *node = DMap_Find( routype->mapNames, name );
			if( node ) ito = node->value.pInt;
		}
		if( ito >= routype->nested->size ){
			DaoStream_WriteMBS( self->errorStream, "redundant argument" );
			goto Failed;
		}
		type = (DaoType*) routype->nested->items.pType[ito]->aux;
		if( set[ito] ){
			DaoStream_WriteMBS( self->errorStream, "duplicated argument" );
			goto Failed;
		}
		set[ito] = 1;
		DaoList_SetArgument( argParams, ito, type, value, & sval );
	}
	DString_Delete( val );
	return 1;
Failed:
	DString_Delete( val );
	return 0;
}

DaoNamespace* DaoVmSpace_LoadEx( DaoVmSpace *self, const char *file, int run )
{
	DArray *args = DArray_New(D_STRING);
	DString *path = DString_New(1);
	DaoNamespace *ns = NULL;

	SplitByWhiteSpaces( file, args );
	DString_Assign( path, args->items.pString[0] );
	switch( DaoVmSpace_CompleteModuleName( self, path, DAO_MODULE_ANY ) ){
	case DAO_MODULE_DAC :
	case DAO_MODULE_DAO : ns = DaoVmSpace_LoadDaoModuleExt( self, path, args, run ); break;
	case DAO_MODULE_DLL : ns = DaoVmSpace_LoadDllModule( self, path ); break;
	case DAO_MODULE_ANY : ns = DaoVmSpace_LoadDaoModuleExt( self, path, args, run ); break; /* any suffix */
	default : break;
	}
	DArray_Delete( args );
	DString_Delete( path );
	if( ns == NULL ) return 0;
	return ns;
}
DaoNamespace* DaoVmSpace_Load( DaoVmSpace *self, const char *file )
{
	return DaoVmSpace_LoadEx( self, file, 0 );
}
/* Link "ns" to the module/namespace corresponding to "mod". */
/* If the module "mod" is not loaded yet, it will be loaded first. */
/* Return the namespace corresponding to "mod". */
DaoNamespace* DaoVmSpace_LinkModule( DaoVmSpace *self, DaoNamespace *ns, const char *mod )
{
	DaoNamespace *modns = DaoVmSpace_Load( self, mod );
	if( modns == NULL ) return NULL;
	DaoNamespace_AddParent( ns, modns );
	return modns;
}

static int CheckCodeCompletion( DString *source, DaoLexer *lexer )
{
	DArray *tokens = lexer->tokens;
	int i, bcount, cbcount, sbcount, tki = 0, completed = 1;
	DString_ToMBS( source );
	DaoLexer_Tokenize( lexer, source->mbs, DAO_LEX_COMMENT|DAO_LEX_SPACE );
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
	DaoValue *value;
	DaoNamespace *ns;
	DaoLexer *lexer = DaoLexer_New();
	DString *input = DString_New(1);
	const char *varRegex = "^ %s* = %s* %S+";
	const char *srcRegex = "^ %s* %w+ %. dao .* $";
	const char *sysRegex = "^ %\\ %s* %w+ %s* .* $";
	char *chs;
	int ch, newline = 0;
	DString_SetMBS( self->mainNamespace->name, "interactive codes" );
	self->mainNamespace->options |= DAO_NS_AUTO_GLOBAL;
	ns = DaoVmSpace_LinkModule( self, self->mainNamespace, "help" );
	value = ns ? DaoNamespace_FindData( ns, "help_message" ) : NULL;
	if( value && value->type == DAO_STRING ) printf( "%s\n", DaoValue_TryGetMBString( value ) );
	while(1){
		DString_Clear( input );
		DaoValue_Clear( self->mainProcess->stackValues );
		if( self->ReadLine ){
			chs = self->ReadLine( "(dao) " );
			if( chs == NULL ){
				printf( "\n" );
				break;
			}
			while( chs ){
				DString_AppendMBS( input, chs );
				DString_AppendChar( input, '\n' );
				dao_free( chs );
				if( CheckCodeCompletion( input, lexer ) ){
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
					if( CheckCodeCompletion( input, lexer ) ) break;
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
			DaoProcess_Eval( self->mainProcess, self->mainNamespace, input->mbs );
		}else if( DString_MatchMBS( input, varRegex, NULL, NULL ) ){
			DString_ChangeMBS( input, "^ %s* = %s*", "", 0 );
			DString_InsertMBS( input, "return ", 0, 0, 0 );
			if( callback ){
				(*callback)( input->mbs );
				continue;
			}
			DaoProcess_Eval( self->mainProcess, self->mainNamespace, input->mbs );
		}else{
			if( callback ){
				(*callback)( input->mbs );
				continue;
			}
			DaoProcess_Eval( self->mainProcess, self->mainNamespace, input->mbs );
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
	DaoLexer_Delete( lexer );
}

static void DaoVmSpace_PrintCode( DaoVmSpace *self )
{
	DaoNamespace *ns = self->mainNamespace;
	daoint i;
	if( self->options & DAO_OPTION_LIST_BC ){
		for(i=ns->cstUser; i<ns->constants->size; i++){
			DaoValue *p = ns->constants->items.pConst[i]->value;
			if( p->type == DAO_ROUTINE && & p->xRoutine != ns->mainRoutine ){
				DaoRoutine_PrintCode( & p->xRoutine, self->stdioStream );
			}else if( p->type == DAO_CLASS ){
				DaoStream_WriteMBS( self->stdioStream, "\n\n" );
				DaoClass_PrintCode( & p->xClass, self->stdioStream );
			}
		}
		DaoStream_Flush( self->stdioStream );
		if( ns->mainRoutine ) DaoRoutine_PrintCode( ns->mainRoutine, self->stdioStream );
	}
}
static void DaoVmSpace_ExeCmdArgs( DaoVmSpace *self )
{
	if( self->options & DAO_OPTION_VINFO ){
		DaoStream_WriteNewLine( self->stdioStream );
		DaoStream_WriteMBS( self->stdioStream, dao_copy_notice );
		DaoStream_WriteNewLine( self->stdioStream );
	}
	if( self->options & DAO_OPTION_HELP )  DaoStream_WriteMBS( self->stdioStream, cmd_help );
	DaoStream_Flush( self->stdioStream );

	DaoVmSpace_PrintCode( self );
}
void DaoVmSpace_ConvertPath( DaoVmSpace *self, DString *path )
{
	char *daodir = getenv( "DAO_DIR" );
	char *home = getenv( "HOME" );

	if( DString_Find( path, self->startPath, 0 ) == 0 ){
		DString_ReplaceMBS( path, "$(CMD_DIR)/", 0, self->startPath->size );
	}else if( DString_Find( path, self->daoBinPath, 0 ) == 0 ){
		DString_ReplaceMBS( path, "$(EXE_DIR)/", 0, self->daoBinPath->size );
	}else if( daodir && DString_FindMBS( path, daodir, 0 ) == 0 ){
		DString_ReplaceMBS( path, "$(DAO_DIR)", 0, strlen(daodir) );
	}else if( home && DString_FindMBS( path, home, 0 ) == 0 ){
		DString_ReplaceMBS( path, "$(HOME)", 0, strlen(home) );
	}
}
#if 0
void DaoVmSpace_ConvertPath2( DaoVmSpace *self, DString *path )
{
	char *daodir = getenv( "DAO_DIR" );
	char *home = getenv( "HOME" );

	if( DString_FindMBS( path, "$(CMD_DIR)/", 0 ) == 0 ){
		DString_Replace( path, self->startPath, 0, strlen( "$(CMD_DIR)/" ) );
	}else if( DString_FindMBS( path, "$(EXE_DIR)/", 0 ) == 0 ){
		DString_ReplaceMBS( path, self->daoBinPath, 0, strlen( "$(EXE_DIR)/" ) );
	}else if( daodir && DString_FindMBS( path, "$(DAO_DIR)", 0 ) == 0 ){
		DString_ReplaceMBS( path, daodir, 0, strlen( "$(DAO_DIR)" ) );
	}else if( home && DString_FindMBS( path, "$(HOME)", 0 ) == 0 ){
		DString_ReplaceMBS( path, home, 0, strlen( "$(HOME)" ) );
	}
}
#endif
void DaoVmSpace_SaveByteCodes( DaoVmSpace *self, DaoByteCoder *coder, DaoNamespace *ns )
{
	FILE *fout;
	DString *fname = DString_New(1);
	DString *output = DString_New(1);

	DString_Append( fname, ns->name );
	if( fname->size > ns->lang->size ) fname->size -= ns->lang->size;
	DString_AppendMBS( fname, "dac" );
	fout = fopen( fname->mbs, "w+" );

	DaoByteCoder_EncodeHeader( coder, ns->name->mbs, output );
	DaoByteCoder_EncodeToString( coder, output );

	DaoFile_WriteString( fout, output );
	DString_Delete( output );
	fclose( fout );

	DaoStream_WriteMBS( self->stdioStream, "Source file: " );
	DaoStream_WriteString( self->stdioStream, ns->name );
	DaoStream_WriteChar( self->stdioStream, '\n' );
	DaoStream_WriteMBS( self->stdioStream, "Compiled to: " );
	DaoStream_WriteString( self->stdioStream, fname );
	DaoStream_WriteMBS( self->stdioStream, "\n\n" );
	DString_Delete( fname );
}

void DString_AppendUInt16( DString *bytecodes, int value )
{
	uchar_t bytes[2];
	bytes[0] = (value >> 8) & 0xFF;
	bytes[1] = value & 0xFF;
	DString_AppendDataMBS( bytecodes, (char*) bytes, 2 );
}
void DString_AppendUInt32( DString *bytecodes, uint_t value )
{
	uchar_t bytes[4];
	bytes[0] = (value >> 24) & 0xFF;
	bytes[1] = (value >> 16) & 0xFF;
	bytes[2] = (value >>  8) & 0xFF;
	bytes[3] = value & 0xFF;
	DString_AppendDataMBS( bytecodes, (char*) bytes, 4 );
}

int DaoDecodeUInt16( const char *data )
{
	const uchar_t *p = (const uchar_t*) data;
	return (p[0]<<8) + p[1];
}
int DaoDecodeUInt32( const char *data )
{
	const uchar_t *p = (const uchar_t*) data;
	return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3];
}
static void DaoVmSpace_SaveArchive( DaoVmSpace *self, DArray *argValues )
{
	FILE *fin, *fout;
	int i, count = 1;
	int slen = strlen( DAO_DLL_SUFFIX );
	DaoNamespace *ns = self->mainNamespace;
	DString *archive = DString_New(1);
	DString *data = DString_New(1);

	DString_Append( archive, ns->name );
	if( archive->size > ns->lang->size ) archive->size -= ns->lang->size;
	DString_AppendMBS( archive, "dar" );

	DaoStream_WriteMBS( self->stdioStream, "Creating Dao archive file: " );
	DaoStream_WriteString( self->stdioStream, archive );
	DaoStream_WriteMBS( self->stdioStream, "\n" );

	fout = fopen( archive->mbs, "w+" );
	archive->size = 0;

	count = self->sourceArchive->size/2;
	for(i=0; i<self->sourceArchive->size; i+=2){
		DString *name = self->sourceArchive->items.pString[i];
		DString *source = self->sourceArchive->items.pString[i+1];
		DaoStream_WriteMBS( self->stdioStream, "Adding to the archive: " );
		DaoStream_WriteString( self->stdioStream, name );
		DaoStream_WriteMBS( self->stdioStream, "\n" );
		DaoVmSpace_ConvertPath( self, name );
		DString_AppendUInt16( archive, name->size );
		DString_Append( archive, name );
		DString_AppendUInt32( archive, source->size );
		DString_Append( archive, source );
		if( DString_FindMBS( name, DAO_DLL_SUFFIX, 0 ) != name->size - slen ){
			// TODO:
		}
	}
	for(i=0; i<argValues->size; ++i){
		DString *file = argValues->items.pString[i];
		DString_Assign( data, file );
		Dao_MakePath( self->pathWorking, data );
		fin = fopen( data->mbs, "r" );
		if( fin == NULL ){
			DaoStream_WriteMBS( self->errorStream, "WARNING: can not open resource file \"" );
			DaoStream_WriteMBS( self->errorStream, data->mbs );
			DaoStream_WriteMBS( self->errorStream, "\".\n" );
			continue;
		}
		DaoStream_WriteMBS( self->stdioStream, "Adding to the archive: " );
		DaoStream_WriteString( self->stdioStream, data );
		DaoStream_WriteMBS( self->stdioStream, "\n" );
		count += 1;
		DaoFile_ReadAll( fin, data, 1 );
		DString_AppendUInt16( archive, file->size );
		DString_Append( archive, file );
		DString_AppendUInt32( archive, data->size );
		DString_Append( archive, data );
	}

	fprintf( fout, "\33\33\r\n" );
	DString_Clear( data );
	DString_AppendUInt32( data, count );
	DaoFile_WriteString( fout, data );
	DaoFile_WriteString( fout, archive );
	DString_Delete( archive );
	DString_Delete( data );
	fclose( fout );
}
void DaoVmSpace_LoadArchive( DaoVmSpace *self, DString *archive )
{
	DString *name;
	DaoVModule module = { NULL, 0, NULL, NULL };
	char *data = (char*) archive->mbs;
	int slen = strlen( DAO_DLL_SUFFIX );
	int pos = 4, size = archive->size;
	int i, m, n, files;

	DString_ToMBS( archive );
	DString_Clear( self->mainSource );
	if( size < 8 ) return;
	name = DString_New(1);
	data = (char*) archive->mbs;
	files = DaoDecodeUInt32( data + pos );
	pos += 4;
	for(i=0; i<files; ++i){
		if( (pos + 2) >= size ) break;
		m = DaoDecodeUInt16( data + pos );
		if( (pos + 2 + m + 4) >= size ) break;
		n = DaoDecodeUInt32( data + pos + 2 + m );
		if( i == 0 ){
			DString_SetMBS( self->mainSource, "/@/" );
			DString_AppendDataMBS( self->mainSource, data + pos + 2, m );
			DaoNamespace_SetName( self->mainNamespace, self->mainSource->mbs );
			DString_SetDataMBS( self->mainSource, data + pos + 2 + m + 4, n );
		}else{
			DString_SetDataMBS( name, data + pos + 2, m );
			/* Ignore DLL modules: */
			if( DString_FindMBS( name, DAO_DLL_SUFFIX, 0 ) != m - slen ){
				module.name = name->mbs;
				module.length = n;
				module.data = (uchar_t*) data + pos + 2 + m + 4;
				DaoVmSpace_AddVirtualModule( self, & module );
			}
		}
		pos += 2 + m + 4 + n;
	}
	DString_Delete( name );
}
int DaoVmSpace_Eval( DaoVmSpace *self, const char *src )
{
	DaoProcess *process = DaoVmSpace_AcquireProcess( self );
	DaoNamespace *ns = DaoNamespace_New( self, "?" );
	int retc = DaoProcess_Eval( process, ns, src );
	DaoVmSpace_ReleaseProcess( self, process );
	DaoGC_TryDelete( (DaoValue*) ns );
	return retc;
}
int DaoVmSpace_RunMain( DaoVmSpace *self, const char *file )
{
	DaoNamespace *ns = self->mainNamespace;
	DaoProcess *vmp = self->mainProcess;
	DaoStream *io = self->errorStream;
	DaoRoutine *mainRoutine, *expMain = NULL;
	DaoRoutine *rout = NULL;
	DaoValue **ps;
	DString *name;
	DArray *argNames;
	DArray *argValues;
	size_t tm = 0;
	daoint N;
	int i, j, res;

	if( file == NULL || file[0] ==0 || self->evalCmdline ){
		DArray_PushFront( self->nameLoading, self->pathWorking );
		DArray_PushFront( self->pathLoading, self->pathWorking );
		if( self->evalCmdline ){
			DaoRoutine *rout;
			DString_SetMBS( self->mainNamespace->name, "command line codes" );
			if( DaoProcess_Compile( vmp, ns, self->mainSource->mbs ) ==0 ) return 0;
			DaoVmSpace_ExeCmdArgs( self );
			rout = ns->mainRoutines->items.pRoutine[ ns->mainRoutines->size-1 ];
			if( DaoProcess_Call( vmp, rout, NULL, NULL, 0 ) ) return 0;
		}else{
			DaoVmSpace_ExeCmdArgs( self );
		}
		if( (self->options & DAO_OPTION_INTERUN) && self->userHandler == NULL )
			DaoVmSpace_Interun( self, NULL );
		return 1;
	}
	argNames = DArray_New(D_STRING);
	argValues = DArray_New(D_STRING);
	DaoVmSpace_ParseArguments( self, ns, file, NULL, argNames, argValues );
	DaoVmSpace_AddPath( self, ns->path->mbs );
	DArray_PushFront( self->nameLoading, ns->name );
	DArray_PushFront( self->pathLoading, ns->path );
	DArray_PushFront( self->loadedModules, ns );
	if( DMap_Find( self->nsModules, ns->name ) == NULL ){
		MAP_Insert( self->nsModules, ns->name, ns );
	}

	tm = Dao_FileChangedTime( ns->name->mbs );
	ns->time = tm;

	/* self->fileName may has been changed */
	res = DaoVmSpace_ReadSource( self, ns->name, self->mainSource );
	if( strncmp( self->mainSource->mbs, "\33\33\r\n", 4 ) == 0 ){
		DString *archive = DString_Copy( self->mainSource );
		DaoVmSpace_LoadArchive( self, archive );
		DString_Delete( archive );
	}
	if( self->options & DAO_OPTION_ARCHIVE ){
		DArray_Append( self->sourceArchive, ns->name );
		DArray_Append( self->sourceArchive, self->mainSource );
	}
	if( self->mainSource->mbs[0] == DAO_BC_SIGNATURE[0] ){
		DaoByteCoder *byteCoder = DaoVmSpace_AcquireByteCoder( self );
		res = DaoByteCoder_Decode( byteCoder, self->mainSource );
		if( self->options & DAO_OPTION_LIST_BC ) DaoByteCoder_Disassemble( byteCoder );
		res = res && DaoByteCoder_Build( byteCoder, ns );
		DaoVmSpace_ReleaseByteCoder( self, byteCoder );
	}else{
		DaoParser *parser = DaoVmSpace_AcquireParser( self );

		if( self->options & DAO_OPTION_COMP_BC ){
			parser->byteCoder = DaoVmSpace_AcquireByteCoder( self );
			parser->byteBlock = DaoByteCoder_Init( parser->byteCoder );
		}
		parser->nameSpace = ns;
		DString_Assign( parser->fileName, ns->name );
		res = res && DaoParser_LexCode( parser, self->mainSource->mbs, 1 );
		res = res && DaoParser_ParseScript( parser );

		if( res && (self->options & DAO_OPTION_COMP_BC) ){
			DaoVmSpace_SaveByteCodes( self, parser->byteCoder, ns );
			if( self->options & DAO_OPTION_LIST_BC ){
				DaoByteCoder_Disassemble( parser->byteCoder );
			}
		}
		if( parser->byteCoder ) DaoVmSpace_ReleaseByteCoder( self, parser->byteCoder );
		DaoVmSpace_ReleaseParser( self, parser );
	}
	if( res && !(self->options & DAO_OPTION_ARCHIVE) ){
		DString name = DString_WrapMBS( "main" );
		int error = 0, id = DaoNamespace_FindConst( ns, & name );
		DaoValue *cst = DaoNamespace_GetConst( ns, id );
		expMain = DaoVmSpace_FindExplicitMain( ns, argNames, argValues, & error );
		if( expMain ){
			res = DaoVmSpace_ConvertArguments( self, expMain, argNames, argValues );
		}else if( (cst && cst->type == DAO_ROUTINE) || argValues->size ){
			res = 0;
		}
		if( res == 0 ){
			DaoVmSpace_PrintCode( self );
			DaoStream_WriteMBS( io, "ERROR: invalid command line arguments.\n" );
		}
	}
	DArray_Delete( argNames );

	if( res == 0 ){
		DArray_Delete( argValues );
		return 0;
	}

	if( self->options & DAO_OPTION_ARCHIVE ){
		DaoVmSpace_SaveArchive( self, argValues );
		DArray_Delete( argValues );
		return 1;
	}
	DArray_Delete( argValues );
	if( self->options & DAO_OPTION_COMP_BC ) return 1;

	mainRoutine = ns->mainRoutine;

	DaoVmSpace_ExeCmdArgs( self );
	/* always execute default __main__() routine first for initialization: */
	if( mainRoutine ){
		DaoProcess_PushRoutine( vmp, mainRoutine, NULL );
		DaoProcess_Execute( vmp );
	}
#ifdef DAO_WITH_CONCURRENT
	if( vmp->status >= DAO_PROCESS_SUSPENDED ){
		DMutex_Lock( & self->mutexLoad );
		while( vmp->status >= DAO_PROCESS_SUSPENDED )
			DCondVar_TimedWait( & self->condvWait, & self->mutexLoad, 0.01 );
		DMutex_Unlock( & self->mutexLoad );
	}
#endif
	/* check and execute explicitly defined main() routine  */
	ps = ns->argParams->items.items.pValue;
	N = ns->argParams->items.size;
	if( expMain != NULL ){
		int ret = DaoProcess_Call( vmp, expMain, NULL, ps, N );
		if( ret == DAO_ERROR_PARAM ){
			DaoStream_WriteMBS( io, "ERROR: invalid command line arguments.\n" );
		}
		if( ret ) return 0;
	}
	if( (self->options & DAO_OPTION_INTERUN) && self->userHandler == NULL )
		DaoVmSpace_Interun( self, NULL );

	return 1;
}
int DaoVmSpace_CompleteModuleName( DaoVmSpace *self, DString *fname, int types )
{
	int i, modtype = DAO_MODULE_NONE;
	daoint slen = strlen( DAO_DLL_SUFFIX );
	daoint size;
	DString_ToMBS( fname );
	size = fname->size;
	if( (types & DAO_MODULE_DAC) && size >4 && DString_FindMBS( fname, ".dac", 0 ) == size-4 ){
		DaoVmSpace_SearchPath( self, fname, DAO_FILE_PATH, 1 );
		if( TestFile( self, fname ) ) modtype = DAO_MODULE_DAC;
	}else if( (types & DAO_MODULE_DAO) && size >4 && DString_FindMBS( fname, ".dao", 0 ) == size-4 ){
		DaoVmSpace_SearchPath( self, fname, DAO_FILE_PATH, 1 );
		if( TestFile( self, fname ) ) modtype = DAO_MODULE_DAO;
	}else if( types == DAO_MODULE_ANY && size >4 && DString_FindMBS( fname, ".cgi", 0 ) == size-4 ){
		DaoVmSpace_SearchPath( self, fname, DAO_FILE_PATH, 1 );
		if( TestFile( self, fname ) ) modtype = DAO_MODULE_DAO;
	}else if( (types & DAO_MODULE_DLL) && size > slen && DString_FindMBS( fname, DAO_DLL_SUFFIX, 0 ) == size - slen ){
		DaoVmSpace_SearchPath( self, fname, DAO_FILE_PATH, 1 );
		if( TestFile( self, fname ) ) modtype = DAO_MODULE_DLL;
	}else{
		DString *fn = DString_New(1);
		DString *path = DString_New(1);
		DString *file = DString_New(1);
		daoint pos = fname->size;
		while( pos && (fname->mbs[pos-1] == '_' || isalnum( fname->mbs[pos-1] )) ) pos -= 1;
		if( pos && (fname->mbs[pos-1] == '/' || fname->mbs[pos-1] == '\\') ){
			DString_SubString( fname, path, 0, pos );
			DString_SubString( fname, file, pos, fname->size - pos );
		}else{
			DString_Assign( file, fname );
		}
		for(i=0; i<DAO_FILE_TYPE_NUM; i++){
			if( !(types & daoModuleTypes[i]) ) continue;
			if( daoModuleTypes[i] < DAO_MODULE_DLL ){
				DString_Assign( fn, fname );
			}else{
				if( strncmp( fname->mbs, daoDllPrefix[i], 3 ) == 0 ) break;
				DString_Assign( fn, path );
				DString_AppendMBS( fn, daoDllPrefix[i] );
				DString_Append( fn, file );
			}
			DString_AppendMBS( fn, daoFileSuffix[i] );
			DaoVmSpace_SearchPath( self, fn, DAO_FILE_PATH, 1 );
#if 0
			printf( "%i %s %s\n", i, fn->mbs, self->nameLoading->items.pString[0]->mbs );
#endif
			/*
			// skip the current file, since one may create .dao file with the same name
			// as a dll module to load that dll module in certain way, or do extra things.
			*/
			if( self->nameLoading->size && DString_EQ( fn, self->nameLoading->items.pString[0] ) ) continue;
			if( TestFile( self, fn ) ){
				modtype = daoModuleTypes[i];
				if( modtype > DAO_MODULE_DLL ) modtype = DAO_MODULE_DLL;
				DString_Assign( fname, fn );
				break;
			}
		}
		if( modtype == DAO_MODULE_NONE ){
			DaoVmSpace_SearchPath( self, fname, DAO_FILE_PATH, 1 );
			if( TestFile( self, fname ) ) modtype = DAO_MODULE_ANY;
		}
		DString_Delete( fn );
		DString_Delete( path );
		DString_Delete( file );
	}
	return modtype;
}
static void DaoVmSpace_PopLoadingNamePath( DaoVmSpace *self, int path )
{
	DaoVmSpace_Lock( self );
	if( path ) DArray_PopFront( self->pathLoading );
	DArray_PopFront( self->nameLoading );
	DaoVmSpace_Unlock( self );
}
/*
// Loading module in Dao source file.
// The first time the module is loaded:
//   (1) its implicit main (codes outside of any class and function) is executed;
//   (2) then, its explicit main that matches with "args" will be executed.
// The next time the module is loaded:
//   (1) its implicit main is executed, IF run != 0; (mainly for IDE)
//   (2) its explicit main that matches with "args" will be executed.
*/
DaoNamespace* DaoVmSpace_LoadDaoModuleExt( DaoVmSpace *self, DString *libpath, DArray *args, int run )
{
	DString *source = NULL;
	DArray *argNames = NULL, *argValues = NULL;
	DaoNamespace *ns = NULL, *ns2 = NULL;
	DaoRoutine *mainRoutine = NULL;
	DaoParser *parser = NULL;
	DaoProcess *process;
	DString name;
	DNode *node;
	size_t tm = 0;
	daoint i = DString_FindMBS( libpath, "/addpath.dao", 0 );
	daoint j = DString_FindMBS( libpath, "/delpath.dao", 0 );
	daoint nsCount = self->loadedModules->size;
	int bl, m, poppath = 0;
	int cfgpath = i != MAXSIZE && i == libpath->size - 12;
	cfgpath = cfgpath || (j != MAXSIZE && j == libpath->size - 12);
	/*  XXX if cfgpath == true, only parsing? */

	if( args ){
		argNames = DArray_New(D_STRING);
		argValues = DArray_New(D_STRING);
	}

	DString_ToMBS( libpath );
	ns = ns2 = DaoVmSpace_FindNamespace( self, libpath );

	tm = Dao_FileChangedTime( libpath->mbs );
	/* printf( "time = %lli,  %s  %p\n", tm, libpath->mbs, node ); */
	if( ns && ns->time >= tm ){
		if( args ) DaoVmSpace_ParseArguments( self, ns, NULL, args, argNames, argValues );
		if( run ) goto ExecuteImplicitMain;
		goto ExecuteExplicitMain;
	}

	source = DString_New(1);
	if( ! DaoVmSpace_ReadSource( self, libpath, source ) ) goto LoadingFailed;

	if( self->options & DAO_OPTION_ARCHIVE ){
		DArray_Append( self->sourceArchive, libpath );
		DArray_Append( self->sourceArchive, source );
	}

	/*
	   printf("%p : loading %s\n", self, libpath->mbs );
	 */
	ns = DaoNamespace_New( self, libpath->mbs );
	ns->time = tm;
	if( args ) DaoVmSpace_ParseArguments( self, ns, NULL, args, argNames, argValues );

	DaoVmSpace_Lock( self );
	DArray_PushFront( self->loadedModules, ns );
	MAP_Insert( self->nsModules, libpath, ns );
	DArray_PushFront( self->nameLoading, ns->name );
	if( ns->path->size ) DArray_PushFront( self->pathLoading, ns->path );
	DaoVmSpace_Unlock( self );
	poppath = ns->path->size;

	if( source->mbs[0] == DAO_BC_SIGNATURE[0] ){
		DaoByteCoder *byteCoder = DaoVmSpace_AcquireByteCoder( self );
		int bl = DaoByteCoder_Decode( byteCoder, source );
		if( self->options & DAO_OPTION_LIST_BC ) DaoByteCoder_Disassemble( byteCoder );
		bl = bl && DaoByteCoder_Build( byteCoder, ns );
		DaoVmSpace_ReleaseByteCoder( self, byteCoder );
		if( bl == 0 ) goto LoadingFailed;
	}else{
		parser = DaoVmSpace_AcquireParser( self );
		parser->vmSpace = self;
		parser->nameSpace = ns;
		DString_Assign( parser->fileName, libpath );
		if( ! DaoParser_LexCode( parser, DString_GetMBS( source ), 1 ) ) goto LoadingFailed;
		if( self->options & DAO_OPTION_COMP_BC ){
			parser->byteCoder = DaoVmSpace_AcquireByteCoder( self );
			parser->byteBlock = DaoByteCoder_Init( parser->byteCoder );
		}
		if( ! DaoParser_ParseScript( parser ) ) goto LoadingFailed;
		if( ns->mainRoutine == NULL ) goto LoadingFailed;
		DString_SetMBS( ns->mainRoutine->routName, "__main__" );
		if( parser->byteCoder ){
			DaoVmSpace_SaveByteCodes( self, parser->byteCoder, ns );
			DaoVmSpace_ReleaseByteCoder( self, parser->byteCoder );
		}
		DaoVmSpace_ReleaseParser( self, parser );
		parser = NULL;
	}

ExecuteImplicitMain :
	if( ns->mainRoutine->body->vmCodes->size > 1 ){
		int status;
		process = DaoVmSpace_AcquireProcess( self );
		DaoVmSpace_Lock( self );
		DArray_PushFront( self->nameLoading, ns->path );
		DArray_PushFront( self->pathLoading, ns->path );
		DaoVmSpace_Unlock( self );
		DaoProcess_PushRoutine( process, ns->mainRoutine, NULL );
		DaoProcess_Execute( process );
#ifdef DAO_WITH_CONCURRENT
		if( process->status >= DAO_PROCESS_SUSPENDED ){
			DMutex_Lock( & self->mutexLoad );
			while( process->status >= DAO_PROCESS_SUSPENDED )
				DCondVar_TimedWait( & self->condvWait, & self->mutexLoad, 0.01 );
			DMutex_Unlock( & self->mutexLoad );
		}
#endif
		status = process->status;
		DaoVmSpace_ReleaseProcess( self, process );
		DaoVmSpace_Lock( self );
		DArray_PopFront( self->nameLoading );
		DArray_PopFront( self->pathLoading );
		DaoVmSpace_Unlock( self );
		if( status == DAO_PROCESS_ABORTED ) goto LoadingFailed;
	}

ExecuteExplicitMain :

	/* check and execute explicitly defined main() routine  */
	if( args ){
		int error = 0;
		mainRoutine = DaoVmSpace_FindExplicitMain( ns, argNames, argValues, & error );
		if( mainRoutine ){
			if( DaoVmSpace_ConvertArguments( self, mainRoutine, argNames, argValues ) == 0 )
				goto InvalidArgument;
		}else if( error == DAO_ERROR_PARAM ){
			goto InvalidArgument;
		}
	}
	if( mainRoutine != NULL ){
		int ret, N = ns->argParams->items.size;
		DaoValue **ps = ns->argParams->items.items.pValue;
		process = DaoVmSpace_AcquireProcess( self );
		ret = DaoProcess_Call( process, mainRoutine, NULL, ps, N );
		DaoVmSpace_ReleaseProcess( self, process );
		if( ret == DAO_ERROR_PARAM ) goto InvalidArgument;
		if( ret ) goto LoadingFailed;
	}
	DaoVmSpace_PopLoadingNamePath( self, poppath );
	if( self->loadedModules->size > (nsCount+1) ){
		DaoVmSpace_Lock( self );
		DArray_Erase( self->loadedModules, 0, self->loadedModules->size - (nsCount+1) );
		DaoVmSpace_Unlock( self );
	}
	if( source ) DString_Delete( source );
	if( argNames ) DArray_Delete( argNames );
	if( argValues ) DArray_Delete( argValues );
	return ns;
InvalidArgument:
	DaoStream_WriteMBS( self->errorStream, "ERROR: invalid arguments for the explicit main().\n" );
LoadingFailed :
	DaoVmSpace_PopLoadingNamePath( self, poppath );
	if( self->loadedModules->size > nsCount ){
		DaoVmSpace_Lock( self );
		DArray_Erase( self->loadedModules, 0, self->loadedModules->size - nsCount );
		DaoVmSpace_Unlock( self );
	}
	if( source ) DString_Delete( source );
	if( argNames ) DArray_Delete( argNames );
	if( argValues ) DArray_Delete( argValues );
	if( parser ) DaoVmSpace_ReleaseParser( self, parser );
	return 0;
}
DaoNamespace* DaoVmSpace_LoadDaoModule( DaoVmSpace *self, DString *libpath )
{
	return DaoVmSpace_LoadDaoModuleExt( self, libpath, NULL, 0 );
}

static DaoNamespace* DaoVmSpace_LoadDllModule( DaoVmSpace *self, DString *libpath )
{
	DNode *node;
	DString *name = NULL;
	DaoModuleOnLoad funpter = NULL;
	DaoNamespace *ns = NULL;
	void *handle = NULL;
	daoint nsCount = self->loadedModules->size;
	daoint i, retc;

	ns = DaoVmSpace_FindNamespace( self, libpath );
	if( ns ) return ns;

	DString_ToMBS( libpath );
	if( (node = MAP_Find( self->vmodules, libpath ) ) ){
		funpter = (DaoModuleOnLoad) node->value.pVoid;
		ns = DaoNamespace_New( self, libpath->mbs );
	}else{
		handle = Dao_OpenDLL( libpath->mbs );
		if( ! handle ){
			DaoStream_WriteMBS( self->errorStream, "ERROR: unable to open the library file \"" );
			DaoStream_WriteMBS( self->errorStream, libpath->mbs );
			DaoStream_WriteMBS( self->errorStream, "\".\n");
			return 0;
		}
		name = DString_New(1);
		DString_SetMBS( name, "DaoOnLoad" );
		ns = DaoNamespace_New( self, libpath->mbs );
		ns->libHandle = handle;
		funpter = (DaoModuleOnLoad) Dao_GetSymbolAddress( handle, "DaoOnLoad" );
		if( funpter == NULL ){
			int size = strlen( DAO_DLL_SUFFIX );
			DString_SetDataMBS( name, ns->file->mbs, ns->file->size - size );
			for(i=0; i<DAO_FILE_TYPE_NUM; i++){
				if( daoModuleTypes[i] < DAO_MODULE_DLL ) continue;
				if( DString_FindMBS( name, daoDllPrefix[i], 0 ) != 0 ) continue;
				DString_Erase( name, 0, strlen( daoDllPrefix[i] ) );
			}
			DString_InsertMBS( name, "Dao", 0, 0, 3 );
			DString_AppendMBS( name, "_OnLoad" );
			funpter = (DaoModuleOnLoad) Dao_GetSymbolAddress( handle, name->mbs );
			if( funpter == NULL ){
				for(i=3; i<name->size-7; i++) name->mbs[i] = tolower( name->mbs[i] );
				funpter = (DaoModuleOnLoad) Dao_GetSymbolAddress( handle, name->mbs );
			}
			if( funpter == NULL ){
				name->mbs[3] = toupper( name->mbs[3] );
				funpter = (DaoModuleOnLoad) Dao_GetSymbolAddress( handle, name->mbs );
			}
			if( funpter == NULL ){
				for(i=3; i<name->size-7; i++) name->mbs[i] = toupper( name->mbs[i] );
				funpter = (DaoModuleOnLoad) Dao_GetSymbolAddress( handle, name->mbs );
			}
		}
	}
	if( self->options & DAO_OPTION_ARCHIVE ){
		if( name == NULL ) name = DString_New(1);
		if( funpter == NULL ) DString_Clear( name );
		DArray_Append( self->sourceArchive, libpath );
		DArray_Append( self->sourceArchive, name );
	}
	if( name ) DString_Delete( name );

	DaoVmSpace_Lock( self );
	DArray_PushFront( self->loadedModules, ns );
	MAP_Insert( self->nsModules, libpath, ns );
	DaoVmSpace_Unlock( self );

	/*
	// no warning or error for loading a C/C++ dynamic linking library
	// for solving symbols in Dao modules.
	*/
	if( funpter == NULL ) return ns;

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
		DaoVmSpace_Lock( self );
		MAP_Erase( self->nsModules, ns->name );
		DArray_Erase( self->loadedModules, 0, self->loadedModules->size - nsCount );
		DaoVmSpace_Unlock( self );
		return NULL;
	}
	DaoNamespace_UpdateLookupTable( ns );
	if( self->loadedModules->size > (nsCount+1) ){
		DaoVmSpace_Lock( self );
		DArray_Erase( self->loadedModules, 0, self->loadedModules->size - (nsCount+1) );
		DaoVmSpace_Unlock( self );
	}
	return ns;
}
void DaoVmSpace_AddVirtualModule( DaoVmSpace *self, DaoVModule *module )
{
	DNode *node;
	DString *fname = DString_New(1);
	DString *source = DString_New(1);
	char *data = (char*) module->data;
	daoint pos, n = module->length;

	DString_SetMBS( fname, "/@/" );
	DString_AppendMBS( fname, module->name );
	if( module->onload ){
		MAP_Insert( self->vmodules, fname, module->onload );
	}else{
		if( n < 0 ) n = strlen( data );
		node = DMap_Find( self->vfiles, fname );
		if( node ){
			DString_AppendDataMBS( node->value.pString, data, n );
		}else{
			DString_ToMBS( source );
			DString_SetDataMBS( source, data, n );
			MAP_Insert( self->vfiles, fname, source );
		}
	}
	pos = DString_RFindChar( fname, '/', -1 );
	DString_Erase( fname, pos, -1 );
	DArray_PushFront( self->virtualPaths, fname );
	DString_Delete( fname );
	DString_Delete( source );
}

/* base is assumed to be absolute, and path is assumed to be relative: */
void Dao_MakePath( DString *base, DString *path )
{
	base = DString_Copy( base );
	DString_ToMBS( base );
	DString_ToMBS( path );
	Dao_NormalizePathSep( base );
	Dao_NormalizePathSep( path );
	while( DString_MatchMBS( path, " ^ %.%. / ", NULL, NULL ) ){
		if( DString_MatchMBS( base, " [^/] + ( / | ) $ ", NULL, NULL ) ){
			DString_ChangeMBS( path, " ^ %.%. / ", "", 1 );
			DString_ChangeMBS( base, " [^/] + ( / |) $ ", "", 0 );
		}else{
			DString_Delete( base );
			return;
		}
	}
	if( DString_MatchMBS( path, " ^ %.%. $ ", NULL, NULL ) ){
		if( DString_MatchMBS( base, " [^/] + ( / | ) $ ", NULL, NULL ) ){
			DString_Clear( path );
			DString_ChangeMBS( base, " [^/] + ( / |) $ ", "", 0 );
		}
	}
	if( base->size && path->size ){
		if( base->mbs[ base->size-1 ] != '/' && path->mbs[0] != '/' )
			DString_InsertChar( path, '/', 0 );
		DString_Insert( path, base, 0, 0, 0 );
	}else if( base->size && path->size == 0 ){
		DString_Assign( path, base );
	}
	DString_ChangeMBS( path, "/ %. (/|$)", "/", 0 );
	DString_Delete( base );
}
int DaoVmSpace_SearchResource( DaoVmSpace *self, DString *fname )
{
	DString *path;
	DString_ToMBS( fname );
	if( fname->size == 0 || fname->mbs[0] != '@' ) return 0;
	path = DString_New(1);
	DString_AppendMBS( path, "/@/" );
	DString_AppendMBS( path, fname->mbs + 1 );
	if( TestPath( self, path, DAO_FILE_PATH ) == 0 ){
		DString_SetMBS( path, fname->mbs + 1 );
		Dao_MakePath( self->pathWorking, path );
	}
	if( TestPath( self, path, DAO_FILE_PATH ) ){
		DString_Assign( fname, path );
		DString_Delete( path );
		return 1;
	}
	DString_Delete( path );
	return 0;
}
int DaoVmSpace_SearchPath2( DaoVmSpace *self, DArray *paths, DString *fname, int type )
{
	DString *path = DString_New(1);
	daoint i;
	for(i=0; i<paths->size; ++i){
		DString_Assign( path, paths->items.pString[i] );
		if( path->size && path->mbs[ path->size-1 ] != '/' ) DString_AppendMBS( path, "/" );
		DString_Append( path, fname );
		/*
		   printf( "%s %s\n", paths->items.pString[i]->mbs, path->mbs );
		 */
		if( TestPath( self, path, type ) ){
			DString_Assign( fname, path );
			DString_Delete( path );
			return 1;
		}
	}
	DString_Delete( path );
	return 0;
}
void DaoVmSpace_SearchPath( DaoVmSpace *self, DString *fname, int type, int check )
{
	daoint i;
	char *p;
	DString *path;

	DString_ToMBS( fname );
	if( DaoVmSpace_SearchResource( self, fname ) ) return;

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

	/* Virtual paths are more preferrable than other paths: */
	if( DaoVmSpace_SearchPath2( self, self->virtualPaths, fname, type ) ) goto FreeString;

	/* ./source.dao; ../../source.dao */
	if( strstr( fname->mbs, "./" ) !=NULL || strstr( fname->mbs, "../" ) !=NULL ){

		if( self->pathLoading->size ){
			DString_Assign( path, self->pathLoading->items.pString[0] );
			if( path->size ==0 ) goto FreeString;
		}else if( self->pathWorking->size==0 ) goto FreeString;

		Dao_MakePath( path, fname );
		goto FreeString;
	}

	if( DaoVmSpace_SearchPath2( self, self->pathLoading, fname, type ) ) goto FreeString;

	if( path->size > 0 && path->mbs[ path->size -1 ] != '/' ) DString_AppendMBS( path, "/" );
	DString_Append( path, fname );
	/* printf( "%s %s\n", path->mbs, path->mbs ); */
	if( ! check || TestPath( self, path, type ) ){
		DString_Assign( fname, path );
		goto FreeString;
	}
	if( DaoVmSpace_SearchPath2( self, self->pathSearching, fname, type ) ) goto FreeString;

FreeString:
	DString_Delete( path );
}
void DaoVmSpace_SetPath( DaoVmSpace *self, const char *path )
{
	char *p;
	DString_SetMBS( self->pathWorking, path );
	while( ( p = strchr( self->pathWorking->mbs, '\\') ) !=NULL ) *p = '/';
}
/* Make path only relative to the current loading path or working path: */
void DaoVmSpace_MakePath( DaoVmSpace *self, DString *path )
{
	DString *wpath = self->pathWorking;

	DString_ToMBS( path );
	if( path->size == 0 ) return;
	/* C:\dir\source.dao; /home/...  */
	if( path->size > 1 && (path->mbs[0] == '/' || path->mbs[1] == ':') ) return;

	if( self->pathLoading->size ) wpath = self->pathLoading->items.pString[0];
	if( path->mbs[0] == '.' ){
		Dao_MakePath( wpath, path );
	}else{
		DString *tmp = DString_Copy( wpath );
		if( tmp->size > 0 && tmp->mbs[ tmp->size-1 ] != '/' ) DString_AppendMBS( tmp, "/" );
		DString_Append( tmp, path );
		DString_Assign( path, tmp );
		DString_Delete( tmp );
	}
}
void DaoVmSpace_AddPath( DaoVmSpace *self, const char *path )
{
	DString *tmp, *pstr;
	char *p;

	if( path == NULL || path[0] == '\0' ) return;

	pstr = DString_New(1);
	DString_SetMBS( pstr, path );
	while( ( p = strchr( pstr->mbs, '\\') ) !=NULL ) *p = '/';

	DaoVmSpace_MakePath( self, pstr );

	if( pstr->mbs[pstr->size-1] == '/' ) DString_Erase( pstr, pstr->size-1, 1 );

	if( Dao_IsDir( pstr->mbs ) ){
		int len = pstr->size - strlen( "modules/auxlib" );
		if( len >= 0 ){
			if( DString_FindMBS( pstr, "modules/auxlib", 0 ) == len ) self->hasAuxlibPath = 1;
			if( DString_FindMBS( pstr, "modules/syslib", 0 ) == len ) self->hasSyslibPath = 1;
		}
		DArray_PushFront( self->pathSearching, pstr );
		DString_AppendMBS( pstr, "/addpath.dao" );
		if( TestFile( self, pstr ) ){
			/* the namespace may have got no chance to reduce its reference count: */
			DaoNamespace *ns = DaoVmSpace_LoadDaoModuleExt( self, pstr, NULL, 0 );
			GC_IncRC( ns );
			GC_DecRC( ns );
		}
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

	DaoVmSpace_MakePath( self, pstr );

	if( pstr->mbs[pstr->size-1] == '/' ) DString_Erase( pstr, pstr->size-1, 1 );

	for(i=0; i<self->pathSearching->size; i++ ){
		if( DString_Compare( pstr, self->pathSearching->items.pString[i] ) == 0 ){
			id = i;
			break;
		}
	}
	if( id >= 0 ){
		DString *pathDao = DString_Copy( pstr );
		DString_AppendMBS( pathDao, "/delpath.dao" );
		if( TestFile( self, pathDao ) ){
			DaoNamespace *ns = DaoVmSpace_LoadDaoModuleExt( self, pathDao, NULL, 0 );
			GC_IncRC( ns );
			GC_DecRC( ns );
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

extern DaoTypeBase vmpTyper;

extern DaoTypeBase DaoFdSet_Typer;

extern void DaoInitLexTable();

static void DaoConfigure_FromFile( const char *name )
{
	double number;
	int i, ch, isnum, isint, integer=0, yes;
	FILE *fin = fopen( name, "r" );
	DaoToken *tk1, *tk2;
	DaoLexer *lexer;
	DArray *tokens;
	DString *mbs;
	if( fin == NULL ) return;
	mbs = DString_New(1);
	lexer = DaoLexer_New();
	tokens = lexer->tokens;
	while( ( ch=getc(fin) ) != EOF ) DString_AppendChar( mbs, ch );
	fclose( fin );
	DString_ToLower( mbs );
	DaoLexer_Tokenize( lexer, mbs->mbs, DAO_LEX_ESCAPE );
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
			if( tk2->type >= DTOK_DIGITS_DEC && tk2->type <= DTOK_NUMBER_SCI ){
				isnum = 1;
				if( tk2->type <= DTOK_NUMBER_HEX ){
					isint = 1;
					number = integer = strtol( tk2->string.mbs, NULL, 0 );
				}else{
					number = strtod( tk2->string.mbs, NULL );
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
			}else if( TOKCMP( tk1, "typedcode" )==0 ){
				if( yes <0 ) goto InvalidConfigValue;
				daoConfig.typedcode = yes;
			}else if( TOKCMP( tk1, "optimize" )==0 ){
				if( yes <0 ) goto InvalidConfigValue;
				daoConfig.optimize = yes;
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
		printf( "error: invalid configuration option name: %s!\n", tk1->string.mbs );
		break;
InvalidConfigValue :
		printf( "error: invalid configuration option value: %s!\n", tk2->string.mbs );
		break;
	}
	DaoLexer_Delete( lexer );
	DString_Delete( mbs );
#ifdef DAO_MBSTRING_ONLY
	daoConfig.mbs = 1;
	daoConfig.wcs = 0;
#endif
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
#ifdef DAO_MBSTRING_ONLY
	daoConfig.mbs = 1;
	daoConfig.wcs = 0;
#endif
}



#ifdef DEBUG
static void dao_FakeList_FakeList( DaoProcess *_proc, DaoValue *_p[], int _n );
static void dao_FakeList_Size( DaoProcess *_proc, DaoValue *_p[], int _n );
static void dao_FakeList_Iter( DaoProcess *_proc, DaoValue *_p[], int _n );
static void dao_FakeList_GetItem( DaoProcess *_proc, DaoValue *_p[], int _n );
static void dao_FakeList_SetItem( DaoProcess *_proc, DaoValue *_p[], int _n );

#define FakeListName "FakeList<@T<short|int|float>=int,@S=int>"
#define FakeListName2 "FakeList<@T<short|int|float>,@S>"

static DaoFuncItem dao_FakeList_Meths[] =
{
	/* the names of allocators must be identical to the typer name: */
	{ dao_FakeList_FakeList, FakeListName2 "( size=0 )" },
	{ dao_FakeList_Size, "size( self :FakeList )=>int" },
	{ dao_FakeList_Iter, "iterate( self :FakeList )=>int" },
	{ dao_FakeList_GetItem, "[]( self :FakeList<@T<short|int|float>>, index :int )=>int" },
	{ dao_FakeList_SetItem, "[]=( self :FakeList<@T<short|int|float>>, value :int, index :int )=>int" },
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
  DaoCdata *cdata = DaoCdata_New( FakeList_Typer.core->kernel->abtype, (void*)(daoint)size );
  GC_ShiftRC( retype, cdata->ctype );
  cdata->ctype = retype;
  DaoProcess_PutValue( _proc, (DaoValue*)cdata );
}
static void dao_FakeList_Size( DaoProcess *_proc, DaoValue *_p[], int _n )
{
  daoint size = (daoint) DaoCdata_GetData( & _p[0]->xCdata );
  DaoProcess_PutInteger( _proc, size );
}
static void dao_FakeList_Iter( DaoProcess *_proc, DaoValue *_p[], int _n )
{
  daoint i, size = (daoint) DaoCdata_GetData( & _p[0]->xCdata );
  for(i=0; i<size; ++i) printf( "FakeList::iterate(): %i\n", i );
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



static void DaoBuiltIn_Warn( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoType *type = DaoException_GetType( DAO_WARNING );
	DaoException *self = (DaoException*)DaoException_New( type );
	DaoStream *stream = proc->stdioStream ? proc->stdioStream : proc->vmSpace->stdioStream;
	DString_Assign( self->info, p[0]->xString.data );
	DaoException_Init( self, proc, NULL );
	DaoException_Print( self, stream );
	DaoException_Delete( self );
}
static void DaoBuiltIn_Panic( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoType *type = DaoException_GetType( DAO_ERROR );
	DaoException *self = (DaoException*) DaoValue_CastCstruct( p[0], type );
	DaoObject *object = DaoValue_CastObject( p[0] );
	if( self == NULL && object != NULL ){
		self = (DaoException*) DaoObject_CastCdata( object, type );
	}
	if( self == NULL ) self = (DaoException*)DaoException_New2( type, p[0] );
	DaoException_Init( self, proc, NULL );
	DArray_Append( proc->exceptions, object ? (void*)object : (void*)self );
}
static void DaoBuiltIn_Recover( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoList *list = DaoProcess_PutList( proc );
	DaoStackFrame *frame = proc->topFrame->prev; /* caller frame */
	if( frame == NULL || frame->prev == NULL || frame->prev == proc->firstFrame ) return;
	/* recover() will return a list of exceptions only in the first defer codes: */
	while( proc->exceptions->size > frame->prev->exceptBase ){
		DaoList_Append( list, DArray_Back( proc->exceptions ) );
		DArray_PopBack( proc->exceptions );
	}
}
static void DaoBuiltIn_Recover2( DaoProcess *proc, DaoValue *p[], int n )
{
	daoint i;
	DaoValue *ret = NULL;
	DaoValue *type = p[0];
	DaoStackFrame *frame = proc->topFrame->prev; /* caller frame */

	if( frame == NULL || frame->prev == NULL || frame->prev == proc->firstFrame ) return;
	for(i=proc->exceptions->size-1; i>=frame->prev->exceptBase; --i){
		DaoValue *value = proc->exceptions->items.pValue[i];
		DaoObject *object = DaoValue_CastObject( value );
		if( object ){
			if( DaoClass_ChildOf( object->defClass, type ) ) ret = value;
		}else if( type->type == DAO_CTYPE ){
			if( DaoValue_CastCstruct( value, type->xCtype.cdtype ) != NULL ) ret = value;
		}
		if( ret ){
			DArray_Erase( proc->exceptions, i, 1 );
			break;
		}
	}
	DaoProcess_PutValue( proc, ret ? ret : dao_none_value );
}
static void DaoBuiltIn_Frame( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoValue *retvalue = NULL;
	DaoValue *defvalue = n ? p[0] : NULL;
	DaoVmCode *sect = DaoGetSectionCode( proc->activeCode );
	int ecount;

	if( sect == NULL ) return;
	if( DaoProcess_PushSectionFrame( proc ) == NULL ) return;
	DaoProcess_AcquireCV( proc );
	ecount = proc->exceptions->size;
	DaoProcess_Execute( proc );
	retvalue = proc->stackValues[0];
	DaoProcess_ReleaseCV( proc );
	DaoProcess_PopFrame( proc );
	DaoProcess_SetActiveFrame( proc, proc->topFrame );
	if( proc->exceptions->size > ecount ){
		if( n > 0 ){
			DaoProcess_PutValue( proc, defvalue );
			DArray_Erase( proc->exceptions, ecount, -1 );
		}
	}else{
		DaoProcess_PutValue( proc, retvalue );
	}
}
static void DaoBuiltIn_Test( DaoProcess *proc, DaoValue *p[], int n )
{
	printf( "%i\n", p[0]->type );
}

DaoFuncItem dao_builtin_methods[] =
{
	{ DaoBuiltIn_Warn,      "warn( message : string )" },
	{ DaoBuiltIn_Panic,     "panic( value : any )" },
	{ DaoBuiltIn_Recover,   "recover( ) => list<any>" },
	{ DaoBuiltIn_Recover2,  "recover( eclass : class<Exception> ) => any" },
	{ DaoBuiltIn_Frame,     "frame() [=>@T] => @T" },
	{ DaoBuiltIn_Frame,     "frame( default_value :@T ) [=>@T] => @T" },
#if DEBUG
	{ DaoBuiltIn_Test,      "__test1__( ... )" },
	{ DaoBuiltIn_Test,      "__test2__( ... : int|string )" },
#endif
	{ NULL, NULL }
};



extern void DaoType_Init();




#ifdef DAO_WITH_THREAD
DaoType *dao_type_channel = NULL;
DaoType *dao_type_future  = NULL;

extern DMutex mutex_string_sharing;
extern DMutex mutex_type_map;
extern DMutex mutex_values_setup;
extern DMutex mutex_methods_setup;
extern DMutex mutex_routines_update;
extern DMutex mutex_routine_specialize;
extern DMutex mutex_routine_specialize2;
extern DMutex dao_cdata_mutex;
extern DaoFuncItem dao_mt_methods[];
#endif

DaoType *dao_type_stream = NULL;
extern DaoFuncItem dao_std_methods[];
extern DaoFuncItem dao_io_methods[];

#include<signal.h>
void print_trace();

extern DMap *dao_cdata_bindings;

int DaoVmSpace_TryInitDebugger( DaoVmSpace *self, const char *module )
{
	DaoDebugger *debugger = self->debugger;
	DaoVmSpace_Load( self, module ? module : "debugger" );
	if( self->debugger != debugger ) return 1;
	DaoStream_WriteMBS( self->errorStream, "Failed to enable debugger!\n" );
	return 0;
}
int DaoVmSpace_TryInitProfiler( DaoVmSpace *self, const char *module )
{
	DaoProfiler *profiler = self->profiler;
	DaoVmSpace_Load( self, module ? module : "profiler" );
	if( self->profiler && self->profiler != profiler ) return 1;
	DaoStream_WriteMBS( self->errorStream, "Failed to enable profiler!\n" );
	return 0;
}
int DaoVmSpace_TryInitJIT( DaoVmSpace *self, const char *module )
{
	DaoVmSpace_Load( self, module ? module : "jit" );
	if( dao_jit.Compile ) return 1;
	DaoStream_WriteMBS( self->errorStream, "Failed to enable Just-In-Time compiling!\n" );
	return 0;
}

static int Dao_GetExecutablePath( const char *command, DString *path )
{
	char *PATH = getenv( "PATH" );
	DString paths = DString_WrapMBS( PATH );
	daoint i = 0;

	DString_Reset( path, 0 );
	DString_ToMBS( path );
	if( PATH == NULL ) return 0;

	while( i < paths.size ){
		daoint j = DString_FindChar( & paths, DAO_ENV_PATH_SEP, i );
		daoint len = (j == MAXSIZE) ? paths.size - i : j - i;
		DString base = DString_WrapBytes( paths.mbs + i, len );
		DString_SetMBS( path, command );
		Dao_MakePath( & base, path );
		Dao_NormalizePathSep( path );
		if( Dao_IsFile( path->mbs ) ) return 1;
		if( j == MAXSIZE ) break;
		i = j + 1;
	}
	return 0;
}


DaoVmSpace* DaoVmSpace_MainVmSpace()
{
	return mainVmSpace;
}
DaoVmSpace* DaoInit( const char *command )
{
	DString *mbs, *mbs2;
	DaoVmSpace *vms;
	DaoNamespace *ns, *ns2;
	DaoType *type, *type1, *type2, *type3, *type4;
	DaoModuleOnLoad fpter;
	void *handle;
	daoint i, n;

	if( mainVmSpace ) return mainVmSpace;

	dao_cdata_bindings = DHash_New(0,0);

	/* signal( SIGSEGV, print_trace ); */
	/* signal( SIGABRT, print_trace ); */

#ifdef DAO_WITH_THREAD
	DMutex_Init( & mutex_string_sharing );
	DMutex_Init( & mutex_type_map );
	DMutex_Init( & mutex_values_setup );
	DMutex_Init( & mutex_methods_setup );
	DMutex_Init( & mutex_routines_update );
	DMutex_Init( & mutex_routine_specialize );
	DMutex_Init( & mutex_routine_specialize2 );
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

	DaoGC_Start();

#if 0
#warning"-------------using concurrent GC by default!"
	DaoCGC_Start();
#endif

	dao_type_udf = DaoType_New( "?", DAO_UDT, NULL, NULL );
	dao_type_any = DaoType_New( "any", DAO_ANY, NULL, NULL );
	dao_type_int = DaoType_New( "int", DAO_INTEGER, NULL, NULL );
	dao_type_float = DaoType_New( "float", DAO_FLOAT, NULL, NULL );
	dao_type_double = DaoType_New( "double", DAO_DOUBLE, NULL, NULL );
	dao_type_complex = DaoType_New( "complex", DAO_COMPLEX, NULL, NULL );
	dao_type_string = DaoType_New( "string", DAO_STRING, NULL, NULL );
	dao_type_routine = DaoType_New( "routine<=>?>", DAO_ROUTINE, (DaoValue*)dao_type_udf, NULL );

	mainVmSpace = vms = DaoVmSpace_New();

	DString_Reserve( mainVmSpace->startPath, 512 );
	getcwd( mainVmSpace->startPath->mbs, 511 );
	DString_Reset( mainVmSpace->startPath, strlen( mainVmSpace->startPath->mbs ) );
	Dao_NormalizePathSep( mainVmSpace->startPath );

	DString_AppendPathSep( mainVmSpace->startPath );
	if( command ){
		DString *mbs = mainVmSpace->daoBinPath;
		int absolute = command[0] == '/';
		int relative = command[0] == '.';
		DString_SetMBS( mbs, command );
		Dao_NormalizePathSep( mbs );
#ifdef WIN32
		absolute = isalpha( command[0] ) && command[1] == ':';
#endif
		if( absolute == 0 ){
			if( relative ){
				Dao_MakePath( mainVmSpace->startPath, mbs );
			}else{
				Dao_GetExecutablePath( command, mbs );
			}
		}
#ifdef DEBUG
		if( ! Dao_IsFile( mbs->mbs ) ){
			printf( "WARNING: the path of the executable cannot be located!" );
		}
#endif
		while( (i = mbs->size) && mbs->mbs[i-1] != '/' && mbs->mbs[i-1] != '\\' ) mbs->size --;
		mbs->mbs[ mbs->size ] = 0;
	}

	ns = vms->nsInternal;

	DaoProcess_CacheValue( vms->mainProcess, (DaoValue*) dao_type_udf );
	DaoProcess_CacheValue( vms->mainProcess, (DaoValue*) dao_type_routine );
	DaoNamespace_AddTypeConstant( ns, dao_type_any->name, dao_type_any );
	DaoNamespace_AddTypeConstant( ns, dao_type_int->name, dao_type_int );
	DaoNamespace_AddTypeConstant( ns, dao_type_float->name, dao_type_float );
	DaoNamespace_AddTypeConstant( ns, dao_type_double->name, dao_type_double );
	DaoNamespace_AddTypeConstant( ns, dao_type_complex->name, dao_type_complex );
	DaoNamespace_AddTypeConstant( ns, dao_type_string->name, dao_type_string );

	dao_type_none = DaoNamespace_MakeValueType( ns, dao_none_value );
	dao_type_for_iterator = DaoParser_ParseTypeName( "tuple<valid:int,iterator:any>", ns, NULL );

	DString_SetMBS( dao_type_for_iterator->name, "for_iterator" );
	DaoNamespace_AddType( ns, dao_type_for_iterator->name, dao_type_for_iterator );

	dao_type_tuple = DaoParser_ParseTypeName( "tuple<...>", ns, NULL );

	dao_array_types[DAO_NONE] = DaoNamespace_MakeType( ns, "array", DAO_ARRAY, NULL, & dao_type_none, 1 );
	dao_array_types[DAO_INTEGER] = DaoNamespace_MakeType( ns, "array", DAO_ARRAY, NULL, & dao_type_int, 1 );
	dao_array_types[DAO_FLOAT]   = DaoNamespace_MakeType( ns, "array", DAO_ARRAY, NULL, & dao_type_float, 1 );
	dao_array_types[DAO_DOUBLE]  = DaoNamespace_MakeType( ns, "array", DAO_ARRAY, NULL, & dao_type_double, 1 );
	dao_array_types[DAO_COMPLEX] = DaoNamespace_MakeType( ns, "array", DAO_ARRAY, NULL, & dao_type_complex, 1 );
	dao_type_array_empty = dao_array_types[DAO_NONE];

	DaoNamespace_TypeDefine( ns, "enum<false:true>", "bool" );

#if 0
	/*
	// Do not use "#ifdef DEBUG", it will make bytecode encoded by interpreter
	// compiled in debug mode, fail the decoding by interpreter compiled in
	// release mode.
	*/
	DaoNamespace_TypeDefine( ns, "int", "short" );
	DaoNamespace_WrapType( vms->nsInternal, dao_FakeList_Typer, 1 );
	DaoNamespace_TypeDefine( ns, "FakeList<short>", "FakeList<int>" );
#endif

#ifdef DAO_WITH_NUMARRAY
	DaoNamespace_SetupType( vms->nsInternal, & numarTyper );
#endif

	DaoNamespace_SetupType( vms->nsInternal, & stringTyper );
	DaoNamespace_SetupType( vms->nsInternal, & comTyper );
	dao_type_list_template = DaoNamespace_WrapGenericType( ns, & listTyper, DAO_LIST );
	dao_type_map_template = DaoNamespace_WrapGenericType( ns, & mapTyper, DAO_MAP );

	dao_type_list_any = DaoType_Specialize( dao_type_list_template, NULL, 0 );
	dao_type_map_any  = DaoType_Specialize( dao_type_map_template, NULL, 0 );

	dao_type_list_empty = DaoType_Copy( dao_type_list_any );
	dao_type_map_empty = DaoType_Copy( dao_type_map_any );
	DaoProcess_CacheValue( vms->mainProcess, (DaoValue*) dao_type_list_empty );
	DaoProcess_CacheValue( vms->mainProcess, (DaoValue*) dao_type_map_empty );

	GC_ShiftRC( dao_type_complex, comTyper.core->kernel->abtype );
	GC_ShiftRC( dao_type_string, stringTyper.core->kernel->abtype );
	comTyper.core->kernel->abtype = dao_type_complex;
	stringTyper.core->kernel->abtype = dao_type_string;

	ns2 = DaoVmSpace_GetNamespace( vms, "io" );
	DaoNamespace_AddConstValue( ns, "io", (DaoValue*) ns2 );
	dao_type_stream = DaoNamespace_WrapType( ns2, & streamTyper, 0 );
	GC_ShiftRC( dao_type_stream, vms->stdioStream->ctype );
	GC_ShiftRC( dao_type_stream, vms->errorStream->ctype );
	vms->stdioStream->ctype = dao_type_stream;
	vms->errorStream->ctype = dao_type_stream;
	DaoNamespace_WrapFunctions( ns2, dao_io_methods );
	DaoNamespace_AddConstValue( ns2, "stdio",  (DaoValue*) vms->stdioStream );
	DaoNamespace_AddConstValue( ns2, "stderr", (DaoValue*) vms->errorStream );

	dao_default_cdata.ctype = DaoNamespace_WrapType( vms->nsInternal, & defaultCdataTyper, 1 );
	GC_IncRC( dao_default_cdata.ctype );

	DaoException_Setup( vms->nsInternal );

#ifdef DAO_WITH_CONCURRENT
	ns2 = DaoVmSpace_GetNamespace( vms, "mt" );
	DaoNamespace_AddConstValue( ns, "mt", (DaoValue*) ns2 );
	dao_type_channel = DaoNamespace_WrapType( ns2, & channelTyper, 0 );
	dao_type_future  = DaoNamespace_WrapType( ns2, & futureTyper, 0 );
	DaoNamespace_WrapFunctions( ns2, dao_mt_methods );
#endif
	DaoNamespace_SetupType( vms->nsInternal, & vmpTyper );

	ns2 = DaoVmSpace_GetNamespace( vms, "std" );
	DaoNamespace_AddConstValue( ns, "std", (DaoValue*) ns2 );
	DaoNamespace_WrapFunctions( ns2, dao_std_methods );

	DaoNamespace_WrapFunctions( ns, dao_builtin_methods );

	DaoNamespace_AddParent( vms->mainNamespace, vms->nsInternal );

	DaoVmSpace_InitPath( vms );

	/*
	   printf( "initialized...\n" );
	 */
	DString_Delete( mbs );
	return vms;
}
extern DaoType* DaoParser_ParseTypeName( const char *type, DaoNamespace *ns, DaoClass *cls );
extern DaoType *simpleTypes[ DAO_ARRAY ];
void DaoQuit()
{
	int i;
	DNode *it = NULL;

	/* TypeTest(); */
#ifdef DAO_WITH_CONCURRENT
	DaoCallServer_Stop();
#endif

	if( daoConfig.iscgi ) return;

	if( (mainVmSpace->options & DAO_OPTION_PROFILE) && mainVmSpace->profiler ){
		DaoProfiler *profiler = mainVmSpace->profiler;
		if( profiler->Report ) profiler->Report( profiler, mainVmSpace->stdioStream );
	}

	GC_DecRC( dao_default_cdata.ctype );
	dao_default_cdata.ctype = NULL;

	DaoVmSpace_DeleteData( mainVmSpace );
	for(i=0; i<DAO_ARRAY; i++){
		GC_DecRC( simpleTypes[i] );
		simpleTypes[i] = NULL;
	}
	DaoGC_Finish();
#ifdef DEBUG
	for(it=DMap_First(mainVmSpace->nsModules); it; it=DMap_Next(mainVmSpace->nsModules,it) ){
		printf( "Warning: namespace/module \"%s\" is not collected with reference count %i!\n",
				((DaoNamespace*)it->value.pValue)->name->mbs, it->value.pValue->xBase.refCount );
	}
#endif
	DaoVmSpace_Delete( mainVmSpace );
	DMap_Delete( dao_cdata_bindings );
	dao_cdata_bindings = NULL;
	dao_type_stream = NULL;
	mainVmSpace = NULL;
	mainProcess = NULL;
	if( dao_jit.Quit ){
		dao_jit.Quit();
		dao_jit.Quit = NULL;
		dao_jit.Free = NULL;
		dao_jit.Compile = NULL;
		dao_jit.Execute = NULL;
	}
#ifdef DAO_WITH_THREAD
	DMutex_Destroy( & mutex_string_sharing );
	DMutex_Destroy( & mutex_type_map );
	DMutex_Destroy( & mutex_values_setup );
	DMutex_Destroy( & mutex_methods_setup );
	DMutex_Destroy( & mutex_routines_update );
	DMutex_Destroy( & mutex_routine_specialize );
	DMutex_Destroy( & mutex_routine_specialize2 );
	DMutex_Destroy( & dao_cdata_mutex );
#endif
}
DaoNamespace* DaoVmSpace_FindModule( DaoVmSpace *self, DString *fname )
{
	DaoNamespace* ns = DaoVmSpace_FindNamespace( self, fname );
	if( ns ) return ns;
	DaoVmSpace_CompleteModuleName( self, fname, DAO_MODULE_ANY );
	return DaoVmSpace_FindNamespace( self, fname );
}
DaoNamespace* DaoVmSpace_LoadModule( DaoVmSpace *self, DString *fname )
{
	DaoNamespace *ns = NULL;
	int modtype = DAO_MODULE_ANY;
	if( self->options & DAO_OPTION_COMP_BC ) modtype = DAO_MODULE_DAO | DAO_MODULE_DLL;
	switch( DaoVmSpace_CompleteModuleName( self, fname, modtype ) ){
	case DAO_MODULE_DAC :
	case DAO_MODULE_DAO : ns = DaoVmSpace_LoadDaoModule( self, fname ); break;
	case DAO_MODULE_DLL : ns = DaoVmSpace_LoadDllModule( self, fname ); break;
	}
	return ns;
}

