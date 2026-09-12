/** @file
 * THE STAMPED KINDS — a mark laid down as COPIES of a picture rather than
 * as a stroke: `brush::Scatter`, which places its art at samples along
 * the outline, and `brush::Pattern`, which lays a tile end to end and fits
 * the count to the run.
 *
 * Both stand on the same two bodies: the placement resolver, which turns
 * a `brush::Placement` into concrete samples (position and tangent), and
 * `drawStamp`, which is the one place a picture is turned, scaled and
 * blended onto a sample — so a scatter grain and a pattern tile cannot
 * disagree about what a stamp at a sample means.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Stamps.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/path/Numeric.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "BakedArt.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose::brush {

namespace {

/** Resolve a Placement into concrete samples (position + tangent).
 *  @param spacing the host brush's spacing — what an UNSET `interval`
 *         resolves to (the sugar; see Placement::interval). Passed in
 *         rather than defaulted here because only the brush owns it. */
std::vector<PathSample> placementSamples(const SkPath& path, const Placement& p,
                                         float spacing) {
  std::vector<PathSample> out;
  using Mode = Placement::Mode;
  if (p.mode == Mode::Interval || p.mode == Mode::CentralPoint) {
    const float interval = p.interval.value_or(spacing);
    SkContourMeasureIter iter(path, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      const float step =
          interval <= 1.0f ? len * std::max(interval, 0.001f) : interval;
      // A zero-length contour, or a fractional interval on one, gives a
      // step that never advances: the walk below would stand still
      // forever on any offset that starts before the end.
      if (step <= 0 || len <= 0) continue;
      const float phase = p.offset <= 1.0f && p.offset >= -1.0f &&
                                  p.mode == Mode::Interval && interval <= 1.0f
                              ? len * p.offset
                              : p.offset;
      auto sampleAt = [&](float d) {
        SkPoint pos;
        SkVector tan;
        if (contour->getPosTan(std::clamp(d, 0.0f, len), &pos, &tan))
          out.push_back({pos, tan, d, len > 0 ? d / len : 0});
      };
      if (p.mode == Mode::CentralPoint) {
        sampleAt(len * 0.5f);
      } else {
        // the loop walks a distance; the accumulated float is the position
        // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
        for (float d = phase + step * 0.5f; d < len; d += step) sampleAt(d);
      }
    }
    return out;
  }
  // Vertex family: walk the REAL verbs per contour.
  std::vector<std::vector<SkPoint>> contours;
  SkPath::RawIter it(path);
  SkPoint pts[4];
  for (SkPath::Verb v = it.next(pts); v != SkPath::kDone_Verb;
       v = it.next(pts)) {
    switch (v) {
      case SkPath::kMove_Verb:
        contours.push_back({pts[0]});
        break;
      case SkPath::kLine_Verb:
        contours.back().push_back(pts[1]);
        break;
      case SkPath::kQuad_Verb:
      case SkPath::kConic_Verb:
        contours.back().push_back(pts[2]);
        break;
      case SkPath::kCubic_Verb:
        contours.back().push_back(pts[3]);
        break;
      default:
        break;
    }
  }
  for (const auto& c : contours) {
    if (c.empty()) continue;
    auto tangentAt = [&](size_t i) {
      const SkPoint prev = c[i > 0 ? i - 1 : i];
      const SkPoint next = c[i + 1 < c.size() ? i + 1 : i];
      SkVector t{next.x() - prev.x(), next.y() - prev.y()};
      const float m = std::hypot(t.x(), t.y());
      return m > 1e-4f ? SkVector{t.x() / m, t.y() / m} : SkVector{1, 0};
    };
    const float n = (float)c.size();
    switch (p.mode) {
      case Mode::Vertex:
        for (size_t i = 0; i < c.size(); ++i)
          out.push_back(
              {c[i], tangentAt(i), 0, n > 1 ? (float)i / (n - 1) : 0});
        break;
      case Mode::FirstVertex:
        out.push_back({c.front(), tangentAt(0), 0, 0});
        break;
      case Mode::LastVertex:
        out.push_back({c.back(), tangentAt(c.size() - 1), 0, 1});
        break;
      case Mode::InnerVertices:
        for (size_t i = 1; i + 1 < c.size(); ++i)
          out.push_back(
              {c[i], tangentAt(i), 0, n > 1 ? (float)i / (n - 1) : 0});
        break;
      case Mode::SegmentCenter:
        for (size_t i = 0; i + 1 < c.size(); ++i) {
          const SkPoint mid{(c[i].x() + c[i + 1].x()) / 2,
                            (c[i].y() + c[i + 1].y()) / 2};
          SkVector t{c[i + 1].x() - c[i].x(), c[i + 1].y() - c[i].y()};
          const float m = std::hypot(t.x(), t.y());
          if (m > 1e-4f)
            out.push_back({mid,
                           {t.x() / m, t.y() / m},
                           0,
                           n > 1 ? ((float)i + 0.5f) / (n - 1) : 0});
        }
        break;
      default:
        break;
    }
  }
  return out;
}

