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

#include "stdio.h"
#include "errno.h"

#include"dao.h"
DAO_INIT_MODULE

void JSON_Indent( DString *text, int indent )
{
	int i;
	for( i = 0; i < indent; i++ )
		DString_AppendWChar( text, L'\t' );
}

int JSON_SerializeValue( DaoValue *value, DString *text, int indent )
{
	int i, res;
	wchar_t buf[100];
	wchar_t *sep = indent >= 0? L",\n" : L",";
	DaoList *list;
	DaoMap *map;
	DNode *node;
	switch( DaoValue_Type( value ) ){
	case DAO_INTEGER:
		swprintf( buf,
#ifndef __MINGW32__
				  sizeof(buf),
#endif
				  sizeof(dint) == 4? L"%i" : L"%lli", DaoValue_TryGetInteger( value ) );
		DString_AppendWCS( text, buf );
		break;
	case DAO_FLOAT:
		swprintf( buf,
#ifndef __MINGW32__
				  sizeof(buf),
#endif
				  L"%f", DaoValue_TryGetFloat( value ) );
		DString_AppendWCS( text, buf );
		break;
	case DAO_DOUBLE:
		swprintf( buf,
#ifndef __MINGW32__
				  sizeof(buf),
#endif
				  L"%f", DaoValue_TryGetDouble( value ) );
		DString_AppendWCS( text, buf );
		break;
	case DAO_STRING:
		DString_AppendWChar( text, L'"' );
		DString_AppendWCS( text, DaoValue_TryGetWCString( value ) );
		DString_AppendWChar( text, L'"' );
		break;
	case DAO_LIST:
		if( indent >= 0 ){
			DString_AppendWCS( text, L"[\n" );
			indent++;
		}
		else
			DString_AppendWCS( text, L"[" );
		list = DaoValue_CastList( value );
		for( i = 0; i < DaoList_Size( list ); i++ ){
			JSON_Indent( text, indent );
			if( ( res = JSON_SerializeValue( DaoList_GetItem( list, i ) , text, indent ) ) != 0 )
				return res;
			if( i != DaoList_Size( list ) - 1 )
				DString_AppendWCS( text, sep );
			else if( indent >= 0 )
				DString_AppendWCS( text, L"\n" );
		}
		if( indent > 0 )
			indent--;
		JSON_Indent( text, indent );
		DString_AppendWCS( text, L"]");
		break;
	case DAO_MAP:
		if( indent >= 0 ){
			DString_AppendWCS( text, L"{\n" );
			indent++;
		}
		else
			DString_AppendWCS( text, L"{" );
		map = DaoValue_CastMap( value );
		node = DaoMap_First( map );
		while( node != NULL ){
			JSON_Indent( text, indent );
			if( DaoValue_Type( DNode_Key( node ) ) != DAO_STRING )
				return -1;
			if( ( res = JSON_SerializeValue( DNode_Key( node ), text, indent ) ) != 0 )
				return res;
			DString_AppendWCS( text, L": " );
			if( ( res = JSON_SerializeValue( DNode_Value( node ), text, indent ) ) != 0 )
				return res;
			node = DaoMap_Next( map, node );
			if( node != NULL )
				DString_AppendWCS( text, sep );
			else if( indent >= 0 )
				DString_AppendWCS( text, L"\n" );
		}
		if( indent > 0 )
			indent--;
		JSON_Indent( text, indent );
		DString_AppendWCS( text, L"}");
		break;
	case DAO_NULL:
		DString_AppendWCS( text, L"null" );
		break;
	default:
		return DaoValue_Type( value );
	}
	return 0;
}

