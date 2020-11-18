#
# Settings for generating `upcxx_config.h` header
#
# NOTE: Please update utils/config/README.md if changing the exported
# environment (UPCXX_CONFIG_VARS or UPCXX_CONFIG_FRAG_VARS).
#

# Scripts (in $(upcxx_src)/utils/config) to be run IN ORDER
UPCXX_CONFIG_SCRIPTS = \
	upcxx_network.sh \
	builtin_assume_aligned.sh \
	hidden_am_concurrency_level.sh

# Environment (UPCXX_* prefixed) to export to configure probe scripts
UPCXX_CONFIG_VARS = \
	UPCXX_TOPSRC='$(upcxx_src)'                   \
	UPCXX_TOPBLD='$(upcxx_bld)'                   \
	UPCXX_GMAKE='$(GMAKE)'                        \
	UPCXX_ASSERT='$(ASSERT)'                      \
	UPCXX_OPTLEV='$(OPTLEV)'                      \
	UPCXX_DBGSYM='$(DBGSYM)'                      \
	UPCXX_NETWORK='$(GASNET_CONDUIT)'             \
	UPCXX_CODEMODE='$(GASNET_CODEMODE)'           \
	UPCXX_THREADMODE='$(GASNET_THRADMODE)'        \
	UPCXX_CROSS='$(CROSS)'

# Make variables (w/o their GASNET_ prefix) to be extracted from the
# GASNet makefile fragment and exported in the environment.
UPCXX_CONFIG_FRAG_VARS = \
	CC  CFLAGS   OPT_CFLAGS   MISC_CFLAGS    \
	CXX CXXFLAGS OPT_CXXFLAGS MISC_CXXFLAGS  \
	CPPFLAGS     MISC_CPPFLAGS               \
	CXXCPPFLAGS  MISC_CXXCPPFLAGS            \
	DEFINES      INCLUDES                    \
	LD LDFLAGS LIBS
