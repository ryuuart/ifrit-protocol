#pragma once

/** @file
 * SigilCompose typography — THE EFFECT AS A VALUE: what a body sees for
 * one glyph (`GlyphInfo`), the deviation from rest it returns (`GlyphMod`),
 * the callable those two make (`GlyphModFn`), the comparable value one is
 * wrapped in (`TextEffect`, with `Phase` for a sequence) — and nothing
 * else. The effects the runtime evaluates by STRUCTURE are the catalogue
 * beside this file, <sigilcompose/typography/TextFx.h>; the stock presets
 * that are plain values over the seam are the kit's, in
 * <sigilcompose/kit/Kinetic.h>.
 *
 * Everything on the seam is a COMPARABLE VALUE. That is not decoration:
 * the reconciler prunes a re-described element only when it can prove the
 * description is the same one, and a `std::function` cannot be compared.
 * A preset carries its name and its parameters; an ad-hoc lambda carries
 * the key its author gave it.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPoint.h>
#include <sigilcore/compute/Noise.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/BoundFloat.h>
#include <sigilweave/style/ShapingStyle.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sigil::compose {

/** What an effect sees for one glyph.
 *
 *  Enumeration order is stable across relayouts while the text is
 *  unchanged, and every index here is a fact about the glyph's place in
 *  THIS layout of THIS text — which is what lets an effect address "the
 *  third letter of its word" or "everything on line two" without the
 *  author counting glyphs by hand. */
struct GlyphInfo {
  size_t index = 0;    ///< glyph position in the paragraph
  size_t count = 1;    ///< total glyphs
  SkPoint rest;        ///< the glyph's laid-out origin (pen position)
  float advance = 0;   ///< the glyph's advance width
  float fontSize = 0;  ///< the glyph's font size (em-relative effects)

  uint32_t glyphInWord = 0;     ///< index of this glyph within its word
  uint32_t wordGlyphCount = 1;  ///< glyphs in that word
  uint32_t cluster = 0;         ///< UTF-16 cluster offset inside its run;
                                ///< a base and its marks share one value
  uint32_t textIndex = 0;       ///< that cluster as an offset into the text
  uint32_t wordIndex = 0;       ///< index of its word in the paragraph
  uint32_t lineIndex = 0;       ///< the flow line it landed on
  /** Index of its style span in the materialized paragraph.
   *
   *  A NUMBER FOR AN EFFECT TO READ, NOT A HANDLE TO ADDRESS BY: spans are
   *  cut and merged by every span restyle the leaf declares, so this
   *  renumbers when a `spanPaint` anywhere ahead of it splits one — and the
   *  restyle resolver runs while that list is being edited, so the two
   *  resolvers could not be made to agree on what a given index names. The
   *  handle on a treatment is the NAME the run was written under, which
   *  `sel::style` addresses and which only new content changes. */
  uint32_t styleIndex = 0;
  uint32_t sentenceIndex = 0;  ///< 0-based sentence
  /** Which beat of the track's own stagger this glyph belongs to, and how
   *  many beats there are — the unit numbering the track resolved, not a
   *  paragraph-wide count. A per-word track sees word ordinals here. */
  uint32_t unitIndex = 0;
  uint32_t unitCount = 1;
};

/** One glyph's deviation from rest — what an effect returns for local
 *  progress t ∈ [0,1]. alpha 0 skips the glyph entirely.
 *
 *  This is the type the composition algebra operates on: stacked tracks,
 *  `fx::mix`, a `fx::seq` crossfade and a `fx::keys` segment all combine
 *  GlyphMods the same way — dx/dy, rotateDeg, skewXDeg and skewYDeg ADD;
 *  scale, scaleX, scaleY, alpha and colorMul MULTIPLY; colorAdd ADDS and
 *  colorScreen SCREENS, each channelwise; and the two
 *  SUBSTITUTIONS, `axis` and
 *  `codepoint`, are last-one-wins. Substitutions do not blend because
 *  there is no half-way glyph between two outlines: a later track that
 *  names one replaces what an earlier one named, and a `fx::seq`
 *  crossfade cuts them at the middle of its window rather than lerping.
 *  (An axis coordinate is the exception inside a crossfade: two phases
 *  driving the SAME axis lerp their values, because the face does have a
 *  continuum between them.) */
