#ifndef _cfdca0c3_057d_4ee9_9c82_b68e4bc96a0f
#define _cfdca0c3_057d_4ee9_9c82_b68e4bc96a0f

#include <upcxx/backend.hpp>
#include <upcxx/bind.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/future.hpp>
#include <upcxx/team.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // detail::rpc_remote_results
  
  namespace detail {
    template<typename Call, typename = void>
    struct rpc_remote_results {
      // no `type`
    };
    template<typename Fn, typename ...Arg>
    struct rpc_remote_results<
        Fn(Arg...),
        decltype(
          std::declval<typename binding<Fn>::off_wire_type>()(
            std::declval<typename binding<Arg>::off_wire_type>()...
          ),
          void()
        )
      > {
      using type = typename decltype(
          upcxx::apply_as_future(
            std::declval<typename binding<Fn>::off_wire_type>(),
            std::declval<typename binding<Arg>::off_wire_type>()...
          )
        )::results_type;
    };
  }
  
  //////////////////////////////////////////////////////////////////////
  // rpc_ff

  namespace detail {
    struct rpc_ff_event_values {
      template<typename Event>
      using tuple_t = std::tuple<>;
    };

    template<typename Call, typename Cxs,
             typename = typename rpc_remote_results<Call>::type>
    struct rpc_ff_return;
    
    template<typename Call, typename ...Cx, typename Results>
    struct rpc_ff_return<Call, completions<Cx...>, Results> {
      using type = typename detail::completions_returner<
          /*EventPredicate=*/detail::event_is_here,
          /*EventValues=*/detail::rpc_ff_event_values,
          completions<Cx...>
        >::return_t;
    };
  }
  
  // defaulted completions
  template<typename Fn, typename ...Arg>
  auto rpc_ff(const team &tm, intrank_t recipient, Fn &&fn, Arg &&...args)
    // computes our return type, but SFINAE's out if fn(args...) is ill-formed
    -> typename detail::rpc_ff_return<Fn(Arg...), completions<>>::type {

    static_assert(
      detail::trait_forall<
          is_serializable,
          typename binding<Arg>::on_wire_type...
        >::value,
      "All rpc arguments must be Serializable."
    );
      
    backend::template send_am_master<progress_level::user>(
      tm, recipient,
      upcxx::bind(std::forward<Fn>(fn), std::forward<Arg>(args)...)
    );
  }
  
  template<typename Fn, typename ...Arg>
  auto rpc_ff(intrank_t recipient, Fn &&fn, Arg &&...args)
    // computes our return type, but SFINAE's out if fn(args...) is ill-formed
    -> typename detail::rpc_ff_return<Fn(Arg...), completions<>>::type {
    
    return rpc_ff(world(), recipient, std::forward<Fn>(fn), std::forward<Arg>(args)...);
  }

  // explicit completions
  template<typename Cxs, typename Fn, typename ...Arg>
  auto rpc_ff(const team &tm, intrank_t recipient, Cxs cxs, Fn &&fn, Arg &&...args)
    // computes our return type, but SFINAE's out if fn(args...) is ill-formed
    -> typename detail::rpc_ff_return<Fn(Arg...), Cxs>::type {

    static_assert(
      detail::trait_forall<
          is_serializable,
          typename binding<Arg>::on_wire_type...
        >::value,
      "All rpc arguments must be Serializable."
    );
      
    auto state = detail::completions_state<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rpc_ff_event_values,
        Cxs
      >{std::move(cxs)};
    
    auto returner = detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rpc_ff_event_values,
        Cxs
      >{state};
    
    backend::template send_am_master<progress_level::user>(
      tm, recipient,
      upcxx::bind(std::forward<Fn>(fn), std::forward<Arg>(args)...)
    );
    
    // send_am_master doesn't support async source-completion, so we know
    // its trivially satisfied.
    state.template operator()<source_cx_event>();
    
    return returner();
  }
  
  template<typename Cxs, typename Fn, typename ...Arg>
  auto rpc_ff(intrank_t recipient, Cxs cxs, Fn &&fn, Arg &&...args)
    // computes our return type, but SFINAE's out if fn(args...) is ill-formed
    -> typename detail::rpc_ff_return<Fn(Arg...), Cxs>::type {
  
    return rpc_ff(world(), recipient, std::move(cxs), std::forward<Fn>(fn), std::forward<Arg>(args)...);
  }
  
  //////////////////////////////////////////////////////////////////////
  // rpc
  
  namespace detail {
    template<typename PointerToLpcStalled>
    struct rpc_recipient_after;

    template<typename ...T>
    struct rpc_recipient_after<detail::lpc_dormant<T...>*> {
      intrank_t initiator;
      detail::lpc_dormant<T...> *remote_lpc;
      
      template<typename ...Arg>
      void operator()(Arg &&...arg) const {
        backend::template send_awaken_lpc<T...>(
          upcxx::world(), initiator,
          remote_lpc, std::tuple<T...>(std::forward<Arg>(arg)...)
        );
      }
    };

    template<typename Call>
    struct rpc_event_values;

    template<typename Fn, typename ...Arg>
    struct rpc_event_values<Fn(Arg...)> {
      using results_tuple = typename rpc_remote_results<Fn(Arg...)>::type;
      
      static_assert(
        is_serializable<results_tuple>::value,
        "rpc return values must be Serializable."
      );
      
      static_assert(
        (detail::binding_all_immediate<results_tuple>::value &&
         detail::trait_forall_tupled<serialization_references_buffer_not, results_tuple>::value),
        "rpc return values may not have type dist_object<T>&, team&, or view<T>."
      );
      
      template<typename Event>
      using tuple_t = typename std::conditional<
          std::is_same<Event, operation_cx_event>::value,
          /*Event == operation_cx_event:*/deserialized_type_t<results_tuple>,
          /*Event != operation_cx_event:*/std::tuple<>
        >::type;
    };
    
    // Computes return type for rpc.
    template<typename Call, typename Cxs,
             typename = typename rpc_remote_results<Call>::type>
    struct rpc_return;
    
    template<typename Call, typename ...Cx, typename Result>
    struct rpc_return<Call, completions<Cx...>, Result> {
      using type = typename detail::completions_returner<
          /*EventPredicate=*/detail::event_is_here,
          /*EventValues=*/detail::rpc_event_values<Call>,
          completions<Cx...>
        >::return_t;
    };
  }
  
  namespace detail {
    template<typename Cxs, typename Fn, typename ...Arg>
    auto rpc(const team &tm, intrank_t recipient, Cxs cxs, Fn &&fn, Arg &&...args)
      // computes our return type, but SFINAE's out if fn(args...) is ill-formed
      -> typename detail::rpc_return<Fn(Arg...), Cxs>::type {
      
      static_assert(
        detail::trait_forall<
            is_serializable,
            typename binding<Arg>::on_wire_type...
          >::value,
        "All rpc arguments must be Serializable."
      );
        
      using cxs_state_t = detail::completions_state<
          /*EventPredicate=*/detail::event_is_here,
          /*EventValues=*/detail::rpc_event_values<Fn&&(Arg&&...)>,
          Cxs
        >;
      
      cxs_state_t state(std::move(cxs));
      
      auto returner = detail::completions_returner<
          /*EventPredicate=*/detail::event_is_here,
          /*EventValues=*/detail::rpc_event_values<Fn&&(Arg&&...)>,
          Cxs
        >(state);
      
      intrank_t initiator = backend::rank_me;
      auto *op_lpc = std::move(state).template to_lpc_dormant<operation_cx_event>();
      
      using fn_bound_t = typename detail::bind<Fn&&, Arg&&...>::return_type;
      fn_bound_t fn_bound = upcxx::bind(std::forward<Fn>(fn), std::forward<Arg>(args)...);
      
      backend::template send_am_master<progress_level::user>(
        tm, recipient,
        upcxx::bind(
          [=](deserialized_type_t<fn_bound_t> &&fn_bound1) {
            return upcxx::apply_as_future(std::move(fn_bound1))
              .then(
                // Wish we could just use a lambda here, but since it has
                // to take variadic Arg... we have to call to an outlined
                // class. I'm not sure if even C++14's allowance of `auto`
                // lambda args would be enough.
                detail::rpc_recipient_after<decltype(op_lpc)>{
                  initiator, op_lpc
                }
              );
          },
          std::move(fn_bound)
        )
      );
      
      // send_am_master doesn't support async source-completion, so we know
      // its trivially satisfied.
      state.template operator()<source_cx_event>();
      
      return returner();
    }
  }
  
  template<typename Cxs, typename Fn, typename ...Arg>
  auto rpc(const team &tm, intrank_t recipient, Cxs cxs, Fn &&fn, Arg &&...args)
    // computes our return type, but SFINAE's out if fn(args...) is ill-formed
    -> typename detail::rpc_return<Fn(Arg...), Cxs>::type {
    
    return detail::template rpc<Cxs, Fn&&, Arg&&...>(
        tm, recipient, std::move(cxs), std::forward<Fn>(fn), std::forward<Arg>(args)...
      );
  }
  
  template<typename Cxs, typename Fn, typename ...Arg>
  auto rpc(intrank_t recipient, Cxs cxs, Fn &&fn, Arg &&...args)
    // computes our return type, but SFINAE's out if fn(args...) is ill-formed
    -> typename detail::rpc_return<Fn(Arg...), Cxs>::type {
    
    return detail::template rpc<Cxs, Fn&&, Arg&&...>(
        world(), recipient, std::move(cxs), std::forward<Fn>(fn), std::forward<Arg>(args)...
      );
  }
  
  // rpc: default completions variant
  template<typename Fn, typename ...Arg>
  auto rpc(const team &tm, intrank_t recipient, Fn &&fn, Arg &&...args)
    // computes our return type, but SFINAE's out if fn(args...) is ill-formed
    -> typename detail::rpc_return<Fn(Arg...), completions<future_cx<operation_cx_event>>>::type {
    
    return detail::template rpc<completions<future_cx<operation_cx_event>>, Fn&&, Arg&&...>(
      tm, recipient, operation_cx::as_future(),
      std::forward<Fn>(fn), std::forward<Arg>(args)...
    );
  }
  
  template<typename Fn, typename ...Arg>
  auto rpc(intrank_t recipient, Fn &&fn, Arg &&...args)
    // computes our return type, but SFINAE's out if fn(args...) is ill-formed
    -> typename detail::rpc_return<Fn(Arg...), completions<future_cx<operation_cx_event>>, typename detail::rpc_remote_results<Fn(Arg...)>::type>::type {
    
    return detail::template rpc<completions<future_cx<operation_cx_event>>, Fn&&, Arg&&...>(
      world(), recipient, operation_cx::as_future(),
      std::forward<Fn>(fn), std::forward<Arg>(args)...
    );
  }
}
#endif
