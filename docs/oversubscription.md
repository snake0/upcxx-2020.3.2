## Oversubscription of UPC++ Ranks to CPUs ##

For performance, it is recommended that UPC++ programs are run such that no two
OS threads executing UPC++ code (in the same process or not) ever contend for
the same CPU concurrently. For single threaded processes, this usually means
ensuring there are at least as many CPUs as there are ranks. The performance
peril of not following this advice is due to the *encouraged* standard practice
of spin-like loops where `upcxx::progress()` is called repeatedly until a user
condition is met (like the readiness of a future, i.e. `future::wait()`).
Should one thread be spin-waiting for communication initiated by another, but
that other thread cannot get access to its CPU because the first is spinning
there, the stalled thread will have to wait until the spinning one is
interrupted by the OS scheduler, and that can take a very long time.

In cases where oversubscription is desired (such as testing with more ranks
than you have CPUs on your laptop), UPC++ can be configured at launch time with
`UPCXX_OVERSUBSCRIBED` in the environment to *play* nice with other threads
on the system by periodically issuing a "yield" syscall to the OS from within
`upcxx::progress()`.

Supported values for `UPCXX_OVERSUBSCRIBED`:

  * `0|n[o]`: Oversubscription is assumed false. `progress()` never yields to OS.

  * `1|y[es]`: Oversubscription assumed true. A yield is issued if N consecutive
    calls to `progress()` detect no incoming communication events (for
    some implementation defined value N, likely 10).

  * `<unset>|<empty>`: A default value is chosen at startup by querying the local
    machine for the number of total CPUs and enabling progress yield if they are
    fewer than the number of ranks running locally.

