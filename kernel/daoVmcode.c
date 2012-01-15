/*=========================================================================================
  This file is a part of a virtual machine for the Dao programming language.
  Copyright (C) 2006-2012, Fu Limin. Email: fu@daovm.net, limin.fu@yahoo.com

  This software is free software; you can redistribute it and/or modify it under the terms 
  of the GNU Lesser General Public License as published by the Free Software Foundation; 
  either version 2.1 of the License, or (at your option) any later version.

  This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the GNU Lesser General Public License for more details.
  =========================================================================================*/

#include"daoVmcode.h"

/* Used only by parser for compiling expression lists: */
unsigned char permutableCodes[] = 
{
	1 /*NOP*/, 1 /*DATA*/, 
	1 /*GETCL*/, 1 /*GETCK*/, 1 /*GETCG*/, 
	1 /*GETVH*/, 1 /*GETVL*/, 1 /*GETVO*/, 1 /*GETVK*/, 1 /*GETVG*/, 
	0 /*GETI*/ , 0 /*GETDI*/, 0 /*GETMI*/, 0 /*GETF*/ , 0 /*GETMF*/,
	1 /*SETVH*/, 1 /*SETVL*/, 1 /*SETVO*/, 1 /*SETVK*/, 1 /*SETVG*/, 
	0 /*SETI*/, 0 /*SETDI*/, 0 /*SETMI*/, 0 /*SETF*/, 0 /*SETMF*/,
	1 /*LOAD*/, 0 /*CAST*/, 0 /*MOVE*/,
	0 /*NOT*/, 0 /*UNMS*/, 0 /*BITREV*/,
	0 /*ADD*/, 0 /*SUB*/, 0 /*MUL*/, 0 /*DIV*/, 0 /*MOD*/, 0 /*POW*/,
	0 /*AND*/, 0 /*OR*/, 0 /*LT*/, 0 /*LE*/, 0 /*EQ*/, 0 /*NE*/, 1 /*IN*/,
	0 /*BITAND*/, 0 /*BITOR*/, 0 /*BITXOR*/, 0 /*BITLFT*/, 0 /*BITRIT*/,
	1 /*CHECK*/, 1 /*NAMEVA*/, 1 /*PAIR*/, 1 /*TUPLE*/,
	1 /*LIST*/, 1 /*MAP*/, 1 /*HASH*/, 1 /*ARRAY*/, 1 /*MATRIX*/, 1 /*CURRY*/, 1 /*MCURRY*/,
	1 /*ROUTINE*/, 1 /*CLASS*/, 1 /*GOTO*/, 1 /*SWITCH*/, 1 /*CASE*/, 0 /*ITER*/, 1 /*TEST*/, 
	1 /*MATH*/, 0 /*CALL*/, 0 /*MCALL*/, 
	0 /*CRRE*/, 0 /*JITC*/, 0 /*RETURN*/, 0 /*YIELD*/, 0 /*DEBUG*/, 0 /*SECT*/
};

