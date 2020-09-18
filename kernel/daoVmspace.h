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

#ifndef DAO_VMSPACE_H
#define DAO_VMSPACE_H

#include"stdio.h"

#include"daoType.h"
#include"daoThread.h"
#include"daoOptimizer.h"
#include"daoInferencer.h"
#include"daoProcess.h"
#include"daoBytecode.h"

enum DaoPathType
{
	DAO_FILE_PATH,
	DAO_DIR_PATH
};

enum DaoModuleTypes
{
	DAO_MODULE_NONE = 0,
	DAO_MODULE_DAC  = 1,
	DAO_MODULE_DAO  = 2,
	DAO_MODULE_DLL  = 4,
	DAO_MODULE_ANY  = DAO_MODULE_DAC|DAO_MODULE_DAO|DAO_MODULE_DLL
};

enum DaoModuleRunMode
{
	DAO_MODULE_MAIN_NONE ,
	DAO_MODULE_MAIN_ONCE ,
	DAO_MODULE_MAIN_ALWAYS
};


/*
// Dao Virtual Machine Space:
// For handling:
// -- Execution options and configuration;
// -- Module loading and namespace management;
// -- C types and functions defined in modules;
// -- Path management;
//
// Notes:
// Though it is very desirable to support completely independent vm spaces with
// independent garbage collectors, there are various practical issues which may
// make such support not worthy the efforts.
//
// First of all, many functions will need to take additional parameters, which
// would make the API tedious to use. Second, the values will need to be space
// aware such that passing values between different vm spaces can be prevented,
// to make sure the garbage collectors can work independently. This is hard to
// guarantee with the current design where Dao values are directly accessible.
//
// Things could be simplified if an additional field of vm space pointer or id
// is added to Dao values, but there will still be much hassle.
*/
struct DaoVmSpace
{
	DAO_VALUE_COMMON;

	/*
	// The common namespace for standard types and values;
	*/
	DaoNamespace  *daoNamespace;

	/*
	// Namespace for the main script or scripts from an interactive console.
	*/
	DaoNamespace  *mainNamespace;

	/*
	// To run the main script specified in the commad line (or the first loaded one),
	// or scripts from an interactive console.
	*/
	DaoProcess  *mainProcess;

	DaoStream  *stdioStream;
	DaoStream  *errorStream;

	DString *daoBinFile;
	DString *daoBinPath;
	DString *startPath;
	DString *mainSource;
	DString *pathWorking;
	DList   *nameLoading;
	DList   *pathLoading;
	DList   *pathSearching; /* <DString*> */
	DList   *virtualPaths;  /* <DString*> */
	DList   *sourceArchive;
	DList   *argParams;

	int     stopit;
	int     options;
	int     evalCmdline;
	int     auxLoaded;

	DMap   *vfiles;
	DMap   *vmodules;

	/* map full file name (including path and suffix) to module namespace */
	DMap   *nsModules; /* No GC for this, namespaces should remove themselves from this; */
	DMap   *nsPlugins; /* Modules that can be used without explicit loading; */
	DMap   *nsRefs;

	DMap   *typeKernels;
	DMap   *spaceData;

	DMap   *cdataWrappers;  /* VM space unique wrappers for Cdata objects; */

	DMap   *allProcesses;
	DMap   *allRoutines;
	DMap   *allParsers;
	DMap   *allByteCoders;
	DMap   *allInferencers;
	DMap   *allOptimizers;

	DList  *processes;
	DList  *routines;
	DList  *parsers;
	DList  *byteCoders;
	DList  *inferencers;
	DList  *optimizers;
	DList  *typeCores;

	DaoDebugger  *debugger;
	DaoProfiler  *profiler;
	DaoHandler   *handler;

	DaoType  *typeNone;
	DaoType  *typeUdf;
	DaoType  *typeAny;
	DaoType  *typeBool;
	DaoType  *typeInt;
	DaoType  *typeFloat;
	DaoType  *typeComplex;
	DaoType  *typeString;
	DaoType  *typeEnum;
	DaoType  *typeTuple;
	DaoType  *typeArray;
	DaoType  *typeArrayEmpty;
	DaoType  *typeList;
	DaoType  *typeListEmpty;
	DaoType  *typeListAny;
	DaoType  *typeMap;
	DaoType  *typeMapEmpty;
	DaoType  *typeMapAny;
	DaoType  *typeRoutine;
	DaoType  *typeCdata;
	DaoType  *typeIteratorInt;
	DaoType  *typeIteratorAny;
	DaoType  *typeException;
	DaoType  *typeWarning;
	DaoType  *typeError;
	DaoType  *typeFuture;
	DaoType  *typeChannel;
	DaoType  *typeStream;
	DaoType  *typeIODevice;
	DaoType  *typeArrays[DAO_COMPLEX+1];

#ifdef DAO_WITH_THREAD
	DMutex    moduleMutex;
	DMutex    cacheMutex;
	DMutex    miscMutex;
#endif

	void  *taskletServer;