struct GlyphMod {
  float dx = 0, dy = 0;
  float scale = 1;  ///< uniform; multiplies scaleX and scaleY below
  float rotateDeg = 0;
  float alpha = 1;
  /** A per-channel multiplier over EVERY pass the glyph's style draws. A
   *  pass painting a flat colour multiplies it; a pass painting a shader
   *  takes the equivalent modulation, so a gradient keeps its ramp and
   *  wears the tint over it. White is no tint. */
  SkColor4f colorMul = {1, 1, 1, 1};
  /** A per-channel ADDITIVE term over every pass the glyph's style draws —
   *  the hard flash a multiplier cannot say, because a multiplier only
   *  moves a colour toward black and moves a zero channel not at all. The
   *  glyph's painted colour becomes `colour·colorMul + colorAdd`, then
   *  `colorScreen` below, clamped to [0,1] at the draw. Adds ACROSS TRACKS
   *  channelwise and clamps once at the draw, so two half flashes make one
   *  full one. RGB only: the alpha component rides the algebra but reaches
   *  no draw — coverage is the multiplicative lane's (`alpha`,
   *  `colorMul.fA`). Zero is no flash and costs nothing. */
  SkColor4f colorAdd = {0, 0, 0, 0};
  /** A per-channel SCREEN term — the painted colour c becomes
   *  1 − (1 − c)(1 − colorScreen) — the phosphor glow that lifts each
   *  channel in proportion to its headroom and never clips. Screens
   *  COMMUTATIVELY across tracks (1 − (1−a)(1−b) reads the same both
   *  ways), so stacked glows compose order-free; applied after `colorMul`
   *  and `colorAdd`, which is what makes one colour-matrix carry all
   *  three. RGB only, as `colorAdd` is. Zero is no glow and costs
   *  nothing. */
  SkColor4f colorScreen = {0, 0, 0, 0};
  /** Non-uniform scale and shear on each axis (degrees). An RSXform encodes
   *  a rotation and ONE scale and no shear at all, so a glyph whose composed
   *  deviation uses any of these draws under its own matrix — same passes,
   *  same paint, one canvas concat — while its neighbours keep the shared
   *  transform array.
   *
   *  The two angles read as `Element::skewX` and `Element::skewY` do:
   *  positive `skewXDeg` leans the top toward −x, positive `skewYDeg`
   *  pushes the right side toward +y, and a glyph naming both takes the
   *  single shear pair `(tan x, tan y)` rather than one shear applied after
   *  the other. */
  float scaleX = 1, scaleY = 1;
  float skewXDeg = 0, skewYDeg = 0;
  /** A variable-font axis coordinate, applied at DRAW time by swapping the
   *  glyph's face for a varied clone. The shaped positions are reused as
   *  they are, so this is sound only for an ADVANCE-INVARIANT axis: the
   *  runtime probes the glyph's face once per axis and REFUSES one that
   *  moves advances, drawing the glyph at its shaped face instead (GRAD is
   *  the advance-invariant weight; wght moves advances on most faces and
   *  belongs in the shaping style, which re-shapes). Unset: the shaped
   *  face. */
  std::optional<sigil::weave::FontVariation> axis;
  /** Draw a different code point in this glyph's place, resolved through
   *  the glyph's own shaped font and drawn at the original's pen position.
   *  Sound only for an EQUAL-ADVANCE replacement — a proportional
   *  substitution would move every letter after it and needs a reshape, not
   *  a redraw — so the runtime measures both and refuses the ones that
   *  differ, drawing the original. 0 is no substitution. */
  char32_t codepoint = 0;
};

/** The raw callable behind an effect: (glyph, local progress, random
 *  stream) → deviation. Wrap one in a named `TextEffect` — the seam never
 *  holds a bare function, because a bare function cannot be compared.
 *
 *  THE STREAM IS SEEDED FROM THE GLYPH'S IDENTITY, so the same letter of
 *  the same text draws the same numbers on every frame and across every
 *  relayout — a scatter that is stable is a scatter that can be cached,
 *  and one reseeded per frame would jitter forever and never settle. It
 *  is reseeded fresh for each glyph, so an effect may draw as many values
 *  as it likes without the sequence depending on how many its neighbours
 *  drew. */
