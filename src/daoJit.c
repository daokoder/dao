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


#include"daoRoutine.h"
#include"daoOpcode.h"
#include"daoContext.h"
#include"daoProcess.h"
#include"daoGC.h"
#include"daoObject.h"
#include"daoNumtype.h"
#include"daoThread.h"
#include"daoJit.h"
#include"stdlib.h"
#include"string.h"
#include"math.h"

#ifdef DAO_WITH_JIT

#define ALIGN_LEN   4*sizeof(void*)
#define ALIGN_MASK  (ALIGN_LEN-1)
#define ALIGN( x )  ((x+ALIGN_MASK) & ~ALIGN_MASK)

typedef struct DaoJitMapper DaoJitMapper;

struct DaoJitMapper
{
	DArray *jitBlocks; /* <DaoJitMemory*> */
#ifdef DAO_WITH_THREAD
	DMutex mutex;
#endif
};

DaoJitMapper daoJitMapper;

void DaoJitMapper_Init()
{
	daoJitMapper.jitBlocks = DArray_New(0);
#ifdef DAO_WITH_THREAD
	DMutex_Init( & daoJitMapper.mutex );
#endif
}

static void DaoJitMapper_MapRoutine( DaoRoutine *rout )
{
	DaoJitMemory *jm, *jm2;
	int i, n, id, first, last, size, bytes = 0;
	void *p;
	if( rout->jitMemory ) return;
	if( rout->binCodes->size ==0 ) return;
#ifdef DAO_WITH_THREAD
	DMutex_Lock( & daoJitMapper.mutex );
#endif
	for(i=0; i<rout->binCodes->size; i++){
		bytes += ALIGN( rout->binCodes->items.pString[i]->size + 8);
	}
	size = ALIGN( bytes + 16 );
	first = 0;
	last = daoJitMapper.jitBlocks->size-1;
	id = -1;
	while( first <= last ){
		id = ( first + last ) / 2;
		jm = (DaoJitMemory*) daoJitMapper.jitBlocks->items.pVoid[id];
		bytes = jm->size - jm->offset;
		//printf( "first = %i, last = %i, id = %i, bytes = %i\n", first, last, id, bytes );
		if( size == bytes ){
			break;
		}else if( size > bytes ){
			first = id + 1;
		}else{
			last = id - 1;
		}
	}
	//printf( "%i, id = %i\n", daoJitMapper.jitBlocks->size, id );
	if( last <0 || first >= daoJitMapper.jitBlocks->size ){
		id = daoJitMapper.jitBlocks->size;
		jm = DaoJitMemory_New( size );
		DArray_Append( daoJitMapper.jitBlocks, jm );
	}else{
		jm = (DaoJitMemory*) daoJitMapper.jitBlocks->items.pVoid[first];
	}
	//printf( "%i %i %i\n", jm->size, jm->offset, size );
	DArray_Resize( rout->jitFuncs, rout->binCodes->size, NULL );
	//printf( "DaoJitMapper_MapRoutine(): %i %i\n", daoJitMapper.jitBlocks->size, id );
	for(i=0; i<rout->binCodes->size; i++){
		n = rout->binCodes->items.pString[i]->size;
		p = rout->binCodes->items.pString[i]->mbs;
		rout->jitFuncs->items.pVoid[i] = jm->memory + jm->offset;
		//printf( "%p %i %i\n", jm->memory + jm->offset, jm->size-jm->offset, n );
		DaoJitMemory_Unprotect( jm );
		memcpy( jm->memory + jm->offset, p, n );
		DaoJitMemory_Protect( jm );
		jm->offset += ALIGN( n + 8 );
	}
	rout->jitMemory = jm;
	GC_IncRC( jm );
	for( i=id-1; i>=0; i--){
		jm2 = (DaoJitMemory*) daoJitMapper.jitBlocks->items.pVoid[i];
		if( jm2->size - jm2->offset < jm->size - jm->offset ) break;
		daoJitMapper.jitBlocks->items.pVoid[i] = jm;
		daoJitMapper.jitBlocks->items.pVoid[i+1] = jm2;
	}
#ifdef DAO_WITH_THREAD
	DMutex_Unlock( & daoJitMapper.mutex );
#endif
}

/* x86 x87 */
/* notes:
 *
 * %ebx, %esi and %edi should be preserved.
 *
 * for simplicity:
 * use %edx to store pointer locVars;
 * use %edi to store pointer extra;
 */
/* x86-64, parameter passing by registers:
 * linux: 
 *   the first 6 integers: RDI, RSI, RDX, RCX, R8, R9;
 *   the first 8 float/double: XMM0-XMM7, float low 32bits, double low 64bits;
 * win32: 
 *   the first 4 integers, RCX, RDX, R8, R9;
 *   the first 4 float/double: XMM0-XMM3;
 *
 * returned value:
 * linux:
 *   integer %rax;
 *   float low 32bits of XMM0
 *   double low 64bits of XMM0
 */

#if defined( _MSC_VER ) && defined( _M_X64 )

#define DAO_64BITS
#define DAO_64BITS_MSC

#elif defined( __GNUC__ ) && defined( __LP64__ )

#define DAO_64BITS
#define DAO_64BITS_GNUC

#endif

#define ADDR_BYTES  sizeof(void*)


#define X87_STACK_SIZE 8

#define EAX 0
#define ECX 1
#define EDX 2
#define EBX 3
#define ESP 4
#define EBP 5
#define ESI 6
#define EDI 7

#define EAX3 0
#define ECX3 1<<3
#define EDX3 2<<3
#define EBX3 3<<3
#define ESP3 4<<3
#define EBP3 5<<3
#define ESI3 6<<3
#define EDI3 7<<3

#define XMM0 0
#define XMM1 1

#define XMM0_O3 0
#define XMM1_O3 1<<3

enum
{
	SIB_S1 = 1<<8,
	SIB_S2 = 2<<8,
	SIB_S4 = 4<<8,
	SIB_S8 = 8<<8,
	DIS_B1 = 16<<8,
	DIS_B4 = 32<<8
};

static dint sizeof_value = sizeof(DValue);
static dint disp_value_v = (dint) & ((DValue*)0)->v;
static dint disp_string_mbs = (dint) & ((DString*)0)->mbs;
static dint disp_string_wcs = (dint) & ((DString*)0)->wcs;
static dint disp_string_size = (dint) & ((DaoMBS*)0)->size; 
static dint disp_string_data = (dint) & ((DaoMBS*)0)->data; 
static dint disp_list_items = (dint) & ((DaoList*)0)->items; 

static dint disp_object_data = (dint) & ((DaoObject*)0)->objValues; 
static dint disp_array_size = (dint) & ((DVarray*)0)->size;
static dint disp_array_data = (dint) & ((DVarray*)0)->data;

static dint disp_narray_data = (dint) & ((DaoArray*)0)->data;
static dint disp_narray_size = (dint) & ((DaoArray*)0)->size;

#define OpValue( id ) ( id * sizeof_value + disp_value_v )

#define LAB_RETURN  0x7fffffff

enum
{
	X86_LABEL ,

	X86_RET ,
	X86_SAHF ,
	X86_PUSH_D ,
	X86_PUSH_R ,
	X86_POP_R ,

	X86_JMP8 ,
	X86_JB8 , /* jump according to UNSIGNED integer comparison */
	X86_JBE8 ,
	X86_JE8_DUP ,
	X86_JNE8_DUP ,
	X86_JL8 , /* jump according to SIGNED integer comparison */
	X86_JLE8 ,
	X86_JE8 ,
	X86_JNE8 ,

	X86_JMP32 ,

	X86_JB32 ,
	X86_JBE32 ,
	X86_JE32 ,
	X86_JNE32 ,

	X86_JA32 ,
	X86_JAE32 ,

	X86_MOV8_RD , /* 8-bits immediate data to register */
	X86_MOV8_RR , /* 8-bits register to register */
	X86_MOV8_RM , /* 8-bits memory to register */
	X86_MOV8_MR , /* 8-bits register to memory */

	X86_MOV32_RD ,
	X86_MOV32_RR ,
	X86_MOV32_RM , /* in 64-bits mode, the memory is 64 bits */
	X86_MOV32_MR ,

	X86_MOV32B_RM , /* the memory is always 32 bits */
	X86_MOV32B_MR ,

	X86_MOV64_RD , /* 64-data to one of the R[]X registers */
	X86_MOV64_RR8 , /* register R8 to one of the R[]X registers */
	X86_MOV64_R8R ,

	X86_ADD8_RD , /* 8-bits immediate data to register */
	X86_ADD8_RR , /* 8-bits register to register */
	X86_ADD8_RM , /* 8-bits memory to register */
	X86_ADD8_MR , 

	X86_ADD32_RD ,
	X86_SUB32_RD ,
	X86_IMUL32_RD ,

	X86_ADD32_RR ,
	X86_SUB32_RR ,
	X86_IMUL32_RR ,
	X86_IDIV32_RR ,

	X86_ADD32_RM ,
	X86_ADD32_MR ,

	X86_CMP32_RD8 ,
	X86_CMP32_RR ,

	X86_PUSH_M ,
	X86_CALL ,

	X87_FNOP ,
	X87_FNINIT ,
	X87_FTST ,
	X87_FCLEX ,
	X87_FNSTSW_AX ,
	X87_FDECSTP ,
	X87_FINCSTP ,
	X87_FFREE_ST ,
	X87_FXCH_ST ,
	X87_FSTP_ST ,
	X87_FLD_ST ,
	X87_FLD1 ,
	X87_FLDZ ,
	X87_FILD_R ,
	X87_FILD32_M ,
	X87_FLD32_M ,
	X87_FLD64_M ,
	X87_FIST32_M ,
	X87_FST32_M ,
	X87_FST64_M ,
	X87_FISTP32_R ,
	X87_FISTP32_M ,
	X87_FSTP32_M ,
	X87_FSTP64_M ,

	X87_FICOMP32_R ,
	X87_FIADD32_R ,
	X87_FISUB32_R ,
	X87_FIMUL32_R ,
	X87_FIDIV32_R ,

	X87_FICOMP32_M ,
	X87_FCOM32_M ,
	X87_FCOMP32_M ,
	X87_FCOM64_M ,
	X87_FCOMP64_M ,
	X87_FIADD32_M ,
	X87_FISUB32_M ,
	X87_FIMUL32_M ,
	X87_FIDIV32_M ,
	X87_FADD32_M ,
	X87_FSUB32_M ,
	X87_FMUL32_M ,
	X87_FDIV32_M ,
	X87_FADD64_M ,
	X87_FSUB64_M ,
	X87_FMUL64_M ,
	X87_FDIV64_M ,

	X87_FIADDR32_M , /* for convinience */
	X87_FISUBR32_M ,
	X87_FIMULR32_M ,
	X87_FIDIVR32_M ,
	X87_FADDR32_M , /* for convinience */
	X87_FSUBR32_M ,
	X87_FMULR32_M ,
	X87_FDIVR32_M ,
	X87_FADDR64_M , /* for convinience */
	X87_FSUBR64_M ,
	X87_FMULR64_M ,
	X87_FDIVR64_M ,

	X87_FCOM_ST ,
	X87_FCOMP_ST ,
	X87_FADD_ST0 , /* st(0) = st(0)+st(i) */
	X87_FSUB_ST0 ,
	X87_FMUL_ST0 ,
	X87_FDIV_ST0 ,
	X87_FADD_STI , /* st(i) = st(0)+st(i) */
	X87_FSUB_STI ,
	X87_FMUL_STI ,
	X87_FDIV_STI ,
	X87_FADDR_ST0 ,
	X87_FSUBR_ST0 ,
	X87_FMULR_ST0 ,
	X87_FDIVR_ST0 ,
	X87_FADDR_STI ,
	X87_FSUBR_STI ,
	X87_FMULR_STI ,
	X87_FDIVR_STI ,
	X87_FCOMI ,

	SSE_MOVSS_RM , /* memory to xmmreg */
	SSE_MOVSD_RM , /* memory to xmmreg */
	SSE_MOVSD_MR , /* xmmreg to memory */
	SSE_MOVSD_RR , /* xmmreg to xmmreg */
	SSE_UCOMISS_RR ,
};

