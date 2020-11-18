#if UPCXX_BACKEND
  #include <upcxx/upcxx.hpp>
#else
  #include <upcxx/serialization.hpp>
  #include <upcxx/utility.hpp>
#endif
#include "util.hpp"

using namespace std;
using namespace upcxx;

template<typename T>
bool equals(T const &a, T const &b) {
  return a == b;
}

template<typename T, std::size_t n>
bool equals(T const (&a)[n], T const (&b)[n]) {
  bool ans = true;
  for(std::size_t i=0; i != n; i++)
    ans &= equals(a[i], b[i]);
  return ans;
}

template<typename T>
void roundtrip(T const &x) {
  void *buf0 = upcxx::detail::alloc_aligned(8*serialization_align_max, serialization_align_max);
  
  detail::serialization_writer<
      decltype(serialization_traits<T>::static_ubound)::is_valid
    > w(buf0, 8*serialization_align_max);

  serialization_traits<T>::serialize(w, x);

  void *buf1 = upcxx::detail::alloc_aligned(w.size(), std::max(sizeof(void*), w.align()));
  w.compact_and_invalidate(buf1);
  
  detail::serialization_reader r(buf1);
  
  typename std::aligned_storage<sizeof(T),alignof(T)>::type x1_;
  T *x1 = serialization_traits<T>::deserialize(r, &x1_);

  const bool is_triv = is_trivially_serializable<T>::value; // workaround a bug in Xcode 8.2.1
  UPCXX_ASSERT_ALWAYS(equals(*x1, x), "Serialization roundtrip failed. sizeof(T)="<<sizeof(T)<<" is_triv_serz="<<is_triv);

  upcxx::detail::destruct(*x1);
  std::free(buf1);
  std::free(buf0);
}

struct nonpod_base {
  char h, i;
  int _123[3]{1,2,3};
  nonpod_base **mirror;
  
  nonpod_base() {
    mirror = new nonpod_base*(this);
  }
  nonpod_base(char h, char i): nonpod_base() {
    this->h = h;
    this->i = i;
  }
  nonpod_base(const nonpod_base &that): nonpod_base() {
    this->h = that.h;
    this->i = that.i;
  }

  ~nonpod_base() {
    UPCXX_ASSERT_ALWAYS(*mirror == this);
    UPCXX_ASSERT_ALWAYS(_123[0]==1 && _123[1]==2 && _123[2]==3);
    delete mirror;
  }
  
  bool operator==(nonpod_base const &that) const {
    UPCXX_ASSERT_ALWAYS(*this->mirror == this);
    UPCXX_ASSERT_ALWAYS(*that.mirror == &that);
    return this->h == that.h && this->i == that.i;
  }
};

struct nonpod1: nonpod_base {
  nonpod1() = default;
  nonpod1(char h, char i): nonpod_base(h,i) {}
  UPCXX_SERIALIZED_FIELDS(h,i,_123)
};

struct nonpod2: nonpod_base {
  nonpod2(char h, char i): nonpod_base(h,i) {}
  nonpod2(char h, char i, int x123[3], char const *dummy):
    nonpod_base(h,i) {
    UPCXX_ASSERT_ALWAYS(x123[0]==1 && x123[1]==2 && x123[2]==3);
    UPCXX_ASSERT_ALWAYS(std::string(dummy) == "dummy");
  }
  UPCXX_SERIALIZED_VALUES(h,i,_123,"dummy")
};

struct nonpod3: nonpod_base {
  nonpod3(char h, char i): nonpod_base(h,i) {}
  
  struct upcxx_serialization {
    template<typename Writer>
    static void serialize(Writer &w, nonpod3 const &x) {
      w.write(0xbeef);
      w.write(x.h);
      w.write(x.i);
    }
    template<typename Reader>
    static nonpod3* deserialize(Reader &r, void *spot) {
      UPCXX_ASSERT_ALWAYS(r.template read<int>() == 0xbeef);
      char h = r.template read<char>();
      char i = r.template read<char>();
      return ::new(spot) nonpod3(h,i);
    }
  };
};

struct nonpod4: nonpod_base {
  nonpod4(char h, char i): nonpod_base(h,i) {}
};

namespace upcxx {
  template<>
  struct serialization<nonpod4> {
    // unpspec'd upper-bound support
    template<typename Prefix>
    static auto ubound(Prefix pre, nonpod4 const &x)
      -> decltype(pre.cat_ubound_of(x.h).cat_ubound_of(x.i)) {
      return pre.cat_ubound_of(x.h).cat_ubound_of(x.i);
    }
    
