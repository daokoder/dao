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
#include"time.h"
#include"math.h"

#ifdef DAO_WITH_NETWORK

#ifdef UNIX

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>

#elif WIN32

#include"winsock.h"

typedef size_t socklen_t;

#endif

#include"daolib.h"
#include"daoString.h"
#include"daoMap.h"
#include"daoArray.h"
#include"daoType.h"
#include"daoNumtype.h"
#include"daoThread.h"
#include"daoStdlib.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoSched.h"
#include"daoVmspace.h"

#define BACKLOG 1000 /*  how many pending connections queue will hold */
#define MAX_DATA 512 /*  max number of bytes we can get at once */

/* types */
enum DaoProxyProtocol
{
	DPP_TRANS_DATA = 0,
	DPP_TRANS_END  = 1,
	DPP_NULL
};

typedef struct DaoDataPacket
{
	char  type;
	char  tag; /* for string and numarray */
	short size;
	int   dataI1; /* for string and numarray */
	int   dataI2; /* for numarray */
	char  data[ MAX_DATA + 100 ];
} DaoDataPacket;

static int offset = (char*) ( & ((DaoDataPacket*)0)->data ) - (char*) ( & ((DaoDataPacket*)0)->type );

#define GET_FDSET( p )  ((fd_set*) p->v.cdata->data)
static void DaoFdSet_Zero( DaoContext *ctx, DValue *par[], int N  )
{
	FD_ZERO( GET_FDSET( par[0] ) );
}
static void DaoFdSet_Set( DaoContext *ctx, DValue *par[], int N  )
{
	FD_SET( par[1]->v.i, GET_FDSET( par[0] ) );
}
static void DaoFdSet_Clear( DaoContext *ctx, DValue *par[], int N  )
{
	FD_CLR( par[1]->v.i, GET_FDSET( par[0] ) );
}
static void DaoFdSet_IsSet( DaoContext *ctx, DValue *par[], int N  )
{
	DaoContext_PutInteger( ctx, FD_ISSET( par[1]->v.i, GET_FDSET( par[0] ) ) );
}
extern DaoTypeBase DaoFdSet_Typer;
static void DaoFdSet_New( DaoContext *ctx, DValue *par[], int N  )
{
	fd_set *set = dao_malloc( sizeof(fd_set) );
	FD_ZERO( set );
	DaoContext_PutCData( ctx, set, & DaoFdSet_Typer );
}
static DaoFuncItem fdsetMeths[] =
{
	{  DaoFdSet_New,        "fd_set()=>fd_set" },
	{  DaoFdSet_Zero,       "zero( self : fd_set )" },
	{  DaoFdSet_Set,        "set( self : fd_set, fd : int )" },
	{  DaoFdSet_Clear,      "clear( self : fd_set, fd : int )" },
	{  DaoFdSet_IsSet,      "isset( self : fd_set, fd : int )=>int" },
	{ NULL, NULL }
};
DaoTypeBase DaoFdSet_Typer = 
{ "fd_set", NULL, NULL, fdsetMeths, {0}, (FuncPtrDel)free, NULL };

