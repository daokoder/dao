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

#ifndef _DAO_H
#define _DAO_H

#include"wctype.h"
#include"wchar.h"
#include"stdio.h"
#include"stdlib.h"

#define DAO_H_VERSION 20101028

/* define an integer type with size equal to the size of pointers
 * under both 32-bits and 64-bits systems. */

#if defined(MAC_OSX) && ! defined(UNIX)
#define UNIX
#endif /* MAC_OSX */

#ifdef WIN32

#if defined( _MSC_VER ) && defined( _M_X64 ) || defined( __x86_64__ )
typedef long long           dint;
typedef unsigned long long  uint_t;
#else
typedef long                dint;
typedef unsigned long       uint_t;
#endif /* defined() */

/* Get rid of the effects of UNICODE: */
#ifdef UNICODE
#undef UNICODE
#endif /* UNICODE */

#include<windows.h>

#define DAO_DLL __declspec(dllexport)

#define DAO_DLL_EXPORT __declspec(dllexport)
#define DAO_DLL_IMPORT __declspec(dllimport)

#define DAO_DLL_SUFFIX ".dll"
#define DAO_LIB_DEFAULT "C:\\dao\\dao.dll"

#define DaoLoadLibrary( name ) LoadLibrary( name )
#define DaoFindSymbol( handle, name ) GetProcAddress( (HMODULE)handle, name )

#elif defined(UNIX) /* UNIX */

#include<dlfcn.h>
#define DAO_DLL

#define DAO_DLL_EXPORT
#define DAO_DLL_IMPORT

#define DaoLoadLibrary( name ) dlopen( name, RTLD_NOW | RTLD_GLOBAL )
#define DaoFindSymbol( handle, name ) dlsym( handle, name )
#define DaoCloseLibrary( handle ) dlclose( handle )

typedef long          dint;
typedef unsigned long uint_t;

#ifdef MAC_OSX

#define DAO_DLL_SUFFIX ".dylib"
#define DAO_LIB_DEFAULT "/usr/local/dao/dao.dylib"

#else /* UNIX */

#define DAO_DLL_SUFFIX ".so"
#define DAO_LIB_DEFAULT "/usr/local/dao/dao.so"

#endif /* MAC_OSX */

#else /* other system */

#define DAO_DLL
#define DAO_DLL_EXPORT
#define DAO_DLL_IMPORT

typedef long          dint;
typedef unsigned long uint_t;

#endif /* WIN32 */

typedef struct DaoAPI    DaoAPI;

#ifndef DAO_DIRECT_API
#define DAO_API_PROXY DAO_DLL DaoAPI __dao;
#else
#define DAO_API_PROXY
#endif

#ifdef __cplusplus
#define DAO_EXTC_OPEN extern "C"{
#define DAO_EXTC_CLOSE }
#else
#define DAO_EXTC_OPEN
#define DAO_EXTC_CLOSE
#endif

#ifndef DAO_DIRECT_API
DAO_EXTC_OPEN
extern DAO_DLL DaoAPI __dao;
DAO_EXTC_CLOSE
#endif

#define \
    DAO_INIT_MODULE \
DAO_EXTC_OPEN \
DAO_DLL int DaoH_Version = DAO_H_VERSION; \
DAO_DLL int DaoOnLoad( DaoVmSpace *vmSpace, DaoNameSpace *ns ); \
DAO_API_PROXY; \
DAO_EXTC_CLOSE


#define DAO_MAX_CDATA_SUPER 10


