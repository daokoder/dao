/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

/* Contribution logs: */
/* 2011-02-11, Aleksey Danilov: added socket type with OOP-style methods; */
/* 2011-02-13, Aleksey Danilov: added socket error handling; */

#include"stdio.h"
#include"stdlib.h"
#include"string.h"
#include"time.h"
#include"math.h"
#include"errno.h"

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
#define fileno _fileno

#endif

#include"dao.h"

DAO_INIT_MODULE

#define dao_malloc malloc
#define dao_free free

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


static void GetErrorMessage( char *buffer, int code )
{
	switch( code ){
#ifdef WIN32
	case WSAENETDOWN:            strcpy( buffer, "The network subsystem has failed (WSAENETDOWN)" ); break;
	case WSAEAFNOSUPPORT:        strcpy( buffer, "The address family is not supported (WSAEAFNOSUPPORT)" ); break;
	case WSAEINPROGRESS:         strcpy( buffer, "The service provider is processing another call (WSAEINPROGRESS)" ); break;
	case WSAEMFILE:              strcpy( buffer, "No more file descriptors available (WSAEMFILE)" ); break;
	case WSAENOBUFS:             strcpy( buffer, "Insufficient buffer space (WSAENOBUFS)" ); break;
	case WSAEPROTONOSUPPORT:     strcpy( buffer, "The protocol is not supported (WSAEPROTONOSUPPORT)" ); break;
	case WSAEPROVIDERFAILEDINIT: strcpy( buffer, "The service provider failed to initialize (WSAEPROVIDERFAILEDINIT)" ); break;
	case WSAECONNRESET:          strcpy( buffer, "The connection was terminated by the remote side (WSAECONNRESET)" ); break;
	case WSAEADDRNOTAVAIL:       strcpy( buffer, "The address is not available (WSAEADDRNOTAVAIL)" ); break;
	case WSAECONNREFUSED:        strcpy( buffer, "The connection was refused (WSAECONNREFUSED)" ); break;
	case WSAENETUNREACH:         strcpy( buffer, "The network is not reachable (WSAENETUNREACH)" ); break;
	case WSAEHOSTUNREACH:        strcpy( buffer, "The host is not reachable (WSAEHOSTUNREACH)" ); break;
	case WSAETIMEDOUT:           strcpy( buffer, "The attempt to establish the connection timed out (WSAETIMEDOUT)" ); break;
	case WSAENETRESET:           strcpy( buffer, "The connection was broken (WSAENETRESET)" ); break;
	case WSAEMSGSIZE:            strcpy( buffer, "The message is too large (WSAEMSGSIZE)" ); break;
	case WSAECONNABORTED:        strcpy( buffer, "The connection was terminated due to a time-out or other failure (WSAECONNABORTED)" ); break;
	case WSAEINVAL:              strcpy( buffer, "Invalid parameter (WSAEINVAL)" ); break;
	case WSAHOST_NOT_FOUND:      strcpy( buffer, "Authoritative answer host not found (WSAHOST_NOT_FOUND)" ); break;
	case WSATRY_AGAIN:           strcpy( buffer, "Nonauthoritative host not found, or no response from the server (WSATRY_AGAIN)" ); break;
	case WSANO_RECOVERY:         strcpy( buffer, "A non-recoverable error occurred (WSANO_RECOVERY)" ); break;
	case WSANO_DATA:             strcpy( buffer, "Address not found (WSANO_DATA)" ); break;
	case WSASYSNOTREADY:         strcpy( buffer, "The network subsystem is not ready (WSASYSNOTREADY)" ); break;
	case WSAVERNOTSUPPORTED:     strcpy( buffer, "The version is not supported (WSAVERNOTSUPPORTED)" ); break;
	case WSAEPROCLIM:            strcpy( buffer, "A limit on the number of tasks has been reached (WSAEPROCLIM)" ); break;
#else
	case EAFNOSUPPORT:    strcpy( buffer, "The address family is not supported (EAFNOSUPPORT)" ); break;
	case EMFILE:
	case ENFILE:          strcpy( buffer, "No more file descriptors available (EMFILE/ENFILE)" ); break;
	case EPROTONOSUPPORT: strcpy( buffer, "The protocol is not supported (EPROTONOSUPPORT)" ); break;
	case EACCES:          strcpy( buffer, "The process does not have the appropriate privileges (EACCES)" ); break;
	case ENOBUFS:         strcpy( buffer, "Insufficient buffer space (ENOBUFS)" ); break;
	case EADDRNOTAVAIL:   strcpy( buffer, "The address is not available (EADDRNOTAVAIL)" ); break;
	case ETIMEDOUT:       strcpy( buffer, "The attempt to establish the connection timed out (ETIMEDOUT)" ); break;
	case ECONNREFUSED:    strcpy( buffer, "The connection was refused (ECONNREFUSED)" ); break;
	case ENETUNREACH:     strcpy( buffer, "The network is not reachable (ENETUNREACH)" ); break;
	case EADDRINUSE:      strcpy( buffer, "The socket address is already in use (EADDRINUSE)" ); break;
	case EINTR:           strcpy( buffer, "The data sending was interrupted by a signal (EINTR)" ); break;
	case EMSGSIZE:        strcpy( buffer, "The message is too large (EMSGSIZE)" ); break;
	case EPIPE:           strcpy( buffer, "The connection was broken (EPIPE)" ); break;
	case EINVAL:          strcpy( buffer, "Invalid parameter (EINVAL)" ); break;
#endif
	default: sprintf( buffer, "Unknown system error (%x)", code );
	}
}

