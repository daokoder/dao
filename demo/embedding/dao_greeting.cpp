#include"dao_greeting.h"

DAO_INIT_MODULE;
DaoVmSpace *__daoVmSpace = NULL;

DaoCallbackData* DaoCallbackData_New( DaoRoutine *callback, DValue *userdata )
{
  DaoCallbackData *self = new DaoCallbackData;
  self->callback = callback;
  memset( & self->userdata, 0, sizeof(DValue) );
  DValue_Copy( & self->userdata, *userdata );
  return self;
}

#ifdef __cplusplus
extern "C"{
#endif

static DaoNumItem constNumbers[] =
{

  { "AA", DAO_INTEGER, AA },
  { "BB", DAO_INTEGER, BB },
  { "CC", DAO_INTEGER, CC },
  { "false", DAO_INTEGER, 0 },
  { "true", DAO_INTEGER, 1 },
  { NULL, 0, 0 }
};
static DaoNumItem dao_CxxNS_Nums[] =
{
  { "AA", DAO_INTEGER, CxxNS::AA },
  { "BB", DAO_INTEGER, CxxNS::BB },
  { "CC", DAO_INTEGER, CxxNS::CC },
  { "FALSE", DAO_INTEGER, CxxNS::FALSE },
  { "TRUE", DAO_INTEGER, CxxNS::TRUE },
  { NULL, 0, 0 }
};
static void dao__Testing( DaoContext *_ctx, DValue *_p[], int _n );
static void dao__Testing_dao_2( DaoContext *_ctx, DValue *_p[], int _n );
static void dao__Testing_dao_3( DaoContext *_ctx, DValue *_p[], int _n );
static void dao__GetGreetingObject( DaoContext *_ctx, DValue *_p[], int _n );
static void dao__Testing_dao_4( DaoContext *_ctx, DValue *_p[], int _n );

static DaoFuncItem dao_Funcs[] =
{
  { dao__GetGreetingObject, "GetGreetingObject(  )=>Greeting" },
  { dao__Testing_dao_4, "Testing( bl : int=CxxNS::FALSE )" },
  { NULL, NULL }
};
/* greeting.h */
static void dao__Testing( DaoContext *_ctx, DValue *_p[], int _n )
{
  Greeting* greeting= (Greeting*) DaoCData_GetData( _p[0]->v.cdata );
  CxxNS::Bool bl= (CxxNS::Bool) _p[1]->v.i;

  CxxNS::Testing( greeting, bl );
}
/* greeting.h */
static void dao__Testing_dao_2( DaoContext *_ctx, DValue *_p[], int _n )
{
  int a= (int) _p[0]->v.i;
  CxxNS::Bool bl= (CxxNS::Bool) _p[1]->v.i;

  CxxNS::Testing( a, bl );
}
/* greeting.h */
static void dao__Testing_dao_3( DaoContext *_ctx, DValue *_p[], int _n )
{
  CxxNS::Test* test= (CxxNS::Test*) DaoCData_GetData( _p[0]->v.cdata );
  CxxNS::Bool bl= (CxxNS::Bool) _p[1]->v.i;

  CxxNS2::Testing( test, bl );
}
/* greeting.h */
static void dao__GetGreetingObject( DaoContext *_ctx, DValue *_p[], int _n )
{

  Greeting* _GetGreetingObject = GetGreetingObject(  );
  DaoContext_WrapCData( _ctx, (void*) _GetGreetingObject, dao_Greeting_Typer );
}
/* greeting.h */
static void dao__Testing_dao_4( DaoContext *_ctx, DValue *_p[], int _n )
{
  CxxNS::Bool bl= (CxxNS::Bool) _p[0]->v.i;

  Testing( bl );
}

static DaoFuncItem dao_CxxNS_Funcs[] = 
{
  { dao__Testing, "Testing( greeting : Greeting, bl : int=FALSE )" },
  { dao__Testing_dao_2, "Testing( a : int, bl : int=FALSE )" },
	{ NULL, NULL }
};
static DaoFuncItem dao_CxxNS2_Funcs[] = 
{
  { dao__Testing_dao_3, "Testing( test : CxxNS::Test, bl : int=CxxNS::FALSE )" },
	{ NULL, NULL }
};
static DaoTypeBase *dao_CxxNS_Types[2] = 
{
	dao_Test_Typer,
	NULL
};
int DaoOnLoad( DaoVmSpace *vms, DaoNameSpace *ns )
{
  DaoNameSpace *ns2;
  DaoTypeBase *typers[4];
  const char *aliases[1];
  __daoVmSpace = vms;
  typers[0] = dao_Bool_Typer,
  typers[1] = dao_Greeting_Typer,
  typers[2] = NULL;
  aliases[0] = NULL;
  DaoNameSpace_TypeDefine( ns, "int", "Enum1" );
  ns2 = DaoNameSpace_GetNameSpace( ns, "CxxNS" );
  DaoNameSpace_TypeDefine( ns2, "int", "Bool" );
  DaoNameSpace_TypeDefine( ns2, "int", "Enum2" );
  DaoNameSpace_AddConstNumbers( ns, constNumbers );
  DaoNameSpace_AddTypes( ns, typers, 0 );
  ns2 = DaoNameSpace_GetNameSpace( ns, "CxxNS" );
  DaoNameSpace_AddConstNumbers( ns2, dao_CxxNS_Nums );
  DaoNameSpace_AddTypes( ns2, dao_CxxNS_Types, 0 );
  ns2 = DaoNameSpace_GetNameSpace( ns, "CxxNS2" );
  ns2 = DaoNameSpace_GetNameSpace( ns, "CxxNS" );
  DaoNameSpace_AddFunctions( ns2, dao_CxxNS_Funcs );
  ns2 = DaoNameSpace_GetNameSpace( ns, "CxxNS2" );
  DaoNameSpace_AddFunctions( ns2, dao_CxxNS2_Funcs );
  DaoNameSpace_AddFunctions( ns, dao_Funcs );
  DaoNameSpace_TypeDefine( ns, "CxxNS::Test", "Test2" );
  ns2 = DaoNameSpace_GetNameSpace( ns, "CxxNS" );
  DaoNameSpace_SetupTypes( ns2, dao_CxxNS_Types );
  DaoNameSpace_SetupTypes( ns, typers );
  return 0;
}
#ifdef __cplusplus
}
#endif

