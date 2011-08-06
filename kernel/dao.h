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

#ifndef __DAO_H__
#define __DAO_H__

#include<wctype.h>
#include<wchar.h>
#include<stdio.h>
#include<stdlib.h>

#define DAO_VERSION "1.2"
#define DAO_H_VERSION 20110806

#if defined(MAC_OSX) && ! defined(UNIX)
#define UNIX
#endif /* MAC_OSX */

#ifdef WIN32

#if defined( _MSC_VER ) && defined( _M_X64 ) || defined( __x86_64__ )
#define DAO_USE_INT64
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

#ifdef __x86_64__
#define DAO_USE_INT64
#endif

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

#ifdef __x86_64__
#define DAO_USE_INT64
#endif

#endif /* WIN32 */

/* define an integer type with size equal to the size of pointers
 * under both 32-bits and 64-bits systems. */
#ifdef DAO_USE_INT64
typedef long long           dint;
typedef unsigned long long  uint_t;
#else
typedef long                dint;
typedef unsigned long       uint_t;
#endif /* defined() */

#ifdef __cplusplus
#define DAO_EXTC_OPEN extern "C"{
#define DAO_EXTC_CLOSE }
#else
#define DAO_EXTC_OPEN
#define DAO_EXTC_CLOSE
#endif

/* define module initializer: */
#define \
    DAO_INIT_MODULE \
DAO_EXTC_OPEN \
DAO_DLL int DaoH_Version = DAO_H_VERSION; \
DAO_DLL int DaoOnLoad( DaoVmSpace *vmSpace, DaoNameSpace *ns ); \
DAO_EXTC_CLOSE


#define DAO_MAX_CDATA_SUPER 10


