/** @file
 * THE LEAVES A PAINT IS BUILT FROM: the solid, the raw shader wrap, the
 * material instance, the three gradients with the unit-square ramp and
 * the box read their box units compile to, the image and the caller-owned pixel
 * buffer behind it, and the SkSL runtime effect. Each factory mints the shader
 * it can resolve eagerly and the comparable recipe two independently built
 * paints are compared by.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkGradient.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilshaders/MaterialSkia.h>

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "PaintInternal.h"

namespace sigil::material::skia {

namespace {

struct RampArrays {
  std::vector<SkColor4f> colors;  // the shader builder's own colour
  std::vector<float> positions;   // empty → evenly spaced
};

/** The stops as the shader builder takes them. Evenly spaced stops hand
 *  over no offsets, so Skia spaces them itself exactly as it always has. */
RampArrays split(const ColorStops& stops) {
  RampArrays r;
  r.colors.reserve(stops.size());
  for (const ColorStop& s : stops.stops())
    r.colors.push_back(toSkColor(s.color));
  if (stops.evenlySpaced()) return r;
  r.positions.reserve(stops.size());
  for (const ColorStop& s : stops.stops()) r.positions.push_back(s.offset);
  return r;
}

/** What a radius of 1 is in the unit square, for a box-unit radial. */
float extentOf(RadialExtent extent) {
  return extent == RadialExtent::ClosestSide ? 0.5f : 0.70710678f;
}

// SkGradient's color/position spans are non-owning — keep `r` alive across the
// SkShaders::*Gradient call (the shader copies the stops during construction).
SkGradient makeGradient(const RampArrays& r, SkTileMode tile) {
  return SkGradient({{r.colors.data(), r.colors.size()},
                     {r.positions.data(), r.positions.size()},
                     tile},
                    {});
}

}  // namespace

}  // namespace sigil::material::skia

namespace sigil::material {

using skia::PaintAccess;

Paint Paint::solid(Color color) {
  Paint m;
  m.m_isSolid = true;
  m.m_solid = color;
  return m;
}

Paint Paint::recipe(Material material) {
  // A material built up from a base and layers is folded into one paint;
  // a bare program is held as the instance it is.
  if (material.isComposed()) return skia::paint(material);
  Paint paint = PaintAccess::unresolvedRecipe(std::move(material));
  paint.m_shader = PaintAccess::hold(PaintAccess::buildBacked(paint, nullptr));
  return paint;
}

}  // namespace sigil::material

