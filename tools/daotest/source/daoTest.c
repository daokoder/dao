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

static int dao_test_inliner( DaoNamespace *NS, DString *mode, DString *VT, DString *out, int line )
{
	daoint start = 0, rb = DString_FindChar( VT, ']', 0 );

	if( (dao_tests->size % 3) == 0 ){
		DString_Reset( out, 0 );
		DString_AppendInteger( out, line );
		DArray_Append( dao_tests, out );
	}

	DString_Reset( out, 0 );
	DString_SetDataMBS( out, VT->mbs + rb + 1, VT->size - 2*(rb + 1) );
	while( start < out->size && isspace( out->mbs[start] ) ) start += 1;

	DArray_Append( dao_tests, out );

	DString_Reset( out, 0 );
	DString_AppendChar( out, ' ' );
	return 0;
}

const char *last_line_format =
"Summary:  files, %2i passed, %2i failed;  units: %3i passed, %3i failed;\n";

const char *last_line_format2 =
"All Test Summary:  files, %i passed, %i failed;  units: %i passed, %i failed;\n";

const char *last_line_pattern =
"<1>(.*) (([^\n]*)) {{Summary:  files,}} %s* "
"<2>(%d+) {{ passed,}} %s* <3>(%d+) {{ failed;  units:}} %s*"
"<4>(%d+) {{ passed,}} %s* <5>(%d+) {{ failed;}}%s* $";

void GetCounts( DString *text, int *mps, int *mfs, int *ps, int *fs )
{
	char *p;
	DString_ChangeMBS( text, last_line_pattern, "%2 %3 %4 %5", 0 );
	*mps = strtoll( text->mbs, & p, 10 );
	*mfs = strtoll( p + 1, & p, 10 );
	*ps = strtoll( p + 1, & p, 10 );
	*fs = strtoll( p + 1, & p, 10 );
}


int main( int argc, char **argv )
{
	DaoTestStream stream0 = {NULL,NULL,NULL,NULL,NULL,NULL};
	DaoTestStream *stream = & stream0;
	DaoNamespace *ns;
	DString *string;
	DString *summary;
	DString *info;
	FILE *fin, *logfile = NULL;
	int passes = 0, mpasses = 0;
	int fails = 0, mfails = 0;
	int i, j, logopt = argc;
	int groupopt = argc;

	if( argc <= 1 ) return 0;

	for(i=1; i<argc; ++i){
		if( strcmp( argv[i], "--log" ) == 0 ){
			if( logopt == argc && (i + 1) < argc ) logopt = i;
		}else if( strcmp( argv[i], "--group" ) == 0 ){
			if( groupopt == argc && (i + 1) < argc ) groupopt = i;
		}
	}

	if( strcmp( argv[1], "--sum" ) == 0 ){
		int passes2 = 0, mpasses2 = 0;
		int fails2 = 0, mfails2 = 0;
		if( logopt == argc ) return 1;
		vmSpace = DaoInit( argv[0] );
		string = DString_New(1);
		summary = DString_New(1);
		info = DString_New(1);
		logfile = fopen( argv[logopt+1], "r+b" );
		if( logfile ){
			DaoFile_ReadAll( logfile, string, 0 );
			DString_Assign( summary, string );
			DString_ChangeMBS( summary, last_line_pattern, "%1", 0 );
			GetCounts( string, & mpasses, & mfails, & passes, & fails );
			fseek( logfile, 0, SEEK_SET );
		}else{
			logfile = fopen( argv[logopt+1], "w+b" );
		}
		fprintf( logfile, "%s", summary->mbs );
		DString_Reset( summary, 0 );
		for(i=2; i<logopt; ++i){
			daoint start = 0, end;
			int mps, ps, mfs, fs;
			fin = fopen( argv[i], "rb" );
			if( fin == NULL ) continue;
			DaoFile_ReadAll( fin, string, 1 );
			DString_Assign( info, string );
			DString_ChangeMBS( info, last_line_pattern, "%1", 0 );
			DString_Append( summary, info );
			end = string->size;
			if( DString_MatchMBS( string, last_line_pattern, & start, & end ) == 0 ) continue;

			GetCounts( string, & mps, & mfs, & ps, & fs );
			mpasses2 += mps;
			passes2 += ps;
			mfails2 += mfs;
			fails2 += fs;
		}
		fprintf( logfile, "%-15s", argv[groupopt+1] );
		fprintf( logfile, last_line_format, mpasses2, mfails2, passes2, fails2 );
		mpasses += mpasses2;
		mfails += mfails2;
		passes += passes2;
		fails += fails2;
		fprintf( logfile, "%s\n", summary->mbs );
		fprintf( logfile, last_line_format2, mpasses, mfails, passes, fails );
		fclose( logfile );
		DString_Delete( string );
		DString_Delete( summary );
		DString_Delete( info );
		DaoQuit();
		return 0;
	}

	logfile = fopen( argv[logopt+1], "w+b" );
	for(i=1; i<logopt; ++i){
		vmSpace = DaoInit( argv[0] );

		ns = DaoVmSpace_GetNamespace( vmSpace, "dao" );
		DaoNamespace_AddCodeInliner( ns, "test", dao_test_inliner );

		string = DString_New(1);
		dao_tests = DArray_New(D_STRING);
		ns = DaoVmSpace_Load( vmSpace, argv[i] );
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
			for(j=0; j<dao_tests->size; j+=3){
				DString *id = dao_tests->items.pString[j];
				DString *codes = dao_tests->items.pString[j+1];
				DString *result = dao_tests->items.pString[j+2];
				DaoNamespace *ns2 = DaoNamespace_New( vmSpace, "test" );
				int failed = fail;

				stream->output = output;
				DString_Reset( output, 0 );
				DaoNamespace_AddParent( ns2, ns );
				DaoProcess_Eval( proc, ns2, codes->mbs );
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
					DaoProcess_Eval( proc2, ns3, result->mbs );
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
					FILE *log = logfile ? logfile : stderr;
					if( output->size > 500 ) DString_Reset( output, 500 );
					fprintf( log, "\n#############################################\n" );
					fprintf( log, "\nFAILED: %s, line %s:\n", argv[i], id->mbs );
					fprintf( log, "OUTPUT:\n\n%s\n\n", output->mbs );
					fprintf( log, "EXPECTED:\n\n%s\n\n\n", result->mbs );
					fflush( log );
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
	}

	printf( "Test summary:\nfiles: %4i passed, %4i failed;\nunits: %4i passed, %4i failed;\n",
			mpasses, mfails, passes, fails );

	if( logfile ){
		if( fails ) fprintf( logfile, "#############################################\n" );
		fprintf( logfile, last_line_format, mpasses, mfails, passes, fails );
		fclose( logfile );
	}
	return fails;
}
