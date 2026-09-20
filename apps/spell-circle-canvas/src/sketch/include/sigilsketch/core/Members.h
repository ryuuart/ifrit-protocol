#pragma once

/** @file
 * @ingroup sketch-core
 *
 * A SKETCH'S OWN MEMBERS, AS CALLABLES: what the one prefix search in
 * the tree is asked about when a body names fewer parameters than a host
 * offers it.
 */

#include <sigilcore/callable/Callable.h>

#include <utility>

namespace sigil::sketch::detail {

/** `body.setup(…)` AS A CALLABLE.
 *
 *  A member function is not a value, and the prefix search asks what a
 *  CALLABLE can be invoked with, so it cannot be pointed at a member
 *  directly. This is the one wrapper that makes the member something it
 *  can be asked about — and, for a type that spells no such member at
 *  all, the answer is that it can be invoked with nothing, which is how
 *  a misspelling is caught rather than silently accepted.
 *
 *  Both surfaces use it: `setup` is handed the canvas context in one and
 *  the set's in the other, and the wrapper names neither. */
template <class Body>
struct SetupCall {
  Body& body;
  template <class... Offered>
    requires requires(Body& sketch, Offered&&... offered) {
      sketch.setup(std::forward<Offered>(offered)...);
    }
  decltype(auto) operator()(Offered&&... offered) const {
    return body.setup(std::forward<Offered>(offered)...);
  }
};

/** `body.update(…)` as a callable: the canvas surface's per-frame call. */
template <class Body>
struct UpdateCall {
  Body& body;
  template <class... Offered>
    requires requires(Body& sketch, Offered&&... offered) {
      sketch.update(std::forward<Offered>(offered)...);
    }
  decltype(auto) operator()(Offered&&... offered) const {
    return body.update(std::forward<Offered>(offered)...);
  }
};

/** `body.describe(…)` as a callable: the set surface's whole frame. */
template <class Body>
struct DescribeCall {
  Body& body;
  template <class... Offered>
    requires requires(Body& sketch, Offered&&... offered) {
      sketch.describe(std::forward<Offered>(offered)...);
    }
  decltype(auto) operator()(Offered&&... offered) const {
    return body.describe(std::forward<Offered>(offered)...);
  }
};

}  // namespace sigil::sketch::detail
