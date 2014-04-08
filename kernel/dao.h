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

#ifndef __DAO_H__
#define __DAO_H__

#include<wctype.h>
#include<wchar.h>
#include<stdio.h>
#include<stdlib.h>
#include<stddef.h>


#define DAO_VERSION "2.0"


#if (defined DAO_WITH_CONCURRENT && !defined DAO_WITH_THREAD)
#  define DAO_WITH_THREAD
#endif


#if defined(MAC_OSX) && ! defined(UNIX)
#  define UNIX
#endif /* MAC_OSX */


#ifdef WIN32

/* Get rid of the effects of UNICODE: */
#  ifdef UNICODE
#    undef UNICODE
#  endif /* UNICODE */

#  define DAO_DLL_EXPORT __declspec(dllexport)
#  define DAO_DLL_IMPORT __declspec(dllimport)

#  ifdef DAO_KERNEL
#     define DAO_DLL DAO_DLL_EXPORT
#  else
#     define DAO_DLL DAO_DLL_IMPORT
#  endif

#else /* other system */

#  define DAO_DLL extern
#  define DAO_DLL_EXPORT
#  define DAO_DLL_IMPORT

#endif /* WIN32 */


#ifndef DAO_INT_FORMAT
#  ifdef WIN32
#    define DAO_INT_FORMAT  "Ii"
#  else
#    define DAO_INT_FORMAT  "ti"
#  endif /* WIN32 */
#endif /* DAO_INT_FORMAT */


#define DAO_MAX_CDATA_SUPER 8


