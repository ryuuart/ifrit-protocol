#pragma once

/** @file
 * Keyed rebuild guards — the "compare the inputs, rebuild on change" caches
 * every animated TextFlow consumer ends up hand-rolling (a paragraph rebuilt
 * when its text/typeface/size change, an SkPath or SkShader rebuilt when the
 * canvas resizes). Hand-rolled copies smear the cache key across `m_last*`
 * members and an if-condition that must stay in sync with them; forgetting
 * one member is invisible until something animates stale. These guards make
 * the key a single declared tuple and take the variable part — what
 * rebuilding *means* — as a callable.
 */

#include <concepts>
#include <tuple>
#include <utility>

namespace textflowkit {

/**
 * Tracks a tuple of rebuild inputs and fires a callable when they change.
 *
 * Declare every input that should invalidate the cached state as a template
 * parameter, then pass the current values to ensure() each frame:
 *
 * ```
 * RebuildGuard<std::u16string, const SkTypeface *, float> m_guard;
 * ...
 * m_guard.ensure({text, typeface.get(), fontSize}, [&] {
 *   m_paragraph.clear();
 *   m_paragraph.appendText(text, style);
 * });
 * ```
 *
 * Key types need `==` and copy/move; compare identity-carrying handles like
 * `sk_sp<SkTypeface>` by raw pointer so the guard tracks *which* object, not
 * a reference count. The callable runs before the key is stored, so a throw
 * leaves the guard invalid and the rebuild retries on the next call.
 */
template <typename... Keys> class RebuildGuard {
public:
  /** Runs `buildFn` when the keys differ from the last successful build (or
   *  nothing was built yet). Returns true when rebuilding occurred. */
  template <typename BuildFn>
    requires std::invocable<BuildFn &>
  bool ensure(std::tuple<Keys...> keys, BuildFn &&buildFn) {
    if (m_built && keys == m_keys)
      return false;
    buildFn();
    m_keys = std::move(keys);
    m_built = true;
    return true;
  }

  /** Forces the next ensure() to rebuild regardless of its keys. */
  void invalidate() { m_built = false; }

  /** Returns whether a build has completed since construction/invalidate. */
  [[nodiscard]] bool built() const { return m_built; }

private:
  std::tuple<Keys...> m_keys;
  bool m_built = false;
};

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
template <typename Value, typename... Keys> class CachedValue {
public:
  /** Returns the cached value, rebuilding it via `buildFn` when the keys
   *  differ from the last build. */
  template <typename BuildFn>
    requires std::invocable<BuildFn &> &&
             std::convertible_to<std::invoke_result_t<BuildFn &>, Value>
  Value &ensure(std::tuple<Keys...> keys, BuildFn &&buildFn) {
    m_guard.ensure(std::move(keys), [&] { m_value = buildFn(); });
    return m_value;
  }

  /** Keyless build-once convenience for lazily constructed values. */
  template <typename BuildFn>
    requires(sizeof...(Keys) == 0) && std::invocable<BuildFn &> &&
            std::convertible_to<std::invoke_result_t<BuildFn &>, Value>
  Value &ensure(BuildFn &&buildFn) {
    return ensure(std::tuple<>{}, std::forward<BuildFn>(buildFn));
  }

  /** Forces the next ensure() to rebuild regardless of its keys. */
  void invalidate() { m_guard.invalidate(); }

  [[nodiscard]] Value &value() { return m_value; }
  [[nodiscard]] const Value &value() const { return m_value; }

private:
  Value m_value{};
  RebuildGuard<Keys...> m_guard;
};

} // namespace textflowkit
