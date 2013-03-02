/*
// Dao Make Tool
// http://www.daovm.net
//
// Copyright (c) 2013, Limin Fu
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

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include<math.h>
#include"dao.h"
#include"daoStdtype.h"
#include"daoNamespace.h"
#include"daoProcess.h"


static DaoVmSpace *vmSpace = NULL;


typedef struct DaoMakeUnit     DaoMakeUnit;
typedef struct DaoMakeObjects  DaoMakeObjects;
typedef struct DaoMakeTarget   DaoMakeTarget;
typedef struct DaoMakeProject  DaoMakeProject;


enum DaoMakeTargetTypes
{
	DAOMAKE_EXECUTABLE ,
	DAOMAKE_SHAREDLIB ,
	DAOMAKE_STATICLIB
};

const char *const daomake_suffix_keys[] =
{
	"EXE-SUFFIX" ,
	"DLL-SUFFIX" ,
	"LIB-SUFFIX" 
};



struct DaoMakeUnit
{
	DAO_CSTRUCT_COMMON;

	DArray  *definitions;
	DArray  *includePaths;
	DArray  *linkingPaths;
	DArray  *compilingFlags;
	DArray  *linkingFlags;
	DArray  *usingPackages;
};


struct DaoMakeObjects
{
	DaoMakeUnit  base;

	DArray  *headers;
	DArray  *sources;
};

struct DaoMakeTarget
{
	DaoMakeUnit  base;

	DString  *name;
	DArray   *objects;
	uchar_t   ttype;
	uchar_t   dynamicLinking;
	uchar_t   dynamicExporting;
};

struct DaoMakeProject
{
	DaoMakeUnit  base;

	DString  *sourceName;
	DString  *sourcePath;
	DString  *projectName;

	DArray  *targets;
	DArray  *subProjects;

	DArray  *compilingFlags;
	DArray  *linkingFlags;

	DMap  *headerMacros;   /* HEADERS = header1.h header2.h; */
	DMap  *cflagsMacros;   /* CFLAGS = ...; */
	DMap  *lflagsMacros;   /* LFLAGS = ...; */
	DMap  *objectRules;    /* TARGET: DEPS \n\t COMMAND; */
	DMap  *objectsMacros;  /* OBJECTS = ...; */
	DMap  *targetRules;    /* TARGET: DEPS \n\t COMMAND; */

	DString  *mbs;
	DArray   *strings;
	uint_t    usedStrings;
};



static DaoMap *daomake_settings = NULL;
static DaoMap *daomake_projects = NULL;

static DaoType *daomake_type_unit = NULL;
static DaoType *daomake_type_objects = NULL;
static DaoType *daomake_type_target  = NULL;
static DaoType *daomake_type_project = NULL;



void DaoMakeUnit_Init( DaoMakeUnit *self, DaoType *type )
{
	DaoCstruct_Init( (DaoCstruct*)self, type );
	self->definitions = DArray_New(D_STRING);
	self->includePaths = DArray_New(D_STRING);
	self->linkingPaths = DArray_New(D_STRING);
	self->compilingFlags = DArray_New(D_STRING);
	self->linkingFlags = DArray_New(D_STRING);
	self->usingPackages = DArray_New(D_VALUE);
}
void DaoMakeUnit_Free( DaoMakeUnit *self )
{
	DaoCstruct_Free( (DaoCstruct*) self );
	DArray_Delete( self->definitions );
	DArray_Delete( self->includePaths );
	DArray_Delete( self->linkingPaths );
	DArray_Delete( self->compilingFlags );
	DArray_Delete( self->linkingFlags );
	DArray_Delete( self->usingPackages );
}

