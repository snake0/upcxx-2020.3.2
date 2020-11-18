#include <upcxx/upcxx.hpp>
#include <iostream>
#include <assert.h>

int main() {
  upcxx::init();

  upcxx::global_ptr<int> gp(nullptr);

  assert(gp.is_local());
 
  upcxx::barrier();
  if (!upcxx::rank_me()) std::cout<<"SUCCESS"<<std::endl;
  
  upcxx::finalize();
  return 0;
}
