#pragma once

/** @file
 * Internal to the typography tier — the text engine's runtime side over the
 * kernel's descriptions and instances: the per-walk glyph structure every
 * track shares, selection resolution as glyphs and as text ranges, the
 * cascade arithmetic that turns one master progress into a local time per
 * glyph, the composition algebra of glyph deviations, and the engine
 * operations the painter value installed on a text description calls
 * through.
 */

#include <span>
#include <string>
#include <vector>

#include "ComposeRuntime.h"

namespace sigil::compose::detail {

/** ONE WALK'S GLYPHS, and which unit of each granularity they fall in.
 *
 *  Built once per paint from the finished layout and shared by every track
 *  on the element, because the expensive parts — walking the placed glyphs,
 *  numbering the words and lines — do not depend on which track is asking.
 *  Reused across frames: build() keeps the allocations. */
struct GlyphStructure {
  static constexpr size_t kUnits = 5;  ///< one lane per Unit enumerator

  std::vector<GlyphInfo> glyphs;  ///< in draw order, structure filled in
  /** Per Unit: glyph index → the unit it belongs to, numbered from 0 in
   *  draw order. */
  std::array<std::vector<uint32_t>, kUnits> unitOf;
  std::array<uint32_t, kUnits> unitCounts{};
  /** THE SAME LANES NUMBERED OVER THE STORY, for a leaf that is one frame
   *  of a chain: the beats of a cascade over a threaded story run on one
   *  clock across the whole of it, so the fortieth word is beat forty
   *  wherever it landed and a stagger does not restart at each frame.
   *
   *  Only three granularities have a story ordinal to report — the word,
   *  the sentence and the line each carry one on the placed glyph, because
   *  every frame builds the whole story's paragraph and resumes at a word.
   *  A CLUSTER AND A GLYPH DO NOT: a cluster's ordinal is a walk position
   *  and the walk is this frame's, so those two lanes are the frame's
   *  numbering and a cascade over them restarts per frame. Empty on a leaf
   *  that is not a frame of a chain, which then reads the lanes above and
   *  is numbered exactly as it always was. */
  std::array<std::vector<uint32_t>, kUnits> storyUnitOf;
  std::array<uint32_t, kUnits> storyUnitCounts{};

  TextScope scope;  ///< carried so every resolver reads one value

  void build(const sigil::weave::ParagraphLayout& layout,
             const sigil::weave::Paragraph& paragraph, TextScope scope = {});
};

/** Which glyphs a selector addresses: one byte per glyph, in walk order.
 *  A pattern that does not compile answers all-zero and warns once, and so
 *  does an `sel::style` name @p named does not carry. */
std::vector<uint8_t> resolveSelection(const Selector& selector,
                                      const GlyphStructure& structure,
                                      const sigil::weave::Paragraph& paragraph,
                                      std::span<const NamedRun> named);
/** The once-per-pattern diagnostic behind an unresolvable selector. */
void warnBadSelectorPattern(const std::u8string& pattern);
/** The once-per-name diagnostic behind an `sel::style` no run answers to. */
void warnNoSuchStyleName(const std::u8string& name);
/** The once-per-key diagnostic behind an `sel::inFrame` naming a frame no
 *  leaf in the tree carries. */
void warnNoSuchFrameKey(const std::u8string& key);
/** WHICH TEXT A SELECTOR ADDRESSES, as UTF-16 ranges rather than glyphs —
 *  the form span restyling needs, because a restyle happens on the
 *  Paragraph, before there are glyphs to point at.
 *
 *  Sorted, merged and non-overlapping. `|`, `&` and `!` are interval
 *  arithmetic over the text; the complement is taken against the whole
 *  text. `sel::line` reads @p lines, or @p columns where the passage is
 *  vertical and a line IS a column — the geometry a previous layout
 *  produced, passed as plain values rather than as a layout because the
 *  paragraph that layout belongs to is the one being replaced — and
 *  addresses nothing when both are empty. `Selector::take`/`drop` slice
 *  glyphs inside a unit, which no text range can express: an `sel::each`
 *  selector answers with its whole units and the slice warns once.
 *  `sel::style` reads @p named, which is why the table is built before the
 *  restyles that consume it run. */
std::vector<sigil::weave::CharRange> resolveTextRanges(
    const Selector& selector, sigil::weave::Paragraph& paragraph,
    sigil::weave::FontContext& fonts,
    std::span<const sigil::weave::LineMetrics> lines,
    std::span<const sigil::weave::ColumnMetrics> columns,
    std::span<const NamedRun> named, TextScope scope = {});

/** ONE TRACK'S CASCADE RESOLVED AGAINST A LAID-OUT PARAGRAPH: which beat
 *  every glyph falls in at each level, and the ladder those beats run on.
 *
 *  ONE BODY for the painter and for the `beatsOf` query. A second spelling
 *  would let a mark travelling beside a cascade be told a different
 *  schedule from the glyphs it is marking, which is the whole defect the
 *  query exists to close. Reused in place across frames: build() assigns
 *  into the per-glyph lanes rather than clearing them, so a page of
 *  animated type does not mint a pair of vectors per track per frame. */
struct TrackCascade {
  motion::Cascade cascade;
  std::vector<uint32_t> outerUnit;  ///< glyph → its beat
  std::vector<uint32_t> innerUnit;  ///< glyph → its beat inside that beat;
                                    ///< empty without a nested cascade

