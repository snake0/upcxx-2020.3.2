#ifndef _4fd2caba_e406_4f0e_ab34_0e0224ec36a5
#define _4fd2caba_e406_4f0e_ab34_0e0224ec36a5

#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>

#include <upcxx/backend/gasnet/runtime.hpp>

#include <gasnet_fwd.h>

#include <climits>
#include <cstdint>
#include <vector>
#include <string>
#include <type_traits>

namespace upcxx {
  // All supported atomic operations.
  enum class atomic_op : gex_OP_t { 

       // accessors
       load             = GEX_OP_GET, 
       store            = GEX_OP_SET, 
       compare_exchange = GEX_OP_FCAS,

       // arithmetic
       add              = GEX_OP_ADD,   fetch_add        = GEX_OP_FADD,
       sub              = GEX_OP_SUB,   fetch_sub        = GEX_OP_FSUB,
       inc              = GEX_OP_INC,   fetch_inc        = GEX_OP_FINC,
       dec              = GEX_OP_DEC,   fetch_dec        = GEX_OP_FDEC,
       mul              = GEX_OP_MULT,  fetch_mul        = GEX_OP_FMULT,
       min              = GEX_OP_MIN,   fetch_min        = GEX_OP_FMIN,
       max              = GEX_OP_MAX,   fetch_max        = GEX_OP_FMAX,

       // bitwise operations
       bit_and          = GEX_OP_AND,   fetch_bit_and    = GEX_OP_FAND,
       bit_or           = GEX_OP_OR,    fetch_bit_or     = GEX_OP_FOR,
       bit_xor          = GEX_OP_XOR,   fetch_bit_xor    = GEX_OP_FXOR,
  };
  
  namespace detail {

    extern const char *atomic_op_str(upcxx::atomic_op op);
    extern std::string opset_to_string(gex_OP_t opset);

    inline int memory_order_flags(std::memory_order order) {
      switch (order) {
        case std::memory_order_acquire: return GEX_FLAG_AD_ACQ;
        case std::memory_order_release: return GEX_FLAG_AD_REL;
        case std::memory_order_acq_rel: return (GEX_FLAG_AD_ACQ | GEX_FLAG_AD_REL);
        case std::memory_order_relaxed: return 0;
        case std::memory_order_seq_cst:
          UPCXX_ASSERT(0, "Unsupported memory order: std::memory_order_seq_cst");
          break;
        case std::memory_order_consume:
          UPCXX_ASSERT(0, "Unsupported memory order: std::memory_order_consume");
          break;
        default:
          UPCXX_ASSERT(0, "Unrecognized memory order: " << (int)order);      
      }
      return 0; // unreachable
    }

    template<std::size_t size, int bit_flavor /*0=unsigned, 1=signed, 2=floating*/>
    struct atomic_proxy_help { using type = char; static constexpr gex_DT_t dt = 0; /* for error checking */ };
    template<>
    struct atomic_proxy_help<4,0> { using type = std::uint32_t; static constexpr gex_DT_t dt = GEX_DT_U32; };
    template<>
    struct atomic_proxy_help<4,1> { using type = std::int32_t;  static constexpr gex_DT_t dt = GEX_DT_I32; };
    template<>
    struct atomic_proxy_help<4,2> { using type = float;         static constexpr gex_DT_t dt = GEX_DT_FLT; };
    template<>
    struct atomic_proxy_help<8,0> { using type = std::uint64_t; static constexpr gex_DT_t dt = GEX_DT_U64; };
    template<>
    struct atomic_proxy_help<8,1> { using type = std::int64_t;  static constexpr gex_DT_t dt = GEX_DT_I64; };
    template<>
    struct atomic_proxy_help<8,2> { using type = double;        static constexpr gex_DT_t dt = GEX_DT_DBL; };

    template<typename T>
    inline constexpr int bit_flavor() { // 0=unsigned, 1=signed, 2=floating
      return std::is_floating_point<T>::value ? 2 : (std::is_unsigned<T>::value ? 0 : 1);
    }

    // 6 combinations explicitly instantiated in atomic.cpp TU:
    // {size=4,8} X {bit_flavor=0,1,2}
    template<std::size_t size, int bit_flavor>
    struct atomic_domain_untyped {
      using proxy_type = typename atomic_proxy_help<size, bit_flavor>::type;
      static constexpr gex_DT_t dt = atomic_proxy_help<size, bit_flavor>::dt;
      // call to backend gasnet function
      static gex_Event_t inject ( std::uintptr_t ad, 
        void *result_ptr, intrank_t jobrank, void *raw_ptr, 
        atomic_op opcode, proxy_type val1, proxy_type val2,
        gex_Flags_t flags
      );

