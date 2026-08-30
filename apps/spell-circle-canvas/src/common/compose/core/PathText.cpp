/** @file
 * Text on a path: which fields of a TextPath decide the layout, the run
 * broken across the baseline's contours, the tangent ladder, and the rest
 * pose of one glyph — level on a straight baseline, on the curve, or down
 * a column.
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

/** THE FIELDS OF A TextPath THAT DECIDE THE LAYOUT, as opposed to the
 *  placement of glyphs the layout already made. How the baseline is shaped,
 *  which way the run reads and where along it the run RESTS decide which
 *  words land on which contour; the perpendicular offset, the orientation
 *  and the tangent snapping are read per glyph at paint and move nothing
 *  the breaker decided.
 *
 *  A BOUND phase rests at zero and is applied at paint, which is what makes
 *  a marquee a repaint rather than a reflow. */
bool samePathLayout(const TextPath& a, const TextPath& b) {
  const float restA = a.at.plain() ? *a.at.plain() : 0.0f;
  const float restB = b.at.plain() ? *b.at.plain() : 0.0f;
  return a.path == b.path && a.align == b.align && a.autoFlip == b.autoFlip &&
         restA == restB;
}

/** Directions a path tangent snaps to at LAYOUT time — the tangents baked
 *  into the path layout's rest poses, which nothing on the paint path
 *  reads (the painter re-derives every pose exactly and snaps with the
 *  size-cut ladder below). `TextPath::exactTangent` is the opt-out. */
constexpr int kPathTangentSteps = 64;

/** How many directions the rotation ladder offers a glyph rendered at
 *  `pixelSize`.
 *
 *  A turning glyph's rotation is snapped so it lands on a BOUNDED set of
 *  directions: every distinct rotation is both a batch bucket and a
 *  glyph-atlas strike, and lifting the ladder entirely mints a fresh strike
 *  per letter per frame for several times the price of any ladder measured
 *  here. How FINE the ladder must be is a visual question, and it has an
 *  exact answer.
 *
 *  One step turns the glyph by 2π/N, which sweeps a point `r` from the
 *  rotation centre through r·2π/N pixels. Take `r` as the glyph's own
 *  half-em — the far edge of its ink — and cut the ladder at sixteen steps
 *  per pixel of em, and that sweep is (px/2)·2π/(16·px) = π/16 ≈ 0.20 px AT
 *  EVERY SIZE. The number to stay under is a QUARTER of a pixel, because
 *  that is the phase grid a moving run's origins sit on: a ladder whose
 *  step sweeps further than the grid is the coarsest thing left in the
 *  motion and ticks letter by letter as each glyph crosses a step at its
 *  own moment, while one that sweeps less disappears underneath it.
 *
 *  Both ends are clamped. The floor keeps small rings on the ladder they
 *  have always had. The ceiling is what bounds the strike population at
 *  all — the ladder's whole reason to exist — and it binds from 128 px of
 *  em upward, where a step's sweep begins to pass the grid again;
 *  `TextPath::exactTangent` is the escape for artwork set that large. */
int tangentLadderSteps(float pixelSize) {
  constexpr float kStepsPerPixel = 16.0f;
  constexpr int kMinSteps = 64, kMaxSteps = 2048;
  return std::clamp((int)std::lround(pixelSize * kStepsPerPixel), kMinSteps,
                    kMaxSteps);
}

/** THE RUN BROKEN ACROSS ITS BASELINE'S CONTOURS.
 *
 *  Shaped once — real kerning, real ligatures, real advances — and then
 *  laid out through SigilWeave's own contour-interval geometry: EVERY
 *  contour becomes one interval of one line, so which words land on which
 *  contour, how they fill it, and where each pen sits are the paragraph
 *  engine's answers rather than a second implementation of them. A word
 *  that does not fit the contour it reached starts the next one, and a run
 *  that outlasts the last contour simply stops.
 *
 *  Cached against everything that decides it — the content, the box the
 *  baseline resolves against, and the baseline value. The `at` phase is not
 *  among them: it re-places glyphs the layout already placed, at paint. */
