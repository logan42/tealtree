#ifndef __tealtree__gheap__
#define __tealtree__gheap__
// gheap has too many internal checks, so running it without NDEBUG makes little sense.
// So we define NDEBUG just for gheap and undefine it right after the header file.

#if !defined(NDEBUG) 
#define UNDEFINE_NDEBUG_AFTER_GHEAP
#endif

#include <gheap.hpp>

#ifdef UNDEFINE_NDEBUG_AFTER_GHEAP
#undef NDEBUG
#endif

#endif