static void GetHostErrorMessage( char *buffer, int code )
{
#ifdef WIN32
	GetErrorMessage( buffer, code );
#else
	switch( code ){
	case HOST_NOT_FOUND: strcpy( buffer, "Host not found (HOST_NOT_FOUND)" ); break;
	case TRY_AGAIN:      strcpy( buffer, "No response from the server (TRY_AGAIN)" ); break;
	case NO_RECOVERY:    strcpy( buffer, "A non-recoverable error occurred (NO_RECOVERY)" ); break;
	case NO_ADDRESS:     strcpy( buffer, "Address not found (NO_ADDRESS)" ); break;
	default:             sprintf( buffer, "Unknown system error (%x)", code );
	}
#endif
}

static int GetError()
{
#ifdef WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

static int GetHostError()
{
#ifdef WIN32
	return WSAGetLastError();
#else
	return h_errno;
#endif
}

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
		DaoNetwork_Close(sockfd);
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

#ifdef WIN32
#define INTERRUPTED WSAEINTR
#else
#define INTERRUPTED EINTR
#endif

int LoopSend( int sockfd, char *buf, int size, int flags )
{
	int left = size;
	int numbytes;
	do{
		numbytes = send( sockfd, buf, left, flags );
		if(numbytes != -1){
			left -= numbytes;
			buf += numbytes;
		}
		else if( GetError() != INTERRUPTED )
			return -1;
	}
	while( left > 0 );
	return size;
}
int LoopReceive( int sockfd, char *buf, int size, int flags )
{
	int numbytes;
	do
		numbytes = recv( sockfd, buf, size, flags );
	while( numbytes == -1 && GetError() == INTERRUPTED );
	return numbytes;
}
int DaoNetwork_Send( int sockfd, DString *buf )
{
	return LoopSend( sockfd, DString_GetMBS( buf ), DString_Size( buf ), 0);
}
int DaoNetwork_Receive( int sockfd, DString *buf, int max )
{
	int numbytes;
	if( max <=0 || max >= 1E4 ) max = 1E4;
	DString_ToMBS( buf );
	DString_Resize( buf, max );
	numbytes = LoopReceive( sockfd, (char*)DString_GetMBS( buf ), max, 0 );
	if( numbytes >=0 ) DString_Resize( buf, numbytes );
	/* if( numbytes >=0 ) buf->size = numbytes; */
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

void DaoPrintNumber( char *buf, long double value )
{
	int expon, digit;
	long double prod, frac;
	char *p = buf;
	if( value <0.0 ){
		p[0] = '-';
		p ++;
		value = -value;
	}
	frac = frexp( value, & expon );
	/* printf( "DaoPrintNumber: frac = %Lf %Lf\n", frac, value ); */
	while(1){
		prod = frac * RADIX;
		digit = (int) prod;
		frac = prod - digit;
		sprintf( p, "%c", mydigits[ digit ] );
		p ++;
		if( frac <= 0 ) break;
	}
	sprintf( p, "#%i", expon );
	/* printf( "DaoPrintNumber: %s, %Lg\n", buf, value ); */
	return;
}
long double DaoParseNumber( char *buf )
{
	long double frac = 0;
	int expon, sign = 1;
	char *p = buf;
	long double factor = 1.0 / RADIX;
	long double accum = factor;
	if( buf[0] == '-' ){
		p ++;
		sign = -1;
	}
	/* printf( "DaoParseNumber: %s\n", buf ); */
	while( *p && *p != '#' ){
		int digit = *p;
		digit -= digit >= 'a' ? 'a' - 10 : '0';
		frac += accum * digit;
		accum *= factor;
		p ++;
	}
	expon = strtol( p+1, NULL, 10 );
	/* printf( "DaoParseNumber: %Lf %Lf %Lf %s\n", frac, accum, ldexp( frac, expon ), p+1 ); */
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
	return LoopSend( sockfd, (char*)&packet, length, 0);
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
	DaoPrintNumber( packet.data, value );
	length = offset + strlen( packet.data ) + 1;
	/* printf( "send number: %i, %s\n", length, packet.data ); */
	packet.type = DAO_INTEGER;
	packet.size = htons( length );
	return LoopSend( sockfd, (char*)&packet, length, 0);
}
static int DaoNetwork_SendFloat( int sockfd, float value )
{
	DaoDataPacket packet;
	int length;
	packet.tag = 0;
	packet.dataI1 = packet.dataI2 = 0;
	DaoPrintNumber( packet.data, value );
	length = offset + strlen( packet.data ) + 1;
	packet.type = DAO_FLOAT;
	packet.size = htons( length );
	return LoopSend( sockfd, (char*)&packet, length, 0);
}
static int DaoNetwork_SendNumber( int sockfd, DaoValue *data )
{
	if( DaoValue_Type( data ) == DAO_INTEGER )
		return DaoNetwork_SendInteger( sockfd, DaoValue_TryGetInteger( data ) );
	else if(  DaoValue_Type( data ) == DAO_FLOAT )
		return DaoNetwork_SendFloat( sockfd, DaoValue_TryGetFloat( data ) );
	else
		return DaoNetwork_SendFloat( sockfd, DaoValue_TryGetDouble( data ) );
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
		if( LoopSend( sockfd, (char*) &packet, length, 0) == -1 )
			return -1;
		shift += copy;
	}
	return 0;
}
static int DaoNetwork_SendString( int sockfd, DString *str )
{
	DString_ToMBS( str );
	return DaoNetwork_SendChars( sockfd, DString_GetMBS( str ), DString_Size( str ) );
}
static int DaoNetwork_SendComplex( int sockfd, complex16 data )
{
	DaoDataPacket packet;
	int len, length = offset;
	char *buf2 = packet.data;
	packet.tag = 0;
	packet.dataI1 = packet.dataI2 = 0;
	packet.type = DAO_COMPLEX;
	DaoPrintNumber( buf2, data.real );
	len = strlen( buf2 ) + 1;
	length += len;
	buf2 += len;
	DaoPrintNumber( buf2, data.imag );
	length += strlen( buf2 );
	packet.size = htons( length );
	return LoopSend( sockfd, (char*) &packet, length, 0);
}
static int DaoNetwork_SendArray( int sockfd, DaoArray *data )
{
	DaoDataPacket packet;
	char *buf2 = packet.data;
	int length = 0;
	int numtype = DaoArray_NumType( data );
	int M = DaoArray_Size( data );
	int j, len;

	packet.type = DAO_ARRAY;
	packet.tag = numtype;
	packet.dataI1 = htonl( M );
	packet.dataI2 = htonl( 0 );
	if( numtype == DAO_INTEGER ){
		dint *vec = DaoArray_ToInteger( data );
		for(j=0; j<M; j++){
			DaoPrintNumber( buf2, vec[j] );
			len = strlen( buf2 ) + 1;
			length += len;
			buf2 += len;
			if( length >= MAX_DATA ){
				packet.size = htons( offset + length );
				if( LoopSend( sockfd, (char*) &packet, offset + length, 0) == -1 )
					return -1;
				length = 0;
				buf2 = packet.data;
				packet.dataI2 = htonl( j+1 );
			}
		}
	}else if( numtype == DAO_FLOAT ){
		float *vec = DaoArray_ToFloat( data );
		for(j=0; j<M; j++){
			DaoPrintNumber( buf2, vec[j] );
			len = strlen( buf2 ) + 1;
			length += len;
			buf2 += len;
			if( length >= MAX_DATA ){
				packet.size = htons( offset + length );
				if( LoopSend( sockfd, (char*) &packet, offset + length, 0) == -1 )
					return -1;
				length = 0;
				buf2 = packet.data;
				packet.dataI2 = htonl( j+1 );
			}
		}
	}else{
		double *vec = DaoArray_ToDouble( data );
		if( numtype == DAO_COMPLEX ) M += M;
		for(j=0; j<M; j++){
			DaoPrintNumber( buf2, vec[j] );
			len = strlen( buf2 ) + 1;
			length += len;
			buf2 += len;
			if( length >= MAX_DATA ){
				packet.size = htons( offset + length );
				if( LoopSend( sockfd, (char*) &packet, offset + length, 0) == -1 )
					return -1;
				length = 0;
				buf2 = packet.data;
				packet.dataI2 = htonl( j+1 );
			}
		}
	}
	packet.size = htons( offset + length );
	return LoopSend( sockfd, (char*) &packet, offset + length, 0);
}
static int DaoNetwork_SendExt( DaoProcess *proc, int sockfd, DaoValue *data[], int size )
{
	int i;
	for( i=0; i<size; i++ ){
		DaoValue *item = data[i];
		switch( DaoValue_Type( item ) ){
		case DAO_INTEGER :
		case DAO_FLOAT :
		case DAO_DOUBLE :
			DaoNetwork_SendNumber( sockfd, item );
			break;
		case DAO_STRING :
			DaoNetwork_SendString( sockfd, DaoString_Get( DaoValue_CastString( item ) ) );
			break;
		case DAO_COMPLEX :
			DaoNetwork_SendComplex( sockfd, DaoComplex_Get( DaoValue_CastComplex( item ) ) );
			break;
		case DAO_ARRAY :
			DaoNetwork_SendArray( sockfd, DaoValue_CastArray( item ) );
			break;
		default : break;
		}
	}
	return 1;
}
int DaoNetwork_ReceiveExt( DaoProcess *proc, int sockfd, DaoList *data )
{
	DaoFactory *fac = DaoProcess_GetFactory( proc );
	int i, j, numbytes, start, size, M;
	char buf[ MAX_DATA + MAX_DATA + 100];
	char *buf2 = buf;
	short numtype;
	complex16  com;
	DaoArray *arr = NULL;
	DaoValue *item;
	DString  *str = DString_New(1);
	float *fv = NULL;
	double *dv = NULL;
	DaoDataPacket *inpack;
	int dpp = 0, count = 0;
	char bufin[ MAX_DATA + MAX_DATA + 2 ];

	/* printf( "receive3 : offset = %i\n", offset ); */
	numbytes = LoopReceive( sockfd, bufin, MAX_DATA, 0 );
	if( numbytes == -1 ){
		DString_Delete( str );
		return -1;
	}
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
				count = LoopReceive( sockfd, bufin + numbytes, MAX_DATA, 0 );
				/* printf( "count = %i\n", count ); */
				if( count < 0 ){
					DString_Delete( str );
					return -1;
				}
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
			/* printf( "type = %i\n", inpack->type ); */
			switch( inpack->type ){
			case DAO_INTEGER :
				item = (DaoValue*) DaoFactory_NewInteger( fac, DaoParseNumber( inpack->data ) );
				DaoList_PushBack( data, item );
				break;
			case DAO_FLOAT :
				item = (DaoValue*) DaoFactory_NewFloat( fac, DaoParseNumber( inpack->data ) );
				/* printf( "number: %s %g\n", inpack->data, num->item.f ); */
				DaoList_PushBack( data, item );
				break;
			case DAO_DOUBLE :
				item = (DaoValue*) DaoFactory_NewDouble( fac, DaoParseNumber( inpack->data ) );
				DaoList_PushBack( data, item );
				break;
			case DAO_STRING :
				if( inpack->tag == 0 ) DString_Clear( str );
				DString_AppendMBS( str, inpack->data );
				item = (DaoValue*) DaoFactory_NewMBString( fac, NULL, 0);
				DaoString_Set( DaoValue_CastString( item ), str );
				/* printf( "string: %s\n", inpack->data ); */
				if( inpack->tag ==2 || size <= MAX_DATA ) DaoList_PushBack( data, item );
				break;
			case DAO_COMPLEX :
				com.real = DaoParseNumber( buf2 );
				while( *buf2 ) buf2 ++;
				com.imag = DaoParseNumber( buf2+1 );
				item = (DaoValue*) DaoFactory_NewComplex( fac, com );
				DaoList_PushBack( data, item );
				break;
			case DAO_ARRAY :
				numtype = inpack->tag;
				M = ntohl( inpack->dataI1 );
				j = ntohl( inpack->dataI2 );
				/* printf( "ARRAY: M=%i; j=%i\n", M, j ); */
				if( j == 0 ){
					item = (DaoValue*) DaoFactory_NewArray( fac, DAO_INTEGER );
					arr = DaoValue_CastArray( item );
					DaoArray_SetNumType( arr, numtype );
					DaoArray_ResizeVector( arr, M );
					DaoList_PushBack( data, item );
				}
				if( numtype == DAO_INTEGER ){
					dint *iv = DaoArray_ToInteger( arr );
					for(i=j; i<M; i++){
						iv[i] = DaoParseNumber( buf2 );
						while( *buf2 ) buf2 ++;
						buf2 ++;
						if( ( buf2 - inpack->data ) >= MAX_DATA ) break;
					}
				}else if( numtype == DAO_FLOAT ){
					fv = DaoArray_ToFloat( arr );
					for(i=j; i<M; i++){
						fv[i] = DaoParseNumber( buf2 );
						while( *buf2 ) buf2 ++;
						buf2 ++;
						if( ( buf2 - inpack->data ) >= MAX_DATA ) break;
					}
				}else{
					dv = DaoArray_ToDouble( arr );
					if( numtype == DAO_COMPLEX ) M += M;
					for(i=j; i<M; i++){
						dv[i] = DaoParseNumber( buf2 );
						while( *buf2 ) buf2 ++;
						buf2 ++;
						if( ( buf2 - inpack->data ) >= MAX_DATA ) break;
					}
				}
				break;
			default : break;
			}
			start += size;
		}
		if( inpack->type ==0 && inpack->tag == DPP_TRANS_END ) break;
		if( count <= 0 ) break;
		/* printf( "before %i %i\n", daoProxyPort, sockfd ); */
		numbytes = LoopReceive( sockfd, bufin, MAX_DATA, 0 );
		/* printf( "after %i %i\n", daoProxyPort, sockfd ); */
		if( numbytes == -1 ){
			DString_Delete( str );
			return -1;
		}
	}
	DString_Delete( str );
	return dpp;
}