void drawStamp(SkCanvas& c, const SkPicture& picture, const PathSample& sample,
               bool align, float rotateDeg, float scaleX, float scaleY,
               const StampModifier& m) {
  if (m.skip || m.alpha <= 0.003f || m.scale <= 0.001f) return;
  const SkRect cull = picture.cullRect();
  c.save();
  c.translate(sample.position.x(), sample.position.y());
  if (align)
    c.rotate(geometry::path::degrees(
        std::atan2(sample.tangent.y(), sample.tangent.x())));
  c.translate(m.dAlong, m.dNormal);  // tangent frame (post-align)
  c.rotate(rotateDeg + m.rotateDeg);
  c.scale(scaleX * m.scale, scaleY * m.scale);
  c.translate(-cull.width() / 2, -cull.height() / 2);
  if (m.alpha < 1.0f) {
    SkPaint fade;
    fade.setAlphaf(m.alpha);
    c.drawPicture(&picture, nullptr, &fade);
  } else {
    c.drawPicture(&picture);
  }
  c.restore();
}

}  // namespace

void Scatter::paint(SkCanvas& c, const PaintContext& ctx) const {
  if (spacing <= 0 || !ctx.fonts) return;
  // Prefer the instance-side store, so a brush value rebuilt every
  // describe still finds its art's bake; the member cache is the
  // standalone-paint fallback.
  sk_sp<SkPicture> picture;
  if (ctx.stamps) {
    if (const StampCache::Entry* e = ctx.stamps->get(art.node()))
      picture = e->picture;
    if (!picture) {
      // Shell box: snapshot() sizes by the root's CHILDREN and ignores
      // the root's own dimensions.
      picture = snapshot(box().child(art), *ctx.fonts);
      ctx.stamps->put(art.node(), {picture, nullptr, {0, 0}});
    }
  } else {
    if (!cache->picture || !bakedFromNode(cache->bakedFor, art.node())) {
      cache->picture = snapshot(box().child(art), *ctx.fonts);
      cache->bakedFor = art.node();
    }
    picture = cache->picture;
  }
  if (!picture) return;

  // An unset place.interval takes `spacing`, resolved here where the
  // spacing lives rather than by comparing against a sentinel value an
  // author could also have typed on purpose.
  std::vector<PathSample> samples =
      placementSamples(ctx.outline, place, spacing);
  for (size_t i = 0; i < samples.size(); ++i) {
    StampModifier m;
    if (modifier) m = modifier(samples[i], i, samples.size());
    if (seed != 0) {
      const uint32_t k = (uint32_t)i;
      m.dAlong += core::noise::hash(seed, 4 * k) * jitterAlong;
      m.dNormal += core::noise::hash(seed, 4 * k + 1) * jitterNormal;
      m.scale *= 1.0f + core::noise::hash(seed, 4 * k + 2) * jitterScale;
      m.rotateDeg += core::noise::hash(seed, 4 * k + 3) * jitterRotateDeg;
    }
    drawStamp(c, *picture, samples[i], alignToPath, 0, 1, 1, m);
  }
}

