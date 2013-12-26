/*
// Dao Virtual Machine
// http://www.daovm.net
//
// Copyright (c) 2006-2013, Limin Fu
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
// THIS SOFTWARE IS PROVIDED  BY THE COPYRIGHT HOLDERS AND  CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED  WARRANTIES,  INCLUDING,  BUT NOT LIMITED TO,  THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL  THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE FOR ANY DIRECT,
// INDIRECT,  INCIDENTAL, SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL  DAMAGES (INCLUDING,
// BUT NOT LIMITED TO,  PROCUREMENT OF  SUBSTITUTE  GOODS OR  SERVICES;  LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY OF
// LIABILITY,  WHETHER IN CONTRACT,  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef DAO_BYTECODE_H
#define DAO_BYTECODE_H

#include "daoStdtype.h"


#define DAO_BC_SIGNATURE  "\33Dao\2\0\r\n"


/*
//##########################################################################
//
// Header:
//
// Byte       # ESC, 0x1B;
// Byte       # 0x44, namely 'D';
// Byte       # 0x61, namely 'a';
// Byte       # 0x6F, namely 'o';
// Byte       # major version number, 0x1;
// Byte       # minor version number, 0x2;
// Byte       # Carriage Return (CR), 0x0D;
// Byte       # Line Feed (LF), 0x0A;
// Byte       # format version, 0x0 for the official one;
// Byte       # size of integer type, default 0x4;
// Byte[20]   # 20 reserved bytes;
// Byte       # Carriage Return (CR), 0x0D;
// Byte       # Line Feed (LF), 0x0A;
// Byte[2]    # length of the source path;
// Byte[]     # source path (null-terminated);
// Byte       # Carriage Return (CR), 0x0D;
// Byte       # Line Feed (LF), 0x0A;
//
//##########################################################################
//
// Chunk structure: one byte for chunk type, eight bytes for data.
//
// Typical structure:
// A (1 byte), B (2 bytes), C (2 bytes), D (2 bytes), E (2 bytes)
// A (1 byte), B (2 bytes), C (2 bytes), D (4 bytes)
// A (1 byte), B (4 bytes), C (4 bytes)
//
//##########################################################################
//
// Specifications:
//
//################
// Values:
//########
//
// int:
// ASM_VALUE(1Byte): DAO_INTEGER(2Bytes), Zeros(6Bytes);
// ASM_END(1B): Value(4B/8B), Zeros(4B/0B);
//
//
// float:
// ASM_VALUE(1B): DAO_FLOAT(2B), Zeros(6B);
// ASM_END(1B): Value(4B), Zeros(4B);
//
//
// double:
// ASM_VALUE(1B): DAO_DOUBLE(2B), Zeros(6B);
// ASM_END(1B): Value(8B);
//
//
// complex:
// ASM_VALUE(1B): DAO_COMPLEX(2B), Zeros(6B);
// ASM_DATA(1B): Real(8B);
// ASM_END(1B): Imag(8B);
//
//
// long:
// ASM_VALUE(1B): DAO_LONG(2B), Zeros(2B), Size(4B);
// ASM_DATA(1B); Digits (8B);
// ASM_END(1B): Digits(8B);
//
//
// string:
// ASM_VALUE(1B): DAO_STRING(2B), MBS/WCS(2B), Size(4B);
// ASM_DATA(1B); Bytes (8B);
// ASM_END(1B): Bytes(8B);
//
//
// enum symbol:
// ASM_VALUE(1B): DAO_ENUM(2B), Name-Index(2B), Type-Index(2B), Zeros(2B);
// ASM_END(1B): Value(4B), Zeros(0);
//
//   Notes:
//   The "Name-Index" and "Type-Index" reference previous blocks which are
//   located backwardly by a such "index" offset. Such index is stored as
//   a two-byte short. In case short is not sufficient to represent such
//   index, an intermediate indexing chunk can be used:
//
//     ASM_SEEK(1B): New-Index(4B), Zeros(4B);
// 
//   When "New-Index" is also seeked backwardly, and is relative to the
//   seek chunk.
//
//
// array:
// ASM_VALUE(1B): DAO_ARRAY(2B), Numeric-Type(1B), Dimensions(1B), Size(4B);
// ASM_DATA(1B); Dim1(4B), Dim2(4B);
// ASM_DATA(1B); More dimensions;
// ASM_DATA(1B); Data(4B), Data(4B); Or Data(8B);
// ASM_DATA(1B); More Data;
// ASM_END(1B): Data(8B);
//
//
// list:
// ASM_VALUE(1B): DAO_LIST(2B), Type-Index(2B), Size(4B);
// ASM_DATA(1B); Value-Index(2B), Value-Index(2B), Value-Index(2B), Value-Index(2B);
// ASM_END(1B): Value-Index(2B), Value-Index(2B), Value-Index(2B), Value-Index(2B);
//
//
// map:
// ASM_VALUE(1B): DAO_MAP(2B), Type-Index(2B), Hash-Seed(4B);
// ASM_DATA(1B); Value-Index(2B), Value-Index(2B), Value-Index(2B), Value-Index(2B);
// ASM_END(1B): Value-Index(2B), Value-Index(2B), Value-Index(2B), Value-Index(2B);
//
// A pair of "Value-Index"s is for a pair of key-value, zero marks the end.
//
//
// tuple:
// ASM_VALUE(1B): DAO_TUPLE(2B), Type-Index(2B), Size(4B);
// ASM_DATA(1B); Value-Index(2B), Value-Index(2B), Value-Index(2B), Value-Index(2B);
// ASM_END(1B): Value-Index(2B), Value-Index(2B), Value-Index(2B), Value-Index(2B);
//
//
//
//
//#########
// Blocks:
//#########
//
// routine:
// ASM_ROUTINE(1B): Name-Index(2B), Type-Index(2B), Host-Index(2B), Attrib(2B);
// ...
// ASM_END: LineDef(2B), Zeros(6B);
//
//
// class:
// ASM_CLASS(1B): Name-Index(2B), Parent-Index(2B), Host-Index(2B), Attrib(2B);
// ...
// ASM_END(1B): LineDef(2B), Zeros(6B);
//
//
// enum:
// ASM_ENUM(1B): Name-Index(2B), Enum/Flag(2B), Count(4B);
// ASM_DATA(1B): Name-Index(2B), Value(4B), Zeros(2B);
// ASM_END(1B): Name-Index(2B), Value(4B), Zeros(2B);
//
//
// type:
// ASM_TYPE(1B): Name-Index(2B), TypeID(2B), Aux-Index(2B), CodeBlockType-Index(2B);
// ASM_DATA(1B): Type-Index(2B) X 4;
// ASM_END(1B): Type-Index(2B) X 4;
//
// Note 1: the nested types are zero Type-Index terminated;
// Note 2: "Aux-Index" could be index to returned type or class block etc;
// Note 3: if "TypeID" == 0, and "Aux-Index" reference to a type, create an alias;
// Note 4: if "TypeID" != 0, and "Aux-Index" reference to a string, import type;
//
//
// value:
// See above;
//
//
// evaluation:
// ASM_EVAL(1B): Opcode(2B), OperandCount(2B), Value-Index(2B), Value-Index(2B);
//   ASM_DATA(1B): Value-Index(2B), Value-Index(2B), Value-Index(2B), Value-Index(2B);
// ASM_END(1B): Value-Index(2B), Value-Index(2B), Value-Index(2B), Value-Index(2B);
//
//
// consts:
// ASM_CONSTS(1B): Count(2B), Value-Index(2B), Value-Index(2B), Value-Index(2B);
//   ASM_DATA(1B): Value-Index(2B), Value-Index(2B), Value-Index(2B), Value-Index(2B);
// ASM_END(1B): Value-Index(2B), Value-Index(2B), Value-Index(2B), Value-Index(2B);
//
//
// types:
// ASM_TYPES(1B): Count(2B), Zeros(2B), Var-Index(2B), Type-Index(2B);
//   ASM_DATA(1B): Var-Index(2B), Type-Index(2B), Var-Index(2B), Type-Index(2B);
// ASM_END(1B): Var-Index(2B), Type-Index(2B), Var-Index(2B), Type-Index(2B);
//
//
// code:
// ASM_CODE(1B): Zeros(2B), Line-Num-Count(2B), LineNum(2B), Count(2B);
// ASM_DATA(1B): LineNum(2B), Count(2B), LineNum(2B), Count(2B);
// ASM_DATA(1B): Opcode(2B), A(2B), B(2B), C(2B);
// ASM_END(1B): Opcode(2B), A(2B), B(2B), C(2B);
//
//
//###########
// Statement:
//###########
//
// load statement:
// ASM_LOAD(1B): File-Path-Index(2B), Optional-Name-Index(2B), Zeros(4B);
//
// use namespace:
// ASM_USE(1B): DAO_NAMESPACE(2B), Value-Index(2B), Zeros(4B);
//
// use enum constants:
// ASM_USE(1B): DAO_ENUM(2B), Type-Index(2B), Zeros(4B);
//
// use constructors:
// ASM_USE(1B): DAO_ROUTINE(2B), Name-Index(2B), Type-Index(2B), Zeros(2B);
//
// var declaration:
// ASM_VAR(1B): Name-Index(2B), Value-Index(2B), Type-Index(2B), Zeros(2B);
//
// const declaration:
// ASM_CONST(1B): Name-Index(2B), Value-Index(2B), Zeros(4B);
//
// static declaration:
// ASM_STATIC(1B): Name-Index(2B), Value-Index(2B), Type-Index(2B), Zeros(2B);
//
// global declaration:
// ASM_GLOBAL(1B): Name-Index(2B), Value-Index(2B), Type-Index(2B), Zeros(2B);
//
// mixin:
// ASM_MIXIN(1B): Value-Index(2B), Value-Index(2B), Value-Index(2B), Value-Index(2B);
//
// decorator target:
// ASM_DECO(1B): Prefix-Index(2B), ~(2B), 0, 0;
// ASM_DECO(1B): ~(2B), Suffix-Index(2B), 0, 0;
// ASM_DECO(1B): Prefix-Index(2B), ~, Suffix-Index(2B), 0;
// ASM_DECO(1B): ~(2B), 0, 0, 0;
// 
// seek:
// ASM_SEEK(1B): New-Index(2B), Zeros(6B);
//
//##########################################################################
//
// Note:
//
// 1. Constant folding:
//    If a constant folding involves NO function call and NONE user defined
//    types, encode the value directly; Otherwise, encode the expressions
//    the produce the value;
//
//    Because a function call may change the environment, the only way
//    to ensure reproducing an identical (equivalent) one is to make the
//    calls; And the only way to obtain a meaningful object of a user defined
//    type is to evaluate the leaves of the expression, which may make
//    it easier or possible to get the object.
//
// 2. Global constant and variables (and class constants and statics):
//    Instructions that access these data should be encoded with the
//    constant/variable's lookup names; If no lookup name is available,
//    automatically generate a unique one;
//
//##########################################################################
//
// load web.cgi
//
// enum Bool{ False, True }
// use enum Bool
//
// static abc = random_string( 100 )
// global index = 123 + %abc
// global address : tuple<number:int,string,Bool> = ( 123, 'Main St.', $False )
// 
// class Klass
// {
//   const name = "abc"; 
//   var index = 123;
//   static state = 456;
//   routine Method( a:int ){
//   }
// }
//
// const kname = Klass::name
// global klass = Klass()
// 
// routine Func()
// {
//     name = index
// }
//
//##########################################################################
//
// 32-Bytes Header;
//
// ASM_ROUTINE: 0, 0, 0;
//
//   ASM_VALUE: DAO_STRING, 0, 7;
//   ASM_END: "web/cgi";
//
//   ASM_LOAD: 1 ("web/cgi"), 0;
//
//   ASM_VALUE: DAO_STRING, 0, 4;
//   ASM_END: "Bool";
//
//   ASM_VALUE: DAO_STRING, 0, 5;
//   ASM_END: "False";
//
//   ASM_VALUE: DAO_STRING, 0, 4;
//   ASM_END: "True";
//
//   ASM_ENUM: 3 ("Bool"), 0, 2;
//     ASM_DATA: 2 ("False"), 0 (int32);
//   ASM_END: 1 ("True"), 1 (int32);
//
//   ASM_USE: DAO_ENUM, 1 ("Bool"), 0;
//
//   ASM_VALUE: DAO_STRING, 0, 13;
//     ASM_DATA: "random_s";
//   ASM_END: "tring";
//
//   ASM_VALUE: DAO_INTEGER, 0, 13;
//   ASM_END: 10, 0;
//
//   ASM_EVAL: CALL, 2, 2, 1;
//   ASM_END: 0;
//
//   ASM_VALUE: DAO_STRING, 0, 3;
//   ASM_END: "abc";
//
//   ASM_STATIC: 1 ("abc"), 2, 0;
//
//   ASM_TYPE: 0, DAO_INTEGER, 0, 0;
//   ASM_END: 0;
//
//   ASM_VALUE: DAO_STRING, 0, 6;
//   ASM_END: "number";
//
//   ASM_TYPE: 0, DAO_PAR_NAMED, 1 ("number"), 0;
//   ASM_END: 2 (int), 0;
//
//   ASM_TYPE: 0, DAO_STRING, 0, 0;
//   ASM_END: 0;
//
//   ASM_VALUE: DAO_STRING, 0, 30;
//     ASM_DATA: "tuple<nu";
//     ASM_DATA: "mber:int";
//     ASM_DATA: ",string,";
//   ASM_VALUE: ",Bool>";
//
//   ASM_TYPE: 1, DAO_TUPLE, 0, 0;
//   ASM_END: 3, 2, 10, 0;
//
//   ASM_VALUE: DAO_INTEGER, 0;
//   ASM_END: 123, 0;
//
//   ASM_VALUE: DAO_STRING, 0, 8;
//   ASM_END: "Main St.";
//
//   ASM_VALUE: DAO_ENUM, 15 ("False"), 0;
//   ASM_VALUE: 0;
//
//   ASM_VALUE: DAO_TUPLE, 4, 3;
//   ASM_END: 3, 2, 1, 0;
//
//   ASM_VALUE: DAO_STRING, 0, 7;
//   ASM_END: "address";
//
//   ASM_GLOBAL: 1 ("address"), 2, 4;
//
//
//   ASM_CLASS: "Klass", 0;
//     ASM_CONST: "name", 9 (block), 0;
//     ASM_VAR: "index", 
//   ASM_END: 0;
//
//   ASM_CONSTS: # local constants;
//   ASM_END: 0;
//
//   ASM_TYPES: # explicit types for local variables;
//   ASM_END: 0;
//
//   ASM_CODE:
//   ASM_END: 0;
//
// ASM_END: 0, 0, 0;
//
//##########################################################################
*/
enum DaoAuxOpcode
{
	DAO_ASM_NONE      ,
	DAO_ASM_ROUTINE   ,
	DAO_ASM_CLASS     ,
	DAO_ASM_INTERFACE ,
	DAO_ASM_ENUM      ,
	DAO_ASM_TYPE      ,
	DAO_ASM_VALUE     ,
	DAO_ASM_EVAL      ,
	DAO_ASM_CONSTS    ,
	DAO_ASM_TYPES     ,
	DAO_ASM_CODE      ,
	DAO_ASM_END       ,
	DAO_ASM_LOAD      ,
	DAO_ASM_USE       ,
	DAO_ASM_CONST     ,
	DAO_ASM_STATIC    ,
	DAO_ASM_GLOBAL    ,
	DAO_ASM_VAR       ,
	DAO_ASM_MIXIN     ,
	DAO_ASM_DECOPAT   ,
	DAO_ASM_DATA      ,
	DAO_ASM_SEEK
};



