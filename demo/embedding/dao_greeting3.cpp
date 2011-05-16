#include"dao_greeting.h"

DaoMethod* Dao_Get_Object_Method( DaoCData *cd, DValue *obj, const char *name )
{
  DaoMethod *meth;
  if( cd == NULL ) return NULL;
  obj->v.object = DaoCData_GetObject( cd );
  if( obj->v.object == NULL ) return NULL;
  obj->t = DAO_OBJECT;
  meth = DaoObject_GetMethod( obj->v.object, name );
  if( meth == NULL ) return NULL;
  if( meth->type != DAO_METAROUTINE && meth->type != DAO_ROUTINE ) return NULL;
  return meth;
}

static otto Function_10007( int *_cs, DaoMethod *_ro, DValue *_ob, const otto &value )
{
  const DValue _dao_nil = {0,0,0,0,{0}};
  DValue _dp[1] = { _dao_nil };
  DValue *_dp2[1] = { _dp+0 };
  DValue _res;
  DaoCData *_cd;
  DaoVmProcess *_vmp;
  otto _test;
  if( _ro == NULL ) goto EndCall;
  _dp[0] = DValue_WrapCData( dao_otto_Typer, (void*) & value );

  _ro = DaoMethod_Resolve( _ro, _ob, _dp2, 1 );
  if( _ro == NULL || _ro->type != DAO_ROUTINE ) goto EndCall;
  _vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );
  if( (*_cs = DaoVmProcess_Call( _vmp, _ro, _ob, _dp2, 1 )) ==0 ) goto EndCall;
  _res = DaoVmProcess_GetReturned( _vmp );
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );
  if( _res.t == DAO_OBJECT && (_cd = DaoObject_MapCData( _res.v.object, dao_otto_Typer ) ) ){
    _res.t = DAO_CDATA;
    _res.v.cdata = _cd;
  }
  if( _res.t == DAO_CDATA && DaoCData_IsType( _res.v.cdata, dao_otto_Typer ) ){
    _test = *(otto*) DValue_CastCData( &_res, dao_otto_Typer );
  }

EndCall:
  DValue_ClearAll( _dp, 1 );
  return _test;
}

static void Function_10003( int *_cs, DaoMethod *_ro, DValue *_ob )
{
  if( _ro == NULL ) return;
  _ro = DaoMethod_Resolve( _ro, _ob, NULL, 0 );
  if( _ro == NULL || _ro->type != DAO_ROUTINE ) return;
  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );
  *_cs = DaoVmProcess_Call( _vmp, _ro, _ob, NULL, 0 );
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );
}

static void Function_10005( int *_cs, DaoMethod *_ro, DValue *_ob, const Greeting &g )
{
  const DValue _dao_nil = {0,0,0,0,{0}};
  DValue _dp[1] = { _dao_nil };
  DValue *_dp2[1] = { _dp+0 };
  if( _ro == NULL ) return;
  _dp[0] = DValue_WrapCData( dao_Greeting_Typer, (void*) & g );

  _ro = DaoMethod_Resolve( _ro, _ob, _dp2, 1 );
  if( _ro == NULL || _ro->type != DAO_ROUTINE ) return;
  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );
  *_cs = DaoVmProcess_Call( _vmp, _ro, _ob, _dp2, 1 );
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );
  DValue_ClearAll( _dp, 1 );
}

static void Function_10002( int *_cs, DaoMethod *_ro, DValue *_ob, const char* msg )
{
  const DValue _dao_nil = {0,0,0,0,{0}};
  DValue _dp[1] = { _dao_nil };
  DValue *_dp2[1] = { _dp+0 };
  if( _ro == NULL ) return;
  _dp[0] = DValue_NewMBString( (char*) msg, strlen( (char*)msg ) );

  _ro = DaoMethod_Resolve( _ro, _ob, _dp2, 1 );
  if( _ro == NULL || _ro->type != DAO_ROUTINE ) return;
  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );
  *_cs = DaoVmProcess_Call( _vmp, _ro, _ob, _dp2, 1 );
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );
  DValue_ClearAll( _dp, 1 );
}

AutobindTest* Dao_AutobindTest_New()
{
	AutobindTest *self = new AutobindTest();
	return self;
}



