#pragma once

/** @file
 * SigilCompose decoration primitives — the extension over the kernel's
 * Decoration seam (see API.md "Primitives, not a zoo"). Concrete
 * treatments are data over three general primitives, each a thin value
 * struct over machinery Skia ships:
 *
 *  - PathFormat: format any stroke along the node's outline — width and
 *    paint plus an optional dash pattern, a stamped path repeated along
 *    the contour (vines, chains), or any custom SkPathEffect.
 *  - Slice: map an image onto the box through a lattice
 *    (drawImageLattice: N-patch; nine-slice is the 3×3 case).
 *  - ContourWalk: walk the outline by arc length and run a draw program
 *    at each sample, the canvas pre-positioned at the sample with x
 *    along the tangent — the general procedural border.
 *
 * All are DecorationScheme values: attach with .background()/.foreground().
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/Lines.h"    // insetOutline, cornerBrackets, cornerGaps
#include "sigilcompose/Material.h" // Wash — the material-valued decoration
#include "sigilcompose/GpuImage.h"

#include <sigilimage/ImageAsset.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkClipOp.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/effects/Sk1DPathEffect.h>
#include <include/effects/SkDashPathEffect.h>

#include <cmath>
#include <optional>

namespace sigil::compose {

/** Stroke the node's outline, formatted by data: dashes, a stamped
 *  path, or a custom effect (composable — dash of a stamp is legal in
 *  Skia by chaining effects yourself via `effect`). */
struct PathFormat {
  /** Where the stroke sits relative to the outline (the Photoshop/Figma
   *  stroke-position control). Center straddles it; Inner clips the
   *  stroke inside the shape (borders that never fatten the silhouette);
   *  Outer clips it outside (keylines around a filled shape). Inner and
   *  Outer are meaningful on CLOSED outlines — an open rail has no
   *  inside. */
  enum class Align : uint8_t { Center, Inner, Outer };

  float width = 1.0f;
  Fill strokeFill = Fill::color({1, 1, 1, 1});
  Align align = Align::Center;

  /** Dash on/off intervals in px (empty → solid). */
  std::vector<SkScalar> dashIntervals;
  /** Stroke cap and join on the paint itself. The card illustration of
   *  the Fallout 2 study is ~30 open contours of line art; every one of
   *  them ended square and mitred at the joints because this paint was
   *  built and never asked. Distinct from `lines::Line`'s join item,
   *  which is about the OFFSET CONTOUR rather than the stroke paint. */
  /** A Material for the stroke, superseding `strokeFill` when set.
   *
   *  `fill()` takes a Material — unit-square authoring, structural
   *  comparison, live uniforms — and a stroke took only the kernel
   *  `Fill`, which is node-local pixels compared by shader pointer. On an
   *  object whose surfaces are mostly STROKES (every bar, ring and
   *  engraved circle of an astrolabe) that meant writing the same brass
   *  twice, once per return type, with the world→node conversion done by
   *  hand in both. */
  std::optional<Material> strokeMaterial;
  SkPaint::Cap cap = SkPaint::kButt_Cap;
  SkPaint::Join join = SkPaint::kMiter_Join;
  float dashPhase = 0.0f;
  /** Bind the dash phase to a wrapping Output and the dashes MARCH —
   *  the commonest animated-line idiom in map and diagram UI (a selected
   *  route, a live link, a cut line). Like `trimPhase`, it supersedes the
   *  constant and declares the decoration animated, so the node repaints
   *  without a re-describe. A study wrote a 25-line DecorationScheme for
   *  want of this. */
  const choreograph::Output<float> *dashPhaseBinding = nullptr;

  /** Stamp this path repeatedly along the contour (advance px apart),
   *  rotated to follow it — vines, chains, ornament runs. */
  SkPath stampPath;
  float stampAdvance = 0.0f;

  /** Escape hatch: any SkPathEffect; overrides dash/stamp when set. */
  sk_sp<SkPathEffect> effect;

