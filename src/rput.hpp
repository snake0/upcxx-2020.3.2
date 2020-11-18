#ifndef _f6435716_8dd3_47f3_9519_bf1663d2cb80
#define _f6435716_8dd3_47f3_9519_bf1663d2cb80

#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/serialization.hpp>

// For the time being, our implementation of put/get requires the
// gasnet backend. Ideally we would detect gasnet via UPCXX_BACKEND_GASNET
// and if not present, rely on a reference implementation over
// upcxx::backend generic API.
#include <upcxx/backend/gasnet/runtime.hpp>

namespace upcxx {
  namespace detail {
    // rma_put_sync: Enumerates both the least-acceptable amount of synchronicity
    // required at injection time, as well as amount of synchronicity actually
    // achieved. These are totally ordered such that if a < b, then b has strictly
    // more events synchronously complete.
    enum class rma_put_sync: int {
      // Explicitly assigned so that backend/gasnet/runtime.hpp can reliably
      // match them.
      src_cb=0,
      src_into_op_cb=1,
      src_now=2,
      op_now=3
    };
    
    // Does the actual gasnet PUT. Input sync_lb level is our demand, returned sync
    // level is guaranteed to be equal or greater.
    template<rma_put_sync sync_lb>
    rma_put_sync rma_put(
      intrank_t rank_d, void *buf_d,
      const void *buf_s, std::size_t size,
      backend::gasnet::handle_cb *source_cb,
      backend::gasnet::handle_cb *operation_cb
    );
    
    ////////////////////////////////////////////////////////////////////
    // rput_event_values: Value for completions_state's EventValues
    // template argument. rput events always report no values.
    struct rput_event_values {
      template<typename Event>
      using tuple_t = std::tuple<>;
    };
    
    ////////////////////////////////////////////////////////////////////////////
    
    template<typename Cxs, bool by_val>
    struct rput_traits {
      static constexpr bool want_op = completions_has_event<Cxs, operation_cx_event>::value;
      static constexpr bool op_is_sync = completions_is_event_sync<Cxs, operation_cx_event>::value;
      
      static constexpr bool want_src = by_val || completions_has_event<Cxs, source_cx_event>::value;
      static constexpr bool src_is_sync = by_val || completions_is_event_sync<Cxs, source_cx_event>::value;
      
      static constexpr bool want_remote = completions_has_event<Cxs, remote_cx_event>::value;

      using cx_state_here_t = detail::completions_state<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rput_event_values,
        Cxs>;
      using cx_state_remote_t = detail::completions_state<
        /*EventPredicate=*/detail::event_is_remote,
        /*EventValues=*/detail::rput_event_values,
        Cxs>;

      using return_t = typename detail::completions_returner<
          /*EventPredicate=*/detail::event_is_here,
          /*EventValues=*/detail::rput_event_values,
          Cxs
        >::return_t;

      template<typename T>
      static void assert_sane() {
        static_assert(
          is_trivially_serializable<T>::value,
          "RMA operations only work on TriviallySerializable types."
        );
        
        UPCXX_ASSERT_ALWAYS((want_op | want_remote),
          "Not requesting either operation or remote completion is surely an "
          "error. You'll have know way of ever knowing when the target memory is "
          "safe to read or write again."
        );
      }
    };

    ////////////////////////////////////////////////////////////////////////////

    template<typename Obj, typename Traits, bool op_is_handle>
    struct rput_op_handle_cb;

    template<typename Obj, typename Traits, bool src_is_handle, bool op_is_handle>
    struct rput_src_handle_cb;

    template<typename Obj, typename Traits, bool replies>
    struct rput_reply_cb;

    ////////////////////////////////////////////////////////////////////////////
    
