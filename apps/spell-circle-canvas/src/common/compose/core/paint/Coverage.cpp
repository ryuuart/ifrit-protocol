/** @file
 * The coverage boundary: what a node DREW, as a path.
 *
 * A node's shape and its glyph outlines are both descriptions — one of a
 * box, one of a placement — and neither knows anything about an alpha
 * cut-out, a clip or a mask. Those are facts about the pixels, so this
 * answers them from the pixels: rasterise the node's layer into an alpha
 * surface, keep the pixels its paint covered, and trace their outline.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRegion.h>
#include <include/core/SkSurface.h>
#include <include/pathops/SkPathOps.h>
#include <src/core/SkScopeExit.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "HeldCoverage.h"
#include "PaintInternal.h"
#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** THE CEILING ON THE TRACE RASTER, in pixels along the node's longer
 *  side.
 *
 *  The trace rasterises at the NODE'S OWN DEVICE SCALE, so a step of the
 *  staircase it produces is one device pixel and the boundary is as fine
 *  as the edge the viewer is looking at — which is the whole reason to
 *  trace pixels rather than a shape. This is only the bound that keeps a
 *  very large node from asking for a very large surface: past it the
 *  raster is scaled down to fit and the steps grow, which is the same
 *  coarsening a decoration on a huge node can afford and the only
 *  alternative to refusing outright. */
constexpr int kMaxTraceRaster = 2048;

/** Whether what @p inst covers may have changed since its trace: its paint
 *  is dirty for a reason other than a change of bake density. */
bool coverageChanged(const Instance& inst) {
  return inst.paintDirty && !inst.paintDirtyOnlyForDensity;
}

std::atomic<size_t> coverageTraces{0};

}  // namespace

size_t detail::coverageTracesTaken() { return coverageTraces.load(); }

/** THE ALPHA, AS A REGION, at the tolerance the node declared: a pixel is
 *  inside when its coverage reaches `covered`, which
 *  `Element::decorationOutline` states as a fraction of full opacity and which
 *  defaults to the rule an unantialiased rasteriser uses — the paint
 *  reached at least half the pixel, so the traced edge is where the drawn
 *  edge is rather than half a pixel outside it. Raise it and a wash or a
 *  soft glow stops being a silhouette; lower it and it starts being one.
 *
 *  Skia carries no alpha-raster-to-path tracer, and it does carry
 *  everything a region needs: covered pixels become horizontal runs, runs
 *  that repeat down the raster become one band of rects, and SkRegion
 *  turns rects into a region and a region into its boundary path. The
 *  answer is exact for the raster it was given — every edge axis-aligned,
 *  every step one pixel — which is the honest shape of a traced raster and
 *  not an approximation of a smooth curve. */
SkRegion detail::regionOfCoveredPixels(const SkPixmap& alpha, uint8_t covered) {
  const int width = alpha.width(), height = alpha.height();
  std::vector<SkIRect> rects;
  std::vector<SkIRect> row, previous;  // the runs of this row, and the last
  int bandTop = 0;
  const auto flushBand = [&](int bandBottom) {
    for (const SkIRect& run : previous)
      rects.push_back(
          SkIRect::MakeLTRB(run.left(), bandTop, run.right(), bandBottom));
  };
  for (int y = 0; y < height; ++y) {
    row.clear();
    const uint8_t* pixels = alpha.addr8(0, y);
    int runStart = -1;
    for (int x = 0; x < width; ++x) {
      // ANY INK AT ALL is the floor: a threshold of zero asks for
      // everything the node drew, not for the whole box, and a pixel no
      // paint reached is outside whatever the tolerance says.
      const bool inside = pixels[x] > 0 && pixels[x] >= covered;
      if (inside && runStart < 0) runStart = x;
      if (!inside && runStart >= 0) {
        row.push_back(SkIRect::MakeLTRB(runStart, 0, x, 1));
        runStart = -1;
      }
    }
    if (runStart >= 0) row.push_back(SkIRect::MakeLTRB(runStart, 0, width, 1));
    // Rows with the same runs are one band of rects rather than one set of
    // rects each: a box traces to a single rect however tall it is, and the
    // rect count a region has to merge stays proportional to the
    // silhouette's detail instead of to its height.
    const bool sameAsAbove =
        row.size() == previous.size() &&
        std::equal(row.begin(), row.end(), previous.begin());
    if (!sameAsAbove) {
      flushBand(y);
      previous = row;
      bandTop = y;
    }
  }
  flushBand(height);
  SkRegion region;
  region.setRects(rects.data(), (int)rects.size());
  return region;
}

