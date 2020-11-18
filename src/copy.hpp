#ifndef _f42075e8_08a8_4472_8972_3919ea92e6ff
#define _f42075e8_08a8_4472_8972_3919ea92e6ff

#include <upcxx/backend.hpp>
#include <upcxx/cuda.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/rput.hpp>

#include <functional>

namespace upcxx {
  namespace detail {
    void rma_copy_get(void *buf_d, intrank_t rank_s, void const *buf_s, std::size_t size, backend::gasnet::handle_cb *cb);
    void rma_copy_put(intrank_t rank_d, void *buf_d, void const *buf_s, std::size_t size, backend::gasnet::handle_cb *cb);
    void rma_copy_local(
        int dev_d, void *buf_d,
        int dev_s, void const *buf_s, std::size_t size,
        cuda::event_cb *cb
      );

    constexpr int host_device = -1;

    template<typename Cxs>
    typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs
    >::return_t
    copy(const int dev_s, const intrank_t rank_s, void *const buf_s,
         const int dev_d, const intrank_t rank_d, void *const buf_d,
         const std::size_t size, Cxs cxs);
  }
  
  template<typename T, memory_kind Ks,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  inline
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs
    >::return_t
  copy(global_ptr<T,Ks> src, T *dest, std::size_t n,
       Cxs cxs=completions<future_cx<operation_cx_event>>{{}}) {
    UPCXX_GPTR_CHK(src);
    UPCXX_ASSERT(src && dest, "pointer arguments to copy may not be null");
    return detail::copy( src.device_, src.rank_, src.raw_ptr_,
                         detail::host_device, upcxx::rank_me(), dest,
                         n * sizeof(T), std::move(cxs) );
  }

  template<typename T, memory_kind Kd,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  inline
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs
    >::return_t
  copy(T const *src, global_ptr<T,Kd> dest, std::size_t n,
       Cxs cxs=completions<future_cx<operation_cx_event>>{{}}) {
    UPCXX_GPTR_CHK(dest);
    UPCXX_ASSERT(src && dest, "pointer arguments to copy may not be null");
    return detail::copy( detail::host_device, upcxx::rank_me(), const_cast<void*>(src),
                         dest.device_, dest.rank_, dest.raw_ptr_,
                         n * sizeof(T), std::move(cxs) );
  }
  
  template<typename T, memory_kind Ks, memory_kind Kd,
           typename Cxs = completions<future_cx<operation_cx_event>>>
  inline
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs
    >::return_t
  copy(global_ptr<T,Ks> src, global_ptr<T,Kd> dest, std::size_t n,
       Cxs cxs=completions<future_cx<operation_cx_event>>{{}}) {
    UPCXX_GPTR_CHK(src); UPCXX_GPTR_CHK(dest);
    UPCXX_ASSERT(src && dest, "pointer arguments to copy may not be null");
    return detail::copy(
      src.device_, src.rank_, src.raw_ptr_,
      dest.device_, dest.rank_, dest.raw_ptr_,
      n*sizeof(T), std::move(cxs)
    );
  }