static const char *const instrNames[] =
{
	"LABEL" ,

	"RET" ,
	"SAHF" ,
	"PUSH_D" ,
	"PUSH_R" ,
	"POP_R" ,

	"JMP8" ,
	"JB8" ,
	"JBE8" ,
	"JE8" ,
	"JNE8" ,
	"JL8" ,
	"JLE8" ,
	"JE8" ,
	"JNE8" ,
	"JMP32" ,
	"JB32" ,
	"JBE32" ,
	"JE32" ,
	"JNE32" ,
	"JA32" ,
	"JAE32" ,

	"MOV8_RD" , /* 8-bits immediate data to register */
	"MOV8_RR" , /* 8-bits register to register */
	"MOV8_RM" , /* 8-bits memory to register */
	"MOV8_MR" , 

	"MOV32_RD" ,
	"MOV32_RR" ,
	"MOV32_RM" ,
	"MOV32_MR" ,

	"MOV32B_RM" ,
	"MOV32B_MR" ,

	"MOV64_RD" ,
	"MOV64_RR8" ,
	"MOV64_R8R" ,

	"ADD8_RD" , /* 8-bits immediate data to register */
	"ADD8_RR" , /* 8-bits register to register */
	"ADD8_RM" , /* 8-bits memory to register */
	"ADD8_MR" , 

	"ADD32_RD" ,
	"SUB32_RD" ,
	"IMUL32_RD" ,

	"ADD32_RR" ,
	"SUB32_RR" ,
	"IMUL32_RR" ,
	"IDIV32_RR" ,

	"ADD32_RM" ,
	"ADD32_MR" ,

	"CMP32_RD8" ,
	"CMP32_RR" ,

	"PUSH_M" ,
	"CALL" ,

	"FNOP" ,
	"FNINIT" ,
	"FTST" ,
	"FCLEX" ,
	"FNSTSW_AX" ,
	"FDECSTP" ,
	"FINCSTP" ,
	"FFREE_ST" ,
	"FXCH_ST" ,
	"FSTP_ST" ,
	"FLD_ST" ,
	"FLD1" ,
	"FLDZ" ,
	"FILD_R" ,
	"FILD32_M" ,
	"FLD32_M" ,
	"FLD64_M" ,
	"FIST32_M" ,
	"FST32_M" ,
	"FST64_M" ,
	"FISTP32_R" ,
	"FISTP32_M" ,
	"FSTP32_M" ,
	"FSTP64_M" ,

	"FICOMP32_R",
	"FIADD32_R",
	"FISUB32_R",
	"FIMUL32_R",
	"FIDIV32_R",

	"FICOMP32_M",
	"FCOM32_M",
	"FCOMP32_M",
	"FCOM64_M",
	"FCOMP64_M",
	"FIADD32_M",
	"FISUB32_M",
	"FIMUL32_M",
	"FIDIV32_M",
	"FADD32_M",
	"FSUB32_M",
	"FMUL32_M",
	"FDIV32_M",
	"FADD64_M",
	"FSUB64_M",
	"FMUL64_M",
	"FDIV64_M",

	"FIADDR32_M" ,
	"FISUBR32_M" ,
	"FIMULR32_M" ,
	"FIDIVR32_M" ,
	"FADDR32_M" ,
	"FSUBR32_M" ,
	"FMULR32_M" ,
	"FDIVR32_M" ,
	"FADDR64_M" ,
	"FSUBR64_M" ,
	"FMULR64_M" ,
	"FDIVR64_M" ,

	"FCOM_ST" ,
	"FCOMP_ST" ,
	"FADD_ST0" ,
	"FSUB_ST0" ,
	"FMUL_ST0" ,
	"FDIV_ST0" ,
	"FADD_STI" ,
	"FSUB_STI" ,
	"FMUL_STI" ,
	"FDIV_STI" ,
	"FADDR_ST0" ,
	"FSUBR_ST0" ,
	"FMULR_ST0" ,
	"FDIVR_ST0" ,
	"FADDR_STI" ,
	"FSUBR_STI" ,
	"FMULR_STI" ,
	"FDIVR_STI" ,
	"FCOMI" ,

	"MOVSS_RM" ,
	"MOVSD_RM" ,
	"MOVSD_MR" ,
	"MOVSD_RR" ,
	"UCOMISS_RR" ,
};

static void DString_AppendUBytes( DString *self, const uchar_t *bytes, size_t n )
{
	DString_AppendDataMBS( self, (const char*) bytes, n );
}

static const uchar_t ZeroByte4[] = { 0x00, 0x00, 0x00, 0x00 };

static const uchar_t x86CodeByte1[] =
{
	0xC3 , // RET
	0x9E , // SAHF
	0x68 , // PUSH_D
	0x50 , // PUSH_R
	0x58 , // POP_R
};
static const uchar_t x86CodeByte2[][2] =
{
	{ 0xC6, 0xC0 }, // X86_MOV8_RD , /* 8-bits immediate data to register */
	{ 0x8A, 0xC0 }, // X86_MOV8_RR , /* 8-bits register to register */
	{ 0x8A, 0x00 }, // X86_MOV8_RM , /* 8-bits memory to register */
	{ 0x88, 0x00 }, // X86_MOV8_MR , 

	{ 0xC7, 0xC0 }, // X86_MOV32_RD ,
	{ 0x8B, 0xC0 }, // X86_MOV32_RR ,
	{ 0x8B, 0x00 }, // X86_MOV32_RM ,
	{ 0x89, 0x00 }, // X86_MOV32_MR ,

	{ 0x8B, 0x00 }, // X86_MOV32B_RM ,
	{ 0x89, 0x00 }, // X86_MOV32B_MR ,

	{ 0x00, 0x00 }, // X86_MOV64_RD ,
	{ 0x8B, 0xC0 }, // X86_MOV64_RR8 ,
	{ 0x8B, 0xC0 }, // X86_MOV64_R8R ,

	{ 0x80, 0xC0 }, // ADD8_RD
	{ 0x02, 0xC0 }, // ADD8_RR
	{ 0x02, 0x00 }, // ADD8_RM
	{ 0x00, 0x00 }, // ADD8_MR

	{ 0x81, 0xC0 }, // ADD32_RD
	{ 0x81, 0xE8 }, // SUB32_RD
	{ 0x69, 0xC0 }, // IMUL32_RD

	{ 0x03, 0xC0 }, // ADD32_RR
	{ 0x2B, 0xC0 }, // SUB32_RR
	{ 0xF7, 0xE8 }, // IMUL32_RR EAX with register
	{ 0xF7, 0xF8 }, // IDIV32_RR EAX by register

	{ 0x03, 0x00 }, // ADD32_RM
	{ 0x01, 0x00 }, // ADD32_MR

	{ 0x83, 0xF8 }, // X86_CMP32_RD8 ,
	{ 0x3B, 0xC0 }, // X86_CMP32_RR ,

	{ 0xFF, 0x30 }, // X86_PUSH_M
	{ 0xFF, 0xD0 }, // X86_CALL
};
static const uchar_t x86CodeByte1Disp8[] =
{
	0xEB , // X86_JMP8
	0x72 , // X86_JB8
	0x76 , // X86_JBE8
	0x74 , // X86_JE8
	0x75 , // X86_JNE8
	0x7C , // X86_JL8
	0x7E , // X86_JLE8
	0x74 , // X86_JE8
	0x75 , // X86_JNE8
};
static const uchar_t x86CodeByte1Disp32[] =
{
	0xE9 , // X86_JMP32
};
static const uchar_t x86CodeByte2Disp32[][2] =
{
	{ 0x0F, 0x82 }, // X86_JB32
	{ 0x0F, 0x86 }, // X86_JBE32
	{ 0x0F, 0x84 }, // X86_JE32
	{ 0x0F, 0x85 }, // X86_JNE32
	{ 0x0F, 0x87 }, // X86_JA32
	{ 0x0F, 0x83 }, // X86_JAE32
};
static const uchar_t x87Code[][2] =
{
	{ 0xD9, 0xD0 }, // X87_FNOP
	{ 0xDB, 0xE3 }, // X87_FNINIT
	{ 0xD9, 0xE4 }, // X87_FTST
	{ 0xDB, 0xE2 }, // X87_FCLEX
	{ 0xDF, 0xE0 }, // X87_FNSTSW_AX
	{ 0xD9, 0xF6 }, // X87_FDECSTP
	{ 0xD9, 0xF7 }, // X87_FINCSTP
	{ 0xDD, 0xC0 }, // X87_FFREE_ST ,
	{ 0xD9, 0xC8 }, // X87_FXCH_ST
	{ 0xDD, 0xD8 }, // X87_FSTP_ST
	{ 0xD9, 0xC0 }, // X87_FLD_ST
	{ 0xD9, 0xE8 }, // X87_FLD1
	{ 0xD9, 0xEE }, // X87_FLDZ
	{ 0xDB, 0xC0 }, // X87_FILD_R
	{ 0xDB, 0x00 }, // X87_FILD32_M
	{ 0xD9, 0x00 }, // X87_FLD32_M
	{ 0xDD, 0x00 }, // X87_FLD64_M
	{ 0xDB, 0x10 }, // X87_FIST32_M
	{ 0xD9, 0x10 }, // X87_FST32_M
	{ 0xDD, 0x10 }, // X87_FST64_M
	{ 0xDB, 0xD8 }, // X87_FISTP32_R XXX not valid???
	{ 0xDB, 0x18 }, // X87_FISTP32_M
	{ 0xD9, 0x18 }, // X87_FSTP32_M ,
	{ 0xDD, 0x18 }, // X87_FSTP64_M ,

	{ 0xDA, 0xD8 }, // X87_FICOMP32_R ,
	{ 0xDA, 0xC0 }, // X87_FIADD32_R
	{ 0xDA, 0xE0 }, // X87_FISUB32_R
	{ 0xDA, 0xC8 }, // X87_FIMUL32_R
	{ 0xDA, 0xF0 }, // X87_FIDIV32_R

	{ 0xDA, 0x18 }, // X87_FICOMP32_M
	{ 0xD8, 0x10 }, // X87_FCOM32_M
	{ 0xD8, 0x18 }, // X87_FCOMP32_M
	{ 0xDC, 0x10 }, // X87_FCOM64_M
	{ 0xDC, 0x18 }, // X87_FCOMP64_M
	{ 0xDA, 0x00 }, // X87_FIADD32_M
	{ 0xDA, 0x20 }, // X87_FISUB32_M
	{ 0xDA, 0x08 }, // X87_FIMUL32_M
	{ 0xDA, 0x30 }, // X87_FIDIV32_M
	{ 0xD8, 0x00 }, // X87_FADD32_M
	{ 0xD8, 0x20 }, // X87_FSUB32_M
	{ 0xD8, 0x08 }, // X87_FMUL32_M
	{ 0xD8, 0x30 }, // X87_FDIV32_M
	{ 0xDC, 0x00 }, // X87_FADD64_M
	{ 0xDC, 0x20 }, // X87_FSUB64_M
	{ 0xDC, 0x08 }, // X87_FMUL64_M
	{ 0xDC, 0x30 }, // X87_FDIV64_M

	{ 0xDA, 0x00 }, // X87_FIADDR32_M
	{ 0xDA, 0x28 }, // X87_FISUBR32_M
	{ 0xDA, 0x08 }, // X87_FIMULR32_M
	{ 0xDA, 0x38 }, // X87_FIDIVR32_M
	{ 0xD8, 0x00 }, // X87_FADDR32_M
	{ 0xD8, 0x28 }, // X87_FSUBR32_M
	{ 0xD8, 0x08 }, // X87_FMULR32_M
	{ 0xD8, 0x38 }, // X87_FDIVR32_M
	{ 0xDC, 0x00 }, // X87_FADDR64_M
	{ 0xDC, 0x28 }, // X87_FSUBR64_M
	{ 0xDC, 0x08 }, // X87_FMULR64_M
	{ 0xDC, 0x38 }, // X87_FDIVR64_M

	{ 0xD8, 0xD0 }, // X87_FCOM_ST
	{ 0xD8, 0xD8 }, // X87_FCOMP_ST
	{ 0xD8, 0xC0 }, // X87_FADD_ST0
	{ 0xD8, 0xE0 }, // X87_FSUB_ST0
	{ 0xD8, 0xC8 }, // X87_FMUL_ST0
	{ 0xD8, 0xF0 }, // X87_FDIV_ST0
	{ 0xDC, 0xC0 }, // X87_FADD_STI
	{ 0xDC, 0xE0 }, // X87_FSUB_STI
	{ 0xDC, 0xC8 }, // X87_FMUL_STI
	{ 0xDC, 0xF0 }, // X87_FDIV_STI
	{ 0xD8, 0xC0 }, // X87_FADDR_ST0
	{ 0xD8, 0xE8 }, // X87_FSUBR_ST0
	{ 0xD8, 0xC8 }, // X87_FMULR_ST0
	{ 0xD8, 0xF8 }, // X87_FDIVR_ST0
	{ 0xDC, 0xC0 }, // X87_FADDR_STI
	{ 0xDC, 0xE8 }, // X87_FSUBR_STI
	{ 0xDC, 0xC8 }, // X87_FMULR_STI
	{ 0xDC, 0xF8 }, // X87_FDIVR_STI
	{ 0xDD, 0xF0 }, // X87_FCOMI
};
static const uchar_t sseCode[][4] =
{
	{ 0xF3, 0x0F, 0x10, 0x00 }, // MOVSS_RM
	// See: Section 3.2 of 
	// Intel® 64 and IA-32 Architectures 
	// Software Developer's Manual, Volume 2A.
	// The encoding listed in the Table B-26 of Volume 2B
	// seem to have reverced the encoding of MOVSD_RM and MOVSD_MR???
	{ 0xF2, 0x0F, 0x10, 0x00 }, // MOVSD_RM
	{ 0xF2, 0x0F, 0x11, 0x00 }, // MOVSD_MR
	{ 0xF2, 0x0F, 0x10, 0xC0 }, // MOVSD_RR
	{ 0x0F, 0x2F, 0xC0, 0 }, // UCOMISS_RR
};

/**/
DaoJitCode* DaoJitCode_New()
{
	DaoJitCode *self = (DaoJitCode*) dao_malloc( sizeof(DaoJitCode) );
	self->opcode = self->rmsti = self->modsib = self->extra = 0;
	self->vmcID = -1;
	return self;
}
void DaoJitCode_Print( DaoJitCode *self )
{
	printf( "%-12s:  %6X,  %6X,  %6li,%3li;  %6i\n", instrNames[self->opcode], 
			self->rmsti, self->modsib, self->extra, 
			(self->extra-disp_value_v)/sizeof_value, self->vmcID );
}

typedef struct DaoJIT  DaoJIT;
typedef struct X87StackData  X87StackData;

struct X87StackData
{
	int   operand; /* operand variable associated with the register */
	int   store; /* result variable associated with the register */
	int   index; /* store and pop data to memory in the order of index */
	short map_oper; /* operand association is valid */
	short map_store; /* result association is valid */
};


struct DaoJIT
{
	short x87Top;
	int   index;
	X87StackData x87Stack[ X87_STACK_SIZE ];

	DArray *jitCodes; /* <DaoJitCode*> */
	DaoJitCode *jc;

	short parLocal;
	short parExtra;

	int nlabel;
	DMap *labelAddress; /* <int,int> */
	DMap *labelExtraMap; /* <int,int> */
	DArray *labelUsage; /* <int> */
	DArray *labelExtra; /* <int> */

	DString *gotoDest;

	DaoRoutine *routine;
};

static DaoJIT* DaoJIT_New()
{
	DaoJIT *self = dao_malloc( sizeof(DaoJIT) );
	self->x87Top = -1;
	self->index = 0;
	self->jitCodes = DArray_New(D_JITCODE);
	self->jc = DaoJitCode_New();
	self->nlabel = 0;
	self->labelAddress = DMap_New(0,0);
	self->labelExtraMap = DMap_New(0,0);
	self->labelUsage = DArray_New(0);
	self->labelExtra = DArray_New(0);
	self->routine = NULL;
	self->parLocal = self->parExtra = -1;
	return self;
}
static void DaoJIT_Delete( DaoJIT *self )
{
	DArray_Delete( self->jitCodes );
	DArray_Delete( self->labelUsage );
	DArray_Delete( self->labelExtra );
	DMap_Delete( self->labelAddress );
	DMap_Delete( self->labelExtraMap );
	dao_free( self->jc );
	dao_free( self );
}
static void DaoJIT_PrintX87Stack( DaoJIT *self )
{
	int i;
	for(i=0; i<=self->x87Top; i++){
		X87StackData it = self->x87Stack[i];
		printf( "op = %3i (%i), store = %3i (%i), index = %4i\n",
				it.operand, it.map_oper, it.store, it.map_store, it.index);
	}
}

