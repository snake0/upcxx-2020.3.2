#ifndef _223a1448_cf6d_42a9_864f_57409c60efe9
#define _223a1448_cf6d_42a9_864f_57409c60efe9

#include <upcxx/backend/gasnet/runtime_fwd.hpp>
#include <upcxx/backend/gasnet/handle_cb.hpp>
#include <upcxx/backend/gasnet/reply_cb.hpp>

#include <upcxx/backend_fwd.hpp>
#include <upcxx/bind.hpp>
#include <upcxx/command.hpp>
#include <upcxx/persona.hpp>
#include <upcxx/team_fwd.hpp>

#include <cstdint>

////////////////////////////////////////////////////////////////////////
// declarations for: upcxx/backend/gasnet/runtime.cpp

namespace upcxx {
namespace backend {
namespace gasnet {
  static constexpr std::size_t am_size_rdzv_cutover_min = 256;
  extern std::size_t am_size_rdzv_cutover;
  extern std::size_t am_long_size_max;

  struct sheap_footprint_t {
    std::size_t count, bytes;
  };

  // Addresses of these passed to gasnet::allocate/deallocate
  extern sheap_footprint_t sheap_footprint_rdzv;
  extern sheap_footprint_t sheap_footprint_misc;
  extern sheap_footprint_t sheap_footprint_user;

  #if UPCXX_BACKEND_GASNET_SEQ
    extern handle_cb_queue master_hcbs;
  #endif

  // Allocate from shared heap with accounting dumped to given footprint struct
  // (not optional). Failure mode is null return  for foot == &gasnet::sheap_footprint_user
  // and job death with diagnostic dump otherwise.
  void* allocate(std::size_t size, std::size_t align, sheap_footprint_t *foot);

  // Deallocate shared heap buffer, foot must match that given to allocate.
  void  deallocate(void *p, sheap_footprint_t *foot);
  
  void after_gasnet();

  // Register a handle callback for the current persona
  void register_cb(handle_cb *cb);
  