	char* (*ReadLine)( const char *prompt, DString *buffer );
	int   (*AddHistory)( const char *cmd );
};

extern DaoVmSpace *masterVmSpace;

DAO_DLL DaoVmSpace* DaoVmSpace_New();
/*
// DaoVmSpace is not handled by GC, it should be deleted manually.
// Normally, DaoVmSpace structures are allocated in the beginning of a program and
// persist until the program exits. So DaoVmSpace_Delete() is rarely needed to be called.
*/
DAO_DLL void DaoVmSpace_Delete( DaoVmSpace *self );
DAO_DLL void DaoVmSpace_TryDelete( DaoVmSpace *self );

DAO_DLL void DaoVmSpace_Lock( DaoVmSpace *self );
DAO_DLL void DaoVmSpace_Unlock( DaoVmSpace *self );

DAO_DLL void DaoVmSpace_LockCache( DaoVmSpace *self );
DAO_DLL void DaoVmSpace_UnlockCache( DaoVmSpace *self );

DAO_DLL int DaoVmSpace_ParseOptions( DaoVmSpace *self, const char *options );

DAO_DLL int DaoVmSpace_RunMain( DaoVmSpace *self, const char *file );

DAO_DLL DaoNamespace* DaoVmSpace_Load( DaoVmSpace *self, const char *file );
DAO_DLL DaoNamespace* DaoVmSpace_LoadEx( DaoVmSpace *self, const char *file, int run );

DAO_DLL DaoNamespace* DaoVmSpace_LoadModule( DaoVmSpace *self, DString *name, DaoParser *parser );
DAO_DLL DaoNamespace* DaoVmSpace_FindNamespace( DaoVmSpace *self, DString *name );

DAO_DLL int DaoVmSpace_TestFile( DaoVmSpace *self, DString *fname );
DAO_DLL int DaoVmSpace_ReadFile( DaoVmSpace *self, DString *fname, DString *source );
DAO_DLL int DaoVmSpace_SearchResource( DaoVmSpace *self, DString *fname, DString *search );

DAO_DLL void DaoVmSpace_ConvertPath( DaoVmSpace *self, DString *path );

DAO_DLL int DaoVmSpace_SearchModulePath( DaoVmSpace *self, DString *fname, int lib );
DAO_DLL int DaoVmSpace_CompleteModuleName( DaoVmSpace *self, DString *fname, int lib );

DAO_DLL void DaoVmSpace_SetPath( DaoVmSpace *self, const char *path );
DAO_DLL void DaoVmSpace_AddPath( DaoVmSpace *self, const char *path );
DAO_DLL void DaoVmSpace_DelPath( DaoVmSpace *self, const char *path );

DAO_DLL const char* DaoVmSpace_GetCopyNotice();

DAO_DLL void DaoVmSpace_AddKernel( DaoVmSpace *self, DaoTypeCore *core, DaoTypeKernel *kernel );
DAO_DLL DaoTypeKernel* DaoVmSpace_GetKernel( DaoVmSpace *self, DaoTypeCore *core );

DAO_DLL DaoType* DaoVmSpace_GetType( DaoVmSpace *self, DaoTypeCore *core );
DAO_DLL DaoType* DaoVmSpace_GetCommonType( DaoVmSpace *self, int type, int subtype );
DAO_DLL DaoType* DaoVmSpace_MakeExceptionType( DaoVmSpace *self, const char *name );

DAO_DLL DaoRoutine* DaoVmSpace_AcquireRoutine( DaoVmSpace *self );
DAO_DLL DaoParser* DaoVmSpace_AcquireParser( DaoVmSpace *self );
DAO_DLL DaoByteCoder* DaoVmSpace_AcquireByteCoder( DaoVmSpace *self );
DAO_DLL DaoInferencer* DaoVmSpace_AcquireInferencer( DaoVmSpace *self );
DAO_DLL DaoOptimizer* DaoVmSpace_AcquireOptimizer( DaoVmSpace *self );
DAO_DLL void DaoVmSpace_ReleaseRoutine( DaoVmSpace *self, DaoRoutine *rout );
DAO_DLL void DaoVmSpace_ReleaseParser( DaoVmSpace *self, DaoParser *parser );
DAO_DLL void DaoVmSpace_ReleaseByteCoder( DaoVmSpace *self, DaoByteCoder *byteCoder );
DAO_DLL void DaoVmSpace_ReleaseInferencer( DaoVmSpace *self, DaoInferencer *inferencer );
DAO_DLL void DaoVmSpace_ReleaseOptimizer( DaoVmSpace *self, DaoOptimizer *optimizer );

void DaoAux_Delete( DMap *aux );

DAO_DLL DaoCdata* DaoVmSpace_MakeCdata( DaoVmSpace *self, DaoType *type, void *data, int own );
DAO_DLL void DaoVmSpace_ReleaseCdata( DaoVmSpace *self, DaoType *type, void *data );
DAO_DLL void DaoVmSpace_UpdateCdata( DaoVmSpace *self, DaoCdata *cdata, void *data );

#endif
