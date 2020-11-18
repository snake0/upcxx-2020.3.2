#ifndef _740290a8_56e6_4fa4_b251_ff87c02bede0
#define _740290a8_56e6_4fa4_b251_ff87c02bede0

#include <upcxx/diagnostic.hpp>

#include <cstdint>

namespace upcxx {
namespace backend {
namespace gasnet {
  struct handle_cb;
  struct handle_cb_queue;

  struct handle_cb_successor {
    handle_cb_queue *q_;
    handle_cb **pp_;
    void operator()(handle_cb *succ);
  };
  
  struct handle_cb {
    handle_cb *next_ = reinterpret_cast<handle_cb*>(0x1);
    std::uintptr_t handle = 0;
    
    virtual void execute_and_delete(handle_cb_successor) = 0;
  };

  template<typename Fn>
  struct handle_cb_impl_fn final: handle_cb {
    Fn fn;
    handle_cb_impl_fn(Fn &&fn): fn(std::move(fn)) {}
    virtual void execute_and_delete(handle_cb_successor) {
      fn();
      delete this;
    }
  };

  template<typename Fn1, typename Fn = typename std::decay<Fn1>::type>
  handle_cb_impl_fn<Fn>* make_handle_cb(Fn1 &&fn) {
    return new handle_cb_impl_fn<Fn>(std::forward<Fn1>(fn));
  }
  
  
  // This type is contained within `__thread` storage, so it must be:
  //   1. trivially destructible.
  //   2. constexpr constructible equivalent to zero-initialization.
  struct handle_cb_queue {
    friend struct handle_cb_successor;
    
    handle_cb *head_;
    //handle_cb **tailp_;
    std::uintptr_t tailp_xor_head_;

    // Tracks number of consecutive burst()'s that were fruitless and aborted
    // due to too many handle test failures ("misses").
    int aborted_burst_n_;
    
  private:
    handle_cb** get_tailp() const {
      return reinterpret_cast<handle_cb**>(
        this->tailp_xor_head_ ^ reinterpret_cast<std::uintptr_t>(&this->head_)
      );
    }
    void set_tailp(handle_cb **tailp) {
      this->tailp_xor_head_ = reinterpret_cast<std::uintptr_t>(&this->head_)
                            ^ reinterpret_cast<std::uintptr_t>(tailp);
    }
    
  public:
    constexpr handle_cb_queue():
      head_(),
      tailp_xor_head_(),
      aborted_burst_n_() {
    }
    handle_cb_queue(handle_cb_queue const&) = delete;
    
    bool empty() const;
    
    void enqueue(handle_cb *cb);
    
    template<typename Cb>
    void execute_outside(Cb *cb);
    
    int burst(bool spinning); // defined in runtime.cpp
  };
  
  //////////////////////////////////////////////////////////////////////////////

  inline bool handle_cb_queue::empty() const {
    return this->head_ == nullptr;
  }
  
  inline void handle_cb_queue::enqueue(handle_cb *cb) {
    UPCXX_ASSERT(cb->next_ == reinterpret_cast<handle_cb*>(0x1));
    cb->next_ = nullptr;
    *this->get_tailp() = cb;
    this->set_tailp(&cb->next_);
  }
  
  template<typename Cb>
  void handle_cb_queue::execute_outside(Cb *cb) {
    cb->execute_and_delete(handle_cb_successor{this, this->get_tailp()});
  }
  
  inline void handle_cb_successor::operator()(handle_cb *succ) {
    if(succ->next_ == reinterpret_cast<handle_cb*>(0x1)) {
      if(*pp_ == nullptr)
        q_->set_tailp(&succ->next_);
      succ->next_ = *pp_;
      *pp_ = succ;
    }
  }
}}}
#endif
