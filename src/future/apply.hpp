#ifndef _e1661a2a_f7f6_44f7_97dc_4f3e6f4fd018
#define _e1661a2a_f7f6_44f7_97dc_4f3e6f4fd018

#include <upcxx/future/core.hpp>
#include <upcxx/future/make_future.hpp>
#include <upcxx/utility.hpp>

namespace upcxx {
namespace detail {
  //////////////////////////////////////////////////////////////////////
  
  // dispatches on return type of function call
  template<typename Fn, typename Args, typename ArgIxs, typename Return>
  struct apply_tupled_as_future_dispatch;

  template<typename Fn, typename Args, typename Return>
  struct apply_variadic_as_future_dispatch;

  //////////////////////////////////////////////////////////////////////
  // detail::apply_***_as_future_dispatch: return = void

  template<typename Fn, typename Args, int ...ai>
  struct apply_tupled_as_future_dispatch<
      Fn, Args,
      /*ArgIxs=*/detail::index_sequence<ai...>,
      /*Return=*/void
    > {
    using return_type = detail::make_future<>::return_type;
    
    return_type operator()(Fn fn, Args args) {
      static_cast<Fn&&>(fn)(std::get<ai>(static_cast<Args&&>(args))...);
      return upcxx::make_future();
    }
  };

  template<typename Fn, typename ...Arg>
  struct apply_variadic_as_future_dispatch<
      Fn, /*Args=*/std::tuple<Arg...>, /*Return=*/void
    > {
    using return_type = detail::make_future<>::return_type;
    
    return_type operator()(Fn fn, Arg ...arg) {
      static_cast<Fn&&>(fn)(static_cast<Arg&&>(arg)...);
      return upcxx::make_future();
    }
  };
  
  //////////////////////////////////////////////////////////////////////
  // detail::apply_***_as_future_dispatch: return not a future and return != void

  template<typename Fn, typename Args, int ...ai, typename Return>
  struct apply_tupled_as_future_dispatch<
      Fn, Args,
      /*ArgIxs=*/detail::index_sequence<ai...>,
      Return
    > {
    using return_type = typename detail::make_future<Return>::return_type;
    
    return_type operator()(Fn fn, Args args) {
      return upcxx::make_future<Return>(
        static_cast<Fn&&>(fn)(std::get<ai>(static_cast<Args&&>(args))...)
      );
    }
  };

  template<typename Fn, typename ...Arg, typename Return>
  struct apply_variadic_as_future_dispatch<
      Fn, /*Args=*/std::tuple<Arg...>, Return
    > {
    using return_type = typename detail::make_future<Return>::return_type;
    
    return_type operator()(Fn fn, Arg ...arg) {
      return upcxx::make_future<Return>(
        static_cast<Fn&&>(fn)(static_cast<Arg&&>(arg)...)
      );
    }
  };
  
  //////////////////////////////////////////////////////////////////////
  // detail::apply_***_as_future_dispatch: return is future

  template<typename Fn, typename Args, int ...ai, typename Kind, typename ...T>
  struct apply_tupled_as_future_dispatch<
      Fn, Args,
      /*ArgIxs=*/detail::index_sequence<ai...>,
      /*Return=*/future1<Kind,T...>
    > {
    using return_type = future1<Kind,T...>;

    return_type operator()(Fn fn, Args args) {
      return static_cast<Fn&&>(fn)(std::get<ai>(static_cast<Args&&>(args))...);
    }
  };
  
  template<typename Fn, typename ...Arg, typename Kind, typename ...T>
  struct apply_variadic_as_future_dispatch<
      Fn, /*Args=*/std::tuple<Arg...>, /*Return=*/future1<Kind,T...>
    > {
    using return_type = future1<Kind,T...>;

    return_type operator()(Fn fn, Arg ...arg) {
      return static_cast<Fn&&>(fn)(static_cast<Arg&&>(arg)...);
    }
  };
  
  //////////////////////////////////////////////////////////////////////
  // future/core.hpp: detail::pply_tupled_as_future

  template<typename Fn, typename Args,
           typename ArgsD = typename std::decay<Args>::type>
  struct apply_tupled_as_future_help;

  template<typename Fn, typename Args, typename ...Arg>
  struct apply_tupled_as_future_help<
      Fn, Args, /*ArgD=*/std::tuple<Arg...>
    >:
    apply_tupled_as_future_dispatch<
      Fn, Args,
      /*ArgIxs=*/detail::make_index_sequence<sizeof...(Arg)>,
      /*Return=*/typename std::result_of<Fn(Arg...)>::type
    > {
  };
  
  template<typename Fn, typename Args>
  struct apply_tupled_as_future: apply_tupled_as_future_help<Fn,Args> {};

  //////////////////////////////////////////////////////////////////////
  // future/core.hpp: detail::apply_futured_as_future

  template<typename Fn, typename Arg,
           typename ArgD = typename std::decay<Arg>::type>
  struct apply_futured_as_future_help;

  template<typename Fn, typename Arg, typename Kind, typename ...T>
  struct apply_futured_as_future_help<Fn, Arg, /*ArgD=*/future1<Kind,T...>> {
    using tupled = apply_tupled_as_future_help<
      Fn, /*Args=*/std::tuple<typename detail::add_lref_if_nonref<T>::type...>
    >;
    
    using return_type = typename tupled::return_type;
    
    return_type operator()(Fn fn, Arg arg) {
      return tupled()(
        static_cast<Fn&&>(fn),
        static_cast<Arg&&>(arg).impl_.result_lrefs_getter()()
      );
    }
    
    return_type operator()(Fn fn, future_dependency<future1<Kind,T...>> const &arg) {
      return tupled()(
        static_cast<Fn&&>(fn),
        arg.result_lrefs_getter()()
      );
    }
  };

  template<typename Fn, typename Arg>
  struct apply_futured_as_future: apply_futured_as_future_help<Fn,Arg> {};
}}

namespace upcxx {
  template<typename Fn, typename ...Arg>
  auto apply_as_future(Fn &&fn, Arg &&...arg)
    UPCXX_RETURN_DECLTYPE(
      detail::apply_variadic_as_future_dispatch<
          Fn&&, std::tuple<Arg&&...>,
          typename std::result_of<Fn&&(Arg&&...)>::type
        >()(static_cast<Fn&&>(fn), static_cast<Arg&&>(arg)...)
    ) {
    return detail::apply_variadic_as_future_dispatch<
        Fn&&, std::tuple<Arg&&...>,
        typename std::result_of<Fn&&(Arg&&...)>::type
      >()(static_cast<Fn&&>(fn), static_cast<Arg&&>(arg)...);
  }

  template<typename Fn, typename Args>
  auto apply_tupled_as_future(Fn &&fn, Args &&args)
    UPCXX_RETURN_DECLTYPE(
      detail::apply_tupled_as_future<Fn&&,Args&&>()(
        static_cast<Fn&&>(fn), static_cast<Args&&>(args)
      )
    ) {
    return detail::apply_tupled_as_future<Fn&&,Args&&>()(
      static_cast<Fn&&>(fn), static_cast<Args&&>(args)
    );
  }
}
#endif
