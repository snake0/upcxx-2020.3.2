#include <upcxx/upcxx.hpp>
#include <iostream>
#include <sstream>

int main() {
  upcxx::init();
  
  std::ostringstream oss;
  oss << "Hello from "<<upcxx::rank_me()<<" of "<<upcxx::rank_n()<<'\n';
  std::cout << oss.str() << std::flush;
  
  upcxx::finalize();
  return 0;
}