const SkPath& Composer::Impl::coverageOutline(Instance& inst, SkSize size,
                                              float contentScale) {
  const bool sized = size.width() > 0 && size.height() > 0;
  // The device scale is part of the answer, not just of its cost: the
  // staircase is one device pixel a step, so a node that moves to a denser
  // display traces a finer boundary and must be traced again.
  const float threshold = inst.description->coverageThreshold;
  // WHAT THE RASTER COVERS IS THE NODE'S PAINT BOUNDS, not its box. The
  // silhouette is the ink, and a node's ink stands wherever its carriers
  // put it — a declared shape resolved past the box it was handed, a
  // decoration's bleed, a glyph's overhang, a routed path — so a surface
  // allocated at the box traces a boundary cut square at the box's edges
  // and hands a decoration an outline the node never drew. This is the one
  // place a node is sized, and the trace reads it like every other
  // consumer. The rect it answers is in the node's own local space, so an
  // origin left of the box is a NEGATIVE left and the raster carries that
  // offset.
  const SkRect covers = sized ? ownPaintBounds(inst) : SkRect::MakeEmpty();
  // What the subtree covers is all a trace reads, so a subtree whose
  // colours move and whose coverage holds keeps its trace, and so does one
  // dirty only because the bake density changed: the trace is taken at the
  // host's scale and nothing it covers depends on the density.
  const bool stale = coverageChanged(inst) || inst.silhouetteVolatile ||
                     inst.coverageOutlineSize != size ||
                     inst.coverageOutlineBounds != covers ||
                     inst.coverageOutlineScale != contentScale ||
                     inst.coverageOutlineThreshold != threshold;
  if (!stale || !sized) {
    if (!sized) {
      inst.coverageOutline.reset();
      inst.coverageParts.clear();
      inst.coverageOutlineSize = size;
      inst.coverageOutlineBounds = covers;
      inst.coverageOutlineScale = contentScale;
      inst.coverageOutlineThreshold = threshold;
    }
    return inst.coverageOutline;
  }
  inst.coverageOutlineSize = size;
  inst.coverageOutlineBounds = covers;
  inst.coverageOutlineScale = contentScale;
  inst.coverageOutlineThreshold = threshold;
  inst.coverageOutline.reset();
  inst.coverageParts.clear();
  if (covers.isEmpty()) return inst.coverageOutline;

  const float longer = std::max(covers.width(), covers.height());
  const float scale = std::min(contentScale > 0 ? contentScale : 1.0f,
                               (float)kMaxTraceRaster / std::max(longer, 1.0f));
  // A node whose own paint was invalidated traces its own part again.
  if (coverageChanged(inst)) inst.coverageOwn.scale = -1.0f;
  // A SILHOUETTE WHOSE PARTS HOLD: where only children move, and move
  // rigidly, the node's own paint and each child's silhouette are traced
  // once and held, and a frame places them where the children now stand
  // rather than painting the whole subtree into a raster again.
  if (std::optional<SkPath> composed =
          composedCoverage(inst, covers, scale, contentScale, threshold)) {
    inst.coverageOutline = std::move(*composed);
    return inst.coverageOutline;
  }
  inst.coverageOutline =
      traceCoverage(inst, covers, scale, contentScale, threshold, Phase::All);
  return inst.coverageOutline;
}

SkPath Composer::Impl::traceCoverage(Instance& inst, const SkRect& covers,
                                     float scale, float contentScale,
                                     float threshold, Phase phase) {
  // THE RASTER'S GRID IS THE BOX'S GRID. The pixel edges are placed on
  // whole multiples of the trace's own step, so the pixels covering the
  // box are the same pixels whether or not a carrier widened the rect
  // around them: a boundary is a staircase of whole steps and a rect that
  // moved the grid by a fraction of one would restep every edge in the
  // picture for a carrier standing somewhere else entirely.
  const SkIRect raster =
      SkRect::MakeLTRB(covers.left() * scale, covers.top() * scale,
                       covers.right() * scale, covers.bottom() * scale)
          .roundOut();
  const int width = std::max(1, raster.width());
  const int height = std::max(1, raster.height());
  // One channel, because coverage is the only channel the answer reads.
  const sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeA8(width, height));
  if (!surface) return SkPath();
  coverageTraces.fetch_add(1);
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.translate((float)-raster.left(), (float)-raster.top());
  canvas.scale(scale, scale);

  // The trace's canvas is an offscreen raster at a scale of its own, so
  // nothing pinned to a device rect may be baked inside it: it is an
  // UNPINNED recording, the same refusal a recording under a declared
  // motion takes, and its own canvas is the device it draws on.
  //
  // At FULL OPACITY and the plain source blend, which is what the node's
  // layer holds: its own opacity and blend mode are applied to the
  // finished layer above this, and a silhouette that thinned as the node
  // faded would walk a decoration's edge for a reason that has nothing to
  // do with what was drawn.
  // A picture replayed inside the trace hands the ENCLOSING recording its
  // device blits and its verdict on the matrix unless the trace holds them
  // apart: this raster is not that recording, and what happens in it
  // decides nothing about what the enclosing one may hold.
  {
    const Instance* const outerTrace = coverageTrace;
    const RecordingState outer = recording;
    bool painted = false;
    const SkScopeExit leaveTrace([this, &inst, outerTrace, &outer, &painted] {
      coverageTrace = outerTrace;
      recording = outer;
      // An interrupted trace holds no silhouette and must be retried.
      if (!painted) inst.coverageOutlineScale = -1.0f;
    });
    coverageTrace = &inst;
    recording.replay = SkMatrix::I();
    recording.replayInverse = SkMatrix::I();
    recording.clearHeld();
    recording.clearScaleWindow();
    ++recording.depth;
    ++recording.unpinnedDepth;
    // The raster is what the trace's draws land on: an image bound for the
    // enclosing recording's device would draw nothing into it.
    recording.canvas = &canvas;
    paintContent(inst, canvas, contentScale, SkBlendMode::kSrcOver, 1.0f,
                 phase);
    painted = true;
  }

  SkPixmap alpha;
  if (!surface->peekPixels(&alpha)) return SkPath();
  const SkRegion covered = regionOfCoveredPixels(
      alpha, (uint8_t)std::clamp(std::lround(threshold * 255.0f), 0L, 255L));
  if (covered.isEmpty()) return SkPath();
  // Back into the node's own local space, which is where a decoration and
  // a flow exclusion both read it: the raster's own pixels, unscaled, then
  // shifted by the offset the grid was placed at. A node whose ink stays
  // inside its box traces the path it always did, since that offset is
  // zero steps for it.
  return covered.getBoundaryPath()
      .makeTransform(
          SkMatrix::Translate((float)raster.left(), (float)raster.top()))
      .makeScale(1.0f / scale, 1.0f / scale);
}

