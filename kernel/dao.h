/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

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
#include<stddef.h>

#define DAO_VERSION "1.2"
#define DAO_H_VERSION 20120112

#if (defined DAO_WITH_CONCURRENT && !defined DAO_WITH_THREAD)
#define DAO_WITH_THREAD
#endif

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

#if defined(__STRICT_ANSI__) && defined(DAO_USE_INT64)
#undef DAO_USE_INT64
#endif

/* define an integer type with size equal to the size of pointers
 * under both 32-bits and 64-bits systems. */
#ifdef DAO_USE_INT64
typedef long long           long_t;
typedef unsigned long long  ulong_t;
#else
typedef long                long_t;
typedef unsigned long       ulong_t;
#endif /* DAO_USE_INT64 */

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
DAO_DLL int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns ); \
DAO_EXTC_CLOSE


#define DAO_MAX_CDATA_SUPER 10


#ifdef __cplusplus
extern "C"{
#endif

extern int daoProxyPort;

enum DaoTypes
{
	DAO_NONE  = 0,
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
	DAO_ROUTINE   ,
	DAO_PROCESS ,
	DAO_NAMESPACE ,
	DAO_VMSPACE   ,
	DAO_MUTEX ,
	DAO_CONDVAR ,
	DAO_SEMA ,
	DAO_TYPE ,
	END_CORE_TYPES
};
enum DaoProcessStatus
{
	DAO_VMPROC_FINISHED ,  /* finished normally */
	DAO_VMPROC_ABORTED ,   /* execution aborted */
	DAO_VMPROC_SUSPENDED , /* suspended, by coroutine.yield() */
	DAO_VMPROC_RUNNING ,   /* currently running */
	DAO_VMPROC_STACKED     /* new context is pushed onto the stack of the process */
};
enum DaoNamespaceOption
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

typedef ptrdiff_t      daoint;
typedef unsigned char  uchar_t;
typedef unsigned short ushort_t;
typedef unsigned int   uint_t;

typedef struct DString     DString;
typedef struct DArray      DArray;
typedef struct DLong       DLong;
typedef struct DNode       DNode;
typedef struct DMap        DMap;

typedef struct DArray          DaoFactory;
typedef struct DaoTypeCore     DaoTypeCore;
typedef struct DaoTypeBase     DaoTypeBase;
typedef struct DaoUserStream   DaoUserStream;
typedef struct DaoUserHandler  DaoUserHandler;
typedef struct DaoCallbackData DaoCallbackData;

typedef union  DaoValue        DaoValue;
typedef struct DaoNone         DaoNone;
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
typedef struct DaoRoutine      DaoRoutine;
typedef struct DaoInterface    DaoInterface;
typedef struct DaoClass        DaoClass;
typedef struct DaoObject       DaoObject;
typedef struct DaoStream       DaoStream;
typedef struct DaoCtype        DaoCtype;
typedef struct DaoCdata        DaoCdata;
typedef struct DaoRegex        DaoRegex;
typedef struct DaoNamespace    DaoNamespace;
typedef struct DaoVmSpace      DaoVmSpace;
typedef struct DaoProcess      DaoProcess;
typedef struct DaoMutex        DaoMutex;
typedef struct DaoCondVar      DaoCondVar;
typedef struct DaoSema         DaoSema;
typedef struct DaoType         DaoType;

/* Complex type: */
typedef struct complex16 { double real, imag; } complex16;

typedef void (*CallbackOnString)( const char *str );
typedef void (*FuncDaoInit)();
typedef void  (*DThreadTask)( void *arg );
typedef void* (*FuncPtrCast)( void* );
typedef void  (*FuncPtrDel)( void* );
typedef void  (*DaoCFunction) ( DaoProcess *process, DaoValue *params[], int npar );

typedef int (*DaoModuleLoader)( DaoNamespace *nspace, DString *filename, DString *emsg );
typedef int (*DaoCodeInliner)( DaoNamespace *nspace, DString *mode, DString *source, DString *out );

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
	DaoCFunction  fpter;    /* C function pointer; */
	const char   *proto;    /* function prototype: name( parlist ) => return_type */
};

/* Typer structure, contains type information of each Dao type: */
struct DaoTypeBase
{
	const char    *name; /* type name; */
	DaoTypeCore   *core; /* data used internally; */
	DaoNumItem    *numItems; /* constant number list */
	DaoFuncItem   *funcItems; /* method list: should end with a null item */

	/*
	// typers for super types, to create c type hierarchy;
	// mainly useful for wrapping c++ libraries.
	*/
	DaoTypeBase   *supers[ DAO_MAX_CDATA_SUPER ];

	/*
	// function(s) to cast a C/C++ type to one of its parent type:
	// usually they should be set to NULL, but for wrapping C++ class
	// with virtual methods, it is necessary to provide casting function(s)
	// in the following form:
	//   void* cast_Sub_to_Base( void *data ) { return (Base*)(Sub*)data; }
	*/
	FuncPtrCast    casts[ DAO_MAX_CDATA_SUPER ];

	/*
	// function to free data:
	// it is called on the data field (DaoCdata::data), if the type is a opaque C type;
	// otherwise, it is called on the type itself.
	// For opaque C type, it is only called for DaoCdata created by DaoCdata_New(),
	// DaoProcess_PutCdata(), or DaoFactory_NewCdata() with nonzero "own" parameter.
	*/
	void  (*Delete)( void *self );

	/*
	// Get garbage collectable fields (Dao data types with refCount by the type): 
	// Dao data types should be pushed into "values";
	// DArray holding Dao data types should be pushed into "arrays";
	// DMap holding Dao data types should be pushed into "maps";
	// When "remove" != 0, references to data that are pushed to "values" should be broken;
	*/
	void  (*GetGCFields)( void *self, DArray *values, DArray *arrays, DArray *maps, int remove );
};

/* Callback data: freed when "callback" or "userdata" is collected by GC. */
struct DaoCallbackData
{
	DaoRoutine  *callback;
	DaoValue    *userdata;
};

struct DaoUserStream
{
	/* count>0: read count bytes; count=0: one line; count<0: until EOF */
	void (*StdioRead)( DaoUserStream *self, DString *input, int count );
	void (*StdioWrite)( DaoUserStream *self, DString *output );
	void (*StdioFlush)( DaoUserStream *self );
};

