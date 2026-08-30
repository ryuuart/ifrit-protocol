/** @file
 * The schedule read back: where each mark's selector landed, and the beats
 * and span of one track's cascade, resolved from the same pieces the painter
 * uses.
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

#include "ComposeRuntime.h"
#include "PaintInternal.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// THE SCHEDULE, READ BACK
//
// Resolved out of the same three pieces the painter uses and in the same
// order — the glyph structure, the track's selection, and TrackCascade — so
// a mark placed from this is on the beat the glyphs are on by construction
// rather than by an author keeping two sets of numbers in step.
//
// Computed on demand rather than recorded during paint. A settled text node
// stops painting and replays a picture, and a query that read a paint-time
// buffer would answer with whatever the last live frame left behind; a query
// that resolves against the current layout and the current progress answers
// for the frame being asked about.

namespace {

/** Once per (mark key) whose selector found no glyphs. */
void warnMarkSelectsNothing(const std::string& key) {
  static std::set<std::string> warned;
  if (!warned.insert(key).second) return;
  SkDebugf(
      "[compose] mark(\"%s\") selects no glyphs in this text, so it places "
      "nothing and draws nothing. A selector naming a word, line or sentence "
      "the passage does not have, a style name no run was written with, and "
      "a pattern that does not compile all resolve to nothing.\n",
      key.c_str());
}

}  // namespace

void Composer::Impl::resolveTextMarks(Instance& inst) {
  inst.textMarkRects.clear();
  if (!inst.desc || !inst.desc->textData || !inst.paragraph) return;
  const detail::TextData& textData = *inst.desc->textData;
  const std::vector<detail::MarkAnchor>& marks = textData.marks;
  if (marks.empty()) return;
  // A PATH-laid run's marks stand on the curve the letters stand on: the
  // baseline resolves against the node's box (so the caller must not ask
  // before layout has settled that box), and the pose below then places
  // each advance box exactly where the paint places its glyph. A mark is a
  // LAYOUT answer, so it stands where the run RESTS on the curve — a run
  // driven along its baseline (`at` bound) is a paint-time deviation, and
  // a layout rect that chased it would be stale by the next frame.
  const TextPath* onPath = textData.onPath ? &*textData.onPath : nullptr;
  if (onPath) {
    const SkRect rect = instanceRect(inst);
    ensurePathLayout(inst, *onPath, {rect.width(), rect.height()});
    if (!inst.pathValid) return;  // no measurable baseline: nothing lands
  }

  // The layout the letters are drawn from — the path one where the run
  // rides a curve, otherwise the flow one this node measures by, the same
  // placement `paragraphLayout()` reports.
  const sigil::weave::ParagraphLayout& layout =
      onPath ? inst.pathLayout : inst.textLayout;
  static thread_local detail::GlyphStructure structure;
  structure.build(layout, *inst.paragraph);
  if (structure.glyphs.empty()) return;
  const auto count = (uint32_t)structure.glyphs.size();

  const PoseContext poseCtx{&inst, &layout, onPath, onPath != nullptr, 0.0f};
  std::vector<std::pair<BandKey, GlyphBand>> bandMemo;
  for (const detail::MarkAnchor& anchor : marks) {
    const std::vector<uint8_t> selected = detail::resolveSelection(
        anchor.where, structure, *inst.paragraph, inst.textNamedRuns);
    // ONE RECT FOR THE WHOLE SELECTION: the union of every advance box it
    // addressed, built from the same pose and the same box the schedule
    // read-back builds a beat's rect from, so a mark and a beat cannot
    // disagree about where a unit is.
    SkRect box = SkRect::MakeEmpty();
    bool any = false;
    uint32_t ordinal = 0;
    sigil::weave::forEachPlacedGlyph(
        layout, *inst.paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
          const uint32_t g = ordinal++;
          if (g >= count || !selected[g]) return;
          RestPose pose;
          if (!restPoseOf(poseCtx, placed, pose)) return;
          const SkRect glyph =
              glyphBox(placed, pose, bandOf(placed.shaped, bandMemo));
          if (any) {
            box.join(glyph);
          } else {
            box = glyph;
            any = true;
          }
        });
    if (!any) {
      warnMarkSelectsNothing(anchor.key);
      continue;
    }
    inst.textMarkRects.emplace_back(anchor.key, box);
  }
}