std::optional<SkPath> Composer::Impl::composedCoverage(Instance& inst,
                                                       const SkRect& covers,
                                                       float scale,
                                                       float contentScale,
                                                       float threshold) {
  const ElementNode& node = *inst.description;
  const ComputedStyle& style = inst.computed;
  // The node's own paint must hold, and nothing of its own may wrap its
  // children: a clip, a mask or a filter over the layer reads the children
  // where they stand and is not the union of their silhouettes.
  if (inst.ownCoverageVolatile || coverageChanged(inst) || style.clipContent ||
      layerEffectOf(node) || backdropEffectOf(node) || node.hasMasks() ||
      hostsSpace(inst))
    return std::nullopt;
  bool anyMoving = false;
  struct Part {
    Instance* child;
    SkMatrix placed;
  };
  std::vector<Part> parts;
  for (const auto& owned : inst.children) {
    Instance& child = *owned;
    if (child.computed.layout.display == Display::None) continue;
    const ElementNode& described = *child.description;
    // A child is held when what it draws changes in nothing but where it
    // stands: no content of its own in motion, nothing moving below it,
    // drawn whole at full opacity and the plain blend, on a flat plane, and
    // dressed by no mark of its own, which a trace of the child alone
    // would leave out.
    if (child.silhouetteVolatile || coverageChanged(child) ||
        described.fxData ||
        !described.backgrounds.empty() || !described.foregrounds.empty() ||
        child.computed.paint.opacity.identity() != nullptr ||
        child.computed.paint.blendMode != SkBlendMode::kSrcOver ||
        child.resolveFloat(Instance::kOpacity, child.computed.paint.opacity) <
            1.0f ||
        hostsSpace(child))
      return std::nullopt;
    const NodeTransform tf = transformOf(child);
    if (tf.spatial() || described.depthData) return std::nullopt;
    anyMoving |= child.transformLive;
    const SkRect rect = instanceRect(child);
    SkMatrix placed = SkMatrix::Translate(rect.left(), rect.top());
    placed.preConcat(tf.matrix({0, 0}, rect.width(), rect.height()));
    parts.push_back({&child, placed});
  }
  // Only a node whose children are moving is worth composing: a still one
  // is traced once and held whole.
  if (!anyMoving) return std::nullopt;

  Instance::OwnCoverage& own = inst.coverageOwn;
  if (own.bounds != covers || own.scale != contentScale ||
      own.threshold != threshold) {
    own.outline = traceCoverage(inst, covers, scale, contentScale, threshold,
                                Phase::OwnOnly);
    own.bounds = covers;
    own.scale = contentScale;
    own.threshold = threshold;
  }
  std::vector<CoveragePart> held;
  std::vector<SkPath> pieces;
  if (!own.outline.isEmpty()) {
    held.push_back({own.outline, SkMatrix::I()});
    pieces.push_back(own.outline);
  }
  for (const Part& part : parts) {
    Instance& child = *part.child;
    const SkRect rect = instanceRect(child);
    const SkPath& outline =
        coverageOutline(child, {rect.width(), rect.height()}, contentScale);
    if (outline.isEmpty()) continue;
    held.push_back({outline, part.placed});
    pieces.push_back(outline.makeTransform(part.placed));
  }
  std::optional<SkPath> result;
  if (pieces.empty()) {
    result = SkPath();
  } else if (pieces.size() == 1) {
    result = std::move(pieces.front());
  } else {
    // Silhouettes overlap where children do, and a hole in one may be
    // covered by another, so the parts are united rather than appended.
    SkOpBuilder united;
    for (const SkPath& piece : pieces) united.add(piece, kUnion_SkPathOp);
    result = united.resolve();
  }
  if (result) inst.coverageParts = std::move(held);
  return result;
}

}  // namespace sigil::compose