namespace sigil::material::skia {

Paint PaintAccess::unresolvedRecipe(Material material) {
  Paint paint;
  paint.m_worldSpace = std::as_const(material).worldSpace();
  paint.m_backed = std::make_shared<Paint::Backed>(std::move(material));
  return paint;
}

namespace {

/** The same options with the points read as px: what a box-unit gradient
 *  builds its unit-square child with before the box scales it. */
GradientOptions inPixels(GradientOptions options) {
  options.units = GradientUnits::Pixels;
  return options;
}

/** HOW A BOX GRADIENT READS ITS CHILD: the modes the box read's program
 *  names, and the numbers each one takes. */
struct BoxPlacement {
  enum Mode : int { UnitSquare, PixelsAroundCenter, Along, Ellipse, Circle };
  Mode mode = UnitSquare;
  SkPoint center = {0, 0};
  SkVector direction = {0, 0};
  SkVector radii = {0, 0};
  float extent = 0;
};

/** A BOX-UNIT GRADIENT that the unit-square ramp cannot draw — a repeat, a
 *  focus, a conic, a CSS-shaped gradient: the gradient laid out away from
 *  the box as a child, read at the painted point's place in the box as
 *  @p placement says. It rides the GEOMETRY tier through uResolution. A
 *  conic is read in px around its centre, so its angles stay true on a
 *  box that is not square. */
Paint boxGradient(Paint unitGradient, const BoxPlacement& placement) {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [built, error] = SkRuntimeEffect::MakeForShader(
        SkString(std::string(shaderSource("BoxGradient.sksl")).c_str()));
    if (!built)
      SkDebugf("sigilmaterial box gradient shader: %s\n", error.c_str());
    return built;
  }();
  if (!effect) return unitGradient;
  Paint box = PaintAccess::sksl(
      effect, {{"uMode", (float)placement.mode}, {"uExtent", placement.extent}});
  box.set("uCenter",
          std::array<float, 2>{placement.center.x(), placement.center.y()});
  box.set("uDirection", std::array<float, 2>{placement.direction.x(),
                                             placement.direction.y()});
  box.set("uRadii",
          std::array<float, 2>{placement.radii.x(), placement.radii.y()});
  box.slot("uGradient", std::move(unitGradient));
  return box;
}

Paint boxGradient(Paint unitGradient, SkPoint center, bool conic) {
  return boxGradient(
      std::move(unitGradient),
      {.mode = conic ? BoxPlacement::PixelsAroundCenter
                     : BoxPlacement::UnitSquare,
       .center = center});
}

/** The point-to-point options a CSS-shaped gradient's child is built
 *  with: in px, with the repeat and the conic window it was given. */
GradientOptions childOptions(const GradientOptions& options) {
  GradientOptions child;
  child.units = GradientUnits::Pixels;
  child.repeat = options.repeat;
  child.startDegrees = options.startDegrees;
  child.endDegrees = options.endDegrees;
  return child;
}

/** Where a gradient toward @p toward starts and ends in the unit square.
 *  Toward a corner, the line runs between the two opposite corners, so
 *  the stops' middle is the other diagonal at any proportions. */
std::pair<glm::vec2, glm::vec2> towardPoints(Toward toward) {
  switch (toward) {
    case Toward::Top:
      return {{0.5f, 1}, {0.5f, 0}};
    case Toward::TopRight:
      return {{0, 1}, {1, 0}};
    case Toward::Right:
      return {{0, 0.5f}, {1, 0.5f}};
    case Toward::BottomRight:
      return {{0, 0}, {1, 1}};
    case Toward::Bottom:
      return {{0.5f, 0}, {0.5f, 1}};
    case Toward::BottomLeft:
      return {{1, 0}, {0, 1}};
    case Toward::Left:
      return {{1, 0.5f}, {0, 0.5f}};
    case Toward::TopLeft:
      return {{1, 1}, {0, 0}};
  }
  return {{0.5f, 0}, {0.5f, 1}};
}

}  // namespace

}  // namespace sigil::material::skia

