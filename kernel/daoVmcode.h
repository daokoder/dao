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

#ifndef DAO_VMCODE_H
#define DAO_VMCODE_H

#include"daoBase.h"

enum DaoOpcode
{
	DVM_NOP = 0, /* no operation, the VM assumes maximum one NOP between two effective codes; */
	DVM_DATA ,  /* create primitive data: A: type<=DAO_COMPLEX, B: value, C: register; */
	DVM_GETCL , /* get local const: C = A::B; */
	DVM_GETCK , /* get class const: C = A::B; current class, A=0; parent class: A>=1; */
	DVM_GETCG , /* get global const: C = A::B; current namespace, A=0; loaded: A>=1; */
	DVM_GETVH , /* get host variable from code section: C = A::B; A, outer level; */
	DVM_GETVS , /* get static variable or captured up variable from closure; */
	DVM_GETVO , /* get instance object variables: C = A::B; A=0; */
	DVM_GETVK , /* get class global variables: C = A::B; A: the same as GETCK; */
	DVM_GETVG , /* get global variables: C = A::B; A: the same as GETCG; */
	DVM_GETI ,  /* GET Item(s) : C = A[B]; */
	DVM_GETDI , /* GET Item(s) : C = A[B], B is the (direct) index; */
	DVM_GETMI , /* GET Item(s) : C = A[A+1, ..., A+B]; */
	DVM_GETF ,  /* GET Field : C = A.B */
	DVM_SETVH , /* set host variable in code section: C::B = A; C, outer level; */
	DVM_SETVS , /* set static variable or captured up variable from closure; */
	DVM_SETVO , /* set object variables: C::B = A, C the same as A in DVM_GETVO */
	DVM_SETVK , /* set class variables: C::B = A, C the same as A in DVM_GETVK */
	DVM_SETVG , /* set global variables: C::B = A, C the same as A in DVM_GETVG */
	DVM_SETI ,  /* SET Item(s) : C[B] = A;  */
	DVM_SETDI , /* SET Item(s) : C[B] = A, B is the (direct) index; */
	DVM_SETMI , /* SET Item(s) : C[C+1, ..., C+B] = A;  */
	DVM_SETF ,  /* SET Field : C.B = A */
	DVM_LOAD , /* put local value A as reference at C; */
	DVM_CAST , /* convert A to C if they have different types; */
	DVM_MOVE , /* C = A; if B==0, XXX it is compile from assignment, for typing system only */
	DVM_NOT ,  /* C = ! A; not */
	DVM_MINUS , /* C = - A; unary minus; */
	DVM_TILDE , /* C = ~ A */
	DVM_SIZE , /* C = % A; size operation; */
	DVM_ADD ,  /* C = A + B;  */
	DVM_SUB ,  /* C = A - B;  */
	DVM_MUL ,  /* C = A * B;  */
	DVM_DIV ,  /* C = A / B;  */
	DVM_MOD ,  /* C = A % B;  */
	DVM_POW ,  /* C = A ** B; */
	DVM_AND ,  /* C = A && B; */
	DVM_OR ,   /* C = A || B; */
	DVM_LT ,   /* C = A <  B; */
	DVM_LE ,   /* C = A <= B; */
	DVM_EQ ,   /* C = A == B; */
	DVM_NE ,   /* C = A != B; */
	DVM_IN ,   /* C = A in B; */
	DVM_BITAND , /* C = A & B */
	DVM_BITOR ,  /* C = A | B */
	DVM_BITXOR , /* C = A ^ B */
	DVM_BITLFT , /* C = A << B */
	DVM_BITRIT , /* C = A >> B */
	DVM_CHECK ,  /* check type: C = A ?= B; C = A ?< B, where A is data, B is data or type */
	DVM_NAMEVA , /* C = A => B: name A, local constant, value B, local register */
	DVM_PAIR ,   /* C = A : B; create a pair of index, as a tuple; */
	DVM_TUPLE ,  /* tuple: C = ( A, A+1, ..., A+B-1 ); B>=2, items can be: name=>value */
	DVM_LIST ,   /* list: C = { A, A+1, ..., A+B-1 }; */
	DVM_MAP ,    /* map:  C = { A => A+1, ..., A+B-2 => A+B-1 }; if B==0, empty; */
	DVM_HASH ,   /* hash: C = { A -> A+1, ..., A+B-2 -> A+B-1 }; if B==0, empty; */
	DVM_VECTOR , /* vector: C = [ A, A+1, ..., A+B-1 ]; */
	DVM_MATRIX , /* matrix: C=[A,..,A+c-1;..;A+c*(r-1),..,A+c*r-1]; B=rc;r,c:8-bits each.*/
	DVM_APLIST , /* arithmetic progression list: C = { A : ... : A+B-1 }, B = 2 or 3; */
	DVM_APVECTOR , /* arithmetic progression vector: C = [ A : ... : A+B-1 ], B = 2 or 3; */
	DVM_CURRY ,    /* class_or_routine_name: A{ A+1, ..., A+B } */
	DVM_MCURRY ,   /* object.method: A{ A+1, ..., A+B } */
	DVM_ROUTINE ,  /* create a function, possibly with closure */
	DVM_GOTO ,   /* go to B; */
	DVM_SWITCH , /* A: variable, B: location of default codes, C: number of cases */
	DVM_CASE ,   /* A: constant of the case, B: location of the case codes, C: case mode */
	DVM_ITER ,   /* create an iterator at C for A if B==0, else test an array of iterators; */
	DVM_TEST ,   /* if A, go to the next one; else, goto B-th instruction; */
	DVM_MATH ,   /* C = A( B ); A: sin,cos,...; B: double,complex */
	DVM_CALL ,   /* function call: C = A( A+1, A+2, ..., A+B ); If B==0, no parameters; */
	DVM_MCALL ,  /* method call: x.y(...), pass x as the first parameter; */
	DVM_RETURN , /* return A,A+1,..,A+B-1; B==0: no returns; C==1: return from functional; */
	DVM_YIELD , /* yield A, A+1,.., A+B-1; return data at C when resumed; */
	DVM_EVAL ,  /* evaluate a code section: C=@@(A){}, if B==1, otherwise, C=@@{}; */
	DVM_SECT ,  /* code section label, parameters: A,A+1,...,A+B-1; C #explicit parameters; */
	DVM_JITC ,  /* run Just-In-Time compiled Code A, and skip the next B instructions; */
	DVM_DEBUG , /* prompt to debugging mode; */

