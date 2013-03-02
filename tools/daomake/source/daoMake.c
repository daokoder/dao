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

#include"stdio.h"
#include"stdlib.h"
#include"string.h"
#include"dao.h"
#include"daoStdtype.h"
#include"daoNamespace.h"
#include"daoProcess.h"


static DaoVmSpace *vmSpace = NULL;


typedef struct DaoMakeComUnit  DaoMakeComUnit;
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



struct DaoMakeComUnit
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
	DaoMakeComUnit  base;

	DArray  *headers;
	DArray  *sources;
};

struct DaoMakeTarget
{
	DaoMakeComUnit  base;

	DString  *name;
	DArray   *objects;
	daoint    ttype;
};

struct DaoMakeProject
{
	DaoMakeComUnit  base;

	DString  *sourceName;
	DString  *sourcePath;
	DString  *projectName;

	DArray  *objects;
	DArray  *targets;
	DArray  *subProjects;

	DArray  *compilingFlags;
	DArray  *linkingFlags;

	uchar_t  enableDynamicLinking;
	uchar_t  enableDynamicExporting;

	DMap  *headerMacros;   /* HEADERS = header1.h header2.h; */
	DMap  *cflagsMacros;   /* CFLAGS = ...; */
	DMap  *lflagsMacros;   /* LFLAGS = ...; */
	DMap  *objectRules;    /* TARGET: DEPS \n\t COMMAND; */
	DMap  *objectsMacros;  /* OBJECTS = ...; */
	DMap  *targetRules;    /* TARGET: DEPS \n\t COMMAND; */

	DString  *stringBuffer1;
	DString  *stringBuffer2;
	DString  *stringBuffer3;
	DString  *stringBuffer4;
	DString  *stringBuffer5;
	DString  *stringBuffer6;
	DString  *mbsBuffer1;
	DString  *mbsBuffer2;
};



static DaoMap *daomake_platform = NULL;
static DaoMap *daomake_projects = NULL;

static DaoType *daomake_type_comunit = NULL;
static DaoType *daomake_type_objects = NULL;
static DaoType *daomake_type_target  = NULL;
static DaoType *daomake_type_project = NULL;



void DaoMakeComUnit_Init( DaoMakeComUnit *self, DaoType *type )
{
	DaoCstruct_Init( (DaoCstruct*)self, type );
	self->definitions = DArray_New(D_STRING);
	self->includePaths = DArray_New(D_STRING);
	self->linkingPaths = DArray_New(D_STRING);
	self->compilingFlags = DArray_New(D_STRING);
	self->linkingFlags = DArray_New(D_STRING);
	self->usingPackages = DArray_New(D_VALUE);
}
void DaoMakeComUnit_Free( DaoMakeComUnit *self )
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
	DaoMakeComUnit_Init( (DaoMakeComUnit*) & self->base, daomake_type_objects );
	self->headers = DArray_New(D_STRING);
	self->sources = DArray_New(D_STRING);
	return self;
}
void DaoMakeObjects_Delete( DaoMakeObjects *self )
{
	DaoMakeComUnit_Free( (DaoMakeComUnit*) & self->base );
	DArray_Delete( self->headers );
	DArray_Delete( self->sources );
	dao_free( self );
}
DaoMakeTarget* DaoMakeTarget_New()
{
	DaoMakeTarget *self = (DaoMakeTarget*) dao_calloc( 1, sizeof(DaoMakeTarget) );
	DaoMakeComUnit_Init( (DaoMakeComUnit*) & self->base, daomake_type_objects );
	self->name = DString_New(1);
	self->objects = DArray_New(D_STRING);
	self->ttype = DAOMAKE_EXECUTABLE;
	return self;
}
void DaoMakeTarget_Delete( DaoMakeTarget *self )
{
	DaoMakeComUnit_Free( (DaoMakeComUnit*) & self->base );
	DString_Delete( self->name );
	DArray_Delete( self->objects );
	dao_free( self );
}
DaoMakeProject* DaoMakeProject_New()
{
	DaoMakeProject *self = (DaoMakeProject*) dao_calloc( 1, sizeof(DaoMakeProject) );
	DaoMakeComUnit_Init( (DaoMakeComUnit*) & self->base, daomake_type_objects );
	self->sourceName = DString_New(1);
	self->sourcePath = DString_New(1);
	self->projectName = DString_New(1);

	self->objects = DArray_New(D_VALUE);
	self->targets = DArray_New(D_VALUE);
	self->subProjects = DArray_New(D_VALUE);

	self->compilingFlags = DArray_New(D_STRING);
	self->linkingFlags = DArray_New(D_STRING);

	self->enableDynamicLinking = 0;
	self->enableDynamicExporting = 0;

	self->headerMacros = DMap_New(D_STRING,D_STRING);
	self->cflagsMacros = DMap_New(D_STRING,D_STRING);
	self->lflagsMacros = DMap_New(D_STRING,D_STRING);
	self->objectRules = DMap_New(D_STRING,D_STRING);
	self->objectsMacros = DMap_New(D_STRING,D_STRING);
	self->targetRules = DMap_New(D_STRING,D_STRING);

	self->stringBuffer1 = DString_New(1);
	self->stringBuffer2 = DString_New(1);
	self->stringBuffer3 = DString_New(1);
	self->stringBuffer4 = DString_New(1);
	self->stringBuffer5 = DString_New(1);
	self->stringBuffer6 = DString_New(1);
	self->mbsBuffer1 = DString_New(1);
	self->mbsBuffer2 = DString_New(1);
	return self;
}
void DaoMakeProject_Delete( DaoMakeProject *self )
{
	DaoMakeComUnit_Free( (DaoMakeComUnit*) & self->base );
	DString_Delete( self->sourceName );
	DString_Delete( self->sourcePath );
	DString_Delete( self->projectName );

	DArray_Delete( self->objects );
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

	DString_Delete( self->stringBuffer1 );
	DString_Delete( self->stringBuffer2 );
	DString_Delete( self->stringBuffer3 );
	DString_Delete( self->stringBuffer4 );
	DString_Delete( self->stringBuffer5 );
	DString_Delete( self->stringBuffer6 );
	DString_Delete( self->mbsBuffer1 );
	DString_Delete( self->mbsBuffer2 );
	dao_free( self );
}



