#pragma once
/** @file
 * THE TWO SUBSTITUTION GATES a dressed glyph can ask for: a driven
 * variable-font axis, and a code-point swap.
 *
 * Both replace what the SHAPER decided while keeping the pen positions it
 * computed, so both are refused wherever honouring them would move a
 * letter — an axis that moves advances, a replacement whose advance
 * differs along the axis its run advances on. Both memoize their verdict,
 * because a verdict is a property of the face and probing one costs
 * metrics calls; the axis half reads `detail::axisGate`, so every place
 * that judges a driven axis reads one verdict.
 */

#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <include/core/SkTypes.h>

namespace sigil::weave {
class FontContext;
struct FontVariation;
}  // namespace sigil::weave

namespace sigil::compose::detail {

/** The face a driven axis asks for, or null when the gate refuses it — the
 *  glyph then draws at its shaped face, which is the whole refusal.
 *
 *  Off a continuous track the coordinate is snapped to a ladder cut by
 *  rendered size and the clone is memoized, so the faces a scene can reach
 *  are bounded and each is rasterized once. On one, the coordinate passes
 *  through raw and the clone is transient. */
sk_sp<SkTypeface> drivenFace(sigil::weave::FontContext& fonts,
                             const sk_sp<SkTypeface>& base, float pixelSize,
                             const sigil::weave::FontVariation& axis,
                             bool continuous);

/** The glyph @p codepoint asks for in @p face, or 0 when the gate refuses
 *  it — the original then draws, which is the whole refusal. */
SkGlyphID substituteGlyph(sigil::weave::FontContext& fonts,
                          const sk_sp<SkTypeface>& face, SkGlyphID original,
                          char32_t codepoint, bool vertical);

}  // namespace sigil::compose::detail