	/* optimized opcodes: */
	DVM_DATA_I ,
	DVM_DATA_F ,
	DVM_DATA_D ,
	DVM_DATA_C ,

	DVM_GETCL_I ,
	DVM_GETCL_F ,
	DVM_GETCL_D ,
	DVM_GETCL_C ,
	DVM_GETCK_I ,
	DVM_GETCK_F ,
	DVM_GETCK_D ,
	DVM_GETCK_C ,
	DVM_GETCG_I ,
	DVM_GETCG_F ,
	DVM_GETCG_D ,
	DVM_GETCG_C ,

	DVM_GETVH_I ,
	DVM_GETVH_F ,
	DVM_GETVH_D ,
	DVM_GETVH_C ,
	DVM_GETVS_I ,
	DVM_GETVS_F ,
	DVM_GETVS_D ,
	DVM_GETVS_C ,
	DVM_GETVO_I ,
	DVM_GETVO_F ,
	DVM_GETVO_D ,
	DVM_GETVO_C ,
	DVM_GETVK_I ,
	DVM_GETVK_F ,
	DVM_GETVK_D ,
	DVM_GETVK_C ,
	DVM_GETVG_I ,
	DVM_GETVG_F ,
	DVM_GETVG_D ,
	DVM_GETVG_C ,

	DVM_SETVH_II ,
	DVM_SETVH_FF ,
	DVM_SETVH_DD ,
	DVM_SETVH_CC ,
	DVM_SETVS_II ,
	DVM_SETVS_FF ,
	DVM_SETVS_DD ,
	DVM_SETVS_CC ,
	DVM_SETVO_II ,
	DVM_SETVO_FF ,
	DVM_SETVO_DD ,
	DVM_SETVO_CC ,
	DVM_SETVK_II ,
	DVM_SETVK_FF ,
	DVM_SETVK_DD ,
	DVM_SETVK_CC ,
	DVM_SETVG_II ,
	DVM_SETVG_FF ,
	DVM_SETVG_DD ,
	DVM_SETVG_CC ,

