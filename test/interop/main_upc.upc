// Simple UPC program that calls UPC++
#include <assert.h>
#include <stdio.h>

#include <upc.h>

#include "test.h"

shared int A[THREADS];

int main(int argc, char **argv) {
  int val = 42;
  if (argc > 1) val = atoi(argv[1]);

  int peer = (MYTHREAD+1)%THREADS;
  A[MYTHREAD] = val+MYTHREAD;
  upc_barrier;
  int fetch = A[peer];
  assert(fetch == val+peer);

  int v1 = test_upc(val); // call a UPC test routine
  assert(v1 == val);

  int v2 = test_upcxx(val); // call a UPC++ test routine
  assert(v2 == val);

  upc_barrier;
  peer = (MYTHREAD+THREADS-1)%THREADS;
  fetch = A[peer];
  assert(fetch == val+peer);

  if (!MYTHREAD) printf("SUCCESS\n");
  return 0;
}
