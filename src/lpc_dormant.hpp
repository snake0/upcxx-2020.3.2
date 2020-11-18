#ifndef _2b6c472e_b888_4b91_9051_8f0b7aad9192
#define _2b6c472e_b888_4b91_9051_8f0b7aad9192

#include <upcxx/backend_fwd.hpp>
#include <upcxx/future.hpp>
#include <upcxx/lpc.hpp>

namespace upcxx {
  namespace detail {
    /* A dormant lpc is one that hasn't been enqueued to run, but knows its target
     * persona and progress level so that it may be enqueued later. The `T...`
     * values are a tuple of data this lpc is waiting for before it can be
     * enqueued.
     *
     * The address of a lpc_dormant is particularly useful to for communicating
     * the address of a continuation to survive a roundtrip.
     */
    template<typename ...T>
    struct lpc_dormant: lpc_base {
      persona *target;
      progress_level level;

      // enqueue all lpc's contained in the list for which `this` is the head.
      void awaken(std::tuple<T...> &&results);
    };

    // Make a lpc_dormant* from lambda.
    template<typename ...T, typename Fn>
    lpc_dormant<T...>* make_lpc_dormant(
        persona &target, progress_level level, Fn &&fn,
        lpc_dormant<T...> *tail
      );
    
    // Make lpc_dormant* which will enqueue a given quiesced promise (one which
    // has no requirement/fulfillment activity concurrently occurring) to apply
    // its deferred fulfillments.
    template<typename ...T>
    lpc_dormant<T...>* make_lpc_dormant_quiesced_promise(
        persona &target, progress_level level,
        detail::future_header_promise<T...> *pro, // takes ref
        lpc_dormant<T...> *tail
      );
    
    ////////////////////////////////////////////////////////////////////////////
    
    template<typename ...T>
    struct lpc_dormant_qpromise final: lpc_dormant<T...> {
      lpc_dormant_qpromise(persona &target, progress_level level, detail::future_header_promise<T...> *pro) {
        this->target = &target;
        this->level = level;
        this->vtbl = reinterpret_cast<lpc_vtable*>(0x1 | reinterpret_cast<std::uintptr_t>(pro));
      }
    };
    
    template<typename ...T>
    struct lpc_dormant_fn_base: lpc_dormant<T...> {
      union { std::tuple<T...> results; };
      lpc_dormant_fn_base() {}
      ~lpc_dormant_fn_base() {}
    };
    
    template<typename Fn, typename ...T>
    struct lpc_dormant_fn final: lpc_dormant_fn_base<T...> {
      Fn fn;

      template<int ...i>
      void apply_help(detail::index_sequence<i...>) {
        std::move(this->fn)(std::move(std::get<i>(this->results))...);
      }
      
      static void the_execute_and_delete(lpc_base *me1) {
        auto *me = static_cast<lpc_dormant_fn*>(me1);
        me->apply_help(detail::make_index_sequence<sizeof...(T)>());
        using results_t = std::tuple<T...>;
        me->results.~results_t();
        delete me;
      }
      
      static constexpr lpc_vtable the_vtbl = {&the_execute_and_delete};
      
      lpc_dormant_fn(persona &target, progress_level level, Fn &&fn):
        fn(std::forward<Fn>(fn)) {
        this->vtbl = &the_vtbl;
        this->target = &target;
        this->level = level;
      }
    };
    
    template<typename Fn, typename ...T>
    constexpr lpc_vtable lpc_dormant_fn<Fn,T...>::the_vtbl;

    // Make a lpc_dormant* from lambda.
    template<typename ...T, typename Fn1>
    lpc_dormant<T...>* make_lpc_dormant(
        persona &target, progress_level level, Fn1 &&fn,
        lpc_dormant<T...> *tail
      ) {
      using Fn = typename std::decay<Fn1>::type;
      auto *lpc = new lpc_dormant_fn<Fn,T...>(target, level, std::forward<Fn1>(fn));
      lpc->intruder.p.store(tail, std::memory_order_relaxed);
      return lpc;
    }

    // Make lpc_dormant* which will enqueue a given quiesced promise (one which
    // has no requirement/fulfillment activity concurrently occurring) to apply
    // its deferred fulfillments.
    template<typename ...T>
    lpc_dormant<T...>* make_lpc_dormant_quiesced_promise(
        persona &target, progress_level level,
        detail::future_header_promise<T...> *pro, // takes ref
        lpc_dormant<T...> *tail
      ) {
      auto *lpc = new lpc_dormant_qpromise<T...>(target, level, /*move ref*/pro);
      lpc->intruder.p.store(tail, std::memory_order_relaxed);
      return lpc;
    }

    template<typename ...T>
    void lpc_dormant<T...>::awaken(std::tuple<T...> &&results) {
      persona_tls &tls = the_persona_tls;
      lpc_dormant *p = this;
      
      do {
        lpc_dormant *next = static_cast<lpc_dormant*>(p->intruder.p.load(std::memory_order_relaxed));
        std::uintptr_t vtbl_u = reinterpret_cast<std::uintptr_t>(p->vtbl);
        
        if(vtbl_u & 0x1) {
          auto *pro = reinterpret_cast<future_header_promise<T...>*>(vtbl_u ^ 0x1);
          
          if(next == nullptr)
            pro->base_header_result.construct_results(std::move(results));
          else
            pro->base_header_result.construct_results(results);

          tls.enqueue_quiesced_promise(
            *p->target, p->level,
            /*move ref*/pro, /*result*/1 + /*anon*/0,
            /*known_active=*/std::integral_constant<bool, !UPCXX_BACKEND_GASNET_PAR>()
          );
          
          delete static_cast<lpc_dormant_qpromise<T...>*>(p);
        }
        else {
          auto *p1 = static_cast<lpc_dormant_fn_base<T...>*>(p);
          
          if(next == nullptr)
            ::new(&p1->results) std::tuple<T...>(std::move(results));
          else
            ::new(&p1->results) std::tuple<T...>(results);
          
          tls.enqueue(
            *p->target, p->level, p,
            /*known_active=*/std::integral_constant<bool, !UPCXX_BACKEND_GASNET_PAR>()
          );
        }
        
        p = next;
      }
      while(p != nullptr);
    }
  }
}
#endif
