#include"dao_greeting.h"

#ifdef __cplusplus
extern "C"{
#endif

/*  greeting.h */


static DaoNumItem dao_AutobindTest_Nums[] =
{
  { NULL, 0, 0 }
};
static void dao_AutobindTest_AutobindTest( DaoContext *_ctx, DValue *_p[], int _n );

static DaoFuncItem dao_AutobindTest_Meths[] = 
{
  { dao_AutobindTest_AutobindTest, "AutobindTest(  )=>AutobindTest" },
  { NULL, NULL }
};
static void Dao_AutobindTest_Delete( void *self )
{
	delete (AutobindTest*) self;
}
static DaoTypeBase AutobindTest_Typer = 
{ "AutobindTest", NULL, dao_AutobindTest_Nums, dao_AutobindTest_Meths, 
  { 0 }, Dao_AutobindTest_Delete, NULL };
DaoTypeBase DAO_DLL_GREETING *dao_AutobindTest_Typer = & AutobindTest_Typer;
static void dao_AutobindTest_AutobindTest( DaoContext *_ctx, DValue *_p[], int _n )
{
	AutobindTest *self = Dao_AutobindTest_New();
	DaoContext_PutCData( _ctx, self, dao_AutobindTest_Typer );
}

/*  greeting.h */


static DaoNumItem dao_Bool_Nums[] =
{
  { NULL, 0, 0 }
};

static DaoFuncItem dao_Bool_Meths[] = 
{
  { NULL, NULL }
};
static void Dao_Bool_Delete( void *self )
{
	free( self );
}
static DaoTypeBase Bool_Typer = 
{ "Bool", NULL, dao_Bool_Nums, dao_Bool_Meths, 
  { 0 }, Dao_Bool_Delete, NULL };
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
static void dao_Greeting_TestNull( DaoContext *_ctx, DValue *_p[], int _n );
static void dao_Greeting_VirtWithDefault( DaoContext *_ctx, DValue *_p[], int _n );

static DaoFuncItem dao_Greeting_Meths[] = 
{
  { dao_Greeting_Greeting, "Greeting( msg : string=\'\' )=>Greeting" },
  { dao_Greeting_DoGreeting, "DoGreeting( self : Greeting, name : string )" },
  { dao_Greeting_PrintMessage, "PrintMessage( self : Greeting )" },
  { dao_Greeting_SetMessage, "SetMessage( self : Greeting, msg : string )" },
  { dao_Greeting_TestGreeting, "TestGreeting( self : Greeting, g : Greeting, name : string )" },
  { dao_Greeting_TestNull, "TestNull( self : Greeting, _cp0 : Greeting_Null )=>Greeting_Null" },
  { dao_Greeting_VirtWithDefault, "VirtWithDefault( self : Greeting, g : Greeting =0 )" },
  { NULL, NULL }
};
static void Dao_Greeting_Delete( void *self )
{
	delete (Greeting*) self;
}
static int Dao_Greeting_DelTest( void *self0 )
{
	Greeting *self = (Greeting*) self0;
	return (self!=GetGreetingObject());
}
static DaoTypeBase Greeting_Typer = 
{ "Greeting", NULL, dao_Greeting_Nums, dao_Greeting_Meths, 
  { 0 }, Dao_Greeting_Delete, Dao_Greeting_DelTest };
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
  self->DoGreeting( name );
}
/* greeting.h */
static void dao_Greeting_PrintMessage( DaoContext *_ctx, DValue *_p[], int _n )
{
  Greeting* self= (Greeting*) DaoCData_GetData( _p[0]->v.cdata );
  self->PrintMessage(  );
}
/* greeting.h */
static void dao_Greeting_SetMessage( DaoContext *_ctx, DValue *_p[], int _n )
{
  Greeting* self= (Greeting*) DaoCData_GetData( _p[0]->v.cdata );
  char* msg= (char*) DString_GetMBS( _p[1]->v.s );
  self->SetMessage( msg );
}
/* greeting.h */
static void dao_Greeting_TestGreeting( DaoContext *_ctx, DValue *_p[], int _n )
{
  Greeting* self= (Greeting*) DaoCData_GetData( _p[0]->v.cdata );
  Greeting* g= (Greeting*) DaoCData_GetData( _p[1]->v.cdata );
  char* name= (char*) DString_GetMBS( _p[2]->v.s );
  self->TestGreeting( g, name );
}
/* greeting.h */
static void dao_Greeting_TestNull( DaoContext *_ctx, DValue *_p[], int _n )
{
  Greeting* self= (Greeting*) DaoCData_GetData( _p[0]->v.cdata );
  Greeting::Null* _cp0= (Greeting::Null*) DaoCData_GetData( _p[1]->v.cdata );
  Greeting::Null _TestNull = self->TestNull( *_cp0 );
  DaoContext_PutCData( _ctx, (void*)new Greeting::Null( _TestNull ), dao_Greeting_Null_Typer );
}
/* greeting.h */
static void dao_Greeting_VirtWithDefault( DaoContext *_ctx, DValue *_p[], int _n )
{
  Greeting* self= (Greeting*) DaoCData_GetData( _p[0]->v.cdata );
  Greeting* g= (Greeting*) DaoCData_GetData( _p[1]->v.cdata );
  if(_n<=1) self->VirtWithDefault(  );
  else self->VirtWithDefault( *g );
}