struct DaoSocket
{
	int id;
	int state;
};

#define MAX_ERRMSG 100
#define SOCKET_BOUND 1
#define SOCKET_CONNECTED 2
#define SOCKET_LISTENING 3

typedef struct DaoSocket DaoSocket;

extern DaoTypeBase socketTyper;
DaoType *daox_type_socket = NULL;

static DaoSocket* DaoSocket_New(  )
{
	DaoSocket *self = dao_malloc( sizeof(DaoSocket) );
	self->id = -1;
	self->state = 0;
	return self;
}

static void DaoSocket_Close( DaoSocket *self )
{
	if( self->id != -1 ){
		DaoNetwork_Close( self->id );
		self->id = -1;
		self->state = 0;
	}
}

static void DaoSocket_Delete( DaoSocket *self )
{
	DaoSocket_Close( self );
	dao_free( self );
}

static int DaoSocket_Bind( DaoSocket *self, int port )
{
	DaoSocket_Close( self );
	self->id = DaoNetwork_Bind( port );
	if( self->id != -1 )
		self->state = SOCKET_BOUND;
	return self->id;
}

static int DaoSocket_Connect( DaoSocket *self, DString *host, int port )
{
	DaoSocket_Close( self );
	self->id = DaoNetwork_Connect( DString_GetMBS( host ), port );
	if( self->id != -1 )
		self->state = SOCKET_CONNECTED;
	return self->id;
}

