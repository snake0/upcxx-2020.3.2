#include <iostream>
#include <upcxx/upcxx.hpp>

#include "../util.hpp"

int calls = 0;

void foo0(void) { calls++; std::cout << "foo0" << std::endl; }
void foo2I(int a, int b) { calls++; std::cout << "foo2I(" << a << "," << b << ")" << std::endl; }

int main(int argc, char *argv[])
{
    upcxx::init();
    print_test_header();

    foo0();    // works (regular C++)
    (&foo0)(); // works (regular C++)
    upcxx::rpc(upcxx::rank_me(),foo0).wait();  // works
    upcxx::rpc(upcxx::rank_me(),&foo0).wait(); // works

    foo2I(1,2);    // works (regular C++)
    (&foo2I)(1,2); // works (regular C++)
    upcxx::rpc(upcxx::rank_me(),foo2I,1,2).wait();  // FAILS!!!
    upcxx::rpc(upcxx::rank_me(),&foo2I,1,2).wait(); // works

    print_test_success(calls == 8);
    upcxx::finalize();
    return 0;
}   
