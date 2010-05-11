
#include<stdio.h>

// If you want to use direct APIs of Dao, define the following
// preprocessor option before EVERY including of dao.h.
// Or add it in your Makefile or project compiling settings.
//#define DAO_DIRECT_API
#include<dao.h>

static void dao_FakeNumber_FakeNumber( DaoContext *_ctx, DValue *_p[], int _n );
static void dao_FakeNumber_AddSubMulDiv( DaoContext *_ctx, DValue *_p[], int _n );

static DaoFuncItem dao_FakeNumber_Meths[] = 
{
  { dao_FakeNumber_FakeNumber, "FakeNumber()=>FakeNumber" },
  { dao_FakeNumber_AddSubMulDiv, "+( a : FakeNumber, b : FakeNumber, op=0 )" },
  { dao_FakeNumber_AddSubMulDiv, "-( a : FakeNumber, b : FakeNumber, op=1 )" },
  { dao_FakeNumber_AddSubMulDiv, "*( a : FakeNumber, b : FakeNumber, op=2 )" },
  { dao_FakeNumber_AddSubMulDiv, "/( a : FakeNumber, b : FakeNumber, op=3 )" },
  { NULL, NULL }
};
static void Dao_FakeNumber_Delete( void *self ){}
static DaoTypeBase FakeNumber_Typer = 
{ NULL, "FakeNumber", NULL, dao_FakeNumber_Meths, {0}, NULL, Dao_FakeNumber_Delete };
DaoTypeBase *dao_FakeNumber_Typer = & FakeNumber_Typer;

static void dao_FakeNumber_FakeNumber( DaoContext *_ctx, DValue *_p[], int _n )
{
  DaoContext_PutCData( _ctx, NULL, dao_FakeNumber_Typer );
}
static void dao_FakeNumber_AddSubMulDiv( DaoContext *_ctx, DValue *_p[], int _n )
{
  DaoContext_PutCData( _ctx, NULL, dao_FakeNumber_Typer );
  printf( "Arithmetic on two numbers (a fake). opcode: %i\n", _p[2]->v.i );
}

const char* dao_source = 
"f1 = FakeNumber()\n"
"f2 = FakeNumber()\n"
"f3 = f1 + f2\n"
"f3 = f1 - f2\n"
"f3 = f1 * f2\n"
"f3 = f1 / f2\n"
"class FakeNumber2 : FakeNumber{ routine FakeNumber2() : FakeNumber(){} }"
"ff1 = FakeNumber2()\n"
"ff2 = FakeNumber2()\n"
"ff3 = ff1 + ff2\n"
;

int main( int argc, char *argv[] )
{
  DString *src;
  DaoVmSpace *vms;
  DaoNameSpace *ns;
  DaoVmProcess *vmp;

  // Search and load the Dao library.
  // DaoInitLibrary() can take a parameter which is the path
  // to the dynamic loading file of the Dao library.
  // If the parameter is NULL, the current path is searched,
  // then the path defined by environment variable DAO_DIR,
  // then $(HOME)/dao, and then the default system path:
  // /usr/local/dao/ or C:\dao\.
  //
  // With direct APIs, the example must be linked against the Dao library.
  // So if direct APIs are used, the following call is not necessary.
#ifndef DAO_DIRECT_API
  if( DaoInitLibrary( NULL ) ==0 ) return 1;
#endif

  // Initialize Dao library, and get the default DaoVmSpace object.
  // DaoVmSpace is responsible for handling interpreter settings,
  // paths and module loading etc. It is need to create several
  // other types of objects.
  vms = DaoInit();

  // Get the main namespace of an DaoVmSpace object.
  // You can also call DaoNameSpace_New( vms ) to create one.
  ns  = DaoVmSpace_MainNameSpace( vms );

  // Get the main virtual machine process of an DaoVmSpace object.
  // You can also call DaoVmProcess_New( vms ) to create one.
  vmp = DaoVmSpace_MainVmProcess( vms );

  // Prepare the Dao source codes:
  src = DString_New(1);
  DString_SetMBS( src, dao_source );

  // Call the entry function to import the type wrapping FakeNumber
  // into the namespace ns.
  //
  // Calling to this function is not necessary, if and only if
  // the wrapping codes are compiled as dynamic loading library,
  // and there is a proper load statement in the Dao codes.
  //
  // Here the wrapping codes are compiled together with this
  // example, so this entry function must be called:
  DaoNameSpace_AddType( ns, dao_FakeNumber_Typer, 1 );

  // Execute the Dao scripts:
  // Since the wrapped functions and types are imported into
  // namespace ns, it is need to access the wrapped functions and types
  // in the Dao scripts when it is executed:
  DaoVmProcess_Eval( vmp, ns, src, 1 );

  DString_Delete( src );
  DaoQuit(); // Finalizing
  return 0;
}
