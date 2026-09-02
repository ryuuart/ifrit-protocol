#pragma once

/** @file
 * SigilCompose text model — the multi-track per-glyph seam (Unit, Selector,
 * TextEffect, Stagger, Track, and Beat, a resolved cascade read back) and
 * RichText, the mixed-style value the text() factory takes. The stock
 * effects that plug the seam live in <sigilcompose/TextFx.h>.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Erased.h>
#include <sigilcompose/core/Motion.h>
#include <sigilcore/compute/Noise.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/layout/PositionedRun.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/style/PaintStyle.h>
#include <sigilweave/style/Style.h>

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::weave {
class FontContext;
}  // namespace sigil::weave

class SkCanvas;

namespace sigil::compose {

class Material;
struct PaintContext;
struct TextPath;
namespace detail {
struct Instance;
}  // namespace detail

// ---------------------------------------------------------------------------
// TEXT FX — the multi-track per-glyph seam (presets in
// <sigilcompose/TextFx.h>)
//
// A text() element carries an ordered list of TRACKS. One track is
// (selector, effect, stagger, progress): WHICH glyphs it addresses, WHAT
// deviation from rest it asks of them, HOW their start times spread, and
// the master 0→1 that drives the whole thing. Several tracks on one
// element compose per glyph — offsets and rotations ADD, scale and alpha
// MULTIPLY — so a rise, a per-word wobble and a colour-independent fade
// are three independent values rather than one hand-merged lambda.
//
// Everything on the seam is a COMPARABLE VALUE. That is not decoration:
// the reconciler prunes a re-described element only when it can prove the
// description is the same one, and a `std::function` cannot be compared.
// A preset carries its name and its parameters; an ad-hoc lambda carries
// the key its author gave it.

/** The granularity a selector slices and a stagger beats over.
 *
 *  `Cluster` is the default and the one that keeps text correct: a base
 *  letter and its combining marks, or the several glyphs an emoji
 *  sequence shapes to, are ONE cluster and move together. `Glyph` is the
 *  raw shaping unit and will separate those marks from what they sit on. */
enum class Unit : uint8_t { Glyph, Cluster, Word, Line, Sentence };

/** The granularity names, spelled the way tracks read: `unit::Word`. */
namespace unit {
inline constexpr Unit Glyph = Unit::Glyph;
inline constexpr Unit Cluster = Unit::Cluster;
inline constexpr Unit Word = Unit::Word;
inline constexpr Unit Line = Unit::Line;
inline constexpr Unit Sentence = Unit::Sentence;
}  // namespace unit

/** A deterministic generator, handed to every effect.
 *
 *  Seeded from the GLYPH's identity, so the same letter of the same text
 *  draws the same random numbers on every frame and across every
 *  relayout — a scatter that is stable is a scatter that can be cached,
 *  and one reseeded per frame would jitter forever and never settle.
 *  Reseeded fresh for each glyph, so an effect may draw as many values as
 *  it likes without the sequence depending on how many its neighbours
 *  drew.
 *
 *  Not a cryptographic generator and not a substitute for one. */
class Rng {
 public:
  explicit Rng(uint64_t seed) : m_state(seed) {}
  /** The next 32 bits: a splitmix64 stream — the counter steps by the
   *  gamma and the stepped value goes through the avalanche, whose HIGH
   *  half is the word handed back. */
  uint32_t bits() {
    m_state += core::noise::kMix64Gamma;
    return (uint32_t)(core::noise::mix64(m_state) >> 32u);
  }
  /** The next value in [0, 1). */
  float unit() { return (float)(bits() >> 8u) * (1.0f / 16777216.0f); }
  /** The next value in [-1, 1). */
  float signedUnit() { return unit() * 2.0f - 1.0f; }
  /** The next value in [lo, hi). */
  float range(float lo, float hi) { return lo + unit() * (hi - lo); }

 private:
  uint64_t m_state;
};

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

/** The raw callable behind an effect: (glyph, local progress, rng) →
 *  deviation. Wrap one in a named `TextEffect` — the seam never holds a
 *  bare function, because a bare function cannot be compared. */
using GlyphModFn = std::function<GlyphMod(const GlyphInfo&, float, Rng&)>;

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
             bool displaces = true);

  /** A VARIABLE-FONT AXIS held at one coordinate for every glyph the track
   *  addresses — a grade, an optical size, a slant applied at draw time
   *  with no reshape. The kernel's own effect: an `Element::spanStyle`
   *  that changes only such axes is carried as a track holding it.
   *
   *  Only an ADVANCE-INVARIANT axis is honoured: the glyphs keep the pen
   *  positions shaping gave them, so an axis that moves advances would
   *  leave them sitting wrong. The runtime probes the face once per axis
   *  and refuses one that does, drawing at the shaped face and warning
   *  once — GRAD is the advance-invariant weight most faces carry, while
   *  wght belongs in the shaping style, which re-shapes. */
  static TextEffect variableAxis(const char (&tag)[5], float value);

  /** Evaluates the deviation. An empty effect answers the identity. */
  GlyphMod operator()(const GlyphInfo& g, float t, Rng& rng) const {
    return m_state && m_state->fn ? m_state->fn(g, t, rng) : GlyphMod{};
  }
  explicit operator bool() const { return m_state && (bool)m_state->fn; }
  /** Pixels beyond the element's box this effect may paint. */
  [[nodiscard]] float reach() const { return m_state ? m_state->reach : 0.0f; }
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] std::span<const float> params() const;
  [[nodiscard]] std::span<const TextEffect> operands() const;

  bool operator==(const TextEffect& other) const;

  /** A phase of `fx::seq` ending at local `t` — `a.until(0.35f)`. */
  [[nodiscard]] class Phase until(float t) const;

  /** Builds a composite (`fx::seq`, `fx::mix`) — the operands ride the
   *  value so the result compares by structure. `displaces` is the fact
   *  DERIVED from those operands: a composite moves its glyphs when any
   *  operand it may evaluate does. */
  static TextEffect composite(std::string name, std::vector<float> params,
                              std::vector<TextEffect> operands, GlyphModFn fn,
                              float reach, bool displaces);

  /** A PASS EFFECT: the track's evaluation is one shader pass over the
   *  addressed units' rendered pixels, not a per-glyph deviation — the
   *  factory behind `fx::pass` below, where the contract is
   *  documented. The material must be RECIPE-BACKED (`Material::recipe`)
   *  over a recipe with an SkSL body, because the runtime bakes the unit
   *  count into a specialization of that recipe; any other material warns
   *  once and returns an EMPTY effect, so the track draws its glyphs at
   *  rest. */
  static TextEffect pass(Material material);
  /** The pass material, or null for every per-glyph effect — what the
   *  runtime dispatches on. */
  [[nodiscard]] const Material* passMaterial() const;

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
  [[nodiscard]] std::span<const float> restPhases() const;

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
  [[nodiscard]] bool displaces() const;

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
     *  pointer because Material is declared below this class; it rides
     *  equality by VALUE (Material::operator==), like an Effect child. */
    std::shared_ptr<const Material> pass;
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

// ---------------------------------------------------------------------------
// The effects the RUNTIME evaluates by structure rather than by calling a
// preset's body: the substitution, the shader pass, the keyframe table and
// the combinators over whole effects. Their bodies are kernel code; the
// presets that are plain values live in TextFx.h.

