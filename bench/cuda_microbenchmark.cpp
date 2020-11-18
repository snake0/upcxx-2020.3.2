#include <upcxx/upcxx.hpp>
#include <iostream>
#include <chrono>

using namespace std;
using namespace upcxx;

template<upcxx::memory_kind src_memory_kind, upcxx::memory_kind dst_memory_kind,
    int flood>
static double helper(int warmup, int window_size, int trials, int len,
        global_ptr<uint8_t, src_memory_kind> &src_ptr,
        global_ptr<uint8_t, dst_memory_kind> &dst_ptr,
        int is_active_rank) {
    double elapsed = 0.0;
    upcxx::future<> all = upcxx::make_future();

    if (is_active_rank) {
        for (int i = 0; i < warmup; i++) {
            for (int j = 0; j < window_size; j++) {
                upcxx::future<> fut = upcxx::copy(src_ptr, dst_ptr, len);
                if (flood) {
                    all = upcxx::when_all(all, fut);
                } else {
                    fut.wait();
                }
            }

            all.wait(); // no-op if !COPY_ASYNC
        }
    }

    upcxx::barrier();

    if (is_active_rank) {
        upcxx::future<> all = upcxx::make_future();

        std::chrono::steady_clock::time_point start =
            std::chrono::steady_clock::now();

        for (int i = 0; i < trials; i++) {
            for (int j = 0; j < window_size; j++) {
                upcxx::future<> fut = upcxx::copy(src_ptr, dst_ptr, len);
                if (flood) {
                    all = upcxx::when_all(all, fut);
                } else {
                    fut.wait();
                }
            }
        }

        all.wait();

        std::chrono::steady_clock::time_point end =
            std::chrono::steady_clock::now();
        elapsed = std::chrono::duration<double>(end - start).count();
    }

    upcxx::barrier();

    return elapsed;
}

template<int flood>
static void run_all_copies(int warmup, int window_size, int trials, int msg_len,
        global_ptr<uint8_t,memory_kind::cuda_device> &local_gpu_array,
        global_ptr<uint8_t,memory_kind::cuda_device> &remote_gpu_array,
        global_ptr<uint8_t, memory_kind::host> &host_array, int is_active_rank,
        double &local_gpu_to_remote_gpu, double &remote_gpu_to_local_gpu,
        double &local_host_to_remote_gpu, double &remote_gpu_to_local_host) {
    local_gpu_to_remote_gpu =
        helper<memory_kind::cuda_device, memory_kind::cuda_device, flood>(
                warmup, window_size, trials, msg_len,
                local_gpu_array, remote_gpu_array, is_active_rank);
    remote_gpu_to_local_gpu =
        helper<memory_kind::cuda_device, memory_kind::cuda_device, flood>(
                warmup, window_size, trials, msg_len,
                remote_gpu_array, local_gpu_array, is_active_rank);
    local_host_to_remote_gpu =
        helper<memory_kind::host, memory_kind::cuda_device, flood>(
                warmup, window_size, trials, msg_len,
                host_array, remote_gpu_array, is_active_rank);
    remote_gpu_to_local_host =
        helper<memory_kind::cuda_device, memory_kind::host, flood>(
                warmup, window_size, trials, msg_len,
                remote_gpu_array, host_array, is_active_rank);
}

static void print_latency_results(double local_gpu_to_remote_gpu,
        double remote_gpu_to_local_gpu, double local_host_to_remote_gpu,
        double remote_gpu_to_local_host, int trials, int window_size) {
    long nmsgs = trials * window_size;

    std::cout << "Latency results for 8-byte transfers" << std::endl;

    std::cout << "  Local GPU -> Remote GPU: " <<
        (local_gpu_to_remote_gpu / double(nmsgs)) <<
        " s of latency" << std::endl;
    std::cout << "  Remote GPU -> Local GPU: " <<
        (remote_gpu_to_local_gpu / double(nmsgs)) <<
        " s of latency" << std::endl;
    std::cout << "  Local Host -> Remote GPU: " <<
        (local_host_to_remote_gpu / double(nmsgs)) <<
        " s of latency" << std::endl;
    std::cout << "  Remote GPU -> Local Host: " <<
        (remote_gpu_to_local_host / double(nmsgs)) <<
        " s of latency" << std::endl;
}

static void print_bandwidth_results(double local_gpu_to_remote_gpu,
        double remote_gpu_to_local_gpu, double local_host_to_remote_gpu,
        double remote_gpu_to_local_host, int trials, int window_size,
        int msg_len, int bidirectional, int flood) {
    std::string sync_type, dir_type;
    long nmsgs = trials * window_size;
    if (bidirectional) {
        nmsgs *= 2;
    }

    long nbytes = nmsgs * msg_len;
    double gbytes = double(nbytes) / (1024.0 * 1024.0 * 1024.0);

    if (flood) {
        sync_type = "Asynchronous";
    } else {
        sync_type = "Blocking";
    }

    if (bidirectional) {
        dir_type = "bi-directional";
    } else {
        dir_type = "uni-directional";
    }

    std::cout << sync_type << " " << dir_type << " bandwidth results for " <<
        "message size = " << msg_len << " byte(s)" << std::endl;

    std::cout << "  Local GPU -> Remote GPU: " <<
        (double(nmsgs) / local_gpu_to_remote_gpu) << " msgs/s, " <<
        (double(gbytes) / local_gpu_to_remote_gpu) << " GB/s" <<
        std::endl;
    std::cout << "  Remote GPU -> Local GPU: " <<
        (double(nmsgs) / remote_gpu_to_local_gpu) << " msgs/s, " <<
        (double(gbytes) / remote_gpu_to_local_gpu) << " GB/s" <<
        std::endl;
    std::cout << "  Local Host -> Remote GPU: " <<
        (double(nmsgs) / local_host_to_remote_gpu) << " msgs/s, " <<
        (double(gbytes) / local_host_to_remote_gpu) << " GB/s" <<
        std::endl;
    std::cout << "  Remote GPU -> Local Host: " <<
        (double(nmsgs) / remote_gpu_to_local_host) << " msgs/s, " <<
        (double(gbytes) / remote_gpu_to_local_host) << " GB/s" <<
        std::endl;
}

