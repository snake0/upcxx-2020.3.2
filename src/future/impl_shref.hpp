#ifndef _9a741fd1_dcc1_4fe9_829f_ae86fa6b86ab
#define _9a741fd1_dcc1_4fe9_829f_ae86fa6b86ab

#include <upcxx/future/core.hpp>
#include <upcxx/utility.hpp>

// TODO: Consider adding debug checks for operations against a moved-out of
// future_impl_shref.

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // future_is_trivially_ready: future_impl_shref specialization
  
  template<typename HeaderOps, typename ...T>
  struct future_is_trivially_ready<
      future1<detail::future_kind_shref<HeaderOps>, T...>
    > {
    static constexpr bool value = HeaderOps::is_trivially_ready_result;
  };
  
  namespace detail {
    ////////////////////////////////////////////////////////////////////
    // future_impl_shref: Future implementation using ref-counted
    // pointer to header.
    
    template<typename HeaderOps, typename ...T>
    struct future_impl_shref {
      future_header *hdr_;
    
    public:
      future_impl_shref() {
        this->hdr_ = future_header_nil::nil();
      }
      // hdr comes comes with a reference included for us
      template<typename Header>
      future_impl_shref(Header *hdr) {
        this->hdr_ = hdr;
      }
      future_impl_shref(future_header_result<T...> *hdr) {
        this->hdr_ = &hdr->base_header;
      }
      
      future_impl_shref(const future_impl_shref &that) {
        this->hdr_ = that.hdr_;
        HeaderOps::incref(this->hdr_);
      }
      template<typename HeaderOps1>
      future_impl_shref(
          const future_impl_shref<HeaderOps1,T...> &that,
          typename std::enable_if<std::is_base_of<HeaderOps,HeaderOps1>::value>::type* = 0
        ) {
        this->hdr_ = that.hdr_;
        HeaderOps1::incref(this->hdr_);
      }
      
      future_impl_shref& operator=(const future_impl_shref &that) {
        future_header *this_hdr = this->hdr_;
        future_header *that_hdr = that.hdr_;
        
        this->hdr_  = that_hdr;
        
        HeaderOps::incref(that_hdr);
        HeaderOps::template dropref<T...>(this_hdr, /*maybe_nil=*/std::true_type());
        
        return *this;
      }
      
      template<typename HeaderOps1>
      typename std::enable_if<
          std::is_base_of<HeaderOps,HeaderOps1>::value,
          future_impl_shref&
        >::type
      operator=(const future_impl_shref<HeaderOps1,T...> &that) {
        future_header *this_hdr = this->hdr_;
        future_header *that_hdr = that.hdr_;
        
        this->hdr_  = that_hdr;
        
        HeaderOps1::incref(that_hdr);
        HeaderOps::template dropref<T...>(this_hdr, /*maybe_nil=*/std::true_type());
        
        return *this;
      }
      
      future_impl_shref(future_impl_shref &&that) {
        this->hdr_ = that.hdr_;
        that.hdr_ = future_header_nil::nil();
      }
      template<typename Ops1>
      future_impl_shref(
          future_impl_shref<Ops1,T...> &&that,
          typename std::enable_if<std::is_base_of<HeaderOps,Ops1>::value>::type* = 0
        ) {
        this->hdr_ = that.hdr_;
        that.hdr_ = future_header_nil::nil();
      }
      
      future_impl_shref& operator=(future_impl_shref &&that) {
        future_header *tmp = this->hdr_;
        this->hdr_ = that.hdr_;
        that.hdr_ = tmp;
        return *this;
      }
      
      // build from some other future implementation
      template<typename Impl>
      future_impl_shref(Impl &&that) {
        this->hdr_ = that.steal_header();
      }
      
      ~future_impl_shref() {
        HeaderOps::template dropref<T...>(this->hdr_, /*maybe_nil=*/std::true_type());
      }
    
    public:
      bool ready() const {
        return HeaderOps::is_trivially_ready_result || hdr_->status_ == future_header::status_ready;
      }
      
      detail::constant_function<std::tuple<T&...>> result_lrefs_getter() const {
        return {
          detail::tuple_lrefs(
            future_header_result<T...>::results_of(hdr_->result_)
          )
        };
      }
      
      auto result_rvals()
        UPCXX_RETURN_DECLTYPE(
          detail::tuple_rvals(
            future_header_result<T...>::results_of(hdr_->result_)
          )
        ) {
        return detail::tuple_rvals(
          future_header_result<T...>::results_of(hdr_->result_)
        );
      }
      
      typedef HeaderOps header_ops;
      
      future_header* steal_header() {
        future_header *hdr = this->hdr_;
        this->hdr_ = future_header_nil::nil();
        return hdr;
      }
    };
    
    
    //////////////////////////////////////////////////////////////////////
    // future_dependency: future_impl_shref specialization
    
    template<typename HeaderOps,
             bool is_trivially_ready_result = HeaderOps::is_trivially_ready_result>
    struct future_dependency_shref_base;
    
    template<typename HeaderOps>
    struct future_dependency_shref_base<HeaderOps, /*is_trivially_ready_result=*/false> {
      future_header::dependency_link link_;
      
      future_dependency_shref_base(
          future_header_dependent *suc_hdr,
          future_header *arg_hdr
        ) {
        
        if(HeaderOps::is_possibly_dependent && (
           arg_hdr->status_ == future_header::status_proxying ||
           arg_hdr->status_ == future_header::status_proxying_active
          )) {
          arg_hdr = future_header::drop_for_proxied(arg_hdr);
        }
        
        if(arg_hdr->status_ != future_header::status_ready) {
          this->link_.suc = suc_hdr;
          this->link_.dep = arg_hdr;
          this->link_.sucs_next = arg_hdr->sucs_head_;
          arg_hdr->sucs_head_ = &this->link_;
          
          suc_hdr->status_ += 1;
        }
        else
          this->link_.dep = future_header::drop_for_result(arg_hdr);
      }
      
      future_dependency_shref_base(future_dependency_shref_base const&) = delete;
      future_dependency_shref_base(future_dependency_shref_base&&) = delete;
      
      void unlink_() { this->link_.unlink(); }
      
      future_header* header_() const { return link_.dep; }
    };
    
    template<typename HeaderOps>
    struct future_dependency_shref_base<HeaderOps, /*is_trivially_ready_result=*/true> {
      future_header *hdr_;
      
      future_dependency_shref_base(
          future_header_dependent *suc_hdr,
          future_header *arg_hdr
        ) {
        hdr_ = arg_hdr;
      }
      
      inline void unlink_() {}
      
      future_header* header_() const { return hdr_; }
    };
    
    template<typename HeaderOps, typename ...T>
    struct future_dependency<
        future1<future_kind_shref<HeaderOps>, T...>
      >:
      future_dependency_shref_base<HeaderOps> {
      
      future_dependency(
          future_header_dependent *suc_hdr,
          future1<future_kind_shref<HeaderOps>, T...> arg
        ):
        future_dependency_shref_base<HeaderOps>{
          suc_hdr,
          arg.impl_.steal_header()
        } {
      }
      
      void cleanup_early() {
        this->unlink_();
        auto *hdr = this->header_();
        HeaderOps::template dropref<T...>(hdr, /*maybe_nil=*/std::true_type());
      }
      
      void cleanup_ready() {
        future_header_ops_result_ready::template dropref<T...>(this->header_(), /*maybe_nil*/std::false_type());
      }
      
      detail::constant_function<std::tuple<T&...>> result_lrefs_getter() const {
        return {
          detail::tuple_lrefs(
            future_header_result<T...>::results_of(this->header_())
          )
        };
      }
      
      future_header* cleanup_ready_get_header() {
        return this->header_();
      }
    };
  }
}
#endif
