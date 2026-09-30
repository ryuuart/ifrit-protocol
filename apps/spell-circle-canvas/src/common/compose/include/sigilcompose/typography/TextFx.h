#pragma once

/** @file
 * @ingroup compose-typography
 *
 * SigilCompose typography — THE `textFx::` CATALOGUE: the effects the runtime
 * evaluates by STRUCTURE rather than by calling a preset's body. The
 * substitution, the shader pass, the keyframed tween and the colour tween,
 * the hold, the two combinators, and the escape hatch an ad-hoc body goes
 * through.
 *
 * The value they are all built as is <sigilcompose/typography/TextEffect.h>;
 * the entrance a glyph walks home is <sigilcompose/typography/Entrance.h>,
 * and the stock values over both — the entrances and the loop — are
 * <sigilcompose/typography/Presets.h>. What separates the shelves: a
 * preset is a plain value a caller may copy and change field by field,
 * and everything here is a door that builds an effect the runtime reads.
 */

#include <sigilcompose/core/Utf8.h>
#include <sigilcompose/typography/TextEffect.h>
#include <sigilmotion/values/Tween.h>

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace sigil::compose {

// ---------------------------------------------------------------------------
// The effects the RUNTIME evaluates by structure rather than by calling a
// preset's body: the substitution, the shader pass, the two tweens and the
// combinators over whole effects. Their bodies are the engine's; the
// presets that are plain values are in typography/Presets.h.

/** THE TEXT-EFFECT CATALOGUE: what one `textFx()` track does to each glyph
 *  it addresses — a substitution, a shader pass, a keyframed deviation, a
 *  colour reveal, a hold, the two combinators that compose whole effects,
 *  and the escape hatch an ad-hoc body goes through.
 *
 *  Everything here is a shape the RUNTIME reads and evaluates by
 *  structure. The presets that are plain values over the same seam —
 *  the entrances and the loop, in typography/Presets.h — are tweens and
 *  effects a caller copies and changes. Both reach a track as the same
 *  `TextEffect` type and compose in one track list. */
namespace textFx {

/** THE DISPLAY SIZE A REACH IS DECLARED AGAINST when no effect knows the
 *  font size at construction: a glyph grown by a factor about its own
 *  centre escapes its box by half the excess of this many pixels in each
 *  direction, and a keyframed tween's growths and leans are read against
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

/** The same effect over a charset spelled as UTF-8, the way every text
 *  door takes its words, so a charset read from a words file or shared with
 *  the text it churns reaches the effect as it is written. Each code point
 *  is one substitution candidate, whatever its encoded length; malformed
 *  UTF-8 is an empty charset, which substitutes nothing. */
[[nodiscard]] TextEffect scramble(const Utf8& charset, int steps = 14);

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
 *  unit's own schedule clock as uniform data, so per-letter treatment is
 *  data rather than scene structure and the cost is one draw plus one pass
 *  whatever the unit count is.
 *
 *      struct Burn { material::Color uEdge; };
 *      auto dissolve = std::make_shared<const material::Recipe>(
 *          material::Recipe::of<Burn>("ember.burn")
 *              .body(material::Target::SkSL, kBurnSksl));
 *      material::Material burn(dissolve, Burn{ink});
 *      text(u8"EMBER DECODE", display)
 *          .textFx({.effect = textFx::pass(burn),
 *                   .tween = {.delay = motion::stagger(260ms)},
 *                   .unit = weave::Unit::Cluster});
 *
 *  THE MATERIAL MUST BE A RECIPE INSTANCE over a recipe with an SkSL body, because the unit count is baked into the compiled
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
 *  `uUnitPhase[i].x` is the same `Beat::localTime`, driven by the track's
 *  progress through its schedule, so the pass, a mark and the glyphs can
 *  never disagree about the schedule. `uUnitPhase[i].y` is a per-unit seed
 *  in [1, 256), stable across frames and relayouts, which is what lets a
 *  seeded dissolve settle and cache instead of churning forever. Under a
 *  nested schedule there is one entry per (outer, inner) beat, matching the
 *  beats the query reports.
 *
 *  THE PASS IS BOUNDED, unlike a raw `Element::filter` shader: it paints
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
 *  A pass is a WHOLE-TRACK statement: inside `textFx::sequence`, `textFx::mix`
 *  or `textFx::hold` its material is not consulted and the operand contributes
 *  the identity. Sequence a pass by driving its progress; gate its onset in
 *  its own SkSL, which holds the whole schedule.
 *
 *  THE DECLARED REST — `textFx::pass(m).restsAt(0)`, `.restsAt(1)`,
 *  `.restsAt(0, 1)`: the author's promise that the SkSL is an EXACT
 *  pass-through at those unit phases — at a declared phase it returns its
 *  input pixels untouched. When every addressed unit's resolved local time
 *  sits on a declared phase, the runtime skips the layer and the shader
 *  and draws the batches directly, so a pass on a node that repaints for
 *  unrelated reasons (an orbiting `textOnPath` ring under a settled pass)
 *  stops paying for a shader that is changing nothing. The promise is
 *  UNVERIFIABLE, in the same family as `Track::reach` and a material's
 *  `bleed()`: declare a phase where the shader is not a pass-through and
 *  the picture POPS at the seam, snapping between shaded and raw glyphs as
 *  the schedule crosses the declared phase, with no diagnostic. The
 *  comparison is exact — which the schedule supplies, a one-shot schedule
 *  clamping a unit to exactly 0 before its beat and exactly 1 after. Under
 *  a LOOPING schedule (`Track::loop`) a unit touches 0 only at the
 *  instant its beat re-opens, so `restsAt(0)` effectively never engages
 *  there — correctly, the cycle is always mid-flight somewhere — while a
 *  unit RESTS at exactly 1 between beats, so `restsAt(1)` engages whenever
 *  no beat is mid-cycle. Undeclared, a pass always runs. The declaration
 *  rides the effect's comparable parameters, so two passes differing only in
 *  their rests compare unequal and re-patch. */
[[nodiscard]] TextEffect pass(material::Material material);

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
 *  THE ONE PLACEMENT FACT THE LIBRARY CANNOT INFER lives here too. Every other
 *  effect answers `TextEffect::displaces` for itself — a preset knows its own
 *  deviation, `textFx::tween` reads its stops, `textFx::sequence`,
 *  `textFx::mix` and `textFx::hold` derive from their operands — but a lambda
 *  is opaque until it runs, so this door assumes the moving answer and takes
 *  `.displacing(false)` as the promise that the body leaves every pen position
 *  alone. */
[[nodiscard]] inline TextEffect effect(std::string key,
                                       GlyphModifierFunction program,
                                       float reach = 48.0f,
                                       std::vector<float> parameters = {}) {
  return TextEffect(std::move(key), std::move(parameters), std::move(program),
                    reach);
}

/** A KEYFRAMED DEVIATION: Motion's one keyframe grammar over the
 *  `GlyphModifier` a track returns — `.from` where every unit starts,
 *  `.keyframes` the stops it passes through (`motion::Keyframe`: where it
 *  goes, its share of the path, its curve), or `.to` alone — run over one
 *  unit of local progress.
 *
 *      const TextEffect rubberBand = textFx::tween({
 *          .keyframes = {{.to = {.scaleX = 1.25f, .scaleY = 0.75f}, .duration = 300ms},
 *                        {.to = {.scaleX = 1.15f, .scaleY = 0.85f}, .duration = 200ms},
 *                        {.to = {}, .duration = 500ms}},
 *          .ease = motion::ease::inOutCubic});
 *
 *  THE SPELLING IS `tween`, beside `textFx::enter`, because the two read
 *  different values: `enter` walks a glyph HOME along a `Displaced` path,
 *  and this walks any deviation — a squash, a flash, a pulse that returns —
 *  which is no arrival. One name for both would also make a braced
 *  argument, which names no type, ambiguous between them.
 *
 *  WHERE A UNIT RUNS is the track's tween; this one says only the path.
 *  `.from` unset is the glyph at rest (`GlyphModifier{}`), and so is `.to`
 *  when neither it nor a keyframe is named. A keyframe's `duration` is its
 *  SHARE of the path, the way Motion resolves a step's length — one with
 *  none takes the tween's `duration` over the keyframe count, so a list
 *  with no durations is equal shares — and the whole path, however long it
 *  sums to, is the unit's local progress 0→1. Local time outside [0,1]
 *  holds the ends.
 *
 *  THE CURVE APPLIES PER SEGMENT, as Motion's keyframes do: every segment
 *  runs the whole curve — a keyframe's own `ease`, else the tween's — so
 *  the deviation AT a stop is exactly the stop. UNSET, A SEGMENT IS
 *  STRAIGHT (`motion::ease::linear`), not Motion's `outQuad` default: a
 *  collective running a unit's progress reads an empty curve as straight,
 *  and the stops are what the author placed.
 *
 *  Interpolation is `compose::interpolate` over `GlyphModifier` — the
 *  `textFx::sequence` crossfade's arithmetic: componentwise, with
 *  `codepoint` and a DIFFERING `axis` tag CUT at the middle of the segment
 *  rather than lerped, since there is no half-way glyph between two
 *  outlines; an `axis` lerps only between two stops naming the SAME tag.
 *
 *  The stops ARE the identity — two tweens over the same numbers, the same
 *  shares and the same named curves compare equal and prune — and the
 *  effect declares its own reach, and whether it displaces, from the
 *  offsets, growths and leans its stops publish. */
[[nodiscard]] TextEffect tween(motion::Tween<GlyphModifier> description);

/** A COLOUR REVEAL AS A SCHEDULE: one `motion::Tween` of colours — `.from`
 *  what the glyphs read at local 0, `.to` (or the last keyframe) what they
 *  read at local 1, any `.keyframes` between — a karaoke wipe, a highlight
 *  sweeping a word, an initial catching its colour as it lands.
 *
 *      text(u8"sung", lyric.fill(sung))
 *          .textFx({.effect = textFx::tint({.from = pale, .to = sung})})
 *
 *  THE ELEMENT IS SET IN THE COLOUR THE TWEEN RESTS AT, AND THE EFFECT
 *  MULTIPLIES DOWN TOWARD THE REST. That inversion is the one thing to get
 *  right here. A `GlyphModifier` carries `colorMultiplier`, a per-channel
 *  MULTIPLIER over every pass the glyph's style draws, and a multiplier can
 *  only take a colour toward black — so the DESTINATION is what the style
 *  paints, and every other stop is reached by dividing by it. The stops
 *  still read in time order and the division is done here: a line set in
 *  `sung` under `{.from = pale, .to = sung}` wipes from pale to sung. Set
 *  the line in `pale` and it draws pale throughout, which is the obvious
 *  first mistake and has no diagnostic.
 *
 *  Multiplying is also what lets this tint a gradient-filled or
 *  image-filled line without knowing what fills it. Its cost is that a
 *  DESTINATION CHANNEL OF ZERO cannot be departed from — nothing multiplies
 *  0 into anything else — so that channel holds at 0 for the whole ramp
 *  whatever the other stops say there. The way UP is the other two colour
 *  terms: `GlyphModifier::colorAdd` is the hard flash over whatever the
 *  style paints, `GlyphModifier::colorScreen` the glow that brightens toward
 *  white without clipping — both usually spoken through `textFx::tween`.
 *
 *  WHEN a unit runs is the track's tween; this one says only the path, and a
 *  keyframe's `duration` is its share of it, as in `textFx::tween`. Alpha is
 *  untouched: a reveal that also fades wants an alpha track, which composes
 *  with this one. UNSET, THE CURVE IS A SMOOTHSTEP (`motion::ease::smoothstep`)
 *  because a hard cut at display size flickers at any frame rate; the width
 *  of the edge is bought with the track's `duration`, not with the curve. A
 *  named curve runs every segment whole.
 *
 *  A TINT THAT NAMES NO REST — `.from` alone, no `.to` and no keyframes —
 *  COMES TO REST AT THE LEAF'S OWN INK, whatever that ink is when it
 *  paints: a colour a class or a custom property states, a ramp, an
 *  image. It is the form for a line inked through the cascade, because
 *  the colour is stated once, in the sheet:
 *
 *      text(u8"sung").ink(var("sung"))
 *          .textFx({.effect = textFx::tint({.from = pale})})
 *
 *  The rest is read off the glyph when the effect is applied —
 *  `GlyphInfo::ink`, the colour its ink paints it in there — and every
 *  stop is reached by dividing by it as above, so local 0 draws `from`,
 *  local 1 draws the ink, and a class that restates `--sung` moves only
 *  the rest. */
[[nodiscard]] TextEffect tint(motion::Tween<material::Color> description);

/** NOTHING UNTIL THE BEAT OPENS: `effect` as it is, except that a unit
 *  whose beat has not begun paints nothing at all.
 *
 *  A schedule hands every unit a local time clamped to [0,1], so a unit
 *  waiting its turn is handed 0 — and an effect that deviates at 0 is
 *  already performing before its beat. A substitution is the case that
 *  shows: `textFx::scramble` churns from local 0, so a glyph still waiting
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
 *  its glyphs out each time it did. A LOOPING SCHEDULE
 * (`Track::loop`) has nothing for this to withhold either: its fold
 * keeps every unit somewhere in its cycle — there is no "not yet" — and local
 * time touches 0 only at the instant a beat re-opens, so the veto blanks that
 * single instant and nothing else. An effect on a looping schedule gates its own
 *  arrival, the way a streak table's head is its own entrance. */
[[nodiscard]] TextEffect hold(TextEffect effect);

/** PHASES IN LOCAL TIME: each phase sees a renormalized 0→1 over its own
 *  window, so `textFx::sequence(a.until(0.35f),
 *  b.until(0.75f).crossfade(0.10f), c)` plays `a` over the first 35% of every
 *  unit's beat, `b` over the next 40% and `c` over the rest — each running its
 *  full curve.
 *
 *  The default joint is a hard cut. `.crossfade(f)` on the ENDING phase lerps
 *  its deviation into the next one's, componentwise, over the last `f` of
 *  local time before the joint.
 *
 *  A sequence is NOT a keyframed tween over effects, and neither combinator
 *  is the other's special case: a phase is an EFFECT re-clocked over its
 *  window and free to move throughout it, where a stop is one deviation
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

}  // namespace textFx

}  // namespace sigil::compose
