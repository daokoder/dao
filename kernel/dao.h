/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2016, Limin Fu
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

#include<stdio.h>
#include<stdlib.h>
#include<stddef.h>


#define DAO_VERSION "2.0"


#if (defined DAO_WITH_CONCURRENT && !defined DAO_WITH_THREAD)
#  define DAO_WITH_THREAD
#endif


#if defined(MACOSX) && ! defined(UNIX)
#  define UNIX
#endif /* MACOSX */


#ifdef WIN32

#  define DAO_DLL_EXPORT __declspec(dllexport)
#  define DAO_DLL_IMPORT __declspec(dllimport)

#  ifdef DAO_KERNEL
#     define DAO_DLL DAO_DLL_EXPORT
#  else
#     define DAO_DLL DAO_DLL_IMPORT
#  endif

#else /* other system */

#  define DAO_DLL extern
#  define DAO_DLL_EXPORT extern
#  define DAO_DLL_IMPORT extern

#endif /* WIN32 */


#ifndef DAO_API
#  define DAO_API(Linkage,Type,Name,Signature)  DAO_##Linkage##_DLL Type Name Signature
#endif


#ifndef DAO_I64
#  ifdef WIN32
#    define DAO_INT  "Ii"
#    define DAO_I64  "I64i"
#  else
#    define DAO_INT  "ti"
#    define DAO_I64  "lli"
#  endif /* WIN32 */
#endif /* DAO_I64 */


#define DAO_MAX_BASE_TYPES 8


