#include <upcxx/upcxx.hpp>
#include <iostream>

struct foo {
  upcxx::dist_object<int> v;
  int get(int rank) const {
    return v.fetch(rank).wait();
  }
};


int main() {
  upcxx::init();
 
  const foo f{4};
  int x = f.get(0);
  
  upcxx::finalize();
  return 0;
}
