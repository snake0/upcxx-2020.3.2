#include <upcxx/upcxx.hpp>

int nCount[2];
int phase = 1;

int main() {
  upcxx::init();

  auto f = [] (int phase){ nCount[phase]++;};
  upcxx::global_ptr<int> uL = upcxx::new_<int>();
  upcxx::rpc(0, f, phase);
  if ( uL ){
    rput( 4, uL, upcxx::remote_cx::as_rpc( f , phase) );
  } 

  upcxx::finalize();
  return 0;
} 
