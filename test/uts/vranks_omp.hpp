#ifndef _bcf4d64e_19bd_4080_86c4_57aaf780e98c
#define _bcf4d64e_19bd_4080_86c4_57aaf780e98c

#include <upcxx/persona.hpp>
#include <upcxx/os_env.hpp>

#include <atomic>
#include <vector>

#include <omp.h>
#include <sched.h>

#define VRANKS_IMPL "omp"
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

    omp_set_num_threads(vrank_n);
    
    #pragma omp parallel num_threads(vrank_n)
    {
      int vrank_me = omp_get_thread_num();
      vranks[vrank_me] = &upcxx::default_persona();
      
      #pragma omp barrier
      
      fn(vrank_me, vrank_n);
    }
  }
}
#endif
