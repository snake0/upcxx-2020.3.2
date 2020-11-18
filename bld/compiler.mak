#
# Compiler-specific settings
#
# The following are available (among others)
#   GASNET_{CC,CXX}
#   GASNET_{CC,CXX}_FAMILY
#   GASNET_{CC,CXX}_SUBFAMILY
#   GASNET_{C,CXX}FLAGS
#
# NOTE: sufficiently old GNU Make lacks 'else if*'

#
# UPCXX_STDCXX
# This is the C++ language level option appropriate to the compiler
#
# First the default:
UPCXX_STDCXX := -std=c++11
# Then compiler-specific overrides:
ifeq ($(GASNET_CXX_FAMILY),Intel)
UPCXX_STDCXX := -std=c++14
endif
# Then throw it all away if $CXX already specifies a language level:
ifneq ($(findstring -std=c++,$(GASNET_CXX))$(findstring -std=gnu++,$(GASNET_CXX)),)
UPCXX_STDCXX :=
endif

#
# LIBUPCXX_CFLAGS
# Any CFLAGS specific to compilation of objects in libupcxx
#
LIBUPCXX_CFLAGS := -Wall

# PGI:
# + strip unsupported `-Wall`
ifeq ($(GASNET_CC_FAMILY),PGI)
LIBUPCXX_CFLAGS := $(filter-out -Wall,$(LIBUPCXX_CFLAGS))
endif

# Deal w/ (unsupported) nvcc -Xcompiler mess
ifeq ($(GASNET_CC_SUBFAMILY),NVIDIA)
LIBUPCXX_CFLAGS := $(patsubst %,-Xcompiler %,$(LIBUPCXX_CFLAGS))
endif

#
# LIBUPCXX_CXXFLAGS
# Any CXXFLAGS specific to compilation of objects in libupcxx
#
LIBUPCXX_CXXFLAGS := -Wall

# PGI:
# + strip unsupported `-Wall`
# + address issue #286 (bogus warning on future/core.cpp)
ifeq ($(GASNET_CXX_FAMILY),PGI)
LIBUPCXX_CXXFLAGS := $(filter-out -Wall,$(LIBUPCXX_CXXFLAGS)) --diag_suppress1427
endif

# Intel:
# + address issue #286 (bogus warning on future/core.cpp)  -Wextra only
ifeq ($(GASNET_CXX_FAMILY),Intel)
ifneq ($(findstring -Wextra,$(GASNET_CXX) $(GASNET_CXXFLAGS)),)
LIBUPCXX_CXXFLAGS += -Wno-invalid-offsetof
endif
endif

# Deal w/ (unsupported) nvcc -Xcompiler mess
ifeq ($(GASNET_CXX_SUBFAMILY),NVIDIA)
LIBUPCXX_CXXFLAGS := $(patsubst %,-Xcompiler %,$(LIBUPCXX_CXXFLAGS))
endif

#
# UPCXX_DEP_GEN
# Incantation to generate a dependency file on stdout.
# The resulting output should name both arguments as "targets", dependent on all
# files visited in preprocess.  This ensures both the object (or executable) and
# dependency file are kept up-to-date.
#
# Abstract "prototype":
#   $(call UPCXX_DEP_GEN,COMPILER_AND_FLAGS,TARGET1,TARGET2,SRC,EXTRA_FLAGS)
# Simple example (though general case lacks the common `basename`):
#   $(call UPCXX_DEP_GEN,$(CXX) $(CXXFLAGS),$(basename).o,$(basename).d,$(basename).cpp,$(EXTRA_FLAGS))
#
# Note 1: Generation to stdout is used because PGI compilers ignore `-o foo` in
# the presence `-E` and lack support for `-MF`.  Meanwhile all supported
# compilers send `-E` output to stdout by default.  Furthermore, PGI's handling
# of `-MT target` differs such that we must post-process.
#
# TODO: Options like `-MM` can save time (omitting numerous system headers from
# the list of files to stat when building).  However, it is not currently used
# because `nobs` documents the behavior of `-MM` as broken with PGI.
# Compiler-family could be used to include/exclude such flags here.
#
# Some explanation of the default flags, extracted from gcc man page:
#   -M
#       Instead of outputting the result of preprocessing, output a rule
#       suitable for make describing the dependencies [...]
#   -MT target
#       Change the target of the rule emitted by dependency generation. [...]
#
UPCXX_DEP_GEN = $(1) -E -M -MT '$(2) $(3)' $(4) $(5)
ifeq ($(GASNET_CXX_FAMILY),PGI)
# PGI doesn't handle `-MT 'foo.o foo.d'` the way other compiler families do
# NOTE: we've assumed '@' does not appear in the target paths
UPCXX_DEP_GEN = $(1) -E -M -MT UPCXX_DEP_TARGET $(4) $(5) | sed -e 's@^UPCXX_DEP_TARGET@$(2) $(3)@'
endif
ifeq ($(GASNET_CXX_SUBFAMILY),NVIDIA)
# NOTE: this is known to be insufficient if nvcc's backend is PGI
UPCXX_DEP_GEN = $(1) -E -Xcompiler "-M -MT '$(2) $(3)'" $(4) $(5)
endif