  // Send AM (packed command), receiver executes in handler.
  void send_am_eager_restricted(
    const team &tm,
    intrank_t recipient,
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  
  // Send fully bound callable, receiver executes in handler.
  template<typename Fn>
  void send_am_restricted(const team &tm, intrank_t recipient, Fn &&fn);
  
  // Send AM (packed command), receiver executes in `level` progress.
  void send_am_eager_master(
    progress_level level,
    const team &tm,
    intrank_t recipient,
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  void send_am_eager_persona(
    progress_level level,
    const team &tm,
    intrank_t recipient_rank,
    persona *recipient_persona, // if low-bit set then this is a persona** to be dereferenced remotely
    void *command_buf,
    std::size_t buf_size,
    std::size_t buf_align
  );
  
  // Send AM (packed command) via rendezvous, receiver executes druing `level`.
  void send_am_rdzv(
    progress_level level,
    const team &tm,
    intrank_t recipient_rank,
    persona *recipient_persona, // nullptr == master, or, if low-bit set then this is a persona** to be dereferenced remotely
    void *command_buf,
    std::size_t buf_size, std::size_t buf_align
  );

  struct bcast_payload_header;
  
  void bcast_am_master_eager(
    progress_level level,
    const team &tm,
    intrank_t rank_d_ub, // in range [0, 2*rank_n-1)
    bcast_payload_header *payload,
    size_t cmd_size,
    size_t cmd_align
  );
  void bcast_am_master_rdzv(
    progress_level level,
    const team &tm,
    intrank_t rank_d_ub, // in range [0, 2*rank_n-1)
    intrank_t wrank_owner, // world team coordinates
    bcast_payload_header *payload_owner, // owner address of payload
    bcast_payload_header *payload_sender, // sender (my) address of payload
    size_t cmd_size,
    size_t cmd_align
  );

  enum class rma_put_then_am_sync: int {
    // These numeric assignments intentionally match like-named members of
    // detail::rma_put_sync as this *may* assist the compiler in optimizing
    // enum translations, though correctness does not depend on it.
    src_cb=0,
    src_now=2,
    op_now=3
  };

  template<rma_put_then_am_sync sync_lb/*src_cb,src_now*/, typename AmFn>
  rma_put_then_am_sync rma_put_then_am_master(
    const team &tm, intrank_t rank_d,
    void *buf_d, void const *buf_s, std::size_t buf_size,
    progress_level am_level, AmFn &&am_fn,
    backend::gasnet::handle_cb *src_cb,
    backend::gasnet::reply_cb *rem_cb
  );

  // The receiver-side rpc message type. Inherits lpc base type since rpc's
  // reside in the lpc queues.
  struct rpc_as_lpc: detail::lpc_base {
    detail::lpc_vtable the_vtbl;
    void *payload; // serialized rpc command
    bool is_rdzv; // was this shipped via rdzv?
    bool rdzv_rank_s_local; // only used when shipped via rdzv
    intrank_t rdzv_rank_s; // only used when shipped via rdzv
    
    // rpc producer's should use `reader_of` and `cleanup` as the similarly
    // named template parameters to `command<lpc_base*>::serialize()`. That will allow
    // the `executor` function of the command to be used as the `execute_and_delete`
    // of the lpc.
    static detail::serialization_reader reader_of(detail::lpc_base *me) {
      return detail::serialization_reader(static_cast<rpc_as_lpc*>(me)->payload);
    }
    
    template<bool definitely_not_rdzv, bool restricted=false>
    static void cleanup(detail::lpc_base *me);

    // Build copy of a packed command buffer (upcxx/command.hpp) as a rpc_as_lpc.
    template<typename RpcAsLpc = rpc_as_lpc>
    static RpcAsLpc* build_eager(
      void *cmd_buf, // if null then nothing copied over
      std::size_t cmd_size,
      std::size_t cmd_alignment // alignment requirement of packing
    );
    
    // Allocate space to receive a rdzv. The payload should be used as the dest
    // of the GET. When the GET completes, the command executor should be copied
    // into our `execute_and_delete`.
    template<typename RpcAsLpc = rpc_as_lpc>
    static RpcAsLpc* build_rdzv_lz(
      bool use_sheap,
      std::size_t cmd_size,
      std::size_t cmd_alignment // alignment requirement of packing
    );
  };

  struct bcast_payload_header {
    team_id tm_id;
    union {
      int eager_subrank_ub;
      std::atomic<std::int64_t> rdzv_refs;
    };

    // required by union with nontrivial atomic member
    bcast_payload_header() noexcept {}
    ~bcast_payload_header() noexcept {}
  };
  
  // The receiver-side bcast'd rpc message type. Inherits lpc base type since rpc's
  // reside in the lpc queues.
  struct bcast_as_lpc: rpc_as_lpc {
    int eager_refs;
    
    static detail::serialization_reader reader_of(detail::lpc_base *me) {
      detail::serialization_reader r(static_cast<bcast_as_lpc*>(me)->payload);
      r.unplace(storage_size_of<bcast_payload_header>());
      return r;
    }
    
    template<bool definitely_not_rdzv>
    static void cleanup(detail::lpc_base *me);
  };
  
  template<typename Ub,
           bool is_static_and_eager = (Ub::static_size <= gasnet::am_size_rdzv_cutover_min)>
  struct am_send_buffer;

  template<>
  struct am_send_buffer</*Ub=*/invalid_storage_size_t, /*is_static_and_eager=*/false> {
    void *buffer;
    bool is_eager;
    std::uint16_t cmd_align;
    std::size_t cmd_size;

    static constexpr std::size_t cmd_size_static_ub = std::size_t(-1);
    
    static constexpr std::size_t tiny_size = 512 < serialization_align_max ? 512 : serialization_align_max;
    detail::xaligned_storage<tiny_size, serialization_align_max> tiny_;
    
    detail::serialization_writer</*bounded=*/false> prepare_writer(invalid_storage_size_t, std::size_t rdzv_cutover_size) {
      return detail::serialization_writer<false>(tiny_.storage(), tiny_size);
    }
    
    void finalize_buffer(detail::serialization_writer<false> &&w, std::size_t rdzv_cutover_size) {
      is_eager = w.size() <= gasnet::am_size_rdzv_cutover_min ||
                 w.size() <= rdzv_cutover_size;
      cmd_size = w.size();
      cmd_align = w.align();
      
      if(is_eager && w.contained_in_initial())
        buffer = tiny_.storage();
      else {
        if(is_eager)
          buffer = detail::alloc_aligned(w.size(), w.align());
        else
          buffer = gasnet::allocate(w.size(), w.align(), &gasnet::sheap_footprint_rdzv);
        
        w.compact_and_invalidate(buffer);
      }
    }

    am_send_buffer() = default;
    am_send_buffer(am_send_buffer const&) = delete;
    
    am_send_buffer(am_send_buffer &&that) {
      this->is_eager = that.is_eager;
      this->buffer = that.buffer == that.tiny_.storage() ? this->tiny_.storage() : that.buffer;
      this->tiny_ = that.tiny_;
      this->cmd_size = that.cmd_size;
      this->cmd_align = that.cmd_align;
      that.is_eager = false; // disables destructor
    }

    ~am_send_buffer() {
      if(is_eager && buffer != tiny_.storage())
        std::free(buffer);
    }
  };

  template<typename Ub>
  struct am_send_buffer<Ub, /*is_static_and_eager=*/false> {
    void *buffer;
    bool is_eager;
    std::uint16_t cmd_align;
    std::size_t cmd_size;

    static constexpr std::size_t cmd_size_static_ub = std::size_t(-1);
    
    static constexpr std::size_t tiny_size = 512 < serialization_align_max ? 512 : serialization_align_max;
    static constexpr std::size_t tiny_align = (Ub::static_align_ub < serialization_align_max) ? Ub::static_align_ub : serialization_align_max;
    detail::xaligned_storage<tiny_size, tiny_align> tiny_;

    detail::serialization_writer</*bounded=*/true> prepare_writer(Ub ub, std::size_t rdzv_cutover_size) {
      is_eager = ub.size <= gasnet::am_size_rdzv_cutover_min ||
                 ub.size <= rdzv_cutover_size;
      
      if(is_eager) {
        UPCXX_ASSERT(ub.align <= serialization_align_max);
        if(ub.size <= tiny_size)
          buffer = tiny_.storage();
        else
          buffer = detail::alloc_aligned(ub.size, ub.align);
      }
      else
        buffer = gasnet::allocate(ub.size, ub.align, &gasnet::sheap_footprint_rdzv);
      
      return detail::serialization_writer<true>(buffer);
    }

    void finalize_buffer(detail::serialization_writer<true> &&w, std::size_t rdzv_cutover_size) {
      cmd_size = w.size();
      cmd_align = w.align();
    }
    
    am_send_buffer() = default;
    am_send_buffer(am_send_buffer const&) = delete;

    am_send_buffer(am_send_buffer &&that) {
      this->is_eager = that.is_eager;
      this->buffer = that.buffer == that.tiny_.storage() ? this->tiny_.storage() : that.buffer;
      this->tiny_ = that.tiny_;
      this->cmd_size = that.cmd_size;
      this->cmd_align = that.cmd_align;
      that.is_eager = false; // disables destructor
    }
    
    ~am_send_buffer() {
      if(is_eager && buffer != tiny_.storage())
        std::free(buffer);
    }
  };

  template<typename Ub>
  struct am_send_buffer<Ub, /*is_static_and_eager=*/true> {
    detail::xaligned_storage<Ub::static_size, Ub::static_align> buf_;
    static constexpr bool is_eager = true;
    std::uint16_t cmd_align;
    std::size_t cmd_size;
    void *const buffer = buf_.storage();

    static constexpr std::size_t cmd_size_static_ub = Ub::static_size;
    
    detail::serialization_writer</*bounded=*/true> prepare_writer(Ub, std::size_t rdzv_cutover_size) {
      return detail::serialization_writer<true>(buf_.storage());
    }
    
    void finalize_buffer(detail::serialization_writer<true> &&w, std::size_t rdzv_cutover_size) {
      cmd_size = w.size();
      cmd_align = w.align();
    }

    am_send_buffer() = default;
    am_send_buffer(am_send_buffer const&) = delete;
    
    am_send_buffer(am_send_buffer &&that) {
      this->buf_ = that.buf_;
      this->cmd_size = that.cmd_size;
      this->cmd_align = that.cmd_align;
    }
  };
}}}

//////////////////////////////////////////////////////////////////////
// implementation of: upcxx::backend

namespace upcxx {
namespace backend {
  //////////////////////////////////////////////////////////////////////
  // during_level
  
