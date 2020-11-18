# Makefile fragment for cached "make exe" behavior

export SHELL = $(UPCXX_BASH)

# Variables for more readable rules
self       = $(upcxx_src)/bld/Makefile-exe.mak
target     = $(UPCXX_EXE)
depfile    = $(basename $(UPCXX_EXE)).d
library    = $(UPCXX_DIR)/lib/libupcxx.a
upcxx_meta = $(UPCXX_DIR)/bin/upcxx-meta

# Sequenced build (depfile then target) is simplest way to handle dependency tracking
# The use of `-s` prevents "[long full path] is up to date." messages.
default: force
	@$(MAKE) -s -f $(self) $(depfile)
	@$(MAKE) -s -f $(self) $(target)

force:

include $(upcxx_src)/bld/compiler.mak

.PHONY: default force

ifneq ($(wildcard $(depfile)),)
include $(depfile)
endif

$(depfile): $(SRC) $(library)
	@source $(upcxx_meta) SET; \
	 case '$(SRC)' in \
	 *.c) eval "$(call UPCXX_DEP_GEN,$$CC  $$CFLAGS   $$CPPFLAGS,$(target),$(depfile),$(SRC),$(EXTRAFLAGS)) > $(depfile)";; \
	 *)   eval "$(call UPCXX_DEP_GEN,$$CXX $$CXXFLAGS $$CPPFLAGS,$(target),$(depfile),$(SRC),$(EXTRAFLAGS)) > $(depfile)";; \
	 esac

$(target): $(SRC) $(library) $(depfile)
	@source $(upcxx_meta) SET; \
	 case '$(SRC)' in \
	 *.c) cmd="$$CC  $$CCFLAGS  $$CPPFLAGS $$LDFLAGS $(SRC) $$LIBS -o $(target) $(EXTRAFLAGS)";; \
	 *)   cmd="$$CXX $$CXXFLAGS $$CPPFLAGS $$LDFLAGS $(SRC) $$LIBS -o $(target) $(EXTRAFLAGS)";; \
	 esac; \
	 echo "$$cmd"; eval "$$cmd"
