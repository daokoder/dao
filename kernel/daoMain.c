/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2013, Limin Fu
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
// THIS SOFTWARE IS PROVIDED  BY THE COPYRIGHT HOLDERS AND  CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED  WARRANTIES,  INCLUDING,  BUT NOT LIMITED TO,  THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL  THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE FOR ANY DIRECT,
// INDIRECT,  INCIDENTAL, SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL  DAMAGES (INCLUDING,
// BUT NOT LIMITED TO,  PROCUREMENT OF  SUBSTITUTE  GOODS OR  SERVICES;  LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY OF
// LIABILITY,  WHETHER IN CONTRACT,  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include"stdio.h"
#include"stdlib.h"
#include"string.h"
#include"dao.h"

#ifdef DAO_USE_READLINE
#include"readline/readline.h"
#include"readline/history.h"
#endif

#include"signal.h"
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
#ifndef MAC_OSX
		rl_replace_line( "", 0 );
		rl_forced_update_display();
#else
		rl_reset_terminal( "" );
#endif
#endif
	}else{
		printf( "keyboard interrupt...\n" );
	}
}


/*
// Adding virtual modules:
//
// * Create a C source file and define an variable of DaoVModule array;
// * Name the variable as "dao_virtual_modules";
// * Terminate the array with { NULL, ... };
// * Compile files with -DDAO_WITH_STATIC_MODULES;
// * Link these file together (possibly with the C modules);
//
// In this way, these virtual modules can be loaded in the normal way.
//
// DaoVModule dao_virtual_modules[] =
// {
//     { "hello.dao", 21, "io.writeln( 'hello world!' )", NULL },
//     { NULL, 0, NULL, NULL }
// };
*/

extern DaoVModule dao_virtual_modules[];


int main( int argc, char **argv )
{
	int i, k, idsrc, vmods = 0;
	DString *opts, *args;

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

#ifdef DAO_WITH_STATIC_MODULES
	idsrc = 1;
	vmods = 0;
	while( dao_virtual_modules[vmods].name ){
		DaoVmSpace_AddVirtualModule( vmSpace, & dao_virtual_modules[vmods] );
		vmods ++;
	}
#endif

	k = idsrc;
	if( k < 0 ) k = argc;

	opts = DString_New(1);
	args  = DString_New(1);
	for(i=1; i<k; i++ ){
		DString_AppendMBS( opts, argv[i] );
		DString_AppendChar( opts, '\1' );
	}
	if( idsrc >= 0 ){
		for(i=idsrc; i<argc; i++ ){
			DString_AppendMBS( args, argv[i] );
			DString_AppendChar( args, '\1' );
		}
	}
	DaoVmSpace_ParseOptions( vmSpace, DString_GetMBS( opts ) );

#ifdef DAO_WITH_STATIC_MODULES
	if( vmods ){
		DString_InsertChar( args, '\1', 0 );
		DString_InsertMBS( args, dao_virtual_modules[0].name, 0, 0, 0 );
		/* set the path for the virtual files: */
		DaoVmSpace_SetPath( vmSpace, "/@/" );
	}else
#endif
	if( idsrc < 0 && argc == 1 ){
		DString_AppendChar( opts, '\1' );
		DString_AppendMBS( opts, "-vi" );
		DaoVmSpace_ParseOptions( vmSpace, DString_GetMBS( opts ) );
	}


#ifdef DAO_USE_READLINE
	DaoVmSpace_ReadLine( vmSpace, DaoReadLine );
	DaoVmSpace_AddHistory( vmSpace, add_history );
	read_history( NULL );
#endif

	if( DaoVmSpace_GetOptions( vmSpace ) & DAO_OPTION_INTERUN )
		signal( SIGINT, DaoSignalHandler );

	/* Start execution. */
	k = ! DaoVmSpace_RunMain( vmSpace, DString_GetMBS( args ) );

#ifdef DAO_USE_READLINE
	write_history( NULL );
#endif

	DString_Delete( args );
	DString_Delete( opts );
	DaoQuit();
	return k;
}
