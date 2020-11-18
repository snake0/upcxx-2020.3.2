#include <upcxx/backend.hpp>
#include <upcxx/barrier.hpp>
#include <upcxx/rpc.hpp>

#include <iostream>

#include "util.hpp"

using upcxx::rank_me;
using upcxx::rank_n;
using std::cout;
using std::endl;

bool *from_nebrs;
bool done = false;

void arrive(upcxx::intrank_t origin) {
  if (rank_me() == origin)
    done = true;
  else {
    UPCXX_ASSERT_ALWAYS(!from_nebrs[origin], "already received an rpc from neighbor " << origin);
    from_nebrs[origin] = true;
      
    upcxx::intrank_t nebr = (rank_me() + 1)%rank_n();
    
    upcxx::rpc_ff(nebr, [=]() {
      arrive(origin);
    });
  }
}

int main() {
  upcxx::init();

  print_test_header();
  
  upcxx::intrank_t me = rank_me();
  upcxx::intrank_t nebr = (me + 1) % rank_n();

  from_nebrs = (bool*)calloc(rank_n(), sizeof(bool));

  upcxx::barrier();
  
  upcxx::rpc_ff(nebr, [=]() {
    arrive(me);
  });
  
  while(!done)
    upcxx::progress();

  cout << "Rank " << me << " done" << endl;
  
  // need to wait untill all ranks have finished before checking the from_nebrs array
  upcxx::barrier();
  
  for (int i = 0; i < rank_n(); i++) {
    if (i == me) continue;
    UPCXX_ASSERT_ALWAYS(from_nebrs[i], "From neighbor " << i << "is not set");
  }

  print_test_success();
  
  upcxx::finalize();
  return 0;
}
