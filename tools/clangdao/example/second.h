
#ifndef __SECOND_H__
#define __SECOND_H__

#include"first.h"

#define slots

class SecondClass;

namespace NS{
	void second_function( SecondClass *obj, int n );

	class Nested{};

	namespace NS2{ class Nested{}; }

	enum Enum1{ AA1 };
};

typedef int INT;
void second_function( SecondClass *obj, INT n, bool bl );
void second_function2( SecondClass *obj, INT n, NS::Enum1 e=NS::AA1 );
void second_function2( SecondClass *obj, INT n, NS::Enum1 *e );

class SecondClass : public FirstClass
{
	public:
	SecondClass();

	static void Meth( int index );

	void FirstVirt();
	virtual float* Meth2( int index, SecondClass *obj );
	virtual int* Excluded( int excluding[][], void*(*ex)() )const=0;
	virtual int Excluded( void*(*callback)() )const=0;

	virtual void Test( NS::Enum1 & e1, NS::Enum1 *e2 );
	public slots:
	void t();
};
//void second_function2( const SecondClass & obj = SecondClass() );
//void second_function2( const FirstClass & obj = FirstClass() );

#endif