static const char *const vmOperNames[] = 
{
	"NOP", "DATA", 
	"GETCL", "GETCK", "GETCG", 
	"GETVH", "GETVL", "GETVO", "GETVK", "GETVG", 
	"GETI", "GETDI", "GETMI", "GETF", "GETMF",
	"SETVH", "SETVL", "SETVO", "SETVK", "SETVG", 
	"SETI", "SETDI", "SETMI", "SETF", "SETMF",
	"LOAD", "CAST", "MOVE",
	"NOT", "UNMS", "BITREV",
	"ADD", "SUB", "MUL", "DIV", "MOD", "POW",
	"AND", "OR", "LT", "LE", "EQ", "NE", "IN",
	"BITAND", "BITOR", "BITXOR", "BITLFT", "BITRIT",
	"CHECK", "NAMEVA", "PAIR", "TUPLE",
	"LIST", "MAP", "HASH", "ARRAY", "MATRIX", "CURRY", "MCURRY",
	"ROUTINE", "CLASS", "GOTO", "SWITCH", "CASE", "ITER", "TEST", 
	"MATH", "CALL", "MCALL", 
	"CRRE", "JITC", "RETURN", "YIELD", "DEBUG", "SECT",
	"DATA_I" , "DATA_F" , "DATA_D" , 
	"GETCL_I" , "GETCL_F" , "GETCL_D" , 
	"GETCK_I" , "GETCK_F" , "GETCK_D" , 
	"GETCG_I" , "GETCG_F" , "GETCG_D" , 
	"GETVH_I" , "GETVH_F" , "GETVH_D" , 
	"GETVL_I" , "GETVL_F" , "GETVL_D" , 
	"GETVO_I" , "GETVO_F" , "GETVO_D" , 
	"GETVK_I" , "GETVK_F" , "GETVK_D" , 
	"GETVG_I" , "GETVG_F" , "GETVG_D" , 
	"SETVH_II", "SETVH_IF", "SETVH_ID", 
	"SETVH_FI", "SETVH_FF", "SETVH_FD", 
	"SETVH_DI", "SETVH_DF", "SETVH_DD",
	"SETVL_II", "SETVL_IF", "SETVL_ID", 
	"SETVL_FI", "SETVL_FF", "SETVL_FD", 
	"SETVL_DI", "SETVL_DF", "SETVL_DD",
	"SETVO_II", "SETVO_IF", "SETVO_ID", 
	"SETVO_FI", "SETVO_FF", "SETVO_FD", 
	"SETVO_DI", "SETVO_DF", "SETVO_DD",
	"SETVK_II", "SETVK_IF", "SETVK_ID", 
	"SETVK_FI", "SETVK_FF", "SETVK_FD", 
	"SETVK_DI", "SETVK_DF", "SETVK_DD",
	"SETVG_II", "SETVG_IF", "SETVG_ID", 
	"SETVG_FI", "SETVG_FF", "SETVG_FD", 
	"SETVG_DI", "SETVG_DF", "SETVG_DD",
	"MOVE_II", "MOVE_IF", "MOVE_ID",
	"MOVE_FI", "MOVE_FF", "MOVE_FD",
	"MOVE_DI", "MOVE_DF", "MOVE_DD",
	"MOVE_CC", "MOVE_SS", "MOVE_PP",
	"NOT_I", "NOT_F", "NOT_D", 
	"UNMS_I", "UNMS_F", "UNMS_D",
	"BITREV_I", "BITREV_F", "BITREV_D",
	"UNMS_C",
	"ADD_III", "SUB_III", "MUL_III", "DIV_III", "MOD_III", "POW_III",
	"AND_III", "OR_III", "LT_III", "LE_III", "EQ_III", "NE_III", 
	"BITAND_III", "BITOR_III", "BITXOR_III", "BITLFT_III", "BITRIT_III", 
	"ADD_FFF", "SUB_FFF", "MUL_FFF", "DIV_FFF", "MOD_FFF", "POW_FFF",
	"AND_FFF", "OR_FFF", "LT_FFF", "LE_FFF", "EQ_FFF", "NE_FFF", 
	"BITAND_FFF", "BITOR_FFF", "BITXOR_FFF", "BITLFT_FFF", "BITRIT_FFF", 
	"ADD_DDD", "SUB_DDD", "MUL_DDD", "DIV_DDD", "MOD_DDD", "POW_DDD",
	"AND_DDD", "OR_DDD", "LT_DDD", "LE_DDD", "EQ_DDD", "NE_DDD", 
	"BITAND_DDD", "BITOR_DDD", "BITXOR_DDD", "BITLFT_DDD", "BITRIT_DDD", 
	"ADD_FNN", "SUB_FNN", "MUL_FNN", "DIV_FNN", "MOD_FNN", "POW_FNN",
	"AND_FNN", "OR_FNN", "LT_FNN", "LE_FNN", "EQ_FNN", "NE_FNN", 
	"BITLFT_FNN", "BITRIT_FNN", 
	"ADD_DNN", "SUB_DNN", "MUL_DNN", "DIV_DNN", "MOD_DNN", "POW_DNN",
	"AND_DNN", "OR_DNN", "LT_DNN", "LE_DNN", "EQ_DNN", "NE_DNN", 
	"BITLFT_DNN", "BITRIT_DNN", 
	"ADD_CC", "SUB_CC", "MUL_CC", "DIV_CC", 
	"ADD_SS", "LT_SS", "LE_SS", "EQ_SS", "NE_SS",
	"GETI_LI", "SETI_LI",
	"GETI_SI", "SETI_SII",
	"GETI_LII", "GETI_LFI", "GETI_LDI", "GETI_LSI",
	"SETI_LIII", "SETI_LIIF", "SETI_LIID",
	"SETI_LFII", "SETI_LFIF", "SETI_LFID",
	"SETI_LDII", "SETI_LDIF", "SETI_LDID", "SETI_LSIS",
	"GETI_AII", "GETI_AFI", "GETI_ADI",
	"SETI_AIII", "SETI_AIIF", "SETI_AIID",
	"SETI_AFII", "SETI_AFIF", "SETI_AFID",
	"SETI_ADII", "SETI_ADIF", "SETI_ADID",
	"GETI_TI", "SETI_TI", 
	"GETF_T", "GETF_TI", "GETF_TF", "GETF_TD", "GETF_TS",
	"SETF_T",
	"SETF_TII", "SETF_TIF", "SETF_TID",
	"SETF_TFI", "SETF_TFF", "SETF_TFD",
	"SETF_TDI", "SETF_TDF", "SETF_TDD", "SETF_TSS",
	"GETI_ACI", "SETI_ACI",
	"GETMI_A", "SETMI_A",
	"GETF_KC", "GETF_KG", "GETF_OC", "GETF_OG", "GETF_OV", 
	"SETF_KG", "SETF_OG", "SETF_OV",
	"GETF_KCI", "GETF_KCF", "GETF_KCD",
	"GETF_KGI", "GETF_KGF", "GETF_KGD",
	"GETF_OCI", "GETF_OCF", "GETF_OCD",
	"GETF_OGI", "GETF_OGF", "GETF_OGD",
	"GETF_OVI", "GETF_OVF", "GETF_OVD",
	"SETF_KGII", "SETF_KGIF", "SETF_KGID", 
	"SETF_KGFI", "SETF_KGFF", "SETF_KGFD", 
	"SETF_KGDI", "SETF_KGDF", "SETF_KGDD", 
	"SETF_OGII", "SETF_OGIF", "SETF_OGID", 
	"SETF_OGFI", "SETF_OGFF", "SETF_OGFD", 
	"SETF_OGDI", "SETF_OGDF", "SETF_OGDD", 
	"SETF_OVII", "SETF_OVIF", "SETF_OVID",
	"SETF_OVFI", "SETF_OVFF", "SETF_OVFD",
	"SETF_OVDI", "SETF_OVDF", "SETF_OVDD",
	"TEST_I", "TEST_F", "TEST_D",
	"CHECK_ST" ,
	"GOTO",
	"???",

	/* for compiling only */
	"label",
	"load2",
	"loop",
	"branch",
	"do" ,
	"lbra" ,
	"rbra" ,
	"try" ,
	"raise" ,
	"catch" ,
	"unused"
};
const char* getOpcodeName( int opc )
{
	if( opc >= 0 && opc < DVM_NULL ) return vmOperNames[ opc ];
	if( opc > DVM_NULL && opc <= DVM_UNUSED ) return vmOperNames[ opc-DVM_LABEL+DVM_NULL+1 ];
	return "???";
}

void DaoVmCode_Print( DaoVmCode self, char *buffer )
{
	const char *name = getOpcodeName( self.code );
	static const char *fmt = "%-11s : %6i , %6i , %6i ;\n";
	if( buffer == NULL )
		printf( fmt, name, self.a, self.b, self.c );
	else
		sprintf( buffer, fmt, name, self.a, self.b, self.c );
}
void DaoVmCodeX_Print( DaoVmCodeX self, char *annot )
{
	const char *name = getOpcodeName( self.code );
	static const char *fmt = "%-11s : %6i , %6i , %6i ;  %4i,  %s\n";
	printf( fmt, name, self.a, self.b, self.c, self.line, annot ? annot : "" );
}
