#include <upcxx/upcxx.hpp>
#include <cassert>
#include <iostream>

int main() {
  upcxx::init();

  std::size_t x = upcxx::cuda_device::default_alignment<int>();
  std::cout << "upcxx::cuda_device::default_alignment<int> = " << x << std::endl;
  assert(x > 0);

  upcxx::barrier();

  if (!upcxx::rank_me()) std::cout << "SUCCESS" << std::endl;

  upcxx::finalize();
  return 0;
}