    template<typename Writer>
    static void serialize(Writer &w, nonpod4 const &x) {
      w.write(0xbeef);
      w.write(x.h);
      w.write(x.i);
    }
    template<typename Reader>
    static nonpod4* deserialize(Reader &r, void *spot) {
      UPCXX_ASSERT_ALWAYS(r.template read<int>() == 0xbeef);
      char h = r.template read<char>();
      char i = r.template read<char>();
      return ::new(spot) nonpod4(h,i);
    }
  };
}

// This case tests that UPCXX_SERIALIZED_BASE works and that serialization macros
// in base classes are shadowed by those in derived classes.

struct nonpod5_side {
  UPCXX_SERIALIZED_VALUES(123)
  nonpod5_side(int _123) { UPCXX_ASSERT_ALWAYS(_123 == 123); }
};

struct nonpod5: nonpod_base, nonpod5_side {
  nonpod5(): nonpod_base(), nonpod5_side(123) {}
  nonpod5(char h, char i): nonpod_base(h,i), nonpod5_side(123) {}
  UPCXX_SERIALIZED_FIELDS(h,i,_123, UPCXX_SERIALIZED_BASE(nonpod5_side))
};

struct nonpod6: nonpod_base, nonpod5_side {
  nonpod6(): nonpod_base(), nonpod5_side(123) {}
  nonpod6(char h, char i): nonpod_base(h,i), nonpod5_side(123) {}
  nonpod6(char h, char i, nonpod5_side side): nonpod_base(h,i), nonpod5_side(side) {}
  UPCXX_SERIALIZED_VALUES(h,i, UPCXX_SERIALIZED_BASE(nonpod5_side))
};

static_assert(is_trivially_serializable<const int>::value, "Uh-oh.");

static_assert(is_trivially_serializable<std::pair<int,char>>::value, "Uh-oh.");
static_assert(is_trivially_serializable<std::pair<const int,char>>::value, "Uh-oh.");
static_assert(serialization_traits<std::pair<int,char>>::is_actually_trivially_serializable, "Uh-oh.");
static_assert(serialization_traits<std::pair<const int,char>>::is_actually_trivially_serializable, "Uh-oh.");

static_assert(is_trivially_serializable<std::tuple<int,char>>::value, "Uh-oh.");
static_assert(is_trivially_serializable<std::tuple<const int,char>>::value, "Uh-oh.");
static_assert(serialization_traits<std::tuple<int,char>>::is_actually_trivially_serializable, "Uh-oh.");
static_assert(serialization_traits<std::tuple<const int,char>>::is_actually_trivially_serializable, "Uh-oh.");

static_assert(is_serializable<std::hash<int>>::value, "Uh-oh.");
static_assert(is_trivially_serializable<std::hash<int>>::value, "Uh-oh.");
static_assert(is_serializable<std::hash<std::string>>::value, "Uh-oh.");
static_assert(is_trivially_serializable<std::equal_to<std::vector<int>>>::value, "Uh-oh.");

static_assert(!is_trivially_serializable<std::string>::value, "Uh-oh");

static_assert(!is_trivially_serializable<nonpod1>::value, "Uh-oh");
static_assert(is_serializable<nonpod1>::value, "Uh-oh");

static_assert(!is_trivially_serializable<nonpod2>::value, "Uh-oh");
static_assert(is_serializable<nonpod2>::value, "Uh-oh");

static_assert(!is_trivially_serializable<nonpod3>::value, "Uh-oh");
static_assert(is_serializable<nonpod3>::value, "Uh-oh");

static_assert(!is_trivially_serializable<nonpod4>::value, "Uh-oh");
static_assert(is_serializable<nonpod4>::value, "Uh-oh");

static_assert(!is_trivially_serializable<nonpod5>::value, "Uh-oh");
static_assert(is_serializable<nonpod5>::value, "Uh-oh");

struct asym_type {
  struct upcxx_serialization {
    template<typename W>
    static void serialize(W &w, asym_type const &x) {
      w.template write<int>(123);
    }
    template<typename R>
    static int* deserialize(R &r, void *spot) {
      return ::new int(r.template read<int>());
    }
  };
};

static_assert(is_serializable<asym_type>::value, "Uh-oh.");
static_assert(!is_trivially_serializable<asym_type>::value, "Uh-oh.");
static_assert(std::is_same<int, deserialized_type_t<asym_type>>::value, "Uh-oh.");

struct mod_eq {
  int mod;
  bool operator()(int a, int b) const { return a%mod == b%mod; }
};

struct mod_hash: nonpod_base {
  int mod;
  mod_hash(int m): mod(m) {}
  std::size_t operator()(int a) const { return ((a % mod) + mod) % mod; }
  UPCXX_SERIALIZED_VALUES(mod)
};

