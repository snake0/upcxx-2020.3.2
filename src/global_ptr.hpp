#ifndef _bcef2443_cf3b_4148_be6d_be2d24f46848
#define _bcef2443_cf3b_4148_be6d_be2d24f46848

/**
 * global_ptr.hpp
 */

#include <upcxx/backend.hpp>
#include <upcxx/diagnostic.hpp>
#include <upcxx/memory_kind.hpp>

#include <cassert> // assert
#include <cstddef> // ptrdiff_t
#include <cstdint> // uintptr_t
#include <cstring> // memcpy
#include <iostream> // ostream
#include <type_traits> // is_const, is_volatile

namespace upcxx {
  //////////////////////////////////////////////////////////////////////////////
  // global_ptr
  
  template<typename T, memory_kind KindSet = memory_kind::host>
  class global_ptr {
  public:
    static_assert(!std::is_const<T>::value && !std::is_volatile<T>::value,
                  "global_ptr<T> does not support cv qualification on T");

    using element_type = T;
    static constexpr memory_kind kind = KindSet;

    #ifndef UPCXX_GPTR_CHECK_ENABLED
    // -DUPCXX_GPTR_CHECK_ENABLED=0/1 independently controls gptr checking (default enabled with assertions)
    #define UPCXX_GPTR_CHECK_ENABLED UPCXX_ASSERT_ENABLED 
    #endif
    #ifndef UPCXX_GPTR_CHECK_ALIGNMENT
    #define UPCXX_GPTR_CHECK_ALIGNMENT 1 // -DUPCXX_GPTR_CHECK_ALIGNMENT=0 disables alignment checking
    #endif
    void check(bool allow_null=true, const char *context=nullptr) const {
        void *this_sanity_check = (void*)this;
        UPCXX_ASSERT_ALWAYS(this_sanity_check, "global_ptr::check() invoked on a null pointer to global_ptr");
        #if UPCXX_GPTR_CHECK_ALIGNMENT
          constexpr size_t align = detail::align_of<T>();
        #else
          constexpr size_t align = 0;
        #endif
        backend::validate_global_ptr(allow_null, rank_, reinterpret_cast<void*>(raw_ptr_), device_, 
                                     KindSet, align, detail::typename_of<T>(), context);
    }
    #if UPCXX_GPTR_CHECK_ENABLED
      #define UPCXX_GPTR_CHK(p)         ((p).check(true, __func__))
      #define UPCXX_GPTR_CHK_NONNULL(p) ((p).check(false, __func__))
    #else
      #define UPCXX_GPTR_CHK(p)         ((void)0)
      #define UPCXX_GPTR_CHK_NONNULL(p) ((void)0)
    #endif
    
    explicit global_ptr(detail::internal_only, intrank_t rank, T *raw,
                        int device = -1):
      #if UPCXX_MANY_KINDS
        device_(device),
      #endif
      rank_(rank),
      raw_ptr_(raw) {
      static_assert(std::is_trivially_copyable<global_ptr<T,KindSet>>::value, "Internal error.");
      UPCXX_GPTR_CHK(*this);
    }

    // global_ptr offset and reinterpret in a single operation
    template <typename U>
    explicit global_ptr(detail::internal_only, 
                        const global_ptr<U, KindSet> &other, std::ptrdiff_t offset):
      #if UPCXX_MANY_KINDS
        device_(other.device_),
      #endif
      rank_(other.rank_),
      raw_ptr_(reinterpret_cast<T*>(
                 reinterpret_cast<::std::uintptr_t>(other.raw_ptr_) + offset)) { 
        UPCXX_GPTR_CHK(other);
        UPCXX_ASSERT(other, "Global pointer expression may not be null");
        UPCXX_GPTR_CHK_NONNULL(*this);
      }

    template<memory_kind KindSet1,
             typename = typename std::enable_if<((int)KindSet & (int)KindSet1) == (int)KindSet1>::type>
    global_ptr(global_ptr<T,KindSet1> const &that):
      global_ptr(detail::internal_only(), that.rank_, that.raw_ptr_, that.device_) {
      UPCXX_GPTR_CHK(*this);
    }
    
