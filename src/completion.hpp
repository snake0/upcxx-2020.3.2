#ifndef _96368972_b5ed_4e48_ac4f_8c868279e3dd
#define _96368972_b5ed_4e48_ac4f_8c868279e3dd

#include <upcxx/backend.hpp>
#include <upcxx/bind.hpp>
#include <upcxx/future.hpp>
#include <upcxx/lpc_dormant.hpp>
#include <upcxx/persona.hpp>
#include <upcxx/utility.hpp>

#include <tuple>

namespace upcxx {
  //////////////////////////////////////////////////////////////////////
  // Event names for common completion events as used by rput/rget etc.
  // This set is extensible from anywhere in the source.
  
  struct source_cx_event;
  struct remote_cx_event;
  struct operation_cx_event;

  namespace detail {
    // Useful type predicates for selecting events (as needed by
    // completions_state's EventPredicate argument).
    template<typename Event>
    struct event_is_here: std::false_type {};
    template<>
    struct event_is_here<source_cx_event>: std::true_type {};
    template<>
    struct event_is_here<operation_cx_event>: std::true_type {};

    template<typename Event>
    struct event_is_remote: std::false_type {};
    template<>
    struct event_is_remote<remote_cx_event>: std::true_type {};
  }
    
  //////////////////////////////////////////////////////////////////////
  // Signalling actions tagged by the event they react to.

  // Future completion
  template<typename Event, progress_level level = progress_level::user>
  struct future_cx {
    using event_t = Event;
    using deserialized_cx = future_cx<Event,level>;
  };

  // Promise completion
  template<typename Event, typename ...T>
  struct promise_cx {
    using event_t = Event;
    using deserialized_cx = promise_cx<Event,T...>;
    detail::promise_shref<T...> pro_;
  };

  // Synchronous completion via best-effort buffering
  template<typename Event>
  struct buffered_cx {
    using event_t = Event;
    using deserialized_cx = buffered_cx<Event>;
  };

  // Synchronous completion via blocking on network/peers
  template<typename Event>
  struct blocking_cx {
    using event_t = Event;
    using deserialized_cx = blocking_cx<Event>;
  };

  // LPC completion
  template<typename Event, typename Fn>
  struct lpc_cx {
    using event_t = Event;
    using deserialized_cx = lpc_cx<Event,Fn>;
    
    persona *target_;
    Fn fn_;

    lpc_cx(persona &target, Fn fn):
      target_(&target),
      fn_(std::move(fn)) {
    }
  };
  
  // RPC completion. Arguments are bound into fn_.
  template<typename Event, typename Fn>
  struct rpc_cx {
    using event_t = Event;
    using deserialized_cx = rpc_cx<Event, typename serialization_traits<Fn>::deserialized_type>;
    
    Fn fn_;
    rpc_cx(Fn fn): fn_(std::move(fn)) {}
  };
  
  //////////////////////////////////////////////////////////////////////
  // completions<...>: A list of tagged completion actions. We use
  // lisp-like lists where the head is the first element and the tail
  // is the list of everything after.
  
  template<typename ...Cxs>
  struct completions;
  template<>
  struct completions<> {};
  template<typename H, typename ...T>
  struct completions<H,T...>: completions<T...> {
    H head;

    H&& head_moved() {
      return static_cast<H&&>(head);
    }
    completions<T...>&& tail_moved() {
      return static_cast<completions<T...>&&>(*this);
    }
    
    constexpr completions(H head, T ...tail):
      completions<T...>(std::move(tail)...),
      head(std::move(head)) {
    }
    constexpr completions(H head, completions<T...> tail):
      completions<T...>(std::move(tail)),
      head(std::move(head)) {
    }
  };

  //////////////////////////////////////////////////////////////////////
  // operator "|": Concatenates two completions lists.
  
