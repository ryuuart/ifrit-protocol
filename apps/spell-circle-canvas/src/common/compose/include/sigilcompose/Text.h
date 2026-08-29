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
#include <sigilweave/Paragraph.h>
#include <sigilweave/Style.h>

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sigilcompose/Motion.h"

namespace sigil::compose {

class Material;

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
  /** The next 32 bits (SplitMix64's finalizer over a 64-bit counter). */
  uint32_t bits() {
    m_state += 0x9e3779b97f4a7c15ull;
    uint64_t z = m_state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return (uint32_t)((z ^ (z >> 31)) >> 32);
  }
  /** The next value in [0, 1). */
  float unit() { return (float)(bits() >> 8) * (1.0f / 16777216.0f); }
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
   *  with no reshape. The kernel's own effect: `Element::spanAxis` is a
   *  track carrying it.
   *
   *  Only an ADVANCE-INVARIANT axis is honoured: the glyphs keep the pen
   *  positions shaping gave them, so an axis that moves advances would
   *  leave them sitting wrong. The runtime probes the face once per axis
   *  and refuses one that does, drawing at the shaped face and warning
   *  once — GRAD is the advance-invariant weight most faces carry, while
   *  wght belongs in the shaping style, which re-shapes. */
  static TextEffect axis(const char (&tag)[5], float value);

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
   *  factory behind `fx::pass` (TextFx.h), where the contract is
   *  documented. The material must carry SkSL SOURCE
   *  (`Material::sksl(std::string, …)`), because the runtime bakes the
   *  unit count into the compiled shader; any other material warns once
   *  and returns an EMPTY effect, so the track draws its glyphs at rest. */
  static TextEffect pass(Material material);
  /** The pass material, or null for every per-glyph effect — what the
   *  runtime dispatches on. */
  [[nodiscard]] const Material* passMaterial() const;

  /** DECLARES A PHASE WHERE THIS PASS IS AN EXACT PASS-THROUGH — an
   *  author's promise the runtime spends but cannot verify, in the same
   *  family as `isAnimated`, `bleed()` and `reach`. When every unit the
   *  track addresses sits at a declared phase, the runtime skips the layer
   *  and the shader and draws the glyphs directly. The contract, and what
   *  a false promise looks like, is documented at `fx::pass` (TextFx.h).
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

/** Starts a mixed-text value whose default is @p base — see RichText. */
[[nodiscard]] RichText rich(sigil::weave::TextStyle base = {});

/** UTF-8 std::string → std::u8string for text() call sites. */
inline std::u8string toU8(std::string_view s) {
  return std::u8string(s.begin(), s.end());
}

}  // namespace sigil::compose
