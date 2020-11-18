#ifndef _e004d4d0_de3d_4761_a53a_85652b347070
#define _e004d4d0_de3d_4761_a53a_85652b347070

#include <upcxx/future/core.hpp>
#include <upcxx/future/apply.hpp>
#include <upcxx/future/impl_mapped.hpp>

namespace upcxx {
  namespace detail {
    ////////////////////////////////////////////////////////////////////
    // future_body_then: Body type for then's
    
    /* future_body_then_base: base class for future_body_then.
     * separates non-lambda specific code away from lambda specific code
     * to encourage code sharing by the compiler.
     */
    struct future_body_then_base: future_body {
      future_body_then_base(void *storage): future_body(storage) {}
      
      // future_body_then::leave_active calls this after lambda evaluation
      template<typename Kind, typename ...T>
      void leave_active_into_proxy(
          bool pure,
          future_header_dependent *hdr,
          void *storage,
          future1<Kind,T...> proxied
        ) {
        
        // drop one reference for lambda execution if impure,
        // another for active queue.
        if(0 == hdr->decref(pure ? 1 : 2)) {
          // we died
          operator delete(storage);
          delete hdr;
        }
        else {
          future_header *proxied_hdr = proxied.impl_.steal_header();
          
          if(Kind::template with_types<T...>::header_ops::is_trivially_ready_result) {
            operator delete(storage); // body dead
            // we know proxied_hdr is its own result
            hdr->enter_ready(proxied_hdr);
          }
          else {
            hdr->enter_proxying(
              ::new(storage) future_body_proxy<T...>(storage),
              proxied_hdr
            );
          }
        }
      }
    };
    
    template<typename FuArg, typename Fn>
    struct future_body_then final: future_body_then_base {
      future_dependency<FuArg> dep_;
      Fn fn_;
      
      future_body_then(
          void *storage, future_header_dependent *hdr,
          FuArg arg, Fn fn
        ):
        future_body_then_base(storage),
        dep_(hdr, std::move(arg)),
        fn_(std::move(fn)) {
      }
      
      // then's can't be destructed early so we inherit the stub
      //void destruct_early();
      
      void leave_active(future_header_dependent *hdr) {
        auto proxied = apply_futured_as_future<Fn&&, FuArg&>()(std::move(this->fn_), this->dep_);
        
        void *me_mem = this->storage_;
        this->dep_.cleanup_ready();
        this->~future_body_then();
        
        this->leave_active_into_proxy(
          /*pure=*/false, hdr, me_mem, std::move(proxied)
        );
      }
    };
    
    ////////////////////////////////////////////////////////////////////
    // future_body_then_pure: Body type for then_pure's
    
    template<typename FuArg, typename Fn>
    struct future_body_then_pure final: future_body_then_base {
      future_dependency<FuArg> dep_;
      Fn fn_;
      
      future_body_then_pure(
          void *storage, future_header_dependent *hdr,
          FuArg arg, Fn fn
        ):
        future_body_then_base(storage),
        dep_(hdr, std::move(arg)),
        fn_(std::move(fn)) {
      }
      
      // then_pure's can be destructed early so we override stub
      void destruct_early() {
        this->dep_.cleanup_early();
        this->~future_body_then_pure();
      }
      
      void leave_active(future_header_dependent *hdr) {
        auto proxied = apply_futured_as_future<Fn&&, FuArg&>()(std::move(this->fn_), this->dep_);
        
        void *me_mem = this->storage_;
        this->dep_.cleanup_ready();
        this->~future_body_then_pure();
        
        this->leave_active_into_proxy(
          /*pure=*/true, hdr, me_mem, std::move(proxied)
        );
      }
    };
  } // namespace detail
  
  //////////////////////////////////////////////////////////////////////
  // future_then:
    
  namespace detail {
    template<typename Arg, typename Fn,
             typename FnRetKind, typename ...FnRetT>
    struct future_then<
        Arg, Fn,
        future1<FnRetKind,FnRetT...>, /*arg_trivial=*/false
      > {

      using return_type = future1<future_kind_shref<future_header_ops_dependent>, FnRetT...>;
      
      template<typename Arg1, typename Fn1>
      return_type operator()(Arg1 &&arg, Fn1 &&fn) {
        future_header_dependent *hdr = new future_header_dependent;
        hdr->incref(1); // another for function execution
        
        union body_union_t {
          future_body_then<Arg,Fn> then;
          future_body_proxy<FnRetT...> proxy;
        };
        void *storage = future_body::operator new(sizeof(body_union_t));
        
        future_body_then<Arg,Fn> *body =
          ::new(storage) future_body_then<Arg,Fn>(
            storage, hdr,
            std::forward<Arg1>(arg),
            std::forward<Fn1>(fn)
          );
        hdr->body_ = body;
        
        if(hdr->status_ == future_header::status_active)
          hdr->entered_active();
        
        return future_impl_shref<future_header_ops_dependent, FnRetT...>(hdr);
      }
    };
    
