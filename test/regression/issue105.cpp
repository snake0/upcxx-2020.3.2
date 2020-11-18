#include <upcxx/upcxx.hpp>
#include <iostream>
#include <unistd.h>

using namespace std;

using upcxx::say;

int main() {
  upcxx::init();

  if(upcxx::rank_me() == 0 && !UPCXX_ASSERT_ENABLED) {
    say()<<"This test will likely deadlock. Build it with ASSERT=1 in "
           "the environment so that it asserts before deadlocking.";
  }
  
  UPCXX_ASSERT_ALWAYS(upcxx::rank_n() % 2 == 0);
  say()<<"sending outermost RPC.";
  upcxx::rpc(upcxx::rank_me() ^ 1,[]() {
    say()<<"in outermost RPC";
    sleep(1);
    say()<<"sending inner RPC";
    auto f = upcxx::rpc(upcxx::rank_me() ^ 1,[]() {
      say()<<"in inner RPC";
    });
    say()<<"waiting for inner RPC";
    f.wait(); // deadlock here
    say()<<"something else";
  }).wait();

  say()<<"done";

  upcxx::finalize();
  return 0;
}
