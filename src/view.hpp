#ifndef _3493eefe_7dec_42a4_b7dc_b98f99716dfe
#define _3493eefe_7dec_42a4_b7dc_b98f99716dfe

#include <upcxx/serialization.hpp>
#include <upcxx/utility.hpp>

#include <cstdint>
#include <iterator>
#include <stdexcept>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////////////
  // deserializing_iterator: Wraps a serialization_reader whose head is pointing
  // to a consecutive sequence of packed T's.
  namespace detail {
    template<typename T,
             bool skip_is_fast = serialization_traits<T>::skip_is_fast>
    struct serialization_view_element;
  }
    
  template<typename T>
  class deserializing_iterator {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T;
    using iterator_category = std::input_iterator_tag;
    
  private:
    detail::serialization_reader r_;

  public:
    deserializing_iterator(char const *p = nullptr) noexcept: r_(p) {}
    
    T operator*() const noexcept {
      detail::serialization_reader r1(r_);
      detail::raw_storage<T> raw;
      detail::serialization_view_element<T>::deserialize(r1, &raw);
      return raw.value_and_destruct();
    }
    
    deserializing_iterator operator++(int) noexcept {
      deserializing_iterator old = *this;
      detail::serialization_view_element<T>::skip(r_);
      return old;
    }

    deserializing_iterator& operator++() noexcept {
      detail::serialization_view_element<T>::skip(r_);
      return *this;
    }

    friend bool operator==(deserializing_iterator a, deserializing_iterator b) noexcept {
      return a.r_.head() == b.r_.head();
    }
    friend bool operator!=(deserializing_iterator a, deserializing_iterator b) noexcept {
      return a.r_.head() != b.r_.head();
    }
    friend bool operator<(deserializing_iterator a, deserializing_iterator b) noexcept {
      return a.r_.head() < b.r_.head();
    }
    friend bool operator>(deserializing_iterator a, deserializing_iterator b) noexcept {
      return a.r_.head() > b.r_.head();
    }
    friend bool operator<=(deserializing_iterator a, deserializing_iterator b) noexcept {
      return a.r_.head() <= b.r_.head();
    }
    friend bool operator>=(deserializing_iterator a, deserializing_iterator b) noexcept {
      return a.r_.head() >= b.r_.head();
    }
  };

  //////////////////////////////////////////////////////////////////////////////
  // view_default_iterator<T>::type: Determines the best iterator type for
  // looking at a consecutive sequence of packed T's.
  
  template<typename T,
           bool trivial = is_trivially_serializable<T>::value>
  struct view_default_iterator;
  
  template<typename T>
  struct view_default_iterator<T, /*trivial=*/true> {
    using type = T*;
  };
  template<typename T>
  struct view_default_iterator<T, /*trivial=*/false> {
    using type = deserializing_iterator<T>;
  };
  
  template<typename T>
  using view_default_iterator_t = typename view_default_iterator<T>::type;

  //////////////////////////////////////////////////////////////////////////////
  // view: A non-owning range delimited by a begin and end
  // iterator which can be serialized, but when deserialized the iterator type
  // will change to its default value (deserializing_iterator_of<T>::type).
  
  namespace detail {
    template<typename T, typename Iter,
             bool trivial = serialization_traits<T>::is_actually_trivially_serializable>
    struct serialization_view;

    template<typename Me, typename T, typename Iter>
    struct view_pointerness {
      using iterator = Iter;
      using size_type = std::size_t;
      using difference_type = std::ptrdiff_t;
      using value_type = T;
      using pointer = T*;
      using const_pointer = T const*;
      using reference = typename std::iterator_traits<Iter>::reference;
      using const_reference = typename std::conditional<
          std::is_same<reference, T&>::value,
          T const&,
          reference
        >::type;
    };

    template<typename Me, typename T, typename T1>
    struct view_pointerness<Me, T, T1*> {
      using iterator = T const*;
      using size_type = std::size_t;
      using difference_type = std::ptrdiff_t;
      using value_type = T;
      using pointer = T const*;
      using const_pointer = T const*;
      using reference = T const&;
      using const_reference = T const&;
      using const_iterator = T const*;
      using const_reverse_iterator = std::reverse_iterator<T const*>;

      constexpr const_pointer data() const noexcept {
        return static_cast<Me const*>(this)->beg_;
      }
      
      constexpr const_iterator cbegin() const noexcept {
        return static_cast<Me const*>(this)->beg_;
      }
      constexpr const_iterator cend() const noexcept {
        return static_cast<Me const*>(this)->end_;
      }
      
      constexpr const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(static_cast<Me const*>(this)->end_);
      }
      constexpr const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(static_cast<Me const*>(this)->beg_);
      }
    };

    template<typename Me, typename T, typename Iter,
             typename = void>
    struct view_randomness {
      // no operator[]
    };

    template<typename Me, typename T, typename Iter>
    struct view_randomness<
        Me, T, Iter,
        typename std::conditional<true, void, decltype(std::declval<Iter>() + int())>::type
      > {

      using reference = typename view_pointerness<Me,T,Iter>::reference;
      
      reference operator[](std::size_t i) const noexcept {
        return *(static_cast<Me const*>(this)->beg_ + i);
      }

      reference at(std::size_t i) const {
        Me const *me = static_cast<Me const*>(this);
        
        if(i < me->n_)
          return *(me->beg_ + i);
        else
          throw std::out_of_range("Index out of range for view<T>.");
      }
    };

    template<typename Me, typename T, typename Iter,
             typename = void>
    struct view_backwardness {
      // No rbegin(), rend(), or back()
    };

    template<typename Me, typename T, typename Iter>
    struct view_backwardness<
        Me, T, Iter,
        typename std::conditional<true, void, decltype(--std::declval<Iter&>())>::type
      > {

      using reference = typename view_pointerness<Me,T,Iter>::reference;
      using iterator = typename view_pointerness<Me,T,Iter>::iterator;
      using reverse_iterator = std::reverse_iterator<iterator>;

      constexpr reverse_iterator rbegin() const noexcept {
        return reverse_iterator(static_cast<Me const*>(this)->end_);
      }
      constexpr reverse_iterator rend() const noexcept {
        return reverse_iterator(static_cast<Me const*>(this)->beg_);
      }

      reference back() const noexcept {
        Iter x(static_cast<Me const*>(this)->end_);
        return *--x;
      }
    };
  }
  
  template<typename T, typename Iter = view_default_iterator_t<T>>
  class view:
    public detail::view_pointerness<view<T,Iter>, T, Iter>,
    public detail::view_randomness<view<T,Iter>, T, Iter>,
    public detail::view_backwardness<view<T,Iter>, T, Iter> {
    
    friend detail::view_pointerness<view<T,Iter>, T, Iter>;
    friend detail::view_randomness<view<T,Iter>, T, Iter>;
    friend detail::view_backwardness<view<T,Iter>, T, Iter>;
    
    friend serialization<view<T,Iter>>;
    friend detail::serialization_view<T,Iter>;

  public:
    using iterator = typename detail::view_pointerness<view<T,Iter>, T, Iter>::iterator;
    using reference = typename detail::view_pointerness<view<T,Iter>, T, Iter>::reference;
    
  private:  
    Iter beg_, end_;
    std::size_t n_;

  public:
    constexpr view():
      beg_(), end_(), n_(0) {
    }
    constexpr view(Iter begin, Iter end, std::size_t n):
      beg_(std::move(begin)),
      end_(std::move(end)),
      n_(n) {
    }
    
    constexpr iterator begin() const noexcept { return beg_; }
    constexpr iterator end() const noexcept { return end_; }

    constexpr std::size_t size() const noexcept { return n_; }
    constexpr bool empty() const noexcept { return n_ == 0; }

    constexpr reference front() const noexcept { return *beg_; }
  };
  
  //////////////////////////////////////////////////////////////////////////////
  // make_view: Factory functions for view.
  
  template<typename Bag,
           typename T = typename Bag::value_type,
           typename Iter = typename Bag::const_iterator>
  view<T,Iter> make_view(Bag const &bag) noexcept {
    return view<T,Iter>(bag.cbegin(), bag.cend(), bag.size());
  }

  template<typename T, std::size_t n>
  constexpr view<T, T const*> make_view(T const(&bag)[n]) {
    return view<T,T const*>((T const*)bag, (T const*)bag + n, n);
  }

  template<typename Iter,
           typename T = typename std::iterator_traits<Iter>::value_type,
           typename = decltype(std::distance(std::declval<Iter>(), std::declval<Iter>()))>
  view<T,Iter> make_view(Iter begin, Iter end) noexcept {
    std::size_t n = std::distance(begin, end);
    return view<T,Iter>(static_cast<Iter&&>(begin), static_cast<Iter&&>(end), n);
  }
  
  template<typename Iter,
           typename T = typename std::iterator_traits<Iter>::value_type>
  constexpr view<T,Iter> make_view(Iter begin, Iter end, std::size_t n) {
    return view<T,Iter>(static_cast<Iter&&>(begin), static_cast<Iter&&>(end), n);
  }

  //////////////////////////////////////////////////////////////////////////////
  // serialization<view>:
  
  namespace detail {
    template<typename T>
    struct serialization_view_element<T, /*skip_is_fast=*/true>:
      serialization_traits<T> {
    };

    template<typename T>
    struct serialization_view_element<T, /*skip_is_fast=*/false> {
      template<typename SS>
      static constexpr auto ubound(SS ub0, T const &x) noexcept
        UPCXX_RETURN_DECLTYPE(
          ub0.template cat_size_of<std::size_t>()
             .template cat_ubound_of<T>(x)
        ) {
        return ub0.template cat_size_of<std::size_t>()
                  .template cat_ubound_of<T>(x);
      }

      static constexpr bool references_buffer = serialization_traits<T>::references_buffer;

      template<typename Writer>
      static void serialize(Writer &w, T const &x) noexcept {
        void *delta = w.place(storage_size_of<std::size_t>());
        std::size_t sz0 = w.size();
        w.template write<T>(x);
        std::size_t sz1 = w.size();
        ::new(delta) std::size_t(sz1 - sz0);
      }

      template<typename Reader>
      static T* deserialize(Reader &r, void *spot) noexcept {
        r.template read_trivial<std::size_t>();
        return r.template read_into<T>(spot);
      }

      static constexpr bool skip_is_fast = true;
      
      template<typename Reader>
      static void skip(Reader &r) noexcept {
        std::size_t delta = r.template read_trivial<std::size_t>();
        r.jump(delta);
      }
    };

    // Non-trivially packed T. On the wire this is two size_t's, one for the skip
    // delta and one for the sequence length and then the consecutively packed T's.
    template<typename T, typename Iter>
    struct serialization_view<T, Iter, /*trivial=*/false> {
      // no ubound
      
      template<typename Writer>
      static void serialize(Writer &w, view<T,Iter> const &x) noexcept {
        void *delta = w.place(storage_size_of<std::size_t>());
        std::size_t size0 = w.size();

        std::size_t n = x.n_;
        w.template write_trivial<std::size_t>(n);

        for(auto elt = x.beg_; n != 0; n--, ++elt)
          serialization_view_element<T>::serialize(w, *elt);
        
        std::size_t size1 = w.size();
        ::new(delta) std::size_t(size1 - size0);
      }

      using deserialized_type = view<typename serialization_traits<T>::deserialized_type/*, default iterator*/>;
      
      template<typename Reader>
      static deserialized_type* deserialize(Reader &r, void *spot) noexcept {
        std::size_t delta = r.template read_trivial<std::size_t>();
        
        Reader r1(r);
        std::size_t n = r1.template read_trivial<std::size_t>();

        using Iter1 = typename deserialized_type::iterator;
        
        deserialized_type *ans = ::new(spot) deserialized_type(
          Iter1(r1.head()),
          Iter1(r.head() + delta),
          n
        );
        
        r.jump(delta);

        return ans;
      }

      template<typename Reader>
      static void skip(Reader &r) noexcept {
        std::size_t delta = r.template read_trivial<std::size_t>();
        r.jump(delta);
      }
    };

    // Trivially packed T's. On the wire this is a size_t for the element count
    // followed by the consecutively packed T's.
    template<typename T, typename Iter>
    struct serialization_view<T, Iter, /*trivial=*/true> {
      template<typename SS>
      static auto ubound(SS ub0, view<T,Iter> const &x) noexcept ->
        decltype(ub0.template cat_size_of<std::size_t>()
                    .cat(storage_size_of<T>().arrayed(x.n_))) {
        return ub0.template cat_size_of<std::size_t>()
                  .cat(storage_size_of<T>().arrayed(x.n_));
      }

      template<typename Writer>
      static void serialize(Writer &w, view<T,Iter> const &x) noexcept {
        w.template write_trivial<std::size_t>(x.n_);
        w.write_sequence(x.beg_, x.end_, x.n_);
      }

      using deserialized_type = view<T/*, default iterator*/>;
      
      template<typename Reader>
      static void skip(Reader &r) noexcept {
        std::size_t n = r.template read_trivial<std::size_t>();
        r.unplace(storage_size_of<T>().arrayed(n));
      }
      
      template<typename Reader>
      static deserialized_type* deserialize(Reader &r, void *spot) noexcept {
        std::size_t n = r.template read_trivial<std::size_t>();
        void *elts_mem = r.unplace(storage_size_of<T>().arrayed(n));
        T *elts = detail::launder_unconstructed((T*)elts_mem);
        return ::new(spot) view<T>(elts, elts + n, n);
      }
    };
  }

  template<typename T, typename Iter>
  struct serialization<view<T,Iter>>:
      detail::serialization_view<T,Iter> {
    static constexpr bool is_serializable = serialization_traits<T>::is_serializable;
    static constexpr bool references_buffer = true;
    static constexpr bool skip_is_fast = true;
  };
}

#endif