DaoMakeObjects* DaoMakeObjects_New()
{
	DaoMakeObjects *self = (DaoMakeObjects*) dao_calloc( 1, sizeof(DaoMakeObjects) );
	DaoMakeUnit_Init( (DaoMakeUnit*) & self->base, daomake_type_objects );
	self->headers = DArray_New(D_STRING);
	self->sources = DArray_New(D_STRING);
	return self;
}
void DaoMakeObjects_Delete( DaoMakeObjects *self )
{
	DaoMakeUnit_Free( (DaoMakeUnit*) & self->base );
	DArray_Delete( self->headers );
	DArray_Delete( self->sources );
	dao_free( self );
}
DaoMakeTarget* DaoMakeTarget_New()
{
	DaoMakeTarget *self = (DaoMakeTarget*) dao_calloc( 1, sizeof(DaoMakeTarget) );
	DaoMakeUnit_Init( (DaoMakeUnit*) & self->base, daomake_type_target );
	self->name = DString_New(1);
	self->objects = DArray_New(D_VALUE);
	self->ttype = DAOMAKE_EXECUTABLE;
	self->dynamicLinking = 0;
	self->dynamicExporting = 0;
	return self;
}
void DaoMakeTarget_Delete( DaoMakeTarget *self )
{
	DaoMakeUnit_Free( (DaoMakeUnit*) & self->base );
	DString_Delete( self->name );
	DArray_Delete( self->objects );
	dao_free( self );
}
DaoMakeProject* DaoMakeProject_New()
{
	DaoMakeProject *self = (DaoMakeProject*) dao_calloc( 1, sizeof(DaoMakeProject) );
	DaoMakeUnit_Init( (DaoMakeUnit*) & self->base, daomake_type_project );
	self->sourceName = DString_New(1);
	self->sourcePath = DString_New(1);
	self->projectName = DString_New(1);

	self->targets = DArray_New(D_VALUE);
	self->subProjects = DArray_New(D_VALUE);

	self->compilingFlags = DArray_New(D_STRING);
	self->linkingFlags = DArray_New(D_STRING);

	self->headerMacros = DMap_New(D_STRING,D_STRING);
	self->cflagsMacros = DMap_New(D_STRING,D_STRING);
	self->lflagsMacros = DMap_New(D_STRING,D_STRING);
	self->objectRules = DMap_New(D_STRING,D_STRING);
	self->objectsMacros = DMap_New(D_STRING,D_STRING);
	self->targetRules = DMap_New(D_STRING,D_STRING);

	self->mbs = DString_New(1);
	self->strings = DArray_New(D_STRING);
	self->usedStrings = 0;
	DArray_Append( self->strings, self->mbs );
	return self;
}
void DaoMakeProject_Delete( DaoMakeProject *self )
{
	DaoMakeUnit_Free( (DaoMakeUnit*) & self->base );
	DString_Delete( self->sourceName );
	DString_Delete( self->sourcePath );
	DString_Delete( self->projectName );

	DArray_Delete( self->targets );
	DArray_Delete( self->subProjects );

	DArray_Delete( self->compilingFlags );
	DArray_Delete( self->linkingFlags );

	DMap_Delete( self->headerMacros );
	DMap_Delete( self->cflagsMacros );
	DMap_Delete( self->lflagsMacros );
	DMap_Delete( self->objectRules );
	DMap_Delete( self->objectsMacros );
	DMap_Delete( self->targetRules );

	DString_Delete( self->mbs );
	DArray_Delete( self->strings );
	dao_free( self );
}




static void MD5_Append( DString *md5, uint32_t h )
{
	const char *hex = "0123456789abcdef";
	uint32_t k;
	DString_Reserve( md5, md5->size + 8 );
	k = (h>> 0)&0xff;  md5->mbs[md5->size++] = hex[k>>4];  md5->mbs[md5->size++] = hex[k&0xf];
	k = (h>> 8)&0xff;  md5->mbs[md5->size++] = hex[k>>4];  md5->mbs[md5->size++] = hex[k&0xf];
	k = (h>>16)&0xff;  md5->mbs[md5->size++] = hex[k>>4];  md5->mbs[md5->size++] = hex[k&0xf];
	k = (h>>24)&0xff;  md5->mbs[md5->size++] = hex[k>>4];  md5->mbs[md5->size++] = hex[k&0xf];
	md5->mbs[md5->size] = '\0';
}
static void MD5_Update( uint32_t H[4], uint32_t W[16], uint32_t K[64] )
{
	static uint32_t R[64] = {
		7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
		5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
		4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
		6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
	};
	uint32_t A = H[0];
	uint32_t B = H[1];
	uint32_t C = H[2];
	uint32_t D = H[3];
	uint32_t k;
	for(k=0; k<16; k++){
		uint32_t f = (B & C) | ((~B) & D);
		uint32_t g = k;
		uint32_t t = D;
		uint32_t x = A + f + K[k] + W[g];
		D = C;
		C = B;
		B = B + ((x << R[k]) | (x >> (32-R[k])));
		A = t;
	}
	for(k=16; k<32; k++){
		uint32_t f = (D & B) | ((~D) & C);
		uint32_t g = (k*5 + 1) % 16;
		uint32_t t = D;
		uint32_t x = A + f + K[k] + W[g];
		D = C;
		C = B;
		B = B + ((x << R[k]) | (x >> (32-R[k])));
		A = t;
	}
	for(k=32; k<48; k++){
		uint32_t f = B ^ C ^ D;
		uint32_t g = (k*3 + 5) % 16;
		uint32_t t = D;
		uint32_t x = A + f + K[k] + W[g];
		D = C;
		C = B;
		B = B + ((x << R[k]) | (x >> (32-R[k])));
		A = t;
	}
	for(k=48; k<64; k++){
		uint32_t f = C ^ (B | (~D));
		uint32_t g = (k*7) % 16;
		uint32_t t = D;
		uint32_t x = A + f + K[k] + W[g];
		D = C;
		C = B;
		B = B + ((x << R[k]) | (x >> (32-R[k])));
		A = t;
	}
	H[0] += A;
	H[1] += B;
	H[2] += C;
	H[3] += D;
}

