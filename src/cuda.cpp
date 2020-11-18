#include <upcxx/cuda.hpp>
#include <upcxx/cuda_internal.hpp>

namespace detail = upcxx::detail;

using std::size_t;

#if UPCXX_CUDA_ENABLED
  namespace {
    detail::segment_allocator make_segment(upcxx::cuda::device_state *st, void *base, size_t size) {
      bool throw_bad_alloc = false;
      CU_CHECK(cuCtxPushCurrent(st->context));
      
      CUdeviceptr p = 0x0;

      if(-size == 1) {
        size_t lo=1<<20, hi=size_t(16)<<30;
        
        while(hi-lo > 64<<10) {
          if(p) cuMemFree(p);
          size = (lo + hi)/2;
          CUresult r = cuMemAlloc(&p, size);

          if(r == CUDA_ERROR_OUT_OF_MEMORY)
            hi = size;
          else if(r == CUDA_SUCCESS)
            lo = size;
          else
            CU_CHECK(r);
        }

        st->segment_to_free = p;
        base = reinterpret_cast<void*>(p);
      }
      else if(base == nullptr) {
        CUresult r = cuMemAlloc(&p, size);
        if(r == CUDA_ERROR_OUT_OF_MEMORY) {
          throw_bad_alloc = true;
          p = reinterpret_cast<CUdeviceptr>(nullptr);
        }
        else
          UPCXX_ASSERT_ALWAYS(r == CUDA_SUCCESS, "Requested cuda allocation failed: size="<<size<<", return="<<int(r));
        
        st->segment_to_free = p;
        base = reinterpret_cast<void*>(p);
      }
      else
        st->segment_to_free = reinterpret_cast<CUdeviceptr>(nullptr);
      
      CUcontext dump;
      CU_CHECK(cuCtxPopCurrent(&dump));

      if(throw_bad_alloc)
        throw std::bad_alloc();
      else
        return detail::segment_allocator(base, size);
    }
  }

  upcxx::cuda::device_state *upcxx::cuda::devices[upcxx::cuda::max_devices] = {/*nullptr...*/};
#endif

#if UPCXX_CUDA_ENABLED
void upcxx::cuda::cu_failed(CUresult res, const char *file, int line, const char *expr) {
  const char *errname, *errstr;
  cuGetErrorName(res, &errname);
  cuGetErrorString(res, &errstr);
  
  std::stringstream ss;
  ss << expr <<"\n  error="<<errname<<": "<<errstr;
  
  upcxx::fatal_error(ss.str(), "CUDA call failed", file, line);
}

void upcxx::cuda::curt_failed(cudaError_t res, const char *file, int line, const char *expr) {
  const char *errname, *errstr;
  errname = cudaGetErrorName(res);
  errstr = cudaGetErrorString(res);
  
  std::stringstream ss;
  ss << expr <<"\n  error="<<errname<<": "<<errstr;
  
  upcxx::fatal_error(ss.str(), "CUDA call failed", file, line);
}
#endif

upcxx::cuda_device::cuda_device(int device):
  device_(device) {

  UPCXX_ASSERT_ALWAYS(backend::master.active_with_caller());

  #if UPCXX_CUDA_ENABLED
    if(device != invalid_device_id) {
      UPCXX_ASSERT_ALWAYS(cuda::devices[device] == nullptr, "Cuda device "<<device<<" already initialized.");
      
      CUcontext ctx;
      CUresult res = cuDevicePrimaryCtxRetain(&ctx, device);
      if(res == CUDA_ERROR_NOT_INITIALIZED) {
        cuInit(0);
        res = cuDevicePrimaryCtxRetain(&ctx, device);
      }
      CU_CHECK_ALWAYS(("cuDevicePrimaryCtxRetain()", res));
      CU_CHECK_ALWAYS(cuCtxPushCurrent(ctx));

      cuda::device_state *st = new cuda::device_state;
      st->context = ctx;
      CU_CHECK_ALWAYS(cuStreamCreate(&st->stream, CU_STREAM_NON_BLOCKING));
      cuda::devices[device] = st;
      
      CU_CHECK_ALWAYS(cuCtxPopCurrent(&ctx));
    }
  #else
    UPCXX_ASSERT_ALWAYS(device == invalid_device_id);
  #endif
}

upcxx::cuda_device::~cuda_device() {
  UPCXX_ASSERT_ALWAYS(device_ == invalid_device_id, "upcxx::cuda_device must have destroy() called before it dies.");
}

void upcxx::cuda_device::destroy(upcxx::entry_barrier eb) {
  UPCXX_ASSERT(backend::master.active_with_caller());

  backend::quiesce(upcxx::world(), eb);

  #if UPCXX_CUDA_ENABLED
  if(device_ != invalid_device_id) {
    cuda::device_state *st = cuda::devices[device_];
    UPCXX_ASSERT(st != nullptr);
    cuda::devices[device_] = nullptr;

    if(st->segment_to_free)
      CU_CHECK_ALWAYS(cuMemFree(st->segment_to_free));
    
    CU_CHECK_ALWAYS(cuCtxPushCurrent(st->context));
    CU_CHECK_ALWAYS(cuStreamDestroy(st->stream));
    CU_CHECK_ALWAYS(cuCtxSetCurrent(nullptr));
    CU_CHECK_ALWAYS(cuDevicePrimaryCtxRelease(device_));
    
    delete st;
  }
  #endif
  
  device_ = invalid_device_id;
}

detail::device_allocator_core<upcxx::cuda_device>::device_allocator_core(
    upcxx::cuda_device *dev, void *base, size_t size
  ):
  detail::device_allocator_base(
    dev ? dev->device_ : upcxx::cuda_device::invalid_device_id,
    #if UPCXX_CUDA_ENABLED
      dev ? make_segment(cuda::devices[dev->device_], base, size)
          : segment_allocator(nullptr, 0)
    #else
      segment_allocator(nullptr, 0)
    #endif
  ) {
  UPCXX_ASSERT_ALWAYS(backend::master.active_with_caller());
}

detail::device_allocator_core<upcxx::cuda_device>::~device_allocator_core() {
  if(upcxx::initialized()) {
    // The thread safety restriction of this call still applies when upcxx isn't
    // initialized, we just have no good way of asserting it so we conditionalize
    // on initialized().
    UPCXX_ASSERT_ALWAYS(backend::master.active_with_caller());
  }

  #if UPCXX_CUDA_ENABLED  
    if(device_ != upcxx::cuda_device::invalid_device_id) {
      cuda::device_state *st = cuda::devices[device_];
      
      if(st && st->segment_to_free) {
        CU_CHECK_ALWAYS(cuCtxPushCurrent(st->context));
        CU_CHECK_ALWAYS(cuMemFree(st->segment_to_free));
        CUcontext dump;
        CU_CHECK_ALWAYS(cuCtxPopCurrent(&dump));

        st->segment_to_free = reinterpret_cast<CUdeviceptr>(nullptr);
      }
    }
  #endif
}
