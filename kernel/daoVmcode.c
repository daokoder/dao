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

#include"daoVmcode.h"

typedef struct DaoVmCodeInfo DaoVmCodeInfo;

struct DaoVmCodeInfo
{
	const char     *name;
	unsigned short  base;
	unsigned char   type;
	unsigned char   perm; /* Used only by parser for compiling expression lists: */
};

static DaoVmCodeInfo dao_code_infolist[] =
{
	{ "NOP",        DVM_NOP,        DAO_CODE_NOP,     1 },
	{ "DATA",       DVM_DATA,       DAO_CODE_GETC,    1 },
	{ "GETCL",      DVM_GETCL,      DAO_CODE_GETC,    1 },
	{ "GETCK",      DVM_GETCK,      DAO_CODE_GETC,    1 },
	{ "GETCG",      DVM_GETCG,      DAO_CODE_GETC,    1 },
	{ "GETVH",      DVM_GETVH,      DAO_CODE_GETU,    1 },
	{ "GETVS",      DVM_GETVS,      DAO_CODE_GETG,    1 },
	{ "GETVO",      DVM_GETVO,      DAO_CODE_GETG,    1 },
	{ "GETVK",      DVM_GETVK,      DAO_CODE_GETG,    1 },
	{ "GETVG",      DVM_GETVG,      DAO_CODE_GETG,    1 },
	{ "GETI",       DVM_GETI,       DAO_CODE_GETI,    0 },
	{ "GETDI",      DVM_GETDI,      DAO_CODE_GETF,    0 },
	{ "GETMI",      DVM_GETMI,      DAO_CODE_GETM,    0 },
	{ "GETF",       DVM_GETF,       DAO_CODE_GETF,    0 },
	{ "SETVH",      DVM_SETVH,      DAO_CODE_SETU,    1 },
	{ "SETVS",      DVM_SETVS,      DAO_CODE_SETG,    1 },
	{ "SETVO",      DVM_SETVO,      DAO_CODE_SETG,    1 },
	{ "SETVK",      DVM_SETVK,      DAO_CODE_SETG,    1 },
	{ "SETVG",      DVM_SETVG,      DAO_CODE_SETG,    1 },
	{ "SETI",       DVM_SETI,       DAO_CODE_SETI,    0 },
	{ "SETDI",      DVM_SETDI,      DAO_CODE_SETF,    0 },
	{ "SETMI",      DVM_SETMI,      DAO_CODE_SETM,    0 },
	{ "SETF",       DVM_SETF,       DAO_CODE_SETF,    0 },
	{ "LOAD",       DVM_LOAD,       DAO_CODE_MOVE,    1 },
	{ "CAST",       DVM_CAST,       DAO_CODE_MOVE,    0 },
	{ "MOVE",       DVM_MOVE,       DAO_CODE_MOVE,    0 },
	{ "NOT",        DVM_NOT,        DAO_CODE_UNARY,   0 },
	{ "MINUS",      DVM_MINUS,      DAO_CODE_UNARY,   0 },
	{ "TILDE",      DVM_TILDE,      DAO_CODE_UNARY,   0 },
	{ "SIZE",       DVM_SIZE,       DAO_CODE_UNARY,   0 },
	{ "ADD",        DVM_ADD,        DAO_CODE_BINARY,  0 },
	{ "SUB",        DVM_SUB,        DAO_CODE_BINARY,  0 },
	{ "MUL",        DVM_MUL,        DAO_CODE_BINARY,  0 },
	{ "DIV",        DVM_DIV,        DAO_CODE_BINARY,  0 },
	{ "MOD",        DVM_MOD,        DAO_CODE_BINARY,  0 },
	{ "POW",        DVM_POW,        DAO_CODE_BINARY,  0 },
	{ "AND",        DVM_AND,        DAO_CODE_BINARY,  0 },
	{ "OR",         DVM_OR,         DAO_CODE_BINARY,  0 },
	{ "LT",         DVM_LT,         DAO_CODE_BINARY,  0 },
	{ "LE",         DVM_LE,         DAO_CODE_BINARY,  0 },
	{ "EQ",         DVM_EQ,         DAO_CODE_BINARY,  0 },
	{ "NE",         DVM_NE,         DAO_CODE_BINARY,  0 },
	{ "IN",         DVM_IN,         DAO_CODE_BINARY,  1 },
	{ "BITAND",     DVM_BITAND,     DAO_CODE_BINARY,  0 },
	{ "BITOR",      DVM_BITOR,      DAO_CODE_BINARY,  0 },
	{ "BITXOR",     DVM_BITXOR,     DAO_CODE_BINARY,  0 },
	{ "BITLFT",     DVM_BITLFT,     DAO_CODE_BINARY,  0 },
	{ "BITRIT",     DVM_BITRIT,     DAO_CODE_BINARY,  0 },
	{ "CHECK",      DVM_CHECK,      DAO_CODE_BINARY,  1 },
	{ "NAMEVA",     DVM_NAMEVA,     DAO_CODE_UNARY2,  1 },
	{ "PAIR",       DVM_PAIR,       DAO_CODE_BINARY,  1 },
	{ "TUPLE",      DVM_TUPLE,      DAO_CODE_ENUM,    1 },
	{ "LIST",       DVM_LIST,       DAO_CODE_ENUM,    1 },
	{ "MAP",        DVM_MAP,        DAO_CODE_ENUM,    1 },
	{ "HASH",       DVM_HASH,       DAO_CODE_ENUM,    1 },
	{ "VECTOR",     DVM_VECTOR,     DAO_CODE_ENUM,    1 },
	{ "MATRIX",     DVM_MATRIX,     DAO_CODE_MATRIX,  1 },
	{ "APLIST",     DVM_APLIST,     DAO_CODE_ENUM,    1 },
	{ "APVECTOR",   DVM_APVECTOR,   DAO_CODE_ENUM,    1 },
	{ "CURRY",      DVM_CURRY,      DAO_CODE_ENUM2,   1 },
	{ "MCURRY",     DVM_MCURRY,     DAO_CODE_ENUM2,   1 },
	{ "ROUTINE",    DVM_ROUTINE,    DAO_CODE_ROUTINE, 1 },
	{ "GOTO",       DVM_GOTO,       DAO_CODE_JUMP,    0 },
	{ "SWITCH",     DVM_SWITCH,     DAO_CODE_BRANCH,  0 },
	{ "CASE",       DVM_CASE,       DAO_CODE_JUMP,    0 },
	{ "ITER",       DVM_ITER,       DAO_CODE_MOVE,    0 },
	{ "TEST",       DVM_TEST,       DAO_CODE_BRANCH,  0 },
	{ "MATH",       DVM_MATH,       DAO_CODE_UNARY2,  1 },
	{ "CALL",       DVM_CALL,       DAO_CODE_CALL,    0 },
	{ "MCALL",      DVM_MCALL,      DAO_CODE_CALL,    0 },
	{ "RETURN",     DVM_RETURN,     DAO_CODE_EXPLIST, 0 },
	{ "YIELD",      DVM_YIELD,      DAO_CODE_YIELD,   0 },
	{ "EVAL",       DVM_EVAL,       DAO_CODE_ROUTINE /*To avoid DCE*/,   0 },
	{ "SECT",       DVM_SECT,       DAO_CODE_EXPLIST, 0 },
	{ "JITC",       DVM_JITC,       DAO_CODE_NOP,     0 },
	{ "DEBUG",      DVM_DEBUG,      DAO_CODE_NOP,     0 },
	{ "DATA_I",     DVM_DATA_I,     DAO_CODE_GETC,    0 },
	{ "DATA_F",     DVM_DATA_F,     DAO_CODE_GETC,    0 },
	{ "DATA_D",     DVM_DATA_D,     DAO_CODE_GETC,    0 },
	{ "DATA_C",     DVM_DATA_C,     DAO_CODE_GETC,    0 },
	{ "GETCL_I",    DVM_GETCL_I,    DAO_CODE_GETC,    0 },
	{ "GETCL_F",    DVM_GETCL_F,    DAO_CODE_GETC,    0 },
	{ "GETCL_D",    DVM_GETCL_D,    DAO_CODE_GETC,    0 },
	{ "GETCL_C",    DVM_GETCL_C,    DAO_CODE_GETC,    0 },
	{ "GETCK_I",    DVM_GETCK_I,    DAO_CODE_GETC,    0 },
	{ "GETCK_F",    DVM_GETCK_F,    DAO_CODE_GETC,    0 },
	{ "GETCK_D",    DVM_GETCK_D,    DAO_CODE_GETC,    0 },
	{ "GETCK_C",    DVM_GETCK_C,    DAO_CODE_GETC,    0 },
	{ "GETCG_I",    DVM_GETCG_I,    DAO_CODE_GETC,    0 },
	{ "GETCG_F",    DVM_GETCG_F,    DAO_CODE_GETC,    0 },
	{ "GETCG_D",    DVM_GETCG_D,    DAO_CODE_GETC,    0 },
	{ "GETCG_C",    DVM_GETCG_C,    DAO_CODE_GETC,    0 },
	{ "GETVH_I",    DVM_GETVH_I,    DAO_CODE_GETU,    0 },
	{ "GETVH_F",    DVM_GETVH_F,    DAO_CODE_GETU,    0 },
	{ "GETVH_D",    DVM_GETVH_D,    DAO_CODE_GETU,    0 },
	{ "GETVH_C",    DVM_GETVH_C,    DAO_CODE_GETU,    0 },
	{ "GETVS_I",    DVM_GETVS_I,    DAO_CODE_GETG,    0 },
	{ "GETVS_F",    DVM_GETVS_F,    DAO_CODE_GETG,    0 },
	{ "GETVS_D",    DVM_GETVS_D,    DAO_CODE_GETG,    0 },
	{ "GETVS_C",    DVM_GETVS_C,    DAO_CODE_GETG,    0 },
	{ "GETVO_I",    DVM_GETVO_I,    DAO_CODE_GETG,    0 },
	{ "GETVO_F",    DVM_GETVO_F,    DAO_CODE_GETG,    0 },
	{ "GETVO_D",    DVM_GETVO_D,    DAO_CODE_GETG,    0 },
	{ "GETVO_C",    DVM_GETVO_C,    DAO_CODE_GETG,    0 },
	{ "GETVK_I",    DVM_GETVK_I,    DAO_CODE_GETG,    0 },
	{ "GETVK_F",    DVM_GETVK_F,    DAO_CODE_GETG,    0 },
	{ "GETVK_D",    DVM_GETVK_D,    DAO_CODE_GETG,    0 },
	{ "GETVK_C",    DVM_GETVK_C,    DAO_CODE_GETG,    0 },
	{ "GETVG_I",    DVM_GETVG_I,    DAO_CODE_GETG,    0 },
	{ "GETVG_F",    DVM_GETVG_F,    DAO_CODE_GETG,    0 },
	{ "GETVG_D",    DVM_GETVG_D,    DAO_CODE_GETG,    0 },
	{ "GETVG_C",    DVM_GETVG_C,    DAO_CODE_GETG,    0 },
	{ "SETVH_II",   DVM_SETVH_II,   DAO_CODE_SETU,    0 },
	{ "SETVH_FF",   DVM_SETVH_FF,   DAO_CODE_SETU,    0 },
	{ "SETVH_DD",   DVM_SETVH_DD,   DAO_CODE_SETU,    0 },
	{ "SETVH_CC",   DVM_SETVH_CC,   DAO_CODE_SETU,    0 },
	{ "SETVS_II",   DVM_SETVS_II,   DAO_CODE_SETG,    0 },
	{ "SETVS_FF",   DVM_SETVS_FF,   DAO_CODE_SETG,    0 },
	{ "SETVS_DD",   DVM_SETVS_DD,   DAO_CODE_SETG,    0 },
	{ "SETVS_CC",   DVM_SETVS_CC,   DAO_CODE_SETG,    0 },
	{ "SETVO_II",   DVM_SETVO_II,   DAO_CODE_SETG,    0 },
	{ "SETVO_FF",   DVM_SETVO_FF,   DAO_CODE_SETG,    0 },
	{ "SETVO_DD",   DVM_SETVO_DD,   DAO_CODE_SETG,    0 },
	{ "SETVO_CC",   DVM_SETVO_CC,   DAO_CODE_SETG,    0 },
	{ "SETVK_II",   DVM_SETVK_II,   DAO_CODE_SETG,    0 },
	{ "SETVK_FF",   DVM_SETVK_FF,   DAO_CODE_SETG,    0 },
	{ "SETVK_DD",   DVM_SETVK_DD,   DAO_CODE_SETG,    0 },
	{ "SETVK_CC",   DVM_SETVK_CC,   DAO_CODE_SETG,    0 },
	{ "SETVG_II",   DVM_SETVG_II,   DAO_CODE_SETG,    0 },
	{ "SETVG_FF",   DVM_SETVG_FF,   DAO_CODE_SETG,    0 },
	{ "SETVG_DD",   DVM_SETVG_DD,   DAO_CODE_SETG,    0 },
	{ "SETVG_CC",   DVM_SETVG_CC,   DAO_CODE_SETG,    0 },
	{ "MOVE_II",    DVM_MOVE_II,    DAO_CODE_MOVE,    0 },
	{ "MOVE_IF",    DVM_MOVE_IF,    DAO_CODE_MOVE,    0 },
	{ "MOVE_ID",    DVM_MOVE_ID,    DAO_CODE_MOVE,    0 },
	{ "MOVE_FI",    DVM_MOVE_FI,    DAO_CODE_MOVE,    0 },
	{ "MOVE_FF",    DVM_MOVE_FF,    DAO_CODE_MOVE,    0 },
	{ "MOVE_FD",    DVM_MOVE_FD,    DAO_CODE_MOVE,    0 },
	{ "MOVE_DI",    DVM_MOVE_DI,    DAO_CODE_MOVE,    0 },
	{ "MOVE_DF",    DVM_MOVE_DF,    DAO_CODE_MOVE,    0 },
	{ "MOVE_DD",    DVM_MOVE_DD,    DAO_CODE_MOVE,    0 },
	{ "MOVE_CI",    DVM_MOVE_CI,    DAO_CODE_MOVE,    0 },
	{ "MOVE_CF",    DVM_MOVE_CF,    DAO_CODE_MOVE,    0 },
	{ "MOVE_CD",    DVM_MOVE_CD,    DAO_CODE_MOVE,    0 },
	{ "MOVE_CC",    DVM_MOVE_CC,    DAO_CODE_MOVE,    0 },
	{ "MOVE_SS",    DVM_MOVE_SS,    DAO_CODE_MOVE,    0 },
	{ "MOVE_PP",    DVM_MOVE_PP,    DAO_CODE_MOVE,    0 },
	{ "MOVE_XX",    DVM_MOVE_XX,    DAO_CODE_MOVE,    0 },
	{ "NOT_I",      DVM_NOT_I,      DAO_CODE_UNARY,   0 },
	{ "NOT_F",      DVM_NOT_F,      DAO_CODE_UNARY,   0 },
	{ "NOT_D",      DVM_NOT_D,      DAO_CODE_UNARY,   0 },
	{ "MINUS_I",    DVM_MINUS_I,    DAO_CODE_UNARY,   0 },
	{ "MINUS_F",    DVM_MINUS_F,    DAO_CODE_UNARY,   0 },
	{ "MINUS_D",    DVM_MINUS_D,    DAO_CODE_UNARY,   0 },
	{ "MINUS_C",    DVM_MINUS_C,    DAO_CODE_UNARY,   0 },
	{ "TILDE_I",    DVM_TILDE_I,    DAO_CODE_UNARY,   0 },
	{ "TILDE_C",    DVM_TILDE_C,    DAO_CODE_UNARY,   0 },
	{ "ADD_III",    DVM_ADD_III,    DAO_CODE_BINARY,  0 },
	{ "SUB_III",    DVM_SUB_III,    DAO_CODE_BINARY,  0 },
	{ "MUL_III",    DVM_MUL_III,    DAO_CODE_BINARY,  0 },
	{ "DIV_III",    DVM_DIV_III,    DAO_CODE_BINARY,  0 },
	{ "MOD_III",    DVM_MOD_III,    DAO_CODE_BINARY,  0 },
	{ "POW_III",    DVM_POW_III,    DAO_CODE_BINARY,  0 },
	{ "AND_III",    DVM_AND_III,    DAO_CODE_BINARY,  0 },
	{ "OR_III",     DVM_OR_III,     DAO_CODE_BINARY,  0 },
	{ "LT_III",     DVM_LT_III,     DAO_CODE_BINARY,  0 },
	{ "LE_III",     DVM_LE_III,     DAO_CODE_BINARY,  0 },
	{ "EQ_III",     DVM_EQ_III,     DAO_CODE_BINARY,  0 },
	{ "NE_III",     DVM_NE_III,     DAO_CODE_BINARY,  0 },
	{ "BITAND_III", DVM_BITAND_III, DAO_CODE_BINARY,  0 },
	{ "BITOR_III",  DVM_BITOR_III,  DAO_CODE_BINARY,  0 },
	{ "BITXOR_III", DVM_BITXOR_III, DAO_CODE_BINARY,  0 },
	{ "BITLFT_III", DVM_BITLFT_III, DAO_CODE_BINARY,  0 },
	{ "BITRIT_III", DVM_BITRIT_III, DAO_CODE_BINARY,  0 },
	{ "ADD_FFF",    DVM_ADD_FFF,    DAO_CODE_BINARY,  0 },
	{ "SUB_FFF",    DVM_SUB_FFF,    DAO_CODE_BINARY,  0 },
	{ "MUL_FFF",    DVM_MUL_FFF,    DAO_CODE_BINARY,  0 },
	{ "DIV_FFF",    DVM_DIV_FFF,    DAO_CODE_BINARY,  0 },
	{ "MOD_FFF",    DVM_MOD_FFF,    DAO_CODE_BINARY,  0 },
	{ "POW_FFF",    DVM_POW_FFF,    DAO_CODE_BINARY,  0 },
	{ "AND_FFF",    DVM_AND_FFF,    DAO_CODE_BINARY,  0 },
	{ "OR_FFF",     DVM_OR_FFF,     DAO_CODE_BINARY,  0 },
	{ "LT_IFF",     DVM_LT_IFF,     DAO_CODE_BINARY,  0 },
	{ "LE_IFF",     DVM_LE_IFF,     DAO_CODE_BINARY,  0 },
	{ "EQ_IFF",     DVM_EQ_IFF,     DAO_CODE_BINARY,  0 },
	{ "NE_IFF",     DVM_NE_IFF,     DAO_CODE_BINARY,  0 },
	{ "ADD_DDD",    DVM_ADD_DDD,    DAO_CODE_BINARY,  0 },
	{ "SUB_DDD",    DVM_SUB_DDD,    DAO_CODE_BINARY,  0 },
	{ "MUL_DDD",    DVM_MUL_DDD,    DAO_CODE_BINARY,  0 },
	{ "DIV_DDD",    DVM_DIV_DDD,    DAO_CODE_BINARY,  0 },
	{ "MOD_DDD",    DVM_MOD_DDD,    DAO_CODE_BINARY,  0 },
	{ "POW_DDD",    DVM_POW_DDD,    DAO_CODE_BINARY,  0 },
	{ "AND_DDD",    DVM_AND_DDD,    DAO_CODE_BINARY,  0 },
	{ "OR_DDD",     DVM_OR_DDD,     DAO_CODE_BINARY,  0 },
	{ "LT_IDD",     DVM_LT_IDD,     DAO_CODE_BINARY,  0 },
	{ "LE_IDD",     DVM_LE_IDD,     DAO_CODE_BINARY,  0 },
	{ "EQ_IDD",     DVM_EQ_IDD,     DAO_CODE_BINARY,  0 },
	{ "NE_IDD",     DVM_NE_IDD,     DAO_CODE_BINARY,  0 },
	{ "ADD_CCC",    DVM_ADD_CCC,    DAO_CODE_BINARY,  0 },
	{ "SUB_CCC",    DVM_SUB_CCC,    DAO_CODE_BINARY,  0 },
	{ "MUL_CCC",    DVM_MUL_CCC,    DAO_CODE_BINARY,  0 },
	{ "DIV_CCC",    DVM_DIV_CCC,    DAO_CODE_BINARY,  0 },
	{ "EQ_ICC",     DVM_EQ_ICC,     DAO_CODE_BINARY,  0 },
	{ "NE_ICC",     DVM_NE_ICC,     DAO_CODE_BINARY,  0 },
	{ "ADD_SSS",    DVM_ADD_SSS,    DAO_CODE_BINARY,  0 },
	{ "LT_ISS",     DVM_LT_ISS,     DAO_CODE_BINARY,  0 },
	{ "LE_ISS",     DVM_LE_ISS,     DAO_CODE_BINARY,  0 },
	{ "EQ_ISS",     DVM_EQ_ISS,     DAO_CODE_BINARY,  0 },
	{ "NE_ISS",     DVM_NE_ISS,     DAO_CODE_BINARY,  0 },
	{ "GETI_LI",    DVM_GETI_LI,    DAO_CODE_GETI,    0 },
	{ "SETI_LI",    DVM_SETI_LI,    DAO_CODE_SETI,    0 },
	{ "GETI_SI",    DVM_GETI_SI,    DAO_CODE_GETI,    0 },
	{ "SETI_SII",   DVM_SETI_SII,   DAO_CODE_SETI,    0 },
	{ "GETI_LII",   DVM_GETI_LII,   DAO_CODE_GETI,    0 },
	{ "GETI_LFI",   DVM_GETI_LFI,   DAO_CODE_GETI,    0 },
	{ "GETI_LDI",   DVM_GETI_LDI,   DAO_CODE_GETI,    0 },
	{ "GETI_LCI",   DVM_GETI_LCI,   DAO_CODE_GETI,    0 },
	{ "GETI_LSI",   DVM_GETI_LSI,   DAO_CODE_GETI,    0 },
	{ "SETI_LIII",  DVM_SETI_LIII,  DAO_CODE_SETI,    0 },
	{ "SETI_LFIF",  DVM_SETI_LFIF,  DAO_CODE_SETI,    0 },
	{ "SETI_LDID",  DVM_SETI_LDID,  DAO_CODE_SETI,    0 },
	{ "SETI_LCIC",  DVM_SETI_LCIC,  DAO_CODE_SETI,    0 },
	{ "SETI_LSIS",  DVM_SETI_LSIS,  DAO_CODE_SETI,    0 },
	{ "GETI_AII",   DVM_GETI_AII,   DAO_CODE_GETI,    0 },
	{ "GETI_AFI",   DVM_GETI_AFI,   DAO_CODE_GETI,    0 },
	{ "GETI_ADI",   DVM_GETI_ADI,   DAO_CODE_GETI,    0 },
	{ "GETI_ACI",   DVM_GETI_ACI,   DAO_CODE_GETI,    0 },
	{ "SETI_AIII",  DVM_SETI_AIII,  DAO_CODE_SETI,    0 },
	{ "SETI_AFIF",  DVM_SETI_AFIF,  DAO_CODE_SETI,    0 },
	{ "SETI_ADID",  DVM_SETI_ADID,  DAO_CODE_SETI,    0 },
	{ "SETI_ACIC",  DVM_SETI_ACIC,  DAO_CODE_SETI,    0 },
	{ "GETI_TI",    DVM_GETI_TI,    DAO_CODE_GETI,    0 },
	{ "SETI_TI",    DVM_SETI_TI,    DAO_CODE_SETI,    0 },
	{ "GETF_TI",    DVM_GETF_TI,    DAO_CODE_GETF,    0 },
	{ "GETF_TF",    DVM_GETF_TF,    DAO_CODE_GETF,    0 },
	{ "GETF_TD",    DVM_GETF_TD,    DAO_CODE_GETF,    0 },
	{ "GETF_TC",    DVM_GETF_TC,    DAO_CODE_GETF,    0 },
	{ "GETF_TX",    DVM_GETF_TX,    DAO_CODE_GETF,    0 },
	{ "SETF_TII",   DVM_SETF_TII,   DAO_CODE_SETF,    0 },
	{ "SETF_TFF",   DVM_SETF_TFF,   DAO_CODE_SETF,    0 },
	{ "SETF_TDD",   DVM_SETF_TDD,   DAO_CODE_SETF,    0 },
	{ "SETF_TCC",   DVM_SETF_TCC,   DAO_CODE_SETF,    0 },
	{ "SETF_TSS",   DVM_SETF_TSS,   DAO_CODE_SETF,    0 },
	{ "SETF_TPP",   DVM_SETF_TPP,   DAO_CODE_SETF,    0 },
	{ "SETF_TXX",   DVM_SETF_TXX,   DAO_CODE_SETF,    0 },
	{ "GETMI_AII",  DVM_GETMI_AII,  DAO_CODE_GETM,    0 },
	{ "GETMI_AFI",  DVM_GETMI_AFI,  DAO_CODE_GETM,    0 },
	{ "GETMI_ADI",  DVM_GETMI_ADI,  DAO_CODE_GETM,    0 },
	{ "GETMI_ACI",  DVM_GETMI_ACI,  DAO_CODE_GETM,    0 },
	{ "SETMI_AIII", DVM_SETMI_AIII, DAO_CODE_SETM,    0 },
	{ "SETMI_AFIF", DVM_SETMI_AFIF, DAO_CODE_SETM,    0 },
	{ "SETMI_ADID", DVM_SETMI_ADID, DAO_CODE_SETM,    0 },
	{ "SETMI_ACIC", DVM_SETMI_ACIC, DAO_CODE_SETM,    0 },
	{ "GETF_CX",    DVM_GETF_CX,    DAO_CODE_GETF,    0 },
	{ "SETF_CX",    DVM_SETF_CX,    DAO_CODE_SETF,    0 },
	{ "GETF_KC",    DVM_GETF_KC,    DAO_CODE_GETF,    0 },
	{ "GETF_KG",    DVM_GETF_KG,    DAO_CODE_GETF,    0 },
	{ "GETF_OC",    DVM_GETF_OC,    DAO_CODE_GETF,    0 },
	{ "GETF_OG",    DVM_GETF_OG,    DAO_CODE_GETF,    0 },
	{ "GETF_OV",    DVM_GETF_OV,    DAO_CODE_GETF,    0 },
	{ "SETF_KG",    DVM_SETF_KG,    DAO_CODE_SETF,    0 },
	{ "SETF_OG",    DVM_SETF_OG,    DAO_CODE_SETF,    0 },
	{ "SETF_OV",    DVM_SETF_OV,    DAO_CODE_SETF,    0 },
	{ "GETF_KCI",   DVM_GETF_KCI,   DAO_CODE_GETF,    0 },
	{ "GETF_KCF",   DVM_GETF_KCF,   DAO_CODE_GETF,    0 },
	{ "GETF_KCD",   DVM_GETF_KCD,   DAO_CODE_GETF,    0 },
	{ "GETF_KCC",   DVM_GETF_KCC,   DAO_CODE_GETF,    0 },
	{ "GETF_KGI",   DVM_GETF_KGI,   DAO_CODE_GETF,    0 },
	{ "GETF_KGF",   DVM_GETF_KGF,   DAO_CODE_GETF,    0 },
	{ "GETF_KGD",   DVM_GETF_KGD,   DAO_CODE_GETF,    0 },
	{ "GETF_KGC",   DVM_GETF_KGC,   DAO_CODE_GETF,    0 },
	{ "GETF_OCI",   DVM_GETF_OCI,   DAO_CODE_GETF,    0 },
	{ "GETF_OCF",   DVM_GETF_OCF,   DAO_CODE_GETF,    0 },
	{ "GETF_OCD",   DVM_GETF_OCD,   DAO_CODE_GETF,    0 },
	{ "GETF_OCC",   DVM_GETF_OCC,   DAO_CODE_GETF,    0 },
	{ "GETF_OGI",   DVM_GETF_OGI,   DAO_CODE_GETF,    0 },
	{ "GETF_OGF",   DVM_GETF_OGF,   DAO_CODE_GETF,    0 },
	{ "GETF_OGD",   DVM_GETF_OGD,   DAO_CODE_GETF,    0 },
	{ "GETF_OGC",   DVM_GETF_OGC,   DAO_CODE_GETF,    0 },
	{ "GETF_OVI",   DVM_GETF_OVI,   DAO_CODE_GETF,    0 },
	{ "GETF_OVF",   DVM_GETF_OVF,   DAO_CODE_GETF,    0 },
	{ "GETF_OVD",   DVM_GETF_OVD,   DAO_CODE_GETF,    0 },
	{ "GETF_OVC",   DVM_GETF_OVC,   DAO_CODE_GETF,    0 },
	{ "SETF_KGII",  DVM_SETF_KGII,  DAO_CODE_SETF,    0 },
	{ "SETF_KGFF",  DVM_SETF_KGFF,  DAO_CODE_SETF,    0 },
	{ "SETF_KGDD",  DVM_SETF_KGDD,  DAO_CODE_SETF,    0 },
	{ "SETF_KGCC",  DVM_SETF_KGCC,  DAO_CODE_SETF,    0 },
	{ "SETF_OGII",  DVM_SETF_OGII,  DAO_CODE_SETF,    0 },
	{ "SETF_OGFF",  DVM_SETF_OGFF,  DAO_CODE_SETF,    0 },
	{ "SETF_OGDD",  DVM_SETF_OGDD,  DAO_CODE_SETF,    0 },
	{ "SETF_OGCC",  DVM_SETF_OGCC,  DAO_CODE_SETF,    0 },
	{ "SETF_OVII",  DVM_SETF_OVII,  DAO_CODE_SETF,    0 },
	{ "SETF_OVFF",  DVM_SETF_OVFF,  DAO_CODE_SETF,    0 },
	{ "SETF_OVDD",  DVM_SETF_OVDD,  DAO_CODE_SETF,    0 },
	{ "SETF_OVCC",  DVM_SETF_OVCC,  DAO_CODE_SETF,    0 },
	{ "TEST_I",     DVM_TEST_I,     DAO_CODE_BRANCH,  0 },
	{ "TEST_F",     DVM_TEST_F,     DAO_CODE_BRANCH,  0 },
	{ "TEST_D",     DVM_TEST_D,     DAO_CODE_BRANCH,  0 },
	{ "MATH_I",     DVM_MATH_I,     DAO_CODE_UNARY2,  1 },
	{ "MATH_F",     DVM_MATH_F,     DAO_CODE_UNARY2,  1 },
	{ "MATH_D",     DVM_MATH_D,     DAO_CODE_UNARY2,  1 },
	{ "CHECK_ST",   DVM_CHECK_ST,   DAO_CODE_BINARY,  0 },
	{ "GOTO",       DVM_GOTO,       DAO_CODE_JUMP,    0 },
	{ "???",        DVM_UNUSED,     DAO_CODE_NOP,     0 },

	/* for compiling only */
	{ "label",      DVM_UNUSED,     DAO_CODE_NOP, 0 },
	{ "load2",      DVM_UNUSED,     DAO_CODE_NOP, 0 },
	{ "loop",       DVM_UNUSED,     DAO_CODE_NOP, 0 },
	{ "branch",     DVM_UNUSED,     DAO_CODE_NOP, 0 },
	{ "do",         DVM_UNUSED,     DAO_CODE_NOP, 0 },
	{ "lbra",       DVM_UNUSED,     DAO_CODE_NOP, 0 },
	{ "rbra",       DVM_UNUSED,     DAO_CODE_NOP, 0 },
	{ "default",    DVM_UNUSED,     DAO_CODE_NOP, 0 },
	{ "unused",     DVM_UNUSED,     DAO_CODE_NOP, 0 }
};

