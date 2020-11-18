#include <upcxx/upcxx.hpp>
#include <cassert>
#include "../util.hpp"

using namespace upcxx;

int main() {
  upcxx::init();

  print_test_header();

  promise<> p(2); // empty promise with 2 dependencies
  assert(!p.get_future().ready());
  p.fulfill_anonymous(1); 
  assert(!p.get_future().ready());
  p.fulfill_anonymous(1); 
  assert(p.get_future().ready()); // crashes here

  print_test_success(true);

  upcxx::finalize();
  return 0;
} 