namespace fx {

/** THE DECODING TEXT: every glyph churns through `charset` before landing
 *  on the letter the text actually says.
 *
 *  A substitution keeps the ORIGINAL glyph's pen position, so it is only
 *  honoured where the replacement has the original's advance — the runtime
 *  measures both and refuses the rest, drawing the true letter. That makes
 *  a monospaced face the natural home for this, and a charset of
 *  same-width characters the way to get it out of a proportional one.
 *
 *  Each glyph resolves at its own seeded moment and all of them have
 *  resolved by t = 1. `steps` is how many times a glyph re-rolls across its
 *  whole local time; the churn is seeded from the glyph's identity, so it
 *  is the same churn on every frame. */
[[nodiscard]] TextEffect scramble(
    std::u32string charset = U"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",
    int steps = 14);

/** A SHADER PASS AS A TRACK'S EFFECT — "a shader per letter" without a
 *  shader per letter. The track's units are rendered ONCE into a layer and
 *  @p material runs once over that layer, handed each unit's box and each
 *  unit's own cascade clock as uniform data, so per-letter treatment is
 *  data rather than scene structure and the cost is one draw plus one pass
 *  whatever the unit count is.
 *
 *      struct Burn { material::Color uEdge; };
 *      auto dissolve = std::make_shared<const material::Recipe>(
 *          material::Recipe::of<Burn>("ember.burn")
 *              .body(material::Target::SkSL, kBurnSksl));
 *      auto burn = Material::recipe(material::Material(dissolve, Burn{ink}));
 *      text(u8"EMBER DECODE", display)
 *          .fx({.effect = fx::pass(burn),
 *               .stagger = stagger(unit::Cluster, {.eachMs = 260})});
 *
 *  THE MATERIAL MUST BE RECIPE-BACKED (`Material::recipe`) over a recipe
 *  with an SkSL body, because the unit count is baked into the compiled
 *  shader — a runtime effect's array size is fixed at compile and SkSL has
 *  no uniform-bounded loop. The RUNTIME owns that specialization: it holds
 *  a second recipe over the same params, per distinct unit count, whose
 *  body is
 *
 *      uniform shader uContent;        // the units' rendered layer
 *      uniform float4 uUnitRect[N];    // per unit: x, y, w, h
 *      uniform float2 uUnitPhase[N];   // per unit: local 0→1, seed
 *      const int kUnitCount = N;      // the loop bound
 *
 *  ahead of the author's — write the body against those names and do not
 *  declare them, and put every uniform of your own in the params struct
 *  rather than in the body's text. The params ARE the ABI: a field the
 *  body never reads is named on stderr rather than silently dropped. A
 *  body that does not compile is reported once against its recipe's name,
 *  with the compiler's own message — whose LINE NUMBERS count from the
 *  head of the specialization, the generated declarations and the four
 *  above them, not from the first line you wrote. Any other material warns
 *  once and returns an EMPTY effect, so the track draws its glyphs at rest
 *  rather than nothing.
 *
 *  THE COORDINATES ARE THE NODE'S OWN PX: `main(xy)`, `uUnitRect` and the
 *  layer all share the frame the letters were laid out in, and the layer
 *  is sampled at the device's resolution, so a 2x host stays sharp with no
 *  supersampled bake. `uUnitRect` entries are the SAME rects
 *  `Composer::beatsOf` reports (that query lifts them to composer space);
 *  `uUnitPhase[i].x` is the same `Beat::localT`, driven by the track's
 *  progress through its cascade, so the pass, a mark and the glyphs can
 *  never disagree about the schedule. `uUnitPhase[i].y` is a per-unit seed
 *  in [1, 256), stable across frames and relayouts, which is what lets a
 *  seeded dissolve settle and cache instead of churning forever. Under a
 *  nested cascade there is one entry per (outer, inner) beat, matching the
 *  beats the query reports.
 *
 *  THE PASS IS BOUNDED, unlike a raw `Element::effect` shader: it paints
 *  the node's box grown by the track's `reach` and nothing outside it. An
 *  effect built here declares the material's `bleed()` as its reach, so a
 *  pass that marks beyond the letters says how far on the value that
 *  paints, or on the track.
 *
 *  ORDER AGAINST DEVIATION TRACKS on the same node: deviations apply
 *  FIRST. The layer holds the addressed glyphs as every deviation track
 *  left them — risen, scattered, tinted — and the pass reads those pixels,
 *  because a pass is post-processing and pixels are what it processes. A
 *  glyph addressed by a pass draws only inside that pass's layer, never
 *  directly as well; several pass tracks run in declaration order, each
 *  over its own selection's layer, and a glyph two passes address renders
 *  in both. A path baseline and a vertical column place glyphs before any
 *  of this, so a pass rides both, with unit rects turned the way the
 *  layout turned the letters.
 *
 *  A pass is a WHOLE-TRACK statement: inside `fx::seq`, `fx::mix` or
 *  `fx::hold` its material is not consulted and the operand contributes
 *  the identity. Sequence a pass by driving its progress; gate its onset
 *  in its own SkSL, which holds the whole schedule.
 *
 *  THE DECLARED REST — `fx::pass(m).restsAt(0)`, `.restsAt(1)`,
 *  `.restsAt(0, 1)`: the author's promise that the SkSL is an EXACT
 *  pass-through at those unit phases — at a declared phase it returns its
 *  input pixels untouched. When every addressed unit's resolved local time
 *  sits on a declared phase, the runtime skips the layer and the shader
 *  and draws the batches directly, so a pass on a node that repaints for
 *  unrelated reasons (an orbiting `onPath` ring under a settled pass)
 *  stops paying for a shader that is changing nothing. The promise is
 *  UNVERIFIABLE, in the same family as `Track::reach` and a material's
 *  `bleed()`: declare a phase where the shader is not a pass-through and
 *  the picture POPS at the seam, snapping between shaded and raw glyphs as
 *  the schedule crosses the declared phase, with no diagnostic. The
 *  comparison is exact — which the schedule supplies, a one-shot cascade
 *  clamping a unit to exactly 0 before its beat and exactly 1 after. Under
 *  a LOOPING cascade (`Stagger::loopMs`) a unit touches 0 only at the
 *  instant its beat re-opens, so `restsAt(0)` effectively never engages
 *  there — correctly, the cycle is always mid-flight somewhere — while a
 *  unit RESTS at exactly 1 between beats, so `restsAt(1)` engages whenever
 *  no beat is mid-cycle. Undeclared, a pass always runs. The declaration
 *  rides the effect's comparable params, so two passes differing only in
 *  their rests compare unequal and re-patch. */
[[nodiscard]] TextEffect pass(Material material);

/** THE ESCAPE HATCH: an ad-hoc effect body under an author-given key.
 *
 *  The key IS the identity — two effects with the same key compare equal
 *  and the reconciler will prune one onto the other, so give a different
 *  body a different key or the old one silently keeps drawing. Bake the
 *  parameters that vary into the key (or into `params`) rather than
 *  capturing them silently.
 *
 *  `reach` is how far past the element's box the body may push a glyph;
 *  the default covers the shipped presets' range.
 *
 *  THE ONE PLACEMENT FACT THE LIBRARY CANNOT INFER lives here too. Every
 *  other effect answers `TextEffect::displaces` for itself — a preset knows
 *  its own deviation, `fx::keys` reads its table, `fx::seq`, `fx::mix` and
 *  `fx::hold` derive from their operands — but a lambda is opaque until it
 *  runs, so this door assumes the moving answer and takes
 *  `.displacing(false)` as the promise that the body leaves every pen
 *  position alone. */
[[nodiscard]] inline TextEffect effect(std::string key, GlyphModFn program,
                                       float reach = 48.0f,
                                       std::vector<float> params = {}) {
  return TextEffect(std::move(key), std::move(params), std::move(program),
                    reach);
}

/** ONE ENTRY OF A `fx::keys` TABLE: where it sits in local time, the
 *  deviation there, and — optionally — the curve for the segment that
 *  STARTS at it. */
struct Key {
  float at = 0;  ///< local time, 0→1
  GlyphMod mod;  ///< the deviation at that moment
  /** The curve this entry's own segment is interpolated with, overriding
   *  the table's. Unset takes the table's; the LAST entry's is never read,
   *  because no segment starts there. */
  choreograph::EaseFn ease;
};

/** THE KEYFRAME TABLE: a list of (local time, deviation) entries, and the
 *  curve between them.
 *
 *      const TextEffect rubberBand = fx::keys({
 *          {0.00f, {}},
 *          {0.30f, {.scaleX = 1.25f, .scaleY = 0.75f}},
 *          {0.50f, {.scaleX = 1.15f, .scaleY = 0.85f}},
 *          {1.00f, {}},
 *      }, &choreograph::easeInOutCubic);
 *
 *  Entries are read IN ORDER and each pair is one segment; local time
 *  before the first entry holds the first entry's deviation and time after
 *  the last holds the last's. Two entries at the same moment are a STEP,
 *  and the later one is what the step lands on.
 *
 *  THE CURVE APPLIES PER SEGMENT, not across the table: `at` says where a
 *  segment ends, the curve says how it is crossed, and every segment runs
 *  the whole curve. That is what a published keyframe list means — a table
 *  crossed by one curve end to end would ease into the first entry and out
 *  of the last and run the middle at whatever slope the curve happened to
 *  have there. Unset, a segment is linear.
 *
 *  Interpolation is COMPONENTWISE and follows the `fx::seq` crossfade
 *  exactly, because it is the same arithmetic: `codepoint` cuts at the
 *  middle of the segment rather than lerping, since there is no half-way
 *  glyph between two outlines; `axis` lerps only when the two entries name
 *  the SAME tag, and otherwise cuts the same way.
 *
 *  The table IS the identity — two `keys` over the same numbers and the
 *  same named curves compare equal and prune — and it declares its own
 *  reach from the offsets, growths and leans it publishes. */
[[nodiscard]] TextEffect keys(std::vector<Key> table,
                              choreograph::EaseFn ease = nullptr);

/** NOTHING UNTIL THE BEAT OPENS: `effect` as it is, except that a unit
 *  whose beat has not begun paints nothing at all.
 *
 *  A cascade hands every unit a local time clamped to [0,1], so a unit
 *  waiting its turn is handed 0 — and an effect that deviates at 0 is
 *  already performing before its beat. A substitution is the case that
 *  shows: `fx::scramble` churns from local 0, so a glyph still waiting
 *  shows a WRONG letter rather than no letter. This says "not yet".
 *
 *  The hold is ALPHA 0, not the identity: the point is a glyph that has not
 *  arrived, and the identity is a glyph sitting at rest, which for a
 *  substitution is the answer the effect exists to withhold. Alpha
 *  multiplies across tracks, so this is a VETO — a glyph whose held track
 *  has not opened paints nothing however many other tracks have opened on
 *  it. Put the hold on the track that owns the glyph's arrival.
 *
 *  A ONE-SHOT effect is what this is for. A loop effect reads a wrapping
 *  phase that passes through 0 on every cycle, and a held loop would blink
 *  its glyphs out each time it did. A LOOPING CASCADE (`Stagger::loopMs`)
 *  has nothing for this to withhold either: its fold keeps every unit
 *  somewhere in its cycle — there is no "not yet" — and local time touches
 *  0 only at the instant a beat re-opens, so the veto blanks that single
 *  instant and nothing else. An effect on a looping cascade gates its own
 *  arrival, the way a streak table's head is its own entrance. */
[[nodiscard]] TextEffect hold(TextEffect effect);

/** PHASES IN LOCAL TIME: each phase sees a renormalized 0→1 over its own
 *  window, so `fx::seq(a.until(0.35f), b.until(0.75f).xfade(0.10f), c)`
 *  plays `a` over the first 35% of every unit's beat, `b` over the next
 *  40% and `c` over the rest — each running its full curve.
 *
 *  The default joint is a hard cut. `.xfade(f)` on the ENDING phase lerps
 *  its deviation into the next one's, componentwise, over the last `f` of
 *  local time before the joint.
 *
 *  A sequence is NOT a keyframe table over effects, and neither combinator
 *  is the other's special case: a phase is an EFFECT re-clocked over its
 *  window and free to move throughout it, where a key is one deviation
 *  standing still and lerped toward. What they do share is the
 *  componentwise interpolation — the crossfade here and a segment there run
 *  the same arithmetic, so the substitutions cut the same way in both. */
[[nodiscard]] TextEffect seq(std::vector<Phase> phases);
template <typename... Rest>
[[nodiscard]] TextEffect seq(Phase first, Rest&&... rest) {
  std::vector<Phase> phases;
  phases.reserve(1 + sizeof...(Rest));
  phases.push_back(std::move(first));
  (phases.push_back(Phase(std::forward<Rest>(rest))), ...);
  return seq(std::move(phases));
}

/** BOTH AT ONCE: evaluates every operand at the same local t and composes
 *  the results by the same algebra stacked tracks use — dx/dy and
 *  rotation add, scale and alpha multiply. Comparable when its operands
 *  are. */
[[nodiscard]] TextEffect mix(std::vector<TextEffect> effects);
template <typename... Rest>
[[nodiscard]] TextEffect mix(TextEffect first, Rest&&... rest) {
  std::vector<TextEffect> effects;
  effects.reserve(1 + sizeof...(Rest));
  effects.push_back(std::move(first));
  (effects.push_back(std::forward<Rest>(rest)), ...);
  return mix(std::move(effects));
}

}  // namespace fx

