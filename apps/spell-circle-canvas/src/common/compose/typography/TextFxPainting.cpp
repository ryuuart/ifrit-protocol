/** @file
 * Kinetic typography at paint: the two substitution gates a dressed glyph
 * can ask for, the glyph-paint override, the band a glyph occupies, and the
 * fx painter itself — master progress, stagger remap, per-glyph deviation,
 * batched RSXform draws and the pass lanes.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkShader.h>
#include <include/core/SkStrokeRec.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/effects/SkTrimPathEffect.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilweave/choreograph/Choreograph.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/fonts/Shaper.h>  // makeFont — textFill's cap-height metrics

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "AxisGate.h"
#include "ComposeRuntime.h"
#include "PaintInternal.h"
#include "TextEngine.h"
#include "TextPose.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Kinetic typography: master progress → stagger remap → per-glyph mods →
// batched RSXform draws (one per font/color bucket — never per glyph).

// ---------------------------------------------------------------------------
// The two SUBSTITUTION gates a dressed glyph can ask for — a driven
// variable-font axis, and a code-point swap. Both replace what the SHAPER
// decided while keeping the pen positions it computed, so both are refused
// wherever that would move a letter, and both memoize their verdict: a
// verdict is a property of the face, and probing one costs metrics calls.
// The axis gate is `detail::axisGate`, shared so every place that judges a
// driven axis reads one verdict.

namespace {

/** How many steps the driven-axis ladder offers a glyph rendered at
 *  `pixelSize`.
 *
 *  A driven coordinate is snapped so a smooth sweep lands on a BOUNDED set
 *  of faces: every distinct coordinate is a distinct clone, a distinct batch
 *  bucket and a distinct set of glyph-atlas strikes.
 *
 *  How coarse that ladder may be is a visual question, and the answer
 *  depends on the size the glyph is rendered at. One step is a fixed
 *  distance in the axis's own design units; a design unit displaces an
 *  outline by a fixed fraction of the em; and a fixed fraction of the em is
 *  MORE PIXELS the larger the em is drawn. So one step's visible effect
 *  grows in proportion to the rendered size, and a step count that does not
 *  grow with it is a ladder that disappears on a caption and shows on a
 *  headline. It rises in proportion instead.
 *
 *  Both ends are clamped. Below the floor a finer ladder buys nothing the
 *  eye can use at any size the type is still legible at; the ceiling is what
 *  makes the retained clone population bounded at all, which is the only
 *  reason a ladder exists rather than the raw coordinate. */
int axisLadderSteps(float pixelSize) {
  constexpr float kStepsPerPixel = 4.0f;
  constexpr int kMinSteps = 64, kMaxSteps = 512;
  return std::clamp((int)std::lround(pixelSize * kStepsPerPixel), kMinSteps,
                    kMaxSteps);
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
  static thread_local std::map<Key, Verdict> table;
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
  static thread_local std::unordered_set<uint64_t> warned;
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

}  // namespace

GlyphBand bandOf(const sigil::weave::ShapedWord* shaped,
                 std::vector<std::pair<BandKey, GlyphBand>>& memo) {
  if (!shaped || !shaped->typeface) return {};
  const BandKey key{shaped->typeface.get(), shaped->fontSize};
  for (const auto& [seen, band] : memo)
    if (seen == key) return band;
  SkFontMetrics metrics;
  sigil::weave::makeFont(shaped->typeface, shaped->fontSize)
      .getMetrics(&metrics);
  // Skia reports the ascent as a NEGATIVE offset from the baseline; the band
  // wants both halves positive.
  const GlyphBand band{-metrics.fAscent, metrics.fDescent};
  memo.emplace_back(key, band);
  return band;
}

/** One glyph's advance box, placed and turned the way the layout placed and
 *  turned it, as an axis-aligned bound.
 *
 *  The box is taken around the ADVANCE CENTRE the rest pose reports, which
 *  is what makes one rule cover all four baselines: a wrapped line and a
 *  mixed-style run differ only in where the centre and the band are, a path
 *  run and a rotated column run differ only in which way the box is turned,
 *  and an upright vertical glyph's advance runs down the column instead of
 *  across it. ONE body for three readers — the beatsOf query, the mark
 *  resolver and a pass track's uUnitRect — so none can disagree about
 *  where a unit is. */
