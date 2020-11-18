#include <upcxx/allocate.hpp>
#include <upcxx/barrier.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rput.hpp>
#include <upcxx/rget.hpp>
#include <upcxx/rpc.hpp>

#include "util.hpp"

using upcxx::global_ptr;
using upcxx::intrank_t;
using upcxx::future;
using upcxx::promise;
using upcxx::operation_cx;
using upcxx::source_cx;
using upcxx::remote_cx;

global_ptr<int> my_thing;
int got_rpc = 0;

struct non_pod {
  non_pod() {}
  non_pod(const non_pod&) {}
};

int main() {
  upcxx::init();

  print_test_header();
  
  intrank_t me = upcxx::rank_me();
  intrank_t n = upcxx::rank_n();
  intrank_t nebr = (me + 1) % n;
  
  my_thing = upcxx::allocate<int>();
  
  upcxx::barrier();

  // Uncomment and watch it fail with static_assert about
  // DefinitelyTriviallySerializable of non_pod.
  //upcxx::rget(global_ptr<non_pod>{});
  
  global_ptr<int> nebr_thing; {
    future<global_ptr<int>> fut = upcxx::rpc(nebr, []() { return my_thing; });
    nebr_thing = fut.wait();
  }
  
  future<> done_g;
  bool done_s = false;
  
  int value = 100+me;
  
  done_g = upcxx::rput(
    &value, nebr_thing, 1,
    operation_cx::as_future() |
    source_cx::as_lpc(upcxx::master_persona(), [&done_s]() { done_s=true; }) |
    remote_cx::as_rpc([=]() { got_rpc++; })
  );
  
  int buf;
  done_g = done_g.then([&]() {
    promise<int> *pro1 = new promise<int>;
    upcxx::rget(nebr_thing, operation_cx::as_promise(*pro1));
    
    promise<> *pro2 = new promise<>;
    upcxx::rget(nebr_thing, &buf, 1,
                operation_cx::as_promise(*pro2) |
                remote_cx::as_rpc([=](){ got_rpc++; }));
    
    return upcxx::when_all(
        pro1->finalize(),
        pro2->finalize()
      ).then([&,pro1,pro2](int got) {
        delete pro1;
        delete pro2;

        UPCXX_ASSERT_ALWAYS(got == 100 + me, "got incorrect value, " << got << " != " << (100 + me));
        UPCXX_ASSERT_ALWAYS(got == buf, "got not equal to buf");
        std::cout << "get(put(X)) == X\n";
      });
  });
  
  done_g.wait();
  
  while(!done_s || got_rpc != 2)
    upcxx::progress();
  
  //upcxx::barrier();
  
  upcxx::deallocate(my_thing);

  print_test_success();
  
  upcxx::finalize();
  return 0;
}