  template<typename ...B>
  constexpr completions<B...> operator|(
      completions<> a, completions<B...> b
    ) {
    return b;
  }
  template<typename Ah, typename ...At, typename ...B>
  constexpr completions<Ah,At...,B...> operator|(
      completions<Ah,At...> a,
      completions<B...> b
    ) {
    return completions<Ah,At...,B...>{
      a.head_moved(),
      a.tail_moved() | std::move(b)
    };
  }

  //////////////////////////////////////////////////////////////////////
  // detail::completions_has_event: detects if there exists an action
  // tagged by the given event in the completions list.

  namespace detail {
    template<typename Cxs, typename Event>
    struct completions_has_event;
    
    template<typename Event>
    struct completions_has_event<completions<>, Event> {
      static constexpr bool value = false;
    };
    template<typename CxH, typename ...CxT, typename Event>
    struct completions_has_event<completions<CxH,CxT...>, Event> {
      static constexpr bool value =
        std::is_same<Event, typename CxH::event_t>::value ||
        completions_has_event<completions<CxT...>, Event>::value;
    };
  }

  //////////////////////////////////////////////////////////////////////
  // detail::completions_is_event_sync: detects if there exists a
  // buffered_cx or blocking_cx action tagged by the given event in the
  // completions list

  namespace detail {
    template<typename Cxs, typename Event>
    struct completions_is_event_sync;
    
    template<typename Event>
    struct completions_is_event_sync<completions<>, Event> {
      static constexpr bool value = false;
    };
    template<typename ...CxT, typename Event>
    struct completions_is_event_sync<completions<buffered_cx<Event>,CxT...>, Event> {
      static constexpr bool value = true;
    };
    template<typename ...CxT, typename Event>
    struct completions_is_event_sync<completions<blocking_cx<Event>,CxT...>, Event> {
      static constexpr bool value = true;
    };
    template<typename CxH, typename ...CxT, typename Event>
    struct completions_is_event_sync<completions<CxH,CxT...>, Event> {
      static constexpr bool value =
        completions_is_event_sync<completions<CxT...>, Event>::value;
    };
  }
  
  //////////////////////////////////////////////////////////////////////
  // User-interface for obtaining a completion tied to an event.

  namespace detail {
    template<typename Event>
    struct support_as_future {
      static constexpr completions<future_cx<Event>> as_future() {
        return {future_cx<Event>{}};
      }
    };

    template<typename Event>
    struct support_as_promise {
      template<typename ...T>
      static constexpr completions<promise_cx<Event, T...>> as_promise(promise<T...> pro) {
        return {promise_cx<Event, T...>{
          static_cast<promise_shref<T...>&&>(promise_as_shref(pro))
        }};
      }
    };

    template<typename Event>
    struct support_as_lpc {
      template<typename Fn>
      static constexpr completions<lpc_cx<Event, Fn>> as_lpc(persona &target, Fn func) {
        return {
          lpc_cx<Event, Fn>{target, std::forward<Fn>(func)}
        };
      }
    };

    template<typename Event>
    struct support_as_buffered {
      static constexpr completions<buffered_cx<Event>> as_buffered() {
        return {buffered_cx<Event>{}};
      }
    };

    template<typename Event>
    struct support_as_blocking {
      static constexpr completions<blocking_cx<Event>> as_blocking() {
        return {blocking_cx<Event>{}};
      }
    };

    template<typename Event>
    struct support_as_rpc {
      template<typename Fn, typename ...Args>
      static completions<
          rpc_cx<Event, typename detail::bind<Fn&&, Args&&...>::return_type>
        >
      as_rpc(Fn &&fn, Args &&...args) {
        return {
          rpc_cx<Event, typename detail::bind<Fn&&, Args&&...>::return_type>{
            upcxx::bind(std::forward<Fn>(fn), std::forward<Args>(args)...)
          }
        };
      }
    };
  }