    template<typename Arg, typename Fn,
             typename FnRetKind, typename ...FnRetT>
    struct future_then<
        Arg, Fn,
        future1<FnRetKind,FnRetT...>, /*arg_trivial=*/true
      > {

      using return_type = future1<FnRetKind,FnRetT...>;
      
      // Return type used to be: future1<future_kind_shref<future_header_ops_general>, FnRetT...>
      // Was there a reason for type-erasing it to the general kind?
      template<typename Arg1, typename Fn1>
      return_type operator()(Arg1 &&arg, Fn1 &&fn) {
        return apply_futured_as_future<Fn1&&, Arg1&&>()(
          std::forward<Fn1>(fn),
          std::forward<Arg1>(arg)
        );
      }
    };
  } // namespace detail

  #if 0
    // a>>b === a.then(b)
    template<typename ArgKind, typename Fn1, typename ...ArgT>
    inline auto operator>>(future1<ArgKind,ArgT...> arg, Fn1 &&fn)
      -> decltype(
        detail::future_then<
          future1<ArgKind,ArgT...>,
          typename std::decay<Fn1>::type
        >()(
          std::move(arg),
          std::forward<Fn1>(fn)
        )
      ) {
      return detail::future_then<
          future1<ArgKind,ArgT...>,
          typename std::decay<Fn1>::type
        >()(
          std::move(arg),
          std::forward<Fn1>(fn)
        );
    }
    
    template<typename Fn1, typename ...ArgT>
    inline future<ArgT...>& operator>>=(future<ArgT...> &arg, Fn1 &&fn) {
      arg = detail::future_then<
          future<ArgT...>,
          typename std::decay<Fn1>::type
        >()(
          std::move(arg),
          std::forward<Fn1>(fn)
        );
      return arg;
    }
  #endif
    
  namespace detail {
    template<typename Arg, typename Fn, typename FnRetKind, typename ...FnRetT>
    struct future_then_pure<
        Arg, Fn, future1<FnRetKind, FnRetT...>,
        /*arg_trivial=*/false,
        /*fnret_trivial=*/false
      > {

      using return_type = future1<future_kind_shref<future_header_ops_dependent>, FnRetT...>;
      
      template<typename Arg1, typename Fn1>
      return_type operator()(Arg1 &&arg, Fn1 &&fn) {
        future_header_dependent *hdr = new future_header_dependent;
        
        union body_union_t {
          future_body_then_pure<Arg,Fn> then;
          future_body_proxy<FnRetT...> proxy;
        };
        void *body_mem = future_body::operator new(sizeof(body_union_t));
        
        hdr->body_ = ::new(body_mem) future_body_then_pure<Arg,Fn>(
          body_mem, hdr,
          std::forward<Arg1>(arg),
          std::forward<Fn1>(fn)
        );
        
        if(hdr->status_ == future_header::status_active)
          hdr->entered_active();
        
        return future_impl_shref<future_header_ops_dependent, FnRetT...>(hdr);
      }
    };
    
    template<typename Arg, typename Fn, typename FnRetKind, typename ...FnRetT, bool fnret_trivial>
    struct future_then_pure<
        Arg, Fn, future1<FnRetKind, FnRetT...>,
        /*arg_trivial=*/true,
        fnret_trivial
      > {
      using return_type = future1<FnRetKind, FnRetT...> ;

      template<typename Arg1, typename Fn1>
      return_type operator()(Arg1 &&arg, Fn1 &&fn) {
        return apply_futured_as_future<Fn1&&,Arg1&&>()(
          std::forward<Fn1>(fn),
          std::forward<Arg1>(arg)
        );
      }
    };
    
    template<typename Arg, typename Fn, typename FnRetKind, typename ...FnRetT>
    struct future_then_pure<
        Arg, Fn, future1<FnRetKind, FnRetT...>,
        /*arg_trivial=*/false,
        /*fnret_trivial=*/true
      > {
      using return_type =future1<future_kind_mapped<Arg,Fn>, FnRetT...>;

      template<typename Arg1, typename Fn1>
      return_type operator()(Arg1 &&arg, Fn1 &&fn) {
        return future_impl_mapped<Arg,Fn,FnRetT...>{
          std::forward<Arg1>(arg),
          std::forward<Fn1>(fn)
        };
      }
    };
  } // namespace detail
}
#endif