	DVM_MOVE_II , /* integer = integer */
	DVM_MOVE_IF , /* integer = float */
	DVM_MOVE_ID , /* integer = double */
	DVM_MOVE_FI ,
	DVM_MOVE_FF ,
	DVM_MOVE_FD ,
	DVM_MOVE_DI ,
	DVM_MOVE_DF ,
	DVM_MOVE_DD ,
	DVM_MOVE_CI ,
	DVM_MOVE_CF ,
	DVM_MOVE_CD ,
	DVM_MOVE_CC , /* complex = complex */
	DVM_MOVE_SS , /* string = string */
	DVM_MOVE_PP , /* C = A; C and A are of the same non-primitive type, A must not be constant; */
	DVM_MOVE_XX , /* C = A; C and A are of the same type, or C is of any type; */
	DVM_NOT_I ,
	DVM_NOT_F ,
	DVM_NOT_D ,
	DVM_MINUS_I ,
	DVM_MINUS_F ,
	DVM_MINUS_D ,
	DVM_MINUS_C ,
	DVM_TILDE_I ,
	DVM_TILDE_C ,
	/* C = A + B: will be compiled into: ADD, MOVE,
	 * and the C operand of ADD is always an intermediate data with type to be inferred,
	 * so it is only necessary to add specialized opcode according the A,B operands. */
	/* integer: */
	DVM_ADD_III ,
	DVM_SUB_III ,
	DVM_MUL_III ,
	DVM_DIV_III ,
	DVM_MOD_III ,
	DVM_POW_III ,
	DVM_AND_III ,
	DVM_OR_III ,
	DVM_LT_III ,
	DVM_LE_III ,
	DVM_EQ_III ,
	DVM_NE_III ,
	DVM_BITAND_III ,
	DVM_BITOR_III ,
	DVM_BITXOR_III ,
	DVM_BITLFT_III ,
	DVM_BITRIT_III ,
	/* float: */
	DVM_ADD_FFF ,
	DVM_SUB_FFF ,
	DVM_MUL_FFF ,
	DVM_DIV_FFF ,
	DVM_MOD_FFF ,
	DVM_POW_FFF ,
	DVM_AND_FFF ,
	DVM_OR_FFF ,
	DVM_LT_IFF ,
	DVM_LE_IFF ,
	DVM_EQ_IFF ,
	DVM_NE_IFF ,
	/* double: */
	DVM_ADD_DDD ,
	DVM_SUB_DDD ,
	DVM_MUL_DDD ,
	DVM_DIV_DDD ,
	DVM_MOD_DDD ,
	DVM_POW_DDD ,
	DVM_AND_DDD ,
	DVM_OR_DDD ,
	DVM_LT_IDD ,
	DVM_LE_IDD ,
	DVM_EQ_IDD ,
	DVM_NE_IDD ,

	DVM_ADD_CCC ,
	DVM_SUB_CCC ,
	DVM_MUL_CCC ,
	DVM_DIV_CCC ,

	DVM_EQ_ICC ,
	DVM_NE_ICC ,

	/* string */
	DVM_ADD_SSS ,
	DVM_LT_ISS ,
	DVM_LE_ISS ,
	DVM_EQ_ISS ,
	DVM_NE_ISS ,

