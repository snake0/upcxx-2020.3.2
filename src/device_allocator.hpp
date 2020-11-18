#ifndef _0a792abf_0420_42b8_91a0_67a4b337f136
#define _0a792abf_0420_42b8_91a0_67a4b337f136

#include <upcxx/backend_fwd.hpp>
#include <upcxx/concurrency.hpp>
#include <upcxx/cuda.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/segment_allocator.hpp>

namespace upcxx {
  namespace detail {
    struct device_allocator_base {
      int device_;
      detail::segment_allocator seg_;

    public:
      device_allocator_base(int device, detail::segment_allocator seg):
        device_(device),
        seg_(std::move(seg)) {
      }
      device_allocator_base(device_allocator_base const&) = delete;
      device_allocator_base(device_allocator_base&&) = default;
    };

    // specialized per device type
    template<typename Device>
    struct device_allocator_core; /*: device_allocator_base {
      static constexpr std::size_t min_alignment;
      template<typename T>
      static constexpr std::size_t default_alignment();

      device_allocator_core(Device &dev, typename Device::pointer<void> base, std::size_t size);
      device_allocator_core(device_allocator_core&&) = default;
    };*/
  }
  
  template<typename Device>
  class device_allocator: detail::device_allocator_core<Device> {
    detail::par_mutex lock_;
    
  public:
    using device_type = Device;

    device_allocator():
      detail::device_allocator_core<Device>(nullptr, Device::template null_pointer<void>(), 0) {
    }
    device_allocator(Device &dev, typename Device::template pointer<void> base, std::size_t size):
      detail::device_allocator_core<Device>(&dev, base, size) {
    }
    device_allocator(Device &dev, std::size_t size):
      detail::device_allocator_core<Device>(&dev, Device::template null_pointer<void>(), size) {
    }
    
    device_allocator(device_allocator &&that):
      // base class move ctor
      detail::device_allocator_core<Device>::device_allocator_core(
        static_cast<detail::device_allocator_core<Device>&&>(
          // use comma operator to create a temporary lock_guard surrounding
          // the invocation of our base class's move ctor
          (std::lock_guard<detail::par_mutex>(that.lock_), that)
        )
      ) {
    }

    template<typename T>
    global_ptr<T,Device::kind> allocate(std::size_t n=1,
                                        std::size_t align = Device::template default_alignment<T>()) {
      lock_.lock();
      void *ptr = this->seg_.allocate(
          n*sizeof(T),
          std::max<std::size_t>(align, this->min_alignment)
        );
      lock_.unlock();
      
      if(ptr == nullptr)
        return global_ptr<T,Device::kind>(nullptr);
      else
        return global_ptr<T,Device::kind>(
          detail::internal_only(),
          upcxx::rank_me(),
          (T*)ptr,
          this->device_
        );
    }

    template<typename T>
    void deallocate(global_ptr<T,Device::kind> p) {
      UPCXX_GPTR_CHK(p);
      if(p) {
        UPCXX_ASSERT(p.device_ == this->device_ && p.rank_ == upcxx::rank_me());
        lock_.lock();
        this->seg_.deallocate(p.raw_ptr_);
        lock_.unlock();
      }
    }

    template<typename T>
    global_ptr<T,Device::kind> to_global_ptr(typename Device::template pointer<T> p) const {
      return global_ptr<T,Device::kind>(
        detail::internal_only(),
        upcxx::rank_me(),
        p,
        this->device_
      );
    }

    #if 0 // removed from spec
    template<typename T>
    global_ptr<T,Device::kind> try_global_ptr(typename Device::template pointer<T> p) const {
      return this->seg_.in_segment((void*)p)
        ? global_ptr<T,Device::kind>(
          detail::internal_only(),
          upcxx::rank_me(),
          p,
          this->device_
        )
        : global_ptr<T,Device::kind>(nullptr);
    }
    #endif
    
    template<typename T>
    static typename Device::id_type device_id(global_ptr<T,Device::kind> gp) {
      UPCXX_GPTR_CHK(gp);
      return Device::invalid_device_id == -1 // this is true statically so faster case will always be taken
        ? gp.device_
        : gp ? gp.device_ : Device::invalid_device_id;
    }
    
    template<typename T>
    static typename Device::template pointer<T> local(global_ptr<T,Device::kind> gp) {
      UPCXX_GPTR_CHK(gp);
      UPCXX_ASSERT(gp.where() == upcxx::rank_me());
      return gp.raw_ptr_;
    }
  };
}

#endif