typedef struct DaoMainCoder   DaoMainCoder;
typedef struct DaoBlockCoder  DaoBlockCoder;


struct DaoBlockCoder
{
	uint_t   type  : 8;
	uint_t   index : 24;

	uchar_t  begin[8];
	uchar_t  end[8];

	DMap  *blocks;

	DaoValue  *value;

	DaoMainCoder   *main;

	/* Children blocks: */
	DaoBlockCoder  *first;
	DaoBlockCoder  *last;

	/* Sibling blocks: */
	DaoBlockCoder  *prev;
	DaoBlockCoder  *next;
};

struct DaoMainCoder
{
	DaoVmSpace  *vmspace;

	uint_t   index;
	uchar_t  intSize;
	uchar_t  error;

	DaoBlockCoder  *top;

	DMap  *blocks; /* hash<DaoValue*,DaoBlockCoder*> */

	DArray  *stack;  /* list<DaoBlockCoder*> */
	DArray  *caches; /* list<DaoBlockCoder*> */
	DArray  *lines;
};

DaoBlockCoder* DaoBlockCoder_New( DaoMainCoder *main );
void DaoBlockCoder_Delete( DaoBlockCoder *self );

DaoMainCoder* DaoMainCoder_New( DaoVmSpace *vms );
void DaoMainCoder_Delete( DaoMainCoder *self );

