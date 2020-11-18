#ifndef _eb1a60f5_4086_4689_a513_8486eacfd815
#define _eb1a60f5_4086_4689_a513_8486eacfd815

#include <upcxx/future/core.hpp>
#include <upcxx/future/impl_when_all.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // when_all()
  
  namespace detail {
    template<typename Ans, typename ...Tups>
    struct tuple_cat_return;
      
    template<typename Ans>
    struct tuple_cat_return<Ans> {
      using type = Ans;
    };

    template<typename ...Acc, typename ...Tup0_Args, typename ...Tups>
    struct tuple_cat_return<std::tuple<Acc...>, std::tuple<Tup0_Args...>, Tups...> {
      using type = typename tuple_cat_return<std::tuple<Acc..., Tup0_Args...>, Tups...>::type;
    };
    
    // compute return type of when_all
    template<typename ...Arg>
    using when_all_return_t = 
      future_from_tuple_t<
        future_kind_when_all<Arg...>,
        #if 0
        decltype(std::tuple_cat(
          std::declval<typename Arg::results_type>()...
        ))
        #else
        typename tuple_cat_return<std::tuple<>, typename Arg::results_type...>::type
        #endif
      >;
  }
  
  template<typename ...Arg>
  detail::when_all_return_t<Arg...> when_all(Arg ...args) {
    return typename detail::when_all_return_t<Arg...>::impl_type{
      std::move(args)...
    };
  }
}
#endif
