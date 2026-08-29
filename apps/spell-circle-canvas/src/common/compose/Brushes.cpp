#include "sigilcompose/Brushes.h"

#include <include/core/SkContourMeasure.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkStrokeRec.h>
#include <include/core/SkSurface.h>
#include <include/core/SkVertices.h>
#include <include/effects/SkCornerPathEffect.h>
#include <include/effects/SkDashPathEffect.h>
#include <include/effects/SkDiscretePathEffect.h>

#include <cmath>
#include <limits>

#include "sigilgeometry/Contour.h"
#include "sigilgeometry/Noise.h"
#include "sigilgeometry/Skia.h"

namespace sigil::compose {

void LayeredBrush::paint(SkCanvas& c, const PaintContext& ctx) const {
  for (const StrokeLayer& layer : layers) {
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(layer.width);
    p.setStrokeCap(layer.roundCap ? SkPaint::kRound_Cap : SkPaint::kButt_Cap);
    p.setColor4f(layer.color, nullptr);
    p.setBlendMode(layer.blend);
    if (layer.blurSigma > 0)
      p.setMaskFilter(
          SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, layer.blurSigma));
    if (!layer.dash.empty())
      p.setPathEffect(SkDashPathEffect::Make(
          SkSpan(layer.dash.data(), layer.dash.size()), layer.dashPhase));
    c.drawPath(ctx.outline, p);
  }
}

namespace ops {

PathOp debug(const char* tag) {
  std::string t = tag;
  return [t](const SkPath& p) {
    const SkRect b = p.getBounds();
    SkDebugf("[ops::debug %s] bounds (%.1f,%.1f %.1fx%.1f)\n", t.c_str(),
             b.left(), b.top(), b.width(), b.height());
    SkContourMeasureIter iter(p, false);
    int i = 0;
    while (sk_sp<SkContourMeasure> c = iter.next())
      SkDebugf("  contour %d: len %.1f %s\n", i++, c->length(),
               c->isClosed() ? "CLOSED" : "open");
    return p;
  };
}

PathOp chain(std::vector<PathOp> steps) {
  return [steps = std::move(steps)](const SkPath& p) {
    SkPath r = p;
    for (const PathOp& op : steps)
      if (op) r = op(r);
    return r;
  };
}

}  // namespace ops

GeometryOp::GeometryOp(Shaper s) : m_bleed(s.bleed()) {
  m_held = s;
  m_equals = [](const std::any& a, const std::any& b) {
    return std::any_cast<const Shaper&>(a) == std::any_cast<const Shaper&>(b);
  };
  m_apply = [held = std::move(s)](const SkPath& p) { return held.shape(p); };
}