DaoBlockCoder* DaoMainCoder_Init( DaoMainCoder *self );
DaoBlockCoder* DaoMainCoder_NewBlock( DaoMainCoder *self, int type );

DaoBlockCoder* DaoBlockCoder_NewBlock( DaoBlockCoder *self, int type );
DaoBlockCoder* DaoBlockCoder_FindBlock( DaoBlockCoder *self, DaoValue *value );
DaoBlockCoder* DaoBlockCoder_AddBlock( DaoBlockCoder *self, DaoValue *value, int type );
DaoBlockCoder* DaoBlockCoder_AddRoutineBlock( DaoBlockCoder *self, DaoRoutine *routine );
DaoBlockCoder* DaoBlockCoder_AddEvalBlock( DaoBlockCoder *self, DaoValue *value, int code, int operands );

void DaoBlockCoder_InsertBlockIndex( DaoBlockCoder *self, uchar_t *code, DaoBlockCoder *block );

DaoBlockCoder* DaoBlockCoder_EncodeString( DaoBlockCoder *self, DString *string );
DaoBlockCoder* DaoBlockCoder_EncodeType( DaoBlockCoder *self, DaoType *type );
DaoBlockCoder* DaoBlockCoder_EncodeValue( DaoBlockCoder *self, DaoValue *value );
DaoBlockCoder* DaoBlockCoder_EncodeLoadStmt( DaoBlockCoder *self, DString *mod, DString *ns );
DaoBlockCoder* DaoBlockCoder_EncodeSeekStmt( DaoBlockCoder *self, DaoBlockCoder *target );