void Composer::Impl::ensurePathLayout(Instance& inst, const TextPath& spec,
                                      SkSize size) {
  if (inst.pathValid && inst.pathRev == inst.contentRev &&
      inst.pathSize.width() == size.width() &&
      inst.pathSize.height() == size.height() && inst.pathSpec &&
      samePathLayout(*inst.pathSpec, spec))
    return;

  if (!inst.paragraph) return;  // no content materialized: nothing to place
  inst.pathValid = false;
  inst.pathIntervals.clear();
  inst.pathLayout = {};
  inst.pathRev = inst.contentRev;
  inst.pathSize = size;
  inst.pathSpec = spec;
  inst.pathTotalLength = 0;
  if (!spec.path) return;

  const SkPath baseline = spec.path(size);
  // The centre Orient::Radial radiates from: the bounds of the resolved
  // baseline, which for every dial-shaped path is its centre.
  const SkRect baselineBounds = baseline.getBounds();
  inst.pathCentroid = {baselineBounds.centerX(), baselineBounds.centerY()};

  // The run's own width, from the shaped advances. This is what Align
  // measures against, and it is why the run has to be shaped first — it
  // comes from the straight MEASURE layout, which is also the node's box.
  float runWidth = 0;
  sigil::weave::forEachPlacedGlyph(
      inst.textLayout, *inst.paragraph,
      [&](const sigil::weave::PlacedGlyph& placed) {
        runWidth = std::max(runWidth, placed.rest.x() + placed.advance);
      });

  static thread_local std::vector<geometry::Contour> contours;
  contours = geometry::Contour::of(baseline);
  if (contours.empty()) return;
  float length = 0;
  for (const geometry::Contour& contour : contours) length += contour.length();
  inst.pathTotalLength = length;

  // One arc-length coordinate over the whole chain, for the two questions
  // that are about the BASELINE rather than about one glyph: is it closed,
  // and which way does the run read along it.
  const auto posTan = [](float distance, SkPoint* position, SkVector* tangent) {
    const auto read = [&](const geometry::Contour& contour, float d) {
      const auto sample = contour.at(d);
      if (!sample) return false;
      if (position) *position = geometry::toSk(sample->position);
      if (tangent) *tangent = geometry::toSk(sample->tangent);
      return true;
    };
    for (const geometry::Contour& contour : contours) {
      if (distance <= contour.length()) return read(contour, distance);
      distance -= contour.length();
    }
    const geometry::Contour& last = contours.back();
    return read(last, last.length());
  };

  // "Closed" here means geometrically closed, not flagged closed:
  // shapes::arc() defaults to a 359.9-degree sweep and is the library's own
  // spelling for a ring, but addArc leaves it open. Dropping half a centred
  // caption off a ring because of a tenth of a degree is not a behaviour
  // anyone wants.
  bool closed = contours.size() == 1 && contours.front().closed();
  if (!closed) {
    SkPoint head, tail;
    SkVector ignored;
    if (posTan(0, &head, &ignored) && posTan(length, &tail, &ignored))
      closed = SkPoint::Distance(head, tail) <= std::max(1.0f, length * 0.002f);
  }

  const float restAt = spec.at.plain() ? *spec.at.plain() : 0.0f;
  inst.pathRestAt = restAt;
  float start = restAt * length;
  if (spec.align == TextPath::Align::Center)
    start -= runWidth * 0.5f;
  else if (spec.align == TextPath::Align::End)
    start -= runWidth;

  // autoFlip is a decision about the RUN, not about each glyph. Turning
  // glyphs over one at a time reverses the reading order — a caption on the
  // lower half of a clockwise ring would come out mirrored — so the run
  // decides once and then reads along the reversed baseline.
  //
  // The decision is a MAJORITY over the run, not a reading at its midpoint.
  // A midpoint sample is exactly ambiguous where the tangent is vertical,
  // which is precisely where a ring caption centred at the top or bottom of
  // a circle puts it: `tan.x < 0` is false at x == 0, so the most natural
  // spelling of all — a circle, at = 0, centred, autoFlip — would silently
  // do nothing. Sampling across the run has no such point.
  //
  // A run that wraps PAST the crossover cannot be fixed by one flip, and
  // this model does not pretend otherwise: the majority reads right way up
  // and the tail does not. Setting the top and bottom halves as two
  // separate runs is the way around it.
  bool flipRun = false;
  if (spec.autoFlip) {
    constexpr int kVotes = 9;
    int upsideDown = 0, upright = 0;
    for (int vote = 0; vote < kVotes; ++vote) {
      float at = start + runWidth * ((float)vote + 0.5f) / (float)kVotes;
      if (closed) at = std::fmod(std::fmod(at, length) + length, length);
      SkPoint position;
      SkVector tangent;
      if (!posTan(std::clamp(at, 0.0f, length), &position, &tangent)) continue;
      if (tangent.x() < 0)
        ++upsideDown;
      else if (tangent.x() > 0)
        ++upright;
    }
    flipRun = upsideDown > upright;
  }

  // A flipped run enters at the END of its stretch of baseline and walks
  // BACKWARDS along it: its pen still travels the letters in reading order,
  // but its arc position decreases and every tangent faces about. One
  // decision for the whole run, expressed as the interval's own direction
  // of travel — turning the letters over one at a time is what would
  // reverse the reading order.
  //
  // ONE LINE, one interval per contour from the ENTRY POINT onwards. `at`
  // is a fraction of the WHOLE baseline, contours chained end to end, which
  // is what lets seven chords of a heptagon carry seven captions addressed
  // by fraction alone. So the entry point picks the contour it falls in,
  // enters it partway, and every contour after that is offered whole.
  //
  // What a contour boundary IS, on the other hand, is a break: a word that
  // does not fit the contour it reached starts the next one rather than
  // bending across the gap between two disconnected curves.
  const float entry = flipRun ? start + runWidth : start;
  size_t entryContour = 0;
  float entryLocal = entry;
  while (entryContour + 1 < contours.size() &&
         entryLocal > contours[entryContour].length()) {
    entryLocal -= contours[entryContour].length();
    ++entryContour;
  }
  inst.pathIntervals.reserve(contours.size());
  const auto pushInterval = [&](size_t index, float localStart) {
    sigil::weave::LineInterval interval;
    interval.contour = contours[index];
    interval.contourStart = localStart;
    interval.advanceScale = flipRun ? -1.0f : 1.0f;
    interval.wrapContour = closed;
    // How much baseline this contour still offers from where the pen
    // entered it — the whole of it when the pen wraps, because a loop has
    // no end to run out of.
    const float contourLength = contours[index].length();
    interval.length =
        closed
            ? contourLength
            : std::max(flipRun ? localStart : contourLength - localStart, 0.0f);
    inst.pathIntervals.push_back(std::move(interval));
  };
  if (flipRun) {
    pushInterval(entryContour, entryLocal);
    for (size_t index = entryContour; index-- > 0;)
      pushInterval(index, contours[index].length());
  } else {
    pushInterval(entryContour, entryLocal);
    for (size_t index = entryContour + 1; index < contours.size(); ++index)
      pushInterval(index, 0.0f);
  }

  sigil::weave::LineSetFlow flow({inst.pathIntervals});
  sigil::weave::ParagraphLayoutOptions options = textLayoutOptions(inst);
  // The baseline places the run; an interval-relative alignment on top of
  // that would fight `at` and Align for the same authority.
  options.alignment = sigil::weave::TextAlignment::kStart;
  options.pathText.tangentRotationSteps =
      spec.exactTangent ? 0 : kPathTangentSteps;
  inst.pathLayout =
      sigil::weave::layoutParagraph(fonts, *inst.paragraph, flow, options);
  inst.pathValid = true;
}

