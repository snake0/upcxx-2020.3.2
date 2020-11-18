#include <upcxx/upcxx.hpp>
#include <iostream>
#include "../util.hpp"
#include <gasnet.h>
#include <assert.h>

using namespace std;
using namespace upcxx;

int main() {
    upcxx::init();
    print_test_header();

    if (rank_n() < 2) {
      cout << "SKIPPED: Test requires 2 or more ranks." << endl;
      print_test_success(true);
      upcxx::finalize();
      return 0;
    }

    dist_object<int> dobj(rank_me());
    if (rank_me() == 0) {
      liberate_master_persona(); // drop master persona

      // fetch
      int sum = 0;
      int check = 0;
      for (int i=1; i < rank_n(); i++) {
        sum += dobj.fetch(i).wait();
        check += i;
      }
      assert(check == sum);

      persona_scope *ps = new persona_scope(master_persona());  // re-acquire master
    }

    barrier();

    print_test_success(true);
    upcxx::finalize();
    return 0;
} 
