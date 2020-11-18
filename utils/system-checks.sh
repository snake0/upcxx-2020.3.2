#!/bin/bash

sys_info() {
    # Output information to assist in bug reports
    if test -z "$UPCXX_INSTALL_QUIET" ; then (
        if test -d .git ; then
            echo UPCXX revision: `git describe --always 2> /dev/null`
        fi
        echo System: `uname -a 2>&1`
        /usr/bin/sw_vers 2> /dev/null
        /usr/bin/xcodebuild -version 2> /dev/null 
        /usr/bin/lsb_release -a 2> /dev/null
        echo " "
        echo Date: `date 2>&1`
        echo Current directory: `pwd 2>&1`
        echo Install directory: $install_to
        local SETTINGS=
        for var in CC CXX GASNET GASNET_CONFIGURE_ARGS CROSS OPTLEV DBGSYM \
	           UPCXX_BACKEND GASNET_INSTALL_TO \
		   UPCXX_CODEMODE UPCXX_THREADMODE \
		   UPCXX_CUDA \
		   ; do
            if test "${!var:+set}" = set; then
                SETTINGS="$SETTINGS $var='${!var}'"
            fi
        done
        echo "Settings:$SETTINGS"
        echo " "
        fpy=${UPCXX_PYTHON:-$(type -p python)}
        echo "$fpy: " $($fpy --version 2>&1)
        echo " "
    ) fi
}