DaoCxx_Greeting* DAO_DLL_GREETING DaoCxx_Greeting_New( const char* msg )
{
	DaoCxx_Greeting *self = new DaoCxx_Greeting( msg );
	self->DaoInitWrapper();
	return self;
}
void DaoCxxVirt_Greeting::DaoInitWrapper( Greeting *s, DaoCData *d )
{
	self = s;
	cdata = d;

}
DaoCxx_Greeting::~DaoCxx_Greeting()
{
	if( cdata ){
		DaoCData_SetData( cdata, NULL );
		DaoCData_SetExtReference( cdata, 0 );
	} 
}
void DaoCxx_Greeting::DaoInitWrapper()
{
	cdata = DaoCData_New( dao_Greeting_Typer, this );
	DaoCxxVirt_Greeting::DaoInitWrapper( this, cdata );
}
Greeting* DAO_DLL_GREETING Dao_Greeting_Copy( const Greeting &p )
{
	Greeting *object = new Greeting( p );
	return object;
}
void DaoCxxVirt_Greeting::DoGreeting( int &_cs, const char* name )
{
  DValue _obj = {0,0,0,0,{0}};
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "DoGreeting" );
  if( _ro ==NULL || _obj.t != DAO_OBJECT ) return;
  Function_10002( & _cs, _ro, & _obj, name );
}
void DaoCxxVirt_Greeting::VirtWithDefault( int &_cs, const Greeting &g )
{
  DValue _obj = {0,0,0,0,{0}};
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "VirtWithDefault" );
  if( _ro ==NULL || _obj.t != DAO_OBJECT ) return;
  Function_10005( & _cs, _ro, & _obj, g );
}
void DaoCxx_Greeting::DoGreeting( const char* name )
{
  int _cs = 0;
  DValue _obj = {0,0,0,0,{0}};
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "DoGreeting" );
  if( _ro && _obj.t == DAO_OBJECT ){
    DaoCxxVirt_Greeting::DoGreeting( _cs, name );
	if( _cs ) return;
  }
  Greeting::DoGreeting( name );
}
void DaoCxx_Greeting::VirtWithDefault( const Greeting &g )
{
  int _cs = 0;
  DValue _obj = {0,0,0,0,{0}};
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "VirtWithDefault" );
  if( _ro && _obj.t == DAO_OBJECT ){
    DaoCxxVirt_Greeting::VirtWithDefault( _cs, g );
	if( _cs ) return;
  }
  Greeting::VirtWithDefault( g );
}


DaoCxx_Greeting2* DAO_DLL_GREETING DaoCxx_Greeting2_New(  )
{
	DaoCxx_Greeting2 *self = new DaoCxx_Greeting2(  );
	self->DaoInitWrapper();
	return self;
}
void DaoCxxVirt_Greeting2::DaoInitWrapper( Greeting2 *s, DaoCData *d )
{
	self = s;
	cdata = d;
	DaoCxxVirt_Greeting::DaoInitWrapper( s, d );
}
DaoCxx_Greeting2::~DaoCxx_Greeting2()
{
	if( cdata ){
		DaoCData_SetData( cdata, NULL );
		DaoCData_SetExtReference( cdata, 0 );
	} 
}
void DaoCxx_Greeting2::DaoInitWrapper()
{
	cdata = DaoCData_New( dao_Greeting2_Typer, this );
	DaoCxxVirt_Greeting2::DaoInitWrapper( this, cdata );
}
Greeting2* DAO_DLL_GREETING Dao_Greeting2_Copy( const Greeting2 &p )
{
	Greeting2 *object = new Greeting2( p );
	return object;
}
void DaoCxxVirt_Greeting2::DoGreeting( int &_cs, const char* name )
{
   DaoCxxVirt_Greeting::DoGreeting( _cs, name );
}
void DaoCxxVirt_Greeting2::VirtWithDefault( int &_cs, const Greeting &g )
{
   DaoCxxVirt_Greeting::VirtWithDefault( _cs, g );
}
void DaoCxx_Greeting2::DoGreeting( const char* name )
{
  int _cs = 0;
  DValue _obj = {0,0,0,0,{0}};
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "DoGreeting" );
  if( _ro && _obj.t == DAO_OBJECT ){
    DaoCxxVirt_Greeting2::DoGreeting( _cs, name );
	if( _cs ) return;
  }
  Greeting::DoGreeting( name );
}
void DaoCxx_Greeting2::VirtWithDefault( const Greeting &g )
{
  int _cs = 0;
  DValue _obj = {0,0,0,0,{0}};
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "VirtWithDefault" );
  if( _ro && _obj.t == DAO_OBJECT ){
    DaoCxxVirt_Greeting2::VirtWithDefault( _cs, g );
	if( _cs ) return;
  }
  Greeting::VirtWithDefault( g );
}