namespace sigil::material {

Paint Paint::linearGradient(glm::vec2 startPoint, glm::vec2 endPoint,
                            ColorStops stops, GradientOptions options) {
  using namespace skia;
  const SkPoint start{startPoint.x, startPoint.y};
  const SkPoint end{endPoint.x, endPoint.y};
  if (options.units == GradientUnits::Box) {
    if (options.repeat == Repeat::Pad)
      return skia::detail::unitRamp(start, end, stops.stops(), false);
    return boxGradient(linearGradient(startPoint, endPoint, std::move(stops),
                                      inPixels(options)),
                       {0, 0}, false);
  }
  const SkTileMode tile = toSkTileMode(options.repeat);
  RampArrays arrays = split(stops);
  const SkPoint pts[2] = {start, end};
  Paint m = PaintAccess::wrap(
      SkShaders::LinearGradient(pts, makeGradient(arrays, tile)));
  auto rec = std::make_shared<Recipe>();
  rec->kind = Recipe::Kind::Linear;
  rec->p0 = start;
  rec->p1 = end;
  rec->stops = std::move(stops);
  rec->tile = tile;
  m.m_recipe = std::move(rec);
  return m;
}

Paint Paint::radialGradient(glm::vec2 centerPoint, float radius,
                            ColorStops stops, GradientOptions options) {
  using namespace skia;
  const SkPoint center{centerPoint.x, centerPoint.y};
  if (options.units == GradientUnits::Box) {
    if (!options.focus && options.repeat == Repeat::Pad) {
      // The unit-square ramp reads its radius against the half-diagonal,
      // so the closest side is that radius over the square root of two.
      const float scaled = options.extent == RadialExtent::ClosestSide
                               ? radius * 0.70710678f
                               : radius;
      return skia::detail::unitRamp(center, {scaled, scaled}, stops.stops(),
                                    true);
    }
    const float unit = extentOf(options.extent);
    GradientOptions unitOptions = inPixels(options);
    unitOptions.focusRadius *= unit;
    return boxGradient(radialGradient(centerPoint, radius * unit,
                                      std::move(stops), unitOptions),
                       {0, 0}, false);
  }
  const SkTileMode tile = toSkTileMode(options.repeat);
  RampArrays r = split(stops);
  auto rec = std::make_shared<Recipe>();
  Paint m;
  if (options.focus) {
    const SkPoint focus{options.focus->x, options.focus->y};
    m = PaintAccess::wrap(SkShaders::TwoPointConicalGradient(
        focus, options.focusRadius, center, radius, makeGradient(r, tile)));
    rec->kind = Recipe::Kind::Conical;
    rec->p0 = focus;
    rec->p1 = center;
    rec->f0 = options.focusRadius;
    rec->f1 = radius;
  } else {
    m = PaintAccess::wrap(
        SkShaders::RadialGradient(center, radius, makeGradient(r, tile)));
    rec->kind = Recipe::Kind::Radial;
    rec->p0 = center;
    rec->f0 = radius;
  }
  rec->stops = std::move(stops);
  rec->tile = tile;
  m.m_recipe = std::move(rec);
  return m;
}

Paint Paint::conicGradient(glm::vec2 centerPoint, ColorStops stops,
                           GradientOptions options) {
  using namespace skia;
  const SkPoint center{centerPoint.x, centerPoint.y};
  if (options.units == GradientUnits::Box)
    return boxGradient(
        conicGradient({0, 0}, std::move(stops), inPixels(options)), center,
        true);
  const float startDegrees = options.startDegrees;
  const float endDegrees = options.endDegrees;
  // Skia's sweep starts at 3 o'clock and reads only angles inside
  // [0, 360], clamping rather than wrapping; the window is stated from 12
  // o'clock. A window that lands inside Skia's circle is handed over as
  // it is; any other is turned to start at Skia's zero and the sweep
  // rotated back by the same angle, so only a window wider than a turn
  // clamps. The numbers only meet here, so this is where the diagnostic
  // lives — once per process.
  float sweepStart = startDegrees - 90.0f;
  float sweepEnd = endDegrees - 90.0f;
  SkMatrix turn = SkMatrix::I();
  if (sweepStart < 0.0f || sweepEnd > 360.0f) {
    turn = SkMatrix::RotateDeg(sweepStart, center);
    sweepEnd -= sweepStart;
    sweepStart = 0.0f;
  }
  if (sweepEnd > 360.0f) {
    static std::atomic_bool warnedSweepWindow{false};
    if (!warnedSweepWindow.exchange(true, std::memory_order_relaxed)) {
      SkDebugf(
          "[material] material::Paint::conicGradient(start %.1f, end %.1f): "
          "a window wider than one turn CLAMPS, it does not wrap, so the "
          "run past the turn paints in the nearest stop's flat colour. "
          "(warned once)\n",
          startDegrees, endDegrees);
    }
  }
  const SkTileMode tile = toSkTileMode(options.repeat);
  RampArrays r = split(stops);
  Paint m = PaintAccess::wrap(SkShaders::SweepGradient(
      center, sweepStart, sweepEnd, makeGradient(r, tile),
      turn.isIdentity() ? nullptr : &turn));
  auto rec = std::make_shared<Recipe>();
  rec->kind = Recipe::Kind::Sweep;
  rec->p0 = center;
  rec->f0 = startDegrees;
  rec->f1 = endDegrees;
  rec->stops = std::move(stops);
  rec->tile = tile;
  m.m_recipe = std::move(rec);
  return m;
}

}  // namespace sigil::material