#ifdef __cplusplus
extern "C"{
#endif

extern int daoProxyPort;

enum DaoTypes
{
	DAO_NULL  = 0,
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
	DAO_TUPLE ,
	DAO_STREAM ,
	DAO_OBJECT ,
	DAO_CDATA  ,
	DAO_CLASS  ,
	DAO_CTYPE  ,
	DAO_INTERFACE ,
	DAO_FUNCTREE ,
	DAO_ROUTINE   ,
	DAO_FUNCTION  ,
	DAO_CONTEXT   ,
	DAO_VMPROCESS ,
	DAO_NAMESPACE ,
	DAO_VMSPACE   ,
	DAO_MUTEX ,
	DAO_CONDVAR ,
	DAO_SEMA ,
	DAO_THREAD ,
	DAO_TYPE ,
	END_CORE_TYPES
};
enum DaoVmProcessStatus
{
	DAO_VMPROC_FINISHED ,  /* finished normally */
	DAO_VMPROC_ABORTED ,   /* execution aborted */
	DAO_VMPROC_SUSPENDED , /* suspended, by coroutine.yield() */
	DAO_VMPROC_RUNNING ,   /* currently running */
	DAO_VMPROC_STACKED     /* new context is pushed onto the stack of the process */
};
enum DaoNameSpaceOption
{
	/* automatically make variable declared outside {} global, for interactive mode */
	DAO_NS_AUTO_GLOBAL = (1<<0)
};
/* Execution options, combinable by | */
enum DaoExecOption
{
	DAO_EXEC_HELP      = (1<<0), /* -h, --help:       print this help information; */
	DAO_EXEC_VINFO     = (1<<1), /* -v, --version:    print version information; */
	DAO_EXEC_DEBUG     = (1<<2), /* -d, --debug:      run in debug mode; */
	DAO_EXEC_SAFE      = (1<<3), /* -s, --safe:       run in safe mode; */
	DAO_EXEC_INTERUN   = (1<<4), /* -i, --interactive: run in interactive mode; */
	DAO_EXEC_LIST_BC   = (1<<5), /* -l, --list-bc:    print compiled bytecodes; */
	DAO_EXEC_COMP_BC   = (1<<6), /* -c, --compile:    compile to bytecodes;(TODO) */
	DAO_EXEC_INCR_COMP = (1<<7), /* -n, --incr-comp:  incremental compiling; */
	DAO_EXEC_JIT       = (1<<8), /* -J, --jit:        enable JIT compiling; */
	DAO_EXEC_NO_TC     = (1<<9), /* -T, --no-typed-code:   no typed code; */

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
	DAO_ERROR_FILE ,

	DAO_WARNING_SYNTAX ,
	DAO_WARNING_VALUE ,

	ENDOF_BASIC_EXCEPT
};

typedef unsigned char uchar_t;

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

typedef union  DaoValue        DaoValue;
typedef struct DaoNull         DaoNull;
typedef struct DaoInteger      DaoInteger;
typedef struct DaoFloat        DaoFloat;
typedef struct DaoDouble       DaoDouble;
typedef struct DaoComplex      DaoComplex;
typedef struct DaoLong         DaoLong;
typedef struct DaoString       DaoString;
typedef struct DaoEnum         DaoEnum;
typedef struct DaoArray        DaoArray;
typedef struct DaoList         DaoList;
typedef struct DaoMap          DaoMap;
typedef struct DaoTuple        DaoTuple;
typedef struct DaoFunctree  DaoFunctree;
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
typedef struct DaoNameValue    DaoNameValue;
typedef struct DaoMutex        DaoMutex;
typedef struct DaoCondVar      DaoCondVar;
typedef struct DaoSema         DaoSema;
typedef struct DaoThread       DaoThread;
typedef struct DaoType         DaoType;

/* Complex type: */
typedef struct complex16 { double real, imag; } complex16;

/* Dummy type for functions, casted from DaoRoutine, DaoFunction or DaoFunctree: */
typedef struct DaoMethod { uchar_t type;  } DaoMethod;

typedef void (*CallbackOnString)( const char *str );
typedef void (*FuncDaoInit)();
typedef void  (*DThreadTask)( void *arg );
typedef void* (*FuncPtrCast)( void* );
typedef void  (*FuncPtrDel)( void* );
typedef int   (*FuncPtrTest)( void* );
typedef void  (*DaoFuncPtr) ( DaoContext *context, DaoValue *params[], int npar );

typedef struct DaoNumItem   DaoNumItem;
typedef struct DaoFuncItem  DaoFuncItem;

struct DaoNumItem
{
	const char *name;
	int         type;  /* DAO_INTEGER, DAO_FLOAT or DAO_DOUBLE */
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

	/* function(s) to cast a C/C++ type to one of its parent type:
	 * usually they should be set to NULL, but for wrapping C++ class
	 * with virtual methods, it is necessary to provide casting function(s)
	 * in the following form:
	 *   void* cast_Sub_to_Base( void *data ) { return (Base*)(Sub*)data; } */
	FuncPtrCast    casts[ DAO_MAX_CDATA_SUPER ];

	/* function to free data:
	 * only for DaoCData created by DaoValue_NewCData() or DaoCData_New() */
	void   (*Delete)( void *data );
	/* test if the data is deletable by Dao: called by gc before deletion. */
	int    (*DelTest)( void *data );
};

/* Callback data: freed when "callback" or "userdata" is collected by GC. */
struct DaoCallbackData
{
	DaoMethod  *callback;
	DaoValue   *userdata;
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


/* See the end of this file for some descriptions. */

DAO_DLL DaoVmSpace* DaoInit();
DAO_DLL void DaoQuit();

DAO_DLL int DaoValue_Type( DaoValue *self );

DAO_DLL DaoValue* DaoValue_NewInteger( dint v );
DAO_DLL DaoValue* DaoValue_NewFloat( float v );
DAO_DLL DaoValue* DaoValue_NewDouble( double v );
DAO_DLL DaoValue* DaoValue_NewMBString( const char *s, int n );
DAO_DLL DaoValue* DaoValue_NewWCString( const wchar_t *s, int n );
DAO_DLL DaoValue* DaoValue_NewVectorB( char *s, int n ); 
DAO_DLL DaoValue* DaoValue_NewVectorUB( unsigned char *s, int n ); 
DAO_DLL DaoValue* DaoValue_NewVectorS( short *s, int n ); 
DAO_DLL DaoValue* DaoValue_NewVectorUS( unsigned short *s, int n ); 
DAO_DLL DaoValue* DaoValue_NewVectorI( int *s, int n ); 
DAO_DLL DaoValue* DaoValue_NewVectorUI( unsigned int *s, int n ); 
DAO_DLL DaoValue* DaoValue_NewVectorF( float *s, int n ); 
DAO_DLL DaoValue* DaoValue_NewVectorD( double *s, int n ); 
DAO_DLL DaoValue* DaoValue_NewMatrixB( signed char **s, int n, int m );
DAO_DLL DaoValue* DaoValue_NewMatrixUB( unsigned char **s, int n, int m );
DAO_DLL DaoValue* DaoValue_NewMatrixS( short **s, int n, int m );
DAO_DLL DaoValue* DaoValue_NewMatrixUS( unsigned short **s, int n, int m );
DAO_DLL DaoValue* DaoValue_NewMatrixI( int **s, int n, int m );
DAO_DLL DaoValue* DaoValue_NewMatrixUI( unsigned int **s, int n, int m );
DAO_DLL DaoValue* DaoValue_NewMatrixF( float **s, int n, int m );
DAO_DLL DaoValue* DaoValue_NewMatrixD( double **s, int n, int m );
DAO_DLL DaoValue* DaoValue_NewBuffer( void *s, int n );
DAO_DLL DaoValue* DaoValue_NewStream( FILE *f );
DAO_DLL DaoValue* DaoValue_NewCData( DaoTypeBase *typer, void *data );
DAO_DLL DaoValue* DaoValue_WrapCData( DaoTypeBase *typer, void *data );

DAO_DLL DaoInteger*  DaoValue_CastInteger( DaoValue *self );
DAO_DLL DaoFloat*    DaoValue_CastFloat( DaoValue *self );
DAO_DLL DaoDouble*   DaoValue_CastDouble( DaoValue *self );
DAO_DLL DaoComplex*  DaoValue_CastComplex( DaoValue *self );
DAO_DLL DaoLong*     DaoValue_CastLong( DaoValue *self );
DAO_DLL DaoString*   DaoValue_CastString( DaoValue *self );
DAO_DLL DaoEnum*     DaoValue_CastEnum( DaoValue *self );
DAO_DLL DaoArray*    DaoValue_CastArray( DaoValue *self );
DAO_DLL DaoList*     DaoValue_CastList( DaoValue *self );
DAO_DLL DaoMap*      DaoValue_CastMap( DaoValue *self );
DAO_DLL DaoTuple*    DaoValue_CastTuple( DaoValue *self );
DAO_DLL DaoStream*   DaoValue_CastStream( DaoValue *self );
DAO_DLL DaoObject*   DaoValue_CastObject( DaoValue *self );
DAO_DLL DaoCData*    DaoValue_CastCData( DaoValue *self );
DAO_DLL DaoClass*    DaoValue_CastClass( DaoValue *self );
DAO_DLL DaoCtype*    DaoValue_CastCtype( DaoValue *self );

DAO_DLL DaoInterface*    DaoValue_CastInterface( DaoValue *self );
DAO_DLL DaoFunctree*  DaoValue_CastMetaRoutine( DaoValue *self );
DAO_DLL DaoRoutine*      DaoValue_CastRoutine( DaoValue *self );
DAO_DLL DaoFunction*     DaoValue_CastFunction( DaoValue *self );
DAO_DLL DaoContext*      DaoValue_CastContext( DaoValue *self );
DAO_DLL DaoVmProcess*    DaoValue_CastVmProcess( DaoValue *self );
DAO_DLL DaoNameSpace*    DaoValue_CastNameSpace( DaoValue *self );
DAO_DLL DaoNamedValue*   DaoValue_CastNamedValue( DaoValue *self );
DAO_DLL DaoType*         DaoValue_CastType( DaoValue *self );

DAO_DLL char* DaoValue_GetMBString( DaoValue *self );
DAO_DLL wchar_t* DaoValue_GetWCString( DaoValue *self );
DAO_DLL void*  DaoValue_CastCData( DaoValue *self, DaoTypeBase *totyper );
DAO_DLL void*  DaoValue_GetCData( DaoValue *self );
DAO_DLL void** DaoValue_GetCData2( DaoValue *self );
DAO_DLL void DaoValue_Copy( DaoValue *self, DaoValue from );
DAO_DLL void DaoValue_Clear( DaoValue *v );
DAO_DLL void DaoValue_ClearAll( DaoValue *v, int n );

DAO_DLL DString* DString_New( int mbs );
DAO_DLL DString* DString_Copy( DString *self );
DAO_DLL void DString_Delete( DString *self );
DAO_DLL size_t DString_Size( DString *self );
DAO_DLL void DString_Clear( DString *self );
DAO_DLL void DString_Resize( DString *self, size_t size );
DAO_DLL int  DString_IsMBS( DString *self );
DAO_DLL void DString_SetMBS( DString *self, const char *chs );
DAO_DLL void DString_SetWCS( DString *self, const wchar_t *chs );
DAO_DLL void DString_SetDataMBS( DString *self, const char *data, size_t n );
DAO_DLL void DString_SetDataWCS( DString *self, const wchar_t *data, size_t n );
DAO_DLL void DString_ToWCS( DString *self );
DAO_DLL void DString_ToMBS( DString *self );
DAO_DLL char* DString_GetMBS( DString *self );
DAO_DLL wchar_t* DString_GetWCS( DString *self );
DAO_DLL void DString_Erase( DString *self, size_t start, size_t n );
DAO_DLL void DString_Insert( DString *self, DString *s, size_t i, size_t m, size_t n );
DAO_DLL void DString_InsertChar( DString *self, const char ch, size_t at );
DAO_DLL void DString_InsertMBS( DString *self, const char *s, size_t i, size_t m, size_t n );
DAO_DLL void DString_InsertWCS( DString *self, const wchar_t *s, size_t i, size_t m, size_t n );
DAO_DLL void DString_Append( DString *self, DString *chs );
DAO_DLL void DString_AppendChar( DString *self, const char ch );
DAO_DLL void DString_AppendWChar( DString *self, const wchar_t ch );
DAO_DLL void DString_AppendMBS( DString *self, const char *chs );
DAO_DLL void DString_AppendWCS( DString *self, const wchar_t *chs );
DAO_DLL void DString_AppendDataMBS( DString *self, const char *data, size_t count );
DAO_DLL void DString_AppendDataWCS( DString *self, const wchar_t *data,size_t count );
DAO_DLL void DString_SubString( DString *self, DString *sub, size_t from, size_t count );
DAO_DLL size_t DString_Find( DString *self, DString *chs, size_t start );
DAO_DLL size_t DString_FindMBS( DString *self, const char *ch, size_t start );
DAO_DLL size_t DString_FindChar( DString *self, char ch, size_t start );
DAO_DLL size_t DString_FindWChar( DString *self, wchar_t ch, size_t start );
DAO_DLL size_t DString_RFind( DString *self, DString *chs, size_t start );
DAO_DLL size_t DString_RFindMBS( DString *self, const char *ch, size_t start );
DAO_DLL size_t DString_RFindChar( DString *self, char ch, size_t start );
DAO_DLL void DString_Assign( DString *left, DString *right );
DAO_DLL int  DString_Compare( DString *left, DString *right );

DAO_DLL DaoInteger* DaoInteger_New( dint value );
DAO_DLL dint        DaoInteger_Get( DaoInteger *self );
DAO_DLL void        DaoInteger_Set( DaoInteger *self, dint value );

DAO_DLL DaoFloat* DaoFloat_New( float value );
DAO_DLL float     DaoFloat_Get( DaoFloat *self );
DAO_DLL void      DaoFloat_Set( DaoFloat *self, float value );

DAO_DLL DaoDouble* DaoDouble_New( double value );
DAO_DLL double     DaoDouble_Get( DaoDouble *self );
DAO_DLL void       DaoDouble_Set( DaoDouble *self, double value );

DAO_DLL DaoComplex* DaoComplex_New( complex16 value );
DAO_DLL complex16   DaoComplex_Get( DaoComplex *self );
DAO_DLL void        DaoComplex_Set( DaoComplex *self, complex16 value );

DAO_DLL DaoLong*  DaoLong_New();
//DLong*   DaoLong_Get( DaoLong *self );
//void        DaoLong_Set( DaoLong *self, DLong *value );

DAO_DLL DaoString*  DaoString_New( int mbs );
DAO_DLL DaoString*  DaoString_NewMBS( const char *mbs );
DAO_DLL DaoString*  DaoString_NewWCS( const wchar_t *wcs );
DAO_DLL DaoString*  DaoString_NewBytes( const char *bytes, int n );

DAO_DLL DString*       DaoString_Get( DaoString *self );
DAO_DLL const char*    DaoString_GetMBS( DaoString *self );
DAO_DLL const wchar_t* DaoString_GetWCS( DaoString *self );

DAO_DLL void  DaoString_Set( DaoString *self, DString *str );
DAO_DLL void  DaoString_SetMBS( DaoString *self, const char *mbs );
DAO_DLL void  DaoString_SetWCS( DaoString *self, const wchar_t *wcs );
DAO_DLL void  DaoString_SetBytes( DaoString *self, const char *bytes, int n );

DAO_DLL DaoEnum* DaoEnum_New();
//float    DaoEnum_Get( DaoEnum *self );
//void     DaoEnum_Set( DaoEnum *self, float value );

DAO_DLL DaoList* DaoList_New();
DAO_DLL int  DaoList_Size( DaoList *self );
DAO_DLL DaoValue* DaoList_Front( DaoList *self );
DAO_DLL DaoValue* DaoList_Back( DaoList *self );
DAO_DLL DaoValue* DaoList_GetItem( DaoList *self, int pos );
DAO_DLL int DaoList_SetItem( DaoList *self, DaoValue *item, int pos );
DAO_DLL int DaoList_Insert( DaoList *self, DaoValue *item, int pos );
DAO_DLL int DaoList_PushFront( DaoList *self, DaoValue *item );
DAO_DLL int DaoList_PushBack( DaoList *self, DaoValue *item );
DAO_DLL void DaoList_PopFront( DaoList *self );
DAO_DLL void DaoList_PopBack( DaoList *self );
DAO_DLL void DaoList_Erase( DaoList *self, int pos );
DAO_DLL void DaoList_Clear( DaoList *self );

DAO_DLL DaoMap* DaoMap_New( int hashing );
DAO_DLL int  DaoMap_Size( DaoMap *self );
DAO_DLL int  DaoMap_Insert( DaoMap *self, DaoValue *key, DaoValue *value );
DAO_DLL int  DaoMap_InsertMBS( DaoMap *self, const char *key, DaoValue *value );
DAO_DLL int  DaoMap_InsertWCS( DaoMap *self, const wchar_t *key, DaoValue *value );
DAO_DLL void DaoMap_Erase( DaoMap *self, DaoValue *key );
DAO_DLL void DaoMap_EraseMBS( DaoMap *self, const char *key );
DAO_DLL void DaoMap_EraseWCS( DaoMap *self, const wchar_t *key );
DAO_DLL void DaoMap_Clear( DaoMap *self );
DAO_DLL DaoValue* DaoMap_GetValue( DaoMap *self, DaoValue *key  );
DAO_DLL DaoValue* DaoMap_GetValueMBS( DaoMap *self, const char *key  );
DAO_DLL DaoValue* DaoMap_GetValueWCS( DaoMap *self, const wchar_t *key  );

DAO_DLL DNode* DaoMap_First( DaoMap *self );
DAO_DLL DNode* DaoMap_Next( DaoMap *self, DNode *iter );
DAO_DLL DaoValue* DNode_Key( DNode *self );
DAO_DLL DaoValue* DNode_Value( DNode *self );

DAO_DLL DaoTuple* DaoTuple_New( int size );
DAO_DLL int  DaoTuple_Size( DaoTuple *self );
DAO_DLL void   DaoTuple_SetItem( DaoTuple *self, DaoValue *it, int pos );
DAO_DLL DaoValue* DaoTuple_GetItem( DaoTuple *self, int pos );

DAO_DLL DaoArray* DaoArray_New( int numtype );
DAO_DLL int  DaoArray_NumType( DaoArray *self );
DAO_DLL void DaoArray_SetNumType( DaoArray *self, short numtype );
DAO_DLL int  DaoArray_Size( DaoArray *self );
DAO_DLL int  DaoArray_DimCount( DaoArray *self );
DAO_DLL int  DaoArray_SizeOfDim( DaoArray *self, int d );
DAO_DLL void DaoArray_GetShape( DaoArray *self, size_t *dims );
DAO_DLL int  DaoArray_HasShape( DaoArray *self, size_t *dims, int D );
DAO_DLL int  DaoArray_GetFlatIndex( DaoArray *self, size_t *indexes );
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

DAO_DLL DaoMethod* DaoMethod_Resolve( DaoMethod *self, DaoValue *o, DaoValue *p[], int n );

DAO_DLL DaoValue* DaoObject_GetField( DaoObject *self, const char *name );
DAO_DLL DaoMethod* DaoObject_GetMethod( DaoObject *self, const char *name );
DAO_DLL DaoCData* DaoObject_MapCData( DaoObject *self, DaoTypeBase *typer );

DAO_DLL DaoStream* DaoStream_New();
DAO_DLL void DaoStream_SetFile( DaoStream *self, FILE *fd );
DAO_DLL FILE* DaoStream_GetFile( DaoStream *self );

DAO_DLL DaoCData* DaoCData_New( DaoTypeBase *typer, void *data );
DAO_DLL DaoCData* DaoCData_Wrap( DaoTypeBase *typer, void *data );
DAO_DLL int    DaoCData_IsType( DaoCData *self, DaoTypeBase *typer );
DAO_DLL int    DaoCData_OwnData( DaoCData *self );
DAO_DLL void   DaoCData_SetExtReference( DaoCData *self, int bl );
DAO_DLL void   DaoCData_SetData( DaoCData *self, void *data );
DAO_DLL void   DaoCData_SetBuffer( DaoCData *self, void *data, size_t size );
DAO_DLL void   DaoCData_SetArray( DaoCData *self, void *data, size_t size, int itsize );
DAO_DLL void*  DaoCData_CastData( DaoCData *self, DaoTypeBase *totyper );
DAO_DLL void*  DaoCData_GetData( DaoCData *self );
DAO_DLL void*  DaoCData_GetBuffer( DaoCData *self );
DAO_DLL void** DaoCData_GetData2( DaoCData *self );
DAO_DLL DaoObject* DaoCData_GetObject( DaoCData *self );
DAO_DLL DaoTypeBase* DaoCData_GetTyper( DaoCData *self );

DAO_DLL DaoRegex* DaoRegex_New( DString *pattern );
DAO_DLL int DaoRegex_Match( DaoRegex *self, DString *src, size_t *start, size_t *end );
DAO_DLL int DaoRegex_SubMatch( DaoRegex *self, int gid, size_t *start, size_t *end );
DAO_DLL int DaoRegex_Change( DaoRegex *self, DString *src, DString *target, int index );

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
DAO_DLL DaoTuple*  DaoContext_PutTuple( DaoContext *self );
DAO_DLL DaoStream* DaoContext_PutFile( DaoContext *self, FILE *file );
DAO_DLL DaoCData*  DaoContext_PutCData( DaoContext *self, void *data, DaoTypeBase *typer );
DAO_DLL DaoCData*  DaoContext_PutCPointer( DaoContext *self, void *data, int size );
DAO_DLL DaoCData*  DaoContext_WrapCData( DaoContext *self, void *data, DaoTypeBase *typer );
DAO_DLL DaoCData*  DaoContext_CopyCData( DaoContext *self, void *d, int n, DaoTypeBase *t );
DAO_DLL DaoValue*  DaoContext_PutValue( DaoContext *self, DaoValue *value );

DAO_DLL DaoVmProcess* DaoContext_CurrentProcess( DaoContext *self );
DAO_DLL void DaoContext_RaiseException( DaoContext *self, int type, const char *value );

DAO_DLL DaoVmProcess* DaoVmProcess_New( DaoVmSpace *vms );
DAO_DLL int DaoVmProcess_Compile( DaoVmProcess *self, DaoNameSpace *ns, DString *src, int rpl );
DAO_DLL int DaoVmProcess_Eval( DaoVmProcess *self, DaoNameSpace *ns, DString *src, int rpl );
DAO_DLL int DaoVmProcess_Call( DaoVmProcess *s, DaoMethod *f, DaoValue *o, DaoValue *p[], int n );
DAO_DLL void  DaoVmProcess_Stop( DaoVmProcess *self );
DAO_DLL DaoValue* DaoVmProcess_GetReturned( DaoVmProcess *self );
DAO_DLL DaoRegex* DaoVmProcess_MakeRegex( DaoVmProcess *self, DString *patt, int mbs );

DAO_DLL DaoNameSpace* DaoNameSpace_New( DaoVmSpace *vms, const char *name );
DAO_DLL DaoNameSpace* DaoNameSpace_GetNameSpace( DaoNameSpace *self, const char *name );
DAO_DLL int  DaoNameSpace_AddParent( DaoNameSpace *self, DaoNameSpace *parent );
DAO_DLL void DaoNameSpace_AddConstNumbers( DaoNameSpace *self, DaoNumItem *items );
DAO_DLL void DaoNameSpace_AddConstValue( DaoNameSpace *self, const char *name, DaoValue *data );
DAO_DLL void DaoNameSpace_AddValue( DaoNameSpace *self, const char *name, DaoValue *d, const char *type);
DAO_DLL DaoValue* DaoNameSpace_FindData( DaoNameSpace *self, const char *name );
DAO_DLL DaoType* DaoNameSpace_TypeDefine( DaoNameSpace *self, const char *old, const char *type );
DAO_DLL DaoType* DaoNameSpace_WrapType( DaoNameSpace *self, DaoTypeBase *typer );
DAO_DLL DaoFunction* DaoNameSpace_WrapFunction( DaoNameSpace *self, DaoFuncPtr fp, const char *proto );
DAO_DLL int DaoNameSpace_TypeDefines( DaoNameSpace *self, const char *alias[] );
DAO_DLL int DaoNameSpace_WrapTypes( DaoNameSpace *self, DaoTypeBase *typer[] );
DAO_DLL int DaoNameSpace_WrapFunctions( DaoNameSpace *self, DaoFuncItem *items );
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
DAO_DLL DaoVmProcess* DaoVmSpace_AcquireProcess( DaoVmSpace *self );
DAO_DLL void DaoVmSpace_ReleaseProcess( DaoVmSpace *self, DaoVmProcess *proc );

DAO_DLL void DaoVmSpace_SetUserHandler( DaoVmSpace *self, DaoUserHandler *handler );
DAO_DLL void DaoVmSpace_ReadLine( DaoVmSpace *self, ReadLine fptr );
DAO_DLL void DaoVmSpace_AddHistory( DaoVmSpace *self, AddHistory fptr );

DAO_DLL void DaoVmSpace_AddVirtualFile( DaoVmSpace *self, const char *f, const char *s );
DAO_DLL void DaoVmSpace_SetPath( DaoVmSpace *self, const char *path );
DAO_DLL void DaoVmSpace_AddPath( DaoVmSpace *self, const char *path );
DAO_DLL void DaoVmSpace_DelPath( DaoVmSpace *self, const char *path );
DAO_DLL void  DaoVmSpace_Stop( DaoVmSpace *self, int bl );

DAO_DLL void DaoGC_IncRC( DaoValue *p );
DAO_DLL void DaoGC_DecRC( DaoValue *p );

DAO_DLL DaoType* DaoType_GetFromTypeStructure( DaoTypeBase *typer );

DAO_DLL DaoCallbackData* DaoCallbackData_New( DaoMethod *callback, DaoValue *userdata );

#endif

#ifdef __cplusplus
}
#endif

#endif

/*

DaoVmSpace* DaoInit();
Initialize the Dao library

void DaoQuit();
Wait for unfinished computation and then quit

int DaoValue_Type( DaoValue *self );

Create basic Dao values
DaoValue* DaoValue_NewInteger( dint v );
DaoValue* DaoValue_NewFloat( float v );
DaoValue* DaoValue_NewDouble( double v );

Values created by the following DaoValue_NewXyz() and DaoValue_WrapXyz(),
must be cleared by DaoValue_Clear() to avoid memory leaking:

Create Multi-Byte String (MBS): 
DaoValue* DaoValue_NewMBString( const char *s, int n );
Create Wide Character String (WCS): 
DaoValue* DaoValue_NewWCString( const wchar_t *s, int n );

 Create [1 x n] vector (DaoArray): 
DaoValue* DaoValue_NewVectorB( char *s, int n );  byte 
DaoValue* DaoValue_NewVectorUB( unsigned char *s, int n );  unsigned byte 
DaoValue* DaoValue_NewVectorS( short *s, int n );  short 
DaoValue* DaoValue_NewVectorUS( unsigned short *s, int n );  unsigned short 
DaoValue* DaoValue_NewVectorI( int *s, int n );  int 
DaoValue* DaoValue_NewVectorUI( unsigned int *s, int n );  unsigned int 
DaoValue* DaoValue_NewVectorF( float *s, int n );  float 
DaoValue* DaoValue_NewVectorD( double *s, int n );  double 

 Create [n x m] matrix (DaoArray): 
DaoValue* DaoValue_NewMatrixB( signed char **s, int n, int m );
DaoValue* DaoValue_NewMatrixUB( unsigned char **s, int n, int m );
DaoValue* DaoValue_NewMatrixS( short **s, int n, int m );
DaoValue* DaoValue_NewMatrixUS( unsigned short **s, int n, int m );
DaoValue* DaoValue_NewMatrixI( int **s, int n, int m );
DaoValue* DaoValue_NewMatrixUI( unsigned int **s, int n, int m );
DaoValue* DaoValue_NewMatrixF( float **s, int n, int m );
DaoValue* DaoValue_NewMatrixD( double **s, int n, int m );

 Create "n" bytes buffer (DaoCData): 
DaoValue* DaoValue_NewBuffer( void *s, int n );
 Create io stream object (DaoStream): 
DaoValue* DaoValue_NewStream( FILE *f );

 Create DaoCData object wrapping the opaque data with given type: 
 data will be deleted with the DaoCData structure created by this function 
DaoValue* DaoValue_NewCData( DaoTypeBase *typer, void *data );

 Create DaoCData object wrapping the opaque data with given type: 
 data will NOT be deleted with the DaoCData structure created by this function 
DaoValue* DaoValue_WrapCData( DaoTypeBase *typer, void *data );

DaoInteger*      DaoValue_CastInteger( DaoValue *self );
DaoFloat*        DaoValue_CastFloat( DaoValue *self );
DaoDouble*       DaoValue_CastDouble( DaoValue *self );
DaoComplex*      DaoValue_CastComplex( DaoValue *self );
DaoLong*         DaoValue_CastLong( DaoValue *self );
DaoString*       DaoValue_CastString( DaoValue *self );
DaoEnum*         DaoValue_CastEnum( DaoValue *self );
DaoArray*        DaoValue_CastArray( DaoValue *self );
DaoList*         DaoValue_CastList( DaoValue *self );
DaoMap*          DaoValue_CastMap( DaoValue *self );
DaoTuple*        DaoValue_CastTuple( DaoValue *self );
DaoStream*       DaoValue_CastStream( DaoValue *self );
DaoObject*       DaoValue_CastObject( DaoValue *self );
DaoCData*        DaoValue_CastCData( DaoValue *self );
DaoClass*        DaoValue_CastClass( DaoValue *self );
DaoCtype*        DaoValue_CastCtype( DaoValue *self );
DaoInterface*    DaoValue_CastInterface( DaoValue *self );
DaoFunctree*  DaoValue_CastMetaRoutine( DaoValue *self );
DaoRoutine*      DaoValue_CastRoutine( DaoValue *self );
DaoFunction*     DaoValue_CastFunction( DaoValue *self );
DaoContext*      DaoValue_CastContext( DaoValue *self );
DaoVmProcess*    DaoValue_CastVmProcess( DaoValue *self );
DaoNameSpace*    DaoValue_CastNameSpace( DaoValue *self );
DaoNamedValue*   DaoValue_CastNamedValue( DaoValue *self );
DaoType*         DaoValue_CastType( DaoValue *self );

 Get Multi-Byte String (MBS): 
 Return NULL if it is not a string; 
 If it is a Wide Character String (WCS), convert to MBS first; 
char* DaoValue_GetMBString( DaoValue *self );
wchar_t* DaoValue_GetWCString( DaoValue *self );

 If "self" stores a c data object with type matching to "totyper",
 * return a pointer to the opaque object that has been properly casted using
 * the "casts" methods of typer structures (DaoTypeBase).
 * Otherwise return NULL. 
void*  DaoValue_CastCData( DaoValue *self, DaoTypeBase *totyper );
 Get the pointer to the opaque data: 
void*  DaoValue_GetCData( DaoValue *self );
void** DaoValue_GetCData2( DaoValue *self );

 Copy value from "from" to "self": 
void DaoValue_Copy( DaoValue *self, DaoValue from );

 Clear the data in the value: 
void DaoValue_Clear( DaoValue *v );
 Clear the data in the value array: 
void DaoValue_ClearAll( DaoValue *v, int n );

 --------------------------------
 * String Manipulation:
 * --------------------------------
DString* DString_New( int mbs );
void DString_Delete( DString *self );

size_t DString_Size( DString *self );
void DString_Clear( DString *self );
void DString_Resize( DString *self, size_t size );

int DString_IsMBS( DString *self );
void DString_SetMBS( DString *self, const char *chs );
void DString_SetWCS( DString *self, const wchar_t *chs );
void DString_SetDataMBS( DString *self, const char *data, size_t n );
void DString_SetDataWCS( DString *self, const wchar_t *data, size_t n );
void DString_ToWCS( DString *self );
void DString_ToMBS( DString *self );
char* DString_GetMBS( DString *self );
wchar_t* DString_GetWCS( DString *self );

 Erase "n" characters starting from "start": 
void DString_Erase( DString *self, size_t start, size_t n );

 Insert "s" at "i" with "m" characters removed and "n" copied,
   if "n" is zero, copy all characters of "s" 
void DString_Insert( DString *self, DString *s, size_t i, size_t m, size_t n );
void DString_InsertChar( DString *self, const char ch, size_t at );

 Insert "s" at "i" with "m" characters removed and "n" copied,
   if "n" is zero, consider "s" as null-terminated string. 
void DString_InsertMBS( DString *self, const char *s, size_t i, size_t m, size_t n );
void DString_InsertWCS( DString *self, const wchar_t *s, size_t i, size_t m, size_t n );

void DString_Append( DString *self, DString *chs );
void DString_AppendChar( DString *self, const char ch );
void DString_AppendWChar( DString *self, const wchar_t ch );
void DString_AppendMBS( DString *self, const char *chs );
void DString_AppendWCS( DString *self, const wchar_t *chs );
void DString_AppendDataMBS( DString *self, const char *data, size_t count );
void DString_AppendDataWCS( DString *self, const wchar_t *data,size_t count );
void DString_SubString( DString *self, DString *sub, size_t from, size_t count );

size_t DString_Find( DString *self, DString *chs, size_t start );
size_t DString_FindMBS( DString *self, const char *ch, size_t start );
size_t DString_FindChar( DString *self, char ch, size_t start );
size_t DString_FindWChar( DString *self, wchar_t ch, size_t start );
 Backward searching: 
size_t DString_RFind( DString *self, DString *chs, size_t start );
size_t DString_RFindMBS( DString *self, const char *ch, size_t start );
size_t DString_RFindChar( DString *self, char ch, size_t start );

DString* DString_Copy( DString *self );

 Assign/Copy "right" to "left" 
void DString_Assign( DString *left, DString *right );

 Compare and return -1, 0, or 1:
 * if "left" is found to be less than, equal to or greater than "right": 
int  DString_Compare( DString *left, DString *right );


DaoInteger* DaoInteger_New( dint value );
dint        DaoInteger_Get( DaoInteger *self );
void        DaoInteger_Set( DaoInteger *self, dint value );

DaoFloat* DaoFloat_New( float value );
float     DaoFloat_Get( DaoFloat *self );
void      DaoFloat_Set( DaoFloat *self, float value );

DaoDouble* DaoDouble_New( double value );
double     DaoDouble_Get( DaoDouble *self );
void       DaoDouble_Set( DaoDouble *self, double value );

DaoComplex* DaoComplex_New( complex16 value );
complex16   DaoComplex_Get( DaoComplex *self );
void        DaoComplex_Set( DaoComplex *self, complex16 value );

DaoLong*  DaoLong_New();
//DLong*   DaoLong_Get( DaoLong *self );
//void        DaoLong_Set( DaoLong *self, DLong *value );

DaoString*     DaoString_New( DString *str );
DaoString*     DaoString_NewMBS( const char *mbs );
DaoString*     DaoString_NewWCS( const wchar_t *wcs );
DaoString*     DaoString_NewBytes( const char *bytes, int n );
DString*       DaoString_Get( DaoString *self );
const char*    DaoString_GetMBS( DaoString *self );
const wchar_t* DaoString_GetWCS( DaoString *self );
void           DaoString_Set( DaoString *self, DString *str );
void           DaoString_SetMBS( DaoString *self, const char *mbs );
void           DaoString_SetWCS( DaoString *self, const wchar_t *wcs );
void           DaoString_SetBytes( DaoString *self, const char *bytes, int n );

DaoEnum* DaoEnum_New();
//float    DaoEnum_Get( DaoEnum *self );
//void     DaoEnum_Set( DaoEnum *self, float value );

DaoList* DaoList_New();
int  DaoList_Size( DaoList *self );
DValue DaoList_Front( DaoList *self );
DValue DaoList_Back( DaoList *self );
DValue DaoList_GetItem( DaoList *self, int pos );

 The following functions return 0 if seccessful, 1 if failed: 
int DaoList_SetItem( DaoList *self, DValue item, int pos );
int DaoList_Insert( DaoList *self, DValue item, int pos );
int DaoList_PushFront( DaoList *self, DValue item );
int DaoList_PushBack( DaoList *self, DValue item );
void DaoList_PopFront( DaoList *self );
void DaoList_PopBack( DaoList *self );
void DaoList_Erase( DaoList *self, int pos );
void DaoList_Clear( DaoList *self );

DaoMap* DaoMap_New( int hashing );
int  DaoMap_Size( DaoMap *self );
 return 0 if successful; return 1 if key not matching, 2 if value not matching: 
int  DaoMap_Insert( DaoMap *self, DValue key, DValue value );
int  DaoMap_InsertMBS( DaoMap *self, const char *key, DValue value );
int  DaoMap_InsertWCS( DaoMap *self, const wchar_t *key, DValue value );
void DaoMap_Erase( DaoMap *self, DValue key );
void DaoMap_EraseMBS( DaoMap *self, const char *key );
void DaoMap_EraseWCS( DaoMap *self, const wchar_t *key );
void DaoMap_Clear( DaoMap *self );
DValue DaoMap_GetValue( DaoMap *self, DValue key  );
DValue DaoMap_GetValueMBS( DaoMap *self, const char *key  );
DValue DaoMap_GetValueWCS( DaoMap *self, const wchar_t *key  );
DNode* DaoMap_First( DaoMap *self );
DNode* DaoMap_Next( DaoMap *self, DNode *iter );
DValue* DNode_Key( DNode *self );
DValue* DNode_Value( DNode *self );

DaoTuple* DaoTuple_New( int size );
int  DaoTuple_Size( DaoTuple *self );
void   DaoTuple_SetItem( DaoTuple *self, DValue it, int pos );
DValue DaoTuple_GetItem( DaoTuple *self, int pos );

 Create a numeric array with specified numeric type:
 * which can be DAO_INTEGER, DAO_FLOAT, DAO_DOUBLE or DAO_COMPLEX. 
DaoArray* DaoArray_New( int numtype );
int  DaoArray_NumType( DaoArray *self );
void DaoArray_SetNumType( DaoArray *self, short numtype );
 Get the count of elements in the array: 
int  DaoArray_Size( DaoArray *self );
 Get number of dimensions (2 for vector and matrix): 
int  DaoArray_DimCount( DaoArray *self );
 Get the size of the given dimension: 
int   DaoArray_SizeOfDim( DaoArray *self, int d );
 Get the shape as an array of sizes for each dimension: 
void  DaoArray_GetShape( DaoArray *self, size_t *dims );
 Check the array if it has the specified shape: 
int DaoArray_HasShape( DaoArray *self, size_t *dims, int D );

 Compute the raw/flat index from multiple indexes.
 * "indexes" is expected to contain the same number of indexes 
 * as the array's number of dimensions. 
int   DaoArray_GetFlatIndex( DaoArray *self, size_t *indexes );

 Resize to a vector with "N" elements: 
void  DaoArray_ResizeVector( DaoArray *self, int N );
 Resize to a array with specified shape: 
void  DaoArray_ResizeArray( DaoArray *self, size_t *dims, int D );
 Reshape to a array with specified shape:
 * return 0 if the array contains different number of elements
 * from the expected number of element in the target shape;
 * otherwise, reshaping is done and return 1. 
int DaoArray_Reshape( DaoArray *self, size_t *dims, int D );

 Get raw data as a vector, type conversion may be performed: 
signed char* DaoArray_ToByte( DaoArray *self );
short*  DaoArray_ToShort( DaoArray *self );
int*    DaoArray_ToInt( DaoArray *self );
float*  DaoArray_ToFloat( DaoArray *self );
double* DaoArray_ToDouble( DaoArray *self );
unsigned char*  DaoArray_ToUByte( DaoArray *self );
unsigned short* DaoArray_ToUShort( DaoArray *self );
unsigned int*   DaoArray_ToUInt( DaoArray *self );

signed char** DaoArray_GetMatrixB( DaoArray *self, int row );
short**  DaoArray_GetMatrixS( DaoArray *self, int row );
int**    DaoArray_GetMatrixI( DaoArray *self, int row );
float**  DaoArray_GetMatrixF( DaoArray *self, int row );
double** DaoArray_GetMatrixD( DaoArray *self, int row );

 Re-interpret the raw data as bytes, and convert them to
 * the current numeric type of the array: 
void  DaoArray_FromByte( DaoArray *self );  as bytes 
void  DaoArray_FromShort( DaoArray *self );  as shorts 
void  DaoArray_FromInt( DaoArray *self );  as ints 
void  DaoArray_FromFloat( DaoArray *self );  as floats 
void  DaoArray_FromDouble( DaoArray *self );  as doubles 
void  DaoArray_FromUByte( DaoArray *self );  as unsigned bytes 
void  DaoArray_FromUShort( DaoArray *self );  as unsigned shorts 
void  DaoArray_FromUInt( DaoArray *self );  as unsigned ints 

 Set data from vector or matrix: 
void  DaoArray_SetVectorB( DaoArray *self, char* vec, int N );
void  DaoArray_SetVectorS( DaoArray *self, short* vec, int N );
void  DaoArray_SetVectorI( DaoArray *self, int* vec, int N );
void  DaoArray_SetVectorF( DaoArray *self, float* vec, int N );
void  DaoArray_SetVectorD( DaoArray *self, double* vec, int N );
void  DaoArray_SetMatrixB( DaoArray *self, signed char **mat, int row, int col );
void  DaoArray_SetMatrixS( DaoArray *self, short **mat, int row, int col );
void  DaoArray_SetMatrixI( DaoArray *self, int **mat, int row, int col );
void  DaoArray_SetMatrixF( DaoArray *self, float **mat, int row, int col );
void  DaoArray_SetMatrixD( DaoArray *self, double **mat, int row, int col );
void  DaoArray_SetVectorUB( DaoArray *self, unsigned char* vec, int N );
void  DaoArray_SetVectorUS( DaoArray *self, unsigned short* vec, int N );
void  DaoArray_SetVectorUI( DaoArray *self, unsigned int* vec, int N );

void* DaoArray_GetBuffer( DaoArray *self );
void DaoArray_SetBuffer( DaoArray *self, void *buffer, size_t size );

 Check if "self" is a function or overloaded functions that can be called as:
 *     func( p[0], ..., p[n] )
 * Or,
 *     o->func( p[0], ..., p[n] )
 * If yes, return the best matched function; otherwise return NULL. 
DaoMethod* DaoMethod_Resolve( DaoMethod *self, DValue *o, DValue *p[], int n );

DValue DaoObject_GetField( DaoObject *self, const char *name );
 return a null value, or a value of DaoFunctree, DaoRoutine or DaoFunction: 
DaoMethod* DaoObject_GetMethod( DaoObject *self, const char *name );
DaoCData* DaoObject_MapCData( DaoObject *self, DaoTypeBase *typer );

DaoStream* DaoStream_New();
void DaoStream_SetFile( DaoStream *self, FILE *fd );
FILE* DaoStream_GetFile( DaoStream *self );

 data will be deleted with the new DaoCData 
DaoCData* DaoCData_New( DaoTypeBase *typer, void *data );
 data will not be deleted with the new DaoCData 
DaoCData* DaoCData_Wrap( DaoTypeBase *typer, void *data );
int    DaoCData_IsType( DaoCData *self, DaoTypeBase *typer );
 return 1 if the data will be deleted with the DaoCData, otherwise 0 
int    DaoCData_OwnData( DaoCData *self );
 tell daovm that self->data has external reference 
void   DaoCData_SetExtReference( DaoCData *self, int bl );
void   DaoCData_SetData( DaoCData *self, void *data );
void   DaoCData_SetBuffer( DaoCData *self, void *data, size_t size );
void   DaoCData_SetArray( DaoCData *self, void *data, size_t size, int itsize );
void*  DaoCData_CastData( DaoCData *self, DaoTypeBase *totyper );
void*  DaoCData_GetData( DaoCData *self );
void*  DaoCData_GetBuffer( DaoCData *self );
void** DaoCData_GetData2( DaoCData *self );
DaoObject* DaoCData_GetObject( DaoCData *self );
DaoTypeBase* DaoCData_GetTyper( DaoCData *self );

DaoRegex* DaoRegex_New( DString *pattern );
int DaoRegex_Match( DaoRegex *self, DString *src, size_t *start, size_t *end );
int DaoRegex_SubMatch( DaoRegex *self, int gid, size_t *start, size_t *end );
int DaoRegex_Change( DaoRegex *self, DString *src, DString *target, int index );

DaoMutex* DaoMutex_New( DaoVmSpace *vms );
void DaoMutex_Lock( DaoMutex *self );
void DaoMutex_Unlock( DaoMutex *self );
int DaoMutex_TryLock( DaoMutex *self );

dint*      DaoContext_PutInteger( DaoContext *self, dint value );
float*     DaoContext_PutFloat( DaoContext *self, float value );
double*    DaoContext_PutDouble( DaoContext *self, double value );
complex16* DaoContext_PutComplex( DaoContext *self, complex16 value );
DString*   DaoContext_PutMBString( DaoContext *self, const char *mbs );
DString*   DaoContext_PutWCString( DaoContext *self, const wchar_t *wcs );
DString*   DaoContext_PutString( DaoContext *self, DString *str );
DString*   DaoContext_PutBytes( DaoContext *self, const char *bytes, int N );
DEnum*     DaoContext_PutEnum( DaoContext *self, const char *symbols );
DaoArray*  DaoContext_PutArrayInteger( DaoContext *self, int *array, int N );
DaoArray*  DaoContext_PutArrayShort( DaoContext *self, short *array, int N );
DaoArray*  DaoContext_PutArrayFloat( DaoContext *self, float *array, int N );
DaoArray*  DaoContext_PutArrayDouble( DaoContext *self, double *array, int N );
DaoArray*  DaoContext_PutArrayComplex( DaoContext *self, complex16 *array, int N );
DaoList*   DaoContext_PutList( DaoContext *self );
DaoMap*    DaoContext_PutMap( DaoContext *self );
DaoArray*  DaoContext_PutArray( DaoContext *self );
DaoTuple*  DaoContext_PutTuple( DaoContext *self );
DaoStream* DaoContext_PutFile( DaoContext *self, FILE *file );
DValue* DaoContext_PutValue( DaoContext *self, DValue value );
 data will be deleted with the new DaoCData 
DaoCData*  DaoContext_PutCData( DaoContext *self, void *data, DaoTypeBase *typer );
DaoCData*  DaoContext_PutCPointer( DaoContext *self, void *data, int size );
DaoBase*   DaoContext_PutResult( DaoContext *self, DaoBase *data );
 data will not be deleted with the new DaoCData 
DaoCData*  DaoContext_WrapCData( DaoContext *self, void *data, DaoTypeBase *typer );
 data will be deleted with the new DaoCData 
DaoCData*  DaoContext_CopyCData( DaoContext *self, void *d, int n, DaoTypeBase *t );

DaoVmProcess* DaoContext_CurrentProcess( DaoContext *self );
void DaoContext_RaiseException( DaoContext *self, int type, const char *value );

DaoVmProcess* DaoVmProcess_New( DaoVmSpace *vms );

 Compile source codes in "src", with substitution of escape chars in strings, if rpl != 0 
int DaoVmProcess_Compile( DaoVmProcess *self, DaoNameSpace *ns, DString *src, int rpl );
 Evaluate source codes in "src", with substitution of escape chars in strings, if rpl != 0 
int DaoVmProcess_Eval( DaoVmProcess *self, DaoNameSpace *ns, DString *src, int rpl );

 f: function to be called, one of DaoFunctree, DaoRoutine and DaoFunction: 
 Try to call "f" as:
 *     f( p[0], ..., p[n] )
 * Or,
 *     o->f( p[0], ..., p[n] )
 * Return 1 if successful, otherwise return 0. 
int DaoVmProcess_Call( DaoVmProcess *s, DaoMethod *f, DValue *o, DValue *p[], int n );
void  DaoVmProcess_Stop( DaoVmProcess *self );
DValue DaoVmProcess_GetReturned( DaoVmProcess *self );
DaoRegex* DaoVmProcess_MakeRegex( DaoVmProcess *self, DString *patt, int mbs );

DaoNameSpace* DaoNameSpace_New( DaoVmSpace *vms, const char *name );
 get namespace with the name, create if not exits: 
DaoNameSpace* DaoNameSpace_GetNameSpace( DaoNameSpace *self, const char *name );
int  DaoNameSpace_AddParent( DaoNameSpace *self, DaoNameSpace *parent );
void DaoNameSpace_AddConstNumbers( DaoNameSpace *self0, DaoNumItem *items );
void DaoNameSpace_AddConstValue( DaoNameSpace *self, const char *s, DValue v );
void DaoNameSpace_AddConstData( DaoNameSpace *self, const char *name, DaoBase *data );
void DaoNameSpace_AddData( DaoNameSpace *self, const char *name, DaoBase *data, const char *type);
void DaoNameSpace_AddValue( DaoNameSpace *self, const char *name, DValue data, const char *type);
DValue DaoNameSpace_FindData( DaoNameSpace *self, const char *name );

 equivalent to: typedef old type; in scripts 
 return NULL if failed 
DaoType* DaoNameSpace_TypeDefine( DaoNameSpace *self, const char *old, const char *type );
 wrap c type, return NULL if failed 
DaoType* DaoNameSpace_WrapType( DaoNameSpace *self, DaoTypeBase *typer );
 wrap c function, return NULL if failed 
DaoFunction* DaoNameSpace_WrapFunction( DaoNameSpace *self, DaoFuncPtr fp, const char *proto );

   parameters alias[] is an array of type name aliases,
   used as typedefs like: typedef alias[2*i] alias[2*i+1];
   the last item in alias[] should also be NULL.
   return the number of failed typedefs.
 
int DaoNameSpace_TypeDefines( DaoNameSpace *self, const char *alias[] );
 wrap c types, the last item in typer[] should be NULL;
   types that are cross-used in parameter lists
   (e.g. type A appears in the parameter list of B's methods,
   and type B appears in the parameter list of A's methods),
   should be wrapped using this function.
   return the number of failed wrapping.
 
int DaoNameSpace_WrapTypes( DaoNameSpace *self, DaoTypeBase *typer[] );
 wrap c functions, return the number of failed wrapping. 
int DaoNameSpace_WrapFunctions( DaoNameSpace *self, DaoFuncItem *items );
 load the scripts in "file" to the namespace 
int DaoNameSpace_Load( DaoNameSpace *self, const char *file );
int DaoNameSpace_GetOptions( DaoNameSpace *self );
void DaoNameSpace_SetOptions( DaoNameSpace *self, int options );

DaoVmSpace* DaoVmSpace_New();
int DaoVmSpace_ParseOptions( DaoVmSpace *self, DString *options );
void DaoVmSpace_SetOptions( DaoVmSpace *self, int options );
int  DaoVmSpace_GetOptions( DaoVmSpace *self );

int DaoVmSpace_RunMain( DaoVmSpace *self, DString *file );
DaoNameSpace* DaoVmSpace_Load( DaoVmSpace *self, DString *file );
DaoNameSpace* DaoVmSpace_GetNameSpace( DaoVmSpace *self, const char *name );
DaoNameSpace* DaoVmSpace_MainNameSpace( DaoVmSpace *self );
DaoVmProcess* DaoVmSpace_MainVmProcess( DaoVmSpace *self );
 get a process object from a pool 
DaoVmProcess* DaoVmSpace_AcquireProcess( DaoVmSpace *self );
 return a process object from a pool 
void DaoVmSpace_ReleaseProcess( DaoVmSpace *self, DaoVmProcess *proc );

void DaoVmSpace_SetUserHandler( DaoVmSpace *self, DaoUserHandler *handler );
void DaoVmSpace_ReadLine( DaoVmSpace *self, ReadLine fptr );
void DaoVmSpace_AddHistory( DaoVmSpace *self, AddHistory fptr );

void DaoVmSpace_AddVirtualFile( DaoVmSpace *self, const char *f, const char *s );
void DaoVmSpace_SetPath( DaoVmSpace *self, const char *path );
void DaoVmSpace_AddPath( DaoVmSpace *self, const char *path );
void DaoVmSpace_DelPath( DaoVmSpace *self, const char *path );

void  DaoVmSpace_Stop( DaoVmSpace *self, int bl );

void DaoGC_IncRC( DaoBase *p );
void DaoGC_DecRC( DaoBase *p );

DaoType* DaoType_GetFromTypeStructure( DaoTypeBase *typer );

DaoCallbackData* DaoCallbackData_New( DaoMethod *callback, DaoValue *userdata );
*/
