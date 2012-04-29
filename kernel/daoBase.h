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

#ifndef DAO_BASE_H
#define DAO_BASE_H

#include"dao.h"

#if defined (__GNUC__)

#if NO_FENV

#define dao_fe_clear()  0
#define dao_fe_status()  0

#else

#include"fenv.h"
#define dao_fe_clear()  feclearexcept( FE_ALL_EXCEPT )
#define dao_fe_status()  fetestexcept( FE_ALL_EXCEPT )

#endif

#ifdef FE_DIVBYZERO
#define dao_fe_divbyzero()  fetestexcept( FE_DIVBYZERO )
#else
#define dao_fe_divbyzero()  0
#endif

#ifdef FE_UNDERFLOW
#define dao_fe_underflow()  fetestexcept( FE_UNDERFLOW )
#else
#define dao_fe_underflow()  0
#endif

#ifdef FE_OVERFLOW
#define dao_fe_overflow()  fetestexcept( FE_OVERFLOW )
#else
#define dao_fe_overflow()  0
#endif

#ifdef FE_INVALID
#define dao_fe_invalid()  fetestexcept( FE_INVALID )
#else
#define dao_fe_invalid()  0
#endif


#elif defined (_MSC_VER)

#include"float.h"

#define dao_fe_clear()  _clearfp()
#define dao_fe_status()  _status87()
#define dao_fe_divbyzero()  (_status87() & _SW_ZERODIVIDE)
#define dao_fe_underflow()  (_status87() & _SW_UNDERFLOW)
#define dao_fe_overflow()  (_status87() & _SW_OVERFLOW)
#define dao_fe_invalid()  (_status87() & _SW_INVALID)

#define strtoll _strtoi64
#define wcstoll _wcstoi64

#endif

#if defined (WIN32) && !defined (__GNUC__)
#define snprintf _snprintf
#define popen _popen
#define pclose _pclose
#endif


typedef struct DRoutines     DRoutines;

typedef struct DaoCdataCore  DaoCdataCore;
typedef struct DaoTypeKernel DaoTypeKernel;
typedef struct DTypeSpecTree DTypeSpecTree;

typedef struct DaoToken      DaoToken;
typedef struct DaoInode      DaoInode;

typedef struct DaoVmCode     DaoVmCode;
typedef struct DaoVmCodeX    DaoVmCodeX;
typedef struct DaoVmcArray   DaoVmcArray;

typedef struct DaoFuture     DaoFuture;
typedef struct DaoException  DaoException;
typedef struct DaoNameValue  DaoNameValue;
typedef struct DaoConstant   DaoConstant;
typedef struct DaoVariable   DaoVariable;

typedef struct DaoCnode      DaoCnode;
typedef struct DaoOptimizer  DaoOptimizer;

typedef struct DaoMacro      DaoMacro;
typedef struct DaoParser     DaoParser;

typedef struct DaoStackFrame    DaoStackFrame;

#define STRCMP( x, y ) strcmp( (x)->mbs, y )
#define TOKCMP( x, y ) strcmp( (x)->string->mbs, y )

/* bit structure of lookup index:
 * S4P2U12I16 = SSSSPPUUUUUUUUUUUUIIIIIIIIIIIIIIII
 * S: storage; P: permission; U: up/parent; I: index*/
#define LOOKUP_BIND( st, pm, up, id )  (((st)<<28)|((pm)<<26)|((up)<<16)|id)

#define LOOKUP_BIND_LC( id ) ((DAO_LOCAL_CONSTANT<<28)|id)
#define LOOKUP_BIND_GC( id ) ((DAO_GLOBAL_CONSTANT<<28)|id)
#define LOOKUP_BIND_GV( id ) ((DAO_GLOBAL_VARIABLE<<28)|id)

#define LOOKUP_ST( one )  ((one)>>28)
#define LOOKUP_PM( one )  (((one)>>26)&0x3)
#define LOOKUP_UP( one )  (((one)>>16)&0x3ff)
#define LOOKUP_ID( one )  ((unsigned short)((one)&0xffff))

DAO_DLL void* dao_malloc( size_t size );
DAO_DLL void* dao_calloc( size_t nmemb, size_t size );
DAO_DLL void* dao_realloc( void *ptr, size_t size );
DAO_DLL void  dao_free( void *p );

typedef struct DaoConfig  DaoConfig;
struct DaoConfig
{
	short cpu;  /* number of CPU */
	short jit;  /* enable JIT compiling, TODO fix */
	short safe; /* enable safe running mode */
	short typedcode; /* enable typed VM codes */
	short optimize;  /* enable optimization */
	short incompile; /* enable incremental compiling */
	short iscgi;     /* is CGI script */
	short tabspace;  /* number of spaces counted for a tab */
	short chindent;  /* check indentation */
	short mbs; /* MBS only */
	short wcs; /* WCS only */
};

extern DaoConfig daoConfig;


#endif
