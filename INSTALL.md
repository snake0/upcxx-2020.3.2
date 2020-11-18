# UPC\+\+ Installation #

This file documents software installation of [UPC++](https://upcxx.lbl.gov).

For information on using UPC++, see: [README.md](README.md)    

## System Requirements

### Supported Platforms

UPC++ makes aggressive use of template meta-programming techniques, and requires
a modern C++11/14 compiler and corresponding STL implementation.

The current release is known to work on the following configurations:

* Apple macOS/x86\_64 (smp and udp conduits):
    - The most recent Xcode release for each macOS release is well-tested
        + It is suspected that any Xcode (ie Apple clang) release 8.0 or newer will work
    - Free Software Foundation g++ (e.g., as installed by Homebrew or Fink)
      version 6.4.0 or newer should also work

* Linux/x86\_64 with one of the following compilers:
    - g++ 6.4.0 or newer    
    - clang++ 4.0.0 or newer (with libstdc++ from g++ 6.4.0 or newer)    
    - Intel C++ 17.0.2 or newer (with libstdc++ from g++ 6.4.0 or newer)    
    - PGI C++ 19.1 or newer (with libstdc++ from g++ 6.4.0 or newer)    

    If `/usr/bin/g++` is older than 6.4.0 (even if using another compiler),
    see [Linux Compiler Notes](#markdown-header-linux-compiler-notes), below.

* Linux/ppc64le (aka IBM POWER little-endian) with one of the following compilers:
    - g++ 6.4.0 or newer
    - clang++ 5.0.0 or newer (with libstdc++ from g++ 6.4.0 or newer)    
    - PGI C++ 18.10 or newer (with libstdc++ from g++ 6.4.0 or newer)    

    If `/usr/bin/g++` is older than 6.4.0 (even if using another compiler),
    see [Linux Compiler Notes](#markdown-header-linux-compiler-notes), below.

* Linux/aarch64 (aka "arm64" or "armv8") with one of the following compilers:
    - g++ 6.4.0 or newer
    - clang++ 4.0.0 or newer (with libstdc++ from g++ 6.4.0 or newer)   

    If `/usr/bin/g++` is older than 6.4.0 (even if using another compiler),
    see [Linux Compiler Notes](#markdown-header-linux-compiler-notes), below.

    Note that gcc- and clang-based toolchains from Arm Ltd. exist, but have
    not been tested with UPC++.

    Support for InfiniBand on Linux/aarch64 should be considered experimental.
    For more information, please see
    [GASNet bug 3997](https://gasnet-bugs.lbl.gov/bugzilla/show_bug.cgi?id=3997).

* Cray XC/x86\_64 with one of the following PrgEnv environment modules and
  its dependencies (smp and aries conduits):
    - PrgEnv-gnu with gcc/6.4.0 (or later) loaded.
    - PrgEnv-intel with gcc/6.4.0 (or later) loaded.
    - PrgEnv-cray with cce/9.0.0 (or later) loaded.
      Note that does not include support for "cce/9.x.y-classic".

    ALCF's PrgEnv-llvm is also supported on the Cray XC.  Unlike Cray's
    PrgEnv-\* modules, PrgEnv-llvm is versioned to match the llvm toolchain
    it includes, rather than the Cray PE version.  UPC++ has been tested
    against PrgEnv-llvm/4.0 (clang++ 4.0) and newer.  When using PrgEnv-llvm,
    it is recommended to `module unload xalt` to avoid a large volume of
    verbose linker output in this configuration.  Mixing with OpenMP in this
    configuration is not currently supported.  (smp and aries conduits).

### Miscellaneous software requirements:

* Python3 or Python2 version 2.7.5 or newer

* Perl version 5.005 or newer

* GNU Bash 3.2 or newer (must be installed, user's shell doesn't matter)

* GNU Make 3.80 or newer

* The following standard Unix tools: 'awk', 'sed', 'env', 'basename', 'dirname'

### Linux Compiler Notes:

* If /usr/bin/g++ is older than 6.4.0 (even if using a different C++
  compiler for UPC++) please read [docs/local-gcc.md](docs/local-gcc.md).

* If using a non-GNU compiler with /usr/bin/g++ older than 6.4.0, please also
  read [docs/alt-compilers.md](docs/alt-compilers.md).

## Installation Instructions

The recipe for building and installing UPC\+\+ is the same as many packages
using the GNU Autoconf and Automake infrastructure (though UPC\+\+ does not
use either).  The high-level steps are as follows:

1. `configure`  
     Configures UPC\+\+ with key settings such as the installation location
2. `make all`  
     Compiles the UPC\+\+ package
3. `make check` (optional, but recommended)  
     Verifies the correctness of the UPC\+\+ build prior to its installation
4. `make install`  
     Installs the UPC\+\+ package to the user-specified location
5. `make test_install` (optionally, but highly recommended)  
     Verifies the installed package
6. Post-install recommendations

The following numbered sections provide detailed descriptions of each step above.
Following those are sections with platform-specific instructions.

#### 1. Configuring UPC\+\+

```bash
cd <upcxx-source-dir>
./configure  --prefix=<upcxx-install-path>
```

Or, to have distinct source and build trees (for instance to compile multiple
configurations from a common source directory):
```bash
mkdir <upcxx-build-path>
cd <upcxx-build-path>
<upcxx-source-path>/configure  --prefix=<upcxx-install-path>
```

This will configure the UPC\+\+ library to be installed to the given
`<upcxx-install-path>` directory. Users are recommended to use paths to
non-existent or empty directories as the installation path so that
uninstallation is as trivial as `rm -rf <upcxx-install-path>`.

Depending on the platform, additional command-line arguments may be necessary
when invoking `configure`. For guidance, see the platform-specific instructions
in the following sections, below:

* [Configuration: Cray XC](#markdown-header-configuration-cray-xc)
* [Configuration: Linux](#markdown-header-configuration-linux)
* [Configuration: Apple macOS](#markdown-header-configuration-apple-macos)
* [Configuration: CUDA GPU support](#markdown-header-configuration-cuda-gpu-support)

Running `<upcxx-source-path>/configure --help` will provide general
information on the available configuration options, and similar information is
provided in the [Advanced Configuration](#markdown-header-advanced-configuration)
section below.

If you are using a source tarball release downloaded from the website, it
should include an embedded copy of GASNet-EX and `configure` will default to
using that.  However if you are using a git clone or other repo snapshot of
UPC++, then `configure` may default to downloading the GASNet-EX communication
library, in which case an Internet connection is needed at configuration time.

GNU Make 3.80 or newer is required to build UPC\+\+.  If neither `make` nor
`gmake` in your `$PATH` meets this requirement, you may use `--with-gmake=...`
to specify the full path to an appropriate version.  You may need to
substitute `gmake`, or your configured value, for `make` where it appears in
the following steps.  The final output from `configure` will provide the
appropriate commands.

Python3 or Python2 (version 2.7.5 or later) is required by UPC\+\+.  By
default, `configure` searches `$PATH` for several common Python interpreter
names.  If that does not produce a suitable interpreter, you may override
this using `--with-python=...` to specify a python interpreter.  If you
provide a full path, the value is used as given.  Otherwise, the `$PATH` at
configure-time is searched to produce a full path.  Either way, the resulting
full path to the python interpreter will be used in the installed `upcxx-run`
script, rather than a runtime search of `$PATH`.  Therefore, the interpreter
specified must be available in a batch-job environment where applicable.

Bash 3.2 or newer is required by UPC\+\+ scripts, including `configure`.  By
default, `configure` will try `/bin/sh` and then the first instance of `bash`
found in `$PATH`.  If neither of these is bash 3.2 (or newer), or if the one
found is not appropriate to use (for instance not accessible on compute
nodes), one can override the automated selection by invoking `configure` _via_
the desired instance of `bash`:
```bash
/usr/gnu/bin/bash <upcxx-source-path>/configure ...
```

#### 2. Compiling UPC\+\+

```bash
make all
```

This will compile the UPC\+\+ runtime libraries, including the GASNet-EX
communications runtime.  One may run, for instance, `make -j8 all` to build
with eight concurrent processes.  This may significantly reduce the time
required. However parallel make can also obscure error messages, so if you
encounter a failure you should retry without a `-j` option.

#### 3. Testing the UPC\+\+ build (optional)

Though it is not required, we recommend testing the completeness and correctness
of the UPC\+\+ build before proceeding to the installation step.  In general
the environment used to compile UPC\+\+ tests and run them may not be the
same (most notably, on batch-scheduled and/or cross-compiled platforms).
The following command assumes it is invoked in an environment suitable for *both*,
if such is available:

```bash
make check
```

This compiles all available tests for the default network and then runs them.
One can override the default network by appending `NETWORKS='net1 net2'`
to this command, with network names (such as `smp`, `udp`, `ibv` or `aries`)
substituted for the `netN` placeholders.

Setting of `NETWORKS` to restrict what is tested may be necessary, for
instance, if GASNet-EX detected libraries for a network not physically present
in your system.  This will often occur for InfiniBand (which GASNet-EX
identifies as `ibv`) due to presence of the associated libraries on many Linux
distributions.  One may, if desired, return to the configure step and pass
`--disable-ibv` (or other undesired network) to remove support for a given
network from the build of UPC\+\+.

If it is not possible to both compile and run parallel applications in the
same environment, then one may apply the following two steps in place of
`make check`:

1. In an environment suited to compilation, run `make tests-clean tests`.
This will remove any test executables left over from previous attempts, and
then compiles all tests for all available networks.  One may restrict this to
a subset of the available networks by appending a setting for `NETWORKS`,
as described above for `make check`.

2. In an environment suited to execution of parallel applications, run
`make run-tests`.  As in the first step, one may set `NETWORKS` on the `make`
command line to limit the tests run to some subset of the tests built above.

#### 4. Installing the compiled UPC\+\+ package

```bash
make install
```

This will install the UPC\+\+ runtime libraries and accompanying utilities to
the location specified via `--prefix=...` at configuration time.  If that
value is not the desired installation location, then `make install
prefix=<desired-install-directory>` may be used to override the value given at
configure time.

#### 5. Testing the install UPC\+\+ package (optional)

```bash
make tests-clean test_install
```

This optional command removes any test executables left over from previous
attempts, and then builds a simple "Hello, World" test for each supported
network using the *installed* UPC\+\+ libraries and compiler wrapper.

At the end of the output will be instructions for running these tests if
desired.

#### 6. Post-install recommendations

After step 5 (or step 4, if skipping step 5) one may safely remove the
directory `<upcxx-source-path>` (and `<upcxx-build-path>`, if used) since they
are not needed by the installed package.

One may use the utilities `upcxx` (compiler wrapper), `upcxx-run` (launch
wrapper) and `upcxx-meta` (UPC\+\+ metadata utility) by their full path in
`<upcxx-install-path>/bin`.  However, it is common to append that directory to
one's `$PATH` environment variable (the best means to do so are beyond this
scope of this document).

Additionally, one may wish to set the environment variable `$UPCXX_INSTALL`
to `<upcxx-install-path>`, as this is assumed by several UPC\+\+ examples.

For systems using "environment modules" an example modulefile is provided
as `<upcxx-install-path>/share/modulefiles/upcxx/<upcxx-version>`.  This
sets both `$PATH` and `$UPCXX_INSTALL` as recommended above.  Consult
the documentation for the environment modules package on how to use this file.

For users of CMake 3.6 or newer, `<upcxx-install-path>/share/cmake/UPCXX`
contains a `UPCXXConfig.cmake`.  Consult CMake documentation for instructions
on use of this file.

Finally, `<upcxx-install-path>/bin/test-upcxx-install.sh` is a script which can
be run to replicate the verification performed by `make test_install` _without_
`<upcxx-source-path>` and/or `<upcxx-build-path>`.  This could be useful, for
instance, to verify permissions for a user other than the one performing the
installation.

### Configuration: Cray XC

By default, on a Cray XC logic in `configure` will automatically detect either
the SLURM or Cray ALPS job scheduler and will cross-configure for the
appropriate one.  If this auto-detection fails, you may need to explicitly
pass the appropriate value for your system:

* `--with-cross=cray-aries-slurm`: Cray XC systems using the SLURM job scheduler (srun)
* `--with-cross=cray-aries-alps`: Cray XC systems using the Cray ALPS job scheduler (aprun)

When Intel compilers are being used (usually the default for these systems),
`g++` in `$PATH` must be version 6.4.0 or newer.  If the default is too old,
then you may need to explicitly load a `gcc` environment module, e.g.:

```bash
module load gcc/7.1.0
cd <upcxx-source-path>
./configure --prefix=<upcxx-install-path> --with-cross=cray-aries-slurm
```

If using PrgEnv-cray, then version 9.0 or newer of the Cray compilers is
required.  This means the cce/9.0.0 or later environment module must be
loaded, and not "cce/9.0.0-classic" (the "-classic" Cray compilers are not
supported).

The `configure` script will use the `cc` and `CC` compiler aliases of the Cray
programming environment loaded.  It is *not* necessary to specify these
explicitly using `--with-cc` or `--with-cxx`.

Currently only Intel-based Cray XC systems have been tested, including Xeon
and Xeon Phi (aka "KNL").  Note that UPC++ has not yet been tested on an
ARM-based Cray XC.

After running `configure`, return to
[Step 2: Compiling UPC\+\+](#markdown-header-2-compiling-upc4343), above.

### Configuration: Linux

The `configure` command above will work as-is. The default compilers used will
be gcc/g++. The `--with-cc=...` and `--with-cxx=...` options may specify
alternatives to override this behavior.  Additional options providing finer
control over how UPC\+\+ is configured can be found in the
[Advanced Configuration](#markdown-header-advanced-configuration) section below.

By default ibv-conduit (InfiniBand support) will use MPI for job spawning if a
working `mpicc` is found in your `$PATH` when UPC\+\+ is built.  When this
occurs, one must pass `--with-cxx=mpicxx` (or similar) to `configure` to ensure
correct linkage of ibv-conduit executables.  It is then important that GASNet's
MPI support use a corresponding/compatible `mpicc` and `mpirun`.  In the common
case, the un-prefixed `mpicc` and `mpirun` in `$PATH` are compatible (ie. same
vendor/version/ABI) with the provided `--with-cxx=mpicxx`, in which case
nothing more should be required.  Otherwise, one may need to additionally pass
options like `--with-mpi-cc='/path/to/compatible/mpicc -options'` and/or
`--with-mpirun-cmd='/path/to/compatible/mpirun -np %N %C'`.  Please see
GASNet's mpi-conduit documentation for details.  Alternatively, one may pass
`--disable-mpi-compat` to disable support for MPI as a job spawner, eliminating
the need to use an MPI C\+\+ compiler.

After running `configure`, return to
[Step 2: Compiling UPC\+\+](#markdown-header-2-compiling-upc4343), above.

### Configuration: Apple macOS

On macOS, UPC++ defaults to using the Apple LLVM clang compiler that is part
of the Xcode Command Line Tools.

The Xcode Command Line Tools need to be installed *before* invoking `configure`,
i.e.:

```bash
xcode-select --install
```

Alternatively, the `--with-cc=...` and `--with-cxx=...` options to `configure`
may be used to specify different compilers.

In order to use a debugger on macOS, we advise you to enable "Developer
Mode".  This is a system setting, not directly related to UPC\+\+.
Developer Mode may already be enabled, for instance if one granted Xcode
permission when it asked to enable it.  If not, then an Administrator must
run `DevToolsSecurity -enable` in Terminal.  This mode allows *all* users to
use development tools, including the `lldb` debugger.  If that is not
desirable, then use of debuggers will be limited to members of the
`_developer` group.  An internet search for `macos _developer group` will
provide additional information.

After running `configure`, return to
[Step 2: Compiling UPC\+\+](#markdown-header-2-compiling-upc4343), above.

### Configuration: CUDA GPU support

UPC++ now includes *prototype* support for communication operations on memory buffers
resident in a CUDA-compatible NVIDIA GPU. 
Note the CUDA support in this UPC++ release is a proof-of-concept reference implementation
which has not been tuned for performance. In particular, the current implementation of
`upcxx::copy` does not utilize hardware offload and is expected to underperform 
relative to solutions using RDMA, GPUDirect and similar technologies.
Performance will improve in an upcoming release.

System Requirements:

* NVIDIA-branded [CUDA-compatible GPU hardware](https://developer.nvidia.com/cuda-gpus)
* NVIDIA CUDA toolkit v9.0 or later. Available for [download here](https://developer.nvidia.com/cuda-downloads).

To activate the UPC++ support for CUDA, pass `--with-cuda` to the `configure`
script:

```bash
cd <upcxx-source-path>
./configure --prefix=<upcxx-install-path> --with-cuda
```

This expects to find the NVIDIA `nvcc` compiler wrapper in your `$PATH` and
will attempt to extract the correct build settings for your system.  If this
automatic extraction fails (resulting in preprocessor or linker errors
mentioning CUDA), then you may need to manually override the following
options to `configure`:

* `--with-nvcc=...`: the full path to the `nvcc` compiler wrapper from the CUDA toolkit. 
   Eg `--with-nvcc=/Developer/NVIDIA/CUDA-10.0/bin/nvcc`
* `--with-cuda-cppflags=...`: preprocessor flags to add for locating the CUDA toolkit headers.
   Eg `--with-cuda-cppflags='-I/Developer/NVIDIA/CUDA-10.0/include'`
* `--with-cuda-libflags=...`: linker flags to use for linking CUDA executables.
   Eg `--with-cuda-libflags='-Xlinker -force_load -Xlinker /Developer/NVIDIA/CUDA-10.0/lib/libcudart_static.a -L/Developer/NVIDIA/CUDA-10.0/lib -lcudadevrt -Xlinker -rpath -Xlinker /usr/local/cuda/lib -Xlinker -framework -Xlinker CoreFoundation -framework CUDA'`

Note that you must build UPC++ with the same host compiler toolchain as is used
by `nvcc` when compiling any UPC++ CUDA programs. That is, both UPC++ and your
UPC++ application must be compiled using the same host compiler toolchain.
You can ensure this is the case by either (1) configuring UPC++ with the same
compiler as your system nvcc uses, or (2) using the `-ccbin` command line
argument to `nvcc` during application compilation to ensure it uses the same host
compiler as was passed to the UPC++ `configure` script.
   
UPC++ CUDA operation can be validated using the following programs in the source tree:

* `test/copy.cpp`: correctness tester for the UPC++ `cuda_device`
* `bench/cuda_microbenchmark.cpp`: performance microbenchmark for `upcxx::copy` using GPU memory
* `example/cuda_vecadd`: demonstration of using UPC++ `cuda_device` to orchestrate
  communication for a program invoking CUDA computational kernels on the GPU.

See the "Memory Kinds" section in the _UPC++ Programmer's Guide_ for more details on 
using the CUDA support.

After running `configure`, return to
[Step 2: Compiling UPC\+\+](#markdown-header-2-compiling-upc4343), above.

## Advanced Configuration

The `configure` script tries to pick sensible defaults for the platform it is
running on, but its behavior can be controlled using the following command line
options:

* `--prefix=...`: The location at which UPC\+\+ is to be installed.  The
  default is `/usr/local/upcxx`.
* `--with-cc=...` and `--with-cxx=...`: The C and C\+\+ compilers to use.
* `--with-cross=...`: The cross-configure settings script to pull from the
  GASNet-EX source tree (`<gasnet>/other/contrib/cross-configure-${VALUE}`).
* `--without-cross`: Disable automatic cross-compilation, for instance to
  compile for the front-end of a Cray XC system.
* `--with-default-network=...`: Sets the default network to be used by the
  `upcxx` compiler wrapper.  Valid values are listed under "UPC\+\+ Backends" in
  [README.md](README.md).  The default is `aries` when cross-compiling for a
  Cray XC, and (currently) `smp` for all other systems.  Users with high-speed
  networks, such as InfiniBand (`ibv`), are encouraged to set this parameter
  to a value appropriate for their system.
* `--with-gasnet=...`: Provides the GASNet-EX source tree from which UPC\+\+
  will configure and build its own copies of GASNet-EX. This can be a path to a
  tarball, URL to a tarball, or path to a full source tree. If provided, this
  must correspond to a recent and compatible version of GASNet-EX (NOT GASNet-1).
  Defaults to an embedded copy of GASNet-EX, or the GASNet-EX download URL.
* `--with-gmake=...`: GNU Make command to use; must be 3.80 or newer.  The
  default behavior is to search `$PATH` for a `make` or `gmake` which meets this
  minimum version requirement.
* `--with-python=...`: Python interpreter to use; must be Python3 or Python2
  version 2.7.5 or newer.  The default behavior is to search `$PATH` for a
  suitable interpreter when `upcxx-run` is executed.  This option results in the
  use of a full path to the Python interpreter in `upcxx-run`.
* Options for control of (optional) CUDA support are documented in the section
  [Configuration: CUDA GPU support](#markdown-header-configuration-cuda-gpu-support)
* Options not recognized by the UPC\+\+ `configure` script will be passed to
  the GASNet-EX `configure`.  For instance, `--with-mpirun-cmd=...` might be
  required to setup MPI-based launch of ibv-conduit applications.  Please read
  the GASNet-EX documentation for more information on this and many other
  options available to configure GASNet-EX.  Additionally, passing the option
  `--help=recursive` to the UPC\+\+ configure script will produce GASNet-EX's
  configure help message.

In addition to these explicit configure options, there are several environment
variables which can implicitly affect the configuration of GASNet-EX.  The most
common of these are listed at the end of the output of `configure --help`.

