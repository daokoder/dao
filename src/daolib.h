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

#ifndef DAO_LIB_H
#define DAO_LIB_H

#ifndef DAO_DIRECT_API
#define DAO_DIRECT_API
#endif

#include"dao.h"

#define DAO_VERSION "1.2"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DVarray    DVarray;
typedef struct DVaTuple   DVaTuple;
typedef struct DPtrTuple  DPtrTuple;

typedef struct DRoutine        DRoutine;

typedef struct DaoThdMaster    DaoThdMaster;
typedef struct DaoFunCurry     DaoFunCurry;
typedef struct DaoCDataCore    DaoCDataCore;

/* Initialize the Dao Virtual Machine libraray.
 * Return a virtual machine space ready to use.
 * Only the following data types can be used before calling DaoInit():
 * XXX
 * DString, DArray, DMap.
 */
DaoVmSpace* DaoInit();
void DaoQuit();

/* =====================================================================================
 * Basic data structure.
 * =====================================================================================*/

/* ----------------------------------------------------------------------------------------
 * Array Manipulation:
 * It may hold integer or pointer types.
 * 
 * "type" should be zero, D_STRING, D_ARRAY or D_MAP:
 * ----------------------------------------------------------------------------------------*/
DArray* DArray_New( short type );
void DArray_Delete( DArray *self );
/* Array assignment must be between arrays with the same trait, i.e. they must be created
 * with the same parameter in DArray_New();
 * Elements are copied if the arrays are created with DArray_New(1). */
void DArray_Assign( DArray *left, DArray *right );
void DArray_Swap( DArray *left, DArray *right );
void DArray_Resize( DArray *self, size_t size, void *value );
void DArray_Clear( DArray *self );
void DArray_Insert( DArray *self, void *val, size_t id );
void DArray_Erase( DArray *self, size_t start, size_t n );
void DArray_PushFront( DArray *self, void *val );
void DArray_PopFront( DArray *self );
void DArray_PushBack( DArray *self, void *val );
void DArray_PopBack( DArray *self );
void  DArray_SetItem( DArray *self, size_t index, void *value );
/* void* DArray_GetItem( DArray *self, size_t index ); */
void* DArray_Front( DArray *self );
void* DArray_Back( DArray *self );
void** DArray_GetBuffer( DArray *self );

/* -------------------------------------------------------------------------------------
 * Map
 * -------------------------------------------------------------------------------------*/
/* Iteration on nodes. */
DNode* DNode_Next( DNode *self );
DNode* DNode_First( DNode *self );
/* Get key and value, for iteration purpose. They are not copied to return!  */
void* DNode_GetKey( DNode *self );
void* DNode_GetValue( DNode *self );

DMap* DMap_New( short kt, short vt );
void DMap_Delete( DMap *self );
void DMap_Clear( DMap *self );
void DMap_Erase( DMap *self, void *key );
DNode* DMap_Insert( DMap *self, void *key, void *value );

DNode* DMap_Find( DMap *self, void *key );
DNode* DMap_First( DMap *self );
/* Find maximum key less than "key" */
DNode* DMap_FindML( DMap *self, void *key );
/* Find minimum key greater than "key" */
DNode* DMap_FindMG( DMap *self, void *key );

/* =======================================================================================
 * Dao data types:
 * =======================================================================================*/

/* ---------------------
 * Dao List 
 * ---------------------*/
DaoList* DaoList_New();
void DaoList_Delete( DaoList *self );
void DaoList_Clear( DaoList *self );

int  DaoList_Size( DaoList *self );
DValue DaoList_Front( DaoList *self );
DValue DaoList_Back( DaoList *self );
DValue DaoList_GetItem( DaoList *self, int pos );
DaoTuple* DaoList_ToTuple( DaoList *self, DaoTuple *proto );
void DaoList_SetItem( DaoList *self, DValue item, int pos );
void DaoList_Insert( DaoList *self, DValue item, int pos );
void DaoList_Append( DaoList *self, DValue item );
void DaoList_PushFront( DaoList *self, DValue item );
void DaoList_PushBack( DaoList *self, DValue item );
void DaoList_PopFront( DaoList *self );
void DaoList_PopBack( DaoList *self );
void DaoList_Erase( DaoList *self, int index );

/* Dao Map*/
DaoMap* DaoMap_New( int hashing );
void DaoMap_Delete( DaoMap *self );
void DaoMap_Clear( DaoMap *self );

int DaoMap_Size( DaoMap *self );
int DaoMap_Insert( DaoMap *self, DValue key, DValue value );
void DaoMap_Erase( DaoMap *self, DValue key );
DValue DaoMap_GetValue( DaoMap *self, DValue key  );
void DaoMap_InsertMBS( DaoMap *self, const char *key, DValue value );
void DaoMap_InsertWCS( DaoMap *self, const wchar_t *key, DValue value );
void DaoMap_EraseMBS ( DaoMap *self, const char *key );
void DaoMap_EraseWCS ( DaoMap *self, const wchar_t *key );
DValue DaoMap_GetValueMBS( DaoMap *self, const char *key  );
DValue DaoMap_GetValueWCS( DaoMap *self, const wchar_t *key  );

