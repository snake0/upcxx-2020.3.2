/*  bupc_tentative.h
 *
 *  Header file for C programs that want to interoperate with UPC code.
 *  This header exposes a subset of the bupc_extern.h entry points as 
 *  tentative definitions, allowing C programs to access them conditionally
 *  upon linkage with libupcr. The basic idea is when libupcr is linked,
 *  all the function pointers in this header will reference the bupc_extern API,
 *  and otherwise they will all be NULL.
 *  See bupc_extern.h for usage documentation on the corresponding entry points.
 *
 *  The mechanisms in this header file are not part of the UPC standard, and
 *  are extensions particular to the Berkeley UPC system.  They should be
 *  considered to have 'experimental' status, and may be changed.
 *
 *  The authoritative version of this header lives here:
 *  $Source: bitbucket.org:berkeleylab/upc-runtime.git/bupc_tentative.h $
 *  Clients may embed and distribute this header, but should NOT modify
 *  any of the definitions.
 *
 *  This interface is available starting in UPCR v2018.5.3, runtime spec v3.13.
 *  
 *  Version history: (BUPC_TENTATIVE_VERSION_{MAJOR,MINOR})
 *  ---------------
 *  1.3 : UPCR v2019.4.6, runtime spec v3.14
 *    - Added BUPC_TENTATIVE_SPECIFIER for finer-grained compilation control
        and to ensure correct behavior with -fno-common (default for gcc 10+)
 *  1.2 : UPCR v2019.4.5, runtime spec v3.14
 *    - bupc_tentative_free now accepts thread=-1 for "unknown local thread"
 *    - Added bupc_tentative_process_thread_layout
 *    - Added bupc_tentative_threadof
 *  1.1 : UPCR v2018.5.3, runtime spec v3.13
 *    - Initial revision
 */

#ifndef __BUPC_TENTATIVE_H
#define __BUPC_TENTATIVE_H

#ifdef __cplusplus
#error This header is not compatible with C++.
/* C++ does not support tentative definitions, which are a required mechanism for this header to work as intended.
 * C++ clients should use this header from a C file linked into their system.
 */
#endif

#ifndef UPCRI_HAS_ATTRIBUTE
  #if defined(__has_attribute) // introduced around gcc 5.x (2015)
    #define UPCRI_HAS_ATTRIBUTE(x) __has_attribute(x)
  #else
    #define UPCRI_HAS_ATTRIBUTE(x) 0
  #endif
#endif

#ifndef BUPC_TENTATIVE_SPECIFIER
  #if UPCRI_HAS_ATTRIBUTE(__common__)
    // This attribute is notably available in or before:
    // gcc 4.8.5, clang 4.0.0, Intel 2017.0, PGI 18.4, Xcode 8.2.1
    #define BUPC_TENTATIVE_SPECIFIER __attribute__((__common__))
  #else
    #define BUPC_TENTATIVE_SPECIFIER // empty - be conservative
  #endif
#endif

// monotonic version tracking for the symbols below
// guaranteed to be non-zero
#define BUPC_TENTATIVE_VERSION_MAJOR 1
#define BUPC_TENTATIVE_VERSION_MINOR 3

/* These are INTENTIONALLY TENTATIVE definitions.
 * do NOT add extern/static storage qualifiers or initializers to any of these!!
 */

BUPC_TENTATIVE_SPECIFIER
int bupc_tentative_version_major;
BUPC_TENTATIVE_SPECIFIER
int bupc_tentative_version_minor;

BUPC_TENTATIVE_SPECIFIER
void (*bupc_tentative_init)(int *argc, char ***argv); 

BUPC_TENTATIVE_SPECIFIER
void (*bupc_tentative_init_reentrant)(int *argc, char ***argv, 
			 int (*pmain_func)(int, char **) ); 

BUPC_TENTATIVE_SPECIFIER
void (*bupc_tentative_exit)(int exitcode);

BUPC_TENTATIVE_SPECIFIER
int (*bupc_tentative_mythread)(void);
BUPC_TENTATIVE_SPECIFIER
int (*bupc_tentative_threads)(void);

BUPC_TENTATIVE_SPECIFIER
char * (*bupc_tentative_getenv)(const char *env_name);

BUPC_TENTATIVE_SPECIFIER
void (*bupc_tentative_notify)(int barrier_id);
BUPC_TENTATIVE_SPECIFIER
void (*bupc_tentative_wait)(int barrier_id);
BUPC_TENTATIVE_SPECIFIER
void (*bupc_tentative_barrier)(int barrier_id);

BUPC_TENTATIVE_SPECIFIER
void * (*bupc_tentative_alloc)(size_t bytes);
BUPC_TENTATIVE_SPECIFIER
void * (*bupc_tentative_all_alloc)(size_t nblocks, size_t blocksz);
BUPC_TENTATIVE_SPECIFIER
void (*bupc_tentative_free)(void *ptr, int thread);
BUPC_TENTATIVE_SPECIFIER
void (*bupc_tentative_all_free)(void *ptr, int thread);

BUPC_TENTATIVE_SPECIFIER
int  (*bupc_tentative_threadof)(void *ptr);

BUPC_TENTATIVE_SPECIFIER
void (*bupc_tentative_process_thread_layout)(int *upcthreads_in_process,
                                             int *first_upcthread_in_process);

BUPC_TENTATIVE_SPECIFIER
void (*bupc_tentative_config_info)(const char **upcr_config_str,
                                   const char **gasnet_config_str,
                                   const char **upcr_version_str,
                                   int    *upcr_runtime_spec_major,
                                   int    *upcr_runtime_spec_minor,
                                   int    *upcr_debug,
                                   int    *upcr_pthreads,
                                   size_t *upcr_pagesize);

#endif /* __BUPC_TENTATIVE_H */