void DString_MD5( DString *self, DString *md5 )
{
	DString *padding = md5;
	uint64_t i, k, m, n, twop32 = ((uint64_t)1)<<32;
	uint32_t H[4] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 };
	uint32_t K[64], W[16];
	int32_t size = self->size;
	int32_t chunks = self->size / 64;
	uint8_t *data = (uint8_t*) self->mbs;

	if( self->wcs ){
		data = (uint8_t*) self->wcs;
		size *= sizeof(wchar_t);
		chunks = size / 64;
	}

	for(i=0; i<64; i++) K[i] = (uint32_t) floor( fabs( sin(i+1) ) * twop32 );
	for(i=0; i<chunks; i++){
		for(k=0; k<16; k++){
			uint32_t b = i*64 + k*4;
			uint32_t m = data[b];
			m |= ((uint32_t)data[b+1])<<8;
			m |= ((uint32_t)data[b+2])<<16;
			m |= ((uint32_t)data[b+3])<<24;
			W[k] = m;
		}
		MD5_Update( H, W, K );
	}
	DString_ToMBS( padding );
	DString_Reserve( padding, 128 );
	padding->size = 64;
	m = size - chunks*64;
	if( m ) memcpy( padding->mbs, data + chunks*64, m*sizeof(char) );
	if( m + 8 > 64 ) padding->size = 128;
	chunks = padding->size / 64;

	data = (uint8_t*) padding->mbs;
	data[m] = 1<<7; // first bit 1 followed by bit 0s;
	for(i=m+1; i<padding->size-8; i++) data[i] = 0;
	n = size * 8;
	// last 64 bits to store the string size in little endian:
	data[i] = n & 0xff;
	data[i+1] = (n >> 8) & 0xff;
	data[i+2] = (n >> 16) & 0xff;
	data[i+3] = (n >> 24) & 0xff;
	data[i+4] = (n >> 32) & 0xff;
	data[i+5] = (n >> 40) & 0xff;
	data[i+6] = (n >> 48) & 0xff;
	data[i+7] = (n >> 56) & 0xff;
	for(i=0; i<chunks; i++){
		for(k=0; k<16; k++){
			uint32_t b = i*64 + k*4;
			uint32_t m = data[b];
			m |= ((uint32_t)data[b+1])<<8;
			m |= ((uint32_t)data[b+2])<<16;
			m |= ((uint32_t)data[b+3])<<24;
			W[k] = m;
		}
		MD5_Update( H, W, K );
	}
	md5->size = 0;
	MD5_Append( md5, H[0] );
	MD5_Append( md5, H[1] );
	MD5_Append( md5, H[2] );
	MD5_Append( md5, H[3] );
}



DString* DaoMake_GetSettingValue( const char *key )
{
	DaoValue *value = DaoMap_GetValueMBS( daomake_settings, key );
	if( value == NULL ) return NULL;
	return DaoValue_TryGetString( value );
}
void DaoMakeTarget_SetName( DaoMakeTarget *self, DString *name )
{
	DString_Reset( self->name, 0 );
	DString_Append( self->name, name );
	if( daomake_settings ){
		DString *suffix = DaoMake_GetSettingValue( daomake_suffix_keys[ self->ttype ] );
		if( suffix ) DString_Append( self->name, suffix );
	}
}




void DaoMakeUnit_MakeCompilingFlags( DaoMakeUnit *self, DString *cflags )
{
	daoint i, j;
	DString_Reset( cflags, 0 );
	for(i=0; i<self->compilingFlags->size; ++i){
		DString_Append( cflags, self->compilingFlags->items.pString[i] );
	}
	for(i=0; i<self->includePaths->size; ++i){
		DString *path = self->includePaths->items.pString[i];
		DString_AppendMBS( cflags, " -I" );
		DString_Append( cflags, path );
	}
	for(i=0; i<self->definitions->size; i+=2){
		DString *definition = self->definitions->items.pString[i];
		DString *value = self->definitions->items.pString[i+1];
		DString_AppendMBS( cflags, " -D" );
		DString_Append( cflags, definition );
		if( value->size ){
			wchar_t ch = value->wcs ? value->wcs[0] : value->mbs[0];
			int nonumber = iswdigit( ch ) == 0;
			DString_AppendChar( cflags, '=' );
			if( nonumber ) DString_AppendMBS( cflags, "\\\"" );
			DString_Append( cflags, value );
			if( nonumber ) DString_AppendMBS( cflags, "\\\"" );
		}
	}
	for(i=0; i<self->usingPackages->size; ++i){
		DaoMakeProject *project = self->usingPackages->items.pVoid[i];
		for(j=0; j<project->compilingFlags->size; ++j){
			DString *flag = project->compilingFlags->items.pString[j];
			if( cflags->size ) DString_AppendChar( cflags, ' ' );
			DString_Append( cflags, flag );
		}
	}
}
void DaoMakeUnit_MakeLinkingFlags( DaoMakeUnit *self, DString *lflags )
{
	daoint i, j;
	DString_Reset( lflags, 0 );
	for(i=0; i<self->linkingPaths->size; ++i){
		DString *path = self->linkingPaths->items.pString[i];
		DString_AppendMBS( lflags, " -L" );
		DString_Append( lflags, path );
	}
	for(i=0; i<self->linkingFlags->size; ++i){
		if( lflags->size ) DString_AppendChar( lflags, ' ' );
		DString_Append( lflags, self->linkingFlags->items.pString[i] );
	}
	for(i=0; i<self->usingPackages->size; ++i){
		DaoMakeProject *project = self->usingPackages->items.pVoid[i];
		for(j=0; j<project->linkingFlags->size; ++j){
			DString *flag = project->linkingFlags->items.pString[j];
			if( lflags->size ) DString_AppendChar( lflags, ' ' );
			DString_Append( lflags, flag );
		}
	}
}


