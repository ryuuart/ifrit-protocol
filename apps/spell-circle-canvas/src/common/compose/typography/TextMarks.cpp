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
#include <boost/unordered/unordered_flat_set.hpp>
#include <chrono>
#include <cmath>
#include <tuple>
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
  static boost::unordered_flat_set<std::string> warned;
  if (!warned.insert(key).second) return;
  SkDebugf(
      "[compose] mark(\"%s\") selects no glyphs in this text, so it places "
      "nothing and draws nothing. A selector naming a word, line or sentence "
      "the passage does not have, a style name no run was written with, and "
      "a pattern that does not compile all resolve to nothing.\n",
      key.c_str());
}

}  // namespace

void detail::resolveTextMarks(Composer::Impl& impl, Instance& inst) {
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
    const SkRect rect = impl.instanceRect(inst);
    ensurePathLayout(impl, inst, *onPath, {rect.width(), rect.height()});
    if (!textStateOf(inst).pathValid)
      return;  // no measurable baseline: nothing lands
  }

  // The layout the letters are drawn from — the path one where the run
  // rides a curve, otherwise the flow one this node measures by, the same
  // placement `paragraphLayout()` reports.
  const sigil::weave::ParagraphLayout& layout =
      onPath ? textStateOf(inst).pathLayout : inst.textLayout;
  static thread_local detail::GlyphStructure structure;
  structure.build(layout, *inst.paragraph, scopeOf(inst));
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

std::vector<TextUnit> detail::unitsOfText(Composer::Impl& impl, Instance& inst,
                                          const Selector& selector, Unit unit) {
  if (!inst.desc || !inst.paragraph) return {};
  const sigil::weave::Paragraph& paragraph = *inst.paragraph;
  const TextData* textData =
      inst.desc->textData ? &*inst.desc->textData : nullptr;
  const TextPath* onPath =
      textData && textData->onPath ? &*textData->onPath : nullptr;
  if (onPath) {
    const SkRect rect = impl.instanceRect(inst);
    ensurePathLayout(impl, inst, *onPath, {rect.width(), rect.height()});
    if (!textStateOf(inst).pathValid) return {};
  }
  const sigil::weave::ParagraphLayout& layout =
      onPath ? textStateOf(inst).pathLayout : inst.textLayout;

  static thread_local detail::GlyphStructure structure;
  structure.build(layout, paragraph, scopeOf(inst));
  if (structure.glyphs.empty()) return {};
  const auto count = (uint32_t)structure.glyphs.size();
  const std::vector<uint8_t> selected = detail::resolveSelection(
      selector, structure, paragraph, inst.textNamedRuns);
  const std::vector<uint32_t>& unitOf = structure.unitOf[(size_t)unit];
  const bool vertical =
      paragraph.writingMode() == sigil::weave::WritingMode::kVerticalRL;

  const PoseContext poseCtx{&inst, &layout, onPath, onPath != nullptr, 0.0f};
  std::vector<std::pair<BandKey, GlyphBand>> bandMemo;
  std::vector<TextUnit> units;
  // The source unit AND the line it landed on. A base that broke across a
  // line or a column is two entries, on the two lines — which is what lets
  // a reading split with its base, and is a truer answer than one rect
  // spanning a break could ever be.
  std::vector<std::pair<uint32_t, int>> keys;
  uint32_t ordinal = 0;
  sigil::weave::forEachPlacedGlyph(
      layout, paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
        const uint32_t g = ordinal++;
        if (g >= count || !selected[g]) return;
        RestPose pose;
        if (!restPoseOf(poseCtx, placed, pose)) return;
        const GlyphBand band = bandOf(placed.shaped, bandMemo);
        const SkRect box = glyphBox(placed, pose, band);
        const uint32_t source = g < unitOf.size() ? unitOf[g] : 0;
        for (size_t i = keys.size(); i-- > 0;)
          if (keys[i].first == source && keys[i].second == placed.lineIndex) {
            TextUnit& existing = units[i];
            existing.rect.join(box);
            existing.range.start =
                std::min(existing.range.start, placed.textIndex);
            existing.range.end =
                std::max(existing.range.end, placed.textIndex + 1);
            return;
          }
        TextUnit entry;
        entry.rect = box;
        entry.index = (uint32_t)units.size();
        // A COLUMN HAS NO BASELINE: its glyphs centre themselves across the
        // column's axis, so that axis is what the annotation beside them
        // reads. A line reports the baseline they stand on.
        entry.axis = vertical ? pose.centre.x() : placed.rest.y();
        entry.pitch = layout.linePitch;
        entry.ascent = band.ascent;
        entry.descent = band.descent;
        entry.writingMode = paragraph.writingMode();
        // The form is what the placement did with the glyph: a run shaped
        // top-to-bottom stands upright, one whose placement was baked per
        // glyph is turned with the column, and a horizontal run standing in
        // a column is set across it.
        if (!vertical)
          entry.verticalForm = sigil::weave::VerticalForm::kAuto;
        else if (placed.transformed)
          entry.verticalForm = sigil::weave::VerticalForm::kRotated;
        else if (placed.shaped && placed.shaped->vertical)
          entry.verticalForm = sigil::weave::VerticalForm::kUpright;
        else
          entry.verticalForm = sigil::weave::VerticalForm::kTateChuYoko;
        entry.range = {placed.textIndex, placed.textIndex + 1};
        for (const sigil::weave::StyleSpan& span : paragraph.spans())
          if (placed.textIndex < span.end) {
            entry.style = span.style;
            break;
          }
        entry.lineIndex = placed.lineIndex;
        keys.emplace_back(source, placed.lineIndex);
        units.push_back(std::move(entry));
      });
  return units;
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

