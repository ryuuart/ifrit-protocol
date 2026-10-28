#pragma once

/** @file
 * @ingroup weave-kit
 *
 * Paragraph-aware layout memoization: a paint-only restyle never
 * requires a relayout, so a scene that guards its layoutParagraph() call
 * renders a screenful of text for the cost of one layout. The guard's
 * key must include the paragraph's revision AND its needsShaping() flag,
 * and forgetting either silently freezes live edits; LayoutGuard bakes
 * both in and callers declare only the inputs it cannot see.
 */

#include <sigilcore/memo/Rebuild.h>
#include <sigilweave/paragraph/Paragraph.h>

#include <concepts>
#include <cstdint>
#include <tuple>
#include <utility>

/** THE DISCIPLINE A CONSUMER OF THE ENGINE NEEDS, which the engine
 *  itself will not impose: the layout guard, the glyph-bucket
 *  accumulator, the hyphenation and line-edge tables the layout asks for
 *  and holds no opinion about, the OpenType feature presets, the
 *  one-call label draw, and deterministic sample content. Every piece is
 *  a stock value or a helper a consumer could have written, none of it
 *  on the path the engine takes on its own, and it is a separate
 *  interface target, so a consumer that wants none of it links none. */
namespace sigil::weave::kit {

/** Fires a relayout callable when a paragraph's content or the declared
 * layout inputs change. It is SigilCore's rebuild guard over the declared
 * keys, invalidated whenever the paragraph was edited or needs shaping,
 * so the paragraph's two inputs are never a key a caller can forget. The
 * guard owns no layout storage: the callable writes wherever the caller
 * keeps its layouts, which is what lets one guard cover a multi-layout
 * rebuild. The paragraph's revision is recorded AFTER the callable runs.
 * @trap A callable that never lays this paragraph out leaves
 * `Paragraph::needsShaping` set, and the guard then fires every call.
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
    if (paragraph.needsShaping() || paragraph.revision() != m_revision)
      m_guard.invalidate();
    if (!m_guard.ensure(std::move(keys), relayoutFunction)) return false;
    m_revision = paragraph.revision();
    return true;
  }

  /** Forces the next ensure() to relayout regardless of its inputs. */
  void invalidate() { m_guard.invalidate(); }

 private:
  core::RebuildGuard<Keys...> m_guard;
  uint64_t m_revision = 0;
};

}  // namespace sigil::weave::kit
