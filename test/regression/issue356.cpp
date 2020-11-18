#include <upcxx/upcxx.hpp>
#include <iostream>
#include <assert.h>

class FieldPoint {
private:
 double x;
 double y;

public:
 double get_x() { return x; }
 double get_y() { return y; }

 UPCXX_SERIALIZED_FIELDS(x,y) // "must be invoked...(at) public access level"

 static FieldPoint factory(double x, double y) {
   FieldPoint fp;
   fp.x = x;
   fp.y = y;
   return fp;
 }
#ifndef WORKAROUND1
private:
#endif
 FieldPoint() : x(0), y(0) {} // "default constructor may have any access level"
};

class Point {
private:
 double x;
 double y;

public:
 double get_x() { return x; }
 double get_y() { return y; }

 UPCXX_SERIALIZED_VALUES(float(x), float(y)) // "must be invoked...(at) public access level"

 // "must have a constructor that can be invoked with the resulting rvalues from the body of a static member function"
 static Point factory(float x, float y) {
   Point p(x,y);
   return p;
 }

#ifndef WORKAROUND2
private:
#endif
 Point(float a, float b) : x(a), y(b) {} // the only constructor
};

int main() {
  upcxx::init();
  
  std::cout<<"Hello from "<<upcxx::rank_me()<<" of "<<upcxx::rank_n()<<"\n";
  upcxx::barrier();
  double v1 = 100+upcxx::rank_me(), v2 = 1000+upcxx::rank_me();

  FieldPoint fp = FieldPoint::factory(v1, v2);
  assert(fp.get_x() == v1 && fp.get_y() == v2);

#ifndef DISABLE_SF
  upcxx::rpc(0, [](FieldPoint p) { std::cout << p.get_x() << ", " << p.get_y() << std::endl; }, fp).wait();
#endif

  upcxx::barrier();

  Point p = Point::factory(v1, v2);
  assert(p.get_x() == v1 && p.get_y() == v2);

#ifndef DISABLE_SV
  upcxx::rpc(0, [](Point lp) { std::cout << lp.get_x() << ", " << lp.get_y() << std::endl; }, p).wait();
#endif

  upcxx::barrier();
  if (!upcxx::rank_me()) std::cout<<"SUCCESS"<<std::endl;
  upcxx::finalize();
  return 0;
}