/*
// This structure can be passed to DaoVmSpace by DaoVmSpace_SetUserHandler(),
// to change the handling of debugging and profiling behaviour.
*/
struct DaoUserHandler
{
	void (*StdlibDebug)( DaoUserHandler *self, DaoProcess *process );
	/* properly change some NOP codes to DEBUG codes */
	void (*BreakPoints)( DaoUserHandler *self, DaoRoutine *routine );
	/* profiling hooks, for future use */
	void (*Called)( DaoUserHandler *self, DaoRoutine *caller, DaoRoutine *callee );
	void (*Returned)( DaoUserHandler *self, DaoRoutine *caller, DaoRoutine *callee );
	/* invoke host execution to do whatever (e.g., to process GUI events) */
	void (*InvokeHost)( DaoUserHandler *self, DaoProcess *process );
};
typedef char* (*ReadLine)( const char *prompt );
typedef void  (*AddHistory)( const char *cmd );



/*
// DaoInit() should be called to initialize the Dao library,
// before using any other functions. Optional parameter "command"
// should be the name of the executable, and will be used to setup
// the module searching paths if the environment variable "DAO_DIR"
// has not been set. This function will return the first instance
// of DaoVmSpace.
*/
DAO_DLL DaoVmSpace* DaoInit( const char *command );

/*
// DaoQuit() should be called to finalize the library. It will wait 
// for unfinished computation, and do some cleanup, then quit.
*/
DAO_DLL void DaoQuit();



/*
// DaoValue_Type() returns the type of the value.
*/
DAO_DLL int DaoValue_Type( DaoValue *self );

/*
// The following functions will check the type of the DaoValue and 
// cast it to the requested type on success. Otherwise return NULL;
*/
DAO_DLL DaoInteger*   DaoValue_CastInteger( DaoValue *self );
DAO_DLL DaoFloat*     DaoValue_CastFloat( DaoValue *self );
DAO_DLL DaoDouble*    DaoValue_CastDouble( DaoValue *self );
DAO_DLL DaoComplex*   DaoValue_CastComplex( DaoValue *self );
DAO_DLL DaoLong*      DaoValue_CastLong( DaoValue *self );
DAO_DLL DaoString*    DaoValue_CastString( DaoValue *self );
DAO_DLL DaoEnum*      DaoValue_CastEnum( DaoValue *self );
DAO_DLL DaoArray*     DaoValue_CastArray( DaoValue *self );
DAO_DLL DaoList*      DaoValue_CastList( DaoValue *self );
DAO_DLL DaoMap*       DaoValue_CastMap( DaoValue *self );
DAO_DLL DaoTuple*     DaoValue_CastTuple( DaoValue *self );
DAO_DLL DaoStream*    DaoValue_CastStream( DaoValue *self );
DAO_DLL DaoObject*    DaoValue_CastObject( DaoValue *self );
DAO_DLL DaoCdata*     DaoValue_CastCdata( DaoValue *self );
DAO_DLL DaoClass*     DaoValue_CastClass( DaoValue *self );
DAO_DLL DaoInterface* DaoValue_CastInterface( DaoValue *self );
DAO_DLL DaoRoutine*   DaoValue_CastRoutine( DaoValue *self );
DAO_DLL DaoProcess*   DaoValue_CastProcess( DaoValue *self );
DAO_DLL DaoNamespace* DaoValue_CastNamespace( DaoValue *self );
DAO_DLL DaoType*      DaoValue_CastType( DaoValue *self );

/*
// The following functions will check the type of the DaoValue and 
// return the requested data on success. Otherwise return zero or NULL;
*/
DAO_DLL daoint    DaoValue_TryGetInteger( DaoValue *self );
DAO_DLL float     DaoValue_TryGetFloat( DaoValue *self );
DAO_DLL double    DaoValue_TryGetDouble( DaoValue *self );
DAO_DLL complex16 DaoValue_TryGetComplex( DaoValue *self );
DAO_DLL char*     DaoValue_TryGetMBString( DaoValue *self );
DAO_DLL wchar_t*  DaoValue_TryGetWCString( DaoValue *self );
DAO_DLL DString*  DaoValue_TryGetString( DaoValue *self );
DAO_DLL int       DaoValue_TryGetEnum( DaoValue *self );
DAO_DLL void*     DaoValue_TryGetCdata( DaoValue *self );
DAO_DLL void**    DaoValue_TryGetCdata2( DaoValue *self );

/*
// DaoValue_TryCastCdata() will cast the data of the cdata to the type
// as specified by "totype". This will essentially call a chain of cast
// functions as specified in the "casts" fields of DaoTypeBase structures
// along the inheritance chain between the type of the cdata value and
// the type "totype".
//
// Return NULL if "totype" is not a parent type of the value.
*/
DAO_DLL void* DaoValue_TryCastCdata( DaoValue *self, DaoType *totype );

/*
// DaoValue_Copy() copies value from "source" to "dest".
//
// For simple types such as int, float, double, complex, long, string and enum,
// if "dest" already holds an object of the same type as "source", the data
// field(s) are copied from "source" to "dest"; otherwise, a new object of the
// same type with the same data will be created at "dest".
//
// For other types, only the pointer is copied to "dest", and the reference
// count is updated for both "source" and original object at "dest".
*/
DAO_DLL void DaoValue_Copy( DaoValue *source, DaoValue **dest );

/*
// DaoValue_ClearAll() will clear values in the array that holds "n" values.
*/
DAO_DLL void DaoValue_ClearAll( DaoValue *v[], int n );



/*
// String functions:
//
// DString_New() creates a new multi-byte string (MBS) or wide-character string;
*/
DAO_DLL DString* DString_New( int mbs );

DAO_DLL DString* DString_Copy( DString *self );
DAO_DLL void DString_Delete( DString *self );
DAO_DLL daoint DString_Size( DString *self );
DAO_DLL void DString_Clear( DString *self );
DAO_DLL int  DString_IsMBS( DString *self );

/*
// DString_Reset() reset the size of the string to "size",
// if "size" is smaller than the capacity of the string;
// otherwise resize it to "size".
*/
DAO_DLL void DString_Reset( DString *self, daoint size );

/*
// DString_Resize() resizes the string to size "size".
*/
DAO_DLL void DString_Resize( DString *self, daoint size );

/*
// DString_SetMBS() and DString_SetWCS() replace the data of the string
// with data specified by null-terminated C string.
*/
DAO_DLL void DString_SetMBS( DString *self, const char *chs );
DAO_DLL void DString_SetWCS( DString *self, const wchar_t *chs );

/*
// DString_SetDataMBS() and DString_SetDataWCS() replace the data of string
// with data specified by (wide) character array with "n" characters.
// If "n" is zero, the character array is assumed to be null-terminated.
*/
DAO_DLL void DString_SetDataMBS( DString *self, const char *data, daoint n );
DAO_DLL void DString_SetDataWCS( DString *self, const wchar_t *data, daoint n );

