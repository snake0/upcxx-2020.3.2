#ifndef _fbcda049_eb37_438b_9a9c_a2ce81b8b8f5
#define _fbcda049_eb37_438b_9a9c_a2ce81b8b8f5

/**
 * allocate.hpp
 */

#include <upcxx/backend.hpp>
#include <upcxx/diagnostic.hpp>
#include <upcxx/global_ptr.hpp>

#include <algorithm> // max
#include <cmath> // ceil
#include <cstdint>
#include <sstream>
#include <cstddef> // max_align_t
#include <limits> // numeric_limits
#include <new> // bad_alloc
#include <type_traits> // aligned_storage, is_default_constructible,
                       // is_destructible, is_trivially_destructible

namespace upcxx {
  struct bad_shared_alloc : public std::bad_alloc {
    bad_shared_alloc(const char *where=nullptr, size_t nbytes=0) {
      std::stringstream ss;
      ss << _base << "UPC++ shared heap is out of memory on process " << rank_me();
      if (where) ss << "\n inside upcxx::" << where;
      if (nbytes) ss << " while trying to allocate " << nbytes <<  " more bytes";
      ss << "\n " << detail::shared_heap_stats();
      ss << "\n You may need to request a larger shared heap with `upcxx-run -shared-heap`"
               " or $UPCXX_SHARED_HEAP_SIZE.";
      _what = ss.str();
    }
    bad_shared_alloc(const std::string & reason) : _what(_base) {
      _what += reason;
    }
    virtual const char* what() const noexcept {
      return _what.c_str();
    }
    private:
     std::string _what;
     static constexpr const char *_base = "upcxx::bad_shared_alloc: ";
  };
  //////////////////////////////////////////////////////////////////////
  /* Declared in: upcxx/backend_fwd.hpp
  
  void* allocate(std::size_t size,
                 std::size_t alignment = alignof(std::max_align_t));
  
  void deallocate(void* ptr);
  */
  
  //////////////////////////////////////////////////////////////////////
  
  template<typename T>
  global_ptr<T> allocate(std::size_t n = 1, std::size_t alignment = alignof(T)) {
    void *p = upcxx::allocate(n * sizeof(T), alignment);
    return p == nullptr
      ? global_ptr<T>(nullptr)
      : global_ptr<T>(
          detail::internal_only{},
          upcxx::rank_me(),
          reinterpret_cast<T*>(p)
        );
  }

  template<typename T>
  void deallocate(global_ptr<T> gptr) {
    UPCXX_GPTR_CHK(gptr);
    if (gptr != nullptr) {
      UPCXX_ASSERT(
        gptr.rank_ == upcxx::rank_me(),
        "upcxx::deallocate must be called by owner of global pointer"
      );
      
      upcxx::deallocate(gptr.raw_ptr_);
    }
  }

  namespace detail {
    template<bool throws, typename T, typename ...Args>
    global_ptr<T> new_(Args &&...args) {
      void *ptr = allocate(sizeof(T), alignof(T));
      
      if (ptr == nullptr) {
        if (throws) {
          throw bad_shared_alloc(__func__, std::max(sizeof(T), alignof(T)));
        }
        return nullptr;
      }
      
      try {
        ::new(ptr) T(std::forward<Args>(args)...); // placement new
      } catch (...) {
        // reclaim memory and rethrow the exception
        deallocate(ptr);
        throw;
      }

      return global_ptr<T>(
        detail::internal_only{},
        upcxx::rank_me(),
        reinterpret_cast<T*>(ptr)
      );
    }
  }

  template<typename T, typename ...Args>
  global_ptr<T> new_(Args &&...args) {
    return detail::new_</*throws=*/true, T>(std::forward<Args>(args)...);
  }

  template<typename T, typename ...Args>
  global_ptr<T> new_(const std::nothrow_t &tag, Args &&...args) {
    return detail::new_</*throws=*/false, T>(std::forward<Args>(args)...);
  }

  namespace detail {
    template<bool throws, typename T>
    global_ptr<T> new_array(std::size_t n) {
      static_assert(std::is_default_constructible<T>::value,
                    "T must be default constructible");
      
      std::size_t size = sizeof(std::size_t);
      size = (size + alignof(T)-1) & -alignof(T);

      if(n > (std::numeric_limits<std::size_t>::max() - size) / sizeof(T)) {
        // more bytes required than can be represented by size_t
        if(throws)
          throw bad_shared_alloc(std::string(__func__) + "(" + std::to_string(n) 
                              + ") requested more bytes than can be represented by size_t!");
        return nullptr;
      }

      std::size_t offset = size;
      size += n * sizeof(T);
      
      void *ptr = upcxx::allocate(size, std::max(alignof(std::size_t), alignof(T)));
      
      if(ptr == nullptr) {
        if(throws) throw bad_shared_alloc(__func__, size);
        return nullptr;
      }
      
      *reinterpret_cast<std::size_t*>(ptr) = n;
      T *elts = reinterpret_cast<T*>((char*)ptr + offset);
      
      if(!std::is_trivially_constructible<T>::value) {
        T *p = elts;
        try {
          for(T *p1=elts+n; p != p1; p++)
            ::new(p) T;
        } catch (...) {
          // destruct constructed elements, reclaim memory, and
          // rethrow the exception
          if(!std::is_trivially_destructible<T>::value) {
            for(T *p2=p-1; p2 >= elts; p2--)
              p2->~T();
          }
          deallocate(ptr);
          throw;
        }
      }
      
      return global_ptr<T>(detail::internal_only{}, upcxx::rank_me(), elts);
    }
  }

  template<typename T>
  global_ptr<T> new_array(std::size_t n) {
    return detail::new_array</*throws=*/true, T>(n);
  }

  template<typename T>
  global_ptr<T> new_array(std::size_t n, const std::nothrow_t &tag) {
    return detail::new_array</*throws=*/false, T>(n);
  }

  template<typename T>
  void delete_(global_ptr<T> gptr) {
    static_assert(std::is_destructible<T>::value,
                  "T must be destructible");
    UPCXX_GPTR_CHK(gptr);
    
    if (gptr != nullptr) {
      UPCXX_ASSERT(
        gptr.rank_ == upcxx::rank_me(),
        "upcxx::delete_ must be called by owner of shared memory."
      );
      
      T *ptr = gptr.raw_ptr_;
      ptr->~T();
      upcxx::deallocate(ptr);
    }
  }

  template<typename T>
  void delete_array(global_ptr<T> gptr) {
    static_assert(std::is_destructible<T>::value,
                  "T must be destructible");
    UPCXX_GPTR_CHK(gptr);
    
    if (gptr != nullptr) {
      UPCXX_ASSERT(
        gptr.rank_ == upcxx::rank_me(),
        "upcxx::delete_array must be called by owner of shared memory."
      );
      
      T *tptr = gptr.local();
      
      // padding to keep track of number of elements
      std::size_t padding = sizeof(std::size_t);
      padding = (padding + alignof(T)-1) & -alignof(T);
      
      void *ptr = reinterpret_cast<char*>(tptr) - padding;
      
      if (!std::is_trivially_destructible<T>::value) {
        std::size_t size = *reinterpret_cast<std::size_t*>(ptr);
        for(T *p=tptr, *p1=tptr+size; p != p1; p++)
          p->~T();
      }
      
      upcxx::deallocate(ptr);
    }
  }
} // namespace upcxx

#endif