static void JSON_Serialize( DaoProcess *proc, DaoValue *p[], int N )
{
	char buf[100];
	int res = DaoValue_TryGetEnum( p[1] );
	if( ( res = JSON_SerializeValue( p[0], DaoProcess_PutWCString( proc, L"" ), res? -1 : 0 ) ) != 0 ){
		if( res == -1 )
			strcpy( buf, "Non-string key in map/object" );
		else{
			strcpy( buf, "Unsupported value type: " );
			switch( res ){
				case DAO_COMPLEX:   strcat( buf, "complex" ); break;
				case DAO_LONG:      strcat( buf, "long" ); break;
				case DAO_ENUM:      strcat( buf, "enum" ); break;
				case DAO_ARRAY:	    strcat( buf, "array" );	break;
				case DAO_TUPLE:	    strcat( buf, "tuple" );	break;
				case DAO_STREAM:    strcat( buf, "stream" ); break;
				case DAO_MUTEX:
				case DAO_SEMA:
				case DAO_CONDVAR:
				case DAO_OBJECT:    strcat( buf, "object" ); break;
				case DAO_CLASS:
				case DAO_NAMESPACE:	strcat( buf, "class/namespace" ); break;
				case DAO_CDATA:
				case DAO_CTYPE:	    strcat( buf, "cdata/ctype" ); break;
				case DAO_INTERFACE:	strcat( buf, "interface" );	break;
				case DAO_FUNCTREE:
				case DAO_ROUTINE:
				case DAO_FUNCTION:  strcat( buf, "routine" ); break;
				case DAO_TYPE:      strcat( buf, "type" ); break;
				default:            strcat( buf, "[type not recognized]" );
			}
		}
		DaoProcess_RaiseException( proc, DAO_ERROR, buf );
	}
}

enum
{
	JSON_ArrayNotClosed = 1,
	JSON_ObjectNotClosed,
	JSON_UnexpectedRSB,
	JSON_UnexpectedRCB,
	JSON_UnexpectedComa,
	JSON_MissingComa,
	JSON_InvalidToken,
	JSON_UnexpectedColon,
	JSON_MissingColon,
	JSON_NonStringKey
};

DaoValue* JSON_ParseString( wchar_t* *text )
{
	wchar_t* end = *text + 1;
	DaoValue *value;
	for( ; *end != L'\0'; end++ )
		if( *end == L'\\' && *(end + 1) != L'\0' )
			end++;
		else if( *end == L'"' ){
			value = DaoValue_NewWCString( *text + 1, end - *text - 1 );
			*text = end + 1;
			return value;
		}
	return NULL;
}

DaoValue* JSON_ParseNumber( wchar_t* *text )
{
	wchar_t* stop;
	double dres;
	int ires;
	errno = 0;
	ires = wcstol( *text, &stop, 10 );
	if( errno == ERANGE || ( *stop != L'\0' && wcschr( L"eE.", *stop ) != NULL && stop != *text ) ){
		dres = wcstod( *text, &stop );
		*text = stop;
		return DaoValue_NewDouble( dres );
	}
	else if( stop != *text ){
		*text = stop;
		return DaoValue_NewInteger( ires );
	}
	return NULL;
}

DaoValue* JSON_ParseSpecialLiteral( wchar_t* *text )
{
	wchar_t buf[6];
	wcsncpy( buf, *text, 5 );
	buf[5] = L'\0';
	if( wcscmp( buf, L"false" ) == 0 ){
		*text += 5;
		return DaoValue_NewInteger( 0 );
	}
	buf[4] = L'\0';
	if( wcscmp( buf, L"true" ) == 0 ){
		*text += 4;
		return DaoValue_NewInteger( 1 );
	}
	else if( wcscmp( buf, L"null" ) == 0 ){
		*text += 4;
		return DaoValue_NewNull();
	}
	return NULL;
}

wchar_t* JSON_FindData( wchar_t* text, int *line )
{
	for( ; *text != L'\0'; text++ ){
		if( *text == L'/' && *( text + 1 ) == L'/' )
			for( text += 2; ; text++ ){
				if( *text == L'\0' )
					return NULL;
				else if( *text == L'\n' ){
					(*line)++;
					break;
				}
			}
		else if( *text == L'/' && *( text + 1 ) == L'*' )
			for( text += 2; ; text++ ){
				if( *text == L'\0' )
					return NULL;
				else if( *text == L'*' && *( ++text ) == L'/' )
					break;
				else if( *text == L'\n' )
					(*line)++;
			}
		else if( *text == L'\n' )
			(*line)++;
		else if( !wcschr( L" \t\r", *text ) )
			return text;
	}
	return NULL;
}