  /** Per-DECORATION trim window (fractions of arc length) — one node can
   *  carry a full static band AND a marching sliver as two strokes. Wraps
   *  like TrimMode::Wrap (seam-crossing windows stitch into one contour).
   *  Bind `trimPhase` to a wrapping Output and THIS stroke marches while
   *  its siblings hold still (declares the decoration animated).
   *
   *  IT COMPOSES WITH THE NODE'S `trim()`, which is the part people miss.
   *  A decoration receives the ALREADY-trimmed outline, so its own window
   *  is a fraction of the revealed part — `trimStart 0.9, trimEnd 1.0` on
   *  a second stroke is a bright sliver riding the head of a self-drawing
   *  line, and needs no second node:
   *
   *      PathFormat head = util::stroke(6, Fill::color(kBright));
   *      head.trimStart = 0.90f; head.trimEnd = 1.0f;
   *      box().outline(curve).trim(0, &growth)
   *           .foreground(util::stroke(3, Fill::color(kBody)))
   *           .foreground(head);
   *
   *  Spelled out because two studies concluded there was one trim window
   *  per NODE and each rebuilt this as a duplicate element re-measuring
   *  the same path. */
  float trimStart = 0.0f, trimEnd = 1.0f;
  float trimOffset = 0.0f;
  const choreograph::Output<float> *trimPhase = nullptr; // replaces offset

  /** Structural equality so a static stroked/dashed/stamped border prunes
   *  without memo (the custom SkPathEffect compares by pointer identity). */
  bool operator==(const PathFormat &) const = default;

  /** Stroke reach beyond the outline (recording cull grows by this). */
  float bleed() const {
    return align == Align::Inner ? 0.0f
           : align == Align::Outer ? width
                                   : width * 0.5f;
  }
  /** A bound trim phase, a bound dash phase, or a live stroke material
   *  repaints per frame (declared volatility). */
  bool animated() const {
    return trimPhase != nullptr || dashPhaseBinding != nullptr ||
           (strokeMaterial && strokeMaterial->isLive());
  }
  float phase() const {
    return dashPhaseBinding ? dashPhaseBinding->value() : dashPhase;
  }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    // Inner/Outer: clip to the shape's side and stroke DOUBLE width — the
    // visible half lands entirely on the kept side (the standard trick).
    const bool aligned = align != Align::Center;
    p.setStrokeWidth(aligned ? width * 2 : width);
    p.setStrokeCap(cap);
    p.setStrokeJoin(join);
    const Fill stroke =
        strokeMaterial ? strokeMaterial->resolve(ctx) : strokeFill;
    if (stroke.kind == Fill::Kind::Color)
      p.setColor4f(stroke.colorValue, nullptr);
    else if (stroke.kind == Fill::Kind::Shader)
      p.setShader(stroke.shaderValue);

    sk_sp<SkPathEffect> chosen = effect;
    if (!chosen && stampAdvance > 0 && !stampPath.isEmpty())
      chosen = SkPath1DPathEffect::Make(stampPath, stampAdvance, phase(),
                                        SkPath1DPathEffect::kRotate_Style);
    if (!chosen && !dashIntervals.empty())
      chosen = SkDashPathEffect::Make(
          SkSpan(dashIntervals.data(), dashIntervals.size()), phase());
    p.setPathEffect(std::move(chosen));

    // The decoration's own trim window (wrapping; the marching sliver).
    const SkPath *drawn = &ctx.outline;
    SkPath windowed;
    const float off = trimPhase ? trimPhase->value() : trimOffset;
    const float s0 = trimStart + off, e0 = trimEnd + off;
    const float span = e0 - s0;
    if (span > 0.0f && span < 1.0f) {
      const float s = s0 - std::floor(s0);
      const float e = e0 - std::floor(e0);
      SkPathBuilder window;
      SkContourMeasureIter iter(ctx.outline, false);
      while (sk_sp<SkContourMeasure> contour = iter.next()) {
        const float len = contour->length();
        if (s < e) {
          (void)contour->getSegment(s * len, e * len, &window, true);
        } else if (s > e) {
          (void)contour->getSegment(s * len, len, &window, true);
          // A closed contour has a real seam, so joining both pieces avoids
          // doubled caps there. An open route has no seam: continuing without
          // a moveTo would invent a straight chord from its end to its start.
          (void)contour->getSegment(0, e * len, &window,
                                    !contour->isClosed());
        }
      }
      windowed = window.detach();
      if (!windowed.isEmpty())
        drawn = &windowed;
    } else if (span <= 0.0f) {
      return; // empty window — nothing to stroke
    }

