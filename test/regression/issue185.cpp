#include <upcxx/upcxx.hpp>
#include "../util.hpp"

int main() {
  upcxx::init();
  print_test_header();

  upcxx::team const &tm = upcxx::world();
  upcxx::dist_object<int> dobj1(47, tm);
  upcxx::dist_object<int> dobj2(tm, 47);

  print_test_success(true);
  upcxx::finalize();
  return 0;
}
