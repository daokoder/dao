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
  DPP_ADMIN_REQUEST = 2,
  DPP_ADMIN_REPORT  = 3,
  DPP_MPI_SPAWN = 4,
  DPP_MPI_OSPROC = 5,
  DPP_MPI_VMPROC = 6,
  DPP_MPI_REGISTER = 7,
  DPP_MPI_FINDPID = 8,
  DPP_NULL
};

#ifdef DAO_WITH_MPI
int daoProxyPort = DAO_PROXY_PORT;

static dint my_sockfd; /*  as server */
static dint max_port = DAO_PROXY_PORT + 1;

static DaoMap  *pid2Port = NULL;
static DaoMap  *pid2Mutex = NULL;
static DaoMap  *pid2Condv = NULL;
#endif

#ifdef DAO_WITH_THREAD
static DMutex    portMutex;
#endif

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

typedef struct DaoFdSet { fd_set set; } DaoFdSet;
static void* DaoFdSet_New(){ return dao_malloc( sizeof(DaoFdSet) ); }
DaoTypeBase DaoFdSet_Typer = 
{ NULL, "fd_set", NULL, NULL, {0}, (FuncPtrNew)DaoFdSet_New, (FuncPtrDel)free };

#define GET_FDSET( p )  ( & ( (DaoFdSet*) p->v.cdata->data )->set )

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
  DValue item = daoNilValue;
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
  DValue value = daoNilString;
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
static void DaoNetLib_FD_ZERO( DaoContext *ctx, DValue *par[], int N  )
{
  FD_ZERO( GET_FDSET( par[0] ) );
}
static void DaoNetLib_FD_SET( DaoContext *ctx, DValue *par[], int N  )
{
  FD_SET( par[0]->v.i, GET_FDSET( par[1] ) );
}
static void DaoNetLib_FD_CLR( DaoContext *ctx, DValue *par[], int N  )
{
  FD_CLR( par[0]->v.i, GET_FDSET( par[1] ) );
}
static void DaoNetLib_FD_ISSET( DaoContext *ctx, DValue *par[], int N  )
{
  DaoContext_PutInteger( ctx, FD_ISSET( par[0]->v.i, GET_FDSET( par[1] ) ) );
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
  {  DaoNetLib_FD_ZERO,       "FD_ZERO( set :fd_set )" },
  {  DaoNetLib_FD_SET,        "FD_SET( fd :int, set :fd_set )" },
  {  DaoNetLib_FD_CLR,        "FD_CLR( fd :int, set :fd_set )" },
  {  DaoNetLib_FD_ISSET,      "FD_ISSET( fd :int, set :fd_set )" },
  {  DaoNetLib_Select,
    "select( nfd :int, setr :fd_set, setw :fd_set, sete :fd_set, tv :float )=>int" },
  { NULL, NULL }
};

static DaoTypeCore netCore =
{
  0, NULL, NULL, NULL,
  DaoBase_GetField,
  DaoBase_SetField,
  DaoBase_GetItem,
  DaoBase_SetItem,
  DaoBase_Print,
  DaoBase_Copy,
};
DaoTypeBase libNetTyper = {
  & netCore,
  "network",
  NULL,
  netMeths,
  {0},
  NULL, NULL
};

DaoCData libNetwork = 
{ DAO_CDATA, DAO_DATA_CONST, {0,0}, 1,0, NULL,NULL, NULL, & libNetTyper, 0,0,0,0 };

