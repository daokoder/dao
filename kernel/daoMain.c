/*
// Dao Virtual Machine
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

#include"stdio.h"
#include"stdlib.h"
#include"string.h"

#include"daoType.h"
#include"daoVmspace.h"

/*#include"mcheck.h" */

static DaoVmSpace *vmSpace = NULL;

int main( int argc, char **argv )
{
	int i, k, idsrc;
	DString *opts, *args;
	DaoVmSpace *vmSpace;

	/*mtrace(); */

	vmSpace = DaoInit( argv[0] );

	idsrc = -1;
	for(i=1; i<argc; i++){
		if( strcmp( argv[i], "-e" ) ==0 || strcmp( argv[i], "--eval" ) ==0 ) break;
		/* also allows execution of script files without suffix .dao */
		if( argv[i][0] != '-' ){
			idsrc = i;
			break;
		}
	}

	k = idsrc;
	if( k < 0 ) k = argc;

	opts = DString_New(1);
	for(i=1; i<k; i++ ){
		DString_AppendMBS( opts, argv[i] );
		DString_AppendChar( opts, '\0' );
	}

	DaoVmSpace_ParseOptions( vmSpace, opts );

	args  = DString_New(1);
	if( idsrc >= 0 ){
		for(i=idsrc; i<argc; i++ ){
			DString_AppendMBS( args, argv[i] );
			DString_AppendChar( args, '\0' );
		}
	}else if( argc==1 ){
		DString_AppendChar( opts, '\0' );
		DString_AppendMBS( opts, "-vi" );
		DaoVmSpace_ParseOptions( vmSpace, opts );
	}

	/* Start execution. */
	k = ! DaoVmSpace_RunMain( vmSpace, args );
	DString_Delete( args );
	DString_Delete( opts );
	DaoQuit();
	/* printf( "FINISHED %s\n", getenv( "PROC_NAME" ) ); */
	return k;
}
