/*
// This file is part of the virtual machine for the Dao programming language.
// Copyright (C) 2006-2012, Limin Fu. Email: daokoder@gmail.com
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this 
// software and associated documentation files (the "Software"), to deal in the Software 
// without restriction, including without limitation the rights to use, copy, modify, merge, 
// publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons 
// to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or 
// substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