#ifdef DAO_WITH_MPI
int DaoSpawn_OsProc( DString *pid, DString *src, float timeout )
{
  char *pname = DString_GetMBS( pid );

  /* printf( "DaoSpawn_OsProc(): %s\n", DString_GetMBS( pidself ) ); */

#ifdef UNIX
  char buf1[64];
  char buf2[64];
  sprintf( buf1, "-p%s", pname );
  sprintf( buf2, "--proc-port=%i", daoProxyPort );
  if( fork()==0 ){
    /* errno = 0; */
    /* printf( "====================: %s\n" , strerror(errno) ); */
    execlp( "dao", "dao", buf1, buf2, DString_GetMBS( src ), NULL );
    exit(0);
  }
#elif WIN32
  char buf[256];
  PROCESS_INFORMATION info;
  STARTUPINFO st = {0};
  sprintf( buf, "dao.exe -p%s --proc-port=%i %s", pname, daoProxyPort, DString_GetMBS( src ) );
  CreateProcess( NULL, buf, NULL, NULL, 0, 0, NULL, NULL, & st, & info );

#endif
  /* printf( "%s, timeout = %g\n", DString_GetMBS( pid ), timeout ); */
  /* condition wait */
#ifdef DAO_WITH_THREAD
  if( abs( timeout ) > 1.0E-9 ){
    DaoMutex   *mutex;
    DaoCondVar *condv;
    DValue value;

    DMutex_Lock( & portMutex );
    mutex = DaoMutex_New( NULL );
    condv = DaoCondVar_New( NULL );
    value.t = DAO_MUTEX;
    value.v.p = (DaoBase*) mutex;
    DaoMap_InsertMBS( pid2Mutex, pname, value );
    value.t = DAO_CONDVAR;
    value.v.p = (DaoBase*) condv;
    DaoMap_InsertMBS( pid2Condv, pname, value );
    DMutex_Unlock( & portMutex );

    DMutex_Lock( & mutex->myMutex );
    if( timeout > 1.0E-9 ){
      if( DCondVar_TimedWait( & condv->myCondVar, & mutex->myMutex, timeout ) ){
        /* printf( "timeout ......\n" ); */
        DMutex_Unlock( & mutex->myMutex );
        return 0;
      }
    }else{
      DCondVar_Wait( & condv->myCondVar, & mutex->myMutex );
    }
    DMutex_Unlock( & mutex->myMutex );
    return max_port-1;
  }
#endif
  return 0;
}
int DaoSpawn_VmProcOsProcHost( DString *host, int port, DString *pid, DString *rout, float timeout )
{
  fd_set set;
  int sockfd, count;
  DValue val;
  DValue num;
  DaoList *list;
  DString *buf;
  struct timeval tv;

  /* printf( "DaoSpawn_VmProcOsProcHost(): %i %g %s\n", port, timeout, DString_GetMBS( host ) ); */
  if( ( sockfd = DaoNetwork_Connect( DString_GetMBS( host ), port ) ) == -1 ){
    perror("connect");
    return 0;
  }
  /* printf( "DaoSpawn_VmProcOsProcHost()\n" ); */

  list = DaoList_New();
  buf = DString_New(1);
  num = DValue_NewFloat( timeout );

  DaoNetwork_SendTag( sockfd, DPP_MPI_SPAWN );
  DString_Assign( buf, pid );
  DaoNetwork_SendString( sockfd, buf );
  DString_Assign( buf, rout );
  DaoNetwork_SendString( sockfd, buf );
  if( DString_FindChar( pid, '@', 0 )==0 ) DaoNetwork_SendNumber( sockfd, num );
  /* To let the other side return from DaoNetwork_ReceiveExt(), and proceed.  */
  DaoNetwork_SendNil( sockfd );
  
  port = 0;
  FD_ZERO( & set );
  FD_SET( sockfd, & set );
  /* printf("select : timeout = %g, %i\n", timeout, sockfd ); */
  if( timeout >0 ){
    tv.tv_sec = (int)timeout;
    tv.tv_usec = ( timeout - (float)tv.tv_sec ) * 1E6;
    count = select( sockfd +1, & set, NULL, NULL, & tv );
  }else{
    count = select( sockfd +1, & set, NULL, NULL, NULL );
  }
  /* printf("select : count = %i, %i\n", count, port ); */
  if( count >0 ){
    DaoNetwork_ReceiveExt( sockfd, list );
    if( list->items->size >0 ){
      val = list->items->data[0];
      if( val.t == DAO_INTEGER ) port = val.v.i;
    }
  }
  DaoNetwork_Close( sockfd );
  DaoList_Delete( list );
  DString_Delete( buf );
  return port;
}
int DaoSpawn_VmProcOsProc( int port, DString *pid, DString *rout, float timeout )
{
  /* printf( "DaoSpawn_VmProcOsProc(): %i\n", port ); */
  DString *host = DString_New(1);
  DString_SetMBS( host, "localhost" );
  port = DaoSpawn_VmProcOsProcHost( host, port, pid, rout, timeout );
  DString_Delete( host );
  return port;
}
int DaoSpawn_VmProcHost( DString *host, DString *pid, DString *rout, float timeout )
{
  return DaoSpawn_VmProcOsProcHost( host, DAO_PROXY_PORT, pid, rout, timeout );
}
int DaoSpawn_OsProcHost( DString *host, DString *pid, DString *src, float timeout )
{
  DString_InsertChar( pid, '@', 0 ); /* indicate an OS process */
  return DaoSpawn_VmProcOsProcHost( host, DAO_PROXY_PORT, pid, src, timeout );
}
void DaoProxy_Register( DString *host, int port, DString *mypid )
{
  int sockfd;
  /* printf( ">>>>>>>>>> %s %i %s\n", DString_GetMBS( host ), port, DString_GetMBS( mypid ) ); */
  if( (sockfd = DaoNetwork_Connect( DString_GetMBS( host ), port ) ) == -1 ) {
    perror("connect");
    return;
  }
  DaoNetwork_SendTag( sockfd, DPP_MPI_REGISTER );
  DaoNetwork_SendString( sockfd, mypid );
  DaoNetwork_SendInteger( sockfd, daoProxyPort );
  DaoNetwork_SendNil( sockfd );
  DaoNetwork_Close( sockfd );
}
int DaoProxy_FindPID( DaoContext *ctx, DString *host, int port, DString *pid, DString *suffix )
{
  fd_set set;
  dint sockfd, count, rtc = 0;
  DaoList *list;
  DValue val;
  DString *pname;

  /* printf( ">>>>>>>>>> %s %i %s\n", DString_GetMBS( host ), port, DString_GetMBS( pid ) ); */
  if( (sockfd = DaoNetwork_Connect( DString_GetMBS( host ), port ) ) == -1 ) {
    perror("connect");
    return 0;
  }
  DaoNetwork_SendTag( sockfd, DPP_MPI_FINDPID );
  DaoNetwork_SendString( sockfd, pid );
  DaoNetwork_SendNil( sockfd );
  
  list = DaoList_New();
  FD_ZERO( & set );
  FD_SET( sockfd, & set );
  count = select( sockfd +1, & set, NULL, NULL, NULL );
  /* printf("select : count = %i, %i\n", count, port ); */
  if( count >0 ){
    DaoNetwork_ReceiveExt( sockfd, list );
    if( list->items->size >0 ){
      val = list->items->data[0];
      if( val.t == DAO_INTEGER ) rtc = val.v.i;
    }
  }
  DaoNetwork_Close( sockfd );
  if( rtc >0 ){
    pname = DString_Copy( pid );
    DString_Append( pname, suffix );
    DMap_Insert( ctx->vmSpace->friendPids, (void*)pname, (void*)rtc );
    DString_Delete( pname );
  }
  DaoList_Delete( list );
  return rtc >0;
}

