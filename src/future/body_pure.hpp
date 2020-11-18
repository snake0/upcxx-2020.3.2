#ifndef _f4b79a6c_897a_4e4b_8f8f_23fb28c6b7fd
#define _f4b79a6c_897a_4e4b_8f8f_23fb28c6b7fd

#include <upcxx/future/core.hpp>

namespace upcxx {
namespace detail {
  ////////////////////////////////////////////////////////////////////////
  // future_body_pure
  
  template<typename Kind, typename ...T>
  struct future_body_pure<future1<Kind, T...>> final: future_body {
    future_dependency<future1<Kind, T...>> dep_;
    
  public:
    future_body_pure(
        void *storage,
        future_header_dependent *suc_hdr,
        future1<Kind, T...> arg
      ):
      future_body{storage},
      dep_{suc_hdr, std::move(arg)} {
    }
    
    void destruct_early() {
      this->dep_.cleanup_early();
      this->~future_body_pure();
    }
    
    void leave_active(future_header_dependent *hdr) {
      void *storage = this->storage_;
      
      if(0 == hdr->decref(1)) { // left active queue
        this->dep_.cleanup_ready();
        this->~future_body_pure();
        future_body::operator delete(storage);
        delete hdr;
      }
      else {
        future_header *result = this->dep_.cleanup_ready_get_header();
        this->~future_body_pure();
        future_body::operator delete(storage);
        
        hdr->enter_ready(result);
      }
    }
  };
}}
#endif
