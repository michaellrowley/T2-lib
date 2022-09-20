#ifndef T2_BASE
#define T2_BASE

// I wouldn't recommend using this file for performant code
// because it eliminates some of the clarity that reviewers
// can get by looking at the includes at the top of a C++
// file and includes way more objects than you're likely to
// actually use.

#include "./net/net.hpp"
#include "./protocols.hpp"
#include "./utility/utility.hpp"

#endif