namespace sigil::material::skia::detail {

Paint cssLinearGradient(ColorStops stops, LinearDirection direction,
                        const GradientOptions& options) {
  if (direction.toward) {
    const auto [start, end] = towardPoints(*direction.toward);
    return boxGradient(Paint::linearGradient(start, end, std::move(stops),
                                             childOptions(options)),
                       BoxPlacement{});
  }
  // CSS's angle runs clockwise from straight up, and the canvas's y grows
  // downward, so 0 points along -y and 90 along +x.
  const float radians = direction.angleDegrees * 3.14159265358979f / 180.0f;
  return boxGradient(
      Paint::linearGradient({0, 0}, {1, 0}, std::move(stops),
                            childOptions(options)),
      {.mode = BoxPlacement::Along,
       .center = {0.5f, 0.5f},
       .direction = {std::sin(radians), -std::cos(radians)}});
}

Paint cssRadialGradient(ColorStops stops, RadialEndingShape shape,
                        const GradientOptions& options) {
  Paint child = Paint::radialGradient({0, 0}, 1, std::move(stops),
                                      childOptions(options));
  const SkPoint center{shape.at.x, shape.at.y};
  if (shape.shape == RadialShape::Circle)
    return boxGradient(std::move(child), {.mode = BoxPlacement::Circle,
                                          .center = center,
                                          .extent = (float)shape.extent});
  // An ellipse is the box's own proportions, so its extents are fractions
  // of the unit square measured from its centre; through a corner it
  // keeps the matching side's proportions, which puts it the square root
  // of two farther out.
  const glm::vec2 near = glm::min(shape.at, glm::vec2(1) - shape.at);
  const glm::vec2 far = glm::max(shape.at, glm::vec2(1) - shape.at);
  glm::vec2 radii = far * 1.41421356f;
  switch (shape.extent) {
    case RadialExtent::ClosestSide:
      radii = near;
      break;
    case RadialExtent::FarthestSide:
      radii = far;
      break;
    case RadialExtent::ClosestCorner:
      radii = near * 1.41421356f;
      break;
    case RadialExtent::FarthestCorner:
      break;
  }
  return boxGradient(std::move(child), {.mode = BoxPlacement::Ellipse,
                                        .center = center,
                                        .radii = {radii.x, radii.y}});
}

Paint cssConicGradient(ColorStops stops, ConicStart start,
                       const GradientOptions& options) {
  GradientOptions child = childOptions(options);
  child.startDegrees += start.fromDegrees;
  child.endDegrees += start.fromDegrees;
  return boxGradient(Paint::conicGradient({0, 0}, std::move(stops), child),
                     {.mode = BoxPlacement::PixelsAroundCenter,
                      .center = {start.at.x, start.at.y}});
}

}  // namespace sigil::material::skia::detail

