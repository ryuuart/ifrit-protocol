/** @file
 * Recording bounds: the rect a node's own paint covers, the motion path a
 * travelling node follows, and the subtree union a recording is culled by.
 */

#include <include/core/SkCanvas.h>
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
#include "sigilgeometry/path/Pose.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Recording bounds

/** The rect this node's OWN paint covers, in its own local space — children
 *  excluded; recordBounds() below adds the child union. The node's box,
 *  grown by every declared bleed (decorations, stroke passes, echo offsets,
 *  band width profiles, material reserves), then joined with the geometry a
 *  layout rect does not bound at all: a routed connector/rail path, a text
 *  run's path baseline, and a borrowed band spine, each outset by its own
 *  reach. */
SkRect Composer::Impl::ownPaintBounds(Instance& inst) {
  const ElementNode& node = *inst.desc;
  const SkRect rect = instanceRect(inst);
  SkRect local = SkRect::MakeWH(rect.width(), rect.height());
  float bleed = 0;
  for (const Decoration& d : node.backgrounds)
    bleed = std::max(bleed, d.bleed());
  for (const Decoration& d : node.foregrounds)
    bleed = std::max(bleed, d.bleed());
  if (node.fxData)
    for (const Decoration& d : node.fxData->overlays)
      bleed = std::max(bleed, d.bleed());
  if (node.strokeData)
    for (const detail::StrokePass& pass : node.strokeData->passes)
      bleed = std::max(bleed, pass.what.bleed());
  // A band reaches profile.max() px off its spine, and a width profile is
  // REQUIRED to be able to report that number — which is the whole reason
  // `max()` is part of that interface. A width function that cannot state
  // its own maximum can only be clipped silently.
  if (const Across* band = node.bandWidth())
    bleed = std::max(bleed, band->profile.max());
  for (const Echo& e : echoesOf(node))
    bleed =
        std::max(bleed, std::max(std::abs(e.offset.fX), std::abs(e.offset.fY)));
  // An fx() track throws glyphs OUTSIDE the text's box — a rise starts
  // below the line, a scatter starts anywhere in its disc — and a cull
  // taken at the box truncates them at the cached picture or texture
  // bounds, exactly as an under-reported decoration bleed does. Each track
  // declares how far it reaches, the same over-report-is-safe contract
  // `bleed()` and `reach()` carry.
  for (const Track& t : tracksOf(node))
    if (t.effect) bleed = std::max(bleed, t.reachPx());
  // A Material can declare a reserve too: a fill whose own outline escapes
  // the node's box is truncated at the cached picture or texture bounds
  // otherwise, exactly as an under-reported decoration bleed is. Both
  // carriers are checked — the live/geometry slot and the static recipe.
  if (node.materialData) {
    if (node.materialData->live)
      bleed = std::max(bleed, node.materialData->live->bleed());
    if (node.materialData->recipe)
      bleed = std::max(bleed, node.materialData->recipe->bleed());
  }
  if (bleed > 0) local.outset(bleed, bleed);
  // Routed elements paint their derive-resolved PATH, which is not bounded
  // by the layout rect (a connector's box is one thing, its wire another) —
  // the cull must hold the route plus its stroke reach.
  if (node.deriveData &&
      (!node.deriveData->connectFrom.empty() ||
       !node.deriveData->railAnchors.empty()) &&
      !inst.connectorPath.isEmpty()) {
    SkRect route = inst.connectorPath.getBounds();
    route.outset(bleed + 8.0f, bleed + 8.0f);
    local.join(route);
  }
  // A PATH BASELINE is the same problem once more. The baseline resolves
  // against the node's own box, so a `shapes::` generator normally stays
  // inside it — but nothing requires that: a Shape may return a curve well
  // outside the box, and `TextPath::offset` rides the type further off it
  // again. The glyphs then stand an ascent above that curve and a descent
  // below it, plus whatever the tracks reach, so the cull holds the curve
  // outset by the whole band. Over-reporting is safe here as everywhere;
  // under-reporting truncates the run at the cached picture or texture
  // bounds with no diagnostic.
  if (node.textData && node.textData->onPath) {
    const TextPath& spec = *node.textData->onPath;
    const SkPath baseline = spec.path({rect.width(), rect.height()});
    if (!baseline.isEmpty()) {
      const TextMetrics band = metrics(node.textData->style, fonts);
      const float reach =
          std::max(band.ascent, band.descent) + std::abs(spec.offset) + bleed;
      SkRect curve = baseline.getBounds();
      curve.outset(reach, reach);
      local.join(curve);
    }
  }
  // A BAND is the same problem: the bleed above covers the width axis, but
  // a BORROWED spine (band(around(key))) can sit anywhere relative to this
  // node's own box, so the cull has to hold the spine itself — exactly the
  // routed case one paragraph up, and for the same reason.
  if (const Across* band = node.bandWidth()) {
    const SkPath spine =
        node.deriveData->bandSpine
            ? node.deriveData->bandSpine({rect.width(), rect.height()})
            : inst.bandSpine;
    if (!spine.isEmpty()) {
      SkRect swept = spine.getBounds();
      swept.outset(bleed + band->profile.max(), bleed + band->profile.max());
      local.join(swept);
    }
  }
  return local;
}

