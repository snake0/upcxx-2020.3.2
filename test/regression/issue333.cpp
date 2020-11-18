#include <upcxx/upcxx.hpp>

int main(){
  upcxx::init();

  volatile bool cuda_enabled = false;
  upcxx::cuda_device *gpu_device;
  upcxx::device_allocator<upcxx::cuda_device> *gpu_alloc;

  if (cuda_enabled) {
    gpu_device = new upcxx::cuda_device( 0 ); // Open device 0
    gpu_alloc = new upcxx::device_allocator<upcxx::cuda_device>(*gpu_device, 16*1024);

    upcxx::global_ptr<int,upcxx::memory_kind::cuda_device> g =
       gpu_alloc->allocate<int>(4 + gpu_device->default_alignment<int>());
    
  }

  upcxx::finalize();
  return 0;
}
