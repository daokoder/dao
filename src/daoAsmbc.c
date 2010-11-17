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

#include"daoAsmbc.h"

#ifdef DAO_WITH_ASMBC
#warning "==============================================="
#warning "==============================================="
#warning "  this implementation of this feature is       "
#warning "  incomplete and outdated.                     "
#warning "                                               "
#warning "  please edit the Makefile and comment out     "
#warning "  the lines containing DAO_WITH_ASMBC.         "
#warning "==============================================="
#warning "==============================================="
#endif

#ifdef DAO_WITH_ASMBC

#include"stdlib.h"
#include"string.h"
#include"stdio.h"

#include"daoAsmbc.h"
#include"daoType.h"
#include"daoClass.h"
#include"daoRoutine.h"
#include"daoParser.h"
#include"daoVmspace.h"
#include"daoContext.h"

/* 'I' : integer; 'F' : float; 'S' : string; 'C' : complex; 
 * 'L' : list; 'H' : map; 'A' : array; 
 * 'R' : routine; 'K' : class; 'T' : type; 'N' : namespace
 */
static const char* constTypeChar = "IFSCLHARKTN";
static const int constTypeCount = 11;
static const char* const sectNames[] =
{
	"" ,
	"@REQUIRE" , 
	"@SOURCE" , 
	"@INTEGER" , 
	"@FLOAT" , 
	"@STRING" ,
	"@COMPLEX" ,
	"@LIST" ,
	"@MAP" ,
	"@ARRAY" ,
	"@ROUTINE" ,
	"@CLASS" ,
	"@TYPE" ,
	"@CONST" ,
	"@GLOBAL" ,
	"@MY" ,
	"@PRIVATE" ,
	"@PROTECTED" ,
	"@PUBLIC" ,
	"@LOAD" ,
	"@MAIN" ,
	"@SUB" ,
	"@VARTYPE" ,
	"@CODE" ,
	"@DEF" ,
	"@METHOD" ,
	"@END"
};
enum {
	SECT_NULL ,
	SECT_REQUIRE ,
	SECT_SOURCE ,
	SECT_INTEGER ,
	SECT_FLOAT ,
	SECT_STRING ,
	SECT_COMPLEX ,
	SECT_LIST ,
	SECT_MAP ,
	SECT_ARRAY ,
	SECT_ROUTINE ,
	SECT_CLASS ,
	SECT_TYPE ,
	SECT_CONST ,
	SECT_GLOBAL ,
	SECT_MY ,
	SECT_PRIVATE ,
	SECT_PROTECTED ,
	SECT_PUBLIC ,
	SECT_LOAD ,
	SECT_MAIN ,
	SECT_SUB ,
	SECT_VARTYPE ,
	SECT_CODE ,
	SECT_DEF ,
	SECT_METHOD ,
	SECT_END ,
};
static void AppendSectVar( DString *bc, DString *mbs )
{
	char *s = mbs->mbs;
	char buf[3];
	ushort_t i = atoi( s+1 );
	buf[0] = s[0];
	buf[1] = i & 0xff;
	buf[2] = (i & 0xff00)>>8;
	DString_AppendDataMBS( bc, buf, 3 );
}
static void AppendSectShort( DString *bc, short v )
{
	char buf[2];
	buf[0] = v & 0xff;
	buf[1] = (v & 0xff00)>>8;
	DString_AppendDataMBS( bc, buf, 2 );
}
static char* NextToken( char *P, DString *t, char mark, DString *bc )
{
	t->size = 0;
	while( *P ==' ' || *P =='\t' || *P =='\n' || *P =='#' ){
		if( *P =='#' ) while( *P && *P !='\n' ) P ++;
		P ++;
	}
	if( mark !='@' && *P =='@' ) return P;
	if( *P =='\"' || *P =='\'' ){
		char open = *( P ++ );
		while( *P != '\n' && *P != open ){
			if( *P =='\\' ) DString_AppendChar( t, *( P ++ ) );
			DString_AppendChar( t, *( P ++ ) );
		}
		printf( "here %s\n", t->mbs );
		if( *( P ++ ) != open ) return NULL;
	}else{
		while( *P && *P!=' ' && *P!='\t' && *P!='\n' && *P!='#')
			DString_AppendChar( t, *(P++));
		printf( "here %s\n", t->mbs );
	}
	while( *P ==' ' || *P =='\t' ) P ++;
	if( *P =='#' ) while( *P !='\n' ) P ++;
	if( bc && mark =='$' ) AppendSectVar( bc, t );
	//  printf( "@TOKEN: %s %c;\n", t->mbs, *P );
	return P;
}
static DaoBase* AsmGetConst( DArray *consts[], DString *mbs, const char type )
{
	const char *id = mbs->mbs;
	int i;
	if( id[0] < 'A' || id[0] > 'Z' || ( type >0 && id[0] != type ) ) return NULL;
	if( consts[id[0]-'A'] == NULL ) return NULL;
	i = atoi( id+1 );
	if( i <0 || i >= consts[id[0]-'A']->size ) return NULL;
	return consts[id[0]-'A']->items.pBase[i];
}
static void AppendSectHead( DString *bc, char sect )
{
	char buf[3];
	buf[0] = buf[1] = 0;
	buf[2] = sect;
	DString_AppendDataMBS( bc, buf, 3 );
}
extern void print_number( char *buf, double value );
extern double parse_number( char *buf );
int DaoParseAssembly( DaoVmSpace *self, DaoNameSpace *ns, DString *src, DString *bc )
{
	DaoParser  *parser = DaoParser_New();
	DaoRoutine *routine = NULL;
	DaoClass   *klass = NULL, *klass2;
	DaoString  *daostr;
	DaoNumber  *num;
	DaoVmCode  *vmc = DaoVmCode_New();
	DaoType *abtp, *abtp2;
	DaoBase    *dbase;
	DaoNameSpace *module, *modas;
	DArray  *consts[26];
	DArray  *nested = DArray_New(0);
	DArray  *varImport = DArray_New(1);
	DMap    *mapRoutine = DMap_New(1,0);
	DMap    *mapVmc = DMap_New(1,0);
	DMap    *mapNames = DMap_New(1,0);
	DNodeKv *node;
	DString *sp;
	DString *tok = DString_New(1);
	DString *str = DString_New(1);
	DString *tka = DString_New(1);
	DString *tkb = DString_New(1);
	DString *tkc = DString_New(1);
	DString *src2 = DString_Copy( src );
	char buf[50];
	char *P = src2->mbs;
	short state = 0;
	short i, code = 0, opa = 0, opb = 0, opc = 0;
	int permi = DAO_DATA_PUBLIC;

	memset( consts, 0, 26*sizeof(DArray*) );
	for(i=0; i<constTypeCount; i++) consts[ constTypeChar[i]-'A' ] = DArray_New(0);
	consts[ 'P'-'A' ] = consts[ 'S'-'A' ]; /* parameter names */
	consts[ 'N'-'A' ] = consts[ 'S'-'A' ]; /* namespace names */
	parser->nameSpace = ns;

	for(i=0; i<=DVM_SECT; i++)
		MAP_Insert( mapVmc, DString_SetMBS( tok, getOpcodeName(i) ), i );
	DString_Clear( tok ); /* for safety: tok->size=0 in NextToken() */

	printf( "DaoParseAssembly()\n" );
	while( *P ){
		//printf( "%s %i %i =%c=\n", sectNames[state], state, SECT_END, *P );
		P = NextToken( P, tok, '@', NULL );
		printf( "tok = %s\n", tok->mbs );

		if( STRCMP( tok, "@REQUIRE" ) ==0 ){
			P = NextToken( P, tok, 0, NULL );
			if( P ==NULL ) goto InvalidFormat;
			if( bc ) AppendSectHead( bc, SECT_REQUIRE );
		}else if( STRCMP( tok, "@SOURCE" ) ==0 ){
			P = NextToken( P, tok, 0, NULL );
			if( P ==NULL ) goto InvalidFormat;
			if( bc ) AppendSectHead( bc, SECT_SOURCE );
		}else if( STRCMP( tok, "@FLOAT" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_FLOAT );
			for(;;){
				P = NextToken( P, tok, 0, NULL );
				if( tok->size ==0 ) break;
				num = DaoDouble_New( strtod( tok->mbs, NULL ) );
				DArray_Append( consts['F'-'A'], num );
				if( bc ){
					DString_Append( bc, tok );
					DString_AppendChar( bc, 0 );
				}
			}
		}else if( STRCMP( tok, "@STRING" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_STRING );
			for(;;){
				P = NextToken( P, tok, 0, NULL );
				if( P ==NULL ) goto InvalidFormat;
				if( tok->size ==0 ) break;
				printf( "@STRING: %s\n", tok->mbs );
				daostr = DaoString_New();
				/* Do not use DString_Assign( daostr->chars, tok ),
				 * because NextToken() may modify "tok" */
				DString_SetMBS( daostr->chars, tok->mbs );
				DArray_Append( consts['S'-'A'], daostr );
				if( bc ){
					DString_Append( bc, tok );
					DString_AppendChar( bc, 0 );
				}
			}
		}else if( STRCMP( tok, "@CLASS" ) ==0 || STRCMP( tok, "@ROUTINE" ) ==0 ){
			int eclass = ( STRCMP( tok, "@CLASS" ) ==0 );
			if( bc ) AppendSectHead( bc, eclass ? SECT_CLASS : SECT_ROUTINE );
			for(;;){
				P = NextToken( P, tok, '$', bc ); // routine name
				if( tok->size ==0 ) break;
				daostr = AsmGetConst( consts, tok, 'S' );
				if( daostr ==NULL ) goto InvalidFormat;
				sp = STRV( daostr );
				if( eclass ){
					klass = DaoClass_New();
					DaoClass_SetName( klass, sp );
					routine = klass->classRoutine;
					DArray_Append( consts['K'-'A'], klass );
				}else{
					routine = DaoRoutine_New();
					DString_Assign( routine->routName, sp );
					DArray_Append( consts['R'-'A'], routine );
				}
				GC_ShiftRC( ns, routine->nameSpace );
				routine->nameSpace = ns;
				routine->locRegCount = 20; //XXX
				opa = DaoNameSpace_FindConst( ns, sp );
				if( opa >=0 ){
					dbase = DaoNameSpace_GetConst( ns, opa );
					if( eclass ==0 && dbase->type == DAO_ROUTINE )
						DRoutine_AddOverLoad( (DRoutine*)dbase, (DRoutine*)routine );
					else goto InvalidFormat;
				}else if( eclass ){
					DaoNameSpace_AddConst( ns, sp, klass );
				}else{
					DaoNameSpace_AddConst( ns, sp, routine );
				}

				abtp2 = NULL;
				DArray_Clear( nested );
				DMap_Clear( mapNames );
				DString_SetMBS( str, "routine<" );
				if( *P == 'T' ){
					P = NextToken( P, tok, '$', bc );
					abtp2 = (DaoType*)AsmGetConst( consts, tok, 'T' );
					if( abtp2 ==NULL ) goto InvalidFormat;
				}
				while( *P == 'P' ){
					P = NextToken( P, tok, '$', bc );
					daostr = (DaoString*)AsmGetConst( consts, tok, 'P' );
					if( daostr ==NULL ) goto InvalidFormat;
					abtp = NULL;
					dbase = NULL;
					if( *P == 'T' ){
						P = NextToken( P, tok, '$', bc );
						abtp = (DaoType*)AsmGetConst( consts, tok, 'T' );
						if( abtp ==NULL ) goto InvalidFormat;
					}
					if( *P != 'P' && *P != '\n' ){
						P = NextToken( P, tok, '$', bc );
						dbase = AsmGetConst( consts, tok, 0 );
						if( dbase ==NULL ) goto InvalidFormat;
						if( abtp ==NULL ) abtp = DaoNameSpace_GetType( ns, dbase );
					}
					if( abtp ==NULL ) abtp = DaoType_New( "?", DAO_UDF, 0,0 );
					sp = STRV( daostr );
					DString_Append( str, sp );
					DArray_Append( nested, abtp );
					MAP_Insert( mapNames, sp, routine->parCount );
					if( dbase ){
						DString_AppendMBS( str, "=" );
						DString_Append( str, abtp->name );
					}else{
						DString_AppendMBS( str, ":" );
						DString_Append( str, abtp->name );
					}
					routine->parCount ++;
				}
				DString_AppendMBS( str, "=>" );
				if( eclass )
					abtp2 = klass->objType;
				else if( abtp2 ==NULL )
					abtp2 = DaoType_New( "?", DAO_UDF, 0,0 );
				DString_Append( str, abtp2->name );
				DString_AppendChar( str, '>' );
				DArray_Append( nested, abtp2 );
				routine->routType = DaoType_New( str->mbs, DAO_ROUTINE, 0, nested );
				routine->routType->mapNames = DMap_Copy( mapNames );
				routine->routType->parCount = routine->parCount;
				printf( "%s %i\n", str->mbs, routine->parCount );
				if( bc ) DString_AppendChar( bc, 0 );
			}
		}else if( STRCMP( tok, "@TYPE" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_TYPE );
			for(;;){
				P = NextToken( P, tok, '$', bc );
				if( tok->size ==0 ) break;
				daostr = (DaoString*)AsmGetConst( consts, tok, 'S' );
				if( daostr ==NULL ) goto InvalidFormat;
				abtp = DaoParser_ParseTypeName( parser, STRV(daostr)->mbs );
				if( abtp ==NULL ) goto InvalidFormat;
				DArray_Append( consts['T'-'A'], abtp );
				printf( "@TYPE: %s\n", abtp->name->mbs );
				if( bc ) DString_AppendChar( bc, 0 );
			}
		}else if( STRCMP( tok, "@CONST" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_CONST );
			for(;;){
				P = NextToken( P, tok, '$', bc );
				if( tok->size ==0 ) break;
				daostr = (DaoString*)AsmGetConst( consts, tok, 'S' );
				if( daostr ==NULL ) goto InvalidFormat;
				if( *P == '\n' ) goto InvalidFormat;
				P = NextToken( P, tok, '$', bc );
				dbase = AsmGetConst( consts, tok, 0 );
				if( dbase ==NULL ) goto InvalidFormat;
				if( klass && routine ==NULL ){
					DaoClass_AddConst( klass, STRV(daostr), dbase, permi );
				}else{
					DaoNameSpace_AddConst( ns, STRV(daostr), dbase );
				}
				if( bc ) DString_AppendChar( bc, 0 );
			}
		}else if( STRCMP( tok, "@GLOBAL" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_GLOBAL );
			for(;;){
				P = NextToken( P, tok, '$', bc );
				if( tok->size ==0 ) break;
				daostr = (DaoString*)AsmGetConst( consts, tok, 'S' );
				if( daostr ==NULL ) goto InvalidFormat;
				abtp = NULL;
				if( *P == 'T' ){
					P = NextToken( P, tok, '$', bc );
					abtp = (DaoType*) AsmGetConst( consts, tok, 'T' );
					if( abtp == NULL ) goto InvalidFormat;
				}
				if( klass && routine ==NULL ){
					DaoClass_AddGlobalVar( klass, STRV(daostr), NULL, permi, abtp );
				}else{
					DaoNameSpace_AddVariable( ns, STRV(daostr), NULL, abtp );
				}
				if( bc ) DString_AppendChar( bc, 0 );
			}
		}else if( STRCMP( tok, "@MY" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_MY );
			for(;;){
				P = NextToken( P, tok, '$', bc );
				if( tok->size ==0 ) break;
				daostr = (DaoString*)AsmGetConst( consts, tok, 'S' );
				if( daostr ==NULL ) goto InvalidFormat;
				abtp = NULL;
				dbase = NULL;
				if( *P == 'T' ){
					P = NextToken( P, tok, '$', bc );
					abtp = (DaoType*) AsmGetConst( consts, tok, 'T' );
					if( abtp == NULL ) goto InvalidFormat;
				}
				if( *P !='\n' ){
					P = NextToken( P, tok, '$', bc );
					dbase = AsmGetConst( consts, tok, 0 );
					if( dbase == NULL ) goto InvalidFormat;
				}
				if( klass && routine ==NULL ){
					DaoClass_AddObjectVar( klass, STRV(daostr), dbase, permi, abtp );
				}else{
					goto InvalidFormat;
				}
				if( bc ) DString_AppendChar( bc, 0 );
			}
		}else if( STRCMP( tok, "@PRIVATE" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_PRIVATE );
			permi = DAO_DATA_PRIVATE;

		}else if( STRCMP( tok, "@PROTECTED" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_PROTECTED );
			permi = DAO_DATA_PROTECTED;

		}else if( STRCMP( tok, "@PUBLIC" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_PUBLIC );
			permi = DAO_DATA_PUBLIC;

		}else if( STRCMP( tok, "@DEF" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_DEF );
			permi = DAO_DATA_PUBLIC;
			P = NextToken( P, tok, '$', bc );
			routine = NULL;
			klass = (DaoClass*)AsmGetConst( consts, tok, 'K' );
			if( klass ==NULL || klass->type != DAO_CLASS ) goto InvalidFormat;
			while( *P == 'K' ){
				P = NextToken( P, tok, '$', bc );
				klass2 = (DaoClass*)AsmGetConst( consts, tok, 'K' );
				if( klass2 ==NULL || klass2->type != DAO_CLASS ) goto InvalidFormat;
				DaoClass_AddSuperClass( klass, klass2 );
			}
		}else if( STRCMP( tok, "@LOAD" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_LOAD );
			if( *P != 'S' ) goto InvalidFormat;
			P = NextToken( P, tok, '$', bc );
			dbase = AsmGetConst( consts, tok, 'S' );
			if( dbase ==NULL ) goto InvalidFormat;
			printf( "@LOAD: %s\n", STRV( dbase )->mbs );
			if( *P =='B' ){ /* By loader Bx */
				P = NextToken( P, tok, '$', bc );
			}else{
				module = DaoVmSpace_LoadModule( self, STRV( dbase ) );
			}
			modas = NULL;
			printf( "module = %p =%c=\n", module, *P );
			if( *P =='N' ){ /* as */
				P = NextToken( P, tok, '$', bc );
				daostr = AsmGetConst( consts, tok, 'N' );
				if( daostr ==NULL ) goto InvalidFormat;
				opa = DaoNameSpace_FindConst( ns, STRV( daostr ) );
				dbase = opa >=0 ? DaoNameSpace_GetConst( ns, opa ) : NULL;
				printf( "dbase : %i\n", dbase ? dbase->type : -1 );
				if( dbase && dbase->type == DAO_NAMESPACE ){
					modas = (DaoNameSpace*) dbase;
				}else if( dbase ){
					goto InvalidFormat;
				}else{
					modas = DaoNameSpace_New( self );
					DaoNameSpace_AddConst( ns, STRV(daostr), modas );
				}
			}
			DArray_Clear( varImport );
			while( *P =='S' ){ /* import */
				P = NextToken( P, tok, '$', bc );
				daostr = AsmGetConst( consts, tok, 'S' );
				if( daostr ==NULL ) goto InvalidFormat;
				DArray_Append( varImport, STRV( daostr ) );
			}
			if( modas ){
				DaoNameSpace_Import( modas, module, varImport );
			}else{
				DaoNameSpace_Import( ns, module, varImport );
			}

		}else if( STRCMP( tok, "@MAIN" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_MAIN );
			klass = NULL;
			routine = DaoRoutine_New();
			GC_IncRC( ns );
			routine->nameSpace = ns;
			ns->mainRoutine = routine;//XXX
			printf( "@MAIN: ;%c;\n", *P );
			P = NextToken( P, tok, 0, NULL );
			if( *P == '@' ) goto InvalidFormat; 
			routine->locRegCount = atoi( tok->mbs );
			if( bc ) AppendSectShort( bc, routine->locRegCount );

		}else if( STRCMP( tok, "@SUB" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_SUB );
			routine = NULL;
			P = NextToken( P, tok, '$', bc );
			if( tok->mbs[0] == 'K' ){
				klass = (DaoClass*)AsmGetConst( consts, tok, 'K' );
				DaoClass_DeriveClassData( klass );
				routine = klass->classRoutine;
			}else if( tok->mbs[0] == 'R' ){
				klass = NULL;
				routine = (DaoRoutine*)AsmGetConst( consts, tok, 'R' );
			}
			printf( "@SUB: %p ;%c;\n", routine, *P );
			if( routine ==NULL ) goto InvalidFormat;
			printf( "@SUB: %s %p ;%c;\n", routine->routName->mbs, routine, *P );
			if( *P !='\n' ){
				P = NextToken( P, tok, 0, NULL );
				routine->locRegCount = atoi( tok->mbs );
				if( bc ) AppendSectShort( bc, routine->locRegCount );
			}else goto InvalidFormat; 

		}else if( STRCMP( tok, "@CODE" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_CODE );
			//printf( "@CODE: %s\n", tok->mbs );
			for(;;){
				P = NextToken( P, tok, 0, NULL ); if( *P =='\n' ) goto InvalidFormat;
				if( tok->size ==0 ) break;
				P = NextToken( P, tka, 0, NULL ); if( *P =='\n' ) goto InvalidFormat;
				P = NextToken( P, tkb, 0, NULL ); if( *P =='\n' ) goto InvalidFormat;
				P = NextToken( P, tkc, 0, NULL ); if( *P !='\n' ) goto InvalidFormat;
				opa = atoi( tka->mbs );
				opb = atoi( tkb->mbs );
				opc = atoi( tkc->mbs );
				node = MAP_Find( mapVmc, tok );
				if( node == NULL ) goto InvalidFormat;
				vmc->opCode = node->value.pInt;
				if( bc ) DString_AppendChar( bc, vmc->opCode+1 );
				DaoVmCode_Print( vmc, NULL );
				if( vmc->opCode == DVM_GETCL ){
					dbase = AsmGetConst( consts, tka, 0 );
					if( dbase ==NULL ) goto InvalidFormat;
					opa = DRoutine_AddConst( (DRoutine*)routine, dbase );
					if( bc ){
						DString_AppendChar( bc, tka->mbs[0] );
						AppendSectShort( bc, atoi( tka->mbs+1 ) );
						DString_AppendChar( bc, 0 );
						AppendSectShort( bc, opc );
					}
				}else if( vmc->opCode >= DVM_GETCG && vmc->opCode <= DVM_GETVO ){
					dbase = AsmGetConst( consts, tka, 'S' );
					if( dbase ==NULL ) goto InvalidFormat;
					printf( "name = ;%s;\n", STRV(dbase)->mbs );
					switch( vmc->opCode ){
					case DVM_GETCG :
						opa = DaoNameSpace_FindConst( ns, STRV( dbase ) );
						break;
					case DVM_GETCK :
						node = MAP_Find( klass->cstDataIndex, STRV( dbase ) );
						if( node ==NULL ) goto InvalidFormat;
						opa = node->value.pInt;
						break;
					case DVM_GETVG :
						opa = DaoNameSpace_FindVariable( ns, STRV( dbase ) );
						break;
					case DVM_GETVK :
						node = MAP_Find( klass->glbDataIndex, STRV( dbase ) );
						if( node ==NULL ) goto InvalidFormat;
						opa = node->value.pInt;
						break;
					case DVM_GETVO :
						node = MAP_Find( klass->objDataIndex, STRV( dbase ) );
						if( node ==NULL ) goto InvalidFormat;
						opa = node->value.pInt;
						break;
					default : break;
					}
					if( bc ){
						DString_AppendChar( bc, tka->mbs[0] );
						AppendSectShort( bc, atoi( tka->mbs+1 ) );
						DString_AppendChar( bc, 0 );
						AppendSectShort( bc, opc );
					}
				}else if( vmc->opCode >= DVM_SETVG && vmc->opCode <= DVM_SETVO ){
					dbase = AsmGetConst( consts, tkc, 'S' );
					if( dbase ==NULL ) goto InvalidFormat;
					switch( vmc->opCode ){
					case DVM_SETVG :
						opc = DaoNameSpace_FindVariable( ns, STRV( dbase ) );
						break;
					case DVM_SETVK :
						node = MAP_Find( klass->glbDataIndex, STRV( dbase ) );
						if( node ==NULL ) goto InvalidFormat;
						opc = node->value.pInt;
						break;
					case DVM_SETVO :
						node = MAP_Find( klass->objDataIndex, STRV( dbase ) );
						if( node ==NULL ) goto InvalidFormat;
						opc = node->value.pInt;
						break;
					default : break;
					}
					if( bc ){
						AppendSectShort( bc, opa );
						DString_AppendChar( bc, 0 );
						DString_AppendChar( bc, tkc->mbs[0] );
						AppendSectShort( bc, atoi( tkc->mbs+1 ) );
					}
				}else{
					if( bc ){
						AppendSectShort( bc, opa );
						AppendSectShort( bc, opb );
						AppendSectShort( bc, opc );
					}
				}
				vmc->opA = opa; vmc->opB = opb; vmc->opC = opc;
				printf( "opa = %i; opb = %i; opc = %i\n", opa, opb, opc );
				DArray_Append( routine->vmCodes, vmc );
			}
			if( STRCMP( tok, "RETURN" ) !=0 ){
				vmc->opCode = DVM_RETURN;
				vmc->opA = vmc->opB = vmc->opC = 0;
				DArray_Append( routine->vmCodes, vmc );
			}
			if( DaoRoutine_SetVmCodes( routine, routine->vmCodes ) ==0 ) goto InvalidFormat;
			if( bc ) DString_AppendChar( bc, 0 );
		}else if( STRCMP( tok, "@END" ) ==0 ){
			if( bc ) AppendSectHead( bc, SECT_END );
			break;
		}
	}
	if( bc ) AppendSectShort( bc, 0 );
	printf( "assembled ...\n" );
	return 1;
InvalidFormat :
	return 0;
}
static DaoBase* BcGetConst( DArray *consts[], const char id[3], const char type,
		DString *asmc )
{
	ushort_t i;
	if( id[0] < 'A' || id[0] > 'Z' || ( type >0 && id[0] != type ) ) return NULL;
	if( consts[id[0]-'A'] == NULL ) return NULL;
	i = id[1] | ( (ushort_t)id[2] << 8 );
	printf( "id = %i %i %i\n", i, id[1], id[2] );
	if( i <0 || i >= consts[id[0]-'A']->size ) return NULL;
	if( asmc ){
		char buf[20];
		sprintf( buf, "%c%i ", id[0], i );
		DString_AppendMBS( asmc, buf );
	}
	return consts[id[0]-'A']->items.pBase[i];
}
int DaoParseByteCode( DaoVmSpace *self, DaoNameSpace *ns, DString *src, DString *asmc )
{
	DaoParser  *parser = DaoParser_New();
	DaoRoutine *routine = NULL;
	DaoClass   *klass = NULL, *klass2;
	DaoString  *daostr;
	DaoNumber  *num;
	DaoVmCode  *vmc = DaoVmCode_New();
	DaoType *abtp, *abtp2;
	DaoBase    *dbase;
	DaoNameSpace *module, *modas;
	DArray  *consts[26];
	DArray  *nested = DArray_New(0);
	DArray  *varImport = DArray_New(1);
	DMap    *mapRoutine = DMap_New(1,0);
	DMap    *mapVmc = DMap_New(1,0);
	DMap    *mapNames = DMap_New(1,0);
	DNodeKv *node;
	DString *sp;
	DString *tok = DString_New(1);
	DString *str = DString_New(1);
	DString *tka = DString_New(1);
	DString *tkb = DString_New(1);
	DString *tkc = DString_New(1);
	DString *src2 = DString_Copy( src );
	char buf[50];
	char *P = src2->mbs;
	short sect = 0;
	short i, code = 0, opa = 0, opb = 0, opc = 0;
	int permi = DAO_DATA_PUBLIC;

	memset( consts, 0, 26*sizeof(DArray*) );
	for(i=0; i<constTypeCount; i++) consts[ constTypeChar[i]-'A' ] = DArray_New(0);
	consts[ 'P'-'A' ] = consts[ 'S'-'A' ]; /* parameter names */
	consts[ 'N'-'A' ] = consts[ 'S'-'A' ]; /* namespace names */
	parser->nameSpace = ns;
	printf( "size = %i\n", src2->size );
	printf( "P = %c %c %s\n", P[0], P[1], sectNames[P[2]] );
	while( ( P - src2->mbs ) < src2->size ){
		if( *P ==0 && *(P+1)==0 ){
			P += 2;
			continue;
		}
		sect = *( P ++ );
		printf( "P2 = %i %i %s\n", sect, (P-src2->mbs), sectNames[sect] );
		if( asmc && sect ){
			DString_AppendMBS( asmc, sectNames[sect] );
			DString_AppendChar( asmc, '\n' );
		}
		switch( sect ){
		case SECT_NULL :
			break;
		case SECT_REQUIRE :
			break;
		case SECT_SOURCE :
			break;
		case SECT_FLOAT :
			while( *P && *(P+1) ){
				num = DaoDouble_New( strtod( P, NULL ) );
				DArray_Append( consts['F'-'A'], num );
				if( asmc ){
					DString_AppendMBS( asmc, P );
					DString_AppendChar( asmc, '\n' );
				}
				P += strlen( P )+1;
			}
			break;
		case SECT_STRING :
			while( *P && *(P+1) ){
				daostr = DaoString_New();
				DString_SetMBS( daostr->chars, P );
				printf( "%s\n", P );
				DArray_Append( consts['S'-'A'], daostr );
				if( asmc ){
					DString_AppendChar( asmc, '\"' );
					DString_AppendMBS( asmc, P );
					DString_AppendMBS( asmc, "\"\n" );
				}
				P += strlen( P )+1;
			}
			break;
		case SECT_COMPLEX :
			break;
		case SECT_LIST :
			break;
		case SECT_MAP :
			break;
		case SECT_ARRAY :
			break;
		case SECT_ROUTINE :
		case SECT_CLASS :
			while( *P && *(P+1) ){
				daostr = BcGetConst( consts, P, 'S', asmc );
				P += 3;
				if( daostr ==NULL ) goto InvalidFormat;
				sp = STRV( daostr );
				if( sect == SECT_CLASS ){
					klass = DaoClass_New();
					DaoClass_SetName( klass, sp );
					routine = klass->classRoutine;
					DArray_Append( consts['K'-'A'], klass );
				}else{
					routine = DaoRoutine_New();
					DString_Assign( routine->routName, sp );
					DArray_Append( consts['R'-'A'], routine );
				}
				printf( "routine %s()\n", sp->mbs );
				GC_ShiftRC( ns, routine->nameSpace );
				routine->nameSpace = ns;
				routine->locRegCount = 20; //XXX
				opa = DaoNameSpace_FindConst( ns, sp );
				if( opa >=0 ){
					dbase = DaoNameSpace_GetConst( ns, opa );
					if( sect == SECT_ROUTINE && dbase->type == DAO_ROUTINE )
						DRoutine_AddOverLoad( (DRoutine*)dbase, (DRoutine*)routine );
					else goto InvalidFormat;
				}else if( sect == SECT_CLASS ){
					DaoNameSpace_AddConst( ns, sp, klass );
				}else{
					DaoNameSpace_AddConst( ns, sp, routine );
				}

				abtp2 = NULL;
				DArray_Clear( nested );
				DMap_Clear( mapNames );
				DString_SetMBS( str, "routine<" );
				if( *P == 'T' ){
					abtp2 = (DaoType*)BcGetConst( consts, P, 'T', asmc );
					P += 3;
					if( abtp2 ==NULL ) goto InvalidFormat;
				}
				while( *P == 'P' ){
					daostr = (DaoString*)BcGetConst( consts, P, 'P', asmc );
					P += 3;
					if( daostr ==NULL ) goto InvalidFormat;
					abtp = NULL;
					dbase = NULL;
					if( *P == 'T' ){
						abtp = (DaoType*)BcGetConst( consts, P, 'T', asmc );
						P += 3;
						if( abtp ==NULL ) goto InvalidFormat;
					}
					if( *P != 'P' && *P != 0 ){
						dbase = BcGetConst( consts, P, 0, asmc );
						P += 3;
						if( dbase ==NULL ) goto InvalidFormat;
						if( abtp ==NULL ) abtp = DaoNameSpace_GetType( ns, dbase );
					}
					if( abtp ==NULL ) abtp = DaoType_New( "?", DAO_UDF, 0,0 );
					sp = STRV( daostr );
					DString_Append( str, sp );
					DArray_Append( nested, abtp );
					MAP_Insert( mapNames, sp, routine->parCount );
					if( dbase ){
						DString_AppendMBS( str, "=" );
						DString_Append( str, abtp->name );
					}else{
						DString_AppendMBS( str, ":" );
						DString_Append( str, abtp->name );
					}
					routine->parCount ++;
				}
				DString_AppendMBS( str, "=>" );
				if( sect == SECT_CLASS )
					abtp2 = klass->objType;
				else if( abtp2 ==NULL )
					abtp2 = DaoType_New( "?", DAO_UDF, 0,0 );
				DString_Append( str, abtp2->name );
				DString_AppendChar( str, '>' );
				DArray_Append( nested, abtp2 );
				routine->routType = DaoType_New( str->mbs, DAO_ROUTINE, 0, nested );
				routine->routType->mapNames = DMap_Copy( mapNames );
				routine->routType->parCount = routine->parCount;
				printf( "BC routine: %s %i\n", str->mbs, routine->parCount );
				P ++;
				if( asmc ) DString_AppendChar( asmc, '\n' );
			}
			break;
		case SECT_TYPE :
			while( *P && *(P+1) ){
				daostr = (DaoString*)BcGetConst( consts, P, 'S', asmc );
				P += 3;
				if( daostr ==NULL ) goto InvalidFormat;
				abtp = DaoParser_ParseTypeName( parser, STRV(daostr)->mbs );
				if( abtp ==NULL ) goto InvalidFormat;
				DArray_Append( consts['T'-'A'], abtp );
				printf( "@TYPE: %s\n", abtp->name->mbs );
				P ++;
				if( asmc ) DString_AppendChar( asmc, '\n' );
			}
			break;
		case SECT_CONST :
			while( *P && *(P+1) ){
				daostr = (DaoString*)BcGetConst( consts, P, 'S', asmc );
				P += 3;
				if( daostr ==NULL ) goto InvalidFormat;
				if( *P ==0 ) goto InvalidFormat;
				dbase = BcGetConst( consts, P, 0, asmc );
				P += 3;
				if( dbase ==NULL ) goto InvalidFormat;
				if( klass && routine ==NULL ){
					DaoClass_AddConst( klass, STRV(daostr), dbase, permi );
				}else{
					DaoNameSpace_AddConst( ns, STRV(daostr), dbase );
				}
				P ++;
				if( asmc ) DString_AppendChar( asmc, '\n' );
			}
			break;
		case SECT_GLOBAL :
			while( *P && *(P+1) ){
				daostr = (DaoString*)BcGetConst( consts, P, 'S', asmc );
				P += 3;
				if( daostr ==NULL ) goto InvalidFormat;
				abtp = NULL;
				if( *P == 'T' ){
					abtp = (DaoType*) BcGetConst( consts, P, 'T', asmc );;
					P += 3;
					if( abtp == NULL ) goto InvalidFormat;
				}
				if( klass && routine ==NULL ){
					DaoClass_AddGlobalVar( klass, STRV(daostr), NULL, permi, abtp );
				}else{
					DaoNameSpace_AddVariable( ns, STRV(daostr), NULL, abtp );
				}
				P ++;
				if( asmc ) DString_AppendChar( asmc, '\n' );
			}
			break;
		case SECT_MY :
			while( *P && *(P+1) ){
				daostr = (DaoString*)BcGetConst( consts, P, 'S', asmc );
				P += 3;
				if( daostr ==NULL ) goto InvalidFormat;
				abtp = NULL;
				dbase = NULL;
				if( *P == 'T' ){
					abtp = (DaoType*) BcGetConst( consts, P, 'T', asmc );;
					P += 3;
					if( abtp == NULL ) goto InvalidFormat;
				}
				if( *P !=0 ){
					dbase = BcGetConst( consts, P, 0, asmc );
					P += 3;
					if( dbase == NULL ) goto InvalidFormat;
				}
				if( klass && routine ==NULL ){
					DaoClass_AddObjectVar( klass, STRV(daostr), dbase, permi, abtp );
				}else{
					goto InvalidFormat;
				}
				P ++;
				if( asmc ) DString_AppendChar( asmc, '\n' );
			}
			break;
		case SECT_PRIVATE :
			permi = DAO_DATA_PRIVATE;
			break;
		case SECT_PROTECTED :
			permi = DAO_DATA_PROTECTED;
			break;
		case SECT_PUBLIC :
			permi = DAO_DATA_PUBLIC;
			break;
		case SECT_LOAD :
			if( *P != 'S' ) goto InvalidFormat;
			dbase = BcGetConst( consts, P, 'S', asmc );
			P += 3;
			if( dbase ==NULL ) goto InvalidFormat;
			printf( "@LOAD: %s\n", STRV( dbase )->mbs );
			if( *P =='B' ){ /* By loader Bx */
				P += 3;
			}else{
				module = DaoVmSpace_LoadModule( self, STRV( dbase ) );
			}
			modas = NULL;
			printf( "module = %p =%c=\n", module, *P );
			if( *P =='N' ){ /* as */
				daostr = BcGetConst( consts, P, 'N', asmc );
				P += 3;
				if( daostr ==NULL ) goto InvalidFormat;
				opa = DaoNameSpace_FindConst( ns, STRV( daostr ) );
				dbase = opa >=0 ? DaoNameSpace_GetConst( ns, opa ) : NULL;
				printf( "dbase : %i\n", dbase ? dbase->type : -1 );
				if( dbase && dbase->type == DAO_NAMESPACE ){
					modas = (DaoNameSpace*) dbase;
				}else if( dbase ){
					goto InvalidFormat;
				}else{
					modas = DaoNameSpace_New( self );
					DaoNameSpace_AddConst( ns, STRV(daostr), modas );
				}
			}
			DArray_Clear( varImport );
			while( *P =='S' ){ /* import */
				daostr = BcGetConst( consts, P, 'S', asmc );
				P += 3;
				if( daostr ==NULL ) goto InvalidFormat;
				DArray_Append( varImport, STRV( daostr ) );
			}
			if( modas ){
				DaoNameSpace_Import( modas, module, varImport );
			}else{
				DaoNameSpace_Import( ns, module, varImport );
			}
			if( asmc ) DString_AppendChar( asmc, '\n' );
			break;
		case SECT_MAIN :
			klass = NULL;
			routine = DaoRoutine_New();
			GC_IncRC( ns );
			routine->nameSpace = ns;
			ns->mainRoutine = routine;//XXX
			if( *P ==0 && *(P+1) ==0 ) goto InvalidFormat; 
			routine->locRegCount = P[0] | (P[1]<<8);
			printf( "count = %i\n", routine->locRegCount );
			P += 2;
			if( asmc ){
				sprintf( buf, "%i\n", routine->locRegCount );
				DString_AppendMBS( asmc, buf );
			}
			break;
		case SECT_SUB :
			routine = NULL;
			if( *P == 'K' ){
				klass = (DaoClass*)BcGetConst( consts, P, 'K', asmc );
				DaoClass_DeriveClassData( klass );
				routine = klass->classRoutine;
			}else if( *P == 'R' ){
				klass = NULL;
				routine = (DaoRoutine*)BcGetConst( consts, P, 'R', asmc );
			}
			P += 3;
			if( routine ==NULL ) goto InvalidFormat;
			if( *P ==0 && *(P+1) ==0 ) goto InvalidFormat; 
			routine->locRegCount = P[0] | (P[1]<<8);
			P += 2;
			if( asmc ){
				sprintf( buf, "%i\n", routine->locRegCount );
				DString_AppendMBS( asmc, buf );
			}
			break;
		case SECT_VARTYPE :
			break;
		case SECT_CODE :
			for( ; *P; P+=7 ){
				opa = P[1] | ( P[2] <<8 );
				opb = P[3] | ( P[4] <<8 );
				opc = P[5] | ( P[6] <<8 );
				vmc->opCode = P[0]-1;
				DaoVmCode_Print( vmc, NULL );
				if( asmc ){
					DString_AppendMBS( asmc, getOpcodeName( vmc->opCode ) );
					DString_AppendChar( asmc, ' ' );
				}
				if( vmc->opCode == DVM_GETCL ){
					dbase = BcGetConst( consts, P+1, 0, asmc );
					if( asmc ){
						sprintf( buf, "%i %i\n", opb, opc );
						DString_AppendMBS( asmc, buf );
					}
					if( dbase ==NULL ) goto InvalidFormat;
					opa = DRoutine_AddConst( (DRoutine*)routine, dbase );
				}else if( vmc->opCode >= DVM_GETCG && vmc->opCode <= DVM_GETVO ){
					dbase = BcGetConst( consts, P+1, 'S', asmc );
					if( asmc ){
						sprintf( buf, "%i %i\n", opb, opc );
						DString_AppendMBS( asmc, buf );
					}
					printf( "name = ;%p;\n", dbase );
					if( dbase ==NULL ) goto InvalidFormat;
					switch( vmc->opCode ){
					case DVM_GETCG :
						opa = DaoNameSpace_FindConst( ns, STRV( dbase ) );
						break;
					case DVM_GETCK :
						node = MAP_Find( klass->cstDataIndex, STRV( dbase ) );
						if( node ==NULL ) goto InvalidFormat;
						opa = node->value.pInt;
						break;
					case DVM_GETVG :
						opa = DaoNameSpace_FindVariable( ns, STRV( dbase ) );
						break;
					case DVM_GETVK :
						node = MAP_Find( klass->glbDataIndex, STRV( dbase ) );
						if( node ==NULL ) goto InvalidFormat;
						opa = node->value.pInt;
						break;
					case DVM_GETVO :
						node = MAP_Find( klass->objDataIndex, STRV( dbase ) );
						if( node ==NULL ) goto InvalidFormat;
						opa = node->value.pInt;
						break;
					default : break;
					}
				}else if( vmc->opCode >= DVM_SETVG && vmc->opCode <= DVM_SETVO ){
					if( asmc ){
						sprintf( buf, "%i %i\n", opa, opb );
						DString_AppendMBS( asmc, buf );
					}
					dbase = BcGetConst( consts, P+4, 'S', asmc );
					if( asmc ) DString_AppendChar( asmc, '\n' );
					if( dbase ==NULL ) goto InvalidFormat;
					switch( vmc->opCode ){
					case DVM_SETVG :
						opc = DaoNameSpace_FindVariable( ns, STRV( dbase ) );
						break;
					case DVM_SETVK :
						node = MAP_Find( klass->glbDataIndex, STRV( dbase ) );
						if( node ==NULL ) goto InvalidFormat;
						opc = node->value.pInt;
						break;
					case DVM_SETVO :
						node = MAP_Find( klass->objDataIndex, STRV( dbase ) );
						if( node ==NULL ) goto InvalidFormat;
						opc = node->value.pInt;
						break;
					default : break;
					}
				}else{
					if( asmc ){
						sprintf( buf, "%i %i %i\n", opa, opb, opc );
						DString_AppendMBS( asmc, buf );
					}
				}
				vmc->opA = opa; vmc->opB = opb; vmc->opC = opc;
				printf( "opa = %i; opb = %i; opc = %i\n", opa, opb, opc );
				DArray_Append( routine->vmCodes, vmc );
			}
			if( STRCMP( tok, "RETURN" ) !=0 ){
				vmc->opCode = DVM_RETURN;
				vmc->opA = vmc->opB = vmc->opC = 0;
				DArray_Append( routine->vmCodes, vmc );
			}
			if( asmc ) DString_AppendChar( asmc, '\n' );
			if( DaoRoutine_SetVmCodes( routine, routine->vmCodes ) ==0 ) goto InvalidFormat;
			break;
		case SECT_DEF :
			permi = DAO_DATA_PUBLIC;
			routine = NULL;
			klass = (DaoClass*)BcGetConst( consts, P, 'K', asmc );
			P += 3;
			if( klass ==NULL || klass->type != DAO_CLASS ) goto InvalidFormat;
			while( *P == 'K' ){
				klass2 = (DaoClass*)BcGetConst( consts, P, 'K', asmc );
				P += 3;
				if( klass2 ==NULL || klass2->type != DAO_CLASS ) goto InvalidFormat;
				DaoClass_AddSuperClass( klass, klass2 );
			}
			if( asmc ) DString_AppendChar( asmc, '\n' );
			break;
		case SECT_METHOD :
			break;
		case SECT_END :
			break;
		default : break;
		}
	}
	printf( "assembled ...\n" );
	return 1;
InvalidFormat :
	printf( "assembled ... error\n" );
	return 0;
}

#endif