/** WHICH GLYPHS a track addresses, as a comparable value.
 *
 *  Built from `sel::` (see below), combined with `|` (union), `&`
 *  (intersection) and `!` (complement). A default-constructed selector
 *  addresses EVERYTHING, which is what a track that names none gets.
 *
 *  Resolution happens once per (content, layout, selector) and is cached
 *  on the element, so a regular expression over a paragraph is matched
 *  when the text changes or reflows rather than once per frame. A pattern
 *  that does not compile selects nothing and warns once. */
class Selector {
 public:
  Selector() = default;  ///< everything

  /** Within EACH unit of an `sel::each` selector, keep `n` glyphs from
   *  wherever `drop()` left off. `take(n)` and `drop(n)` on their own
   *  partition every unit exactly: no glyph is in both, none is in
   *  neither. */
  [[nodiscard]] Selector take(int n) const;
  /** Within each unit, skip the first `n` glyphs and keep the rest. */
  [[nodiscard]] Selector drop(int n) const;

  [[nodiscard]] Selector operator|(const Selector& other) const;
  [[nodiscard]] Selector operator&(const Selector& other) const;
  [[nodiscard]] Selector operator!() const;
  bool operator==(const Selector& other) const;

  /** The forms a selector can take. Public because the resolver is a free
   *  function over a paragraph rather than a member. */
  enum class Kind : uint8_t {
    All,
    Word,
    Words,
    Line,
    Sentence,
    Range,
    Regex,
    Text,
    Style,
    Each,
    Union,
    Intersect,
    Complement,
  };
  /** Everything a selector of any kind needs, in one shape. The fields
   *  a given Kind ignores stay at their defaults — one flat state keeps
   *  selectors cheap to copy and comparable by value, which is what
   *  lets a track's selector take part in prop equality. */
  struct State {
    Kind kind = Kind::All;
    uint32_t lo = 0, hi = 0;  ///< Word/Words/Line/Sentence/Range bounds
    /** Regex/Text needle, or the style NAME an `sel::style` addresses — one
     *  slot, because no selector carries two of them and a second string
     *  would ride on every selector in the tree to serve one form. */
    std::u8string pattern;
    Unit each = Unit::Glyph;  ///< Each granularity
    int take = -1;            ///< Each: glyphs kept per unit (-1 = all)
    int drop = 0;             ///< Each: glyphs skipped per unit
    std::vector<Selector> operands;
    bool operator==(const State&) const = default;
  };
  /** Null for a default-constructed (everything) selector. */
  [[nodiscard]] const State* state() const { return m_state.get(); }
  static Selector of(State s);