DaoValue* JSON_ParseObject( DaoValue *object, wchar_t* *text, int *error, int *line );

DaoValue* JSON_ParseArray( DaoValue *exlist, wchar_t* *text, int *error, int *line )
{
	wchar_t* data;
	DaoValue *list = exlist? exlist : DaoValue_NewList();
	DaoValue *value;
	int coma = 0, rc = exlist? 0 : 1;
	(*text)++;
	for( ;; ){
		data = JSON_FindData( *text, line );
		if( data == NULL ){
			if( rc )
				DaoGC_DecRC( list );
			*error = JSON_ArrayNotClosed;
			return NULL;
		}
		*text = data;
		if( *data == L']' ){
			if( coma || DaoList_Size( DaoValue_CastList( list ) ) == 0 ){
				(*text)++;
				return list;
			}
			else{
				*error = JSON_UnexpectedRSB;
				return NULL;
			}
		}
		else if( *data == L',' ){
			if( !coma ){
				if( rc )
					DaoGC_DecRC( list );
				*error = JSON_UnexpectedComa;
				return NULL;
			}
			coma = 0;
			(*text)++;
			continue;
		}
		else if( coma ){
			if( rc )
				DaoGC_DecRC( list );
			*error = JSON_MissingComa;
			return NULL;
		}
		else if( *data == L'"' )
			value = JSON_ParseString( text );
		else if( *data == L'[' )
			value = JSON_ParseArray( NULL, text, error, line );
		else if( *data == L'{' )
			value = JSON_ParseObject( NULL, text, error, line );
		else if( wcschr( L"0123456789-", *data ) != NULL )
			value = JSON_ParseNumber( text );
		else
			value = JSON_ParseSpecialLiteral( text );
		if( value == NULL ){
			if( rc )
				DaoGC_DecRC( list );
			if( !*error )
				*error = JSON_InvalidToken;
			return NULL;
		}
		DaoList_PushBack( DaoValue_CastList( list ), value );
		DaoGC_DecRC( value );
		coma = 1;
	}
}

DaoValue* JSON_ParseObject( DaoValue *exmap, wchar_t* *text, int *error, int *line )
{
	wchar_t* data;
	DaoValue *map = exmap? exmap : DaoValue_NewMap( 0 );
	DaoValue *key, *value;
	DaoValue **val = &key;
	int coma = 0, colon = 0, rc = exmap? 0 : 1;
	(*text)++;
	for( ;; ){
		data = JSON_FindData( *text, line );
		if( data == NULL ){
			if( rc )
				DaoGC_DecRC( map );
			*error = JSON_ObjectNotClosed;
			return NULL;
		}
		*text = data;
		if( *data == L'}' ){
			if( coma || DaoMap_Size( DaoValue_CastMap( map ) ) == 0 ){
				(*text)++;
				return map;
			}
			else{
				*error = JSON_UnexpectedRCB;
				return NULL;
			}
		}
		else if( *data == L',' ){
			if( !coma ){
				if( rc )
					DaoGC_DecRC( map );
				*error = JSON_UnexpectedComa;
				return NULL;
			}
			coma = 0;
			(*text)++;
			continue;
		}
		else if( coma ){
			if( rc )
				DaoGC_DecRC( map );
			*error = JSON_MissingComa;
			return NULL;
		}
		else if( *data == L':' ){
			if( !colon ){
				if( rc )
					DaoGC_DecRC( map );
				*error = JSON_UnexpectedColon;
				return NULL;
			}
			colon = 0;
			(*text)++;
			continue;
		}
		else if( colon ){
			if( rc )
				DaoGC_DecRC( map );
			*error = JSON_MissingColon;
			return NULL;
		}
		else if( *data == L'"' )
			*val = JSON_ParseString( text );
		else if( val == &key ){
			if( rc )
				DaoGC_DecRC( map );
			*error = JSON_NonStringKey;
			return NULL;
		}
		else if( *data == L'[' )
			*val = JSON_ParseArray( NULL, text, error, line );
		else if( *data == L'{' )
			*val = JSON_ParseObject( NULL, text, error, line );
		else if( wcschr( L"0123456789-", *data ) != NULL )
			*val = JSON_ParseNumber( text );
		else
			*val = JSON_ParseSpecialLiteral( text );
		if( *val == NULL ){
			if( rc )
				DaoGC_DecRC( map );
			if( !*error )
				*error = JSON_InvalidToken;
			return NULL;
		}
		if( val == &key ){
			val = &value;
			colon = 1;
		}
		else{
			DaoMap_Insert( DaoValue_CastMap( map ), key, value );
			DaoGC_DecRC( key );
			DaoGC_DecRC( value );
			val = &key;
			coma = 1;
		}
	}
}

