#ifndef _850ece2c_7b55_43a8_9e57_8cbd44974055
#define _850ece2c_7b55_43a8_9e57_8cbd44974055

#include <upcxx/backend_fwd.hpp>
#include <upcxx/cuda_fwd.hpp>
#include <upcxx/future.hpp>
#include <upcxx/intru_queue.hpp>
#include <upcxx/lpc.hpp>

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace upcxx {
  class persona;
  class persona_scope;
  
  namespace detail {
    struct persona_scope_raw;
    struct persona_scope_redundant;
    struct persona_tls;
  }

  // This type is contained within `__thread` storage, so it must be:
  //   1. trivially destructible.
  //   2. constexpr constructible equivalent to zero-initialization.
  class persona {
    friend struct detail::persona_tls;
    friend class persona_scope;
    friend struct detail::persona_scope_redundant;
    
  private:
    // persona *owner = this;
    std::atomic<std::uintptr_t> owner_xor_this_;
  
    detail::lpc_inbox<detail::intru_queue_safety::mpsc> peer_inbox_[2];
    detail::lpc_inbox<detail::intru_queue_safety::none> self_inbox_[2];
    
  private:
    detail::intru_queue<
        detail::lpc_base,
        detail::intru_queue_safety::none,
        &detail::lpc_base::intruder
      >
      pros_deferred_trivial_;
    
  public:
    backend::persona_state backend_state_;
    cuda::persona_state cuda_state_;
    std::intptr_t undischarged_n_; // num reasons progress_required() is true
  
  private:
    persona* get_owner() const;
    void set_owner(persona *val);
    
  private:
    // Constructs the default persona for the current thread.
    constexpr persona(detail::internal_only):
      owner_xor_this_(), // owner = this, default persona's are their own owner
      peer_inbox_(),
      self_inbox_(),
      pros_deferred_trivial_(),
      backend_state_(),
      undischarged_n_(0) {
    }
  
  public:
    // Constructs a non-default persona.
    persona():
      owner_xor_this_(reinterpret_cast<std::uintptr_t>(this)), // owner = null
      peer_inbox_(),
      self_inbox_(),
      pros_deferred_trivial_(),
      backend_state_(),
      undischarged_n_(0) {
    }
    
    bool active_with_caller() const;
    bool active_with_caller(detail::persona_tls &tls) const;
    bool active() const;
    
  public:
    template<typename Fn>
    void lpc_ff(detail::persona_tls &tls, Fn fn);
    
    template<typename Fn>
    void lpc_ff(Fn fn);
  
  private:
    template<typename Results, typename Promise>
    struct lpc_initiator_finish {
      Results results_;
      Promise *pro_;
      
      void operator()() {
        pro_->fulfill_result(std::move(results_));
        delete pro_;
      }
    };
    
    template<typename Promise>
    struct lpc_recipient_executed {
      persona *initiator_;
      Promise *pro_;
      
      template<typename ...Args>
      void operator()(Args &&...args) {
        std::tuple<typename std::decay<Args>::type...> results{
          std::forward<Args>(args)...
        };
        
        initiator_->lpc_ff(
          lpc_initiator_finish<decltype(results), Promise>{
            std::move(results),
            pro_
          }
        );
      }
    };
    
    template<typename Fn, typename Promise>
    struct lpc_recipient_execute {
      persona *initiator_;
      Promise *pro_;
      Fn fn_;
      
      void operator()() {
        upcxx::apply_as_future(fn_)
          .then(lpc_recipient_executed<Promise>{initiator_, pro_});
      }
    };
  
  public:
    template<typename Fn>
    auto lpc(Fn fn)
      -> typename detail::future_from_tuple_t<
        detail::future_kind_shref<detail::future_header_ops_general>, // the default future kind
        typename decltype(upcxx::apply_as_future(fn))::results_type
      >;
  };
  
  //////////////////////////////////////////////////////////////////////////////
  // persona_scope
  
  namespace detail {
    // Holds all the fields for a persona_scope but in a trivial type.
    struct persona_scope_raw {
      friend struct detail::persona_tls;
      
      persona_scope_raw *next_;
      std::uintptr_t persona_xor_default_;
      persona_scope_raw *next_unique_;
      detail::persona_tls *tls; // used by `persona_scope_redundant`
      union {
        void *lock_; // used by `persona_scope`
        persona *restore_top_persona_; // used by `persona_scope_redundant`
      };
      void(*unlocker_)(void*);
      
      // Constructs the default persona scope for the current thread.
      constexpr persona_scope_raw():
        next_(),
        persona_xor_default_(),
        next_unique_(),
        tls(),
        lock_(),
        unlocker_() {
      }
      
      //////////////////////////////////////////////////////////////////////////
      // accessors
      
      persona* get_persona(detail::persona_tls &tls) const;
      void set_persona(persona *val, detail::persona_tls &tls);
    };
    
    // A more efficient kind of persona scope used when its known that we're
    // redundantly pushing a persona to the current thread's stack when its
    // already a member of the stack.
    struct persona_scope_redundant: persona_scope_raw {
      persona_scope_redundant(persona &persona, detail::persona_tls &tls);
      ~persona_scope_redundant();
    };
  }
  
  class persona_scope: public detail::persona_scope_raw {
    friend struct detail::persona_tls;
    
  private:
    // the_default_dummy_'s constructor
    persona_scope() {
      // immediately invalidate this scope
      this->next_ = reinterpret_cast<persona_scope_raw*>(0x1);
    }
    
    persona_scope(persona &persona, detail::persona_tls &tls);
    
    template<typename Mutex>
    persona_scope(Mutex &lock, persona &persona, detail::persona_tls &tls);
    
  public:
    static persona_scope the_default_dummy_;
    
    persona_scope(persona &persona);
    
    template<typename Mutex>
    persona_scope(Mutex &lock, persona &persona);
    
    persona_scope(persona_scope const&) = delete;
    
    persona_scope(persona_scope &&that) {
      *static_cast<detail::persona_scope_raw*>(this) = static_cast<detail::persona_scope_raw&>(that);
      that.next_ = reinterpret_cast<persona_scope_raw*>(0x1);
    }
    
    ~persona_scope();
  };
  
  //////////////////////////////////////////////////////////////////////
  // detail::persona_tls
  
  namespace detail {
    // This type is contained within `__thread` storage, so it must be:
    //   1. trivially destructible.
    //   2. constexpr constructible equivalent to zero-initialization.
    struct persona_tls {
      int progressing_add_1;
      unsigned burstable_bits;
      
      persona default_persona;
      // persona_scope default_scope;
      persona_scope_raw default_scope_raw;
      // persona_scope_raw *top = &this->default_scope_raw;
      std::uintptr_t top_xor_default; // = xor(top, &this->default_scope_raw)
      // persona_scope_raw *top_unique = &this->default_scope_raw;
      std::uintptr_t top_unique_xor_default;  // = xor(top_unique, &this->default_scope_raw)
      // persona *top_persona = &this->default_persona;
      std::uintptr_t top_persona_xor_default;  // = xor(top_persona, &this->default_persona)
      
      static_assert(std::is_trivially_destructible<persona>::value, "upcxx::persona must be TriviallyDestructible.");
      static_assert(std::is_trivially_destructible<persona_scope_raw>::value, "upcxx::detail::persona_scope_raw must be TriviallyDestructible.");
      
      constexpr persona_tls():
        progressing_add_1(),
        burstable_bits(),
        default_persona(internal_only()), // call special constructor that builds default persona
        default_scope_raw(),
        top_xor_default(),
        top_unique_xor_default(),
        top_persona_xor_default() {
      }
      
      //////////////////////////////////////////////////////////////////////////
      // getters/setters for fields with zero-friendly encodings
      
      int get_progressing() const {
        return progressing_add_1 - 1;
      }
      void set_progressing(int val) {
        progressing_add_1 = val + 1;
      }
      
      bool is_burstable(progress_level lev) const {
        // The OR with 1 makes internal progress always burstable
        return (burstable_bits | 1) & (1<<(int)lev);
      }
      void flip_burstable(progress_level lev) {
        burstable_bits ^= 1<<(int)lev;
      }
      
      persona_scope& default_scope() {
        return *static_cast<persona_scope*>(&default_scope_raw);
      }
      persona_scope const& default_scope() const {
        return *static_cast<persona_scope const*>(&default_scope_raw);
      }
      
      persona_scope_raw* get_top_scope() const {
        return reinterpret_cast<persona_scope_raw*>(
          top_xor_default ^ reinterpret_cast<std::uintptr_t>(&default_scope())
        );
      }
      void set_top_scope(persona_scope_raw *val) {
        top_xor_default = reinterpret_cast<std::uintptr_t>(&default_scope())
                        ^ reinterpret_cast<std::uintptr_t>(val);
      }
      
      persona_scope_raw* get_top_unique_scope() const {
        return reinterpret_cast<persona_scope_raw*>(
          top_unique_xor_default ^ reinterpret_cast<std::uintptr_t>(&default_scope())
        );
      }
      void set_top_unique_scope(persona_scope_raw *val) {
        top_unique_xor_default = reinterpret_cast<std::uintptr_t>(&default_scope())
                               ^ reinterpret_cast<std::uintptr_t>(val);
      }
      
      persona* get_top_persona() const {
        return reinterpret_cast<persona*>(
          top_persona_xor_default ^ reinterpret_cast<std::uintptr_t>(&default_persona)
        );
      }
      void set_top_persona(persona *val) {
        top_persona_xor_default = reinterpret_cast<std::uintptr_t>(&default_persona)
                                ^ reinterpret_cast<std::uintptr_t>(val);
      }
      
      //////////////////////////////////////////////////////////////////////////
      // operations
      
      // Enqueue a lambda onto persona's progress level queue. Note: this
      // lambda may execute in the calling context if permitted.
      template<typename Fn, bool known_active>
      void during(persona&, progress_level level, Fn &&fn, std::integral_constant<bool,known_active> known_active1 = {});

      // Enqueue a promise to be fulfilled during user progress of a currently
      // active persona.
      template<typename ...T>
      void fulfill_during_user_of_active(persona&, future_header_promise<T...> *pro_hdr/*takes ref*/, std::intptr_t deps);
      
      // Enqueue a lambda onto persona's progress level queue. Unlike
      // `during`, lambda will definitely not execute in calling context.
      template<typename Fn, bool known_active=false>
      void defer(persona&, progress_level level, Fn &&fn, std::integral_constant<bool,known_active> known_active1 = {});
      
      template<bool known_active=false>
      void enqueue(persona&, progress_level level, detail::lpc_base *m, std::integral_constant<bool,known_active> known_active1 = {});

      // Enqueue a quiesced promise (one on which no other dependency
      // requirement/fulfillment will occur) to be fulfilled during progress of
      // given persona
      template<typename ...T, bool known_active=false>
      void enqueue_quiesced_promise(
        persona &target, progress_level level,
        future_header_promise<T...> *pro,
        std::intptr_t result_plus_anon,
        std::integral_constant<bool,known_active> known_active1 = {}
      );
      
      bool progress_required();
      bool progress_required(persona_scope &bottom);
      
      // Call `fn` on each `persona&` active with calling thread.
      template<typename Fn>
      void foreach_active_as_top(Fn &&fn);
      
      // Returns number of lpc's fired. Persona *should* be top-most active
      // on this thread, but don't think anything would break if it isn't.
      int burst_internal(persona&);
      int burst_user(persona&);
      
      int persona_only_progress();
    };
    
    extern __thread persona_tls the_persona_tls;
    
    inline void* thread_id() {
      return (void*)&the_persona_tls;
    }
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // persona definitions
  
  inline persona* persona::get_owner() const {
    return reinterpret_cast<persona*>(
      owner_xor_this_.load(std::memory_order_relaxed) ^
      reinterpret_cast<std::uintptr_t>(this)
    );
  }
  
  inline void persona::set_owner(persona *val) {
    owner_xor_this_.store(
      reinterpret_cast<std::uintptr_t>(val) ^ reinterpret_cast<std::uintptr_t>(this),
      std::memory_order_relaxed
    );
  }
  
  inline bool persona::active_with_caller() const {
    return active_with_caller(detail::the_persona_tls);
  }
  
  inline bool persona::active_with_caller(detail::persona_tls &tls) const {
    return this->get_owner() == &tls.default_persona;
  }
  
  inline bool persona::active() const {
    return this->get_owner() != nullptr;
  }

  template<typename Fn>
  void persona::lpc_ff(Fn fn) {
    this->lpc_ff(detail::the_persona_tls, std::forward<Fn>(fn));
  }
  
  template<typename Fn>
  void persona::lpc_ff(detail::persona_tls &tls, Fn fn) {
    if(this->active_with_caller(tls))
      this->self_inbox_[(int)progress_level::user].send(std::move(fn));
    else
      this->peer_inbox_[(int)progress_level::user].send(std::move(fn));
  }
  
  template<typename Fn>
  auto persona::lpc(Fn fn)
    -> typename detail::future_from_tuple_t<
      detail::future_kind_shref<detail::future_header_ops_general>, // the default future kind
      typename decltype(upcxx::apply_as_future(fn))::results_type
    > {
    
    using results_type = typename decltype(upcxx::apply_as_future(fn))::results_type;
    using results_promise = detail::tuple_types_into_t<results_type, promise>;
    
    detail::persona_tls &tls = detail::the_persona_tls;
    
    results_promise *pro = new results_promise;
    auto ans = pro->get_future();
    
    this->lpc_ff(tls,
      lpc_recipient_execute<Fn, results_promise>{
        /*initiator*/tls.get_top_persona(),
        /*promise*/pro,
        /*fn*/std::move(fn)
      }
    );
      
    return ans;
  }
  
  //////////////////////////////////////////////////////////////////////
  // persona_scope definitions
    
  inline persona* detail::persona_scope_raw::get_persona(detail::persona_tls &tls) const {
    return reinterpret_cast<persona*>(
      persona_xor_default_ ^
      reinterpret_cast<std::uintptr_t>(&tls.default_persona)
    );
  }
  
  inline void detail::persona_scope_raw::set_persona(persona *val, detail::persona_tls &tls) {
    persona_xor_default_ = reinterpret_cast<std::uintptr_t>(val)
                         ^ reinterpret_cast<std::uintptr_t>(&tls.default_persona);
  }
  
  inline detail::persona_scope_redundant::persona_scope_redundant(
      persona &persona,
      detail::persona_tls &tls
    ) {
    this->tls = &tls;
    
    // point this scope at persona
    this->set_persona(&persona, tls);
    
    // push this scope on this thread's stack
    this->next_ = tls.get_top_scope();
    tls.set_top_scope(this);
    this->restore_top_persona_ = tls.get_top_persona();
    tls.set_top_persona(&persona);
  }
  
  inline detail::persona_scope_redundant::~persona_scope_redundant() {
    detail::persona_tls &tls = *this->tls;
    UPCXX_ASSERT(this == tls.get_top_scope());
    tls.set_top_scope(this->next_);
    tls.set_top_persona(this->restore_top_persona_);
  }
  
  inline persona_scope::persona_scope(persona &persona):
    persona_scope(persona, detail::the_persona_tls) {
  }
  
  inline persona_scope::persona_scope(persona &p, detail::persona_tls &tls) {
    this->lock_ = nullptr;
    this->unlocker_ = nullptr;
    
    bool was_active = p.active();
    UPCXX_ASSERT(!was_active || p.active_with_caller(tls), "Persona already active in another thread.");
    
    // point this scope at persona
    this->set_persona(&p, tls);
    
    // set persona's owner thread to this thread
    p.set_owner(&tls.default_persona);
    
    // push this scope on this thread's stack
    this->next_ = tls.get_top_scope();
    tls.set_top_scope(this);
    tls.set_top_persona(&p);
    
    if(!was_active) {
      this->next_unique_ = tls.get_top_unique_scope();
      tls.set_top_unique_scope(this);
    }
    else
      this->next_unique_ = reinterpret_cast<persona_scope_raw*>(0x1);
    
    UPCXX_ASSERT(p.active_with_caller(tls));
  }
  
  template<typename Mutex>
  persona_scope::persona_scope(Mutex &lock, persona &p):
    persona_scope(lock, p, detail::the_persona_tls) {
  }
  
  template<typename Mutex>
  persona_scope::persona_scope(Mutex &lock, persona &p, detail::persona_tls &tls) {
    this->lock_ = &lock;
    this->unlocker_ = (void(*)(void*))[](void *lock) {
      static_cast<Mutex*>(lock)->unlock();
    };
    
    lock.lock();
    
    bool was_active = p.active();
    UPCXX_ASSERT(!was_active || p.active_with_caller(tls), "Persona already active in another thread.");
    
    // point this scope at persona
    this->set_persona(&p, tls);
    
    // set persona's owner thread to this thread
    p.set_owner(&tls.default_persona);
    
    // push this scope on this thread's stack
    this->next_ = tls.get_top_scope();
    tls.set_top_scope(this);
    tls.set_top_persona(&p);
    
    if(!was_active) {
      this->next_unique_ = tls.get_top_unique_scope();
      tls.set_top_unique_scope(this);
    }
    else
      this->next_unique_ = reinterpret_cast<persona_scope_raw*>(0x1);
    
    UPCXX_ASSERT(p.active_with_caller(tls));
  }
  
  inline persona_scope::~persona_scope() {
    if(this->next_ != reinterpret_cast<persona_scope_raw*>(0x1)) {
      detail::persona_tls &tls = detail::the_persona_tls;
      
      UPCXX_ASSERT(this == tls.get_top_scope());
      
      tls.set_top_scope(this->next_);
      tls.set_top_persona(this->next_->get_persona(tls));
      
      if(this->next_unique_ != reinterpret_cast<persona_scope_raw*>(0x1)) {
        this->get_persona(tls)->set_owner(nullptr);
        tls.set_top_unique_scope(this->next_unique_);
      }
      
      if(this->unlocker_)
        this->unlocker_(this->lock_);
    }
  }
  
  //////////////////////////////////////////////////////////////////////
  
  inline persona& default_persona() {
    detail::persona_tls &tls = detail::the_persona_tls;
    return tls.default_persona;
  }
  
  inline persona& current_persona() {
    detail::persona_tls &tls = detail::the_persona_tls;
    return *tls.get_top_scope()->get_persona(tls);
  }
  
  inline persona_scope& default_persona_scope() {
    return persona_scope::the_default_dummy_;
  }
  
  inline persona_scope& top_persona_scope() {
    detail::persona_tls &tls = detail::the_persona_tls;
    detail::persona_scope_raw *top = tls.get_top_scope();
    return top == &tls.default_scope_raw
      ? persona_scope::the_default_dummy_
      : *static_cast<persona_scope*>(top);
  }
  
  //////////////////////////////////////////////////////////////////////
  
  template<typename Fn, bool known_active>
  void detail::persona_tls::during(
      persona &p,
      progress_level level,
      Fn &&fn,
      std::integral_constant<bool, known_active>
    ) {
    persona_tls &tls = *this;
    
    if(known_active || p.active_with_caller(tls)) {
      if(tls.is_burstable(level)) {
        tls.flip_burstable(level);
        {
          persona_scope_redundant tmp(p, tls);
          fn();
        }
        tls.flip_burstable(level);
      }
      else
        p.self_inbox_[(int)level].send(std::forward<Fn>(fn));
    }
    else
      p.peer_inbox_[(int)level].send(std::forward<Fn>(fn));
  }
  
  template<typename ...T>
  void detail::persona_tls::fulfill_during_user_of_active(
      persona &per,
      future_header_promise<T...> *pro_hdr, // take ref
      std::intptr_t deps
    ) {
    
    UPCXX_ASSERT(per.active_with_caller(*this));
    
    // This is where we use the fact that promises can be enqueued as lpc's.
    // Of course they can only reside (intrusively) in exactly one lpc queue,
    // but since promise manipulation isn't thread-safe, we can assume that
    // being in multiple queues concurrently introduces a data race, and is
    // therefor not possible. So if the promise is already in a queue we can
    // just assume its the one we're putting it in.
    
    constexpr int user = (int)progress_level::user;
    
    promise_meta *meta = &pro_hdr->pro_meta;
    
    if(deps == (meta->deferred_decrements += deps)) { // ensure not already in the queue
      // transfer given ref into queue
      if(future_header_promise<T...>::is_trivially_deletable)
        per.pros_deferred_trivial_.enqueue(&meta->base);
      else
        per.self_inbox_[user].enqueue(&meta->base);
    }
    else
      pro_hdr->dropref(); // given a ref we dont want
  }
  
  template<typename Fn, bool known_active>
  void detail::persona_tls::defer(
      persona &p,
      progress_level level,
      Fn &&fn,
      std::integral_constant<bool, known_active>
    ) {
    persona_tls &tls = *this;
    
    if(known_active || p.active_with_caller(tls))
      p.self_inbox_[(int)level].send(std::forward<Fn>(fn));
    else
      p.peer_inbox_[(int)level].send(std::forward<Fn>(fn));
  }

  template<bool known_active>
  void detail::persona_tls::enqueue(
      persona &p,
      progress_level level,
      lpc_base *m,
      std::integral_constant<bool, known_active>
    ) {
    persona_tls &tls = *this;
    
    if(known_active || p.active_with_caller(tls))
      p.self_inbox_[(int)level].enqueue(m);
    else
      p.peer_inbox_[(int)level].enqueue(m);
  }

  template<typename ...T, bool known_active>
  void detail::persona_tls::enqueue_quiesced_promise(
      persona &target, progress_level level,
      detail::future_header_promise<T...> *pro, // takes ref
      std::intptr_t result_plus_anon,
      std::integral_constant<bool, known_active>
    ) {
    auto *meta = &pro->pro_meta;
    UPCXX_ASSERT(meta->deferred_decrements == 0); // promise not quiesced!
    meta->deferred_decrements = result_plus_anon;
    
    if(known_active || target.active_with_caller(*this)) {
      if(level == progress_level::user &&
         future_header_promise<T...>::is_trivially_deletable)
        target.pros_deferred_trivial_.enqueue(&meta->base);
      else
        target.self_inbox_[(int)level].enqueue(&meta->base);
    }
    else
      target.peer_inbox_[(int)level].enqueue(&meta->base);
  }
  
  inline bool detail::persona_tls::progress_required() {
    persona_tls &tls = *this;
    persona_scope_raw *ps = tls.get_top_scope();
    persona *p = ps->get_persona(tls);
    return p->undischarged_n_ != 0;
  }
  
  inline bool detail::persona_tls::progress_required(persona_scope &bottom) {
    persona_tls &tls = *this;
    persona_scope_raw *ps = tls.get_top_scope();
    persona_scope_raw *bot = &bottom == &persona_scope::the_default_dummy_
      ? &tls.default_scope_raw
      : static_cast<persona_scope_raw*>(&bottom);
    
    while(true) {
      persona *p = ps->get_persona(tls);
      if(p->undischarged_n_ != 0)
        return true;
      if(ps == bot)
        return false;
      ps = ps->next_;
    }
  }
  
  template<typename Fn>
  inline void detail::persona_tls::foreach_active_as_top(Fn &&fn) {
    persona_tls &tls = *this;
    persona_scope_raw *ps = tls.get_top_unique_scope();
    do {
      persona *p = ps->get_persona(tls);
      detail::persona_scope_redundant as_top(*p, tls);
      fn(*p);
      ps = ps->next_unique_;
    } while(ps != nullptr);
  }

  inline int detail::persona_tls::burst_internal(persona &p) {
    constexpr int q_internal = (int)progress_level::internal;
    
    #if 0
      bool all_empty = true;
      
      all_empty &= p.peer_inbox_[q_internal].empty();
      all_empty &= p.self_inbox_[q_internal].empty();
      
      if(all_empty) return 0;
    #endif
    
    int exec_n = 0;
    
    exec_n += p.peer_inbox_[q_internal].burst();
    exec_n += p.self_inbox_[q_internal].burst();
    
    return exec_n;
  }
  
  inline int detail::persona_tls::burst_user(persona &p) {
    constexpr int q_user = (int)progress_level::user;
    
    #if 0
      bool all_empty = true;
      
      all_empty &= p.peer_inbox_[q_user].empty();
      all_empty &= p.self_inbox_[q_user].empty();
      all_empty &= p.pros_deferred_trivial_.empty();
      
      if(all_empty) return 0;
    #endif
    
    int exec_n = 0;
    
    exec_n += p.peer_inbox_[q_user].burst();
    exec_n += p.self_inbox_[q_user].burst();
    
    exec_n += p.pros_deferred_trivial_.burst(
      [](lpc_base *m) {
        detail::promise_vtable::fulfill_deferred_and_drop_trivial(m);
      }
    );
    
    return exec_n;
  }
  
  inline int detail::persona_tls::persona_only_progress() {
    auto &tls = *this;
    
    if(-1 != tls.get_progressing())
      return 0;
    tls.set_progressing((int)progress_level::user);
    tls.flip_burstable(progress_level::user);
    
    int exec_n = 0;
    tls.foreach_active_as_top([&](persona &p) {
      exec_n += tls.burst_internal(p);
      exec_n += tls.burst_user(p);
    });
    
    tls.flip_burstable(progress_level::user);
    tls.set_progressing(-1);
    return exec_n;
  }
}
#endif
