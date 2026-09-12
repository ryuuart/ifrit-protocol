/** @file
 * THE WALK THAT DECIDES WHICH BEAT each glyph falls in, at each level of
 * a nested cascade, and the algebra that composes the deviations those
 * beats produce: offsets and rotations add, scale and alpha multiply,
 * substitutions take the last one, and the lerp a crossfade and a
 * keyframe table both run. The arithmetic over the beat NUMBERS is
 * SigilMotion's cascade; what is here is how a paragraph's units are
 * numbered against it and what a glyph does with the answer.
 */

#include <sigilcompose/typography/TextEffect.h>
#include <sigilcore/compute/Intervals.h>

#include <algorithm>
#include <cstring>
#include <tuple>
#include <vector>

#include "TextEngine.h"

namespace sigil::compose {

namespace detail {

void TrackCascade::build(const Track& track, const GlyphStructure& structure,
                         const std::vector<uint8_t>& selected) {
  const motion::Spread& spec = track.stagger;
  const auto count = (uint32_t)structure.glyphs.size();
  // THE STORY'S NUMBERING where this leaf is one frame of a chain, the
  // leaf's own everywhere else. A cascade over a threaded story runs one
  // clock across the whole of it: the fortieth word is beat forty wherever
  // it landed, so a stagger does not restart at each frame. The lanes are
  // empty for an ordinary leaf, which is then numbered exactly as it always
  // was.
  const bool story = !structure.storyUnitOf[(size_t)track.unit].empty();
  const std::vector<uint32_t>& outerLane =
      story ? structure.storyUnitOf[(size_t)track.unit]
            : structure.unitOf[(size_t)track.unit];
  outerUnit.assign(count, 0);
  uint32_t outerCount = 0;
  if (track.beatsOver == Beats::Text) {
    // THE PARAGRAPH'S OWN NUMBERING, which is the whole point of the
    // setting: a unit's beat does not depend on which of its glyphs this
    // track happens to address, so two tracks that split one paragraph run
    // one clock however differently their selections resolve.
    for (uint32_t g = 0; g < count; ++g) outerUnit[g] = outerLane[g];
    outerCount = story ? structure.storyUnitCounts[(size_t)track.unit]
                       : structure.unitCounts[(size_t)track.unit];
  } else {
    // Renumber the units the SELECTION covers, from 0, in draw order — then
    // a stagger's From, its amount-mode division and its distribution all
    // read the count the author sees rather than the paragraph's.
    uint32_t previous = ~0u;
    for (uint32_t g = 0; g < count; ++g) {
      if (!selected[g]) continue;
      if (outerLane[g] != previous) {
        previous = outerLane[g];
        ++outerCount;
      }
      outerUnit[g] = outerCount - 1;
    }
  }

  uint32_t innerCount = 0;
  if (spec.inner) {
    // The nested level is numbered against the same list as the outer one:
    // one setting governs the cascade, so a nested beat cannot be counted
    // one way at the top and another underneath.
    const bool overText = track.beatsOver == Beats::Text;
    const std::vector<uint32_t>& innerLane =
        structure.unitOf[(size_t)track.innerUnit];
    innerUnit.assign(count, 0);
    uint32_t within = 0, previousOuter = ~0u, previousInner = ~0u;
    for (uint32_t g = 0; g < count; ++g) {
      if (!overText && !selected[g]) continue;
      if (outerUnit[g] != previousOuter) {
        previousOuter = outerUnit[g];
        previousInner = ~0u;
        within = 0;
      }
      if (innerLane[g] != previousInner) {
        previousInner = innerLane[g];
        ++within;
      }
      innerUnit[g] = within - 1;
      innerCount = std::max(innerCount, within);
    }
  } else {
    innerUnit.clear();
  }
  cascade.build(spec, outerCount, innerCount);
}

// ---------------------------------------------------------------------------
// Composition

/** FIELD PIN. `compose()` and `lerpModifier()` below are hand-written
 * exhaustive lists over GlyphModifier's members, and so is the routing decision
 * in TextFxPainting.cpp that sends a glyph down the matrix path. All three fail
 * the same way when a field is added and one of them is not told: silently, by
 *  drawing the deviation of some other track or some other moment. */
void glyphModifierFieldPin(GlyphModifier& v) {
  auto& [dx, dy, scale, rotateDeg, alpha, colorMultiplier, colorAdd,
         colorScreen, scaleX, scaleY, skewXDeg, skewYDeg, axis, codepoint] = v;
  static_assert(
      std::tuple_size_v<decltype(std::tie(dx, dy, scale, rotateDeg, alpha,
                                          colorMultiplier, colorAdd,
                                          colorScreen, scaleX, scaleY, skewXDeg,
                                          skewYDeg, axis, codepoint))> == 14,
      "GlyphModifier gained or lost a field — rule on it in compose() and "
      "lerpModifier() below, in appendKeyParameters() (a field a keys table "
      "cannot "
      "spell makes two different tables compare equal), and in the matrix "
      "ROUTING in TextFxPainting.cpp (a field an RSXform cannot carry has to "
      "send "
      "its glyph down the matrix path), then bump this count.");
}

void compose(GlyphModifier& into, const GlyphModifier& next) {
  into.dx += next.dx;
  into.dy += next.dy;
  into.rotateDeg += next.rotateDeg;
  into.skewXDeg += next.skewXDeg;
  into.skewYDeg += next.skewYDeg;
  into.scale *= next.scale;
  into.scaleX *= next.scaleX;
  into.scaleY *= next.scaleY;
  into.alpha *= next.alpha;
  into.colorMultiplier = {into.colorMultiplier.fR * next.colorMultiplier.fR,
                          into.colorMultiplier.fG * next.colorMultiplier.fG,
                          into.colorMultiplier.fB * next.colorMultiplier.fB,
                          into.colorMultiplier.fA * next.colorMultiplier.fA};
  // The additive term ADDS unclamped — the clamp happens once, at the draw,
  // so two half flashes make one full one rather than each clamping alone.
  into.colorAdd = {
      into.colorAdd.fR + next.colorAdd.fR, into.colorAdd.fG + next.colorAdd.fG,
      into.colorAdd.fB + next.colorAdd.fB, into.colorAdd.fA + next.colorAdd.fA};
  // The screen term SCREENS: 1 − (1−a)(1−b), commutative and associative,
  // so stacked glows compose in any track order and never leave [0,1].
  const auto screen = [](float a, float b) { return 1 - (1 - a) * (1 - b); };
  into.colorScreen = {screen(into.colorScreen.fR, next.colorScreen.fR),
                      screen(into.colorScreen.fG, next.colorScreen.fG),
                      screen(into.colorScreen.fB, next.colorScreen.fB),
                      screen(into.colorScreen.fA, next.colorScreen.fA)};
  // The two SUBSTITUTIONS are last-one-wins rather than combined: two
  // tracks naming two outlines for one glyph have no arithmetic between
  // them, and averaging their numbers would draw a third thing neither
  // asked for.
  if (next.axis) into.axis = next.axis;
  if (next.codepoint) into.codepoint = next.codepoint;
}

GlyphModifier lerpModifier(const GlyphModifier& a, const GlyphModifier& b,
                           float w) {
  GlyphModifier out;
  out.dx = a.dx + (b.dx - a.dx) * w;
  out.dy = a.dy + (b.dy - a.dy) * w;
  out.rotateDeg = a.rotateDeg + (b.rotateDeg - a.rotateDeg) * w;
  out.skewXDeg = a.skewXDeg + (b.skewXDeg - a.skewXDeg) * w;
  out.skewYDeg = a.skewYDeg + (b.skewYDeg - a.skewYDeg) * w;
  out.scale = a.scale + (b.scale - a.scale) * w;
  out.scaleX = a.scaleX + (b.scaleX - a.scaleX) * w;
  out.scaleY = a.scaleY + (b.scaleY - a.scaleY) * w;
  out.alpha = a.alpha + (b.alpha - a.alpha) * w;
  out.colorMultiplier = {
      a.colorMultiplier.fR + (b.colorMultiplier.fR - a.colorMultiplier.fR) * w,
      a.colorMultiplier.fG + (b.colorMultiplier.fG - a.colorMultiplier.fG) * w,
      a.colorMultiplier.fB + (b.colorMultiplier.fB - a.colorMultiplier.fB) * w,
      a.colorMultiplier.fA + (b.colorMultiplier.fA - a.colorMultiplier.fA) * w};
  // The two colour terms lerp componentwise like every other continuous
  // field — a flash decays through straight interpolation of its own
  // channels, not through the compose() arithmetic, which is for stacking.
  const auto lerpColor = [w](const SkColor4f& x, const SkColor4f& y) {
    return SkColor4f{x.fR + (y.fR - x.fR) * w, x.fG + (y.fG - x.fG) * w,
                     x.fB + (y.fB - x.fB) * w, x.fA + (y.fA - x.fA) * w};
  };
  out.colorAdd = lerpColor(a.colorAdd, b.colorAdd);
  out.colorScreen = lerpColor(a.colorScreen, b.colorScreen);
  // An axis coordinate is the one substitution with a continuum: two
  // phases driving the SAME axis blend their values and the face
  // interpolates between them. Everything else CUTS at the middle of the
  // window — there is no half-way glyph between two outlines, and lerping
  // a code point would draw whatever letter happened to sit between them
  // in the font's encoding.
  out.axis = a.axis;
  if (a.axis && b.axis && std::memcmp(a.axis->tag, b.axis->tag, 4) == 0)
    out.axis->value = a.axis->value + (b.axis->value - a.axis->value) * w;
  else if (w >= 0.5f)
    out.axis = b.axis;
  out.codepoint = w >= 0.5f ? b.codepoint : a.codepoint;
  return out;
}

uint64_t glyphSeed(const GlyphInfo& g, uint32_t lane) {
  // The glyph's identity, and only that: its position in the walk and the
  // text offset it came from. Both hold across relayouts while the text is
  // unchanged, which is what makes a seeded scatter cacheable. `lane`
  // separates the operands of a composite, so mixing two scatters gives two
  // scatters rather than one drawn twice — and it is the OPERAND's index,
  // never a counter, so a phase draws the same numbers whether it is being
  // crossfaded into or playing alone.
  return mix64Value(((uint64_t)g.textIndex << 32u) ^ (uint64_t)g.index) +
         mix64Value(lane);
}

}  // namespace detail

}  // namespace sigil::compose