#if 0
static void Test( int i )
{
	printf( "printed by Test(): %i %i\n", i, Test );
}

static double pow2( double x,double y )
{
	//printf( "pow: %f %f\n", x, y );
	printf( "pow: %i %i\n", (int)x, (int)y );
	return pow( x, y );
}
#endif

#define DaoJIT_X87Pop( self )  if( self->x87Top >=0 ) self->x87Top --;

static void DaoJIT_X87Push( DaoJIT *self, int op, int res )
{
	if( self->x87Top < -1 ) self->x87Top = -1;
	self->index ++;
	self->x87Top ++;
	self->x87Stack[ self->x87Top ].operand = op;
	self->x87Stack[ self->x87Top ].store = res;
	self->x87Stack[ self->x87Top ].map_oper = 1;
	self->x87Stack[ self->x87Top ].map_store = 0;
	self->x87Stack[ self->x87Top ].index = self->index;
}

static int DaoJIT_FindX87Stacked( DaoJIT *self, int opi )
{
	int i, index, max =-1, imax =-1;
	for(i=self->x87Top; i >=0; i--){
		index = self->x87Stack[i].index;
		if( self->x87Stack[i].map_store && self->x87Stack[i].store == opi ){
			if( index > max ){
				max = index;
				imax = i;
			}
		}
		if( self->x87Stack[i].map_oper && self->x87Stack[i].operand == opi ){
			if( index > max ){
				max = index;
				imax = i;
			}
		}
	}
	if( imax >=0 ) return self->x87Top - imax;
	return -1;
}
/* mov32 12(%ebp), %eax;
   DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EBP, DIS_B1, 12 );

   mov32 id(%eax), %eax;
   DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EAX, DIS_B4, id );

   mov32 %eax, id(%ebx); 
   the bits encoding register is always before the bits encoding memory!!!
   DaoJIT_AddCode( self, X86_MOV32_MR, EAX3|EBX, DIS_B4, id );
 */
static void DaoJIT_AddCode( DaoJIT *self, int code, int rmsti, int modsib, dint extra )
{
	self->jc->opcode = code;
	self->jc->rmsti = rmsti;
	self->jc->modsib = modsib;
	self->jc->extra = extra;
	if( code == X87_FLD_ST || code == X87_FLD1 || code == X87_FLDZ )
		DaoJIT_X87Push( self, -1, extra );
	DArray_Append( self->jitCodes, self->jc );
}
/* fld32 %eax
 * DaoJIT_X87Load( self, X87_FLD32, 0, -1, EAX );
 *
 * fld32 12(%eax)
 * DaoJIT_X87Load( self, X87_FLD32_MD8, 12, -1, EAX );
 *
 * if put >= 0: generate code to store the FPU register to the desired 
 * memory location, by calling DaoJIT_AdjustStack( reg ):
 * mov32  12(%ebp), %reg;
 * fstp32 put(%reg);
 * DaoJIT_AddCode( self, X86_MOV32_RM, (reg<<3) | EBP , DIS_B1, 12 );
 * DaoJIT_AddCode( self, X87_FSTP32_M, reg , DIS_B1, OpValue(put) );
 */
static void DaoJIT_X87Load( DaoJIT *self, int code, int rmsti, int modsib, int get, int put )
{
	self->jc->opcode = code;
	self->jc->rmsti = rmsti;
	self->jc->modsib = modsib;
	self->jc->extra = OpValue( get );
	DaoJIT_X87Push( self, get, put );
	DArray_Append( self->jitCodes, self->jc );
}
static void DaoJIT_X87Load2( DaoJIT *self, int get, int put, int reg )
{
	int t = 0;
	DaoType *abtp = NULL;
	if( self->routine ){
		abtp = self->routine->regType->items.pType[get];
		if( abtp ) t = abtp->tid;
	}
	if( t >= DAO_INTEGER && t <= DAO_DOUBLE )
		DaoJIT_X87Load( self, X87_FILD32_M + (t-DAO_INTEGER), reg, DIS_B4, get, put );
}
static void DaoJIT_X87Load3( DaoJIT *self, int code, int rmsti, int modsib, int disp, int put )
{
	self->jc->opcode = code;
	self->jc->rmsti = rmsti;
	self->jc->modsib = modsib;
	self->jc->extra = disp;
	DaoJIT_X87Push( self, -1, put );
	DArray_Append( self->jitCodes, self->jc );
}
static void DaoJIT_X87LoadNonLocal( DaoJIT *self, int code, 
		int rmsti, int modsib, int get, int put )
{
	self->jc->opcode = code;
	self->jc->rmsti = rmsti;
	self->jc->modsib = modsib;
	self->jc->extra = OpValue( get );
	DaoJIT_X87Push( self, -1, put );
	DArray_Append( self->jitCodes, self->jc );
}
static void DaoJIT_X87Swap2Top( DaoJIT *self, int st )
{
	int top = self->x87Top;
	X87StackData tmp = self->x87Stack[top];
	if( st <= 0 || st > top ) return;
	self->x87Stack[top] = self->x87Stack[top-st];
	self->x87Stack[top-st] = tmp;
	DaoJIT_AddCode( self, X87_FXCH_ST, st, 0, 0 );
}
static void DaoJIT_X87StoreInt32( DaoJIT *self, int opa, int reg )
{
	int sta;
	DaoJIT_AddCode( self, X87_FIST32_M, self->parLocal, DIS_B4, OpValue( opa ) );
	DaoJIT_AddCode( self, X87_FISTP32_R, reg, 0, 0 );
	sta = DaoJIT_FindX87Stacked( self, opa );
	while( sta >=0 ){
		DaoJIT_X87Swap2Top( self, sta );
		DaoJIT_AddCode( self, X87_FFREE_ST, 0, 0, 0 );
		DaoJIT_AddCode( self, X87_FINCSTP, 0, 0, 0 );
		self->x87Top --;
		sta = DaoJIT_FindX87Stacked( self, opa );
	}
}
static void DaoJIT_StoreX87Stack( DaoJIT *self );
static int DaoJIT_Encode( DaoJIT *self, DaoVmcArray *vmCodes, int id, int m, int m2 );

static void DaoJIT_X87StoreAndPop( DaoJIT *self )
{
	//printf( "DaoJIT_X87StoreAndPop()\n" );
	int j, tp, mem, index;
	DaoType *abtp = NULL;
	if( self->x87Top <0 ) return;
	mem = self->x87Stack[self->x87Top].store;
	index = self->x87Stack[self->x87Top].index;
#if 1
	if( mem >=0 ){
		for(j=0; j<self->x87Top; j++){
			if( self->x87Stack[j].store == mem ){
				if( self->x87Stack[j].index < index )
					self->x87Stack[j].store = -1;
				else{
					DaoJIT_AddCode( self, X87_FFREE_ST, 0, 0, 0 );
					DaoJIT_AddCode( self, X87_FINCSTP, 0, 0, 0 );
					self->x87Top --;
					return;
				}
			}
		}
	}
#endif
	tp = 0;
	mem = self->x87Stack[self->x87Top].store;
	if( mem >=0 && self->routine ){
		abtp = self->routine->regType->items.pType[mem];
		if( abtp ) tp = abtp->tid;
	}
	if( tp >= DAO_INTEGER && tp <= DAO_DOUBLE ){
		mem = OpValue( mem );
		DaoJIT_AddCode( self, X87_FISTP32_M+(tp-DAO_INTEGER), EDX, DIS_B4, mem );
		self->x87Top --;
	}else{
		DaoJIT_AddCode( self, X87_FFREE_ST, 0, 0, 0 );
		DaoJIT_AddCode( self, X87_FINCSTP, 0, 0, 0 );
		self->x87Top --;
	}
}

static void DaoJIT_AdjustStack( DaoJIT *self, int n )
{
	//DaoJIT_PrintX87Stack( self );
	//printf( "DaoJIT_AdjustStack()\n" );
	int i, j, index, imin=-1, min=self->index+1;
	int clear = (n==self->x87Top+1);
	for(j=0; j<n; j++){
		min = self->index + 1;
		imin = -1;
		for(i=0; i<=self->x87Top; i++){
			index = self->x87Stack[i].index;
			if( index < min && self->x87Stack[i].store >=0 ){
				min = index;
				imin = self->x87Top - i;
			}
		}
		//printf( "%i %i %i %i %i\n", self->index, imin, imin2, min, self->x87Top );
		if( clear && imin <0 && self->x87Top >1 ){
			self->x87Top = -1;
			DaoJIT_AddCode( self, X87_FNINIT, 0, 0, 0 );
			return;
		}
		if( imin >0 ) DaoJIT_X87Swap2Top( self, imin );
		DaoJIT_X87StoreAndPop( self );
	}
	//DaoJIT_PrintX87Stack( self );
}

extern void DaoJIT_DoMove( DaoContext *self, int id );
extern void DaoJIT_MovePP( DaoContext *self, int id );
extern void DaoJIT_DoBinArith( DaoContext *self, int id );
extern void DaoJIT_MoveString( DaoContext *self, int id );
extern void DaoJIT_AddString( DaoContext *self, int id );
extern void DaoJIT_GETI_LI( DaoContext *self, int id );
extern void DaoJIT_SETI_LI( DaoContext *self, int id );
extern void DaoJIT_GETI_LSI( DaoContext *self, int id );
extern void DaoJIT_SETI_LSIS( DaoContext *self, int id );

static void* jitCalls[] =
{
#if 0
	DVM_GETC , 
	DVM_GETV , 
	DVM_GETI ,  
	DVM_GETF ,  
	DVM_SETV , 
	DVM_SETI , 
	DVM_SETF , 
	DVM_LOAD ,
	DVM_CAST , 
#else
	0,0,0,0,0,0,0,0,0,
#endif
	DaoJIT_DoMove , 
#if 0
	DVM_NOT ,  
	DVM_UNMS , 
	DVM_BITREV , 
#else
	0,0,0,
#endif
	DaoJIT_DoBinArith ,  
	DaoJIT_DoBinArith ,  
	DaoJIT_DoBinArith ,  
	DaoJIT_DoBinArith ,  
	DaoJIT_DoBinArith ,  
	DaoJIT_DoBinArith ,  
#if 0
	DVM_AND ,  
	DVM_OR ,   
	DVM_LT ,   
	DVM_LE ,   
	DVM_EQ ,   
	DVM_NE ,   
	DVM_BITAND , 
	DVM_BITOR ,  
	DVM_BITXOR ,  
	DVM_BITLFT , 
	DVM_BITRIT , 
	DVM_PAIR , 
	DVM_TUPLE , 
	DVM_LIST , 
	DVM_MAP , 
	DVM_ARRAY , 
	DVM_MATRIX , 
	DVM_CURRY , 
	DVM_MCURRY , 
	DVM_GOTO , 
	DVM_SWITCH , 
	DVM_ITER , 
	DVM_TEST , 
	DVM_CALL , 
	DVM_MCALL , 
	DVM_CLOSE , 
	DVM_CRRE , 
	DVM_JITC , 
	DVM_RETURN , 
	DVM_SECT ,   
#endif
};

static void DaoJIT_AddCheckRange( DaoJIT *self, int index, int size )
{
	DaoJIT_AddCode( self, X86_CMP32_RR, (size<<3)|index, 0, 0 );
	DaoJIT_AddCode( self, X86_JLE8, 0, 0, self->nlabel );
	DaoJIT_AddCode( self, X86_CMP32_RD8, index, 0, 0 );
	DaoJIT_AddCode( self, X86_JL8, 0, 0, self->nlabel );
	DaoJIT_AddCode( self, X86_JMP8, 0, 0, self->nlabel+1 );

	DaoJIT_AddCode( self, X86_LABEL, 0, 0, self->nlabel );
	DaoJIT_AddCode( self, X86_MOV32_RD, EAX, 0, self->jc->vmcID );
	DaoJIT_AddCode( self, X86_JMP32, 0, 0, LAB_RETURN );
	DaoJIT_AddCode( self, X86_LABEL, 0, 0, self->nlabel+1 );
	self->nlabel += 2;
}

static void DaoJIT_GetParLocal( DaoJIT *self )
{
#ifdef DAO_64BITS
	DaoJIT_AddCode( self, X86_MOV64_RR8, EDX3, 0,0 );
#else
	DaoJIT_AddCode( self, X86_MOV32_RM, EDX3|EBP, DIS_B1, 2*ADDR_BYTES );
#endif
}
static short DaoJIT_GetParExtra( DaoJIT *self )
{
	if( self->parExtra >=0 ) return EDI;
	DaoJIT_AddCode( self, X86_PUSH_R, EDI, 0,0 );
	DaoJIT_AddCode( self, X86_MOV32_RM, EDI3|EBP, DIS_B1, 3*ADDR_BYTES );
	self->parExtra = EDI;
	return EDI;
}