namespace sigil::material::skia {

Paint PaintAccess::image(sk_sp<SkImage> image, SkTileMode horizontalTile,
                         SkTileMode verticalTile, const SkMatrix& local,
                         SkSamplingOptions sampling) {
  if (!image) return {};
  Paint m = wrap(
      SkShaders::Image(image, horizontalTile, verticalTile, sampling, &local));
  auto rec = std::make_shared<Paint::Recipe>();
  rec->kind = Paint::Recipe::Kind::Image;
  rec->image = std::move(image);
  rec->tx = horizontalTile;
  rec->ty = verticalTile;
  rec->local = local;
  rec->sampling = sampling;
  m.m_recipe = std::move(rec);
  return m;
}

Paint PaintAccess::buffer(std::shared_ptr<PixelBuffer> source,
                          SkTileMode horizontalTile, SkTileMode verticalTile,
                          const SkMatrix& local, SkSamplingOptions sampling) {
  if (!source) return {};
  sk_sp<SkImage> snapshot = source->image();
  if (!snapshot) return {};
  Paint m = wrap(SkShaders::Image(snapshot, horizontalTile, verticalTile,
                                  sampling, &local));
  auto rec = std::make_shared<Paint::Recipe>();
  rec->kind = Paint::Recipe::Kind::Buffer;
  rec->image = std::move(snapshot);
  rec->revision = source->revision();
  rec->source = std::move(source);
  rec->tx = horizontalTile;
  rec->ty = verticalTile;
  rec->local = local;
  rec->sampling = sampling;
  m.m_recipe = std::move(rec);
  return m;
}

// ---- PixelBuffer -----------------------------------------------------------

struct PixelBuffer::State {
  SkBitmap bitmap;
  std::unique_ptr<SkCanvas> canvas;
};

PixelBuffer::PixelBuffer(int width, int height)
    : m_state(std::make_unique<State>()) {
  m_state->bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(std::max(1, width), std::max(1, height)));
  m_state->bitmap.eraseColor(SK_ColorTRANSPARENT);
  m_state->canvas = std::make_unique<SkCanvas>(m_state->bitmap);
}
PixelBuffer::~PixelBuffer() = default;
SkBitmap& PixelBuffer::bitmap() { return m_state->bitmap; }
SkCanvas& PixelBuffer::canvas() { return *m_state->canvas; }
sk_sp<SkImage> PixelBuffer::image() {
  if (m_snapshotRevision != m_revision) {
    // Copy on the first request after a commit. The bitmap stays mutable
    // while paints retain the immutable snapshot of their own revision.
    m_snapshot = SkImages::RasterFromBitmap(m_state->bitmap);
    m_snapshotRevision = m_revision;
  }
  return m_snapshot;
}

