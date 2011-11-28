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

#include<ctype.h>
#include<stdio.h>
#include<string.h>

#include"dao.h"
#include"daoValue.h"
DAO_INIT_MODULE

#define dao_free free

struct Format
{
	char sign;
	char notation;
	int precision;
	int alignment;
	char centered;
	int indexed, sliced;
	int index, index2;
	char *name;
	wchar_t *wname;
	int namelen;
};

typedef struct Format Format;

static int PrintValue( DaoValue *value, DString *dest, Format *format, DString *tmp, void *buffer)
{
	char *buf = buffer, fmt[20] = {'%', 0};
	int i = 1, diff, alignment = format->alignment, notation, len = 0, error = 0, integer = 0, res;
	int indexed = format->indexed, sliced = format->sliced, index = format->index, index2 = format->index2;
	int centered = format->centered;
	int type = DaoValue_Type( value );
	DNode *node;
	DString *valstr;
	DaoList *vallist;
	DaoMap *valmap;
	DaoTuple *valtuple;
	DaoArray *valarray;
	void *number = NULL;
	notation = ( format->notation != 0 );
	if( notation ){
		if( format->sign )
			fmt[i++] = ( format->sign == 1)? '+' : ' ';
		if( format->precision ){
			fmt[i++] = '.';
			sprintf( fmt + i, "%i", format->precision );
		}
		if( format->notation == 'i' || format->notation == 'x' || format->notation == 'X' ){
			strcat( fmt, ( sizeof(dint) == 4 )? "l" : "ll" );
			integer = 1;
		}
		len = strlen( fmt );
		fmt[len++] = format->notation;
	}
	switch( type ){
	case DAO_NONE:
		strcpy( buf, "none" );
		break;
	case DAO_INTEGER:
		if( notation ){
			if( integer )
				sprintf( buf, fmt, DaoValue_TryGetInteger( value ) );
			else
				sprintf( buf, fmt, (double)DaoValue_TryGetInteger( value ) );
		}
		else
			sprintf( buf, ( sizeof(dint) == 4 )? "%li" : "%lli", DaoValue_TryGetInteger( value ) );
		break;
	case DAO_FLOAT:
		if( notation ){
			if( integer )
				sprintf( buf, fmt, (dint)DaoValue_TryGetFloat( value ) );
			else
				sprintf( buf, fmt, DaoValue_TryGetFloat( value ) );
		}
		else
			sprintf( buf, "%g", DaoValue_TryGetFloat( value ) );
		break;
	case DAO_DOUBLE:
		if( notation ){
			if( integer )
				sprintf( buf, fmt, (dint)DaoValue_TryGetDouble( value ) );
			else
				sprintf( buf, fmt, DaoValue_TryGetDouble( value ) );
		}
		else
			sprintf( buf, "%g", DaoValue_TryGetDouble( value ) );
		break;
	case DAO_COMPLEX:
		if( notation ){
			complex16 comp = DaoValue_TryGetComplex( value );
			strcat( fmt, "%+" );
			if( format->sign )
				strncpy( fmt + len + 2, fmt + 2, len - 2 );
			else
				strncpy( fmt + len + 2, fmt + 1, len - 1 );
			if( integer )
				sprintf( buf, fmt, (dint)comp.real, (dint)comp.imag );
			else
				sprintf( buf, fmt, comp.real, comp.imag );
		}
		else
			sprintf( buf, "%g%+g$", DaoValue_TryGetComplex( value ).real, DaoValue_TryGetComplex( value ).imag );
		break;
	case DAO_LONG:
		DLong_Print( value->xLong.value, tmp );
		break;
	case DAO_ENUM:
		DaoEnum_MakeName( DaoValue_CastEnum( value ), tmp );
		break;
	case DAO_STRING:
		valstr = DaoString_Get( DaoValue_CastString( value ) );
		if( sliced ){
			if( index < 0 )
				index += DString_Size( valstr );
			if( index2 < 0 )
				index2 += DString_Size( valstr );
			if( index < 0 || index >= DString_Size( valstr ) || index2 < 0 || index2 >= DString_Size( valstr ))
				return 9;
			if ( index2 < index )
				return 10;
			if( index != index2 ){
				if( DString_IsMBS( valstr ) )
					DString_SetDataMBS( tmp,  DString_GetMBS( valstr ) + index, index2 - index + 1 );
				else
					DString_SetDataWCS( tmp, DString_GetWCS( valstr ) + index, index2 - index + 1 );
				break;
			}
			sliced = 0;
			indexed = 1;
		}
		if( indexed ){
			if( index < 0 )
				index += DString_Size( valstr );
			if( index < 0 || index >= DString_Size( valstr ) )
				return 6;
			if( DString_IsMBS( valstr ) )
				sprintf( buf, notation? fmt : "%i", (int)DString_GetMBS( valstr )[index] );
			else
				sprintf( buf, notation? fmt : "%i", (int)DString_GetWCS( valstr )[index] );
		}
		else
			DString_Assign( tmp, valstr );
		break;
	case DAO_LIST:
		vallist = DaoValue_CastList( value );
		if( sliced ){
			if( index < 0 )
				index += DaoList_Size( vallist );
			if( index2 < 0 )
				index2 += DaoList_Size( vallist );
			if( index < 0 || index >= DaoList_Size( vallist ) || index2 < 0 || index2 >= DaoList_Size( vallist ) )
				return 9;
			if ( index2 < index )
				return 10;
			if( index != index2 ){
				format->sliced = 0;
				for( i = index; i <= index2; i++ ){
					if( i != index )
						DString_AppendMBS( dest, ", " );
					if( ( res = PrintValue( DaoList_GetItem( vallist, i ), dest, format, tmp, buffer ) ) == 1
							|| res == 2)
						return 2;
					else if( res == 3 || res == 4 )
						error = 4;
				}
				break;
			}
			sliced = 0;
			indexed = 1;
		}
		if( indexed ){
			if( index < 0 )
				index += DaoList_Size( vallist );
			if( index < 0 || index >= DaoList_Size( vallist ) )
				return 6;
			format->indexed = 0;
			return PrintValue( DaoList_GetItem( vallist, index ), dest, format, tmp, buffer );
		}
		else
			for( i = 0; i < DaoList_Size( vallist ); i++ ){
				if( i )
					DString_AppendMBS( dest, ", " );
				if( ( res = PrintValue( DaoList_GetItem( vallist, i ), dest, format, tmp, buffer ) ) == 1
						|| res == 2)
					return 2;
				else if( res == 3 || res == 4 )
					error = 4;
			}
		break;
	case DAO_MAP:
		valmap = DaoValue_CastMap( value );
		for( i = 0, node = DaoMap_First( valmap ); node; node = DaoMap_Next( valmap, node ), i++ ){
			if( i )
				DString_AppendMBS( dest, ", " );
			if( ( res = PrintValue( DNode_Key( node ), dest, format, tmp, buffer ) ) == 1 || res == 2 )
				return 2;
			else if( res == 3 || res == 4 )
				error = 4;
			DString_AppendMBS( dest, valmap->items->hashing ? " : " : " => " );
			if( ( res = PrintValue( DNode_Value( node ), dest, format, tmp, buffer ) ) == 1 || res == 2 )
				return 2;
			else if( res == 3 || res == 4 )
				error = 4;
		}
		break;
	case DAO_ARRAY:
		valarray = DaoValue_CastArray( value );
		if( sliced ){
			complex16 comp = {0, 0};
			int rowsize = 1, maxdim;
			if( DaoArray_SizeOfDim( valarray, 0 ) != 1 ){
				maxdim = DaoArray_SizeOfDim( valarray, 0 );
				for( i = 1; i < DaoArray_DimCount( valarray ); i++ )
					rowsize *= DaoArray_SizeOfDim( valarray, i );
			}
			else
				maxdim = DaoArray_SizeOfDim( valarray, 1 );
			if( index < 0 )
				index += maxdim;
			if( index2 < 0 )
				index2 += maxdim;
			if( index < 0 || index >= maxdim || index2 < 0 || index2 >= maxdim )
				return 9;
			if ( index2 < index )
				return 10;
			switch( DaoArray_NumType( valarray ) ){
			case DAO_INTEGER: number = DaoInteger_New( 0 ); break;
			case DAO_FLOAT:   number = DaoFloat_New( 0 ); break;
			case DAO_DOUBLE:  number = DaoDouble_New( 0 ); break;
			case DAO_COMPLEX: number = DaoComplex_New( comp ); break;
			default: break;
			}
			format->sliced = 0;
			for( i = index*rowsize; i < ( index2 + 1 )*rowsize; i++ ){
				if( i != index*rowsize )
					DString_AppendMBS( dest, ", " );
				switch( DaoArray_NumType( valarray ) ){
				case DAO_INTEGER: DaoInteger_Set( (DaoInteger*)number, DaoArray_ToInteger( valarray )[i] ); break;
				case DAO_FLOAT:   DaoFloat_Set( (DaoFloat*)number, DaoArray_ToFloat( valarray )[i] ); break;
				case DAO_DOUBLE:  DaoDouble_Set( (DaoDouble*)number, DaoArray_ToDouble( valarray )[i] ); break;
				case DAO_COMPLEX: DaoComplex_Set( (DaoComplex*)number,
												  ( (complex16*)DaoArray_GetBuffer( valarray ) )[i] ); break;
				default: break;
				}
				PrintValue( (DaoValue*)number, dest, format, tmp, buffer );
			}
			dao_free( number );
		}
		else if( indexed ){
			if( index < 0 )
				index += DaoArray_Size( valarray );
			if( index < 0 || index >= DaoArray_Size( valarray ) )
				return 6;
			format->indexed = 0;
			switch( DaoArray_NumType( valarray ) ){
			case DAO_INTEGER: number = DaoInteger_New( DaoArray_ToInteger( valarray )[index] ); break;
			case DAO_FLOAT:   number = DaoFloat_New( DaoArray_ToFloat( valarray )[index] ); break;
			case DAO_DOUBLE:  number = DaoDouble_New( DaoArray_ToDouble( valarray )[index] ); break;
			case DAO_COMPLEX: number = DaoComplex_New( ( (complex16*)DaoArray_GetBuffer( valarray ) )[index] ); break;
			default: break;
			}
			res = PrintValue( (DaoValue*)number, dest, format, tmp, buffer );
			dao_free( number );
			return res;
		}
		else{
			complex16 comp = {0, 0};
			switch( DaoArray_NumType( valarray ) ){
			case DAO_INTEGER: number = DaoInteger_New( 0 ); break;
			case DAO_FLOAT:   number = DaoFloat_New( 0 ); break;
			case DAO_DOUBLE:  number = DaoDouble_New( 0 ); break;
			case DAO_COMPLEX: number = DaoComplex_New( comp ); break;
			default: break;
			}
			for( i = 0; i < DaoArray_Size( valarray ); i++ ){
				if( i )
					DString_AppendMBS( dest, ", " );
				switch( DaoArray_NumType( valarray ) ){
				case DAO_INTEGER: DaoInteger_Set( (DaoInteger*)number, DaoArray_ToInteger( valarray )[i] ); break;
				case DAO_FLOAT:   DaoFloat_Set( (DaoFloat*)number, DaoArray_ToFloat( valarray )[i] ); break;
				case DAO_DOUBLE:  DaoDouble_Set( (DaoDouble*)number, DaoArray_ToDouble( valarray )[i] ); break;
				case DAO_COMPLEX: DaoComplex_Set( (DaoComplex*)number,
												  ( (complex16*)DaoArray_GetBuffer( valarray ) )[i] ); break;
				default: break;
				}
				PrintValue( (DaoValue*)number, dest, format, tmp, buffer );
			}
			dao_free( number );
		}
		break;
	case DAO_TUPLE:
		valtuple = DaoValue_CastTuple( value );
		if( format->name || format->wname ){
			if( format->name )
				DString_SetDataMBS( tmp, format->name, format->namelen );
			else
				DString_SetDataWCS( tmp, format->wname, format->namelen );
			if( ( value = DaoTuple_GetItem( valtuple, DaoTuple_GetIndex( valtuple, tmp ) ) ) == NULL )
				return 8;
			format->name = NULL;
			format->wname = NULL;
			return PrintValue( value, dest, format, tmp, buffer );
		}
		else if( indexed ){
			if( index < 0 )
				index += DaoTuple_Size( valtuple );
			if( index < 0 || index >= DaoTuple_Size( valtuple ) )
				return 6;
			format->indexed = 0;
			return PrintValue( DaoTuple_GetItem( valtuple, index ), dest, format, tmp, buffer );
		}
		else
			for( i = 0; i < DaoTuple_Size( valtuple ); i++ ){
				if( i )
					DString_AppendMBS( dest, ", " );
				if( ( res = PrintValue( DaoTuple_GetItem( valtuple, i ), dest, format, tmp, buffer ) ) == 1 || res == 2 )
					return 2;
				else if( res == 3 || res == 4 )
					error = 4;
			}
		break;
	default: return 1;
	}
	if( format->name && type != DAO_TUPLE )
		return 7;
	if( sliced && type != DAO_STRING && type != DAO_LIST && type != DAO_ARRAY )
		return 11;
	if( !indexed || ( type >= DAO_STRING && type != DAO_MAP ) ){
		if( type <= DAO_COMPLEX || ( type == DAO_STRING && indexed ) ){
			len = strlen( buf );
			if( centered )
				diff = ( alignment - len )/2 + ( ( ( alignment - len )%2 )? 1 : 0 );
			else
				diff = alignment - len;
			if( diff > 0 )
				for( i = 0; i < diff; i++ )
					DString_AppendChar( dest, ' ' );
			DString_AppendMBS( dest, buf );
			if( centered )
				diff = ( alignment - len )/2;
			else
				diff = ( alignment < 0 )? -alignment - len : 0;
			if( diff > 0 )
				for( i = 0; i < diff; i++ )
					DString_AppendChar( dest, ' ' );
		}
		else if( type == DAO_LONG || type == DAO_ENUM || ( type == DAO_STRING && !indexed ) ){
			len = DString_Size( tmp );
			if( centered )
				diff = ( alignment - len )/2 + ( ( ( alignment - len )%2 )? 1 : 0 );
			else
				diff = alignment - len;
			if( diff > 0 )
				for( i = 0; i < diff; i++ )
					DString_AppendChar( dest, ' ' );
			DString_Append( dest, tmp );
			if( centered )
				diff = ( alignment - len )/2;
			else
				diff = ( alignment < 0 )? -alignment - len : 0;
			if( diff > 0 )
				for( i = 0; i < diff; i++ )
					DString_AppendChar( dest, ' ' );
		}
	}
	else
		return 5;
	if( notation )
		return ( type != DAO_ENUM && ( type != DAO_STRING || indexed ) && type != DAO_LONG && type != DAO_NONE )?
					error : 3;
	else
		return error;
}
static void DaoFormat( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *fmt = DaoString_Get( DaoValue_CastString( p[0] ) );
	DString *str = DString_New( DString_IsMBS( fmt )? 1 : 0 );
	DString *tmp = DString_New( 1 );
	Format format;
	DaoValue *value;
	int pos, pos2, prevpos, sliced;
	int num, error, width, alignment, sign, precision, notation, argend, index, index2, indexed, centered, namelen;
	int argnamelen;
	char *mbs, buf[200], *name, *argname;
	wchar_t *wcs, *wname, *wargname;
	DString_Reserve( str, DString_Size( fmt ) );
	format.name = NULL;
	format.wname = NULL;
	if( DString_IsMBS( fmt ) ){
		/*MBS*/
		mbs = DString_GetMBS( fmt );
		for( pos = 0; pos < DString_Size( fmt ); pos += 2){
			num = error = alignment = sign = precision = notation = argend = index = indexed = centered = namelen = 0;
			sliced = index2 = argnamelen = 0;
			name = argname = NULL;
			value = NULL;
			prevpos = pos;
			pos = DString_FindChar( fmt, '$', pos );
			if( pos == -1 ){
				DString_AppendDataMBS( str, mbs + prevpos, DString_Size( fmt ) - prevpos );
				break;
			}
			else{
				DString_AppendDataMBS( str, mbs + prevpos, pos - prevpos );
				if( mbs[pos + 1] == '$' ){
					DString_AppendChar( str, '$' );
					continue;
				}
				else if( mbs[pos + 1] == '(' ){
					prevpos = pos + 2;
					pos = DString_FindChar( fmt, ')', prevpos );
					if( pos == -1 ){
						DaoProcess_RaiseException( proc, DAO_WARNING, "Placeholder bracket not closed!" );
						break;
					}
					else if( pos == prevpos ){
						DaoProcess_RaiseException( proc, DAO_WARNING, "Empty placeholder!" );
						continue;
					}
					if( ( isalpha( mbs[prevpos] ) || mbs[prevpos] == '_' ) && !argend ){
						int litera = 0;
						argname = mbs + prevpos;
						for(; prevpos < pos; prevpos++, argnamelen++ )
							if( isalpha( mbs[prevpos] ) )
								litera = 1;
							else if( isdigit( mbs[prevpos] ) ){
								if( !litera )
									break;
							}
							else if( mbs[prevpos] != '_' )
								break;
						if( !litera ){
							sprintf( buf, "Invalid placeholder name (stopped on char '%c')!", mbs[prevpos] );
							DaoProcess_RaiseException( proc, DAO_WARNING, buf );
							pos--;
							DString_AppendChar( str, '?' );
							continue;
						}
					}
					for(; prevpos < pos; prevpos++ )
						if( isdigit( mbs[prevpos] ) && !argend && !argname )
							num = num*10 + ( mbs[prevpos] - '0' );
						else if( mbs[prevpos - 1] == '(' ){
							sprintf( buf, "Invalid character in placeholder ID ('%c')!", mbs[prevpos] );
							DaoProcess_RaiseException( proc, DAO_WARNING, buf );
							error = 1;
							break;
						}
						else if( mbs[prevpos] == '.' && !argend ){
							argend = 1;
							prevpos++;
							if( strchr( ":<>=)", mbs[prevpos] ) ){
								sprintf( buf, "Empty argument field name!" );
								DaoProcess_RaiseException( proc, DAO_WARNING, buf );
								error = 1;
								break;
							}
							if( !isalpha( mbs[prevpos] ) && mbs[prevpos] != '_' ){
								sprintf( buf, "Invalid character in argument field name ('%c')!", mbs[prevpos] );
								DaoProcess_RaiseException( proc, DAO_WARNING, buf );
								error = 1;
								break;
							}
							name = mbs + prevpos;
							namelen = 1;
							for( prevpos++; isalnum( mbs[prevpos] ) || mbs[prevpos] == '_'; prevpos++  )
								namelen++;
							if( namelen > 100 ){
								sprintf( buf, "Argument field name too large (greater than 100 characters)!" );
								DaoProcess_RaiseException( proc, DAO_WARNING, buf );
								error = 1;
								break;
							}
							prevpos--;
						}
						else if( mbs[prevpos] == '[' && !argend ){
							argend = 1;
							prevpos++;
							pos2 = DString_FindChar( fmt, ']', prevpos );
							if( pos2 == -1 ){
								DaoProcess_RaiseException( proc, DAO_WARNING, "Index bracket not closed!" );
								error = 1;
								break;
							}
							if( prevpos == pos2 ){
								DaoProcess_RaiseException( proc, DAO_WARNING, "Empty index!" );
								error = 1;
								break;
							}
							if( mbs[prevpos] == '-' || mbs[prevpos] == '+' ){
								if( prevpos == pos2 - 1 || mbs[prevpos + 1] == ':' ){
									DaoProcess_RaiseException( proc, DAO_WARNING, "Missing index number!" );
									error = 1;
									break;
								}
								indexed = ( mbs[prevpos] == '-' )? -1 : 1;
								prevpos++;
							}
							else
								indexed = 1;
							for(; prevpos < pos2; prevpos++ )
								if( isdigit( mbs[prevpos] ) )
									index = index*10 + ( mbs[prevpos] - '0' );
								else if ( mbs[prevpos] == ':' ){
									prevpos++;
									if( prevpos == pos2 ){
										sliced = 1;
										index2 = -1;
										break;
									}
									if( mbs[prevpos] == '-' || mbs[prevpos] == '+' ){
										if( prevpos == pos2 - 1 ){
											DaoProcess_RaiseException( proc, DAO_WARNING, "Missing index number!" );
											error = 1;
											break;
										}
										sliced = ( mbs[prevpos] == '-' )? -1 : 1;
										prevpos++;
									}
									else
										sliced = 1;
									for(; prevpos < pos2; prevpos++ )
										if( isdigit( mbs[prevpos] ) )
											index2 = index2*10 + ( mbs[prevpos] - '0' );
										else{
											sprintf( buf, "Invalid character in index ('%c')!", mbs[prevpos] );
											DaoProcess_RaiseException( proc, DAO_WARNING, buf );
											sliced = 0;
											error = 1;
											break;
										}
									index2 *= sliced;
									if( error )
										break;
								}
								else{
									sprintf( buf, "Invalid character in index ('%c')!", mbs[prevpos] );
									DaoProcess_RaiseException( proc, DAO_WARNING, buf );
									indexed = 0;
									error = 1;
									break;
								}
							index *= indexed;
							if( sliced )
								indexed = 0;
							prevpos = pos2;
						}
						else if( strchr( "<>=", mbs[prevpos] ) ){
							argend = 1;
							if( prevpos == pos - 1 ){
								DaoProcess_RaiseException( proc, DAO_WARNING, "Field width not specified!" );
								break;
							}
							alignment = ( mbs[prevpos] == '<' )? -1 : 1;
							if( mbs[prevpos] == '=' )
								centered = 1;
							width = 0;
							for( prevpos++; prevpos < pos; prevpos++)
								if( isdigit(mbs[prevpos] ) )
									width = width*10 + ( mbs[prevpos] - '0' );
								else{
									sprintf( buf, "Invalid character in field width ('%c')!", mbs[prevpos] );
									DaoProcess_RaiseException( proc, DAO_WARNING, buf );
									width = 0;
									break;
								}
							if( width > 1000 || width < 0 ){
								DaoProcess_RaiseException( proc, DAO_WARNING,
														   "Field width too large (greater than 1000)!" );
								width = 0;
							}
							alignment = width? alignment*width : 0;
						}
						else if( mbs[prevpos] == ':' ){
							argend = 1;
							if( prevpos == pos - 1 || strchr( "<>=", mbs[prevpos + 1] ) ){
								DaoProcess_RaiseException( proc, DAO_WARNING, "Empty numeric format!" );
								continue;
							}
							prevpos++;
							if( mbs[prevpos] == '+' ){
								sign = 1;
								prevpos++;
							}
							else if( mbs[prevpos] == ' ' ){
								sign = -1;
								prevpos++;
							}
							if( prevpos != pos && !strchr( "<>=.", mbs[prevpos] ) ){
								notation = mbs[prevpos];
								if( !strchr( "ixXfgG", notation ) ){
									sprintf( buf, "Invalid numeric format ('%c')!", mbs[prevpos] );
									DaoProcess_RaiseException( proc, DAO_WARNING, buf );
									notation = 0;
									continue;
								}
								if( sign && ( notation == 'x' || notation == 'X' ) )
									DaoProcess_RaiseException( proc, DAO_WARNING, "Signed hexadecimal numeric format!" );
								prevpos++;
							}
							if( prevpos != pos && mbs[prevpos] == '.' ){
								if( prevpos == pos - 1 || mbs[prevpos + 1] == '<'
										|| mbs[prevpos + 1] == '>' ){
									DaoProcess_RaiseException( proc, DAO_WARNING, "Empty precision specifier!" );
									continue;
								}
								for( prevpos++; isdigit( mbs[prevpos] ); prevpos++ )
									precision = precision*10 + ( mbs[prevpos] - '0' );
								if( precision > 1000 || precision < 0 ){
									DaoProcess_RaiseException( proc, DAO_WARNING,
															   "Precision too large (greater than 1000)" );
									precision = 0;
								}
							}
							prevpos--;
							if( ( sign || precision ) && !notation )
								DaoProcess_RaiseException( proc, DAO_WARNING, "Incomplete numeric format!" );
						}
						else{
							sprintf( buf, "Invalid character in placeholder ('%c')!", mbs[prevpos] );
							DaoProcess_RaiseException( proc, DAO_WARNING, buf );
							error = 1;
							break;
						}
					pos--;
					if( error ){
						DString_AppendChar( str, '?' );
						continue;
					}
				}
				else{
					if( !isdigit( mbs[pos + 1] ) ){
						sprintf( buf, "Invalid placeholder index ('%c')!", mbs[pos + 1] );
						DaoProcess_RaiseException( proc, DAO_WARNING, buf );
						DString_AppendChar( str, '?' );
						continue;
					}
					num = mbs[pos + 1] - '0';
				}
				if( argname ){
					int i;
					for( i = 1; i < N; i++ )
						if( DaoValue_Type( p[i] ) == DAO_PAR_NAMED ){
							DString *sname = ( (DaoNameValue*)p[i] )->name;
							if( DString_Size( sname ) == argnamelen &&
									!strncmp( DString_GetMBS( sname ), argname, argnamelen ) ){
								value = ( (DaoNameValue*)p[i] )->value;
								break;
							}
						}
					if( !value ){
						DString *sname = DString_New( 1 );
						DString_SetDataMBS( sname, argname, argnamelen );
						sprintf( buf, "No argument matching placeholder name ('%s')!", DString_GetMBS( sname ) );
						DString_Delete( sname );
						DaoProcess_RaiseException( proc, DAO_WARNING, buf );
						DString_AppendChar( str, '?' );
						continue;
					}
				}
				else if( num > N - 2 || num < 0 ){
					sprintf( buf, "Placeholder index too large (%i)!", num );
					DaoProcess_RaiseException( proc, DAO_WARNING, buf );
					DString_AppendChar( str, '?' );
					continue;
				}
				format.alignment = alignment;
				format.centered = centered;
				format.notation = notation;
				format.sign = sign;
				format.precision = precision;
				format.indexed = indexed;
				format.sliced = sliced;
				format.index = index;
				format.index2 = index2;
				format.name = name;
				format.namelen = namelen;
				if( value )
					error = PrintValue( value, str, &format, tmp, buf );
				else if( DaoValue_Type( p[num + 1] ) == DAO_PAR_NAMED )
					error = PrintValue( ( (DaoNameValue*)p[num + 1] )->value, str, &format, tmp, buf );
				else
					error = PrintValue( p[num + 1], str, &format, tmp, buf );
				switch( error ){
				case 1: sprintf( buf, "Unsupported argument type (argument %i)!", num ); break;
				case 2: sprintf( buf, "Unsupported element type (argument %i)!", num ); break;
				case 3: sprintf( buf, "Conflicting numeric format and argument type (argument %i)!", num ); break;
				case 4: sprintf( buf, "Conflicting numeric format and element type (argument %i)!", num ); break;
				case 5: sprintf( buf, "Conflicting indexing and argument type (argument %i)!", num ); break;
				case 6: sprintf( buf, "Index out of range (argument %i, index %i)!", num, index ); break;
				case 7: sprintf( buf, "Named field for argument which is not a tuple (argument %i)!", num ); break;
				case 8: sprintf( buf, "Named field does not exist (argument %i)!", num ); break;
				case 9: sprintf( buf, "Slice out of range (argument %i, slice %i : %i)!", num, index, index2 ); break;
				case 10: sprintf( buf, "Invalid slice (argument %i, slice %i : %i)!", num, index, index2 ); break;
				case 11: sprintf( buf, "Conflicting slicing and argument type (argument %i)!", num ); break;
				default: break;
				}
				if( error ){
					DaoProcess_RaiseException( proc, DAO_WARNING, buf );
					if( error != 3 && error != 4 )
						DString_AppendChar( str, '?' );
				}
				DString_Clear( tmp );
			}
		}
	}
	else{
		/*WCS*/
		wcs = DString_GetWCS( fmt );
		for( pos = 0; pos < DString_Size( fmt ); pos += 2){
			num = error = alignment = sign = precision = notation = argend = index = indexed = centered = namelen = 0;
			sliced = index2 = argnamelen = 0;
			wname = wargname = NULL;
			value = NULL;
			prevpos = pos;
			pos = DString_FindWChar( fmt, L'$', pos );
			if( pos == -1 ){
				DString_AppendDataWCS( str, wcs + prevpos, DString_Size( fmt ) - prevpos );
				break;
			}
			else{
				DString_AppendDataWCS( str, wcs + prevpos, pos - prevpos );
				if( wcs[pos + 1] == L'$' ){
					DString_AppendWChar( str, L'$' );
					continue;
				}
				else if( wcs[pos + 1] == L'(' ){
					prevpos = pos + 2;
					pos = DString_FindWChar( fmt, L')', prevpos );
					if( pos == -1 ){
						DaoProcess_RaiseException( proc, DAO_WARNING, "Placeholder bracket not closed!" );
						break;
					}
					else if( pos == prevpos ){
						DaoProcess_RaiseException( proc, DAO_WARNING, "Empty placeholder!" );
						continue;
					}
					if( ( iswalpha( wcs[prevpos] ) || wcs[prevpos] == L'_' ) && !argend ){
						int litera = 0;
						wargname = wcs + prevpos;
						for(; prevpos < pos; prevpos++, argnamelen++ )
							if( iswalpha( wcs[prevpos] ) )
								litera = 1;
							else if( iswdigit( wcs[prevpos] ) ){
								if( !litera )
									break;
							}
							else if( wcs[prevpos] != L'_' )
								break;
						if( !litera ){
							sprintf( buf, "Invalid placeholder name (stopped on char '%c')!", wcs[prevpos] );
							DaoProcess_RaiseException( proc, DAO_WARNING, buf );
							pos--;
							DString_AppendWChar( str, L'?' );
							continue;
						}
					}
					for(; prevpos < pos; prevpos++ )
						if( iswdigit( wcs[prevpos] ) && !argend && !wargname )
							num = num*10 + ( wcs[prevpos] - L'0' );
						else if( wcs[prevpos - 1] == L'(' ){
							sprintf( buf, "Invalid character in placeholder ID ('%c')!", wcs[prevpos] );
							DaoProcess_RaiseException( proc, DAO_WARNING, buf );
							error = 1;
							break;
						}
						else if( wcs[prevpos] == L'.' && !argend ){
							argend = 1;
							prevpos++;
							if( wcschr( L":<>=)", wcs[prevpos] ) ){
								sprintf( buf, "Empty argument field name!" );
								DaoProcess_RaiseException( proc, DAO_WARNING, buf );
								error = 1;
								break;
							}
							if( !iswalpha( wcs[prevpos] ) && wcs[prevpos] != L'_' ){
								sprintf( buf, "Invalid character in argument field name ('%c')!", wcs[prevpos] );
								DaoProcess_RaiseException( proc, DAO_WARNING, buf );
								error = 1;
								break;
							}
							wname = wcs + prevpos;
							namelen = 1;
							for( prevpos++; iswalnum( wcs[prevpos] ) || wcs[prevpos] == L'_'; prevpos++  )
								namelen++;
							if( namelen > 100 ){
								sprintf( buf, "Argument field name too large (greater than 100 characters)!" );
								DaoProcess_RaiseException( proc, DAO_WARNING, buf );
								error = 1;
								break;
							}
							prevpos--;
						}
						else if( wcs[prevpos] == L'[' && !argend ){
							argend = 1;
							prevpos++;
							pos2 = DString_FindWChar( fmt, L']', prevpos );
							if( pos2 == -1 ){
								DaoProcess_RaiseException( proc, DAO_WARNING, "Index bracket not closed!" );
								error = 1;
								break;
							}
							if( prevpos == pos2 ){
								DaoProcess_RaiseException( proc, DAO_WARNING, "Empty index!" );
								error = 1;
								break;
							}
							if( wcs[prevpos] == L'-' || wcs[prevpos] == L'+' ){
								if( prevpos == pos2 - 1 || wcs[prevpos + 1] == L':' ){
									DaoProcess_RaiseException( proc, DAO_WARNING, "Missing index number!" );
									error = 1;
									break;
								}
								indexed = ( wcs[prevpos] == L'-' )? -1 : 1;
								prevpos++;
							}
							else
								indexed = 1;
							for(; prevpos < pos2; prevpos++ )
								if( iswdigit( wcs[prevpos] ) )
									index = index*10 + ( wcs[prevpos] - L'0' );
								else if ( wcs[prevpos] == L':' ){
									prevpos++;
									if( prevpos == pos2 ){
										sliced = 1;
										index2 = -1;
										break;
									}
									if( wcs[prevpos] == L'-' || wcs[prevpos] == L'+' ){
										if( prevpos == pos2 - 1 ){
											DaoProcess_RaiseException( proc, DAO_WARNING, "Missing index number!" );
											error = 1;
											break;
										}
										sliced = ( wcs[prevpos] == L'-' )? -1 : 1;
										prevpos++;
									}
									else
										sliced = 1;
									for(; prevpos < pos2; prevpos++ )
										if( iswdigit( wcs[prevpos] ) )
											index2 = index2*10 + ( wcs[prevpos] - L'0' );
										else{
											sprintf( buf, "Invalid character in index ('%c')!", wcs[prevpos] );
											DaoProcess_RaiseException( proc, DAO_WARNING, buf );
											sliced = 0;
											error = 1;
											break;
										}
									index2 *= sliced;
									if( error )
										break;
								}
								else{
									sprintf( buf, "Invalid character in index ('%c')!", wcs[prevpos] );
									DaoProcess_RaiseException( proc, DAO_WARNING, buf );
									indexed = 0;
									error = 1;
									break;
								}
							index *= indexed;
							if( sliced )
								indexed = 0;
							prevpos = pos2;
						}
						else if( wcschr( L"<>=", wcs[prevpos] ) ){
							argend = 1;
							if( prevpos == pos - 1 ){
								DaoProcess_RaiseException( proc, DAO_WARNING, "Field width not specified!" );
								break;
							}
							alignment = ( wcs[prevpos] == L'<' )? -1 : 1;
							if( wcs[prevpos] == L'=' )
								centered = 1;
							width = 0;
							for( prevpos++; prevpos < pos; prevpos++)
								if( iswdigit( wcs[prevpos] ) )
									width = width*10 + ( wcs[prevpos] - L'0' );
								else{
									sprintf( buf, "Invalid character in field width ('%c')!", wcs[prevpos] );
									DaoProcess_RaiseException( proc, DAO_WARNING, buf );
									width = 0;
									break;
								}
							if( width > 1000 || width < 0 ){
								DaoProcess_RaiseException( proc, DAO_WARNING,
														   "Field width too large (greater than 1000)!" );
								width = 0;
							}
							alignment = width? alignment*width : 0;
						}
						else if( wcs[prevpos] == L':' ){
							argend = 1;
							if( prevpos == pos - 1 || wcschr( L"<>=", wcs[prevpos + 1] ) ){
								DaoProcess_RaiseException( proc, DAO_WARNING, "Empty numeric format!" );
								continue;
							}
							prevpos++;
							if( wcs[prevpos] == L'+' ){
								sign = 1;
								prevpos++;
							}
							else if( wcs[prevpos] == L' ' ){
								sign = -1;
								prevpos++;
							}
							if( prevpos != pos && !wcschr( L"<>=.", wcs[prevpos] ) ){
								notation = wcs[prevpos];
								if( !wcschr( L"ixXfgG", notation ) ){
									sprintf( buf, "Invalid numeric format ('%c')!", wcs[prevpos] );
									DaoProcess_RaiseException( proc, DAO_WARNING, buf );
									notation = 0;
									continue;
								}
								if( sign && ( notation == L'x' || notation == L'X' ) )
									DaoProcess_RaiseException( proc, DAO_WARNING, "Signed hexadecimal numeric format!" );
								prevpos++;
							}
							if( prevpos != pos && wcs[prevpos] == L'.' ){
								if( prevpos == pos - 1 || wcs[prevpos + 1] == L'<'
										|| wcs[prevpos + 1] == L'>' ){
									DaoProcess_RaiseException( proc, DAO_WARNING, "Empty precision specifier!" );
									continue;
								}
								for( prevpos++; iswdigit( wcs[prevpos] ); prevpos++ )
									precision = precision*10 + ( wcs[prevpos] - L'0' );
								if( precision > 1000 || precision < 0 ){
									DaoProcess_RaiseException( proc, DAO_WARNING,
															   "Precision too large (greater than 1000)" );
									precision = 0;
								}
							}
							prevpos--;
							if( ( sign || precision ) && !notation )
								DaoProcess_RaiseException( proc, DAO_WARNING, "Incomplete numeric format!" );
						}
						else{
							sprintf( buf, "Invalid character in placeholder ('%c')!", wcs[prevpos] );
							DaoProcess_RaiseException( proc, DAO_WARNING, buf );
							error = 1;
							break;
						}
					pos--;
					if( error ){
						DString_AppendWChar( str, L'?' );
						continue;
					}
				}
				else{
					if( !iswdigit( wcs[pos + 1] ) ){
						sprintf( buf, "Invalid placeholder index ('%c')!", wcs[pos + 1] );
						DaoProcess_RaiseException( proc, DAO_WARNING, buf );
						DString_AppendWChar( str, L'?' );
						continue;
					}
					num = wcs[pos + 1] - L'0';
				}
				if( wargname ){
					int i;
					for( i = 1; i < N; i++ )
						if( DaoValue_Type( p[i] ) == DAO_PAR_NAMED ){
							DString *sname = ( (DaoNameValue*)p[i] )->name;
							if( DString_Size( sname ) == argnamelen &&
									!wcsncmp( DString_GetWCS( sname ), wargname, argnamelen ) ){
								value = ( (DaoNameValue*)p[i] )->value;
								break;
							}
						}
					if( !value ){
						DString *sname = DString_New( 1 );
						DString_SetDataWCS( sname, wargname, argnamelen );
						sprintf( buf, "No argument matching placeholder name ('%s')!", DString_GetMBS( sname ) );
						DString_Delete( sname );
						DaoProcess_RaiseException( proc, DAO_WARNING, buf );
						DString_AppendWChar( str, L'?' );
						continue;
					}
				}
				else if( num > N - 2 || num < 0 ){
					sprintf( buf, "Placeholder index too large (%i)!", num );
					DaoProcess_RaiseException( proc, DAO_WARNING, buf );
					DString_AppendWChar( str, L'?' );
					continue;
				}
				format.alignment = alignment;
				format.centered = centered;
				format.notation = notation;
				format.sign = sign;
				format.precision = precision;
				format.indexed = indexed;
				format.sliced = sliced;
				format.index = index;
				format.index2 = index2;
				format.wname = wname;
				format.namelen = namelen;
				if( value )
					error = PrintValue( value, str, &format, tmp, buf );
				else if( DaoValue_Type( p[num + 1] ) == DAO_PAR_NAMED )
					error = PrintValue( ( (DaoNameValue*)p[num + 1] )->value, str, &format, tmp, buf );
				else
					error = PrintValue( p[num + 1], str, &format, tmp, buf );
				switch( error ){
				case 1: sprintf( buf, "Unsupported argument type (argument %i)!", num ); break;
				case 2: sprintf( buf, "Unsupported element type (argument %i)!", num ); break;
				case 3: sprintf( buf, "Conflicting numeric format and argument type (argument %i)!", num ); break;
				case 4: sprintf( buf, "Conflicting numeric format and element type (argument %i)!", num ); break;
				case 5: sprintf( buf, "Conflicting indexing and argument type (argument %i)!", num ); break;
				case 6: sprintf( buf, "Index out of range (argument %i, index %i)!", num, index ); break;
				case 7: sprintf( buf, "Named field for argument which is not a tuple (argument %i)!", num ); break;
				case 8: sprintf( buf, "Named field does not exist (argument %i)!", num ); break;
				case 9: sprintf( buf, "Slice out of range (argument %i, slice %i : %i)!", num, index, index2 ); break;
				case 10: sprintf( buf, "Invalid slice (argument %i, slice %i : %i)!", num, index, index2 ); break;
				case 11: sprintf( buf, "Conflicting slicing and argument type (argument %i)!", num ); break;
				default: break;
				}
				if( error ){
					DaoProcess_RaiseException( proc, DAO_WARNING, buf );
					if( error != 3 && error != 4 )
						DString_AppendWChar( str, L'?' );
				}
				DString_Clear( tmp );
			}
		}
	}
	DString_Delete( tmp );
	DaoProcess_PutString( proc, str );
	DString_Delete( str );
}

int DaoOnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace_WrapFunction( ns, (DaoFuncPtr)DaoFormat, "format( self: string, ... )=>string" );
	return 0;
}
