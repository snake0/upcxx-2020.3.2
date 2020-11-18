#include <upcxx/upcxx.hpp>
#include <iostream>

using namespace std;
using namespace upcxx;

int main() {
  upcxx::init();

  std::ostringstream oss1, oss2;
  global_ptr<int> gpi = new_<int>(42);
  global_ptr<char> gpc = new_array<char>(80);
  strcpy(gpc.local(), "hello world");

  cout << gpi << endl;
  cout << gpc << endl;
  oss1 << gpc;
  strcpy(gpc.local(), "uh, oh.");
  oss2 << gpc;
  if (!rank_me()) {
    if (oss1.str() != oss2.str()) {
      cout << "ERROR: '" << oss1.str() << "' != '" << oss2.str() << "'" << endl;
    }
  }
  upcxx::barrier();
  gpc = NULL;
  cout << gpc << endl;
 
  upcxx::barrier();
  if (!rank_me()) cout << "done." << endl;
  upcxx::finalize();
  return 0;
}
