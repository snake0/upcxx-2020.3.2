/* This is stub file used when generating libupcxx.a.
 */
#include <upcxx/upcxx.hpp>
#include <upcxx/upcxx_internal.hpp>

namespace upcxx {
  // these must be runtime functions compiled into the library, to comply with xSDK community policy M8
  long release_version() { return UPCXX_VERSION; }
  long spec_version()    { return UPCXX_SPEC_VERSION; }
}

