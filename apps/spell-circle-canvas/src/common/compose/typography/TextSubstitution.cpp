/** @file
 * The two substitution gates, and the ladder the driven axis snaps to.
 * TextSubstitution.h states what each answers and why it may refuse.
 */

#include "TextSubstitution.h"

#include <include/core/SkTypes.h>  // SkDebugf — the library's one channel
#include <sigilweave/fonts/FontContext.h>

#include <boost/container/flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cmath>
#include <tuple>

#include "AxisGate.h"
#include "TextPose.h"

namespace sigil::compose::detail {

// ---------------------------------------------------------------------------
// The two SUBSTITUTION gates a dressed glyph can ask for — a driven
// variable-font axis, and a code-point swap. Both replace what the SHAPER
// decided while keeping the pen positions it computed, so both are refused
// wherever that would move a letter, and both memoize their verdict: a
// verdict is a property of the face, and probing one costs metrics calls.
// The axis gate is `detail::axisGate`, shared so every place that judges a
// driven axis reads one verdict.

/** How many coordinates the driven-axis ladder offers a glyph rendered at
 *  `pixelSize`. Four steps per pixel of em, between 64 and 512.
 *
 *  A driven coordinate snapped to this lands on a bounded set of FACES:
 *  every distinct coordinate is a distinct clone, a distinct batch bucket
 *  and a distinct set of glyph-atlas strikes, so the ceiling is what makes
 *  the retained clone population bounded at all. It is coarser than the
 *  tangent's because an axis step displaces an outline within the letter,
 *  where a rotation step sweeps its far edge. */
int axisLadderSteps(float pixelSize) {
  return ladderSteps(pixelSize, 4.0f, 64, 512);
}

/** The face a driven axis asks for, or null when the gate refuses it — the
 *  glyph then draws at its shaped face, which is the whole refusal.
 *
 *  Off a continuous track the coordinate is snapped to the ladder above and
 *  the clone is memoized, so the faces a scene can reach are bounded and
 *  each is rasterized once. ON one, the coordinate passes through raw and
 *  the clone is TRANSIENT: an unsnapped value has no bounded set to memoize,
 *  so retaining it would add a permanently held clone per frame for as long
 *  as the process runs. The price of the opt-out is therefore a fresh face
 *  and fresh glyph rasterization every frame — constant per frame, and
 *  exactly what "continuous" is asking for. */
sk_sp<SkTypeface> drivenFace(sigil::weave::FontContext& fonts,
                             const sk_sp<SkTypeface>& base, float pixelSize,
                             const sigil::weave::FontVariation& axis,
                             bool continuous) {
  const char tag[5] = {axis.tag[0], axis.tag[1], axis.tag[2], axis.tag[3], 0};
  const detail::AxisGate& gate = detail::axisGate(fonts, base, tag);
  if (!gate.allowed) return nullptr;
  sigil::weave::FontVariation coordinate = axis;
  coordinate.value = std::clamp(axis.value, gate.min, gate.max);
  if (gate.max > gate.min) {
    if (continuous)
      return fonts.variedTypefaceTransient(base, {&coordinate, 1});
    const float steps = (float)axisLadderSteps(pixelSize);
    const float span = gate.max - gate.min;
    coordinate.value =
        gate.min + std::round((coordinate.value - gate.min) / span * steps) *
                       (span / steps);
  }
  // A degenerate range offers one reachable coordinate, which is a ladder of
  // one whether or not the track asked for a ladder.
  return fonts.variedTypeface(base, {&coordinate, 1});
}

/** The glyph a code-point substitution resolves to, or 0 when it is
 *  refused.
 *
 *  A substitution draws its replacement at the ORIGINAL glyph's pen
 *  position, so it is sound exactly when the two advance the pen equally
 *  ALONG THE AXIS THAT PEN STEPS ON: a level run steps by the horizontal
 *  advance, an upright column by the vertical one. Reading the wrong axis
 *  refuses a kana-to-digit churn down a column whose glyphs all step one em
 *  down it, and admits a pair that really would shift the column below the
 *  swap. A mismatch on the measured axis is a reshape and not a redraw, so
 *  it is refused.
 *
 *  Both advances are a property of the FACE — em fractions, not pixels — so
 *  one probe answers for every size the pair is ever drawn at, and BOTH
 *  axes are read on that one probe. The axis therefore stays out of the
 *  memo key: a pair's replacement glyph is the same glyph whichever way the
 *  run flows, and only the verdict differs, so the entry carries a verdict
 *  per axis and the lookup picks the one the run asked for. Keying on the
 *  axis instead would probe the same pair twice for two facts one probe
 *  already has. */
SkGlyphID substituteGlyph(sigil::weave::FontContext& fonts,
                          const sk_sp<SkTypeface>& face, SkGlyphID original,
                          char32_t codepoint, bool vertical) {
  if (!face) return 0;
  struct Verdict {
    SkGlyphID replacement = 0;       ///< 0: the face cannot draw the code point
    bool alike[2] = {false, false};  ///< indexed by the axis: level, upright
  };
  using Key = std::tuple<uint32_t, SkGlyphID, uint32_t>;
  static thread_local boost::container::flat_map<Key, Verdict> table;
  auto [entry, fresh] = table.try_emplace(
      Key{face->uniqueID(), original, (uint32_t)codepoint}, Verdict{});
  Verdict& verdict = entry->second;
  if (fresh) {
    verdict.replacement = face->unicharToGlyph((SkUnichar)(uint32_t)codepoint);
    for (int axis = 0; verdict.replacement && axis < 2; ++axis) {
      // A thousandth of the em: no face's equal-advance pair misses it and
      // no proportional pair meets it.
      constexpr float kAdvanceEpsilonEm = 0.001f;
      const bool down = axis == 1;
      verdict.alike[axis] =
          std::abs(fonts.glyphAdvanceEm(face, original, down) -
                   fonts.glyphAdvanceEm(face, verdict.replacement, down)) <=
          kAdvanceEpsilonEm;
    }
  }
  if (verdict.replacement == 0) return 0;
  const int axis = vertical ? 1 : 0;
  if (verdict.alike[axis]) return verdict.replacement;
  // Once per face and axis: a scramble over a proportional charset would
  // otherwise report every character of it, one line each.
  static thread_local boost::unordered_flat_set<uint64_t> warned;
  if (warned.insert(((uint64_t)face->uniqueID() << 1u) | (uint64_t)axis).second)
    SkDebugf(
        "sigilcompose fx: a code-point substitution on this font is "
        "proportional %s — refused (the replacement is drawn at the "
        "original's pen position, so a different advance would move every "
        "letter after it; substitute within an equal-advance charset, or "
        "change the text and re-shape)\n",
        vertical ? "down a column" : "along a line");
  return 0;
}

}  // namespace sigil::compose::detail