namespace brush {

Solid solid(float width, Fill fill, PathFormat::Align align) {
  Solid s;
  s.width = width;
  s.strokeFill = std::move(fill);
  s.align = align;
  return s;
}

void Weave::paint(SkCanvas& c, const PaintContext& ctx) const {
  if (strands.empty()) return;
  // 1. Resolve every strand's geometry. A relative strand is a
  //    displacement of the boundary in the (along, across) frame the
  //    band owns; an absolute one brings its own path — and if NO
  //    strand is relative, the boundary is simply an unpainted host.
  std::vector<SkPath> paths;
  paths.reserve(strands.size());
  for (const Strand& s : strands) {
    switch (s.path.source()) {
      case StrandPath::Source::Relative:
        paths.push_back(s.path.profile().max() == 0.0f
                            ? ctx.outline
                            : profileOffset(ctx.outline, s.path.profile()));
        break;
      case StrandPath::Source::Borrowed:
        paths.push_back(ctx.borrowedPath(s.path.key()));
        break;
      case StrandPath::Source::Authored:
        paths.push_back(s.path.path());
        break;
    }
  }

  const auto paintStrand = [&](size_t i) {
    const PaintContext sub{ctx.size,         paths[i],      ctx.elapsedSeconds,
                           ctx.contentScale, ctx.animating, ctx.fonts,
                           ctx.borrowed};
    strands[i].brush.paint(c, sub);
  };

  // 2. List order first — the whole picture, correct wherever nothing
  //    crosses, which is every layers() and most of any weave.
  for (size_t i = 0; i < paths.size(); ++i) paintStrand(i);

  // 3. Repair the crossings the rule disagrees with. Crossings are
  //    DISCOVERED, never authored, and memoized on the resolved paths:
  //    a frame whose geometry did not move pays one vector-of-paths
  //    comparison instead of the flatten-and-test. The cache changes
  //    WHEN discovery runs and never what is drawn.
  if (!crossingCache->valid || crossingCache->key != paths) {
    crossingCache->found = discoverCrossings(paths);
    crossingCache->key = paths;
    crossingCache->valid = true;
    ++crossingCache->computes;
  }
  const std::vector<Crossing>& crossings = crossingCache->found;
  if (crossings.empty()) return;
  const auto reachOf = [&](size_t i) {
    // The MARK's full width, not the cull's bleed(): an Align::Inner
    // stroke bleeds zero while painting a mark `width` wide, so a region
    // built from bleed() would be too small to cover its own crossing.
    return patch > 0 ? patch : std::max(strands[i].brush.reach(), 1.0f);
  };

  // Each strand's arc length, so a crossing's `along` fractions convert to
  // px — which is what bounds one knot's patch away from its neighbours'.
  // Also whether the strand is a CYCLE, because on a closed contour the
  // fractions 0.02 and 0.98 are neighbours, not opposites.
  std::vector<float> lengths(paths.size(), 0.0f);
  std::vector<char> cyclic(paths.size(), 0);
  for (size_t i = 0; i < paths.size(); ++i) {
    SkContourMeasureIter it(paths[i], false);
    int contours = 0;
    bool lastClosed = false;
    while (sk_sp<SkContourMeasure> m = it.next()) {
      lengths[i] += m->length();
      lastClosed = m->isClosed();
      ++contours;
    }
    // ONE closed contour, and no more: with several contours the `along`
    // parameter runs them end to end, so its two ends are not adjacent
    // and wrapping would be a lie.
    cyclic[i] = (contours == 1 && lastClosed) ? 1 : 0;
  }
  // THE KNOT'S TERRITORY: half the arc distance to the nearest adjacent
  // crossing, on either strand, whichever is closer. Without it the
  // lenses of an ordinary braid touch, pathops merges them into ONE
  // contour, and crossing 0's patch owns the whole run — the weave then
  // reads as a single strand laid on top of the others.
  const auto positionOn = [](const Crossing& x, size_t strandIndex) {
    return x.a == strandIndex ? x.alongA : x.alongB;
  };
  const auto territoryOf = [&](const Crossing& x) {
    float limit = std::numeric_limits<float>::max();
    for (const size_t s : {x.a, x.b}) {
      const float mine = positionOn(x, s);
      for (const Crossing& other : crossings) {
        if (&other == &x || (other.a != s && other.b != s)) continue;
        float delta = std::abs(positionOn(other, s) - mine);
        // On a CYCLE the seam is not a boundary: two knots at 0.02 and
        // 0.98 sit 4% apart, not 96%. Without the wrap, crossings
        // straddling the seam read as maximally distant, their bound
        // vanishes, and their lenses merge — on two overlapping rings
        // that puts both knots in one patch painted in one colour.
        //
        // Conditional on closedness, because wrapping an OPEN strand
        // whose crossings sit near its two ends would over-clip: those
        // ends really are far apart.
        if (cyclic[s]) delta = std::min(delta, 1.0f - delta);
        const float gap = delta * lengths[s];
        if (gap > 0.01f) limit = std::min(limit, gap * 0.5f);
      }
    }
    // No neighbour on either strand: the lens needs no bound, and a
    // number large enough to contain it is the honest spelling of that.
    if (limit == std::numeric_limits<float>::max()) limit = 1e6f;
    return limit;
  };

  for (const Crossing& x : crossings) {
    const Order order = crossing.decide(x);
    const size_t top = order == Order::Over ? x.a : x.b;
    // `b` painted later, so it is already on top. Nothing to do.
    if (top == x.b) continue;
    c.save();
    c.clipPath(crossingPatch(paths[x.a], reachOf(x.a), paths[x.b], reachOf(x.b),
                             x.at, territoryOf(x)),
               true);
    paintStrand(top);
    c.restore();
  }
}

Weave layers(std::vector<Decoration> stack) {
  Weave w;
  w.strands.reserve(stack.size());
  for (Decoration& d : stack)
    w.strands.push_back(Strand{strand::self(), std::move(d)});
  return w;
}

Weave weave(std::vector<Strand> strands, CrossingRule rule) {
  Weave w;
  w.strands = std::move(strands);
  w.crossing = std::move(rule);
  return w;
}

}  // namespace brush

void Brush::paint(SkCanvas& c, const PaintContext& ctx) const {
  SkPath styled = ctx.outline;
  for (const Shaper& g : pipeline) styled = g.shape(styled);
  for (const Layer& l : layers) {
    SkPath layerPath = styled;
    for (const Shaper& g : l.shapers) layerPath = g.shape(layerPath);
    const PaintContext restyled{ctx.size,           std::move(layerPath),
                                ctx.elapsedSeconds, ctx.contentScale,
                                ctx.animating,      ctx.fonts,
                                ctx.borrowed};
    l.dec.paint(c, restyled);
  }
}