/* ---------------------
 * Dao Tuple 
 * ---------------------*/
DaoTuple* DaoTuple_New( int size );
void DaoTuple_Delete( DaoTuple *self );

int  DaoTuple_Size( DaoTuple *self );
DaoList* DaoTuple_ToList( DaoTuple *self );

/* =======================================================================================
 * Dao NameSpace
 * =======================================================================================*/
DaoNameSpace* DaoNameSpace_New( DaoVmSpace *vms );
void DaoNameSpace_Delete( DaoNameSpace *self );

/* return virtual register id of the const named "name". return -1 if not found. */
int DaoNameSpace_FindConst( DaoNameSpace *self, DString *name );
int DaoNameSpace_AddConst( DaoNameSpace *self, DString *name, DValue value, int pm );
void DaoNameSpace_SetConst( DaoNameSpace *self, int index, DValue value );
DValue DaoNameSpace_GetConst( DaoNameSpace *self, int i );

int DaoNameSpace_FindVariable( DaoNameSpace *self, DString *name );
int DaoNameSpace_AddVariable( DaoNameSpace *self, DString *s, DValue v, DaoType *t, int p );
int DaoNameSpace_SetVariable( DaoNameSpace *self, int index, DValue var );
DValue DaoNameSpace_GetVariable( DaoNameSpace *self, int i );

void DaoNameSpace_SetData( DaoNameSpace *self, DString *name, DValue );
DValue DaoNameSpace_GetData( DaoNameSpace *self, DString *name );

int DaoCData_ChildOf( DaoTypeBase *self, DaoTypeBase *super );

/* =======================================================================================
 * Class and class instance type:
 * =======================================================================================*/
int DaoClass_GetData( DaoClass *self, DString *name, DValue *value, DaoClass *thisClass, DValue **d2 );

/* Create class instance, and initialize its members with default values */
DaoObject* DaoObject_New( DaoClass *klass, DaoObject *that, int offset );
void DaoObject_Delete( DaoObject *self );

DaoClass* DaoObject_MyClass( DaoObject *self );
int DaoObject_ChildOf( DaoObject *self, DaoObject *obj );

int DaoObject_SetData( DaoObject *self, DString *name, DValue value, DaoObject *objThis );
int DaoObject_GetData( DaoObject *self, DString *name, DValue *data, DaoObject *objThis, DValue **d2 );

/* =======================================================================================
 * Regular expression matching
 * =======================================================================================*/
DaoRegex* DaoRegex_New( DString *src );
int DaoRegex_Match( DaoRegex *self, DString *src, size_t *start, size_t *end );
int DaoRegex_Change( DaoRegex *self, DString *src, DString *target, 
    int index, size_t *start, size_t *end );
int DaoRegex_Extract( DaoRegex *self, DString *s, DVarray *ls, short tp );

/* =======================================================================================
 * Numeric Array:
 * =======================================================================================*/
DaoArray* DaoArray_New( int type );
DaoArray* DaoArray_Copy( DaoArray *self );
void DaoArray_Delete( DaoArray *self );

void DaoArray_SetNumType( DaoArray *self, short numtype );
int DaoArray_NumType( DaoArray *self );
int DaoArray_Size( DaoArray *self );
int DaoArray_DimCount( DaoArray *self );
int DaoArray_SizeOfDim( DaoArray *self, int d );
void DaoArray_GetShape( DaoArray *self, size_t *dims );
int DaoArray_HasShape( DaoArray *self, size_t *dims, int D );
int DaoArray_Reshape( DaoArray *self, size_t *dims, int D );
int  DaoArray_GetFlatIndex( DaoArray *self, size_t *index );
void DaoArray_ResizeVector( DaoArray *self, int N );
void DaoArray_ResizeArray( DaoArray *self, size_t *dims, int D );

/* =======================================================================================
 * Running time context for Dao routine:
 * =======================================================================================*/
/* Create a new context, must be initialized by DaoContext_Init() */
DaoContext* DaoContext_New();
void DaoContext_Delete( DaoContext *self );

/* Intialize a context according to routine */
void DaoContext_Init( DaoContext *self, DaoRoutine *routine );
/* Intialize a context according to overloaded routine with
 * parameter types matching to "pars" */
/* Must be called after DaoContext_Init() */
int DaoContext_InitWithParams( DaoContext *self, DaoVmProcess *vmp, DValue *pars[], int npar );

/* Put result at the result register for current instruction */
void DaoContext_SetResult( DaoContext *self, DaoBase *result );

DaoBase* DaoContext_CreateResult( DaoContext *self, short type );