    // null pointer represented with rank 0
    global_ptr(std::nullptr_t nil = nullptr):
      global_ptr(detail::internal_only(), 0, nullptr) {
      UPCXX_GPTR_CHK(*this);
    }
    
    bool is_local() const {
      UPCXX_GPTR_CHK(*this);
      return device_ == -1 && (raw_ptr_ == nullptr || backend::rank_is_local(rank_));
    }

    bool is_null() const {
      UPCXX_GPTR_CHK(*this);
      return device_ == -1 && raw_ptr_ == nullptr;
    }
    
    // This creates ambiguity with gp/int arithmetic like `my_gp + 1` since 
    // the compiler can't decide if it wants to upconvert the 1 to ptrdiff_t
    // or downconvert (to bool) the gp and use operator+(int,int). This is why
    // our operator+/- have overloads for all the integral types (those smaller
    // than `int` aren't necessary due to promotion).
    explicit operator bool() const {
      return !is_null();
    }
    
    T* local() const {
      UPCXX_GPTR_CHK(*this);
      return KindSet != memory_kind::host && device_ != -1
        ? nullptr
        : static_cast<T*>(
          backend::localize_memory(
            rank_,
            reinterpret_cast<std::uintptr_t>(raw_ptr_)
          )
      );
    }

    intrank_t where() const {
      UPCXX_GPTR_CHK(*this);
      return rank_;
    }

    memory_kind dynamic_kind() const {
      UPCXX_GPTR_CHK(*this);
      if(0 == (int(KindSet) & (int(KindSet)-1))) // determines if KindSet is a singleton set
        return KindSet;
      else
        return device_ == -1 ? memory_kind::host : memory_kind::cuda_device;
    }
    
    global_ptr operator+=(std::ptrdiff_t diff) {
      if (diff) UPCXX_GPTR_CHK_NONNULL(*this);
      else      UPCXX_GPTR_CHK(*this);
      raw_ptr_ += diff;
      UPCXX_GPTR_CHK(*this);
      return *this;
    }
    friend global_ptr operator+(global_ptr a, int b) { return a += (ptrdiff_t)b; }
    friend global_ptr operator+(global_ptr a, long b) { return a += (ptrdiff_t)b; }
    friend global_ptr operator+(global_ptr a, long long b) { return a += (ptrdiff_t)b; }
    friend global_ptr operator+(global_ptr a, unsigned int b) { return a += (ptrdiff_t)b; }
    friend global_ptr operator+(global_ptr a, unsigned long b) { return a += (ptrdiff_t)b; }
    friend global_ptr operator+(global_ptr a, unsigned long long b) { return a += (ptrdiff_t)b; }
    
    friend global_ptr operator+(int b, global_ptr a) { return a += (ptrdiff_t)b; }
    friend global_ptr operator+(long b, global_ptr a) { return a += (ptrdiff_t)b; }
    friend global_ptr operator+(long long b, global_ptr a) { return a += (ptrdiff_t)b; }
    friend global_ptr operator+(unsigned int b, global_ptr a) { return a += (ptrdiff_t)b; }
    friend global_ptr operator+(unsigned long b, global_ptr a) { return a += (ptrdiff_t)b; }
    friend global_ptr operator+(unsigned long long b, global_ptr a) { return a += (ptrdiff_t)b; }

    global_ptr operator-=(std::ptrdiff_t diff) {
      if (diff) UPCXX_GPTR_CHK_NONNULL(*this);
      else      UPCXX_GPTR_CHK(*this);
      raw_ptr_ -= diff;
      UPCXX_GPTR_CHK(*this);
      return *this;
    }
    friend global_ptr operator-(global_ptr a, int b) { return a -= (ptrdiff_t)b; }
    friend global_ptr operator-(global_ptr a, long b) { return a -= (ptrdiff_t)b; }
    friend global_ptr operator-(global_ptr a, long long b) { return a -= (ptrdiff_t)b; }
    friend global_ptr operator-(global_ptr a, unsigned int b) { return a -= (ptrdiff_t)b; }
    friend global_ptr operator-(global_ptr a, unsigned long b) { return a -= (ptrdiff_t)b; }
    friend global_ptr operator-(global_ptr a, unsigned long long b) { return a -= (ptrdiff_t)b; }
    
