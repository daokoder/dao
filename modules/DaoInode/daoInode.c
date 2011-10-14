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
DAO_INIT_MODULE

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
};

typedef struct DInode DInode;

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
	DInode_Close( self );
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

extern DaoTypeBase inodeTyper;

int DInode_ChildrenRegex( DInode *self, int type, DaoList *dest, DaoRegex *pattern )
{
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
		DInode *inode;
		do
			if( strcmp( finfo.name, "." ) && strcmp( finfo.name, ".." ) ){
				DString_SetDataMBS( str, finfo.name, strlen(finfo.name) );
				strcpy( buffer + len, finfo.name );
				inode = DInode_New();
				if( ( res = DInode_Open( inode, buffer ) ) != 0 ){
					DInode_Delete( inode );
					return res;
				}
				if( ( inode->type == type || type == 2 ) && DaoRegex_Match( pattern, str, NULL, NULL ) ){
					value = DaoValue_NewCdata( &inodeTyper, inode );
					DaoList_PushBack( dest, value );
				}
				else
					DInode_Delete( inode );
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
		DInode *inode;
		while( ( finfo = readdir( handle ) ) )
			if( strcmp( finfo->d_name, "." ) && strcmp( finfo->d_name, ".." ) ){
				DString_SetDataMBS( str, finfo->d_name, strlen(finfo->d_name) );
				strcpy( buffer + len, finfo->d_name );
				inode = DInode_New();
				if( ( res = DInode_Open( inode, buffer ) ) != 0 ){
					DInode_Delete( inode );
					return res;
				}
				if( ( inode->type == type || type == 2 ) && DaoRegex_Match( pattern, str, NULL, NULL ) ){
					value = DaoValue_NewCdata( &inodeTyper, inode );
					DaoList_PushBack( dest, value );
				}
				else
					DInode_Delete( inode );
			}
		DString_Delete( str );
		closedir( handle );
	}
#endif
	else
		return errno;
	return 0;
}

static void GetErrorMessage( char *buffer, int code, int special )
{
	switch ( code ){
	case EACCES:
	case EBADF:
		strcpy( buffer, "Access permission required (EACCES/EBADF)");
		break;
	case EBUSY:
		strcpy (buffer, "The inode's path is being used (EBUSY)" );
		break;
	case ENOTEMPTY:
	case EEXIST:
		strcpy( buffer, special? "The directory is not empty (ENOTEMPTY/EEXIST)" : "The inode already exists (EEXIST/ENOTEMPTY)" );
		break;
	case EPERM:
	case ENOTDIR:
	case EISDIR:
		strcat( buffer, "Inconsistent type of the file object(s) (EPERM/ENOTDIR/EISDIR)" );
		break;
	case EINVAL:
		strcpy( buffer, special? "The inode's path does not exist (EINVAL)" : "Trying to make the directory its own subdirectory (EINVAL)" );
		break;
	case EMLINK:
		strcat( buffer, "Trying to create too many entries in the parent directory (EMLINK)" );
		break;
	case ENOENT:
		strcpy( buffer, "The inode's path does not exist (ENOENT)" );
		break;
	case ENOSPC:
		strcpy( buffer, "No space for a new entry in the file system (ENOSPC)" );
		break;
	case EROFS:
		strcpy( buffer, "Trying to write to a read-only file system (EROFS)" );
		break;
	case EXDEV:
		strcpy( buffer, "Trying to relocate the inode to a different file system (EXDEV)" );
		break;
	case ENAMETOOLONG:
		strcpy( buffer, "The inode's path is too long (ENAMETOOLONG)" );
		break;
	case EMFILE:
	case ENFILE:
		strcpy( buffer, "Too many inodes open (EMFILE/ENFILE)" );
		break;
	case ENOMEM:
		strcpy( buffer, "Not enough memory (ENOMEM)" );
		break;
	default:
		sprintf( buffer, "Unknown system error (%x)", code );
	}
}