bool resolveTrackSchedule(Composer::Impl& impl, Instance& inst,
                          size_t trackIndex, TrackSchedule& out) {
  if (!inst.desc || !inst.paragraph) return false;
  const std::span<const Track> tracks = tracksOf(*inst.desc);
  if (trackIndex >= tracks.size()) return false;
  out.track = &tracks[trackIndex];
  if (!out.track->effect) return false;

  // The layout the last draw() left standing — the path one where the run
  // rides a curve, so the beats are on the curve the letters are on.
  if (inst.desc->textData) {
    const std::optional<TextPath>& path = inst.desc->textData->onPath;
    if (path.has_value()) out.onPath = &path.value();
  }
  out.ridesPath = out.onPath && textStateOf(inst).pathValid;
  if (out.onPath && !out.ridesPath) return false;
  out.layout = out.ridesPath ? &textStateOf(inst).pathLayout : &inst.textLayout;

  static thread_local detail::GlyphStructure structure;
  structure.build(*out.layout, *inst.paragraph, scopeOf(inst));
  out.glyphCount = (uint32_t)structure.glyphs.size();
  if (out.glyphCount == 0) return false;

  out.selected = detail::resolveSelection(out.track->where, structure,
                                          *inst.paragraph, inst.textNamedRuns);
  out.resolved.build(*out.track, structure, out.selected);
  return true;
}
}  // namespace

float detail::cascadeSpanOfTrack(Composer::Impl& impl, Instance& inst,
                                 size_t trackIndex) {
  TrackSchedule schedule;
  if (!resolveTrackSchedule(impl, inst, trackIndex, schedule)) return 0.0f;
  return schedule.resolved.cascade.totalMs;
}

std::vector<Beat> detail::beatsOfTrack(Composer::Impl& impl, Instance& inst,
                                       size_t trackIndex) {
  TrackSchedule schedule;
  if (!resolveTrackSchedule(impl, inst, trackIndex, schedule)) return {};
  if (!inst.paragraph.has_value()) return {};
  const sigil::weave::Paragraph& paragraph = inst.paragraph.value();
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
    phaseArc = (inst.resolvePathAt() - textStateOf(inst).pathRestAt) *
               textStateOf(inst).pathTotalLength;
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
      layout, paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
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
        // The schedule half is the cascade's own answer, so a mark
        // travelling beside a track cannot be told a different one from
        // the glyphs it is marking; the rect is this library's.
        Beat beat{resolved.cascade.beat(master, outer, inner)};
        beat.rect = box;
        keys.emplace_back(outer, inner);
        beats.push_back(beat);
      });
  return beats;
}

}  // namespace sigil::compose
