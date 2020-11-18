#include <upcxx/barrier.hpp>
#include <upcxx/rpc.hpp>
#include <upcxx/view.hpp>
#include <upcxx/dist_object.hpp>

#include "util.hpp"

#include <atomic>
#include <deque>
#include <list>
#include <vector>
#include <cmath>
#include <string>
#include <thread>

using namespace upcxx;
using namespace std;

template<typename T>
struct is_actually_trivially_serializable {
  static constexpr bool value = serialization_traits<T>::is_actually_trivially_serializable;
};

////////////////////////////////////////////////////////////////////////////////
// static tests

// int is known trivial and implemented that way
static_assert(is_trivially_serializable<int>::value, "ERROR");
static_assert(is_actually_trivially_serializable<int>::value, "ERROR");

// tuple<float,double> is known trivial and implemented that way
static_assert(is_trivially_serializable<tuple<float,double>>::value, "ERROR");
static_assert(is_actually_trivially_serializable<tuple<float,double>>::value, "ERROR");

// Nothing trivial about a type containing a vector, but still supported
static_assert(!is_trivially_serializable<vector<int>>::value, "ERROR");
static_assert(is_serializable<vector<int>>::value, "ERROR");
static_assert(!is_actually_trivially_serializable<vector<int>>::value, "ERROR");

static_assert(!is_trivially_serializable<tuple<int,vector<int>>>::value, "ERROR");
static_assert(is_serializable<tuple<int,vector<int>>>::value, "ERROR");
static_assert(!is_actually_trivially_serializable<tuple<int,vector<int>>>::value, "ERROR");

// User defined POD's are known trivial
struct my_pod { int foo; };
static_assert(is_trivially_serializable<my_pod>::value, "ERROR");
static_assert(is_serializable<my_pod>::value, "ERROR");
static_assert(is_actually_trivially_serializable<my_pod>::value, "ERROR");

// We don't *know* T is trivially packed, but the implementation assumes it.
struct my_nonpod1 {
  int foo;
  my_nonpod1() = default;
  my_nonpod1(const my_nonpod1&) {}
};
static_assert(!is_trivially_serializable<my_nonpod1>::value, "ERROR");
static_assert(!is_serializable<my_nonpod1>::value, "ERROR");
static_assert(!is_serializable<tuple<my_nonpod1>>::value, "ERROR");
static_assert(is_actually_trivially_serializable<my_nonpod1>::value, "ERROR");

// We are told T is trivially packed
struct my_nonpod2 {
  int foo;
  my_nonpod2() = default;
  my_nonpod2(const my_nonpod2&) {}
};
namespace upcxx {
  template<>
  struct is_trivially_serializable<my_nonpod2>: std::true_type {};
}
static_assert(is_trivially_serializable<my_nonpod2>::value, "ERROR");
static_assert(is_serializable<my_nonpod2>::value, "ERROR");
static_assert(is_actually_trivially_serializable<my_nonpod2>::value, "ERROR");

// test upcxx::view<T>
static_assert(!is_trivially_serializable<upcxx::view<int>>::value, "ERROR");
static_assert(is_serializable<upcxx::view<int>>::value, "ERROR");
static_assert(!is_actually_trivially_serializable<upcxx::view<int>>::value, "ERROR");

static_assert(!is_trivially_serializable<upcxx::view<vector<int>>>::value, "ERROR");
static_assert(is_serializable<upcxx::view<vector<int>>>::value, "ERROR");
static_assert(!is_actually_trivially_serializable<upcxx::view<vector<int>>>::value, "ERROR");

#define view_t upcxx::view<upcxx::view<int, vector<int>::const_iterator>, vector<upcxx::view<int, vector<int>::const_iterator>>::const_iterator>
static_assert(!is_trivially_serializable<view_t>::value, "ERROR");
static_assert(is_serializable<view_t>::value, "ERROR");
static_assert(!is_actually_trivially_serializable<view_t>::value, "ERROR");
#undef view_t

#define view_t upcxx::view<vector<int>, vector<int>::iterator>
static_assert(!is_trivially_serializable<view_t>::value, "ERROR");
static_assert(is_serializable<view_t>::value, "ERROR");
static_assert(!is_actually_trivially_serializable<view_t>::value, "ERROR");
#undef view_t

