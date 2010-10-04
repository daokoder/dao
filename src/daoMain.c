/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2010, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#include"stdio.h"
#include"stdlib.h"
#include"string.h"
#include"daoType.h"
#include"daoVmspace.h"

/*#include"mcheck.h" */

int main( int argc, char **argv )
{
	int i, k, idsrc;
	char *daodir = getenv( "DAO_DIR" );
	DString *opts, *args;
	DaoVmSpace *vmSpace;

	if( daodir == NULL && argv[0][0] == '/' ){
		k = strlen( argv[0] );
		if( strcmp( argv[0] + k - 4, "/dao" ) ==0 ){
			daodir = (char*) dao_malloc( k + 10 );
			strncpy( daodir, "DAO_DIR=", 9 );
			strncat( daodir, argv[0], k - 4 );
			putenv( daodir );
			daodir += 8;
		}
	}
	/*mtrace(); */

	vmSpace = DaoInit();
	idsrc = -1;
	for(i=1; i<argc; i++){
		char *ch = strstr( argv[i], ".dao" );
		char *ch2 = strstr( argv[i], ".cgi" );
		if( ch && ( strcmp( ch, ".dao" ) ==0 || strcmp( ch, ".dao.o" ) ==0
					|| strcmp( ch, ".dao.s" ) ==0 ) ){
			idsrc = i;
			break;
		}else if( ch2 && strcmp( ch2, ".cgi" ) == 0 ){
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
	if( ! DaoVmSpace_RunMain( vmSpace, args ) ) return 1;
	DString_Delete( args );
	DString_Delete( opts );
	DaoQuit();
	/* printf( "FINISHED %s\n", getenv( "PROC_NAME" ) ); */
	return 0;
}