/* encode one VM code */
static int DaoJIT_Encode( DaoJIT *self, DaoVmcArray *vmCodes, int id, int min, int max )
{
	DaoVmCode *vmc = vmCodes->codes + id;
	DaoVmCode *vmc2;
	DaoType **type = self->routine->regType->items.pType;
	DNode *node;
	ushort_t code = vmc->code;
	ushort_t opa = vmc->a;
	ushort_t opb = vmc->b;
	ushort_t opc = vmc->c;
	ushort_t opa2, opb2;
	int count = self->routine->regType->size;
	int sta, stb, stc, diff, ta=0, tb=0, tc=0;
	int i, c, d, lab;
	dint fptr = 0;
	short extra;
	short movaddr = ADDR_BYTES == 4 ? X86_MOV32_RD : X86_MOV64_RD;
	short bcall = 0, acall = 0;

	//printf( "%i %i, ", id, self->gotoDest->mbs[id] );
	if( self->gotoDest->mbs[id] ) DaoJIT_StoreX87Stack( self );

	if( id > min ){
		code = vmCodes->codes[id-1].code;
		if( (code <= DVM_POW && jitCalls[ code ]) 
				|| code == DVM_GETI_LI || code == DVM_GETI_LSI
				|| code == DVM_SETI_LI || code == DVM_SETI_LSIS
				|| code == DVM_MOVE_SS || code == DVM_MOVE_PP 
				|| code == DVM_ADD_SS ) bcall = 1;
	}
	if( id+1 <= max ){
		code = vmCodes->codes[id+1].code;
		if( (code <= DVM_POW && jitCalls[ code ]) 
				|| code == DVM_GETI_LI || code == DVM_GETI_LSI
				|| code == DVM_SETI_LI || code == DVM_SETI_LSIS
				|| code == DVM_MOVE_SS || code == DVM_MOVE_PP 
				|| code == DVM_ADD_SS ) acall = 1;
	}
	code = vmc->code;
	if( id+1 < max ){
		vmc2 = vmCodes->codes + (id+1);
		opa2 = vmc2->a;
		opb2 = vmc2->b;
	}
	//DaoVmCode_Print( *vmc, NULL );
	if( opa < count && type[opa] ) ta = type[opa]->tid;
	if( opb < count && type[opb] ) tb = type[opb]->tid;
	if( opc < count && type[opc] ) tc = type[opc]->tid;
	for(i=0; i<=self->x87Top; i++){
		if( self->x87Stack[i].store >=0 && self->x87Stack[i].map_store ==0 ){
			self->x87Stack[i].map_oper = 0;
			self->x87Stack[i].map_store = 1;
		}
	}

	//printf( "encoding: %i\n", id );

	self->jc->vmcID = id;
	switch( code ){
	case DVM_TEST_I :
		sta = DaoJIT_FindX87Stacked( self, opa );
		if( sta >=0 ) DaoJIT_StoreX87Stack( self );
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EDX, DIS_B4, OpValue(opa) );
		DaoJIT_AddCode( self, X86_CMP32_RD8, EAX, 0, 0 );
		DaoJIT_StoreX87Stack( self );
		DaoJIT_AddCode( self, X86_JE32, 0, 0, self->nlabel );
		DMap_Insert( self->labelExtraMap, (void*)(size_t)opb, (void*)(size_t)self->nlabel );
		self->nlabel ++;
		break;
	case DVM_TEST_F :
	case DVM_TEST_D :
		sta = DaoJIT_FindX87Stacked( self, opa );
		if( sta !=0 && self->x87Top+1 == X87_STACK_SIZE )
			DaoJIT_AdjustStack( self, 2 );
		if( sta >0 ){
			DaoJIT_AddCode( self, X87_FLD_ST, sta, 0, -1 );
		}else if( sta <0 ){
			if( code == DVM_TEST_F )
				DaoJIT_X87Load( self, X87_FLD32_M, EDX, DIS_B4, opa, -1 );
			else
				DaoJIT_X87Load( self, X87_FLD64_M, EDX, DIS_B4, opa, -1 );
		}
		DaoJIT_AddCode( self, X87_FTST, 0, 0, 0 );
		DaoJIT_AddCode( self, X87_FNSTSW_AX, 0, 0, 0 );
		DaoJIT_AddCode( self, X86_SAHF, 0, 0,0  );
		DaoJIT_StoreX87Stack( self );
		DaoJIT_AddCode( self, X86_JE32, 0, 0, self->nlabel );
		DMap_Insert( self->labelExtraMap, (void*) (size_t)opb, (void*)(size_t)self->nlabel );
		self->nlabel ++;
		break;
	case DVM_GOTO :
		DaoJIT_StoreX87Stack( self );
		DaoJIT_AddCode( self, X86_JMP32, 0, 0, self->nlabel );
		DArray_Append( self->labelExtra, (void*) (size_t)self->nlabel );
		DArray_Append( self->labelExtra, (void*) (size_t)opb );
		self->nlabel ++;
		break;
	case DVM_GETV_I : case DVM_GETV_F : case DVM_GETV_D :
		DaoJIT_PrintX87Stack( self );
		extra = DaoJIT_GetParExtra( self );
		if( opa == DAO_G )
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|extra, DIS_B1, 1*ADDR_BYTES );
		else if( opa == DAO_K )
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|extra, DIS_B1, 2*ADDR_BYTES );
		else
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|extra, DIS_B1, 3*ADDR_BYTES );

		switch( code ){
		case DVM_GETV_I : 
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EAX, DIS_B4, OpValue(opb) );
			DaoJIT_AddCode( self, X86_MOV32_MR, EAX3|EDX, DIS_B4, OpValue(opc) );
			break;
		case DVM_GETV_F : 
			if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
			DaoJIT_X87LoadNonLocal( self, X87_FLD32_M, EAX, DIS_B4, opb, opc );
			break;
		default :
			if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
			DaoJIT_X87LoadNonLocal( self, X87_FLD64_M, EAX, DIS_B4, opb, opc );
			break;
		}
		break;
	case DVM_SETV_II : case DVM_SETV_IF : case DVM_SETV_ID :
	case DVM_SETV_FI : case DVM_SETV_FF : case DVM_SETV_FD :
	case DVM_SETV_DI : case DVM_SETV_DF : case DVM_SETV_DD :
		extra = DaoJIT_GetParExtra( self );
		if( opc == DAO_G ){
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|extra, DIS_B1, 1*ADDR_BYTES );
		}else if( opc == DAO_K ){
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|extra, DIS_B1, 2*ADDR_BYTES );
		}else{
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|extra, DIS_B1, 3*ADDR_BYTES );
		}
		if( code == DVM_SETV_II ){
			DaoJIT_AddCode( self, X86_MOV32_RM, EBX3|EDX, DIS_B4, OpValue(opa) );
			DaoJIT_AddCode( self, X86_MOV32_MR, EBX3|EAX, DIS_B4, OpValue(opb) );
		}else{
			sta = DaoJIT_FindX87Stacked( self, opa );
			if( sta >=0 ){
				DaoJIT_X87Swap2Top( self, sta );
			}else{
				if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
				if( code == DVM_SETV_FI || code == DVM_SETV_DI ){
					DaoJIT_X87Load( self, X87_FILD32_M, EDX, DIS_B4, opa, -1 );
				}else if( code==DVM_SETV_IF || code==DVM_SETV_FF || code==DVM_SETV_DF ){
					DaoJIT_X87Load( self, X87_FLD32_M, EDX, DIS_B4, opa, -1 );
				}else{
					DaoJIT_X87Load( self, X87_FLD64_M, EDX, DIS_B4, opa, -1 );
				}
			}
			if( code <= DVM_SETV_ID ){
				DaoJIT_AddCode( self, X87_FIST32_M, EAX, DIS_B4, OpValue( opb ) );
			}else if( code <= DVM_SETV_FD ){
				DaoJIT_AddCode( self, X87_FST32_M, EAX, DIS_B4, OpValue( opb ) );
			}else{
				DaoJIT_AddCode( self, X87_FST64_M, EAX, DIS_B4, OpValue( opb ) );
			}
		}
		break;
	case DVM_MOVE_II :
		sta = DaoJIT_FindX87Stacked( self, opa );
		stc = DaoJIT_FindX87Stacked( self, opc );
		if( sta >=0 || stc >=0 ) DaoJIT_StoreX87Stack( self );
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EDX, DIS_B4, OpValue(opa) );
		DaoJIT_AddCode( self, X86_MOV32_MR, EAX3|EDX, DIS_B4, OpValue(opc) );
		break;
	case DVM_MOVE_FI :
	case DVM_MOVE_DI :
		if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
		sta = DaoJIT_FindX87Stacked( self, opa );
		if( sta >=0 ){
			DaoJIT_AddCode( self, X87_FLD_ST, sta, 0, opc );
		}else{
			DaoJIT_X87Load( self, X87_FILD32_M, EDX, DIS_B4, opa, opc );
			self->x87Stack[self->x87Top].map_store = 1;
		}
		break;
	case DVM_MOVE_IF :
	case DVM_MOVE_FF :
	case DVM_MOVE_DF :
		if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
		sta = DaoJIT_FindX87Stacked( self, opa );
		if( sta >=0 ){
			DaoJIT_AddCode( self, X87_FLD_ST, sta, 0, opc );
		}else{
			DaoJIT_X87Load( self, X87_FLD32_M, EDX, DIS_B4, opa, opc );
			self->x87Stack[self->x87Top].map_store = 1;
		}
		/* to be sure floating value is converted to integer for
		 * the possible usage of opc in the next instructions */
		if( code == DVM_MOVE_IF ) DaoJIT_StoreX87Stack( self );
		break;
	case DVM_MOVE_ID :
	case DVM_MOVE_FD :
	case DVM_MOVE_DD :
		if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
		sta = DaoJIT_FindX87Stacked( self, opa );
		if( sta >=0 ){
			DaoJIT_AddCode( self, X87_FLD_ST, sta, 0, opc );
		}else{
			DaoJIT_X87Load( self, X87_FLD64_M, EDX, DIS_B4, opa, opc );
			self->x87Stack[self->x87Top].map_store = 1;
		}
		if( code == DVM_MOVE_ID ) DaoJIT_StoreX87Stack( self );
		break;
	case DVM_ADD_III :
	case DVM_SUB_III :
	case DVM_MUL_III :
	case DVM_DIV_III :
		sta = DaoJIT_FindX87Stacked( self, opa );
		if( sta >=0 ){
			DaoJIT_X87Swap2Top( self, sta );
			DaoJIT_X87StoreInt32( self, opa, EAX );
		}else{
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EDX, DIS_B4, OpValue(opa) );
		}
		if( opa == opb ){
			DaoJIT_AddCode( self, X86_ADD32_RR + code - DVM_ADD_III, EAX3|EAX, 0,0 );
		}else{
			stb = DaoJIT_FindX87Stacked( self, opb );
			if( stb >=0 ){
				DaoJIT_X87Swap2Top( self, stb );
				DaoJIT_X87StoreInt32( self, opb, EBX );
			}else{
				DaoJIT_AddCode( self, X86_MOV32_RM, EBX3|EDX, DIS_B4, OpValue(opb)  );
			}
			if( code >= DVM_MUL_III ){
				if( ADDR_BYTES ==8 ) DaoJIT_AddCode( self, X86_MOV64_R8R, EDX, 0,0 );
				DaoJIT_AddCode( self, movaddr, EDX, 0,0 );
			}
			DaoJIT_AddCode( self, X86_ADD32_RR + code - DVM_ADD_III, EAX3|EBX, 0,0 );
		}
		if( code >= DVM_MUL_III ) DaoJIT_GetParLocal( self );
		DaoJIT_AddCode( self, X86_MOV32_MR, EAX3|EDX, DIS_B4, OpValue(opc) );
		break;
	case DVM_AND_III :
	case DVM_OR_III :
		sta = DaoJIT_FindX87Stacked( self, opa );
		if( sta >=0 ){
			DaoJIT_X87Swap2Top( self, sta );
			DaoJIT_X87StoreInt32( self, opa, EAX );
		}else{
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EDX, DIS_B4, OpValue(opa) );
		}
		if( opa == opb ){
			DaoJIT_AddCode( self, X86_MOV32_MR, EAX3|EDX, DIS_B4, OpValue(opc) );
		}else{
			stb = DaoJIT_FindX87Stacked( self, opb );
			if( stb >=0 ){
				DaoJIT_X87Swap2Top( self, stb );
				DaoJIT_X87StoreInt32( self, opb, EBX );
			}else{
				DaoJIT_AddCode( self, X86_MOV32_RM, EBX3|EDX, DIS_B4, OpValue(opb) );
			}
			DaoJIT_AddCode( self, X86_CMP32_RD8, EAX, 0, 0 );
			DaoJIT_AddCode( self, X86_JE8, 0,0, self->nlabel );
			if( code == DVM_AND_III )
				DaoJIT_AddCode( self, X86_MOV32_MR, EBX3|EDX, DIS_B4, OpValue(opc) );
			else
				DaoJIT_AddCode( self, X86_MOV32_MR, EAX3|EDX, DIS_B4, OpValue(opc) );
			DaoJIT_AddCode( self, X86_JMP8, 0,0, self->nlabel+1 );

			DaoJIT_AddCode( self, X86_LABEL, 0,0, self->nlabel );
			if( code == DVM_AND_III )
				DaoJIT_AddCode( self, X86_MOV32_MR, EAX3|EDX, DIS_B4, OpValue(opc) );
			else
				DaoJIT_AddCode( self, X86_MOV32_MR, EBX3|EDX, DIS_B4, OpValue(opc) );
			DaoJIT_AddCode( self, X86_LABEL, 0,0, self->nlabel+1 );
			self->nlabel += 2;
		}
		break;
	case DVM_LT_III :
	case DVM_LE_III :
	case DVM_EQ_III :
	case DVM_NE_III :
		sta = DaoJIT_FindX87Stacked( self, opa );
		if( sta >=0 ){
			DaoJIT_X87Swap2Top( self, sta );
			DaoJIT_X87StoreInt32( self, opa, EAX );
		}else{
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EDX, DIS_B4, OpValue(opa) );
		}
		if( id +1 < max && vmc2->code == DVM_TEST_I && opa2 == opc ){
			switch( code ){
			case DVM_LT_III : c = X86_JAE32; break;
			case DVM_LE_III : c = X86_JA32; break;
			case DVM_EQ_III : c = X86_JNE32; break;
			case DVM_NE_III : c = X86_JE32; break;
			default : break;
			}
			if( opa == opb ){
				DaoJIT_AddCode( self, X86_CMP32_RR, EAX3|EAX, 0, 0 );
				DaoJIT_StoreX87Stack( self );
				DaoJIT_AddCode( self, c, 0, 0, self->nlabel );
			}else{
				stb = DaoJIT_FindX87Stacked( self, opb );
				if( stb >=0 ){
					DaoJIT_X87Swap2Top( self, stb );
					DaoJIT_X87StoreInt32( self, opb, EBX );
				}else{
					DaoJIT_AddCode( self, X86_MOV32_RM, EBX3|EDX, DIS_B4, OpValue(opb) );
				}
				DaoJIT_AddCode( self, X86_CMP32_RR, EAX3|EBX, 0, 0 );
				DaoJIT_StoreX87Stack( self );
				DaoJIT_AddCode( self, c, 0,0, self->nlabel );
			}
			DMap_Insert( self->labelExtraMap, (void*) (dint)opb2, (void*) (dint)self->nlabel );
			self->nlabel ++;
			return 2;
		}
		if( opa == opb ){
			DaoJIT_AddCode( self, X86_CMP32_RR, EAX3|EAX, 0, 0 );
			DaoJIT_AddCode( self, X86_JL8 + code - DVM_LT_III, 0,0, self->nlabel );
		}else{
			stb = DaoJIT_FindX87Stacked( self, opb );
			if( stb >=0 ){
				DaoJIT_X87Swap2Top( self, stb );
				DaoJIT_X87StoreInt32( self, opb, EBX );
			}else{
				DaoJIT_AddCode( self, X86_MOV32_RM, EBX3|EDX, DIS_B4, OpValue(opb) );
			}
			DaoJIT_AddCode( self, X86_CMP32_RR, EAX3|EBX, 0, 0 );
			DaoJIT_AddCode( self, X86_JL8 + code - DVM_LT_III, 0,0, self->nlabel );
		}
		DaoJIT_AddCode( self, X86_MOV8_RD, EAX3, 0, 0 );
		DaoJIT_AddCode( self, X86_JMP8, 0,0, self->nlabel+1 );
		DaoJIT_AddCode( self, X86_LABEL, 0,0, self->nlabel );
		DaoJIT_AddCode( self, X86_MOV8_RD, EAX3, 0, 3 );
		DaoJIT_AddCode( self, X86_LABEL, 0,0, self->nlabel+1 );
		DaoJIT_AddCode( self, X86_MOV32_MR, EAX3|EDX, DIS_B4, OpValue(opc) );
		self->nlabel += 2;
		break;
	case DVM_ADD_FFF : case DVM_SUB_FFF : case DVM_MUL_FFF : case DVM_DIV_FFF :
	case DVM_ADD_DDD : case DVM_SUB_DDD : case DVM_MUL_DDD : case DVM_DIV_DDD :
	case DVM_ADD_FNN : case DVM_SUB_FNN : case DVM_MUL_FNN : case DVM_DIV_FNN :
	case DVM_ADD_DNN : case DVM_SUB_DNN : case DVM_MUL_DNN : case DVM_DIV_DNN :
		diff = code - DVM_ADD_DNN;
		if( code <= DVM_DIV_FFF ) diff = code - DVM_ADD_FFF;
		else if( code <= DVM_DIV_DDD ) diff = code - DVM_ADD_DDD;
		else if( code <= DVM_DIV_FNN ) diff = code - DVM_ADD_FNN;

		sta = DaoJIT_FindX87Stacked( self, opa );
		stb = DaoJIT_FindX87Stacked( self, opb );
		stc = DaoJIT_FindX87Stacked( self, opc );
		//printf( "%i %i %i; %i\n", sta, stb, stc, self->x87Top+1 );
		if( opa == opc ){
			stc = DaoJIT_FindX87Stacked( self, opc );
			if( stc >=0 ){
				if( stc ) DaoJIT_X87Swap2Top( self, stc );
				//if( self->x87Stack[self->x87Top].store != opc )
				//  printf( "store mis-match\n" );
				self->x87Stack[self->x87Top].store = opc;
			}else{
				if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
				DaoJIT_X87Load2( self, opc, opc, EDX );
			}
			stb = DaoJIT_FindX87Stacked( self, opb );
			if( stb >=0 ){
				DaoJIT_AddCode( self, X87_FADD_ST0 + diff, stb, 0, 0 );
			}else if( tb == DAO_INTEGER ){
				DaoJIT_AddCode( self, X87_FIADD32_M +diff, EDX, DIS_B4, OpValue(opb) );
			}else if( tb == DAO_FLOAT ){
				DaoJIT_AddCode( self, X87_FADD32_M +diff, EDX, DIS_B4, OpValue(opb) );
			}else{
				DaoJIT_AddCode( self, X87_FADD64_M +diff, EDX, DIS_B4, OpValue(opb) );
			}
		}else if( opb == opc ){
			stc = DaoJIT_FindX87Stacked( self, opc );
			if( stc >=0 ){
				if( stc >0 ) DaoJIT_X87Swap2Top( self, stc );
				//if( self->x87Stack[self->x87Top].store != opc )
				//  printf( "store mis-match\n" );
				self->x87Stack[self->x87Top].store = opc;
			}else{
				if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
				DaoJIT_X87Load2( self, opc, opc, EDX );
			}
			sta = DaoJIT_FindX87Stacked( self, opa );
			if( sta >=0 ){
				DaoJIT_AddCode( self, X87_FADDR_ST0 + diff, sta, 0, 0 );
			}else if( ta == DAO_INTEGER ){
				DaoJIT_AddCode( self, X87_FIADDR32_M +diff, EDX, DIS_B4, OpValue(opa) );
			}else if( ta == DAO_FLOAT ){
				DaoJIT_AddCode( self, X87_FADDR32_M +diff, EDX, DIS_B4, OpValue(opa) );
			}else{
				DaoJIT_AddCode( self, X87_FADDR64_M +diff, EDX, DIS_B4, OpValue(opa) );
			}
		}else{
			stc = DaoJIT_FindX87Stacked( self, opc );
			if( stc >=0 ){
				if( stc >0 ) DaoJIT_X87Swap2Top( self, stc );
				DaoJIT_X87StoreAndPop( self );
			}else if( self->x87Top+1 == X87_STACK_SIZE ){
				DaoJIT_AdjustStack( self, 2 );
			}
			sta = DaoJIT_FindX87Stacked( self, opa );
			if( sta >=0 ){
				DaoJIT_AddCode( self, X87_FLD_ST, sta, 0, opc );
			}else{
				DaoJIT_X87Load2( self, opa, opc, EDX );
			}
			stb = DaoJIT_FindX87Stacked( self, opb );
			if( stb >=0 ){
				DaoJIT_AddCode( self, X87_FADD_ST0 + diff, stb, 0, 0 );
			}else if( tb == DAO_INTEGER ){
				DaoJIT_AddCode( self, X87_FIADD32_M +diff, EDX, DIS_B4, OpValue(opb) );
			}else if( tb == DAO_FLOAT ){
				DaoJIT_AddCode( self, X87_FADD32_M +diff, EDX, DIS_B4, OpValue(opb) );
			}else{
				DaoJIT_AddCode( self, X87_FADD64_M +diff, EDX, DIS_B4, OpValue(opb) );
			}
		}
		break;
	case DVM_AND_FFF : case DVM_OR_FFF :
	case DVM_AND_DDD : case DVM_OR_DDD :
	case DVM_AND_FNN : case DVM_OR_FNN :
	case DVM_AND_DNN : case DVM_OR_DNN :
		if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
		sta = DaoJIT_FindX87Stacked( self, opa );
		stb = DaoJIT_FindX87Stacked( self, opb );
		if( sta >=0 ){
			DaoJIT_AddCode( self, X87_FLD_ST, sta, 0, opa );
		}else{
			DaoJIT_X87Load2( self, opa, opa, EDX );
		}
		sta = DaoJIT_FindX87Stacked( self, opa );
		if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
		DaoJIT_AddCode( self, X87_FTST, 0, 0, 0 );
		DaoJIT_AddCode( self, X87_FNSTSW_AX, 0, 0, 0 );
		DaoJIT_AddCode( self, X86_SAHF, 0, 0, 0 );
		DaoJIT_AddCode( self, X86_JE8, 0,0, self->nlabel );
		if( code == DVM_AND_FFF || code == DVM_AND_DDD
				|| code == DVM_AND_FNN || code == DVM_AND_DNN ){
			if( stb >=0 ){
				DaoJIT_AddCode( self, X87_FLD_ST, stb, 0, opc );
			}else{
				DaoJIT_X87Load2( self, opb, opc, EDX );
			}
		}else{
			DaoJIT_AddCode( self, X87_FLD_ST, sta, 0, opc );
		}
		DaoJIT_AddCode( self, X86_JMP8, 0,0, self->nlabel+1 );

		DaoJIT_AddCode( self, X86_LABEL, 0,0, self->nlabel );
		if( code == DVM_AND_FFF || code == DVM_AND_DDD
				|| code == DVM_AND_FNN || code == DVM_AND_DNN ){
			DaoJIT_AddCode( self, X87_FLD_ST, sta, 0, opc );
		}else{
			stb = DaoJIT_FindX87Stacked( self, opb );
			if( stb >=0 ){
				DaoJIT_AddCode( self, X87_FLD_ST, stb, 0, opc );
			}else{
				DaoJIT_X87Load2( self, opb, opc, EDX );
			}
		}
		DaoJIT_AddCode( self, X86_LABEL, 0,0, self->nlabel +1 );
		DaoJIT_X87Pop( self );
		self->nlabel += 2;
		break;
	case DVM_LT_FFF : case DVM_LE_FFF : case DVM_EQ_FFF : case DVM_NE_FFF :
	case DVM_LT_DDD : case DVM_LE_DDD : case DVM_EQ_DDD : case DVM_NE_DDD :
	case DVM_LT_FNN : case DVM_LE_FNN : case DVM_EQ_FNN : case DVM_NE_FNN :
	case DVM_LT_DNN : case DVM_LE_DNN : case DVM_EQ_DNN : case DVM_NE_DNN :
		if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
		sta = DaoJIT_FindX87Stacked( self, opa );
		stb = DaoJIT_FindX87Stacked( self, opb ); /* possibly opb == opc */
