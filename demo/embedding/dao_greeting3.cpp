#include"dao_greeting.h"

extern DaoVmSpace *__daoVmSpace;


void Dao_Get_Object_Method( DaoCData *cd, DaoObject **ob, DaoRoutine **ro, const char *name )
{
  DValue va;
  if( cd == NULL ) return;
  *ob = DaoCData_GetObject( cd );
  if( *ob == NULL ) return;
  va = DaoObject_GetField( *ob, name );
  if( va.t == DAO_ROUTINE ) *ro = va.v.routine;
}

void Function_0x1c3eb0( DaoVmProcess *_vmp, DaoRoutine *_ro, DaoObject *_ob, const char* msg )
{
  const DValue _dao_nil = {0,0,0,0,{0}};
  DValue _dp[1] = { _dao_nil };
  DValue *_dp2[1] = { _dp+0 };
  if( _ro ==NULL || _vmp ==NULL ) return;
  _dp[0] = DValue_NewMBString( (char*) msg, strlen( (char*)msg ) );

  DaoVmProcess_Call( _vmp, _ro, _ob, _dp2, 1 );
  DValue_ClearAll( _dp, 1 );
}



DaoCxx_Greeting* DAO_DLL_GREETING DaoCxx_Greeting_New( const char* msg )
{
	DaoCxx_Greeting *self = new DaoCxx_Greeting( msg );
	self->Init();
	return self;
}
void DaoCxxVirt_Greeting::Init( Greeting *s, DaoCData *d, DaoVmProcess *p )
{
	self = s;
	cdata = d;
	vmproc = p;

}
void DaoCxx_Greeting::Init()
{
	cdata = DaoCData_New( dao_Greeting_Typer, this );
	vmproc = DaoVmProcess_New( __daoVmSpace );
	DaoGC_IncRC( (DaoBase*) vmproc );
	DaoGC_IncRC( (DaoBase*) cdata );
	DaoCxxVirt_Greeting::Init( this, cdata, vmproc );
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
  if( _ro ==NULL || _ob ==NULL || vmproc ==NULL ) return;
  Function_0x1c3eb0( vmproc, _ro, _ob, name );
}
void DaoCxx_Greeting::DoGreeting( const char* name )
{
  DaoObject *_o = DaoCData_GetObject( cdata );
  if( _o && vmproc && DaoObject_GetField( _o, "DoGreeting" ).t ==DAO_ROUTINE ){
     DaoCxxVirt_Greeting::DoGreeting( name );
  }else{
     Greeting::DoGreeting( name );
  }
}

namespace CxxNS{
Test* Dao_Test_New()
{
	Test *self = new Test();
	return self;
}

}
