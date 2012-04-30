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
// * Redistributions of source code must retain the above copyright notice, this list
//   of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright notice, this list
//   of conditions and the following disclaimer in the documentation and/or other materials
//   provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
// TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
