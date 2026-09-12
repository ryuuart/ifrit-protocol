#pragma once

/** @file
 * SigilCompose typography — THE `fx::` CATALOGUE: the effects the runtime
 * evaluates by STRUCTURE rather than by calling a preset's body. The
 * substitution, the shader pass, the keyframe table, the hold, the two
 * combinators, and the escape hatch an ad-hoc body goes through.
 *
 * The value they are all built as is <sigilcompose/typography/TextEffect.h>;
 * the presets that are plain values over the seam — the entrances, the
 * loops, the tints — are the kit's, in <sigilcompose/kit/Kinetic.h>. What
 * separates the two shelves is who evaluates: a preset is a body this
 * library calls, and everything here is a shape the runtime reads.
 */

#include <sigilcompose/typography/TextEffect.h>

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace sigil::compose {

// ---------------------------------------------------------------------------
// The effects the RUNTIME evaluates by structure rather than by calling a
// preset's body: the substitution, the shader pass, the keyframe table and
// the combinators over whole effects. Their bodies are the engine's; the
// presets that are plain values are the kit's, in kit/Kinetic.h.

namespace fx {

/** THE DISPLAY SIZE A REACH IS DECLARED AGAINST when no effect knows the
 *  font size at construction: a glyph grown by a factor about its own
 *  centre escapes its box by half the excess of this many pixels in each
 *  direction, and a keyframe table's growths and leans are read against
 *  it the same way. Over-reporting is safe, so a preset drawn smaller than
 *  this reserves more than it needs and one drawn larger declares its own
 *  `Track::reach`. */
inline constexpr float kNominalSizePx = 96.0f;

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

/** A VARIABLE-FONT AXIS HELD AT ONE COORDINATE for every glyph the track
 *  addresses — a grade, an optical size, a slant applied at draw time with
 *  no reshape.
 *
 *  Only an ADVANCE-INVARIANT axis is honoured: the glyphs keep the pen
 *  positions shaping gave them, so an axis that moved advances would leave
 *  them sitting wrong. The runtime probes the face once per axis and
 *  refuses one that does, drawing at the shaped face and warning once —
 *  GRAD is the advance-invariant weight most faces carry, while wght
 *  belongs in the shaping style, which re-shapes. */
[[nodiscard]] inline TextEffect variableAxis(const char (&tag)[5],
                                             float value) {
  return TextEffect::variableAxis(tag, value);
}

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
 *      auto burn = material::skia::Paint::recipe(
 *          material::Material(dissolve, Burn{ink}));
 *      text(u8"EMBER DECODE", display)
 *          .fx({.effect = fx::pass(burn),
 *               .stagger = {.eachMs = 260}, .unit = weave::Unit::Cluster});
 *
 *  THE MATERIAL MUST BE RECIPE-BACKED (`material::skia::Paint::recipe`) over
 *  a recipe
 *  with an SkSL body, because the unit count is baked into the compiled
 *  shader — a runtime effect's array size is fixed at compile and SkSL has
 *  no uniform-bounded loop. The RUNTIME owns that specialization: it holds
 *  a second recipe over the same parameters, per distinct unit count, whose
 *  body is
 *
 *      uniform shader uContent;        // the units' rendered layer
 *      uniform float4 uUnitRect[N];    // per unit: x, y, w, h
 *      uniform float2 uUnitPhase[N];   // per unit: local 0→1, seed
 *      const int kUnitCount = N;      // the loop bound
 *
 *  ahead of the author's — write the body against those names and do not
 *  declare them, and put every uniform of your own in the parameters struct
 *  rather than in the body's text. The parameters ARE the ABI: a field the
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
 *  A pass is a WHOLE-TRACK statement: inside `fx::sequence`, `fx::mix` or
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
 *  a LOOPING cascade (`motion::Spread::loopMs`) a unit touches 0 only at the
 *  instant its beat re-opens, so `restsAt(0)` effectively never engages
 *  there — correctly, the cycle is always mid-flight somewhere — while a
 *  unit RESTS at exactly 1 between beats, so `restsAt(1)` engages whenever
 *  no beat is mid-cycle. Undeclared, a pass always runs. The declaration
 *  rides the effect's comparable parameters, so two passes differing only in
 *  their rests compare unequal and re-patch. */
[[nodiscard]] TextEffect pass(material::skia::Paint material);

/** THE ESCAPE HATCH: an ad-hoc effect body under an author-given key.
 *
 *  The key IS the identity — two effects with the same key compare equal
 *  and the reconciler will prune one onto the other, so give a different
 *  body a different key or the old one silently keeps drawing. Bake the
 *  parameters that vary into the key (or into `parameters`) rather than
 *  capturing them silently.
 *
 *  `reach` is how far past the element's box the body may push a glyph;
 *  the default covers the shipped presets' range.
 *
 *  THE ONE PLACEMENT FACT THE LIBRARY CANNOT INFER lives here too. Every
 *  other effect answers `TextEffect::displaces` for itself — a preset knows
 *  its own deviation, `fx::keys` reads its table, `fx::sequence`, `fx::mix` and
 *  `fx::hold` derive from their operands — but a lambda is opaque until it
 *  runs, so this door assumes the moving answer and takes
 *  `.displacing(false)` as the promise that the body leaves every pen
 *  position alone. */
[[nodiscard]] inline TextEffect effect(std::string key,
                                       GlyphModifierFunction program,
                                       float reach = 48.0f,
                                       std::vector<float> parameters = {}) {
  return TextEffect(std::move(key), std::move(parameters), std::move(program),
                    reach);
}

/** ONE ENTRY OF A `fx::keys` TABLE: where it sits in local time, the
 *  deviation there, and — optionally — the curve for the segment that
 *  STARTS at it. */
struct Key {
  float at = 0;            ///< local time, 0→1
  GlyphModifier modifier;  ///< the deviation at that moment
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
 *  Interpolation is COMPONENTWISE and follows the `fx::sequence` crossfade
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
 *  its glyphs out each time it did. A LOOPING CASCADE
 * (`motion::Spread::loopMs`) has nothing for this to withhold either: its fold
 * keeps every unit somewhere in its cycle — there is no "not yet" — and local
 * time touches 0 only at the instant a beat re-opens, so the veto blanks that
 * single instant and nothing else. An effect on a looping cascade gates its own
 *  arrival, the way a streak table's head is its own entrance. */
[[nodiscard]] TextEffect hold(TextEffect effect);

/** PHASES IN LOCAL TIME: each phase sees a renormalized 0→1 over its own
 *  window, so `fx::sequence(a.until(0.35f), b.until(0.75f).crossfade(0.10f),
 * c)` plays `a` over the first 35% of every unit's beat, `b` over the next 40%
 * and `c` over the rest — each running its full curve.
 *
 *  The default joint is a hard cut. `.crossfade(f)` on the ENDING phase lerps
 *  its deviation into the next one's, componentwise, over the last `f` of
 *  local time before the joint.
 *
 *  A sequence is NOT a keyframe table over effects, and neither combinator
 *  is the other's special case: a phase is an EFFECT re-clocked over its
 *  window and free to move throughout it, where a key is one deviation
 *  standing still and lerped toward. What they do share is the
 *  componentwise interpolation — the crossfade here and a segment there run
 *  the same arithmetic, so the substitutions cut the same way in both. */
[[nodiscard]] TextEffect sequence(std::vector<Phase> phases);
template <typename... Rest>
[[nodiscard]] TextEffect sequence(Phase first, Rest&&... rest) {
  std::vector<Phase> phases;
  phases.reserve(1 + sizeof...(Rest));
  phases.push_back(std::move(first));
  (phases.push_back(Phase(std::forward<Rest>(rest))), ...);
  return sequence(std::move(phases));
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

}  // namespace sigil::compose
