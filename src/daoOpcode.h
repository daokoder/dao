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

#ifndef DAO_OPCODE_H
#define DAO_OPCODE_H

enum DaoOpcode
{
	DVM_NOP = 0, /* no operation */
	DVM_DATA , /* create primitive data: A: type, B: value, C: register; */
	DVM_GETCL , /* get local const: C = A::B; current routine, A=0; up routine: A=1; */
	DVM_GETCK , /* get class const: C = A::B; current class, A=0; parent class: A>=1; */
	DVM_GETCG , /* get global const: C = A::B; current namespace, A=0; loaded: A>=1; */
	DVM_GETVL , /* get local variables: C = A::B; A=1, up routine; */
	DVM_GETVO , /* get instance object variables: C = A::B; A=0; */
	DVM_GETVK , /* get class global variables: C = A::B; A: the same as GETCK; */
	DVM_GETVG , /* get global variables: C = A::B; A: the same as GETCG; */
	DVM_GETI ,  /* GET Item(s) : C = A[B]; */
	DVM_GETF ,  /* GET Field : C = A.B */
	DVM_GETMF , /* GET Meta Field: C = A->B */
	DVM_SETVL , /* set local variables: C::B = A, C the same as A in DVM_GETVL */
	DVM_SETVO , /* set object variables: C::B = A, C the same as A in DVM_GETVO */
	DVM_SETVK , /* set class variables: C::B = A, C the same as A in DVM_GETVK */
	DVM_SETVG , /* set global variables: C::B = A, C the same as A in DVM_GETVG */
	DVM_SETI , /* SET Item(s) : C[B] = A;  */
	DVM_SETF , /* SET Field : C.B = A */
	DVM_SETMF , /* SET Meta Field : C->B = A */
	DVM_LOAD ,
	DVM_CAST , /* convert A to C if they have different types; */
	DVM_MOVE , /* C = A; if B==0, XXX it is compile from assignment, for typing system only */
	DVM_NOT ,  /* C = ! A; not */
	DVM_UNMS , /* C = - A; unary minus; */
	DVM_BITREV , /* C = ~ A */
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
	DVM_BITXOR ,  /* C = A ^ B */
	DVM_BITLFT , /* C = A << B */
	DVM_BITRIT , /* C = A >> B */
	DVM_CHECK , /* check type: C = A ?= B; C = A ?< B, where A is data, B is data or type */
	DVM_NAMEVA , /* C = A => B: name A, local constant, value B, local register */
	DVM_PAIR , /* C = A : B; create a pair of index, as an array; */
	DVM_TUPLE , /* tuple: C = ( A, A+1, ..., A+B-1 ); B>=2, items can be: name=>value */
	DVM_LIST , /* list: C = { A, A+1, ..., A+B-10 }, or {A:A+1},{ A : A+1 : A+2 }; */
	DVM_MAP , /* map: C = { A => A+1, ..., A+B-2 => A+B-1 }; if B==0, empty; */
	DVM_HASH , /* hash: C = { A : A+1, ..., A+B-2 : A+B-1 }; if B==0, empty; */
	DVM_ARRAY , /* array: C = [ A, A+1, ..., A+B-10 ], or [A:A+1],[ A : A+1 : A+2 ]; */
	DVM_MATRIX , /* matrix: C=[A,..,A+c-1;..;A+c*(r-1),..,A+c*r-1]; B=rc;r,c:8-bits each.*/
	DVM_CURRY , /* class_or_routine_name: A{ A+1, ..., A+B } */
	DVM_MCURRY , /* object.method: A{ A+1, ..., A+B } */
	DVM_ROUTINE , /* create a function, possibly with closure */
	DVM_CLASS , /* create a class, C = class( A ){ B }, A: a tuple, B: a proto-class */
	DVM_GOTO , /* go to B; */
	DVM_SWITCH , /* A: variable, B: location of default codes, C: number of cases */
	DVM_CASE , /* A: constant of the case, B: location of the case codes, C: case mode */
	DVM_ITER , /* create or reset an iterator at C for A; */
	DVM_TEST , /* if A, go to the next one; else, goto B-th instruction; */
	DVM_MATH , /* C = A( B ); A: sin,cos,...; B: double,complex */
	DVM_FUNCT , /* C = A( B ); A: map,reduce,...; B: list,tuple */
	DVM_CALL , /* call C = A( A+1, A+2, ..., A+B ); If B==0, no parameters;
				  call with caller's params C = A( 0, 1, ... ), if B==DAO_CALLER_PARAM. */
	DVM_MCALL , /* method call: x.y(...), pass x as the first parameter */
	DVM_CRRE , /* Check(B=0), Raise(C=0) or Rescue(C>0, goto C if not matching) Exceptions:
				  A,A+1,..,A+B-2; If B==1, no exception to raise or rescue. */
	DVM_JITC , /* run Just-In-Time compiled Code A, and skip the next B instructions */
	DVM_RETURN , /* return A, A+1,.., A+B-1; if B==0, no returns;  */
	DVM_YIELD , /* yield A, A+1,.., A+B-1; return data at C when resumed; */
	DVM_DEBUG , /* prompt to debugging mode */
	DVM_SECT ,   /* indicate the starting of a code subsection, A is the id. */