using GlyphModFn =
    std::function<GlyphMod(const GlyphInfo&, float, core::noise::Mix64Stream&)>;

/** THE EFFECT, as a comparable value: a name, its parameters, and any
 *  operand effects it was built from.
 *
 *  Two effects are equal when they carry the same name, the same
 *  parameters, equal operands and the same curves — so `fx::rise(26) ==
 *  fx::rise(26)`,
 *  `fx::rise(26) != fx::rise(30)`, and a `fx::seq` of equal phases equals
 *  another built the same way. That equality is what lets a re-described
 *  element with unchanged tracks PRUNE instead of re-recording every
 *  frame.
 *
 *  The name is a promise about the body: equal values must produce
 *  identical deviations for identical inputs. Two different lambdas given
 *  one key compare equal and one of them will silently never be used. */
class TextEffect {
 public:
  TextEffect() = default;
  /** A named effect over parameters. `reach` is how far, in pixels, this
   *  effect may push a glyph outside the element's box — the number the
   *  recording cull grows by, so a wide scatter is not truncated.
   *
   *  `curves` are the easing functions the body reads, carried on the value
   *  so they reach the comparison: a named curve is compared by identity and
   *  a lambda compares UNEQUAL, which is the rule every other curve slot in
   *  the library follows. Leaving a curve out of this list would make an
   *  effect that reshapes its motion compare equal to the one it replaced,
   *  and the reconciler would keep drawing the old one.
   *
   *  `displaces` is the placement fact below — true unless the body provably
   *  leaves every glyph on its pen position. */
  TextEffect(std::string name, std::vector<float> params, GlyphModFn fn,
             float reach, std::vector<choreograph::EaseFn> curves = {},
             bool displaces = true) {
    auto state = std::make_shared<State>();
    state->name = std::move(name);
    state->params = std::move(params);
    state->curves = std::move(curves);
    state->fn = std::move(fn);
    state->reach = reach;
    state->displaces = displaces;
    m_state = std::move(state);
  }

  /** A VARIABLE-FONT AXIS held at one coordinate for every glyph the track
   *  addresses — a grade, an optical size, a slant applied at draw time
   *  with no reshape. The effect the kernel builds for itself: an
   *  `Element::spanStyle` that changes only such axes is carried as a
   *  track holding it.
   *
   *  Only an ADVANCE-INVARIANT axis is honoured: the glyphs keep the pen
   *  positions shaping gave them, so an axis that moves advances would
   *  leave them sitting wrong. The runtime probes the face once per axis
   *  and refuses one that does, drawing at the shaped face and warning
   *  once — GRAD is the advance-invariant weight most faces carry, while
   *  wght belongs in the shaping style, which re-shapes. */
  static TextEffect variableAxis(const char (&tag)[5], float value) {
    const sigil::weave::FontVariation coordinate(tag, value);
    return TextEffect(
        "variableAxis",
        {(float)(unsigned char)tag[0], (float)(unsigned char)tag[1],
         (float)(unsigned char)tag[2], (float)(unsigned char)tag[3], value},
        [coordinate](const GlyphInfo&, float, core::noise::Mix64Stream&) {
          GlyphMod m;
          m.axis = coordinate;
          return m;
        },
        // An advance-invariant axis leaves every pen position where the
        // layout put it, so the effect does not displace.
        0.0f, {}, /*displaces=*/false);
  }

  /** Evaluates the deviation. An empty effect answers the identity. */
  GlyphMod operator()(const GlyphInfo& g, float t,
                      core::noise::Mix64Stream& rng) const {
    return m_state && m_state->fn ? m_state->fn(g, t, rng) : GlyphMod{};
  }
  explicit operator bool() const { return m_state && (bool)m_state->fn; }
  /** Pixels beyond the element's box this effect may paint. */
  [[nodiscard]] float reach() const { return m_state ? m_state->reach : 0.0f; }
  [[nodiscard]] const std::string& name() const {
    static const std::string kEmpty;
    return m_state ? m_state->name : kEmpty;
  }
  [[nodiscard]] std::span<const float> params() const {
    return m_state ? std::span<const float>(m_state->params)
                   : std::span<const float>();
  }
  [[nodiscard]] std::span<const TextEffect> operands() const {
    return m_state ? std::span<const TextEffect>(m_state->operands)
                   : std::span<const TextEffect>();
  }

