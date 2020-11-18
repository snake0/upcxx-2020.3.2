#include <upcxx/upcxx.hpp>
#include <iostream>
#include <assert.h>

int main() {
  upcxx::init();
  
  upcxx::dist_object<int> dobj(1);
  upcxx::dist_id<int> di = dobj.id();
  upcxx::dist_object<int> &ref = di.here();
  assert(&ref == &dobj);

  upcxx::team &tm = upcxx::local_team();
  upcxx::team_id ti = tm.id();
  upcxx::team &tr = ti.here();
  assert(&tr == &tm);

  upcxx::barrier();
  if (!upcxx::rank_me()) std::cout << "SUCCESS" << std::endl;
  
  upcxx::finalize();
  return 0;
}