 private:
  std::shared_ptr<const State> m_state;
};

/** THE SELECTOR VOCABULARY. Absolute forms name a position in the text;
 *  `each` slices every unit of one granularity the same way. */
namespace sel {
/** The i-th word of the paragraph (SigilWeave's line-break units). */
[[nodiscard]] Selector word(uint32_t index);
/** Words `[lo, hi)`. */
[[nodiscard]] Selector words(uint32_t lo, uint32_t hi);
/** The i-th flow line OF THE CURRENT LAYOUT — re-resolved when the text
 *  reflows, so a narrower box moves the selection with the break. */
[[nodiscard]] Selector line(uint32_t index);
/** The i-th sentence (ICU sentence segmentation). */
[[nodiscard]] Selector sentence(uint32_t index);
/** Every glyph whose cluster falls inside a UTF-16 range of the text. */
[[nodiscard]] Selector range(sigil::weave::CharRange chars);
/** Every match of an ICU regular expression (the pattern is UTF-8). A
 *  pattern that does not compile selects NOTHING and warns once. */
[[nodiscard]] Selector regex(std::u8string_view utf8Pattern);
/** Every occurrence of a literal substring. */
[[nodiscard]] Selector text(std::u8string_view utf8Substring);
/** EVERY RUN DRESSED UNDER THIS NAME — the runs a `rich()` value added with
 *  `add(utf8, styleName)`, addressed by the name rather than by the words
 *  they happen to contain.
 *
 *  This is the treatment as a handle: a glossary set in one registered
 *  style stays addressable when the copy changes, where naming the literal
 *  text means editing the selector every time an author edits a sentence.
 *
 *  It addresses the run's TEXT, so it survives everything that changes what
 *  that text looks like: re-registering the name against a different
 *  `weave::StyleSet` entry, or a `spanPaint`/`spanStyle` cutting across it,
 *  leaves the same runs selected.
 *
 *  ONLY A NAMED `rich()` RUN CARRIES A NAME. Plain `text(utf8, style)`, a
 *  `rich()` run given a style directly, and the `shared_ptr<Paragraph>`
 *  overload have none, so this addresses nothing there — as does a name no
 *  run was written with. Either way it selects nothing and warns once per
 *  name. */
[[nodiscard]] Selector style(std::string_view name);
/** Every unit of `granularity`, ready to be sliced with `.take()` /
 *  `.drop()`. Unsliced it is the same as selecting everything. */
[[nodiscard]] Selector each(Unit granularity);
}  // namespace sel

/** WHICH LIST A CASCADE NUMBERS ITS BEATS AGAINST.
 *
 *  `Selection` numbers the units the track's OWN selector resolved: a
 *  track addressing one word beats once, whatever the paragraph's word
 *  count is. That is the right answer for a track that owns its text, and
 *  the wrong one for two tracks sharing a paragraph — their beats line up
 *  only while their selections happen to resolve lists of the same length,
 *  and the frame they stop doing so the two halves of every unit start
 *  arriving at different times with no diagnostic.
 *
 *  `Text` numbers every unit of the cascade's granularity in the whole
 *  paragraph, addressed or not, so word ten is beat ten in every track
 *  that beats over words. Two tracks that partition one paragraph then
 *  share one clock BY CONSTRUCTION rather than by coincidence. */
enum class Beats : uint8_t { Selection, Text };

/** The beat-numbering names, spelled the way a cascade reads:
 *  `beats::Text`. */
namespace beats {
inline constexpr Beats Selection = Beats::Selection;
inline constexpr Beats Text = Beats::Text;
}  // namespace beats

/** THE PER-UNIT TIME REMAP (the GSAP stagger model). The track's master
 *  progress [0,1] spans `durationMs + eachMs·(N−1)` of virtual time,
 *  where N is the number of UNITS the cascade numbers; unit i starts after
 *  its delay and runs for durationMs.
 *
 *  The unit is what makes this more than per-glyph spacing: `over =
 *  unit::Word` beats once per word, and every glyph of that word shares
 *  its beat. The default, `unit::Cluster`, is per-glyph for ordinary
 *  Latin text and keeps a base letter attached to its combining marks
 *  everywhere else.
 *
 *  The spread is EVEN unless `cueMs` names the times outright. */
struct Stagger {
  float eachMs = 30;
  /** Amount-mode (mutually exclusive with eachMs; wins when > 0): the
   *  TOTAL spread, divided across however many units there are. Use it
   *  when the budget for the whole entrance is fixed and the text may
   *  change length — `eachMs` keeps per-unit spacing and lets the total
   *  grow, this keeps the total and shrinks the spacing. */
  float amountMs = 0;
  /** AN IRREGULAR TABLE: one start time per unit, in ms from the start of
   *  the track's progress, read by unit index. Caption, lyric and lip-sync
   *  timing is a table cut against a recording, and no even spread is a
   *  substitute for one.
   *
   *  Non-empty, it REPLACES the even spread: `eachMs`, `amountMs`, `from`
   *  and `distribution` say nothing, because a table already states both
   *  the order and the shape of the cascade. Everything else this struct
   *  says still holds — `over` is still what a unit is, `durationMs` is
   *  still how long one unit's own motion lasts, and `then()` still nests a
   *  second cascade inside every beat.
   *
   *  A unit past the end of the table starts at the LAST entry, so a short
   *  table piles its tail on one beat rather than inventing times; entries
   *  past the last unit are ignored. Either mismatch warns once. Build one
   *  with `cues()`. */
  std::vector<float> cueMs;
  float durationMs = 450;
  /** THE PER-UNIT WRAPPING BEAT: set above 0 and the cascade LOOPS — every
   *  unit's beat RE-OPENS on its own cycle of this period, phase-offset by
   *  the unit's start time (even ladder and cue table alike), so steady
   *  continuous motion — rain re-dropping column by column, arrivals that
   *  never stop arriving — is DECLARED rather than faked by re-running a
   *  one-shot.
   *
   *  THE MASTER STILL CLOCKS IT, and one full sweep 0→1 is exactly ONE
   *  CYCLE: the master maps onto `loopMs` of virtual time instead of the
   *  one-shot span, and unit i reads
   *  `clamp(((master·loopMs − startMs_i) mod loopMs) / durationMs)`.
   *  Master 0 and master 1 name the same instant of the cycle, so a
   *  WRAPPING bound phase — an Output stepped mod 1, the clock
   *  `fx::waveLoop` already reads — drives it seamlessly forever, and
   *  driving that phase is what keeps the element painting live: a looping
   *  cascade at a CONSTANT master is one still frame of the cycle, exactly
   *  as a wave at one phase is.
   *
   *  BETWEEN a beat's close and its next opening the unit rests at local 1
   *  — its landed deviation — and returns to 0 the instant its beat
   *  re-opens, so an effect that loops cleanly ends where nothing shows.
   *  Start offsets FOLD mod the period: two units whose starts differ by
   *  exactly `loopMs` share a phase, and a period shorter than
   *  `durationMs` re-opens a beat before it lands. The fold also means
   *  every unit is ALWAYS somewhere in its cycle — there is no "before
   *  the first beat" — so `fx::hold` has nothing left to veto (its t ≤ 0
   *  test is true only at the instant of re-opening); an effect on a
   *  looping cascade gates its own arrival instead.
   *
   *  `spanMs` and `Composer::cascadeSpanMs` answer the PERIOD — still the
   *  ms the master maps onto, and the number a driver needs: wrap the
   *  phase every `loopMs` of wall time and the schedule runs at its
   *  authored ms. ONE loop governs the whole cascade, read off the OUTER
   *  spec under `then()` as `beatsOver` is; a nested loopMs is ignored.
   *  0 — the default — is the one-shot cascade. */
  float loopMs = 0;
  /** Where the cascade starts. `Random` is keyed on the unit count and
   *  `seed`, so a scatter is the SAME scatter on every frame and after
   *  every relayout; `Edges` starts at both ends and meets in the middle. */
  enum class From : uint8_t {
    Start,
    Center,
    End,
    Random,
    Edges
  } from = From::Start;
  /** WHICH scatter `From::Random` deals. The ranking hash is keyed on the
   *  unit count alone at the default 0, so two same-count cascades scatter
   *  IDENTICALLY — three curtains of equal columns would all drop in one
   *  order. A nonzero seed mixes into that hash and deals an independent
   *  scatter per value, which is what several fields of one composition
   *  want. The scatter stays the scrambled EVEN ladder either way: every
   *  unit takes a distinct rank, so no two units ever open together, and
   *  `distribution` still shapes how those ranks crowd. Read only under
   *  `From::Random`; the other origins are their own order. */
  uint32_t seed = 0;
  /** Which units get a beat. */
  Unit over = Unit::Cluster;
  /** WHICH LIST those beats are numbered against — see `Beats`. The default
   *  numbers the track's own selection, which is what a track that owns its
   *  text means; `beats::Text` numbers the paragraph, which is what two
   *  tracks partitioning one paragraph need if they are to share a clock.
   *  A NESTED cascade takes the outer one's answer: a nested `beatsOver` is
   *  ignored, as its `durationMs` is. */
  Beats beatsOver = Beats::Selection;
  /** Shapes the START TIMES across the cascade (not the per-unit motion,
   *  which the effect and the progress own): the linear ramp of delays is
   *  passed through this curve, so an ease-in distribution crowds the
   *  early units together and lets the tail spread out. Null is the
   *  uniform spacing. */
  choreograph::EaseFn distribution = nullptr;
  /** A NESTED cascade inside each of this one's beats — see `then()`.
   *  Held out of line because a Stagger cannot contain itself by value. */
  std::shared_ptr<const Stagger> inner;

