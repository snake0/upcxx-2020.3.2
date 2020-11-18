// This micro-benchmark measures the cost of selected library operations, 
// focusing on CPU overheads.
//
// Usage: a.out (iterations)

#include <upcxx/upcxx.hpp>
#include <gasnetex.h>
#include <gasnet_tools.h>
#include <memory>
#include <iostream>
#include <sstream>
#include <vector>

gasnett_tick_t ticktime(void) { return gasnett_ticks_now(); }
uint64_t tickcvt(gasnett_tick_t ticks) { return gasnett_ticks_to_ns(ticks); }
static int accuracy = 6;
void report(const char *desc, int64_t totaltime, int iters) __attribute__((noinline));
void report(const char *desc, int64_t totaltime, int iters) {
  if (!upcxx::rank_me()) {
      char format[80];
      snprintf(format, sizeof(format), "%i: %%-60s: %%%i.%if s  %%%i.%if us\n",
              upcxx::rank_me(), (4+accuracy), accuracy, (4+accuracy), accuracy);
      printf(format, desc, totaltime/1.0E9, (totaltime/1000.0)/iters);
      fflush(stdout);
  }
}

extern int volatile ctr;
int volatile ctr = 0;
void direct_inc(void) __attribute__((noinline));
void direct_inc(void) {
  ctr = 1 + ctr; // C++20 deprecates ++ and += on volatile
}

void noop0(void) {
}

void noop8(int d1, int d2, int d3, int d4, int d5, int d6, int d7, int d8) {
}

void doit() __attribute__((noinline));
void doit1() __attribute__((noinline));
void doit2() __attribute__((noinline));
void doit3() __attribute__((noinline));

#define TIME_OPERATION_FULL(desc, preop, op, postop) do {  \
    int i, _iters = iters, _warmupiters = MAX(1,iters/10); \
    gasnett_tick_t start,end;  /* use ticks interface */   \
    upcxx::barrier();          /* for best accuracy */     \
    preop;       /* warm-up */                             \
    for (i=0; i < _warmupiters; i++) { op; }               \
    postop;                                                \
    upcxx::barrier();                                      \
    start = ticktime();                                    \
    preop;                                                 \
    for (i=0; i < _iters; i++) { op; }                     \
    postop;                                                \
    end = ticktime();                                      \
    upcxx::barrier();                                      \
    if (((const char *)(desc)) && ((char*)(desc))[0])      \
      report((desc), tickcvt(end - start), iters);         \
    else report(#op, tickcvt(end - start), iters);         \
  } while (0)
#define TIME_OPERATION(desc, op) TIME_OPERATION_FULL(desc, {}, op, {})
#define TIME_NOOP() for (int i=0;i<3;i++) upcxx::barrier()
#define TIME_SINGLE(desc,op) do {                          \
    if (upcxx::rank_me()) TIME_NOOP();                     \
    else TIME_OPERATION(desc,op);                          \
  } while (0)

int nranks, self, peer;
uint64_t iters = 10000;

