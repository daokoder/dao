
#ifndef __FIRST_H__
#define __FIRST_H__

#include<stdlib.h>

#define TEST	(12+34)

typedef float Matrix[4][4];
typedef unsigned int Fl_Shortcut;
Fl_Shortcut fl_old_shortcut(const char*);

class FirstClass
{
	public:
	FirstClass( const FirstClass & other );
	virtual ~FirstClass();

	virtual void FirstVirt( float *vec, int size );
	virtual void FirstVirt( float **mat, int n, int m );
	virtual void FirstVirt( char *ss[3] ); // XXX
	virtual void FirstVirt( float *mat[3], int n ); // XXX
	virtual void FirstVirt( float mat[3][4] );
	virtual void FirstVirt2( Matrix mat )=0;

	int (*callback)( FirstClass *context, int test );

	protected:
	virtual void FirstVirt3( int )=0;
};

void first_function( int abc=123+456 );
void* my_func( char *p = NULL );
void my_func( FirstClass *p );
void my_func2( FirstClass *p = NULL );
int my_func2( int *p, int n=TEST );

typedef int myint;
int my_func3( myint n=TEST );

int my_func( int vec[], int n );
int my_func2( int vec3[3], int n );

#if 0
template<class T>
class vector
{
	public:
	vector();

	virtual void test();
};
void test( vector<int> vec );
#else
#include <vector>
#include <string>
void test2( std::vector<FirstClass> a );
using namespace std;
template<class T>
class MyVector : public vector<T>
{
	public:
	MyVector();

	virtual void test();
};
void test( MyVector<string> vec );
void test( vector<int> a );
void test( vector<FirstClass> a );
void test_string( std::string *s );
namespace std{
void test_string2( string *s );
}
#if 0
#endif
#endif

#endif