      // Our encoding:
      // atomic_gex_ops == ad_gex_handle == 0: 
      //   an invalid (destroyed) object. 
      // atomic_gex_ops == 0, ad_gex_handle != 0 : 
      //   a constructed but empty domain which was not registered with gasnet 
      //  (hence ad_gex_handle was not produced by gasnet). 
      // atomic_gex_ops != 0, ad_gex_handle != 0 : 
      //   a live domain constructed by gasnet.

      // The or'd values for the atomic operations.
      gex_OP_t atomic_gex_ops = 0;
      // The opaque gasnet atomic domain handle.
      std::uintptr_t ad_gex_handle = 0;

      const team *parent_tm_;
      
      // default constructor doesn't do anything besides initializing both:
      //   atomic_gex_ops = 0, ad_gex_handle = 0
      atomic_domain_untyped() {}

      // The constructor takes a vector of operations. Currently, flags is currently unsupported.
      atomic_domain_untyped(std::vector<atomic_op> const &ops, const team &tm);
      
      ~atomic_domain_untyped();

      void destroy(entry_barrier eb);
    };

  } // namespace detail 
  
  // Atomic domain for any supported type.
  template<typename T>
  class atomic_domain : 
    private detail::atomic_domain_untyped<sizeof(T), detail::bit_flavor<T>()> {
 
    private:
      using proxy_type = typename detail::atomic_proxy_help<sizeof(T), detail::bit_flavor<T>()>::type;

      // for checking type is 32 or 64-bit non-const integral/floating type
      static constexpr bool is_atomic =
        (std::is_integral<T>::value || std::is_floating_point<T>::value) && 
	!std::is_const<T>::value &&
        (sizeof(T) == 4 || sizeof(T) == 8);
      
      static_assert(is_atomic,
          "Atomic domains only supported on non-const 32 and 64-bit integral or floating-point types");

      // event values for non-fetching operations
      struct nofetch_aop_event_values {
        template<typename Event>
        using tuple_t = std::tuple<>;
      };
      // event values for fetching operations
      struct fetch_aop_event_values {
        template<typename Event>
        using tuple_t = typename std::conditional<
            std::is_same<Event, operation_cx_event>::value, std::tuple<T>, std::tuple<> >::type;
      };

      // The class that handles the gasnet event. This is for non-fetching ops.
      // Must be declared final for the 'delete this' call.
      template<typename CxStateHere>
      struct nofetch_op_cb final: backend::gasnet::handle_cb {
        CxStateHere state_here;

        nofetch_op_cb(CxStateHere state_here) : state_here{std::move(state_here)} {}

        // The callback executed upon event completion.
        void execute_and_delete(backend::gasnet::handle_cb_successor) override {
          this->state_here.template operator()<operation_cx_event>();
          delete this;
        }
      };

      // The class that handles the gasnet event. For fetching ops.
      template<typename CxStateHere>
      struct fetch_op_cb final: backend::gasnet::handle_cb {
        CxStateHere state_here;
        T result;

        fetch_op_cb(CxStateHere state_here) : state_here{std::move(state_here)} {}

        void execute_and_delete(backend::gasnet::handle_cb_successor) override {
          this->state_here.template operator()<operation_cx_event>(std::move(result));
          delete this;
        }
      };

      // convenience declarations
      template<typename Cxs>
      using FETCH_RTYPE = typename detail::completions_returner<detail::event_is_here,
          fetch_aop_event_values, Cxs>::return_t;
      template<typename Cxs>
      using NOFETCH_RTYPE = typename detail::completions_returner<detail::event_is_here,
          nofetch_aop_event_values, Cxs>::return_t;
      using FUTURE_CX = completions<future_cx<operation_cx_event> >;

      // generic fetching atomic operation
      template<typename Cxs = FUTURE_CX>
      FETCH_RTYPE<Cxs> fop(atomic_op aop, global_ptr<T> gptr, std::memory_order order,
                           T val1 = 0, T val2 = 0, Cxs cxs = Cxs{{}}) const {
        UPCXX_ASSERT(this->atomic_gex_ops || this->ad_gex_handle, "Atomic domain is not constructed");
        UPCXX_ASSERT((detail::completions_has_event<Cxs, operation_cx_event>::value));
        UPCXX_GPTR_CHK(gptr);
        UPCXX_ASSERT(gptr != nullptr, "Global pointer for atomic operation is null");
        UPCXX_ASSERT(this->parent_tm_->from_world(gptr.rank_,-1) >= 0, 
                     "Global pointer must reference a member of the team used to construct atomic_domain");
        UPCXX_ASSERT(static_cast<gex_OP_t>(aop) & this->atomic_gex_ops,
              "Atomic operation '" << detail::atomic_op_str(aop) << "'"
              " not in domain's operation set '" << 
              detail::opset_to_string(this->atomic_gex_ops) << "'\n");

        
        // we only have local completion, not remote
        using cxs_here_t = detail::completions_state<detail::event_is_here,
            fetch_aop_event_values, Cxs>;
        
        // Create the callback object
        auto *cb = new fetch_op_cb<cxs_here_t>{cxs_here_t{std::move(cxs)}};
        
        auto returner = detail::completions_returner<detail::event_is_here,
            fetch_aop_event_values, Cxs>{cb->state_here};
        
        // execute the backend gasnet function
        gex_Event_t h = this->inject( this->ad_gex_handle,
          &cb->result, gptr.rank_, gptr.raw_ptr_, 
          aop, static_cast<proxy_type>(val1), static_cast<proxy_type>(val2), 
          detail::memory_order_flags(order) | GEX_FLAG_RANK_IS_JOBRANK
        );
        
        if (h != GEX_EVENT_INVALID) { // asynchronous AMO in-flight
          cb->handle = reinterpret_cast<uintptr_t>(h);
          backend::gasnet::register_cb(cb);
          backend::gasnet::after_gasnet();
        } else { // gasnet completed AMO synchronously
          UPCXX_ASSERT(cb->handle == 0);
          backend::gasnet::get_handle_cb_queue().execute_outside(cb);
        }
        
        return returner();
      }

      // generic non-fetching atomic operation
      template<typename Cxs = FUTURE_CX>
      NOFETCH_RTYPE<Cxs> op(atomic_op aop, global_ptr<T> gptr, std::memory_order order,
                            T val1 = 0, T val2 = 0, Cxs cxs = Cxs{{}}) const {
        UPCXX_ASSERT(this->atomic_gex_ops || this->ad_gex_handle, "Atomic domain is not constructed");
        UPCXX_ASSERT((detail::completions_has_event<Cxs, operation_cx_event>::value));
        UPCXX_GPTR_CHK(gptr);
        UPCXX_ASSERT(gptr != nullptr, "Global pointer for atomic operation is null");
        UPCXX_ASSERT(this->parent_tm_->from_world(gptr.rank_,-1) >= 0, 
                     "Global pointer must reference a member of the team used to construct atomic_domain");
        UPCXX_ASSERT(static_cast<gex_OP_t>(aop) & this->atomic_gex_ops,
              "Atomic operation '" << detail::atomic_op_str(aop) << "'"
              " not in domain's operation set '" << 
              detail::opset_to_string(this->atomic_gex_ops) << "'\n");
        
        // we only have local completion, not remote
        using cxs_here_t = detail::completions_state<detail::event_is_here,
            nofetch_aop_event_values, Cxs>;
        
        // Create the callback object on stack..
        nofetch_op_cb<cxs_here_t> cb(cxs_here_t(std::move(cxs)));
        
        auto returner = detail::completions_returner<detail::event_is_here,
            nofetch_aop_event_values, Cxs>{cb.state_here};
        
        // execute the backend gasnet function
        gex_Event_t h = this->inject( this->ad_gex_handle,
          nullptr, gptr.rank_, gptr.raw_ptr_, 
          aop, static_cast<proxy_type>(val1), static_cast<proxy_type>(val2), 
          detail::memory_order_flags(order) | GEX_FLAG_RANK_IS_JOBRANK
        );

        if (h != GEX_EVENT_INVALID) { // asynchronous AMO in-flight
          cb.handle = reinterpret_cast<uintptr_t>(h);
          // move callback to heap since it lives asynchronously
          backend::gasnet::register_cb(new decltype(cb)(std::move(cb)));
          backend::gasnet::after_gasnet();
        } else { // gasnet completed AMO synchronously
          UPCXX_ASSERT(cb.handle == 0);
          // do callback's execute_and_delete, minus the delete
          cb.state_here.template operator()<operation_cx_event>();
        }
        
        return returner();
      }

    public:
      // default constructor 
      // issue #316: this is NOT guaranteed by spec
      #if 0
      atomic_domain() {}
      #endif

      atomic_domain(atomic_domain &&that) {
        this->ad_gex_handle = that.ad_gex_handle;
        this->atomic_gex_ops = that.atomic_gex_ops;
        this->parent_tm_ = that.parent_tm_;
        // revert `that` to non-constructed state
        that.atomic_gex_ops = 0;
        that.ad_gex_handle = 0;
        that.parent_tm_ = nullptr;
      }

      #if 0 // disabling move-assignment, for now
      atomic_domain &operator=(atomic_domain &&that) {
        // only allow assignment moves onto "dead" object
        UPCXX_ASSERT(atomic_gex_ops == 0,
                     "Move assignment is only allowed on a default-constructed atomic_domain");
        this->ad_gex_handle = that.ad_gex_handle;
        this->atomic_gex_ops = that.atomic_gex_ops;
        this->parent_tm_ = that.parent_tm_;
        // revert `that` to non-constructed state
        that.atomic_gex_ops = 0;
        that.ad_gex_handle = 0;
        that.parent_tm_ = nullptr;
        return *this;
      }
      #endif
      
      // The constructor takes a vector of operations. Currently, flags is currently unsupported.
      atomic_domain(std::vector<atomic_op> const &ops, const team &tm = upcxx::world()) :
        detail::atomic_domain_untyped<sizeof(T), detail::bit_flavor<T>()>(ops, tm) {}
      
      void destroy(entry_barrier eb = entry_barrier::user) {
        detail::atomic_domain_untyped<sizeof(T), detail::bit_flavor<T>()>::destroy(eb);
      }

      ~atomic_domain() {}
      
      template<typename Cxs = FUTURE_CX>
      NOFETCH_RTYPE<Cxs> store(global_ptr<T> gptr, T val, std::memory_order order,
                               Cxs cxs = Cxs{{}}) const {
        return op(atomic_op::store, gptr, order, val, (T)0, cxs);
      }
      template<typename Cxs = FUTURE_CX>
      FETCH_RTYPE<Cxs> load(global_ptr<T> gptr, std::memory_order order, Cxs cxs = Cxs{{}}) const {
        return fop(atomic_op::load, gptr, order, (T)0, (T)0, cxs);
      }
      template<typename Cxs = FUTURE_CX>
      NOFETCH_RTYPE<Cxs> inc(global_ptr<T> gptr, std::memory_order order, Cxs cxs = Cxs{{}}) const {
        return op(atomic_op::inc, gptr, order, (T)0, (T)0, cxs);
      }
      template<typename Cxs = FUTURE_CX>
      NOFETCH_RTYPE<Cxs> dec(global_ptr<T> gptr, std::memory_order order, Cxs cxs = Cxs{{}}) const {
        return op(atomic_op::dec,gptr, order, (T)0, (T)0, cxs);
      }
      template<typename Cxs = FUTURE_CX>
      FETCH_RTYPE<Cxs> fetch_inc(global_ptr<T> gptr, std::memory_order order, Cxs cxs = Cxs{{}}) const {
        return fop(atomic_op::fetch_inc, gptr, order, (T)0, (T)0, cxs);
      }
      template<typename Cxs = FUTURE_CX>
      FETCH_RTYPE<Cxs> fetch_dec(global_ptr<T> gptr, std::memory_order order, Cxs cxs = Cxs{{}}) const {
        return fop(atomic_op::fetch_dec, gptr, order, (T)0, (T)0, cxs);
      }
      template<typename Cxs = FUTURE_CX>
      FETCH_RTYPE<Cxs> compare_exchange(global_ptr<T> gptr, T val1, T val2, std::memory_order order,
                                        Cxs cxs = Cxs{{}}) const {
        return fop(atomic_op::compare_exchange, gptr, order, val1, val2, cxs);
      }
      
      #define UPCXX_AD_METHODS(name, constraint)\
        template<typename Cxs = FUTURE_CX>\
        constraint(FETCH_RTYPE<Cxs>) \
	fetch_##name(global_ptr<T> gptr, T val, std::memory_order order,\
                                      Cxs cxs = Cxs{{}}) const {\
          return fop(atomic_op::fetch_##name, gptr, order, val, (T)0, cxs);\
        }\
        template<typename Cxs = FUTURE_CX>\
        constraint(NOFETCH_RTYPE<Cxs>) \
	name(global_ptr<T> gptr, T val, std::memory_order order,\
                                Cxs cxs = Cxs{{}}) const {\
          return op(atomic_op::name, gptr, order, val, (T)0, cxs);\
        }
      // sfinae helpers to disable unsupported type/op combos
      #define UPCXX_AD_INTONLY(R) typename std::enable_if<std::is_integral<T>::value,R>::type
      #define UPCXX_AD_ANYTYPE(R) R
      UPCXX_AD_METHODS(add,    UPCXX_AD_ANYTYPE)
      UPCXX_AD_METHODS(sub,    UPCXX_AD_ANYTYPE)
      UPCXX_AD_METHODS(mul,    UPCXX_AD_ANYTYPE)
      UPCXX_AD_METHODS(min,    UPCXX_AD_ANYTYPE)
      UPCXX_AD_METHODS(max,    UPCXX_AD_ANYTYPE)
      UPCXX_AD_METHODS(bit_and,UPCXX_AD_INTONLY)
      UPCXX_AD_METHODS(bit_or, UPCXX_AD_INTONLY)
      UPCXX_AD_METHODS(bit_xor,UPCXX_AD_INTONLY)
      #undef UPCXX_AD_METHODS
      #undef UPCXX_AD_INTONLY
      #undef UPCXX_AD_ANYTYPE
  };
} // namespace upcxx

#endif
