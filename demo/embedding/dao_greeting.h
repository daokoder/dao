#ifndef __DAO_GREETING_H__
#define __DAO_GREETING_H__
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

extern DaoVmSpace *__daoVmSpace;

#ifdef __cplusplus
extern "C"{
#endif

extern DaoTypeBase *dao_AutobindTest_Typer;
extern DaoTypeBase *dao_Bool_Typer;
extern DaoTypeBase *dao_Greeting_Typer;
extern DaoTypeBase *dao_Greeting2_Typer;
extern DaoTypeBase *dao_Greeting_Null_Typer;
extern DaoTypeBase *dao_Test_Typer;
extern DaoTypeBase *dao_otto_Typer;
extern DaoTypeBase *dao_otto2_Typer;
#ifdef __cplusplus
}
#endif


AutobindTest* DAO_DLL_GREETING Dao_AutobindTest_New();

class DAO_DLL_GREETING DaoCxxVirt_Greeting 
{
	public:
	DaoCxxVirt_Greeting(){ self = 0; cdata = 0; }
	void DaoInitWrapper( Greeting *self, DaoCData *d );
	Greeting *self;
	DaoCData *cdata;
	void DoGreeting( const char* name );
	void VirtWithDefault( const Greeting &g );

};
class DAO_DLL_GREETING DaoCxx_Greeting : public Greeting, public DaoCxxVirt_Greeting
{ 
	public:
   DaoCxx_Greeting( const char* msg=NULL  ) : Greeting( msg ){}
	~DaoCxx_Greeting();
	void DaoInitWrapper();
	void DoGreeting( const char* name );
	void VirtWithDefault( const Greeting &g = Greeting() );
};
Greeting* Dao_Greeting_Copy( const Greeting &p );
DaoCxx_Greeting* DAO_DLL_GREETING DaoCxx_Greeting_New( const char* msg );

class DAO_DLL_GREETING DaoCxxVirt_Greeting2  : public DaoCxxVirt_Greeting
{
	public:
	DaoCxxVirt_Greeting2(){ self = 0; cdata = 0; }
	void DaoInitWrapper( Greeting2 *self, DaoCData *d );
	Greeting2 *self;
	DaoCData *cdata;
	void DoGreeting( const char* name );
	void VirtWithDefault( const Greeting &g );

};
class DAO_DLL_GREETING DaoCxx_Greeting2 : public Greeting2, public DaoCxxVirt_Greeting2
{ 
	public:
	~DaoCxx_Greeting2();
	void DaoInitWrapper();
	void DoGreeting( const char* name );
	void VirtWithDefault( const Greeting &g = Greeting() );
};
Greeting2* Dao_Greeting2_Copy( const Greeting2 &p );
DaoCxx_Greeting2* DAO_DLL_GREETING DaoCxx_Greeting2_New(  );
namespace CxxNS{

Test* DAO_DLL_GREETING Dao_Test_New();
}

class DAO_DLL_GREETING DaoCxxVirt_otto 
{
	public:
	DaoCxxVirt_otto(){ self = 0; cdata = 0; }
	void DaoInitWrapper( otto *self, DaoCData *d );
	otto *self;
	DaoCData *cdata;
	otto test( const otto &value );

};
class DAO_DLL_GREETING DaoCxx_otto : public otto, public DaoCxxVirt_otto
{ 
	public:
   DaoCxx_otto( int b=123 ) : otto( b ){}
	~DaoCxx_otto();
	void DaoInitWrapper();
	otto test( const otto &value );
	otto DaoWrap_test( const otto &value ){ return otto::test( value ); }
};
otto* Dao_otto_Copy( const otto &p );
DaoCxx_otto* DAO_DLL_GREETING DaoCxx_otto_New( int b );

class DAO_DLL_GREETING DaoCxxVirt_otto2  : public DaoCxxVirt_otto
{
	public:
	DaoCxxVirt_otto2(){ self = 0; cdata = 0; }
	void DaoInitWrapper( otto2 *self, DaoCData *d );
	otto2 *self;
	DaoCData *cdata;
	otto test( const otto &value );

};
class DAO_DLL_GREETING DaoCxx_otto2 : public otto2, public DaoCxxVirt_otto2
{ 
	public:
	~DaoCxx_otto2();
	void DaoInitWrapper();
	otto test( const otto &value );
};
otto2* Dao_otto2_Copy( const otto2 &p );
DaoCxx_otto2* DAO_DLL_GREETING DaoCxx_otto2_New(  );
#endif