    template<typename Obj, typename Traits, bool op_is_handle>
    struct rput_src_handle_cb<Obj, Traits, /*src_is_handle=*/true, op_is_handle>:
      backend::gasnet::handle_cb {
      backend::gasnet::handle_cb* the_src_cb(backend::gasnet::handle_cb *otherwise=nullptr) {
        return this;
      }

      void src_hook() {/*default is nop*/}
      
      template<typename=void> // prevents instantiation unless actually called lest the cast inside will type fail
      void add_op_suc(backend::gasnet::handle_cb_successor suc, std::true_type /*op_is_handle1*/) {
        Obj *me = static_cast<Obj*>(this);
        suc(static_cast<rput_op_handle_cb<Obj,Traits,true>*>(me));
      }
      void add_op_suc(backend::gasnet::handle_cb_successor suc, std::false_type /*op_is_handle*/) {
        // nop
      }
      
      void execute_and_delete(backend::gasnet::handle_cb_successor suc) {
        auto *o = static_cast<Obj*>(this);
        o->cx_state_here.template operator()<source_cx_event>();
        this->add_op_suc(suc, std::integral_constant<bool, op_is_handle>());
        o->src_hook(); // potentially overriden by Obj
      }
    };
    
    template<typename Obj, typename Traits, bool op_is_handle>
    struct rput_src_handle_cb<Obj, Traits, /*src_is_handle=*/false, op_is_handle> {
      constexpr backend::gasnet::handle_cb* the_src_cb(backend::gasnet::handle_cb *otherwise=nullptr) const {
        return otherwise;
      }

      void src_hook() {/*default is nop*/}
    };

    ////////////////////////////////////////////////////////////////////////////
    
    template<typename Obj, typename Traits>
    struct rput_op_handle_cb<Obj, Traits, /*op_is_handle=*/true>:
      backend::gasnet::handle_cb {
      backend::gasnet::handle_cb* the_op_cb() { return this; }

      void execute_and_delete(backend::gasnet::handle_cb_successor suc) {
        auto *o = static_cast<Obj*>(this);
        o->cx_state_here.template operator()<operation_cx_event>();
        delete o;
      }
    };

    template<typename Obj, typename Traits>
    struct rput_op_handle_cb<Obj, Traits, /*op_is_handle=*/false> {
      constexpr backend::gasnet::handle_cb* the_op_cb() const { return nullptr; }
    };

    ////////////////////////////////////////////////////////////////////////////

    template<typename Obj, typename Traits>
    struct rput_reply_cb<Obj, Traits, /*replies=*/true>:
      backend::gasnet::reply_cb {
      backend::gasnet::reply_cb* the_reply_cb() { return this; }

      void reply_hook() {/*default is nop*/}

      static void the_execute_and_delete(lpc_base *me_lpc) {
        auto *me = static_cast<rput_reply_cb*>(me_lpc);
        Obj *o = static_cast<Obj*>(me);
        o->reply_hook(); // potentially overriden by Obj
      }

      static constexpr lpc_vtable the_vtbl = {&the_execute_and_delete};

      rput_reply_cb(): reply_cb(&the_vtbl, &upcxx::current_persona()) {}
    };
    
    template<typename Obj, typename Traits>
    constexpr lpc_vtable rput_reply_cb<Obj,Traits,true>::the_vtbl;

    template<typename Obj, typename Traits>
    struct rput_reply_cb<Obj, Traits, /*replies=*/false> {
      constexpr backend::gasnet::reply_cb* the_reply_cb() const { return nullptr; }
      
      void reply_hook() {/*default is nop*/}
    };

    ////////////////////////////////////////////////////////////////////////////

    // The base object tracking everything about a rput injection. We specialize
    // on boolean traits to determine wire protocol. Presence of remote_cx
    // (want_remote=1) dictates we use gasnet::rma_put_then_am_master, otherwise
    // we use detail::rma_put.
    template<typename Obj, typename Traits,
             bool want_remote = Traits::want_remote,
             bool want_op = Traits::want_op,
             bool op_is_sync = Traits::op_is_sync,
             bool want_src = Traits::want_src>
    struct rput_obj_base;
    
