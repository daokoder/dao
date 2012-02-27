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

/* Contribution logs: */
/* 2011-02-13, Aleksey Danilov: added initial implementation. */

#include"string.h"
#include"errno.h"
#include<sys/stat.h>

#include"dao.h"

#define dao_malloc malloc
#define dao_free free
#define dao_realloc realloc

#ifdef WIN32

#include"io.h"
#ifdef _MSC_VER
#define chdir _chdir
#define rmdir _rmdir
#define getcwd _getcwd
#define mkdir _mkdir
#define stat _stat
#define chmod _chmod
#endif

#else
#include"dirent.h"
#endif

#ifdef UNIX
#include<unistd.h>
#include<sys/time.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 512
#endif

#define MAX_ERRMSG 100

struct DInode
{
	char *path;
	short type;
	time_t ctime;
	time_t mtime;
	short pread;
	short pwrite;
	short pexec;
	size_t size;
};

typedef struct DInode DInode;

DaoType *daox_type_fsnode = NULL;

DInode* DInode_New()
{
	DInode *res = (DInode*)dao_malloc( sizeof(DInode) );
	res->path = NULL;
	res->type = -1;
	return res;
}

void DInode_Close( DInode *self )
{
	if( self->path ){
		dao_free( self->path );
		self->path = NULL;
		self->type = -1;
	}
}

void DInode_Delete( DInode *self )
{
	DInode_Close( self );
	dao_free( self );
}

#ifdef WIN32
#define IS_PATH_SEP( c ) ( ( c ) == '\\' || ( c ) == '/' || ( c ) == ':' )
#define STD_PATH_SEP "\\"
#else
#define IS_PATH_SEP( c ) ( ( c ) == '/' )
#define STD_PATH_SEP "/"
#endif

int DInode_Open( DInode *self, const char *path )
{
	char buf[MAX_PATH + 1] = {0};
	struct stat info;
	int len, i;
	DInode_Close( self );
	if( !path )
		return 1;
	len = strlen( path );
	if( stat( path, &info ) != 0 )
		return errno;
	for( i = 0; i < len; i++ )
		if( path[i] == '.' && ( i == 0 || IS_PATH_SEP( path[i - 1] ) )
			&& ( i == len - 1 || IS_PATH_SEP( path[i + 1] ) ||
			( path[i + 1] == '.' && ( i == len - 2 || IS_PATH_SEP( path[i + 2] ) ) ) ) )
			return -1;
#ifdef WIN32
	if( !( info.st_mode & _S_IFDIR ) && !( info.st_mode & _S_IFREG ) )
		return 1;
	self->pread = info.st_mode & _S_IREAD;
	self->pwrite = info.st_mode & _S_IWRITE;
	self->pexec = info.st_mode & _S_IEXEC;
	self->type = ( info.st_mode & _S_IFDIR )? 0 : 1;
	self->size = ( info.st_mode & _S_IFDIR )? 0 : info.st_size;
	if( len < 2 || path[1] != ':' ){
		if( !getcwd( buf, MAX_PATH ) )
			return errno;
		strcat( buf, "\\" );
	}
#else
	if( !S_ISDIR( info.st_mode ) && !S_ISREG( info.st_mode ) )
		return 1;
	self->pread = info.st_mode & S_IRUSR;
	self->pwrite = info.st_mode & S_IWUSR;
	self->pexec = info.st_mode & S_IXUSR;
	self->type = ( S_ISDIR( info.st_mode ) )? 0 : 1;
	self->size = ( S_ISDIR( info.st_mode ) )? 0 : info.st_size;
	if( path[0] != '/' ){
		if( !getcwd( buf, MAX_PATH ) )
			return errno;
		strcat( buf, "/" );
	}
#endif
	len += strlen( buf );
	if( len > MAX_PATH )
		return ENAMETOOLONG;
	self->path = (char*)dao_malloc( len + 1 );
	strcpy( self->path, buf );
	strcat( self->path, path );
#ifndef WIN32
	if( self->path[len - 1] == '/' && len > 1 )
		self->path[len - 1] = '\0';
#endif
	self->ctime = info.st_ctime;
	self->mtime = info.st_mtime;
	return 0;
}