#if 0
		if( sta >=0 ) DaoJIT_X87StoreInt32( self, opa, EAX );
		stb = DaoJIT_FindX87Stacked( self, opb );
		if( stb >=0 ) DaoJIT_X87StoreInt32( self, opb, EAX );
		DaoJIT_AddCode( self, SSE_MOVSS_RM, XMM0_O3|EDX, DIS_B4, OpValue(opa) );
		DaoJIT_AddCode( self, SSE_MOVSS_RM, XMM1_O3|EDX, DIS_B4, OpValue(opb) );
		DaoJIT_AddCode( self, SSE_UCOMISS_RR, XMM0_O3|XMM1, 0, 0 );
#else
		if( sta >=0 ){
			DaoJIT_AddCode( self, X87_FLD_ST, sta, 0, opc );
		}else{
			DaoJIT_X87Load2( self, opa, opc, EDX );
		}
		tb = type[opb]->tid;
		if( stb >=0 ){
			DaoJIT_AddCode( self, X87_FCOMP_ST, stb+1, 0, 0 );
		}else if( tb == DAO_INTEGER ){
			DaoJIT_AddCode( self, X87_FICOMP32_M, EDX, DIS_B4, OpValue(opb) );
		}else if( tb == DAO_FLOAT ){
			DaoJIT_AddCode( self, X87_FCOMP32_M, EDX, DIS_B4, OpValue(opb) );
		}else{
			DaoJIT_AddCode( self, X87_FCOMP64_M, EDX, DIS_B4, OpValue(opb) );
		}
		DaoJIT_X87Pop( self );
		//printf( "stack: %i\n", self->x87Stack->size );
		DaoJIT_AddCode( self, X87_FNSTSW_AX, 0, 0, 0 );
		DaoJIT_AddCode( self, X86_SAHF, 0, 0, 0 );