void Pattern::paint(SkCanvas& c, const PaintContext& ctx) const {
  if (!ctx.fonts) return;
  static const std::shared_ptr<detail::ElementNode> kNoArt;
  auto node = [](const std::optional<Element>& e)
      -> const std::shared_ptr<detail::ElementNode>& {
    return e ? e->node() : kNoArt;
  };
  const std::shared_ptr<detail::ElementNode>& sideNode = side.node();
  const std::shared_ptr<detail::ElementNode>& startNode = node(start);
  const std::shared_ptr<detail::ElementNode>& endNode = node(end);
  const std::shared_ptr<detail::ElementNode>& cornerNode =
      corner ? corner->art.node() : kNoArt;
  if (!bakedFromNode(cache->bakedSide, sideNode) ||
      !bakedFromNode(cache->bakedStart, startNode) ||
      !bakedFromNode(cache->bakedEnd, endNode) ||
      !bakedFromNode(cache->bakedCorner, cornerNode)) {
    *cache = Cache{};
    cache->bakedSide = sideNode;
    cache->bakedStart = startNode;
    cache->bakedEnd = endNode;
    cache->bakedCorner = cornerNode;
  }
  // Each slot warms from the instance-side store first, so a Pattern
  // value rebuilt every describe — whose member cache is empty — still
  // reuses its arts' bakes; a miss bakes once and publishes back.
  auto bake = [&](const Element& e, sk_sp<SkPicture>& slot) {
    if (slot) return;
    if (ctx.stamps)
      if (const StampCache::Entry* hit = ctx.stamps->get(e.node()))
        slot = hit->picture;
    if (!slot) {  // shell box: snapshot() sizes by the root's CHILDREN
      slot = snapshot(box().child(e), *ctx.fonts);
      if (ctx.stamps && slot)
        ctx.stamps->put(e.node(), {slot, nullptr, {0, 0}});
    }
  };
  bake(side, cache->side);
  if (start) bake(*start, cache->start);
  if (end) bake(*end, cache->end);
  if (corner) bake(corner->art, cache->corner);
  if (!cache->side) return;
  // An AUTHORED advance is floored at a pixel exactly as the intrinsic
  // one is: a tile a fraction of a pixel long is millions of tiles on any
  // run long enough to see.
  const float authored = advance > 0 ? std::max(advance, 1.0f) : 0.0f;
  const float tileLen =
      authored > 0 ? authored : std::max(cache->side->cullRect().width(), 1.0f);

  size_t placed = 0;
  // Two passes: count side tiles first so modifier sees the true total.
  std::vector<std::pair<PathSample, float>> sideSlots;  // sample + scaleX
  std::vector<std::pair<PathSample, const SkPicture*>> caps;

  for (const geometry::path::Contour& contour :
       geometry::path::Contour::of(ctx.outline)) {
    const float len = contour.length();
    const bool closed = contour.closed();

    // Corners come from the one shared scanner (lines::detail::
    // cornersOrWarn), so the same shape reports the same corners whichever
    // decoration asks, and the diagnostic it prints when a scan finds
    // nothing reaches a pattern brush too.
    //
    // The scanner returns the vertex position AND the two leg tangents,
    // which is what the corner tiles below need. Re-probing the contour
    // at a fixed distance from the vertex instead would land both probes
    // on the same leg whenever it is shorter than the probe, aiming the
    // tile at the outgoing tangent regardless of the alignment asked
    // for.
    std::vector<geometry::path::Contour::Corner> corners;
    if (cache->corner)
      corners = sigil::compose::detail::cornersOrWarn(
          contour, cornerAngleDeg, tileLen * 0.5f,
          std::clamp(tileLen * 0.25f, 1.0f, 6.0f));

    // Open-contour caps reserve their slots at the ends.
    float head = 0, tail = 0;
    if (!closed && cache->start)
      head = authored > 0 ? authored : cache->start->cullRect().width();
    if (!closed && cache->end)
      tail = authored > 0 ? authored : cache->end->cullRect().width();

    // Runs between corners (and cap margins). Each corner RESERVES half
    // its own length at each end of its two adjacent runs, so the side
    // tiles butt against the corner art instead of running under it.
    const float cornerRoom =
        cache->corner ? (cornerLength > 0 ? cornerLength
                                          : cache->corner->cullRect().width())
                      : 0.0f;
    const float halfCorner = cornerRoom * 0.5f;
    std::vector<float> bounds{head};
    for (const geometry::path::Contour::Corner& hit : corners)
      if (hit.distance > head && hit.distance < len - tail) {
        bounds.push_back(hit.distance - halfCorner);  // run ends before
        bounds.push_back(hit.distance + halfCorner);  // next run starts after
      }
    bounds.push_back(len - tail);

    for (size_t r = 0; r + 1 < bounds.size(); ++r) {
      // Odd spans are the reserved corner gaps themselves — skip them.
      if (halfCorner > 0 && r % 2 == 1) continue;
      const float a = bounds[r], b = bounds[r + 1];
      const float L = b - a;
      if (L < tileLen * 0.25f) continue;
      const int n = std::max(1, (int)std::lround(L / tileLen));
      const float slot = L / (float)n;
      const float sx = stretchToFit ? slot / tileLen : 1.0f;
      for (int i = 0; i < n; ++i) {
        const float d = a + slot * ((float)i + 0.5f);
        if (const auto at = contour.at(d))
          sideSlots.push_back(
              {{geometry::path::toSk(at->position),
                geometry::path::toSk(at->tangent), d, len > 0 ? d / len : 0},
               sx});
      }
    }

    // Corner tiles sit on the bisector of the break, or on the outgoing
    // leg — both tangents came out of the detection, so nothing is
    // re-probed here. No diagnostic is needed either: the alignment is a
    // required constructor argument of CornerArt, so corner art with no
    // stated alignment cannot be described in the first place.
    if (cache->corner)
      for (const geometry::path::Contour::Corner& hit : corners) {
        const auto at = contour.at(hit.distance);
        if (!at) continue;
        SkVector dir{hit.in.x + hit.out.x, hit.in.y + hit.out.y};
        // A hairpin's legs cancel: in + out ≈ 0 and atan2(0,0) is a
        // silent zero rotation. Fall back to the outgoing leg.
        if (dir.length() < 1e-3f || corner->align == CornerAlign::Outgoing)
          dir = geometry::path::toSk(hit.out);
        caps.push_back({{geometry::path::toSk(at->position), dir, hit.distance,
                         len > 0 ? hit.distance / len : 0},
                        cache->corner.get()});
      }
    if (!closed && cache->start) {
      if (const auto at = contour.at(head * 0.5f))
        caps.push_back({{geometry::path::toSk(at->position),
                         geometry::path::toSk(at->tangent), 0, 0},
                        cache->start.get()});
    }
    if (!closed && cache->end) {
      if (const auto at = contour.at(len - tail * 0.5f))
        caps.push_back({{geometry::path::toSk(at->position),
                         geometry::path::toSk(at->tangent), len, 1},
                        cache->end.get()});
    }
  }

  for (const auto& [sample, sx] : sideSlots) {
    StampModifier m;
    if (modifier) m = modifier(sample, placed, sideSlots.size());
    drawStamp(c, *cache->side, sample, true, 0, sx, 1, m);
    ++placed;
  }
  for (const auto& [sample, picture] : caps)
    drawStamp(c, *picture, sample, true, 0, 1, 1, {});
}

}  // namespace sigil::compose::brush
