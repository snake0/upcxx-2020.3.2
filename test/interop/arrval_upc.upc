// arrval_upc
//
// This UPC program demonstrates passing and use of UPC++ shared objects across
// the layer boundary. It calls into UPC++ to allocate objects on the shared heap, 
// then creates pointer-to-shared's referencing those objects and performs RMA on them.
// Pointers to shared objects always cross the layer boundary as a raw C local 
// pointer (in this case (val_t *)), but can then be upcast to (shared val_t *),
// passed to other ranks and used just like any other pointer-to-shared.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <upc.h>
#include <upc_castable.h>

#include "test.h"

#define CHECK(lp, base, sz) do { \
 if (arrval_check(lp, base, sz)) ERROR("failed arrval_check(" #lp "=%p, " \
                                       #base "=0x%04x, " #sz "=%lu)", \
                                       (lp), (int)(base), (unsigned long)(sz)); \
} while (0)

static char _gp_str[3][120];
#define DUMP(idx,gp) (bupc_dump_shared((gp), &_gp_str[idx][0], sizeof(_gp_str[idx])), &_gp_str[idx][0])

void free_(val_t *ptr) { free(ptr); }

// Usage: arrval <num_iters> <max_chunk>
// Peak shared memory utilization is approximately: (4 * max_chunk * num_iters)
int main(int argc, char **argv) {

  test_upcxx_init();

  int iters = 0;
  size_t maxchunk = 0;

  if (argc > 1) {
    iters = atoi(argv[1]);
    argc--; argv++;
  }
  if (!iters) iters = 100;
  if (argc > 1) {
    maxchunk = (size_t)atol(argv[1]);
    argc--; argv++;
  }
  if (!maxchunk) maxchunk = 16*1024;

  if (!MYTHREAD) printf("Running arrval_upc num_iters=%i max_chunk=%lu\n", iters, (unsigned long)maxchunk);

  upc_barrier;

  int gpeer = (MYTHREAD + 1) % THREADS;
  int lpeer = -1; // choose a local peer
  for (int i=1; i <= THREADS; i++) {
    int peer = (MYTHREAD + i) % THREADS;
    upc_thread_info_t ti = upc_thread_info(peer);
    if (ti.guaranteedCastable & UPC_CASTABLE_ALLOC) {
      lpeer = peer;
      break;
    }
  }
  assert(lpeer >= 0 && lpeer < THREADS);
  char msg[80];
  snprintf(msg,sizeof(msg),"rank %i/%i: sending to gpeer=%i lpeer=%i\n", 
    (int)MYTHREAD, (int)THREADS, gpeer, lpeer);
  fputs(msg,stdout); fflush(stdout);

  upc_barrier;

  val_t **bufs = malloc(sizeof(val_t *)*iters);
  for (size_t sz = 1; sz <= maxchunk; sz*=2) {
    size_t bsz = sz * sizeof(val_t);
    for (int i=0; i < iters; i++) {
      // create an object from the opposite model heap and initialize it
      val_t *lp = construct_arr_upcxx(sz);
      assert(lp);
      bufs[i] = lp;
      val_t base = BASEVAL(MYTHREAD, i);
      arrval_set(lp, base, sz);
      CHECK(lp, base, sz);

      // check upcast and downcasts
      shared [] val_t *gp = bupc_inverse_cast(lp);
      if (!gp) ERROR("Failed to bupc_inverse_cast(%p)", lp);
      if (upc_threadof(gp) != MYTHREAD) ERROR("upc_threadof(%s)=%i",DUMP(1,gp),(int)upc_threadof(gp));
      val_t *lp2 = (val_t *)gp;
      if (lp != lp2) ERROR("lp=%p != lp2=%p",lp,lp2);

      static shared [] val_t * shared [1] ptrs[THREADS];
      ptrs[MYTHREAD] = gp;
      upc_barrier;

      // exchange pointers around local team and test usability
      shared [] val_t *lgp = ptrs[lpeer];
      val_t *llp = upc_cast(lgp);
      if (!llp) ERROR("failed upc_cast(%s)", DUMP(1,lgp));
      int where = upc_threadof(lgp);
      if (where != lpeer) ERROR("upc_threadof(lgp:%s)=%i expect=%i", DUMP(1,lgp), where, lpeer);
      shared [] val_t *cgp = bupc_inverse_cast(llp);
      if (cgp != lgp) ERROR("cgp:%s != lgp:%s",DUMP(1,cgp),DUMP(2,lgp));
      val_t base_lpeer = BASEVAL(lpeer, i);
      CHECK(llp, base_lpeer, sz);

      // create a temporary local buffer using various methods
      val_t *tmp = NULL;
      void (*tmpfree)(val_t *) = NULL;
      switch (i % 3) {
        case 0: 
          tmp = malloc(bsz);
          tmpfree = free_;
        break;
        case 1: 
          tmp = construct_arr_upc(sz);
          tmpfree = destruct_arr_upc;
        break;
        case 2: 
          tmp = construct_arr_upcxx(sz);
          tmpfree = destruct_arr_upcxx;
        break;
      }
      assert(tmp && tmpfree);

      // test local RMA
      arrval_set(tmp, (val_t)-1, sz);
      upc_memget(tmp, lgp, bsz); 
      CHECK(tmp, base_lpeer, sz);
      arrval_set(tmp, base, sz);
      upc_memput(lgp, tmp, bsz);
      CHECK(llp, base, sz);
      arrval_set(tmp, base_lpeer, sz);
      upc_memput(lgp, tmp, bsz);
      CHECK(llp, base_lpeer, sz);

      upc_barrier;

      // exchange pointers around world and test usability
      shared [] val_t *ggp = ptrs[gpeer];
      int gwhere = upc_threadof(ggp);
      if (gwhere != gpeer) ERROR("upc_threadof(ggp:%s)=%i expect=%i",DUMP(1,ggp),gwhere,gpeer);
      val_t base_gpeer = BASEVAL(gpeer, i);

      // test global RMA
      arrval_set(tmp, (val_t)-1, sz);
      upc_memget(tmp, ggp, bsz);
      CHECK(tmp, base_gpeer, sz);
      arrval_set(tmp, base, sz);
      upc_memput(ggp, tmp, bsz);
      arrval_set(tmp, (val_t)-1, sz);
      upc_memget(tmp, ggp, bsz);
      CHECK(tmp, base, sz);
      arrval_set(tmp, base_gpeer, sz);
      upc_memput(ggp, tmp, bsz);
      arrval_set(tmp, (val_t)-1, sz);
      upc_memget(tmp, ggp, bsz);
      CHECK(tmp, base_gpeer, sz);

      tmpfree(tmp);

      upc_barrier;

      CHECK(lp, base, sz);
    }
    // cleanup
    for (int i=0; i < iters; i++) {
      val_t *lp = bufs[i];
      assert(lp);
      val_t base = BASEVAL(MYTHREAD, i);
      CHECK(lp, base, sz);
      destruct_arr_upcxx(lp);
    }
    upc_barrier;
  }
  free(bufs);

  test_upcxx_finalize();

  if (!MYTHREAD) printf("SUCCESS\n");
  return 0;
}