void DaoProxy_Spawn( void *params )
{
  /* printf( "DaoProxy_Spawn()\n" ); */
  DaoList *list = (DaoList*)params;
  DValue *par = list->items->data;
  DString *pid = par[0].v.s;
  DString *src =  par[1].v.s;
  float timeout =  par[2].v.f;
  int sockfd = par[3].v.i;
  int port = DaoSpawn_OsProc( pid, src, timeout );
  DValue num = DValue_NewInteger( port );
  if( sockfd >= 0 ){
    DaoNetwork_SendNumber( sockfd, num );
    /* To make sure the other side return from DaoNetwork_ReceiveExt(), and proceed. */
    DaoNetwork_SendNil( sockfd );
    DaoNetwork_Close( sockfd );
  }
}
int DaoProxy_Send( DString *sender, DString *host, int port, DString *pid, DVarray *message )
{
  int sockfd, count, rtc=0;
  DString *mbs;
  DaoList *list;
  DValue val;
  fd_set set;
  
  /* printf( "DaoProxy_Send: %s, port = %i\n", DString_GetMBS( host ), port ); */
  if( (sockfd = DaoNetwork_Connect( DString_GetMBS( host ), port ) ) == -1 ) {
    /* perror("connect"); */
    return 0;
  }
  /* printf( "DaoProxy_Send: %s, sockfd = %i\n", DString_GetMBS( host ), sockfd ); */
  mbs = DString_New(1);
  list = DaoList_New();
  DaoNetwork_SendTag( sockfd, pid==NULL ? DPP_MPI_OSPROC : DPP_MPI_VMPROC );
  if( pid ){
    DString_Assign( mbs, pid );
    DaoNetwork_SendString( sockfd, mbs );
  }
  DString_Assign( mbs, sender );
  DaoNetwork_SendString( sockfd, mbs );
  DaoNetwork_SendExt( sockfd, message->data, message->size ); /* XXX */
  DaoNetwork_SendNil( sockfd );

  FD_ZERO( & set );
  FD_SET( sockfd, & set );
  count = select( sockfd +1, & set, NULL, NULL, NULL );
  /* printf("select : count = %i, %i\n", count, port ); */
  if( count >0 ){
    DaoNetwork_ReceiveExt( sockfd, list );
    if( list->items->size >0 ){
      val = list->items->data[0];
      if( val.t == DAO_INTEGER ) rtc = val.v.i;
    }
  }
  DaoNetwork_Close( sockfd );
  return rtc;
  /* printf( "END OF SEND\n" ); */
}
extern DaoVmProcess* DaoVmProcess_Create( DaoContext *ctx, DValue *par[], int N );
extern void MPI_Send( DaoContext *ctx, DValue *par[], int N );
void DaoProxy_Receive( DaoContext *ctx, DValue *par[], int N )
{
  DaoVmProcess *vmproc = NULL;
  DValue num = DValue_NewInteger( daoProxyPort );
  DValue nv, sv;
  DValue *pp;
  DNode  *node;
  int new_fd, dpp;
  char *chs;

  /* printf( "DaoProxy_Receive() started...\n" ); */
  while(1){
    DaoList  *res = DaoList_New();
    /* printf( "<<<<receive1: %i %i\n", daoProxyPort, my_sockfd ); */
    new_fd = accept( my_sockfd, NULL, NULL );
    /* printf( "<<<<receive2: %i %i\n", daoProxyPort, new_fd ); */
    if( new_fd == -1 ) return;
    dpp = DaoNetwork_ReceiveExt( new_fd, res );
    pp = res->items->data;
    num.v.i = daoProxyPort;
    switch( dpp ){
    case DPP_ADMIN_REQUEST :
      {
        /* printf( "DPP_ADMIN_REQUEST %i\n", max_port ); */
        DaoNetwork_SendInteger( new_fd, max_port );
        DaoNetwork_SendTag( new_fd, DPP_TRANS_END );
        break;
      }
    case DPP_ADMIN_REPORT :
      {
        /* printf( "DPP_ADMIN_REPORT\n" ); */
#ifdef DAO_WITH_THREAD
        DaoMutex   *mutex;
        DaoCondVar *condv;
#endif
        DString *mbs;
        sv = res->items->data[0];
        nv = res->items->data[1];
        max_port = nv.v.i;
        chs = DString_GetMBS( sv.v.s );
        DaoMap_InsertMBS( pid2Port, chs, nv );
        /* printf( "report port = %i; %s\n", max_port, chs ); */
        mbs = DString_Copy( sv.v.s );
        DString_InsertChar( mbs, '@', 0 );
        if( DMap_Find( ctx->vmSpace->friendPids, (void*)mbs ) == NULL )
          DMap_Insert( ctx->vmSpace->friendPids, (void*)mbs, (void*) max_port );

        DString_InsertMBS( mbs, "main", 0, 0, 0 );
        if( DMap_Find( ctx->vmSpace->friendPids, (void*)mbs ) == NULL )
          DMap_Insert( ctx->vmSpace->friendPids, (void*)mbs, (void*) max_port );
        /*
         */
        max_port ++;

#ifdef DAO_WITH_THREAD
        DMutex_Lock( & portMutex );
        mutex = (DaoMutex*)DaoMap_GetValueMBS( pid2Mutex, chs ).v.p;
        condv = (DaoCondVar*)DaoMap_GetValueMBS( pid2Condv, chs ).v.p;
        DMutex_Unlock( & portMutex );

        if( mutex ){
          DMutex_Lock( & mutex->myMutex );
          DCondVar_Signal( & condv->myCondVar );
          DMutex_Unlock( & mutex->myMutex );

          DMutex_Lock( & portMutex );
          DaoMap_EraseMBS( pid2Mutex, chs );
          DaoMap_EraseMBS( pid2Condv, chs );
          DMutex_Unlock( & portMutex );
        }
#endif
        break;
      }
    case DPP_MPI_SPAWN :
      {
        DString *pid = pp[0].v.s;
        /*
           printf( "@@@@@@@@@@@@@@@@@@spawning: %i %s %s\n", res->items->size,
           pid->mbs, DString_GetMBS(pp[1]->v.s) );
         */
        if( pid->mbs[0] == '@' ){
          DValue p = DaoMap_GetValueMBS( pid2Port, pid->mbs +1 );
#ifdef DAO_WITH_THREAD
          if( p.t ==0 ){
            DThread *thread = (DThread*) dao_malloc( sizeof(DThread) );
            DString_Erase( pid, 0, 1 );
            num.v.i = new_fd;
            DaoList_Append( res, num );
            DThread_Init( thread );
            DThread_Start( thread, DaoProxy_Spawn, res );
            continue;
          }
#endif
          DaoNetwork_SendNumber( new_fd, p );
        }else{
          /* spawn virtual machine process */
          node = DMap_Find( ctx->vmSpace->friendPids, (void*)pid );
          if( node == NULL ){
            /* XXX */
            /* /vmproc = DaoVmProcess_Create( ctx, pp+1, res->items->size-1 ); */
            vmproc->pauseType = DAO_VMP_SPAWN;
            if( vmproc ){
              DValue par = { DAO_VMPROCESS, 0, 0, 0, {0} };
              DValue *par2 = & par;
              par.v.vmp = vmproc;
              DString_Assign( vmproc->mpiData->name, pid );
              DMap_Insert( ctx->vmSpace->friendPids, (void*)pid, vmproc );
              MPI_Send( ctx, & par2, 1 );
            }else{
              DaoVmProcess_PrintException( ctx->process, 1 );
              num.v.i = 0;
            }
            DMap_Insert( ctx->vmSpace->friendPids, (void*)pid, vmproc );
          }
          DaoNetwork_SendNumber( new_fd, num );
        }
        break;
      }
#if( defined DAO_WITH_THREAD && (defined DAO_WITH_MPI || defined DAO_WITH_AFC ) )
    case DPP_MPI_OSPROC :
      {
        /*
           printf( "...............OSPROC\n" );
         */
        DVarray *array = DVarray_New();
        DValue v = { DAO_VMPROCESS, 0, {0} };
        v.v.p = (DaoBase*) ctx->vmSpace->mainProcess;
        num.v.i = 0;
        DVarray_PushFront( res->items, v );
        DVarray_Swap( array, res->items );
        /* DO NOT send in HURRY mode, since the mainProcess might be running! */
        DaoSched_Send( array, 0, NULL, NULL );
        num.v.i = 1;
        DaoNetwork_SendNumber( new_fd, num );
        break;
      }
    case DPP_MPI_VMPROC :
      {
        /*
           printf( "...............VMPROC\n" );
         */
        num.v.i = 0;
        node = DMap_Find( ctx->vmSpace->friendPids, (void*) pp[0]->v.s );
        /*
           printf( "DPP_MPI_VMPROC: %p %s\n", node, DaoString_GetMBS( pp[0] ) );
         */
        if( node != NULL ){
          DVarray *array = DVarray_New();
          DValue value = daoNilValue;
          if( node->value.pBase ) value.t = node->value.pBase->type;
          value.v.p = node->value.pBase;
          DaoList_PopFront( res );
          DVarray_PushFront( res->items, value );
          DVarray_Swap( array, res->items );
          DaoSched_Send( array, 0, NULL, NULL );
          num.v.i = 10;
          DVarray_Delete( array );
        }
        DaoNetwork_SendNumber( new_fd, num );
        break;
      }
#endif
    case DPP_MPI_REGISTER :
      {
        DString *pid = pp[0].v.s;
        DMap_Insert( ctx->vmSpace->friendPids, (void*)pid, (void*) pp[1].v.i );
        break;
      }
    case DPP_MPI_FINDPID :
      {
        DString *pid = pp[0].v.s;
        if( pid->mbs[0] == '@' ){
          node = DMap_Find( ctx->vmSpace->friendPids, (void*) pid );
          if( node != NULL ) DaoNetwork_SendInteger( new_fd, node->value.pInt );
        }else{
          node = DMap_Find( ctx->vmSpace->friendPids, (void*) pid );
          if( node != NULL ) DaoNetwork_SendInteger( new_fd, daoProxyPort );
        }
        break;
      }
    default : break;
    }
    DaoNetwork_SendNil( new_fd );
    DaoNetwork_Close(new_fd);
  }
  /* printf( "END OF RECEIVE\n" ); */
}
#endif
void DaoNetwork_Init( DaoVmSpace *vms, DaoNameSpace *ns )
{
#ifdef DAO_WITH_MPI
  struct hostent *hent;
  static char buf[256] = "LOCALHOST=";
#endif
  
#ifdef WIN32
  WSADATA wsaData;   /*  if this doesn't work */
  /* WSAData wsaData; // then try this instead */

  if(WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
    fprintf(stderr, "WSAStartup failed.\n");
    exit(1);
  } 
#endif
#ifdef DAO_WITH_THREAD
  DMutex_Init( & portMutex );
#endif

#ifdef DAO_WITH_MPI
  if( getenv( "PROC_NAME" ) || getenv( "PROC_PORT" ) ){
    hent = gethostbyname( "localhost" );
    if( hent->h_addrtype == AF_INET ){
      strncat( buf, hent->h_name, 240 );
    }else{ /* AF_INET6 */
      strncat( buf, "localhost", 240 );
    }
    putenv( buf );
  }else{
    strncat( buf, "localhost", 240 );
    putenv( buf );
  }
#endif

#ifdef DAO_WITH_MPI
  pid2Port = DaoMap_New();
  pid2Mutex = DaoMap_New();
  pid2Condv = DaoMap_New();

  DaoNameSpace_AddFunction( ns, DaoProxy_Receive, "proxy_receive()" );
#endif
}
#ifdef DAO_WITH_MPI
void DaoProxy_Init( DaoVmSpace *vmSpace )
{
  int yes = 1, attempt = 0;
  int msfd;
  int paport = DAO_PROXY_PORT;
  int length;
  struct sockaddr_in my_addr;    /*  my address information */

  char *proc_name = getenv( "PROC_NAME" );
  char *proc_port = getenv( "PROC_PORT" );
  DValue   num = DValue_NewInteger(1);
  DaoList *list = DaoList_New();
  DValue val;

  if( proc_name == NULL ) proc_name = "MASTER";
  if( proc_port != NULL ) paport = strtol( proc_port, NULL, 10 );

  /* printf( "init %s: %i\n", proc_name, paport ); */

  length = strlen( proc_name ) + offset;
  if( ( msfd = DaoNetwork_Connect( "localhost", paport ) ) != -1 ){
    DaoNetwork_SendTag( msfd, DPP_ADMIN_REQUEST );
    /* DaoNetwork_SendChars( msfd, proc_name, strlen( proc_name ) ); */
    /* DaoNetwork_SendNumber( msfd, daoProxyPort ); */
    DaoNetwork_SendTag( msfd, DPP_TRANS_END );

    DaoNetwork_ReceiveExt( msfd, list );
    if( list->items->size >0 ){
      val = list->items->data[0];
      if( val.t == DAO_INTEGER ) daoProxyPort = val.v.i;
    }
  }
  /* printf( "init %s: %i\n", proc_name, daoProxyPort ); */

  if( ( my_sockfd = socket(AF_INET, SOCK_STREAM, 0) ) == -1 ) {
    perror("socket");
    exit(1);
  }
  if( setsockopt( my_sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(int) ) == -1 ) {
    perror( "setsockopt" );
    exit(1);
  }

  my_addr.sin_family = AF_INET;         /*  host byte order */
  my_addr.sin_port = htons( daoProxyPort );     /*  short, network byte order */
  my_addr.sin_addr.s_addr = INADDR_ANY; /*  automatically fill with my IP */
  memset(&(my_addr.sin_zero), '\0', 8); /*  zero the rest of the struct */

  while( bind( my_sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr) ) == -1){
    daoProxyPort ++;
    attempt ++;
    my_addr.sin_port = htons( daoProxyPort );     /*  short, network byte order */
    if( attempt >= 1000 ) return;
  }

  if( listen( my_sockfd, BACKLOG ) == -1 ){
    perror("listen");
    exit(1);
  }
  if( ( msfd = DaoNetwork_Connect( "localhost", paport ) ) != -1 ){
    DaoNetwork_SendTag( msfd, DPP_ADMIN_REPORT );
    DaoNetwork_SendChars( msfd, proc_name, strlen( proc_name ) );
    DaoNetwork_SendInteger( msfd, daoProxyPort );
    DaoNetwork_SendTag( msfd, DPP_TRANS_END );
  }
  DaoNetwork_Close( msfd );
  num.v.i = daoProxyPort;
  DaoMap_InsertMBS( pid2Port, proc_name, num );
}
#endif

#endif
