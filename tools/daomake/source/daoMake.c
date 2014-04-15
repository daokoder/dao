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
#include<errno.h>

#include"daoValue.h"
#include"daoStdtype.h"
#include"daoNamespace.h"
#include"daoProcess.h"
#include"daoStream.h"
#include"daoVmspace.h"

#ifdef WIN32

#include"io.h"
#ifdef _MSC_VER
#define getcwd _getcwd
#define mkdir _mkdir
#define rmdir _rmdir
#define chmod _chmod
#endif

#endif

#ifdef UNIX
#include<sys/stat.h>
#include<unistd.h>
#include<dirent.h>
#endif

#ifdef LINUX
#define DAOMAKE_PLATFORM  "linux"
#elif defined( MAC_OSX )
#define DAOMAKE_PLATFORM  "macosx"
#elif defined( FREEBSD )
#define DAOMAKE_PLATFORM  "freebsd"
#elif defined( MINIX )
#define DAOMAKE_PLATFORM  "minix"
#elif defined( BEOS )
#define DAOMAKE_PLATFORM  "beos"
#elif defined( MINGW )
#define DAOMAKE_PLATFORM  "mingw"
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
	DAOMAKE_JAVASCRIPT ,
	DAOMAKE_TESTING ,
	DAOMAKE_COMMAND ,
	DAOMAKE_DIRECTORY
};

const char *const daomake_test_sumfile = "daotest_result_summary.txt";
const char *const daomake_objects_dir = "DaoMake.Objs";

const char *const daomake_mode_keys[] =
{
	"RELEASE-AFLAG" ,
	"RELEASE-CFLAG" ,
	"RELEASE-LFLAG" ,
	"DEBUG-AFLAG" ,
	"DEBUG-CFLAG" ,
	"DEBUG-LFLAG" ,
	"PROFILE-AFLAG" ,
	"PROFILE-CFLAG" ,
	"PROFILE-LFLAG"
};

const char *const daomake_prefix_keys[] =
{
	"" ,
	"DLL-PREFIX" ,
	"LIB-PREFIX" ,
	"" ,
	"" ,
	"" ,
	""
};

const char *const daomake_suffix_keys[] =
{
	"EXE-SUFFIX" ,
	"DLL-SUFFIX" ,
	"LIB-SUFFIX" ,
	"" ,
	"" ,
	"" ,
	""
};



struct DaoMakeUnit
{
	DAO_CSTRUCT_COMMON;

	DaoMakeProject *project;

	DArray  *definitions;
	DArray  *includePaths;
	DArray  *linkingPaths;
	DArray  *assemblingFlags;
	DArray  *compilingFlags;
	DArray  *linkingFlags;
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
	DString  *extradeps;
	DString  *testMacro;
	DString  *path;
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
	DString  *binaryPath;
	DString  *projectName;
	uchar_t   generateFinder;

	DArray   *targets;
	DArray   *variables;
	DArray   *installs;
	DMap     *tests;

	DMap     *exportPaths;
	DMap     *exportCFlags;
	DMap     *exportLFlags;
	DMap     *exportShlibs;
	DMap     *exportStlibs;

	/*
	// In the following maps:
	// the keys are the macro names or the target names;
	// the values are the entire macro or rule;
	*/
	DMap     *headerMacros;   /* HEADERS = header1.h header2.h; */
	DMap     *aflagsMacros;   /* AFLAGS = ...; */
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
static DaoMap  *daomake_assemblers = NULL;
static DaoMap  *daomake_compilers = NULL;
static DaoMap  *daomake_linkers = NULL;
static DaoList *daomake_includes = NULL;

static DMap *daomake_boolean_options = NULL;
static DMap *daomake_string_options = NULL;

static DMap *daomake_cmdline_defines = NULL;

static DMap *daomake_makefile_paths = NULL;

static DaoType *daomake_type_unit = NULL;
static DaoType *daomake_type_objects = NULL;
static DaoType *daomake_type_target  = NULL;
static DaoType *daomake_type_project = NULL;

static DString *daomake_platform = NULL;
static DString *daomake_current_path = NULL;
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
	self->project = NULL;
	self->definitions = DArray_New(D_STRING);
	self->includePaths = DArray_New(D_STRING);
	self->linkingPaths = DArray_New(D_STRING);
	self->assemblingFlags = DArray_New(D_STRING);
	self->compilingFlags = DArray_New(D_STRING);
	self->linkingFlags = DArray_New(D_STRING);
	self->staticLibNames = DArray_New(D_STRING);
}
void DaoMakeUnit_Free( DaoMakeUnit *self )
{
	DaoCstruct_Free( (DaoCstruct*) self );
	DArray_Delete( self->definitions );
	DArray_Delete( self->includePaths );
	DArray_Delete( self->linkingPaths );
	DArray_Delete( self->assemblingFlags );
	DArray_Delete( self->compilingFlags );
	DArray_Delete( self->linkingFlags );
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
	self->extradeps = DString_New(1);
	self->testMacro = DString_New(1);
	self->path = DString_New(1);
	self->install = DString_New(1);
	self->ttype = DAOMAKE_EXECUTABLE;
	self->dynamicLinking = 1;
	self->dynamicExporting = 1;
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
	DString_Delete( self->extradeps );
	DString_Delete( self->testMacro );
	DString_Delete( self->path );
	DString_Delete( self->install );
	dao_free( self );
}
DaoMakeProject* DaoMakeProject_New()
{
	DaoMakeProject *self = (DaoMakeProject*) dao_calloc( 1, sizeof(DaoMakeProject) );
	DaoMakeUnit_Init( (DaoMakeUnit*) & self->base, daomake_type_project );
	self->sourceName = DString_New(1);
	self->sourcePath = DString_New(1);
	self->binaryPath = DString_New(1);
	self->projectName = DString_New(1);

	self->targets = DArray_New(D_VALUE);
	self->variables = DArray_New(D_STRING);
	self->installs = DArray_New(D_VALUE);
	self->tests = DMap_New(D_STRING,D_ARRAY);

	self->exportPaths = DMap_New(D_STRING,D_STRING);
	self->exportCFlags = DMap_New(D_STRING,D_STRING);
	self->exportLFlags = DMap_New(D_STRING,D_STRING);
	self->exportShlibs = DMap_New(D_STRING,D_STRING);
	self->exportStlibs = DMap_New(D_STRING,D_STRING);

	self->headerMacros = DMap_New(D_STRING,D_STRING);
	self->aflagsMacros = DMap_New(D_STRING,D_STRING);
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
	DString_Delete( self->binaryPath );
	DString_Delete( self->projectName );

	DArray_Delete( self->targets );
	DArray_Delete( self->variables );
	DArray_Delete( self->installs );
	DMap_Delete( self->tests );

	DMap_Delete( self->exportPaths );
	DMap_Delete( self->exportCFlags );
	DMap_Delete( self->exportLFlags );
	DMap_Delete( self->exportShlibs );
	DMap_Delete( self->exportStlibs );

	DMap_Delete( self->headerMacros );
	DMap_Delete( self->aflagsMacros );
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
static int DString_CommonPrefixLength( DString *self, DString *other )
{
	daoint i = 0;
	if( other->mbs ){
		if( self->mbs == NULL ) DString_ToMBS( self );
	}else if( self->wcs == NULL ){
		DString_ToWCS( self );
	}
	if( self->mbs ){
		while( i < self->size && i < other->size && self->mbs[i] == other->mbs[i] ) i += 1;
	}else{
		while( i < self->size && i < other->size && self->wcs[i] == other->wcs[i] ) i += 1;
	}
	return i;
}



DString* DaoMake_GetSettingValue( const char *key )
{
	DaoValue *value = DaoMap_GetValueMBS( daomake_settings, key );
	if( value == NULL ) return NULL;
	return DaoValue_TryGetString( value );
}
int DaoMake_IsFile( const char *path )
{
#ifdef WIN32
	int att = GetFileAttributes( path );
	if( att == -1 ) return 0;
	return !(att & FILE_ATTRIBUTE_DIRECTORY);
#else
	return Dao_IsFile( path ); /* Does not work for ".." in msys environment; */
#endif
}
int DaoMake_IsDir( const char *path )
{
#ifdef WIN32
	int att = GetFileAttributes( path );
	if( att == -1 ) return 0;
	return att & FILE_ATTRIBUTE_DIRECTORY;
#else
	return Dao_IsDir( path ); /* Does not work for ".." in msys environment; */
#endif
}
int DaoMake_MakeDir( const char *dir )
{
#ifdef WIN32
	return mkdir( dir );
#else
	return mkdir( dir, 0777 );
#endif
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
		DString_AppendPathSep( fname );
		DString_Append( fname, file );
		if( DaoMake_IsFile( fname->mbs ) ){
			res = path;
			break;
		}
	}
	DString_Delete( fname );
	return res;
}
void DaoMake_MakeOutOfSourcePath( DString *path, int isfile )
{
	DString *binpath = vmSpace->startPath;
	daoint k = binpath->size + 1;

	if( daomake_out_of_source == 0 ) return;
	DString_Insert( path, binpath, 0, daomake_main_source_path->size, binpath->size );

	while( k < path->size ){
		while( k < path->size && path->mbs[k] != '/' ) k += 1;
		if( k == path->size ) break;
		path->mbs[k] = '\0';
		if( DaoMake_IsDir( path->mbs ) == 0 && DaoMake_IsFile( path->mbs ) == 0 ){
			DaoMake_MakeDir( path->mbs );
		}
		path->mbs[k] = '/';
		k += 1;
	}
	if( isfile ) return;
	if( DaoMake_IsDir( path->mbs ) == 0 && DaoMake_IsFile( path->mbs ) == 0 ){
		DaoMake_MakeDir( path->mbs );
	}
}
void Dao_MakePath( DString *base, DString *path );
void DaoMake_MakePath( DString *base, DString *path )
{
	DString_ToMBS( path );
	if( path->size == 0 ) return;
#ifdef WIN32
	if( path->size >= 2 && isalpha( path->mbs[0] ) && path->mbs[1] == ':' ) goto Finalize;
#else
	if( path->mbs[0] == '/' ) goto Finalize;
#endif
	Dao_MakePath( base, path );
Finalize:
	if( DaoMake_IsDir( path->mbs ) ) DString_AppendPathSep( path );
}
void DaoMakeProject_MakeSourcePath( DaoMakeProject *self, DString *path )
{
	DaoMake_MakePath( self->sourcePath, path );
}
void DaoMakeProject_MakeBinaryPath( DaoMakeProject *self, DString *path )
{
	DaoMake_MakePath( self->binaryPath, path );
}
void DaoMakeProject_MakeRelativePath( DString *current, DString *path )
{
	daoint i = 0;
	current = DString_Copy( current );
	DString_ToMBS( current );
	DString_ToMBS( path );
	DString_AppendPathSep( current );
	while( i < current->size && i < path->size && current->mbs[i] == path->mbs[i] ) i += 1;
	if( i == 0 ) goto Finalize;
	while( i >= 0 && path->mbs[i] != '/' ) i -= 1;
	if( i <= 0 ) goto Finalize;
	DString_Erase( current, 0, i + 1 );
	DString_Erase( path, 0, i + 1 );
	i = 0;
	while( i < current->size ){
		while( current->mbs[i] != '/' ) i += 1;
		DString_InsertMBS( path, "../", 0, 0, 0 );
		i += 1;
	}
	if( path->size == 0 ) DString_AppendChar( path, '.' );
Finalize:
	DString_Delete( current );
}