  /** Compounds a second cascade inside every beat of this one:
   *  `stagger(unit::Word, {…}).then(unit::Glyph, {…})` delays each word,
   *  then delays each glyph within its word's beat. The outer
   *  `durationMs` is ignored — a beat lasts exactly as long as the inner
   *  cascade needs. */
  Stagger& then(Unit granularity, Stagger nested);

  /** THE VIRTUAL SPAN, in ms: what a track's master progress [0,1] maps
   *  onto when this cascade numbers @p unitCount units — the moment the
   *  last beat closes. `durationMs + eachMs·(N−1)` for the even ladder,
   *  `durationMs + amountMs` past one unit in amount mode (every count
   *  past one answers the same, because the amount IS the spread), the
   *  latest time any unit reads out of a cue table plus `durationMs`, and
   *  the compounded extent under `then()`, where @p innerUnitCount is how
   *  many inner units one beat holds (the widest beat's count, where they
   *  vary). Zero units answer as one unit does: `durationMs` alone.
   *
   *  A LOOPING cascade (`loopMs` > 0) answers its PERIOD, whatever the
   *  counts: the master maps onto one cycle, so the period is what a
   *  driver's wrap must span for the schedule to run at its authored ms.
   *
   *  This is the DECLARE-TIME form, for the number a description needs
   *  before any node exists — above all a progress transition whose
   *  duration should cover the cascade exactly, so the last beat closes
   *  as the master arrives at 1 and the schedule runs at its authored ms.
   *  `Composer::cascadeSpanMs` reads the same number off a MOUNTED track,
   *  with the unit counts the laid-out text supplies; the two agree
   *  because one resolved-cascade body computes both. */
  [[nodiscard]] float spanMs(uint32_t unitCount,
                             uint32_t innerUnitCount = 1) const;

  bool operator==(const Stagger& other) const;
};

/** Names the units a cascade beats over: `stagger(unit::Word, {.eachMs =
 *  60})`. Sugar for setting `Stagger::over`, and the spelling `then()`
 *  reads against. */
[[nodiscard]] Stagger stagger(Unit granularity, Stagger spec = {});

/** AN IRREGULAR CASCADE, from a table of start times in ms:
 *  `cues({0, 340, 720, 1180, 1600}, {.durationMs = 180})`.
 *
 *  A cue table IS a Stagger — it answers only "when does unit k start", and
 *  every other thing a cascade says (what a unit is, how long one unit's
 *  motion lasts, whether a second cascade nests inside each beat, which
 *  list the beats are numbered against) is orthogonal to that and still
 *  wanted. Being one value rather than two also keeps a track's cascade one
 *  slot, with one equality and one prune.
 *
 *  So this goes wherever `stagger()` goes, and the two compose — name the
 *  granularity with `stagger(unit::Word, cues({…}))`. Sugar for setting
 *  `Stagger::cueMs`, whose documentation states what a table shorter or
 *  longer than the unit list does. */
[[nodiscard]] Stagger cues(std::vector<float> startMs, Stagger spec = {});

/** ONE TRACK: which glyphs, what deviation, how the beats spread, and the
 *  master progress that drives it.
 *
 *  `progress` takes the full Animatable treatment — a plain constant,
 *  a `with()`/`animate()` transition (retarget-safe: each track owns its
 *  own transition slot, so retargeting the second track leaves the first
 *  alone), or a `ch::Output` binding. One-shot effects consume 0→1; loop
 *  effects read a WRAPPING bound phase. While any track's progress moves
 *  the element paints live; once every track settles it caches like a
 *  static leaf. */
struct Track {
  Selector where;    ///< default: every glyph
  TextEffect effect; /**< what it does */
  Stagger stagger;
  Animatable<float> progress = 1.0f;
  /** Pixels beyond the element's box this track may paint, which the
   *  recording cull grows by. Negative means "ask the effect", which is
   *  what every preset answers for itself; set it when a keyed lambda
   *  throws glyphs further than the default allows. Over-reporting is
   *  safe, under-reporting truncates cached output with no diagnostic. */
  float reach = -1.0f;
  /** SKIP THE SNAPPING for the glyphs this track addresses. A driven
   *  rotation, alpha, colour multiplier and axis coordinate are quantized
   *  before they reach the draw, because each distinct value is both a
   *  distinct batch bucket and a distinct glyph-atlas strike. Continuous
   *  values buy smoothness with exactly that: one strike minted per value
   *  and every addressed glyph rasterized again every frame. Set it where
   *  the steps show — a slow lift at display size, a tint sweeping along a
   *  wordmark — and nowhere else. A glyph any addressing track declares
   *  continuous is continuous. */
  bool continuous = false;

  /** How far this track really reaches: its own number when it declares
   *  one, otherwise its effect's. */
  [[nodiscard]] float reachPx() const {
    return reach >= 0 ? reach : effect.reach();
  }
  /** Structural equality, EXCLUDING `progress` — an Animatable is compared
   *  where every other animated slot is, by the reconciler. */
  bool sameShape(const Track& other) const;
  /** Full equality: the shape above plus the progress. */
  bool operator==(const Track& other) const;
};

/** ONE BEAT OF A RESOLVED CASCADE — what `Composer::beatsOf` reports.
 *
 *  A stagger is otherwise an invisible remap: it numbers units, spreads
 *  them, and tells nobody. Anything that must travel WITH a cascade and is
 *  not a glyph — a bouncing ball, a playhead, a travelling underline, a
 *  caret, a per-unit meter — then has to restate `i · eachMs` in its own
 *  arithmetic, which stops agreeing with the engine the moment the cascade
 *  nests or takes a cue table. This is the schedule read back instead. */