/*
// DString_ToMBS() and DString_ToWCS() convert the string to the requested
// character type.
*/
DAO_DLL void DString_ToMBS( DString *self );
DAO_DLL void DString_ToWCS( DString *self );
DAO_DLL char* DString_GetMBS( DString *self );
DAO_DLL wchar_t* DString_GetWCS( DString *self );
DAO_DLL void DString_Chop( DString *self );
DAO_DLL void DString_Trim( DString *self );
DAO_DLL void DString_Erase( DString *self, daoint start, daoint n );
DAO_DLL void DString_Insert( DString *self, DString *s, daoint i, daoint m, daoint n );
DAO_DLL void DString_InsertChar( DString *self, const char ch, daoint at );
DAO_DLL void DString_InsertMBS( DString *self, const char *s, daoint i, daoint m, daoint n );
DAO_DLL void DString_InsertWCS( DString *self, const wchar_t *s, daoint i, daoint m, daoint n );
DAO_DLL void DString_Append( DString *self, DString *chs );
DAO_DLL void DString_AppendChar( DString *self, const char ch );
DAO_DLL void DString_AppendWChar( DString *self, const wchar_t ch );
DAO_DLL void DString_AppendMBS( DString *self, const char *chs );
DAO_DLL void DString_AppendWCS( DString *self, const wchar_t *chs );
DAO_DLL void DString_AppendDataMBS( DString *self, const char *data, daoint count );
DAO_DLL void DString_AppendDataWCS( DString *self, const wchar_t *data,daoint count );
DAO_DLL void DString_SubString( DString *self, DString *sub, daoint from, daoint count );
DAO_DLL daoint DString_Find( DString *self, DString *chs, daoint start );
DAO_DLL daoint DString_FindMBS( DString *self, const char *ch, daoint start );
DAO_DLL daoint DString_FindChar( DString *self, char ch, daoint start );
DAO_DLL daoint DString_FindWChar( DString *self, wchar_t ch, daoint start );
DAO_DLL daoint DString_RFind( DString *self, DString *chs, daoint start );
DAO_DLL daoint DString_RFindMBS( DString *self, const char *ch, daoint start );
DAO_DLL daoint DString_RFindChar( DString *self, char ch, daoint start );
DAO_DLL void DString_Assign( DString *left, DString *right );
DAO_DLL int DString_Compare( DString *left, DString *right );



DAO_DLL DaoInteger* DaoInteger_New( daoint value );
DAO_DLL daoint      DaoInteger_Get( DaoInteger *self );
DAO_DLL void        DaoInteger_Set( DaoInteger *self, daoint value );

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

DAO_DLL DaoString*  DaoString_New( int mbs );
DAO_DLL DaoString*  DaoString_NewMBS( const char *mbs );
DAO_DLL DaoString*  DaoString_NewWCS( const wchar_t *wcs );
DAO_DLL DaoString*  DaoString_NewBytes( const char *bytes, daoint n );

DAO_DLL daoint  DaoString_Size( DaoString *self );

DAO_DLL DString*  DaoString_Get( DaoString *self );
DAO_DLL const char*  DaoString_GetMBS( DaoString *self );
DAO_DLL const wchar_t* DaoString_GetWCS( DaoString *self );

DAO_DLL void  DaoString_Set( DaoString *self, DString *str );
DAO_DLL void  DaoString_SetMBS( DaoString *self, const char *mbs );
DAO_DLL void  DaoString_SetWCS( DaoString *self, const wchar_t *wcs );
DAO_DLL void  DaoString_SetBytes( DaoString *self, const char *bytes, daoint n );

DAO_DLL DaoEnum* DaoEnum_New( DaoType *type, int value );



DAO_DLL DaoList* DaoList_New();
DAO_DLL int  DaoList_Size( DaoList *self );
DAO_DLL DaoValue* DaoList_Front( DaoList *self );
DAO_DLL DaoValue* DaoList_Back( DaoList *self );
DAO_DLL DaoValue* DaoList_GetItem( DaoList *self, daoint pos );
DAO_DLL int DaoList_SetItem( DaoList *self, DaoValue *item, daoint pos );
DAO_DLL int DaoList_Insert( DaoList *self, DaoValue *item, daoint pos );
DAO_DLL int DaoList_PushFront( DaoList *self, DaoValue *item );
DAO_DLL int DaoList_PushBack( DaoList *self, DaoValue *item );
DAO_DLL void DaoList_PopFront( DaoList *self );
DAO_DLL void DaoList_PopBack( DaoList *self );
DAO_DLL void DaoList_Erase( DaoList *self, daoint pos );
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
DAO_DLL void DaoTuple_SetItem( DaoTuple *self, DaoValue *it, int pos );
DAO_DLL DaoValue* DaoTuple_GetItem( DaoTuple *self, int pos );



DAO_DLL DaoArray* DaoArray_New( int numtype );
DAO_DLL int  DaoArray_NumType( DaoArray *self );
DAO_DLL void DaoArray_SetNumType( DaoArray *self, short numtype );
DAO_DLL int  DaoArray_Size( DaoArray *self );
DAO_DLL int  DaoArray_DimCount( DaoArray *self );
DAO_DLL int  DaoArray_SizeOfDim( DaoArray *self, int d );
DAO_DLL void DaoArray_GetShape( DaoArray *self, daoint *dims );
DAO_DLL int  DaoArray_HasShape( DaoArray *self, daoint *dims, int D );
DAO_DLL int  DaoArray_GetFlatIndex( DaoArray *self, daoint *indexes );
DAO_DLL void DaoArray_ResizeVector( DaoArray *self, daoint N );
DAO_DLL void DaoArray_ResizeArray( DaoArray *self, daoint *dims, int D );
DAO_DLL int  DaoArray_Reshape( DaoArray *self, daoint *dims, int D );

DAO_DLL daoint* DaoArray_ToInteger( DaoArray *self );
DAO_DLL float*  DaoArray_ToFloat( DaoArray *self );
DAO_DLL double* DaoArray_ToDouble( DaoArray *self );
DAO_DLL signed   char* DaoArray_ToSByte( DaoArray *self );
DAO_DLL unsigned char* DaoArray_ToUByte( DaoArray *self );
DAO_DLL signed   short* DaoArray_ToSShort( DaoArray *self );
DAO_DLL unsigned short* DaoArray_ToUShort( DaoArray *self );
DAO_DLL signed   int* DaoArray_ToSInt( DaoArray *self );
DAO_DLL unsigned int* DaoArray_ToUInt( DaoArray *self );

DAO_DLL void  DaoArray_FromInteger( DaoArray *self ); 
DAO_DLL void  DaoArray_FromFloat( DaoArray *self ); 
DAO_DLL void  DaoArray_FromDouble( DaoArray *self ); 
DAO_DLL void  DaoArray_FromSByte( DaoArray *self ); 
DAO_DLL void  DaoArray_FromUByte( DaoArray *self ); 
DAO_DLL void  DaoArray_FromSShort( DaoArray *self ); 
DAO_DLL void  DaoArray_FromUShort( DaoArray *self ); 
DAO_DLL void  DaoArray_FromUInt( DaoArray *self ); 
DAO_DLL void  DaoArray_FromSInt( DaoArray *self ); 