static void DString_AppendDefinition( DString *defs, DString *key, DString *value )
{
	DString_AppendGap( defs );
	DString_AppendMBS( defs, "-D" );
	DString_Append( defs, key );
	if( value->size ){
		DString_AppendChar( defs, '=' );
		DString_Append( defs, value );
	}
}
void DaoMakeUnit_MakeDefinitions( DaoMakeUnit *self, DString *defs )
{
	DNode *it;
	daoint i;
	for(i=0; i<self->definitions->size; i+=2){
		DString *definition = self->definitions->items.pString[i];
		DString *value = self->definitions->items.pString[i+1];
		DString_AppendDefinition( defs, definition, value );
	}
	if( self->ctype != daomake_type_objects ) return;
	for(it=DMap_First(daomake_cmdline_defines); it; it=DMap_Next(daomake_cmdline_defines,it)){
		DString_AppendDefinition( defs, it->key.pString, it->value.pString );
	}
}
void DaoMakeUnit_MakeIncludePathsEx( DaoMakeUnit *self, DString *cflags, DString *refpath )
{
	DString *path = DString_New(1);
	daoint i;
	for(i=0; i<self->includePaths->size; ++i){
		DString_Assign( path, self->includePaths->items.pString[i] );
		DaoMakeProject_MakeSourcePath( self->project, path );
		if( refpath ) DaoMakeProject_MakeRelativePath( refpath, path );
		DString_AppendGap( cflags );
		DString_AppendMBS( cflags, "-I" );
		DString_Append( cflags, path );
	}
	DString_Delete( path );
}
void DaoMakeUnit_MakeIncludePaths( DaoMakeUnit *self, DString *cflags )
{
	DaoMakeUnit_MakeIncludePathsEx( self, cflags, self->project->binaryPath );
}
void DaoMakeUnit_MakeLinkingPathsEx( DaoMakeUnit *self, DString *lflags, DString *refpath )
{
	DString *rpath = DaoMake_GetSettingValue( "DLL-RPATH" );
	DString *path = DString_New(1);
	daoint i;
	for(i=0; i<self->linkingPaths->size; ++i){
		DString_Assign( path, self->linkingPaths->items.pString[i] );
		DaoMakeProject_MakeBinaryPath( self->project, path );
		DString_AppendGap( lflags );
		DString_Append( lflags, rpath );
		DString_Append( lflags, path );
		if( refpath ) DaoMakeProject_MakeRelativePath( refpath, path );
		DString_AppendGap( lflags );
		DString_AppendMBS( lflags, "-L" );
		DString_Append( lflags, path );
	}
	DString_Delete( path );
}
void DaoMakeUnit_MakeLinkingPaths( DaoMakeUnit *self, DString *lflags )
{
	DaoMakeUnit_MakeLinkingPathsEx( self, lflags, self->project->binaryPath );
}
void DaoMakeUnit_MakeAssemblingFlagsEx( DaoMakeUnit *self, DString *aflags, DString *refpath )
{
	daoint i, j;
	DString_AppendGap( aflags );
	for(i=0; i<self->assemblingFlags->size; ++i){
		DString_AppendGap( aflags );
		DString_Append( aflags, self->assemblingFlags->items.pString[i] );
	}
	DaoMakeUnit_MakeIncludePathsEx( self, aflags, refpath );
	DaoMakeUnit_MakeDefinitions( self, aflags );
}
void DaoMakeUnit_MakeAssemblingFlags( DaoMakeUnit *self, DString *aflags )
{
	DaoMakeUnit_MakeAssemblingFlagsEx( self, aflags, self->project->binaryPath );
}
void DaoMakeUnit_MakeCompilingFlagsEx( DaoMakeUnit *self, DString *cflags, DString *refpath )
{
	daoint i, j;
	DString_AppendGap( cflags );
	for(i=0; i<self->compilingFlags->size; ++i){
		DString_AppendGap( cflags );
		DString_Append( cflags, self->compilingFlags->items.pString[i] );
	}
	DaoMakeUnit_MakeIncludePathsEx( self, cflags, refpath );
	DaoMakeUnit_MakeDefinitions( self, cflags );
}
void DaoMakeUnit_MakeCompilingFlags( DaoMakeUnit *self, DString *cflags )
{
	DaoMakeUnit_MakeCompilingFlagsEx( self, cflags, self->project->binaryPath );
}
void DaoMakeUnit_MakeLinkingFlagsEx( DaoMakeUnit *self, DString *lflags, DString *refpath )
{
	daoint i, j;
	DString_AppendGap( lflags );
	DaoMakeUnit_MakeLinkingPathsEx( self, lflags, refpath );
	for(i=0; i<self->linkingFlags->size; ++i){
		DString_AppendGap( lflags );
		DString_Append( lflags, self->linkingFlags->items.pString[i] );
	}
}
void DaoMakeUnit_MakeLinkingFlags( DaoMakeUnit *self, DString *lflags )
{
	DaoMakeUnit_MakeLinkingFlagsEx( self, lflags, self->project->binaryPath );
}

static void DaoMakeTarget_MakeCompilingFlagsEx( DaoMakeTarget *self, DaoMakeProject *pro, DString *flags, DString *refpath )
{
	daoint i;
	if( pro ) DaoMakeUnit_MakeCompilingFlagsEx( & pro->base, flags, refpath );
	DaoMakeUnit_MakeCompilingFlagsEx( & self->base, flags, refpath );
	for(i=0; i<self->objects->size; ++i){
		DaoMakeObjects *objects = (DaoMakeObjects*) self->objects->items.pVoid[i];
		DaoMakeUnit_MakeCompilingFlagsEx( & objects->base, flags, refpath );
	}
}

static void DaoMakeTarget_MakeLinkingFlagsEx( DaoMakeTarget *self, DaoMakeProject *pro, DString *flags, DString *refpath )
{
	daoint i;
	if( pro ) DaoMakeUnit_MakeLinkingFlagsEx( & pro->base, flags, refpath );
	DaoMakeUnit_MakeLinkingFlagsEx( & self->base, flags, refpath );
	for(i=0; i<self->objects->size; ++i){
		DaoMakeObjects *objects = (DaoMakeObjects*) self->objects->items.pVoid[i];
		DaoMakeUnit_MakeLinkingFlagsEx( & objects->base, flags, refpath );
	}
}

static void DaoMakeTarget_MakeCompilingFlags( DaoMakeTarget *self, DaoMakeProject *pro, DString *flags )
{
	DaoMakeTarget_MakeCompilingFlagsEx( self, pro, flags, self->base.project->binaryPath );
}
static void DaoMakeTarget_MakeLinkingFlags( DaoMakeTarget *self, DaoMakeProject *pro, DString *flags )
{
	DaoMakeTarget_MakeLinkingFlagsEx( self, pro, flags, self->base.project->binaryPath );
}