template<typename Derived, typename T>
struct my_seq_base {
  // test inheritance of upcxx_serialization
  struct upcxx_serialization {
    template<typename Writer>
    static void serialize(Writer &w, Derived const &x) {
      auto handle = w.template reserve<int>();
      int n = (int)x.elts.size();
      uint64_t rng = 0xbeef;

      // perform write_sequence's of random length until all n elts have been written
      for(int i=0; i < n;) {
        int n1 = rng % (n - i + 1);
        int i1 = i + n1;
        rng ^= rng >> 30;
        rng *= 0x1234567890abcdef;
        rng += 0xbeef;
        
        w.write_sequence(x.elts.begin() + i, x.elts.begin() + i1, n1); // with explicit elt count
        i = i1;
        
        n1 = rng % (n - i + 1);
        i1 = i + n1;
        rng ^= rng >> 30;
        rng *= 0x1234567890abcdef;
        rng += 0xbeef;
        
        w.write_sequence(x.elts.begin() + i, x.elts.begin() + i1); // without explicit elt count
        i = i1;
      }
      
      w.template commit<int>(handle, n);
    }

    template<typename Reader>
    static Derived* deserialize(Reader &r, void *spot) {
      int n = r.template read<int>();
      void *mem = ::operator new(n*sizeof(T));

      T *elts = nullptr;
      if(n > 0) {
        // deserialize one elt individually
        elts = r.template read_into<T>(mem);
        // then the rest as a sequence
        r.template read_sequence_into<T>(elts+1, n-1);
      }

      Derived *ans = ::new(spot) Derived(decltype(Derived::elts)(elts, elts + n));
      for(int i=0; i < n; i++)
        elts[i].~T();
      ::operator delete(mem);
      return ans;
    }
  };
};

template<typename T>
struct my_seq1: public my_seq_base<my_seq1<T>, T> {
  // inherits upcxx_serialization
  
  std::vector<T> elts;
  my_seq1(std::vector<T> elts): elts(std::move(elts)) {}

  friend bool operator==(my_seq1 const &a, my_seq1 const &b) {
    return a.elts == b.elts;
  }
};

template<typename T>
struct my_seq2: public my_seq_base<my_seq2<T>, T> {
  // inherits upcxx_serialization

  std::deque<T> elts;
  my_seq2(std::deque<T> elts): elts(std::move(elts)) {}

  friend bool operator==(my_seq2 const &a, my_seq2 const &b) {
    return a.elts == b.elts;
  }
};

struct noserz {
  UPCXX_SERIALIZED_DELETE()
};

