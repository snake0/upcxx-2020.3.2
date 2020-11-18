#include <upcxx/upcxx.hpp>
#include <iostream>
#include "../util.hpp"

using namespace std;
using namespace upcxx;

int main() {
    upcxx::init();
    print_test_header();

    global_ptr<int> gptr = new_<int>(0);
    auto f = rput(10, gptr);
    f.wait();

    static int done = 0;
    rput(10, gptr, remote_cx::as_rpc([]() { done = 1; }));
    while (!done) progress();

    static int done2 = 0;
    int x = 42;
    int cnt = 1;
    rput(&x, gptr, cnt, remote_cx::as_rpc([](int z) { done2 = 1; },77));
    while (!done2) progress();

    print_test_success(true);
    upcxx::finalize();
    return 0;
} 
