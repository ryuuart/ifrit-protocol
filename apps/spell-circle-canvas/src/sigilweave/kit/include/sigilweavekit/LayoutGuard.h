#pragma once

/** @file
 * Paragraph-aware layout memoization. Re-breaking lines every frame is the
 * single most common invisible cost in animated SigilWeave use: paint-only
 * restyles (setPaint, shader swaps, marker repaints) never require a
 * relayout, so a scene that guards its layoutParagraph() call renders a
 * screenful of text for the cost of one layout, not sixty a second.
 *
 * The hand-rolled version of that guard has a correctness trap: its key must
 * include the paragraph's revision *and* its needsShaping() flag, and
 * forgetting either silently freezes live edits. LayoutGuard bakes both in;
 * callers declare only the inputs the library cannot see (canvas size,
 * alignment, break strategy, a quantized measure).
 */

#include <sigilweave/Paragraph.h>

#include <cmath>
#include <concepts>
#include <cstdint>
#include <tuple>
#include <utility>

namespace sigil::weave::kit {

/**
 * Fires a relayout callable when a paragraph's content or the declared
 * layout inputs change. The guard owns no layout storage — the callable
 * writes wherever the caller keeps its ParagraphLayout(s), which is what
 * lets one guard cover multi-layout rebuilds (e.g. the same paragraph broken
 * two ways side by side):
 *
 * ```
 * LayoutGuard<SkISize, TextAlignment> m_guard;
 * ...
 * m_guard.ensure(m_paragraph, {size, params.alignment}, [&] {
 *   m_layout = layoutParagraph(fontContext, m_paragraph, flow, options);
 * });
 * ```
 *
 * The paragraph's revision is recorded *after* the callable runs, so the
 * shaping performed inside the relayout does not re-trigger the guard. If
 * the callable never actually lays this paragraph out, needsShaping() stays
 * set and the guard fires every call — a fail-toward-correct default.
 */
template <typename... Keys> class LayoutGuard {
public:
  /** Runs `relayoutFn` when the paragraph edited/needs shaping, the keys
   *  changed, or nothing was laid out yet. Returns true when it ran. */
  template <typename RelayoutFn>
    requires std::invocable<RelayoutFn &>
  bool ensure(const sigil::weave::Paragraph &paragraph, std::tuple<Keys...> keys,
              RelayoutFn &&relayoutFn) {
    if (m_valid && !paragraph.needsShaping() &&
        paragraph.revision() == m_revision && keys == m_keys)
      return false;
    relayoutFn();
    m_keys = std::move(keys);
    m_revision = paragraph.revision();
    m_valid = true;
    return true;
  }

  /** Forces the next ensure() to relayout regardless of its inputs. */
  void invalidate() { m_valid = false; }

private:
  std::tuple<Keys...> m_keys;
  uint64_t m_revision = 0;
  bool m_valid = false;
};

/** Snaps `value` to whole multiples of `step`.
 *
 * The companion trick for animated layout inputs: a breathing measure or
 * drifting box moves well under a pixel per frame, so quantizing it before
 * it enters a guard key means most frames pose the *same* layout problem
 * and re-hit the cache — and sub-pixel measure changes are invisible
 * anyway. Pick `step` as the coarsest granularity the effect can't show.
 */
[[nodiscard]] inline float quantize(float value, float step = 1.0f) {
  return std::round(value / step) * step;
}

} // namespace sigil::weave::kit