  struct source_cx:
    detail::support_as_blocking<source_cx_event>,
    detail::support_as_buffered<source_cx_event>,
    detail::support_as_future<source_cx_event>,
    detail::support_as_lpc<source_cx_event>,
    detail::support_as_promise<source_cx_event> {};
  
  struct operation_cx:
    detail::support_as_blocking<operation_cx_event>,
    detail::support_as_future<operation_cx_event>,
    detail::support_as_lpc<operation_cx_event>,
    detail::support_as_promise<operation_cx_event> {};
  
  struct remote_cx:
    detail::support_as_rpc<remote_cx_event> {};
  
  //////////////////////////////////////////////////////////////////////
  // cx_state: Per action state that survives until the event
  // is triggered. For future_cx's this holds a promise instance which
  // seeds the future given back to the user. All other cx actions get
  // their information stored as-is. All of these expose `operator()(T...)`
  // which is used to "fire" the action safely from any progress context.
  // Notice that the args are taken as by-value T..., this ensures they each
  // make get their own private copy, which they should then move into the
  // users callable or promise etc.
  
  namespace detail {
    template<typename Cx /* the action */,
             typename EventArgsTup /* tuple containing list of action's value types*/>
    struct cx_state;
    
    template<typename Event>
    struct cx_state<buffered_cx<Event>, std::tuple<>> {
      cx_state(buffered_cx<Event>) {}
      void operator()() {}
    };
    
    template<typename Event>
    struct cx_state<blocking_cx<Event>, std::tuple<>> {
      cx_state(blocking_cx<Event>) {}
      void operator()() {}
    };
    
    template<typename Event, progress_level level, typename ...T>
    struct cx_state<future_cx<Event,level>, std::tuple<T...>> {
      future_header_promise<T...> *pro_; // holds ref

      cx_state(future_cx<Event,level>):
        pro_(new future_header_promise<T...>) {
      }

      detail::promise_future_t<T...> get_future() const {
        return detail::promise_get_future(pro_);
      }
      
      lpc_dormant<T...>* to_lpc_dormant(lpc_dormant<T...> *tail) && {
        return detail::make_lpc_dormant_quiesced_promise<T...>(
          upcxx::current_persona(), progress_level::user, /*move ref*/pro_, tail
        );
      }
      
      void operator()(T ...vals) {
        backend::fulfill_during<level>(
          /*move ref*/pro_, std::tuple<T...>(static_cast<T&&>(vals)...)
        );
      }
    };

    // promise and events have matching (non-empty) types
    template<typename Event, typename ...T>
    struct cx_state<promise_cx<Event,T...>, std::tuple<T...>> {
      future_header_promise<T...> *pro_; // holds ref

      cx_state(promise_cx<Event,T...> &&cx):
        pro_(cx.pro_.steal_header()) {
        detail::promise_require_anonymous(pro_, 1);
      }

      lpc_dormant<T...>* to_lpc_dormant(lpc_dormant<T...> *tail) && {
        future_header_promise<T...> *pro = /*move ref*/pro_;
        return detail::make_lpc_dormant(
          upcxx::current_persona(), progress_level::user,
          [/*move ref*/pro](T &&...results) {
            backend::fulfill_during<progress_level::user>(
              /*move ref*/pro, std::tuple<T...>(static_cast<T&&>(results)...)
            );
          },
          tail
        );
      }
      
      void operator()(T ...vals) {
        backend::fulfill_during<progress_level::user>(
          /*move ref*/pro_, std::tuple<T...>(static_cast<T&&>(vals)...)
        );
      }
    };
    // event is empty
    template<typename Event, typename ...T>
    struct cx_state<promise_cx<Event,T...>, std::tuple<>> {
      future_header_promise<T...> *pro_; // holds ref

      cx_state(promise_cx<Event,T...> &&cx):
        pro_(cx.pro_.steal_header()) {
        detail::promise_require_anonymous(pro_, 1);
      }
      
