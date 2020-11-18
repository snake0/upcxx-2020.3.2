#include <atomic>
#include <iostream>
#include <sstream>
#include <thread>

#include <sched.h>

const int thread_n = 8;

int main() {
  std::atomic<int> setup_bar{0};
  auto thread_fn = [&](int me) {
    std::ostringstream oss;
    oss << "Hello from "<<me<<'\n';
    std::cout << oss.str() << std::flush;
    setup_bar.fetch_add(1);
    while(setup_bar.load(std::memory_order_relaxed) != thread_n)
      sched_yield();
  };

  std::thread* threads[thread_n];
  for(int t=1; t < thread_n; t++)
    threads[t] = new std::thread{thread_fn, t};
  thread_fn(0);

  for(int t=1; t < thread_n; t++) {
    threads[t]->join();
    delete threads[t];
  }

  std::cout << "Done." << std::endl;

  return 0;
}
