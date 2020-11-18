#include <upcxx/upcxx.hpp>
#include <iostream>
#include <assert.h>
#include <type_traits>

// this test validates that all the ways to default-construct a team_id
// generate the same unique invalid team_id

using upcxx::team_id;
#if UPCXX_VERSION > 20200300
static_assert(!std::is_trivial<upcxx::team_id>::value, "oops");
#endif
static_assert(std::is_standard_layout<upcxx::team_id>::value, "oops");
static_assert(std::is_default_constructible<upcxx::team_id>::value, "oops");
static_assert(std::is_trivially_copyable<upcxx::team_id>::value, "oops");


int errs = 0;

team_id gid1;
team_id gid2 = team_id();
team_id gid3{};
team_id gid4 = {};
team_id gida[2];

struct T {
  team_id id1;
  team_id id2 = team_id();
  team_id id3{};
  team_id id4 = {};
  team_id ida[2];
  static team_id sid1;
  static team_id sid2;
  static team_id sid3;
  static team_id sid4;
};

team_id T::sid1;
team_id T::sid2 = team_id();
team_id T::sid3{};
team_id T::sid4 = {};

void run_check() {
  team_id id1;
  team_id id2 = team_id();
  team_id id3{};
  team_id id4 = {};
  team_id ida1[2] = {};
  team_id ida2[2];
  team_id *ida3 = new team_id[2]{};
  team_id *ida4 = new team_id[2];
  T t1;
  T t2 = T();
  T t3{};
  T t4 = {};
  bool b1 = id2 == id4; // check EqualityComparable
  bool b2 = id2 < id4; // check LessThanComparable
  #define CHECK(var) CHECKEQ(var, upcxx::team_id())
  #define CHECKEQ(var1,var2) do { \
    if ((var1) != (var2)) { \
      std::cout << "ERROR: " << #var1 ": " << (var1) \
                << " != " #var2 ": " << (var2) << std::endl; \
      errs++; \
    } \
  } while(0)
  if (!upcxx::rank_me()) {
    CHECK(id1); CHECK(id2); CHECK(id3); CHECK(id4);
    CHECK(ida1[0]); CHECK(ida1[1]); 
    CHECK(ida2[0]); CHECK(ida2[1]); 
    CHECK(ida3[0]); CHECK(ida3[1]); 
    CHECK(ida4[0]); CHECK(ida4[1]); 
    CHECK(gid1); CHECK(gid2); CHECK(gid3); CHECK(gid4); CHECK(gida[0]); CHECK(gida[1]); 
    CHECK(T::sid1); CHECK(T::sid2); CHECK(T::sid3); CHECK(T::sid4);
    CHECK(t1.id1); CHECK(t1.id2); CHECK(t1.id3); CHECK(t1.id4); CHECK(t1.ida[0]); CHECK(t1.ida[1]);
    CHECK(t2.id1); CHECK(t2.id2); CHECK(t2.id3); CHECK(t2.id4); CHECK(t2.ida[0]); CHECK(t2.ida[1]);
    CHECK(t3.id1); CHECK(t3.id2); CHECK(t3.id3); CHECK(t3.id4); CHECK(t3.ida[0]); CHECK(t3.ida[1]);
    CHECK(t4.id1); CHECK(t4.id2); CHECK(t4.id3); CHECK(t4.id4); CHECK(t4.ida[0]); CHECK(t4.ida[1]);
  }
  rpc(0,[](team_id aid1, team_id aid2, team_id aid3) {
        CHECKEQ(aid1, gid1);
        CHECKEQ(aid2, gid2);
        CHECK(aid3);
        }, id1, id2, team_id()).wait();
  delete [] ida3;
  delete [] ida4;
}

double stack_writer(double v) {
  if (v < 3) return 1;
  else return stack_writer(v-2) * stack_writer(v-1) * v;
}

int main() {
  upcxx::init();
  
  std::cout<<"Hello from "<<upcxx::rank_me()<<" of "<<upcxx::rank_n()<<"\n";
  upcxx::barrier();

  for (int i=0; i < 2; i++) { 
    stack_writer(10.1);
    run_check();
  }

  upcxx::barrier();
  if (!upcxx::rank_me() && errs == 0) std::cout<<"SUCCESS"<<std::endl;
  upcxx::finalize();
  return 0;
}