#ifdef __cplusplus
extern "C"{
#endif


enum DaoTypes
{
	DAO_NONE  = 0,
	DAO_INTEGER ,
	DAO_FLOAT   ,
	DAO_DOUBLE  ,
	DAO_COMPLEX ,
	DAO_STRING ,
	DAO_ENUM  ,
	DAO_ARRAY ,
	DAO_LIST  ,
	DAO_MAP   ,
	DAO_TUPLE ,
	DAO_OBJECT ,
	DAO_CSTRUCT ,
	DAO_CDATA  ,
	DAO_CLASS  ,
	DAO_CTYPE  ,
	DAO_INTERFACE ,
	DAO_ROUTINE   ,
	DAO_PROCESS ,
	DAO_NAMESPACE ,
	DAO_VMSPACE   ,
	DAO_TYPE ,
	END_CORE_TYPES
};
enum DaoProcessStatus
{
	DAO_PROCESS_FINISHED ,  /* finished normally */
	DAO_PROCESS_ABORTED ,   /* execution aborted */
	DAO_PROCESS_SUSPENDED , /* suspended, by coroutine.yield() */
	DAO_PROCESS_RUNNING ,   /* currently running */
	DAO_PROCESS_STACKED     /* new context is pushed onto the stack of the process */
};
enum DaoNamespaceOption
{
	/* automatically make variable declared outside {} global, for interactive mode */
	DAO_NS_AUTO_GLOBAL = (1<<0)
};
/* Execution options, combinable by | */
enum DaoOptions
{
	DAO_OPTION_HELP     = (1<<0), /* -h, --help:         print this help information; */
	DAO_OPTION_VINFO    = (1<<1), /* -v, --version:      print version information; */
	DAO_OPTION_INTERUN  = (1<<2), /* -i, --interactive:  run in interactive mode; */
	DAO_OPTION_DEBUG    = (1<<3), /* -d, --debug:        run in debug mode; */
	DAO_OPTION_PROFILE  = (1<<4), /* -p, --profile:      run in profile mode; */
	DAO_OPTION_LIST_BC  = (1<<5), /* -l, --list-code:    print compiled bytecodes; */
	DAO_OPTION_JIT      = (1<<6), /* -j, --jit:          enable JIT compiling; */
	DAO_OPTION_COMP_BC  = (1<<7), /* -c, --compile:      compile to bytecodes; */
	DAO_OPTION_ARCHIVE  = (1<<8), /* -a, --archive:      build archive file; */

	/*
	// DAO_OPTION_IDE:
	// -- disable JIT;
	// -- disable function specialization;
	// -- insert NOP codes for convenient setting up of break points;
	*/
	DAO_OPTION_IDE = (1<<31)
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


/*
// define an integer type with size equal to the size of pointers
// under both 32-bits and 64-bits systems.
*/
typedef ptrdiff_t       daoint;
typedef unsigned char   uchar_t;
typedef unsigned short  ushort_t;
typedef unsigned int    uint_t;

typedef struct DString     DString;
typedef struct DArray      DArray;
typedef struct DNode       DNode;
typedef struct DMap        DMap;

typedef struct DaoTypeCore     DaoTypeCore;
typedef struct DaoTypeBase     DaoTypeBase;
typedef struct DaoStackFrame   DaoStackFrame;
typedef struct DaoUserStream   DaoUserStream;
typedef struct DaoUserHandler  DaoUserHandler;
typedef struct DaoDebugger     DaoDebugger;
typedef struct DaoProfiler     DaoProfiler;
typedef struct DaoParser       DaoParser;

typedef union  DaoValue        DaoValue;
typedef struct DaoNone         DaoNone;
typedef struct DaoInteger      DaoInteger;
typedef struct DaoFloat        DaoFloat;
typedef struct DaoDouble       DaoDouble;
typedef struct DaoComplex      DaoComplex;
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
typedef struct DaoCstruct      DaoCstruct;
typedef struct DaoRegex        DaoRegex;
typedef struct DaoException    DaoException;
typedef struct DaoNamespace    DaoNamespace;
typedef struct DaoVmSpace      DaoVmSpace;
typedef struct DaoProcess      DaoProcess;
typedef struct DaoChannel      DaoChannel;
typedef struct DaoType         DaoType;

/* Complex type: */
typedef struct complex16 { double real, imag; } complex16;

typedef void  (*CallbackOnString)( const char *str );
typedef void  (*DThreadTask)( void *arg );
typedef void* (*FuncPtrCast)( void*, int );
typedef void  (*FuncPtrDel)( void* );
typedef void  (*DaoCFunction)( DaoProcess *process, DaoValue *params[], int npar );

typedef int (*DaoTokenFilter)( DaoParser *parser );
typedef int (*DaoModuleOnLoad)( DaoVmSpace *vmspace, DaoNamespace *nspace );
typedef int (*DaoCodeInliner)( DaoNamespace *nspace, DString *mode, DString *source, DString *out, int line );

typedef struct DaoNumItem   DaoNumItem;
typedef struct DaoFuncItem  DaoFuncItem;
typedef struct DaoVModule   DaoVModule;

struct DaoNumItem
{
	const char *name;
	int         type;  /* DAO_INTEGER, DAO_FLOAT or DAO_DOUBLE */
	double      value;
};
struct DaoFuncItem
{
	DaoCFunction  fpter;  /* C function pointer; */
	const char   *proto;  /* function prototype: name( parlist ) => return_type */
};
struct DaoVModule
{
	const char       *name;    /* path + file name for the module; */
	signed int        length;  /* length of the file; */
	unsigned char    *data;    /* file content; */
	DaoModuleOnLoad   onload;  /* onload function pointer for C module; */
};


/* Type information structure for creating Dao types for C/C++ types: */
struct DaoTypeBase
{
	const char    *name;      /* type name; */
	DaoTypeCore   *core;      /* data used internally; */
	DaoNumItem    *numItems;  /* constant number list: should end with a null item; */
	DaoFuncItem   *funcItems; /* method list: should end with a null item; */

	/*
	// typers for super types, to create c type hierarchy;
	// mainly useful for wrapping c++ libraries.
	*/
	DaoTypeBase   *supers[ DAO_MAX_CDATA_SUPER ];

	/*
	// function(s) to cast a C/C++ type to and from one of its parent type:
	// usually they should be set to NULL, but for wrapping C++ class
	// with virtual method(s), it is necessary to provide casting function(s)
	// in the following form:
	//   void* cast_Sub_Base( void *data, int down_casting ) {
	//       if( down_casting ) return static_cast<Sub*>( (Base*)data );
	//       return dynamic_cast<Base*>( (Sub*)data );
	//   }
	// or:
	//   void* cast_Sub_Base( void *data, int down_casting ) {
	//       if( down_casting ) return dynamic_cast<Sub*>( (Base*)data );
	//       return dynamic_cast<Base*>( (Sub*)data );
	//   }
	// for class with virtual base(s).
	*/
	FuncPtrCast    casts[ DAO_MAX_CDATA_SUPER ];

	/*
	// function to free data:
	// it is called on the data field (DaoCdata::data), if the type is a opaque C type;
	// otherwise, it is called on the type itself.
	// For opaque C type, it is only called for DaoCdata created by DaoCdata_New(),
	// DaoProcess_PutCdata(), or DaoProcess_NewCdata() with nonzero "own" parameter.
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

struct DaoUserStream
{
	DaoStream *stream;
	/* count>0: read count bytes; count=0: one line; count<0: until EOF */
	void (*StdioRead)( DaoUserStream *self, DString *input, int count );
	void (*StdioWrite)( DaoUserStream *self, DString *output );
	void (*StdioFlush)( DaoUserStream *self );
	void (*SetColor)( DaoUserStream *self, const char *fgcolor, const char *bgcolor );
};

/*
// These structures can be passed to DaoVmSpace
// to change the handling of debugging and profiling behaviour.
*/
struct DaoDebugger
{
	void (*Debug)( DaoDebugger *self, DaoProcess *process, DaoStream *stream );
	/* properly change some NOP codes to DEBUG codes */
	void (*BreakPoints)( DaoDebugger *self, DaoRoutine *routine );
};
struct DaoProfiler
{
	void (*Reset)( DaoProfiler *self );
	void (*EnterFrame)( DaoProfiler *self, DaoProcess *proc, DaoStackFrame *frame, int start );
	void (*LeaveFrame)( DaoProfiler *self, DaoProcess *proc, DaoStackFrame *frame, int end );
	void (*Summarize)( DaoProfiler *self, DaoList *stat );
	void (*Report)( DaoProfiler *self, DaoStream *stream );
};
struct DaoUserHandler
{
	/* invoke host execution to do whatever (e.g., to process GUI events) */
	void (*InvokeHost)( DaoUserHandler *self, DaoProcess *process );
};
typedef char* (*ReadLine)( const char *prompt );
typedef int   (*AddHistory)( const char *cmd );



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
DAO_DLL DaoString*    DaoValue_CastString( DaoValue *self );
DAO_DLL DaoEnum*      DaoValue_CastEnum( DaoValue *self );
DAO_DLL DaoArray*     DaoValue_CastArray( DaoValue *self );
DAO_DLL DaoList*      DaoValue_CastList( DaoValue *self );
DAO_DLL DaoMap*       DaoValue_CastMap( DaoValue *self );
DAO_DLL DaoTuple*     DaoValue_CastTuple( DaoValue *self );
DAO_DLL DaoStream*    DaoValue_CastStream( DaoValue *self );
DAO_DLL DaoObject*    DaoValue_CastObject( DaoValue *self );
DAO_DLL DaoCstruct*   DaoValue_CastCstruct( DaoValue *self, DaoType *totype );
DAO_DLL DaoCdata*     DaoValue_CastCdata( DaoValue *self, DaoType *totype );
DAO_DLL DaoClass*     DaoValue_CastClass( DaoValue *self );
DAO_DLL DaoInterface* DaoValue_CastInterface( DaoValue *self );
DAO_DLL DaoRoutine*   DaoValue_CastRoutine( DaoValue *self );
DAO_DLL DaoProcess*   DaoValue_CastProcess( DaoValue *self );
DAO_DLL DaoNamespace* DaoValue_CastNamespace( DaoValue *self );
DAO_DLL DaoType*      DaoValue_CastType( DaoValue *self );

DAO_DLL DaoValue* DaoValue_MakeNone();

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
DAO_DLL void*     DaoValue_TryGetArray( DaoValue *self );
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
DAO_DLL DString* DString_NewMBS( const char *mbs );
DAO_DLL DString* DString_NewWCS( const wchar_t *wcs );

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
// If "n" is negative, the character array is assumed to be null-terminated.
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

/*
// DString_Erase() erases "n" characters starting from "start".
// If "n" is negative, erases all the rest from "start".
*/
DAO_DLL void DString_Erase( DString *self, daoint start, daoint n );

/*
// DString_Insert() inserts "s" to "self" at position "i".
// "m" characters will be erased at "i" from "self";
// "n" characters will be copied to "self" from "s";
// If "m" is negative, all characters starting from "i" will be erased.
*/
DAO_DLL void DString_Insert( DString *self, DString *s, daoint i, daoint m, daoint n );
DAO_DLL void DString_InsertMBS( DString *self, const char *s, daoint i, daoint m, daoint n );
DAO_DLL void DString_InsertWCS( DString *self, const wchar_t *s, daoint i, daoint m, daoint n );
DAO_DLL void DString_InsertChar( DString *self, const char ch, daoint at );
DAO_DLL void DString_Append( DString *self, DString *chs );
DAO_DLL void DString_AppendChar( DString *self, const char ch );
DAO_DLL void DString_AppendWChar( DString *self, const wchar_t ch );
DAO_DLL void DString_AppendMBS( DString *self, const char *chs );
DAO_DLL void DString_AppendWCS( DString *self, const wchar_t *chs );

/*
// DString_AppendDataMBS() and DString_AppendDataWCS() append "count" number
// of MBS or WCS characters to "self".
*/
DAO_DLL void DString_AppendDataMBS( DString *self, const char *data, daoint count );
DAO_DLL void DString_AppendDataWCS( DString *self, const wchar_t *data,daoint count );

/*
// DString_SubString() retrieves a substring starting from "from" with "count" characters.
// If "count" is negative, all the rest characters are retrieved.
*/
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
DAO_DLL DaoComplex* DaoComplex_New2( double real, double imag );
DAO_DLL complex16   DaoComplex_Get( DaoComplex *self );
DAO_DLL void        DaoComplex_Set( DaoComplex *self, complex16 value );

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
DAO_DLL DaoType* DaoList_GetType( DaoList *self );
DAO_DLL int DaoList_SetType( DaoList *self, DaoType *type );
DAO_DLL daoint DaoList_Size( DaoList *self );

DAO_DLL DaoValue* DaoList_Front( DaoList *self );
DAO_DLL DaoValue* DaoList_Back( DaoList *self );
DAO_DLL DaoValue* DaoList_GetItem( DaoList *self, daoint pos );

/*
// The following functions return 0 on seccess, and 1 otherwise:
*/
DAO_DLL int DaoList_SetItem( DaoList *self, DaoValue *item, daoint pos );
DAO_DLL int DaoList_Insert( DaoList *self, DaoValue *item, daoint pos );
DAO_DLL int DaoList_PushFront( DaoList *self, DaoValue *item );
DAO_DLL int DaoList_PushBack( DaoList *self, DaoValue *item );

DAO_DLL void DaoList_PopFront( DaoList *self );
DAO_DLL void DaoList_PopBack( DaoList *self );
DAO_DLL void DaoList_Erase( DaoList *self, daoint pos );
DAO_DLL void DaoList_Clear( DaoList *self );


enum DaoHashSeeds{ DAO_HASH_NONE, DAO_HASH_DEFAULT, DAO_HASH_RANDOM };
/*
// DaoMap_New() creates a map or hash map:
*/
DAO_DLL DaoMap* DaoMap_New( unsigned int hashing );
DAO_DLL DaoType* DaoMap_GetType( DaoMap *self );
DAO_DLL int DaoMap_SetType( DaoMap *self, DaoType *type );
DAO_DLL daoint DaoMap_Size( DaoMap *self );

/*
// The following functions return 0 on success;
// Return 1 if key not matching, and 2 if value not matching:
*/
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
DAO_DLL DaoType* DaoTuple_GetType( DaoTuple *self );
DAO_DLL int  DaoTuple_SetType( DaoTuple *self, DaoType *type );
DAO_DLL int  DaoTuple_Size( DaoTuple *self );
DAO_DLL void DaoTuple_SetItem( DaoTuple *self, DaoValue *it, int pos );
DAO_DLL DaoValue* DaoTuple_GetItem( DaoTuple *self, int pos );


/*
// DaoArray_New() creates a numeric array with specified numeric type,
// which must be one of DAO_INTEGER, DAO_FLOAT, DAO_DOUBLE or DAO_COMPLEX.
*/
DAO_DLL DaoArray* DaoArray_New( int numtype );
DAO_DLL int  DaoArray_NumType( DaoArray *self );
DAO_DLL void DaoArray_SetNumType( DaoArray *self, short numtype );

/*
// DaoArray_Size() gets the total number of elements in the array;
// DaoArray_DimCount() gets the number of dimensions (2 for vector and matrix);
// DaoArray_SizeOfDim() gets the size of the given dimension;
*/
DAO_DLL int  DaoArray_Size( DaoArray *self );
DAO_DLL int  DaoArray_DimCount( DaoArray *self );
DAO_DLL int  DaoArray_SizeOfDim( DaoArray *self, int d );

/*
// DaoArray_GetShape() gets the shape as an array of sizes for each dimension;
// DaoArray_HasShape() checks the array if it has the specified shape;
// DaoArray_Reshape() changes the array to specified shaped;
*/
DAO_DLL void DaoArray_GetShape( DaoArray *self, daoint *dims );
DAO_DLL int  DaoArray_HasShape( DaoArray *self, daoint *dims, int D );
DAO_DLL int  DaoArray_Reshape( DaoArray *self, daoint *dims, int D );

/*
// DaoArray_ResizeVector() resizes the array to a vector with "N" elements;
// DaoArray_ResizeArray() resizes the array to a array with specified shape;
*/
DAO_DLL void DaoArray_ResizeVector( DaoArray *self, daoint N );
DAO_DLL void DaoArray_ResizeArray( DaoArray *self, daoint *dims, int D );

/*
// DaoArray_GetFlatIndex() computes the raw/flat index from multiple indexes.
// "indexes" is expected to contain the same number of indexes
// as the array's number of dimensions.
*/
DAO_DLL int DaoArray_GetFlatIndex( DaoArray *self, daoint *indexes );

/*
// The following functions convert the data in the internal buffer to
// the specified numeric type, without altering the numeric type of the array.
//
// DaoArray_FromXyz() should be called after DaoArray_ToXyz() to restore
// the consistency between the numeric type of the array and the data buffer.
*/
DAO_DLL daoint* DaoArray_ToInteger( DaoArray *self );
DAO_DLL float*  DaoArray_ToFloat( DaoArray *self );
DAO_DLL double* DaoArray_ToDouble( DaoArray *self );
DAO_DLL signed   char* DaoArray_ToSByte( DaoArray *self );
DAO_DLL unsigned char* DaoArray_ToUByte( DaoArray *self );
DAO_DLL signed   short* DaoArray_ToSShort( DaoArray *self );
DAO_DLL unsigned short* DaoArray_ToUShort( DaoArray *self );
DAO_DLL signed   int* DaoArray_ToSInt( DaoArray *self );
DAO_DLL unsigned int* DaoArray_ToUInt( DaoArray *self );

/*
// The following functions DaoArray_FromXyz() re-interprets the internal data
// buffer as of "Xyz" type, and convert them to the numeric type of the array.
*/
DAO_DLL void DaoArray_FromInteger( DaoArray *self );
DAO_DLL void DaoArray_FromFloat( DaoArray *self );
DAO_DLL void DaoArray_FromDouble( DaoArray *self );
DAO_DLL void DaoArray_FromSByte( DaoArray *self );
DAO_DLL void DaoArray_FromUByte( DaoArray *self );
DAO_DLL void DaoArray_FromSShort( DaoArray *self );
DAO_DLL void DaoArray_FromUShort( DaoArray *self );
DAO_DLL void DaoArray_FromUInt( DaoArray *self );
DAO_DLL void DaoArray_FromSInt( DaoArray *self );

DAO_DLL void DaoArray_SetVectorI( DaoArray *self, daoint* vec, daoint N );
DAO_DLL void DaoArray_SetVectorF( DaoArray *self, float* vec, daoint N );
DAO_DLL void DaoArray_SetVectorD( DaoArray *self, double* vec, daoint N );
DAO_DLL void DaoArray_SetVectorSB( DaoArray *self, signed char* vec, daoint N );
DAO_DLL void DaoArray_SetVectorUB( DaoArray *self, unsigned char* vec, daoint N );
DAO_DLL void DaoArray_SetVectorSS( DaoArray *self, signed short* vec, daoint N );
DAO_DLL void DaoArray_SetVectorUS( DaoArray *self, unsigned short* vec, daoint N );
DAO_DLL void DaoArray_SetVectorSI( DaoArray *self, signed int* vec, daoint N );
DAO_DLL void DaoArray_SetVectorUI( DaoArray *self, unsigned int* vec, daoint N );
DAO_DLL void DaoArray_SetMatrixI( DaoArray *self, daoint **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixF( DaoArray *self, float **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixD( DaoArray *self, double **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixSB( DaoArray *self, signed char **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixUB( DaoArray *self, unsigned char **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixSS( DaoArray *self, signed short **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixUS( DaoArray *self, unsigned short **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixSI( DaoArray *self, signed int **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixUI( DaoArray *self, unsigned int **mat, daoint row, daoint col );

DAO_DLL void* DaoArray_GetBuffer( DaoArray *self );
DAO_DLL void  DaoArray_SetBuffer( DaoArray *self, void *buffer, daoint size );



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
DAO_DLL DaoCstruct* DaoObject_CastCstruct( DaoObject *self, DaoType *type );
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
DAO_DLL FILE* DaoStream_GetFile( DaoStream *self );
DAO_DLL int DaoStream_ReadLine( DaoStream *self, DString *line );
DAO_DLL int DaoFile_ReadLine( FILE *fin, DString *line );
DAO_DLL int DaoFile_ReadAll( FILE *fin, DString *all, int close );
DAO_DLL void DaoFile_WriteString( FILE *fout, DString *str );
DAO_DLL DaoUserStream* DaoStream_SetUserStream( DaoStream *self, DaoUserStream *us );



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

DAO_DLL DaoRegex* DaoRegex_New( DString *pattern );
DAO_DLL int DaoRegex_Match( DaoRegex *self, DString *src, daoint *start, daoint *end );
DAO_DLL int DaoRegex_SubMatch( DaoRegex *self, int gid, daoint *start, daoint *end );
DAO_DLL int DaoRegex_Change( DaoRegex *self, DString *src, DString *target, int index );



DAO_DLL DaoProcess* DaoProcess_New( DaoVmSpace *vms );
DAO_DLL int DaoProcess_Compile( DaoProcess *self, DaoNamespace *ns, const char *src );
DAO_DLL int DaoProcess_Eval( DaoProcess *self, DaoNamespace *ns, const char *src );
DAO_DLL int DaoProcess_Call( DaoProcess *s, DaoRoutine *f, DaoValue *o, DaoValue *p[], int n );
DAO_DLL void DaoProcess_SetStdio( DaoProcess *self, DaoStream *stream );
DAO_DLL void DaoProcess_RaiseException( DaoProcess *self, int type, const char *info );
DAO_DLL DaoException* DaoProcess_RaiseUserException( DaoProcess *self, const char *type, const char *info );
DAO_DLL DaoValue* DaoProcess_GetReturned( DaoProcess *self );
DAO_DLL DaoRegex* DaoProcess_MakeRegex( DaoProcess *self, DString *patt, int mbs );
DAO_DLL DaoType*  DaoProcess_GetReturnType( DaoProcess *self );

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
// All these functions return NULL when failed. This happens when the requested
// returning value does not match to the returning type that is specified by the
// function prototype of the wrapped C function.
*/
DAO_DLL DaoNone*   DaoProcess_PutNone( DaoProcess *self );
DAO_DLL daoint*    DaoProcess_PutInteger( DaoProcess *self, daoint value );
DAO_DLL float*     DaoProcess_PutFloat( DaoProcess *self, float value );
DAO_DLL double*    DaoProcess_PutDouble( DaoProcess *self, double value );
DAO_DLL complex16* DaoProcess_PutComplex( DaoProcess *self, complex16 value );
DAO_DLL DString*   DaoProcess_PutMBString( DaoProcess *self, const char *mbs );
DAO_DLL DString*   DaoProcess_PutWCString( DaoProcess *self, const wchar_t *wcs );
DAO_DLL DString*   DaoProcess_PutString( DaoProcess *self, DString *str );
DAO_DLL DString*   DaoProcess_PutBytes( DaoProcess *self, const char *bytes, daoint N );
DAO_DLL DaoEnum*   DaoProcess_PutEnum( DaoProcess *self, const char *symbols );
DAO_DLL DaoArray*  DaoProcess_PutArray( DaoProcess *self );
DAO_DLL DaoList*   DaoProcess_PutList( DaoProcess *self );
DAO_DLL DaoMap*    DaoProcess_PutMap( DaoProcess *self, unsigned int hashing );
DAO_DLL DaoStream* DaoProcess_PutFile( DaoProcess *self, FILE *file );
DAO_DLL DaoValue*  DaoProcess_PutValue( DaoProcess *self, DaoValue *value );

DAO_DLL DaoArray*  DaoProcess_PutVectorSB( DaoProcess *self, signed   char *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorUB( DaoProcess *self, unsigned char *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorSS( DaoProcess *self, signed   short *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorUS( DaoProcess *self, unsigned short *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorSI( DaoProcess *self, signed   int *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorUI( DaoProcess *self, unsigned int *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorI( DaoProcess *self, daoint *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorF( DaoProcess *self, float  *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorD( DaoProcess *self, double *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorC( DaoProcess *self, complex16 *array, daoint N );

/*
// DaoProcess_PutTuple() creates a tuple as the returned value.
//
// The tuple size can be specified in the following ways:
// 1. size==0: implicit size, to be determined by the destination type;
// 2. size!=0: explicit size, the absolute value of this parameter;
//
// The destination type has to be compatible with the to-be-created tuple,
// otherwise, a null pointer is returned to indicate an error.
//
// If the destination type is a variadic tuple (with variable size) type,
// an explicit size must be at least as great as the minimum size of the
// tuple type. And if the destination type is a fixed-size tuple type,
// the explicit size must be the same as the size of the tuple type.
//
// If size is negative, the size number of latest values created or pushed into
// the process's value cache will be used to created the tuple, and these values
// are subsequently removed from the cache.
//
// Example:
//   DaoProcess_NewInteger( proc, 123 );
//   DaoProcess_NewMBString( proc, "abc", -1 );
//   DaoProcess_PutTuple( proc, -2 );
// This will put a tuple of (123, 'abc').
*/
DAO_DLL DaoTuple*  DaoProcess_PutTuple( DaoProcess *self, int size );

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
DAO_DLL DaoType* DaoNamespace_MakeExceptionType( DaoNamespace *self, const char *name, int fatal );
DAO_DLL DaoType* DaoNamespace_WrapType( DaoNamespace *self, DaoTypeBase *typer, int opaque );
DAO_DLL DaoRoutine* DaoNamespace_WrapFunction( DaoNamespace *self, DaoCFunction fp, const char *proto );
DAO_DLL int DaoNamespace_TypeDefines( DaoNamespace *self, const char *alias[] );
DAO_DLL int DaoNamespace_WrapTypes( DaoNamespace *self, DaoTypeBase *typer[] );
DAO_DLL int DaoNamespace_WrapFunctions( DaoNamespace *self, DaoFuncItem *items );
DAO_DLL int DaoNamespace_Load( DaoNamespace *self, const char *file );
DAO_DLL int DaoNamespace_GetOptions( DaoNamespace *self );
DAO_DLL void DaoNamespace_SetOptions( DaoNamespace *self, int options );
DAO_DLL void DaoNamespace_AddCodeInliner( DaoNamespace *self, const char *name, DaoCodeInliner fp );



DAO_DLL DaoVmSpace* DaoVmSpace_New();
DAO_DLL DaoVmSpace* DaoVmSpace_MainVmSpace();
DAO_DLL int DaoVmSpace_ParseOptions( DaoVmSpace *self, const char *options );
DAO_DLL void DaoVmSpace_SetOptions( DaoVmSpace *self, int options );
DAO_DLL int  DaoVmSpace_GetOptions( DaoVmSpace *self );

DAO_DLL int DaoVmSpace_Eval( DaoVmSpace *self, const char *src );
DAO_DLL int DaoVmSpace_RunMain( DaoVmSpace *self, const char *file );
DAO_DLL DaoNamespace* DaoVmSpace_Load( DaoVmSpace *self, const char *file );
DAO_DLL DaoNamespace* DaoVmSpace_LinkModule( DaoVmSpace *self, DaoNamespace *ns, const char *mod );
DAO_DLL DaoNamespace* DaoVmSpace_GetNamespace( DaoVmSpace *self, const char *name );
DAO_DLL DaoNamespace* DaoVmSpace_MainNamespace( DaoVmSpace *self );
DAO_DLL DaoProcess* DaoVmSpace_MainProcess( DaoVmSpace *self );
DAO_DLL DaoProcess* DaoVmSpace_AcquireProcess( DaoVmSpace *self );
DAO_DLL void DaoVmSpace_ReleaseProcess( DaoVmSpace *self, DaoProcess *proc );

DAO_DLL DaoStream* DaoVmSpace_StdioStream( DaoVmSpace *self );
DAO_DLL DaoStream* DaoVmSpace_ErrorStream( DaoVmSpace *self );

DAO_DLL DaoUserStream* DaoVmSpace_SetUserStdio( DaoVmSpace *self, DaoUserStream *stream );
DAO_DLL DaoUserStream* DaoVmSpace_SetUserStdError( DaoVmSpace *self, DaoUserStream *stream );
DAO_DLL DaoUserHandler* DaoVmSpace_SetUserHandler( DaoVmSpace *self, DaoUserHandler *handler );
DAO_DLL DaoDebugger* DaoVmSpace_SetUserDebugger( DaoVmSpace *self, DaoDebugger *debugger );
DAO_DLL DaoProfiler* DaoVmSpace_SetUserProfiler( DaoVmSpace *self, DaoProfiler *profiler );
DAO_DLL void DaoVmSpace_ReadLine( DaoVmSpace *self, ReadLine fptr );
DAO_DLL void DaoVmSpace_AddHistory( DaoVmSpace *self, AddHistory fptr );

DAO_DLL void DaoVmSpace_AddVirtualModule( DaoVmSpace *self, DaoVModule *module );
DAO_DLL void DaoVmSpace_SetPath( DaoVmSpace *self, const char *path );
DAO_DLL void DaoVmSpace_AddPath( DaoVmSpace *self, const char *path );
DAO_DLL void DaoVmSpace_DelPath( DaoVmSpace *self, const char *path );
DAO_DLL const char* DaoVmSpace_CurrentWorkingPath( DaoVmSpace *self );
DAO_DLL const char* DaoVmSpace_CurrentLoadingPath( DaoVmSpace *self );

DAO_DLL void DaoVmSpace_Stop( DaoVmSpace *self, int bl );

/*
// DaoVmSpace_TryInitJIT() tries to load the JIT module.
// If "module" is NULL, the module will be searched in the module paths.
// Return 1 on success.
*/
DAO_DLL int DaoVmSpace_TryInitJIT( DaoVmSpace *self, const char *module );



/*
// DaoProcess_CacheValue() caches the value in the process.
*/
DAO_DLL void DaoProcess_CacheValue( DaoProcess *self, DaoValue *value );

/*
// DaoProcess_GetLastValues() returns the last "N" created or cached
// values as an array.
*/
DAO_DLL DaoValue** DaoProcess_GetLastValues( DaoProcess *self, int N );

/*
// DaoProcess_PopValues() return the last N value cached by the
// above DaoProcess_CacheValue() or the following DaoProcess_NewXXX()
// methods.
*/
DAO_DLL void DaoProcess_PopValues( DaoProcess *self, int N );

/*
// The following methods create values of the requested type with data
// specified by the parameter(s). Values created in this way have references
// stored in the process's cache, so that user does not need to handle the
// reference counting of the created value.
*/
DAO_DLL DaoNone*    DaoProcess_NewNone( DaoProcess *self );
DAO_DLL DaoInteger* DaoProcess_NewInteger( DaoProcess *self, daoint v );
DAO_DLL DaoFloat*   DaoProcess_NewFloat( DaoProcess *self, float v );
DAO_DLL DaoDouble*  DaoProcess_NewDouble( DaoProcess *self, double v );
DAO_DLL DaoComplex* DaoProcess_NewComplex( DaoProcess *self, complex16 v );
DAO_DLL DaoString*  DaoProcess_NewString( DaoProcess *self, int mbs );
/*
// Negative "n" indicates a null-terminated string:
*/
DAO_DLL DaoString*  DaoProcess_NewMBString( DaoProcess *self, const char *s, daoint n );
DAO_DLL DaoString*  DaoProcess_NewWCString( DaoProcess *self, const wchar_t *s, daoint n );
DAO_DLL DaoEnum*    DaoProcess_NewEnum( DaoProcess *self, DaoType *type, int value );
DAO_DLL DaoTuple*   DaoProcess_NewTuple( DaoProcess *self, int count );
DAO_DLL DaoList*    DaoProcess_NewList( DaoProcess *self );

/*
// DaoProcess_NewMap() creates a (hash) map.
*/
DAO_DLL DaoMap*   DaoProcess_NewMap( DaoProcess *self, unsigned int hashing );

/*
// DaoProcess_NewArray() creates a numeric array with element type
// specified by the parameter "type".
*/
DAO_DLL DaoArray* DaoProcess_NewArray( DaoProcess *self, int type );

/*
// DaoProcess_NewVectorSB() creates an integer vector from an array of signed byte;
// DaoProcess_NewVectorUB() creates an integer vector from an array of unsigned byte;
// DaoProcess_NewVectorSS() creates an integer vector from an array of signed short;
// DaoProcess_NewVectorUS() creates an integer vector from an array of unsigned short;
// DaoProcess_NewVectorSI() creates an integer vector from an array of signed int;
// DaoProcess_NewVectorUI() creates an integer vector from an array of unsigned int;
// DaoProcess_NewVectorI()  creates an integer vector from an array of daoint;
// DaoProcess_NewVectorF()  creates an float vector from an array of float;
// DaoProcess_NewVectorD()  creates an double vector from an array of double;
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
DAO_DLL DaoArray* DaoProcess_NewVectorSB( DaoProcess *self, signed   char *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorUB( DaoProcess *self, unsigned char *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorSS( DaoProcess *self, signed   short *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorUS( DaoProcess *self, unsigned short *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorSI( DaoProcess *self, signed   int *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorUI( DaoProcess *self, unsigned int *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorI( DaoProcess *self, daoint *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorF( DaoProcess *self, float  *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorD( DaoProcess *self, double *s, daoint n );

/*
// DaoProcess_NewMatrixSB() creates an integer matrix from a [n x m] matrix of signed byte;
// DaoProcess_NewMatrixUB() creates an integer matrix from a [n x m] matrix of unsigned byte;
// DaoProcess_NewMatrixSS() creates an integer matrix from a [n x m] matrix of signed short;
// DaoProcess_NewMatrixUS() creates an integer matrix from a [n x m] matrix of unsigned short;
// DaoProcess_NewMatrixSI() creates an integer matrix from a [n x m] matrix of signed int;
// DaoProcess_NewMatrixUI() creates an integer matrix from a [n x m] matrix of unsigned int;
// DaoProcess_NewMatrixI() creates an integer matrix from a [n x m] matrix of daoint;
// DaoProcess_NewMatrixF() creates an float matrix from a [n x m] matrix of float;
// DaoProcess_NewMatrixD() creates an double matrix from a [n x m] matrix of double;
*/
DAO_DLL DaoArray* DaoProcess_NewMatrixSB( DaoProcess *self, signed   char **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixUB( DaoProcess *self, unsigned char **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixSS( DaoProcess *self, signed   short **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixUS( DaoProcess *self, unsigned short **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixSI( DaoProcess *self, signed   int **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixUI( DaoProcess *self, unsigned int **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixI( DaoProcess *self, daoint **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixF( DaoProcess *self, float  **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixD( DaoProcess *self, double **s, daoint n, daoint m );

/*
// DaoProcess_NewStream() creates a new stream with specified file.
*/
DAO_DLL DaoStream* DaoProcess_NewStream( DaoProcess *self, FILE *file );

/*
// DaoProcess_NewCdata() creates a new cdata object with specified type and data.
// If and only if "owned" is not zero, the created cdata will be responsible to
// deallocated "data".
*/
DAO_DLL DaoCdata* DaoProcess_NewCdata( DaoProcess *self, DaoType *type, void *data, int owned );

DAO_DLL DaoType* DaoType_GetItemType( DaoType *self, int i );


DAO_DLL void DaoGC_IncRC( DaoValue *p );
DAO_DLL void DaoGC_DecRC( DaoValue *p );
DAO_DLL void DaoGC_ShiftRC( DaoValue *up, DaoValue *down );

/*
// DaoGC_TryDelete() will register the object for collection.
// It is the same as:
//   DaoGC_IncRC( p );
//   DaoGC_DecRC( p );
*/
DAO_DLL void DaoGC_TryDelete( DaoValue *p );

DAO_DLL int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns );

DAO_DLL void Dao_MakePath( DString *base, DString *path );

DAO_DLL void* dao_malloc( size_t size );
DAO_DLL void* dao_calloc( size_t nmemb, size_t size );
DAO_DLL void* dao_realloc( void *ptr, size_t size );
DAO_DLL void  dao_free( void *p );

#ifdef __cplusplus
}
#endif

#endif

