/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2011, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

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

#ifdef DAO_USE_READLINE
#include"readline/readline.h"
#include"readline/history.h"
#endif

#include"daoType.h"
#include"daoVmspace.h"

//#include"artifact454.c"
#include"virt.c"

/*#include"mcheck.h" */

static int readingline = 0;
static DaoVmSpace *vmSpace = NULL;

static char* DaoReadLine( const char *s )
{
	char *line;
	readingline = 1;
#ifdef DAO_USE_READLINE
	line = readline( s );
#endif
	readingline = 0;
	return line;
}
static void DaoSignalHandler( int sig )
{
	DaoVmSpace_Stop( vmSpace, 1);
	if( readingline ){
		printf( "\n" );
#ifdef DAO_USE_READLINE
		if( rl_end ==0 ) printf( "type \"q\" to quit.\n" );
		rl_replace_line( "", 0 );
		rl_forced_update_display();
#endif
	}else{
		printf( "keyboard interrupt...\n" );
	}
}

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

#ifdef DAO_USE_READLINE
	DaoVmSpace_ReadLine( vmSpace, DaoReadLine );
	DaoVmSpace_AddHistory( vmSpace, add_history );
#endif

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

	//printf( "FINISHED %s\n", getenv( "PROC_NAME" ) );
	return 0;
}
