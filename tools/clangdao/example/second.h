
#ifndef __SECOND_H__
#define __SECOND_H__

#include"first.h"

class SecondClass;

void second_function( SecondClass *obj, int n );

class SecondClass : public FirstClass
{
	public:

	void Meth( int index );
	void Meth( int index, SecondClass *obj );
};

#endif