// ---------------------------------------------------------------------------
// travel(): the motion path
//
// The animated lane is `t` — WHERE ALONG the curve the node sits — so the
// whole bind() chain applies to the schedule while the Shape supplies the
// geometry. A Shape is a function of a SIZE, and the curve is resolved
// against the PARENT's box (the frame the node moves in), so a relayout
// re-shapes the curve under a moving node. `t` is untouched by that: the
// node slides to the same fraction of the new curve rather than jumping to
// a different phase of its schedule.

std::optional<std::pair<SkPoint, float>> Composer::Impl::motionPathSample(
    Instance& inst, const SkSize& frame) {
  const ElementNode& node = *inst.desc;
  if (!node.motionData || !(bool)node.motionData->path) return std::nullopt;
  const MotionPath& spec = *node.motionData;

  // The table, cached against the two inputs that determine it: the Shape
  // VALUE and the size it was resolved at. No dirty flag — a comparable
  // scheme keeps its table across describes, a raw callable re-measures
  // (which is the escape hatch's documented cost, here as everywhere).
  if (!inst.motion) inst.motion = std::make_unique<Instance::MotionCache>();
  Instance::MotionCache& cache = *inst.motion;
  if (!(cache.shape == spec.path) || cache.size.width() != frame.width() ||
      cache.size.height() != frame.height()) {
    cache.shape = spec.path;
    cache.size = frame;
    cache.contours = geometry::path::Contour::of(spec.path(frame));
    cache.total = geometry::path::totalLength(cache.contours);
    cache.closed = geometry::path::closedThroughout(cache.contours);
  }
  if (!(cache.total > 0))
    return std::nullopt;  // no measurable length ⇒ not engaged

  // WRAP on a closed curve, CLAMP on an open one. The FRACTION is what
  // wraps, so `t` past 1 is another lap of the whole curve rather than
  // another lap of whichever contour it landed in; the pose read below
  // then walks every contour as one arc-length coordinate.
  const auto walk = [&](float u) {
    float w = cache.closed ? std::fmod(u, 1.0f) : std::clamp(u, 0.0f, 1.0f);
    if (cache.closed && w < 0.0f) w += 1.0f;
    return geometry::path::toSk(
        geometry::path::poseAlong(cache.contours, w * cache.total).position);
  };

  const float t = inst.resolveFloat(Instance::kMotionT, spec.t);
  const SkPoint here = walk(t);
  float orient = 0;
  if (spec.lookAhead != 0.0f) {
    SkVector chord = walk(t + spec.lookAhead) - here;
    // At the end of an OPEN curve the forward chord collapses; hold the
    // last good one rather than reading atan2(0, 0).
    if (chord.length() <= 1e-6f) chord = here - walk(t - spec.lookAhead);
    if (chord.length() > 1e-6f)
      orient = std::atan2(chord.y(), chord.x()) * 180.0f / SK_FloatPI;
  }
  return std::make_pair(here, orient);
}