/* Put a number at the result register, return the number object */
dint*      DaoContext_PutInteger( DaoContext *self, dint value );
float*     DaoContext_PutFloat( DaoContext *self, float value );
double*    DaoContext_PutDouble( DaoContext *self, double value );
complex16* DaoContext_PutComplex8( DaoContext *self, complex8 value );
complex16* DaoContext_PutComplex16( DaoContext *self, complex16 value );
DString*   DaoContext_PutMBString( DaoContext *self, const char *mbs );
DString*   DaoContext_PutWCString( DaoContext *self, const wchar_t *wcs );
DString*   DaoContext_PutBytes( DaoContext *self, const char *bytes, int N );
DaoArray*  DaoContext_PutArrayInteger( DaoContext *self, int *array, int N );
DaoArray*  DaoContext_PutArrayShort( DaoContext *self, short *array, int N );
DaoArray*  DaoContext_PutArrayFloat( DaoContext *self, float *array, int N );
DaoArray*  DaoContext_PutArrayDouble( DaoContext *self, double *array, int N );
DaoArray*  DaoContext_PutArrayComplex8( DaoContext *self, complex8 *array, int N );
DaoArray*  DaoContext_PutArrayComplex16( DaoContext *self, complex16 *array, int N );
DaoList*   DaoContext_PutList( DaoContext *self );
DaoMap*    DaoContext_PutMap( DaoContext *self );
DaoArray*  DaoContext_PutArray( DaoContext *self );
DaoCData*  DaoContext_PutCData( DaoContext *self, void *data, DaoTypeBase *plgTyper );
DaoCData*  DaoContext_PutCPointer( DaoContext *self, void *data, int size );

/**/
void DaoContext_Print( DaoContext *self, const char *chs );
void DaoContext_RaiseException( DaoContext *self, int type, const char *value );

/* =======================================================================================
 * Dao virtual machine proecess:
 * =======================================================================================*/
/* Create a new virtual machine process */
DaoVmProcess* DaoVmProcess_New( DaoVmSpace *vms );
void DaoVmProcess_Delete( DaoVmProcess *self );

/* Push a routine into the calling stack of the VM process, new context is created */
void DaoVmProcess_PushRoutine( DaoVmProcess *self, DaoRoutine *routine );
/* Push an initialized context into the calling stack of the VM process */
void DaoVmProcess_PushContext( DaoVmProcess *self, DaoContext *context );

/* Execute from the top of the calling stack */
int DaoVmProcess_Execute( DaoVmProcess *self );
/* Push a context onto the calling stack, and executing this context only */
int DaoVmProcess_ExecuteCtx( DaoVmProcess *self, DaoContext *context );

/* Resume a coroutine */
/* coroutine.yeild( a, b, ... ); store object a,b,... in "DaoList *result"
 * 
 * param = coroutine.resume( corout, a, b, ... ); pass "DaoList *par" as a,b,...
 * they become addition result from yeild().
 */
int DaoVmProcess_Resume( DaoVmProcess *self, DValue *par[], int N, DaoList *list );
void DaoVmProcess_Yield( DaoVmProcess *self, DValue *par[], int N, DaoList *list );

/* =======================================================================================
 * Dao virtual machine space:
 * =======================================================================================*/
DaoVmSpace* DaoVmSpace_New();
/* DaoVmSpace is not handled by GC, it should be deleted manually. 
 * Normally, DaoVmSpace structures are allocated in the beginning of a program and 
 * persist until the program exits. So DaoVmSpace_Delete() is rarely needed to be called.
 */
void DaoVmSpace_Delete( DaoVmSpace *self );

int DaoVmSpace_ParseOptions( DaoVmSpace *self, DString *options );
void   DaoVmSpace_SetOptions( DaoVmSpace *self, int options );
int    DaoVmSpace_GetOptions( DaoVmSpace *self );

int DaoVmSpace_Compile( DaoVmSpace *self, DaoNameSpace *ns, DString *src, int rpl );
DaoNameSpace* DaoVmSpace_Load( DaoVmSpace *self, DString *file );

DaoNameSpace* DaoVmSpace_LoadModule( DaoVmSpace *self, DString *fname, DArray *reqns );
DaoNameSpace* DaoVmSpace_LoadDaoModule( DaoVmSpace *self, DString *fname );
DaoNameSpace* DaoVmSpace_LoadDllModule( DaoVmSpace *self, DString *fname, DArray *reqns );

void DaoVmSpace_MakePath( DaoVmSpace *self, DString *fname, int check );
void DaoVmSpace_SetPath( DaoVmSpace *self, const char *path );
void DaoVmSpace_AddPath( DaoVmSpace *self, const char *path );
void DaoVmSpace_DelPath( DaoVmSpace *self, const char *path );
DaoTypeBase* DaoVmSpace_GetTyper( short type );

void DaoSched_Init( DaoVmSpace *vms );
void DaoSched_Send( DVarray *msg, int mode, DaoVmProcess *sender, DaoObject *future );
void DaoSched_Join();

#ifdef __cplusplus
}
#endif

#endif
