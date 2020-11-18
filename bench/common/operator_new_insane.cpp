#include <cstdlib> // posix_memalign
#include <cstdint>

namespace opnew_insane {
  struct frobj {
    frobj *next;
  };
  
  frobj *free_bin[256] = {};
  
  constexpr int bin_of_size(std::size_t size) {
    return size==0 ? 0 : (size-1)/32;
  }
  constexpr std::size_t size_of_bin(int bin) {
    return 32*(bin + 1);
  }
  
  void opnew_populate(int bin) {
    std::size_t size = size_of_bin(bin);
    
    void *p;
    if(posix_memalign(&p, 128<<10, 128<<10))
      /*ignore*/;
    
    char *blob_beg = (char*)p;
    char *blob_end = blob_beg + (128<<10);
    
    *(int*)blob_beg = bin;
    blob_beg += 64;
    
    frobj *o_prev = nullptr;
    while(blob_beg + size <= blob_end) {
      frobj *o = (frobj*)blob_beg;
      blob_beg += size;
      o->next = o_prev;
      o_prev = o;
    }
    
    free_bin[bin] = o_prev;
  }
  
  void* opnew(std::size_t size) {
    int bin = bin_of_size(size);
    
    if(free_bin[bin] == nullptr)
      opnew_populate(bin);
    
    frobj *o = free_bin[bin];
    free_bin[bin] = o->next;
    return o;
  }
  
  void opdel(void *p) {
    frobj *o = (frobj*)p;
    int bin = *reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(p) & -(128<<10));
    o->next = free_bin[bin];
    free_bin[bin] = o;
  }
}

void* operator new(std::size_t size) {
  return opnew_insane::opnew(size);
}

void operator delete(void *p) {
  opnew_insane::opdel(p);
}