namespace detail {

namespace {

/** The ramp's own source with its stop markers filled in: either the
 *  chain of mixes of a short ramp, or the read of a long one's lookup. */
sk_sp<SkRuntimeEffect> rampEffect(const std::string& declarations,
                                  const std::string& mixes, size_t count) {
  std::string source(shaderSource("UnitRamp.sksl"));
  const auto insert = [&](std::string_view marker,
                          std::string_view replacement) {
    const size_t at = source.find(marker);
    if (at != std::string::npos) source.replace(at, marker.size(), replacement);
  };
  insert("// SIGIL_STOP_DECLARATIONS", declarations);
  insert("// SIGIL_STOP_MIXES", mixes);
  auto [effect, error] =
      SkRuntimeEffect::MakeForShader(SkString(source.c_str()));
  if (!effect)
    SkDebugf("sigilmaterial unitRamp shader (%zu stops): %s\n", count,
             error.c_str());
  return effect;
}

/** How many stops a ramp mixes inline per pixel. Past this the chain of
 *  mixes costs more than one filtered read, so the ramp is evaluated once
 *  into a lookup and the pixel reads that. */
constexpr size_t kStopsMixedInline = 16;

/** How many entries a lookup holds. One less than it is a multiple of
 *  every power of two up to 1024, so the stops of an evenly spaced ramp
 *  of up to 1025 stops land on entries exactly and the filtered read
 *  between two entries is the straight mix the stops describe. */
constexpr int kLookupEntries = 1025;

/** The ramp's colour at @p t, unpremultiplied — the same chain of mixes
 *  the inline form runs per pixel, run here once per entry. */
std::array<float, 4> rampAt(const std::vector<ColorStop>& stops, float t) {
  Color colour = stops.front().color;
  for (size_t i = 1; i < stops.size(); ++i) {
    const float span = std::max(stops[i].offset - stops[i - 1].offset, 1e-6f);
    const float weight =
        std::clamp((t - stops[i - 1].offset) / span, 0.0f, 1.0f);
    const Color& next = stops[i].color;
    colour = {colour.r + (next.r - colour.r) * weight,
              colour.g + (next.g - colour.g) * weight,
              colour.b + (next.b - colour.b) * weight,
              colour.a + (next.a - colour.a) * weight};
  }
  return {colour.r, colour.g, colour.b, colour.a};
}

/** THE LOOKUP: the ramp evaluated once at every entry, in half floats with
 *  no colour space so the read hands the shader the very numbers the
 *  stops hold. */
sk_sp<SkImage> rampLookup(const std::vector<ColorStop>& stops) {
  SkBitmap exact;
  exact.allocPixels(SkImageInfo::Make(kLookupEntries, 1, kRGBA_F32_SkColorType,
                                      kUnpremul_SkAlphaType));
  auto* entries = static_cast<std::array<float, 4>*>(exact.getAddr(0, 0));
  for (int i = 0; i < kLookupEntries; ++i)
    entries[i] = rampAt(stops, (float)i / (float)(kLookupEntries - 1));
  SkBitmap half;
  half.allocPixels(SkImageInfo::Make(kLookupEntries, 1, kRGBA_F16_SkColorType,
                                     kUnpremul_SkAlphaType));
  exact.readPixels(half.pixmap());
  half.setImmutable();
  return half.asImage();
}

/** A ramp of many stops: the unit-square ramp reading its lookup at t
 *  once per pixel, with the lookup made once when the paint is. */
Paint lookupRamp(SkPoint a, SkPoint b, const std::vector<ColorStop>& stops,
                 bool radial) {
  static const sk_sp<SkRuntimeEffect> effect = rampEffect(
      "uniform shader uLookup;\n"
      "uniform float uLookupEntries;\n",
      "  col = float4(unpremul(uLookup.eval(\n"
      "      float2(t * (uLookupEntries - 1.0) + 0.5, 0.5))));\n",
      stops.size());
  if (!effect) return Paint::solid(stops.front().color);
  Paint material =
      PaintAccess::sksl(effect, {{"uRadial", radial ? 1.0f : 0.0f},
                                 {"uLookupEntries", (float)kLookupEntries}});
  material.set("uA", std::array<float, 2>{a.x(), a.y()});
  material.set("uB", std::array<float, 2>{b.x(), b.y()});
  material.set("uC0", stops.front().color);
  material.set("uS0", stops.front().offset);
  material.slot("uLookup",
                image(rampLookup(stops), Repeat::Pad, Repeat::Pad,
                      SkMatrix::I(), SkSamplingOptions(SkFilterMode::kLinear)));
  return material;
}

}  // namespace

Paint unitRamp(SkPoint a, SkPoint b, std::vector<ColorStop> stops,
               bool radial) {
  // A short ramp bakes its stop count into the source and one effect is
  // cached per count: the chain of mixes is cheaper than a read. A long
  // one reads a lookup made once, so its cost per pixel does not grow with
  // its stops and it takes as many as it is given.
  if (stops.empty()) return Paint::solid(material::Color{0, 0, 0, 0});
  if (stops.size() == 1) return Paint::solid(stops.front().color);
  if (stops.size() > kStopsMixedInline) return lookupRamp(a, b, stops, radial);
  const size_t n = stops.size();

  struct Cached {
    size_t count;
    sk_sp<SkRuntimeEffect> effect;
  };
  static std::mutex mutex;
  static std::vector<Cached> cache;
  sk_sp<SkRuntimeEffect> fx;
  {
    const std::lock_guard lock(mutex);
    for (const Cached& cached : cache)
      if (cached.count == n) {
        fx = cached.effect;
        break;
      }
    if (!fx) {
      std::string declarations;
      std::string mixes;
      for (size_t i = 1; i < n; ++i) {
        declarations += "uniform float4 uC" + std::to_string(i) + ";\n";
        declarations += "uniform float uS" + std::to_string(i) + ";\n";
        const std::string previous = std::to_string(i - 1);
        const std::string current = std::to_string(i);
        mixes += "  col = mix(col, uC" + current + ", clamp((t - uS" +
                 previous + ") / max(uS" + current + " - uS" + previous +
                 ", 1e-6), 0.0, 1.0));\n";
      }
      fx = rampEffect(declarations, mixes, n);
      if (!fx) return Paint::solid(stops.front().color);
      cache.push_back({n, fx});
    }
  }

  Paint material = PaintAccess::sksl(fx, {{"uRadial", radial ? 1.0f : 0.0f}});
  material.set("uA", std::array<float, 2>{a.x(), a.y()});
  material.set("uB", std::array<float, 2>{b.x(), b.y()});
  for (size_t i = 0; i < n; ++i) {
    material.set("uC" + std::to_string(i), stops[i].color);
    material.set("uS" + std::to_string(i), stops[i].offset);
  }
  return material;
}

}  // namespace detail