  template<typename Fn>
  void during_level(
      std::integral_constant<progress_level, progress_level::internal>,
      Fn &&fn,
      persona &active_per
    ) {
    
    // TODO: revisit the purpose of this seemingly wrong assertion
    //UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    fn();
  }
  
  template<typename Fn>
  void during_level(
      std::integral_constant<progress_level, progress_level::user>,
      Fn &&fn,
      persona &active_per
    ) {
    detail::persona_tls &tls = detail::the_persona_tls;
    
    // TODO: revisit the purpose of this seemingly wrong assertion
    //UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller(tls));
    //persona &active_per = UPCXX_BACKEND_GASNET_SEQ
    //  ? backend::master
    //  : *tls.get_top_persona();
    
    tls.during(
      active_per, progress_level::user, std::forward<Fn>(fn),
      /*known_active=*/std::true_type{}
    );
  }

  template<progress_level level, typename Fn>
  void during_level(Fn &&fn, persona &active_per) {
    during_level(
      std::integral_constant<progress_level,level>{},
      std::forward<Fn>(fn),
      active_per
    );
  }
  
  //////////////////////////////////////////////////////////////////////
  // send_am_{master|persona}

  template<typename Fn, bool restricted=false>
  auto prepare_am(
      Fn &&fn,
      std::size_t rdzv_cutover_size = gasnet::am_size_rdzv_cutover,
      std::integral_constant<bool, restricted> restricted1={}
    ) -> gasnet::am_send_buffer<decltype(detail::command<detail::lpc_base*>::ubound(empty_storage_size, fn))> {
    
    using gasnet::am_send_buffer;
    using gasnet::rpc_as_lpc;
    
    auto ub = detail::command<detail::lpc_base*>::ubound(empty_storage_size, fn);
    
    constexpr bool definitely_not_rdzv = ub.static_size <= gasnet::am_size_rdzv_cutover_min;

    am_send_buffer<decltype(ub)> am_buf;
    auto w = am_buf.prepare_writer(ub, rdzv_cutover_size);
    
    detail::command<detail::lpc_base*>::template serialize<
        &rpc_as_lpc::reader_of,
        &rpc_as_lpc::template cleanup<definitely_not_rdzv, restricted>
      >(w, ub.size, fn);

    am_buf.finalize_buffer(std::move(w), rdzv_cutover_size);
    
    return am_buf;
  }

