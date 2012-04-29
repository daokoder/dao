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

//#include"artifact454.c"
#include"virt.c"

/*#include"mcheck.h" */

int main( int argc, char **argv )
{
	int i, k, idsrc;
	DString *opts, *args;
	DaoVmSpace *vmSpace;

	/*mtrace(); */

	vmSpace = DaoInit( argv[0] );

	args  = DString_New(1);
	for(i=1; i<argc; i++ ){
		DString_AppendMBS( args, argv[i] );
		DString_AppendChar( args, '\0' );
	}
	k = 0;
	while( dao_virtual_files[k][0] ){
		DaoVmSpace_AddVirtualFile( vmSpace, dao_virtual_files[k][0], dao_virtual_files[k][1] );
		k ++;
	}
	if( k ==0 ) return 1;
	DString_InsertChar( args, '\0', 0 );
	DString_InsertMBS( args, dao_virtual_files[0][0], 0, 0, 0 );
	DaoVmSpace_SetPath( vmSpace, "/@/" ); // path for the virtual files

	/* Start execution. */
	if( ! DaoVmSpace_RunMain( vmSpace, args ) ) return 1;
	DaoQuit();

	return 0;
}
