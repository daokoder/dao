/*
// This file is part of the virtual machine for the Dao programming language.
// Copyright (C) 2006-2012, Limin Fu. Email: daokoder@gmail.com
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this 
// software and associated documentation files (the "Software"), to deal in the Software 
// without restriction, including without limitation the rights to use, copy, modify, merge, 
// publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons 
// to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or 
// substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef DAO_VMSPACE_H
#define DAO_VMSPACE_H

#include"stdio.h"

#include"daoType.h"
#include"daoThread.h"

enum DaoPathType{ DAO_FILE_PATH, DAO_DIR_PATH };

extern const char *const dao_copy_notice;

/* Dao Virtual Machine Space:
 * For handling:
 * -- Execution options and configuration;
 * -- Module loading and namespace management;
 * -- C types and functions defined in modules;
 * -- Path management;
 */
struct DaoVmSpace
{
    DAO_DATA_COMMON;

    /* To run the main script specified in the commad line (or the first loaded one),
     * or scripts from an interactive console. */
    DaoProcess  *mainProcess;
    /* To store globals in the main script,
     * or scripts from an interactive console. */
    DaoNamespace  *mainNamespace;

    /* for some internal scripts and predefined objects or types */
    DaoNamespace  *nsInternal;

    DaoStream  *stdioStream;
    DaoStream  *errorStream;

    DMap    *allProcesses;
    DArray  *processes;

	DString *mainSource;
    DString *pathWorking;
    DArray  *nameLoading;
    DArray  *pathLoading;
    DArray  *pathSearching; /* <DString*> */

    int options;
    char stopit;
    char safeTag;
    char evalCmdline;
	char hasAuxlibPath;
	char hasSyslibPath;

    DMap  *vfiles;
	DMap  *vmodules;

    /* map full file name (including path and suffix) to module namespace */
    DMap  *nsModules; /* No GC for this, namespaces should remove themselves from this; */
    DMap  *allTokens;

    DaoUserHandler *userHandler;

    char* (*ReadLine)( const char *prompt );
    void  (*AddHistory)( const char *cmd );

#ifdef DAO_WITH_THREAD
    DMutex  mutexLoad;
    DMutex  mutexProc;
#endif
};

extern DaoVmSpace *mainVmSpace;

DAO_DLL DaoVmSpace* DaoVmSpace_New();
/* DaoVmSpace is not handled by GC, it should be deleted manually. 
 * Normally, DaoVmSpace structures are allocated in the beginning of a program and 
 * persist until the program exits. So DaoVmSpace_Delete() is rarely needed to be called.
 */
DAO_DLL void DaoVmSpace_Delete( DaoVmSpace *self );

DAO_DLL void DaoVmSpace_Lock( DaoVmSpace *self );
DAO_DLL void DaoVmSpace_Unlock( DaoVmSpace *self );

DAO_DLL int DaoVmSpace_ParseOptions( DaoVmSpace *self, DString *options );

DAO_DLL int DaoVmSpace_Compile( DaoVmSpace *self, DaoNamespace *ns, DString *src, int rpl );
DAO_DLL int DaoVmSpace_RunMain( DaoVmSpace *self, DString *file );

DAO_DLL DaoNamespace* DaoVmSpace_Load( DaoVmSpace *self, DString *file, int run );
DAO_DLL DaoNamespace* DaoVmSpace_LoadModule( DaoVmSpace *self, DString *fname );
DAO_DLL DaoNamespace* DaoVmSpace_FindModule( DaoVmSpace *self, DString *fname );
DAO_DLL DaoNamespace* DaoVmSpace_FindNamespace( DaoVmSpace *self, DString *name );

DAO_DLL void DaoVmSpace_SearchPath( DaoVmSpace *self, DString *fname, int type, int check );

DAO_DLL void DaoVmSpace_SetPath( DaoVmSpace *self, const char *path );
DAO_DLL void DaoVmSpace_AddPath( DaoVmSpace *self, const char *path );
DAO_DLL void DaoVmSpace_DelPath( DaoVmSpace *self, const char *path );

DAO_DLL DaoTypeBase* DaoVmSpace_GetTyper( short type );

#endif
