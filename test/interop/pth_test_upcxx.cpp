#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <mutex>
#include <upcxx/upcxx.hpp>

#include "test.h"

// Cross language interop is complicated when UPC is in -pthreads rank mode,
// because UPC++ uses process-based ranks.
// Additional state is used to track the many-to-one rank mapping, and ensure
// library init/fini is correctly performed once per process

static int process_threads = 0; // number of upc threads in this process
static int master_thread = 1<<30; // upc thread w/ the master persona for this process
static std::map<int, upcxx::persona*> process_thread_info;

extern void test_upcxx_init() {
  int mythread = test_upc_mythread();
  if (!mythread) std::cout << "test_upcxx_init(): Starting UPC++ init" << std::endl;

  { static std::mutex m;
    std::lock_guard<std::mutex> lock(m);

    master_thread = std::min(master_thread, mythread); // elect a process master based on upc threadid
    process_threads++;  // count the number of UPC threads in this process
    process_thread_info[mythread] = &upcxx::default_persona(); // save info for later
  }
  test_upc_barrier(); // ensure thread registration complete
  if (mythread == master_thread) {
    upcxx::init(); // init the UPC++ library
    char msg[80];
    snprintf(msg,sizeof(msg),"rank %i/%i: UPC++ init complete, %i UPC threads in this process, master=%i\n", 
      upcxx::rank_me(), upcxx::rank_n(), process_threads, master_thread);
    std::cout << msg << std::flush;
  }
  test_upc_barrier(); // ensure init complete
}

extern void test_upcxx_finalize() {
  test_upc_barrier();
  if (test_upc_mythread() == master_thread) upcxx::finalize();
  test_upc_barrier();
}
extern int test_upcxx(int input) {
  int mythread = test_upc_mythread();

  test_upcxx_init();

  if (mythread == master_thread) {

    if (!upcxx::rank_me()) printf("test_upcxx(%i): starting\n", input);
    upcxx::barrier();

    upcxx::global_ptr<int> gp = upcxx::new_<int>(upcxx::rank_me());
    upcxx::dist_object<upcxx::global_ptr<int>> dobj(gp);

    int peer = (upcxx::rank_me() + 1) % upcxx::rank_n();
    upcxx::global_ptr<int> pgp = dobj.fetch(peer).wait();
    int fetch = rget(pgp).wait();
    if (fetch != peer) ERROR("bad fetch=" << fetch << " expected=" << peer);
    upcxx::barrier();

    char msg[80];
    snprintf(msg,sizeof(msg),"rank %i/%i: local rank %i/%i\n", upcxx::rank_me(), upcxx::rank_n(),
      upcxx::local_team().rank_me(), upcxx::local_team().rank_n());
    std::cout << msg << std::flush;

    int lpeer = ( upcxx::local_team().rank_me() + 1 ) % upcxx::local_team().rank_n();

    upcxx::dist_object<upcxx::global_ptr<int>> dobj_local(upcxx::local_team(), gp);
    upcxx::global_ptr<int> lgp = dobj_local.fetch(lpeer).wait();
    if (!lgp.is_local()) ERROR("failed locality check for " << lgp);
    int *llp = lgp.local();
    int expect = upcxx::local_team()[lpeer];
    int where = lgp.where();
    if (where != expect) ERROR("lgp:" << lgp << " .where()=" << where << " expect=" << expect);
    int lfetch = *llp;
    if (lfetch != expect) ERROR("bad fetch=" << lfetch << " expected=" << expect);

    upcxx::global_ptr<int> cgp = upcxx::try_global_ptr(llp);
    if (cgp != lgp) ERROR("cgp:" << cgp << " != lgp:" << lgp);

    upcxx::barrier();

    upcxx::delete_(gp);

    upcxx::barrier();
    if (!upcxx::rank_me()) std::cout << "test_upcxx: ending" << std::endl;

  } // master thread

  test_upcxx_finalize();

  return input;
}

extern val_t *construct_arr_upcxx(size_t cnt) {
  upcxx::global_ptr<val_t> gp = upcxx::allocate<val_t>(cnt);
  if (!gp) ERROR("failed to upcxx::allocate(" << cnt << ")");
  upcxx::intrank_t where = gp.where();
  if (where != upcxx::rank_me()) ERROR("upcxx::allocate().where() = " << where);
  bool islocal = gp.is_local();
  if (!islocal) ERROR("upcxx::allocate().is_local() = false");
  val_t *lp = gp.local();
  if (!lp) ERROR("upcxx::allocate().local() = null");
  return lp;
}

extern void destruct_arr_upcxx(val_t *ptr) {
  if (!ptr) ERROR("destruct_arr_upcxx(null)");
  upcxx::global_ptr<val_t> gp = upcxx::try_global_ptr(ptr);
  if (!gp) ERROR("failed try_global_ptr(" << ptr << ")");
  upcxx::deallocate(gp); 
}