DAO_DLL void  DaoArray_SetVectorI( DaoArray *self, daoint* vec, daoint N );
DAO_DLL void  DaoArray_SetVectorF( DaoArray *self, float* vec, daoint N );
DAO_DLL void  DaoArray_SetVectorD( DaoArray *self, double* vec, daoint N );
DAO_DLL void  DaoArray_SetVectorSB( DaoArray *self, signed char* vec, daoint N );
DAO_DLL void  DaoArray_SetVectorUB( DaoArray *self, unsigned char* vec, daoint N );
DAO_DLL void  DaoArray_SetVectorSS( DaoArray *self, signed short* vec, daoint N );
DAO_DLL void  DaoArray_SetVectorUS( DaoArray *self, unsigned short* vec, daoint N );
DAO_DLL void  DaoArray_SetVectorSI( DaoArray *self, signed int* vec, daoint N );
DAO_DLL void  DaoArray_SetVectorUI( DaoArray *self, unsigned int* vec, daoint N );
DAO_DLL void  DaoArray_SetMatrixI( DaoArray *self, daoint **mat, daoint row, daoint col );
DAO_DLL void  DaoArray_SetMatrixF( DaoArray *self, float **mat, daoint row, daoint col );
DAO_DLL void  DaoArray_SetMatrixD( DaoArray *self, double **mat, daoint row, daoint col );
DAO_DLL void  DaoArray_SetMatrixSB( DaoArray *self, signed char **mat, daoint row, daoint col );
DAO_DLL void  DaoArray_SetMatrixUB( DaoArray *self, unsigned char **mat, daoint row, daoint col );
DAO_DLL void  DaoArray_SetMatrixSS( DaoArray *self, signed short **mat, daoint row, daoint col );
DAO_DLL void  DaoArray_SetMatrixUS( DaoArray *self, unsigned short **mat, daoint row, daoint col );
DAO_DLL void  DaoArray_SetMatrixSI( DaoArray *self, signed int **mat, daoint row, daoint col );
DAO_DLL void  DaoArray_SetMatrixUI( DaoArray *self, unsigned int **mat, daoint row, daoint col );

DAO_DLL void* DaoArray_GetBuffer( DaoArray *self );
DAO_DLL void DaoArray_SetBuffer( DaoArray *self, void *buffer, daoint size );



/*
// DaoRoutine_Resolve() resolves from overloaded routines to get the best routine that
// matches the parameter values.
//
// Note: if the routine is not overloaded, it is returned without checking.
//
// The parameter values are normally specified in an array pointed by "P" with "N" entries.
// If a non-null value "O" is passed, it will be checked against the "self" parameter of
// the routine if it has one, and the values in "P" will be checked against the rest of
// of the parameters.
*/
DAO_DLL DaoRoutine* DaoRoutine_Resolve( DaoRoutine *self, DaoValue *O, DaoValue *P[], int N );

/*
// DaoRoutine_IsWrapper() checks if the routine is a wrapped C function.
*/
DAO_DLL int DaoRoutine_IsWrapper( DaoRoutine *self );



DAO_DLL DaoRoutine* DaoObject_GetMethod( DaoObject *self, const char *name );
DAO_DLL DaoValue*   DaoObject_GetField( DaoObject *self, const char *name );
DAO_DLL DaoCdata*   DaoObject_CastCdata( DaoObject *self, DaoType *type );



DAO_DLL DaoStream* DaoStream_New();
DAO_DLL void DaoStream_Delete( DaoStream *self );
DAO_DLL void DaoStream_Close( DaoStream *self );
DAO_DLL void DaoStream_Flush( DaoStream *self );
DAO_DLL void DaoStream_WriteChar( DaoStream *self, char val );
DAO_DLL void DaoStream_WriteInt( DaoStream *self, daoint val );
DAO_DLL void DaoStream_WriteFloat( DaoStream *self, double val );
DAO_DLL void DaoStream_WriteString( DaoStream *self, DString *val );
DAO_DLL void DaoStream_WriteMBS( DaoStream *self, const char *val );
DAO_DLL void DaoStream_WriteWCS( DaoStream *self, const wchar_t *val );
DAO_DLL void DaoStream_WritePointer( DaoStream *self, void *val );
DAO_DLL void DaoStream_SetFile( DaoStream *self, FILE *fd );
DAO_DLL void DaoStream_SetUserStream( DaoStream *self, DaoUserStream *us );
DAO_DLL FILE* DaoStream_GetFile( DaoStream *self );
DAO_DLL int DaoStream_ReadLine( DaoStream *self, DString *line );
DAO_DLL int DaoFile_ReadLine( FILE *fin, DString *line );
DAO_DLL int DaoFile_ReadAll( FILE *fin, DString *all, int close );
DAO_DLL void DaoFile_WriteString( FILE *fout, DString *str );



DAO_DLL DaoCdata* DaoCdata_New( DaoType *type, void *data );
DAO_DLL DaoCdata* DaoCdata_Wrap( DaoType *type, void *data );
DAO_DLL int    DaoCdata_IsType( DaoCdata *self, DaoType *type );
DAO_DLL int    DaoCdata_OwnData( DaoCdata *self );
DAO_DLL void   DaoCdata_SetType( DaoCdata *self, DaoType *type );
DAO_DLL void   DaoCdata_SetData( DaoCdata *self, void *data );
DAO_DLL void*  DaoCdata_CastData( DaoCdata *self, DaoType *totype );
DAO_DLL void*  DaoCdata_GetData( DaoCdata *self );
DAO_DLL void** DaoCdata_GetData2( DaoCdata *self );
DAO_DLL DaoObject* DaoCdata_GetObject( DaoCdata *self );
DAO_DLL DaoTypeBase* DaoCdata_GetTyper( DaoCdata *self );

DAO_DLL DaoRegex* DaoRegex_New( DString *pattern );
DAO_DLL int DaoRegex_Match( DaoRegex *self, DString *src, daoint *start, daoint *end );
DAO_DLL int DaoRegex_SubMatch( DaoRegex *self, int gid, daoint *start, daoint *end );
DAO_DLL int DaoRegex_Change( DaoRegex *self, DString *src, DString *target, int index );

DAO_DLL DaoMutex* DaoMutex_New();
DAO_DLL void DaoMutex_Lock( DaoMutex *self );
DAO_DLL void DaoMutex_Unlock( DaoMutex *self );
DAO_DLL int DaoMutex_TryLock( DaoMutex *self );