	/* optimized opcodes: */
	DVM_SETVL_II , 
	DVM_SETVL_IF , 
	DVM_SETVL_ID , 
	DVM_SETVL_FI , 
	DVM_SETVL_FF , 
	DVM_SETVL_FD , 
	DVM_SETVL_DI , 
	DVM_SETVL_DF , 
	DVM_SETVL_DD , 
	DVM_SETVO_II , 
	DVM_SETVO_IF , 
	DVM_SETVO_ID , 
	DVM_SETVO_FI , 
	DVM_SETVO_FF , 
	DVM_SETVO_FD , 
	DVM_SETVO_DI , 
	DVM_SETVO_DF , 
	DVM_SETVO_DD , 
	DVM_SETVK_II , 
	DVM_SETVK_IF , 
	DVM_SETVK_ID , 
	DVM_SETVK_FI , 
	DVM_SETVK_FF , 
	DVM_SETVK_FD , 
	DVM_SETVK_DI , 
	DVM_SETVK_DF , 
	DVM_SETVK_DD , 
	DVM_SETVG_II , 
	DVM_SETVG_IF , 
	DVM_SETVG_ID , 
	DVM_SETVG_FI , 
	DVM_SETVG_FF , 
	DVM_SETVG_FD , 
	DVM_SETVG_DI , 
	DVM_SETVG_DF , 
	DVM_SETVG_DD , 