int DInode_Reopen( DInode *self )
{
	struct stat info;
	if( stat( self->path, &info ) != 0 )
		return errno;
#ifdef WIN32
	if( !( info.st_mode & _S_IFDIR ) && !( info.st_mode & _S_IFREG ) )
		return 1;
	self->pread = info.st_mode & _S_IREAD;
	self->pwrite = info.st_mode & _S_IWRITE;
	self->pexec = info.st_mode & _S_IEXEC;
	self->type = ( info.st_mode & _S_IFDIR )? 0 : 1;
	self->size = ( info.st_mode & _S_IFDIR )? 0 : info.st_size;
#else
	if( !S_ISDIR( info.st_mode ) && !S_ISREG( info.st_mode ) )
		return 1;
	self->pread = info.st_mode & S_IRUSR;
	self->pwrite = info.st_mode & S_IWUSR;
	self->pexec = info.st_mode & S_IXUSR;
	self->type = ( S_ISDIR( info.st_mode ) )? 0 : 1;
	self->size = ( S_ISDIR( info.st_mode ) )? 0 : info.st_size;
#endif
	self->ctime = info.st_ctime;
	self->mtime = info.st_mtime;
	return 0;
}

char* DInode_Parent( DInode *self, char *buffer )
{
	int i;
	if( !self->path )
		return NULL;
	for (i = strlen( self->path ) - 1; i >= 0; i--)
		if( IS_PATH_SEP( self->path[i] ) )
			break;
	if( self->path[i + 1] == '\0' )
		return NULL;
#ifdef WIN32
	if( self->path[i] == ':' ){
		strncpy( buffer, self->path, i + 1 );
		buffer[i + 1] = '\\';
		buffer[i + 2] = '\0';
	}
	else{
		if( i == 2 )
			i++;
		strncpy( buffer, self->path, i );
		buffer[i] = '\0';
	}
#else
	if( i == 0 )
		strcpy( buffer, "/" );
	else{
		strncpy( buffer, self->path, i );
		buffer[i] = '\0';
	}
#endif
	return buffer;
}