DAO_DLL DaoCondVar* DaoCondVar_New();
DAO_DLL void DaoCondVar_Delete( DaoCondVar *self );
DAO_DLL void DaoCondVar_Wait( DaoCondVar *self, DaoMutex *mutex );
DAO_DLL int  DaoCondVar_TimedWait( DaoCondVar *self, DaoMutex *mutex, double seconds );
DAO_DLL void DaoCondVar_Signal( DaoCondVar *self );
DAO_DLL void DaoCondVar_BroadCast( DaoCondVar *self );

DAO_DLL DaoSema* DaoSema_New( int n );
DAO_DLL void DaoSema_Delete( DaoSema *self );
DAO_DLL void DaoSema_Wait( DaoSema *self );
DAO_DLL void DaoSema_Post( DaoSema *self );
DAO_DLL void DaoSema_SetValue( DaoSema *self, int n );
DAO_DLL int  DaoSema_GetValue( DaoSema *self );



DAO_DLL DaoProcess* DaoProcess_New( DaoVmSpace *vms );
DAO_DLL int DaoProcess_Compile( DaoProcess *self, DaoNamespace *ns, DString *src, int rpl );
DAO_DLL int DaoProcess_Eval( DaoProcess *self, DaoNamespace *ns, DString *src, int rpl );
DAO_DLL int DaoProcess_Call( DaoProcess *s, DaoRoutine *f, DaoValue *o, DaoValue *p[], int n );
DAO_DLL void DaoProcess_Stop( DaoProcess *self );
DAO_DLL void DaoProcess_SetStdio( DaoProcess *self, DaoStream *stream );
DAO_DLL void DaoProcess_RaiseException( DaoProcess *self, int type, const char *value );
DAO_DLL DaoValue* DaoProcess_GetReturned( DaoProcess *self );
DAO_DLL DaoRegex* DaoProcess_MakeRegex( DaoProcess *self, DString *patt, int mbs );
DAO_DLL DaoFactory* DaoProcess_GetFactory( DaoProcess *self );

DAO_DLL DaoType*   DaoProcess_GetReturnType( DaoProcess *self );