// ---------------------------------------------------------------------------
// THE REST POSE of one glyph: where the baseline put its advance centre, and
// which way it faces there. Plain text rests level on its own straight
// baseline; a path run rests on the curve; a vertical column's glyph rests on
// the column axis. Returning false DROPS the glyph — a run walking off the end
// of an open baseline should look like it, rather than piling every remaining
// letter on the last point.
//
// ONE BODY for the painter and for the beatsOf query, because a mark placed
// beside a cascade must land where the letters land: on the curve where they
// ride a curve, down the column where they stand in a column.

bool restPoseOf(const PoseContext& ctx, const sigil::weave::PlacedGlyph& placed,
                RestPose& pose) {
  const sigil::weave::ParagraphLayout& layout = *ctx.layout;
  if (!ctx.ridesPath) {
    pose.cosine = 1.0f;
    pose.sine = 0.0f;
    pose.centreOffset.reset();
    // A ROTATED run — Latin lying on its side in a CJK column — is placed
    // per glyph off its interval exactly as a path run is, and deviates
    // in the frame the interval turned it to.
    if (placed.transformed) {
      const sigil::weave::LineInterval* interval =
          placed.intervalIndex >= 0 &&
                  (size_t)placed.intervalIndex < layout.intervals.size()
              ? &layout.intervals[(size_t)placed.intervalIndex]
              : nullptr;
      if (!interval) return false;
      SkVector tangent;
      if (!interval->placeAt(placed.pen, 0.0f, layout.tangentRotationSteps,
                             &pose.centre, &tangent))
        return false;
      const float magnitude = std::hypot(tangent.x(), tangent.y());
      if (magnitude <= 1e-6f) return false;
      pose.cosine = tangent.x() / magnitude;
      pose.sine = tangent.y() / magnitude;
      return true;
    }
    // An UPRIGHT run stands level in a column that runs down the page: its
    // advance is vertical while the glyph is still drawn from a horizontal
    // origin, so the pose centre is the point on the COLUMN AXIS the pen
    // reached, and the back-out to the draw origin is whatever vector
    // separates the two.
    if (placed.shaped && placed.shaped->vertical) {
      const sigil::weave::LineInterval* interval =
          placed.intervalIndex >= 0 &&
                  (size_t)placed.intervalIndex < layout.intervals.size()
              ? &layout.intervals[(size_t)placed.intervalIndex]
              : nullptr;
      if (interval) {
        SkVector tangent;
        if (interval->placeAt(placed.pen, 0.0f, 0, &pose.centre, &tangent)) {
          pose.centreOffset = SkVector{pose.centre.x() - placed.rest.x(),
                                       pose.centre.y() - placed.rest.y()};
          return true;
        }
      }
    }
    // Horizontal flow, and 縦中横 — a horizontally shaped run set upright
    // across the column, whose advance runs across the page like any
    // other horizontal run's.
    pose.centre = {placed.rest.x() + placed.advance * 0.5f, placed.rest.y()};
    return true;
  }
  if (placed.intervalIndex < 0 ||
      (size_t)placed.intervalIndex >= ctx.inst->pathIntervals.size())
    return false;
  const sigil::weave::LineInterval& interval =
      ctx.inst->pathIntervals[(size_t)placed.intervalIndex];
  SkPoint position;
  SkVector tangent;
  // EXACT, not snapped: the snapping is a rasterization concession and
  // belongs to the rotation alone. `offset` rides the type off the
  // baseline along the perpendicular, so a tangent rounded onto a ladder
  // step would slide it along the curve by however far the rounding was.
  if (!interval.placeAt(placed.pen, ctx.phaseArc, 0, &position, &tangent))
    return false;
  const float magnitude = std::hypot(tangent.x(), tangent.y());
  if (magnitude <= 1e-6f) return false;
  float dirX = tangent.x() / magnitude, dirY = tangent.y() / magnitude;
  // Perpendicular offset, positive to the LEFT of travel (outward on a
  // clockwise circle). The path replaces the glyph's own baseline.
  // Measured along TRAVEL even under Radial orientation, so `offset`
  // keeps meaning "how far off the baseline the type rides" regardless of
  // which way the glyph ends up facing.
  position.offset(dirY * ctx.onPath->offset, -dirX * ctx.onPath->offset);
  // Radial: the glyph's BASELINE runs along the radius, so the run reads
  // outward from the centre like a spoke. That is how an astrolabe limb,
  // a compass rose and a radial axis label their divisions — you turn the
  // instrument to read them.
  //
  // Note this is genuinely a different thing from what Tangent already
  // does. On a circle, "up points outward" IS the tangent orientation (a
  // clock face's 6 is upside down for exactly that reason), so the only
  // orientation a path baseline was missing is the one where the type
  // radiates.
  if (ctx.onPath->orient == TextPath::Orient::Upright) {
    dirX = 1.0f;
    dirY = 0.0f;
  } else if (ctx.onPath->orient == TextPath::Orient::Radial) {
    const float ox = position.x() - ctx.inst->pathCentroid.x();
    const float oy = position.y() - ctx.inst->pathCentroid.y();
    const float radius = std::hypot(ox, oy);
    if (radius <= 1e-6f) return false;
    dirX = ox / radius;
    dirY = oy / radius;
  }
  // The ROTATION snaps, and only the rotation: a continuous per-glyph
  // angle mints a fresh glyph mask per letter in Skia's cache. The ladder
  // is cut by RENDERED SIZE (tangentLadderSteps): a fixed angular step
  // sweeps a bigger glyph's extremity through more pixels, so display
  // lettering on a turning ring gets a proportionally finer ladder and
  // does not tick letter by letter as each glyph crosses a step at its
  // own moment.
  pose.centre = position;
  pose.cosine = dirX;
  pose.sine = dirY;
  if (!ctx.onPath->exactTangent)
    sigil::weave::quantizeAngle(
        std::atan2(dirY, dirX),
        tangentLadderSteps(placed.shaped ? placed.shaped->fontSize : 0.0f),
        pose.cosine, pose.sine);
  return true;
}

}  // namespace sigil::compose
