#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <upcxx/upcxx.hpp>

#include "test.h"

extern int test_upcxx(int input) {

  upcxx::init();

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

  upcxx::finalize();

  return input;
}

extern void test_upcxx_init() {
  upcxx::init();
}

extern void test_upcxx_finalize() {
  upcxx::finalize();
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