SkRect glyphBox(const sigil::weave::PlacedGlyph& placed, const RestPose& pose,
                const GlyphBand& band) {
  const float size = placed.shaped ? placed.shaped->fontSize : 0.0f;
  const bool upright = placed.shaped && placed.shaped->vertical;
  const float halfAlong = std::abs(placed.advance) * 0.5f;
  // Along the advance, then across it. An upright vertical glyph advances
  // DOWN its column and is about one em wide across it; everything else
  // advances along its baseline and stands `band` tall across it.
  const float x0 = upright ? -size * 0.5f : -halfAlong;
  const float x1 = upright ? size * 0.5f : halfAlong;
  const float y0 = upright ? -halfAlong : -band.ascent;
  const float y1 = upright ? halfAlong : band.descent;
  SkRect box = SkRect::MakeEmpty();
  bool first = true;
  for (const SkPoint corner :
       {SkPoint{x0, y0}, SkPoint{x1, y0}, SkPoint{x1, y1}, SkPoint{x0, y1}}) {
    const SkPoint at{
        pose.centre.x() + corner.x() * pose.cosine - corner.y() * pose.sine,
        pose.centre.y() + corner.x() * pose.sine + corner.y() * pose.cosine};
    if (first) {
      box = SkRect::MakeLTRB(at.x(), at.y(), at.x(), at.y());
      first = false;
    } else {
      box.fLeft = std::min(box.fLeft, at.x());
      box.fTop = std::min(box.fTop, at.y());
      box.fRight = std::max(box.fRight, at.x());
      box.fBottom = std::max(box.fBottom, at.y());
    }
  }
  return box;
}

/** The seed a pass hands its unit in `uUnitPhase[i].y`: distinct per
 *  (outer, inner) beat, in [1, 256), and a pure function of the beat's
 *  numbering — so it is the same seed on every frame and after every
 *  relayout, which is what lets a seeded dissolve settle and cache. */
float passUnitSeed(uint32_t outer, uint32_t inner) {
  Rng rng(((uint64_t)outer << 32u) | (uint64_t)inner);
  return 1.0f + rng.unit() * 255.0f;
}