 namespace detail {
  template<typename Cxs>
  typename detail::completions_returner<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs
  >::return_t
  copy(const int dev_s, const intrank_t rank_s, void *const buf_s,
       const int dev_d, const intrank_t rank_d, void *const buf_d,
       const std::size_t size, Cxs cxs) {
    
    using cxs_here_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_here,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;
    using cxs_remote_t = detail::completions_state<
      /*EventPredicate=*/detail::event_is_remote,
      /*EventValues=*/detail::rput_event_values,
      Cxs>;

    cxs_here_t *cxs_here = new cxs_here_t(std::move(cxs));
    cxs_remote_t cxs_remote(std::move(cxs));

    persona *initiator_per = &upcxx::current_persona();
    
    auto returner = detail::completions_returner<
        /*EventPredicate=*/detail::event_is_here,
        /*EventValues=*/detail::rput_event_values,
        Cxs
      >(*cxs_here);

    if(upcxx::rank_me() != rank_d && upcxx::rank_me() != rank_s) {
      int initiator = upcxx::rank_me();
      
      backend::send_am_master<progress_level::internal>(
        upcxx::world(), rank_d,
        [=]() {
          auto operation_cx_as_internal_future = upcxx::completions<upcxx::future_cx<upcxx::operation_cx_event, progress_level::internal>>{{}};
          
          detail::copy( dev_s, rank_s, buf_s,
                        dev_d, rank_d, buf_d,
                        size, operation_cx_as_internal_future )
          .then([=]() {
            const_cast<cxs_remote_t&>(cxs_remote).template operator()<remote_cx_event>();
            
            backend::send_am_persona<progress_level::internal>(
              upcxx::world(), initiator, initiator_per,
              [=]() {
                cxs_here->template operator()<source_cx_event>();
                cxs_here->template operator()<operation_cx_event>();
                delete cxs_here;
              }
            );
          });
        }
      );
    }
    else if(rank_d == rank_s) {
      detail::rma_copy_local(dev_d, buf_d, dev_s, buf_s, size,
        cuda::make_event_cb([=]() {
          cxs_here->template operator()<source_cx_event>();
          cxs_here->template operator()<operation_cx_event>();
          const_cast<cxs_remote_t&>(cxs_remote).template operator()<remote_cx_event>();
          delete cxs_here;
        })
      );
    }
    else if(rank_d == upcxx::rank_me()) {
      cxs_remote_t *cxs_remote_heaped = new cxs_remote_t(std::move(cxs_remote));
      
      /* We are the destination, so semantically like a GET, even though a PUT
       * is used to transfer on the network
       */
      void *bounce_d;
      if(dev_d == host_device)
        bounce_d = buf_d;
      else {
        bounce_d = backend::gasnet::allocate(size, 64, &backend::gasnet::sheap_footprint_rdzv);
      }
      
      backend::send_am_master<progress_level::internal>(
        upcxx::world(), rank_s,
        [=]() {
          auto make_bounce_s_cont = [=](void *bounce_s) {
            return [=]() {
              detail::rma_copy_put(rank_d, bounce_d, bounce_s, size,
              backend::gasnet::make_handle_cb([=]() {
                  if(dev_s != host_device)
                    backend::gasnet::deallocate(bounce_s, &backend::gasnet::sheap_footprint_rdzv);
                  
                  backend::send_am_persona<progress_level::internal>(
                    upcxx::world(), rank_d, initiator_per,
                    [=]() {
                      cxs_here->template operator()<source_cx_event>();
                      
                      auto bounce_d_cont = [=]() {
                        if(dev_d != host_device)
                          backend::gasnet::deallocate(bounce_d, &backend::gasnet::sheap_footprint_rdzv);

                        cxs_remote_heaped->template operator()<remote_cx_event>();
                        cxs_here->template operator()<operation_cx_event>();
                        delete cxs_remote_heaped;
                        delete cxs_here;
                      };
                      
                      if(dev_d == host_device)
                        bounce_d_cont();
                      else
                        detail::rma_copy_local(dev_d, buf_d, host_device, bounce_d, size, cuda::make_event_cb(bounce_d_cont));
                    }
                  );
                })
              );
            };
          };
          
          if(dev_s == host_device)
            make_bounce_s_cont(buf_s)();
          else {
            void *bounce_s = backend::gasnet::allocate(size, 64, &backend::gasnet::sheap_footprint_rdzv);
            
            detail::rma_copy_local(
              host_device, bounce_s, dev_s, buf_s, size,
              cuda::make_event_cb(make_bounce_s_cont(bounce_s))
            );
          }
        }
      );
    }
    else {
      /* We are the source, so semantically this is a PUT even though we use a
       * GET to transfer over network.
       */
      auto make_bounce_s_cont = [&](void *bounce_s) {
        return [=]() {
          if(dev_s != host_device) {
            // since source side has a bounce buffer, we can signal source_cx as soon
            // as its populated
            cxs_here->template operator()<source_cx_event>();
          }
          
          backend::send_am_master<progress_level::internal>(
            upcxx::world(), rank_d,
            upcxx::bind(
              [=](cxs_remote_t &&cxs_remote) {
                void *bounce_d = dev_d == host_device ? buf_d : backend::gasnet::allocate(size, 64, &backend::gasnet::sheap_footprint_rdzv);
                
                detail::rma_copy_get(bounce_d, rank_s, bounce_s, size,
                  backend::gasnet::make_handle_cb([=]() {
                    auto bounce_d_cont = [=]() {
                      if(dev_d != host_device)
                        backend::gasnet::deallocate(bounce_d, &backend::gasnet::sheap_footprint_rdzv);
                      
                      const_cast<cxs_remote_t&>(cxs_remote).template operator()<remote_cx_event>();

                      backend::send_am_persona<progress_level::internal>(
                        upcxx::world(), rank_s, initiator_per,
                        [=]() {
                          if(dev_s != host_device)
                            backend::gasnet::deallocate(bounce_s, &backend::gasnet::sheap_footprint_rdzv);
                          else {
                            // source didnt use bounce buffer, need to source_cx now
                            cxs_here->template operator()<source_cx_event>();
                          }
                          cxs_here->template operator()<operation_cx_event>();
                          
                          delete cxs_here;
                        }
                      );
                    };
                    
                    if(dev_d == host_device)
                      bounce_d_cont();
                    else
                      detail::rma_copy_local(dev_d, buf_d, host_device, bounce_d, size, cuda::make_event_cb(bounce_d_cont));
                  })
                );
              }, std::move(cxs_remote)
            )
          );
        };
      };

      if(dev_s == host_device)
        make_bounce_s_cont(buf_s)();
      else {
        void *bounce_s = backend::gasnet::allocate(size, 64, &backend::gasnet::sheap_footprint_rdzv);
        
        detail::rma_copy_local(host_device, bounce_s, dev_s, buf_s, size, cuda::make_event_cb(make_bounce_s_cont(bounce_s)));
      }
    }

    return returner();
  }
 } // namespace detail
}
#endif