int main(int argc, char **argv) {
   {
       upcxx::init();

       const int partner = (rank_me() + 1) % rank_n();
       int max_msg_size = 4 * 1024 * 1024; // 4MB
       std::size_t segsize = max_msg_size;
       auto gpu_device = upcxx::cuda_device( 0 ); // open device 0

       // alloc GPU segment
       auto gpu_alloc = upcxx::device_allocator<upcxx::cuda_device>(gpu_device,
               segsize);

       global_ptr<uint8_t,memory_kind::cuda_device> local_gpu_array =
           gpu_alloc.allocate<uint8_t>(max_msg_size);
       global_ptr<uint8_t, memory_kind::host> host_array =
           upcxx::new_array<uint8_t>(max_msg_size);

       upcxx::dist_object<upcxx::global_ptr<uint8_t, memory_kind::cuda_device>> dobj(local_gpu_array);
       global_ptr<uint8_t, memory_kind::cuda_device> remote_gpu_array =
           dobj.fetch(partner).wait();

       int warmup = 10;
       int trials = 100;
       int window_size = 100;

       if (argc > 1) trials = atoi(argv[1]);
       if (argc > 2) window_size = atoi(argv[2]);

       if (rank_me() == 0) {
           std::cout << "Running " << trials << " trials of window_size=" <<
               window_size << std::endl;
       }

       double local_gpu_to_remote_gpu, remote_gpu_to_local_gpu,
              local_host_to_remote_gpu, remote_gpu_to_local_host;

       run_all_copies<0>(warmup, window_size, trials, 8, local_gpu_array,
               remote_gpu_array, host_array, (rank_me() == 0),
               local_gpu_to_remote_gpu, remote_gpu_to_local_gpu,
               local_host_to_remote_gpu, remote_gpu_to_local_host);

       if (rank_me() == 0) {
           print_latency_results(local_gpu_to_remote_gpu,
                   remote_gpu_to_local_gpu, local_host_to_remote_gpu,
                   remote_gpu_to_local_host, trials, window_size);
           std::cout << std::endl;
       }

       upcxx::barrier();

       int msg_len = 1;
       while (msg_len <= max_msg_size) {
           // Uni-directional blocking bandwidth test
           int is_active_rank = !(rank_me() & 1);
           run_all_copies<0>(warmup, window_size, trials, msg_len,
                   local_gpu_array, remote_gpu_array, host_array, is_active_rank,
                   local_gpu_to_remote_gpu, remote_gpu_to_local_gpu,
                   local_host_to_remote_gpu, remote_gpu_to_local_host);

           if (rank_me() == 0) {
               print_bandwidth_results(local_gpu_to_remote_gpu,
                       remote_gpu_to_local_gpu, local_host_to_remote_gpu,
                       remote_gpu_to_local_host, trials, window_size,
                       msg_len, 0, 0);
               std::cout << std::endl;
           }
           
           upcxx::barrier();

           // Uni-directional non-blocking bandwidth test
           run_all_copies<1>(warmup, window_size, trials, msg_len,
                   local_gpu_array, remote_gpu_array, host_array, is_active_rank,
                   local_gpu_to_remote_gpu, remote_gpu_to_local_gpu,
                   local_host_to_remote_gpu, remote_gpu_to_local_host);

           if (rank_me() == 0) {
               print_bandwidth_results(local_gpu_to_remote_gpu,
                       remote_gpu_to_local_gpu, local_host_to_remote_gpu,
                       remote_gpu_to_local_host, trials, window_size,
                       msg_len, 0, 1);
               std::cout << std::endl;
           }

           upcxx::barrier();

           // Bi-directional blocking bandwidth test
           is_active_rank = 1;
           run_all_copies<0>(warmup, window_size, trials, msg_len,
                   local_gpu_array, remote_gpu_array, host_array, is_active_rank,
                   local_gpu_to_remote_gpu, remote_gpu_to_local_gpu,
                   local_host_to_remote_gpu, remote_gpu_to_local_host);

           if (rank_me() == 0) {
               print_bandwidth_results(local_gpu_to_remote_gpu,
                       remote_gpu_to_local_gpu, local_host_to_remote_gpu,
                       remote_gpu_to_local_host, trials, window_size,
                       msg_len, 1, 0);
               std::cout << std::endl;
           }
           
           upcxx::barrier();

           // Bi-directional non-blocking bandwidth test
           run_all_copies<1>(warmup, window_size, trials, msg_len,
                   local_gpu_array, remote_gpu_array, host_array, is_active_rank,
                   local_gpu_to_remote_gpu, remote_gpu_to_local_gpu,
                   local_host_to_remote_gpu, remote_gpu_to_local_host);

           if (rank_me() == 0) {
               print_bandwidth_results(local_gpu_to_remote_gpu,
                       remote_gpu_to_local_gpu, local_host_to_remote_gpu,
                       remote_gpu_to_local_host, trials, window_size,
                       msg_len, 1, 1);
               std::cout << std::endl;
           }

           upcxx::barrier();

           msg_len *= 2;
       }

       gpu_alloc.deallocate(local_gpu_array);
       upcxx::delete_array(host_array);
       gpu_device.destroy();

       upcxx::barrier();

       if (!rank_me())  std::cout << "SUCCESS" << std::endl;
   }
   upcxx::finalize();
}
