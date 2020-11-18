#ifndef _7949681d_8a89_4f83_afb9_de702bf1a46b
#define _7949681d_8a89_4f83_afb9_de702bf1a46b

#include <sstream>

namespace upcxx {
  void fatal_error(const char *msg, const char *title=nullptr, const char *file=0, int line=0);
  inline void fatal_error(const std::string &msg, const char *title=nullptr, const char *file=0, int line=0) {
    fatal_error(msg.c_str(), title, file, line);
  }

  void assert_failed(const char *file, int line, const char *msg=nullptr);
  inline void assert_failed(const char *file, int line, const std::string &str) {
    assert_failed(file, line, str.c_str());
  }
}

// unconditional fatal error, with file/line and custom message
#define UPCXX_FATAL_ERROR(ios_msg) \
      ::upcxx::fatal_error(([&]() { ::std::stringstream _upcxx_fatal_ss; \
                                     _upcxx_fatal_ss << ios_msg; \
                                     return _upcxx_fatal_ss.str(); })(), \
                           nullptr, __FILE__, __LINE__)

#define UPCXX_ASSERT_1(ok) \
 ( (ok) ? (void)0 : \
      ::upcxx::assert_failed(__FILE__, __LINE__, ::std::string("Failed condition: " #ok)) )

#define UPCXX_ASSERT_2(ok, ios_msg) \
 ( (ok) ? (void)0 : \
      ::upcxx::assert_failed(__FILE__, __LINE__, \
        ([&]() { ::std::stringstream _upcxx_assert_ss; \
                 _upcxx_assert_ss << ios_msg; \
                 return _upcxx_assert_ss.str(); })()) )

#define UPCXX_ASSERT_DISPATCH(_1, _2, NAME, ...) NAME

#ifndef UPCXX_ASSERT_ENABLED
  #define UPCXX_ASSERT_ENABLED 0
#endif

// Assert that will only happen in debug-mode.
#if UPCXX_ASSERT_ENABLED
  #define UPCXX_ASSERT(...) UPCXX_ASSERT_DISPATCH(__VA_ARGS__, UPCXX_ASSERT_2, UPCXX_ASSERT_1, _DUMMY)(__VA_ARGS__)
#else
  #define UPCXX_ASSERT(...) ((void)0)
#endif

// Assert that happens regardless of debug-mode.
#define UPCXX_ASSERT_ALWAYS(...) UPCXX_ASSERT_DISPATCH(__VA_ARGS__, UPCXX_ASSERT_2, UPCXX_ASSERT_1, _DUMMY)(__VA_ARGS__)

// In debug mode this will abort. In non-debug this is a nop.
#define UPCXX_INVOKE_UB() UPCXX_ASSERT(false, "Undefined behavior!")

// static assert that is permitted in expression context
#define UPCXX_STATIC_ASSERT(cnd, msg) ([=](){static_assert(cnd, msg);})

namespace upcxx {
  // ostream-like class which will print to standard error with as
  // much atomicity as possible. Incluces current rank and trailing
  // newline.
  // usage:
  //   upcxx::say() << "hello world";
  // prints:
  //   [0] hello world \n
  class say {
    std::stringstream ss;
  public:
    say();
    ~say();
    
    template<typename T>
    say& operator<<(T const &that) {
      ss << that;
      return *this;
    }
  };
}

#endif