static void DaoInode_Open( DaoProcess *proc, DaoValue *p[], int N )
{
	char errbuf[MAX_ERRMSG];
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	int res;
	if( ( res = DInode_Open( self, DaoValue_TryGetMBString( p[1] ) ) ) != 0 ){
		if( res == 1 )
			strcpy( errbuf, "Trying to open something which is not a file/directory" );
		else if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
}

static void DaoInode_Isopen( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_PutInteger( proc, ((DInode*)DaoValue_TryGetCdata( p[0] ))->path ? 1 : 0 );
}

static void DaoInode_Close( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode_Close( self );
}

static void DaoInode_Path( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	DaoProcess_PutMBString( proc, self->path );
}

static void DaoInode_Name( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	int i;
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	for (i = strlen( self->path ) - 1; i >= 0; i--)
		if( IS_PATH_SEP( self->path[i] ) )
			break;
	if( self->path[i + 1] == '\0' )
		DaoProcess_PutMBString( proc, self->path );
	else
		DaoProcess_PutMBString( proc, self->path + i + 1 );
}

static void DaoInode_Parent( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode *par;
	char path[MAX_PATH + 1];
	int res = 0;
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	par = DInode_New();
	if( !DInode_Parent( self, path ) || ( res = DInode_Open( par, path ) ) != 0 ){
		DInode_Delete( par );
		if( res == 0 )
			strcpy( path, "The inode has no parent" );
		else
			GetErrorMessage( path, res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, path );
		return;
	}
	DaoProcess_PutCdata( proc, (void*)par, &inodeTyper );
}

static void DaoInode_Type( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	DaoProcess_PutEnum( proc, self->type == 1 ? "file" : "dir" );
}

static void DaoInode_Rename( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	int res;
	char errbuf[MAX_ERRMSG];
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	if ( (res = DInode_Rename( self, DaoValue_TryGetMBString( p[1] ) ) ) != 0 ){
		if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else if( res == 1 )
			strcpy( errbuf, "Trying to rename root inode" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
}

static void DaoInode_Remove( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	int res;
	char errbuf[MAX_ERRMSG];
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	if ( (res = DInode_Remove( self ) ) != 0 ){
		GetErrorMessage( errbuf, res, self->type == 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
}

static void DaoInode_Ctime( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	DaoProcess_PutInteger( proc, self->ctime );
}

static void DaoInode_Mtime( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	DaoProcess_PutInteger( proc, self->mtime );
}

static void DaoInode_Access( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	char res[20] = {0};
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	if( self->pread )
		strcat( res, "$read" );
	if( self->pwrite )
		strcat( res, "$write" );
	if( self->pexec )
		strcat( res, "$execute" );
	DaoProcess_PutEnum( proc, res );
}

static void DaoInode_Makefile( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode *child;
	char errbuf[MAX_ERRMSG];
	int res;
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	if( self->type != 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not a directory" );
		return;
	}
	child = DInode_New();
	if( ( res = DInode_Append( self, DaoValue_TryGetMBString( p[1] ), 0, child ) ) != 0 ){
		DInode_Delete( child );
		if( res == 1 )
			strcpy( errbuf, "The inode's name is invalid (EINVAL)" );
		else if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	DaoProcess_PutCdata( proc, (void*)child, &inodeTyper );
}

static void DaoInode_Makedir( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode *child;
	char errbuf[MAX_ERRMSG];
	int res;
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	if( self->type != 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not a directory" );
		return;
	}
	child = DInode_New();
	if( ( res = DInode_Append( self, DaoValue_TryGetMBString( p[1] ), 1, child ) ) != 0 ){
		DInode_Delete( child );
		if( res == 1 )
			strcpy( errbuf, "The inode's name is invalid (EINVAL)" );
		else if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	DaoProcess_PutCdata( proc, (void*)child, &inodeTyper );
}

static void DaoInode_Isroot( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	if( self->path == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	DaoProcess_PutInteger( proc, IS_PATH_SEP( self->path[strlen( self->path ) - 1] ) ? 1 : 0 );
}

static void DaoInode_Child( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode *child;
	char path[MAX_PATH + 1], *str;
	int res;
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	if( self->type != 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not a directory" );
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
			strcpy( path, "The inode is not a directory" );
		else if( res == -1 )
			strcpy( path, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( path, res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, path );
		return;                                  
	}
	DaoProcess_PutCdata( proc, (void*)child, &inodeTyper );
}

static void DInode_Children( DInode *self, DaoProcess *proc, int type, DString *pat, int ft )
{
	DaoList *list = DaoProcess_PutList( proc );
	char buffer[MAX_PATH + 1], *filter;
	int res, i, j, len;
	DString *strbuf;
	DaoRegex *pattern;
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	if( self->type != 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not a directory" );
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
	if( ( res = DInode_ChildrenRegex( self, type, list, pattern ) ) != 0 ){
		GetErrorMessage( buffer, res, 1 );
		DaoProcess_RaiseException( proc, DAO_ERROR, buffer );
		return;
	}
}

static void DaoInode_Children( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode_Children( self, proc, DaoValue_TryGetEnum( p[1] ), DaoString_Get( DaoValue_CastString( p[2] ) ),
					 DaoValue_TryGetEnum( p[3] ) );
}

static void DaoInode_Files( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode_Children( self, proc, 1, DaoString_Get( DaoValue_CastString( p[1] ) ), DaoValue_TryGetEnum( p[2] ) );
}

static void DaoInode_Dirs( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	DInode_Children( self, proc, 2, DaoString_Get( DaoValue_CastString( p[1] ) ), DaoValue_TryGetEnum( p[2] ) );
}

static void DaoInode_Suffix( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *self = (DInode*)DaoValue_TryGetCdata( p[0] );
	char *pos;
	if( !self->path ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open!" );
		return;
	}
	pos = strrchr( self->path, '.' );
	DaoProcess_PutMBString( proc, pos? pos + 1 : "" );
}

static void DaoInode_New( DaoProcess *proc, DaoValue *p[], int N )
{
	char errbuf[MAX_ERRMSG];
	DInode *inode = DInode_New();
	int res;
	if( ( res = DInode_Open( inode, DaoString_GetMBS( DaoValue_CastString( p[0] ) ) ) ) != 0 ){
		if( res == 1 )
			strcpy( errbuf, "Trying to open something which is not a file/directory" );
		else if( res == -1 )
			strcpy( errbuf, "'.' and '..' entries in path are not allowed" );
		else
			GetErrorMessage( errbuf, res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
	DaoProcess_PutCdata( proc, (void*)inode, &inodeTyper );
}

static void DaoInode_GetCWD( DaoProcess *proc, DaoValue *p[], int N )
{
	char buf[MAX_PATH + 1];
	int res = 0;
	DInode *inode = DInode_New();
	if( !getcwd( buf, MAX_PATH ) || ( res = DInode_Open( inode, buf ) ) != 0 ){
		DInode_Delete( inode );
		GetErrorMessage( buf, ( res == 0 )? errno : res, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, buf );
		return;
	}
	DaoProcess_PutCdata( proc, (void*)inode, &inodeTyper );
}

static void DaoInode_SetCWD( DaoProcess *proc, DaoValue *p[], int N )
{
	DInode *inode = (DInode*)DaoValue_TryGetCdata( p[0] );
	char errbuf[MAX_PATH + 1];
	if( inode->path == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not open" );
		return;
	}
	if( inode->type != 0 ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "The inode is not a directory" );
		return;
	}
	if( chdir( inode->path ) != 0 ){
		GetErrorMessage( errbuf, errno, 0 );
		DaoProcess_RaiseException( proc, DAO_ERROR, errbuf );
		return;
	}
}

static DaoFuncItem inodeMeths[] =
{
	{ DaoInode_New,      "inode( path : string )=>inode" },
	{ DaoInode_Open,     "open( self : inode, path : string )" },
	{ DaoInode_Isopen,   "isopen( self : inode )=>int" },
	{ DaoInode_Close,    "close( self : inode )" },
	{ DaoInode_Path,     "path( self : inode )=>string" },
	{ DaoInode_Name,     "name( self : inode )=>string" },
	{ DaoInode_Type,     "type( self : inode )=>enum<file, dir>" },
	{ DaoInode_Suffix,   "suffix( self: inode )=>string" },
	{ DaoInode_Parent,   "parent( self : inode )=>inode" },
	{ DaoInode_Ctime,    "ctime( self : inode )=>int" },
	{ DaoInode_Mtime,    "mtime( self : inode )=>int" },
	{ DaoInode_Access,   "access( self : inode )=>enum<read; write; execute>" },
	{ DaoInode_Rename,   "rename( self : inode, path : string )" },
	{ DaoInode_Remove,   "remove( self : inode )" },
	{ DaoInode_Makefile, "makefile( self : inode, path : string )=>inode" },
	{ DaoInode_Makedir,  "makedir( self : inode, path : string )=>inode" },
	{ DaoInode_Isroot,   "isroot( self : inode )=>int" },
	{ DaoInode_Children, "children( self : inode, type : enum<files; dirs>, filter='*', filtering : enum<wildcard, regex> = $wildcard )=>list<inode>" },
	{ DaoInode_Files,    "files( self : inode, filter='*', filtering : enum<wildcard, regex> = $wildcard )=>list<inode>" },
	{ DaoInode_Dirs,     "dirs( self : inode, filter='*', filtering : enum<wildcard, regex> = $wildcard )=>list<inode>" },
	{ DaoInode_Child,    "[]( self : inode, path : string )=>inode" },
	{ DaoInode_GetCWD,   "getcwd(  )=>inode" },
	{ DaoInode_SetCWD,   "setcwd( self : inode )" },
	{ NULL, NULL }
};

DaoTypeBase inodeTyper = {
	"inode", NULL, NULL, inodeMeths, {NULL}, {0}, (FuncPtrDel)DInode_Delete, NULL
};

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace_WrapType( ns, & inodeTyper );
	return 0;
}
