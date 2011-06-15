// Module name:
#define module_name First
#undef module_name 

// wrapping hints for function: void* my_func( char *p );
// it means the function may take a null pointer as parameter
// and return a null pointer:
#define my_func_hint_nullable( p_dao_hint_nullable ) my_func(char*)
#undef my_func_hint_nullable

// wrapping hints for function: int my_func2( int *p, int n );
// it means the first parameter is actually an array (and can be null),
// with number of elements specified by the second parameter:
#define dao_my_func2( p_dao_hint_array_n_hint_nullable, n ) my_func2(int*,int)
#undef dao_my_func2

// Headers to be wrapped:
#include "first.h"
