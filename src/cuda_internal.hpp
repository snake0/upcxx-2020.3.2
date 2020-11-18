#ifndef _f49d3597_3d5a_4d7a_822c_d7e602400723
#define _f49d3597_3d5a_4d7a_822c_d7e602400723

#include <upcxx/cuda.hpp>
#include <upcxx/diagnostic.hpp>

#if UPCXX_CUDA_ENABLED
  #include <cuda.h>
  #include <cuda_runtime_api.h>

  namespace upcxx {
    namespace cuda {
      void cu_failed(CUresult res, const char *file, int line, const char *expr);
      void curt_failed(cudaError_t res, const char *file, int line, const char *expr);
    }
  }
  
  #define CU_CHECK(expr) do { \
      CUresult res_xxxxxx = (expr); \
      if(UPCXX_ASSERT_ENABLED && res_xxxxxx != CUDA_SUCCESS) \
        ::upcxx::cuda::cu_failed(res_xxxxxx, __FILE__, __LINE__, #expr); \
    } while(0)

  #define CU_CHECK_ALWAYS(expr) do { \
      CUresult res_xxxxxx = (expr); \
      if(res_xxxxxx != CUDA_SUCCESS) \
        ::upcxx::cuda::cu_failed(res_xxxxxx, __FILE__, __LINE__, #expr); \
    } while(0)

  #define CURT_CHECK(expr) do { \
      cudaError_t res_xxxxxx = (expr); \
      if(UPCXX_ASSERT_ENABLED && res_xxxxxx != cudaSuccess) \
        ::upcxx::cuda::curt_failed(res_xxxxxx, __FILE__, __LINE__, #expr); \
    } while(0)

  #define CURT_CHECK_ALWAYS(expr) do { \
      cudaError_t res_xxxxxx = (expr); \
      if(res_xxxxxx != cudaSuccess) \
        ::upcxx::cuda::curt_failed(res_xxxxxx, __FILE__, __LINE__, #expr); \
    } while(0)

  namespace upcxx {
    namespace cuda {
      struct device_state {
        CUcontext context;
        CUstream stream;
        CUdeviceptr segment_to_free;
      };
    }
  }
#endif
#endif
