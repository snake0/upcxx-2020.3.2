# Debugging

General recommendations for debugging UPC++ programs:

1. Whenever debugging your UPC++ program, **ALWAYS** build in debug mode, 
i.e. compile with `export UPCXX_CODEMODE=debug` (or equivalently, `upcxx -g`).  This enables thousands of
sanity checks system-wide that can greatly accelerate narrowing down the
problem. Just remember to switch back to production mode `UPCXX_CODEMODE=O3` (`upcxx -O`)
for building performance tests!

2. If your problem is a simple enough that a crash stack might help to solve it, 
set `export GASNET_BACKTRACE=1` at run-time (or equivalently, `upcxx-run -backtrace`) and you will get a backtrace from
any rank crash.

    If the problem is a hang (instead of a crash), you can generate an on-demand
    backtrace by setting `export GASNET_BACKTRACE_SIGNAL=USR1` at runtime,
    wait for the hang, and then send that signal to the rank processes you 
    wish to backtrace, ie: `kill -USR1 <pid>`. Note this signal needs to be
    sent to the actual worker process, which may be running remotely on some systems.

3. Otherwise, if the problem occurs with a single rank, you can spawn
smp-conduit jobs in a serial debugger just like any other process.  
E.g., build with `UPCXX_NETWORK=smp` and then start your debugger
with a command like: `env GASNET_PSHM_NODES=1 gdb yourprogram`.

4. Otherwise, if you need multiple ranks and/or a distributed backend, we
recommend setting one or more of the following variables at runtime, and then
following the on-screen instructions to attach a debugger to the failing rank
process(es) and then resume them (by changing a process variable):


    * `GASNET_FREEZE`: set to 1 to make UPC++ always pause and wait for a debugger to attach on startup

    * `GASNET_FREEZE_ON_ERROR`: set to 1 to make UPC++ pause and wait for a
       debugger to attach on any fatal errors or fatal signals

    * `GASNET_FREEZE_SIGNAL`: set to a signal name (e.g. "SIGINT" or "SIGUSR1")
      to specify a signal that will cause the process to freeze and await debugger
      attach.

Note in particular that runs of multi-rank jobs on many systems include
non-trivial spawning activities (e.g., required spawning scripts and/or `fork`
calls) that serial debuggers generally won't correctly follow and handle. Hence
the general recommendation to debug multi-rank jobs by attaching your favorite
debugger to already-running rank processes.