    if (aligned) {
      canvas.save();
      canvas.clipPath(ctx.outline, align == Align::Inner
                                       ? SkClipOp::kIntersect
                                       : SkClipOp::kDifference,
                      true);
      canvas.drawPath(*drawn, p);
      canvas.restore();
    } else {
      canvas.drawPath(*drawn, p);
    }
  }
};

/** Image-onto-box through a lattice (per-cell stretch); nine-slice is
 *  xDivs/yDivs of size 2. Empty divs stretch the whole image. */
struct Slice {
  std::shared_ptr<const sigil::image::ImageAsset> asset;
  std::vector<int> xDivs;
  std::vector<int> yDivs;
  /** drawImageLattice takes the image DIRECTLY — on Graphite a raster
   *  image must promote first or the lattice silently vanishes (the
   *  invisible-nine-slice finding). Excluded from equality. */
  std::shared_ptr<gpuimg::Promoted> gpuCache =
      std::make_shared<gpuimg::Promoted>();
  /** How the slices sample. Linear is right for a soft frame and wrong
   *  for pixel art, which is most of what nine-slice is FOR — a window
   *  chrome, a dialog border, a button from a tile sheet. Every blessed
   *  image path hardcoded linear until `Element::sampling()` landed on
   *  the image leaf; this is the same fix on the decoration. */
  SkFilterMode filter = SkFilterMode::kLinear;

  /** Structural equality (asset by pointer identity) so a static nine-slice
   *  frame prunes without memo. The promotion cache is identity-free. */
  bool operator==(const Slice &o) const {
    return asset == o.asset && xDivs == o.xDivs && yDivs == o.yDivs &&
           filter == o.filter;
  }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    if (!asset || asset->frames().empty())
      return;
    sk_sp<SkImage> img = asset->frames().front().image;
    if (!img)
      return;
    const SkRect dst = SkRect::MakeWH(ctx.size.width(), ctx.size.height());
    gpuimg::drawLattice(canvas, *gpuCache, std::move(img), xDivs, yDivs, dst,
                        filter);
  }
};

/** One arc-length sample along the outline. */
struct PathSample {
  SkPoint position;
  SkVector tangent;
  float distance = 0.0f;
  float fraction = 0.0f; // 0..1 within its contour
};

/** Walk the outline at `spacing` px intervals; at every sample the
 *  canvas is translated to the sample and rotated so +x follows the
 *  tangent. The general procedural border. Two bodies, composable:
 *
 *  - `draw`: a raw program per sample (per-step images, SkSL, nested
 *    drawing). Set `animatedWalk` when it depends on
 *    ctx.elapsedSeconds (declared volatility).
 *  - `stamp`: a full element subtree — laid out and recorded ONCE via
 *    snapshot() (at intrinsic size, its own decorations and all), then
 *    replayed per sample centered on the contour. Recursion is closed:
 *    the stamp's decorations may walk their own contours. With
 *    `animatedWalk` the stamp re-records each paint, sampling any
 *    bound ch::Outputs at their current values.
 *
 *  When both are set, `stamp` replays first, then `draw` runs on top. */
struct ContourWalk {
  float spacing = 16.0f;
  std::function<void(SkCanvas &, const PathSample &, const PaintContext &)>
      draw;
  bool animatedWalk = false;

  std::optional<Element> stamp;

  bool animated() const { return animatedWalk; }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    if ((!draw && !stamp) || spacing <= 0)
      return;

    // Bake (or re-bake) the stamp element: once per description for
    // static stamps, once per paint for animated ones.
    const void *stampNode = stamp ? stamp->node().get() : nullptr;
    if (stampCache->bakedFor != stampNode) {
      stampCache->picture.reset();
      stampCache->bakedFor = stampNode;
    }
    if (stamp && ctx.fonts && (!stampCache->picture || animatedWalk))
      stampCache->picture = snapshot(*stamp, *ctx.fonts);
    const sk_sp<SkPicture> &stampPicture = stampCache->picture;

