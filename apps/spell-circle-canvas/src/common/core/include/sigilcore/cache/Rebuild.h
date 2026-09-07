#pragma once

/** @file
 * Rebuild on change — the "compare the inputs, rebuild when they differ"
 * cache every animated consumer otherwise hand-rolls.
 *
 * A hand-rolled copy smears the key across a row of `m_last*` members and
 * an if-condition that has to stay in step with them; forgetting one
 * member is invisible until something animates stale. These make the key
 * one declared tuple and take the variable part — what rebuilding MEANS —
 * as a callable.
 *
 * Nothing here knows what is being rebuilt, so no drawing, layout or
 * device vocabulary appears: the guard holds keys, the value holds
 * whatever the callable answers, and `quantizeKey` is the one arithmetic
 * a key wants — snapping an input that drifts by a fraction per frame so
 * most frames pose the same problem and hit.
 */

#include <cmath>
#include <concepts>
#include <tuple>
#include <utility>

namespace sigil::core {

/**
 * Tracks a tuple of rebuild inputs and fires a callable when they change.
 *
 * Declare every input that should invalidate the cached state as a
 * template parameter, then pass the current values to `ensure()`:
 *
 * ```
 * RebuildGuard<std::u16string, const SkTypeface*, float> m_guard;
 * m_guard.ensure({text, typeface.get(), fontSize}, [&] { rebuild(); });
 * ```
 *
 * Key types need `==` and copy/move; compare an identity-carrying handle
 * by raw pointer so the guard tracks WHICH object rather than a reference
 * count. The callable runs before the key is stored, so a throw leaves
 * the guard invalid and the rebuild retries on the next call.
 */
template <class... Keys>
class RebuildGuard {
 public:
  /** Runs @p build when the keys differ from the last successful build,
   *  or when nothing has been built yet. True when a rebuild happened. */
  template <class Build>
    requires std::invocable<Build&>
  bool ensure(std::tuple<Keys...> keys, Build&& build) {
    if (m_built && keys == m_keys) return false;
    build();
    m_keys = std::move(keys);
    m_built = true;
    return true;
  }

  /** Forces the next `ensure()` to rebuild whatever its keys say. */
  void invalidate() { m_built = false; }

  /** Whether a build has completed since construction or `invalidate()`. */
  [[nodiscard]] bool built() const { return m_built; }

 private:
  std::tuple<Keys...> m_keys;
  bool m_built = false;
};

/**
 * A value owned together with its guard: `ensure()` answers the held
 * value, rebuilding it from the callable only when the keys change. The
 * case it is for is an expensive derived object keyed on something that
 * moves — a flattened path or a gradient keyed on a size — which must not
 * be rebuilt per frame:
 *
 * ```
 * CachedValue<SkPath, SkISize> m_ring;
 * const SkPath& ring = m_ring.ensure({size}, [&] { return makeRing(size); });
 * ```
 */
template <class Value, class... Keys>
class CachedValue {
 public:
  /** The held value, rebuilt through @p build when the keys differ from
   *  the last build. */
  template <class Build>
    requires std::invocable<Build&> &&
             std::convertible_to<std::invoke_result_t<Build&>, Value>
  Value& ensure(std::tuple<Keys...> keys, Build&& build) {
    m_guard.ensure(std::move(keys), [&] { m_value = build(); });
    return m_value;
  }

  /** The keyless build-once form, for a value that is merely lazy. */
  template <class Build>
    requires(sizeof...(Keys) == 0) && std::invocable<Build&> &&
            std::convertible_to<std::invoke_result_t<Build&>, Value>
  Value& ensure(Build&& build) {
    return ensure(std::tuple<>{}, std::forward<Build>(build));
  }

  /** Forces the next `ensure()` to rebuild whatever its keys say. */
  void invalidate() { m_guard.invalidate(); }

  [[nodiscard]] Value& value() { return m_value; }
  [[nodiscard]] const Value& value() const { return m_value; }

 private:
  Value m_value{};
  RebuildGuard<Keys...> m_guard;
};

/** @p value snapped to whole multiples of @p step, for an input on its way
 *  into a key. An animated measure or a drifting box moves well under a
 *  pixel per frame: snapped, most frames pose the same problem and hit,
 *  and a sub-pixel difference was invisible anyway. @p step is the
 *  coarsest granularity the effect cannot show. */
[[nodiscard]] inline float quantizeKey(float value, float step = 1.0f) {
  return std::round(value / step) * step;
}

}  // namespace sigil::core
