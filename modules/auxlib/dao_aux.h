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

#include"dao.h"

DAO_DLL void DaoArray_SetVectorI( DaoArray *self, daoint* vec, daoint N );
DAO_DLL void DaoArray_SetVectorF( DaoArray *self, float* vec, daoint N );
DAO_DLL void DaoArray_SetVectorD( DaoArray *self, double* vec, daoint N );
DAO_DLL void DaoArray_SetVectorSB( DaoArray *self, signed char* vec, daoint N );
DAO_DLL void DaoArray_SetVectorUB( DaoArray *self, unsigned char* vec, daoint N );
DAO_DLL void DaoArray_SetVectorSS( DaoArray *self, signed short* vec, daoint N );
DAO_DLL void DaoArray_SetVectorUS( DaoArray *self, unsigned short* vec, daoint N );
DAO_DLL void DaoArray_SetVectorSI( DaoArray *self, signed int* vec, daoint N );
DAO_DLL void DaoArray_SetVectorUI( DaoArray *self, unsigned int* vec, daoint N );
DAO_DLL void DaoArray_SetMatrixI( DaoArray *self, daoint **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixF( DaoArray *self, float **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixD( DaoArray *self, double **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixSB( DaoArray *self, signed char **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixUB( DaoArray *self, unsigned char **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixSS( DaoArray *self, signed short **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixUS( DaoArray *self, unsigned short **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixSI( DaoArray *self, signed int **mat, daoint row, daoint col );
DAO_DLL void DaoArray_SetMatrixUI( DaoArray *self, unsigned int **mat, daoint row, daoint col );


DAO_DLL DaoArray*  DaoProcess_PutVectorSB( DaoProcess *self, signed   char *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorUB( DaoProcess *self, unsigned char *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorSS( DaoProcess *self, signed   short *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorUS( DaoProcess *self, unsigned short *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorSI( DaoProcess *self, signed   int *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorUI( DaoProcess *self, unsigned int *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorI( DaoProcess *self, daoint *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorF( DaoProcess *self, float  *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorD( DaoProcess *self, double *array, daoint N );
DAO_DLL DaoArray*  DaoProcess_PutVectorC( DaoProcess *self, complex16 *array, daoint N );


/*
// DaoProcess_NewVectorSB() creates an integer vector from an array of signed byte;
// DaoProcess_NewVectorUB() creates an integer vector from an array of unsigned byte;
// DaoProcess_NewVectorSS() creates an integer vector from an array of signed short;
// DaoProcess_NewVectorUS() creates an integer vector from an array of unsigned short;
// DaoProcess_NewVectorSI() creates an integer vector from an array of signed int;
// DaoProcess_NewVectorUI() creates an integer vector from an array of unsigned int;
// DaoProcess_NewVectorI()  creates an integer vector from an array of daoint;
// DaoProcess_NewVectorF()  creates an float vector from an array of float;
// DaoProcess_NewVectorD()  creates an double vector from an array of double;
//
// If "n" is not zero, the created array will allocate a new buffer, and copy
// the data from the C array passed as parameter to the new buffer; otherwise,
// the created array will directly use the C array as buffer.
//
// In the case that the C array is directly used, one can call reshape() to set
// the array to proper shape before using. The C array must be ensured to be valid
// throughout the use of the created array; and its deallocation must be handled by
// the owner of the C array. A typical scenario of using array in this way is to call
// a Dao function from C, and pass a C array to the Dao function.
*/
DAO_DLL DaoArray* DaoProcess_NewVectorSB( DaoProcess *self, signed   char *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorUB( DaoProcess *self, unsigned char *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorSS( DaoProcess *self, signed   short *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorUS( DaoProcess *self, unsigned short *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorSI( DaoProcess *self, signed   int *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorUI( DaoProcess *self, unsigned int *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorI( DaoProcess *self, daoint *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorF( DaoProcess *self, float  *s, daoint n );
DAO_DLL DaoArray* DaoProcess_NewVectorD( DaoProcess *self, double *s, daoint n );

/*
// DaoProcess_NewMatrixSB() creates an integer matrix from a [n x m] matrix of signed byte;
// DaoProcess_NewMatrixUB() creates an integer matrix from a [n x m] matrix of unsigned byte;
// DaoProcess_NewMatrixSS() creates an integer matrix from a [n x m] matrix of signed short;
// DaoProcess_NewMatrixUS() creates an integer matrix from a [n x m] matrix of unsigned short;
// DaoProcess_NewMatrixSI() creates an integer matrix from a [n x m] matrix of signed int;
// DaoProcess_NewMatrixUI() creates an integer matrix from a [n x m] matrix of unsigned int;
// DaoProcess_NewMatrixI() creates an integer matrix from a [n x m] matrix of daoint;
// DaoProcess_NewMatrixF() creates an float matrix from a [n x m] matrix of float;
// DaoProcess_NewMatrixD() creates an double matrix from a [n x m] matrix of double;
*/
DAO_DLL DaoArray* DaoProcess_NewMatrixSB( DaoProcess *self, signed   char **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixUB( DaoProcess *self, unsigned char **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixSS( DaoProcess *self, signed   short **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixUS( DaoProcess *self, unsigned short **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixSI( DaoProcess *self, signed   int **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixUI( DaoProcess *self, unsigned int **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixI( DaoProcess *self, daoint **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixF( DaoProcess *self, float  **s, daoint n, daoint m );
DAO_DLL DaoArray* DaoProcess_NewMatrixD( DaoProcess *self, double **s, daoint n, daoint m );