DString* DaoMakeProject_GetBufferString( DaoMakeProject *self )
{
	if( self->usedStrings >= self->strings->size )
		DArray_Append( self->strings, self->strings->items.pString[0] );
	self->usedStrings += 1;
	self->strings->items.pString[ self->usedStrings - 1 ]->size = 0;
	return self->strings->items.pString[ self->usedStrings - 1 ];
}


DString* DaoMakeProject_SubMD5( DaoMakeProject *self, DString *data )
{
	DString *md5 = self->mbs;
	DString_MD5( data, md5 );
	DString_ToUpper( md5 );
	DString_Reset( md5, 8 ); /* TODO: customizable; */
	return md5;
}


/* Return macro name: */
DString* DaoMakeProject_MakeHeaderMacro( DaoMakeProject *self, DaoMakeObjects *objects )
{
	DString *files = DaoMakeProject_GetBufferString( self );
	DString *macro = DaoMakeProject_GetBufferString( self );
	DString *md5 = self->mbs;
	DNode *it;
	daoint i;

	DString_Reset( files, 0 );
	for(i=0; i<objects->headers->size; ++i){
		if( i ) DString_AppendChar( files, ' ' );
		DString_Append( files, objects->headers->items.pString[i] );
	}
	md5 = DaoMakeProject_SubMD5( self, files );

	DString_Reset( macro, 0 );
	DString_AppendMBS( macro, "HEADERS_" );
	DString_Append( macro, md5 );

	it = DMap_Find( self->headerMacros, macro );
	if( it ){
		self->usedStrings -= 2;
		return it->key.pString;
	}

	it = DMap_Insert( self->headerMacros, macro, macro );
	DString_AppendMBS( it->value.pString, " = " );
	DString_Append( it->value.pString, files );

	self->usedStrings -= 2;
	return it->key.pString;
}

DString* DaoMakeProject_MakeSimpleMacro( DaoMakeProject *self, DMap *macros, DString *value, const char *prefix )
{
	DString *name = DaoMakeProject_GetBufferString( self );
	DString *md5 = DaoMakeProject_SubMD5( self, value );
	DNode *it;

	DString_Reset( name, 0 );
	DString_AppendMBS( name, prefix );
	DString_AppendChar( name, '_' );
	DString_Append( name, md5 );

	it = DMap_Find( macros, name );
	if( it ){
		self->usedStrings -= 1;
		return it->key.pString;
	}

	it = DMap_Insert( self->headerMacros, name, name );
	DString_AppendMBS( it->value.pString, " =" );
	DString_Append( it->value.pString, value );
	self->usedStrings -= 1;
	return it->key.pString;
}
DString* DaoMakeProject_MakeCFlagsMacro( DaoMakeProject *self, DString *flags )
{
	return DaoMakeProject_MakeSimpleMacro( self, self->cflagsMacros, flags, "CFLAGS" );
}
DString* DaoMakeProject_MakeLFlagsMacro( DaoMakeProject *self, DString *flags )
{
	return DaoMakeProject_MakeSimpleMacro( self, self->lflagsMacros, flags, "LFLAGS" );
}

/* Return object file name: */
DString* DaoMakeProject_MakeObjectRule( DaoMakeProject *self, DaoMakeTarget *target, DaoMakeObjects *objects, DString *source )
{
	DString *cflags = DaoMakeProject_GetBufferString( self );
	DString *cflag = DaoMakeProject_GetBufferString( self );
	DString *signature = DaoMakeProject_GetBufferString( self );
	DString *md5 = self->mbs;
	DNode *it;

	DString_Reset( cflags, 0 );
	DaoMakeUnit_MakeCompilingFlags( & self->base, cflag );
	DString_Append( cflags, cflag );

	DaoMakeUnit_MakeCompilingFlags( & target->base, cflag );
	DString_Append( cflags, cflag );

	DaoMakeUnit_MakeCompilingFlags( & objects->base, cflag );
	DString_Append( cflags, cflag );

	DString_Assign( signature, cflags );
	DString_AppendChar( signature, ' ' );
	DString_Append( signature, source );

	md5 = DaoMakeProject_SubMD5( self, signature );

	/* Unique (quasi) target name: */
	DString_Reset( signature, 0 );
	DString_Append( signature, source );
	DString_AppendChar( signature, '.' );
	DString_Append( signature, md5 );
	DString_AppendMBS( signature, ".o" );

	it = DMap_Find( self->objectRules, signature );
	if( it ){
		self->usedStrings -= 3;
		return it->key.pString;
	}

	it = DMap_Insert( self->headerMacros, signature, signature );
	DString_AppendMBS( it->value.pString, ": " );
	DString_Append( it->value.pString, source );
	DString_AppendMBS( it->value.pString, " $(" );
	DString_Append( it->value.pString, DaoMakeProject_MakeHeaderMacro( self, objects ) );
	DString_AppendMBS( it->value.pString, ")\n\t$(CC) $(" ); // TODO: C++
	DString_Append( it->value.pString, DaoMakeProject_MakeCFlagsMacro( self, cflags ) );
	DString_AppendMBS( it->value.pString, ") -c " );
	DString_Append( it->value.pString, source );
	DString_AppendMBS( it->value.pString, " -o " );
	DString_Append( it->value.pString, signature );

	self->usedStrings -= 3;
	return it->key.pString;
}

/* Return objects macro name: */
DString* DaoMakeProject_MakeObjectsMacro( DaoMakeProject *self, DaoMakeTarget *target, DaoMakeObjects *objects )
{
	DString *objs = DaoMakeProject_GetBufferString( self );
	daoint i;

	DString_Reset( objs, 0 );
	for(i=0; i<objects->sources->size; ++i){
		DString *source = objects->sources->items.pString[i];
		DString *obj = DaoMakeProject_MakeObjectRule( self, target, objects, source );
		if( i ) DString_AppendChar( objs, ' ' );
		DString_Append( objs, obj );
	}
	objs = DaoMakeProject_MakeSimpleMacro( self, self->objectsMacros, objs, "OBJECTS" );
	self->usedStrings -= 1;
	return objs;
}

DString* DaoMakeProject_MakeTargetRule( DaoMakeProject *self, DaoMakeTarget *target )
{
	DString *tname = DaoMakeProject_GetBufferString( self );
	DString *lflags = DaoMakeProject_GetBufferString( self );
	DString *lflag = DaoMakeProject_GetBufferString( self );
	DString *signature = DaoMakeProject_GetBufferString( self );
	DString *rule = DaoMakeProject_GetBufferString( self );
	DString *objs = DaoMakeProject_GetBufferString( self );
	DString *suffix = DaoMake_GetSettingValue( daomake_suffix_keys[ target->ttype ] );
	DString *lflagMacro;
	DNode *it;
	daoint i;

	DString_Reset( tname, 0 );
	DString_Append( tname, target->name );
	if( suffix ) DString_Append( tname, suffix );

	DaoMakeUnit_MakeLinkingFlags( & self->base, lflag );
	DString_Append( lflags, lflag );

	DaoMakeUnit_MakeLinkingFlags( & target->base, lflag );
	DString_Append( lflags, lflag );

	DString_Reset( rule, 0 );
	DString_Reset( objs, 0 );
	DString_Append( rule, tname );
	DString_AppendMBS( rule, ": " );
	for(i=0; i<target->objects->size; ++i){
		DaoMakeObjects *objects = (DaoMakeObjects*) target->objects->items.pVoid[i];
		DString *objmacro = DaoMakeProject_MakeObjectsMacro( self, target, objects );

		DaoMakeUnit_MakeLinkingFlags( & objects->base, lflag );
		DString_Append( lflags, lflag );
		if( i ) DString_AppendChar( objs, ' ' );
		DString_AppendMBS( objs, "$(" );
		DString_Append( objs, objmacro );
		DString_AppendChar( objs, ')' );
	}
	DString_Append( rule, objs );
	if( target->ttype == DAOMAKE_STATICLIB ){
		DString *arc = DaoMake_GetSettingValue( "AR" );
		DString_AppendMBS( rule, "\n\t" );
		if( arc ) DString_Append( rule, arc );
		DString_AppendMBS( rule, " " );
		DString_Append( rule, tname );
		DString_AppendMBS( rule, " " );
		DString_Append( rule, objs );
	}else{
		if( target->ttype == DAOMAKE_EXECUTABLE ){
		}else if( target->ttype == DAOMAKE_SHAREDLIB ){
			DString *flag = DaoMake_GetSettingValue( "DLL-FLAG" );
			if( flag ){
				DString_AppendChar( lflags, ' ' );
				DString_Append( lflags, flag );
			}
		}
		lflagMacro = DaoMakeProject_MakeLFlagsMacro( self, lflags );
		DString_AppendMBS( rule, "\n\t$(CC) " ); // TODO: C++

		DString_Append( rule, objs );

		DString_AppendMBS( rule, " $(" );
		DString_Append( rule, lflagMacro );
		DString_AppendMBS( rule, ") " );

		DString_AppendMBS( rule, " -o " );
		DString_Append( rule, tname );
	}

	it = DMap_Insert( self->targetRules, tname, rule );
	self->usedStrings -= 6;
	return it->key.pString;
}

