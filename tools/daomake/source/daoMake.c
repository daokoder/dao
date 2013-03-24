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
#include"daoValue.h"
#include"daoStdtype.h"
#include"daoNamespace.h"
#include"daoProcess.h"
#include"daoStream.h"
#include"daoVmspace.h"

#ifdef WIN32

#include"io.h"
#ifdef _MSC_VER
#define mkdir _mkdir
#endif

#endif

#ifdef UNIX
#include<sys/stat.h>
#endif


static DaoVmSpace *vmSpace = NULL;


typedef struct DaoMakeUnit     DaoMakeUnit;
typedef struct DaoMakeObjects  DaoMakeObjects;
typedef struct DaoMakeTarget   DaoMakeTarget;
typedef struct DaoMakeProject  DaoMakeProject;


enum DaoMakeModes
{
	DAOMAKE_RELEASE ,
	DAOMAKE_DEBUG ,
	DAOMAKE_PROFILE
};

enum DaoMakeTargetTypes
{
	DAOMAKE_EXECUTABLE ,
	DAOMAKE_SHAREDLIB ,
	DAOMAKE_STATICLIB ,
	DAOMAKE_TESTING ,
	DAOMAKE_COMMAND ,
	DAOMAKE_DIRECTORY
};

const char *const daomake_test_sumfile = "daotest_result_summary.txt";

const char *const daomake_mode_keys[] =
{
	"RELEASE-CFLAG" ,
	"RELEASE-LFLAG" ,
	"DEBUG-CFLAG" ,
	"DEBUG-LFLAG" ,
	"PROFILE-CFLAG" ,
	"PROFILE-LFLAG"
};

const char *const daomake_prefix_keys[] =
{
	"" ,
	"DLL-PREFIX" ,
	"LIB-PREFIX" ,
	"" ,
	""
};

const char *const daomake_suffix_keys[] =
{
	"EXE-SUFFIX" ,
	"DLL-SUFFIX" ,
	"LIB-SUFFIX" ,
	"" ,
	""
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
	DArray  *usingPackages2;
	DArray  *staticLibNames;
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
	DArray   *tests;
	DArray   *commands;
	DArray   *depends;
	DString  *testMacro;
	DString  *install;
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
	uchar_t   generateFinder;

	DArray   *targets;
	DArray   *variables;
	DArray   *installs;
	DMap     *tests;

	DArray   *compilingFlags;
	DArray   *linkingFlags;
	DMap     *exportPaths;
	DMap     *exportStaticLibs;

	/*
	// In the following maps:
	// the keys are the macro names or the target names;
	// the values are the entire macro or rule;
	*/
	DMap     *headerMacros;   /* HEADERS = header1.h header2.h; */
	DMap     *cflagsMacros;   /* CFLAGS = ...; */
	DMap     *lflagsMacros;   /* LFLAGS = ...; */
	DMap     *objectRules;    /* OBJECT: DEPS \n\t COMMAND; */
	DMap     *objectsMacros;  /* OBJECTS = ...; */
	DMap     *testRules;      /* TEST: DEPS \n\t COMMAND; */
	DMap     *testsMacros;    /* TESTS = ...; */
	DArray   *targetRules;    /* TARGET: DEPS \n\t COMMAND; */

	DMap     *signatures;
	uint_t    signature;

	DString  *mbs;
	DArray   *strings;
	uint_t    usedStrings;

	DMap     *mapStringInt;
};



static DaoMap  *daomake_projects = NULL;
static DaoMap  *daomake_settings = NULL;
static DaoMap  *daomake_compilers = NULL;
static DaoMap  *daomake_linkers = NULL;
static DaoList *daomake_includes = NULL;

static DMap *daomake_boolean_options = NULL;
static DMap *daomake_string_options = NULL;

static DMap *daomake_makefile_paths = NULL;

static DaoType *daomake_type_unit = NULL;
static DaoType *daomake_type_objects = NULL;
static DaoType *daomake_type_target  = NULL;
static DaoType *daomake_type_project = NULL;

static DString *daomake_main_source_path = NULL;
static DString *daomake_test_tool = NULL;
static DString *daomake_test_tool_option = NULL;

static char *daomake_makefile_suffix = "";
static int daomake_build_mode = DAOMAKE_RELEASE;
static int daomake_out_of_source = 0;
static int daomake_reset_cache = 0;
static int daomake_test_count = 0;