      lpc_dormant<>* to_lpc_dormant(lpc_dormant<> *tail) && {
        future_header_promise<T...> *pro = /*move ref*/pro_;
        return detail::make_lpc_dormant(
          upcxx::current_persona(), progress_level::user,
          [/*move ref*/pro]() {
            backend::fulfill_during<progress_level::user>(/*move ref*/pro, 1);
          },
          tail
        );
      }
      
      void operator()() {
        backend::fulfill_during<progress_level::user>(/*move ref*/pro_, 1);
      }
    };
    // promise and event are empty
    template<typename Event>
    struct cx_state<promise_cx<Event>, std::tuple<>> {
      future_header_promise<> *pro_; // holds ref

      cx_state(promise_cx<Event> &&cx):
        pro_(cx.pro_.steal_header()) {
        detail::promise_require_anonymous(pro_, 1);
      }

      lpc_dormant<>* to_lpc_dormant(lpc_dormant<> *tail) && {
        future_header_promise<> *pro = /*move ref*/pro_;
        return detail::make_lpc_dormant<>(
          upcxx::current_persona(), progress_level::user,
          [/*move ref*/pro]() {
            backend::fulfill_during<progress_level::user>(/*move ref*/pro, 1);
          },
          tail
        );
      }
      
      void operator()() {
        backend::fulfill_during<progress_level::user>(/*move ref*/pro_, 1);
      }
    };
    
    template<typename Event, typename Fn, typename ...T>
    struct cx_state<lpc_cx<Event,Fn>, std::tuple<T...>> {
      persona *target_;
      Fn fn_;
      
      cx_state(lpc_cx<Event,Fn> &&cx):
        target_(cx.target_),
        fn_(static_cast<Fn&&>(cx.fn_)) {
        upcxx::current_persona().undischarged_n_ += 1;
      }

      lpc_dormant<T...>* to_lpc_dormant(lpc_dormant<T...> *tail) && {
        upcxx::current_persona().undischarged_n_ -= 1;
        return detail::make_lpc_dormant(*target_, progress_level::user, std::move(fn_), tail);
      }
      
      void operator()(T ...vals) {
        target_->lpc_ff(
          detail::lpc_bind<Fn,T...>(static_cast<Fn&&>(fn_), static_cast<T&&>(vals)...)
        );
        upcxx::current_persona().undischarged_n_ -= 1;
      }
    };
    
    template<typename Event, typename Fn, typename ...T>
    struct cx_state<rpc_cx<Event,Fn>, std::tuple<T...>> {
      Fn fn_;
      
      cx_state(rpc_cx<Event,Fn> &&cx):
        fn_(static_cast<Fn&&>(cx.fn_)) {
      }
      
      typename std::result_of<Fn&&(T&&...)>::type
      operator()(T ...vals) {
        return static_cast<Fn&&>(fn_)(static_cast<T&&>(vals)...);
      }
    };
  }

  //////////////////////////////////////////////////////////////////////
  // detail::completions_state: Constructed against a user-supplied
  // completions<...> value. This is what remembers the actions or
  // holds the promises (need by future returns) and so should probably
  // be heap allocated. To fire all actions registered to an event
  // call `operator()` with the event type as the first template arg.
  //
  // EventPredicate<Event>::value: Maps an event-type to a compile-time
  // bool value for enabling that event in this object. Events which
  // aren't enabled are not copied out of the constructor-time
  // completions<...> instance and execute no-ops under operator().
  //
  // EventValues::future_t<Event>: Maps an event-type to a type-list
  // (as future) which types the values reported by the completed
  // action. `operator()` will expect that the runtime values it receives
  // match the types reported by this map for the given event.
  //
  // ordinal: indexes the nesting depth of this type so that base classes
  // with identical types can be disambiguated.
  namespace detail {
    template<template<typename> class EventPredicate,
             typename EventValues,
             typename Cxs,
             int ordinal=0> 
    struct completions_state /*{
      using completions_t = Cxs;

      // True iff no events contained in `Cxs` are enabled by `EventPredicate`.
      static constexpr bool empty;

      // Fire actions corresponding to `Event` if its enabled. Type-list
      // V... should match the T... in `EventValues::future_t<Event>`.
      template<typename Event, typename ...V>
      void operator()(V&&...);
    }*/;
    