#ifdef __cplusplus
extern "C"{
#endif

extern int daoProxyPort;

enum DaoTypes
{
	DAO_NIL     = 0,
	DAO_INTEGER ,
	DAO_FLOAT   ,
	DAO_DOUBLE  ,
	DAO_COMPLEX ,
	DAO_LONG  ,
	DAO_STRING ,
	DAO_ENUM  ,
	DAO_ARRAY ,
	DAO_LIST  ,
	DAO_MAP   ,
	DAO_PAIR  ,
	DAO_TUPLE ,
	DAO_STREAM ,
	DAO_OBJECT ,
	DAO_CDATA  ,
	DAO_REGEX     ,
	DAO_INTERFACE ,
	DAO_CLASS     ,
	DAO_FUNCTION  ,
	DAO_ROUTINE   ,
	DAO_CONTEXT   ,
	DAO_VMPROCESS ,
	DAO_NAMESPACE ,
	DAO_VMSPACE   ,
	DAO_MUTEX ,
	DAO_CONDVAR ,
	DAO_SEMA ,
	DAO_THREAD ,
	END_CORE_TYPES
};
enum DaoVmProcessStatus
{
	DAO_VMPROC_FINISHED ,  /* finished normally */
	DAO_VMPROC_ABORTED ,
	DAO_VMPROC_SUSPENDED , /* suspended, by coroutine.yield() */
	DAO_VMPROC_RUNNING ,
	DAO_VMPROC_STACKED     /* new context is pushed onto the stack of the process */
};
enum DaoNameSpaceOption
{
	/* automatically make variable declared outside {} global, for interactive mode */
	DAO_NS_AUTO_GLOBAL = (1<<0)
};
/* Execution options, combinatable by | */
enum DaoExecOption
{
	DAO_EXEC_HELP      = (1<<0), /* -h, --help:       print this help information; */
	DAO_EXEC_VINFO     = (1<<1), /* -v, --version:    print version information; */
	DAO_EXEC_DEBUG     = (1<<2), /* -d, --debug:      run in debug mode; */
	DAO_EXEC_SAFE      = (1<<3), /* -s, --safe:       run in safe mode; */
	DAO_EXEC_INTERUN   = (1<<4), /* -i, --ineractive: run in interactive mode; */
	DAO_EXEC_LIST_BC   = (1<<5), /* -l, --list-bc:    print compiled bytecodes; */
	DAO_EXEC_COMP_BC   = (1<<6), /* -c, --compile:    compile to bytecodes;(TODO) */
	DAO_EXEC_INCR_COMP = (1<<7), /* -n, --incr-comp:  incremental compiling; */
	DAO_EXEC_NO_JIT    = (1<<8), /* -J, --no-jit:     no JIT compiling; */
	DAO_EXEC_NO_TC     = (1<<9), /* -T, --no-typed-code:   no typed code; */

	DAO_EXEC_MBS_ONLY = (1<<20),
	DAO_EXEC_WCS_ONLY = (1<<21),

	/* -- disable JIT;
	 * -- disable function specialization;
	 * -- insert NOP codes for conveniently setting up break points; */
	DAO_EXEC_IDE = (1<<31)
};
enum DaoExceptionType
{
	DAO_EXCEPTION = 0,
	DAO_EXCEPTION_NONE ,
	DAO_EXCEPTION_ANY ,
	DAO_WARNING ,
	DAO_ERROR ,

	DAO_ERROR_FIELD ,
	DAO_ERROR_FIELD_NOTEXIST ,
	DAO_ERROR_FIELD_NOTPERMIT ,
	DAO_ERROR_FLOAT ,
	DAO_ERROR_FLOAT_DIVBYZERO ,
	DAO_ERROR_FLOAT_OVERFLOW ,
	DAO_ERROR_FLOAT_UNDERFLOW ,
	DAO_ERROR_INDEX ,
	DAO_ERROR_INDEX_OUTOFRANGE ,
	DAO_ERROR_KEY ,
	DAO_ERROR_KEY_NOTEXIST ,
	DAO_ERROR_PARAM ,
	DAO_ERROR_SYNTAX ,
	DAO_ERROR_TYPE ,
	DAO_ERROR_VALUE ,

	DAO_WARNING_SYNTAX ,
	DAO_WARNING_VALUE ,

	ENDOF_BASIC_EXCEPT
};

typedef unsigned char uchar_t;

typedef struct DValue      DValue;
typedef struct DString     DString;
typedef struct DArray      DArray;
typedef struct DLong       DLong;
typedef struct DEnum       DEnum;
typedef struct DNode       DNode;
typedef struct DMap        DMap;

typedef struct DaoTypeCore     DaoTypeCore;
typedef struct DaoTypeBase     DaoTypeBase;
typedef struct DaoUserHandler  DaoUserHandler;
typedef struct DaoCallbackData DaoCallbackData;

typedef struct DaoBase         DaoBase;
typedef struct DaoArray        DaoArray;
typedef struct DaoList         DaoList;
typedef struct DaoMap          DaoMap;
typedef struct DaoPair         DaoPair;
typedef struct DaoTuple        DaoTuple;
typedef struct DaoRoutine      DaoRoutine;
typedef struct DaoFunction     DaoFunction;
typedef struct DaoInterface    DaoInterface;
typedef struct DaoClass        DaoClass;
typedef struct DaoObject       DaoObject;
typedef struct DaoStream       DaoStream;
typedef struct DaoCData        DaoCData;
typedef struct DaoRegex        DaoRegex;
typedef struct DaoNameSpace    DaoNameSpace;
typedef struct DaoVmSpace      DaoVmSpace;
typedef struct DaoContext      DaoContext;
typedef struct DaoVmProcess    DaoVmProcess;
typedef struct DaoMutex        DaoMutex;
typedef struct DaoCondVar      DaoCondVar;
typedef struct DaoSema         DaoSema;
typedef struct DaoThread       DaoThread;
typedef struct DaoType         DaoType;

/* Complex type: */
typedef struct complex8  { float  real, imag; } complex8;
typedef struct complex16 { double real, imag; } complex16;

/* Structure for symbol, enum and flag:
 * Storage modes:
 * Symbol: $AA => { type<$AA>, 0 }
 * Symbols: $AA + $BB => { type<$AA$BB>, 1|2 }
 * Enum: MyEnum{ AA=1, BB=2 }, MyEnum.AA => { type<MyEnum>, 1 }
 * Flag: MyFlag{ AA=1, BB=2 }, MyFlag.AA + MyFlag.BB => { type<MyFlag>, 1|2 }
 */
struct DEnum
{
	DaoType  *type;
	dint      value;
};


struct DValue
{
	uchar_t  t; /* type */
	uchar_t  sub; /* sub-type */
	uchar_t  cst; /* const */
	uchar_t  ndef; /* not a default parameter */
	union {
		dint           i; /* int */
		float          f; /* float */
		double         d; /* double */
		complex16     *c; /* complex */
		DLong         *l; /* long */
		DString       *s; /* string */
		DEnum         *e; /* enum */
		DaoBase       *p; /* NOT one of the above data types */
		DaoArray      *array;
		DaoList       *list;
		DaoMap        *map;
		DaoPair       *pair;
		DaoTuple      *tuple;
		DaoRoutine    *routine;
		DaoFunction   *func;
		DaoObject     *object;
		DaoClass      *klass;
		DaoCData      *cdata;
		DaoContext    *context;
		DaoStream     *stream;
		DaoInterface  *inter;
		DaoNameSpace  *ns;
		DaoVmProcess  *vmp;
	} v ;
};

typedef void (*CallbackOnString)( const char *str );
typedef void (*FuncInitAPI)( DaoAPI *api );
typedef void  (*DThreadTask)( void *arg );
typedef void  (*FuncPtrDel)( void* );
typedef int   (*FuncPtrTest)( void* );
typedef void  (*DaoFuncPtr) ( DaoContext *context, DValue *params[], int npar );

typedef struct DaoNumItem   DaoNumItem;
typedef struct DaoFuncItem  DaoFuncItem;

struct DaoNumItem
{
	const char *name;
	int         type;
	double      value;
};
struct DaoFuncItem
{
	DaoFuncPtr  fpter;    /* C function pointer; */
	const char *proto;    /* function prototype: name( parlist ) => return_type */
};

/* Typer structure, contains type information of each Dao type: */
struct DaoTypeBase
{
	const char    *name; /* type name; */
	DaoTypeCore   *priv; /* data used internally; */
	DaoNumItem    *numItems; /* constant number list */
	DaoFuncItem   *funcItems; /* method list: should end with a null item */

	/* typers for super types, to create c type hierarchy;
	 * mainly useful for wrapping c++ libraries. */
	DaoTypeBase   *supers[ DAO_MAX_CDATA_SUPER ];

	/* function to free data:
	 * only for DaoCData created by DValue_NewCData() or DaoCData_New() */
	void   (*Delete)( void *data );
	/* test if the data is deletable by Dao: called by gc before deletion. */
	int    (*DelTest)( void *data );
};

/* Callback data: freed when "callback" or "userdata" is collected by GC. */
struct DaoCallbackData
{
	DaoRoutine  *callback;
	DValue       userdata;
};

/* This structure can be passed to DaoVmSpace by DaoVmSpace_SetUserHandler(),
 * to change the handling of standard input/output, debugging and profiling
 * behaviour. */
struct DaoUserHandler
{
	/* count>0: read count bytes; count=0: one line; count<0: until EOF */
	void (*StdioRead)( DaoUserHandler *self, DString *input, int count );
	void (*StdioWrite)( DaoUserHandler *self, DString *output );
	void (*StdioFlush)( DaoUserHandler *self );
	void (*StdlibDebug)( DaoUserHandler *self, DaoContext *context );
	/* properly change some NOP codes to DEBUG codes */
	void (*BreakPoints)( DaoUserHandler *self, DaoRoutine *routine );
	/* profiling hooks, for future use */
	void (*Called)( DaoUserHandler *self, DaoRoutine *caller, DaoRoutine *callee );
	void (*Returned)( DaoUserHandler *self, DaoRoutine *caller, DaoRoutine *callee );
	/* invoke host execution to do whatever (e.g., to process GUI events) */
	void (*InvokeHost)( DaoUserHandler *self, DaoContext *context );
};
typedef char* (*ReadLine)( const char *prompt );
typedef void  (*AddHistory)( const char *cmd );


/* API structure for indirect interfaces */
/* See the DAO_DIRECT_API part for descriptions of the functions */
struct DaoAPI
{
	DaoVmSpace* (*DaoInit)();
	void        (*DaoQuit)();

	DValue (*DValue_NewInteger)( dint v );
	DValue (*DValue_NewFloat)( float v );
	DValue (*DValue_NewDouble)( double v );
	DValue (*DValue_NewMBString)( const char *s, int n );
	DValue (*DValue_NewWCString)( const wchar_t *s, int n );
	DValue (*DValue_NewVectorB)( char *s, int n );
	DValue (*DValue_NewVectorUB)( unsigned char *s, int n );
	DValue (*DValue_NewVectorS)( short *s, int n );
	DValue (*DValue_NewVectorUS)( unsigned short *s, int n );
	DValue (*DValue_NewVectorI)( int *s, int n );
	DValue (*DValue_NewVectorUI)( unsigned int *s, int n );
	DValue (*DValue_NewVectorF)( float *s, int n );
	DValue (*DValue_NewVectorD)( double *s, int n );
	DValue (*DValue_NewMatrixB)( signed char **s, int n, int m );
	DValue (*DValue_NewMatrixUB)( unsigned char **s, int n, int m );
	DValue (*DValue_NewMatrixS)( short **s, int n, int m );
	DValue (*DValue_NewMatrixUS)( unsigned short **s, int n, int m );
	DValue (*DValue_NewMatrixI)( int **s, int n, int m );
	DValue (*DValue_NewMatrixUI)( unsigned int **s, int n, int m );
	DValue (*DValue_NewMatrixF)( float **s, int n, int m );
	DValue (*DValue_NewMatrixD)( double **s, int n, int m );
	DValue (*DValue_NewBuffer)( void *s, int n );
	DValue (*DValue_NewStream)( FILE *f );
	DValue (*DValue_NewCData)( DaoTypeBase *typer, void *data );
	DValue (*DValue_WrapCData)( DaoTypeBase *typer, void *data );
	void (*DValue_Copy)( DValue *self, DValue from );
	void (*DValue_Clear)( DValue *v );
	void (*DValue_ClearAll)( DValue *v, int n );

	DString* (*DString_New)( int mbs );
	void (*DString_Delete)( DString *self );

	size_t (*DString_Size)( DString *self );
	void (*DString_Clear)( DString *self );
	void (*DString_Resize)( DString *self, size_t size );

	int (*DString_IsMBS)( DString *self );
	void (*DString_SetMBS)( DString *self, const char *chs );
	void (*DString_SetWCS)( DString *self, const wchar_t *chs );
	void (*DString_SetDataMBS)( DString *self, const char *data, size_t n );
	void (*DString_SetDataWCS)( DString *self, const wchar_t *data, size_t n );
	void (*DString_ToWCS)( DString *self );
	void (*DString_ToMBS)( DString *self );
	char* (*DString_GetMBS)( DString *self );
	wchar_t* (*DString_GetWCS)( DString *self );

	void (*DString_Erase)( DString *self, size_t start, size_t n );
	void (*DString_Insert)( DString *self, DString *s, size_t i, size_t m, size_t n );
	void (*DString_InsertMBS)( DString *self, const char *s, size_t i, size_t m, size_t n );
	void (*DString_InsertChar)( DString *self, const char ch, size_t at );
	void (*DString_InsertWCS)( DString *self, const wchar_t *s, size_t i, size_t m, size_t n );
	void (*DString_Append)( DString *self, DString *chs );
	void (*DString_AppendChar)( DString *self, const char ch );
	void (*DString_AppendWChar)( DString *self, const wchar_t ch );
	void (*DString_AppendMBS)( DString *self, const char *chs );
	void (*DString_AppendWCS)( DString *self, const wchar_t *chs );
	void (*DString_AppendDataMBS)( DString *self, const char *data, size_t n );
	void (*DString_AppendDataWCS)( DString *self, const wchar_t *data,size_t n );

	void (*DString_SubString)( DString *self, DString *sub, size_t from, size_t n );

	size_t (*DString_Find)( DString *self, DString *chs, size_t start );
	size_t (*DString_RFind)( DString *self, DString *chs, size_t start );
	size_t (*DString_FindMBS)( DString *self, const char *ch, size_t start );
	size_t (*DString_RFindMBS)( DString *self, const char *ch, size_t start );
	size_t (*DString_FindChar)( DString *self, char ch, size_t start );
	size_t (*DString_FindWChar)( DString *self, wchar_t ch, size_t start );
	size_t (*DString_RFindChar)( DString *self, char ch, size_t start );

	DString* (*DString_Copy)( DString *self );
	void (*DString_Assign)( DString *left, DString *right );
	int  (*DString_Compare)( DString *left, DString *right );

	DaoList* (*DaoList_New)();
	int    (*DaoList_Size)( DaoList *self );
	DValue (*DaoList_Front)( DaoList *self );
	DValue (*DaoList_Back)( DaoList *self );
	DValue (*DaoList_GetItem)( DaoList *self, int pos );

	void (*DaoList_SetItem)( DaoList *self, DValue item, int pos );
	void (*DaoList_Insert) ( DaoList *self, DValue item, int pos );
	void (*DaoList_Erase)( DaoList *self, int pos );
	void (*DaoList_Clear)( DaoList *self );
	void (*DaoList_PushFront)( DaoList *self, DValue item );
	void (*DaoList_PushBack) ( DaoList *self, DValue item );
	void (*DaoList_PopFront)( DaoList *self );
	void (*DaoList_PopBack) ( DaoList *self );

	DaoMap* (*DaoMap_New)( int hashing );
	int  (*DaoMap_Size)( DaoMap *self );
	int  (*DaoMap_Insert)( DaoMap *self, DValue key, DValue value );
	void (*DaoMap_Erase) ( DaoMap *self, DValue key );
	void (*DaoMap_Clear) ( DaoMap *self );
	void (*DaoMap_InsertMBS)( DaoMap *self, const char *key, DValue value );
	void (*DaoMap_InsertWCS)( DaoMap *self, const wchar_t *key, DValue value );
	void (*DaoMap_EraseMBS) ( DaoMap *self, const char *key );
	void (*DaoMap_EraseWCS) ( DaoMap *self, const wchar_t *key );
	DValue (*DaoMap_GetValue)( DaoMap *self, DValue key  );
	DValue (*DaoMap_GetValueMBS)( DaoMap *self, const char *key  );
	DValue (*DaoMap_GetValueWCS)( DaoMap *self, const wchar_t *key  );
	DNode* (*DaoMap_First)( DaoMap *self );
	DNode* (*DaoMap_Next)( DaoMap *self, DNode *iter );
	DValue* (*DNode_Key)( DNode *self );
	DValue* (*DNode_Value)( DNode *self );

	DaoTuple* (*DaoTuple_New)( int size );
	int    (*DaoTuple_Size)( DaoTuple *self );
	void   (*DaoTuple_SetItem)( DaoTuple *self, DValue it, int pos );
	DValue (*DaoTuple_GetItem)( DaoTuple *self, int pos );

	int  (*DaoArray_NumType)( DaoArray *self );
	void (*DaoArray_SetNumType)( DaoArray *self, short numtype );

	int  (*DaoArray_Size)( DaoArray *self );
	int  (*DaoArray_DimCount)( DaoArray *self );
	int  (*DaoArray_SizeOfDim)( DaoArray *self, int d );
	void (*DaoArray_GetShape)( DaoArray *self, size_t *dims );
	int  (*DaoArray_HasShape)( DaoArray *self, size_t *dims, int D );
	int  (*DaoArray_GetFlatIndex)( DaoArray *self, size_t *index );

	void (*DaoArray_ResizeVector)( DaoArray *self, int N );
	void (*DaoArray_ResizeArray)( DaoArray *self, size_t *dims, int D );
	int  (*DaoArray_Reshape)( DaoArray *self, size_t *dims, int D );

	signed char* (*DaoArray_ToByte)( DaoArray *self );
	short*       (*DaoArray_ToShort)( DaoArray *self );
	int*         (*DaoArray_ToInt)( DaoArray *self );
	float*       (*DaoArray_ToFloat)( DaoArray *self );
	double*      (*DaoArray_ToDouble)( DaoArray *self );
	unsigned char*  (*DaoArray_ToUByte)( DaoArray *self );
	unsigned short* (*DaoArray_ToUShort)( DaoArray *self );
	unsigned int*   (*DaoArray_ToUInt)( DaoArray *self );

	signed char** (*DaoArray_GetMatrixB)( DaoArray *self, int row );
	short**  (*DaoArray_GetMatrixS)( DaoArray *self, int row );
	int**    (*DaoArray_GetMatrixI)( DaoArray *self, int row );
	float**  (*DaoArray_GetMatrixF)( DaoArray *self, int row );
	double** (*DaoArray_GetMatrixD)( DaoArray *self, int row );

	void (*DaoArray_FromByte)( DaoArray *self );
	void (*DaoArray_FromShort)( DaoArray *self );
	void (*DaoArray_FromInt)( DaoArray *self );
	void (*DaoArray_FromFloat)( DaoArray *self );
	void (*DaoArray_FromDouble)( DaoArray *self );
	void (*DaoArray_FromUByte)( DaoArray *self );
	void (*DaoArray_FromUShort)( DaoArray *self );
	void (*DaoArray_FromUInt)( DaoArray *self );

	void (*DaoArray_SetVectorB)( DaoArray *self, char* vec, int N );
	void (*DaoArray_SetVectorS)( DaoArray *self, short* vec, int N );
	void (*DaoArray_SetVectorI)( DaoArray *self, int* vec, int N );
	void (*DaoArray_SetVectorF)( DaoArray *self, float* vec, int N );
	void (*DaoArray_SetVectorD)( DaoArray *self, double* vec, int N );

	void (*DaoArray_SetMatrixB)( DaoArray *self, signed char **mat, int row, int col );
	void (*DaoArray_SetMatrixS)( DaoArray *self, short **mat, int row, int col );
	void (*DaoArray_SetMatrixI)( DaoArray *self, int **mat, int row, int col );
	void (*DaoArray_SetMatrixF)( DaoArray *self, float **mat, int row, int col );
	void (*DaoArray_SetMatrixD)( DaoArray *self, double **mat, int row, int col );

	void (*DaoArray_SetVectorUB)( DaoArray *self, unsigned char* vec, int N );
	void (*DaoArray_SetVectorUS)( DaoArray *self, unsigned short* vec, int N );
	void (*DaoArray_SetVectorUI)( DaoArray *self, unsigned int* vec, int N );

	void* (*DaoArray_GetBuffer)( DaoArray *self );
	void  (*DaoArray_SetBuffer)( DaoArray *self, void *buffer, size_t size );

	DValue (*DaoObject_GetField)( DaoObject *self, const char *name );
	DaoCData* (*DaoObject_MapCData)( DaoObject *self, DaoTypeBase *typer );
	/* TODO: DaoObject* DaoObject_MapObject( DaoObject *self, DaoClass *klass ) */

	DaoStream* (*DaoStream_New)();
	void  (*DaoStream_SetFile)( DaoStream *self, FILE *fd );
	FILE* (*DaoStream_GetFile)( DaoStream *self );

	int (*DaoFunction_Call)( DaoFunction *func, DaoCData *self, DValue *p[], int n );

	DaoCData* (*DaoCData_New)( DaoTypeBase *typer, void *data );
	DaoCData* (*DaoCData_Wrap)( DaoTypeBase *typer, void *data );
	int    (*DaoCData_IsType)( DaoCData *self, DaoTypeBase *typer );
	void   (*DaoCData_SetExtReference)( DaoCData *self, int bl );
	void   (*DaoCData_SetData)( DaoCData *self, void *data );
	void   (*DaoCData_SetBuffer)( DaoCData *self, void *data, size_t size );
	void   (*DaoCData_SetArray) ( DaoCData *self, void *data, size_t size, int memsize );
	void*  (*DaoCData_GetData)( DaoCData *self );
	void*  (*DaoCData_GetBuffer)( DaoCData *self );
	void** (*DaoCData_GetData2)( DaoCData *self );
	DaoObject* (*DaoCData_GetObject)( DaoCData *self );
	DaoTypeBase* (*DaoCData_GetTyper)( DaoCData *self );

	DaoMutex* (*DaoMutex_New)( DaoVmSpace *vms );
	void (*DaoMutex_Lock)( DaoMutex *self );
	void (*DaoMutex_Unlock)( DaoMutex *self );
	int  (*DaoMutex_TryLock)( DaoMutex *self );

	dint*      (*DaoContext_PutInteger)( DaoContext *self, dint value );
	float*     (*DaoContext_PutFloat)( DaoContext *self, float value );
	double*    (*DaoContext_PutDouble)( DaoContext *self, double value );
	complex16* (*DaoContext_PutComplex)( DaoContext *self, complex16 value );
	DString*   (*DaoContext_PutMBString)( DaoContext *self, const char *mbs );
	DString*   (*DaoContext_PutWCString)( DaoContext *self, const wchar_t *wcs );
	DString*   (*DaoContext_PutString)( DaoContext *self, DString *str );
	DString*   (*DaoContext_PutBytes)( DaoContext *self, const char *bytes, int N );
	DEnum*     (*DaoContext_PutEnum)( DaoContext *self, const char *symbols );
	DaoArray*  (*DaoContext_PutArrayInteger)( DaoContext *self, int *array, int N );
	DaoArray*  (*DaoContext_PutArrayShort)( DaoContext *self, short *array, int N );
	DaoArray*  (*DaoContext_PutArrayFloat)( DaoContext *self, float *array, int N );
	DaoArray*  (*DaoContext_PutArrayDouble)( DaoContext *self, double *array, int N );
	DaoArray*  (*DaoContext_PutArrayComplex)( DaoContext *self, complex16 *array, int N );
	DaoList*   (*DaoContext_PutList)( DaoContext *self );
	DaoMap*    (*DaoContext_PutMap)( DaoContext *self );
	DaoArray*  (*DaoContext_PutArray)( DaoContext *self );
	DaoStream* (*DaoContext_PutFile)( DaoContext *self, FILE *file );
	DaoCData*  (*DaoContext_PutCData)( DaoContext *self, void *data, DaoTypeBase *t );
	DaoCData*  (*DaoContext_PutCPointer)( DaoContext *self, void *data, int size );
	DaoBase*   (*DaoContext_PutResult)( DaoContext *self, DaoBase *data );
	DValue* (*DaoContext_PutValue)( DaoContext *self, DValue value );

	DaoCData*  (*DaoContext_WrapCData)( DaoContext *self, void *data, DaoTypeBase *t );
	DaoCData* (*DaoContext_CopyCData)( DaoContext *self, void *d, int n, DaoTypeBase *t );

	void (*DaoContext_RaiseException)( DaoContext *self, int type, const char *value );

	DaoVmProcess* (*DaoVmProcess_New)( DaoVmSpace *vms );
	int (*DaoVmProcess_Compile)( DaoVmProcess *self, DaoNameSpace *ns, DString *src, int rpl );
	int (*DaoVmProcess_Eval)( DaoVmProcess *self, DaoNameSpace *ns, DString *src, int rpl );
	int (*DaoVmProcess_Call)( DaoVmProcess *self, DaoRoutine *r, DaoObject *o, DValue *p[], int n);
	void (*DaoVmProcess_Stop)( DaoVmProcess *self );
	DValue (*DaoVmProcess_GetReturned)( DaoVmProcess *self );

	DaoNameSpace* (*DaoNameSpace_New)( DaoVmSpace *vms );
	DaoNameSpace* (*DaoNameSpace_GetNameSpace)( DaoNameSpace *self, const char *name );
	void (*DaoNameSpace_AddParent)( DaoNameSpace *self, DaoNameSpace *parent );
	void (*DaoNameSpace_AddConstNumbers)( DaoNameSpace *self, DaoNumItem *items );
	void (*DaoNameSpace_AddConstValue)( DaoNameSpace*self, const char *s, DValue v );
	void (*DaoNameSpace_AddConstData)( DaoNameSpace *self, const char *name, DaoBase *data );
	void (*DaoNameSpace_AddData)( DaoNameSpace *self, const char *s, DaoBase *d, const char *t);
	void (*DaoNameSpace_AddValue)( DaoNameSpace *self, const char *s, DValue v, const char *t);
	DValue (*DaoNameSpace_FindData)( DaoNameSpace *self, const char *name );

	int (*DaoNameSpace_TypeDefine)( DaoNameSpace *self, const char *old, const char *type );
	int (*DaoNameSpace_TypeDefines)( DaoNameSpace *self, const char *alias[] );
	int (*DaoNameSpace_WrapType)( DaoNameSpace *self, DaoTypeBase *typer );
	int (*DaoNameSpace_WrapTypes)( DaoNameSpace *self, DaoTypeBase *typer[] );
	int (*DaoNameSpace_WrapFunction)( DaoNameSpace *self, DaoFuncPtr fp, const char *proto );
	int (*DaoNameSpace_WrapFunctions)( DaoNameSpace *self, DaoFuncItem *items );
	int (*DaoNameSpace_SetupType)( DaoNameSpace *self, DaoTypeBase *typer );
	int (*DaoNameSpace_SetupTypes)( DaoNameSpace *self, DaoTypeBase *typer[] );
	int (*DaoNameSpace_Load)( DaoNameSpace *self, const char *file );
	int (*DaoNameSpace_GetOptions)( DaoNameSpace *self );
	void (*DaoNameSpace_SetOptions)( DaoNameSpace *self, int options );

	DaoVmSpace* (*DaoVmSpace_New)();

	int (*DaoVmSpace_ParseOptions)( DaoVmSpace *self, DString *options );
	void (*DaoVmSpace_SetOptions)( DaoVmSpace *self, int options );
	int  (*DaoVmSpace_GetOptions)( DaoVmSpace *self );

	int (*DaoVmSpace_RunMain)( DaoVmSpace *self, DString *file );
	DaoNameSpace* (*DaoVmSpace_Load)( DaoVmSpace *self, DString *file );
	DaoNameSpace* (*DaoVmSpace_GetNameSpace)( DaoVmSpace *self, const char *name );
	DaoNameSpace* (*DaoVmSpace_MainNameSpace)( DaoVmSpace *self );
	DaoVmProcess* (*DaoVmSpace_MainVmProcess)( DaoVmSpace *self );
	DaoVmProcess* (*DaoVmSpace_AcquireProcess)( DaoVmSpace *self );
	void (*DaoVmSpace_ReleaseProcess)( DaoVmSpace *self, DaoVmProcess *proc );

	void (*DaoVmSpace_SetUserHandler)( DaoVmSpace *self, DaoUserHandler *handler );
	void (*DaoVmSpace_ReadLine)( DaoVmSpace *self, ReadLine fptr );
	void (*DaoVmSpace_AddHistory)( DaoVmSpace *self, AddHistory fptr );

	void (*DaoVmSpace_AddVirtualFile)( DaoVmSpace *self, const char *f, const char *s );
	void (*DaoVmSpace_SetPath)( DaoVmSpace *self, const char *path );
	void (*DaoVmSpace_AddPath)( DaoVmSpace *self, const char *path );
	void (*DaoVmSpace_DelPath)( DaoVmSpace *self, const char *path );

	void  (*DaoVmSpace_Stop)( DaoVmSpace *self, int bl );

	void (*DaoGC_IncRC)( DaoBase *p );
	void (*DaoGC_DecRC)( DaoBase *p );

	DaoCallbackData* (*DaoCallbackData_New)( DaoRoutine *callback, DValue userdata );
};



#ifdef DAO_DIRECT_API

/* Initialize the Dao library */
DAO_DLL DaoVmSpace* DaoInit();
/* Wait for unfinished computation and then quit */
DAO_DLL void        DaoQuit();

/* Create basic Dao values */
DAO_DLL DValue DValue_NewInteger( dint v );
DAO_DLL DValue DValue_NewFloat( float v );
DAO_DLL DValue DValue_NewDouble( double v );
DAO_DLL DValue DValue_NewMBString( const char *s, int n );
DAO_DLL DValue DValue_NewWCString( const wchar_t *s, int n );
DAO_DLL DValue DValue_NewVectorB( char *s, int n );
DAO_DLL DValue DValue_NewVectorUB( unsigned char *s, int n );
DAO_DLL DValue DValue_NewVectorS( short *s, int n );
DAO_DLL DValue DValue_NewVectorUS( unsigned short *s, int n );
DAO_DLL DValue DValue_NewVectorI( int *s, int n );
DAO_DLL DValue DValue_NewVectorUI( unsigned int *s, int n );
DAO_DLL DValue DValue_NewVectorF( float *s, int n );
DAO_DLL DValue DValue_NewVectorD( double *s, int n );
DAO_DLL DValue DValue_NewMatrixB( signed char **s, int n, int m );
DAO_DLL DValue DValue_NewMatrixUB( unsigned char **s, int n, int m );
DAO_DLL DValue DValue_NewMatrixS( short **s, int n, int m );
DAO_DLL DValue DValue_NewMatrixUS( unsigned short **s, int n, int m );
DAO_DLL DValue DValue_NewMatrixI( int **s, int n, int m );
DAO_DLL DValue DValue_NewMatrixUI( unsigned int **s, int n, int m );
DAO_DLL DValue DValue_NewMatrixF( float **s, int n, int m );
DAO_DLL DValue DValue_NewMatrixD( double **s, int n, int m );
DAO_DLL DValue DValue_NewBuffer( void *s, int n );
DAO_DLL DValue DValue_NewStream( FILE *f );
/* data will be deleted with the DaoCData structure created by this function */
DAO_DLL DValue DValue_NewCData( DaoTypeBase *typer, void *data );
/* data will NOT be deleted with the DaoCData structure created by this function */
DAO_DLL DValue DValue_WrapCData( DaoTypeBase *typer, void *data );
DAO_DLL void DValue_Copy( DValue *self, DValue from );
DAO_DLL void DValue_Clear( DValue *v );
DAO_DLL void DValue_ClearAll( DValue *v, int n );

/* --------------------------------
 * String Manipulation:
 * --------------------------------*/
DAO_DLL DString* DString_New( int mbs );
DAO_DLL void DString_Delete( DString *self );

DAO_DLL size_t DString_Size( DString *self );
DAO_DLL void DString_Clear( DString *self );
DAO_DLL void DString_Resize( DString *self, size_t size );

DAO_DLL int DString_IsMBS( DString *self );
DAO_DLL void DString_SetMBS( DString *self, const char *chs );
DAO_DLL void DString_SetWCS( DString *self, const wchar_t *chs );
DAO_DLL void DString_SetDataMBS( DString *self, const char *data, size_t n );
DAO_DLL void DString_SetDataWCS( DString *self, const wchar_t *data, size_t n );
DAO_DLL void DString_ToWCS( DString *self );
DAO_DLL void DString_ToMBS( DString *self );
DAO_DLL char* DString_GetMBS( DString *self );
DAO_DLL wchar_t* DString_GetWCS( DString *self );

DAO_DLL void DString_Erase( DString *self, size_t start, size_t n );
/* insert "s" at "i" with "m" characters removed and "n" copied,
   if "n" is zero, copy all characters of "s" */
DAO_DLL void DString_Insert( DString *self, DString *s, size_t i, size_t m, size_t n );
DAO_DLL void DString_InsertChar( DString *self, const char ch, size_t at );
/* insert "s" at "i" with "m" characters removed and "n" copied,
   if "n" is zero, consider "s" as null-terminated string. */
DAO_DLL void DString_InsertMBS( DString *self, const char *s, size_t i, size_t m, size_t n );
DAO_DLL void DString_InsertWCS( DString *self, const wchar_t *s, size_t i, size_t m, size_t n );
DAO_DLL void DString_Append( DString *self, DString *chs );
DAO_DLL void DString_AppendChar( DString *self, const char ch );
DAO_DLL void DString_AppendWChar( DString *self, const wchar_t ch );
DAO_DLL void DString_AppendMBS( DString *self, const char *chs );
DAO_DLL void DString_AppendWCS( DString *self, const wchar_t *chs );
DAO_DLL void DString_AppendDataMBS( DString *self, const char *data, size_t n );
DAO_DLL void DString_AppendDataWCS( DString *self, const wchar_t *data,size_t n );
DAO_DLL void DString_SubString( DString *self, DString *sub, size_t from, size_t n );

DAO_DLL size_t DString_Find( DString *self, DString *chs, size_t start );
DAO_DLL size_t DString_RFind( DString *self, DString *chs, size_t start );
DAO_DLL size_t DString_FindMBS( DString *self, const char *ch, size_t start );
DAO_DLL size_t DString_RFindMBS( DString *self, const char *ch, size_t start );
DAO_DLL size_t DString_FindChar( DString *self, char ch, size_t start );
DAO_DLL size_t DString_FindWChar( DString *self, wchar_t ch, size_t start );
DAO_DLL size_t DString_RFindChar( DString *self, char ch, size_t start );

DAO_DLL DString* DString_Copy( DString *self );
DAO_DLL void DString_Assign( DString *left, DString *right );
DAO_DLL int  DString_Compare( DString *left, DString *right );

DAO_DLL DaoList* DaoList_New();
DAO_DLL int  DaoList_Size( DaoList *self );
DAO_DLL DValue DaoList_Front( DaoList *self );
DAO_DLL DValue DaoList_Back( DaoList *self );
DAO_DLL DValue DaoList_GetItem( DaoList *self, int pos );

DAO_DLL void DaoList_SetItem( DaoList *self, DValue item, int pos );
DAO_DLL void DaoList_Insert( DaoList *self, DValue item, int pos );
DAO_DLL void DaoList_Erase( DaoList *self, int pos );
DAO_DLL void DaoList_Clear( DaoList *self );
DAO_DLL void DaoList_PushFront( DaoList *self, DValue item );
DAO_DLL void DaoList_PushBack( DaoList *self, DValue item );
DAO_DLL void DaoList_PopFront( DaoList *self );
DAO_DLL void DaoList_PopBack( DaoList *self );

DAO_DLL DaoMap* DaoMap_New( int hashing );
DAO_DLL int  DaoMap_Size( DaoMap *self );
DAO_DLL int  DaoMap_Insert( DaoMap *self, DValue key, DValue value );
DAO_DLL void DaoMap_Erase( DaoMap *self, DValue key );
DAO_DLL void DaoMap_Clear( DaoMap *self );
DAO_DLL void DaoMap_InsertMBS( DaoMap *self, const char *key, DValue value );
DAO_DLL void DaoMap_InsertWCS( DaoMap *self, const wchar_t *key, DValue value );
DAO_DLL void DaoMap_EraseMBS( DaoMap *self, const char *key );
DAO_DLL void DaoMap_EraseWCS( DaoMap *self, const wchar_t *key );
DAO_DLL DValue DaoMap_GetValue( DaoMap *self, DValue key  );
DAO_DLL DValue DaoMap_GetValueMBS( DaoMap *self, const char *key  );
DAO_DLL DValue DaoMap_GetValueWCS( DaoMap *self, const wchar_t *key  );
DAO_DLL DNode* DaoMap_First( DaoMap *self );
DAO_DLL DNode* DaoMap_Next( DaoMap *self, DNode *iter );
DAO_DLL DValue* DNode_Key( DNode *self );
DAO_DLL DValue* DNode_Value( DNode *self );

DAO_DLL DaoTuple* DaoTuple_New( int size );
DAO_DLL int  DaoTuple_Size( DaoTuple *self );
DAO_DLL void   DaoTuple_SetItem( DaoTuple *self, DValue it, int pos );
DAO_DLL DValue DaoTuple_GetItem( DaoTuple *self, int pos );

DAO_DLL int  DaoArray_NumType( DaoArray *self );
DAO_DLL void DaoArray_SetNumType( DaoArray *self, short numtype );
DAO_DLL int  DaoArray_Size( DaoArray *self );
DAO_DLL int  DaoArray_DimCount( DaoArray *self );
DAO_DLL int   DaoArray_SizeOfDim( DaoArray *self, int d );
DAO_DLL void  DaoArray_GetShape( DaoArray *self, size_t *dims );
DAO_DLL int DaoArray_HasShape( DaoArray *self, size_t *dims, int D );
DAO_DLL int   DaoArray_GetFlatIndex( DaoArray *self, size_t *index );
DAO_DLL void  DaoArray_ResizeVector( DaoArray *self, int N );
DAO_DLL void  DaoArray_ResizeArray( DaoArray *self, size_t *dims, int D );
DAO_DLL int DaoArray_Reshape( DaoArray *self, size_t *dims, int D );

DAO_DLL signed char* DaoArray_ToByte( DaoArray *self );
DAO_DLL short*  DaoArray_ToShort( DaoArray *self );
DAO_DLL int*    DaoArray_ToInt( DaoArray *self );
DAO_DLL float*  DaoArray_ToFloat( DaoArray *self );
DAO_DLL double* DaoArray_ToDouble( DaoArray *self );
DAO_DLL unsigned char*  DaoArray_ToUByte( DaoArray *self );
DAO_DLL unsigned short* DaoArray_ToUShort( DaoArray *self );
DAO_DLL unsigned int*   DaoArray_ToUInt( DaoArray *self );

DAO_DLL signed char** DaoArray_GetMatrixB( DaoArray *self, int row );
DAO_DLL short**  DaoArray_GetMatrixS( DaoArray *self, int row );
DAO_DLL int**    DaoArray_GetMatrixI( DaoArray *self, int row );
DAO_DLL float**  DaoArray_GetMatrixF( DaoArray *self, int row );
DAO_DLL double** DaoArray_GetMatrixD( DaoArray *self, int row );

DAO_DLL void  DaoArray_FromByte( DaoArray *self );
DAO_DLL void  DaoArray_FromShort( DaoArray *self );
DAO_DLL void  DaoArray_FromInt( DaoArray *self );
DAO_DLL void  DaoArray_FromFloat( DaoArray *self );
DAO_DLL void  DaoArray_FromDouble( DaoArray *self );
DAO_DLL void  DaoArray_FromUByte( DaoArray *self );
DAO_DLL void  DaoArray_FromUShort( DaoArray *self );
DAO_DLL void  DaoArray_FromUInt( DaoArray *self );

DAO_DLL void  DaoArray_SetVectorB( DaoArray *self, char* vec, int N );
DAO_DLL void  DaoArray_SetVectorS( DaoArray *self, short* vec, int N );
DAO_DLL void  DaoArray_SetVectorI( DaoArray *self, int* vec, int N );
DAO_DLL void  DaoArray_SetVectorF( DaoArray *self, float* vec, int N );
DAO_DLL void  DaoArray_SetVectorD( DaoArray *self, double* vec, int N );
DAO_DLL void  DaoArray_SetMatrixB( DaoArray *self, signed char **mat, int row, int col );
DAO_DLL void  DaoArray_SetMatrixS( DaoArray *self, short **mat, int row, int col );
DAO_DLL void  DaoArray_SetMatrixI( DaoArray *self, int **mat, int row, int col );
DAO_DLL void  DaoArray_SetMatrixF( DaoArray *self, float **mat, int row, int col );
DAO_DLL void  DaoArray_SetMatrixD( DaoArray *self, double **mat, int row, int col );
DAO_DLL void  DaoArray_SetVectorUB( DaoArray *self, unsigned char* vec, int N );
DAO_DLL void  DaoArray_SetVectorUS( DaoArray *self, unsigned short* vec, int N );
DAO_DLL void  DaoArray_SetVectorUI( DaoArray *self, unsigned int* vec, int N );

DAO_DLL void* DaoArray_GetBuffer( DaoArray *self );
DAO_DLL void DaoArray_SetBuffer( DaoArray *self, void *buffer, size_t size );

DAO_DLL DValue DaoObject_GetField( DaoObject *self, const char *name );
DAO_DLL DaoCData* DaoObject_MapCData( DaoObject *self, DaoTypeBase *typer );

DAO_DLL DaoStream* DaoStream_New();
DAO_DLL void DaoStream_SetFile( DaoStream *self, FILE *fd );
DAO_DLL FILE* DaoStream_GetFile( DaoStream *self );

/* self->func( p, ... ); NOT IMPLEMENTED YET!!! */
DAO_DLL int DaoFunction_Call( DaoFunction *func, DaoCData *self, DValue *p[], int n );

/* data will be deleted with the new DaoCData */
DAO_DLL DaoCData* DaoCData_New( DaoTypeBase *typer, void *data );
/* data will not be deleted with the new DaoCData */
DAO_DLL DaoCData* DaoCData_Wrap( DaoTypeBase *typer, void *data );
DAO_DLL int    DaoCData_IsType( DaoCData *self, DaoTypeBase *typer );
DAO_DLL void   DaoCData_SetExtReference( DaoCData *self, int bl );
DAO_DLL void   DaoCData_SetData( DaoCData *self, void *data );
DAO_DLL void   DaoCData_SetBuffer( DaoCData *self, void *data, size_t size );
DAO_DLL void   DaoCData_SetArray( DaoCData *self, void *data, size_t size, int itsize );
DAO_DLL void*  DaoCData_GetData( DaoCData *self );
DAO_DLL void*  DaoCData_GetBuffer( DaoCData *self );
DAO_DLL void** DaoCData_GetData2( DaoCData *self );
DAO_DLL DaoObject* DaoCData_GetObject( DaoCData *self );
DAO_DLL DaoTypeBase* DaoCData_GetTyper( DaoCData *self );

DAO_DLL DaoMutex* DaoMutex_New( DaoVmSpace *vms );
DAO_DLL void DaoMutex_Lock( DaoMutex *self );
DAO_DLL void DaoMutex_Unlock( DaoMutex *self );
DAO_DLL int DaoMutex_TryLock( DaoMutex *self );

DAO_DLL dint*      DaoContext_PutInteger( DaoContext *self, dint value );
DAO_DLL float*     DaoContext_PutFloat( DaoContext *self, float value );
DAO_DLL double*    DaoContext_PutDouble( DaoContext *self, double value );
DAO_DLL complex16* DaoContext_PutComplex( DaoContext *self, complex16 value );
DAO_DLL DString*   DaoContext_PutMBString( DaoContext *self, const char *mbs );
DAO_DLL DString*   DaoContext_PutWCString( DaoContext *self, const wchar_t *wcs );
DAO_DLL DString*   DaoContext_PutString( DaoContext *self, DString *str );
DAO_DLL DString*   DaoContext_PutBytes( DaoContext *self, const char *bytes, int N );
DAO_DLL DEnum*     DaoContext_PutEnum( DaoContext *self, const char *symbols );
DAO_DLL DaoArray*  DaoContext_PutArrayInteger( DaoContext *self, int *array, int N );
DAO_DLL DaoArray*  DaoContext_PutArrayShort( DaoContext *self, short *array, int N );
DAO_DLL DaoArray*  DaoContext_PutArrayFloat( DaoContext *self, float *array, int N );
DAO_DLL DaoArray*  DaoContext_PutArrayDouble( DaoContext *self, double *array, int N );
DAO_DLL DaoArray*  DaoContext_PutArrayComplex( DaoContext *self, complex16 *array, int N );
DAO_DLL DaoList*   DaoContext_PutList( DaoContext *self );
DAO_DLL DaoMap*    DaoContext_PutMap( DaoContext *self );
DAO_DLL DaoArray*  DaoContext_PutArray( DaoContext *self );
DAO_DLL DaoStream* DaoContext_PutFile( DaoContext *self, FILE *file );
DAO_DLL DValue* DaoContext_PutValue( DaoContext *self, DValue value );
/* data will be deleted with the new DaoCData */
DAO_DLL DaoCData*  DaoContext_PutCData( DaoContext *self, void *data, DaoTypeBase *typer );
DAO_DLL DaoCData*  DaoContext_PutCPointer( DaoContext *self, void *data, int size );
DAO_DLL DaoBase*   DaoContext_PutResult( DaoContext *self, DaoBase *data );
/* data will not be deleted with the new DaoCData */
DAO_DLL DaoCData*  DaoContext_WrapCData( DaoContext *self, void *data, DaoTypeBase *typer );
/* data will be deleted with the new DaoCData */
DAO_DLL DaoCData*  DaoContext_CopyCData( DaoContext *self, void *d, int n, DaoTypeBase *t );

DAO_DLL void DaoContext_RaiseException( DaoContext *self, int type, const char *value );

DAO_DLL DaoVmProcess* DaoVmProcess_New( DaoVmSpace *vms );
DAO_DLL int DaoVmProcess_Compile( DaoVmProcess *self, DaoNameSpace *ns, DString *src, int rpl );
DAO_DLL int DaoVmProcess_Eval   ( DaoVmProcess *self, DaoNameSpace *ns, DString *src, int rpl );
DAO_DLL int DaoVmProcess_Call( DaoVmProcess *s, DaoRoutine *r, DaoObject *o, DValue *p[], int n );
DAO_DLL void  DaoVmProcess_Stop( DaoVmProcess *self );
DAO_DLL DValue DaoVmProcess_GetReturned( DaoVmProcess *self );

DAO_DLL DaoNameSpace* DaoNameSpace_New( DaoVmSpace *vms );
/* get namespace with the name, create if not exits: */
DAO_DLL DaoNameSpace* DaoNameSpace_GetNameSpace( DaoNameSpace *self, const char *name );
DAO_DLL void DaoNameSpace_AddParent( DaoNameSpace *self, DaoNameSpace *parent );
DAO_DLL void DaoNameSpace_AddConstNumbers( DaoNameSpace *self0, DaoNumItem *items );
DAO_DLL void DaoNameSpace_AddConstValue( DaoNameSpace *self, const char *s, DValue v );
DAO_DLL void DaoNameSpace_AddConstData( DaoNameSpace *self, const char *name, DaoBase *data );
DAO_DLL void DaoNameSpace_AddData( DaoNameSpace *self, const char *name, DaoBase *data, const char *type);
DAO_DLL void DaoNameSpace_AddValue( DaoNameSpace *self, const char *name, DValue data, const char *type);
DAO_DLL DValue DaoNameSpace_FindData( DaoNameSpace *self, const char *name );

/* equivalent to: typedef old type; in scripts */
DAO_DLL int DaoNameSpace_TypeDefine( DaoNameSpace *self, const char *old, const char *type );
/*
   parameters alias[] is an array of type name aliases,
   used as typedefs like: typedef alias[2*i] alias[2*i+1];
   the last item in alias[] should also be NULL.
 */
DAO_DLL int DaoNameSpace_TypeDefines( DaoNameSpace *self, const char *alias[] );
/* wrap c type */
DAO_DLL int DaoNameSpace_WrapType( DaoNameSpace *self, DaoTypeBase *typer );
/* wrap c types, the last item in typer[] should be NULL;
   types that are cross-used in parameter lists
   (e.g. type A appears in the parameter list of B's methods,
   and type B appears in the parameter list of A's methods),
   should be wrapd using this function.
 */
DAO_DLL int DaoNameSpace_WrapTypes( DaoNameSpace *self, DaoTypeBase *typer[] );
/* wrap c function */
DAO_DLL int DaoNameSpace_WrapFunction( DaoNameSpace *self, DaoFuncPtr fp, const char *proto );
/* wrap c functions */
DAO_DLL int DaoNameSpace_WrapFunctions( DaoNameSpace *self, DaoFuncItem *items );
DAO_DLL int DaoNameSpace_SetupType( DaoNameSpace *self, DaoTypeBase *typer );
DAO_DLL int DaoNameSpace_SetupTypes( DaoNameSpace *self, DaoTypeBase *typer[] );
/* load the scripts in "file" to the namespace */
DAO_DLL int DaoNameSpace_Load( DaoNameSpace *self, const char *file );
DAO_DLL int DaoNameSpace_GetOptions( DaoNameSpace *self );
DAO_DLL void DaoNameSpace_SetOptions( DaoNameSpace *self, int options );

DAO_DLL DaoVmSpace* DaoVmSpace_New();
DAO_DLL int DaoVmSpace_ParseOptions( DaoVmSpace *self, DString *options );
DAO_DLL void DaoVmSpace_SetOptions( DaoVmSpace *self, int options );
DAO_DLL int  DaoVmSpace_GetOptions( DaoVmSpace *self );

DAO_DLL int DaoVmSpace_RunMain( DaoVmSpace *self, DString *file );
DAO_DLL DaoNameSpace* DaoVmSpace_Load( DaoVmSpace *self, DString *file );
DAO_DLL DaoNameSpace* DaoVmSpace_GetNameSpace( DaoVmSpace *self, const char *name );
DAO_DLL DaoNameSpace* DaoVmSpace_MainNameSpace( DaoVmSpace *self );
DAO_DLL DaoVmProcess* DaoVmSpace_MainVmProcess( DaoVmSpace *self );
/* get a process object from a pool */
DAO_DLL DaoVmProcess* DaoVmSpace_AcquireProcess( DaoVmSpace *self );
/* return a process object from a pool */
DAO_DLL void DaoVmSpace_ReleaseProcess( DaoVmSpace *self, DaoVmProcess *proc );

DAO_DLL void DaoVmSpace_SetUserHandler( DaoVmSpace *self, DaoUserHandler *handler );
DAO_DLL void DaoVmSpace_ReadLine( DaoVmSpace *self, ReadLine fptr );
DAO_DLL void DaoVmSpace_AddHistory( DaoVmSpace *self, AddHistory fptr );

DAO_DLL void DaoVmSpace_AddVirtualFile( DaoVmSpace *self, const char *f, const char *s );
DAO_DLL void DaoVmSpace_SetPath( DaoVmSpace *self, const char *path );
DAO_DLL void DaoVmSpace_AddPath( DaoVmSpace *self, const char *path );
DAO_DLL void DaoVmSpace_DelPath( DaoVmSpace *self, const char *path );

DAO_DLL void  DaoVmSpace_Stop( DaoVmSpace *self, int bl );

DAO_DLL void DaoGC_IncRC( DaoBase *p );
DAO_DLL void DaoGC_DecRC( DaoBase *p );

DAO_DLL DaoCallbackData* DaoCallbackData_New( DaoRoutine *callback, DValue userdata );

#else

#define DaoInit()  __dao.DaoInit()
#define DaoQuit()  __dao.DaoQuit()

#define DValue_NewInteger( v )  __dao.DValue_NewInteger( v )
#define DValue_NewFloat( v )  __dao.DValue_NewFloat( v )
#define DValue_NewDouble( v )  __dao.DValue_NewDouble( v )
#define DValue_NewMBString( s, n )  __dao.DValue_NewMBString( s, n )
#define DValue_NewWCString( s, n )  __dao.DValue_NewWCString( s, n )
#define DValue_NewVectorB( s, n )  __dao.DValue_NewVectorB( s, n )
#define DValue_NewVectorUB( s, n )  __dao.DValue_NewVectorUB( s, n )
#define DValue_NewVectorS( s, n )  __dao.DValue_NewVectorS( s, n )
#define DValue_NewVectorUS( s, n )  __dao.DValue_NewVectorUS( s, n )
#define DValue_NewVectorI( s, n )  __dao.DValue_NewVectorI( s, n )
#define DValue_NewVectorUI( s, n )  __dao.DValue_NewVectorUI( s, n )
#define DValue_NewVectorF( s, n )  __dao.DValue_NewVectorF( s, n )
#define DValue_NewVectorD( s, n )  __dao.DValue_NewVectorD( s, n )
#define DValue_NewMatrixB( s, n, m )  __dao.DValue_NewMatrixB( s, n, m )
#define DValue_NewMatrixUB( s, n, m )  __dao.DValue_NewMatrixUB( s, n, m )
#define DValue_NewMatrixS( s, n, m )  __dao.DValue_NewMatrixS( s, n, m )
#define DValue_NewMatrixUS( s, n, m )  __dao.DValue_NewMatrixUS( s, n, m )
#define DValue_NewMatrixI( s, n, m )  __dao.DValue_NewMatrixI( s, n, m )
#define DValue_NewMatrixUI( s, n, m )  __dao.DValue_NewMatrixUI( s, n, m )
#define DValue_NewMatrixF( s, n, m )  __dao.DValue_NewMatrixF( s, n, m )
#define DValue_NewMatrixD( s, n, m )  __dao.DValue_NewMatrixD( s, n, m )
#define DValue_NewBuffer( s, n )  __dao.DValue_NewBuffer( s, n )
#define DValue_NewStream( f )  __dao.DValue_NewStream( f )
#define DValue_NewCData( typer, data )  __dao.DValue_NewCData( typer, data )
#define DValue_WrapCData( typer, data )  __dao.DValue_WrapCData( typer, data )
#define DValue_Copy( self, from )  __dao.DValue_Copy( self, from )
#define DValue_Clear( v )  __dao.DValue_Clear( v )
#define DValue_ClearAll( v, n )  __dao.DValue_ClearAll( v, n )

#define DString_New( mbs ) __dao.DString_New( mbs )
#define DString_Delete( self ) __dao.DString_Delete( self )
#define DString_Size( self )  __dao.DString_Size( self )
#define DString_Clear( self )  __dao.DString_Clear( self )
#define DString_Resize( self, size )  __dao.DString_Resize( self, size )

#define DString_IsMBS( self )  __dao.DString_IsMBS( self )
#define DString_SetMBS( self, chs )  __dao.DString_SetMBS( self, chs )
#define DString_SetWCS( self, chs )  __dao.DString_SetWCS( self, chs )
#define DString_SetDataMBS( self, s, n )  __dao.DString_SetDataMBS( self, s, n )
#define DString_SetDataWCS( self, s, n )  __dao.DString_SetDataWCS( self, s, n )
#define DString_ToWCS( self )  __dao.DString_ToWCS( self )
#define DString_ToMBS( self )  __dao.DString_ToMBS( self )
#define DString_GetMBS( self )  __dao.DString_GetMBS( self )
#define DString_GetWCS( self )  __dao.DString_GetWCS( self )

#define DString_Erase( self, tart, n )  __dao.DString_Erase( self, tart, n )
#define DString_Insert( self, s, i, m, n )  __dao.DString_Insert( self, s, i, m, n )
#define DString_InsertMBS( self, s, i, m, n )  __dao.DString_InsertMBS( self, s, i, m, n )
#define DString_InsertChar( self, ch, at )  __dao.DString_InsertChar( self, ch, at )
#define DString_InsertWCS( self, s, i, m, n )  __dao.DString_InsertWCS( self, s, i, m, n )
#define DString_Append( self, chs )  __dao.DString_Append( self, chs )
#define DString_AppendChar( self, ch )  __dao.DString_AppendChar( self, ch )
#define DString_AppendWChar( self, ch )  __dao.DString_AppendWChar( self, ch )
#define DString_AppendMBS( self, chs )  __dao.DString_AppendMBS( self, chs )
#define DString_AppendWCS( self, chs )  __dao.DString_AppendWCS( self, chs )
#define DString_AppendDataMBS( self, s, n )  __dao.DString_AppendDataMBS( self, s, n )
#define DString_AppendDataWCS( self, s, n )  __dao.DString_AppendDataWCS( self, s, n )
#define DString_SubString( self, sub, i, n )  __dao.DString_SubString( self, sub, i, n )

#define DString_Find( self, chs, start )  __dao.DString_Find( self, chs, start )
#define DString_RFind( self, chs, start )  __dao.DString_RFind( self, chs, start )
#define DString_FindMBS( self, ch, start )  __dao.DString_FindMBS( self, ch, start )
#define DString_RFindMBS( self, ch, start )  __dao.DString_RFindMBS( self, ch, start )
#define DString_FindChar( self, ch, start )  __dao.DString_FindChar( self, ch, start )
#define DString_FindWChar( self, ch, start )  __dao.DString_FindWChar( self, ch, start )
#define DString_RFindChar( self, ch, start )  __dao.DString_RFindChar( self, ch, start )

#define DString_Copy( self )  __dao.DString_Copy( self )
#define DString_Assign( left, right )  __dao.DString_Assign( left, right )
#define DString_Compare( left, right )  __dao.DString_Compare( left, right )

#define DaoList_New()  __dao.DaoList_New()
#define DaoList_Size( self )  __dao.DaoList_Size( self )
#define DaoList_Front( self )  __dao.DaoList_Front( self )
#define DaoList_Back( self )  __dao.DaoList_Back( self )
#define DaoList_GetItem( self, pos )  __dao.DaoList_GetItem( self, pos )

#define DaoList_SetItem( self, item, pos )  __dao.DaoList_SetItem( self, item, pos )
#define DaoList_Insert( self, item, pos )  __dao.DaoList_Insert( self, item, pos )
#define DaoList_Erase( self, pos )  __dao.DaoList_Erase( self, pos )
#define DaoList_Clear( self )  __dao.DaoList_Clear( self )
#define DaoList_PushFront( self, item )  __dao.DaoList_PushFront( self, item )
#define DaoList_PushBack( self, item )  __dao.DaoList_PushBack( self, item )
#define DaoList_PopFront( self )  __dao.DaoList_PopFront( self )
#define DaoList_PopBack( self )  __dao.DaoList_PopBack( self )

#define DaoMap_New( hashing )  __dao.DaoMap_New( hashing )
#define DaoMap_Size( self )  __dao.DaoMap_Size( self )
#define DaoMap_Insert( self, key, value )  __dao.DaoMap_Insert( self, key, value )
#define DaoMap_Erase( self, key )  __dao.DaoMap_Erase( self, key )
#define DaoMap_Clear( self )  __dao.DaoMap_Clear( self )
#define DaoMap_InsertMBS( self, key, value )  __dao.DaoMap_InsertMBS( self, key, value )
#define DaoMap_InsertWCS( self, key, value )  __dao.DaoMap_InsertWCS( self, key, value )
#define DaoMap_EraseMBS( self, key )  __dao.DaoMap_EraseMBS( self, key )
#define DaoMap_EraseWCS( self, key )  __dao.DaoMap_EraseWCS( self, key )
#define DaoMap_GetValue( self, key  )  __dao.DaoMap_GetValue( self, key  )
#define DaoMap_GetValueMBS( self, key  )  __dao.DaoMap_GetValueMBS( self, key  )
#define DaoMap_GetValueWCS( self, key  )  __dao.DaoMap_GetValueWCS( self, key  )
#define DaoMap_First( self )  __dao.DaoMap_First( self )
#define DaoMap_Next( self, iter )  __dao.DaoMap_Next( self, iter )
#define DNode_Key( self )  __dao.DNode_Key( self )
#define DNode_Value( self )  __dao.DNode_Value( self )

#define DaoTuple_New( size )  __dao.DaoTuple_New( size )
#define DaoTuple_Size( self )  __dao.DaoTuple_Size( self )
#define DaoTuple_SetItem( self, it, pos )  __dao.DaoTuple_SetItem( self, it, pos )
#define DaoTuple_GetItem( self, pos )  __dao.DaoTuple_GetItem( self, pos )

#define DaoArray_NumType( self ) __dao.DaoArray_NumType( self )
#define DaoArray_SetNumType( self, numtype ) __dao.DaoArray_SetNumType( self, numtype )
#define DaoArray_Size( self ) __dao.DaoArray_Size( self )
#define DaoArray_DimCount( self ) __dao.DaoArray_DimCount( self )
#define DaoArray_SizeOfDim( self, d ) __dao.DaoArray_SizeOfDim( self, d )
#define DaoArray_GetShape( self, dims ) __dao.DaoArray_GetShape( self, dims )
#define DaoArray_HasShape( self, dims, D ) __dao.DaoArray_HasShape( self, dims, D )
#define DaoArray_GetFlatIndex( self, index ) __dao.DaoArray_GetFlatIndex( self, index )
#define DaoArray_ResizeVector( self, N ) __dao.DaoArray_ResizeVector( self, N )
#define DaoArray_ResizeArray( self, dims, D ) __dao.DaoArray_ResizeArray( self, dims, D )
#define DaoArray_Reshape( self, dims, D ) __dao.DaoArray_Reshape( self, dims, D )

#define DaoArray_ToByte( self ) __dao.DaoArray_ToByte( self )
#define DaoArray_ToShort( self ) __dao.DaoArray_ToShort( self )
#define DaoArray_ToInt( self ) __dao.DaoArray_ToInt( self )
#define DaoArray_ToFloat( self ) __dao.DaoArray_ToFloat( self )
#define DaoArray_ToDouble( self ) __dao.DaoArray_ToDouble( self )
#define DaoArray_ToUByte( self ) __dao.DaoArray_ToUByte( self )
#define DaoArray_ToUShort( self ) __dao.DaoArray_ToUShort( self )
#define DaoArray_ToUInt( self ) __dao.DaoArray_ToUInt( self )

#define DaoArray_GetMatrixB( self, row ) __dao.DaoArray_GetMatrixB( self, row )
#define DaoArray_GetMatrixS( self, row ) __dao.DaoArray_GetMatrixS( self, row )
#define DaoArray_GetMatrixI( self, row ) __dao.DaoArray_GetMatrixI( self, row )
#define DaoArray_GetMatrixF( self, row ) __dao.DaoArray_GetMatrixF( self, row )
#define DaoArray_GetMatrixD( self, row ) __dao.DaoArray_GetMatrixD( self, row )

#define DaoArray_FromByte( self )  __dao.DaoArray_FromByte( self )
#define DaoArray_FromShort( self )  __dao.DaoArray_FromShort( self )
#define DaoArray_FromInt( self )  __dao.DaoArray_FromInt( self )
#define DaoArray_FromFloat( self )  __dao.DaoArray_FromFloat( self )
#define DaoArray_FromDouble( self )  __dao.DaoArray_FromDouble( self )
#define DaoArray_FromUByte( self )  __dao.DaoArray_FromUByte( self )
#define DaoArray_FromUShort( self )  __dao.DaoArray_FromUShort( self )
#define DaoArray_FromUInt( self )  __dao.DaoArray_FromUInt( self )

#define DaoArray_SetVectorB( self,  vec, N ) __dao.DaoArray_SetVectorB( self,  vec, N )
#define DaoArray_SetVectorS( self,  vec, N ) __dao.DaoArray_SetVectorS( self,  vec, N )
#define DaoArray_SetVectorI( self,  vec, N ) __dao.DaoArray_SetVectorI( self,  vec, N )
#define DaoArray_SetVectorF( self,  vec, N ) __dao.DaoArray_SetVectorF( self,  vec, N )
#define DaoArray_SetVectorD( self,  vec, N ) __dao.DaoArray_SetVectorD( self,  vec, N )
#define DaoArray_SetMatrixB( self, mat, n, m ) __dao.DaoArray_SetMatrixB( self, mat, n, m )
#define DaoArray_SetMatrixS( self, mat, n, m ) __dao.DaoArray_SetMatrixS( self, mat, n, m )
#define DaoArray_SetMatrixI( self, mat, n, m ) __dao.DaoArray_SetMatrixI( self, mat, n, m )
#define DaoArray_SetMatrixF( self, mat, n, m ) __dao.DaoArray_SetMatrixF( self, mat, n, m )
#define DaoArray_SetMatrixD( self, mat, n, m ) __dao.DaoArray_SetMatrixD( self, mat, n, m )
#define DaoArray_SetVectorUB( self, vec, N ) __dao.DaoArray_SetVectorUB( self, vec, N )
#define DaoArray_SetVectorUS( self, vec, N ) __dao.DaoArray_SetVectorUS( self, vec, N )
#define DaoArray_SetVectorUI( self, vec, N ) __dao.DaoArray_SetVectorUI( self, vec, N )

#define DaoArray_GetBuffer( self )  __dao.DaoArray_GetBuffer( self )
#define DaoArray_SetBuffer( self, buffer, size )  __dao.SetBuffer( self, buffer, size )

#define DaoObject_GetField( self, name )  __dao.DaoObject_GetField( self, name )
#define DaoObject_MapCData( self, typer )  __dao.DaoObject_MapCData( self, typer )

#define DaoStream_New()  __dao.DaoStream_New()
#define DaoStream_SetFile( self, fd )  __dao.DaoStream_SetFile( self, fd )
#define DaoStream_GetFile( self )  __dao.DaoStream_GetFile( self )

#define DaoFunction_Call( func, self, p, n )  __dao.DaoFunction_Call( func, self, p, n )

#define DaoCData_New( typer, data )  __dao.DaoCData_New( typer, data )
#define DaoCData_Wrap( typer, data )  __dao.DaoCData_Wrap( typer, data )
#define DaoCData_IsType( self, typer )  __dao.DaoCData_IsType( self, typer )
#define DaoCData_SetExtReference( self, bl ) __dao.DaoCData_SetExtReference( self, bl )
#define DaoCData_SetData( self, data ) __dao.DaoCData_SetData( self, data )
#define DaoCData_SetBuffer( self, data, size ) __dao.DaoCData_SetBuffer( self, data, size )
#define DaoCData_SetArray( self, data, n, ms ) __dao.DaoCData_SetArray( self, data, n, ms )
#define DaoCData_GetData( self ) __dao.DaoCData_GetData( self )
#define DaoCData_GetBuffer( self ) __dao.DaoCData_GetBuffer( self )
#define DaoCData_GetData2( self ) __dao.DaoCData_GetData2( self )
#define DaoCData_GetObject( self ) __dao.DaoCData_GetObject( self )
#define DaoCData_GetTyper( self )  __dao.DaoCData_GetTyper( self )

#define DaoMutex_New( vms )  __dao.DaoMutex_New( vms )
#define DaoMutex_Lock( self )  __dao.DaoMutex_Lock( self )
#define DaoMutex_Unlock( self )  __dao.DaoMutex_Unlock( self )
#define DaoMutex_TryLock( self )  __dao.DaoMutex_TryLock( self )

#define DaoContext_PutInteger( self, value ) __dao.DaoContext_PutInteger( self, value )
#define DaoContext_PutFloat( self, value ) __dao.DaoContext_PutFloat( self, value )
#define DaoContext_PutDouble( self, value ) __dao.DaoContext_PutDouble( self, value )
#define DaoContext_PutComplex( self, value ) __dao.DaoContext_PutComplex( self, value )
#define DaoContext_PutMBString( self, mbs ) __dao.DaoContext_PutMBString( self, mbs )
#define DaoContext_PutWCString( self, wcs ) __dao.DaoContext_PutWCString( self, wcs )
#define DaoContext_PutString( self, str ) __dao.DaoContext_PutString( self, str )
#define DaoContext_PutBytes( self, bytes, N ) __dao.DaoContext_PutBytes( self, bytes, N )
#define DaoContext_PutEnum( self, symbols ) __dao.DaoContext_PutEnum( self, symbols )
#define DaoContext_PutArrayInteger( s, a, N ) __dao.DaoContext_PutArrayInteger( s, a, N )
#define DaoContext_PutArrayShort( s, a, N ) __dao.DaoContext_PutArrayShort( s, a, N )
#define DaoContext_PutArrayFloat( s, a, N ) __dao.DaoContext_PutArrayFloat( s, a, N )
#define DaoContext_PutArrayDouble( s, a, N ) __dao.DaoContext_PutArrayDouble( s, a, N )
#define DaoContext_PutArrayComplex( s, a, N ) __dao.DaoContext_PutArrayComplex( s, a, N )
#define DaoContext_PutList( self ) __dao.DaoContext_PutList( self )
#define DaoContext_PutMap( self ) __dao.DaoContext_PutMap( self )
#define DaoContext_PutArray( self ) __dao.DaoContext_PutArray( self )
#define DaoContext_PutFile( self, file ) __dao.DaoContext_PutFile( self, file )
#define DaoContext_PutCData( s, data, typer ) __dao.DaoContext_PutCData( s, data, typer )
#define DaoContext_PutCPointer( s, data, size ) __dao.DaoContext_PutCPointer( s, data, size )
#define DaoContext_PutValue(s,v) __dao.DaoContext_PutValue(s, v );
#define DaoContext_PutResult( self, data )  __dao.DaoContext_PutResult( self, data )
#define DaoContext_WrapCData( s, data, typer ) __dao.DaoContext_WrapCData( s, data, typer )
#define DaoContext_CopyCData( s, d, n, t )  __dao.DaoContext_CopyCData( s, d, n, t )

#define DaoContext_RaiseException( s, t, v ) __dao.DaoContext_RaiseException( s, t, v ) ;

#define DaoVmProcess_New( vms )  __dao.DaoVmProcess_New( vms )
#define DaoVmProcess_Compile( self, ns, s, r ) __dao.DaoVmProcess_Compile( self, ns, s, r )
#define DaoVmProcess_Eval( self, ns, src, rp ) __dao.DaoVmProcess_Eval( self, ns, src, rp )
#define DaoVmProcess_Call( s, r, o, p, n )  __dao.DaoVmProcess_Call( s, r, o, p, n )
#define DaoVmProcess_Stop( self )  __dao.DaoVmProcess_Stop( self )
#define DaoVmProcess_GetReturned( s )  __dao.DaoVmProcess_GetReturned( s )

#define DaoNameSpace_New( vms )  __dao.DaoNameSpace_New( vms )
#define DaoNameSpace_GetNameSpace( ns, s ) __dao.DaoNameSpace_GetNameSpace( ns, s )
#define DaoNameSpace_AddParent( self, ns ) __dao.DaoNameSpace_AddParent( self, ns )
#define DaoNameSpace_AddConstNumbers( s, n ) __dao.DaoNameSpace_AddConstNumbers( s, n )
#define DaoNameSpace_AddConstValue( sf,s,v )  __dao.DaoNameSpace_AddConstValue( sf,s,v )
#define DaoNameSpace_AddConstData( sf,s,d )  __dao.DaoNameSpace_AddConstData( sf,s,d )
#define DaoNameSpace_AddData( o, s, d, t)  __dao.DaoNameSpace_AddData( o, s, d, t)
#define DaoNameSpace_AddValue( o, s, d, t)  __dao.DaoNameSpace_AddValue( o, s, d, t)
#define DaoNameSpace_FindData( self, name )  __dao.DaoNameSpace_FindData( self, name )

#define DaoNameSpace_TypeDefine( self, o, n ) __dao.DaoNameSpace_TypeDefine( self, o, n )
#define DaoNameSpace_TypeDefines( self, alias ) __dao.DaoNameSpace_TypeDefines( self, alias )
#define DaoNameSpace_WrapType( self, t ) __dao.DaoNameSpace_WrapType( self, t )
#define DaoNameSpace_WrapTypes( self, ts ) __dao.DaoNameSpace_WrapTypes( self, ts )
#define DaoNameSpace_WrapFunction( ns, f, p ) __dao.DaoNameSpace_WrapFunction( ns, f, p )
#define DaoNameSpace_WrapFunctions( ns, fs ) __dao.DaoNameSpace_WrapFunctions( ns, fs )
#define DaoNameSpace_SetupType( self, typer ) __dao.DaoNameSpace_SetupType( self, typer )
#define DaoNameSpace_SetupTypes( self, types ) __dao.DaoNameSpace_SetupTypes( self, types )
#define DaoNameSpace_Load( self, file ) __dao.DaoNameSpace_Load( self, file )
#define DaoNameSpace_GetOptions( self ) __dao.DaoNameSpace_GetOptions( self )
#define DaoNameSpace_SetOptions( self, opt ) __dao.DaoNameSpace_SetOptions( self, opt )

#define DaoVmSpace_New()  __dao.DaoVmSpace_New()
#define DaoVmSpace_ParseOptions( self, o )  __dao.DaoVmSpace_ParseOptions( self, o )
#define DaoVmSpace_SetOptions( self, o )  __dao.DaoVmSpace_SetOptions( self, o )
#define DaoVmSpace_GetOptions( self )  __dao.DaoVmSpace_GetOptions( self )

#define DaoVmSpace_RunMain( self, file )  __dao.DaoVmSpace_RunMain( self, file )
#define DaoVmSpace_Load( self, file )  __dao.DaoVmSpace_Load( self, file )
#define DaoVmSpace_GetNameSpace( self )  __dao.DaoVmSpace_GetNameSpace( self, name )
#define DaoVmSpace_MainNameSpace( self )  __dao.DaoVmSpace_MainNameSpace( self )
#define DaoVmSpace_MainVmProcess( self )  __dao.DaoVmSpace_MainVmProcess( self )
#define DaoVmSpace_AcquireProcess( self )  __dao.DaoVmSpace_AcquireProcess( self )
#define DaoVmSpace_ReleaseProcess( self, p )  __dao.DaoVmSpace_ReleaseProcess( self, p )

#define DaoVmSpace_SetUserHandler(self, hd)  __dao.DaoVmSpace_SetUserHandler(self, hd)
#define DaoVmSpace_ReadLine( self, fptr )  __dao.DaoVmSpace_ReadLine( self, fptr )
#define DaoVmSpace_AddHistory( self, fptr )  __dao.DaoVmSpace_AddHistory( self, fptr )

#define DaoVmSpace_AddVirtualFile(self,f,s)  __dao.DaoVmSpace_AddVirtualFile(self,f,s)
#define DaoVmSpace_SetPath( self, path )  __dao.DaoVmSpace_SetPath( self, path )
#define DaoVmSpace_AddPath( self, path )  __dao.DaoVmSpace_AddPath( self, path )
#define DaoVmSpace_DelPath( self, path )  __dao.DaoVmSpace_DelPath( self, path )

#define DaoVmSpace_Stop( self, bl )  __dao.DaoVmSpace_Stop( self, bl )

#define DaoGC_IncRC( p )  __dao.DaoGC_IncRC( p )
#define DaoGC_DecRC( p )  __dao.DaoGC_DecRC( p )

#define DaoCallbackData_New( cb, ud ) __dao.DaoCallbackData_New( cb, ud )


#if defined( UNIX ) || defined( WIN32 )
#include<string.h>
/* the parameter must be: NULL, or argv[0], or a Dao DLL file */
static int DaoInitLibrary( const char *bin )
{
	FuncInitAPI pfunc;
	void *handle = NULL;
	char *daodir = getenv( "DAO_DIR" );
	char *daolib = NULL;
	int k=0;

	/* Dao DLL file: */
	if( bin && strcmp( bin + (strlen( bin ) - strlen( DAO_DLL_SUFFIX )), DAO_DLL_SUFFIX ) ==0 ){
		handle = DaoLoadLibrary( bin );
		pfunc = (FuncInitAPI)DaoFindSymbol( handle, "DaoInit" );
		if( pfunc == NULL ) return 0;
		(*pfunc)( & __dao );
		return 1;
	}
	handle = DaoLoadLibrary( "./dao" DAO_DLL_SUFFIX );
	if( daodir == NULL && bin && bin[0] == '/' && strstr( bin, "/bin/" ) ==NULL ){
		/* set the DAO_DIR environment variable if the command is
		 * started from a customized directory.
		 * for example, in web scripts, it may have:
		 *
		 * #!/home/some_user/some_dir/dao
		 *
		 * some_scripts();
		 *
		 * in such cases, setting DAO_DIR will allow the interpreter to
		 * find the library and module locations properly. */
		k = strlen( bin );
		if( strcmp( bin + k - 4, "/dao" ) ==0 ){
			daodir = (char*) malloc( k + 10 );
			strncpy( daodir, "DAO_DIR=", 9 );
			strncat( daodir, bin, k - 4 );
			putenv( daodir );
			daodir += 8;
		}
	}
	if( handle == NULL && daodir ){
		k = strlen( daodir );
		daolib = (char*) malloc( k + 10 );
		strncpy( daolib, daodir, k + 1 );
		if( daolib[k] == '/' ) daolib[k] = 0;
		strcat( daolib, "/dao" DAO_DLL_SUFFIX );
		handle = DaoLoadLibrary( daolib );
		free( daolib );
	}
	if( handle == NULL ) handle = DaoLoadLibrary( "~/dao/dao" DAO_DLL_SUFFIX );
	if( handle == NULL ) handle = DaoLoadLibrary( DAO_LIB_DEFAULT );
	if( handle == NULL ){
		/*
		   printf( "%s\n", dlerror() );
		 */
		printf( "loading Dao library file failed\n" );
		return 0;
	}

	pfunc = (FuncInitAPI)DaoFindSymbol( handle, "DaoInitAPI" );
	if( pfunc == NULL ) return 0;
	(*pfunc)( & __dao );
	return 1;
}
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif
