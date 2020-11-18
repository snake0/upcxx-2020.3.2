#include <upcxx/backend.hpp>
#include <upcxx/future.hpp>
#include <iostream>

int main() {
  upcxx::init();
  
  upcxx::promise<> pro1;

  pro1.require_anonymous(1);
  auto f1 = pro1.finalize();
  pro1.fulfill_anonymous(1);
  f1.wait();

  pro1 = upcxx::promise<>();
  pro1.require_anonymous(1);
  f1 = pro1.finalize();
  pro1.fulfill_anonymous(1);
  f1.wait();

  //This must fail with the asserts on because pro1 has
  //already been fulfilled
  pro1.require_anonymous(1);
  f1 = pro1.finalize();
  pro1.fulfill_anonymous(1);
  f1.wait();

  upcxx::finalize();
  return 0;
}