    template<template<typename> class EventPredicate,
             typename EventValues,
             int ordinal>
    struct completions_state<EventPredicate, EventValues,
                             completions<>, ordinal> {

      using completions_t = completions<>;
      static constexpr bool empty = true;
      
      completions_state(completions<>) {}
      
      template<typename Event, typename ...V>
      void operator()(V &&...vals) {/*nop*/}

      struct event_bound {
        template<typename ...V>
        void operator()(V &&...vals) {/*nop*/}
      };
      
      template<typename Event>
      event_bound bind_event() && {
        return event_bound{};
      }

      template<typename Event>
      typename detail::tuple_types_into<
          typename EventValues::template tuple_t<Event>,
          lpc_dormant
        >::type*
      to_lpc_dormant() && {
        return nullptr;
      }
    };

    template<bool event_selected, typename EventValues, typename Cx, int ordinal>
    struct completions_state_head;
    
    template<typename EventValues, typename Cx, int ordinal>
    struct completions_state_head<
        /*event_enabled=*/false, EventValues, Cx, ordinal
      > {
      static constexpr bool empty = true;

      completions_state_head(Cx &&cx) {}
      
      template<typename Event, typename ...V>
      void operator()(V&&...) {/*nop*/}
    };

    template<typename Cx>
    using cx_event_t = typename Cx::event_t;

