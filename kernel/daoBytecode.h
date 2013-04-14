/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2013, Limin Fu
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

#ifndef DAO_BYTECODE_H
#define DAO_BYTECODE_H

#include "daoStdtype.h"


#define DAO_BC_SIGNATURE  "\33Dao\1\2\r\n"


typedef struct DaoByteEncoder  DaoByteEncoder;
typedef struct DaoByteDecoder  DaoByteDecoder;

struct DaoByteEncoder
{
	DaoNamespace  *nspace;

	daoint    valueCount;
	daoint    constCount;
	daoint    varCount;

	DString  *header;
	DString  *source;
	DString  *modules;
	DString  *identifiers;
	DString  *declarations;
	DString  *types;
	DString  *values;
	DString  *constants;
	DString  *variables;
	DString  *glbtypes;
	DString  *interfaces;
	DString  *classes;
	DString  *routines;

	DString  *tmpBytes;
	DString  *valueBytes;
	DArray   *lookups;      /* <daoint> */
	DArray   *names;        /* <DString*> (not managed); */

	DArray   *objects;      /* <DaoValue*> */
	DArray   *lines;

	DArray   *hosts;
	DMap     *handled;

	DMap  *mapLookupHost;   /* <DaoValue*,DaoValue*> */
	DMap  *mapLookupName;   /* <DaoValue*,DString*> */

	DMap  *mapIdentifiers;  /* <DString*,daoint> */
	DMap  *mapDeclarations; /* <DaoValue*,daoint> */
	DMap  *mapTypes;        /* <DaoType*,daoint> */
	DMap  *mapValues;       /* <DaoValue*,daoint> */
	DMap  *mapValueBytes;   /* <DString*,daoint> */
	DMap  *mapInterfaces;   /* <DaoInterface*,daoint> */
	DMap  *mapClasses;      /* <DaoClass*,daoint> */
	DMap  *mapRoutines;     /* <DaoRoutine*,daoint> */
};

DaoByteEncoder* DaoByteEncoder_New();
void DaoByteEncoder_Delete( DaoByteEncoder *self );

void DaoByteEncoder_Encode( DaoByteEncoder *self, DaoNamespace *nspace, DString *output );



struct DaoByteDecoder
{
	DaoVmSpace    *vmspace;
	DaoNamespace  *nspace;

	DArray   *identifiers;  /* <DString*> */
	DArray   *namespaces;   /* <DaoNamespace*> */
	DArray   *declarations; /* <DaoValue*> */
	DArray   *types;        /* <DaoType*> */
	DArray   *values;       /* <DString*>: encoded values; */
	DArray   *interfaces;   /* <DaoInterface*> */
	DArray   *classes;      /* <DaoClass*> */
	DArray   *routines;     /* <DaoRoutine*> */

	DArray   *array;
	DString  *string;
	DMap     *valueTypes;
	DMap     *map;

	int  intSize;

	uchar_t  *codes;
	uchar_t  *end;
	uchar_t  *error;
};


DaoByteDecoder* DaoByteDecoder_New( DaoVmSpace *vmspace );
void DaoByteDecoder_Delete( DaoByteDecoder *self );

int DaoByteDecoder_Decode( DaoByteDecoder *self, DString *input, DaoNamespace *nspace );

#endif