  /** Two effects are the same effect when their identity — the preset name
   *  or the author's key, the parameters, the operands, the reach they
   *  reserve, the pass material and the named curves — is; a lambda-valued
   *  curve compares unequal, conservatively, through SigilMotion's
   *  `easeEqual`.
   *
   *  THE REACH IS PART OF IT because it is what the painter reserves and
   *  culls against: one key handed a wider reach is a body that paints
   *  further out, and comparing equal would prune it onto the narrower
   *  reserve and clip the output it was widened for. */
  bool operator==(const TextEffect& other) const {
    if (m_state == other.m_state) return true;  // copies of one value
    if (!m_state || !other.m_state) return false;
    if (m_state->name != other.m_state->name ||
        m_state->params != other.m_state->params ||
        m_state->reach != other.m_state->reach ||
        m_state->operands != other.m_state->operands)
      return false;
    // A pass compares by its MATERIAL, by value — the paint's own recipe
    // equality, so two passes over one source with equal constants prune,
    // and a live pass material never compares equal, conservatively.
    if ((m_state->pass != nullptr) != (other.m_state->pass != nullptr))
      return false;
    if (m_state->pass && !(*m_state->pass == *other.m_state->pass))
      return false;
    if (m_state->curves.size() != other.m_state->curves.size()) return false;
    for (size_t i = 0; i < m_state->curves.size(); ++i)
      if (!motion::easeEqual(m_state->curves[i], other.m_state->curves[i]))
        return false;
    return true;
  }

  /** A phase of `fx::seq` ending at local `t` — `a.until(0.35f)`. */
  [[nodiscard]] class Phase until(float t) const;

  /** Builds a composite (`fx::seq`, `fx::mix`) — the operands ride the
   *  value so the result compares by structure. `displaces` is the fact
   *  DERIVED from those operands: a composite moves its glyphs when any
   *  operand it may evaluate does. */
  static TextEffect composite(std::string name, std::vector<float> params,
                              std::vector<TextEffect> operands, GlyphModFn fn,
                              float reach, bool displaces) {
    auto state = std::make_shared<State>();
    state->name = std::move(name);
    state->params = std::move(params);
    state->operands = std::move(operands);
    state->fn = std::move(fn);
    state->reach = reach;
    state->displaces = displaces;
    TextEffect out;
    out.m_state = std::move(state);
    return out;
  }

  /** A PASS EFFECT: the track's evaluation is one shader pass over the
   *  addressed units' rendered pixels, not a per-glyph deviation — the
   *  factory behind `fx::pass` below, where the contract is
   *  documented. The material must be RECIPE-BACKED
   *  (`material::skia::Paint::recipe`)
   *  over a recipe with an SkSL body, because the runtime bakes the unit
   *  count into a specialization of that recipe; any other material warns
   *  once and returns an EMPTY effect, so the track draws its glyphs at
   *  rest. */
  static TextEffect pass(material::skia::Paint material);
  /** The pass material, or null for every per-glyph effect — what the
   *  runtime dispatches on. */
  [[nodiscard]] const material::skia::Paint* passMaterial() const {
    return m_state ? m_state->pass.get() : nullptr;
  }

  /** DECLARES A PHASE WHERE THIS PASS IS AN EXACT PASS-THROUGH — an
   *  author's promise the runtime spends but cannot verify, in the same
   *  family as `isAnimated`, `bleed()` and `reach`. When every unit the
   *  track addresses sits at a declared phase, the runtime skips the layer
   *  and the shader and draws the glyphs directly. The contract, and what
   *  a false promise looks like, is documented at `fx::pass` below.
   *  Pass effects only: on any other effect this warns once and returns
   *  the effect unchanged. The declaration rides the effect's params, so
   *  it participates in equality as every parameter does. */
  [[nodiscard]] TextEffect restsAt(float phase) const;
  /** Both ends: `fx::pass(m).restsAt(0, 1)`. */
  [[nodiscard]] TextEffect restsAt(float a, float b) const;
  /** The declared pass-through phases — empty when none were declared,
   *  and for every per-glyph effect. */
  [[nodiscard]] std::span<const float> restPhases() const {
    return m_state && m_state->pass ? std::span<const float>(m_state->params)
                                    : std::span<const float>();
  }