namespace brush {

void Restyled::paint(SkCanvas& c, const PaintContext& ctx) const {
  // No null check: GeometryOp::apply passes the path through unchanged
  // when it holds nothing.
  PaintContext restyled{ctx.size,           op.apply(ctx.outline),
                        ctx.elapsedSeconds, ctx.contentScale,
                        ctx.animating,      ctx.fonts,
                        ctx.borrowed};
  inner.paint(c, restyled);
}

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
        contours.back().push_back(pts[2]);
        break;
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

void drawStamp(SkCanvas& c, const SkPicture& pic, const PathSample& sample,
               bool align, float rotateDeg, float scaleX, float scaleY,
               const StampMod& m) {
  if (m.skip || m.alpha <= 0.003f || m.scale <= 0.001f) return;
  const SkRect cull = pic.cullRect();
  c.save();
  c.translate(sample.position.x(), sample.position.y());
  if (align)
    c.rotate(std::atan2(sample.tangent.y(), sample.tangent.x()) * 57.29578f);
  c.translate(m.dAlong, m.dNormal);  // tangent frame (post-align)
  c.rotate(rotateDeg + m.rotateDeg);
  c.scale(scaleX * m.scale, scaleY * m.scale);
  c.translate(-cull.width() / 2, -cull.height() / 2);
  if (m.alpha < 1.0f) {
    SkPaint fade;
    fade.setAlphaf(m.alpha);
    c.drawPicture(&pic, nullptr, &fade);
  } else {
    c.drawPicture(&pic);
  }
  c.restore();
}

}  // namespace

void Scatter::paint(SkCanvas& c, const PaintContext& ctx) const {
  if (spacing <= 0 || !ctx.fonts) return;
  // Prefer the instance-side store, so a brush value rebuilt every
  // describe still finds its art's bake; the member cache is the
  // standalone-paint fallback.
  sk_sp<SkPicture> pic;
  if (ctx.stamps) {
    if (const StampCache::Entry* e = ctx.stamps->get(art.node())) pic = e->pic;
    if (!pic) {
      // Shell box: snapshot() sizes by the root's CHILDREN and ignores
      // the root's own dimensions.
      pic = snapshot(box().child(art), *ctx.fonts);
      ctx.stamps->put(art.node(), {pic, nullptr, {0, 0}});
    }
  } else {
    if (!cache->pic || cache->bakedFor != art.node().get()) {
      cache->pic = snapshot(box().child(art), *ctx.fonts);
      cache->bakedFor = art.node().get();
    }
    pic = cache->pic;
  }
  if (!pic) return;

  // An unset place.interval takes `spacing`, resolved here where the
  // spacing lives rather than by comparing against a sentinel value an
  // author could also have typed on purpose.
  std::vector<PathSample> samples =
      placementSamples(ctx.outline, place, spacing);
  for (size_t i = 0; i < samples.size(); ++i) {
    StampMod m;
    if (mod) m = mod(samples[i], i, samples.size());
    if (seed != 0) {
      const uint32_t k = (uint32_t)i;
      m.dAlong += geometry::noise::hash(seed, 4 * k) * jitterAlong;
      m.dNormal += geometry::noise::hash(seed, 4 * k + 1) * jitterNormal;
      m.scale *= 1.0f + geometry::noise::hash(seed, 4 * k + 2) * jitterScale;
      m.rotateDeg += geometry::noise::hash(seed, 4 * k + 3) * jitterRotateDeg;
    }
    drawStamp(c, *pic, samples[i], alignToPath, 0, 1, 1, m);
  }
}

