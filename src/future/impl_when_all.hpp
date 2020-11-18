#ifndef _3e3a0cc8_bebc_49a7_9f94_53f023c2bd53
#define _3e3a0cc8_bebc_49a7_9f94_53f023c2bd53

#include <upcxx/future/core.hpp>
#include <upcxx/future/body_pure.hpp>
#include <upcxx/utility.hpp>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // future_is_trivially_ready: future_impl_when_all specialization
  
  template<typename ...Arg, typename ...T>
  struct future_is_trivially_ready<
      future1<detail::future_kind_when_all<Arg...>, T...>
    > {
    static constexpr bool value = detail::trait_forall<upcxx::future_is_trivially_ready, Arg...>::value;
  };
  
  namespace detail {
    ////////////////////////////////////////////////////////////////////
    // future_impl_when_all: Future implementation concatenating
    // results of multiple futures.
    
    template<typename ArgTuple, typename ...T>
    struct future_impl_when_all;
    
    template<typename ...FuArg, typename ...T>
    struct future_impl_when_all<std::tuple<FuArg...>, T...> {
      std::tuple<FuArg...> args_;
    
    private:
      template<typename Bool0, typename ...Bools>
      static bool all_(Bool0 x0, Bools ...xs) { return x0 & all_(xs...); }
      static bool all_() { return true; }
      
      template<int ...i>
      bool ready_(detail::index_sequence<i...>) const {
        return all_(std::get<i>(this->args_).impl_.ready()...);
      }
      
      template<int ...i>
      struct result_lrefs_function {
        typedef std::tuple<decltype(std::declval<FuArg>().impl_.result_lrefs_getter())...> getters_tuple;
        
        getters_tuple getters_;
        
        result_lrefs_function(std::tuple<FuArg...> const &args):
          getters_{std::get<i>(args).impl_.result_lrefs_getter()...} {
        }
        
        auto operator()() const
          UPCXX_RETURN_DECLTYPE(std::tuple_cat(std::get<i>(getters_)()...)) {
          return std::tuple_cat(std::get<i>(getters_)()...);
        }
      };
      
      template<int ...i>
      result_lrefs_function<i...> result_lrefs_getter_(
          detail::index_sequence<i...>
        ) const {
        return result_lrefs_function<i...>{this->args_};
      }
      
      template<int ...i>
      auto result_rvals_(detail::index_sequence<i...>)
        UPCXX_RETURN_DECLTYPE(std::tuple_cat(
            std::get<i>(this->args_).impl_.result_rvals()...
          )
        ) {
        return std::tuple_cat(
          std::get<i>(this->args_).impl_.result_rvals()...
        );
      }
      
    public:
      future_impl_when_all(FuArg ...args):
        args_{std::move(args)...} {
      }
      
      bool ready() const {
        return this->ready_(detail::make_index_sequence<sizeof...(FuArg)>());
      }
      
      auto result_lrefs_getter() const
        UPCXX_RETURN_DECLTYPE(this->result_lrefs_getter_(detail::make_index_sequence<sizeof...(FuArg)>())) {
        return this->result_lrefs_getter_(detail::make_index_sequence<sizeof...(FuArg)>());
      }
      
      auto result_rvals()
        UPCXX_RETURN_DECLTYPE(this->result_rvals_(detail::make_index_sequence<sizeof...(FuArg)>())) {
        return this->result_rvals_(detail::make_index_sequence<sizeof...(FuArg)>());
      }
      
      //////////////////////////////////////////////////////////////////
      // *** Performance opportunity missed ***
      // Given knowledge that all of our arguments are trivially ready,
      // we could elide building a body just to see that it's immediately
      // active.
      
      typedef future_header_ops_general header_ops;
      
      future_header* steal_header() {
        future_header_dependent *hdr = new future_header_dependent;
        
        typedef future_body_pure<future1<future_kind_when_all<FuArg...>,T...>> body_type;
        void *body_mem = body_type::operator new(sizeof(body_type));
        
        hdr->body_ = ::new(body_mem) body_type{body_mem, hdr, std::move(*this)};
        
        if(hdr->status_ == future_header::status_active)
          hdr->entered_active();
        
        return hdr;
      }
    };
    
    
    //////////////////////////////////////////////////////////////////////
    // future_dependency: future_impl_when_all specialization
    
    template<int i, typename Arg>
    struct future_dependency_when_all_arg {
      future_dependency<Arg> dep_;
      
      future_dependency_when_all_arg(
          future_header_dependent *suc_hdr,
          Arg arg
        ):
        dep_{suc_hdr, std::move(arg)} {
      }
    };
    
    template<typename AllArg, typename IxSeq>
    struct future_dependency_when_all_base;
    
    template<typename ...Arg, typename ...T, int ...i>
    struct future_dependency_when_all_base<
        future1<future_kind_when_all<Arg...>, T...>,
        detail::index_sequence<i...>
      >:
      // variadically inherit from each future_dependency specialization
      private future_dependency_when_all_arg<i,Arg>... {
      
      typedef future_dependency_when_all_base<
          future1<future_kind_when_all<Arg...>, T...>,
          detail::index_sequence<i...>
        > this_t;
      
      future_dependency_when_all_base(
          future_header_dependent *suc_hdr,
          future1<future_kind_when_all<Arg...>, T...> all_args
        ):
        future_dependency_when_all_arg<i,Arg>(
          suc_hdr,
          std::move(std::get<i>(all_args.impl_.args_))
        )... {
      }
      
      template<typename ...U>
      static void force_each_(U ...x) {}
      
      void cleanup_early() {
        // run cleanup_early on each base class
        force_each_((
          static_cast<future_dependency_when_all_arg<i,Arg>*>(this)->dep_.cleanup_early(),
          0
        )...);
      }
      
      void cleanup_ready() {
        // run cleanup_ready on each base class
        force_each_((
          static_cast<future_dependency_when_all_arg<i,Arg>*>(this)->dep_.cleanup_ready(),
          0
        )...);
      }
      
      struct result_lrefs_function {
        std::tuple<
            decltype(std::declval<future_dependency<Arg>>().result_lrefs_getter())...
          > getters_;
        
        result_lrefs_function(this_t const *me):
          getters_{
            static_cast<future_dependency_when_all_arg<i,Arg> const*>(me)->dep_.result_lrefs_getter()...
          } {
        }
        
        auto operator()() const
          UPCXX_RETURN_DECLTYPE(std::tuple_cat(std::get<i>(getters_)()...)) {
          return std::tuple_cat(std::get<i>(getters_)()...);
        }
      };
      
      result_lrefs_function result_lrefs_getter() const {
        return result_lrefs_function{this};
      }
    };
    
    template<typename ...Arg, typename ...T>
    struct future_dependency<
        future1<future_kind_when_all<Arg...>, T...>
      >:
      future_dependency_when_all_base<
        future1<future_kind_when_all<Arg...>, T...>,
        detail::make_index_sequence<sizeof...(Arg)>
      > {
      
      future_dependency(
          future_header_dependent *suc_hdr,
          future1<future_kind_when_all<Arg...>, T...> arg
        ):
        future_dependency_when_all_base<
            future1<future_kind_when_all<Arg...>, T...>,
            detail::make_index_sequence<sizeof...(Arg)>
          >{suc_hdr, std::move(arg)} {
      }
      
      future_header* cleanup_ready_get_header() {
        future_header *hdr = &(new future_header_result<T...>(
            /*not_ready=*/false,
            /*values=*/this->result_lrefs_getter()()
          ))->base_header;
        this->cleanup_ready();
        return hdr;
      }
    };
  }
}
#endif
