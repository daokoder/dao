/*
// Dao Standard Modules
// http://www.daovm.net
//
// Copyright (c) 2006-2012, Limin Fu
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
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __DAO_SYS_H__
#define __DAO_SYS_H__

#include"daoStdtype.h"

typedef struct Dao_Buffer Dao_Buffer;

struct Dao_Buffer
{
	DAO_CDATA_COMMON;

	union {
		void           *pVoid;
		signed   char  *pSChar;
		unsigned char  *pUChar;
		signed   short *pSShort;
		unsigned short *pUShort;
		signed   int   *pSInt;
		unsigned int   *pUInt;
		float          *pFloat;
		double         *pDouble;
	} buffer;
	size_t  size;
	size_t  bufsize;
};

DAO_DLL Dao_Buffer* Dao_Buffer_New( size_t size );
DAO_DLL void Dao_Buffer_Resize( Dao_Buffer *self, size_t size );
DAO_DLL void Dao_Buffer_Delete( Dao_Buffer *self );

#endif
