#ifndef _a6becf70_cd32_4da7_82e9_379ce395b781
#define _a6becf70_cd32_4da7_82e9_379ce395b781

// UPCXX_VERSION release identifier format:
// YYYYMMPPL = [YEAR][MONTH][PATCH]
#define UPCXX_VERSION 20200302L

// UPCXX_SPEC_VERSION release identifier format:
// YYYYMM00L = [YEAR][MONTH]
#define UPCXX_SPEC_VERSION 20200300L

namespace upcxx {
  long release_version();
  long spec_version();
}

#include <upcxx/allocate.hpp>
#include <upcxx/atomic.hpp>
#include <upcxx/backend.hpp>
#include <upcxx/barrier.hpp>
#include <upcxx/broadcast.hpp>
#include <upcxx/copy.hpp>
#include <upcxx/cuda.hpp>
#include <upcxx/dist_object.hpp>
#include <upcxx/future.hpp>
#include <upcxx/global_ptr.hpp>
#include <upcxx/os_env.hpp>
#include <upcxx/persona.hpp>
#include <upcxx/reduce.hpp>
#include <upcxx/rget.hpp>
#include <upcxx/rput.hpp>
#include <upcxx/rpc.hpp>
#include <upcxx/team.hpp>
#include <upcxx/vis.hpp>
//#include <upcxx/wait.hpp>
#include <upcxx/view.hpp>
#include <upcxx/memberof.hpp>

#endif