static void DaoSocket_Lib_Close( DaoProcess *proc, DaoValue *par[], int N  )
{
	DaoSocket_Close( (DaoSocket*)DaoValue_TryGetCdata( par[0] ) );
}

static void DaoSocket_Lib_Bind( DaoProcess *proc, DaoValue *par[], int N  )
{
	char errbuf[MAX_ERRMSG];
	DaoSocket *self = (DaoSocket*)DaoValue_TryGetCdata( par[0] );
	if( DaoSocket_Bind( self, DaoValue_TryGetInteger( par[1] ) ) == -1 ){
		GetErrorMessage( errbuf, GetError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
	}
}

static void DaoSocket_Lib_Listen( DaoProcess *proc, DaoValue *par[], int N  )
{
	char errbuf[MAX_ERRMSG];
	DaoSocket *self = (DaoSocket*)DaoValue_TryGetCdata( par[0] );
	if( self->state != SOCKET_BOUND ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The socket is not bound" );
		return;
	}
	if( DaoNetwork_Listen( self->id, DaoValue_TryGetInteger( par[1] ) ) == -1 ){
		GetErrorMessage( errbuf, GetError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	self->state = SOCKET_LISTENING;
}

static void DaoSocket_Lib_Accept( DaoProcess *proc, DaoValue *par[], int N  )
{
	char errbuf[MAX_ERRMSG];
	DaoSocket *self = (DaoSocket*)DaoValue_TryGetCdata( par[0] );
	DaoSocket *sock;
	if( self->state != SOCKET_LISTENING ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The socket is not in the listening state" );
		return;
	}
	sock = DaoSocket_New(  );
	sock->id = DaoNetwork_Accept( self->id );
	if( sock->id == -1 ){
		GetErrorMessage( errbuf, GetError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	sock->state = SOCKET_CONNECTED;
	DaoProcess_PutCdata( proc, (void*)sock, daox_type_socket );
}

static void DaoSocket_Lib_Connect( DaoProcess *proc, DaoValue *par[], int N  )
{
	char errbuf[MAX_ERRMSG];
	DaoSocket *self = (DaoSocket*)DaoValue_TryGetCdata( par[0] );
	if( DaoSocket_Connect( self, DaoString_Get( DaoValue_CastString( par[1] ) ), DaoValue_TryGetInteger( par[2] ) ) == -1 ){
		GetErrorMessage( errbuf, GetError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
	}
}

static void DaoSocket_Lib_Send( DaoProcess *proc, DaoValue *par[], int N  )
{
	char errbuf[MAX_ERRMSG];
	DaoSocket *self = (DaoSocket*)DaoValue_TryGetCdata( par[0] );
	int n;
	if( self->state != SOCKET_CONNECTED ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The socket is not connected" );
		return;
	}
	n = DaoNetwork_Send( self->id, DaoString_Get( DaoValue_CastString( par[1] ) ) );
	if( n == -1 ){
		GetErrorMessage( errbuf, GetError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
	}
	DaoProcess_PutInteger( proc, n );
}

static void DaoSocket_Lib_Receive( DaoProcess *proc, DaoValue *par[], int N  )
{
	char errbuf[MAX_ERRMSG];
	DaoSocket *self = (DaoSocket*)DaoValue_TryGetCdata( par[0] );
	if( self->state != SOCKET_CONNECTED ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The socket is not connected" );
		return;
	}
	DString *mbs = DaoProcess_PutMBString( proc, "" );
	if( DaoNetwork_Receive( self->id, mbs, DaoValue_TryGetInteger( par[1] ) ) == -1 ){
		GetErrorMessage( errbuf, GetError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
	}
}

static void DaoSocket_Lib_SendDao( DaoProcess *proc, DaoValue *par[], int N  )
{
	char errbuf[MAX_ERRMSG];
	int n;
	DaoSocket *self = (DaoSocket*)DaoValue_TryGetCdata( par[0] );
	if( self->state != SOCKET_CONNECTED ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The socket is not connected" );
		return;
	}
	n = DaoNetwork_SendExt( proc, self->id, par+1, N-1 );
	if( n == -1 ){
		GetErrorMessage( errbuf, GetError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	DaoProcess_PutInteger( proc, n );
}

static void DaoSocket_Lib_ReceiveDao( DaoProcess *proc, DaoValue *par[], int N  )
{
	char errbuf[MAX_ERRMSG];
	DaoSocket *self = (DaoSocket*)DaoValue_TryGetCdata( par[0] );
	if( self->state != SOCKET_CONNECTED ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The socket is not connected" );
		return;
	}
	DaoList *res = DaoProcess_PutList( proc );
	if( DaoNetwork_ReceiveExt( proc, self->id, res ) == -1 ){
		GetErrorMessage( errbuf, GetError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
	}
}

static void DaoSocket_Lib_Id( DaoProcess *proc, DaoValue *par[], int N  )
{
	DaoSocket *self = (DaoSocket*)DaoValue_TryGetCdata( par[0] );
	DaoProcess_PutInteger( proc, self->id );
}

static void DaoSocket_Lib_State( DaoProcess *proc, DaoValue *par[], int N  )
{
	DaoSocket *self = (DaoSocket*)DaoValue_TryGetCdata( par[0] );
	char buffer[10];
	switch( self->state ){
	case 0:                strcpy( buffer, "closed" ); break;
	case SOCKET_BOUND:     strcpy( buffer, "bound" ); break;
	case SOCKET_LISTENING: strcpy( buffer, "listening" ); break;
	case SOCKET_CONNECTED: strcpy( buffer, "connected" ); break;
	}
	DaoProcess_PutEnum( proc, buffer );
}

static void DaoSocket_Lib_GetPeerName( DaoProcess *proc, DaoValue *par[], int N  )
{
	char errbuf[MAX_ERRMSG];
	DaoSocket *self = (DaoSocket*)DaoValue_TryGetCdata( par[0] );
	struct sockaddr_in addr;
#ifdef WIN32
	int size = sizeof( struct sockaddr_in );
#else
	socklen_t size = sizeof( struct sockaddr_in );
#endif
	if( self->state != SOCKET_CONNECTED ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The socket is not connected" );
		return;
	}
	if( getpeername( self->id, (struct sockaddr *) & addr, & size ) == -1 ){
		GetErrorMessage( errbuf, GetError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	DString *res = DaoProcess_PutMBString( proc, "" );
	DString_SetMBS( res, inet_ntoa( addr.sin_addr ) );
}

static DaoFuncItem socketMeths[] =
{
	{  DaoSocket_Lib_Bind,          "bind( self :socket, port :int )" },
	{  DaoSocket_Lib_Listen,        "listen( self :socket, backlog=10 )" },
	{  DaoSocket_Lib_Accept,        "accept( self :socket )=>socket" },
	{  DaoSocket_Lib_Connect,       "connect( self :socket, host :string, port :int )" },
	{  DaoSocket_Lib_Send,          "send( self :socket, data :string )=>int" },
	{  DaoSocket_Lib_Receive,       "receive( self :socket, maxlen=512 )=>string" },
	{  DaoSocket_Lib_SendDao,       "send_dao( self :socket, ... )=>int" },
	{  DaoSocket_Lib_ReceiveDao,    "receive_dao( self :socket )=>list<int|float|double|complex|string|array>" },
	{  DaoSocket_Lib_GetPeerName,   "peername( self :socket )=>string" },
	{  DaoSocket_Lib_Id,            "id( self :socket )=>int" },
	{  DaoSocket_Lib_State,         "state( self :socket )=>enum<closed, bound, listening, connected>" },
	{  DaoSocket_Lib_Close,         "close( self :socket )" },
	{ NULL, NULL }
};

DaoTypeBase socketTyper = {
	"socket", NULL, NULL, socketMeths, {0}, {0}, (FuncPtrDel)DaoSocket_Delete, NULL
};

static void DaoNetLib_Bind( DaoProcess *proc, DaoValue *par[], int N  )
{
	char errbuf[MAX_ERRMSG];
	DaoSocket *sock = DaoSocket_New(  );
	if( DaoSocket_Bind( sock, DaoValue_TryGetInteger( par[0] ) ) == -1 ){
		GetErrorMessage( errbuf, GetError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	DaoProcess_PutCdata( proc, (void*)sock, daox_type_socket );
}
static void DaoNetLib_Connect( DaoProcess *proc, DaoValue *p[], int N  )
{
	char errbuf[MAX_ERRMSG];
	DaoSocket *sock = DaoSocket_New(  );
	if( DaoSocket_Connect( sock, DaoString_Get( DaoValue_CastString( p[0] ) ), DaoValue_TryGetInteger( p[1] ) ) == -1 ){
		GetErrorMessage( errbuf, GetError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	DaoProcess_PutCdata( proc, (void*)sock, daox_type_socket );
}
static void DaoNetLib_GetHost( DaoProcess *proc, DaoValue *par[], int N  )
{
	char errbuf[MAX_ERRMSG];
	struct hostent *hent;
	struct sockaddr_in addr;
	struct in_addr id;
	size_t size = sizeof( struct sockaddr_in );
	const char *host = DaoString_GetMBS( DaoValue_CastString( par[0] ) );
	DaoFactory *factory = DaoProcess_GetFactory( proc );
	DaoMap *res = DaoProcess_PutMap( proc );
	DaoValue *value;
	if( DaoString_Size( DaoValue_CastString( par[0] ) ) ==0 ) return;
	if( host[0] >= '0' && host[0] <= '9' ){
#ifdef UNIX
		if( inet_aton( host, & id ) == 0 ) return;
#elif WIN32
		id.s_addr = inet_addr( host );
#endif
		addr.sin_family = AF_INET; 
		addr.sin_addr = id;
		memset( &( addr.sin_zero ), '\0', 8);
		hent = gethostbyaddr( (void*) & addr, size, AF_INET );
	}else{
		hent = gethostbyname( host );
	}
	if( hent == NULL ){
		GetHostErrorMessage( errbuf, GetHostError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	if( hent->h_addrtype == AF_INET ){
		char **p = hent->h_aliases;
		char **q = hent->h_addr_list;
		while( *p ){
			value = (DaoValue*) DaoFactory_NewMBString( factory, inet_ntoa( *(struct in_addr*) (*q) ), 0 );
			DaoMap_InsertMBS( res, *p, value );
			p ++;
			q ++;
		}
	}else{ /* AF_INET6 */
	}
}

static void DaoNetLib_Select( DaoProcess *proc, DaoValue *par[], int N  )
{
	char errbuf[MAX_ERRMSG];
	struct timeval tv;
	int i;
	fd_set set1, set2;
	DaoFactory *factory = DaoProcess_GetFactory( proc );
	DaoTuple *tuple = DaoProcess_PutTuple( proc );
	DaoList *list1 = DaoValue_CastList( par[0] );
	DaoList *list2 = DaoValue_CastList( par[1] );
	DaoList *reslist;
	DaoValue *value;
	DaoSocket *socket;
	FILE *file;
	if( DaoList_Size( list1 ) == 0 && DaoList_Size( list2 ) == 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "Both read and write parameters are empty lists" );
		return;
	}
	FD_ZERO( &set1 );
	FD_ZERO( &set2 );
	for( i = 0; i < DaoList_Size( list1 ); i++ ){
		value = DaoList_GetItem( list1, i );
		if( DaoValue_Type( value ) == DAO_STREAM ){
			file = DaoStream_GetFile( DaoValue_CastStream( value ) );
			if( file == NULL ){
				DaoProcess_RaiseException( proc, DAO_ERROR, "The read list contains a stream not associated with a file" );
				return;
			}
			FD_SET( fileno( file ), &set1 );
		}else{
			socket = (DaoSocket*)DaoValue_TryGetCdata( value );
			if( socket->id == -1 ){
				DaoProcess_RaiseException( proc, DAO_ERROR, "The read list contains a closed socket" );
				return;
			}
			FD_SET( socket->id, &set1 );
		}
	}
	for( i = 0; i < DaoList_Size( list2 ); i++ ){
		value = DaoList_GetItem( list2, i );
		if( DaoValue_Type( value ) == DAO_STREAM ){
			file = DaoStream_GetFile( DaoValue_CastStream( value ) );
			if( file == NULL ){
				DaoProcess_RaiseException( proc, DAO_ERROR, "The write list contains a stream not associated with a file" );
				return;
			}
			FD_SET( fileno( file ), &set2 );
		}else{
			socket = (DaoSocket*)DaoValue_TryGetCdata( value );
			if( socket->id == -1 ){
				DaoProcess_RaiseException( proc, DAO_ERROR, "The write list contains a closed socket" );
				return;
			}
			FD_SET( socket->id, &set2 );
		}
	}
	tv.tv_sec = (int)DaoValue_TryGetFloat( par[2] );
	tv.tv_usec = ( DaoValue_TryGetFloat( par[2] )- tv.tv_sec ) * 1E6;
	if( select( FD_SETSIZE, &set1, &set2, NULL, & tv ) == -1 ){
		GetErrorMessage( errbuf, GetError() );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	value = (DaoValue*) DaoFactory_NewList( factory );
	DaoTuple_SetItem( tuple, value, 0 );
	reslist = DaoValue_CastList( DaoTuple_GetItem( tuple, 0 ) );
	for( i = 0; i < DaoList_Size( list1 ); i++ ){
		value = DaoList_GetItem( list1, i );
		if( DaoValue_Type( value ) == DAO_STREAM ){
			if( FD_ISSET( fileno( DaoStream_GetFile( DaoValue_CastStream( value ) ) ), &set1 ) )
				DaoList_PushBack( reslist, value );
		}else if( FD_ISSET( ((DaoSocket*)DaoValue_TryGetCdata( value ))->id, &set1 ) )
			DaoList_PushBack( reslist, value );
	}
	value = (DaoValue*) DaoFactory_NewList( factory );
	DaoTuple_SetItem( tuple, value, 1 );
	reslist = DaoValue_CastList( DaoTuple_GetItem( tuple, 1 ) );
	for( i = 0; i < DaoList_Size( list2 ); i++ ){
		value = DaoList_GetItem( list2, i );
		if( DaoValue_Type( value ) == DAO_STREAM ){
			if( FD_ISSET( fileno( DaoStream_GetFile( DaoValue_CastStream( value ) ) ), &set2 ) )
				DaoList_PushBack( reslist, value );
		}else if( FD_ISSET( ((DaoSocket*)DaoValue_TryGetCdata( value ))->id, &set2 ) )
			DaoList_PushBack( reslist, value );
	}
}

static DaoFuncItem netMeths[] =
{
	{  DaoNetLib_Bind,          "bind( port :int )=>socket" },
	{  DaoNetLib_Connect,       "connect( host :string, port :int )=>socket" },
	{  DaoNetLib_GetHost,       "gethost( host :string )=>map<string,string>" },
	{  DaoNetLib_Select,
		"select( read :list<@X<stream|socket>>, write :list<@Y<stream|socket>>, timeout :float )=>tuple<read :list<@X>, write :list<@Y>>" },
	{ NULL, NULL }
};

DaoTypeBase libNetTyper = {
	"net", NULL, NULL, netMeths, {0}, {0}, NULL, NULL
};

void DaoNetwork_Init( DaoVmSpace *vms, DaoNamespace *ns )
{

#ifdef WIN32
	char errbuf[MAX_ERRMSG];
	WSADATA wsaData;   /*  if this doesn't work */
	/* WSAData wsaData; // then try this instead */

	if(WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
		strcpy( errbuf, "WSAStartup failed: " );
		GetErrorMessage( errbuf + strlen( errbuf ), GetError() );
		strcat( errbuf, "\n" );
		fprintf(stderr, errbuf );
		exit(1);
	} 
#endif

}

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	daox_type_socket = DaoNamespace_WrapType( ns, & socketTyper, 1 );
	DaoNamespace_WrapType( ns, & libNetTyper, 1 );
	DaoNetwork_Init( vmSpace, ns );
	return 0;
}
