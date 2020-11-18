#include <upcxx/upcxx.hpp>
#include <iostream>
#include <sstream>

int main() {
  upcxx::init();
  
  std::ostringstream oss;
  oss << "Hello from "<<upcxx::rank_me()<<" of "<<upcxx::rank_n()<<'\n';
  std::cout << oss.str() << std::flush;
 
  upcxx::barrier();
  if (!upcxx::rank_me()) std::cout << "SUCCESS" << std::endl;
  upcxx::finalize();
  return 0;
}
