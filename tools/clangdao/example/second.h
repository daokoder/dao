
#ifndef __SECOND_H__
#define __SECOND_H__

#include"first.h"

#define slots

class SecondClass;

namespace NS{
void second_function( SecondClass *obj, int n );

class Nested{};
};
void second_function( SecondClass *obj, int n );

class SecondClass : public FirstClass
{
	public:
	SecondClass();

	static void Meth( int index );

	void FirstVirt();
	virtual float* Meth2( int index, SecondClass *obj );
	virtual int* Excluded( int excluding[][], void*(*ex)() )const=0;
	public slots:
	void t();
};

#endif
