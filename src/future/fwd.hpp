#ifndef _251dbe84_2eaa_42c1_9288_81c834900371
#define _251dbe84_2eaa_42c1_9288_81c834900371

#include <tuple>

////////////////////////////////////////////////////////////////////////////////
// Forwards of future API

namespace upcxx {
  namespace detail {
    struct future_header;
    template<typename ...T>
    struct future_header_result;
    template<typename ...T>
    struct future_header_promise;
    struct future_header_dependent;
    
    template<typename FuArg>
    struct future_dependency;
    
    struct future_body;
    struct future_body_proxy_;
    template<typename ...T>
    struct future_body_proxy;
    template<typename FuArg>
    struct future_body_pure;
    template<typename FuArg, typename Fn>
    struct future_body_then;
    
    // classes of all-static functions for working with headers
    struct future_header_ops_general;
    struct future_header_ops_result;
    struct future_header_ops_result_ready;
    struct future_header_ops_promise;
    struct future_header_ops_dependent;
    
    // future implementations
    template<typename HeaderOps, typename ...T>
    struct future_impl_shref;
    template<typename ...T>
    struct future_impl_result;
    template<typename ArgTuple, typename ...T>
    struct future_impl_when_all;
    template<typename FuArg, typename Fn, typename ...T>
    struct future_impl_mapped;
    
    // future1 implementation mappers
    template<typename HeaderOps>
    struct future_kind_shref {
      template<typename ...T>
      using with_types = future_impl_shref<HeaderOps,T...>;
    };
    
    struct future_kind_result {
      template<typename ...T>
      using with_types = future_impl_result<T...>;
    };
    
    template<typename ...FuArg>
    struct future_kind_when_all {
      template<typename ...T>
      using with_types = future_impl_when_all<std::tuple<FuArg...>, T...>;
    };
    
    template<typename FuArg, typename Fn>
    struct future_kind_mapped {
      template<typename ...T>
      using with_types = future_impl_mapped<FuArg,Fn,T...>;
    };
  }
  
  //////////////////////////////////////////////////////////////////////
  // future1: The type given to users.
  // implemented in: upcxx/future/future1.hpp
  
  template<typename Kind, typename ...T>
  struct future1;
  
  //////////////////////////////////////////////////////////////////////
  // future: An alias for future1 using a shared reference implementation.
  
  template<typename ...T>
  using future = future1<
    detail::future_kind_shref<detail::future_header_ops_general>,
    T...
  >;
  
  template<typename ...T>
  class promise;
  
  //////////////////////////////////////////////////////////////////////
  // future_is_trivially_ready: Trait for detecting trivially ready
  // futures. Specializations provided in each future implementation.
  
  template<typename Future>
  struct future_is_trivially_ready/*{
    static constexpr bool value;
  }*/;
  
  //////////////////////////////////////////////////////////////////////
  // Future/continuation function-application support
  // implemented in: upcxx/future/apply.hpp
  
  namespace detail {
    // Apply function to tupled arguments lifting return to future.
    // Defined in: future/apply.hpp
    template<typename Fn, typename ArgTup>
    struct apply_tupled_as_future/*{
      typedef future1<Kind,U...> return_type;
      return_type operator()(Fn &&fn, std::tuple<T...> &&arg);
    }*/;
    
    // Apply function to results of future with return lifted to future.
    template<typename Fn, typename Arg>
    struct apply_futured_as_future/*{
      typedef future1<Kind,U...> return_type;
      return_type operator()(Fn fn, future1<Kind,T...> arg);
    }*/;
    
    template<typename Fn, typename Arg>
    using apply_futured_as_future_return_t = typename apply_futured_as_future<Fn,Arg>::return_type;
  }

  //////////////////////////////////////////////////////////////////////
  // detail::future_from_tuple: Generate future1 type from a Kind and
  // result types in a tuple.
  
  namespace detail {
    template<typename Kind, typename Tup>
    struct future_from_tuple;
    template<typename Kind, typename ...T>
    struct future_from_tuple<Kind, std::tuple<T...>> {
      using type = future1<Kind, T...>;
    };
    
    template<typename Kind, typename Tup>
    using future_from_tuple_t = typename future_from_tuple<Kind,Tup>::type;
  }
  
  //////////////////////////////////////////////////////////////////////
  // detail::future_then()(a,b): implementats `a.then(b)`
  // detail::future_then_pure()(a,b): implementats `a.then_pure(b)`
  // implemented in: upcxx/future/then.hpp
  
  namespace detail {
    template<
      typename Arg, typename Fn,
      typename FnRet = apply_futured_as_future_return_t<Fn,Arg>,
      bool arg_trivial = future_is_trivially_ready<Arg>::value>
    struct future_then;
    
    template<
      typename Arg, typename Fn,
      typename FnRet = apply_futured_as_future_return_t<Fn,Arg>,
      bool arg_trivial = future_is_trivially_ready<Arg>::value,
      bool fnret_trivial = future_is_trivially_ready<FnRet>::value>
    struct future_then_pure;
  }
}
#endif
