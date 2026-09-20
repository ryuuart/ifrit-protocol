#pragma once

/** @file
 * @ingroup weave-kit
 *
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

#include <sigilweave/paragraph/Paragraph.h>

#include <concepts>
#include <cstdint>
#include <tuple>
#include <utility>

/** THE DISCIPLINE A CONSUMER OF THE ENGINE NEEDS, which the engine
 *  itself will not impose: the layout guard that keeps a scene from
 *  re-breaking its lines every frame, the glyph-bucket accumulator that
 *  keeps per-glyph animation to a handful of draw calls, the hyphenation
 *  and line-edge tables the layout asks for and holds no opinion about,
 *  the OpenType feature presets, the one-call label draw, and
 *  deterministic sample content.
 *
 *  Every piece here is a stock value or a helper a consumer could have
 *  written; none of it is on the path the engine takes on its own. It is
 *  a separate interface target, so a consumer that wants none of it links
 *  none of it. */
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
template <typename... Keys>
class LayoutGuard {
 public:
  /** Runs `relayoutFunction` when the paragraph edited/needs shaping, the keys
   *  changed, or nothing was laid out yet. Returns true when it ran. */
  template <typename RelayoutFunction>
    requires std::invocable<RelayoutFunction&>
  bool ensure(const sigil::weave::Paragraph& paragraph,
              std::tuple<Keys...> keys, RelayoutFunction&& relayoutFunction) {
    if (m_valid && !paragraph.needsShaping() &&
        paragraph.revision() == m_revision && keys == m_keys)
      return false;
    relayoutFunction();
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

}  // namespace sigil::weave::kit