#endif    
		c = 0;
		d = 0;
		if( code <= DVM_NE_FFF ){
			c = X86_JB8 + code - DVM_LT_FFF;
			d = DVM_TEST_F;
		}else if( code <= DVM_NE_DDD ){
			c = X86_JB8 + code - DVM_LT_DDD;
			d = DVM_TEST_D;
		}else if( code <= DVM_NE_FNN ){
			c = X86_JB8 + code - DVM_LT_FNN;
			d = DVM_TEST_F;
		}else if( code <= DVM_NE_DNN ){
			c = X86_JB8 + code - DVM_LT_DNN;
			d = DVM_TEST_D;
		}
		if( id +1 < max && opa2 == opc && vmc2->code == d ){
			switch( c ){
			case X86_JB8 :  c = X86_JAE32; break;
			case X86_JBE8 : c = X86_JA32;  break;
			case X86_JE8 :  c = X86_JNE32; break;
			case X86_JNE8 : c = X86_JE32;  break;
			default : break;
			}
			DaoJIT_StoreX87Stack( self );
			DaoJIT_AddCode( self, c, 0,0, self->nlabel );
			DMap_Insert( self->labelExtraMap, (void*) (dint)opb2, (void*) (dint)self->nlabel );
			self->nlabel ++;
			return 2;
		}

		DaoJIT_AddCode( self, c, 0,0, self->nlabel );

		DaoJIT_AddCode( self, X87_FLDZ, 0, 0, opc );
		DaoJIT_X87Pop( self );
		DaoJIT_AddCode( self, X86_JMP8, 0, 0, self->nlabel +1 );
		DaoJIT_AddCode( self, X86_LABEL, 0, 0, self->nlabel );
		DaoJIT_AddCode( self, X87_FLD1, 0, 0, opc );
		DaoJIT_AddCode( self, X86_LABEL, 0, 0, self->nlabel +1 );
		self->nlabel += 2;
		//printf( "stack: %i\n", self->x87Stack->size );
		break;
	case DVM_GETI_SI :
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EDX, DIS_B4, OpValue(opa) );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|EAX, DIS_B1, disp_string_mbs );
		DaoJIT_AddCode( self, X86_MOV32_RM, EBX3|EDX, DIS_B4, OpValue(opb) );

		lab = self->nlabel;
		self->nlabel += 2;
		DaoJIT_AddCode( self, X86_CMP32_RD8, ECX, 0, 0 );
		DaoJIT_AddCode( self, X86_JE8, 0,0, lab );

		/* check range */
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|ECX, DIS_B1, disp_string_size );
		DaoJIT_AddCheckRange( self, EBX, EAX );

		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|ECX, DIS_B1, disp_string_data );
		DaoJIT_AddCode( self, movaddr, EAX, 0, 0 ); /* clear register */
		DaoJIT_AddCode( self, X86_MOV8_RM, EAX3, SIB_S1|EBX3|ECX, 0 );
		DaoJIT_AddCode( self, X86_JMP8, 0,0, lab+1 );

		DaoJIT_AddCode( self, X86_LABEL, 0, 0, lab );
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EAX, DIS_B1, disp_string_wcs );

		/* check range */
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|EAX, DIS_B1, disp_string_size );
		DaoJIT_AddCheckRange( self, EBX, ECX );

		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EAX, DIS_B1, disp_string_data );
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3, SIB_S4|EBX3|EAX, 0 );
		DaoJIT_AddCode( self, X86_LABEL, 0,0, lab+1 );

		DaoJIT_AddCode( self, X86_MOV32_MR, EAX3|EDX, DIS_B4, OpValue(opc) );
		break;
	case DVM_SETI_SII :
		DaoJIT_AddCode( self, X86_MOV32_RM, EBX3|EDX, DIS_B4, OpValue(opb) );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|EDX, DIS_B4, OpValue(opc) );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|ECX, DIS_B1, disp_string_mbs );

		lab = self->nlabel;
		self->nlabel += 2;
		DaoJIT_AddCode( self, X86_CMP32_RD8, ECX, 0,0 );
		DaoJIT_AddCode( self, X86_JE8, 0,0, lab );
		/* check range */
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|ECX, DIS_B1, disp_string_size );
		DaoJIT_AddCheckRange( self, EBX, EAX );

		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EDX, DIS_B4, OpValue(opa) );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|ECX, DIS_B1, disp_string_data );
		DaoJIT_AddCode( self, X86_MOV8_MR, EAX3, SIB_S1|EBX3|ECX, 0 );
		DaoJIT_AddCode( self, X86_JMP8, 0,0, lab+1 );

		DaoJIT_AddCode( self, X86_LABEL, 0,0, lab );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|EDX, DIS_B4, OpValue(opc) );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|ECX, DIS_B1, disp_string_wcs );
		/* check range */
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|ECX, DIS_B1, disp_string_size );
		DaoJIT_AddCheckRange( self, EBX, EAX );

		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EDX, DIS_B4, OpValue(opa) );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|ECX, DIS_B1, disp_string_data );
		DaoJIT_AddCode( self, X86_MOV32_MR, EAX3, SIB_S4|EBX3|ECX, 0 );
		DaoJIT_AddCode( self, X86_LABEL, 0,0, lab+1 );
		break;
	case DVM_GETI_LII :  case DVM_GETI_LFI :  case DVM_GETI_LDI :
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EDX, DIS_B4, OpValue(opa) );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|EAX, DIS_B1, disp_list_items );
		DaoJIT_AddCode( self, X86_MOV32_RM, EBX3|EDX, DIS_B4, OpValue(opb) );
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|ECX, DIS_B1, disp_array_size );
		DaoJIT_AddCheckRange( self, EBX, EAX );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|ECX, DIS_B1, disp_array_data );
		DaoJIT_AddCode( self, X86_IMUL32_RD, EBX3|EBX, 0, sizeof_value );
		DaoJIT_AddCode( self, X86_ADD32_RR, ECX3|EBX, 0, 0 );
		if( code == DVM_GETI_LII ){
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|ECX, DIS_B1, disp_value_v );
			DaoJIT_AddCode( self, X86_MOV32_MR, EAX3|EDX, DIS_B4, OpValue(opc) );
		}else if( code == DVM_GETI_LFI ){
			if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
			DaoJIT_X87Load3( self, X87_FLD32_M, ECX, DIS_B1, disp_value_v, opc );
		}else{
			if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
			DaoJIT_X87Load3( self, X87_FLD64_M, ECX, DIS_B1, disp_value_v, opc );
		}
		break;
	case DVM_SETI_LIII : case DVM_SETI_LIIF : case DVM_SETI_LIID :
	case DVM_SETI_LFII : case DVM_SETI_LFIF : case DVM_SETI_LFID :
	case DVM_SETI_LDII : case DVM_SETI_LDIF : case DVM_SETI_LDID :
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|EDX, DIS_B4, OpValue(opc) );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|ECX, DIS_B1, disp_list_items );
		DaoJIT_AddCode( self, X86_MOV32_RM, EBX3|EDX, DIS_B4, OpValue(opb) );
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|ECX, DIS_B1, disp_array_size );
		DaoJIT_AddCheckRange( self, EBX, EAX );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|ECX, DIS_B1, disp_array_data );
		DaoJIT_AddCode( self, X86_IMUL32_RD, EBX3|EBX, 0, sizeof_value );
		DaoJIT_AddCode( self, X86_ADD32_RR, ECX3|EBX, 0, 0 );

		sta = DaoJIT_FindX87Stacked( self, opa );
		if( sta <0 ){
			if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
			DaoJIT_X87Load2( self, opa, -1, EDX );
		}else if( sta >0 ){
			DaoJIT_X87Swap2Top( self, sta );
		}
		if( code <= DVM_SETI_LIID ){
			DaoJIT_AddCode( self, X87_FIST32_M, ECX, DIS_B1, disp_value_v );
		}else if( code <= DVM_SETI_LFID ){
			DaoJIT_AddCode( self, X87_FST32_M, ECX, DIS_B1, disp_value_v );
		}else{
			DaoJIT_AddCode( self, X87_FST64_M, ECX, DIS_B1, disp_value_v );
		}
		break;
	case DVM_GETI_AII :  case DVM_GETI_AFI :  case DVM_GETI_ADI :
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EDX, DIS_B4, OpValue(opa) );
		DaoJIT_AddCode( self, X86_MOV32_RM, EBX3|EDX, DIS_B4, OpValue(opb) );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|EAX, DIS_B1, disp_narray_size );
		DaoJIT_AddCheckRange( self, EBX, ECX );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|EAX, DIS_B1, disp_narray_data );
		if( code == DVM_GETI_ADI )
			DaoJIT_AddCode( self, X86_IMUL32_RD, EBX3|EBX, 0, 8 );
		else
			DaoJIT_AddCode( self, X86_IMUL32_RD, EBX3|EBX, 0, 4 );
		DaoJIT_AddCode( self, X86_ADD32_RR, ECX3|EBX, 0, 0 );

		if( code == DVM_GETI_AII ){
			/* DaoArray.data.pInt */
			DaoJIT_AddCode( self, X86_MOV32B_RM, EAX3|ECX, 0, 0 );
			DaoJIT_AddCode( self, X86_MOV32_MR, EAX3|EDX, DIS_B4, OpValue(opc) );
		}else if( code == DVM_GETI_AFI ){
			if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
			DaoJIT_X87Load3( self, X87_FLD32_M, ECX, 0, 0, opc );
		}else{
			if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
			DaoJIT_X87Load3( self, X87_FLD64_M, ECX, 0, 0, opc );
		}
		break;
	case DVM_SETI_AIII : case DVM_SETI_AIIF : case DVM_SETI_AIID :
	case DVM_SETI_AFII : case DVM_SETI_AFIF : case DVM_SETI_AFID :
	case DVM_SETI_ADII : case DVM_SETI_ADIF : case DVM_SETI_ADID :
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|EDX, DIS_B4, OpValue(opc) );
		DaoJIT_AddCode( self, X86_MOV32_RM, EBX3|EDX, DIS_B4, OpValue(opb) );
		DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|ECX, DIS_B1, disp_narray_size );
		DaoJIT_AddCheckRange( self, EBX, EAX );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|ECX, DIS_B1, disp_narray_data );
		if( code >= DVM_SETI_ADII && code <= DVM_SETI_ADID )
			DaoJIT_AddCode( self, X86_IMUL32_RD, EBX3|EBX, 0, 8 );
		else
			DaoJIT_AddCode( self, X86_IMUL32_RD, EBX3|EBX, 0, 4 );
		DaoJIT_AddCode( self, X86_ADD32_RR, ECX3|EBX, 0, 0 );

		sta = DaoJIT_FindX87Stacked( self, opa );
		if( sta <0 ){
			if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
			DaoJIT_X87Load2( self, opa, -1, EDX );
		}else if( sta >0 ){
			DaoJIT_X87Swap2Top( self, sta );
		}
		if( code <= DVM_SETI_AIID ){
			DaoJIT_AddCode( self, X87_FIST32_M, ECX, 0, 0 );
		}else if( code <= DVM_SETI_AFID ){
			DaoJIT_AddCode( self, X87_FST32_M, ECX, 0, 0 );
		}else{
			DaoJIT_AddCode( self, X87_FST64_M, ECX, 0, 0 );
		}
		break;
	case DVM_GETF_OVI :  case DVM_GETF_OVF :  case DVM_GETF_OVD :
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|EDX, DIS_B4, OpValue(opa) );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|ECX, DIS_B1, disp_object_data );
		if( code == DVM_GETF_OVI ){
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|ECX, DIS_B4, OpValue(opb) );
			DaoJIT_AddCode( self, X86_MOV32_MR, EAX3|EDX, DIS_B4, OpValue(opc) );
		}else if( code == DVM_GETF_OVF ){
			if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
			DaoJIT_X87Load3( self, X87_FLD32_M, ECX, DIS_B4, OpValue(opb), opc );
		}else{
			if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
			DaoJIT_X87Load3( self, X87_FLD64_M, ECX, DIS_B4, OpValue(opb), opc );
		}
		break;
	case DVM_SETF_OVII : case DVM_SETF_OVIF : case DVM_SETF_OVID :
	case DVM_SETF_OVFI : case DVM_SETF_OVFF : case DVM_SETF_OVFD :
	case DVM_SETF_OVDI : case DVM_SETF_OVDF : case DVM_SETF_OVDD :
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|EDX, DIS_B4, OpValue(opc) );
		DaoJIT_AddCode( self, X86_MOV32_RM, ECX3|ECX, DIS_B1, disp_object_data );
		sta = DaoJIT_FindX87Stacked( self, opa );
		if( sta <0 ){
			if( self->x87Top+1 == X87_STACK_SIZE ) DaoJIT_AdjustStack( self, 2 );
			DaoJIT_X87Load2( self, opa, -1, EDX );
		}else if( sta >0 ){
			DaoJIT_X87Swap2Top( self, sta );
		}
		if( code <= DVM_SETF_OVID ){
			DaoJIT_AddCode( self, X87_FIST32_M, ECX, DIS_B4, OpValue(opb) );
		}else if( code <= DVM_SETF_OVFD ){
			DaoJIT_AddCode( self, X87_FST32_M, ECX, DIS_B4, OpValue(opb) );
		}else{
			DaoJIT_AddCode( self, X87_FST64_M, ECX, DIS_B4, OpValue(opb) );
		}
		break;
	case DVM_POW_DDD :
		DaoJIT_AddCode( self, X86_SUB32_RD, ESP, 0, 4*ADDR_BYTES );
		/* use %eax instead of %esp, because using %esp requires a sib byte */
		DaoJIT_AddCode( self, X86_MOV32_RR, EAX3|ESP, 0,0 );
		sta = DaoJIT_FindX87Stacked( self, opa );
		stb = DaoJIT_FindX87Stacked( self, opb );
		if( sta ==0 ) DaoJIT_AddCode( self, X87_FST64_M, EAX, 0, 0 );
		if( stb ==0 ) DaoJIT_AddCode( self, X87_FST64_M, EAX, DIS_B1, 2*ADDR_BYTES );
		if( sta >0 ){
			DaoJIT_X87Swap2Top( self, sta );
			DaoJIT_AddCode( self, X87_FST64_M, EAX, 0, 0 );
			if( stb == sta )
				DaoJIT_AddCode( self, X87_FST64_M, EAX, DIS_B1, 2*ADDR_BYTES );
		}
		if( stb >0 && stb != sta ){
			DaoJIT_X87Swap2Top( self, stb );
			DaoJIT_AddCode( self, X87_FST64_M, EAX, 0, 0 );
		}
		DaoJIT_StoreX87Stack( self );
		if( sta <0 ){
			DaoJIT_AddCode( self, X87_FLD64_M, EDX, DIS_B4, OpValue( opa ) );
			DaoJIT_AddCode( self, X87_FSTP64_M, EAX, 0, 0 );
		}
		if( stb <0 ){
			DaoJIT_AddCode( self, X87_FLD64_M, EDX, DIS_B4, OpValue( opb ) );
			DaoJIT_AddCode( self, X87_FSTP64_M, EAX, DIS_B1, 2*ADDR_BYTES );
		}
		self->x87Top = -1;
#ifdef DAO_64BITS
		DaoJIT_AddCode( self, SSE_MOVSD_RM, XMM0_O3|EAX, DIS_B1, 0 );
		DaoJIT_AddCode( self, SSE_MOVSD_RM, XMM1_O3|EAX, DIS_B1, 2*ADDR_BYTES );
		DaoJIT_AddCode( self, X86_PUSH_R, EDI, 0,0 );
		DaoJIT_AddCode( self, X86_PUSH_R, EDX, 0,0 );
#endif
		DaoJIT_AddCode( self, movaddr, EAX, 0, (dint) pow );
		DaoJIT_AddCode( self, X86_CALL, EAX, 0, 0 );
#ifdef DAO_64BITS
		DaoJIT_AddCode( self, X86_POP_R, EDX, 0,0 );
		DaoJIT_AddCode( self, X86_POP_R, EDI, 0,0 );
		DaoJIT_AddCode( self, SSE_MOVSD_MR, XMM0_O3|EDX, DIS_B4, OpValue(opc) );
#else
		DaoJIT_X87Push( self, opc, opc );
		DaoJIT_AddCode( self, X86_MOV32_RM, EDX3|EBP, DIS_B1, 2*ADDR_BYTES );