struct Beat {
  /** The unit's laid-out rect, in the composer's coordinate space: the
   *  axis-aligned bound of the advance boxes the layout placed for the
   *  glyphs this track addresses in this beat. It follows a wrapped line,
   *  a mixed-style run's own size, a path run's curve and a vertical
   *  column's axis, because it is read off the placement rather than
   *  measured again. */
  SkRect rect = SkRect::MakeEmpty();
  /** The OUTER unit this beat belongs to, numbered as the cascade numbers
   *  it — the track's own selection under `beats::Selection`, the whole
   *  paragraph under `beats::Text`. A nested cascade reports several beats
   *  sharing one `unitIndex`, one per inner unit inside that outer beat,
   *  which is what lets a per-word mark and a per-letter one read the same
   *  list. */
  uint32_t unitIndex = 0;
  /** When this beat opens, in ms from the start of the track's progress —
   *  the COMPOUNDED delay, outer plus inner, under a nested cascade. */
  float startMs = 0;
  /** This beat's own 0→1 at the track's progress right now — the same
   *  number the effect is being handed for these glyphs. Under a looping
   *  cascade (`Stagger::loopMs`) it is the WRAPPED local time of the
   *  current cycle, and no cycle index rides beside it: the master is a
   *  phase mod 1 and carries no cycle count into the composer, so cycle
   *  identity lives with whoever steps the phase. */
  float localT = 0;
  /** The beat is running: it has begun and has not finished — under a
   *  looping cascade, mid-beat in its current cycle. */
  bool active = false;

  bool operator==(const Beat&) const = default;
};

/** A READING SET BESIDE THE TYPE IT READS — furigana over a compound,
 *  emphasis dots down a column, a gloss under a phrase.
 *
 *  IT IS PART OF THE TEXT, not a thing standing next to it, and the one
 *  fact that makes it so is `reserve`: the band the reading occupies is
 *  stated BEFORE the base is laid out, from the annotation's own metrics,
 *  and goes into the base's strut. The base is then broken and placed once
 *  with the room already there, and the readings are placed on the result.
 *  Nothing chases anything, and there is no round of convergence to run
 *  out of. `kit::annotate` is the other half of the idea — a sibling that
 *  reserves nothing and stands beside the finished text — and marginalia,
 *  callouts and word labels belong there.
 *
 *  MONO, GROUP AND JUKUGO RUBY ARE THE UNIT CHOICE and nothing else.
 *  `unit::Cluster` gives one reading per character, which is mono ruby;
 *  `unit::Word` gives one per word, which is group ruby; and a base that
 *  BREAKS ACROSS A LINE OR A COLUMN reports its units on both, so its
 *  reading splits with it, in proportion to the base's advance either
 *  side. That is not a special case here — it is what reading the units off
 *  the placement means.
 *
 *  THE SIZE IS THE ANNOTATION'S OWN. `style` is a whole TextStyle, and
 *  there is no fraction of the base's size anywhere in the library: a ruby
 *  set at half the base is a decision, and decisions of that kind are the
 *  caller's. */
struct Annotation {
  /** Which of the base's units are annotated. */
  Selector where;
  /** The granularity the readings map to — cluster for mono ruby, word for
   *  group ruby, sentence or line for a note over a passage. */
  Unit unit = Unit::Cluster;
  /** One reading per addressed unit, in draw order. A LIST OF ONE is used
   *  for every unit, which is how a row of identical emphasis marks is
   *  written; a list shorter than the units leaves the rest bare. */
  std::vector<std::u8string> readings;
  /** The reading's own type. */
  sigil::weave::TextStyle style;
  /** Which side of the type it stands on: `Before` is above a line and to
   *  the RIGHT of a column, `After` below a line and to the LEFT of one —
   *  the sides each writing mode reads its furniture on. */
  enum class Side { Before, After };
  Side side = Side::Before;
  /** Standoff from the type's own band, px. */
  float gap = 0;
  /** Whether the band this reading occupies is put into the base's strut
   *  before the base is laid out. False sets the reading over the type it
   *  reads, which is what an emphasis dot does and a furigana never
   *  does. */
  bool reserve = true;

  bool operator==(const Annotation& other) const {
    return where == other.where && unit == other.unit &&
           readings == other.readings && style == other.style &&
           side == other.side && gap == other.gap && reserve == other.reserve;
  }
};

/** ONE UNIT OF A LAID-OUT TEXT, as everything beside the text reads it —
 *  what `Composer::units` reports and what every annotation is placed
 *  from.
 *
 *  A `Beat` is the same rect under a schedule: it needs an `fx()` track, a
 *  stagger and a progress, and it answers about a cascade. This answers
 *  about the TEXT — where a word, a cluster or a line landed, on which
 *  baseline, in which writing mode, set in which style — for a selector and
 *  a unit, with no track anywhere. It is read off the placement rather than
 *  measured again, so it follows a wrapped line, a mixed-style run's own
 *  size, a path run's curve and a vertical column's axis by construction.
 *
 *  ONE ENTRY PER UNIT, in draw order. A selector that addresses several
 *  units reports several entries — which is the whole difference from
 *  `mark()`, whose one rect is the union of them all. */
struct TextUnit {
  /** The unit's laid-out rect in the node's own space: the axis-aligned
   *  bound of the advance boxes of the glyphs the selector addressed in
   *  it. */
  SkRect rect = SkRect::MakeEmpty();
  /** The unit's ordinal among those the selector addressed, from 0 in draw
   *  order. */
  uint32_t index = 0;
  /** HORIZONTAL: the baseline the unit stands on, in y. VERTICAL: the
   *  central axis of the column it stands in, in x — a column has no
   *  baseline, and its glyphs centre themselves across that axis. */
  float axis = 0;
  /** The flow's band depth: the line's pitch, which in a vertical setting
   *  is the width of the column. */
  float pitch = 0;
  /** The band the unit's own face occupies either side of its baseline,
   *  from the face's metrics rather than from its ink — so a unit of
   *  lowercase and a unit of capitals report the same band. */
  float ascent = 0, descent = 0;
  /** Which way the passage runs. */
  sigil::weave::WritingMode writingMode =
      sigil::weave::WritingMode::kHorizontal;
  /** How the unit stands in its column: upright, turned with the column,
   *  or set across it. `kAuto` in a horizontal passage, where the question
   *  does not arise. */
  sigil::weave::VerticalForm verticalForm = sigil::weave::VerticalForm::kAuto;
  /** The text the unit covers, as UTF-16 units into the node's
   *  paragraph. */
  sigil::weave::CharRange range;
  /** The style the unit's first glyph is set in — the annotation's cue for
   *  a size of its own, since the library decides no typographic ratio. */
  sigil::weave::TextStyle style;
  /** The flow line (or COLUMN) the unit landed on. */
  int lineIndex = 0;

  bool operator==(const TextUnit& other) const {
    return rect == other.rect && index == other.index && axis == other.axis &&
           pitch == other.pitch && ascent == other.ascent &&
           descent == other.descent && writingMode == other.writingMode &&
           verticalForm == other.verticalForm && range == other.range &&
           style == other.style && lineIndex == other.lineIndex;
  }
};

// ---------------------------------------------------------------------------
// MIXED TEXT — one value, several styles
//
// A paragraph whose words are not all set the same way is a VALUE here, not
// a document to be marked up. There is no markup language: a run of text
// carries a style, or the name of one, and that is the whole vocabulary.
// Whatever else a passage needs — a colour on the numbers, a weight on one
// phrase — is asked for by SELECTOR after the fact
// (`Element::spanPaint` / `Element::spanStyle`), so the content stays
// content and the type treatment stays in one place.
//
// The escape hatch is unchanged: `text(std::shared_ptr<Paragraph>, options)`
// hands the engine a document built by hand, for the passage too custom for
// either of these.