  template<typename AmBuf>
  void send_prepared_am_master(progress_level level, const team &tm, intrank_t recipient, AmBuf &&am) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());

    if(am.is_eager)
      gasnet::send_am_eager_master(level, tm, recipient, am.buffer, am.cmd_size, am.cmd_align);
    else
      gasnet::send_am_rdzv(level, tm, recipient, /*master*/nullptr, am.buffer, am.cmd_size, am.cmd_align);
  }
  
  template<upcxx::progress_level level, typename Fn>
  void send_am_master(const team &tm, intrank_t recipient, Fn &&fn) {
    #if 0
      UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());

      using gasnet::am_send_buffer;
      using gasnet::rpc_as_lpc;
      
      using Fn = typename std::decay<Fn1>::type;

      auto ub = detail::command<detail::lpc_base*>::ubound(empty_storage_size, fn);
      
      constexpr bool definitely_not_rdzv = ub.static_size <= gasnet::am_size_rdzv_cutover_min;
      
      am_send_buffer<decltype(ub)> am_buf;
      auto w = am_buf.prepare_writer(ub, gasnet::am_size_rdzv_cutover);
      
      detail::command<detail::lpc_base*>::template serialize<
          rpc_as_lpc::reader_of,
          rpc_as_lpc::template cleanup</*definitely_not_rdzv=*/definitely_not_rdzv>
        >(w, ub.size, fn);

      am_buf.finalize_buffer(std::move(w), gasnet::am_size_rdzv_cutover);
      
      if(am_buf.is_eager)
        gasnet::send_am_eager_master(level, tm, recipient, am_buf.buffer, am_buf.cmd_size, am_buf.cmd_align);
      else
        gasnet::send_am_rdzv(level, tm, recipient, /*master*/nullptr, am_buf.buffer, am_buf.cmd_size, am_buf.cmd_align);
    #else
      backend::send_prepared_am_master(
        level, tm, recipient, prepare_am(std::forward<Fn>(fn))
      );
    #endif
  }