  /** DOES THIS EFFECT MOVE ITS GLYPHS OFF THEIR PEN POSITIONS? A glyph mask
   *  is rasterized for a QUANTIZED origin, so a run whose letters creep by a
   *  fraction of a pixel per frame does not creep at all on whole pixels:
   *  each letter stands still until its own origin crosses a pixel boundary
   *  and then hops a whole one. A track whose effect answers true and whose
   *  progress is live puts its run's placement on the finer subpixel grid,
   *  which is the only placement that can express the creep.
   *
   *  It is a fact about the DEVIATION, not about the schedule: an effect
   *  that only fades, tints, substitutes a code point or holds a variable
   *  axis leaves every pen position exactly where the layout put it, and a
   *  run under it keeps whole-pixel origins however hard its progress is
   *  running. Offsets, rotation, shear and scale are what move a glyph. */
  [[nodiscard]] bool displaces() const { return m_state && m_state->displaces; }

  /** DECLARES THE FACT ABOVE for a body the library cannot read — the one
   *  knob `fx::effect` needs, since an ad-hoc lambda's deviation is opaque
   *  until it runs. It defaults to TRUE, so an undeclared body is placed
   *  smoothly; `fx::effect(key, body).displacing(false)` is the author's
   *  promise that the body never moves a glyph, and the cost of a false
   *  promise is exactly the tick the grid exists to remove.
   *
   *  Every effect the library builds ANSWERS FOR ITSELF and needs no call
   *  here: a preset knows its own deviation, `fx::keys` reads its table, and
   *  `fx::seq`, `fx::mix` and `fx::hold` derive from their operands. The
   *  declaration rides the effect's params, so two bodies under one key that
   *  disagree about placement compare unequal and re-patch.
   *
   *  A PASS is not a placement: `fx::pass` runs its shader over pixels that
   *  were already rasterized at the glyphs' resting origins, so refining
   *  those origins says nothing about where the shader puts its output.
   *  Calling this on a pass warns once and returns the effect unchanged. */
  [[nodiscard]] TextEffect displacing(bool moves) const;

 private:
  struct State {
    std::string name;
    std::vector<float> params;
    std::vector<TextEffect> operands;
    std::vector<choreograph::EaseFn> curves;
    GlyphModFn fn;
    float reach = 0;
    /** Whether the body moves glyphs off their pen positions — see
     *  displaces(). True is the safe answer, so it is the default. */
    bool displaces = true;
    /** Set only by pass(): the material run over the units' layer. Held by
     *  pointer so an effect that is not a pass carries nothing; it rides
     *  equality by VALUE (the paint's operator==), like an Effect child. */
    std::shared_ptr<const material::skia::Paint> pass;
  };
  /** restsAt()'s one body: appends the phases to the pass's params — a
   *  pass carries no other parameters, so its params slot IS the rest
   *  declaration and the phases join equality with no second clause. */
  [[nodiscard]] TextEffect withRests(std::initializer_list<float> phases) const;
  std::shared_ptr<const State> m_state;
};

/** One phase of a `fx::seq`: an effect, where it ends in local time, and
 *  how long it crossfades into whatever follows. */
class Phase {
 public:
  Phase(TextEffect e)  // NOLINT: implicit by design (seq(a.until(…), b))
      : m_effect(std::move(e)) {}
  Phase(TextEffect e, float endsAt)
      : m_effect(std::move(e)), m_endsAt(endsAt) {}
  /** Lerp this phase's deviation into the next one's over the last
   *  `fraction` of local time before the joint. Default is a hard cut. */
  Phase& xfade(float fraction) {
    m_overlap = fraction;
    return *this;
  }
  [[nodiscard]] const TextEffect& effect() const { return m_effect; }
  [[nodiscard]] float endsAt() const { return m_endsAt; }
  [[nodiscard]] float overlap() const { return m_overlap; }
  bool operator==(const Phase&) const = default;

 private:
  TextEffect m_effect;
  float m_endsAt = 1.0f;
  float m_overlap = 0.0f;
};

inline Phase TextEffect::until(float t) const { return Phase(*this, t); }

}  // namespace sigil::compose
