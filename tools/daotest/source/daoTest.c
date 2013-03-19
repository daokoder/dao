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




static int passes = 0, mpasses = 0;
static int fails = 0, mfails = 0;
static int passes2 = 0, mpasses2 = 0;
static int fails2 = 0, mfails2 = 0;

void PrintTestSummary( const char *project )
{
	if( passes == passes2 && fails == fails2 ) return;
	printf( "%-20s :    files,%3i passed,%3i failed;    units,%4i passed,%4i failed;\n",
			project, mpasses - mpasses2, mfails - mfails2, passes - passes2, fails - fails2 );
}

int main( int argc, char **argv )
{
	DaoTestStream stream0 = {NULL,NULL,NULL,NULL,NULL,NULL};
	DaoTestStream *stream = & stream0;
	DaoNamespace *ns;
	DString *string;
	char project[512] = {0};
	char project2[512] = {0};
	char *testfile, *cur, *end;
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
			strncpy( project, cur, head - cur );
			project[head - cur] = '\0';
			if( strcmp( project, project2 ) != 0 ) changed = 1;
			cur = head + 3;
		}else{
			changed = 1;
		}
		if( changed ){
			PrintTestSummary( project2 );
			passes2 = passes;
			mpasses2 = mpasses;
			fails2 = fails;
			mfails2 = mfails;
		}
		strcpy( project2, project );

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
			int pass = 0, fail = 0;
			DaoProcess *proc = DaoVmSpace_AcquireProcess( vmSpace );
			DString *output = DString_New(1);
			DString *output2 = DString_New(1);
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

				stream->output = output;
				DString_Reset( output, 0 );
				DaoNamespace_AddParent( ns2, ns );
				DaoProcess_Eval( proc, ns2, codes->mbs, 1 );
				DString_Trim( output );
				DString_Trim( result );
				if( output->size == 0 && result->size != 0 ){
					/* If there is no output, check the lasted evaluated value: */
					DaoProcess *proc2 = DaoVmSpace_AcquireProcess( vmSpace );
					DaoNamespace *ns3 = DaoNamespace_New( vmSpace, "result" );
					int cmp;
					stream->output = output2;
					DString_Reset( output2, 0 );
					DaoNamespace_AddParent( ns3, ns );
					DaoProcess_Eval( proc2, ns3, result->mbs, 1 );
					cmp = DaoValue_Compare( proc->stackValues[0], proc2->stackValues[0] );
					DaoVmSpace_ReleaseProcess( vmSpace, proc2 );
					DaoGC_TryDelete( (DaoValue*) ns3 );
					pass += cmp == 0;
					fail += cmp != 0;
				}else if( DString_EQ( output, result ) ){
					/* Check if the output is the same as expected: */
					pass += 1;
				}else if( (regex = DaoProcess_MakeRegex( proc, result, 1 )) ){
					/* Check if the result is a string pattern and if the output matches it: */
					daoint start = 0;
					daoint end = output->size;
					int match = DaoRegex_Match( regex, output, & start, & end );
					pass += match != 0;
					fail += match == 0;
				}else{
					fail += 1;
				}
				if( fail > failed ){
					if( output->size > 500 ) DString_Reset( output, 500 );
					fprintf( stderr, "\nFAILED: %s (%s) ", cur, id->mbs );
					fprintf( stderr, "with the following output:\n%s\n", output->mbs );
				}
				DaoGC_TryDelete( (DaoValue*) ns2 );
			}
			DaoVmSpace_ReleaseProcess( vmSpace, proc );
			DString_Delete( output );
			DString_Delete( output2 );
			passes += pass;
			fails += fail;
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
	PrintTestSummary( project2 );

	printf( "----------\n" );
	printf( "All Tests:    files, %3i passed, %3i failed;    units, %3i passed, %3i failed;\n",
			mpasses, mfails, passes, fails );

	return fails;
}
