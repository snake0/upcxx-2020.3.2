#include <upcxx/upcxx.hpp>
#include "../util.hpp"

using namespace std;

int main() {
  upcxx::init();
  print_test_header();
  
  int countdown = 2;
  auto handler1 = [&]() { countdown -= 1; };

  #if 1 // breaks
    auto handler2 = handler1;
  #else // handler2 gets a different lambda type, passes
    auto handler2 = [&]() { countdown -= 1; };
  #endif
  
  upcxx::rpc(upcxx::rank_me(),
    upcxx::operation_cx::as_lpc(upcxx::current_persona(), handler1) |
    upcxx::operation_cx::as_lpc(upcxx::current_persona(), handler2),
    []() {}
  );
  
  while(countdown != 0)
    upcxx::progress();

  print_test_success(true);
  upcxx::finalize();
}
