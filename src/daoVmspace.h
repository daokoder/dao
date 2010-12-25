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

#ifndef DAO_VMSPACE_H
#define DAO_VMSPACE_H

#include"stdio.h"

#include"daoType.h"
#include"daoThread.h"

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
    DaoVmProcess  *mainProcess;
    /* To store globals in the main script,
     * or scripts from an interactive console. */
    DaoNameSpace  *mainNamespace;

    /* for some internal scripts and predefined objects or types */
    DaoNameSpace  *nsInternal;

    DaoThdMaster  *thdMaster;
    DaoStream     *stdStream;

    DArray *processes;

    DString *pathWorking;
    DArray  *nameLoading;
    DArray  *pathLoading;
    DArray  *pathSearching; /* <DString*> */

    DString *fileName;
    DString *source;
    int options;
    char stopit;
    char safeTag;
    char evalCmdline;

    DMap  *vfiles;

    /* map full file name (including path and suffix) to module namespace */
    DMap  *nsModules;
    /* map file name (excluding path and suffix) to module namespace:
     * mainly for requiring modules in load statement */
    DMap  *modRequire;
    DMap  *allTokens;

    DaoUserHandler *userHandler;

    char* (*ReadLine)( const char *prompt );
    void  (*AddHistory)( const char *cmd );

#ifdef DAO_WITH_THREAD
    DMutex  mutexLoad;
    DMutex  mutexProc;
    int locked;
#endif
};

DaoVmSpace* DaoVmSpace_New();
/* DaoVmSpace is not handled by GC, it should be deleted manually. 
 * Normally, DaoVmSpace structures are allocated in the beginning of a program and 
 * persist until the program exits. So DaoVmSpace_Delete() is rarely needed to be called.
 */
void DaoVmSpace_Delete( DaoVmSpace *self );

void DaoVmSpace_Lock( DaoVmSpace *self );
void DaoVmSpace_Unlock( DaoVmSpace *self );

int DaoVmSpace_ParseOptions( DaoVmSpace *self, DString *options );

int DaoVmSpace_Compile( DaoVmSpace *self, DaoNameSpace *ns, DString *src, int rpl );
int DaoVmSpace_RunMain( DaoVmSpace *self, DString *file );

DaoNameSpace* DaoVmSpace_Load( DaoVmSpace *self, DString *file );
DaoNameSpace* DaoVmSpace_LoadModule( DaoVmSpace *self, DString *fname, DArray *reqns );
DaoNameSpace* DaoVmSpace_LoadDaoModule( DaoVmSpace *self, DString *fname );
DaoNameSpace* DaoVmSpace_LoadDllModule( DaoVmSpace *self, DString *fname, DArray *reqns );

void DaoVmSpace_MakePath( DaoVmSpace *self, DString *fname, int check );

void DaoVmSpace_SetPath( DaoVmSpace *self, const char *path );
void DaoVmSpace_AddPath( DaoVmSpace *self, const char *path );
void DaoVmSpace_DelPath( DaoVmSpace *self, const char *path );

DaoVmSpace* DaoInit();

#endif