DString* DaoMakeProject_GetBufferString( DaoMakeProject *self )
{
	if( self->usedStrings >= self->strings->size )
		DArray_Append( self->strings, self->strings->items.pString[0] );
	self->usedStrings += 1;
	DString_Reset( self->strings->items.pString[ self->usedStrings - 1 ], 0 );
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
		DaoMakeProject_MakeRelativePath( self->binaryPath, file );
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
DString* DaoMakeProject_MakeAFlagsMacro( DaoMakeProject *self, DString *flags )
{
	return DaoMakeProject_MakeSimpleMacro( self, self->aflagsMacros, flags, "AFLAGS" );
}

const char* DaoMakeProject_GetFileExtension( DString *file )
{
	daoint pos;
	DString_ToMBS( file );
	pos = DString_RFindChar( file, '.', -1 );
	if( pos == MAXSIZE ) return "";
	return file->mbs + pos;
}
DString* DaoMakeProject_GetProgramMacro( DaoMap *map, DString *file )
{
	DaoValue *value;
	const char *ext = DaoMakeProject_GetFileExtension( file );
	if( ext == NULL ) return NULL;
	value = DaoMap_GetValueMBS( map, ext );
	if( value == NULL ) return NULL;
	return DaoValue_TryGetString( value );
}
DString* DaoMakeProject_GetLanguageAssembler( DString *file )
{
	return DaoMakeProject_GetProgramMacro( daomake_assemblers, file );
}
DString* DaoMakeProject_GetLanguageCompiler( DString *file )
{
	return DaoMakeProject_GetProgramMacro( daomake_compilers, file );
}
DString* DaoMakeProject_GetLanguageLinker( DString *file )
{
	return DaoMakeProject_GetProgramMacro( daomake_linkers, file );
}

/* Return object file name: */
DString* DaoMakeProject_MakeObjectRule( DaoMakeProject *self, DaoMakeTarget *target, DaoMakeObjects *objects, DString *source )
{
	DString *source2 = DaoMakeProject_GetBufferString( self );
	DString *cflags = DaoMakeProject_GetBufferString( self );
	DString *cflag = DaoMakeProject_GetBufferString( self );
	DString *signature = DaoMakeProject_GetBufferString( self );
	DString *assembler = DaoMakeProject_GetLanguageAssembler( source );
	DString *compiler = DaoMakeProject_GetLanguageCompiler( source );
	DString *md5 = self->mbs;
	DString *mode;
	DNode *it;
	daoint pos;

	DString_Reset( cflags, 0 );
	DString_Append( source2, source );
	DaoMakeProject_MakeSourcePath( self, source2 );
	DaoMakeProject_MakeRelativePath( self->binaryPath, source2 );

	if( assembler ){
		mode = DaoMake_GetSettingValue( daomake_mode_keys[ 3*daomake_build_mode ] );
		if( mode ) DString_Append( cflags, mode );

		DaoMakeUnit_MakeAssemblingFlags( & self->base, cflags );
		DaoMakeUnit_MakeAssemblingFlags( & target->base, cflags );
		DaoMakeUnit_MakeAssemblingFlags( & objects->base, cflags );
	}else{
		mode = DaoMake_GetSettingValue( daomake_mode_keys[ 3*daomake_build_mode+1 ] );
		if( mode ) DString_Append( cflags, mode );

		DaoMakeUnit_MakeCompilingFlags( & self->base, cflags );
		DaoMakeUnit_MakeCompilingFlags( & target->base, cflags );
		DaoMakeUnit_MakeCompilingFlags( & objects->base, cflags );
	}

	DString_Assign( signature, cflags );
	DString_AppendGap( signature );
	DString_Append( signature, source );

	md5 = DaoMakeProject_SubMD5( self, signature );

	/* Unique (quasi) target name: */
	DString_Reset( signature, 0 );
	DString_Append( signature, source );
	pos = DString_RFindChar( signature, '/', -1 );
	if( pos != MAXSIZE ) DString_Erase( signature, 0, pos + 1 );
	DString_InsertMBS( signature, "DaoMake.Objs/", 0, 0, 0 );
	DString_AppendChar( signature, '.' );
	DString_Append( signature, md5 );
	if( target->ttype == DAOMAKE_JAVASCRIPT ) DString_AppendMBS( signature, ".ll" );
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
	if( target->ttype == DAOMAKE_JAVASCRIPT ) DString_AppendMBS( it->value.pString, "EM" );
	if( assembler ){
		DString_Append( it->value.pString, assembler );
		/* #include main appear in .S file: */
		DString_AppendMBS( it->value.pString, ") $(" );
		DString_Append( it->value.pString, DaoMakeProject_MakeAFlagsMacro( self, cflags ) );
		DString_AppendMBS( it->value.pString, ") -c " );
	}else{
		if( compiler ){
			DString_Append( it->value.pString, compiler );
		}else{
			DString_AppendMBS( it->value.pString, "CC" );
		}
		DString_AppendMBS( it->value.pString, ") $(" );
		DString_Append( it->value.pString, DaoMakeProject_MakeCFlagsMacro( self, cflags ) );
		DString_AppendMBS( it->value.pString, ") -c " );
	}
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
	if( self->ttype == DAOMAKE_JAVASCRIPT ) DString_AppendMBS( name, ".js" );
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
	DString_AppendGap( deps );
	DString_Append( deps, target->extradeps );
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
			DString_AppendMBS( signature, daomake_objects_dir );
			DString_AppendPathSep( signature);
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
			DString_AppendMBS( it->value.pString, " $(DAOTEST)\n\t-$(DAOTEST) " );
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

	mode = DaoMake_GetSettingValue( daomake_mode_keys[ 3*daomake_build_mode+2 ] );
	if( mode ) DString_Append( lflags, mode );

	DaoMakeUnit_MakeLinkingFlags( & self->base, lflags );
	DaoMakeUnit_MakeLinkingFlags( & target->base, lflags );

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

		DaoMakeUnit_MakeLinkingFlags( & objects->base, lflags );

		DString_AppendGap( objs );
		DString_AppendMBS( objs, "$(" );
		DString_Append( objs, objmacro );
		DString_AppendChar( objs, ')' );
	}
	DString_Append( rule, objs );
	if( target->ttype == DAOMAKE_STATICLIB ){
		DString *arc = DaoMake_GetSettingValue( "AR" );
		DString_AppendMBS( rule, "\n\t" );
		DString_AppendMBS( rule, "-@$(DAOMAKE) remove " );
		DString_Append( rule, tname );
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
			DString *flag2 = DaoMake_GetSettingValue( "DLL-NAME" );
			if( flag ){
				DString_AppendGap( lflags );
				DString_Append( lflags, flag );
			}
			if( flag2 ){
				DString_AppendGap( lflags );
				DString_Append( lflags, flag2 );
				DString_Append( lflags, tname );
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
		if( target->ttype == DAOMAKE_JAVASCRIPT ) DString_AppendMBS( rule, "EM" );
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

void DaoMakeProject_MakeInstallPath( DaoMakeProject *self, DString *path, DString *install, DString *uninstall, DMap *mapPaths, int top )
{
	DString *sub;

	if( DaoMake_IsDir( path->mbs ) ) return;
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

	if( sub->size && DaoMake_IsDir( sub->mbs ) == 0 ){
		DaoMakeProject_MakeInstallPath( self, sub, install, uninstall, mapPaths, 0 );
	}
	DString_AppendMBS( install, "\t$(DAOMAKE) mkdir2 " );
	DString_Append( install, path );
	DString_AppendChar( install, '\n' );

	if( top ){
		DString_AppendMBS( uninstall, "\t$(DAOMAKE) remove " );
		DString_Append( uninstall, path );
		DString_AppendChar( uninstall, '\n' );
	}

	self->usedStrings -= 1;
}
void DaoMakeProject_MakeCopy( DaoMakeProject *self, DString *src, DString *dest, DString *output )
{
	DString_AppendMBS( output, "\t$(DAOMAKE) copy " );
	DString_Append( output, src );
	DString_AppendChar( output, ' ' );
	DString_Append( output, dest );
	DString_AppendChar( output, '\n' );
}
void DaoMakeProject_MakeRemove( DaoMakeProject *self, DString *file, DString *path, DString *output )
{
	DString_ToMBS( path );
	DString_AppendMBS( output, "\t$(DAOMAKE) remove " );
	DString_Append( output, path );
	if( file ){
		daoint pos;
		char *file2;
		DString_ToMBS( file );
		pos = DString_RFindChar( file, '/', -1 );
		file2 = pos == MAXSIZE ? file->mbs : file->mbs + pos + 1;
		DString_AppendPathSep( output );
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
		if( DaoMake_IsDir( path->mbs ) == 0 ){
			DaoMakeProject_MakeInstallPath( self, path, install, uninstall, mapPaths, 1 );
		}else{
			DaoMakeProject_MakeRemove( self, tname, path, uninstall );
		}
		DaoMakeProject_MakeRelativePath( self->binaryPath, tname );
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

	if( 1 ){
		DString *all = DaoMakeProject_GetBufferString( self );
		DString *phony = DaoMakeProject_GetBufferString( self );
		DString *test = DaoMakeProject_GetBufferString( self );
		DString *testsum = DaoMakeProject_GetBufferString( self );
		for(i=0; i<self->targets->size; ++i){
			DaoMakeTarget *target = (DaoMakeTarget*) self->targets->items.pVoid[i];
			DString *ruleName = DaoMakeProject_MakeTargetRule( self, target );
			if( target->ttype == DAOMAKE_TESTING ){
				DString_AppendGap( test );
				DString_Append( test, ruleName );
				continue;
			}
			DString_AppendGap( all );
			DString_Append( all, ruleName );
			DString_Trim( ruleName );
			if( target->ttype == DAOMAKE_COMMAND && DString_MatchMBS( ruleName, "%W", NULL, NULL ) ==0 ){
				DString_AppendGap( phony );
				DString_Append( phony, ruleName );
			}else if( target->ttype >= DAOMAKE_DIRECTORY ){
				DString_AppendGap( phony );
				DString_Append( phony, ruleName );
			}
		}
		DString_AppendMBS( makefile, "all: " );
		DString_Append( makefile, all );
		if( ismain && daomake_test_count && daomake_build_mode ){
			DString_AppendGap( makefile );
			DString_AppendMBS( makefile, "test" );
		}
		DString_AppendMBS( makefile, "\n\n" );

		DString_AppendMBS( makefile, ".PHONY: test\n" );
		DString_AppendMBS( makefile, "test: " );
		DString_Append( makefile, all );
		DString_AppendGap( makefile );
		DString_Append( makefile, test );

		DString_AppendGap( makefile );
		DString_AppendMBS( makefile, "subtest" );
		DString_AppendGap( makefile );
		if( ismain && daomake_test_count ){
			DString_AppendMBS( makefile, "testsum" );
		}
		DString_AppendMBS( makefile, "\n\n" );
		if( phony->size ){
			DString_AppendMBS( makefile, ".PHONY: " );
			DString_Append( makefile, phony );
			DString_AppendChar( makefile, '\n' );
		}
		self->usedStrings -= 4;
	}

	DString_AppendMBS( makefile, "\nDAOMAKE = " );
	DString_Append( makefile, vmSpace->daoBinPath );
	DString_AppendPathSep( makefile );
	DString_AppendMBS( makefile, "daomake\n\n" );

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

	for(it=DMap_First(self->aflagsMacros); it; it=DMap_Next(self->aflagsMacros,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendMBS( makefile, "\n\n" );
	}
	DString_AppendChar( makefile, '\n' );

	for(it=DMap_First(self->cflagsMacros); it; it=DMap_Next(self->cflagsMacros,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendMBS( makefile, "\n\n" );
	}
	DString_AppendChar( makefile, '\n' );

	for(it=DMap_First(self->lflagsMacros); it; it=DMap_Next(self->lflagsMacros,it)){
		DString_Append( makefile, it->value.pString );
		DString_AppendMBS( makefile, "\n\n" );
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
		DString *suffix = DaoMake_GetSettingValue( daomake_suffix_keys[ DAOMAKE_EXECUTABLE ] );
		DString_AppendMBS( makefile, "DAOTEST = " );
		DString_Append( makefile, daomake_test_tool );
		DString_Append( makefile, suffix );
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
	DString_AppendPathSep( makefile );
	DString_AppendMBS( makefile, daomake_test_sumfile );
	DString_AppendMBS( makefile, "\n\n" );

	DString_AppendMBS( makefile, "testsum:\n" );
	if( ismain ){
		DString_AppendMBS( makefile, "\t@$(DAOMAKE) echo \"Summarizing test results ...\"\n" );
		DString_AppendMBS( makefile, "\t-@$(DAOMAKE) remove " );
		DString_AppendMBS( makefile, " $(TESTSUM)\n" );
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
		DString_AppendMBS( makefile, "$(DAOMAKE) remove" );
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

	DString_AppendMBS( makefile, "distclean:\n\t" );
	if( self->objectsMacros->size + self->testsMacros->size ){
		DString_AppendMBS( makefile, "$(DAOMAKE) remove " );
		DString_AppendMBS( makefile, daomake_objects_dir );
		DString_AppendChar( makefile, '\n' );
	}
	DaoMakeProject_MakeDirectoryMake( self, makefile, "distclean" );
	DString_AppendChar( makefile, '\n' );
	DString_AppendMBS( makefile, ".PHONY: distclean\n\n" );

	/* Regenerate if there was MD5 signature conflict: */
	if( self->signature != sig ) DaoMakeProject_MakeFile( self, makefile );
}

static void DaoMakeTarget_MakeExport( DaoMakeTarget *self, DString *cflags, DString *lflags, DString *shlibs, DString *stlibs, DMap *incdirs, DMap *lnkdirs )
{
	DaoMakeProject *project = self->base.project;
	DaoValue **installs = project->installs->items.pValue;
	DString *path = DaoMakeProject_GetBufferString( project );
	DString *key = DaoMakeProject_GetBufferString( project );
	DMap *headers = DMap_New(D_STRING,0);
	DNode *it;
	daoint i, j;
	for(i=0; i<self->objects->size; ++i){
		DaoMakeObjects *objs = (DaoMakeObjects*) DArray_Item( self->objects, i );
		for(j=0; j<objs->headers->size; ++j){
			DMap_Insert( headers, DArray_String( objs->headers, j ), 0 );
		}
	}
	for(i=0; i<project->installs->size; i+=2){
		DString *dest = DaoValue_TryGetString( installs[i+1] );
		DString *file = DaoValue_TryGetString( installs[i] );
		if( file == NULL ) continue;
		/*
		// "dest" is added to "-I" compiling flags, only if:
		// 1. "file" is a header file to be installed to "dest"; Or,
		// 2. "file" is a directory, which contains one of the used header files;
		*/
		if( DMap_Find( headers, file ) == NULL ){
			DString_ToMBS( file );
			if( DaoMake_IsDir( file->mbs ) == 0 ) continue;
			/* Check if the directory contains one of the header files: */
			for(it=DMap_First(headers); it; it=DMap_Next(headers,it)){
				if( DString_Find( it->key.pString, file, 0 ) == 0 ) break;
			}
			if( it== NULL ) continue;
		}
		DString_Reset( key, 0 );
		DString_Reset( path, 0 );
		DString_Append( path, dest );
		DaoMakeProject_MakeSourcePath( project, path );
		DString_Append( key, self->name );
		DString_AppendMBS( key, "::::" );
		DString_Append( key, path );
		if( DMap_Find( incdirs, key ) ) continue;
		DMap_Insert( incdirs, key, 0 );
		DString_AppendGap( cflags );
		DString_AppendMBS( cflags, "-I" );
		DString_Append( cflags, path );
	}
	DString_Reset( key, 0 );
	DString_Append( key, self->name );
	DString_AppendMBS( key, "::::" );
	DString_Append( key, self->install );
	if( DMap_Find( lnkdirs, key ) == NULL ){
		DMap_Insert( lnkdirs, key, 0 );
		DString_AppendGap( lflags );
		DString_AppendMBS( lflags, "-L" );
		DString_Append( lflags, self->install );
	}
	if( self->ttype == DAOMAKE_SHAREDLIB ){
		DString_AppendMBS( shlibs, "-l" );
		DString_Append( shlibs, self->name );
	}else if( self->ttype == DAOMAKE_STATICLIB ){
		DString_AppendGap( stlibs );
		DString_Append( stlibs, self->install );
		DString_AppendPathSep( stlibs );
		DaoMakeTarget_MakeName( self, path );
		DString_Append( stlibs, path );
	}
	project->usedStrings -= 2;
}
void DString_AppendVerbatim( DString *self, DString *text, DString *md5 )
{
	DString_MD5( text, md5 );
	DString_Reset( md5, 8 );
	DString_ToUpper( md5 );
	DString_AppendMBS( self, "@[" );
	DString_Append( self, md5 );
	DString_AppendMBS( self, "]" );
	DString_Append( self, text );
	DString_AppendMBS( self, "@[" );
	DString_Append( self, md5 );
	DString_AppendMBS( self, "]" );
}

typedef struct DaoMakeProjMaps DaoMakeProjMaps;
struct DaoMakeProjMaps
{
	DMap *mapCFlags;
	DMap *mapLFlags;
	DMap *mapShlibs;
	DMap *mapStlibs;
};

DaoMakeProjMaps* DaoMakeProjMaps_New()
{
	DaoMakeProjMaps *self = (DaoMakeProjMaps*) dao_malloc(sizeof(DaoMakeProjMaps));
	self->mapCFlags = DMap_New(D_STRING,D_STRING);
	self->mapLFlags = DMap_New(D_STRING,D_STRING);
	self->mapShlibs = DMap_New(D_STRING,D_STRING);
	self->mapStlibs = DMap_New(D_STRING,D_STRING);
	return self;
}
void DaoMakeProjMaps_Delete( DaoMakeProjMaps *self )
{
	DMap_Delete( self->mapCFlags );
	DMap_Delete( self->mapLFlags );
	DMap_Delete( self->mapShlibs );
	DMap_Delete( self->mapStlibs );
	dao_free( self );
}

static void DaoMakeProjMaps_TryInitKey( DaoMakeProjMaps *self, DString *name, DString *cflags, DString *lflags, DString *shlibs, DString *stlibs )
{
	if( DMap_Find( self->mapCFlags, name ) == NULL ){
		DMap_Insert( self->mapCFlags, name, cflags );
		DMap_Insert( self->mapLFlags, name, lflags );
		DMap_Insert( self->mapShlibs, name, shlibs );
		DMap_Insert( self->mapStlibs, name, stlibs );
	}
}
static void DaoMakeProjMaps_AppendFlags( DaoMakeProjMaps *self, DString *name, DString *cflags, DString *lflags, DString *shlibs, DString *stlibs )
{
	DNode *itCFlags = DMap_Find( self->mapCFlags, name );
	DNode *itLFlags = DMap_Find( self->mapLFlags, name );
	DNode *itShlibs = DMap_Find( self->mapShlibs, name );
	DNode *itStlibs = DMap_Find( self->mapStlibs, name );

	if( cflags->size ){
		DString_AppendGap( itCFlags->value.pString );
		DString_Append( itCFlags->value.pString, cflags );
	}
	if( lflags->size ){
		DString_AppendGap( itLFlags->value.pString );
		DString_Append( itLFlags->value.pString, lflags );
	}
	if( shlibs->size ){
		DString_AppendGap( itShlibs->value.pString );
		DString_Append( itShlibs->value.pString, shlibs );
	}
	if( stlibs->size ){
		DString_AppendGap( itStlibs->value.pString );
		DString_Append( itStlibs->value.pString, stlibs );
	}
}
static void DString_AppendRepeatedChars( DString *self, char ch, int count )
{
	while( (count--) > 0 ) DString_AppendChar( self, ch );
}
static void DaoMakeProject_MakeExports( DaoMakeProject *self, DaoMakeProjMaps *promaps, DString *cflags, DString *lflags, DString *shlibs, DString *stlibs )
{
	DNode *itCFlags = DMap_First( self->exportCFlags );
	DNode *itLFlags = DMap_First( self->exportLFlags );
	DNode *itShlibs = DMap_First( self->exportShlibs );
	DNode *itStlibs = DMap_First( self->exportStlibs );
	DNode *it;
	for(; itCFlags; ){
		DaoMakeProjMaps_TryInitKey( promaps, itCFlags->key.pString, cflags, lflags, shlibs, stlibs );
		it = DMap_Find( promaps->mapCFlags, itCFlags->key.pVoid );
		if( it ){
			DString_AppendGap( it->value.pString );
			DString_Append( it->value.pString, itCFlags->value.pString );
		}
		it = DMap_Find( promaps->mapLFlags, itLFlags->key.pVoid );
		if( it ){
			DString_AppendGap( it->value.pString );
			DString_Append( it->value.pString, itLFlags->value.pString );
		}
		it = DMap_Find( promaps->mapShlibs, itShlibs->key.pVoid );
		if( it ){
			DString_AppendGap( it->value.pString );
			DString_Append( it->value.pString, itShlibs->value.pString );
		}
		it = DMap_Find( promaps->mapStlibs, itStlibs->key.pVoid );
		if( it ){
			DString_AppendGap( it->value.pString );
			DString_Append( it->value.pString, itStlibs->value.pString );
		}
		itCFlags = DMap_Next( self->exportCFlags, itCFlags );
		itLFlags = DMap_Next( self->exportLFlags, itLFlags );
		itShlibs = DMap_Next( self->exportShlibs, itShlibs );
		itStlibs = DMap_Next( self->exportStlibs, itStlibs );
	}
}
static void DaoMakeProject_WriteExports( DaoMakeProject *self, DaoMakeProjMaps *promaps, DString *output, int tab )
{
	DString *md5 = self->mbs;
	DNode *itCFlags = DMap_First( promaps->mapCFlags );
	DNode *itLFlags = DMap_First( promaps->mapLFlags );
	DNode *itShlibs = DMap_First( promaps->mapShlibs );
	DNode *itStlibs = DMap_First( promaps->mapStlibs );
	for(; itCFlags; ){
		DString_AppendRepeatedChars( output, '\t', tab );
		DString_AppendMBS( output, "cflags_" );
		DString_Append( output, itCFlags->key.pString );
		DString_AppendMBS( output, " = " );
		DString_AppendVerbatim( output, itCFlags->value.pString, md5 );
		DString_AppendMBS( output, "\n" );

		DString_AppendRepeatedChars( output, '\t', tab );
		DString_AppendMBS( output, "lflags_" );
		DString_Append( output, itLFlags->key.pString );
		DString_AppendMBS( output, " = " );
		DString_AppendVerbatim( output, itLFlags->value.pString, md5 );
		DString_AppendMBS( output, "\n" );

		DString_AppendRepeatedChars( output, '\t', tab );
		DString_AppendMBS( output, "shlibs_" );
		DString_Append( output, itShlibs->key.pString );
		DString_AppendMBS( output, " = " );
		DString_AppendVerbatim( output, itShlibs->value.pString, md5 );
		DString_AppendMBS( output, "\n" );

		DString_AppendRepeatedChars( output, '\t', tab );
		DString_AppendMBS( output, "stlibs_" );
		DString_Append( output, itStlibs->key.pString );
		DString_AppendMBS( output, " = " );
		DString_AppendVerbatim( output, itStlibs->value.pString, md5 );
		DString_AppendMBS( output, "\n" );

		DString_AppendRepeatedChars( output, '\t', tab );
		DString_AppendMBS( output, "project.ExportLibrary( \"" );
		DString_Append( output, itCFlags->key.pString );
		DString_AppendMBS( output, "\", cflags_" );
		DString_Append( output, itCFlags->key.pString );
		DString_AppendMBS( output, ", lflags_" );
		DString_Append( output, itCFlags->key.pString );
		DString_AppendMBS( output, ", shlibs_" );
		DString_Append( output, itCFlags->key.pString );
		DString_AppendMBS( output, ", stlibs_" );
		DString_Append( output, itCFlags->key.pString );
		DString_AppendMBS( output, " )\n" );

		itCFlags = DMap_Next( promaps->mapCFlags, itCFlags );
		itLFlags = DMap_Next( promaps->mapLFlags, itLFlags );
		itShlibs = DMap_Next( promaps->mapShlibs, itShlibs );
		itStlibs = DMap_Next( promaps->mapStlibs, itStlibs );
	}
}
void DaoMakeProject_MakeFindPackageForInstall( DaoMakeProject *self, DString *output, int tab )
{
	DString *empty = DaoMakeProject_GetBufferString( self );
	DString *incdir = DaoMakeProject_GetBufferString( self );
	DString *lnkdir = DaoMakeProject_GetBufferString( self );
	DString *cflags = DaoMakeProject_GetBufferString( self );
	DString *lflags = DaoMakeProject_GetBufferString( self );
	DString *shlibs = DaoMakeProject_GetBufferString( self );
	DString *stlibs = DaoMakeProject_GetBufferString( self );
	DString *proCFlags = DaoMakeProject_GetBufferString( self );
	DString *proLFlags = DaoMakeProject_GetBufferString( self );
	DaoValue **installs = self->installs->items.pValue;
	DaoMakeProjMaps *promaps = DaoMakeProjMaps_New();
	DMap *incdirs = DMap_New(D_STRING,0);
	DMap *lnkdirs = DMap_New(D_STRING,0);
	daoint i;

	DaoMakeUnit_MakeDefinitions( & self->base, proCFlags );
	DaoMakeUnit_MakeLinkingFlagsEx( & self->base, proLFlags, NULL );

	for(i=0; i<self->installs->size; i+=2){
		DString *dest = DaoValue_TryGetString( installs[i+1] );
		DString *file = DaoValue_TryGetString( installs[i] );
		DaoMakeTarget *tar = (DaoMakeTarget*) DaoValue_CastCdata( installs[i], daomake_type_target );
		if( tar == NULL ) continue;
		if( tar->ttype != DAOMAKE_SHAREDLIB && tar->ttype != DAOMAKE_STATICLIB ) continue;
		if( tar->install->size == 0 ) continue;

		DString_Reset( cflags, 0 );
		DString_Reset( lflags, 0 );
		DString_Reset( shlibs, 0 );
		DString_Reset( stlibs, 0 );
		DaoMakeTarget_MakeExport( tar, cflags, lflags, shlibs, stlibs, incdirs, lnkdirs );

		DaoMakeProjMaps_TryInitKey( promaps, tar->name, proCFlags, proLFlags, empty, empty );
		DaoMakeProjMaps_AppendFlags( promaps, tar->name, cflags, lflags, shlibs, stlibs );
	}
	DaoMakeProject_MakeExports( self, promaps, proCFlags, proLFlags, empty, empty );
	DaoMakeProject_WriteExports( self, promaps, output, tab );

	DMap_Delete( incdirs );
	DMap_Delete( lnkdirs );
	DaoMakeProjMaps_Delete( promaps );
	self->usedStrings -= 9;
}
void DaoMakeProject_MakeFindPackageForBuild( DaoMakeProject *self, DString *output, int tab )
{
	DString *empty = DaoMakeProject_GetBufferString( self );
	DString *incdir = DaoMakeProject_GetBufferString( self );
	DString *lnkdir = DaoMakeProject_GetBufferString( self );
	DString *cflags = DaoMakeProject_GetBufferString( self );
	DString *lflags = DaoMakeProject_GetBufferString( self );
	DString *shlibs = DaoMakeProject_GetBufferString( self );
	DString *stlibs = DaoMakeProject_GetBufferString( self );
	DString *proCFlags = DaoMakeProject_GetBufferString( self );
	DString *proLFlags = DaoMakeProject_GetBufferString( self );
	DaoValue **installs = self->installs->items.pValue;
	DaoMakeProjMaps *promaps = DaoMakeProjMaps_New();
	DMap *incdirs = DMap_New(D_STRING,0);
	DMap *lnkdirs = DMap_New(D_STRING,0);
	daoint i;

	DaoMakeUnit_MakeCompilingFlags( & self->base, proCFlags );
	DaoMakeUnit_MakeLinkingFlags( & self->base, proLFlags );

	for(i=0; i<self->targets->size; ++i){
		DaoMakeTarget *tar = (DaoMakeTarget*) self->targets->items.pVoid[i];
		if( tar->ttype != DAOMAKE_SHAREDLIB && tar->ttype != DAOMAKE_STATICLIB ) continue;

		DString_Reset( cflags, 0 );
		DString_Reset( lflags, 0 );
		DString_Reset( shlibs, 0 );
		DString_Reset( stlibs, 0 );
		DaoMakeTarget_MakeCompilingFlags( tar, NULL, cflags );
		DaoMakeTarget_MakeLinkingFlags( tar, NULL, lflags );
		if( tar->ttype == DAOMAKE_SHAREDLIB ){
			DString *key = empty;
			DString_Reset( key, 0 );
			DString_Append( key, tar->name );
			DString_AppendMBS( key, "::::" );
			DString_Append( key, tar->base.project->binaryPath );
			if( DMap_Find( lnkdirs, key ) == NULL ){
				DMap_Insert( lnkdirs, key, 0 );
				DString *rpath = DaoMake_GetSettingValue( "DLL-RPATH" );
				DString_Append( shlibs, rpath );
				DString_Append( shlibs, tar->base.project->binaryPath );
				DString_AppendMBS( shlibs, " -L" );
				DString_Append( shlibs, tar->base.project->binaryPath );
			}
			DString_Reset( empty, 0 );
			DString_AppendMBS( shlibs, " -l" );
			DString_Append( shlibs, tar->name );
		}else if( tar->ttype == DAOMAKE_STATICLIB ){
			DaoMakeTarget_MakeName( tar, stlibs );
			DaoMakeProject_MakeBinaryPath( self, stlibs );
		}

		DaoMakeProjMaps_TryInitKey( promaps, tar->name, proCFlags, proLFlags, empty, empty );
		DaoMakeProjMaps_AppendFlags( promaps, tar->name, cflags, lflags, shlibs, stlibs );
	}
	DaoMakeProject_MakeExports( self, promaps, proCFlags, proLFlags, empty, empty );
	DaoMakeProject_WriteExports( self, promaps, output, tab );

	for(i=0; i<self->targets->size; ++i){
		DaoMakeTarget *tar = (DaoMakeTarget*) self->targets->items.pVoid[i];
		if( tar->ttype != DAOMAKE_DIRECTORY ) continue;
	}

	DMap_Delete( incdirs );
	DMap_Delete( lnkdirs );
	DaoMakeProjMaps_Delete( promaps );
	self->usedStrings -= 9;
}
void DaoMakeProject_MakeFindPackage( DaoMakeProject *self, DString *output, int caching )
{
	DString *filePath = DaoMakeProject_GetBufferString( self );
	DString *installPath = DaoMakeProject_GetBufferString( self );
	DString *find1 = DaoMakeProject_GetBufferString( self );
	DString *find2 = DaoMakeProject_GetBufferString( self );
	DaoValue **installs = self->installs->items.pValue;
	DString *md5 = self->mbs;
	daoint i;

	DString_Append( filePath, self->sourcePath );
	DString_AppendPathSep( filePath );
	DString_AppendMBS( filePath, "Find" );
	DString_Append( filePath, self->projectName );
	DString_AppendMBS( filePath, ".dao" );

	for(i=0; i<self->installs->size; i+=2){
		DString *dest = DaoValue_TryGetString( installs[i+1] );
		DString *file = DaoValue_TryGetString( installs[i] );
		if( file == NULL ) continue;
		DString_ToMBS( file );
		if( DString_EQ( file, filePath ) ){
			DString_Append( installPath, dest );
			break;
		}
	}

	DString_Reset( output, 0 );
	DString_AppendMBS( output, "project = DaoMake::Project( \"" );
	DString_Append( output, self->projectName );
	DString_AppendMBS( output, "\" )\n" );

	if( installPath->size == 0 || caching ){
		DaoMakeProject_MakeFindPackageForBuild( self, find1, 0 );
		DString_Append( output, find1 );
		if( find1->size == 0 ) DString_Reset( output, 0 );
		self->usedStrings -= 4;
		return;
	}
	DaoMakeProject_MakeFindPackageForInstall( self, find1, 1 );
	DaoMakeProject_MakeFindPackageForBuild( self, find2, 1 );

	DString_AppendMBS( output, "if( project.SourcePath() == " );
	DString_AppendVerbatim( output, installPath, md5 );
	DString_AppendMBS( output, " ){\n" );
	DString_Append( output, find1 );
	DString_AppendMBS( output, "}else{\n" );
	DString_Append( output, find2 );
	DString_AppendMBS( output, "}" );
	self->usedStrings -= 4;
	if( find1->size + find2->size == 0 ) DString_Reset( output, 0 );
}





static void DArray_ImportStringList( DArray *self, DaoList *list )
{
	int i, size = DaoList_Size( list );
	for(i=0; i<size; ++i){
		DaoValue *value = DaoList_GetItem( list, i );
		DArray_Append( self, DaoValue_TryGetString( value ) );
	}
}


static void DaoMakeUnit_ImportPaths( DaoMakeUnit *self, DArray *paths, DaoValue *p[], int N )
{
	int i;
	for(i=0; i<N; ++i){
		DString *path = DaoValue_TryGetString( p[i] );
		if( path == NULL ) continue;
		path = (DString*) DArray_Append( paths, path );
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
	DaoMakeUnit_ImportPaths( self, self->includePaths, p+1, N-1 );
}
static void UNIT_AddLinkingPath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DaoMakeUnit_ImportPaths( self, self->linkingPaths, p+1, N-1 );
}
static void UNIT_AddAssemblingFlag( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DArray_ImportStringParameters( self->assemblingFlags, p+1, N-1 );
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
static void UNIT_AddRpath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DString *rpath = DaoMake_GetSettingValue( "DLL-RPATH" );
	int i;
	for(i=1; i<N; ++i){
		DString *flag, *path = DaoValue_TryGetString( p[i] );
		if( path == NULL ) continue;
		flag = (DString*) DArray_PushBack( self->linkingFlags, rpath );
		DString_Append( flag, path );
	}
}
static void DaoMakeUnit_UseLibrary( DaoMakeUnit *self, DaoMakeProject *pro, DString *name, int ttype, int import )
{
	DString *flags = DaoMakeProject_GetBufferString( pro );
	DString *rpath = DaoMake_GetSettingValue( "DLL-RPATH" );
	DString *flag;
	DNode *it;
	daoint i;

	if( self == (DaoMakeUnit*) pro ){
		fprintf( stderr, "\nWARNING: project %s uses library from itself!\n\n", pro->projectName->mbs );
		return;
	}

	for(i=0; i<pro->targets->size; ++i){
		DaoMakeTarget *tar = (DaoMakeTarget*) pro->targets->items.pVoid[i];
		if( tar->ttype != ttype ) continue;
		if( DString_EQ( tar->name, name ) == 0 ) continue;
		DString_Reset( flags, 0 );
		DaoMakeTarget_MakeCompilingFlagsEx( tar, pro, flags, self->project->binaryPath );
		DArray_Append( self->compilingFlags, flags );
		DString_Reset( flags, 0 );
		DaoMakeTarget_MakeLinkingFlagsEx( tar, pro, flags, self->project->binaryPath );
		DArray_Append( self->linkingFlags, flags );

		if( import && DaoMake_GetSettingValue( "WIN32" ) == NULL ) break;
		if( tar->install->size ){
			flag = (DString*) DArray_PushBack( self->linkingFlags, rpath );
			DString_Append( flag, tar->install );
		}
		if( ttype == DAOMAKE_SHAREDLIB ){
			DArray_PushBack( self->linkingPaths, tar->base.project->binaryPath );
			name = (DString*) DArray_PushBack( self->linkingFlags, name );
			DString_InsertMBS( name, "-l", 0, 0, 0 );
		}else if( ttype == DAOMAKE_STATICLIB ){
			DString *prefix = DaoMake_GetSettingValue( daomake_prefix_keys[ ttype ] );
			DString *suffix = DaoMake_GetSettingValue( daomake_suffix_keys[ ttype ] );
			name = (DString*) DArray_PushBack( self->linkingFlags, name );
			if( prefix != NULL ) DString_Insert( name, prefix, 0, 0, 0 );
			DString_Append( name, suffix );
			DaoMake_MakePath( tar->base.project->binaryPath, name );
		}
		break;
	}
	pro->usedStrings -= 1;

	it = DMap_Find( pro->exportCFlags, name );
	if( it ) DArray_Append( self->compilingFlags, it->value.pString );

	it = DMap_Find( pro->exportLFlags, name );
	if( it ) DArray_Append( self->linkingFlags, it->value.pString );

	if( ttype == DAOMAKE_STATICLIB ){
		it = DMap_Find( pro->exportStlibs, name );
		if( it && it->value.pString->size ){
			DArray_Append( self->linkingFlags, it->value.pString );
			return;
		}
	}
	it = DMap_Find( pro->exportShlibs, name );
	if( it && it->value.pString->size ){
		DArray_Append( self->linkingFlags, it->value.pString );
	}
}
static void DaoMakeUnit_UseAllLibraries( DaoMakeUnit *self, DaoMakeProject *pro, int ttype, int import )
{
	DMap *names = DMap_New(D_STRING,0);
	DNode *it;
	daoint i;

	for(i=0; i<pro->targets->size; ++i){
		DaoMakeTarget *tar = (DaoMakeTarget*) pro->targets->items.pVoid[i];
		if( tar->ttype != ttype ) continue;
		DMap_Insert( names, tar->name, 0 );
	}
	for(it=DMap_First(pro->exportCFlags); it; it=DMap_Next(pro->exportCFlags,it)){
		DMap_Insert( names, it->key.pString, 0 );
	}
	for(it=DMap_First(names); it; it=DMap_Next(names,it)){
		DaoMakeUnit_UseLibrary( self, pro, it->key.pString, ttype, import );
	}
	DMap_Delete( names );
}
static void UNIT_UseImportLib( DaoProcess *proc, DaoValue *p[], int N )
{
	int i;
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DaoMakeProject *pro = (DaoMakeProject*) p[1];

	for(i=2; i<N; ++i){
		DString *name = DaoValue_TryGetString( p[i] );
		if( name == NULL ) continue;
		DaoMakeUnit_UseLibrary( self, pro, name, DAOMAKE_SHAREDLIB, 1 );
	}
	if( N == 2 ) DaoMakeUnit_UseAllLibraries( self, pro, DAOMAKE_SHAREDLIB, 1 );
}
static void UNIT_UseSharedLib( DaoProcess *proc, DaoValue *p[], int N )
{
	int i;
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DaoMakeProject *pro = (DaoMakeProject*) p[1];

	for(i=2; i<N; ++i){
		DString *name = DaoValue_TryGetString( p[i] );
		if( name == NULL ) continue;
		DaoMakeUnit_UseLibrary( self, pro, name, DAOMAKE_SHAREDLIB, 0 );
	}
	if( N == 2 ) DaoMakeUnit_UseAllLibraries( self, pro, DAOMAKE_SHAREDLIB, 0 );
}
static void UNIT_UseStaticLib( DaoProcess *proc, DaoValue *p[], int N )
{
	int i;
	DaoMakeUnit *self = (DaoMakeUnit*) p[0];
	DaoMakeProject *pro = (DaoMakeProject*) p[1];
	for(i=2; i<N; ++i){
		DString *name = DaoValue_TryGetString( p[i] );
		if( name == NULL ) continue;
		DaoMakeUnit_UseLibrary( self, pro, name, DAOMAKE_STATICLIB, 0 );
	}
	if( N == 2 ) DaoMakeUnit_UseAllLibraries( self, pro, DAOMAKE_STATICLIB, 0 );
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
	{ UNIT_AddAssemblingFlag,  "AddAssemblingFlag( self : Unit, flag : string, ... : string )" },
	{ UNIT_AddCompilingFlag,  "AddCompilingFlag( self : Unit, flag : string, ... : string )" },
	{ UNIT_AddLinkingFlag,    "AddLinkingFlag( self : Unit, flag : string, ... : string )" },
	{ UNIT_AddRpath,          "AddRpath( self : Unit, flag : string, ... : string )" },

	{ UNIT_UseImportLib,  "UseImportLibrary( self : Unit, pro : Project, ... : string )" },
	{ UNIT_UseSharedLib,  "UseSharedLibrary( self : Unit, pro : Project, ... : string )" },
	{ UNIT_UseStaticLib,  "UseStaticLibrary( self : Unit, pro : Project, ... : string )" },

	{ UNIT_MakeDefinitions,     "MakeDefinitions( self : Unit ) => string" },
	{ UNIT_MakeIncludePaths,    "MakeIncludePaths( self : Unit ) => string" },
	{ UNIT_MakeLinkingPaths,    "MakeLinkingPaths( self : Unit ) => string" },
	{ UNIT_MakeCompilingFlags,  "MakeCompilingFlags( self : Unit ) => string" },
	{ UNIT_MakeLinkingFlags,    "MakeLinkingFlags( self : Unit ) => string" },
	{ NULL, NULL }
};
DaoTypeBase DaoMakeUnit_Typer =
{
	"Unit", NULL, NULL, (DaoFuncItem*) DaoMakeUnitMeths, {0}, {0}, NULL, NULL
};




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




static void TARGET_Name( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeTarget *self = (DaoMakeTarget*) p[0];
	DString *res = DaoProcess_PutMBString( proc, "" );
	DaoMakeTarget_MakeName( self, res );
}
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
	self->dynamicExporting = p[1]->xEnum.value;
}
static void TARGET_EnableDynamicLinking( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeTarget *self = (DaoMakeTarget*) p[0];
	self->dynamicLinking = p[1]->xEnum.value;
}
static void TARGET_SetPath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeTarget *self = (DaoMakeTarget*) p[0];
	DString *dest = p[1]->xString.data;
	DString_Assign( self->path, dest );
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
	{ TARGET_Name,  "Name( self : Target ) => string" },
	{ TARGET_AddObjects,  "AddObjects( self : Target, objects : Objects, ... : Objects )" },
	{ TARGET_AddCommand,  "AddCommand( self : Target, command : string, ... : string )" },
	{ TARGET_AddTest,     "AddTest( self : Target, test : string, ... : string )" },
	{ TARGET_AddDepends,  "AddDependency( self : Target, target : Target, ... : Target )" },
	{ TARGET_EnableDynamicExporting,  "EnableDynamicExporting( self : Target, bl :enum<FALSE,TRUE> = $TRUE )" },
	{ TARGET_EnableDynamicLinking,    "EnableDynamicLinking( self : Target, bl :enum<FALSE,TRUE> = $TRUE )" },
	{ TARGET_SetPath,  "SetTargetPath( self : Target, path : string )" },
	{ TARGET_Install,  "Install( self : Target, dest : string )" },
	{ NULL, NULL }
};
static void TARGET_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int rm )
{
	DaoMakeTarget *self = (DaoMakeTarget*) p;
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
	DString *binRoot = vmSpace->startPath;
	DString *srcRoot = daomake_main_source_path;
	DaoNamespace *ns = proc->activeNamespace;
	DaoMakeProject *self = DaoMakeProject_New();

	self->base.project = self;
	DaoProcess_PutValue( proc, (DaoValue*) self );
	DString_Assign( self->sourceName, ns->file );
	DString_Assign( self->sourcePath, ns->path );
	DString_Assign( self->binaryPath, ns->path );
	DString_Assign( self->projectName, name );
	if( daomake_out_of_source ){
		DString_Insert( self->binaryPath, binRoot, 0, srcRoot->size, binRoot->size );
	}
	if( name->mbs ){
		DaoMap_InsertMBS( daomake_projects, name->mbs, (DaoValue*) self );
	}else{
		DaoMap_InsertWCS( daomake_projects, name->wcs, (DaoValue*) self );
	}
}
static void PROJECT_AddOBJ( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DaoMakeObjects *objs = DaoMakeObjects_New();

	objs->base.project = self;
	DArray_ImportStringList( objs->sources, (DaoList*) p[1] );
	DArray_ImportStringList( objs->headers, (DaoList*) p[2] );
	DaoProcess_PutValue( proc, (DaoValue*) objs );
}
static void PROJECT_AddTarget( DaoProcess *proc, DaoValue *p[], int N, int ttype )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DaoMakeTarget *target = DaoMakeTarget_New();
	int i;
	target->ttype = ttype;
	target->base.project = self;
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
static void PROJECT_AddJS( DaoProcess *proc, DaoValue *p[], int N )
{
	PROJECT_AddTarget( proc, p, N, DAOMAKE_JAVASCRIPT );
}
static void PROJECT_AddTest( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DaoMakeTarget *target = DaoMakeTarget_New();
	int i;
	daomake_test_count += (N-2);
	target->ttype = DAOMAKE_TESTING;
	target->base.project = self;
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
	target->base.project = self;
	DString_Assign( target->name, name );
	i = DString_FindChar( target->name, ':', 0 );
	if( i >= 0 ){
		DString_Erase( target->name, i, -1 );
		DString_SubString( name, target->extradeps, i+1, -1 );
	}
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
	DaoNamespace *ns;
	int i;
	target->ttype = DAOMAKE_DIRECTORY;
	target->base.project = self;
	DString_Assign( target->name, name );
	for(i=2; i<N; ++i){
		DString *path = DaoValue_TryGetString( p[i] );
		DString_Append( file, path );
		DString_AppendPathSep( file );
		DString_AppendMBS( file, "makefile.dao" );
		ns = DaoVmSpace_Load( proc->vmSpace, file->mbs );
		if( ns == NULL ) continue;
		DArray_Append( target->commands, path );

		DString_SetMBS( file, daomake_objects_dir );
		DaoMake_MakePath( ns->path, file );
		if( daomake_out_of_source ){
			DaoMake_MakeOutOfSourcePath( file, 0 );
		}else{
			DaoMake_MakeDir( file->mbs );
		}
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
static void PROJECT_SourcePath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DaoProcess_PutString( proc, self->sourcePath );
}
static void PROJECT_BinaryPath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DString *path = DaoProcess_PutString( proc, self->sourcePath );
	DString *binpath = vmSpace->startPath;
	if( daomake_out_of_source == 0 ) return;
	DString_Insert( path, binpath, 0, daomake_main_source_path->size, binpath->size );
}
static void PROJECT_ExportLibrary( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DString *lib = DaoValue_TryGetString( p[1] );
	DString *cflag = DaoValue_TryGetString( p[2] );
	DString *lflag = DaoValue_TryGetString( p[3] );
	DString *libs = DaoValue_TryGetString( p[4] );
	DString *file = DaoValue_TryGetString( p[5] );
	DMap_Insert( self->exportCFlags, lib, cflag );
	DMap_Insert( self->exportLFlags, lib, lflag );
	DMap_Insert( self->exportShlibs, lib, libs );
	DMap_Insert( self->exportStlibs, lib, file );
}
static void PROJECT_ExportPath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DString *name = DaoValue_TryGetString( p[1] );
	DString *path = DaoValue_TryGetString( p[2] );
	DaoMakeProject_MakeBinaryPath( self, path );
	DMap_Insert( self->exportPaths, name, path );
}
static void PROJECT_GetPath( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoMakeProject *self = (DaoMakeProject*) p[0];
	DString *name = DaoValue_TryGetString( p[1] );
	DString *res = DaoProcess_PutMBString( proc, "" );
	DNode *it = DMap_Find( self->exportPaths, name );
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

	{ PROJECT_AddOBJ,  "AddObjects( self : Project, sources : list<string>, headers : list<string> = {} ) => Objects" },
	{ PROJECT_AddEXE,  "AddExecutable( self : Project, name : string, objs : Objects, ... : Objects ) =>Target" },
	{ PROJECT_AddDLL,  "AddSharedLibrary( self : Project, name : string, objs : Objects, ... : Objects ) =>Target" },
	{ PROJECT_AddARC,  "AddStaticLibrary( self : Project, name : string, objs : Objects, ... : Objects ) =>Target" },
	{ PROJECT_AddJS,   "AddJavaScriptLibrary( self : Project, name : string, objs : Objects, ... : Objects ) =>Target" },
	{ PROJECT_AddTest, "AddTest( self : Project, group : string, test : string, ... : string ) => Target" },
	{ PROJECT_AddCMD,  "AddCommand( self : Project, name : string, command : string, ... : string ) => Target" },

	{ PROJECT_AddDIR,  "AddDirectory( self : Project, name : string, path : string, ... : string ) => Target" },

	{ PROJECT_AddVAR,   "AddVariable( self : Project, name : string, value : string )" },

	{ PROJECT_InstallTarget,  "Install( self : Project, dest : string, target : Target, ... : Target )" },
	{ PROJECT_InstallFile,    "Install( self : Project, dest : string, file : string, ... : string )" },
	{ PROJECT_InstallFiles,   "Install( self : Project, dest : string, headers : list<string> )" },

	{ PROJECT_SourcePath,  "SourcePath( self : Project ) => string" },
	{ PROJECT_BinaryPath,  "BinaryPath( self : Project ) => string" },

	{ PROJECT_ExportLibrary,  "ExportLibrary( self : Project, name : string, cflag : string, lflag : string, shlib = '', stlib = '' )" },

	{ PROJECT_ExportPath,    "ExportPath( self : Project, name : string, path : string )" },
	{ PROJECT_GetPath,       "GetPath( self : Project, name : string ) => string" },
	{ PROJECT_GenerateFinder,   "GenerateFinder( self : Project, bl : enum<FALSE,TRUE> = $TRUE )" },
	{ NULL, NULL }
};
static void PROJ_GetGCFields( void *p, DArray *values, DArray *arrays, DArray *maps, int rm )
{
	DaoMakeProject *self = (DaoMakeProject*) p;
	DArray_Append( arrays, self->targets );
	DArray_Append( arrays, self->installs );
}
DaoTypeBase DaoMakeProject_Typer =
{
	"Project", NULL, NULL, (DaoFuncItem*) DaoMakeProjectMeths,
	{ & DaoMakeUnit_Typer, NULL }, {0},
	(FuncPtrDel)DaoMakeProject_Delete,  PROJ_GetGCFields
};




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

	DString_AppendPathSep( cache );
	DString_AppendMBS( cache, "CacheFind" );
	DString_Append( cache, name);
	DString_AppendMBS( cache, ".dao" );
	DaoMake_MakeOutOfSourcePath( cache, 1 );

	if( DaoVmSpace_CompleteModuleName( vmSpace, cache, DAO_MODULE_DAO ) == DAO_MODULE_NONE ) reset = 1;
	DaoVmSpace_CompleteModuleName( vmSpace, original, DAO_MODULE_DAO );
	otime = Dao_FileChangedTime( original->mbs );
	ctime = Dao_FileChangedTime( cache->mbs );
	if( otime > ctime ) reset = 1;

	if( project == NULL && reset == 0 ){
		if( DaoVmSpace_Load( vmSpace, cache->mbs ) ){
			project = DaoMap_GetValue( daomake_projects, p[0] );
			if( project == NULL ){
				if( p[1]->xEnum.value ){
					DaoProcess_RaiseException( proc, DAO_ERROR, message->mbs );
				}
				project = dao_none_value;
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
		project = dao_none_value;
	}else{
		DaoMakeProject *proj = (DaoMakeProject*) project;
		DaoMakeProject_MakeFindPackage( proj, cache, 1 );
		if( fout ) fprintf( fout, "%s", cache->mbs );
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
static void DAOMAKE_TestCompile( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *md5, *command, *source, *output;
	DString *code = DaoValue_TryGetString( p[0] );
	DString *lflag = DaoValue_TryGetString( p[1] );
	DString *cflag = DaoValue_TryGetString( p[2] );
	daoint pos1, pos2, pos3, cxx = DaoValue_TryGetInteger( p[3] );
	daoint *res = DaoProcess_PutInteger( proc, 0 );
	FILE *file;

	DString_Trim( cflag );
	DString_Trim( lflag );
	DString_Trim( code );
	if( code->size == 0 ) return;

	md5 = DString_New(1);
	command = DString_New(1);
	source = DString_New(1);
	output = DString_New(1);
	DString_AppendMBS( command, cxx ? "c++ " : "cc " );
	if( cflag->size ){
		DString_Append( command, cflag );
		DString_AppendMBS( command, " " );
	}
	pos1 = command->size;
	DString_AppendMBS( command, "-o " );
	pos2 = command->size;
	if( lflag->size ){
		DString_Append( command, lflag );
		DString_AppendMBS( command, " " );
	}
	pos3 = command->size;

	DString_Append( command, code );
	DString_MD5( command, md5 );
	DString_Reset( md5, 12 );

	DString_AppendMBS( source, daomake_objects_dir );
	DString_AppendMBS( source, "/source_" );
	DString_Append( source, md5 );
	DString_AppendMBS( source, cxx ? ".cxx" : ".c" );

	DString_AppendMBS( output, daomake_objects_dir );
	DString_AppendMBS( output, "/binary_" );
	DString_Append( output, md5 );

	DaoMake_MakePath( daomake_main_source_path, source );
	DaoMake_MakePath( daomake_main_source_path, output );
	if( daomake_out_of_source ){
		DaoMake_MakeOutOfSourcePath( source, 1 );
		DaoMake_MakeOutOfSourcePath( output, 1 );
	}

	DString_Delete( md5 );
	if( DaoMake_IsFile( output->mbs ) ){
		DString_Delete( command );
		DString_Delete( source );
		DString_Delete( output );
		*res = 1;
		return;
	}

	DString_Erase( command, pos3, -1 );
	DString_InsertMBS( command, " ", pos2, 0, -1 );
	DString_Insert( command, output, pos2, 0, 0 );
	DString_InsertMBS( command, " ", pos1, 0, -1 );
	DString_Insert( command, source, pos1, 0, -1 );

	printf( "Checking: %s\n", command->mbs );
	file = fopen( source->mbs, "w+" );
	DaoFile_WriteString( file, code );
	fclose( file );

	DString_Assign( source, output );
	DString_SetMBS( output, daomake_objects_dir );
	DString_AppendMBS( output, "/null" );
	DString_AppendMBS( command, " &> " );
	DString_Append( command, output );

	system( command->mbs );
	*res = DaoMake_IsFile( source->mbs );
	DString_Delete( command );
	DString_Delete( source );
	DString_Delete( output );
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
static void DAOMAKE_Suffix( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoNamespace *ns = proc->activeNamespace;
	DaoProcess_PutMBString( proc, daomake_makefile_suffix );
}
static void DAOMAKE_IsFile( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *path = DaoValue_TryGetString( p[0] );
	DString_ToMBS( path );
	DaoMake_MakePath( proc->activeNamespace->path, path );
	DaoProcess_PutInteger( proc, DaoMake_IsFile( path->mbs ) );
}
static void DAOMAKE_IsDir( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *path = DaoValue_TryGetString( p[0] );
	DString_ToMBS( path );
	DaoMake_MakePath( proc->activeNamespace->path, path );
	DaoProcess_PutInteger( proc, DaoMake_IsDir( path->mbs ) );
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
static void DAOMAKE_MakePath( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *base = DaoValue_TryGetString( p[0] );
	DString *sub = DaoValue_TryGetString( p[1] );
	DString *res = DaoProcess_PutString( proc, sub );
	DaoMake_MakePath( base, res );
}
static void DAOMAKE_MakeRpath( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *flag = DaoProcess_PutMBString( proc, "" );
	DString *rpath = DaoMake_GetSettingValue( "DLL-RPATH" );
	int i;
	for(i=0; i<N; ++i){
		DString *path = DaoValue_TryGetString( p[i] );
		if( path == NULL ) continue;
		DString_AppendGap( flag );
		DString_Append( flag, rpath );
		DString_Append( flag, path );
	}
}
static void DAOMAKE_BuildMode( DaoProcess *proc, DaoValue *p[], int N )
{
	static const char *const build_modes[] = { "RELEASE", "DEBUG", "PROFILE" };
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
static void DAOMAKE_Platform( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutString( proc, daomake_platform );
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
static void DAOMAKE_IsBeOS( DaoProcess *proc, DaoValue *p[], int N )
{
	DAOMAKE_IsPlatform( proc, "BEOS" );
}
static void DAOMAKE_IsHaiku( DaoProcess *proc, DaoValue *p[], int N )
{
	DAOMAKE_IsPlatform( proc, "HAIKU" );
}

static void DAOMAKE_IsWin32( DaoProcess *proc, DaoValue *p[], int N )
{
	DAOMAKE_IsPlatform( proc, "WIN32" );
}
static void DAOMAKE_IsMinGW( DaoProcess *proc, DaoValue *p[], int N )
{
	DAOMAKE_IsPlatform( proc, "MINGW" );
}
static void DAOMAKE_Is64Bit( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutInteger( proc, sizeof(void*) == 8 );
}

static DaoFuncItem DaoMakeMeths[] =
{
	{ DAOMAKE_FindPackage, "FindPackage( name : string, opt :enum<OPTIONAL,REQUIRED> = $OPTIONAL ) => Project|none" },
	{ DAOMAKE_FindFile,    "FindFile( file : string, hints : list<string> = {} ) => string" },
	{ DAOMAKE_TestCompile, "TestCompile( code :string, lflag='', cflag='', cxx=0 ) => int" },

	{ DAOMAKE_OptionBOOL,  "Option( name : string, value : enum<OFF,ON> ) => enum<OFF,ON>" },
	{ DAOMAKE_OptionSTR,   "Option( name : string, value = '' ) => string" },

	{ DAOMAKE_Suffix,      "MakefileSuffix() => string" },

	{ DAOMAKE_Shell,       "Shell( command : string ) => string" },

	{ DAOMAKE_SourcePath,  "SourcePath() => string" },
	{ DAOMAKE_BinaryPath,  "BinaryPath() => string" },

	{ DAOMAKE_MakePath,    "MakePath( base : string, sub : string ) => string" },

	{ DAOMAKE_MakeRpath,   "MakeRpath( path : string, ... : string ) => string" },

	{ DAOMAKE_BuildMode,   "BuildMode() => enum<RELEASE,DEBUG,PROFILE>" },

	{ DAOMAKE_SetTestTool, "SetTestTool( test : string, log_option = '--log' )" },

	{ DAOMAKE_IsFile,      "IsFile( path : string ) => int" },
	{ DAOMAKE_IsDir,       "IsDir( path : string ) => int" },

	{ DAOMAKE_Platform,    "Platform() => string" },
	{ DAOMAKE_IsUnix,      "IsUnix() => int" },
	{ DAOMAKE_IsLinux,     "IsLinux() => int" },
	{ DAOMAKE_IsMacOSX,    "IsMacOSX() => int" },
	{ DAOMAKE_IsBSD,       "IsBSD() => int" },
	{ DAOMAKE_IsFreeBSD,   "IsFreeBSD() => int" },
	{ DAOMAKE_IsMinix,     "IsMinix() => int" },
	{ DAOMAKE_IsBeOS,      "IsBeOS() => int" },
	{ DAOMAKE_IsHaiku,     "IsHaiku() => int" },

	{ DAOMAKE_IsWin32,     "IsWin32() => int" },
	{ DAOMAKE_IsMinGW,     "IsMinGW() => int" },

	{ DAOMAKE_Is64Bit,     "Is64Bit() => int" },
	{ NULL, NULL }
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



int DaoMake_RemoveFile( const char *path )
{
	return remove( path );
}
int DaoMake_RemoveDirectory( const char *path );
int DaoMake_RemovePath( const char *from )
{
	if( DaoMake_IsFile( from ) ){
		return DaoMake_RemoveFile( from );
	}else if( DaoMake_IsDir( from ) ){
		return DaoMake_RemoveDirectory( from );
	}
	return 1;
}
int DaoMake_RemoveDirectory( const char *path )
{
	DString *src;
	char *dirname;
	int rc = 0;
#ifdef WIN32
	intptr_t handle;
	struct _finddata_t finfo;
#else
	DIR *handle;
	struct dirent *finfo;
#endif

	if( DaoMake_IsFile( path ) ) return 1;

	src = DString_New(1);

#ifdef WIN32
	DString_AppendMBS( src, path );
	if( src->size && src->mbs[src->size-1] == '/' || src->mbs[src->size-1] == '\\' ){
		DString_AppendMBS( src, "*" );
	}else{
		DString_AppendMBS( src, "/*" );
	}
	handle = _findfirst( src->mbs, & finfo );
	if( handle != -1 ){
		do {
			if( strcmp( finfo.name, "." ) && strcmp( finfo.name, ".." ) ){
				DString_Reset( src, 0 );
				DString_AppendMBS( src, path );
				DString_AppendPathSep( src );
				DString_AppendMBS( src, finfo.name );
				DaoMake_RemovePath( src->mbs );
			}
		} while( !_findnext( handle, &finfo ) );
		_findclose( handle );
	}else rc = errno;
#else
	/* Using POSIX opendir/readdir otherwise */
	handle = opendir( path );
	if( handle ){
		while( ( finfo = readdir( handle ) ) ){
			if( strcmp( finfo->d_name, "." ) && strcmp( finfo->d_name, ".." ) ){
				DString_Reset( src, 0 );
				DString_AppendMBS( src, path );
				DString_AppendPathSep( src );
				DString_AppendMBS( src, finfo->d_name );
				DaoMake_RemovePath( src->mbs );
			}
		}
		closedir( handle );
	}else rc = errno;
#endif
	DString_Delete( src );
	rc |= rmdir( path );
	return rc;
}
int DaoMake_Remove( int argc, char *argv[] )
{
	int i, rec = 0;
	for(i=0; i<argc; i++){
		char *path = argv[i];
		if( DaoMake_IsFile( path ) ){
			rec |= DaoMake_RemoveFile( path );
		}else if( DaoMake_IsDir( path ) ){
			rec |= DaoMake_RemoveDirectory( path );
		}
	}
	return rec;
}
int DaoMake_CopyFile( const char *from, const char *to, int update )
{
	FILE *fin, *fout;
	struct stat info;
	DString *dest;

	if( DaoMake_IsFile( from ) == 0 ) return 1;

	dest = DString_New(1);
	DString_SetMBS( dest, to );
	if( DaoMake_IsDir( dest->mbs ) ){
		char *sep = strrchr( from, '/' );
		DString_AppendPathSep( dest );
		if( sep == NULL ){
			DString_AppendMBS( dest, from );
		}else{
			DString_AppendMBS( dest, sep + 1 );
		}
	}
	if( update && Dao_FileChangedTime( from ) <= Dao_FileChangedTime( dest->mbs ) ){
		DString_Delete( dest );
		return 0;
	}
	fin = fopen( from, "rb" );
	fout = fopen( dest->mbs, "w+b" );
	if( fin == NULL || fout == NULL ){
		if( fin ) fclose( fin );
		if( fout ) fclose( fout );
		DString_Delete( dest );
		return 1;
	}
	if( stat( from, & info ) == 0 ) chmod( dest->mbs, info.st_mode );
	DaoFile_ReadAll( fin, dest, 1 );
	DaoFile_WriteString( fout, dest );
	fclose( fout );
	DString_Delete( dest );
	return 0;
}
int DaoMake_CopyDirectory( const char *from, const char *to, int update );
int DaoMake_CopyPathFile( const char *from, const char *to, int update )
{
	if( DaoMake_IsDir( from ) )  return DaoMake_CopyDirectory( from, to, update );
	if( DaoMake_IsFile( from ) ) return DaoMake_CopyFile( from, to, update );
	return 1;
}
int DaoMake_CopyDirectory( const char *from, const char *to, int update )
{
	DString *src, *dest;
	char *dirname;
	int i, rc = 0;
#ifdef WIN32
	intptr_t handle;
	struct _finddata_t finfo;
#else
	DIR *handle;
	struct dirent *finfo;
#endif

	if( DaoMake_IsFile( from ) || DaoMake_IsFile( to ) ) return 1;

	src = DString_New(1);
	dest = DString_New(1);
	DString_SetMBS( src, from );
	DString_SetMBS( dest, to );
	if( DaoMake_IsFile( dest->mbs ) ){
		DString_Delete( src );
		DString_Delete( dest );
		return 1;
	}
	DString_AppendPathSep( dest );
	if( DaoMake_IsDir( dest->mbs ) == 0 ){
		/* Copy to a new folder: */
		DaoMake_MakeDir( dest->mbs );
	}else{
		/* Copy under an existing folder: */
		int lastsep = src->size && src->mbs[src->size-1] == '/';
		int pos = DString_RFindChar( src, '/', src->size - 1 - lastsep );
		if( pos > 0 ){
			DString_AppendMBS( dest, src->mbs + pos + 1 );
		}else{
			DString_AppendMBS( dest, from );
		}
		if( DaoMake_IsDir( dest->mbs ) == 0 ) DaoMake_MakeDir( dest->mbs );
	}

#ifdef WIN32
	DString_AppendPathSep( src );
	DString_AppendMBS( src, "*" );
	handle = _findfirst( src->mbs, & finfo );
	if( handle != -1 ){
		do {
			if( strcmp( finfo.name, "." ) && strcmp( finfo.name, ".." ) ){
				DString_Reset( src, 0 );
				DString_AppendMBS( src, from );
				DString_AppendPathSep( src );
				DString_AppendMBS( src, finfo.name );
				DaoMake_CopyPathFile( src->mbs, dest->mbs, update );
			}
		} while( !_findnext( handle, &finfo ) );
		_findclose( handle );
	}else rc = errno;
#else
	/* Using POSIX opendir/readdir otherwise */
	handle = opendir( from );
	if( handle ){
		while( ( finfo = readdir( handle ) ) ){
			if( strcmp( finfo->d_name, "." ) && strcmp( finfo->d_name, ".." ) ){
				DString_Reset( src, 0 );
				DString_AppendMBS( src, from );
				DString_AppendPathSep( src );
				DString_AppendMBS( src, finfo->d_name );
				DaoMake_CopyPathFile( src->mbs, dest->mbs, update );
			}
		}
		closedir( handle );
	}else rc = errno;
#endif
	DString_Delete( src );
	DString_Delete( dest );
	return rc;
}
int DaoMake_Copy( int argc, char *argv[] )
{
	int i, update = 0;
	if( argc == 0 ) return 1;
	if( strcmp( argv[0], "-u" ) == 0 ){
		update = 1;
		argc -= 1;
		argv += 1;
	}
	if( argc < 2 ) return 1;
	if( argc > 2 && DaoMake_IsFile( argv[argc-1] ) ) return 1;
	for(i=0; (i+1)<argc; ++i){
		if( DaoMake_CopyPathFile( argv[i], argv[argc-1], update ) ) return 1;
	}
	return 0;
}




static const char *const daomake_doc_options =
"DaoMake Options: \n\
    --platform          platform name for which to generate makefiles;\n\
    --mode              building mode (release, debug or profile);\n\
    --suffix            makefile suffix (default none);\n\
    --reset             reset package searching caches;\n\
    --help              print this help information;\n\
    --option-OPT=value  create an option entry;\n\
    --define-DEF=value  pass a definition to the compiler;\n\
";

const char *const daomake_error_makefile_existing =
"Error: existing Makefile was not generated by DaoMake:\n  %s\n"
"Please use a (different) Makefile extension with the \"--suffix\" option.\n\n";

static const char *const daomake_lang_assemblers[] =
{
	".s" ,    "CC" , /* gcc supports command line arguments such as -I -c etc.; */
	".S" ,    "CC" ,
	NULL ,    NULL
};

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
	".s" ,    "FC;CC;CXX" ,
	".S" ,    "FC;CC;CXX" ,
	NULL ,    NULL
};


int main( int argc, char *argv[] )
{
	int i, k, m;
	char *platform = DAOMAKE_PLATFORM;
	char *mode = NULL;
	FILE *fin, *fout;
	DaoNamespace *nspace;
	DString *makefile = DString_New(1);
	DString *srcdir = DString_New(1);
	DString *source;
	DString *name;
	DNode *it;

	vmSpace = DaoInit( argv[0] );
	daomake_current_path = DString_New(1);
	DString_Reset( daomake_current_path, 1024 );
	getcwd( daomake_current_path->mbs, 1024 );
	DString_Reset( daomake_current_path, strlen( daomake_current_path->mbs ) );

	/* Utility subcommands: */
	if( argc > 1 ){
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
			if( argc == 2 ) return 1;
			return DaoMake_IsFile( argv[2] ) == 0;
		}else if( strcmp( argv[1], "isdir" ) == 0 ){
			if( argc == 2 ) return 1;
			return DaoMake_IsDir( argv[2] ) == 0;
		}else if( strcmp( argv[1], "mkdir" ) == 0 ){
			return DaoMake_MakeDir( argv[2] );
		}else if( strcmp( argv[1], "mkdir2" ) == 0 ){
			if( DaoMake_IsFile( argv[2] ) ) return 1;
			if( DaoMake_IsDir( argv[2] ) ) return 0;
			return DaoMake_MakeDir( argv[2] );
		}else if( strcmp( argv[1], "remove" ) == 0 ){
			return DaoMake_Remove( argc - 2, argv + 2 );
		}else if( strcmp( argv[1], "copy" ) == 0 ){
			return DaoMake_Copy( argc - 2, argv + 2 );
		}else if( strcmp( argv[1], "eval" ) == 0 ){
			DaoRoutine *rout;
			DaoNamespace *ns = DaoVmSpace_MainNamespace( vmSpace );
			DaoProcess *vmp = DaoVmSpace_MainProcess( vmSpace );
			if( argc <= 2 ) return 1;
			DArray_PushFront( vmSpace->nameLoading, vmSpace->pathWorking );
			DArray_PushFront( vmSpace->pathLoading, vmSpace->pathWorking );
			DString_SetMBS( vmSpace->mainNamespace->name, "command line codes" );
			if( DaoProcess_Compile( vmp, ns, argv[2] ) ==0 ) return 0;
			rout = ns->mainRoutines->items.pRoutine[ ns->mainRoutines->size-1 ];
			return DaoProcess_Call( vmp, rout, NULL, NULL, 0 );
		}
	}

	daomake_makefile_paths = DMap_New(D_STRING,0);
	daomake_boolean_options = DMap_New(D_STRING,0);
	daomake_string_options = DMap_New(D_STRING,D_STRING);
	daomake_cmdline_defines = DMap_New(D_STRING,0);
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
		}else if( strstr( arg, "--define-" ) == arg ){
			DString key = DString_WrapMBS( arg + 9 );
			DString value;
			daoint bl = -1;
			if( (i + 1) == argc ) goto ErrorMissingArgValue;
			value = DString_WrapMBS( argv[++i] );
			DMap_Insert( daomake_cmdline_defines, & key, & value );
		}else if( strcmp( arg, "--suffix" ) == 0 ){
			if( (i + 1) == argc ) goto ErrorMissingArgValue;
			daomake_makefile_suffix = argv[++i];
		}else if( strcmp( arg, "--reset" ) == 0 ){
			daomake_reset_cache = 1;
		}else if( strcmp( arg, "--help" ) == 0 ){
			printf( "%s\n", daomake_doc_options );
			if( i == 1 && argc == 2 ) return 0;
		}else if( arg[0] == '-' ){
			fprintf( stderr, "Error: unknown argument \"%s\"!\n", arg );
			return 1;
		}else if( (i + 1) == argc ){
			DString_SetMBS( makefile, argv[i] );
			if( DaoMake_IsDir( makefile->mbs ) ){
				DString *file = DString_New(1);
				const char *names[] = { "makefile", "Makefile", "make", "Make" };
				DString_AppendPathSep( makefile );
				for(k=0; k<4; ++k){
					DString_Reset( file, 0 );
					DString_Append( file, makefile );
					DString_AppendMBS( file, names[k] );
					DString_AppendMBS( file, ".dao" );
					if( DaoMake_IsFile( file->mbs ) ){
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
	daomake_platform = DString_New(1);
	daomake_projects = DaoMap_New(0);
	daomake_settings = DaoMap_New(0);
	daomake_assemblers = DaoMap_New(0);
	daomake_compilers = DaoMap_New(0);
	daomake_linkers = DaoMap_New(0);
	daomake_includes = DaoList_New();
	DaoGC_IncRC( (DaoValue*) daomake_projects );
	DaoGC_IncRC( (DaoValue*) daomake_settings );
	DaoGC_IncRC( (DaoValue*) daomake_assemblers );
	DaoGC_IncRC( (DaoValue*) daomake_compilers );
	DaoGC_IncRC( (DaoValue*) daomake_linkers );
	DaoGC_IncRC( (DaoValue*) daomake_includes );

	nspace = DaoVmSpace_GetNamespace( vmSpace, "DaoMake" );
	DaoNamespace_AddConst( vmSpace->nsInternal, nspace->name, (DaoValue*) nspace, DAO_DATA_PUBLIC );
	DaoNamespace_AddConst( vmSpace->mainNamespace, nspace->name, (DaoValue*) nspace, DAO_DATA_PUBLIC );
	daomake_type_unit    = DaoNamespace_WrapType( nspace, & DaoMakeUnit_Typer, 0 );
	daomake_type_objects = DaoNamespace_WrapType( nspace, & DaoMakeObjects_Typer, 0 );
	daomake_type_target  = DaoNamespace_WrapType( nspace, & DaoMakeTarget_Typer, 0 );
	daomake_type_project = DaoNamespace_WrapType( nspace, & DaoMakeProject_Typer, 0 );
	DaoNamespace_WrapFunctions( nspace, DaoMakeMeths );
	DaoNamespace_AddValue( nspace, "Settings", (DaoValue*) daomake_settings, "map<string,string>" );
	DaoNamespace_AddValue( nspace, "Assemblers", (DaoValue*) daomake_assemblers, "map<string,string>" );
	DaoNamespace_AddValue( nspace, "Compilers", (DaoValue*) daomake_compilers, "map<string,string>" );
	DaoNamespace_AddValue( nspace, "Linkers", (DaoValue*) daomake_linkers, "map<string,string>" );
	DaoNamespace_AddValue( nspace, "Includes", (DaoValue*) daomake_includes, "list<string>" );

	DaoMap_AddKeyValues( daomake_assemblers, daomake_lang_assemblers );
	DaoMap_AddKeyValues( daomake_compilers, daomake_lang_compilers );
	DaoMap_AddKeyValues( daomake_linkers, daomake_lang_linkers );

	name = DString_New(1);
	DaoVmSpace_AddPath( vmSpace, vmSpace->daoBinPath->mbs );
	DString_SetMBS( name, ".." );
	Dao_MakePath( vmSpace->daoBinPath, name );
	DaoVmSpace_AddPath( vmSpace, name->mbs );
#ifdef UNIX
	DString_SetMBS( name, "../shared/daomake" );
	Dao_MakePath( vmSpace->daoBinPath, name );
	DaoVmSpace_AddPath( vmSpace, name->mbs );
#endif
	if( platform && *platform ){
		DaoNamespace *pns;
		DString_SetMBS( daomake_platform, platform );
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
	srcdir->mbs[srcdir->size] = '\0';

	daomake_out_of_source = DString_EQ( srcdir, vmSpace->startPath ) == 0;
	daomake_main_source_path = srcdir;
	daomake_test_tool = DString_New(1);
	daomake_test_tool_option = DString_New(1);
	DString_SetMBS( daomake_test_tool, "daotest" );
	DString_SetMBS( daomake_test_tool_option, "--log" );

	DString_SetMBS( name, daomake_objects_dir );
	DaoMake_MakePath( daomake_main_source_path, name );
	if( daomake_out_of_source ){
		DaoMake_MakeOutOfSourcePath( name, 0 );
	}else{
		DaoMake_MakeDir( name->mbs );
	}

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
		DString_AppendPathSep( name );
		DString_AppendMBS( name, "Makefile" );
		DString_AppendMBS( name, daomake_makefile_suffix );
		DaoMake_MakeOutOfSourcePath( name, 1 );
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
		DaoMakeProject_MakeFindPackage( project, source, 0 );
		if( source->size == 0 ) continue;

		DString_Reset( name, 0 );
		DString_Append( name, project->sourcePath );
		DString_AppendPathSep( name );
		DString_AppendMBS( name, "Find" );
		DString_Append( name, project->projectName );
		DString_AppendMBS( name, ".dao" );
		DaoMake_MakeOutOfSourcePath( name, 1 );

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
