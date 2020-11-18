#ifndef __INTEROP_TEST_H
#define __INTEROP_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

// This call is used to init UPCR when main is not in UPC code.
// It's called indirectly by upcxx::init() when needed, but that 
// library lacks access to the real argc/argv of the process.
// For this reason, one may get more portable spawning behavior 
// by calling it explicitly early in main() with the real argc/argv.
extern void bupc_init(int *argc, char ***argv);

// enable UPC code to explicitly control UPC++ state:
extern void test_upcxx_init();
extern void test_upcxx_finalize();

// collectively run a test in UPC/UPC++ and return the given value
extern int test_upc(int input);
extern int test_upcxx(int input);

// convenience helpers for -pthread tests
extern void test_upc_barrier();
extern int  test_upc_mythread();

#undef ERROR
#ifdef __cplusplus
  #define ERROR(stream) do { \
    std::cerr << upcxx::rank_me() << ": ERROR at " \
              << __FILE__ << ":" << __LINE__ << ": " \
              << stream << std::endl; \
    abort(); \
  } while(0)
#else
  static char _msg[1024];
  #define ERROR(...) do { \
    char *_p = _msg; \
    _p += sprintf(_p,"%i: ERROR at %s:%i: ", (int)MYTHREAD, __FILE__, __LINE__); \
    _p += sprintf(_p, __VA_ARGS__); \
    strcpy(_p,"\n"); \
    fputs(_msg, stderr); fflush(stderr); \
    abort(); \
  } while(0)
#endif

// utilities for arrval tests:

typedef uint32_t val_t;
#define BASEVAL(rank,iter) ((((val_t)(rank) << 8) & 0xFFFF) | ((val_t)(iter) & 0xFF))
#define ARRVAL(base,idx) (((val_t)(base) << 16) | ((val_t)(idx) & 0xFFFF))
// set a memory region to known values, and validate those values
extern void arrval_set  (val_t *ptr, val_t base, size_t cnt);
extern int  arrval_check(val_t *ptr, val_t base, size_t cnt);

// construct an uninitialized array in shared memory from UPC or UPC++
extern val_t *construct_arr_upc  (size_t cnt);
extern val_t *construct_arr_upcxx(size_t cnt);

// destruct an object created by construct_arr
extern void destruct_arr_upc  (val_t *ptr);
extern void destruct_arr_upcxx(val_t *ptr);

#ifdef __cplusplus
}
#endif

#endif