namespace CxxNS{
Test* Dao_Test_New()
{
	Test *self = new Test();
	return self;
}

}

DaoCxx_otto* DAO_DLL_GREETING DaoCxx_otto_New( int b )
{
	DaoCxx_otto *self = new DaoCxx_otto( b );
	self->DaoInitWrapper();
	return self;
}
void DaoCxxVirt_otto::DaoInitWrapper( otto *s, DaoCData *d )
{
	self = s;
	cdata = d;

}
DaoCxx_otto::~DaoCxx_otto()
{
	if( cdata ){
		DaoCData_SetData( cdata, NULL );
		DaoCData_SetExtReference( cdata, 0 );
	} 
}
void DaoCxx_otto::DaoInitWrapper()
{
	cdata = DaoCData_New( dao_otto_Typer, this );
	DaoCxxVirt_otto::DaoInitWrapper( this, cdata );
}
otto* DAO_DLL_GREETING Dao_otto_Copy( const otto &p )
{
	otto *object = new otto( p );
	return object;
}
otto DaoCxxVirt_otto::test( int &_cs, const otto &value )
{
  DValue _obj = {0,0,0,0,{0}};
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "test" );
  otto _test;
  if( _ro ==NULL || _obj.t != DAO_OBJECT ) return _test;
  return (otto)Function_10007( & _cs, _ro, & _obj, value );
}
void DaoCxxVirt_otto::vtest( int &_cs  )
{
  DValue _obj = {0,0,0,0,{0}};
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "vtest" );
  if( _ro ==NULL || _obj.t != DAO_OBJECT ) return;
  _ro = DaoMethod_Resolve( _ro, & _obj, NULL, 0 );
  if( _ro == NULL || _ro->type != DAO_ROUTINE ) return;
  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );
  DaoVmProcess_Call( _vmp, _ro, & _obj, NULL, 0 );
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );
}
otto DaoCxx_otto::test( const otto &value )
{
  int _cs = 0;
  DValue _obj = {0,0,0,0,{0}};
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "test" );
  if( _ro && _obj.t == DAO_OBJECT ){
    otto _test = DaoCxxVirt_otto::test( _cs, value );
	if( _cs ) return _test;
  }
  return otto::test( value );
}
void DaoCxx_otto::vtest(  )
{
  int _cs = 0;
  DValue _obj = {0,0,0,0,{0}};
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "vtest" );
  if( _ro && _obj.t == DAO_OBJECT ){
    DaoCxxVirt_otto::vtest( _cs  );
	if( _cs ) return;
  }
  otto::vtest(  );
}


DaoCxx_otto2* DAO_DLL_GREETING DaoCxx_otto2_New(  )
{
	DaoCxx_otto2 *self = new DaoCxx_otto2(  );
	self->DaoInitWrapper();
	return self;
}
void DaoCxxVirt_otto2::DaoInitWrapper( otto2 *s, DaoCData *d )
{
	self = s;
	cdata = d;
	DaoCxxVirt_otto::DaoInitWrapper( s, d );
}
DaoCxx_otto2::~DaoCxx_otto2()
{
	if( cdata ){
		DaoCData_SetData( cdata, NULL );
		DaoCData_SetExtReference( cdata, 0 );
	} 
}
void DaoCxx_otto2::DaoInitWrapper()
{
	cdata = DaoCData_New( dao_otto2_Typer, this );
	DaoCxxVirt_otto2::DaoInitWrapper( this, cdata );
}
otto2* DAO_DLL_GREETING Dao_otto2_Copy( const otto2 &p )
{
	otto2 *object = new otto2( p );
	return object;
}
otto DaoCxxVirt_otto2::test( int &_cs, const otto &value )
{
  return DaoCxxVirt_otto::test( _cs, value );
}
otto DaoCxx_otto2::test( const otto &value )
{
  int _cs = 0;
  DValue _obj = {0,0,0,0,{0}};
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "test" );
  if( _ro && _obj.t == DAO_OBJECT ){
    otto _test = DaoCxxVirt_otto2::test( _cs, value );
	if( _cs ) return _test;
  }
  return otto::test( value );
}
void DaoCxx_otto2::vtest(  )
{
  int _cs = 0;
  DValue _obj = {0,0,0,0,{0}};
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "vtest" );
  if( _ro && _obj.t == DAO_OBJECT ){
    DaoCxxVirt_otto::vtest( _cs  );
	if( _cs ) return;
  }
  otto2::vtest(  );
}

