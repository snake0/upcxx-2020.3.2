#include <cstdint>
#include <queue>
#include <cstdlib>

#if UPCXX_BACKEND
  #include <upcxx/upcxx.hpp>
#else
  #include <upcxx/diagnostic.hpp>
  #include <upcxx/future.hpp>
#endif

#include "util.hpp"

using namespace upcxx;
using namespace std;

struct the_q_compare {
  // shuffle bits of a pointer
  template<typename T>
  static uintptr_t mix(T *p) {
    uintptr_t u = reinterpret_cast<uintptr_t>(p);
    uintptr_t knuth = 0x9e3779b97f4a7c15u;
    u ^= u >> 35;
    u *= knuth;
    u ^= u >> 21;
    u *= knuth;
    return u;
  }
  
  template<typename T>
  bool operator()(T *a, T *b)  {
    return mix(a) < mix(b);
  }
};

// A global list of promises in a randomized order. Progress is made by
// satisfying these promises. Randomization is to immitate the wacky
// nature of asynchronous execution.
priority_queue<
    promise<int>*,
    vector<promise<int>*>,
    the_q_compare
  > the_q;

int fib_smart(int i) {
  int a = 0, b = 1;
  while(i--) {
    int c = a + b;
    a = b;
    b = c;
  }
  return a;
}

future<int> fib(int i) {
  // Instead of returning the result values, we always return futures
  // derived from partially satisfied promises containing the result value.
  // The promises are insterted into the random queue. This randomizes
  // the order the fibonacci tree gets evaluated.
  
  if(i <= 1) {
    auto *p = new promise<int>;
    p->require_anonymous(1);
    p->fulfill_result(i);
    the_q.push(p);
    return p->get_future();
  }
  else
    return when_all(fib(i-1), fib(i-2))
      .then([=](int x1, int x2)->future<int> {
        static int iter = 0;
        
        // branches are equivalent, they just test more of future's
        // internal codepaths.
        if(iter++ & 1) {
          auto *p = new promise<int>;
          p->require_anonymous(1);
          UPCXX_ASSERT_ALWAYS(x1 + x2 == fib_smart(i), "i="<<i<<" x1="<<x1<<" x2="<<x2);
          p->fulfill_result(x1 + x2);
          the_q.push(p);
          return p->get_future();
        }
        else {
          return make_future(x1 + x2);
        }
      });
}

#if 0
void* operator new(size_t size) {
  return malloc(size);
}

// hopefully we see lots of "free!"'s scattered through run and not all
// clumped up at the end.
void operator delete(void *p) {
  cout << "free!\n";
  free(p);
}
#endif

template<typename T>
void say_type() {
  // Causes a static_assert error. Typically the compiler will pretty print
  // the type of T into the context above the error.
  static_assert(sizeof(T) == 0, "That's a type!");
}

int main() {
#if UPCXX_BACKEND
  upcxx::init();
#endif

  print_test_header();
    
  const int arg = 5;
  
  future<int> ans0 = fib(arg);
  future<int> ans1 = ans0.then([](int x) { return x+1; }).then(fib);
  future<int> ans2 = /*future<int>*/(ans1.then_pure([](int x){return 2*x;})).then(fib);
  
  when_all(
      // stress nested concatenation
      when_all(
        when_all(),
        ans0,
        when_all(ans1),
        ans1.then_pure([](int x) { return x*x; }),
        make_future<const int&>(arg)
      ),
      make_future<vector<int>>({0*0, 1*1, 2*2, 3*3, 4*4})
    ).then(
      [=](int ans0, int ans1, int ans1_sqr, int arg, const vector<int> &some_vec) {
        cout << "fib("<<arg <<") = "<<ans0<<'\n';
        UPCXX_ASSERT_ALWAYS(ans0 == 5, "expected 5, got " << ans0);
        cout << "fib("<<ans0+1<<") = "<<ans1<<'\n';
        UPCXX_ASSERT_ALWAYS(ans1 == 8, "expected 8, got " << ans1);
        cout << "fib("<<ans0+1<<")**2 = "<<ans1_sqr<<'\n';
        UPCXX_ASSERT_ALWAYS(ans1_sqr == 8*8, "expected 64, got " << ans1_sqr);
        
        for(int i=0; i < 5; i++) {
            UPCXX_ASSERT_ALWAYS(some_vec[i] == i*i, "expected " << i*i << ", got " << some_vec[i]);
        }
      }
    );
  
  // drain progress queue
  while(!the_q.empty()) {
    promise<int> *p = the_q.top();
    the_q.pop();
    
    p->fulfill_anonymous(1);
    delete p;
  }
  
  auto nop = [](){};
  #define THEM(member)\
    static_assert(std::is_same<void, decltype(make_future().member)>::value, "Uh-oh");\
    (void)make_future().member;\
    static_assert(std::is_same<int, decltype(ans0.member)>::value, "Uh-oh");\
    (void)ans0.member;\
    static_assert(std::is_same<int, decltype(when_all(ans0).member)>::value, "Uh-oh");\
    (void)when_all(ans0).member;\
    static_assert(std::is_same<tuple<int,int,float,int>, decltype(when_all(ans0, ans1, make_future(3.14f), ans2).member)>::value, "Uh-oh");\
    (void)when_all(ans0,ans1,make_future(3.14f),ans2).member;
  THEM(result())
  THEM(wait(nop))
  #undef THEM

  #define THEM(member)\
    static_assert(std::is_same<void, decltype(make_future().member)>::value, "Uh-oh");\
    (void)make_future().member;\
    static_assert(std::is_same<int const&, decltype(ans0.member)>::value, "Uh-oh");\
    (void)ans0.member;\
    static_assert(std::is_same<int const&, decltype(when_all(ans0).member)>::value, "Uh-oh");\
    (void)when_all(ans0).member;\
    static_assert(std::is_same<tuple<int const&,int const&,float const&,int const&>, decltype(when_all(ans0, ans1, make_future<float>(3.14), ans2).member)>::value, "Uh-oh");\
    (void)when_all(ans0, ans1, make_future<float>(3.14), ans2).member;
  THEM(result_reference())
  THEM(wait_reference(nop))
  #undef THEM
  
  static_assert(std::is_same<float, decltype(make_future(true,1,3.14f).result<2>())>::value, "Uh-oh");
  static_assert(std::is_same<float const&, decltype(make_future(true,1,3.14f).result_reference<2>())>::value, "Uh-oh");
  static_assert(std::is_same<float, decltype(make_future(true,1,3.14f).wait<2>(nop))>::value, "Uh-oh");
  static_assert(std::is_same<float const&, decltype(make_future(true,1,3.14f).wait_reference<2>(nop))>::value, "Uh-oh");
  
  static_assert(std::is_same<tuple<bool,int>,decltype(make_future(true,1).result_tuple())>::value, "uh-oh");
  static_assert(std::is_same<tuple<bool,int>,decltype(make_future(true,1).wait_tuple(nop))>::value, "uh-oh");
  
  UPCXX_ASSERT_ALWAYS(ans2.ready(), "Answer is not ready");
  cout << "fib("<<(2*ans1.result())<<") = "<<ans2.result()<<'\n';
  UPCXX_ASSERT_ALWAYS(ans2.result() == 987, "expected 987, got " << ans2.result());
  
  print_test_success();
  
#if UPCXX_BACKEND
  upcxx::finalize();
#endif
  return 0;
}