void DaoMakeUnit_Init( DaoMakeUnit *self, DaoType *type )
{
	DaoCstruct_Init( (DaoCstruct*)self, type );
	self->definitions = DArray_New(D_STRING);
	self->includePaths = DArray_New(D_STRING);
	self->linkingPaths = DArray_New(D_STRING);
	self->compilingFlags = DArray_New(D_STRING);
	self->linkingFlags = DArray_New(D_STRING);
	self->usingPackages = DArray_New(D_VALUE);
	self->usingPackages2 = DArray_New(D_VALUE);
	self->staticLibNames = DArray_New(D_STRING);
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
	DArray_Delete( self->usingPackages2 );
	DArray_Delete( self->staticLibNames );
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
	self->tests   = DArray_New(D_STRING);
	self->commands = DArray_New(D_STRING);
	self->depends = DArray_New(D_VALUE);
	self->testMacro = DString_New(1);
	self->install = DString_New(1);
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
	DArray_Delete( self->tests );
	DArray_Delete( self->commands );
	DArray_Delete( self->depends );
	DString_Delete( self->testMacro );
	DString_Delete( self->install );
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
	self->variables = DArray_New(D_STRING);
	self->installs = DArray_New(D_VALUE);
	self->tests = DMap_New(D_STRING,D_ARRAY);

	self->compilingFlags = DArray_New(D_STRING);
	self->linkingFlags = DArray_New(D_STRING);
	self->exportPaths = DMap_New(D_STRING,D_STRING);
	self->exportStaticLibs = DMap_New(D_STRING,D_STRING);

	self->headerMacros = DMap_New(D_STRING,D_STRING);
	self->cflagsMacros = DMap_New(D_STRING,D_STRING);
	self->lflagsMacros = DMap_New(D_STRING,D_STRING);
	self->objectRules = DMap_New(D_STRING,D_STRING);
	self->objectsMacros = DMap_New(D_STRING,D_STRING);
	self->testRules = DMap_New(D_STRING,D_STRING);
	self->testsMacros = DMap_New(D_STRING,D_STRING);
	self->targetRules = DArray_New(D_STRING);
	self->signatures = DMap_New(D_STRING,D_STRING);
	self->signature = 4;

	self->mbs = DString_New(1);
	self->strings = DArray_New(D_STRING);
	self->usedStrings = 0;
	DArray_Append( self->strings, self->mbs );

	self->mapStringInt = DMap_New(D_STRING,0);
	return self;
}
void DaoMakeProject_Delete( DaoMakeProject *self )
{
	DaoMakeUnit_Free( (DaoMakeUnit*) & self->base );
	DString_Delete( self->sourceName );
	DString_Delete( self->sourcePath );
	DString_Delete( self->projectName );

	DArray_Delete( self->targets );
	DArray_Delete( self->variables );
	DArray_Delete( self->installs );
	DMap_Delete( self->tests );

	DArray_Delete( self->compilingFlags );
	DArray_Delete( self->linkingFlags );
	DMap_Delete( self->exportPaths );
	DMap_Delete( self->exportStaticLibs );

	DMap_Delete( self->headerMacros );
	DMap_Delete( self->cflagsMacros );
	DMap_Delete( self->lflagsMacros );
	DMap_Delete( self->objectRules );
	DMap_Delete( self->objectsMacros );
	DMap_Delete( self->testRules );
	DMap_Delete( self->testsMacros );
	DMap_Delete( self->signatures );
	DArray_Delete( self->targetRules );

	DString_Delete( self->mbs );
	DArray_Delete( self->strings );
	DMap_Delete( self->mapStringInt );
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
static void DString_AppendGap( DString *self )
{
	if( self->mbs ){
		char *mbs = self->mbs;
		daoint size = self->size;
		while( size >= 2 && isspace( mbs[size-2] ) && isspace( mbs[size-1] ) ) size -= 1;
		self->mbs[size] = '\0';
		self->size = size;
		if( size && isspace( mbs[size-1] ) == 0 ) DString_AppendChar( self, ' ' );
	}else{
		wchar_t *wcs = self->wcs;
		daoint size = self->size;
		while( size >= 2 && iswspace( wcs[size-2] ) && iswspace( wcs[size-1] ) ) size -= 1;
		self->wcs[size] = '\0';
		self->size = size;
		if( size && iswspace( wcs[size-1] ) == 0 ) DString_AppendWChar( self, L' ' );
	}
}



DString* DaoMake_GetSettingValue( const char *key )
{
	DaoValue *value = DaoMap_GetValueMBS( daomake_settings, key );
	if( value == NULL ) return NULL;
	return DaoValue_TryGetString( value );
}
void DaoMake_MakeDir( const char *dir )
{
	mkdir( dir, 0777 );
}
DString* DaoMake_FindFile( DString *file, DaoList *hints )
{
	DString *res = NULL;
	DString *fname = DString_New(1);
	daoint i, size = DaoList_Size( hints );
	for(i=0; i<size; ++i){
		DString *path = DaoValue_TryGetString( DaoList_GetItem( hints, i ) );
		if( path == NULL || path->size == 0 ) continue;
		DString_Reset( fname, 0 );
		DString_Append( fname, path );
		if( fname->mbs[fname->size-1] != '/' ) DString_AppendChar( fname, '/' );
		DString_Append( fname, file );
		if( Dao_IsFile( fname->mbs ) ){
			res = path;
			break;
		}
	}
	DString_Delete( fname );
	return res;
}
void DaoMake_MakeOutOfSourcePath( DString *path )
{
	DString *binpath = vmSpace->startPath;
	daoint k = binpath->size + 1;

	if( daomake_out_of_source == 0 ) return;
	DString_Insert( path, binpath, 0, daomake_main_source_path->size, binpath->size );

	while( k < path->size ){
		while( k < path->size && path->mbs[k] != '/' ) k += 1;
		if( k == path->size ) break;
		path->mbs[k] = '\0';
		if( Dao_IsDir( path->mbs ) == 0 && Dao_IsFile( path->mbs ) == 0 ){
			DaoMake_MakeDir( path->mbs );
		}
		path->mbs[k] = '/';
		k += 1;
	}
}
void Dao_MakePath( DString *base, DString *path );
void DaoMake_MakePath( DString *base, DString *path )
{
	DString_ToMBS( path );
	if( path->size == 0 ) return;
#ifdef WIN32
	if( path->size >= 2 && isalpha( path->mbs[0] ) && path->mbs[1] == ':' ) return;
#else
	if( path->mbs[0] == '/' ) return;
#endif
	Dao_MakePath( base, path );
}
void DaoMakeProject_MakeSourcePath( DaoMakeProject *self, DString *path )
{
	DaoMake_MakePath( self->sourcePath, path );
}
void DaoMakeProject_MakeBinaryPath( DaoMakeProject *self, DString *path )
{
	DaoMake_MakePath( vmSpace->startPath, path );
}




void DaoMakeUnit_MakeDefinitions( DaoMakeUnit *self, DString *defs )
{
	daoint i;
	for(i=0; i<self->definitions->size; i+=2){
		DString *definition = self->definitions->items.pString[i];
		DString *value = self->definitions->items.pString[i+1];
		DString_AppendGap( defs );
		DString_AppendMBS( defs, "-D" );
		DString_Append( defs, definition );
		if( value->size ){
			wchar_t ch = value->wcs ? value->wcs[0] : value->mbs[0];
			int nonumber = iswdigit( ch ) == 0;
			DString_AppendChar( defs, '=' );
			if( nonumber ) DString_AppendMBS( defs, "\\\"" );
			DString_Append( defs, value );
			if( nonumber ) DString_AppendMBS( defs, "\\\"" );
		}
	}
}
void DaoMakeUnit_MakeIncludePaths( DaoMakeUnit *self, DString *cflags )
{
	daoint i;
	for(i=0; i<self->includePaths->size; ++i){
		DString *path = self->includePaths->items.pString[i];
		DString_AppendGap( cflags );
		DString_AppendMBS( cflags, "-I" );
		DString_Append( cflags, path );
	}
}
void DaoMakeUnit_MakeLinkingPaths( DaoMakeUnit *self, DString *lflags )
{
	daoint i;
	for(i=0; i<self->linkingPaths->size; ++i){
		DString *path = self->linkingPaths->items.pString[i];
		DString_AppendMBS( lflags, " -L" );
		DString_Append( lflags, path );
	}
}
void DaoMakeProject_ExportCompilingFlags( DaoMakeProject *self, DString *cflags )
{
	daoint j;
	for(j=0; j<self->compilingFlags->size; ++j){
		DString *flag = self->compilingFlags->items.pString[j];
		DString_AppendGap( cflags );
		DString_Append( cflags, flag );
	}
}
void DaoMakeProject_ExportLinkingFlags( DaoMakeProject *self, DString *lflags )
{
	daoint j;
	for(j=0; j<self->linkingFlags->size; ++j){
		DString *flag = self->linkingFlags->items.pString[j];
		DString_AppendGap( lflags );
		DString_Append( lflags, flag );
	}
}
void DaoMakeUnit_MakeCompilingFlags( DaoMakeUnit *self, DString *cflags )
{
	daoint i, j;
	DString_Reset( cflags, 0 );
	for(i=0; i<self->compilingFlags->size; ++i){
		DString_AppendGap( cflags );
		DString_Append( cflags, self->compilingFlags->items.pString[i] );
	}
	DaoMakeUnit_MakeIncludePaths( self, cflags );
	DaoMakeUnit_MakeDefinitions( self, cflags );
	for(i=0; i<self->usingPackages->size; ++i){
		DaoMakeProject *project = self->usingPackages->items.pVoid[i];
		DaoMakeProject_ExportCompilingFlags( project, cflags );
	}
	for(i=0; i<self->usingPackages2->size; ++i){
		DaoMakeProject *project = self->usingPackages2->items.pVoid[i];
		DaoMakeProject_ExportCompilingFlags( project, cflags );
	}
}
void DaoMakeUnit_MakeLinkingFlags( DaoMakeUnit *self, DString *lflags )
{
	daoint i, j;
	DString_Reset( lflags, 0 );
	DaoMakeUnit_MakeLinkingPaths( self, lflags );
	for(i=0; i<self->linkingFlags->size; ++i){
		DString_AppendGap( lflags );
		DString_Append( lflags, self->linkingFlags->items.pString[i] );
	}
	for(i=0; i<self->usingPackages->size; ++i){
		DaoMakeProject *project = (DaoMakeProject*) self->usingPackages->items.pVoid[i];
		for(j=0; j<project->linkingFlags->size; ++j){
			DString *flag = project->linkingFlags->items.pString[j];
			DString_AppendGap( lflags );
			DString_Append( lflags, flag );
		}
	}
	for(i=0; i<self->usingPackages2->size; ++i){
		DaoMakeProject *project = (DaoMakeProject*) self->usingPackages2->items.pVoid[i];
		DString *name = self->staticLibNames->items.pString[i];
		DNode *it = DMap_Find( project->exportStaticLibs, name );
		if( it ){
			DString_AppendGap( lflags );
			DString_Append( lflags, it->value.pString );
		}
		DaoMakeProject_ExportLinkingFlags( project, lflags );
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
	DNode *it;
	DString *md5 = self->mbs;
	DString_MD5( data, md5 );
	DString_ToUpper( md5 );
	DString_Reset( md5, self->signature );
	it = DMap_Find( self->signatures, md5 );
	if( it ){
		if( DString_EQ( data, it->value.pString ) == 0 ) self->signature += 2;
	}else{
		DMap_Insert( self->signatures, md5, data );
	}
	return md5;
}


/* Return macro name: */
DString* DaoMakeProject_MakeHeaderMacro( DaoMakeProject *self, DaoMakeObjects *objects )
{
	DString *file = DaoMakeProject_GetBufferString( self );
	DString *files = DaoMakeProject_GetBufferString( self );
	DString *macro = DaoMakeProject_GetBufferString( self );
	DString *md5 = self->mbs;
	DNode *it;
	daoint i;

	DString_Reset( files, 0 );
	for(i=0; i<objects->headers->size; ++i){
		DString_AppendGap( files );
		DString_Reset( file, 0 );
		DString_Append( file, objects->headers->items.pString[i] );
		DaoMakeProject_MakeSourcePath( self, file );
		DString_Append( files, file );
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

	it = DMap_Insert( macros, name, name );
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

const char* DaoMakeProject_GetFileExtension( DString *file )
{
	daoint pos;
	DString_ToMBS( file );
	pos = DString_RFindChar( file, '.', -1 );
	if( pos == MAXSIZE ) return "";
	return file->mbs + pos;
}
DString* DaoMakeProject_GetLanguageCompiler( DString *file )
{
	const char *ext = DaoMakeProject_GetFileExtension( file );
	if( ext == NULL ) return NULL;
	return DaoValue_TryGetString( DaoMap_GetValueMBS( daomake_compilers, ext ) );
}
DString* DaoMakeProject_GetLanguageLinker( DString *file )
{
	const char *ext = DaoMakeProject_GetFileExtension( file );
	if( ext == NULL ) return NULL;
	return DaoValue_TryGetString( DaoMap_GetValueMBS( daomake_linkers, ext ) );
}

/* Return object file name: */
DString* DaoMakeProject_MakeObjectRule( DaoMakeProject *self, DaoMakeTarget *target, DaoMakeObjects *objects, DString *source )
{
	DString *source2 = DaoMakeProject_GetBufferString( self );
	DString *cflags = DaoMakeProject_GetBufferString( self );
	DString *cflag = DaoMakeProject_GetBufferString( self );
	DString *signature = DaoMakeProject_GetBufferString( self );
	DString *compiler = DaoMakeProject_GetLanguageCompiler( source );
	DString *md5 = self->mbs;
	DString *mode;
	DNode *it;
	daoint pos;

	DString_Reset( cflags, 0 );
	DString_Append( source2, source );
	DaoMakeProject_MakeSourcePath( self, source2 );

	mode = DaoMake_GetSettingValue( daomake_mode_keys[ 2*daomake_build_mode ] );
	if( mode ) DString_Append( cflags, mode );

	DaoMakeUnit_MakeCompilingFlags( & self->base, cflag );
	DString_AppendGap( cflags );
	DString_Append( cflags, cflag );

	DaoMakeUnit_MakeCompilingFlags( & target->base, cflag );
	DString_AppendGap( cflags );
	DString_Append( cflags, cflag );

	DaoMakeUnit_MakeCompilingFlags( & objects->base, cflag );
	DString_AppendGap( cflags );
	DString_Append( cflags, cflag );

	DString_Assign( signature, cflags );
	DString_AppendGap( signature );
	DString_Append( signature, source );

	md5 = DaoMakeProject_SubMD5( self, signature );

	/* Unique (quasi) target name: */
	DString_Reset( signature, 0 );
	DString_Append( signature, source );
	pos = DString_RFindChar( signature, '/', -1 );
	if( pos != MAXSIZE ) DString_Erase( signature, 0, pos + 1 );
	DString_AppendChar( signature, '.' );
	DString_Append( signature, md5 );
	DString_AppendMBS( signature, ".o" );

	it = DMap_Find( self->objectRules, signature );
	if( it ){
		self->usedStrings -= 4;
		return it->key.pString;
	}

	it = DMap_Insert( self->objectRules, signature, signature );
	DString_AppendMBS( it->value.pString, ": " );
	DString_Append( it->value.pString, source2 );
	DString_AppendMBS( it->value.pString, " $(" );
	DString_Append( it->value.pString, DaoMakeProject_MakeHeaderMacro( self, objects ) );
	DString_AppendMBS( it->value.pString, ")\n\t$(" );
	if( compiler ){
		DString_Append( it->value.pString, compiler );
	}else{
		DString_AppendMBS( it->value.pString, "CC" );
	}
	DString_AppendMBS( it->value.pString, ") $(" );
	DString_Append( it->value.pString, DaoMakeProject_MakeCFlagsMacro( self, cflags ) );
	DString_AppendMBS( it->value.pString, ") -c " );
	DString_Append( it->value.pString, source2 );
	DString_AppendMBS( it->value.pString, " -o " );
	DString_Append( it->value.pString, signature );

	self->usedStrings -= 4;
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
		DString_AppendGap( objs );
		DString_Append( objs, obj );
	}
	objs = DaoMakeProject_MakeSimpleMacro( self, self->objectsMacros, objs, "OBJECTS" );
	self->usedStrings -= 1;
	return objs;
}

void DaoMakeTarget_MakeName( DaoMakeTarget *self, DString *name )
{
	DString *prefix = DaoMake_GetSettingValue( daomake_prefix_keys[ self->ttype ] );
	DString *suffix = DaoMake_GetSettingValue( daomake_suffix_keys[ self->ttype ] );

	DString_Reset( name, 0 );
	if( prefix ) DString_Append( name, prefix );
	DString_Append( name, self->name );
	if( suffix ) DString_Append( name, suffix );
}
void DaoMakeProject_MakeDependency( DaoMakeProject *self, DaoMakeTarget *target, DString *deps )
{
	DString *tname = DaoMakeProject_GetBufferString( self );
	daoint i;
	for(i=0; i<target->depends->size; ++i){
		DaoMakeTarget *t = (DaoMakeTarget*) target->depends->items.pVoid[i];
		DaoMakeTarget_MakeName( t, tname );
		DString_AppendGap( deps );
		DString_Append( deps, tname );
	}
	for(i=0; i<target->base.usingPackages2->size; ++i){
		DaoMakeProject *project = (DaoMakeProject*)target->base.usingPackages2->items.pVoid[i];
		DString *name = target->base.staticLibNames->items.pString[i];
		DNode *it = DMap_Find( project->exportStaticLibs, name );
		if( it ){
			DString_AppendGap( deps );
			DString_Append( deps, it->value.pString );
		}
	}
	self->usedStrings -= 1;
}
DString* DaoMakeProject_MakeTargetRule( DaoMakeProject *self, DaoMakeTarget *target )
{
	DString *tname = DaoMakeProject_GetBufferString( self );
	DString *deps = DaoMakeProject_GetBufferString( self );
	DString *lflags = DaoMakeProject_GetBufferString( self );
	DString *lflag = DaoMakeProject_GetBufferString( self );
	DString *signature = DaoMakeProject_GetBufferString( self );
	DString *rule = DaoMakeProject_GetBufferString( self );
	DString *objs = DaoMakeProject_GetBufferString( self );
	DString *linker = DaoMakeProject_GetBufferString( self );
	DString *macro, *mode;
	DNode *it, *lk = NULL;
	daoint i, j, objCount = 0;

	DaoMakeTarget_MakeName( target, tname );
	DaoMakeProject_MakeDependency( self, target, deps );
	DString_Reset( rule, 0 );
	DString_Append( rule, tname );
	DString_AppendMBS( rule, ": " );
	DString_Append( rule, deps );
	DString_AppendGap( rule );

	if( target->ttype >= DAOMAKE_COMMAND ){
		DString *dir = DaoMakeProject_GetBufferString( self );
		DString_ToMBS( dir );
		DString_AppendMBS( rule, "\n" );
		for(i=0; i<target->commands->size; ++i){
			DString *cmd = target->commands->items.pString[i];
			if( target->ttype == DAOMAKE_DIRECTORY ){
				DString_Reset( dir, 0 );
				DString_Append( dir, cmd );
				DaoMakeProject_MakeSourcePath( self, dir );
				if( DMap_Find( daomake_makefile_paths, dir ) == NULL ) continue;
			}
			DString_AppendChar( rule, '\t' );
			if( target->ttype == DAOMAKE_COMMAND ){
				DString_Append( rule, cmd );
			}else if( target->ttype == DAOMAKE_DIRECTORY ){
				DString_AppendMBS( rule, "cd " );
				DString_Append( rule, cmd );
				DString_AppendMBS( rule, " && $(MAKE) -f Makefile" );
				DString_AppendMBS( rule, daomake_makefile_suffix );
			}
			DString_AppendChar( rule, '\n' );
		}
		self->usedStrings -= 7;
		DArray_Append( self->targetRules, tname );
		DArray_Append( self->targetRules, rule );
		return target->name;
	}else if( target->ttype == DAOMAKE_TESTING ){
		DString_Reset( objs, 0 );
		for(i=0; i<target->tests->size; ++i){
			DString *md5, *test = target->tests->items.pString[i];
			md5 = DaoMakeProject_SubMD5( self, test );
			DString_Reset( signature, 0 );
			DString_Append( signature, test );
			DString_AppendChar( signature, '.' );
			DString_Append( signature, md5 );
			DString_AppendMBS( signature, ".test" );

			DString_AppendGap( objs );
			DString_Append( objs, signature );

			it = DMap_Find( self->testRules, signature );
			if( it ) continue;

			DString_Reset( rule, 0 );
			DString_Append( rule, test );
			if( daomake_out_of_source ) DaoMakeProject_MakeSourcePath( self, rule );

			it = DMap_Insert( self->testRules, signature, signature );
			DString_AppendMBS( it->value.pString, ": " );
			DString_Append( it->value.pString, rule );
			DString_AppendMBS( it->value.pString, "\n\t-$(DAOTEST) " );
			DString_Append( it->value.pString, rule );
			DString_AppendGap( it->value.pString );
			DString_Append( it->value.pString, daomake_test_tool_option );
			DString_AppendGap( it->value.pString );
			DString_Append( it->value.pString, signature );
		}
		DString_Reset( lflag, 0 );
		DString_Append( lflag, target->name );
		DString_ToUpper( lflag );
		macro = DaoMakeProject_MakeSimpleMacro( self, self->testsMacros, objs, lflag->mbs );

		DString_Reset( target->testMacro, 0 );
		DString_Append( target->testMacro, macro );

		DString_Reset( rule, 0 );
		DString_Append( rule, target->name );
		DString_AppendMBS( rule, ": $(" );
		DString_Append( rule, macro );
		DString_AppendMBS( rule, ")" );

		DArray_Append( self->targetRules, target->name );
		DArray_Append( self->targetRules, rule );
		self->usedStrings -= 7;
		return target->name;
	}

	mode = DaoMake_GetSettingValue( daomake_mode_keys[ 2*daomake_build_mode+1 ] );
	if( mode ) DString_Append( lflags, mode );

	DaoMakeUnit_MakeLinkingFlags( & self->base, lflag );
	DString_AppendGap( lflags );
	DString_Append( lflags, lflag );

	DaoMakeUnit_MakeLinkingFlags( & target->base, lflag );
	DString_AppendGap( lflags );
	DString_Append( lflags, lflag );

	DString_Reset( objs, 0 );
	DMap_Reset( self->mapStringInt );
	for(i=0; i<target->objects->size; ++i){
		DaoMakeObjects *objects = (DaoMakeObjects*) target->objects->items.pVoid[i];
		DString *objmacro = DaoMakeProject_MakeObjectsMacro( self, target, objects );

		objCount += objects->sources->size;
		/* Find common linkers: */
		for(j=0; j<objects->sources->size; ++j){
			DString *source = objects->sources->items.pString[j];
			DString *linkers = DaoMakeProject_GetLanguageLinker( source );
			daoint pos, start = 0;
			pos = DString_FindChar( linkers, ';', start );
			while( start < linkers->size ){
				if( pos == MAXSIZE ) pos = linkers->size;
				DString_SubString( linkers, linker, start, pos - start );
				it = DMap_Find( self->mapStringInt, linker );
				if( it == NULL ) it = DMap_Insert( self->mapStringInt, linker, 0 );
				it->value.pInt += 1;
				start = pos + 1;
				pos = DString_FindChar( linkers, ';', start );
			}
		}

		DaoMakeUnit_MakeLinkingFlags( & objects->base, lflag );
		DString_AppendGap( lflags );
		DString_Append( lflags, lflag );

		DString_AppendGap( objs );
		DString_AppendMBS( objs, "$(" );
		DString_Append( objs, objmacro );
		DString_AppendChar( objs, ')' );
	}
	DString_Append( rule, objs );
	if( target->ttype == DAOMAKE_STATICLIB ){
		DString *arc = DaoMake_GetSettingValue( "AR" );
		DString_AppendMBS( rule, "\n\t" );
		if( arc ) DString_Append( rule, arc );
		DString_AppendGap( rule );
		DString_Append( rule, tname );
		DString_AppendGap( rule );
		DString_Append( rule, objs );
	}else{
		for(it=lk=DMap_First(self->mapStringInt); it; it=DMap_Next(self->mapStringInt,it)){
			if( it->value.pInt > lk->value.pInt ) lk = it;
		}
		if( target->ttype == DAOMAKE_EXECUTABLE ){
		}else if( target->ttype == DAOMAKE_SHAREDLIB ){
			DString *flag = DaoMake_GetSettingValue( "DLL-FLAG" );
			if( flag ){
				DString_AppendGap( lflags );
				DString_Append( lflags, flag );
			}
		}
		if( target->dynamicExporting ){
			DString *flag = DaoMake_GetSettingValue( "DYNAMIC-EXPORT" );
			if( flag ){
				DString_AppendGap( lflags );
				DString_Append( lflags, flag );
			}
		}
		if( target->dynamicLinking ){
			DString *flag = DaoMake_GetSettingValue( "DYNAMIC-IMPORT" );
			if( flag ){
				DString_AppendGap( lflags );
				DString_Append( lflags, flag );
			}
		}
		macro = DaoMakeProject_MakeLFlagsMacro( self, lflags );
		DString_AppendMBS( rule, "\n\t$(" );
		if( lk && lk->value.pInt == objCount ){
			DString_Append( rule, lk->key.pString );
		}else{
			DString_AppendMBS( rule, "CC" );
		}
		DString_AppendMBS( rule, ") " );

		DString_Append( rule, objs );

		DString_AppendMBS( rule, " $(" );
		DString_Append( rule, macro );
		DString_AppendMBS( rule, ") " );

		DString_AppendMBS( rule, " -o " );
		DString_Append( rule, tname );
	}

	DArray_Append( self->targetRules, tname );
	DArray_Append( self->targetRules, rule );
	self->usedStrings -= 7;
	return self->targetRules->items.pString[self->targetRules->size-2];
}

void DaoMakeProject_MakeInstallPath( DaoMakeProject *self, DString *path, DString *install, DString *uninstall, DMap *mapPaths )
{
	DString *mkdir = DaoMake_GetSettingValue( "MKDIR" );
	DString *del = DaoMake_GetSettingValue( "DEL-FILE" );
	DString *sub;

	if( mkdir == NULL || del == NULL ) return;
	if( Dao_IsDir( path->mbs ) ) return;
	if( DMap_Find( mapPaths, path ) ) return;
	DMap_Insert( mapPaths, path, 0 );

	sub = DaoMakeProject_GetBufferString( self );
	DString_Reset( sub, 0 );
	DString_ToMBS( sub );
	DString_Append( sub, path );
	if( sub->size && sub->mbs[sub->size-1] == '/' ) sub->size --;
	while( sub->size && sub->mbs[sub->size-1] != '/' ) sub->size --;
	if( sub->size && sub->mbs[sub->size-1] == '/' ) sub->size --;
	sub->mbs[sub->size] = '\0';

	if( sub->size && Dao_IsDir( sub->mbs ) == 0 ){
		DaoMakeProject_MakeInstallPath( self, sub, install, uninstall, mapPaths );
	}
	DString_AppendChar( install, '\t' );
	DString_Append( install, mkdir );
	DString_AppendChar( install, ' ' );
	DString_Append( install, path );
	DString_AppendChar( install, '\n' );

	DString_AppendChar( uninstall, '\t' );
	DString_Append( uninstall, del );
	DString_AppendChar( uninstall, ' ' );
	DString_Append( uninstall, path );
	DString_AppendChar( uninstall, '\n' );

	self->usedStrings -= 1;
}
void DaoMakeProject_MakeCopy( DaoMakeProject *self, DString *src, DString *dest, DString *output )
{
	DString *copydir = DaoMake_GetSettingValue( "COPY-DIR" );
	if( copydir == NULL ) return;
	DString_AppendChar( output, '\t' );
	DString_Append( output, copydir );
	DString_AppendChar( output, ' ' );
	DString_Append( output, src );
	DString_AppendChar( output, ' ' );
	DString_Append( output, dest );
	DString_AppendChar( output, '\n' );
}
void DaoMakeProject_MakeRemove( DaoMakeProject *self, DString *file, DString *path, DString *output )
{
	DString *del = DaoMake_GetSettingValue( "DEL-FILE" );
	if( del == NULL ) return;
	DString_ToMBS( path );
	DString_AppendChar( output, '\t' );
	DString_Append( output, del );
	DString_AppendChar( output, ' ' );
	DString_Append( output, path );
	if( file ){
		daoint pos;
		char *file2;
		DString_ToMBS( file );
		pos = DString_RFindChar( file, '/', -1 );
		file2 = pos == MAXSIZE ? file->mbs : file->mbs + pos + 1;
		if( path->size && path->mbs[path->size-1] != '/' ) DString_AppendChar( output, '/' );
		DString_AppendMBS( output, file2 );
	}
	DString_AppendChar( output, '\n' );
}
void DaoMakeProject_MakeDirectoryMake( DaoMakeProject *self, DString *makefile, const char *mktarget )
{
	DString *dir = DaoMakeProject_GetBufferString( self );
	daoint i, j;
	for(i=0; i<self->targets->size; ++i){
		DaoMakeTarget *target = (DaoMakeTarget*) self->targets->items.pVoid[i];
		if( target->ttype != DAOMAKE_DIRECTORY ) continue;
		for(j=0; j<target->commands->size; ++j){
			DString *subdir = target->commands->items.pString[j];
			DString_Reset( dir, 0 );
			DString_Append( dir, subdir );
			DaoMakeProject_MakeSourcePath( self, dir );
			if( DMap_Find( daomake_makefile_paths, dir ) == NULL ) continue;
			DString_AppendMBS( makefile, "\tcd " );
			DString_Append( makefile, subdir );
			DString_AppendMBS( makefile, " && $(MAKE) -f Makefile" );
			DString_AppendMBS( makefile, daomake_makefile_suffix );
			DString_AppendChar( makefile, ' ' );
			DString_AppendMBS( makefile, mktarget );
			DString_AppendChar( makefile, '\n' );
		}
	}
	self->usedStrings -= 1;
}
void DaoMakeProject_MakeInstallation( DaoMakeProject *self, DString *makefile )
{
	DString *tname = DaoMakeProject_GetBufferString( self );
	DString *install = DaoMakeProject_GetBufferString( self );
	DString *uninstall = DaoMakeProject_GetBufferString( self );
	DString *file = DaoMakeProject_GetBufferString( self );
	DMap *mapPaths = DMap_New(D_STRING,0);
	daoint i;

	for(i=0; i<self->installs->size; i+=2){
		DString *file = DaoValue_TryGetString( self->installs->items.pValue[i] );
		DaoMakeTarget *target = (DaoMakeTarget*) self->installs->items.pVoid[i];
		DString *path = DaoValue_TryGetString( self->installs->items.pValue[i+1] );
		DaoMakeProject_MakeSourcePath( self, path );
		if( file ){
			DString_Assign( tname, file );
			DaoMakeProject_MakeSourcePath( self, tname );
		}else{
			DaoMakeTarget_MakeName( target, tname );
		}
		if( Dao_IsDir( path->mbs ) == 0 ){
			DaoMakeProject_MakeInstallPath( self, path, install, uninstall, mapPaths );
		}else{
			DaoMakeProject_MakeRemove( self, tname, path, uninstall );
		}
		DaoMakeProject_MakeCopy( self, tname, path, install );
	}
	DMap_Delete( mapPaths );
	self->usedStrings -= 4;
	DaoMakeProject_MakeDirectoryMake( self, install, "install" );
	DaoMakeProject_MakeDirectoryMake( self, uninstall, "uninstall" );
	DString_AppendMBS( makefile, "install:\n" );
	DString_Append( makefile, install );
	DString_AppendMBS( makefile, "\n\n" );

	DString_AppendMBS( makefile, "uninstall:\n" );
	DString_Append( makefile, uninstall );
	DString_AppendMBS( makefile, "\n\n" );
	DString_AppendMBS( makefile, ".PHONY: install uninstall\n\n" );
}
void DaoMakeProject_MakeFile( DaoMakeProject *self, DString *makefile )
{
	DNode *it;
	DString *del = DaoMake_GetSettingValue( "DEL-FILE" );
	daoint ismain = DString_EQ( self->sourcePath, daomake_main_source_path );
	daoint i, j, sig = self->signature;

	self->usedStrings = 0;
	DMap_Reset( self->headerMacros );
	DMap_Reset( self->cflagsMacros );
	DMap_Reset( self->lflagsMacros );
	DMap_Reset( self->objectRules );
	DMap_Reset( self->objectsMacros );
	DMap_Reset( self->signatures );
	DArray_Clear( self->targetRules );
	DString_Reset( makefile, 0 );
	if( (self->targets->size + self->installs->size) == 0 ) return;

	if( self->targets->size ){
		DString *phony = DaoMakeProject_GetBufferString( self );
		DString *test = DaoMakeProject_GetBufferString( self );
		DString *testsum = DaoMakeProject_GetBufferString( self );
		DString_AppendMBS( makefile, "all:" );
		for(i=0; i<self->targets->size; ++i){
			DaoMakeTarget *target = (DaoMakeTarget*) self->targets->items.pVoid[i];
			DString *ruleName = DaoMakeProject_MakeTargetRule( self, target );
			if( target->ttype == DAOMAKE_TESTING ){
				DString_AppendGap( test );
				DString_Append( test, ruleName );
				continue;
			}
			DString_AppendGap( makefile );
			DString_Append( makefile, ruleName );
			if( target->ttype >= DAOMAKE_COMMAND ){
				DString_AppendGap( phony );
				DString_Append( phony, ruleName );
			}
		}
		if( ismain && daomake_test_count && daomake_build_mode ){
			DString_AppendGap( makefile );
			DString_AppendMBS( makefile, "test testsum" );
		}
		DString_AppendMBS( makefile, "\n\n" );

		DString_AppendMBS( makefile, ".PHONY: test\n" );
		DString_AppendMBS( makefile, "test:" );
		DString_AppendGap( makefile );
		DString_Append( makefile, test );

		DString_AppendGap( makefile );
		DString_AppendMBS( makefile, "subtest" );
		DString_AppendGap( makefile );
		DString_AppendMBS( makefile, "testsum\n\n" );
		if( phony->size ){
			DString_AppendMBS( makefile, ".PHONY: " );
			DString_Append( makefile, phony );
			DString_AppendChar( makefile, '\n' );
		}
		self->usedStrings -= 3;
	}

	DString_AppendMBS( makefile, "\nDAOMAKE = " );
	DString_Append( makefile, vmSpace->daoBinPath );
	DString_AppendMBS( makefile, "/daomake\n\n" );

	for(i=0; i<self->variables->size; i+=2){
		DString_Append( makefile, self->variables->items.pString[i] );
		DString_AppendMBS( makefile, " =" );
		DString_Append( makefile, self->variables->items.pString[i+1] );
		DString_AppendChar( makefile, '\n' );
	}
	DString_AppendChar( makefile, '\n' );

	for(it=DMap_First(self->headerMacros); it; it=DMap_Next(self->headerMacros,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendChar( makefile, '\n' );
	}
	DString_AppendChar( makefile, '\n' );

	for(it=DMap_First(self->cflagsMacros); it; it=DMap_Next(self->cflagsMacros,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendChar( makefile, '\n' );
	}
	DString_AppendChar( makefile, '\n' );

	for(it=DMap_First(self->lflagsMacros); it; it=DMap_Next(self->lflagsMacros,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendChar( makefile, '\n' );
	}
	DString_AppendChar( makefile, '\n' );

	for(it=DMap_First(self->objectRules); it; it=DMap_Next(self->objectRules,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendMBS( makefile, "\n\n" );
	}
	DString_AppendChar( makefile, '\n' );

	for(it=DMap_First(self->objectsMacros); it; it=DMap_Next(self->objectsMacros,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendMBS( makefile, "\n\n" );
	}

	if( self->testRules->size ){
		DString_AppendMBS( makefile, "DAOTEST = " );
		DString_Append( makefile, daomake_test_tool );
		DString_AppendMBS( makefile, "\n\n" );
	}

	for(it=DMap_First(self->testRules); it; it=DMap_Next(self->testRules,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendMBS( makefile, "\n\n" );
	}
	DString_AppendChar( makefile, '\n' );

	for(it=DMap_First(self->testsMacros); it; it=DMap_Next(self->testsMacros,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendMBS( makefile, "\n\n" );
	}

	for(i=0; i<self->targetRules->size; i+=2){
		DString_Append( makefile, self->targetRules->items.pString[i+1] );
		DString_AppendMBS( makefile, "\n\n" );
	}

	DString_AppendMBS( makefile, "subtest:\n" );
	DaoMakeProject_MakeDirectoryMake( self, makefile, "test" );
	DString_AppendChar( makefile, '\n' );
	DString_AppendMBS( makefile, ".PHONY: subtest\n\n" );

	DString_AppendMBS( makefile, "TESTSUM =" );
	DString_Append( makefile, vmSpace->startPath );
	DString_AppendMBS( makefile, "/" );
	DString_AppendMBS( makefile, daomake_test_sumfile );
	DString_AppendMBS( makefile, "\n\n" );

	DString_AppendMBS( makefile, "testsum:\n" );
	if( ismain ){
		DString *check = DaoMake_GetSettingValue( "HAS-FILE" );
		DString *del = DaoMake_GetSettingValue( "DEL-FILE" );
		DString_AppendMBS( makefile, "\t@$(DAOMAKE) echo \"Summarizing test results ...\"\n" );
		if( check && del ){
			DString_AppendMBS( makefile, "\t-@" );
			DString_Append( makefile, check );
			DString_AppendMBS( makefile, " $(TESTSUM) && " );
			DString_Append( makefile, del );
			DString_AppendMBS( makefile, " $(TESTSUM)\n" );
		}
	}
	for(i=0; i<self->targets->size; ++i){
		DaoMakeTarget *target = (DaoMakeTarget*) self->targets->items.pVoid[i];
		if( target->ttype != DAOMAKE_TESTING ) continue;

		DString_AppendMBS( makefile, "\t@$(DAOTEST) --sum $(" );
		DString_Append( makefile, target->testMacro );
		DString_AppendMBS( makefile, ") --log $(TESTSUM) --group " );
		DString_Append( makefile, target->name );
		DString_AppendMBS( makefile, "\n" );
	}
	DaoMakeProject_MakeDirectoryMake( self, makefile, "testsum" );
	if( ismain ) DString_AppendMBS( makefile, "\t@$(DAOMAKE) cat $(TESTSUM)\n" );
	DString_AppendChar( makefile, '\n' );
	DString_AppendMBS( makefile, ".PHONY: testsum\n\n" );

	DaoMakeProject_MakeInstallation( self, makefile );

	DString_AppendMBS( makefile, "clean:\n\t" );
	if( self->objectsMacros->size + self->testsMacros->size ){
		if( del ) DString_Append( makefile, del );
		for(it=DMap_First(self->objectsMacros); it; it=DMap_Next(self->objectsMacros,it)){
			DString_AppendGap( makefile );
			DString_AppendMBS( makefile, "$(" );
			DString_Append( makefile, it->key.pString );
			DString_AppendChar( makefile, ')' );
		}
		for(it=DMap_First(self->testsMacros); it; it=DMap_Next(self->testsMacros,it)){
			DString_AppendGap( makefile );
			DString_AppendMBS( makefile, "$(" );
			DString_Append( makefile, it->key.pString );
			DString_AppendChar( makefile, ')' );
		}
		DString_AppendChar( makefile, '\n' );
	}
	DaoMakeProject_MakeDirectoryMake( self, makefile, "clean" );
	DString_AppendChar( makefile, '\n' );
	DString_AppendMBS( makefile, ".PHONY: clean\n\n" );

	/* Regenerate if there was MD5 signature conflict: */
	if( self->signature != sig ) DaoMakeProject_MakeFile( self, makefile );
}

void DaoMakeProject_MakeFindPackage( DaoMakeProject *self, DString *output )
{
	DString *incdir = DaoMakeProject_GetBufferString( self );
	DString *lnkdir = DaoMakeProject_GetBufferString( self );
	DString *cflags = DaoMakeProject_GetBufferString( self );
	DString *lflags = DaoMakeProject_GetBufferString( self );
	DaoValue **installs = self->installs->items.pValue;
	DMap *incdirs = DMap_New(D_STRING,0);
	DMap *lnkdirs = DMap_New(D_STRING,0);
	DString *md5 = self->mbs;
	daoint i, count = 0;

	DString_Reset( output, 0 );
	DString_AppendMBS( output, "project = DaoMake::Project( \"" );
	DString_Append( output, self->projectName );
	DString_AppendMBS( output, "\" )\n" );

	DaoMakeUnit_MakeDefinitions( & self->base, cflags );
	DaoMakeProject_ExportLinkingFlags( self, lflags );

	for(i=0; i<self->installs->size; i+=2){
		DString *dest = DaoValue_TryGetString( installs[i+1] );
		DString *file = DaoValue_TryGetString( installs[i] );
		DaoMakeTarget *target = (DaoMakeTarget*) DaoValue_CastCdata( installs[i], daomake_type_target );
		DaoMakeProject_MakeSourcePath( self, dest );
		if( target ){
			int ttype = target->ttype;
			if( ttype != DAOMAKE_SHAREDLIB && ttype != DAOMAKE_STATICLIB ) continue;
			count += 1;
			if( DMap_Find( lnkdirs, dest ) ){
				DString_AppendGap( lnkdir );
				DString_AppendMBS( lnkdir, "-l" );
				DString_Append( lnkdir, target->name );
				continue;
			}
			DMap_Insert( lnkdirs, dest, 0 );
			DString_AppendGap( lnkdir );
			DString_AppendMBS( lnkdir, "-L" );
			DString_Append( lnkdir, dest );
			DString_AppendGap( lnkdir );
			DString_AppendMBS( lnkdir, "-l" );
			DString_Append( lnkdir, target->name );
		}else if( DString_MatchMBS( file, " %. (h | hpp | hxx) $", NULL, NULL ) ){
			if( DMap_Find( incdirs, dest ) ) continue;
			DMap_Insert( incdirs, dest, 0 );
			DString_AppendGap( incdir );
			DString_AppendMBS( incdir, "-I" );
			DString_Append( incdir, dest );
		}
	}
	if( count == 0 ){
		DString_Reset( output, 0 );
		return;
	}
	if( incdir->size ){
		DString_AppendGap( cflags );
		DString_Append( cflags, incdir );
	}

	if( lnkdir->size ){
		DString_AppendGap( lflags );
		DString_Append( lflags, lnkdir );
	}

	DString_MD5( cflags, md5 );
	DString_Reset( md5, 12 );
	DString_ToUpper( md5 );
	DString_AppendMBS( output, "cflags = @[" );
	DString_Append( output, md5 );
	DString_AppendMBS( output, "]" );
	DString_Append( output, cflags );
	DString_AppendMBS( output, "@[" );
	DString_Append( output, md5 );
	DString_AppendMBS( output, "]\n" );

	DString_MD5( lflags, md5 );
	DString_Reset( md5, 12 );
	DString_ToUpper( md5 );
	DString_AppendMBS( output, "lflags = @[" );
	DString_Append( output, md5 );
	DString_AppendMBS( output, "]" );
	DString_Append( output, lflags );
	DString_AppendMBS( output, "@[" );
	DString_Append( output, md5 );
	DString_AppendMBS( output, "]\n" );

	DString_AppendMBS( output, "project.ExportCompilingFlags( cflags )\n" );
	DString_AppendMBS( output, "project.ExportLinkingFlags( lflags )\n" );

	DMap_Delete( incdirs );
	DMap_Delete( lnkdirs );
	self->usedStrings -= 4;
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
	int i;
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	for(i=1; i<N; ++i) DArray_Append( self->usingPackages, p[i] );
}
static void UNIT_UseStaticPackage( DaoProcess *proc, DaoValue *p[], int N )
{
	int i;
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	for(i=2; i<N; ++i){
		DString *name = DaoValue_TryGetString( p[i] );
		if( name == NULL ) continue;
		DArray_Append( self->usingPackages2, p[1] );
		DArray_Append( self->staticLibNames, name );
	}
}
static void UNIT_MakeDefinitions( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DString *res = DaoProcess_PutMBString( proc, "" );
	DaoMakeUnit_MakeDefinitions( self, res );
}
static void UNIT_MakeIncludePaths( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DString *res = DaoProcess_PutMBString( proc, "" );
	DaoMakeUnit_MakeIncludePaths( self, res );
}
static void UNIT_MakeLinkingPaths( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DString *res = DaoProcess_PutMBString( proc, "" );
	DaoMakeUnit_MakeLinkingPaths( self, res );
}
static void UNIT_MakeCompilingFlags( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DString *res = DaoProcess_PutMBString( proc, "" );
	DaoMakeUnit_MakeCompilingFlags( self, res );
}
static void UNIT_MakeLinkingFlags( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DString *res = DaoProcess_PutMBString( proc, "" );
	DaoMakeUnit_MakeLinkingFlags( self, res );
}
static DaoFuncItem DaoMakeUnitMeths[]=
{
	{ UNIT_AddDefinition,     "AddDefinition( self : Unit, name : string, value = '' )" },
	{ UNIT_AddIncludePath,    "AddIncludePath( self : Unit, path : string, ... : string )" },
	{ UNIT_AddLinkingPath,    "AddLinkingPath( self : Unit, path : string, ... : string )" },
	{ UNIT_AddCompilingFlag,  "AddCompilingFlag( self : Unit, flag : string, ... : string )" },
	{ UNIT_AddLinkingFlag,    "AddLinkingFlag( self : Unit, flag : string, ... : string )" },
	{ UNIT_UsePackage,        "UsePackage( self : Unit, pkg : Project, ... : Project )" },
	{ UNIT_UseStaticPackage,  "UseStaticPackage( self : Unit, pkg : Project, name : string, ... : string )" },

	{ UNIT_MakeDefinitions,     "MakeDefinitions( self : Unit ) => string" },
	{ UNIT_MakeIncludePaths,    "MakeIncludePaths( self : Unit ) => string" },
	{ UNIT_MakeLinkingPaths,    "MakeLinkingPaths( self : Unit ) => string" },
	{ UNIT_MakeCompilingFlags,  "MakeCompilingFlags( self : Unit ) => string" },
	{ UNIT_MakeLinkingFlags,    "MakeLinkingFlags( self : Unit ) => string" },
	{ NULL, NULL }
};
static void UNIT_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int rm )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p;
	DArray_Append( arrays, self->usingPackages );
	DArray_Append( arrays, self->usingPackages2 );
}
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
	{ OBJECTS_New,   "Objects( sources : list<string>, headers : list<string> = {} ) => Objects" },
	{ OBJECTS_AddHeaders,  "AddHeaders( file : string, ... : string )" },
	{ OBJECTS_AddSources,  "AddSources( file : string, ... : string )" },
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
static void TARGET_AddCommand( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeTarget *self = (DaoMakeTarget*) p[0];
	int i;
	for(i=1; i<N; ++i){
		DString *cmd = DaoValue_TryGetString( p[i] );
		if( cmd == NULL ) continue;
		DArray_Append( self->commands, cmd );
	}
}
static void TARGET_AddTest( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeTarget *self = (DaoMakeTarget*) p[0];
	int i;
	for(i=1; i<N; ++i){
		DString *test = DaoValue_TryGetString( p[i] );
		if( test == NULL ) continue;
		DArray_Append( self->tests, test );
	}
}
static void TARGET_AddDepends( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeTarget *self = (DaoMakeTarget*) p[0];
	int i;
	for(i=1; i<N; ++i){
		DaoMakeTarget *target = (DaoMakeTarget*) DaoValue_CastCdata( p[i], daomake_type_target );
		if( target == NULL ) continue;
		DArray_Append( self->depends, target );
	}
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
static void TARGET_Install( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns = proc->activeNamespace;
	DaoMakeTarget *self = (DaoMakeTarget*) p[0];
	DString *dest = p[1]->xString.data;
	DString_Assign( self->install, dest );
	DaoMake_MakePath( ns->path, self->install );
}
static DaoFuncItem DaoMakeTargetMeths[]=
{
	{ TARGET_AddObjects,  "AddObjects( self : Target, objects : Objects, ... : Objects )" },
	{ TARGET_AddCommand,  "AddCommand( self : Target, command : string, ... : string )" },
	{ TARGET_AddTest,     "AddTest( self : Target, test : string, ... : string )" },
	{ TARGET_AddDepends,  "AddDependency( self : Target, target : Target, ... : Target )" },
	{ TARGET_EnableDynamicExporting,  "EnableDynamicExporting( self : Target )" },
	{ TARGET_EnableDynamicLinking,    "EnableDynamicLinking( self : Target )" },
	{ TARGET_Install,  "Install( self : Target, dest : string )" },
	{ NULL, NULL }
};
static void TARGET_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int rm )
{
	DaoMakeTarget *self = (DaoMakeTarget*) p;
	UNIT_GetGCFields( p, values, arrays, maps, rm );
	DArray_Append( arrays, self->objects );
	DArray_Append( arrays, self->depends );
}
DaoTypeBase DaoMakeTarget_Typer =
{
	"Target", NULL, NULL, (DaoFuncItem*) DaoMakeTargetMeths,
	{ & DaoMakeUnit_Typer, NULL }, {0},
	(FuncPtrDel)DaoMakeTarget_Delete, TARGET_GetGCFields
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
	if( name->mbs )
		DaoMap_InsertMBS( daomake_projects, name->mbs, (DaoValue*) self );
	else
		DaoMap_InsertWCS( daomake_projects, name->wcs, (DaoValue*) self );
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
static void PROJECT_AddTest( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DaoMakeTarget *target = DaoMakeTarget_New();
	int i;
	daomake_test_count += (N-2);
	target->ttype = DAOMAKE_TESTING;
	DString_Assign( target->name, DaoValue_TryGetString( p[1] ) );
	for(i=2; i<N; ++i) DArray_Append( target->tests, DaoValue_TryGetString( p[i] ) );
	DaoProcess_PutValue( proc, (DaoValue*) target );
	DArray_Append( self->targets, (DaoValue*) target );
}
static void PROJECT_AddCMD( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DString *name = DaoValue_TryGetString( p[1] );
	DaoMakeTarget *target = DaoMakeTarget_New();
	int i;
	target->ttype = DAOMAKE_COMMAND;
	DString_Assign( target->name, name );
	for(i=2; i<N; ++i){
		DString *cmd = DaoValue_TryGetString( p[i] );
		DArray_Append( target->commands, cmd );
	}
	DaoProcess_PutValue( proc, (DaoValue*) target );
	DArray_Append( self->targets, (DaoValue*) target );
}
static void PROJECT_AddDIR( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DString *file = DaoMakeProject_GetBufferString( self );
	DString *name = DaoValue_TryGetString( p[1] );
	DaoMakeTarget *target = DaoMakeTarget_New();
	int i;
	target->ttype = DAOMAKE_DIRECTORY;
	DString_Assign( target->name, name );
	for(i=2; i<N; ++i){
		DString *path = DaoValue_TryGetString( p[i] );
		DString_Append( file, path );
		if( file->size && file->mbs[file->size-1] != '/' ) DString_AppendChar( file, '/' );
		DString_AppendMBS( file, "makefile.dao" );
		DaoVmSpace_Load( proc->vmSpace, file->mbs );
		DArray_Append( target->commands, path );
	}
	DaoProcess_PutValue( proc, (DaoValue*) target );
	DArray_Append( self->targets, (DaoValue*) target );
	self->usedStrings -= 1;
}
static void PROJECT_AddVAR( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DString *name = DaoValue_TryGetString( p[1] );
	DString *value = DaoValue_TryGetString( p[2] );
	DArray_Append( self->variables, name );
	DArray_Append( self->variables, value );
}
static void PROJECT_InstallTarget( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns = proc->activeNamespace;
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DString *dest = DaoValue_TryGetString( p[1] );
	int i;
	DaoMake_MakePath( ns->path, dest );
	for(i=2; i<N; ++i){
		DaoMakeTarget *target = (DaoMakeTarget*) p[i];
		DArray_Append( self->installs, p[i] );
		DArray_Append( self->installs, p[1] );
		if( target->ttype <= DAOMAKE_STATICLIB ) DString_Assign( target->install, dest );
	}
}
static void PROJECT_InstallFile( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns = proc->activeNamespace;
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DString *dest = DaoValue_TryGetString( p[1] );
	int i;
	DaoMake_MakePath( ns->path, dest );
	for(i=2; i<N; ++i){
		DString *file = DaoValue_TryGetString( p[i] );
		DaoMake_MakePath( ns->path, file );
		DArray_Append( self->installs, p[i] );
		DArray_Append( self->installs, p[1] );
	}
}
static void PROJECT_InstallFiles( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns = proc->activeNamespace;
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DaoList *list = (DaoList*) p[2];
	int i, size = DaoList_Size( list );
	DaoMake_MakePath( ns->path, p[1]->xString.data );
	for(i=0; i<size; ++i){
		DaoValue *it = DaoList_GetItem( list, i );
		DArray_Append( self->installs, it );
		DArray_Append( self->installs, p[1] );
	}
}
static void PROJECT_ExportCFlags( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DArray_Append( self->compilingFlags, DaoValue_TryGetString( p[1] ) );
}
static void PROJECT_ExportLFlags( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DArray_Append( self->linkingFlags, DaoValue_TryGetString( p[1] ) );
}
static void PROJECT_ExportPath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DString *name = DaoValue_TryGetString( p[1] );
	DString *path = DaoValue_TryGetString( p[2] );
	DaoMakeProject_MakeBinaryPath( self, path );
	DMap_Insert( self->exportPaths, name, path );
}
static void PROJECT_ExportLib( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DaoMakeTarget *target = (DaoMakeTarget*) p[1];
	DString *path = DaoMakeProject_GetBufferString( self );
	DaoMakeTarget_MakeName( target, path );
	DaoMakeProject_MakeBinaryPath( self, path );
	DMap_Insert( self->exportStaticLibs, target->name, path );
	self->usedStrings -= 1;
}
static void PROJECT_GetPath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DString *name = DaoValue_TryGetString( p[1] );
	DString *res = DaoProcess_PutMBString( proc, "" );
	DNode *it = DMap_Find( self->exportPaths, name );
	if( it ) DString_Assign( res, it->value.pString );
}
static void PROJECT_GetLib( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DString *name = DaoValue_TryGetString( p[1] );
	DString *res = DaoProcess_PutMBString( proc, "" );
	DNode *it = DMap_Find( self->exportStaticLibs, name );
	if( it ) DString_Assign( res, it->value.pString );
}
static void PROJECT_GenerateFinder( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	self->generateFinder = p[1]->xEnum.value;
}
static DaoFuncItem DaoMakeProjectMeths[]=
{
	{ PROJECT_New,     "Project( name : string ) => Project" },

	{ PROJECT_AddEXE,  "AddExecutable( self : Project, name : string, objs : Objects, ... : Objects ) =>Target" },
	{ PROJECT_AddDLL,  "AddSharedLibrary( self : Project, name : string, objs : Objects, ... : Objects ) =>Target" },
	{ PROJECT_AddARC,  "AddStaticLibrary( self : Project, name : string, objs : Objects, ... : Objects ) =>Target" },
	{ PROJECT_AddTest, "AddTest( self : Project, group : string, test : string, ... : string ) => Target" },
	{ PROJECT_AddCMD,  "AddCommand( self : Project, name : string, command : string, ... : string ) => Target" },

	{ PROJECT_AddDIR,  "AddDirectory( self : Project, name : string, path : string, ... : string ) => Target" },

	{ PROJECT_AddVAR,   "AddVariable( self : Project, name : string, value : string )" },

	{ PROJECT_InstallTarget,  "Install( self : Project, dest : string, target : Target, ... : Target )" },
	{ PROJECT_InstallFile,    "Install( self : Project, dest : string, file : string, ... : string )" },
	{ PROJECT_InstallFiles,   "Install( self : Project, dest : string, headers : list<string> )" },

	{ PROJECT_ExportCFlags,  "ExportCompilingFlags( self : Project, flags : string )" },
	{ PROJECT_ExportLFlags,  "ExportLinkingFlags( self : Project, flags : string )" },
	{ PROJECT_ExportPath,    "ExportPath( self : Project, name : string, path : string )" },
	{ PROJECT_ExportLib,     "ExportStaticLibrary( self : Project, lib : Target )" },
	{ PROJECT_GetPath,       "GetPath( self : Project, name : string ) => string" },
	{ PROJECT_GenerateFinder,   "GenerateFinder( self : Project, bl : enum<FALSE,TRUE> = $TRUE )" },
	/*{ PROJECT_GetLib,  "GetStaticLibrary( self : Project, name : string ) => string" },*/
	{ NULL, NULL }
};
static void PROJ_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int rm )
{
	DaoMakeProject *self = (DaoMakeProject*) p;
	UNIT_GetGCFields( p, values, arrays, maps, rm );
	DArray_Append( arrays, self->targets );
	DArray_Append( arrays, self->installs );
}
DaoTypeBase DaoMakeProject_Typer =
{
	"Project", NULL, NULL, (DaoFuncItem*) DaoMakeProjectMeths,
	{ & DaoMakeUnit_Typer, NULL }, {0},
	(FuncPtrDel)DaoMakeProject_Delete,  PROJ_GetGCFields
};



extern size_t FileChangedTime( const char *file );

static void DAOMAKE_FindPackage( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoValue *project = DaoMap_GetValue( daomake_projects, p[0] );
	DString *name = DaoValue_TryGetString( p[0] );
	DString *cache = DString_Copy( proc->activeNamespace->path );
	DString *original = DString_New(1);
	DString *message = DString_New(1);
	daoint i, reset = daomake_reset_cache;
	size_t otime, ctime;
	FILE *fout = NULL;

	DString_AppendMBS( message, "Package \"" );
	DString_Append( message, name );
	DString_AppendMBS( message, "\" not found!" );

	DString_SetMBS( original, "packages/Find" );
	DString_Append( original, name);
	DString_AppendMBS( original, ".dao" );

	DString_AppendMBS( cache, "/CacheFind" );
	DString_Append( cache, name);
	DString_AppendMBS( cache, ".dao" );
	DaoMake_MakeOutOfSourcePath( cache );

	if( DaoVmSpace_CompleteModuleName( vmSpace, cache ) == DAO_MODULE_NONE ) reset = 1;
	DaoVmSpace_CompleteModuleName( vmSpace, original );
	otime = FileChangedTime( original->mbs );
	ctime = FileChangedTime( cache->mbs );
	if( otime > ctime ) reset = 1;

	if( project == NULL && reset == 0 ){
		if( DaoVmSpace_Load( vmSpace, cache->mbs ) ){
			project = DaoMap_GetValue( daomake_projects, p[0] );
			if( project == NULL ){
				if( p[1]->xEnum.value ){
					DaoProcess_RaiseException( proc, DAO_ERROR, message->mbs );
				}
				project = daomake_type_project->value;
			}
			DaoProcess_PutValue( proc, project );
			DString_Delete( original );
			DString_Delete( message );
			DString_Delete( cache );
			return;
		}
	}
	if( project == NULL ){
		if( DaoVmSpace_Load( vmSpace, original->mbs ) )fout = fopen( cache->mbs, "w+" );
	}
	project = DaoMap_GetValue( daomake_projects, p[0] );
	if( project == NULL ){
		if( p[1]->xEnum.value ){
			DaoProcess_RaiseException( proc, DAO_ERROR, message->mbs );
		}
		project = daomake_type_project->value;
	}else{
		DaoMakeProject *proj = (DaoMakeProject*) project;
		DString_Reset( cache, 0 );
		DString_Append( cache, name );
		if( fout ) fprintf( fout, "%s = DaoMake::Project( \"%s\" )\n", cache->mbs, cache->mbs );
		for(i=0; i<proj->compilingFlags->size; ++i){
			DString *flag = proj->compilingFlags->items.pString[i];
			DString_ToMBS( flag );
			if( fout ) fprintf( fout, "%s.ExportCompilingFlags( \"%s\" )\n", cache->mbs, flag->mbs );
		}
		for(i=0; i<proj->linkingFlags->size; ++i){
			DString *flag = proj->linkingFlags->items.pString[i];
			DString_ToMBS( flag );
			if( fout ) fprintf( fout, "%s.ExportLinkingFlags( \"%s\" )\n", cache->mbs, flag->mbs );
		}
	}
	if( fout ) fclose( fout );
	DaoProcess_PutValue( proc, project );
	DString_Delete( original );
	DString_Delete( message );
	DString_Delete( cache );
}
static void DAOMAKE_FindFile( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *file = DaoValue_TryGetString( p[0] );
	DaoList *hints = (DaoList*) p[1];
	DString *path = DaoMake_FindFile( file, hints );
	DString *res = DaoProcess_PutMBString( proc, "" );
	if( path == NULL ) path = DaoMake_FindFile( file, daomake_includes );
	if( path ) DString_Assign( res, path );
}
static void DAOMAKE_OptionBOOL( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *name = DaoValue_TryGetString( p[0] );
	DaoEnum *value = DaoValue_CastEnum( p[1] );
	DaoEnum *res = (DaoEnum*) DaoProcess_PutValue( proc, p[1] );
	DNode *it = DMap_Find( daomake_boolean_options, name );
	if( it ) res->value = it->value.pInt;
}
static void DAOMAKE_OptionSTR( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *name = DaoValue_TryGetString( p[0] );
	DString *res = DaoValue_TryGetString( p[1] );
	DNode *it = DMap_Find( daomake_string_options, name );
	if( it ) res = it->value.pString;
	DaoProcess_PutString( proc, res );
}
static void DAOMAKE_IsFile( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *path = DaoValue_TryGetString( p[0] );
	DString_ToMBS( path );
	DaoMake_MakePath( proc->activeNamespace->path, path );
	DaoProcess_PutInteger( proc, Dao_IsFile( path->mbs ) );
}
static void DAOMAKE_IsDir( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *path = DaoValue_TryGetString( p[0] );
	DString_ToMBS( path );
	DaoMake_MakePath( proc->activeNamespace->path, path );
	DaoProcess_PutInteger( proc, Dao_IsDir( path->mbs ) );
}
static void DAOMAKE_Shell( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *res = DaoProcess_PutMBString( proc, "" );
	DString *cmd = DaoValue_TryGetString( p[0] );
	FILE *fin = popen( DString_GetMBS( cmd ), "r" );
	DaoFile_ReadAll( fin, res, 0 );
	pclose( fin );
}
static void DAOMAKE_SourcePath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns = proc->activeNamespace;
	DaoProcess_PutString( proc, ns->path );
}
static void DAOMAKE_BinaryPath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutString( proc, vmSpace->startPath );
}
static void DAOMAKE_BuildMode( DaoProcess *proc, DaoValue *p[], int N )
{
	static const char *const build_modes[] = { "release", "debug", "profile" };
	DaoProcess_PutEnum( proc, build_modes[ daomake_build_mode ] );
}
static void DAOMAKE_SetTestTool( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *tool = DaoValue_TryGetString( p[0] );
	DString *option = DaoValue_TryGetString( p[1] );
	DString_ToMBS( tool );
	DaoMake_MakePath( proc->activeNamespace->path, tool );
	DString_Reset( daomake_test_tool, 0 );
	DString_Reset( daomake_test_tool_option, 0 );
	DString_Append( daomake_test_tool, tool );
	DString_Append( daomake_test_tool_option, option );
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
static void DAOMAKE_IsBSD( DaoProcess *proc, DaoValue *p[], int N )
{
	DAOMAKE_IsPlatform( proc, "BSD" );
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
	{ DAOMAKE_FindPackage, "FindPackage( name : string, opt :enum<OPTIONAL,REQUIRED> = $OPTIONAL ) => Project" },
	{ DAOMAKE_FindFile,    "FindFile( file : string, hints : list<string> = {} ) => string" },

	{ DAOMAKE_OptionBOOL,  "Option( name : string, value : enum<OFF,ON> ) => enum<OFF,ON>" },
	{ DAOMAKE_OptionSTR,   "Option( name : string, value = '' ) => string" },

	{ DAOMAKE_Shell,       "Shell( command : string ) => string" },

	{ DAOMAKE_SourcePath,  "SourcePath() => string" },
	{ DAOMAKE_BinaryPath,  "BinaryPath() => string" },

	{ DAOMAKE_BuildMode,   "BuildMode() => enum<RELEASE,DEBUG,PROFILE>" },

	{ DAOMAKE_SetTestTool, "SetTestTool( test : string, log_option = '--log' )" },

	{ DAOMAKE_IsFile,      "IsFile( path : string ) => int" },
	{ DAOMAKE_IsDir,       "IsDir( path : string ) => int" },

	{ DAOMAKE_IsUnix,      "IsUnix() => int" },
	{ DAOMAKE_IsLinux,     "IsLinux() => int" },
	{ DAOMAKE_IsMacOSX,    "IsMacOSX() => int" },
	{ DAOMAKE_IsBSD,       "IsBSD() => int" },
	{ DAOMAKE_IsFreeBSD,   "IsFreeBSD() => int" },
	{ DAOMAKE_IsMinix,     "IsMinix() => int" },

	{ DAOMAKE_IsWin32,     "IsWin32() => int" },
	{ DAOMAKE_IsMinGW,     "IsMinGW() => int" },
	{ NULL, NULL }
};




static const char *const daomake_doc_options =
"DaoMake Options: \n\
    --platform    platform name for which to generate makefiles;\n\
    --mode        building mode (release, debug or profile);\n\
    --suffix      makefile suffix (default none);\n\
    --reset       reset package searching caches;\n\
    --help        print this help information;\n\
";

const char *const daomake_error_makefile_existing = 
"Error: existing Makefile was not generated by DaoMake:\n  %s\n"
"Please use a (different) Makefile extension with the \"--suffix\" option.\n\n";

static const char *const daomake_lang_compilers[] =
{
	".c" ,    "CC" ,
	".m" ,    "CC" ,
	".cc" ,   "CXX" ,
	".cpp" ,  "CXX" ,
	".cxx" ,  "CXX" ,
	".c++" ,  "CXX" ,
	".mm" ,   "CXX" ,
	".f" ,    "FC" ,
	NULL ,    NULL
};

static const char *const daomake_lang_linkers[] =
{
	".c" ,    "CC;CXX" ,
	".m" ,    "CC;CXX" ,
	".cc" ,   "CXX" ,
	".cpp" ,  "CXX" ,
	".cxx" ,  "CXX" ,
	".c++" ,  "CXX" ,
	".mm" ,   "CXX" ,
	".f" ,    "FC;CC;CXX" ,
	NULL ,    NULL
};

static void DaoMap_AddKeyValues( DaoMap *self, const char *const keyvalues[] )
{
	int i;
	for(i=0; keyvalues[i]; i+=2){
		DaoString key = { DAO_STRING,0,0,0,1,NULL};
		DaoString value = { DAO_STRING,0,0,0,1,NULL};
		DString sk = DString_WrapMBS( keyvalues[i] );
		DString sv = DString_WrapMBS( keyvalues[i+1] );
		key.data = & sk;
		value.data = & sv;
		DMap_Insert( self->items, & key, & value );
	}
}

int main( int argc, char **argv )
{
	int i, k, m;
	char *platform = NULL;
	char *mode = NULL;
	FILE *fin, *fout;
	DaoNamespace *nspace;
	DString *makefile = DString_New(1);
	DString *srcdir = DString_New(1);
	DString *source;
	DString *name;
	DNode *it;

	/* Utility subcommands: */
	if( strcmp( argv[1], "cat" ) == 0 ){
		for(i=2; i<argc; i++){
			fin = fopen( argv[i], "rb" );
			if( fin == NULL ) continue;
			DaoFile_ReadAll( fin, makefile, 1 );
			printf( "%s\n", makefile->mbs );
		}
		return 0;
	}else if( strcmp( argv[1], "echo" ) == 0 ){
		for(i=2; i<argc; i++){
			if( i > 2 ) printf( "\n" );
			printf( "%s", argv[i] );
		}
		printf( "\n" );
		return 0;
	}else if( strcmp( argv[1], "isfile" ) == 0 ){
		return Dao_IsFile( argv[2] ) == 0;
	}else if( strcmp( argv[1], "isdir" ) == 0 ){
		return Dao_IsDir( argv[2] ) == 0;
	}else if( strcmp( argv[1], "mkdir" ) == 0 ){
		return mkdir( argv[2], 0777 );
	}else if( strcmp( argv[1], "copy" ) == 0 ){
		if( argc < 4 ) return 1;
		fin = fopen( argv[2], "rb" );
		fout = fopen( argv[3], "w+b" );
		if( fin == NULL || fout == NULL ){
			if( fin ) fclose( fin );
			if( fout ) fclose( fout );
			return 1;
		}
		DaoFile_ReadAll( fin, makefile, 1 );
		DaoFile_WriteString( fout, makefile );
		fclose( fout );
		return 0;
	}

	vmSpace = DaoInit( argv[0] );

	daomake_makefile_paths = DMap_New(D_STRING,0);
	daomake_boolean_options = DMap_New(D_STRING,0);
	daomake_string_options = DMap_New(D_STRING,D_STRING);
	DString_SetMBS( makefile, "makefile.dao" );
	for(i=1; i<argc; i++){
		char *arg = argv[i];
		if( strcmp( arg, "--platform" ) == 0 ){
			if( (i + 1) == argc ) goto ErrorMissingArgValue;
			platform = argv[++i];
		}else if( strcmp( arg, "--mode" ) == 0 ){
			if( (i + 1) == argc ) goto ErrorMissingArgValue;
			mode = argv[++i];
			if( strcmp( mode, "release" ) == 0 ){
				daomake_build_mode = DAOMAKE_RELEASE;
			}else if( strcmp( mode, "debug" ) == 0 ){
				daomake_build_mode = DAOMAKE_DEBUG;
			}else if( strcmp( mode, "profile" ) == 0 ){
				daomake_build_mode = DAOMAKE_PROFILE;
			}else{
				goto ErrorInvalidArgValue;
			}
		}else if( strstr( arg, "--option-" ) == arg ){
			DString key = DString_WrapMBS( arg + 9 );
			DString value;
			daoint bl = -1;
			if( (i + 1) == argc ) goto ErrorMissingArgValue;
			value = DString_WrapMBS( argv[++i] );
			DMap_Insert( daomake_string_options, & key, & value );
			if( strcmp( argv[i], "FALSE" ) ==0 || strcmp( argv[i], "false" ) ==0 ){
				bl = 0;
			}else if( strcmp( argv[i], "NO" ) ==0 || strcmp( argv[i], "no" ) ==0 ){
				bl = 0;
			}else if( strcmp( argv[i], "OFF" ) ==0 || strcmp( argv[i], "off" ) ==0 ){
				bl = 0;
			}else if( strcmp( argv[i], "TRUE" ) ==0 || strcmp( argv[i], "true" ) ==0 ){
				bl = 1;
			}else if( strcmp( argv[i], "YES" ) ==0 || strcmp( argv[i], "yes" ) ==0 ){
				bl = 1;
			}else if( strcmp( argv[i], "ON" ) ==0 || strcmp( argv[i], "on" ) ==0 ){
				bl = 1;
			}
			if( bl >= 0 ) DMap_Insert( daomake_boolean_options, & key, (void*)bl );
		}else if( strcmp( arg, "--suffix" ) == 0 ){
			if( (i + 1) == argc ) goto ErrorMissingArgValue;
			daomake_makefile_suffix = argv[++i];
		}else if( strcmp( arg, "--reset" ) == 0 ){
			daomake_reset_cache = 1;
		}else if( strcmp( arg, "--help" ) == 0 ){
			printf( "%s\n", daomake_doc_options );
		}else if( arg[0] == '-' ){
			fprintf( stderr, "Error: unknown argument \"%s\"!\n", arg );
			return 1;
		}else if( (i + 1) == argc ){
			DString_SetMBS( makefile, argv[i] );
			if( Dao_IsDir( makefile->mbs ) ){
				DString *file = DString_New(1);
				const char *names[] = { "makefile", "Makefile", "make", "Make" };
				if( makefile->size && makefile->mbs[makefile->size-1] != '/' )
					DString_AppendChar( makefile, '/' );
				for(k=0; k<4; ++k){
					DString_Reset( file, 0 );
					DString_Append( file, makefile );
					DString_AppendMBS( file, names[k] );
					DString_AppendMBS( file, ".dao" );
					if( Dao_IsFile( file->mbs ) ){
						DString_Assign( makefile, file );
						break;
					}
				}
				DString_Delete( file );
			}
		}
		continue;
ErrorMissingArgValue:
		fprintf( stderr, "Error: missing argument value for \"%s\"!\n", arg );
		return 1;
ErrorInvalidArgValue:
		fprintf( stderr, "Error: invalid argument value for \"%s\"!\n", arg );
		return 1;
	}

	/* Use no hashing: the same string will be hashed differently in MBS and WCS! */
	daomake_projects = DaoMap_New(0);
	daomake_settings = DaoMap_New(0);
	daomake_compilers = DaoMap_New(0);
	daomake_linkers = DaoMap_New(0);
	daomake_includes = DaoList_New();
	DaoGC_IncRC( (DaoValue*) daomake_projects );
	DaoGC_IncRC( (DaoValue*) daomake_settings );
	DaoGC_IncRC( (DaoValue*) daomake_compilers );
	DaoGC_IncRC( (DaoValue*) daomake_linkers );
	DaoGC_IncRC( (DaoValue*) daomake_includes );

	nspace = DaoVmSpace_GetNamespace( vmSpace, "DaoMake" );
	daomake_type_unit    = DaoNamespace_WrapType( nspace, & DaoMakeUnit_Typer, 0 );
	daomake_type_objects = DaoNamespace_WrapType( nspace, & DaoMakeObjects_Typer, 0 );
	daomake_type_target  = DaoNamespace_WrapType( nspace, & DaoMakeTarget_Typer, 0 );
	daomake_type_project = DaoNamespace_WrapType( nspace, & DaoMakeProject_Typer, 0 );
	DaoNamespace_WrapFunctions( nspace, DaoMakeMeths );
	DaoNamespace_AddValue( nspace, "Settings", (DaoValue*) daomake_settings, "map<string,string>" );
	DaoNamespace_AddValue( nspace, "Compilers", (DaoValue*) daomake_compilers, "map<string,string>" );
	DaoNamespace_AddValue( nspace, "Linkers", (DaoValue*) daomake_linkers, "map<string,string>" );
	DaoNamespace_AddValue( nspace, "Includes", (DaoValue*) daomake_includes, "list<string>" );

	DaoMap_AddKeyValues( daomake_compilers, daomake_lang_compilers );
	DaoMap_AddKeyValues( daomake_linkers, daomake_lang_linkers );

	name = DString_New(1);
	DaoVmSpace_AddPath( vmSpace, vmSpace->daoBinPath->mbs );
#ifdef UNIX
	DString_SetMBS( name, "../shared/daomake" );
	Dao_MakePath( vmSpace->daoBinPath, name );
	DaoVmSpace_AddPath( vmSpace, name->mbs );
#endif
	if( platform ){
		DaoNamespace *pns;
		DString_SetMBS( name, "platforms/" );
		DString_AppendMBS( name, platform );
		DString_AppendMBS( name, ".dao" );
		pns = DaoVmSpace_Load( vmSpace, name->mbs );
		if( pns == NULL ){
			fprintf( stderr, "Error: invalid platform \"%s\"!\n", platform );
			return 1;
		}
	}

	DaoMake_MakePath( vmSpace->startPath, makefile );
	DString_Append( srcdir, makefile );
	while( srcdir->size && srcdir->mbs[srcdir->size-1] != '/' ) srcdir->size -= 1;
	if( srcdir->size && srcdir->mbs[srcdir->size-1] == '/' ) srcdir->size -= 1;
	srcdir->mbs[srcdir->size] = '\0';

	daomake_out_of_source = DString_EQ( srcdir, vmSpace->startPath ) == 0;
	daomake_main_source_path = srcdir;
	daomake_test_tool = DString_New(1);
	daomake_test_tool_option = DString_New(1);
	DString_SetMBS( daomake_test_tool, "daotest" );
	DString_SetMBS( daomake_test_tool_option, "--log" );

	/* Start execution. */
	k = ! DaoVmSpace_RunMain( vmSpace, makefile->mbs );

	source = DString_New(1);
	for(it=DaoMap_First(daomake_projects); it; it=DaoMap_Next(daomake_projects,it)){
		DaoMakeProject *project = (DaoMakeProject*) it->value.pVoid;
		if( (project->targets->size + project->installs->size) == 0 ) continue;
		DMap_Insert( daomake_makefile_paths, project->sourcePath, 0 );
	}

	for(it=DaoMap_First(daomake_projects); it; it=DaoMap_Next(daomake_projects,it)){
		DaoMakeProject *project = (DaoMakeProject*) it->value.pVoid;
		if( (project->targets->size + project->installs->size) == 0 ) continue;
		DString_Reset( name, 0 );
		DString_Append( name, project->sourcePath );
		DString_AppendMBS( name, "/Makefile" );
		DString_AppendMBS( name, daomake_makefile_suffix );
		DaoMake_MakeOutOfSourcePath( name );
		if( daomake_reset_cache == 0 ){
			fin = fopen( name->mbs, "r" );
			if( fin ){
				DaoFile_ReadAll( fin, source, 1 );
				if( DString_FindMBS( source, "# Generated by DaoMake:", 0 ) != 0 ){
					fprintf( stderr, daomake_error_makefile_existing, name->mbs );
					return 1;
				}
			}
		}
		fout = fopen( name->mbs, "w+" );
		fprintf( fout, "# Generated by DaoMake: DO NOT EDIT!\n" );
		fprintf( fout, "# Targeting platform %s.\n\n", platform ? platform : "none" );
		DaoMakeProject_MakeFile( project, source );
		DaoFile_WriteString( fout, source );
		fclose( fout );

		if( project->generateFinder == 0 ) continue;
		DaoMakeProject_MakeFindPackage( project, source );
		if( source->size == 0 ) continue;

		DString_Reset( name, 0 );
		DString_Append( name, project->sourcePath );
		DString_AppendMBS( name, "/Find" );
		DString_Append( name, project->projectName );
		DString_AppendMBS( name, ".dao" );
		DaoMake_MakeOutOfSourcePath( name );

		fout = fopen( name->mbs, "w+" );
		fprintf( fout, "# Generated by DaoMake: DO NOT EDIT!\n" );
		DaoFile_WriteString( fout, source );
		fclose( fout );
	}
	DString_Delete( name );
	DString_Delete( source );
	DString_Delete( makefile );
	DString_Delete( srcdir );

	DaoGC_DecRC( (DaoValue*) daomake_projects );
	DaoGC_DecRC( (DaoValue*) daomake_settings );
	DaoGC_DecRC( (DaoValue*) daomake_compilers );
	DaoGC_DecRC( (DaoValue*) daomake_linkers );
	DaoGC_DecRC( (DaoValue*) daomake_includes );

	DaoQuit();
	return k;
}