int main() {
#if UPCXX_BACKEND
  upcxx::init();
#endif

  print_test_header();

  #if 0
    // instantiating serialization when DELETED dies compile time
    upcxx::serialization_traits<noserz>::deserialized_value(noserz{});
  #endif
  // compile time queries when serialization is DELETED still work
  static_assert(!is_serializable<noserz>::value, "Uh-oh.");
  
  // roundtrip checks using serialization_traits::deserialized_value()
  UPCXX_ASSERT_ALWAYS(upcxx::serialization_traits<int>::deserialized_value(0xbeef) == 0xbeef);
  UPCXX_ASSERT_ALWAYS(upcxx::serialization_traits<std::string>::deserialized_value(std::string(10000,'x')) == std::string(10000,'x'));
  {
    std::forward_list<int> fwd0;
    for(int i=0; i < 10000; i++)
      fwd0.push_front(i);
    std::forward_list<int> fwd1 = upcxx::serialization_traits<std::forward_list<int>>::deserialized_value(fwd0);
    UPCXX_ASSERT_ALWAYS(fwd0 == fwd1);
  }
  {
    std::forward_list<std::string> fwd0;
    for(int i=0; i < 10000; i++)
      fwd0.push_front(std::string(i%300, 'a' + i%26));
    std::forward_list<std::string> fwd1 = upcxx::serialization_traits<std::forward_list<std::string>>::deserialized_value(fwd0);
    UPCXX_ASSERT_ALWAYS(fwd0 == fwd1);
  }

  // roundtrip checks not relying on serialization_traits::deserialized_value()
  roundtrip<std::int8_t>(1);
  roundtrip<std::uint16_t>(1000);
  roundtrip<std::int32_t>(1<<29);
  roundtrip<std::uint32_t>(1u<<31);
  roundtrip<float>(3.14f);
  roundtrip<double>(3.14);
  roundtrip(nonpod1('h','i'));
  roundtrip(std::array<int,10>{{0,1,2,3,4,5,6,7,8,9}});
  roundtrip<int[10]>({0,1,2,3,4,5,6,7,8,9});
  roundtrip<nonpod2[3]>({{'a','b'}, {'x','y'}, {'u','v'}});
  roundtrip(std::make_pair('a', 1));
  roundtrip(std::make_pair('a', nonpod3('h','i')));
  roundtrip(std::make_tuple('a', 1, 3.14));
  roundtrip(std::make_tuple('a', 1, 3.14, std::string("abcdefghijklmnopqrstuvwxyz")));
  roundtrip(std::vector<int>{1,2,3});
  roundtrip(std::deque<nonpod1>{{'a','b'}, {'x','y'}});
  roundtrip<std::vector<std::list<std::tuple<std::string,nonpod2>>>>(
    std::initializer_list<std::list<std::tuple<std::string,nonpod2>>>{
      {},
      std::initializer_list<std::tuple<std::string,nonpod2>>{
        std::tuple<std::string,nonpod2>("hi",{'a','b'}),
        std::tuple<std::string,nonpod2>("bob",{'x','y'}),
        std::tuple<std::string,nonpod2>("alice",{'\0','!'})
      }
    }
  );
  roundtrip<std::vector<std::list<std::tuple<std::string,nonpod3>>>>(
    std::initializer_list<std::list<std::tuple<std::string,nonpod3>>>{
      {},
      std::initializer_list<std::tuple<std::string,nonpod3>>{
        std::tuple<std::string,nonpod3>("hi",{'a','b'}),
        std::tuple<std::string,nonpod3>("bob",{'x','y'}),
        std::tuple<std::string,nonpod3>("alice",{'\0','!'})
      }
    }
  );
  roundtrip<std::vector<std::list<std::tuple<std::string,nonpod4>>>>(
    std::initializer_list<std::list<std::tuple<std::string,nonpod4>>>{
      {},
      std::initializer_list<std::tuple<std::string,nonpod4>>{
        std::tuple<std::string,nonpod4>("hi",{'a','b'}),
        std::tuple<std::string,nonpod4>("bob",{'x','y'}),
        std::tuple<std::string,nonpod4>("alice",{'\0','!'})
      }
    }
  );
  roundtrip<std::vector<std::list<std::tuple<std::string,nonpod5>>>>(
    std::initializer_list<std::list<std::tuple<std::string,nonpod5>>>{
      {},
      std::initializer_list<std::tuple<std::string,nonpod5>>{
        std::tuple<std::string,nonpod5>("hi",{'a','b'}),
        std::tuple<std::string,nonpod5>("bob",{'x','y'}),
        std::tuple<std::string,nonpod5>("alice",{'\0','!'})
      }
    }
  );
  roundtrip<std::vector<std::list<std::tuple<std::string,nonpod6>>>>(
    std::initializer_list<std::list<std::tuple<std::string,nonpod6>>>{
      {},
      std::initializer_list<std::tuple<std::string,nonpod6>>{
        std::tuple<std::string,nonpod6>("hi",{'a','b'}),
        std::tuple<std::string,nonpod6>("bob",{'x','y'}),
        std::tuple<std::string,nonpod6>("alice",{'\0','!'})
      }
    }
  );

  {
    std::unordered_map<int, std::pair<int,std::string>> m;
    for(int i=0; i < 1000; i++)
      m[i] = {i, std::string(11*i,'x')};
    roundtrip(m);
  }
  { // test non-default key hasher and equality functors
    std::unordered_map<int, std::pair<int,std::string>, mod_hash, mod_eq> m(1, mod_hash(10), mod_eq{10});
    for(int i=0; i < 1000; i++)
      m[i] = {i, std::string(11*i,'x')};
    roundtrip(m);
  }
  {
    std::map<int, std::pair<int,std::string>> m;
    for(int i=0; i < 1000; i++)
      m[i] = {i, std::string(11*i,'x')};
    roundtrip(m);
  }
  {
    std::multiset<std::pair<int,std::string>> m;
    for(int i=0; i < 1000; i++)
      m.insert({i, std::string(11*i,'x')});
    roundtrip(m);
  }

  {
    std::vector<short> lots;
    for(int i=0; i < 1<<20; i++)
      lots.push_back(i & 0x1234);
    
    roundtrip<std::pair<nonpod2, my_seq1<my_seq1<short>>>>({
      {'u','v'},
      my_seq1<my_seq1<short>>(
        std::initializer_list<my_seq1<short>>{
          std::vector<short>{100, 200, 300},
          lots,
          std::vector<short>{},
          std::vector<short>{100, 200, 300}
        }
      )
    });
  }

  {
    std::deque<int> lots;
    for(int i=0; i < 1<<20; i++)
      lots.push_back(i);
    
    roundtrip<std::pair<nonpod3, my_seq2<my_seq2<int>>>>({
      {'u','v'},
      my_seq2<my_seq2<int>>(
        std::initializer_list<my_seq2<int>>{
          std::deque<int>{100, 200, 300},
          lots,
          std::deque<int>{},
          std::deque<int>{100, 200, 300}
        }
      )
    });
  }

  print_test_success();

#if UPCXX_BACKEND
  upcxx::finalize();
#endif
  return 0;
}
