#include <stdio.h>
#include <upc.h>
#include <assert.h>

#include "test.h"


extern int test_upc(int input) {
  if (!MYTHREAD) printf("test_upc(%i): starting\n", input);
  upc_barrier;
  shared int *sip = upc_all_alloc(THREADS,sizeof(int));
  sip[MYTHREAD] = MYTHREAD;
  upc_barrier;
  int peer = (MYTHREAD+1)%THREADS; 
  int fetch = sip[peer];
  if (fetch != peer) {
    fprintf(stderr,"%i: ERROR bad fetch=%i expected=%i\n",(int)MYTHREAD,fetch, peer);
    abort();
  }
  upc_barrier;
  upc_all_free(sip);
  upc_barrier;
  if (!MYTHREAD) printf("test_upc: ending\n");
  return input;
}

extern void test_upc_barrier() {
  upc_barrier;
}

extern int  test_upc_mythread() {
  return MYTHREAD;
}

extern val_t *construct_arr_upc(size_t cnt) {
  size_t sz = sizeof(val_t) * cnt;
  shared [] val_t * gp = upc_alloc(sz);
  assert(gp);
  assert(upc_threadof(gp) == MYTHREAD);
  val_t *lp = (val_t *)gp;
  assert(lp);
  return lp;
}

extern void destruct_arr_upc(val_t *ptr) {
  assert(ptr);
  shared [] val_t * gp = bupc_inverse_cast(ptr);
  assert(gp);
  assert(upc_threadof(gp) == MYTHREAD);
  upc_free(gp);
}

// these functions are just C code
extern void arrval_set  (val_t *ptr, val_t base, size_t cnt) {
  assert(ptr);
  for (size_t i = 0; i < cnt; i++) {
    val_t val = ARRVAL(base,i);
    ptr[i] = val;
  }
}
extern int arrval_check(val_t *ptr, val_t base, size_t cnt) {
  int errs = 0;
  assert(ptr);
  for (size_t i = 0; i < cnt; i++) {
    val_t expect = ARRVAL(base,i);
    val_t val = ptr[i];
    if (val != expect) {
      char msg[255];
      sprintf(msg,"%i: ERROR in arrval_check() %p[%i]=0x%04x expected=0x%04x\n",
              (int)MYTHREAD, ptr, (int)i, (unsigned)val, (unsigned)expect);
      fputs(msg,stderr);
      errs++;
    }
  }
  return errs;
}


