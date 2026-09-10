/** @file
 * THE FX PAINTER: master progress → stagger remap → per-glyph deviation →
 * batched RSXform draws (one per font/colour bucket, never per glyph), with
 * the pass lanes beside them. The substitution gates it asks are
 * TextSubstitution.h's and the unit box it reports is TextUnitBox.cpp's.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkShader.h>
#include <sigilgeometry/path/Numeric.h>  // radians — the degree conversion
#include <sigilweave/decoration/DecorationRects.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "ComposeRuntime.h"
#include "PaintInternal.h"
#include "TextEngine.h"
#include "TextPose.h"
#include "TextSubstitution.h"

namespace sigil::compose {

using namespace detail;

/** The seed a pass hands its unit in `uUnitPhase[i].y`: distinct per
 *  (outer, inner) beat, in [1, 256), and a pure function of the beat's
 *  numbering — so it is the same seed on every frame and after every
 *  relayout, which is what lets a seeded dissolve settle and cache. */
float passUnitSeed(uint32_t outer, uint32_t inner) {
  core::noise::Mix64Stream rng(((uint64_t)outer << 32u) | (uint64_t)inner);
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
  structure.build(layout, *inst.paragraph, scopeOf(inst));
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
      textStateOf(inst).selectionHeight != inst.measuredForHeight ||
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
    textStateOf(inst).selectionHeight = inst.measuredForHeight;
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
    r.resolved.build(track, structure, *r.selected);
  }
  // A path run with no tracks still draws: every glyph keeps the identity
  // deviation and rests on the curve.
  if (used == 0 && !ridesPath) return;
  const std::span<const Resolved> live(resolved.data(), used);

  // A TRACK DRAWS ITS OWN GLYPHS, in batched buckets, and a bucket carries
  // glyphs alone — so the band a span asked for (an underline, a
  // strikethrough, the sideline beside a column) has to be drawn here, and
  // it is drawn AT REST: from the layout's own runs, untouched by any
  // deviation the tracks apply to the letters above it. That is the same
  // stand a mark() takes under a track — a rect resolved from the layout
  // cannot chase a paint-time pose — and it is the honest one for a band,
  // which spans a whole run rather than following one letter. A run the
  // layout TURNED carries no band either way, so type on a path keeps
  // none.
  //
  // The phases mirror the batched draw a resting node takes: highlights go
  // under every glyph pass, and everything else flushes past them.
  static thread_local std::vector<std::pair<SkRect, SkPaint>> aboveBands;
  aboveBands.clear();
  sigil::weave::detail::forEachDecorationRect(
      layout.runs, inst.paragraph->spans(), override,
      sigil::weave::detail::DecorationPhase::kBelowGlyphs,
      [&](SkRect rect, const SkPaint& bandPaint) {
        canvas.drawRect(rect, bandPaint);
      });
  sigil::weave::detail::forEachDecorationRect(
      layout.runs, inst.paragraph->spans(), override,
      sigil::weave::detail::DecorationPhase::kAboveGlyphs,
      [&](SkRect rect, const SkPaint& bandPaint) {
        aboveBands.emplace_back(rect, bandPaint);
      });

  // PASS TRACKS (fx::pass): each renders its addressed glyphs into its own
  // lane instead of the canvas, accumulating one rect and one local time
  // per (outer, inner) beat — the same enumeration beatsOfTrack reports,
  // from the same TrackCascade, so the pass and the query cannot disagree
  // about the schedule. A glyph a pass addresses draws only inside that
  // pass's layer; a glyph two passes address renders in both.
  using BeatKey = std::pair<uint32_t, uint32_t>;  // (outer, inner)
  struct PassLane {
    const Resolved* source = nullptr;
    sigil::weave::GlyphRSXformBatches batches;
    std::vector<BeatKey> keys;  // one per beat the track runs
    std::vector<SkRect> rects;  // one per beat
    std::vector<float> locals;  // localT per beat
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
          core::noise::Mix64Stream rng(detail::glyphSeed(info));
          detail::compose(mod, r.track->effect(info, t, rng));
          continuous |= r.track->continuous;
        }
        // THE BEAT IS NOTED BEFORE THE INK IS. A glyph the deviation faded
        // or shrank away draws nothing, but it is still a member of its
        // beat, and a pass's uniform arrays are numbered against the beats
        // the query reports: a lane that skipped it would renumber every
        // beat after it and hand the material a different unit's rect and
        // phase from the one the author asked about.
        const auto noteBeat = [&](PassLane& lane) {
          const detail::TrackCascade& rc = lane.source->resolved;
          const BeatKey key{rc.outerUnit[g],
                            rc.innerUnit.empty() ? 0u : rc.innerUnit[g]};
          const SkRect box =
              glyphBox(placed, pose, bandOf(placed.shaped, bandMemo));
          if (const size_t at = indexOfKey(lane.keys, key);
              at < lane.keys.size()) {
            lane.rects[at].join(box);
            return;
          }
          lane.keys.push_back(key);
          lane.rects.push_back(box);
          lane.locals.push_back(
              rc.cascade.localTime(lane.source->master, key.first, key.second));
        };
        const auto noteBeatsAndDrop = [&] {
          for (const std::unique_ptr<PassLane>& lane : passes)
            if ((*lane->source->selected)[g]) noteBeat(*lane);
        };
        if (mod.alpha <= 0.003f || mod.scale <= 0.001f) {
          noteBeatsAndDrop();
          return;
        }
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
        if (alpha <= 0.0f) {
          noteBeatsAndDrop();
          return;
        }
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
          const float radians = geometry::path::radians(mod.rotateDeg);
          if (continuous) {
            cosv = std::cos(radians);
            sinv = std::sin(radians);
          } else {
            // The SAME size-cut ladder the baseline's tangent takes, and
            // for the same reason: a track's rotation composes with that
            // tangent onto one glyph, so a coarser ladder here would be the
            // coarsest thing in the letter's motion and would tick where the
            // baseline does not. Track::continuous is still the opt-out
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
            matrix.preSkew(std::tan(geometry::path::radians(mod.skewXDeg)),
                           std::tan(geometry::path::radians(mod.skewYDeg)));
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
          noteBeat(*lane);
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
    material::skia::detail::PassInputs inputs;
    const SkMatrix toTile = SkMatrix::Translate(-reach, -reach);
    inputs.content = layer->makeShader(SkTileMode::kDecal, SkTileMode::kDecal,
                                       SkFilterMode::kLinear, &toTile, &tile);
    inputs.rects = rects.data();
    inputs.phases = phases.data();
    inputs.units = n;
    const material::skia::Paint* passPaint =
        lane->source->track->effect.passMaterial();
    sk_sp<SkShader> pass = passPaint->resolvePass(inputs, frameOf(ctx));
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

  // The bands that sit above the type, flushed past every glyph the node
  // drew — the batched buckets and the pass lanes alike, so a strikethrough
  // reads over a shaded pass exactly as it reads over plain letters. The
  // paints are dropped straight after: one can hold a shader, and this
  // storage outlives the frame.
  for (const auto& [rect, bandPaint] : aboveBands)
    canvas.drawRect(rect, bandPaint);
  aboveBands.clear();
}

}  // namespace sigil::compose
