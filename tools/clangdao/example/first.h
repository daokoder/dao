
#ifndef __FIRST_H__
#define __FIRST_H__

#include<stdlib.h>

#define TEST	(12+34)

class FirstClass
{
};

void first_function( int abc=123+456 );
void* my_func( char *p = NULL );
int my_func2( int *p, int n=TEST );

typedef int myint;
int my_func3( myint n=TEST );

int my_func( int vec[], int n );
int my_func2( int vec3[3], int n );

#endif
