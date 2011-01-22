
/*=========================================================================================
   This is the CGI module for the Dao programming language.
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
#include"unistd.h"
#include"time.h"

// must be larger than the size of boundary string for multipart/form-data:
#define LOCAL_BUF_SIZE 1000

#include"dao.h"

DAO_INIT_MODULE;

#ifdef MAC_OSX
#  include <crt_externs.h>
#  define environ (*_NSGetEnviron())
#else
extern char ** environ;
#endif

static DaoVmSpace *vmMaster = NULL;

static void InsertKeyValue( DaoMap *mulmap, DaoMap *map, DValue vk, DValue vv )
{
	DValue vlist = { DAO_LIST, 0, 0, 0, {0} };
	DValue val;
	DaoMap_Insert( map, vk, vv );
	if( mulmap ){
		val = DaoMap_GetValue( mulmap, vk );
		if( val.t == DAO_NIL ){
			vlist.v.list = DaoList_New();
			DaoMap_Insert( mulmap, vk, vlist );
			val = DaoMap_GetValue( mulmap, vk );
		}
		DaoList_PushBack( val.v.list, vv );
	}
}
static void ParseKeyValueString( DaoMap *mulmap, DaoMap *map, const char *s )
{
	int i = 0;
	int nc = 0;
	int len = 0;
	char buffer[ LOCAL_BUF_SIZE + 1 ];
	
	DString *key = DString_New(1);
	DString *value = DString_New(1);
	DValue vk = { DAO_STRING, 0, 0, 0, {0} };
	DValue vv = { DAO_STRING, 0, 0, 0, {0} };
	vk.v.s = key;
	vv.v.s = value;

	len = strlen( s );
	nc = 0;
	int isKey = 1;
	char tmp[3];
	tmp[2] = 0;
	for(i=0; i<len; i++){
		if( s[i] == '=' ){
			buffer[nc] = 0;
			DString_AppendMBS( key, buffer );
			nc = 0;
			isKey = 0;
		}else if( s[i] == '&' || s[i] == ';' || i+1==len ){
			if( i+1 == len ){
				buffer[ nc ] = s[i];
				nc ++;
			}
			if( DString_Size( key ) > 0 ){
				buffer[ nc ] = 0;
				DString_AppendMBS( value, buffer );
				InsertKeyValue( mulmap, map, vk, vv );
				DString_Clear( key );
				DString_Clear( value );
				nc = 0;
				isKey = 1;
			}else if( nc > 0 ){
				buffer[ nc ] = 0;
				DString_AppendMBS( key, buffer );
				DString_SetMBS( value, "NULL" );
				InsertKeyValue( mulmap, map, vk, vv );
				DString_Clear( key );
				DString_Clear( value );
				nc = 0;
				isKey = 1;
			}
		}else if( s[i] != ' ' ){
			if( nc >= LOCAL_BUF_SIZE ){
				buffer[ nc ] = 0;
				if( isKey ){
					DString_AppendMBS( key, buffer );
				}else{
					DString_AppendMBS( value, buffer );
				}
				nc = 0;
			}
			if( s[i] == '%' ){
				tmp[0] = s[i+1];
				tmp[1] = s[i+2];
				buffer[ nc ] = strtol( tmp, NULL, 16 );
				i += 2;
			}else if( s[i] == '+' ){
				buffer[ nc ] = ' ';
			}else{
				buffer[ nc ] = s[i];
			}
			nc ++;
		}
	}
	if( DString_Size( key ) > 0 ){
		buffer[ nc ] = 0;
		DString_AppendMBS( value, buffer );
		InsertKeyValue( mulmap, map, vk, vv );
	}else if( nc > 0 ){
		buffer[ nc ] = 0;
		DString_AppendMBS( key, buffer );
		DString_SetMBS( value, "NULL" );
		InsertKeyValue( mulmap, map, vk, vv );
	}
	DString_Delete( key );
	DString_Delete( value );
}
static void ParseKeyValueStringArray( DaoMap *map, char **p )
{
	int nc = 0;
	char buffer[ LOCAL_BUF_SIZE + 1 ];
	
	DString *key = DString_New(1);
	DString *value = DString_New(1);
	DValue vk = { DAO_STRING, 0, 0, 0, {0} };
	DValue vv = { DAO_STRING, 0, 0, 0, {0} };
	vk.v.s = key;
	vv.v.s = value;
	while( *p != NULL ){
		char *c = *p;
		nc = 0;
		while( *c != '=' ){
			if( nc >= LOCAL_BUF_SIZE ){
				buffer[ nc ] = 0;
				DString_AppendMBS( key, buffer );
				nc = 0;
			}
			buffer[ nc ] = *c;
			nc ++;
			c ++;
		}
		buffer[ nc ] = 0;
		DString_AppendMBS( key, buffer );
		c ++;
		DString_AppendMBS( value, c );
		DaoMap_Insert( map, vk, vv );
		DString_Clear( key );
		DString_Clear( value );
		p ++;
	}
	DString_Delete( key );
	DString_Delete( value );
}
static void PreparePostData( DaoMap *httpPOSTS, DaoMap *httpPOST, DaoMap *httpFILE )
{
	DString *key = DString_New(1);
	DString *value = DString_New(1);
	DString *dynaBuffer = DString_New(1);
	int i = 0;
	int len = 0;
	char buffer[ LOCAL_BUF_SIZE + 1 ];
	char *last = buffer + (LOCAL_BUF_SIZE-1);
	
	char *contentLength = getenv( "CONTENT_LENGTH" );
	char *contentType = getenv( "CONTENT_TYPE" );
	DValue vk = { DAO_STRING, 0, 0, 0, {0} };
	DValue vv = { DAO_STRING, 0, 0, 0, {0} };
	vk.v.s = key;
	vv.v.s = value;
	len = 0;
	*last = 0;
	if( contentLength != NULL ) len = strtol( contentLength, NULL, 10);
	if( contentType != NULL ){
		//printf( "CONTENT_TYPE = %s\n", contentType );
		if( strstr( contentType, "multipart/form-data" ) == NULL ){
			i = 0;
			DString_Clear( dynaBuffer );
			while( i < len ){
				int n = 0;
				int ch = getchar();
				while( ch != EOF ){
					buffer[n] = (char)ch;
					n ++;
					if( n == LOCAL_BUF_SIZE ) break;
					ch = getchar();
				}
				buffer[ n ] = 0;
				//printf( "%s|||||||||||||||||\n", buffer );
				char *p = strchr( buffer, '&' );
				if( p != NULL ){
					*p = 0; // null-terminating
					p++;
					DString_AppendMBS( dynaBuffer, buffer );
					ParseKeyValueString( httpPOSTS, httpPOST, DString_GetMBS( dynaBuffer ) );
					DString_Clear( dynaBuffer );
					DString_AppendMBS( dynaBuffer, p );
				}else{
					DString_AppendMBS( dynaBuffer, buffer );
				}
				i += LOCAL_BUF_SIZE;
			}
			ParseKeyValueString( httpPOSTS, httpPOST, DString_GetMBS( dynaBuffer ) );
		}else{
			char *boundary = strstr( contentType, "boundary" );
			boundary = strstr( boundary, "------" );
			i = 0;
			char *part = NULL;
			while( ! feof( stdin ) ){
				if( part == NULL ) part = fgets( buffer, LOCAL_BUF_SIZE, stdin );
				if( part == NULL ) break;
				if( strstr( part, boundary ) == NULL ) break;

				// read content information
				DString_Clear( dynaBuffer );
				buffer[ LOCAL_BUF_SIZE ] = 0; // null-terminating
				char *p = fgets( buffer, LOCAL_BUF_SIZE, stdin );
				if( p == NULL ) break; // the last boundary scanned
				p = strchr( p, '\n' );
				*p = 0; // null-terminating
				DString_AppendMBS( dynaBuffer, buffer );
				char *info = (char*)DString_GetMBS( dynaBuffer );
				info = strchr( info, '=' );
				info += 2; // at char after name="
				p = info;
				while( *p != '\"' ) p ++;
				*p = 0; // null-terminating
				DString_SetMBS( key,  info );
				p ++;
				if( (p = strstr(p,"filename") ) == NULL ){
					p = fgets( buffer, LOCAL_BUF_SIZE, stdin );
					p = fgets( buffer, LOCAL_BUF_SIZE, stdin );
					// now real data:
					DString_Clear( value );
					while( p != NULL && strstr( p, boundary ) == NULL ){
						char *t = strstr( p, "\r\n" );
						if( t != NULL ) *t = 0;
						DString_AppendMBS( value, buffer );
						if( feof( stdin ) ) break;
						p = fgets( buffer, LOCAL_BUF_SIZE, stdin );
						t = strchr( p, '\n' );
						if( t!= NULL ) *(t+1) = 0;
					}
					if( p != NULL ) part = p;
					DaoMap_Insert( httpPOST, vk, vv );
				}else{
					DaoStream *stream = DaoStream_New();
					FILE *file = tmpfile();
					char *t = NULL;
					DValue vs = { DAO_STREAM, 0, 0, 0, {0} };
					vs.v.stream = stream;
					p = strchr( p, '\"' ) + 1;
					info = p;
					while( *p != '\"' ) p ++;
					*p = 0; // null-terminating
					DaoStream_SetFile( stream, file );
					//XXX stream->TYPER->SetName( stream, info );
					DString_Clear( value );
					// Content-Type ...\r\n
					p = fgets( buffer, LOCAL_BUF_SIZE, stdin );
					// \r\n
					p = fgets( buffer, LOCAL_BUF_SIZE, stdin );
					// data
#if 0
					int count = fread( buffer, 1, LOCAL_BUF_SIZE, stdin );
					while( count && strstr( buffer, boundary ) == NULL ){
						fwrite( buffer, 1, count, file );
						fprintf( file, "%s\n", "===========================" );
						if( feof( stdin ) ) break;
						count = fread( buffer, 1, LOCAL_BUF_SIZE, stdin );
					}
#else
					char tail[3] = { 0, 0, 0 };
					int count, ntail = 0;
					p = fgets( buffer, LOCAL_BUF_SIZE, stdin );

					while( p != NULL && strstr( p, boundary ) == NULL ){
						if( feof( stdin ) ){
							// XXX
							break;
						}else{
							t = p;
							while( t != last && (*t) != '\n' ) t ++;
							if( (*t) == '\n' ){
								count = t-p+1;
								if( count >= 2 ){
									count -= 2;
									if( ntail ) fwrite( tail, 1, ntail, file );
									tail[0] = p[ count ];
									tail[1] = p[ count+1 ];
									ntail = 2;
									fwrite( p, 1, count, file );
								}else if( count == 1 ){
									if( ntail == 2 ){
										fwrite( tail, 1, 1, file );
										tail[0] = tail[1];
										tail[1] = p[0];
									}else if( ntail ==1 ){
										tail[1] = p[0];
										ntail = 2;
									}else{
										tail[0] = p[0];
										ntail = 1;
									}
								}
							}else{
								if( ntail ) fwrite( tail, 1, ntail, file );
								count = LOCAL_BUF_SIZE-3;
								tail[0] = p[ count ];
								tail[1] = p[ count+1 ];
								ntail = 2;
								fwrite( p, 1, count, file );
							}
						}
						p = fgets( buffer, LOCAL_BUF_SIZE, stdin );
					}
#endif
					//if( p != NULL ) part = p;
					rewind( file );
					DaoMap_Insert( httpFILE, vk, vs );
				}
			}
		}
	}
	DString_Delete( key );
	DString_Delete( value );
	DString_Delete( dynaBuffer );
}
/*
static int RequestHandler_Execute( RequestHandler *self )
{
	char *docroot = getenv(  "DOCUMENT_ROOT", self->request.envp );
	char *script = getenv(  "SCRIPT_NAME", self->request.envp );
	char *remote = getenv(  "REMOTE_ADDR", self->request.envp );
	char *conlen = getenv(  "CONTENT_LENGTH", self->request.envp );
	
	char path[512];
	path[0] = '\0';
	strcat( path, docroot );
	if( docroot[ strlen( docroot ) -1 ] != '/' ) strcat( path, "/" );
	if( script[0] == '/' ) script++;
	strcat( path, script );

	char *p = strrchr( path, '/' );
	p[0] = 0;
	DaoVmSpace_SetPath( self->vmSpace, path );
	DaoVmSpace_SetPath( vmMaster, path );
	p[0] = '/';

	DaoRoutine *rout = getRoutine( path );
	ClientData *data = getClientData( remote );
	PrepareHttpVariables( data, & self->request );

	strcpy( self->signature, remote );
	strcat( self->signature, path );
	printf("%s\n", self->signature );
	if( data->seeStop ){
		int i;
		for( i=0; i<thdCount; i++){
			RequestHandler *p = engines[i];
			//XXX if( this != p && strcmp( self->signature, p->self->signature ) == 0 ) p->stop();
		}
	}

	// They must be set after FCGX_Accept_r() !!!
	DaoVmSpace_SetStdReader( self->vmSpace, & self->stdReader );
	DaoVmSpace_SetStdWriter( self->vmSpace, & self->stdWriter );
	//self->vmSpace->setEventHandler( eventHandler );
	
	DaoVmSpace_SetNameSpace( self->vmSpace, data->nsData );
	int bl = DaoVmSpace_ExecRoutine( self->vmSpace, rout, 0 );
	data->timestamp = time(NULL);
	DaoVmSpace_SetNameSpace( self->vmSpace, NULL );
	return bl;
}
*/
const char alnumChars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
void DaoCGI_RandomString( DaoContext *ctx, DValue *p[], int N )
{
	int len = p[0]->v.i;
	int alnum = p[1]->v.i;
	int i;
	DString *res = DaoContext_PutMBString( ctx, "" );
	if( alnum ){
		for(i=0; i<len; i++)
			DString_AppendChar( res, alnumChars[ (int)(62 * (rand()/(RAND_MAX+1.0)) ) ] );
	}else{
		for(i=0; i<len; i++)
			DString_AppendChar( res, (char)(255 * (rand()/(RAND_MAX+1.0))) );
	}
}
int DaoOnLoad( DaoVmSpace *vmSpace, DaoNameSpace *ns )
{
	DaoMap *httpENV, *httpGET, *httpPOST, *httpFILE, *httpCOOKIE;
	DaoMap *httpGETS, *httpPOSTS;
	srand( time(NULL) );

	vmMaster = vmSpace;
	DaoVmSpace_SetOptions( vmSpace, 
			//DaoVmSpace_GetExecOptions( vmSpace ) | DAO_EXEC_SAFE_EVAL | DAO_EXEC_MBS_ONLY );
			DaoVmSpace_GetOptions( vmSpace ) | DAO_EXEC_MBS_ONLY );

	DaoNameSpace_WrapFunction( ns, DaoCGI_RandomString, "random_string( n : int, alnum=1 )" );

	httpENV = DaoMap_New(1);
	httpGET = DaoMap_New(1);
	httpPOST = DaoMap_New(1);
	httpFILE = DaoMap_New(1);
	httpCOOKIE = DaoMap_New(1);
	httpGETS = DaoMap_New(1);
	httpPOSTS = DaoMap_New(1);

	DaoNameSpace_AddData( ns, "HTTP_ENV", (DaoBase*) httpENV, "map<string,string>" );
	DaoNameSpace_AddData( ns, "HTTP_GET", (DaoBase*) httpGET, "map<string,string>" );
	DaoNameSpace_AddData( ns, "HTTP_POST", (DaoBase*) httpPOST, "map<string,string>" );
	DaoNameSpace_AddData( ns, "HTTP_FILE", (DaoBase*) httpFILE, "map<string,stream>" );
	DaoNameSpace_AddData( ns, "HTTP_COOKIE", (DaoBase*)httpCOOKIE, "map<string,string>");
	DaoNameSpace_AddData(ns,"HTTP_GETS",(DaoBase*)httpGETS,"map<string,list<string> >");
	DaoNameSpace_AddData(ns,"HTTP_POSTS",(DaoBase*)httpPOSTS,"map<string,list<string> >");

	// Prepare HTTP_ENV:
	ParseKeyValueStringArray( httpENV, environ );
	
	// Prepare HTTP_GET:
	char *query = getenv( "QUERY_STRING" );
	if( query ) ParseKeyValueString( httpGETS, httpGET, query );
	query = getenv( "HTTP_COOKIE" );
	if( query ) ParseKeyValueString( NULL, httpCOOKIE, query );

	PreparePostData( httpPOSTS, httpPOST, httpFILE );
	return 0;
}