  template<typename AmBuf>
  void send_prepared_am_persona(
      upcxx::progress_level level, const team &tm,
      intrank_t recipient_rank, persona *recipient_persona,
      AmBuf &&am
    ) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    if(am.is_eager)
      gasnet::send_am_eager_persona(level, tm, recipient_rank, recipient_persona, am.buffer, am.cmd_size, am.cmd_align);
    else
      gasnet::send_am_rdzv(level, tm, recipient_rank, recipient_persona, am.buffer, am.cmd_size, am.cmd_align);
  }
  
  template<upcxx::progress_level level, typename Fn>
  void send_am_persona(
      const team &tm,
      intrank_t recipient_rank,
      persona *recipient_persona,
      Fn &&fn
    ) {
    #if 0
      UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
      
      using gasnet::am_send_buffer;
      using gasnet::rpc_as_lpc;
      
      auto ub = detail::command<detail::lpc_base*>::ubound(empty_storage_size, fn);
      
      constexpr bool definitely_not_rdzv = ub.static_size <= gasnet::am_size_rdzv_cutover_min;

      am_send_buffer<decltype(ub)> am_buf;
      auto w = am_buf.prepare_writer(ub, gasnet::am_size_rdzv_cutover);
      
      detail::command<detail::lpc_base*>::template serialize<
          rpc_as_lpc::reader_of,
          rpc_as_lpc::template cleanup</*definitely_not_rdzv=*/definitely_not_rdzv>
        >(w, ub.size, fn);

      am_buf.finalize_buffer(std::move(w), gasnet::am_size_rdzv_cutover);
      
      if(am_buf.is_eager)
        gasnet::send_am_eager_persona(level, tm, recipient_rank, recipient_persona, am_buf.buffer, am_buf.cmd_size, am_buf.cmd_align);
      else
        gasnet::send_am_rdzv(level, tm, recipient_rank, recipient_persona, am_buf.buffer, am_buf.cmd_size, am_buf.cmd_align);
    #else
      backend::send_prepared_am_persona(
        level, tm, recipient_rank, recipient_persona,
        prepare_am(std::forward<Fn>(fn))
      );
    #endif
  }

  template<typename ...T>
  void send_awaken_lpc(const team &tm, intrank_t recipient, detail::lpc_dormant<T...> *lpc, std::tuple<T...> &&vals) {
    auto am_buf(prepare_am(
      upcxx::bind([=](std::tuple<T...> &&vals) {
          lpc->awaken(std::move(vals));
        },
        std::move(vals)
      ),
      gasnet::am_size_rdzv_cutover,
      /*restricted=*/std::true_type()
    ));

    if(am_buf.is_eager)
      gasnet::send_am_eager_restricted(tm, recipient, am_buf.buffer, am_buf.cmd_size, am_buf.cmd_align);
    else
      gasnet::send_am_rdzv(
        progress_level::internal, tm, recipient,
        // mark low-bit so callee knows its a remote persona**, not a persona*
        reinterpret_cast<persona*>(0x1 | reinterpret_cast<std::uintptr_t>(&lpc->target)),
        am_buf.buffer, am_buf.cmd_size, am_buf.cmd_align
      );
  }
  
  template<progress_level level, typename Fn1>
  void bcast_am_master(const team &tm, Fn1 &&fn) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());
    
    using gasnet::am_send_buffer;
    using gasnet::bcast_as_lpc;
    using gasnet::bcast_payload_header;
    
    auto ub = detail::command<detail::lpc_base*>::ubound(
      empty_storage_size.cat_size_of<bcast_payload_header>(),
      fn
    );
    
    constexpr bool definitely_not_rdzv = ub.static_size <= gasnet::am_size_rdzv_cutover_min;
    std::size_t rdzv_cutover_size = gasnet::am_size_rdzv_cutover;
    
    am_send_buffer<decltype(ub)> am_buf;

    auto w = am_buf.prepare_writer(ub, rdzv_cutover_size);
    w.place(storage_size_of<bcast_payload_header>());
    
    detail::command<detail::lpc_base*>::template serialize<
        bcast_as_lpc::reader_of,
        bcast_as_lpc::template cleanup</*definitely_not_rdzv=*/definitely_not_rdzv>
      >(w, ub.size, fn);
    
    am_buf.finalize_buffer(std::move(w), rdzv_cutover_size);

    bcast_payload_header *payload = new(am_buf.buffer) bcast_payload_header;
    payload->tm_id = tm.id();
    
    if(am_buf.is_eager) {
      gasnet::bcast_am_master_eager(
          level, tm, tm.rank_me() + tm.rank_n(),
          payload, am_buf.cmd_size, am_buf.cmd_align
        );
    }
    else {
      new(&payload->rdzv_refs) std::atomic<std::int64_t>(0);
      
      gasnet::bcast_am_master_rdzv(
          level, tm,
          /*rank_d_ub*/tm.rank_me() + tm.rank_n(),
          /*rank_owner*/backend::rank_me,
          /*payload_owner/sender*/payload, payload,
          am_buf.cmd_size, am_buf.cmd_align
        );
    }
  }
}}

//////////////////////////////////////////////////////////////////////
// implementation of: upcxx::backend::gasnet