void detail::paintTextFx(Composer::Impl& impl, Instance& inst, SkCanvas& canvas,
                         const sigil::weave::PaintStyle* override,
                         const TextPath* onPath, SkSize size,
                         const PaintContext& ctx) {
  if (!inst.paragraph) return;  // no content materialized: nothing to draw
  static thread_local std::vector<Track> joinedTracks;
  const std::span<const Track> tracks = paintedTracksOf(inst, joinedTracks);

  // ONE COMPOSITION ORDER, stated here because everything below assumes it:
  // THE BASELINE PLACES THE GLYPH, THEN THE TRACKS DEVIATE FROM THAT
  // PLACEMENT, IN THE FRAME THE BASELINE PUT IT IN. On a path run that
  // means a rise lifts a letter off the curve along its own local
  // perpendicular rather than straight up the canvas, and a track's
  // rotation adds to the tangent it was already turned to. The two are not
  // alternatives and neither wins: `fx()` and `onPath()` compose.
  if (onPath) ensurePathLayout(impl, inst, *onPath, size);
  const bool ridesPath = onPath && textStateOf(inst).pathValid;
  if (onPath && !ridesPath) return;  // no measurable baseline: nothing rides
  const sigil::weave::ParagraphLayout& layout =
      ridesPath ? textStateOf(inst).pathLayout : inst.textLayout;

  // ONE walk builds the structure every track selects and staggers over —
  // the glyph list with its word/line/cluster/sentence numbering. Held
  // thread-locally: a frame of animated text rebuilds it, but never
  // reallocates it.
  static thread_local detail::GlyphStructure structure;
  structure.build(layout, *inst.paragraph);
  const uint32_t count = (uint32_t)structure.glyphs.size();
  if (count == 0) return;

  // WHERE ALONG the baseline the run sits this frame, as arc length. The
  // layout was built at the phase's RESTING value, so this is the delta the
  // paint applies on top — zero for a phase that never moves, and the whole
  // marquee for one bound to an output. It shifts the run's ENTRY POINT,
  // which is the same shift whichever way the run reads.
  float phaseArc = 0;
  if (ridesPath)
    phaseArc = (inst.resolvePathAt() - textStateOf(inst).pathRestAt) *
               textStateOf(inst).pathTotalLength;

  // Selections, resolved once per (content, layout width, selector list).
  // A regular expression over the paragraph is a per-EDIT cost this way,
  // not a per-frame one.
  bool selectionsStale =
      textStateOf(inst).selectionRev != inst.contentRev ||
      textStateOf(inst).selectionWidth != inst.measuredForWidth ||
      textStateOf(inst).selectionKeys.size() != tracks.size();
  if (!selectionsStale)
    for (size_t i = 0; i < tracks.size(); ++i)
      if (!(textStateOf(inst).selectionKeys[i] == tracks[i].where)) {
        selectionsStale = true;
        break;
      }
  if (!selectionsStale)
    for (const std::vector<uint8_t>& mask : textStateOf(inst).selectionMasks)
      if (mask.size() != count) {
        selectionsStale = true;
        break;
      }
  if (selectionsStale) {
    textStateOf(inst).selectionKeys.clear();
    textStateOf(inst).selectionMasks.clear();
    textStateOf(inst).selectionKeys.reserve(tracks.size());
    textStateOf(inst).selectionMasks.reserve(tracks.size());
    for (const Track& track : tracks) {
      textStateOf(inst).selectionKeys.push_back(track.where);
      textStateOf(inst).selectionMasks.push_back(detail::resolveSelection(
          track.where, structure, *inst.paragraph, inst.textNamedRuns));
    }
    textStateOf(inst).selectionRev = inst.contentRev;
    textStateOf(inst).selectionWidth = inst.measuredForWidth;
  }

  // Each track's cascade, numbered against whichever list its stagger names
  // — its own selection by default, the whole paragraph under beats::Text.
  struct Resolved {
    const Track* track = nullptr;
    const std::vector<uint8_t>* selected = nullptr;
    detail::TrackCascade resolved;
    float master = 1.0f;
  };
  // Kept across frames and reused in place: clearing would drop the two
  // per-glyph vectors' allocations, which is a fresh pair per track per
  // text node per frame on a page full of animated type.
  static thread_local std::vector<Resolved> resolved;
  size_t used = 0;
  for (size_t i = 0; i < tracks.size(); ++i) {
    const Track& track = tracks[i];
    if (!track.effect) continue;
    if (used == resolved.size()) resolved.emplace_back();
    Resolved& r = resolved[used++];
    r.track = &track;
    r.selected = &textStateOf(inst).selectionMasks[i];
    const AnimatedFloat* anim =
        i < inst.trackAnims.size() ? inst.trackAnims[i].get() : nullptr;
    r.master =
        std::clamp(inst.resolveFloatAt(anim, track.progress), 0.0f, 1.0f);
    r.resolved.build(track.stagger, structure, *r.selected);
  }
  // A path run with no tracks still draws: every glyph keeps the identity
  // deviation and rests on the curve.
  if (used == 0 && !ridesPath) return;
  const std::span<const Resolved> live(resolved.data(), used);

  // A TRACK DRAWS ITS OWN GLYPHS, in batched buckets, and a bucket carries
  // glyphs alone — so a band a span asked for (an underline, a
  // strikethrough, the sideline beside a column) is not drawn on a node
  // that moves. Say so once rather than leaving an author to discover it
  // by its absence.
  for (const sigil::weave::StyleSpan& span : inst.paragraph->spans()) {
    if (span.style.paint.decorations.empty()) continue;
    static thread_local bool warnedAboutBands = false;
    if (!warnedAboutBands) {
      warnedAboutBands = true;
      SkDebugf(
          "sigilcompose fx: a span of this text asks for a decoration and "
          "the text also carries an fx() track, which draws its glyphs "
          "itself — the band is not drawn. Split the two: the passage that "
          "wears the band stands still, and the one that moves wears "
          "none.\n");
    }
    break;
  }

  // PASS TRACKS (fx::pass): each renders its addressed glyphs into its own
  // lane instead of the canvas, accumulating one rect and one local time
  // per (outer, inner) beat — the same enumeration beatsOfTrack reports,
  // from the same TrackCascade, so the pass and the query cannot disagree
  // about the schedule. A glyph a pass addresses draws only inside that
  // pass's layer; a glyph two passes address renders in both.
  struct PassLane {
    const Resolved* source = nullptr;
    sigil::weave::GlyphRSXformBatches batches;
    std::vector<std::pair<uint32_t, uint32_t>> keys;  // (outer, inner) beats
    std::vector<SkRect> rects;                        // one per beat
    std::vector<float> locals;                        // localT per beat
  };
  std::vector<std::unique_ptr<PassLane>> passes;
  for (const Resolved& r : live)
    if (r.track->effect.passMaterial()) {
      passes.push_back(std::make_unique<PassLane>());
      passes.back()->source = &r;
    }
  std::vector<std::pair<BandKey, GlyphBand>> bandMemo;

  static thread_local sigil::weave::GlyphRSXformBatches batches;
  batches.clear();

  // WHERE THIS RUN'S GLYPHS LAND MOVES FROM FRAME TO FRAME, which is what
  // decides whether their origins go on Skia's subpixel phase grid or on
  // whole pixels. Three ways a run creeps: a driven baseline phase (the
  // marquee runs under the type), a driven transform at or above the node
  // (the figure turns under the type), and a live fx() track whose effect
  // displaces (the letters travel under their own schedule). All three make
  // every letter's device position advance by a fraction of a pixel per
  // frame, which whole-pixel origins cannot express — each letter stands
  // still and then hops a whole pixel at its own moment.
  //
  // Read off the DECLARATION rather than off a frame-to-frame diff, so a
  // marquee parked at a phase keeps the placement it was turning with. A
  // diff would hand a stopping ring one last quarter-pixel shift at the
  // moment it settled, which is a tick in exactly the place a tick is most
  // visible.
  const bool pathDriven =
      ridesPath && (onPath->at.binding() ||
                    (inst.anims[Instance::kTextPathAt] &&
                     inst.anims[Instance::kTextPathAt]->value.isConnected()));
  batches.subpixel = pathDriven || inst.placementUnderMotion;
  // A pass lane draws the same run into its own layer, and a letter must
  // not sit one place inside a pass and another outside it.
  for (const std::unique_ptr<PassLane>& lane : passes)
    lane->batches.subpixel = batches.subpixel;

  const PoseContext poseCtx{&inst, &layout, onPath, ridesPath, phaseArc};

  uint32_t ordinal = 0;
  sigil::weave::forEachPlacedGlyph(
      layout, *inst.paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
        const uint32_t g = ordinal++;
        if (g >= count) return;
        RestPose pose;
        if (!restPoseOf(poseCtx, placed, pose)) return;
        GlyphInfo info = structure.glyphs[g];

        // Every track that addresses this glyph, composed: offsets, shear
        // and rotations add, scale, alpha and the colour multiplier
        // multiply, and the two substitutions are last-one-wins.
        // A glyph no track addresses keeps the identity deviation and
        // draws at rest.
        GlyphMod mod;
        bool continuous = false;
        for (const Resolved& r : live) {
          if (!(*r.selected)[g]) continue;
          // A pass deviates nothing per glyph — its whole evaluation is
          // the layer draw below, downstream of every deviation here.
          if (r.track->effect.passMaterial()) continue;
          const detail::TrackCascade& rc = r.resolved;
          info.unitIndex = rc.outerUnit[g];
          info.unitCount =
              std::max<uint32_t>((uint32_t)rc.cascade.outerOrder.size(), 1u);
          const float t =
              rc.cascade.localTime(r.master, rc.outerUnit[g],
                                   rc.innerUnit.empty() ? 0u : rc.innerUnit[g]);
          Rng rng(detail::glyphSeed(info));
          detail::compose(mod, r.track->effect(info, t, rng));
          continuous |= r.track->continuous;
        }
        if (mod.alpha <= 0.003f || mod.scale <= 0.001f) return;
        // SNAP what an effect can drive continuously. Every distinct value
        // below is a distinct batch bucket AND a distinct glyph-atlas
        // strike, so a smooth sweep left alone would rasterize every
        // addressed letter afresh on every frame. Track::continuous is the
        // opt-out, and it buys smoothness with exactly that cost.
        const auto snap = [continuous](float value, float ceiling) {
          const float clamped = std::clamp(value, 0.0f, ceiling);
          return continuous ? clamped : std::round(clamped * 32.0f) / 32.0f;
        };
        // The colour multiplier's ALPHA folds into the fade rather than
        // snapping on a ladder of its own: two independent 32-step alphas
        // would be a thousand buckets where one is thirty-two.
        const float alpha = snap(mod.alpha * mod.colorMul.fA, 1.0f);
        if (alpha <= 0.0f) return;
        // A multiplier above 1 brightens, which is a legitimate tint; the
        // ceiling is only there so a runaway number cannot mint buckets
        // without bound.
        constexpr float kTintCeiling = 4.0f;
        const SkColor4f tint{snap(mod.colorMul.fR, kTintCeiling),
                             snap(mod.colorMul.fG, kTintCeiling),
                             snap(mod.colorMul.fB, kTintCeiling), 1.0f};
        // The additive and screen terms ride the same 32-step ladder,
        // ceilinged at 1: an add past full is clamped at the draw anyway,
        // and a screen past full has no headroom left to lift. RGB only —
        // their alpha components state nothing at a draw. The snap is what
        // bounds the memoized filter population, and Track::continuous
        // lifts it here exactly as it does for the multiplier.
        const SkColor4f flash{snap(mod.colorAdd.fR, 1.0f),
                              snap(mod.colorAdd.fG, 1.0f),
                              snap(mod.colorAdd.fB, 1.0f), 0.0f};
        const SkColor4f glow{snap(mod.colorScreen.fR, 1.0f),
                             snap(mod.colorScreen.fG, 1.0f),
                             snap(mod.colorScreen.fB, 1.0f), 0.0f};
        float cosv = 1.0f, sinv = 0.0f;
        if (mod.rotateDeg != 0) {
          const float radians = mod.rotateDeg * 0.017453293f;
          if (continuous) {
            cosv = std::cos(radians);
            sinv = std::sin(radians);
          } else {
            // The SAME size-cut ladder the baseline's tangent takes, and
            // for the same reason: a track's rotation composes with that
            // tangent onto one glyph, so a coarser ladder here would be the
            // coarsest thing in the letter's motion and would tick where the
            // baseline no longer does. Track::continuous is still the opt-out
            // that buys the exact angle at a fresh strike per letter per
            // frame.
            sigil::weave::quantizeAngle(
                radians,
                tangentLadderSteps(placed.shaped ? placed.shaped->fontSize
                                                 : 0.0f),
                cosv, sinv);
          }
        }
        const float halfAdvance = placed.advance * 0.5f;
        // The glyph-local back-out from the pose centre to the draw origin.
        // Horizontal type and tate-chu-yoko take the (halfAdvance, 0) the
        // RSXform convention assumes; an upright glyph in a column does not,
        // because half of ITS advance is a step down the page.
        const SkVector local =
            pose.centreOffset.value_or(SkVector{halfAdvance, 0});
        // THE DEVIATION APPLIES IN THE REST POSE'S OWN FRAME. On a level
        // baseline that is the canvas frame and this is the identity, so a
        // plain run is untouched; on a curve it is what makes `fx::rise`
        // lift a letter off the CURVE rather than off the canvas, and what
        // keeps a stagger's shove tangential to the lettering it belongs
        // to. The rotations compose the same way: the track's angle turns
        // the glyph from wherever the baseline had already turned it.
        const SkPoint centre = {
            pose.centre.x() + pose.cosine * mod.dx - pose.sine * mod.dy,
            pose.centre.y() + pose.sine * mod.dx + pose.cosine * mod.dy};
        const float turnCos = pose.cosine * cosv - pose.sine * sinv;
        const float turnSin = pose.sine * cosv + pose.cosine * sinv;

        // The whole span style rides along, so a letter in flight keeps the
        // gradient, stroke and glow passes it was styled with — and the
        // textFill/textStroke override, when the node carries one.
        sigil::weave::GlyphDress dress;
        dress.alphaScale = alpha;
        dress.colorMul = tint;
        dress.colorAdd = flash;
        dress.colorScreen = glow;
        if (pose.centreOffset) dress.centreOffset = &*pose.centreOffset;
        if (mod.axis && placed.shaped)
          dress.face =
              drivenFace(impl.fonts, placed.shaped->typeface,
                         placed.shaped->fontSize, *mod.axis, continuous);
        SkGlyphID glyph = placed.glyph;
        if (mod.codepoint && placed.shaped)
          if (const SkGlyphID substitute = substituteGlyph(
                  impl.fonts, placed.shaped->typeface, placed.glyph,
                  mod.codepoint, placed.shaped->vertical))
            glyph = substitute;

        // ROUTING, per glyph and not per node: an RSXform carries a
        // rotation and ONE scale, so a glyph that shears or scales
        // unevenly draws under its own matrix while every glyph that does
        // not keeps the shared transform array untouched.
        SkMatrix matrix;
        if (mod.skewXDeg != 0 || mod.skewYDeg != 0 || mod.scaleX != 1 ||
            mod.scaleY != 1) {
          matrix.setAll(turnCos, -turnSin, centre.x(), turnSin, turnCos,
                        centre.y(), 0, 0, 1);
          // ONE shear carrying both angles, as the node's own skew lanes
          // take them — not an x shear applied after a y one, which would
          // put a product of the two tangents on the diagonal and scale the
          // glyph as well as leaning it.
          if (mod.skewXDeg != 0 || mod.skewYDeg != 0)
            matrix.preSkew(std::tan(mod.skewXDeg * 0.017453293f),
                           std::tan(mod.skewYDeg * 0.017453293f));
          matrix.preScale(mod.scale * mod.scaleX, mod.scale * mod.scaleY);
          // Innermost, so the pivot shift rides the scale exactly as it
          // does inside an RSXform.
          matrix.preTranslate(-local.x(), -local.y());
          dress.matrix = &matrix;
        } else {
          dress.center = centre;
          dress.cosine = turnCos * mod.scale;
          dress.sine = turnSin * mod.scale;
        }
        // ROUTE: a glyph a pass addresses is drawn inside that pass's
        // layer — with the deviation just composed, because the pass reads
        // pixels and the deviated pixels are the node's truth — and never
        // directly as well, which would double it under the pass's output.
        bool inPass = false;
        for (const std::unique_ptr<PassLane>& lane : passes) {
          if (!(*lane->source->selected)[g]) continue;
          inPass = true;
          lane->batches.addGlyph(placed.shaped,
                                 override ? *override : *placed.paint, glyph,
                                 halfAdvance, dress);
          // The beat this glyph belongs to, and its box joined into that
          // beat's rect — the same (outer, inner) walk beatsOfTrack takes.
          const detail::TrackCascade& rc = lane->source->resolved;
          const uint32_t outer = rc.outerUnit[g];
          const uint32_t inner = rc.innerUnit.empty() ? 0u : rc.innerUnit[g];
          const SkRect box =
              glyphBox(placed, pose, bandOf(placed.shaped, bandMemo));
          bool joined = false;
          for (size_t i = lane->keys.size(); i-- > 0;)
            if (lane->keys[i].first == outer && lane->keys[i].second == inner) {
              lane->rects[i].join(box);
              joined = true;
              break;
            }
          if (!joined) {
            lane->keys.emplace_back(outer, inner);
            lane->rects.push_back(box);
            lane->locals.push_back(
                rc.cascade.localTime(lane->source->master, outer, inner));
          }
        }
        if (!inPass)
          batches.addGlyph(placed.shaped, override ? *override : *placed.paint,
                           glyph, halfAdvance, dress);
      });
  batches.draw(&canvas);

  // THE PASSES, in track declaration order, each once: record the lane's
  // glyphs as a picture, hand it to the material as `uContent`, upload the
  // per-beat rects and phases, and draw ONE rect — the node's box grown by
  // the track's reach, which is the pass's whole footprint. The picture
  // shader rasterizes at the device's resolution, so the letters stay
  // sharp on a scaled host with no supersampled bake; everything is in the
  // node's own px, the frame main(xy) receives.
  for (const std::unique_ptr<PassLane>& lane : passes) {
    const auto n = (uint32_t)lane->keys.size();
    if (n == 0) continue;  // the selection resolved nothing: nothing to burn
    // THE DECLARED REST (fx::pass(…).restsAt(…)): when EVERY beat's
    // resolved local time sits on a declared pass-through phase, the layer
    // and the shader are skipped and the glyphs draw directly — the route
    // the compile refusal already takes, so a resting pass is
    // byte-identical to resting type. The test is EXACT equality, which is
    // what the schedule supplies: a one-shot cascade clamps a unit to
    // exactly 0 before its beat and exactly 1 after it. Under a looping
    // cascade a unit touches 0 only at the instant its beat re-opens, so a
    // rest declared at 0 effectively never engages there — correctly, the
    // cycle is always mid-flight somewhere — while units genuinely REST at
    // exactly 1 between beats, so a rest at 1 engages whenever no beat is
    // mid-cycle.
    const std::span<const float> rests =
        lane->source->track->effect.restPhases();
    if (!rests.empty()) {
      bool atRest = true;
      for (const float local : lane->locals) {
        bool declared = false;
        for (const float rest : rests) declared |= local == rest;
        if (!declared) {
          atRest = false;
          break;
        }
      }
      if (atRest) {
        lane->batches.draw(&canvas);
        continue;
      }
    }
    const float reach = lane->source->track->reachPx();
    const SkRect bounds =
        SkRect::MakeWH(size.width(), size.height()).makeOutset(reach, reach);
    // THE LAYER'S OWN FRAME is tile space — `bounds` translated so its
    // reach corner is the origin — and this is the ONE place node px and
    // tile px meet: the recording translates the glyphs in, and the local
    // matrix below translates the sampling back out. Skia's picture shader
    // anchors its rasterized tile at the local origin whatever the tile
    // rect's own origin says (the tile's offset survives into neither
    // backend's sampling matrix), so a tile handed to it starting at
    // (-reach, -reach) would land the whole layer a reach away from the
    // glyphs. Everything outside this pairing — main(xy), uUnitRect, the
    // rect drawn — stays in the node's own px and never sees tile space.
    const SkRect tile = SkRect::MakeWH(bounds.width(), bounds.height());
    SkPictureRecorder recorder;
    SkCanvas* layerCanvas = recorder.beginRecording(tile);
    layerCanvas->translate(reach, reach);
    lane->batches.draw(layerCanvas);
    const sk_sp<SkPicture> layer = recorder.finishRecordingAsPicture();
    static thread_local std::vector<float> rects, phases;
    rects.clear();
    phases.clear();
    rects.reserve((size_t)n * 4);
    phases.reserve((size_t)n * 2);
    for (uint32_t i = 0; i < n; ++i) {
      const SkRect& r = lane->rects[i];
      rects.insert(rects.end(), {r.x(), r.y(), r.width(), r.height()});
      phases.push_back(lane->locals[i]);
      phases.push_back(passUnitSeed(lane->keys[i].first, lane->keys[i].second));
    }
    detail::TextPassInputs inputs;
    const SkMatrix toTile = SkMatrix::Translate(-reach, -reach);
    inputs.content = layer->makeShader(SkTileMode::kDecal, SkTileMode::kDecal,
                                       SkFilterMode::kLinear, &toTile, &tile);
    inputs.rects = rects.data();
    inputs.phases = phases.data();
    inputs.units = n;
    const Material* material = lane->source->track->effect.passMaterial();
    sk_sp<SkShader> pass = material->resolvePass(inputs, ctx);
    if (!pass) {
      // The refusal already said why (no source, or it does not compile):
      // show resting letters rather than nothing, so the text survives
      // the mistake.
      lane->batches.draw(&canvas);
      continue;
    }
    SkPaint paint;
    paint.setShader(std::move(pass));
    canvas.drawRect(bounds, paint);
  }
}

}  // namespace sigil::compose
