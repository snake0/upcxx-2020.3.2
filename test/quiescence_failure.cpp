#include <upcxx/upcxx.hpp>

/* The purpose of this test is to intentionally call `upcxx::finalize()` with
 * rendezvous based RPC's in-flight just to see what failure mode this
 * may excite in the runtime. A diagnostic of some sort is expected.
 */
 
void bounce() {
  upcxx::rpc_ff(
    (upcxx::rank_me()+1)%upcxx::rank_n(),
    [](std::vector<char> const &baggage) { bounce(); },
    std::vector<char>(1<<20) // big enough to spill out of eager protocol
  );
}

int main() {
  upcxx::init();
  if(upcxx::rank_me() == 0)
    bounce();
  upcxx::finalize();
  return 0;
}
