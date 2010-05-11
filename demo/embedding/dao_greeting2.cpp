#include"dao_greeting.h"

extern DaoVmSpace *__daoVmSpace;

#ifdef __cplusplus
extern "C"{
#endif

/*  greeting.h */


static DaoNumItem dao_Bool_Nums[] =
{
  { NULL, 0, 0 }
};

static DaoFuncItem dao_Bool_Meths[] = 
{
  { NULL, NULL }
};
static void Dao_Bool_Delete( void *self ){ free( self ); }
static DaoTypeBase Bool_Typer = 
{ NULL, "Bool", dao_Bool_Nums, dao_Bool_Meths, {  0 }, 
NULL, Dao_Bool_Delete };
DaoTypeBase DAO_DLL_GREETING *dao_Bool_Typer = & Bool_Typer;

/*  greeting.h */


static DaoNumItem dao_Greeting_Nums[] =
{
  { NULL, 0, 0 }
};
static void dao_Greeting_Greeting( DaoContext *_ctx, DValue *_p[], int _n );
static void dao_Greeting_DoGreeting( DaoContext *_ctx, DValue *_p[], int _n );
static void dao_Greeting_PrintMessage( DaoContext *_ctx, DValue *_p[], int _n );
static void dao_Greeting_SetMessage( DaoContext *_ctx, DValue *_p[], int _n );
static void dao_Greeting_TestGreeting( DaoContext *_ctx, DValue *_p[], int _n );

static DaoFuncItem dao_Greeting_Meths[] = 
{
  { dao_Greeting_Greeting, "Greeting( msg : string )=>Greeting" },
  { dao_Greeting_DoGreeting, "DoGreeting( self : Greeting, name : string )" },
  { dao_Greeting_PrintMessage, "PrintMessage( self : Greeting )" },
  { dao_Greeting_SetMessage, "SetMessage( self : Greeting, msg : string )" },
  { dao_Greeting_TestGreeting, "TestGreeting( self : Greeting, g : Greeting, name : string )" },
  { NULL, NULL }
};
static void Dao_Greeting_Delete( void *self ){  delete (Greeting*)self; }
static DaoTypeBase Greeting_Typer = 
{ NULL, "Greeting", dao_Greeting_Nums, dao_Greeting_Meths, {  0 }, 
NULL, Dao_Greeting_Delete };
DaoTypeBase DAO_DLL_GREETING *dao_Greeting_Typer = & Greeting_Typer;
/* greeting.h */
static void dao_Greeting_Greeting( DaoContext *_ctx, DValue *_p[], int _n )
{
  char* msg= (char*) DString_GetMBS( _p[0]->v.s );
	DaoCxx_Greeting *_self = DaoCxx_Greeting_New( msg );
	DaoContext_PutResult( _ctx, (DaoBase*) _self->cdata );
}
/* greeting.h */
static void dao_Greeting_DoGreeting( DaoContext *_ctx, DValue *_p[], int _n )
{
  Greeting* self= (Greeting*) DaoCData_GetData( _p[0]->v.cdata );
  char* name= (char*) DString_GetMBS( _p[1]->v.s );
  self->Greeting::DoGreeting( name );
}
/* greeting.h */
static void dao_Greeting_PrintMessage( DaoContext *_ctx, DValue *_p[], int _n )
{
  Greeting* self= (Greeting*) DaoCData_GetData( _p[0]->v.cdata );
  self->Greeting::PrintMessage(  );
}
/* greeting.h */
static void dao_Greeting_SetMessage( DaoContext *_ctx, DValue *_p[], int _n )
{
  Greeting* self= (Greeting*) DaoCData_GetData( _p[0]->v.cdata );
  char* msg= (char*) DString_GetMBS( _p[1]->v.s );
  self->Greeting::SetMessage( msg );
}
/* greeting.h */
static void dao_Greeting_TestGreeting( DaoContext *_ctx, DValue *_p[], int _n )
{
  Greeting* self= (Greeting*) DaoCData_GetData( _p[0]->v.cdata );
  Greeting* g= (Greeting*) DaoCData_GetData( _p[1]->v.cdata );
  char* name= (char*) DString_GetMBS( _p[2]->v.s );
  self->Greeting::TestGreeting( g, name );
}

/*  greeting.h */


static void dao_Test_GETF_index( DaoContext *_ctx, DValue *_p[], int _n );
static void dao_Test_SETF_index( DaoContext *_ctx, DValue *_p[], int _n );
static void dao_Test_GETF_value( DaoContext *_ctx, DValue *_p[], int _n );
static void dao_Test_SETF_value( DaoContext *_ctx, DValue *_p[], int _n );
static DaoNumItem dao_Test_Nums[] =
{
  { NULL, 0, 0 }
};
static void dao_Test_Test( DaoContext *_ctx, DValue *_p[], int _n );
static void dao_Test_Print( DaoContext *_ctx, DValue *_p[], int _n );

static DaoFuncItem dao_Test_Meths[] = 
{
  { dao_Test_GETF_index, ".index( self : Test )=>int" },
  { dao_Test_SETF_index, ".index=( self : Test, index : int )" },
  { dao_Test_GETF_value, ".value( self : Test )=>double" },
  { dao_Test_SETF_value, ".value=( self : Test, value : double )" },
  { dao_Test_Test, "Test(  )=>Test" },
  { dao_Test_Print, "Print( self : Test )" },
  { NULL, NULL }
};
static void Dao_Test_Delete( void *self ){  delete (CxxNS::Test*)self; }
static DaoTypeBase Test_Typer = 
{ NULL, "Test", dao_Test_Nums, dao_Test_Meths, {  0 }, 
NULL, Dao_Test_Delete };
DaoTypeBase DAO_DLL_GREETING *dao_Test_Typer = & Test_Typer;
static void dao_Test_GETF_index( DaoContext *_ctx, DValue *_p[], int _n )
{
  CxxNS::Test *self = (CxxNS::Test*)DaoCData_GetData(_p[0]->v.cdata);
  DaoContext_PutInteger( _ctx, (int) self->index );
}
static void dao_Test_SETF_index( DaoContext *_ctx, DValue *_p[], int _n )
{
  CxxNS::Test *self = (CxxNS::Test*)DaoCData_GetData(_p[0]->v.cdata);
  self->index = (int) _p[1]->v.i;
}
static void dao_Test_GETF_value( DaoContext *_ctx, DValue *_p[], int _n )
{
  CxxNS::Test *self = (CxxNS::Test*)DaoCData_GetData(_p[0]->v.cdata);
  DaoContext_PutDouble( _ctx, (double) self->value );
}
static void dao_Test_SETF_value( DaoContext *_ctx, DValue *_p[], int _n )
{
  CxxNS::Test *self = (CxxNS::Test*)DaoCData_GetData(_p[0]->v.cdata);
  self->value = (double) _p[1]->v.d;
}
static void dao_Test_Test( DaoContext *_ctx, DValue *_p[], int _n )
{
	CxxNS::Test *self = CxxNS::Dao_Test_New();
	DaoContext_PutCData( _ctx, self, dao_Test_Typer );
}
/* greeting.h */
static void dao_Test_Print( DaoContext *_ctx, DValue *_p[], int _n )
{
  CxxNS::Test* self= (CxxNS::Test*) DaoCData_GetData( _p[0]->v.cdata );
  self->Test::Print(  );
}

/*  greeting.h */

/*  greeting.h */

#ifdef __cplusplus
}
#endif