Paint PaintAccess::unresolvedSksl(sk_sp<SkRuntimeEffect> effect) {
  Paint paint;
  if (!effect) {
    // A material that fails to build must be loud. The usual route here is
    // `MakeForShader` returning null on a shader compile error and the
    // caller passing that null straight in: the material resolves to NONE,
    // the node paints nothing, and the compile error is long gone from the
    // log by the time anyone looks. Say it at BUILD, where the mistake is.
    static std::atomic_bool warnedNullEffect{false};
    if (!warnedNullEffect.exchange(true, std::memory_order_relaxed)) {
      SkDebugf(
          "[material] material::skia::sksl(null effect): the paint is NONE "
          "and its node will paint nothing. Check the error string "
          "MakeForShader returned next to the effect. (warned once)\n");
    }
    return paint;
  }
  paint.m_live = std::make_shared<Paint::Live>();
  paint.m_live->effect = std::move(effect);
  paint.m_live->usesTime =
      validUniform(paint.m_live->effect, "uTime", UniformType::kFloat);
  paint.m_live->usesScale =
      validUniform(paint.m_live->effect, "uContentScale", UniformType::kFloat);
  paint.m_live->usesGeometry =
      validUniform(paint.m_live->effect, "uResolution", UniformType::kFloat2);
  paint.m_live->usesWorld = validWorldUniform(paint.m_live->effect);
  paint.m_live->usesSampling = validUniform(
      paint.m_live->effect, "uLocalToSample", UniformType::kFloat3x3);
  return paint;
}

Paint PaintAccess::sksl(sk_sp<SkRuntimeEffect> effect,
                        std::vector<std::pair<std::string, float>> constants) {
  Paint paint = unresolvedSksl(std::move(effect));
  if (!paint.m_live) return paint;
  for (auto& [name, value] : constants) {
    if (!validUniform(paint.m_live->effect, name, UniformType::kFloat)) {
      warnUnknownUniform("sksl", name);
      continue;
    }
    putByName(paint.m_live->constants, std::move(name), value);
  }
  refresh(paint);
  return paint;
}

Paint image(sk_sp<SkImage> image, Repeat horizontal, Repeat vertical,
            const SkMatrix& local, SkSamplingOptions sampling) {
  return PaintAccess::image(std::move(image), toSkTileMode(horizontal),
                            toSkTileMode(vertical), local, sampling);
}

Paint buffer(std::shared_ptr<PixelBuffer> source, Repeat horizontal,
             Repeat vertical, const SkMatrix& local,
             SkSamplingOptions sampling) {
  return PaintAccess::buffer(std::move(source), toSkTileMode(horizontal),
                             toSkTileMode(vertical), local, sampling);
}

Paint sksl(sk_sp<SkRuntimeEffect> effect,
           std::vector<std::pair<std::string, float>> constants) {
  return PaintAccess::sksl(std::move(effect), std::move(constants));
}

Paint paint(sk_sp<SkShader> shader) {
  return PaintAccess::wrap(std::move(shader));
}

}  // namespace sigil::material::skia
