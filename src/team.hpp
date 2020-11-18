#ifndef _773d73ab_fce8_4b18_9cd0_d7c76204383c
#define _773d73ab_fce8_4b18_9cd0_d7c76204383c

#include <upcxx/team_fwd.hpp>
#include <upcxx/backend.hpp>

namespace upcxx {
  namespace detail {
    template<typename T>
    future_header_promise<T>* registered_promise(digest id, int initial_anon) {
      UPCXX_ASSERT(backend::master.active_with_caller());
      
      future_header_promise<T> *pro;
      
      // We use dist_master_registry to map from `dist_id<T>` to
      // `promise<dist_object<T>&>*`
      auto it_and_inserted = registry.insert({id, nullptr});
      
      if(it_and_inserted.second) {
        pro = new future_header_promise<T>;
        detail::promise_require_anonymous(pro, initial_anon);
        it_and_inserted.first->second = static_cast<void*>(pro);
      }
      else
        pro = static_cast<future_header_promise<T>*>(it_and_inserted.first->second);
      
      return pro;
    }

    template<typename T, typename ...U>
    T* registered_state(digest id, U &&...ctor_args) {
      UPCXX_ASSERT(backend::master.active_with_caller());
      
      T *thing;
      
      // We use dist_master_registry to map from `dist_id<T>` to
      // `promise<dist_object<T>&>*`
      auto it_and_inserted = registry.insert({id, nullptr});
      
      if(it_and_inserted.second) {
        thing = new T(std::forward<U>(ctor_args)...);
        it_and_inserted.first->second = static_cast<void*>(thing);
      }
      else
        thing = static_cast<T*>(it_and_inserted.first->second);
      
      return thing;
    }
  }
  
  inline bool local_team_contains(intrank_t rank) {
    return backend::rank_is_local(rank);
  }
  
  inline team& world() {
    return detail::the_world_team.value();
  }
  inline team& local_team() {
    return detail::the_local_team.value();
  }

  // team references are bound using their id's.
  template<>
  struct binding<team&> {
    using on_wire_type = team_id;
    using off_wire_type = team&;
    using off_wire_future_type = typename detail::make_future<team&>::return_type;
    using stripped_type = team&;
    static constexpr bool immediate = true;
    
    static team_id on_wire(team const &o) {
      return o.id();
    }
    
    static team& off_wire(team_id id) {
      return id.here();
    }

    static off_wire_future_type off_wire_future(team_id id) {
      return upcxx::make_future<team&>(id.here());
    }
  };
  
  template<>
  struct binding<team const&>:
    binding<team&> {
    
    using stripped_type = team const&;
  };
  
  template<>
  struct binding<team&&> {
    #if 0
      // TODO: figure out how to make this static_assert trip only when this
      // instantiation is USED.
      static_assert(false,
        "Moving a team&& into a binding must surely be an error!"
      );
    #endif
  };
}
#endif
  
  
