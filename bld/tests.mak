#
# Lists of test files relative to $(top_srcdir)/test
#

testprograms_seq = \
	hello_upcxx.cpp \
	atomics.cpp \
	collectives.cpp \
	dist_object.cpp \
	local_team.cpp \
	barrier.cpp \
	rpc_barrier.cpp \
	rpc_ff_ring.cpp \
	rput.cpp \
	vis.cpp \
	vis_stress.cpp \
	uts/uts_ranks.cpp

testprograms_par = \
	rput_thread.cpp \
	uts/uts_hybrid.cpp \
	view.cpp
