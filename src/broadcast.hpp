#ifndef _b91be200_fd4d_41f5_a326_251161564ec7
#define _b91be200_fd4d_41f5_a326_251161564ec7

#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/bind.hpp>
#include <upcxx/team.hpp>

namespace upcxx {
  namespace detail {
    ////////////////////////////////////////////////////////////////////
    // broadcast_[scalar|vector]_event_values: Value for completions_state's
    // EventValues template argument.

    template<typename T>
    struct broadcast_scalar_event_values {
      template<typename Event>
      using tuple_t = typename std::conditional<
          std::is_same<Event, operation_cx_event>::value,
            std::tuple<T>,
            std::tuple<>
        >::type;
    };
    
    struct broadcast_vector_event_values {
      template<typename Event>
      using tuple_t = std::tuple<>;
    };
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // upcxx::broadcast_nontrivial
  
  template<typename T1,
           typename Cxs = completions<future_cx<operation_cx_event>>,
           typename T = typename std::decay<T1>::type>
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::broadcast_scalar_event_values<T>,
      Cxs
    >::return_t
  broadcast_nontrivial(
      T1 &&value, intrank_t root,
      const team &tm = upcxx::world(),
      Cxs cxs = completions<future_cx<operation_cx_event>>{{}}
    ) {

    using cxs_state_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::broadcast_scalar_event_values<T>,
      Cxs>;
    
    struct broadcast_state {
      int awaiting;
      union { T value; };
      union { cxs_state_t cxs_state; };
      
      broadcast_state() { awaiting = 2; }
      ~broadcast_state() {
        value.~T();
        cxs_state.~cxs_state_t();
      }

      void contribute(digest my_id) {
        if(0 == --this->awaiting) {
          this->cxs_state.template operator()<operation_cx_event>(std::move(this->value));
          delete this;
          detail::registry.erase(my_id);
        }
      }
    };
    
    digest id = const_cast<team*>(&tm)->next_collective_id(detail::internal_only());

    broadcast_state *s = detail::template registered_state<broadcast_state>(id);

    if(tm.rank_me() == root) {
      backend::bcast_am_master<progress_level::user>(
        tm,
        upcxx::bind([=](T &&value) {
            broadcast_state *s = detail::template registered_state<broadcast_state>(id);
            ::new(&s->value) T(std::move(value));
            s->contribute(id);
          },
          value
        )
      );

      ::new(&s->value) T(std::move(value));
      s->contribute(id);
    }

    ::new(&s->cxs_state) cxs_state_t(std::move(cxs));
    
    detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::broadcast_scalar_event_values<T>,
      Cxs>
    returner(s->cxs_state);
    
    s->contribute(id);
    
    return returner();
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // upcxx::broadcast
  
  namespace detail {
    // Calls GEX broadcast
    void broadcast_trivial(
      const team &tm, intrank_t root, void *buf, std::size_t size,
      backend::gasnet::handle_cb *cb
    );
  }
  
  template<typename T,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::broadcast_vector_event_values,
      Cxs
    >::return_t
  broadcast(
      T *buf, std::size_t n, intrank_t root,
      const team &tm = upcxx::world(),
      Cxs cxs = completions<future_cx<operation_cx_event>>{{}}
    ) {
    static_assert(
      upcxx::is_trivially_serializable<T>::value,
      "Only TriviallySerializable types permitted for `upcxx::broadcast`. "
      "Consider `upcxx::broadcast_nontrivial` instead (experimental feature, "
      "use at own risk)."
    );
    
    struct broadcast_cb final: backend::gasnet::handle_cb {
      detail::completions_state<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::broadcast_vector_event_values,
        Cxs> cxs_state;

      broadcast_cb(Cxs &&cxs): cxs_state(std::move(cxs)) {}
      
      void execute_and_delete(backend::gasnet::handle_cb_successor) override {
        cxs_state.template operator()<operation_cx_event>();
        delete this;
      }
    };
    
    broadcast_cb *cb = new broadcast_cb(std::move(cxs));
    
    detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::broadcast_vector_event_values,
        Cxs>
      returner(cb->cxs_state);

    detail::broadcast_trivial(tm, root, (void*)buf, n*sizeof(T), cb);

    return returner();
  }
  
  template<typename T1,
           typename Cxs = completions<future_cx<operation_cx_event>>,
           typename T = typename std::decay<T1>::type>
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::broadcast_scalar_event_values<T>,
      Cxs
    >::return_t
  broadcast(
      T1 value, intrank_t root,
      const team &tm = upcxx::world(),
      Cxs cxs = completions<future_cx<operation_cx_event>>{{}}
    ) {
    
    static_assert(
      upcxx::is_trivially_serializable<T>::value,
      "Only TriviallySerializable types permitted for `upcxx::broadcast`. "
      "Consider `upcxx::broadcast_nontrivial` instead (experimental feature, "
      "use at own risk)."
    );

    struct broadcast_cb final: backend::gasnet::handle_cb {
      T value;
      detail::completions_state<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::broadcast_scalar_event_values<T>,
        Cxs> cxs_state;
      
      broadcast_cb(T &&value, Cxs &&cxs):
        value(std::move(value)),
        cxs_state(std::move(cxs)) {
      }
      
      void execute_and_delete(backend::gasnet::handle_cb_successor) override {
        cxs_state.template operator()<operation_cx_event>(std::move(value));
        delete this;
      }
    };
    
    broadcast_cb *cb = new broadcast_cb(std::move(value), std::move(cxs));

    detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::broadcast_scalar_event_values<T>,
        Cxs>
      returner(cb->cxs_state);
    
    detail::broadcast_trivial(tm, root, (void*)&cb->value, sizeof(T), cb);

    return returner();
  }
}
#endif