	DVM_MOVE_II ,
	DVM_MOVE_IF ,
	DVM_MOVE_ID ,
	DVM_MOVE_FI ,
	DVM_MOVE_FF ,
	DVM_MOVE_FD ,
	DVM_MOVE_DI ,
	DVM_MOVE_DF ,
	DVM_MOVE_DD ,
	DVM_MOVE_CC , /* move complex number */
	DVM_MOVE_SS , /* string */
	DVM_MOVE_PP , /* pointer for typed operands */
	DVM_NOT_I ,
	DVM_NOT_F ,
	DVM_NOT_D ,
	DVM_UNMS_I ,
	DVM_UNMS_F ,
	DVM_UNMS_D ,
	DVM_BITREV_I ,
	DVM_BITREV_F ,
	DVM_BITREV_D ,
	DVM_UNMS_C ,
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
	DVM_LT_FFF ,
	DVM_LE_FFF ,
	DVM_EQ_FFF ,
	DVM_NE_FFF ,
	DVM_BITAND_FFF ,
	DVM_BITOR_FFF ,
	DVM_BITXOR_FFF ,
	DVM_BITLFT_FFF ,
	DVM_BITRIT_FFF ,
	/* double: */
	DVM_ADD_DDD ,
	DVM_SUB_DDD ,
	DVM_MUL_DDD ,
	DVM_DIV_DDD ,
	DVM_MOD_DDD ,
	DVM_POW_DDD ,
	DVM_AND_DDD ,
	DVM_OR_DDD ,
	DVM_LT_DDD ,
	DVM_LE_DDD ,
	DVM_EQ_DDD ,
	DVM_NE_DDD ,
	DVM_BITAND_DDD ,
	DVM_BITOR_DDD ,
	DVM_BITXOR_DDD ,
	DVM_BITLFT_DDD ,
	DVM_BITRIT_DDD ,
	/* mixed operand, float result */
	DVM_ADD_FNN ,
	DVM_SUB_FNN ,
	DVM_MUL_FNN ,
	DVM_DIV_FNN ,
	DVM_MOD_FNN ,
	DVM_POW_FNN ,
	DVM_AND_FNN ,
	DVM_OR_FNN ,
	DVM_LT_FNN ,
	DVM_LE_FNN ,
	DVM_EQ_FNN ,
	DVM_NE_FNN ,
	DVM_BITLFT_FNN ,
	DVM_BITRIT_FNN ,
	/* mixed operand, double result */
	DVM_ADD_DNN ,
	DVM_SUB_DNN ,
	DVM_MUL_DNN ,
	DVM_DIV_DNN ,
	DVM_MOD_DNN ,
	DVM_POW_DNN ,
	DVM_AND_DNN ,
	DVM_OR_DNN ,
	DVM_LT_DNN ,
	DVM_LE_DNN ,
	DVM_EQ_DNN ,
	DVM_NE_DNN ,
	DVM_BITLFT_DNN ,
	DVM_BITRIT_DNN ,

	/* string */
	DVM_ADD_SS , 
	DVM_LT_SS ,
	DVM_LE_SS ,
	DVM_EQ_SS ,
	DVM_NE_SS ,

	/* single indexing C=A[B]: GETI and MOVE */
	/* index should be integer, may be casted from float/double by the typing system */
	DVM_GETI_LI , /* optimization opcode for list: get item(s) : C = A[B]; list[int] */
	DVM_SETI_LI , /* set item : C[B] = A;  */
	DVM_GETI_SI , /* get char from a string: string[int] */
	DVM_SETI_SII , /* set char to a string: string[int]=int */
	DVM_GETI_LII , /* get item : C = A[B]; list<int>[int] */
	DVM_GETI_LFI , /* get item : C = A[B]; list<float>[int] */
	DVM_GETI_LDI , /* get item : C = A[B]; list<double>[int] */
	DVM_GETI_LSI , /* get item : C = A[B]; list<double>[int] */
	DVM_SETI_LIII , /* set item : C[B] = A; list<int>[int]=int */
	DVM_SETI_LIIF , /* set item : C[B] = A; list<int>[int]=float */
	DVM_SETI_LIID , /* set item : C[B] = A; list<int>[int]=double */
	DVM_SETI_LFII , /* set item : C[B] = A; list<float>[int]=int */
	DVM_SETI_LFIF , /* set item : C[B] = A;  */
	DVM_SETI_LFID , /* set item : C[B] = A;  */
	DVM_SETI_LDII , /* set item : C[B] = A; list<double>[int]=int */
	DVM_SETI_LDIF , /* set item : C[B] = A;  */
	DVM_SETI_LDID , /* set item : C[B] = A;  */
	DVM_SETI_LSIS , /* set item : C[B] = A;  */
	DVM_GETI_AII , /* get item : C = A[B]; array<int>[int] */
	DVM_GETI_AFI , /* get item : C = A[B]; array<float>[int] */
	DVM_GETI_ADI , /* get item : C = A[B]; array<double>[int] */
	DVM_SETI_AIII , /* set item : C[B] = A;  */
	DVM_SETI_AIIF , /* set item : C[B] = A;  */
	DVM_SETI_AIID , /* set item : C[B] = A;  */
	DVM_SETI_AFII , /* set item : C[B] = A;  */
	DVM_SETI_AFIF , /* set item : C[B] = A;  */
	DVM_SETI_AFID , /* set item : C[B] = A; array<float>[int]=double */
	DVM_SETI_ADII , /* set item : C[B] = A;  */
	DVM_SETI_ADIF , /* set item : C[B] = A;  */
	DVM_SETI_ADID , /* set item : C[B] = A;  */