    template<typename EventValues, typename Cx, int ordinal>
    struct completions_state_head<
        /*event_enabled=*/true, EventValues, Cx, ordinal
      > {
      static constexpr bool empty = false;

      cx_state<Cx, typename EventValues::template tuple_t<cx_event_t<Cx>>> state_;
      
      completions_state_head(Cx &&cx):
        state_(std::move(cx)) {
      }
      
      template<typename ...V>
      void operator_case(std::integral_constant<bool,true>, V &&...vals) {
        // Event matches CxH::event_t
        state_.operator()(std::forward<V>(vals)...);
      }
      template<typename ...V>
      void operator_case(std::integral_constant<bool,false>, V &&...vals) {
        // Event mismatch = nop
      }

      // fire state if Event == CxH::event_t
      template<typename Event, typename ...V>
      void operator()(V &&...vals) {
        this->operator_case(
          std::integral_constant<
            bool,
            std::is_same<Event, typename Cx::event_t>::value
          >{},
          std::forward<V>(vals)...
        );
      }
      
      template<typename Event, typename Lpc>
      Lpc* to_lpc_dormant_case(std::true_type, Lpc *tail) && {
        return std::move(state_).to_lpc_dormant(tail);
      }

      template<typename Event, typename Lpc>
      Lpc* to_lpc_dormant_case(std::false_type, Lpc *tail) && {
        return tail;
      }

      template<typename Event, typename Lpc>
      Lpc* to_lpc_dormant(Lpc *tail) && {
        return std::move(*this).template to_lpc_dormant_case<Event>(
          std::integral_constant<bool,
            std::is_same<Event, typename Cx::event_t>::value
          >(),
          tail
        );
      }
    };
    
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH, typename ...CxT,
             int ordinal>
    struct completions_state<EventPredicate, EventValues,
                             completions<CxH,CxT...>, ordinal>:
        // head base class
        completions_state_head<EventPredicate<typename CxH::event_t>::value,
                               EventValues, CxH, ordinal>,
        // Tail base class. Incrementing the ordinal is essential so that the
        // head bases of this tail base are disambiguated from our head.
        completions_state<EventPredicate, EventValues,
                          completions<CxT...>, ordinal+1> {

      using completions_t = completions<CxH, CxT...>;
      
      using head_t = completions_state_head<
          /*event_enabled=*/EventPredicate<typename CxH::event_t>::value,
          EventValues, CxH, ordinal
        >;
      using tail_t = completions_state<EventPredicate, EventValues,
                                       completions<CxT...>, ordinal+1>;

      static constexpr bool empty = head_t::empty && tail_t::empty;
      
      completions_state(completions<CxH,CxT...> &&cxs):
        head_t(cxs.head_moved()),
        tail_t(cxs.tail_moved()) {
      }
      completions_state(head_t &&head, tail_t &&tail):
        head_t(std::move(head)),
        tail_t(std::move(tail)) {
      }
      
      head_t& head() { return static_cast<head_t&>(*this); }
      head_t const& head() const { return *this; }
      
      tail_t& tail() { return static_cast<tail_t&>(*this); }
      tail_t const& tail() const { return *this; }
      
      template<typename Event, typename ...V>
      void operator()(V &&...vals) {
        // fire the head element
        head_t::template operator()<Event>(
          static_cast<
              // An empty tail means we are the lucky one who gets the
              // opportunity to move-out the given values (if caller supplied
              // reference type permits, thank you reference collapsing).
              typename std::conditional<tail_t::empty, V&&, V const&>::type
            >(vals)...
        );
        // recurse to fire remaining elements
        tail_t::template operator()<Event>(static_cast<V&&>(vals)...);
      }

      template<typename Event>
      struct event_bound {
        template<typename ...V>
        void operator()(completions_state &&me, V &&...vals) {
          // fire the head element
          static_cast<head_t&>(me).template operator()<Event>(std::forward<V>(vals)...);
          // recurse to fire remaining elements
          static_cast<tail_t&>(me).template operator()<Event>(std::forward<V>(vals)...);
        }
      };
      
      template<typename Event>
      typename detail::template bind<event_bound<Event>, completions_state>::return_type
      bind_event() && {
        return upcxx::bind(event_bound<Event>(), std::move(*this));
      }

      template<typename Event>
      typename detail::tuple_types_into<
          typename EventValues::template tuple_t<Event>,
          lpc_dormant
        >::type*
      to_lpc_dormant() && {
        return static_cast<head_t&&>(*this).template to_lpc_dormant<Event>(
          static_cast<tail_t&&>(*this).template to_lpc_dormant<Event>()
        );
      }
    };
  }

  //////////////////////////////////////////////////////////////////////
  // Serialization of a completions_state of rpc_cx's
  
  template<typename EventValues, typename Event, typename Fn, int ordinal>
  struct serialization<
      detail::completions_state_head<
        /*event_enabled=*/true, EventValues, rpc_cx<Event,Fn>, ordinal
      >
    > {
    using type = detail::completions_state_head<true, EventValues, rpc_cx<Event,Fn>, ordinal>;

    static constexpr bool is_serializable = serialization_traits<Fn>::is_serializable;
    
    template<typename Ub>
    static auto ubound(Ub ub, type const &s)
      UPCXX_RETURN_DECLTYPE(
        ub.template cat_ubound_of<Fn>(s.state_.fn_)
      ) {
      return ub.template cat_ubound_of<Fn>(s.state_.fn_);
    }

    template<typename Writer>
    static void serialize(Writer &w, type const &s) {
      w.template write<Fn>(s.state_.fn_);
    }

    using deserialized_type = detail::completions_state_head<
        true, EventValues,
        rpc_cx<Event, typename serialization_traits<Fn>::deserialized_type>,
        ordinal
      >;
    
    static constexpr bool skip_is_fast = serialization_traits<Fn>::skip_is_fast;
    static constexpr bool references_buffer = serialization_traits<Fn>::references_buffer;
    
    template<typename Reader>
    static void skip(Reader &r) {
      r.template skip<Fn>();
    }

    template<typename Reader>
    static deserialized_type* deserialize(Reader &r, void *spot) {
      return new(spot) deserialized_type(r.template read<Fn>());
    }
  };