	/* single indexing C=A[B]: GETI and MOVE */
	/* index should be integer, may be casted from float/double by the typing system */
	DVM_GETI_LI , /* get item : C = A[B]; X=list<X>[int] */
	DVM_SETI_LI , /* set item : C[B] = A; list<X>[int]=X, or list<any>[int]=X; */
	DVM_GETI_SI , /* get char from a string: string[int] */
	DVM_SETI_SII , /* set char to a string: string[int]=int */
	DVM_GETI_LII , /* get item : C = A[B]; list<int>[int] */
	DVM_GETI_LFI , /* get item : C = A[B]; list<float>[int] */
	DVM_GETI_LDI , /* get item : C = A[B]; list<double>[int] */
	DVM_GETI_LCI , /* get item : C = A[B]; list<complex>[int] */
	DVM_GETI_LSI , /* get item : C = A[B]; list<string>[int] */
	DVM_SETI_LIII , /* set item : C[B] = A; list<int>[int]=int */
	DVM_SETI_LFIF , /* set item : C[B] = A;  */
	DVM_SETI_LDID , /* set item : C[B] = A;  */
	DVM_SETI_LCIC , /* set item : C[B] = A;  */
	DVM_SETI_LSIS , /* set item : C[B] = A;  */
	DVM_GETI_AII , /* get item : C = A[B]; array<int>[int] */
	DVM_GETI_AFI , /* get item : C = A[B]; array<float>[int] */
	DVM_GETI_ADI , /* get item : C = A[B]; array<double>[int] */
	DVM_GETI_ACI , /* get item : C = A[B]; array<complex>[int] */
	DVM_SETI_AIII , /* set item : C[B] = A;  */
	DVM_SETI_AFIF , /* set item : C[B] = A;  */
	DVM_SETI_ADID , /* set item : C[B] = A;  */
	DVM_SETI_ACIC , /* set item : C[B] = A;  */

	DVM_GETI_TI , /* get item : C = A[B]; tuple<...>[int] */
	DVM_SETI_TI , /* set item : C[B] = A; tuple<...>[int]=X; */

	/* access field by constant index; specialized from GETI[const] or GETF */
	DVM_GETF_TI , /* get integer field by constant index; */
	DVM_GETF_TF , /* get float field by constant index; */
	DVM_GETF_TD , /* get double field by constant index; */
	DVM_GETF_TC , /* get complex field by constant index; */
	DVM_GETF_TX , /* get type checked field by constant index; */
	DVM_SETF_TII , /* set integer field to integer. */
	DVM_SETF_TFF , /* set float field to float. */
	DVM_SETF_TDD , /* set double field to double. */
	DVM_SETF_TCC , /* set complex field to double. */
	DVM_SETF_TSS , /* set string field to string. */
	DVM_SETF_TPP , /* set item: C[B]=A or C.B=A; tuple<..X..>[int]=X, or tuple<..any..>[int]=X; */
	DVM_SETF_TXX , /* set item: C[B]=A or C.B=A; tuple<..X..>[int]=X, or tuple<..any..>[int]=X; */

	/* multiple indexing a[i,j] */
	DVM_GETMI_AII , /* array: get item(s) : C = A[B]; B,C: integer, A integer array; */
	DVM_GETMI_AFI , /* array: get item(s) : C = A[B]; B,C: integer, A float array; */
	DVM_GETMI_ADI , /* array: get item(s) : C = A[B]; B,C: integer, A double array; */
	DVM_GETMI_ACI , /* array: get item(s) : C = A[B]; B,C: integer, A complex array; */
	DVM_SETMI_AIII , /* set item(s) : C[B] = A; A,B: integer, C complex array; */
	DVM_SETMI_AFIF , /* set item(s) : C[B] = A; A,B: integer, C complex array; */
	DVM_SETMI_ADID , /* set item(s) : C[B] = A; A,B: integer, C complex array; */
	DVM_SETMI_ACIC , /* set item(s) : C[B] = A; A,B: integer, C complex array; */

	DVM_GETF_CX , /* get complex field: real/imag; */
	DVM_SETF_CX , /* set complex field: real/imag; */

	/* setters and getters */
	/* get/set member of class instance by index instead of name: */
	DVM_GETF_KC , /* get class field, const; code: GET Member Field Const*/
	DVM_GETF_KG , /* get class field, global */
	DVM_GETF_OC , /* get class instance field, const; code: GET Member Field Const*/
	DVM_GETF_OG , /* get class instance field, global */
	DVM_GETF_OV , /* get class instance field, variable */
	DVM_SETF_KG , /* set class static field: field type equals to opa type, or is "any" type; */
	DVM_SETF_OG , /* set class static field: field type equals to opa type, or is "any" type; */
	DVM_SETF_OV , /* set class instance field: field type equals to opa type, or is "any" type; */

