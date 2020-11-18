#include <iostream>
#include <sstream>
#include <gasnet.h>

int main() {
  gasnet_init(nullptr, nullptr);
  gasnet_attach(nullptr, 0, 1<<20, 0);
  
  std::ostringstream oss;
  oss << "Hello from "<<gasnet_mynode()<<'\n';
  std::cout << oss.str() << std::flush;

  // barrier to ensure all ranks have sent their output before any exit (and possibly end the job)
  gasnet_barrier_notify(0,0);
  gasnet_barrier_wait(0,0);
  
  return 0;
}
