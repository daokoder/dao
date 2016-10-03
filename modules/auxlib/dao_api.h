/*
// Dao Standard Modules
// http://daoscript.org
//
// Copyright (c) 2015, Limin Fu
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



#ifdef DAO_API
#  undef DAO_API
#endif


#ifdef DAO_API_INIT
#  define DAO_API(Linkage,Type,Name,Signature)  _##Name = Name
/*
; printf( "%i %p\n", __LINE__, & _##Name )
 */
#else
#  define DAO_API(Linkage,Type,Name,Signature)  DAO_##Linkage##_DLL Type (*_##Name) Signature
#endif


#ifdef DAO_HAS_RANDOM
#include"../random/dao_random.h"
#endif


#ifdef DAO_HAS_STREAM
#include"../stream/dao_stream.h"
#endif


#ifdef DAO_HAS_TIME
#include"../time/dao_time.h"
#endif


#ifdef DAO_HAS_DECIMAL
#include"../decimal/dao_decimal.h"
#endif


#ifdef DAO_HAS_ZIP
#include"../zip/dao_zip.h"
#endif


#ifdef DAO_HAS_CRYPTO
#include"../crypto/dao_crypto.h"
#endif


#ifdef DAO_HAS_IMAGE
#include"../image/source/dao_image.h"
#endif