	/* C=A.B : GETF and MOVE */
	DVM_GETF_KCI , /* GET Member Field Const Integer */
	DVM_GETF_KCF , /* GET Member Field Const Float */
	DVM_GETF_KCD , /* GET Member Field Const Double*/
	DVM_GETF_KCC , /* GET Member Field Const Complex*/
	DVM_GETF_KGI ,
	DVM_GETF_KGF ,
	DVM_GETF_KGD ,
	DVM_GETF_KGC ,
	DVM_GETF_OCI , /* GET Member Field Const Integer */
	DVM_GETF_OCF , /* GET Member Field Const Float */
	DVM_GETF_OCD , /* GET Member Field Const Double*/
	DVM_GETF_OCC , /* GET Member Field Const Complex*/
	DVM_GETF_OGI ,
	DVM_GETF_OGF ,
	DVM_GETF_OGD ,
	DVM_GETF_OGC ,
	DVM_GETF_OVI ,
	DVM_GETF_OVF ,
	DVM_GETF_OVD ,
	DVM_GETF_OVC ,
	/* C.B=A specialize according to both: C.B and A */
	DVM_SETF_KGII ,
	DVM_SETF_KGFF ,
	DVM_SETF_KGDD ,
	DVM_SETF_KGCC ,
	DVM_SETF_OGII ,
	DVM_SETF_OGFF ,
	DVM_SETF_OGDD ,
	DVM_SETF_OGCC ,
	DVM_SETF_OVII ,
	DVM_SETF_OVFF ,
	DVM_SETF_OVDD ,
	DVM_SETF_OVCC ,

	DVM_TEST_I ,
	DVM_TEST_F ,
	DVM_TEST_D ,

	DVM_MATH_I ,
	DVM_MATH_F ,
	DVM_MATH_D ,

	DVM_CHECK_ST , /* check against simple types: int, float, double, complex, long, string; */

	/* increase a count, and perform the normal goto operation
	 * if the count does not exceed a safe bound. */
	DVM_SAFE_GOTO,

	DVM_NULL
};
typedef enum DaoOpcode DaoOpcode;

/*
// Additional nodes for the virtual machine instructions:
//
// DVM_GETVH and DVM_SETVH:
//
// These two instructions are for accessing up-values from closures or from code sections.
// The value of operand ::a (DVM_GETVH) or ::c (DVM_SETVH) instructs what kind of up-value
// is accessed; and the value of operand ::b tells which up-value is accessed.
//
// Zero value of the operand ::a or ::c indicates the up-value is accessed from a closure.
// The up-value is a local variable of the routine in which the closure was created.
//
// A non-zero value of ::a or ::c indicates the up-value
// TODO
//
// DVM_ROUTINE
// TODO
*/

/* Extra vmcode type for compiling:
 * They are used to setup proper indexing, branching or jumping. After this,
 * they will be removed or replaced with proper instructions from DaoOpcode.
 */
enum DaoOpcodeExtra
{
	DVM_LABEL = DVM_NULL + 1,
	DVM_LOAD2 ,
	DVM_LOOP ,
	DVM_BRANCH ,
	DVM_DO ,
	DVM_LBRA ,
	DVM_RBRA ,
	DVM_DEFAULT ,
	DVM_UNUSED
};

enum DaoMathFunct
{
	DVM_MATH_RAND ,
	DVM_MATH_CEIL ,
	DVM_MATH_FLOOR ,
	DVM_MATH_ABS ,
	DVM_MATH_ARG ,
	DVM_MATH_IMAG ,
	DVM_MATH_NORM ,
	DVM_MATH_REAL ,
	DVM_MATH_ACOS ,
	DVM_MATH_ASIN ,
	DVM_MATH_ATAN ,
	DVM_MATH_COS ,
	DVM_MATH_COSH ,
	DVM_MATH_EXP ,
	DVM_MATH_LOG ,
	DVM_MATH_SIN ,
	DVM_MATH_SINH ,
	DVM_MATH_SQRT ,
	DVM_MATH_TAN ,
	DVM_MATH_TANH
};
enum DaoFunctMeth
{
	DVM_FUNCT_APPLY ,
	DVM_FUNCT_SORT ,
	DVM_FUNCT_MAP ,
	DVM_FUNCT_FOLD ,
	DVM_FUNCT_UNFOLD ,
	DVM_FUNCT_FIND ,
	DVM_FUNCT_SELECT ,
	DVM_FUNCT_INDEX ,
	DVM_FUNCT_COUNT ,
	DVM_FUNCT_ITERATE ,
	DVM_FUNCT_STRING ,
	DVM_FUNCT_ARRAY ,
	DVM_FUNCT_LIST ,
	DVM_FUNCT_KEYS ,
	DVM_FUNCT_VALUES ,
	DVM_FUNCT_NULL
};