    SkContourMeasureIter iter(ctx.outline, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float length = contour->length();
      for (float d = 0; d < length; d += spacing) {
        SkPoint pos;
        SkVector tan;
        if (!contour->getPosTan(d, &pos, &tan))
          continue;
        PathSample sample{pos, tan, d, length > 0 ? d / length : 0};
        canvas.save();
        canvas.translate(pos.x(), pos.y());
        canvas.rotate(std::atan2(tan.y(), tan.x()) * 180.0f / 3.14159265f);
        if (stampPicture) {
          const SkRect cull = stampPicture->cullRect();
          canvas.save();
          canvas.translate(-cull.width() / 2, -cull.height() / 2);
          canvas.drawPicture(stampPicture);
          canvas.restore();
        }
        if (draw)
          draw(canvas, sample, ctx);
        canvas.restore();
      }
    }
  }

  /** @private Replay cache, shared across the by-value copies
   *  Decoration makes; paint() is const, the bake is memoization.
   *  (Public to keep ContourWalk an aggregate for designated init.) */
  struct StampCache {
    sk_sp<SkPicture> picture;
    const void *bakedFor = nullptr;
  };
  std::shared_ptr<StampCache> stampCache = std::make_shared<StampCache>();
};

/** Paints a decoration against geometry you built yourself, inside a
 *  `custom()` program.
 *
 *  The whole brush vocabulary — `PathFormat` with its stroke alignment,
 *  dashes, stamps and its own trim window; `lines::Line`; `Brush`;
 *  `shapes::inset` — reads only `PaintContext::outline`. So none of it
 *  is actually restricted to a node's own shape, and geometry that
 *  changes per frame (a simulated rope, a live EQ curve, a plotted
 *  signal) can wear all of it:
 *
 *      custom([&](SkCanvas &c, const PaintContext &ctx) {
 *        decorations::paintOn(c, ctx, ropePath(), lines::cased(...));
 *      }).cache(Cache::None)
 *
 *  Spelled out because the roadmap recorded the opposite — that live
 *  geometry in `custom()` forfeits the decoration vocabulary along with
 *  pruning — and a researcher caught it by reading the source rather than
 *  the list. It was a discoverability gap wearing a capability gap's
 *  clothes, which is the fourth of those this program has found. */
/** Floods the node's OUTLINE with a Material through a blend mode — the
 *  material-valued decoration the primitive set was missing.
 *
 *  `Element::overlay()` puts a layer over the fill and under the
 *  children; `foreground()` puts one over everything. But `foreground()`
 *  takes a `Decoration`, and the primitives are `PathFormat` (strokes),
 *  `Slice`, `ContourWalk` and a raw `PaintProgram` — none of which fills
 *  a shape with a Material. So "a grain pass over this whole panel,
 *  above its children" had to be a raw PaintProgram, which is an
 *  incomparable `std::function` and therefore never prunes.
 *
 *  `Wash` is a comparable VALUE (Material compares structurally by
 *  recipe), so a static wash prunes like any other decoration, and it
 *  declares itself animated when the material is live.
 *
 *      .foreground(decorations::wash(patterns::grain(0.3f, 2, 7.0f),
 *                                    SkBlendMode::kSoftLight, 0.35f))
 */
struct Wash {
  Material material;
  SkBlendMode blend = SkBlendMode::kSrcOver;
  /** Strength, 0..1 — applied as alpha on the pass. The `amount` a
   *  `Material::blend` layer still does not have (ROADMAP.md §5), here
   *  at least for the decoration form. */
  float amount = 1.0f;

  bool operator==(const Wash &o) const {
    return material == o.material && blend == o.blend && amount == o.amount;
  }
  bool animated() const { return material.isLive(); }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    const float a = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
    if (a <= 0.0f)
      return;
    const Fill fill = material.resolve(ctx);
    SkPaint p;
    p.setAntiAlias(true);
    p.setBlendMode(blend);
    if (fill.kind == Fill::Kind::Color) {
      SkColor4f c = fill.colorValue;
      c.fA *= a;
      p.setColor4f(c, nullptr);
    } else if (fill.kind == Fill::Kind::Shader) {
      p.setShader(fill.shaderValue);
      p.setAlphaf(a);
    } else {
      return;
    }
    canvas.drawPath(ctx.outline, p);
  }
};

