
#ifndef __SECOND_H__
#define __SECOND_H__

#include"first.h"

class SecondClass;

namespace NS{
void second_function( SecondClass *obj, int n );
};

class SecondClass : public FirstClass
{
	public:
	SecondClass();

	static void Meth( int index );

	virtual float* Meth2( int index, SecondClass *obj );
};

#endif