# platform_sanity_checks(): defaults $CC and $CXX if they are unset
#   validates the compiler and system versions for compatibility
#   setting UPCXX_INSTALL_NOCHECK=1 disables this function completely
platform_sanity_checks() {
    if test -z "$UPCXX_INSTALL_NOCHECK" ; then
        local KERNEL=`uname -s 2> /dev/null`
        local KERNEL_GOOD=
        if test Linux = "$KERNEL" || test Darwin = "$KERNEL" ; then
            KERNEL_GOOD=1
        fi
        if test -n "$CRAY_PRGENVCRAY" && expr "$CRAY_CC_VERSION" : "^[78]" > /dev/null; then
            echo 'ERROR: UPC++ on Cray XC with PrgEnv-cray requires cce/9.0 or newer.'
            exit 1
        elif test -n "$CRAY_PRGENVCRAY" && expr x"$CRAY_PE_CCE_VARIANT" : "xCC=Classic" > /dev/null; then
            echo 'ERROR: UPC++ on Cray XC with PrgEnv-cray does not support the "-classic" compilers such as' $(grep -o 'cce/[^:]*' <<<$LOADEDMODULES)
            exit 1
        elif test -n "$CRAY_PRGENVPGI" ; then
            echo 'ERROR: UPC++ on Cray XC currently requires PrgEnv-gnu, intel or cray. Please do: `module switch PrgEnv-pgi PrgEnv-FAMILY` for your preferred compiler FAMILY'
            exit 1
        elif test -n "$CRAY_PRGENVINTEL" &&
             g++ --version | head -1 | egrep ' +\([^\)]+\) +([1-5]\.|6\.[0-3])' 2>&1 > /dev/null ; then
            echo 'ERROR: UPC++ on Cray XC with PrgEnv-intel requires g++ version 6.4 or newer in \$PATH. Please do: `module load gcc`, or otherwise ensure a new-enough `g++` is available.'
            exit 1
        elif test -n "$CRAY_PRGENVGNU$CRAY_PRGENVINTEL$CRAY_PRGENVCRAY" && test -n "$UPCXX_CROSS"; then
            CC=${CC:-cc}
            CXX=${CXX:-CC}
        elif test "$KERNEL" = "Darwin" ; then # default to XCode clang
            CC=${CC:-/usr/bin/clang}
            CXX=${CXX:-/usr/bin/clang++}
        else
            CC=${CC:-gcc}
            CXX=${CXX:-g++}
        fi
        local ARCH=`uname -m 2> /dev/null`
        local ARCH_GOOD=
        local ARCH_BAD=
        if test x86_64 = "$ARCH" ; then
            ARCH_GOOD=1
        elif test ppc64le = "$ARCH" ; then
            ARCH_GOOD=1
        elif test aarch64 = "$ARCH" ; then
            ARCH_GOOD=1
            # ARM-based Cray XC not yet tested
            if test -n "$CRAY_PEVERSION" ; then
              ARCH_GOOD=
            fi
        elif expr "$ARCH" : 'i.86' >/dev/null 2>&1 ; then
            ARCH_BAD=1
        fi

        # absify compilers, checking they exist
        cxx_exec=$(type -p ${CXX%% *})
        if [[ -z "$cxx_exec" ]]; then
            echo "ERROR: could not find CXX='${CXX%% *}' in \$PATH"
            exit 1
        fi
        CXX=$cxx_exec${CXX/${CXX%% *}/}
        cc_exec=$(type -p ${CC%% *})
        if [[ -z "$cc_exec" ]]; then
            echo "ERROR: could not find CC='${CC%% *}' in \$PATH"
            exit 1
        fi
        CC=$cc_exec${CC/${CC%% *}/}
        if test -z "$UPCXX_INSTALL_QUIET" ; then
            echo $CXX
            $CXX --version 2>&1 | grep -v 'warning #10315'
            echo $CC
            $CC --version 2>&1 | grep -v 'warning #10315'
            echo " "
        fi

        local CXXVERS=`$CXX --version 2>&1`
        local CCVERS=`$CC --version 2>&1`
        local COMPILER_BAD=
        local COMPILER_GOOD=
        if echo "$CXXVERS" | egrep 'Apple LLVM version [1-7]\.' 2>&1 > /dev/null ; then
            COMPILER_BAD=1
        elif echo "$CXXVERS" | egrep 'Apple LLVM version ([8-9]\.|[1-9][0-9])' 2>&1 > /dev/null ; then
            COMPILER_GOOD=1
        elif echo "$CXXVERS" | egrep 'PGI Compilers and Tools'  > /dev/null ; then
            if [[ "$ARCH,$KERNEL" = 'x86_64,Linux' ]] &&
                 egrep ' +(19|[2-9][0-9])\.[0-9]+-' <<<"$CXXVERS" 2>&1 >/dev/null ; then
               # Ex: "pgc++ 19.7-0 LLVM 64-bit target on x86-64 Linux -tp nehalem"
               # 19.1 and newer "GOOD"
               COMPILER_GOOD=1
            elif [[ "$ARCH,$KERNEL" = 'ppc64le,Linux' ]] &&
                 egrep ' +(18\.10|(19|[2-9][0-9])\.[0-9]+)-' <<<"$CXXVERS" 2>&1 >/dev/null ; then
               # Ex: "pgc++ 18.10-0 linuxpower target on Linuxpower"
               # 18.10 and newer "GOOD" (no 18.x was released for x > 10)
               COMPILER_GOOD=1
            else
               # Unsuported platform or version
               COMPILER_BAD=1
            fi
        elif echo "$CXXVERS" | egrep 'IBM XL'  > /dev/null ; then
            COMPILER_BAD=1
        elif echo "$CXXVERS" | egrep 'Free Software Foundation' 2>&1 > /dev/null &&
	     echo "$CXXVERS" | head -1 | egrep ' +\([^\)]+\) +([1-5]\.|6\.[0-3])' 2>&1 > /dev/null ; then
            COMPILER_BAD=1
        elif echo "$CXXVERS" | egrep ' +\(ICC\) +(17\.0\.[2-9]|1[89]\.|2[0-9]\.)' 2>&1 > /dev/null ; then
	    # Ex: icpc (ICC) 18.0.1 20171018
            COMPILER_GOOD=1
        elif echo "$CXXVERS" | egrep ' +\(ICC\) ' 2>&1 > /dev/null ; then
	    :
        elif echo "$CXXVERS" | egrep 'Free Software Foundation' 2>&1 > /dev/null &&
             echo "$CXXVERS" | head -1 | egrep ' +\([^\)]+\) +([6-9]\.|[1-9][0-9])' 2>&1 > /dev/null ; then
            # Ex: g++ (Ubuntu 5.4.0-6ubuntu1~16.04.4) 5.4.0 20160609
            #     g++-7 (Homebrew GCC 7.2.0) 7.2.0
            #     foo (GCC) 7.2.0
            COMPILER_GOOD=1
            # Arm Ltd's gcc not yet tested
            if test aarch64 = "$ARCH" && echo "$CXXVERS" | head -1 | egrep ' +\(ARM' 2>&1 > /dev/null ; then
              COMPILER_GOOD=
            fi
        elif echo "$CXXVERS" | egrep 'clang version [23]' 2>&1 > /dev/null ; then
            COMPILER_BAD=1
        elif test x86_64 = "$ARCH" && echo "$CXXVERS" | egrep 'clang version ([4-9]\.|[1-9][0-9])' 2>&1 > /dev/null ; then
            COMPILER_GOOD=1
        elif test ppc64le = "$ARCH" && echo "$CXXVERS" | egrep 'clang version ([5-9]\.|[1-9][0-9])' 2>&1 > /dev/null ; then
	    # Issue #236: ppc64le/clang support floor is 5.x. clang-4.x/ppc has correctness issues and is deliberately left "unvalidated"
            COMPILER_GOOD=1
        elif test aarch64 = "$ARCH" && echo "$CXXVERS" | egrep 'clang version ([4-9]\.|[1-9][0-9])' 2>&1 > /dev/null ; then
            COMPILER_GOOD=1
            # Arm Ltd's clang not yet tested
            if echo "$CXXVERS" | egrep '^Arm C' 2>&1 > /dev/null ; then
              COMPILER_GOOD=
            fi
        fi

        local RECOMMEND
        read -r -d '' RECOMMEND<<'EOF'
We recommend one of the following C++ compilers (or any later versions):
           Linux on x86_64:   g++ 6.4.0, LLVM/clang 4.0.0, PGI 19.1, Intel C 17.0.2
           Linux on ppc64le:  g++ 6.4.0, LLVM/clang 5.0.0, PGI 18.10
           Linux on aarch64:  g++ 6.4.0, LLVM/clang 4.0.0
           macOS on x86_64:   g++ 6.4.0, Xcode/clang 8.0.0
           Cray XC systems:   PrgEnv-gnu with gcc/6.4.0 environment module loaded
                              PrgEnv-intel with Intel C 17.0.2 and gcc/6.4.0 environment module loaded
                              PrgEnv-cray with cce/9.0.0 environment module loaded
                              ALCF's PrgEnv-llvm/4.0
EOF
        if test -n "$ARCH_BAD" ; then
            echo "ERROR: This version of UPC++ does not support the '$ARCH' architecture."
            echo "ERROR: $RECOMMEND"
            exit 1
        elif test -n "$COMPILER_BAD" ; then
            echo 'ERROR: Your C++ compiler is known to lack the support needed to build UPC++. '\
                 'Please set $CC and $CXX to point to a newer C/C++ compiler suite.'
            echo "ERROR: $RECOMMEND"
            exit 1
        elif test -z "$COMPILER_GOOD" || test -z "$KERNEL_GOOD" || test -z "$ARCH_GOOD" ; then
            echo 'WARNING: Your C++ compiler or platform has not been validated to run UPC++'
            echo "WARNING: $RECOMMEND"
        fi
    fi
    return 0
}

platform_settings() {
   local KERNEL=`uname -s 2> /dev/null`
   case "$KERNEL" in
     *)
       ;;
   esac
}