    std::ptrdiff_t operator-(global_ptr rhs) const {
      if (raw_ptr_ == rhs.raw_ptr_) { UPCXX_GPTR_CHK(*this); UPCXX_GPTR_CHK(rhs); }
      else  { UPCXX_GPTR_CHK_NONNULL(*this); UPCXX_GPTR_CHK_NONNULL(rhs); }
      UPCXX_ASSERT(device_ == rhs.device_, "operator-(global_ptr,global_ptr): requires pointers of the same kind & device.");
      UPCXX_ASSERT(rank_ == rhs.rank_, "operator-(global_ptr,global_ptr): requires pointers to the same rank.");
      return raw_ptr_ - rhs.raw_ptr_;
    }

    global_ptr& operator++() {
      return *this = *this + 1;
    }

    global_ptr operator++(int) {
      global_ptr old = *this;
      *this = *this + 1;
      return old;
    }

    global_ptr& operator--() {
      return *this = *this - 1;
    }

    global_ptr operator--(int) {
      global_ptr old = *this;
      *this = *this - 1;
      return old;
    }
    
    friend bool operator==(global_ptr a, global_ptr b) {
      UPCXX_GPTR_CHK(a); UPCXX_GPTR_CHK(b); 
      return a.device_ == b.device_ && a.rank_ == b.rank_ && a.raw_ptr_ == b.raw_ptr_;
    }
    friend bool operator==(global_ptr a, std::nullptr_t) {
      return a == global_ptr(nullptr);
    }
    friend bool operator==(std::nullptr_t, global_ptr b) {
      return global_ptr(nullptr) == b;
    }
    
    friend bool operator!=(global_ptr a, global_ptr b) {
      UPCXX_GPTR_CHK(a); UPCXX_GPTR_CHK(b); 
      return a.device_ != b.device_ || a.rank_ != b.rank_ || a.raw_ptr_ != b.raw_ptr_;
    }
    friend bool operator!=(global_ptr a, std::nullptr_t) {
      return a != global_ptr(nullptr);
    }
    friend bool operator!=(std::nullptr_t, global_ptr b) {
      return global_ptr(nullptr) != b;
    }
    
    // Comparison operators specify partial order
    #define UPCXX_COMPARE_OP(op) \
      friend bool operator op(global_ptr a, global_ptr b) {\
        UPCXX_GPTR_CHK(a); UPCXX_GPTR_CHK(b); \
        return a.raw_ptr_ op b.raw_ptr_;\
      }\
      friend bool operator op(global_ptr a, std::nullptr_t b) {\
        UPCXX_GPTR_CHK(a); \
        return a.raw_ptr_ op b;\
      }\
      friend bool operator op(std::nullptr_t a, global_ptr b) {\
        UPCXX_GPTR_CHK(b); \
        return a op b.raw_ptr_;\
      }
    UPCXX_COMPARE_OP(<)
    UPCXX_COMPARE_OP(<=)
    UPCXX_COMPARE_OP(>)
    UPCXX_COMPARE_OP(>=)
    #undef UPCXX_COMAPRE_OP
    
  private:
    friend struct std::less<global_ptr<T,KindSet>>;
    friend struct std::less_equal<global_ptr<T,KindSet>>;
    friend struct std::greater<global_ptr<T,KindSet>>;
    friend struct std::greater_equal<global_ptr<T,KindSet>>;
    friend struct std::hash<global_ptr<T,KindSet>>;

    template<typename U, typename V, memory_kind K>
    friend global_ptr<U,K> reinterpret_pointer_cast(global_ptr<V,K> ptr);