#ifdef __cplusplus
extern "C"{
#endif


enum DaoTypeID
{
	DAO_NONE  = 0,
	DAO_BOOLEAN ,
	DAO_INTEGER ,
	DAO_FLOAT   ,
	DAO_COMPLEX ,
	DAO_STRING ,
	DAO_ENUM  ,
	DAO_ARRAY ,
	DAO_LIST  ,
	DAO_MAP   ,
	DAO_TUPLE ,
	DAO_OBJECT ,
	DAO_CINVALUE ,
	DAO_CSTRUCT ,
	DAO_CDATA  ,
	DAO_CTYPE  ,
	DAO_CLASS  ,
	DAO_CINTYPE ,
	DAO_INTERFACE ,
	DAO_ROUTINE   ,
	DAO_PROCESS ,
	DAO_NAMESPACE ,
	DAO_TYPE ,
	END_CORE_TYPES
};

enum DaoErrorCode
{
	DAO_OK = 0,
	DAO_ERROR ,
	DAO_ERROR_FIELD ,
	DAO_ERROR_FIELD_ABSENT ,
	DAO_ERROR_FIELD_HIDDEN ,
	DAO_ERROR_INDEX ,
	DAO_ERROR_INDEX_RANGE ,
	DAO_ERROR_KEY ,
	DAO_ERROR_KEY_ABSENT ,
	DAO_ERROR_ARG ,
	DAO_ERROR_TYPE ,
	DAO_ERROR_VALUE ,
	DAO_ERROR_FLOAT
};

enum DaoProcessStatus
{
	DAO_PROCESS_FINISHED ,  /* finished normally */
	DAO_PROCESS_ABORTED ,   /* execution aborted */
	DAO_PROCESS_SUSPENDED , /* suspended, by future::wait() etc. */
	DAO_PROCESS_RUNNING ,   /* currently running */
	DAO_PROCESS_STACKED     /* new frame on the process stack */
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
	DAO_OPTION_SANDBOX  = (1<<9), /* -s, --sandbox:      run in sandbox mode; */

	/*
	// DAO_OPTION_IDE:
	// -- disable JIT;
	// -- disable function specialization;
	// -- insert NOP codes for convenient setting up of break points;
	*/
	DAO_OPTION_IDE = (1<<31)
};

enum DaoCtypeWrapOptions
{
	DAO_CTYPE_INVAR  = 1
};



/*
// Define an integer type with size equal to the size of pointers
// on both 32-bits and 64-bits systems.
*/
typedef ptrdiff_t       daoint;
typedef unsigned char   uchar_t;
typedef unsigned short  ushort_t;
typedef unsigned int    uint_t;

typedef char  dao_boolean;

#ifdef DAO_USE_SYS_BIT_INT
typedef ptrdiff_t  dao_integer;
#else
typedef long long  dao_integer;
#endif

typedef double  dao_float;

/* Complex type: */
typedef struct dao_complex
{
	dao_float  real;
	dao_float  imag;
} dao_complex;

typedef struct DString         DString;
typedef struct DArray          DArray;
typedef struct DList           DList;
typedef struct DNode           DNode;
typedef struct DMap            DMap;

typedef struct DaoTypeCore     DaoTypeCore;
typedef struct DaoTypeKernel   DaoTypeKernel;

typedef struct DaoVmCode       DaoVmCode;
typedef struct DaoStackFrame   DaoStackFrame;
typedef struct DaoHandler      DaoHandler;
typedef struct DaoDebugger     DaoDebugger;
typedef struct DaoProfiler     DaoProfiler;
typedef struct DaoParser       DaoParser;

typedef union  DaoValue        DaoValue;
typedef struct DaoNone         DaoNone;
typedef struct DaoBoolean      DaoBoolean;
typedef struct DaoInteger      DaoInteger;
typedef struct DaoFloat        DaoFloat;
typedef struct DaoComplex      DaoComplex;
typedef struct DaoString       DaoString;
typedef struct DaoEnum         DaoEnum;
typedef struct DaoArray        DaoArray;
typedef struct DaoList         DaoList;
typedef struct DaoMap          DaoMap;
typedef struct DaoTuple        DaoTuple;
typedef struct DaoRoutine      DaoRoutine;
typedef struct DaoInterBase    DaoInterBase;
typedef struct DaoInterface    DaoInterface;
typedef struct DaoCinType      DaoCinType;
typedef struct DaoCinValue     DaoCinValue;
typedef struct DaoClass        DaoClass;
typedef struct DaoObject       DaoObject;
typedef struct DaoStream       DaoStream;
typedef struct DaoCtype        DaoCtype;
typedef struct DaoCstruct      DaoCstruct;
typedef struct DaoCdata        DaoCdata;
typedef struct DaoRange        DaoRange;
typedef struct DaoRegex        DaoRegex;
typedef struct DaoException    DaoException;
typedef struct DaoNamespace    DaoNamespace;
typedef struct DaoVmSpace      DaoVmSpace;
typedef struct DaoProcess      DaoProcess;
typedef struct DaoChannel      DaoChannel;
typedef struct DaoType         DaoType;
typedef struct DaoIterator     DaoIterator;

/*
// The following macros can be used instead of the original type names,
// such that the element types, key and value types can be annotated
// explicitly as parts of the type names. Though not compiler checked,
// such use may increase readability of the source code.
*/
#define DArray_(ElementType)       DArray
#define DList_(ElementType)        DList
#define DMap_(KeyType,ValueType)   DMap
#define DHash_(KeyType,ValueType)  DMap

typedef void  (*DaoDeleteFunction)( DaoValue *self );
typedef void  (*DaoCFunction)( DaoProcess *process, DaoValue *params[], int npar );

typedef int (*DaoTokenFilter)( DaoParser *parser );
typedef int (*DaoModuleOnLoad)( DaoVmSpace *vmspace, DaoNamespace *nspace );
typedef int (*DaoCodeInliner)( DaoNamespace *nspace, DString *mode, DString *source, DString *out, int line );

typedef struct DaoNumberEntry    DaoNumberEntry;
typedef struct DaoFunctionEntry  DaoFunctionEntry;
typedef struct DaoVirtualModule  DaoVirtualModule;

struct DaoNumberEntry
{
	const char *name;
	int         type;  /* DAO_INTEGER, DAO_FLOAT or DAO_DOUBLE */
	double      value;
};

struct DaoFunctionEntry
{
	DaoCFunction  fpter;  /* C function pointer; */
	const char   *proto;  /* Function prototype: name( parlist ) => return_type */
};

struct DaoVirtualModule
{
	const char       *name;    /* Name (path + file) for the module; */
	signed int        length;  /* Length of the file; */
	unsigned char    *data;    /* File content; */
	DaoModuleOnLoad   onload;  /* Onload function pointer for C module; */
};



/*
// Type definition structure for Dao types:
//
// Standard operations on the type can be provided as function pointers
// in the type definition.
//
// These function pointers are grouped into pairs per operation, where
// the first is the type checking function used at compiling time, and
// the second is the execution function called at running time to do the
// operation. The type checking function should return the resulting type
// on success and null otherwise.
//
// For user defined types, these function pointers can be set to nulls
// to use the default implementations, which will use operator overloading
// from the member methods to do type checkings and running time executions.
//
// At running time, the executing DaoProcess object is usually passed to the
// execution functions, so that these functions can put the resulting values
// directly on the process stack. Otherwise, they should just return the results,
// and let the VM to put them on the stack if necessary.
//
// However, for the comparison and conversion functions, the DaoProcess could
// be NULL when their host types are used as map and hash map keys.
*/
struct DaoTypeCore
{
	const char  *name;
	/*
	// Type name;
	*/

	DaoTypeCore  *bases[8];
	/*
	// Type core for the base types:
	// Multiple inheritance is supported for interfaces and C++ classes;
	*/

	DaoNumberEntry  *numbers;
	/*
	// Member numbers;
	*/

	DaoFunctionEntry  *methods;
	/*
	// Method definitions: should end with a null item;
	*/

	DaoType* (*CheckGetField)( DaoType *self, DString *field, DaoRoutine *rout );
	DaoValue* (*DoGetField)( DaoValue *self, DString *field, DaoProcess *proc );
	/*
	// Functions for GETF:
	*/

	int (*CheckSetField)( DaoType *self, DString *field, DaoType *value, DaoRoutine *rout );
	int (*DoSetField)( DaoValue *self, DString *field, DaoValue *value, DaoProcess *proc );
	/*
	// Functions for SETF:
	*/

	DaoType*  (*CheckGetItem)( DaoType *self, DaoType *index[], int N, DaoRoutine *rout );
	DaoValue* (*DoGetItem)( DaoValue *self, DaoValue *index[], int N, DaoProcess *proc );
	/*
	// Functions for GETI, GETDI and GETMI:
	*/

	int (*CheckSetItem)( DaoType *self, DaoType *index[], int N, DaoType *value, DaoRoutine *rout );
	int (*DoSetItem)( DaoValue *self, DaoValue *index[], int N, DaoValue *value, DaoProcess *proc );
	/*
	// Functions for SETI, SETDI and SETMI:
	// The should return zero on success and error code otherwise;
	*/

	DaoType* (*CheckUnary)( DaoType *self, DaoVmCode *op, DaoRoutine *rout );
	DaoValue* (*DoUnary)( DaoValue *self, DaoVmCode *op, DaoProcess *proc );
	/*
	// Functions for unary operations:
	// The self parameter is the operand;
	*/

	DaoType* (*CheckBinary)( DaoType *self, DaoVmCode *op, DaoType *operands[2], DaoRoutine *rout );
	DaoValue* (*DoBinary)( DaoValue *self, DaoVmCode *op, DaoValue *operands[2], DaoProcess *proc );
	/*
	// Functions for binary operations:
	// The self parameter is the same as one of the operands;
	*/

	int (*CheckComparison)( DaoType *self, DaoType *other, DaoRoutine *rout );
	int (*DoComparison)( DaoValue *self, DaoValue *other, DaoProcess *proc );
	/*
	// Functions for comparison operations:
	*/

	DaoType*  (*CheckConversion)( DaoType *self, DaoType *type, DaoRoutine *rout );
	DaoValue* (*DoConversion)( DaoValue *self, DaoType *type, int copy, DaoProcess *proc );
	/*
	// Functions for conversion operation:
	// These functions are used for type casting and hashing;
	//
	// The type parameter will be the target type for the conversion;
	// For hashing, the target type will be the integer type, and
	// the resulting integer will be further hashed by the internal
	// hashing function (MurmurHash3) to compute the final hash value.
	//
	// For conversion between wrapped C/C++ types, the conversion function
	// must handle up and down casting properly.
	//
	// If the conversion is used to convert invariables to variables,
	// the copy parameter will be set to one. And non-primitive values
	// are expected to be copied before returning. However, if they are
	// not copied, the Copy() functions (if defined) in their type cores
	// will be used to create copies.
	*/

	DaoType* (*CheckForEach)( DaoType *self, DaoRoutine *rout );
	void (*DoForEach)( DaoValue *self, DaoIterator *iterator, DaoProcess *proc );
	/*
	// Functions for preparing for-in iteration:
	// The check function must return a specialized type of the built-in
	// iterator type;
	// The execution function should initialize the iterator properly;
	// The GetItem functions should handle the case where DaoIterator
	// is the sub-index;
	*/

	void (*Print)( DaoValue *self, DaoStream *stream, DMap *cycmap, DaoProcess *proc );
	/*
	// Function for printing:
	*/

	void (*Slice)( DaoValue *self );
	/*
	// Function to complete slice operation:
	*/

	DaoValue* (*Copy)( DaoValue *self, DaoValue *target );
	/*
	// Function for copying objects of user defined types:
	// If the target object is present in the parameter, the data of self will
	// be copied to the target.
	// Otherwise, a new object with the same data as self should be returned.
	*/

	void (*Delete)( DaoValue *self );
	/*
	// Function for deleting the value:
	*/

	void (*HandleGC)( DaoValue *self, DList *values, DList *lists, DList *maps, int remove );
	/*
	// Function for handling garbage collection:
	// User defined types may hold counted references to Dao data values;
	// This function should export these values to the parameter lists for GC scans;
	//
	// Directly referenced member values should be pushed into "values";
	// Member DLists holding referenced values should be pushed into "lists";
	// Member DMaps holding referenced values should be pushed into "maps";
	//
	// When the user defined value is marked for deletion by GC, the "remove"
	// parameter will be non zero. In the case, references to the member values
	// that are pushed to "values" should be removed by setting them to nulls.
	*/
};

DAO_DLL DaoTypeCore* DaoCstruct_GetDefaultCore();


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

struct DaoHandler
{
	void (*PrintNote)( DaoHandler *self, DaoValue *value );
	void (*PrintError)( DaoHandler *self, const char *msg, const char *file, int start, int end );
};

typedef char* (*ReadLine)( const char *prompt, DString *buffer );
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
DAO_DLL DaoBoolean*   DaoValue_CastBoolean( DaoValue *self );
DAO_DLL DaoInteger*   DaoValue_CastInteger( DaoValue *self );
DAO_DLL DaoFloat*     DaoValue_CastFloat( DaoValue *self );
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
DAO_DLL DaoCinValue*  DaoValue_CastCinValue( DaoValue *self );
DAO_DLL DaoClass*     DaoValue_CastClass( DaoValue *self );
DAO_DLL DaoInterface* DaoValue_CastInterface( DaoValue *self );
DAO_DLL DaoCinType*   DaoValue_CastCinType( DaoValue *self );
DAO_DLL DaoRoutine*   DaoValue_CastRoutine( DaoValue *self );
DAO_DLL DaoProcess*   DaoValue_CastProcess( DaoValue *self );
DAO_DLL DaoNamespace* DaoValue_CastNamespace( DaoValue *self );
DAO_DLL DaoType*      DaoValue_CastType( DaoValue *self );

DAO_DLL DaoValue* DaoValue_MakeNone();

/*
// The following functions will check the type of the DaoValue and
// return the requested data on success. Otherwise return zero or NULL;
*/
DAO_DLL dao_boolean  DaoValue_TryGetBoolean( DaoValue *self );
DAO_DLL dao_integer  DaoValue_TryGetInteger( DaoValue *self );
DAO_DLL dao_float    DaoValue_TryGetFloat( DaoValue *self );
DAO_DLL dao_complex  DaoValue_TryGetComplex( DaoValue *self );

DAO_DLL char*     DaoValue_TryGetChars( DaoValue *self );
DAO_DLL DString*  DaoValue_TryGetString( DaoValue *self );
DAO_DLL int       DaoValue_TryGetEnum( DaoValue *self );
DAO_DLL void*     DaoValue_TryGetArray( DaoValue *self );
DAO_DLL void*     DaoValue_TryGetCdata( DaoValue *self );
DAO_DLL void**    DaoValue_TryGetCdata2( DaoValue *self );

/*
// DaoValue_TryCastCdata() will cast the data of the cdata to the type
// as specified by "totype". This will essentially call a chain of cast
// functions as specified in the "casts" fields of DaoTypeCore structures
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
DAO_DLL DString* DString_New();
DAO_DLL DString* DString_NewChars( const char *mbs );

DAO_DLL DString* DString_Copy( DString *self );
DAO_DLL void DString_Delete( DString *self );
DAO_DLL daoint DString_Size( DString *self );
DAO_DLL void DString_Clear( DString *self );

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
// DString_SetChars() replace the data of the string
// with data specified by null-terminated C string.
*/
DAO_DLL void DString_SetChars( DString *self, const char *chs );

/*
// DString_SetBytes() replace the data of string with data specified
// by character array with "n" characters.
// If "n" is negative, the character array is assumed to be null-terminated.
*/
DAO_DLL void DString_SetBytes( DString *self, const char *data, daoint n );

DAO_DLL char* DString_GetData( DString *self );
DAO_DLL void DString_Chop( DString *self, int utf8 );
DAO_DLL void DString_Trim( DString *self, int head, int tail, int utf8 );

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
DAO_DLL void DString_InsertChars( DString *self, const char *s, daoint i, daoint m, daoint n );
DAO_DLL void DString_InsertChar( DString *self, const char ch, daoint at );
DAO_DLL void DString_Append( DString *self, DString *chs );
DAO_DLL void DString_AppendChar( DString *self, const char ch );
DAO_DLL void DString_AppendWChar( DString *self, size_t ch );
DAO_DLL void DString_AppendChars( DString *self, const char *chs );

/*
// DString_AppendBytes() append "count" number of MBS characters to "self".
*/
DAO_DLL void DString_AppendBytes( DString *self, const char *data, daoint count );

/*
// DString_SubString() retrieves a substring starting from "from" with "count" characters.
// If "count" is negative, all the rest characters are retrieved.
*/
DAO_DLL void DString_SubString( DString *self, DString *sub, daoint from, daoint count );

DAO_DLL daoint DString_Find( DString *self, DString *chs, daoint start );
DAO_DLL daoint DString_FindChars( DString *self, const char *ch, daoint start );
DAO_DLL daoint DString_FindChar( DString *self, char ch, daoint start );
DAO_DLL daoint DString_RFind( DString *self, DString *chs, daoint start );
DAO_DLL daoint DString_RFindChars( DString *self, const char *ch, daoint start );
DAO_DLL daoint DString_RFindChar( DString *self, char ch, daoint start );
DAO_DLL void DString_Assign( DString *left, DString *right );
DAO_DLL int DString_Compare( DString *left, DString *right );

DAO_DLL int DString_Match( DString *self, const char *pat, daoint *start, daoint *end );
DAO_DLL int DString_Change( DString *self, const char *pat, const char *target, int index );


DAO_DLL DaoBoolean* DaoBoolean_New( dao_boolean value );
DAO_DLL dao_boolean DaoBoolean_Get( DaoBoolean *self );
DAO_DLL void        DaoBoolean_Set( DaoBoolean *self, dao_boolean value );

DAO_DLL DaoInteger* DaoInteger_New( dao_integer value );
DAO_DLL dao_integer DaoInteger_Get( DaoInteger *self );
DAO_DLL void        DaoInteger_Set( DaoInteger *self, dao_integer value );

DAO_DLL DaoFloat*   DaoFloat_New( dao_float value );
DAO_DLL dao_float   DaoFloat_Get( DaoFloat *self );
DAO_DLL void        DaoFloat_Set( DaoFloat *self, dao_float value );

DAO_DLL DaoComplex* DaoComplex_New( dao_complex value );
DAO_DLL DaoComplex* DaoComplex_New2( dao_float real, dao_float imag );
DAO_DLL dao_complex DaoComplex_Get( DaoComplex *self );
DAO_DLL void        DaoComplex_Set( DaoComplex *self, dao_complex value );

DAO_DLL DaoString*  DaoString_New();
DAO_DLL DaoString*  DaoString_NewChars( const char *mbs );
DAO_DLL DaoString*  DaoString_NewBytes( const char *bytes, daoint n );

DAO_DLL daoint  DaoString_Size( DaoString *self );

DAO_DLL DString*  DaoString_Get( DaoString *self );
DAO_DLL const char* DaoString_GetChars( DaoString *self );

DAO_DLL void  DaoString_Set( DaoString *self, DString *str );
DAO_DLL void  DaoString_SetChars( DaoString *self, const char *mbs );
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
DAO_DLL int  DaoMap_InsertChars( DaoMap *self, const char *key, DaoValue *value );

DAO_DLL void DaoMap_Erase( DaoMap *self, DaoValue *key );
DAO_DLL void DaoMap_EraseChars( DaoMap *self, const char *key );
DAO_DLL void DaoMap_Clear( DaoMap *self );
DAO_DLL DaoValue* DaoMap_GetValue( DaoMap *self, DaoValue *key  );
DAO_DLL DaoValue* DaoMap_GetValueChars( DaoMap *self, const char *key  );


DAO_DLL DNode* DaoMap_Find( DaoMap *self, DaoValue *key );
DAO_DLL DNode* DaoMap_First( DaoMap *self );
DAO_DLL DNode* DaoMap_Next( DaoMap *self, DNode *iter );
DAO_DLL DaoValue* DNode_Key( DNode *self );
DAO_DLL DaoValue* DNode_Value( DNode *self );



DAO_DLL DaoTuple* DaoTuple_New( int size );
DAO_DLL DaoType* DaoTuple_GetType( DaoTuple *self );
DAO_DLL int DaoTuple_SetType( DaoTuple *self, DaoType *type );
DAO_DLL int DaoTuple_Size( DaoTuple *self );
DAO_DLL int DaoTuple_SetItem( DaoTuple *self, DaoValue *it, int pos );
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
DAO_DLL daoint DaoArray_GetFlatIndex( DaoArray *self, daoint *indexes );

/*
// The following functions convert the data in the internal buffer to
// the specified numeric type, without altering the numeric type of the array.
//
// DaoArray_FromXyz() should be called after DaoArray_ToXyz() to restore
// the consistency between the numeric type of the array and the data buffer.
*/
DAO_DLL dao_integer*    DaoArray_ToInteger( DaoArray *self );
DAO_DLL dao_float*      DaoArray_ToFloat( DaoArray *self );
DAO_DLL signed   char*  DaoArray_ToSInt8( DaoArray *self );
DAO_DLL unsigned char*  DaoArray_ToUInt8( DaoArray *self );
DAO_DLL signed   short* DaoArray_ToSInt16( DaoArray *self );
DAO_DLL unsigned short* DaoArray_ToUInt16( DaoArray *self );
DAO_DLL signed   int*   DaoArray_ToSInt32( DaoArray *self );
DAO_DLL unsigned int*   DaoArray_ToUInt32( DaoArray *self );
DAO_DLL float*          DaoArray_ToFloat32( DaoArray *self );
DAO_DLL double*         DaoArray_ToFloat64( DaoArray *self );

/*
// The following functions DaoArray_FromXyz() re-interprets the internal data
// buffer as of "Xyz" type, and convert them to the numeric type of the array.
*/
DAO_DLL void DaoArray_FromInteger( DaoArray *self );
DAO_DLL void DaoArray_FromFloat( DaoArray *self );
DAO_DLL void DaoArray_FromSInt8( DaoArray *self );
DAO_DLL void DaoArray_FromUInt8( DaoArray *self );
DAO_DLL void DaoArray_FromSInt16( DaoArray *self );
DAO_DLL void DaoArray_FromUInt16( DaoArray *self );
DAO_DLL void DaoArray_FromUInt32( DaoArray *self );
DAO_DLL void DaoArray_FromSInt32( DaoArray *self );
DAO_DLL void DaoArray_FromFloat32( DaoArray *self );
DAO_DLL void DaoArray_FromFloat64( DaoArray *self );

DAO_DLL void* DaoArray_GetBuffer( DaoArray *self );
DAO_DLL void  DaoArray_SetBuffer( DaoArray *self, void *buffer, daoint size );



/*
// See description in "daoRoutine.h".
*/
DAO_DLL DaoRoutine* DaoRoutine_Resolve( DaoRoutine *self, DaoValue *svalue, DaoType *stype, DaoValue *values[], DaoType *types[], int count, int callmode );
DAO_DLL DaoRoutine* DaoRoutine_ResolveByValue( DaoRoutine *self, DaoValue *svalue, DaoValue *values[], int count );
DAO_DLL DaoRoutine* DaoRoutine_ResolveByType( DaoRoutine *self, DaoType *stype, DaoType *types[], int count );

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
DAO_DLL void DaoStream_WriteInt( DaoStream *self, dao_integer val );
DAO_DLL void DaoStream_WriteFloat( DaoStream *self, double val );
DAO_DLL void DaoStream_WriteString( DaoStream *self, DString *val );
DAO_DLL void DaoStream_WriteLocalString( DaoStream *self, DString *val );
DAO_DLL void DaoStream_WriteChars( DaoStream *self, const char *val );
DAO_DLL void DaoStream_WritePointer( DaoStream *self, void *val );
DAO_DLL int DaoStream_ReadLine( DaoStream *self, DString *line );
DAO_DLL int DaoFile_ReadLine( FILE *fin, DString *line );
DAO_DLL int DaoFile_ReadAll( FILE *fin, DString *all, int close );
DAO_DLL int DaoFile_WriteString( FILE *fout, DString *str );



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
DAO_DLL DaoNamespace* DaoProcess_Compile( DaoProcess *self, DaoNamespace *ns, const char *s );
DAO_DLL DaoValue* DaoProcess_Eval( DaoProcess *self, DaoNamespace *ns, const char *s );
DAO_DLL int DaoProcess_Call( DaoProcess *s, DaoRoutine *f, DaoValue *o, DaoValue *p[], int n );
DAO_DLL DaoRoutine* DaoProcess_ActiveRoutine( DaoProcess *self );
DAO_DLL DaoValue* DaoProcess_GetReturned( DaoProcess *self );
DAO_DLL DaoType*  DaoProcess_GetReturnType( DaoProcess *self );
DAO_DLL DaoRegex* DaoProcess_MakeRegex( DaoProcess *self, DString *patt );
DAO_DLL DaoCdata* DaoProcess_MakeCdata( DaoProcess *self, DaoType *type, void *data, int owned );
DAO_DLL void DaoProcess_SetStdio( DaoProcess *self, DaoStream *stream );
DAO_DLL void DaoProcess_RaiseException( DaoProcess *self, const char *type, const char *info, DaoValue *data );
DAO_DLL void DaoProcess_RaiseException2( DaoProcess *self, const char *type, const char *info, char *args );
DAO_DLL void DaoProcess_RaiseWarning( DaoProcess *self, const char *type, const char *info );
DAO_DLL void DaoProcess_RaiseError( DaoProcess *self, const char *type, const char *info );

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
DAO_DLL DaoNone*     DaoProcess_PutNone( DaoProcess *self );
DAO_DLL dao_boolean* DaoProcess_PutBoolean( DaoProcess *self, dao_boolean value );
DAO_DLL dao_integer* DaoProcess_PutInteger( DaoProcess *self, dao_integer value );
DAO_DLL dao_float*   DaoProcess_PutFloat( DaoProcess *self, dao_float value );
DAO_DLL dao_complex* DaoProcess_PutComplex( DaoProcess *self, dao_complex value );
DAO_DLL DString*   DaoProcess_PutChars( DaoProcess *self, const char *mbs );
DAO_DLL DString*   DaoProcess_PutBytes( DaoProcess *self, const char *bytes, daoint N );
DAO_DLL DString*   DaoProcess_PutString( DaoProcess *self, DString *str );
DAO_DLL DaoEnum*   DaoProcess_PutEnum( DaoProcess *self, const char *symbols );
DAO_DLL DaoArray*  DaoProcess_PutArray( DaoProcess *self );
DAO_DLL DaoList*   DaoProcess_PutList( DaoProcess *self );
DAO_DLL DaoMap*    DaoProcess_PutMap( DaoProcess *self, unsigned int hashing );
DAO_DLL DaoValue*  DaoProcess_PutValue( DaoProcess *self, DaoValue *value );


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
//   DaoProcess_NewString( proc, "abc", -1 );
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
DAO_DLL void DaoNamespace_AddConstNumbers( DaoNamespace *self, DaoNumberEntry *items );
DAO_DLL void DaoNamespace_AddConstValue( DaoNamespace *self, const char *name, DaoValue *data );
DAO_DLL void DaoNamespace_AddValue( DaoNamespace *self, const char *name, DaoValue *d, const char *type);
DAO_DLL DaoValue* DaoNamespace_FindData( DaoNamespace *self, const char *name );
DAO_DLL DaoType* DaoNamespace_DefineType( DaoNamespace *self, const char *type, const char *alias );
DAO_DLL DaoType* DaoNamespace_WrapType( DaoNamespace *self, DaoTypeCore *core, int tid, int options );
DAO_DLL DaoType* DaoNamespace_WrapInterface( DaoNamespace *self, DaoTypeCore *core );
DAO_DLL DaoType* DaoNamespace_WrapCinType( DaoNamespace *self, DaoTypeCore *c, DaoType *a, DaoType *t );
DAO_DLL DaoRoutine* DaoNamespace_WrapFunction( DaoNamespace *self, DaoCFunction fp, const char *proto );
DAO_DLL int DaoNamespace_AliasTypes( DaoNamespace *self, const char *alias[] );
DAO_DLL int DaoNamespace_WrapTypes( DaoNamespace *self, DaoTypeCore *cores[] );
DAO_DLL int DaoNamespace_WrapFunctions( DaoNamespace *self, DaoFunctionEntry fd[] );
DAO_DLL int DaoNamespace_GetOptions( DaoNamespace *self );
DAO_DLL void DaoNamespace_SetOptions( DaoNamespace *self, int options );
DAO_DLL void DaoNamespace_AddCodeInliner( DaoNamespace *self, const char *name, DaoCodeInliner fp );



DAO_DLL DaoVmSpace* DaoVmSpace_New();
DAO_DLL DaoVmSpace* DaoVmSpace_MainVmSpace();
DAO_DLL int DaoVmSpace_ParseOptions( DaoVmSpace *self, const char *options );
DAO_DLL void DaoVmSpace_SetOptions( DaoVmSpace *self, int options );
DAO_DLL int  DaoVmSpace_GetOptions( DaoVmSpace *self );

DAO_DLL int DaoVmSpace_RunMain( DaoVmSpace *self, const char *file );
DAO_DLL DaoValue* DaoVmSpace_Eval( DaoVmSpace *self, const char *source );
DAO_DLL DaoNamespace* DaoVmSpace_Load( DaoVmSpace *self, const char *file );
DAO_DLL DaoNamespace* DaoVmSpace_LinkModule( DaoVmSpace *self, DaoNamespace *ns, const char *mod );
DAO_DLL DaoNamespace* DaoVmSpace_GetNamespace( DaoVmSpace *self, const char *name );
DAO_DLL DaoNamespace* DaoVmSpace_MainNamespace( DaoVmSpace *self );
DAO_DLL DaoProcess* DaoVmSpace_MainProcess( DaoVmSpace *self );
DAO_DLL DaoProcess* DaoVmSpace_AcquireProcess( DaoVmSpace *self );
DAO_DLL void DaoVmSpace_ReleaseProcess( DaoVmSpace *self, DaoProcess *proc );

DAO_DLL DaoStream* DaoVmSpace_StdioStream( DaoVmSpace *self );
DAO_DLL DaoStream* DaoVmSpace_ErrorStream( DaoVmSpace *self );

DAO_DLL DaoStream* DaoVmSpace_SetStdio( DaoVmSpace *self, DaoStream *stream );
DAO_DLL DaoStream* DaoVmSpace_SetStdError( DaoVmSpace *self, DaoStream *stream );
DAO_DLL DaoDebugger* DaoVmSpace_SetDebugger( DaoVmSpace *self, DaoDebugger *debugger );
DAO_DLL DaoProfiler* DaoVmSpace_SetProfiler( DaoVmSpace *self, DaoProfiler *profiler );
DAO_DLL DaoHandler*  DaoVmSpace_SetHandler( DaoVmSpace *self, DaoHandler *handler );
DAO_DLL void DaoVmSpace_ReadLine( DaoVmSpace *self, ReadLine fptr );
DAO_DLL void DaoVmSpace_AddHistory( DaoVmSpace *self, AddHistory fptr );

DAO_DLL int DaoVmSpace_AddPlugin( DaoVmSpace *self, DString *name, DaoNamespace *nspace );

DAO_DLL int DaoVmSpace_AddVirtualModules( DaoVmSpace *self, DaoVirtualModule modules[] );
DAO_DLL void DaoVmSpace_AddVirtualModule( DaoVmSpace *self, DaoVirtualModule *module );
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
// reference counting of the created value if it used in local or temporary
// variables, namely, no need to call DaoGC_IncRC() and DaoGC_DecRC().
// However if it is used in any variable where DaoGC_IncRC() or DaoGC_DecRC()
// is called, ensure they are called in pair (when assigning to a variable,
// another option is to use DaoGC_Assign()).
*/
DAO_DLL DaoNone*    DaoProcess_NewNone( DaoProcess *self );
DAO_DLL DaoBoolean* DaoProcess_NewBoolean( DaoProcess *self, dao_boolean v );
DAO_DLL DaoInteger* DaoProcess_NewInteger( DaoProcess *self, dao_integer v );
DAO_DLL DaoFloat*   DaoProcess_NewFloat( DaoProcess *self, dao_float v );
DAO_DLL DaoComplex* DaoProcess_NewComplex( DaoProcess *self, dao_complex v );
/*
// Negative "n" indicates a null-terminated string:
*/
DAO_DLL DaoString*  DaoProcess_NewString( DaoProcess *self, const char *s, daoint n );
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
// DaoProcess_NewCdata() creates a new cdata object with specified type and data.
// If and only if "owned" is not zero, the created cdata will be responsible to
// deallocated "data".
*/
DAO_DLL DaoCdata* DaoProcess_NewCdata( DaoProcess *self, DaoType *type, void *data, int owned );

DAO_DLL DaoType* DaoType_GetItemType( DaoType *self, int i );


DAO_DLL void DaoGC_IncRC( DaoValue *p );
DAO_DLL void DaoGC_DecRC( DaoValue *p );
DAO_DLL void DaoGC_Assign( DaoValue **dest, DaoValue *src );

/*
// DaoGC_TryDelete() will register the object for collection.
// It is the same as:
//   DaoGC_IncRC( p );
//   DaoGC_DecRC( p );
*/
DAO_DLL void DaoGC_TryDelete( DaoValue *p );

DAO_DLL int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns );

DAO_DLL void* dao_malloc( size_t size );
DAO_DLL void* dao_calloc( size_t nmemb, size_t size );
DAO_DLL void* dao_realloc( void *ptr, size_t size );
DAO_DLL void  dao_free( void *p );
DAO_DLL void  dao_abort( const char *error );

#ifdef __cplusplus
}
#endif

#endif