static void JSON_Deserialize( DaoProcess *proc, DaoValue *p[], int N )
{
	int error = 0, line = 1;
	wchar_t *text = JSON_FindData( DaoValue_TryGetWCString( p[0] ), &line );
	DaoValue *value;
	char buf[100];
	if( text == NULL ){
		DaoProcess_RaiseException( proc, DAO_ERROR, "JSON data not found" );
		return;
	}
	if( *text == L'{' )
		value = JSON_ParseObject( (DaoValue*)DaoProcess_PutMap( proc ), &text, &error, &line );
	else if( *text == L'[' )
		value = JSON_ParseArray( (DaoValue*)DaoProcess_PutList( proc ), &text, &error, &line );
	else{
		DaoProcess_RaiseException( proc, DAO_ERROR, "JSON data is not an object or array" );
		return;
	}
	if( value == NULL ){
		strcpy( buf, "JSON parser error at line " );
		itoa( line, buf + strlen( buf ), 10 );
		strcat( buf, ": " );
		switch( error ){
		case JSON_ArrayNotClosed:  strcat( buf, "unexpected end of data (array)" ); break;
		case JSON_ObjectNotClosed: strcat( buf, "unexpected end of data (object)" ); break;
		case JSON_UnexpectedRSB:   strcat( buf, "unexpected ']'" ); break;
		case JSON_UnexpectedRCB:   strcat( buf, "unexpected '}'" ); break;
		case JSON_UnexpectedComa:  strcat( buf, "unexpected ','" ); break;
		case JSON_MissingComa:     strcat( buf, "missing ','" ); break;
		case JSON_InvalidToken:    strcat( buf, "invalid token" ); break;
		case JSON_UnexpectedColon: strcat( buf, "unexpected ':'" ); break;
		case JSON_MissingColon:    strcat( buf, "missing ':'" ); break;
		case JSON_NonStringKey:    strcat( buf, "non-string key in object" ); break;
		default:                   strcat( buf, "[undefined error]" );
		}
		DaoProcess_RaiseException( proc, DAO_ERROR, buf );
		return;
	}
	if( JSON_FindData( text, &line ) != NULL )
		DaoProcess_RaiseException( proc, DAO_ERROR, "JSON data does not form a single structure" );
}

static DaoFuncItem jsonMeths[] =
{
	{ JSON_Serialize,   "serialize( data: map<string, any>|list<any>, style: enum<pretty,compact>=$pretty )=>string" },
	{ JSON_Deserialize, "deserialize( text: string )=>map<string, any>|list<any>" },
	{ NULL, NULL }
};

DaoTypeBase jsonTyper = {
	"json", NULL, NULL, jsonMeths, {NULL}, {0}, NULL, NULL
};

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace_WrapType( ns, & jsonTyper );
	return 0;
}