void Pattern::paint(SkCanvas& c, const PaintContext& ctx) const {
  if (!ctx.fonts) return;
  auto node = [](const std::optional<Element>& e) -> const void* {
    return e ? e->node().get() : nullptr;
  };
  const void* sideNode = side.node().get();
  const void* startNode = node(start);
  const void* endNode = node(end);
  const void* cornerNode = corner ? corner->art.node().get() : nullptr;
  if (cache->bakedSide != sideNode || cache->bakedStart != startNode ||
      cache->bakedEnd != endNode || cache->bakedCorner != cornerNode) {
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
        slot = hit->pic;
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
  const float tileLen =
      advance > 0 ? advance : std::max(cache->side->cullRect().width(), 1.0f);

  size_t placed = 0;
  // Two passes: count side tiles first so mod sees the true total.
  std::vector<std::pair<PathSample, float>> sideSlots;  // sample + scaleX
  std::vector<std::pair<PathSample, const SkPicture*>> caps;

  for (const geometry::Contour& contour : geometry::Contour::of(ctx.outline)) {
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
    std::vector<geometry::Contour::Corner> corners;
    if (cache->corner)
      corners = sigil::compose::detail::cornersOrWarn(
          contour, cornerAngleDeg, tileLen * 0.5f,
          std::clamp(tileLen * 0.25f, 1.0f, 6.0f));

    // Open-contour caps reserve their slots at the ends.
    float head = 0, tail = 0;
    if (!closed && cache->start)
      head = advance > 0 ? advance : cache->start->cullRect().width();
    if (!closed && cache->end)
      tail = advance > 0 ? advance : cache->end->cullRect().width();

    // Runs between corners (and cap margins). Each corner RESERVES half
    // its own length at each end of its two adjacent runs, so the side
    // tiles butt against the corner art instead of running under it.
    const float cornerRoom =
        cache->corner ? (cornerLength > 0 ? cornerLength
                                          : cache->corner->cullRect().width())
                      : 0.0f;
    const float halfCorner = cornerRoom * 0.5f;
    std::vector<float> bounds{head};
    for (const geometry::Contour::Corner& hit : corners)
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
              {{geometry::toSk(at->position), geometry::toSk(at->tangent), d,
                len > 0 ? d / len : 0},
               sx});
      }
    }

    // Corner tiles sit on the bisector of the break, or on the outgoing
    // leg — both tangents came out of the detection, so nothing is
    // re-probed here. No diagnostic is needed either: the alignment is a
    // required constructor argument of CornerArt, so corner art with no
    // stated alignment cannot be described in the first place.
    if (cache->corner)
      for (const geometry::Contour::Corner& hit : corners) {
        const auto at = contour.at(hit.distance);
        if (!at) continue;
        SkVector dir{hit.in.x + hit.out.x, hit.in.y + hit.out.y};
        // A hairpin's legs cancel: in + out ≈ 0 and atan2(0,0) is a
        // silent zero rotation. Fall back to the outgoing leg.
        if (dir.length() < 1e-3f || corner->align == CornerAlign::Outgoing)
          dir = geometry::toSk(hit.out);
        caps.push_back({{geometry::toSk(at->position), dir, hit.distance,
                         len > 0 ? hit.distance / len : 0},
                        cache->corner.get()});
      }
    if (!closed && cache->start) {
      if (const auto at = contour.at(head * 0.5f))
        caps.push_back(
            {{geometry::toSk(at->position), geometry::toSk(at->tangent), 0, 0},
             cache->start.get()});
    }
    if (!closed && cache->end) {
      if (const auto at = contour.at(len - tail * 0.5f))
        caps.push_back({{geometry::toSk(at->position),
                         geometry::toSk(at->tangent), len, 1},
                        cache->end.get()});
    }
  }

  for (const auto& [sample, sx] : sideSlots) {
    StampMod m;
    if (mod) m = mod(sample, placed, sideSlots.size());
    drawStamp(c, *cache->side, sample, true, 0, sx, 1, m);
    ++placed;
  }
  for (const auto& [sample, pic] : caps)
    drawStamp(c, *pic, sample, true, 0, 1, 1, {});
}

void Ribbon::paint(SkCanvas& c, const PaintContext& ctx) const {
  SkPaint p;
  p.setAntiAlias(true);
  if (fill.kind == Fill::Kind::Color)
    p.setColor4f(fill.colorValue, nullptr);
  else if (fill.kind == Fill::Kind::Shader)
    p.setShader(fill.shaderValue);

  if (hasProfile()) {
    // One geometry with band(): the region between the two profile
    // rails, per contour, with proper joins.
    const SkPath region =
        bandRegion(ctx.outline, Across{width}, Formation::Centered);
    if (!region.isEmpty()) c.drawPath(region, p);
    return;
  }

  const float stride = std::max(step, 0.5f);
  SkContourMeasureIter iter(ctx.outline, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    std::vector<SkPoint> left, right;
    for (float d = 0;; d += stride) {
      const float at = std::min(d, len);
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(at, &pos, &tan)) break;
      const PathSample s{pos, tan, at, len > 0 ? at / len : 0};
      float w;
      if (nibAngleDeg >= 0) {
        const float a =
            std::atan2(tan.y(), tan.x()) - nibAngleDeg * 0.017453293f;
        w = widthStart *
            (nibContrast + (1 - nibContrast) * std::abs(std::sin(a)));
      } else {
        w = widthStart + (widthEnd - widthStart) * s.fraction;
      }
      const SkVector n{-tan.y(), tan.x()};
      left.push_back({pos.x() + n.x() * w / 2, pos.y() + n.y() * w / 2});
      right.push_back({pos.x() - n.x() * w / 2, pos.y() - n.y() * w / 2});
      if (at >= len) break;
    }
    if (left.size() < 2) continue;
    SkPathBuilder band;
    band.moveTo(left.front());
    for (size_t i = 1; i < left.size(); ++i) band.lineTo(left[i]);
    for (size_t i = right.size(); i-- > 0;) band.lineTo(right[i]);
    band.close();
    c.drawPath(band.detach(), p);
  }
}