DaoBlockCoder* DaoBlockCoder_EncodeInteger( DaoBlockCoder *self, daoint value );
DaoBlockCoder* DaoBlockCoder_EncodeFloat( DaoBlockCoder *self, float value );
DaoBlockCoder* DaoBlockCoder_EncodeDouble( DaoBlockCoder *self, double value );
DaoBlockCoder* DaoBlockCoder_EncodeComplex( DaoBlockCoder *self, DaoComplex *value );
DaoBlockCoder* DaoBlockCoder_EncodeLong( DaoBlockCoder *self, DLong *value );
DaoBlockCoder* DaoBlockCoder_EncodeEnum( DaoBlockCoder *self, DaoEnum *value );

DaoBlockCoder* DaoBlockCoder_EncodeArray( DaoBlockCoder *self, DaoArray *value );
DaoBlockCoder* DaoBlockCoder_EncodeList( DaoBlockCoder *self, DaoList *value );

DaoBlockCoder* DaoBlockCoder_EncodeValue( DaoBlockCoder *self, DaoValue *value );

void DaoBlockCoder_EncodeValues( DaoBlockCoder *self, DaoValue **values, int count );
int DaoBlockCoder_EncodeValues2( DaoBlockCoder *self, DArray *values );
void DaoBlockCoder_AddBlockIndexData( DaoBlockCoder *self, int head, int size );