#endif
		DaoJIT_AddCode( self, X86_ADD32_RD, ESP, 0, 4*ADDR_BYTES );
		// DaoJIT_GetParLocal( self ); XXX
		break;
	default : 
		if( (code <= DVM_POW && jitCalls[ code ]) 
				|| code == DVM_GETI_LI || code == DVM_GETI_LSI
				|| code == DVM_SETI_LI || code == DVM_SETI_LSIS
				|| code == DVM_MOVE_SS || code == DVM_MOVE_PP 
				|| code == DVM_ADD_SS ){
			DaoJIT_StoreX87Stack( self );
#ifdef DAO_64BITS_GNUC
			if( bcall ==0 ){
				DaoJIT_AddCode( self, X86_PUSH_R, EDX, 0,0 );
				DaoJIT_AddCode( self, X86_PUSH_R, EDI, 0,0 );
				DaoJIT_AddCode( self, X86_MOV32_RM, EDI3|EDI, 0, 0 ); /* context */
				if( acall ){
					DaoJIT_AddCode( self, X86_PUSH_R, EBX, 0,0 );
					DaoJIT_AddCode( self, X86_PUSH_R, EDI, 0,0 );
					DaoJIT_AddCode( self, X86_MOV32_RR, EBX3|ESP, 0,0 );
				}
			}else{
				DaoJIT_AddCode( self, X86_MOV32_RM, EDI3|EBX, 0, 0 ); /* context */
			}
			DaoJIT_AddCode( self, X86_MOV32_RD, ESI, 0, id ); /* index */
#elif DAO_64BITS_MSC
#else
			DaoJIT_AddCode( self, X86_PUSH_D, 0, 0, id );
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EBP, DIS_B1, 3*ADDR_BYTES );
			DaoJIT_AddCode( self, X86_MOV32_RM, EAX3|EAX, 0, 0 );
			DaoJIT_AddCode( self, X86_PUSH_R, EAX, 0, 0 );
#endif
			switch( code ){
			case DVM_ADD_SS : fptr = (dint) DaoJIT_AddString; break;
			case DVM_MOVE_SS : fptr = (dint) DaoJIT_MoveString; break;
			case DVM_MOVE_PP : fptr = (dint) DaoJIT_MovePP; break;
			case DVM_GETI_LI : fptr = (dint) DaoJIT_GETI_LI; break;
			case DVM_SETI_LI : fptr = (dint) DaoJIT_SETI_LI; break;
			case DVM_GETI_LSI : fptr = (dint) DaoJIT_GETI_LSI; break;
			case DVM_SETI_LSIS : fptr = (dint) DaoJIT_SETI_LSIS; break;
			default : fptr = (dint) jitCalls[ code ]; 
					  if( fptr == 0 || code > DVM_SECT ){
						  printf( "ERROR: jit compiling is missing for the instruction:\n" );
						  exit(0);
					  }
					  break;
			}
			DaoJIT_AddCode( self, movaddr, EAX, 0, fptr );
			//printf( "%p %lx\n", DaoJIT_AddString, (dint)DaoJIT_AddString );
			DaoJIT_AddCode( self, X86_CALL, EAX, 0, 0 );
#ifdef DAO_64BITS_GNUC
			if( acall ==0 ){
				if( bcall ){
					DaoJIT_AddCode( self, X86_POP_R, EDI, 0,0 );
					DaoJIT_AddCode( self, X86_POP_R, EBX, 0,0 );
				}
				DaoJIT_AddCode( self, X86_POP_R, EDI, 0,0 );
				DaoJIT_AddCode( self, X86_POP_R, EDX, 0,0 );
			}
#elif DAO_64BITS_MSC
#else
			DaoJIT_AddCode( self, X86_MOV32_RM, EDX3|EBP, DIS_B1, 2*ADDR_BYTES );
			DaoJIT_AddCode( self, X86_ADD32_RD, ESP, 0, 2*ADDR_BYTES );
#endif
		}else{
			printf( "ERROR: jit compiling is missing for the instruction:\n" );
			exit(0);
		}
		break;
	}
	node = DMap_Find( self->labelExtraMap, (void*)(dint) (id+1) );
	if( node ){
		DaoJIT_StoreX87Stack( self );
		DaoJIT_AddCode( self, X86_LABEL, 0,0, node->value.pInt );
	}
	return 1;
}
static void DaoJIT_AddLabels( DaoJIT *self )
{
	DaoJitCode *jc;
	int i, j;
	for(i=0; i<self->labelExtra->size; i+=2){
		dint id = self->labelExtra->items.pInt[i+1];
		dint lab = self->labelExtra->items.pInt[i];
		for(j=0; j<self->jitCodes->size; j++){
			jc = self->jitCodes->items.pJitc[j];
			//printf( "here %i %i\n", id, jc->vmcID );
			if( jc->vmcID == id ){
				if( jc->opcode == X86_LABEL && jc->extra == lab ) break;
				self->jc->vmcID = id;
				self->jc->opcode = X86_LABEL;
				self->jc->extra = lab;
				self->jc->rmsti = 0;
				self->jc->modsib = 0;
				DArray_Insert( self->jitCodes, self->jc, j );
				break;
			}
		}
	}
}
static void DaoJIT_StoreX87Stack( DaoJIT *self )
{
	//printf( "DaoJIT_StoreX87Stack() %i\n", self->x87Top );
	if( self->x87Top <0 ) return;
	DaoJIT_AdjustStack( self, self->x87Top +1 );
}

static void DaoJIT_GenerateX86Code( DaoJIT *self, DString *bin )
{
	DaoJitCode *jc;
	uchar_t buf[20];
	uchar_t *sp;
	int i, code, rmsti, modsib, x87, disp, sib;
	dint extra;
	DString_ToMBS( bin );
	DString_Clear( bin );
	/* edx stores the address of DaoContext->regLocals */
	for(i=0; i<self->jitCodes->size; i++){
		jc = self->jitCodes->items.pJitc[i];
		code = jc->opcode;
		rmsti = jc->rmsti;
		modsib = jc->modsib;
		extra = jc->extra;
		x87 = code - X87_FNOP;
		disp = sib = 0;
		switch( modsib & 0xFF00 ){ 
			/* combination of SIB and DISP is possible, but not used */
		case SIB_S1 : sib = 1; modsib = (modsib&0x00FF);      break;
		case SIB_S2 : sib = 1; modsib = (modsib&0x00FF)|0x40; break;
		case SIB_S4 : sib = 1; modsib = (modsib&0x00FF)|0x80; break;
		case SIB_S8 : sib = 1; modsib = (modsib&0x00FF)|0xC0; break;
		case DIS_B1 : disp = 1; break;
		case DIS_B4 : disp = 4; break;
		default : break;
		}
		if( sib ) rmsti |= 0x4;
		if( disp ){
			if( abs( extra ) < 0x7F ) disp = 1; else disp = 4;
			switch( disp ){
			case 1 : rmsti |= 0x40; break;
			case 4 : rmsti |= 0x80; break;
			default : break;
			}
		}
		buf[0] = extra & 0x000000ff;
		buf[1] = ( extra & 0x0000ff00 ) >>8;
		buf[2] = ( extra & 0x00ff0000 ) >>16;
		buf[3] = ( extra & 0xff000000 ) >>24;
		//printf( "%3i:  %6X, %6X  %6i,  %6i\n", i, rmsti, modsib, disp, extra );

		if( code == X86_LABEL ){
			DMap_Insert( self->labelAddress, (void*)(dint) extra, (void*) bin->size );
		}else if( code <= X86_SAHF ){
			DString_AppendChar( bin, x86CodeByte1[ code - X86_RET ] );
		}else if( code == X86_PUSH_D ){
			DString_AppendChar( bin, x86CodeByte1[ code - X86_RET ] );
			DString_AppendUBytes( bin, buf, 4 );
		}else if( code <= X86_POP_R ){
			DString_AppendChar( bin, x86CodeByte1[ code - X86_RET ] | rmsti );
		}else if( code == X86_JMP8 ){
			DArray_Append( self->labelUsage, (void*)(dint) extra );
			DArray_Append( self->labelUsage, (void*) 1 );
			DString_AppendChar( bin, x86CodeByte1Disp8[ code - X86_JMP8 ] );
			DArray_Append( self->labelUsage, (void*) bin->size );
			DString_AppendChar( bin, '\0' );
			DArray_Append( self->labelUsage, (void*) bin->size );
		}else if( code == X86_JMP32 ){
			DArray_Append( self->labelUsage, (void*)(dint) extra );
			DArray_Append( self->labelUsage, (void*) 4 );
			DString_AppendChar( bin, x86CodeByte1Disp32[ code - X86_JMP32 ] );
			DArray_Append( self->labelUsage, (void*) bin->size );
			DString_AppendUBytes( bin, ZeroByte4, 4 );
			DArray_Append( self->labelUsage, (void*) bin->size );
		}else if( code <= X86_JNE8 ){
			DArray_Append( self->labelUsage, (void*)(dint) extra );
			DArray_Append( self->labelUsage, (void*) 1 );
			DString_AppendChar( bin, x86CodeByte1Disp8[ code - X86_JMP8 ] );
			DArray_Append( self->labelUsage, (void*) bin->size );
			DString_AppendChar( bin, '\0' );
			DArray_Append( self->labelUsage, (void*) bin->size );
		}else if( code <= X86_JAE32 ){
			DArray_Append( self->labelUsage, (void*)(dint) extra );
			DArray_Append( self->labelUsage, (void*) 4 );
			DString_AppendUBytes( bin, x86CodeByte2Disp32[ code - X86_JB32 ], 2 );
			DArray_Append( self->labelUsage, (void*) bin->size );
			DString_AppendUBytes( bin, ZeroByte4, 4 );
			DArray_Append( self->labelUsage, (void*) bin->size );
		}else if( code ==X86_MOV8_RD || code ==X86_MOV8_RM || code ==X86_MOV8_MR
				|| code == X86_MOV32_RM || code == X86_MOV32_MR
				|| code == X86_MOV32B_RM || code == X86_MOV32B_MR
				|| code == X86_ADD8_RD || code ==X86_ADD8_RM || code ==X86_ADD8_MR
				|| code == X86_ADD32_RM || code == X86_ADD32_MR || code == X86_CMP32_RD8
				|| code == X86_MOV32_RD || code ==X86_ADD32_RD
				|| code == X86_SUB32_RD || code == X86_PUSH_M
				|| code == X86_IMUL32_RD ){
			if( sizeof(void*) ==8 ){ 
				if( code == X86_MOV32_RM || code == X86_MOV32_MR
						|| code == X86_ADD32_RM || code == X86_ADD32_MR )
					DString_AppendChar( bin, 0x48 );
#if 1
				else if( code == X86_MOV32_RD || code == X86_ADD32_RD
						|| code == X86_SUB32_RD || code == X86_IMUL32_RD )
					DString_AppendChar( bin, 0x48 );
#endif
				else if( code == X86_CMP32_RD8 )
					DString_AppendChar( bin, 0x48 );
			}
			DString_AppendChar( bin, x86CodeByte2[ code - X86_MOV8_RD ][0] );
			DString_AppendChar( bin, x86CodeByte2[ code - X86_MOV8_RD ][1] | rmsti );
			if( sib ) DString_AppendChar( bin, modsib );
			if( disp ) DString_AppendUBytes( bin, buf, disp );
			if( code == X86_MOV8_RD || code == X86_ADD8_RD || code == X86_CMP32_RD8 ){
				DString_AppendUBytes( bin, buf, 1 );
			}else if( code ==X86_MOV32_RD || code ==X86_ADD32_RD 
					|| code == X86_SUB32_RD || code == X86_IMUL32_RD ){
				DString_AppendUBytes( bin, buf, 4 );
			}
		}else if( ( code >= X86_MOV8_RR && code <= X86_MOV8_MR )
				|| ( code >= X86_MOV32_RR && code <= X86_MOV32_MR )
				|| ( code >= X86_ADD8_RR && code <= X86_ADD8_MR )
				|| ( code >= X86_ADD32_RR && code <= X86_ADD32_MR )
				|| code == X86_CMP32_RR || code == X86_CALL ){
			if( sizeof(void*) ==8 ){
				if( code == X86_CMP32_RR || code == X86_CALL 
						|| ( code >= X86_MOV32_RR && code <= X86_MOV32_MR )
						|| ( code >= X86_ADD32_RR && code <= X86_ADD32_MR ) )
					DString_AppendChar( bin, 0x48 );
			}
			DString_AppendChar( bin, x86CodeByte2[ code - X86_MOV8_RD ][0] );
			DString_AppendChar( bin, x86CodeByte2[ code - X86_MOV8_RD ][1] | rmsti );
		}else if( code == X86_MOV64_RD ){
			DString_AppendChar( bin, 0x48 );
			DString_AppendChar( bin, 0xB8 | rmsti );
#ifdef DAO_64BITS
			buf[4] = ( extra & 0x000000ff00000000 ) >>32;
			buf[5] = ( extra & 0x0000ff0000000000 ) >>40;
			buf[6] = ( extra & 0x00ff000000000000 ) >>48;
			buf[7] = ( extra & 0xff00000000000000 ) >>56;
#endif
			DString_AppendUBytes( bin, buf, 8 );
		}else if( code == X86_MOV64_RR8 ){
			DString_AppendChar( bin, 0x49 );
			DString_AppendChar( bin, x86CodeByte2[ code - X86_MOV8_RD ][0] );
			DString_AppendChar( bin, x86CodeByte2[ code - X86_MOV8_RD ][1] | rmsti );
		}else if( code == X86_MOV64_R8R ){
			DString_AppendChar( bin, 0x4C );
			DString_AppendChar( bin, x86CodeByte2[ code - X86_MOV8_RD ][0] );
			DString_AppendChar( bin, x86CodeByte2[ code - X86_MOV8_RD ][1] | rmsti );
		}else if( code >= SSE_MOVSS_RM && code <= SSE_MOVSD_RR ){
			DString_AppendUBytes( bin, sseCode[ code - SSE_MOVSS_RM ], 3 );
			DString_AppendChar( bin, sseCode[ code - SSE_MOVSS_RM ][3] | rmsti );
			if( sib ) DString_AppendChar( bin, modsib );
			if( disp ) DString_AppendUBytes( bin, buf, disp );
		}else if( code == SSE_UCOMISS_RR ){
			DString_AppendUBytes( bin, sseCode[ code - SSE_MOVSS_RM ], 2 );
			DString_AppendChar( bin, sseCode[ code - SSE_MOVSS_RM ][2] | rmsti );
		}else if( code < X87_FNOP ){
		}else if( code <= X87_FINCSTP ){
			DString_AppendUBytes( bin, x87Code[ x87 ], 2 );
		}else if( code <= X87_FLD_ST ){
			DString_AppendChar( bin, x87Code[ x87 ][0] );
			DString_AppendChar( bin, x87Code[ x87 ][1] | rmsti );
		}else if( code <= X87_FLDZ ){
			DString_AppendUBytes( bin, x87Code[ x87 ], 2 );
		}else if( code <= X87_FILD_R ){
			DString_AppendChar( bin, x87Code[ x87 ][0] );
			DString_AppendChar( bin, x87Code[ x87 ][1] | rmsti );
		}else if( code <= X87_FSTP64_M ){
			DString_AppendChar( bin, x87Code[ x87 ][0] );
			DString_AppendChar( bin, x87Code[ x87 ][1] | rmsti );
			if( sib ) DString_AppendChar( bin, modsib );
			if( disp ) DString_AppendUBytes( bin, buf, disp );
		}else if( code <= X87_FIDIV32_R ){
			DString_AppendChar( bin, x87Code[ x87 ][0] );
			DString_AppendChar( bin, x87Code[ x87 ][1] | rmsti );
		}else if( code <= X87_FDIVR64_M ){
			DString_AppendChar( bin, x87Code[ x87 ][0] );
			DString_AppendChar( bin, x87Code[ x87 ][1] | rmsti );
			if( sib ) DString_AppendChar( bin, modsib );
			if( disp ) DString_AppendUBytes( bin, buf, disp );
		}else{
			DString_AppendChar( bin, x87Code[ x87 ][0] );
			DString_AppendChar( bin, x87Code[ x87 ][1] | rmsti );
		}
	}
	DString_AppendChar( bin, 0xC3 );
	for(i=0; i<self->labelUsage->size; i+=4 ){
		dint lab = self->labelUsage->items.pInt[i];
		dint byt = self->labelUsage->items.pInt[i+1];
		dint use = self->labelUsage->items.pInt[i+2];
		dint ofs = self->labelUsage->items.pInt[i+3];
		dint def = 1;
		DNode *node = DMap_Find( self->labelAddress, (void*)(dint)lab );
		if( node ) def = node->value.pInt;
		sp = (uchar_t*) bin->mbs + use;
		if( byt == 1 ){
			uchar_t disp = def - ofs;
			sp[0] = disp;
		}else if( byt == 4 ){
			int disp = def - ofs;
			sp[0] = disp & 0x000000ff;
			sp[1] = ( disp & 0x0000ff00 ) >>8;
			sp[2] = ( disp & 0x00ff0000 ) >>16;
			sp[3] = ( disp & 0xff000000 ) >>24;
			//printf( "offset: %i %.2X %.2X %.2X %.2X\n", disp, sp[0], sp[1], sp[2], sp[3] );
		}
	}
}

