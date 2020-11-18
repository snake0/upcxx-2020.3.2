#ifndef _572e1a8a_95cb_4968_808c_69661ac8b133
#define _572e1a8a_95cb_4968_808c_69661ac8b133

#include <new>

#include "report.hpp"

// Include this header only from the "main" translation unit since we provide
// definitions of operator new/delete.
//
// Run with OPNEW=0,1,2 in environment during nobs compile:
//  0 standard operator new/delete:
//    (a.k.a malloc/free). As of 2017 this is pretty good since glibc received
//    fast hread caching for small objects.
//
//  1 ltalloc:
//    Seems like a pretty good general purpose operator new/delete replacement
//    that handles lots of small objects very well. I can't imagine us being
//    able to do any better than this. But, if we do see a gap between insane
//    and ltalloc maybe we can consider providing our own.
//
//  2 "insane" allocator:
//    This allocator is not thread safe and does not support sizes > 8K, but if
//    those global program constraints are met it should be the fastest thing
//    possible. If we can't show marked improvements using this allocator over
//    other off the shelf ones, then I don't think we need to bother writing  
//    our own allocator.
//
// OPNEW_INLINE controls whether the operator new/del replacement definitions
// will occur directly in the header or in a separate source file.  Because of
// this, this header can only be included by one translation unit in a program,
// otherwise the linker would suffer redundant definitions. Benchmarks are typically
// all in one TU, so having the replacements inlineable in that TU should catch
// most opportunities for allocator inlining (esp since most upcxx allocations
// are done in inlineable header code).

#ifndef OPNEW
# define OPNEW 0
#endif

#ifndef OPNEW_INLINE
# define OPNEW_INLINE 0
#endif

#if OPNEW == 0 // std

#elif OPNEW == 1 // ltalloc
  #if OPNEW_INLINE
    #include "ltalloc.cc"
  #else
    #include "ltalloc.h"
  #endif

#elif OPNEW == 2 // insane
  #if OPNEW_INLINE
    #include "operator_new_insane.cpp"
  #else
    #include "operator_new_insane.hpp"
  #endif
#endif

constexpr bench::row<const char*, int> opnew_row() {
  return bench::column("opnew",
      OPNEW == 0 ? "std" :
      OPNEW == 1 ? "ltalloc" :
      OPNEW == 2 ? "insane" :
      "?"
    )
    & bench::column("opnew_inline", OPNEW_INLINE);
}

#endif
