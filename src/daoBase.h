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

#ifndef DAO_BASE_H
#define DAO_BASE_H

#include"daolib.h"

#if defined (__GNUC__)

#include"fenv.h"

#define dao_fe_clear()  feclearexcept( FE_ALL_EXCEPT )
#define dao_fe_status()  fetestexcept( FE_ALL_EXCEPT )

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

#endif

#if defined (WIN32) && !defined (__GNUC__)
#define snprintf _snprintf
#define popen _popen
#define pclose _pclose
#endif

typedef unsigned short ushort_t;
typedef unsigned long ulong_t;

#ifdef __STRICT_ANSI__
typedef   signed long llong_t;
typedef unsigned long ullong_t;
#else
typedef   signed long long llong_t;
typedef unsigned long long ullong_t;
#endif

typedef struct DaoToken    DaoToken;
typedef struct DaoInode    DaoInode;

typedef struct DaoVmFrame  DaoVmFrame;

typedef struct DaoVmCode       DaoVmCode;
typedef struct DaoVmCodeX      DaoVmCodeX;
typedef struct DaoVmcArray     DaoVmcArray;

typedef struct DaoException    DaoException;

typedef struct DaoMacro        DaoMacro;
typedef struct DaoParser       DaoParser;
typedef struct DaoAsmWriter    DaoAsmWriter;

typedef struct DaoGarbageCollector  DaoGarbageCollector;

typedef DaoBase* (*NewPtr)();
typedef void     (*DelPtr)( DaoBase * );

#define STRCMP( x, y ) strcmp( (x)->mbs, y )
#define TOKCMP( x, y ) strcmp( (x)->string->mbs, y )

void* dao_malloc( size_t size );
void* dao_calloc( size_t nmemb, size_t size );
void* dao_realloc( void *ptr, size_t size );
void  dao_free( void *p );

typedef struct DaoConfig  DaoConfig;
struct DaoConfig
{
  short cpu;  /* number of CPU */
  short jit;  /* enable JIT compiling, TODO fix */
  short safe; /* enable safe running mode */
  short typedcode; /* enable typed VM codes */
  short incompile; /* enable incremental compiling */
  short iscgi;     /* is CGI script */
  short tabspace;  /* number of spaces counted for a tab */
  short chindent;  /* check indentation */
};

extern DaoConfig daoConfig;

typedef struct DaoJitMemory  DaoJitMemory;

struct DaoVmCode
{
  ushort_t code; /* opcode */
  ushort_t a, b, c; /* register ids for operands */
};
struct DaoVmCodeX
{
  ushort_t code; /* opcode */
  ushort_t a, b, c; /* register ids for operands */
  ushort_t line; /* line number in the source file */
  ushort_t level; /* lexical level */
  DString *annot; /* annotation */
};
void DaoVmCode_Print( DaoVmCode self, char *buffer );
void DaoVmCodeX_Print( DaoVmCodeX self, char *buffer );

#endif