/** MIXED-STYLE TEXT AS A COMPARABLE VALUE — the builder `text()` takes.
 *
 *      auto p = rich(base)
 *                   .add(u8"Signal ")
 *                   .add(u8"woven", accent)
 *                   .add(u8" through ")
 *                   .add(u8"noise", mono);
 *      text(p).fx({.effect = fx::slide(-60),
 *                  .stagger = stagger(unit::Word, {.eachMs = 120})});
 *
 *  `add(utf8)` sets a run in the base style; `add(utf8, style)` sets it in
 *  its own; `add(utf8, name)` sets it in a style looked up by NAME (see
 *  below). Runs are appended in order and concatenate into one paragraph —
 *  nothing is inserted between them, so the spaces are the author's.
 *
 *  WHY IT IS A VALUE, and what that buys over the `shared_ptr<Paragraph>`
 *  overload: two rich texts describing the same runs in the same styles are
 *  EQUAL, so a component that rebuilds its text every describe prunes
 *  exactly like a static leaf. The pointer overload cannot answer that
 *  question — a fresh `make_shared` is a fresh identity and reads as
 *  changed content every time. Internally the runs materialize into one
 *  weave `Paragraph` on the instance, rebuilt only when the described value
 *  changes; the shaping cache is content-addressed, so a rebuild that
 *  changed one run re-shapes one run.
 *
 *  NAMES resolve through a `weave::StyleSet`, which comes from one of two
 *  places: `styles()` supplies one explicitly, and `env::Provide<StyleSet>`
 *  supplies one ambiently to everything described in its scope. AN EXPLICIT
 *  SET ALWAYS WINS, whichever order the two are written in. A name the set
 *  does not register resolves to the base handed to `rich()` — the base is
 *  this text's one default, and a misspelled name shows as content set in
 *  it rather than as content that did not draw.
 *
 *  Resolution happens as the run is added (and again over every named run
 *  when `styles()` arrives), so the finished value holds real styles and
 *  depends on no scope that has since ended. */
class RichText {
 public:
  /** One run of text and the style it is set in — or one INLINE SLOT, which
   *  is a run whose content is the single object-replacement character the
   *  flow anchors a reserved box at. */
  struct Run {
    std::u8string utf8;
    sigil::weave::TextStyle style;  ///< resolved: its own, its name's, or base
    std::string styleName;          ///< the name it was written with, if any
    /** Non-empty on a SLOT run: the name a child of this text node is laid
     *  out into. */
    std::string slotKey;
    SkSize slotSize = {0, 0};    ///< the box the breakers reserve
    float slotBaselineDrop = 0;  ///< the box's bottom, below the baseline
    bool operator==(const Run&) const = default;
  };

  RichText() = default;
  /** Starts a value whose unstyled runs — and unregistered names — are set
   *  in @p baseStyle. */
  explicit RichText(sigil::weave::TextStyle baseStyle)
      : m_base(std::move(baseStyle)) {}

  /** Appends a run in the base style. */
  RichText& add(std::u8string_view utf8);
  /** Appends a run in its own style. */
  RichText& add(std::u8string_view utf8, sigil::weave::TextStyle style);
  /** Appends a run in the style registered under @p styleName. */
  RichText& add(std::u8string_view utf8, std::string_view styleName);

  /** Reserves an INLINE SLOT: `size` px of blank space woven into the flow,
   *  and the name a child Element of this text node is laid out into.
   *
   *      text(rich(body).add(u8"press ").slot("key", {28, 18}).add(u8" now"))
   *          .child(box().key("key").fill(ink).corners({4}))
   *
   *  The reserved box is ONE UNBREAKABLE WORD: a line never breaks inside
   *  it, and it moves the line's height when it is taller than the type.
   *  `baselineDrop` is how far the box's BOTTOM sits below the baseline —
   *  0 stands it on the baseline like an inline image, and about the face's
   *  descent centres a pill on the x-height.
   *
   *  The child is an ordinary subtree: it animates, caches and hit-tests
   *  like any other element, and it re-lands wherever the placeholder lands
   *  when the text reflows. It is a POSITIONED subtree — the placeholder
   *  rect is its box, so flex layout does not run inside it and its own
   *  children take explicit rects, exactly as under `positioned()`.
   *
   *  A TEXT SLOT IS NOT A MOUNT SLOT. `slot()` and `Composer::renderSlot`
   *  name a hole a HOST fills from outside the description, and those names
   *  live in one registry for the whole composition. These names live in
   *  this rich-text value alone and are matched against the `key()` of this
   *  text node's own children — so two captions may both reserve a slot
   *  called "icon" without colliding, and neither is reachable by
   *  `renderSlot`. A child keyed for a slot the content does not declare
   *  draws nothing, and says so once. */
  RichText& slot(std::string key, SkSize size, float baselineDrop = 0);
  /** Supplies the style set names resolve through, beating any the
   *  environment offers, and re-resolves every named run already added. */
  RichText& styles(sigil::weave::StyleSet set);

  /** The style unstyled runs and unregistered names are set in. */
  [[nodiscard]] const sigil::weave::TextStyle& base() const { return m_base; }
  /** The runs, in the order they were added. */
  [[nodiscard]] std::span<const Run> runs() const { return m_runs; }
  [[nodiscard]] bool empty() const { return m_runs.empty(); }

  /** Equal when the base, the runs, their resolved styles and the names
   *  they were written with all match — the question the prune asks.
   *
   *  The style SET is deliberately not compared: a name is resolved as it
   *  is added, so two values that resolved to the same styles describe the
   *  same paragraph however they got there, and an entry neither of them
   *  names cannot make them differ. */
  bool operator==(const RichText& other) const {
    return m_base == other.m_base && m_runs == other.m_runs;
  }

 private:
  sigil::weave::TextStyle m_base;
  std::vector<Run> m_runs;
  sigil::weave::StyleSet m_styles;
  bool m_hasStyles = false;       // a set is in play (explicit or inherited)
  bool m_stylesExplicit = false;  // styles() gave it; env cannot replace it
};

/** A TEXT FILLED INTO AS MANY FRAMES AS IT IS GIVEN.
 *
 *      Story article(rich(body).add(u8"…"));
 *      article.paragraphs({heading, para, para});
 *
 *      root.child(frame(article).key("a").thread("b").width(Dim(300)))
 *          .child(frame(article).key("b").thread("c").width(Dim(300)))
 *          .child(frame(article).key("c").width(Dim(300)).ellipsis(u8"…"));
 *
 *  A story is CONTENT PLUS ITS BLOCK STYLES and nothing else — it holds no
 *  layout, no cursor and no frame. Each frame of a chain fills from where
 *  the one before it stopped, and the cut moves as any frame's measure
 *  moves. The BLOCKS ARE NUMBERED FROM THE STORY'S START, so the third
 *  block is set the same way whichever frame it happens to land in.
 *
 *  Pitch, writing mode and block styles are the story's and a frame cannot
 *  override them: a frame that wants a different pitch is a different
 *  story. What a frame decides is its own geometry — its box, its
 *  exclusions, a silhouette it flows around — and whether it is the last,
 *  which is the one that sets an ellipsis. Overflow on any other frame is
 *  the normal case and draws nothing.
 *
 *  It is a VALUE: two stories describing the same runs in the same styles
 *  are equal, so a component that rebuilds its story every describe prunes
 *  exactly like a static leaf. */
class Story {
 public:
  Story() = default;
  /** A story over mixed content. */
  explicit Story(RichText content) : m_content(std::move(content)) {}
  /** A story over one styled string. */
  Story(std::u8string utf8, sigil::weave::TextStyle style)
      : m_content(RichText(std::move(style))) {
    m_content.add(m_contentScratch = std::move(utf8));
  }

