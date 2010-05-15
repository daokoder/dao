
#include<stdio.h>

class Greeting
{
  int   size;
  char *message;

  public:

  Greeting( const char * msg );
  void SetMessage( const char * msg );
  void PrintMessage();

  // virtual function to be re-implemented by Dao class.
  virtual void DoGreeting( const char *name );

  // This method will invoke g->DoGreeting( name ).
  //
  // When a Dao class is derived from this C++ class,
  // an instance (object) of that Dao class will contain 
  // a parent object that is an instance of a wrapping 
  // C++ class derived from Greeting (see dao_greeting.h).
  //
  // The derived Dao class may re-implement the above
  // virtual method.
  //
  // If g is an parent object of an Dao object,
  // a call to g->DoGreeting( name ) will execute the
  // virtual method re-implemented in Dao.
  void TestGreeting( Greeting *g, const char *name );
};

Greeting* GetGreetingObject();

enum Enum1 { AA, BB, CC };

namespace CxxNS
{
  enum Bool { FALSE, TRUE };
  enum Enum2 { AA, BB, CC };

  typedef Bool Bool2;

  class Test
  {
    public:

    int index;
    double value;

    void Print(){ printf( "%5i: %9f\n", index, value ); }
  };
  /* reference a class from global scope */
  void Testing( Greeting *greeting, Bool bl=FALSE );
  void Testing( int a, Bool2 bl=FALSE );
}

typedef CxxNS::Test Test2;

/* reference to a type and a constant from a namespace */
void Testing( CxxNS::Bool bl=CxxNS::FALSE );

namespace CxxNS2
{
  /* cross referencing to members from another namespace */
  void Testing( CxxNS::Test *test, CxxNS::Bool bl=CxxNS::FALSE );
}
