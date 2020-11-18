/* This benchmark just calls operator new/delete a lot with an average of
 * 512 small objects live at any time. It should be very susceptible to even the
 * tiniest additional overhead between allocators.
 * 
 * This is not a UPC++ benchmark, it isn't even parallel.
 * 
 * Reported dimensions:
 *   Those of:./common/operator_new.hpp. This amounts to the allocator choice
 *   (opnew=std|ltalloc|insane) and whether it was possibly inlined
 *   (opnew_inline=0|1).
 *
 * Reported measurements:
 *   secs: The number of seconds it took to run a fixed number of allocate/deletes.
 * 
 * Compile-time parameters:
 *   See ./common/operator_new.hpp
 */

#include "common/timer.hpp"
#include "common/report.hpp"
#include "common/operator_new.hpp"

using namespace bench;

void *bag[1024] = {};

int main() {
  timer t;
  
  for(unsigned i=0; i < 200<<20; i++) {
    unsigned j = i*0x12345679u >> (32-10);
    if(bag[j]) operator delete(bag[j]);
    bag[j] = operator new(32); // we use a statically known constant
  }
  
  auto secs = t.reset();
  
  report rep(__FILE__);
  
  rep.emit({"secs"}, opnew_row() & column("secs", secs));
  
  return 0;
}