/*
// The following functions can be called within a wrapped C function to create
// the returned value. The value object will be properly created (if necessary)
// for returning, which means for types such as enum, array, list, map and tuple,
// the returned value object will have proper type.
//
// For example, if the wrapped function specifies "list<int>" as the returning type,
// DaoProcess_PutList() will return a list of type "list<int>", and only DaoInteger
// can be pushed successfully into the list. And if "tuple<float,string>" is the
// specified returning type, the tuple returned by DaoProcess_PutTuple() will be
// a tuple that can only accept DaoInteger as its first item, and DaoString as its
// second item.
//
// All these functions return NULL when failed. This happens when the intended
// returning value does not match to the returning type that is specified by the
// function prototype of the wrapped C function.
*/
DAO_DLL daoint*    DaoProcess_PutInteger( DaoProcess *self, daoint value );
DAO_DLL float*     DaoProcess_PutFloat( DaoProcess *self, float value );
DAO_DLL double*    DaoProcess_PutDouble( DaoProcess *self, double value );
DAO_DLL complex16* DaoProcess_PutComplex( DaoProcess *self, complex16 value );
DAO_DLL DString*   DaoProcess_PutMBString( DaoProcess *self, const char *mbs );
DAO_DLL DString*   DaoProcess_PutWCString( DaoProcess *self, const wchar_t *wcs );
DAO_DLL DString*   DaoProcess_PutString( DaoProcess *self, DString *str );
DAO_DLL DString*   DaoProcess_PutBytes( DaoProcess *self, const char *bytes, daoint N );
DAO_DLL DaoEnum*   DaoProcess_PutEnum( DaoProcess *self, const char *symbols );
DAO_DLL DaoArray*  DaoProcess_PutArrayInteger( DaoProcess *self, daoint *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutArrayFloat( DaoProcess *self, float *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutArrayDouble( DaoProcess *self, double *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutArrayComplex( DaoProcess *self, complex16 *array, daoint N );
DAO_DLL DaoList*   DaoProcess_PutList( DaoProcess *self );
DAO_DLL DaoMap*    DaoProcess_PutMap( DaoProcess *self );
DAO_DLL DaoArray*  DaoProcess_PutArray( DaoProcess *self );
DAO_DLL DaoTuple*  DaoProcess_PutTuple( DaoProcess *self );
DAO_DLL DaoStream* DaoProcess_PutFile( DaoProcess *self, FILE *file );
DAO_DLL DaoValue*  DaoProcess_PutValue( DaoProcess *self, DaoValue *value );

/*
// DaoProcess_PutCdata() creates a cdata as the returned value.
// This cdata will be responsible to deallocate "data".
*/
DAO_DLL DaoCdata*  DaoProcess_PutCdata( DaoProcess *self, void *data, DaoType *type );

/*
// DaoProcess_PutCdata() creates a cdata as the returned value.
// This cdata will not be responsible to deallocate "data".
*/
DAO_DLL DaoCdata*  DaoProcess_WrapCdata( DaoProcess *self, void *data, DaoType *type );

/*
// DaoProcess_PutCdata() creates a cdata as the returned value.
// This cdata will make a copy of "D" which is assumed to has "N" bytes,
// and be responsible to deallocate the copied data.
*/
DAO_DLL DaoCdata*  DaoProcess_CopyCdata( DaoProcess *self, void *D, int N, DaoType *T );



DAO_DLL DaoNamespace* DaoNamespace_New( DaoVmSpace *vms, const char *name );
DAO_DLL DaoNamespace* DaoNamespace_GetNamespace( DaoNamespace *self, const char *name );
DAO_DLL int  DaoNamespace_AddParent( DaoNamespace *self, DaoNamespace *parent );
DAO_DLL void DaoNamespace_AddConstNumbers( DaoNamespace *self, DaoNumItem *items );
DAO_DLL void DaoNamespace_AddConstValue( DaoNamespace *self, const char *name, DaoValue *data );
DAO_DLL void DaoNamespace_AddValue( DaoNamespace *self, const char *name, DaoValue *d, const char *type);
DAO_DLL DaoValue* DaoNamespace_FindData( DaoNamespace *self, const char *name );
DAO_DLL DaoType* DaoNamespace_TypeDefine( DaoNamespace *self, const char *old, const char *type );
DAO_DLL DaoType* DaoNamespace_WrapType( DaoNamespace *self, DaoTypeBase *typer, int opaque );
DAO_DLL DaoRoutine* DaoNamespace_WrapFunction( DaoNamespace *self, DaoCFunction fp, const char *proto );
DAO_DLL int DaoNamespace_TypeDefines( DaoNamespace *self, const char *alias[] );
DAO_DLL int DaoNamespace_WrapTypes( DaoNamespace *self, DaoTypeBase *typer[] );
DAO_DLL int DaoNamespace_WrapFunctions( DaoNamespace *self, DaoFuncItem *items );
DAO_DLL int DaoNamespace_Load( DaoNamespace *self, const char *file );
DAO_DLL int DaoNamespace_GetOptions( DaoNamespace *self );
DAO_DLL void DaoNamespace_SetOptions( DaoNamespace *self, int options );
DAO_DLL void DaoNamespace_AddModuleLoader( DaoNamespace *self, const char *name, DaoModuleLoader fp );
DAO_DLL void DaoNamespace_AddCodeInliner( DaoNamespace *self, const char *name, DaoCodeInliner fp );



DAO_DLL DaoVmSpace* DaoVmSpace_New();
DAO_DLL int DaoVmSpace_ParseOptions( DaoVmSpace *self, DString *options );
DAO_DLL void DaoVmSpace_SetOptions( DaoVmSpace *self, int options );
DAO_DLL int  DaoVmSpace_GetOptions( DaoVmSpace *self );

DAO_DLL int DaoVmSpace_RunMain( DaoVmSpace *self, DString *file );
DAO_DLL DaoNamespace* DaoVmSpace_Load( DaoVmSpace *self, DString *file, int run );
DAO_DLL DaoNamespace* DaoVmSpace_LinkModule( DaoVmSpace *self, DaoNamespace *ns, const char *mod );
DAO_DLL DaoNamespace* DaoVmSpace_GetNamespace( DaoVmSpace *self, const char *name );
DAO_DLL DaoNamespace* DaoVmSpace_MainNamespace( DaoVmSpace *self );
DAO_DLL DaoProcess* DaoVmSpace_MainProcess( DaoVmSpace *self );
DAO_DLL DaoProcess* DaoVmSpace_AcquireProcess( DaoVmSpace *self );
DAO_DLL DaoFactory* DaoVmSpace_AcquireFactory( DaoVmSpace *self );
DAO_DLL void DaoVmSpace_ReleaseProcess( DaoVmSpace *self, DaoProcess *proc );
DAO_DLL void DaoVmSpace_ReleaseFactory( DaoVmSpace *self, DaoFactory *factory );

DAO_DLL void DaoVmSpace_SetStdio( DaoVmSpace *self, DaoStream *stream );
DAO_DLL void DaoVmSpace_SetStdError( DaoVmSpace *self, DaoStream *stream );
DAO_DLL void DaoVmSpace_SetUserHandler( DaoVmSpace *self, DaoUserHandler *handler );
DAO_DLL void DaoVmSpace_ReadLine( DaoVmSpace *self, ReadLine fptr );
DAO_DLL void DaoVmSpace_AddHistory( DaoVmSpace *self, AddHistory fptr );

DAO_DLL void DaoVmSpace_AddVirtualFile( DaoVmSpace *self, const char *f, const char *s );
DAO_DLL void DaoVmSpace_SetPath( DaoVmSpace *self, const char *path );
DAO_DLL void DaoVmSpace_AddPath( DaoVmSpace *self, const char *path );
DAO_DLL void DaoVmSpace_DelPath( DaoVmSpace *self, const char *path );
DAO_DLL const char* DaoVmSpace_CurrentWorkingPath( DaoVmSpace *self );
DAO_DLL const char* DaoVmSpace_CurrentLoadingPath( DaoVmSpace *self );

DAO_DLL void DaoVmSpace_Stop( DaoVmSpace *self, int bl );



/*
// DaoFactory_CacheValue() caches the value in the value factory.
*/
DAO_DLL void DaoFactory_CacheValue( DaoFactory *self, DaoValue *value );

/*
// DaoFactory_GetLastValues() returns the last "N" created or cached
// values as an array.
*/
DAO_DLL DaoValue** DaoFactory_GetLastValues( DaoFactory *self, int N );

/*
// The following methods create values of the requested type with data
// specified by the parameter(s). Values created in this way have references
// stored in the value factory, so that user does not need to handle the
// reference counting of the created value.
*/
DAO_DLL DaoNone*    DaoFactory_NewNone( DaoFactory *self );
DAO_DLL DaoInteger* DaoFactory_NewInteger( DaoFactory *self, daoint v );
DAO_DLL DaoFloat*   DaoFactory_NewFloat( DaoFactory *self, float v );
DAO_DLL DaoDouble*  DaoFactory_NewDouble( DaoFactory *self, double v );
DAO_DLL DaoComplex* DaoFactory_NewComplex( DaoFactory *self, complex16 v );
DAO_DLL DaoLong*    DaoFactory_NewLong( DaoFactory *self );
DAO_DLL DaoString*  DaoFactory_NewString( DaoFactory *self, int mbs );
DAO_DLL DaoString*  DaoFactory_NewMBString( DaoFactory *self, const char *s, daoint n );
DAO_DLL DaoString*  DaoFactory_NewWCString( DaoFactory *self, const wchar_t *s, daoint n );
DAO_DLL DaoEnum*    DaoFactory_NewEnum( DaoFactory *self, DaoType *type, int value );
DAO_DLL DaoList*    DaoFactory_NewList( DaoFactory *self );

/*
// DaoFactory_NewMap() creates a (hash) map.
*/
DAO_DLL DaoMap*   DaoFactory_NewMap( DaoFactory *self, int hashing );

/*
// DaoFactory_NewArray() creates a numeric array with element type
// specified by the parameter "type".
*/
DAO_DLL DaoArray* DaoFactory_NewArray( DaoFactory *self, int type );

/*
// DaoFactory_NewVectorSB() creates an integer vector from an array of signed byte;
// DaoFactory_NewVectorUB() creates an integer vector from an array of unsigned byte;
// DaoFactory_NewVectorSS() creates an integer vector from an array of signed short;
// DaoFactory_NewVectorUS() creates an integer vector from an array of unsigned short;
// DaoFactory_NewVectorSI() creates an integer vector from an array of signed int;
// DaoFactory_NewVectorUI() creates an integer vector from an array of unsigned int;
// DaoFactory_NewVectorI()  creates an integer vector from an array of daoint;
// DaoFactory_NewVectorF()  creates an float vector from an array of float;
// DaoFactory_NewVectorD()  creates an double vector from an array of double;
//
// If "n" is not zero, the created array will allocate a new buffer, and copy
// the data from the C array passed as parameter to the new buffer; otherwise,
// the created array will directly use the C array as buffer.
//
// In the case that the C array is directly used, one can call reshape() to set
// the array to proper shape before using. The C array must be ensured to be valid
// throughout the use of the created array; and its deallocation must be handled by
// the owner of the C array. A typical scenario of using array in this way is to call
// a Dao function from C, and pass a C array to the Dao function.
*/
DAO_DLL DaoArray* DaoFactory_NewVectorSB( DaoFactory *self, signed char *s, daoint n ); 
DAO_DLL DaoArray* DaoFactory_NewVectorUB( DaoFactory *self, unsigned char *s, daoint n ); 
DAO_DLL DaoArray* DaoFactory_NewVectorSS( DaoFactory *self, signed short *s, daoint n ); 
DAO_DLL DaoArray* DaoFactory_NewVectorUS( DaoFactory *self, unsigned short *s, daoint n ); 
DAO_DLL DaoArray* DaoFactory_NewVectorSI( DaoFactory *self, signed int *s, daoint n ); 
DAO_DLL DaoArray* DaoFactory_NewVectorUI( DaoFactory *self, unsigned int *s, daoint n ); 
DAO_DLL DaoArray* DaoFactory_NewVectorI( DaoFactory *self, daoint *s, daoint n ); 
DAO_DLL DaoArray* DaoFactory_NewVectorF( DaoFactory *self, float *s, daoint n ); 
DAO_DLL DaoArray* DaoFactory_NewVectorD( DaoFactory *self, double *s, daoint n ); 

/*
// DaoFactory_NewMatrixSB() creates an integer matrix from a [n x m] matrix of signed byte;
// DaoFactory_NewMatrixUB() creates an integer matrix from a [n x m] matrix of unsigned byte;
// DaoFactory_NewMatrixSS() creates an integer matrix from a [n x m] matrix of signed short;
// DaoFactory_NewMatrixUS() creates an integer matrix from a [n x m] matrix of unsigned short;
// DaoFactory_NewMatrixSI() creates an integer matrix from a [n x m] matrix of signed int;
// DaoFactory_NewMatrixUI() creates an integer matrix from a [n x m] matrix of unsigned int;
// DaoFactory_NewMatrixI() creates an integer matrix from a [n x m] matrix of daoint;
// DaoFactory_NewMatrixF() creates an float matrix from a [n x m] matrix of float;
// DaoFactory_NewMatrixD() creates an double matrix from a [n x m] matrix of double;
*/
DAO_DLL DaoArray* DaoFactory_NewMatrixSB( DaoFactory *self, signed char **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoFactory_NewMatrixUB( DaoFactory *self, unsigned char **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoFactory_NewMatrixSS( DaoFactory *self, signed short **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoFactory_NewMatrixUS( DaoFactory *self, unsigned short **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoFactory_NewMatrixSI( DaoFactory *self, signed int **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoFactory_NewMatrixUI( DaoFactory *self, unsigned int **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoFactory_NewMatrixI( DaoFactory *self, daoint **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoFactory_NewMatrixF( DaoFactory *self, float **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoFactory_NewMatrixD( DaoFactory *self, double **s, daoint n, daoint m );

/*
// DaoFactory_NewStream() creates a new stream with specified file.
*/
DAO_DLL DaoStream* DaoFactory_NewStream( DaoFactory *self, FILE *file );

/*
// DaoFactory_NewCdata() creates a new cdata object with specified type and data.
// If and only if "owned" is not zero, the created cdata will be responsible to
// deallocated "data".
*/
DAO_DLL DaoCdata* DaoFactory_NewCdata( DaoFactory *self, DaoType *type, void *data, int owned );


DAO_DLL void DaoGC_IncRC( DaoValue *p );
DAO_DLL void DaoGC_DecRC( DaoValue *p );

DAO_DLL DaoType* DaoType_GetFromTypeStructure( DaoTypeBase *typer );

DAO_DLL DaoCallbackData* DaoCallbackData_New( DaoRoutine *callback, DaoValue *userdata );

#ifdef __cplusplus
}
#endif

#endif

/*

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


DaoList* DaoList_New();
int  DaoList_Size( DaoList *self );
DaoValue DaoList_Front( DaoList *self );
DaoValue DaoList_Back( DaoList *self );
DaoValue DaoList_GetItem( DaoList *self, size_t pos );

 The following functions return 0 if seccessful, 1 if failed: 
int DaoList_SetItem( DaoList *self, DaoValue *item, size_t pos );
int DaoList_Insert( DaoList *self, DaoValue *item, size_t pos );
int DaoList_PushFront( DaoList *self, DaoValue *item );
int DaoList_PushBack( DaoList *self, DaoValue *item );
void DaoList_PopFront( DaoList *self );
void DaoList_PopBack( DaoList *self );
void DaoList_Erase( DaoList *self, size_t pos );
void DaoList_Clear( DaoList *self );

DaoMap* DaoMap_New( int hashing );
int  DaoMap_Size( DaoMap *self );
 return 0 if successful; return 1 if key not matching, 2 if value not matching: 
int  DaoMap_Insert( DaoMap *self, DaoValue key, DaoValue value );
int  DaoMap_InsertMBS( DaoMap *self, const char *key, DaoValue value );
int  DaoMap_InsertWCS( DaoMap *self, const wchar_t *key, DaoValue value );
void DaoMap_Erase( DaoMap *self, DaoValue key );
void DaoMap_EraseMBS( DaoMap *self, const char *key );
void DaoMap_EraseWCS( DaoMap *self, const wchar_t *key );
void DaoMap_Clear( DaoMap *self );
DaoValue DaoMap_GetValue( DaoMap *self, DaoValue key  );
DaoValue DaoMap_GetValueMBS( DaoMap *self, const char *key  );
DaoValue DaoMap_GetValueWCS( DaoMap *self, const wchar_t *key  );
DNode* DaoMap_First( DaoMap *self );
DNode* DaoMap_Next( DaoMap *self, DNode *iter );
DaoValue* DNode_Key( DNode *self );
DaoValue* DNode_Value( DNode *self );

DaoTuple* DaoTuple_New( int size );
int  DaoTuple_Size( DaoTuple *self );
void   DaoTuple_SetItem( DaoTuple *self, DaoValue it, int pos );
DaoValue DaoTuple_GetItem( DaoTuple *self, int pos );

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
void  DaoArray_ResizeVector( DaoArray *self, size_t N );
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
void  DaoArray_SetMatrixD( DaoArray *self, double **mat, size_t row, size_t col );
void  DaoArray_SetVectorUB( DaoArray *self, unsigned char* vec, size_t N );
void  DaoArray_SetVectorUS( DaoArray *self, unsigned short* vec, size_t N );
void  DaoArray_SetVectorUI( DaoArray *self, unsigned int* vec, size_t N );

void* DaoArray_GetBuffer( DaoArray *self );
void DaoArray_SetBuffer( DaoArray *self, void *buffer, size_t size );

 Check if "self" is a function or overloaded functions that can be called as:
 *     func( p[0], ..., p[n] )
 * Or,
 *     o->func( p[0], ..., p[n] )
 * If yes, return the best matched function; otherwise return NULL. 
DaoRoutine* DaoRoutine_Resolve( DaoRoutine *self, DaoValue *o, DaoValue *p[], int n );

 return a null value, or a value of DaoRoutine: 
DaoRoutine* DaoObject_GetMethod( DaoObject *self, const char *name );
DaoValue DaoObject_GetField( DaoObject *self, const char *name );
DaoCdata* DaoObject_CastCdata( DaoObject *self, DaoType *type );

DaoStream* DaoStream_New();
void DaoStream_SetFile( DaoStream *self, FILE *fd );
FILE* DaoStream_GetFile( DaoStream *self );

 data will be deleted with the new DaoCdata 
DaoCdata* DaoCdata_New( DaoType *type, void *data );
 data will not be deleted with the new DaoCdata 
DaoCdata* DaoCdata_Wrap( DaoType *type, void *data );
int    DaoCdata_IsType( DaoCdata *self, DaoType *typer );
 return 1 if the data will be deleted with the DaoCdata, otherwise 0 
int    DaoCdata_OwnData( DaoCdata *self );
 tell daovm that self->data has external reference 
void   DaoCdata_SetData( DaoCdata *self, void *data );
void*  DaoCdata_CastData( DaoValue *self, DaoType *totype );
void*  DaoCdata_GetData( DaoCdata *self );
void** DaoCdata_GetData2( DaoCdata *self );
DaoObject* DaoCdata_GetObject( DaoCdata *self );
DaoTypeBase* DaoCdata_GetTyper( DaoCdata *self );

DaoRegex* DaoRegex_New( DString *pattern );
int DaoRegex_Match( DaoRegex *self, DString *src, size_t *start, size_t *end );
int DaoRegex_SubMatch( DaoRegex *self, int gid, size_t *start, size_t *end );
int DaoRegex_Change( DaoRegex *self, DString *src, DString *target, int index );

DaoMutex* DaoMutex_New();
void DaoMutex_Lock( DaoMutex *self );
void DaoMutex_Unlock( DaoMutex *self );
int DaoMutex_TryLock( DaoMutex *self );

DaoProcess* DaoProcess_New( DaoVmSpace *vms );

 Compile source codes in "src", with substitution of escape chars in strings, if rpl != 0 
int DaoProcess_Compile( DaoProcess *self, DaoNamespace *ns, DString *src, int rpl );
 Evaluate source codes in "src", with substitution of escape chars in strings, if rpl != 0 
int DaoProcess_Eval( DaoProcess *self, DaoNamespace *ns, DString *src, int rpl );

 f: function to be called, one of DaoRoutree, DaoRoutine: 
 Try to call "f" as:
 *     f( p[0], ..., p[n] )
 * Or,
 *     o->f( p[0], ..., p[n] )
 * Return 0 if successful, otherwise return DAO_ERROR, DAO_ERROR_PARAM etc. 
int DaoProcess_Call( DaoProcess *s, DaoRoutine *f, DaoValue *o, DaoValue *p[], int n );
void  DaoProcess_Stop( DaoProcess *self );
DaoValue DaoProcess_GetReturned( DaoProcess *self );
DaoRegex* DaoProcess_MakeRegex( DaoProcess *self, DString *patt, int mbs );


DaoNamespace* DaoNamespace_New( DaoVmSpace *vms, const char *name );
 get namespace with the name, create if not exits: 
DaoNamespace* DaoNamespace_GetNamespace( DaoNamespace *self, const char *name );
int  DaoNamespace_AddParent( DaoNamespace *self, DaoNamespace *parent );
void DaoNamespace_AddConstNumbers( DaoNamespace *self0, DaoNumItem *items );
void DaoNamespace_AddConstValue( DaoNamespace *self, const char *s, DaoValue v );
void DaoNamespace_AddConstData( DaoNamespace *self, const char *name, DaoBase *data );
void DaoNamespace_AddData( DaoNamespace *self, const char *name, DaoBase *data, const char *type);
void DaoNamespace_AddValue( DaoNamespace *self, const char *name, DaoValue data, const char *type);
DaoValue DaoNamespace_FindData( DaoNamespace *self, const char *name );

 equivalent to: typedef old type; in scripts 
 return NULL if failed 
DaoType* DaoNamespace_TypeDefine( DaoNamespace *self, const char *old, const char *type );
 wrap c type, return NULL if failed 
DaoType* DaoNamespace_WrapType( DaoNamespace *self, DaoTypeBase *typer, int opaque );
 wrap c function, return NULL if failed 
DaoRoutine* DaoNamespace_WrapFunction( DaoNamespace *self, DaoCFunction fp, const char *proto );

   parameters alias[] is an array of type name aliases,
   used as typedefs like: typedef alias[2*i] alias[2*i+1];
   the last item in alias[] should also be NULL.
   return the number of failed typedefs.
 
int DaoNamespace_TypeDefines( DaoNamespace *self, const char *alias[] );
 wrap c types, the last item in typer[] should be NULL;
   types that are cross-used in parameter lists
   (e.g. type A appears in the parameter list of B's methods,
   and type B appears in the parameter list of A's methods),
   should be wrapped using this function.
   return the number of failed wrapping.
 
int DaoNamespace_WrapTypes( DaoNamespace *self, DaoTypeBase *typer[] );
 wrap c functions, return the number of failed wrapping. 
int DaoNamespace_WrapFunctions( DaoNamespace *self, DaoFuncItem *items );
 load the scripts in "file" to the namespace 
int DaoNamespace_Load( DaoNamespace *self, const char *file );
int DaoNamespace_GetOptions( DaoNamespace *self );
void DaoNamespace_SetOptions( DaoNamespace *self, int options );

DaoVmSpace* DaoVmSpace_New();
int DaoVmSpace_ParseOptions( DaoVmSpace *self, DString *options );
void DaoVmSpace_SetOptions( DaoVmSpace *self, int options );
int  DaoVmSpace_GetOptions( DaoVmSpace *self );

int DaoVmSpace_RunMain( DaoVmSpace *self, DString *file );
DaoNamespace* DaoVmSpace_Load( DaoVmSpace *self, DString *file );
DaoNamespace* DaoVmSpace_GetNamespace( DaoVmSpace *self, const char *name );
DaoNamespace* DaoVmSpace_MainNamespace( DaoVmSpace *self );
DaoProcess* DaoVmSpace_MainProcess( DaoVmSpace *self );
 get a process object from a pool 
DaoProcess* DaoVmSpace_AcquireProcess( DaoVmSpace *self );
 return a process object from a pool 
void DaoVmSpace_ReleaseProcess( DaoVmSpace *self, DaoProcess *proc );

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

DaoCallbackData* DaoCallbackData_New( DaoRoutine *callback, DaoValue *userdata );
*/