void DaoMainCoder_EncodeHeader( DaoMainCoder *self, const char *fname, DString *output );
void DaoMainCoder_EncodeToString( DaoMainCoder *self, DString *output );
void DaoMainCoder_Disassemble( DaoMainCoder *self );


typedef struct DaoByteEncoder  DaoByteEncoder;
typedef struct DaoByteDecoder  DaoByteDecoder;

struct DaoByteEncoder
{
	DaoNamespace  *nspace;

	daoint    valueCount;
	daoint    constCount;
	daoint    varCount;

	DString  *header;
	DString  *source;
	DString  *modules;
	DString  *identifiers;
	DString  *declarations;
	DString  *types;
	DString  *values;
	DString  *constants;
	DString  *variables;
	DString  *glbtypes;
	DString  *interfaces;
	DString  *classes;
	DString  *routines;

	DString  *tmpBytes;
	DString  *valueBytes;
	DArray   *lookups;      /* <daoint> */
	DArray   *names;        /* <DString*> (not managed); */

	DArray   *objects;      /* <DaoValue*> */
	DArray   *lines;

	DArray   *hosts;
	DMap     *handled;

	DMap  *mapLookupHost;   /* <DaoValue*,DaoValue*> */
	DMap  *mapLookupName;   /* <DaoValue*,DString*> */