void DaoNetwork_Close( int sockfd );
int DaoNetwork_Bind( int port )
{
	struct sockaddr_in myaddr;    /*  my address information */
	int yes = 0;
	int sockfd = socket( AF_INET, SOCK_STREAM, 0);
	if( sockfd == -1 ) return -1;
	if( setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(int) ) == -1 ) return -1;

	myaddr.sin_family = AF_INET;         /*  host byte order */
	myaddr.sin_port = htons( port );     /*  short, network byte order */
	myaddr.sin_addr.s_addr = INADDR_ANY; /*  automatically fill with my IP */
	memset( &( myaddr.sin_zero ), '\0', 8); /*  zero the rest of the struct */

	if( bind( sockfd, (struct sockaddr *) & myaddr, sizeof(struct sockaddr) ) == -1){
		close(sockfd);
		return -1;
	}
	return sockfd;
}
int DaoNetwork_Listen( int sockfd, int backlog )
{
	return listen( sockfd, backlog );
}
int DaoNetwork_Accept( int sockfd )
{
	return accept( sockfd, NULL, NULL );
}
int DaoNetwork_Connect( const char *host, unsigned short port )
{
	int sockfd;
	struct sockaddr_in addr;
	struct hostent *he;
	if( ( he = gethostbyname( host )) == NULL) return -1;  /*  get the host info */
	if( ( sockfd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1) return -1;

	addr.sin_family = AF_INET;    /*  host byte order */
	addr.sin_port = htons( port );  /*  short, network byte order */
	addr.sin_addr = *(struct in_addr *)he->h_addr;

	/* printf( "DaoNetwork_Connect() : %s, %i %i\n", host, port, sockfd ); */
	if( connect( sockfd, (struct sockaddr *)& addr, sizeof(struct sockaddr)) == -1){
		DaoNetwork_Close( sockfd );
		/* printf( "DaoNetwork_Connect() failed: %s, %i %i\n", host, port, sockfd ); */
		/* perror("connect"); */
		return -1;
	}
	return sockfd;
}
int DaoNetwork_Send( int sockfd, DString *buf )
{
	return send( sockfd, DString_GetMBS( buf ), DString_Size( buf ), 0);
}
int DaoNetwork_Receive( int sockfd, DString *buf, int max )
{
	int numbytes;
	if( max <=0 || max >= 1E4 ) max = 1E4;
	DString_ToMBS( buf );
	DString_Resize( buf, max );
	numbytes = recv( sockfd, (char*)DString_GetMBS( buf ), max, 0 );
	/*if( numbytes >=0 ) DString_Resize( buf, numbytes );*/
	if( numbytes >=0 ) buf->size = numbytes;
	return numbytes;
}
void DaoNetwork_Close( int sockfd )
{
#ifdef UNIX
	close( sockfd );
#elif WIN32
	closesocket( sockfd );
#endif
}

#define RADIX 32
static const char *mydigits = "0123456789abcdefghijklmnopqrstuvw";

void DaoPrintNumber32( char *buf, double value )
{
	int expon, digit;
	double prod, frac;
	char *p = buf;
	if( value <0.0 ){
		p[0] = '-';
		p ++;
		value = -value;
	}
	frac = frexp( value, & expon );
	/* printf( "DaoPrintNumber32: frac = %f %f\n", frac, value ); */
	while(1){
		prod = frac * RADIX;
		digit = (int) prod;
		frac = prod - digit;
		sprintf( p, "%c", mydigits[ digit ] );
		p ++;
		if( frac <= 0 ) break;
	}
	sprintf( p, "#%i", expon );
	/* printf( "DaoPrintNumber32: %s, %g\n", buf, value ); */
	return;
}
double DaoParseNumber32( char *buf )
{
	double frac = 0;
	int expon, sign = 1;
	char *p = buf;
	double factor = 1.0 / RADIX;
	double accum = factor;
	if( buf[0] == '-' ){
		p ++;
		sign = -1;
	}
	/* printf( "DaoParseNumber32: %s\n", buf ); */
	while( *p && *p != '#' ){
		int digit = *p;
		digit -= digit >= 'a' ? 'a' - 10 : '0';
		frac += accum * digit;
		accum *= factor;
		p ++;
	}
	expon = strtol( p+1, NULL, 10 );
	/* printf( "DaoParseNumber32: %f %f %f %s\n", frac, accum, ldexp( frac, expon ), p+1 ); */
	return ldexp( frac, expon ) * sign;
}

static int DaoNetwork_SendTag( int sockfd, char tag )
{
	DaoDataPacket packet;
	int length;
	length = offset + 1;
	packet.type = 0;
	packet.tag = tag;
	packet.size = htons( length );
	packet.dataI1 = packet.dataI2 = 0;
	send( sockfd, (char*)&packet, length, 0);
	return 1; /*XXX*/
}
static int DaoNetwork_SendNil( int sockfd )
{
	return DaoNetwork_SendTag( sockfd, DPP_TRANS_END );
}
static int DaoNetwork_SendInteger( int sockfd, int value )
{
	DaoDataPacket packet;
	int length;
	packet.tag = 0;
	packet.dataI1 = packet.dataI2 = 0;
	DaoPrintNumber32( packet.data, value );
	length = offset + strlen( packet.data ) + 1;
	/* printf( "send number: %i, %s\n", length, packet.data ); */
	packet.type = DAO_INTEGER;
	packet.size = htons( length );
	send( sockfd, (char*)&packet, length, 0);
	return 1;
}
static int DaoNetwork_SendFloat( int sockfd, float value )
{
	DaoDataPacket packet;
	int length;
	packet.tag = 0;
	packet.dataI1 = packet.dataI2 = 0;
	DaoPrintNumber32( packet.data, value );
	length = offset + strlen( packet.data ) + 1;
	packet.type = DAO_FLOAT;
	packet.size = htons( length );
	send( sockfd, (char*)&packet, length, 0);
	return 1;
}
static int DaoNetwork_SendNumber( int sockfd, DValue data )
{
	if( data.t == DAO_INTEGER )
		return DaoNetwork_SendInteger( sockfd, data.v.i );
	else if( data.t == DAO_FLOAT )
		return DaoNetwork_SendFloat( sockfd, data.v.f );
	else
		return DaoNetwork_SendFloat( sockfd, data.v.d );
}
static int DaoNetwork_SendChars( int sockfd, const char *mbs, int len )
{
	DaoDataPacket packet;
	int length = 0;
	int shift = 0;

	packet.type = DAO_STRING;
	packet.dataI1 = packet.dataI2 = 0;
	while( shift < len ){
		int copy = ( len - shift ) >= MAX_DATA ? MAX_DATA : ( len - shift );
		/* tag: 0, new string; 1, cat string; 2, finish string: */
		packet.tag = shift == 0 ? 0 : ( ( len - shift ) > MAX_DATA ? 1 : 2 );
		length = copy + offset + 1;
		strncpy( packet.data, mbs + shift, copy );
		packet.data[copy] = 0;
		packet.size = htons( length );
		send( sockfd, (char*) &packet, length, 0);
		shift += copy;
	}
	return 1;
}
static int DaoNetwork_SendString( int sockfd, DString *str )
{
	DString_ToMBS( str );
	return DaoNetwork_SendChars( sockfd, str->mbs, str->size );
}
static int DaoNetwork_SendComplex( int sockfd, complex16 data )
{
	DaoDataPacket packet;
	int len, length = offset;
	char *buf2 = packet.data;
	packet.tag = 0;
	packet.dataI1 = packet.dataI2 = 0;
	packet.type = DAO_COMPLEX;
	DaoPrintNumber32( buf2, data.real );
	len = strlen( buf2 ) + 1;
	length += len;
	buf2 += len;
	DaoPrintNumber32( buf2, data.imag );
	length += strlen( buf2 );
	packet.size = htons( length );
	send( sockfd, (char*) &packet, length, 0);
	return 1;
}
#ifdef DAO_WITH_NUMARRAY
static int DaoNetwork_SendArray( int sockfd, DaoArray *data )
{
	DaoDataPacket packet;
	char *buf2 = packet.data;
	int length = 0;
	int numtype = DaoArray_NumType( data );
	int M = DaoArray_Size( data );
	int j, len;

	packet.type = data->type;
	packet.tag = numtype;
	packet.dataI1 = htonl( M );
	packet.dataI2 = htonl( 0 );
	if( numtype == DAO_INTEGER ){
		int *vec = DaoArray_ToInt( data );
		for(j=0; j<M; j++){
			DaoPrintNumber32( buf2, vec[j] );
			len = strlen( buf2 ) + 1;
			length += len;
			buf2 += len;
			if( length >= MAX_DATA ){
				packet.size = htons( offset + length );
				send( sockfd, (char*) &packet, offset + length, 0);
				length = 0;
				buf2 = packet.data;
				packet.dataI2 = htonl( j+1 );
			}
		}
	}else if( numtype == DAO_FLOAT ){
		float *vec = DaoArray_ToFloat( data );
		for(j=0; j<M; j++){
			DaoPrintNumber32( buf2, vec[j] );
			len = strlen( buf2 ) + 1;
			length += len;
			buf2 += len;
			if( length >= MAX_DATA ){
				packet.size = htons( offset + length );
				send( sockfd, (char*) &packet, offset + length, 0);
				length = 0;
				buf2 = packet.data;
				packet.dataI2 = htonl( j+1 );
			}
		}
	}else{
		double *vec = DaoArray_ToDouble( data );
		if( numtype == DAO_COMPLEX ) M += M;
		for(j=0; j<M; j++){
			DaoPrintNumber32( buf2, vec[j] );
			len = strlen( buf2 ) + 1;
			length += len;
			buf2 += len;
			if( length >= MAX_DATA ){
				packet.size = htons( offset + length );
				send( sockfd, (char*) &packet, offset + length, 0);
				length = 0;
				buf2 = packet.data;
				packet.dataI2 = htonl( j+1 );
			}
		}
	}
	packet.size = htons( offset + length );
	send( sockfd, (char*) &packet, offset + length, 0);
	return 1;
}
#endif
int DaoNetwork_SendExt( int sockfd, DValue *data[], int size )
{
	int i;
	for( i=0; i<size; i++ ){
		DValue item = *data[i];
		switch( item.t ){
		case DAO_INTEGER :
		case DAO_FLOAT :
		case DAO_DOUBLE :
			DaoNetwork_SendNumber( sockfd, item );
			break;
		case DAO_STRING :
			DaoNetwork_SendString( sockfd, item.v.s );
			break;
		case DAO_COMPLEX :
			DaoNetwork_SendComplex( sockfd, * item.v.c );
			break;
#ifdef DAO_WITH_NUMARRAY
		case DAO_ARRAY :
			DaoNetwork_SendArray( sockfd, item.v.array );
			break;
#endif
		default : break;
		}
	}
	return 1;
}
int DaoNetwork_ReceiveExt( int sockfd, DaoList *data )
{
	int i, j, numbytes, start, size, M;
	char buf[ MAX_DATA + MAX_DATA + 100];
	char *buf2 = buf;
	short numtype;
	complex16  com;
	DValue item = daoNullValue;
	DString  *str = DString_New(1);
	DaoArray *arr = NULL;
	float *fv = NULL;
	double *dv = NULL;
	DaoDataPacket *inpack;
	int dpp = 0, count = 0;
	char bufin[ MAX_DATA + MAX_DATA + 2 ];

	/* printf( "receive3 : offset = %i\n", offset ); */
	numbytes = recv( sockfd, bufin, MAX_DATA, 0 );
	while( numbytes >0 ){
		bufin[ numbytes ] = '\0';
		start = 0;
		count = 1;
		/* printf( "DaoProxy_Receive: %i\n", bufin[0] ); */
		while( start < numbytes ){
			inpack = (DaoDataPacket*) (bufin+start);
			size = ntohs( inpack->size );
			/* printf( "chunk: %i, %i, %i, %i\n", inpack->type, numbytes, start, size ); */
			if( size > ( numbytes - start ) /* if the packet is not received completely: */
					|| ( start + offset >= numbytes ) /* ensure valid header */ ){
				numbytes = numbytes - start;
				memmove( bufin, bufin + start, numbytes );
				/* printf( "1 count = %i\n", count ); */
				count = recv( sockfd, bufin + numbytes, MAX_DATA, 0 );
				/* printf( "count = %i\n", count ); */
				if( count < 0 ) break;
				numbytes += count;
				bufin[ numbytes ] = '\0';
				start = 0;
				continue;
			}
			if( inpack->type == 0 && inpack->tag == DPP_TRANS_END ) break;
			if( inpack->type == 0 ){
				dpp = inpack->tag;
				start += size;
				continue;
			}
			buf2 = inpack->data;
			item.t = inpack->type;
			/* printf( "type = %i\n", inpack->type ); */
			switch( inpack->type ){
			case DAO_INTEGER :
				item.v.i = DaoParseNumber32( inpack->data );
				DaoList_Append( data, item );
				break;
			case DAO_FLOAT :
				item.v.f = DaoParseNumber32( inpack->data );
				/* printf( "number: %s %g\n", inpack->data, num->item.f ); */
				DaoList_Append( data, item );
				break;
			case DAO_DOUBLE :
				item.v.d = DaoParseNumber32( inpack->data );
				DaoList_Append( data, item );
				break;
			case DAO_STRING :
				item.v.s = str;
				if( inpack->tag == 0 ) DString_Clear( str );
				DString_AppendMBS( str, inpack->data );
				/* printf( "string: %s\n", inpack->data ); */
				if( inpack->tag ==2 || size <= MAX_DATA ) DaoList_Append( data, item );
				break;
			case DAO_COMPLEX :
				com.real = DaoParseNumber32( buf2 );
				while( *buf2 ) buf2 ++;
				com.imag = DaoParseNumber32( buf2+1 );
				item.v.c = & com;
				DaoList_Append( data, item );
				break;
#ifdef DAO_WITH_NUMARRAY
			case DAO_ARRAY :
				numtype = inpack->tag;
				M = ntohl( inpack->dataI1 );
				j = ntohl( inpack->dataI2 );
				/* printf( "ARRAY: M=%i; j=%i\n", M, j ); */
				if( j == 0 ){
					arr = DaoArray_New( DAO_INTEGER );
					item.v.array = arr;
					DaoArray_SetNumType( arr, numtype );
					DaoArray_ResizeVector( arr, M );
					DaoList_Append( data, item );
				}
				if( numtype == DAO_INTEGER ){
					int *iv = DaoArray_ToInt( arr );
					for(i=j; i<M; i++){
						iv[i] = DaoParseNumber32( buf2 );
						while( *buf2 ) buf2 ++;
						buf2 ++;
						if( ( buf2 - inpack->data ) >= MAX_DATA ) break;
					}
				}else if( numtype == DAO_FLOAT ){
					fv = DaoArray_ToFloat( arr );
					for(i=j; i<M; i++){
						fv[i] = DaoParseNumber32( buf2 );
						while( *buf2 ) buf2 ++;
						buf2 ++;
						if( ( buf2 - inpack->data ) >= MAX_DATA ) break;
					}
				}else{
					dv = DaoArray_ToDouble( arr );
					if( numtype == DAO_COMPLEX ) M += M;
					for(i=j; i<M; i++){
						dv[i] = DaoParseNumber32( buf2 );
						while( *buf2 ) buf2 ++;
						buf2 ++;
						if( ( buf2 - inpack->data ) >= MAX_DATA ) break;
					}
				}
				break;
#endif
			default : break;
			}
			start += size;
		}
		if( inpack->type ==0 && inpack->tag == DPP_TRANS_END ) break;
		if( count <= 0 ) break;
		/* printf( "before %i %i\n", daoProxyPort, sockfd ); */
		numbytes = recv( sockfd, bufin, MAX_DATA, 0 );
		/* printf( "after %i %i\n", daoProxyPort, sockfd ); */
	}
	DString_Delete( str );
	return dpp;
}

static void DaoNetLib_Bind( DaoContext *ctx, DValue *par[], int N  )
{
	DaoContext_PutInteger( ctx, DaoNetwork_Bind( par[0]->v.i ) );
}
static void DaoNetLib_Listen( DaoContext *ctx, DValue *par[], int N  )
{
	DaoContext_PutInteger( ctx, DaoNetwork_Listen( par[0]->v.i, par[1]->v.i ) );
}
static void DaoNetLib_Accept( DaoContext *ctx, DValue *par[], int N  )
{
	DaoContext_PutInteger( ctx, DaoNetwork_Accept( par[0]->v.i ) );
}
static void DaoNetLib_Connect( DaoContext *ctx, DValue *p[], int N  )
{
	DaoContext_PutInteger( ctx, DaoNetwork_Connect( DString_GetMBS( p[0]->v.s ), p[1]->v.i ) );
}
static void DaoNetLib_Send( DaoContext *ctx, DValue *par[], int N  )
{
	DaoContext_PutInteger( ctx, DaoNetwork_Send( par[0]->v.i, par[1]->v.s ) );
}
static void DaoNetLib_Receive( DaoContext *ctx, DValue *par[], int N  )
{
	DString *mbs = DaoContext_PutMBString( ctx, "" );
	DaoNetwork_Receive( par[0]->v.i, mbs, par[1]->v.i );
}
static void DaoNetLib_SendDao( DaoContext *ctx, DValue *par[], int N  )
{
	DaoContext_PutInteger( ctx, DaoNetwork_SendExt( par[0]->v.i, par+1, N-1 ) );
}
static void DaoNetLib_ReceiveDao( DaoContext *ctx, DValue *par[], int N  )
{
	DaoList *res = DaoContext_PutList( ctx );
	DaoNetwork_ReceiveExt( par[0]->v.i, res );
}
#ifdef WIN32
#define ENOTSOCK WSAENOTSOCK
#define ENOTCONN WSAENOTCONN
#endif
static void DaoNetLib_GetPeerName( DaoContext *ctx, DValue *par[], int N  )
{
	struct sockaddr_in addr;
	socklen_t size = sizeof( struct sockaddr_in );
	int rt = getpeername( par[0]->v.i, (struct sockaddr *) & addr, & size );
	DString *res = DaoContext_PutMBString( ctx, "" );
	switch( rt ){
	case 0 : DString_SetMBS( res, inet_ntoa( addr.sin_addr ) ); break;
	case ENOTSOCK : DString_SetMBS( res, "ENOTSOCK" ); break;
	case ENOTCONN : DString_SetMBS( res, "ENOTCONN" ); break;
	default : DString_SetMBS( res, "EUNKNOWN" ); break;
	}
}
static void DaoNetLib_Close( DaoContext *ctx, DValue *par[], int N  )
{
	DaoNetwork_Close( par[0]->v.i );
}
static void DaoNetLib_GetHost( DaoContext *ctx, DValue *par[], int N  )
{
	struct hostent *hent;
	struct sockaddr_in addr;
	size_t size = sizeof( struct sockaddr_in );
	const char *host = DString_GetMBS( par[0]->v.s );
	DaoMap *res = DaoContext_PutMap( ctx );
	DString *str;
	DValue value = daoNullString;
	if( DString_Size( par[0]->v.s ) ==0 ) return;
	if( host[0] >= '0' && host[0] <= '9' ){
		struct in_addr id;
#ifdef UNIX
		if( inet_aton( host, & id ) == 0 ) return;
#elif WIN32
		id.s_addr = inet_addr( host );
#endif
		addr.sin_family = AF_INET; 
		addr.sin_addr = id;
		memset( &( addr.sin_zero ), '\0', 8);
		hent = gethostbyaddr( (void*) & addr, size, AF_INET );
		if( hent == NULL ) return;
	}else{
		hent = gethostbyname( host );
	}
	str = value.v.s = DString_New(1);
	if( hent->h_addrtype == AF_INET ){
		char **p = hent->h_aliases;
		char **q = hent->h_addr_list;
		while( *p ){
			DString_SetMBS( str, inet_ntoa( *(struct in_addr*) (*q) ) );
			DaoMap_InsertMBS( res, *p, value );
			p ++;
			q ++;
		}
	}else{ /* AF_INET6 */
	}
	DString_Delete( str );
}
static void DaoNetLib_Select( DaoContext *ctx, DValue *par[], int N  )
{
	struct timeval tv;
	tv.tv_sec = (int)par[4]->v.f;
	tv.tv_usec = ( par[4]->v.f - tv.tv_sec ) * 1E6;
	DaoContext_PutInteger( ctx, select( par[0]->v.i, GET_FDSET( par[1] ),
				GET_FDSET( par[2] ), GET_FDSET( par[3] ), & tv ) );
}

static DaoFuncItem netMeths[] =
{
	{  DaoNetLib_Bind,          "bind( port :int )=>int" },
	{  DaoNetLib_Listen,        "listen( socket :int, backlog=10 )=>int" },
	{  DaoNetLib_Accept,        "accept( socket :int )=>int" },
	{  DaoNetLib_Connect,       "connect( host :string, port :int )=>int" },
	{  DaoNetLib_Send,          "send( socket :int, data :string )=>int" },
	{  DaoNetLib_Receive,       "receive( socket :int, maxlen=512 )=>string" },
	{  DaoNetLib_SendDao,       "send_dao( socket :int, ... )=>int" },
	{  DaoNetLib_ReceiveDao,    "receive_dao( socket :int )=>list<any>" },
	{  DaoNetLib_GetPeerName,   "getpeername( socket :int )=>string" },
	{  DaoNetLib_Close,         "close( socket :int )" },
	{  DaoNetLib_GetHost,       "gethost( host :string )=>map<string,string>" },
	{  DaoNetLib_Select,
		"select( nfd :int, setr :fd_set, setw :fd_set, sete :fd_set, tv :float )=>int" },
	{ NULL, NULL }
};

static DaoTypeCore netCore =
{
	0, NULL, NULL, NULL, NULL,
	DaoBase_GetField,
	DaoBase_SetField,
	DaoBase_GetItem,
	DaoBase_SetItem,
	DaoBase_Print,
	DaoBase_Copy,
};
DaoTypeBase libNetTyper = {
	"network", NULL, NULL, netMeths, {0}, NULL, NULL
};

void DaoNetwork_Init( DaoVmSpace *vms, DaoNameSpace *ns )
{

#ifdef WIN32
	WSADATA wsaData;   /*  if this doesn't work */
	/* WSAData wsaData; // then try this instead */

	if(WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
		fprintf(stderr, "WSAStartup failed.\n");
		exit(1);
	} 
#endif

}

#endif