    template<typename Obj, typename Traits, bool want_src>
    struct rput_obj_base<Obj, Traits,
        /*want_remote=*/true,
        /*want_op=*/true, /*op_is_sync=*/false,
        want_src
      >:
      rput_src_handle_cb<Obj, Traits, /*src_is_handle=*/!Traits::src_is_sync, /*op_is_handle=*/false>,
      rput_op_handle_cb<Obj, Traits, /*op_is_handle=*/false>,
      rput_reply_cb<Obj, Traits, /*replies=*/true> {
      
      std::int8_t outstanding = 2; // counts source and reply
      
      void src_hook() {
        if(--this->outstanding == 0) {
          static_cast<Obj*>(this)->cx_state_here.template operator()<operation_cx_event>();
          delete static_cast<Obj*>(this);
        }
      }
      
      void reply_hook() { this->src_hook(); }

      static constexpr rma_put_sync sync_lb = Traits::src_is_sync
        ? rma_put_sync::src_now
        : rma_put_sync::src_cb;

      static constexpr backend::gasnet::rma_put_then_am_sync sync_lb1 = Traits::src_is_sync
        ? backend::gasnet::rma_put_then_am_sync::src_now
        : backend::gasnet::rma_put_then_am_sync::src_cb;
      
      template<typename RemoteFn>
      rma_put_sync inject(
          intrank_t rank_d, void *buf_d, void const *buf_s, std::size_t buf_size,
          RemoteFn &&remote
        ) {
        //upcxx::say()<<"amlong with reply";
        auto *o = static_cast<Obj*>(this);

        auto sync_out = backend::gasnet::template rma_put_then_am_master<sync_lb1>(
          upcxx::world(), rank_d, buf_d, buf_s, buf_size,
          progress_level::user, std::move(remote),
          this->the_src_cb(),
          this->the_reply_cb()
        );

        if((int)sync_lb1 <= (int)backend::gasnet::rma_put_then_am_sync::src_cb &&
           (int)sync_out == (int)backend::gasnet::rma_put_then_am_sync::src_cb)
          return rma_put_sync::src_cb;
        
        if(sync_out == backend::gasnet::rma_put_then_am_sync::src_now)
          return rma_put_sync::src_now;
        return rma_put_sync::op_now;
      }
    };
    
    template<typename Obj, typename Traits, bool want_src>
    struct rput_obj_base<Obj, Traits,
        /*want_remote=*/true,
        /*want_op=*/true, /*op_is_sync=*/true,
        want_src
      >:
      rput_src_handle_cb<Obj, Traits, /*src_is_handle=*/false, /*op_is_handle=*/false>,
      rput_op_handle_cb<Obj, Traits, /*op_is_handle=*/false>,
      rput_reply_cb<Obj, Traits, /*replies=*/true> {

      bool remote_done = false;
      
      void reply_hook() {
        this->remote_done = true;
      }

      static constexpr rma_put_sync sync_lb = rma_put_sync::op_now;
      
      template<typename RemoteFn>
      rma_put_sync inject(
          intrank_t rank_d, void *buf_d, void const *buf_s, std::size_t buf_size,
          RemoteFn &&remote
        ) {
        //upcxx::say()<<"amlong with reply blocking";
        auto *o = static_cast<Obj*>(this);
        
        auto sync_out = backend::gasnet::template rma_put_then_am_master<
            backend::gasnet::rma_put_then_am_sync::src_now
          >(
          upcxx::world(), rank_d, buf_d, buf_s, buf_size,
          progress_level::user, std::move(remote),
          nullptr,
          static_cast<backend::gasnet::reply_cb*>(this)
        );

        if(sync_out == backend::gasnet::rma_put_then_am_sync::op_now)
          this->remote_done = true;
        
        while(!this->remote_done)
          upcxx::progress(upcxx::progress_level::internal);

        return rma_put_sync::op_now;
      }
    };