int main(int argc, char **argv) {
  if (argc > 1) iters = atol(argv[1]);
  upcxx::init();

  nranks = upcxx::rank_n();
  self = upcxx::rank_me();
  // cross-machine symmetric pairing
  if (nranks % 2 == 1 && self == nranks - 1) peer = self;
  else if (self < nranks/2) peer = self + nranks/2;
  else peer = self - nranks/2;
  std::stringstream ss;

  ss << self << "/" << nranks << " : " << gasnett_gethostname() << " : peer=" << peer << "\n";
  std::cout << ss.str() << std::flush;

  upcxx::barrier();

  if (!upcxx::rank_me()) {
      printf("Running misc performance test with %llu iterations...\n",(unsigned long long)iters);
      printf("%-60s    Total time    Avg. time\n"
             "%-60s    ----------    ---------\n", "", "");
      fflush(stdout);
  }
    if (!upcxx::rank_me()) std::cout << "\n Serial overhead tests:" << std::endl;

  doit();

  upcxx::barrier();
  if (!upcxx::rank_me()) std::cout << "SUCCESS" << std::endl;

  upcxx::finalize();

  return 0;
}
// artificially split up into separate functions to get better default behavior from the optimizer
void doit() {
    TIME_OPERATION("measurement overhead",{ ctr = 1 + ctr; }); // C++20 deprecates ++ and += on volatile
    TIME_OPERATION("direct function call",direct_inc());

    upcxx::global_ptr<char> gpb1 = upcxx::new_array<char>(4096);
    upcxx::global_ptr<char> gpb2 = upcxx::new_array<char>(4096);
    char *buf1 = gpb1.local();
    char *buf2 = gpb2.local();
    TIME_OPERATION("memcpy(4KB)",std::memcpy(buf1, buf2, 4096));
    upcxx::delete_array(gpb1);
    upcxx::delete_array(gpb2);

    doit1();
}
void doit1() {
    if (!upcxx::rank_me()) std::cout << "\n Local UPC++ tests:" << std::endl;
    TIME_OPERATION("upcxx::progress",upcxx::progress());

    upcxx::persona &selfp = upcxx::current_persona();
    TIME_OPERATION("self.lpc(noop0)",selfp.lpc(&noop0).wait());
    TIME_OPERATION("self.lpc(lamb0)",selfp.lpc([](){}).wait());

    TIME_OPERATION("upcxx::rpc(self,noop0)",upcxx::rpc(self,&noop0).wait());
    TIME_OPERATION("upcxx::rpc(self,noop8)",upcxx::rpc(self,&noop8,0,0,0,0,0,0,0,0).wait());
    TIME_OPERATION("upcxx::rpc(self,lamb0)",upcxx::rpc(self,[](){}).wait());
    TIME_OPERATION("upcxx::rpc(self,lamb8)",upcxx::rpc(self,
        [](int d1, int d2, int d3, int d4, int d5, int d6, int d7, int d8){},
        0,0,0,0,0,0,0,0).wait());

    doit2();
}
upcxx::global_ptr<double> gp;
upcxx::global_ptr<double> gp_peer;
upcxx::global_ptr<std::int64_t> gpi64;
upcxx::global_ptr<std::int64_t> gpi64_peer;
void doit2() {
    upcxx::dist_object<upcxx::global_ptr<double>> dod(upcxx::new_<double>(0));
    gp = *dod;
    gp_peer = dod.fetch(peer).wait();
    TIME_OPERATION("upcxx::rput<double>(self)",upcxx::rput(0.,gp).wait());

    upcxx::dist_object<upcxx::global_ptr<std::int64_t>> doi(upcxx::new_<std::int64_t>(0));
    gpi64 = *doi;
    gpi64_peer = doi.fetch(peer).wait();

    using upcxx::atomic_op;
    { upcxx::promise<> p;
      upcxx::atomic_domain<std::int64_t> ad_fa({atomic_op::fetch_add, atomic_op::add}, 
                                               upcxx::local_team());
      TIME_OPERATION("atomic_domain<int64>::add(relaxed) loopback overhead", 
                     ad_fa.add(gpi64, 1, std::memory_order_relaxed, upcxx::operation_cx::as_promise(p)));
      p.finalize().wait();
      TIME_OPERATION("atomic_domain<int64>::fetch_add(relaxed) loopback latency", 
                     ad_fa.fetch_add(gpi64, 1, std::memory_order_relaxed).wait());
      ad_fa.destroy();
    }

    doit3();
}
void doit3() {
  if (upcxx::rank_n() > 1) {
    bool all_local = upcxx::reduce_all(upcxx::local_team_contains(peer), upcxx::op_fast_bit_and).wait();
    if (!upcxx::rank_me()) std::cout << "\n Remote UPC++ tests: " 
                                     << (all_local ? "(local_team())" : "(world())") << std::endl;
    TIME_OPERATION("upcxx::rpc(peer,noop0)",upcxx::rpc(peer,&noop0).wait());
    TIME_OPERATION("upcxx::rpc(peer,noop8)",upcxx::rpc(peer,&noop8,0,0,0,0,0,0,0,0).wait());
    TIME_OPERATION("upcxx::rpc(peer,lamb0)",upcxx::rpc(peer,[](){}).wait());
    TIME_OPERATION("upcxx::rpc(peer,lamb8)",upcxx::rpc(peer,
        [](int d1, int d2, int d3, int d4, int d5, int d6, int d7, int d8){},
        0,0,0,0,0,0,0,0).wait());
    TIME_OPERATION("upcxx::rput<double>(peer)",upcxx::rput(0.,gp_peer).wait());

    using upcxx::atomic_op;
    { upcxx::promise<> p;
      upcxx::team &ad_team = (all_local ? upcxx::local_team() : upcxx::world());
      upcxx::atomic_domain<std::int64_t> ad_fa({atomic_op::fetch_add, atomic_op::add}, ad_team);
      TIME_OPERATION("atomic_domain<int64>::add(relaxed) peer overhead", 
                     ad_fa.add(gpi64_peer, 1, std::memory_order_relaxed, upcxx::operation_cx::as_promise(p)));
      p.finalize().wait();
      TIME_OPERATION("atomic_domain<int64>::fetch_add(relaxed) peer latency", 
                     ad_fa.fetch_add(gpi64_peer, 1, std::memory_order_relaxed).wait());
      ad_fa.destroy();
    }
  }
}