/** THE BORDER — a frame is not a 1 px rounded rect.
 *
 *  Every study in the corpus that wanted a frame either stroked the box and
 *  accepted a rounded rect, or hand-placed four Elements at the corners.
 *  The second is worse than it looks: four absolutely-positioned nodes do
 *  not follow the shape when it resizes, do not follow it AT ALL when the
 *  shape is not a rectangle, and cost four nodes each.
 *
 *  `Border` is one comparable value over machinery that already existed —
 *  `lines::insetOutline` (the offset `shapes::Inset` had locked inside a
 *  decoration adaptor), `lines::cornerBrackets` / `lines::cornerGaps` (the
 *  corner scan `PatternBrush` already ran), and an ordinary dashed stroke:
 *
 *      .foreground(decorations::brackets(2, ink, 18))     // reticle corners
 *
 *  Each corner helper takes a trailing `angleDeg`. It is the last
 *  parameter because it is usually the default — but WITHOUT it the
 *  helpers could not reach `Border::cornerAngleDeg` at all, so an n-gon
 *  bezel (18 degrees per vertex on a 20-gon, under the 30 default) had no
 *  spelling short of building the Border by hand.
 *      .foreground(decorations::border(1, ink, 6))        // inset rule
 *      .foreground(decorations::gappedRule(1, ink, 14))   // open corners
 *
 *  It follows any silhouette, so chamfering the node's outline puts the
 *  brackets on the chamfers with no further instruction. */
struct Border {
  float width = 1.0f;
  Fill fill = Fill::color({1, 1, 1, 1});
  /** px INSIDE the node's outline; negative moves it outside. The second
   *  border of a double frame is the same value with a different inset. */
  float inset = 0.0f;

  /** What the rule does at the corners. */
  enum class Mode : uint8_t {
    Continuous, ///< the whole outline — an ordinary stroke
    Bracket,    ///< ONLY within `corner` px of each corner: the four L's
    Gapped,     ///< everything EXCEPT within `corner` px: the open corner
    Weighted,   ///< continuous, but `cornerWidth` near each corner
  };
  Mode mode = Mode::Continuous;
  /** Arm length (Bracket), omission (Gapped), or the weighted run
   *  (Weighted), in px of arc length either side of the corner. */
  float corner = 18.0f;
  /** Weighted mode only: the width used near the corners. */
  float cornerWidth = 0.0f;
  /** The tangent break that counts as a corner. A gently ROUNDED corner
   *  has no hard break and therefore no corner — brackets vanish and a
   *  gapped rule runs continuous. That is correct, and it is the first
   *  thing that surprises people.
   *
   *  **A regular n-gon turns 360/n degrees per vertex, so this default
   *  finds nothing above 12 sides.** A 20-gon turns 18° and renders blank;
   *  a dodecagon turns exactly 30° and is on the boundary. That is not
   *  exotic — an n-gon bezel is the natural frame for a die face or a
   *  rosette, which is what this vocabulary was added to serve. Pass
   *  roughly 0.6 × the turn angle (12° for a 20-gon).
   *
   *  The default is deliberately NOT adaptive. The scan steps 2 px, so an
   *  arc of radius r turns ~114/r degrees per sample — about 11° at
   *  r = 10 — and any threshold low enough to catch a 20-gon's vertices
   *  would shatter a small rounded corner into a run of false ones. The
   *  number stays yours; when a scan finds nothing the library prints the
   *  sharpest break it did see and what to pass. */
  float cornerAngleDeg = 30.0f;

  std::vector<SkScalar> dash;
  float dashPhase = 0.0f;
  const choreograph::Output<float> *dashPhaseBinding = nullptr;
  SkPaint::Cap cap = SkPaint::kButt_Cap;
  SkPaint::Join join = SkPaint::kMiter_Join;

  bool operator==(const Border &) const = default;
  bool animated() const { return dashPhaseBinding != nullptr; }
  float phase() const {
    return dashPhaseBinding ? dashPhaseBinding->value() : dashPhase;
  }
  float bleed() const {
    const float heaviest = std::max(width, cornerWidth);
    return std::max(0.0f, heaviest * 0.5f - inset);
  }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    // `width` is the RUN's width, and Weighted mode has a second width for
    // the corners — so width == 0 is not "draw nothing", it is "corners
    // only, no runs between them", which is a real frame and one of the
    // things the corner vocabulary exists for. Bailing on width <= 0 made
    // weightedCorners(0, w, …) silently draw nothing at all.
    const float heaviest =
        mode == Mode::Weighted ? std::max(width, cornerWidth) : width;
    if (ctx.outline.isEmpty() || heaviest <= 0)
      return;
    const SkPath base =
        inset != 0 ? lines::insetOutline(ctx.outline, inset) : ctx.outline;

