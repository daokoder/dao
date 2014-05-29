/*
// Dao Standard Modules
// http://www.daovm.net
//
// Copyright (c) 2011,2012, Limin Fu
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

#include<stdlib.h>
#include<string.h>
#include<math.h>
#include"daoString.h"
#include"daoValue.h"
#include"daoParser.h"
#include"daoNamespace.h"
#include"daoNumtype.h"
#include"daoGC.h"
#include"dao_aux.h"



#define DefineFunction_DaoArray_SetVector( name, type ) \
void name( DaoArray *self, type *vec, daoint N ) \
{ \
	daoint i; \
	if( vec && N == 0 ){ \
		DaoArray_UseData( self, vec ); \
		return; \
	} \
	if( N != self->size ) DaoArray_ResizeData( self, N, self->size ); \
	switch( self->etype ){ \
	case DAO_INTEGER : for(i=0; i<N; i++) self->data.i[i] = (daoint) vec[i]; break; \
	case DAO_FLOAT   : for(i=0; i<N; i++) self->data.f[i] = vec[i]; break; \
	case DAO_DOUBLE  : for(i=0; i<N; i++) self->data.d[i] = vec[i]; break; \
	case DAO_COMPLEX : \
		for(i=0; i<N; i++){ \
			self->data.c[i].real = vec[i+i]; \
			self->data.c[i].imag = vec[i+i+1]; \
		} \
		break; \
	default : break; \
	} \
}

DefineFunction_DaoArray_SetVector( DaoArray_SetVectorSB, signed char );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorSS, signed short );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorSI, signed int );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorUB, unsigned char );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorUS, unsigned short );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorUI, unsigned int );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorI, daoint );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorF, float );
DefineFunction_DaoArray_SetVector( DaoArray_SetVectorD, double );

#define DefineFunction_DaoArray_SetMatrix( name, type ) \
void name( DaoArray *self, type **mat, daoint R, daoint C ) \
{ \
	daoint dm[2]; \
	daoint i, N = R * C; \
	dm[0] = R; dm[1] = C; \
	if( N != self->size ) DaoArray_ResizeData( self, N, self->size ); \
	DaoArray_Reshape( self, dm, 2 ); \
	switch( self->etype ){ \
	case DAO_INTEGER : for(i=0; i<N; i++) self->data.i[i] = (daoint)mat[i/R][i%R]; break; \
	case DAO_FLOAT   : for(i=0; i<N; i++) self->data.f[i] = mat[i/R][i%R]; break; \
	case DAO_DOUBLE  : for(i=0; i<N; i++) self->data.d[i] = mat[i/R][i%R]; break; \
	case DAO_COMPLEX : for(i=0; i<N; i++){ \
						   self->data.c[i].real = mat[i/R][2*(i%R)]; \
						   self->data.c[i].imag = mat[i/R][2*(i%R)+1]; \
					   } \
					   break; \
	default : break; \
	} \
}

DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixSB, signed char );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixSS, signed short );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixSI, signed int );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixUB, unsigned char );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixUS, unsigned short );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixUI, unsigned int );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixI, daoint );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixF, float );
DefineFunction_DaoArray_SetMatrix( DaoArray_SetMatrixD, double );


#ifdef DAO_WITH_NUMARRAY
DaoArray* DaoProcess_NewVectorSB( DaoProcess *self, signed char *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorSB( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorUB( DaoProcess *self, unsigned char *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorUB( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorSS( DaoProcess *self, signed short *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorSS( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorUS( DaoProcess *self, unsigned short *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorUS( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorSI( DaoProcess *self, signed int *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorSI( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorUI( DaoProcess *self, unsigned int *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorUI( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorI( DaoProcess *self, daoint *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetVectorI( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorF( DaoProcess *self, float *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_FLOAT );
	if( s ) DaoArray_SetVectorF( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewVectorD( DaoProcess *self, double *s, daoint n )
{
	DaoArray *res = DaoArray_New( DAO_DOUBLE );
	if( s ) DaoArray_SetVectorD( res, s, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixSB( DaoProcess *self, signed char **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixSB( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixUB( DaoProcess *self, unsigned char **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixUB( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixSS( DaoProcess *self, signed short **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixSS( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixUS( DaoProcess *self, unsigned short **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixUS( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixSI( DaoProcess *self, signed int **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixSI( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixUI( DaoProcess *self, unsigned int **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixUI( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixI( DaoProcess *self, daoint **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_INTEGER );
	if( s ) DaoArray_SetMatrixI( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixF( DaoProcess *self, float **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_FLOAT );
	if( s ) DaoArray_SetMatrixF( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewMatrixD( DaoProcess *self, double **s, daoint n, daoint m )
{
	DaoArray *res = DaoArray_New( DAO_DOUBLE );
	if( s ) DaoArray_SetMatrixD( res, s, n, m );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}
DaoArray* DaoProcess_NewBuffer( DaoProcess *self, void *p, daoint n )
{
	DaoArray *res = DaoArray_New(0);
	DaoArray_SetBuffer( res, p, n );
	DaoProcess_CacheValue( self, (DaoValue*) res );
	return res;
}


DaoArray* DaoProcess_PutVectorSB( DaoProcess *self, signed  char *array, daoint N )
{
	DaoArray *res = DaoProcess_PutArray( self );
	DaoArray_SetNumType( res, DAO_INTEGER );
	if( array ) DaoArray_SetVectorSB( res, array, N );
	return res;
}
DaoArray* DaoProcess_PutVectorUB( DaoProcess *self, unsigned char *array, daoint N )
{
	DaoArray *res = DaoProcess_PutArray( self );
	DaoArray_SetNumType( res, DAO_INTEGER );
	if( array ) DaoArray_SetVectorUB( res, array, N );
	return res;
}
DaoArray* DaoProcess_PutVectorSS( DaoProcess *self, signed  short *array, daoint N )
{
	DaoArray *res = DaoProcess_PutArray( self );
	DaoArray_SetNumType( res, DAO_INTEGER );
	if( array ) DaoArray_SetVectorSS( res, array, N );
	return res;
}
DaoArray* DaoProcess_PutVectorUS( DaoProcess *self, unsigned short *array, daoint N )
{
	DaoArray *res = DaoProcess_PutArray( self );
	DaoArray_SetNumType( res, DAO_INTEGER );
	if( array ) DaoArray_SetVectorUS( res, array, N );
	return res;
}
DaoArray* DaoProcess_PutVectorSI( DaoProcess *self, signed  int *array, daoint N )
{
	DaoArray *res = DaoProcess_PutArray( self );
	DaoArray_SetNumType( res, DAO_INTEGER );
	if( array ) DaoArray_SetVectorSI( res, array, N );
	return res;
}
DaoArray* DaoProcess_PutVectorUI( DaoProcess *self, unsigned int *array, daoint N )
{
	DaoArray *res = DaoProcess_PutArray( self );
	DaoArray_SetNumType( res, DAO_INTEGER );
	if( array ) DaoArray_SetVectorUI( res, array, N );
	return res;
}
DaoArray* DaoProcess_PutVectorI( DaoProcess *self, daoint *array, daoint N )
{
	DaoArray *res = DaoProcess_PutArray( self );
	DaoArray_SetNumType( res, DAO_INTEGER );
	if( array ) DaoArray_SetVectorI( res, array, N );
	return res;
}
DaoArray* DaoProcess_PutVectorF( DaoProcess *self, float *array, daoint N )
{
	DaoArray *res = DaoProcess_PutArray( self );
	DaoArray_SetNumType( res, DAO_FLOAT );
	if( array ) DaoArray_SetVectorF( res, array, N );
	return res;
}
DaoArray* DaoProcess_PutVectorD( DaoProcess *self, double *array, daoint N )
{
	DaoArray *res = DaoProcess_PutArray( self );
	DaoArray_SetNumType( res, DAO_DOUBLE );
	if( array ) DaoArray_SetVectorD( res, array, N );
	return res;
}
DaoArray* DaoProcess_PutVectorC( DaoProcess *self, complex16 *array, daoint N )
{
	DaoArray *res = DaoProcess_PutArray( self );
	DaoArray_SetNumType( res, DAO_COMPLEX );
	if( array ) DaoArray_SetVectorD( res, (double*)array, N );
	return res;
}

#else

static DaoArray* DaoValue_NewArray()
{
	printf( "Error: numeric array is disabled!\n" );
	return NULL;
}
DaoArray* DaoProcess_NewVectorB( DaoProcess *self, char *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorUB( DaoProcess *self, unsigned char *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorS( DaoProcess *self, short *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorUS( DaoProcess *self, unsigned short *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorI( DaoProcess *self, daoint *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorUI( DaoProcess *self, unsigned int *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorF( DaoProcess *self, float *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewVectorD( DaoProcess *self, double *s, daoint n )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixB( DaoProcess *self, signed char **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixUB( DaoProcess *self, unsigned char **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixS( DaoProcess *self, short **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixUS( DaoProcess *self, unsigned short **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixI( DaoProcess *self, daoint **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixUI( DaoProcess *self, unsigned int **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixF( DaoProcess *self, float **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewMatrixD( DaoProcess *self, double **s, daoint n, daoint m )
{
	return DaoValue_NewArray();
}
DaoArray* DaoProcess_NewBuffer( DaoProcess *self, void *s, daoint n )
{
	return DaoValue_NewArray();
}

static DaoArray* NullArray( DaoProcess *self )
{
	DaoProcess_RaiseError( self, NULL, getCtInfo( DAO_DISABLED_NUMARRAY ) );
	return NULL;
}
DaoArray* DaoProcess_PutVectorSB( DaoProcess *s, signed  char *v, daoint N ){ return NullArray(s); }
DaoArray* DaoProcess_PutVectorUB( DaoProcess *s, unsigned char *v, daoint N ){ return NullArray(s); }
DaoArray* DaoProcess_PutVectorSS( DaoProcess *s, signed  short *v, daoint N ){ return NullArray(s); }
DaoArray* DaoProcess_PutVectorUS( DaoProcess *s, unsigned short *v, daoint N ){ return NullArray(s); }
DaoArray* DaoProcess_PutVectorSI( DaoProcess *s, signed  int *v, daoint N ){ return NullArray(s); }
DaoArray* DaoProcess_PutVectorUI( DaoProcess *s, unsigned int *v, daoint N ){ return NullArray(s); }
DaoArray* DaoProcess_PutVectorI( DaoProcess *s, daoint *v, daoint n ){ return NullArray(s); }
DaoArray* DaoProcess_PutVectorF( DaoProcess *s, float *v, daoint n ){ return NullArray(s); }
DaoArray* DaoProcess_PutVectorD( DaoProcess *s, double *v, daoint n ){ return NullArray(s); }
DaoArray* DaoProcess_PutVectorC( DaoProcess *s, complex16 *v, daoint n ){ return NullArray(s); }
#endif


static void AUX_Tokenize( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *source = p[0]->xString.value;
	DaoList *list = DaoProcess_PutList( proc );
	DaoLexer *lexer = DaoLexer_New();
	DArray *tokens = lexer->tokens;
	int i, rc = 0;
	rc = DaoLexer_Tokenize( lexer, source->chars, DAO_LEX_COMMENT|DAO_LEX_SPACE );
	if( rc ){
		DaoString *str = DaoString_New(1);
		for(i=0; i<tokens->size; i++){
			DString_Assign( str->value, & tokens->items.pToken[i]->string );
			DArray_Append( list->value, (DaoValue*) str );
		}
		DaoString_Delete( str );
	}
	DaoLexer_Delete( lexer );
}
static void AUX_Log( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *info = p[0]->xString.value;
	FILE *fout = fopen( "dao.log", "a" );
	fprintf( fout, "%s\n", info->chars );
	fclose( fout );
}
static void AUX_Test( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoProcess_RaiseException( proc, "Exception::Error::SomeError", "just a test", NULL );
}

static DaoFuncItem auxMeths[]=
{
	{ AUX_Tokenize,    "tokenize( source :string )=>list<string>" },
	{ AUX_Log,         "log( info='' )" },
#ifdef DEBUG
	{ AUX_Test,        "__test__()" },
#endif
	{ NULL, NULL }
};

DAO_DLL int DaoAux_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	ns = DaoVmSpace_GetNamespace( vmSpace, "std" );
	DaoNamespace_WrapFunctions( ns, auxMeths );
	return 0;
}

