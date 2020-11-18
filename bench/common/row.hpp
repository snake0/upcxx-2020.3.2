#ifndef _8ac3790f_c4bf_4694_b235_b6120a960977
#define _8ac3790f_c4bf_4694_b235_b6120a960977

#include <cstring>
#include <ostream>

namespace bench {
  // A row<T...> is a conjunction of name=value assignments. All names are
  // `const char*` strings, values are type T...
  // Row's are equality comparable and hashable making them fit for use as keys
  // std::unordered_map.
  
  // To build a row use the `column(name,val)` function which will return a singleton
  // row and `operator&` to concatenate them. Ex:
  //   auto my_row = column("name", "john") & column("age", 101);
  //   // decltype(my_row) is row<const char*, int>
  // The type of the row depends on the left-to-right ordering of the values.
  // Equality and hashing are robust wrt to rows of different orderings (and
  // thus different types).
  
  template<typename ...T>
  struct row;
  
  namespace detail {
    template<typename T>
    void print_py_value(std::ostream &o, T const &val) {
      o << val;
    }
    
    inline void print_py_value(std::ostream &o, const char *val) {
      o << '"';
      for(const char *p = val; *p != 0; p++) {
        if(*p == '"' || *p == '\\')
          o << '\\';
        o << *p;
      }
      o << '"';
    }
    
    inline void print_py_value(std::ostream &o, std::string const &val) {
      print_py_value(o, val.c_str());
    }
    
    template<typename T>
    bool equals(T const &a, T const &b) {
      return a == b;
    }
    inline bool equals(const char *a, const char *b) {
      return 0 == std::strcmp(a, b);
    }
  }
  
  template<>
  struct row<> {
    void print_name_eq_val(std::ostream &o, bool leading_comma) const {}
  };
  
  template<typename A, typename ...B>
  struct row<A,B...> {
    const char *name_;
    A val_;
    row<B...> tail_;
    
    void print_name_eq_val(std::ostream &o, bool leading_comma) const {
      o << (leading_comma ? ", " : "") << name_ << '=';
      detail::print_py_value(o, val_);
      tail_.print_name_eq_val(o, true);
    }
  };
  
  template<typename ...T>
  std::ostream& operator<<(std::ostream &o, row<T...> const &x) {
    x.print_name_eq_val(o, false);
    return o;
  }
  
  // Build a row consisting of a single key=val pair. Combine multiple of these
  // with operator&.
  template<typename T>
  constexpr row<T> column(char const *name, T val) {
    return row<T>{name, std::move(val), row<>{}};
  }
  
  // operator& conjoins two rows into a result row.
  template<typename A0, typename ...A, typename ...B>
  constexpr row<A0,A...,B...> operator&(row<A0,A...> const &a, row<B...> const &b) {
    return {a.name_, a.val_, a.tail_ & b};
  }
  template<typename ...B>
  constexpr row<B...> operator&(row<> const &a, row<B...> const &b) {
    return b;
  }
  
  namespace detail {
    template<typename A, typename RowB>
    struct row_head_within;
    
    template<typename A>
    struct row_head_within<A, row<>> {
      bool operator()(char const *a_name, A const &a_val, row<> const &b) const {
        return false;
      }
    };
    template<typename A, typename ...B1>
    struct row_head_within<A, row<A,B1...>> {
      bool operator()(char const *a_name, A const &a_val, row<A,B1...> const &b) const {
        return (
            equals(a_name, b.name_) &&
            equals(a_val, b.val_)
          ) || row_head_within<A, row<B1...>>()(a_name, a_val, b.tail_);
      }
    };
    template<typename A, typename B0, typename ...B1>
    struct row_head_within<A, row<B0,B1...>> {
      bool operator()(char const *a_name, A const &a_val, row<B0,B1...> const &b) const {
        return row_head_within<A, row<B1...>>()(a_name, a_val, b.tail_);
      }
    };
    
    template<typename RowA, typename RowB>
    struct row_within;
    
    template<typename A0, typename ...A1, typename ...B>
    struct row_within<row<A0,A1...>, row<B...>> {
      bool operator()(row<A0,A1...> const &a, row<B...> const &b) const {
        return row_head_within<A0,row<B...>>()(a.name_, a.val_, b)
          && row_within<row<A1...>,row<B...>>()(a.tail_, b);
      }
    };
    
    template<typename ...B>
    struct row_within<row<>, row<B...>> {
      bool operator()(row<> const &a, row<B...> const &b) const {
        return true;
      }
    };
  }
  
  // operator==
  template<typename ...A, typename ...B>
  constexpr bool operator==(row<A...> const &a, row<B...> const &b) {
    return sizeof...(A) == sizeof...(B) && detail::row_within<row<A...>,row<B...>>()(a, b);
  }
  template<typename ...A, typename ...B>
  constexpr bool operator!=(row<A...> const &a, row<B...> const &b) {
    return sizeof...(A) != sizeof...(B) || !detail::row_within<row<A...>,row<B...>>()(a, b);
  }
}

namespace std {
  template<>
  struct hash<bench::row<>> {
    size_t operator()(bench::row<> const &x) const {
      return 0;
    }
  };
  
  template<typename A, typename ...B>
  struct hash<bench::row<A,B...>> {
    size_t operator()(bench::row<A,B...> const &x) const {
      size_t h = 0xdeadbeef;
      for(int i=0; x.name_[i] != 0; i++) {
        h += x.name_[i];
        h *= 31;
      }
      h ^= h >> 20;
      h ^= std::hash<A>()(x.val_);
      h *= 0xa74937b1;
      h ^= h >> 11;
      return h + std::hash<bench::row<B...>>()(x.tail_);
    }
  };
  
  template<typename ...B>
  struct hash<bench::row<const char*,B...>> {
    size_t operator()(bench::row<const char*,B...> const &x) const {
      size_t h = 0xdeadbeef;
      for(int i=0; x.name_[i] != 0; i++) {
        h += x.name_[i];
        h *= 31;
      }
      h ^= h >> 20;
      for(int i=0; x.val_[i] != 0; i++) {
        h += x.val_[i];
        h *= 31;
      }
      h *= 0xa74937b1;
      h ^= h >> 11;
      return h + std::hash<bench::row<B...>>()(x.tail_);
    }
  };
}

#endif
