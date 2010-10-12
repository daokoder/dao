#include"dao_greeting.h"

void Dao_Get_Object_Method( DaoCData *cd, DaoObject **ob, DaoRoutine **ro, const char *name )
{
  DValue va;
  if( cd == NULL ) return;
  *ob = DaoCData_GetObject( cd );
  if( *ob == NULL ) return;
  va = DaoObject_GetField( *ob, name );
  if( va.t == DAO_ROUTINE ) *ro = va.v.routine;
}

void Function_0x628570( DaoRoutine *_ro, DaoObject *_ob, const Greeting &g )
{
  const DValue _dao_nil = {0,0,0,0,{0}};
  DValue _dp[1] = { _dao_nil };
  DValue *_dp2[1] = { _dp+0 };
  if( _ro == NULL ) return;
  _dp[0] = DValue_NewCData( dao_Greeting_Typer, (void*) & g );

  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );
  DaoVmProcess_Call( _vmp, _ro, _ob, _dp2, 1 );
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );
  DValue_ClearAll( _dp, 1 );
}

void Function_0x61dab0( DaoRoutine *_ro, DaoObject *_ob, const char* msg )
{
  const DValue _dao_nil = {0,0,0,0,{0}};
  DValue _dp[1] = { _dao_nil };
  DValue *_dp2[1] = { _dp+0 };
  if( _ro == NULL ) return;
  _dp[0] = DValue_NewMBString( (char*) msg, strlen( (char*)msg ) );

  DaoVmProcess *_vmp = DaoVmSpace_AcquireProcess( __daoVmSpace );
  DaoVmProcess_Call( _vmp, _ro, _ob, _dp2, 1 );
  DaoVmSpace_ReleaseProcess( __daoVmSpace, _vmp );
  DValue_ClearAll( _dp, 1 );
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
		for(; refCount>0; refCount--) DaoGC_DecRC( (DaoBase*)cdata );
	} 
}
void DaoCxx_Greeting::DaoInitWrapper()
{
	cdata = DaoCData_New( dao_Greeting_Typer, this );
	DaoCxxVirt_Greeting::DaoInitWrapper( this, cdata );
}
void DaoCxx_Greeting::DaoAddExternalReference()
{
	refCount += 1;
	DaoGC_IncRC( (DaoBase*) cdata );
}
Greeting* DAO_DLL_GREETING Dao_Greeting_Copy( const Greeting &p )
{
	Greeting *object = new Greeting( p );
	return object;
}
void DaoCxxVirt_Greeting::DoGreeting( const char* name )
{
  DaoObject *_ob = NULL;
  DaoRoutine *_ro = NULL;
  Dao_Get_Object_Method( cdata, & _ob, & _ro, "DoGreeting" );
  if( _ro ==NULL || _ob ==NULL ) return;
  Function_0x61dab0( _ro, _ob, name );
}
void DaoCxxVirt_Greeting::VirtWithDefault( const Greeting &g )
{
  DaoObject *_ob = NULL;
  DaoRoutine *_ro = NULL;
  Dao_Get_Object_Method( cdata, & _ob, & _ro, "VirtWithDefault" );
  if( _ro ==NULL || _ob ==NULL ) return;
  Function_0x628570( _ro, _ob, g );
}
void DaoCxx_Greeting::DoGreeting( const char* name )
{
  DaoObject *_o = DaoCData_GetObject( cdata );
  if( _o && DaoObject_GetField( _o, "DoGreeting" ).t ==DAO_ROUTINE ){
     DaoCxxVirt_Greeting::DoGreeting( name );
  }else{
     Greeting::DoGreeting( name );
  }
}
void DaoCxx_Greeting::VirtWithDefault( const Greeting &g )
{
  DaoObject *_o = DaoCData_GetObject( cdata );
  if( _o && DaoObject_GetField( _o, "VirtWithDefault" ).t ==DAO_ROUTINE ){
     DaoCxxVirt_Greeting::VirtWithDefault( g );
  }else{
     Greeting::VirtWithDefault( g );
  }
}


namespace CxxNS{
Test* Dao_Test_New()
{
	Test *self = new Test();
	return self;
}

}