namespace upcxx {
namespace backend {
namespace gasnet {
  //////////////////////////////////////////////////////////////////////
  // register_handle_cb

  inline handle_cb_queue& get_handle_cb_queue() {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());

    #if UPCXX_BACKEND_GASNET_SEQ
      return gasnet::master_hcbs;
    #elif UPCXX_BACKEND_GASNET_PAR
      return upcxx::current_persona().backend_state_.hcbs;
    #endif
  }
  
  inline void register_cb(handle_cb *cb) {
    get_handle_cb_queue().enqueue(cb);
  }
  
  //////////////////////////////////////////////////////////////////////
  // send_am_restricted
  
  template<typename Fn>
  void send_am_restricted(const team &tm, intrank_t recipient, Fn &&fn) {
    UPCXX_ASSERT(!UPCXX_BACKEND_GASNET_SEQ || backend::master.active_with_caller());

    auto am_buf(prepare_am(
      std::forward<Fn>(fn), gasnet::am_size_rdzv_cutover, /*restricted=*/std::true_type()
    ));
    
    UPCXX_ASSERT(am_buf.is_eager);
    gasnet::send_am_eager_restricted(tm, recipient, am_buf.buffer, am_buf.cmd_size, am_buf.cmd_align);
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // rpc_as_lpc
  
  template<>
  inline void rpc_as_lpc::cleanup</*definitely_not_rdzv=*/true, /*restricted=*/false>(detail::lpc_base *me1) {
    rpc_as_lpc *me = static_cast<rpc_as_lpc*>(me1);
    std::free(me->payload);
  }
  
  template<>
  inline void rpc_as_lpc::cleanup</*definitely_not_rdzv=*/true, /*restricted=*/true>(detail::lpc_base *me1) {
    // nop
  }

  template<>
  inline void bcast_as_lpc::cleanup</*definitely_not_rdzv=*/true>(detail::lpc_base *me1) {
    bcast_as_lpc *me = static_cast<bcast_as_lpc*>(me1);
    if(0 == --me->eager_refs)
      std::free(me->payload);
  }

  //////////////////////////////////////////////////////////////////////////////
  // rma_put_then_am_master
  
  template<rma_put_then_am_sync sync_lb, bool packed_protocol>
  rma_put_then_am_sync rma_put_then_am_master_protocol(
    const team &tm, intrank_t rank_d,
    void *buf_d, void const *buf_s, std::size_t buf_size,
    progress_level am_level, void *am_cmd, std::size_t am_size, std::size_t am_align,
    handle_cb *src_cb, reply_cb *rem_cb
  );
  
  template<rma_put_then_am_sync sync_lb, typename AmFn>
  rma_put_then_am_sync rma_put_then_am_master(
      const team &tm, intrank_t rank_d,
      void *buf_d, void const *buf_s, std::size_t buf_size,
      progress_level am_level, AmFn &&am_fn,
      handle_cb *src_cb, reply_cb *rem_cb
    ) {

    bool rank_d_is_local = backend::rank_is_local(rank_d);
    
    constexpr std::size_t arg_size = sizeof(std::int32_t);

    auto am(backend::prepare_am(am_fn, rank_d_is_local ? am_size_rdzv_cutover : /*rdzv disabled=*/std::size_t(-1)));

    if(rank_d_is_local) {
      void *buf_d_local = backend::localize_memory_nonnull(rank_d, reinterpret_cast<std::uintptr_t>(buf_d));
      std::memcpy(buf_d_local, buf_s, buf_size);
      backend::send_prepared_am_master(am_level, upcxx::world(), rank_d, std::move(am));
      return rma_put_then_am_sync::op_now;
    }
    else {
      if(am.cmd_size_static_ub <= 13*arg_size || am.cmd_size <= 13*arg_size) {
        return gasnet::template rma_put_then_am_master_protocol<sync_lb, /*packed=*/true>(
          tm, rank_d, buf_d, buf_s, buf_size,
          am_level, am.buffer, am.cmd_size, am.cmd_align,
          src_cb, rem_cb
        );
      }
      else {
        return gasnet::template rma_put_then_am_master_protocol<sync_lb, /*packed=*/false>(
          tm, rank_d, buf_d, buf_s, buf_size,
          am_level, am.buffer, am.cmd_size, am.cmd_align,
          src_cb, rem_cb
        );
      }
    }
  }
}}}

////////////////////////////////////////////////////////////////////////////////
// Bring in all backend definitions in case they haven't been included yet.
#include <upcxx/backend.hpp>

#endif
