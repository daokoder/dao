/*=========================================================================================
  This file is a part of the Dao standard modules.
  Copyright (C) 2011, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#include"time.h"
#include"math.h"
#include"string.h"
#include"locale.h"
#include"stdlib.h"

#ifdef UNIX
#include<unistd.h>
#include<sys/time.h>
#endif

#ifdef _MSC_VER
#define putenv _putenv
#endif

#include"daoString.h"
#include"daoValue.h"

DAO_INIT_MODULE

static void SYS_Ctime( DaoProcess *proc, DaoValue *p[], int N )
{
	struct tm *ctime;
	time_t t = (time_t)p[0]->xInteger.value;
	DaoTuple *tuple = DaoTuple_New( 7 );
	DaoValue **items = tuple->items;
	if( t == 0 ) t = time(NULL);
	ctime = gmtime( & t );
	items[0] = DaoValue_NewInteger( ctime->tm_year + 1900 );
	items[1] = DaoValue_NewInteger( ctime->tm_mon + 1 );
	items[2] = DaoValue_NewInteger( ctime->tm_mday );
	items[3] = DaoValue_NewInteger( ctime->tm_wday + 1 );
	items[4] = DaoValue_NewInteger( ctime->tm_hour );
	items[5] = DaoValue_NewInteger( ctime->tm_min );
	items[6] = DaoValue_NewInteger( ctime->tm_sec );
	DaoProcess_PutValue( proc, (DaoValue*) tuple );
}
static int addStringFromMap( DaoValue *self, DString *S, DaoMap *sym, const char *key, int id )
{
	DNode *node;

	if( S==NULL || sym==NULL ) return 0;
	DString_SetMBS( self->xString.data, key );
	node = DMap_Find( sym->items, & self );
	if( node ){
		DaoList *list = & node->value.pValue->xList;
		if( list->type == DAO_LIST && list->items.size > id ){
			DaoValue *p = list->items.items.pValue[ id ];
			if( p->type == DAO_STRING ){
				DString_Append( S, p->xString.data );
				return 1;
			}
		}
	}
	return 0;
}
static void SYS_Ctimef( DaoProcess *proc, DaoValue *p[], int N )
{
	int  i;
	int halfday = 0;
	const int size = p[1]->xString.data->size;
	const char *format = DString_GetMBS( p[1]->xString.data );
	char buf[100];
	char *p1 = buf+1;
	char *p2;
	DaoMap *sym = NULL;
	DaoString *ds = DaoString_New(1);;
	DaoValue *key = (DaoValue*) ds;
	DString *S;

	struct tm *ctime;
	time_t t = (time_t)p[0]->xInteger.value;
	if( t == 0 ) t = time(NULL);
	ctime = gmtime( & t );

	if( N > 1 ){
		sym = (DaoMap*)p[2];
		if( sym->items->size == 0 ) sym = NULL;
	}
	S = DaoProcess_PutMBString( proc, "" );

	for( i=0; i+1<size; i++ ){
		if( format[i] == '%' && ( format[i+1] == 'a' || format[i+1] == 'A' ) ){
			halfday = 1;
			break;
		}
	}
	buf[0] = '0'; /* for padding */

	for( i=0; i+1<size; i++ ){
		p2 = p1;
		p1[0] = 0;
		if( format[i] == '%' ){
			const char ch = format[i+1];
			switch( ch ){
			case 'Y' :
				sprintf( p1, "%i", ctime->tm_year+1900 );
				break;
			case 'y' :
				sprintf( p1, "%i", ctime->tm_year+1900 );
				p2 += 2;
				break;
			case 'M' :
			case 'm' :
				if( ! addStringFromMap( key, S, sym, "month", ctime->tm_mon ) ){
					sprintf( p1, "%i", ctime->tm_mon+1 );
					if( ch=='M' && p1[1]==0 ) p2 = buf; /* padding 0; */
				}else p2 = NULL;
				break;
			case 'D' :
			case 'd' :
				if( ! addStringFromMap( key, S, sym, "date", ctime->tm_mday ) ){
					sprintf( p1, "%i", ctime->tm_mday );
					if( ch=='D' && p1[1]==0 ) p2 = buf; /* padding 0; */
				}else p2 = NULL;
				break;
			case 'W' :
			case 'w' :
				if( ! addStringFromMap( key, S, sym, "week", ctime->tm_wday ) )
					sprintf( p1, "%i", ctime->tm_wday+1 );
				else p2 = NULL;
				break;
			case 'H' :
			case 'h' :
				if( halfday )
					sprintf( p1, "%i", ctime->tm_hour %12 );
				else
					sprintf( p1, "%i", ctime->tm_hour );
				if( ch=='H' && p1[1]==0 ) p2 = buf; /* padding 0; */
				break;
			case 'I' :
			case 'i' :
				sprintf( p1, "%i", ctime->tm_min );
				if( ch=='I' && p1[1]==0 ) p2 = buf; /* padding 0; */
				break;
			case 'S' :
			case 's' :
				sprintf( p1, "%i", ctime->tm_sec );
				if( ch=='S' && p1[1]==0 ) p2 = buf; /* padding 0; */
				break;
			case 'a' :
				if( ! addStringFromMap( key, S, sym, "halfday", 0 ) ){
					if( ctime->tm_hour >= 12 ) strcpy( p1, "pm" );
					else strcpy( p1, "am" );
				}else p2 = NULL;
				break;
			case 'A' :
				if( ! addStringFromMap( key, S, sym, "halfday", 1 ) ){
					if( ctime->tm_hour >= 12 ) strcpy( p1, "PM" );
					else strcpy( p1, "AM" );
				}else p2 = NULL;
				break;
			default : break;
			}
			if( p2 ) DString_AppendMBS( S, p2 );
			i ++;
		}else{
			DString_AppendChar( S, format[i] );
		}
	}
	if( i+1 == size ) DString_AppendChar( S, format[i] );
	DaoString_Delete( ds );
}
static void SYS_Sleep( DaoProcess *proc, DaoValue *p[], int N )
{
#ifdef DAO_WITH_THREAD
	DMutex    mutex;
	DCondVar  condv;
#endif

	double s = p[0]->xFloat.value;
	if( s < 0 ){
		DaoProcess_RaiseException( proc, DAO_WARNING_VALUE, "expecting positive value" );
		return;
	}
#ifdef DAO_WITH_THREAD
	/* sleep only the current thread: */
	DMutex_Init( & mutex );
	DCondVar_Init( & condv );
	DMutex_Lock( & mutex );
	DCondVar_TimedWait( & condv, & mutex, s );
	DMutex_Unlock( & mutex );
	DMutex_Destroy( & mutex );
	DCondVar_Destroy( & condv );
#elif UNIX
	sleep( (int)s ); /* This may cause the whole process to sleep. */
#else
	Sleep( s * 1000 );
#endif
}
static void SYS_Exit( DaoProcess *proc, DaoValue *p[], int N )
{
	exit( (int)p[0]->xInteger.value );
}
static void SYS_Shell( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutInteger( proc, system( DString_GetMBS( p[0]->xString.data ) ) );
}
static void SYS_Time( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutInteger( proc, time( NULL ) );
}
static void SYS_Time2( DaoProcess *proc, DaoValue *p[], int N )
{
	/* extern long timezone; */
	/* extern int daylight; // not on WIN32 */
	struct tm ctime;
	DaoValue **tup = p[0]->xTuple.items;
	memset( & ctime, 0, sizeof( struct tm ) );
	ctime.tm_year = tup[0]->xInteger.value - 1900;
	ctime.tm_mon = tup[1]->xInteger.value - 1;
	ctime.tm_mday = tup[2]->xInteger.value;
	ctime.tm_hour = tup[4]->xInteger.value;/* + daylight; */
	ctime.tm_min = tup[5]->xInteger.value;
	ctime.tm_sec = tup[6]->xInteger.value;
	ctime.tm_isdst = 0;
	DaoProcess_PutInteger( proc, (int) mktime( & ctime ) );
}
static void SYS_SetLocale( DaoProcess *proc, DaoValue *p[], int N )
{
	int category = 0;
	char* old;
	switch( p[0]->xEnum.value ){
	case 0: category = LC_ALL; break;
	case 1: category = LC_COLLATE; break;
	case 2: category = LC_CTYPE; break;
	case 3: category = LC_MONETARY; break;
	case 4: category = LC_NUMERIC; break;
	case 5: category = LC_TIME; break;
	}
	old = setlocale( category, DString_GetMBS( p[1]->xString.data ) );
	if ( old )
		DaoProcess_PutMBString( proc, old );
	else
		DaoProcess_RaiseException( proc, DAO_ERROR, "invalid locale" );
}
static void SYS_Clock( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutFloat( proc, ((float)clock())/CLOCKS_PER_SEC );
}
static void SYS_GetEnv( DaoProcess *proc, DaoValue *p[], int N )
{
	char *evar = getenv( DString_GetMBS( p[0]->xString.data ) );
	DaoProcess_PutMBString( proc, evar? evar : "" );
}
static void SYS_PutEnv( DaoProcess *proc, DaoValue *p[], int N )
{
	char *name = DString_GetMBS( p[0]->xString.data );
	char *value = DString_GetMBS( p[1]->xString.data );
	char *buf = malloc( strlen( name ) + strlen( value ) + 2 );
	if( !buf ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "memory allocation failed" );
		return;
	}
	sprintf( buf, "%s=%s", name, value );
	if( putenv( buf ) ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "error putting environment variable" );
		free( buf );
	}
}

static DaoFuncItem sysMeths[]=
{
	{ SYS_Shell,     "shell( command :string )" },
	{ SYS_Sleep,     "sleep( seconds :float )" },
	{ SYS_Exit,      "exit( code=0 )" },
	{ SYS_Clock,     "clock()=>float" },
	{ SYS_Ctime,     "ctime( time=0 )=>tuple<year:int,month:int,day:int,wday:int,hour:int,minute:int,second:int>" },
	{ SYS_Ctimef,    "ctimef( time=0, format=\'%Y-%M-%D, %H:%I:%S\', "
		"namemap : map<string,list<string>> = {=>} )=>string" },
	{ SYS_Time,      "time(  )=>int" },
	{ SYS_Time2,     "time( tm : tuple<year:int,month:int,day:int,wday:int,hour:int,minute:int,second:int> )=>int" },
	{ SYS_SetLocale,
		"setlocale( category: enum<all,collate,ctype,monetary,numeric,time> = $all, locale = '' )=>string" },
	{ SYS_GetEnv,    "getenv( name: string )=>string" },
	{ SYS_PutEnv,    "putenv( name: string, value = '' )"},
	{ NULL, NULL }
};

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace_WrapFunctions( ns, sysMeths );
	return 0;
}