Ribbon taper(float widthStart, float widthEnd, Fill fill) {
  Ribbon r;
  r.widthStart = widthStart;
  r.widthEnd = widthEnd;
  r.fill = std::move(fill);
  return r;
}

Ribbon calligraphic(float nibAngleDeg, float width, Fill fill, float contrast) {
  Ribbon r;
  r.widthStart = width;
  r.nibAngleDeg = nibAngleDeg;
  r.nibContrast = contrast;
  r.fill = std::move(fill);
  return r;
}

void Art::paint(SkCanvas& c, const PaintContext& ctx) const {
  if (!ctx.fonts) return;
  if (!cache->image || cache->bakedFor != art.node().get()) {
    cache->bakedFor = art.node().get();
    cache->image = nullptr;
    // Consult the instance-side store before doing any raster work.
    if (ctx.stamps) {
      if (const StampCache::Entry* hit = ctx.stamps->get(art.node());
          hit && hit->image) {
        cache->image = hit->image;
        cache->artSize = hit->artSize;
      }
    }
  }
  if (!cache->image) {
    // Shell box: snapshot() and measure() size by the root's CHILDREN
    // and ignore the root's own dimensions.
    const SkSize sz = measure(box().child(art), *ctx.fonts);
    if (sz.isEmpty()) return;
    sk_sp<SkPicture> pic = snapshot(box().child(art), *ctx.fonts);
    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        std::max(1, (int)std::ceil(sz.width() * 2.0f)),
        std::max(1, (int)std::ceil(sz.height() * 2.0f))));
    if (!pic || !surface) return;
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    surface->getCanvas()->scale(2.0f, 2.0f);
    surface->getCanvas()->drawPicture(pic);
    cache->image = surface->makeImageSnapshot();
    cache->artSize = sz;
    if (ctx.stamps && cache->image)
      ctx.stamps->put(art.node(), {nullptr, cache->image, cache->artSize});
  }
  if (!cache->image) return;

  const float texW = (float)cache->image->width();
  const float texH = (float)cache->image->height();
  const float half = 0.5f * (height > 0 ? height : cache->artSize.height());
  SkPaint p;
  p.setAntiAlias(true);
  p.setShader(
      cache->image->makeShader(SkTileMode::kClamp, SkTileMode::kClamp,
                               SkSamplingOptions(SkFilterMode::kLinear)));

  SkContourMeasureIter iter(ctx.outline, false);
  std::vector<SkPoint> positions, texs;
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float length = contour->length();
    if (length < 1.0f) continue;
    const int stations =
        std::max(2, (int)std::ceil(length / std::max(1.0f, stationPx)));
    positions.clear();
    texs.clear();
    positions.reserve((size_t)(stations + 1) * 2);
    texs.reserve((size_t)(stations + 1) * 2);
    for (int i = 0; i <= stations; ++i) {
      const float f = (float)i / (float)stations;
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(length * f, &pos, &tan)) continue;
      const SkVector normal{-tan.fY, tan.fX};
      positions.push_back(pos + normal * half);
      positions.push_back(pos - normal * half);
      texs.push_back({texW * f, 0.0f});
      texs.push_back({texW * f, texH});
    }
    if (positions.size() < 4) continue;
    c.drawVertices(SkVertices::MakeCopy(SkVertices::kTriangleStrip_VertexMode,
                                        (int)positions.size(), positions.data(),
                                        texs.data(), nullptr),
                   SkBlendMode::kModulate, p);
  }
}

Art artAlong(Element art, float height, float stationPx) {
  Art b;
  b.art = std::move(art);
  b.height = height;
  b.stationPx = stationPx;
  b.reach = std::max(32.0f, height);
  return b;
}

Ribbon ribbon(Profile width, Fill fill) {
  Ribbon r;
  r.width = std::move(width);
  r.fill = std::move(fill);
  return r;
}

}  // namespace brush

}  // namespace sigil::compose