void DaoMakeProject_MakeFile( DaoMakeProject *self, DString *makefile )
{
	DNode *it;
	daoint i;

	DString_Reset( makefile, 0 );
	if( self->targets->size == 0 && self->subProjects->size == 0 ) return;

	DString_AppendMBS( makefile, "all:" );
	for(i=0; i<self->targets->size; ++i){
		DaoMakeTarget *target = (DaoMakeTarget*) self->targets->items.pVoid[i];
		DString *ruleName = DaoMakeProject_MakeTargetRule( self, target );
		DString_AppendChar( makefile, ' ' );
		DString_Append( makefile, ruleName );
	}
	if( self->subProjects->size ) DString_AppendMBS( makefile, " SUBPROJECTS" );
	DString_AppendChar( makefile, '\n' );

	for(it=DMap_First(self->headerMacros); it; it=DMap_Next(self->headerMacros,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendChar( makefile, '\n' );
	}

	for(it=DMap_First(self->cflagsMacros); it; it=DMap_Next(self->cflagsMacros,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendChar( makefile, '\n' );
	}

	for(it=DMap_First(self->lflagsMacros); it; it=DMap_Next(self->lflagsMacros,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendChar( makefile, '\n' );
	}

	for(it=DMap_First(self->objectRules); it; it=DMap_Next(self->objectRules,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendMBS( makefile, "\n\n" );
	}

	for(it=DMap_First(self->objectsMacros); it; it=DMap_Next(self->objectsMacros,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendMBS( makefile, "\n\n" );
	}

	for(it=DMap_First(self->targetRules); it; it=DMap_Next(self->targetRules,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendChar( makefile, '\n' );
	}

	if( self->subProjects->size ) DString_AppendMBS( makefile, "\nSUBPROJECTS:" );
	for(i=0; i<self->subProjects->size; ++i){
		DaoMakeProject *project = (DaoMakeProject*) self->subProjects->items.pVoid[i];
		DString_AppendMBS( makefile, "\n\tcd " );
		DString_Append( makefile, project->sourcePath );
		DString_AppendMBS( makefile, " && $(MAKE)" );
	}
}





static void DArray_ImportStringList( DArray *self, DaoList *list )
{
	int i, size = DaoList_Size( list );
	for(i=0; i<size; ++i){
		DaoValue *value = DaoList_GetItem( list, i );
		DArray_Append( self, DaoValue_TryGetString( value ) );
	}
}
static void DArray_ImportStringParameters( DArray *self, DaoValue *p[], int N )
{
	int i;
	for(i=0; i<N; ++i){
		DString *path = DaoValue_TryGetString( p[i] );
		if( path ) DArray_Append( self, path );
	}
}



static void UNIT_AddDefinition( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DArray_Append( self->definitions, DaoValue_TryGetString( p[1] ) );
	DArray_Append( self->definitions, DaoValue_TryGetString( p[2] ) );
}
static void UNIT_AddIncludePath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DArray_ImportStringParameters( self->includePaths, p+1, N-1 );
}
static void UNIT_AddLinkingPath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DArray_ImportStringParameters( self->linkingPaths, p+1, N-1 );
}
static void UNIT_AddCompilingFlag( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DArray_ImportStringParameters( self->compilingFlags, p+1, N-1 );
}
static void UNIT_AddLinkingFlag( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DArray_ImportStringParameters( self->linkingFlags, p+1, N-1 );
}
static void UNIT_UsePackage( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	int i;
	for(i=1; i<N; ++i){
		DaoMakeProject *pkg = (DaoMakeProject*)DaoValue_CastCdata( p[i], daomake_type_project );
		if( pkg == NULL ) DArray_Append( self->usingPackages, pkg );
	}
}
static DaoFuncItem DaoMakeUnitMeths[]=
{
	{ UNIT_AddDefinition,     "AddDefinition( self : Unit, name : string, value = '' )" },
	{ UNIT_AddIncludePath,    "AddIncludePath( self : Unit, path : string, ... )" },
	{ UNIT_AddLinkingPath,    "AddLinkingPath( self : Unit, path : string, ... )" },
	{ UNIT_AddCompilingFlag,  "AddCompilingFlag( self : Unit, flag : string, ... )" },
	{ UNIT_AddLinkingFlag,    "AddLinkingFlag( self : Unit, flag : string, ... )" },
	{ UNIT_UsePackage,        "UsePackage( self : Unit, pkg : Project, ... )" },
	{ NULL, NULL }
};
DaoTypeBase DaoMakeUnit_Typer =
{
	"Unit", NULL, NULL, (DaoFuncItem*) DaoMakeUnitMeths, {0}, {0}, NULL, NULL
};




static void OBJECTS_New( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeObjects *self = DaoMakeObjects_New();

	DArray_ImportStringList( self->sources, (DaoList*) p[0] );
	DArray_ImportStringList( self->headers, (DaoList*) p[1] );
	DaoProcess_PutValue( proc, (DaoValue*) self );
}
static void OBJECTS_AddHeaders( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeObjects *self = (DaoMakeObjects*) p[0];
	DArray_ImportStringList( self->headers, (DaoList*) p[1] );
}
static void OBJECTS_AddSources( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeObjects *self = (DaoMakeObjects*) p[0];
	DArray_ImportStringList( self->sources, (DaoList*) p[1] );
}
static DaoFuncItem DaoMakeObjectsMeths[]=
{
	{ OBJECTS_New,   "Objects( sources : list<string>, headers : list<string> ) => Objects" },
	{ OBJECTS_AddHeaders,  "AddHeaders( file : string, ... )" },
	{ OBJECTS_AddSources,  "AddSources( file : string, ... )" },
	{ NULL, NULL }
};
DaoTypeBase DaoMakeObjects_Typer =
{
	"Objects", NULL, NULL, (DaoFuncItem*) DaoMakeObjectsMeths,
	{ & DaoMakeUnit_Typer, NULL }, {0},
	(FuncPtrDel)DaoMakeObjects_Delete, NULL
};




static void TARGET_AddObjects( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeTarget *self = (DaoMakeTarget*) p[0];
	int i;
	for(i=1; i<N; ++i) DArray_Append( self->objects, p[i] );
}
static void TARGET_EnableDynamicExporting( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeTarget *self = (DaoMakeTarget*) p[0];
	self->dynamicExporting = 1;
}
static void TARGET_EnableDynamicLinking( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeTarget *self = (DaoMakeTarget*) p[0];
	self->dynamicLinking = 1;
}
static DaoFuncItem DaoMakeTargetMeths[]=
{
	{ TARGET_AddObjects,  "AddObjects( self : Target, objects : Objects, ... )" },
	{ TARGET_EnableDynamicExporting,  "EnableDynamicExporting( self : Target )" },
	{ TARGET_EnableDynamicLinking,    "EnableDynamicLinking( self : Target )" },
	{ NULL, NULL }
};
DaoTypeBase DaoMakeTarget_Typer =
{
	"Target", NULL, NULL, (DaoFuncItem*) DaoMakeTargetMeths,
	{ & DaoMakeUnit_Typer, NULL }, {0},
	(FuncPtrDel)DaoMakeTarget_Delete, NULL
};




static void PROJECT_New( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *name = DaoValue_TryGetString( p[0] );
	DaoNamespace *ns = proc->activeNamespace;
	DaoMakeProject *self = DaoMakeProject_New();

	DaoProcess_PutValue( proc, (DaoValue*) self );
	DString_Assign( self->sourceName, ns->file );
	DString_Assign( self->sourcePath, ns->path );
	DString_Assign( self->projectName, name );
	DaoMap_InsertMBS( daomake_projects, name->mbs, (DaoValue*) self );
}
static void PROJECT_AddTarget( DaoProcess *proc, DaoValue *p[], int N, int ttype )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DaoMakeTarget *target = DaoMakeTarget_New();
	int i;
	target->ttype = ttype;
	DString_Assign( target->name, DaoValue_TryGetString( p[1] ) );
	for(i=2; i<N; ++i) DArray_Append( target->objects, p[i] );
	DaoProcess_PutValue( proc, (DaoValue*) target );
	DArray_Append( self->targets, (DaoValue*) target );
}
static void PROJECT_AddEXE( DaoProcess *proc, DaoValue *p[], int N )
{
	PROJECT_AddTarget( proc, p, N, DAOMAKE_EXECUTABLE );
}
static void PROJECT_AddDLL( DaoProcess *proc, DaoValue *p[], int N )
{
	PROJECT_AddTarget( proc, p, N, DAOMAKE_SHAREDLIB );
}
static void PROJECT_AddARC( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *arc = DaoMake_GetSettingValue( "AR" );
	if( arc == NULL ) DaoProcess_RaiseException( proc, DAO_ERROR, "The platform does not support static library!" );
	PROJECT_AddTarget( proc, p, N, DAOMAKE_STATICLIB );
}
static DaoFuncItem DaoMakeProjectMeths[]=
{
	{ PROJECT_New,     "Project( name : string ) => Project" },
	{ PROJECT_AddEXE,  "AddExecutable( self : Project, name : string, objs : Objects, ... ) =>Target" },
	{ PROJECT_AddDLL,  "AddSharedLibrary( self : Project, name : string, objs : Objects, ... ) =>Target" },
	{ PROJECT_AddARC,  "AddStaticLibrary( self : Project, name : string, objs : Objects, ... ) =>Target" },
	{ NULL, NULL }
};
DaoTypeBase DaoMakeProject_Typer =
{
	"Project", NULL, NULL, (DaoFuncItem*) DaoMakeProjectMeths,
	{ & DaoMakeUnit_Typer, NULL }, {0},
	(FuncPtrDel)DaoMakeProject_Delete, NULL
};




static void DAOMAKE_FindPackage( DaoProcess *proc, DaoValue *p[], int N )
{
}
static void DAOMAKE_FindFile( DaoProcess *proc, DaoValue *p[], int N )
{
}
static void DAOMAKE_IsPlatform( DaoProcess *proc, const char *name )
{
	DString *value = DaoMake_GetSettingValue( name );
	DaoProcess_PutInteger( proc, value != NULL );
}

static void DAOMAKE_IsUnix( DaoProcess *proc, DaoValue *p[], int N )
{
	DAOMAKE_IsPlatform( proc, "UNIX" );
}
static void DAOMAKE_IsLinux( DaoProcess *proc, DaoValue *p[], int N )
{
	DAOMAKE_IsPlatform( proc, "LINUX" );
}
static void DAOMAKE_IsMacOSX( DaoProcess *proc, DaoValue *p[], int N )
{
	DAOMAKE_IsPlatform( proc, "MACOSX" );
}
static void DAOMAKE_IsFreeBSD( DaoProcess *proc, DaoValue *p[], int N )
{
	DAOMAKE_IsPlatform( proc, "FREEBSD" );
}
static void DAOMAKE_IsMinix( DaoProcess *proc, DaoValue *p[], int N )
{
	DAOMAKE_IsPlatform( proc, "MINIX" );
}

static void DAOMAKE_IsWin32( DaoProcess *proc, DaoValue *p[], int N )
{
	DAOMAKE_IsPlatform( proc, "WIN32" );
}
static void DAOMAKE_IsMinGW( DaoProcess *proc, DaoValue *p[], int N )
{
	DAOMAKE_IsPlatform( proc, "MINGW32" );
}

static DaoFuncItem DaoMakeMeths[] =
{
	{ DAOMAKE_FindPackage, "FindPackage( name : string ) => Project" },
	{ DAOMAKE_FindFile,    "FindFile( file : string, hints : list<string> ) => string" },

	{ DAOMAKE_IsUnix,      "IsUnix() => int" },
	{ DAOMAKE_IsLinux,     "IsLinux() => int" },
	{ DAOMAKE_IsMacOSX,    "IsMacOSX() => int" },
	{ DAOMAKE_IsFreeBSD,   "IsFreeBSD() => int" },
	{ DAOMAKE_IsMinix,     "IsMinix() => int" },

	{ DAOMAKE_IsWin32,     "IsWin32() => int" },
	{ DAOMAKE_IsMinGW,     "IsMinGW() => int" },
	{ NULL, NULL }
};



int main( int argc, char **argv )
{
	int i, k;
	char *platform = NULL;
	char *mode = NULL;
	char *makefile = "makefile.dao";
	DaoNamespace *nspace;
	DString *source;
	DString *name;
	DNode *it;

	vmSpace = DaoInit( argv[0] );

	for(i=1; i<argc; i++){
		char *arg = argv[i];
		if( strcmp( arg, "--platform" ) == 0 ){
			if( (i + 1) == argc ) goto ErrorMissingArgValue;
			platform = argv[++i];
		}else if( strcmp( arg, "--mode" ) == 0 ){
			if( (i + 1) == argc ) goto ErrorMissingArgValue;
			mode = argv[++i];
		}else if( arg[0] == '-' ){
			fprintf( stderr, "Error: unknown argument \"%s\"!\n", arg );
			return 1;
		}else if( (i + 1) == argc ){
			makefile = argv[i];
		}
		continue;
ErrorMissingArgValue:
		fprintf( stderr, "Error: missing argument value for \"%s\"!\n", arg );
		return 1;
	}

	/* Use no hashing: the same string will be hashed differently in MBS and WCS! */
	daomake_settings = DaoMap_New(0);
	daomake_projects = DaoMap_New(0);
	DaoGC_IncRC( (DaoValue*) daomake_settings );
	DaoGC_IncRC( (DaoValue*) daomake_projects );

	nspace = DaoVmSpace_GetNamespace( vmSpace, "DaoMake" );
	daomake_type_unit    = DaoNamespace_WrapType( nspace, & DaoMakeUnit_Typer, 0 );
	daomake_type_objects = DaoNamespace_WrapType( nspace, & DaoMakeObjects_Typer, 0 );
	daomake_type_target  = DaoNamespace_WrapType( nspace, & DaoMakeTarget_Typer, 0 );
	daomake_type_project = DaoNamespace_WrapType( nspace, & DaoMakeProject_Typer, 0 );
	DaoNamespace_WrapFunctions( nspace, DaoMakeMeths );
	DaoNamespace_AddValue( nspace, "Settings", (DaoValue*) daomake_settings, "map<string,string>" );

	name = DString_New(1);
	if( platform ){
		DString_SetMBS( name, "platforms/" );
		DString_AppendMBS( name, platform );
		DString_AppendMBS( name, ".dao" );
		DaoVmSpace_Load( vmSpace, name->mbs );
	}

	/* Start execution. */
	k = ! DaoVmSpace_RunMain( vmSpace, makefile );

	source = DString_New(1);
	for(it=DaoMap_First(daomake_projects); it; it=DaoMap_Next(daomake_projects,it)){
		DaoMakeProject *project = (DaoMakeProject*) it->value.pVoid;
		FILE *fout;
		if( project->targets->size == 0 ) continue;
		DString_Reset( name, 0 );
		DString_Append( name, project->sourcePath );
		DString_AppendMBS( name, "/Makefile.dmk" );
		fout = fopen( name->mbs, "w+" );
		DaoMakeProject_MakeFile( project, source );
		DaoFile_WriteString( fout, source );
		fclose( fout );
	}
	DString_Delete( name );
	DString_Delete( source );

	DaoGC_DecRC( (DaoValue*) daomake_settings );
	DaoGC_DecRC( (DaoValue*) daomake_projects );

	DaoQuit();
	return k;
}