namespace {
/** ONE TRACK'S SCHEDULE RESOLVED FOR A QUERY — the front half `beatsOfTrack`
 *  and `cascadeSpanOfTrack` share: which layout the letters are on, which
 *  glyphs the track addresses, and the cascade those glyphs run. ONE BODY,
 *  so the span, the beats and the glyphs cannot disagree about the
 *  schedule. False means the track runs nothing a query can report: no
 *  text, no such track, no effect, or a path baseline that has not
 *  resolved. */
struct TrackSchedule {
  const Track* track = nullptr;
  const TextPath* onPath = nullptr;
  bool ridesPath = false;
  const sigil::weave::ParagraphLayout* layout = nullptr;
  uint32_t glyphCount = 0;
  std::vector<uint8_t> selected;
  detail::TrackCascade resolved;
};

bool resolveTrackSchedule(Instance& inst, size_t trackIndex,
                          TrackSchedule& out) {
  if (!inst.desc || !inst.paragraph) return false;
  const std::span<const Track> tracks = tracksOf(*inst.desc);
  if (trackIndex >= tracks.size()) return false;
  out.track = &tracks[trackIndex];
  if (!out.track->effect) return false;

  // The layout the last draw() left standing — the path one where the run
  // rides a curve, so the beats are on the curve the letters are on.
  if (inst.desc->textData && inst.desc->textData->onPath.has_value())
    out.onPath = &inst.desc->textData->onPath.value();
  out.ridesPath = out.onPath && inst.pathValid;
  if (out.onPath && !out.ridesPath) return false;
  out.layout = out.ridesPath ? &inst.pathLayout : &inst.textLayout;

  static thread_local detail::GlyphStructure structure;
  structure.build(*out.layout, *inst.paragraph);
  out.glyphCount = (uint32_t)structure.glyphs.size();
  if (out.glyphCount == 0) return false;

  out.selected = detail::resolveSelection(out.track->where, structure,
                                          *inst.paragraph, inst.textNamedRuns);
  out.resolved.build(out.track->stagger, structure, out.selected);
  return true;
}
}  // namespace

float Composer::Impl::cascadeSpanOfTrack(Instance& inst, size_t trackIndex) {
  TrackSchedule schedule;
  if (!resolveTrackSchedule(inst, trackIndex, schedule)) return 0.0f;
  return schedule.resolved.cascade.totalMs;
}

std::vector<Beat> Composer::Impl::beatsOfTrack(Instance& inst,
                                               size_t trackIndex) {
  TrackSchedule schedule;
  if (!resolveTrackSchedule(inst, trackIndex, schedule)) return {};
  const Track& track = *schedule.track;
  const auto count = schedule.glyphCount;
  const std::vector<uint8_t>& selected = schedule.selected;
  const detail::TrackCascade& resolved = schedule.resolved;
  const sigil::weave::ParagraphLayout& layout = *schedule.layout;

  const AnimatedFloat* anim = trackIndex < inst.trackAnims.size()
                                  ? inst.trackAnims[trackIndex].get()
                                  : nullptr;
  const float master =
      std::clamp(inst.resolveFloatAt(anim, track.progress), 0.0f, 1.0f);

  float phaseArc = 0;
  if (schedule.ridesPath)
    phaseArc = (inst.resolvePathAt() - inst.pathRestAt) * inst.pathTotalLength;
  const PoseContext poseCtx{&inst, &layout, schedule.onPath, schedule.ridesPath,
                            phaseArc};

  // ONE BEAT PER (outer, inner) PAIR THE TRACK ACTUALLY RUNS, in draw order,
  // and the rect is the union of the glyphs THIS track addresses in it: a
  // partitioning track reports where its own half of each unit sits, which
  // is the half its effect moves.
  std::vector<Beat> beats;
  std::vector<std::pair<uint32_t, uint32_t>> keys;
  std::vector<std::pair<BandKey, GlyphBand>> bandMemo;
  uint32_t ordinal = 0;
  sigil::weave::forEachPlacedGlyph(
      layout, *inst.paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
        const uint32_t g = ordinal++;
        if (g >= count || !selected[g]) return;
        RestPose pose;
        if (!restPoseOf(poseCtx, placed, pose)) return;
        const uint32_t outer = resolved.outerUnit[g];
        const uint32_t inner =
            resolved.innerUnit.empty() ? 0u : resolved.innerUnit[g];
        const SkRect box =
            glyphBox(placed, pose, bandOf(placed.shaped, bandMemo));
        for (size_t i = keys.size(); i-- > 0;)
          if (keys[i].first == outer && keys[i].second == inner) {
            beats[i].rect.join(box);
            return;
          }
        Beat beat;
        beat.rect = box;
        beat.unitIndex = outer;
        beat.startMs = resolved.cascade.startMs(outer, inner);
        beat.localT = resolved.cascade.localTime(master, outer, inner);
        // A beat that has begun and not finished. The clamped local time
        // reads 0 both before the beat opens and exactly as it does, and 1
        // for the whole of the rest of the track's life.
        beat.active = beat.localT > 0.0f && beat.localT < 1.0f;
        keys.emplace_back(outer, inner);
        beats.push_back(beat);
      });
  return beats;
}

}  // namespace sigil::compose
