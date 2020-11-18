#include <upcxx/upcxx.hpp>
#include <cassert>
#include <iostream>

using namespace upcxx;

int main() {
  upcxx::init();

  team t = world().split(1, rank_me());
  team_id id = t.id();
  assert(&t != &world());
  assert(id != world().id());
  
  future<team &> f = id.when_here();
  assert(f.ready());
  assert(&f.result() == &t);

  team &t1 = id.here();
  assert(&t1 == &t);

  t.destroy();

  upcxx::barrier();
  
  if (!upcxx::rank_me()) std::cout << "SUCCESS" << std::endl;

  upcxx::finalize();
  return 0;
}
