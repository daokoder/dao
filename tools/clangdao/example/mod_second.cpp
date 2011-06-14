// Module name:
#define module_name Second
#undef module_name

// The "Second" module will require the "First" module:
#include "mod_first.cpp"

// Headers to be wrapped:
#include "second.h"