int DInode_Rename( DInode *self, const char *path )
{
	char buf[MAX_PATH + 1] = {0};
	int i, len;
	if( !self->path )
		return 1;
	len = strlen( path );
	for( i = 0; i < len; i++ )
		if( path[i] == '.' && ( i == 0 || IS_PATH_SEP( path[i - 1] ) )
			&& ( i == len - 1 || IS_PATH_SEP( path[i + 1] ) ||
			( path[i + 1] == '.' && ( i == len - 2 || IS_PATH_SEP( path[i + 2] ) ) ) ) )
			return -1;
	if( !DInode_Parent( self, buf ) )
		return 1;
#ifdef WIN32
	if( len < 2 || path[1] != ':' ){
#else
	if( path[0] != '/' ){
#endif
		strcat( buf, STD_PATH_SEP );
		len += strlen( buf );
		if( len > MAX_PATH )
			return ENAMETOOLONG;
		strcat( buf, path );
	}else{
		if( len > MAX_PATH )
			return ENAMETOOLONG;
		strcpy( buf, path );
	}
	if( rename( self->path, buf ) != 0 )
		return errno;
	self->path = (char*)dao_realloc( self->path, len + 1 );
	strcpy( self->path, buf );
	return 0;
}

int DInode_Remove( DInode *self )
{
	if( !self->path )
		return 1;
	if( self->type == 0 ){
		if( rmdir( self->path ) != 0 )
			return errno;
	}else{
		if( unlink( self->path ) != 0 )
			return errno;
	}
	return 0;
}

int DInode_Append( DInode *self, const char *path, int dir, DInode *dest )
{
	int i, len;
	char buf[MAX_PATH + 1];
	FILE *handle;
	struct stat info;
	if( !self->path || self->type != 0 || !dest )
		return 1;
	len = strlen( path );
	for( i = 0; i < len; i++ )
		if( path[i] == '.' && ( i == 0 || IS_PATH_SEP( path[i - 1] ) )
			&& ( i == len - 1 || IS_PATH_SEP( path[i + 1] ) ||
			( path[i + 1] == '.' && ( i == len - 2 || IS_PATH_SEP( path[i + 2] ) ) ) ) )
			return -1;
	if( DInode_Parent( self, buf ) ){
		strcpy( buf, self->path );
		strcat( buf, STD_PATH_SEP );
	}
	else
		strcpy( buf, self->path );
	if( strlen( buf ) + len > MAX_PATH )
		return ENAMETOOLONG;
	strcat( buf, path );
#ifdef WIN32
	if( stat( buf, &info ) == 0 && ( ( dir && ( info.st_mode & _S_IFDIR ) )
		|| ( !dir && ( info.st_mode & _S_IFREG ) ) ) )
		return DInode_Open( dest, buf );
#else
	if( stat( buf, &info ) == 0 && ( ( dir && S_ISDIR( info.st_mode ) )
		|| ( !dir && S_ISREG( info.st_mode ) ) ) )
		return DInode_Open( dest, buf );
#endif
	if( !dir ){
		if( !( handle = fopen( buf, "w" ) ) )
			return ( errno == EINVAL ) ? 1 : errno;
		fclose( handle );
	}else{
#ifdef WIN32
	if( mkdir( buf ) != 0 )
		return ( errno == EINVAL ) ? 1 : errno;
#else
	if( mkdir( buf, S_IRWXU|S_IRGRP|S_IXGRP|S_IXOTH ) != 0 )
		return ( errno == EINVAL ) ? 1 : errno;
#endif
	}
	return DInode_Open( dest, buf );
}

extern DaoTypeBase fsnodeTyper;

int DInode_ChildrenRegex( DInode *self, int type, DaoProcess *proc, DaoList *dest, DaoRegex *pattern )
{
	DaoFactory *factory = DaoProcess_GetFactory( proc );
	char buffer[MAX_PATH + 1];
	int len, res;
#ifdef WIN32
	intptr_t handle;
	struct _finddata_t finfo;
#else
	DIR *handle;
	struct dirent *finfo;
#endif
	if( !self->path || self->type != 0 )
		return 1;
    strcpy( buffer, self->path );
	len = strlen( buffer );
#ifdef WIN32
	/* Using _findfirst/_findnext for Windows */
	if( IS_PATH_SEP( buffer[len - 1] ) )
    	strcpy( buffer + len, "*" );
    else
		strcpy( buffer + len++, "\\*" );
	handle = _findfirst( buffer, &finfo );
	if (handle != -1){
		DString *str = DString_New( 1 );
		DaoValue *value;
		DInode *fsnode;
		do
			if( strcmp( finfo.name, "." ) && strcmp( finfo.name, ".." ) ){
				DString_SetDataMBS( str, finfo.name, strlen(finfo.name) );
				strcpy( buffer + len, finfo.name );
				fsnode = DInode_New();
				if( ( res = DInode_Open( fsnode, buffer ) ) != 0 ){
					DInode_Delete( fsnode );
					return res;
				}
				if( ( fsnode->type == type || type == 2 ) && DaoRegex_Match( pattern, str, NULL, NULL ) ){
					value = (DaoValue*) DaoFactory_NewCdata( factory, daox_type_fsnode, fsnode, 1 );
					DaoList_PushBack( dest, value );
				}
				else
					DInode_Delete( fsnode );
			}
		while( !_findnext( handle, &finfo ) );
		DString_Delete( str );
		_findclose( handle );
	}
#else
	/* Using POSIX opendir/readdir otherwise */
	handle = opendir( buffer );
	if( !IS_PATH_SEP( buffer[len - 1] ) )
		strcpy( buffer + len++,  "/");
	if( handle ){
		DString *str = DString_New( 1 );
		DaoValue *value;
		DInode *fsnode;
		while( ( finfo = readdir( handle ) ) )
			if( strcmp( finfo->d_name, "." ) && strcmp( finfo->d_name, ".." ) ){
				DString_SetDataMBS( str, finfo->d_name, strlen(finfo->d_name) );
				strcpy( buffer + len, finfo->d_name );
				fsnode = DInode_New();
				if( ( res = DInode_Open( fsnode, buffer ) ) != 0 ){
					DInode_Delete( fsnode );
					return res;
				}
				if( ( fsnode->type == type || type == 2 ) && DaoRegex_Match( pattern, str, NULL, NULL ) ){
					value = (DaoValue*) DaoFactory_NewCdata( factory, daox_type_fsnode, fsnode, 1 );
					DaoList_PushBack( dest, value );
				}
				else
					DInode_Delete( fsnode );
			}
		DString_Delete( str );
		closedir( handle );
	}
#endif
	else
		return errno;
	return 0;
}

int DInode_SetAccess(DInode *self, int mode)
{
#ifdef WIN32
	if (chmod(self->path, ((mode & 1)? _S_IREAD : 0) | ((mode & 2)? _S_IWRITE : 0)))
#else
	if (chmod(self->path, ((mode & 1)? S_IREAD : 0) | ((mode & 2)? S_IWRITE : 0) | ((mode & 4)? S_IEXEC : 0)))
#endif
		return errno;
	return 0;
}

static void GetErrorMessage( char *buffer, int code, int special )
{
	switch ( code ){
	case EACCES:
	case EBADF:
		strcpy( buffer, "Access not permitted (EACCES/EBADF)");
		break;
	case EBUSY:
		strcpy (buffer, "The fsnode's path is being used (EBUSY)" );
		break;
	case ENOTEMPTY:
	case EEXIST:
		strcpy( buffer, special? "The directory is not empty (ENOTEMPTY/EEXIST)" : "The fsnode already exists (EEXIST/ENOTEMPTY)" );
		break;
	case EPERM:
	case ENOTDIR:
	case EISDIR:
		strcat( buffer, "Inconsistent type of the file object(s) (EPERM/ENOTDIR/EISDIR)" );
		break;
	case EINVAL:
		strcpy( buffer, special? "The fsnode's path does not exist (EINVAL)" : "Trying to make the directory its own subdirectory (EINVAL)" );
		break;
	case EMLINK:
		strcat( buffer, "Trying to create too many entries in the parent directory (EMLINK)" );
		break;
	case ENOENT:
		strcpy( buffer, "The fsnode's path does not exist (ENOENT)" );
		break;
	case ENOSPC:
		strcpy( buffer, "No space for a new entry in the file system (ENOSPC)" );
		break;
	case EROFS:
		strcpy( buffer, "Trying to write to a read-only file system (EROFS)" );
		break;
	case EXDEV:
		strcpy( buffer, "Trying to relocate the fsnode to a different file system (EXDEV)" );
		break;
	case ENAMETOOLONG:
		strcpy( buffer, "The fsnode's path is too long (ENAMETOOLONG)" );
		break;
	case EMFILE:
	case ENFILE:
		strcpy( buffer, "Too many files open (EMFILE/ENFILE)" );
		break;
	case ENOMEM:
		strcpy( buffer, "Not enough memory (ENOMEM)" );
		break;
	default:
		sprintf( buffer, "Unknown system error (%x)", code );
	}
}

static void FSNode_Update( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	char errbuf[MAX_PATH + 1];
	int res;
	if( ( res = DInode_Reopen( self ) ) != 0 ){
		if( res == 1 )
			strcpy( errbuf, "Trying to open something which is not a file/directory" );
		else
			GetErrorMessage( errbuf, res, 0 );
		if( res == 1 || res == ENOENT )
			snprintf( errbuf + strlen( errbuf ), 256, ": %s", self->path );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
	}
}

static void FSNode_Path( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutMBString( proc, self->path );
}

static void FSNode_Name( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	int i;
	for (i = strlen( self->path ) - 1; i >= 0; i--)
		if( IS_PATH_SEP( self->path[i] ) )
			break;
	if( self->path[i + 1] == '\0' )
		DaoProcess_PutMBString( proc, self->path );
	else
		DaoProcess_PutMBString( proc, self->path + i + 1 );
}

static void FSNode_Parent( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode *par;
	char path[MAX_PATH + 1];
	int res = 0;
	par = DInode_New();
	if( !DInode_Parent( self, path ) || ( res = DInode_Open( par, path ) ) != 0 ){
		DInode_Delete( par );
		if( res == 0 )
			strcpy( path, "The fsnode has no parent" );
		else
			GetErrorMessage( path, res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, path );
		return;
	}
	DaoProcess_PutCdata( proc, (void*)par, daox_type_fsnode );
}

static void FSNode_Type( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutEnum( proc, self->type == 1 ? "file" : "dir" );
}

static void FSNode_Size( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutInteger( proc, self->size );
}

static void FSNode_Rename( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	int res;
	char errbuf[MAX_ERRMSG];
	if ( (res = DInode_Rename( self, DaoValue_TryGetMBString( p[1] ) ) ) != 0 ){
		if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else if( res == 1 )
			strcpy( errbuf, "Trying to rename root fsnode" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
}

static void FSNode_Remove( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	int res;
	char errbuf[MAX_ERRMSG];
	if ( (res = DInode_Remove( self ) ) != 0 ){
		GetErrorMessage( errbuf, res, self->type == 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
}

static void FSNode_Ctime( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutInteger( proc, self->ctime );
}

static void FSNode_Mtime( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutInteger( proc, self->mtime );
}

static void FSNode_Access( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	char res[20] = {0};
	if( self->pread )
		strcat( res, "$read" );
	if( self->pwrite )
		strcat( res, "$write" );
	if( self->pexec )
		strcat( res, "$execute" );
	DaoProcess_PutEnum( proc, res );
}

static void FSNode_SetAccess(DaoProcess *proc, DaoValue *p[], int N)
{
	DInode *self = (DInode*)DaoValue_TryGetCdata(p[0]);
	char errbuf[MAX_ERRMSG];
	int res = DInode_SetAccess(self, DaoValue_TryGetEnum(p[1]));
	if (res){
		GetErrorMessage(errbuf, res, 0);
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
	}
	else
		FSNode_Update(proc, p, N);
}

static void FSNode_Makefile( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode *child;
	char errbuf[MAX_ERRMSG];
	int res;
	if( self->type != 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The fsnode is not a directory" );
		return;
	}
	child = DInode_New();
	if( ( res = DInode_Append( self, DaoValue_TryGetMBString( p[1] ), 0, child ) ) != 0 ){
		DInode_Delete( child );
		if( res == 1 )
			strcpy( errbuf, "The fsnode's name is invalid (EINVAL)" );
		else if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	DaoProcess_PutCdata( proc, (void*)child, daox_type_fsnode );
}

static void FSNode_Makedir( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode *child;
	char errbuf[MAX_ERRMSG];
	int res;
	if( self->type != 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The fsnode is not a directory" );
		return;
	}
	child = DInode_New();
	if( ( res = DInode_Append( self, DaoValue_TryGetMBString( p[1] ), 1, child ) ) != 0 ){
		DInode_Delete( child );
		if( res == 1 )
			strcpy( errbuf, "The fsnode's name is invalid (EINVAL)" );
		else if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	DaoProcess_PutCdata( proc, (void*)child, daox_type_fsnode );
}

static void FSNode_Isroot( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutInteger( proc, IS_PATH_SEP( self->path[strlen( self->path ) - 1] ) ? 1 : 0 );
}

static void FSNode_Child( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode *child;
	char path[MAX_PATH + 1], *str;
	int res;
	if( self->type != 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The fsnode is not a directory" );
		return;
	}
	child = DInode_New();
	strcpy( path, self->path );
	strcat( path, STD_PATH_SEP );
	str = DaoValue_TryGetMBString( p[1] );
	if( strlen( path ) + strlen( str ) > MAX_PATH ){
		GetErrorMessage( path, ENAMETOOLONG, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, path );
		return;
	}
	strcat( path, str );
	if( ( res = DInode_Open( child, path ) ) != 0 ){
		DInode_Delete( child );
		if( res == 1 )
			strcpy( path, "The fsnode is not a directory" );
		else if( res == -1 )
			strcpy( path, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( path, res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, path );
		return;                                  
	}
	DaoProcess_PutCdata( proc, (void*)child, daox_type_fsnode );
}

static void DInode_Children( DInode *self, DaoProcess *proc, int type, DString *pat, int ft )
{
	DaoList *list = DaoProcess_PutList( proc );
	char buffer[MAX_PATH + 1], *filter;
	int res, i, j, len;
	DString *strbuf;
	DaoRegex *pattern;
	if( self->type != 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The fsnode is not a directory" );
		return;
	}
	filter = DString_GetMBS( pat );
	len = strlen( filter );
	if( len > MAX_PATH ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The filter is too large" );
		return;
	}
	if( ft == 0 ){
		buffer[0] = '^';
		buffer[1] = '(';
		for( i = 0, j = 2; i < len && j < MAX_PATH - 1; i++, j++ )
			switch( filter[i] )
			{
			case '?':
				buffer[j] = '.';
				break;
			case '*':
				buffer[j++] = '.';
				buffer[j] = '*';
				break;
			case ';':
				buffer[j] = '|';
				break;
			case '{':
			case '}':
			case '(':
			case ')':
			case '%':
			case '.':
			case '|':
			case '$':
			case '^':
			case '[':
			case ']':
			case '+':
			case '-':
			case '<':
			case '>':
				buffer[j++] = '%';
				buffer[j] = filter[i];
				break;
			default:
				buffer[j] = filter[i];
			}
		if( j >= MAX_PATH - 1 ){
			DaoProcess_RaiseException( proc, DAO_ERROR, "The filter is too large" );
			return;
		}
		buffer[j] = ')';
		buffer[j + 1] = '$';
		buffer[j + 2] = '\0';
	}
	else
		strcpy( buffer, filter );
	strbuf = DString_New( 1 );
	DString_SetMBS( strbuf, buffer );
	pattern = DaoProcess_MakeRegex( proc, strbuf, 1 );
    DString_Delete( strbuf );
    if( !pattern )
    	return;
	type = ( type == 3 ) ? 2 : ( ( type == 1 ) ? 1 : 0 );
	if( ( res = DInode_ChildrenRegex( self, type, proc, list, pattern ) ) != 0 ){
		GetErrorMessage( buffer, res, 1 );
		DaoProcess_RaiseException( proc, DAO_ERROR, buffer );
		return;
	}
}

static void FSNode_Children( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode_Children( self, proc, DaoValue_TryGetEnum( p[1] ), DaoString_Get( DaoValue_CastString( p[2] ) ),
					 DaoValue_TryGetEnum( p[3] ) );
}

static void FSNode_Files( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode_Children( self, proc, 1, DaoString_Get( DaoValue_CastString( p[1] ) ), DaoValue_TryGetEnum( p[2] ) );
}

static void FSNode_Dirs( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode_Children( self, proc, 2, DaoString_Get( DaoValue_CastString( p[1] ) ), DaoValue_TryGetEnum( p[2] ) );
}

static void FSNode_Suffix( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	char *pos;
	pos = strrchr( self->path, '.' );
	DaoProcess_PutMBString( proc, pos? pos + 1 : "" );
}

static void FSNode_New( DaoProcess *proc, DaoValue *p[], int N )
{
	char errbuf[MAX_ERRMSG + 256];
	DInode *fsnode = DInode_New();
	int res;
	char *path = DaoValue_TryGetMBString( p[0] );
	if( ( res = DInode_Open( fsnode, path ) ) != 0 ){
		if( res == 1 )
			strcpy( errbuf, "Trying to open something which is not a file/directory" );
		else if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( errbuf, res, 0 );
		if( res == 1 || res == ENOENT )
			snprintf( errbuf + strlen( errbuf ), 256, ": %s", path );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	DaoProcess_PutCdata( proc, (void*)fsnode, daox_type_fsnode );
}

static void FSNode_GetCWD( DaoProcess *proc, DaoValue *p[], int N )
{
	char buf[MAX_PATH + 1];
	int res = 0;
	DInode *fsnode = DInode_New();
	if( !getcwd( buf, MAX_PATH ) || ( res = DInode_Open( fsnode, buf ) ) != 0 ){
		DInode_Delete( fsnode );
		GetErrorMessage( buf, ( res == 0 )? errno : res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, buf );
		return;
	}
	DaoProcess_PutCdata( proc, (void*)fsnode, daox_type_fsnode );
}

static void FSNode_SetCWD( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *fsnode = (DInode*)DaoValue_TryGetCdata( p[0] );
	char errbuf[MAX_PATH + 1];
	if( fsnode->type != 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The fsnode is not a directory" );
		return;
	}
	if( chdir( fsnode->path ) != 0 ){
		GetErrorMessage( errbuf, errno, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
}

static DaoFuncItem fsnodeMeths[] =
{
	{ FSNode_New,      "fsnode( path : string )=>fsnode" },
	{ FSNode_Path,     "path( self : fsnode )=>string" },
	{ FSNode_Name,     "name( self : fsnode )=>string" },
	{ FSNode_Type,     "type( self : fsnode )=>enum<file, dir>" },
	{ FSNode_Size,     "size( self : fsnode )=>int" },
	{ FSNode_Suffix,   "suffix( self: fsnode )=>string" },
	{ FSNode_Parent,   "parent( self : fsnode )=>fsnode" },
	{ FSNode_Ctime,    "ctime( self : fsnode )=>int" },
	{ FSNode_Mtime,    "mtime( self : fsnode )=>int" },
	{ FSNode_Access,   "access( self : fsnode )=>enum<read; write; execute>" },
	{ FSNode_SetAccess,"set_access( self : fsnode, mode: enum<read; write; execute>)" },
	{ FSNode_Rename,   "rename( self : fsnode, path : string )" },
	{ FSNode_Remove,   "remove( self : fsnode )" },
	{ FSNode_Makefile, "mkfile( self : fsnode, path : string )=>fsnode" },
	{ FSNode_Makedir,  "mkdir( self : fsnode, path : string )=>fsnode" },
	{ FSNode_Isroot,   "isroot( self : fsnode )=>int" },
	{ FSNode_Children, "children( self : fsnode, type : enum<files; dirs>, filter='*', filtering : enum<wildcard, regex> = $wildcard )=>list<fsnode>" },
	{ FSNode_Files,    "files( self : fsnode, filter='*', filtering : enum<wildcard, regex> = $wildcard )=>list<fsnode>" },
	{ FSNode_Dirs,     "dirs( self : fsnode, filter='*', filtering : enum<wildcard, regex> = $wildcard )=>list<fsnode>" },
	{ FSNode_Child,    "[]( self : fsnode, path : string )=>fsnode" },
	{ FSNode_GetCWD,   "cwd(  )=>fsnode" },
	{ FSNode_SetCWD,   "set_cwd( self : fsnode )" },
	{ FSNode_Update,   "update( self : fsnode )" },
	{ NULL, NULL }
};

DaoTypeBase fsnodeTyper = {
	"fsnode", NULL, NULL, fsnodeMeths, {NULL}, {0}, (FuncPtrDel)DInode_Delete, NULL
};

#ifdef DAO_INLINE_FSNODE
int DaoFSNode_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
#else
int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
#endif
{
	daox_type_fsnode = DaoNamespace_WrapType( ns, & fsnodeTyper, 1 );
	return 0;
}