#define view_t upcxx::view<vector<my_nonpod1>, vector<my_nonpod1>::iterator>
static_assert(!is_trivially_serializable<view_t>::value, "ERROR");
static_assert(!is_serializable<view_t>::value, "ERROR");
static_assert(!is_actually_trivially_serializable<view_t>::value, "ERROR");
#undef view_t

#define view_t upcxx::view<vector<my_nonpod2>, vector<my_nonpod2>::iterator>
static_assert(!is_trivially_serializable<view_t>::value, "ERROR");
static_assert(is_serializable<view_t>::value, "ERROR");
static_assert(!is_actually_trivially_serializable<view_t>::value, "ERROR");
#undef view_t

#define pair_t std::pair<std::string, double>
static_assert(!is_trivially_serializable<pair_t>::value, "ERROR");
static_assert(is_serializable<pair_t>::value, "ERROR");
static_assert(!is_actually_trivially_serializable<pair_t>::value, "ERROR");
#undef pair_t

////////////////////////////////////////////////////////////////////////////////
// runtime test

persona worker;
atomic<bool> worker_shutdown{false};

const upcxx::detail::promise_vtable *volatile cleanser;

int main() {
  upcxx::init();
  {
    print_test_header();

    cleanser = &upcxx::detail::the_promise_vtable<>::vtbl;
    UPCXX_ASSERT_ALWAYS(cleanser->execute_and_delete != nullptr);

    cleanser = &upcxx::detail::the_promise_vtable<int>::vtbl;
    UPCXX_ASSERT_ALWAYS(cleanser->execute_and_delete != nullptr);
  
    std::thread worker_thread{
      []() {
        persona_scope worker_scope{worker};

        while(!worker_shutdown.load(memory_order_relaxed))
          upcxx::progress();
      }
    };

    dist_object<int> dobj{rank_me()};
    dist_object<int> rpc_ff_balance{0};
    
    for(int iter=0; iter < 10; iter++) {
      constexpr int hunk1_n = 4, hunk2_n = 100;

      // build our hunks to ship in rpc
      vector<list<tuple<int,int>>> hunk1(hunk1_n);
      vector<view<tuple<int,int>, list<tuple<int,int>>::const_iterator>> hunk1v;
      deque<int> hunk2;

      for(int i=0; i < hunk1_n; i++) {
        for(int j=0; j < 1 + i*i; j++)
          hunk1[i].push_back(std::make_tuple(j, j*j));
        hunk1v.push_back(upcxx::make_view(hunk1[i]));
      }

      for(int i=0; i < hunk2_n; i++)
        hunk2.push_back(i*i);

      // send those hunks
      auto rpc_done1 = upcxx::rpc(
        /*target*/(rank_me() + 1)%rank_n(),
        
        [=](dist_object<int> &dobj,
           view<list<tuple<int,int>>> hunk1,
           view<view<tuple<int,int>>> hunk1v,
           view<int> hunk2,
           vector<char> const &abc,
           std::string const &hey,
           std::pair<std::string, double> const &hi123,
           my_pod,
           my_nonpod2
        ) {
          UPCXX_ASSERT_ALWAYS(*dobj == rank_me());
          UPCXX_ASSERT_ALWAYS(abc.size()==3 && abc[0]=='a'&&abc[1]=='b'&&abc[2]=='c');
          UPCXX_ASSERT_ALWAYS(hey == "hey");
          UPCXX_ASSERT_ALWAYS(hi123.first == "hi" && int(hi123.second)==123);
          
          return worker.lpc([=]() {
            int i;

            UPCXX_ASSERT_ALWAYS(hunk1.size() == (size_t)hunk1_n);
            i = hunk1_n;
            for(list<tuple<int,int>> const& x: hunk1) {
              i--;
              UPCXX_ASSERT_ALWAYS(x.size() == 1u + i*i);
              
              int j=0;
              for(auto y: x) {
                UPCXX_ASSERT_ALWAYS(std::get<0>(y) == j);
                UPCXX_ASSERT_ALWAYS(std::get<1>(y) == j*j);
                j++;
              }
            }

            UPCXX_ASSERT_ALWAYS(hunk1v.size() == (size_t)hunk1_n);
            i = hunk1_n;
            for(upcxx::view<tuple<int,int>> const& x: hunk1v) {
              i--;
              UPCXX_ASSERT_ALWAYS(x.size() == 1u + i*i);
              
              int j=0;
              for(auto y: x) {
                UPCXX_ASSERT_ALWAYS(std::get<0>(y) == j);
                UPCXX_ASSERT_ALWAYS(std::get<1>(y) == j*j);
                j++;
              }
            }

            UPCXX_ASSERT_ALWAYS(hunk2.size() == (size_t)hunk2_n);
            UPCXX_ASSERT_ALWAYS(hunk2.empty() == (hunk2.size() == 0));

            UPCXX_ASSERT_ALWAYS(&hunk2.front() == hunk2.begin());
            UPCXX_ASSERT_ALWAYS(&hunk2.back() == hunk2.end()-1);

            for(i=0; i < hunk2_n; i++) {
              // hooray operator[]
              UPCXX_ASSERT_ALWAYS(hunk2[i] == i*i);
              UPCXX_ASSERT_ALWAYS(hunk2.at(i) == i*i);
              UPCXX_ASSERT_ALWAYS(hunk2.data()[i] == i*i);

              UPCXX_ASSERT_ALWAYS(*(hunk2.begin() + i) == i*i);
              UPCXX_ASSERT_ALWAYS(*(hunk2.end() - hunk2_n + i) == i*i);
              UPCXX_ASSERT_ALWAYS(*(hunk2.rbegin() + hunk2_n-1 - i) == i*i);
              UPCXX_ASSERT_ALWAYS(*(hunk2.rend()-1 - i) == i*i);

              UPCXX_ASSERT_ALWAYS(*(hunk2.cbegin() + i) == i*i);
              UPCXX_ASSERT_ALWAYS(*(hunk2.cend() - hunk2_n + i) == i*i);
              UPCXX_ASSERT_ALWAYS(*(hunk2.crbegin() + hunk2_n-1 - i) == i*i);
              UPCXX_ASSERT_ALWAYS(*(hunk2.crend()-1 - i) == i*i);
            }
          #if 1
            });
          #elif 1
            // static_assert's "rpcs must return DefinitelySerializable"
            }).then([]() { return my_nonpod1(); });
          #else
            // static_assert's "rpcs cant return view"
            }).then([]() { return upcxx::make_view((int*)0, (int*)0); });
          #endif
        },
        dobj,
        upcxx::make_view(hunk1.rbegin(), hunk1.rend()),
        upcxx::make_view(hunk1v.crbegin(), hunk1v.crend()),
        upcxx::make_view(hunk2),
        vector<char>{'a','b','c'},
        std::string("hey"),
        std::pair<std::string,double>("hi", 123),
        my_pod{},
        my_nonpod2{} // change to my_nonpod1 and watch the static_assert for is_serializable fail!
      );

      // wait til hunks are processed by remote worker thread
      rpc_done1.wait();

      // verify that network buffer lifetime extends to returned future in rpc_ff.
      int origin = rank_me();
      int target[1] = {(rank_me() + 1)%rank_n()};
      upcxx::promise<> *rpc_done2 = new upcxx::promise<>;
      upcxx::rpc_ff(
        target[0],
        [=](upcxx::view<int> v) {
          // kick it off to worker thread
          return worker.lpc([=]() {
            // worker kicks it back to master
            return upcxx::master_persona().lpc(
              [=]() {
                // master validates buffer contents
                UPCXX_ASSERT_ALWAYS(v[0] == upcxx::rank_me());
                // tell sender the work is complete
                upcxx::rpc_ff(origin, [=]() { rpc_done2->finalize(); });
              }
            );
          });
        },
        upcxx::make_view(target)
      );

      rpc_done2->get_future().wait();
      delete rpc_done2;
    }

    // quiesce the world
    upcxx::barrier();

    // flag worker to die
    worker_shutdown.store(true, std::memory_order_relaxed);
    worker_thread.join();
    
    print_test_success();
  }
  upcxx::finalize();
}
