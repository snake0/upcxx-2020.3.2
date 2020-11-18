#ifndef _c554ca08_f811_43b3_8a89_b20c52b59a1c
#define _c554ca08_f811_43b3_8a89_b20c52b59a1c

#include <upcxx/future/core.hpp>
#include <upcxx/future/apply.hpp>
#include <upcxx/future/body_pure.hpp>
#include <upcxx/utility.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // future_is_trivially_ready: future_impl_mapped specialization
  
  template<typename Arg, typename Fn, typename ...T>
  struct future_is_trivially_ready<
      future1<detail::future_kind_mapped<Arg,Fn>,T...>
    > {
    static constexpr bool value = future_is_trivially_ready<Arg>::value;
  };
  
  namespace detail {
    ////////////////////////////////////////////////////////////////////
    // future_impl_mapped: Pure-function bound to an underlying future.
    
    template<typename FuArg, typename Fn, typename ...T>
    struct future_impl_mapped {
      FuArg arg_;
      Fn fn_;
    
    public:
      future_impl_mapped(FuArg arg, Fn fn):
        arg_{std::move(arg)},
        fn_{std::move(fn)} {
      }
      
      bool ready() const {
        return this->arg_.impl_.ready();
      }
      
      struct result_lrefs_function {
        typedef apply_futured_as_future_return_t<Fn,FuArg> fn_return_t;
        typedef decltype(std::declval<fn_return_t>().impl_.result_lrefs_getter()) fn_return_getter_t;
        
        fn_return_t result_;
        fn_return_getter_t getter_;
        
        result_lrefs_function(fn_return_t result):
          result_{std::move(result)},
          getter_{result_.impl_.result_lrefs_getter()} {
        }
        
        result_lrefs_function(const result_lrefs_function &that):
          result_{that.result_},
          getter_{this->result_.impl_.result_lrefs_getter()} {
        }
        result_lrefs_function& operator=(const result_lrefs_function &that) {
          this->result_ = that.result_;
          this->getter_ = this->result_.impl_.result_lrefs_getter();
          return *this;
        }
        
        result_lrefs_function(result_lrefs_function &&that):
          result_{std::move(that.result_)},
          getter_{this->result_.impl_.result_lrefs_getter()} {
        }
        result_lrefs_function& operator=(result_lrefs_function &&that) {
          this->result_ = std::move(that.result_);
          this->getter_ = this->result_.impl_.result_lrefs_getter();
          return *this;
        }
        
        auto operator()() const
          UPCXX_RETURN_DECLTYPE(getter_()) {
          return getter_();
        }
      };
      
      result_lrefs_function result_lrefs_getter() const {
        return result_lrefs_function{
          apply_futured_as_future<Fn const&, FuArg const&>()(this->fn_, this->arg_)
        };
      }
      
      std::tuple<T...> result_rvals() {
        return std::tuple<T...>{
          apply_futured_as_future<Fn&, FuArg&>()(this->fn_, this->arg_)
            .impl_.result_lrefs_getter()()
        };
      }
      
    public:
      typedef future_header_ops_general header_ops;
      
      future_header* steal_header() {
        future_header_dependent *hdr = new future_header_dependent;
        
        typedef future_body_pure<future1<future_kind_mapped<FuArg,Fn>,T...>> body_type;
        void *body_mem = body_type::operator new(sizeof(body_type));
        
        body_type *body = ::new(body_mem) body_type{
          body_mem, hdr, std::move(*this)
        };
        hdr->body_ = body;
        
        if(hdr->status_ == future_header::status_active)
          hdr->entered_active();
        
        return hdr;
      }
    };
    
    
    //////////////////////////////////////////////////////////////////////
    // future_dependency: future_impl_mapped specialization
    
    template<typename FuArg, typename Fn, typename ...T>
    struct future_dependency<
        future1<future_kind_mapped<FuArg,Fn>, T...>
      > {
      future_dependency<FuArg> dep_;
      Fn fn_;
      
    public:
      future_dependency(
          future_header_dependent *suc_hdr,
          future1<future_kind_mapped<FuArg,Fn>, T...> arg
        ):
        dep_{suc_hdr, std::move(arg.impl_.arg_)},
        fn_{std::move(arg.impl_.fn_)} {
      }
      
      void cleanup_early() { dep_.cleanup_early(); }
      void cleanup_ready() { dep_.cleanup_ready(); }
      
      typedef typename future_impl_mapped<FuArg,Fn,T...>::result_lrefs_function result_lrefs_function;
      
      result_lrefs_function result_lrefs_getter() const {
        return result_lrefs_function{
          apply_futured_as_future<Fn const&, FuArg const&>()(this->fn_, this->dep_)
        };
      }
      
      future_header* cleanup_ready_get_header() {
        future_header *hdr = &(new future_header_result<T...>(
            /*not_ready=*/false,
            this->result_lrefs_getter()()
          ))->base_header;
        
        dep_.cleanup_ready();
        
        return hdr;
      }
    };
  }
}
#endif
