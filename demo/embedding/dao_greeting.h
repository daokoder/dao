#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<dao.h>
#include"greeting.h"
#ifndef DAO_GREETING_STATIC
#ifndef DAO_DLL_GREETING
#define DAO_DLL_GREETING DAO_DLL_EXPORT
#endif
#else
#define DAO_DLL_GREETING
#endif

#ifdef __cplusplus
extern "C"{
#endif

extern DaoTypeBase *dao_Bool_Typer;
extern DaoTypeBase *dao_Greeting_Typer;
extern DaoTypeBase *dao_Test_Typer;
#ifdef __cplusplus
}
#endif

struct DaoCallbackData
{
  DaoRoutine *callback;
  DValue      userdata;
};
DaoCallbackData* DaoCallbackData_New( DaoRoutine *callback, DValue *userdata );


class DAO_DLL_GREETING DaoCxxVirt_Greeting 
{
	public:
	DaoCxxVirt_Greeting(){ self = 0; cdata = 0; vmproc = 0; }
	~DaoCxxVirt_Greeting(){
		DaoGC_DecRC( (DaoBase*) vmproc );
		DaoGC_DecRC( (DaoBase*) cdata );
	}
	void Init( Greeting *self, DaoCData *d, DaoVmProcess *p );
	Greeting *self;
	DaoCData *cdata;
	DaoVmProcess *vmproc;
	void DoGreeting( const char* name );

};
class DAO_DLL_GREETING DaoCxx_Greeting : public Greeting, public DaoCxxVirt_Greeting
{ 
	public:
   DaoCxx_Greeting( const char* msg ) : Greeting( msg ){}
	void Init();
	void DoGreeting( const char* name );
};
Greeting* Dao_Greeting_Copy( const Greeting &p );
DaoCxx_Greeting* DAO_DLL_GREETING DaoCxx_Greeting_New( const char* msg );
namespace CxxNS{

Test* DAO_DLL_GREETING Dao_Test_New();
}
