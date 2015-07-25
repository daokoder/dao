
#include<math.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>
#include"daoStdtype.h"
#include"daoValue.h"
#include"daoProcess.h"


typedef struct DaoxUserPodType  DaoxUserPodType;

/* bit integer */
struct DaoxUserPodType
{
	DAO_CPOD_COMMON;

	dao_integer value;
};
DaoType *daox_type_user_pod_type = NULL;

DAO_DLL DaoxUserPodType* DaoxUserPodType_New();
DAO_DLL void DaoxUserPodType_Delete( DaoxUserPodType *self );


DaoxUserPodType* DaoxUserPodType_New()
{
	DaoxUserPodType *self = (DaoxUserPodType*) DaoCpod_New( daox_type_user_pod_type, sizeof(DaoxUserPodType) );
	return self;
}
void DaoxUserPodType_Delete( DaoxUserPodType *self )
{
	DaoCpod_Delete( (DaoCpod*) self );
}

static void DaoxUserPodType_GetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid )
{
}
static void DaoxUserPodType_SetItem1( DaoValue *self0, DaoProcess *proc, DaoValue *pid, DaoValue *value )
{
}
static void DaoxUserPodType_GetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N )
{
	switch( N ){
	case 0 : DaoxUserPodType_GetItem1( self, proc, dao_none_value ); break;
	case 1 : DaoxUserPodType_GetItem1( self, proc, ids[0] ); break;
	default : DaoProcess_RaiseError( proc, "Index", "not supported" );
	}
}
static void DaoxUserPodType_SetItem( DaoValue *self, DaoProcess *proc, DaoValue *ids[], int N, DaoValue *value )
{
	switch( N ){
	case 0 : DaoxUserPodType_SetItem1( self, proc, dao_none_value, value ); break;
	case 1 : DaoxUserPodType_SetItem1( self, proc, ids[0], value ); break;
	default : DaoProcess_RaiseError( proc, "Index", "not supported" );
	}
}
static DaoTypeCore userTypeCore=
{
	NULL,
	DaoValue_GetField,
	DaoValue_SetField,
	DaoxUserPodType_GetItem,
	DaoxUserPodType_SetItem,
	NULL
};
static void UT_New1( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxUserPodType *self = DaoxUserPodType_New();
	DaoProcess_PutValue( proc, (DaoValue*) self );
	self->value = p[0]->xInteger.value;
}
static void UT_GETI( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxUserPodType *self = (DaoxUserPodType*) DaoValue_CastCstruct( p[0], daox_type_user_pod_type );
	DaoxUserPodType_GetItem1( p[0], proc, p[1] );
}
static void UT_BinaryOper2( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoxUserPodType *C = DaoxUserPodType_New();
	DaoxUserPodType *A = (DaoxUserPodType*) p[0];
	DaoxUserPodType *B = (DaoxUserPodType*) p[1];
	DaoProcess_PutValue( proc, (DaoValue*) C );
}
static void UT_CompOper2( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoValue *C = NULL;
	DaoxUserPodType *A = (DaoxUserPodType*) p[0];
	DaoxUserPodType *B = (DaoxUserPodType*) p[1];
	daoint D = 0;
	if( C ) DaoProcess_PutValue( proc, C );
	else DaoProcess_PutInteger( proc, D );
}
static void UT_ADD2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_BinaryOper2( proc, p, N, DVM_ADD );
}
static void UT_SUB2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_BinaryOper2( proc, p, N, DVM_SUB );
}
static void UT_MUL2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_BinaryOper2( proc, p, N, DVM_MUL );
}
static void UT_DIV2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_BinaryOper2( proc, p, N, DVM_DIV );
}
static void UT_MOD2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_BinaryOper2( proc, p, N, DVM_MOD );
}
static void UT_POW2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_BinaryOper2( proc, p, N, DVM_POW );
}
static void UT_AND2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_CompOper2( proc, p, N, DVM_AND );
}
static void UT_OR2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_CompOper2( proc, p, N, DVM_OR );
}
static void UT_LT2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxUserPodType *A = (DaoxUserPodType*) p[0];
	DaoxUserPodType *B = (DaoxUserPodType*) p[1];
	DaoProcess_PutInteger( proc, A->value < B->value );
}
static void UT_LE2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxUserPodType *A = (DaoxUserPodType*) p[0];
	DaoxUserPodType *B = (DaoxUserPodType*) p[1];
	DaoProcess_PutInteger( proc, A->value <= B->value );
}
static void UT_EQ2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxUserPodType *A = (DaoxUserPodType*) p[0];
	DaoxUserPodType *B = (DaoxUserPodType*) p[1];
	DaoProcess_PutInteger( proc, A->value == B->value );
}
static void UT_NE2( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoxUserPodType *A = (DaoxUserPodType*) p[0];
	DaoxUserPodType *B = (DaoxUserPodType*) p[1];
	DaoProcess_PutInteger( proc, A->value != B->value );
}
static void UT_UnaryOper( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	daoint ta;
	DaoxUserPodType *A = (DaoxUserPodType*) p[0];
	DaoxUserPodType *C = DaoxUserPodType_New();
	DaoProcess_PutValue( proc, (DaoValue*) C );
}
static void UT_MINUS( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_UnaryOper( proc, p, N, DVM_MINUS );
}
static void UT_NOT( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_UnaryOper( proc, p, N, DVM_NOT );
}
static void UT_TILDE( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_UnaryOper( proc, p, N, DVM_TILDE );
}
static void UT_BitOper2( DaoProcess *proc, DaoValue *p[], int N, int oper )
{
	DaoxUserPodType *A = (DaoxUserPodType*) p[0];
	DaoxUserPodType *B = (DaoxUserPodType*) p[1];
	DaoxUserPodType *C = DaoxUserPodType_New();
	DaoProcess_PutValue( proc, (DaoValue*) C );
}
static void UT_BITAND2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_BitOper2( proc, p, N, DVM_BITAND );
}
static void UT_BITOR2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_BitOper2( proc, p, N, DVM_BITOR );
}
static void UT_BITXOR2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_BitOper2( proc, p, N, DVM_BITXOR );
}
static void UT_BITLFT2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_BitOper2( proc, p, N, DVM_BITLFT );
}
static void UT_BITRIT2( DaoProcess *proc, DaoValue *p[], int N )
{
	UT_BitOper2( proc, p, N, DVM_BITRIT );
}
static void UT_CastToInt( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoxUserPodType *self = (DaoxUserPodType*) p[0];
	DaoProcess_PutInteger( proc, self->value );
}
static void UT_CastToString( DaoProcess *proc, DaoValue *p[], int n )
{
	DaoxUserPodType *self = (DaoxUserPodType*) p[0];
	DString *res = DaoProcess_PutChars( proc, "" );
	DString_Reserve( res, 50 );
	res->size = sprintf( res->chars, "UserPodType.{%" DAO_I64 "}", self->value );
}
static DaoFuncItem userPodTypeMeths[]=
{
	{ UT_New1, "UserPodType( value: int ) => UserPodType" },

	{ UT_GETI, "[]( self: UserPodType, idx: none ) => UserPodType" },
	{ UT_GETI, "[]( self: UserPodType, idx: int ) => int" },

	{ UT_ADD2, "+( A: UserPodType, B: UserPodType ) => UserPodType" },
	{ UT_SUB2, "-( A: UserPodType, B: UserPodType ) => UserPodType" },
	{ UT_MUL2, "*( A: UserPodType, B: UserPodType ) => UserPodType" },
	{ UT_DIV2, "/( A: UserPodType, B: UserPodType ) => UserPodType" },
	{ UT_MOD2, "%( A: UserPodType, B: UserPodType ) => UserPodType" },
	{ UT_POW2, "**( A: UserPodType, B: UserPodType ) => UserPodType" },

	{ UT_AND2, "&&( A: UserPodType, B: UserPodType ) => UserPodType" },
	{ UT_OR2,  "||( A: UserPodType, B: UserPodType ) => UserPodType" },
	{ UT_LT2,  "< ( A: UserPodType, B: UserPodType ) => int" },
	{ UT_LE2,  "<=( A: UserPodType, B: UserPodType ) => int" },
	{ UT_EQ2,  "==( A: UserPodType, B: UserPodType ) => int" },
	{ UT_NE2,  "!=( A: UserPodType, B: UserPodType ) => int" },

	{ UT_MINUS, "-( A: UserPodType ) => UserPodType" },
	{ UT_NOT,   "!( A: UserPodType ) => UserPodType" },
	{ UT_TILDE, "~( A: UserPodType ) => UserPodType" },

	{ UT_BITAND2, "&( A: UserPodType, B: UserPodType ) => UserPodType" },
	{ UT_BITOR2,  "|( A: UserPodType, B: UserPodType ) => UserPodType" },
	{ UT_BITXOR2, "^( A: UserPodType, B: UserPodType ) => UserPodType" },
	{ UT_BITLFT2, "<<( A: UserPodType, B: UserPodType ) => UserPodType" },
	{ UT_BITRIT2, ">>( A: UserPodType, B: UserPodType ) => UserPodType" },

	{ UT_CastToInt,     "(int)( self: UserPodType, hashing = false )" },
	{ UT_CastToString,  "(string)( self: UserPodType )" },

	{ NULL, NULL },
};
DaoTypeBase userPodTypeTyper =
{
	"UserPodType", NULL, NULL, (DaoFuncItem*) userPodTypeMeths, {0}, {0},
	(FuncPtrDel)DaoxUserPodType_Delete, NULL
};

DAO_DLL int DaoUserpodtype_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	daox_type_user_pod_type = DaoNamespace_WrapType( ns, & userPodTypeTyper, DAO_CPOD, 0 );
	return 0;
}
