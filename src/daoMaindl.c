/*=========================================================================================
  This is a shell for the virtual machine for the Dao programming language.
  Copyright (C) 2006-2010, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

/*
 * This file is distributed under GPL to use readline library.
 */

#include"stdio.h"
#include"stdlib.h"
#include"string.h"

#ifdef DAO_USE_READLINE
#include"readline/readline.h"
#include"readline/history.h"
#endif

#include"dao.h"
#include"signal.h"

DAO_INIT_MODULE

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
	DString *opts, *args;
	int i, k, idsrc;

#ifndef DAO_DIRECT_API
	if( DaoInitLibrary( argv[0] ) ==0 ) return 1;
#endif
	vmSpace = DaoInit();

	opts = DString_New(1);
	args = DString_New(1);

#ifdef DAO_USE_READLINE
	DaoVmSpace_ReadLine( vmSpace, DaoReadLine );
	DaoVmSpace_AddHistory( vmSpace, add_history );
#endif

	idsrc = -1;
	for(i=1; i<argc; i++){
		/* also allows execution of script files without suffix .dao */
		if( argv[i][0] != '-' ){
			idsrc = i;
			break;
		}
	}

	k = idsrc;
	if( k < 0 ) k = argc;

	for(i=1; i<k; i++ ){
		DString_AppendMBS( opts, argv[i] );
		DString_AppendChar( opts, '\0' );
	}

	DaoVmSpace_ParseOptions( vmSpace, opts );

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
	if( strstr( DString_GetMBS( opts ), "v" ) ){ /*XXX*/
		printf( "\n  A simple shell for the Dao Virtual Machine.\n" );
		printf( "  Copyright(C) 2006-2010, Fu Limin.\n" );
		printf( "  This shell is distributed under GNU General Public License.\n" );
	}
	if( DaoVmSpace_GetOptions( vmSpace ) & DAO_EXEC_INTERUN )
		signal( SIGINT, DaoSignalHandler );

	if( ! DaoVmSpace_RunMain( vmSpace, args ) ) return 1;
	DString_Delete( opts );
	DString_Delete( args );
	DaoQuit();
	return 0;
}
