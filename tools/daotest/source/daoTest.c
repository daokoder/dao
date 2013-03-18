/*
// Dao Test Tool
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



typedef struct DaoTestStream  DaoTestStream;

struct DaoTestStream
{
	DaoStream *stream;
	/* count>0: read count bytes; count=0: one line; count<0: until EOF */
	void (*StdioRead)( DaoTestStream *self, DString *input, int count );
	void (*StdioWrite)( DaoTestStream *self, DString *output );
	void (*StdioFlush)( DaoTestStream *self );
	void (*SetColor)( DaoTestStream *self, const char *fgcolor, const char *bgcolor );

	DString  *output;
};


static void DaoTestStream_Write( DaoTestStream *self, DString *output )
{
	DString_Append( self->output, output );
}



static DArray  *dao_tests = NULL;
static DaoVmSpace *vmSpace = NULL;

static int dao_test_inliner( DaoNamespace *NS, DString *mode, DString *VT, DString *out )
{
	daoint start = 0, rb = DString_FindChar( VT, ']', 0 );

	DString_Reset( out, 0 );
	DString_SetDataMBS( out, VT->mbs + rb + 1, VT->size - 2*(rb + 1) );
	while( start < out->size && isspace( out->mbs[start] ) ) start += 1;

	if( (dao_tests->size % 3) == 0 ) DArray_Append( dao_tests, mode );
	DArray_Append( dao_tests, out );

	DString_Reset( out, 0 );
	DString_AppendChar( out, ' ' );
	return 0;
}



static const char *const daotest_stat = "Tests for project: %s, %i passes, %i fails;\n";


int main( int argc, char **argv )
{
	DaoTestStream stream0 = {NULL,NULL,NULL,NULL,NULL,NULL};
	DaoTestStream *stream = & stream0;
	DaoNamespace *ns;
	DString *string;
	char project[512] = {0};
	char project2[512] = {0};
	char *testfile, *cur, *end;
	int passes = 0, mpasses = 0;
	int fails = 0, mfails = 0;
	int pass = 0, fail = 0;
	int i, k, len;
	FILE *fin;

	if( argc <= 1 ) return 0;

	vmSpace = DaoInit( argv[0] );

	string = DString_New(1);
	fin = fopen( argv[1], "r" );
	if( fin ) DaoFile_ReadAll( fin, string, 1 );
	if( string->size == 0 ){
		DString_Delete( string );
		DaoQuit();
		return 0;
	}
	len = string->size;
	testfile = (char*) dao_malloc( len + 1 );
	memmove( testfile, string->mbs, len + 1 );
	DString_Delete( string );
	DaoQuit();


	cur = testfile;
	end = testfile + len;
	while( cur < end ){
		char *next = strchr( cur, '\n' );
		char *head = strstr( cur, ":: " );
		char *proj = cur;
		int changed = 0;
		if( next ) *next = '\0';
		if( *cur == '#' ){
			if( next == NULL ) break;
			cur = next + 1;
			continue;
		}
		if( head ){
			strncpy( project2, cur, head - cur );
			if( strcmp( project, project2 ) != 0 ) changed = 1;
			cur = head + 3;
		}else{
			changed = 1;
		}
		if( changed ){
			if( pass + fail ) printf( daotest_stat, project, pass, fail );
			pass = fail = 0;
		}
		strcpy( project, project2 );

		vmSpace = DaoInit( argv[0] );

		ns = DaoVmSpace_GetNamespace( vmSpace, "dao" );
		DaoNamespace_AddCodeInliner( ns, "test", dao_test_inliner );

		string = DString_New(1);
		dao_tests = DArray_New(D_STRING);
		ns = DaoVmSpace_Load( vmSpace, cur );
		if( ns == NULL ){
			mfails += 1;
			fails += 1;
		}else{
			DaoProcess *proc = DaoVmSpace_AcquireProcess( vmSpace );
			DString *output = DString_New(1);
			DaoRegex *regex;
			stream->StdioWrite = DaoTestStream_Write;
			stream->output = output;
			DaoVmSpace_SetUserStdio( vmSpace, (DaoUserStream*) stream );
			for(i=0; i<dao_tests->size; i+=3){
				DString *id = dao_tests->items.pString[i];
				DString *codes = dao_tests->items.pString[i+1];
				DString *result = dao_tests->items.pString[i+2];
				DaoNamespace *ns2 = DaoNamespace_New( vmSpace, "test" );
				int failed = fail;

				DString_Reset( output, 0 );
				DaoNamespace_AddParent( ns2, ns );
				DaoProcess_Eval( proc, ns2, codes->mbs, 1 );
				DString_Trim( output );
				DString_Trim( result );
				if( DString_EQ( output, result ) ){
					pass += 1;
				}else if( (regex = DaoProcess_MakeRegex( proc, result, 1 )) ){
					daoint start = 0;
					daoint end = output->size;
					if( DaoRegex_Match( regex, output, & start, & end ) ){
						pass += 1;
					}else{
						fail += 1;
					}
				}else{
					fail += 1;
				}
				if( fail > failed ){
					if( output->size > 500 ) DString_Reset( output, 500 );
					fprintf( stderr, "FAILED: %s (%s) ", cur, id->mbs );
					fprintf( stderr, "with the following output:\n%s\n", output->mbs );
				}
				DaoGC_TryDelete( (DaoValue*) ns2 );
			}
			DaoVmSpace_ReleaseProcess( vmSpace, proc );
			DString_Delete( output );
			passes += pass;
			mpasses += fail == 0;
			mfails += fail != 0;
		}
		DString_Delete( string );
		DArray_Delete( dao_tests );
		DaoQuit();

		if( next == NULL ) break;
		cur = next + 1;
		continue;
	}
	if( pass + fail ) printf( daotest_stat, project, pass, fail );

	return fails;
}
