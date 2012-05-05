/*
// Dao Standard Modules
// http://www.daovm.net
//
// Copyright (c) 2006-2012, Limin Fu
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
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

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
#include"dao_sys.h"


static void SYS_Ctime( DaoProcess *proc, DaoValue *p[], int N )
{
	struct tm *ctime;
	time_t t = (time_t)p[0]->xInteger.value;
	DaoTuple *tuple = DaoTuple_New( 7 );
	DaoValue **items = tuple->items;
	if( t == 0 ) t = time(NULL);
	ctime = gmtime( & t );
	items[0] = (DaoValue*) DaoProcess_NewInteger( proc, ctime->tm_year + 1900 );
	items[1] = (DaoValue*) DaoProcess_NewInteger( proc, ctime->tm_mon + 1 );
	items[2] = (DaoValue*) DaoProcess_NewInteger( proc, ctime->tm_mday );
	items[3] = (DaoValue*) DaoProcess_NewInteger( proc, ctime->tm_wday + 1 );
	items[4] = (DaoValue*) DaoProcess_NewInteger( proc, ctime->tm_hour );
	items[5] = (DaoValue*) DaoProcess_NewInteger( proc, ctime->tm_min );
	items[6] = (DaoValue*) DaoProcess_NewInteger( proc, ctime->tm_sec );
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
	DaoString *ds = DaoString_New(1);
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
static void SYS_Popen( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoStream *stream = NULL;
	char *mode;
	DString *fname;

	stream = DaoStream_New();
	stream->file = (DFile*)dao_malloc( sizeof(DFile) );
	stream->file->rc = 1;
	stream->attribs |= DAO_IO_PIPE;
	fname = stream->fname;
	DString_Assign( fname, p[0]->xString.data );
	if( DString_Size( fname ) >0 ){
		mode = DString_GetMBS( p[1]->xString.data );
		stream->file->fd = popen( DString_GetMBS( fname ), mode );
		if( stream->file->fd == NULL ){
			dao_free( stream->file );
			stream->file = NULL;
			DaoProcess_RaiseException( proc, DAO_ERROR, "error opening pipe" );
		}
		stream->mode = 0;
		if( strstr( mode, "+" ) )
			stream->mode = DAO_IO_WRITE | DAO_IO_READ;
		else{
			if( strstr( mode, "r" ) )
				stream->mode |= DAO_IO_READ;
			if( strstr( mode, "w" ) || strstr( mode, "a" ) )
				stream->mode |= DAO_IO_WRITE;
		}
	}else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "empty command line" );
	}
	DaoProcess_PutValue( proc, (DaoValue*)stream );
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
	old = setlocale( category, N == 1 ? NULL : DString_GetMBS( p[1]->xString.data ) );
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
	{ SYS_Popen,     "popen( cmd :string, mode :string )=>stream" },
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

DaoTypeBase modSysCoreTyper = { "sys", NULL, NULL, sysMeths, {0}, {0}, NULL, NULL };



static void DaoBUF_New( DaoProcess *proc, DaoValue *p[], int N )
{
	daoint size = p[0]->xInteger.value;
	Dao_Buffer *self = Dao_Buffer_New( size >= 0 ? size : 0 );
	DaoProcess_PutValue( proc, (DaoValue*) self );
	if( size < 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "negative buffer size" );
		return;
	}
}
static void DaoBUF_Size( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	DaoProcess_PutInteger( proc, self->size );
}
static void DaoBUF_Resize( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	daoint size = p[1]->xInteger.value;
	if( size < 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "negative buffer size" );
		return;
	}
	Dao_Buffer_Resize( self, size );
}
static void DaoBUF_CopyData( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	Dao_Buffer *other = (Dao_Buffer*) p[1];
	if( other->bufsize == 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR_PARAM, "invalid value" );
		return;
	}
	if( self->bufsize < other->size ) Dao_Buffer_Resize( self, other->size );
	memcpy( self->buffer.pVoid, other->buffer.pVoid, other->size );
	self->size = other->size;
}
static void DaoBUF_GetString( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	DString *str = DaoProcess_PutMBString( proc, "" );
	if( p[1]->xEnum.value == 0 ){
		DString_Resize( str, self->size );
		memcpy( str->mbs, self->buffer.pVoid, self->size );
	}else{
		DString_ToWCS( str );
		DString_Resize( str, self->size / sizeof( wchar_t ) );
		memcpy( str->wcs, self->buffer.pVoid, str->size * sizeof( wchar_t ) );
	}
}
static void DaoBUF_SetString( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	DString *str = p[1]->xString.data;
	if( str->mbs ){
		Dao_Buffer_Resize( self, str->size );
		memcpy( self->buffer.pVoid, str->mbs, str->size );
	}else{
		Dao_Buffer_Resize( self, str->size * sizeof(wchar_t) );
		memcpy( self->buffer.pVoid, str->wcs, str->size * sizeof(wchar_t) );
	}
}
static int DaoBUF_CheckRange( Dao_Buffer *self, int i, int m, DaoProcess *proc )
{
	if( i*m >=0 && i*m < self->size ) return 0;
	DaoProcess_RaiseException( proc, DAO_ERROR_INDEX_OUTOFRANGE, "" );
	return 1;
}
static void DaoBUF_GetByte( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	daoint i = p[1]->xInteger.value;
	if( DaoBUF_CheckRange( self, i, sizeof(char), proc ) ) return;
	DaoProcess_PutInteger( proc, p[2]->xEnum.value ? self->buffer.pUChar[i] : self->buffer.pSChar[i] );
}
static void DaoBUF_GetShort( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	daoint i = p[1]->xInteger.value;
	if( DaoBUF_CheckRange( self, i, sizeof(short), proc ) ) return;
	DaoProcess_PutInteger( proc, p[2]->xEnum.value ? self->buffer.pUShort[i] : self->buffer.pSShort[i] );
}
static void DaoBUF_GetInt( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	daoint i = p[1]->xInteger.value;
	if( DaoBUF_CheckRange( self, i, sizeof(int), proc ) ) return;
	DaoProcess_PutInteger( proc, p[2]->xEnum.value ? self->buffer.pUInt[i] : self->buffer.pSInt[i] );
}
static void DaoBUF_GetFloat( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	if( DaoBUF_CheckRange( self, p[1]->xInteger.value, sizeof(float), proc ) ) return;
	DaoProcess_PutFloat( proc, self->buffer.pFloat[ p[1]->xInteger.value ] );
}
static void DaoBUF_GetDouble( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	if( DaoBUF_CheckRange( self, p[1]->xInteger.value, sizeof(double), proc ) ) return;
	DaoProcess_PutDouble( proc, self->buffer.pDouble[ p[1]->xInteger.value ] );
}
static void DaoBUF_SetByte( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	daoint i = p[1]->xInteger.value;
	if( DaoBUF_CheckRange( self, i, sizeof(char), proc ) ) return;
	if( p[3]->xEnum.value )
		self->buffer.pUChar[i] = (unsigned char)p[2]->xInteger.value;
	else
		self->buffer.pSChar[i] = (signed char)p[2]->xInteger.value;
}
static void DaoBUF_SetShort( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	daoint i = p[1]->xInteger.value;
	if( DaoBUF_CheckRange( self, i, sizeof(short), proc ) ) return;
	if( p[3]->xEnum.value )
		self->buffer.pUShort[i] = (unsigned short)p[2]->xInteger.value;
	else
		self->buffer.pSShort[i] = (signed short)p[2]->xInteger.value;
}
static void DaoBUF_SetInt( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	daoint i = p[1]->xInteger.value;
	if( DaoBUF_CheckRange( self, i, sizeof(int), proc ) ) return;
	if( p[3]->xEnum.value )
		self->buffer.pUInt[i] = (unsigned int)p[2]->xInteger.value;
	else
		self->buffer.pSInt[i] = (signed int)p[2]->xInteger.value;
}
static void DaoBUF_SetFloat( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	if( DaoBUF_CheckRange( self, p[1]->xInteger.value, sizeof(float), proc ) ) return;
	self->buffer.pFloat[ p[1]->xInteger.value ] = p[2]->xFloat.value;
}
static void DaoBUF_SetDouble( DaoProcess *proc, DaoValue *p[], int N )
{
	Dao_Buffer *self = (Dao_Buffer*) p[0];
	if( DaoBUF_CheckRange( self, p[1]->xInteger.value, sizeof(double), proc ) ) return;
	self->buffer.pDouble[ p[1]->xInteger.value ] = p[2]->xDouble.value;
}
static DaoFuncItem bufferMeths[]=
{
	{ DaoBUF_New,       "buffer( size=0 )=>buffer" },
	{ DaoBUF_Size,      "size( self :buffer )=>int" },
	{ DaoBUF_Resize,    "resize( self :buffer, size :int )" },
	{ DaoBUF_CopyData,  "copydata( self :buffer, buf :buffer )" },
	{ DaoBUF_GetString, "getstring( self :buffer, stype :enum<mbs, wcs> = $mbs )=>string" },
	{ DaoBUF_SetString, "setstring( self :buffer, str : string )" },
	{ DaoBUF_GetByte,   "getbyte( self :buffer, index :int, stype :enum<signed,unsigned> = $signed )=>int" },
	{ DaoBUF_GetShort,  "getshort( self :buffer, index :int, stype :enum<signed,unsigned> = $signed )=>int" },
	{ DaoBUF_GetInt,    "getint( self :buffer, index :int, stype :enum<signed,unsigned> = $signed )=>int" },
	{ DaoBUF_GetFloat,  "getfloat( self :buffer, index :int )=>float" },
	{ DaoBUF_GetDouble, "getdouble( self :buffer, index :int )=>double" },
	{ DaoBUF_SetByte,   "setbyte( self :buffer, index :int, value :int, stype :enum<signed,unsigned> = $signed)" },
	{ DaoBUF_SetShort,  "setshort( self :buffer, index :int, value :int, stype :enum<signed,unsigned> = $signed)"},
	{ DaoBUF_SetInt,    "setint( self :buffer, index :int, value :int, stype :enum<signed,unsigned> = $signed)" },
	{ DaoBUF_SetFloat,  "setfloat( self :buffer, index :int, value : float )" },
	{ DaoBUF_SetDouble, "setdouble( self :buffer, index :int, value : double )" },
	{ NULL, NULL },
};

DaoTypeBase bufferTyper =
{
	"buffer", NULL, NULL, (DaoFuncItem*) bufferMeths, {0}, {0},
	(FuncPtrDel)Dao_Buffer_Delete, NULL
};
static DaoType *daox_type_buffer = NULL;

Dao_Buffer* Dao_Buffer_New( size_t size )
{
	Dao_Buffer *self = (Dao_Buffer*) dao_malloc( sizeof(Dao_Buffer) );
	DaoCdata_InitCommon( (DaoCdata*)self, daox_type_buffer );
	self->size = self->bufsize = 0;
	self->buffer.pVoid = NULL;
	Dao_Buffer_Resize( self, size );
	return self;
}
void Dao_Buffer_Resize( Dao_Buffer *self, size_t size )
{
	self->size = size;
	if( self->size + 1 >= self->bufsize ){
		self->bufsize = self->size + self->bufsize * 0.1 + 1;
		self->buffer.pVoid = dao_realloc( self->buffer.pVoid, self->bufsize );
	}else if( self->size < self->bufsize * 0.75 ){
		self->bufsize = self->bufsize * 0.8 + 1;
		self->buffer.pVoid = dao_realloc( self->buffer.pVoid, self->bufsize );
	}
}
void Dao_Buffer_Delete( Dao_Buffer *self )
{
	DaoCdata_FreeCommon( (DaoCdata*)self );
	if( self->buffer.pVoid ) dao_free( self->buffer.pVoid );
	dao_free( self );
}

DAO_DLL int DaoSys_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	daox_type_buffer = DaoNamespace_WrapType( ns, & bufferTyper, 0 );
	DaoNamespace_WrapType( ns, & modSysCoreTyper, 1 );
	return 0;
}