    auto strokeWith = [&](const SkPath &path, float w) {
      if (path.isEmpty() || w <= 0)
        return;
      SkPaint p;
      p.setAntiAlias(true);
      p.setStyle(SkPaint::kStroke_Style);
      p.setStrokeWidth(w);
      p.setStrokeCap(cap);
      p.setStrokeJoin(join);
      if (fill.kind == Fill::Kind::Color)
        p.setColor4f(fill.colorValue, nullptr);
      else if (fill.kind == Fill::Kind::Shader)
        p.setShader(fill.shaderValue);
      if (!dash.empty())
        p.setPathEffect(
            SkDashPathEffect::Make(SkSpan(dash.data(), dash.size()), phase()));
      canvas.drawPath(path, p);
    };

    switch (mode) {
    case Mode::Continuous:
      strokeWith(base, width);
      break;
    case Mode::Bracket:
      strokeWith(lines::cornerBrackets(base, corner, cornerAngleDeg), width);
      break;
    case Mode::Gapped:
      strokeWith(lines::cornerGaps(base, corner, cornerAngleDeg), width);
      break;
    case Mode::Weighted:
      // Two passes of the same windows: the runs BETWEEN corners at
      // `width`, the corners themselves at `cornerWidth`. The printed rule
      // that thickens where it turns.
      strokeWith(lines::cornerGaps(base, corner, cornerAngleDeg), width);
      strokeWith(lines::cornerBrackets(base, corner, cornerAngleDeg),
                 cornerWidth > 0 ? cornerWidth : width);
      break;
    }
  }
};

namespace decorations {
inline Wash wash(Material material, SkBlendMode blend = SkBlendMode::kSrcOver,
                 float amount = 1.0f) {
  return Wash{std::move(material), blend, amount};
}

/** A plain rule around the node's outline, `inset` px inside it. */
inline Border border(float width, Fill fill, float inset = 0.0f) {
  return Border{.width = width, .fill = std::move(fill), .inset = inset};
}

/** CORNER BRACKETS: four L-shaped marks and nothing else — the reticle,
 *  the selection handle, the crop mark, the target frame. `arm` is the
 *  length of each leg in px of arc length. */
inline Border brackets(float width, Fill fill, float arm = 18.0f,
                       float inset = 0.0f, float angleDeg = 30.0f) {
  return Border{.width = width,
                .fill = std::move(fill),
                .inset = inset,
                .mode = Border::Mode::Bracket,
                .corner = arm,
                .cornerAngleDeg = angleDeg};
}

/** A rule that STOPS SHORT of every corner, leaving `gap` px of paper. */
inline Border gappedRule(float width, Fill fill, float gap = 14.0f,
                         float inset = 0.0f, float angleDeg = 30.0f) {
  return Border{.width = width,
                .fill = std::move(fill),
                .inset = inset,
                .mode = Border::Mode::Gapped,
                .corner = gap,
                .cornerAngleDeg = angleDeg};
}

/** A border whose WEIGHT changes at the corner: `width` along the runs,
 *  `cornerWidth` within `arm` px of each turn. */
inline Border weightedCorners(float width, float cornerWidth, Fill fill,
                              float arm = 18.0f, float inset = 0.0f,
                              float angleDeg = 30.0f) {
  return Border{.width = width,
                .fill = std::move(fill),
                .inset = inset,
                .mode = Border::Mode::Weighted,
                .corner = arm,
                .cornerWidth = cornerWidth,
                .cornerAngleDeg = angleDeg};
}

/** DOUBLE BORDER with independent insets — two rules, one value, as a
 *  LayerStyle (the shape `lines::railwayCarto` already established). The
 *  inner rule is often the dotted or lighter one; pass whatever you like.
 *
 *      .style(decorations::doubleBorder(
 *          decorations::border(1.6f, ink),
 *          decorations::border(0.8f, ink, 6)))
 */
inline LayerStyle doubleBorder(Border outer, Border inner) {
  return LayerStyle{{}, {Decoration(std::move(outer)),
                         Decoration(std::move(inner))}};
}

inline void paintOn(SkCanvas &canvas, const PaintContext &ctx, SkPath outline,
                    const Decoration &decoration) {
  PaintContext local = ctx;
  local.outline = std::move(outline);
  decoration.paint(canvas, local);
}
} // namespace decorations

} // namespace sigil::compose