    template<typename U, memory_kind K>
    friend std::ostream& operator<<(std::ostream &os, global_ptr<U,K> ptr);

    //explicit global_ptr(intrank_t rank, T* ptr)
    //  : rank_(rank), raw_ptr_(ptr) {}
  
  public: //private!
    #if UPCXX_MANY_KINDS
      std::int32_t device_;
    #else
      static constexpr std::int32_t device_ = -1;
    #endif
    intrank_t rank_;
    T* raw_ptr_;
  };

  template<typename T, typename U, memory_kind K>
  global_ptr<T,K> static_pointer_cast(global_ptr<U,K> ptr) {
    UPCXX_GPTR_CHK(ptr);
    return global_ptr<T,K>(detail::internal_only(),
                           ptr.rank_,
                           static_cast<T*>(ptr.raw_ptr_),
                           ptr.device_);
  }

  template<typename T, typename U, memory_kind K>
  global_ptr<T,K> reinterpret_pointer_cast(global_ptr<U,K> ptr) {
    UPCXX_GPTR_CHK(ptr);
    return global_ptr<T,K>(detail::internal_only(),
                           ptr.rank_,
                           reinterpret_cast<T*>(ptr.raw_ptr_),
                           ptr.device_);
  }

  template<memory_kind K, typename T, memory_kind K1>
  // sfinae out if there is no overlap between the two KindSet's
  typename std::enable_if<(int(K) & int(K1)) != 0 , global_ptr<T,K>>::type
  static_kind_cast(global_ptr<T,K1> p) {
    UPCXX_GPTR_CHK(p);
    return global_ptr<T,K>(detail::internal_only(), p.rank_, p.raw_ptr_, p.device_);
  }
  
  template<memory_kind K, typename T, memory_kind K1>
  // sfinae out if there is no overlap between the two KindSet's
  typename std::enable_if<(int(K) & int(K1)) != 0 , global_ptr<T,K>>::type
  dynamic_kind_cast(global_ptr<T,K1> p) {
    UPCXX_GPTR_CHK(p);
    return ((int)p.dynamic_kind() & (int)K) != 0
        ? global_ptr<T,K>(
          detail::internal_only(), p.rank_, p.raw_ptr_, p.device_
        )
        : global_ptr<T,K>(nullptr);
  }

  template<typename T, memory_kind K>
  std::ostream& operator<<(std::ostream &os, global_ptr<T,K> ptr) {
    // UPCXX_GPTR_CHK(ptr) // allow output of bad pointers for diagnostic purposes
    return os << "(gp: " << ptr.rank_ << ", " 
              << reinterpret_cast<void*>(ptr.raw_ptr_) // issue #223
	      << ", dev=" << ptr.device_ << ")";
  }

  template<typename T>
  global_ptr<T> to_global_ptr(T *p) {
    if(p == nullptr)
      return global_ptr<T>(nullptr);
    else {
      intrank_t rank;
      std::uintptr_t raw;
    
      std::tie(rank, raw) = backend::globalize_memory((void*)p);
    
      return global_ptr<T>(detail::internal_only(), rank, reinterpret_cast<T*>(raw));
    }
  }
  