  template<typename EventValues, typename Cx, int ordinal>
  struct serialization<
      detail::completions_state_head</*event_enabled=*/false, EventValues, Cx, ordinal>
    >:
    detail::serialization_trivial<
      detail::completions_state_head</*event_enabled=*/false, EventValues, Cx, ordinal>,
      /*empty=*/true
    > {
    static constexpr bool is_serializable = true;
  };
  
  template<template<typename> class EventPredicate,
           typename EventValues, int ordinal>
  struct serialization<
      detail::completions_state<EventPredicate, EventValues, completions<>, ordinal>
    >:
    detail::serialization_trivial<
      detail::completions_state<EventPredicate, EventValues, completions<>, ordinal>,
      /*empty=*/true
    > {
    static constexpr bool is_serializable = true;
  };

  template<template<typename> class EventPredicate,
           typename EventValues, typename CxH, typename ...CxT, int ordinal>
  struct serialization<
      detail::completions_state<EventPredicate, EventValues, completions<CxH,CxT...>, ordinal>
    > {
    using type = detail::completions_state<EventPredicate, EventValues, completions<CxH,CxT...>, ordinal>;

    static constexpr bool is_serializable =
      serialization_traits<typename type::head_t>::is_serializable &&
      serialization_traits<typename type::tail_t>::is_serializable;
    
    template<typename Ub>
    static auto ubound(Ub ub, type const &cxs)
      UPCXX_RETURN_DECLTYPE(
        ub.template cat_ubound_of<typename type::head_t>(cxs.head())
          .template cat_ubound_of<typename type::tail_t>(cxs.tail())
      ) {
      return ub.template cat_ubound_of<typename type::head_t>(cxs.head())
               .template cat_ubound_of<typename type::tail_t>(cxs.tail());
    }

    template<typename Writer>
    static void serialize(Writer &w, type const &cxs) {
      w.template write<typename type::head_t>(cxs.head());
      w.template write<typename type::tail_t>(cxs.tail());
    }

    using deserialized_type = detail::completions_state<
        EventPredicate, EventValues,
        completions<typename CxH::deserialized_cx,
                    typename CxT::deserialized_cx...>,
        ordinal
      >;

    static constexpr bool skip_is_fast =
      serialization_traits<typename type::head_t>::skip_is_fast &&
      serialization_traits<typename type::tail_t>::skip_is_fast;
    static constexpr bool references_buffer =
      serialization_traits<typename type::head_t>::references_buffer ||
      serialization_traits<typename type::tail_t>::references_buffer;

    template<typename Reader>
    static void skip(Reader &r) {
      r.template skip<typename type::head_t>();
      r.template skip<typename type::tail_t>();
    }

    template<typename Reader>
    static deserialized_type* deserialize(Reader &r, void *spot) {
      typename type::head_t h = r.template read<typename type::head_t>();
      typename type::tail_t t = r.template read<typename type::tail_t>();
      return new(spot) deserialized_type(std::move(h), std::move(t));
    }
  };

  //////////////////////////////////////////////////////////////////////
  // detail::completions_returner: Manage return type for completions<...>
  // object. Construct one of these instances against a
  // detail::completions_state&. Call operator() to get the return value.

  namespace detail {
    template<template<typename> class EventPredicate,
             typename EventValues, typename Cxs>
    struct completions_returner;

    template<template<typename> class EventPredicate,
             typename EventValues>
    struct completions_returner<EventPredicate, EventValues, completions<>> {
      using return_t = void;

      template<int ordinal>
      completions_returner(
          completions_state<EventPredicate, EventValues, completions<>, ordinal>&
        ) {
      }
      
      void operator()() const {/*return void*/}
    };
    
