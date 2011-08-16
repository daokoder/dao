#include"dao_Greeting.h"
DaoMethod* Dao_Get_Object_Method( DaoCData *cd, DaoObject **obj, const char *name )
{
  DaoMethod *meth;
  if( cd == NULL ) return NULL;
  *obj = DaoCData_GetObject( cd );
  if( *obj == NULL ) return NULL;
  meth = DaoObject_GetMethod( *obj, name );
  if( meth == NULL ) return NULL;
  if( DaoValue_CastFunction( (DaoValue*)meth ) ) return NULL; /*do not call C function*/
  return meth;
}
static void DaoPF10001( int *_cs, DaoMethod *_ro, DaoObject *_ob )
{
  if( _ro == NULL ) return;
  _ro = DaoMethod_Resolve( _ro, (DaoValue*)_ob, NULL, 0 );
  if( DaoValue_CastRoutine( (DaoValue*)_ro ) == NULL ) return;
  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );
  *_cs = DaoVmProcess_Call( _vmp, _ro, (DaoValue*)_ob, NULL, 0 );
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );
}
static void DaoPF10003( int *_cs, DaoMethod *_ro, DaoObject *_ob, const Greeting& g )
{
  DaoValue *_dp[1] = { NULL };
  if( _ro == NULL ) return;
  _dp[0] = DaoValue_WrapCData( dao_Greeting_Typer, (void*) &g );

  _ro = DaoMethod_Resolve( _ro, (DaoValue*)_ob, _dp, 1 );
  if( DaoValue_CastRoutine( (DaoValue*)_ro ) == NULL ) return;
  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );
  *_cs = DaoVmProcess_Call( _vmp, _ro, (DaoValue*)_ob, _dp, 1 );
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );
  DaoValue_ClearAll( _dp, 1 );
}

DaoCxx_otto* DAO_DLL_GREETING DaoCxx_otto_New( int b )
{
	DaoCxx_otto *self = new DaoCxx_otto( b );
	self->DaoInitWrapper();
	return self;
}
void DaoCxxVirt_otto::vtest( int &_cs  )
{
  DaoObject *_obj = NULL;
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "vtest" );
  if( _ro == NULL || _obj == NULL ) return;
  _ro = DaoMethod_Resolve( _ro, (DaoValue*)_obj, NULL, 0 );
  if( DaoValue_CastRoutine( (DaoValue*)_ro ) == NULL ) return;
  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );
  DaoVmProcess_Call( _vmp, _ro, (DaoValue*)_obj, NULL, 0 );
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );
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
void DaoCxx_otto::vtest(  )
{
  int _cs = 0;
  DaoObject *_obj = NULL;
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "vtest" );
  if( _ro && _obj ){
    ((DaoCxxVirt_otto*)this)->DaoCxxVirt_otto::vtest( _cs  );
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
void DaoCxxVirt_otto2::vtest( int &_cs  )
{
  DaoObject *_obj = NULL;
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "vtest" );
  if( _ro == NULL || _obj == NULL ) return;
  _ro = DaoMethod_Resolve( _ro, (DaoValue*)_obj, NULL, 0 );
  if( DaoValue_CastRoutine( (DaoValue*)_ro ) == NULL ) return;
  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );
  DaoVmProcess_Call( _vmp, _ro, (DaoValue*)_obj, NULL, 0 );
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );
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
void DaoCxx_otto2::vtest(  )
{
  int _cs = 0;
  DaoObject *_obj = NULL;
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "vtest" );
  if( _ro && _obj ){
    ((DaoCxxVirt_otto2*)this)->DaoCxxVirt_otto2::vtest( _cs  );
    if( _cs ) return;
  }
  otto2::vtest(  );
}

DaoCxx_Greeting* DAO_DLL_GREETING DaoCxx_Greeting_New( const char* msg )
{
	DaoCxx_Greeting *self = new DaoCxx_Greeting( msg );
	self->DaoInitWrapper();
	return self;
}
void DaoCxxVirt_Greeting::DoGreeting( int &_cs, const char* name )
{
  DaoObject *_obj = NULL;
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "DoGreeting" );
  if( _ro == NULL || _obj == NULL ) return;
  ( & _cs, _ro, _obj, name );
}
void DaoCxxVirt_Greeting::VirtWithDefault( int &_cs, const Greeting& g )
{
  DaoObject *_obj = NULL;
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "VirtWithDefault" );
  if( _ro == NULL || _obj == NULL ) return;
  DaoPF10003( & _cs, _ro, _obj, g );
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
void DaoCxx_Greeting::DoGreeting( const char* name )
{
  int _cs = 0;
  DaoObject *_obj = NULL;
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "DoGreeting" );
  if( _ro && _obj ){
    ((DaoCxxVirt_Greeting*)this)->DaoCxxVirt_Greeting::DoGreeting( _cs, name );
    if( _cs ) return;
  }
  Greeting::DoGreeting( name );
}
void DaoCxx_Greeting::VirtWithDefault( const Greeting& g )
{
  int _cs = 0;
  DaoObject *_obj = NULL;
  DaoMethod *_ro = Dao_Get_Object_Method( cdata, & _obj, "VirtWithDefault" );
  if( _ro && _obj ){
    ((DaoCxxVirt_Greeting*)this)->DaoCxxVirt_Greeting::VirtWithDefault( _cs, g );
    if( _cs ) return;
  }
  Greeting::VirtWithDefault( g );
}
Greeting::Null* Dao_Greeting_0_Null_New()
{
	Greeting::Null *self = new Greeting::Null();
	return self;
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
AutobindTest* Dao_AutobindTest_New()
{
	AutobindTest *self = new AutobindTest();
	return self;
}
CxxNS::Test* Dao_CxxNS_0_Test_New()
{
	CxxNS::Test *self = new CxxNS::Test();
	return self;
}
