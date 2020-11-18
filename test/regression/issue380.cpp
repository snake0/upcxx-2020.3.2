#include <upcxx/upcxx.hpp>
#include "../util.hpp"

int main(int argc, char **argv) {
  upcxx::init();
  print_test_header();
  int src = 0xbeef;
  upcxx::global_ptr<int> dst = upcxx::new_<int>();
  auto futs = upcxx::rput(&src, dst, 1, upcxx::operation_cx::as_future() | upcxx::source_cx::as_future());
  std::get<0>(futs).wait();
  std::get<1>(futs).wait();
  UPCXX_ASSERT_ALWAYS(*dst.local() == src);
  print_test_success();
  upcxx::finalize();
  return 0;
}