  template<typename T>
  global_ptr<T> try_global_ptr(T *p) {
    intrank_t rank;
    std::uintptr_t raw;
    
    std::tie(rank, raw) =
      p == nullptr
        ? std::tuple<intrank_t, std::uintptr_t>(0, 0x0)
        : backend::globalize_memory((void*)p, std::make_tuple(0, 0x0));
    
    return global_ptr<T>(detail::internal_only(), rank, reinterpret_cast<T*>(raw));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Specializations of standard function objects

namespace std {
  // Comparators specify total order
  template<typename T, upcxx::memory_kind K>
  struct less<upcxx::global_ptr<T,K>> {
    bool operator()(upcxx::global_ptr<T,K> lhs,
                              upcxx::global_ptr<T,K> rhs) const {
      UPCXX_GPTR_CHK(lhs); UPCXX_GPTR_CHK(rhs); 
      bool ans = lhs.raw_ptr_ < rhs.raw_ptr_;
      ans &= lhs.rank_ == rhs.rank_;
      ans |= lhs.rank_ < rhs.rank_;
      ans &= lhs.device_ == rhs.device_;
      ans |= lhs.device_ < rhs.device_;
      return ans;
    }
  };
  
  template<typename T, upcxx::memory_kind K>
  struct less_equal<upcxx::global_ptr<T,K>> {
    bool operator()(upcxx::global_ptr<T,K> lhs,
                              upcxx::global_ptr<T,K> rhs) const {
      UPCXX_GPTR_CHK(lhs); UPCXX_GPTR_CHK(rhs); 
      bool ans = lhs.raw_ptr_ <= rhs.raw_ptr_;
      ans &= lhs.rank_ == rhs.rank_;
      ans |= lhs.rank_ < rhs.rank_;
      ans &= lhs.device_ == rhs.device_;
      ans |= lhs.device_ < rhs.device_;
      return ans;
    }
  };
  
  template<typename T, upcxx::memory_kind K>
  struct greater<upcxx::global_ptr<T,K>> {
    bool operator()(upcxx::global_ptr<T,K> lhs,
                              upcxx::global_ptr<T,K> rhs) const {
      UPCXX_GPTR_CHK(lhs); UPCXX_GPTR_CHK(rhs); 
      bool ans = lhs.raw_ptr_ > rhs.raw_ptr_;
      ans &= lhs.rank_ == rhs.rank_;
      ans |= lhs.rank_ > rhs.rank_;
      ans &= lhs.device_ == rhs.device_;
      ans |= lhs.device_ > rhs.device_;
      return ans;
    }
  };
  
  template<typename T, upcxx::memory_kind K>
  struct greater_equal<upcxx::global_ptr<T,K>> {
    bool operator()(upcxx::global_ptr<T,K> lhs,
                              upcxx::global_ptr<T,K> rhs) const {
      UPCXX_GPTR_CHK(lhs); UPCXX_GPTR_CHK(rhs); 
      bool ans = lhs.raw_ptr_ >= rhs.raw_ptr_;
      ans &= lhs.rank_ == rhs.rank_;
      ans |= lhs.rank_ > rhs.rank_;
      ans &= lhs.device_ == rhs.device_;
      ans |= lhs.device_ > rhs.device_;
      return ans;
    }
  };

  template<typename T, upcxx::memory_kind K>
  struct hash<upcxx::global_ptr<T,K>> {
    std::size_t operator()(upcxx::global_ptr<T,K> gptr) const {
      UPCXX_GPTR_CHK(gptr); 
      /** Utilities derived from Boost, subject to the following license:

      Boost Software License - Version 1.0 - August 17th, 2003

      Permission is hereby granted, free of charge, to any person or organization
      obtaining a copy of the software and accompanying documentation covered by
      this license (the "Software") to use, reproduce, display, distribute,
      execute, and transmit the Software, and to prepare derivative works of the
      Software, and to permit third-parties to whom the Software is furnished to
      do so, all subject to the following:

      The copyright notices in the Software and this entire statement, including
      the above license grant, this restriction and the following disclaimer,
      must be included in all copies of the Software, in whole or in part, and
      all derivative works of the Software, unless such copies or derivative
      works are solely in the form of machine-executable object code generated by
      a source language processor.

      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
      IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
      FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
      SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
      FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
      ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
      DEALINGS IN THE SOFTWARE.
      */

      std::uint64_t b = std::uint64_t(gptr.device_)<<32 | std::uint32_t(gptr.rank_);
      std::uint64_t a = reinterpret_cast<std::uint64_t>(gptr.raw_ptr_);
      a ^= b + 0x9e3779b9 + (a<<6) + (a>>2);
      return std::size_t(a);
    }
  };
} // namespace std

#endif