static int ExtendCompilable( DaoRoutine *self, int from )
{
	DaoVmCode *vmcs = self->vmCodes->codes;
	DaoVmCode *vmc, *vmc2;
	int N = self->vmCodes->size;
	int i, code, m = 0;
	//printf( "ExtendCompilable( %s ) >>>>>>>>>>> %i\n", self->routName->mbs, from );
	for(i=from; i<N; i++){
		vmc = vmcs + i;
		//printf( "%3i: ", i ); DaoVmCode_Print( *vmc, NULL );
		code = vmc->code;
		/* XXX create an array to mark the instructions that are jit-compilable */
		if( ( code >= DVM_GETV_I && code <= DVM_SETV_DD )
				|| ( code >= DVM_MOVE_II && code <= DVM_MOVE_DD )
				|| code == DVM_GETI_SI || code == DVM_SETI_SII
				|| ( code >= DVM_GETI_LII && code <= DVM_SETI_ADID )
				|| ( code >= DVM_GETF_OVI && code <= DVM_GETF_OVD )
				|| ( code >= DVM_SETF_OVII && code <= DVM_SETF_OVDD )
				|| ( code >= DVM_ADD_III && code <= DVM_DIV_III )
				|| ( code >= DVM_AND_III && code <= DVM_NE_III )
				|| ( code >= DVM_ADD_FFF && code <= DVM_DIV_FFF )
				|| ( code >= DVM_AND_FFF && code <= DVM_NE_FFF )
				|| ( code >= DVM_ADD_DDD && code <= DVM_DIV_DDD )
				|| ( code >= DVM_AND_DDD && code <= DVM_NE_DDD )
				|| ( code >= DVM_ADD_FNN && code <= DVM_DIV_FNN )
				|| ( code >= DVM_AND_FNN && code <= DVM_NE_FNN )
				|| ( code >= DVM_ADD_DNN && code <= DVM_DIV_DNN )
				|| ( code >= DVM_AND_DNN && code <= DVM_NE_DNN )
		  ){
			m ++;
		}else if( code == DVM_POW_DDD || code == DVM_GETI_LI ){
			m ++;
		}else if( code == DVM_ADD_SS ){
			m ++;
		}else if( code == DVM_MOVE_SS || code == DVM_MOVE_PP ){
			m ++;
		}else if( code <= DVM_POW && jitCalls[ code ] ){
			m ++;
		}else if( code == DVM_TEST_I || code == DVM_TEST_F || code == DVM_TEST_D ){
			int k = ExtendCompilable( self, i + 1 );
			vmc2 = vmcs + (i+1+k);
			//DaoVmCode_Print( *vmc2, NULL );
			//printf( ">>>>>>>>>>>>>>>>>>>> %i %i %i %i\n", i, k, i+1+k, from );
			if( vmc2->code == DVM_GOTO && vmc2->b >= from && vmc2->b < i+1+k ){
				m += k + 2;
				i += k + 1;
			}else{
				break;
			}
		}else{
			break;
		}
	}
	//printf( "<<<< m = %i\n", m );
	return m;
}
#include<assert.h>
#include"daoVmspace.h"
#include"daoType.h"
void DaoRoutine_JitCompile( DaoRoutine *self )
{
	DaoVmCode *vmcs = self->vmCodes->codes;
	DaoVmCode *vmc;
	DaoVmCode vmcode;
	DaoVmCodeX anncode;
	DString *bin, *dest;
	int N = self->vmCodes->size;
	int i, j, k, k2, m, code;
	//  printf( "DaoRoutine_JitCompile()\n" );
	bin = DString_New(1);
	dest = DString_New(1);
	vmcode.code = DVM_JITC;
	vmcode.c = 0;
	anncode.code = DVM_JITC;
	anncode.c = 0;
	anncode.annot = NULL;
	//return;
	DString_Resize( dest, N );
	memset( dest->mbs, 0, N );
	for(i=0; i<N; i++){
		vmc = vmcs + i;
		k = vmc->code;
		if( k == DVM_GOTO || k == DVM_TEST 
				|| k == DVM_TEST_I || k == DVM_TEST_F || k == DVM_TEST_D )
			dest->mbs[ vmc->b+1 ] = 1;
	}
	//DaoRoutine_PrintCode( self, self->nameSpace->vmSpace->stdStream );
	for(i=0; i<N; i++){
		m = ExtendCompilable( self, i );
		vmc = vmcs + (i+m);
		code = vmc->code;
		/*
		   printf( "i = %i:\n", m );
		   DaoVmCode_Print( vmcs[i], NULL );
		   DaoVmCode_Print( vmcs[i+m], NULL );
		 */
		if( m >=10 ){
			DaoJIT *jit = DaoJIT_New();
			jit->gotoDest = dest;
			vmc = vmcs + i;
			assert( vmc->code != DVM_JITC );
			vmcode.a = self->binCodes->size;
			vmcode.b = m + 1;
			anncode.a = vmcode.a;
			anncode.b = vmcode.b;
			anncode.level = self->annotCodes->items.pVmc[i]->level;
			anncode.line = self->annotCodes->items.pVmc[i]->line;
			DArray_Insert( self->annotCodes, & anncode, i );
			DaoVmcArray_Insert( self->vmCodes, vmcode, i );
			vmcs = self->vmCodes->codes;
			N = self->vmCodes->size;
			jit->routine = self;
			DString_Clear( bin );
			DaoJIT_AddCode( jit, X86_PUSH_R, EBP, 0,0 );
			DaoJIT_AddCode( jit, X86_MOV32_RR, EBP3|ESP, 0,0 );
			DaoJIT_AddCode( jit, X86_PUSH_R, EBX, 0,0 );
#ifdef DAO_64BITS_GNUC
			jit->parLocal = EDX;
			jit->parExtra = EDI;
			DaoJIT_AddCode( jit, X86_MOV32_RR, EDX3|EDI, 0, 0 );
			DaoJIT_AddCode( jit, X86_MOV32_RR, EDI3|ESI, 0, 0 );
#elif DAO_64BITS_MSC
			jit->parLocal = EDX;
			jit->parExtra = EDI;
			DaoJIT_AddCode( jit, X86_MOV32_RR, EDI3|ECX, 0, 0 );
#else
			jit->parLocal = EDX;
			DaoJIT_AddCode( jit, X86_MOV32_RM, EDX3|EBP, DIS_B1, 2*ADDR_BYTES );
#endif
			for(j=1; j<=m;){
				for( k=0; k<m; k++ ){
					k2 = self->vmCodes->codes[i+k].code;
					if( k2 == DVM_TEST_I || k2 == DVM_TEST_F || k2 == DVM_TEST_D
							|| k2 == DVM_GOTO ){
						if( self->vmCodes->codes[i+k].b == i+j ){
							DaoJIT_StoreX87Stack( jit );
							break;
						}
					}
				}
				j += DaoJIT_Encode( jit, self->vmCodes, i+j, i+1, m+i );
			}
			DaoJIT_AddLabels( jit );
			DaoJIT_AddCode( jit, X86_MOV32_RD, EAX, 0, 0 );
			DaoJIT_AddCode( jit, X86_LABEL, 0, 0, LAB_RETURN );
			DaoJIT_StoreX87Stack( jit );
			DaoJIT_AddCode( jit, X86_POP_R, EBX,0,0 );
			if( jit->parExtra >=0 && ADDR_BYTES ==4 )
				DaoJIT_AddCode( jit, X86_POP_R, EDI,0,0 );
			DaoJIT_AddCode( jit, X86_MOV32_RR, ESP3|EBP, 0,0 );
			DaoJIT_AddCode( jit, X86_POP_R, EBP,0,0 );
			DaoJIT_GenerateX86Code( jit, bin );

#if 0
			for(j=1; j<=m; j++){
				printf( "%6i: ", j+i );
				DaoVmCode_Print( self->vmCodes->codes[j+i], NULL );
			}
			for(j=0; j<jit->jitCodes->size; j++){
				printf( "%6i: ", j );
				DaoJitCode_Print( jit->jitCodes->items.pJitc[j] );
			}
#endif
			DaoJIT_Delete( jit );
			DArray_Append( self->binCodes, bin );

#if 0
#include"stdio.h"
			FILE *fout = popen( "x86dis -e 0 -s att", "w" );
			FILE *fout2 = fopen( "debug.o", "w" );
			for(j=0; j<bin->size; j++){
				unsigned char ch = bin->mbs[j];
				//fprintf( fout, "%c", ch );
				fprintf( fout2, "%c", ch );
				printf( "%.2X ", ch );
			}
			printf( "\n" );
			pclose( fout );
			fclose( fout2 );
			if( sizeof(void*) == 4 )
				system( "ndisasm debug.o" );
			else
				system( "ndisasm -b 64 debug.o" );
#endif
			i += m + 1;
		}
	}
	DString_Delete( bin );
	DString_Delete( dest );
	DaoJitMapper_MapRoutine( self );
}

#ifdef UNIX
#include<assert.h>
#include<sys/mman.h>

#ifdef MAC_OSX
#define MAP_ANONYMOUS MAP_ANON
#endif

DaoJitMemory* DaoJitMemory_New( int size )
{
	DaoJitMemory *self = (DaoJitMemory*) dao_malloc( sizeof(DaoJitMemory) );
	DaoBase_Init( self, DAO_NIL );
	self->size = ROUND_PAGE( size );
	self->offset = 16;
	self->memory = mmap( NULL, self->size, PROT_EXEC|PROT_READ|PROT_WRITE, 
			MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );
	if( self->memory == MAP_FAILED ){
		self->size = 0;
		self->memory = NULL;
	}
	self->heap = self->memory;
	return self;
}
void DaoJitMemory_Delete( DaoJitMemory *self )
{
	if( self->size && self->memory != NULL ) munmap( self->memory, self->size );
	dao_free( self );
}
void DaoJitMemory_Protect( DaoJitMemory *self )
{
	mprotect( self->memory, self->size, PROT_READ|PROT_EXEC );
}
void DaoJitMemory_Unprotect( DaoJitMemory *self )
{
	mprotect( self->memory, self->size, PROT_READ|PROT_EXEC|PROT_WRITE );
}
#elif WIN32
#include <windows.h>

#ifdef HEAP_CREATE_ENABLE_EXECUTE
#define HCFLAGS  (HEAP_NO_SERIALIZE|HEAP_CREATE_ENABLE_EXECUTE)
#else
#define HCFLAGS  (HEAP_NO_SERIALIZE|0x00040000)
#endif

DaoJitMemory* DaoJitMemory_New( int size )
{
	DaoJitMemory *self = (DaoJitMemory*) dao_malloc( sizeof(DaoJitMemory) );
	DaoBase_Init( self, DAO_NIL );
	self->offset = 16;
	self->heap = (void*) HeapCreate( HCFLAGS, 0, 0 ); 
	self->size = 0;
	self->memory = NULL;
	if( self->heap ){
		self->size = ROUND_PAGE( size );
		self->memory = HeapAlloc( self->heap, 0, self->size );
	}
	return self;
}
void DaoJitMemory_Delete( DaoJitMemory *self )
{
	if( self->size && self->heap != NULL ) HeapDestroy( self->heap );
	dao_free( self );
}
void DaoJitMemory_Protect( DaoJitMemory *self )
{
}
void DaoJitMemory_Unprotect( DaoJitMemory *self )
{
}
#else
#endif

#endif