	DMap  *mapIdentifiers;  /* <DString*,daoint> */
	DMap  *mapDeclarations; /* <DaoValue*,daoint> */
	DMap  *mapTypes;        /* <DaoType*,daoint> */
	DMap  *mapValues;       /* <DaoValue*,daoint> */
	DMap  *mapValueBytes;   /* <DString*,daoint> */
	DMap  *mapInterfaces;   /* <DaoInterface*,daoint> */
	DMap  *mapClasses;      /* <DaoClass*,daoint> */
	DMap  *mapRoutines;     /* <DaoRoutine*,daoint> */
};

DaoByteEncoder* DaoByteEncoder_New();
void DaoByteEncoder_Delete( DaoByteEncoder *self );

void DaoByteEncoder_Encode( DaoByteEncoder *self, DaoNamespace *nspace, DString *output );



struct DaoByteDecoder
{
	DaoVmSpace    *vmspace;
	DaoNamespace  *nspace;

	DArray   *identifiers;  /* <DString*> */
	DArray   *namespaces;   /* <DaoNamespace*> */
	DArray   *declarations; /* <DaoValue*> */
	DArray   *types;        /* <DaoType*> */
	DArray   *values;       /* <DString*>: encoded values; */
	DArray   *interfaces;   /* <DaoInterface*> */
	DArray   *classes;      /* <DaoClass*> */
	DArray   *routines;     /* <DaoRoutine*> */

	DArray   *array;
	DString  *string;
	DMap     *valueTypes;
	DMap     *map;

	int  intSize;

	uchar_t  *codes;
	uchar_t  *end;
	uchar_t  *error;
};


DaoByteDecoder* DaoByteDecoder_New( DaoVmSpace *vmspace );
void DaoByteDecoder_Delete( DaoByteDecoder *self );

int DaoByteDecoder_Decode( DaoByteDecoder *self, DString *input, DaoNamespace *nspace );


#endif