  /** How each BLOCK of the story is set, in block order — the same value
   *  `Element::paragraphs` takes, and stated once for every frame. */
  Story& paragraphs(std::vector<sigil::weave::ParagraphStyle> blocks) {
    m_blocks = std::move(blocks);
    return *this;
  }

  [[nodiscard]] const RichText& content() const { return m_content; }
  [[nodiscard]] std::span<const sigil::weave::ParagraphStyle> blocks() const {
    return m_blocks;
  }
  [[nodiscard]] bool empty() const { return m_content.empty(); }

  bool operator==(const Story& other) const {
    return m_content == other.m_content && m_blocks == other.m_blocks;
  }

 private:
  RichText m_content;
  std::u8string m_contentScratch;
  std::vector<sigil::weave::ParagraphStyle> m_blocks;
};

/** Starts a mixed-text value whose default is @p base — see RichText. */
[[nodiscard]] RichText rich(sigil::weave::TextStyle base = {});

/** UTF-8 std::string → std::u8string for text() call sites. */
inline std::u8string toU8(std::string_view s) {
  return std::u8string(s.begin(), s.end());
}

// ---------------------------------------------------------------------------
// THE TEXT PAINTER — the seam the kernel draws dressed type through

namespace detail {
/** ONE `rich()` RUN THAT WAS WRITTEN UNDER A STYLE NAME, and the text it
 *  occupies — what `sel::style` resolves against.
 *
 *  The name is tied to the run's TEXT rather than to the style span it
 *  produced, and that is the whole reason the answer holds up. Spans are
 *  cut and merged by every `spanPaint` and `spanStyle` the leaf declares,
 *  so a span index is a number about the paragraph's current normal form;
 *  a run's extent is a fact about the content that only new content
 *  changes. Re-registering the name against a different style, or a restyle
 *  slicing across the run, leaves this untouched.
 *
 *  Built as the runs are appended, in declaration order. Empty for every
 *  content form that carries no names. */
struct NamedRun {
  std::string name;
  sigil::weave::CharRange chars;
};
}  // namespace detail

/** WHAT THE KERNEL ASKS OF DRESSED TYPE — every operation the composer
 *  needs from text that is not simply resting on its own straight baseline:
 *  a run carrying fx() tracks, riding a path, anchoring marks, or restyled
 *  by selector. The kernel holds the paragraph, lays it out and draws it at
 *  rest by itself; everything below is answered by the value a text verb
 *  installs on the description (`fx()`, `onPath()`, `mark()`, `spanStyle()`,
 *  `spanPaint()`, `variationDrive()`). A text node carrying none of those
 *  has no painter, and the kernel then draws its paragraph at rest, resolves
 *  no marks and restyles nothing — the same picture a painter would draw for
 *  a description with nothing to dress.
 *
 *  The instance handed in is the kernel's retained node for the text; the
 *  painter reads its paragraph and layout and keeps its own engine state on
 *  it. */
class TextPainterOps {
 public:
  virtual ~TextPainterOps() = default;
  /** THE GLYPH DRAW for dressed text: the rest pose comes from the baseline
   *  — level on a plain run, on the curve and turned to it on a path run —
   *  and every fx() track's deviation applies on top of it. @p override is
   *  the glyph-paint override textFill()/textStroke() ask for, or null;
   *  @p onPath is null for text with no baseline path; @p size is the
   *  node's box; @p ctx is the node's paint context. */
  virtual void paint(detail::Instance& inst, SkCanvas& canvas,
                     const sigil::weave::PaintStyle* override,
                     const TextPath* onPath, SkSize size,
                     const PaintContext& ctx) const = 0;
  /** WHERE EACH mark() ANCHORS: refills the instance's mark rects from the
   *  layout the letters are drawn from, one rect per anchor. */
  virtual void marks(detail::Instance& inst) const = 0;
  /** WHERE THE UNITS A SELECTOR ADDRESSES LANDED, one entry each, read off
   *  the same layout the letters are drawn from — the query behind
   *  `Composer::units`. */
  virtual std::vector<TextUnit> units(detail::Instance& inst,
                                      const Selector& selector,
                                      Unit unit) const = 0;
  /** LAYS OUT EVERY READING this text carries against the layout its
   *  letters are drawn from, and leaves the results on the instance for
   *  the kernel to draw. */
  virtual void annotations(detail::Instance& inst) const = 0;
  /** THE BAND THIS TEXT'S RESERVING ANNOTATIONS NEED, from their own
   *  metrics alone — asked BEFORE the text is laid out, which is what
   *  makes a reservation a layout input rather than a cycle. */
  virtual sigil::weave::ReservedBand reservedBand(
      detail::Instance& inst, std::span<const Annotation> annotations) const = 0;
  /** WHICH TEXT A SELECTOR ADDRESSES, as UTF-16 ranges — sorted, merged,
   *  non-overlapping. `sel::line` reads @p lines, or @p columns where the
   *  passage is vertical; `sel::style` reads @p named. */
  virtual std::vector<sigil::weave::CharRange> ranges(
      const Selector& selector, sigil::weave::Paragraph& paragraph,
      sigil::weave::FontContext& fonts,
      std::span<const sigil::weave::LineMetrics> lines,
      std::span<const sigil::weave::ColumnMetrics> columns,
      std::span<const detail::NamedRun> named) const = 0;
  /** Whether a restyle to @p style over @p ranges can be carried as
   *  draw-time axis tracks instead of re-shaping the text it covers: the
   *  style must differ from every covered span's only in variable-font
   *  axes, drop none the text was shaped with, and every axis it moves must
   *  be advance-invariant on that span's face. On success @p axes holds one
   *  (tag, coordinate) per axis that actually changes.
   *
   *  @p paintCarried is the text whose PAINT an earlier declaration owns.
   *  A span lying wholly inside it is compared on its other dimensions
   *  alone, because the fold writes no paint at all: the colour standing
   *  there is the one that is meant to stand, so a difference between it
   *  and @p style's own paint is not a reshape. */
  virtual bool foldable(
      detail::Instance& inst, const sigil::weave::TextStyle& style,
      std::span<const sigil::weave::CharRange> ranges,
      const sigil::weave::Paragraph& paragraph,
      std::span<const sigil::weave::CharRange> paintCarried,
      std::vector<std::pair<std::string, float>>& axes) const = 0;
  /** THE SCHEDULE ONE TRACK IS RUNNING, resolved against the layout the
   *  last draw produced; rects in the node's own space. */
  virtual std::vector<Beat> beats(detail::Instance& inst,
                                  size_t trackIndex) const = 0;
  /** THE SAME SCHEDULE'S WHOLE VIRTUAL SPAN in ms; 0 wherever beats()
   *  answers empty. */
  virtual float cascadeSpanMs(detail::Instance& inst,
                              size_t trackIndex) const = 0;
};

/** The painter as a description carries it: a comparable value, excluded
 *  from structural equality because it is the same engine on every text
 *  that has one. */
using TextPainter = Erased<TextPainterOps>;

namespace detail {
/** THE ENGINE WITHOUT A DESCRIPTION TO CARRY IT. A text leaf installs the
 *  painter when it dresses its type, and one that dresses nothing has
 *  none — which is right for drawing, and wrong for a query that asks
 *  where a plain passage's words landed. The typography tier registers
 *  itself here as it is linked in, and the read-back queries fall through
 *  to it. Null in a program that links the kernel without that tier, where
 *  those queries answer empty, as an unknown key does. */
void registerTextEngine(const TextPainterOps* engine);
[[nodiscard]] const TextPainterOps* registeredTextEngine();
}  // namespace detail

}  // namespace sigil::compose