enum DaoCodeType
{
	DAO_CODE_NOP ,      /*  Local variable operands: None; */
	DAO_CODE_GETC ,     /*  C; local variable operand; */
	DAO_CODE_GETG ,     /*  C;     */
	DAO_CODE_GETU ,     /*  B,C;   */
	DAO_CODE_GETF ,     /*  A,C;   */
	DAO_CODE_GETI ,     /*  A,B,C; */
	DAO_CODE_GETM ,     /*  C,A,A+1,...,A+B; */
	DAO_CODE_SETG ,     /*  A;   */
	DAO_CODE_SETU ,     /*  A,B;   */
	DAO_CODE_SETF ,     /*  A,C;   */
	DAO_CODE_SETI ,     /*  A,B,C; */
	DAO_CODE_SETM ,     /*  A,C,C+1,...,C+B; */
	DAO_CODE_MOVE ,     /*  A,C;   */
	DAO_CODE_UNARY ,    /*  A,C;   */
	DAO_CODE_BINARY ,   /*  A,B,C; */
	DAO_CODE_UNARY2 ,   /*  B,C;   */
	DAO_CODE_MATRIX ,   /*  C,A,A+1,...,A+N-1; where N=(B>>8)*(B&0xff); */
	DAO_CODE_ENUM ,     /*  C,A,A+1,...,A+B-1; */
	DAO_CODE_ENUM2 ,    /*  C,A,A+1,...,A+B; */
	DAO_CODE_CALL ,     /*  C,A,A+1,...,A+N; where N=B&0xff*/
	DAO_CODE_ROUTINE ,  /*  C,A,A+1,...,A+B; */
	DAO_CODE_CLASS ,    /*  C,A,A+1,...,A+B;   */
	DAO_CODE_YIELD ,    /*  C,A,A+1,...,A+B-1; */
	DAO_CODE_EXPLIST ,  /*  A,A+1,...,A+B-1; */
	DAO_CODE_BRANCH ,   /*  A;   */
	DAO_CODE_JUMP
};

enum DaoOperandType
{
	DAO_OPERAND_N , /* None; */
	DAO_OPERAND_A ,
	DAO_OPERAND_B ,
	DAO_OPERAND_C
};

struct DaoVmCode
{
	unsigned short  code; /* opcode */
	unsigned short  a, b, c; /* register ids for operands */
};

DAO_DLL const char* DaoVmCode_GetOpcodeName( int code );
DAO_DLL uchar_t     DaoVmCode_GetOpcodeBase( int code );
DAO_DLL uchar_t     DaoVmCode_CheckPermutable( int code );
DAO_DLL uchar_t     DaoVmCode_GetOpcodeType( DaoVmCode *self );
DAO_DLL uchar_t     DaoVmCode_GetResultOperand( DaoVmCode *self );
DAO_DLL DaoVmCode   DaoVmCode_CheckOperands( DaoVmCode *self );

DAO_DLL DaoVmCode*  DVector_PushCode( DVector *self, DaoVmCode code );


struct DaoVmCodeX
{
	unsigned short  code; /* opcode */
	unsigned short  a, b, c; /* register ids for operands */
	unsigned short  level; /* lexical level */
	unsigned short  line; /* line number in the source file */
	unsigned int    first; /* first token */
	unsigned short  middle; /* middle token, with respect to first */
	unsigned short  last; /* last token, with respect to first */
};
void DaoVmCode_Print( DaoVmCode self, char *buffer );
void DaoVmCodeX_Print( DaoVmCodeX self, char *buffer );


#define DaoGetSectionCode1(C) ((C[1].code == DVM_GOTO && C[2].code == DVM_SECT) ? C+2 : NULL)
#define DaoGetSectionCode2(C) ((C[1].code == DVM_GOTO && C[3].code == DVM_SECT) ? C+3 : NULL)
#define DaoGetSectionCode(C)  (C[2].code == DVM_NOP ? DaoGetSectionCode2(C) : DaoGetSectionCode1(C))

#endif
