#ifndef _f2c7c1fc_cd4a_4123_bf50_a6542f8efa2c
#define _f2c7c1fc_cd4a_4123_bf50_a6542f8efa2c

#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/future.hpp>
#include <upcxx/team.hpp>

namespace upcxx {
  namespace detail {
    ////////////////////////////////////////////////////////////////////
    // barrier_event_values: Value for completions_state's EventValues
    // template argument. barrier events always report no values.
    struct barrier_event_values {
      template<typename Event>
      using tuple_t = std::tuple<>;
    };

    void barrier_async_inject(const team &tm, backend::gasnet::handle_cb *cb);
  }
  
  void barrier(const team &tm = upcxx::world());
  
  template<typename Cxs = completions<future_cx<operation_cx_event>>>
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::barrier_event_values,
      Cxs
    >::return_t
  barrier_async(
      const team &tm = upcxx::world(),
      Cxs cxs = completions<future_cx<operation_cx_event>>({})
    ) {

    struct barrier_cb final: backend::gasnet::handle_cb {
      detail::completions_state<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::barrier_event_values,
        Cxs> state;

      barrier_cb(Cxs &&cxs): state(std::move(cxs)) {}
      
      void execute_and_delete(backend::gasnet::handle_cb_successor) {
        state.template operator()<operation_cx_event>();
        delete this;
      }
    };

    barrier_cb *cb = new barrier_cb(std::move(cxs));
    
    auto returner = detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::barrier_event_values,
        Cxs
      >(cb->state);
    
    detail::barrier_async_inject(tm, cb);
    
    return returner();
  }
}
#endif