static uchar_t dao_vmcode_result_operand[] =
{
	DAO_OPERAND_N ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C , /* DAO_CODE_GETM; */
	DAO_OPERAND_A ,
	DAO_OPERAND_A ,
	DAO_OPERAND_A ,
	DAO_OPERAND_A ,
	DAO_OPERAND_A , /* DAO_CODE_SETM; */
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_C ,
	DAO_OPERAND_N ,
	DAO_OPERAND_N ,
	DAO_OPERAND_N
};

const char* DaoVmCode_GetOpcodeName( int code )
{
	if( code >= 0 && code <= DVM_UNUSED ) return dao_code_infolist[ code ].name;
	return "???";
}
uchar_t DaoVmCode_GetOpcodeBase( int code )
{
	if( code >= 0 && code <= DVM_UNUSED ) return dao_code_infolist[ code ].base;
	return DVM_NOP;
}
uchar_t DaoVmCode_CheckPermutable( int code )
{
	if( code >= 0 && code <= DVM_UNUSED ) return dao_code_infolist[ code ].perm;
	return 0;
}
uchar_t DaoVmCode_GetOpcodeType( DaoVmCode *self )
{
	int code = self->code;
	//if( code == DVM_ITER && self->b ) return DAO_CODE_ENUM;
	if( code >= 0 && code <= DVM_UNUSED ) return dao_code_infolist[ code ].type;
	return DAO_CODE_NOP;
}
uchar_t DaoVmCode_GetResultOperand( DaoVmCode *self )
{
	return dao_vmcode_result_operand[ DaoVmCode_GetOpcodeType( self ) ];
}
DaoVmCode DaoVmCode_CheckOperands( DaoVmCode *self )
{
	DaoVmCode vmc = { 0, 0, 0, 0 };
	switch( DaoVmCode_GetOpcodeType( self ) ){
	case DAO_CODE_NOP :
		break;
	case DAO_CODE_GETC :
	case DAO_CODE_GETG :
		vmc.c = 1;
		break;
	case DAO_CODE_SETU :
		vmc.a = 1;
		if( self->c != 0 ) vmc.b = 1;
		break;
	case DAO_CODE_SETG :
	case DAO_CODE_BRANCH :
		vmc.a = 1;
		break;
	case DAO_CODE_EXPLIST :
		if( self->b ) vmc.a = 1;
		break;
	case DAO_CODE_GETF : case DAO_CODE_SETF :
	case DAO_CODE_MOVE : case DAO_CODE_UNARY :
		vmc.a = 1;
		vmc.c = 1;
		break;
	case DAO_CODE_GETM :
	case DAO_CODE_ENUM2 : case DAO_CODE_MATRIX :
	case DAO_CODE_ROUTINE : case DAO_CODE_CALL :
		vmc.a = 1;
		vmc.c = 1;
		break;
	case DAO_CODE_SETM:
		vmc.a = 1;
		vmc.c = 1;
		break;
	case DAO_CODE_ENUM :
	case DAO_CODE_YIELD :
		if( self->b ) vmc.a = 1;
		vmc.c = 1;
		break;
	case DAO_CODE_SETI :
	case DAO_CODE_GETI :
	case DAO_CODE_BINARY :
		vmc.a = 1;
		vmc.b = 1;
		vmc.c = 1;
		break;
	case DAO_CODE_GETU :
		vmc.c = 1;
		if( self->a ) vmc.b = 1;
		break;
	case DAO_CODE_UNARY2 :
		vmc.b = 1;
		vmc.c = 1;
		break;
	default: break;
	}
	return vmc;
}

void DaoVmCode_Print( DaoVmCode self, char *buffer )
{
	const char *name = DaoVmCode_GetOpcodeName( self.code );
	static const char *fmt = "%-11s : %6i , %6i , %6i ;\n";
	if( buffer == NULL )
		printf( fmt, name, self.a, self.b, self.c );
	else
		sprintf( buffer, fmt, name, self.a, self.b, self.c );
}
void DaoVmCodeX_Print( DaoVmCodeX self, char *annot )
{
	const char *name = DaoVmCode_GetOpcodeName( self.code );
	static const char *fmt = "%-11s : %6i , %6i , %6i ;  %4i,  %s\n";
	printf( fmt, name, self.a, self.b, self.c, self.line, annot ? annot : "" );
}