	DVM_GETI_TI , /* optimization opcode for tuple: get item(s) : C = A[B]; tuple[int] */
	DVM_SETI_TI , /* set item : C[B] = A;  */
	/* access field by constant index; specialized from GETI[const] or GETF */
	DVM_GETF_T ,
	DVM_GETF_TI , /* get integer field by constant index; */
	DVM_GETF_TF , /* get float field by constant index; */
	DVM_GETF_TD , /* get double field by constant index; */
	DVM_GETF_TS , /* get string field by constant index; */
	DVM_SETF_T ,
	DVM_SETF_TII , /* set integer field to integer. */
	DVM_SETF_TIF , /* set integer field to float. */
	DVM_SETF_TID , /* set integer field to double. */
	DVM_SETF_TFI , /* set float field to integer. */
	DVM_SETF_TFF , /* set float field to float. */
	DVM_SETF_TFD , /* set float field to double. */
	DVM_SETF_TDI , /* set double field to integer. */
	DVM_SETF_TDF , /* set double field to float. */
	DVM_SETF_TDD , /* set double field to double. */
	DVM_SETF_TSS , /* set string field to string. */

	DVM_ADD_CC ,
	DVM_SUB_CC ,
	DVM_MUL_CC ,
	DVM_DIV_CC ,
	DVM_GETI_ACI , /* get item : C = A[B]; array<complex>[int] */
	DVM_SETI_ACI , /* set item : C[B] = A; */

	/* multiple indexing a[i,j] */
	DVM_GETI_AM , /* array: get item(s) : C = A[B]; B: integer/float numbers */
	DVM_SETI_AM , /* set item(s) : C[B] = A;  */

	/* get member of class instance by index instead of name: */
	DVM_GETF_M , /* get standard method */
	/* setters and getters */
	DVM_GETF_KC , /* get class field, const; code: GET Member Field Const*/
	DVM_GETF_KG , /* get class field, global */
	DVM_GETF_OC , /* get class instance field, const; code: GET Member Field Const*/
	DVM_GETF_OG , /* get class instance field, global */
	DVM_GETF_OV , /* get class instance field, variable */
	DVM_SETF_KG ,
	DVM_SETF_OG ,
	DVM_SETF_OV ,

	/* C=A.B : GETF and MOVE */
	DVM_GETF_KCI , /* GET Member Field Const Integer */
	DVM_GETF_KGI ,
	DVM_GETF_OCI , /* GET Member Field Const Integer */
	DVM_GETF_OGI ,
	DVM_GETF_OVI ,
	DVM_GETF_KCF , /* GET Member Field Const Float */
	DVM_GETF_KGF ,
	DVM_GETF_OCF , /* GET Member Field Const Float */
	DVM_GETF_OGF ,
	DVM_GETF_OVF ,
	DVM_GETF_KCD , /* GET Member Field Const Double*/
	DVM_GETF_KGD ,
	DVM_GETF_OCD , /* GET Member Field Const Double*/
	DVM_GETF_OGD ,
	DVM_GETF_OVD ,
	/* C.B=A specialize according to both: C.B and A */
	DVM_SETF_KGII ,
	DVM_SETF_OGII ,
	DVM_SETF_OVII ,
	DVM_SETF_KGIF ,
	DVM_SETF_OGIF ,
	DVM_SETF_OVIF ,
	DVM_SETF_KGID ,
	DVM_SETF_OGID ,
	DVM_SETF_OVID ,
	DVM_SETF_KGFI ,
	DVM_SETF_OGFI ,
	DVM_SETF_OVFI ,
	DVM_SETF_KGFF ,
	DVM_SETF_OGFF ,
	DVM_SETF_OVFF ,
	DVM_SETF_KGFD ,
	DVM_SETF_OGFD ,
	DVM_SETF_OVFD ,
	DVM_SETF_KGDI ,
	DVM_SETF_OGDI ,
	DVM_SETF_OVDI ,
	DVM_SETF_KGDF ,
	DVM_SETF_OGDF ,
	DVM_SETF_OVDF ,
	DVM_SETF_KGDD ,
	DVM_SETF_OGDD ,
	DVM_SETF_OVDD ,