    template<typename Obj, typename Traits, bool want_src>
    struct rput_obj_base<Obj, Traits,
        /*want_remote=*/true,
        /*want_op=*/false, /*op_is_sync=*/false,
        want_src
      >:
      rput_src_handle_cb<Obj, Traits, /*src_is_handle=*/want_src && !Traits::src_is_sync, /*op_is_handle=*/false>,
      rput_op_handle_cb<Obj, Traits, /*op_is_handle=*/false> {

      // We handle absence of source_cx as assuming synchronous completion as
      // opposed to asynchronous (with an ignored notification) since this (naked
      // remote_cx) is a bizarre thing to ask for. So bizarre that I feel its more
      // likely a user mistake rather than they actually have a source buffer
      // that they feel safe giving over to us forever.
      static constexpr bool src_now = !want_src || Traits::src_is_sync;
      
      static constexpr rma_put_sync sync_lb = src_now
        ? rma_put_sync::src_now
        : rma_put_sync::src_cb;

      static constexpr backend::gasnet::rma_put_then_am_sync sync_lb1 = src_now
        ? backend::gasnet::rma_put_then_am_sync::src_now
        : backend::gasnet::rma_put_then_am_sync::src_cb;
      
      template<typename RemoteFn>
      rma_put_sync inject(
          intrank_t rank_d, void *buf_d, void const *buf_s, std::size_t buf_size,
          RemoteFn &&remote
        ) {
        //upcxx::say()<<"amlong without reply";
        auto sync_out = backend::gasnet::template rma_put_then_am_master<sync_lb1>(
          upcxx::world(), rank_d, buf_d, buf_s, buf_size,
          progress_level::user, std::move(remote),
          this->the_src_cb(), nullptr
        );

        if((int)sync_lb1 <= (int)backend::gasnet::rma_put_then_am_sync::src_cb &&
           (int)sync_out == (int)backend::gasnet::rma_put_then_am_sync::src_cb)
          return rma_put_sync::src_cb;
        
        if(sync_out == backend::gasnet::rma_put_then_am_sync::src_now)
          return rma_put_sync::src_now;
        return rma_put_sync::op_now;
      }
    };
    
    template<typename Obj, typename Traits, bool op_is_sync, bool want_src>
    struct rput_obj_base<Obj, Traits,
        /*want_remote=*/false,
        /*want_op=*/true, op_is_sync,
        want_src
      >:
      rput_src_handle_cb<Obj, Traits, /*src_is_handle=*/want_src && !Traits::src_is_sync, /*op_is_handle=*/!op_is_sync>,
      rput_op_handle_cb<Obj, Traits, /*op_is_handle=*/!op_is_sync> {

      static constexpr rma_put_sync sync_lb =
        op_is_sync          ? rma_put_sync::op_now :
        Traits::src_is_sync ? rma_put_sync::src_now :
        want_src            ? rma_put_sync::src_cb
                            : rma_put_sync::src_into_op_cb;

      template<typename RemoteFn>
      rma_put_sync inject(
          intrank_t rank_d, void *buf_d, void const *buf_s, std::size_t buf_size,
          RemoteFn&&
        ) {
        return detail::template rma_put<sync_lb>(
          rank_d, buf_d, buf_s, buf_size,
          this->the_src_cb(), this->the_op_cb()
        );
      }
    };

    ////////////////////////////////////////////////////////////////////////////
    
    template<typename Cxs, typename Traits>
    struct rput_obj final:
      detail::rput_obj_base<rput_obj<Cxs,Traits>, Traits> {

      typename Traits::cx_state_here_t cx_state_here;
      
      rput_obj(Cxs &&cxs):
        cx_state_here(std::move(cxs)) {
      }
    };

    ////////////////////////////////////////////////////////////////////////////
    
