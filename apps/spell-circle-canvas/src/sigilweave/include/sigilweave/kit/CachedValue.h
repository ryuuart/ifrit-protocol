#pragma once

/** @file
 * A value owned together with its rebuild guard: expensive derived objects
 * (a flattened path, a gradient shader) that must not be rebuilt per frame
 * are returned from ensure() and rebuilt only when their declared keys
 * change.
 */

#include <concepts>
#include <tuple>
#include <utility>

#include "sigilweave/kit/RebuildGuard.h"

namespace sigil::weave::kit {

/**
 * A value owned together with its rebuild guard: `ensure()` returns the
 * cached value, rebuilding it from the callable only when the keys change.
 * The canonical use is expensive Sk objects derived from the canvas size —
 * a flattened path, a gradient shader — that must not be rebuilt per frame:
 *
 * ```
 * CachedValue<SkPath, SkISize> m_ring;
 * ...
 * const SkPath &ring = m_ring.ensure({size}, [&] { return makeRing(size); });
 * ```
 */
template <typename Value, typename... Keys>
class CachedValue {
 public:
  /** Returns the cached value, rebuilding it via `buildFn` when the keys
   *  differ from the last build. */
  template <typename BuildFn>
    requires std::invocable<BuildFn&> &&
             std::convertible_to<std::invoke_result_t<BuildFn&>, Value>
  Value& ensure(std::tuple<Keys...> keys, BuildFn&& buildFn) {
    m_guard.ensure(std::move(keys), [&] { m_value = buildFn(); });
    return m_value;
  }

  /** Keyless build-once convenience for lazily constructed values. */
  template <typename BuildFn>
    requires(sizeof...(Keys) == 0) && std::invocable<BuildFn&> &&
            std::convertible_to<std::invoke_result_t<BuildFn&>, Value>
  Value& ensure(BuildFn&& buildFn) {
    return ensure(std::tuple<>{}, std::forward<BuildFn>(buildFn));
  }

  /** Forces the next ensure() to rebuild regardless of its keys. */
  void invalidate() { m_guard.invalidate(); }

  [[nodiscard]] Value& value() { return m_value; }
  [[nodiscard]] const Value& value() const { return m_value; }

 private:
  Value m_value{};
  RebuildGuard<Keys...> m_guard;
};

}  // namespace sigil::weave::kit