	DVM_TEST_I ,
	DVM_TEST_F ,
	DVM_TEST_D ,

	DVM_CALL_CF , /* call C function */
	DVM_CALL_CMF , /* call C math function: double=func(double), JIT compilable */

	DVM_CALL_TC , /* type checked call, no need to check for overloading */
	DVM_MCALL_TC ,

	/* increase a count, and perform the normal goto operation 
	 * if the count does not exceed a safe bound. */
	DVM_SAFE_GOTO, 

	DVM_NULL
};
typedef enum DaoOpcode DaoOpcode;

/* Extra vmcode type for compiling:
 * They are used to setup proper indexing, branching or jumping. After this, 
 * they will be removed or replaced with proper instructions from DaoOpcode.
 */
enum DaoOpcodeExtra
{
	DVM_IDX = 1000,
	DVM_INCR ,
	DVM_DECR ,
	DVM_COMMA , 
	DVM_IF ,
	DVM_ELIF ,
	DVM_ELSE ,
	DVM_WHILE_AUX , /* B=jump_to_exit/break_loop, C=jump_to_skip_loop */
	DVM_WHILE ,
	DVM_FOR_AUX , /* B=jump_to_exit/break_loop, C=jump_to_skip_loop */
	DVM_FOR_STEP ,
	DVM_FOR ,
	DVM_DO ,
	DVM_UNTIL ,
	DVM_DOWHILE ,
	DVM_CASETAG ,
	DVM_DEFAULT ,
	DVM_BREAK ,
	DVM_SKIP ,
	DVM_LBRA ,
	DVM_RBRA ,
	DVM_LBRA2 ,
	DVM_RBRA2 ,
	DVM_TRY ,
	DVM_RETRY ,
	DVM_RAISE ,
	DVM_RESCUE ,
	DVM_LABEL ,
	DVM_SCBEGIN ,
	DVM_SCEND ,
	DVM_ENUM ,
	/* XXX: global a = "abc"; a.toupper();
	   generate DVM_SETVG_AUX after a call invoked by a global object,
	   remove it in typing system if the object is not a number or string.
	 */
	DVM_SETVG_AUX , 
	DVM_REFER ,
	DVM_UNUSED ,
	DVM_UNUSED2
};

enum DaoMathFunct
{
	DVM_MATH_ABS ,
	DVM_MATH_ACOS ,
	DVM_MATH_ARG ,
	DVM_MATH_ASIN ,
	DVM_MATH_ATAN ,
	DVM_MATH_CEIL ,
	DVM_MATH_COS ,
	DVM_MATH_COSH ,
	DVM_MATH_EXP ,
	DVM_MATH_FLOOR ,
	DVM_MATH_IMAG ,
	DVM_MATH_LOG ,
	DVM_MATH_NORM ,
	DVM_MATH_RAND ,
	DVM_MATH_REAL ,
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
	DVM_FUNCT_SELECT ,
	DVM_FUNCT_INDEX ,
	DVM_FUNCT_COUNT ,
	DVM_FUNCT_EACH ,
	DVM_FUNCT_REPEAT ,
	DVM_FUNCT_STRING ,
	DVM_FUNCT_ARRAY ,
	DVM_FUNCT_LIST ,
	DVM_FUNCT_NULL
};

#define DVR_MAX       0x70000

#define BITS_HIGH4 (0xffff<<12)
#define BITS_LOW12 (0xffff>>4)


#endif
