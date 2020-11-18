#include <upcxx/backend.hpp>
#include <upcxx/future.hpp>
#include <iostream>

int main() {
  upcxx::init();
  
  upcxx::promise<double> pro2;
  pro2.require_anonymous(3);
  auto f2 = pro2.finalize();
  pro2.fulfill_anonymous(1);
  pro2.fulfill_result(2.0);
  pro2.fulfill_anonymous(1);
  double res2 = f2.wait();
  std::cout<<"Result of pro2 is "<<res2<<std::endl;

  //This must fail with the asserts on because fulfill_result
  //cannot be called multiple time
  pro2 =upcxx::promise<double>();
  pro2.require_anonymous(3);
  f2 = pro2.finalize();
  pro2.fulfill_anonymous(1);
  pro2.fulfill_result(2.0);
  pro2.fulfill_result(3.0);
  res2 = f2.wait();
  std::cout<<"Result of pro2 is "<<res2<<std::endl;


  upcxx::finalize();
  return 0;
}

