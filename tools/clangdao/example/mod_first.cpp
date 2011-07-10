// Module name:
#define module_name First
#undef module_name 

// Wrapping hints for function: void* my_func( char *p );
// It means the function may take a null pointer as parameter
// and return a null pointer:
#define my_func_dao_hint_nullable( pp_dao_hint_nullable ) my_func(FirstClass*)
#undef my_func_dao_hint_nullable

// Wrapping hints for function: int my_func2( int *p, int n );
// It means the first parameter is actually an array (and can be null),
// with number of elements specified by the second parameter:
#define dao_my_func2( p_dao_hint_array_n_hint_nullable, n ) my_func2(int*,int)
#undef dao_my_func2

#define dao_FirstClass_FirstVirt( vec_dao_hint_array_size, size ) FirstClass::FirstVirt(float*,int)
#undef dao_FirstClass_FirstVirt

#define dao_FirstClass_FirstVirt( mat_dao_hint_array_n_m, n, m ) FirstClass::FirstVirt(float**,int,int)
#undef dao_FirstClass_FirstVirt

// Basically such macros of function hints will do nothing,
// but their definitions will be used by clangdao to understand
// which functions need fixup to properly handle the parameters
// and/or the returned value.
//
// Such macro should be a function like macro, and defined as the prototype
// of the function that needs fixup.
// The macro name can be anything that hasn't been defined.
// The macro parameters can also be anything.
// But to be useful, one of the parameters or the macro name should contain
// a pattern to to describe a hint.
//
// The basic pattern to describe a hint is: <name>_dao_hint_<thehint>
// <name> can be any name. The first hint in a pattern should follow
// "_dao_hint_", and the extra hints should then follow just "_hint_".
// <thehint> should be description of the hint, such as,
// one word decription "nullable" to mean a parameter may take a NULL 
// pointer as parameter or the functioin may return a NULL pointer.
// The hint can also include multiple words, such as "array_n" for a
// pointer parameter (or returned value), to indicate that, the pointer
// type is actually an array, with "n" number of items, where "n" should
// be the name of one of its parameters.

// Headers to be wrapped:
#include "first.h"

#include <vector>