/*  greeting.h */


static DaoNumItem dao_Greeting2_Nums[] =
{
  { NULL, 0, 0 }
};
static void dao_Greeting2_Greeting2( DaoContext *_ctx, DValue *_p[], int _n );

static DaoFuncItem dao_Greeting2_Meths[] = 
{
  { dao_Greeting2_Greeting2, "Greeting2(  )=>Greeting2" },
  { NULL, NULL }
};
static void Dao_Greeting2_Delete( void *self )
{
	delete (Greeting2*) self;
}
static DaoTypeBase Greeting2_Typer = 
{ "Greeting2", NULL, dao_Greeting2_Nums, dao_Greeting2_Meths, 
  { dao_Greeting_Typer, 0 }, Dao_Greeting2_Delete, NULL };
DaoTypeBase DAO_DLL_GREETING *dao_Greeting2_Typer = & Greeting2_Typer;
static void dao_Greeting2_Greeting2( DaoContext *_ctx, DValue *_p[], int _n )
{
	DaoCxx_Greeting2 *self = DaoCxx_Greeting2_New();
	DaoContext_PutResult( _ctx, (DaoBase*) self->cdata );
}

/*  greeting.h */


static DaoNumItem dao_Greeting_Null_Nums[] =
{
  { NULL, 0, 0 }
};

static DaoFuncItem dao_Greeting_Null_Meths[] = 
{
  { NULL, NULL }
};
static void Dao_Greeting_Null_Delete( void *self )
{
	delete (Greeting::Null*) self;
}
static DaoTypeBase Greeting_Null_Typer = 
{ "Greeting_Null", NULL, dao_Greeting_Null_Nums, dao_Greeting_Null_Meths, 
  { 0 }, Dao_Greeting_Null_Delete, NULL };
DaoTypeBase DAO_DLL_GREETING *dao_Greeting_Null_Typer = & Greeting_Null_Typer;

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
static void Dao_Test_Delete( void *self )
{
	delete (CxxNS::Test*) self;
}
static DaoTypeBase Test_Typer = 
{ "Test", NULL, dao_Test_Nums, dao_Test_Meths, 
  { 0 }, Dao_Test_Delete, NULL };
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
  self->Print(  );
}

/*  greeting.h */


static DaoNumItem dao_otto_Nums[] =
{
  { NULL, 0, 0 }
};
static void dao_otto_otto( DaoContext *_ctx, DValue *_p[], int _n );
static void dao_otto_geta( DaoContext *_ctx, DValue *_p[], int _n );

static DaoFuncItem dao_otto_Meths[] = 
{
  { dao_otto_otto, "otto( b : int=123 )=>otto" },
  { dao_otto_geta, "geta( self : otto )=>int" },
  { NULL, NULL }
};
static void Dao_otto_Delete( void *self )
{
	delete (otto*) self;
}
static DaoTypeBase otto_Typer = 
{ "otto", NULL, dao_otto_Nums, dao_otto_Meths, 
  { 0 }, Dao_otto_Delete, NULL };
DaoTypeBase DAO_DLL_GREETING *dao_otto_Typer = & otto_Typer;
/* greeting.h */
static void dao_otto_otto( DaoContext *_ctx, DValue *_p[], int _n )
{
  int b= (int) _p[0]->v.i;
	otto *_self = Dao_otto_New( b );
	DaoContext_PutCData( _ctx, _self, dao_otto_Typer );
}
/* greeting.h */
static void dao_otto_geta( DaoContext *_ctx, DValue *_p[], int _n )
{
  otto* self= (otto*) DaoCData_GetData( _p[0]->v.cdata );
  int _geta = self->geta(  );
  DaoContext_PutInteger( _ctx, (int) _geta );
}

/*  greeting.h */

/*  greeting.h */

#ifdef __cplusplus
}
#endif

