## ChangeLog

This is the ChangeLog for public releases of [UPC++](https://upcxx.lbl.gov).

For information on using UPC++, see: [README.md](README.md)    
For information on installing UPC++, see: [INSTALL.md](INSTALL.md)

### 2020.07.17: Bug-fix release 2020.3.2

New features/enhancements:

* Shared heap exhaustion in `upcxx::new_(array)` now throws `upcxx::bad_shared_alloc` (a type
  derived from `std::bad_alloc`) which provides additional diagnostics about the failure.

Notable bug fixes:

* issue #343: Guarantee equality for default-constructed invalid `upcxx::team_id`
* issue #353: configure: automatically cross-compile on Cray XC
* issue #356: `SERIALIZED_{FIELDS|VALUES}` incorrectly require public constructors
* issue #369: `completion_cx::as_future()` typically leaks
* issue #371: `team_id`s are not "universal" as documented
* issue #373: No `python` in `$PATH` in recent Linux distros
* issue #380: Compile regression on bulk `upcxx::rput` with source+operation completions

Breaking changes:

* Configure-time envvar `CROSS` has been renamed to `UPCXX_CROSS`.
  For backwards compat, the former is still accepted when the latter is unset.
* Implementation of `upcxx::team_id` is no longer Trivial (was never guaranteed to be).
  It remains DefaultConstructible, TriviallyCopyable, StandardLayoutType, EqualityComparable

### 2020.03.12: Release 2020.3.0

Infrastructure and requirements changes:

* The `install` and `run-tests` scripts have been replaced with a `configure`
  script (with GNU autoconf-compatible options) and corresponding `make all`,
  `make check` and `make install` steps.
  See [INSTALL.md](INSTALL.md) for step-by-step instructions.
* The minimum required GNU Make has risen from 3.79 to 3.80 (released in 2002).
* There is no longer a requirement for Python2.7.  While `upcxx-run` still
  requires a Python interpreter, both Python3 and Python2 (>= 2.7.5) are
  acceptable.

New features/enhancements: (see specification and programmer's guide for full details)

* Added support for non-trivial serialization of user-defined types.  See the 
  [new chapter of the programmer's guide](https://upcxx.lbl.gov/docs/html/guide.html#serialization)
  for an introduction, and [the specification](docs/spec.pdf) for all the details.
* Implement `upcxx_memberof(_general)()`, enabling RMA access to fields of remote objects
* `upcxx::promise` now behaves as a CopyAssignable handle to a reference-counted hidden object,
  meaning users no longer have to worry about promise lifetime issues.
* `atomic_domain<T>` is now implemented for all 32- and 64-bit integer types,
  in addition to the fixed-width integer types, `(u)int{32,64}_t`.
* `atomic_domain<T>` implementation has been streamlined to reduce overhead,
  most notably for targets with affinity to `local_team` using CPU-based atomics.
* Improve GASNet polling heuristic to reduce head-of-line blocking
* Installed CMake package file is now more robust, and supports versioning.
* `global_ptr<T>` now enforces many additional correctness checks in debug mode
* Significantly improve compile latency associated with `upcxx` compiler wrapper script
* Improve handling of `upcxx-run -v` verbose options

Notable bug fixes:

* issue #83: strengthen `global_ptr` correctness assertions
* issue #142: Clarify semantic restrictions on `UPCXX_THREADMODE=seq`
* issue #173: upcxx-run for executables in CWD and error messages
* issue #247: unused-local-typedef warning for some compilers
* issue #266: Redundant completion handler type breaks compile
* issue #267: Same completion value can't be received multiple times
* issue #271: Use of `-pthread` in `example/prog-guide/Makefile` breaks PGI
* issue #277: Ensure `completion_cx::as_promise(p)` works even when p is destroyed prior to fulfillment
* issue #280: Off-by-one error in promise constructor leads to breakage with any explicit argument
* issue #282: Improve installed CMake package files
* issue #287: Bogus install warnings on Cray XC regarding CC/CXX
* issue #289: link failure for PGI with -std=c++17
* issue #294: redundant-move warning in completion.hpp from g++-9.2
* issue #304: Bad behavior for misspelled CC or CXX
* issue #323: Incorrect behavior for `global_ptr<T>(nullptr).is_local()` in multi-node jobs
* issue #333: Multiply defined symbol `detail::device_allocator_core<cuda_device>::min_alignment` w/ std=c++2a
* [spec issue #148](https://bitbucket.org/berkeleylab/upcxx-spec/issues/148): Missing `const` qualifiers on team and other API objects
* [spec issue #155](https://bitbucket.org/berkeleylab/upcxx-spec/issues/155): value argument type to value collectives is changed to a simple by-value T

This library release mostly conforms to the
[UPC++ v1.0 Specification, Revision 2020.3.0](docs/spec.pdf).
The following features from that specification are not yet implemented:

* view buffer lifetime extension for `remote_cx::as_rpc` (issue #262)
* `boost_serialization` adapter class is not yet implemented (issue #330)
* `future<T&&>::{result,wait}_reference` currently return the wrong type.
  E.g. `future<int&&>::result_reference()` returns `int const&` instead of `int&&` (issue #313)

Breaking changes:

* `atomic_domain` has been specified as non-DefaultConstructible since UPC++ spec draft 8 (2018.9)
  This release removes the default constructor implementation from `atomic_domain` (issue #316).
* The trait template classes `upcxx::is_definitely_trivially_serializable` and
  `upcxx::is_definitely_serializable` have been renamed such that the "definitely"
  word has been dropped, e.g.: `upcxx::is_trivially_serializable`.
* `future::{result,wait}_moved`, deprecated since 2019.9.0, have been removed.
  New `future::{result,wait}_reference` calls provide related functionality.

### 2019.09.14: Release 2019.9.0

New features/enhancements: (see specification and programmer's guide for full details)

* `upcxx` has several new convenience options (see `upcxx -help`)
* `upcxx::rput(..., remote_cx::as_rpc(...))` has received an improved implementation
  for remote peers where the dependent rpc is injected immediately following
  the put. This pipelining reduces latency and sensitivity to initiator attentiveness,
  improving performance in most cases (for the exception, see issue #261).
* Accounting measures have been added to track the shared-heap utilization of the
  UPC++ runtime (specifically rendezvous buffers) so that in the case of shared-heap
  exhaustion an informative assertion will fire. Also, fewer rendezvous buffers are
  now required by the runtime, thus alleviating some pressure on the shared heap.
* Environment variable `UPCXX_OVERSUBSCRIBED` has been added to control whether the 
  runtime should yield to OS (`sched_yield()`) within calls to `upcxx::progress()`).
  See [docs/oversubscription.md](docs/oversubscription.md).
* Release tarball downloads now embed a copy of GASNet-EX that is used by default during install.
  Git clones of the repo will still default to downloading GASNet-EX during install.
  The `GASNET` envvar can still be set at install time to change the default behavior.
* A CMake module for UPC++ is now installed. See 'Using UPC++ with CMake' in [README.md](README.md)
* `atomic_domain<float>` and `atomic_domain<double>` are now implemented
* Interoperability support for Berkeley UPC's `-pthreads` mode, see [docs/upc-hybrid.md](docs/upc-hybrid.md)
* New define `UPCXX_SPEC_VERSION` documents the implemented revision of the UPC++ specification

Support has been added for the following compilers/platforms 
(for details, see 'System Requirements' in [INSTALL.md](INSTALL.md)):

* PGI v19.1+ on Linux/x86\_64
* PGI v18.10+ on Linux/ppc64le
* clang v5.0+ on Linux/ppc64le
* PrgEnv/cray with CCE v9.0+ on the Cray XC
* ALCF's PrgEnv/llvm v4.0+ on the Cray XC
* NEW platform: Linux/aarch64 (aka "arm64" or "armv8")
    + gcc v6.4.0+
    + clang 4.0.0+

Notable bug fixes:

* issue #140: `upcxx::discharge()` does not discharge `remote_cx::as_rpc()`
* issue #168: `upcxx::progress_required` always returns 0
* issue #170: `team_id::when_here()` is unimplemented
* issue #181: Library linkage failures when user compiles with a different `-std=c++` level
* issue #184: `bench/put_flood` crashes on opt/Linux
* issue #203: strict aliasing violations in `device_allocator`
* issue #204: No support for `nvcc --compiler-bindir=...`
* issue #210: `cuda_device::default_alignment()` not implemented
* issue #223: `operator<<(std::ostream, global_ptr<T>)` does not match spec
* issue #224: missing const qualifier on `dist_object<T>.fetch()`
* issue #228: incorrect behavior for `upcxx -g -O`
* issue #229: Teach `upcxx` wrapper to compile C language files
* issue #234: Generalized operation completion for `barrier_async` and `broadcast`
* issue #243: Honor `$UPCXX_PYTHON` during install
* issue #260: `GASNET_CONFIGURE_ARGS` can break UPC++ build
* issue #264: `upcxx-meta CXX` and `CC` are not full-path expanded
* issue #268: Completion handlers can't accept `&&` references
* [spec issue #141](https://bitbucket.org/berkeleylab/upcxx-spec/issues/141): resolve empty transfer ambiguities (count=0 RMA)
* [spec issue #142](https://bitbucket.org/berkeleylab/upcxx-spec/issues/142): add `persona::active_with_caller()`

This library release mostly conforms to the
[UPC++ v1.0 Specification, Revision 2019.9.0](https://bitbucket.org/berkeleylab/upcxx/downloads/upcxx-spec-2019.9.0.pdf).
The following features from that specification are not yet implemented:

* view buffer lifetime extension for `remote_cx::as_rpc` (issue #262)
* User-defined Class Serialization interface (coming soon!)

Breaking changes:

* Applications are recommended to replace calls to `std::getenv` with `upcxx::getenv_console`,
  to maximize portability to loosely coupled distributed systems.
* envvar `UPCXX_GASNET_CONDUIT` has been renamed to `UPCXX_NETWORK`.
  For backwards compat, the former is still accepted when the latter is unset.
* `upcxx::allocate()` and `device_allocator<Device>::allocate()` have changed signature.
  The `alignment` parameter has moved from being a final defaulted
  template argument to being a final defaulted function argument.
* `future<T...>::result_moved()` and `future<T...>::wait_moved()` are deprecated,
  and will be removed in a future release.

### 2019.05.27: Bug-fix release 2019.3.2

Notable bug fixes:

* issue #209: Broken install defaulting of CC/CXX on macOS

Fixes the following notable bug in the GASNet library
  (see https://gasnet-bugs.lbl.gov for details):

* bug3943: infrequent startup hang with PSHM and over 62 PPN

### 2019.03.15: Release 2019.3.0

New features/enhancements: (see specification and programmer's guide for full details)

* Prototype Memory Kinds support for CUDA-based NVIDIA GPUs, see [INSTALL.md](INSTALL.md).
    Note the CUDA support in this UPC++ release is a proof-of-concept reference implementation
    which has not been tuned for performance. In particular, the current implementation of
    `upcxx::copy` does not utilize hardware offload and is expected to underperform 
    relative to solutions using RDMA, GPUDirect and similar technologies.
    Performance will improve in an upcoming release.
* Support for interoperability with Berkeley UPC, see [upc-hybrid.md](docs/upc-hybrid.md)
* There is now an offline installer package for UPC++, for systems lacking connectivity
* Barrier synchronization performance has been improved
* Installer now defaults to more build parallelism, improving efficiency (see `UPCXX_MAKE`)

Notable bug fixes:

* issue #100: Fix shared heap setting propagation on loosely-coupled clusters
* issue #118: Enforce GEX version interlock at compile time
* issue #177: Completion broken for non-fetching binary AMOs
* issue #183: `bench/{put_flood,nebr_exchange}` were failing to compile
* issue #185: Fix argument order for `dist_object` constructor to match spec
* issue #187: Improve Python detection logic for the install script
* issue #190: Teach upcxx-run to honor `UPCXX_PYTHON`
* issue #202: Make `global_ptr::operator bool` conversion explicit 
* issue #205: incorrect metadata handling in `~persona_scope()`

This library release mostly conforms to the
[UPC++ v1.0 Draft 10 Specification](https://bitbucket.org/berkeleylab/upcxx/downloads/upcxx-spec-V1.0-Draft10.pdf).
The following features from that specification are not yet implemented:

* `barrier_async()` and `broadcast()` only support default future-based completion (issue #234)
* `atomic_domain<float>` and `atomic_domain<double>` are not yet implemented (issue #235)
* `team_id::when_here()` is unimplemented (issue #170)
* User-defined Class Serialization interface 

Breaking changes:

* envvar `UPCXX_SEGMENT_MB` has been renamed to `UPCXX_SHARED_HEAP_SIZE`.
  For backwards compat, the former is still accepted when the latter is unset.
* The minimum-supported version of GNU g++ is now 6.4.0
    - This also applies to the stdlibc++ used by Clang or Intel compilers
* The minimum-supported version of llvm/clang for Linux is now 4.0

### 2018.09.26: Release 2018.9.0

New features/enhancements: (see specification and programmer's guide for full details)

* Subset teams and team-aware APIs are added and implemented
* Non-Blocking Collective operations, with team support: barrier, broadcast, reduce
* New atomic operations: `mul, min, max, bit_{and,or,xor}`
* `future::{wait,result}*` return types are now "smarter", allowing more concise syntax
* New `upcxx` compiler wrapper makes it easier to build UPC++ programs
* `upcxx-run`: improved functionality and handling of -shared-heap arguments
* New supported platforms:
    - GNU g++ compiler on macOS (e.g., as installed by Homebrew or Fink)
    - PrgEnv-intel version 17.0.2 or later on Cray XC x86-64 systems
    - Intel C++ version 17.0.2 or later on x86-64/Linux
    - GNU g++ compiler on ppc64le/Linux
* `rput_{strided,(ir)regular}` now provide asynchronous source completion
* Performance improvements to futures, promises and LPCs
* UPC++ library now contains ident strings that can be used to query version info
  from a compiled executable, using the UNIX `ident` tool.

Notable bug fixes:

* issue #49: stability and portability issues caused by C++ `thread_local`
* issue #141: missing promise move assignment operator

This library release mostly conforms to the
[UPC++ v1.0 Draft 8 Specification](https://bitbucket.org/berkeleylab/upcxx/downloads/upcxx-spec-V1.0-Draft8.pdf).
The following features from that specification are not yet implemented:

* `barrier_async()` and `broadcast()` only support default future-based completion (issue #234)
* `atomic_domain<float>` and `atomic_domain<double>` are not yet implemented (issue #235)
* `team_id::when_here()` is unimplemented (issue #170)
* User-defined Serialization interface

Breaking changes:

* `global_ptr<T>(T*)` "up-cast" constructor has been replaced with `to_global_ptr<T>(T*)`
* `atomic_domain` now requires a call to new collective `destroy()` before destructor
* `allreduce` has been renamed to `reduce_all`

### 2018.05.10: Release 2018.3.2

This is a re-release of version 2018.3.0 (see below) that corrects a packaging error.

### 2018.03.31: Release 2018.3.0

New features/enhancements:

 * Non-Contiguous One-Sided RMA interface is now fully implemented.
 * Remote Atomics have been revamped, expanded and implemented. See the updated specification
   for usage details.  The current implementation leverages hardware support in
   shared memory and NIC offload support in Cray Aries.
 * View-Based Serialization - see the specification for details
 * Implementation of local memory translation (achieved with
   `global_ptr::local()` / `global_ptr(T*)`). This encompasses a limited
   implementation of teams to support `upcxx::world` and `upcxx::local_team`
   so clients may query their local neighborhood of ranks.

Notable bug fixes:

 * issue #119: Build system is now more robust to GASNet-EX download failures.
 * issue #125: Fix upcxx-run exit code handling.
 * Minor improvements to upcxx-run and run-tests.

This library release mostly conforms to the
[UPC++ v1.0 Draft 6 Specification](https://bitbucket.org/berkeleylab/upcxx/downloads/upcxx-spec-V1.0-Draft6.pdf).
The following features from that specification are not yet implemented:

 * Teams: `team::split`, `team_id`, collectives over teams, passing
       `team&` arguments to rpcs, constructing `dist_object` over teams.
 * Vector broadcast `broadcast(T *buf, size_t count, ...)`
 * `barrier_async`
 * User-defined Serialization interface

This release is not yet performant, and may be unstable or buggy.

Please report any problems in the [issue tracker](https://upcxx-bugs.lbl.gov).

### 2018.01.31: Release 2018.1.0 BETA

This is a BETA preview release of UPC++ v1.0. 

New features/enhancements:

 * Generalized completion. This allows the application to be notified about the
   status of UPC\+\+ operations in a handful of ways. For each event, the user
   is free to choose among: futures, promises, callbacks, delivery of remote
   rpc, and in some cases even blocking until the event has occurred.
 * Internal use of lock-free datastructures for `lpc` queues.
 * Improvements to the `upcxx-run` command.
 * Improvements to internal assertion checking and diagnostics.
  
This library release mostly conforms to the
[UPC++ v1.0 Draft 5 Specification](https://bitbucket.org/berkeleylab/upcxx/downloads/upcxx-spec-V1.0-Draft5.pdf).
The following features from that specification are not yet implemented:

 * Teams
 * Vector broadcast `broadcast(T *buf, size_t count, ...)`
 * `barrier_async`
 * Serialization
 * Non-contiguous transfers
 * Atomics

This release is not performant, and may be unstable or buggy.

### 2017.09.30: Release 2017.9.0

The initial public release of UPC++ v1.0. 

This library release mostly conforms to the
[UPC++ v1.0 Draft 4 Specification](https://bitbucket.org/berkeleylab/upcxx/downloads/upcxx-spec-V1.0-Draft4.pdf).
The following features from that specification are not yet implemented:

 * Continuation-based and Promise-based completion (use future completion for
   now)
 * `rput_then_rpc`
 * Teams
 * Vector broadcast `broadcast(T *buf, size_t count, ...)`
 * `barrier_async`
 * Serialization
 * Non-contiguous transfers

This release is not performant, and may be unstable or buggy.

### 2017.09.01: Release v1.0-pre

This is a prerelease of v1.0. This prerelease supports most of the functionality
covered in the UPC++ specification, except personas, promise-based completion,
teams, serialization, and non-contiguous transfers. This prerelease is not
performant, and may be unstable or buggy. Please notify us of issues by sending
email to `upcxx@googlegroups.com`.