    template<typename Obj, typename Traits>
    void rput_post_inject(Obj *o, rma_put_sync sync_returned) {
      backend::gasnet::handle_cb *first_cb;

      // `Obj::sync_lb` is the sync level that was given to injection routine.
      // `sync_returned` is the possibly escalated value returned from injection.
      // Our job is to fire off completions and cleanup for any escalations.
      // The benefit is that if source or op completion were satisfied
      // immediately, we detect that and fire off completions code here *without*
      // incurring virtual dispatch.
      
      // We have guarantee that Obj::sync_lb <= sync_returned. We use that here
      // to help the optimizer know these conditions statically.
      if(rma_put_sync::src_now <= Obj::sync_lb ||
         rma_put_sync::src_now <= sync_returned
        ) {
        o->cx_state_here.template operator()<source_cx_event>();
        o->src_hook();
        
        if(rma_put_sync::op_now <= Obj::sync_lb ||
           rma_put_sync::op_now <= sync_returned
          ) {
          o->cx_state_here.template operator()<operation_cx_event>();
          delete o;
          return; // skips handle registration
        }
        else
          first_cb = o->the_op_cb(); // null if using injected with gasnet::rput_then_am_master
      }
      else
        first_cb = o->the_src_cb(/*otherwise=*/o->the_op_cb());
      
      if(first_cb != nullptr) // this nullness is always statically known, i'm trusting optimizer to see that
        backend::gasnet::register_cb(first_cb);
      
      backend::gasnet::after_gasnet();
    }
  } // namespace detail

  ////////////////////////////////////////////////////////////////////////////

  template<typename T,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  typename detail::rput_traits<Cxs, /*by_val=*/true>::return_t
  rput(T value_s,
       global_ptr<T> gp_d,
       Cxs cxs = completions<future_cx<operation_cx_event>>{{}}) {

    using traits_t = detail::rput_traits<Cxs, /*by_val=*/true>;
    using object_t = detail::rput_obj<Cxs, traits_t>;
    
    traits_t::template assert_sane<T>();

    UPCXX_GPTR_CHK(gp_d);
    UPCXX_ASSERT(gp_d, "pointer arguments to rput may not be null");
    
    object_t *o = new object_t(std::move(cxs));
    
    detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rput_event_values,
        Cxs
      > returner(o->cx_state_here);
    
    detail::rma_put_sync sync_done = o->inject(
      gp_d.rank_, gp_d.raw_ptr_, &value_s, sizeof(T),
      typename traits_t::cx_state_remote_t(std::move(cxs))
        .template bind_event<remote_cx_event>()
    );
    detail::template rput_post_inject<object_t, traits_t>(o, sync_done);
    return returner();
  }
  
  template<typename T,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  typename detail::rput_traits<Cxs, /*by_val=*/false>::return_t
  rput(T const *buf_s,
       global_ptr<T> gp_d,
       std::size_t n,
       Cxs cxs = completions<future_cx<operation_cx_event>>{{}}) {

    using traits_t = detail::rput_traits<Cxs, /*by_val=*/false>;
    using object_t = detail::rput_obj<Cxs, traits_t>;
    
    traits_t::template assert_sane<T>();

    UPCXX_GPTR_CHK(gp_d);
    UPCXX_ASSERT(buf_s && gp_d, "pointer arguments to rput may not be null");
    
    object_t *o = new object_t(std::move(cxs));
    
    detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rput_event_values,
        Cxs
      > returner(o->cx_state_here);
    
    detail::rma_put_sync sync_done = o->inject(
      gp_d.rank_, gp_d.raw_ptr_, buf_s, n*sizeof(T),
      typename traits_t::cx_state_remote_t(std::move(cxs))
        .template bind_event<remote_cx_event>()
    );
    detail::template rput_post_inject<object_t, traits_t>(o, sync_done);
    return returner();
  }
}
#endif