  /** THE GLYPH ADAPTER, and the whole of what compose adds to a schedule:
   *  @p track says what a unit is (`over`, `innerOver`) and which list the
   *  beats are numbered against (`beatsOver`), this walk turns the laid-out
   *  glyphs into those numbers, and the arithmetic over them is
   *  SigilMotion's. */
  void build(const Track& track, const GlyphStructure& structure,
             const std::vector<uint8_t>& selected);
};

/** The composition algebra, in one place: offsets, rotations and shears ADD,
 *  scale, alpha and the colour multiplier MULTIPLY, the additive colour term
 *  ADDS and the screen term SCREENS. Stacked tracks, fx::mix, a seq
 *  crossfade and a keys segment all go through these two, so they cannot
 *  drift apart. */
void compose(GlyphMod& into, const GlyphMod& next);
GlyphMod lerpMod(const GlyphMod& a, const GlyphMod& b, float w);
/** FIELD PIN for GlyphMod (see the FIELD PINS block above) — defined beside
 *  the two functions it guards, never called. */
void glyphModFieldPin(GlyphMod& v);
/** The seed an effect's random stream is constructed from — the glyph's
 * identity plus the operand lane inside a composite. */
uint64_t glyphSeed(const GlyphInfo& g, uint32_t lane = 0);

// ---------------------------------------------------------------------------
// The engine operations, as the kernel's seam value calls them. Each takes
// the composer's retained state explicitly: the kernel holds the fonts,
// the clock and the layout the text was measured by, and the engine reads
// them through it.

/** THE ONE GLYPH DRAW for text that is not simply resting on its own
 *  straight baseline. The rest pose comes from the baseline — level on a
 *  plain run, on the curve and turned to it on a path run — and every
 *  fx() track's deviation applies ON TOP OF IT, in that pose's own frame.
 *  `onPath` is null for text with no baseline path. A track whose effect
 *  is a PASS (fx::pass) renders its addressed glyphs — deviations
 *  applied — into a layer instead of the canvas and runs its material
 *  once over that layer; `ctx` is the node's paint context, which that
 *  resolve reads for its clock, box and injected uniforms. */
void paintTextFx(Composer::Impl& impl, Instance& inst, SkCanvas& canvas,
                 const sigil::weave::PaintStyle* override,
                 const TextPath* onPath, SkSize size, const PaintContext& ctx);
/** Breaks the run across the baseline's contours through SigilWeave's
 *  contour-interval geometry, and caches the result on the instance. */
void ensurePathLayout(Composer::Impl& impl, Instance& inst,
                      const TextPath& spec, SkSize size);
/** THE SCHEDULE ONE TRACK IS RUNNING, resolved against the layout the
 *  last draw() produced — the read-back behind Composer::beatsOf. Rects
 *  come out in the NODE's own space; the caller offsets them into the
 *  composer's, as the bounds query does. */
std::vector<Beat> beatsOfTrack(Composer::Impl& impl, Instance& inst,
                               size_t trackIndex);
/** WHERE THE UNITS A SELECTOR ADDRESSES LANDED, one entry each, in draw
 *  order — the read-back behind Composer::units. Read off the same layout
 *  the letters are drawn from and built from the same pose and the same
 *  advance box a mark and a beat are built from, so an annotation placed
 *  from this and a mark anchored to the same selection cannot disagree
 *  about where a unit is. Rects come out in the NODE's own space. */
std::vector<TextUnit> unitsOfText(Composer::Impl& impl, Instance& inst,
                                  const Selector& selector, Unit unit);
/** THE BAND A TEXT'S RESERVING READINGS NEED, from their own metrics
 *  alone: the tallest reading's line height plus its standoff, on each
 *  side that carries one. Asked BEFORE the base is laid out. */
sigil::weave::ReservedBand reservedBandOf(
    Composer::Impl& impl, std::span<const Annotation> annotations);
/** LAYS OUT EVERY READING against the placement the base reached, leaving
 *  the results on the instance for the kernel to draw. A base unit that
 *  broke across a line or a column has its reading split between the
 *  pieces in proportion to the base's advance either side. */
void resolveTextAnnotations(Composer::Impl& impl, Instance& inst);
/** THE SAME SCHEDULE'S WHOLE VIRTUAL SPAN in ms — the read-back behind
 *  Composer::cascadeSpanMs, resolved by the same body as beatsOfTrack.
 *  0 wherever beatsOfTrack answers empty. */
float cascadeSpanOfTrack(Composer::Impl& impl, Instance& inst,
                         size_t trackIndex);
/** WHERE EACH mark() ANCHORS, refilling `textMarkRects` from the layout
 *  the letters are drawn from: one rect per anchor, the union of the
 *  advance boxes of the glyphs its selector addressed. A flow run's
 *  marks resolve during measure; a PATH run's resolve in ensureLayout's
 *  post-layout pass, because the curve resolves against the node's
 *  final box and the marks then stand on it — at the run's resting
 *  placement, since a layout rect cannot chase a paint-time `at`. */
void resolveTextMarks(Composer::Impl& impl, Instance& inst);
/** Whether one spanStyle restyle can be carried as draw-time axis tracks
 *  instead of re-shaping the text it covers: its style must differ from
 *  every covered span's only in variable-font axes, drop none the text
 *  was shaped with, and every axis it moves must be advance-invariant on
 *  that span's face. On success @p axes holds one (tag, coordinate) per
 *  axis that actually changes.
 *
 *  @p paintCarried is the text whose paint an earlier declaration owns:
 *  a span wholly inside it is compared on its other dimensions alone,
 *  because a fold writes no paint and leaves that colour standing. */
bool foldableAsAxes(Composer::Impl& impl, const sigil::weave::TextStyle& style,
                    std::span<const sigil::weave::CharRange> ranges,
                    const sigil::weave::Paragraph& paragraph,
                    std::span<const sigil::weave::CharRange> paintCarried,
                    std::vector<std::pair<std::string, float>>& axes);

}  // namespace sigil::compose::detail