void DaoMakeTarget_SetName( DaoMakeTarget *self, DString *name )
{
	DString_Reset( self->name, 0 );
	DString_Append( self->name, name );
	if( daomake_platform ){
		const char *key = daomake_suffix_keys[ self->ttype ];
		DaoValue *value = DaoMap_GetValueMBS( daomake_platform, key );
		DString *suffix = DaoValue_TryGetString( value );
		if( suffix ) DString_Append( self->name, suffix );
	}
}




void DaoMakeComUnit_MakeCompilingFlags( DaoMakeComUnit *self, DString *cflags )
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
void DaoMakeComUnit_MakeLinkingFlags( DaoMakeComUnit *self, DString *lflags )
{
	daoint i, j;
	DString_Reset( lflags, 0 );
	for(i=0; i<self->linkingPaths->size; ++i){
		DString *path = self->linkingPaths->items.pString[i];
		DString_AppendMBS( lflags, " -L" );
		DString_Append( lflags, path );
	}
	for(i=0; i<self->linkingFlags->size; ++i){
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


DString* DaoMakeProject_SubMD5( DaoMakeProject *self, DString *data )
{
	DString *md5 = self->mbsBuffer1;
	//DString_MD5( data, md5 );
	DString_SetMBS( data, "12345678" );
	DString_ToUpper( md5 );
	DString_Reset( md5, 8 ); /* TODO: customizable; */
	return md5;
}


/* Return macro name: */
DString* DaoMakeProject_MakeHeaderMacro( DaoMakeProject *self, DaoMakeObjects *objects )
{
	DString *files = self->stringBuffer1;
	DString *macro = self->stringBuffer2;
	DString *md5 = self->mbsBuffer1;
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
	if( it ) return it->key.pString;

	it = DMap_Insert( self->headerMacros, macro, macro );
	DString_AppendMBS( it->value.pString, " = " );
	DString_Append( it->value.pString, files );

	return it->key.pString;
}

DString* DaoMakeProject_MakeSimpleMacro( DaoMakeProject *self, DMap *macros, DString *value, const char *prefix )
{
	DString *name = self->stringBuffer1;
	DString *md5 = DaoMakeProject_SubMD5( self, value );
	DNode *it;

	DString_Reset( name, 0 );
	DString_AppendMBS( name, prefix );
	DString_AppendChar( name, '_' );
	DString_Append( name, md5 );

	it = DMap_Find( macros, name );
	if( it ) return it->key.pString;

	it = DMap_Insert( self->headerMacros, name, name );
	DString_AppendMBS( it->value.pString, " =" );
	DString_Append( it->value.pString, value );
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
	DString *cflags = self->stringBuffer1;
	DString *cflag = self->stringBuffer2;
	DString *signature = self->stringBuffer3;
	DString *md5 = self->mbsBuffer1;
	DNode *it;

	DString_Reset( cflags, 0 );
	DaoMakeComUnit_MakeCompilingFlags( & self->base, cflag );
	DString_Append( cflags, cflag );

	DaoMakeComUnit_MakeCompilingFlags( & target->base, cflag );
	DString_Append( cflags, cflag );

	DaoMakeComUnit_MakeCompilingFlags( & objects->base, cflag );
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
	if( it ) return it->key.pString;

	it = DMap_Insert( self->headerMacros, signature, signature );
	DString_AppendMBS( it->value.pString, ": " );
	DString_Append( it->value.pString, source );
	DString_AppendMBS( it->value.pString, " " );
	DString_Append( it->value.pString, DaoMakeProject_MakeHeaderMacro( self, objects ) );
	DString_AppendMBS( it->value.pString, "\n\t$(CC) $(" ); // TODO: C++
	DString_Append( it->value.pString, DaoMakeProject_MakeCFlagsMacro( self, cflags ) );
	DString_AppendMBS( it->value.pString, ") -c " );
	DString_Append( it->value.pString, source );
	DString_AppendMBS( it->value.pString, " -o " );
	DString_Append( it->value.pString, signature );

	return it->key.pString;
}

/* Return objects macro name: */
DString* DaoMakeProject_MakeObjectsMacro( DaoMakeProject *self, DaoMakeTarget *target, DaoMakeObjects *objects )
{
	DString *objs = self->stringBuffer4;
	daoint i;

	DString_Reset( objs, 0 );
	for(i=0; i<objects->sources->size; ++i){
		DString *source = objects->sources->items.pString[i];
		DString *obj = DaoMakeProject_MakeObjectRule( self, target, objects, source );
		if( i ) DString_AppendChar( objs, ' ' );
		DString_Append( objs, obj );
	}
	return DaoMakeProject_MakeSimpleMacro( self, self->objectsMacros, objs, "OBJECTS" );
}

DString* DaoMakeProject_MakeTargetRule( DaoMakeProject *self, DaoMakeTarget *target )
{
	DString *lflags = DString_New(1);
	DString *lflag = self->stringBuffer2;
	DString *signature = self->stringBuffer3;
	DString *rule = self->stringBuffer5;
	DString *objs = self->stringBuffer5;
	DString *lflagMacro;
	DNode *it;
	daoint i;

	DaoMakeComUnit_MakeLinkingFlags( & self->base, lflag );
	DString_Append( lflags, lflag );

	DaoMakeComUnit_MakeLinkingFlags( & target->base, lflag );
	DString_Append( lflags, lflag );

	DString_Reset( rule, 0 );
	DString_Reset( objs, 0 );
	DString_Append( rule, target->name );
	DString_AppendMBS( rule, ": " );
	for(i=0; i<target->objects->size; ++i){
		DaoMakeObjects *objects = (DaoMakeObjects*) target->objects->items.pVoid[i];
		DString *objmacro = DaoMakeProject_MakeObjectsMacro( self, target, objects );

		DaoMakeComUnit_MakeLinkingFlags( & objects->base, lflag );
		DString_Append( lflags, lflag );
		if( i ) DString_AppendChar( objs, ' ' );
		DString_AppendMBS( objs, "$(" );
		DString_Append( objs, objmacro );
		DString_AppendChar( objs, ')' );
	}
	lflagMacro = DaoMakeProject_MakeLFlagsMacro( self, lflags );
	DString_Delete( lflags );

	DString_Append( rule, objs );
	DString_AppendMBS( rule, "\n\t$(CC) " ); // TODO: C++
	DString_Append( rule, objs );
	DString_AppendMBS( rule, " $(" );
	DString_Append( rule, lflagMacro );
	DString_AppendMBS( rule, ") " );

	DString_AppendMBS( rule, " -o " );
	DString_Append( rule, target->name );

	it = DMap_Insert( self->targetRules, target->name, rule );
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




static void DAOMAKE_Project( DaoProcess *proc, DaoValue *p[], int N )
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
static void DAOMAKE_FindPackage( DaoProcess *proc, DaoValue *p[], int N )
{
}
static void DAOMAKE_FindFile( DaoProcess *proc, DaoValue *p[], int N )
{
}
static void DAOMAKE_IsPlatform( DaoProcess *proc, const char *name )
{
	DaoValue *value = DaoMap_GetValueMBS( daomake_platform, name );
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
	{ DAOMAKE_Project,     "Project( name : string ) => Project" },
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
DaoTypeBase DaoMakeProject_Typer =
{
	"DaoMake", NULL, NULL, (DaoFuncItem*) DaoMakeMeths, {0}, {0},
	(FuncPtrDel)DaoMakeProject_Delete, NULL
};



int main( int argc, char **argv )
{
	int i, k;
	char *platform = NULL;
	char *mode = NULL;
	char *makefile = "makefile.dao";
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

	daomake_platform = DaoMap_New(1);
	daomake_projects = DaoMap_New(1);
	DaoGC_IncRC( (DaoValue*) daomake_platform );
	DaoGC_IncRC( (DaoValue*) daomake_projects );

	/* Start execution. */
	k = ! DaoVmSpace_RunMain( vmSpace, makefile );

	name = DString_New(1);
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

	DaoGC_DecRC( (DaoValue*) daomake_platform );
	DaoGC_DecRC( (DaoValue*) daomake_projects );

	DaoQuit();
	return k;
}
