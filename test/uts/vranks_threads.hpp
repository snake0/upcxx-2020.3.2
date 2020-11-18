#ifndef _62625b7f_859f_4b3a_a765_ff3505ce3d8b
#define _62625b7f_859f_4b3a_a765_ff3505ce3d8b

#include <upcxx/persona.hpp>
#include <upcxx/os_env.hpp>

#include <atomic>
#include <thread>
#include <vector>

#include <sched.h>

#define VRANKS_IMPL "threads"
#define VRANK_LOCAL thread_local

namespace vranks {
  std::vector<upcxx::persona*> vranks;
  
  template<typename Fn>
  void send(int vrank, Fn msg) {
    vranks[vrank]->lpc_ff(std::move(msg));
  }
  
  inline void progress() {
    bool worked = upcxx::detail::the_persona_tls.persona_only_progress();
    
    static thread_local int nothings = 0;
    
    if(worked)
      nothings = 0;
    else if(nothings++ == 10) {
      sched_yield();
      nothings = 0;
    }
  }
  
  template<typename Fn>
  void spawn(Fn fn) {
    int vrank_n = upcxx::os_env<int>("THREADS", 10);
    vranks.resize(vrank_n);
    
    std::vector<std::thread*> threads{(unsigned)vrank_n};
    std::atomic<int> bar0{0};
    
    auto thread_main = [&](int vrank_me) {
      vranks[vrank_me] = &upcxx::default_persona();
      bar0.fetch_add(1);
      while(bar0.load(std::memory_order_acquire) != vrank_n)
        sched_yield();
      
      fn(vrank_me, vrank_n);
    };
    
    for(int vr=1; vr < vrank_n; vr++)
      threads[vr] = new std::thread{thread_main, vr};
    
    thread_main(0);
    
    for(int vr=1; vr < vrank_n; vr++) {
      threads[vr]->join();
      delete threads[vr];
    }
  }
}
#endif