Composer::Impl::NodeTransform Composer::Impl::transformOf(Instance& inst) {
  const ElementNode& node = *inst.desc;
  NodeTransform out;
  out.rot = inst.resolveFloat(Instance::kRotate, node.paint.rotate);
  out.scl = inst.resolveFloat(Instance::kScale, node.paint.scale);
  out.sx = inst.resolveFloat(Instance::kScaleX, node.paint.scaleX);
  out.sy = inst.resolveFloat(Instance::kScaleY, node.paint.scaleY);
  out.skx = inst.resolveFloat(Instance::kSkewX, node.paint.skewX);
  out.sky = inst.resolveFloat(Instance::kSkewY, node.paint.skewY);

  const SkRect rect = instanceRect(inst);
  // The curve is resolved in the frame the node MOVES in — its parent's
  // box (a root node has none, so its own box, which is the canvas).
  const SkRect frameRect = inst.parent ? instanceRect(*inst.parent) : rect;
  if (std::optional<std::pair<SkPoint, float>> sample = motionPathSample(
          inst, SkSize{frameRect.width(), frameRect.height()})) {
    // PRECEDENCE: the path drives position OUTRIGHT (the lanes are not
    // read at all), and ADDS its tangent angle to rotate() rather than
    // replacing it — see MotionPath.
    const SkPoint origin =
        resolveOrigin(node.paint, rect.width(), rect.height());
    out.tx = sample->first.x() - rect.left() - origin.x();
    out.ty = sample->first.y() - rect.top() - origin.y();
    out.rot += sample->second;
    return out;
  }
  out.tx = inst.resolveFloat(Instance::kTx, node.paint.translateX);
  out.ty = inst.resolveFloat(Instance::kTy, node.paint.translateY);
  return out;
}

/** The rect a node's RECORDING must cover, in its own local space: its own
 *  paint bounds (ownPaintBounds above), unioned with every child's bounds
 *  mapped through that child's layout offset and static paint transforms.
 *
 *  WHAT THIS RECT DOES NOT DO, which is easy to assume it does:
 *  SkPictureRecorder does NOT reject ops outside the cull rect at record
 *  time. An op drawn wholly outside it is still recorded, even when the
 *  cull rect is EMPTY, and a plain drawPicture replays it — the pixels
 *  land. Culling against the cull rect happens only when a bounding-box
 *  hierarchy is attached (SkRTreeFactory clips each op's bounds to the cull
 *  rect as it builds the tree, so an outside op is dropped at PLAYBACK),
 *  and no BBH is attached here, so the picture path never culls.
 *  ComposeCullRect.PictureCullDoesNotCullWithoutABbh holds that behaviour
 *  down.
 *
 *  What the rect IS load-bearing for are this function's other three
 *  consumers, all of which clip for real: the BOUNDED saveLayer opened for
 *  a group opacity/blend and for a layer effect (saveLayer bounds ARE a
 *  clip), the Cache::Texture bake surface, which is sized from this rect
 *  mapped to device, and the dstIn coverage drawRect. A child translated
 *  beyond its parent's box vanishes through THOSE if the child union below
 *  is dropped — pinned by
 *  ComposeCache.OverflowingChildSurvives{GroupOpacityLayer,TextureBake}.
 *  Overflow is legal; the rect must hold it, the same way it must hold a
 *  decoration's declared bleed.
 *
 *  Animated transforms are fine here: resolveFloat reads the record-time
 *  value, and a RUNNING transform makes the subtree volatile, so nothing
 *  records at all. A clipped node contributes only its own box, because its
 *  children cannot escape it. */
SkRect Composer::Impl::recordBounds(Instance& inst) {
  const ElementNode& node = *inst.desc;
  SkRect local = ownPaintBounds(inst);
  if (node.clipContent) return local;
  for (auto& child : inst.children) {
    const ElementNode& cn = *child->desc;
    const SkRect crect = instanceRect(*child);
    SkRect cb = recordBounds(*child);  // child-local
    const NodeTransform tf = transformOf(*child);
    // The matrix comes from NodeTransform::matrix(), gate included, and not
    // from a copy of that build written here. One resolver, three consumers
    // — paint()'s matrix, this child union, and hitInstance()'s inverse —
    // and the three must build the SAME matrix or a node draws where it
    // cannot be hit. A hand-rolled gate here that omits a lane is
    // invisible: a child whose only transform was a per-axis scale would
    // contribute UNSCALED bounds, so its parent's effect layer, opacity
    // layer and texture bake would all be sized to the unscaled box and
    // truncate the overflow.
    const SkMatrix m = tf.matrix({crect.left(), crect.top()}, cn.paint,
                                 crect.width(), crect.height());
    local.join(m.mapRect(cb));
  }
  return local;
}

}  // namespace sigil::compose