    template<template<typename> class EventPredicate,
             typename EventValues, typename Cxs,
             typename TailReturn>
    struct completions_returner_head;
    
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH_event, progress_level level, typename ...CxT,
             typename ...TailReturn_tuplees>
    struct completions_returner_head<
        EventPredicate, EventValues,
        completions<future_cx<CxH_event,level>, CxT...>,
        std::tuple<TailReturn_tuplees...>
      > {
      
      using return_t = std::tuple<
          future_from_tuple_t<
            // kind for promise-built future
            detail::future_kind_shref<detail::future_header_ops_promise>,
            typename EventValues::template tuple_t<CxH_event>
          >,
          TailReturn_tuplees...
        >;

      return_t ans_;
      return_t&& operator()() { return std::move(ans_); }
      
      template<typename CxState, typename Tail>
      completions_returner_head(CxState &s, Tail &&tail):
        ans_{
          std::tuple_cat(
            std::make_tuple(s.head().state_.get_future()),
            tail()
          )
        } {
      }
    };

    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH_event, progress_level level, typename ...CxT,
             typename TailReturn_not_tuple>
    struct completions_returner_head<
        EventPredicate, EventValues,
        completions<future_cx<CxH_event,level>, CxT...>,
        TailReturn_not_tuple
      > {
      
      using return_t = std::tuple<
          future_from_tuple_t<
            // kind for promise-built future
            detail::future_kind_shref<detail::future_header_ops_promise>,
            typename EventValues::template tuple_t<CxH_event>
          >,
          TailReturn_not_tuple
        >;
      
      return_t ans_;
      return_t&& operator()() { return std::move(ans_); }
      
      template<typename CxState, typename Tail>
      completions_returner_head(CxState &s, Tail &&tail):
        ans_(
          std::make_tuple(
            s.head().state_.get_future(),
            tail()
          )
        ) {
      }
    };

    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH_event, progress_level level, typename ...CxT>
    struct completions_returner_head<
        EventPredicate, EventValues,
        completions<future_cx<CxH_event,level>, CxT...>,
        void
      > {
      
      using return_t = future_from_tuple_t<
          // kind for promise-built future
          detail::future_kind_shref<detail::future_header_ops_promise>,
          typename EventValues::template tuple_t<CxH_event>
        >;
      
      return_t ans_;
      return_t&& operator()() { return std::move(ans_); }
      
      template<typename CxState, typename Tail>
      completions_returner_head(CxState &s, Tail&&):
        ans_(
          s.head().state_.get_future()
        ) {
      }
    };
    
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH_not_future, typename ...CxT,
             typename TailReturn>
    struct completions_returner_head<
        EventPredicate, EventValues,
        completions<CxH_not_future, CxT...>,
        TailReturn
      >:
      completions_returner<
          EventPredicate, EventValues, completions<CxT...>
        > {
      
      template<typename CxState>
      completions_returner_head(
          CxState&,
          completions_returner<
            EventPredicate, EventValues, completions<CxT...>
          > &&tail
        ):
        completions_returner<
            EventPredicate, EventValues, completions<CxT...>
          >{std::move(tail)} {
      }
    };
    
    template<template<typename> class EventPredicate,
             typename EventValues, typename CxH, typename ...CxT>
    struct completions_returner<EventPredicate, EventValues,
                                completions<CxH,CxT...>>:
      completions_returner_head<
        EventPredicate, EventValues,
        completions<CxH,CxT...>,
        typename completions_returner<
            EventPredicate, EventValues, completions<CxT...>
          >::return_t
      > {

      template<int ordinal>
      completions_returner(
          completions_state<
              EventPredicate, EventValues, completions<CxH,CxT...>, ordinal
            > &s
        ):
        completions_returner_head<
          EventPredicate, EventValues,
          completions<CxH,CxT...>,
          typename completions_returner<
              EventPredicate, EventValues, completions<CxT...>
            >::return_t
        >{s,
          completions_returner<
              EventPredicate, EventValues, completions<CxT...>
            >{s.tail()}
        } {
      }
    };
  }
}
#endif

